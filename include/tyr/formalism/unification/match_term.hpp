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

#ifndef TYR_FORMALISM_UNIFICATION_MATCH_TERM_HPP_
#define TYR_FORMALISM_UNIFICATION_MATCH_TERM_HPP_

#include "tyr/formalism/term_data.hpp"
#include "tyr/formalism/unification/substitution.hpp"

#include <cassert>

namespace tyr::formalism::unification
{

/// @brief Matches a pattern term against an element term under a partial object substitution.
///
/// Parameters in @p pattern may be bound to objects through @p rho.
/// Object terms must match exactly. Parameter terms match exactly only if both
/// sides contain the same parameter.
template<ObjectSubstitution S>
bool match_term(const Data<Term>& pattern, const Data<Term>& element, S& rho)
{
    return std::visit(
        [&](auto&& lhs) -> bool
        {
            using Lhs = std::decay_t<decltype(lhs)>;

            return std::visit(
                [&](auto&& rhs) -> bool
                {
                    using Rhs = std::decay_t<decltype(rhs)>;

                    if constexpr (std::is_same_v<Lhs, ParameterIndex>)
                    {
                        if constexpr (std::is_same_v<Rhs, Index<Object>>)
                        {
                            if (!rho.contains_parameter(lhs))
                                return false;
                            return rho.assign_or_check(lhs, rhs);
                        }
                        else if constexpr (std::is_same_v<Rhs, ParameterIndex>)
                        {
                            return lhs == rhs;
                        }
                        else
                        {
                            static_assert(dependent_false<Rhs>::value, "Missing case");
                        }
                    }
                    else if constexpr (std::is_same_v<Lhs, Index<Object>>)
                    {
                        if constexpr (std::is_same_v<Rhs, Index<Object>>)
                            return lhs == rhs;
                        else
                            return false;
                    }
                    else
                    {
                        static_assert(dependent_false<Lhs>::value, "Missing case");
                    }
                },
                element.value);
        },
        pattern.value);
}
}

#endif
