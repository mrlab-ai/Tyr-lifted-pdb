valgrind \
    --tool=callgrind \
    --callgrind-out-file=callgrind.out \
    --toggle-collect='_ZN3tyr7datalog4kpkc9DeltaKPKC24set_next_assignment_setsERKNS0_22StaticConsistencyGraphERKNS0_14AssignmentSetsE' \
    --collect-jumps=yes \
    --dump-instr=yes \
    ./build/exe/gbfs_lazy \
    -D ../../benchmarks/pddl-benchmarks/htg-domains/rovers-large-simple/goal-2/domain.pddl \
    -P ../../benchmarks/pddl-benchmarks/htg-domains/rovers-large-simple/goal-2/p-r1-w1000-o1-1-g2.pddl \
    -O plan.txt \
    -N 1


valgrind \
    --tool=callgrind \
    --collect-jumps=yes \
    --dump-instr=yes \
    ./build/exe/gbfs_lazy \
    -D ../../benchmarks/pddl-benchmarks/htg-domains/rovers-large-simple/goal-2/domain.pddl \
    -P ../../benchmarks/pddl-benchmarks/htg-domains/rovers-large-simple/goal-2/p-r1-w1000-o1-1-g2.pddl \
    -O plan.txt \
    -N 1
    