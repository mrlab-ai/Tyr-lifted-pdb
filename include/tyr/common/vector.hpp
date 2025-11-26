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

#include <cista/containers/vector.h>
#include <cstddef>
#include <iterator>

namespace tyr
{

template<typename T, template<typename> typename Ptr, bool IndexPointers, typename TemplateSizeType, class Allocator, typename Context>
class Proxy<::cista::basic_vector<T, Ptr, IndexPointers, TemplateSizeType, Allocator>, Context>
{
public:
    using Container = ::cista::basic_vector<T, Ptr, IndexPointers, TemplateSizeType, Allocator>;

    Proxy(const Container& data, const Context& context) : m_context(&context), m_data(&data) {}

    size_t size() const noexcept { return data().size(); }
    bool empty() const noexcept { return data().empty(); }

    auto operator[](size_t i) const
    {
        if constexpr (IsProxyable<T, Context>)
        {
            return Proxy<T, Context>(data()[i], context());
        }
        else
        {
            return data()[i];
        }
    }

    decltype(auto) front() const
    {
        if constexpr (IsProxyable<T, Context>)
        {
            return Proxy<T, Context>(data().front(), context());
        }
        else
        {
            return data().front();
        }
    }

    struct const_iterator
    {
        const Context* ctx;
        const T* ptr;

        using difference_type = std::ptrdiff_t;
        using value_type = std::conditional_t<IsProxyable<T, Context>, ::tyr::Proxy<T, Context>, T>;
        using iterator_category = std::random_access_iterator_tag;
        using iterator_concept = std::random_access_iterator_tag;

        const_iterator() : ctx(nullptr), ptr(nullptr) {}
        const_iterator(const T* ptr, const Context& ctx) : ctx(&ctx), ptr(ptr) {}

        auto operator*() const
        {
            if constexpr (IsProxyable<T, Context>)
            {
                return Proxy<T, Context>(*ptr, *ctx);
            }
            else
            {
                return *ptr;
            }
        }

        // ++
        const_iterator& operator++()
        {
            ++ptr;
            return *this;
        }
        const_iterator operator++(int)
        {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        // --
        const_iterator& operator--()
        {
            --ptr;
            return *this;
        }
        const_iterator operator--(int)
        {
            auto tmp = *this;
            --(*this);
            return tmp;
        }

        // += / -=
        const_iterator& operator+=(difference_type n)
        {
            ptr += n;
            return *this;
        }
        const_iterator& operator-=(difference_type n)
        {
            ptr -= n;
            return *this;
        }

        // + / -
        friend const_iterator operator+(const_iterator it, difference_type n)
        {
            it += n;
            return it;
        }

        friend const_iterator operator+(difference_type n, const_iterator it)
        {
            it += n;
            return it;
        }

        friend const_iterator operator-(const_iterator it, difference_type n)
        {
            it -= n;
            return it;
        }

        // iterator - iterator
        friend difference_type operator-(const_iterator lhs, const_iterator rhs) { return lhs.ptr - rhs.ptr; }

        // []
        auto operator[](difference_type n) const
        {
            if constexpr (IsProxyable<T, Context>)
                return Proxy<T, Context>(*(ptr + n), *ctx);
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

    const_iterator begin() const { return const_iterator { data().data(), context() }; }

    const_iterator end() const { return const_iterator { data().data() + data().size(), context() }; }

    const Context& context() const { return *m_context; }
    const Container& data() const { return *m_data; }

private:
    const Context* m_context;
    const Container* m_data;
};

template<typename T, typename V>
class FlatVector
{
    static_assert(IsFlatType<T>, "FlatVector requires a flat type T");

public:
    FlatVector() noexcept = default;

    void reset(const V& default_value) { std::fill(m_data.begin(), m_data.end(), default_value); }

    void resize_to_fit(Index<T> index, const V& default_value)
    {
        const auto value = index.get_value();

        if (value >= m_data.size())
            m_data.resize(value + 1, default_value);
    }

    V& operator[](Index<T> index)
    {
        assert(index.get_value() < m_data.size());

        return m_data[index.get_value()];
    }

    const V& operator[](Index<T> index) const
    {
        assert(index.get_value() < m_data.size());

        return m_data[index.get_value()];
    }

private:
    std::vector<V> m_data;
};

template<typename T, typename V>
class GroupVector
{
    static_assert(IsGroupType<T>, "GroupVector requires a group type T");

public:
    GroupVector() noexcept = default;

    void reset(const V& default_value)
    {
        for (auto& vec : m_data)
            std::fill(vec.begin(), vec.end(), default_value);
    }

    void resize_to_fit(Index<T> index, const V& default_value)
    {
        const auto value = index.get_value();
        const auto group = index.get_group().get_value();

        if (group >= m_data.size())
            m_data.resize(group + 1);

        if (value >= m_data[group].size())
            m_data[group].resize(value + 1, default_value);
    }

    V& operator[](Index<T> index)
    {
        assert(index.get_group().get_value() < m_data.size() && index.get_value() < m_data[index.get_group().get_value()].size());

        return m_data[index.get_group().get_value()][index.get_value()];
    }

    const V& operator[](Index<T> index) const
    {
        assert(index.get_group().get_value() < m_data.size() && index.get_value() < m_data[index.get_group().get_value()].size());

        return m_data[index.get_group().get_value()][index.get_value()];
    }

private:
    std::vector<std::vector<V>> m_data;
};
}

#endif