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

#ifndef TYR_PLANNING_VIEWS_HPP_
#define TYR_PLANNING_VIEWS_HPP_

#include "tyr/formalism/planning/fdr_value.hpp"

#include <boost/dynamic_bitset.hpp>
#include <vector>

namespace tyr::planning
{

template<typename T>
class FDRFactListView
{
};

class FDRFactListView<boost::dynamic_bitset<>>
{
private:
    const boost::dynamic_bitset<>* data;

public:
    FDRFactListView(const boost::dynamic_bitset<>& data) : data(&data) {}

    size_t size() const noexcept { return data->size(); }
};

class FDRFactListView<std::vector<formalism::FDRValue>>
{
private:
    const std::vector<formalism::FDRValue>* data;

public:
    FDRFactListView(const std::vector<formalism::FDRValue>& data) : data(&data) {}

    size_t size() const noexcept { return data->size(); }
};

}

#endif