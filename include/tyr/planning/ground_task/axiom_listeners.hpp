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

#ifndef TYR_PLANNING_GROUND_TASK_AXIOM_LISTENERS_HPP_
#define TYR_PLANNING_GROUND_TASK_AXIOM_LISTENERS_HPP_

#include "tyr/formalism/ground_atom_view.hpp"
#include "tyr/formalism/ground_literal_view.hpp"
#include "tyr/formalism/overlay_repository.hpp"
#include "tyr/formalism/planning/ground_axiom_view.hpp"
#include "tyr/formalism/planning/ground_fdr_conjunctive_condition_view.hpp"
//
#include "tyr/common/declarations.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/formatter.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/common/types.hpp"
#include "tyr/formalism/declarations.hpp"
#include "tyr/planning/ground_task/axiom_stratification.hpp"

namespace tyr::planning
{
using GroundAxiomListenerStratum = UnorderedMap<Index<formalism::GroundAtom<formalism::DerivedTag>>, UnorderedSet<Index<formalism::GroundAxiom>>>;

struct GroundAxiomListenerStrata
{
    std::vector<GroundAxiomListenerStratum> data;
};

extern GroundAxiomListenerStrata compute_listeners(const GroundAxiomStrata& strata, const formalism::OverlayRepository<formalism::Repository>& context);
}

#endif