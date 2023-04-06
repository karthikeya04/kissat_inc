#include "inline.h"

#ifndef HYPER_PROPAGATION

static inline void
kissat_watch_large_delayed(kissat *solver, unsigneds *delayed)
{
	const unsigned *end_delayed = END_STACK(*delayed);
	const unsigned *d = BEGIN_STACK(*delayed);
	watches *all_watches = solver->watches;
	while (d != end_delayed)
	{
		const unsigned lit = *d++;
		assert(d != end_delayed);
		const watch watch = {.raw = *d++};
		assert(!watch.type.binary);
		watches *lit_watches = all_watches + lit;
		assert(d != end_delayed);
		const reference ref = *d++;
		const unsigned blocking = watch.blocking.lit;
		LOGREF(ref, "watching %s blocking %s in", LOGLIT(lit),
			   LOGLIT(blocking));
		kissat_push_blocking_watch(solver, lit_watches, blocking, ref);
	}
	CLEAR_STACK(*delayed);
}

#endif

#if defined(HYPER_PROPAGATION) || defined(PROBING_PROPAGATION)

static inline void
kissat_update_probing_propagation_statistics(kissat *solver,
											 unsigned propagated)
{
	assert(propagated <= solver->propagated);
	propagated = solver->propagated - propagated;

	LOG(PROPAGATION_TYPE " propagation took %" PRIu64 " propagations",
		propagated);

	LOG(PROPAGATION_TYPE " propagation took %" PRIu64 " ticks", solver->ticks);

	ADD(propagations, propagated);
	ADD(probing_ticks, solver->ticks);

	ADD(probing_propagations, propagated);
	ADD(ticks, solver->ticks);
}

#endif

static inline void
kissat_delay_watching_large(kissat *solver, unsigneds *delayed,
							unsigned lit, unsigned other, reference ref)
{
	const watch watch = kissat_blocking_watch(other);
	PUSH_STACK(*delayed, lit);
	PUSH_STACK(*delayed, watch.raw);
	PUSH_STACK(*delayed, ref);
}

enum type_rdtsc
{
	rdtsc_fence,
	nofence
};
static inline uint64_t
rdtsc(enum type_rdtsc type)
{
	unsigned a = 0, d = 0;
	if (type == rdtsc_fence)
	{
		asm volatile("mfence");
		asm volatile("rdtsc"
					 : "=a"(a), "=d"(d));
		asm volatile("mfence");
	}
	else
	{
		asm volatile("rdtsc"
					 : "=a"(a), "=d"(d));
	}
	return ((uint64_t)a) | (((uint64_t)d) << 32);
}

#ifdef FOUR_PHASE_PROPAGATION
/* Fields in this struct should be in such a way that there are no additional loads when you are traversing nb-clauses*/
typedef struct nbentry
{
	watch tail;
	watch *q;

} nbentry;
typedef struct nbdata
{
	size_t sz;
	size_t idx;
	nbentry *nblist;

} nbdata;

static inline void
add_to_nblist(nbdata *nbdata, nbentry entry)
{
	size_t idx = nbdata->idx;
	size_t sz = nbdata->sz;
	if (idx == sz)
	{
		// realloc
		nbdata->sz = 2 * sz;
		nbdata->nblist = realloc(nbdata->nblist, nbdata->sz * sizeof(nbentry));
	}
	nbdata->nblist[idx] = entry;
	nbdata->idx++;
}

typedef struct bentry
{
	unsigned blocking;
	bool redundant;

} bentry;

typedef struct bdata
{
	size_t sz;
	size_t idx;
	bentry *blist;
} bdata;

static inline void
add_to_blist(bdata *bdata, bentry entry)
{
	size_t idx = bdata->idx;
	size_t sz = bdata->sz;
	if (idx == sz)
	{
		// realloc
		bdata->sz = 2 * sz;
		bdata->blist = realloc(bdata->blist, bdata->sz * sizeof(bentry));
	}
	bdata->blist[idx] = entry;
	bdata->idx++;
}
#endif
static inline clause *
PROPAGATE_LITERAL(kissat *solver,
#if defined(HYPER_PROPAGATION) || defined(PROBING_PROPAGATION)
				  const clause *ignore,
#endif
				  const unsigned lit)
{
	assert(solver->watching);
	LOG(PROPAGATION_TYPE " propagating %s", LOGLIT(lit));
	assert(VALUE(lit) > 0);
	assert(EMPTY_STACK(solver->delayed));

	const word *arena = BEGIN_STACK(solver->arena);
	assigned *assigned = solver->assigned;
	value *values = solver->values;

	const unsigned not_lit = NOT(lit);
#ifdef HYPER_PROPAGATION
	const bool hyper = GET_OPTION(hyper);
#endif
	watches *watches = &WATCHES(not_lit);
	watch *begin_watches = BEGIN_WATCHES(*watches), *q = begin_watches;
	const watch *end_watches = END_WATCHES(*watches), *p = q;
	unsigneds *delayed = &solver->delayed;

	uint64_t ticks = kissat_cache_lines(watches->size, sizeof(watch));

	clause *res = 0;

#ifdef PREF_HEURISTIC
	bool flag = (solver->iter_count % 100 == 0);
#endif

#ifdef FOUR_PHASE_PROPAGATION
	/* Initialize a container to store Non-binary clauses */
	nbdata nbdata;
	nbdata.idx = 0;
	nbdata.sz = 100;
	nbdata.nblist = malloc(nbdata.sz * sizeof(nbentry));

	/* Initialize a container to store binary clauses */
	bdata bdata;
	bdata.idx = 0;
	bdata.sz = 100;
	bdata.blist = malloc(bdata.sz * sizeof(bentry));

	/* value of 'p' where the iteration has ended. != end_watches when a conflicting clause is found */
	const watch *end_watch_iter = end_watches;

	/* move_q : if true at the current entry, decrement q by 2*/
	bool move_q[end_watches - begin_watches];

	/*   ************************   PHASE 1 (Binary) *********************************/
	while (q != end_watches)
	{
#ifdef PREF_HEURISTIC
		uint64_t start;
		if (solver->current_phase == 2 && flag)
		{
			start = rdtsc(rdtsc_fence);
		}
#endif
		const watch head = *q++;

#ifdef PREF_HEURISTIC
		if (solver->current_phase == 2 && flag)
		{
			flag = false;
			uint64_t end = rdtsc(rdtsc_fence);
			uint64_t latency = end - start;
			solver->avg_latency = (solver->N * solver->avg_latency + latency) / (solver->N + 1);
			solver->N += 1;
		}
#endif
		const unsigned blocking = head.blocking.lit;
		printf("%d\n", blocking);
		assert(VALID_INTERNAL_LITERAL(blocking));
		const value blocking_value = values[blocking];

		if (head.type.binary)
		{

			if (blocking_value > 0)
				continue;
			const bool redundant = head.binary.redundant;

			if (blocking_value < 0)
			{
				res = kissat_binary_conflict(solver, redundant,
											 not_lit, blocking);
				end_watch_iter = q;
				break;
			}
			else
			{
				assert(!blocking_value);
				// kissat_assign_binary(solver, values, assigned,
				// 					 redundant, blocking, not_lit);
				bentry bentry;
				bentry.blocking = blocking;
				bentry.redundant = redundant;
				add_to_blist(&bdata, bentry);
				// add it to a list
			}
		}
		else
		{
			const watch tail = *q++;

			if (blocking_value > 0)
				continue;

			nbentry nbentry;
			nbentry.tail = tail;
			nbentry.q = q;
			add_to_nblist(&nbdata, nbentry);
			// add the entry to nblist
		}
	}

	/*   ************************   PHASE 2 (Non-binary) *********************************/
	size_t curr_idx = 0;
	size_t nb_sz = nbdata.idx;
	nbentry *nblist = nbdata.nblist;
	while (curr_idx < nb_sz)
	{ // iterate through the non-bin list
		// say we already have tail, q, p, c
		nbentry nbentry = nblist[curr_idx];
		curr_idx++;
		const watch tail = nbentry.tail;
		watch *q = nbentry.q;
		size_t widx = q - begin_watches;
		move_q[widx] = false;
		if (curr_idx < nb_sz)
			__builtin_prefetch(arena + nblist[curr_idx].tail.raw, 0, 3); // prefetch next clause

		const reference ref = tail.raw;
		assert(ref < SIZE_STACK(solver->arena));
		clause *c = (clause *)(arena + ref);

#if defined(HYPER_PROPAGATION) || defined(PROBING_PROPAGATION)
		if (c == ignore)
			continue;
#endif
		ticks++;

		if (c->garbage)
		{
			// q -= 2;
			move_q[widx] = true;

			continue;
		}

		unsigned *lits = BEGIN_LITS(c);
		const unsigned other = lits[0] ^ lits[1] ^ not_lit;
		assert(lits[0] != lits[1]);
		assert(VALID_INTERNAL_LITERAL(other));
		assert(not_lit != other);
		assert(lit != other);
		const value other_value = values[other]; // perhaps prefetch this too(note : high res time so this could be causing high bad speculation)

		if (other_value > 0)
			q[-2].blocking.lit = other;
		else
		{
			const unsigned *end_lits = lits + c->size;
			unsigned *searched = lits + c->searched;
			assert(c->lits + 2 <= searched);
			assert(searched < end_lits);
			unsigned *r, replacement = INVALID_LIT;
			value replacement_value = -1;
			for (r = searched; r != end_lits; r++)
			{
				replacement = *r;
				assert(VALID_INTERNAL_LITERAL(replacement));
				replacement_value = values[replacement];
				if (replacement_value >= 0)
					break;
			}
			if (replacement_value < 0)
			{
				for (r = lits + 2; r != searched; r++)
				{
					replacement = *r;
					assert(VALID_INTERNAL_LITERAL(replacement));
					replacement_value = values[replacement];
					if (replacement_value >= 0)
						break;
				}
			}

			if (replacement_value >= 0)
				c->searched = r - c->lits;

			if (replacement_value > 0)
			{
				assert(replacement != INVALID_LIT);
				q[-2].blocking.lit = replacement;
			}
			else if (!replacement_value)
			{
				assert(replacemewatches->sizent != INVALID_LIT);
				LOGREF(ref, "unwatching %s in", LOGLIT(not_lit));
				// q -= 2;
				move_q[widx] = true;
				lits[0] = other;
				lits[1] = replacement;
				assert(lits[0] != lits[1]);
				*r = not_lit;
				kissat_delay_watching_large(solver, delayed,
											replacement, other, ref);
				ticks++;
			}
			else if (other_value)
			{
				assert(replacement_value < 0);
				assert(blocking_value < 0);
				assert(other_value < 0);
				LOGREF(ref, "conflicting");
				res = c;
				end_watch_iter = p;
				break;
			}
#ifdef HYPER_PROPAGATION
			else if (hyper)
			{
				assert(replacement_value < 0);
				unsigned dom = kissat_find_dominator(solver, other, c);
				if (dom != INVALID_LIT)
				{
					LOGBINARY(dom, other, "hyper binary resolvent");

					INC(hyper_binary_resolved);
					INC(clauses_added);

					INC(hyper_binaries);
					INC(clauses_redundant);

					CHECK_AND_ADD_BINARY(dom, other);
					ADD_BINARY_TO_PROOF(dom, other);

					kissat_assign_binary(solver, values, assigned,
										 true, other, dom);

					delay_watching_hyper(solver, delayed, dom, other);
					delay_watching_hyper(solver, delayed, other, dom);

					kissat_delay_watching_large(solver, delayed,
												not_lit, other, ref);

					LOGREF(ref, "unwatching %s in", LOGLIT(not_lit));
					move_q[widx] = true;
					// q -= 2;
				}
				else
					kissat_assign_reference(solver, values,
											assigned, other, ref, c);
			}
#endif
			else
			{
				assert(replacement_value < 0);
				kissat_assign_reference(solver, values,
										assigned, other, ref, c);
			}
		}
	}
	/*   ************************   PHASE 3 (Assign binary) *********************************/
	curr_idx = 0;
	size_t b_sz = bdata.idx;
	bentry *blist = bdata.blist;
	while (curr_idx < b_sz)
	{ // iterate through binary clauses with blocking_value = 0
		bool redundant = blist[curr_idx].redundant;
		unsigned blocking = blist[curr_idx].blocking;
		curr_idx++;
		kissat_assign_binary(solver, values, assigned,
							 redundant, blocking, not_lit);
	}
	/*   ************************   PHASE 4 (Compact) *********************************/
	q = begin_watches;
	p = begin_watches;
	size_t widx = 0;
	while (p != end_watch_iter)
	{
		*q++ = *p++;
		if (move_q[widx])
			q -= 2;
		widx++;
	}
#endif

#ifndef FOUR_PHASE_PROPAGATION
	while (p != end_watches)
	{
#ifdef PREF_HEURISTIC
		uint64_t start;
		if (solver->current_phase == 2 && flag)
		{
			start = rdtsc(rdtsc_fence);
		}
#endif
		const watch head = *q++ = *p++;

#ifdef PREF_HEURISTIC
		if (solver->current_phase == 2 && flag)
		{
			flag = false;
			uint64_t end = rdtsc(rdtsc_fence);
			uint64_t latency = end - start;
			solver->avg_latency = (solver->N * solver->avg_latency + latency) / (solver->N + 1);
			solver->N += 1;
		}
#endif
		const unsigned blocking = head.blocking.lit;
		assert(VALID_INTERNAL_LITERAL(blocking));
		const value blocking_value = values[blocking];

		if (head.type.binary)
		{

			if (blocking_value > 0)
				continue;
			const bool redundant = head.binary.redundant;

			if (blocking_value < 0)
			{
				res = kissat_binary_conflict(solver, redundant,
											 not_lit, blocking);
				break;
			}
			else
			{
				assert(!blocking_value);
				kissat_assign_binary(solver, values, assigned,
									 redundant, blocking, not_lit);
			}
		}
		else
		{

			const watch tail = *q++ = *p++;

			if (blocking_value > 0)
				continue;
			const reference ref = tail.raw;
			assert(ref < SIZE_STACK(solver->arena));
			clause *c = (clause *)(arena + ref);
#if defined(HYPER_PROPAGATION) || defined(PROBING_PROPAGATION)
			if (c == ignore)
				continue;
#endif
			ticks++;

			if (c->garbage)
			{
				q -= 2;
				continue;
			}

			unsigned *lits = BEGIN_LITS(c);
			const unsigned other = lits[0] ^ lits[1] ^ not_lit;
			assert(lits[0] != lits[1]);
			assert(VALID_INTERNAL_LITERAL(other));
			assert(not_lit != other);
			assert(lit != other);
			const value other_value = values[other];
			if (other_value > 0)
				q[-2].blocking.lit = other;
			else
			{
				const unsigned *end_lits = lits + c->size;
				unsigned *searched = lits + c->searched;
				assert(c->lits + 2 <= searched);
				assert(searched < end_lits);
				unsigned *r, replacement = INVALID_LIT;
				value replacement_value = -1;
				for (r = searched; r != end_lits; r++)
				{
					replacement = *r;
					assert(VALID_INTERNAL_LITERAL(replacement));
					replacement_value = values[replacement];
					if (replacement_value >= 0)
						break;
				}
				if (replacement_value < 0)
				{
					for (r = lits + 2; r != searched; r++)
					{
						replacement = *r;
						assert(VALID_INTERNAL_LITERAL(replacement));
						replacement_value = values[replacement];
						if (replacement_value >= 0)
							break;
					}
				}

				if (replacement_value >= 0)
					c->searched = r - c->lits;

				if (replacement_value > 0)
				{
					assert(replacement != INVALID_LIT);
					q[-2].blocking.lit = replacement;
				}
				else if (!replacement_value)
				{
					assert(replacemewatches->sizent != INVALID_LIT);
					LOGREF(ref, "unwatching %s in", LOGLIT(not_lit));
					q -= 2;
					lits[0] = other;
					lits[1] = replacement;
					assert(lits[0] != lits[1]);
					*r = not_lit;
					kissat_delay_watching_large(solver, delayed,
												replacement, other, ref);
					ticks++;
				}
				else if (other_value)
				{
					assert(replacement_value < 0);
					assert(blocking_value < 0);
					assert(other_value < 0);
					LOGREF(ref, "conflicting");
					res = c;
					break;
				}
#ifdef HYPER_PROPAGATION
				else if (hyper)
				{
					assert(replacement_value < 0);
					unsigned dom = kissat_find_dominator(solver, other, c);
					if (dom != INVALID_LIT)
					{
						LOGBINARY(dom, other, "hyper binary resolvent");

						INC(hyper_binary_resolved);
						INC(clauses_added);

						INC(hyper_binaries);
						INC(clauses_redundant);

						CHECK_AND_ADD_BINARY(dom, other);
						ADD_BINARY_TO_PROOF(dom, other);

						kissat_assign_binary(solver, values, assigned,
											 true, other, dom);

						delay_watching_hyper(solver, delayed, dom, other);
						delay_watching_hyper(solver, delayed, other, dom);

						kissat_delay_watching_large(solver, delayed,
													not_lit, other, ref);

						LOGREF(ref, "unwatching %s in", LOGLIT(not_lit));
						q -= 2;
					}
					else
						kissat_assign_reference(solver, values,
												assigned, other, ref, c);
				}
#endif
				else
				{
					assert(replacement_value < 0);
					kissat_assign_reference(solver, values,
											assigned, other, ref, c);
				}
			}
		}
	}
#endif

	solver->ticks += ticks;

	while (p != end_watches)
		*q++ = *p++;
	SET_END_OF_WATCHES(*watches, q);

#ifdef HYPER_PROPAGATION
	watch_hyper_delayed(solver, delayed);
#else
	kissat_watch_large_delayed(solver, delayed);
#endif

	return res;
}
