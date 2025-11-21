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

#ifndef TYR_FORMALISM_PROXY_TRAITS_HPP_
#define TYR_FORMALISM_PROXY_TRAITS_HPP_

#include "tyr/formalism/declarations.hpp"

namespace tyr::formalism
{

template<typename T>
struct ProxyTraits
{
};

template<IsOp Op, typename T, IsContext C>
struct ProxyTraits<UnaryOperatorProxy<Op, T, C>>
{
};

template<IsOp Op, typename T, IsContext C>
struct ProxyTraits<BinaryOperatorProxy<Op, T, C>>
{
};

template<IsOp Op, typename T, IsContext C>
struct ProxyTraits<MultiOperatorProxy<Op, T, C>>
{
};

template<typename T, IsContext C>
struct ProxyTraits<ArithmeticOperatorProxy<T, C>>
{
};

template<typename T, IsContext C>
struct ProxyTraits<BooleanOperatorProxy<T, C>>
{
};

template<IsContext C>
struct ProxyTraits<VariableProxy<C>>
{
};

template<IsContext C>
struct ProxyTraits<ObjectProxy<C>>
{
};

struct Term;
template<IsContext C>
struct ProxyTraits<TermProxy<C>>
{
};

template<IsStaticOrFluentTag T, IsContext C>
struct ProxyTraits<PredicateProxy<T, C>>
{
};

template<IsStaticOrFluentTag T, IsContext C>
struct ProxyTraits<AtomProxy<T, C>>
{
};

template<IsStaticOrFluentTag T, IsContext C>
struct ProxyTraits<LiteralProxy<T, C>>
{
};

template<IsStaticOrFluentTag T, IsContext C>
struct ProxyTraits<GroundAtomProxy<T, C>>
{
};

template<IsStaticOrFluentTag T, IsContext C>
struct ProxyTraits<GroundLiteralProxy<T, C>>
{
};

template<IsStaticOrFluentTag T, IsContext C>
struct ProxyTraits<FunctionProxy<T, C>>
{
};

template<IsStaticOrFluentTag T, IsContext C>
struct ProxyTraits<FunctionTermProxy<T, C>>
{
};

template<IsStaticOrFluentTag T, IsContext C>
struct ProxyTraits<GroundFunctionTermProxy<T, C>>
{
};

template<IsStaticOrFluentTag T, IsContext C>
struct ProxyTraits<GroundFunctionTermValueProxy<T, C>>
{
};

template<IsContext C>
struct ProxyTraits<FunctionExpressionProxy<C>>
{
};

template<IsContext C>
struct ProxyTraits<GroundFunctionExpressionProxy<C>>
{
};

template<IsContext C>
struct ProxyTraits<RuleProxy<C>>
{
};

template<IsContext C>
struct ProxyTraits<GroundRuleProxy<C>>
{
};

template<IsContext C>
struct ProxyTraits<ProgramProxy<C>>
{
};

}

#endif
