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

#ifndef TYR_FORMALISM_UNIFICATION_SUBSTITUTION_HPP_
#define TYR_FORMALISM_UNIFICATION_SUBSTITUTION_HPP_

#include "tyr/common/comparators.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/formalism/parameter_index.hpp"
#include "tyr/formalism/term_data.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <optional>
#include <vector>

namespace tyr::formalism::unification
{

template<typename T>
class SubstitutionFunction
{
public:
    using value_type = T;

    SubstitutionFunction() : m_domain(empty_domain()) {}

    explicit SubstitutionFunction(std::vector<ParameterIndex> parameters) :
        m_domain(std::make_shared<const Domain>(std::move(parameters))),
        m_data(m_domain->parameters.size())
    {
    }

    static SubstitutionFunction from_range(ParameterIndex offset, size_t size)
    {
        auto parameters = std::vector<ParameterIndex> {};
        parameters.reserve(size);

        for (size_t i = 0; i < size; ++i)
            parameters.push_back(ParameterIndex(uint_t(offset) + i));

        return SubstitutionFunction(std::move(parameters));
    }

    [[nodiscard]] size_t size() const noexcept { return m_data.size(); }

    [[nodiscard]] const std::vector<ParameterIndex>& parameters() const noexcept { return m_domain->parameters; }

    [[nodiscard]] bool contains_parameter(ParameterIndex p) const noexcept { return m_domain->position_of(p) != Domain::npos; }

    [[nodiscard]] bool is_bound(ParameterIndex p) const noexcept
    {
        const auto pos = m_domain->position_of(p);
        return pos != Domain::npos && m_data[pos].has_value();
    }

    [[nodiscard]] bool is_unbound(ParameterIndex p) const noexcept
    {
        const auto pos = m_domain->position_of(p);
        return pos != Domain::npos && !m_data[pos].has_value();
    }

    [[nodiscard]] bool has_binding(ParameterIndex p) const noexcept
    {
        const auto slot = try_get(p);
        return slot != nullptr && slot->has_value();
    }

    [[nodiscard]] const std::optional<T>* try_get(ParameterIndex p) const noexcept
    {
        const auto pos = m_domain->position_of(p);
        return pos != Domain::npos ? &m_data[pos] : nullptr;
    }

    [[nodiscard]] std::optional<T>* try_get(ParameterIndex p) noexcept
    {
        const auto pos = m_domain->position_of(p);
        return pos != Domain::npos ? &m_data[pos] : nullptr;
    }

    const std::optional<T>& operator[](ParameterIndex p) const { return m_data[m_domain->position_of(p)]; }

    std::optional<T>& operator[](ParameterIndex p) { return m_data[m_domain->position_of(p)]; }

    [[nodiscard]] bool assign(ParameterIndex p, const T& value)
    {
        auto& slot = m_data[m_domain->position_of(p)];
        if (slot.has_value())
            return false;
        slot = value;
        return true;
    }

    [[nodiscard]] bool assign_or_check(ParameterIndex p, const T& value)
    {
        auto& slot = m_data[m_domain->position_of(p)];
        if (!slot.has_value())
        {
            slot = value;
            return true;
        }
        return EqualTo<T> {}(*slot, value);
    }

    /// Returns true iff no parameter in the domain currently has a bound value.
    [[nodiscard]] bool is_identity() const noexcept
    {
        return std::all_of(m_data.begin(), m_data.end(), [](const auto& slot) { return !slot.has_value(); });
    }

    template<typename F>
    void for_each_binding(F&& f) const
    {
        const auto& params = m_domain->parameters;
        for (size_t i = 0; i < params.size(); ++i)
        {
            if (m_data[i].has_value())
                std::forward<F>(f)(params[i], *m_data[i]);
        }
    }

    void reset() noexcept { std::fill(m_data.begin(), m_data.end(), std::nullopt); }

    auto identifying_members() const noexcept { return std::tie(m_domain->parameters, m_data); }

private:
    // The substitution's domain (parameter list + parameter->slot index) is
    // immutable after construction and is identical across every copy derived
    // from the same substitution. Storing it behind a shared_ptr lets copies
    // share it instead of re-allocating and re-hashing the position map on every
    // copy — the hot path of projection enumeration copies substitutions
    // constantly, where this map copy was ~15-20% of total time.
    struct Domain
    {
        static constexpr size_t npos = static_cast<size_t>(-1);

        std::vector<ParameterIndex> parameters;
        // Only built when the domain is NOT a contiguous ascending range; for the
        // common case (from_range) `position_of` is pure arithmetic and this map
        // stays empty. The m_positions hash lookups were ~13% of projection-build
        // time on substitution-heavy domains (pipesworld).
        UnorderedMap<ParameterIndex, size_t> positions;
        uint_t min_param = 0;
        bool contiguous = true;

        explicit Domain(std::vector<ParameterIndex> ps) : parameters(std::move(ps))
        {
            for (size_t i = 0; i < parameters.size(); ++i)
            {
                const auto v = uint_t(parameters[i]);
                if (i == 0)
                    min_param = v;
                else if (v != min_param + i)
                {
                    contiguous = false;
                    break;
                }
            }
            if (!contiguous)
            {
                positions.reserve(parameters.size());
                for (size_t i = 0; i < parameters.size(); ++i)
                {
                    [[maybe_unused]] const auto [it, inserted] = positions.emplace(parameters[i], i);
                    assert(inserted && "Duplicate parameter in SubstitutionFunction domain");
                }
            }
        }

        // Slot index of parameter p, or npos if p is not in the domain.
        [[nodiscard]] size_t position_of(ParameterIndex p) const noexcept
        {
            if (contiguous)
            {
                const auto v = uint_t(p);
                if (v < min_param)
                    return npos;
                const size_t idx = v - min_param;
                return idx < parameters.size() ? idx : npos;
            }
            const auto it = positions.find(p);
            return it != positions.end() ? it->second : npos;
        }
    };

    static const std::shared_ptr<const Domain>& empty_domain()
    {
        static const std::shared_ptr<const Domain> instance = std::make_shared<const Domain>(std::vector<ParameterIndex> {});
        return instance;
    }

    std::shared_ptr<const Domain> m_domain;
    std::vector<std::optional<T>> m_data;
};

template<typename S, typename V>
concept SubstitutionFor = requires(S s, const S cs, ParameterIndex p, const V& v) {
    typename S::value_type;
    requires std::same_as<typename S::value_type, V>;
    { cs.contains_parameter(p) } -> std::same_as<bool>;
    { cs.is_bound(p) } -> std::same_as<bool>;
    { cs.is_unbound(p) } -> std::same_as<bool>;
    { cs[p] } -> std::same_as<const std::optional<V>&>;
    { s[p] } -> std::same_as<std::optional<V>&>;
    { s.assign_or_check(p, v) } -> std::same_as<bool>;
};

template<typename S>
concept ObjectSubstitution = SubstitutionFor<S, Index<Object>>;

template<typename S>
concept TermSubstitution = SubstitutionFor<S, Data<Term>>;

}
#endif
