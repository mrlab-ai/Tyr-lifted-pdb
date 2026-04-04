/*
 * Copyright (C) 2025-2026 Dominik Drexler
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

#ifndef TYR_FORMALISM_UNIFICATION_APPLY_SUBSTITUTION_HPP_
#define TYR_FORMALISM_UNIFICATION_APPLY_SUBSTITUTION_HPP_

#include "tyr/formalism/unification/structure_traits.hpp"
#include "tyr/formalism/unification/substitution.hpp"
#include "tyr/formalism/unification/term_operations.hpp"

namespace tyr::formalism::unification
{

template<ObjectSubstitution S>
Data<Term> apply_substitution(const Data<Term>& term, const S& rho)
{
    if (!is_parameter(term))
        return term;

    const auto p = get_parameter(term);
    if (!rho.contains_parameter(p) || rho.is_unbound(p))
        return term;

    return Data<Term>(*rho[p]);
}

template<typename T, ObjectSubstitution S>
T apply_substitution(const T& value, const S& rho)
{
    return structure_traits<T>::transform_terms(value, [&](const Data<Term>& term) { return apply_substitution(term, rho); });
}

}  // namespace tyr::formalism::unification

#endif