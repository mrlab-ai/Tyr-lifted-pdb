/*
 * Copyright (C) 2025 Dominik Drexler
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

#include "tyr/formalism/planning/invariants/synthesis.hpp"

#include "tyr/common/comparators.hpp"
#include "tyr/common/declarations.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/formalism/planning/expression_arity.hpp"
#include "tyr/formalism/planning/grounder.hpp"
#include "tyr/formalism/planning/invariants/invariant.hpp"
#include "tyr/formalism/planning/planning_task.hpp"
#include "tyr/formalism/planning/repository.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <unordered_set>
#include <variant>
#include <vector>

namespace tyr::formalism::planning::invariant
{
namespace
{

// Fig 7
bool covers(const Invariant& inv, const TempAtom& element)
{
    for (const auto& atom : inv.atoms)
    {
        if (atom.predicate != element.predicate)
            continue;

        if (atom.terms.size() != element.terms.size())
            continue;

        Substitution substitution(inv.num_rigid_variables + inv.num_counted_variables);
        bool mismatch = false;

        for (uint_t i = 0; i < atom.terms.size(); ++i)
        {
            const auto& lhs = atom.terms[i];
            const auto rhs = element.terms[i];

            const bool ok = std::visit(
                [&](auto&& lhs_arg) -> bool
                {
                    using Lhs = std::decay_t<decltype(lhs_arg)>;

                    return std::visit(
                        [&](auto&& rhs_arg) -> bool
                        {
                            using Rhs = std::decay_t<decltype(rhs_arg)>;

                            if constexpr (std::is_same_v<Lhs, ParameterIndex>)
                            {
                                const bool is_counted = (static_cast<uint_t>(lhs_arg) >= inv.num_rigid_variables);

                                if (is_counted)
                                    return substitution.assign_or_check(lhs_arg, rhs);

                                if constexpr (std::is_same_v<Rhs, ParameterIndex>)
                                    return lhs_arg == rhs_arg;
                                else
                                    return false;
                            }
                            else if constexpr (std::is_same_v<Lhs, Index<Object>>)
                            {
                                if constexpr (std::is_same_v<Rhs, Index<Object>>)
                                    return lhs_arg == rhs_arg;
                                else
                                    return false;
                            }
                            else
                            {
                                static_assert(dependent_false<Lhs>::value, "Missing case");
                            }
                        },
                        rhs.value);
                },
                lhs.value);

            if (!ok)
            {
                mismatch = true;
                break;
            }
        }

        if (!mismatch)
            return true;
    }

    return false;
}

bool is_operator_too_heavy(const TempAction& op, const Invariant& inv)
{
    // TODO:
    // Let o′ be a copy of o.
    // Duplicate all (non-trivially) quantified effects of o′ .
    // Assign unique names to all quantified variables in effects of o′ .

    for (const auto conj_eff_lhs : op.effects)
    {
        for (const auto conj_eff_rhs : op.effects)
        {
            for (const auto atom_lhs : conj_eff_lhs.add_effects)
            {
                if (!inv.predicates.contains(atom_lhs.predicate))
                    continue;

                for (const auto atom_rhs : conj_eff_rhs.add_effects)
                {
                    if (!inv.predicates.contains(atom_rhs.predicate))
                        continue;

                    // TODO:
                    // if the parameters of operator o′ can be renamed so that
                    // (e.atom 6= e′ .atom and
                    // covers(V , Φ, e.atom) and covers(V , Φ, e′ .atom) and
                    // o′ .precond ∧ e.cond ∧ e′ .cond ∧ ¬e.atom ∧ ¬e′ .atom
                    // is satisfiable):
                    // return true. { The operator is too heavy. }
                }
            }
        }
    }

    return false;  // { The operator is not too heavy. }
}

bool is_add_effect_unbalanced(const TempAction& op, const TempAtom& e, const Invariant& inv)
{
    for (const auto conj_eff : op.effects)
    {
        // TODO:
        // Let o′ be a copy of o where the parameters are minimally renamed so that covers(V , Φ, e.atom) is true.

        for (const auto atom : conj_eff.del_effects)
        {
            // TODO:
            // if the quantified variables of e′ can be renamed so that
            // (e.atom 6= e′ .atom and covers(V , Φ, e′ .atom) and
            // o′ .precond ∧ e.cond ∧ ¬e.atom |= e′ .cond ∧ e′ .atom):
            // return false. { e′ balances e. }
        }
    }

    return true;  // { The add effect is unbalanced. }
}

struct Threat
{
    size_t op_index;
    size_t effect_index;
    size_t add_index;
};

enum class ProofStatus
{
    Proven,
    TooHeavy,
    UnbalancedAddEffect
};

struct ProofResult
{
    ProofStatus status;
    std::optional<Threat> threat;
};

ProofResult prove_invariant(const Invariant& inv, const TempActionList& ops)
{
    for (size_t op_index = 0; op_index < ops.size(); ++op_index)
    {
        const auto& op = ops[op_index];

        if (is_operator_too_heavy(op, inv))
            return { ProofStatus::TooHeavy, Threat { op_index, 0, 0 } };  // { Reject the candidate. }

        for (size_t effect_index = 0; effect_index < op.effects.size(); ++effect_index)
        {
            const auto& conj_eff = op.effects[effect_index];

            for (size_t add_index = 0; add_index < conj_eff.add_effects.size(); ++add_index)
            {
                const auto& atom = conj_eff.add_effects[add_index];
                {
                    if (is_add_effect_unbalanced(op, atom, inv))
                        return { ProofStatus::UnbalancedAddEffect, Threat { op_index, effect_index, add_index } };  // { Reject the candidate. }
                }
            }
        }
    }

    return { ProofStatus::Proven, std::nullopt };  // { Accept the candidate. }
}

// Fig 8
InvariantList refine_candidate(const Invariant& inv, const Threat& threat, const TempActionList& ops)
{
    // TODO
    // Select some schematic operator o and add effect e such that
    // is-add-effect-unbalanced(o, e, V , Φ) returns true.
    // for each atom ϕ′ over variables from V and at most one other variable
    // for which covers(V , Φ, ϕ′ ) is not true:
    // Φ′ := Φ ∪ {ϕ′ }
    // Simplify Φ′ by removing atoms from Φ that are covered by ϕ′ .
    // (These cannot contribute to the weight of an instance of hV, Φ′i.)
    // Simplify Φ′ by removing unused parameters.
    // if not is-add-effect-unbalanced(o, e, V , Φ′ ):
    // Add hV, Φ′i to the set of invariant candidates.
}

TempAtom make_initial_atom(PredicateView<FluentTag> predicate, size_t counted_position)
{
    const auto arity = static_cast<size_t>(predicate.get_arity());
    auto terms = std::vector<Data<Term>> {};
    terms.reserve(arity);

    if (counted_position == arity)
    {
        for (size_t i = 0; i < arity; ++i)
            terms.emplace_back(Data<Term>(ParameterIndex(i)));
    }
    else
    {
        const auto counted_index = ParameterIndex(arity - 1);

        for (size_t i = 0; i < arity; ++i)
        {
            if (i == counted_position)
            {
                terms.emplace_back(Data<Term>(counted_index));
            }
            else
            {
                const auto rigid_index = (i < counted_position) ? ParameterIndex(i) : ParameterIndex(i - 1);
                terms.emplace_back(Data<Term>(rigid_index));
            }
        }
    }

    return TempAtom {
        .predicate = predicate,
        .terms = std::move(terms),
    };
}

InvariantList make_initial_candidates(PredicateListView<FluentTag> predicates)
{
    auto result = InvariantList {};

    for (const auto predicate : predicates)
    {
        const auto arity = predicate.get_arity();

        // Case 1: no counted variable
        {
            Invariant inv {};
            inv.num_rigid_variables = arity;
            inv.num_counted_variables = 0;
            inv.atoms.push_back(make_initial_atom(predicate, arity));
            inv.predicates.insert(predicate);
            result.push_back(std::move(inv));
        }

        // Case 2: exactly one counted variable
        for (size_t counted_position = 0; counted_position < arity; ++counted_position)
        {
            Invariant inv {};
            inv.num_rigid_variables = arity - 1;
            inv.num_counted_variables = 1;
            inv.atoms.push_back(make_initial_atom(predicate, counted_position));
            inv.predicates.insert(predicate);
            result.push_back(std::move(inv));
        }
    }

    return result;
}
}

// Fig 15 and 17
InvariantList synthesize_invariants(TaskView task)
{
    auto ops = make_temp_actions(task.get_domain().get_actions());

    auto queue = make_initial_candidates(task.get_domain().get_predicates<FluentTag>());

    auto accepted = InvariantList {};

    while (!queue.empty())
    {
        auto candidate = std::move(queue.back());
        queue.pop_back();

        auto result = prove_invariant(candidate, ops);

        if (result.status == ProofStatus::Proven)
        {
            accepted.push_back(std::move(candidate));
        }
        else if (result.status == ProofStatus::UnbalancedAddEffect)
        {
            auto refined = refine_candidate(candidate, *result.threat, ops);
            queue.insert(queue.end(), std::make_move_iterator(refined.begin()), std::make_move_iterator(refined.end()));
        }
    }

    return accepted;
}
}
