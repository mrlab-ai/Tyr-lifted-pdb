#ifndef TYR_FORMALISM_UNIFICATION_STRUCTURE_TRAITS_IMPL_HPP_
#define TYR_FORMALISM_UNIFICATION_STRUCTURE_TRAITS_IMPL_HPP_

#include "tyr/formalism/unification/structure_traits.hpp"

#include <vector>

namespace tyr::formalism::unification
{

template<typename T>
struct structure_traits<std::vector<T>>
{
    template<typename F>
    static bool zip_terms(const std::vector<T>& lhs, const std::vector<T>& rhs, F&& f)
    {
        if (lhs.size() != rhs.size())
            return false;

        for (size_t i = 0; i < lhs.size(); ++i)
        {
            if (!tyr::formalism::unification::zip_terms(lhs[i], rhs[i], f))
                return false;
        }

        return true;
    }

    template<typename F>
    static std::vector<T> transform_terms(const std::vector<T>& values, F&& f)
    {
        std::vector<T> result;
        result.reserve(values.size());

        for (const auto& value : values)
            result.push_back(tyr::formalism::unification::transform_terms(value, f));

        return result;
    }
};

template<typename T, typename F>
bool zip_terms(const T& lhs, const T& rhs, F&& f)
{
    return structure_traits<T>::zip_terms(lhs, rhs, std::forward<F>(f));
}

template<typename T, typename F>
T transform_terms(const T& value, F&& f)
{
    return structure_traits<T>::transform_terms(value, std::forward<F>(f));
}

}  // namespace tyr::formalism::unification

#endif