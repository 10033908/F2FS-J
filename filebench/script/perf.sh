PID=$1

sudo perf stat -e task-clock,cpu-clock,context-switches,cpu-migrations,page-faults,ref-cycles,stalled-cycles-backend,\
instructions,branches,branch-misses,L1-icache-prefetches,L1-icache-prefetches-misses,L1-icache-load-misses,\
L1-icache-loads,L1-dcache-prefetches,L1-dcache-prefetch-misses,L1-dcache-loads,L1-dcache-load-misses,\
L1-dcache-stores,L1-dcache-store-misses,LLC-prefetches,LLC-prefetches-misses,LLC-loads,LLC-load-misses,LLC-stores,LLC-store-misses -p $PID sleep 5