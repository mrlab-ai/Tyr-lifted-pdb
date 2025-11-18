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

#ifndef TYR_FORMALISM_PROGRAM_PROXY_HPP_
#define TYR_FORMALISM_PROGRAM_PROXY_HPP_

#include "tyr/common/span.hpp"
#include "tyr/formalism/declarations.hpp"
#include "tyr/formalism/ground_atom_proxy.hpp"
#include "tyr/formalism/program_index.hpp"
#include "tyr/formalism/repository.hpp"
#include "tyr/formalism/rule_proxy.hpp"

namespace tyr::formalism
{
class ProgramProxy
{
private:
    const Repository* repository;
    ProgramIndex index;

public:
    ProgramProxy(const Repository& repository, ProgramIndex index) : repository(&repository), index(index) {}

    const auto& get() const { return repository->operator[]<Program>(index); }

    auto get_index() const { return index; }
    auto get_static_atoms() const { return SpanProxy<GroundAtomIndex<StaticTag>, Repository>(*repository, get().static_atoms); }
    auto get_fluent_atoms() const { return SpanProxy<GroundAtomIndex<FluentTag>, Repository>(*repository, get().fluent_atoms); }
    auto get_rules() const { return SpanProxy<RuleIndex, Repository>(*repository, get().rules); }
};
}

#endif
