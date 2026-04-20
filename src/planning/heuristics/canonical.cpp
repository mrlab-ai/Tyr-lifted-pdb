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

#include "tyr/planning/heuristics/canonical.hpp"

#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/graphs/bron_kerbosch.hpp"
#include "tyr/planning/formatter.hpp"
#include "tyr/planning/ground_task.hpp"
#include "tyr/planning/heuristics/projection_abstraction.hpp"
#include "tyr/planning/lifted_task.hpp"

namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;
namespace u = tyr::formalism::unification;

namespace tyr::planning
{
namespace
{
template<TaskKind Kind>
auto create_component_heuristics(const ProjectionAbstractionList<Kind>& projections)
{
    auto result = std::vector<std::shared_ptr<Heuristic<Kind>>> {};
    for (const auto& projection : projections)
        result.push_back(ProjectionAbstractionHeuristic<Kind>::create(projection));

    return result;
}

using Binding = u::SubstitutionFunction<Index<f::Object>>;
using LabelsByAction = UnorderedMap<fp::ActionView, std::vector<Binding>>;

template<typename T>
bool substitutions_equal_exact(const u::SubstitutionFunction<T>& lhs, const u::SubstitutionFunction<T>& rhs)
{
    if (lhs.parameters() != rhs.parameters())
        return false;

    for (const auto p : lhs.parameters())
    {
        if (lhs[p] != rhs[p])
            return false;
    }

    return true;
}

template<typename T>
bool substitutions_compatible(const u::SubstitutionFunction<T>& lhs, const u::SubstitutionFunction<T>& rhs)
{
    for (const auto p : lhs.parameters())
    {
        const auto* lhs_slot = lhs.try_get(p);
        const auto* rhs_slot = rhs.try_get(p);

        if (lhs_slot == nullptr || rhs_slot == nullptr)
            continue;

        if (!lhs_slot->has_value() || !rhs_slot->has_value())
            continue;

        if (**lhs_slot != **rhs_slot)
            return false;
    }

    return true;
}

template<typename T>
void push_unique_exact(std::vector<u::SubstitutionFunction<T>>& vec, const u::SubstitutionFunction<T>& sigma)
{
    if (std::none_of(vec.begin(), vec.end(), [&](const auto& other) { return substitutions_equal_exact(other, sigma); }))
    {
        vec.push_back(sigma);
    }
}

template<TaskKind Kind>
auto collect_labels(const ProjectionAbstractionList<Kind>& projections)
{
    auto result = std::vector<LabelsByAction> {};
    result.reserve(projections.size());

    for (const auto& projection : projections)
    {
        auto labels = LabelsByAction {};

        const auto& changing = projection.state_changing_transitions();
        for (auto t = changing.find_first(); t != boost::dynamic_bitset<>::npos; t = changing.find_next(t))
        {
            const auto& transition = projection.transitions()[t];
            push_unique_exact(labels[transition.original_action], transition.substitution);
        }

        result.push_back(std::move(labels));
    }

    return result;
}

bool labels_overlap(const LabelsByAction& lhs, const LabelsByAction& rhs)
{
    for (const auto& [action, lhs_bindings] : lhs)
    {
        const auto it = rhs.find(action);
        if (it == rhs.end())
            continue;

        const auto& rhs_bindings = it->second;

        for (const auto& sigma_l : lhs_bindings)
        {
            for (const auto& sigma_r : rhs_bindings)
            {
                if (substitutions_compatible(sigma_l, sigma_r))
                    return true;
            }
        }
    }

    return false;
}

template<TaskKind Kind>
auto create_additivity_graph(const ProjectionAbstractionList<Kind>& projections)
{
    const auto k = projections.size();
    const auto labels = collect_labels(projections);

    // Initialize empty dense graph with k vertices.
    auto graph = graphs::bron_kerbosch::Graph(k);

    for (uint_t i = 0; i < k; ++i)
    {
        for (uint_t j = i + 1; j < k; ++j)
        {
            const bool overlaps = labels_overlap(labels[i], labels[j]);

            if (!overlaps)
            {
                graph.matrix[i].set(j);
                graph.matrix[j].set(i);
            }
        }
    }

    return graph;
}

}

template<TaskKind Kind>
CanonicalHeuristic<Kind>::CanonicalHeuristic(const ProjectionAbstractionList<Kind>& projections) :
    m_components(create_component_heuristics(projections)),
    m_additive_partitions(graphs::bron_kerbosch::compute_maximal_cliques(create_additivity_graph(projections)))
{
}

template<TaskKind Kind>
std::shared_ptr<CanonicalHeuristic<Kind>> CanonicalHeuristic<Kind>::create(const ProjectionAbstractionList<Kind>& projections)
{
    return std::make_shared<CanonicalHeuristic<Kind>>(projections);
}

template<TaskKind Kind>
float_t CanonicalHeuristic<Kind>::evaluate(const StateView<Kind>& state)
{
    float_t h = 0;
    for (const auto& partition : m_additive_partitions)
    {
        float_t h_sum = 0;
        for (const auto& i : partition)
        {
            const auto h_i = m_components[i];
            h_sum += h_i->evaluate(state);
        }

        h = std::max(h, h_sum);
    }
    return h;
}

template class CanonicalHeuristic<LiftedTag>;
template class CanonicalHeuristic<GroundTag>;

}