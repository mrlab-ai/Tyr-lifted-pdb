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

#include "tyr/formalism/declarations.hpp"
#include "tyr/planning/ground_task/match_tree/declarations.hpp"
#include "tyr/planning/ground_task/match_tree/options.hpp"
#include "tyr/planning/ground_task/match_tree/repository.hpp"
#include "tyr/planning/ground_task/match_tree/statistics.hpp"

namespace tyr::planning::match_tree
{
/* MatchTree */
template<typename Tag>
class MatchTreeImpl
{
private:
    RepositoryPtr<Tag> m_context;
    Options m_options;

    View<Index<Node<Tag>>, Repository<Tag>> m_root;
    Statistics m_statistics;

    std::vector<View<Index<Node<Tag>>, Repository<Tag>>> m_evaluate_stack;  ///< temporary during evaluation.

    MatchTreeImpl();

    // MatchTreeImpl(View<IndexList<Tag>, C> elements, const Options& options = Options());

public:
    template<typename formalism::Context C>
    static std::unique_ptr<MatchTreeImpl<Tag>> create(View<IndexList<Tag>, C> elements, const Options& options = Options());

    // Uncopieable and unmoveable to prohibit invalidating spans on m_elements.
    MatchTreeImpl(const MatchTreeImpl& other) = delete;
    MatchTreeImpl& operator=(const MatchTreeImpl& other) = delete;
    MatchTreeImpl(MatchTreeImpl&& other) = delete;
    MatchTreeImpl& operator=(MatchTreeImpl&& other) = delete;

    void generate_applicable_elements_iteratively(const UnpackedStateImpl& state, IndexList<Tag>& out_applicable_elements);

    const Statistics& get_statistics() const;
};

}

#endif
