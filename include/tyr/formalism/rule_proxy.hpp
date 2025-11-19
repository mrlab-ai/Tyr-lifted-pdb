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

#ifndef TYR_FORMALISM_RULE_PROXY_HPP_
#define TYR_FORMALISM_RULE_PROXY_HPP_

#include "tyr/common/span.hpp"
#include "tyr/formalism/atom_proxy.hpp"
#include "tyr/formalism/declarations.hpp"
#include "tyr/formalism/literal_proxy.hpp"
#include "tyr/formalism/repository.hpp"
#include "tyr/formalism/rule_index.hpp"
#include "tyr/formalism/variable_proxy.hpp"

namespace tyr::formalism
{
template<IsContext C>
class RuleProxy
{
private:
    const C* context;
    RuleIndex index;

public:
    RuleProxy(const C& context, RuleIndex index) : context(&context), index(index) {}

    const auto& get() const { return get_repository(*context).template operator[]<Rule>(index); }

    auto get_index() const { return index; }
    auto get_variables() const { return SpanProxy<C, VariableIndex>(*context, get().variables); }
    auto get_static_body() const { return SpanProxy<C, LiteralIndex<StaticTag>>(*context, get().static_body); }
    auto get_fluent_body() const { return SpanProxy<C, LiteralIndex<FluentTag>>(*context, get().fluent_body); }
    auto get_head() const { return AtomProxy(*context, get().head); }
};
}

#endif
