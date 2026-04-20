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

#ifndef TYR_COMMON_LESS_HPP_
#define TYR_COMMON_LESS_HPP_

#include "tyr/common/declarations.hpp"

#include <array>
#include <cmath>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace tyr
{

/// @brief `Less` is our custom less comparator, like std::less.
///
/// Forwards to std::less by default.
/// Specializations can be injected into the namespace.
template<typename T>
struct Less
{
    bool operator()(const T& lhs, const T& rhs) const noexcept { return std::less<T> {}(lhs, rhs); }
};

template<Identifiable T>
struct Less<T>
{
    using is_transparent = void;
    using Members = decltype(std::declval<T>().identifying_members());

    bool operator()(const T& lhs, const T& rhs) const noexcept { return Less<Members> {}(lhs.identifying_members(), rhs.identifying_members()); }

    template<typename U>
    bool operator()(const T& lhs, const U& rhs) const noexcept
        requires requires { std::less<> {}(lhs.identifying_members(), rhs); }
    {
        return Less<Members> {}(lhs.identifying_members(), rhs);
    }

    template<typename U>
    bool operator()(const U& lhs, const T& rhs) const noexcept
        requires requires { std::less<> {}(lhs, rhs.identifying_members()); }
    {
        return Less<Members> {}(lhs, rhs.identifying_members());
    }
};

}

#endif
