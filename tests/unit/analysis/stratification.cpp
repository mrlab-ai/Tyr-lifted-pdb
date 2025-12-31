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

#include "../utils.hpp"

#include <gtest/gtest.h>
#include <tyr/tyr.hpp>

namespace a = tyr::analysis;

namespace tyr::tests
{

TEST(TyrTests, TyrAnalysisStratification)
{
    auto [program, repository] = create_example_problem();

    auto rule_strata = a::compute_rule_stratification(program);

    std::cout << to_string(rule_strata.data) << std::endl;
}
}