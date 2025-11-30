/*
 * Copyright (C) 2023 Dominik Drexler and Simon Stahlberg
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
 *<
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef TYR_SRC_PLANNING_LOKI_TO_TYR_HPP_
#define TYR_SRC_PLANNING_LOKI_TO_TYR_HPP_

#include "tyr/formalism/builder.hpp"
#include "tyr/formalism/declarations.hpp"
#include "tyr/formalism/overlay_repository.hpp"
#include "tyr/formalism/repository.hpp"
#include "tyr/planning/declarations.hpp"

#include <loki/loki.hpp>
#include <tuple>
#include <unordered_map>
#include <variant>

namespace tyr::planning
{

using IndexPredicateVariant = std::variant<Index<formalism::Predicate<formalism::StaticTag>>,
                                           Index<formalism::Predicate<formalism::FluentTag>>,
                                           Index<formalism::Predicate<formalism::DerivedTag>>>;

using IndexAtomVariant =
    std::variant<Index<formalism::Atom<formalism::StaticTag>>, Index<formalism::Atom<formalism::FluentTag>>, Index<formalism::Atom<formalism::DerivedTag>>>;

using IndexLiteralVariant = std::
    variant<Index<formalism::Literal<formalism::StaticTag>>, Index<formalism::Literal<formalism::FluentTag>>, Index<formalism::Literal<formalism::DerivedTag>>>;

using IndexGroundAtomVariant = std::variant<Index<formalism::GroundAtom<formalism::StaticTag>>,
                                            Index<formalism::GroundAtom<formalism::FluentTag>>,
                                            Index<formalism::GroundAtom<formalism::DerivedTag>>>;

using IndexGroundLiteralVariant = std::variant<Index<formalism::GroundLiteral<formalism::StaticTag>>,
                                               Index<formalism::GroundLiteral<formalism::FluentTag>>,
                                               Index<formalism::GroundLiteral<formalism::DerivedTag>>>;

using IndexFunctionVariant = std::variant<Index<formalism::Function<formalism::StaticTag>>,
                                          Index<formalism::Function<formalism::FluentTag>>,
                                          Index<formalism::Function<formalism::AuxiliaryTag>>>;

using IndexFunctionTermVariant = std::variant<Index<formalism::FunctionTerm<formalism::StaticTag>>,
                                              Index<formalism::FunctionTerm<formalism::FluentTag>>,
                                              Index<formalism::FunctionTerm<formalism::AuxiliaryTag>>>;

using IndexGroundFunctionTermVariant = std::variant<Index<formalism::GroundFunctionTerm<formalism::StaticTag>>,
                                                    Index<formalism::GroundFunctionTerm<formalism::FluentTag>>,
                                                    Index<formalism::GroundFunctionTerm<formalism::AuxiliaryTag>>>;

using IndexGroundFunctionTermValueVariant = std::variant<Index<formalism::GroundFunctionTermValue<formalism::StaticTag>>,
                                                         Index<formalism::GroundFunctionTermValue<formalism::FluentTag>>,
                                                         Index<formalism::GroundFunctionTermValue<formalism::AuxiliaryTag>>>;

class LokiToTyrTranslator
{
private:
    /* Computed in prepare step */

    // For type analysis of predicates.
    std::unordered_set<std::string> m_fluent_predicates;   ///< Fluent predicates that appear in an effect
    std::unordered_set<std::string> m_derived_predicates;  ///< Derived predicates

    // For type analysis of functions.
    std::unordered_set<std::string> m_fexpr_functions;            ///< Functions that appear in a lifted function expression, i.e., numeric effect or constraint
    std::unordered_set<std::string> m_effect_function_skeletons;  ///< Functions that appear in an effect

    template<std::ranges::input_range Range>
    void prepare(const Range& range)
    {
        std::for_each(std::begin(range), std::end(range), [&](auto&& arg) { this->prepare(arg); });
    }
    template<typename T>
    void prepare(const std::optional<T>& element)
    {
        if (element.has_value())
        {
            this->prepare(element.value());
        }
    }
    void prepare(loki::FunctionSkeleton function_skeleton);
    void prepare(loki::Object object);
    void prepare(loki::Parameter parameter);
    void prepare(loki::Predicate predicate);
    void prepare(loki::Requirements requirements);
    void prepare(loki::Type type);
    void prepare(loki::Variable variable);
    void prepare(loki::Term term);
    void prepare(loki::Atom atom);
    void prepare(loki::Literal literal);
    void prepare(loki::FunctionExpressionNumber function_expression);
    void prepare(loki::FunctionExpressionBinaryOperator function_expression);
    void prepare(loki::FunctionExpressionMultiOperator function_expression);
    void prepare(loki::FunctionExpressionMinus function_expression);
    void prepare(loki::FunctionExpressionFunction function_expression);
    void prepare(loki::FunctionExpression function_expression);
    void prepare(loki::Function function);
    void prepare(loki::Condition condition);
    void prepare(loki::Effect effect);
    void prepare(loki::Action action);
    void prepare(loki::Axiom axiom);
    void prepare(loki::Domain domain);
    void prepare(loki::FunctionValue function_value);
    void prepare(loki::OptimizationMetric metric);
    void prepare(loki::Problem problem);

    /**
     * Common translations.
     */

    template<formalism::Context C>
    IndexFunctionVariant translate_common(loki::FunctionSkeleton function_skeleton, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Index<formalism::Object> translate_common(loki::Object object, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Index<formalism::Variable> translate_common(loki::Parameter parameter, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    IndexPredicateVariant translate_common(loki::Predicate predicate, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Index<formalism::Variable> translate_common(loki::Variable variable, formalism::Builder& builder, C& context);

    /**
     * Lifted translation.
     */

    template<typename T, formalism::Context C>
    auto translate_lifted(const std::vector<const T*>& input, formalism::Builder& builder, C& context)
    {
        using ReturnType = decltype(this->translate_lifted(std::declval<const T*>(), builder, std::declval<C&>()));
        auto output = ::cista::offset::vector<ReturnType> {};
        output.reserve(input.size());
        std::transform(std::begin(input),
                       std::end(input),
                       std::back_inserter(output),
                       [&](auto&& arg) { return this->translate_lifted(arg, builder, context); });
        return output;
    }

    template<formalism::Context C>
    Data<formalism::Term> translate_lifted(loki::Term term, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    IndexAtomVariant translate_lifted(loki::Atom atom, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    IndexLiteralVariant translate_lifted(loki::Literal literal, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Data<formalism::FunctionExpression> translate_lifted(loki::FunctionExpressionNumber function_expression, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Data<formalism::FunctionExpression> translate_lifted(loki::FunctionExpressionBinaryOperator function_expression, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Data<formalism::FunctionExpression> translate_lifted(loki::FunctionExpressionMultiOperator function_expression, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Data<formalism::FunctionExpression> translate_lifted(loki::FunctionExpressionMinus function_expression, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Data<formalism::FunctionExpression> translate_lifted(loki::FunctionExpressionFunction function_expression, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Data<formalism::FunctionExpression> translate_lifted(loki::FunctionExpression function_expression, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    IndexFunctionVariant translate_lifted(loki::Function function, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Data<formalism::BooleanOperator<Data<formalism::FunctionExpression>>>
    translate_lifted(loki::ConditionNumericConstraint condition, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Index<formalism::ConjunctiveCondition>
    translate_lifted(loki::Condition condition, const IndexList<formalism::Variable>& parameters, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    IndexList<formalism::ConditionalEffect>
    translate_lifted(loki::Effect effect, const IndexList<formalism::Variable>& parameters, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Index<formalism::Action> translate_lifted(loki::Action action, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Index<formalism::Axiom> translate_lifted(loki::Axiom axiom, formalism::Builder& builder, C& context);

    /**
     * Grounded translation
     */

    template<typename T, formalism::Context C>
    auto translate_grounded(const std::vector<const T*>& input, formalism::Builder& builder, C& context)
    {
        using ReturnType = decltype(this->translate_grounded(std::declval<const T*>(), builder, std::declval<C&>()));
        auto output = ::cista::offset::vector<ReturnType> {};
        output.reserve(input.size());
        std::transform(std::begin(input), std::end(input), std::back_inserter(output), [&](auto&& arg) { return this->translate_grounded(arg, context); });
        return output;
    }

    template<formalism::Context C>
    Index<formalism::Object> translate_grounded(loki::Term term, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    IndexGroundAtomVariant translate_grounded(loki::Atom atom, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    IndexGroundLiteralVariant translate_grounded(loki::Literal literal, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Data<formalism::GroundFunctionExpression> translate_grounded(loki::FunctionExpressionNumber function_expression, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Data<formalism::GroundFunctionExpression>
    translate_grounded(loki::FunctionExpressionBinaryOperator function_expression, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Data<formalism::GroundFunctionExpression>
    translate_grounded(loki::FunctionExpressionMultiOperator function_expression, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Data<formalism::GroundFunctionExpression> translate_grounded(loki::FunctionExpressionMinus function_expression, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Data<formalism::GroundFunctionExpression> translate_grounded(loki::FunctionExpressionFunction function_expression, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Data<formalism::GroundFunctionExpression> translate_grounded(loki::FunctionExpression function_expression, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    IndexGroundFunctionTermVariant translate_grounded(loki::Function function, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    IndexGroundFunctionTermValueVariant translate_grounded(loki::FunctionValue numeric_fluent, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Data<formalism::BooleanOperator<Data<formalism::GroundFunctionExpression>>>
    translate_grounded(loki::ConditionNumericConstraint condition, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    std::tuple<IndexList<formalism::GroundLiteral<formalism::StaticTag>>,
               IndexList<formalism::GroundLiteral<formalism::FluentTag>>,
               IndexList<formalism::GroundLiteral<formalism::DerivedTag>>,
               DataList<formalism::BooleanOperator<Data<formalism::GroundFunctionExpression>>>>
    translate_grounded(loki::Condition condition, formalism::Builder& builder, C& context);

    template<formalism::Context C>
    Index<formalism::Metric> translate_grounded(loki::OptimizationMetric optimization_metric, formalism::Builder& builder, C& context);

public:
    LokiToTyrTranslator();

    DomainPtr translate(const loki::Domain& domain, formalism::Builder& builder, formalism::RepositoryPtr context);

    LiftedTaskPtr translate(const loki::Problem& problem, formalism::Builder& builder, formalism::OverlayRepositoryPtr<formalism::Repository> context);
};

}

#endif