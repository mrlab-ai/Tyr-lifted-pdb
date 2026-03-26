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

#ifndef TYR_PYTHON_PLANNING_BINDINGS_HPP
#define TYR_PYTHON_PLANNING_BINDINGS_HPP

#include "../init_declarations.hpp"

#include <nanobind/trampoline.h>

namespace tyr::planning
{

template<typename Task>
class PyPatternGenerator : public PatternGenerator<Task>
{
public:
    using Base = PatternGenerator<Task>;

    NB_TRAMPOLINE(Base, 1);

    /* Trampoline (need one for each virtual function) */
    PatternCollection generate() override { NB_OVERRIDE_PURE(generate); }
};

inline void bind_pattern(nb::module_& m, const std::string& name)
{
    using T = Pattern;

    nb::class_<T>(m, name.c_str())  //
        .def(nb::init<formalism::planning::FDRFactViewList<formalism::FluentTag>>(), "facts"_a);
}

template<typename Task>
void bind_projection_abstraction(nb::module_& m, const std::string& name)
{
    using T = ProjectionAbstraction<Task>;

    nb::class_<T>(m, name.c_str());
}

template<typename Task>
void bind_pattern_generator(nb::module_& m, const std::string& name)
{
    using T = PatternGenerator<Task>;

    nb::class_<T, PyPatternGenerator<Task>>(m, name.c_str())  //
        .def("generate", &T::generate);
}

template<typename Task>
void bind_goal_pattern_generator(nb::module_& m, const std::string& name)
{
    using T = GoalPatternGenerator<Task>;

    nb::class_<T, PatternGenerator<Task>>(m, name.c_str())  //
        .def(nb::new_([](std::shared_ptr<const Task> task) { return T::create(std::move(task)); }), "task"_a);
}

template<typename Task>
void bind_projection_generator(nb::module_& m, const std::string& name)
{
    using T = ProjectionGenerator<Task>;

    nb::class_<T>(m, name.c_str())  //
        .def(nb::init<std::shared_ptr<const Task>, PatternCollection>(), "task"_a, "patterns"_a)
        .def("generate", &T::generate);
}

}

#endif