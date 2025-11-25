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

#ifndef TYR_FORMALISM_GROUND_FUNCTION_EXPRESSION_PROXY_HPP_
#define TYR_FORMALISM_GROUND_FUNCTION_EXPRESSION_PROXY_HPP_

#include "tyr/common/variant.hpp"
#include "tyr/formalism/ground_function_expression_data.hpp"
#include "tyr/formalism/repository.hpp"

namespace tyr
{

template<formalism::IsContext C>
class Proxy<Data<formalism::GroundFunctionExpression>, C>
{
private:
    const C* context;
    Data<formalism::GroundFunctionExpression> fexpr;

public:
    using Tag = formalism::GroundFunctionExpression;

    auto get() const { return Proxy<typename Data<formalism::GroundFunctionExpression>::Variant, C>(fexpr.value, *context); }

    Proxy(Data<formalism::GroundFunctionExpression> fexpr, const C& context) : context(&context), fexpr(fexpr) {}
};
}

#endif
