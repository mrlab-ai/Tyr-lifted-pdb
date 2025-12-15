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

#ifndef TYR_FORMALISM_PLANNING_ACTION_DATA_HPP_
#define TYR_FORMALISM_PLANNING_ACTION_DATA_HPP_

#include "tyr/common/types.hpp"
#include "tyr/common/types_utils.hpp"
#include "tyr/formalism/declarations.hpp"
#include "tyr/formalism/planning/action_index.hpp"
#include "tyr/formalism/planning/conditional_effect_index.hpp"
#include "tyr/formalism/planning/fdr_conjunctive_condition_index.hpp"

namespace tyr
{

template<>
struct Data<formalism::Action>
{
    using Tag = formalism::Action;

    Index<formalism::Action> index;
    ::cista::offset::string name;
    uint_t original_arity;
    Index<formalism::FDRConjunctiveCondition> condition;
    IndexList<formalism::ConditionalEffect> effects;

    Data() = default;
    Data(Index<formalism::Action> index,
         ::cista::offset::string name,
         uint_t original_arity,
         Index<formalism::FDRConjunctiveCondition> condition,
         IndexList<formalism::ConditionalEffect> effects) :
        index(index),
        name(std::move(name)),
        condition(condition),
        effects(std::move(effects))
    {
    }
    Data(const Data& other) = delete;
    Data& operator=(const Data& other) = delete;
    Data(Data&& other) = default;
    Data& operator=(Data&& other) = default;

    void clear() noexcept
    {
        tyr::clear(index);
        tyr::clear(name);
        tyr::clear(condition);
        tyr::clear(effects);
    }

    auto cista_members() const noexcept { return std::tie(index, name, original_arity, condition, effects); }
    auto identifying_members() const noexcept { return std::tie(name, original_arity, condition, effects); }
};
}

#endif
