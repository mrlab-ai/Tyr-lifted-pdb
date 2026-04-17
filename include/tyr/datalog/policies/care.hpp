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
 * FactSets
 */

struct NoCareFactSetPolicy
{
    FactSets fact_sets;

    template<formalism::FactKind T>
    bool check_literal(formalism::datalog::LiteralView<T> element, const formalism::datalog::GrounderContext& context) const;

    template<formalism::FactKind T>
    float_t check_function_term(formalism::datalog::FunctionTermView<T> element, const formalism::datalog::GrounderContext& context) const;

    template<formalism::FactKind T>
    bool check_literal(formalism::datalog::GroundLiteralView<T> element) const;

    template<formalism::FactKind T>
    float_t check_function_term(formalism::datalog::GroundFunctionTermView<T> element) const;
};

struct CareFactSetPolicy
{
    FactSets fact_sets;
    FactSets care_fact_sets;

    template<formalism::FactKind T>
    bool check_literal(formalism::datalog::LiteralView<T> element, const formalism::datalog::GrounderContext& context) const;

    template<formalism::FactKind T>
    float_t check_function_term(formalism::datalog::FunctionTermView<T> element, const formalism::datalog::GrounderContext& context) const;

    template<formalism::FactKind T>
    bool check_literal(formalism::datalog::GroundLiteralView<T> element) const;

    template<formalism::FactKind T>
    float_t check_function_term(formalism::datalog::GroundFunctionTermView<T> element) const;
};

/**
 * AssignmentSets
 */

struct NoCareAssignmentSetPolicy
{
    AssignmentSets assignment_sets;

    template<formalism::FactKind T>
    struct PredicateChecker
    {
        const PredicateAssignmentSet<T>& pred_set;

        template<typename Assignment>
        bool is_consistent(const Assignment& assignment, bool polarity) const;
    };

    template<formalism::FactKind T>
    struct FunctionChecker
    {
        const FunctionAssignmentSet<T>& func_set;

        bool intersect_interval(const VertexAssignment& assignment, ClosedInterval<float_t>& interval) const;
        bool intersect_interval(const EdgeAssignment& assignment, ClosedInterval<float_t>& interval) const;
    };

    template<formalism::FactKind T>
    PredicateChecker<T> make_predicate_checker(Index<formalism::Predicate<T>> predicate) const
    {
        return PredicateChecker<T> { assignment_sets.template get<T>().predicate.at(predicate) };
    }

    template<formalism::FactKind T>
    FunctionChecker<T> make_function_checker(Index<formalism::Function<T>> function) const
    {
        return FunctionChecker<T> { assignment_sets.template get<T>().function.at(function) };
    }
};

struct CareAssignmentSetPolicy
{
    AssignmentSets assignment_sets;
    AssignmentSets care_assignment_sets;

    template<formalism::FactKind T>
    struct PredicateChecker
    {
        const PredicateAssignmentSet<T>& pred_set;
        const PredicateAssignmentSet<T>& care_pred_set;

        template<typename Assignment>
        bool is_consistent(const Assignment& assignment, bool polarity) const;
    };

    template<formalism::FactKind T>
    struct FunctionChecker
    {
        const FunctionAssignmentSet<T>& func_set;

        bool intersect_interval(const VertexAssignment& assignment, ClosedInterval<float_t>& interval) const;
        bool intersect_interval(const EdgeAssignment& assignment, ClosedInterval<float_t>& interval) const;
    };

    template<formalism::FactKind T>
    PredicateChecker<T> make_predicate_checker(Index<formalism::Predicate<T>> predicate) const
    {
        return PredicateChecker<T> { assignment_sets.template get<T>().predicate.at(predicate),
                                     care_assignment_sets.template get<T>().predicate.at(predicate) };
    }

    template<formalism::FactKind T>
    FunctionChecker<T> make_function_checker(Index<formalism::Function<T>> function) const
    {
        return FunctionChecker<T> { assignment_sets.template get<T>().function.at(function) };
    }
};

/**
 * Combined
 */

struct NoCarePolicy
{
    using FactSetPolicy = NoCareFactSetPolicy;
    using AssignmentSetPolicy = NoCareAssignmentSetPolicy;

    static FactSetPolicy make_fact_set_policy(const ConstFactsWorkspace& cws, const FactsWorkspace& ws)
    {
        return FactSetPolicy { FactSets { cws.fact_sets, ws.fact_sets } };
    }

    static AssignmentSetPolicy make_assignment_set_policy(const ConstFactsWorkspace& cws, const FactsWorkspace& ws)
    {
        return AssignmentSetPolicy { AssignmentSets { cws.assignment_sets, ws.assignment_sets } };
    }
};

struct CarePolicy
{
    using FactSetPolicy = CareFactSetPolicy;
    using AssignmentSetPolicy = CareAssignmentSetPolicy;

    static FactSetPolicy make_fact_set_policy(const ConstFactsWorkspace& cws, const FactsWorkspace& ws)
    {
        return FactSetPolicy { FactSets { cws.fact_sets, ws.fact_sets }, FactSets { cws.care_fact_sets, ws.care_fact_sets } };
    }

    static AssignmentSetPolicy make_assignment_set_policy(const ConstFactsWorkspace& cws, const FactsWorkspace& ws)
    {
        return AssignmentSetPolicy { AssignmentSets { cws.assignment_sets, ws.assignment_sets },
                                     AssignmentSets { cws.care_assignment_sets, ws.care_assignment_sets } };
    }
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

template<formalism::FactKind T>
bool NoCareFactSetPolicy::check_literal(formalism::datalog::LiteralView<T> element, const formalism::datalog::GrounderContext& context) const
{
    auto binding_or_nullopt = find_relation_binding(element.get_atom().get_predicate().get_index(), element.get_atom().get_terms(), context);
    if (!binding_or_nullopt)
        return element.get_polarity() == false;

    return fact_sets.template get<T>().predicate.contains(*binding_or_nullopt) == element.get_polarity();
}

template<formalism::FactKind T>
float_t NoCareFactSetPolicy::check_function_term(formalism::datalog::FunctionTermView<T> element, const formalism::datalog::GrounderContext& context) const
{
    auto binding_or_nullopt = find_relation_binding(element.get_function().get_index(), element.get_terms(), context);
    if (!binding_or_nullopt)
        return std::numeric_limits<float_t>::quiet_NaN();  // Indicate invalid binding with NaN

    return fact_sets.template get<T>().function[*binding_or_nullopt];
}

template<formalism::FactKind T>
bool NoCareFactSetPolicy::check_literal(formalism::datalog::GroundLiteralView<T> element) const
{
    return fact_sets.template get<T>().predicate.contains(element.get_atom().get_row()) == element.get_polarity();
}

template<formalism::FactKind T>
float_t NoCareFactSetPolicy::check_function_term(formalism::datalog::GroundFunctionTermView<T> element) const
{
    return fact_sets.template get<T>().function[element.get_row()];
}

template<formalism::FactKind T>
bool CareFactSetPolicy::check_literal(formalism::datalog::LiteralView<T> element, const formalism::datalog::GrounderContext& context) const
{
    auto binding_or_nullopt = find_relation_binding(element.get_atom().get_predicate().get_index(), element.get_atom().get_terms(), context);

    if constexpr (std::is_same_v<T, formalism::StaticTag>)
    {
        if (!binding_or_nullopt)
            return !element.get_polarity();

        return fact_sets.template get<T>().predicate.contains(*binding_or_nullopt) == element.get_polarity();
    }
    else
    {
        // Outside care => don't care => satisfied regardless of polarity.
        if (!binding_or_nullopt)
            return true;

        if (!care_fact_sets.template get<formalism::FluentTag>().predicate.contains(*binding_or_nullopt))
            return true;

        return fact_sets.template get<formalism::FluentTag>().predicate.contains(*binding_or_nullopt) == element.get_polarity();
    }
}

template<formalism::FactKind T>
float_t CareFactSetPolicy::check_function_term(formalism::datalog::FunctionTermView<T> element, const formalism::datalog::GrounderContext& context) const
{
    auto binding_or_nullopt = find_relation_binding(element.get_function().get_index(), element.get_terms(), context);
    if (!binding_or_nullopt)
        return std::numeric_limits<float_t>::quiet_NaN();

    return fact_sets.template get<T>().function[*binding_or_nullopt];
}

template<formalism::FactKind T>
bool CareFactSetPolicy::check_literal(formalism::datalog::GroundLiteralView<T> element) const
{
    if constexpr (std::is_same_v<T, formalism::StaticTag>)
    {
        const auto binding = element.get_atom().get_row();

        return fact_sets.template get<T>().predicate.contains(binding) == element.get_polarity();
    }
    else
    {
        const auto binding = element.get_atom().get_row();

        if (!care_fact_sets.template get<formalism::FluentTag>().predicate.contains(binding))
            return true;

        return fact_sets.template get<formalism::FluentTag>().predicate.contains(binding) == element.get_polarity();
    }
}

template<formalism::FactKind T>
float_t CareFactSetPolicy::check_function_term(formalism::datalog::GroundFunctionTermView<T> element) const
{
    return fact_sets.template get<T>().function[element.get_row()];
}

/**
 * AssignmentSetPolicy
 */

/**
 * NoCareAssignmentSetPolicy::PredicateChecker
 */

template<formalism::FactKind T>
template<typename Assignment>
bool NoCareAssignmentSetPolicy::PredicateChecker<T>::is_consistent(const Assignment& assignment, bool polarity) const
{
    return polarity == pred_set.at(assignment);
}

/**
 * CareAssignmentSetPolicy::PredicateChecker
 */

template<formalism::FactKind T>
template<typename Assignment>
bool CareAssignmentSetPolicy::PredicateChecker<T>::is_consistent(const Assignment& assignment, bool polarity) const
{
    if constexpr (std::is_same_v<T, formalism::StaticTag>)
    {
        return pred_set.at(assignment) == polarity;
    }
    else
    {
        // If this projected assignment cannot be extended to any care fact,
        // then it is outside the pattern and hence don't care.
        if (!care_pred_set.at(assignment))
            return true;

        // Otherwise fall back to the ordinary projected truth test.
        return pred_set.at(assignment) == polarity;
    }
}

/**
 * NoCareAssignmentSetPolicy::FunctionChecker
 */

template<formalism::FactKind T>
bool NoCareAssignmentSetPolicy::FunctionChecker<T>::intersect_interval(const VertexAssignment& assignment, ClosedInterval<float_t>& bounds) const
{
    bounds = intersect(bounds, func_set.at(assignment));
    return !empty(bounds);
}

template<formalism::FactKind T>
bool NoCareAssignmentSetPolicy::FunctionChecker<T>::intersect_interval(const EdgeAssignment& assignment, ClosedInterval<float_t>& bounds) const
{
    bounds = intersect(bounds, func_set.at(assignment));
    return !empty(bounds);
}

/**
 * CareAssignmentSetPolicy::FunctionChecker
 *
 * Currently identical to NoCare, since don't-care semantics are only applied to predicates.
 */

template<formalism::FactKind T>
bool CareAssignmentSetPolicy::FunctionChecker<T>::intersect_interval(const VertexAssignment& assignment, ClosedInterval<float_t>& bounds) const
{
    bounds = intersect(bounds, func_set.at(assignment));
    return !empty(bounds);
}

template<formalism::FactKind T>
bool CareAssignmentSetPolicy::FunctionChecker<T>::intersect_interval(const EdgeAssignment& assignment, ClosedInterval<float_t>& bounds) const
{
    bounds = intersect(bounds, func_set.at(assignment));
    return !empty(bounds);
}
}

#endif
