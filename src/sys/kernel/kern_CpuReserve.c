/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Functions for reservation creation and management */
#include <kerninc/kernel.h>
#include <kerninc/CpuReserve.h>
#include <kerninc/Process.h>
#include <kerninc/SysTimer.h>
#include <kerninc/Machine.h>
#include <kerninc/rbtree.h>
#include <kerninc/Process-inline.h>
#include <eros/ffs.h>
#include <eros/fls.h>
#include <idl/capros/SchedC.h>

static void indexFixup(void);

/* these should be moved to KernTune.h eventually 
   they are here for now, while the scheduler is still 
   being developed */

/* have a global variable for rbtree min find */
void printTree(rbnode *n);

#define NODE_SIZE 32
#define MAX_RESERVE 32
#define RES_LEVELS 1            /* = log32 (MAX_RESERVE) */
#define SPAN 1                  /* = (MAX_RESERVE/32)    */

/*#define RESERVE_DEBUG*/
/*#define TREE_INSERT*/

Reserve *res_ReserveTable = 0;

Reserve **ReservePointers = 0;

typedef struct res_active res_active;
struct res_active {
  uint32_t bits;
  union {
    struct res_active *next;
    Reserve *res;
  } u[32];
};

rbtree *res_tree = 0;
rbnode *res_nodes[MAX_RESERVE];
res_active *res_ResTreeRoot = 0;
static int res_nextFree = 0;

typedef int (*qsortfn)(void *, void *);
extern void readyq_ReserveWakeup(ReadyQueue *r, struct Activity *t);
extern void readyq_ReserveTimeout(ReadyQueue *r, struct Activity *t);

INLINE int
flsb(uint32_t x)
{
  if (!x)
    return 0;
  else
    return ffs32(x) + 1;
}

int
res_cmpfn(void *v1, void *v2)
{
  Reserve *r1 = *((Reserve **)v1);
  Reserve *r2 = *((Reserve **)v2);

  if (r1->period < r2->period)
    return -1;
  if (r1->period > r2->period)
    return 1;
  return 0;
}

int
res_cmpnodes(const rbnode *r1, const rbnode *r2)
{
  if (r1->value.w < r2->value.w)
    return -1;
  if (r1->value.w > r2->value.w)
    return 1;
  return 0;
}

int
res_cmpkey(const rbnode *r1, const rbkey *k1)
{
  if (r1->value.w < k1->w)
    return -1;
  if (r1->value.w > k1->w)
    return 1;
  return 0;
}

/* initializes the readyQ field of the reserve */
static void
res_InitReadyQ(Reserve *r)
{
  sq_Init(&r->readyQ.queue);
  r->readyQ.mask = (1u << capros_SchedC_Priority_Reserve);
  r->readyQ.other = r;
  r->readyQ.doWakeup = readyq_ReserveWakeup;
  r->readyQ.doQuantaTimeout = readyq_ReserveTimeout;
}

/////////////////////////////// TEMP //////////////////

typedef struct resList resList;
struct resList {
  Link head;
};

typedef struct resListItem resListItem;
struct resListItem {
  Link item;
  Reserve *res;
};

resList InactiveList = {
  {&InactiveList.head, &InactiveList.head}
};

resListItem itemBuffer[3] = {
  {{&itemBuffer[0].item, &itemBuffer[0].item}, 0},
  {{&itemBuffer[1].item, &itemBuffer[1].item}, 0},
  {{&itemBuffer[2].item, &itemBuffer[2].item}, 0},
};

resListItem *
rli_create(Reserve *r)
{
#if 0
  resListItem *rli = MALLOC(resListItem, 1/*sizeof(resListItem)*/);
  link_Init(&rli->item);
  rli->res = r;
#endif
  resListItem *rli = &itemBuffer[r->index];
  rli->res = r;

  return rli;
}

void
rli_insert(resListItem *it, uint64_t key)
{
  if (link_isSingleton(&InactiveList.head))
    link_insertAfter(&InactiveList.head, &it->item);
  else {
    Link *ptr = InactiveList.head.next;
    while (ptr && ptr->next != &InactiveList.head) {
      Reserve *r = ((resListItem *)ptr)->res;
      if (key < r->nextDeadline) {
        link_insertAfter(&InactiveList.head, &it->item);
        return;
      }
      ptr = ptr->next;
    }
    /* reached end of list */
    link_insertAfter(ptr, &it->item);
  }
}

Reserve *
rli_getMin()
{
  resListItem *r = 0;
  Link *it = 0;
  
  if (link_isSingleton(&InactiveList.head))
    return 0;
  it = InactiveList.head.next;
  r = (resListItem *)it;
  
  return r->res;
}

void
rli_removeMin()
{
  assert (link_isSingleton(&InactiveList.head) == false);
  link_Unlink(InactiveList.head.next);
}

/////////////////////////////// END TEMP //////////////


/* allocate reserve table */
void 
res_AllocReserves()
{
  int i = 0;
  Reserve *r = 0;
  
  res_ReserveTable = MALLOC(Reserve, MAX_RESERVE);
  ReservePointers = MALLOC(Reserve *, MAX_RESERVE);
  for (i = 0; i < MAX_RESERVE; i++) {
    r = &res_ReserveTable[i];
    r->index = i;
    r->duration = 0;
    r->period = 0;
    r->nextDeadline = 0;
    r->timeAcc = 0;
    r->totalTimeAcc = 0;
    r->lastSched = 0;
    r->lastDesched = 0;
    r->isActive = false;
    res_InitReadyQ(r);
    ReservePointers[i] = r;
  }
  res_AllocResTree();
  printf("Allocated Reserve Table\n");
}

/* allocate RB-tree for deplenished reserves */
static void
res_AllocRBNodes()
{
  int i = 0;

  for (i = 0; i < MAX_RESERVE; i++)
    res_nodes[i] = rbnode_create(0, 0, &res_ReserveTable[i]);
  res_tree = rbtree_create(res_cmpnodes, res_cmpkey, false);
}

#if RES_LEVELS > 1
#error "AllocResTree() needs to be implemented"
#endif
#if 0
/* allocate active reserve tree */
void
res_AllocResTree()
{
  int i = 0;
  int j = 0;
  int offset = 0;

  res_ResTreeRoot = MALLOC(res_active, sizeof(res_active));
  
  for (i = 0; i < SPAN; i++) {
    res_ResTreeRoot->u[i].next = MALLOC(res_active, sizeof(res_active));
    for (j = 0; j < SPAN; j++) {
      res_ResTreeRoot->u[i].next->u[j].res = &res_ReserveTable[offset];
      offset++;
    }
  }
  res_AllocRBNodes();
  printf("Allocated active and deplenished reserve trees\n");
}
#endif

/* allocate active reserve tree */
void
res_AllocResTree()
{
  int i = 0;

  res_ResTreeRoot = MALLOC(res_active, sizeof(res_active));
  
  for (i = 0; i < NODE_SIZE; i++) {
    res_ResTreeRoot->u[i].res = &res_ReserveTable[i];
  }
  res_AllocRBNodes();
  printf("Allocated Active and Deplenished reserve trees\n");
}

/* finds the active reserve with the earliest deadline */
unsigned 
res_find_earliest_reserve()
{
  res_active* top = res_ResTreeRoot;
  int s = SPAN;
  unsigned ndx = flsb(top->bits);

#if 0
  printf("in find_earliest, map = %d ,ndx = %d\n", top->bits, ndx);
#endif
  while (s > 1) {
    top = top->u[ndx].next;
    ndx = flsb(top->bits);
    s = s >> 5; /* divide by 32 */
  }
  return ndx;
}

extern uint32_t act_RunQueueMap;
void
res_SetActive(uint32_t ndx)
{
#if RES_LEVELS > 1
#error "AllocResTree() needs to be implemented"
#endif
#if 0
  uint32_t shift = 5 * (RES_LEVELS-1);
  res_active *top = res_ResTreeRoot;
  uint32_t bit = 0;

  while (shift > 0) {
    bit = (ndx >> shift) & (31u);
    top->bits |= (1u << bit);
    shift -= 5;
    top = top->u[bit].next;
  }
  bit = ndx & (31u);
  top->bits |= (1u << bit);
#endif
  res_active *top = res_ResTreeRoot;

  top->bits |= (1u << ndx);
  res_ReserveTable[ndx].isActive = true;
  act_RunQueueMap |= res_ReserveTable[ndx].readyQ.mask;
#ifdef RESERVE_DEBUG
  printf("reserve %d set active. res_map = %d\n", ndx, top->bits);
#endif
}

void
res_SetInactive(uint32_t ndx)
{
#if RES_LEVELS > 1
#error "AllocResTree() needs to be implemented"
#endif
#if 0
  uint32_t shift = 5 * (RES_LEVELS-1);
  res_active *top = res_ResTreeRoot;
  uint32_t bit = 0;
  
  while (shift > 0) {
    bit = (ndx >> shift) & (31u);
    top->bits &= ~(1u << bit);
    shift -= 5;
    top = top->u[bit].next;
  }
  bit = ndx & (31u);
#endif
  res_active *top = res_ResTreeRoot;

  top->bits &= ~(1u << ndx);
  res_ReserveTable[ndx].isActive = false;
#ifdef RESERVE_DEBUG
  printf("reserve %d set inactive. res_map = %d", ndx, top->bits);
  printf(" run queue map = %d\n", fls32(act_RunQueueMap) -1);
#endif
}

/* once a reserve has been accepted by the admission controller,
   call this function to set the parameters and make it active
   note: the reserve table needs to be sorted to reflect the new
   entry. units of p, d is ticks 
*/
void
res_ActivateReserve(Reserve *r)
{
#if 0
  r->period = p;
  r->duration = d;
  r->isActive = true;
  r->nextDeadline = sysT_Now() + r->period;
  r->timeLeft = r->duration;
  res_SetActive(r->index);
  res_nextFree++;
#endif
  printf("sorting...\n");
  //qsort(res_ReserveTable, MAX_RESERVE, sizeof(Reserve), (qsortfn)res_cmpfn);
  qsort(ReservePointers, MAX_RESERVE, sizeof(Reserve*), (qsortfn)res_cmpfn);
  indexFixup();
  printf("done sorting...\n");
}

/* once an existing reservation is no longer needed, deactivate it 
   and make it available for a new reservation
*/
void
res_DeactivateReserve(Reserve *r)
{
  r->period = 0;
  r->duration = 0;
  r->isActive = false;
  r->nextDeadline = 0;
  r->timeAcc = 0;
  r->totalTimeAcc = 0;
  res_SetInactive(r->index);
  res_nextFree--;
  qsort(ReservePointers, MAX_RESERVE, sizeof(Reserve*), (qsortfn)res_cmpfn);
  indexFixup();
}

/* Get the next reserve to run by traversing active reserve tree */
Reserve *
res_GetEarliestReserve()
{
  unsigned ndx = res_find_earliest_reserve();

#if 0
  printf("found res %d\n", ndx);
#endif
  if (ndx == 0 )
    return 0;
  return &res_ReserveTable[ndx-1];
}

/* replenish reserve duration at beginning of period */
void
res_ReplenishReserve(Reserve* r)
{
#ifdef RESERVE_DEBUG
  printf("reserve index %d", r->index);
  printf("used %U ms time this period\n",
         mach_TicksToMilliseconds(r->timeAcc));
#endif
  r->timeAcc = 0;
  r->nextDeadline = r->nextDeadline + r->period;
  res_SetActive(r->index);
}

/* reserve has run out of time. set it inactive
   until it replenishes. 
*/
void
res_DeplenishReserve(Reserve* r)
{
#ifdef RESERVE_DEBUG
  printf("start DeplenishReserve()...\n");
#endif
  if (r->timeAcc >= r->duration) {
    /* move reserve to deplenished reserve list */
#if TREE_INSERT
    res_nodes[r->index]->value.w = r->nextDeadline;
    /* because indices can change, set the data field */
    res_nodes[r->index]->data = r; 
#endif/*TREE_INSERT*/

#ifdef RESERVE_DEBUG
    printf("inserting node with key value %d", r->nextDeadline);
    printf(" nd value = %d\n", ((Reserve *)nd->data)->nextDeadline);
#endif

#if TREE_INSERT
    rbtree_insert(res_tree, res_nodes[r->index]);
#else
    rli_insert(rli_create(r), r->nextDeadline);
#endif

#ifdef RESERVE_DEBUG
    printf("inserted inactive reserve index %d in reserve tree...", r->index);
    //printf("next replenish = %d\n", 
    //     /*mach_TicksToMilliseconds*/(NextReplenishTime));
#endif
  }
#ifdef RESERVE_DEBUG
  printf("replenish time = %d\n", r->nextDeadline); 
  //printf("done DeplenishReserve()...\n");
#endif
}

/* determine the next time for a scheduling interrupt
   this will be =
   min(duration of current running reserve, 
       start of next period of deplenished reserves)
   if there is no current reserve, this will be =
   min(10ms, time till next replenishment);
*/
#define QUANTA 10
uint64_t
NextTimeInterrupt(Reserve *current)
{
  uint64_t mintime = 0;
  uint64_t now = 0;
  Reserve *r = 0;
#ifdef TREE_INSERT
  rbnode *next = 0;
#endif
  uint64_t NextReplenishTime = 0;

  //printf("starting NextTimeInterrupt()...\n");
  now = sysT_Now();
#ifdef TREE_INSERT
  next = rbtree_min(res_tree);
  r = (Reserve *)next->data;
#else
  r = rli_getMin();
#endif
  if (r == NULL)
    NextReplenishTime = 0;
  else {
    NextReplenishTime = r->nextDeadline;
  }
  
  if (current == 0) {
    //printf("starting NextTimeInterrupt(0)...\n");
    if (NextReplenishTime < now)
      mintime = now + mach_MillisecondsToTicks(QUANTA);
    else
      mintime = min(NextReplenishTime, 
                    now + mach_MillisecondsToTicks(QUANTA)); 
    //printf("mintime = %d\n", mach_TicksToMilliseconds(mintime));
    return mintime;
  }
  else {
    if (NextReplenishTime < now)
      mintime = now + (current->duration - current->timeAcc);
    else
      mintime = min(NextReplenishTime, now + 
                    (current->duration - current->timeAcc));
  }

#ifdef RESERVE_DEBUG
  printf("now = %d", /*mach_TicksToMilliseconds*/(now));
  printf(" mintime = %d\n", /*mach_TicksToMilliseconds*/(mintime));
#endif

  return mintime;//+now;
}

static void
DoNeedReplenish()
{
  uint64_t now = sysT_Now();
#ifdef TREE_INSERT
  rbnode *next = 0;
#endif
  Reserve *r = 0;
  bool done = false;

#if 1
  //printf("in DoNeedReplenish()...\n");
  while (!done) {
#ifdef TREE_INSERT
    next = rbtree_min(res_tree);
    r = (Reserve *)next->data;
#else
    r = rli_getMin();
#endif
    if (r == NULL)
      return;
    //printf("next deadline = %u", r->nextDeadline);
    //printf(" now = %u\n", now);
    if (now >= r->nextDeadline) {
      //printf("calling Replenish() from DoNeedReplenish()...\n");
      res_ReplenishReserve(r);
#ifdef TREE_INSERT
      rbtree_remove(res_tree, next);
#else
      rli_removeMin();
#endif
#ifdef RESERVE_DEBUG
      printf("removed reserve index %d from inactive tree...\n", r->index);
#endif

    }
    else {
      done = true;
    }
  }
#endif/*1*/
  //printf("done DoNeedReplenish()...\n");
}

void printTable(int top)
{
  int i = 0;

  for (i = 0; i < top; i++) {
    printf("reserve entry %d", i);
    printf(" d = %d", res_ReserveTable[i].duration);
    printf(" p = %d", res_ReserveTable[i].period);
    printf(" a = %d", res_ReserveTable[i].isActive);
    printf("\n");
  }
}

void printTree(rbnode *n) 
{
  Reserve *r = 0;

  if (n->left != TREE_NIL)
    printTree(n->left);
  r = (Reserve *)n->data;
  printf("node index = %d", r->index);
  printf(" data = %d\n", r->nextDeadline);
  if (n->right != TREE_NIL)
    printTree(n->right);
}

static void indexFixup(void)
{
  int i = 0;

  for (i = 0; i < MAX_RESERVE; i++)
    res_ReserveTable[i].index = i;
}

#define DURATION 30
#define PERIOD 100
Reserve *
res_GetNextReserve()
{
  Reserve *r = 0;
  
  /* for testing purposes, preset the parameters of the reserve to schedulable
     values. once the admission controller is added, use that.
  */
  r = &res_ReserveTable[res_nextFree];
  r->period = mach_MillisecondsToTicks(PERIOD)/*+res_nextFree*/;
  r->duration = mach_MillisecondsToTicks(DURATION);
  r->timeAcc = 0;
  r->isActive = true;


#if 1
  printf("set reserve with p = %u", r->period);
  printf(" active = %d\n", res_ReserveTable[res_nextFree].isActive);
#endif
  res_ActivateReserve(r);
  res_nextFree++;
  printTable(res_nextFree);

  return r;
}

void
res_SetReserveInfo(uint32_t p, uint32_t d, uint32_t ndx)
{
  Reserve *r = 0;

  r = &res_ReserveTable[ndx];
  r->period = mach_MillisecondsToTicks(p);
  r->duration = mach_MillisecondsToTicks(d);
  r->timeAcc = 0;
  r->isActive = true;

#if 1
  printf("accepted reserve with d = %u", d);
  printf(" p = %u\n", p);
#endif
  res_ActivateReserve(r);
  printTable(ndx+1);
}

// This procedure is called from the clock interrupt with IRQ disabled.
void 
res_ActivityTimeout(uint64_t now)
{
  //printf("start QuantaExpired...%d\n", now);
  
  if (act_Current() && act_Current()->readyQ->other) {
    Reserve * r = (Reserve *)act_Current()->readyQ->other;
    
    if (r->isActive) {
      r->lastDesched = now;
#ifdef RESERVE_DEBUG
      printf("old time left = %d", r->timeLeft);
#endif
      r->timeAcc += r->lastDesched - r->lastSched;
      r->totalTimeAcc += r->timeAcc;
#ifdef RESERVE_DEBUG
      printf(" reserve index = %d", r->index);
      printf(" time left = %u", r->timeLeft);
      printf(" duration = %u\n", r->duration);
#endif
#if 0
      printf(" active = %d", r->isActive);
      printf(" table active = %d\n", res_ReserveTable[r->index].isActive);
#endif
      if (r->timeAcc >= r->duration) {
#ifdef RESERVE_DEBUG
        printf("reserve exhausted: %d\n", r->timeLeft);
#endif
        res_SetInactive(r->index);
      }
#ifdef RESERVE_DEBUG
      printf("done QuantaExpired for reserve...\n");
#endif
    }
  }

  DoNeedReplenish();
  act_ForceResched();
}
