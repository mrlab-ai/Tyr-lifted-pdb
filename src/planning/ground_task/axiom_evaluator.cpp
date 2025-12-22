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

#include "tyr/planning/ground_task/axiom_evaluator.hpp"

#include "tyr/planning/ground_task/match_tree/match_tree.hpp"

using namespace tyr::formalism;

namespace tyr::planning
{
static void evaluate_axioms_bottomup_for_strata(UnpackedState<GroundTask>& state,
                                                const GroundTask& task,
                                                const match_tree::MatchTree<formalism::GroundAxiom>& match_tree,
                                                IndexList<formalism::GroundAxiom>& applicable_axioms)
{
    while (true)
    {
        auto discovered_new_atom = bool { false };

        applicable_axioms.clear();
        match_tree.generate(state, applicable_axioms);

        for (const auto axiom : applicable_axioms)
        {
            const auto atom = make_view(axiom, task).get_atom().get_index();
        }

        if (!discovered_new_atom)
            break;
    }
}

void evaluate_axioms_bottomup(UnpackedState<GroundTask>& state, const GroundTask& task, IndexList<formalism::GroundAxiom>& applicable_axioms)
{
    auto derived_atoms = state.get_atoms<DerivedTag>;

    for (const auto& match_tree : task.get_axiom_match_tree_strata())
        evaluate_axioms_bottomup_for_strata(state, task, *match_tree, applicable_axioms);
}
}
