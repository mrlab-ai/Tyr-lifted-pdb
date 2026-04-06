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

#ifndef TYR_DATABASE_JOIN_HPP_
#define TYR_DATABASE_JOIN_HPP_

#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/database/concepts.hpp"
#include "tyr/database/detail/join_utils.hpp"
#include "tyr/database/join_plan.hpp"
#include "tyr/database/relation.hpp"
#include "tyr/formalism/parameter_index.hpp"

#include <concepts>
#include <cstddef>
#include <optional>
#include <ranges>
#include <span>
#include <unordered_map>

namespace tyr::db
{
template<FixedRowTable Table>
Relation<Table> natural_join(const Relation<Table>& lhs, const Relation<Table>& rhs, Table& out_table)
{
    assert(lhs.is_well_formed());
    assert(rhs.is_well_formed());

    const auto plan = make_natural_join_plan(lhs.columns, rhs.columns);

    assert(out_table.length() == plan.out_columns.size());
    out_table.clear();

    using value_type = typename Table::value_type;
    using index_type = typename Table::index_type;
    using key_type = std::vector<value_type>;
    using index_bucket_type = std::vector<index_type>;
    using map_type = gtl::flat_hash_map<key_type, index_bucket_type, Hash<key_type>, EqualTo<key_type>>;

    auto out_row = std::vector<value_type>(plan.out_columns.size());

    if (plan.is_cross_product())
    {
        for (std::size_t li = 0; li < lhs.table->size(); ++li)
        {
            const auto lhs_row = (*lhs.table)[li];

            for (std::size_t ri = 0; ri < rhs.table->size(); ++ri)
            {
                const auto rhs_row = (*rhs.table)[ri];
                detail::merge_rows<Table>(lhs_row, rhs_row, plan.rhs_payload_pos, out_row);
                out_table.insert(out_row);
            }
        }

        return Relation<Table>(out_table, plan.out_columns);
    }

    // Hash rhs on the join key.
    auto rhs_index = map_type {};
    rhs_index.reserve(rhs.table->size());

    auto key_buffer = std::vector<value_type> {};
    key_buffer.reserve(plan.rhs_key_pos.size());

    for (std::size_t ri = 0; ri < rhs.table->size(); ++ri)
    {
        const auto rhs_row = (*rhs.table)[ri];
        detail::extract_key<Table>(rhs_row, plan.rhs_key_pos, key_buffer);

        auto [it, inserted] = rhs_index.try_emplace(key_buffer);
        it->second.push_back(index_type(ri));
    }

    key_buffer.clear();
    key_buffer.reserve(plan.lhs_key_pos.size());

    for (std::size_t li = 0; li < lhs.table->size(); ++li)
    {
        const auto lhs_row = (*lhs.table)[li];
        detail::extract_key<Table>(lhs_row, plan.lhs_key_pos, key_buffer);

        const auto it = rhs_index.find(key_buffer);
        if (it == rhs_index.end())
            continue;

        for (const auto rhs_index_value : it->second)
        {
            const auto rhs_row = (*rhs.table)[std::size_t(rhs_index_value)];
            detail::merge_rows<Table>(lhs_row, rhs_row, plan.rhs_payload_pos, out_row);
            out_table.insert(out_row);
        }
    }

    return Relation<Table>(out_table, plan.out_columns);
}

template<FixedRowTable Table>
Relation<Table> anti_join(const Relation<Table>& lhs, const Relation<Table>& rhs, Table& out_table)
{
    assert(lhs.is_well_formed());
    assert(rhs.is_well_formed());

    const auto plan = make_natural_join_plan(lhs.columns, rhs.columns);

    // Anti-join keeps lhs schema.
    assert(out_table.length() == lhs.arity());
    out_table.clear();

    using value_type = typename Table::value_type;
    using key_type = std::vector<value_type>;
    using set_type = gtl::flat_hash_set<key_type, Hash<key_type>, EqualTo<key_type>>;

    auto out_row = std::vector<value_type>(lhs.arity());

    if (plan.is_cross_product())
    {
        // If there are no shared columns, every lhs row matches every rhs row.
        // Therefore lhs survives only when rhs is empty.
        if (rhs.table->empty())
        {
            for (std::size_t li = 0; li < lhs.table->size(); ++li)
            {
                const auto lhs_row = (*lhs.table)[li];
                detail::copy_row<Table>(lhs_row, out_row);
                out_table.insert(out_row);
            }
        }

        return Relation<Table>(out_table, lhs.columns);
    }

    // Build set of rhs keys.
    auto rhs_keys = set_type {};
    rhs_keys.reserve(rhs.table->size());
    auto key_buffer = std::vector<value_type> {};

    for (std::size_t ri = 0; ri < rhs.table->size(); ++ri)
    {
        const auto rhs_row = (*rhs.table)[ri];
        detail::extract_key<Table>(rhs_row, plan.rhs_key_pos, key_buffer);
        rhs_keys.insert(key_buffer);
    }

    for (std::size_t li = 0; li < lhs.table->size(); ++li)
    {
        const auto lhs_row = (*lhs.table)[li];
        detail::extract_key<Table>(lhs_row, plan.lhs_key_pos, key_buffer);

        if (rhs_keys.contains(key_buffer))
            continue;

        detail::copy_row<Table>(lhs_row, out_row);
        out_table.insert(out_row);
    }

    return Relation<Table>(out_table, lhs.columns);
}

}

#endif