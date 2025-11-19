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

#ifndef TYR_FORMALISM_DECLARATIONS_HPP_
#define TYR_FORMALISM_DECLARATIONS_HPP_

#include "tyr/common/config.hpp"
#include "tyr/common/declarations.hpp"

namespace tyr::formalism
{

/**
 * Tags to distinguish predicates and downstream types
 */

struct StaticTag
{
};
struct FluentTag
{
};

template<typename T>
concept IsStaticOrFluentTag = std::same_as<T, StaticTag> || std::same_as<T, FluentTag>;

/**
 * Tags to dispatch operators
 */

struct OpEq
{
};
struct OpLe
{
};
struct OpLt
{
};
struct OpGe
{
};
struct OpGt
{
};
struct OpAdd
{
};
struct OpMul
{
};
struct OpDiv
{
};
struct OpSub
{
};

template<typename T>
concept IsOp = std::same_as<T, OpEq> || std::same_as<T, OpLe> || std::same_as<T, OpLt> || std::same_as<T, OpGe> || std::same_as<T, OpGt>
               || std::same_as<T, OpAdd> || std::same_as<T, OpMul> || std::same_as<T, OpDiv> || std::same_as<T, OpSub>;

/**
 * Context
 */

class Repository;

/// @brief Make Repository a trivial context.
/// @param context
/// @return
inline const Repository& get_repository(const Repository& context) noexcept { return context; }

template<typename T>
concept IsContext = requires(const T& a) {
    { get_repository(a) } -> std::same_as<const Repository&>;
};

static_assert(IsContext<Repository>);

/**
 * Forward declarations
 */

class Repository;

struct Double;

template<IsOp Op, typename T>
struct UnaryOperatorIndex;
template<IsOp Op, typename T>
struct UnaryOperator;
template<IsContext C, IsOp Op, typename T>
class UnaryOperatorProxy;

template<IsOp Op, typename T>
struct BinaryOperatorIndex;
template<IsOp Op, typename T>
struct BinaryOperator;
template<IsContext C, IsOp Op, typename T>
class BinaryOperatorProxy;

template<IsOp Op, typename T>
struct MultiOperatorIndex;
template<IsOp Op, typename T>
struct MultiOperator;
template<IsContext C, IsOp Op, typename T>
class MultiOperatorProxy;

struct VariableIndex;
struct Variable;
template<IsContext C>
class VariableProxy;

struct ObjectIndex;
struct Object;
template<IsContext C>
class ObjectProxy;

struct Term;
template<IsContext C>
class TermProxy;

template<IsStaticOrFluentTag T>
struct PredicateIndex;
template<IsStaticOrFluentTag T>
struct Predicate;
template<IsContext C, IsStaticOrFluentTag T>
class PredicateProxy;

template<IsStaticOrFluentTag T>
struct AtomIndex;
template<IsStaticOrFluentTag T>
struct Atom;
template<IsContext C, IsStaticOrFluentTag T>
class AtomProxy;

template<IsStaticOrFluentTag T>
struct LiteralIndex;
template<IsStaticOrFluentTag T>
struct Literal;
template<IsContext C, IsStaticOrFluentTag T>
class LiteralProxy;

template<IsStaticOrFluentTag T>
struct GroundAtomIndex;
template<IsStaticOrFluentTag T>
struct GroundAtom;
template<IsContext C, IsStaticOrFluentTag T>
class GroundAtomProxy;

template<IsStaticOrFluentTag T>
struct GroundLiteralIndex;
template<IsStaticOrFluentTag T>
struct GroundLiteral;
template<IsContext C, IsStaticOrFluentTag T>
class GroundLiteralProxy;

template<IsStaticOrFluentTag T>
struct FunctionIndex;
template<IsStaticOrFluentTag T>
struct Function;
template<IsContext C, IsStaticOrFluentTag T>
class FunctionProxy;

template<IsStaticOrFluentTag T>
struct FunctionTermIndex;
template<IsStaticOrFluentTag T>
struct FunctionTerm;
template<IsContext C, IsStaticOrFluentTag T>
class FunctionTermProxy;

template<IsStaticOrFluentTag T>
struct GroundFunctionTermIndex;
template<IsStaticOrFluentTag T>
struct GroundFunctionTerm;
template<IsContext C, IsStaticOrFluentTag T>
class GroundFunctionTermProxy;

template<IsStaticOrFluentTag T>
struct GroundFunctionTermValueIndex;
template<IsStaticOrFluentTag T>
struct GroundFunctionTermValue;
template<IsContext C, IsStaticOrFluentTag T>
class GroundFunctionTermValueProxy;

struct FunctionExpression;
template<IsContext C>
class FunctionExpressionProxy;

struct GroundFunctionExpression;
template<IsContext C>
class GroundFunctionExpressionProxy;

struct RuleIndex;
struct Rule;
template<IsContext C>
class RuleProxy;

struct GroundRuleIndex;
struct GroundRule;
template<IsContext C>
class GroundRuleProxy;

struct ProgramIndex;
struct Program;
template<IsContext C>
class ProgramProxy;

}

#endif
