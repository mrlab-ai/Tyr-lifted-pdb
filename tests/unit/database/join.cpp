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

#include <gtest/gtest.h>

#include "tyr/common/block_array_set.hpp"
#include "tyr/database/join.hpp"

#include <array>

namespace db = tyr::db;
namespace f = tyr::formalism;

namespace tyr::tests
{

using Table = BlockArraySet<uint_t>;

static_assert(db::FixedRowTable<Table>);

TEST(TyrTests, TyrDatabaseNaturalJoinWithBlockArraySet)
{
    auto lhs_table = Table(2);
    auto rhs_table = Table(2);
    auto out_table = Table(3);

    EXPECT_TRUE(lhs_table.insert(std::array<uint_t, 2> { 1, 10 }).second);
    EXPECT_TRUE(lhs_table.insert(std::array<uint_t, 2> { 2, 20 }).second);
    EXPECT_TRUE(lhs_table.insert(std::array<uint_t, 2> { 3, 30 }).second);

    EXPECT_TRUE(rhs_table.insert(std::array<uint_t, 2> { 10, 100 }).second);
    EXPECT_TRUE(rhs_table.insert(std::array<uint_t, 2> { 20, 200 }).second);
    EXPECT_TRUE(rhs_table.insert(std::array<uint_t, 2> { 40, 400 }).second);

    const auto lhs = db::Relation<Table>(lhs_table, { f::ParameterIndex(0), f::ParameterIndex(1) });
    const auto rhs = db::Relation<Table>(rhs_table, { f::ParameterIndex(1), f::ParameterIndex(2) });

    const auto joined = db::natural_join(lhs, rhs, out_table);

    ASSERT_TRUE(joined.is_well_formed());
    EXPECT_EQ(joined.columns, (std::vector<f::ParameterIndex> { f::ParameterIndex(0), f::ParameterIndex(1), f::ParameterIndex(2) }));
    EXPECT_EQ(joined.table->size(), 2);

    const auto row_0 = std::array<uint_t, 3> { 1, 10, 100 };
    const auto row_1 = std::array<uint_t, 3> { 2, 20, 200 };
    const auto row_2 = std::array<uint_t, 3> { 3, 30, 300 };

    EXPECT_TRUE(joined.table->contains(row_0));
    EXPECT_TRUE(joined.table->contains(row_1));
    EXPECT_FALSE(joined.table->contains(row_2));
}

TEST(TyrTests, TyrDatabaseAntiJoinWithBlockArraySet)
{
    auto lhs_table = Table(2);
    auto rhs_table = Table(1);
    auto out_table = Table(2);

    EXPECT_TRUE(lhs_table.insert(std::array<uint_t, 2> { 1, 10 }).second);
    EXPECT_TRUE(lhs_table.insert(std::array<uint_t, 2> { 2, 20 }).second);
    EXPECT_TRUE(lhs_table.insert(std::array<uint_t, 2> { 3, 30 }).second);

    EXPECT_TRUE(rhs_table.insert(std::array<uint_t, 1> { 20 }).second);
    EXPECT_TRUE(rhs_table.insert(std::array<uint_t, 1> { 40 }).second);

    const auto lhs = db::Relation<Table>(lhs_table, { f::ParameterIndex(0), f::ParameterIndex(1) });
    const auto rhs = db::Relation<Table>(rhs_table, { f::ParameterIndex(1) });

    const auto anti_joined = db::anti_join(lhs, rhs, out_table);

    ASSERT_TRUE(anti_joined.is_well_formed());
    EXPECT_EQ(anti_joined.columns, lhs.columns);
    EXPECT_EQ(anti_joined.table->size(), 2);

    const auto surviving_row_0 = std::array<uint_t, 2> { 1, 10 };
    const auto surviving_row_1 = std::array<uint_t, 2> { 3, 30 };
    const auto removed_row = std::array<uint_t, 2> { 2, 20 };

    EXPECT_TRUE(anti_joined.table->contains(surviving_row_0));
    EXPECT_TRUE(anti_joined.table->contains(surviving_row_1));
    EXPECT_FALSE(anti_joined.table->contains(removed_row));
}

}