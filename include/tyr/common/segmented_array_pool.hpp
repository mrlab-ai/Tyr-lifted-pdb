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

#ifndef TYR_COMMON_SEGMENTED_ARRAY_POOL_HPP_
#define TYR_COMMON_SEGMENTED_ARRAY_POOL_HPP_

#include <cstddef>
#include <vector>

namespace tyr
{

template<typename T>
class SegmentedArrayPool
{
private:
    void increase_capacity()
    {
        // 1) If current segment has space, we’re done.
        if (m_cur_seg < m_segments.size() && m_cur_pos + m_array_size <= m_segment_size)
        {
            return;
        }

        // 2) Next segment exists -> jump there
        if (m_cur_seg + 1 < m_segments.size())
        {
            m_cur_seg = m_cur_seg + 1;
            m_cur_pos = 0;
            return;
        }

        // 3) No existing segment fits -> allocate a new one.

        m_segments.emplace_back(m_segment_size);

        m_cur_seg = m_segments.size() - 1;
        m_cur_pos = 0;
    }

public:
    SegmentedArrayPool(size_t array_size, size_t num_arrays_per_segment = 1024) :
        m_array_size(array_size),
        m_num_arrays_per_segment(num_arrays_per_segment),
        m_segment_size(num_arrays_per_segment * array_size),
        m_cur_seg(0),
        m_cur_pos(0)
    {
    }

    T* allocate()
    {
        increase_capacity();

        T* result = &m_segments[m_cur_seg][m_cur_pos];

        m_cur_pos += m_array_size;

        return result;
    }

    void clear() noexcept
    {
        m_cur_seg = 0;
        m_cur_pos = 0;
    }

private:
    std::vector<std::vector<T>> m_segments;

    size_t m_array_size;
    size_t m_num_arrays_per_segment;
    size_t m_segment_size;

    size_t m_cur_seg;
    size_t m_cur_pos;
};

}

#endif
