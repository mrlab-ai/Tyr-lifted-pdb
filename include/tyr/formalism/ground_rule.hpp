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

#ifndef TYR_FORMALISM_GROUND_RULE_HPP_
#define TYR_FORMALISM_GROUND_RULE_HPP_

#include "tyr/formalism/declarations.hpp"
#include "tyr/formalism/ground_literal.hpp"

namespace tyr::formalism
{
struct GroundRuleIndex
{
    uint_t value {};

    GroundRuleIndex() = default;
    explicit GroundRuleIndex(uint_t value) : value(value) {}

    uint_t get() const noexcept { return value; }

    auto cista_members() const noexcept { return std::tie(value); }
};

using GroundRuleIndexList = cista::offset::vector<GroundRuleIndex>;

struct GroundRuleImpl
{
    GroundRuleIndex index;
    GroundLiteralIndexList<StaticTag> static_body;
    GroundLiteralIndexList<FluentTag> fluent_body;
    GroundAtomIndex<FluentTag> head;

    using IndexType = GroundRuleIndex;

    GroundRuleImpl() = default;
    GroundRuleImpl(GroundRuleIndex index,
                   GroundLiteralIndexList<StaticTag> static_body,
                   GroundLiteralIndexList<FluentTag> fluent_body,
                   GroundAtomIndex<FluentTag> head) :
        index(index),
        static_body(std::move(static_body)),
        fluent_body(std::move(fluent_body)),
        head(head)
    {
    }

    auto cista_members() const noexcept { return std::tie(index, static_body, fluent_body, head); }
};

}

#endif
