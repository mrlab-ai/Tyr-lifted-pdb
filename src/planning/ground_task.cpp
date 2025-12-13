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
#include "tyr/formalism/merge.hpp"

using namespace tyr::formalism;

namespace tyr::planning
{

static auto create_fdr_conjunctive_condition(View<Index<GroundConjunctiveCondition>, OverlayRepository<Repository>> element,
                                             const UnorderedMap<Index<GroundAtom<FluentTag>>, Index<GroundAtom<FluentTag>>>& fluent_atoms_mapping,
                                             const UnorderedMap<Index<GroundAtom<DerivedTag>>, Index<GroundAtom<DerivedTag>>>& derived_atoms_mapping,
                                             const UnorderedMap<Index<GroundAtom<FluentTag>>, Data<FDRFact<FluentTag>>>& fluent_variables_mapping,
                                             const UnorderedMap<Index<GroundAtom<DerivedTag>>, Data<FDRFact<DerivedTag>>>& derived_variables_mapping,
                                             Builder& builder,
                                             OverlayRepository<Repository>& repository)
{
    auto fdr_conj_cond_ptr = builder.get_builder<FDRConjunctiveCondition>();
    auto& fdr_conj_cond = *fdr_conj_cond_ptr;
    fdr_conj_cond.clear();

    canonicalize(fdr_conj_cond);
    return repository.get_or_create(fdr_conj_cond, builder.get_buffer()).first;
}

static auto create_fdr_action(View<Index<GroundAction>, OverlayRepository<Repository>> element,
                              const UnorderedMap<Index<GroundAtom<FluentTag>>, Index<GroundAtom<FluentTag>>>& fluent_atoms_mapping,
                              const UnorderedMap<Index<GroundAtom<DerivedTag>>, Index<GroundAtom<DerivedTag>>>& derived_atoms_mapping,
                              const UnorderedMap<Index<GroundAtom<FluentTag>>, Data<FDRFact<FluentTag>>>& fluent_variables_mapping,
                              const UnorderedMap<Index<GroundAtom<DerivedTag>>, Data<FDRFact<DerivedTag>>>& derived_variables_mapping,
                              Builder& builder,
                              OverlayRepository<Repository>& repository)
{
    auto fdr_action_ptr = builder.get_builder<FDRAction>();
    auto& fdr_action = *fdr_action_ptr;
    fdr_action.clear();

    canonicalize(fdr_action);
    return repository.get_or_create(fdr_action, builder.get_buffer()).first;
}

static auto create_fdr_axiom(View<Index<GroundAxiom>, OverlayRepository<Repository>> element,
                             const UnorderedMap<Index<GroundAtom<FluentTag>>, Index<GroundAtom<FluentTag>>>& fluent_atoms_mapping,
                             const UnorderedMap<Index<GroundAtom<DerivedTag>>, Index<GroundAtom<DerivedTag>>>& derived_atoms_mapping,
                             const UnorderedMap<Index<GroundAtom<FluentTag>>, Data<FDRFact<FluentTag>>>& fluent_variables_mapping,
                             const UnorderedMap<Index<GroundAtom<DerivedTag>>, Data<FDRFact<DerivedTag>>>& derived_variables_mapping,
                             Builder& builder,
                             OverlayRepository<Repository>& repository)
{
    auto fdr_axiom_ptr = builder.get_builder<FDRAxiom>();
    auto& fdr_axiom = *fdr_axiom_ptr;
    fdr_axiom.clear();

    canonicalize(fdr_axiom);
    return repository.get_or_create(fdr_axiom, builder.get_buffer()).first;
}

template<FactKind T>
auto create_fdr_variables(const std::vector<IndexList<GroundAtom<T>>>& mutexes, Builder& builder, OverlayRepository<Repository>& repository)
{
    auto variables = IndexList<formalism::FDRVariable<T>> {};
    auto mapping = UnorderedMap<Index<GroundAtom<T>>, Data<FDRFact<T>>> {};

    auto fdr_variable_ptr = builder.get_builder<FDRVariable<T>>();
    auto& fdr_variable = *fdr_variable_ptr;

    for (const auto& group : mutexes)
    {
        // Create FDRVariable
        fdr_variable.clear();
        fdr_variable.domain_size = group.size() + 1;
        for (const auto atom : group)
            fdr_variable.atoms.push_back(atom);

        canonicalize(fdr_variable);
        const auto new_fdr_variable = repository.get_or_create(fdr_variable, builder.get_buffer()).first;

        variables.push_back(new_fdr_variable.get_index());

        // Create mapping
        for (uint_t i = 0; i < group.size(); ++i)
        {
            const auto atom = group[i];
            mapping.emplace(atom, Data<FDRFact<T>>(new_fdr_variable.get_index(), FDRValue { i }));
        }
    }

    return std::make_pair(std::move(variables), std::move(mapping));
}

auto create_task(View<Index<Task>, OverlayRepository<Repository>> task,
                 View<IndexList<GroundAtom<FluentTag>>, OverlayRepository<Repository>> fluent_atoms,
                 View<IndexList<GroundAtom<DerivedTag>>, OverlayRepository<Repository>> derived_atoms,
                 View<IndexList<GroundAction>, OverlayRepository<Repository>> actions,
                 View<IndexList<GroundAxiom>, OverlayRepository<Repository>> axioms,
                 OverlayRepository<Repository>& repository)
{
    auto builder = Builder();
    auto fdr_task_ptr = builder.get_builder<FDRTask>();
    auto& fdr_task = *fdr_task_ptr;

    auto fluent_atoms_mapping = UnorderedMap<Index<GroundAtom<FluentTag>>, Index<GroundAtom<FluentTag>>> {};
    auto derived_atoms_mapping = UnorderedMap<Index<GroundAtom<DerivedTag>>, Index<GroundAtom<DerivedTag>>> {};

    fdr_task.name = task.get_name();
    fdr_task.domain = task.get_domain().get_index();
    for (const auto predicate : task.get_derived_predicates())
        fdr_task.derived_predicates.push_back(merge(predicate, builder, repository).get_index());
    for (const auto atom : task.get_atoms<StaticTag>())
        fdr_task.static_atoms.push_back(merge(atom, builder, repository).get_index());
    for (const auto atom : fluent_atoms)
    {
        const auto new_atom = merge(atom, builder, repository);
        fdr_task.fluent_atoms.push_back(new_atom.get_index());
        fluent_atoms_mapping.emplace(atom.get_index(), new_atom.get_index());
    }
    for (const auto atom : derived_atoms)
    {
        const auto new_atom = merge(atom, builder, repository);
        fdr_task.derived_atoms.push_back(new_atom.get_index());
        derived_atoms_mapping.emplace(atom.get_index(), new_atom.get_index());
    }
    for (const auto fterm_value : task.get_fterm_values<StaticTag>())
        fdr_task.static_fterm_values.push_back(merge(fterm_value, builder, repository).get_index());
    for (const auto fterm_value : task.get_fterm_values<FluentTag>())
        fdr_task.fluent_fterm_values.push_back(merge(fterm_value, builder, repository).get_index());
    if (task.get_auxiliary_fterm_value().has_value())
        fdr_task.auxiliary_fterm_value = merge(task.get_auxiliary_fterm_value().value(), builder, repository).get_index();
    if (task.get_metric())
        fdr_task.metric = merge(task.get_metric().value(), builder, repository).get_index();
    for (const auto axiom : task.get_axioms())
        fdr_task.axioms.push_back(merge(axiom, builder, repository).get_index());

    /// --- Create binary mutex groups as baseline; TODO: compute stronger mutexes
    auto fluent_mutex_groups = std::vector<IndexList<GroundAtom<FluentTag>>> {};
    for (const auto atom : fdr_task.fluent_atoms)
        fluent_mutex_groups.push_back(IndexList<GroundAtom<FluentTag>> { atom });
    auto derived_mutex_groups = std::vector<IndexList<GroundAtom<DerivedTag>>> {};
    for (const auto atom : fdr_task.derived_atoms)
        derived_mutex_groups.push_back(IndexList<GroundAtom<DerivedTag>> { atom });

    /// --- Create FDR variables
    auto [fluent_variables_, fluent_variable_mapping] = create_fdr_variables(fluent_mutex_groups, builder, repository);
    fdr_task.fluent_variables = fluent_variables_;

    auto [derived_variables_, derived_variable_mapping] = create_fdr_variables(derived_mutex_groups, builder, repository);
    fdr_task.derived_variables = derived_variables_;

    /// --- Create FDR fluent facts
    for (const auto atom : task.get_atoms<FluentTag>())
        if (fluent_atoms_mapping.contains(atom.get_index()))
            fdr_task.fluent_facts.push_back(fluent_variable_mapping.at(fluent_atoms_mapping.at(atom.get_index())));

    /// --- Create FDR goal
    fdr_task.goal = create_fdr_conjunctive_condition(task.get_goal(),
                                                     fluent_atoms_mapping,
                                                     derived_atoms_mapping,
                                                     fluent_variable_mapping,
                                                     derived_variable_mapping,
                                                     builder,
                                                     repository)
                        .get_index();

    /// --- Create FDR actions and axioms
    for (const auto action : actions)
        fdr_task.ground_actions.push_back(
            create_fdr_action(action, fluent_atoms_mapping, derived_atoms_mapping, fluent_variable_mapping, derived_variable_mapping, builder, repository)
                .get_index());
    for (const auto axiom : axioms)
        fdr_task.ground_axioms.push_back(
            create_fdr_axiom(axiom, fluent_atoms_mapping, derived_atoms_mapping, fluent_variable_mapping, derived_variable_mapping, builder, repository)
                .get_index());

    canonicalize(fdr_task);
    return repository.get_or_create(fdr_task, builder.get_buffer()).first;
}

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
    m_overlay_repository(std::make_shared<OverlayRepository<Repository>>(*m_domain->get_repository(), *m_repository)),
    m_task(create_task(task,
                       make_view(fluent_atoms, *overlay_repository),
                       make_view(derived_atoms, *overlay_repository),
                       make_view(actions, *overlay_repository),
                       make_view(axioms, *overlay_repository),
                       *m_overlay_repository)),
    m_fluent_layout(create_layouts<FluentTag, OverlayRepository<Repository>, uint_t>(m_task.get_variables<FluentTag>())),
    m_derived_layout(create_layouts<DerivedTag, OverlayRepository<Repository>, uint_t>(m_task.get_variables<DerivedTag>()))
{
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
