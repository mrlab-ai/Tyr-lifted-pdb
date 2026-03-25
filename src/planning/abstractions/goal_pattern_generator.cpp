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

#include "tyr/planning/abstractions/goal_pattern_generator.hpp"

#include "tyr/common/declarations.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/formalism/planning/declarations.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/planning/views.hpp"
#include "tyr/planning/abstractions/pattern_generator.hpp"
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/ground_task.hpp"
#include "tyr/planning/lifted_task.hpp"

namespace tyr::planning
{

template<typename Task>
GoalPatternGenerator<Task>::GoalPatternGenerator(std::shared_ptr<const Task> task) : m_task(std::move(task))
{
}

template<typename Task>
std::shared_ptr<GoalPatternGenerator<Task>> GoalPatternGenerator<Task>::create(std::shared_ptr<const Task> task)
{
    return std::make_shared<GoalPatternGenerator<Task>>(std::move(task));
}

template<typename Task>
PatternCollection GoalPatternGenerator<Task>::generate()
{
    auto patterns = PatternCollection {};

    for (const auto fact : m_task->get_task().get_goal().template get_facts<formalism::FluentTag>())
        patterns.push_back(Pattern({ fact }));

    return patterns;
}

template class GoalPatternGenerator<LiftedTask>;
template class GoalPatternGenerator<GroundTask>;

}
