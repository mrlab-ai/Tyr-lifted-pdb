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

#ifndef TYR_DATABASE_JOIN_PLAN_HPP_
#define TYR_DATABASE_JOIN_PLAN_HPP_

#include "tyr/formalism/parameter_index.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace tyr::db
{

struct JoinPlan
{
    std::vector<std::size_t> lhs_key_pos;
    std::vector<std::size_t> rhs_key_pos;
    std::vector<std::size_t> rhs_payload_pos;
    std::vector<formalism::ParameterIndex> out_columns;

    bool is_cross_product() const noexcept { return lhs_key_pos.empty(); }
};

inline JoinPlan make_natural_join_plan(std::span<const formalism::ParameterIndex> lhs_columns, std::span<const formalism::ParameterIndex> rhs_columns)
{
    auto plan = JoinPlan {};

    // Build lookup for rhs columns.
    auto rhs_pos = UnorderedMap<formalism::ParameterIndex, std::size_t> {};
    rhs_pos.reserve(rhs_columns.size());

    for (std::size_t i = 0; i < rhs_columns.size(); ++i)
        rhs_pos.emplace(rhs_columns[i], i);

    // Shared columns define the join key.
    auto shared_rhs = std::vector<bool>(rhs_columns.size(), false);

    for (std::size_t i = 0; i < lhs_columns.size(); ++i)
    {
        if (const auto it = rhs_pos.find(lhs_columns[i]); it != rhs_pos.end())
        {
            plan.lhs_key_pos.push_back(i);
            plan.rhs_key_pos.push_back(it->second);
            shared_rhs[it->second] = true;
        }
    }

    // Output schema = lhs columns followed by rhs-only columns.
    plan.out_columns.reserve(lhs_columns.size() + rhs_columns.size());
    plan.out_columns.insert(plan.out_columns.end(), lhs_columns.begin(), lhs_columns.end());

    for (std::size_t j = 0; j < rhs_columns.size(); ++j)
    {
        if (!shared_rhs[j])
        {
            plan.rhs_payload_pos.push_back(j);
            plan.out_columns.push_back(rhs_columns[j]);
        }
    }

    return plan;
}

}

#endif