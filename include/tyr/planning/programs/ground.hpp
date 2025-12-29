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

#ifndef TYR_PLANNING_PROGRAMS_GROUND_TASK_HPP_
#define TYR_PLANNING_PROGRAMS_GROUND_TASK_HPP_

#include "tyr/analysis/domains.hpp"
#include "tyr/analysis/listeners.hpp"
#include "tyr/analysis/stratification.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/formalism/datalog/repository.hpp"
#include "tyr/formalism/datalog/views.hpp"
#include "tyr/formalism/overlay_repository.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/planning/views.hpp"
#include "tyr/planning/declarations.hpp"

namespace tyr::planning
{

class GroundTaskProgram
{
public:
    // Mapping from program rule to task action; there may be multiple actions
    using AppPredicateToActionsMapping = UnorderedMap<Index<formalism::Predicate<formalism::FluentTag>>, std::vector<Index<formalism::planning::Action>>>;

    using AppPredicateToAxiomsMapping = UnorderedMap<Index<formalism::Predicate<formalism::FluentTag>>, std::vector<Index<formalism::planning::Axiom>>>;

    explicit GroundTaskProgram(View<Index<formalism::planning::Task>, formalism::OverlayRepository<formalism::planning::Repository>> task);

    const AppPredicateToActionsMapping& get_predicate_to_actions_mapping() const noexcept;
    const AppPredicateToAxiomsMapping& get_predicate_to_axioms_mapping() const noexcept;
    View<Index<formalism::datalog::Program>, formalism::datalog::Repository> get_program() const noexcept;
    const formalism::datalog::RepositoryPtr& get_repository() const noexcept;
    const analysis::ProgramVariableDomains& get_domains() const noexcept;
    const analysis::RuleStrata& get_strata() const noexcept;
    const analysis::ListenerStrata& get_listeners() const noexcept;

private:
    AppPredicateToActionsMapping m_predicate_to_actions;
    AppPredicateToAxiomsMapping m_predicate_to_axioms;

    formalism::datalog::RepositoryPtr m_repository;
    View<Index<formalism::datalog::Program>, formalism::datalog::Repository> m_program;

    analysis::ProgramVariableDomains m_domains;
    analysis::RuleStrata m_strata;
    analysis::ListenerStrata m_listeners;
};

}

#endif