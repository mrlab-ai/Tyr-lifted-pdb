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

#ifndef TYR_ANALYSIS_VARIABLE_DOMAINS_HPP_
#define TYR_ANALYSIS_VARIABLE_DOMAINS_HPP_

#include "tyr/common/formatter.hpp"
#include "tyr/common/unordered_set.hpp"
#include "tyr/formalism/formalism.hpp"

#include <unordered_set>
#include <vector>

namespace tyr::analysis
{
using DomainSet = std::unordered_set<formalism::ObjectIndex, Hash<formalism::ObjectIndex>, EqualTo<formalism::ObjectIndex>>;
using DomainSetList = std::vector<DomainSet>;
using DomainSetListList = std::vector<DomainSetList>;

using DomainList = std::vector<formalism::ObjectIndex>;
using DomainListList = std::vector<DomainList>;
using DomainListListList = std::vector<DomainListList>;

struct VariableDomains
{
    DomainListListList static_predicate_domains;
    DomainListListList fluent_predicate_domains;
    DomainListListList rule_domains;
};

VariableDomains compute_variable_list_per_predicate(formalism::ProgramProxy<> program)
{
    const auto num_static_predicates = program.get_predicates<formalism::StaticTag>().size();
    const auto num_fluent_predicates = program.get_predicates<formalism::FluentTag>().size();

    auto objects = std::vector<formalism::ObjectIndex> {};
    for (const auto object : program.get_objects())
        objects.push_back(object.get_index());
    auto universe = DomainSet(objects.begin(), objects.end());

    ///--- Step 1: Initialize static and fluent predicate parameter domains

    auto static_predicate_domain_sets = DomainSetListList(num_static_predicates);
    for (const auto predicate : program.get_predicates<formalism::StaticTag>())
        static_predicate_domain_sets[predicate.get_index().value].resize(predicate.get_arity());

    auto fluent_predicate_domain_sets = DomainSetListList(num_fluent_predicates);
    for (const auto predicate : program.get_predicates<formalism::FluentTag>())
        fluent_predicate_domain_sets[predicate.get_index().value].resize(predicate.get_arity());

    for (const auto atom : program.get_atoms<formalism::StaticTag>())
    {
        const auto predicate = atom.get_predicate();
        auto pos = size_t { 0 };
        for (const auto& object : atom.get_terms())
            static_predicate_domain_sets[predicate.get_index().value][pos++].insert(object.get_index());
    }

    for (const auto atom : program.get_atoms<formalism::FluentTag>())
    {
        const auto predicate = atom.get_predicate();
        auto pos = size_t { 0 };
        for (const auto& object : atom.get_terms())
            fluent_predicate_domain_sets[predicate.get_index().value][pos++].insert(object.get_index());
    }

    ///--- Step 2: Compute rule parameter domains as tightest bound from the previously computed domains of the static predicates.

    auto func_restrict_rule_parameter_domain = [&](auto&& atom, auto&& parameter_domains)
    {
        const auto predicate = atom.get_predicate();

        auto pos = size_t { 0 };
        for (const auto term : atom.get_terms())
        {
            term.visit(
                [&](auto&& arg)
                {
                    using ProxyType = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_same_v<ProxyType, formalism::ObjectProxy<>>) {}
                    else if constexpr (std::is_same_v<ProxyType, formalism::ParameterIndex>)
                    {
                        const auto parameter_index = to_uint_t(arg);
                        auto& parameter_domain = parameter_domains[parameter_index];
                        auto& predicate_domain = static_predicate_domain_sets[predicate.get_index().value][pos];

                        intersect_inplace(parameter_domain, predicate_domain);
                    }
                    else
                    {
                        static_assert(dependent_false<ProxyType>::value, "Missing case");
                    }
                });
            ++pos;
        }
    };

    auto rule_domain_sets = DomainSetListList();
    {
        for (const auto rule : program.get_rules())
        {
            auto variables = rule.get_variables();
            auto parameter_domains = DomainSetList(variables.size(), universe);

            for (const auto literal : rule.get_static_body())
            {
                func_restrict_rule_parameter_domain(literal.get_atom(), parameter_domains);
            }

            rule_domain_sets.push_back(std::move(parameter_domains));
        }
    }

    ///--- Step 3: Lift the fluent predicate domains given the variable relationships in the rules.

    auto func_lift_fluent_domain = [&](auto&& atom, auto&& parameter_domains)
    {
        const auto predicate = atom.get_predicate();

        auto pos = size_t { 0 };
        for (const auto term : atom.get_terms())
        {
            term.visit(
                [&](auto&& arg)
                {
                    using ProxyType = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_same_v<ProxyType, formalism::ObjectProxy<>>) {}
                    else if constexpr (std::is_same_v<ProxyType, formalism::ParameterIndex>)
                    {
                        const auto parameter_index = to_uint_t(arg);
                        auto& parameter_domain = parameter_domains[parameter_index];
                        auto& predicate_domain = fluent_predicate_domain_sets[predicate.get_index().value][pos];

                        union_inplace(predicate_domain, parameter_domain);
                    }
                    else
                    {
                        static_assert(dependent_false<ProxyType>::value, "Missing case");
                    }
                });
            ++pos;
        }
    };

    for (const auto rule : program.get_rules())
    {
        auto& parameter_domains = rule_domain_sets[rule.get_index().value];

        for (const auto literal : rule.get_fluent_body())
        {
            func_lift_fluent_domain(literal.get_atom(), parameter_domains);
        }

        func_lift_fluent_domain(rule.get_head(), parameter_domains);
    }

    ///--- Step 4: Compress sets to vectors.
    auto static_predicate_domains = DomainListListList();
    static_predicate_domains.reserve(static_predicate_domain_sets.size());
    for (const auto& parameter_domains : static_predicate_domain_sets)
    {
        auto predicate_domains_vec = DomainListList();
        predicate_domains_vec.reserve(parameter_domains.size());
        for (const auto& parameter_domain : parameter_domains)
        {
            predicate_domains_vec.push_back(DomainList(parameter_domain.begin(), parameter_domain.end()));
        }
        static_predicate_domains.push_back(predicate_domains_vec);
    }

    auto fluent_predicate_domains = DomainListListList();
    fluent_predicate_domains.reserve(fluent_predicate_domain_sets.size());
    for (const auto& parameter_domains : fluent_predicate_domain_sets)
    {
        auto predicate_domains_vec = DomainListList();
        predicate_domains_vec.reserve(parameter_domains.size());
        for (const auto& parameter_domain : parameter_domains)
        {
            predicate_domains_vec.push_back(DomainList(parameter_domain.begin(), parameter_domain.end()));
        }
        fluent_predicate_domains.push_back(predicate_domains_vec);
    }

    auto rule_domains = DomainListListList();
    rule_domains.reserve(rule_domain_sets.size());
    for (const auto& parameter_domains : rule_domain_sets)
    {
        auto parameter_domains_vec = DomainListList();
        parameter_domains_vec.reserve(parameter_domains.size());
        for (const auto& parameter_domain : parameter_domains)
        {
            parameter_domains_vec.push_back(DomainList(parameter_domain.begin(), parameter_domain.end()));
        }
        rule_domains.push_back(parameter_domains_vec);
    }

    std::cout << "Static domains: " << "\n" << static_predicate_domains << std::endl;
    std::cout << "Fluent domains: " << "\n" << fluent_predicate_domains << std::endl;
    std::cout << "Rule domains: " << "\n" << rule_domains << std::endl;

    return VariableDomains { std::move(static_predicate_domains), std::move(fluent_predicate_domains), std::move(rule_domains) };
}
}

#endif