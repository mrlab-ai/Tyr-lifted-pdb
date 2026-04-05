/*
 * Copyright (C) 2025-2026 Dominik Drexler
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

#include "tyr/formalism/planning/mutable/formatter.hpp"

#include <fmt/core.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <ostream>

namespace tyr
{

template<formalism::FactKind T>
std::ostream& print(std::ostream& os, const formalism::planning::MutableAtom<T>& el)
{
    fmt::print(os, "({} {})", to_string(el.predicate.get_name()), fmt::join(to_strings(el.terms), " "));
    return os;
}

template std::ostream& print(std::ostream& os, const formalism::planning::MutableAtom<formalism::StaticTag>& el);
template std::ostream& print(std::ostream& os, const formalism::planning::MutableAtom<formalism::FluentTag>& el);
template std::ostream& print(std::ostream& os, const formalism::planning::MutableAtom<formalism::DerivedTag>& el);

template<formalism::FactKind T>
std::ostream& print(std::ostream& os, const formalism::planning::MutableLiteral<T>& el)
{
    if (el.polarity)
        fmt::print(os, "({})", to_string(el.atom));
    else
        fmt::print(os, "(not {})", to_string(el.atom));
    return os;
}

template std::ostream& print(std::ostream& os, const formalism::planning::MutableLiteral<formalism::StaticTag>& el);
template std::ostream& print(std::ostream& os, const formalism::planning::MutableLiteral<formalism::FluentTag>& el);
template std::ostream& print(std::ostream& os, const formalism::planning::MutableLiteral<formalism::DerivedTag>& el);

std::ostream& print(std::ostream& os, const formalism::planning::MutableConditionalEffect& el)
{
    os << "MutableConditionalEffect(\n";
    {
        IndentScope scope(os);

        os << print_indent << "num parent variables = " << el.num_parent_variables << "\n";

        os << print_indent << "num variables = " << el.num_variables << "\n";

        os << print_indent << "condition = " << el.condition << "\n";

        os << print_indent << "effect = " << el.effect << "\n";
    }
    os << print_indent << ")";

    return os;
}

std::ostream& print(std::ostream& os, const formalism::planning::MutableConjunctiveCondition& el)
{
    os << "ConjunctiveCondition(\n";
    {
        IndentScope scope(os);

        os << print_indent << "num parent variables = " << el.num_parent_variables << "\n";

        os << print_indent << "num variables = " << el.num_variables << "\n";

        os << print_indent << "static literals = " << el.static_literals << "\n";

        os << print_indent << "fluent literals = " << el.fluent_literals << "\n";
    }
    os << print_indent << ")";

    return os;
}

std::ostream& print(std::ostream& os, const formalism::planning::MutableConjunctiveEffect& el)
{
    os << "MutableConjunctiveEffect(\n";
    {
        IndentScope scope(os);

        os << print_indent << "num parent variables = " << el.num_parent_variables << "\n";

        os << print_indent << "num variables = " << el.num_variables << "\n";

        os << print_indent << "literals = " << el.literals << "\n";
    }
    os << print_indent << ")";

    return os;
}

std::ostream& print(std::ostream& os, const formalism::planning::MutableAction& el)
{
    os << "Action(\n";
    {
        IndentScope scope(os);

        os << print_indent << "num variables = " << el.num_variables << "\n";

        os << print_indent << "condition = " << el.condition << "\n";

        os << print_indent << "effects = " << el.effects << "\n";
    }
    os << print_indent << ")";

    return os;
}

namespace formalism::planning
{

template<FactKind T>
std::ostream& operator<<(std::ostream& os, const MutableAtom<T>& el)
{
    return tyr::print(os, el);
}

template std::ostream& operator<<(std::ostream& os, const MutableAtom<StaticTag>& el);
template std::ostream& operator<<(std::ostream& os, const MutableAtom<FluentTag>& el);
template std::ostream& operator<<(std::ostream& os, const MutableAtom<DerivedTag>& el);

template<FactKind T>
std::ostream& operator<<(std::ostream& os, const MutableLiteral<T>& el)
{
    return tyr::print(os, el);
}

template std::ostream& operator<<(std::ostream& os, const MutableLiteral<StaticTag>& el);
template std::ostream& operator<<(std::ostream& os, const MutableLiteral<FluentTag>& el);
template std::ostream& operator<<(std::ostream& os, const MutableLiteral<DerivedTag>& el);

std::ostream& operator<<(std::ostream& os, const MutableConditionalEffect& el) { return tyr::print(os, el); }

std::ostream& operator<<(std::ostream& os, const MutableConjunctiveCondition& el) { return tyr::print(os, el); }

std::ostream& operator<<(std::ostream& os, const MutableConjunctiveEffect& el) { return tyr::print(os, el); }

std::ostream& operator<<(std::ostream& os, const MutableAction& el) { return tyr::print(os, el); }

}
}
