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

#ifndef TYR_DATALOG_TRANSFORMATION_ELIMINATE_DANGLING_EXISTENTIALS_HPP_
#define TYR_DATALOG_TRANSFORMATION_ELIMINATE_DANGLING_EXISTENTIALS_HPP_

#include "tyr/datalog/declarations.hpp"
#include "tyr/formalism/datalog/program_index.hpp"
#include "tyr/formalism/datalog/program_view.hpp"
#include "tyr/formalism/datalog/repository.hpp"

namespace tyr::datalog
{

extern Index<formalism::datalog::Program> eliminate_dangling_existentials(View<Index<formalism::datalog::Program>, formalism::datalog::Repository> program,
                                                                          formalism::datalog::Repository& destination);

}

#endif
