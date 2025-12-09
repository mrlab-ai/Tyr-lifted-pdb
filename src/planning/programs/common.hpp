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

#ifndef TYR_SRC_PLANNING_PROGRAMS_COMMON_HPP_
#define TYR_SRC_PLANNING_PROGRAMS_COMMON_HPP_

#include "tyr/formalism/compile.hpp"
#include "tyr/formalism/declarations.hpp"
#include "tyr/formalism/merge.hpp"
#include "tyr/formalism/overlay_repository.hpp"
#include "tyr/formalism/repository.hpp"
#include "tyr/formalism/views.hpp"

namespace tyr::planning
{
extern View<Index<formalism::Rule>, formalism::Repository>
create_axiom_rule(View<Index<formalism::Axiom>, formalism::OverlayRepository<formalism::Repository>> axiom,
                  formalism::Builder& builder,
                  formalism::Repository& repository,
                  formalism::MergeCache<formalism::OverlayRepository<formalism::Repository>, formalism::Repository>& merge_cache,
                  formalism::CompileCache<formalism::OverlayRepository<formalism::Repository>, formalism::Repository>& compile_cache);
}

#endif