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

#ifndef TYR_DATABASE_CONCEPTS_HPP_
#define TYR_DATABASE_CONCEPTS_HPP_

#include <concepts>
#include <cstddef>
#include <optional>
#include <ranges>
#include <span>

namespace tyr::db
{
// TODO: define a concept that describes table requirements to be able to define operations such as joins/anti-joins.
// The BlockArraySet should satisfy this concept, and if not we could adapt it to do so.

template<typename T>
concept FixedRowTable = requires(T table, const T ctable, std::span<const typename T::value_type> row, std::size_t i) {
    typename T::value_type;
    typename T::index_type;
    typename T::ConstArrayView;

    // fixed row width
    { ctable.length() } -> std::convertible_to<std::size_t>;

    // number of rows
    { ctable.size() } -> std::convertible_to<std::size_t>;
    { ctable.empty() } -> std::convertible_to<bool>;

    // row access
    { ctable[i] } -> std::same_as<typename T::ConstArrayView>;

    // lookup / insertion / reset
    { ctable.find(row) } -> std::same_as<std::optional<typename T::index_type>>;
    { table.insert(row) } -> std::same_as<std::pair<typename T::index_type, bool>>;
    { table.clear() } noexcept;
};

}  // namespace tyr::db

#endif