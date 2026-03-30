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
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/planning/views.hpp"
#include "tyr/planning/declarations.hpp"

namespace tyr::planning
{
struct Pattern
{
    formalism::planning::FDRFactViewList<formalism::FluentTag> facts;

    UnorderedSet<formalism::planning::FDRFactView<formalism::FluentTag>> facts_set;
    UnorderedSet<formalism::planning::PredicateView<formalism::FluentTag>> predicates_set;

    explicit Pattern(formalism::planning::FDRFactViewList<formalism::FluentTag> facts_) : facts(facts_), facts_set(), predicates_set()
    {
        for (const auto fact : facts_)
        {
            assert(fact.has_value());
            facts_set.insert(fact);
            predicates_set.insert(fact.get_atom()->get_predicate());
        }
    }

    auto size() const noexcept { return facts.size(); }
};

using PatternCollection = std::vector<Pattern>;

template<TaskKind Kind>
class PatternGenerator
{
public:
    virtual ~PatternGenerator() = default;

    virtual PatternCollection generate() = 0;
};

}

#endif
