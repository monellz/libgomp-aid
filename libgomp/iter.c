/* Copyright (C) 2005-2018 Free Software Foundation, Inc.
   Contributed by Richard Henderson <rth@redhat.com>.

   This file is part of the GNU Offloading and Multi Processing Library
   (libgomp).

   Libgomp is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   Libgomp is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */

/* This file contains routines for managing work-share iteration, both
   for loops and sections.  */

#include "libgomp.h"
#include <stdlib.h>
#include <sys/time.h>


#define AID_DEBUG

#ifdef AID_DEBUG
#include <stdio.h>
#define AID_LOG(format, ...) \
    do {    \
        fprintf(stdout, "[AID] " format "\n", ##__VA_ARGS__); \
    } while (0)
#else
#define AID_LOG(...) do {} while (0)
#endif

/* This function implements the STATIC scheduling method.  The caller should
   iterate *pstart <= x < *pend.  Return zero if there are more iterations
   to perform; nonzero if not.  Return less than 0 if this thread had
   received the absolutely last iteration.  */

int
gomp_iter_static_next (long *pstart, long *pend)
{
  struct gomp_thread *thr = gomp_thread ();
  struct gomp_team *team = thr->ts.team;
  struct gomp_work_share *ws = thr->ts.work_share;
  unsigned long nthreads = team ? team->nthreads : 1;

  if (thr->ts.static_trip == -1)
    return -1;

  /* Quick test for degenerate teams and orphaned constructs.  */
  if (nthreads == 1)
    {
      *pstart = ws->next;
      *pend = ws->end;
      thr->ts.static_trip = -1;
      return ws->next == ws->end;
    }

  /* We interpret chunk_size zero as "unspecified", which means that we
     should break up the iterations such that each thread makes only one
     trip through the outer loop.  */
  if (ws->chunk_size == 0)
    {
      unsigned long n, q, i, t;
      unsigned long s0, e0;
      long s, e;

      if (thr->ts.static_trip > 0)
	return 1;

      /* Compute the total number of iterations.  */
      s = ws->incr + (ws->incr > 0 ? -1 : 1);
      n = (ws->end - ws->next + s) / ws->incr;
      i = thr->ts.team_id;

      /* Compute the "zero-based" start and end points.  That is, as
         if the loop began at zero and incremented by one.  */
      q = n / nthreads;
      t = n % nthreads;
      if (i < t)
	{
	  t = 0;
	  q++;
	}
      s0 = q * i + t;
      e0 = s0 + q;

      /* Notice when no iterations allocated for this thread.  */
      if (s0 >= e0)
	{
	  thr->ts.static_trip = 1;
	  return 1;
	}

      /* Transform these to the actual start and end numbers.  */
      s = (long)s0 * ws->incr + ws->next;
      e = (long)e0 * ws->incr + ws->next;

      *pstart = s;
      *pend = e;
      thr->ts.static_trip = (e0 == n ? -1 : 1);
      return 0;
    }
  else
    {
      unsigned long n, s0, e0, i, c;
      long s, e;

      /* Otherwise, each thread gets exactly chunk_size iterations
	 (if available) each time through the loop.  */

      s = ws->incr + (ws->incr > 0 ? -1 : 1);
      n = (ws->end - ws->next + s) / ws->incr;
      i = thr->ts.team_id;
      c = ws->chunk_size;

      /* Initial guess is a C sized chunk positioned nthreads iterations
	 in, offset by our thread number.  */
      s0 = (thr->ts.static_trip * nthreads + i) * c;
      e0 = s0 + c;

      /* Detect overflow.  */
      if (s0 >= n)
	return 1;
      if (e0 > n)
	e0 = n;

      /* Transform these to the actual start and end numbers.  */
      s = (long)s0 * ws->incr + ws->next;
      e = (long)e0 * ws->incr + ws->next;

      *pstart = s;
      *pend = e;

      if (e0 == n)
	thr->ts.static_trip = -1;
      else
	thr->ts.static_trip++;
      return 0;
    }
}


/* This function implements the DYNAMIC scheduling method.  Arguments are
   as for gomp_iter_static_next.  This function must be called with ws->lock
   held.  */

bool
gomp_iter_dynamic_next_locked (long *pstart, long *pend)
{
  struct gomp_thread *thr = gomp_thread ();
  struct gomp_work_share *ws = thr->ts.work_share;
  long start, end, chunk, left;

  start = ws->next;
  if (start == ws->end)
    return false;

  chunk = ws->chunk_size;
  left = ws->end - start;
  if (ws->incr < 0)
    {
      if (chunk < left)
	chunk = left;
    }
  else
    {
      if (chunk > left)
	chunk = left;
    }
  end = start + chunk;

  ws->next = end;
  *pstart = start;
  *pend = end;
  return true;
}


#ifdef HAVE_SYNC_BUILTINS
/* Similar, but doesn't require the lock held, and uses compare-and-swap
   instead.  Note that the only memory value that changes is ws->next.  */

bool
gomp_iter_dynamic_next (long *pstart, long *pend)
{
  struct gomp_thread *thr = gomp_thread ();
  struct gomp_work_share *ws = thr->ts.work_share;
  long start, end, nend, chunk, incr;

  end = ws->end;
  incr = ws->incr;
  chunk = ws->chunk_size;

  if (__builtin_expect (ws->mode, 1))
    {
      long tmp = __sync_fetch_and_add (&ws->next, chunk);
      if (incr > 0)
	{
	  if (tmp >= end)
	    return false;
	  nend = tmp + chunk;
	  if (nend > end)
	    nend = end;
	  *pstart = tmp;
	  *pend = nend;
	  return true;
	}
      else
	{
	  if (tmp <= end)
	    return false;
	  nend = tmp + chunk;
	  if (nend < end)
	    nend = end;
	  *pstart = tmp;
	  *pend = nend;
	  return true;
	}
    }

  start = __atomic_load_n (&ws->next, MEMMODEL_RELAXED);
  while (1)
    {
      long left = end - start;
      long tmp;

      if (start == end)
	return false;

      if (incr < 0)
	{
	  if (chunk < left)
	    chunk = left;
	}
      else
	{
	  if (chunk > left)
	    chunk = left;
	}
      nend = start + chunk;

      tmp = __sync_val_compare_and_swap (&ws->next, start, nend);
      if (__builtin_expect (tmp == start, 1))
	break;

      start = tmp;
    }

  *pstart = start;
  *pend = nend;
  return true;
}


bool
gomp_iter_aid_static_next (long *pstart, long *pend)
{
  struct gomp_thread *thr = gomp_thread ();
  struct gomp_work_share *ws = thr->ts.work_share;
  long start, end, nend, chunk, incr;

  unsigned thread_id = thr->ts.team_id;
  unsigned nthreads = thr->ts.team->nthreads;
  // TODO: it should be more general
  bool on_big_core = thread_id >= nthreads / 2;

  end = ws->end;
  incr = ws->incr;
  chunk = ws->chunk_size;

  AID_LOG ("tid: %d, into gomp_iter_aid_static_next, ws->mode=%d", thread_id, ws->mode);
  AID_LOG ("\ttid: %d, config chunk:%ld, end:%ld, incr:%ld, next:%ld", thread_id, ws->chunk_size, ws->end, ws->incr, ws->next);

  #define STEAL_NEXT_BY(_chunk) \
    do { \
      long tmp = __sync_fetch_and_add (&ws->next, _chunk); \
      if (incr > 0) { \
        if (tmp >= end) return false; \
        nend = tmp + _chunk; \
        ws->aid_allocated_iter[thread_id] += _chunk; \
        if (nend > end) nend = end; \
        *pstart = tmp; \
        *pend = nend; \
        return true; \
      } else { \
        if (tmp <= end) return false; \
        nend = tmp + _chunk; \
        ws->aid_allocated_iter[thread_id] += _chunk; \
        if (nend < end) nend = end; \
        *pstart = tmp; \
        *pend = nend; \
        return true; \
      } \
    } while (0)

  #define AID_STATIC_NEXT_WITH(_k, _sf, _on_big_core) \
    do { \
      long _chunk = _on_big_core? _k * _sf: _k; \
      _chunk -= ws->aid_allocated_iter[thread_id];  \
      if (incr > 0) { \
        if (_chunk <= 0) return false; \
        AID_LOG ("tid (%d) allocated %ld iter, with %ld iter executed", thread_id, _chunk, ws->aid_allocated_iter[thread_id]); \
        long tmp = __sync_fetch_and_add(&ws->next, _chunk); \
        if (tmp >= end) return false; \
        ws->aid_allocated_iter[thread_id] += _chunk; \
        nend = tmp + _chunk; \
        if (nend > end) nend = end; \
        *pstart = tmp; \
        *pend = nend; \
        return true; \
      } else { \
        if (_chunk >= 0) return false; \
        AID_LOG ("tid (%d) allocated %ld iter, with %ld iter executed", thread_id, _chunk, ws->aid_allocated_iter[thread_id]); \
        long tmp = __sync_fetch_and_add(&ws->next, _chunk); \
        if (tmp <= end) return false; \
        ws->aid_allocated_iter[thread_id] += _chunk; \
        nend = tmp + _chunk; \
        if (nend < end) nend = end; \
        *pstart = tmp; \
        *pend = nend; \
        return true; \
      } \
    } while (0)

  if (__builtin_expect (ws->mode, 1)) {
    switch (ws->aid_states[thread_id]) {
      case AID_INIT: {
        ws->aid_states[thread_id] = AID_SAMPLING;

        // There might be some delay so the clock must be started after it
        long tmp = __sync_fetch_and_add(&ws->next, chunk);

        // start clock
        struct timeval tv;
        gettimeofday(&tv, NULL);
        ws->aid_consumed_time[thread_id] = tv.tv_sec * 1000000 + tv.tv_usec;

        if (incr > 0) {
          if (tmp >= end) return false;
          nend = tmp + chunk;
          ws->aid_allocated_iter[thread_id] += chunk; 
          if (nend > end) nend = end;
          *pstart = tmp;
          *pend = nend;
          return true;
        } else {
          if (tmp <= end) return false;
          nend = tmp + chunk;
          ws->aid_allocated_iter[thread_id] += chunk; 
          if (nend < end) nend = end;
          *pstart = tmp;
          *pend = nend;
          return true;
        }

        break;
      }
      case AID_SAMPLING: {
        // end clock
        struct timeval tv;
        gettimeofday(&tv, NULL);
        ws->aid_consumed_time[thread_id] = tv.tv_sec * 1000000 + tv.tv_usec - ws->aid_consumed_time[thread_id];
        unsigned cnt = __sync_add_and_fetch (&ws->aid_thread_sampling_completed, 1);
        if (cnt >= nthreads) {
          // The current thread is the last one which finish its sampling phase
          // It's responsible for calculating the speedup and then go into AID_RUNNING
          ws->aid_states[thread_id] = AID_RUNNING;

          // calculating speedup
          unsigned long bigcore_time = 0;
          unsigned long smallcore_time = 0;
          /* WARNING
            two assumption:
            1. the number of threads must be equal to the core number
            2. 0 - core_number // 2 is little core,  core_number // 2 - core_number is big core

            TODO: use an environment variable to set big/small cores
          */

          for (int i = 0; i < nthreads / 2; ++i) {
            smallcore_time += ws->aid_consumed_time[i];
          }
          for (int i = nthreads / 2; i < nthreads; ++i) {
            bigcore_time += ws->aid_consumed_time[i];
          }
          unsigned sf, k;
          sf = (bigcore_time + smallcore_time - 1) / smallcore_time;
          if (sf == 0) sf = 1;
          // TODO: it should be more general
          k = ws->aid_ni / ((sf + 1) * nthreads / 2);

          ws->aid_sf = sf;
          ws->aid_k = k;

          AID_LOG ("tid (%d) calculated sf = %u(small/big: %lu/%lu), k = %u with ni = %ld, nthreads = %d",
                  thread_id, sf, smallcore_time, bigcore_time, k, ws->aid_ni, nthreads);

          // statically allocate iterations based its core type
          AID_STATIC_NEXT_WITH(k, sf, on_big_core);
        } else {
          ws->aid_states[thread_id] = AID_WAITING;

          // steal CHUNK iterations
          STEAL_NEXT_BY(chunk);
        }
        break;
      }
      case AID_WAITING: {
        unsigned k = ws->aid_k;
        unsigned sf = ws->aid_sf;
        if (sf > 0 && k > 0) {
          // It means all threads have prepared for AID RUNNING
          ws->aid_states[thread_id] = AID_RUNNING;
          AID_STATIC_NEXT_WITH(k, sf, on_big_core);
        } else {
          // steal CHUNK iterations
          STEAL_NEXT_BY(chunk);
        }
        break;
      }
      case AID_RUNNING: {
        AID_LOG ("Warnning: tid (%d) into running, now it uses openmp dynamic", thread_id);
        STEAL_NEXT_BY(chunk);
        break;
      }
      default:
        gomp_fatal("Unknown aid state type");
    }
  }

  AID_LOG ("tid %d, Unimplemented for gomp_work_share->mode != 1, mode=%d\n", thread_id, ws->mode);
  exit(0);



  if (__builtin_expect (ws->mode, 1))
    {
      long tmp = __sync_fetch_and_add (&ws->next, chunk);
      if (incr > 0)
	{
	  if (tmp >= end)
	    return false;
	  nend = tmp + chunk;
	  if (nend > end)
	    nend = end;
	  *pstart = tmp;
	  *pend = nend;
	  return true;
	}
      else
	{
	  if (tmp <= end)
	    return false;
	  nend = tmp + chunk;
	  if (nend < end)
	    nend = end;
	  *pstart = tmp;
	  *pend = nend;
	  return true;
	}
    }

  gomp_fatal("unimplemented for gomp_work_share->mode != 1");

  start = __atomic_load_n (&ws->next, MEMMODEL_RELAXED);
  while (1)
    {
      long left = end - start;
      long tmp;

      if (start == end)
	return false;

      if (incr < 0)
	{
	  if (chunk < left)
	    chunk = left;
	}
      else
	{
	  if (chunk > left)
	    chunk = left;
	}
      nend = start + chunk;

      tmp = __sync_val_compare_and_swap (&ws->next, start, nend);
      if (__builtin_expect (tmp == start, 1))
	break;

      start = tmp;
    }

  *pstart = start;
  *pend = nend;
  return true;
}

#endif /* HAVE_SYNC_BUILTINS */


/* This function implements the GUIDED scheduling method.  Arguments are
   as for gomp_iter_static_next.  This function must be called with the
   work share lock held.  */

bool
gomp_iter_guided_next_locked (long *pstart, long *pend)
{
  struct gomp_thread *thr = gomp_thread ();
  struct gomp_work_share *ws = thr->ts.work_share;
  struct gomp_team *team = thr->ts.team;
  unsigned long nthreads = team ? team->nthreads : 1;
  unsigned long n, q;
  long start, end;

  if (ws->next == ws->end)
    return false;

  start = ws->next;
  n = (ws->end - start) / ws->incr;
  q = (n + nthreads - 1) / nthreads;

  if (q < ws->chunk_size)
    q = ws->chunk_size;
  if (q <= n)
    end = start + q * ws->incr;
  else
    end = ws->end;

  ws->next = end;
  *pstart = start;
  *pend = end;
  return true;
}

#ifdef HAVE_SYNC_BUILTINS
/* Similar, but doesn't require the lock held, and uses compare-and-swap
   instead.  Note that the only memory value that changes is ws->next.  */

bool
gomp_iter_guided_next (long *pstart, long *pend)
{
  struct gomp_thread *thr = gomp_thread ();
  struct gomp_work_share *ws = thr->ts.work_share;
  struct gomp_team *team = thr->ts.team;
  unsigned long nthreads = team ? team->nthreads : 1;
  long start, end, nend, incr;
  unsigned long chunk_size;

  start = __atomic_load_n (&ws->next, MEMMODEL_RELAXED);
  end = ws->end;
  incr = ws->incr;
  chunk_size = ws->chunk_size;

  while (1)
    {
      unsigned long n, q;
      long tmp;

      if (start == end)
	return false;

      n = (end - start) / incr;
      q = (n + nthreads - 1) / nthreads;

      if (q < chunk_size)
	q = chunk_size;
      if (__builtin_expect (q <= n, 1))
	nend = start + q * incr;
      else
	nend = end;

      tmp = __sync_val_compare_and_swap (&ws->next, start, nend);
      if (__builtin_expect (tmp == start, 1))
	break;

      start = tmp;
    }

  *pstart = start;
  *pend = nend;
  return true;
}
#endif /* HAVE_SYNC_BUILTINS */
