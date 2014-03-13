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
**  pth_time.c: Pth time calculations
*/
                             /* ``Real programmers confuse
                                  Christmas and Halloween
                                  because DEC 25 = OCT 31.''
                                             -- Unknown     */
#include "pth_p.h"

#if cpp
#define PTH_TIME_NOW  (pth_time_t *)(0)
#define PTH_TIME_ZERO &pth_time_zero
#define PTH_TIME(sec,usec) { sec, usec }
#define pth_time_equal(t1,t2) \
        (((t1).tv_sec == (t2).tv_sec) && ((t1).tv_usec == (t2).tv_usec))
#endif /* cpp */

/* murray added begin */
struct pth_tsc {
    unsigned int hi;
    unsigned int lo;
};
typedef struct tsc_time pth_tsc_t;

intern pth_tsc_t pth_tsc_zero = {0, 0};
/* murray added end */

/* a global variable holding a zero time */
intern pth_time_t pth_time_zero = { 0L, 0L };

/* sleep for a specified amount of microseconds */
intern void pth_time_usleep(unsigned long usec)
{
#ifdef HAVE_USLEEP
    usleep((unsigned int )usec);
#else
    struct timeval timeout;
    timeout.tv_sec  = usec / 1000000;
    timeout.tv_usec = usec - (1000000 * timeout.tv_sec);
    while (pth_sc(select)(1, NULL, NULL, NULL, &timeout) < 0 && errno == EINTR) ;
#endif
    return;
}

/* calculate: t1 = t2 */
#if cpp
#if defined(HAVE_GETTIMEOFDAY_ARGS1)
#define __gettimeofday(t) gettimeofday(t)
#else
#define __gettimeofday(t) gettimeofday(t, NULL)
#endif

/* murray added begin */
#define pth_tsc_set(c1,c2) \
    do { \
        if ((c2) == PTH_TIME_NOW) \
            __asm__ __volatile__("rdtsc":"=a"(c2->lo), "=d"(c2->hi)) \
        else { \
            (c1)->lo  = (c2)->lo; \
            (c1)->hi = (c2)->hi; \
        } \
    } while (0)


/* calculate c1 <=> c2 */
intern int pth_tsc_cmp(pth_tsc_t *c1, pth_tsc_t *c2)
{
    int rc;

    rc = c1->hi - c2->hi;
    
    return (rc == 0)?rc:(c1->low - c2->low);
}

/* calculate c1 = c1 - c2 */
#define pth_tsc_sub(c1,c2) \
    do { \
        (c1)->hi -= (c2)->hi; \
        if ((c1)->lo < (c2)->lo) { \
            (c1)->hi--; \
            (c1)->lo = ~((c2)->lo) + (c1)->lo + 1; \
        } \
        else \
            (c1)->lo -= (c2)->lo; \
    } while (0)

/* my CPU frequency is 2GHZ */
#define PTH_TSC_SEC_OFFSET  31


/* convert a tsc difference into a time struct */
intern void pth_time_c2t(pth_tsc_t *diff, struct timeval *t)
{
    t->tv_sec = ((diff->hi)<<1) + ((diff->lo)>>PTH_TSC_SEC_OFFSET);
    t->tv_usec = (((diff->lo) & 0x7FFFFFFF)>>1) / 1000;/* or can we just >>10? */
}


/* convert a time struct into a tsc difference */
intern void pth_Time_t2c(struct timeval *t, pth_tsc_t *diff)
{
    diff->lo = t->tv_usec*2000; /* *1000->nanosec, *2->tsc count, or can we just <<11? */
    if (t->tv_sec & 0x01)
        diff->lo |= 0x80000000;
    diff->hi = t->tv_sec>>1;
}


/* calculate the time that from c(tsc count) to now */
intern void pth_tsc_passingtime(pth_tsc_t *c, struct timeval *t)
{
    pth_tsc_t nowc = pth_tsc_zero;

    pth_tsc_set(&nowc, PTH_TIME_NOW);
    pth_tsc_sub(&nowc, c);
    pth_tsc_c2t(&nowc, t);
}
/* murray added end */

#define pth_time_set(t1,t2) \
    do { \
        if ((t2) == PTH_TIME_NOW) \
            __gettimeofday((t1)); \
        else { \
            (t1)->tv_sec  = (t2)->tv_sec; \
            (t1)->tv_usec = (t2)->tv_usec; \
        } \
    } while (0)
#if 0
#endif
#endif /* cpp */


/* time value constructor */
pth_time_t pth_time(long sec, long usec)
{
    pth_time_t tv;

    tv.tv_sec  = sec;
    tv.tv_usec = usec;
    return tv;
}

/* timeout value constructor */
pth_time_t pth_timeout(long sec, long usec)
{
    pth_time_t tv;
    pth_time_t tvd;

    pth_time_set(&tv, PTH_TIME_NOW);
    tvd.tv_sec  = sec;
    tvd.tv_usec = usec;
    pth_time_add(&tv, &tvd);
    return tv;
}

/* calculate: t1 <=> t2 */
intern int pth_time_cmp(pth_time_t *t1, pth_time_t *t2)
{
    int rc;

    rc = t1->tv_sec - t2->tv_sec;
    if (rc == 0)
         rc = t1->tv_usec - t2->tv_usec;
    return rc;
}

/* calculate: t1 = t1 + t2 */
#if cpp
#define pth_time_add(t1,t2) \
    (t1)->tv_sec  += (t2)->tv_sec; \
    (t1)->tv_usec += (t2)->tv_usec; \
    if ((t1)->tv_usec > 1000000) { \
        (t1)->tv_sec  += 1; \
        (t1)->tv_usec -= 1000000; \
    }
#endif

/* calculate: t1 = t1 - t2 */
#if cpp
#define pth_time_sub(t1,t2) \
    (t1)->tv_sec  -= (t2)->tv_sec; \
    (t1)->tv_usec -= (t2)->tv_usec; \
    if ((t1)->tv_usec < 0) { \
        (t1)->tv_sec  -= 1; \
        (t1)->tv_usec += 1000000; \
    }
#endif

/* calculate: t1 = t1 / n */
intern void pth_time_div(pth_time_t *t1, int n)
{
    long q, r;

    q = (t1->tv_sec / n);
    r = (((t1->tv_sec % n) * 1000000) / n) + (t1->tv_usec / n);
    if (r > 1000000) {
        q += 1;
        r -= 1000000;
    }
    t1->tv_sec  = q;
    t1->tv_usec = r;
    return;
}

/* calculate: t1 = t1 * n */
intern void pth_time_mul(pth_time_t *t1, int n)
{
    t1->tv_sec  *= n;
    t1->tv_usec *= n;
    t1->tv_sec  += (t1->tv_usec / 1000000);
    t1->tv_usec  = (t1->tv_usec % 1000000);
    return;
}

/* convert a time structure into a double value */
intern double pth_time_t2d(pth_time_t *t)
{
    double d;

    d = ((double)t->tv_sec*1000000 + (double)t->tv_usec) / 1000000;
    return d;
}

/* convert a time structure into a integer value */
intern int pth_time_t2i(pth_time_t *t)
{
    int i;

    i = (t->tv_sec*1000000 + t->tv_usec) / 1000000;
    return i;
}

/* check whether time is positive */
intern int pth_time_pos(pth_time_t *t)
{
    if (t->tv_sec > 0 && t->tv_usec > 0)
        return 1;
    else
        return 0;
}

