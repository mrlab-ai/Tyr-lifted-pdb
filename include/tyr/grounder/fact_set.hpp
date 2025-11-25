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

#ifndef TYR_GROUNDER_FACT_SET_HPP_
#define TYR_GROUNDER_FACT_SET_HPP_

#include "tyr/formalism/declarations.hpp"

#include <boost/dynamic_bitset.hpp>

namespace tyr::grounder
{
template<formalism::IsContext C>
struct FactSets
{
    Proxy<IndexList<formalism::GroundAtom<formalism::StaticTag>>, C> static_atoms;
    Proxy<IndexList<formalism::GroundAtom<formalism::FluentTag>>, C> fluent_atoms;
    Proxy<IndexList<formalism::GroundFunctionTermValue<formalism::StaticTag>>, C> static_function_values;
    Proxy<IndexList<formalism::GroundFunctionTermValue<formalism::FluentTag>>, C> fluent_function_values;

    boost::dynamic_bitset<> static_atoms_bitset;
    boost::dynamic_bitset<> fluent_atoms_bitset;
    std::vector<float_t> static_function_values_vec;
    std::vector<float_t> fluent_function_values_vec;
};
}

#endif