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

#ifndef TYR_ANALYSIS_DECLARATIONS_HPP_
#define TYR_ANALYSIS_DECLARATIONS_HPP_

#include "tyr/common/declarations.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/formalism/declarations.hpp"
#include "tyr/formalism/object_index.hpp"

#include <vector>

namespace tyr::analysis
{
/**
 * Domains
 */

using DomainSet = UnorderedSet<Index<formalism::Object>>;
using DomainSetList = std::vector<DomainSet>;
using DomainSetListList = std::vector<DomainSetList>;

using DomainList = std::vector<Index<formalism::Object>>;
using DomainListList = std::vector<DomainList>;
using DomainListListList = std::vector<DomainListList>;

struct VariableDomain
{
    std::vector<Index<formalism::Object>> objects;

    auto begin() noexcept { return objects.begin(); }
    auto end() noexcept { return objects.end(); }
    auto begin() const noexcept { return objects.begin(); }
    auto end() const noexcept { return objects.end(); }

    auto size() const noexcept { return objects.size(); }
    bool empty() const noexcept { return objects.empty(); }

    auto& operator[](std::size_t i) noexcept { return objects[i]; }
    const auto& operator[](std::size_t i) const noexcept { return objects[i]; }
};

using VariableDomainList = std::vector<VariableDomain>;

template<formalism::FactKind T>
struct PredicateDomain
{
    VariableDomainList variable_domains;
};

template<formalism::FactKind T>

using PredicateDomainList = std::vector<PredicateDomain<T>>;

template<formalism::FactKind T>
struct FunctionDomain
{
    VariableDomainList variable_domains;
};

template<formalism::FactKind T>
using FunctionDomainList = std::vector<FunctionDomain<T>>;

struct RuleDomain
{
    VariableDomainList variable_domains;
};

using RuleDomainList = std::vector<RuleDomain>;

struct ConjunctiveConditionDomain
{
    VariableDomainList variable_domains;
};

struct ConjunctiveEffectDomain
{
    VariableDomainList variable_domains;
};

struct ConditionalEffectDomain
{
    VariableDomainList variable_domains;
};

using ConditionalEffectDomainList = std::vector<ConditionalEffectDomain>;

struct ActionDomain
{
    ConjunctiveConditionDomain precondition_domain;
    ConditionalEffectDomainList effect_domains;
};

using ActionDomainList = std::vector<ActionDomain>;

struct AxiomDomain
{
    ConjunctiveConditionDomain precondition_domain;
};

using AxiomDomainList = std::vector<AxiomDomain>;

struct ProgramVariableDomains
{
    PredicateDomainList<formalism::StaticTag> static_predicate_domains;
    PredicateDomainList<formalism::FluentTag> fluent_predicate_domains;
    FunctionDomainList<formalism::StaticTag> static_function_domains;
    FunctionDomainList<formalism::FluentTag> fluent_function_domains;
    RuleDomainList rule_domains;
};

struct TaskVariableDomains
{
    PredicateDomainList<formalism::StaticTag> static_predicate_domains;
    PredicateDomainList<formalism::FluentTag> fluent_predicate_domains;
    PredicateDomainList<formalism::DerivedTag> derived_predicate_domains;
    FunctionDomainList<formalism::StaticTag> static_function_domains;
    FunctionDomainList<formalism::FluentTag> fluent_function_domains;
    ActionDomainList action_domains;
    AxiomDomainList axiom_domains;
};

/**
 * Stratification
 */

struct RuleStrata;
}

#endif
