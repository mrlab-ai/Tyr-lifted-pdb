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

#ifndef TYR_FORMALISM_PLANNING_PLANNING_DOMAIN_HPP_
#define TYR_FORMALISM_PLANNING_PLANNING_DOMAIN_HPP_

#include "tyr/formalism/planning/declarations.hpp"
#include "tyr/formalism/planning/domain_view.hpp"
#include "tyr/formalism/planning/repository.hpp"

namespace tyr::formalism::planning
{

class PlanningDomain
{
public:
    PlanningDomain(View<Index<Domain>, Repository> domain, std::shared_ptr<Repository> repository) : m_repository(std::move(repository)), m_domain(domain)
    {
        if (&m_domain.get_context() != m_repository.get())
            throw std::invalid_argument("Domain context does not match the given Repository.");
    }

    auto get_domain() const noexcept { return m_domain; }
    const auto& get_repository() const noexcept { return m_repository; }

private:
    std::shared_ptr<Repository> m_repository;
    View<Index<Domain>, Repository> m_domain;
};

}

#endif