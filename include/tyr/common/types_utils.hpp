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
 *<
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef TYR_COMMON_TYPES_UTILS_HPP_
#define TYR_COMMON_TYPES_UTILS_HPP_

#include "tyr/common/types.hpp"

namespace tyr
{
template<typename T>
concept Clearable = requires(T& a) { a.clear(); };

template<typename T>
struct is_cista_variant_helper : std::false_type
{
};

template<typename... Ts>
struct is_cista_variant_helper<::cista::offset::variant<Ts...>> : std::true_type
{
};

template<typename T>
concept CistaVariantConcept = is_cista_variant_helper<T>::value;

template<typename T>
struct is_optional_variant_helper : std::false_type
{
};

template<typename T>
struct is_optional_variant_helper<::cista::optional<T>> : std::true_type
{
};

template<typename T>
concept CistaOptionalConcept = is_optional_variant_helper<std::remove_cvref_t<T>>::value;

template<typename T>
void clear(T& element)
{
    if constexpr (Clearable<T>)
        element.clear();
    else if constexpr (CistaVariantConcept<T>)
    {
        static_assert(std::is_default_constructible_v<T>, "Cannot clear variant: not default constructible.");
        // hard reset to default alternative.
        element.destruct();
        new (&element) T {};
    }
    else if constexpr (CistaOptionalConcept<T>)
        element = std::nullopt;
    else if constexpr (std::is_default_constructible_v<T> && std::is_assignable_v<T&, T&&>)
        element = T {};
    else if constexpr (std::is_default_constructible_v<T> && std::is_move_constructible_v<T> && std::is_swappable_v<T>)
    {
        T tmp {};
        using std::swap;
        swap(element, tmp);
    }
    else
        static_assert(dependent_false<T>::value, "Cannot clear T.");
}

}

#endif