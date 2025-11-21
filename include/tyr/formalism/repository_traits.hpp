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

#ifndef TYR_FORMALISM_REPOSITORY_TRAITS_HPP_
#define TYR_FORMALISM_REPOSITORY_TRAITS_HPP_

#include "tyr/cista/declarations.hpp"
#include "tyr/formalism/declarations.hpp"

#include <boost/hana.hpp>

namespace tyr::formalism
{

/// @brief `FlatRepositoryEntry` is the mapping from data type to an indexed hash set.
template<typename T>
using FlatRepositoryEntry = boost::hana::pair<boost::hana::type<T>, cista::IndexedHashSet<T>>;

/// @brief `IndexedRepositoryEntry` is the mapping from data type to a list of indexed hash sets.
template<typename T>
using IndexedRepositoryEntry = boost::hana::pair<boost::hana::type<T>, cista::IndexedHashSetList<T>>;

template<typename T>
struct RepositoryTraits
{
    using EntryType = FlatRepositoryEntry<T>;
};

template<IsStaticOrFluentTag T>
struct RepositoryTraits<Atom<T>>
{
    using EntryType = IndexedRepositoryEntry<Atom<T>>;
};

template<IsStaticOrFluentTag T>
struct RepositoryTraits<GroundAtom<T>>
{
    using EntryType = IndexedRepositoryEntry<GroundAtom<T>>;
};

template<IsStaticOrFluentTag T>
struct RepositoryTraits<Literal<T>>
{
    using EntryType = IndexedRepositoryEntry<Literal<T>>;
};

template<IsStaticOrFluentTag T>
struct RepositoryTraits<FunctionTerm<T>>
{
    using EntryType = IndexedRepositoryEntry<FunctionTerm<T>>;
};

template<IsStaticOrFluentTag T>
struct RepositoryTraits<GroundFunctionTerm<T>>
{
    using EntryType = IndexedRepositoryEntry<GroundFunctionTerm<T>>;
};

template<IsStaticOrFluentTag T>
struct RepositoryTraits<GroundFunctionTermValue<T>>
{
    using EntryType = IndexedRepositoryEntry<GroundFunctionTermValue<T>>;
};

template<>
struct RepositoryTraits<GroundRule>
{
    using EntryType = IndexedRepositoryEntry<GroundRule>;
};

/// Checks whether T is defined to use a single indexed hash set.
template<typename T>
concept IsFlatRepository = std::same_as<typename RepositoryTraits<T>::EntryType, FlatRepositoryEntry<T>>;

/// Checks whether T is defined to use a list of indexed hash sets.
template<typename T>
concept IsIndexedRepository = std::same_as<typename RepositoryTraits<T>::EntryType, IndexedRepositoryEntry<T>>;

}

#endif