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

#ifndef TYR_PYTHON_FORMALISM_PLANNING_BINDINGS_HPP
#define TYR_PYTHON_FORMALISM_PLANNING_BINDINGS_HPP

#include "../../init_declarations.hpp"

namespace tyr::formalism::planning
{
template<FactKind T>
void bind_predicate(nb::module_& m, const std::string& name)
{
    using V = View<Index<Predicate<T>>, Repository>;

    nb::class_<V>(m, name.c_str())
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_name", &V::get_name)
        .def("get_arity", &V::get_arity);
}

template<FactKind T>
void bind_atom(nb::module_& m, const std::string& name)
{
    using V = View<Index<Atom<T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_predicate", &V::get_predicate)
        .def("get_terms", &V::get_terms);
}

template<FactKind T>
void bind_ground_atom(nb::module_& m, const std::string& name)
{
    using V = View<Index<GroundAtom<T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_predicate", &V::get_predicate)
        .def("get_objects", &V::get_objects);
}

template<FactKind T>
void bind_literal(nb::module_& m, const std::string& name)
{
    using V = View<Index<Literal<T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_atom", &V::get_atom)
        .def("get_polarity", &V::get_polarity);
}

template<FactKind T>
void bind_ground_literal(nb::module_& m, const std::string& name)
{
    using V = View<Index<GroundLiteral<T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_atom", &V::get_atom)
        .def("get_polarity", &V::get_polarity);
}

template<FactKind T>
void bind_fdr_variable(nb::module_& m, const std::string& name)
{
    using V = View<Index<FDRVariable<T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_domain_size", &V::get_domain_size)
        .def("get_atoms", &V::get_atoms);
}

template<FactKind T>
void bind_fdr_fact(nb::module_& m, const std::string& name)
{
    using V = View<Data<FDRFact<T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_value", &V::get_value)
        .def("get_atom", &V::get_atom);
}

template<FactKind T>
void bind_function(nb::module_& m, const std::string& name)
{
    using V = View<Index<Function<T>>, Repository>;

    nb::class_<V>(m, name.c_str())
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_name", &V::get_name)
        .def("get_arity", &V::get_arity);
}

template<FactKind T>
void bind_function_term(nb::module_& m, const std::string& name)
{
    using V = View<Index<FunctionTerm<T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_function", &V::get_function)
        .def("get_terms", &V::get_terms);
}

template<FactKind T>
void bind_ground_function_term(nb::module_& m, const std::string& name)
{
    using V = View<Index<GroundFunctionTerm<T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_function", &V::get_function)
        .def("get_objects", &V::get_objects);
}

template<FactKind T>
void bind_ground_function_term_value(nb::module_& m, const std::string& name)
{
    using V = View<Index<GroundFunctionTermValue<T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_fterm", &V::get_fterm)
        .def("get_value", &V::get_value);
}

template<NumericEffectOpKind Op, FactKind T>
void bind_numeric_effect(nb::module_& m, const std::string& name)
{
    using V = View<Index<NumericEffect<Op, T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_fterm", &V::get_fterm)
        .def("get_fexpr", &V::get_fexpr);
}

template<NumericEffectOpKind Op, FactKind T>
void bind_ground_numeric_effect(nb::module_& m, const std::string& name)
{
    using V = View<Index<GroundNumericEffect<Op, T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_fterm", &V::get_fterm)
        .def("get_fexpr", &V::get_fexpr);
}

template<FactKind T>
void bind_numeric_effect_operator(nb::module_& m, const std::string& name)
{
    using V = View<Data<NumericEffectOperator<T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_variant", &V::get_variant);
}

template<FactKind T>
void bind_ground_numeric_effect_operator(nb::module_& m, const std::string& name)
{
    using V = View<Data<GroundNumericEffectOperator<T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_variant", &V::get_variant);
}

template<OpKind Op, typename T>
void bind_unary_operator(nb::module_& m, const std::string& name)
{
    using V = View<Index<UnaryOperator<Op, T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_arg", &V::get_arg);
}

template<OpKind Op, typename T>
void bind_binary_operator(nb::module_& m, const std::string& name)
{
    using V = View<Index<BinaryOperator<Op, T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_lhs", &V::get_lhs)
        .def("get_rhs", &V::get_rhs);
}

template<OpKind Op, typename T>
void bind_multi_operator(nb::module_& m, const std::string& name)
{
    using V = View<Index<MultiOperator<Op, T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_args", &V::get_args);
}

template<typename T>
void bind_arithmethic_operator(nb::module_& m, const std::string& name)
{
    using V = View<Data<ArithmeticOperator<T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_variant", &V::get_variant);
}

template<typename T>
void bind_boolean_operator(nb::module_& m, const std::string& name)
{
    using V = View<Data<BooleanOperator<T>>, Repository>;

    nb::class_<V>(m, name.c_str())  //
        .def("__str__", [](const V& self) { return to_string(self); })
        .def("__repr__", [](const V& self) { return to_string(self); })
        .def("__eq__", [](const V& self, const V& other) { return EqualTo<V> {}(self, other); })
        .def("__hash__", [](const V& self) { return Hash<V> {}(self); })
        .def("get_variant", &V::get_variant);
}
}

#endif
