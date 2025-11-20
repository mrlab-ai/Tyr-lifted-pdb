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

#ifndef TYR_FORMALISM_BINARY_OPERATOR_PROXY_HPP_
#define TYR_FORMALISM_BINARY_OPERATOR_PROXY_HPP_

#include "tyr/common/variant.hpp"
#include "tyr/formalism/binary_operator_index.hpp"
#include "tyr/formalism/declarations.hpp"

namespace tyr::formalism
{
template<IsOp Op, typename T, IsContext C>
class BinaryOperatorProxy
{
private:
    using IndexType = typename T::IndexType;

    const C* context;
    IndexType index;

public:
    BinaryOperatorProxy(IndexType index, const C& context) : context(&context), index(index) {}

    const auto& get() const { return get_repository(*context).template operator[]<BinaryOperator<Op, T>>(index); }

    auto get_index() const { return index; }
    auto get_lhs() const { return VariantProxy(get().lhs, *context); }
    auto get_rhs() const { return VariantProxy(get().rhs, *context); }
};

}

#endif
