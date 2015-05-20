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
#include "cpuevents.h"
#include "globals.h"
#include "collector.h"
#include "apic.h"               /* for CPU_PERF_VECTOR */
#include "time.h"

#include <linux/linkage.h>      /* for asmlinkage */
#include <linux/interrupt.h>
#include <asm/desc.h>           /* for gate_desc  */
#include <asm/uaccess.h>

/*
 * PMU macro definitions
 */
#define CPU_EVENTS_SUPPORTED 3
#define CPU_CLKEVT_THRESHOLD 5000LL
#define CPU_EVTCNT_THRESHOLD 0x80000000LL

#define DEBUGCTL_MSR             0x1d9
#define MSR_PERF_GLOBAL_OVF_CTRL 0x390

/// P6 MSRs
#define MSR_PERF_GLOBAL_CTRL 0x38f
#define IA32_FIXED_CTR_CTRL  0x38d

#define IA32_FIXED_CTR0      0x309 //instruction ret counter
      //IA32_FIXED_CTR0+1)           core counter
      //IA32_FIXED_CTR0+2            ref counter

#define IA32_PERFEVTSEL0     0x186
#define IA32_PMC0            0x0c1

#define IA32_PERF_GLOBAL_STATUS     0x38e

/// SNB power MSRs
#define VTSS_MSR_PKG_ENERGY_STATUS  0x611
#define VTSS_MSR_PP0_ENERGY_STATUS  0x639
#define VTSS_MSR_PP1_ENERGY_STATUS  0x641    /// 06_2a
#define VTSS_MSR_DRAM_ENERGY_STATUS 0x619    /// 06_2d

/// NHM/SNB C-state residency MSRs
#define VTSS_MSR_CORE_C3_RESIDENCY 0x3fc
#define VTSS_MSR_CORE_C6_RESIDENCY 0x3fd
#define VTSS_MSR_CORE_C7_RESIDENCY 0x3fe ///SNB

/* Knight family, KNC */
#define KNX_CORE_PMC0                   0x20
#define KNX_CORE_PMC1                   0x21
#define KNX_CORE_PERFEVTSEL0            0x28
#define KNX_CORE_PERFEVTSEL1            0x29
#define KNC_PERF_SPFLT_CTRL             0x2C         // KNC only
#define KNX_CORE_PERF_GLOBAL_STATUS     0x2D
#define KNX_CORE_PERF_GLOBAL_OVF_CTRL   0x2E
#define KNX_CORE_PERF_GLOBAL_CTRL       0x2F

/**
 * Globals for CPU Monitoring Functionality
 */
static int pmu_counter_no    = 0;
static int pmu_counter_width = 0;
static unsigned long long pmu_counter_width_mask = 0x000000ffffffffffULL;

static int pmu_fixed_counter_no    = 0;
static int pmu_fixed_counter_width = 0;
static unsigned long long pmu_fixed_counter_width_mask = 0x000000ffffffffffULL;

/// event descriptors
cpuevent_desc_t cpuevent_desc[CPU_EVENTS_SUPPORTED];
sysevent_desc_t sysevent_desc[] = {
    {"Synchronization Context Switches", "Thread being swapped out due to contention on a synchronization object"},
    {"Preemption Context Switches", "Thread being preempted by the OS scheduler"},
    {"Wait Time", "Time while the thread is waiting on a synchronization object"},
    {"Inactive Time", "Time while the thread is preempted and resides in the ready queue"},
    {"Idle Time", "Time while no other thread was active before activating the current thread"},
    {"Idle Wakeup", "Thread waking up the system from idleness"},
    {"C3 Residency", "Time in low power sleep mode with all but the shared cache flushed"},
    {"C6 Residency", "Time in low power sleep mode with all caches flushed"},
    {"C7 Residency", "Time in low power sleep mode with all caches flushed and powered off"},
    {"Energy Core", "Energy (uJoules) consumed by the processor core"},
    {"Energy GFX", "Energy (uJoules) consumed by the uncore graphics"},
    {"Energy Pack", "Energy (uJoules) consumed by the processor package"},
    {"Energy DRAM", "Energy (uJoules) consumed by the memory"},
    {"Charge SoC", "Charge (Coulombs) consumed by the system"},
#ifdef VTSS_SYSCALL_TRACE
    {"Syscalls", "The number of calls to OS functions"},
    {"Syscalls Time", "Time spent in system calls"},
#endif
    {NULL, NULL}
};

extern void vtss_perfvec_handler(void);

void vtss_cpuevents_enable(void)
{
//    printk("cpu events enable\n");
    vtss_pmi_enable();
    /* enable counters globally (required for some Core2 & Core i7 systems) */
    if (hardcfg.family == 0x06 && hardcfg.model >= 0x0f) {
        unsigned long long mask = (((1ULL << pmu_fixed_counter_no) - 1) << 32) | ((1ULL << pmu_counter_no) - 1);

        TRACE("MSR(0x%x)<=0x%llx", MSR_PERF_GLOBAL_CTRL, mask);
        wrmsrl(MSR_PERF_GLOBAL_CTRL, mask);
        mask |= 3ULL << 62;
        TRACE("MSR(0x%x)<=0x%llx", MSR_PERF_GLOBAL_OVF_CTRL, mask);
        wrmsrl(MSR_PERF_GLOBAL_OVF_CTRL, mask);
    } else if (hardcfg.family == 0x0b) { // KNX_CORE_FAMILY
        unsigned long long mask = (1ULL << pmu_counter_no) - 1;

        TRACE("MSR(0x%x)<=0x%llx", KNX_CORE_PERF_GLOBAL_CTRL, mask);
        wrmsrl(KNX_CORE_PERF_GLOBAL_CTRL, mask);
        if (hardcfg.model == 0x01) { // KNC Only
            TRACE("MSR(0x%x)<=0x%llx", KNX_CORE_PERF_GLOBAL_OVF_CTRL, mask);
            wrmsrl(KNX_CORE_PERF_GLOBAL_OVF_CTRL, mask);
            mask |= 1ULL << 63;
            TRACE("MSR(0x%x)<=0x%llx", KNC_PERF_SPFLT_CTRL, mask);
            wrmsrl(KNC_PERF_SPFLT_CTRL, mask);
        }
    }
}

static cpuevent_t cpuevent; /*< NOTE: Need  for offset calculation in _stop()/_freeze() */

void vtss_cpuevents_stop(void)
{
    int offset = (int)((char*)&cpuevent.opaque - (char*)&cpuevent);

    if (cpuevent_desc[0].vft) {
        cpuevent_desc[0].vft->stop((cpuevent_t*)((char*)&(cpuevent_desc[0].opaque) - offset));
    }
}

void vtss_cpuevents_freeze(void)
{
    int offset = (int)((char*)&cpuevent.opaque - (char*)&cpuevent);

    if (cpuevent_desc[0].vft) {
        cpuevent_desc[0].vft->freeze((cpuevent_t*)((char*)&cpuevent_desc[0].opaque - offset));
    }
}

#include "cpuevents_p6.c"
#include "cpuevents_knx.c"
#include "cpuevents_sys.c"

void vtss_cpuevents_reqcfg_default(int need_clear, int defsav)
{
    int i, len0, len1;
    int mux_cnt = 0;
    int namespace_size = 0;

    INFO("reqcfg.cpuevent_count_v1 = %d", (int)reqcfg.cpuevent_count_v1);
    if (need_clear) {
        memset(&reqcfg, 0, sizeof(process_cfg_t));
        reqcfg.trace_cfg.trace_flags =  VTSS_CFGTRACE_CTX    | VTSS_CFGTRACE_CPUEV   |
                                        VTSS_CFGTRACE_SWCFG  | VTSS_CFGTRACE_HWCFG   |
                                        VTSS_CFGTRACE_SAMPLE | VTSS_CFGTRACE_OSEV    |
                                        VTSS_CFGTRACE_MODULE | VTSS_CFGTRACE_PROCTHR |
                                        VTSS_CFGTRACE_STACKS | VTSS_CFGTRACE_TREE;
        TRACE("trace_flags=0x%0X (%u)", reqcfg.trace_cfg.trace_flags, reqcfg.trace_cfg.trace_flags);
    }
    for (i = 0; i < CPU_EVENTS_SUPPORTED; i++) {
        if (i > 1 && hardcfg.family != 0x06)
            break;

        len0 = (int)strlen(cpuevent_desc[i].name)+1;
        len1 = (int)strlen(cpuevent_desc[i].desc)+1;

        if (namespace_size + len0 + len1 >= VTSS_CFG_SPACE_SIZE * 16)
            break;

        TRACE("Add cpuevent[%02d]: '%s' into mux_grp=%d", reqcfg.cpuevent_count_v1, cpuevent_desc[i].name, mux_cnt);
        /// copy event name
        memcpy(&reqcfg.cpuevent_namespace_v1[namespace_size], cpuevent_desc[i].name, len0);
        /// adjust event record
        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].name_off = (int)((size_t)&reqcfg.cpuevent_namespace_v1[namespace_size] - (size_t)&reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1]);
        /// adjust namespace size
        namespace_size += len0;
        /// copy event description
        memcpy(&reqcfg.cpuevent_namespace_v1[namespace_size], cpuevent_desc[i].desc, len1);
        /// adjust event record
        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].desc_off = (int)((size_t)&reqcfg.cpuevent_namespace_v1[namespace_size] - (size_t)&reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1]);
        /// adjust namespace size
        namespace_size += len1;

        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].name_len = len0;
        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].desc_len = len1;

        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].event_id = i;
        if (defsav) {
//            printk("in pmu init: defsav = %d\n", defsav);
            reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].interval = defsav;
        } else if (hardcfg.family == 0x06) { // P6
            reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].interval = 2000000;
        } else if (hardcfg.family == 0x0b) { // KNX_CORE_FAMILY
            reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].interval = 20000000;
        } else {
            reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].interval = 10000000;
        }
        printk("in pmu init: defsav = %d\n", reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].interval);
        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].mux_grp  = mux_cnt;
        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].mux_alg  = VTSS_CFGMUX_SEQ;
        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].mux_arg  = 1;

        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].selmsr.idx = cpuevent_desc[i].selmsr;
        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].selmsr.val = cpuevent_desc[i].selmsk;
        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].selmsr.msk = 0;
        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].cntmsr.idx = cpuevent_desc[i].cntmsr;
        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].cntmsr.val = 0;
        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].cntmsr.msk = 0;
        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].extmsr.idx = cpuevent_desc[i].extmsr;
        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].extmsr.val = cpuevent_desc[i].extmsk;
        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].extmsr.msk = 0;

        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].reqtype = VTSS_CFGREQ_CPUEVENT_V1;
        reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].reqsize = sizeof(cpuevent_cfg_v1_t) + len0 + len1;
        reqcfg.cpuevent_count_v1++;
    }
}

void vtss_sysevents_reqcfg_append(void)
{
    int i, j;
    int mux_grp = 0;
    int namespace_size = 0;

    const int idle_idx = 2;
    const int idle_last = 8;
    const int active_idx = 9;
    const int active_last = 13;
    int sys_event_idx = 2;

    INFO("reqcfg.cpuevent_count_v1 = %d", (int)reqcfg.cpuevent_count_v1);
    /* Find out the count of mux groups and namespace size */
    for (i = 0; i < reqcfg.cpuevent_count_v1; i++) {
        mux_grp = (mux_grp < reqcfg.cpuevent_cfg_v1[i].mux_grp) ? reqcfg.cpuevent_cfg_v1[i].mux_grp : mux_grp;
        namespace_size += reqcfg.cpuevent_cfg_v1[i].name_len + reqcfg.cpuevent_cfg_v1[i].desc_len;
    }
    /* insert system event records (w/names) into each mux_grp */
    for (i = sys_event_idx; i < vtss_sysevent_end && reqcfg.cpuevent_count_v1 < VTSS_CFG_CHAIN_SIZE; i++) {
        if (sysevent_type[i] == vtss_sysevent_end) {
            /* skip events that are not supported on this architecture */
            continue;
        }
        if (i >= idle_idx && i <= idle_last && (!( reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_PWRIDLE)))
        {
            // idle power not required
            continue;
        }
        if (i >= active_idx && i <= active_last && (!( reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_PWRACT)))
        {
            // active power not required
            continue;
        }
        for (j = 0; j <= mux_grp && reqcfg.cpuevent_count_v1 < VTSS_CFG_CHAIN_SIZE; j++) {
            int len0 = (int)strlen(sysevent_desc[i].name)+1;
            int len1 = (int)strlen(sysevent_desc[i].desc)+1;

            if (namespace_size + len0 + len1 >= VTSS_CFG_SPACE_SIZE * 16) {
                i = vtss_sysevent_end;
                break;
            }

            TRACE("Add sysevent[%02d]: '%s' into mux_grp=%d of %d", reqcfg.cpuevent_count_v1, sysevent_desc[i].name, j, mux_grp);
            /// copy event name
            memcpy(&reqcfg.cpuevent_namespace_v1[namespace_size], sysevent_desc[i].name, len0);
            /// adjust event record
            reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].name_off = (int)((size_t)&reqcfg.cpuevent_namespace_v1[namespace_size] - (size_t)&reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1]);
            /// adjust namespace size
            namespace_size += len0;
            /// copy event description
            memcpy(&reqcfg.cpuevent_namespace_v1[namespace_size], sysevent_desc[i].desc, len1);
            /// adjust event record
            reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].desc_off = (int)((size_t)&reqcfg.cpuevent_namespace_v1[namespace_size] - (size_t)&reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1]);
            /// adjust namespace size
            namespace_size += len1;

            /// copy event record
            reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].name_len = len0;
            reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].desc_len = len1;
            reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].mux_grp  = j;
            reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].event_id = i + VTSS_CFG_CHAIN_SIZE;
            reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].interval = 0;

            reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].reqtype = VTSS_CFGREQ_CPUEVENT_V1;
            reqcfg.cpuevent_cfg_v1[reqcfg.cpuevent_count_v1].reqsize = sizeof(cpuevent_cfg_v1_t) + len0 + len1;
            reqcfg.cpuevent_count_v1++;
        }
    }
}

// called from process_init() to form a common event chain from the configuration records
void vtss_cpuevents_upload(cpuevent_t* cpuevent_chain, cpuevent_cfg_v1_t* cpuevent_cfg, int count)
{
    int i = 0;
    int j = 0;
    int mux_cnt = 0;
    int fixed_cnt[3];
    int fixed_cnt_slave=0;
    fixed_cnt[0]=fixed_cnt[1]=fixed_cnt[2]=-1;
    INFO("reqcfg.cpuevent_count_v1 = %d", (int)reqcfg.cpuevent_count_v1);
    for (i = 0; i < count; i++) {
        cpuevent_chain[i].valid = 1;
        if (cpuevent_cfg[i].event_id >= VTSS_CFG_CHAIN_SIZE) {
            /// fake sysevents
            cpuevent_chain[i].vft      = &vft_sys;
            cpuevent_chain[i].interval = sysevent_type[cpuevent_cfg[i].event_id - VTSS_CFG_CHAIN_SIZE];
            cpuevent_chain[i].modifier = 0;
        } else {
            cpuevent_chain[i].vft      = cpuevent_desc[0].vft;
            cpuevent_chain[i].interval = cpuevent_cfg[i].interval;
            cpuevent_chain[i].slave_interval = 0;

            /// copy MSRs
            cpuevent_chain[i].selmsr = cpuevent_cfg[i].selmsr.idx;
            cpuevent_chain[i].selmsk = cpuevent_cfg[i].selmsr.val;
            cpuevent_chain[i].cntmsr = cpuevent_cfg[i].cntmsr.idx;
            cpuevent_chain[i].extmsr = cpuevent_cfg[i].extmsr.idx;
            cpuevent_chain[i].extmsk = cpuevent_cfg[i].extmsr.val;

            if (cpuevent_cfg[i].name_len) {
                /* replace BR_INST_RETIRED.NEAR_CALL_R3_PS event */
                if (!memcmp(((char*)&cpuevent_cfg[i] + cpuevent_cfg[i].name_off),
                             "BR_INST_RETIRED.NEAR_CALL_R3_PS", cpuevent_cfg[i].name_len))
                {
                    cpuevent_cfg[i].name_len = 11; /* strlen("Call Count") + '\0' */
                    memcpy(((char*)&cpuevent_cfg[i] + cpuevent_cfg[i].name_off),
                           "Call Count", cpuevent_cfg[i].name_len);
                }
            }
            /// correct the sampling interval if not setup explicitly
            if (!cpuevent_chain[i].interval && cpuevent_cfg[i].cntmsr.val) {
                if ((cpuevent_chain[i].interval = -(int)(cpuevent_cfg[i].cntmsr.val | 0xffffffff00000000ULL)) < CPU_CLKEVT_THRESHOLD) {
                    cpuevent_chain[i].interval = CPU_CLKEVT_THRESHOLD * 400;
                }
                cpuevent_cfg[i].interval = cpuevent_chain[i].interval;
            }
            /// set up counter offset for fixed events
            if (hardcfg.family == 0x06 && cpuevent_cfg[i].selmsr.idx == IA32_FIXED_CTR_CTRL) {
                if (cpuevent_cfg[i].cntmsr.idx - IA32_FIXED_CTR0 < 3 && cpuevent_cfg[i].cntmsr.idx - IA32_FIXED_CTR0 >= 0)
                {
                    fixed_cnt[cpuevent_cfg[i].cntmsr.idx - IA32_FIXED_CTR0]=i;
                }
                /// form the modifier to enable correct masking of control MSR in vft->restart()
                cpuevent_chain[i].modifier = (int)((cpuevent_cfg[i].selmsr.val >>
                                                   (4 * (cpuevent_cfg[i].cntmsr.idx - IA32_FIXED_CTR0))) << 16);
                ((event_modifier_t*)&cpuevent_chain[i].modifier)->cnto = cpuevent_cfg[i].cntmsr.idx - IA32_FIXED_CTR0;
            } else {
                cpuevent_chain[i].modifier = (int)(cpuevent_cfg[i].selmsr.val & VTSS_EVMOD_ALL);
            }
            if (hardcfg.family == 0x0b) { // KNX_CORE_FAMILY
                /// set up counter events as slaves (that follow leading events)
                if (cpuevent_cfg[i].cntmsr.idx > KNX_CORE_PMC0) {
                    cpuevent_chain[i].slave_interval = cpuevent_cfg[i].interval;
                    cpuevent_chain[i].interval = 0;
                }
            }
        }
        cpuevent_chain[i].mux_grp = cpuevent_cfg[i].mux_grp;
        cpuevent_chain[i].mux_alg = cpuevent_cfg[i].mux_alg;

        /// force the driver to change MUX group at every sample
        switch (cpuevent_chain[i].mux_alg) {
        case VTSS_CFGMUX_SEQ:
        case VTSS_CFGMUX_MST:
        case VTSS_CFGMUX_SLV:
            cpuevent_chain[i].mux_arg = 1;
            break;
        default:
            cpuevent_chain[i].mux_arg = cpuevent_cfg[i].mux_arg;
        }

        mux_cnt = (mux_cnt < cpuevent_chain[i].mux_grp) ? cpuevent_chain[i].mux_grp : mux_cnt;

        TRACE("Upload event[%02d]: '%s' .modifier=%x .selmsr=%x .cntmsr=%x .selmsk=%x",
              i, ((char*)&cpuevent_cfg[i] + cpuevent_cfg[i].name_off),
              cpuevent_chain[i].modifier, cpuevent_chain[i].selmsr,
              cpuevent_chain[i].cntmsr,   cpuevent_chain[i].selmsk
        );
    }

    for (j = 2; j>=0; j--)
    {
        if (fixed_cnt[j] == -1) continue;
        if (fixed_cnt_slave == 0)
        {
            fixed_cnt_slave = 1;
            continue;
        }
        /// set up fixed counter events as slaves (that follow leading events)
        cpuevent_chain[fixed_cnt[j]].slave_interval = cpuevent_cfg[fixed_cnt[j]].interval;
        cpuevent_chain[fixed_cnt[j]].interval = 0;
    }
    if (i) {
        cpuevent_chain[0].mux_cnt = mux_cnt;
        if (hardcfg.family == 0x06) { // P6
            /* force LBR collection to correct sampled IPs */
            reqcfg.trace_cfg.trace_flags |= VTSS_CFGTRACE_LASTBR;
        }
    }
}

/// TODO: generate correct records for system-wide sampling/counting
/// called from swap_in(), pmi_handler(), and vtssreq_trigger()
/// to read event values and form a sample record
void vtss_cpuevents_sample(cpuevent_t* cpuevent_chain)
{
    int i;

    if (unlikely(!cpuevent_chain)){
        ERROR("CPU event chain is empty!!!");
        return;
    }

    /// select between thread-specific and per-processor chains (system-wide)
    for (i = 0; i < VTSS_CFG_CHAIN_SIZE && cpuevent_chain[i].valid; i++) {
        TRACE("[%02d]: mux_idx=%d, mux_grp=%d of %d %s", i,
              cpuevent_chain[i].mux_idx, cpuevent_chain[i].mux_grp, cpuevent_chain[0].mux_cnt,
              (cpuevent_chain[i].mux_grp != cpuevent_chain[i].mux_idx) ? "skip" : ".vft->freeze_read()");
        if (cpuevent_chain[i].mux_grp != cpuevent_chain[i].mux_idx)
            continue;
        cpuevent_chain[i].vft->freeze_read((cpuevent_t*)&cpuevent_chain[i]);
    }
}

/* this->tmp: positive - update and restart, negative - just restart
 *  0 - reset counter,
 *  1 - switch_to,
 *  2 - preempt,
 *  3 - sync,
 * -1 - switch_to no update,
 * -2 - preempt no update,
 * -3 - sync no update
 */
void vtss_cpuevents_quantum_border(cpuevent_t* cpuevent_chain, int flag)
{
    int i;
    if (unlikely(!cpuevent_chain)){
        ERROR("CPU event chain is empty!!!");
        return;
    }
#if 0
    /// compute idle characteristics
    if (0 /*tidx == pcb_cpu.idle_tidx*/) {
        long long tmp;
        if (flag == 1) { /* switch_to */
            pcb_cpu.idle_c1_residency = vtss_time_cpu();

            if (sysevent_type[vtss_sysevent_idle_c3] != vtss_sysevent_end) {
                rdmsrl(VTSS_MSR_CORE_C3_RESIDENCY, pcb_cpu.idle_c3_residency);
                rdmsrl(VTSS_MSR_CORE_C6_RESIDENCY, pcb_cpu.idle_c6_residency);
            }
            if (sysevent_type[vtss_sysevent_idle_c7] != vtss_sysevent_end) {
                rdmsrl(VTSS_MSR_CORE_C7_RESIDENCY, pcb_cpu.idle_c7_residency);
            }
        } else if (pcb_cpu.idle_c1_residency) {
            pcb_cpu.idle_duration = vtss_time_cpu() - pcb_cpu.idle_c1_residency;
            pcb_cpu.idle_c1_residency = 0;

            if (sysevent_type[vtss_sysevent_idle_c3] != vtss_sysevent_end) {
                rdmsrl(VTSS_MSR_CORE_C3_RESIDENCY, tmp);
                pcb_cpu.idle_c3_residency = tmp - pcb_cpu.idle_c3_residency;
                rdmsrl(VTSS_MSR_CORE_C6_RESIDENCY, tmp);
                pcb_cpu.idle_c6_residency = tmp - pcb_cpu.idle_c6_residency;
            }
            if (sysevent_type[vtss_sysevent_idle_c7] != vtss_sysevent_end) {
                rdmsrl(VTSS_MSR_CORE_C7_RESIDENCY, tmp);
                pcb_cpu.idle_c7_residency = tmp - pcb_cpu.idle_c7_residency;
            }
        }
        if (pcb_cpu.idle_duration < 0 || pcb_cpu.idle_c3_residency < 0 || pcb_cpu.idle_c6_residency < 0 || pcb_cpu.idle_c7_residency < 0) {
            pcb_cpu.idle_duration = pcb_cpu.idle_c3_residency = pcb_cpu.idle_c6_residency = pcb_cpu.idle_c7_residency = 0;
        }
    }
#endif
    for (i = 0; i < VTSS_CFG_CHAIN_SIZE && cpuevent_chain[i].valid; i++) {
        TRACE("[%02d]: mux_idx=%d, mux_grp=%d of %d %s flag=%d", i,
              cpuevent_chain[i].mux_idx, cpuevent_chain[i].mux_grp, cpuevent_chain[i].mux_cnt,
              (cpuevent_chain[i].mux_grp != cpuevent_chain[i].mux_idx) ? "skip" : ".vft->update_restart()", flag);
        if (cpuevent_chain[i].mux_grp != cpuevent_chain[i].mux_idx)
            continue;
        cpuevent_chain[i].tmp = flag;
        cpuevent_chain[i].vft->update_restart((cpuevent_t*)&cpuevent_chain[i]);
    }
#if 0
    /// flush idleness data
    if (0 /*tidx != pcb_cpu.idle_tidx*/) {
        pcb_cpu.idle_duration     = 0;
        pcb_cpu.idle_c1_residency = 0;
        pcb_cpu.idle_c3_residency = 0;
        pcb_cpu.idle_c6_residency = 0;
        pcb_cpu.idle_c7_residency = 0;
    }
#endif
}

// called from swap_in() and pmi_handler()
// to re-select multiplexion groups and restart counting
void vtss_cpuevents_restart(cpuevent_t* cpuevent_chain, int flag)
{
    int i, j;
    long long muxchange_time = 0;
    int muxchange_alt = 0;
    int mux_idx = 0;
    int mux_cnt;
    int mux_alg;
    int mux_arg;
    int mux_flag;

    vtss_cpuevents_enable();
    for (i = 0; i < VTSS_CFG_CHAIN_SIZE && cpuevent_chain[i].valid; i++) {
        /// update current MUX group in accordance with MUX algorithm
        /// and parameter restart counting for the active MUX group
        if (i == 0) {
            /// load MUX context
            muxchange_time = cpuevent_chain[0].muxchange_time;
            muxchange_alt  = cpuevent_chain[0].muxchange_alt;
            mux_idx = cpuevent_chain[0].mux_idx;
            mux_cnt = cpuevent_chain[0].mux_cnt;
            mux_alg = cpuevent_chain[0].mux_alg;
            mux_arg = cpuevent_chain[0].mux_arg;

            /// update current MUX index
            switch (mux_alg) {
            case VTSS_CFGMUX_NONE:
                /// no update to MUX index
                break;

            case VTSS_CFGMUX_TIME:
                if (!muxchange_time) {
                    /// setup new time interval
                    muxchange_time = vtss_time_cpu() + (mux_arg * hardcfg.cpu_freq);
                } else if (vtss_time_cpu() >= muxchange_time) {
                    mux_idx = (mux_idx + 1 > mux_cnt) ? 0 : mux_idx + 1;
                    muxchange_time = 0;
                }
                break;

            case VTSS_CFGMUX_MST:
            case VTSS_CFGMUX_SLV:
                for (j = 0, mux_flag = 0; j < VTSS_CFG_CHAIN_SIZE && cpuevent_chain[j].valid; j++) {
                    if (cpuevent_chain[j].mux_grp == mux_idx && cpuevent_chain[j].mux_alg == VTSS_CFGMUX_MST) {
                        if (cpuevent_chain[j].vft->overflowed((cpuevent_t*)&cpuevent_chain[j])) {
                            mux_flag = 1;
                            break;
                        }
                    }
                }
                if (!mux_flag) {
                    break;
                }
                /// else fall through

            case VTSS_CFGMUX_SEQ:
                if (!muxchange_alt) {
                    muxchange_alt = mux_arg;
                } else {
                    if (!--muxchange_alt) {
                        mux_idx = (mux_idx + 1 > mux_cnt) ? 0 : mux_idx + 1;
                    }
                }
                break;

            default:
                /// erroneously configured, ignore
                break;
            }
        }

        /// save MUX context
        cpuevent_chain[i].muxchange_time = muxchange_time;
        cpuevent_chain[i].muxchange_alt  = muxchange_alt;
        cpuevent_chain[i].mux_idx        = mux_idx;

        TRACE("[%02d]: mux_idx=%d, mux_grp=%d of %d %s", i,
              cpuevent_chain[i].mux_idx, cpuevent_chain[i].mux_grp, cpuevent_chain[0].mux_cnt,
              (cpuevent_chain[i].mux_grp != cpuevent_chain[i].mux_idx) ? "skip" : ".vft->restart()");
        /* restart counting */
        if (cpuevent_chain[i].mux_grp != cpuevent_chain[i].mux_idx)
            continue;
        cpuevent_chain[i].vft->restart((cpuevent_t*)&cpuevent_chain[i]);
    }
}

static int vtss_cpuevents_check_overflow(cpuevent_t* cpuevent_chain)
{
    int i;

    for (i = 0; i < VTSS_CFG_CHAIN_SIZE && cpuevent_chain[i].valid; i++) {
        if (cpuevent_chain[i].vft->overflowed((cpuevent_t*)&cpuevent_chain[i])) {
            // TODO: ...
        }
    }
    return 0;
}

static void vtss_cpuevents_save(void *ctx)
{
    unsigned long flags;
    gate_desc *idt_base;
    struct desc_ptr idt_ptr;

    local_irq_save(flags);
    if (hardcfg.family == 0x06 && hardcfg.model >= 0x0f) {
        rdmsrl(MSR_PERF_GLOBAL_OVF_CTRL, pcb_cpu.saved_msr_ovf);
        wrmsrl(MSR_PERF_GLOBAL_OVF_CTRL, 0ULL);
        rdmsrl(MSR_PERF_GLOBAL_CTRL,     pcb_cpu.saved_msr_perf);
        wrmsrl(MSR_PERF_GLOBAL_CTRL,     0ULL);
        rdmsrl(DEBUGCTL_MSR,             pcb_cpu.saved_msr_debug);
        wrmsrl(DEBUGCTL_MSR,             0ULL);
    } else if (hardcfg.family == 0x0b) { // KNX_CORE_FAMILY
        rdmsrl(KNX_CORE_PERF_GLOBAL_OVF_CTRL, pcb_cpu.saved_msr_ovf);
        wrmsrl(KNX_CORE_PERF_GLOBAL_OVF_CTRL, 0ULL);
        rdmsrl(KNX_CORE_PERF_GLOBAL_CTRL,     pcb_cpu.saved_msr_perf);
        wrmsrl(KNX_CORE_PERF_GLOBAL_CTRL,     0ULL);
    }
    store_idt(&idt_ptr);
    idt_base = (gate_desc*)idt_ptr.address;
    pcb_cpu.idt_base = idt_base;
    memcpy(&pcb_cpu.saved_perfvector, &idt_base[CPU_PERF_VECTOR], sizeof(gate_desc));
    local_irq_restore(flags);
}

static void vtss_cpuevents_stop_all(void *ctx)
{
    unsigned long flags;

    local_irq_save(flags);
    vtss_cpuevents_stop();
    if (hardcfg.family == 0x06 && hardcfg.model >= 0x0f) {
        wrmsrl(MSR_PERF_GLOBAL_OVF_CTRL, 0ULL);
        wrmsrl(MSR_PERF_GLOBAL_CTRL,     0ULL);
        wrmsrl(DEBUGCTL_MSR,             0ULL);
    } else if (hardcfg.family == 0x0b) { // KNX_CORE_FAMILY
        wrmsrl(KNX_CORE_PERF_GLOBAL_OVF_CTRL, 0ULL);
        wrmsrl(KNX_CORE_PERF_GLOBAL_CTRL,     0ULL);
    }
    vtss_pmi_disable();
    local_irq_restore(flags);
}

static void vtss_cpuevents_restore(void *ctx)
{
    unsigned long flags;
    gate_desc *idt_base;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    unsigned long cr0;
#endif

    local_irq_save(flags);
    if (hardcfg.family == 0x06 && hardcfg.model >= 0x0f) {
        wrmsrl(MSR_PERF_GLOBAL_OVF_CTRL, pcb_cpu.saved_msr_ovf);
        wrmsrl(MSR_PERF_GLOBAL_CTRL,     pcb_cpu.saved_msr_perf);
        wrmsrl(DEBUGCTL_MSR,             pcb_cpu.saved_msr_debug);
    } else if (hardcfg.family == 0x0b) { // KNX_CORE_FAMILY
        wrmsrl(KNX_CORE_PERF_GLOBAL_OVF_CTRL, pcb_cpu.saved_msr_ovf);
        wrmsrl(KNX_CORE_PERF_GLOBAL_CTRL,     pcb_cpu.saved_msr_perf);
    }
    idt_base = pcb_cpu.idt_base;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    cr0 = read_cr0();
    write_cr0(cr0 & ~X86_CR0_WP);
#endif
    write_idt_entry(idt_base, CPU_PERF_VECTOR, &pcb_cpu.saved_perfvector);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    write_cr0(cr0);
#endif
        
    local_irq_restore(flags);
}

static void vtss_cpuevents_setup(void *ctx)
{
    unsigned long flags;
    gate_desc *idt_base, g;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    unsigned long cr0;
#endif
    local_irq_save(flags);
    idt_base = pcb_cpu.idt_base;
    pack_gate(&g, GATE_INTERRUPT, (unsigned long)vtss_perfvec_handler, 3, 0, __KERNEL_CS);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    cr0 = read_cr0();
    write_cr0(cr0 & ~X86_CR0_WP);
#endif
    write_idt_entry(idt_base, CPU_PERF_VECTOR, &g);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    write_cr0(cr0);
#endif

    local_irq_restore(flags);
}

int vtss_cpuevents_init_pmu(int defsav)
{
    INFO ("init pmu, reqcfg.cpuevent_count_v1=%d", (int)reqcfg.cpuevent_count_v1);
    if ((reqcfg.cpuevent_count_v1 == 0 && !(reqcfg.trace_cfg.trace_flags & (VTSS_CFGTRACE_CTX|VTSS_CFGTRACE_PWRACT|VTSS_CFGTRACE_PWRIDLE)))||
        (reqcfg.cpuevent_count_v1 == 0 && hardcfg.family == 0x0b))
    {
        /* There is no configuration was get from runtool, so init defaults */
        INFO("There is no configuration was get from runtool, so init defaults");
        vtss_cpuevents_reqcfg_default(1, defsav);
        vtss_sysevents_reqcfg_append();
    }
    on_each_cpu(vtss_cpuevents_save,  NULL, SMP_CALL_FUNCTION_ARGS);
    on_each_cpu(vtss_cpuevents_setup, NULL, SMP_CALL_FUNCTION_ARGS);
    return 0;
}

void vtss_cpuevents_fini_pmu(void)
{
    on_each_cpu(vtss_cpuevents_stop_all, NULL, SMP_CALL_FUNCTION_ARGS);
    on_each_cpu(vtss_cpuevents_restore,  NULL, SMP_CALL_FUNCTION_ARGS);
}

union cpuid_0AH_eax
{
    struct
    {
        unsigned int version_id:8;
        unsigned int counters_no:8;
        unsigned int counters_width:8;
        unsigned int reserved:8;
    } split;
    unsigned int full;
};

union cpuid_0AH_edx
{
    struct
    {
        unsigned int fixed_counters_no:5;
        unsigned int fixed_counters_width:8;
        unsigned int reserved:29;
    } split;
    unsigned int full;
};

int vtss_cpuevents_init(void)
{
    int counter_offset = 0; /* .modifier = VTSS_EVMOD_ALL | counter_offset; */

    if (hardcfg.family == 0x0b) {   // KNX_CORE_FAMILY
        pmu_counter_no    = 2;
        pmu_counter_width = 40;
        pmu_counter_width_mask = (((unsigned long long)1) << pmu_counter_width) - 1;

        pmu_fixed_counter_no    = 0;
        pmu_fixed_counter_width = 0;
        pmu_fixed_counter_width_mask = 0;
    } else if (hardcfg.family == 0x0f || hardcfg.family == 0x06) { // P4 or P6
        union cpuid_0AH_eax eax;
        union cpuid_0AH_edx edx;
        unsigned int ebx, ecx;

        cpuid(0x0a, &eax.full, &ebx, &ecx, &edx.full);

        pmu_counter_no    = eax.split.counters_no;
        pmu_counter_width = eax.split.counters_width;
        pmu_counter_width_mask = (1ULL << pmu_counter_width) - 1;

        pmu_fixed_counter_no    = edx.split.fixed_counters_no;
        pmu_fixed_counter_width = edx.split.fixed_counters_width;
        pmu_fixed_counter_width_mask = (1ULL << pmu_fixed_counter_width) - 1;

        counter_offset = (pmu_counter_no >= 4) ? VTSS_EVMOD_CNT3 : VTSS_EVMOD_CNT1;
    }
    TRACE("PMU: counters=%d, width=%d (0x%llX)", pmu_counter_no, pmu_counter_width, pmu_counter_width_mask);
    TRACE("PMU:    fixed=%d, width=%d (0x%llX)", pmu_fixed_counter_no, pmu_fixed_counter_width, pmu_fixed_counter_width_mask);

    if (!pmu_counter_no) {
        ERROR("PMU counters are not detected");
        hardcfg.family = VTSS_UNKNOWN_ARCH;
    }

    memset(cpuevent_desc, 0, sizeof(cpuevent_desc));
    if (hardcfg.family == 0x0b) {   // KNX_CORE_FAMILY
        TRACE("KNX-cpuevents is used");
        cpuevent_desc[0].event_id = VTSS_EVID_NONHALTED_CLOCKTICKS;
        cpuevent_desc[0].vft      = &vft_knx;
        cpuevent_desc[0].name     = "CPU_CLK_UNHALTED";
        cpuevent_desc[0].desc     = "CPU_CLK_UNHALTED";
        cpuevent_desc[0].modifier = VTSS_EVMOD_ALL | counter_offset;
        cpuevent_desc[0].selmsr   = KNX_CORE_PERFEVTSEL0;
        cpuevent_desc[0].cntmsr   = KNX_CORE_PMC0;
        cpuevent_desc[0].selmsk   = 0x53002A;

        cpuevent_desc[1].event_id = VTSS_EVID_INSTRUCTIONS_RETIRED;
        cpuevent_desc[1].vft      = &vft_knx;
        cpuevent_desc[1].name     = "INSTRUCTIONS_EXECUTED";
        cpuevent_desc[1].desc     = "INSTRUCTIONS_EXECUTED";
        cpuevent_desc[1].modifier = VTSS_EVMOD_ALL | counter_offset;
        cpuevent_desc[1].selmsr   = KNX_CORE_PERFEVTSEL0+1;
        cpuevent_desc[1].cntmsr   = KNX_CORE_PMC0+1;
        cpuevent_desc[1].selmsk   = 0x530016;
    } else if (hardcfg.family == 0x06) {   // P6
        TRACE("P6-cpuevents is used");
        cpuevent_desc[0].event_id = VTSS_EVID_FIXED_INSTRUCTIONS_RETIRED;
        cpuevent_desc[0].vft      = &vft_p6;
        cpuevent_desc[0].name     = "INST_RETIRED.ANY";
        cpuevent_desc[0].desc     = "INST_RETIRED.ANY";
        cpuevent_desc[0].modifier = VTSS_EVMOD_ALL | counter_offset;
        cpuevent_desc[0].selmsr   = IA32_FIXED_CTR_CTRL;
        cpuevent_desc[0].cntmsr   = IA32_FIXED_CTR0;
        cpuevent_desc[0].selmsk   = 0x0000000b;

        cpuevent_desc[1].event_id = VTSS_EVID_FIXED_NONHALTED_CLOCKTICKS;
        cpuevent_desc[1].vft      = &vft_p6;
        cpuevent_desc[1].name     = "CPU_CLK_UNHALTED.THREAD";
        cpuevent_desc[1].desc     = "CPU_CLK_UNHALTED.THREAD";
        cpuevent_desc[1].modifier = VTSS_EVMOD_ALL | counter_offset;
        cpuevent_desc[1].selmsr   = IA32_FIXED_CTR_CTRL;
        cpuevent_desc[1].cntmsr   = IA32_FIXED_CTR0+1;
        cpuevent_desc[1].selmsk   = 0x000000b0;

        cpuevent_desc[2].event_id = VTSS_EVID_FIXED_NONHALTED_REFTICKS;
        cpuevent_desc[2].vft      = &vft_p6;
        cpuevent_desc[2].name     = "CPU_CLK_UNHALTED.REF";
        cpuevent_desc[2].desc     = "CPU_CLK_UNHALTED.REF";
        cpuevent_desc[2].modifier = VTSS_EVMOD_ALL | counter_offset;
        cpuevent_desc[2].selmsr   = IA32_FIXED_CTR_CTRL;
        cpuevent_desc[2].cntmsr   = IA32_FIXED_CTR0+2;
        cpuevent_desc[2].selmsk   = 0x00000b00;

        /* CPU BUG: broken fixed counters on some Meroms and Penryns */
        if (hardcfg.model == 0x0f && hardcfg.stepping < 0x0b) {
            ERROR("All fixed counters are broken");
        } else if (hardcfg.model == 0x17) {
            ERROR("CPU_CLK_UNHALTED.REF fixed counter is broken");
        }

        { /* check for read-only counter mode */
            unsigned long long tmp, tmp1;

            wrmsrl(IA32_PERFEVTSEL0, 0ULL);
            wrmsrl(IA32_PMC0, 0ULL);
            rdmsrl(IA32_PMC0, tmp);
            tmp |= 0x7f00ULL;
            wrmsrl(IA32_PMC0, tmp);
            rdmsrl(IA32_PMC0, tmp1);
            if (tmp1 != tmp) {
                /* read-only counters, change the event VFT */
                vft_p6.restart     = vf_p6_restart_ro;
                vft_p6.freeze_read = vf_p6_freeze_read_ro;
            }
            wrmsrl(IA32_PMC0, 0ULL);
        }
    }
    /// TODO: validate SNB and MFLD energy meters:
    /// sysevent_type[vtss_sysevent_energy_xxx] = vtss_sysevent_end if not present
    INFO("family=%x, model=%x", hardcfg.family,hardcfg.model);
    if (hardcfg.family == 0x06) {
        if(hardcfg.model == 0x2a || hardcfg.model == 0x3a || hardcfg.model == 0x3c ||
           hardcfg.model == 0x3d || hardcfg.model == 0x3e || hardcfg.model == 0x3f || 
           hardcfg.model == 0x45 || hardcfg.model == 0x46)
        {
            sysevent_type[vtss_sysevent_energy_dram] = vtss_sysevent_end;
        } else if (hardcfg.model == 0x2d) {
            sysevent_type[vtss_sysevent_energy_gfx]  = vtss_sysevent_end;
        } else {
            sysevent_type[vtss_sysevent_energy_core] = vtss_sysevent_end;
            sysevent_type[vtss_sysevent_energy_gfx]  = vtss_sysevent_end;
            sysevent_type[vtss_sysevent_energy_pack] = vtss_sysevent_end;
            sysevent_type[vtss_sysevent_energy_dram] = vtss_sysevent_end;
        }
    } else {
        sysevent_type[vtss_sysevent_energy_core] = vtss_sysevent_end;
        sysevent_type[vtss_sysevent_energy_gfx]  = vtss_sysevent_end;
        sysevent_type[vtss_sysevent_energy_pack] = vtss_sysevent_end;
        sysevent_type[vtss_sysevent_energy_dram] = vtss_sysevent_end;
    }
    ///if (hardcfg.family != 0x06 || hardcfg.model != 0x27)
    sysevent_type[vtss_sysevent_energy_soc] = vtss_sysevent_end;
#if 0
    /// disable C-state residency events if not supported
    if (hardcfg.family == 0x06) {
        switch (hardcfg.model) {
        /// NHM/WMR
        case 0x1a:
        case 0x1e:
        case 0x1f:
        case 0x2e:
        case 0x25:
        case 0x2c:
            sysevent_type[vtss_sysevent_idle_c7] = vtss_sysevent_end;
            break;
        /// SNB/IVB
        case 0x2a:
        case 0x2d:
        case 0x3a:
            break;
        /// Medfield/CedarTrail/CloverTrail
        case 0x27:
        /// TODO: make sure Cx MSRs are supported
        ///case 0x35:
        ///case 0x36:
            break;
        default:
            sysevent_type[vtss_sysevent_idle_c3] = vtss_sysevent_end;
            sysevent_type[vtss_sysevent_idle_c6] = vtss_sysevent_end;
            sysevent_type[vtss_sysevent_idle_c7] = vtss_sysevent_end;
        }
    } else {
        sysevent_type[vtss_sysevent_idle_c3] = vtss_sysevent_end;
        sysevent_type[vtss_sysevent_idle_c6] = vtss_sysevent_end;
        sysevent_type[vtss_sysevent_idle_c7] = vtss_sysevent_end;
    }
#else
    /* TODO: Not implemeted. Turn off for Linux now all idle_* */
    sysevent_type[vtss_sysevent_idle_time]   = vtss_sysevent_end;
    sysevent_type[vtss_sysevent_idle_wakeup] = vtss_sysevent_end;
    sysevent_type[vtss_sysevent_idle_c3]     = vtss_sysevent_end;
    sysevent_type[vtss_sysevent_idle_c6]     = vtss_sysevent_end;
    sysevent_type[vtss_sysevent_idle_c7]     = vtss_sysevent_end;
#endif
    return 0;
}

void vtss_cpuevents_fini(void)
{
    pmu_counter_no         = 0;
    pmu_counter_width      = 0;
    pmu_counter_width_mask = 0x000000ffffffffffULL;

    pmu_fixed_counter_no         = 0;
    pmu_fixed_counter_width      = 0;
    pmu_fixed_counter_width_mask = 0x000000ffffffffffULL;

    hardcfg.family = VTSS_UNKNOWN_ARCH;
}
