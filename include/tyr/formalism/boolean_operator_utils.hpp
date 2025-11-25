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

#ifndef TYR_FORMALISM_BOOLEAN_OPERATOR_UTILS_HPP_
#define TYR_FORMALISM_BOOLEAN_OPERATOR_UTILS_HPP_

#include "tyr/common/closed_interval.hpp"
#include "tyr/common/declarations.hpp"
#include "tyr/formalism/declarations.hpp"

namespace tyr::formalism
{
template<IsBooleanOp O, IsFloatingPoint A>
inline bool evaluate_existential(const ClosedInterval<A>& lhs, const ClosedInterval<A>& rhs)
{
    static_assert(dependent_false<O>::value, "evaluate_existential is not defined for this operator type.");
};
}

#endif
