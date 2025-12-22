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

#ifndef TYR_PLANNING_GROUND_TASK_UNPACKED_STATE_HPP_
#define TYR_PLANNING_GROUND_TASK_UNPACKED_STATE_HPP_

#include "tyr/common/config.hpp"
#include "tyr/formalism/ground_atom_index.hpp"
#include "tyr/formalism/ground_function_term_index.hpp"
#include "tyr/formalism/planning/fdr_fact_data.hpp"
#include "tyr/formalism/planning/fdr_variable_index.hpp"
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/unpacked_state.hpp"

#include <boost/dynamic_bitset.hpp>
#include <cassert>
#include <limits>
#include <vector>

namespace tyr::planning
{
template<>
class UnpackedState<GroundTask>
{
public:
    using TaskType = GroundTask;

    UnpackedState() = default;

    /**
     * UnpackedStateConcept
     */

    StateIndex get_index() const;
    void set(StateIndex index);

    formalism::FDRValue get(Index<formalism::FDRVariable<formalism::FluentTag>> index) const;
    void set(Data<formalism::FDRFact<formalism::FluentTag>> fact);

    float_t get(Index<formalism::GroundFunctionTerm<formalism::FluentTag>> index) const;
    void set(Index<formalism::GroundFunctionTerm<formalism::FluentTag>> index, float_t value);

    bool test(Index<formalism::GroundAtom<formalism::DerivedTag>> index) const;
    void set(Index<formalism::GroundAtom<formalism::DerivedTag>> index);

    void clear();

    /**
     * For GroundTask
     */

    void reset(size_t num_fluent_facts, size_t num_numeric_variables, size_t num_derived_atoms);
    void reset_unextended_part(size_t num_fluent_facts, size_t num_numeric_variables);
    void reset_extended_part(size_t num_numeric_variables);

private:
    StateIndex m_index;
    std::vector<formalism::FDRValue> m_fluent_values;
    boost::dynamic_bitset<> m_derived_atoms;
    std::vector<float_t> m_numeric_variables;
};

static_assert(UnpackedStateConcept<UnpackedState<GroundTask>>);

/**
 * Implementations
 */

inline StateIndex UnpackedState<GroundTask>::get_index() const { return m_index; }

inline void UnpackedState<GroundTask>::set(StateIndex index) { m_index = index; }

// Fluent facts
inline formalism::FDRValue UnpackedState<GroundTask>::get(Index<formalism::FDRVariable<formalism::FluentTag>> index) const
{
    if (index.get_value() >= m_fluent_values.size())
        return formalism::FDRValue { 0 };
    return m_fluent_values[index.get_value()];
}

inline void UnpackedState<GroundTask>::set(Data<formalism::FDRFact<formalism::FluentTag>> fact)
{
    if (fact.variable.get_value() >= m_fluent_values.size())
        m_fluent_values.resize(fact.variable.get_value() + 1, formalism::FDRValue { 0 });
    m_fluent_values[fact.variable.get_value()] = fact.value;
}

// Fluent numeric variables
inline float_t UnpackedState<GroundTask>::get(Index<formalism::GroundFunctionTerm<formalism::FluentTag>> index) const
{
    if (index.get_value() >= m_numeric_variables.size())
        return std::numeric_limits<float_t>::quiet_NaN();
    return m_numeric_variables[index.get_value()];
}

inline void UnpackedState<GroundTask>::set(Index<formalism::GroundFunctionTerm<formalism::FluentTag>> index, float_t value)
{
    if (index.get_value() >= m_numeric_variables.size())
        m_numeric_variables.resize(index.get_value() + 1, std::numeric_limits<float_t>::quiet_NaN());
    m_numeric_variables[index.get_value()] = value;
}

// Derived atoms
inline bool UnpackedState<GroundTask>::test(Index<formalism::GroundAtom<formalism::DerivedTag>> index) const
{
    if (index.get_value() >= m_derived_atoms.size())
        return false;
    return m_derived_atoms.test(index.get_value());
}

inline void UnpackedState<GroundTask>::set(Index<formalism::GroundAtom<formalism::DerivedTag>> index)
{
    if (index.get_value() >= m_derived_atoms.size())
        m_derived_atoms.resize(index.get_value() + 1, false);
    m_derived_atoms.set(index.get_value());
}

inline void UnpackedState<GroundTask>::clear()
{
    m_fluent_values.clear();
    m_derived_atoms.clear();
    m_numeric_variables.clear();
}

inline void UnpackedState<GroundTask>::reset(size_t num_fluent_facts, size_t num_numeric_variables, size_t num_derived_atoms)
{
    reset_unextended_part(num_fluent_facts, num_derived_atoms);
    reset_extended_part(num_derived_atoms);
}

inline void UnpackedState<GroundTask>::reset_unextended_part(size_t num_fluent_facts, size_t num_numeric_variables)
{
    m_fluent_values.resize(num_fluent_facts);
    std::fill(m_fluent_values.begin(), m_fluent_values.end(), formalism::FDRValue::none());
    m_numeric_variables.resize(num_numeric_variables);
    std::fill(m_numeric_variables.begin(), m_numeric_variables.end(), std::numeric_limits<float_t>::quiet_NaN());
}

inline void UnpackedState<GroundTask>::reset_extended_part(size_t num_derived_atoms)
{
    m_derived_atoms.resize(num_derived_atoms);
    m_derived_atoms.reset();
}
}

#endif
