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
 * Forward declarations
 */

class Repository;

struct Double;

template<IsOp Op, typename T>
struct UnaryOperatorIndex;
template<IsOp Op, typename T>
class UnaryOperatorProxy;
template<IsOp Op, typename T>
struct UnaryOperator;

template<IsOp Op, typename T>
struct BinaryOperatorIndex;
template<IsOp Op, typename T>
class BinaryOperatorProxy;
template<IsOp Op, typename T>
struct BinaryOperator;

template<IsOp Op, typename T>
struct MultiOperatorIndex;
template<IsOp Op, typename T>
class MultiOperatorProxy;
template<IsOp Op, typename T>
struct MultiOperator;

struct VariableIndex;
struct Variable;
class VariableProxy;

struct ObjectIndex;
struct Object;
class ObjectProxy;

struct Term;
class TermProxy;

template<IsStaticOrFluentTag T>
struct PredicateIndex;
template<IsStaticOrFluentTag T>
struct Predicate;
template<IsStaticOrFluentTag T>
class PredicateProxy;

template<IsStaticOrFluentTag T>
struct AtomIndex;
template<IsStaticOrFluentTag T>
struct Atom;
template<IsStaticOrFluentTag T>
class AtomProxy;

template<IsStaticOrFluentTag T>
struct LiteralIndex;
template<IsStaticOrFluentTag T>
struct Literal;
template<IsStaticOrFluentTag T>
class LiteralProxy;

template<IsStaticOrFluentTag T>
struct GroundAtomIndex;
template<IsStaticOrFluentTag T>
struct GroundAtom;
template<IsStaticOrFluentTag T>
class GroundAtomProxy;

template<IsStaticOrFluentTag T>
struct GroundLiteralIndex;
template<IsStaticOrFluentTag T>
struct GroundLiteral;
template<IsStaticOrFluentTag T>
class GroundLiteralProxy;

template<IsStaticOrFluentTag T>
struct FunctionIndex;
template<IsStaticOrFluentTag T>
struct Function;
template<IsStaticOrFluentTag T>
class FunctionProxy;

template<IsStaticOrFluentTag T>
struct FunctionTermIndex;
template<IsStaticOrFluentTag T>
struct FunctionTerm;
template<IsStaticOrFluentTag T>
class FunctionTermProxy;

template<IsStaticOrFluentTag T>
struct GroundFunctionTermIndex;
template<IsStaticOrFluentTag T>
struct GroundFunctionTerm;
template<IsStaticOrFluentTag T>
class GroundFunctionTermProxy;

struct FunctionExpression;
class FunctionExpressionProxy;

struct GroundFunctionExpression;
class GroundFunctionExpressionProxy;

struct RuleIndex;
struct Rule;
class RuleProxy;

struct GroundRuleIndex;
struct GroundRule;
class GroundRuleProxy;

struct ProgramIndex;
struct Program;
class ProgramProxy;
}

#endif
