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

#ifndef TYR_COMMON_DYNAMIC_BIT_PACKED_UINT_ARRAY_POOL_HPP_
#define TYR_COMMON_DYNAMIC_BIT_PACKED_UINT_ARRAY_POOL_HPP_

#include <bit>
#include <concepts>
#include <cstddef>
#include <limits>

namespace tyr
{
// The idea is to store elements in a relation bit packed and deduplicate via a set.
// This will save roughly 7 times memory over the current cista based approach for arity 3 relations.
template<typename T, std::unsigned_integral Block>
class DynamicBitPackedUintArrayPool
{
public:
    static constexpr uint8_t MIN_WIDTH = 8;
    static constexpr uint8_t MAX_WIDTH = std::numeric_limits<Block>::digits();

    class ArrayView;

    auto get_segment_index(size_t index) const noexcept { return std::bit_width(index + 1) - 1; }
    auto get_segment_pos(size_t index) const noexcept { return index - ((size_t { 1 } << get_segment_index(index)) - 1); }

    auto operator[](size_t index) const noexcept { return ArrayView(index, *this); }

    auto length() const noexcept { return m_length; }
    auto width() const noexcept { return m_width; }
    auto capacity() const noexcept { return m_capacity; }
    auto size() const noexcept { return m_size; }

    explicit DynamicBitPackedUintArrayPool(size_t length) : m_length(length), m_width(MIN_WIDTH) {}

    class ArrayView
    {
    public:
        ArrayView(const DynamicBitPackedUintArrayPool& context, size_t index) : m_context(&context), m_index(index) {}

        // TODO: implement standard array interface

        T operator[](size_t pos) const noexcept { return static_cast<T>() }

    private:
        const DynamicBitPackedUintArrayPool* m_context;
        size_t m_index;
    };

private:
    // Segments grow geometrically, i.e., 1, 2, 4, 8, 16, 32, 64, 128 to make index access easy
    std::vector<std::vector<Block>> m_segments;

    size_t m_length;
    size_t m_width;

    size_t m_capacity;
    size_t m_size;
};
}

#endif
