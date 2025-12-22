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

#include "tyr/planning/ground_task/axiom_scheduler.hpp"

using namespace tyr::formalism;

namespace tyr::planning
{

GroundAxiomSchedulerStratum::GroundAxiomSchedulerStratum(const GroundAxiomStratum& axioms,
                                                         const GroundAxiomListenerStratum& listeners,
                                                         const OverlayRepository<Repository>& context) :
    m_axioms(axioms),
    m_listeners(listeners),
    m_context(context),
    m_active_atoms(),
    m_active_set(),
    m_active()
{
    for (const auto axiom : axioms)
    {
        const auto atom = uint_t(make_view(axiom, context).get_head().get_index());
        if (atom >= m_active_atoms.size())
            m_active_atoms.resize(atom + 1, false);
    }
}

void GroundAxiomSchedulerStratum::activate_all()
{
    m_active.clear();
    for (const auto axiom : m_axioms)
        m_active.push_back(axiom);
}

void GroundAxiomSchedulerStratum::on_start_iteration() noexcept { m_active_atoms.reset(); }

void GroundAxiomSchedulerStratum::on_generate(Index<GroundAtom<DerivedTag>> atom)
{
    assert(uint_t(atom) < m_active_atoms.size());

    m_active_atoms.set(uint_t(atom));
}

void GroundAxiomSchedulerStratum::on_finish_iteration()
{
    m_active_set.clear();
    for (auto i = m_active_atoms.find_first(); i != boost::dynamic_bitset<>::npos; i = m_active_atoms.find_next(i))
        if (const auto it = m_listeners.find(Index<GroundAtom<DerivedTag>>(i)); it != m_listeners.end())
            for (const auto axiom : it->second)
                m_active_set.insert(axiom);

    m_active.clear();
    for (const auto axiom : m_active_set)
        m_active.push_back(axiom);
}

View<IndexList<GroundAxiom>, OverlayRepository<Repository>> GroundAxiomSchedulerStratum::get_active_axioms() { return make_view(m_active, m_context); }

GroundAxiomSchedulerStrata
create_axiom_scheduler_strata(const GroundAxiomStrata& axioms, const GroundAxiomListenerStrata& listeners, const OverlayRepository<Repository>& context)
{
    assert(axioms.data.size() == listeners.data.size());

    auto result = GroundAxiomSchedulerStrata {};
    for (uint_t i = 0; i < axioms.data.size(); ++i)
        result.data.emplace_back(axioms.data[i], listeners.data[i], context);

    return result;
}

}
