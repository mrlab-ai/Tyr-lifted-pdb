#include <gtest/gtest.h>
#include <boost/json.hpp>
#include <tyr/common/common.hpp>
#include <tyr/formalism/formalism.hpp>
#include <tyr/planning/planning.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace p = tyr::planning;
namespace fp = tyr::formalism::planning;
namespace json = boost::json;

namespace tyr::tests
{
namespace
{
fs::path repo_root() { return fs::path(std::string(ROOT_DIR)); }

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

struct InstanceTestCase
{
    std::string domain_name;
    fs::path domain_path;
    fs::path instance_path;
    std::vector<float_t> expected_sys1;
};

std::string format_values(const std::vector<float_t>& values)
{
    auto out = std::ostringstream {};
    out << '[';
    for (size_t index = 0; index < values.size(); ++index)
    {
        if (index != 0)
            out << ", ";
        out << values[index];
    }
    out << ']';
    return out.str();
}

std::vector<float_t> sorted_values(const std::vector<float_t>& values)
{
    auto sorted = values;
    std::sort(sorted.begin(), sorted.end());
    return sorted;
}

std::string make_test_name(const InstanceTestCase& test_case)
{
    auto name = test_case.domain_name + "__" + test_case.instance_path.filename().string();
    for (auto& character : name)
        if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_')
            character = '_';
    return name;
}

std::vector<InstanceTestCase> load_sys1_test_cases()
{
    const auto fixture_path = fs::path(std::string(HEURISTICS_TEST_FIXTURES_DIR)) / "projection_abstraction_estimates.json";
    auto ifs = std::ifstream(fixture_path);
    auto ss = std::ostringstream {};
    ss << ifs.rdbuf();
    const auto root = json::parse(ss.str()).as_object();

    auto cases = std::vector<InstanceTestCase> {};
    for (const auto& [domain_key, domain_val] : root)
    {
        if (domain_key == "fd_git_hash")
            continue;
        const auto& domain_obj = domain_val.as_object();
        const auto domain_path = repo_root() / std::string(domain_obj.at("domain").as_string());
        for (const auto& inst : domain_obj.at("instances").as_array())
        {
            const auto& inst_obj = inst.as_object();
            const auto& sys1_val = inst_obj.at("initial_projection_heuristic_estimates_sys_1");
            if (sys1_val.is_null())
                continue;
            const auto instance_path = repo_root() / std::string(inst_obj.at("path").as_string());
            auto expected = std::vector<float_t> {};
            for (const auto& v : sys1_val.as_array())
                expected.push_back(static_cast<float_t>(v.to_number<double>()));
            cases.push_back({ std::string(domain_key), domain_path, instance_path, std::move(expected) });
        }
    }
    return cases;
}
}

class ProjectionAbstractionSys1Test : public ::testing::TestWithParam<InstanceTestCase> {};

TEST_P(ProjectionAbstractionSys1Test, InitialStateEstimates)
{
    const auto& tc = GetParam();
    SCOPED_TRACE("domain=" + tc.domain_path.string() + ", instance=" + tc.instance_path.string());
    ASSERT_FALSE(tc.expected_sys1.empty()) << "fixture contains an empty sys_1 estimate vector";

    auto lifted_task = compute_lifted_task(tc.domain_path, tc.instance_path);
    auto patterns = p::GoalPatternGenerator<p::LiftedTag>(lifted_task).generate();
    auto projections = p::ProjectionGenerator<p::LiftedTag>(lifted_task, patterns).generate();
    auto heuristics = create_projection_abstraction_heuristics(projections);
    auto state_repository = p::StateRepository<p::LiftedTag>::create(lifted_task, ExecutionContext::create(1));
    auto initial_state = state_repository->get_initial_state();
    auto values = evaluate_heuristics(heuristics, initial_state);

    EXPECT_EQ(sorted_values(values), sorted_values(tc.expected_sys1))
        << "expected=" << format_values(tc.expected_sys1)
        << ", actual=" << format_values(values);
}

INSTANTIATE_TEST_SUITE_P(
    TyrPlanningHeuristicsProjectionAbstractionSys1,
    ProjectionAbstractionSys1Test,
    ::testing::ValuesIn(load_sys1_test_cases()),
    [](const ::testing::TestParamInfo<InstanceTestCase>& info)
    {
        return make_test_name(info.param);
    });
}