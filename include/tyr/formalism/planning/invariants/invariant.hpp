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

#ifndef TYR_FORMALISM_PLANNING_INVARIANTS_INVARIANT_HPP_
#define TYR_FORMALISM_PLANNING_INVARIANTS_INVARIANT_HPP_

#include "tyr/common/comparators.hpp"
#include "tyr/common/declarations.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/formalism/planning/expression_arity.hpp"
#include "tyr/formalism/planning/grounder.hpp"
#include "tyr/formalism/planning/planning_task.hpp"
#include "tyr/formalism/planning/repository.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <unordered_set>
#include <variant>
#include <vector>

namespace tyr::formalism::planning::invariant
{
struct TempAtom
{
    PredicateView<FluentTag> predicate;
    std::vector<Data<Term>> terms;

    auto identifying_members() const noexcept { return std::tie(predicate, terms); }
};

using TempAtomList = std::vector<TempAtom>;

struct TempLiteral
{
    TempAtom atom;
    bool polarity;

    auto identifying_members() const noexcept { return std::tie(atom, polarity); }
};

using TempLiteralList = std::vector<TempLiteral>;

struct TempEffect
{
    size_t num_variables;
    TempLiteralList condition;
    TempAtomList add_effects;
    TempAtomList del_effects;
};

using TempEffectList = std::vector<TempEffect>;

struct TempAction
{
    size_t num_variables;
    TempLiteralList precondition;
    TempEffectList effects;
};

using TempActionList = std::vector<TempAction>;

struct Invariant
{
    size_t num_rigid_variables;
    size_t num_counted_variables;
    TempAtomList atoms;
    UnorderedSet<PredicateView<FluentTag>> predicates;
};

using InvariantList = std::vector<Invariant>;

struct Substitution
{
    std::vector<std::optional<Data<Term>>> data;

    Substitution() = default;
    explicit Substitution(size_t size) : data(size) {}

    void reset() noexcept { std::fill(data.begin(), data.end(), std::nullopt); }

    void resize(size_t size)
    {
        data.resize(size);
        reset();
    }

    size_t size() const noexcept { return data.size(); }

    bool contains(ParameterIndex parameter) const noexcept
    {
        return static_cast<uint_t>(parameter) < data.size() && data[static_cast<uint_t>(parameter)].has_value();
    }

    const std::optional<Data<Term>>& get(ParameterIndex parameter) const noexcept { return data[static_cast<uint_t>(parameter)]; }

    std::optional<Data<Term>>& get(ParameterIndex parameter) noexcept { return data[static_cast<uint_t>(parameter)]; }

    bool assign(ParameterIndex parameter, const Data<Term>& term)
    {
        auto& slot = data[static_cast<uint_t>(parameter)];
        if (slot.has_value())
            return false;
        slot = term;
        return true;
    }

    bool assign_or_check(ParameterIndex parameter, const Data<Term>& term)
    {
        auto& slot = data[static_cast<uint_t>(parameter)];
        if (!slot.has_value())
        {
            slot = term;
            return true;
        }
        return *slot == term;
    }
};

inline Data<Term> make_temp_term(TermView element) { return element.get_data(); }

inline TempAtom make_temp_atom(AtomView<FluentTag> element)
{
    auto terms = std::vector<Data<Term>> {};
    terms.reserve(element.get_terms().size());

    for (const auto term : element.get_terms())
        terms.push_back(make_temp_term(term));

    return TempAtom {
        .predicate = element.get_predicate(),
        .terms = std::move(terms),
    };
}

inline TempLiteral make_temp_literal(LiteralView<FluentTag> element)
{
    return TempLiteral {
        .atom = make_temp_atom(element.get_atom()),
        .polarity = element.get_polarity(),
    };
}

inline TempEffect make_temp_effect(ConditionalEffectView element)
{
    TempEffect result {};

    result.num_variables = element.get_arity();

    for (const auto lit : element.get_condition().get_literals<FluentTag>())
        result.condition.push_back(make_temp_literal(lit));

    for (const auto lit : element.get_effect().get_literals())
    {
        if (lit.get_polarity())
            result.add_effects.push_back(make_temp_atom(lit.get_atom()));
        else
            result.del_effects.push_back(make_temp_atom(lit.get_atom()));
    }

    return result;
}

inline TempAction make_temp_action(ActionView op)
{
    TempAction result {};

    result.num_variables = op.get_arity();

    for (const auto lit : op.get_condition().get_literals<FluentTag>())
        result.precondition.push_back(make_temp_literal(lit));

    result.effects.reserve(op.get_effects().size());
    for (const auto cond_eff : op.get_effects())
        result.effects.push_back(make_temp_effect(cond_eff));

    return result;
}

inline TempActionList make_temp_actions(ActionListView ops)
{
    auto result = TempActionList {};
    result.reserve(ops.size());

    for (const auto op : ops)
        result.push_back(make_temp_action(op));

    return result;
}

}

#endif
