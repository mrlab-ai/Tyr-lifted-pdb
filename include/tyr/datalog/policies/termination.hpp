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

#ifndef TYR_SOLVER_POLICIES_TERMINATION_HPP_
#define TYR_SOLVER_POLICIES_TERMINATION_HPP_

#include "tyr/common/config.hpp"
#include "tyr/formalism/datalog/declarations.hpp"
#include "tyr/formalism/datalog/ground_atom_index.hpp"

#include <concepts>

namespace tyr::datalog
{

template<typename T>
concept TerminationPolicy = requires(T& p, Index<formalism::datalog::GroundAtom<formalism::FluentTag>> atom) {
    { p.achieve_or_node(atom) } -> std::same_as<bool>;
    { p.clear() } -> std::same_as<void>;
};

class NoTerminationPolicy
{
public:
    bool achieve_or_node(Index<formalism::datalog::GroundAtom<formalism::FluentTag>> atom) const noexcept { return false; }
    void clear() noexcept {}
};
}

#endif
