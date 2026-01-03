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

#ifndef TYR_SOLVER_POLICIES_ANNOTATION_HPP_
#define TYR_SOLVER_POLICIES_ANNOTATION_HPP_

#include "tyr/common/config.hpp"
#include "tyr/formalism/binding_index.hpp"
#include "tyr/formalism/datalog/declarations.hpp"
#include "tyr/formalism/datalog/ground_atom_index.hpp"
#include "tyr/formalism/datalog/rule_index.hpp"

#include <concepts>
#include <tuple>
#include <vector>

namespace tyr::datalog
{
template<typename... Ts>
using Annotation = std::tuple<Ts...>;

template<typename... Ts>
using AnnotationList = std::vector<Annotation<Ts...>>;

// circle "or"-node
template<typename T>
concept OrAnnotationPolicy = requires(T& p, Index<formalism::datalog::GroundAtom<formalism::FluentTag>> atom, uint_t cost) {
    { p.annotate(atom, cost) } -> std::same_as<void>;
    { p.clear() } -> std::same_as<void>;
};

// rectangular "and"-node
template<typename T>
concept AndAnnotationPolicy = requires(T& p, Index<formalism::datalog::Rule> rule, const IndexList<formalism::Object>& binding, uint_t cost) {
    { p.annotate(rule, binding, cost) } -> std::same_as<void>;
    { p.clear() } -> std::same_as<void>;
};

class NoOrAnnotationPolicy
{
public:
    void annotate(Index<formalism::datalog::GroundAtom<formalism::FluentTag>> atom, uint_t cost) const noexcept {}
    void clear() noexcept {}
};

class NoAndAnnotationPolicy
{
public:
    void annotate(Index<formalism::datalog::Rule> rule, const IndexList<formalism::Object>& binding, uint_t cost) const noexcept {}
    void clear() noexcept {}
};

template<OrAnnotationPolicy OrAP, AndAnnotationPolicy AndAP>
struct AnnotationPolicies
{
    OrAP or_ap;
    std::vector<AndAP> and_aps;

    AnnotationPolicies(OrAP or_ap, std::vector<AndAP> and_aps) : or_ap(std::move(or_ap)), and_aps(std::move(and_aps)) {}
};

}

#endif
