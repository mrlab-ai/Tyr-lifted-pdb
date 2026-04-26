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

#include "tyr/planning/applicability_lifted.hpp"

#include "tyr/planning/lifted_task.hpp"
#include "tyr/planning/lifted_task/unpacked_state.hpp"

#ifndef TYR_HEADER_INSTANTIATION
#include "tyr/planning/applicability_lifted.ipp"

namespace tyr::planning
{

/**
 * evaluate
 */

// LiftedUnaryOperatorView

template float_t evaluate(formalism::planning::LiftedUnaryOperatorView<formalism::OpSub> element, const ApplicabilityContext& context);

// LiftedBinaryOperatorView arithmetic

template float_t evaluate(formalism::planning::LiftedBinaryOperatorView<formalism::OpAdd> element, const ApplicabilityContext& context);
template float_t evaluate(formalism::planning::LiftedBinaryOperatorView<formalism::OpSub> element, const ApplicabilityContext& context);
template float_t evaluate(formalism::planning::LiftedBinaryOperatorView<formalism::OpMul> element, const ApplicabilityContext& context);
template float_t evaluate(formalism::planning::LiftedBinaryOperatorView<formalism::OpDiv> element, const ApplicabilityContext& context);

// LiftedBinaryOperatorView boolean

template bool evaluate(formalism::planning::LiftedBinaryOperatorView<formalism::OpEq> element, const ApplicabilityContext& context);
template bool evaluate(formalism::planning::LiftedBinaryOperatorView<formalism::OpNe> element, const ApplicabilityContext& context);
template bool evaluate(formalism::planning::LiftedBinaryOperatorView<formalism::OpGe> element, const ApplicabilityContext& context);
template bool evaluate(formalism::planning::LiftedBinaryOperatorView<formalism::OpGt> element, const ApplicabilityContext& context);
template bool evaluate(formalism::planning::LiftedBinaryOperatorView<formalism::OpLe> element, const ApplicabilityContext& context);
template bool evaluate(formalism::planning::LiftedBinaryOperatorView<formalism::OpLt> element, const ApplicabilityContext& context);

// LiftedMultiOperatorView

template float_t evaluate(formalism::planning::LiftedMultiOperatorView<formalism::OpAdd> element, const ApplicabilityContext& context);
template float_t evaluate(formalism::planning::LiftedMultiOperatorView<formalism::OpMul> element, const ApplicabilityContext& context);

// NumericEffectView

template float_t evaluate(formalism::planning::NumericEffectView<formalism::planning::OpAssign, formalism::FluentTag> element,
                          const ApplicabilityContext& context);
template float_t evaluate(formalism::planning::NumericEffectView<formalism::planning::OpIncrease, formalism::FluentTag> element,
                          const ApplicabilityContext& context);
template float_t evaluate(formalism::planning::NumericEffectView<formalism::planning::OpDecrease, formalism::FluentTag> element,
                          const ApplicabilityContext& context);
template float_t evaluate(formalism::planning::NumericEffectView<formalism::planning::OpScaleUp, formalism::FluentTag> element,
                          const ApplicabilityContext& context);
template float_t evaluate(formalism::planning::NumericEffectView<formalism::planning::OpScaleDown, formalism::FluentTag> element,
                          const ApplicabilityContext& context);

template float_t evaluate(formalism::planning::NumericEffectView<formalism::planning::OpIncrease, formalism::AuxiliaryTag> element,
                          const ApplicabilityContext& context);

// NumericEffectOperatorView

template float_t evaluate(formalism::planning::NumericEffectOperatorView<formalism::FluentTag> element, const ApplicabilityContext& context);
template float_t evaluate(formalism::planning::NumericEffectOperatorView<formalism::AuxiliaryTag> element, const ApplicabilityContext& context);

/**
 * is_applicable
 */

// LiteralListView

template bool is_applicable(formalism::planning::LiteralListView<formalism::StaticTag> elements, const ApplicabilityContext& context);
template bool is_applicable(formalism::planning::LiteralListView<formalism::FluentTag> elements, const ApplicabilityContext& context);
template bool is_applicable(formalism::planning::LiteralListView<formalism::DerivedTag> elements, const ApplicabilityContext& context);

// NumericEffectView over fluent function terms

template bool is_applicable(formalism::planning::NumericEffectView<formalism::planning::OpAssign, formalism::FluentTag> element,
                            const ApplicabilityContext& context,
                            formalism::planning::EffectFamilyList& ref_fluent_effect_families);
template bool is_applicable(formalism::planning::NumericEffectView<formalism::planning::OpIncrease, formalism::FluentTag> element,
                            const ApplicabilityContext& context,
                            formalism::planning::EffectFamilyList& ref_fluent_effect_families);
template bool is_applicable(formalism::planning::NumericEffectView<formalism::planning::OpDecrease, formalism::FluentTag> element,
                            const ApplicabilityContext& context,
                            formalism::planning::EffectFamilyList& ref_fluent_effect_families);
template bool is_applicable(formalism::planning::NumericEffectView<formalism::planning::OpScaleUp, formalism::FluentTag> element,
                            const ApplicabilityContext& context,
                            formalism::planning::EffectFamilyList& ref_fluent_effect_families);
template bool is_applicable(formalism::planning::NumericEffectView<formalism::planning::OpScaleDown, formalism::FluentTag> element,
                            const ApplicabilityContext& context,
                            formalism::planning::EffectFamilyList& ref_fluent_effect_families);

}

#endif