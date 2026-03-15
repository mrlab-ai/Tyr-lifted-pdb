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

#ifndef TYR_FORMALISM_DATALOG_SERIALIZATION_REPOSITORY_HPP_
#define TYR_FORMALISM_DATALOG_SERIALIZATION_REPOSITORY_HPP_

// Include specialization headers first
#include "tyr/formalism/datalog/datas.hpp"
#include "tyr/formalism/datalog/indices.hpp"
//
#include "tyr/buffer/declarations.hpp"
#include "tyr/buffer/indexed_hash_set.hpp"
#include "tyr/buffer/segmented_buffer.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/common/tuple.hpp"
#include "tyr/formalism/datalog/declarations.hpp"
#include "tyr/formalism/datalog/relation_table_repository.hpp"
#include "tyr/formalism/function_view.hpp"
#include "tyr/formalism/predicate_view.hpp"

#include <cassert>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace tyr::formalism::datalog
{

class SerializationRepository
{
private:
    template<typename T>
    struct Slot
    {
        buffer::IndexedHashSet<T> container {};
        size_t parent_size = 0;
    };

    template<typename T>
    struct Entry
    {
        using slot_type = Slot<T>;

        slot_type slot;
    };

    using RepositoryStorage = std::tuple<Entry<Variable>,
                                         Entry<Object>,
                                         Entry<Binding>,
                                         Entry<Predicate<StaticTag>>,
                                         Entry<Predicate<FluentTag>>,
                                         Entry<Atom<StaticTag>>,
                                         Entry<Atom<FluentTag>>,
                                         Entry<GroundAtom<StaticTag>>,
                                         Entry<GroundAtom<FluentTag>>,
                                         Entry<Literal<StaticTag>>,
                                         Entry<Literal<FluentTag>>,
                                         Entry<GroundLiteral<StaticTag>>,
                                         Entry<GroundLiteral<FluentTag>>,
                                         Entry<Function<StaticTag>>,
                                         Entry<Function<FluentTag>>,
                                         Entry<FunctionTerm<StaticTag>>,
                                         Entry<FunctionTerm<FluentTag>>,
                                         Entry<GroundFunctionTerm<StaticTag>>,
                                         Entry<GroundFunctionTerm<FluentTag>>,
                                         Entry<GroundFunctionTermValue<StaticTag>>,
                                         Entry<GroundFunctionTermValue<FluentTag>>,
                                         Entry<UnaryOperator<OpSub, Data<FunctionExpression>>>,
                                         Entry<BinaryOperator<OpAdd, Data<FunctionExpression>>>,
                                         Entry<BinaryOperator<OpSub, Data<FunctionExpression>>>,
                                         Entry<BinaryOperator<OpMul, Data<FunctionExpression>>>,
                                         Entry<BinaryOperator<OpDiv, Data<FunctionExpression>>>,
                                         Entry<MultiOperator<OpAdd, Data<FunctionExpression>>>,
                                         Entry<MultiOperator<OpMul, Data<FunctionExpression>>>,
                                         Entry<BinaryOperator<OpEq, Data<FunctionExpression>>>,
                                         Entry<BinaryOperator<OpNe, Data<FunctionExpression>>>,
                                         Entry<BinaryOperator<OpLe, Data<FunctionExpression>>>,
                                         Entry<BinaryOperator<OpLt, Data<FunctionExpression>>>,
                                         Entry<BinaryOperator<OpGe, Data<FunctionExpression>>>,
                                         Entry<BinaryOperator<OpGt, Data<FunctionExpression>>>,
                                         Entry<UnaryOperator<OpSub, Data<GroundFunctionExpression>>>,
                                         Entry<BinaryOperator<OpAdd, Data<GroundFunctionExpression>>>,
                                         Entry<BinaryOperator<OpSub, Data<GroundFunctionExpression>>>,
                                         Entry<BinaryOperator<OpMul, Data<GroundFunctionExpression>>>,
                                         Entry<BinaryOperator<OpDiv, Data<GroundFunctionExpression>>>,
                                         Entry<MultiOperator<OpAdd, Data<GroundFunctionExpression>>>,
                                         Entry<MultiOperator<OpMul, Data<GroundFunctionExpression>>>,
                                         Entry<BinaryOperator<OpEq, Data<GroundFunctionExpression>>>,
                                         Entry<BinaryOperator<OpNe, Data<GroundFunctionExpression>>>,
                                         Entry<BinaryOperator<OpLe, Data<GroundFunctionExpression>>>,
                                         Entry<BinaryOperator<OpLt, Data<GroundFunctionExpression>>>,
                                         Entry<BinaryOperator<OpGe, Data<GroundFunctionExpression>>>,
                                         Entry<BinaryOperator<OpGt, Data<GroundFunctionExpression>>>,
                                         Entry<ConjunctiveCondition>,
                                         Entry<Rule>,
                                         Entry<GroundConjunctiveCondition>,
                                         Entry<GroundRule>,
                                         Entry<Program>>;

    const SerializationRepository* m_parent;
    RepositoryStorage m_repository;
    buffer::SegmentedBuffer m_arena;

    /**
     * Clear
     */

    template<typename T>
    void clear_entry(Entry<T>& entry) noexcept
    {
        entry.slot.container.clear();
        entry.slot.parent_size = m_parent ? m_parent->template size<T>() : size_t { 0 };
    }

    void clear_entries() noexcept
    {
        std::apply([&](auto&... entry) { (clear_entry(entry), ...); }, m_repository);
    }

public:
    SerializationRepository(const SerializationRepository* parent = nullptr) : m_parent(parent), m_repository(), m_arena() { clear_entries(); }

    /**
     * Global methods
     */

    template<typename T>
    std::optional<View<Index<T>, SerializationRepository>> find_with_hash(const Data<T>& builder, size_t h) const noexcept
    {
        const auto& entry = std::get<Entry<T>>(m_repository);
        const auto& container = entry.slot.container;
        assert(h == container.hash(builder) && "The given hash does not match container internal's hash.");

        if (auto ptr = container.find_with_hash(builder, h))
            return View<Index<T>, SerializationRepository>(ptr->index, *this);

        return m_parent ? m_parent->template find_with_hash<T>(builder, h) : std::nullopt;
    }

    template<typename T>
    std::optional<View<Index<T>, SerializationRepository>> find(const Data<T>& builder) const noexcept
    {
        const auto& entry = std::get<Entry<T>>(m_repository);
        const auto& container = entry.slot.container;
        const auto h = container.hash(builder);

        return find_with_hash<T>(builder, h);
    }

    template<typename T>
    std::pair<View<Index<T>, SerializationRepository>, bool> get_or_create(Data<T>& builder, buffer::Buffer& buf)
    {
        auto& entry = std::get<Entry<T>>(m_repository);
        auto& slot = entry.slot;
        auto& container = slot.container;
        const auto h = container.hash(builder);

        if (m_parent)
            if (auto ptr = m_parent->template find_with_hash<T>(builder, h))
                return { *ptr, false };

        // Manually assign index to continue indexing.
        builder.index.value = slot.parent_size + container.size();

        const auto [ptr, success] = container.insert_with_hash(h, builder, buf, m_arena);

        return { View<Index<T>, SerializationRepository>(ptr->index, *this), success };
    }

    /// @brief Access the element with the given index.
    template<typename T>
    const Data<T>& operator[](Index<T> index) const noexcept
    {
        assert(index != Index<T>::max() && "Unassigned index.");

        const auto& entry = std::get<Entry<T>>(m_repository);
        const auto& slot = entry.slot;
        const auto parent_size = slot.parent_size;

        // In parent range -> delegate
        if (index.value < parent_size)
        {
            assert(m_parent);
            return (*m_parent)[index];
        }

        // Local range -> shift down
        index.value -= parent_size;

        return slot.container[index];
    }

    template<typename T>
    const Data<T>& front() const
    {
        const auto& entry = std::get<Entry<T>>(m_repository);
        const auto& slot = entry.slot;

        if (slot.parent_size > 0)
        {
            assert(m_parent);
            return m_parent->template front<T>();  // recurse to root-most non-empty
        }

        return slot.container.front();
    }

    /// @brief Get the number of stored elements.
    template<typename T>
    size_t size() const noexcept
    {
        const auto& entry = std::get<Entry<T>>(m_repository);
        const auto& slot = entry.slot;

        return slot.parent_size + slot.container.size();
    }

    /// @brief Clear the repository but keep memory allocated.
    void clear() noexcept
    {
        m_arena.clear();
        clear_entries();
    }

    template<typename T>
    const SerializationRepository& get_canonical_context(Index<T> index) const noexcept
    {
        const auto& entry = std::get<Entry<T>>(m_repository);
        const auto& slot = entry.slot;

        if (index.value < slot.parent_size)
        {
            assert(m_parent && "Element not found in the repository chain.");
            return m_parent->get_canonical_context(index);
        }

        return *this;
    }

    /**
     * Local methods
     */

    template<typename T>
    std::optional<Index<T>> find_local_with_hash(const Data<T>& builder, size_t h) const noexcept
    {
        const auto& entry = std::get<Entry<T>>(m_repository);
        const auto& container = entry.slot.container;
        assert(h == container.hash(builder));

        if (auto ptr = container.find_with_hash(builder, h))
            return Index<T>(ptr->index);

        return std::nullopt;
    }

    template<typename T>
    std::optional<Index<T>> find_local(const Data<T>& builder) const noexcept
    {
        const auto& entry = std::get<Entry<T>>(m_repository);
        const auto& container = entry.slot.container;
        return find_local_with_hash<T>(builder, container.hash(builder));
    }

    template<typename T>
    std::pair<Index<T>, bool> get_or_create_local(Data<T>& builder, buffer::Buffer& buf)
    {
        auto& entry = std::get<Entry<T>>(m_repository);
        auto& slot = entry.slot;
        auto& container = slot.container;
        const auto h = container.hash(builder);

        if (auto ptr = container.find_with_hash(builder, h))
            return { Index<T>(ptr->index), false };

        builder.index.value = slot.parent_size + container.size();

        const auto [ptr, success] = container.insert_with_hash(h, builder, buf, m_arena);
        return { Index<T>(ptr->index), success };
    }

    template<typename T>
    const Data<T>& at_local(Index<T> index) const noexcept
    {
        const auto& entry = std::get<Entry<T>>(m_repository);
        const auto parent_size = entry.slot.parent_size;

        assert(index.value >= parent_size);
        return entry.slot.container[Index<T>(index.value - parent_size)];
    }

    template<typename T>
    const Data<T>& front_local() const
    {
        const auto& entry = std::get<Entry<T>>(m_repository);
        return entry.slot.container.front();
    }

    template<typename T>
    size_t local_size() const noexcept
    {
        const auto& entry = std::get<Entry<T>>(m_repository);
        return entry.slot.container.size();
    }

    template<typename T>
    size_t parent_size() const noexcept
    {
        const auto& entry = std::get<Entry<T>>(m_repository);
        return entry.slot.parent_size;
    }

    template<typename T>
    bool is_local(Index<T> index) const noexcept
    {
        assert(index != Index<T>::max() && "Unassigned index.");
        const auto& entry = std::get<Entry<T>>(m_repository);
        return uint_t(index) >= entry.slot.parent_size;
    }
};

}

#endif
