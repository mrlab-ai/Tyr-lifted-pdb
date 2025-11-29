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

#ifndef TYR_COMMON_OPTIONAL_HPP_
#define TYR_COMMON_OPTIONAL_HPP_

#include "tyr/common/declarations.hpp"
#include "tyr/common/types.hpp"

#include <cista/containers/optional.h>

namespace tyr
{

template<typename Context, typename T>
class View<::cista::optional<T>, Context>
{
public:
    using Optional = ::cista::offset::variant<T...>;

    View(const Optional& handle, const Context& context) : m_context(&context), m_handle(&handle) {}

    const auto& get_data() const noexcept { return *m_handle; }
    const auto& get_context() const noexcept { return *m_context; }
    const auto& get_handle() const noexcept { return m_handle; }

private:
    const Context* m_context;
    const Optional* m_handle;
};

}
#endif
