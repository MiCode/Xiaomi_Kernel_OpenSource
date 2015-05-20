/*
  Copyright (C) 2010-2014 Intel Corporation.  All Rights Reserved.

  This file is part of SEP Development Kit

  SEP Development Kit is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation.

  SEP Development Kit is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SEP Development Kit; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

  As a special exception, you may use this file as part of a free software
  library without restriction.  Specifically, if other files instantiate
  templates or use macros or inline functions from this file, or you compile
  this file and link it with other files to produce an executable, this
  file does not by itself cause the resulting executable to be covered by
  the GNU General Public License.  This exception does not however
  invalidate any other reasons why the executable file might be covered by
  the GNU General Public License.
*/
#ifndef _VTSS_TIME_H_
#define _VTSS_TIME_H_

#include "vtss_autoconf.h"

#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>

static inline unsigned long long vtss_freq_cpu(void) __attribute__ ((always_inline));
static inline unsigned long long vtss_freq_cpu(void)
{
    return (tsc_khz * 1000ULL);
}

static inline unsigned long long vtss_freq_real(void) __attribute__ ((always_inline));
static inline unsigned long long vtss_freq_real(void)
{
    return vtss_time_source ? (tsc_khz * 1000ULL) : 1000000000ULL; /* 1ns */
}

static inline unsigned long long vtss_time_cpu(void) __attribute__ ((always_inline));
static inline unsigned long long vtss_time_cpu(void)
{
    return (unsigned long long)get_cycles();
}

static inline unsigned long long vtss_time_real(void) __attribute__ ((always_inline));
static inline unsigned long long vtss_time_real(void)
{
    if (!vtss_time_source) {
        struct timespec now;
        getrawmonotonic(&now); /* getnstimeofday(&now); */
        return (unsigned long long)timespec_to_ns(&now);
    } else
        return (unsigned long long)get_cycles();
}

static inline void vtss_time_get_sync(unsigned long long* ptsc, unsigned long long* preal) __attribute__ ((always_inline));
static inline void vtss_time_get_sync(unsigned long long* ptsc, unsigned long long* preal)
{
    unsigned long long tsc = vtss_time_cpu();

    if (!vtss_time_source) {
        struct timespec now1, now2;
        getrawmonotonic(&now1);
        rdtsc_barrier();
        getrawmonotonic(&now2);
        *ptsc  = (tsc + vtss_time_cpu()) / 2;
        *preal = (timespec_to_ns(&now1) + timespec_to_ns(&now2)) / 2;
    } else
        *ptsc = *preal = tsc;
}

#endif /* _VTSS_TIME_H_ */
