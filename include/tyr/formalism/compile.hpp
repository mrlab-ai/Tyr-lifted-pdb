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

#ifndef TYR_FORMALISM_COMPILE_HPP_
#define TYR_FORMALISM_COMPILE_HPP_

#include "tyr/common/tuple.hpp"
#include "tyr/formalism/builder.hpp"
#include "tyr/formalism/canonicalization.hpp"
#include "tyr/formalism/declarations.hpp"
#include "tyr/formalism/merge.hpp"

namespace tyr::formalism
{

/**
 * Forward declarations
 */

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<Predicate<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination);

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<Atom<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination);

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<GroundAtom<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination);

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<Literal<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination);

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<GroundLiteral<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination);

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<Function<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination);

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<FunctionTerm<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination);

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<GroundFunctionTerm<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination);

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<GroundFunctionTermValue<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination);

/**
 * Implementations
 */

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<Predicate<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination)
{
    auto predicate_ptr = builder.template get_builder<formalism::Predicate<T_DST>>();
    auto& predicate = *predicate_ptr;
    predicate.clear();

    predicate.name = element.get_name();
    predicate.arity = element.get_arity();

    canonicalize(predicate);
    return destination.get_or_create(predicate, builder.get_buffer()).first;
}

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<Atom<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination)
{
    auto atom_ptr = builder.template get_builder<formalism::Atom<T_DST>>();
    auto& atom = *atom_ptr;
    atom.clear();

    atom.predicate = compile<T_SRC, T_DST>(element.get_predicate(), builder, destination).get_index();
    for (const auto term : element.get_terms())
        atom.terms.push_back(merge(term, builder, destination).get_data());

    canonicalize(atom);
    return destination.get_or_create(atom, builder.get_buffer()).first;
}

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<GroundAtom<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination)
{
    auto atom_ptr = builder.template get_builder<formalism::GroundAtom<T_DST>>();
    auto& atom = *atom_ptr;
    atom.clear();

    atom.predicate = compile<T_SRC, T_DST>(element.get_predicate(), builder, destination).get_index();
    atom.binding = merge(element.get_binding(), builder, destination).get_index();

    canonicalize(atom);
    return destination.get_or_create(atom, builder.get_buffer()).first;
}

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<Literal<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination)
{
    auto literal_ptr = builder.template get_builder<formalism::Literal<T_DST>>();
    auto& literal = *literal_ptr;
    literal.clear();

    literal.polarity = element.get_polarity();
    literal.atom = compile<T_SRC, T_DST>(element.get_atom(), builder, destination).get_index();

    canonicalize(literal);
    return destination.get_or_create(literal, builder.get_buffer()).first;
}

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<GroundLiteral<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination)
{
    auto literal_ptr = builder.template get_builder<formalism::GroundLiteral<T_DST>>();
    auto& literal = *literal_ptr;
    literal.clear();

    literal.polarity = element.get_polarity();
    literal.atom = compile<T_SRC, T_DST>(element.get_atom(), builder, destination).get_index();

    canonicalize(literal);
    return destination.get_or_create(literal, builder.get_buffer()).first;
}

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<Function<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination)
{
    auto function_ptr = builder.template get_builder<formalism::Function<T_DST>>();
    auto& function = *function_ptr;
    function.clear();

    function.name = element.get_name();
    function.arity = element.get_arity();

    canonicalize(function);
    return destination.get_or_create(function, builder.get_buffer()).first;
}

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<FunctionTerm<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination)
{
    auto fterm_ptr = builder.template get_builder<formalism::FunctionTerm<T_DST>>();
    auto& fterm = *fterm_ptr;
    fterm.clear();

    fterm.function = compile<T_SRC, T_DST>(element.get_function(), builder, destination).get_index();
    for (const auto object : element.get_objects())
        fterm.objects.push_back(merge(object, builder, destination).get_index());

    canonicalize(fterm);
    return destination.get_or_create(fterm, builder.get_buffer()).first;
}

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<GroundFunctionTerm<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination)
{
    auto fterm_ptr = builder.template get_builder<formalism::GroundFunctionTerm<T_DST>>();
    auto& fterm = *fterm_ptr;
    fterm.clear();

    fterm.function = compile<T_SRC, T_DST>(element.get_function(), builder, destination).get_index();
    fterm.binding = merge(element.get_binding(), builder, destination).get_index();

    canonicalize(fterm);
    return destination.get_or_create(fterm, builder.get_buffer()).first;
}

template<FactKind T_SRC, FactKind T_DST, Context C_SRC, Context C_DST>
auto compile(View<Index<GroundFunctionTermValue<T_SRC>>, C_SRC> element, Builder& builder, C_DST& destination)
{
    auto fterm_value_ptr = builder.template get_builder<formalism::GroundFunctionTermValue<T_DST>>();
    auto& fterm_value = *fterm_value_ptr;
    fterm_value.clear();

    fterm_value.fterm = compile<T_SRC, T_DST>(element.get_fterm(), builder, destination).get_index();
    fterm_value.value = element.get_value();

    canonicalize(fterm_value);
    return destination.get_or_create(fterm_value, builder.get_buffer()).first;
}

}

#endif