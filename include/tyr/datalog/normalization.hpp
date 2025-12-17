

namespace tyr::datalog
{

template<FactKind T>
void collect_parameters(View<Data<Term>, OverlayRepository<Repository>> term, UnorderedSet<ParameterIndex>& ref_parameters)
{
    visit(
        [&](auto&& arg)
        {
            using Alternative = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<Alternative, ParameterIndex>)
                ref_parameters.insert(arg);
            else if constexpr (std::is_same_v<Alternative, View<Index<Object>, OverlayRepository<Repository>>>) {}
            else
                static_assert(dependent_false<Alternative>::value, "Missing case");
        },
        term.get_variant());
}

template<FactKind T>
void collect_parameters(View<Index<Atom<T>>, OverlayRepository<Repository>> atom, UnorderedSet<ParameterIndex>& ref_parameters)
{
    for (const auto term : atom.get_terms())
        collect_parameters(term, ref_parameters);
}

template<FactKind T>
void collect_parameters(View<Index<Literal<T>>, OverlayRepository<Repository>> literal, UnorderedSet<ParameterIndex>& ref_parameters)
{
    collect_parameters(literal.get_atom(), ref_parameters);
}

inline std::vector<ParameterIndex> collect_parameters(View<Index<FDRConjunctiveCondition>, OverlayRepository<Repository>> cond)
{
    auto parameters = UnorderedSet<ParameterIndex> {};

    for (const auto literal : cond.template get_literals<StaticTag>())
        if (literal.get_polarity())
            collect_parameters(literal, parameters);

    for (const auto literal : cond.template get_literals<FluentTag>())
        if (literal.get_polarity())
            collect_parameters(literal, parameters);

    for (const auto literal : cond.template get_literals<DerivedTag>())
        if (literal.get_polarity())
            collect_parameters(literal, parameters);

    auto parameters_vec = std::vector<ParameterIndex>(parameters.begin(), parameters.end());
    std::sort(parameters_vec.begin(), parameters_vec.end());

    return parameters_vec;
}

}