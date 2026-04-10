/*
 * Copyright (C) 2025-2026 Dominik Drexler
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

#ifndef TYR_PLANNING_PROGRAMS_TRANSLATION_CONTEXT_HPP_
#define TYR_PLANNING_PROGRAMS_TRANSLATION_CONTEXT_HPP_

#include "tyr/common/declarations.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/formalism/datalog/declarations.hpp"
#include "tyr/formalism/datalog/repository.hpp"
#include "tyr/formalism/planning/declarations.hpp"
#include "tyr/formalism/planning/repository.hpp"

namespace tyr::planning
{

struct D2PTranslationContext
{
    using StaticToStaticPredicateMapping =
        UnorderedMap<formalism::datalog::PredicateView<formalism::StaticTag>, formalism::planning::PredicateView<formalism::StaticTag>>;
    using FluentToFluentPredicateMapping =
        UnorderedMap<formalism::datalog::PredicateView<formalism::FluentTag>, formalism::planning::PredicateView<formalism::FluentTag>>;
    using FluentToDerivedPredicateMapping =
        UnorderedMap<formalism::datalog::PredicateView<formalism::FluentTag>, formalism::planning::PredicateView<formalism::DerivedTag>>;

    StaticToStaticPredicateMapping static_to_static_predicate;
    FluentToFluentPredicateMapping fluent_to_fluent_predicate;
    FluentToDerivedPredicateMapping fluent_to_derived_predicate;
};

struct P2DTranslationContext
{
    using StaticToStaticPredicateMapping =
        UnorderedMap<formalism::planning::PredicateView<formalism::StaticTag>, formalism::datalog::PredicateView<formalism::StaticTag>>;
    using FluentToFluentPredicateMapping =
        UnorderedMap<formalism::planning::PredicateView<formalism::FluentTag>, formalism::datalog::PredicateView<formalism::FluentTag>>;
    using DerivedToFluentPredicateMapping =
        UnorderedMap<formalism::planning::PredicateView<formalism::DerivedTag>, formalism::datalog::PredicateView<formalism::FluentTag>>;

    StaticToStaticPredicateMapping static_to_static_predicate;
    FluentToFluentPredicateMapping fluent_to_fluent_predicate;
    DerivedToFluentPredicateMapping derived_to_fluent_predicate;
};

struct TranslationContext
{
    D2PTranslationContext d2p;
    P2DTranslationContext p2d;
};

}

#endif
