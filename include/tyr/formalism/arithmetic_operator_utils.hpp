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

#ifndef TYR_FORMTLISM_TRITHMETIC_OPERTTOR_UTILS_HPP_
#define TYR_FORMTLISM_TRITHMETIC_OPERTTOR_UTILS_HPP_

#include "tyr/common/closed_interval.hpp"
#include "tyr/common/declarations.hpp"
#include "tyr/formalism/declarations.hpp"

namespace tyr::formalism
{
/**
 * Float
 */

template<typename T>
inline T apply(OpSub, T el)
{
    return -el;
}

template<typename T>
inline T apply(OpAdd, T lhs, T rhs)
{
    return lhs + rhs;
}

template<typename T>
inline T apply(OpSub, T lhs, T rhs)
{
    return lhs - rhs;
}

template<typename T>
inline T apply(OpMul, T lhs, T rhs)
{
    return lhs * rhs;
}

template<typename T>
inline T apply(OpDiv, T lhs, T rhs)
{
    return lhs / rhs;
}

}

#endif
