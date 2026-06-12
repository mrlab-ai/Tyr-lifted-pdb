/*
 * Copyright (C) 2026 Dominik Drexler
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "tyr/planning/lifted_task/abstractions/lifted_systematic_pattern_generator.hpp"

#include "tyr/common/declarations.hpp"
#include "tyr/common/onetbb.hpp"
#include "tyr/common/unordered_set.hpp"
#include "tyr/common/variant.hpp"
#include "tyr/formalism/planning/fdr_context.hpp"
#include "tyr/formalism/planning/views.hpp"
#include "tyr/planning/abstractions/pattern_generator.hpp"
#include "tyr/planning/lifted_task/abstractions/relaxed_reachability.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace tyr::planning
{

namespace
{

namespace f = formalism;
namespace fp = formalism::planning;

using FluentPredicateView = fp::PredicateView<f::FluentTag>;
using StaticPredicateView = fp::PredicateView<f::StaticTag>;
using ActionView = fp::ActionView;
using ConditionalEffectView = fp::ConditionalEffectView;
using FluentFactView = fp::FDRFactView<f::FluentTag>;

/// Mirrors `python/prototypes/pattern_gen.py:_StaticLit`. A precondition
/// literal of some action schema. `is_static=false` is reused for positive
/// fluent preconditions consumed by the R+ reachability filter (Phase 6.7).
struct StaticLit
{
    /// View types have no default ctor (they hold a context pointer); the
    /// optional gives us aggregate-initialization-friendly storage without
    /// per-struct ctors. Exactly one is engaged: `static_pred` when
    /// `is_static`, otherwise `fluent_pred` (positive fluent preconditions
    /// for the R+ filter).
    std::optional<StaticPredicateView> static_pred;
    std::optional<FluentPredicateView> fluent_pred;
    std::vector<std::variant<f::ParameterIndex, fp::ObjectView>> arg_terms;
    bool polarity = true;
    bool is_static = true;
};

/// Mirrors `python/prototypes/pattern_gen.py:_ActionEdge`.
struct ActionEdge
{
    std::optional<ActionView> action;
    std::vector<std::int32_t> eff_param_at_pos;
    std::vector<std::int32_t> pre_param_at_pos;
    std::vector<StaticLit> static_lits;
    std::vector<StaticLit> fluent_pre_lits;
    std::vector<std::vector<fp::ObjectView>> param_domains;
    bool eff_polarity = true;
};

struct FluentPredicatePair
{
    FluentPredicateView eff_pred;
    FluentPredicateView pre_pred;

    auto identifying_members() const noexcept { return std::tie(eff_pred, pre_pred); }
};

struct AtomRecord
{
    std::optional<FluentPredicateView> pred;
    std::vector<std::pair<std::int32_t, std::int32_t>> pos_vars;
    std::vector<std::int32_t> param_at_pos;
    bool has_ceff = false;
    std::uint32_t ceff_index = 0;
    bool polarity = true;
};

/// Python: `_CoEffEdge`. (eff_atom_A, eff_atom_B) coupling for the sys2
/// disjoint-union step. Carries A and B's param-at-pos maps, the action's
/// combined static + positive-fluent-pre literals (with B's ceff-local
/// params renamed by `offset` so the two scopes are disjoint), and the
/// per-param-index typed domain spanning the doubled scope.
struct CoEffEdge
{
    std::optional<ActionView> action;
    std::vector<std::int32_t> eff1_param_at_pos;
    std::vector<std::int32_t> eff2_param_at_pos;
    std::vector<StaticLit> static_lits;
    std::vector<StaticLit> fluent_pre_lits;
    std::vector<std::vector<fp::ObjectView>> param_domains;
};

std::vector<std::vector<fp::ObjectView>> build_param_domain(const analysis::ActionDomainView<fp::Repository>& adom,
                                                            const ConditionalEffectView* ceff_opt,
                                                            std::size_t action_arity)
{
    auto domain = std::vector<std::vector<fp::ObjectView>> {};

    const auto& pre_vds = adom.payload.precondition_domain.payload;
    domain.reserve(action_arity);
    for (std::size_t i = 0; i < action_arity; ++i)
        domain.emplace_back(pre_vds[i].objects);

    if (ceff_opt == nullptr)
        return domain;

    const auto edom_it = adom.payload.effect_domains.find(*ceff_opt);
    if (edom_it == adom.payload.effect_domains.end())
        return domain;

    const auto& cdom_vds = edom_it->second.payload.condition_domain.payload;
    const auto ceff_arity = ceff_opt->get_arity();
    for (std::size_t j = 0; j < ceff_arity; ++j)
    {
        const auto idx = action_arity + j;
        if (idx < cdom_vds.size())
            domain.emplace_back(cdom_vds[idx].objects);
        else
            domain.emplace_back();
    }

    return domain;
}

/// Cache key for `enumerate_matching`. `pred_idx` keys the predicate;
/// `sorted_constraints` is a sorted (position, object-index) list — sorted
/// so the same logical constraint set hashes identically regardless of
/// build order. uint32_t keeps the hash and equality cheap.
struct EnumMatchKey
{
    std::uint32_t pred_idx;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> sorted_constraints;

    auto identifying_members() const noexcept { return std::tie(pred_idx, sorted_constraints); }
};

}  // namespace

struct LiftedSystematicPatternGenerator::Impl
{
    const Task<LiftedTag>& task;
    /// Non-const shared_ptr handle used only to call `RelaxedReachability`,
    /// which mutates internal workspaces. The const-cast is safe because
    /// R+ does not mutate the planning task's logical state — it lazily
    /// builds its own Datalog workspace from the task's RPG program.
    std::shared_ptr<Task<LiftedTag>> task_ptr_for_rr;
    const LiftedSystematicPatternGeneratorOptions& options;
    fp::Repository& repository;
    fp::FDRContext& fdr_context;

    /// Phase-6.6 (eff_pred, pre_pred) → action edges, plus a derived
    /// fast-iteration view: `eff_to_pre[p]` is a list of (pre_pred, &edges)
    /// pairs so the BFS doesn't probe the map twice per neighbor.
    UnorderedMap<FluentPredicatePair, std::vector<ActionEdge>> action_edges;
    UnorderedMap<FluentPredicateView, std::vector<std::pair<FluentPredicateView, const std::vector<ActionEdge>*>>> eff_to_pre;

    /// Symmetric predicate co-occurrence over action schemas — only
    /// nullary-co-occurrence is consumed (after-loop nullary pass);
    /// non-nullary co_preds are reached via `eff_to_pre` instead.
    UnorderedMap<FluentPredicateView, UnorderedSet<FluentPredicateView>> pred_cooccur;

    /// 0-arity fluent predicate → single ground fact, if the FDR context
    /// has translated it (atoms surviving FDR encoding only).
    UnorderedMap<FluentPredicateView, FluentFactView> nullary_facts;

    /// Per-fluent-predicate typed argument domains, indexed by position.
    /// Mirrors Python's `_pred_domains`. Built once; consumed by
    /// `enumerate_matching`.
    UnorderedMap<FluentPredicateView, std::vector<std::vector<fp::ObjectView>>> pred_arg_domains;

    /// Cache for `enumerate_matching`. Hot on the BFS path.
    UnorderedMap<EnumMatchKey, std::vector<FluentFactView>> enum_match_cache;

    /// Phase-6.6 static atom index: static predicate → set of grounded
    /// object tuples (encoded as vector<uint32> of object indices). Used
    /// by `eval_static_lit` to test literal satisfaction under a partial
    /// binding. Includes user-supplied static facts and implicit type
    /// guards Tyr/Loki emits.
    UnorderedMap<StaticPredicateView, UnorderedSet<std::vector<std::uint32_t>>> static_index;

    /// Phase-6.7 delete-relaxation reachable fluent atoms R+. Populated
    /// when `options.reachability && options.static_csp`. Same shape as
    /// `static_index` but keyed by FluentPredicateView.
    UnorderedMap<FluentPredicateView, UnorderedSet<std::vector<std::uint32_t>>> reachable_index;
    bool reachable_computed = false;

    /// Phase-6.9 scorpion_match: the `=` static predicate handle (arity 2),
    /// used to synthesize collision-avoidance inequalities. Empty optional
    /// if the domain doesn't declare `:equality`; in that case the
    /// collision-avoidance check degrades to a no-op exactly as in Python.
    std::optional<StaticPredicateView> eq_pred;

    /// Interesting-mode co-effect edges. Built only when
    /// `options.interesting && options.static_csp`. Keyed by the (A, B)
    /// pair of fluent predicates appearing as positive effect atoms in
    /// some action; stores the parameter-binding info needed to validate
    /// goal-pair candidates at sys2.
    UnorderedMap<FluentPredicatePair, std::vector<CoEffEdge>> eff_eff_edges;

    Impl(std::shared_ptr<const Task<LiftedTag>> task_ptr, const LiftedSystematicPatternGeneratorOptions& opts) :
        task(*task_ptr),
        task_ptr_for_rr(std::const_pointer_cast<Task<LiftedTag>>(task_ptr)),
        options(opts),
        repository(*task_ptr->get_formalism_task().get_repository()),
        fdr_context(*task_ptr->get_formalism_task().get_fdr_context())
    {
        build_eq_pred();
        build_causal_graphs();
        build_eff_to_pre_index();
        build_pred_arg_domains();
        build_nullary_facts();
        build_static_index();
        if (options.reachability && options.static_csp)
            build_reachable_index();
        if (options.interesting && options.static_csp)
            build_eff_eff_edges();
    }

    void build_eq_pred()
    {
        for (const auto pred : task.get_task().get_domain().template get_predicates<f::StaticTag>())
        {
            if (pred.get_name() == "=" && pred.get_arity() == 2)
            {
                eq_pred = pred;
                return;
            }
        }
    }

    void build_causal_graphs()
    {
        const auto task_view = task.get_task();
        const auto domain = task_view.get_domain();
        const auto& var_domains = task.get_formalism_task().get_variable_domains_view();

        for (const auto action : domain.get_actions())
        {
            const auto action_arity = action.get_arity();

            auto pre_atoms = std::vector<AtomRecord> {};
            auto eff_atoms_all = std::vector<AtomRecord> {};
            auto top_static_lits = std::vector<StaticLit> {};
            auto top_fluent_pre_lits = std::vector<StaticLit> {};
            auto ceff_static_lits = std::vector<std::vector<StaticLit>> {};
            auto ceff_fluent_pre_lits = std::vector<std::vector<StaticLit>> {};
            auto ceff_local_index = UnorderedMap<ConditionalEffectView, std::uint32_t> {};

            const auto harvest_fluent = [&](auto literal_range, const ConditionalEffectView* ceff_opt, std::vector<AtomRecord>* pre_out, std::vector<AtomRecord>* eff_out)
            {
                for (const auto literal : literal_range)
                {
                    const auto atom = literal.get_atom();
                    auto rec = AtomRecord {};
                    rec.pred = atom.get_predicate();
                    rec.polarity = literal.get_polarity();
                    rec.has_ceff = (ceff_opt != nullptr);
                    if (ceff_opt != nullptr)
                    {
                        const auto it = ceff_local_index.find(*ceff_opt);
                        rec.ceff_index = (it != ceff_local_index.end()) ? it->second : std::uint32_t(-1);
                    }

                    std::int32_t pos = 0;
                    for (const auto term : atom.get_terms())
                    {
                        std::int32_t param_at_pos = -1;
                        tyr::visit(
                            [&](const auto& alt)
                            {
                                using A = std::decay_t<decltype(alt)>;
                                if constexpr (std::is_same_v<A, f::ParameterIndex>)
                                {
                                    const auto var = static_cast<std::int32_t>(uint_t(alt));
                                    rec.pos_vars.emplace_back(pos, var);
                                    param_at_pos = var;
                                }
                            },
                            term.get_variant());
                        rec.param_at_pos.push_back(param_at_pos);
                        ++pos;
                    }

                    if (pre_out != nullptr)
                        pre_out->push_back(rec);
                    if (eff_out != nullptr)
                        eff_out->push_back(rec);
                }
            };

            const auto harvest_static = [&](auto literal_range, std::vector<StaticLit>& out)
            {
                for (const auto literal : literal_range)
                {
                    const auto atom = literal.get_atom();
                    auto lit = StaticLit {};
                    lit.static_pred = atom.get_predicate();
                    lit.polarity = literal.get_polarity();
                    lit.is_static = true;
                    for (const auto term : atom.get_terms())
                        tyr::visit([&](const auto& alt) { lit.arg_terms.emplace_back(alt); }, term.get_variant());
                    out.push_back(std::move(lit));
                }
            };

            const auto harvest_fluent_pre_lits = [&](auto literal_range, std::vector<StaticLit>& out)
            {
                for (const auto literal : literal_range)
                {
                    if (!literal.get_polarity())
                        continue;
                    const auto atom = literal.get_atom();
                    auto lit = StaticLit {};
                    lit.fluent_pred = atom.get_predicate();
                    lit.polarity = true;
                    lit.is_static = false;
                    for (const auto term : atom.get_terms())
                        tyr::visit([&](const auto& alt) { lit.arg_terms.emplace_back(alt); }, term.get_variant());
                    out.push_back(std::move(lit));
                }
            };

            harvest_fluent(action.get_condition().template get_literals<f::FluentTag>(), nullptr, &pre_atoms, nullptr);
            harvest_static(action.get_condition().template get_literals<f::StaticTag>(), top_static_lits);
            harvest_fluent_pre_lits(action.get_condition().template get_literals<f::FluentTag>(), top_fluent_pre_lits);

            for (const auto ceff : action.get_effects())
            {
                const auto local_idx = static_cast<std::uint32_t>(ceff_static_lits.size());
                ceff_local_index[ceff] = local_idx;
                ceff_static_lits.emplace_back();
                ceff_fluent_pre_lits.emplace_back();
                harvest_fluent(ceff.get_condition().template get_literals<f::FluentTag>(), &ceff, &pre_atoms, nullptr);
                harvest_static(ceff.get_condition().template get_literals<f::StaticTag>(), ceff_static_lits.back());
                harvest_fluent_pre_lits(ceff.get_condition().template get_literals<f::FluentTag>(), ceff_fluent_pre_lits.back());
                harvest_fluent(ceff.get_effect().get_literals(), &ceff, nullptr, &eff_atoms_all);
            }

            // Symmetric predicate co-occurrence (over all atom records of this schema).
            auto schema_preds = UnorderedSet<FluentPredicateView> {};
            for (const auto& r : pre_atoms)
                schema_preds.insert(*r.pred);
            for (const auto& r : eff_atoms_all)
                schema_preds.insert(*r.pred);
            for (const auto p : schema_preds)
                for (const auto q : schema_preds)
                    if (!(EqualTo<FluentPredicateView> {}(p, q)))
                        pred_cooccur[p].insert(q);

            const auto adom_it = var_domains.action_domains.find(action);
            if (adom_it == var_domains.action_domains.end())
                continue;
            const auto& adom = adom_it->second;

            auto top_param_domain = build_param_domain(adom, nullptr, action_arity);
            auto ceff_param_domains = std::vector<std::vector<std::vector<fp::ObjectView>>> {};
            ceff_param_domains.reserve(ceff_static_lits.size());
            for (const auto ceff : action.get_effects())
                ceff_param_domains.push_back(build_param_domain(adom, &ceff, action_arity));

            for (const auto& eff_rec : eff_atoms_all)
            {
                const auto& edge_param_dom = eff_rec.has_ceff ? ceff_param_domains[eff_rec.ceff_index] : top_param_domain;
                auto edge_statics = top_static_lits;
                auto edge_fluent_pres = top_fluent_pre_lits;
                if (eff_rec.has_ceff)
                {
                    const auto& cstatics = ceff_static_lits[eff_rec.ceff_index];
                    const auto& cfluents = ceff_fluent_pre_lits[eff_rec.ceff_index];
                    edge_statics.insert(edge_statics.end(), cstatics.begin(), cstatics.end());
                    edge_fluent_pres.insert(edge_fluent_pres.end(), cfluents.begin(), cfluents.end());
                }

                for (const auto& pre_rec : pre_atoms)
                {
                    if (pre_rec.has_ceff && (!eff_rec.has_ceff || pre_rec.ceff_index != eff_rec.ceff_index))
                        continue;

                    auto edge = ActionEdge {};
                    edge.action = action;
                    edge.eff_param_at_pos = eff_rec.param_at_pos;
                    edge.pre_param_at_pos = pre_rec.param_at_pos;
                    edge.static_lits = edge_statics;
                    edge.fluent_pre_lits = edge_fluent_pres;
                    edge.param_domains = edge_param_dom;
                    edge.eff_polarity = eff_rec.polarity;

                    action_edges[FluentPredicatePair { *eff_rec.pred, *pre_rec.pred }].push_back(std::move(edge));
                }
            }
        }
    }

    /// Precompute eff_pred → list of (pre_pred, &edges) for the BFS hot loop.
    /// Done once after `action_edges` is final so the pointers stay valid.
    void build_eff_to_pre_index()
    {
        for (const auto& [pair, edges] : action_edges)
            eff_to_pre[pair.eff_pred].emplace_back(pair.pre_pred, &edges);
    }

    /// Cache the per-fluent-predicate typed argument domain pointer list,
    /// so `enumerate_matching` doesn't re-walk the VariableDomain view map
    /// per call. The owned storage holds the actual ObjectView lists (the
    /// view-map's entries return temporary `objects` vectors we materialize
    /// once here).
    void build_pred_arg_domains()
    {
        // PredicateDomainViewMap<FluentTag, C> = SimpleScopedDomainViewMap,
        // so the value is the VariableDomainViewList directly (no payload
        // wrapper as on ActionDomainView).
        const auto& var_domains = task.get_formalism_task().get_variable_domains_view();
        for (const auto& [pred, vds] : var_domains.fluent_predicate_domains)
        {
            auto positions = std::vector<std::vector<fp::ObjectView>> {};
            positions.reserve(vds.size());
            for (const auto& vd : vds)
                positions.emplace_back(vd.objects);
            pred_arg_domains.insert({ pred, std::move(positions) });
        }
    }

    const std::vector<std::vector<fp::ObjectView>>* domains_for(FluentPredicateView pred) const
    {
        const auto it = pred_arg_domains.find(pred);
        return it != pred_arg_domains.end() ? &it->second : nullptr;
    }

    /// Construct (binding, atom, fact-view) for a given fluent predicate
    /// and object-list. Returns nullopt if FDR encoding dropped this atom.
    std::optional<FluentFactView> make_fact_view(FluentPredicateView pred, const std::vector<fp::ObjectView>& objects)
    {
        auto binding_data = Data<f::RelationBinding<f::Predicate<f::FluentTag>>>(pred, objects);
        const auto& binding_view = repository.get_or_create(binding_data).first;
        auto atom_data = Data<fp::GroundAtom<f::FluentTag>>(binding_view);
        const auto& atom_view = repository.get_or_create(atom_data).first;
        const auto fact_view = fdr_context.get_fact_view(atom_view);
        if (!fact_view.get_atom().has_value())
            return std::nullopt;
        return fact_view;
    }

    /// Python: `_enumerate_matching`. Cartesian-product enumeration over
    /// the predicate's typed argument domains, with positions in
    /// `constraints` pinned to a single object.
    std::vector<FluentFactView> enumerate_matching(FluentPredicateView pred, const std::vector<std::pair<std::uint32_t, fp::ObjectView>>& constraints)
    {
        // Build cache key (sorted by position).
        auto sorted = constraints;
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        auto key = EnumMatchKey {};
        key.pred_idx = static_cast<std::uint32_t>(uint_t(pred.get_index()));
        key.sorted_constraints.reserve(sorted.size());
        for (const auto& [pos, obj] : sorted)
            key.sorted_constraints.emplace_back(pos, static_cast<std::uint32_t>(uint_t(obj.get_index())));

        if (const auto it = enum_match_cache.find(key); it != enum_match_cache.end())
            return it->second;

        auto domains = domains_for(pred);
        const auto arity = pred.get_arity();
        if (domains == nullptr || domains->size() != arity)
        {
            auto empty = std::vector<FluentFactView> {};
            enum_match_cache[key] = empty;
            return empty;
        }

        // For each position, the iter is either {pinned object} or the full domain.
        auto iters = std::vector<std::vector<fp::ObjectView>> {};
        iters.reserve(arity);
        for (std::size_t pos = 0; pos < arity; ++pos)
        {
            // Search constraints; constraints.size() is small (≤ arity).
            std::optional<fp::ObjectView> pinned;
            for (const auto& [cpos, cobj] : constraints)
            {
                if (cpos == pos)
                {
                    pinned = cobj;
                    break;
                }
            }
            if (pinned)
                iters.push_back({ *pinned });
            else
                iters.push_back((*domains)[pos]);
        }

        // Cartesian product. We walk indices i[0..arity-1] over each iter.
        auto facts = std::vector<FluentFactView> {};
        if (arity == 0)
        {
            if (const auto fact = make_fact_view(pred, {}))
                facts.push_back(*fact);
            enum_match_cache[key] = facts;
            return facts;
        }

        auto idx = std::vector<std::size_t>(arity, 0);
        bool done = false;
        // current[] gets fully overwritten each iteration; initialize via
        // iters[pos][0] so we never default-construct an ObjectView.
        auto current = std::vector<fp::ObjectView> {};
        current.reserve(arity);
        for (std::size_t pos = 0; pos < arity; ++pos)
            current.push_back(iters[pos][0]);
        while (!done)
        {
            for (std::size_t pos = 0; pos < arity; ++pos)
                current[pos] = iters[pos][idx[pos]];
            if (const auto fact = make_fact_view(pred, current))
                facts.push_back(*fact);

            // Advance idx in odometer style.
            std::size_t k = 0;
            while (k < arity)
            {
                ++idx[k];
                if (idx[k] < iters[k].size())
                    break;
                idx[k] = 0;
                ++k;
            }
            if (k == arity)
                done = true;
        }

        enum_match_cache[key] = facts;
        return facts;
    }

    /// Mirrors Python's `_static_index` build. Iterates over the task's
    /// static atoms (initial-state static portion: user-supplied facts
    /// plus Tyr/Loki's implicit type-guard atoms) and groups by predicate.
    void build_static_index()
    {
        for (const auto atom : task.get_task().template get_atoms<f::StaticTag>())
        {
            auto objs = std::vector<std::uint32_t> {};
            for (const auto obj : atom.get_row().get_objects())
                objs.push_back(static_cast<std::uint32_t>(uint_t(obj.get_index())));
            static_index[atom.get_predicate()].insert(std::move(objs));
        }
    }

    /// Phase-6.7 reachability filter. Calls Tyr's native R+ Datalog program
    /// (`RelaxedReachability<LiftedTag>::compute()` — the same fixpoint Fast
    /// Downward's grounder uses via `instantiate.explore`) and groups the
    /// resulting fluent atoms by predicate. Single-threaded execution
    /// context matches the Python prototype.
    void build_reachable_index()
    {
        // Use the task-level R+ memo: the fixpoint is computed at most once per
        // task and shared with other consumers (e.g. the projection generator's
        // reachability filter).
        const auto& reachable_atoms = task_ptr_for_rr->get_or_compute_relaxed_reachable_atoms(
            [&]
            {
                auto execution_context = std::make_shared<ExecutionContext>(1);
                return RelaxedReachability<LiftedTag> { task_ptr_for_rr, execution_context }.compute();
            });
        for (const auto atom : reachable_atoms)
        {
            auto objs = std::vector<std::uint32_t> {};
            for (const auto obj : atom.get_row().get_objects())
                objs.push_back(static_cast<std::uint32_t>(uint_t(obj.get_index())));
            reachable_index[atom.get_predicate()].insert(std::move(objs));
        }
        reachable_computed = true;
    }

    enum class LitResult : std::uint8_t
    {
        Satisfied,
        Violated,
        Undetermined,
    };

    /// Python: `_eval_static_lit`. Returns Satisfied / Violated when the
    /// binding determines the literal's truth; Undetermined when at least
    /// one parameter slot is still free. `free_out` (when provided) is
    /// populated with the unbound parameter indices on Undetermined.
    LitResult eval_static_lit(const StaticLit& lit, const std::unordered_map<std::int32_t, fp::ObjectView>& binding, std::vector<std::int32_t>* free_out = nullptr) const
    {
        // Skip fluent-precondition literals when reachability is off — Python
        // does not append them to the CSP literal list in that case.
        if (!lit.is_static && !options.reachability)
            return LitResult::Satisfied;

        auto resolved = std::vector<std::uint32_t> {};
        resolved.reserve(lit.arg_terms.size());
        bool free = false;
        for (const auto& term : lit.arg_terms)
        {
            if (std::holds_alternative<f::ParameterIndex>(term))
            {
                const auto pi = static_cast<std::int32_t>(uint_t(std::get<f::ParameterIndex>(term)));
                const auto it = binding.find(pi);
                if (it == binding.end())
                {
                    free = true;
                    if (free_out != nullptr)
                        free_out->push_back(pi);
                }
                else
                {
                    resolved.push_back(static_cast<std::uint32_t>(uint_t(it->second.get_index())));
                }
            }
            else  // ObjectView constant
            {
                resolved.push_back(static_cast<std::uint32_t>(uint_t(std::get<fp::ObjectView>(term).get_index())));
            }
        }
        if (free)
            return LitResult::Undetermined;

        bool present = false;
        if (lit.is_static && lit.static_pred.has_value())
        {
            const auto it = static_index.find(*lit.static_pred);
            if (it != static_index.end())
                present = it->second.contains(resolved);
        }
        else if (!lit.is_static && lit.fluent_pred.has_value() && reachable_computed)
        {
            const auto it = reachable_index.find(*lit.fluent_pred);
            if (it != reachable_index.end())
                present = it->second.contains(resolved);
        }

        const bool truth = (lit.polarity == present);
        return truth ? LitResult::Satisfied : LitResult::Violated;
    }

    /// Python: `_satisfy_statics`. Recursive backtracking over still-unbound
    /// parameters; picks the most-constrained parameter (one that appears
    /// in the most open literals) at each branching step. Modeled to be a
    /// faithful translation — same branching heuristic, same termination.
    bool satisfy_statics(const std::unordered_map<std::int32_t, fp::ObjectView>& binding,
                         const std::vector<StaticLit>& lits,
                         const std::vector<std::vector<fp::ObjectView>>& param_domains) const
    {
        auto open_lits = std::vector<const StaticLit*> {};
        auto open_lit_free = std::vector<std::vector<std::int32_t>> {};

        for (const auto& lit : lits)
        {
            auto free = std::vector<std::int32_t> {};
            const auto r = eval_static_lit(lit, binding, &free);
            if (r == LitResult::Satisfied)
                continue;
            if (r == LitResult::Violated)
                return false;
            open_lits.push_back(&lit);
            open_lit_free.push_back(std::move(free));
        }

        if (open_lits.empty())
            return true;

        // Count parameter occurrences across open literals. Pick the
        // parameter with the highest count as the branching variable.
        auto free_counts = std::unordered_map<std::int32_t, std::size_t> {};
        for (const auto& free : open_lit_free)
            for (const auto p : free)
                ++free_counts[p];
        if (free_counts.empty())
            return false;  // unreachable in well-formed input

        std::int32_t pick = -1;
        std::size_t best = 0;
        for (const auto& [p, c] : free_counts)
        {
            if (c > best)
            {
                best = c;
                pick = p;
            }
        }
        if (pick < 0 || static_cast<std::size_t>(pick) >= param_domains.size())
            return false;  // parameter not in scope

        // Build the lits vector for the recursive call (only open ones).
        auto recurse_lits = std::vector<StaticLit> {};
        recurse_lits.reserve(open_lits.size());
        for (const auto* lit_ptr : open_lits)
            recurse_lits.push_back(*lit_ptr);

        for (const auto obj : param_domains[pick])
        {
            auto new_binding = binding;
            new_binding.insert_or_assign(pick, obj);
            if (satisfy_statics(new_binding, recurse_lits, param_domains))
                return true;
        }
        return false;
    }

    /// Python: `_collision_avoidance_lits`. Synthesizes inequality literals
    /// preventing a positive fluent precondition atom from grounding to the
    /// same atom as the action's effect under the binding — mirrors
    /// Scorpion's SAS+ no-op simplification.
    ///
    /// Skipped when:
    ///   - the domain has no `=` predicate (eq_pred missing → no-op),
    ///   - the effect literal has negative polarity (SAS+ no-op only fires
    ///     when pre and eff write the same value to the same variable),
    ///   - the pre literal differs from the effect at >1 parameter position
    ///     (the precise avoidance is a disjunction our CSP can't encode;
    ///     we conservatively keep the edge),
    ///   - the effect has a constant at any position (we lose the value via
    ///     `param_at_pos = -1`; conservatively skip).
    ///
    /// When the eff literal and a fluent_pre lit share the same parameter
    /// index at every position, an "always-collide" lit is emitted: a
    /// positive `=`-lit between two distinct constants, which is never in
    /// the (reflexive-only) `=` static index, so the edge is rejected.
    std::vector<StaticLit> collision_avoidance_lits(const ActionEdge& edge, FluentPredicateView eff_pred) const
    {
        auto extra = std::vector<StaticLit> {};
        if (!eq_pred.has_value())
            return extra;
        if (!edge.eff_polarity)
            return extra;

        const auto& eff_params = edge.eff_param_at_pos;
        for (const auto& lit : edge.fluent_pre_lits)
        {
            if (!lit.fluent_pred.has_value())
                continue;
            if (!EqualTo<FluentPredicateView> {}(*lit.fluent_pred, eff_pred))
                continue;
            if (lit.arg_terms.size() != eff_params.size())
                continue;

            // Diff-positions list: pre/eff disagree (or pre is a constant).
            // null sentinel (via the `skip` bool) means "skip this lit entirely".
            auto diff_positions = std::vector<std::pair<std::size_t, std::variant<f::ParameterIndex, fp::ObjectView>>> {};
            bool skip = false;
            for (std::size_t i = 0; i < lit.arg_terms.size(); ++i)
            {
                const auto eff_param = eff_params[i];
                if (eff_param < 0)
                {
                    skip = true;
                    break;
                }
                if (std::holds_alternative<f::ParameterIndex>(lit.arg_terms[i]))
                {
                    const auto pi = static_cast<std::int32_t>(uint_t(std::get<f::ParameterIndex>(lit.arg_terms[i])));
                    if (pi == eff_param)
                        continue;  // same parameter at this slot — always-equal
                }
                diff_positions.emplace_back(i, lit.arg_terms[i]);
            }
            if (skip)
                continue;

            if (diff_positions.empty())
            {
                // Same ParameterIndex at every position → always collide.
                // Emit positive `=`-lit between two distinct constants;
                // unsatisfiable in the (reflexive) `=` static index → reject edge.
                auto objs = std::vector<fp::ObjectView> {};
                for (const auto obj : task.get_task().get_objects())
                {
                    objs.push_back(obj);
                    if (objs.size() == 2)
                        break;
                }
                if (objs.size() < 2)
                    continue;
                auto syn = StaticLit {};
                syn.static_pred = eq_pred;
                syn.polarity = true;
                syn.is_static = true;
                syn.arg_terms.emplace_back(objs[0]);
                syn.arg_terms.emplace_back(objs[1]);
                extra.push_back(std::move(syn));
                continue;
            }
            if (diff_positions.size() > 1)
                continue;  // conservative skip

            // One-position diff: synthesize `pre_term != eff_param` as a
            // negative `=`-lit. The eff side stored as ParameterIndex
            // (treated identically by `eval_static_lit`).
            const auto eff_param_int = static_cast<f::ParameterIndex>(static_cast<uint_t>(eff_params[diff_positions[0].first]));
            auto syn = StaticLit {};
            syn.static_pred = eq_pred;
            syn.polarity = false;
            syn.is_static = true;
            syn.arg_terms.emplace_back(eff_param_int);
            syn.arg_terms.push_back(diff_positions[0].second);
            extra.push_back(std::move(syn));
        }
        return extra;
    }

    /// Python: `_is_valid_candidate`. Bind action parameters via the goal
    /// atom (effect side) and candidate atom (precondition side), bail if
    /// a parameter index gets two different values, then run satisfy_statics
    /// over the edge's combined static / fluent-pre literal list.
    bool is_valid_candidate(FluentFactView goal_fact, FluentFactView candidate_fact, const ActionEdge& edge, FluentPredicateView eff_pred) const
    {
        auto binding = std::unordered_map<std::int32_t, fp::ObjectView> {};

        // Bind via the effect atom.
        const auto goal_atom = goal_fact.get_atom();
        if (!goal_atom.has_value())
            return false;
        std::size_t pos = 0;
        for (const auto obj : goal_atom->get_row().get_objects())
        {
            if (pos < edge.eff_param_at_pos.size())
            {
                const auto p = edge.eff_param_at_pos[pos];
                if (p >= 0)
                {
                    const auto it = binding.find(p);
                    if (it != binding.end() && !EqualTo<fp::ObjectView> {}(it->second, obj))
                        return false;
                    binding.insert_or_assign(p, obj);
                }
            }
            ++pos;
        }

        // Bind via the precondition atom.
        const auto cand_atom = candidate_fact.get_atom();
        if (!cand_atom.has_value())
            return false;
        pos = 0;
        for (const auto obj : cand_atom->get_row().get_objects())
        {
            if (pos < edge.pre_param_at_pos.size())
            {
                const auto p = edge.pre_param_at_pos[pos];
                if (p >= 0)
                {
                    const auto it = binding.find(p);
                    if (it != binding.end() && !EqualTo<fp::ObjectView> {}(it->second, obj))
                        return false;
                    binding.insert_or_assign(p, obj);
                }
            }
            ++pos;
        }

        // Assemble the literal list: static preconditions + (when R+ on)
        // positive fluent preconditions + (when scorpion_match on) the
        // synthesized collision-avoidance lits.
        auto lits = edge.static_lits;
        if (options.reachability)
            lits.insert(lits.end(), edge.fluent_pre_lits.begin(), edge.fluent_pre_lits.end());
        if (options.scorpion_match)
        {
            auto extra = collision_avoidance_lits(edge, eff_pred);
            lits.insert(lits.end(), extra.begin(), extra.end());
        }
        return satisfy_statics(binding, lits, edge.param_domains);
    }

    // ─── Interesting-mode helpers (Python: `LiftedInterestingPatternGenerator`) ───

    /// Walk an action's top-level static literals into the StaticLit form
    /// used by `eval_static_lit` / `satisfy_statics`.
    std::vector<StaticLit> harvest_action_top_static(ActionView action) const
    {
        auto out = std::vector<StaticLit> {};
        for (const auto literal : action.get_condition().template get_literals<f::StaticTag>())
        {
            const auto atom = literal.get_atom();
            auto lit = StaticLit {};
            lit.static_pred = atom.get_predicate();
            lit.polarity = literal.get_polarity();
            lit.is_static = true;
            for (const auto term : atom.get_terms())
                tyr::visit([&](const auto& alt) { lit.arg_terms.emplace_back(alt); }, term.get_variant());
            out.push_back(std::move(lit));
        }
        return out;
    }

    std::vector<StaticLit> harvest_action_top_fluent_pre(ActionView action) const
    {
        auto out = std::vector<StaticLit> {};
        for (const auto literal : action.get_condition().template get_literals<f::FluentTag>())
        {
            if (!literal.get_polarity())
                continue;
            const auto atom = literal.get_atom();
            auto lit = StaticLit {};
            lit.fluent_pred = atom.get_predicate();
            lit.polarity = true;
            lit.is_static = false;
            for (const auto term : atom.get_terms())
                tyr::visit([&](const auto& alt) { lit.arg_terms.emplace_back(alt); }, term.get_variant());
            out.push_back(std::move(lit));
        }
        return out;
    }

    std::vector<StaticLit> harvest_ceff_static(ConditionalEffectView ceff) const
    {
        auto out = std::vector<StaticLit> {};
        for (const auto literal : ceff.get_condition().template get_literals<f::StaticTag>())
        {
            const auto atom = literal.get_atom();
            auto lit = StaticLit {};
            lit.static_pred = atom.get_predicate();
            lit.polarity = literal.get_polarity();
            lit.is_static = true;
            for (const auto term : atom.get_terms())
                tyr::visit([&](const auto& alt) { lit.arg_terms.emplace_back(alt); }, term.get_variant());
            out.push_back(std::move(lit));
        }
        return out;
    }

    std::vector<StaticLit> harvest_ceff_fluent_pre(ConditionalEffectView ceff) const
    {
        auto out = std::vector<StaticLit> {};
        for (const auto literal : ceff.get_condition().template get_literals<f::FluentTag>())
        {
            if (!literal.get_polarity())
                continue;
            const auto atom = literal.get_atom();
            auto lit = StaticLit {};
            lit.fluent_pred = atom.get_predicate();
            lit.polarity = true;
            lit.is_static = false;
            for (const auto term : atom.get_terms())
                tyr::visit([&](const auto& alt) { lit.arg_terms.emplace_back(alt); }, term.get_variant());
            out.push_back(std::move(lit));
        }
        return out;
    }

    /// Positive effect atoms of a ceff: predicate + per-position param idx.
    /// `-1` sentinel for a constant slot, exactly like AtomRecord.param_at_pos.
    struct PosEffAtom
    {
        std::optional<FluentPredicateView> pred;
        std::vector<std::int32_t> param_at_pos;
    };

    std::vector<PosEffAtom> harvest_ceff_positive_effs(ConditionalEffectView ceff) const
    {
        auto out = std::vector<PosEffAtom> {};
        for (const auto literal : ceff.get_effect().get_literals())
        {
            if (!literal.get_polarity())
                continue;
            const auto atom = literal.get_atom();
            auto eff = PosEffAtom {};
            eff.pred = atom.get_predicate();
            for (const auto term : atom.get_terms())
            {
                std::int32_t v = -1;
                tyr::visit(
                    [&](const auto& alt)
                    {
                        using A = std::decay_t<decltype(alt)>;
                        if constexpr (std::is_same_v<A, f::ParameterIndex>)
                            v = static_cast<std::int32_t>(uint_t(alt));
                    },
                    term.get_variant());
                eff.param_at_pos.push_back(v);
            }
            out.push_back(std::move(eff));
        }
        return out;
    }

    /// Shift every ceff-local ParameterIndex (idx >= action_arity) by
    /// `offset`. Top-level params and concrete Objects pass through. We
    /// rebuild the variant with a renamed `ParameterIndex` value rather
    /// than introducing a separate "raw int" alternative — the project's
    /// ParameterIndex is a uint-mixin, so packing the renamed value works
    /// uniformly with `eval_static_lit`.
    StaticLit rename_lit(const StaticLit& lit, std::size_t action_arity, std::size_t offset) const
    {
        auto out = lit;
        for (auto& term : out.arg_terms)
        {
            if (std::holds_alternative<f::ParameterIndex>(term))
            {
                const auto idx = static_cast<std::size_t>(uint_t(std::get<f::ParameterIndex>(term)));
                if (idx >= action_arity)
                    term = static_cast<f::ParameterIndex>(static_cast<uint_t>(idx + offset));
            }
        }
        return out;
    }

    /// Python: `_build_eff_eff_edges`. Enumerate (eff_atom_A, eff_atom_B)
    /// pairs across all (ceff_A, ceff_B) combinations of each action.
    /// Cases (i)-(iii) of the docstring are handled by iterating over all
    /// (a_idx, b_idx, i, j) tuples and skipping the (same-ceff, same-atom,
    /// no-local-params) case (case iii degenerate).
    void build_eff_eff_edges()
    {
        const auto domain = task.get_task().get_domain();
        const auto& var_domains = task.get_formalism_task().get_variable_domains_view();

        for (const auto action : domain.get_actions())
        {
            const auto action_arity = action.get_arity();
            const auto top_static = harvest_action_top_static(action);
            const auto top_fluent = harvest_action_top_fluent_pre(action);

            // Per-ceff harvest.
            struct CeffData
            {
                ConditionalEffectView ceff;
                std::vector<StaticLit> ceff_static;
                std::vector<StaticLit> ceff_fluent;
                std::vector<PosEffAtom> effs;
                std::size_t arity;
            };
            auto ceff_data = std::vector<CeffData> {};
            std::size_t max_local_arity = 0;
            for (const auto ceff : action.get_effects())
            {
                ceff_data.push_back(CeffData {
                    ceff,
                    harvest_ceff_static(ceff),
                    harvest_ceff_fluent_pre(ceff),
                    harvest_ceff_positive_effs(ceff),
                    ceff.get_arity(),
                });
                max_local_arity = std::max(max_local_arity, static_cast<std::size_t>(ceff.get_arity()));
            }
            const auto offset = max_local_arity;

            // Action domain (for the per-(action, ceff) param-domain table).
            const auto adom_it = var_domains.action_domains.find(action);
            if (adom_it == var_domains.action_domains.end())
                continue;
            const auto& adom = adom_it->second;

            // Pre-compute per-ceff dom-A (un-renamed) for reuse below.
            auto ceff_doms = std::vector<std::vector<std::vector<fp::ObjectView>>> {};
            ceff_doms.reserve(ceff_data.size());
            for (const auto& cd : ceff_data)
                ceff_doms.push_back(build_param_domain(adom, &cd.ceff, action_arity));

            for (std::size_t a_idx = 0; a_idx < ceff_data.size(); ++a_idx)
            {
                for (std::size_t b_idx = 0; b_idx < ceff_data.size(); ++b_idx)
                {
                    const auto& cd_A = ceff_data[a_idx];
                    const auto& cd_B = ceff_data[b_idx];

                    // Build doubled-scope literals: top + A's + renamed B's.
                    auto combined_statics = top_static;
                    combined_statics.insert(combined_statics.end(),
                                            cd_A.ceff_static.begin(), cd_A.ceff_static.end());
                    for (const auto& l : cd_B.ceff_static)
                        combined_statics.push_back(rename_lit(l, action_arity, offset));

                    auto combined_fluent = top_fluent;
                    combined_fluent.insert(combined_fluent.end(),
                                           cd_A.ceff_fluent.begin(), cd_A.ceff_fluent.end());
                    for (const auto& l : cd_B.ceff_fluent)
                        combined_fluent.push_back(rename_lit(l, action_arity, offset));

                    // Combined param domains: A's full (top + A locals) then
                    // B's renamed locals appended at indices >= action_arity + offset.
                    auto combined_dom = ceff_doms[a_idx];
                    const auto& dom_B = ceff_doms[b_idx];
                    if (combined_dom.size() < action_arity + 2 * offset)
                        combined_dom.resize(action_arity + 2 * offset);
                    for (std::size_t p_idx = action_arity; p_idx < dom_B.size(); ++p_idx)
                    {
                        const auto renamed = p_idx + offset;
                        if (renamed >= combined_dom.size())
                            combined_dom.resize(renamed + 1);
                        combined_dom[renamed] = dom_B[p_idx];
                    }

                    for (std::size_t i = 0; i < cd_A.effs.size(); ++i)
                    {
                        for (std::size_t j = 0; j < cd_B.effs.size(); ++j)
                        {
                            const auto& eff_A = cd_A.effs[i];
                            const auto& eff_B = cd_B.effs[j];
                            if (a_idx == b_idx && i == j)
                            {
                                // Case (iii): same ceff, same atom. Only
                                // emit if the atom has ≥1 ceff-local slot
                                // (otherwise two firings produce the same
                                // ground atom — not a co-effect pair).
                                bool has_local = false;
                                for (const auto p : eff_A.param_at_pos)
                                    if (p >= static_cast<std::int32_t>(action_arity))
                                    {
                                        has_local = true;
                                        break;
                                    }
                                if (!has_local)
                                    continue;
                            }
                            // B side: rename ceff-local param indices.
                            auto eff2 = eff_B.param_at_pos;
                            for (auto& p : eff2)
                                if (p >= static_cast<std::int32_t>(action_arity))
                                    p += static_cast<std::int32_t>(offset);

                            auto edge = CoEffEdge {};
                            edge.action = action;
                            edge.eff1_param_at_pos = eff_A.param_at_pos;
                            edge.eff2_param_at_pos = std::move(eff2);
                            edge.static_lits = combined_statics;
                            edge.fluent_pre_lits = combined_fluent;
                            edge.param_domains = combined_dom;
                            eff_eff_edges[FluentPredicatePair { *eff_A.pred, *eff_B.pred }].push_back(std::move(edge));
                        }
                    }
                }
            }
        }
    }

    /// Python: `_is_valid_coeff`. Bind both goal atoms to the edge's
    /// effect-side params, bail on parameter conflict, run satisfy_statics.
    bool is_valid_coeff(FluentFactView g1, FluentFactView g2, const CoEffEdge& edge) const
    {
        auto binding = std::unordered_map<std::int32_t, fp::ObjectView> {};
        const auto bind_side = [&](FluentFactView fact, const std::vector<std::int32_t>& pmap) -> bool
        {
            const auto atom = fact.get_atom();
            if (!atom.has_value())
                return false;
            std::size_t pos = 0;
            for (const auto obj : atom->get_row().get_objects())
            {
                if (pos < pmap.size())
                {
                    const auto p = pmap[pos];
                    if (p >= 0)
                    {
                        const auto it = binding.find(p);
                        if (it != binding.end() && !EqualTo<fp::ObjectView> {}(it->second, obj))
                            return false;
                        binding.insert_or_assign(p, obj);
                    }
                }
                ++pos;
            }
            return true;
        };
        if (!bind_side(g1, edge.eff1_param_at_pos))
            return false;
        if (!bind_side(g2, edge.eff2_param_at_pos))
            return false;

        auto lits = edge.static_lits;
        if (options.reachability)
            lits.insert(lits.end(), edge.fluent_pre_lits.begin(), edge.fluent_pre_lits.end());
        return satisfy_statics(binding, lits, edge.param_domains);
    }

    void build_nullary_facts()
    {
        const auto domain = task.get_task().get_domain();
        for (const auto pred : domain.template get_predicates<f::FluentTag>())
        {
            if (pred.get_arity() != 0)
                continue;
            if (const auto fact = make_fact_view(pred, {}))
                nullary_facts.insert({ pred, *fact });
        }
    }

    /// One BFS step: collect neighbor facts for the current pattern.
    std::vector<FluentFactView> get_neighbor_facts(const std::vector<FluentFactView>& pattern_facts, const UnorderedSet<FluentFactView>& pattern_facts_set)
    {
        auto candidates = std::vector<FluentFactView> {};
        auto seen = UnorderedSet<FluentFactView> {};
        auto pattern_preds = UnorderedSet<FluentPredicateView> {};

        for (const auto fact : pattern_facts)
        {
            const auto atom_opt = fact.get_atom();
            if (!atom_opt.has_value())
                continue;
            const auto atom = *atom_opt;
            const auto pred = atom.get_predicate();
            pattern_preds.insert(pred);

            // Phase-6.6 static-CSP path: iterate every action_edge with
            // eff_pred == pred. The edge's pre_pred is the co_pred we expand
            // toward. Nullary co_preds are handled in the post-loop block.
            const auto eff_it = eff_to_pre.find(pred);
            if (eff_it == eff_to_pre.end())
                continue;

            // Goal atom's objects, by position. The view's iterator isn't
            // random-access (no `operator-`), so build the vector by
            // range-for rather than iterator-pair construction.
            auto goal_objs = std::vector<fp::ObjectView> {};
            for (const auto obj : atom.get_row().get_objects())
                goal_objs.push_back(obj);

            for (const auto& [co_pred, edges_ptr] : eff_it->second)
            {
                if (co_pred.get_arity() == 0)
                    continue;
                for (const auto& edge : *edges_ptr)
                {
                    // Build goal_binding (param_idx → object) from the goal atom.
                    // Use a map so duplicate parameter slots in the effect
                    // (e.g. `(p ?x ?x)`) take the last-seen object, matching
                    // Python's dict-comprehension semantics. Without this,
                    // first-vs-last divergence on aliased slots silently
                    // produces a different candidate set.
                    // ObjectView has no default ctor; use insert_or_assign so
                    // we never default-construct an entry.
                    auto goal_binding = std::unordered_map<std::int32_t, fp::ObjectView> {};
                    for (std::size_t i = 0; i < edge.eff_param_at_pos.size() && i < goal_objs.size(); ++i)
                    {
                        const auto p = edge.eff_param_at_pos[i];
                        if (p >= 0)
                            goal_binding.insert_or_assign(p, goal_objs[i]);
                    }
                    // Build constraints for the candidate (pre side).
                    auto constraints = std::vector<std::pair<std::uint32_t, fp::ObjectView>> {};
                    for (std::size_t pre_pos = 0; pre_pos < edge.pre_param_at_pos.size(); ++pre_pos)
                    {
                        const auto p = edge.pre_param_at_pos[pre_pos];
                        if (p < 0)
                            continue;
                        const auto it = goal_binding.find(p);
                        if (it != goal_binding.end())
                            constraints.emplace_back(pre_pos, it->second);
                    }
                    for (const auto candidate : enumerate_matching(co_pred, constraints))
                    {
                        if (pattern_facts_set.contains(candidate))
                            continue;
                        if (seen.contains(candidate))
                            continue;
                        if (options.static_csp && !is_valid_candidate(fact, candidate, edge, pred))
                            continue;
                        candidates.push_back(candidate);
                        seen.insert(candidate);
                    }
                }
            }
        }

        // Nullary predicates co-occurring with any pattern predicate.
        for (const auto pred : pattern_preds)
        {
            const auto co_it = pred_cooccur.find(pred);
            if (co_it == pred_cooccur.end())
                continue;
            for (const auto co_pred : co_it->second)
            {
                if (co_pred.get_arity() != 0)
                    continue;
                const auto null_it = nullary_facts.find(co_pred);
                if (null_it == nullary_facts.end())
                    continue;
                const auto fact = null_it->second;
                if (pattern_facts_set.contains(fact))
                    continue;
                if (seen.contains(fact))
                    continue;
                candidates.push_back(fact);
                seen.insert(fact);
            }
        }

        return candidates;
    }

    /// Python: `_generate_sga_fact_lists`. BFS from goal singletons,
    /// growing by one fact per level, deduplicating by canonical fact-set
    /// key, capped at `max_pattern_count`.
    std::vector<std::vector<FluentFactView>> generate_patterns(std::size_t max_pattern_size, std::size_t max_pattern_count)
    {
        auto all_lists = std::vector<std::vector<FluentFactView>> {};
        auto seen_keys = UnorderedSet<std::vector<std::uint64_t>> {};

        const auto goal_facts = task.get_task().get_goal().template get_facts<f::PositiveTag>();
        auto current_level = std::vector<std::vector<FluentFactView>> {};
        for (const auto fact : goal_facts)
        {
            if (!fact.get_atom().has_value())
                continue;
            const auto key = pattern_key({ fact });
            if (seen_keys.contains(key))
                continue;
            seen_keys.insert(key);
            all_lists.push_back({ fact });
            current_level.push_back({ fact });
            if (all_lists.size() >= max_pattern_count)
                return all_lists;
        }

        for (std::size_t size = 2; size <= max_pattern_size; ++size)
        {
            if (all_lists.size() >= max_pattern_count)
                break;
            auto next_level = std::vector<std::vector<FluentFactView>> {};
            for (const auto& facts : current_level)
            {
                auto facts_set = UnorderedSet<FluentFactView> {};
                for (const auto f : facts)
                    facts_set.insert(f);
                for (const auto neighbor : get_neighbor_facts(facts, facts_set))
                {
                    auto new_facts = facts;
                    new_facts.push_back(neighbor);
                    const auto key = pattern_key(new_facts);
                    if (seen_keys.contains(key))
                        continue;
                    seen_keys.insert(key);
                    all_lists.push_back(new_facts);
                    next_level.push_back(std::move(new_facts));
                    if (all_lists.size() >= max_pattern_count)
                        return all_lists;
                }
            }
            current_level = std::move(next_level);
            if (current_level.empty())
                break;
        }

        // Interesting-mode sys2 disjoint-union step (Python:
        // `LiftedInterestingPatternGenerator.generate`'s post-SGA loop).
        // Augment with goal-pair patterns {g1, g2} where some action has
        // both atoms in its positive effect set under a feasible binding.
        // Triggered only when interesting is on AND we're at sys2 (the
        // general step for larger sizes is not implemented on either side).
        if (options.interesting && max_pattern_size >= 2)
        {
            auto goal_facts_list = std::vector<FluentFactView> {};
            for (const auto fact : task.get_task().get_goal().template get_facts<f::PositiveTag>())
            {
                if (fact.get_atom().has_value())
                    goal_facts_list.push_back(fact);
            }
            for (std::size_t i = 0; i < goal_facts_list.size(); ++i)
            {
                for (std::size_t j = i + 1; j < goal_facts_list.size(); ++j)
                {
                    if (all_lists.size() >= max_pattern_count)
                        break;
                    const auto g1 = goal_facts_list[i];
                    const auto g2 = goal_facts_list[j];
                    const auto key = pattern_key({ g1, g2 });
                    if (seen_keys.contains(key))
                        continue;
                    const auto pred1 = g1.get_atom()->get_predicate();
                    const auto pred2 = g2.get_atom()->get_predicate();
                    bool feasible = false;
                    // Orientation (pred1, pred2): bind g1↦eff1, g2↦eff2.
                    if (const auto it = eff_eff_edges.find(FluentPredicatePair { pred1, pred2 });
                        it != eff_eff_edges.end())
                    {
                        for (const auto& edge : it->second)
                        {
                            if (is_valid_coeff(g1, g2, edge))
                            {
                                feasible = true;
                                break;
                            }
                        }
                    }
                    // Orientation (pred2, pred1) — stored separately. Skip
                    // when pred1 == pred2 (same key).
                    if (!feasible && !(EqualTo<FluentPredicateView> {}(pred1, pred2)))
                    {
                        if (const auto it = eff_eff_edges.find(FluentPredicatePair { pred2, pred1 });
                            it != eff_eff_edges.end())
                        {
                            for (const auto& edge : it->second)
                            {
                                if (is_valid_coeff(g2, g1, edge))
                                {
                                    feasible = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (feasible)
                    {
                        seen_keys.insert(key);
                        all_lists.push_back({ g1, g2 });
                    }
                }
                if (all_lists.size() >= max_pattern_count)
                    break;
            }
        }
        return all_lists;
    }

    /// Sorted atom-index list — order-independent dedup key. Each FDR fact
    /// is uniquely identified by its ground atom (binary FDR encoding maps
    /// one atom → one fact). `FDRFactView::get_handle()` returns a
    /// `Data<FDRFact>`, not an Index, so we go through the atom view.
    static std::vector<std::uint64_t> pattern_key(const std::vector<FluentFactView>& facts)
    {
        auto key = std::vector<std::uint64_t> {};
        key.reserve(facts.size());
        for (const auto f : facts)
        {
            const auto atom = f.get_atom();
            if (!atom.has_value())
                continue;
            key.push_back(static_cast<std::uint64_t>(uint_t(atom->get_index())));
        }
        std::sort(key.begin(), key.end());
        return key;
    }
};

LiftedSystematicPatternGenerator::LiftedSystematicPatternGenerator(std::shared_ptr<const Task<LiftedTag>> task,
                                                                   LiftedSystematicPatternGeneratorOptions options) :
    m_task(std::move(task)),
    m_options(options),
    m_impl(std::make_unique<Impl>(m_task, m_options))
{
}

LiftedSystematicPatternGenerator::~LiftedSystematicPatternGenerator() = default;

std::shared_ptr<LiftedSystematicPatternGenerator>
LiftedSystematicPatternGenerator::create(std::shared_ptr<const Task<LiftedTag>> task, LiftedSystematicPatternGeneratorOptions options)
{
    return std::make_shared<LiftedSystematicPatternGenerator>(std::move(task), options);
}

std::size_t LiftedSystematicPatternGenerator::num_action_edges() const noexcept
{
    std::size_t total = 0;
    for (const auto& [_, edges] : m_impl->action_edges)
        total += edges.size();
    return total;
}

PatternCollection LiftedSystematicPatternGenerator::generate()
{
    auto patterns = PatternCollection {};
    const auto fact_lists = m_impl->generate_patterns(m_options.max_pattern_size, m_options.max_pattern_count);
    patterns.reserve(fact_lists.size());
    for (const auto& facts : fact_lists)
        patterns.emplace_back(facts);
    return patterns;
}

}
