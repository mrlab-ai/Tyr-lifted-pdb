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

#ifndef TYR_FORMALISM_REPOSITORY_HPP_
#define TYR_FORMALISM_REPOSITORY_HPP_

#include "tyr/cista/declarations.hpp"
#include "tyr/cista/indexed_hash_set.hpp"
#include "tyr/formalism/atom.hpp"
#include "tyr/formalism/declarations.hpp"
#include "tyr/formalism/double.hpp"
#include "tyr/formalism/ground_atom.hpp"
#include "tyr/formalism/ground_literal.hpp"
#include "tyr/formalism/ground_rule.hpp"
#include "tyr/formalism/literal.hpp"
#include "tyr/formalism/program.hpp"
#include "tyr/formalism/relation.hpp"
#include "tyr/formalism/rule.hpp"
#include "tyr/formalism/signed.hpp"
#include "tyr/formalism/symbol.hpp"
#include "tyr/formalism/term.hpp"
#include "tyr/formalism/unsigned.hpp"
#include "tyr/formalism/variable.hpp"

#include <boost/hana.hpp>

namespace tyr::formalism
{

template<typename T>
using MappedType = boost::hana::pair<boost::hana::type<T>, cista::IndexedHashSet<T>>;

template<typename T>
using MappedTypePerIndex = boost::hana::pair<boost::hana::type<T>, cista::IndexedHashSetList<T>>;

template<typename T>
struct TypeProperties
{
    using PairType = MappedType<T>;
};

template<IsStaticOrFluentTag T>
struct TypeProperties<AtomImpl<T>>
{
    using PairType = MappedTypePerIndex<AtomImpl<T>>;
    static auto get_index(const AtomImpl<T>& self) noexcept { return self.index.relation_index; }
};

template<IsStaticOrFluentTag T>
struct TypeProperties<LiteralImpl<T>>
{
    using PairType = MappedTypePerIndex<LiteralImpl<T>>;
    static auto get_index(const LiteralImpl<T>& self) noexcept { return self.index.relation_index; }
};

template<IsStaticOrFluentTag T>
struct TypeProperties<GroundAtomImpl<T>>
{
    using PairType = MappedTypePerIndex<GroundAtomImpl<T>>;
    static auto get_index(const GroundAtomImpl<T>& self) noexcept { return self.index.relation_index; }
};

template<IsStaticOrFluentTag T>
struct TypeProperties<GroundLiteralImpl<T>>
{
    using PairType = MappedTypePerIndex<GroundLiteralImpl<T>>;
    static auto get_index(const GroundLiteralImpl<T>& self) noexcept { return self.index.relation_index; }
};

template<>
struct TypeProperties<GroundRuleImpl>
{
    using PairType = MappedTypePerIndex<GroundRuleImpl>;
    static auto get_index(const GroundRuleImpl& self) noexcept { return self.index.rule_index; }
};

template<typename T>
concept IsMappedType = std::same_as<typename TypeProperties<T>::PairType, MappedType<T>>;

template<typename T>
concept IsMappedTypePerIndex = std::same_as<typename TypeProperties<T>::PairType, MappedTypePerIndex<T>>;

class Repository
{
private:
    using HanaRepository = boost::hana::map<TypeProperties<AtomImpl<StaticTag>>::PairType,
                                            TypeProperties<AtomImpl<FluentTag>>::PairType,
                                            TypeProperties<LiteralImpl<StaticTag>>::PairType,
                                            TypeProperties<LiteralImpl<FluentTag>>::PairType,
                                            TypeProperties<GroundAtomImpl<StaticTag>>::PairType,
                                            TypeProperties<GroundAtomImpl<FluentTag>>::PairType,
                                            TypeProperties<GroundLiteralImpl<StaticTag>>::PairType,
                                            TypeProperties<GroundLiteralImpl<FluentTag>>::PairType,
                                            TypeProperties<RelationImpl<StaticTag>>::PairType,
                                            TypeProperties<RelationImpl<FluentTag>>::PairType,
                                            TypeProperties<VariableImpl>::PairType,
                                            TypeProperties<SymbolImpl>::PairType,
                                            TypeProperties<RuleImpl>::PairType,
                                            TypeProperties<GroundRuleImpl>::PairType,
                                            TypeProperties<ProgramImpl>::PairType>;

    HanaRepository m_repository;

public:
    Repository() = default;

    template<IsMappedTypePerIndex T>
    auto get_or_create(T& builder, cista::Buffer& buf)
    {
        auto& list = boost::hana::at_key(m_repository, boost::hana::type<T> {});

        const auto index = TypeProperties<T>::get_index(builder).get();

        if (index >= list.size())
            list.resize(index + 1);

        auto& repository = list[index];

        return repository.insert(builder, buf);
    }

    template<IsMappedType T>
    auto get_or_create(T& builder, cista::Buffer& buf)
    {
        auto& repository = boost::hana::at_key(m_repository, boost::hana::type<T> {});

        return repository.insert(builder, buf);
    }
};
}

#endif
