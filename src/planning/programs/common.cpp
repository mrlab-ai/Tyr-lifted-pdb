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

#include "common.hpp"

namespace tyr::planning
{

static void process_axiom_body(View<Index<formalism::ConjunctiveCondition>, formalism::OverlayRepository<formalism::Repository>> axiom_body,
                               formalism::Builder& builder,
                               formalism::Repository& repository,
                               formalism::MergeCache<formalism::OverlayRepository<formalism::Repository>, formalism::Repository>& merge_cache,
                               formalism::CompileCache<formalism::OverlayRepository<formalism::Repository>, formalism::Repository>& compile_cache,
                               Data<formalism::ConjunctiveCondition>& conj_cond)
{
    for (const auto literal : axiom_body.get_literals<formalism::StaticTag>())
        conj_cond.static_literals.push_back(formalism::merge(literal, builder, repository, merge_cache).get_index());

    for (const auto literal : axiom_body.get_literals<formalism::FluentTag>())
        conj_cond.fluent_literals.push_back(formalism::merge(literal, builder, repository, merge_cache).get_index());

    for (const auto literal : axiom_body.get_literals<formalism::DerivedTag>())
        conj_cond.fluent_literals.push_back(
            formalism::compile<formalism::DerivedTag, formalism::FluentTag>(literal, builder, repository, compile_cache, merge_cache).get_index());

    for (const auto numeric_constraint : axiom_body.get_numeric_constraints())
        conj_cond.numeric_constraints.push_back(formalism::merge(numeric_constraint, builder, repository, merge_cache).get_data());

    for (const auto literal : axiom_body.get_nullary_literals<formalism::StaticTag>())
        conj_cond.static_nullary_literals.push_back(formalism::merge(literal, builder, repository, merge_cache).get_index());

    for (const auto literal : axiom_body.get_nullary_literals<formalism::FluentTag>())
        conj_cond.fluent_nullary_literals.push_back(formalism::merge(literal, builder, repository, merge_cache).get_index());

    for (const auto literal : axiom_body.get_nullary_literals<formalism::DerivedTag>())
        conj_cond.fluent_nullary_literals.push_back(
            formalism::compile<formalism::DerivedTag, formalism::FluentTag>(literal, builder, repository, compile_cache, merge_cache).get_index());

    for (const auto numeric_constraint : axiom_body.get_nullary_numeric_constraints())
        conj_cond.nullary_numeric_constraints.push_back(formalism::merge(numeric_constraint, builder, repository, merge_cache).get_data());
}

View<Index<formalism::Rule>, formalism::Repository>
create_axiom_rule(View<Index<formalism::Axiom>, formalism::OverlayRepository<formalism::Repository>> axiom,
                  formalism::Builder& builder,
                  formalism::Repository& repository,
                  formalism::MergeCache<formalism::OverlayRepository<formalism::Repository>, formalism::Repository>& merge_cache,
                  formalism::CompileCache<formalism::OverlayRepository<formalism::Repository>, formalism::Repository>& compile_cache)
{
    auto rule_ptr = builder.get_builder<formalism::Rule>();
    auto& rule = *rule_ptr;
    rule.clear();

    auto conj_cond_ptr = builder.get_builder<formalism::ConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    for (const auto variable : axiom.get_variables())
        conj_cond.variables.push_back(formalism::merge(variable, builder, repository, merge_cache).get_index());

    process_axiom_body(axiom.get_body(), builder, repository, merge_cache, compile_cache, conj_cond);

    formalism::canonicalize(conj_cond);
    const auto new_conj_cond = repository.get_or_create(conj_cond, builder.get_buffer()).first;

    rule.body = new_conj_cond.get_index();

    const auto new_head = formalism::compile<formalism::DerivedTag, formalism::FluentTag>(axiom.get_head(), builder, repository, compile_cache, merge_cache);

    rule.head = new_head.get_index();

    formalism::canonicalize(rule);
    return repository.get_or_create(rule, builder.get_buffer()).first;
}
}
