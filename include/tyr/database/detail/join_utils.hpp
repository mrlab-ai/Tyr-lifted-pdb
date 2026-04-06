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

#ifndef TYR_DATABASE_DETAIL_JOIN_UTILS_HPP_
#define TYR_DATABASE_DETAIL_JOIN_UTILS_HPP_

#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/database/concepts.hpp"
#include "tyr/formalism/parameter_index.hpp"

#include <concepts>
#include <cstddef>
#include <optional>
#include <ranges>
#include <span>

namespace tyr::db::detail
{

template<FixedRowTable Table>
void extract_key(typename Table::ConstArrayView row, std::span<const std::size_t> positions, std::vector<typename Table::value_type>& out_key)
{
    out_key.clear();
    out_key.reserve(positions.size());

    for (const auto pos : positions)
        out_key.push_back(row[pos]);
}

template<FixedRowTable Table>
void copy_row(typename Table::ConstArrayView src, std::vector<typename Table::value_type>& dst)
{
    assert(dst.size() == src.size());

    for (std::size_t i = 0; i < src.size(); ++i)
        dst[i] = src[i];
}

template<FixedRowTable Table>
void merge_rows(typename Table::ConstArrayView lhs,
                typename Table::ConstArrayView rhs,
                std::span<const std::size_t> rhs_payload_pos,
                std::vector<typename Table::value_type>& out_row)
{
    std::size_t out_pos = 0;

    for (std::size_t i = 0; i < lhs.size(); ++i)
        out_row[out_pos++] = lhs[i];

    for (const auto pos : rhs_payload_pos)
        out_row[out_pos++] = rhs[pos];

    assert(out_pos == out_row.size());
}

}  // namespace detail

#endif