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

#ifndef TYR_FORMALISM_UNIFICATION_FORMATTER_HPP_
#define TYR_FORMALISM_UNIFICATION_FORMATTER_HPP_

#include "tyr/formalism/formatter.hpp"
#include "tyr/formalism/unification/substitution.hpp"

namespace tyr
{

namespace formalism::unification
{
template<typename T>
std::ostream& operator<<(std::ostream& os, const SubstitutionFunction<T>& el);
}

template<typename T>
std::ostream& print(std::ostream& os, const formalism::unification::SubstitutionFunction<T>& el)
{
    os << "SubstitutionFunction(\n";

    {
        IndentScope scope(os);

        os << print_indent << "parameters = " << el.parameters() << "\n";

        os << print_indent << "bindings = [\n";
        {
            IndentScope scope(os);
            for (const auto& parameter : el.parameters())
                os << print_indent << parameter << " -> " << (el.is_bound(parameter) ? to_string(*el[parameter]) : "unbound") << "\n";
        }
    }
    os << print_indent << ")";

    return os;
}

namespace formalism::unification
{
template<typename T>
std::ostream& operator<<(std::ostream& os, const SubstitutionFunction<T>& el)
{
    return tyr::print(os, el);
}
}

}

#endif
