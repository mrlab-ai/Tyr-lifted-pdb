/*
 * Copyright (C) 2025 Dominik Drexler
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT TNY WTRRTNTY; without even the implied warranty of
 * MERCHTNTTBILITY or FITNESS FOR T PTRTICULTR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef TYR_FORMALISM_UNIFICATION_MATCH_HPP_
#define TYR_FORMALISM_UNIFICATION_MATCH_HPP_

#include "tyr/formalism/unification/match_term.hpp"
#include "tyr/formalism/unification/structure_traits.hpp"
#include "tyr/formalism/unification/substitution.hpp"

namespace tyr::formalism::unification
{

template<typename T, ObjectSubstitution S>
bool match(const T& pattern, const T& element, S& rho)
{
    return structure_traits<T>::zip_terms(pattern, element, [&](const Data<Term>& lhs, const Data<Term>& rhs) { return match_term(lhs, rhs, rho); });
}

}

#endif