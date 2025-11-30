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
#include "tyr/common/types.hpp"

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
struct DerivedTag
{
};
struct AuxiliaryTag
{
};

template<typename T>
concept FactKind = std::same_as<T, StaticTag> || std::same_as<T, FluentTag> || std::same_as<T, DerivedTag> || std::same_as<T, AuxiliaryTag>;

/**
 * Tags to dispatch operators
 */

struct OpEq
{
};
struct OpNe
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
struct OpSub
{
};
struct OpMul
{
};
struct OpDiv
{
};

template<typename T>
concept BooleanOpKind =
    std::same_as<T, OpEq> || std::same_as<T, OpNe> || std::same_as<T, OpLe> || std::same_as<T, OpLt> || std::same_as<T, OpGe> || std::same_as<T, OpGt>;

template<typename T>
concept ArithmeticOpKind = std::same_as<T, OpAdd> || std::same_as<T, OpMul> || std::same_as<T, OpDiv> || std::same_as<T, OpSub>;

template<typename T>
concept OpKind = BooleanOpKind<T> || ArithmeticOpKind<T>;

/**
 * Formalism tag
 */

template<OpKind Op, typename T>
struct UnaryOperator
{
};

template<OpKind Op, typename T>
struct BinaryOperator
{
};

template<OpKind Op, typename T>
struct MultiOperator
{
};

template<typename T>
class BooleanOperator
{
};
template<typename T>
class ArithmeticOperator
{
};

struct Variable
{
};

struct Object
{
};

struct Term
{
};

template<FactKind T>
struct Predicate
{
};

template<FactKind T>
struct Atom
{
};

template<FactKind T>
struct Literal
{
};

template<FactKind T>
struct GroundAtom
{
};

template<FactKind T>
struct GroundLiteral
{
};

template<FactKind T>
struct Function
{
};

template<FactKind T>
struct FunctionTerm
{
};

struct FunctionExpression
{
};

template<FactKind T>
struct GroundFunctionTerm
{
};

struct GroundFunctionExpression
{
};

template<FactKind T>
struct GroundFunctionTermValue
{
};

struct ConjunctiveCondition
{
};

struct GroundConjunctiveCondition
{
};

struct Rule
{
};

struct GroundRule
{
};

struct Program
{
};

struct OpAssign
{
};
struct OpIncrease
{
};
struct OpDecrease
{
};
struct OpScaleUp
{
};
struct OpScaleDown
{
};

template<typename T>
concept NumericEffectOpKind =
    std::same_as<T, OpAssign> || std::same_as<T, OpIncrease> || std::same_as<T, OpDecrease> || std::same_as<T, OpScaleUp> || std::same_as<T, OpScaleDown>;

template<FactKind T>
struct NumericEffect
{
};
template<FactKind T>
struct GroundNumericEffect
{
};

struct ConditionalEffect
{
};
struct GroundConditionalEffect
{
};

struct ConjunctiveEffect
{
};
struct GroundConjunctiveEffect
{
};

struct Action
{
};
struct GroundAction
{
};

struct Axiom
{
};
struct GroundAxiom
{
};

struct Minimize
{
};
struct Maximize
{
};

template<typename T>
concept ObjectiveKind = std::same_as<T, Minimize> || std::same_as<T, Maximize>;

struct Metric
{
};

struct Task
{
};

struct Domain
{
};

/**
 * Context
 */

template<typename Repo, typename Tag>
concept RepositoryAccess = requires(const Repo& r, Index<Tag> idx) {
    { r[idx] } -> std::same_as<const Data<Tag>&>;
};

template<typename T>
concept IsRepository =
    RepositoryAccess<T, Variable> && RepositoryAccess<T, Object> && RepositoryAccess<T, Predicate<StaticTag>> && RepositoryAccess<T, Predicate<FluentTag>>
    && RepositoryAccess<T, Atom<StaticTag>> && RepositoryAccess<T, Atom<FluentTag>> && RepositoryAccess<T, GroundAtom<StaticTag>>
    && RepositoryAccess<T, GroundAtom<FluentTag>> && RepositoryAccess<T, Literal<StaticTag>> && RepositoryAccess<T, Literal<FluentTag>>
    && RepositoryAccess<T, GroundLiteral<StaticTag>> && RepositoryAccess<T, GroundLiteral<FluentTag>> && RepositoryAccess<T, Function<StaticTag>>
    && RepositoryAccess<T, Function<FluentTag>> && RepositoryAccess<T, FunctionTerm<StaticTag>> && RepositoryAccess<T, FunctionTerm<FluentTag>>
    && RepositoryAccess<T, GroundFunctionTerm<StaticTag>> && RepositoryAccess<T, GroundFunctionTerm<FluentTag>>
    && RepositoryAccess<T, GroundFunctionTermValue<StaticTag>> && RepositoryAccess<T, GroundFunctionTermValue<FluentTag>>
    && RepositoryAccess<T, UnaryOperator<OpSub, Data<FunctionExpression>>> && RepositoryAccess<T, BinaryOperator<OpAdd, Data<FunctionExpression>>>
    && RepositoryAccess<T, BinaryOperator<OpSub, Data<FunctionExpression>>> && RepositoryAccess<T, BinaryOperator<OpMul, Data<FunctionExpression>>>
    && RepositoryAccess<T, BinaryOperator<OpDiv, Data<FunctionExpression>>> && RepositoryAccess<T, MultiOperator<OpAdd, Data<FunctionExpression>>>
    && RepositoryAccess<T, MultiOperator<OpMul, Data<FunctionExpression>>> && RepositoryAccess<T, BinaryOperator<OpEq, Data<FunctionExpression>>>
    && RepositoryAccess<T, BinaryOperator<OpLe, Data<FunctionExpression>>> && RepositoryAccess<T, BinaryOperator<OpLt, Data<FunctionExpression>>>
    && RepositoryAccess<T, BinaryOperator<OpGe, Data<FunctionExpression>>> && RepositoryAccess<T, BinaryOperator<OpGt, Data<FunctionExpression>>>
    && RepositoryAccess<T, UnaryOperator<OpSub, Data<GroundFunctionExpression>>> && RepositoryAccess<T, BinaryOperator<OpAdd, Data<GroundFunctionExpression>>>
    && RepositoryAccess<T, BinaryOperator<OpSub, Data<GroundFunctionExpression>>> && RepositoryAccess<T, BinaryOperator<OpMul, Data<GroundFunctionExpression>>>
    && RepositoryAccess<T, BinaryOperator<OpDiv, Data<GroundFunctionExpression>>> && RepositoryAccess<T, MultiOperator<OpAdd, Data<GroundFunctionExpression>>>
    && RepositoryAccess<T, MultiOperator<OpMul, Data<GroundFunctionExpression>>> && RepositoryAccess<T, BinaryOperator<OpEq, Data<GroundFunctionExpression>>>
    && RepositoryAccess<T, BinaryOperator<OpLe, Data<GroundFunctionExpression>>> && RepositoryAccess<T, BinaryOperator<OpLt, Data<GroundFunctionExpression>>>
    && RepositoryAccess<T, BinaryOperator<OpGe, Data<GroundFunctionExpression>>> && RepositoryAccess<T, BinaryOperator<OpGt, Data<GroundFunctionExpression>>>
    && RepositoryAccess<T, Rule> && RepositoryAccess<T, GroundRule> && RepositoryAccess<T, Program>;

template<typename T>
concept Context = requires(const T& a) {
    { get_repository(a) } -> IsRepository;
};

}

#endif
