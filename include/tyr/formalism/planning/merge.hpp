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

#ifndef TYR_FORMALISM_PLANNING_MERGE_PLANNING_HPP_
#define TYR_FORMALISM_PLANNING_MERGE_PLANNING_HPP_

#include "tyr/common/tuple.hpp"
#include "tyr/formalism/planning/builder.hpp"
#include "tyr/formalism/planning/canonicalization.hpp"
#include "tyr/formalism/planning/declarations.hpp"
#include "tyr/formalism/planning/fdr_context.hpp"
#include "tyr/formalism/planning/views.hpp"

namespace tyr::formalism::planning
{

class MergeCache
{
private:
    template<typename T_SRC, typename T_DST = T_SRC>
    struct MapEntryType
    {
        using value_type = std::pair<T_SRC, T_DST>;
        using container_type = UnorderedMap<Index<T_SRC>, Index<T_DST>>;

        container_type container;
    };

    using MergeStorage = std::tuple<MapEntryType<Variable>,
                                    MapEntryType<Object>,
                                    MapEntryType<Binding>,
                                    MapEntryType<Predicate<StaticTag>>,
                                    MapEntryType<Predicate<FluentTag>>,
                                    MapEntryType<Predicate<DerivedTag>>,
                                    MapEntryType<Predicate<FluentTag>, Predicate<DerivedTag>>,
                                    MapEntryType<Predicate<DerivedTag>, Predicate<FluentTag>>,
                                    MapEntryType<Atom<StaticTag>>,
                                    MapEntryType<Atom<FluentTag>>,
                                    MapEntryType<Atom<DerivedTag>>,
                                    MapEntryType<Atom<FluentTag>, Atom<DerivedTag>>,
                                    MapEntryType<Atom<DerivedTag>, Atom<FluentTag>>,
                                    MapEntryType<GroundAtom<StaticTag>>,
                                    MapEntryType<GroundAtom<FluentTag>>,
                                    MapEntryType<GroundAtom<DerivedTag>>,
                                    MapEntryType<GroundAtom<FluentTag>, GroundAtom<DerivedTag>>,
                                    MapEntryType<GroundAtom<DerivedTag>, GroundAtom<FluentTag>>,
                                    MapEntryType<Literal<StaticTag>>,
                                    MapEntryType<Literal<FluentTag>>,
                                    MapEntryType<Literal<DerivedTag>>,
                                    MapEntryType<Literal<FluentTag>, Literal<DerivedTag>>,
                                    MapEntryType<Literal<DerivedTag>, Literal<FluentTag>>,
                                    MapEntryType<GroundLiteral<StaticTag>>,
                                    MapEntryType<GroundLiteral<FluentTag>>,
                                    MapEntryType<GroundLiteral<DerivedTag>>,
                                    MapEntryType<GroundLiteral<FluentTag>, GroundLiteral<DerivedTag>>,
                                    MapEntryType<GroundLiteral<DerivedTag>, GroundLiteral<FluentTag>>,
                                    MapEntryType<Function<StaticTag>>,
                                    MapEntryType<Function<FluentTag>>,
                                    MapEntryType<Function<AuxiliaryTag>>,
                                    MapEntryType<FunctionTerm<StaticTag>>,
                                    MapEntryType<FunctionTerm<FluentTag>>,
                                    MapEntryType<FunctionTerm<AuxiliaryTag>>,
                                    MapEntryType<GroundFunctionTerm<StaticTag>>,
                                    MapEntryType<GroundFunctionTerm<FluentTag>>,
                                    MapEntryType<GroundFunctionTerm<AuxiliaryTag>>,
                                    MapEntryType<GroundFunctionTermValue<StaticTag>>,
                                    MapEntryType<GroundFunctionTermValue<FluentTag>>,
                                    MapEntryType<GroundFunctionTermValue<AuxiliaryTag>>,
                                    MapEntryType<UnaryOperator<OpSub, Data<FunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpAdd, Data<FunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpSub, Data<FunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpMul, Data<FunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpDiv, Data<FunctionExpression>>>,
                                    MapEntryType<MultiOperator<OpAdd, Data<FunctionExpression>>>,
                                    MapEntryType<MultiOperator<OpMul, Data<FunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpEq, Data<FunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpNe, Data<FunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpLe, Data<FunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpLt, Data<FunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpGe, Data<FunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpGt, Data<FunctionExpression>>>,
                                    MapEntryType<UnaryOperator<OpSub, Data<GroundFunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpAdd, Data<GroundFunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpSub, Data<GroundFunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpMul, Data<GroundFunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpDiv, Data<GroundFunctionExpression>>>,
                                    MapEntryType<MultiOperator<OpAdd, Data<GroundFunctionExpression>>>,
                                    MapEntryType<MultiOperator<OpMul, Data<GroundFunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpEq, Data<GroundFunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpNe, Data<GroundFunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpLe, Data<GroundFunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpLt, Data<GroundFunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpGe, Data<GroundFunctionExpression>>>,
                                    MapEntryType<BinaryOperator<OpGt, Data<GroundFunctionExpression>>>,
                                    MapEntryType<NumericEffect<OpAssign, FluentTag>>,
                                    MapEntryType<NumericEffect<OpIncrease, FluentTag>>,
                                    MapEntryType<NumericEffect<OpDecrease, FluentTag>>,
                                    MapEntryType<NumericEffect<OpScaleUp, FluentTag>>,
                                    MapEntryType<NumericEffect<OpScaleDown, FluentTag>>,
                                    MapEntryType<NumericEffect<OpIncrease, AuxiliaryTag>>,
                                    MapEntryType<GroundNumericEffect<OpAssign, FluentTag>>,
                                    MapEntryType<GroundNumericEffect<OpIncrease, FluentTag>>,
                                    MapEntryType<GroundNumericEffect<OpDecrease, FluentTag>>,
                                    MapEntryType<GroundNumericEffect<OpScaleUp, FluentTag>>,
                                    MapEntryType<GroundNumericEffect<OpScaleDown, FluentTag>>,
                                    MapEntryType<GroundNumericEffect<OpIncrease, AuxiliaryTag>>,
                                    MapEntryType<FDRConjunctiveCondition>,
                                    MapEntryType<GroundFDRConjunctiveCondition>,
                                    MapEntryType<ConditionalEffect>,
                                    MapEntryType<GroundConditionalEffect>,
                                    MapEntryType<ConjunctiveEffect>,
                                    MapEntryType<GroundConjunctiveEffect>,
                                    MapEntryType<Action>,
                                    MapEntryType<GroundAction>,
                                    MapEntryType<Axiom>,
                                    MapEntryType<GroundAxiom>,
                                    MapEntryType<Metric>,
                                    MapEntryType<Domain>,
                                    MapEntryType<Task>,
                                    MapEntryType<FDRVariable<FluentTag>>>;

    MergeStorage m_maps;

public:
    MergeCache() = default;

    template<typename T_SRC, typename T_DST = T_SRC>
    auto& get() noexcept
    {
        using Key = std::pair<T_SRC, T_DST>;
        return get_container<Key>(m_maps);
    }
    template<typename T_SRC, typename T_DST = T_SRC>
    const auto& get() const noexcept
    {
        using Key = std::pair<T_SRC, T_DST>;
        return get_container<Key>(m_maps);
    }

    void clear() noexcept
    {
        std::apply([](auto&... slots) { (slots.container.clear(), ...); }, m_maps);
    }
};

template<Context C>
struct MergeContext
{
    Builder& builder;
    C& destination;
    MergeCache& cache;
};

/**
 * Forward declarations
 */

template<FactKind T_SRC, Context C_SRC, Context C_DST, FactKind T_DST>
auto merge(View<Index<Predicate<T_SRC>>, C_SRC> element, MergeContext<C_DST>& context);

template<FactKind T_SRC, Context C_SRC, Context C_DST, FactKind T_DST>
auto merge(View<Index<Atom<T_SRC>>, C_SRC> element, MergeContext<C_DST>& context);

template<FactKind T_SRC, Context C_SRC, Context C_DST, FactKind T_DST>
auto merge(View<Index<GroundAtom<T_SRC>>, C_SRC> element, MergeContext<C_DST>& context);

template<FactKind T_SRC, Context C_SRC, Context C_DST, FactKind T_DST>
auto merge(View<Index<Literal<T_SRC>>, C_SRC> element, MergeContext<C_DST>& context);

template<FactKind T_SRC, Context C_SRC, Context C_DST, FactKind T_DST>
auto merge(View<Index<GroundLiteral<T_SRC>>, C_SRC> element, MergeContext<C_DST>& context);

template<NumericEffectOpKind O, FactKind T, Context C_SRC, Context C_DST>
auto merge(View<Index<NumericEffect<O, T>>, C_SRC> element, MergeContext<C_DST>& context);

template<FactKind T, Context C_SRC, Context C_DST>
auto merge(View<Data<NumericEffectOperator<T>>, C_SRC> element, MergeContext<C_DST>& context);

template<NumericEffectOpKind O, FactKind T, Context C_SRC, Context C_DST>
auto merge(View<Index<GroundNumericEffect<O, T>>, C_SRC> element, MergeContext<C_DST>& context);

template<FactKind T, Context C_SRC, Context C_DST>
auto merge(View<Data<GroundNumericEffectOperator<T>>, C_SRC> element, MergeContext<C_DST>& context);

template<Context C_SRC, Context C_DST>
auto merge(View<Index<FDRVariable<FluentTag>>, C_SRC> element, MergeContext<C_DST>& context);

template<Context C_SRC, Context C_DST>
auto merge(View<Data<FDRFact<FluentTag>>, C_SRC> element, MergeContext<C_DST>& context);

template<Context C_SRC, Context C_DST>
auto merge(View<Index<FDRConjunctiveCondition>, C_SRC> element, MergeContext<C_DST>& context);

template<Context C_SRC, Context C_DST>
auto merge(View<Index<Axiom>, C_SRC> element, MergeContext<C_DST>& context);

template<Context C_SRC, Context C_DST>
auto merge(View<Index<Metric>, C_SRC> element, MergeContext<C_DST>& context);

/**
 * Implementations
 */

template<FactKind T_SRC, Context C_SRC, Context C_DST, FactKind T_DST>
auto merge(View<Index<Predicate<T_SRC>>, C_SRC> element, MergeContext<C_DST>& context)
{
    return with_cache<Predicate<T_SRC>, Predicate<T_DST>>(element,
                                                          context.cache,
                                                          [&]()
                                                          {
                                                              auto predicate_ptr = context.builder.template get_builder<Predicate<T_DST>>();
                                                              auto& predicate = *predicate_ptr;
                                                              predicate.clear();

                                                              predicate.name = element.get_name();
                                                              predicate.arity = element.get_arity();

                                                              canonicalize(predicate);
                                                              return context.destination.get_or_create(predicate, context.builder.get_buffer());
                                                          });
}

template<FactKind T_SRC, Context C_SRC, Context C_DST, FactKind T_DST>
auto merge(View<Index<Atom<T_SRC>>, C_SRC> element, MergeContext<C_DST>& context)
{
    return with_cache<Atom<T_SRC>, Atom<T_DST>>(element,
                                                context.cache,
                                                [&]()
                                                {
                                                    auto atom_ptr = context.builder.template get_builder<Atom<T_DST>>();
                                                    auto& atom = *atom_ptr;
                                                    atom.clear();

                                                    atom.predicate = merge<T_SRC, C_SRC, C_DST, T_DST>(element.get_predicate(), context).first;
                                                    for (const auto term : element.get_terms())
                                                        atom.terms.push_back(merge(term, context));

                                                    canonicalize(atom);
                                                    return context.destination.get_or_create(atom, context.builder.get_buffer());
                                                });
}

template<FactKind T_SRC, Context C_SRC, Context C_DST, FactKind T_DST>
auto merge(View<Index<GroundAtom<T_SRC>>, C_SRC> element, MergeContext<C_DST>& context)
{
    return with_cache<GroundAtom<T_SRC>, GroundAtom<T_DST>>(element,
                                                            context.cache,
                                                            [&]()
                                                            {
                                                                auto atom_ptr = context.builder.template get_builder<GroundAtom<T_DST>>();
                                                                auto& atom = *atom_ptr;
                                                                atom.clear();

                                                                atom.predicate = merge<T_SRC, C_SRC, C_DST, T_DST>(element.get_predicate(), context).first;
                                                                atom.binding = merge(element.get_binding(), context).first;

                                                                canonicalize(atom);
                                                                return context.destination.get_or_create(atom, context.builder.get_buffer());
                                                            });
}

template<FactKind T_SRC, Context C_SRC, Context C_DST, FactKind T_DST>
auto merge(View<Index<Literal<T_SRC>>, C_SRC> element, MergeContext<C_DST>& context)
{
    return with_cache<Literal<T_SRC>, Literal<T_DST>>(element,
                                                      context.cache,
                                                      [&]()
                                                      {
                                                          auto literal_ptr = context.builder.template get_builder<Literal<T_DST>>();
                                                          auto& literal = *literal_ptr;
                                                          literal.clear();

                                                          literal.polarity = element.get_polarity();
                                                          literal.atom = merge<T_SRC, C_SRC, C_DST, T_DST>(element.get_atom(), context).first;

                                                          canonicalize(literal);
                                                          return context.destination.get_or_create(literal, context.builder.get_buffer());
                                                      });
}

template<FactKind T_SRC, Context C_SRC, Context C_DST, FactKind T_DST>
auto merge(View<Index<GroundLiteral<T_SRC>>, C_SRC> element, MergeContext<C_DST>& context)
{
    return with_cache<GroundLiteral<T_SRC>, GroundLiteral<T_DST>>(element,
                                                                  context.cache,
                                                                  [&]()
                                                                  {
                                                                      auto literal_ptr = context.builder.template get_builder<GroundLiteral<T_DST>>();
                                                                      auto& literal = *literal_ptr;
                                                                      literal.clear();

                                                                      literal.polarity = element.get_polarity();
                                                                      literal.atom = merge<T_SRC, C_SRC, C_DST, T_DST>(element.get_atom(), context).first;

                                                                      canonicalize(literal);
                                                                      return context.destination.get_or_create(literal, context.builder.get_buffer());
                                                                  });
}

template<NumericEffectOpKind O, FactKind T, Context C_SRC, Context C_DST>
auto merge(View<Index<NumericEffect<O, T>>, C_SRC> element, MergeContext<C_DST>& context)
{
    return with_cache<NumericEffect<O, T>, NumericEffect<O, T>>(element,
                                                                context.cache,
                                                                [&]()
                                                                {
                                                                    auto numeric_effect_ptr = context.builder.template get_builder<NumericEffect<O, T>>();
                                                                    auto& numeric_effect = *numeric_effect_ptr;
                                                                    numeric_effect.clear();

                                                                    numeric_effect.fterm = merge(element.get_fterm(), context).first;
                                                                    numeric_effect.fexpr = merge(element.get_fexpr(), context).first;

                                                                    canonicalize(numeric_effect);
                                                                    return context.destination.get_or_create(numeric_effect, context.builder.get_buffer());
                                                                });
}

template<FactKind T, Context C_SRC, Context C_DST>
auto merge(View<Data<NumericEffectOperator<T>>, C_SRC> element, MergeContext<C_DST>& context)
{
    return visit([&](auto&& arg) { return Data<NumericEffectOperator<T>>(merge(arg, context).get_index()); }, element.get_variant());
}

template<NumericEffectOpKind O, FactKind T, Context C_SRC, Context C_DST>
auto merge(View<Index<GroundNumericEffect<O, T>>, C_SRC> element, MergeContext<C_DST>& context)
{
    return with_cache<GroundNumericEffect<O, T>, GroundNumericEffect<O, T>>(
        element,
        context.cache,
        [&]()
        {
            auto numeric_effect_ptr = context.builder.template get_builder<GroundNumericEffect<O, T>>();
            auto& numeric_effect = *numeric_effect_ptr;
            numeric_effect.clear();

            numeric_effect.fterm = merge(element.get_fterm(), context).first;
            numeric_effect.fexpr = merge(element.get_fexpr(), context);

            canonicalize(numeric_effect);
            return context.destination.get_or_create(numeric_effect, context.builder.get_buffer());
        });
}

template<FactKind T, Context C_SRC, Context C_DST>
auto merge(View<Data<GroundNumericEffectOperator<T>>, C_SRC> element, MergeContext<C_DST>& context)
{
    return visit([&](auto&& arg) { return Data<GroundNumericEffectOperator<T>>(merge(arg, context).first); }, element.get_variant());
}

template<Context C_SRC, Context C_DST>
auto merge(View<Index<FDRVariable<FluentTag>>, C_SRC> element, MergeContext<C_DST>& context)
{
    return with_cache<FDRVariable<FluentTag>, FDRVariable<FluentTag>>(element,
                                                                      context.cache,
                                                                      [&]()
                                                                      {
                                                                          auto variable_ptr = context.builder.template get_builder<FDRVariable<FluentTag>>();
                                                                          auto& variable = *variable_ptr;
                                                                          variable.clear();

                                                                          variable.domain_size = element.get_domain_size();
                                                                          for (const auto atom : element.get_atoms())
                                                                              variable.atoms.push_back(merge(atom, context).first);

                                                                          canonicalize(variable);
                                                                          return context.destination.get_or_create(variable, context.builder.get_buffer());
                                                                      });
}

template<Context C_SRC, Context C_DST>
auto merge(View<Data<FDRFact<FluentTag>>, C_SRC> element, MergeContext<C_DST>& context)
{
    return Data<FDRFact<FluentTag>>(merge(element.get_variable(), context).first, element.get_value());
}

template<Context C_SRC, Context C_DST>
auto merge(View<Index<FDRConjunctiveCondition>, C_SRC> element, MergeContext<C_DST>& context)
{
    return with_cache<FDRConjunctiveCondition, FDRConjunctiveCondition>(element,
                                                                        context.cache,
                                                                        [&]()
                                                                        {
                                                                            auto conj_cond_ptr =
                                                                                context.builder.template get_builder<FDRConjunctiveCondition>();
                                                                            auto& conj_cond = *conj_cond_ptr;
                                                                            conj_cond.clear();

                                                                            for (const auto variable : element.get_variables())
                                                                                conj_cond.variables.push_back(merge(variable, context).first);
                                                                            for (const auto literal : element.template get_literals<StaticTag>())
                                                                                conj_cond.static_literals.push_back(merge(literal, context).first);
                                                                            for (const auto literal : element.template get_literals<FluentTag>())
                                                                                conj_cond.fluent_literals.push_back(merge(literal, context).first);
                                                                            for (const auto literal : element.template get_literals<DerivedTag>())
                                                                                conj_cond.derived_literals.push_back(merge(literal, context).first);
                                                                            for (const auto numeric_constraint : element.get_numeric_constraints())
                                                                                conj_cond.numeric_constraints.push_back(merge(numeric_constraint, context));

                                                                            canonicalize(conj_cond);
                                                                            return context.destination.get_or_create(conj_cond, context.builder.get_buffer());
                                                                        });
}

template<Context C_SRC, Context C_DST>
auto merge(View<Index<Axiom>, C_SRC> element, MergeContext<C_DST>& context)
{
    return with_cache<Axiom, Axiom>(element,
                                    context.cache,
                                    [&]()
                                    {
                                        auto axiom_ptr = context.builder.template get_builder<Axiom>();
                                        auto& axiom = *axiom_ptr;
                                        axiom.clear();

                                        for (const auto variable : element.get_variables())
                                            axiom.variables.push_back(merge(variable, context).first);
                                        axiom.body = merge(element.get_body(), context).first;
                                        axiom.head = merge(element.get_head(), context).first;

                                        canonicalize(axiom);
                                        return context.destination.get_or_create(axiom, context.builder.get_buffer());
                                    });
}

template<Context C_SRC, Context C_DST>
auto merge(View<Index<Metric>, C_SRC> element, MergeContext<C_DST>& context)
{
    return with_cache<Metric, Metric>(element,
                                      context.cache,
                                      [&]()
                                      {
                                          auto metric_ptr = context.builder.template get_builder<Metric>();
                                          auto& metric = *metric_ptr;
                                          metric.clear();

                                          metric.objective = element.get_objective();
                                          metric.fexpr = merge(element.get_fexpr(), context);

                                          canonicalize(metric);
                                          return context.destination.get_or_create(metric, context.builder.get_buffer());
                                      });
}

}

#endif