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
#include "vtss_config.h"
#include "dsa.h"
#include "globals.h"

#include <linux/percpu.h>

#define DS_AREA_MSR 0x0600

#ifdef DEFINE_PER_CPU_SHARED_ALIGNED
static DEFINE_PER_CPU_SHARED_ALIGNED(unsigned long long, vtss_dsa_cpu_msr);
#else
static DEFINE_PER_CPU(unsigned long long, vtss_dsa_cpu_msr);
#endif
static DEFINE_PER_CPU_SHARED_ALIGNED(vtss_dsa_t, vtss_dsa_per_cpu);

vtss_dsa_t* vtss_dsa_get(int cpu)
{
    return &per_cpu(vtss_dsa_per_cpu, cpu);
}

void vtss_dsa_init_cpu(void)
{
    if (hardcfg.family == 0x06 || hardcfg.family == 0x0f) {
        vtss_dsa_t *dsa = &__get_cpu_var(vtss_dsa_per_cpu);
        if (IS_DSA_64ON32) {
            dsa->v32.reserved[0] = dsa->v32.reserved[1] = NULL;
            dsa->v32.reserved[2] = dsa->v32.reserved[3] = NULL;
        } else {
            dsa->v64.reserved[0] = dsa->v64.reserved[1] = NULL;
        }
        wrmsrl(DS_AREA_MSR, (size_t)dsa);
    }
}

static void vtss_dsa_on_each_cpu_init(void* ctx)
{
    if (hardcfg.family == 0x06 || hardcfg.family == 0x0f) {
        rdmsrl(DS_AREA_MSR, __get_cpu_var(vtss_dsa_cpu_msr));
    }
}

static void vtss_dsa_on_each_cpu_fini(void* ctx)
{
    if (hardcfg.family == 0x06 || hardcfg.family == 0x0f) {
        wrmsrl(DS_AREA_MSR, __get_cpu_var(vtss_dsa_cpu_msr));
    }
}

int vtss_dsa_init(void)
{
    on_each_cpu(vtss_dsa_on_each_cpu_init, NULL, SMP_CALL_FUNCTION_ARGS);
    return 0;
}

void vtss_dsa_fini(void)
{
    on_each_cpu(vtss_dsa_on_each_cpu_fini, NULL, SMP_CALL_FUNCTION_ARGS);
}
