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

#include "../init_declarations.hpp"

namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;

namespace tyr::planning
{

void bind_module_definitions(nb::module_& m)
{
    /**
     * Domain
     */

    nb::class_<Domain>(m, "Domain")  //
        .def("get_repository", &Domain::get_repository)
        .def("get_domain", &Domain::get_domain);

    /**
     * GroundTask
     */

    nb::class_<GroundTask>(m, "GroundTask")  //
        .def("get_repository", &GroundTask::get_repository)
        .def("get_task", &GroundTask::get_task);

    /**
     * LiftedTask
     */

    nb::class_<LiftedTask>(m, "LiftedTask")  //
        .def("get_repository", &LiftedTask::get_repository)
        .def("get_task", &LiftedTask::get_task)
        .def("instantiate_ground_task", &LiftedTask::instantiate_ground_task);

    /**
     * State<GroundTask>
     */

    nb::class_<State<GroundTask>>(m, "GroundState")  //
        .def("get_index", &State<GroundTask>::get_index)
        .def("get_task", &State<GroundTask>::get_task, nb::rv_policy::reference_internal)
        // AccessibleStateConcept
        .def("test", nb::overload_cast<Index<fp::GroundAtom<f::StaticTag>>>(&State<GroundTask>::test, nb::const_), "index"_a)
        .def("test", nb::overload_cast<Index<fp::GroundAtom<f::DerivedTag>>>(&State<GroundTask>::test, nb::const_), "index"_a)
        .def("get", nb::overload_cast<Index<fp::GroundFunctionTerm<f::StaticTag>>>(&State<GroundTask>::get, nb::const_), "index"_a)
        .def("get", nb::overload_cast<Index<fp::FDRVariable<f::FluentTag>>>(&State<GroundTask>::get, nb::const_), "index"_a)
        .def("get", nb::overload_cast<Index<fp::GroundFunctionTerm<f::FluentTag>>>(&State<GroundTask>::get, nb::const_), "index"_a)
        // IterableStateConcept
        .def(
            "static_atoms",
            [](const State<GroundTask>& s)
            {
                auto r = s.get_static_atoms();
                return nb::make_iterator(nb::type<AtomRange<f::StaticTag>>(), "static atom iterator", std::begin(r), std::end(r));
            },
            nb::keep_alive<0, 1>())
        .def(
            "fluent_facts",
            [](const State<GroundTask>& s)
            {
                auto r = s.get_fluent_facts();
                return nb::make_iterator(nb::type<FDRFactRange<GroundTask, f::FluentTag>>(), "fluent fact iterator", std::begin(r), std::end(r));
            },
            nb::keep_alive<0, 1>())
        .def(
            "derived_atoms",
            [](const State<GroundTask>& s)
            {
                auto r = s.get_derived_atoms();
                return nb::make_iterator(nb::type<AtomRange<f::DerivedTag>>(), "derived atom iterator", std::begin(r), std::end(r));
            },
            nb::keep_alive<0, 1>())
        .def(
            "static_fterm_values",
            [](const State<GroundTask>& s)
            {
                auto r = s.get_static_fterm_values();
                return nb::make_iterator(nb::type<FunctionTermValueRange<f::StaticTag>>(), "static function term value iterator", std::begin(r), std::end(r));
            },
            nb::keep_alive<0, 1>())
        .def(
            "fluent_fterm_values",
            [](const State<GroundTask>& s)
            {
                auto r = s.get_fluent_fterm_values();
                return nb::make_iterator(nb::type<FunctionTermValueRange<f::FluentTag>>(), "fluent function term value iterator", std::begin(r), std::end(r));
            },
            nb::keep_alive<0, 1>());

    /**
     * State<LiftedTask>
     */

    nb::class_<State<LiftedTask>>(m, "LiftedState")  //
        .def("get_index", &State<LiftedTask>::get_index)
        .def("get_task", &State<LiftedTask>::get_task, nb::rv_policy::reference_internal)
        // AccessibleStateConcept
        .def("test", nb::overload_cast<Index<fp::GroundAtom<f::StaticTag>>>(&State<LiftedTask>::test, nb::const_), "index"_a)
        .def("test", nb::overload_cast<Index<fp::GroundAtom<f::DerivedTag>>>(&State<LiftedTask>::test, nb::const_), "index"_a)
        .def("get", nb::overload_cast<Index<fp::GroundFunctionTerm<f::StaticTag>>>(&State<LiftedTask>::get, nb::const_), "index"_a)
        .def("get", nb::overload_cast<Index<fp::FDRVariable<f::FluentTag>>>(&State<LiftedTask>::get, nb::const_), "index"_a)
        .def("get", nb::overload_cast<Index<fp::GroundFunctionTerm<f::FluentTag>>>(&State<LiftedTask>::get, nb::const_), "index"_a)
        // IterableStateConcept
        .def(
            "static_atoms",
            [](const State<LiftedTask>& s)
            {
                auto r = s.get_static_atoms();
                return nb::make_iterator(nb::type<AtomRange<f::StaticTag>>(), "static atom iterator", std::begin(r), std::end(r));
            },
            nb::keep_alive<0, 1>())
        .def(
            "fluent_facts",
            [](const State<LiftedTask>& s)
            {
                auto r = s.get_fluent_facts();
                return nb::make_iterator(nb::type<FDRFactRange<LiftedTask, f::FluentTag>>(), "fluent fact iterator", std::begin(r), std::end(r));
            },
            nb::keep_alive<0, 1>())
        .def(
            "derived_atoms",
            [](const State<LiftedTask>& s)
            {
                auto r = s.get_derived_atoms();
                return nb::make_iterator(nb::type<AtomRange<f::DerivedTag>>(), "derived atom iterator", std::begin(r), std::end(r));
            },
            nb::keep_alive<0, 1>())
        .def(
            "static_fterm_values",
            [](const State<LiftedTask>& s)
            {
                auto r = s.get_static_fterm_values();
                return nb::make_iterator(nb::type<FunctionTermValueRange<f::StaticTag>>(), "static function term value iterator", std::begin(r), std::end(r));
            },
            nb::keep_alive<0, 1>())
        .def(
            "fluent_fterm_values",
            [](const State<LiftedTask>& s)
            {
                auto r = s.get_fluent_fterm_values();
                return nb::make_iterator(nb::type<FunctionTermValueRange<f::FluentTag>>(), "fluent function term value iterator", std::begin(r), std::end(r));
            },
            nb::keep_alive<0, 1>());

    /**
     * Parser
     */

    nb::class_<loki::ParserOptions>(m, "ParserOptions")
        .def(nb::init<>())
        .def_rw("strict", &loki::ParserOptions::strict, "Enable strict mode")
        .def_rw("verbose", &loki::ParserOptions::verbose, "Enable verbose output");

    nb::class_<Parser>(m, "Parser")
        .def(nb::init<const fs::path&, const loki::ParserOptions&>(), "domain_filepath"_a, "parser_options"_a)
        .def(nb::init<const std::string&, const fs::path&, const loki::ParserOptions&>(), "domain_description"_a, "domain_filepath"_a, "parser_options"_a)
        .def("parse_task", nb::overload_cast<const fs::path&, const loki::ParserOptions&>(&Parser::parse_task), "task_filepath"_a, "parser_options"_a)
        .def("parse_task",
             nb::overload_cast<const std::string&, const fs::path&, const loki::ParserOptions&>(&Parser::parse_task),
             "task_description"_a,
             "task_filepath"_a,
             "parser_options"_a)
        .def("get_domain", &Parser::get_domain);
}

}