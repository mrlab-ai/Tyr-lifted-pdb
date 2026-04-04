#ifndef TYR_FORMALISM_UNIFICATION_MATCH_RESULT_HPP_
#define TYR_FORMALISM_UNIFICATION_MATCH_RESULT_HPP_

#include <optional>

namespace tyr::formalism::unification
{

enum class MatchFailure
{
    none,
    structure_mismatch,
};

template<typename State>
struct MatchResult
{
    std::optional<State> state;
    MatchFailure failure = MatchFailure::none;

    [[nodiscard]] bool has_value() const noexcept { return state.has_value(); }

    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] const State& value() const { return *state; }

    [[nodiscard]] State& value() { return *state; }
};

}

#endif