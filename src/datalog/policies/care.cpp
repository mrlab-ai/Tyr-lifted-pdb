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

#include "tyr/datalog/policies/care.hpp"

namespace tyr::datalog
{

template bool NoCareFactSetPolicy::check_literal(formalism::datalog::LiteralView<formalism::StaticTag> element,
                                                 const formalism::datalog::GrounderContext& context) const;
template bool NoCareFactSetPolicy::check_literal(formalism::datalog::LiteralView<formalism::FluentTag> element,
                                                 const formalism::datalog::GrounderContext& context) const;

template float_t NoCareFactSetPolicy::check_function_term(formalism::datalog::FunctionTermView<formalism::StaticTag> element,
                                                          const formalism::datalog::GrounderContext& context) const;
template float_t NoCareFactSetPolicy::check_function_term(formalism::datalog::FunctionTermView<formalism::FluentTag> element,
                                                          const formalism::datalog::GrounderContext& context) const;

template bool CareFactSetPolicy::check_literal(formalism::datalog::LiteralView<formalism::StaticTag> element,
                                               const formalism::datalog::GrounderContext& context) const;
template bool CareFactSetPolicy::check_literal(formalism::datalog::LiteralView<formalism::FluentTag> element,
                                               const formalism::datalog::GrounderContext& context) const;

template float_t CareFactSetPolicy::check_function_term(formalism::datalog::FunctionTermView<formalism::StaticTag> element,
                                                        const formalism::datalog::GrounderContext& context) const;
template float_t CareFactSetPolicy::check_function_term(formalism::datalog::FunctionTermView<formalism::FluentTag> element,
                                                        const formalism::datalog::GrounderContext& context) const;

}
