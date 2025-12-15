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

#include <concepts>
#include <variant>
#include <vector>

namespace tyr::planning::match_tree
{

template<typename T>
concept HasConjunctiveCondition = requires(T a) {
    { a.get_condition() };
};

/**
 * Forward declarations
 */

template<HasConjunctiveCondition E>
class PlaceholderNodeImpl;

/**
 * InverseNode
 */

template<HasConjunctiveCondition E>
class IInverseNode;

template<HasConjunctiveCondition E, bool TrueChild, bool FalseChild, bool DontCareChild>
class InverseAtomSelectorNode_TFX;
template<HasConjunctiveCondition E>
class InverseAtomSelectorNode_General;

template<HasConjunctiveCondition E, bool TrueChild, bool FalseChild, bool DontCareChild>
class InverseFactSelectorNode_Binary;
template<HasConjunctiveCondition E>
class InverseFactSelectorNode_General;

template<HasConjunctiveCondition E>
class InverseNumericConstraintSelectorNode_TX;
template<HasConjunctiveCondition E>
class InverseNumericConstraintSelectorNode_T;
template<HasConjunctiveCondition E>

class InverseElementGeneratorNode_Perfect;
template<HasConjunctiveCondition E>
class InverseElementGeneratorNode_Imperfect;

/**
 * Node
 */

template<HasConjunctiveCondition E>
class INode;

template<HasConjunctiveCondition E, bool TrueChild, bool FalseChild, bool DontCareChild>
class AtomSelectorNode_Binary;
template<HasConjunctiveCondition E>
class AtomSelectorNode_General;

template<HasConjunctiveCondition E, bool TrueChild, bool FalseChild, bool DontCareChild>
class FactSelectorNode_Binary;
template<HasConjunctiveCondition E>
class FactSelectorNode_General;

template<HasConjunctiveCondition E>
class NumericConstraintSelectorNode_TX;
template<HasConjunctiveCondition E>
class NumericConstraintSelectorNode_T;

template<HasConjunctiveCondition E>
class ElementGeneratorNode_Perfect;
template<HasConjunctiveCondition E>
class ElementGeneratorNode_Imperfect;

template<HasConjunctiveCondition E>
class INodeScoreFunction;

template<HasConjunctiveCondition E>
class INodeSplitter;

struct Options;
struct Statistics;

template<HasConjunctiveCondition E>
class MatchTreeImpl;
template<HasConjunctiveCondition E>
using MatchTree = std::unique_ptr<MatchTreeImpl<E>>;
template<HasConjunctiveCondition E>
using MatchTreeList = std::vector<MatchTree<E>>;

/**
 * Aliases
 */

template<HasConjunctiveCondition E>
using PlaceholderNode = std::unique_ptr<PlaceholderNodeImpl<E>>;
template<HasConjunctiveCondition E>
using PlaceholderNodeList = std::vector<PlaceholderNode<E>>;

template<HasConjunctiveCondition E>
using InverseNode = std::unique_ptr<IInverseNode<E>>;
template<HasConjunctiveCondition E>
using InverseNodeList = std::vector<InverseNode<E>>;

template<HasConjunctiveCondition E>
using Node = std::unique_ptr<INode<E>>;

template<HasConjunctiveCondition E>
using NodeScoreFunction = std::unique_ptr<INodeScoreFunction<E>>;

template<HasConjunctiveCondition E>
using NodeSplitter = std::unique_ptr<INodeSplitter<E>>;
}

#endif
