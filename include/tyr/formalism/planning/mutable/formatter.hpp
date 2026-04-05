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

#ifndef TYR_FORMALISM_PLANNING_MUTABLE_FORMATTER_HPP_
#define TYR_FORMALISM_PLANNING_MUTABLE_FORMATTER_HPP_

#include "tyr/common/formatter.hpp"
#include "tyr/common/iostream.hpp"
#include "tyr/formalism/formatter.hpp"
#include "tyr/formalism/planning/mutable/action.hpp"
#include "tyr/formalism/planning/mutable/atom.hpp"
#include "tyr/formalism/planning/mutable/conditional_effect.hpp"
#include "tyr/formalism/planning/mutable/conjunctive_condition.hpp"
#include "tyr/formalism/planning/mutable/conjunctive_effect.hpp"

#include <fmt/core.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <ostream>

namespace tyr
{
/**
 * Forward declarations
 */

namespace formalism::planning
{
template<FactKind T>
std::ostream& operator<<(std::ostream& os, const MutableAtom<T>& el);

template<FactKind T>
std::ostream& operator<<(std::ostream& os, const MutableLiteral<T>& el);

std::ostream& operator<<(std::ostream& os, const ConditionalEffect& el);

std::ostream& operator<<(std::ostream& os, const ConjunctiveCondition& el);

std::ostream& operator<<(std::ostream& os, const ConjunctiveEffect& el);

std::ostream& operator<<(std::ostream& os, const Action& el);
}

/**
 * Declarations
 */

template<formalism::FactKind T>
std::ostream& print(std::ostream& os, const formalism::planning::MutableAtom<T>& el);

template<formalism::FactKind T>
std::ostream& print(std::ostream& os, const formalism::planning::MutableLiteral<T>& el);

std::ostream& print(std::ostream& os, const formalism::planning::MutableConditionalEffect& el);

std::ostream& print(std::ostream& os, const formalism::planning::MutableConjunctiveCondition& el);

std::ostream& print(std::ostream& os, const formalism::planning::MutableConjunctiveEffect& el);

std::ostream& print(std::ostream& os, const formalism::planning::MutableAction& el);

namespace formalism::planning
{
template<FactKind T>
std::ostream& operator<<(std::ostream& os, const MutableAtom<T>& el);

template<FactKind T>
std::ostream& operator<<(std::ostream& os, const MutableLiteral<T>& el);

std::ostream& operator<<(std::ostream& os, const MutableConditionalEffect& el);

std::ostream& operator<<(std::ostream& os, const MutableConjunctiveCondition& el);

std::ostream& operator<<(std::ostream& os, const MutableConjunctiveEffect& el);

std::ostream& operator<<(std::ostream& os, const MutableAction& el);
}
}

#endif
