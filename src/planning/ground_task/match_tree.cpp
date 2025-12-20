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

#include "tyr/planning/ground_task/match_tree/match_tree.hpp"

#include <deque>

using namespace tyr::formalism;

namespace tyr::planning::match_tree
{
template<formalism::Context C>
auto get_condition(View<Index<GroundAxiom>, C> element)
{
    return element.get_body();
}

template<formalism::Context C>
auto get_condition(View<Index<GroundAction>, C> element)
{
    return element.get_condition();
}

template<typename Tag>
template<formalism::Context C>
MatchTree<Tag>::MatchTree(View<IndexList<Tag>, C> elements) :
    m_elements(elements.get_data()),
    m_context(std::make_shared<Repository<formalism::OverlayRepository<formalism::Repository>, Tag>>(elements.get_context())),
    m_root(),
    m_evaluate_stack()
{
    if (m_elements.empty())
    {
        // TODO
        return;
    }

    using PreconditionVariant = std::variant<Index<GroundAtom<DerivedTag>>, Index<FDRVariable<FluentTag>>, Data<BooleanOperator<GroundFunctionExpression>>>;

    struct Occurrence
    {
        Index<Tag> element;
        std::variant<std::monostate, bool, FDRValue> detail;
    };

    using PostingList = std::vector<Occurrence>;

    using PreconditionOccurences = UnorderedMap<PreconditionVariant, PostingList>;

    using PreconditionDetails = UnorderedMap<Index<Tag>, UnorderedMap<PreconditionVariant, std::variant<bool, FDRValue>>>;

    auto occurences = PreconditionOccurences {};
    auto details = PreconditionDetails {};

    for (const auto element : elements)
    {
        const auto condition = get_condition(element);

        for (const auto& fact : condition.template get_facts<FluentTag>())
        {
            auto key = fact.get_variable().get_index();
            occurences[key].push_back({ element.get_index(), fact.get_value() });
        }

        for (const auto& literal : condition.template get_facts<DerivedTag>())
        {
            auto key = literal.get_atom().get_index();
            occurences[key].push_back({ element.get_index(), literal.get_polarity() });
        }

        for (const auto& constraint : condition.get_numeric_constraints())
        {
            occurences[constraint].push_back({ element.get_index(), std::monostate {} });
        }
    }

    std::vector<std::pair<PreconditionVariant, IndexList<Tag>>> sorted_preconditions(occurences.begin(), occurences.end());

    std::sort(sorted_preconditions.begin(), sorted_preconditions.end(), [](const auto& a, const auto& b) { return a.second.size() > b.second.size(); });

    struct StackEntry
    {
        size_t depth;
        std::span<const Index<Tag>> elements;
    };

    auto stack = std::deque<StackEntry> {};
    stack.emplace_back(0, std::span(m_elements.begin(), m_elements.end()));

    while (!stack.empty())
    {
        auto entry = stack.back();
        stack.pop_back();

        if (stack.depth == sorted_preconditions.size())
        {
            /// Checked all preconditions => create generator node
        }
        else
        {
            std::visit(
                [](auto&& arg)
                {
                    using Alternative = std::decay_t<decltype(arg)>;

                    if constexpr (std::same_as<Alternative, Index<FDRVariable<FluentTag>>>)
                    {
                        // Create fact node
                    }
                    else if constexpr (std::same_as<Alternative, Index<GroundAtom<DerivedTag>>>)
                    {
                        // Create atom node
                    }
                    else if constexpr (std::same_as<Alternative, Data<BooleanOperator<GroundFunctionExpression>>>)
                    {
                        // Create constraint node
                    }
                    else
                        static_assert(dependent_false<Alternative>::value, "Missing case");
                },
                sorted_preconditions[stack.depth].first);
        }
    }
}

template<typename Tag>
template<formalism::Context C>
std::unique_ptr<MatchTree<Tag>> MatchTree<Tag>::create(View<IndexList<Tag>, C> elements)
{
    return std::make_unique<MatchTree<Tag>>(elements);
}

template<typename Tag>
void MatchTree<Tag>::generate_applicable_elements_iteratively(const UnpackedState<GroundTask>& state, IndexList<Tag>& out_applicable_elements)
{
}

}
