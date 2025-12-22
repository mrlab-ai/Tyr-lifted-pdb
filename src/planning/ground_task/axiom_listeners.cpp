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

#include "tyr/planning/ground_task/axiom_listeners.hpp"

using namespace tyr::formalism;

namespace tyr::planning
{

GroundAxiomListenerStrata compute_listeners(const GroundAxiomStrata& strata, const OverlayRepository<Repository>& context)
{
    auto listeners = GroundAxiomListenerStrata();

    for (const auto& stratum : strata.data)
    {
        auto listeners_in_stratum = GroundAxiomListenerStratum {};

        for (const auto axiom : stratum)
            for (const auto literal : make_view(axiom, context).get_body().get_facts<DerivedTag>())
                if (literal.get_polarity())
                    listeners_in_stratum[literal.get_atom().get_index()].insert(axiom);

        listeners.data.push_back(std::move(listeners_in_stratum));
    }

    // std::cout << listeners.data << std::endl;

    return listeners;
}
}
