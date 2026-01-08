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

#ifndef TYR_DATALOG_STATISTICS_RULE_HPP_
#define TYR_DATALOG_STATISTICS_RULE_HPP_

#include <chrono>

namespace tyr::datalog
{
struct RuleStatistics
{
    uint64_t num_executions = 0;
    std::chrono::nanoseconds parallel_time { 0 };
};

struct AggregatedRuleStatistics
{
    size_t sample_count { 0 };
    std::chrono::nanoseconds parallel_time_min { 0 };
    std::chrono::nanoseconds parallel_time_max { 0 };
    std::chrono::nanoseconds parallel_time_median { 0 };
};

inline AggregatedRuleStatistics compute_aggregated_rule_statistics(const std::vector<datalog::RuleStatistics>& rules)
{
    AggregatedRuleStatistics result {};

    std::vector<std::chrono::nanoseconds> samples;
    samples.reserve(rules.size());

    for (const auto& rs : rules)
    {
        if (rs.num_executions == 0)
            continue;
        samples.push_back(rs.parallel_time);
    }

    result.sample_count = samples.size();
    if (samples.empty())
        return result;

    std::sort(samples.begin(), samples.end(), [](auto a, auto b) { return a.count() < b.count(); });

    result.parallel_time_min = samples.front();
    result.parallel_time_max = samples.back();

    const std::size_t n = samples.size();
    if (n % 2 == 1)
    {
        result.parallel_time_median = samples[n / 2];
    }
    else
    {
        const auto a = samples[n / 2 - 1].count();
        const auto b = samples[n / 2].count();
        result.parallel_time_median = std::chrono::nanoseconds { (a + b) / 2 };
    }

    return result;
}

}

#endif
