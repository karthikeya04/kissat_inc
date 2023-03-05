#include "inline.h"
#include "print.h"

#include <inttypes.h>

void kissat_init_queue(queue *queue)
{
  queue->first = queue->last = DISCONNECT;
  assert(!queue->stamp);

  queue->search.idx = DISCONNECT;
  queue->sz = 1e6;
  assert(!queue->search.stamp);
}

static void
reassign_queue_stamps(kissat *solver, links *links, queue *queue)
{
  kissat_very_verbose(solver, "need to reassign enqueue stamps on queue");
#ifdef QUIET
  (void)solver;
#endif
  queue->stamp = 0;

  struct links *l;
  for (unsigned idx = queue->first; !DISCONNECTED(idx); idx = l->next)
    (l = links + idx)->stamp = ++queue->stamp;

  if (!DISCONNECTED(queue->search.idx))
    queue->search.stamp = links[queue->search.idx].stamp;
}

void compact_exp_queue(kissat *solver, bool inc_size)
{
  struct links *l;
  links *links = solver->links;
  struct exp_queue *q = solver->exp_queue;
  queue queue = solver->queue;
  for (unsigned idx = queue.first; !DISCONNECTED(idx); idx = l->next)
  {
    l = links + idx;
    assert(!DISCONNECTED(l->q_idx));
    unsigned q_idx = l->q_idx;
    struct exp_queue *curr = q + q_idx;
    unsigned n_idx = 0; // new index in exp_queue for the current variable
    if (curr->prev != DISCONNECT)
    {
      n_idx = curr->prev + 1;
    }
    struct exp_queue *new_curr = q + n_idx;
    //new_curr->value = curr->value;
    new_curr->prev = curr->prev;
    new_curr->variable_idx = curr->variable_idx;
    if (!DISCONNECTED(curr->next))
      (q + curr->next)->prev = n_idx;
    if (n_idx > 0)
    {
      (q + n_idx - 1)->next = n_idx;
    }
    l->q_idx = n_idx;
    if (DISCONNECTED(l->next))
    {
      new_curr->next = DISCONNECT;
    }
  }
  if (inc_size)
  {
    unsigned new_size = 2 * sizeof(exp_queue) * solver->queue.sz;
    solver->exp_queue = realloc(solver->exp_queue, new_size);
    solver->queue.sz = 2 * solver->queue.sz;
  }
}

static inline void
enqueue(kissat *solver, unsigned i, links *links, queue *queue)
{
  struct links *p = links + i;
  assert(DISCONNECTED(p->prev));
  assert(DISCONNECTED(p->next));
  assert(DISCONNECTED(p->q_idx));
  const unsigned j = p->prev = queue->last;

  queue->last = i;
  if (DISCONNECTED(j))
  {
    queue->first = i;
    struct exp_queue *q = solver->exp_queue;
    q->variable_idx = i;
    q->prev = DISCONNECT;
    q->next = DISCONNECT;
    //q->value = solver->values[LIT(i)];
    p->q_idx = 0;
  }
  else
  {
    struct links *l = links + j;
    assert(DISCONNECTED(l->next));
    l->next = i;

    assert(!DISCONNECTED(l->q_idx));
    unsigned last_q_idx = l->q_idx;
    struct exp_queue *previous = solver->exp_queue + last_q_idx;
    struct exp_queue *current = solver->exp_queue + last_q_idx + 1;
    assert(DISCONNECTED(previous->next));
    previous->next = last_q_idx + 1;
    current->prev = last_q_idx;
    current->next = DISCONNECT;
    current->variable_idx = i;
    //current->value = solver->values[LIT(i)];
    p->q_idx = last_q_idx + 1;
    if (last_q_idx + 2 == solver->queue.sz)
    {
      compact_exp_queue(solver,true);
    }
  }
  if (queue->stamp == UINT_MAX)
  {
    reassign_queue_stamps(solver, links, queue);
    assert(p->stamp == queue->stamp);
  }
  else
    p->stamp = ++queue->stamp;
}

static inline void
dequeue(unsigned i, links *links, queue *queue, exp_queue *exp_queue)
{
  struct links *l = links + i;
  const unsigned j = l->prev, k = l->next;
  l->prev = l->next = DISCONNECT;

  unsigned q_idx = l->q_idx;
  l->q_idx = DISCONNECT;
  assert(!DISCONNECTED(q_idx));
  struct exp_queue *q = exp_queue + q_idx;
  const unsigned jq = q->prev, kq = q->next;
  q->prev = q->next = DISCONNECT;

  if (DISCONNECTED(j))
  {
    assert(queue->first == i);
    queue->first = k;
  }
  else
  {
    struct links *p = links + j;
    assert(p->next == i);
    p->next = k;

    struct exp_queue *qp = exp_queue + jq;
    assert(qp->next == q_idx);
    qp->next = kq;
  }
  if (DISCONNECTED(k))
  {
    assert(queue->last == i);
    queue->last = j;
  }
  else
  {
    struct links *n = links + k;
    assert(n->prev == i);
    n->prev = j;

    assert(!DISCONNECTED(kq));
    struct exp_queue *qn = exp_queue + kq;
    assert(qn->prev == q_idx);
    qn->prev = jq;
  }
}

#if defined(CHECK_QUEUE) && !defined(NDEBUG)
void kissat_check_queue(kissat *solver)
{
  links *links = solver->links;
  queue *queue = &solver->queue;
  bool passed_search_idx = false;
  for (unsigned idx = queue->first, prev = DISCONNECT;
       !DISCONNECTED(idx); idx = links[idx].next)
  {
    if (!DISCONNECTED(prev))
      assert(links[prev].stamp < links[idx].stamp);
    if (passed_search_idx)
      assert(VALUE(LIT(idx)));
    if (idx == queue->search_idx)
      passed_search_idx = true;
  }
  if (!DISCONNECTED(queue->search_idx))
    assert(links[queue->search_idx].stamp == queue->search_stamp);
}
#else
#define kissat_check_queue(...) \
  do                            \
  {                             \
  } while (0)
#endif

void kissat_enqueue(kissat *solver, unsigned idx)
{
  assert(idx < solver->vars);
  links *links = solver->links, *l = links + idx;
  l->prev = l->next = DISCONNECT;
  l->q_idx = DISCONNECT;
  enqueue(solver, idx, links, &solver->queue);
  LOG("enqueued variable %u stamped %u", idx, l->stamp);
  if (!VALUE(LIT(idx)))
    kissat_update_queue(solver, links, idx);
  kissat_check_queue(solver);
}

void kissat_dequeue(kissat *solver, unsigned idx)
{
  assert(idx < solver->vars);
  LOG("dequeued variable %u", idx);
  links *links = solver->links;
  if (solver->queue.search.idx == idx)
  {
    struct links *l = links + idx;
    unsigned search = l->next;
    if (search == DISCONNECT)
      search = l->prev;
    if (search == DISCONNECT)
    {
      solver->queue.search.idx = DISCONNECT;
      solver->queue.search.stamp = 0;
    }
    else
      kissat_update_queue(solver, links, search);
  }
  dequeue(idx, links, &solver->queue, solver->exp_queue);
  kissat_check_queue(solver);
}

void kissat_move_to_front(kissat *solver, unsigned idx)
{
  queue *queue = &solver->queue;
  links *links = solver->links;
  if (idx == queue->last)
  {
    assert(DISCONNECTED(links[idx].next));
    return;
  }
  assert(idx < solver->vars);
  const value tmp = VALUE(LIT(idx));
  if (tmp && queue->search.idx == idx)
  {
    unsigned prev = links[idx].prev;
    if (!DISCONNECTED(prev))
      kissat_update_queue(solver, links, prev);
    else
    {
      unsigned next = links[idx].next;
      assert(!DISCONNECTED(next));
      kissat_update_queue(solver, links, next);
    }
  }
  dequeue(idx, links, queue, solver->exp_queue);
  enqueue(solver, idx, links, queue);
  LOG("moved-to-front variable %u stamped %u", idx, LINK(idx).stamp);
  if (!tmp)
    kissat_update_queue(solver, links, idx);
  kissat_check_queue(solver);
}
