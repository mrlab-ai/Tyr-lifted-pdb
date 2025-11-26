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

#ifndef TYR_GROUNDER_GENERATOR_HPP_
#define TYR_GROUNDER_GENERATOR_HPP_

#include "tyr/formalism/declarations.hpp"
#include "tyr/formalism/ground_atom_index.hpp"
#include "tyr/grounder/applicability.hpp"
#include "tyr/grounder/declarations.hpp"
#include "tyr/grounder/kpkc.hpp"

namespace tyr::grounder
{
template<formalism::IsContext C>
void ground_nullary_case(const ImmutableRuleWorkspace<C>& immutable_workspace, MutableRuleWorkspace<C>& mutable_workspace)
{
}

template<formalism::IsContext C>
void ground_unary_case(const ImmutableRuleWorkspace<C>& immutable_workspace, MutableRuleWorkspace<C>& mutable_workspace)
{
}

template<formalism::IsContext C>
void ground_general_case(const ImmutableRuleWorkspace<C>& immutable_workspace, MutableRuleWorkspace<C>& mutable_workspace)
{
    kpkc::for_each_k_clique(immutable_workspace.consistency_graph,
                            mutable_workspace.kpkc_workspace,
                            [](auto&& clique)
                            {
                                // TODO: ground the conjunctive condition and check if it applicable
                                std::cout << to_string(clique) << std::endl;

                                // TODO: ground the final rule
                            });
}

template<formalism::IsContext C>
void ground(const ImmutableRuleWorkspace<C>& immutable_workspace, MutableRuleWorkspace<C>& mutable_workspace)
{
    const auto rule = immutable_workspace.rule;
    const auto& fact_sets = immutable_workspace.fact_sets;

    if (!nullary_conditions_hold(rule.get_body(), fact_sets))
        return;

    const auto arity = rule.get_body().get_arity();

    if (arity == 0)
        ground_nullary_case(immutable_workspace, mutable_workspace);
    else if (arity == 1)
        ground_unary_case(immutable_workspace, mutable_workspace);
    else
        ground_general_case(immutable_workspace, mutable_workspace);
}

}

#endif
