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

#ifndef TYR_SOLVER_POLICIES_CARE_HPP_
#define TYR_SOLVER_POLICIES_CARE_HPP_

#include "tyr/common/closed_interval.hpp"
#include "tyr/datalog/assignment.hpp"
#include "tyr/datalog/assignment_sets.hpp"
#include "tyr/datalog/fact_sets.hpp"
#include "tyr/datalog/policies/care_concept.hpp"
#include "tyr/datalog/workspaces/facts.hpp"
#include "tyr/formalism/datalog/declarations.hpp"
#include "tyr/formalism/datalog/grounder_decl.hpp"
#include "tyr/formalism/datalog/repository.hpp"

namespace tyr::datalog
{

/**
 * Fact-set subpolicies
 */

template<formalism::FactKind T>
struct NoCarePredicateFactSetsPolicy
{
    const PredicateFactSets<T>& fact_sets;

    bool check(formalism::datalog::LiteralView<T> element, const formalism::datalog::GrounderContext& context) const;

    bool check(formalism::datalog::GroundLiteralView<T> element) const;
};

template<formalism::FactKind T>
struct NoCareFunctionFactSetsPolicy
{
    const FunctionFactSets<T>& fact_sets;

    float_t check(formalism::datalog::FunctionTermView<T> element, const formalism::datalog::GrounderContext& context) const;

    float_t check(formalism::datalog::GroundFunctionTermView<T> element) const;
};

template<formalism::FactKind T>
struct CarePredicateFactSetsPolicy
{
    const PredicateFactSets<T>& fact_sets;

    bool check(formalism::datalog::LiteralView<T> element, const formalism::datalog::GrounderContext& context) const
    {
        return NoCarePredicateFactSetsPolicy<T> { fact_sets }.check(element, context);
    }

    bool check(formalism::datalog::GroundLiteralView<T> element) const { return NoCarePredicateFactSetsPolicy<T> { fact_sets }.check(element); }
};

template<>
struct CarePredicateFactSetsPolicy<formalism::FluentTag>
{
    const PredicateFactSets<formalism::FluentTag>& fact_sets;
    const PredicateFactSets<formalism::FluentTag>& care_fact_sets;

    bool check(formalism::datalog::LiteralView<formalism::FluentTag> element, const formalism::datalog::GrounderContext& context) const;

    bool check(formalism::datalog::GroundLiteralView<formalism::FluentTag> element) const;
};

template<formalism::FactKind T>
using CareFunctionFactSetsPolicy = NoCareFunctionFactSetsPolicy<T>;

/**
 * Assignment-set subpolicies
 */

template<formalism::FactKind T>
struct NoCarePredicateAssignmentSetPolicy
{
    const PredicateAssignmentSets<T>& assignment_sets;

    struct Checker
    {
        const PredicateAssignmentSet<T>& set;

        template<typename Assignment>
        bool is_consistent(const Assignment& assignment, bool polarity) const;
    };

    Checker make_checker(Index<formalism::Predicate<T>> predicate) const { return Checker { assignment_sets.at(predicate) }; }
};

template<formalism::FactKind T>
struct CarePredicateAssignmentSetPolicy
{
    const PredicateAssignmentSets<T>& assignment_sets;

    struct Checker
    {
        const PredicateAssignmentSet<T>& set;

        template<typename Assignment>
        bool is_consistent(const Assignment& assignment, bool polarity) const;
    };

    Checker make_checker(Index<formalism::Predicate<T>> predicate) const { return Checker { assignment_sets.at(predicate) }; }
};

template<>
struct CarePredicateAssignmentSetPolicy<formalism::FluentTag>
{
    const PredicateAssignmentSets<formalism::FluentTag>& assignment_sets;
    const PredicateAssignmentSets<formalism::FluentTag>& care_assignment_sets;

    struct Checker
    {
        const PredicateAssignmentSet<formalism::FluentTag>& set;
        const PredicateAssignmentSet<formalism::FluentTag>& care_set;

        template<typename Assignment>
        bool is_consistent(const Assignment& assignment, bool polarity) const;
    };

    Checker make_checker(Index<formalism::Predicate<formalism::FluentTag>> predicate) const
    {
        return Checker { assignment_sets.at(predicate), care_assignment_sets.at(predicate) };
    }
};

template<formalism::FactKind T>
struct NoCareFunctionAssignmentSetPolicy
{
    const FunctionAssignmentSets<T>& assignment_sets;

    struct Checker
    {
        const FunctionAssignmentSet<T>& set;

        bool intersect_interval(const VertexAssignment& assignment, ClosedInterval<float_t>& interval) const;

        bool intersect_interval(const EdgeAssignment& assignment, ClosedInterval<float_t>& interval) const;
    };

    Checker make_checker(Index<formalism::Function<T>> function) const { return Checker { assignment_sets.at(function) }; }
};

template<formalism::FactKind T>
using CareFunctionAssignmentSetPolicy = NoCareFunctionAssignmentSetPolicy<T>;

/**
 * Tagged wrappers
 */

template<formalism::FactKind T>
struct TaggedNoCareFactSetPolicy
{
    NoCarePredicateFactSetsPolicy<T> predicate;
    NoCareFunctionFactSetsPolicy<T> function;

    explicit TaggedNoCareFactSetPolicy(const TaggedFactSets<T>& fact_sets) : predicate { fact_sets.predicate }, function { fact_sets.function } {}
};

template<formalism::FactKind T>
struct TaggedNoCareAssignmentSetPolicy
{
    NoCarePredicateAssignmentSetPolicy<T> predicate;
    NoCareFunctionAssignmentSetPolicy<T> function;

    explicit TaggedNoCareAssignmentSetPolicy(const TaggedAssignmentSets<T>& assignment_sets) :
        predicate { assignment_sets.predicate },
        function { assignment_sets.function }
    {
    }
};

template<formalism::FactKind T>
struct TaggedCareFactSetPolicy;

template<>
struct TaggedCareFactSetPolicy<formalism::StaticTag>
{
    NoCarePredicateFactSetsPolicy<formalism::StaticTag> predicate;
    NoCareFunctionFactSetsPolicy<formalism::StaticTag> function;

    explicit TaggedCareFactSetPolicy(const TaggedFactSets<formalism::StaticTag>& fact_sets) : predicate { fact_sets.predicate }, function { fact_sets.function }
    {
    }
};

template<>
struct TaggedCareFactSetPolicy<formalism::FluentTag>
{
    CarePredicateFactSetsPolicy<formalism::FluentTag> predicate;
    NoCareFunctionFactSetsPolicy<formalism::FluentTag> function;

    TaggedCareFactSetPolicy(const TaggedFactSets<formalism::FluentTag>& fact_sets, const PredicateFactSets<formalism::FluentTag>& care_fact_sets) :
        predicate { fact_sets.predicate, care_fact_sets },
        function { fact_sets.function }
    {
    }
};

template<formalism::FactKind T>
struct TaggedCareAssignmentSetPolicy;

template<>
struct TaggedCareAssignmentSetPolicy<formalism::StaticTag>
{
    NoCarePredicateAssignmentSetPolicy<formalism::StaticTag> predicate;
    NoCareFunctionAssignmentSetPolicy<formalism::StaticTag> function;

    explicit TaggedCareAssignmentSetPolicy(const TaggedAssignmentSets<formalism::StaticTag>& assignment_sets) :
        predicate { assignment_sets.predicate },
        function { assignment_sets.function }
    {
    }
};

template<>
struct TaggedCareAssignmentSetPolicy<formalism::FluentTag>
{
    CarePredicateAssignmentSetPolicy<formalism::FluentTag> predicate;
    NoCareFunctionAssignmentSetPolicy<formalism::FluentTag> function;

    TaggedCareAssignmentSetPolicy(const TaggedAssignmentSets<formalism::FluentTag>& assignment_sets,
                                  const PredicateAssignmentSets<formalism::FluentTag>& care_assignment_sets) :
        predicate { assignment_sets.predicate, care_assignment_sets },
        function { assignment_sets.function }
    {
    }
};

/**
 * Combined wrappers
 */

struct NoCareFactSetPolicy
{
    TaggedNoCareFactSetPolicy<formalism::StaticTag> static_policy;
    TaggedNoCareFactSetPolicy<formalism::FluentTag> fluent_policy;

    NoCareFactSetPolicy(const ConstFactsWorkspace& cws, const FactsWorkspace& ws) : static_policy { cws.fact_sets }, fluent_policy { ws.fact_sets } {}

    template<formalism::FactKind T>
    const auto& get() const noexcept
    {
        if constexpr (std::is_same_v<T, formalism::StaticTag>)
            return static_policy;
        else
            return fluent_policy;
    }
};

struct CareFactSetPolicy
{
    TaggedCareFactSetPolicy<formalism::StaticTag> static_policy;
    TaggedCareFactSetPolicy<formalism::FluentTag> fluent_policy;

    CareFactSetPolicy(const ConstFactsWorkspace& cws, const FactsWorkspace& ws) :
        static_policy { cws.fact_sets },
        fluent_policy { ws.fact_sets, ws.care_fact_sets }
    {
    }

    template<formalism::FactKind T>
    const auto& get() const noexcept
    {
        if constexpr (std::is_same_v<T, formalism::StaticTag>)
            return static_policy;
        else
            return fluent_policy;
    }
};

struct NoCareAssignmentSetPolicy
{
    TaggedNoCareAssignmentSetPolicy<formalism::StaticTag> static_policy;
    TaggedNoCareAssignmentSetPolicy<formalism::FluentTag> fluent_policy;

    NoCareAssignmentSetPolicy(const ConstFactsWorkspace& cws, const FactsWorkspace& ws) :
        static_policy { cws.assignment_sets },
        fluent_policy { ws.assignment_sets }
    {
    }

    template<formalism::FactKind T>
    const auto& get() const noexcept
    {
        if constexpr (std::is_same_v<T, formalism::StaticTag>)
            return static_policy;
        else
            return fluent_policy;
    }
};

struct CareAssignmentSetPolicy
{
    TaggedCareAssignmentSetPolicy<formalism::StaticTag> static_policy;
    TaggedCareAssignmentSetPolicy<formalism::FluentTag> fluent_policy;

    CareAssignmentSetPolicy(const ConstFactsWorkspace& cws, const FactsWorkspace& ws) :
        static_policy { cws.assignment_sets },
        fluent_policy { ws.assignment_sets, ws.care_assignment_sets }
    {
    }

    template<formalism::FactKind T>
    const auto& get() const noexcept
    {
        if constexpr (std::is_same_v<T, formalism::StaticTag>)
            return static_policy;
        else
            return fluent_policy;
    }
};

/**
 * Top-level bundles
 */

struct NoCarePolicy
{
    using FactSetPolicy = NoCareFactSetPolicy;
    using AssignmentSetPolicy = NoCareAssignmentSetPolicy;

    static FactSetPolicy make_fact_set_policy(const ConstFactsWorkspace& cws, const FactsWorkspace& ws) { return FactSetPolicy { cws, ws }; }

    static AssignmentSetPolicy make_assignment_set_policy(const ConstFactsWorkspace& cws, const FactsWorkspace& ws) { return AssignmentSetPolicy { cws, ws }; }
};

struct CarePolicy
{
    using FactSetPolicy = CareFactSetPolicy;
    using AssignmentSetPolicy = CareAssignmentSetPolicy;

    static FactSetPolicy make_fact_set_policy(const ConstFactsWorkspace& cws, const FactsWorkspace& ws) { return FactSetPolicy { cws, ws }; }

    static AssignmentSetPolicy make_assignment_set_policy(const ConstFactsWorkspace& cws, const FactsWorkspace& ws) { return AssignmentSetPolicy { cws, ws }; }
};

/**
 * Implementations
 */

template<typename Relation, typename Terms>
auto find_relation_binding(Index<Relation> relation, Terms terms, const formalism::datalog::GrounderContext& context)
{
    auto binding_ptr = context.builder.get_builder<formalism::RelationBinding<Relation>>();
    auto& binding = *binding_ptr;
    binding.clear();

    binding.relation = relation;
    for (const auto term : terms)
    {
        visit(
            [&](auto&& arg)
            {
                using Alternative = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<Alternative, formalism::ParameterIndex>)
                    binding.objects.push_back(context.binding[uint_t(arg)]);
                else if constexpr (std::is_same_v<Alternative, formalism::datalog::ObjectView>)
                    binding.objects.push_back(arg.get_index());
                else
                    static_assert(dependent_false<Alternative>::value, "Missing case");
            },
            term.get_variant());
    }

    canonicalize(binding);
    return context.destination.find(binding);
}

/**
 * NoCarePredicateFactSetsPolicy
 */

template<formalism::FactKind T>
bool NoCarePredicateFactSetsPolicy<T>::check(formalism::datalog::LiteralView<T> element, const formalism::datalog::GrounderContext& context) const
{
    const auto binding_or_nullopt = find_relation_binding(element.get_atom().get_predicate().get_index(), element.get_atom().get_terms(), context);

    if (!binding_or_nullopt)
        return !element.get_polarity();

    return fact_sets.contains(*binding_or_nullopt) == element.get_polarity();
}

template<formalism::FactKind T>
bool NoCarePredicateFactSetsPolicy<T>::check(formalism::datalog::GroundLiteralView<T> element) const
{
    return fact_sets.contains(element.get_atom().get_row()) == element.get_polarity();
}

/**
 * NoCareFunctionFactSetsPolicy
 */

template<formalism::FactKind T>
float_t NoCareFunctionFactSetsPolicy<T>::check(formalism::datalog::FunctionTermView<T> element, const formalism::datalog::GrounderContext& context) const
{
    const auto binding_or_nullopt = find_relation_binding(element.get_function().get_index(), element.get_terms(), context);

    if (!binding_or_nullopt)
        return std::numeric_limits<float_t>::quiet_NaN();

    return fact_sets[*binding_or_nullopt];
}

template<formalism::FactKind T>
float_t NoCareFunctionFactSetsPolicy<T>::check(formalism::datalog::GroundFunctionTermView<T> element) const
{
    return fact_sets[element.get_row()];
}

/**
 * CarePredicateFactSetsPolicy<FluentTag>
 */

inline bool CarePredicateFactSetsPolicy<formalism::FluentTag>::check(formalism::datalog::LiteralView<formalism::FluentTag> element,
                                                                     const formalism::datalog::GrounderContext& context) const
{
    const auto binding_or_nullopt = find_relation_binding(element.get_atom().get_predicate().get_index(), element.get_atom().get_terms(), context);

    // Outside represented universe => outside care => don't care.
    if (!binding_or_nullopt)
        return true;

    if (!care_fact_sets.contains(*binding_or_nullopt))
        return true;

    return fact_sets.contains(*binding_or_nullopt) == element.get_polarity();
}

inline bool CarePredicateFactSetsPolicy<formalism::FluentTag>::check(formalism::datalog::GroundLiteralView<formalism::FluentTag> element) const
{
    const auto binding = element.get_atom().get_row();

    if (!care_fact_sets.contains(binding))
        return true;

    return fact_sets.contains(binding) == element.get_polarity();
}

/**
 * NoCarePredicateAssignmentSetPolicy
 */

template<formalism::FactKind T>
template<typename Assignment>
bool NoCarePredicateAssignmentSetPolicy<T>::Checker::is_consistent(const Assignment& assignment, bool polarity) const
{
    return set.at(assignment) == polarity;
}

/**
 * CarePredicateAssignmentSetPolicy<FluentTag>
 */

template<typename Assignment>
bool CarePredicateAssignmentSetPolicy<formalism::FluentTag>::Checker::is_consistent(const Assignment& assignment, bool polarity) const
{
    // Outside care => don't care.
    if (!care_set.at(assignment))
        return true;

    return set.at(assignment) == polarity;
}

/**
 * NoCareFunctionAssignmentSetPolicy
 */

template<formalism::FactKind T>
bool NoCareFunctionAssignmentSetPolicy<T>::Checker::intersect_interval(const VertexAssignment& assignment, ClosedInterval<float_t>& interval) const
{
    interval = intersect(interval, set.at(assignment));
    return !empty(interval);
}

template<formalism::FactKind T>
bool NoCareFunctionAssignmentSetPolicy<T>::Checker::intersect_interval(const EdgeAssignment& assignment, ClosedInterval<float_t>& interval) const
{
    interval = intersect(interval, set.at(assignment));
    return !empty(interval);
}
}

#endif
