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

#include "proof.hpp"

#include "matching.hpp"
#include "tyr/formalism/planning/invariants/formatter.hpp"

namespace tyr::formalism::planning::invariant
{
namespace
{
bool is_effect_local_parameter(ParameterIndex parameter, size_t num_action_variables) { return static_cast<uint_t>(parameter) >= num_action_variables; }

uint_t get_effect_local_index(ParameterIndex parameter, size_t num_action_variables) { return static_cast<uint_t>(parameter) - num_action_variables; }

bool is_contradictory(const TempLiteral& lhs, const TempLiteral& rhs) { return lhs.atom == rhs.atom && lhs.polarity != rhs.polarity; }

bool satisfiable(const TempLiteralList& lits)
{
    for (size_t i = 0; i < lits.size(); ++i)
    {
        for (size_t j = i + 1; j < lits.size(); ++j)
        {
            if (is_contradictory(lits[i], lits[j]))
                return false;
        }
    }

    return true;
}

bool entails(const TempLiteralList& lhs, const TempLiteralList& rhs)
{
    for (const auto& rhs_lit : rhs)
    {
        const auto it = std::find_if(lhs.begin(), lhs.end(), [&](const auto& lhs_lit) { return lhs_lit == rhs_lit; });

        if (it == lhs.end())
            return false;
    }

    return true;
}

Data<Term> alpha_rename_effect_term(const Data<Term>& term, size_t num_action_variables, size_t fresh_base)
{
    return std::visit(
        [&](auto&& arg) -> Data<Term>
        {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, ParameterIndex>)
            {
                if (!is_effect_local_parameter(arg, num_action_variables))
                    return term;

                const auto local_index = get_effect_local_index(arg, num_action_variables);
                return Data<Term>(ParameterIndex(fresh_base + local_index));
            }
            else if constexpr (std::is_same_v<T, Index<Object>>)
            {
                return term;
            }
            else
            {
                static_assert(dependent_false<T>::value, "Missing case");
            }
        },
        term.value);
}

TempAtom alpha_rename_effect_atom(const TempAtom& atom, size_t num_action_variables, size_t fresh_base)
{
    auto terms = std::vector<Data<Term>> {};
    terms.reserve(atom.terms.size());

    for (const auto& term : atom.terms)
        terms.push_back(alpha_rename_effect_term(term, num_action_variables, fresh_base));

    return TempAtom {
        .predicate = atom.predicate,
        .terms = std::move(terms),
    };
}

TempLiteral alpha_rename_effect_literal(const TempLiteral& lit, size_t num_action_variables, size_t fresh_base)
{
    return TempLiteral {
        .atom = alpha_rename_effect_atom(lit.atom, num_action_variables, fresh_base),
        .polarity = lit.polarity,
    };
}

TempLiteralList alpha_rename_effect_condition(const TempLiteralList& lits, size_t num_action_variables, size_t fresh_base)
{
    auto result = TempLiteralList {};
    result.reserve(lits.size());

    for (const auto& lit : lits)
        result.push_back(alpha_rename_effect_literal(lit, num_action_variables, fresh_base));

    return result;
}
}

bool is_operator_too_heavy(const TempAction& op, const Invariant& inv)
{
    size_t max_num_effect_variables = 0;
    for (const auto& eff : op.effects)
        max_num_effect_variables = std::max(max_num_effect_variables, eff.num_effect_variables);

    const size_t fresh_base_lhs = op.num_variables;
    const size_t fresh_base_rhs = op.num_variables + max_num_effect_variables + 1;

    for (const auto& eff_lhs_raw : op.effects)
    {
        const auto eff_lhs_cond = alpha_rename_effect_condition(eff_lhs_raw.condition, op.num_variables, fresh_base_lhs);

        for (const auto& eff_rhs_raw : op.effects)
        {
            const auto eff_rhs_cond = alpha_rename_effect_condition(eff_rhs_raw.condition, op.num_variables, fresh_base_rhs);

            for (const auto& atom_lhs_raw : eff_lhs_raw.add_effects)
            {
                if (!inv.predicates.contains(atom_lhs_raw.predicate))
                    continue;

                const auto atom_lhs_alpha = alpha_rename_effect_atom(atom_lhs_raw, op.num_variables, fresh_base_lhs);

                for (const auto& sigma_op : enumerate_action_alignments(inv, atom_lhs_alpha, op.num_variables))
                {
                    const auto atom_lhs = apply_substitution(atom_lhs_alpha, sigma_op);

                    if (!covers(inv, atom_lhs))
                        continue;

                    for (const auto& atom_rhs_raw : eff_rhs_raw.add_effects)
                    {
                        if (!inv.predicates.contains(atom_rhs_raw.predicate))
                            continue;

                        const auto atom_rhs_alpha = alpha_rename_effect_atom(atom_rhs_raw, op.num_variables, fresh_base_rhs);

                        const auto atom_rhs = apply_substitution(atom_rhs_alpha, sigma_op);

                        if (atom_lhs == atom_rhs)
                            continue;

                        if (!covers(inv, atom_rhs))
                            continue;

                        auto lhs = apply_substitution(op.precondition, sigma_op);

                        auto cond_lhs = apply_substitution(eff_lhs_cond, sigma_op);
                        lhs.insert(lhs.end(), cond_lhs.begin(), cond_lhs.end());

                        auto cond_rhs = apply_substitution(eff_rhs_cond, sigma_op);
                        lhs.insert(lhs.end(), cond_rhs.begin(), cond_rhs.end());

                        lhs.push_back(TempLiteral { .atom = atom_lhs, .polarity = false });
                        lhs.push_back(TempLiteral { .atom = atom_rhs, .polarity = false });

                        if (satisfiable(lhs))
                            return true;
                    }
                }
            }
        }
    }

    return false;
}

bool is_add_effect_unbalanced(const TempAction& op, const TempEffect& effect, const TempAtom& add_atom, const Invariant& inv)
{
    for (const auto& sigma_op : enumerate_action_alignments(inv, add_atom, op.num_variables))
    {
        const auto e_atom = apply_substitution(add_atom, sigma_op);

        auto lhs = apply_substitution(op.precondition, sigma_op);
        auto e_cond = apply_substitution(effect.condition, sigma_op);
        lhs.insert(lhs.end(), e_cond.begin(), e_cond.end());
        lhs.push_back(TempLiteral { .atom = e_atom, .polarity = false });

        for (const auto& del_eff : op.effects)
        {
            for (const auto& eprime_raw : del_eff.del_effects)
            {
                const auto eprime_partial = apply_substitution(eprime_raw, sigma_op);

                for (const auto& sigma_eff : enumerate_effect_renamings(del_eff, eprime_raw, inv, sigma_op))
                {
                    const auto eprime = apply_substitution(eprime_partial, sigma_eff, del_eff.num_action_variables);

                    if (e_atom == eprime)
                        continue;

                    if (!covers(inv, eprime))
                        continue;

                    auto rhs = apply_substitution(del_eff.condition, sigma_op);
                    rhs = apply_substitution(rhs, sigma_eff, del_eff.num_action_variables);
                    rhs.push_back(TempLiteral { .atom = eprime, .polarity = true });

                    if (entails(lhs, rhs))
                        return false;
                }
            }
        }
    }

    return true;
}

ProofResult prove_invariant(const Invariant& inv, const TempActionList& ops)
{
    for (size_t op_index = 0; op_index < ops.size(); ++op_index)
    {
        const auto& op = ops[op_index];

        if (is_operator_too_heavy(op, inv))
            return { ProofStatus::TooHeavy, Threat { op_index, 0, 0 } };  // { Reject the candidate. }

        for (size_t effect_index = 0; effect_index < op.effects.size(); ++effect_index)
        {
            const auto& conj_eff = op.effects[effect_index];

            for (size_t add_index = 0; add_index < conj_eff.add_effects.size(); ++add_index)
            {
                const auto& atom = conj_eff.add_effects[add_index];

                if (!inv.predicates.contains(atom.predicate))
                    continue;

                if (is_add_effect_unbalanced(op, conj_eff, atom, inv))
                {
                    std::cout << atom << std::endl;
                    return { ProofStatus::UnbalancedAddEffect, Threat { op_index, effect_index, add_index } };  // { Reject the candidate. }
                }
            }
        }
    }

    return { ProofStatus::Proven, std::nullopt };  // { Accept the candidate. }
}
}
