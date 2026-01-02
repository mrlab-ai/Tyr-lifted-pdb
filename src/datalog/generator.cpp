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

#include "tyr/datalog/generator.hpp"

#include "tyr/common/comparators.hpp"         // for opera...
#include "tyr/common/config.hpp"              // for uint_t
#include "tyr/common/equal_to.hpp"            // for EqualTo
#include "tyr/common/formatter.hpp"           // for opera...
#include "tyr/common/hash.hpp"                // for Hash
#include "tyr/common/types.hpp"               // for View
#include "tyr/common/vector.hpp"              // for opera...
#include "tyr/datalog/applicability.hpp"      // for is_ap...
#include "tyr/datalog/consistency_graph.hpp"  // for Vertex
#include "tyr/datalog/declarations.hpp"
#include "tyr/datalog/fact_sets.hpp"
#include "tyr/datalog/formatter.hpp"
#include "tyr/datalog/generator.hpp"
#include "tyr/datalog/kpkc.hpp"       // for for_e...
#include "tyr/datalog/kpkc_data.hpp"  // for Works...
#include "tyr/datalog/workspaces/facts.hpp"
#include "tyr/datalog/workspaces/rule.hpp"
#include "tyr/datalog/workspaces/rule_delta.hpp"
#include "tyr/datalog/workspaces/worker.hpp"
#include "tyr/formalism/datalog/conjunctive_condition_view.hpp"  // for View
#include "tyr/formalism/datalog/declarations.hpp"                // for Context
#include "tyr/formalism/datalog/formatter.hpp"                   // for opera...
#include "tyr/formalism/datalog/grounder.hpp"                    // for Groun...
#include "tyr/formalism/datalog/repository.hpp"                  // for Repos...
#include "tyr/formalism/datalog/views.hpp"

#include <algorithm>                        // for all_of
#include <assert.h>                         // for assert
#include <cista/containers/hash_storage.h>  // for opera...
#include <cista/containers/variant.h>       // for visit
#include <memory>                           // for __sha...
#include <tuple>                            // for opera...
#include <utility>                          // for pair
#include <vector>                           // for vector

namespace f = tyr::formalism;
namespace fd = tyr::formalism::datalog;

namespace tyr::datalog
{

template<f::FactKind T, fd::Context C_SRC, fd::Context C_DST>
bool is_valid_binding(View<Index<fd::Literal<T>>, C_SRC> element, const FactSets& fact_sets, fd::GrounderContext<C_DST>& context)
{
    return fact_sets.template get<T>().predicate.contains(ground(element.get_atom(), context).first) == element.get_polarity();
}

template<f::FactKind T, fd::Context C_SRC, fd::Context C_DST>
bool is_valid_binding(View<IndexList<fd::Literal<T>>, C_SRC> elements, const FactSets& fact_sets, fd::GrounderContext<C_DST>& context)
{
    return std::all_of(elements.begin(), elements.end(), [&](auto&& arg) { return is_valid_binding(arg, fact_sets, context); });
}

template<fd::Context C_SRC, fd::Context C_DST>
bool is_valid_binding(View<DataList<fd::Literal<f::FluentTag>>, C_SRC> elements, const FactSets& fact_sets, fd::GrounderContext<C_DST>& context)
{
    return std::all_of(elements.begin(), elements.end(), [&](auto&& arg) { return is_valid_binding(arg, fact_sets, context); });
}

template<fd::Context C_SRC, fd::Context C_DST>
bool is_valid_binding(View<Data<fd::BooleanOperator<Data<fd::FunctionExpression>>>, C_SRC> element,
                      const FactSets& fact_sets,
                      fd::GrounderContext<C_DST>& context)
{
    return evaluate(make_view(ground(element, context), context.destination), fact_sets);
}

template<fd::Context C_SRC, fd::Context C_DST>
bool is_valid_binding(View<DataList<fd::BooleanOperator<Data<fd::FunctionExpression>>>, C_SRC> elements,
                      const FactSets& fact_sets,
                      fd::GrounderContext<C_DST>& context)
{
    return std::all_of(elements.begin(), elements.end(), [&](auto&& arg) { return is_valid_binding(arg, fact_sets, context); });
}

template<fd::Context C_SRC, fd::Context C_DST>
bool is_valid_binding(View<Index<fd::ConjunctiveCondition>, C_SRC> element, const FactSets& fact_sets, fd::GrounderContext<C_DST>& context)
{
    return is_valid_binding(element.template get_literals<f::StaticTag>(), fact_sets, context)     //
           && is_valid_binding(element.template get_literals<f::FluentTag>(), fact_sets, context)  //
           && is_valid_binding(element.get_numeric_constraints(), fact_sets, context);
}

static auto create_nullary_ground_head_in_stage(View<Index<fd::Atom<f::FluentTag>>, fd::Repository> head, fd::GrounderContext<fd::Repository>& context)
{
    context.binding.clear();

    return ground(head, context);
}

static auto create_unary_ground_head_in_stage(uint_t vertex_index,
                                              const StaticConsistencyGraph& consistency_graph,
                                              View<Index<fd::Atom<f::FluentTag>>, fd::Repository> head,
                                              fd::GrounderContext<fd::Repository>& context)
{
    context.binding.clear();

    const auto& vertex = consistency_graph.get_vertex(vertex_index);
    assert(uint_t(vertex.get_parameter_index()) == 0);
    context.binding.push_back(vertex.get_object_index());

    return ground(head, context);
}

static auto create_general_ground_head_in_stage(const std::vector<uint_t>& clique,
                                                const StaticConsistencyGraph& consistency_graph,
                                                View<Index<fd::Atom<f::FluentTag>>, fd::Repository> head,
                                                fd::GrounderContext<fd::Repository>& context)
{
    context.binding.resize(clique.size());
    for (const auto vertex_index : clique)
    {
        const auto& vertex = consistency_graph.get_vertex(vertex_index);
        assert(uint_t(vertex.get_parameter_index()) < clique.size());
        context.binding[uint_t(vertex.get_parameter_index())] = vertex.get_object_index();
    }

    return ground(head, context);
}

void ground_nullary_case(const FactsWorkspace& fact_ws,
                         const ConstFactsWorkspace& const_fact_ws,
                         RuleWorkspace& rule_ws,
                         const ConstRuleWorkspace& const_rule_ws,
                         RuleDeltaWorkspace& rule_delta_ws,
                         WorkerWorkspace& worker_ws)
{
    auto ground_context_stage = fd::GrounderContext { worker_ws.builder, *rule_delta_ws.repository, rule_delta_ws.binding };
    auto fact_sets = FactSets(const_fact_ws.fact_sets, fact_ws.fact_sets);

    /// --- Rule stage
    const auto ground_head = create_nullary_ground_head_in_stage(const_rule_ws.get_rule().get_head(), ground_context_stage).first;

    if (!rule_delta_ws.ground_heads.contains(ground_head))
    {
        /// --- Rule
        auto ground_context_rule = fd::GrounderContext { worker_ws.builder, rule_ws.overlay_repository, rule_delta_ws.binding };

        // Note: we never go through the consistency graph, and hence, have to check validity on the entire rule body.
        // This should not occur very often anyways.
        if (is_valid_binding(const_rule_ws.get_rule().get_body(), fact_sets, ground_context_rule))
        {
            rule_delta_ws.ground_heads.insert(ground_head);
            rule_ws.ground_heads.push_back(ground_head);
        }
    }
}

void ground_unary_case(const FactsWorkspace& fact_ws,
                       const ConstFactsWorkspace& const_fact_ws,
                       RuleWorkspace& rule_ws,
                       const ConstRuleWorkspace& const_rule_ws,
                       RuleDeltaWorkspace& rule_delta_ws,
                       WorkerWorkspace& worker_ws)
{
    auto ground_context_stage = fd::GrounderContext { worker_ws.builder, *rule_delta_ws.repository, rule_delta_ws.binding };
    auto fact_sets = FactSets(const_fact_ws.fact_sets, fact_ws.fact_sets);

    for (const auto vertex_index : rule_ws.kpkc_workspace.consistent_vertices_vec)
    {
        /// --- Rule stage
        const auto ground_head =
            create_unary_ground_head_in_stage(vertex_index, const_rule_ws.static_consistency_graph, const_rule_ws.get_rule().get_head(), ground_context_stage)
                .first;

        if (!rule_delta_ws.ground_heads.contains(ground_head))
        {
            /// --- Rule
            auto ground_context_rule = fd::GrounderContext { worker_ws.builder, rule_ws.overlay_repository, rule_delta_ws.binding };

            if (is_valid_binding(const_rule_ws.get_unary_conflicting_overapproximation_condition(), fact_sets, ground_context_rule))
            {
                // Ensure that ground rule is truly applicable
                assert(is_applicable(make_view(ground(const_rule_ws.get_rule(), ground_context_rule).first, rule_ws.overlay_repository), fact_sets));

                rule_delta_ws.ground_heads.insert(ground_head);
                rule_ws.ground_heads.push_back(ground_head);
            }
        }
    }
}

void ground_general_case(const FactsWorkspace& fact_ws,
                         const ConstFactsWorkspace& const_fact_ws,
                         RuleWorkspace& rule_ws,
                         const ConstRuleWorkspace& const_rule_ws,
                         RuleDeltaWorkspace& rule_delta_ws,
                         WorkerWorkspace& worker_ws)
{
    auto ground_context_stage = fd::GrounderContext { worker_ws.builder, *rule_delta_ws.repository, rule_delta_ws.binding };
    auto fact_sets = FactSets(const_fact_ws.fact_sets, fact_ws.fact_sets);

    kpkc::for_each_k_clique(
        rule_ws.consistency_graph,
        rule_ws.kpkc_workspace,
        [&](auto&& clique)
        {
            /// --- Rule stage
            const auto ground_head =
                create_general_ground_head_in_stage(clique, const_rule_ws.static_consistency_graph, const_rule_ws.get_rule().get_head(), ground_context_stage)
                    .first;

            if (!rule_delta_ws.ground_heads.contains(ground_head))
            {
                /// --- Rule
                auto ground_context_rule = fd::GrounderContext { worker_ws.builder, rule_ws.overlay_repository, rule_delta_ws.binding };

                if (is_valid_binding(const_rule_ws.get_binary_conflicting_overapproximation_condition(), fact_sets, ground_context_rule))
                {
                    // Ensure that ground rule is truly applicable
                    assert(is_applicable(make_view(ground(const_rule_ws.get_rule(), ground_context_rule).first, rule_ws.overlay_repository), fact_sets));

                    rule_delta_ws.ground_heads.insert(ground_head);
                    rule_ws.ground_heads.push_back(ground_head);
                }
            }
        });
}

void ground(const FactsWorkspace& fact_ws,
            const ConstFactsWorkspace& const_fact_ws,
            RuleWorkspace& rule_ws,
            const ConstRuleWorkspace& const_rule_ws,
            RuleDeltaWorkspace& rule_delta_ws,
            WorkerWorkspace& worker_ws)
{
    auto fact_sets = FactSets(const_fact_ws.fact_sets, fact_ws.fact_sets);

    if (!is_applicable(const_rule_ws.get_nullary_condition(), fact_sets))
        return;

    const auto arity = const_rule_ws.get_rule().get_arity();

    if (arity == 0)
        ground_nullary_case(fact_ws, const_fact_ws, rule_ws, const_rule_ws, rule_delta_ws, worker_ws);
    else if (arity == 1)
        ground_unary_case(fact_ws, const_fact_ws, rule_ws, const_rule_ws, rule_delta_ws, worker_ws);
    else
        ground_general_case(fact_ws, const_fact_ws, rule_ws, const_rule_ws, rule_delta_ws, worker_ws);
}

}