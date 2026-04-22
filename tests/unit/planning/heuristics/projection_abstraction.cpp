#include <gtest/gtest.h>
#include <tyr/common/common.hpp>
#include <tyr/formalism/formalism.hpp>
#include <tyr/planning/planning.hpp>

namespace p = tyr::planning;
namespace fp = tyr::formalism::planning;

namespace tyr::tests
{
namespace
{
fs::path absolute(const std::string& subdir) { return fs::path(std::string(DATA_DIR)) / subdir; }

p::LiftedTaskPtr compute_lifted_task(const fs::path& domain_filepath, const fs::path& problem_filepath)
{
    return p::LiftedTask::create(fp::Parser(domain_filepath).parse_task(problem_filepath));
}

std::vector<p::HeuristicPtr<p::LiftedTag>> create_projection_abstraction_heuristics(const std::vector<p::ProjectionAbstraction<p::LiftedTag>>& projections)
{
    auto heuristics = std::vector<p::HeuristicPtr<p::LiftedTag>> {};
    for (const auto& projection : projections)
        heuristics.push_back(p::ProjectionAbstractionHeuristic<p::LiftedTag>::create(projection));

    return heuristics;
}

std::vector<float_t> evaluate_heuristics(const std::vector<p::HeuristicPtr<p::LiftedTag>>& heuristics, const p::StateView<p::LiftedTag>& state)
{
    auto result = std::vector<float_t> {};
    result.reserve(heuristics.size());

    for (const auto& heuristic : heuristics)
        result.push_back(heuristic->evaluate(state));

    return result;
}

auto estimates(std::initializer_list<float_t> v) { return std::vector<float_t>(v); }
}

TEST(TyrTests, TyrPlanningHeuristicsProjectionAbstractionGripper)
{
    auto lifted_task = compute_lifted_task(absolute("gripper/domain.pddl"), absolute("gripper/p-2-0.pddl"));

    auto patterns = p::GoalPatternGenerator<p::LiftedTag>(lifted_task).generate();

    auto projections = p::ProjectionGenerator<p::LiftedTag>(lifted_task, patterns).generate();

    auto heuristics = create_projection_abstraction_heuristics(projections);

    auto state_repository = p::StateRepository<p::LiftedTag>::create(lifted_task, ExecutionContext::create(1));

    auto initial_state = state_repository->get_initial_state();

    auto values = evaluate_heuristics(heuristics, initial_state);

    EXPECT_EQ(values.size(), 2);
    EXPECT_EQ(values, estimates({ 1., 1. }));
}
}