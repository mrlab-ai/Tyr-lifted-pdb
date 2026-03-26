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

namespace tyr::planning
{
namespace
{
template<typename Task>
auto create_component_heuristics(const ProjectionAbstractionList<Task>& projections)
{
    auto result = std::vector<std::shared_ptr<Heuristic<Task>>> {};
    for (const auto& projection : projections)
        result.push_back(ProjectionAbstractionHeuristic<Task>::create(projection));

    return result;
}

using Label = std::pair<fp::ActionView, fp::ActionBindingView>;

struct LabelHash
{
    size_t operator()(const Label& label) const noexcept
    {
        size_t seed = 0;
        hash_combine(seed, label.first);
        const auto objects = label.second.get_objects();
        for (const auto object : objects)
            hash_combine(seed, object);
        return seed;
    }
};

struct LabelEqualTo
{
    bool operator()(const Label& lhs, const Label& rhs) const noexcept
    {
        if (lhs.first != rhs.first)
            return false;

        const auto lhs_objects = lhs.second.get_objects();
        const auto rhs_objects = rhs.second.get_objects();
        assert(lhs_objects.size() == rhs_objects.size());

        return std::equal(lhs_objects.begin(), lhs_objects.end(), rhs_objects.begin());
    }
};

using LabelSet = gtl::flat_hash_set<Label, LabelHash, LabelEqualTo>;

template<typename Task>
auto create_additivity_graph(const ProjectionAbstractionList<Task>& projections)
{
    const auto k = projections.size();

    auto label_sets = std::vector<LabelSet> {};
    label_sets.reserve(k);
    for (const auto& projection : projections)
    {
        auto label_set = LabelSet {};

        const auto& changing = projection.state_changing_transitions();
        const auto& mapping = projection.get_mapping();

        for (auto t = changing.find_first(); t != boost::dynamic_bitset<>::npos; t = changing.find_next(t))
        {
            const auto transition = projection.transitions()[t];

            // Original action (before projection) + projected ground action binding
            label_set.emplace(mapping.get_original_action(transition.label.get_action()), transition.label.get_row());
        }

        label_sets.push_back(std::move(label_set));
    }

    // Initialize empty dense graph with k vertices.
    auto graph = graphs::bron_kerbosch::Graph(k);

    for (uint_t i = 0; i < k; ++i)
    {
        const auto& li = label_sets[i];

        for (uint_t j = i + 1; j < k; ++j)
        {
            const auto& lj = label_sets[j];

            bool intersects = std::any_of(li.begin(), li.end(), [&](const auto& x) { return lj.contains(x); });

            if (!intersects)
            {
                graph.matrix[i].set(j);
                graph.matrix[j].set(i);
            }
        }
    }

    return graph;
}

}

template<typename Task>
CanonicalHeuristic<Task>::CanonicalHeuristic(const ProjectionAbstractionList<Task>& projections) :
    m_components(create_component_heuristics(projections)),
    m_additive_partitions(graphs::bron_kerbosch::compute_maximal_cliques(create_additivity_graph(projections)))
{
}

template<typename Task>
std::shared_ptr<CanonicalHeuristic<Task>> CanonicalHeuristic<Task>::create(const ProjectionAbstractionList<Task>& projections)
{
    return std::make_shared<CanonicalHeuristic<Task>>(projections);
}

template<typename Task>
float_t CanonicalHeuristic<Task>::evaluate(const StateView<Task>& state)
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

template class CanonicalHeuristic<LiftedTask>;
template class CanonicalHeuristic<GroundTask>;

}
