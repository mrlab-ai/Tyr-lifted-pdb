/*
 * Copyright (C) 2025-2026 Dominik Drexler
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

#ifndef TYR_DATALOG_CONTEXTS_STRATUM_HPP_
#define TYR_DATALOG_CONTEXTS_STRATUM_HPP_

#include "tyr/datalog/contexts/rule.hpp"
#include "tyr/datalog/declarations.hpp"
#include "tyr/datalog/policies/annotation_concept.hpp"
#include "tyr/datalog/policies/termination_concept.hpp"

namespace tyr::datalog
{
template<OrAnnotationPolicyConcept OrAP, AndAnnotationPolicyConcept AndAP, TerminationPolicyConcept TP>
struct ProgramExecutionContext;

template<OrAnnotationPolicyConcept OrAP, AndAnnotationPolicyConcept AndAP, TerminationPolicyConcept TP>
struct StratumExecutionContext
{
    class In
    {
    public:
        explicit In(const ProgramExecutionContext<OrAP, AndAP, TP>& ctx) : m_ctx(ctx) {}

        const auto& program() const noexcept { return m_ctx.in(); }

    private:
        const ProgramExecutionContext<OrAP, AndAP, TP>& m_ctx;
    };

    class Out
    {
    public:
        Out(RuleSchedulerStratum& scheduler, ProgramExecutionContext<OrAP, AndAP, TP>& ctx) : m_scheduler(scheduler), m_ctx(ctx) {}

        auto& scheduler() noexcept { return m_scheduler; }
        const auto& scheduler() const noexcept { return m_scheduler; }
        auto& program() noexcept { return m_ctx.out(); }
        const auto& program() const noexcept { return m_ctx.out(); }

    private:
        RuleSchedulerStratum& m_scheduler;
        ProgramExecutionContext<OrAP, AndAP, TP>& m_ctx;
    };

    StratumExecutionContext(RuleSchedulerStratum& scheduler, ProgramExecutionContext<OrAP, AndAP, TP>& ctx) :
        m_in(ctx),
        m_out(scheduler, ctx)
    {
    }

    /**
     * Initialization
     */

    /**
     * Subcontext
     */

    auto get_rule_execution_context(Index<formalism::datalog::Rule> rule) { return RuleExecutionContext<OrAP, AndAP, TP> { rule, *this }; }

    const auto& in() const noexcept { return m_in; }
    auto& out() noexcept { return m_out; }
    const auto& out() const noexcept { return m_out; }

private:
    In m_in;
    Out m_out;
};
}

#endif
