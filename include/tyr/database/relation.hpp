/*
 * Copyright (C) 2025-2026 Dominik Drexler
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

#ifndef TYR_DATABASE_RELATION_HPP_
#define TYR_DATABASE_RELATION_HPP_

#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/database/concepts.hpp"
#include "tyr/formalism/parameter_index.hpp"

#include <concepts>
#include <cstddef>
#include <optional>
#include <ranges>
#include <span>

namespace tyr::db
{

template<FixedRowTable Table>
struct Relation
{
    Table* table = nullptr;
    std::vector<formalism::ParameterIndex> columns;

    Relation() = default;

    Relation(Table& table_, std::vector<formalism::ParameterIndex> columns_) : table(&table_), columns(std::move(columns_)) {}

    std::size_t arity() const noexcept { return columns.size(); }

    bool is_well_formed() const noexcept
    {
        std::cout << "table: " << (table != nullptr) << ", table length: " << (table != nullptr ? std::to_string(table->length()) : "null")
                  << ", columns size: " << columns.size() << "\n";
        return table != nullptr && table->length() == columns.size();
    }
};

}

#endif