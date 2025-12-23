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
 *<
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef TYR_COMMON_INDEXED_HASH_SET_HPP_
#define TYR_COMMON_INDEXED_HASH_SET_HPP_

#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/common/types.hpp"

#include <concepts>
#include <gtl/phmap.hpp>
#include <memory>
#include <optional>
#include <vector>

namespace tyr
{

template<typename T, Indexable I, typename H = Hash<T>, typename E = EqualTo<T>>
class IndexedHashSet
{
private:
    struct IndexableHash;
    struct IndexableEqualTo;

public:
    IndexedHashSet() : m_vec(std::make_shared<std::vector<T>>()), m_set(0, IndexableHash(m_vec), IndexableEqualTo(m_vec)) {}
    IndexedHashSet(const IndexedHashSet& other) = delete;
    IndexedHashSet& operator=(const IndexedHashSet& other) = delete;
    IndexedHashSet(IndexedHashSet&& other) = default;
    IndexedHashSet& operator=(IndexedHashSet&& other) = default;

    std::optional<I> find(const T& value) const
    {
        if (auto it = m_set.find(value); it != m_set.end())
            return *it;

        return std::nullopt;
    }

    I insert(const T& value)
    {
        if (auto it = m_set.find(value); it != m_set.end())
            return *it;

        I idx(static_cast<uint_t>(m_vec->size()));
        m_vec->push_back(value);
        m_set.emplace(idx);
        return idx;
    }

    const T& operator[](I idx) const noexcept { return (*m_vec)[idx.get_value()]; }

    std::size_t size() const noexcept { return m_vec->size(); }

private:
    struct IndexableHash
    {
        using is_transparent = void;

        std::shared_ptr<const std::vector<T>> vec;
        H hash;

        IndexableHash() noexcept : vec(nullptr) {}
        explicit IndexableHash(std::shared_ptr<const std::vector<T>> vec) noexcept : vec(std::move(vec)) {}

        size_t operator()(I el) const noexcept { return hash((*vec)[el.get_value()]); }
        size_t operator()(const T& el) const noexcept { return hash(el); }
    };

    struct IndexableEqualTo
    {
        using is_transparent = void;

        std::shared_ptr<const std::vector<T>> vec;
        E equal_to;

        IndexableEqualTo() noexcept : vec(nullptr), equal_to() {}
        explicit IndexableEqualTo(std::shared_ptr<const std::vector<T>> vec) noexcept : vec(std::move(vec)), equal_to() {}

        bool operator()(I lhs, I rhs) const noexcept { return equal_to((*vec)[lhs.get_value()], (*vec)[rhs.get_value()]); }

        bool operator()(const T& lhs, I rhs) const noexcept { return equal_to(lhs, (*vec)[rhs.get_value()]); }
        bool operator()(I lhs, const T& rhs) const noexcept { return equal_to((*vec)[lhs.get_value()], rhs); }
        bool operator()(const T& lhs, const T& rhs) const noexcept { return equal_to(lhs, rhs); }
    };

    std::shared_ptr<std::vector<T>> m_vec;
    gtl::flat_hash_set<I, IndexableHash, IndexableEqualTo> m_set;
};
}

#endif