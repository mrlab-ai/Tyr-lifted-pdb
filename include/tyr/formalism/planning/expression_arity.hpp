/*
 * Copyright (C) 2025-2026 Dominik Drexler
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

#ifndef TYR_FORMALISM_PLANNING_EXPRESSION_ARITY_HPP_
#define TYR_FORMALISM_PLANNING_EXPRESSION_ARITY_HPP_

#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/formalism/planning/declarations.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/planning/views.hpp"

#include <numeric>

namespace tyr::formalism::planning
{

/**
 * collect_parameters
 */

inline void collect_parameters(float_t element, UnorderedSet<ParameterIndex>& result);

template<Context C>
void collect_parameters(View<Data<Term>, C> element, UnorderedSet<ParameterIndex>& result);

template<FactKind T, Context C>
void collect_parameters(View<Index<Atom<T>>, C> element, UnorderedSet<ParameterIndex>& result);

template<FactKind T, Context C>
void collect_parameters(View<Index<Literal<T>>, C> element, UnorderedSet<ParameterIndex>& result);

template<FactKind T, Context C>
void collect_parameters(View<Data<FDRFact<T>>, C> element, UnorderedSet<ParameterIndex>& result);

template<FactKind T, Context C>
void collect_parameters(View<Index<FunctionTerm<T>>, C> element, UnorderedSet<ParameterIndex>& result);

template<Context C>
void collect_parameters(View<Data<FunctionExpression>, C> element, UnorderedSet<ParameterIndex>& result);

template<ArithmeticOpKind O, Context C>
void collect_parameters(View<Index<UnaryOperator<O, Data<FunctionExpression>>>, C> element, UnorderedSet<ParameterIndex>& result);

template<OpKind O, Context C>
void collect_parameters(View<Index<BinaryOperator<O, Data<FunctionExpression>>>, C> element, UnorderedSet<ParameterIndex>& result);

template<ArithmeticOpKind O, Context C>
void collect_parameters(View<Index<MultiOperator<O, Data<FunctionExpression>>>, C> element, UnorderedSet<ParameterIndex>& result);

template<Context C>
void collect_parameters(View<Data<ArithmeticOperator<Data<FunctionExpression>>>, C> element, UnorderedSet<ParameterIndex>& result);

template<Context C>
void collect_parameters(View<Data<BooleanOperator<Data<FunctionExpression>>>, C> element, UnorderedSet<ParameterIndex>& result);

template<Context C>
void collect_parameters(View<Index<ConjunctiveCondition>, C> element, UnorderedSet<ParameterIndex>& result);

template<Context C>
void collect_parameters(View<Index<ConjunctiveEffect>, C> element, UnorderedSet<ParameterIndex>& result);

template<Context C>
void collect_parameters(View<Index<ConditionalEffect>, C> element, UnorderedSet<ParameterIndex>& result);

template<Context C>
void collect_parameters(View<Index<Action>, C> element, UnorderedSet<ParameterIndex>& result);

// top-level

template<FactKind T, Context C>
auto collect_parameters(View<Index<Literal<T>>, C> element);

template<FactKind T, Context C>
auto collect_parameters(View<Index<FunctionTerm<T>>, C> element);

template<Context C>
auto collect_parameters(View<Data<BooleanOperator<Data<FunctionExpression>>>, C> element);

template<Context C>
auto collect_parameters(View<Index<Action>, C> element);

/**
 * Implementations
 */

inline void collect_parameters(float_t element, UnorderedSet<ParameterIndex>& result) {}

template<Context C>
void collect_parameters(View<Data<Term>, C> element, UnorderedSet<ParameterIndex>& result)
{
    visit(
        [&](auto&& arg)
        {
            using Alternative = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<Alternative, ParameterIndex>)
                result.insert(arg);
            else if constexpr (std::is_same_v<Alternative, View<Index<Object>, C>>) {}
            else
                static_assert(dependent_false<Alternative>::value, "Missing case");
        },
        element.get_variant());
}

template<FactKind T, Context C>
void collect_parameters(View<Index<Atom<T>>, C> element, UnorderedSet<ParameterIndex>& result)
{
    for (const auto term : element.get_terms())
        collect_parameters(term, result);
}

template<FactKind T, Context C>
void collect_parameters(View<Index<Literal<T>>, C> element, UnorderedSet<ParameterIndex>& result)
{
    collect_parameters(element.get_atom(), result);
}

template<FactKind T, Context C>
void collect_parameters(View<Data<FDRFact<T>>, C> element, UnorderedSet<ParameterIndex>& result)
{
    if (element.has_value())
        collect_parameters(element.get_atom(), result);
}

template<FactKind T, Context C>
void collect_parameters(View<Index<FunctionTerm<T>>, C> element, UnorderedSet<ParameterIndex>& result)
{
    for (const auto term : element.get_terms())
        collect_parameters(term, result);
}

template<Context C>
void collect_parameters(View<Data<FunctionExpression>, C> element, UnorderedSet<ParameterIndex>& result)
{
    visit([&](auto&& arg) { collect_parameters(arg, result); }, element.get_variant());
}

template<ArithmeticOpKind O, Context C>
void collect_parameters(View<Index<UnaryOperator<O, Data<FunctionExpression>>>, C> element, UnorderedSet<ParameterIndex>& result)
{
    collect_parameters(element.get_arg(), result);
}

template<OpKind O, Context C>
void collect_parameters(View<Index<BinaryOperator<O, Data<FunctionExpression>>>, C> element, UnorderedSet<ParameterIndex>& result)
{
    collect_parameters(element.get_lhs(), result);
    collect_parameters(element.get_rhs(), result);
}

template<ArithmeticOpKind O, Context C>
void collect_parameters(View<Index<MultiOperator<O, Data<FunctionExpression>>>, C> element, UnorderedSet<ParameterIndex>& result)
{
    for (const auto arg : element.get_args())
        collect_parameters(arg, result);
}

template<Context C>
void collect_parameters(View<Data<ArithmeticOperator<Data<FunctionExpression>>>, C> element, UnorderedSet<ParameterIndex>& result)
{
    visit([&](auto&& arg) { collect_parameters(arg, result); }, element.get_variant());
}

template<Context C>
void collect_parameters(View<Data<BooleanOperator<Data<FunctionExpression>>>, C> element, UnorderedSet<ParameterIndex>& result)
{
    visit([&](auto&& arg) { collect_parameters(arg, result); }, element.get_variant());
}

template<Context C>
void collect_parameters(View<Index<ConjunctiveCondition>, C> element, UnorderedSet<ParameterIndex>& result)
{
    for (const auto literal : element.template get_literals<StaticTag>())
        collect_parameters(literal, result);
    for (const auto literal : element.template get_literals<FluentTag>())
        collect_parameters(literal, result);
    for (const auto literal : element.template get_literals<DerivedTag>())
        collect_parameters(literal, result);
    for (const auto constraint : element.get_numeric_constraints())
        collect_parameters(constraint, result);
}

template<Context C>
void collect_parameters(View<Index<ConjunctiveEffect>, C> element, UnorderedSet<ParameterIndex>& result)
{
    for (const auto literal : element.get_literals())
        collect_parameters(literal, result);
    for (const auto numeric_effect : element.get_numeric_effects())
        collect_parameters(numeric_effect, result);
    if (element.get_auxiliary_numeric_effect().has_value())
        collect_parameters(element.get_auxiliary_numeric_effect().value(), result);
}

template<Context C>
void collect_parameters(View<Index<ConditionalEffect>, C> element, UnorderedSet<ParameterIndex>& result)
{
    collect_parameters(element.get_condition(), result);
    collect_parameters(element.get_effect(), result);
}

template<Context C>
void collect_parameters(View<Index<Action>, C> element, UnorderedSet<ParameterIndex>& result)
{
    collect_parameters(element.get_condition(), result);
    for (const auto cond_effect : element.get_effects())
        collect_parameters(cond_effect, result);
}

template<FactKind T, Context C>
auto collect_parameters(View<Index<Atom<T>>, C> element)
{
    auto result = UnorderedSet<ParameterIndex> {};
    collect_parameters(element, result);
    return result;
}

template<FactKind T, Context C>
auto collect_parameters(View<Index<Literal<T>>, C> element)
{
    auto result = UnorderedSet<ParameterIndex> {};
    collect_parameters(element, result);
    return result;
}

template<FactKind T, Context C>
auto collect_parameters(View<Index<FunctionTerm<T>>, C> element)
{
    auto result = UnorderedSet<ParameterIndex> {};
    collect_parameters(element, result);
    return result;
}

template<Context C>
auto collect_parameters(View<Data<BooleanOperator<Data<FunctionExpression>>>, C> element)
{
    auto result = UnorderedSet<ParameterIndex> {};
    visit([&](auto&& arg) { collect_parameters(arg, result); }, element.get_variant());
    return result;
}

template<Context C>
auto collect_parameters(View<Index<Action>, C> element)
{
    auto result = UnorderedSet<ParameterIndex> {};
    collect_parameters(element, result);
    return result;
}

}

#endif