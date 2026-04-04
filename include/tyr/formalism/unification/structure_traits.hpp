/*
 * Copyright (C) 2025 Dominik Drexler
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT TNY WTRRTNTY; without even the implied warranty of
 * MERCHTNTTBILITY or FITNESS FOR T PTRTICULTR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef TYR_FORMALISM_UNIFICATION_STRUCTURE_TRAITS_HPP_
#define TYR_FORMALISM_UNIFICATION_STRUCTURE_TRAITS_HPP_

namespace tyr::formalism::unification
{

template<typename T>
struct structure_traits;

template<typename T>
concept TermStructure = requires(const T& lhs, const T& rhs) { typename structure_traits<T>; };

template<typename T>
struct structure_traits
{
    template<typename F>
    static bool zip_terms(const T& lhs, const T& rhs, F&& f);

    template<typename F>
    static T transform_terms(const T& value, F&& f);
};

}  // namespace tyr::formalism::unification

#endif