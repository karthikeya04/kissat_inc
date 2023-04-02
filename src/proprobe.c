#define INLINE_ASSIGN

#include "inline.h"
#include "proprobe.h"

// Keep this 'inlined' file separate:

#include "assign.c"

#define PROPAGATE_LITERAL probing_propagate_literal
#define PROPAGATION_TYPE "probing"
#define PROBING_PROPAGATION

#include "proplit.h"

static clause *
probing_propagate(kissat *solver, clause *ignore)
{
  clause *res = 0;
  while (!res && solver->propagated < SIZE_STACK(solver->trail))
  {
    if (solver->propagated + 1 < SIZE_STACK(solver->trail))
    {
      // watch *q = BEGIN_WATCHES(WATCHES(NOT(solver->trail.begin[solver->propagated + 1])));
      __builtin_prefetch(BEGIN_WATCHES(WATCHES(NOT(solver->trail.begin[solver->propagated + 1]))), 0, 0); // prefetching q for next function call
      //__builtin_prefetch(BEGIN_STACK(solver->arena) + (q + 1)->raw,0,0);
    }
#ifdef CYCLES_PER_ITER
    solver->prefetch = solver->cycles_per_iter > THRESHOLD;
#ifdef USE_COUNTER 
      solver->cnt[solver->prefetch]++;
#endif
#endif
    const unsigned lit = PEEK_STACK(solver->trail, solver->propagated);
    res = probing_propagate_literal(solver, ignore, lit);
    solver->propagated++;
  }
  return res;
}

clause *
kissat_probing_propagate(kissat *solver, clause *ignore)
{
  assert(solver->probing);
  assert(solver->watching);
  assert(!solver->inconsistent);

  START(propagate);

  solver->ticks = 0;
  const unsigned propagated = solver->propagated;
  clause *conflict = probing_propagate(solver, ignore);
  kissat_update_probing_propagation_statistics(solver, propagated);

  STOP(propagate);

  return conflict;
}
