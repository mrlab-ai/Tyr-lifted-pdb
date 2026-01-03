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

#ifndef TYR_SOLVER_POLICIES_ANNOTATION_HPP_
#define TYR_SOLVER_POLICIES_ANNOTATION_HPP_

#include "tyr/common/config.hpp"
#include "tyr/formalism/binding_index.hpp"
#include "tyr/formalism/datalog/declarations.hpp"
#include "tyr/formalism/datalog/ground_atom_index.hpp"
#include "tyr/formalism/datalog/grounder.hpp"
#include "tyr/formalism/datalog/repository.hpp"
#include "tyr/formalism/datalog/rule_index.hpp"
#include "tyr/formalism/overlay_repository.hpp"

#include <concepts>
#include <tuple>
#include <vector>

namespace tyr::datalog
{
/**
 * Annotations
 */

using Cost = uint_t;

using OrAnnotationsList = std::vector<Cost>;

/// @brief `Witness` is the rule and binding in the rule delta repository whose ground rule is the witness for its ground atom in the head.
/// The witness lives in the rule delta repository.
struct Witness
{
    Index<formalism::datalog::Rule> rule;
    Index<formalism::Binding> binding;

    auto identifying_members() const noexcept { return std::tie(rule, binding); }
};

using AndAnnotationsMap = UnorderedMap<Witness, Cost>;
using HeadToBinding = UnorderedMap<Index<formalism::datalog::GroundAtom<formalism::FluentTag>>, Index<formalism::Binding>>;

/// @brief `CostAnnotations` encapsulates the cost of nodes.
struct CostAnnotations
{
    OrAnnotationsList or_annot;
    std::vector<AndAnnotationsMap> and_annots;

    CostAnnotations(OrAnnotationsList or_annot, std::vector<AndAnnotationsMap> and_annots) : or_annot(std::move(or_annot)), and_annots(std::move(and_annots)) {}

    void clear() noexcept
    {
        or_annot.clear();
        for (auto& and_annot : and_annots)
            and_annot.clear();
    }
};

/**
 * Policies
 */

// circle "or"-node
template<typename T>
concept OrAnnotationPolicy = requires(T& p,
                                      Index<formalism::datalog::Rule> rule,
                                      Index<formalism::datalog::GroundAtom<formalism::FluentTag>> head,
                                      OrAnnotationsList& or_annot,
                                      const AndAnnotationsMap& and_annot,
                                      const HeadToBinding& head_to_binding) {
    /// Annotate the cost of the atom from the given witness and annotations.
    { p.annotate(rule, head, or_annot, and_annot, head_to_binding) } -> std::same_as<void>;
    /// Clear the policy for reuse.
    { p.clear() } -> std::same_as<void>;
};

// rectangular "and"-node
template<typename T>
concept AndAnnotationPolicy = requires(T& p,
                                       Index<formalism::datalog::Rule> rule,
                                       const IndexList<formalism::Object>& binding,
                                       Index<formalism::datalog::GroundAtom<formalism::FluentTag>> head,
                                       const OrAnnotationsList& or_annot,
                                       AndAnnotationsMap& and_annot,
                                       HeadToBinding& head_to_binding,
                                       formalism::datalog::GrounderContext<formalism::OverlayRepository<formalism::datalog::Repository>>& rule_context,
                                       formalism::datalog::GrounderContext<formalism::datalog::Repository>& delta_context) {
    /// Ground the witness and annotate the cost of it from the given annotations.
    { p.annotate(rule, binding, head, or_annot, and_annot, head_to_binding, rule_context, delta_context) } -> std::same_as<void>;
    /// Clear the policy for reuse.
    { p.clear() } -> std::same_as<void>;
};

class NoOrAnnotationPolicy
{
public:
    void annotate(Index<formalism::datalog::Rule> rule,
                  Index<formalism::datalog::GroundAtom<formalism::FluentTag>> head,
                  OrAnnotationsList& or_annot,
                  const AndAnnotationsMap& and_annot,
                  const HeadToBinding& head_to_binding) noexcept
    {
    }
    void clear() noexcept {}
};

class NoAndAnnotationPolicy
{
public:
    void annotate(Index<formalism::datalog::Rule> rule,
                  const IndexList<formalism::Object>& binding,
                  Index<formalism::datalog::GroundAtom<formalism::FluentTag>> head,
                  const OrAnnotationsList& or_annot,
                  AndAnnotationsMap& and_annot,
                  HeadToBinding& head_to_binding,
                  formalism::datalog::GrounderContext<formalism::OverlayRepository<formalism::datalog::Repository>>& rule_context,
                  formalism::datalog::GrounderContext<formalism::datalog::Repository>& delta_context) noexcept
    {
    }
    void clear() noexcept {}
};

template<OrAnnotationPolicy OrAP, AndAnnotationPolicy AndAP>
struct AnnotationPolicies
{
    OrAP or_ap;
    std::vector<AndAP> and_aps;

    OrAnnotationsList or_annot;
    std::vector<AndAnnotationsMap> and_annots;

    std::vector<HeadToBinding> head_to_bindings;

    AnnotationPolicies(OrAP or_ap,
                       std::vector<AndAP> and_aps,
                       OrAnnotationsList or_annot,
                       std::vector<AndAnnotationsMap> and_annots,
                       std::vector<HeadToBinding> head_to_bindings) :
        or_ap(std::move(or_ap)),
        and_aps(std::move(and_aps)),
        or_annot(std::move(or_annot)),
        and_annots(std::move(and_annots)),
        head_to_bindings(std::move(head_to_bindings))
    {
    }

    void clear() noexcept
    {
        or_ap.clear();
        for (auto& and_ap : and_aps)
            and_ap.clear();

        or_annot.clear();
        for (auto& and_annot : and_annots)
            and_annot.clear();

        head_to_bindings.clear();
    }
};

}

#endif
