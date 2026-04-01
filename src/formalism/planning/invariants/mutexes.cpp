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

#include "tyr/formalism/planning/invariants/mutexes.hpp"

#include "matching.hpp"

#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

namespace tyr::formalism::planning::invariant
{
namespace
{

struct GroupKey
{
    size_t invariant_index;
    std::vector<Index<Object>> rigid_values;

    friend bool operator==(const GroupKey&, const GroupKey&) = default;

    friend bool operator<(const GroupKey& lhs, const GroupKey& rhs)
    {
        return std::tie(lhs.invariant_index, lhs.rigid_values) < std::tie(rhs.invariant_index, rhs.rigid_values);
    }
};

TempAtom make_temp_ground_atom(GroundAtomView<FluentTag> element)
{
    std::vector<Data<Term>> terms;
    terms.reserve(element.get_row().get_objects().size());

    for (const auto object : element.get_row().get_objects())
        terms.emplace_back(Data<Term>(object.get_index()));

    return TempAtom {
        .predicate = element.get_predicate(),
        .terms = std::move(terms),
    };
}

std::optional<std::vector<Index<Object>>> extract_rigid_values(const Invariant& inv, const TempAtom& pattern, GroundAtomView<FluentTag> atom)
{
    const auto temp_ground_atom = make_temp_ground_atom(atom);
    const auto sigma = match_invariant_against_ground_atom(inv, pattern, temp_ground_atom);
    if (!sigma.has_value())
        return std::nullopt;

    std::vector<Index<Object>> rigid_values;
    rigid_values.reserve(inv.num_rigid_variables);

    for (size_t i = 0; i < inv.num_rigid_variables; ++i)
    {
        const auto value = sigma->get(ParameterIndex(i));
        if (!value.has_value())
            return std::nullopt;

        const auto maybe_object = std::visit(
            [](auto&& arg) -> std::optional<Index<Object>>
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Index<Object>>)
                    return arg;
                else if constexpr (std::is_same_v<T, ParameterIndex>)
                    return std::nullopt;
                else
                    static_assert(dependent_false<T>::value, "Missing case");
            },
            value->value);

        if (!maybe_object.has_value())
            return std::nullopt;

        rigid_values.push_back(*maybe_object);
    }

    return rigid_values;
}

bool is_counted_parameter(const Invariant& inv, ParameterIndex parameter) { return static_cast<uint_t>(parameter) >= inv.num_rigid_variables; }

std::optional<size_t> get_counted_position(const Invariant& inv, const TempAtom& atom)
{
    for (size_t pos = 0; pos < atom.terms.size(); ++pos)
    {
        bool found = false;
        std::visit(
            [&](auto&& arg)
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, ParameterIndex>)
                    found = is_counted_parameter(inv, arg);
            },
            atom.terms[pos].value);

        if (found)
            return pos;
    }

    return std::nullopt;
}

bool instantiate_matches_ground_atom(const TempAtom& pattern,
                                     const std::vector<Index<Object>>& rigid_values,
                                     std::optional<Index<Object>> counted_value,
                                     GroundAtomView<FluentTag> ground_atom)
{
    if (pattern.predicate != ground_atom.get_predicate())
        return false;
    if (pattern.terms.size() != ground_atom.get_row().get_objects().size())
        return false;

    for (size_t pos = 0; pos < pattern.terms.size(); ++pos)
    {
        const auto object = ground_atom.get_row().get_objects()[pos].get_index();

        bool ok = std::visit(
            [&](auto&& arg) -> bool
            {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, ParameterIndex>)
                {
                    const auto idx = static_cast<uint_t>(arg);

                    if (idx < rigid_values.size())
                        return rigid_values[idx] == object;

                    return counted_value.has_value() && *counted_value == object;
                }
                else if constexpr (std::is_same_v<T, Index<Object>>)
                {
                    return arg == object;
                }
                else
                {
                    static_assert(dependent_false<T>::value, "Missing case");
                }
            },
            pattern.terms[pos].value);

        if (!ok)
            return false;
    }

    return true;
}

GroundAtomViewList<FluentTag>
instantiate_group(const Invariant& inv, const std::vector<Index<Object>>& rigid_values, const GroundAtomViewList<FluentTag>& all_atoms)
{
    GroundAtomViewList<FluentTag> result;
    std::set<Index<GroundAtom<FluentTag>>> seen;

    for (const auto& pattern : inv.atoms)
    {
        const auto counted_pos = get_counted_position(inv, pattern);

        if (!counted_pos.has_value())
        {
            for (const auto atom : all_atoms)
            {
                if (!instantiate_matches_ground_atom(pattern, rigid_values, std::nullopt, atom))
                    continue;

                if (seen.insert(atom.get_index()).second)
                    result.push_back(atom);

                break;
            }
        }
        else
        {
            for (const auto atom : all_atoms)
            {
                if (!instantiate_matches_ground_atom(pattern, rigid_values, std::nullopt, atom)
                    && !instantiate_matches_ground_atom(pattern, rigid_values, atom.get_row().get_objects()[*counted_pos].get_index(), atom))
                    continue;

                if (seen.insert(atom.get_index()).second)
                    result.push_back(atom);
            }
        }
    }

    return result;
}

bool initial_atom_matches_part(const Invariant& inv, const TempAtom& part, GroundAtomView<FluentTag> atom)
{
    return match_invariant_against_ground_atom(inv, part, make_temp_ground_atom(atom)).has_value();
}

}  // namespace

std::vector<GroundAtomViewList<FluentTag>>
compute_mutex_groups(PlanningTask& task, Repository& repository, const GroundAtomViewList<FluentTag>& all_atoms, const InvariantList& invariants)
{
    (void) repository;

    std::map<GroupKey, size_t> nonempty_groups;
    std::set<GroupKey> overcrowded_groups;

    for (const auto atom : task.get_task().get_atoms<FluentTag>())
    {
        for (size_t inv_index = 0; inv_index < invariants.size(); ++inv_index)
        {
            const auto& inv = invariants[inv_index];

            if (!inv.predicates.contains(atom.get_predicate()))
                continue;

            for (const auto& part : inv.atoms)
            {
                if (part.predicate != atom.get_predicate())
                    continue;
                if (!initial_atom_matches_part(inv, part, atom))
                    continue;

                const auto rigid_values = extract_rigid_values(inv, part, atom);
                if (!rigid_values.has_value())
                    continue;

                const auto key = GroupKey {
                    .invariant_index = inv_index,
                    .rigid_values = *rigid_values,
                };

                if (!nonempty_groups.contains(key))
                    nonempty_groups.emplace(key, 1);
                else
                    overcrowded_groups.insert(key);
            }
        }
    }

    std::vector<GroundAtomViewList<FluentTag>> result;

    for (const auto& [key, _] : nonempty_groups)
    {
        if (overcrowded_groups.contains(key))
            continue;

        const auto& inv = invariants[key.invariant_index];
        auto group = instantiate_group(inv, key.rigid_values, all_atoms);

        if (!group.empty())
            result.push_back(std::move(group));
    }

    return result;
}

}  // namespace tyr::formalism::planning::invariant