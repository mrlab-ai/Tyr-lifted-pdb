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

#ifndef TYR_PLANNING_ABSTRACTIONS_PATTERN_GENERATOR_HPP_
#define TYR_PLANNING_ABSTRACTIONS_PATTERN_GENERATOR_HPP_

#include "tyr/common/declarations.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/formalism/planning/declarations.hpp"
#include "tyr/formalism/planning/fdr_fact_data.hpp"
#include "tyr/formalism/predicate_index.hpp"
#include "tyr/planning/declarations.hpp"

namespace tyr::planning
{
struct Pattern
{
    UnorderedSet<Data<formalism::planning::FDRFact<formalism::FluentTag>>> facts;
    UnorderedSet<Index<formalism::Predicate<formalism::FluentTag>>> predicates;

    auto size() const noexcept { return facts.size(); }
};

using PatternCollection = std::vector<Pattern>;

template<typename Task>
class PatternGenerator
{
    static_assert(dependent_false<Task>::value, "PatternGenerator is not defined for type T.");
};

}

#endif
