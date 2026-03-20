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

#include "tyr/datalog/fact_sets.hpp"
#include "tyr/formalism/datalog/formatter.hpp"
#include "tyr/formalism/datalog/repository.hpp"

#include <boost/dynamic_bitset.hpp>
#include <limits>

namespace f = tyr::formalism;
namespace fd = tyr::formalism::datalog;

namespace tyr::datalog
{

/**
 * PredicateFactSet
 */

template<f::FactKind T>
PredicateFactSet<T>::PredicateFactSet(fd::PredicateView<T> predicate) : m_predicate(predicate), m_views(), m_bitset()
{
}

template<f::FactKind T>
void PredicateFactSet<T>::reset()
{
    m_views.clear();
    m_bitset.reset();
}

template<f::FactKind T>
void PredicateFactSet<T>::insert(fd::GroundAtomView<T> ground_atom)
{
    const auto ground_atom_index = uint_t(ground_atom.get_index());

    if (ground_atom_index >= m_bitset.size())
        m_bitset.resize(ground_atom_index + 1, false);

    if (m_bitset.test(ground_atom_index))
        return;

    m_views.push_back(ground_atom);
    m_bitset.set(ground_atom_index);
}

template<f::FactKind T>
void PredicateFactSet<T>::insert(fd::GroundAtomListView<T> ground_atoms)
{
    for (const auto ground_atom : ground_atoms)
        insert(ground_atom);
}

template<f::FactKind T>
void PredicateFactSet<T>::insert(const std::vector<formalism::datalog::GroundAtomView<T>>& ground_atoms)
{
    for (const auto ground_atom : ground_atoms)
        insert(ground_atom);
}

template<f::FactKind T>
bool PredicateFactSet<T>::contains(fd::GroundAtomView<T> ground_atom) const noexcept
{
    return tyr::test(uint_t(ground_atom.get_index()), m_bitset);
}

template<f::FactKind T>
const std::vector<fd::GroundAtomView<T>>& PredicateFactSet<T>::get_facts() const noexcept
{
    return m_views;
}

template class PredicateFactSet<f::StaticTag>;
template class PredicateFactSet<f::FluentTag>;

/**
 * FunctionFactSet
 */

template<f::FactKind T>
FunctionFactSet<T>::FunctionFactSet(fd::FunctionView<T> function) : m_function(function), m_bindings(), m_values()
{
}

template<f::FactKind T>
void FunctionFactSet<T>::reset()
{
    m_remap.clear();
    m_bindings.clear();
    m_values.clear();
}

template<f::FactKind T>
void FunctionFactSet<T>::insert(formalism::datalog::FunctionBindingView<T> binding, float_t value)
{
    const auto i = uint_t(binding.get_index().row);

    if (i < m_bindings.size())
        throw std::runtime_error("Multiple value assignments to a ground function term.");

    const auto pos = uint_t(m_bindings.size());
    tyr::set(i, pos, m_remap, std::numeric_limits<uint_t>::max());

    m_bindings.push_back(binding);
    m_values.push_back(value);
}

template<f::FactKind T>
void FunctionFactSet<T>::insert(fd::GroundFunctionTermView<T> fterm, float_t value)
{
    insert(fterm.get_row(), value);
}

template<f::FactKind T>
void FunctionFactSet<T>::insert(const std::vector<formalism::datalog::FunctionBindingView<T>>& bindings, const std::vector<float_t>& values)
{
    assert(bindings.size() == values.size());

    for (uint_t i = 0; i < bindings.size(); ++i)
        insert(bindings[i], values[i]);
}

template<f::FactKind T>
void FunctionFactSet<T>::insert(fd::GroundFunctionTermValueView<T> fterm_value)
{
    insert(fterm_value.get_fterm().get_row(), fterm_value.get_value());
}

template<f::FactKind T>
void FunctionFactSet<T>::insert(fd::GroundFunctionTermValueListView<T> fterm_values)
{
    for (const auto fterm_value : fterm_values)
        insert(fterm_value);
}

template<f::FactKind T>
float_t FunctionFactSet<T>::operator[](fd::GroundFunctionTermView<T> fterm) const noexcept
{
    const auto row = fterm.get_row().get_index();
    const auto i = uint_t(row.row);

    if (i >= m_remap.size())
        return std::numeric_limits<float_t>::quiet_NaN();

    return tyr::get(m_remap[i], m_values, std::numeric_limits<float_t>::quiet_NaN());
}

template<f::FactKind T>
const std::vector<uint_t>& FunctionFactSet<T>::get_remap() const noexcept
{
    return m_remap;
}

template<f::FactKind T>
const std::vector<formalism::datalog::FunctionBindingView<T>>& FunctionFactSet<T>::get_bindings() const noexcept
{
    return m_bindings;
}

template<f::FactKind T>
const std::vector<float_t>& FunctionFactSet<T>::get_values() const noexcept
{
    return m_values;
}

template class FunctionFactSet<f::StaticTag>;
template class FunctionFactSet<f::FluentTag>;

}
