

// Miconic FD
// [<Invariant {origin((0, 1)) [omitted_pos = None]}>,
// <Invariant {origin((-1, 0)) [omitted_pos = 0]}>,
// <Invariant {origin((0, -1)) [omitted_pos = 1]}>,
// <Invariant {lift-at((-1,)) [omitted_pos = 0]}>,
// <Invariant {boarded((0,)) [omitted_pos = None], origin((0, -1)) [omitted_pos = 1]}>,
// <Invariant {boarded((0,)) [omitted_pos = None], origin((0, -1)) [omitted_pos = 1], served((0,)) [omitted_pos = None]}>]

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

#include "tyr/formalism/formalism.hpp"

#include <gtest/gtest.h>

namespace b = tyr::buffer;
namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;
namespace fpi = tyr::formalism::planning::invariant;

namespace tyr::tests
{

namespace
{
auto compute_lifted_task(const fs::path& domain_filepath, const fs::path& problem_filepath) { return fp::Parser(domain_filepath).parse_task(problem_filepath); }

fs::path absolute(const std::string& subdir) { return fs::path(std::string(DATA_DIR)) / subdir; }

inline std::vector<fpi::Invariant> sort_invariants(std::vector<fpi::Invariant> invariants)
{
    std::sort(invariants.begin(), invariants.end());
    return invariants;
}

inline void expect_invariant_sets_equal(std::vector<fpi::Invariant> actual, std::vector<fpi::Invariant> expected)
{
    std::sort(actual.begin(), actual.end());
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(actual, expected);
}

auto atom(fp::Repository& repo, std::string_view predicate_name, const std::vector<int>& parameters)
{
    auto predicate_builder = Data<f::Predicate<f::FluentTag>> { std::string(predicate_name), uint_t(parameters.size()) };
    const auto predicate = repo.get_or_create(predicate_builder).first;

    auto terms = std::vector<Data<f::Term>> {};
    for (const auto i : parameters)
        terms.push_back(Data<f::Term>(f::ParameterIndex(i)));

    return fpi::TempAtom { .predicate = predicate, .terms = terms };
}

auto inv(size_t num_rigid_variables, size_t num_counted_variables, std::initializer_list<fpi::TempAtom> atoms)
{
    return fpi::Invariant(num_rigid_variables, num_counted_variables, std::vector<fpi::TempAtom>(atoms));
}
}

/// @brief
/// Ground truth from Fast Downward:
///
/// <Invariant {current_stage((-1,)) [omitted_pos = 0]}>,
/// <Invariant {current_worker((-1,)) [omitted_pos = 0]}>,
/// <Invariant {max_worker((-1,)) [omitted_pos = 0]}>,
/// <Invariant {num_food((-1,)) [omitted_pos = 0]}>,
/// <Invariant {space_rooms((0,)) [omitted_pos = None]}>,
/// <Invariant {space_rooms((-1,)) [omitted_pos = 0]}>,
/// <Invariant {built_rooms((0, -1)) [omitted_pos = 1], space_rooms((0,)) [omitted_pos = None]}>
TEST(TyrTests, TyrFormalismPlanningInvariantsSynthesisAgricola)
{
    auto lifted_task = compute_lifted_task(absolute("agricola/domain.pddl"), absolute("agricola/test_problem.pddl"));
    auto& repository = *lifted_task.get_repository();

    auto actual = fpi::synthesize_invariants(lifted_task.get_task());

    auto expected = std::vector<fpi::Invariant> {
        inv(0,
            1,
            {
                atom(repository, "current_stage", { 0 }),
            }),
        inv(0,
            1,
            {
                atom(repository, "current_worker", { 0 }),
            }),
        inv(0,
            1,
            {
                atom(repository, "max_worker", { 0 }),
            }),
        inv(0,
            1,
            {
                atom(repository, "num_food", { 0 }),
            }),
        inv(1,
            0,
            {
                atom(repository, "space_rooms", { 0 }),
            }),
        inv(0,
            1,
            {
                atom(repository, "space_rooms", { 0 }),
            }),
        inv(1,
            1,
            {
                atom(repository, "built_rooms", { 0, 1 }),
                atom(repository, "space_rooms", { 0 }),
            }),
    };

    expect_invariant_sets_equal(actual, expected);
}

/// @brief
///
/// <Invariant {at-segment((0, -1)) [omitted_pos = 1]}>,
/// <Invariant {facing((0, -1)) [omitted_pos = 1]}>,
/// <Invariant {is-pushing((0,)) [omitted_pos = None]}>,
/// <Invariant {is-pushing((-1,)) [omitted_pos = 0]}>,
/// <Invariant {airborne((0, -1)) [omitted_pos = 1], at-segment((0, -1)) [omitted_pos = 1]}>,
/// <Invariant {is-moving((0,)) [omitted_pos = None], is-pushing((0,)) [omitted_pos = None]}>,
/// <Invariant {is-moving((-1,)) [omitted_pos = 0], is-pushing((-1,)) [omitted_pos = 0]}>,
/// <Invariant {is-moving((0,)) [omitted_pos = None], is-parked((0, -1)) [omitted_pos = 1], is-pushing((0,)) [omitted_pos = None]}>]
TEST(TyrTests, TyrFormalismPlanningInvariantsSynthesisAirport)
{
    auto lifted_task = compute_lifted_task(absolute("airport/domain.pddl"), absolute("airport/test_problem.pddl"));
    auto& repository = *lifted_task.get_repository();

    auto actual = fpi::synthesize_invariants(lifted_task.get_task());

    auto expected = std::vector<fpi::Invariant> {
        inv(1,
            1,
            {
                atom(repository, "at-segment", { 0, 1 }),
            }),
        inv(1,
            1,
            {
                atom(repository, "facing", { 0, 1 }),
            }),
        inv(1,
            0,
            {
                atom(repository, "is-pushing", { 0 }),
            }),
        inv(0,
            1,
            {
                atom(repository, "is-pushing", { 0 }),
            }),
        inv(1,
            1,
            {
                atom(repository, "airborne", { 0, 1 }),
                atom(repository, "at-segment", { 0, 1 }),
            }),
        inv(1,
            0,
            {
                atom(repository, "is-moving", { 0 }),
                atom(repository, "is-pushing", { 0 }),
            }),
        inv(0,
            1,
            {
                atom(repository, "is-moving", { 0 }),
                atom(repository, "is-pushing", { 0 }),
            }),
        inv(1,
            1,
            {
                atom(repository, "is-moving", { 0 }),
                atom(repository, "is-parked", { 0, 1 }),
                atom(repository, "is-pushing", { 0 }),
            }),
    };

    expect_invariant_sets_equal(actual, expected);
}

/// @brief
///
/// <Invariant {clear((0,)) [omitted_pos = None], on((-1, 0)) [omitted_pos = 0]}>,
/// <Invariant {on((0, -1)) [omitted_pos = 1], on-table((0,)) [omitted_pos = None]}>
TEST(TyrTests, TyrFormalismPlanningInvariantsSynthesisBlocks3)
{
    auto lifted_task = compute_lifted_task(absolute("blocks_3/domain.pddl"), absolute("blocks_3/test_problem.pddl"));
    auto& repository = *lifted_task.get_repository();

    auto actual = fpi::synthesize_invariants(lifted_task.get_task());

    auto expected = std::vector<fpi::Invariant> {
        inv(1,
            1,
            {
                atom(repository, "clear", { 0 }),
                atom(repository, "on", { 1, 0 }),
            }),
        inv(1,
            1,
            {
                atom(repository, "on", { 0, 1 }),
                atom(repository, "on-table", { 0 }),
            }),
    };

    expect_invariant_sets_equal(actual, expected);
}

/// @brief
/// Ground truth from Fast Downward:
///
/// <Invariant {arm-empty(()) [omitted_pos = None], holding((-1,)) [omitted_pos = 0]}>,
/// <Invariant {clear((0,)) [omitted_pos = None], holding((0,)) [omitted_pos = None], on((-1, 0)) [omitted_pos = 0]}>,
/// <Invariant {holding((0,)) [omitted_pos = None], on((0, -1)) [omitted_pos = 1], on-table((0,)) [omitted_pos = None]}>
TEST(TyrTests, TyrFormalismPlanningInvariantsSynthesisBlocks4)
{
    auto lifted_task = compute_lifted_task(absolute("blocks_4/domain.pddl"), absolute("blocks_4/test_problem.pddl"));
    auto& repository = *lifted_task.get_repository();

    auto actual = fpi::synthesize_invariants(lifted_task.get_task());

    auto expected = std::vector<fpi::Invariant> {
        inv(0,
            1,
            {
                atom(repository, "arm-empty", {}),
                atom(repository, "holding", { 0 }),
            }),
        inv(1,
            1,
            {
                atom(repository, "clear", { 0 }),
                atom(repository, "holding", { 0 }),
                atom(repository, "on", { 1, 0 }),
            }),
        inv(1,
            1,
            {
                atom(repository, "holding", { 0 }),
                atom(repository, "on", { 0, 1 }),
                atom(repository, "on-table", { 0 }),
            }),
    };

    expect_invariant_sets_equal(actual, expected);
}

/// @brief
/// Ground truth from Fast Downward:
///
/// <Invariant {at-robot((-1,)) [omitted_pos = 0]}>,
/// <Invariant {locked((0,)) [omitted_pos = None]}>,
/// <Invariant {locked((-1,)) [omitted_pos = 0]}>,
/// <Invariant {at((0, -1)) [omitted_pos = 1], holding((0,)) [omitted_pos = None]}>,
/// <Invariant {arm-empty(()) [omitted_pos = None], holding((-1,)) [omitted_pos = 0]}>,
/// <Invariant {locked((0,)) [omitted_pos = None], open((0,)) [omitted_pos = None]}>,
/// <Invariant {locked((-1,)) [omitted_pos = 0], open((-1,)) [omitted_pos = 0]}>
TEST(TyrTests, TyrFormalismPlanningInvariantsSynthesisGrid)
{
    auto lifted_task = compute_lifted_task(absolute("grid/domain.pddl"), absolute("grid/test_problem.pddl"));
    auto& repository = *lifted_task.get_repository();

    auto actual = fpi::synthesize_invariants(lifted_task.get_task());

    auto expected = std::vector<fpi::Invariant> {
        inv(0,
            1,
            {
                atom(repository, "at-robot", { 0 }),
            }),
        inv(1,
            0,
            {
                atom(repository, "locked", { 0 }),
            }),
        inv(0,
            1,
            {
                atom(repository, "locked", { 0 }),
            }),
        inv(1,
            1,
            {
                atom(repository, "at", { 0, 1 }),
                atom(repository, "holding", { 0 }),
            }),
        inv(0,
            1,
            {
                atom(repository, "arm-empty", {}),
                atom(repository, "holding", { 0 }),
            }),
        inv(1,
            0,
            {
                atom(repository, "locked", { 0 }),
                atom(repository, "open", { 0 }),
            }),
        inv(0,
            1,
            {
                atom(repository, "locked", { 0 }),
                atom(repository, "open", { 0 }),
            }),
    };

    expect_invariant_sets_equal(actual, expected);
}

/// @brief
/// Ground truth from Fast Downward:
///
/// <Invariant {at((0, -1)) [omitted_pos = 1], in((0, -1)) [omitted_pos = 1]}>
TEST(TyrTests, TyrFormalismPlanningInvariantsSynthesisLogistics)
{
    auto lifted_task = compute_lifted_task(absolute("logistics/domain.pddl"), absolute("logistics/test_problem.pddl"));
    auto& repository = *lifted_task.get_repository();

    auto actual = fpi::synthesize_invariants(lifted_task.get_task());

    auto expected = std::vector<fpi::Invariant> {
        inv(1,
            1,
            {
                atom(repository, "at", { 0, 1 }),
                atom(repository, "in", { 0, 1 }),
            }),
    };

    expect_invariant_sets_equal(actual, expected);
}
}