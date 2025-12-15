/*
 * Copyright (C) 2023 Dominik Drexler and Simon Stahlberg
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

#ifndef TYR_PLANNING_GROUND_TASK_MATCH_TREE_MATCH_TREE_HPP_
#define TYR_PLANNING_GROUND_TASK_MATCH_TREE_MATCH_TREE_HPP_

#include "tyr/planning/ground_task/match_tree/declarations.hpp"
#include "tyr/planning/ground_task/match_tree/node_splitters/interface.hpp"
#include "tyr/planning/ground_task/match_tree/nodes/interface.hpp"
#include "tyr/planning/ground_task/match_tree/options.hpp"
#include "tyr/planning/ground_task/match_tree/statistics.hpp"

namespace tyr::planning::match_tree
{
/* MatchTree */
template<HasConjunctiveCondition E>
class MatchTreeImpl
{
private:
    std::vector<E> m_elements;  ///< ATTENTION: must remain persistent. Swapping elements is allowed.
    Options m_options;

    Node<E> m_root;
    Statistics m_statistics;

    std::vector<const INode<E>*> m_evaluate_stack;  ///< temporary during evaluation.

    MatchTreeImpl();

    MatchTreeImpl(std::vector<E> elements, const Options& options = Options());

public:
    static std::unique_ptr<MatchTreeImpl<E>> create(std::vector<E> elements, const Options& options = Options());

    // Uncopieable and unmoveable to prohibit invalidating spans on m_elements.
    MatchTreeImpl(const MatchTreeImpl& other) = delete;
    MatchTreeImpl& operator=(const MatchTreeImpl& other) = delete;
    MatchTreeImpl(MatchTreeImpl&& other) = delete;
    MatchTreeImpl& operator=(MatchTreeImpl&& other) = delete;

    void generate_applicable_elements_iteratively(const UnpackedStateImpl& state, std::vector<E>& out_applicable_elements);

    const Statistics& get_statistics() const;
};

}

#endif
