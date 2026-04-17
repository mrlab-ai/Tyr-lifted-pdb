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
#include "tyr/datalog/assignment_sets.hpp"
#include "tyr/datalog/fact_sets.hpp"
#include "tyr/datalog/workspaces/facts.hpp"
#include "tyr/formalism/datalog/declarations.hpp"
#include "tyr/formalism/datalog/grounder_decl.hpp"
#include "tyr/formalism/datalog/repository.hpp"

namespace tyr::datalog
{

/**
 * FactSets
 */

template<typename P, typename T>
concept FactSetCarePolicyForKind = formalism::FactKind<T>
                                   && requires(const P& policy,
                                               formalism::datalog::LiteralView<T> lit,
                                               formalism::datalog::FunctionTermView<T> fterm,
                                               formalism::datalog::GroundLiteralView<T> glit,
                                               formalism::datalog::GroundFunctionTermView<T> gfterm,
                                               const formalism::datalog::GrounderContext& context) {
                                          { policy.template check_literal<T>(lit, context) } -> std::same_as<bool>;
                                          { policy.template check_function_term<T>(fterm, context) } -> std::same_as<float_t>;
                                          { policy.template check_literal<T>(glit) } -> std::same_as<bool>;
                                          { policy.template check_function_term<T>(gfterm) } -> std::same_as<float_t>;
                                      };

template<typename P>
concept FactSetCarePolicyConcept = FactSetCarePolicyForKind<P, formalism::StaticTag> && FactSetCarePolicyForKind<P, formalism::FluentTag>;

/**
 * AssignmentSets
 */

template<typename P, typename T>
concept AssignmentSetCarePolicyForKind = requires(const P& policy, Index<formalism::Predicate<T>> predicate, Index<formalism::Function<T>> function) {
    requires formalism::FactKind<T>;
    { policy.template make_predicate_checker<T>(predicate) } -> std::same_as<typename P::template PredicateChecker<T>>;
    { policy.template make_function_checker<T>(function) } -> std::same_as<typename P::template FunctionChecker<T>>;
};

template<typename P>
concept AssignmentSetCarePolicyConcept = AssignmentSetCarePolicyForKind<P, formalism::StaticTag> && AssignmentSetCarePolicyForKind<P, formalism::FluentTag>;

/**
 * Combined
 */

template<typename P>
concept CarePolicyConcept = requires(const P& policy, const FactsWorkspace& ws, const ConstFactsWorkspace& cws) {
    typename P::FactSetPolicy;
    typename P::AssignmentSetPolicy;
    { P::make_fact_set_policy(cws, ws) } -> std::same_as<typename P::FactSetPolicy>;
    { P::make_assignment_set_policy(cws, ws) } -> std::same_as<typename P::AssignmentSetPolicy>;
};

}

#endif
