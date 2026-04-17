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

#ifndef TYR_SOLVER_POLICIES_CARE_CONCEPT_HPP_
#define TYR_SOLVER_POLICIES_CARE_CONCEPT_HPP_

#include "tyr/common/closed_interval.hpp"
#include "tyr/datalog/assignment.hpp"
#include "tyr/datalog/workspaces/facts.hpp"
#include "tyr/formalism/datalog/declarations.hpp"
#include "tyr/formalism/datalog/grounder_decl.hpp"

#include <concepts>

namespace tyr::datalog
{

/**
 * Checker concepts
 */

template<typename C>
concept PredicateAssignmentCheckerConcept =
    requires(const C& checker, const VertexAssignment& vertex_assignment, const EdgeAssignment& edge_assignment, bool polarity) {
        { checker.is_consistent(vertex_assignment, polarity) } -> std::same_as<bool>;
        { checker.is_consistent(edge_assignment, polarity) } -> std::same_as<bool>;
    };

template<typename C>
concept FunctionAssignmentCheckerConcept =
    requires(const C& checker, const VertexAssignment& vertex_assignment, const EdgeAssignment& edge_assignment, ClosedInterval<float_t>& interval) {
        { checker.intersect_interval(vertex_assignment, interval) } -> std::same_as<bool>;
        { checker.intersect_interval(edge_assignment, interval) } -> std::same_as<bool>;
    };

/**
 * Tagged fact-set policies
 */

template<typename P, typename T>
concept TaggedFactSetCarePolicyForKind = formalism::FactKind<T>
                                         && requires(const P& policy,
                                                     formalism::datalog::LiteralView<T> lit,
                                                     formalism::datalog::FunctionTermView<T> fterm,
                                                     formalism::datalog::GroundLiteralView<T> glit,
                                                     formalism::datalog::GroundFunctionTermView<T> gfterm,
                                                     const formalism::datalog::GrounderContext& context) {
                                                { policy.predicate.check(lit, context) } -> std::same_as<bool>;
                                                { policy.function.check(fterm, context) } -> std::same_as<float_t>;
                                                { policy.predicate.check(glit) } -> std::same_as<bool>;
                                                { policy.function.check(gfterm) } -> std::same_as<float_t>;
                                            };

/**
 * Combined fact-set policies
 */

template<typename P, typename T>
concept FactSetCarePolicyForKind =
    formalism::FactKind<T> && requires(const P& policy) { requires TaggedFactSetCarePolicyForKind<decltype(policy.template get<T>()), T>; };

template<typename P>
concept FactSetCarePolicyConcept = FactSetCarePolicyForKind<P, formalism::StaticTag> && FactSetCarePolicyForKind<P, formalism::FluentTag>;

/**
 * Tagged assignment-set policies
 */

template<typename P, typename T>
concept TaggedAssignmentSetCarePolicyForKind =
    formalism::FactKind<T> && requires(const P& policy, Index<formalism::Predicate<T>> predicate, Index<formalism::Function<T>> function) {
        requires PredicateAssignmentCheckerConcept<decltype(policy.predicate.make_checker(predicate))>;
        requires FunctionAssignmentCheckerConcept<decltype(policy.function.make_checker(function))>;
    };

/**
 * Combined assignment-set policies
 */

template<typename P, typename T>
concept AssignmentSetCarePolicyForKind =
    formalism::FactKind<T> && requires(const P& policy) { requires TaggedAssignmentSetCarePolicyForKind<decltype(policy.template get<T>()), T>; };

template<typename P>
concept AssignmentSetCarePolicyConcept = AssignmentSetCarePolicyForKind<P, formalism::StaticTag> && AssignmentSetCarePolicyForKind<P, formalism::FluentTag>;

/**
 * Combined top-level care policy
 */

template<typename P>
concept CarePolicyConcept = requires(const ConstFactsWorkspace& cws, const FactsWorkspace& ws) {
    typename P::FactSetPolicy;
    typename P::AssignmentSetPolicy;

    { P::make_fact_set_policy(cws, ws) } -> std::same_as<typename P::FactSetPolicy>;
    { P::make_assignment_set_policy(cws, ws) } -> std::same_as<typename P::AssignmentSetPolicy>;
};

}  // namespace tyr::datalog

#endif