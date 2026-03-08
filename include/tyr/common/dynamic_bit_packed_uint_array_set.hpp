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

#ifndef TYR_COMMON_DYNAMIC_BIT_PACKED_UINT_ARRAY_SET_HPP_
#define TYR_COMMON_DYNAMIC_BIT_PACKED_UINT_ARRAY_SET_HPP_

#include "tyr/common/dynamic_bit_packed_uint_array_pool.hpp"

#include <concepts>

namespace tyr
{
template<std::unsigned_integral Block>
class DynamicBitPackedUintArraySet
{
public:
private:
    DynamicBitPackedUintArrayPool<Block> m_pool;
};
}

#endif
