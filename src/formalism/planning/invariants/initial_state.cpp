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

#include "initial_state.hpp"

#include "matching.hpp"

namespace tyr::formalism::planning::invariant
{
namespace
{
TempAtom make_temp_ground_atom(GroundAtomView<FluentTag> element)
{
    auto terms = std::vector<Data<Term>> {};
    const auto objects = element.get_row().get_objects();
    terms.reserve(objects.size());

    for (const auto object : objects)
        terms.emplace_back(Data<Term>(object.get_index()));

    return TempAtom {
        .predicate = element.get_predicate(),
        .terms = std::move(terms),
    };
}

bool covered_atoms_conflict_in_initial_state(const Invariant& inv, const TempAtom& lhs, const TempAtom& rhs)
{
    for (const auto& pattern_lhs : inv.atoms)
    {
        auto sigma_lhs = match_invariant_against_ground_atom(inv, pattern_lhs, lhs);
        if (!sigma_lhs.has_value())
            continue;

        for (const auto& pattern_rhs : inv.atoms)
        {
            auto sigma_rhs = match_invariant_against_ground_atom(inv, pattern_rhs, rhs);
            if (!sigma_rhs.has_value())
                continue;

            bool compatible = true;

            for (size_t p = 0; p < inv.num_rigid_variables; ++p)
            {
                const auto& lhs_binding = sigma_lhs->get(ParameterIndex(p));
                const auto& rhs_binding = sigma_rhs->get(ParameterIndex(p));

                if (lhs_binding.has_value() && rhs_binding.has_value() && *lhs_binding != *rhs_binding)
                {
                    compatible = false;
                    break;
                }
            }

            if (compatible)
                return true;
        }
    }

    return false;
}
}

bool holds_in_initial_state(const Invariant& inv, GroundAtomListView<FluentTag> initial_atoms)
{
    TempAtomList covered_atoms;
    covered_atoms.reserve(initial_atoms.size());

    for (const auto atom_view : initial_atoms)
    {
        auto atom = make_temp_ground_atom(atom_view);

        for (const auto& pattern : inv.atoms)
        {
            if (match_invariant_against_ground_atom(inv, pattern, atom).has_value())
            {
                covered_atoms.push_back(std::move(atom));
                break;
            }
        }
    }

    for (size_t i = 0; i < covered_atoms.size(); ++i)
    {
        for (size_t j = i + 1; j < covered_atoms.size(); ++j)
        {
            if (covered_atoms_conflict_in_initial_state(inv, covered_atoms[i], covered_atoms[j]))
                return false;
        }
    }

    return true;
}
}
