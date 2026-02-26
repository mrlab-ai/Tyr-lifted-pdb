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

#include "tyr/datalog/transformations/component_decomposition.hpp"

#include "tyr/formalism/datalog/builder.hpp"
#include "tyr/formalism/datalog/merge.hpp"
#include "tyr/formalism/datalog/repository.hpp"
#include "tyr/formalism/datalog/views.hpp"

namespace f = tyr::formalism;
namespace fd = tyr::formalism::datalog;

namespace tyr::datalog
{
static void append_transformed_rules(View<Index<fd::Rule>, fd::Repository> element, fd::MergeContext& context, IndexList<fd::Rule>& rules) {}

Index<fd::Program> component_decomposition(View<Index<fd::Program>, fd::Repository> element, fd::Repository& destination)
{
    auto builder = fd::Builder();
    auto context = fd::MergeContext { builder, destination };

    auto program_ptr = builder.get_builder<fd::Program>();
    auto& program = *program_ptr;
    program.clear();

    for (const auto predicate : element.get_predicates<formalism::StaticTag>())
        program.static_predicates.push_back(merge_d2d(predicate, context).first);

    for (const auto predicate : element.get_predicates<formalism::FluentTag>())
        program.fluent_predicates.push_back(merge_d2d(predicate, context).first);

    for (const auto function : element.get_functions<formalism::StaticTag>())
        program.static_functions.push_back(merge_d2d(function, context).first);

    for (const auto function : element.get_functions<formalism::FluentTag>())
        program.fluent_functions.push_back(merge_d2d(function, context).first);

    for (const auto object : element.get_objects())
        program.objects.push_back(merge_d2d(object, context).first);

    for (const auto atom : element.get_atoms<formalism::StaticTag>())
        program.static_atoms.push_back(merge_d2d(atom, context).first);

    for (const auto atom : element.get_atoms<formalism::FluentTag>())
        program.fluent_atoms.push_back(merge_d2d(atom, context).first);

    for (const auto fterm_value : element.get_fterm_values<formalism::StaticTag>())
        program.static_fterm_values.push_back(merge_d2d(fterm_value, context).first);

    for (const auto fterm_value : element.get_fterm_values<formalism::FluentTag>())
        program.fluent_fterm_values.push_back(merge_d2d(fterm_value, context).first);

    for (const auto rule : element.get_rules())
        append_transformed_rules(rule, context, program.rules);

    return destination.get_or_create(program, builder.get_buffer()).first;
}
}
