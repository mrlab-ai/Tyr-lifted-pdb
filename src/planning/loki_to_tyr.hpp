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
#include "tyr/formalism/canonicalization.hpp"
#include "tyr/formalism/declarations.hpp"
#include "tyr/formalism/overlay_repository.hpp"
#include "tyr/formalism/repository.hpp"
#include "tyr/formalism/views.hpp"
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

using IndexNumericEffectVariant = std::variant<Index<formalism::NumericEffect<formalism::FluentTag>>, Index<formalism::NumericEffect<formalism::AuxiliaryTag>>>;

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
    void prepare(loki::FunctionSkeleton element);
    void prepare(loki::Object element);
    void prepare(loki::Parameter element);
    void prepare(loki::Predicate element);
    void prepare(loki::Requirements element);
    void prepare(loki::Type element);
    void prepare(loki::Variable element);
    void prepare(loki::Term element);
    void prepare(loki::Atom element);
    void prepare(loki::Literal element);
    void prepare(loki::FunctionExpressionNumber element);
    void prepare(loki::FunctionExpressionBinaryOperator element);
    void prepare(loki::FunctionExpressionMultiOperator element);
    void prepare(loki::FunctionExpressionMinus element);
    void prepare(loki::FunctionExpressionFunction element);
    void prepare(loki::FunctionExpression element);
    void prepare(loki::Function element);
    void prepare(loki::Condition element);
    void prepare(loki::Effect element);
    void prepare(loki::Action element);
    void prepare(loki::Axiom element);
    void prepare(loki::Domain element);
    void prepare(loki::FunctionValue element);
    void prepare(loki::OptimizationMetric element);
    void prepare(loki::Problem element);

    /**
     * Common translations.
     */

    struct ParameterIndexMapping
    {
        ParameterIndexMapping() = default;

        UnorderedMap<Index<formalism::Variable>, formalism::ParameterIndex> map;

        void push_parameters(const IndexList<formalism::Variable>& parameters)
        {
            for (const auto parameter : parameters)
                map.emplace(parameter, map.size());
        }

        void pop_parameters(const IndexList<formalism::Variable>& parameters)
        {
            for (const auto parameter : parameters)
                map.erase(parameter);
        }

        formalism::ParameterIndex lookup_parameter_index(Index<formalism::Variable> variable) { return map.at(variable); }
    };

    ParameterIndexMapping m_param_map;

    template<typename T, formalism::Context C>
    auto translate_common(const std::vector<const T*>& input, formalism::Builder& builder, C& context)
    {
        using ReturnType = decltype(this->translate_common(std::declval<const T*>(), builder, std::declval<C&>()));
        auto output = ::cista::offset::vector<ReturnType> {};
        output.reserve(input.size());
        std::transform(std::begin(input),
                       std::end(input),
                       std::back_inserter(output),
                       [&](auto&& arg) { return this->translate_common(arg, builder, context); });
        return output;
    }

    template<formalism::Context C>
    IndexFunctionVariant translate_common(loki::FunctionSkeleton element, formalism::Builder& builder, C& context)
    {
        auto build_function = [&](auto fact_tag) -> IndexFunctionVariant
        {
            using Tag = std::decay_t<decltype(fact_tag)>;

            auto& function = builder.template get_function<Tag>();
            function.clear();
            function.name = element->get_name();
            function.arity = element->get_parameters().size();
            formalism::canonicalize(function);
            return context.get_or_create(function, builder.get_buffer()).first.get_index();
        };

        if (element->get_name() == "total-cost")
            return build_function(formalism::AuxiliaryTag {});
        else if (m_effect_function_skeletons.contains(element->get_name()))
            return build_function(formalism::FluentTag {});
        else
            return build_function(formalism::StaticTag {});
    }

    template<formalism::Context C>
    Index<formalism::Object> translate_common(loki::Object element, formalism::Builder& builder, C& context)
    {
        auto& object = builder.get_object();
        object.clear();
        object.name = element->get_name();
        formalism::canonicalize(object);
        return context.get_or_create(object, builder.get_buffer()).first.get_index();
    }

    template<formalism::Context C>
    Index<formalism::Variable> translate_common(loki::Parameter element, formalism::Builder& builder, C& context)
    {
        return translate_common(element->get_variable(), builder, context);
    }

    template<formalism::Context C>
    IndexPredicateVariant translate_common(loki::Predicate element, formalism::Builder& builder, C& context)
    {
        auto build_predicate = [&](auto fact_tag) -> IndexPredicateVariant
        {
            using Tag = std::decay_t<decltype(fact_tag)>;

            auto& predicate = builder.template get_predicate<Tag>();
            predicate.clear();
            predicate.name = element->get_name();
            predicate.arity = element->get_parameters().size();
            formalism::canonicalize(predicate);
            return context.get_or_create(predicate, builder.get_buffer()).first.get_index();
        };

        if (m_fluent_predicates.count(element->get_name()) && !m_derived_predicates.count(element->get_name()))
            return build_predicate(formalism::FluentTag {});
        else if (m_derived_predicates.count(element->get_name()))
            return build_predicate(formalism::DerivedTag {});
        else
            return build_predicate(formalism::StaticTag {});
    }

    template<formalism::Context C>
    Index<formalism::Variable> translate_common(loki::Variable element, formalism::Builder& builder, C& context)
    {
        auto& variable = builder.get_variable();
        variable.clear();
        variable.name = element->get_name();
        formalism::canonicalize(variable);
        return context.get_or_create(variable, builder.get_buffer()).first.get_index();
    }

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
    Data<formalism::Term> translate_lifted(loki::Term element, formalism::Builder& builder, C& context)
    {
        return std::visit(
            [&](auto&& arg) -> Data<formalism::Term>
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, loki::Object>)
                    return Data<formalism::Term>(translate_common(arg, builder, context));
                else if constexpr (std::is_same_v<T, loki::Variable>)
                    return Data<formalism::Term>(m_param_map.lookup_parameter_index(translate_common(arg, builder, context)));
                else
                    static_assert(dependent_false<T>::value, "Missing case for type");
            },
            element->get_object_or_variable());
    }

    template<formalism::Context C>
    IndexAtomVariant translate_lifted(loki::Atom element, formalism::Builder& builder, C& context)
    {
        auto index_predicate_variant = translate_common(element->get_predicate(), builder, context);

        auto build_atom = [&](auto fact_tag, auto predicate_index) -> IndexAtomVariant
        {
            using Tag = std::decay_t<decltype(fact_tag)>;

            auto& atom = builder.template get_atom<Tag>();
            atom.clear();
            atom.predicate = predicate_index;
            atom.terms = this->translate_lifted(element->get_terms(), builder, context);
            formalism::canonicalize(atom);
            return context.get_or_create(atom, builder.get_buffer()).first.get_index();
        };

        return std::visit(
            [&](auto&& arg) -> IndexAtomVariant
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Index<formalism::Predicate<formalism::StaticTag>>>)
                    return build_atom(formalism::StaticTag {}, arg);
                else if constexpr (std::is_same_v<T, Index<formalism::Predicate<formalism::FluentTag>>>)
                    return build_atom(formalism::FluentTag {}, arg);
                else if constexpr (std::is_same_v<T, Index<formalism::Predicate<formalism::DerivedTag>>>)
                    return build_atom(formalism::DerivedTag {}, arg);
                else
                    static_assert(dependent_false<T>::value, "Missing case for type");
            },
            index_predicate_variant);
    }

    template<formalism::Context C>
    IndexLiteralVariant translate_lifted(loki::Literal element, formalism::Builder& builder, C& context)
    {
        auto index_atom_variant = translate_lifted(element->get_atom(), builder, context);

        auto build_literal = [&](auto fact_tag, auto atom_index) -> IndexLiteralVariant
        {
            using Tag = std::decay_t<decltype(fact_tag)>;

            auto& literal = builder.template get_literal<Tag>();
            literal.clear();
            literal.atom = atom_index;
            literal.polarity = element->get_polarity();
            formalism::canonicalize(literal);
            return context.get_or_create(literal, builder.get_buffer()).first.get_index();
        };

        return std::visit(
            [&](auto&& arg) -> IndexLiteralVariant
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Index<formalism::Atom<formalism::StaticTag>>>)
                    return build_literal(formalism::StaticTag {}, arg);
                else if constexpr (std::is_same_v<T, Index<formalism::Atom<formalism::FluentTag>>>)
                    return build_literal(formalism::FluentTag {}, arg);
                else if constexpr (std::is_same_v<T, Index<formalism::Atom<formalism::DerivedTag>>>)
                    return build_literal(formalism::DerivedTag {}, arg);
                else
                    static_assert(dependent_false<T>::value, "Missing case for type");
            },
            index_atom_variant);
    }

    template<formalism::Context C>
    Data<formalism::FunctionExpression> translate_lifted(loki::FunctionExpressionNumber element, formalism::Builder& builder, C& context)
    {
        return Data<formalism::FunctionExpression>(float_t(element->get_number()));
    }

    template<formalism::Context C>
    Data<formalism::FunctionExpression> translate_lifted(loki::FunctionExpressionBinaryOperator element, formalism::Builder& builder, C& context)
    {
        auto build_binary_op = [&](auto op_tag) -> Data<formalism::FunctionExpression>
        {
            using Tag = std::decay_t<decltype(op_tag)>;

            auto& binary = builder.template get_binary<Tag>();
            binary.clear();
            auto lhs_result = translate_lifted(element->get_left_function_expression(), builder, context);
            auto rhs_result = translate_lifted(element->get_right_function_expression(), builder, context);
            binary.lhs = lhs_result;
            binary.rhs = rhs_result;
            formalism::canonicalize(binary);
            return Data<formalism::FunctionExpression>(context.get_or_create(binary, builder.get_buffer()).first.get_index());
        };

        switch (element->get_binary_operator())
        {
            case loki::BinaryOperatorEnum::PLUS:
                return build_binary_op(formalism::OpAdd {});
            case loki::BinaryOperatorEnum::MINUS:
                return build_binary_op(formalism::OpSub {});
            case loki::BinaryOperatorEnum::MUL:
                return build_binary_op(formalism::OpMul {});
            case loki::BinaryOperatorEnum::DIV:
                return build_binary_op(formalism::OpDiv {});
            default:
                throw std::runtime_error("Unexpected case");
        }
    }

    template<formalism::Context C>
    Data<formalism::FunctionExpression> translate_lifted(loki::FunctionExpressionMultiOperator element, formalism::Builder& builder, C& context)
    {
        auto build_multi_op = [&](auto op_tag) -> Data<formalism::FunctionExpression>
        {
            using Tag = std::decay_t<decltype(op_tag)>;

            auto& multi = builder.template get_multi<Tag>();
            multi.clear();
            multi.args = translate_lifted(element->get_function_expressions(), builder, context);
            formalism::canonicalize(multi);
            return Data<formalism::FunctionExpression>(context.get_or_create(multi, builder.get_buffer()).first.get_index());
        };

        switch (element->get_multi_operator())
        {
            case loki::MultiOperatorEnum::PLUS:
                return build_multi_op(formalism::OpAdd {});
            case loki::MultiOperatorEnum::MUL:
                return build_multi_op(formalism::OpMul {});
            default:
                throw std::runtime_error("Unexpected case");
        }
    }

    template<formalism::Context C>
    Data<formalism::FunctionExpression> translate_lifted(loki::FunctionExpressionMinus element, formalism::Builder& builder, C& context)
    {
        auto& minus = builder.template get_unary<formalism::OpSub>();
        minus.clear();
        minus.arg = translate_lifted(element->get_function_expression(), builder, context);
        formalism::canonicalize(minus);
        return Data<formalism::FunctionExpression>(context.get_or_create(minus, builder.get_buffer()).first.get_index());
    }

    template<formalism::Context C>
    Data<formalism::FunctionExpression> translate_lifted(loki::FunctionExpressionFunction element, formalism::Builder& builder, C& context)
    {
        const auto index_fterm_variant = translate_lifted(element->get_function(), builder, context);

        return std::visit(
            [&](auto&& arg) -> Data<formalism::FunctionExpression>
            {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, Index<formalism::FunctionTerm<formalism::StaticTag>>>)
                    return Data<formalism::FunctionExpression>(arg);
                else if constexpr (std::is_same_v<T, Index<formalism::FunctionTerm<formalism::FluentTag>>>)
                    return Data<formalism::FunctionExpression>(arg);
                else if constexpr (std::is_same_v<T, Index<formalism::FunctionTerm<formalism::AuxiliaryTag>>>)
                    throw std::runtime_error("Cannot create FunctionExpression over auxiliary function term.");
                else
                    static_assert(dependent_false<T>::value, "Missing case for type");
            },
            index_fterm_variant);
    }

    template<formalism::Context C>
    Data<formalism::FunctionExpression> translate_lifted(loki::FunctionExpression element, formalism::Builder& builder, C& context)
    {
        return std::visit([&](auto&& arg) { return translate_lifted(arg, builder, context); }, element->get_function_expression());
    }

    template<formalism::Context C>
    IndexFunctionTermVariant translate_lifted(loki::Function element, formalism::Builder& builder, C& context)
    {
        auto index_function_variant = translate_common(element->get_function_skeleton(), builder, context);

        auto build_function_term = [&](auto fact_tag, auto function_index) -> IndexFunctionTermVariant
        {
            using Tag = std::decay_t<decltype(fact_tag)>;

            auto& fterm = builder.template get_fterm<Tag>();
            fterm.clear();
            fterm.function = function_index;
            fterm.terms = this->translate_lifted(element->get_terms(), builder, context);
            formalism::canonicalize(fterm);
            return context.get_or_create(fterm, builder.get_buffer()).first.get_index();
        };

        return std::visit(
            [&](auto&& arg) -> IndexFunctionTermVariant
            {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, Index<formalism::Function<formalism::StaticTag>>>)
                    return build_function_term(formalism::StaticTag {}, arg);
                else if constexpr (std::is_same_v<T, Index<formalism::Function<formalism::FluentTag>>>)
                    return build_function_term(formalism::FluentTag {}, arg);
                else if constexpr (std::is_same_v<T, Index<formalism::Function<formalism::AuxiliaryTag>>>)
                    return build_function_term(formalism::AuxiliaryTag {}, arg);
                else
                    static_assert(dependent_false<T>::value, "Missing case for type");
            },
            index_function_variant);
    }

    template<formalism::Context C>
    Data<formalism::BooleanOperator<Data<formalism::FunctionExpression>>>
    translate_lifted(loki::ConditionNumericConstraint element, formalism::Builder& builder, C& context)
    {
        auto build_binary_op = [&](auto op_tag) -> Data<formalism::BooleanOperator<Data<formalism::FunctionExpression>>>
        {
            using Tag = std::decay_t<decltype(op_tag)>;

            auto& binary = builder.template get_binary<Tag>();
            binary.clear();
            auto lhs_result = translate_lifted(element->get_left_function_expression(), builder, context);
            auto rhs_result = translate_lifted(element->get_right_function_expression(), builder, context);
            binary.lhs = lhs_result;
            binary.rhs = rhs_result;
            formalism::canonicalize(binary);
            return Data<formalism::BooleanOperator<Data<formalism::FunctionExpression>>>(context.get_or_create(binary, builder.get_buffer()).first.get_index());
        };

        switch (element->get_binary_comparator())
        {
            case loki::BinaryComparatorEnum::EQUAL:
                return build_binary_op(formalism::OpEq {});
            case loki::BinaryComparatorEnum::LESS_EQUAL:
                return build_binary_op(formalism::OpLe {});
            case loki::BinaryComparatorEnum::LESS:
                return build_binary_op(formalism::OpLt {});
            case loki::BinaryComparatorEnum::GREATER_EQUAL:
                return build_binary_op(formalism::OpGe {});
            case loki::BinaryComparatorEnum::GREATER:
                return build_binary_op(formalism::OpGt {});
            default:
                throw std::runtime_error("Unexpected case");
        }
    }

    template<formalism::Context C>
    Index<formalism::ConjunctiveCondition>
    translate_lifted(loki::Condition element, const IndexList<formalism::Variable>& parameters, formalism::Builder& builder, C& context)
    {
        auto& conj_condition = builder.get_conj_cond();
        conj_condition.clear();

        conj_condition.variables = parameters;

        const auto func_insert_literal = [](IndexLiteralVariant index_literal_variant,
                                            IndexList<formalism::Literal<formalism::StaticTag>>& static_literals,
                                            IndexList<formalism::Literal<formalism::FluentTag>>& fluent_literals,
                                            IndexList<formalism::Literal<formalism::DerivedTag>>& derived_literals)
        {
            std::visit(
                [&](auto&& arg)
                {
                    using T = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_same_v<T, Index<formalism::Literal<formalism::StaticTag>>>)
                        static_literals.push_back(arg);
                    else if constexpr (std::is_same_v<T, Index<formalism::Literal<formalism::FluentTag>>>)
                        fluent_literals.push_back(arg);
                    else if constexpr (std::is_same_v<T, Index<formalism::Literal<formalism::DerivedTag>>>)
                        derived_literals.push_back(arg);
                    else
                        static_assert(dependent_false<T>::value, "Missing case for type");
                },
                index_literal_variant);
        };

        if (const auto condition_and = std::get_if<loki::ConditionAnd>(&element->get_condition()))
        {
            for (const auto& part : (*condition_and)->get_conditions())
            {
                if (const auto condition_literal = std::get_if<loki::ConditionLiteral>(&part->get_condition()))
                {
                    const auto index_literal_variant = translate_lifted((*condition_literal)->get_literal(), builder, context);

                    func_insert_literal(index_literal_variant, conj_condition.static_literals, conj_condition.fluent_literals, conj_condition.derived_literals);
                }
                else if (const auto condition_numeric_constraint = std::get_if<loki::ConditionNumericConstraint>(&part->get_condition()))
                {
                    const auto numeric_constraint = translate_lifted((*condition_numeric_constraint), builder, context);

                    conj_condition.numeric_constraints.push_back(numeric_constraint);
                }
                else
                {
                    // std::cout << std::visit([](auto&& arg) { return arg.str(); }, *part) << std::endl;

                    throw std::logic_error("Expected literal in conjunctive condition.");
                }
            }

            formalism::canonicalize(conj_condition);
            return context.get_or_create(conj_condition, builder.get_buffer()).first.get_index();
        }
        else if (const auto condition_literal = std::get_if<loki::ConditionLiteral>(&element->get_condition()))
        {
            const auto index_literal_variant = translate_lifted((*condition_literal)->get_literal(), builder, context);

            func_insert_literal(index_literal_variant, conj_condition.static_literals, conj_condition.fluent_literals, conj_condition.derived_literals);

            formalism::canonicalize(conj_condition);
            return context.get_or_create(conj_condition, builder.get_buffer()).first.get_index();
        }
        else if (const auto condition_numeric_constraint = std::get_if<loki::ConditionNumericConstraint>(&element->get_condition()))
        {
            const auto numeric_constraint = translate_lifted((*condition_numeric_constraint), builder, context);

            conj_condition.numeric_constraints.push_back(numeric_constraint);

            formalism::canonicalize(conj_condition);
            return context.get_or_create(conj_condition, builder.get_buffer()).first.get_index();
        }

        // std::cout << std::visit([](auto&& arg) { return arg.str(); }, *condition_ptr) << std::endl;

        throw std::logic_error("Expected conjunctive condition.");
    }

    template<formalism::Context C>
    IndexNumericEffectVariant translate_lifted(loki::EffectNumeric element, formalism::Builder& builder, C& context)
    {
        auto index_fterm_variant = translate_lifted(element->get_function(), builder, context);

        auto build_numeric_effect_term = [&](auto fact_tag, auto fterm_index) -> IndexNumericEffectVariant
        {
            using Tag = std::decay_t<decltype(fact_tag)>;

            auto& numeric_effect = builder.get_numeric_effect<Tag>();
            numeric_effect.clear();
            numeric_effect.fterm = fterm_index;
            numeric_effect.fexpr = this->translate_lifted(element->get_function_expression(), builder, context);
            formalism::canonicalize(numeric_effect);
            return context.get_or_create(numeric_effect, builder.get_buffer()).first.get_index();
        };

        return std::visit(
            [&](auto&& arg) -> IndexNumericEffectVariant
            {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, Index<formalism::FunctionTerm<formalism::StaticTag>>>)
                    throw std::runtime_error("Cannot create NumericEffect over static function term.");
                else if constexpr (std::is_same_v<T, Index<formalism::FunctionTerm<formalism::FluentTag>>>)
                    return build_numeric_effect_term(formalism::FluentTag {}, arg);
                else if constexpr (std::is_same_v<T, Index<formalism::FunctionTerm<formalism::AuxiliaryTag>>>)
                    return build_numeric_effect_term(formalism::AuxiliaryTag {}, arg);
                else
                    static_assert(dependent_false<T>::value, "Missing case for type");
            },
            index_fterm_variant);
    }

    template<formalism::Context C>
    IndexList<formalism::ConditionalEffect> translate_lifted(loki::Effect element, formalism::Builder& builder, C& context)
    {
        using ConditionalEffectData = UnorderedMap<Index<formalism::ConjunctiveCondition>,
                                                   std::tuple<IndexList<formalism::Literal<formalism::FluentTag>>,
                                                              IndexList<formalism::NumericEffect<formalism::FluentTag>>,
                                                              ::cista::optional<Index<formalism::NumericEffect<formalism::AuxiliaryTag>>>>>;

        const auto translate_effect_func = [&](loki::Effect effect, ConditionalEffectData& ref_conditional_effect_data)
        {
            auto tmp_effect = effect;

            /* 1. Parse universal part. */
            auto parameters = IndexList<formalism::Variable> {};
            if (const auto& tmp_effect_forall = std::get_if<loki::EffectCompositeForall>(&tmp_effect->get_effect()))
            {
                parameters = translate_common((*tmp_effect_forall)->get_parameters(), builder, context);

                tmp_effect = (*tmp_effect_forall)->get_effect();
            }

            ///---------- Push parameters and parse scope -------------
            m_param_map.push_parameters(parameters);
            {
                /* 2. Parse conditional part */
                auto conjunctive_condition = Index<formalism::ConjunctiveCondition>::max();
                if (const auto tmp_effect_when = std::get_if<loki::EffectCompositeWhen>(&tmp_effect->get_effect()))
                {
                    conjunctive_condition = translate_lifted((*tmp_effect_when)->get_condition(), parameters, builder, context);

                    tmp_effect = (*tmp_effect_when)->get_effect();
                }
                if (conjunctive_condition == Index<formalism::ConjunctiveCondition>::max())
                {
                    // Create empty conjunctive condition for unconditional effects
                    auto& conj_cond = builder.get_conj_cond();
                    conj_cond.clear();
                    canonicalize(conj_cond);
                    conjunctive_condition = context.get_or_create(conj_cond, builder.get_buffer()).first.get_index();
                }

                // Fetch container to store the effects
                auto& effect_data = ref_conditional_effect_data[conjunctive_condition];
                auto& data_fluent_literals = std::get<0>(effect_data);
                auto& data_fluent_numeric_effects = std::get<1>(effect_data);
                auto& data_auxiliary_numeric_effect = std::get<2>(effect_data);

                /* 3. Parse effect part */
                if (const auto& effect_literal = std::get_if<loki::EffectLiteral>(&tmp_effect->get_effect()))
                {
                    const auto index_literal_variant = translate_lifted((*effect_literal)->get_literal(), builder, context);

                    if (std::get_if<Index<formalism::Literal<formalism::DerivedTag>>>(&index_literal_variant))
                    {
                        throw std::runtime_error("Effect literal cannot be Derived!");
                    }
                    else if (std::get_if<Index<formalism::Literal<formalism::StaticTag>>>(&index_literal_variant))
                    {
                        throw std::logic_error("Effect lieral cannot be Static!");
                    }

                    data_fluent_literals.push_back(std::get<Index<formalism::Literal<formalism::FluentTag>>>(index_literal_variant));
                }
                else if (const auto& effect_numeric = std::get_if<loki::EffectNumeric>(&tmp_effect->get_effect()))
                {
                    const auto index_numeric_effect_variant = translate_lifted((*effect_numeric), builder, context);

                    if (std::get_if<Index<formalism::NumericEffect<formalism::FluentTag>>>(&index_numeric_effect_variant))
                    {
                        data_fluent_numeric_effects.push_back(std::get<Index<formalism::NumericEffect<formalism::FluentTag>>>(index_numeric_effect_variant));
                    }
                    else if (std::get_if<Index<formalism::NumericEffect<formalism::AuxiliaryTag>>>(&index_numeric_effect_variant))
                    {
                        assert(!data_auxiliary_numeric_effect);
                        data_auxiliary_numeric_effect = std::get<Index<formalism::NumericEffect<formalism::AuxiliaryTag>>>(index_numeric_effect_variant);
                    }
                }
                else
                {
                    // std::cout << std::visit([](auto&& arg) { return arg.str(); }, *tmp_effect) << std::endl;

                    throw std::logic_error("Unexpected effect type. This error indicates a bug in the translation.");
                }
            }
            ///---------- Pop parameters -------------
            m_param_map.pop_parameters(parameters);
        };

        /* Parse the effect */
        auto conditional_effect_data = ConditionalEffectData {};
        // Parse conjunctive part
        if (const auto& effect_and = std::get_if<loki::EffectAnd>(&element->get_effect()))
        {
            for (const auto& nested_effect : (*effect_and)->get_effects())
            {
                translate_effect_func(nested_effect, conditional_effect_data);
            }
        }
        else
        {
            // Parse non conjunctive
            translate_effect_func(element, conditional_effect_data);
        }

        /* Instantiate conditional effects. */
        auto conditional_effects = IndexList<formalism::ConditionalEffect> {};

        for (const auto& [cond_conjunctive_condition, value] : conditional_effect_data)
        {
            const auto& [cond_effect_fluent_literals, cond_effect_fluent_numeric_effects, cond_effect_auxiliary_numeric_effects] = value;

            auto& conj_effect = builder.get_conj_effect();
            conj_effect.clear();
            conj_effect.literals = cond_effect_fluent_literals;
            conj_effect.numeric_effects = cond_effect_fluent_numeric_effects;
            conj_effect.auxiliary_numeric_effect = cond_effect_auxiliary_numeric_effects;
            formalism::canonicalize(conj_effect);
            const auto conj_effect_index = context.get_or_create(conj_effect, builder.get_buffer()).first.get_index();

            auto& cond_effect = builder.get_cond_effect();
            cond_effect.clear();
            cond_effect.condition = cond_conjunctive_condition;
            cond_effect.effect = conj_effect_index;
            formalism::canonicalize(cond_effect);
            const auto cond_effect_index = context.get_or_create(cond_effect, builder.get_buffer()).first.get_index();

            conditional_effects.push_back(cond_effect_index);
        }

        return conditional_effects;
    }

    template<formalism::Context C>
    Index<formalism::Action> translate_lifted(loki::Action element, formalism::Builder& builder, C& context)
    {
        auto& action = builder.get_action();
        action.original_arity = element->get_original_arity();

        // 1. Translate conditions
        auto parameters = translate_common(element->get_parameters(), builder, context);
        ///---------- Push parameters and parse scope -------------
        m_param_map.push_parameters(parameters);
        {
            auto conjunctive_condition = Index<formalism::ConjunctiveCondition>::max();
            if (element->get_condition().has_value())
            {
                conjunctive_condition = translate_lifted(element->get_condition().value(), parameters, builder, context);
            }
            else
            {
                // Create empty one
                auto& conj_cond = builder.get_conj_cond();
                conj_cond.clear();
                canonicalize(conj_cond);
                conjunctive_condition = context.get_or_create(conj_cond, builder.get_buffer()).first.get_index();
            }
            action.condition = conjunctive_condition;

            // 2. Translate effects
            auto conditional_effects = IndexList<formalism::ConditionalEffect> {};
            if (element->get_effect().has_value())
            {
                const auto conditional_effects_ = translate_lifted(element->get_effect().value(), builder, context);
                conditional_effects = conditional_effects_;
            }
            action.effects = conditional_effects;
        }
        ///---------- Pop parameters -------------
        m_param_map.pop_parameters(parameters);

        formalism::canonicalize(action);
        return context.get_or_create(action, builder.get_buffer()).first.get_index();
    }

    template<formalism::Context C>
    Index<formalism::Axiom> translate_lifted(loki::Axiom element, formalism::Builder& builder, C& context)
    {
        auto& axiom = builder.get_axiom();
        axiom.clear();

        auto parameters = translate_common(element->get_parameters(), builder, context);
        ///---------- Push parameters and parse scope -------------
        m_param_map.push_parameters(parameters);
        {
            axiom.body = translate_lifted(element->get_condition(), parameters, builder, context);
            const auto index_literal_variant = translate_lifted(element->get_literal(), builder, context);

            std::visit(
                [&](auto&& arg)
                {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, Index<formalism::Literal<formalism::DerivedTag>>>)
                    {
                        // We store atoms in the head, not literals
                        axiom.head = View<Index<formalism::Literal<formalism::DerivedTag>>, C>(arg, context).get_atom().get_index();
                    }
                    else
                    {
                        throw std::runtime_error("ToMimirStructures::translate_lifted: Expected Literal<DerivedTag> in axiom head.");
                    }
                },
                index_literal_variant);
        }
        ///---------- Pop parameters -------------
        m_param_map.pop_parameters(parameters);

        formalism::canonicalize(axiom);
        return context.get_or_create(axiom, builder.get_buffer()).first.get_index();
    }

    /**
     * Grounded translation
     */

    template<typename T, formalism::Context C>
    auto translate_grounded(const std::vector<const T*>& input, formalism::Builder& builder, C& context)
    {
        using ReturnType = decltype(this->translate_grounded(std::declval<const T*>(), builder, std::declval<C&>()));
        auto output = ::cista::offset::vector<ReturnType> {};
        output.reserve(input.size());
        std::transform(std::begin(input),
                       std::end(input),
                       std::back_inserter(output),
                       [&](auto&& arg) { return this->translate_grounded(arg, builder, context); });
        return output;
    }

    template<formalism::Context C>
    Index<formalism::Object> translate_grounded(loki::Term element, formalism::Builder& builder, C& context)
    {
        return std::visit(
            [&](auto&& arg) -> Index<formalism::Object>
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, loki::Object>)
                    return translate_common(arg, builder, context);
                else if constexpr (std::is_same_v<T, loki::Variable>)
                    throw std::runtime_error("Expected ground term.");
                else
                    static_assert(dependent_false<T>::value, "Missing case for type");
            },
            element->get_object_or_variable());
    }

    template<formalism::Context C>
    IndexGroundAtomVariant translate_grounded(loki::Atom element, formalism::Builder& builder, C& context)
    {
        auto index_predicate_variant = translate_common(element->get_predicate(), builder, context);

        auto build_atom = [&](auto fact_tag, auto predicate_index) -> IndexGroundAtomVariant
        {
            using Tag = std::decay_t<decltype(fact_tag)>;

            auto& atom = builder.template get_ground_atom<Tag>();
            atom.clear();
            atom.predicate = predicate_index;
            atom.objects = this->translate_grounded(element->get_terms(), builder, context);
            formalism::canonicalize(atom);
            return context.get_or_create(atom, builder.get_buffer()).first.get_index();
        };

        return std::visit(
            [&](auto&& arg) -> IndexGroundAtomVariant
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Index<formalism::Predicate<formalism::StaticTag>>>)
                    return build_atom(formalism::StaticTag {}, arg);
                else if constexpr (std::is_same_v<T, Index<formalism::Predicate<formalism::FluentTag>>>)
                    return build_atom(formalism::FluentTag {}, arg);
                else if constexpr (std::is_same_v<T, Index<formalism::Predicate<formalism::DerivedTag>>>)
                    return build_atom(formalism::DerivedTag {}, arg);
                else
                    static_assert(dependent_false<T>::value, "Missing case for type");
            },
            index_predicate_variant);
    }

    template<formalism::Context C>
    IndexGroundLiteralVariant translate_grounded(loki::Literal element, formalism::Builder& builder, C& context)
    {
        auto index_atom_variant = translate_grounded(element->get_atom(), builder, context);

        auto build_literal = [&](auto fact_tag, auto atom_index) -> IndexGroundLiteralVariant
        {
            using Tag = std::decay_t<decltype(fact_tag)>;

            auto& literal = builder.template get_ground_literal<Tag>();
            literal.clear();
            literal.atom = atom_index;
            literal.polarity = element->get_polarity();
            formalism::canonicalize(literal);
            return context.get_or_create(literal, builder.get_buffer()).first.get_index();
        };

        return std::visit(
            [&](auto&& arg) -> IndexGroundLiteralVariant
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Index<formalism::GroundAtom<formalism::StaticTag>>>)
                    return build_literal(formalism::StaticTag {}, arg);
                else if constexpr (std::is_same_v<T, Index<formalism::GroundAtom<formalism::FluentTag>>>)
                    return build_literal(formalism::FluentTag {}, arg);
                else if constexpr (std::is_same_v<T, Index<formalism::GroundAtom<formalism::DerivedTag>>>)
                    return build_literal(formalism::DerivedTag {}, arg);
                else
                    static_assert(dependent_false<T>::value, "Missing case for type");
            },
            index_atom_variant);
    }

    template<formalism::Context C>
    Data<formalism::GroundFunctionExpression> translate_grounded(loki::FunctionExpressionNumber element, formalism::Builder& builder, C& context)
    {
        return Data<formalism::GroundFunctionExpression>(float_t(element->get_number()));
    }

    template<formalism::Context C>
    Data<formalism::GroundFunctionExpression> translate_grounded(loki::FunctionExpressionBinaryOperator element, formalism::Builder& builder, C& context)
    {
        auto build_binary_op = [&](auto op_tag) -> Data<formalism::GroundFunctionExpression>
        {
            using Tag = std::decay_t<decltype(op_tag)>;

            auto& binary = builder.template get_ground_binary<Tag>();
            binary.clear();
            auto lhs_result = translate_grounded(element->get_left_function_expression(), builder, context);
            auto rhs_result = translate_grounded(element->get_right_function_expression(), builder, context);
            binary.lhs = lhs_result;
            binary.rhs = rhs_result;
            formalism::canonicalize(binary);
            return Data<formalism::GroundFunctionExpression>(context.get_or_create(binary, builder.get_buffer()).first.get_index());
        };

        switch (element->get_binary_operator())
        {
            case loki::BinaryOperatorEnum::PLUS:
                return build_binary_op(formalism::OpAdd {});
            case loki::BinaryOperatorEnum::MINUS:
                return build_binary_op(formalism::OpSub {});
            case loki::BinaryOperatorEnum::MUL:
                return build_binary_op(formalism::OpMul {});
            case loki::BinaryOperatorEnum::DIV:
                return build_binary_op(formalism::OpDiv {});
            default:
                throw std::runtime_error("Unexpected case");
        }
    }

    template<formalism::Context C>
    Data<formalism::GroundFunctionExpression> translate_grounded(loki::FunctionExpressionMultiOperator element, formalism::Builder& builder, C& context)
    {
        auto build_multi_op = [&](auto op_tag) -> Data<formalism::GroundFunctionExpression>
        {
            using Tag = std::decay_t<decltype(op_tag)>;

            auto& multi = builder.template get_ground_multi<Tag>();
            multi.clear();
            multi.args = translate_grounded(element->get_function_expressions(), builder, context);
            formalism::canonicalize(multi);
            return Data<formalism::GroundFunctionExpression>(context.get_or_create(multi, builder.get_buffer()).first.get_index());
        };

        switch (element->get_multi_operator())
        {
            case loki::MultiOperatorEnum::PLUS:
                return build_multi_op(formalism::OpAdd {});
            case loki::MultiOperatorEnum::MUL:
                return build_multi_op(formalism::OpMul {});
            default:
                throw std::runtime_error("Unexpected case");
        }
    }

    template<formalism::Context C>
    Data<formalism::GroundFunctionExpression> translate_grounded(loki::FunctionExpressionMinus element, formalism::Builder& builder, C& context)
    {
        auto& minus = builder.template get_ground_unary<formalism::OpSub>();
        minus.clear();
        minus.arg = translate_grounded(element->get_function_expression(), builder, context);
        formalism::canonicalize(minus);
        return Data<formalism::GroundFunctionExpression>(context.get_or_create(minus, builder.get_buffer()).first.get_index());
    }

    template<formalism::Context C>
    Data<formalism::GroundFunctionExpression> translate_grounded(loki::FunctionExpressionFunction element, formalism::Builder& builder, C& context)
    {
        const auto index_fterm_variant = translate_grounded(element->get_function(), builder, context);

        return std::visit(
            [&](auto&& arg) -> Data<formalism::GroundFunctionExpression>
            {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, Index<formalism::GroundFunctionTerm<formalism::StaticTag>>>)
                    return Data<formalism::GroundFunctionExpression>(arg);
                else if constexpr (std::is_same_v<T, Index<formalism::GroundFunctionTerm<formalism::FluentTag>>>)
                    return Data<formalism::GroundFunctionExpression>(arg);
                else if constexpr (std::is_same_v<T, Index<formalism::GroundFunctionTerm<formalism::AuxiliaryTag>>>)
                    throw std::runtime_error("Cannot create GroundFunctionExpression over auxiliary function term.");
                else
                    static_assert(dependent_false<T>::value, "Missing case for type");
            },
            index_fterm_variant);
    }

    template<formalism::Context C>
    Data<formalism::GroundFunctionExpression> translate_grounded(loki::FunctionExpression element, formalism::Builder& builder, C& context)
    {
        return std::visit([&](auto&& arg) { return translate_grounded(arg, builder, context); }, element->get_function_expression());
    }

    template<formalism::Context C>
    IndexGroundFunctionTermVariant translate_grounded(loki::Function element, formalism::Builder& builder, C& context)
    {
        auto index_function_variant = translate_common(element->get_function_skeleton(), builder, context);

        auto build_function_term = [&](auto fact_tag, auto function_index) -> IndexGroundFunctionTermVariant
        {
            using Tag = std::decay_t<decltype(fact_tag)>;

            auto& fterm = builder.template get_ground_fterm<Tag>();
            fterm.clear();
            fterm.function = function_index;
            fterm.objects = this->translate_grounded(element->get_terms(), builder, context);
            formalism::canonicalize(fterm);
            return context.get_or_create(fterm, builder.get_buffer()).first.get_index();
        };

        return std::visit(
            [&](auto&& arg) -> IndexGroundFunctionTermVariant
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Index<formalism::Function<formalism::StaticTag>>>)
                    return build_function_term(formalism::StaticTag {}, arg);
                else if constexpr (std::is_same_v<T, Index<formalism::Function<formalism::FluentTag>>>)
                    return build_function_term(formalism::FluentTag {}, arg);
                else if constexpr (std::is_same_v<T, Index<formalism::Function<formalism::AuxiliaryTag>>>)
                    return build_function_term(formalism::AuxiliaryTag {}, arg);
                else
                    static_assert(dependent_false<T>::value, "Missing case for type");
            },
            index_function_variant);
    }

    template<formalism::Context C>
    IndexGroundFunctionTermValueVariant translate_grounded(loki::FunctionValue element, formalism::Builder& builder, C& context)
    {
        auto index_fterm_variant = translate_grounded(element->get_function(), builder, context);

        auto build_fterm_value = [&](auto fact_tag, auto fterm_index) -> IndexGroundFunctionTermValueVariant
        {
            using Tag = std::decay_t<decltype(fact_tag)>;

            auto& fterm_value = builder.template get_ground_fterm_value<Tag>();
            fterm_value.clear();
            fterm_value.fterm = fterm_index;
            fterm_value.value = element->get_number();
            formalism::canonicalize(fterm_value);
            return context.get_or_create(fterm_value, builder.get_buffer()).first.get_index();
        };

        return std::visit(
            [&](auto&& arg) -> IndexGroundFunctionTermValueVariant
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Index<formalism::GroundFunctionTerm<formalism::StaticTag>>>)
                    return build_fterm_value(formalism::StaticTag {}, arg);
                else if constexpr (std::is_same_v<T, Index<formalism::GroundFunctionTerm<formalism::FluentTag>>>)
                    return build_fterm_value(formalism::FluentTag {}, arg);
                else if constexpr (std::is_same_v<T, Index<formalism::GroundFunctionTerm<formalism::AuxiliaryTag>>>)
                    return build_fterm_value(formalism::AuxiliaryTag {}, arg);
                else
                    static_assert(dependent_false<T>::value, "Missing case for type");
            },
            index_fterm_variant);
    }

    template<formalism::Context C>
    Data<formalism::BooleanOperator<Data<formalism::GroundFunctionExpression>>>
    translate_grounded(loki::ConditionNumericConstraint element, formalism::Builder& builder, C& context)
    {
        auto build_binary_op = [&](auto op_tag) -> Data<formalism::BooleanOperator<Data<formalism::GroundFunctionExpression>>>
        {
            using Tag = std::decay_t<decltype(op_tag)>;

            auto& binary = builder.template get_ground_binary<Tag>();
            binary.clear();
            auto lhs_result = translate_grounded(element->get_left_function_expression(), builder, context);
            auto rhs_result = translate_grounded(element->get_right_function_expression(), builder, context);
            binary.lhs = lhs_result;
            binary.rhs = rhs_result;
            formalism::canonicalize(binary);
            return Data<formalism::BooleanOperator<Data<formalism::GroundFunctionExpression>>>(
                context.get_or_create(binary, builder.get_buffer()).first.get_index());
        };

        switch (element->get_binary_comparator())
        {
            case loki::BinaryComparatorEnum::EQUAL:
                return build_binary_op(formalism::OpEq {});
            case loki::BinaryComparatorEnum::LESS_EQUAL:
                return build_binary_op(formalism::OpLe {});
            case loki::BinaryComparatorEnum::LESS:
                return build_binary_op(formalism::OpLt {});
            case loki::BinaryComparatorEnum::GREATER_EQUAL:
                return build_binary_op(formalism::OpGe {});
            case loki::BinaryComparatorEnum::GREATER:
                return build_binary_op(formalism::OpGt {});
            default:
                throw std::runtime_error("Unexpected case");
        }
    }

    template<formalism::Context C>
    Index<formalism::GroundConjunctiveCondition> translate_grounded(loki::Condition element, formalism::Builder& builder, C& context)
    {
        auto& conj_condition = builder.get_ground_conj_cond();
        conj_condition.clear();

        const auto func_insert_literal = [](IndexGroundLiteralVariant index_literal_variant,
                                            IndexList<formalism::GroundLiteral<formalism::StaticTag>>& static_literals,
                                            IndexList<formalism::GroundLiteral<formalism::FluentTag>>& fluent_literals,
                                            IndexList<formalism::GroundLiteral<formalism::DerivedTag>>& derived_literals)
        {
            std::visit(
                [&](auto&& arg)
                {
                    using T = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_same_v<T, Index<formalism::GroundLiteral<formalism::StaticTag>>>)
                        static_literals.push_back(arg);
                    else if constexpr (std::is_same_v<T, Index<formalism::GroundLiteral<formalism::FluentTag>>>)
                        fluent_literals.push_back(arg);
                    else if constexpr (std::is_same_v<T, Index<formalism::GroundLiteral<formalism::DerivedTag>>>)
                        derived_literals.push_back(arg);
                    else
                        static_assert(dependent_false<T>::value, "Missing case for type");
                },
                index_literal_variant);
        };

        if (const auto condition_and = std::get_if<loki::ConditionAnd>(&element->get_condition()))
        {
            for (const auto& part : (*condition_and)->get_conditions())
            {
                if (const auto condition_literal = std::get_if<loki::ConditionLiteral>(&part->get_condition()))
                {
                    const auto index_literal_variant = translate_grounded((*condition_literal)->get_literal(), builder, context);

                    func_insert_literal(index_literal_variant, conj_condition.static_literals, conj_condition.fluent_literals, conj_condition.derived_literals);
                }
                else if (const auto condition_numeric_constraint = std::get_if<loki::ConditionNumericConstraint>(&part->get_condition()))
                {
                    const auto numeric_constraint = translate_grounded((*condition_numeric_constraint), builder, context);

                    conj_condition.numeric_constraints.push_back(numeric_constraint);
                }
                else
                {
                    // std::cout << std::visit([](auto&& arg) { return arg.str(); }, *part) << std::endl;

                    throw std::logic_error("Expected literal in conjunctive condition.");
                }
            }

            formalism::canonicalize(conj_condition);
            return context.get_or_create(conj_condition, builder.get_buffer()).first.get_index();
        }
        else if (const auto condition_literal = std::get_if<loki::ConditionLiteral>(&element->get_condition()))
        {
            const auto index_literal_variant = translate_grounded((*condition_literal)->get_literal(), builder, context);

            func_insert_literal(index_literal_variant, conj_condition.static_literals, conj_condition.fluent_literals, conj_condition.derived_literals);

            formalism::canonicalize(conj_condition);
            return context.get_or_create(conj_condition, builder.get_buffer()).first.get_index();
        }
        else if (const auto condition_numeric_constraint = std::get_if<loki::ConditionNumericConstraint>(&element->get_condition()))
        {
            const auto numeric_constraint = translate_grounded((*condition_numeric_constraint), builder, context);

            conj_condition.numeric_constraints.push_back(numeric_constraint);

            formalism::canonicalize(conj_condition);
            return context.get_or_create(conj_condition, builder.get_buffer()).first.get_index();
        }

        // std::cout << std::visit([](auto&& arg) { return arg.str(); }, *condition_ptr) << std::endl;

        throw std::logic_error("Expected conjunctive condition.");
    }

    template<formalism::Context C>
    Index<formalism::Metric> translate_grounded(loki::OptimizationMetric element, formalism::Builder& builder, C& context)
    {
        auto& metric = builder.get_metric();
        metric.clear();

        metric.fexpr = translate_grounded(element->get_function_expression(), builder, context);
        switch (element->get_optimization_metric())
        {
            case loki::OptimizationMetricEnum::MINIMIZE:
            {
                metric.objective = formalism::Minimize {};
                break;
            }
            case loki::OptimizationMetricEnum::MAXIMIZE:
            {
                metric.objective = formalism::Maximize {};
                break;
            }
            default:
                throw std::runtime_error("Unexpected case.");
        }

        formalism::canonicalize(metric);
        return context.get_or_create(metric, builder.get_buffer()).first.get_index();
    }

public:
    LokiToTyrTranslator() = default;

    DomainPtr translate(const loki::Domain& domain, formalism::Builder& builder, formalism::RepositoryPtr context);

    LiftedTaskPtr translate(const loki::Problem& problem, formalism::Builder& builder, DomainPtr domain, formalism::RepositoryPtr domain_context);
};

}

#endif