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
 *<
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef TYR_COMMON_VECTOR_HPP_
#define TYR_COMMON_VECTOR_HPP_

#include "tyr/common/declarations.hpp"
#include "tyr/common/types.hpp"
#include "tyr/common/types_utils.hpp"

#include <cista/containers/vector.h>
#include <cstddef>
#include <iterator>

namespace tyr
{

template<typename T, template<typename> typename Ptr, bool IndexPointers, typename TemplateSizeType, class Allocator, typename C>
class View<::cista::basic_vector<T, Ptr, IndexPointers, TemplateSizeType, Allocator>, C>
{
public:
    using Container = ::cista::basic_vector<T, Ptr, IndexPointers, TemplateSizeType, Allocator>;

    View(const Container& handle, const C& context) noexcept : m_context(&context), m_handle(&handle) {}

    size_t size() const noexcept { return get_data().size(); }
    bool empty() const noexcept { return get_data().empty(); }

    decltype(auto) operator[](size_t i) const noexcept
    {
        if constexpr (ViewConcept<T, C>)
            return make_view(get_data()[i], get_context());
        else
            return get_data()[i];
    }

    decltype(auto) front() const noexcept
    {
        if constexpr (ViewConcept<T, C>)
            return make_view(get_data().front(), get_context());
        else
            return get_data().front();
    }

    struct const_iterator
    {
        const C* ctx;
        const T* ptr;

        using difference_type = std::ptrdiff_t;
        using value_type = std::conditional_t<ViewConcept<T, C>, ::tyr::View<T, C>, T>;
        using iterator_category = std::random_access_iterator_tag;
        using iterator_concept = std::random_access_iterator_tag;

        const_iterator() noexcept : ctx(nullptr), ptr(nullptr) {}
        const_iterator(const T* ptr, const C& ctx) noexcept : ctx(&ctx), ptr(ptr) {}

        decltype(auto) operator*() const noexcept
        {
            if constexpr (ViewConcept<T, C>)
                return make_view(*ptr, *ctx);
            else
                return *ptr;
        }

        // ++
        const_iterator& operator++() noexcept
        {
            ++ptr;
            return *this;
        }
        const_iterator operator++(int) noexcept
        {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        // --
        const_iterator& operator--() noexcept
        {
            --ptr;
            return *this;
        }
        const_iterator operator--(int) noexcept
        {
            auto tmp = *this;
            --(*this);
            return tmp;
        }

        // += / -=
        const_iterator& operator+=(difference_type n) noexcept
        {
            ptr += n;
            return *this;
        }
        const_iterator& operator-=(difference_type n) noexcept
        {
            ptr -= n;
            return *this;
        }

        // + / -
        friend const_iterator operator+(const_iterator it, difference_type n) noexcept
        {
            it += n;
            return it;
        }

        friend const_iterator operator+(difference_type n, const_iterator it) noexcept
        {
            it += n;
            return it;
        }

        friend const_iterator operator-(const_iterator it, difference_type n) noexcept
        {
            it -= n;
            return it;
        }

        // iterator - iterator
        friend difference_type operator-(const_iterator lhs, const_iterator rhs) noexcept { return lhs.ptr - rhs.ptr; }

        // []
        auto operator[](difference_type n) const noexcept
        {
            if constexpr (ViewConcept<T, C>)
                return make_view(*(ptr + n), *ctx);
            else
                return *(ptr + n);
        }

        // comparisons
        friend bool operator==(const const_iterator& lhs, const const_iterator& rhs) noexcept { return lhs.ptr == rhs.ptr; }

        friend bool operator!=(const const_iterator& lhs, const const_iterator& rhs) noexcept { return !(lhs == rhs); }

        friend bool operator<(const const_iterator& lhs, const const_iterator& rhs) noexcept { return lhs.ptr < rhs.ptr; }

        friend bool operator>(const const_iterator& lhs, const const_iterator& rhs) noexcept { return rhs < lhs; }

        friend bool operator<=(const const_iterator& lhs, const const_iterator& rhs) noexcept { return !(rhs < lhs); }

        friend bool operator>=(const const_iterator& lhs, const const_iterator& rhs) noexcept { return !(lhs < rhs); }
    };

    const_iterator begin() const noexcept { return const_iterator { get_data().data(), get_context() }; }

    const_iterator end() const noexcept { return const_iterator { get_data().data() + get_data().size(), get_context() }; }

    const auto& get_data() const noexcept { return *m_handle; }
    const auto& get_context() const noexcept { return *m_context; }
    const auto& get_handle() const noexcept { return m_handle; }

private:
    const C* m_context;
    const Container* m_handle;
};

template<typename T>
const T& get(size_t pos, const std::vector<T>& vec, const T& default_value) noexcept
{
    if (pos >= vec.size())
        return default_value;
    return vec[pos];
}

template<class T>
void set(size_t pos, const T& value, std::vector<T>& vec, const T& default_value)
{
    if (pos >= vec.size())
        vec.resize(pos + 1, default_value);
    vec[pos] = value;
}

/**
 * MultiDimensionalSpan (Row-major)
 */

template<typename T, size_t Rank>
class MultiDimensionalSpan
{
private:
    template<size_t N>
    static constexpr std::array<size_t, N - 1> tail(const std::array<size_t, N>& a) noexcept
    {
        std::array<size_t, N - 1> out {};
        for (size_t i = 1; i < N; ++i)
            out[i - 1] = a[i];
        return out;
    }

    // internal ctor used by slicing
    MultiDimensionalSpan(T* data, const std::array<size_t, Rank>& shapes, const std::array<size_t, Rank>& strides) noexcept :
        m_data(data),
        m_shapes(shapes),
        m_strides(strides)
    {
    }

public:
    MultiDimensionalSpan() : m_data(nullptr), m_shapes(), m_strides() {}
    MultiDimensionalSpan(T* data, const std::array<size_t, Rank>& shapes) : m_data(data), m_shapes(shapes), m_strides()
    {
        // layout_right / row-major strides
        m_strides[Rank - 1] = 1;
        for (size_t i = Rank - 1; i-- > 0;)
            m_strides[i] = m_shapes[i + 1] * m_strides[i + 1];
    }

    // rank-dropping slice along dim 0
    auto operator[](size_t pos) noexcept
        requires(Rank > 1)
    {
        assert(pos < m_shapes[0]);
        return MultiDimensionalSpan<T, Rank - 1>(m_data + pos * m_strides[0], tail(m_shapes), tail(m_strides));
    }

    auto operator[](size_t pos) const noexcept
        requires(Rank > 1)
    {
        assert(pos < m_shapes[0]);
        return MultiDimensionalSpan<T, Rank - 1>(m_data + pos * m_strides[0], tail(m_shapes), tail(m_strides));
    }

    // base case rank-1 indexing

    T& operator[](size_t pos) noexcept
        requires(Rank == 1)
    {
        assert(pos < m_shapes[0]);
        return m_data[pos];
    }

    const T& operator[](size_t pos) const noexcept
        requires(Rank == 1)
    {
        assert(pos < m_shapes[0]);
        return m_data[pos];
    }

    template<typename... Idx>
        requires(sizeof...(Idx) == Rank)
    T& operator()(Idx... idx) noexcept
    {
        size_t off = 0;
        size_t d = 0;
        ((off += size_t(idx) * m_strides[d++]), ...);
        return m_data[off];
    }

    template<typename... Idx>
        requires(sizeof...(Idx) == Rank)
    const T& operator()(Idx... idx) const noexcept
    {
        size_t off = 0;
        size_t d = 0;
        ((off += size_t(idx) * m_strides[d++]), ...);
        return m_data[off];
    }

    const std::array<size_t, Rank>& extent() const noexcept { return m_shapes; }
    const std::array<size_t, Rank>& stride() const noexcept { return m_strides; }

    T* data() noexcept { return m_data; }
    const T* data() const noexcept { return m_data; }

private:
    T* m_data;
    std::array<size_t, Rank> m_shapes;
    std::array<size_t, Rank> m_strides;
};

}

#endif