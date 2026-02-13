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

#include "tyr/planning/lifted_task/abstractions/pattern_generator.hpp"

#include "tyr/common/declarations.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/formalism/planning/declarations.hpp"
#include "tyr/formalism/planning/fdr_fact_view.hpp"
#include "tyr/formalism/predicate_view.hpp"
#include "tyr/planning/abstractions/pattern_generator.hpp"
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/lifted_task.hpp"

namespace tyr::planning
{

PatternGenerator<LiftedTask>::PatternGenerator(LiftedTask& task) : m_task(task) {}

PatternCollection PatternGenerator<LiftedTask>::generate()
{
    auto patterns = PatternCollection {};

    for (const auto fact : m_task.get_task().get_goal().get_facts<formalism::FluentTag>())
    {
        auto pattern = Pattern {};
        pattern.facts.insert(fact.get_data());
        for (const auto atom : fact.get_variable().get_atoms())
        {
            pattern.predicates.insert(atom.get_predicate().get_index());
        }
        patterns.push_back(std::move(pattern));
    }

    return patterns;
}

}
