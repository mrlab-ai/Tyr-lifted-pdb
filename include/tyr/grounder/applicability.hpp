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

#ifndef TYR_GROUNDER_APPLICABILITY_HPP_
#define TYR_GROUNDER_APPLICABILITY_HPP_

#include "tyr/formalism/formalism.hpp"
#include "tyr/grounder/fact_set.hpp"

namespace tyr::grounder
{
template<formalism::IsStaticOrFluentTag T, formalism::IsContext C>
bool literal_holds(View<Index<formalism::GroundLiteral<T>>, C> literal, const PredicateFactSet<T, C>& predicate_fact_sets) noexcept
{
    return predicate_fact_sets.contains(literal.get_atom().get_index()) != literal.get_polarity();
}

template<formalism::IsStaticOrFluentTag T, formalism::IsContext C>
bool literals_hold(View<IndexList<formalism::GroundLiteral<T>>, C> literals, const PredicateFactSet<T, C>& predicate_fact_sets) noexcept
{
    for (const auto literal : literals)
    {
        if (!literal_holds(literal, predicate_fact_sets))
            return false;
    }
    return true;
}

template<formalism::IsContext C>
bool nullary_conditions_hold(View<Index<formalism::ConjunctiveCondition>, C> condition, const FactSets<C>& fact_sets) noexcept
{
    return literals_hold(condition.template get_nullary_literals<formalism::StaticTag>(), fact_sets.static_sets.predicate)
           && literals_hold(condition.template get_nullary_literals<formalism::FluentTag>(), fact_sets.fluent_sets.predicate);
}
}

#endif
