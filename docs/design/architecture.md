# `architecture`

*Tyr at a glance: PDDL in, Plan out, via a fixed outer pipeline whose only point of lifted/ground divergence is the `SuccessorGenerator<Kind>`.*

## Purpose

Tyr turns PDDL domain + problem files into Plans. It is organised as six subsystems with cleanly separated responsibilities: **formalism** owns representation, **planning** owns tasks/states/search, **datalog** drives lifted successor generation, **analysis** handles invariants and grounding-time work, **graphs** and **buffer** provide supporting infrastructure. The defining architectural choice is that lifted and grounded planning share the entire outer pipeline — `Task → SuccessorGenerator → Heuristic → search` — and diverge only inside the `SuccessorGenerator<Kind>`. Everything downstream is template-specialised on a `Kind` tag (`LiftedTag` or `GroundTag`); there are no virtual `SuccessorGenerator` methods or runtime-dispatched search hierarchies.

### How it works

The subsystem topology, with major types per subsystem and ownership annotated inline:

```mermaid
flowchart LR
  subgraph formalism["formalism subsystem"]
    Parser["Parser"]
    PT["PlanningTask"]
    Repo["Repository, Views, Data"]
  end
  subgraph planning["planning subsystem"]
    Task["Task<br/>(owns parsed PDDL)"]
    SG["SuccessorGenerator<br/>(owns StateRepository)"]
    Heur["Heuristic"]
    Search["find_solution<br/>(stateless)"]
  end
  subgraph datalog["datalog subsystem"]
    DL["Program, ProgramWorkspace"]
  end
  subgraph analysis["analysis subsystem"]
    AN["invariants, mutexes, stratification"]
  end
  Parser --> PT
  PT --> Task
  Task --> SG
  Task --> Heur
  SG --> Search
  Heur --> Search
  SG -. used by lifted SG .-> DL
  Task -. used during grounding .-> AN
```

Solid arrows show the in-pipeline relationships within and across `formalism` and `planning`. Dashed edges are conditional consumption: `SG`'s use of `datalog` is lifted-only (the lifted `SuccessorGenerator` evaluates a Datalog program per state; the ground one does not), and `Task`'s use of `analysis` happens only when grounding via `Task<LiftedTag>::instantiate_ground_task()`. State values themselves are never copied during search — they are registered once in the repository and referred to by `Index<State<Kind>>` everywhere else (see [Non-obvious things](#non-obvious-things)). The reference planner [exe/astar_eager.cpp](../../exe/astar_eager.cpp) shows a concrete top-to-bottom traversal through this topology.

## API

Top-level entry points by subsystem. Each subsystem doc owns its own detailed API surface; this list is the cross-subsystem "how do I run Tyr" map.

- **formalism** → [formalism](formalism.md): `formalism::planning::Parser` produces a `PlanningTask` from PDDL files.
- **planning, task layer**: `planning::LiftedTask::create()` (and `Task<LiftedTag>::instantiate_ground_task()` for grounding) — entry types in [include/tyr/planning/lifted_task.hpp](../../include/tyr/planning/lifted_task.hpp), [include/tyr/planning/ground_task.hpp](../../include/tyr/planning/ground_task.hpp).
- **planning, search** → [search](search.md): `astar_eager::find_solution<Kind>` and `gbfs_lazy::find_solution<Kind>`.
- **planning, plug points** → [successor-generation](successor-generation.md), [heuristics](heuristics.md): `SuccessorGenerator<Kind>` and `Heuristic<Kind>`.
- **datalog** → [datalog](datalog.md): the engine that drives lifted successor generation. Internal to the lifted SG; rarely called directly.
- **analysis** → [analysis](analysis.md): invariant synthesis and the lifted→ground translation, called from `Task<LiftedTag>::instantiate_ground_task()`.
- **Python**: `import pytyr` — the same pipeline is mirrored at `pytyr.formalism.planning.Parser`, `pytyr.planning.lifted.Task`, `pytyr.planning.{lifted,ground}.{astar_eager,gbfs_lazy}.find_solution`. See [python/examples/](../../python/examples/).
- **Consumers**: [exe/astar_eager.cpp](../../exe/astar_eager.cpp), [exe/gbfs_lazy.cpp](../../exe/gbfs_lazy.cpp), [python/examples/planning/](../../python/examples/planning/), and downstream user code via the Python bindings.

## Non-obvious things

- **Lifted vs ground is template specialisation, not subclassing.** There is no virtual `SuccessorGenerator` base class. `SuccessorGenerator<LiftedTag>` and `SuccessorGenerator<GroundTag>` are entirely different types with entirely different internals; the same goes for `StateRepository<Kind>`, `Task<Kind>`, `Node<Kind>`. Search algorithms are templated on `Kind`; the compiler produces independent instantiations. Cost: longer build times. Payoff: no vtable overhead in the inner loop, and each specialisation is free to choose its own data layout (datalog workspace for lifted; MatchTree for ground).
- **Grounding is a separate, expensive phase** triggered by `Task<LiftedTag>::instantiate_ground_task()` — see [include/tyr/planning/lifted_task.hpp:52](../../include/tyr/planning/lifted_task.hpp#L52). It runs invariant synthesis (in the analysis subsystem) and produces a fresh `Task<GroundTag>` with a different data layout. A reader who assumes "lifted and ground share the same Task type" will be confused; they share only the `Task<Kind>` *interface shape*, not data.
- **States are indices, not values, throughout search.** A lifted state with thousands of atoms is registered once in `StateRepository<Kind>` and addressed thereafter by `Index<State<Kind>>` (a small integer). Search nodes, open-list entries, and closed-list entries all store indices. Dereference to the actual state is lazy via `StateView<Kind>`. This is what makes lifted search tractable on large instances; without it, copying states into search nodes would dominate memory.

## Pointers

Canonical reading order — start at the top of [_roadmap.md](_roadmap.md) and follow:

1. This doc (`architecture`).
2. [formalism](formalism.md) — the representation everything builds on.
3. [search](search.md) — the entry point most users actually call.
4. [successor-generation](successor-generation.md) — where the lifted/ground split lives.
5. [heuristics](heuristics.md) — the other plug point.
6. [datalog](datalog.md) — the engine beneath lifted successor generation.
7. [analysis](analysis.md) — grounding and invariant synthesis.

Most-relevant code:

- Reference planner: [exe/astar_eager.cpp](../../exe/astar_eager.cpp) — what a complete caller looks like in C++.
- Top-level umbrella header: [include/tyr/tyr.hpp](../../include/tyr/tyr.hpp).
- `Kind` tag declarations: [include/tyr/planning/declarations.hpp](../../include/tyr/planning/declarations.hpp).
- Python examples: [python/examples/planning/astar_eager.py](../../python/examples/planning/astar_eager.py).

---

*Last reviewed: 2026-05-22 against commit `01d444d2`. Audit drift with `make docs-audit DOC=architecture` (planned).*
