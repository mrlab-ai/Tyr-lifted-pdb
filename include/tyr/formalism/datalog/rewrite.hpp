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

#ifndef TYR_FORMALISM_DATALOG_REWRITE_HPP_
#define TYR_FORMALISM_DATALOG_REWRITE_HPP_

#include "tyr/formalism/datalog/merge.hpp"
#include "tyr/formalism/datalog/views.hpp"

namespace tyr::formalism::datalog
{
/**
 * should_keep
 */

inline bool should_keep(float_t element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping);

template<Context C>
bool should_keep(View<Data<Term>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping);

template<FactKind T, Context C>
bool should_keep(View<Index<Atom<T>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping);

template<FactKind T, Context C>
bool should_keep(View<Index<Literal<T>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping);

template<FactKind T, Context C>
bool should_keep(View<Index<FunctionTerm<T>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping);

template<Context C>
bool should_keep(View<Data<FunctionExpression>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping);

template<ArithmeticOpKind O, Context C>
bool should_keep(View<Index<UnaryOperator<O, Data<FunctionExpression>>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping);

template<OpKind O, Context C>
bool should_keep(View<Index<BinaryOperator<O, Data<FunctionExpression>>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping);

template<ArithmeticOpKind O, Context C>
bool should_keep(View<Index<MultiOperator<O, Data<FunctionExpression>>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping);

template<Context C>
bool should_keep(View<Data<ArithmeticOperator<Data<FunctionExpression>>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping);

template<Context C>
bool should_keep(View<Data<BooleanOperator<Data<FunctionExpression>>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping);

inline bool should_keep(float_t element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping) { return true; }

template<Context C>
bool should_keep(View<Data<Term>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping)
{
    return visit(
        [&](auto&& arg)
        {
            using Alternative = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<Alternative, ParameterIndex>)
                return mapping.contains(arg);
            else if constexpr (std::is_same_v<Alternative, View<Index<Object>, C>>)
                return true;
            else
                static_assert(dependent_false<Alternative>::value, "Missing case");
        },
        element.get_variant());
}

template<FactKind T, Context C>
bool should_keep(View<Index<Atom<T>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping)
{
    return std::all_of(element.get_terms().begin(), element.get_terms().end(), [&](auto&& term) { return should_keep(term, mapping); });
}

template<FactKind T, Context C>
bool should_keep(View<Index<Literal<T>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping)
{
    return should_keep(element.get_atom(), mapping);
}

template<FactKind T, Context C>
bool should_keep(View<Index<FunctionTerm<T>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping)
{
    return std::all_of(element.get_terms().begin(), element.get_terms().end(), [&](auto&& term) { return should_keep(term, mapping); });
}

template<Context C>
bool should_keep(View<Data<FunctionExpression>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping)
{
    return visit([&](auto&& arg) { return should_keep(arg, mapping); }, element.get_variant());
}

template<ArithmeticOpKind O, Context C>
bool should_keep(View<Index<UnaryOperator<O, Data<FunctionExpression>>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping)
{
    return should_keep(element.get_arg(), mapping);
}

template<OpKind O, Context C>
bool should_keep(View<Index<BinaryOperator<O, Data<FunctionExpression>>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping)
{
    return should_keep(element.get_lhs(), mapping) && should_keep(element.get_rhs(), mapping);
}

template<ArithmeticOpKind O, Context C>
bool should_keep(View<Index<MultiOperator<O, Data<FunctionExpression>>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping)
{
    return std::all_of(element.get_args().begin(), element.get_args().end(), [&](auto&& arg) { return should_keep(arg, mapping); });
}

template<Context C>
bool should_keep(View<Data<ArithmeticOperator<Data<FunctionExpression>>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping)
{
    return visit([&](auto&& arg) { return should_keep(arg, mapping); }, element.get_variant());
}

template<Context C>
bool should_keep(View<Data<BooleanOperator<Data<FunctionExpression>>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping)
{
    return visit([&](auto&& arg) { return should_keep(arg, mapping); }, element.get_variant());
}

/**
 * merge
 */

template<Context C>
auto merge(View<Data<Term>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping, MergeContext<C>& context);

template<FactKind T, Context C>
auto merge(View<Index<Atom<T>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping, MergeContext<C>& context);

template<FactKind T, Context C>
auto merge(View<Index<Literal<T>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping, MergeContext<C>& context);

template<FactKind T, Context C>
auto merge(View<Index<FunctionTerm<T>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping, MergeContext<C>& context);

template<Context C>
auto merge(View<Data<FunctionExpression>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping, MergeContext<C>& context);

template<ArithmeticOpKind O, Context C>
auto merge(View<Index<UnaryOperator<O, Data<FunctionExpression>>>, C> element,
           const UnorderedMap<ParameterIndex, ParameterIndex>& mapping,
           MergeContext<C>& context);

template<OpKind O, Context C>
auto merge(View<Index<BinaryOperator<O, Data<FunctionExpression>>>, C> element,
           const UnorderedMap<ParameterIndex, ParameterIndex>& mapping,
           MergeContext<C>& context);

template<ArithmeticOpKind O, Context C>
auto merge(View<Index<MultiOperator<O, Data<FunctionExpression>>>, C> element,
           const UnorderedMap<ParameterIndex, ParameterIndex>& mapping,
           MergeContext<C>& context);

template<Context C>
auto merge(View<Data<ArithmeticOperator<Data<FunctionExpression>>>, C> element,
           const UnorderedMap<ParameterIndex, ParameterIndex>& mapping,
           MergeContext<C>& context);

template<Context C>
auto merge(View<Data<BooleanOperator<Data<FunctionExpression>>>, C> element,
           const UnorderedMap<ParameterIndex, ParameterIndex>& mapping,
           MergeContext<C>& context);

template<Context C>
auto merge(View<Data<Term>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping, MergeContext<C>& context)
{
    return visit(
        [&](auto&& arg)
        {
            using Alternative = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<Alternative, ParameterIndex>)
                return Data<Term>(mapping.at(arg));
            else if constexpr (std::is_same_v<Alternative, View<Index<Object>, C>>)
                return Data<Term>(arg.get_index());
            else
                static_assert(dependent_false<Alternative>::value, "Missing case");
        },
        element.get_variant());
}

template<FactKind T, Context C>
auto merge(View<Index<Atom<T>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping, MergeContext<C>& context)
{
    auto atom_ptr = context.builder.template get_builder<Atom<T>>();
    auto& atom = *atom_ptr;
    atom.clear();

    atom.predicate = element.get_predicate().get_index();
    for (const auto term : element.get_terms())
        atom.terms.push_back(merge(term, mapping, context));

    canonicalize(atom);
    return context.destination.get_or_create(atom, context.builder.get_buffer());
}

template<FactKind T, Context C>
auto merge(View<Index<Literal<T>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping, MergeContext<C>& context)
{
    auto literal_ptr = context.builder.template get_builder<Literal<T>>();
    auto& literal = *literal_ptr;
    literal.clear();

    literal.polarity = element.get_polarity();
    literal.atom = merge(element.get_atom(), mapping, context).first;

    canonicalize(literal);
    return context.destination.get_or_create(literal, context.builder.get_buffer());
}

template<FactKind T, Context C>
auto merge(View<Index<FunctionTerm<T>>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping, MergeContext<C>& context)
{
    auto fterm_ptr = context.builder.template get_builder<FunctionTerm<T>>();
    auto& fterm = *fterm_ptr;
    fterm.clear();

    fterm.function = element.get_function().get_index();
    for (const auto term : element.get_terms())
        fterm.terms.push_back(merge(term, mapping, context));

    canonicalize(fterm);
    return context.destination.get_or_create(fterm, context.builder.get_buffer());
}

template<Context C>
auto merge(View<Data<FunctionExpression>, C> element, const UnorderedMap<ParameterIndex, ParameterIndex>& mapping, MergeContext<C>& context)
{
    return visit(
        [&](auto&& arg)
        {
            using Alternative = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<Alternative, float_t>)
                return Data<FunctionExpression>(arg);
            else if constexpr (std::is_same_v<Alternative, View<Data<ArithmeticOperator<Data<FunctionExpression>>>, C>>)
                return Data<FunctionExpression>(merge(arg, mapping, context));
            else
                return Data<FunctionExpression>(merge(arg, mapping, context).first);
        },
        element.get_variant());
}

template<ArithmeticOpKind O, Context C>
auto merge(View<Index<UnaryOperator<O, Data<FunctionExpression>>>, C> element,
           const UnorderedMap<ParameterIndex, ParameterIndex>& mapping,
           MergeContext<C>& context)
{
    auto unary_ptr = context.builder.template get_builder<UnaryOperator<O, Data<FunctionExpression>>>();
    auto& unary = *unary_ptr;
    unary.clear();

    unary.arg = merge(element.get_arg(), mapping, context);

    canonicalize(unary);
    return context.destination.get_or_create(unary, context.builder.get_buffer());
}

template<OpKind O, Context C>
auto merge(View<Index<BinaryOperator<O, Data<FunctionExpression>>>, C> element,
           const UnorderedMap<ParameterIndex, ParameterIndex>& mapping,
           MergeContext<C>& context)
{
    auto binary_ptr = context.builder.template get_builder<BinaryOperator<O, Data<FunctionExpression>>>();
    auto& binary = *binary_ptr;
    binary.clear();

    binary.lhs = merge(element.get_lhs(), mapping, context);
    binary.rhs = merge(element.get_rhs(), mapping, context);

    canonicalize(binary);
    return context.destination.get_or_create(binary, context.builder.get_buffer());
}

template<ArithmeticOpKind O, Context C>
auto merge(View<Index<MultiOperator<O, Data<FunctionExpression>>>, C> element,
           const UnorderedMap<ParameterIndex, ParameterIndex>& mapping,
           MergeContext<C>& context)
{
    auto multi_ptr = context.builder.template get_builder<MultiOperator<O, Data<FunctionExpression>>>();
    auto& multi = *multi_ptr;
    multi.clear();

    for (const auto arg : element.get_args())
        multi.args.push_back(merge(arg, mapping, context));

    canonicalize(multi);
    return context.destination.get_or_create(multi, context.builder.get_buffer());
}

template<Context C>
auto merge(View<Data<ArithmeticOperator<Data<FunctionExpression>>>, C> element,
           const UnorderedMap<ParameterIndex, ParameterIndex>& mapping,
           MergeContext<C>& context)
{
    return visit([&](auto&& arg) { return Data<ArithmeticOperator<Data<FunctionExpression>>>(merge(arg, mapping, context).first); }, element.get_variant());
}

template<Context C>
auto merge(View<Data<BooleanOperator<Data<FunctionExpression>>>, C> element,
           const UnorderedMap<ParameterIndex, ParameterIndex>& mapping,
           MergeContext<C>& context)
{
    return visit([&](auto&& arg) { return Data<BooleanOperator<Data<FunctionExpression>>>(merge(arg, mapping, context).first); }, element.get_variant());
}
}

#endif