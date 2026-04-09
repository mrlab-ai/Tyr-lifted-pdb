/*
 * Copyright (C) 2025-2026 Dominik Drexler
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

#ifndef TYR_FORMALISM_UNIFICATION_MATCH_TERM_HPP_
#define TYR_FORMALISM_UNIFICATION_MATCH_TERM_HPP_

#include "tyr/common/comparators.hpp"
#include "tyr/formalism/term_data.hpp"
#include "tyr/formalism/unification/match_policy.hpp"
#include "tyr/formalism/unification/match_state.hpp"

namespace tyr::formalism::unification
{

template<typename Policy = DefaultMatchPolicy>
bool match_term(const Data<Term>& pattern, const Data<Term>& element, TermMatchState& state, const Policy& policy = {})
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
                        if constexpr (std::is_same_v<Rhs, ParameterIndex>)
                        {
                            return policy.match_parameter_parameter(lhs, rhs, state);
                        }
                        else if constexpr (std::is_same_v<Rhs, Index<Object>>)
                        {
                            return policy.match_parameter_object(lhs, Data<Term>(rhs), state);
                        }
                        else
                        {
                            static_assert(dependent_false<Rhs>::value, "Missing case");
                        }
                    }
                    else if constexpr (std::is_same_v<Lhs, Index<Object>>)
                    {
                        if constexpr (std::is_same_v<Rhs, ParameterIndex>)
                        {
                            return policy.match_object_parameter(Data<Term>(lhs), rhs, state);
                        }
                        else if constexpr (std::is_same_v<Rhs, Index<Object>>)
                        {
                            return lhs == rhs;
                        }
                        else
                        {
                            static_assert(dependent_false<Rhs>::value, "Missing case");
                        }
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

}  // namespace tyr::formalism::unification

#endif