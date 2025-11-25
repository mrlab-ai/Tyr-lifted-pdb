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

#ifndef TYR_COMMON_TYPES_HPP_
#define TYR_COMMON_TYPES_HPP_

#include "tyr/common/declarations.hpp"

#include <cista/containers/variant.h>
#include <cista/containers/vector.h>

namespace tyr
{

template<typename T>
struct Data
{
    static_assert(dependent_false<T>::value, "Data is not defined for type T.");
};

template<typename T>
using DataList = ::cista::offset::vector<Data<T>>;

template<typename T>
struct Index
{
    static_assert(dependent_false<T>::value, "Index is not defined for type T.");
};

template<typename T>
using IndexList = ::cista::offset::vector<Index<T>>;

template<typename T, typename Context>
struct Proxy
{
    // Proxy type is optional for recursing through data using the Context.
};

template<typename T, typename C>
concept IsProxyable = requires(T type, const C& context) { Proxy<T, C>(type, context); };

/**
 * Experimental design: We distinguish between Flat and Group Indices.
 *
 * Flat indices contain a value index uint_t.
 * Group indices contain a group index uint_t and a flat index.
 *
 * Data corresponding to Flat indices is stored in a single IndexedHashSet,
 * while Data corresponding to group indices is stored in per group in a list of IndexedHashSets.
 *
 * For example, predicates are stored using flat indices, while atoms are stored using group indices.
 * The group index for atoms is the corresponding predicate.
 */

template<typename T>
concept HasValue = requires(const T& a) {
    { a.get_value() } -> std::same_as<uint_t>;
};

template<typename T>
concept HasGroup = requires(const T& a) {
    { a.get_group() } -> HasValue;
};

/// @brief Check whether T is a flat type.
template<typename T>
concept IsFlatType = HasValue<Index<T>> && !HasGroup<Index<T>>;

/// @brief Check whether T is a group type.
template<typename T>
concept IsGroupType = HasValue<Index<T>> && HasGroup<Index<T>>;

template<typename Derived>
struct FlatIndexMixin;

template<typename Derived, HasValue Group>
struct GroupIndexMixin;

}

#endif