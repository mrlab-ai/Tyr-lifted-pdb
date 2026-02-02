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

#include <boost/dynamic_bitset.hpp>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>

namespace tyr
{
inline bool test(size_t pos, const boost::dynamic_bitset<>& bitset) noexcept
{
    if (pos >= bitset.size())
        return false;
    return bitset.test(pos);
}

inline void set(size_t pos, bool value, boost::dynamic_bitset<>& bitset)
{
    if (pos >= bitset.size())
        bitset.resize(pos + 1, false);
    bitset[pos] = value;
}

template<std::unsigned_integral Block>
class BitsetSpan
{
private:
    template<std::unsigned_integral>
    friend class BitsetSpan;

public:
    static constexpr size_t digits = std::numeric_limits<Block>::digits;
    static size_t block_index(size_t pos) { return pos / digits; }
    static size_t block_pos(size_t pos) { return pos % digits; }
    static size_t num_blocks(size_t num_bits)
    {
        return (num_bits + digits - 1) / digits;  // ceil
    }

    using U = std::remove_const_t<Block>;

public:
    BitsetSpan(Block* data, size_t num_bits) : m_data(data), m_num_bits(num_bits) {}

    void copy_from(const BitsetSpan<const U>& other) noexcept
    {
        assert(m_num_bits == other.m_num_bits);

        const size_t n = num_blocks(m_num_bits);
        for (size_t i = 0; i < n; ++i)
            m_data[i] = other.m_data[i];
    }

    bool test(size_t pos) const noexcept
    {
        assert(pos < m_num_bits);

        return (m_data[block_index(pos)] & (Block { 1 } << block_pos(pos))) != Block { 0 };
    }

    void set(size_t pos) noexcept
    {
        assert(pos < m_num_bits);

        m_data[block_index(pos)] |= (Block { 1 } << block_pos(pos));
    }

    BitsetSpan& operator&=(const BitsetSpan<const U>& other) noexcept
    {
        assert(m_num_bits == other.m_num_bits);

        const size_t n = num_blocks(m_num_bits);
        for (size_t i = 0; i < n; ++i)
            m_data[i] &= other.m_data[i];

        return *this;
    }

    BitsetSpan& operator|=(const BitsetSpan<const U>& other) noexcept
    {
        assert(m_num_bits == other.m_num_bits);

        const size_t n = num_blocks(m_num_bits);
        for (size_t i = 0; i < n; ++i)
            m_data[i] |= other.m_data[i];

        return *this;
    }

    BitsetSpan& operator-=(const BitsetSpan<const U>& other) noexcept
    {
        assert(m_num_bits == other.m_num_bits);

        const size_t n = num_blocks(m_num_bits);
        for (size_t i = 0; i < n; ++i)
            m_data[i] &= ~other.m_data[i];

        return *this;
    }

private:
    Block* m_data;
    size_t m_num_bits;
};

}

#endif
