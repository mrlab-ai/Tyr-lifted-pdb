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

#include "invariants.hpp"

namespace tyr::formalism::planning::invariant
{
namespace
{
void bind_invariant(nb::module_& m, const std::string& name)
{
    using V = Invariant;

    auto cls = nb::class_<V>(m, name.c_str())
                   .def_ro("num_rigid_variables", &V::num_rigid_variables)
                   .def_ro("num_counted_variables", &V::num_counted_variables)
                   .def_ro("atoms", &V::atoms)
                   .def_ro("predicates", &V::predicates);
    add_print(cls);
    add_hash(cls);
}
}

/**
 * bind_invariants
 */

void bind_invariants(nb::module_& m)
{
    bind_invariant(m, "Invariant");

    m.def("synthesize_invariants", &invariant::synthesize_invariants, "domain"_a);
}
}
