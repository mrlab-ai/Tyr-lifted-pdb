
# Tyr

  

Tyr aims to become a weighted, annotated, and parallelizable datalog solver with a grounder based on k-clique enumeration in k-partite graphs (KPKC) with a focus on AI planning. It is currently unknown whether the KPKC grounder can efficiently support object creation, and hence, Tyr does not support arithmetic expressions in rule heads. However, it supports simple arithmetic expressions in rule bodies, which are sufficient for deciding whether a ground action in numeric planning is applicable. Tyr also provides a PDDL interface, which allows the grounding of an *overapproximation* of the applicable ground actions and axioms in a task, or, in a state, *precisely* the ones applicable in that state.

  

# Key Features

  

-  **Datalog Language Support**: relations over symbols, stratifiable programs

-  **Language Extensions**: weighted rule expansion, rule annotation, early termination

-  **Parallelized Architecture**: lock-free rule parallelization, zero-copy data serialization

-  **Program Analysis**: variable domain analysis, stratification, listeners

-  **Grounder Technology**: k-clique enumeration in k-partite graph (KPKC)

  

# Getting Started

  

## 1. Installation

  

TODO

  

## 2. Integration

  

TODO

  

## 3. Datalog Interface

The high level C++ datalog interface aims to be as follows

```cpp
#include <tyr/tyr.hpp>

auto parser = tyr::formalism::Parser("program.dl");

auto program = parser.get_program();

auto goal_condition = parser.parse_goal_condition("goal_condition.dl")

const auto annotated = bool{true};
const auto weighted = bool{true};

auto solver = tyr::solver::Solver(program);

auto solution = solver.solve(goal_condition, annotated, weighted);

```
  

## 4 PDDL Interface

The high level C++ planning interface aims to be as follows.

```cpp
#include <tyr/tyr.hpp>

auto parser = tyr::formalism::planning::Parser("domain.pddl");
auto task = parse.parse_task("problem.pddl");

// Get the initial node (state + metric value)
auto initial_node = task.get_initial_node();

// Get the applicable actions
auto applicable_actions = initial_node.get_applicable_actions();

// Get the successor nodes (states + metric values)
auto successor_nodes = initial_node.get_successor_nodes();
```
