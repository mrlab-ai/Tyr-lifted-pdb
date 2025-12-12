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

#include <gtest/gtest.h>
#include <tyr/planning/planning.hpp>

using namespace tyr::buffer;
using namespace tyr::formalism;
using namespace tyr::planning;

namespace tyr::tests
{

static GroundTaskPtr compute_ground_task(const fs::path& domain_filepath, const fs::path& problem_filepath)
{
    auto parser = planning::Parser(domain_filepath);
    auto domain = parser.get_domain();
    auto lifted_task = parser.parse_task(problem_filepath);
    return lifted_task->get_ground_task();
}

static fs::path absolute(const std::string& subdir) { return fs::path(std::string(DATA_DIR)) / subdir; }

TEST(TyrTests, TyrPlanningGroundTask)
{
    {
        // Airport

        auto ground_task = compute_ground_task(absolute("airport/domain.pddl"), absolute("airport/test_problem.pddl"));

        EXPECT_EQ(ground_task->get_num_atoms<FluentTag>(), 59);
        EXPECT_EQ(ground_task->get_num_atoms<DerivedTag>(), 379);
        EXPECT_EQ(ground_task->get_num_actions(), 63);
        EXPECT_EQ(ground_task->get_num_axioms(), 420);
    }

    {
        // Assembly

        auto ground_task = compute_ground_task(absolute("assembly/domain.pddl"), absolute("assembly/test_problem.pddl"));

        EXPECT_EQ(ground_task->get_num_atoms<FluentTag>(), 7);
        EXPECT_EQ(ground_task->get_num_atoms<DerivedTag>(), 8);
        EXPECT_EQ(ground_task->get_num_actions(), 6);
        EXPECT_EQ(ground_task->get_num_axioms(), 2);
    }

    {
        // Barman

        auto ground_task = compute_ground_task(absolute("barman/domain.pddl"), absolute("barman/test_problem.pddl"));

        EXPECT_EQ(ground_task->get_num_atoms<FluentTag>(), 26);
        EXPECT_EQ(ground_task->get_num_atoms<DerivedTag>(), 0);
        EXPECT_EQ(ground_task->get_num_actions(), 84);
        EXPECT_EQ(ground_task->get_num_axioms(), 0);
    }

    {
        // Blocks_3

        auto ground_task = compute_ground_task(absolute("blocks_3/domain.pddl"), absolute("blocks_3/test_problem.pddl"));

        EXPECT_EQ(ground_task->get_num_atoms<FluentTag>(), 15);
        EXPECT_EQ(ground_task->get_num_atoms<DerivedTag>(), 0);
        EXPECT_EQ(ground_task->get_num_actions(), 45);
        EXPECT_EQ(ground_task->get_num_axioms(), 0);
    }

    {
        // Blocks_4

        auto ground_task = compute_ground_task(absolute("blocks_4/domain.pddl"), absolute("blocks_4/test_problem.pddl"));

        EXPECT_EQ(ground_task->get_num_atoms<FluentTag>(), 19);
        EXPECT_EQ(ground_task->get_num_atoms<DerivedTag>(), 0);
        EXPECT_EQ(ground_task->get_num_actions(), 24);
        EXPECT_EQ(ground_task->get_num_axioms(), 0);
    }

    {
        // Childsnack

        auto ground_task = compute_ground_task(absolute("childsnack/domain.pddl"), absolute("childsnack/test_problem.pddl"));

        EXPECT_EQ(ground_task->get_num_atoms<FluentTag>(), 8);
        EXPECT_EQ(ground_task->get_num_atoms<DerivedTag>(), 0);
        EXPECT_EQ(ground_task->get_num_actions(), 7);
        EXPECT_EQ(ground_task->get_num_axioms(), 0);
    }

    {
        // Delivery

        auto ground_task = compute_ground_task(absolute("delivery/domain.pddl"), absolute("delivery/test_problem.pddl"));

        EXPECT_EQ(ground_task->get_num_atoms<FluentTag>(), 10);
        EXPECT_EQ(ground_task->get_num_atoms<DerivedTag>(), 0);
        EXPECT_EQ(ground_task->get_num_actions(), 16);
        EXPECT_EQ(ground_task->get_num_axioms(), 0);
    }

    {
        // Driverlog

        auto ground_task = compute_ground_task(absolute("driverlog/domain.pddl"), absolute("driverlog/test_problem.pddl"));

        EXPECT_EQ(ground_task->get_num_atoms<FluentTag>(), 10);
        EXPECT_EQ(ground_task->get_num_atoms<DerivedTag>(), 0);
        EXPECT_EQ(ground_task->get_num_actions(), 14);
        EXPECT_EQ(ground_task->get_num_axioms(), 0);
    }

    {
        // Ferry

        auto ground_task = compute_ground_task(absolute("ferry/domain.pddl"), absolute("ferry/test_problem.pddl"));

        EXPECT_EQ(ground_task->get_num_atoms<FluentTag>(), 9);
        EXPECT_EQ(ground_task->get_num_atoms<DerivedTag>(), 0);
        EXPECT_EQ(ground_task->get_num_actions(), 12);
        EXPECT_EQ(ground_task->get_num_axioms(), 0);
    }

    {
        // Fo-counters

        auto ground_task = compute_ground_task(absolute("fo-counters/domain.pddl"), absolute("fo-counters/test_problem.pddl"));

        EXPECT_EQ(ground_task->get_num_atoms<FluentTag>(), 0);
        EXPECT_EQ(ground_task->get_num_atoms<DerivedTag>(), 0);
        EXPECT_EQ(ground_task->get_num_actions(), 12);
        EXPECT_EQ(ground_task->get_num_axioms(), 0);
    }

    {
        // Grid

        auto ground_task = compute_ground_task(absolute("grid/domain.pddl"), absolute("grid/test_problem.pddl"));

        EXPECT_EQ(ground_task->get_num_atoms<FluentTag>(), 21);
        EXPECT_EQ(ground_task->get_num_atoms<DerivedTag>(), 0);
        EXPECT_EQ(ground_task->get_num_actions(), 35);
        EXPECT_EQ(ground_task->get_num_axioms(), 0);
    }

    {
        // Gripper

        auto ground_task = compute_ground_task(absolute("gripper/domain.pddl"), absolute("gripper/test_problem.pddl"));

        EXPECT_EQ(ground_task->get_num_atoms<FluentTag>(), 12);
        EXPECT_EQ(ground_task->get_num_atoms<DerivedTag>(), 0);
        EXPECT_EQ(ground_task->get_num_actions(), 20);
        EXPECT_EQ(ground_task->get_num_axioms(), 0);
    }
}
}