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

#ifndef TYR_DATALOG_WORKSPACES_PROGRAM_HPP_
#define TYR_DATALOG_WORKSPACES_PROGRAM_HPP_

#include "tyr/datalog/program_context.hpp"
#include "tyr/datalog/rule_scheduler.hpp"
#include "tyr/datalog/workspaces/d2p.hpp"
#include "tyr/datalog/workspaces/facts.hpp"
#include "tyr/datalog/workspaces/p2d.hpp"
#include "tyr/datalog/workspaces/rule.hpp"
#include "tyr/datalog/workspaces/rule_delta.hpp"
#include "tyr/datalog/workspaces/worker.hpp"
#include "tyr/formalism/datalog/builder.hpp"
#include "tyr/formalism/planning/builder.hpp"

#include <chrono>
#include <oneapi/tbb/enumerable_thread_specific.h>

namespace tyr::datalog
{
struct ProgramWorkspace
{
    formalism::datalog::Repository& repository;
    FactsWorkspace facts;

    std::vector<RuleWorkspace> rules;
    std::vector<RuleDeltaWorkspace> rule_deltas;

    D2PWorkspace d2p;
    P2DWorkspace p2d;

    oneapi::tbb::enumerable_thread_specific<WorkerWorkspace> worker;

    /// --- Builder
    formalism::planning::Builder planning_builder;
    formalism::datalog::Builder datalog_builder;

    // --- Scheduler
    RuleSchedulerStrata rule_scheduler_strata;

    struct Statistics
    {
        std::chrono::nanoseconds ground_seq_total_time { 0 };
        std::chrono::nanoseconds merge_seq_total_time { 0 };

        size_t num_merges_inserted { 0 };
        size_t num_merges_discarded { 0 };
    } statistics;

    explicit ProgramWorkspace(ProgramContext& context, const ConstProgramWorkspace& cws);
};

struct ConstProgramWorkspace
{
    ConstFactsWorkspace facts;

    std::vector<ConstRuleWorkspace> rules;

    explicit ConstProgramWorkspace(ProgramContext& context);
};
}

#endif
