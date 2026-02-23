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
#include "tyr/formalism/planning/views.hpp"
#include "tyr/planning/abstractions/pattern_generator.hpp"
#include "tyr/planning/abstractions/projection_generator.hpp"
#include "tyr/planning/algorithms/astar_eager.hpp"
#include "tyr/planning/algorithms/astar_eager/event_handler.hpp"
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/ground_task.hpp"
#include "tyr/planning/ground_task/node.hpp"
#include "tyr/planning/ground_task/successor_generator.hpp"
#include "tyr/planning/heuristics/blind.hpp"
#include "tyr/planning/lifted_task.hpp"
#include "tyr/planning/lifted_task/node.hpp"

namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;

namespace tyr::planning
{

template<f::FactKind T>
static void append_projected_atom(View<Index<fp::GroundAtom<T>>, fp::Repository> element,
                                  const Pattern& pattern,
                                  IndexList<fp::GroundAtom<T>>& ref_projected_atoms,
                                  fp::MergeContext& context)
{
    if (pattern.predicates.contains(element.get_predicate().get_index()))
        ref_projected_atoms.push_back(fp::merge_p2p(element, context).first);
}

template<f::FactKind T>
static void append_projected_literal(View<Index<fp::GroundLiteral<T>>, fp::Repository> element,
                                     const Pattern& pattern,
                                     IndexList<fp::GroundLiteral<T>>& ref_projected_literal,
                                     fp::MergeContext& context)
{
    if (pattern.predicates.contains(element.get_atom().get_predicate().get_index()))
        ref_projected_literal.push_back(fp::merge_p2p(element, context).first);
}

template<f::FactKind T>
static void append_projected_literal(View<Index<fp::Literal<T>>, fp::Repository> element,
                                     const Pattern& pattern,
                                     IndexList<fp::Literal<T>>& ref_projected_literal,
                                     fp::MergeContext& context)
{
    if (pattern.predicates.contains(element.get_atom().get_predicate().get_index()))
        ref_projected_literal.push_back(fp::merge_p2p(element, context).first);
}

template<f::FactKind T>
static void append_projected_fact(View<Data<fp::FDRFact<T>>, fp::Repository> element,
                                  const Pattern& pattern,
                                  DataList<fp::FDRFact<T>>& ref_projected_facts,
                                  fp::MergeContext& context)
{
    if (pattern.facts.contains(element.get_data()))
        ref_projected_facts.push_back(fp::merge_p2p(element, context));
}

static auto create_projected_goal(View<Index<fp::GroundConjunctiveCondition>, fp::Repository> element, const Pattern& pattern, fp::MergeContext& context)
{
    auto conj_cond_ptr = context.builder.template get_builder<fp::GroundConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    for (const auto literal : element.template get_facts<f::StaticTag>())
        conj_cond.static_literals.push_back(merge_p2p(literal, context).first);  // always useful to have
    for (const auto fact : element.template get_facts<f::FluentTag>())
        append_projected_fact(fact, pattern, conj_cond.fluent_facts, context);

    canonicalize(conj_cond);
    return context.destination.get_or_create(conj_cond, context.builder.get_buffer());
}

static auto
create_projected_conjunctive_condition(View<Index<fp::ConjunctiveCondition>, fp::Repository> element, fp::MergeContext& context, const Pattern& pattern)
{
    auto conj_cond_ptr = context.builder.template get_builder<fp::ConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    for (const auto variable : element.get_variables())
        conj_cond.variables.push_back(merge_p2p(variable, context).first);
    for (const auto literal : element.template get_literals<f::StaticTag>())
        conj_cond.static_literals.push_back(merge_p2p(literal, context).first);
    for (const auto literal : element.template get_literals<f::FluentTag>())
        append_projected_literal(literal, pattern, conj_cond.fluent_literals, context);

    canonicalize(conj_cond);
    return context.destination.get_or_create(conj_cond, context.builder.get_buffer());
}

static auto create_projected_conjunctive_effect(View<Index<fp::ConjunctiveEffect>, fp::Repository> element, fp::MergeContext& context, const Pattern& pattern)
{
    auto conj_effect_ptr = context.builder.template get_builder<fp::ConjunctiveEffect>();
    auto& conj_eff = *conj_effect_ptr;
    conj_eff.clear();

    for (const auto literal : element.get_literals())
        append_projected_literal(literal, pattern, conj_eff.literals, context);

    canonicalize(conj_eff);
    return context.destination.get_or_create(conj_eff, context.builder.get_buffer());
}

static void append_projected_conditional_effect(View<Index<fp::ConditionalEffect>, fp::Repository> element,
                                                fp::MergeContext& context,
                                                IndexList<fp::ConditionalEffect>& ref_projected_cond_effect,
                                                const Pattern& pattern)
{
    auto cond_effect_ptr = context.builder.template get_builder<fp::ConditionalEffect>();
    auto& cond_effect = *cond_effect_ptr;
    cond_effect.clear();

    for (const auto variable : element.get_variables())
        cond_effect.variables.push_back(merge_p2p(variable, context).first);
    cond_effect.condition = create_projected_conjunctive_condition(element.get_condition(), context, pattern).first;
    cond_effect.effect = create_projected_conjunctive_effect(element.get_effect(), context, pattern).first;

    canonicalize(cond_effect);
    ref_projected_cond_effect.push_back(context.destination.get_or_create(cond_effect, context.builder.get_buffer()).first);
}

static void append_projected_action(View<Index<fp::Action>, fp::Repository> element,
                                    fp::MergeContext& context,
                                    IndexList<fp::Action>& ref_projected_actions,
                                    const Pattern& pattern)
{
    auto action_ptr = context.builder.template get_builder<fp::Action>();
    auto& action = *action_ptr;
    action.clear();

    action.name = element.get_name();
    action.original_arity = element.get_original_arity();
    for (const auto variable : element.get_variables())
        action.variables.push_back(merge_p2p(variable, context).first);
    action.condition = create_projected_conjunctive_condition(element.get_condition(), context, pattern).first;
    for (const auto effect : element.get_effects())
        append_projected_conditional_effect(effect, context, action.effects, pattern);

    canonicalize(action);
    ref_projected_actions.push_back(context.destination.get_or_create(action, context.builder.get_buffer()).first);
}

static auto create_projected_domain(View<Index<fp::Domain>, fp::Repository> element, fp::MergeContext& context, const Pattern& pattern)
{
    auto domain_ptr = context.builder.template get_builder<fp::Domain>();
    auto& domain = *domain_ptr;
    domain.clear();

    domain.name = element.get_name();
    for (const auto predicate : element.get_predicates<f::StaticTag>())
        domain.static_predicates.push_back(fp::merge_p2p(predicate, context).first);
    for (const auto predicate : element.get_predicates<f::FluentTag>())
        domain.fluent_predicates.push_back(fp::merge_p2p(predicate, context).first);
    for (const auto object : element.get_constants())
        domain.constants.push_back(fp::merge_p2p(object, context).first);

    for (const auto action : element.get_actions())
        append_projected_action(action, context, domain.actions, pattern);

    canonicalize(domain);
    return context.destination.get_or_create(domain, context.builder.get_buffer());
}

static Index<fp::Task> create_projected_task(View<Index<fp::Task>, fp::Repository> element, fp::Repository& destination, const Pattern& pattern)
{
    auto builder = fp::Builder();
    auto context = fp::MergeContext(builder, destination);
    auto task_ptr = builder.get_builder<fp::Task>();
    auto& task = *task_ptr;
    task.clear();

    task.name = element.get_name();
    task.domain = create_projected_domain(element.get_domain(), context, pattern).first;

    for (const auto predicate : element.get_derived_predicates())
        task.derived_predicates.push_back(fp::merge_p2p(predicate, context).first);

    for (const auto object : element.get_objects())
        task.objects.push_back(fp::merge_p2p(object, context).first);

    for (const auto atom : element.get_atoms<f::StaticTag>())
        task.static_atoms.push_back(fp::merge_p2p(atom, context).first);  // always useful to have

    for (const auto atom : element.get_atoms<f::FluentTag>())
        append_projected_atom(atom, pattern, task.fluent_atoms, context);

    task.goal = create_projected_goal(element.get_goal(), pattern, context).first;

    canonicalize(task);
    return destination.get_or_create(task, builder.get_buffer()).first;
}

class ProjectionEventHandler : public astar_eager::EventHandlerBase<ProjectionEventHandler, GroundTask>
{
private:
    /* Implement EventHandlerBase interface */
    friend class astar_eager::EventHandlerBase<ProjectionEventHandler, GroundTask>;

    void on_expand_node_impl(const Node<GroundTask>& node) const {}

    void on_expand_goal_node_impl(const Node<GroundTask>& node) const {}

    void on_generate_node_impl(const LabeledNode<GroundTask>& labeled_succ_node) const {}

    void on_generate_node_relaxed_impl(const LabeledNode<GroundTask>& labeled_succ_node) const {}

    void on_generate_node_not_relaxed_impl(const LabeledNode<GroundTask>& labeled_succ_node) const {}

    void on_close_node_impl(const Node<GroundTask>& node) const {}

    void on_prune_node_impl(const Node<GroundTask>& node) const {}

    void on_start_search_impl(const Node<GroundTask>& node, float_t f_value) const {}

    void on_finish_f_layer_impl(float_t f_value, uint64_t num_expanded_states, uint64_t num_generated_states) const {}

    void on_end_search_impl() const {}

    void on_solved_impl(const Plan<GroundTask>& plan) const {}

    void on_unsolvable_impl() const {}

    void on_exhausted_impl() const {}

public:
    ProjectionEventHandler(size_t verbosity = 0) : astar_eager::EventHandlerBase<ProjectionEventHandler, GroundTask>(verbosity) {}

    static astar_eager::EventHandlerPtr<GroundTask> create(size_t verbosity = 0) { return std::make_shared<ProjectionEventHandler>(verbosity); }
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
        std::cout << make_view(projected_task, *repository).get_domain() << std::endl;
        std::cout << make_view(projected_task, *repository) << std::endl;

        // Step 2: Create the lifted projected task
        auto projected_lifted_task = LiftedTask(m_task.get_domain(), repository, make_view(projected_task, *repository), m_task.get_fdr_context());

        // Step 3: Ground the lifted projected task
        auto projected_ground_task = projected_lifted_task.get_ground_task();

        // Step 4: Fully expand state space while building the projection
        auto event_handler = ProjectionEventHandler::create(2);
        auto options = astar_eager::Options<GroundTask>();
        options.stop_if_goal = false;
        // options.event_handler = event_handler;
        auto blind_heuristic = BlindHeuristic<GroundTask>();
        auto successor_generator = SuccessorGenerator<GroundTask>(projected_ground_task);
        auto search_result = astar_eager::find_solution(*projected_ground_task, successor_generator, blind_heuristic, options);
    }
}

}
