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

#include "tyr/grounder/execution_contexts.hpp"

#include "tyr/formalism/formatter.hpp"
#include "tyr/formalism/grounder_datalog.hpp"

using namespace tyr::formalism;

namespace tyr::grounder
{
/**
 * FactsExecutionContext
 */

FactsExecutionContext::FactsExecutionContext(View<Index<Program>, Repository> program, const analysis::ProgramVariableDomains& domains) :
    fact_sets(program),
    assignment_sets(program, domains, fact_sets)
{
}

FactsExecutionContext::FactsExecutionContext(View<Index<Program>, Repository> program,
                                             TaggedFactSets<FluentTag, Repository> fluent_facts,
                                             const analysis::ProgramVariableDomains& domains) :
    fact_sets(program, fluent_facts),
    assignment_sets(program, domains, fact_sets)
{
}

template<FactKind T>
void FactsExecutionContext::reset() noexcept
{
    fact_sets.template reset<T>();
    assignment_sets.template reset<T>();
}

template void FactsExecutionContext::reset<StaticTag>() noexcept;
template void FactsExecutionContext::reset<FluentTag>() noexcept;

void FactsExecutionContext::reset() noexcept
{
    fact_sets.reset();
    assignment_sets.reset();
}

template<FactKind T>
void FactsExecutionContext::insert(View<IndexList<GroundAtom<T>>, Repository> view)
{
    fact_sets.insert(view);
    assignment_sets.insert(fact_sets.template get<T>());
}

template void FactsExecutionContext::insert(View<IndexList<GroundAtom<StaticTag>>, Repository> view);
template void FactsExecutionContext::insert(View<IndexList<GroundAtom<FluentTag>>, Repository> view);

template<FactKind T>
void FactsExecutionContext::insert(View<IndexList<GroundFunctionTermValue<T>>, Repository> view)
{
    fact_sets.insert(view);
    assignment_sets.insert(fact_sets.template get<T>());
}

template void FactsExecutionContext::insert(View<IndexList<GroundFunctionTermValue<StaticTag>>, Repository> view);
template void FactsExecutionContext::insert(View<IndexList<GroundFunctionTermValue<FluentTag>>, Repository> view);

/**
 * RuleStageExecutionContext
 */

RuleStageExecutionContext::RuleStageExecutionContext() : repository(std::make_shared<Repository>()), binding(), ground_heads(), merge_cache() {}

void RuleStageExecutionContext::clear() noexcept
{
    repository->clear();
    ground_heads.clear();
    merge_cache.clear();
}

/**
 * RuleExecutionContext
 */

RuleExecutionContext::RuleExecutionContext(View<Index<Rule>, Repository> rule,
                                           View<Index<GroundConjunctiveCondition>, Repository> nullary_condition,
                                           View<Index<formalism::ConjunctiveCondition>, formalism::Repository> arity_geq_1_overapproximation_condition,
                                           View<Index<formalism::ConjunctiveCondition>, formalism::Repository> arity_geq_2_overapproximation_condition,
                                           View<Index<formalism::ConjunctiveCondition>, formalism::Repository> conflicting_overapproximation_condition,
                                           const analysis::DomainListList& parameter_domains,
                                           const TaggedAssignmentSets<StaticTag, Repository>& static_assignment_sets,
                                           const Repository& parent) :
    rule(rule),
    nullary_condition(nullary_condition),
    arity_geq_1_overapproximation_condition(arity_geq_1_overapproximation_condition),
    arity_geq_2_overapproximation_condition(arity_geq_2_overapproximation_condition),
    conflicting_overapproximation_condition(conflicting_overapproximation_condition),
    static_consistency_graph(rule.get_body(),
                             arity_geq_1_overapproximation_condition,
                             arity_geq_2_overapproximation_condition,
                             parameter_domains,
                             0,
                             rule.get_arity(),
                             static_assignment_sets),
    consistency_graph(grounder::kpkc::allocate_dense_graph(static_consistency_graph)),
    kpkc_workspace(grounder::kpkc::allocate_workspace(static_consistency_graph)),
    repository(std::make_shared<Repository>()),  // we have to use pointer, since the RuleExecutionContext is moved into a vector
    overlay_repository(parent, *repository),
    binding(),
    ground_heads()
{
}

void RuleExecutionContext::clear() noexcept
{
    repository->clear();
    ground_heads.clear();
}

void RuleExecutionContext::initialize(const AssignmentSets<Repository>& assignment_sets)
{
    grounder::kpkc::initialize_dense_graph_and_workspace(static_consistency_graph, assignment_sets, consistency_graph, kpkc_workspace);
}

/**
 * ThreadExecutionContext
 */

void ThreadExecutionContext::clear() noexcept { merge_cache.clear(); }

/**
 * ProgramToTaskExecutionContext
 */

void ProgramToTaskExecutionContext::clear() noexcept { merge_cache.clear(); }

/**
 * TaskToProgramExecutionContext
 */

void TaskToProgramExecutionContext::clear() noexcept { merge_cache.clear(); }

/**
 * TaskToTaskExecutionContext
 */

void TaskToTaskExecutionContext::clear() noexcept { merge_cache.clear(); }

/**
 * ProgramExecutionContext
 */

struct MaxArityResult
{
    size_t constraint = 0;
    size_t fterm = 0;

    friend MaxArityResult max_arity(const MaxArityResult& lhs, const MaxArityResult& rhs) noexcept
    {
        return MaxArityResult { std::max(lhs.constraint, rhs.constraint), std::max(lhs.fterm, rhs.fterm) };
    }

    size_t arity() const noexcept { return std::max(constraint, fterm); }
    bool is_nullary() const noexcept { return constraint == 0 && fterm == 0; }
};

inline MaxArityResult max_arity(float_t element);

template<ArithmeticOpKind O, Context C>
MaxArityResult max_arity(View<Index<UnaryOperator<O, Data<FunctionExpression>>>, C> element);

template<OpKind O, Context C>
MaxArityResult max_arity(View<Index<BinaryOperator<O, Data<FunctionExpression>>>, C> element);

template<ArithmeticOpKind O, Context C>
MaxArityResult max_arity(View<Index<MultiOperator<O, Data<FunctionExpression>>>, C> element);

template<FactKind T, Context C>
MaxArityResult max_arity(View<Index<FunctionTerm<T>>, C> element);

template<Context C>
MaxArityResult max_arity(View<Data<FunctionExpression>, C> element);

template<Context C>
MaxArityResult max_arity(View<Data<ArithmeticOperator<Data<FunctionExpression>>>, C> element);

template<Context C>
MaxArityResult max_arity(View<Data<BooleanOperator<Data<FunctionExpression>>>, C> element);

inline MaxArityResult max_arity(float_t element) { return MaxArityResult(); }

template<ArithmeticOpKind O, Context C>
MaxArityResult max_arity(View<Index<UnaryOperator<O, Data<FunctionExpression>>>, C> element)
{
    return max_arity(element.get_arg());
}

template<OpKind O, Context C>
MaxArityResult max_arity(View<Index<BinaryOperator<O, Data<FunctionExpression>>>, C> element)
{
    return max_arity(max_arity(element.get_lhs()), max_arity(element.get_rhs()));
}

template<ArithmeticOpKind O, Context C>
MaxArityResult max_arity(View<Index<MultiOperator<O, Data<FunctionExpression>>>, C> element)
{
    const auto child_fexprs = element.get_args();

    return std::accumulate(std::next(child_fexprs.begin()),  // Start from the second expression
                           child_fexprs.end(),
                           max_arity(child_fexprs.front()),
                           [&](const auto& value, const auto& child_expr) { return max_arity(value, max_arity(child_expr)); });
}

template<FactKind T, Context C>
MaxArityResult max_arity(View<Index<FunctionTerm<T>>, C> element)
{
    return MaxArityResult { 0, element.get_function().get_arity() };
}

template<Context C>
MaxArityResult max_arity(View<Data<FunctionExpression>, C> element)
{
    return visit([&](auto&& arg) { return max_arity(arg); }, element.get_variant());
}

template<Context C>
MaxArityResult max_arity(View<Data<ArithmeticOperator<Data<FunctionExpression>>>, C> element)
{
    return visit([&](auto&& arg) { return max_arity(arg); }, element.get_variant());
}

template<Context C>
MaxArityResult max_arity(View<Data<BooleanOperator<Data<FunctionExpression>>>, C> element)
{
    return visit([&](auto&& arg) { return max_arity(arg); }, element.get_variant());
}

static auto create_ground_nullary_condition(View<Index<ConjunctiveCondition>, Repository> condition, Builder& builder, Repository& context)
{
    auto conj_cond_ptr = builder.get_builder<GroundConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    auto binding_empty = IndexList<Object> {};
    auto grounder_context = GrounderContext { builder, context, binding_empty };

    for (const auto literal : condition.get_literals<StaticTag>())
        if (literal.get_atom().get_predicate().get_arity() == 0)
            conj_cond.static_literals.push_back(ground_datalog(literal, grounder_context).first);

    for (const auto literal : condition.get_literals<FluentTag>())
        if (literal.get_atom().get_predicate().get_arity() == 0)
            conj_cond.fluent_literals.push_back(ground_datalog(literal, grounder_context).first);

    for (const auto numeric_constraint : condition.get_numeric_constraints())
        if (max_arity(numeric_constraint).is_nullary())
            conj_cond.numeric_constraints.push_back(ground_common(numeric_constraint, grounder_context));

    canonicalize(conj_cond);
    return context.get_or_create(conj_cond, builder.get_buffer());
}

static auto create_arity_geq_k_overapproximation_conjunctive_condition(size_t k,
                                                                       View<Index<ConjunctiveCondition>, Repository> condition,
                                                                       Builder& builder,
                                                                       Repository& context)
{
    auto conj_cond_ptr = builder.get_builder<ConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    for (const auto literal : condition.get_literals<StaticTag>())
        if ((!literal.get_polarity() && literal.get_atom().get_predicate().get_arity() == k)
            || (literal.get_polarity() && literal.get_atom().get_predicate().get_arity() >= k))
            conj_cond.static_literals.push_back(literal.get_index());

    for (const auto literal : condition.get_literals<FluentTag>())
        if ((!literal.get_polarity() && literal.get_atom().get_predicate().get_arity() == k)
            || (literal.get_polarity() && literal.get_atom().get_predicate().get_arity() >= k))
            conj_cond.fluent_literals.push_back(literal.get_index());

    for (const auto numeric_constraint : condition.get_numeric_constraints())
        if (max_arity(numeric_constraint).arity() >= k)
            conj_cond.numeric_constraints.push_back(numeric_constraint.get_data());

    canonicalize(conj_cond);
    return context.get_or_create(conj_cond, builder.get_buffer());
}

static auto
create_overapproximation_conflicting_conjunctive_condition(View<Index<ConjunctiveCondition>, Repository> condition, Builder& builder, Repository& context)
{
    auto conj_cond_ptr = builder.get_builder<ConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    for (const auto literal : condition.get_literals<StaticTag>())
        if (literal.get_atom().get_predicate().get_arity() > 2)
            conj_cond.static_literals.push_back(literal.get_index());

    for (const auto literal : condition.get_literals<FluentTag>())
        if (literal.get_atom().get_predicate().get_arity() > 2)
            conj_cond.fluent_literals.push_back(literal.get_index());

    for (const auto numeric_constraint : condition.get_numeric_constraints())
        if (max_arity(numeric_constraint).arity() >= 2)
            conj_cond.numeric_constraints.push_back(numeric_constraint.get_data());

    canonicalize(conj_cond);
    return context.get_or_create(conj_cond, builder.get_buffer());
}

ProgramExecutionContext::ProgramExecutionContext(View<Index<Program>, Repository> program,
                                                 RepositoryPtr repository,
                                                 const analysis::ProgramVariableDomains& domains,
                                                 const analysis::RuleStrata& strata,
                                                 const analysis::ListenerStrata& listeners) :
    program(program),
    repository(repository),
    domains(domains),
    strata(strata),
    listeners(listeners),
    rule_scheduler_strata(create_rule_scheduler_strata(strata, listeners, *repository)),
    builder(),
    facts_execution_context(program, domains),
    rule_execution_contexts(),
    rule_stage_execution_contexts(),
    thread_execution_contexts(),
    planning_execution_context(),
    program_to_task_execution_context(),
    task_to_task_execution_context(),
    task_to_program_execution_context()
{
    for (uint_t i = 0; i < program.get_rules().size(); ++i)
    {
        rule_execution_contexts.emplace_back(
            program.get_rules()[i],
            make_view(create_ground_nullary_condition(program.get_rules()[i].get_body(), builder, *repository).first, *repository),
            make_view(create_arity_geq_k_overapproximation_conjunctive_condition(1, program.get_rules()[i].get_body(), builder, *repository).first,
                      *repository),
            make_view(create_arity_geq_k_overapproximation_conjunctive_condition(2, program.get_rules()[i].get_body(), builder, *repository).first,
                      *repository),
            make_view(create_overapproximation_conflicting_conjunctive_condition(program.get_rules()[i].get_body(), builder, *repository).first, *repository),
            domains.rule_domains[i],
            facts_execution_context.assignment_sets.static_sets,
            *repository);
        rule_execution_contexts.back().initialize(facts_execution_context.assignment_sets);
    }
    rule_stage_execution_contexts.resize(rule_execution_contexts.size());
}

}
