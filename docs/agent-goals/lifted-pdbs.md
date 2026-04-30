# Feature Goal: Lifted Pattern Databases

## Objective
Implement the first lifted pattern database heuristic for classical planning.

## Literature References
- [Planning with Pattern Databases](https://cdn.aaai.org/ocs/7280/7280-37829-1-PB.pdf)
  - Relevance: Foundational paper on pattern databases for grounded classical planning
- [Domain-Independent Construction of Pattern Database Heuristics for Cost-Optimal Planning](https://fileadmin.cs.lth.se/ai/Proceedings/aaai07/10/AAAI07-160.pdf)
  - Relevance: The canonical heuristic over pattern databases for grounded classical planning

## Scope
- In scope:
  - Classical planning fragment
  - Efficient implementation of projection abstractions
  - Canonical heuristic 
- Out of scope:
  - Numeric planning fragment
  - More sophisticated heuristics such as cost partitioning over pattern databases

## Success Criteria
- Projection abstractions are fast to compute.

## Constraints
- Do not generate abstract self loops in projection abstractions because the amount is huge and not relevant.
- Use pairs of source and target state to enumerate transitions between them

## Notes
- Compile with 8 cores only.

## Tasks
The agent should work on the highest unchecked task in this list. The agent must not mark tasks as complete.

- [ ] Make projection abstraction generation fast
  - Interesting files/directories:
    - `docs/agent-goals/lifted-pdbs.md`
    - `include/tyr/planning/abstractions/`
    - `src/planning/abstractions/`
    - `include/tyr/formalism/unification/`
    - `include/tyr/formalism/planning`
    - `include/tyr/formalism/planning/mutable/`
    - `include/tyr/planning/lifted_task/abstractions/`
    - `src/planning/lifted_task/abstractions/`
  - Likely modification areas:
    - `src/planning/lifted_task/abstractions/projection_generator.cpp`
  - Validation:
    - Configure and build profiling:
    - `cmake -S . -B build -DBUILD_PROFILING=ON`
    - `cmake --build build --target projection_generator -j8`
    - Run baseline and candidate profiling with `profiling/runner.py`.
    - Compare summaries with `profiling/compare.py`.
    - For quick smoke checks, use `--benchmark-min-time 0.01s --benchmark-timeout 1`.
  - Notes:
    - Identifying a better join order over the predicates can be useful to prune the search tree early.
    - Having a lookup table from variables to predicates for which there exists an atom in the action schema can also be useful to prune the search tree.
