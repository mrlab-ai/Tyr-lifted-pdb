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

#include "tyr/formalism/planning/variable_dependency_graph.hpp"

#include "tyr/common/comparators.hpp"
#include "tyr/formalism/planning/expression_arity.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/views.hpp"

namespace tyr::formalism::planning
{
template<FactKind T>
static void insert_dependencies(View<Index<Literal<T>>, Repository> literal, uint_t k, VariableDependencyGraph::Dependencies& dependencies)
{
    const auto parameters_set = collect_parameters(literal);
    auto parameters = std::vector<ParameterIndex>(parameters_set.begin(), parameters_set.end());
    std::sort(parameters.begin(), parameters.end());

    for (uint_t i = 0; i < parameters.size(); ++i)
    {
        const auto pi = uint_t(parameters[i]);

        for (uint_t j = i + 1; j < parameters.size(); ++j)
        {
            const auto pj = uint_t(parameters[j]);

            const auto i1 = VariableDependencyGraph::get_index(pi, pj, k);
            const auto i2 = VariableDependencyGraph::get_index(pj, pi, k);

            if (literal.get_polarity())
            {
                dependencies.template get<T, PositiveTag>().set(i1);
                dependencies.template get<T, PositiveTag>().set(i2);
            }
            else
            {
                dependencies.template get<T, NegativeTag>().set(i1);
                dependencies.template get<T, NegativeTag>().set(i2);
            }
        }
    }
}

static void insert_dependencies(View<Data<BooleanOperator<Data<FunctionExpression>>>, Repository> numeric_constraint,
                                uint_t k,
                                VariableDependencyGraph::Dependencies& dependencies)
{
    const auto parameters_set = collect_parameters(numeric_constraint);
    auto parameters = std::vector<ParameterIndex>(parameters_set.begin(), parameters_set.end());
    std::sort(parameters.begin(), parameters.end());

    for (uint_t i = 0; i < parameters.size(); ++i)
    {
        const auto pi = uint_t(parameters[i]);

        for (uint_t j = i + 1; j < parameters.size(); ++j)
        {
            const auto pj = uint_t(parameters[j]);

            const auto i1 = VariableDependencyGraph::get_index(pi, pj, k);
            const auto i2 = VariableDependencyGraph::get_index(pj, pi, k);

            // TODO: think about static/fluent separation
            dependencies.template get<FluentTag, PositiveTag>().set(i1);
            dependencies.template get<FluentTag, PositiveTag>().set(i2);
        }
    }
}

static void insert_dependencies(View<Index<ConjunctiveCondition>, Repository> element, uint_t k, VariableDependencyGraph::Dependencies& dependencies)
{
    for (const auto literal : element.get_literals<StaticTag>())
        insert_dependencies(literal, k, dependencies);

    for (const auto literal : element.get_literals<FluentTag>())
        insert_dependencies(literal, k, dependencies);

    for (const auto numeric_constraint : element.get_numeric_constraints())
        insert_dependencies(numeric_constraint, k, dependencies);
}

static void insert_dependencies(View<Index<ConjunctiveEffect>, Repository> element, uint_t k, VariableDependencyGraph::Dependencies& dependencies)
{
    for (const auto literal : element.get_literals())
        insert_dependencies(literal, k, dependencies);

    // TODO: numeric stuff
    // for (const auto numeric_effect : element.get_numeric_effects())
    //    insert_dependencies(numeric_effect, k, dependencies);

    // if (element.get_auxiliary_numeric_effect().has_value())
    //     insert_dependencies(element.get_auxiliary_numeric_effect().value(), k, dependencies);
}

static void insert_dependencies(View<Index<ConditionalEffect>, Repository> element, uint_t k, VariableDependencyGraph::Dependencies& dependencies)
{
    insert_dependencies(element.get_condition(), k, dependencies);
    insert_dependencies(element.get_effect(), k, dependencies);
}

VariableDependencyGraph::VariableDependencyGraph(View<Index<Action>, Repository> element) :
    m_k(element.get_arity()),
    m_static_positive_dependencies(m_k * m_k),
    m_static_negative_dependencies(m_k * m_k),
    m_fluent_positive_dependencies(m_k * m_k),
    m_fluent_negative_dependencies(m_k * m_k)
{
    auto dependencies = VariableDependencyGraph::Dependencies { m_static_positive_dependencies,
                                                                m_static_negative_dependencies,
                                                                m_fluent_positive_dependencies,
                                                                m_fluent_negative_dependencies };

    insert_dependencies(element.get_condition(), m_k, dependencies);

    for (const auto cond_effect : element.get_effects())
        insert_dependencies(cond_effect, m_k, dependencies);
}

}
