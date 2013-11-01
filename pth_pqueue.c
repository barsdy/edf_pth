/*
**  GNU Pth - The GNU Portable Threads
**  Copyright (c) 1999-2006 Ralf S. Engelschall <rse@engelschall.com>
**
**  This file is part of GNU Pth, a non-preemptive thread scheduling
**  library which can be found at http://www.gnu.org/software/pth/.
**
**  This library is free software; you can redistribute it and/or
**  modify it under the terms of the GNU Lesser General Public
**  License as published by the Free Software Foundation; either
**  version 2.1 of the License, or (at your option) any later version.
**
**  This library is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**  Lesser General Public License for more details.
**
**  You should have received a copy of the GNU Lesser General Public
**  License along with this library; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
**  USA, or contact Ralf S. Engelschall <rse@engelschall.com>.
**
**  pth_pqueue.c: Pth thread priority queues
*/
                             /* ``Real hackers can write assembly
                                  code in any language''
                                                   -- Unknown */
/* Murray modified: erease the q_prio */
#include "pth_p.h"

#if cpp

/* thread priority queue */
struct pth_pqueue_st {
    pth_t q_head;
    int   q_num;
};
typedef struct pth_pqueue_st pth_pqueue_t;

#endif /* cpp */

/* initialize a priority queue; O(1) */
intern void pth_pqueue_init(pth_pqueue_t *q)
{
    if (q != NULL) {
        q->q_head = NULL;
        q->q_num  = 0;
    }
    return;
}

/* insert thread into priority queue; O(n) */
/* murray modified for edf schedule */
intern void pth_pqueue_insert(pth_pqueue_t *q, int prio, pth_t t)
{
    pth_t c;
    int rt_task;

    if (q == NULL || t == NULL)
        return;
    rt_task = t->flag & PTH_TASK_RT;
    if (q->q_head == NULL || q->q_num == 0) {
        /* add as first element */
        t->q_prev = t;
        t->q_next = t;
        q->q_head = t;
    }
    else if ((rt_task && !((q->q_head->flag & PTH_TASK_RT) && \
            pth_time_cmp(&q->q_head->deadline, &t->deadline) < 0)) || \
            (!rt_task && !(q->q_head->flag & PTH_TASK_RT) && (t->prio > q->q_head->prio))) {
        /* add as new head of queue */
        t->q_prev = q->q_head->q_prev;
        t->q_next = q->q_head;
        t->q_prev->q_next = t;
        t->q_next->q_prev = t;
        /*
        t->q_prio = prio;
        t->q_next->q_prio = prio - t->q_next->q_prio;
        */
        q->q_head = t;
    }
    else {
        /* insert after elements with earlier or equal deadline */
        c = q->q_head;
        if (rt_task) {
            while (pth_time_cmp(&(c->deadline), &(t->deadline)) <= 0 && c->q_next != q->q_head) 
                c = c->q_next;
        } else {
            while ((c->flag & PTH_TASK_RT) || ((t->prio < c->prio) && c->q_next != q->q_head)) 
                c = c->q_next;
        }
        
        t->q_next = c;
        t->q_prev = c->q_prev;
        c->q_prev->q_next = t;
        c->q_prev = t;

        /*
        t->q_prio = p - prio;
        if (t->q_next != q->q_head)
            t->q_next->q_prio -= t->q_prio;
        */
    }

    q->q_num++;
    return;
}

/* Murray added begin */
/* get maximum priority in priority queue; O(1) */
intern int pth_pqueue_maxprio(pth_pqueue_t *q)
{
    if (q == NULL || q->q_head == NULL)
        return 0;

    return q->q_head->prio;
}

/* flash the pqueue's the PTH_TASK_DONE.O(n) */
intern void pth_pqueue_flash(pth_pqueue_t *q, pth_time_t *now)
{
    pth_t   t;
    pth_time_t offset;

    if (q== NULL || now == NULL)
        return;

    t = q->q_head;
    do {
        if ((t->flag & PTH_TASK_DONE) && \
                (t->flag & PTH_TASK_RT)) {
            /* if deadline - now < period, 
             * then reset PTH_TASK_DONE */
            pth_time_set(&offset, &t->deadline);
            pth_time_sub(&offset, now);
            if (pth_time_cmp(&offset, &t->period) <= 0) 
                t->flag &= ~PTH_TASK_DONE;
        }
        t = t->q_next;
    } while (t != q->q_head);
}
/* Murray added end */

/* murray modified for edf */
/* remove thread with earliest deadline from priority queue; O(1) */
/* when key_load > 1, O(n) */
intern pth_t pth_pqueue_delmax(pth_pqueue_t *q, float work_load)
{
    pth_t t;

    if (q == NULL)
        return NULL;
    if (q->q_head == NULL)
        t = NULL;
    else if (q->q_head->q_next == q->q_head) {
        /* remove the last element and make queue empty */
        t = q->q_head;
        t->q_next = NULL;
        t->q_prev = NULL;
        q->q_head = NULL;
        q->q_num  = 0;
    }

    t = q->q_head;
    if (work_load > 1) {
        while (t->flag & PTH_TASK_HARD == 0 && t->q_next != q->q_head)
            t = t->q_next;

        if (t->q_next == q->q_head) {
            if (t->flag & PTH_TASK_HARD)
                return t;
        }
    } 

    /* remove head of queue */
    t->q_prev->q_next = t->q_next;
    t->q_next->q_prev = t->q_prev;
    q->q_head = t->q_next;
    q->q_num--;

    return t;
}

/* remove thread from priority queue; O(n) */
intern void pth_pqueue_delete(pth_pqueue_t *q, pth_t t)
{
    if (q == NULL)
        return;
    if (q->q_head == NULL)
        return;
    else if (q->q_head == t) {
        if (t->q_next == t) {
            /* remove the last element and make queue empty */
            t->q_next = NULL;
            t->q_prev = NULL;
            // t->q_prio = 0;
            q->q_head = NULL;
            q->q_num  = 0;
        }
        else {
            /* remove head of queue */
            t->q_prev->q_next = t->q_next;
            t->q_next->q_prev = t->q_prev;
            /* 
            t->q_next->q_prio = t->q_prio - t->q_next->q_prio;
            t->q_prio = 0;
            */
            q->q_head = t->q_next;
            q->q_num--;
        }
    }
    else {
        t->q_prev->q_next = t->q_next;
        t->q_next->q_prev = t->q_prev;
        /*
        if (t->q_next != q->q_head)
            t->q_next->q_prio += t->q_prio;
        t->q_prio = 0;
        */
        q->q_num--;
    }
    return;
}

/* determine priority required to favorite a thread; O(1) */
#if cpp
#define pth_pqueue_favorite_prio(q) \
    ((q)->q_head != NULL ? (q)->q_head->q_prio + 1 : PTH_PRIO_MAX)
#endif

/* move a thread inside queue to the top; O(n) */
intern int pth_pqueue_favorite(pth_pqueue_t *q, pth_t t)
{
    if (q == NULL)
        return FALSE;
    if (q->q_head == NULL || q->q_num == 0)
        return FALSE;
    /* element is already at top */
    if (q->q_num == 1)
        return TRUE;
    /* move to top */
    pth_pqueue_delete(q, t);
    pth_pqueue_insert(q, -1, t);
    return TRUE;
}

#if 0
/* increase priority of all(!) threads in queue; O(1) */
intern void pth_pqueue_increase(pth_pqueue_t *q)
{
    if (q == NULL)
        return;
    if (q->q_head == NULL)
        return;
    /* <grin> yes, that's all ;-) */
    q->q_head->q_prio += 1;
    return;
}
#endif

/* return number of elements in priority queue: O(1) */
#if cpp
#define pth_pqueue_elements(q) \
    ((q) == NULL ? (-1) : (q)->q_num)
#endif

/* walk to first thread in queue; O(1) */
#if cpp
#define pth_pqueue_head(q) \
    ((q) == NULL ? NULL : (q)->q_head)
#endif

/* walk to last thread in queue */
intern pth_t pth_pqueue_tail(pth_pqueue_t *q)
{
    if (q == NULL)
        return NULL;
    if (q->q_head == NULL)
        return NULL;
    return q->q_head->q_prev;
}

/* walk to next or previous thread in queue; O(1) */
intern pth_t pth_pqueue_walk(pth_pqueue_t *q, pth_t t, int direction)
{
    pth_t tn;

    if (q == NULL || t == NULL)
        return NULL;
    tn = NULL;
    if (direction == PTH_WALK_PREV) {
        if (t != q->q_head)
            tn = t->q_prev;
    }
    else if (direction == PTH_WALK_NEXT) {
        tn = t->q_next;
        if (tn == q->q_head)
            tn = NULL;
    }
    return tn;
}

/* check whether a thread is in a queue; O(n) */
intern int pth_pqueue_contains(pth_pqueue_t *q, pth_t t)
{
    pth_t tc;
    int found;

    found = FALSE;
    for (tc = pth_pqueue_head(q); tc != NULL;
         tc = pth_pqueue_walk(q, tc, PTH_WALK_NEXT)) {
        if (tc == t) {
            found = TRUE;
            break;
        }
    }
    return found;
}

/* murray added. get the nearest event timespot; */
intern long pth_pqueue_next_timespot(pth_pqueue_t *q, pth_t t, pth_time_t *now)
{
    pth_time_t tmp_t;

    if (q == NULL)
        return -1;

    if (q->q_head == NULL)
        return (t->flag & PTH_TASK_RT) ? \
            t->remain.tv_sec * 1000000 + t->remain.tv_usec:-1;

    /*
    c = q->q_head;
    while (((c->flag & PTH_TASK_DONE) == 0) && \
            (c->q_next != q->q_head))
        c = c->q_next;
    
    if (c->q_next == q->q_head)
        return ts;
    */

    pth_time_set(&tmp_t, &q->q_head->deadline);
    pth_time_sub(&tmp_t, now);
    if (pth_time_cmp(&tmp_t, &t->remain) > 0) 
        return t->remain.tv_sec * 1000000 + t->remain.tv_usec;
    else if (q->q_head->flag & PTH_TASK_DONE) {
        pth_time_set(&tmp_t, &q->q_head->deadline);
        pth_time_sub(&tmp_t, &q->q_head->period);
    }

    return tmp_t.tv_sec * 1000000 + tmp_t.tv_usec;
}

