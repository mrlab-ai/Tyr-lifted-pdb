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

#ifndef TYR_PLANNING_GROUND_TASK_MATCH_TREE_DECLARATIONS_HPP_
#define TYR_PLANNING_GROUND_TASK_MATCH_TREE_DECLARATIONS_HPP_

#include "tyr/formalism/declarations.hpp"

#include <concepts>
#include <variant>
#include <vector>

namespace tyr::planning::match_tree
{
/**
 * Forward declarations
 */

template<typename Tag>
struct PlaceholderNode
{
};

/**
 * InverseNode
 */

template<typename Tag>
struct InverseAtomSelectorNode
{
};

template<typename Tag>
struct InverseFactSelectorNode
{
};

template<typename Tag>
struct InverseNumericConstraintSelectorNode
{
};

template<typename Tag>
struct InverseElementGeneratorNode
{
};

template<typename Tag>
struct InverseNode
{
};

/**
 * Node
 */

template<typename Tag>
struct AtomSelectorNode
{
};

template<typename Tag>
struct FactSelectorNode
{
};

template<typename Tag>
struct NumericConstraintSelectorNode
{
};

template<typename Tag>
struct ElementGeneratorNode
{
};

template<typename Tag>
struct Node
{
};

/**
 * NodeScoreFunction
 */

template<typename Tag>
class INodeScoreFunction;

/**
 * NodeSplitter
 */

template<typename Tag>
class INodeSplitter;

struct Options;
struct Statistics;

template<typename Tag>
class MatchTreeImpl;
template<typename Tag>
using MatchTree = std::unique_ptr<MatchTreeImpl<E>>;
template<typename Tag>
using MatchTreeList = std::vector<MatchTree<E>>;

/**
 * Aliases
 */

template<typename Tag>
using PlaceholderNodeList = std::vector<PlaceholderNode<E>>;

template<typename Tag>
using NodeScoreFunction = std::unique_ptr<INodeScoreFunction<E>>;

template<typename Tag>
using NodeSplitter = std::unique_ptr<INodeSplitter<E>>;

template<formalism::Context C, typename Tag>
class Repository;
template<formalism::Context C, typename Tag>
using RepositoryPtr = std::unique_ptr<Repository>;
}

#endif
