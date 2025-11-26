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

#ifndef TYR_FORMALISM_ATOM_VIEW_HPP_
#define TYR_FORMALISM_ATOM_VIEW_HPP_

#include "tyr/common/types.hpp"
#include "tyr/common/vector.hpp"
#include "tyr/formalism/atom_index.hpp"
#include "tyr/formalism/declarations.hpp"
#include "tyr/formalism/predicate_view.hpp"
#include "tyr/formalism/repository.hpp"
#include "tyr/formalism/term_data.hpp"

namespace tyr
{
template<formalism::IsStaticOrFluentTag T, formalism::IsContext C>
class View<Index<formalism::Atom<T>>, C>
{
private:
    const C* context;
    Index<formalism::Atom<T>> index;

public:
    using Tag = formalism::Atom<T>;

    View(Index<formalism::Atom<T>> index, const C& context) : context(&context), index(index) {}

    const auto& get() const { return get_repository(*context)[index]; }

    auto get_index() const { return index; }
    auto get_predicate() const { return View<Index<formalism::Predicate<T>>, C>(index.group, *context); }
    auto get_terms() const { return View<DataList<formalism::Term>, C>(get().terms, *context); }
};
}

#endif
