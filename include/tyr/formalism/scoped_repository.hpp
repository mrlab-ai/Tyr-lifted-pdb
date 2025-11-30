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

#ifndef TYR_FORMALISM_SCOPED_REPOSITORY_HPP_
#define TYR_FORMALISM_SCOPED_REPOSITORY_HPP_

#include "tyr/formalism/declarations.hpp"
#include "tyr/formalism/repository.hpp"

namespace tyr::formalism
{
template<Context C>
class ScopedRepository
{
private:
    const C& parent_scope;
    C& local_scope;

public:
    ScopedRepository(const C& parent_scope, C& local_scope) : parent_scope(parent_scope), local_scope(local_scope) {}

    template<typename T>
    std::optional<View<Index<T>, ScopedRepository<C>>> find(const Data<T>& builder) const
    {
        if (auto ptr = parent_scope.find(builder))
            return View<Index<T>, ScopedRepository<C>>(ptr->get_index(), *this);

        if (auto ptr = local_scope.find(builder))
            return View<Index<T>, ScopedRepository<C>>(ptr->get_index(), *this);

        return std::nullopt;
    }

    template<typename T, bool AssignIndex = true>
    std::pair<View<Index<T>, ScopedRepository<C>>, bool> get_or_create(Data<T>& builder, buffer::Buffer& buf)
    {
        if (auto ptr = parent_scope.find(builder))
            return std::make_pair(View<Index<T>, ScopedRepository<C>>(ptr->get_index(), *this), false);

        // Manually assign index to continue indexing.
        builder.index.value = parent_scope.template size<T>() + local_scope.template size<T>();

        return std::make_pair(View<Index<T>, ScopedRepository<C>>(local_scope.template get_or_create<T, false>(builder, buf).first.get_index(), *this), true);
    }

    template<typename T>
    const Data<T>& operator[](Index<T> index) const
    {
        const auto parent_scope_size = parent_scope.template size<T>();

        if (index.value < parent_scope_size)
            return parent_scope[index];

        // Subtract parent_scope size to get position in local_scope storage
        index.value -= parent_scope_size;
        return local_scope[index];
    }

    template<typename T>
    size_t size() const
    {
        return parent_scope.template size<T>() + local_scope.template size<T>();
    }
};

/// @brief Make ScopedRepository a trivial context.
template<Context C>
inline const ScopedRepository<C>& get_repository(const ScopedRepository<C>& context) noexcept
{
    return context;
}

// Domain + Task
static_assert(IsRepository<ScopedRepository<Repository>>);
static_assert(Context<ScopedRepository<Repository>>);

// Domain + Task + Worker threads
static_assert(IsRepository<ScopedRepository<ScopedRepository<Repository>>>);
static_assert(Context<ScopedRepository<ScopedRepository<Repository>>>);

}

#endif
