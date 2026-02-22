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

#include "tyr/planning/lifted_task/abstractions/projection_generator.hpp"

#include "tyr/common/declarations.hpp"
#include "tyr/formalism/planning/builder.hpp"
#include "tyr/formalism/planning/declarations.hpp"
#include "tyr/formalism/planning/merge.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/planning/heuristics/blind.hpp"
#include "tyr/planning/lifted_task/node.hpp"
#include "tyr/planning/ground_task/node.hpp"
#include "tyr/formalism/planning/views.hpp"
#include "tyr/planning/ground_task/successor_generator.hpp"
#include "tyr/planning/ground_task.hpp"
#include "tyr/planning/abstractions/pattern_generator.hpp"
#include "tyr/planning/algorithms/astar_eager.hpp"
#include "tyr/planning/abstractions/projection_generator.hpp"
#include "tyr/planning/algorithms/astar_eager/event_handler.hpp"
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/lifted_task.hpp"

namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;

namespace tyr::planning
{

static Index<fp::Task> create_projected_task(View<Index<fp::Task>, fp::Repository> task,
                                             fp::Repository& destination,
                                             const Pattern& pattern)
{
    auto builder = fp::Builder();
    auto context = fp::MergeContext(builder, destination);
    auto projected_task_ptr = builder.get_builder<fp::Task>();
    auto& projected_task = *projected_task_ptr;
    projected_task.clear();

    for (const auto predicate : task.get_domain().get_predicates<f::StaticTag>()) {}
    // program.static_predicates.push_back(fp::merge_p2d(predicate, context).first);

    for (const auto predicate : task.get_domain().get_predicates<f::FluentTag>()) {}
    // program.fluent_predicates.push_back(fp::merge_p2d(predicate, context).first);

    for (const auto predicate : task.get_domain().get_predicates<f::DerivedTag>()) {}
    // program.fluent_predicates.push_back(
    //     fp::merge_p2d<f::DerivedTag, f::OverlayRepository<fp::Repository>, fd::Repository, f::FluentTag>(predicate, context).first);

    for (const auto predicate : task.get_derived_predicates()) {}
    // program.fluent_predicates.push_back(
    //     fp::merge_p2d<f::DerivedTag, f::OverlayRepository<fp::Repository>, fd::Repository, f::FluentTag>(predicate, context).first);

    for (const auto function : task.get_domain().get_functions<f::StaticTag>()) {}
    // program.static_functions.push_back(fp::merge_p2d(function, context).first);

    for (const auto function : task.get_domain().get_functions<f::FluentTag>()) {}
    // program.fluent_functions.push_back(fp::merge_p2d(function, context).first);

    // We can ignore auxiliary function total-cost because it never occurs in a condition

    for (const auto object : task.get_domain().get_constants()) {}
    // program.objects.push_back(fp::merge_p2d(object, context).first);
    for (const auto object : task.get_objects()) {}
    // program.objects.push_back(fp::merge_p2d(object, context).first);

    for (const auto atom : task.get_atoms<f::StaticTag>()) {}
    // program.static_atoms.push_back(fp::merge_p2d(atom, context).first);

    for (const auto atom : task.get_atoms<f::FluentTag>()) {}
    // program.fluent_atoms.push_back(fp::merge_p2d(atom, context).first);

    for (const auto fterm_value : task.get_fterm_values<f::StaticTag>()) {}
    // program.static_fterm_values.push_back(fp::merge_p2d(fterm_value, context).first);

    for (const auto fterm_value : task.get_fterm_values<f::FluentTag>()) {}
    //.fluent_fterm_values.push_back(fp::merge_p2d(fterm_value, context).first);

    for (const auto action : task.get_domain().get_actions()) {}
    // translate_action_to_delete_free_rules(action, program, context, predicate_to_actions);

    for (const auto axiom : task.get_domain().get_axioms()) {}
    // translate_axiom_to_delete_free_axiom_rules(axiom, program, context, predicate_to_axioms);

    for (const auto axiom : task.get_axioms()) {}
    // translate_axiom_to_delete_free_axiom_rules(axiom, program, context, predicate_to_axioms);

    canonicalize(projected_task);
    return destination.get_or_create(projected_task, builder.get_buffer()).first;
}


class ProjectionEventHandler : public astar_eager::EventHandlerBase<ProjectionEventHandler, LiftedTask>
{
private:
    /* Implement EventHandlerBase interface */
    friend class astar_eager::EventHandlerBase<ProjectionEventHandler, LiftedTask>;

    void on_expand_node_impl(const Node<LiftedTask>& node) const {}

    void on_expand_goal_node_impl(const Node<LiftedTask>& node) const{}

    void on_generate_node_impl(const LabeledNode<LiftedTask>& labeled_succ_node) const{}

    void on_generate_node_relaxed_impl(const LabeledNode<LiftedTask>& labeled_succ_node) const{}

    void on_generate_node_not_relaxed_impl(const LabeledNode<LiftedTask>& labeled_succ_node) const{}

    void on_close_node_impl(const Node<LiftedTask>& node) const{}

    void on_prune_node_impl(const Node<LiftedTask>& node) const{}

    void on_start_search_impl(const Node<LiftedTask>& node, float_t f_value) const{}

    void on_finish_f_layer_impl(float_t f_value, uint64_t num_expanded_states, uint64_t num_generated_states) const{}

    void on_end_search_impl() const{}

    void on_solved_impl(const Plan<LiftedTask>& plan) const{}

    void on_unsolvable_impl() const{}

    void on_exhausted_impl() const{}

public:
    ProjectionEventHandler(size_t verbosity = 0) : astar_eager::EventHandlerBase<ProjectionEventHandler, LiftedTask>(verbosity) { }
};

ProjectionGenerator<LiftedTask>::ProjectionGenerator(LiftedTask& task, const PatternCollection& patterns) : m_task(task), m_patterns(patterns) {}

void ProjectionGenerator<LiftedTask>::generate()
{
    for (const auto& pattern : m_patterns)
    {
        // Step 1: Create the projected task
        auto repository = std::make_shared<fp::Repository>(m_task.get_repository().get());
        auto projected_task = create_projected_task(m_task.get_task(), *repository, pattern);

        // TODO: make sure the projected task is correct
        std::cout << make_view(projected_task, *repository) << std::endl;
        
        // Step 2: Create the lifted projected task
        auto projected_lifted_task = LiftedTask(m_task.get_domain(), repository, make_view(projected_task, *repository), m_task.get_fdr_context());
        
        // Step 3: Ground the lifted projected task
        auto projected_ground_task = projected_lifted_task.get_ground_task();
        
        // Step 4: Fully expand state space while building the projection
        auto event_handler = ProjectionEventHandler(2);
        auto options = astar_eager::Options<GroundTask>();
        options.stop_if_goal = false;
        auto blind_heuristic = BlindHeuristic<GroundTask>();
        auto successor_generator = SuccessorGenerator<GroundTask>(projected_ground_task);
        auto search_result = astar_eager::find_solution(*projected_ground_task, successor_generator, blind_heuristic, options);
    }
}

}
