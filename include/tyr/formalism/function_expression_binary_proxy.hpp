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

#ifndef TYR_FORMALISM_FUNCTION_EXPRESSION_BINARY_PROXY_HPP_
#define TYR_FORMALISM_FUNCTION_EXPRESSION_BINARY_PROXY_HPP_

#include "tyr/common/variant.hpp"
#include "tyr/formalism/declarations.hpp"
#include "tyr/formalism/function_expression_binary_index.hpp"
#include "tyr/formalism/repository.hpp"

namespace tyr::formalism
{
struct FunctionExpressionBinaryProxy
{
private:
    const Repository* repository;
    FunctionExpressionBinaryIndex index;

public:
    FunctionExpressionBinaryProxy(const Repository& repository, FunctionExpressionBinaryIndex index) : repository(&repository), index(index) {}

    const auto& get() const { return repository->operator[]<FunctionExpressionBinary>(index); }

    auto get_index() const { return index; }
    auto get_lhs() const { return VariantProxy<FunctionExpression, Repository>(*repository, get().lhs); }
    auto get_rhs() const { return VariantProxy<FunctionExpression, Repository>(*repository, get().rhs); }
};

}

#endif
