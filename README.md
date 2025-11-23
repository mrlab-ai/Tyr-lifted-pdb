# Tyr

Tyr aims to become a weighted, annotated, and parallelizable datalog solver based on k-clique enumeration in k-partite graph (KPKC) with a focus on AI planning. Tyr currently does not aim to support object creation, and hence, does not support arithmetic expressions yet. However, it supports simple arithmetic expressions in rule bodies, which are sufficient for deciding whether a ground action in numeric planning is applicable.

# Key Features

- **Datalog Language Support**: relations over symbols, stratifiable programs
- **Language Extensions**: weighted rule expansion, rule annotation, early termination 
- **Parallelized Architecture**: rule parallelization, zero-copy serialization
- **Program Analysis**: variable domain analysis, stratification, listeners
- **Grounder Technology**: k-clique enumeration in k-partite graph (KPKC)
