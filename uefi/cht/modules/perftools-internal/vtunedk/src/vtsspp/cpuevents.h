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
#ifndef _VTSS_CPUEVENTS_H_
#define _VTSS_CPUEVENTS_H_

#include "vtss_autoconf.h"
#include "vtsscfg.h"          /* for cpuevent_cfg_v1_t */

#include <linux/types.h>        /* for size_t */

/*
 * Event descriptors
 */
struct _cpuevent_t;

typedef struct
{
    void (*start)           (struct _cpuevent_t*);
    void (*stop)            (struct _cpuevent_t*);
    void (*read)            (struct _cpuevent_t*);
    void (*freeze)          (struct _cpuevent_t*);
    void (*restart)         (struct _cpuevent_t*);
    void (*freeze_read)     (struct _cpuevent_t*);
    int  (*overflowed)      (struct _cpuevent_t*);
    long long (*convert)    (struct _cpuevent_t*);
    void (*overflow_update) (struct _cpuevent_t*);
    void (*update_restart)  (struct _cpuevent_t*);
    int  (*select_muxgroup) (struct _cpuevent_t*);
} cpuevent_i;

typedef struct _cpuevent_t
{
    /// counting support
    long long tmp;
    long long count;
    long long frozen_count;
    long long sampled_count;
    long long slave_interval;

    /// virtual function table
    cpuevent_i *vft;

    /// monitoring parameters
    int valid;
    int interval;
    int modifier;
#if 0
    int chain_idx;  /// position within the current event chain 
                    /// (to share counters automatically)

    /// program state determination data
    int trend;
    int trigger_mode;
    int state_idx;
    long long time[2][2];
    long long flux[2];

    /// debug exception control
    int dbg_samples;
    int dbg_samples_orig;
#endif

    /// multiplexion algorithm data
    long long muxchange_time;
    int muxchange_alt;
    int mux_idx;
    int mux_cnt;
    int mux_grp;
    int mux_alg;
    int mux_arg;

    /// processor specific registers/masks
    union
    {
        struct
        {
            int escr0;
            int escr0_mask;
            int escr1;
            int escr1_mask;
            int cccr0;
            int cccr0_mask;
            int cccr1;
            int cccr1_mask;
            int counter0;
            int counter1;
            int escr0e;
            int escr0e_mask;
            int escr1e;
            int escr1e_mask;
        };
        struct
        {
            int selmsr;
            int cntmsr;
            int selmsk;
            int extmsr;
            long long extmsk;
        };
        int opaque[0x20];
    };
} cpuevent_t;

typedef struct
{
    union
    {
        struct
        {
            unsigned short type;
            unsigned short subtype;
        };
        unsigned int event_id;
    };

    cpuevent_i *vft;

    char *name;
    char *desc;

    int modifier;

    /// processor specific registers/masks
    union
    {
        struct
        {
            int escr0;
            int escr0_mask;
            int escr1;
            int escr1_mask;
            int cccr0;
            int cccr0_mask;
            int cccr1;
            int cccr1_mask;
            int counter0;
            int counter1;
            int escr0e;
            int escr0e_mask;
            int escr1e;
            int escr1e_mask;
        };
        struct
        {
            int selmsr;
            int cntmsr;
            int selmsk;
            int extmsr;
            long long extmsk;
        };
        int opaque[0x20];
    };
} cpuevent_desc_t;

extern cpuevent_desc_t cpuevent_desc[];

/// system events types (fake)
typedef enum
{
    vtss_sysevent_sync_cs = 0,
    vtss_sysevent_preempt_cs,
    vtss_sysevent_wait_time,
    vtss_sysevent_inactive_time,
    vtss_sysevent_idle_time,
    vtss_sysevent_idle_wakeup,
    vtss_sysevent_idle_c3,
    vtss_sysevent_idle_c6,
    vtss_sysevent_idle_c7,
    vtss_sysevent_energy_core,
    vtss_sysevent_energy_gfx,
    vtss_sysevent_energy_pack,
    vtss_sysevent_energy_dram,
    vtss_sysevent_energy_soc,
#ifdef VTSS_SYSCALL_TRACE
    vtss_sysevent_syscall,
    vtss_sysevent_syscall_time,
#endif
    vtss_sysevent_end
} sysevent_e;

extern sysevent_e sysevent_type[];

typedef struct
{
    char* name;
    char* desc;
} sysevent_desc_t;

extern sysevent_desc_t sysevent_desc[];

int  vtss_cpuevents_init_pmu(int defsav);
void vtss_cpuevents_fini_pmu(void);
int  vtss_cpuevents_init(void);
void vtss_cpuevents_fini(void);

/// PMU and system events configuration
void vtss_cpuevents_reqcfg_default(int need_clear, int defsav);
void vtss_sysevents_reqcfg_append(void);

/// PMU event handling
void vtss_cpuevents_enable(void);
void vtss_cpuevents_stop(void);
void vtss_cpuevents_freeze(void);
void vtss_cpuevents_upload(cpuevent_t* cpuevent_chain, cpuevent_cfg_v1_t* cpuevent_cfg, int count);
void vtss_cpuevents_sample(cpuevent_t* cpuevent_chain);
void vtss_cpuevents_restart(cpuevent_t* cpuevent_chain, int flag);
void vtss_cpuevents_quantum_border(cpuevent_t* cpuevent_chain, int flag);

#endif /* _VTSS_CPUEVENTS_H_ */
