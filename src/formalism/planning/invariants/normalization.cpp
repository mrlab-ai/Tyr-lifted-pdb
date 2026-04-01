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

#include "normalization.hpp"

#include "matching.hpp"

namespace tyr::formalism::planning::invariant
{
namespace
{
bool uses_parameter(const Data<Term>& term, ParameterIndex parameter)
{
    return std::visit(
        [&](auto&& arg) -> bool
        {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, ParameterIndex>)
                return arg == parameter;
            else if constexpr (std::is_same_v<T, Index<Object>>)
                return false;
            else
                static_assert(dependent_false<T>::value, "Missing case");
        },
        term.value);
}

bool uses_parameter(const TempAtom& atom, ParameterIndex parameter)
{
    return std::find_if(atom.terms.begin(), atom.terms.end(), [&](const auto& term) { return uses_parameter(term, parameter); }) != atom.terms.end();
}

bool uses_parameter(const Invariant& inv, ParameterIndex parameter)
{
    return std::find_if(inv.atoms.begin(), inv.atoms.end(), [&](const auto& atom) { return uses_parameter(atom, parameter); }) != inv.atoms.end();
}

void remove_covered_atoms(Invariant& inv)
{
    TempAtomList kept;
    kept.reserve(inv.atoms.size());

    for (size_t i = 0; i < inv.atoms.size(); ++i)
    {
        bool covered_by_other = false;

        for (size_t j = 0; j < inv.atoms.size(); ++j)
        {
            if (i == j)
                continue;

            const auto singleton = Invariant(inv.num_rigid_variables, inv.num_counted_variables, TempAtomList { inv.atoms[j] });

            if (covers(singleton, inv.atoms[i]))
            {
                covered_by_other = true;
                break;
            }
        }

        if (!covered_by_other)
            kept.push_back(inv.atoms[i]);
    }

    inv.atoms = std::move(kept);

    inv.predicates.clear();
    for (const auto& atom : inv.atoms)
        inv.predicates.insert(atom.predicate);
}

void remove_unused_parameters(Invariant& inv)
{
    const size_t total = inv.num_rigid_variables + inv.num_counted_variables;
    std::vector<bool> used(total, false);

    for (size_t p = 0; p < total; ++p)
        used[p] = uses_parameter(inv, ParameterIndex(p));

    std::vector<std::optional<ParameterIndex>> remap(total, std::nullopt);
    size_t next = 0;
    size_t new_num_rigid = 0;

    for (size_t p = 0; p < total; ++p)
    {
        if (!used[p])
            continue;

        remap[p] = ParameterIndex(next);
        if (p < inv.num_rigid_variables)
            ++new_num_rigid;
        ++next;
    }

    for (auto& atom : inv.atoms)
    {
        for (auto& term : atom.terms)
        {
            term = std::visit(
                [&](auto&& arg) -> Data<Term>
                {
                    using T = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_same_v<T, ParameterIndex>)
                        return Data<Term>(*remap[static_cast<uint_t>(arg)]);
                    else if constexpr (std::is_same_v<T, Index<Object>>)
                        return term;
                    else
                        static_assert(dependent_false<T>::value, "Missing case");
                },
                term.value);
        }
    }

    inv.num_rigid_variables = new_num_rigid;
    inv.num_counted_variables = next - new_num_rigid;
}

bool has_consistent_rigid_role_pattern(const Invariant& inv)
{
    if (inv.atoms.empty())
        return true;

    // For each rigid variable p, remember in which predicate/argument positions
    // it occurs. For the invariant families from the paper, inconsistent swaps
    // like in(V1,V0) versus in(V0,V1) should be rejected.
    for (size_t rigid = 0; rigid < inv.num_rigid_variables; ++rigid)
    {
        std::map<PredicateView<FluentTag>, std::vector<size_t>> positions_by_predicate;

        for (const auto& atom : inv.atoms)
        {
            std::vector<size_t> positions;

            for (size_t i = 0; i < atom.terms.size(); ++i)
            {
                const auto& term = atom.terms[i];
                std::visit(
                    [&](auto&& arg)
                    {
                        using T = std::decay_t<decltype(arg)>;
                        if constexpr (std::is_same_v<T, ParameterIndex>)
                        {
                            if (static_cast<uint_t>(arg) == rigid)
                                positions.push_back(i);
                        }
                    },
                    term.value);
            }

            auto it = positions_by_predicate.find(atom.predicate);
            if (it == positions_by_predicate.end())
            {
                positions_by_predicate.emplace(atom.predicate, std::move(positions));
            }
            else
            {
                if (it->second != positions)
                    return false;
            }
        }
    }

    return true;
}
}

void normalize_invariant(Invariant& inv)
{
    remove_covered_atoms(inv);
    remove_unused_parameters(inv);
    inv = Invariant(inv.num_rigid_variables, inv.num_counted_variables, std::move(inv.atoms));
}

bool is_well_shaped_invariant(const Invariant& inv) { return has_consistent_rigid_role_pattern(inv); }

}
