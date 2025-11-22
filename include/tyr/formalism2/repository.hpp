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

#ifndef TYR_FORMALISM2_REPOSITORY_HPP_
#define TYR_FORMALISM2_REPOSITORY_HPP_

// Include specialization headers first
#include "tyr/formalism2/atom_data.hpp"
#include "tyr/formalism2/atom_index.hpp"
#include "tyr/formalism2/object_data.hpp"
#include "tyr/formalism2/object_index.hpp"
#include "tyr/formalism2/predicate_data.hpp"
#include "tyr/formalism2/predicate_index.hpp"
#include "tyr/formalism2/variable_data.hpp"
#include "tyr/formalism2/variable_index.hpp"
//
#include "tyr/cista/declarations.hpp"
#include "tyr/cista/indexed_hash_set.hpp"

// #include "tyr/formalism/atom.hpp"
// #include "tyr/formalism/binary_operator.hpp"
// #include "tyr/formalism/declarations.hpp"
// #include "tyr/formalism/function.hpp"
// #include "tyr/formalism/function_expression.hpp"
// #include "tyr/formalism/function_term.hpp"
// #include "tyr/formalism/ground_atom.hpp"
// #include "tyr/formalism/ground_function_expression.hpp"
// #include "tyr/formalism/ground_function_term.hpp"
// #include "tyr/formalism/ground_function_term_value.hpp"
// #include "tyr/formalism/ground_literal.hpp"
// #include "tyr/formalism/ground_rule.hpp"
// #include "tyr/formalism/literal.hpp"
// #include "tyr/formalism/multi_operator.hpp"
// #include "tyr/formalism/object.hpp"
// #include "tyr/formalism/predicate.hpp"
// #include "tyr/formalism/program.hpp"
// #include "tyr/formalism/rule.hpp"
// #include "tyr/formalism/term.hpp"
// #include "tyr/formalism/unary_operator.hpp"
#include "tyr/formalism2/declarations.hpp"

#include <boost/hana.hpp>

namespace tyr::formalism
{

class Repository
{
private:
    /// @brief `FlatRepositoryEntry` is the mapping from data type to an indexed hash set.
    template<typename T>
    using FlatRepositoryEntry = boost::hana::pair<boost::hana::type<T>, cista::IndexedHashSet<T>>;

    /// @brief `GroupRepositoryEntry` is the mapping from data type to a list of indexed hash sets.
    template<typename T>
    using GroupRepositoryEntry = boost::hana::pair<boost::hana::type<T>, cista::IndexedHashSetList<T>>;

    template<typename T>
    struct RepositoryTraits
    {
        using EntryType = std::conditional_t<IsGroupType<T>, GroupRepositoryEntry<T>, FlatRepositoryEntry<T>>;
    };

    using HanaRepository = boost::hana::map<RepositoryTraits<Variable>::EntryType,
                                            RepositoryTraits<Object>::EntryType,
                                            RepositoryTraits<Predicate<StaticTag>>::EntryType,
                                            RepositoryTraits<Predicate<FluentTag>>::EntryType,
                                            RepositoryTraits<Atom<StaticTag>>::EntryType,
                                            RepositoryTraits<Atom<FluentTag>>::EntryType>;

    HanaRepository m_repository;

public:
    Repository() = default;

    // nullptr signals that the object does not exist.
    template<IsGroupType T>
    const Data<T>* find(const Data<T>& builder) const
    {
        const auto& list = boost::hana::at_key(m_repository, boost::hana::type<T> {});

        const auto i = builder.index.get_group().get_value();

        if (i >= list.size())
            return nullptr;

        const auto& indexed_hash_set = list[i];

        return indexed_hash_set.find(builder);
    }

    // nullptr signals that the object does not exist.
    template<IsFlatType T>
    const Data<T>* find(const Data<T>& builder) const
    {
        const auto& indexed_hash_set = boost::hana::at_key(m_repository, boost::hana::type<T> {});

        return indexed_hash_set.find(builder);
    }

    // const T* always points to a valid instantiation of the class.
    // We return const T* here to avoid bugs when using structured bindings.
    template<IsGroupType T, bool AssignIndex = true>
    std::pair<const Data<T>*, bool> get_or_create(Data<T>& builder, cista::Buffer& buf)
    {
        auto& list = boost::hana::at_key(m_repository, boost::hana::type<T> {});

        const auto i = builder.index.get_group().get_value();

        if (i >= list.size())
            list.resize(i + 1);

        auto& indexed_hash_set = list[i];

        if constexpr (AssignIndex)
            builder.index.value = indexed_hash_set.size();

        return indexed_hash_set.insert(builder, buf);
    }

    // const T* always points to a valid instantiation of the class.
    // We return const T* here to avoid bugs when using structured bindings.
    template<IsFlatType T, bool AssignIndex = true>
    std::pair<const Data<T>*, bool> get_or_create(Data<T>& builder, cista::Buffer& buf)
    {
        auto& indexed_hash_set = boost::hana::at_key(m_repository, boost::hana::type<T> {});

        if constexpr (AssignIndex)
            builder.index.value = indexed_hash_set.size();

        return indexed_hash_set.insert(builder, buf);
    }

    /// @brief Access the element with the given index.
    template<IsGroupType T>
    const Data<T>& operator[](Index<T> index) const
    {
        const auto& list = boost::hana::at_key(m_repository, boost::hana::type<T> {});

        const auto i = index.get_group().get_value();

        assert(i < list.size());

        const auto& repository = list[i];

        return repository[index];
    }

    /// @brief Access the element with the given index.
    template<IsFlatType T>
    const Data<T>& operator[](Index<T> index) const
    {
        const auto& repository = boost::hana::at_key(m_repository, boost::hana::type<T> {});

        return repository[index];
    }

    /// @brief Get the number of stored elements.
    template<IsGroupType T>
    size_t size(Index<T> index) const
    {
        const auto& list = boost::hana::at_key(m_repository, boost::hana::type<T> {});

        const auto i = index.get_group().get_value();

        assert(i < list.size());

        const auto& repository = list[i];

        return repository.size();
    }

    /// @brief Get the number of stored elements.
    template<IsFlatType T>
    size_t size() const
    {
        const auto& repository = boost::hana::at_key(m_repository, boost::hana::type<T> {});

        return repository.size();
    }
};

static_assert(IsFlatType<Variable>);

static_assert(IsRepository<Repository>);

static_assert(IsContext<Repository>);
}

#endif
