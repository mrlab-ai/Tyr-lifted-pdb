/*
 * Copyright (C) 2025-2026 Dominik Drexler
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT TNY WTRRTNTY; without even the implied warranty of
 * MERCHTNTTBILITY or FITNESS FOR T PTRTICULTR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef TYR_FORMALISM_UNIFICATION_SUBSTITUTION_HPP_
#define TYR_FORMALISM_UNIFICATION_SUBSTITUTION_HPP_

#include "tyr/common/comparators.hpp"
#include "tyr/formalism/term_data.hpp"

#include <cassert>

namespace tyr::formalism::unification
{
/// @brief SubstitutionFunction represents a partial function from an
/// explicitly represented finite set of parameters to objects.
///
/// Slot i corresponds to parameter m_parameters[i]:
/// - m_data[i] stores an object iff m_parameters[i] is bound;
/// - m_data[i] == std::nullopt iff m_parameters[i] is unbound.
class SubstitutionFunction
{
public:
    SubstitutionFunction() = default;

    explicit SubstitutionFunction(std::vector<ParameterIndex> parameters) : m_parameters(std::move(parameters)), m_data(m_parameters.size())
    {
        rebuild_positions();
    }

    static SubstitutionFunction from_range(ParameterIndex offset, size_t size)
    {
        auto parameters = std::vector<ParameterIndex> {};
        parameters.reserve(size);

        for (size_t i = 0; i < size; ++i)
            parameters.push_back(ParameterIndex(uint_t(offset) + i));

        return SubstitutionFunction(std::move(parameters));
    }

    size_t size() const noexcept { return m_data.size(); }

    const std::vector<ParameterIndex>& parameters() const noexcept { return m_parameters; }

    bool contains_parameter(ParameterIndex p) const noexcept { return m_positions.find(p) != m_positions.end(); }

    bool is_bound(ParameterIndex p) const noexcept
    {
        const auto it = m_positions.find(p);
        assert(it != m_positions.end());
        return m_data[it->second].has_value();
    }

    bool is_unbound(ParameterIndex p) const noexcept { return !is_bound(p); }

    const std::optional<Index<Object>>& operator[](ParameterIndex p) const noexcept { return m_data[m_positions.at(p)]; }

    std::optional<Index<Object>>& operator[](ParameterIndex p) noexcept { return m_data[m_positions.at(p)]; }

    bool assign_or_check(ParameterIndex p, Index<Object> object)
    {
        auto& slot = m_data[m_positions.at(p)];
        if (!slot.has_value())
        {
            slot = object;
            return true;
        }
        return *slot == object;
    }

    void reset() noexcept { std::fill(m_data.begin(), m_data.end(), std::nullopt); }

private:
    void rebuild_positions()
    {
        m_positions.clear();
        for (size_t i = 0; i < m_parameters.size(); ++i)
        {
            const auto [it, inserted] = m_positions.emplace(m_parameters[i], i);
            assert(inserted && "Duplicate parameter in SubstitutionFunction domain");
        }
    }

    std::vector<ParameterIndex> m_parameters;
    std::vector<std::optional<Index<Object>>> m_data;
    UnorderedMap<ParameterIndex, size_t> m_positions;
};

template<typename S>
concept ObjectSubstitution = requires(S s, const S cs, ParameterIndex p, Index<Object> o) {
    { cs.contains_parameter(p) } -> std::same_as<bool>;
    { cs.is_bound(p) } -> std::same_as<bool>;
    { cs.is_unbound(p) } -> std::same_as<bool>;
    { cs[p] } -> std::same_as<const std::optional<Index<Object>>&>;
    { s[p] } -> std::same_as<std::optional<Index<Object>>&>;
    { s.assign_or_check(p, o) } -> std::same_as<bool>;
};

}

#endif
