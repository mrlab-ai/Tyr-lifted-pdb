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
#include <tyr/formalism/formalism.hpp>
#include <tyr/planning/lifted_task.hpp>

using namespace tyr::buffer;
using namespace tyr::formalism;
using namespace tyr::planning;

namespace tyr::tests
{

TEST(TyrTests, TyrPlanningLiftedTask)
{
    auto domain_repository = std::make_shared<Repository>();
    auto task_repository = std::make_shared<Repository>();
    auto scoped_task_repository = std::make_shared<ScopedRepository<Repository>>(*domain_repository, *task_repository);

    auto domain_view = View<Index<formalism::planning::Domain>, Repository>(Index<formalism::planning::Domain>(0), *domain_repository);

    auto domain = std::make_shared<planning::Domain>(domain_repository, domain_view);

    auto task_view = View<Index<formalism::planning::Task>, ScopedRepository<Repository>>(Index<formalism::planning::Task>(0), *scoped_task_repository);

    auto task = std::make_shared<planning::LiftedTask>(domain, task_repository, scoped_task_repository, task_view);
}
}