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

#include "tyr/formalism/overlay_repository.hpp"
#include "tyr/formalism/repository.hpp"
#include "tyr/grounder/fact_sets.hpp"

#include <boost/dynamic_bitset.hpp>
#include <limits>

namespace tyr::grounder
{

/**
 * PredicateFactSet
 */

template<formalism::FactKind T>
PredicateFactSet<T>::PredicateFactSet(View<IndexList<formalism::GroundAtom<T>>, formalism::Repository> view) : m_context(view.get_context()), m_indices()
{
    insert(view);
}

template<formalism::FactKind T>
void PredicateFactSet<T>::reset()
{
    m_indices.clear();
    m_bitset.reset();
}

template<formalism::FactKind T>
void PredicateFactSet<T>::insert(View<Index<formalism::GroundAtom<T>>, formalism::Repository> view)
{
    if (&m_context != &view.get_context())
        throw std::runtime_error("Incompatible contexts.");

    const auto index = view.get_index();

    if (index.get_value() >= m_bitset.size())
        m_bitset.resize(index.get_value() + 1, false);

    if (!m_bitset.test(index.get_value()))
        m_indices.push_back(index);

    m_bitset.set(index.get_value());
}

template<formalism::FactKind T>
void PredicateFactSet<T>::insert(View<IndexList<formalism::GroundAtom<T>>, formalism::Repository> view)
{
    for (const auto atom : view)
        insert(atom);
}

template<formalism::FactKind T>
bool PredicateFactSet<T>::contains(Index<formalism::GroundAtom<T>> index) const noexcept
{
    if (index.get_value() >= m_bitset.size())
        return false;
    return m_bitset.test(index.get_value());
}

template<formalism::FactKind T>
bool PredicateFactSet<T>::contains(View<Index<formalism::GroundAtom<T>>, formalism::Repository> view) const noexcept
{
    return contains(view.get_index());
}

template<formalism::FactKind T>
View<IndexList<formalism::GroundAtom<T>>, formalism::Repository> PredicateFactSet<T>::get_facts() const noexcept
{
    return make_view(m_indices, m_context);
}

template<formalism::FactKind T>
const boost::dynamic_bitset<>& PredicateFactSet<T>::get_bitset() const noexcept
{
    return m_bitset;
}

template class PredicateFactSet<formalism::StaticTag>;
template class PredicateFactSet<formalism::FluentTag>;

/**
 * FunctionFactSet
 */

template<formalism::FactKind T>
FunctionFactSet<T>::FunctionFactSet(View<IndexList<formalism::GroundFunctionTermValue<T>>, formalism::Repository> view) :
    m_context(view.get_context()),
    m_indices(),
    m_unique()
{
    insert(view);
}

template<formalism::FactKind T>
void FunctionFactSet<T>::reset()
{
    m_indices.clear();
    m_unique.clear();
    std::fill(m_values.begin(), m_values.end(), std::numeric_limits<float_t>::quiet_NaN());
}

template<formalism::FactKind T>
void FunctionFactSet<T>::insert(View<Index<formalism::GroundFunctionTerm<T>>, formalism::Repository> function_term, float_t value)
{
    const auto fterm_index = function_term.get_index();

    if (m_unique.contains(fterm_index))
        throw std::runtime_error("Multiple value assignments to a ground function term.");

    m_indices.push_back(fterm_index);
    m_unique.insert(fterm_index);
    if (fterm_index.get_value() >= m_values.size())
        m_values.resize(fterm_index.get_value() + 1, std::numeric_limits<float_t>::quiet_NaN());
    m_values[fterm_index.get_value()] = value;
}

template<formalism::FactKind T>
void FunctionFactSet<T>::insert(View<IndexList<formalism::GroundFunctionTerm<T>>, formalism::Repository> function_terms, const std::vector<float_t>& values)
{
    assert(function_terms.size() == values.size());

    for (uint_t i = 0; i < function_terms.size(); ++i)
        insert(function_terms[i], values[i]);
}

template<formalism::FactKind T>
void FunctionFactSet<T>::insert(View<Index<formalism::GroundFunctionTermValue<T>>, formalism::Repository> view)
{
    insert(view.get_fterm(), view.get_value());
}

template<formalism::FactKind T>
void FunctionFactSet<T>::insert(View<IndexList<formalism::GroundFunctionTermValue<T>>, formalism::Repository> view)
{
    for (const auto fterm_value : view)
        insert(fterm_value);
}

template<formalism::FactKind T>
bool FunctionFactSet<T>::contains(Index<formalism::GroundFunctionTerm<T>> index) const noexcept
{
    return m_unique.contains(index);
}

template<formalism::FactKind T>
bool FunctionFactSet<T>::contains(View<Index<formalism::GroundFunctionTerm<T>>, formalism::Repository> view) const noexcept
{
    return contains(view.get_index());
}

template<formalism::FactKind T>
float_t FunctionFactSet<T>::operator[](Index<formalism::GroundFunctionTerm<T>> index) const noexcept
{
    return m_values[index.get_value()];
}

template<formalism::FactKind T>
View<IndexList<formalism::GroundFunctionTerm<T>>, formalism::Repository> FunctionFactSet<T>::get_fterms() const noexcept
{
    return make_view(m_indices, m_context);
}

template<formalism::FactKind T>
const std::vector<float_t>& FunctionFactSet<T>::get_values() const noexcept
{
    return m_values;
}

template class FunctionFactSet<formalism::StaticTag>;
template class FunctionFactSet<formalism::FluentTag>;

/**
 * FactSets
 */

FactSets::FactSets(View<Index<formalism::Program>, formalism::Repository> program) :
    static_sets(program.template get_atoms<formalism::StaticTag>(), program.template get_fterm_values<formalism::StaticTag>()),
    fluent_sets(program.template get_atoms<formalism::FluentTag>(), program.template get_fterm_values<formalism::FluentTag>())
{
}

FactSets::FactSets(View<Index<formalism::Program>, formalism::Repository> program, TaggedFactSets<formalism::FluentTag> fluent_facts) :
    static_sets(program.template get_atoms<formalism::StaticTag>(), program.template get_fterm_values<formalism::StaticTag>()),
    fluent_sets(std::move(fluent_facts))
{
}

}
