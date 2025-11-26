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

#ifndef TYR_COMMON_DYNAMIC_BITSET_HPP_
#define TYR_COMMON_DYNAMIC_BITSET_HPP_

#include "tyr/common/declarations.hpp"
#include "tyr/common/types.hpp"

#include <boost/dynamic_bitset.hpp>
#include <cstddef>
#include <iterator>
#include <span>

namespace tyr
{
template<typename T>
class FlatDynamicBitset
{
    static_assert(IsFlatType<T>, "FlatDynamicBitset requires a flat type T");

public:
    FlatDynamicBitset() noexcept = default;

    void reset() noexcept { m_data.reset(); }

    void resize_to_fit(Index<T> index, bool default_value = false)
    {
        const auto value = index.get_value();

        if (value >= m_data.size())
            m_data.resize(value + 1, default_value);
    }

    void set(Index<T> index)
    {
        assert(index.get_value() < m_data.size());

        m_data.set(index.get_value());
    }

    bool test(Index<T> index) const
    {
        assert(index.get_value() < m_data.size());

        return m_data.test(index.get_value());
    }

private:
    boost::dynamic_bitset<> m_data;
};

template<typename T>
class GroupDynamicBitset
{
    static_assert(IsGroupType<T>, "GroupDynamicBitset requires a group type T");

public:
    GroupDynamicBitset() noexcept = default;

    void reset() noexcept
    {
        for (auto& bitset : m_data)
            bitset.reset();
    }

    void resize_to_fit(Index<T> index, bool default_value = false)
    {
        const auto value = index.get_value();
        const auto group = index.get_group().get_value();

        if (group >= m_data.size())
            m_data.resize(group + 1);

        if (value >= m_data[group].size())
            m_data[group].resize(value + 1, default_value);
    }

    void set(Index<T> index)
    {
        assert(index.get_group().get_value() < m_data.size() && index.get_value() < m_data[index.get_group().get_value()].size());

        m_data[index.get_group().get_value()].set(index.get_value());
    }

    bool test(Index<T> index) const
    {
        assert(index.get_group().get_value() < m_data.size() && index.get_value() < m_data[index.get_group().get_value()].size());

        return m_data[index.get_group().get_value()].test(index.get_value());
    }

private:
    std::vector<boost::dynamic_bitset<>> m_data;
};
}

#endif
