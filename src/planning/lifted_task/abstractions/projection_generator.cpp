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
#include "tyr/common/itertools.hpp"
#include "tyr/common/onetbb.hpp"
#include "tyr/formalism/planning/builder.hpp"
#include "tyr/formalism/planning/datas.hpp"
#include "tyr/formalism/planning/declarations.hpp"
#include "tyr/formalism/planning/fdr_context.hpp"
#include "tyr/formalism/planning/merge.hpp"
#include "tyr/formalism/planning/planning_task.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/planning/variable_dependency_graph.hpp"
#include "tyr/formalism/planning/views.hpp"
#include "tyr/planning/abstractions/explicit_projection.hpp"
#include "tyr/planning/abstractions/pattern_generator.hpp"
#include "tyr/planning/abstractions/projection_generator.hpp"
#include "tyr/planning/applicability.hpp"
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/formatter.hpp"
#include "tyr/planning/heuristics/blind.hpp"
#include "tyr/planning/lifted_task.hpp"
#include "tyr/planning/lifted_task/node.hpp"
#include "tyr/planning/lifted_task/state_view.hpp"
#include "tyr/planning/lifted_task/successor_generator.hpp"
#include "tyr/planning/lifted_task/unpacked_state.hpp"

namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;

namespace tyr::planning
{
namespace
{
template<f::FactKind T>
void append_projected_atom(fp::GroundAtomView<T> element, const Pattern& pattern, IndexList<fp::GroundAtom<T>>& ref_projected_atoms, fp::MergeContext& context)
{
    if (pattern.predicates_set.contains(element.get_predicate()))
        ref_projected_atoms.push_back(fp::merge_p2p(element, context).first.get_index());
}

template<f::FactKind T>
void append_projected_literal(fp::GroundLiteralView<T> element,
                              const Pattern& pattern,
                              IndexList<fp::GroundLiteral<T>>& ref_projected_literal,
                              fp::MergeContext& context)
{
    if (pattern.predicates_set.contains(element.get_atom().get_predicate()))
        ref_projected_literal.push_back(fp::merge_p2p(element, context).first.get_index());
}

template<f::FactKind T>
void append_projected_literal(fp::LiteralView<T> element, const Pattern& pattern, IndexList<fp::Literal<T>>& ref_projected_literal, fp::MergeContext& context)
{
    if (pattern.predicates_set.contains(element.get_atom().get_predicate()))
        ref_projected_literal.push_back(fp::merge_p2p(element, context).first.get_index());
}

template<f::FactKind T>
void append_projected_fact(fp::FDRFactView<T> element,
                           const Pattern& pattern,
                           DataList<fp::FDRFact<T>>& ref_projected_facts,
                           fp::MergeContext& context,
                           fp::FDRContext& fdr_context)
{
    if (pattern.facts_set.contains(element))
    {
        const auto [atom, inserted] = fp::merge_p2p(*element.get_atom(), context);
        assert(!inserted);
        ref_projected_facts.push_back(fdr_context.get_fact(atom));
    }
}

auto create_projected_goal(fp::GroundConjunctiveConditionView element, const Pattern& pattern, fp::MergeContext& context, fp::FDRContext& fdr_context)
{
    auto conj_cond_ptr = context.builder.template get_builder<fp::GroundConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    for (const auto literal : element.template get_facts<f::StaticTag>())
        conj_cond.static_literals.push_back(merge_p2p(literal, context).first.get_index());  // always useful to have
    for (const auto fact : element.template get_facts<f::FluentTag>())
        append_projected_fact(fact, pattern, conj_cond.fluent_facts, context, fdr_context);

    canonicalize(conj_cond);
    return context.destination.get_or_create(conj_cond);
}

auto create_projected_conjunctive_condition(fp::ConjunctiveConditionView element, fp::MergeContext& context, const Pattern& pattern)
{
    auto conj_cond_ptr = context.builder.template get_builder<fp::ConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    for (const auto variable : element.get_variables())
        conj_cond.variables.push_back(merge_p2p(variable, context).first.get_index());
    for (const auto literal : element.template get_literals<f::StaticTag>())
        conj_cond.static_literals.push_back(merge_p2p(literal, context).first.get_index());
    for (const auto literal : element.template get_literals<f::FluentTag>())
        append_projected_literal(literal, pattern, conj_cond.fluent_literals, context);

    canonicalize(conj_cond);
    return context.destination.get_or_create(conj_cond);
}

auto create_projected_conjunctive_effect(fp::ConjunctiveEffectView element, fp::MergeContext& context, const Pattern& pattern)
{
    auto conj_effect_ptr = context.builder.template get_builder<fp::ConjunctiveEffect>();
    auto& conj_eff = *conj_effect_ptr;
    conj_eff.clear();

    for (const auto literal : element.get_literals())
        append_projected_literal(literal, pattern, conj_eff.literals, context);

    canonicalize(conj_eff);
    return context.destination.get_or_create(conj_eff);
}

void append_projected_conditional_effect(fp::ConditionalEffectView element,
                                         fp::MergeContext& context,
                                         IndexList<fp::ConditionalEffect>& ref_projected_cond_effect,
                                         const Pattern& pattern)
{
    auto cond_effect_ptr = context.builder.template get_builder<fp::ConditionalEffect>();
    auto& cond_effect = *cond_effect_ptr;
    cond_effect.clear();

    for (const auto variable : element.get_variables())
        cond_effect.variables.push_back(merge_p2p(variable, context).first.get_index());
    cond_effect.condition = create_projected_conjunctive_condition(element.get_condition(), context, pattern).first.get_index();
    cond_effect.effect = create_projected_conjunctive_effect(element.get_effect(), context, pattern).first.get_index();

    canonicalize(cond_effect);
    ref_projected_cond_effect.push_back(context.destination.get_or_create(cond_effect).first.get_index());
}

void append_projected_action(fp::ActionView element, fp::MergeContext& context, IndexList<fp::Action>& ref_projected_actions, const Pattern& pattern)
{
    auto action_ptr = context.builder.template get_builder<fp::Action>();
    auto& action = *action_ptr;
    action.clear();

    action.name = element.get_name();
    action.original_arity = element.get_original_arity();
    for (const auto variable : element.get_variables())
        action.variables.push_back(merge_p2p(variable, context).first.get_index());
    action.condition = create_projected_conjunctive_condition(element.get_condition(), context, pattern).first.get_index();
    for (const auto effect : element.get_effects())
        append_projected_conditional_effect(effect, context, action.effects, pattern);

    canonicalize(action);
    ref_projected_actions.push_back(context.destination.get_or_create(action).first.get_index());
}

auto create_projected_formalism_domain(fp::DomainView element,
                                       std::shared_ptr<fp::Repository> destination,
                                       fp::MergeContext& context,
                                       fp::RepositoryFactoryPtr factory,
                                       const Pattern& pattern)
{
    auto domain_ptr = context.builder.template get_builder<fp::Domain>();
    auto& domain = *domain_ptr;
    domain.clear();

    domain.name = element.get_name();
    for (const auto predicate : element.get_predicates<f::StaticTag>())
        domain.static_predicates.push_back(fp::merge_p2p(predicate, context).first.get_index());
    for (const auto predicate : element.get_predicates<f::FluentTag>())
        domain.fluent_predicates.push_back(fp::merge_p2p(predicate, context).first.get_index());
    for (const auto object : element.get_constants())
        domain.constants.push_back(fp::merge_p2p(object, context).first.get_index());
    for (const auto action : element.get_actions())
        append_projected_action(action, context, domain.actions, pattern);

    canonicalize(domain);
    return fp::PlanningDomain(context.destination.get_or_create(domain).first, std::move(destination), std::move(factory));
}

/// @brief Project the task to the given pattern.
/// An atom is projected away iff the pattern does not mention the predicate
/// A (ground) fact is projected away iff the pattern deos not contain it.
///
/// The FDR mappings may diverge
/// @param planning_task
/// @param pattern
/// @return
auto create_projected_formalism_task(const fp::PlanningTask& planning_task, const Pattern& pattern)
{
    const auto& factory = planning_task.get_domain().get_repository_factory();

    auto destination = factory->create_shared(planning_task.get_repository().get());
    auto builder = fp::Builder();
    // Note: we have to copy the FDRContext to avoid polluting the parent tasks FDRContext with non-existing atoms.
    auto fdr_context = std::make_shared<fp::FDRContext>(*planning_task.get_fdr_context(), builder, destination);
    auto context = fp::MergeContext(builder, *destination);

    const auto project_domain = create_projected_formalism_domain(planning_task.get_domain().get_domain(), destination, context, factory, pattern);

    auto task_ptr = builder.get_builder<fp::Task>();
    auto& task = *task_ptr;
    task.clear();

    task.name = planning_task.get_task().get_name();
    task.domain = project_domain.get_domain().get_index();
    for (const auto predicate : planning_task.get_task().get_derived_predicates())
        task.derived_predicates.push_back(fp::merge_p2p(predicate, context).first.get_index());
    for (const auto object : planning_task.get_task().get_objects())
        task.objects.push_back(fp::merge_p2p(object, context).first.get_index());
    for (const auto atom : planning_task.get_task().get_atoms<f::StaticTag>())
        task.static_atoms.push_back(fp::merge_p2p(atom, context).first.get_index());  // always useful to have
    for (const auto atom : planning_task.get_task().get_atoms<f::FluentTag>())
        append_projected_atom(atom, pattern, task.fluent_atoms, context);
    task.goal = create_projected_goal(planning_task.get_task().get_goal(), pattern, context, *fdr_context).first.get_index();

    canonicalize(task);
    return fp::PlanningTask(destination->get_or_create(task).first, std::move(fdr_context), destination, project_domain);
}

/// @brief Project a state into the projection.
auto project_state(const StateView<LiftedTask>& element, const Pattern& pattern, StateRepository<LiftedTask>& state_repository)
{
    auto uastate = state_repository.get_unregistered_state();
    for (const auto& fact : element.get_fluent_facts_view())
    {
        if (!pattern.facts_set.contains(fact))
            continue;

        uastate->set(fact.get_data());
    }

    return state_repository.register_state(uastate);
}

/// @brief Create all 2^|pattern| abstract states.
/// This ignores reachability but suffices for domains without unsolvable states.
auto create_abstract_states(const Pattern& pattern, LiftedTask& task, StateRepository<LiftedTask>& state_repository)
{
    auto facts = std::vector<fp::FDRFactView<f::FluentTag>>(pattern.facts.begin(), pattern.facts.end());
    auto astates = std::vector<StateView<LiftedTask>> {};
    auto goal_vertices = std::vector<uint_t> {};

    {
        itertools::for_each_boolean_vector(
            [&](auto&& vec)
            {
                auto uastate = state_repository.get_unregistered_state();
                uastate->clear();

                for (uint_t i = 0; i < vec.size(); ++i)
                    if (vec[i])
                        uastate->set(facts[i].get_data());

                const auto astate = state_repository.register_state(uastate);

                const auto state_context = StateContext { task, astate.get_unpacked_state(), float_t { 0 } };
                const auto is_goal = is_dynamically_applicable(task.get_task().get_goal(), state_context);

                if (is_goal)
                    goal_vertices.push_back(uint_t(astate.get_index()));

                astates.push_back(astate);
            },
            pattern.size());
    }

    return std::make_pair(std::move(astates), std::move(goal_vertices));
}

auto create_abstract_transitions(const std::vector<StateView<LiftedTask>>& astates,
                                 const Pattern& pattern,
                                 LiftedTask& task,
                                 SuccessorGenerator<LiftedTask>& successor_generator)
{
    auto labeled_succ_nodes = std::vector<LabeledNode<LiftedTask>> {};

    auto transitions = TransitionList {};
    auto adj_lists = std::vector<std::vector<uint_t>>(astates.size());

    auto& state_repository = *successor_generator.get_state_repository();

    for (const auto& astate : astates)
    {
        auto& adj_row = adj_lists[uint_t(astate.get_index())];

        auto anode = Node<LiftedTask>(astate, float_t { 0 });

        successor_generator.get_labeled_successor_nodes(anode, labeled_succ_nodes);

        for (const auto& labeled_succ_node : labeled_succ_nodes)
        {
            auto pastate = project_state(labeled_succ_node.node.get_state(), pattern, state_repository);

            assert(uint_t(pastate.get_index()) < astates.size());

            const auto t = transitions.size();
            const auto src = uint_t(astate.get_index());
            const auto dst = uint_t(pastate.get_index());

            transitions.emplace_back(labeled_succ_node.label, src, dst);

            adj_row.push_back(t);
        }
    }

    return std::make_pair(transitions, adj_lists);
}

auto create_projection(const Pattern& pattern, const std::shared_ptr<LiftedTask>& task)
{
    auto execution_context = ExecutionContext::create(1);
    auto successor_generator = SuccessorGenerator<LiftedTask>(task, execution_context);
    auto& state_repository = successor_generator.get_state_repository();

    auto [astates, goal_vertices] = create_abstract_states(pattern, *task, *state_repository);
    auto [transitions, adj_lists] = create_abstract_transitions(astates, pattern, *task, successor_generator);

    return ProjectionAbstraction(std::make_shared<const ForwardProjectionAbstraction<LiftedTask>>(ProjectionMapping<LiftedTask>(pattern),
                                                                                                  std::move(astates),
                                                                                                  std::move(transitions),
                                                                                                  std::move(adj_lists),
                                                                                                  std::move(goal_vertices)));
}
}

ProjectionGenerator<LiftedTask>::ProjectionGenerator(std::shared_ptr<const LiftedTask> task, const PatternCollection& patterns) :
    m_task(task),
    m_patterns(patterns)
{
}

ProjectionAbstractionList<LiftedTask> ProjectionGenerator<LiftedTask>::generate()
{
    auto projections = ProjectionAbstractionList<LiftedTask> {};
    for (const auto& pattern : m_patterns)
    {
        const auto projected_task = LiftedTask::create(create_projected_formalism_task(m_task->get_formalism_task(), pattern));

        projections.push_back(create_projection(pattern, projected_task));
    }
    return projections;
}
}
