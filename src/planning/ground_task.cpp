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

#include "tyr/planning/ground_task.hpp"

#include "tyr/formalism/builder.hpp"
#include "tyr/formalism/canonicalization.hpp"
#include "tyr/formalism/formatter.hpp"
#include "tyr/formalism/merge_datalog.hpp"

using namespace tyr::formalism;

namespace tyr::planning
{

GroundTask::GroundTask(DomainPtr domain,
                       RepositoryPtr repository,
                       OverlayRepositoryPtr<Repository> overlay_repository,
                       View<Index<Task>, OverlayRepository<Repository>> task,
                       IndexList<GroundAtom<FluentTag>> fluent_atoms,
                       IndexList<GroundAtom<DerivedTag>> derived_atoms,
                       IndexList<GroundAction> actions,
                       IndexList<GroundAxiom> axioms) :
    m_num_fluent_atoms(fluent_atoms.size()),
    m_num_derived_atoms(derived_atoms.size()),
    m_num_actions(actions.size()),
    m_num_axioms(axioms.size()),
    m_domain(std::move(domain)),
    m_repository(std::make_shared<Repository>()),
    m_overlay_repository(std::make_shared<OverlayRepository<Repository>>(*m_domain->get_repository(), *m_repository))
{
    // std::cout << m_task << std::endl;
}

Node<GroundTask> get_initial_node() {}

void GroundTask::compute_extended_state(UnpackedState<GroundTask>& unpacked_state) {}

std::vector<std::pair<View<Index<GroundAction>, OverlayRepository<Repository>>, Node<GroundTask>>>
GroundTask::get_labeled_successor_nodes(const Node<GroundTask>& node)
{
    auto result = std::vector<std::pair<View<Index<GroundAction>, OverlayRepository<Repository>>, Node<GroundTask>>> {};
    return result;
}

void GroundTask::get_labeled_successor_nodes(const Node<GroundTask>& node,
                                             std::vector<std::pair<View<Index<GroundAction>, OverlayRepository<Repository>>, Node<GroundTask>>>& out_nodes)
{
}

template<formalism::FactKind T>
size_t GroundTask::get_num_atoms() const noexcept
{
    if constexpr (std::is_same_v<T, formalism::FluentTag>)
        return m_num_fluent_atoms;
    else if constexpr (std::is_same_v<T, formalism::DerivedTag>)
        return m_num_derived_atoms;
    else
        static_assert(dependent_false<T>::value, "Missing case");
}

template size_t GroundTask::get_num_atoms<formalism::FluentTag>() const noexcept;
template size_t GroundTask::get_num_atoms<formalism::DerivedTag>() const noexcept;

size_t GroundTask::get_num_actions() const noexcept { return m_num_actions; }

size_t GroundTask::get_num_axioms() const noexcept { return m_num_axioms; }
}
