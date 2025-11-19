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

#ifndef TYR_FORMALISM_FUNCTION_EXPRESSION_MULTI_HPP_
#define TYR_FORMALISM_FUNCTION_EXPRESSION_MULTI_HPP_

#include "tyr/formalism/declarations.hpp"
#include "tyr/formalism/function_expression.hpp"
#include "tyr/formalism/function_expression_multi_index.hpp"

namespace tyr::formalism
{
struct FunctionExpressionMulti
{
    FunctionExpressionMultiIndex index;
    FunctionExpressionList fexprs;

    using IndexType = FunctionExpressionMultiIndex;

    FunctionExpressionMulti() = default;
    FunctionExpressionMulti(FunctionExpressionMultiIndex index, FunctionExpressionList fexprs) : index(index), fexprs(std::move(fexprs)) {}

    auto cista_members() const noexcept { return std::tie(index, fexprs); }
    auto identifying_members() const noexcept { return std::tie(fexprs); }
};

}

#endif
