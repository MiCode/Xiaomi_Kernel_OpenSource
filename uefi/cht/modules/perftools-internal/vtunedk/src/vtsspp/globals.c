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
#include "globals.h"
#include "apic.h"
#include "time.h"

#include <linux/utsname.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

/// processor control blocks
#ifdef DEFINE_PER_CPU_SHARED_ALIGNED
DEFINE_PER_CPU_SHARED_ALIGNED(vtss_pcb_t, vtss_pcb);
#else
DEFINE_PER_CPU(vtss_pcb_t, vtss_pcb);
#endif

/// trace format information to enable forward compatibility
fmtcfg_t fmtcfg[2];

/// system configuration
vtss_syscfg_t syscfg;

/// hardware configuration
vtss_hardcfg_t hardcfg;

/// profiling configuration
process_cfg_t reqcfg;

/* time source for collection */
int vtss_time_source = 0;

/* time limit for collection */
cycles_t vtss_time_limit = 0ULL;

static void vtss_fmtcfg_init(void)
{
    /*
     * leaf 1: base 
     */
    fmtcfg[0].rank = 0;
    fmtcfg[0].and_mask = UEC_LEAF0 | UEC_LEAF1 | UEC_LEAF2 | UEC_LEAF3;
    fmtcfg[0].cmp_mask = UEC_LEAF1;
    fmtcfg[0].defcount = 0x20;

    fmtcfg[0].defbit[0x00] = 4; /// UECL1_ACTIVITY      0x00000001
    fmtcfg[0].defbit[0x01] = 4; /// UECL1_VRESIDX       0x00000002
    fmtcfg[0].defbit[0x02] = 4; /// UECL1_CPUIDX        0x00000004
    fmtcfg[0].defbit[0x03] = 8; /// UECL1_USRLVLID      0x00000008
    fmtcfg[0].defbit[0x04] = 8; /// UECL1_CPUTSC        0x00000010
    fmtcfg[0].defbit[0x05] = 8; /// UECL1_REALTSC       0x00000020
    fmtcfg[0].defbit[0x06] = 1; /// UECL1_MUXGROUP      0x00000040
    fmtcfg[0].defbit[0x07] = 8; /// UECL1_CPUEVENT      0x00000080
    fmtcfg[0].defbit[0x08] = 8; /// UECL1_CHPSETEV      0x00000100
    fmtcfg[0].defbit[0x09] = 8; /// UECL1_OSEVENT       0x00000200
    fmtcfg[0].defbit[0x0a] = 8; /// UECL1_EXECADDR      0x00000400
    fmtcfg[0].defbit[0x0b] = 8; /// UECL1_REFADDR       0x00000800
    fmtcfg[0].defbit[0x0c] = 8; /// UECL1_EXEPHYSADDR   0x00001000
    fmtcfg[0].defbit[0x0d] = 8; /// UECL1_REFPHYSADDR   0x00002000
    fmtcfg[0].defbit[0x0e] = 4; /// UECL1_TPIDX         0x00004000
    fmtcfg[0].defbit[0x0f] = 8; /// UECL1_TPADDR        0x00008000
    fmtcfg[0].defbit[0x10] = 8; /// UECL1_PWREVENT      0x00010000
    fmtcfg[0].defbit[0x11] = 8; /// UECL1_CPURECTSC     0x00020000
    fmtcfg[0].defbit[0x12] = 8; /// UECL1_REALRECTSC    0x00040000
    fmtcfg[0].defbit[0x13] = 81;    /// UECL1_PADDING       0x00080000
    fmtcfg[0].defbit[0x14] = VTSS_FMTCFG_RESERVED;  /// UECL1_UNKNOWN0      0x00100000
    fmtcfg[0].defbit[0x15] = VTSS_FMTCFG_RESERVED;  /// UECL1_UNKNOWN1      0x00200000
    fmtcfg[0].defbit[0x16] = 82;    /// UECL1_SYSTRACE      0x00400000
    fmtcfg[0].defbit[0x17] = 84;    /// UECL1_LARGETRACE    0x00800000
    fmtcfg[0].defbit[0x18] = 82;    /// UECL1_USERTRACE     0x01000000
    fmtcfg[0].defbit[0x19] = 0;
    fmtcfg[0].defbit[0x1a] = 0;
    fmtcfg[0].defbit[0x1b] = 0;
    fmtcfg[0].defbit[0x1c] = 0;
    fmtcfg[0].defbit[0x1d] = 0;
    fmtcfg[0].defbit[0x1e] = 0;
    fmtcfg[0].defbit[0x1f] = 0;

    /*
     * leaf 1: extended 
     */
    fmtcfg[1].rank = 1;
    fmtcfg[1].and_mask = UEC_LEAF0 | UEC_LEAF1 | UEC_LEAF2 | UEC_LEAF3;
    fmtcfg[1].cmp_mask = UEC_LEAF1;
    fmtcfg[1].defcount = 0x20;

    fmtcfg[1].defbit[0x00] = 8; /// UECL1_EXT_CPUFREQ   0x00000001
    fmtcfg[1].defbit[0x01] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x02] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x03] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x04] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x05] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x06] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x07] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x08] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x09] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x0a] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x0b] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x0c] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x0d] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x0e] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x0f] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x10] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x11] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x12] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x13] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x14] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x15] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x16] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x17] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x18] = VTSS_FMTCFG_RESERVED;
    fmtcfg[1].defbit[0x19] = 0;
    fmtcfg[1].defbit[0x1a] = 0;
    fmtcfg[1].defbit[0x1b] = 0;
    fmtcfg[1].defbit[0x1c] = 0;
    fmtcfg[1].defbit[0x1d] = 0;
    fmtcfg[1].defbit[0x1e] = 0;
    fmtcfg[1].defbit[0x1f] = 0;
}

static void vtss_syscfg_init(void)
{
    char utsname[2*__NEW_UTS_LEN+2];
    vtss_syscfg_t *sysptr = &syscfg;
    struct new_utsname *u = init_utsname();

    /// sysinfo
    syscfg.version = 1;
    syscfg.major = (short)0;
    syscfg.minor = (short)0;
    syscfg.spack = (short)0;
    syscfg.extra = (short)0;
#if defined(CONFIG_X86_32)
    syscfg.type  = VTSS_LINUX_IA32;
#elif defined(CONFIG_X86_64)
#if defined(VTSS_ARCH_KNX)
    syscfg.type  = VTSS_LINUX_KNC;
#else
    syscfg.type  = VTSS_LINUX_EM64T;
#endif
#else
    syscfg.type  = VTSS_UNKNOWN_ARCH;
#endif

    /// host name
    TRACE("u->nodename='%s'", u->nodename);
    sysptr->len = 1 + strlen(u->nodename);
    memcpy(sysptr->host_name, u->nodename, sysptr->len);
    sysptr = (vtss_syscfg_t*)((char*)sysptr + sysptr->len + sizeof(short));

    /// platform brand name
    TRACE("u->sysname='%s'", u->sysname);
    TRACE("u->machine='%s'", u->machine);
    snprintf(utsname, sizeof(utsname)-1, "%s-%s", u->sysname, u->machine);
    sysptr->len = 1 + strlen(utsname);
    memcpy(sysptr->brand_name, utsname, sysptr->len);
    sysptr = (vtss_syscfg_t*)((char*)sysptr + sysptr->len + sizeof(short));

    /// system ID string
    TRACE("u->release='%s'", u->release);
    TRACE("u->version='%s'", u->version);
    snprintf(utsname, sizeof(utsname)-1, "%s %s", u->release, u->version);
    sysptr->len = 1 + strlen(utsname);
    memcpy(sysptr->host_name, utsname, sysptr->len);
    sysptr = (vtss_syscfg_t*)((char*)sysptr + sysptr->len + sizeof(short));

    /// root directory
    sysptr->len = 2; /* 1 + strlen("/") */
    memcpy(sysptr->host_name, "/", sysptr->len);
    sysptr = (vtss_syscfg_t*)((char*)sysptr + sysptr->len + sizeof(short));

    syscfg.record_size = (int)((char *)sysptr - (char *)&syscfg + (char *)&syscfg.len - (char *)&syscfg);
}

union cpuid_01H_eax
{
    struct
    {
        unsigned int stepping:4;
        unsigned int model:4;
        unsigned int family:4;
        unsigned int type:2;
        unsigned int reserved1:2;
        unsigned int model_ext:4;
        unsigned int family_ext:8;
        unsigned int reserved2:4;
    } split;
    unsigned int full;
};

union cpuid_01H_ebx
{
    struct
    {
        unsigned int brand_index:8;
        unsigned int cache_line_size:8;
        unsigned int unit_no:8;
        unsigned int reserved:8;
    } split;
    unsigned int full;
};

union cpuid_04H_eax
{
    struct
    {
        unsigned int reserved:14;
        unsigned int smt_no:12;
        unsigned int core_no:6;
    } split;
    unsigned int full;
};

static void vtss_hardcfg_init(void)
{
    int cpu;
    union cpuid_01H_eax eax1;
    union cpuid_01H_ebx ebx1;
    union cpuid_04H_eax eax4;
    unsigned int ebx, ecx, edx;
    int node_no   = 0;
    int pack_no   = 0;
    int core_no   = 0;
    int thread_no = 0;
    int ht_supported = 0;
    const ktime_t ktime_res = KTIME_MONOTONIC_RES;

    /* Global variable vtss_time_source affects result of vtss_*_real() */
    vtss_time_source = 0;
    if (ktime_equal(KTIME_MONOTONIC_RES, KTIME_LOW_RES)) {
        INFO("An accuracy of kernel timer is not enough. Switch to TSC.");
        vtss_time_source = 1;
    }
    hardcfg.timer_freq = vtss_freq_real(); /* should be after change vtss_time_source */
    hardcfg.cpu_freq   = vtss_freq_cpu();
    TRACE("timer_freq=%lldHz, cpu_freq=%lldHz, ktime_res=%lld", hardcfg.timer_freq, hardcfg.cpu_freq, ktime_res.tv64);
    hardcfg.version = 0x0002;
    // for 32 bits is like 0xC0000000
    // for 64 bits is like 0xffff880000000000
    hardcfg.maxusr_address = PAGE_OFFSET; /*< NOTE: Will be changed in vtss_record_configs() */
    /// initialize execution mode, OS version, and CPU ID parameters
#if defined(CONFIG_X86_32)
    hardcfg.mode    = 32;
    hardcfg.os_type = VTSS_LINUX_IA32;
#elif defined(CONFIG_X86_64)
    hardcfg.mode    = 64;
#if defined(VTSS_ARCH_KNX)
    hardcfg.os_type = VTSS_LINUX_KNC;
#else
    hardcfg.os_type = VTSS_LINUX_EM64T;
#endif
#else
    hardcfg.mode    = 0;
    hardcfg.os_type = VTSS_UNKNOWN_ARCH;
#endif
    hardcfg.os_major = 0;
    hardcfg.os_minor = 0;
    hardcfg.os_sp    = 0;

    cpuid(0x01, &eax1.full, &ebx1.full, &ecx, &edx);
    if ((hardcfg.family = eax1.split.family) == 0x0f)
        hardcfg.family += eax1.split.family_ext;
    hardcfg.model = eax1.split.model;
    if (eax1.split.family == 0x06 || eax1.split.family == 0x0f)
        hardcfg.model += (eax1.split.model_ext << 4);
    hardcfg.stepping = eax1.split.stepping;
    ht_supported = ((edx >> 28) & 1) ? 1 : 0;
    TRACE("CPUID(family=%02x, model=%02x, stepping=%02x, ht=%d)", hardcfg.family, hardcfg.model, hardcfg.stepping, ht_supported);

    cpuid(0x04, &eax4.full, &ebx, &ecx, &edx);
    hardcfg.cpu_no = num_present_cpus();
    /* TODO: determine the number of nodes */
    node_no = 1;
    if (hardcfg.cpu_no == 1) {
        thread_no = core_no = pack_no = 1;
    } else {
        if ((hardcfg.family == 0x0f && hardcfg.model >= 0x04 && hardcfg.stepping >= 0x04) ||
            (hardcfg.family == 0x06 && hardcfg.model >= 0x0f))
        {
            thread_no = eax4.split.smt_no  + 1;
            core_no   = eax4.split.core_no + 1;
        } else if (hardcfg.family == 0x0f) { // P4
            thread_no = ebx1.split.unit_no;
            core_no   = 1;
        } else if (hardcfg.family == 0x0b) { // KNX_CORE
            thread_no = 4;
            core_no   = eax4.split.core_no ? (eax4.split.core_no + 1) : (hardcfg.cpu_no / thread_no);
        } else {
            thread_no = ebx1.split.unit_no;
            core_no   = eax4.split.core_no + 1;
        }
        thread_no = thread_no ? thread_no : 1;
        core_no   = core_no   ? core_no   : 1;
        pack_no   = hardcfg.cpu_no / (core_no * thread_no * node_no);
    }
    TRACE("cpu_no=%d, node_no=%d, pack_no=%d, core_no=%d, thread_no=%d",
          hardcfg.cpu_no, node_no, pack_no, core_no, thread_no);

    /*
     * disable the driver for P4 as it is not going
     * to be supported in the future
     */
    if (hardcfg.family == 0x0f) { // P4
        hardcfg.family = VTSS_UNKNOWN_ARCH;
    }

    /*
     * build cpu map - distribute the current thread to all CPUs
     * to compute CPU IDs for asymmetric system configurations
     */
    for_each_present_cpu(cpu) {
        struct cpuinfo_x86 *c = &cpu_data(cpu);

        hardcfg.cpu_map[cpu].node   = cpu_to_node(cpu);
        hardcfg.cpu_map[cpu].pack   = c->phys_proc_id;
        hardcfg.cpu_map[cpu].core   = c->cpu_core_id;
        hardcfg.cpu_map[cpu].thread = c->initial_apicid & (thread_no - 1);
        TRACE("cpu[%d]: node=%d, pack=%d, core=%d, thread=%d",
                cpu, hardcfg.cpu_map[cpu].node, hardcfg.cpu_map[cpu].pack,
                hardcfg.cpu_map[cpu].core, hardcfg.cpu_map[cpu].thread);
    }
}

void vtss_globals_fini(void)
{
    int cpu;

    vtss_apic_fini();
    for_each_possible_cpu(cpu) {
        vtss_pcb_t* ppcb = &pcb(cpu);
#if 0
        if (ppcb->cpuevent_chain != NULL)
            kfree(ppcb->cpuevent_chain);
        ppcb->cpuevent_chain = NULL;
#endif
        if (ppcb->scratch_ptr != NULL)
            kfree(ppcb->scratch_ptr);
        ppcb->scratch_ptr = NULL;
    }
}

int vtss_globals_init(void)
{
    int cpu;

    memset(&syscfg,  0, sizeof(vtss_syscfg_t));
    memset(&hardcfg, 0, sizeof(vtss_hardcfg_t));
    memset(&fmtcfg,  0, sizeof(fmtcfg_t)*2);
    memset(&reqcfg,  0, sizeof(process_cfg_t));
    for_each_possible_cpu(cpu) {
        vtss_pcb_t* ppcb = &pcb(cpu);
        memset(ppcb, 0, sizeof(vtss_pcb_t));
#if 0
        ppcb->cpuevent_chain = kmalloc_node(VTSS_CFG_CHAIN_SIZE*sizeof(cpuevent_t), GFP_KERNEL, cpu_to_node(cpu));
        if (ppcb->cpuevent_chain == NULL)
            goto fail;
#endif
        ppcb->scratch_ptr = kmalloc_node(VTSS_DYNSIZE_SCRATCH, GFP_KERNEL, cpu_to_node(cpu));
        if (ppcb->scratch_ptr == NULL)
            goto fail;
    }
    vtss_apic_init(); /* Need for vtss_hardcfg_init() */
    vtss_syscfg_init();
    vtss_hardcfg_init();
    vtss_fmtcfg_init();
    return 0;

fail:
    for_each_possible_cpu(cpu) {
        vtss_pcb_t* ppcb = &pcb(cpu);
#if 0
        if (ppcb->cpuevent_chain != NULL)
            kfree(ppcb->cpuevent_chain);
        ppcb->cpuevent_chain = NULL;
#endif
        if (ppcb->scratch_ptr != NULL)
            kfree(ppcb->scratch_ptr);
        ppcb->scratch_ptr = NULL;
    }
    ERROR("NO memory");
    return VTSS_ERR_NOMEMORY;
}
