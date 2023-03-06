#ifndef _queue_h_INCLUDED
#define _queue_h_INCLUDED

#define DISCONNECT UINT_MAX
#define DISCONNECTED(IDX) ((int)(IDX) < 0)

#include "value.h"
struct kissat;

typedef struct links links;
typedef struct exp_queue exp_queue;
typedef struct queue queue;

struct links
{
  unsigned prev, next;
  unsigned stamp;

  unsigned q_idx;
};

struct exp_queue
{
  unsigned variable_idx, prev, next;
  //value value; // value of the lit, if 0 => unassigned
};

struct queue
{
  unsigned first, last, stamp;
  struct
  {
    unsigned idx, stamp;
  } search;
  size_t sz;  // size of exp_queue
};

void compact_exp_queue(kissat *,bool);
void kissat_init_queue (queue *);
void kissat_enqueue (struct kissat *, unsigned idx);
void kissat_dequeue (struct kissat *, unsigned idx);
void kissat_move_to_front (struct kissat *, unsigned idx);

#define LINK(IDX) \
  (solver->links[assert ((IDX) < VARS), (IDX)])

#endif
