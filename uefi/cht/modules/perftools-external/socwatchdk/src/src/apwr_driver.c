/* ***********************************************************************************************

   This file is provided under a dual BSD/GPLv2 license.  When using or
   redistributing this file, you may do so under either license.

   GPL LICENSE SUMMARY

   Copyright(c) 2013 Intel Corporation. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of version 2 of the GNU General Public License as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
   The full GNU General Public License is included in this distribution
   in the file called LICENSE.GPL.

   Contact Information:
   SOCWatch Developer Team <socwatchdevelopers@intel.com>

   BSD LICENSE

   Copyright(c) 2013 Intel Corporation. All rights reserved.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in
   the documentation and/or other materials provided with the
   distribution.
   * Neither the name of Intel Corporation nor the names of its
   contributors may be used to endorse or promote products derived
   from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   ***********************************************************************************************
   */

/**
 * apwr_driver.c: Prototype kernel module to trace the following
 * events that are relevant to power:
 *	- entry into a C-state
 *	- change of processor frequency
 *	- interrupts and timers
 */

#define MOD_AUTHOR "Gautam Upadhyaya <gautam.upadhyaya@intel.com>"
#define MOD_DESC "Power driver for Piersol power tool. Adapted from Romain Cledat's codebase."

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/smp.h> // For smp_call_function

#include <asm/local.h>
#include <asm/cputime.h> // For ktime
#include <asm/io.h> // For ioremap, read, and write

#include <trace/events/timer.h>
#include <trace/events/power.h>
#include <trace/events/irq.h>
#include <trace/events/sched.h>
#include <trace/events/syscalls.h>
struct pool_workqueue; // Get rid of warnings regarding trace_workqueue
#include <trace/events/workqueue.h>

#include <linux/hardirq.h> // for "in_interrupt"
#include <linux/interrupt.h> // for "TIMER_SOFTIRQ, HRTIMER_SOFTIRQ"

#include <linux/kallsyms.h>
#include <linux/stacktrace.h>
#include <linux/hash.h>
#include <linux/poll.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/cpufreq.h>
#include <linux/version.h> // for "LINUX_VERSION_CODE"
#include <asm/unistd.h> // for "__NR_execve"
#include <asm/delay.h> // for "udelay"
#include <linux/suspend.h> // for "pm_notifier"
#include <linux/pci.h>
#include <linux/sfi.h> // To retrieve SCU F/W version

#ifdef CONFIG_RPMSG_IPC
    #include <asm/intel_mid_rpmsg.h>
#endif // CONFIG_RPMSG_IPC
/*
#if DO_ANDROID
    #include <asm/intel-mid.h>
#endif // DO_ANDROID
*/
#ifdef CONFIG_X86_WANT_INTEL_MID
    #include <asm/intel-mid.h>
#endif // CONFIG_X86_WANT_INTEL_MID

#if DO_WAKELOCK_SAMPLE
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
#include <trace/events/wakelock.h> // Works for the custom kernel enabling wakelock tracepoint event
#endif
#endif

#ifndef __arm__
#include <asm/timer.h> // for "CYC2NS_SCALE_FACTOR"
#endif

#include "pw_lock_defs.h"
#include "pw_mem.h" // internally includes "pw_lock_defs.h"
#include "pw_data_structs.h"
#include "pw_output_buffer.h"
#include "pw_defines.h"
#include "pw_matrix.h"

/**** CONFIGURATION ****/

typedef enum {
    NON_ATOM=0,
    MFD,
    LEX,
    CLV
} atom_arch_type_t;

typedef enum {
    NON_SLM=0,
    SLM_VLV2,
    SLM_TNG,
    SLM_ANN,
    SLM_CHV
} slm_arch_type_t;

#define APWR_VERSION_CODE LINUX_VERSION_CODE

static __read_mostly atom_arch_type_t pw_is_atm = NON_ATOM;
static __read_mostly slm_arch_type_t pw_is_slm = NON_SLM;
static __read_mostly bool pw_is_hsw = false;
static __read_mostly bool pw_is_bdw = false;
static __read_mostly bool pw_is_any_thread_set = false;
static __read_mostly bool pw_is_auto_demote_enabled = false;
static __read_mostly u16 pw_msr_fsb_freq_value = 0x0;
static __read_mostly u16 pw_max_non_turbo_ratio = 0x0; // Highest non-turbo ratio i.e. TSC frequency
static __read_mostly u16 pw_max_turbo_ratio = 0x0; // Highest turbo ratio i.e. "HFM"
static __read_mostly u16 pw_max_efficiency_ratio = 0x0; // Lowest non-turbo (and non-thermal-throttled) ratio i.e. "LFM"

__read_mostly u16 pw_scu_fw_major_minor = 0x0;

/* Controls the amount of printks that happen. Levels are:
 *	- 0: no output save for errors and status at end
 *	- 1: single line for each hit (tracepoint, idle notifier...)
 *	- 2: more details
 *	- 3: user stack and kernel stack info
 */
static unsigned int verbosity = 0;

module_param(verbosity, uint, 0);
MODULE_PARM_DESC(verbosity, "Verbosity of output. From 0 to 3 with 3 the most verbose [default=0]");

/*
 * Controls whether we should be probing on
 * syscall enters and exits.
 * Useful for:
 * (1) Fork <-> Exec issues.
 * (2) Userspace <-> Kernelspace timer discrimination.
 */
static unsigned int probe_on_syscalls=0;
module_param(probe_on_syscalls, uint, 0);
MODULE_PARM_DESC(probe_on_syscalls, "Should we probe on syscall enters and exits? 1 ==> YES, 0 ==> NO (Default NO)");

/*
 * For measuring collection times.
 */
static unsigned long startJIFF, stopJIFF;

#define SUCCESS 0
#define ERROR 1

/*
 * Compile-time flags -- these affect
 * which parts of the driver get
 * compiled in.
 */
/*
 * Do we allow blocking reads?
 */
#define ALLOW_BLOCKING_READ 1
/*
 * Control whether the 'OUTPUT' macro is enabled.
 * Set to: "1" ==> 'OUTPUT' is enabled.
 *         "0" ==> 'OUTPUT' is disabled.
 */
// #define DO_DEBUG_OUTPUT 0
/*
 * Control whether to output driver ERROR messages.
 * These are independent of the 'OUTPUT' macro
 * (which controls debug messages).
 * Set to '1' ==> Print driver error messages (to '/var/log/messages')
 *        '0' ==> Do NOT print driver error messages
 */
// #define DO_PRINT_DRIVER_ERROR_MESSAGES 1
/*
 * Do we read the TSC MSR directly to determine
 * TSC (as opposed to using a kernel
 * function call -- e.g. rdtscll)?
 */
#define READ_MSR_FOR_TSC 1
/*
 * Do we support stats collection
 * for the 'PW_IOCTL_STATUS' ioctl?
 */
#define DO_IOCTL_STATS 0
/*
 * Do we check if the special 'B0' MFLD
 * microcode patch has been installed?
 * '1' ==> YES, perform the check.
 * '0' ==> NO, do NOT perform the check.
 */
#define DO_CHECK_BO_MICROCODE_PATCH 1
/*
 * Do we conduct overhead measurements?
 * '1' == > YES, conduct measurements.
 * '0' ==> NO, do NOT conduct measurements.
 */
#define DO_OVERHEAD_MEASUREMENTS 1
/*
 * Should we print some stats at the end of a collection?
 * '1' ==> YES, print stats
 * '0' ==> NO, do NOT print stats
 */
#define DO_PRINT_COLLECTION_STATS 0
/*
 * Do we keep track of IRQ # <--> DEV name mappings?
 * '1' ==> YES, cache mappings.
 * '0' ==> NO, do NOT cache mappings.
 */
#define DO_CACHE_IRQ_DEV_NAME_MAPPINGS 1
/*
 * Do we allow multiple device (names) to
 * map to the same IRQ number? Setting
 * to true makes the driver slower, if
 * more accurate.
 * '1' ==> YES, allow multi-device IRQs
 * '0' ==> NO, do NOT allow.
 */
#define DO_ALLOW_MULTI_DEV_IRQ 0
/*
 * Do we use a constant poll for wakelock names?
 * '1' ==> YES, use a constant pool.
 * '0' ==> NO, do NOT use a constant pool.
 */
#define DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES 1
/*
 * Do we use APERF, MPERF for
 * dynamic freq calculations?
 * '1' ==> YES, use APERF, MPERF
 * '0' ==> NO, use IA32_FIXED_CTR{1,2}
 */
#define USE_APERF_MPERF_FOR_DYNAMIC_FREQUENCY 1
/*
 * MSR used to toggle C-state auto demotions.
 */
#define AUTO_DEMOTE_MSR 0xe2
/*
 * Bit positions to toggle auto-demotion on NHM, ATM
 */
#define NHM_C3_AUTO_DEMOTE (1UL << 25)
#define NHM_C1_AUTO_DEMOTE (1UL << 26)
#define ATM_C6_AUTO_DEMOTE (1UL << 25)
#define AUTO_DEMOTE_FLAGS() ( pw_is_atm ? ATM_C6_AUTO_DEMOTE : (NHM_C3_AUTO_DEMOTE | NHM_C1_AUTO_DEMOTE) )
#define IS_AUTO_DEMOTE_ENABLED(msr) ( pw_is_atm ? (msr) & ATM_C6_AUTO_DEMOTE : (msr) & (NHM_C3_AUTO_DEMOTE | NHM_C1_AUTO_DEMOTE) )
/*
 * PERF_STATUS MSR addr -- bits 12:8, multiplied by the
 * bus clock freq, give the freq the H/W is currently
 * executing at.
 */
#define IA32_PERF_STATUS_MSR_ADDR 0x198
/*
 * Do we use the cpufreq notifier
 * for p-state transitions?
 * Useful on MFLD, where the default
 * TPF seems to be broken.
 */
#define DO_CPUFREQ_NOTIFIER 0
/*
 * Collect S state residency counters
 */
#define DO_S_RESIDENCY_SAMPLE 1
/*
 * Collect ACPI S3 state residency counters
 */
#define DO_ACPI_S3_SAMPLE 1
/*
 * Run the p-state sample generation in parallel for all CPUs
 * at the beginning and the end to avoid any delay
 * due to serial execution
 */
#define DO_GENERATE_CURRENT_FREQ_IN_PARALLEL 1

/*
 * Compile-time constants and
 * other macros.
 */

#define NUM_MAP_BUCKETS_BITS 9
#define NUM_MAP_BUCKETS (1UL << NUM_MAP_BUCKETS_BITS)

// 32 locks for the hash table
#define HASH_LOCK_BITS 5
#define NUM_HASH_LOCKS (1UL << HASH_LOCK_BITS)
#define HASH_LOCK_MASK (NUM_HASH_LOCKS - 1)

#define HASH_LOCK(i) LOCK(hash_locks[(i) & HASH_LOCK_MASK])
#define HASH_UNLOCK(i) UNLOCK(hash_locks[(i) & HASH_LOCK_MASK])

#define NUM_TIMER_NODES_PER_BLOCK 20

#define TIMER_HASH_FUNC(a) hash_ptr((void *)a, NUM_MAP_BUCKETS_BITS)

/* Macro for printk based on verbosity */
#if DO_DEBUG_OUTPUT
#define OUTPUT(level, ...) do { if(unlikely(level <= verbosity)) printk(__VA_ARGS__); } while(0);
#else
#define OUTPUT(level, ...)
#endif // DO_DEBUG_OUTPUT
/*
 * Macro for driver error messages.
 */
#if DO_PRINT_DRIVER_ERROR_MESSAGES
    #define pw_pr_error(...) printk(KERN_ERR __VA_ARGS__)
#else
    #define pw_pr_error(...)
#endif

#define CPU() (raw_smp_processor_id())
#define RAW_CPU() (raw_smp_processor_id())
#define TID() (current->pid)
#define PID() (current->tgid)
#define NAME() (current->comm)
#define PKG(c) ( cpu_data(c).phys_proc_id )
#define IT_REAL_INCR() (current->signal->it_real_incr.tv64)

#define GET_BOOL_STRING(b) ( (b) ? "TRUE" : "FALSE" )

#define BEGIN_IRQ_STATS_READ(p, c) do{		\
    p = &per_cpu(irq_stat, (c));

#define END_IRQ_STATS_READ(p, c)		\
    }while(0)

#define BEGIN_LOCAL_IRQ_STATS_READ(p) do{	\
    p = &__get_cpu_var(irq_stat);

#define END_LOCAL_IRQ_STATS_READ(p)		\
    }while(0)


/*
 * For now, we limit kernel-space backtraces to 20 entries.
 * This decision will be re-evaluated in the future.
 */
// #define MAX_BACKTRACE_LENGTH 20
#define MAX_BACKTRACE_LENGTH TRACE_LEN
/*
 * Is this a "root" timer?
 */
#if DO_PROBE_ON_SYSCALL_ENTER_EXIT
    #define IS_ROOT_TIMER(tid) ( (tid) == 0 || !is_tid_in_sys_list(tid) )
#else
    #define IS_ROOT_TIMER(tid) ( (tid) == 0 )
#endif // DO_PROBE_ON_SYSCALL_ENTER_EXIT
/*
 * 64bit Compare-and-swap.
 */
#define CAS64(p, o, n) cmpxchg64((p), (o), (n)) == (o)
/*
 * Local compare-and-swap.
 */
#define LOCAL_CAS(l, o, n) local_cmpxchg((l), (o), (n)) == (o)
/*
 * Record a wakeup cause (but only if we're the first non-{TPS,TPE}
 * event to occur after a wakeup.
 * @tsc: the TSC when the event occurred
 * @type: the wakeup type: one of c_break_type_t vals
 * @value: a domain-specific value
 * @cpu: the logical CPU on which the timer was initialized; specific ONLY to wakeups caused by timers!
 * @pid: PID of the process that initialized the timer for a timer-wakeup (or -1 for other wakeup events).
 * @tid: TID of the task that initialized the timer for a timer-wakeup (or -1 for other wakeup events).
 */
#define record_wakeup_cause(tsc, type, value, cpu, pid, tid) do { \
    struct wakeup_event *wu_event = &get_cpu_var(wakeup_event_counter); \
    bool is_first_wakeup_event = CAS64(&wu_event->event_tsc, 0, (tsc)); \
    if (is_first_wakeup_event) { \
        wu_event->event_val = (value); \
        wu_event->init_cpu = (cpu); \
        wu_event->event_type = (type); \
        wu_event->event_tid = (tid); \
        wu_event->event_pid = (pid); \
    } \
    put_cpu_var(wakeup_event_counter); \
} while(0)

/*
 * For NHM etc.: Base operating frequency
 * ratio is encoded in 'PLATFORM_INFO' MSR.
 */
#define PLATFORM_INFO_MSR_ADDR 0xCE
/*
 * For MFLD -- base operating frequency
 * ratio is encoded in 'CLOCK_CR_GEYSIII_STAT'
 * MSR (internal communication with Peggy Irelan)
 */
#define CLOCK_CR_GEYSIII_STAT_MSR_ADDR 0x198 // '408 decimal'
/*
 * For SLM -- base operating frequency ratio is encoded in bits 21:16
 * of 'MSR_IA32_IACORE_RATIOS' (also called PUNIT_CR_IACORE_RATIOS)
 */
#define MSR_IA32_IACORE_RATIOS 0x66a
/*
 * For SLM -- max turbo ratio is encoded in bits 4:0 of 'MSR_IA32_IACORE_TURBO_RATIOS'
 */
#define MSR_IA32_IACORE_TURBO_RATIOS 0x66c
/*
 * For "Core" -- max turbo ratio is encoded in bits
 */
#define MSR_TURBO_RATIO_LIMIT 0x1AD
/*
 * Standard Bus frequency. Valid for
 * NHM/WMR.
 * TODO: frequency for MFLD?
 */
#define BUS_CLOCK_FREQ_KHZ_NHM 133000 /* For NHM/WMR. SNB has 100000 */
#define BUS_CLOCK_FREQ_KHZ_MFLD 100000 /* For MFLD. SNB has 100000 */
/*
 * For core and later, Bus freq is encoded in 'MSR_FSB_FREQ'
 */
#define MSR_FSB_FREQ_ADDR 0xCD
/*
 * Try and determine the bus frequency.
 * Used ONLY if the user-program passed
 * us an invalid clock frequency.
 */
#define DEFAULT_BUS_CLOCK_FREQ_KHZ() ({u32 __tmp = (pw_is_atm) ? BUS_CLOCK_FREQ_KHZ_MFLD : BUS_CLOCK_FREQ_KHZ_NHM; __tmp;})
/*
 * MSRs required to enable CPU_CLK_UNHALTED.REF
 * counting.
 */
#define IA32_PERF_GLOBAL_CTRL_ADDR 0x38F
#define IA32_FIXED_CTR_CTL_ADDR 0x38D
/*
 * Standard APERF/MPERF addresses.
 * Required for dynamic freq
 * measurement.
 */
#define MPERF_MSR_ADDR 0xe7
#define APERF_MSR_ADDR 0xe8
/*
 * Fixed counter addresses.
 * Required for dynamic freq
 * measurement.
 */
#define IA32_FIXED_CTR1_ADDR 0x30A
#define IA32_FIXED_CTR2_ADDR 0x30B
/*
 * Bit positions for 'AnyThread' bits for the two
 * IA_32_FIXED_CTR{1,2} MSRs. Always '2 + 4*N'
 * where N == 1 => CTR1, N == 2 => CTR2
 */
#define IA32_FIXED_CTR1_ANYTHREAD_POS (1UL << 6)
#define IA32_FIXED_CTR2_ANYTHREAD_POS (1UL << 10)
#define ENABLE_FIXED_CTR_ANY_THREAD_MASK (IA32_FIXED_CTR1_ANYTHREAD_POS | IA32_FIXED_CTR2_ANYTHREAD_POS)
#define DISABLE_FIXED_CTR_ANY_THREAD_MASK ~ENABLE_FIXED_CTR_ANY_THREAD_MASK
#define IS_ANY_THREAD_SET(msr) ( (msr) & ENABLE_FIXED_CTR_ANY_THREAD_MASK )
/*
 * Toggle between APERF,MPERF and
 * IA32_FIXED_CTR{1,2} for Turbo.
 */
#if USE_APERF_MPERF_FOR_DYNAMIC_FREQUENCY
    #define CORE_CYCLES_MSR_ADDR APERF_MSR_ADDR
    #define REF_CYCLES_MSR_ADDR MPERF_MSR_ADDR
#else // !USE_APERF_MPERF_FOR_DYNAMIC_FREQUENCY
    #define CORE_CYCLES_MSR_ADDR IA32_FIXED_CTR1_ADDR
    #define REF_CYCLES_MSR_ADDR IA32_FIXED_CTR2_ADDR
#endif

/*
 * Size of each 'bucket' for a 'cpu_bitmap'
 */
#define NUM_BITS_PER_BUCKET (sizeof(unsigned long) * 8)
/*
 * Num 'buckets' for each 'cpu_bitmap' in the
 * 'irq_node' struct.
 */
#define NUM_BITMAP_BUCKETS ( (pw_max_num_cpus / NUM_BITS_PER_BUCKET) + 1 )
/*
 * 'cpu_bitmap' manipulation macros.
 */
#define IS_BIT_SET(bit,map) ( test_bit( (bit), (map) ) != 0 )
#define SET_BIT(bit,map) ( test_and_set_bit( (bit), (map) ) )
/*
 * Timer stats accessor macros.
 */
#ifdef CONFIG_TIMER_STATS
	#define TIMER_START_PID(t) ( (t)->start_pid )
	#define TIMER_START_COMM(t) ( (t)->start_comm )
#else
	#define TIMER_START_PID(t) (-1)
	#define TIMER_START_COMM(t) ( "UNKNOWN" )
#endif
/*
 * Helper macro to return time in usecs.
 */
#define CURRENT_TIME_IN_USEC() ({struct timeval tv; \
		do_gettimeofday(&tv);		\
		(unsigned long long)tv.tv_sec*1000000ULL + (unsigned long long)tv.tv_usec;})

#if DO_ACPI_S3_SAMPLE
static u64 startTSC_acpi_s3;
#endif
//
// Required to calculate S0i0 residency counter from non-zero S state counters
#if DO_S_RESIDENCY_SAMPLE || DO_ACPI_S3_SAMPLE
    // static u64 startJIFF_s_residency = 0;
    static u64 startTSC_s_residency = 0;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
#define SMP_CALL_FUNCTION(func,ctx,retry,wait)    smp_call_function((func),(ctx),(wait))
#else
#define SMP_CALL_FUNCTION(func,ctx,retry,wait)    smp_call_function((func),(ctx),(retry),(wait))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0)
    #define PW_HLIST_FOR_EACH_ENTRY(tpos, pos, head, member) hlist_for_each_entry(tpos, pos, head, member)
    #define PW_HLIST_FOR_EACH_ENTRY_SAFE(tpos, pos, n, head, member) hlist_for_each_entry_safe(tpos, pos, n, head, member)
    #define PW_HLIST_FOR_EACH_ENTRY_RCU(tpos, pos, head, member) hlist_for_each_entry_rcu(tpos, pos, head, member)
#else // >= 3.9.0
    #define PW_HLIST_FOR_EACH_ENTRY(tpos, pos, head, member) pos = NULL; hlist_for_each_entry(tpos, head, member)
    #define PW_HLIST_FOR_EACH_ENTRY_SAFE(tpos, pos, n, head, member) pos = NULL; hlist_for_each_entry_safe(tpos, n, head, member)
    #define PW_HLIST_FOR_EACH_ENTRY_RCU(tpos, pos, head, member) pos = NULL; hlist_for_each_entry_rcu(tpos, head, member)
#endif

#define ALLOW_WUWATCH_MSR_READ_WRITE 1
#if ALLOW_WUWATCH_MSR_READ_WRITE
    #define WUWATCH_RDMSR_ON_CPU(cpu, addr, low, high) ({int __tmp = rdmsr_on_cpu((cpu), (addr), (low), (high)); __tmp;})
    #define WUWATCH_RDMSR(addr, low, high) rdmsr((addr), (low), (high))
    #define WUWATCH_RDMSR_SAFE_ON_CPU(cpu, addr, low, high) ({int __tmp = rdmsr_safe_on_cpu((cpu), (addr), (low), (high)); __tmp;})
    #define WUWATCH_RDMSRL(addr, val) rdmsrl((addr), (val))
#else
    #define WUWATCH_RDMSR_ON_CPU(cpu, addr, low, high) ({int __tmp = 0; *(low) = 0; *(high) = 0; __tmp;})
    #define WUWATCH_RDMSR(addr, low, high) ({int __tmp = 0; (low) = 0; (high) = 0; __tmp;})
    #define WUWATCH_RDMSR_SAFE_ON_CPU(cpu, addr, low, high) ({int __tmp = 0; *(low) = 0; *(high) = 0; __tmp;})
    #define WUWATCH_RDMSRL(addr, val) ( (val) = 0 )
#endif // ALLOW_WUWATCH_MSR_READ

/*
 * Data structure definitions.
 */

typedef struct tnode tnode_t;
struct tnode{
    struct hlist_node list;
    unsigned long timer_addr;
    pid_t tid, pid;
    u64 tsc;
    s32 init_cpu;
    u16 is_root_timer : 1;
    u16 trace_sent : 1;
    u16 trace_len : 14;
    unsigned long *trace;
};

typedef struct hnode hnode_t;
struct hnode{
    struct hlist_head head;
};

typedef struct tblock tblock_t;
struct tblock{
    struct tnode *data;
    tblock_t *next;
};

typedef struct per_cpu_mem per_cpu_mem_t;
struct per_cpu_mem{
    tblock_t *block_list;
    hnode_t free_list_head;
};

#define GET_MEM_VARS(cpu) &per_cpu(per_cpu_mem_vars, (cpu))
#define GET_MY_MEM_VARS(cpu) &__get_cpu_var(per_cpu_mem_vars)

/*
 * For IRQ # <--> DEV NAME mappings.
 */
#if DO_CACHE_IRQ_DEV_NAME_MAPPINGS

typedef struct irq_node irq_node_t;
struct irq_node{
    struct hlist_node list;
    struct rcu_head rcu;
    int irq;
    char *name;
    /*
     * We send IRQ # <-> DEV name
     * mappings to Ring-3 ONCE PER
     * CPU. We need a bitmap to let
     * us know which cpus have
     * already had this info sent.
     *
     * FOR NOW, WE ASSUME A MAX OF 64 CPUS!
     * (This assumption is enforced in
     * 'init_data_structures()')
     */
    unsigned long *cpu_bitmap;
};
#define PWR_CPU_BITMAP(node) ( (node)->cpu_bitmap )

typedef struct irq_hash_node irq_hash_node_t;
struct irq_hash_node{
    struct hlist_head head;
};
#endif // DO_CACHE_IRQ_DEV_NAME_MAPPINGS


#define NUM_IRQ_MAP_BITS 6
#define NUM_IRQ_MAP_BUCKETS (1UL << NUM_IRQ_MAP_BITS)
#define IRQ_MAP_HASH_MASK (NUM_IRQ_MAP_BITS - 1)
// #define IRQ_MAP_HASH_FUNC(num) (num & IRQ_MAP_HASH_MASK)
#define IRQ_MAP_HASH_FUNC(a) hash_long((u32)a, NUM_IRQ_MAP_BITS)

#define IRQ_LOCK_MASK HASH_LOCK_MASK

#define IRQ_LOCK(i) LOCK(irq_map_locks[(i) & IRQ_LOCK_MASK])
#define IRQ_UNLOCK(i) UNLOCK(irq_map_locks[(i) & IRQ_LOCK_MASK])

#if DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES

typedef struct wlock_node wlock_node_t;
struct wlock_node{
    struct hlist_node list;
    struct rcu_head rcu;
    int constant_pool_index;
    unsigned long hash_val;
    size_t wakelock_name_len;
    char *wakelock_name;
};

typedef struct wlock_hash_node wlock_hash_node_t;
struct wlock_hash_node{
    struct hlist_head head;
};

#define NUM_WLOCK_MAP_BITS 6
#define NUM_WLOCK_MAP_BUCKETS (1UL << NUM_WLOCK_MAP_BITS)
#define WLOCK_MAP_HASH_MASK (NUM_WLOCK_MAP_BUCKETS - 1) /* Used for modulo: x % y == x & (y-1) iff y is pow-of-2 */
#define WLOCK_MAP_HASH_FUNC(n) pw_hash_string(n)

#define WLOCK_LOCK_MASK HASH_LOCK_MASK

#define WLOCK_LOCK(i) LOCK(wlock_map_locks[(i) & WLOCK_LOCK_MASK])
#define WLOCK_UNLOCK(i) UNLOCK(wlock_map_locks[(i) & WLOCK_LOCK_MASK])

#endif // DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES

/*
 * For syscall nodes
 */
typedef struct sys_node sys_node_t;
struct sys_node{
    struct hlist_node list;
    pid_t tid, pid;
    int ref_count, weight;
};

#define SYS_MAP_BUCKETS_BITS 9
#define NUM_SYS_MAP_BUCKETS (1UL << SYS_MAP_BUCKETS_BITS) // MUST be pow-of-2
#define SYS_MAP_LOCK_BITS 4
#define NUM_SYS_MAP_LOCKS (1UL << SYS_MAP_LOCK_BITS) // MUST be pow-of-2

#define SYS_MAP_NODES_HASH(t) hash_32(t, SYS_MAP_BUCKETS_BITS)
#define SYS_MAP_LOCK_HASH(t) ( (t) & (SYS_MAP_LOCK_BITS - 1) ) // pow-of-2 modulo

#define SYS_MAP_LOCK(index) LOCK(apwr_sys_map_locks[index])
#define SYS_MAP_UNLOCK(index) UNLOCK(apwr_sys_map_locks[index])

#define GET_SYS_HLIST(index) (apwr_sys_map + index)


/*
 * Function declarations (incomplete).
 */
inline bool is_sleep_syscall_i(long id) __attribute__((always_inline));
inline void sys_enter_helper_i(long id, pid_t tid, pid_t pid) __attribute__((always_inline));
inline void sys_exit_helper_i(long id, pid_t tid, pid_t pid) __attribute__((always_inline));
inline void sched_wakeup_helper_i(struct task_struct *task) __attribute__((always_inline));
static int pw_device_open(struct inode *inode, struct file *file);
static int pw_device_release(struct inode *inode, struct file *file);
static ssize_t pw_device_read(struct file *file, char __user * buffer, size_t length, loff_t * offset);
static long pw_device_unlocked_ioctl(struct file *filp, unsigned int ioctl_num, unsigned long ioctl_param);
#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
static long pw_device_compat_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param);
#endif
static long pw_unlocked_handle_ioctl_i(unsigned int ioctl_num, struct PWCollector_ioctl_arg *remote_args, unsigned long ioctl_param);
static int pw_set_platform_res_config_i(struct PWCollector_platform_res_info *remote_info, int size);
static unsigned int pw_device_poll(struct file *filp, poll_table *wait);
static int pw_device_mmap(struct file *filp, struct vm_area_struct *vma);
static int pw_register_dev(void);
static void pw_unregister_dev(void);
// static int pw_read_msr_set_i(struct msr_set *msr_set, int *which_cx, u64 *cx_val);
static int pw_read_msr_info_set_i(struct pw_msr_info_set *msr_set);
#if DO_WAKELOCK_SAMPLE
static unsigned long pw_hash_string(const char *data);
#endif // DO_WAKELOCK_SAMPLE
static int pw_init_data_structures(void);
static void pw_destroy_data_structures(void);

/*
 * Variable declarations.
 */

/*
 * Names for SOFTIRQs.
 * These are taken from "include/linux/interrupt.h"
 */
static const char *pw_softirq_to_name[] = {"HI_SOFTIRQ", "TIMER_SOFTIRQ", "NET_TX_SOFTIRQ", "NET_RX_SOFTIRQ", "BLOCK_SOFTIRQ", "BLOCK_IOPOLL_SOFTIRQ", "TASKLET_SOFTIRQ", "SCHED_SOFTIRQ", "HRTIMER_SOFTIRQ", "RCU_SOFTIRQ"};

/*
 * For microcode PATCH version.
 * ONLY useful for MFLD!
 */
static u32 __read_mostly micro_patch_ver = 0x0;

/*
 * Is the device open right now? Used to prevent
 * concurent access into the same device.
 */
#define DEV_IS_OPEN 0 // see if device is in use
static volatile unsigned long dev_status;

static struct hnode timer_map[NUM_MAP_BUCKETS];

#if DO_CACHE_IRQ_DEV_NAME_MAPPINGS
static PWCollector_irq_mapping_t *irq_mappings_list = NULL;
static irq_hash_node_t irq_map[NUM_IRQ_MAP_BUCKETS];
static int total_num_irq_mappings = 0;
#endif //  DO_CACHE_IRQ_DEV_NAME_MAPPINGS

#if DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES
static wlock_hash_node_t wlock_map[NUM_WLOCK_MAP_BUCKETS];
static int total_num_wlock_mappings = 0;
#define GET_NEXT_CONSTANT_POOL_INDEX() total_num_wlock_mappings++
#endif // DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES

DEFINE_PER_CPU(per_cpu_t, per_cpu_counts);

DEFINE_PER_CPU(stats_t, per_cpu_stats);

DEFINE_PER_CPU(CTRL_values_t, CTRL_data_values);

#ifdef __arm__
DEFINE_PER_CPU(u64, trace_power_prev_time) = 0;
#endif

static DEFINE_PER_CPU(per_cpu_mem_t, per_cpu_mem_vars);

static DEFINE_PER_CPU(u64, num_local_apic_timer_inters) = 0;

static DEFINE_PER_CPU(u32, pcpu_prev_req_freq) = 0;

static DEFINE_PER_CPU(struct msr_set, pw_pcpu_msr_sets);

static struct pw_msr_info_set *pw_pcpu_msr_info_sets ____cacheline_aligned_in_smp = NULL;

static DEFINE_PER_CPU(u32, pcpu_prev_perf_status_val) = 0;


/*
 * TPS helper -- required for overhead
 * measurements.
 */
#if DO_IOCTL_STATS
static DEFINE_PER_CPU(u64, num_inters) = 0;
#endif

/*
 * Macro to add newly allocated timer
 * nodes to individual free lists.
 */
#define LINK_FREE_TNODE_ENTRIES(nodes, size, free_head) do{		\
	int i=0;							\
	for(i=0; i<(size); ++i){					\
	    tnode_t *__node = &((nodes)[i]);				\
	    hlist_add_head(&__node->list, &((free_head)->head));	\
	}								\
    }while(0)


/*
 * Hash locks.
 */
static spinlock_t hash_locks[NUM_HASH_LOCKS];
/*
 * IRQ Map locks.
 */
#if DO_CACHE_IRQ_DEV_NAME_MAPPINGS
static spinlock_t irq_map_locks[NUM_HASH_LOCKS];
#endif
/*
 * Wakelock map locks
 */
#if DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES
static spinlock_t wlock_map_locks[NUM_HASH_LOCKS];
#endif

/*
 * Base operating frequency -- required if
 * checking turbo frequencies.
 */
static __read_mostly u32 base_operating_freq_khz = 0x0;
/*
 * Character device file MAJOR
 * number -- we're now obtaining
 * this dynamically.
 */
static int apwr_dev_major_num = -1;
/*
 * Atomic counter used to synchronize TPS probes and
 * sched wakeups on other cores.
 */
#if DO_TPS_EPOCH_COUNTER
static atomic_t tps_epoch = ATOMIC_INIT(0);
#endif // DO_TPS_EPOCH_COUNTER

/*
 * Variables to create the character device file
 */
static dev_t apwr_dev;
static struct cdev *apwr_cdev;
static struct class *apwr_class = NULL;


#if DO_OVERHEAD_MEASUREMENTS
/*
 * Counter to count # of entries
 * in the timer hash map -- used
 * for debugging.
 */
static atomic_t num_timer_entries = ATOMIC_INIT(0);
#endif
/*
 * The sys map. Individual buckets are unordered.
 */
static struct hlist_head apwr_sys_map[NUM_SYS_MAP_BUCKETS];
/*
 * Spinlock to guard updates to sys map.
 */
static spinlock_t apwr_sys_map_locks[NUM_SYS_MAP_LOCKS];
/*
 * These are used for the 'hrtimer_start(...)'
 * hack.
 */
static u32 tick_count = 0;
static DEFINE_SPINLOCK(tick_count_lock);
static bool should_probe_on_hrtimer_start = true;

DEFINE_PER_CPU(local_t, sched_timer_found) = LOCAL_INIT(0);

static DEFINE_PER_CPU(local_t, num_samples_produced) = LOCAL_INIT(0);
static DEFINE_PER_CPU(local_t, num_samples_dropped) = LOCAL_INIT(0);
/*
 * Collection time, in seconds. Specified by the user via the 'PW_IOCTL_COLLECTION_TIME'
 * ioctl. Used ONLY to decide if we should wake up the power collector after resuming
 * from an S3 (suspend) state.
 */
unsigned long pw_collection_time_secs = 0;
/*
 * Collection time, in clock ticks. Specified by the user via the 'PW_IOCTL_COLLECTION_TIME'
 * ioctl. Used ONLY to decide if we should wake up the power collector after resuming
 * from an S3 (suspend) state.
 */
u64 pw_collection_time_ticks = 0;
/*
 * Snapshot of 'TSC' time on collection START.
 */
u64 pw_collection_start_tsc = 0;
/*
 * Suspend {START, STOP} TSC ticks.
 */
u64 pw_suspend_start_tsc = 0, pw_suspend_stop_tsc = 0;
/*
 * Suspend {START, STOP} S0i3 values.
 */
u64 pw_suspend_start_s0i3 = 0, pw_suspend_stop_s0i3 = 0;
/*
 * The power collector task. Used ONLY to decide whom to send a 'SIGINT' to.
 */
struct task_struct *pw_power_collector_task = NULL;
/*
 * Timer used to defer sending SIGINT.
 * Used ONLY if the device entered ACPI S3 (aka "Suspend-To-Ram") during the
 * collection.
 */
static struct hrtimer pw_acpi_s3_hrtimer;
/*
 * Used to record which wakeup event occured first.
 * Reset on every TPS.
 */
static DEFINE_PER_CPU_SHARED_ALIGNED(struct wakeup_event, wakeup_event_counter) = {0, 0, -1, PW_BREAK_TYPE_U, -1, -1};
/*
 * Did the user mmap our buffers?
 */
static bool pw_did_mmap = false;


/*
 * MACRO helpers to measure function call
 * times.
 */
#if DO_OVERHEAD_MEASUREMENTS

#include "pw_overhead_measurements.h"

/*
 * For each function that you want to profile,
 * do the following (e.g. function 'foo'):
 * **************************************************
 * DECLARE_OVERHEAD_VARS(foo);
 * **************************************************
 * This will declare the two variables required
 * to keep track of overheads incurred in
 * calling/servicing 'foo'. Note that the name
 * that you declare here *MUST* match the function name!
 */

DECLARE_OVERHEAD_VARS(timer_init); // for the "timer_init" family of probes
DECLARE_OVERHEAD_VARS(timer_expire); // for the "timer_expire" family of probes
DECLARE_OVERHEAD_VARS(tps); // for TPS
DECLARE_OVERHEAD_VARS(tps_lite); // for TPS_lite
DECLARE_OVERHEAD_VARS(tpf); // for TPF
DECLARE_OVERHEAD_VARS(timer_insert); // for "timer_insert"
DECLARE_OVERHEAD_VARS(timer_delete); // for "timer_delete"
DECLARE_OVERHEAD_VARS(exit_helper); // for "exit_helper"
DECLARE_OVERHEAD_VARS(map_find_unlocked_i); // for "map_find_i"
DECLARE_OVERHEAD_VARS(get_next_free_node_i); // for "get_next_free_node_i"
DECLARE_OVERHEAD_VARS(ti_helper); // for "ti_helper"
DECLARE_OVERHEAD_VARS(inter_common); // for "inter_common"
DECLARE_OVERHEAD_VARS(irq_insert); // for "irq_insert"
DECLARE_OVERHEAD_VARS(find_irq_node_i); // for "find_irq_node_i"
DECLARE_OVERHEAD_VARS(wlock_insert); // for "wlock_insert"
DECLARE_OVERHEAD_VARS(find_wlock_node_i); // for "find_wlock_node_i"
DECLARE_OVERHEAD_VARS(sys_enter_helper_i);
DECLARE_OVERHEAD_VARS(sys_exit_helper_i);

/*
 * Macros to measure overheads
 */
#define DO_PER_CPU_OVERHEAD_FUNC(func, ...) do{		\
	u64 *__v = &__get_cpu_var(func##_elapsed_time);	\
	u64 tmp_1 = 0, tmp_2 = 0;			\
	local_inc(&__get_cpu_var(func##_num_iters));	\
	tscval(&tmp_1);					\
	{						\
	    func(__VA_ARGS__);				\
	}						\
	tscval(&tmp_2);					\
	*(__v) += (tmp_2 - tmp_1);			\
    }while(0)

#define DO_PER_CPU_OVERHEAD_FUNC_RET(ret, func, ...) do{	\
	u64 *__v = &__get_cpu_var(func##_elapsed_time);		\
	u64 tmp_1 = 0, tmp_2 = 0;				\
	local_inc(&__get_cpu_var(func##_num_iters));		\
	tscval(&tmp_1);						\
	{							\
	    ret = func(__VA_ARGS__);				\
	}							\
	tscval(&tmp_2);						\
	*(__v) += (tmp_2 - tmp_1);				\
    }while(0)


#else // DO_OVERHEAD_MEASUREMENTS

#define DO_PER_CPU_OVERHEAD(v, func, ...) func(__VA_ARGS__)
#define DO_PER_CPU_OVERHEAD_FUNC(func, ...) func(__VA_ARGS__)
#define DO_PER_CPU_OVERHEAD_FUNC_RET(ret, func, ...) ret = func(__VA_ARGS__)

#endif // DO_OVERHEAD_MEASUREMENTS

/*
 * File operations exported by the driver.
 */
struct file_operations Fops = {
    .open = &pw_device_open,
    .read = &pw_device_read,
    .poll = &pw_device_poll,
    // .ioctl = device_ioctl,
    .unlocked_ioctl = &pw_device_unlocked_ioctl,
#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
    .compat_ioctl = &pw_device_compat_ioctl,
#endif // COMPAT && x64
    .mmap = &pw_device_mmap,
    .release = &pw_device_release,
};

/*
 * Functions.
 */

/* Helper function to get TSC */
static inline void tscval(u64 *v)
{
    if (!v) {
        return;
    }
#ifndef __arm__
#if READ_MSR_FOR_TSC && ALLOW_WUWATCH_MSR_READ_WRITE
    {
        u64 res;
        WUWATCH_RDMSRL(0x10, res);
        *v = res;
        // printk(KERN_INFO "TSC = %llu\n", res);
    }
#else
    {
        unsigned int aux;
        rdtscpll(*v, aux);
        // printk(KERN_INFO "TSCPLL = %llu\n", *v);
    }
#endif // READ_MSR_FOR_TSC
#else
    {
        struct timespec ts;
        ktime_get_ts(&ts);
        *v = (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
    }
#endif // not def __arm__
};

/*
 * Initialization and termination routines.
 */
static void destroy_timer_map(void)
{
    /*
     * NOP: nothing to free here -- timer nodes
     * are freed when their corresponding
     * (per-cpu) blocks are freed.
     */
};

static int init_timer_map(void)
{
    int i=0;

    for(i=0; i<NUM_MAP_BUCKETS; ++i){
        INIT_HLIST_HEAD(&timer_map[i].head);
    }

    for (i=0; i<NUM_HASH_LOCKS; ++i) {
        spin_lock_init(&hash_locks[i]);
    }

    return SUCCESS;
};

#if DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES

static int init_wlock_map(void)
{
    int i=0;

    for(i=0; i<NUM_WLOCK_MAP_BUCKETS; ++i){
        INIT_HLIST_HEAD(&wlock_map[i].head);
    }

    /*
     * Init locks
     */
    for(i=0; i<NUM_HASH_LOCKS; ++i){
        spin_lock_init(&wlock_map_locks[i]);
    }

    total_num_wlock_mappings = 0;

    return SUCCESS;
};

static void wlock_destroy_node(struct wlock_node *node)
{
    if(node->wakelock_name){
	pw_kfree(node->wakelock_name);
	node->wakelock_name = NULL;
    }
    pw_kfree(node);
};

static void wlock_destroy_callback(struct rcu_head *head)
{
    struct wlock_node *node = container_of(head, struct wlock_node, rcu);

    if (node) {
        wlock_destroy_node(node);
    }
};

static void destroy_wlock_map(void)
{
    int i=0;

    for(i=0; i<NUM_WLOCK_MAP_BUCKETS; ++i){
        struct hlist_head *head = &wlock_map[i].head;
        while(!hlist_empty(head)){
            struct wlock_node *node = hlist_entry(head->first, struct wlock_node, list);
            hlist_del(&node->list);
            wlock_destroy_callback(&node->rcu);
        }
    }
    total_num_wlock_mappings  = 0;
};

#endif // DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES

#if DO_CACHE_IRQ_DEV_NAME_MAPPINGS

static int init_irq_map(void)
{
    int i=0;

    for(i=0; i<NUM_IRQ_MAP_BUCKETS; ++i){
        INIT_HLIST_HEAD(&irq_map[i].head);
    }

    /*
     * Init locks
     */
    for(i=0; i<NUM_HASH_LOCKS; ++i){
        spin_lock_init(&irq_map_locks[i]);
    }

    total_num_irq_mappings = 0;

    return SUCCESS;
};

static void irq_destroy_callback(struct rcu_head *head)
{
    struct irq_node *node = container_of(head, struct irq_node, rcu);

    if (!node) {
        return;
    }

    if(node->name){
	pw_kfree(node->name);
	node->name = NULL;
    }
    if (node->cpu_bitmap) {
        pw_kfree(node->cpu_bitmap);
        node->cpu_bitmap = NULL;
    }
    pw_kfree(node);
};

static void destroy_irq_map(void)
{
    int i=0;

    for(i=0; i<NUM_IRQ_MAP_BUCKETS; ++i){
        struct hlist_head *head = &irq_map[i].head;
        while(!hlist_empty(head)){
            struct irq_node *node = hlist_entry(head->first, struct irq_node, list);
            if (!node) {
                continue;
            }
            hlist_del(&node->list);
            irq_destroy_callback(&node->rcu);
        }
    }

    if(irq_mappings_list){
        pw_kfree(irq_mappings_list);
        irq_mappings_list = NULL;
    }
};

#endif // DO_CACHE_IRQ_DEV_NAME_MAPPINGS


static void free_timer_block(tblock_t *block)
{
    while (block) {
        tblock_t *next = block->next;
        if (block->data) {
            int i=0;
            for (i=0; i<NUM_TIMER_NODES_PER_BLOCK; ++i) {
                /*
                 * Check trace, just to be sure
                 * (We shouldn't need this -- 'timer_destroy()'
                 * explicitly checks and frees call trace
                 * arrays).
                 */
                if (block->data[i].trace) {
                    pw_kfree(block->data[i].trace);
                }
            }
            pw_kfree(block->data);
        }
        pw_kfree(block);
        block = next;
    }
    return;
};

static tblock_t *allocate_new_timer_block(struct hnode *free_head)
{
    tblock_t *block = pw_kmalloc(sizeof(tblock_t), GFP_ATOMIC);
    if(!block){
	return NULL;
    }
    block->data = pw_kmalloc(sizeof(tnode_t) * NUM_TIMER_NODES_PER_BLOCK, GFP_ATOMIC);
    if(!block->data){
	pw_kfree(block);
	return NULL;
    }
    memset(block->data, 0, sizeof(tnode_t) * NUM_TIMER_NODES_PER_BLOCK);
    if(free_head){
	LINK_FREE_TNODE_ENTRIES(block->data, NUM_TIMER_NODES_PER_BLOCK, free_head);
    }
    block->next = NULL;
    return block;
};

static void destroy_per_cpu_timer_blocks(void)
{
    int cpu = -1;

    for_each_online_cpu(cpu){
	per_cpu_mem_t *pcpu_mem = GET_MEM_VARS(cpu);
	tblock_t *blocks = pcpu_mem->block_list;
	free_timer_block(blocks);
    }
};

static int init_per_cpu_timer_blocks(void)
{
    int cpu = -1;

    for_each_online_cpu(cpu){
	per_cpu_mem_t *pcpu_mem = GET_MEM_VARS(cpu);
	struct hnode *free_head = &pcpu_mem->free_list_head;
        BUG_ON(!free_head);
	INIT_HLIST_HEAD(&free_head->head);
	if(!(pcpu_mem->block_list = allocate_new_timer_block(free_head))){
	    return -ERROR;
        }
    }

    return SUCCESS;
};


void free_sys_node_i(sys_node_t *node)
{
    if (!node) {
	return;
    }
    pw_kfree(node);
};

sys_node_t *alloc_new_sys_node_i(pid_t tid, pid_t pid)
{
    sys_node_t *node = pw_kmalloc(sizeof(sys_node_t), GFP_ATOMIC);
    if (!node) {
	pw_pr_error("ERROR: could NOT allocate new sys node!\n");
	return NULL;
    }
    node->tid = tid; node->pid = pid;
    node->ref_count = node->weight = 1;
    INIT_HLIST_NODE(&node->list);
    return node;
};

int destroy_sys_list(void)
{
    int size = 0, i=0;

    for (i=0; i<NUM_SYS_MAP_BUCKETS; ++i) {
	struct hlist_head *apwr_sys_list = GET_SYS_HLIST(i);
	int tmp_size = 0;
	while (!hlist_empty(apwr_sys_list)) {
	    sys_node_t *node = hlist_entry(apwr_sys_list->first, struct sys_node, list);
	    hlist_del(&node->list);
	    ++tmp_size;
	    free_sys_node_i(node);
	    ++size;
	}
	if (tmp_size) {
	    OUTPUT(3, KERN_INFO "[%d] --> %d\n", i, tmp_size);
	}
    }

#if DO_PRINT_COLLECTION_STATS
    printk(KERN_INFO "SYS_LIST_SIZE = %d\n", size);
#endif

    return SUCCESS;
};

int init_sys_list(void)
{
    int i=0;

    for (i=0; i<NUM_SYS_MAP_BUCKETS; ++i) {
	INIT_HLIST_HEAD(GET_SYS_HLIST(i));
    }

    for (i=0; i<NUM_SYS_MAP_LOCKS; ++i) {
	spin_lock_init(apwr_sys_map_locks + i);
    }

    return SUCCESS;
};

/*
 * MSR info set alloc/dealloc routines.
 */
static void pw_reset_msr_info_sets(void)
{
    int cpu = -1;
    if (likely(pw_pcpu_msr_info_sets)) {
        for_each_possible_cpu(cpu) {
            struct pw_msr_info_set *info_set = pw_pcpu_msr_info_sets + cpu;
            if (likely(info_set->prev_msr_vals)) {
                pw_kfree(info_set->prev_msr_vals);
            }
            if (likely(info_set->curr_msr_count)) {
                pw_kfree(info_set->curr_msr_count);
            }
            if (likely(info_set->c_multi_msg_mem)) {
                pw_kfree(info_set->c_multi_msg_mem);
            }
            memset(info_set, 0, sizeof(*info_set));
        }
    }
};

static void pw_destroy_msr_info_sets(void)
{
    if (likely(pw_pcpu_msr_info_sets)) {
        pw_reset_msr_info_sets();
        pw_kfree(pw_pcpu_msr_info_sets);
        pw_pcpu_msr_info_sets = NULL;
    }
};

static int pw_init_msr_info_sets(void)
{
    BUG_ON(pw_max_num_cpus <= 0);
    pw_pcpu_msr_info_sets = pw_kmalloc(sizeof(struct pw_msr_info_set) * pw_max_num_cpus, GFP_KERNEL);
    if (!pw_pcpu_msr_info_sets) {
        pw_pr_error("ERROR allocating space for info sets!\n");
        return -ERROR;
    }
    memset(pw_pcpu_msr_info_sets, 0, sizeof(struct pw_msr_info_set) * pw_max_num_cpus);
    return SUCCESS;
};


static void pw_destroy_data_structures(void)
{
    destroy_timer_map();

    destroy_per_cpu_timer_blocks();

#if DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES
    destroy_wlock_map();
#endif // DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES

#if DO_CACHE_IRQ_DEV_NAME_MAPPINGS
    destroy_irq_map();
#endif // DO_CACHE_IRQ_DEV_NAME_MAPPINGS


    destroy_sys_list();

    pw_destroy_per_cpu_buffers();

    pw_destroy_msr_info_sets();

    {
        /*
         * Print some stats about # samples produced and # dropped.
         */
#if DO_PRINT_COLLECTION_STATS
        printk(KERN_INFO "DEBUG: There were %llu / %llu dropped samples!\n", pw_num_samples_dropped, pw_num_samples_produced);
#endif
    }
};

static int pw_init_data_structures(void)
{
    /*
     * Find the # CPUs in this system.
     */
    // pw_max_num_cpus = num_online_cpus();
    pw_max_num_cpus = num_possible_cpus();

    /*
     * Init the (per-cpu) free lists
     * for timer mappings.
     */
    if(init_per_cpu_timer_blocks()){
        pw_pr_error("ERROR: could NOT initialize the per-cpu timer blocks!\n");
        pw_destroy_data_structures();
        return -ERROR;
    }

    if(init_timer_map()){
        pw_pr_error("ERROR: could NOT initialize timer map!\n");
        pw_destroy_data_structures();
        return -ERROR;
    }

#if DO_CACHE_IRQ_DEV_NAME_MAPPINGS
    if(init_irq_map()){
        pw_pr_error("ERROR: could NOT initialize irq map!\n");
        pw_destroy_data_structures();
        return -ERROR;
    }
#endif // DO_CACHE_IRQ_DEV_NAME_MAPPINGS

#if DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES
    if (init_wlock_map()) {
        pw_pr_error("ERROR: could NOT initialize wlock map!\n");
        pw_destroy_data_structures();
        return -ERROR;
    }
#endif // DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES

    if (init_sys_list()) {
        pw_pr_error("ERROR: could NOT initialize syscall map!\n");
        pw_destroy_data_structures();
        return -ERROR;
    }

    if (pw_init_per_cpu_buffers()) {
        pw_pr_error("ERROR initializing per-cpu output buffers\n");
        pw_destroy_data_structures();
        return -ERROR;
    }

    if (pw_init_msr_info_sets()) {
        pw_pr_error("ERROR initializing MSR info sets\n");
        pw_destroy_data_structures();
        return -ERROR;
    }

    return SUCCESS;
};

/*
 * Free list manipulation routines.
 */

static int init_tnode_i(tnode_t *node, unsigned long timer_addr, pid_t tid, pid_t pid, u64 tsc, s32 init_cpu, int trace_len, unsigned long *trace)
{

    if (!node) {
        return -ERROR;
    }

    if(node->trace){
        pw_kfree(node->trace);
        node->trace = NULL;
    }

    node->timer_addr = timer_addr; node->tsc = tsc; node->tid = tid; node->pid = pid; node->init_cpu = init_cpu; node->trace_sent = 0; node->trace_len = trace_len;

    if(trace_len >  0){
        /*
         * Root timer!
         */
        node->is_root_timer = 1;
        node->trace = pw_kmalloc(sizeof(unsigned long) * trace_len, GFP_ATOMIC);
        if(!node->trace){
            pw_pr_error("ERROR: could NOT allocate memory for backtrace!\n");
            // pw_kfree(node);
            return -ERROR;
        }
        memcpy(node->trace, trace, sizeof(unsigned long) * trace_len); // dst, src
    }

    /*
     * Ensure everyone sees this...
     */
    smp_mb();

    return SUCCESS;
};

static tnode_t *get_next_free_tnode_i(unsigned long timer_addr, pid_t tid, pid_t pid, u64 tsc, s32 init_cpu, int trace_len, unsigned long *trace)
{
    per_cpu_mem_t *pcpu_mem = GET_MY_MEM_VARS();
    struct hnode *free_head = &pcpu_mem->free_list_head;
    struct hlist_head *head = &free_head->head;

    if(hlist_empty(head)){
	tblock_t *block = allocate_new_timer_block(free_head);
	if(block){
	    block->next = pcpu_mem->block_list;
	    pcpu_mem->block_list = block;
	}
	OUTPUT(3, KERN_INFO "[%d]: ALLOCATED A NEW TIMER BLOCK!\n", CPU());
    }

    if(!hlist_empty(head)){
	struct tnode *node = hlist_entry(head->first, struct tnode, list);
	hlist_del(&node->list);
	/*
	 * 'kmalloc' doesn't zero out memory -- set
	 * 'trace' to NULL to avoid an invalid
	 * 'free' in 'init_tnode_i(...)' just to
	 * be sure (Shouldn't need to have to
	 * do this -- 'destroy_timer()' *should*
	 * have handled it for us).
	 */
	node->trace = NULL;

	if(init_tnode_i(node, timer_addr, tid, pid, tsc, init_cpu, trace_len, trace)){
	    /*
	     * Backtrace couldn't be inited -- re-enqueue
	     * onto the free-list.
	     */
	    node->trace = NULL;
	    hlist_add_head(&node->list, head);
	    return NULL;
	}
	return node;
    }
    return NULL;
};

static void timer_destroy(struct tnode *node)
{
    per_cpu_mem_t *pcpu_mem = GET_MY_MEM_VARS();
    struct hnode *free_head = &pcpu_mem->free_list_head;

    if (!node) {
        return;
    }

    OUTPUT(3, KERN_INFO "DESTROYING %p\n", node);

    if(node->trace){
	pw_kfree(node->trace);
	node->trace = NULL;
    }

    hlist_add_head(&node->list, &((free_head)->head));
};

/*
 * Hash map routines.
 */

static tnode_t *timer_find(unsigned long timer_addr, pid_t tid)
{
    int idx = TIMER_HASH_FUNC(timer_addr);
    tnode_t *node = NULL, *retVal = NULL;
    struct hlist_node *curr = NULL;
    struct hlist_head *head = NULL;

    HASH_LOCK(idx);
    {
	head = &timer_map[idx].head;

        PW_HLIST_FOR_EACH_ENTRY(node, curr, head, list) {
	    if(node->timer_addr == timer_addr && (node->tid == tid || tid < 0)){
		retVal = node;
		break;
	    }
	}
    }
    HASH_UNLOCK(idx);

    return retVal;
};


static void timer_insert(unsigned long timer_addr, pid_t tid, pid_t pid, u64 tsc, s32 init_cpu, int trace_len, unsigned long *trace)
{
    int idx = TIMER_HASH_FUNC(timer_addr);
    struct hlist_node *curr = NULL;
    struct hlist_head *head = NULL;
    struct tnode *node = NULL, *new_node = NULL;
    bool found = false;

    HASH_LOCK(idx);
    {
        head = &timer_map[idx].head;

        PW_HLIST_FOR_EACH_ENTRY(node, curr, head, list) {
            if(node->timer_addr == timer_addr){
                /*
                 * Update-in-place.
                 */
                OUTPUT(3, KERN_INFO "Timer %p UPDATING IN PLACE! Node = %p, Trace = %p\n", (void *)timer_addr, node, node->trace);
                init_tnode_i(node, timer_addr, tid, pid, tsc, init_cpu, trace_len, trace);
                found = true;
                break;
            }
        }

        if(!found){
            /*
             * Insert a new entry here.
             */
	    new_node = get_next_free_tnode_i(timer_addr, tid, pid, tsc, init_cpu, trace_len, trace);
            if(likely(new_node)){
                hlist_add_head(&new_node->list, &timer_map[idx].head);
#if DO_OVERHEAD_MEASUREMENTS
                {
                    smp_mb();
                    atomic_inc(&num_timer_entries);
                }
#endif
            }else{ // !new_node
                pw_pr_error("ERROR: could NOT allocate new timer node!\n");
            }
        }
    }
    HASH_UNLOCK(idx);

    return;
};

static int timer_delete(unsigned long timer_addr, pid_t tid)
{
    int idx = TIMER_HASH_FUNC(timer_addr);
    tnode_t *node = NULL, *found_node = NULL;
    struct hlist_node *curr = NULL, *next = NULL;
    struct hlist_head *head = NULL;
    int retVal = -ERROR;

    HASH_LOCK(idx);
    {
	head = &timer_map[idx].head;

        PW_HLIST_FOR_EACH_ENTRY_SAFE(node, curr, next, head, list) {
	    // if(node->timer_addr == timer_addr && node->tid == tid){
            if(node->timer_addr == timer_addr) {
                if (node->tid != tid){
                    OUTPUT(0, KERN_INFO "WARNING: stale timer tid value? node tid = %d, task tid = %d\n", node->tid, tid);
		}
		hlist_del(&node->list);
		found_node = node;
		retVal = SUCCESS;
		OUTPUT(3, KERN_INFO "[%d]: TIMER_DELETE FOUND HRT = %p\n", tid, (void *)timer_addr);
		break;
	    }
	}
    }
    HASH_UNLOCK(idx);

    if(found_node){
	timer_destroy(found_node);
    }

    return retVal;
};

static void delete_all_non_kernel_timers(void)
{
    struct tnode *node = NULL;
    struct hlist_node *curr = NULL, *next = NULL;
    int i=0, num_timers = 0;

    for(i=0; i<NUM_MAP_BUCKETS; ++i)
	{
	    HASH_LOCK(i);
	    {
                PW_HLIST_FOR_EACH_ENTRY_SAFE(node, curr, next, &timer_map[i].head, list) {
                    if (node->is_root_timer == 0) {
			++num_timers;
			OUTPUT(3, KERN_INFO "[%d]: Timer %p (Node %p) has TRACE = %p\n", node->tid, (void *)node->timer_addr, node, node->trace);
			hlist_del(&node->list);
			timer_destroy(node);
		    }
		}
	    }
	    HASH_UNLOCK(i);
	}
};


static void delete_timers_for_tid(pid_t tid)
{
    struct tnode *node = NULL;
    struct hlist_node *curr = NULL, *next = NULL;
    int i=0, num_timers = 0;

    for(i=0; i<NUM_MAP_BUCKETS; ++i)
	{
	    HASH_LOCK(i);
	    {
                PW_HLIST_FOR_EACH_ENTRY_SAFE(node, curr, next, &timer_map[i].head, list) {
		    if(node->is_root_timer == 0 && node->tid == tid){
			++num_timers;
			OUTPUT(3, KERN_INFO "[%d]: Timer %p (Node %p) has TRACE = %p\n", tid, (void *)node->timer_addr, node, node->trace);
			hlist_del(&node->list);
			timer_destroy(node);
		    }
		}
	    }
	    HASH_UNLOCK(i);
	}

    OUTPUT(3, KERN_INFO "[%d]: # timers = %d\n", tid, num_timers);
};

static int get_num_timers(void)
{
    tnode_t *node = NULL;
    struct hlist_node *curr = NULL;
    int i=0, num=0;


    for (i=0; i<NUM_MAP_BUCKETS; ++i) {
        PW_HLIST_FOR_EACH_ENTRY(node, curr, &timer_map[i].head, list) {
	    ++num;
	    OUTPUT(3, KERN_INFO "[%d]: %d --> %p\n", i, node->tid, (void *)node->timer_addr);
	}
    }

    return num;
};

#if DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES

#if DO_WAKELOCK_SAMPLE
static unsigned long pw_hash_string(const char *data)
{
    unsigned long hash = 0;
    unsigned char c;
    char *str = (char *)data;

    BUG_ON(!data);

    while ((c = *str++)) {
        hash = c + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
};

static wlock_node_t *get_next_free_wlock_node_i(unsigned long hash, size_t wlock_name_len, const char *wlock_name)
{
    wlock_node_t *node = pw_kmalloc(sizeof(wlock_node_t), GFP_ATOMIC);

    if (likely(node)) {
	memset(node, 0, sizeof(wlock_node_t));
	node->hash_val = hash;

	INIT_HLIST_NODE(&node->list);

	if( !(node->wakelock_name = pw_kstrdup(wlock_name, GFP_ATOMIC))){
	    pw_pr_error("ERROR: could NOT kstrdup wlock device name: %s\n", wlock_name);
	    pw_kfree(node);
	    node = NULL;
	} else {
            node->wakelock_name_len = wlock_name_len;
        }
    } else {
	pw_pr_error("ERROR: could NOT allocate new wlock node!\n");
    }

    return node;
};

/*
 * Check if the given wlock # <-> DEV Name mapping exists and, if
 * it does, whether this mapping was sent for the given 'cpu'
 * (We need to send each such mapping ONCE PER CPU to ensure it is
 * received BEFORE a corresponding wlock C-state wakeup).
 */
static int find_wlock_node_i(unsigned long hash, size_t wlock_name_len, const char *wlock_name)
{
    wlock_node_t *node = NULL;
    struct hlist_node *curr = NULL;
    int idx = hash & WLOCK_MAP_HASH_MASK;
    int cp_index = -1;

    rcu_read_lock();
    {
        PW_HLIST_FOR_EACH_ENTRY_RCU (node, curr, &wlock_map[idx].head, list) {
            //printk(KERN_INFO "hash_val = %lu, name = %s, cp_index = %d\n", node->hash_val, node->wakelock_name, node->constant_pool_index);
            if (node->hash_val == hash && node->wakelock_name_len == wlock_name_len && !strcmp(node->wakelock_name, wlock_name)) {
                cp_index = node->constant_pool_index;
                break;
            }
        }
    }
    rcu_read_unlock();

    return cp_index;
};

static pw_mapping_type_t wlock_insert(size_t wlock_name_len, const char *wlock_name, int *cp_index)
{
    wlock_node_t *node = NULL;
    unsigned long hash = WLOCK_MAP_HASH_FUNC(wlock_name);
    pw_mapping_type_t retVal = PW_MAPPING_ERROR;

    if (!wlock_name || !cp_index) {
        pw_pr_error("ERROR: NULL name/index?!\n");
        return PW_MAPPING_ERROR;
    }

    *cp_index = find_wlock_node_i(hash, wlock_name_len, wlock_name);

    //printk(KERN_INFO "wlock_insert: cp_index = %d, name = %s\n", *cp_index, wlock_name);

    if (*cp_index >= 0) {
        /*
         * Mapping FOUND!
         */
        //printk(KERN_INFO "OK: mapping already exists for %s (cp_index = %d)\n", wlock_name, *cp_index);
        return PW_MAPPING_EXISTS;
    }

    node = get_next_free_wlock_node_i(hash, wlock_name_len, wlock_name);

    if (unlikely(node == NULL)) {
	pw_pr_error("ERROR: could NOT allocate node for wlock insertion!\n");
	return PW_MAPPING_ERROR;
    }

    WLOCK_LOCK(hash);
    {
        int idx = hash & WLOCK_MAP_HASH_MASK;
        wlock_node_t *old_node = NULL;
        struct hlist_node *curr = NULL;
        /*
         * It is THEORETICALLY possible that the same wakelock name was passed to 'acquire' twice and that
         * a different process inserted an entry into the wakelock after our check and before we could insert
         * (i.e. a race condition). Check for that first.
         */
        PW_HLIST_FOR_EACH_ENTRY(old_node, curr, &wlock_map[idx].head, list) {
            if (old_node->hash_val == hash && old_node->wakelock_name_len == wlock_name_len && !strcmp(old_node->wakelock_name, wlock_name)) {
                *cp_index = old_node->constant_pool_index;
                //printk(KERN_INFO "wlock mapping EXISTS: cp_index = %d, name = %s\n", *cp_index, wlock_name);
                break;
            }
        }
        if (likely(*cp_index < 0)) {
            /*
             * OK: insert a new node.
             */
            *cp_index = node->constant_pool_index = GET_NEXT_CONSTANT_POOL_INDEX();
	    hlist_add_head_rcu(&node->list, &wlock_map[idx].head);
            retVal = PW_NEW_MAPPING_CREATED;
            //printk(KERN_INFO "CREATED new wlock mapping: cp_index = %d, name = %s\n", *cp_index, wlock_name);
        } else {
            /*
             * Hmnnn ... a race condition. Warn because this is very unlikely!
             */
            //printk(KERN_INFO "WARNING: race condition detected for wlock insert for node %s\n", wlock_name);
            wlock_destroy_node(node);
            retVal = PW_MAPPING_EXISTS;
        }
    }
    WLOCK_UNLOCK(hash);

    return retVal;
};
#endif // DO_WAKELOCK_SAMPLE

/*
 * INTERNAL HELPER: retrieve number of
 * mappings in the wlock mappings list.
 */
#if 0
#ifndef __arm__
static int get_num_wlock_mappings(void)
{
    int retVal = 0;
    int i=0;
    wlock_node_t *node = NULL;
    struct hlist_node *curr = NULL;

    for(i=0; i<NUM_WLOCK_MAP_BUCKETS; ++i)
        PW_HLIST_FOR_EACH_ENTRY(node, curr, &wlock_map[i].head, list) {
	    ++retVal;
	    OUTPUT(0, KERN_INFO "[%d]: wlock Num=%d, Dev=%s\n", i, node->wlock, node->name);
	}

    return retVal;

};
#endif // __arm__
#endif // 0
#endif // DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES

/*
 * IRQ list manipulation routines.
 */
#if DO_CACHE_IRQ_DEV_NAME_MAPPINGS


static irq_node_t *get_next_free_irq_node_i(int cpu, int irq_num, const char *irq_name)
{
    irq_node_t *node = pw_kmalloc(sizeof(irq_node_t), GFP_ATOMIC);

    if (likely(node)) {
	memset(node, 0, sizeof(irq_node_t));
	node->irq = irq_num;
	/*
	 * Set current CPU bitmap.
	 */
        node->cpu_bitmap = pw_kmalloc(sizeof(unsigned long) * NUM_BITMAP_BUCKETS, GFP_ATOMIC);
        if (unlikely(!node->cpu_bitmap)) {
            pw_pr_error("ERROR: could NOT allocate a bitmap for the new irq_node!\n");
            pw_kfree(node);
            return NULL;
        }
        memset(node->cpu_bitmap, 0, sizeof(unsigned long) * NUM_BITMAP_BUCKETS);
        SET_BIT(cpu, PWR_CPU_BITMAP(node));

	INIT_HLIST_NODE(&node->list);

	if( !(node->name = pw_kstrdup(irq_name, GFP_ATOMIC))){
	    pw_pr_error("ERROR: could NOT kstrdup irq device name: %s\n", irq_name);
            pw_kfree(node->cpu_bitmap);
	    pw_kfree(node);
	    node = NULL;
	}
    } else {
	pw_pr_error("ERROR: could NOT allocate new irq node!\n");
    }

    return node;

};

/*
 * Check if the given IRQ # <-> DEV Name mapping exists and, if
 * it does, whether this mapping was sent for the given 'cpu'
 * (We need to send each such mapping ONCE PER CPU to ensure it is
 * received BEFORE a corresponding IRQ C-state wakeup).
 */
static bool find_irq_node_i(int cpu, int irq_num, const char *irq_name, int *index, bool *was_mapping_sent)
{
    irq_node_t *node = NULL;
    struct hlist_node *curr = NULL;
    int idx = IRQ_MAP_HASH_FUNC(irq_num);

    *index = idx;

    rcu_read_lock();

    PW_HLIST_FOR_EACH_ENTRY_RCU (node, curr, &irq_map[idx].head, list) {
	if(node->irq == irq_num
#if DO_ALLOW_MULTI_DEV_IRQ
	   && !strcmp(node->name, irq_name)
#endif // DO_ALLOW_MULTI_DEV_IRQ
	   )
	    {
		/*
		 * OK, so the maping exists. But each
		 * such mapping must be sent ONCE PER
		 * CPU to Ring-3 -- have we done so
		 * for this cpu?
		 */
		// *was_mapping_sent = (node->cpu_bitmap & (1 << cpu)) ? true : false;
                *was_mapping_sent = (IS_BIT_SET(cpu, PWR_CPU_BITMAP(node))) ? true : false;
		rcu_read_unlock();
		return true;
	    }
    }

    rcu_read_unlock();
    return false;
};

/*
 * Check to see if a given IRQ # <-> DEV Name mapping exists
 * in our list of such mappings and, if it does, whether this
 * mapping has been sent to Ring-3. Take appropriate actions
 * if any of these conditions is not met.
 */
static irq_mapping_types_t irq_insert(int cpu, int irq_num, const char *irq_name)
{
    irq_node_t *node = NULL;
    int idx = -1;
    bool found_mapping = false, mapping_sent = false;

    if (!irq_name) {
        pw_pr_error("ERROR: NULL IRQ name?!\n");
        return ERROR_IRQ_MAPPING;
    }
    /*
     * Protocol:
     * (a) if mapping FOUND: return "OK_IRQ_MAPPING_EXISTS"
     * (b) if new mapping CREATED: return "OK_NEW_IRQ_MAPPING_CREATED"
     * (c) if ERROR: return "ERROR_IRQ_MAPPING"
     */

    found_mapping = find_irq_node_i(cpu, irq_num, irq_name, &idx, &mapping_sent);
    if(found_mapping && mapping_sent){
	/*
	 * OK, mapping exists AND we've already
	 * sent the mapping for this CPU -- nothing
	 * more to do.
	 */
	return OK_IRQ_MAPPING_EXISTS;
    }

    /*
     * Either this mapping didn't exist at all,
     * or the mapping wasn't sent for this CPU.
     * In either case, because we're using RCU,
     * we'll have to allocate a new node.
     */

    node = get_next_free_irq_node_i(cpu, irq_num, irq_name);

    if(unlikely(node == NULL)){
	pw_pr_error("ERROR: could NOT allocate node for irq insertion!\n");
	return ERROR_IRQ_MAPPING;
    }

    IRQ_LOCK(idx);
    {
	/*
	 * It is *THEORETICALLY* possible that
	 * a different CPU added this IRQ entry
	 * to the 'irq_map'. For now, disregard
	 * the possiblility (at worst we'll have
	 * multiple entries with the same mapping,
	 * which is OK).
	 */
	bool found = false;
	irq_node_t *old_node = NULL;
	struct hlist_node *curr = NULL;
	if(found_mapping){
            PW_HLIST_FOR_EACH_ENTRY(old_node, curr, &irq_map[idx].head, list) {
		if(old_node->irq == irq_num
#if DO_ALLOW_MULTI_DEV_IRQ
		   && !strcmp(old_node->name, irq_name)
#endif // DO_ALLOW_MULTI_DEV_IRQ
		   )
		    {
			/*
			 * Found older entry -- copy the 'cpu_bitmap'
			 * field over to the new entry (no need to set this
			 * CPU's entry -- 'get_next_free_irq_node_i() has
			 * already done that. Instead, do a BITWISE OR of
			 * the old and new bitmaps)...
			 */
			OUTPUT(0, KERN_INFO "[%d]: IRQ = %d, OLD bitmap = %lu\n", cpu, irq_num, *(old_node->cpu_bitmap));
			// node->cpu_bitmap |= old_node->cpu_bitmap;
                        /*
                         * UPDATE: new 'bitmap' scheme -- copy over the older
                         * bitmap array...
                         */
                        memcpy(node->cpu_bitmap, old_node->cpu_bitmap, sizeof(unsigned long) * NUM_BITMAP_BUCKETS); // dst, src
                        /*
                         * ...then set the current CPU's pos in the 'bitmap'
                         */
                        SET_BIT(cpu, node->cpu_bitmap);
			/*
			 * ...and then replace the old node with
			 * the new one.
			 */
			hlist_replace_rcu(&old_node->list, &node->list);
			call_rcu(&old_node->rcu, &irq_destroy_callback);
			/*
			 * OK -- everything done.
			 */
			found = true;
			break;
		    }
	    }
	    if(!found){
		pw_pr_error("ERROR: CPU = %d, IRQ = %d, mapping_found but not found!\n", cpu, irq_num);
	    }
	}else{
	    hlist_add_head_rcu(&node->list, &irq_map[idx].head);
	    /*
	     * We've added a new mapping.
	     */
	    ++total_num_irq_mappings;
	}
    }
    IRQ_UNLOCK(idx);
    /*
     * Tell caller that this mapping
     * should be sent to Ring-3.
     */
    return OK_NEW_IRQ_MAPPING_CREATED;
};

/*
 * INTERNAL HELPER: retrieve number of
 * mappings in the IRQ mappings list.
 */
static int get_num_irq_mappings(void)
{
    int retVal = 0;
    int i=0;
    irq_node_t *node = NULL;
    struct hlist_node *curr = NULL;

    for (i=0; i<NUM_IRQ_MAP_BUCKETS; ++i) {
        PW_HLIST_FOR_EACH_ENTRY(node, curr, &irq_map[i].head, list) {
	    ++retVal;
	    OUTPUT(0, KERN_INFO "[%d]: IRQ Num=%d, Dev=%s\n", i, node->irq, node->name);
	}
    }

    return retVal;

};

#endif // DO_CACHE_IRQ_DEV_NAME_MAPPINGS

/*
 * SYS map manipulation routines.
 */

inline bool is_tid_in_sys_list(pid_t tid)
{
    sys_node_t *node = NULL;
    struct hlist_node *curr = NULL;
    bool found = false;

    int hindex = SYS_MAP_NODES_HASH(tid);
    int lindex = SYS_MAP_LOCK_HASH(tid);

    SYS_MAP_LOCK(lindex);
    {
	struct hlist_head *apwr_sys_list = GET_SYS_HLIST(hindex);
        PW_HLIST_FOR_EACH_ENTRY(node, curr, apwr_sys_list, list) {
	    if (node->tid == tid) {
		found = true;
		break;
	    }
	}
    }
    SYS_MAP_UNLOCK(lindex);

    return found;
};

inline int check_and_remove_proc_from_sys_list(pid_t tid, pid_t pid)
{
    sys_node_t *node = NULL;
    struct hlist_node *curr = NULL;
    bool found = false;
    int hindex = SYS_MAP_NODES_HASH(tid);
    int lindex = SYS_MAP_LOCK_HASH(tid);

    SYS_MAP_LOCK(lindex);
    {
	struct hlist_head *apwr_sys_list = GET_SYS_HLIST(hindex);
        PW_HLIST_FOR_EACH_ENTRY(node, curr, apwr_sys_list, list) {
	    if (node->tid == tid && node->ref_count > 0) {
		found = true;
		--node->ref_count;
		break;
	    }
	}
    }
    SYS_MAP_UNLOCK(lindex);

    if (!found) {
	return -ERROR;
    }
    return SUCCESS;
};

inline int check_and_delete_proc_from_sys_list(pid_t tid, pid_t pid)
{
    sys_node_t *node = NULL;
    bool found = false;
    struct hlist_node *curr = NULL;
    int hindex = SYS_MAP_NODES_HASH(tid);
    int lindex = SYS_MAP_LOCK_HASH(tid);

    SYS_MAP_LOCK(lindex);
    {
	struct hlist_head *apwr_sys_list = GET_SYS_HLIST(hindex);
        PW_HLIST_FOR_EACH_ENTRY(node, curr, apwr_sys_list, list) {
	    if (node->tid == tid) {
		found = true;
		hlist_del(&node->list);
                OUTPUT(3, KERN_INFO "CHECK_AND_DELETE: successfully deleted node: tid = %d, ref_count = %d, weight = %d\n", tid, node->ref_count, node->weight);
		free_sys_node_i(node);
		break;
	    }
	}
    }
    SYS_MAP_UNLOCK(lindex);

    if (!found) {
	return -ERROR;
    }
    return SUCCESS;
};

inline int check_and_add_proc_to_sys_list(pid_t tid, pid_t pid)
{
    sys_node_t *node = NULL;
    bool found = false;
    int retVal = SUCCESS;
    struct hlist_node *curr = NULL;
    int hindex = SYS_MAP_NODES_HASH(tid);
    int lindex = SYS_MAP_LOCK_HASH(tid);

    SYS_MAP_LOCK(lindex);
    {
	struct hlist_head *apwr_sys_list = GET_SYS_HLIST(hindex);
        PW_HLIST_FOR_EACH_ENTRY(node, curr, apwr_sys_list, list) {
	    if (node->tid == tid) {
		found = true;
		++node->ref_count;
		++node->weight;
		break;
	    }
	}
        if (!found){
	    node = alloc_new_sys_node_i(tid, pid);
	    if (!node) {
		pw_pr_error("ERROR: could NOT allocate new node!\n");
		retVal = -ERROR;
	    } else {
		hlist_add_head(&node->list, apwr_sys_list);
	    }
        }
    }
    SYS_MAP_UNLOCK(lindex);
    return retVal;
};


void print_sys_node_i(sys_node_t *node)
{
    printk(KERN_INFO "SYS_NODE: %d -> %d, %d\n", node->tid, node->ref_count, node->weight);
};


/*
 * HELPER template function to illustrate
 * how to 'produce' data into the
 * (per-cpu) output buffers.
 */
static inline void producer_template(int cpu)
{
    /*
     * Template for any of the 'produce_XXX_sample(...)'
     * functions.
     */
    struct PWCollector_msg msg;
    bool should_wakeup = true; // set to FALSE if calling from scheduling context (e.g. from "sched_wakeup()")
    msg.data_len = 0;

    // Populate 'sample' fields in a domain-specific
    // manner. e.g.:
    // sample.foo = bar
    /*
     * OK, computed 'sample' fields. Now
     * write sample into the output buffer.
     */
    pw_produce_generic_msg(&msg, should_wakeup);
};

#if DO_ACPI_S3_SAMPLE
/*
 * Insert a ACPI S3 Residency counter sample into a (per-cpu) output buffer.
 */
static inline void produce_acpi_s3_sample(u64 tsc, u64 s3_res)
{
    int cpu = raw_smp_processor_id();

    PWCollector_msg_t msg;
    s_residency_sample_t sres;

    /*
     * No residency counters available
     */
    msg.data_type = ACPI_S3;
    msg.cpuidx = cpu;
    msg.tsc = tsc;
    msg.data_len = sizeof(sres);

    /*
    if (startTSC_acpi_s3 == 0) {
        startTSC_acpi_s3 = tsc;
    }

    if (s3flag) {
        sres.data[0] = 0;
        sres.data[1] = s3_res; // tsc - startTSC_acpi_s3;
    } else {
        sres.data[0] = tsc - startTSC_acpi_s3;
        sres.data[1] = 0;
    }
    */
    pw_pr_debug("GU: start tsc = %llu, tsc = %llu, s3_res = %llu\n", startTSC_acpi_s3, tsc, s3_res);

    if (startTSC_acpi_s3 == 0 || s3_res > 0) {
        startTSC_acpi_s3 = tsc;
    }
    sres.data[0] = tsc - startTSC_acpi_s3;
    sres.data[1] = s3_res;

    startTSC_acpi_s3 = tsc;

    msg.p_data = (u64)((unsigned long)(&sres));

    /*
     * OK, everything computed. Now copy
     * this sample into an output buffer
     */
    pw_produce_generic_msg(&msg, true); // "true" ==> allow wakeups
};
#endif // DO_ACPI_S3_SAMPLE

#if DO_S_RESIDENCY_SAMPLE

#ifdef CONFIG_RPMSG_IPC
    #define PW_SCAN_MMAP_DO_IPC(cmd, sub_cmd) rpmsg_send_generic_simple_command(cmd, sub_cmd)
#else
    #define PW_SCAN_MMAP_DO_IPC(cmd, sub_cmd) (-ENODEV)
#endif // CONFIG_RPMSG_IPC

static inline void pw_start_s_residency_counter_i(void)
{
    /*
     * Send START IPC command.
     */
    if (PW_SCAN_MMAP_DO_IPC(INTERNAL_STATE.ipc_start_command, INTERNAL_STATE.ipc_start_sub_command)) {
        printk(KERN_INFO "WARNING: possible error starting S_RES counters!\n");
    }
    pw_pr_debug("GU: SENT START IPC command!\n");
};

static inline void pw_dump_s_residency_counter_i(void)
{
    /*
     * Send DUMP IPC command.
     */
    if (PW_SCAN_MMAP_DO_IPC(INTERNAL_STATE.ipc_dump_command, INTERNAL_STATE.ipc_dump_sub_command)) {
        printk(KERN_INFO "WARNING: possible error dumping S_RES counters!\n");
    }
    pw_pr_debug("GU: SENT DUMP IPC command!\n");
};

static inline void pw_stop_s_residency_counter_i(void)
{
    /*
     * Send STOP IPC command.
     */
    if (PW_SCAN_MMAP_DO_IPC(INTERNAL_STATE.ipc_stop_command, INTERNAL_STATE.ipc_stop_sub_command)) {
        printk(KERN_INFO "WARNING: possible error stopping S_RES counters!\n");
    }
    pw_pr_debug("GU: SENT STOP IPC command!\n");
};

static inline void pw_populate_s_residency_values_i(u64 *values, bool is_begin_boundary)
{
    u16 i=0, j=0;
    u64 value = 0;
    const int counter_size_in_bytes = (int)INTERNAL_STATE.counter_size_in_bytes;
    if (INTERNAL_STATE.collection_type == PW_IO_IPC) {
        pw_dump_s_residency_counter_i(); // TODO: OK to call immediately after 'START'?
    }
#if 1
    for (i=0, j=1; i<INTERNAL_STATE.num_addrs; ++i, ++j) {
        values[j] = 0x0;
        if (j == 4) {
            // pwr library EXPECTS the fifth element to be the ACPI S3 residency value!
            ++j;
        }
        switch (INTERNAL_STATE.collection_type) {
            case PW_IO_IPC:
            case PW_IO_MMIO: // fall-through
                // value = INTERNAL_STATE.platform_remapped_addrs[i];
                // value = *((u64 *)INTERNAL_STATE.platform_remapped_addrs[i]);
                // value = *((u32 *)INTERNAL_STATE.platform_remapped_addrs[i]);
                memcpy(&value, (void *)(unsigned long)INTERNAL_STATE.platform_remapped_addrs[i], counter_size_in_bytes);
                break;
            default:
                printk(KERN_INFO "ERROR: unsupported S0iX collection type: %u!\n", INTERNAL_STATE.collection_type);
                break;
        }
        if (is_begin_boundary) {
            INTERNAL_STATE.init_platform_res_values[i] = value;
            values[j] = 0;
        } else {
            // values[j] = INTERNAL_STATE.init_platform_res_values[i] - value;
            values[j] = value - INTERNAL_STATE.init_platform_res_values[i];
        }
        /*
        if (is_begin_boundary) {
            INTERNAL_STATE.init_platform_res_values[i] = value;
        }
        values[j] = value - INTERNAL_STATE.init_platform_res_values[i];
        */
        pw_pr_debug("\t[%u] ==> %llu (%llu <--> %llu)\n", j, values[j], INTERNAL_STATE.init_platform_res_values[i], value);
    }
#else // if 1
    {
        char __tmp[1024];
        u64 *__p_tmp = (u64 *)&__tmp[0];
        // memcpy((u32 *)&__tmp[0], INTERNAL_STATE.platform_remapped_addrs[0], sizeof(u32) * (INTERNAL_STATE.num_addrs * 2));
        memcpy(__p_tmp, INTERNAL_STATE.platform_remapped_addrs[0], sizeof(u64) * INTERNAL_STATE.num_addrs);
        for (i=0; i<INTERNAL_STATE.num_addrs; ++i) {
            u64 __value1 = 0, __value2 = 0;
            memcpy(&__value1, INTERNAL_STATE.platform_remapped_addrs[i], sizeof(u64));
            __value2 = *((u64 *)INTERNAL_STATE.platform_remapped_addrs[i]);
            printk(KERN_INFO "[%d] ==> %llu, %llu, %llu, %llu\n", i, __p_tmp[i], __value1, __value2, INTERNAL_STATE.platform_remapped_addrs[i]);
        }
    }
#endif // if 0
};

static inline void produce_boundary_s_residency_msg_i(bool is_begin_boundary)
{
    u64 tsc;
    int cpu = raw_smp_processor_id();
    PWCollector_msg_t msg;
    s_res_msg_t *smsg = INTERNAL_STATE.platform_residency_msg;
    u64 *values = smsg->residencies;

    // printk(KERN_INFO "smsg = %p, smsg->residencies = %p\n", smsg, smsg->residencies);

    tscval(&tsc);
    msg.data_type = S_RESIDENCY;
    msg.cpuidx = cpu;
    msg.tsc = tsc;
    // msg.data_len = sizeof(smsg);
    msg.data_len = sizeof(*smsg) + sizeof(u64) * (INTERNAL_STATE.num_addrs + 2); // "+2" for S0i3, S3

    if (startTSC_s_residency == 0) {
        startTSC_s_residency = tsc;
    }
    /*
     * Power library requires S0i0 entry to be delta TSC
     */
    values[0] = tsc - startTSC_s_residency;

    // printk(KERN_INFO "\t[%u] ==> %llu\n", 0, values[0]);

#if 0
    if (INTERNAL_STATE.collection_type == PW_IO_IPC && is_begin_boundary == true) {
        pw_stop_s_residency_counter_i();
        pw_start_s_residency_counter_i();
    }
#endif // if 0

    pw_populate_s_residency_values_i(values, is_begin_boundary);

#if 0
    if (INTERNAL_STATE.collection_type == PW_IO_IPC && is_begin_boundary == false) {
        pw_stop_s_residency_counter_i();
    }
#endif // if 0

    msg.p_data = (u64)((unsigned long)(smsg));

    /*
     * OK, everything computed. Now copy
     * this sample into an output buffer
     */
    pw_produce_generic_msg(&msg, true); // "true" ==> allow wakeups

    /*
     * Check if we need to produce ACPI S3 samples.
     */
    if (pw_is_slm && IS_ACPI_S3_MODE()) {
        if (is_begin_boundary == true) {
            /*
             * Ensure we reset the ACPI S3 'start' TSC counter.
             */
            startTSC_acpi_s3 = 0x0;
        }
        // produce_acpi_s3_sample(0 /* s3 res */);
        produce_acpi_s3_sample(tsc, 0 /* s3 res */);
    }
};

#endif // DO_S_RESIDENCY_SAMPLE


#if DO_WAKELOCK_SAMPLE
/*
 * Insert a Wakelock sample into a (per-cpu) output buffer.
 */
static inline void produce_w_sample(int cpu, u64 tsc, w_sample_type_t type, pid_t tid, pid_t pid, const char *wlname, const char *pname, u64 timeout)
{
    PWCollector_msg_t sample;
    w_wakelock_msg_t w_msg;
    int cp_index = -1;
    size_t len = strlen(wlname);
    size_t msg_len = 0;
    constant_pool_msg_t *cp_msg = NULL;

    pw_mapping_type_t map_type = wlock_insert(len, wlname, &cp_index);

    sample.cpuidx = cpu;

    if (unlikely(map_type == PW_MAPPING_ERROR)) {
        printk(KERN_INFO "ERROR: could NOT insert wlname = %s into constant pool!\n", wlname);
        return;
    }
    /*
     * Preallocate any memory we might need, BEFORE disabling interrupts!
     */
    if (unlikely(map_type == PW_NEW_MAPPING_CREATED)) {
        msg_len = PW_CONSTANT_POOL_MSG_HEADER_SIZE + len + 1;
        cp_msg = pw_kmalloc(msg_len, GFP_ATOMIC);
        if (unlikely(cp_msg == NULL)) {
            /*
             * Hmnnnnn ... we'll need to destroy the newly created node. For now, don't handle this!!!
             * TODO: handle this case!
             */
            printk(KERN_INFO "ERROR: could NOT allocate a new node for a constant-pool mapping: WILL LEAK MEMORY and THIS MAPPING WILL BE MISSING FROM YOUR END RESULTS!");
            return;
        }
    }
    // get_cpu();
    {
        if (unlikely(map_type == PW_NEW_MAPPING_CREATED)) {
            /*
             * We've inserted a new entry into our kernel wakelock constant pool. Tell wuwatch
             * about it.
             */
            cp_msg->entry_type = W_STATE; // This is a KERNEL walock constant pool mapping
            cp_msg->entry_len = len;
            cp_msg->entry_index = cp_index;
            memcpy(cp_msg->entry, wlname, len+1);

            sample.tsc = tsc;
            sample.data_type = CONSTANT_POOL_ENTRY;
            sample.data_len = msg_len;
            sample.p_data = (u64)((unsigned long)cp_msg);

            pw_produce_generic_msg(&sample, false); // "false" ==> do NOT wakeup any sleeping readers
        }
        /*
         * OK, now send the actual wakelock sample.
         */
        w_msg.type = type;
        w_msg.expires = timeout;
        w_msg.tid = tid;
        w_msg.pid = pid;
        w_msg.constant_pool_index = cp_index;
        // memcpy(w_msg.proc_name, pname, PW_MAX_PROC_NAME_SIZE); // process name
        strncpy(w_msg.proc_name, pname, PW_MAX_PROC_NAME_SIZE); // process name

        sample.tsc = tsc;
        sample.data_type = W_STATE;
        sample.data_len = sizeof(w_msg);
        sample.p_data = (u64)(unsigned long)&w_msg;
        /*
         * OK, everything computed. Now copy
         * this sample into an output buffer
         */
        pw_produce_generic_msg(&sample, false); // "false" ==> do NOT wakeup any sleeping readers
    }
    // put_cpu();

    if (unlikely(cp_msg)) {
        //printk(KERN_INFO "OK: sent wakelock mapping: cp_index = %d, name = %s\n", cp_index, wlname);
        pw_kfree(cp_msg);
    }
    //printk(KERN_INFO "OK: sent wakelock msg for wlname = %s\n", wlname);
    return;
};
#endif


/*
 * Insert a P-state transition sample into a (per-cpu) output buffer.
 */
static inline void produce_p_sample(int cpu, unsigned long long tsc, u32 req_freq, u32 perf_status, u8 is_boundary_sample, u64 aperf, u64 mperf)
{
    struct PWCollector_msg sample;
    p_msg_t p_msg;

    sample.cpuidx = cpu;
    sample.tsc = tsc;

    p_msg.unhalted_core_value = aperf;
    p_msg.unhalted_ref_value = mperf;

    p_msg.prev_req_frequency = req_freq;
    p_msg.perf_status_val = (u16)perf_status;
    p_msg.is_boundary_sample = is_boundary_sample;

    sample.data_type = P_STATE;
    sample.data_len = sizeof(p_msg);
    sample.p_data = (u64)((unsigned long)&p_msg);

    pw_pr_debug("DEBUG: TSC = %llu, req_freq = %u, perf-status = %u\n", tsc, req_freq, perf_status);

    /*
     * OK, everything computed. Now copy
     * this sample into an output buffer
     */
    pw_produce_generic_msg(&sample, true); // "true" ==> wakeup sleeping readers, if required
};

/*
 * Insert a K_CALL_STACK sample into a (per-cpu) output buffer.
 */
static inline void produce_k_sample(int cpu, const tnode_t *tentry)
{
    struct PWCollector_msg sample;
    k_sample_t k_sample;

    sample.cpuidx = cpu;
    sample.tsc = tentry->tsc;

    k_sample.tid = tentry->tid;
    k_sample.trace_len = tentry->trace_len;
    /*
     * Generate the "entryTSC" and "exitTSC" values here.
     */
    {
	k_sample.entry_tsc = tentry->tsc - 1;
	k_sample.exit_tsc = tentry->tsc + 1;
    }
    /*
     * Also populate the trace here!
     */
    if(tentry->trace_len){
	int num = tentry->trace_len;
	int i=0;
	u64 *trace = k_sample.trace;
	if(tentry->trace_len >= PW_TRACE_LEN){
	    OUTPUT(0, KERN_ERR "Warning: kernel trace len = %d > TRACE_LEN = %d! Will need CHAINING!\n", num, PW_TRACE_LEN);
	    num = PW_TRACE_LEN;
	}
	/*
	 * Can't 'memcpy()' -- individual entries in
	 * the 'k_sample_t->trace[]' array are ALWAYS
	 * 64 bits wide, REGARDLESS OF THE UNDERLYING
	 * ARCHITECTURE!
	 */
	for(i=0; i<num; ++i){
	    trace[i] = tentry->trace[i];
	}
    }
    OUTPUT(3, KERN_INFO "KERNEL-SPACE mapping!\n");

    sample.data_type = K_CALL_STACK;
    sample.data_len = sizeof(k_sample);
    sample.p_data = (u64)((unsigned long)&k_sample);

    /*
     * OK, everything computed. Now copy
     * this sample into an output buffer
     */
    pw_produce_generic_msg(&sample, true); // "true" ==> wakeup sleeping readers, if required
};

/*
 * Insert an IRQ_MAP sample into a (per-cpu) output buffer.
 */
static inline void produce_i_sample(int cpu, int num, u64 tsc, const char *name)
{
    struct PWCollector_msg sample;
    i_sample_t i_sample;
    /*
    u64 tsc;

    tscval(&tsc);
    */

    sample.cpuidx = cpu;
    sample.tsc = tsc;

    i_sample.irq_num = num;
    memcpy(i_sample.irq_name, name, PW_IRQ_DEV_NAME_LEN); // dst, src

    sample.data_type = IRQ_MAP;
    sample.data_len = sizeof(i_sample);
    sample.p_data = (u64)((unsigned long)&i_sample);

    /*
     * OK, everything computed. Now copy
     * this sample into an output buffer
     */
    pw_produce_generic_msg(&sample, true); // "true" ==> wakeup sleeping readers, if required
};

/*
 * Insert a PROC_MAP sample into a (per-cpu) output buffer.
 */
static inline void produce_r_sample(int cpu, u64 tsc, r_sample_type_t type, pid_t tid, pid_t pid, const char *name)
{
    struct PWCollector_msg sample;
    r_sample_t r_sample;

    sample.cpuidx = cpu;
    sample.tsc = tsc;

    r_sample.type = type;
    r_sample.tid = tid;
    r_sample.pid = pid;
    memcpy(r_sample.proc_name, name, PW_MAX_PROC_NAME_SIZE); // dst, src

    sample.data_type = PROC_MAP;
    sample.data_len = sizeof(r_sample);
    sample.p_data = (u64)((unsigned long)&r_sample);

    /*
     * OK, everything computed. Now copy
     * this sample into an output buffer
     */
    pw_produce_generic_msg(&sample, true); // "true" ==> wakeup sleeping readers, if required
};

/*
 * Insert an M_MAP sample into a (per-cpu) output buffer.
 */
static inline void produce_m_sample(int cpu, const char *name, unsigned long long begin, unsigned long long sz)
{
    struct PWCollector_msg sample;
    m_sample_t m_sample;
    u64 tsc;

    tscval(&tsc);

    sample.cpuidx = cpu;
    sample.tsc = tsc;

    m_sample.start = begin;
    m_sample.end = (begin+sz);
    m_sample.offset = 0;
    memcpy(m_sample.name, name, PW_MODULE_NAME_LEN); // dst, src

    sample.data_type = M_MAP;
    sample.data_len = sizeof(m_sample);
    sample.p_data = (u64)((unsigned long)&m_sample);

    /*
     * OK, everything computed. Now copy
     * this sample into an output buffer
     */
    pw_produce_generic_msg(&sample, true); // "true" ==> wakeup sleeping readers, if required

};


/*
 * Probe functions (and helpers).
 */


/*
 * Generic method to generate a kernel-space call stack.
 * Utilizes the (provided) "save_stack_trace()" function.
 */
int __get_kernel_timerstack(unsigned long buffer[], int len)
{
    struct stack_trace strace;

    strace.max_entries = len; // MAX_BACKTRACE_LENGTH;
    strace.nr_entries = 0;
    strace.entries = buffer;
    strace.skip = 3;

    save_stack_trace(&strace);

    OUTPUT(0, KERN_INFO "[%d]: KERNEL TRACE: nr_entries = %d\n", TID(), strace.nr_entries);

    return strace.nr_entries;
};

/*
 * Generate a kernel-space call stack.
 * Requires the kernel be compiler with frame pointers ON.
 *
 * Returns number of return addresses in the call stack
 * or ZERO, to indicate no stack.
 */
int get_kernel_timerstack(unsigned long buffer[], int len)
{
    return __get_kernel_timerstack(buffer, len);
};


static void timer_init(void *timer_addr)
{
    pid_t tid = TID();
    pid_t pid = PID();
    u64 tsc = 0;
    int trace_len = 0;
    unsigned long trace[MAX_BACKTRACE_LENGTH];
    bool is_root_timer = false;
    s32 init_cpu = RAW_CPU();

    tscval(&tsc);

    /*
     * For accuracy, we ALWAYS collect
     * kernel call stacks.
     */
    if ( (is_root_timer = IS_ROOT_TIMER(tid)) ) {
	/*
	 * get kernel timerstack here.
	 * Requires the kernel be compiled with
	 * frame_pointers on.
	 */
	if (INTERNAL_STATE.have_kernel_frame_pointers) {
	    trace_len = get_kernel_timerstack(trace, MAX_BACKTRACE_LENGTH);
	}
	else{
	    trace_len = 0;
	}
	OUTPUT(0, KERN_INFO "KERNEL-SPACE timer init! Timer_addr = %p, tid = %d, pid = %d\n", timer_addr, tid, pid);
    } else {
        trace_len = 0;
    }
    /*
     * Store the timer if:
     * (a) called for a ROOT process (tid == 0) OR
     * (b) we're actively COLLECTING.
     */
    if (is_root_timer || IS_COLLECTING()) {
	DO_PER_CPU_OVERHEAD_FUNC(timer_insert, (unsigned long)timer_addr, tid, pid, tsc, init_cpu, trace_len, trace);
    }
};

// #if (KERNEL_VER < 35)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_hrtimer_init(struct hrtimer *timer, clockid_t clockid, enum hrtimer_mode mode)
#else
    static void probe_hrtimer_init(void *ignore, struct hrtimer *timer, clockid_t clockid, enum hrtimer_mode mode)
#endif
{
    DO_PER_CPU_OVERHEAD_FUNC(timer_init, timer);
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_timer_init(struct timer_list *timer)
#else
    static void probe_timer_init(void *ignore, struct timer_list *timer)
#endif
{
    /*
     * Debugging ONLY!
     */
    DO_PER_CPU_OVERHEAD_FUNC(timer_init, timer);
};

/*
 * Interval timer state probe.
 * Fired on interval timer initializations
 * (from "setitimer(...)")
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_itimer_state(int which, const struct itimerval *const value, cputime_t expires)
#else
    static void probe_itimer_state(void *ignore, int which, const struct itimerval *const value, cputime_t expires)
#endif
{
    struct hrtimer *timer = &current->signal->real_timer;

    OUTPUT(3, KERN_INFO "[%d]: ITIMER STATE: timer = %p\n", TID(), timer);
    DO_PER_CPU_OVERHEAD_FUNC(timer_init, timer);
};


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_hrtimer_start(struct hrtimer *hrt)
#else
    static void probe_hrtimer_start(void *ignore, struct hrtimer *hrt)
#endif
{
    int cpu = CPU();
    pid_t tid = TID();
    pid_t pid = PID();
    u64 tsc = 0;
    /* const char *name = TIMER_START_COMM(hrt); */
    int i, trace_len;
    char symname[KSYM_NAME_LEN];
    unsigned long trace[MAX_BACKTRACE_LENGTH];
    void *sched_timer_addr = NULL;
    per_cpu_t *pcpu = NULL;
    bool should_unregister = false;

    BUG_ON(!hrt);

    if(!should_probe_on_hrtimer_start){
	OUTPUT(3, KERN_INFO "HRTIMER_START: timer = %p\n", hrt);
	return;
    }

    /*
     * Not sure if "save_stack_trace" or "sprint_symbol" can
     * sleep. To be safe, use the "__get_cpu_var" variants
     * here. Note that it's OK if they give us stale values -- we're
     * not looking for an exact match.
     */
    if(tid || local_read(&__get_cpu_var(sched_timer_found)))
	return;

    /*
     * Basic algo: generate a backtrace for this hrtimer_start
     * tracepoint. Then generate symbolic information for each
     * entry in the backtrace array. Check these symbols.
     * If any one of these symbols is equal to "cpu_idle" then
     * we know that this timer is the "tick" timer for this
     * CPU -- store the address (and the backtrace) in
     * the trace map (and also note that we have, in fact, found
     * the tick timer so that we don't repeat this process again).
     */

    if(INTERNAL_STATE.have_kernel_frame_pointers){
	trace_len = get_kernel_timerstack(trace, MAX_BACKTRACE_LENGTH);
	OUTPUT(0, KERN_INFO "[%d]: %.20s TIMER_START for timer = %p. trace_len = %d\n", tid, TIMER_START_COMM(hrt), hrt, trace_len);
	for(i=0; i<trace_len; ++i){
	    sprint_symbol(symname, trace[i]);
	    OUTPUT(3, KERN_INFO "SYM MAPPING: 0x%lx --> %s\n", trace[i], symname);
	    if(strstr(symname, "cpu_idle")){
		OUTPUT(0, KERN_INFO "FOUND CPU IDLE for cpu = %d . TICK SCHED TIMER = %p\n", cpu, hrt);
		local_inc(&__get_cpu_var(sched_timer_found));
		// *timer_found = true;
		sched_timer_addr = hrt;
	    }
	}
    }else{
	OUTPUT(0, KERN_INFO "NO TIMER STACKS!\n");
    }

    if(sched_timer_addr){
	/*
	 * OK, use the safer "get_cpu_var(...)" variants
	 * here. These disable interrupts.
	 */
	pcpu = &get_cpu_var(per_cpu_counts);
	{
	    cpu = CPU();
	    /*
	     * Races should *NOT* happen. Still, check
	     * to make sure.
	     */
	    if(!pcpu->sched_timer_addr){
		pcpu->sched_timer_addr = sched_timer_addr;

		tsc = 0x1 + cpu;

		timer_insert((unsigned long)sched_timer_addr, tid, pid, tsc, cpu, trace_len, trace);
		/*
		 * Debugging
		 */
		if(!timer_find((unsigned long)sched_timer_addr, tid)){
		    pw_pr_error("ERROR: could NOT find timer %p in hrtimer_start!\n", sched_timer_addr);
		}
	    }
	}
	put_cpu_var(pcpu);

	LOCK(tick_count_lock);
	{
	    if( (should_unregister = (++tick_count == num_online_cpus()))){
		OUTPUT(0, KERN_INFO "[%d]: ALL TICK TIMERS accounted for -- removing hrtimer start probe!\n", cpu);
		should_probe_on_hrtimer_start = false;
	    }
	}
	UNLOCK(tick_count_lock);
    }
};

/*
 * Common function to perform some bookkeeping on
 * IRQ-related wakeups (including (HR)TIMER_SOFTIRQs).
 * Records hits and (if necessary) sends i-sample
 * messages to Ring 3.
 */
static void handle_irq_wakeup_i(int cpu, int irq_num, const char *irq_name, bool was_hit)
{
    /*
     * Send a sample to Ring-3
     * (but only if collecting).
     */
    u64 sample_tsc = 0;
    if (IS_COLLECTING()) {
        tscval(&sample_tsc);
        record_wakeup_cause(sample_tsc, PW_BREAK_TYPE_I, irq_num, -1, -1, -1);
    }
    /*
     * Then send an i-sample instance
     * to Ring 3 (but only if so configured
     * and if this is first time this
     * particular IRQ was seen on the
     * current CPU).
     */
#if DO_CACHE_IRQ_DEV_NAME_MAPPINGS
    {
        int __ret = -1;
        /*
         * We only cache device names if they
         * actually caused a C-state
         * wakeup.
         */
        if(was_hit){
            DO_PER_CPU_OVERHEAD_FUNC_RET(__ret, irq_insert, cpu, irq_num, irq_name);
            /*
             * Protocol:
             * (a) if mapping FOUND (and already SENT for THIS CPU): irq_insert returns "OK_IRQ_MAPPING_EXISTS"
             * (b) if new mapping CREATED (or mapping exists, but NOT SENT for THIS CPU): irq_insert returns "OK_NEW_IRQ_MAPPING_CREATED"
             * (c) if ERROR: irq_insert returns "ERROR_IRQ_MAPPING"
             */
            if(__ret == OK_NEW_IRQ_MAPPING_CREATED && IS_COLLECTING()) {
                /*
                 * Send mapping info to Ring-3.
                 */
                // produce_i_sample(cpu, irq_num, irq_name);
                produce_i_sample(cpu, irq_num, sample_tsc, irq_name);
            }else if(__ret == ERROR_IRQ_MAPPING){
                pw_pr_error("ERROR: could NOT insert [%d,%s] into irq list!\n", irq_num, irq_name);
            }
        }
    }
#endif // DO_CACHE_IRQ_DEV_NAME_MAPPINGS
};

#define TRACK_TIMER_EXPIRES 1

static void timer_expire(void *timer_addr, pid_t tid)
{
    int cpu = -1;
    pid_t pid = -1;
    tnode_t *entry = NULL;
    u64 tsc = 0;
    bool found = false;
    bool was_hit = false;
    bool is_root = false;
    int irq_num = -1;
    s32 init_cpu = -1;

    /*
     * Reduce overhead -- do NOT run
     * if user specifies NO C-STATES.
     */
    if (unlikely(!IS_C_STATE_MODE())) {
	return;
    }

#if !TRACK_TIMER_EXPIRES
    {
        if (IS_COLLECTING()) {
            u64 sample_tsc;
            tscval(&sample_tsc);
            record_wakeup_cause(sample_tsc, PW_BREAK_TYPE_T, 0, -1, PID(), TID());
        }

        return;
    }
#endif

#if DO_IOCTL_STATS
    stats_t *pstats = NULL;
#endif
    /*
     * Atomic context => use __get_cpu_var(...) instead of get_cpu_var(...)
     */
    irq_num = (&__get_cpu_var(per_cpu_counts))->was_timer_hrtimer_softirq;

    // was_hit = local_read(&__get_cpu_var(is_first_event)) == 1;
    was_hit = __get_cpu_var(wakeup_event_counter).event_tsc == 0;

#if DO_IOCTL_STATS
    pstats = &__get_cpu_var(per_cpu_stats);
#endif // DO_IOCTL_STATS

    cpu = CPU();

    if ( (entry = (tnode_t *)timer_find((unsigned long)timer_addr, tid))) {
	pid = entry->pid;
	tsc = entry->tsc;
        init_cpu = entry->init_cpu;
	found = true;
        is_root = entry->is_root_timer;
    } else {
	/*
	 * Couldn't find timer entry -- PID defaults to TID.
	 */
	pid = tid;
	tsc = 0x1;
	OUTPUT(3, KERN_INFO "Warning: [%d]: timer %p NOT found in list!\n", pid, timer_addr);
        is_root = pid == 0;
    }

    if (!found) {
	// tsc = pw_max_num_cpus + 1;
        tsc = 0x0;
	if (tid < 0) {
	    /*
	     * Yes, this is possible, especially if
	     * the timer was fired because of a TIMER_SOFTIRQ.
	     * Special case that here.
	     */
            if (irq_num > 0) {
		/*
		 * Basically, fall back on the SOFTIRQ
		 * option because we couldn't quite figure
		 * out the process that is causing this
		 * wakeup. This is a duplicate of the
		 * equivalent code in "inter_common(...)".
		 */
		const char *irq_name = pw_softirq_to_name[irq_num];
		OUTPUT(3, KERN_INFO "WARNING: could NOT find TID in timer_expire for Timer = %p: FALLING BACK TO TIMER_SOFTIRQ OPTION! was_hit = %s\n", timer_addr, GET_BOOL_STRING(was_hit));
		handle_irq_wakeup_i(cpu, irq_num, irq_name, was_hit);
		/*
		 * No further action is required.
		 */
		return;
	    }
	    else {
		/*
		 * tid < 0 but this was NOT caused
		 * by a TIMER_SOFTIRQ.
		 * UPDATE: this is also possible if
		 * the kernel wasn't compiled with the
		 * 'CONFIG_TIMER_STATS' option set.
		 */
		OUTPUT(0, KERN_INFO "WARNING: NEGATIVE tid in timer_expire!\n");
	    }
	}
    } else {
	/*
	 * OK, found the entry. But timers fired
	 * because of 'TIMER_SOFTIRQ' will have
	 * tid == -1. Guard against that
	 * by checking the 'tid' value. If < 0
	 * then replace with entry->tid
	 */
	if(tid < 0){
	    tid = entry->tid;
	}
    }
    /*
     * Now send a sample to Ring-3.
     * (But only if collecting).
     */
    if (IS_COLLECTING()) {
        u64 sample_tsc;

        tscval(&sample_tsc);
        record_wakeup_cause(sample_tsc, PW_BREAK_TYPE_T, tsc, init_cpu, pid, tid);
    }

    /*
     * OK, send the TIMER::TSC mapping & call stack to the user
     * (but only if this is for a kernel-space call stack AND the
     * user wants kernel call stack info).
     */
    if (is_root && (IS_COLLECTING() || IS_SLEEPING()) && IS_KTIMER_MODE() && found && entry && !entry->trace_sent) {
	produce_k_sample(cpu, entry);
	entry->trace_sent = 1;
    }
};

/*
 * High resolution timer (hrtimer) expire entry probe.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_hrtimer_expire_entry(struct hrtimer *hrt, ktime_t *now)
#else
    static void probe_hrtimer_expire_entry(void *ignore, struct hrtimer *hrt, ktime_t *now)
#endif
{
    DO_PER_CPU_OVERHEAD_FUNC(timer_expire, hrt, TIMER_START_PID(hrt));
};

/*
 * Macro to determine if the given
 * high resolution timer is periodic.
 */
#define IS_INTERVAL_TIMER(hrt) ({					\
	    bool __tmp = false;						\
	    pid_t pid = TIMER_START_PID(hrt);				\
	    ktime_t rem_k = hrtimer_expires_remaining(hrt);		\
	    s64 remaining = rem_k.tv64;					\
	    /* We first account for timers that */			\
	    /* are explicitly re-enqueued. For these */			\
	    /* we check the amount of time 'remaining' */		\
	    /* for the timer i.e.  how much time until */		\
	    /* the timer expires. If this is POSITIVE ==> */		\
	    /* the timer will be re-enqueued onto the */		\
	    /* timer list and is therefore PERIODIC */			\
	    if(remaining > 0){						\
		__tmp = true;						\
	    }else{							\
		/* Next, check for 'itimers' -- these INTERVAL TIMERS are */ \
		/* different in that they're only re-enqueued when their */ \
		/* signal (i.e. SIGALRM) is DELIVERED. Accordingly, we */ \
		/* CANNOT check the 'remaining' time for these timers. Instead, */ \
		/* we compare them to an individual task's 'REAL_TIMER' address.*/ \
		/* N.B.: Call to 'pid_task(...)' influenced by SEP driver code */ \
		struct task_struct *tsk = pid_task(find_pid_ns(pid, &init_pid_ns), PIDTYPE_PID); \
		__tmp = (tsk && ( (hrt) == &tsk->signal->real_timer));	\
	    }								\
	    __tmp; })


/*
 * High resolution timer (hrtimer) expire exit probe.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_hrtimer_expire_exit(struct hrtimer *hrt)
#else
    static void probe_hrtimer_expire_exit(void *ignore, struct hrtimer *hrt)
#endif
{
    if(!IS_INTERVAL_TIMER(hrt)){
        /*
         * timers are run from hardirq context -- no need
         * for expensive 'get_cpu_var(...)' variants.
         */
        per_cpu_t *pcpu = &__get_cpu_var(per_cpu_counts);
        /*
         * REMOVE the timer from
         * our timer map here (but
         * only if this isn't a 'sched_tick'
         * timer!)
         */
        if((void *)hrt != pcpu->sched_timer_addr){
            int ret = -1;
            DO_PER_CPU_OVERHEAD_FUNC_RET(ret, timer_delete, (unsigned long)hrt, TIMER_START_PID(hrt));
            if(ret){
                OUTPUT(0, KERN_INFO "WARNING: could NOT delete timer mapping for HRT = %p, TID = %d, NAME = %.20s\n", hrt, TIMER_START_PID(hrt), TIMER_START_COMM(hrt));
            }else{
                OUTPUT(3, KERN_INFO "OK: DELETED timer mapping for HRT = %p, TID = %d, NAME = %.20s\n", hrt, TIMER_START_PID(hrt), TIMER_START_COMM(hrt));
                // debugging ONLY!
                if(timer_find((unsigned long)hrt, TIMER_START_PID(hrt))){
                    OUTPUT(0, KERN_INFO "WARNING: TIMER_FIND reports TIMER %p STILL IN MAP!\n", hrt);
                }
            }
        }
    }
};

#define DEFERRABLE_FLAG (0x1)
#define IS_TIMER_DEFERRABLE(t) ( (unsigned long)( (t)->base) & DEFERRABLE_FLAG )


/*
 * Timer expire entry probe.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_timer_expire_entry(struct timer_list *t)
#else
    static void probe_timer_expire_entry(void *ignore, struct timer_list *t)
#endif
{
    DO_PER_CPU_OVERHEAD_FUNC(timer_expire, t, TIMER_START_PID(t));
};


/*
 * Function common to all interrupt tracepoints.
 */
static void inter_common(int irq_num, const char *irq_name)
{
    per_cpu_t *pcpu = NULL;

    bool was_hit = false;

#if DO_IOCTL_STATS
    stats_t *pstats = NULL;
#endif

    /*
     * Reduce overhead -- do NOT run
     * if user specifies NO C-STATES.
     */
    if(unlikely(!IS_C_STATE_MODE())){
        return;
    }

    /*
     * Debugging: make sure we're in
     * interrupt context!
     */
    if(!in_interrupt()){
        printk(KERN_ERR "BUG: inter_common() called from a NON-INTERRUPT context! Got irq: %lu and soft: %lu\n", in_irq(), in_softirq());
        return;
    }

    /*
     * Interrupt context: no need for expensive "get_cpu_var(...)" version.
     */
    pcpu = &__get_cpu_var(per_cpu_counts);

    /*
     * If this is a TIMER or an HRTIMER SOFTIRQ then
     * DO NOTHING (let the 'timer_expire(...)'
     * function handle this for greater accuracy).
     */
    if(false && (irq_num == TIMER_SOFTIRQ || irq_num == HRTIMER_SOFTIRQ)){
	pcpu->was_timer_hrtimer_softirq = irq_num;
#if DO_IOCTL_STATS
	/*
	 * Increment counter for timer interrupts as well.
	 */
	local_inc(&pstats->num_timers);
#endif // DO_IOCTL_STATS
	OUTPUT(3, KERN_INFO "(HR)TIMER_SOFTIRQ: # = %d\n", irq_num);
	return;
    }

#if DO_IOCTL_STATS
    pstats = &__get_cpu_var(per_cpu_stats);
    local_inc(&pstats->num_inters);

    /*
     * Increment counter for timer interrupts as well.
     */
    if(in_softirq() && (irq_num == TIMER_SOFTIRQ || irq_num == HRTIMER_SOFTIRQ))
        local_inc(&pstats->num_timers);
#endif

    /*
     * Check if this interrupt caused a C-state
     * wakeup (we'll use that info to decide
     * whether to cache this IRQ # <-> DEV name
     * mapping).
     */
    // was_hit = local_read(&__get_cpu_var(is_first_event)) == 1;
    was_hit = __get_cpu_var(wakeup_event_counter).event_tsc == 0;

    /*
     * OK, record a 'hit' (if applicable) and
     * send an i-sample message to Ring 3.
     */
    handle_irq_wakeup_i(CPU(), irq_num, irq_name, was_hit);
};

/*
 * IRQ tracepoint.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_irq_handler_entry(int irq, struct irqaction *action)
#else
static void probe_irq_handler_entry(void *ignore, int irq, struct irqaction *action)
#endif
{
    const char *name = action->name;
    if (!name) {
        return;
    }
    OUTPUT(3, KERN_INFO "NUM: %d\n", irq);
    // inter_common(irq);
    DO_PER_CPU_OVERHEAD_FUNC(inter_common, irq, name);
};

/*
 * soft IRQ tracepoint.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_softirq_entry(struct softirq_action *h, struct softirq_action *vec)
#else
    static void probe_softirq_entry(void *ignore, struct softirq_action *h, struct softirq_action *vec)
#endif
{
    int irq = -1;
    const char *name = NULL;
    irq = (int)(h-vec);
    name = pw_softirq_to_name[irq];

    if (!name) {
        return;
    }

    OUTPUT(3, KERN_INFO "NUM: %d\n", irq);

    DO_PER_CPU_OVERHEAD_FUNC(inter_common, irq, name);
};
#else // >= 2.6.38
static void probe_softirq_entry(void *ignore, unsigned int vec_nr)
{
	int irq = (int)vec_nr;
	const char *name = pw_softirq_to_name[irq];

	DO_PER_CPU_OVERHEAD_FUNC(inter_common, irq, name);
};
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_workqueue_execution(struct task_struct *wq_thread, struct work_struct *work)
#else
static void probe_workqueue_execution(void * ignore, struct task_struct *wq_thread, struct work_struct *work)
#endif // < 2.6.35
{
    if (IS_COLLECTING()) {
        u64 tsc;
        tscval(&tsc);

        record_wakeup_cause(tsc, PW_BREAK_TYPE_W, 0, -1, -1, -1);
    }
};
#else // >= 2.6.36
static void probe_workqueue_execute_start(void *ignore, struct work_struct *work)
{
    if (IS_COLLECTING()) {
        u64 tsc;
        tscval(&tsc);

        record_wakeup_cause(tsc, PW_BREAK_TYPE_W, 0, -1, -1, -1);
    }
};
#endif // < 2.6.36

/*
 * Basically the same as arch/x86/kernel/irq.c --> "arch_irq_stat_cpu(cpu)"
 */

static u64 my_local_arch_irq_stats_cpu(void)
{
    u64 sum = 0;
    irq_cpustat_t *stats;
#ifdef __arm__
    int i=0;
#endif
    BEGIN_LOCAL_IRQ_STATS_READ(stats);
    {
#ifndef __arm__
// #ifdef CONFIG_X86_LOCAL_APIC
        sum += stats->apic_timer_irqs;
// #endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34)
        sum += stats->x86_platform_ipis;
#endif // 2,6,34
        sum += stats->apic_perf_irqs;
#ifdef CONFIG_SMP
        sum += stats->irq_call_count;
        sum += stats->irq_resched_count;
        sum += stats->irq_tlb_count;
#endif
#ifdef CONFIG_X86_THERMAL_VECTOR
        sum += stats->irq_thermal_count;
#endif
        sum += stats->irq_spurious_count; // should NEVER be non-zero!!!
#else
        sum += stats->__softirq_pending;
#ifdef CONFIG_SMP
        for (i=0; i<NR_IPI; ++i) {
            sum += stats->ipi_irqs[i];
        }
#endif
#endif
    }
    END_LOCAL_IRQ_STATS_READ(stats);
    return sum;
};

static DEFINE_PER_CPU(u64, prev_c6_val) = 0;

/*
 * TPS epoch manipulation functions.
 */
#if DO_TPS_EPOCH_COUNTER

int inc_tps_epoch_i(void)
{
    int retVal = -1;
    /*
     * From "Documentation/memory-barriers.txt": "atomic_inc_return()"
     * has IMPLICIT BARRIERS -- no need to add explicit barriers
     * here!
     */
    retVal = atomic_inc_return(&tps_epoch);
    return retVal;
};

int read_tps_epoch_i(void)
{
    /*
     * Make sure TPS updates have propagated
     */
    smp_mb();
    return atomic_read(&tps_epoch);
};
#endif // DO_TPS_EPOCH_COUNTER

static int pw_read_msr_info_set_i(struct pw_msr_info_set *info_set)
{
    int num_res = 0;
#ifndef __arm__
    int i=0, curr_index = 0;
    u64 val = 0;
    s32 msr_addr = -1;
    // bool init_msr_set_sent = info_set->init_msr_set_sent == 1;
    int num_msrs = INTERNAL_STATE.num_msrs;
    struct pw_msr_addr *msr_addrs = INTERNAL_STATE.msr_addrs;

    /*
     * Read values for EVERY C-state MSR (Thread/Core/Mod/Pkg)
     * TODO:
     * We will need to move away from a linked list to an array.
     */
    for (i=0; i<num_msrs; ++i) {
        msr_addr = msr_addrs[i].addr;
        if (unlikely(msr_addr <= 0)) {
            continue;
        }
        WUWATCH_RDMSRL(msr_addr, val);
        if (unlikely(info_set->prev_msr_vals[i].val == 0x0)) {
            if (msr_addrs[i].id.depth == MPERF) {
                info_set->curr_msr_count[curr_index].id = info_set->prev_msr_vals[i].id;
                info_set->curr_msr_count[curr_index++].val = val;
            }
        } else { // val != 0x0
            if (info_set->prev_msr_vals[i].val != val) {
                if (msr_addrs[i].id.depth > MPERF) {
                    ++num_res;
                }
                info_set->curr_msr_count[curr_index].id = info_set->prev_msr_vals[i].id;
                info_set->curr_msr_count[curr_index++].val = val;
            }
        }
        info_set->prev_msr_vals[i].val = val;
    }
#else
    // probe_power_end fills in these statistics when it is called
    // so we just grab what is set here.  On x86 we grab and set them above
    *which_cx = msr_set->prev_req_cstate;
    *cx_val = msr_set->curr_msr_count[msr_set->prev_req_cstate];
    num_res = 1;
#endif // ifndef __arm__
    return num_res;
};

static void tps_lite(bool is_boundary_sample)
{
    /*
     * (1) Read APERF, MPERF.
     * (2) Produce a "C_LITE_MSG" instance.
     */
    u64 tsc = 0x0;
    u64 mperf = 0x0;
    int cpu = get_cpu();
    {

        tscval(&tsc);

        WUWATCH_RDMSRL(REF_CYCLES_MSR_ADDR, mperf);
    }
    put_cpu();
    /*
     * Data collected. Now enqueue it.
     */
    {
        c_multi_msg_t cm;
        struct PWCollector_msg sample;

        sample.cpuidx = cpu;
        sample.tsc = tsc;

#ifndef __arm__
        cm.mperf = mperf;
#else
        // TODO
        cm.mperf = c0_time;
#endif

        cm.req_state = (u8)APERF;


        cm.wakeup_tsc = 0x0; // don't care
        cm.wakeup_data = 0x0; // don't care
        cm.timer_init_cpu = 0x0; // don't care
        cm.wakeup_pid = -1; // don't care
        cm.wakeup_tid = -1; // don't care
        cm.wakeup_type = 0x0; // don't care
        /*
         * The only field of interest is the 'num_msrs' value.
         */
        cm.num_msrs = 0x0;


#if DO_TPS_EPOCH_COUNTER
        /*
         * We're entering a new TPS "epoch".
         * Increment our counter.
         */
        cm.tps_epoch = inc_tps_epoch_i();
#endif // DO_TPS_EPOCH_COUNTER

        sample.data_type = C_STATE;
        sample.data_len = C_MULTI_MSG_HEADER_SIZE();
        sample.p_data = (u64)((unsigned long)&cm);

        if (IS_COLLECTING() || is_boundary_sample) {
            pw_produce_generic_msg(&sample, true);
        }
    }
};

static void bdry_tps(void)
{
    u64 tsc = 0;
    PWCollector_msg_t sample;
    pw_msr_info_set_t *info_set = NULL;
    u32 prev_req_cstate = 0;
    u8 init_msr_set_sent = 1;
    int num_cx = -1;
    int num_msrs = 0;
    pw_msr_val_t *msr_vals = NULL;
    int cpu;

#ifdef __arm__
    u64 *prev_tsc = NULL;
    u64 c0_time = 0;
#endif

    tscval(&tsc);
    cpu = RAW_CPU();
    /*
     * Read all C-state MSRs.
     */
    {
        info_set = pw_pcpu_msr_info_sets + cpu;
#ifdef __arm__
        ++state;  // on ARM (Nexus 7 at least) states start with LP3 and no C0
        prev_tsc = &__get_cpu_var(trace_power_prev_time);
        c0_time = tsc - *prev_tsc;
        *prev_tsc = tsc;
        set->prev_msr_vals[MPERF] += c0_time;
        c0_time = set->prev_msr_vals[MPERF];
#endif // __arm__
        {
            num_msrs = info_set->num_msrs;
            init_msr_set_sent = info_set->init_msr_set_sent;
            prev_req_cstate = info_set->prev_req_cstate;
            num_cx = pw_read_msr_info_set_i(info_set);
            info_set->prev_req_cstate = (u32)MPERF; // must be after pw_read_msr_set_i for ARM changes
            if (unlikely(init_msr_set_sent == 0)) {
                // memcpy(msr_set, info_set->prev_msr_vals, sizeof(u64) * info_set->num_msrs);
                info_set->init_msr_set_sent = 1;
            }
            if (unlikely(num_cx > 1)) {
                // memcpy(msr_vals, info_set->curr_msr_count, sizeof(u64) * info_set->num_msrs);
            }
        }
    }

    if (num_cx > 1) {
        OUTPUT(0, KERN_INFO "WARNING: [%d]: # cx = %d\n", cpu, num_cx);
    }

    /*
     * Get wakeup cause(s).
     * Only required if we're capturing C-state samples.
     */
    if (IS_C_STATE_MODE()) {
        if (unlikely(init_msr_set_sent == 0)) {
            /*
             * OK, this is the first TPS for this thread during the current collection.
             * Send a "POSIX_TIME_SYNC" message to allow Ring-3 to correlate the TSC used by wuwatch with
             * the "clock_gettime()" used by TPSS.
             */
            {
                struct timespec ts;
                tsc_posix_sync_msg_t tsc_msg;
                u64 tmp_tsc = 0, tmp_nsecs = 0;
                ktime_get_ts(&ts);
                tscval(&tmp_tsc);
                tmp_nsecs = (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
                tsc_msg.tsc_val = tmp_tsc; tsc_msg.posix_mono_val = tmp_nsecs;

                sample.tsc = tsc;
                sample.cpuidx = cpu;
                sample.data_type = TSC_POSIX_MONO_SYNC;
                sample.data_len = sizeof(tsc_msg);
                sample.p_data = (u64)((unsigned long)&tsc_msg);

                pw_produce_generic_msg(&sample, true);
                pw_pr_debug(KERN_INFO "[%d]: SENT POSIX_TIME_SYNC\n", cpu);
                pw_pr_debug(KERN_INFO "[%d]: tsc = %llu posix mono = %llu\n", cpu, tmp_tsc, tmp_nsecs);
            }
            /*
             * 2. Second, the initial MSR 'set' for this (logical) CPU.
             */
            {
                sample.tsc = tsc;
                sample.cpuidx = cpu;
                sample.data_type = C_STATE_MSR_SET;
                // sample.data_len = sizeof(msr_set);
                sample.data_len = num_msrs * sizeof(pw_msr_val_t);
                // sample.p_data = (u64)((unsigned long)msr_set);
                sample.p_data = (u64)((unsigned long)info_set->prev_msr_vals);

                // Why "true"? Document!
                pw_produce_generic_msg(&sample, true);
                pw_pr_debug(KERN_INFO "[%d]: SENT init msr set at TSC = %llu\n", cpu, tsc);
            }
        }

        /*
         * Send the actual TPS message here.
         */
        {
            c_multi_msg_t *cm = (c_multi_msg_t *)info_set->c_multi_msg_mem;
            BUG_ON(!cm);

            sample.cpuidx = cpu;
            sample.tsc = tsc;

            msr_vals = (pw_msr_val_t *)cm->data;

#ifndef __arm__
            cm->mperf = info_set->curr_msr_count[0].val;
#else
            // TODO
            cm->mperf = c0_time;
#endif

            cm->req_state = (u8)MPERF;


            cm->req_state = (u8)APERF;


            cm->wakeup_tsc = 0x0; // don't care
            cm->wakeup_data = 0x0; // don't care
            cm->timer_init_cpu = 0x0; // don't care
            cm->wakeup_pid = -1; // don't care
            cm->wakeup_tid = -1; // don't care
            cm->wakeup_type = 0x0; // don't care
            cm->num_msrs = num_cx;

            /*
             * 'curr_msr_count[0]' contains the MPERF value, which is encoded separately.
             * We therefore read from 'curr_msr_count[1]'
             */
            memcpy(msr_vals, &info_set->curr_msr_count[1], sizeof(pw_msr_val_t) * num_cx);

            sample.data_type = C_STATE;
            sample.data_len = sizeof(pw_msr_val_t) * num_cx + C_MULTI_MSG_HEADER_SIZE();
            sample.p_data = (u64)((unsigned long)cm);

            pw_produce_generic_msg(&sample, true);
        }
    }
};

static void tps(unsigned int type, unsigned int state)
{
    int cpu = CPU(), epoch = 0;
    u64 tsc = 0;
    PWCollector_msg_t sample;
    pw_msr_info_set_t *info_set = NULL;
    bool local_apic_fired = false;
    u32 prev_req_cstate = 0;
    u8 init_msr_set_sent = 1;
    int num_cx = -1;
    char *__buffer = NULL;
    bool did_alloc = false;
    int num_msrs = 0;
    pw_msr_val_t *msr_vals = NULL;

#ifdef __arm__
    u64 *prev_tsc = NULL;
    u64 c0_time = 0;
#endif

    tscval(&tsc);

    /*
     * Read all C-state MSRs.
     */
    get_cpu();
    {
        info_set = pw_pcpu_msr_info_sets + cpu;
#ifdef __arm__
        ++state;  // on ARM (Nexus 7 at least) states start with LP3 and no C0
        prev_tsc = &__get_cpu_var(trace_power_prev_time);
        c0_time = tsc - *prev_tsc;
        *prev_tsc = tsc;
        set->prev_msr_vals[MPERF] += c0_time;
        c0_time = set->prev_msr_vals[MPERF];
#endif // __arm__
        {
            num_msrs = info_set->num_msrs;
            init_msr_set_sent = info_set->init_msr_set_sent;
            prev_req_cstate = info_set->prev_req_cstate;
            num_cx = pw_read_msr_info_set_i(info_set);
            info_set->prev_req_cstate = (u32)state; // must be after pw_read_msr_set_i for ARM changes
            if (unlikely(init_msr_set_sent == 0)) {
                // memcpy(msr_set, info_set->prev_msr_vals, sizeof(u64) * info_set->num_msrs);
                info_set->init_msr_set_sent = 1;
            }
            if (unlikely(num_cx > 1)) {
                // memcpy(msr_vals, info_set->curr_msr_count, sizeof(u64) * info_set->num_msrs);
            }
        }
    }
    put_cpu();

    BUG_ON(init_msr_set_sent == 0);

    if (num_cx > 1) {
        OUTPUT(0, KERN_INFO "WARNING: [%d]: # cx = %d\n", cpu, num_cx);
    }

    /*
     * Get wakeup cause(s).
     * Only required if we're capturing C-state samples.
     */
    if (IS_C_STATE_MODE()) {
        u64 event_tsc = 0, event_val = 0;
        pid_t event_tid = -1, event_pid = -1;
        c_break_type_t event_type = PW_BREAK_TYPE_U;
        s32 event_init_cpu = -1;
        /*
         * See if we can get a wakeup cause, along
         * with associated data.
         * We use "__get_cpu_var()" instead of "get_cpu_var()" because
         * it is OK for us to be preempted out at any time. Also, not
         * disabling preemption saves us about 500 cycles per TPS.
         */
        {
            struct wakeup_event *wu_event = &get_cpu_var(wakeup_event_counter);
            if (wu_event->event_tsc > 0) {
                event_type = wu_event->event_type;
                event_val = wu_event->event_val;
                event_tsc = wu_event->event_tsc;
                event_pid = wu_event->event_pid;
                event_tid = wu_event->event_tid;
                event_init_cpu = wu_event->init_cpu;
                wu_event->event_tsc = 0; // reset for the next wakeup event.
            }
            put_cpu_var(wakeup_event_counter);
        }
        /*
         * Check if the local APIC timer raised interrupts.
         */
        {
            u64 curr_num_local_apic = my_local_arch_irq_stats_cpu();
            u64 *old_num_local_apic = &__get_cpu_var(num_local_apic_timer_inters);
            if (*old_num_local_apic && (*old_num_local_apic != curr_num_local_apic)) {
                local_apic_fired = true;
            }
            *old_num_local_apic = curr_num_local_apic;
        }

        if (event_type == PW_BREAK_TYPE_U && local_apic_fired && IS_COLLECTING()) {
            event_type = PW_BREAK_TYPE_IPI;
            /*
             * We need a 'TSC' for this IPI sample but we don't know
             * WHEN the local APIC timer interrupt was raised. Fortunately, it doesn't
             * matter, because we only need to ensure this sample lies
             * BEFORE the corresponding 'C_STATE' sample in a sorted timeline.
             * We therefore simply subtract one from the C_STATE sample TSC to get
             * the IPI sample TSC.
             */
            event_tsc = tsc - 1;
            event_tid = event_pid = -1;
            event_val = 0;
        }


        if (unlikely(init_msr_set_sent == 0)) {
            /*
             * OK, this is the first TPS for this thread during the current collection.
             * Send a "POSIX_TIME_SYNC" message to allow Ring-3 to correlate the TSC used by wuwatch with
             * the "clock_gettime()" used by TPSS.
             */
            {
                struct timespec ts;
                tsc_posix_sync_msg_t tsc_msg;
                u64 tmp_tsc = 0, tmp_nsecs = 0;
                ktime_get_ts(&ts);
                tscval(&tmp_tsc);
                tmp_nsecs = (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
                tsc_msg.tsc_val = tmp_tsc; tsc_msg.posix_mono_val = tmp_nsecs;

                sample.tsc = tsc;
                sample.cpuidx = cpu;
                sample.data_type = TSC_POSIX_MONO_SYNC;
                sample.data_len = sizeof(tsc_msg);
                sample.p_data = (u64)((unsigned long)&tsc_msg);

                pw_produce_generic_msg(&sample, true);
                pw_pr_debug(KERN_INFO "[%d]: SENT POSIX_TIME_SYNC\n", cpu);
                pw_pr_debug(KERN_INFO "[%d]: tsc = %llu posix mono = %llu\n", cpu, tmp_tsc, tmp_nsecs);
            }
            /*
             * 2. Second, the initial MSR 'set' for this (logical) CPU.
             */
            {
                sample.tsc = tsc;
                sample.cpuidx = cpu;
                sample.data_type = C_STATE_MSR_SET;
                // sample.data_len = sizeof(msr_set);
                sample.data_len = num_msrs * sizeof(pw_msr_val_t);
                // sample.p_data = (u64)((unsigned long)msr_set);
                sample.p_data = (u64)((unsigned long)info_set->prev_msr_vals);

                // Why "true"? Document!
                pw_produce_generic_msg(&sample, true);
                pw_pr_debug(KERN_INFO "[%d]: SENT init msr set at tsc = %llu\n", cpu, tsc);
            }
        }

        /*
         * Send the actual TPS message here.
         */
        {
            c_multi_msg_t *cm = (c_multi_msg_t *)info_set->c_multi_msg_mem;
            BUG_ON(!cm);

            sample.cpuidx = cpu;
            sample.tsc = tsc;

            msr_vals = (pw_msr_val_t *)cm->data;

#ifndef __arm__
            cm->mperf = info_set->curr_msr_count[0].val;
#else
            // TODO
            cm->mperf = c0_time;
#endif

            cm->req_state = (u8)state;


            cm->wakeup_tsc = event_tsc;
            cm->wakeup_data = event_val;
            cm->timer_init_cpu = event_init_cpu;
            cm->wakeup_pid = event_pid;
            cm->wakeup_tid = event_tid;
            cm->wakeup_type = event_type;
            cm->num_msrs = num_cx;

            /*
             * 'curr_msr_count[0]' contains the MPERF value, which is encoded separately.
             * We therefore read from 'curr_msr_count[1]'
             */
            memcpy(msr_vals, &info_set->curr_msr_count[1], sizeof(pw_msr_val_t) * num_cx);

#if DO_TPS_EPOCH_COUNTER
            /*
             * We're entering a new TPS "epoch".
             * Increment our counter.
             */
            epoch = inc_tps_epoch_i();
            // epoch = 0x0;
            cm->tps_epoch = epoch;
#endif // DO_TPS_EPOCH_COUNTER

            sample.data_type = C_STATE;
            sample.data_len = sizeof(pw_msr_val_t) * num_cx + C_MULTI_MSG_HEADER_SIZE();
            sample.p_data = (u64)((unsigned long)cm);

            if (false && cpu == 0) {
                printk(KERN_INFO "[%d]: TSC = %llu, #cx = %d, data_len = %u, break_type = %u\n", cpu, sample.tsc, num_cx, sample.data_len, cm->wakeup_type);
            }

            if (false && cpu == 0 && num_cx == 1) {
                printk(KERN_INFO "[%d]: TSC = %llu, 1 Cx MSR counted: id = %u, val = %llu\n", cpu, sample.tsc, info_set->curr_msr_count[1].id.depth, info_set->curr_msr_count[1].val);
            }

            if (IS_COLLECTING()) {
                pw_produce_generic_msg(&sample, true);
            }
        }

        /*
         * Reset the "first-hit" variable.
         */
        {
            __get_cpu_var(wakeup_event_counter).event_tsc = 0;
        }

        if (unlikely(did_alloc)) {
            did_alloc = false;
            pw_kfree(__buffer);
        }
    } // IS_C_STATE_MODE()

    // Collect S and D state / residency counter samples on CPU0
    if (cpu != 0 || !IS_COLLECTING()) {
        return;
    }
};

/*
 * C-state break.
 * Read MSR residencies.
 * Also gather information on what caused C-state break.
 * If so configured, write C-sample information to (per-cpu)
 * output buffer.
 */
#ifdef __arm__
/*
 * TODO: we may want to change this to call receive_wakeup_cause()
 * and put the logic for the ARM stuff in there.  The reason
 * we don't do it now is we want to make sure this isn't called
 * before the timer or interrupt trace calls or we will attribute
 * the cause of the wakeup as this function instead of an interrupt or
 * timer.  We can probably fix that by cleaning up the logic a bit
 * but for the merge we ignore that for now
 */
static void probe_power_end(void *ignore)
{
    u64 tsc = 0;

    msr_set_t *set = NULL;
    u64 *prev_tsc = NULL;
    u64 trace_time = 0;

    tscval(&tsc);
    prev_tsc = &get_cpu_var(trace_power_prev_time);
    trace_time = tsc - *prev_tsc;

    *prev_tsc = tsc;
    put_cpu_var(trace_power_prev_time);

    /*
     * Set all C-state MSRs.
     */
    set = &get_cpu_var(pw_pcpu_msr_sets);
    {
        set->prev_msr_vals[set->prev_req_cstate] += trace_time;

        memset(set->curr_msr_count, 0, sizeof(u64) * MAX_MSR_ADDRESSES);
        set->curr_msr_count[set->prev_req_cstate] =
            set->prev_msr_vals[set->prev_req_cstate];
    }
    put_cpu_var(pw_pcpu_msr_sets);
}
#endif // __arm__

#if APWR_RED_HAT
/*
 * Red Hat back ports SOME changes from 2.6.37 kernel
 * into 2.6.32 kernel. Special case that here.
 */
static void probe_power_start(unsigned int type, unsigned int state, unsigned int cpu_id)
{
    // tps_i(type, state);
    DO_PER_CPU_OVERHEAD_FUNC(tps, type, state);
};
#else
#if LINUX_VERSION_CODE  < KERNEL_VERSION(2,6,38)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_power_start(unsigned int type, unsigned int state)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static void probe_power_start(void *ignore, unsigned int type, unsigned int state)
#else // 2.6.36 <= version < 2.6.38
static void probe_power_start(void *ignore, unsigned int type, unsigned int state, unsigned int cpu_id)
#endif
{
    if (likely(IS_C_STATE_MODE())) {
        DO_PER_CPU_OVERHEAD_FUNC(tps, type, state);
    } else {
        DO_PER_CPU_OVERHEAD_FUNC(tps_lite, false /* boundary */);
    }
};
#else // version >= 2.6.38
static void probe_cpu_idle(void *ignore, unsigned int state, unsigned int cpu_id)
{
   if (state == PWR_EVENT_EXIT) {
#ifdef __arm__
       probe_power_end(NULL);
#endif
       return;
   }

   if (likely(IS_C_STATE_MODE())) {
       DO_PER_CPU_OVERHEAD_FUNC(tps, 0 /*type*/, state);
   } else {
       DO_PER_CPU_OVERHEAD_FUNC(tps_lite, false /* boundary */);
   }
};
#endif // version
#endif // APWR_RED_HAT
#ifdef __arm__
#if TRACE_CPU_HOTPLUG
static void probe_cpu_hotplug(void *ignore, unsigned int state, int cpu_id)
{
        PWCollector_msg_t output_sample;
        event_sample_t event_sample;
        u64 sample_tsc;

        tscval(&sample_tsc);
        output_sample.cpuidx = cpu_id;
        output_sample.tsc = sample_tsc;

        event_sample.data[0] = cpu_id;
        event_sample.data[1] = state;

#if DO_TPS_EPOCH_COUNTER
        event_sample.data[2] = read_tps_epoch_i();
#endif // DO_TPS_EPOCH_COUNTER

        output_sample.data_type = CPUHOTPLUG_SAMPLE;
        output_sample.data_len = sizeof(event_sample);
        output_sample.p_data = (u64)((unsigned long)&event_sample);

        pw_produce_generic_msg(&output_sample, false); // "false" ==> don't wake any sleeping readers (required from scheduling context)
}
#endif // TRACE_CPU_HOTPLUG
#endif // __arm__

#ifndef __arm__
static void tpf(int cpu, unsigned int type, u32 curr_req_freq, u32 prev_req_freq)
{
    u64 tsc = 0, aperf = 0, mperf = 0;
    // u32 prev_req_freq = 0;
    u32 perf_status = 0;
    u32 prev_perf_status = 0;

#if DO_IOCTL_STATS
    stats_t *pstats = NULL;
#endif

    /*
     * We're not guaranteed that 'cpu' (which is the CPU on which the frequency transition is occuring) is
     * the same as the cpu on which the callback i.e. the 'TPF' probe is executing. This is why we use 'WUWATCH_RDMSR_SAFE_ON_CPU()'
     * to read the various MSRs.
     */
    /*
     * Read TSC value
     */
    u32 l=0, h=0;
    WARN_ON(WUWATCH_RDMSR_SAFE_ON_CPU(cpu, 0x10, &l, &h));
    tsc = (u64)h << 32 | (u64)l;
    /*
     * Read CPU_CLK_UNHALTED.REF and CPU_CLK_UNHALTED.CORE. These required ONLY for AXE import
     * backward compatibility!
     */
#if 1
    {
        WARN_ON(WUWATCH_RDMSR_SAFE_ON_CPU(cpu, CORE_CYCLES_MSR_ADDR, &l, &h));
        aperf = (u64)h << 32 | (u64)l;

        WARN_ON(WUWATCH_RDMSR_SAFE_ON_CPU(cpu, REF_CYCLES_MSR_ADDR, &l, &h));
        mperf = (u64)h << 32 | (u64)l;
    }
#endif

    /*
     * Read the IA32_PERF_STATUS MSR. Bits 12:8 (on Atom ) or 15:0 (on big-core) of this determines
     * the frequency the H/W is currently running at.
     * We delegate the actual frequency computation to Ring-3 because the PERF_STATUS encoding is
     * actually model-specific.
     */
    WARN_ON(WUWATCH_RDMSR_SAFE_ON_CPU(cpu, IA32_PERF_STATUS_MSR_ADDR, &l, &h));
    // perf_status = l; // We're only interested in the lower 16 bits!
    /*
     * Update: 'TPF' is FORWARD facing -- make it BACKWARDS facing here.
     */
    {
        prev_perf_status = per_cpu(pcpu_prev_perf_status_val, cpu);
        per_cpu(pcpu_prev_perf_status_val, cpu) = l;
    }
    perf_status = prev_perf_status;
    /*
    if (false) {
        printk(KERN_INFO "[%d]: prev perf status = %u, curr perf status = %u\n", cpu, prev_perf_status, l);
    }
    */

    /*
     * Retrieve the previous requested frequency, if any.
     */
    if (unlikely(prev_req_freq == 0x0)) {
        prev_req_freq = per_cpu(pcpu_prev_req_freq, cpu);
    }
    per_cpu(pcpu_prev_req_freq, cpu) = curr_req_freq;

    produce_p_sample(cpu, tsc, prev_req_freq, perf_status, 0 /* boundary */, aperf, mperf); // "0" ==> NOT a boundary sample

    OUTPUT(0, KERN_INFO "[%d]: TSC = %llu, OLD_req_freq = %u, NEW_REQ_freq = %u, perf_status = %u\n", cpu, tsc, prev_req_freq, curr_req_freq, perf_status);

#if DO_IOCTL_STATS
    {
	pstats = &get_cpu_var(per_cpu_stats);
	local_inc(&pstats->p_trans);
	put_cpu_var(pstats);
    }
#endif // DO_IOCTL_STATS
};
#endif // not def __arm__

#if DO_CPUFREQ_NOTIFIER
/*
 * CPUFREQ notifier callback function.
 * Used in cases where the default
 * power frequency tracepoint mechanism
 * is broken (e.g. MFLD).
 */
static int apwr_cpufreq_notifier(struct notifier_block *block, unsigned long val, void *data)
{
    struct cpufreq_freqs *freq = data;
    u32 old_state = freq->old; // "state" is frequency CPU is ABOUT TO EXECUTE AT
    u32 new_state = freq->new; // "state" is frequency CPU is ABOUT TO EXECUTE AT
    int cpu = freq->cpu;

    if (unlikely(!IS_FREQ_MODE())) {
	return SUCCESS;
    }

    if (val == CPUFREQ_POSTCHANGE) {
#ifndef __arm__
        // DO_PER_CPU_OVERHEAD_FUNC(tpf, cpu, 2, new_state, old_state);
        tpf(cpu, 2, new_state, old_state);
#else
        u64 tsc;
        u32 prev_req_freq = 0;
        u32 perf_status = 0;

        tscval(&tsc);

        prev_req_freq = per_cpu(pcpu_prev_req_freq, cpu);
        per_cpu(pcpu_prev_req_freq, cpu) = state;

        perf_status = prev_req_freq / INTERNAL_STATE.bus_clock_freq_khz;
        produce_p_sample(cpu, tsc, prev_req_freq, perf_status, 0 /* boundary */, 0, 0); // "0" ==> NOT a boundary sample
#endif // not def __arm__
    }
    return SUCCESS;
};

static struct notifier_block apwr_cpufreq_notifier_block = {
    .notifier_call = &apwr_cpufreq_notifier
};

#else // DO_CPUFREQ_NOTIFIER

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38) // Use 'trace_power_frequency()'
/*
 * P-state transition probe.
 *
 * "type" is ALWAYS "2" (i.e. "POWER_PSTATE", see "include/trace/power.h")
 * "state" is the NEXT frequency range the CPU is going to enter (see "arch/x86/kernel/cpu/cpufreq/acpi-cpufreq.c")
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_power_frequency(unsigned int type, unsigned int state)
#else
static void probe_power_frequency(void *ignore, unsigned int type, unsigned int state)
#endif
{
    if(unlikely(!IS_FREQ_MODE())){
        return;
    }
    DO_PER_CPU_OVERHEAD_FUNC(tpf, CPU(), type, state, 0 /* prev freq, 0 ==> use pcpu var */);
};

#else // version >= 2.6.38 ==> Use 'trace_cpu_frequency()'
static void probe_cpu_frequency(void *ignore, unsigned int new_freq, unsigned int cpu)
{
    if(unlikely(!IS_FREQ_MODE())){
        return;
    }
    DO_PER_CPU_OVERHEAD_FUNC(tpf, cpu, 2 /* type, don't care */, new_freq, 0 /* prev freq, 0 ==> use pcpu var */);
};
#endif // LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)
#endif // DO_CPUFREQ_NOTIFIER


/*
 * Helper function for "probe_sched_exit"
 * Useful for overhead measurements.
 */
static void exit_helper(struct task_struct *task)
{
    pid_t tid = task->pid, pid = task->tgid;

    OUTPUT(3, KERN_INFO "[%d]: SCHED_EXIT\n", tid);
    /*
     * Delete all (non-Kernel) timer mappings created
     * for this thread.
     */
    delete_timers_for_tid(tid);
    /*
     * Delete any sys-node mappings created on behalf
     * of this thread.
     */
    check_and_delete_proc_from_sys_list(tid, pid);

};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_sched_process_exit(struct task_struct *task)
#else
    static void probe_sched_process_exit(void *ignore, struct task_struct *task)
#endif
{
    pid_t tid = task->pid, pid = task->tgid;
    const char *name = task->comm;
    u64 tsc;


    OUTPUT(3, KERN_INFO "[%d, %d]: %s exitting\n", tid, pid, name);

    DO_PER_CPU_OVERHEAD_FUNC(exit_helper, task);

    /*
     * Track task exits ONLY IF COLLECTION
     * ONGOING!
     * UPDATE: track if COLLECTION ONGOING OR
     * IF IN PAUSED STATE!
     */
    if(!IS_COLLECTING() && !IS_SLEEPING()){
	return;
    }

    tscval(&tsc);

    produce_r_sample(CPU(), tsc, PW_PROC_EXIT, tid, pid, name);
};

inline void __attribute__((always_inline)) sched_wakeup_helper_i(struct task_struct *task)
{
    int target_cpu = task_cpu(task), source_cpu = CPU();
    /*
     * "Self-sched" samples are "don't care".
     */
    if (target_cpu != source_cpu) {

        PWCollector_msg_t output_sample;
        event_sample_t event_sample;
        u64 sample_tsc;

        tscval(&sample_tsc);
        output_sample.cpuidx = source_cpu;
        output_sample.tsc = sample_tsc;

        event_sample.data[0] = source_cpu;
        event_sample.data[1] = target_cpu;

#if DO_TPS_EPOCH_COUNTER
        event_sample.data[2] = read_tps_epoch_i();
#endif // DO_TPS_EPOCH_COUNTER

        output_sample.data_type = SCHED_SAMPLE;
        output_sample.data_len = sizeof(event_sample);
        output_sample.p_data = (u64)((unsigned long)&event_sample);

        pw_produce_generic_msg(&output_sample, false); // "false" ==> don't wake any sleeping readers (required from scheduling context)
    }
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_sched_wakeup(struct rq *rq, struct task_struct *task, int success)
#else
static void probe_sched_wakeup(void *ignore, struct task_struct *task, int success)
#endif
{
    if (likely(IS_COLLECTING())) {
        sched_wakeup_helper_i(task);
    }
};


inline bool __attribute__((always_inline)) is_sleep_syscall_i(long id)
{
    switch (id) {
        case __NR_poll: // 7
        case __NR_select: // 23
        case __NR_nanosleep: // 35
        case __NR_alarm: // 37
        case __NR_setitimer: // 38
        case __NR_rt_sigtimedwait: // 128
        case __NR_futex: // 202
        case __NR_timer_settime: // 223
        case __NR_clock_nanosleep: // 230
        case __NR_epoll_wait: // 232
        case __NR_pselect6: // 270
        case __NR_ppoll: // 271
        case __NR_epoll_pwait: // 281
        case __NR_timerfd_settime: // 286
            return true;
        default:
            break;
    }
    return false;
};

inline void  __attribute__((always_inline)) sys_enter_helper_i(long id, pid_t tid, pid_t pid)
{
    if (check_and_add_proc_to_sys_list(tid, pid)) {
        pw_pr_error("ERROR: could NOT add proc to sys list!\n");
    }
    return;
};

inline void  __attribute__((always_inline)) sys_exit_helper_i(long id, pid_t tid, pid_t pid)
{
    check_and_remove_proc_from_sys_list(tid, pid);
};


#if DO_PROBE_ON_SYSCALL_ENTER_EXIT
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_sys_enter(struct pt_regs *regs, long ret)
#else
static void probe_sys_enter(void *ignore, struct pt_regs *regs, long ret)
#endif
{
    long id = syscall_get_nr(current, regs);
    pid_t tid = TID(), pid = PID();

    if (is_sleep_syscall_i(id)) {
        DO_PER_CPU_OVERHEAD_FUNC(sys_enter_helper_i, id, tid, pid);
    }
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_sys_exit(struct pt_regs *regs, long ret)
#else
static void probe_sys_exit(void *ignore, struct pt_regs *regs, long ret)
#endif
{
    long id = syscall_get_nr(current, regs);
    pid_t tid = TID(), pid = PID();

    DO_PER_CPU_OVERHEAD_FUNC(sys_exit_helper_i, id, tid, pid);

    if(id == __NR_execve && IS_COLLECTING()){
        u64 tsc;

        tscval(&tsc);
        OUTPUT(3, KERN_INFO "[%d]: EXECVE ENTER! TID = %d, NAME = %.20s\n", CPU(), TID(), NAME());
        produce_r_sample(CPU(), tsc, PW_PROC_EXEC, TID(), PID(), NAME());
    }
};
#endif // DO_PROBE_ON_SYSCALL_ENTER_EXIT

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_sched_process_fork(struct task_struct *parent, struct task_struct *child)
#else
    static void probe_sched_process_fork(void *ignore, struct task_struct *parent, struct task_struct *child)
#endif
{
    const char *cname = child->comm;
    pid_t ctid = child->pid, cpid = child->tgid;
    u64 tsc;

    tscval(&tsc);

    OUTPUT(3, KERN_INFO "DEBUG: PROCESS_FORK: %d (%.20s) --> %d (%.20s) \n", parent->pid, parent->comm, child->pid, cname);

    if (IS_COLLECTING() || IS_SLEEPING()) {
        produce_r_sample(CPU(), tsc, PW_PROC_FORK, ctid, cpid, cname);
    }
};

/*
 * Notifier for module loads and frees.
 * We register module load and free events -- extract memory bounds for
 * the module (on load). Also track TID, NAME for tracking device driver timers.
 */
int apwr_mod_notifier(struct notifier_block *block, unsigned long val, void *data)
{
    struct module *mod = data;
    int cpu = CPU();
    const char *name = mod->name;
    unsigned long module_core = (unsigned long)mod->module_core;
    unsigned long core_size = mod->core_size;

    if (IS_COLLECTING() || IS_SLEEPING()) {
        if (val == MODULE_STATE_COMING) {
            OUTPUT(0, KERN_INFO "COMING: tid = %d, pid = %d, name = %s, module_core = %lu\n", TID(), PID(), name, module_core);
            produce_m_sample(cpu, name, module_core, core_size);
        } else if (val == MODULE_STATE_GOING) {
            OUTPUT(0, KERN_INFO "GOING: tid = %d, pid = %d, name = %s\n", TID(), PID(), name);
        }
    }
    return SUCCESS;
};

static struct notifier_block apwr_mod_notifier_block = {
    .notifier_call = &apwr_mod_notifier
};


#if DO_WAKELOCK_SAMPLE
/*
 * Wakelock hooks
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_wake_lock(struct wake_lock *lock)
#else
static void probe_wake_lock(void *ignore, struct wake_lock *lock)
#endif
{
    u64 tsc;
    u64 timeout = 0;
    w_sample_type_t wtype;

    /*
     * Track task exits ONLY IF COLLECTION
     * ONGOING OR IF IN PAUSED STATE!
     */
    if(!IS_COLLECTING() && !IS_SLEEPING()){
	return;
    }

    tscval(&tsc);
    if (lock->flags & (1U << 10)){   // Check if WAKE_LOCK_AUTO_EXPIRE is flagged
        wtype = PW_WAKE_LOCK_TIMEOUT;
        timeout = jiffies_to_msecs(lock->expires - jiffies);
    }else{
        wtype = PW_WAKE_LOCK;
    }

    BUG_ON(!lock->name);
    produce_w_sample(CPU(), tsc, wtype , TID(), PID(), lock->name, NAME(), timeout);

    OUTPUT(0, "wake_lock: type=%d, name=%s, timeout=%llu (msec), CPU=%d, PID=%d, TSC=%llu\n", wtype, lock->name, timeout, CPU(), PID(), tsc);
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static void probe_wake_unlock(struct wake_lock *lock)
#else
static void probe_wake_unlock(void *ignore, struct wake_lock *lock)
#endif
{
    u64 tsc;

    /*
     * Track task exits ONLY IF COLLECTION
     * ONGOING OR IF IN PAUSED STATE!
     */
    if(!IS_COLLECTING() && !IS_SLEEPING()){
        return;
    }

    tscval(&tsc);

    produce_w_sample(CPU(), tsc, PW_WAKE_UNLOCK, TID(), PID(), lock->name, NAME(), 0);

    OUTPUT(0, "wake_unlock: name=%s, CPU=%d, PID=%d, TSC=%llu\n", lock->name, CPU(), PID(), tsc);
};

#else
static void probe_wakeup_source_activate(void *ignore, const char *name, unsigned int state)
{
    u64 tsc;
    u64 timeout = 0;
    w_sample_type_t wtype;

    if (name == NULL) {
        printk(KERN_INFO "wake_lock: name=UNKNOWNs, state=%u\n", state);
        return;
    }

    /*
     * Track task exits ONLY IF COLLECTION
     * ONGOING OR IF IN PAUSED STATE!
     */
    if(!IS_COLLECTING() && !IS_SLEEPING()){
	return;
    }

    tscval(&tsc);
    wtype = PW_WAKE_LOCK;

    produce_w_sample(CPU(), tsc, wtype , TID(), PID(), name, NAME(), timeout);

    OUTPUT(0, "wake_lock: type=%d, name=%s, timeout=%llu (msec), CPU=%d, PID=%d, TSC=%llu\n", wtype, name, timeout, CPU(), PID(), tsc);
};

static void probe_wakeup_source_deactivate(void *ignore, const char *name, unsigned int state)
{
    u64 tsc;

    if (name == NULL) {
        printk(KERN_INFO "wake_unlock: name=UNKNOWNs, state=%u\n", state);
        return;
    }

    /*
     * Track task exits ONLY IF COLLECTION
     * ONGOING OR IF IN PAUSED STATE!
     */
    if(!IS_COLLECTING() && !IS_SLEEPING()){
        return;
    }

    tscval(&tsc);

    produce_w_sample(CPU(), tsc, PW_WAKE_UNLOCK, TID(), PID(), name, NAME(), 0);

    OUTPUT(0, "wake_unlock: name=%s, CPU=%d, PID=%d, TSC=%llu\n", name, CPU(), PID(), tsc);
};
#endif
#endif

static int register_timer_callstack_probes(void)
{
    int ret = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
    {
	OUTPUT(0, KERN_INFO "\tTIMER_INIT_EVENTS");
	ret = register_trace_hrtimer_init(probe_hrtimer_init);
	WARN_ON(ret);
	ret = register_trace_timer_init(probe_timer_init);
	WARN_ON(ret);
    }

    {
	OUTPUT(0, KERN_INFO "\tITIMER_STATE_EVENTS");
	ret = register_trace_itimer_state(probe_itimer_state);
	WARN_ON(ret);
    }

    {
	OUTPUT(0, KERN_INFO "\tTIMER_START_EVENTS");
	ret = register_trace_hrtimer_start(probe_hrtimer_start);
	WARN_ON(ret);
    }

    {
	OUTPUT(0, KERN_INFO "\tSCHED_EXIT_EVENTS");
	ret = register_trace_sched_process_exit(probe_sched_process_exit);
	WARN_ON(ret);
    }

#else

    {
	OUTPUT(0, KERN_INFO "\tTIMER_INIT_EVENTS");
	ret = register_trace_hrtimer_init(probe_hrtimer_init, NULL);
	WARN_ON(ret);
	ret = register_trace_timer_init(probe_timer_init, NULL);
	WARN_ON(ret);
    }

    {
	OUTPUT(0, KERN_INFO "\tITIMER_STATE_EVENTS");
	ret = register_trace_itimer_state(probe_itimer_state, NULL);
	WARN_ON(ret);
    }

    {
	OUTPUT(0, KERN_INFO "\tTIMER_START_EVENTS");
	ret = register_trace_hrtimer_start(probe_hrtimer_start, NULL);
	WARN_ON(ret);
    }

    {
	OUTPUT(0, KERN_INFO "\tSCHED_EVENTS");
	ret = register_trace_sched_process_exit(probe_sched_process_exit, NULL);
	WARN_ON(ret);
    }

#endif // KERNEL_VER
    return SUCCESS;
};

static void unregister_timer_callstack_probes(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
    {
	unregister_trace_hrtimer_init(probe_hrtimer_init);
	unregister_trace_timer_init(probe_timer_init);

	tracepoint_synchronize_unregister();
    }

    {
	unregister_trace_itimer_state(probe_itimer_state);

	tracepoint_synchronize_unregister();
    }

    {
	unregister_trace_hrtimer_start(probe_hrtimer_start);

	tracepoint_synchronize_unregister();
    }

    {
	unregister_trace_sched_process_exit(probe_sched_process_exit);

	tracepoint_synchronize_unregister();
    }

#else

    {
	unregister_trace_hrtimer_init(probe_hrtimer_init, NULL);
	unregister_trace_timer_init(probe_timer_init, NULL);

	tracepoint_synchronize_unregister();
    }

    {
	unregister_trace_itimer_state(probe_itimer_state, NULL);

	tracepoint_synchronize_unregister();
    }

    {
	unregister_trace_hrtimer_start(probe_hrtimer_start, NULL);

	tracepoint_synchronize_unregister();
    }

    {
	unregister_trace_sched_process_exit(probe_sched_process_exit, NULL);

	tracepoint_synchronize_unregister();
    }

#endif // KERNEL_VER
};

/*
 * Register all probes which should be registered
 * REGARDLESS OF COLLECTION STATUS.
 */
static int register_permanent_probes(void)
{
    if (probe_on_syscalls) {
#if DO_PROBE_ON_SYSCALL_ENTER_EXIT
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
        {
            WARN_ON(register_trace_sys_enter(probe_sys_enter));
            WARN_ON(register_trace_sys_exit(probe_sys_exit));
        }
#else // LINUX_VERSION
        {
            WARN_ON(register_trace_sys_enter(probe_sys_enter, NULL));
            WARN_ON(register_trace_sys_exit(probe_sys_exit, NULL));
        }
#endif // LINUX_VERSION
#endif // DO_PROBE_ON_SYSCALL_ENTER_EXIT
    }
    return register_timer_callstack_probes();
};

static void unregister_permanent_probes(void)
{
    if (probe_on_syscalls) {
#if DO_PROBE_ON_SYSCALL_ENTER_EXIT
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
        {
            unregister_trace_sys_enter(probe_sys_enter);
            unregister_trace_sys_exit(probe_sys_exit);

            tracepoint_synchronize_unregister();
        }
#else // LINUX_VERSION
        {
            unregister_trace_sys_enter(probe_sys_enter, NULL);
            unregister_trace_sys_exit(probe_sys_exit, NULL);

            tracepoint_synchronize_unregister();
        }
#endif // LINUX_VERSION
#endif // DO_PROBE_ON_SYSCALL_ENTER_EXIT
    }

    unregister_timer_callstack_probes();
};

/*
 * Register all probes which should be registered
 * ONLY FOR AN ONGOING, NON-PAUSED COLLECTION.
 */
static int register_non_pausable_probes(void)
{
    // timer expire
    // irq
    // tps
    // tpf
    int ret = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)

    /*
     * ONLY required for "SLEEP" mode i.e. C-STATES
     */
    if (IS_SLEEP_MODE() || IS_C_STATE_MODE()) {
        if(IS_C_STATE_MODE()){
            OUTPUT(0, KERN_INFO "C_STATE MODE REQUESTED\n");
            {
                OUTPUT(0, KERN_INFO "\tTRACE_BREAK_EVENTS");
                ret = register_trace_timer_expire_entry(probe_timer_expire_entry);
                WARN_ON(ret);
                ret = register_trace_hrtimer_expire_entry(probe_hrtimer_expire_entry);
                WARN_ON(ret);
                ret = register_trace_hrtimer_expire_exit(probe_hrtimer_expire_exit);
                WARN_ON(ret);
                ret = register_trace_irq_handler_entry(probe_irq_handler_entry);
                WARN_ON(ret);
                ret = register_trace_softirq_entry(probe_softirq_entry);
                WARN_ON(ret);
                ret = register_trace_sched_wakeup(probe_sched_wakeup);
                WARN_ON(ret);
                ret = register_trace_workqueue_execution(probe_workqueue_execution);
                if (ret) {
                    printk(KERN_INFO "WARNING: trace_workqueue_execute_start did NOT succeed!\n");
                }
                // WARN_ON(ret);
            }
        }
        {
            OUTPUT(0, KERN_INFO "\tCSTATE_EVENTS");
            ret = register_trace_power_start(probe_power_start);
            WARN_ON(ret);
        }
#ifdef __arm__
        {
            OUTPUT(0, KERN_INFO "\tCSTATE_EVENTS");
            ret = register_trace_power_end(probe_power_end);
            WARN_ON(ret);
        }
#endif // __arm__
    }


#else // KERNEL_VER

    /*
     * ONLY required for "SLEEP" mode i.e. C-STATES
     */
    if (IS_SLEEP_MODE() || IS_C_STATE_MODE()) {
        if(IS_C_STATE_MODE()){
            OUTPUT(0, KERN_INFO "C_STATE MODE REQUESTED\n");
            {
                OUTPUT(0, KERN_INFO "\tTRACE_BREAK_EVENTS");
                ret = register_trace_timer_expire_entry(probe_timer_expire_entry, NULL);
                WARN_ON(ret);
                ret = register_trace_hrtimer_expire_entry(probe_hrtimer_expire_entry, NULL);
                WARN_ON(ret);
                ret = register_trace_hrtimer_expire_exit(probe_hrtimer_expire_exit, NULL);
                WARN_ON(ret);
                ret = register_trace_irq_handler_entry(probe_irq_handler_entry, NULL);
                WARN_ON(ret);
                ret = register_trace_softirq_entry(probe_softirq_entry, NULL);
                WARN_ON(ret);
                ret = register_trace_sched_wakeup(probe_sched_wakeup, NULL);
                WARN_ON(ret);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
                {
                    ret = register_trace_workqueue_execution(probe_workqueue_execution, NULL);
                    if (ret) {
                        printk(KERN_INFO "WARNING: trace_workqueue_execute_start did NOT succeed!\n");
                    }
                    // WARN_ON(ret);
                }
#else // 2.6.36 <= version < 2.6.38
                {
                    ret = register_trace_workqueue_execute_start(probe_workqueue_execute_start, NULL);
                    if (ret) {
                        printk(KERN_INFO "WARNING: trace_workqueue_execute_start did NOT succeed!\n");
                    }
                    // WARN_ON(ret);
                }
#endif // version < 2.6.36
            }
        }
        /*
         * ONLY required for "SLEEP" mode i.e. C-STATES
         */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)
        {
            OUTPUT(0, KERN_INFO "\tCSTATE_EVENTS");
            ret = register_trace_power_start(probe_power_start, NULL);
            WARN_ON(ret);
        }
#ifdef __arm__
        {
            OUTPUT(0, KERN_INFO "\tCSTATE_EVENTS");
            ret = register_trace_power_end(probe_power_end, NULL);
            WARN_ON(ret);
        }
#endif // __arm__
#else // version >= 2.6.38
        {
            OUTPUT(0, KERN_INFO "\tCSTATE_EVENTS");
            ret = register_trace_cpu_idle(probe_cpu_idle, NULL);
            WARN_ON(ret);
        }
#ifdef __arm__
#if TRACE_CPU_HOTPLUG
        {
            OUTPUT(0, KERN_INFO "\tCPU_ON_OFF_EVENTS");
            ret = register_trace_cpu_hotplug(probe_cpu_hotplug, NULL);
            WARN_ON(ret);
        }
#endif // TRACE_CPU_HOTPLUG
#endif // __arm__
        if (IS_C_STATE_MODE()) {
            ret = register_trace_workqueue_execute_start(probe_workqueue_execute_start, NULL);
            if (ret) {
                printk(KERN_INFO "WARNING: trace_workqueue_execute_start did NOT succeed!\n");
            }
            // WARN_ON(ret);
        }
#endif // LINUX_VERSION_CODE < 2.6.38
    }

#endif // KERNEL_VER
    return SUCCESS;
};

static void unregister_non_pausable_probes(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
    // #if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)

    /*
     * ONLY required for "SLEEP" mode i.e. C-STATES
     */
    if (IS_SLEEP_MODE() || IS_C_STATE_MODE()) {
        if(IS_C_STATE_MODE()){
            OUTPUT(0, KERN_INFO "C_STATE MODE REQUESTED\n");
            {
                unregister_trace_timer_expire_entry(probe_timer_expire_entry);
                unregister_trace_hrtimer_expire_entry(probe_hrtimer_expire_entry);
                unregister_trace_hrtimer_expire_exit(probe_hrtimer_expire_exit);
                unregister_trace_irq_handler_entry(probe_irq_handler_entry);
                unregister_trace_softirq_entry(probe_softirq_entry);
                unregister_trace_sched_wakeup(probe_sched_wakeup);
                unregister_trace_workqueue_execution(probe_workqueue_execution);

                tracepoint_synchronize_unregister();
            }
            /*
             * ONLY required for "SLEEP" mode i.e. C-STATES
             */
            {
                unregister_trace_power_start(probe_power_start);
#ifdef __arm__
                unregister_trace_power_end(probe_power_end);
#endif // __arm__
                tracepoint_synchronize_unregister();
            }
        }
    }


#else // KERNEL_VER

    /*
     * ONLY required for "SLEEP" mode i.e. C-STATES
     */
    if (IS_SLEEP_MODE() || IS_C_STATE_MODE()) {
        if(IS_C_STATE_MODE()){
            OUTPUT(0, KERN_INFO "C_STATE MODE REQUESTED\n");
            {
                unregister_trace_timer_expire_entry(probe_timer_expire_entry, NULL);
                unregister_trace_hrtimer_expire_entry(probe_hrtimer_expire_entry, NULL);
                unregister_trace_hrtimer_expire_exit(probe_hrtimer_expire_exit, NULL);
                unregister_trace_irq_handler_entry(probe_irq_handler_entry, NULL);
                unregister_trace_softirq_entry(probe_softirq_entry, NULL);
                unregister_trace_sched_wakeup(probe_sched_wakeup, NULL);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
                {
                    unregister_trace_workqueue_execution(probe_workqueue_execution, NULL);

                    tracepoint_synchronize_unregister();
                }
#else // 2.6.36 <= version < 2.6.38
                {
                    unregister_trace_workqueue_execute_start(probe_workqueue_execute_start, NULL);

                    tracepoint_synchronize_unregister();
                }
#endif // version < 2.6.36

                tracepoint_synchronize_unregister();
            }
        }
        /*
         * ONLY required for "SLEEP" mode i.e. C-STATES
         */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)
        {
            unregister_trace_power_start(probe_power_start, NULL);
#ifdef __arm__
            unregister_trace_power_end(probe_power_end, NULL);
#endif // __arm__
            tracepoint_synchronize_unregister();
        }
#else // version >= 2.6.38
        {
            unregister_trace_cpu_idle(probe_cpu_idle, NULL);

            tracepoint_synchronize_unregister();
        }
#ifdef __arm__
#if TRACE_CPU_HOTPLUG
        {
            unregister_trace_cpu_hotplug(probe_cpu_hotplug, NULL);

            tracepoint_synchronize_unregister();
        }
#endif // TRACE_CPU_HOTPLUG
#endif // __arm__
        if (IS_C_STATE_MODE()) {
            unregister_trace_workqueue_execute_start(probe_workqueue_execute_start, NULL);

            tracepoint_synchronize_unregister();
        }
#endif // LINUX_VERSION_CODE < 2.6.38
    }

#endif // KERNEL_VER
};

/*
 * Register all probes which must be registered
 * ONLY FOR AN ONGOING (i.e. START/PAUSED) COLLECTION.
 */
static int register_pausable_probes(void)
{
    int ret = 0;
    // sys_exit
    // sched_fork
    // module_notifier
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
    /*
     * ALWAYS required.
     */
    {
	OUTPUT(0, KERN_INFO "\tMOD_NOTIFIER_EVENTS");
        register_module_notifier(&apwr_mod_notifier_block);
    }
    /*
     * ALWAYS required.
     */
    {
	OUTPUT(0, KERN_INFO "\tSCHED_FORK_EVENTS");
	ret = register_trace_sched_process_fork(probe_sched_process_fork);
	WARN_ON(ret);
    }

    /*
     * ALWAYS required.
     */
#if DO_PROBE_ON_SYSCALL_ENTER_EXIT

#endif // DO_PROBE_ON_SYSCALL_ENTER_EXIT

    /*
     * ONLY required for "FREQ" mode i.e. P-STATES
     */
    if(IS_FREQ_MODE()){
	OUTPUT(0, KERN_INFO "FREQ MODE REQUESTED\n");
#if DO_CPUFREQ_NOTIFIER
	{
	    OUTPUT(0, KERN_INFO "\tPSTATE_EVENTS\n");
	    cpufreq_register_notifier(&apwr_cpufreq_notifier_block, CPUFREQ_TRANSITION_NOTIFIER);
	}
#else // DO_CPUFREQ_NOTIFIER
	{
	    OUTPUT(0, KERN_INFO "\tPSTATE_EVENTS\n");
	    ret = register_trace_power_frequency(probe_power_frequency);
	    WARN_ON(ret);
	}
#endif // DO_CPUFREQ_NOTIFIER
    }

    if(IS_WAKELOCK_MODE()){
#if DO_WAKELOCK_SAMPLE
        OUTPUT(0, KERN_INFO "\tWAKELOCK_EVENTS");
        ret = register_trace_wake_lock(probe_wake_lock);
        WARN_ON(ret);

        OUTPUT(0, KERN_INFO "\tWAKEUNLOCK_EVENTS");
        ret = register_trace_wake_unlock(probe_wake_unlock);
        WARN_ON(ret);
#endif
    }

#else // KERNEL_VERSION >= 2.6.35

    /*
     * ALWAYS required.
     */
    {
	OUTPUT(0, KERN_INFO "\tMOD_NOTIFIER_EVENTS");
        register_module_notifier(&apwr_mod_notifier_block);
    }

    /*
     * ALWAYS required.
     */
    {
	OUTPUT(0, KERN_INFO "\tSCHED_FORK_EVENTS");
	ret = register_trace_sched_process_fork(probe_sched_process_fork, NULL);
	WARN_ON(ret);
    }

    /*
     * ALWAYS required.
     */
#if DO_PROBE_ON_SYSCALL_ENTER_EXIT

#endif // DO_PROBE_ON_SYSCALL_ENTER_EXIT

    /*
     * ONLY required for "FREQ" mode i.e. P-STATES
     */
    if(IS_FREQ_MODE()){
	OUTPUT(0, KERN_INFO "FREQ MODE REQUESTED!\n");
#if DO_CPUFREQ_NOTIFIER
	{
	    OUTPUT(0, KERN_INFO "\tPSTATE_EVENTS\n");
	    cpufreq_register_notifier(&apwr_cpufreq_notifier_block, CPUFREQ_TRANSITION_NOTIFIER);
	}
#else // DO_CPUFREQ_NOTIFIER
	{
	    OUTPUT(0, KERN_INFO "\tPSTATE_EVENTS\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38) // Use 'trace_power_frequency()'
	    ret = register_trace_power_frequency(probe_power_frequency, NULL);
#else // Use 'trace_cpu_frequency()'
	    ret = register_trace_cpu_frequency(probe_cpu_frequency, NULL);
#endif // LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)
	    WARN_ON(ret);
	}
#endif // DO_CPUFREQ_NOTIFIER
    }

    if(IS_WAKELOCK_MODE()){
#if DO_WAKELOCK_SAMPLE
        OUTPUT(0, KERN_INFO "\tWAKELOCK_EVENTS");
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
        ret = register_trace_wake_lock(probe_wake_lock, NULL);
#else
        ret = register_trace_wakeup_source_activate(probe_wakeup_source_activate, NULL);
#endif
        WARN_ON(ret);

        OUTPUT(0, KERN_INFO "\tWAKEUNLOCK_EVENTS");
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
        ret = register_trace_wake_unlock(probe_wake_unlock, NULL);
#else
        ret = register_trace_wakeup_source_deactivate(probe_wakeup_source_deactivate, NULL);
#endif
        WARN_ON(ret);
#endif
    }

#endif // KERNEL_VER

    return SUCCESS;
};

static void unregister_pausable_probes(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)

    /*
     * ALWAYS required.
     */
    {
        unregister_module_notifier(&apwr_mod_notifier_block);
    }

    /*
     * ALWAYS required.
     */
    {
	unregister_trace_sched_process_fork(probe_sched_process_fork);

	tracepoint_synchronize_unregister();
    }

    /*
     * ALWAYS required.
     */
    /*
     * ONLY required for "FREQ" mode i.e. P-STATES
     */
    if(IS_FREQ_MODE()){
	OUTPUT(0, KERN_INFO "FREQ MODE REQUESTED!\n");
#if DO_CPUFREQ_NOTIFIER
	{
	    OUTPUT(0, KERN_INFO "\tPSTATE_EVENTS\n");
	    cpufreq_unregister_notifier(&apwr_cpufreq_notifier_block, CPUFREQ_TRANSITION_NOTIFIER);
	}
#else // DO_CPUFREQ_NOTIFIER
	{
	    unregister_trace_power_frequency(probe_power_frequency);

	    tracepoint_synchronize_unregister();
	}
#endif // DO_CPUFREQ_NOTIFIER
    }

    if(IS_WAKELOCK_MODE()){
#if DO_WAKELOCK_SAMPLE
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
        unregister_trace_wake_lock(probe_wake_lock);
        unregister_trace_wake_unlock(probe_wake_unlock);
#else
        unregister_trace_wakeup_source_activate(probe_wakeup_source_activate);
        unregister_trace_wakeup_source_deactivate(probe_wakeup_source_deactivate);
#endif

        tracepoint_synchronize_unregister();
#endif
    }

#else // Kernel version >= 2.6.35

    /*
     * ALWAYS required.
     */
    {
        unregister_module_notifier(&apwr_mod_notifier_block);
    }

    /*
     * ALWAYS required.
     */
    {
	unregister_trace_sched_process_fork(probe_sched_process_fork, NULL);

	tracepoint_synchronize_unregister();
    }

    /*
     * ALWAYS required.
     */

    /*
     * ONLY required for "FREQ" mode i.e. P-STATES
     */
    if(IS_FREQ_MODE()){
	OUTPUT(0, KERN_INFO "FREQ MODE REQUESTED\n");
#if DO_CPUFREQ_NOTIFIER
	{
	    OUTPUT(0, KERN_INFO "\tPSTATE_EVENTS\n");
	    cpufreq_unregister_notifier(&apwr_cpufreq_notifier_block, CPUFREQ_TRANSITION_NOTIFIER);
	}
#else // DO_CPUFREQ_NOTIFIER
	{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38) // Use 'trace_power_frequency()'
	    unregister_trace_power_frequency(probe_power_frequency, NULL);
#else // Use 'trace_cpu_frequency()'
            unregister_trace_cpu_frequency(probe_cpu_frequency, NULL);
#endif // LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)

	    tracepoint_synchronize_unregister();
	}
#endif // DO_CPUFREQ_NOTIFIER
    }

    if(IS_WAKELOCK_MODE()){
#if DO_WAKELOCK_SAMPLE
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
        unregister_trace_wake_lock(probe_wake_lock, NULL);
        unregister_trace_wake_unlock(probe_wake_unlock, NULL);
#else
        unregister_trace_wakeup_source_activate(probe_wakeup_source_activate, NULL);
        unregister_trace_wakeup_source_deactivate(probe_wakeup_source_deactivate, NULL);
#endif

        tracepoint_synchronize_unregister();
#endif
    }

#endif // KERNEL_VER
};

/*
 * Service a "read(...)" call from user-space.
 *
 * Returns sample information back to the user.
 * When a user calls the "read" function, the device
 * driver first checks if any (per-cpu) output buffers are full.
 * If yes, then the entire contents of that buffer are
 * copied to the user. If not, then the user blocks, until the
 * buffer-full condition is met.
 */
static ssize_t pw_device_read(struct file *file, char __user *buffer, size_t length, loff_t *offset)
{
    u32 val = 0;
    bool is_flush_mode = INTERNAL_STATE.drain_buffers;

    if (!buffer) {
        pw_pr_error("ERROR: \"read\" called with an empty buffer?!\n");
        return -ERROR;
    }

#if 0
    while (pw_any_seg_full(&val, is_flush_mode) == false) {
        if (val == PW_ALL_WRITES_DONE_MASK) {
            BUG_ON(IS_COLLECTING());
            return 0; // "0" ==> EOF
        }
        val = PW_ALL_WRITES_DONE_MASK;
        if (wait_event_interruptible(pw_reader_queue, ( (!IS_COLLECTING() && !IS_SLEEPING()) || pw_any_seg_full(&val, false /* is flush mode */)))) {
            pw_pr_error("wait_event_interruptible error\n");
            return -ERESTARTSYS;
        }
        /*
         * OK, we were woken up. This can be because we have a full buffer or
         * because a 'STOP/CANCEL' cmd was issued. In the first case, we will have a valid
         * value for 'val' so check for that here.
         */
        if (val != PW_ALL_WRITES_DONE_MASK) {
            // we have a full buffer to return
            break;
        }
        /*
         * No full buffer exists; we may have been woken up because of a 'STOP' cmd. Loop
         * back and check.
         */
        // is_flush_mode = !IS_COLLECTING() && !IS_SLEEPING();
        is_flush_mode = INTERNAL_STATE.drain_buffers;
    }
#else // if 1
    do {
        val = PW_ALL_WRITES_DONE_MASK; is_flush_mode = INTERNAL_STATE.drain_buffers;
        pw_pr_debug(KERN_INFO "Waiting, flush = %s\n", GET_BOOL_STRING(is_flush_mode));
        if (wait_event_interruptible(pw_reader_queue, (pw_any_seg_full(&val, &INTERNAL_STATE.drain_buffers) || (!IS_COLLECTING() && !IS_SLEEPING())))) {
            pw_pr_error("wait_event_interruptible error\n");
            return -ERESTARTSYS;
        }
        pw_pr_debug(KERN_INFO "After wait: val = %u\n", val);
    } while (val == PW_NO_DATA_AVAIL_MASK);
#endif // if 0
    /*
     * Are we done producing/consuming?
     */
    if (val == PW_ALL_WRITES_DONE_MASK) {
        return 0; // "0" ==> EOF
    }
    /*
     * 'mmap' unsupported, for now
     */
    if (false && pw_did_mmap) {
        if (put_user(val, (u32 *)buffer)) {
            pw_pr_error("ERROR in put_user\n");
            return -ERROR;
        }
        return sizeof(val); // 'read' returns # of bytes actually read
    } else {
        /*
         * Copy the buffer contents into userspace.
         */
        size_t bytes_read = 0;
        unsigned long bytes_not_copied = pw_consume_data(val, buffer, length, &bytes_read); // 'read' returns # of bytes actually read
        pw_pr_debug(KERN_INFO "OK: returning %u\n", (unsigned)bytes_read);
        if (unlikely(bytes_not_copied)) {
            return -ERROR;
        }
        return bytes_read;
    }
};

static unsigned int pw_device_poll(struct file *filp, poll_table *wait)
{
    unsigned int mask = 0;
    u32 dummy = 0;

    poll_wait(filp, &pw_reader_queue, wait);

    if (!IS_COLLECTING() || pw_any_seg_full(&dummy, &INTERNAL_STATE.drain_buffers)) { // device is readable if: (a) NOT collecting or (b) any buffers is full
	mask = (POLLIN | POLLRDNORM);
    }

    return mask;
};

/*
 * 'mmap' unsupported, for now
 */
static int pw_device_mmap(struct file *filp, struct vm_area_struct *vma)
{
    long length = vma->vm_end - vma->vm_start;
    unsigned long total_size = 0;

    pw_pr_debug("MMAP received!\n");

    if (true) {
        return -ERROR;
    }

    /*
     * Check size restrictions.
     */
    if (length != pw_buffer_alloc_size) {
        pw_pr_error("ERROR: requested mapping size %ld bytes, MUST be %lu\n", length, pw_buffer_alloc_size);
        return -ERROR;
    }

    if (pw_map_per_cpu_buffers(vma, &total_size)) {
        pw_pr_error("ERROR mapping per-cpu buffers to userspace!\n");
        return -ERROR;
    }

    /*
     * Sanity!
     */
    if (total_size != length) {
        pw_pr_warn("WARNING: mmap: total size = %lu, length = %lu\n", total_size, length);
    } else {
        pw_pr_debug("OK: mmap total size = %lu, length = %lu\n", total_size, length);
    }

    pw_did_mmap = true;

    return SUCCESS;
};


// "copy_from_user" ==> dst, src
#define EXTRACT_LOCAL_ARGS(l,u) copy_from_user((l), (u), sizeof(struct PWCollector_ioctl_arg))

/*
 * Check if command is valid, given current state.
 */
static inline bool is_cmd_valid(PWCollector_cmd_t cmd)
{
    bool is_collecting = IS_COLLECTING(), is_sleeping = IS_SLEEPING();

    if(is_sleeping){
	/*
	 * If currently PAUSEd, the ONLY command
	 * that's NOT allowed is a subsequent PAUSE.
	 */
	if(cmd == PW_PAUSE)
	    return false;
    }
    else if(is_collecting && (cmd == PW_START || cmd == PW_RESUME))
    	return false;
    else if(!is_collecting && (cmd == PW_STOP || cmd == PW_PAUSE || cmd == PW_CANCEL))
    	return false;

    return true;
};

/*
 * Retrieve the base operating frequency
 * for this CPU. The base frequency acts
 * as a THRESHOLD indicator for TURBO -- frequencies
 * ABOVE this are considered TURBO.
 */
static inline void get_base_operating_frequency(void)
{
#ifndef __arm__

    if(!INTERNAL_STATE.bus_clock_freq_khz){
        pw_pr_error("ERROR: cannot set base_operating_frequency until we have a bus clock frequency!\n");
        return;
    }

    base_operating_freq_khz = pw_max_non_turbo_ratio * INTERNAL_STATE.bus_clock_freq_khz;
    pw_pr_debug("RATIO = 0x%x, BUS_FREQ = %u, FREQ = %u\n", (u32)pw_max_non_turbo_ratio , (u32)INTERNAL_STATE.bus_clock_freq_khz, base_operating_freq_khz);
#else
    struct cpufreq_policy *policy;
    if( (policy = cpufreq_cpu_get(0)) == NULL){
        base_operating_freq_khz = policy->max;
    }
#endif // ifndef __arm__
};

/*
 * Set initial config params.
 * These include MSR addresses, and power
 * collection switches.
 */
int set_config(struct PWCollector_config __user *remote_config, int size)
{
    int i=0;
    struct PWCollector_config local_config;

    if (!remote_config) {
        pw_pr_error("ERROR: NULL remote_config value?!\n");
        return -ERROR;
    }

    if( (i = copy_from_user(&local_config, remote_config, sizeof(local_config) /*size*/))) // "copy_from_user" returns number of bytes that COULD NOT be copied
	return i;
    /*
     * Copy Core/Pkg MSR addresses
     */
    memcpy(INTERNAL_STATE.coreResidencyMSRAddresses, local_config.info.coreResidencyMSRAddresses, sizeof(int) * MAX_MSR_ADDRESSES);
    memcpy(INTERNAL_STATE.pkgResidencyMSRAddresses, local_config.info.pkgResidencyMSRAddresses, sizeof(int) * MAX_MSR_ADDRESSES);

    if (true) {
        OUTPUT(0, KERN_INFO "CORE addrs...\n");
        for(i=0; i<MAX_MSR_ADDRESSES; ++i) {
            OUTPUT(0, KERN_INFO "C%d: %d\n", i, INTERNAL_STATE.coreResidencyMSRAddresses[i]);
        }
        OUTPUT(0, KERN_INFO "PKG addrs...\n");
        for(i=0; i<MAX_MSR_ADDRESSES; ++i) {
            OUTPUT(0, KERN_INFO "C%d: %d\n", i, INTERNAL_STATE.pkgResidencyMSRAddresses[i]);
        }
    }
    /*
     * Set C-state clock multiplier.
     */
    INTERNAL_STATE.residency_count_multiplier = local_config.info.residency_count_multiplier;

    /*
     * Make sure we've got a valid multiplier!
     */
    if((int)INTERNAL_STATE.residency_count_multiplier <= 0)
	INTERNAL_STATE.residency_count_multiplier = 1;

    /*
     * Set bus clock frequency -- required for
     * Turbo threshold determination / calculation.
     */
    INTERNAL_STATE.bus_clock_freq_khz = local_config.info.bus_clock_freq_khz;
    /*
     * Check if we've got a valid bus clock frequency -- default to
     * BUS_CLOCK_FREQ_KHZ if not.
     */
    if((int)INTERNAL_STATE.bus_clock_freq_khz <= 0)
	// INTERNAL_STATE.bus_clock_freq_khz = BUS_CLOCK_FREQ_KHZ;
	INTERNAL_STATE.bus_clock_freq_khz = DEFAULT_BUS_CLOCK_FREQ_KHZ();

    OUTPUT(0, KERN_INFO "DEBUG: Bus clock frequency = %u KHz\n", INTERNAL_STATE.bus_clock_freq_khz);

    /*
     * The base operating frequency requires the
     * bus frequency -- set it here.
     */
    get_base_operating_frequency();

    /*
     * Set power switches.
     */
    INTERNAL_STATE.collection_switches = local_config.data;
    pw_pr_debug("\tCONFIG collection switches = %llu\n", INTERNAL_STATE.collection_switches);

    INTERNAL_STATE.d_state_sample_interval = local_config.d_state_sample_interval;
    OUTPUT(0, KERN_INFO "\tCONFIG D-state collection interval (msec) = %d\n", INTERNAL_STATE.d_state_sample_interval);

    return SUCCESS;
};

/*
 * Free up space allocated for MSR information.
 */
static void pw_deallocate_msr_info_i(struct pw_msr_addr **addrs)
{
    if (likely(*addrs)) {
        pw_kfree(*addrs);
        *addrs = NULL;
    }
};

/*
 * Set MSR addrs
 */
int pw_set_msr_addrs(struct pw_msr_info __user *remote_info, int size)
{
    int i=0, retVal = SUCCESS;
    struct pw_msr_info *local_info = NULL;
    struct pw_msr_addr *msr_addrs = NULL;
    int num_msrs = -1;
    char *__buffer = NULL;

    if (!remote_info) {
        pw_pr_error("ERROR: NULL remote_info value?!\n");
        return -ERROR;
    }
    /*
     * 'Size' includes space for the 'header' AND space for all of the individual 'msr_info' values.
     */
    __buffer = pw_kmalloc(sizeof(char) * size, GFP_KERNEL);
    if (!__buffer) {
        pw_pr_error("ERROR allocating space for msr_addrs!\n");
        return -ERROR;
    }
    memset(__buffer, 0, (sizeof(char) * size));

    local_info = (pw_msr_info_t *)__buffer;

    i = copy_from_user(local_info, remote_info, size);
    if (i) { // "copy_from_user" returns number of bytes that COULD NOT be copied
        pw_pr_error("ERROR copying msr_info data from userspace!\n");
        pw_kfree(__buffer);
	return i;
    }
    num_msrs = local_info->num_msr_addrs;
    msr_addrs = (struct pw_msr_addr *)local_info->data;
    pw_pr_debug("pw_set_msr_addrs: size = %d, # msrs = %d\n", size, num_msrs);
    INTERNAL_STATE.num_msrs = num_msrs;
    INTERNAL_STATE.msr_addrs = pw_kmalloc(sizeof(pw_msr_addr_t) * num_msrs, GFP_KERNEL);
    if (unlikely(!INTERNAL_STATE.msr_addrs)) {
        pw_pr_error("ERROR allocating msr_addr array");
        retVal = -ERROR;
        goto done;
    }
    memcpy(INTERNAL_STATE.msr_addrs, msr_addrs, sizeof(pw_msr_addr_t) * num_msrs);
    for (i=0; i<num_msrs; ++i) {
        pw_pr_debug("MSR[%d] = 0x%x\n", i, INTERNAL_STATE.msr_addrs[i].addr);
    }
    /*
     * We also need to allocate space for the MSR sets populated by the "pw_read_msr_info_set_i()" function.
     */
    {
        int cpu = 0;
        for_each_possible_cpu(cpu) {
            pw_msr_info_set_t *info_set = pw_pcpu_msr_info_sets + cpu;
            {
                info_set->num_msrs = num_msrs;
                info_set->prev_msr_vals = pw_kmalloc(sizeof(pw_msr_val_t) * num_msrs, GFP_KERNEL);
                if (unlikely(!info_set->prev_msr_vals)) {
                    pw_pr_error("ERROR allocating space for info_set->prev_msr_vals!\n");
                    pw_kfree(INTERNAL_STATE.msr_addrs);
                    retVal = -ERROR;
                    goto done;
                }
                info_set->curr_msr_count = pw_kmalloc(sizeof(pw_msr_val_t) * num_msrs, GFP_KERNEL);
                if (unlikely(!info_set->curr_msr_count)) {
                    pw_pr_error("ERROR allocating space for info_set->curr_msr_count!\n");
                    pw_kfree(INTERNAL_STATE.msr_addrs);
                    pw_kfree(info_set->prev_msr_vals);
                    info_set->prev_msr_vals = NULL;
                    retVal = -ERROR;
                    goto done;
                }
                info_set->c_multi_msg_mem = pw_kmalloc(sizeof(pw_msr_val_t) * num_msrs + C_MULTI_MSG_HEADER_SIZE(), GFP_KERNEL);
                if (unlikely(!info_set->c_multi_msg_mem)) {
                    pw_pr_error("ERROR allocating space for c_multi_msg scratch space!\n");
                    pw_kfree(INTERNAL_STATE.msr_addrs);
                    pw_kfree(info_set->prev_msr_vals);
                    pw_kfree(info_set->curr_msr_count);
                    info_set->prev_msr_vals = NULL;
                    info_set->curr_msr_count = NULL;
                    retVal = -ERROR;
                    goto done;
                }
                memset(info_set->prev_msr_vals, 0, sizeof(pw_msr_val_t) * num_msrs);
                memset(info_set->curr_msr_count, 0, sizeof(pw_msr_val_t) * num_msrs);
                memset(info_set->c_multi_msg_mem, 0, sizeof(pw_msr_val_t) * num_msrs + C_MULTI_MSG_HEADER_SIZE());
                for (i=0; i<num_msrs; ++i) {
                    info_set->prev_msr_vals[i].id = msr_addrs[i].id;
                }
                pw_pr_debug(KERN_INFO "[%d]: info_set = %p, prev_msr_vals = %p, curr_msr_count = %p\n", cpu, info_set, info_set->prev_msr_vals, info_set->curr_msr_count);
            }
        }
    }
done:
    pw_kfree(__buffer);
    return retVal;
};
/*
 * Free up space required for S0iX addresses.
 */
static void pw_deallocate_platform_res_info_i(void)
{
    if (likely(INTERNAL_STATE.platform_res_addrs)) {
        pw_kfree(INTERNAL_STATE.platform_res_addrs);
        INTERNAL_STATE.platform_res_addrs = NULL;
    }
    /*
     * TODO
     * un-initialize as well?
     */
    if (INTERNAL_STATE.platform_remapped_addrs) {
        int i=0;
        for (i=0; i<INTERNAL_STATE.num_addrs; ++i) {
            if (INTERNAL_STATE.platform_remapped_addrs[i]) {
                iounmap((volatile void *)(unsigned long)INTERNAL_STATE.platform_remapped_addrs[i]);
                // printk(KERN_INFO "OK: unmapped MMIO base addr: 0x%lx\n", INTERNAL_STATE.platform_remapped_addrs[i]);
            }
        }
        pw_kfree(INTERNAL_STATE.platform_remapped_addrs);
        INTERNAL_STATE.platform_remapped_addrs = NULL;
    }
    if (likely(INTERNAL_STATE.platform_residency_msg)) {
        pw_kfree(INTERNAL_STATE.platform_residency_msg);
        INTERNAL_STATE.platform_residency_msg = NULL;
    }
    if (likely(INTERNAL_STATE.init_platform_res_values)) {
        pw_kfree(INTERNAL_STATE.init_platform_res_values);
        INTERNAL_STATE.init_platform_res_values = NULL;
    }
    return;
};
/*
 * Set S0iX method, addresses.
 */
#if 0
int pw_set_platform_res_config_i(struct PWCollector_platform_res_info *remote_info, int size)
{
    struct PWCollector_platform_res_info *local_info;
    char *__buffer = NULL;
    int i=0;
    u64 *__addrs = NULL;

    if (!remote_info) {
        pw_pr_error("ERROR: NULL remote_info value?!\n");
        return -ERROR;
    }
    printk(KERN_INFO "REMOTE_INFO = %p, size = %d\n", remote_info, size);
    /*
     * 'Size' includes space for the 'header' AND space for all of the 64b IO addresses.
     */
    __buffer = pw_kmalloc(sizeof(char) * size, GFP_KERNEL);
    if (!__buffer) {
        pw_pr_error("ERROR allocating space for local platform_res_info!\n");
        return -ERROR;
    }
    memset(__buffer, 0, (sizeof(char) * size));

    local_info = (PWCollector_platform_res_info_t *)__buffer;
    __addrs = (u64 *)local_info->addrs;

    i = copy_from_user(local_info, remote_info, size);
    if (i) { // "copy_from_user" returns number of bytes that COULD NOT be copied
        pw_pr_error("ERROR copying platform residency info data from userspace!\n");
        pw_kfree(__buffer);
	return i;
    }
    printk(KERN_INFO "OK: platform info collection type = %d, # addrs = %u\n", local_info->collection_type, local_info->num_addrs);
    for (i=0; i<local_info->num_addrs; ++i) {
        printk(KERN_INFO "\t[%d] --> 0x%lx\n", i, __addrs[i]);
    }
    pw_kfree(__buffer);
    return -ERROR;
};
#endif // if 0
int pw_set_platform_res_config_i(struct PWCollector_platform_res_info __user *remote_info, int size)
{
    struct PWCollector_platform_res_info local_info;
    int i=0;
    u64 __user *__remote_addrs = NULL;
    char *buffer = NULL;
    // const int counter_size_in_bytes = (int)INTERNAL_STATE.counter_size_in_bytes;

    INTERNAL_STATE.init_platform_res_values = INTERNAL_STATE.platform_remapped_addrs = INTERNAL_STATE.platform_res_addrs = NULL;
    INTERNAL_STATE.platform_residency_msg = NULL;

    if (!remote_info) {
        pw_pr_error("ERROR: NULL remote_info value?!\n");
        return -ERROR;
    }
    __remote_addrs = (u64 *)remote_info->addrs;
    pw_pr_debug("Remote address = %llx\n", __remote_addrs[0]);


    i = copy_from_user(&local_info, remote_info, sizeof(local_info));
    if (i) { // "copy_from_user" returns number of bytes that COULD NOT be copied
        pw_pr_error("ERROR copying platform residency info data from userspace!\n");
	return i;
    }
    // printk(KERN_INFO "OK: platform info collection type = %d, # addrs = %u\n", local_info.collection_type, local_info.num_addrs);

    INTERNAL_STATE.ipc_start_command = local_info.ipc_start_command; INTERNAL_STATE.ipc_start_sub_command = local_info.ipc_start_sub_command;
    INTERNAL_STATE.ipc_stop_command = local_info.ipc_stop_command; INTERNAL_STATE.ipc_stop_sub_command = local_info.ipc_stop_sub_command;
    INTERNAL_STATE.ipc_dump_command = local_info.ipc_dump_command; INTERNAL_STATE.ipc_dump_sub_command = local_info.ipc_dump_sub_command;

    INTERNAL_STATE.num_addrs = local_info.num_addrs;
    INTERNAL_STATE.collection_type = local_info.collection_type;
    INTERNAL_STATE.counter_size_in_bytes = local_info.counter_size_in_bytes;

    if (INTERNAL_STATE.num_addrs > 5) {
        printk(KERN_INFO "ERROR: can only collect a max of 5 platform residency states, for now (%u requested)!\n", INTERNAL_STATE.num_addrs);
        return -ERROR;
    }
    INTERNAL_STATE.platform_res_addrs = (u64 *)pw_kmalloc(sizeof(u64) * INTERNAL_STATE.num_addrs, GFP_KERNEL);
    if (!INTERNAL_STATE.platform_res_addrs) {
        printk(KERN_INFO "ERROR allocating space for local addrs!\n");
        return -ERROR;
    }
    memset(INTERNAL_STATE.platform_res_addrs, 0, (sizeof(u64) * INTERNAL_STATE.num_addrs));

    INTERNAL_STATE.platform_remapped_addrs = (u64 *)pw_kmalloc(sizeof(u64) * INTERNAL_STATE.num_addrs, GFP_KERNEL);
    if (!INTERNAL_STATE.platform_remapped_addrs) {
        printk(KERN_INFO "ERROR allocating space for local addrs!\n");
        pw_kfree(INTERNAL_STATE.platform_res_addrs);
        INTERNAL_STATE.platform_res_addrs = NULL;
        return -ERROR;
    }
    memset(INTERNAL_STATE.platform_remapped_addrs, 0, (sizeof(u64) * INTERNAL_STATE.num_addrs));

    INTERNAL_STATE.init_platform_res_values = (u64 *)pw_kmalloc(sizeof(u64) * INTERNAL_STATE.num_addrs, GFP_KERNEL);
    if (!INTERNAL_STATE.init_platform_res_values) {
        printk(KERN_INFO "ERROR allocating space for local addrs!\n");
        pw_kfree(INTERNAL_STATE.platform_res_addrs);
        pw_kfree(INTERNAL_STATE.platform_remapped_addrs);
        INTERNAL_STATE.platform_res_addrs = NULL;
        INTERNAL_STATE.platform_remapped_addrs = NULL;
        return -ERROR;
    }
    memset(INTERNAL_STATE.init_platform_res_values, 0, (sizeof(u64) * INTERNAL_STATE.num_addrs));

    buffer = (char *)pw_kmalloc(sizeof(s_res_msg_t) + (sizeof(u64) * (INTERNAL_STATE.num_addrs+2)), GFP_KERNEL); // "+2" ==> S0i0, S3
    if (!buffer) {
        printk(KERN_INFO "ERROR allocating space for local addrs!\n");
        pw_kfree(INTERNAL_STATE.platform_res_addrs);
        pw_kfree(INTERNAL_STATE.platform_remapped_addrs);
        pw_kfree(INTERNAL_STATE.init_platform_res_values);
        INTERNAL_STATE.platform_res_addrs = NULL;
        INTERNAL_STATE.platform_remapped_addrs = NULL;
        INTERNAL_STATE.init_platform_res_values = NULL;
        return -ERROR;
    }
    memset(buffer, 0, sizeof(char) * sizeof(s_res_msg_t) + (sizeof(u64) * (INTERNAL_STATE.num_addrs+2)));
    // INTERNAL_STATE.platform_residency_msg = (s_res_msg_t *)pw_kmalloc(sizeof(s_res_msg_t) + (sizeof(u64) * (INTERNAL_STATE.num_addrs+2)), GFP_KERNEL); // "+2" ==> S0i0, S3
    INTERNAL_STATE.platform_residency_msg = (s_res_msg_t *)&buffer[0];
    INTERNAL_STATE.platform_residency_msg->residencies = (u64 *)&buffer[sizeof(s_res_msg_t)];

    i = copy_from_user(INTERNAL_STATE.platform_res_addrs, __remote_addrs, (sizeof(u64) * INTERNAL_STATE.num_addrs));
    if (i) { // "copy_from_user" returns number of bytes that COULD NOT be copied
        pw_pr_error("ERROR copying platform residency info data from userspace!\n");
        pw_deallocate_platform_res_info_i();
	return i;
    }
#if 0
    printk(KERN_INFO "%llx\n", INTERNAL_STATE.platform_res_addrs[0]);
    for (i=0; i<INTERNAL_STATE.num_addrs; ++i) {
        printk(KERN_INFO "\t[%d] --> 0x%lx\n", i, INTERNAL_STATE.platform_res_addrs[i]);
    }
#endif // if 0
    switch (INTERNAL_STATE.collection_type) {
        case PW_IO_IPC:
        case PW_IO_MMIO: // fall-through
#if 1
            for (i=0; i<INTERNAL_STATE.num_addrs; ++i) {
                // INTERNAL_STATE.platform_remapped_addrs[i] = ioremap_nocache(INTERNAL_STATE.platform_res_addrs[i], sizeof(u32) * 1);
                // INTERNAL_STATE.platform_remapped_addrs[i] = (u64)ioremap_nocache(INTERNAL_STATE.platform_res_addrs[i], sizeof(u64) * 1);
                // INTERNAL_STATE.platform_remapped_addrs[i] = (u64)ioremap_nocache(INTERNAL_STATE.platform_res_addrs[i], sizeof(u32) * 1);
                INTERNAL_STATE.platform_remapped_addrs[i] = (u64)(unsigned long)ioremap_nocache((unsigned long)INTERNAL_STATE.platform_res_addrs[i], (unsigned long)(INTERNAL_STATE.counter_size_in_bytes * 1));
                if ((void *)(unsigned long)INTERNAL_STATE.platform_remapped_addrs[i] == NULL) {
                    printk(KERN_INFO "ERROR remapping MMIO addresses %p\n", (void *)(unsigned long)INTERNAL_STATE.platform_res_addrs[i]);
                    pw_deallocate_platform_res_info_i();
                    return -ERROR;
                }
                pw_pr_debug("OK: mapped address %llu to %llu!\n", INTERNAL_STATE.platform_res_addrs[i], INTERNAL_STATE.platform_remapped_addrs[i]);
            }
#else // if 1
            INTERNAL_STATE.platform_remapped_addrs[0] = ioremap_nocache(INTERNAL_STATE.platform_res_addrs[0], sizeof(unsigned long) * (INTERNAL_STATE.num_addrs * 2));
#endif // if 0
            break;
        default:
            printk(KERN_INFO "ERROR: unsupported platform residency collection type: %u!\n", INTERNAL_STATE.collection_type);
            pw_deallocate_platform_res_info_i();
            return -ERROR;
    }
    return SUCCESS;
};

int check_platform(struct PWCollector_check_platform *remote_check, int size)
{
    struct PWCollector_check_platform *local_check;
    const char *unsupported = "UNSUPPORTED_T1, UNSUPPORTED_T2"; // for debugging ONLY
    int len = strlen(unsupported);
    int max_size = sizeof(struct PWCollector_check_platform);
    int retVal = SUCCESS;

    if (!remote_check) {
        pw_pr_error("ERROR: NULL remote_check value?!\n");
        return -ERROR;
    }

    local_check = pw_kmalloc(max_size, GFP_KERNEL);

    if(!local_check){
	pw_pr_error("ERROR: could NOT allocate memory in check_platform!\n");
	return -ERROR;
    }

    memset(local_check, 0, max_size);

    /*
     * Populate "local_check.unsupported_tracepoints" with a (comma-separated)
     * list of unsupported tracepoints. For now, we just leave this
     * blank, reflecting the fact that, on our development systems,
     * every tracepoints is supported.
     *
     * Update: for debugging, write random data here.
     */
    memcpy(local_check->unsupported_tracepoints, unsupported, len);
    /*
     * UPDATE: we're borrowing one of the 'reserved' 64bit values
     * to document the following:
     * (1) Kernel call stacks supported?
     * (2) Kernel compiled with CONFIG_TIMER_STATS?
     * (3) Wakelocks supported?
     */
#ifdef CONFIG_FRAME_POINTER
    local_check->supported_kernel_features |= PW_KERNEL_SUPPORTS_CALL_STACKS;
#endif
#ifdef CONFIG_TIMER_STATS
    local_check->supported_kernel_features |= PW_KERNEL_SUPPORTS_CONFIG_TIMER_STATS;
#endif
#if DO_WAKELOCK_SAMPLE
    local_check->supported_kernel_features |= PW_KERNEL_SUPPORTS_WAKELOCK_PATCH;
#endif
    /*
     * Also update information on the underlying CPU:
     * (1) Was the 'ANY-THREAD' bit set?
     * (2) Was 'Auto-Demote' enabled?
     */
    if (pw_is_any_thread_set) {
        local_check->supported_arch_features |= PW_ARCH_ANY_THREAD_SET;
    }
    if (pw_is_auto_demote_enabled) {
        local_check->supported_arch_features |= PW_ARCH_AUTO_DEMOTE_ENABLED;
    }

    /*
     * Copy everything back to user address space.
     */
    if( (retVal = copy_to_user(remote_check, local_check, size))) // returns number of bytes that COULD NOT be copied
	retVal = -ERROR;

    pw_kfree(local_check);
    return retVal; // all unsupported tracepoints documented
};

/*
 * Return the TURBO frequency threshold
 * for this CPU.
 */
int get_turbo_threshold(struct PWCollector_turbo_threshold *remote_thresh, int size)
{
    struct PWCollector_turbo_threshold local_thresh;

    if (!remote_thresh) {
        pw_pr_error("ERROR: NULL remote_thresh value?!\n");
        return -ERROR;
    }

    if (!base_operating_freq_khz) {
        pw_pr_error("ERROR: retrieving turbo threshold without specifying base operating freq?!\n");
	return -ERROR;
    }

    local_thresh.threshold_frequency = base_operating_freq_khz;

    if(copy_to_user(remote_thresh, &local_thresh, size)) // returns number of bytes that could NOT be copied.
	return -ERROR;

    return SUCCESS;
};

/*
 * Retrieve device driver version
 */
int get_version(struct PWCollector_version_info *remote_version, int size)
{
    struct PWCollector_version_info local_version;

    if (!remote_version) {
        pw_pr_error("ERROR: NULL remote_version value?!\n");
        return -ERROR;
    }

    local_version.version = PW_DRV_VERSION_MAJOR;
    local_version.inter = PW_DRV_VERSION_MINOR;
    local_version.other = PW_DRV_VERSION_OTHER;

    /*
     * Copy everything back to user address space.
     */
    return copy_to_user(remote_version, &local_version, size); // returns number of bytes that could NOT be copiled
};

/*
 * Retrieve microcode patch version.
 * Only useful for MFLD
 */
int get_micro_patch_ver(int *remote_ver, int size)
{
    int local_ver = micro_patch_ver;

    if (!remote_ver) {
        pw_pr_error("ERROR: NULL remote_ver value?!\n");
        return -ERROR;
    }

    /*
     * Copy everything back to user address space.
     */
    return copy_to_user(remote_ver, &local_ver, size); // returns number of bytes that could NOT be copiled
};

int get_status(struct PWCollector_status *remote_status, int size)
{
    struct PWCollector_status local_status;
    int cpu, retVal = SUCCESS;
    stats_t *pstats = NULL;
    unsigned long statusJIFF, elapsedJIFF = 0;

    if (!remote_status) {
        pw_pr_error("ERROR: NULL remote_status value?!\n");
        return -ERROR;
    }

    memset(&local_status, 0, sizeof(local_status));

    /*
     * Set # cpus.
     */
    // local_status.num_cpus = pw_max_num_cpus;
    local_status.num_cpus = num_online_cpus();

    /*
     * Set total collection time elapsed.
     */
    {
	statusJIFF = jiffies;
	if(statusJIFF < INTERNAL_STATE.collectionStartJIFF){
	    OUTPUT(0, KERN_INFO "WARNING: jiffies counter has WRAPPED AROUND!\n");
	    elapsedJIFF = 0; // avoid messy NAN when dividing
	}else{
	    // elapsedJIFF = statusJIFF - startJIFF;
	    elapsedJIFF = statusJIFF - INTERNAL_STATE.collectionStartJIFF;
	}
	OUTPUT(0, KERN_INFO "start = %lu, stop = %lu, elapsed = %lu\n", INTERNAL_STATE.collectionStartJIFF, statusJIFF, elapsedJIFF);
    }
    local_status.time = jiffies_to_msecs(elapsedJIFF);

    /*
     * Set # c-breaks etc.
     * Note: aggregated over ALL cpus,
     * per spec document.
     */
    for_each_online_cpu(cpu){
	pstats = &per_cpu(per_cpu_stats, cpu);
	local_status.c_breaks += local_read(&pstats->c_breaks);
	local_status.timer_c_breaks += local_read(&pstats->timer_c_breaks);
	local_status.inters_c_breaks += local_read(&pstats->inters_c_breaks);
	local_status.p_trans += local_read(&pstats->p_trans);
	local_status.num_inters += local_read(&pstats->num_inters);
	local_status.num_timers += local_read(&pstats->num_timers);
    }

    /*
     * Now copy everything to user-space.
     */
    retVal = copy_to_user(remote_status, &local_status, sizeof(local_status)); // returns number of bytes that COULD NOT be copied

    return retVal;
};

/*
 * Reset all statistics collected so far.
 * Called from a non-running collection context.
 */
static inline void reset_statistics(void)
{
    int cpu;
    stats_t *pstats = NULL;

    /*
     * Note: no need to lock, since we're only
     * going to be called from a non-running
     * collection, and tracepoints are inserted
     * (just) before a collection starts, and removed
     * (just) after a collection ends.
     */
    for_each_online_cpu(cpu){
	/*
	 * Reset the per cpu stats
	 */
	{
	    pstats = &per_cpu(per_cpu_stats, cpu);
	    local_set(&pstats->c_breaks, 0);
	    local_set(&pstats->timer_c_breaks, 0);
	    local_set(&pstats->inters_c_breaks, 0);
	    local_set(&pstats->p_trans, 0);
	    local_set(&pstats->num_inters, 0);
	    local_set(&pstats->num_timers, 0);
	}
    }
};

/*
 * Reset the (PER-CPU) structs containing
 * MSR residency information (amongst
 * other fields).
 */
void reset_per_cpu_msr_residencies(void)
{
    int cpu;

    for_each_online_cpu(cpu) {
        /*
         * Reset the per-cpu residencies
         */
        *(&per_cpu(prev_c6_val, cpu)) = 0;
        /*
         * Reset the "first-hit" variable.
         */
        // local_set(&per_cpu(is_first_event, cpu), 1);
        per_cpu(wakeup_event_counter, cpu).event_tsc = 0;
        /*
         * Reset the 'init_msr_sent' variable.
         */
        memset((&per_cpu(pw_pcpu_msr_sets, cpu)), 0, sizeof(msr_set_t));
        /*
         * Reset the MSR info sets.
         */
        {
            pw_msr_info_set_t *info_set = pw_pcpu_msr_info_sets + cpu;
            if (likely(info_set->prev_msr_vals)) {
                int i = 0;
                for (i=0; i<info_set->num_msrs; ++i) {
                    info_set->prev_msr_vals[i].val = 0x0;
                }
            }
            if (likely(info_set->curr_msr_count)) {
                memset(info_set->curr_msr_count, 0, sizeof(pw_msr_val_t) * info_set->num_msrs);
            }
            info_set->init_msr_set_sent = 0;
        }
        /*
         * Reset stats on # samples produced and # dropped.
         */
        local_set(&per_cpu(num_samples_produced, cpu), 0);
        local_set(&per_cpu(num_samples_dropped, cpu), 0);
    }
    /*
     * Reset the TPS atomic count value.
     */
#if DO_TPS_EPOCH_COUNTER
    atomic_set(&tps_epoch, 0);
#endif
    /*
     * Ensure updates are propagated.
     */
    smp_mb();
};


static void reset_trace_sent_fields(void)
{
    struct tnode *node = NULL;
    struct hlist_node *curr = NULL;
    int i=0;

    for (i=0; i<NUM_MAP_BUCKETS; ++i) {
        PW_HLIST_FOR_EACH_ENTRY(node, curr, &timer_map[i].head, list) {
	    node->trace_sent = 0;
	}
    }
};


/*
 * Run the generation of current p-state sample
 * for all cpus in parallel to avoid the delay
 * due to a serial execution.
 */
#if DO_GENERATE_CURRENT_FREQ_IN_PARALLEL
static void generate_cpu_frequency_per_cpu(int cpu, bool is_start)
{
    u64 tsc = 0, aperf = 0, mperf = 0;
    u32 perf_status = 0;
    u8 is_boundary = (is_start) ? 1 : 2; // "0" ==> NOT boundary, "1" ==> START boundary, "2" ==> STOP boundary
    u32 prev_req_freq = 0;

#ifndef __arm__
    u32 l=0, h=0;
    {
        tscval(&tsc);
    }

    /*
     * Read the IA32_PERF_STATUS MSR. We delegate the actual frequency computation to Ring-3 because
     * the PERF_STATUS encoding is actually model-specific.
     */
    {
        WUWATCH_RDMSR(IA32_PERF_STATUS_MSR_ADDR, l, h);
        perf_status = l; // We're only interested in the lower 16 bits!
        h = 0;
    }


    /*
     * Retrieve the previous requested frequency.
     */
    if (is_start == false) {
        prev_req_freq = per_cpu(pcpu_prev_req_freq, cpu);
    } else {
        /*
         * Collection START: make sure we reset the requested frequency!
         */
        per_cpu(pcpu_prev_req_freq, cpu) = 0;
    }
    per_cpu(pcpu_prev_perf_status_val, cpu) = perf_status;
    /*
     * Also read CPU_CLK_UNHALTED.REF and CPU_CLK_UNHALTED.CORE. These required ONLY for AXE import
     * backward compatibility!
     */
    {
        WUWATCH_RDMSRL(CORE_CYCLES_MSR_ADDR, aperf);

        WUWATCH_RDMSRL(REF_CYCLES_MSR_ADDR, mperf);
    }

#else // __arm__
    msr_set_t *set = NULL;
    set = &get_cpu_var(pw_pcpu_msr_sets);

    tscval(&tsc);
    aperf = set->prev_msr_vals[APERF];
    mperf = set->prev_msr_vals[MPERF];
    put_cpu_var(pw_pcpu_msr_sets);

    perf_status = cpufreq_quick_get(cpu) / INTERNAL_STATE.bus_clock_freq_khz;

    /*
     * Retrieve the previous requested frequency.
     */
    // TODO CAN WE MERGE THIS CODE WITH ifndef __arm__ code above and put it
    // after this code?
    if (is_start == false) {
        prev_req_freq = per_cpu(pcpu_prev_req_freq, cpu);
    }
#endif // ifndef __arm__

    produce_p_sample(cpu, tsc, prev_req_freq, perf_status, is_boundary, aperf, mperf);

};
#else // DO_GENERATE_CURRENT_FREQ_IN_PARALLEL
static void generate_cpu_frequency_per_cpu(int cpu, bool is_start)
{
    u64 tsc = 0, aperf = 0, mperf = 0;
    u32 perf_status = 0;
    u8 is_boundary = (is_start) ? 1 : 2; // "0" ==> NOT boundary, "1" ==> START boundary, "2" ==> STOP boundary
    u32 prev_req_freq = 0;

#ifndef __arm__
    u32 l=0, h=0;
    {
        int ret = WUWATCH_RDMSR_SAFE_ON_CPU(cpu, 0x10, &l, &h);
        if(ret){
            OUTPUT(0, KERN_INFO "WARNING: WUWATCH_RDMSR_SAFE_ON_CPU of TSC failed with code %d\n", ret);
        }
        tsc = h;
        tsc <<= 32;
        tsc += l;
    }

    /*
     * Read the IA32_PERF_STATUS MSR. We delegate the actual frequency computation to Ring-3 because
     * the PERF_STATUS encoding is actually model-specific.
     */
    {
        WARN_ON(WUWATCH_RDMSR_SAFE_ON_CPU(cpu, IA32_PERF_STATUS_MSR_ADDR, &l, &h));
        perf_status = l; // We're only interested in the lower 16 bits!
    }


    /*
     * Retrieve the previous requested frequency.
     */
    if (is_start == false) {
        prev_req_freq = per_cpu(pcpu_prev_req_freq, cpu);
    } else {
        /*
         * Collection START: make sure we reset the requested frequency!
         */
        per_cpu(pcpu_prev_req_freq, cpu) = 0;
    }
    per_cpu(pcpu_prev_perf_status_val, cpu) = perf_status;
    /*
     * Also read CPU_CLK_UNHALTED.REF and CPU_CLK_UNHALTED.CORE. These required ONLY for AXE import
     * backward compatibility!
     */
#if 1
    {
        WARN_ON(WUWATCH_RDMSR_SAFE_ON_CPU(cpu, CORE_CYCLES_MSR_ADDR, &l, &h));
        aperf = (u64)h << 32 | (u64)l;

        WARN_ON(WUWATCH_RDMSR_SAFE_ON_CPU(cpu, REF_CYCLES_MSR_ADDR, &l, &h));
        mperf = (u64)h << 32 | (u64)l;
    }
#endif

#else
    msr_set_t *set = NULL;
    set = &get_cpu_var(pw_pcpu_msr_sets);

    tscval(&tsc);
    aperf = set->prev_msr_vals[APERF];
    mperf = set->prev_msr_vals[MPERF];
    put_cpu_var(pw_pcpu_msr_sets);

    perf_status = cpufreq_quick_get(cpu) / INTERNAL_STATE.bus_clock_freq_khz;

    /*
     * Retrieve the previous requested frequency.
     */
    // TODO CAN WE MERGE THIS CODE WITH ifndef __arm__ code above and put it
    // after this code?
    if (is_start == false) {
        prev_req_freq = per_cpu(pcpu_prev_req_freq, cpu);
    }
#endif // ifndef __arm__

    produce_p_sample(cpu, tsc, prev_req_freq, perf_status, is_boundary, aperf, mperf);

};
#endif // DO_GENERATE_CURRENT_FREQ_IN_PARALLEL

static void generate_cpu_frequency(void *start)
{
    int cpu = raw_smp_processor_id();
    bool is_start = *((bool *)start);

    generate_cpu_frequency_per_cpu(cpu, is_start);
}

/*
 * Measure current CPU operating
 * frequency, and push 'P-samples'
 * onto the (per-cpu) O/P buffers.
 * Also determine the various
 * discrete frequencies the processor
 * is allowed to execute at (basically
 * the various frequencies present
 * in the 'scaling_available_frequencies'
 * sysfs file).
 *
 * REQUIRES CPUFREQ DRIVER!!!
 */
static void get_current_cpu_frequency(bool is_start)
{

#if DO_GENERATE_CURRENT_FREQ_IN_PARALLEL
    SMP_CALL_FUNCTION(&generate_cpu_frequency, (void *)&is_start, 0, 1);
    // smp_call_function is executed for all other CPUs except itself.
    // So, run it for itself.
    generate_cpu_frequency((void *)&is_start);
#else
    int cpu = 0;
    for_each_online_cpu(cpu){
        generate_cpu_frequency_per_cpu(cpu, is_start);
    }
#endif
};

static void generate_end_tps_sample_per_cpu(void *tsc)
{
    if (IS_C_STATE_MODE()) {
        // tps(0 /* type, don't care */, MPERF /* state, don't care at collection end */, true /* boundary */);
        bdry_tps();
    } else {
        tps_lite(true /* boundary */);
    }
    return;
};

static void generate_end_tps_samples(void)
{
    u64 tsc = 0;

    tscval(&tsc);

    SMP_CALL_FUNCTION(&generate_end_tps_sample_per_cpu, (void *)&tsc, 0, 1);
    generate_end_tps_sample_per_cpu((void *)&tsc);
};

/*
 * START/RESUME a collection.
 *
 * (a) (For START ONLY): ZERO out all (per-cpu) O/P buffers.
 * (b) Reset all statistics.
 * (c) Register all tracepoints.
 */
int start_collection(PWCollector_cmd_t cmd)
{
    switch(cmd){
    case PW_START:
	/*
	 * Reset the O/P buffers.
	 *
	 * START ONLY
	 */
        pw_reset_per_cpu_buffers();
	/*
	 * Reset the 'trace_sent' fields
	 * for all trace entries -- this
	 * ensures we send backtraces
	 * once per collection, as
	 * opposed to once per 'insmod'.
	 *
	 * START ONLY
	 */
	{
	    reset_trace_sent_fields();
	}

#if DO_CACHE_IRQ_DEV_NAME_MAPPINGS
	/*
	 * Reset the list of vars required
	 * to transfer IRQ # <-> Name info.
	 * UPDATE: the 'irq_map' should contain
	 * mappings for only those
	 * devices that actually caused C-state
	 * wakeups DURING THE CURRENT COLLECTION.
	 * We therefore reset the map before
	 * every collection (this also auto resets
	 * the "irq_mappings_list" data structure).
	 *
	 * START ONLY
	 */
	{
	    destroy_irq_map();
	    if(init_irq_map()){
		// ERROR
		pw_pr_error("ERROR: could NOT initialize irq map in start_collection!\n");
		return -ERROR;
	    }
	}
#endif

#if DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES
        {
            destroy_wlock_map();
            if (init_wlock_map()) {
                pw_pr_error("ERROR: could NOT initialize wlock map in start_collection1\n");
                return -ERROR;
            }
        }
#endif
	/*
	 * Reset collection stats
	 *
	 * START ONLY
	 */
#if DO_IOCTL_STATS
	{
	    reset_statistics();
	}
#endif

    case PW_RESUME: // fall through
	break;
    default: // should *NEVER* happen!
	printk(KERN_ERR "Error: invalid cmd=%d in start collection!\n", cmd);
	return -ERROR;
    }
    /*
     * Reset the (per-cpu) "per_cpu_t" structs that hold MSR residencies
     *
     * START + RESUME
     */
    {
	reset_per_cpu_msr_residencies();
    }

    /*
     * Get START P-state samples.
     *
     * UPDATE: do this ONLY IF
     * USER SPECIFIES FREQ-mode!
     *
     * START + RESUME???
     */
    if(likely(IS_FREQ_MODE())){
	get_current_cpu_frequency(true); // "true" ==> collection START
    }
    /*
     * Get START C-state samples.
     */
    if (likely(IS_SLEEP_MODE() || IS_C_STATE_MODE())) {
        generate_end_tps_samples();
    }

    /*
     * Take a snapshot of the TSC on collection start -- required for ACPI S3 support.
     */
    tscval(&pw_collection_start_tsc);

    INTERNAL_STATE.collectionStartJIFF = jiffies;
    INTERNAL_STATE.write_to_buffers = true;

    /*
     * OK, all setup completed. Now
     * register the tracepoints.
     */
    switch(cmd){
    case PW_START:
	register_pausable_probes();
    case PW_RESUME: // fall through
	register_non_pausable_probes();
	break;
    default: // should *NEVER* happen!
	printk(KERN_ERR "Error: invalid cmd=%d in start collection!\n", cmd);
	return -ERROR;
    }

#if DO_S_RESIDENCY_SAMPLE
    //struct timeval cur_time;
    if (IS_S_RESIDENCY_MODE()) {
        startTSC_s_residency = 0;
        produce_boundary_s_residency_msg_i(true); // "true" ==> BEGIN boundary
        // startJIFF_s_residency = CURRENT_TIME_IN_USEC();
    }
#endif // DO_S_RESIDENCY_SAMPLE

#if DO_ACPI_S3_SAMPLE
    //struct timeval cur_time;
#if 0
    if(pw_is_slm && IS_ACPI_S3_MODE()){
        /*
         * Ensure we reset the ACPI S3 'start' TSC counter.
         */
        startTSC_acpi_s3 = 0x0;
        produce_acpi_s3_sample(false);
    }
#endif // if 0
#endif

    return SUCCESS;
};

/*
 * STOP/PAUSE/CANCEL a (running) collection.
 *
 * (a) Unregister all tracepoints.
 * (b) Reset all stats.
 * (c) Wake any process waiting for full buffers.
 */
int stop_collection(PWCollector_cmd_t cmd)
{
    /*
     * Reset the power collector task.
     */
    pw_power_collector_task = NULL;
    /*
     * Reset the collection start TSC.
     */
    pw_collection_start_tsc = 0;
    /*
     * Reset the collection time.
     */
    pw_collection_time_ticks = 0;
    /*
     * Was the ACPI S3 hrtimer active? If so, cancel it.
     */
    if (hrtimer_active(&pw_acpi_s3_hrtimer)) {
        printk(KERN_INFO "WARNING: active ACPI S3 timer -- trying to cancel!\n");
        hrtimer_try_to_cancel(&pw_acpi_s3_hrtimer);
    }

#if DO_S_RESIDENCY_SAMPLE
    if (IS_S_RESIDENCY_MODE()) {
        produce_boundary_s_residency_msg_i(false); // "false" ==> NOT begin boundary
        startTSC_s_residency = 0; // redundant!
    }
#endif // DO_S_RESIDENCY_SAMPLE

#if DO_ACPI_S3_SAMPLE
#if 0
    if(pw_is_slm && IS_ACPI_S3_MODE()){
        produce_acpi_s3_sample(false);
    }
#endif // if 0
#endif

    INTERNAL_STATE.collectionStopJIFF = jiffies;
    INTERNAL_STATE.write_to_buffers = false;
    {
	if(true && cmd == PW_PAUSE){
	    u64 tmp_tsc = 0;
	    tscval(&tmp_tsc);
	    OUTPUT(0, KERN_INFO "RECEIVED PAUSE at tsc = %llu\n", tmp_tsc);
	}
    }

    {
	unregister_non_pausable_probes();
    }

    if(cmd == PW_STOP || cmd == PW_CANCEL) {
        unregister_pausable_probes();
        /*
         * Get STOP P-state samples
         */
        if (likely(IS_FREQ_MODE())) {
            get_current_cpu_frequency(false); // "false" ==> collection STOP
        }
        /*
         * Get STOP C-state samples.
         */
        if (likely(IS_SLEEP_MODE() || IS_C_STATE_MODE())) {
            generate_end_tps_samples();
        }
        /*
         * Gather some stats on # of samples produced and dropped.
         */
        {
            pw_count_samples_produced_dropped();
        }
    }



    // Reset the (per-cpu) "per_cpu_t" structs that hold MSR residencies
    {
        reset_per_cpu_msr_residencies();
    }

    /*
     * Reset IOCTL stats
     *
     * STOP/CANCEL ONLY
     */
#if DO_IOCTL_STATS
    if(cmd == PW_STOP || cmd == PW_CANCEL) {
        reset_statistics();
    }
#endif

    /*
     * Tell consumers to 'flush' all buffers. We need to
     * defer this as long as possible because it needs to be
     * close to the 'wake_up_interruptible', below.
     */
    {
	INTERNAL_STATE.drain_buffers = true;
        smp_mb();
    }

    /*
     * There might be a reader thread blocked on a read: wake
     * it up to give it a chance to respond to changed
     * conditions.
     */
    {
        wake_up_interruptible(&pw_reader_queue);
    }

    /*
     * Delete all non-kernel timers.
     * Also delete the wakelock map.
     *
     * STOP/CANCEL ONLY
     */
    if (cmd == PW_STOP || cmd == PW_CANCEL) {
        delete_all_non_kernel_timers();
#if DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES
        destroy_wlock_map();
#endif // DO_USE_CONSTANT_POOL_FOR_WAKELOCK_NAMES
        pw_pr_debug("Debug: deallocating on a stop/cancel!\n");
        pw_deallocate_msr_info_i(&INTERNAL_STATE.msr_addrs);
        // pw_deallocate_platform_res_info_i(&INTERNAL_STATE.platform_res_addrs);
        pw_deallocate_platform_res_info_i();
        pw_reset_msr_info_sets();
    }

    OUTPUT(0, KERN_INFO "\tUNREGISTERED all probes!\n");
    return SUCCESS;
};

long handle_cmd(PWCollector_cmd_t cmd)
{
    PWCollector_cmd_t prev_cmd;
    /*
     * Sanity check cmd range.
     */
    if(cmd < PW_START || cmd > PW_MARK){
	pw_pr_error("Error: UNSUPPORTED cmd=%d\n", cmd);
	return -ERROR;
    }
    /*
     * Check to see if there are any invalid
     * command combinations (e.g. START -> START etc.)
     */
    if(!is_cmd_valid(cmd)){
	pw_pr_error("Error: INVALID requested cmd=%d, CURRENT cmd=%d\n", cmd, INTERNAL_STATE.cmd);
	return -ERROR;
    }
    /*
     * OK, we've gotten a valid command.
     * Store it.
     */
    prev_cmd = INTERNAL_STATE.cmd;
    INTERNAL_STATE.cmd = cmd;
    /*
     * Actions based on specific commands here...
     */
    switch(cmd){
    case PW_START:
    case PW_RESUME:
	INTERNAL_STATE.drain_buffers = false;
	// startJIFF = jiffies;
	{
	    if(start_collection(cmd))
		return -ERROR;
	}
	break;
    case PW_STOP:
	// INTERNAL_STATE.drain_buffers = true;
    case PW_PAUSE:
    case PW_CANCEL:
	// stopJIFF = jiffies;
	{
	    stop_collection(cmd);
	}
	break;
    default:
	pw_pr_error("Error: UNSUPPORTED cmd=%d\n", cmd);
	/*
	 * Reset "cmd" state to what it was before
	 * this ioctl.
	 */
	INTERNAL_STATE.cmd = prev_cmd;
	return -ERROR;
    }
    OUTPUT(3, KERN_INFO "Debug: Successfully switched mode from %d to %d: IS_COLLECTING = %d\n", prev_cmd, cmd, IS_COLLECTING());
    return SUCCESS;
};

long do_cmd(PWCollector_cmd_t cmd, u64 __user *remote_output_args, int size)
{
    int retVal = SUCCESS;

    if (!remote_output_args) {
        pw_pr_error("ERROR: NULL remote_output_args value?!\n");
        return -ERROR;
    }
    /*
     * Handle the command itself.
     */
    if (handle_cmd(cmd)) {
        return -ERROR;
    }
    /*
     * Then check if the user requested some collection stats.
     */
#if DO_COUNT_DROPPED_SAMPLES
    if (cmd == PW_STOP || cmd == PW_CANCEL) {
        // u64 local_args[2] = {total_num_samples_produced, total_num_samples_dropped};
        u64 local_args[2] = {pw_num_samples_produced, pw_num_samples_dropped};
        // u64 local_args[2] = {100, 10}; // for debugging!
        if (copy_to_user(remote_output_args, local_args, size)) // returns number of bytes that could NOT be copied
            retVal = -ERROR;
    }
#endif // DO_COUNT_DROPPED_SAMPLES

    return retVal;
};

/*
 * Callback from Power Manager SUSPEND/RESUME events. Useful if the device was suspended
 * (i.e. entered ACPI 'S3') during the collection.
 */
int pw_alrm_suspend_notifier_callback_i(struct notifier_block *block, unsigned long state, void *dummy)
{
    u64 tsc_suspend_time_ticks = 0;
    u64 suspend_time_ticks = 0;
    // u64 usec = 0;
    u64 suspend_time_usecs = 0;
    u64 base_operating_freq_mhz = base_operating_freq_khz / 1000;

    if (!pw_is_slm) {
        return NOTIFY_DONE;
    }
    switch (state) {
        case PM_SUSPEND_PREPARE:
            /*
             * Entering SUSPEND.
             */
            tscval(&pw_suspend_start_tsc);
            printk(KERN_INFO "pw: SUSPEND PREPARE: tsc = %llu\n", pw_suspend_start_tsc);
            if (likely(IS_COLLECTING())) {
                if (IS_S_RESIDENCY_MODE()) {
                    /*
                     * Generate an S_RESIDENCY sample.
                     */
                    int cpu = RAW_CPU();
                    PWCollector_msg_t msg;
                    s_res_msg_t *smsg = INTERNAL_STATE.platform_residency_msg;
                    u64 *values = smsg->residencies;

#if 0
                    switch (INTERNAL_STATE.collection_type) {
                        case PW_IO_IPC:
                        case PW_IO_MMIO:
                            /*
                             * ASSUMPTION:
                             * S0iX addresses are layed out as following:
                             * S0i1, S0i2, S0i3, <others>
                             * Where "others" is optional.
                             */
                            pw_suspend_start_s0i3 = *((u64 *)INTERNAL_STATE.platform_remapped_addrs[2]);
                            break;
                        default:
                            printk(KERN_INFO "ERROR: unsupported S0iX collection type: %u!\n", INTERNAL_STATE.collection_type);
                            return NOTIFY_DONE;
                    }
#endif // if 0

                    /*
                     * No residency counters available
                     */
                    msg.data_type = S_RESIDENCY;
                    msg.cpuidx = cpu;
                    msg.tsc = pw_suspend_start_tsc;
                    msg.data_len = sizeof(*smsg) + sizeof(u64) * (INTERNAL_STATE.num_addrs + 2); // "+2" for S0i3, S3

                    values[0] = pw_suspend_start_tsc - startTSC_s_residency;

                    pw_populate_s_residency_values_i(values, false); // "false" ==> NOT begin boundary

                    pw_suspend_start_s0i3 = values[3]; // values array has entries in order: S0i0, S0i1, S0i2, S0i3, ...

                    msg.p_data = (u64)((unsigned long)(smsg));

                    /*
                     * OK, everything computed. Now copy
                     * this sample into an output buffer
                     */
                    pw_produce_generic_msg(&msg, true); // "true" ==> allow wakeups
                }
                /*
                 * Also need to send an ACPI S3 sample.
                 */
                if (IS_ACPI_S3_MODE()) {
                    // produce_acpi_s3_sample(false);
                    // produce_acpi_s3_sample(0 /* s3 res */);
                    produce_acpi_s3_sample(pw_suspend_start_tsc, 0 /* s3 res */);
                }
                /*
                 * And finally, the special 'broadcast' wakelock sample.
                 */
                if (IS_WAKELOCK_MODE()) {
                    PWCollector_msg_t sample;
                    w_sample_t w_msg;

                    memset(&w_msg, 0, sizeof(w_msg));

                    sample.cpuidx = RAW_CPU();
                    sample.tsc = pw_suspend_start_tsc;

                    w_msg.type = PW_WAKE_UNLOCK_ALL;

                    sample.data_type = W_STATE;
                    sample.data_len = sizeof(w_msg);
                    sample.p_data = (u64)(unsigned long)&w_msg;
                    /*
                     * OK, everything computed. Now copy
                     * this sample into an output buffer
                     */
                    pw_produce_generic_msg(&sample, true); // "true" ==> wakeup sleeping readers, if required
                }

                printk(KERN_INFO "SUSPEND PREPARE s0i3 = %llu\n", pw_suspend_start_s0i3);
            }
            break;
        case PM_POST_SUSPEND:
            /*
             * Exitted SUSPEND -- check to see if we've been in suspend
             * for longer than the collection time specified by the user.
             * If so, send the use a SIGINT -- that will force it to
             * stop collecting.
             */
            tscval(&pw_suspend_stop_tsc);
            printk(KERN_INFO "pw: POST SUSPEND: tsc = %llu\n", pw_suspend_stop_tsc);
            BUG_ON(pw_suspend_start_tsc == 0);

            if (likely(IS_COLLECTING())) {
                if (IS_S_RESIDENCY_MODE()) {
#if 0
                    switch (INTERNAL_STATE.collection_type) {
                        case PW_IO_IPC:
                        case PW_IO_MMIO:
                            /*
                             * ASSUMPTION:
                             * S0iX addresses are layed out as following:
                             * S0i1, S0i2, S0i3, <others>
                             * Where "others" is optional.
                             */
                            pw_suspend_stop_s0i3 = *((u64 *)INTERNAL_STATE.platform_remapped_addrs[2]);
                            break;
                        default:
                            printk(KERN_INFO "ERROR: unsupported S0iX collection type: %u!\n", INTERNAL_STATE.collection_type);
                            return NOTIFY_DONE;
                    }
#endif // if 0
                    /*
                     * We need to an 'S_RESIDENCY' sample detailing the actual supend
                     * statistics (when did the device get suspended; for how long
                     * was it suspended etc.).
                     */
                    {
                        PWCollector_msg_t msg;
                        s_res_msg_t *smsg = INTERNAL_STATE.platform_residency_msg;
                        u64 *values = smsg->residencies;

                        msg.data_type = S_RESIDENCY;
                        msg.cpuidx = RAW_CPU();
                        msg.tsc = pw_suspend_stop_tsc;
                        msg.data_len = sizeof(*smsg) + sizeof(u64) * (INTERNAL_STATE.num_addrs + 2); // "+2" for S0i3, S3

                        values[0] = pw_suspend_stop_tsc - startTSC_s_residency;

                        pw_populate_s_residency_values_i(values, false); // "false" ==> NOT begin boundary

                        pw_suspend_stop_s0i3 = values[3]; // values array has entries in order: S0i0, S0i1, S0i2, S0i3, ...

                        /*
                         * UPDATE: TNG, VLV have S0iX counter incrementing at TSC frequency!!!
                         */
                        if (pw_is_slm) {
                            suspend_time_ticks = (pw_suspend_stop_s0i3 - pw_suspend_start_s0i3);
                        } else {
                            suspend_time_usecs = (pw_suspend_stop_s0i3 - pw_suspend_start_s0i3);
                            suspend_time_ticks = suspend_time_usecs * base_operating_freq_mhz;
                        }
                        printk(KERN_INFO "BASE operating freq_mhz = %llu\n", base_operating_freq_mhz);
                        printk(KERN_INFO "POST SUSPEND s0i3 = %llu, S3 RESIDENCY = %llu (%llu ticks)\n", pw_suspend_stop_s0i3, suspend_time_usecs, suspend_time_ticks);


                        /*
                         * PWR library EXPECTS 5th entry to be the ACPI S3 residency (in clock ticks)!
                         */
                        values[4] = suspend_time_ticks;

                        msg.p_data = (u64)((unsigned long)(smsg));

                        /*
                         * OK, everything computed. Now copy
                         * this sample into an output buffer
                         */
                        pw_produce_generic_msg(&msg, true); // "true" ==> allow wakeups
                    }
                } // IS_S_RESIDENCY_MODE()
            } else {
                tsc_suspend_time_ticks = (pw_suspend_stop_tsc - pw_suspend_start_tsc);
                suspend_time_ticks = tsc_suspend_time_ticks;
            }
            printk(KERN_INFO "OK: suspend time ticks = %llu\n", suspend_time_ticks);
            if (IS_ACPI_S3_MODE()) {
                // produce_acpi_s3_sample(suspend_time_ticks /* s3 res */);
                produce_acpi_s3_sample(pw_suspend_stop_tsc, suspend_time_ticks /* s3 res */);
            }

            break;
        default:
            pw_pr_error("pw: unknown = %lu\n", state);
    }
    return NOTIFY_DONE;
};
/*
 * PM notifier.
 */
struct notifier_block pw_alrm_pm_suspend_notifier = {
    .notifier_call = &pw_alrm_suspend_notifier_callback_i,
};

static inline int get_arg_lengths(unsigned long ioctl_param, int *in_len, int *out_len)
{
    ioctl_args_stub_t local_stub, *remote_stub;

    if (!in_len || !out_len) {
        pw_pr_error("ERROR: NULL in_len or out_len?!\n");
        return -ERROR;
    }

    remote_stub = (ioctl_args_stub_t *)ioctl_param;
    if(copy_from_user(&local_stub, remote_stub, sizeof(ioctl_args_stub_t))){
	pw_pr_error("ERROR: could NOT extract local stub!\n");
	return -ERROR;
    }
    OUTPUT(0, KERN_INFO "OK: in_len = %d, out_len = %d\n", local_stub.in_len, local_stub.out_len);
    *in_len = local_stub.in_len; *out_len = local_stub.out_len;
    return SUCCESS;
};


#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
#define MATCH_IOCTL(num, pred) ( (num) == (pred) || (num) == (pred##32) )
#else
#define MATCH_IOCTL(num, pred) ( (num) == (pred) )
#endif

/*
 * Service IOCTL calls from user-space.
 * Handles both 32b and 64b calls.
 */
long pw_unlocked_handle_ioctl_i(unsigned int ioctl_num, struct PWCollector_ioctl_arg *remote_args, unsigned long ioctl_param)
{
    int local_in_len, local_out_len;
    PWCollector_cmd_t cmd;
    int tmp = -1;
    struct PWCollector_ioctl_arg local_args;

    // printk(KERN_INFO "HANDLING IOCTL: %u\n", ioctl_num);

    if (!remote_args) {
        pw_pr_error("ERROR: NULL remote_args value?!\n");
        return -ERROR;
    }

    /*
     * (1) Sanity check:
     * Before doing anything, double check to
     * make sure this IOCTL was really intended
     * for us!
     */
    if(_IOC_TYPE(ioctl_num) != APWR_IOCTL_MAGIC_NUM){
	pw_pr_error("ERROR: requested IOCTL TYPE (%d) != APWR_IOCTL_MAGIC_NUM (%d)\n", _IOC_TYPE(ioctl_num), APWR_IOCTL_MAGIC_NUM);
	return -ERROR;
    }
    /*
     * (2) Extract arg lengths.
     */
    if (copy_from_user(&local_args, remote_args, sizeof(local_args))) {
        pw_pr_error("ERROR copying in data from userspace\n");
        return -ERROR;
    }
    local_in_len = local_args.in_len;
    local_out_len = local_args.out_len;
    OUTPUT(0, KERN_INFO "GU: local_in_len = %d, local_out_len = %d\n", local_in_len, local_out_len);
    /*
     * (3) Service individual IOCTL requests.
     */
    if(MATCH_IOCTL(ioctl_num, PW_IOCTL_CONFIG)){
	// printk(KERN_INFO "PW_IOCTL_CONFIG\n");
	// return set_config((struct PWCollector_config *)remote_args->in_arg, local_in_len);
	return set_config((struct PWCollector_config __user *)local_args.in_arg, local_in_len);
    }
    else if(MATCH_IOCTL(ioctl_num, PW_IOCTL_CMD)){
	if (get_user(cmd, ((PWCollector_cmd_t __user *)local_args.in_arg))) {
	    pw_pr_error("ERROR: could NOT extract cmd value!\n");
	    return -ERROR;
	}
	// return handle_cmd(cmd);
        // return do_cmd(cmd, (u64 *)remote_args->out_arg, local_out_len);
        return do_cmd(cmd, (u64 __user *)local_args.out_arg, local_out_len);
    }
    else if(MATCH_IOCTL(ioctl_num, PW_IOCTL_STATUS)){
	// printk(KERN_INFO "PW_IOCTL_STATUS\n");
	/*
	 * For now, we assume STATUS information can only
	 * be retrieved for an ACTIVE collection.
	 */
	if(!IS_COLLECTING()){
	    pw_pr_error("\tError: status information requested, but NO COLLECTION ONGOING!\n");
	    return -ERROR;
	}
#if DO_IOCTL_STATS
	return get_status((struct PWCollector_status __user *)local_args.out_arg, local_out_len);
#else
	return -ERROR;
#endif
    }
    else if(MATCH_IOCTL(ioctl_num, PW_IOCTL_CHECK_PLATFORM)) {
	// printk(KERN_INFO "PW_IOCTL_CHECK_PLATFORM\n");
	if( (tmp = check_platform((struct PWCollector_check_platform __user *)local_args.out_arg, local_out_len)))
	    if(tmp < 0) // ERROR
		return 2; // for PW_IOCTL_CHECK_PLATFORM: >= 2 ==> Error; == 1 => SUCCESS, but not EOF; 0 ==> SUCCESS, EOF
	return tmp;
    }
    else if(MATCH_IOCTL(ioctl_num, PW_IOCTL_VERSION)){
	// printk(KERN_INFO "PW_IOCTL_VERSION\n");
	OUTPUT(3, KERN_INFO "OUT len = %d\n", local_out_len);
	return get_version((struct PWCollector_version_info __user *)local_args.out_arg, local_out_len);
    }
    else if(MATCH_IOCTL(ioctl_num, PW_IOCTL_MICRO_PATCH)){
	// printk(KERN_INFO "PW_IOCTL_MICRO_PATCH\n");
	return get_micro_patch_ver((int __user *)local_args.out_arg, local_out_len);
    }
    else if(MATCH_IOCTL(ioctl_num, PW_IOCTL_TURBO_THRESHOLD)){
	// printk(KERN_INFO "PW_IOCTL_TURBO_THRESHOLD\n");
	return get_turbo_threshold((struct PWCollector_turbo_threshold __user *)local_args.out_arg, local_out_len);
    }
    else if (MATCH_IOCTL(ioctl_num, PW_IOCTL_COLLECTION_TIME)) {
        /*
         * Only supported on Android/Moorestown!!!
         */
        // printk(KERN_INFO "PW_IOCTL_COLLECTION_TIME\n");
// #ifdef CONFIG_X86_MRST
        {
            unsigned int local_collection_time_secs = 0;
            if (get_user(local_collection_time_secs, (unsigned long __user *)local_args.in_arg)) {
                pw_pr_error("ERROR extracting local collection time!\n");
                return -ERROR;
            }
            // printk(KERN_INFO "OK: received local collection time = %u seconds\n", local_collection_time_secs);
            /*
             * Get (and set) collection START time...
             */
            {
                // pw_rtc_time_start = pw_get_current_rtc_time_seconds();
            }
            /*
             * ...and the total collection time...
             */
            {
                pw_collection_time_secs = local_collection_time_secs;
                pw_collection_time_ticks = (u64)local_collection_time_secs * (u64)base_operating_freq_khz * 1000;
            }
            /*
             * ...and the client task.
             */
            {
                pw_power_collector_task = current;
            }
        }
// #endif // CONFIG_X86_MRST
        return SUCCESS;
    }
    else if (MATCH_IOCTL(ioctl_num, PW_IOCTL_MMAP_SIZE)) {
        // printk(KERN_INFO "MMAP_SIZE received!\n");
        if (put_user(pw_buffer_alloc_size, (unsigned long __user *)local_args.out_arg)) {
            pw_pr_error("ERROR transfering buffer size!\n");
            return -ERROR;
        }
        return SUCCESS;
    }
    else if (MATCH_IOCTL(ioctl_num, PW_IOCTL_BUFFER_SIZE)) {
        unsigned long buff_size = pw_get_buffer_size();
        pw_pr_debug("BUFFER_SIZE received!\n");
        if(put_user(buff_size, (unsigned long __user *)local_args.out_arg)) {
            pw_pr_error("ERROR transfering buffer size!\n");
            return -ERROR;
        }
        return SUCCESS;
    }
    else if (MATCH_IOCTL(ioctl_num, PW_IOCTL_DO_D_NC_READ)) {
        return SUCCESS;
    }
    else if (MATCH_IOCTL(ioctl_num, PW_IOCTL_FSB_FREQ)) {
        // printk(KERN_INFO "PW_IOCTL_FSB_FREQ  received!\n");
        /*
         * UPDATE: return fsb-freq AND max non-turbo ratio here.
         * UPDATE: and also the LFM ratio (i.e. "max efficiency")
         * UPDATE: and also the max turbo ratio (i.e. "HFM")
         */
        {
            u64 __fsb_non_turbo = ((pw_u64_t)pw_max_turbo_ratio << 48 | (pw_u64_t)pw_max_non_turbo_ratio << 32 | pw_max_efficiency_ratio << 16 | pw_msr_fsb_freq_value);
            pw_pr_debug("__fsb_non_turbo = %llu\n", __fsb_non_turbo);
            if (put_user(__fsb_non_turbo, (u64 __user *)local_args.out_arg)) {
                pw_pr_error("ERROR transfering FSB_FREQ MSR value!\n");
                return -ERROR;
            }
        }
        return SUCCESS;
    }
    else if (MATCH_IOCTL(ioctl_num, PW_IOCTL_MSR_ADDRS)) {
        // printk(KERN_INFO "PW_IOCTL_MSR_ADDRS\n");
        return pw_set_msr_addrs((struct pw_msr_info __user *)local_args.in_arg, local_in_len);
    }
    else if (MATCH_IOCTL(ioctl_num, PW_IOCTL_PLATFORM_RES_CONFIG)) {
        // printk(KERN_INFO "PW_IOCTL_PLATFORM_RES_CONFIG encountered!\n");
        return pw_set_platform_res_config_i((struct PWCollector_platform_res_info __user *)local_args.in_arg, local_in_len);
        // return -ERROR;
    }
    else{
	// ERROR!
	pw_pr_error("Invalid IOCTL command = %u\n", ioctl_num);
	return -ERROR;
    }
    /*
     * Should NEVER reach here!
     */
    return -ERROR;
};

/*
 * (1) Handle 32b IOCTLs in 32b kernel-space.
 * (2) Handle 64b IOCTLs in 64b kernel-space.
 */
long pw_device_unlocked_ioctl(struct file *filp, unsigned int ioctl_num, unsigned long ioctl_param)
{
    OUTPUT(3, KERN_INFO "64b transfering to handler!\n");
    return pw_unlocked_handle_ioctl_i(ioctl_num, (struct PWCollector_ioctl_arg *)ioctl_param, ioctl_param);
};

#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
/*
 * Handle 32b IOCTLs in 64b kernel-space.
 */
long pw_device_compat_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
    struct PWCollector_ioctl_arg32 __user *remote_args32 = compat_ptr(ioctl_param);
    struct PWCollector_ioctl_arg __user *remote_args = NULL;
    u32 data;
    int tmp;

    remote_args = compat_alloc_user_space(sizeof(*remote_args));
    if (!remote_args) {
        return -ERROR;
    }
    if (get_user(tmp, &remote_args32->in_len) || put_user(tmp, &remote_args->in_len)) {
        return -ERROR;
    }
    if (get_user(tmp, &remote_args32->out_len) || put_user(tmp, &remote_args->out_len)) {
        return -ERROR;
    }
    if (get_user(data, &remote_args32->in_arg) || put_user(compat_ptr(data), &remote_args->in_arg)) {
        return -ERROR;
    }
    if (get_user(data, &remote_args32->out_arg) || put_user(compat_ptr(data), &remote_args->out_arg)) {
        return -ERROR;
    }

    // printk(KERN_INFO "OK, copied. Remote_args = %p\n", remote_args);

    // return -ERROR;
    return pw_unlocked_handle_ioctl_i(ioctl_num, remote_args, ioctl_param);
};
#endif // COMPAT && x64

/*
 * Service an "open(...)" call from user-space.
 */
static int pw_device_open(struct inode *inode, struct file *file)
{
    /*
     * We don't want to talk to two processes at the same time
     */
    if(test_and_set_bit(DEV_IS_OPEN, &dev_status)){
	// Device is busy
	return -EBUSY;
    }

    try_module_get(THIS_MODULE);
    return SUCCESS;
};

/*
 * Service a "close(...)" call from user-space.
 */
static int pw_device_release(struct inode *inode, struct file *file)
{
    OUTPUT(3, KERN_INFO "Debug: Device Release!\n");
    /*
     * Did the client just try to zombie us?
     */
    if(IS_COLLECTING()){
	pw_pr_error("ERROR: Detected ongoing collection on a device release!\n");
	INTERNAL_STATE.cmd = PW_CANCEL;
	stop_collection(PW_CANCEL);
    }
    module_put(THIS_MODULE);
    /*
     * We're now ready for our next caller
     */
    clear_bit(DEV_IS_OPEN, &dev_status);
    return SUCCESS;
};


int pw_register_dev(void)
{
    int ret;

    /*
     * Create the character device
     */
    ret = alloc_chrdev_region(&apwr_dev, 0, 1, PW_DEVICE_NAME);
    apwr_dev_major_num = MAJOR(apwr_dev);
    apwr_class = class_create(THIS_MODULE, "apwr");
    if(IS_ERR(apwr_class))
        printk(KERN_ERR "Error registering apwr class\n");

    device_create(apwr_class, NULL, apwr_dev, NULL, PW_DEVICE_NAME);
    apwr_cdev = cdev_alloc();
    if (apwr_cdev == NULL) {
        printk("Error allocating character device\n");
        return ret;
    }
    apwr_cdev->owner = THIS_MODULE;
    apwr_cdev->ops = &Fops;
    if( cdev_add(apwr_cdev, apwr_dev, 1) < 0 )  {
        printk("Error registering device driver\n");
        return ret;
    }

    return ret;
};

void pw_unregister_dev(void)
{
    /*
     * Remove the device
     */
    unregister_chrdev(apwr_dev_major_num, PW_DEVICE_NAME);
    device_destroy(apwr_class, apwr_dev);
    class_destroy(apwr_class);
    unregister_chrdev_region(apwr_dev, 1);
    cdev_del(apwr_cdev);
};

#if 0
#ifndef __arm__
static void disable_auto_demote(void *dummy)
{
    unsigned long long auto_demote_disable_flags = AUTO_DEMOTE_FLAGS();
    unsigned long long msr_addr = AUTO_DEMOTE_MSR;
    unsigned long long msr_bits = 0, old_msr_bits = 0;

    WUWATCH_RDMSRL(msr_addr, msr_bits);
    old_msr_bits = msr_bits;
    msr_bits &= ~auto_demote_disable_flags;
    wrmsrl(msr_addr, msr_bits);

    if (true) {
        printk(KERN_INFO "[%d]: old_msr_bits = %llu, was auto enabled = %s, DISABLED auto-demote\n", RAW_CPU(), old_msr_bits, GET_BOOL_STRING(IS_AUTO_DEMOTE_ENABLED(old_msr_bits)));
    }
};

static void enable_auto_demote(void *dummy)
{
    unsigned long long auto_demote_disable_flags = AUTO_DEMOTE_FLAGS();
    unsigned long long msr_addr = AUTO_DEMOTE_MSR;
    unsigned long long msr_bits = 0, old_msr_bits = 0;

    WUWATCH_RDMSRL(msr_addr, msr_bits);
    old_msr_bits = msr_bits;
    msr_bits |= auto_demote_disable_flags;
    wrmsrl(msr_addr, msr_bits);

    if (true) {
        printk(KERN_INFO "[%d]: OLD msr_bits = %llu, NEW msr_bits = %llu\n", raw_smp_processor_id(), old_msr_bits, msr_bits);
    }
};
#endif // ifndef __arm__
#endif

static bool check_auto_demote_flags(int cpu)
{
#ifndef __arm__
    u32 l=0, h=0;
    u64 msr_val = 0;
    WARN_ON(WUWATCH_RDMSR_SAFE_ON_CPU(cpu, AUTO_DEMOTE_MSR, &l, &h));
    msr_val = (u64)h << 32 | (u64)l;
    return IS_AUTO_DEMOTE_ENABLED(msr_val);
#else
    return false;
#endif // ifndef __arm__
};

static bool check_any_thread_flags(int cpu)
{
#ifndef __arm__
    u32 l=0, h=0;
    u64 msr_val = 0;
    WARN_ON(WUWATCH_RDMSR_SAFE_ON_CPU(cpu, IA32_FIXED_CTR_CTL_ADDR, &l, &h));
    msr_val = (u64)h << 32 | (u64)l;
    return IS_ANY_THREAD_SET(msr_val);
#else
    return false;
#endif // ifndef __arm__
};

static void check_arch_flags(void)
{
    int cpu = 0;
    /*
     * It is ASSUMED that auto-demote and any-thread will either be set on ALL Cpus or on
     * none!
     */
    pw_is_any_thread_set = check_any_thread_flags(cpu);
    pw_is_auto_demote_enabled = check_auto_demote_flags(cpu);

    OUTPUT(0, KERN_INFO "any thread set = %s, auto demote enabled = %s\n", GET_BOOL_STRING(pw_is_any_thread_set), GET_BOOL_STRING(pw_is_auto_demote_enabled));
    return;
};

#if 0
#ifndef __arm__
/*
 * Enable CPU_CLK_UNHALTED.REF counting
 * by setting bits 8,9 in MSR_PERF_FIXED_CTR_CTRL
 * MSR (addr == 0x38d). Also store the previous
 * value of the MSR.
 */
static void enable_ref(void)
{
    int cpu;
    u64 res;
    int ret;

    u32 *data_copy;// [2];
    u32 data[2];

    for_each_online_cpu(cpu){
        /*
         * (1) Do for IA32_FIXED_CTR_CTL
         */
        {
            data_copy = (&per_cpu(CTRL_data_values, cpu))->fixed_data;
            ret = WUWATCH_RDMSR_SAFE_ON_CPU(cpu, IA32_FIXED_CTR_CTL_ADDR, &data[0], &data[1]);
            WARN(ret, KERN_WARNING "rdmsr failed with code %d\n", ret);
            memcpy(data_copy, data, sizeof(u32) * 2);
            /*
             * Turn on CPU_CLK_UNHALTED.REF counting.
             *
             * UPDATE: also turn on CPU_CLK_UNHALTED.CORE counting.
             */
            // data[0] |= 0x300;
            data[0] |= 0x330;

            ret = wrmsr_safe_on_cpu(cpu, IA32_FIXED_CTR_CTL_ADDR, data[0], data[1]);
        }
        /*
         * (2) Do for IA32_PERF_GLOBAL_CTRL_ADDR
         */
        {
            data_copy = (&per_cpu(CTRL_data_values, cpu))->perf_data;
            ret = WUWATCH_RDMSR_SAFE_ON_CPU(cpu, IA32_PERF_GLOBAL_CTRL_ADDR, &data[0], &data[1]);
            WARN(ret, KERN_WARNING "rdmsr failed with code %d\n", ret);
            memcpy(data_copy, data, sizeof(u32) * 2);
            res = data[1];
            res <<= 32;
            res += data[0];
            OUTPUT(0, KERN_INFO "[%d]: READ res = 0x%llx\n", cpu, res);
            /*
             * Turn on CPU_CLK_UNHALTED.REF counting.
             *
             * UPDATE: also turn on CPU_CLK_UNHALTED.CORE counting.
             * Set bits 33, 34
             */
            // data[0] |= 0x330;
            data[1] |= 0x6;
            // data[0] = data[1] = 0x0;

            ret = wrmsr_safe_on_cpu(cpu, IA32_PERF_GLOBAL_CTRL_ADDR, data[0], data[1]);
        }
    }
};

static void restore_ref(void)
{
    int cpu;
    u64 res;
    int ret;

    u32 *data_copy;
    u32 data[2];

    memset(data, 0, sizeof(u32) * 2);

    for_each_online_cpu(cpu){
        /*
         * (1) Do for IA32_FIXED_CTR_CTL
         */
        {
            data_copy = (&per_cpu(CTRL_data_values, cpu))->fixed_data;
            memcpy(data, data_copy, sizeof(u32) * 2);

            res = data[1];
            res <<= 32;
            res += data[0];

            OUTPUT(3, KERN_INFO "[%d]: PREV res = 0x%llx\n", cpu, res);
            if( (ret = wrmsr_safe_on_cpu(cpu, IA32_FIXED_CTR_CTL_ADDR, data[0], data[1]))){
                pw_pr_error("ERROR writing PREVIOUS IA32_FIXED_CTR_CLT_ADDR values for CPU = %d!\n", cpu);
            }
        }
        /*
         * (2) Do for IA32_PERF_GLOBAL_CTRL_ADDR
         */
        {
            data_copy = (&per_cpu(CTRL_data_values, cpu))->perf_data;
            memcpy(data, data_copy, sizeof(u32) * 2);

            res = data[1];
            res <<= 32;
            res += data[0];

            OUTPUT(3, KERN_INFO "[%d]: PREV res = 0x%llx\n", cpu, res);
            if( (ret = wrmsr_safe_on_cpu(cpu, IA32_PERF_GLOBAL_CTRL_ADDR, data[0], data[1]))){
                pw_pr_error("ERROR writing PREVIOUS IA32_PERF_GLOBAL_CTRL_ADDR values for CPU = %d!\n", cpu);
            }
        }
    }
};
#endif // ifndef __arm__
#endif

static void get_fms(unsigned int *family, unsigned int *model, unsigned int *stepping)
{
    unsigned int ecx, edx;
    unsigned int fms;

    if (!family || !model || !stepping) {
        pw_pr_error("ERROR: NULL family/model/stepping value?!\n");
        return;
    }

    asm("cpuid" : "=a" (fms), "=c" (ecx), "=d" (edx) : "a" (1) : "ebx");

    *family = (fms >> 8) & 0xf;
    *model = (fms >> 4) & 0xf;
    *stepping = fms & 0xf;

    if (*family == 6 || *family == 0xf){
        *model += ((fms >> 16) & 0xf) << 4;
    }
    pw_pr_debug("FMS = 0x%x:%x:%x (%d:%d:%d)\n", *family, *model, *stepping, *family, *model, *stepping);
};
/*
 * Check if we're running on ATM.
 */
static atom_arch_type_t is_atm(void)
{
#ifndef __arm__
    unsigned int family, model, stepping;

    get_fms(&family, &model, &stepping);
    /*
     * This check below will need to
     * be updated for each new
     * architecture type!!!
     */
    if (family == 0x6) {
        switch (model) {
            case 0x27:
                switch (stepping) {
                    case 0x1:
                        return MFD;
                    case 0x2:
                        return LEX;
                }
                break;
            case 0x35:
                return CLV;
        }
    }
#endif // ifndef __arm__
    return NON_ATOM;
};

static slm_arch_type_t is_slm(void)
{
#ifndef __arm__
    unsigned int family, model, stepping;

    get_fms(&family, &model, &stepping);
    /*
     * This check below will need to
     * be updated for each new
     * architecture type!!!
     */
    if (family == 0x6) {
        switch (model) {
            case 0x37:
                return SLM_VLV2;
            case 0x4a:
                return SLM_TNG;
            case 0x4c:
                return SLM_CHV;
            case 0x5a:
                return SLM_ANN;
            default:
                break;
        }
    }
#endif // __arm__
    return NON_SLM;
};

static bool is_hsw(void)
{
#ifndef __arm__
    unsigned int family, model, stepping;

    get_fms(&family, &model, &stepping);
    /*
     * This check below will need to
     * be updated for each new
     * architecture type!!!
     */
    if (family == 0x6) {
        switch (model) {
            case 0x3c:
            case 0x45:
                return true;
            default:
                break;
        }
    }
#endif // __arm__
    return false;
};

static bool is_bdw(void)
{
#ifndef __arm__
    unsigned int family, model, stepping;

    get_fms(&family, &model, &stepping);
    /*
     * This check below will need to
     * be updated for each new
     * architecture type!!!
     */
    if (family == 0x6) {
        switch (model) {
            case 0x3d:
            case 0x47:
                return true;
            default:
                break;
        }
    }
#endif // __arm__
    return false;
};

static void test_wlock_mappings(void)
{
#if DO_WAKELOCK_SAMPLE
    produce_w_sample(0, 0x1, PW_WAKE_LOCK, 0, 0, "abcdef", "swapper", 0x0);
    produce_w_sample(0, 0x1, PW_WAKE_LOCK, 0, 0, "PowerManagerService", "swapper", 0x0);
    produce_w_sample(0, 0x1, PW_WAKE_LOCK, 0, 0, "abcdef", "swapper", 0x0);
#endif
};

#define PW_GET_SCU_FW_MAJOR(num) ( ( (num) >> 8 ) & 0xff)
#define PW_GET_SCU_FW_MINOR(num) ( (num) & 0xff )

#ifdef CONFIG_X86_WANT_INTEL_MID
static int pw_do_parse_sfi_oemb_table_i(struct sfi_table_header *header)
{
    struct sfi_table_oemb *oemb; // "struct sfi_table_oemb" defined in "intel-mid.h"

    oemb = (struct sfi_table_oemb *)header;
    if (!oemb) {
        printk(KERN_INFO "ERROR: NULL sfi table header?!\n");
        return -ERROR;
    }
    pw_scu_fw_major_minor = (oemb->scu_runtime_major_version << 8) | (oemb->scu_runtime_minor_version);
    pw_pr_debug("Major = %u, Minor = %u\n", oemb->scu_runtime_major_version, oemb->scu_runtime_minor_version);

    return SUCCESS;
};
static void pw_do_extract_scu_fw_version(void)
{
    if (sfi_table_parse(SFI_SIG_OEMB, NULL, NULL, &pw_do_parse_sfi_oemb_table_i)) {
        printk(KERN_INFO "WARNING: no SFI information; resetting SCU F/W version!\n");
        pw_scu_fw_major_minor = 0x0;
    }
};
#else // CONFIG_X86_WANT_INTEL_MID
static void pw_do_extract_scu_fw_version(void)
{
    pw_scu_fw_major_minor = 0x0;
};
#endif // CONFIG_X86_WANT_INTEL_MID

static int __init init_hooks(void)
{
    int ret = SUCCESS;

    if (false) {
#ifndef __arm__
        printk(KERN_INFO "F.M = %u.%u\n", boot_cpu_data.x86, boot_cpu_data.x86_model);
#endif // ifndef __arm__
        return -ERROR;
    }


    OUTPUT(0, KERN_INFO "# IRQS = %d\n", NR_IRQS);

    OUTPUT(0, KERN_INFO "Sizeof PWCollector_sample_t = %lu, Sizeof k_sample_t = %lu\n", sizeof(PWCollector_sample_t), sizeof(k_sample_t));

    /*
     * We first check to see if
     * TRACEPOINTS are ENABLED in the kernel.
     * If not, EXIT IMMEDIATELY!
     */
#ifdef CONFIG_TRACEPOINTS
    OUTPUT(0, KERN_INFO "Tracepoints ON!\n");
#else
    pw_pr_error("ERROR: TRACEPOINTS NOT found on system!!!\n");
    return -ERROR;
#endif

    /*
     * Check if we're running on ATM.
     */
    pw_is_atm = is_atm();
    /*
     * Check if we're running on SLM.
     */
    pw_is_slm = is_slm();
    /*
     * Check if we're running on HSW.
     */
    pw_is_hsw = is_hsw();
    /*
     * Check if we're running on BDW.
     */
    pw_is_bdw = is_bdw();
    /*
     * Sanity!
     */
    BUG_ON(pw_is_atm && pw_is_slm && pw_is_hsw && pw_is_bdw);

    /*
     * For MFLD, we also check
     * if the required microcode patches
     * have been installed. If
     * not then EXIT IMMEDIATELY!
     */
#if DO_CHECK_BO_MICROCODE_PATCH
    {
        /*
         * Read MSR 0x8b -- if microcode patch
         * has been applied then the first 12 bits
         * of the higher order 32 bits should be
         * >= 0x102.
         *
         * THIS CHECK VALID FOR ATM ONLY!!!
         */
        /*
         * Do check ONLY if we're ATM!
         */
        if(pw_is_atm){
#ifndef __arm__
            u64 res;
            u32 patch_val;

            WUWATCH_RDMSRL(0x8b, res);
            patch_val = (res >> 32) & 0xfff;
            if(patch_val < 0x102){
                pw_pr_error("ERROR: B0 micro code path = 0x%x: REQUIRED >= 0x102!!!\n", patch_val);
                return -ERROR;
            }
            micro_patch_ver = patch_val;
            OUTPUT(3, KERN_INFO "patch ver = %u\n", micro_patch_ver);
#endif // ifndef __arm__
        }else{
            OUTPUT(0, KERN_INFO "DEBUG: SKIPPING MICROCODE PATCH check -- NON ATM DETECTED!\n");
        }
    }
#endif

    /*
     * Read the 'FSB_FREQ' MSR to determine bus clock freq multiplier.
     * Update: ONLY if saltwell or Silvermont!
     */
    if (pw_is_atm || pw_is_slm) {
        u64 res;

        WUWATCH_RDMSRL(MSR_FSB_FREQ_ADDR, res);
        // memcpy(&pw_msr_fsb_freq_value, &res, sizeof(unsigned long));
        memcpy(&pw_msr_fsb_freq_value, &res, sizeof(pw_msr_fsb_freq_value));
        pw_pr_debug("MSR_FSB_FREQ value = %u\n", pw_msr_fsb_freq_value);
    } else {
        printk(KERN_INFO "NO FSB FREQ!\n");
    }
    /*
     * Read the Max non-turbo ratio.
     */
    {
        u64 res = 0;
        u16 ratio = 0;
        /*
         * Algo:
         * (1) If NHM/WMR/SNB -- read bits 15:8 of 'PLATFORM_INFO_MSR_ADDR'
         * (2) If MFLD/ATM -- read bits 44:40 of 'CLOCK_CR_GEYSIII_STAT' MSR
         * (3) If SLM -- read bits 21:16 of 'PUNIT_CR_IACORE_RATIOS' (MSR_IA32_IACORE_RATIOS) MSR
         * to extract the 'base operating ratio'.
         * To get actual TSC frequency, multiply this ratio
         * with the bus clock frequency.
         */
        if (pw_is_atm) {
            WUWATCH_RDMSRL(CLOCK_CR_GEYSIII_STAT_MSR_ADDR, res);
            /*
             * Base operating Freq ratio is
             * bits 44:40
             */
            ratio = (res >> 40) & 0x1f;
        } else if (pw_is_slm) {
            WUWATCH_RDMSRL(MSR_IA32_IACORE_RATIOS, res);
            ratio = (res >> 16) & 0x3F; // Bits 21:16
            /*
             * Debug code
             */
            {
                WUWATCH_RDMSRL(MSR_IA32_IACORE_TURBO_RATIOS, res);
                pw_pr_debug("IACORE_TURBO_RATIOS: res = %llu, val = %u\n", res, (u32)(res & 0x1f) /* bits [4:0] */);
            }
            /*
             * End debug code.
             */
        } else {
            WUWATCH_RDMSRL(PLATFORM_INFO_MSR_ADDR, res);
            /*
             * Base Operating Freq ratio is
             * bits 15:8
             */
            ratio = (res >> 8) & 0xff;
        }

        pw_max_non_turbo_ratio = ratio;
        pw_pr_debug("MAX non-turbo ratio = %u\n", (u32)pw_max_non_turbo_ratio);
    }
    /*
     * Read the max efficiency ratio
     * (AKA "LFM")
     */
    {
        u64 res = 0;
        u16 ratio = 0;
        /*
         * Algo:
         * (1) If "Core" -- read bits 47:40 of 'PLATFORM_INFO_MSR_ADDR'
         * (2) If Atom[STW] -- ???
         * (3) If Atom[SLM] -- read bits 13:8 of 'PUNIT_CR_IACORE_RATIOS' (MSR_IA32_IACORE_RATIOS) MSR
         * to extract the 'base operating ratio'.
         */
        if (pw_is_atm) {
            /*
             * TODO
             */
            ratio = 0x0;
        } else if (pw_is_slm) {
            WUWATCH_RDMSRL(MSR_IA32_IACORE_RATIOS, res);
            ratio = (res >> 8) & 0x3F; // Bits 13:8
        } else {
            WUWATCH_RDMSRL(PLATFORM_INFO_MSR_ADDR, res);
            ratio = (res >> 40) & 0xff;
        }
        pw_max_efficiency_ratio = ratio;
        pw_pr_debug("MAX EFFICIENCY RATIO = %u\n", (u32)pw_max_efficiency_ratio);
    }
    /*
     * Read the max turbo ratio.
     */
    {
        u64 res = 0;
        u16 ratio = 0;
        /*
         * Algo:
         * (1) If "Core" -- read bits 7:0 of 'MSR_TURBO_RATIO_LIMIT'
         * (2) If Atom[STW] -- ???
         * (3) If Atom[SLM] -- read bits 4:0 of MSR_IA32_IACORE_TURBO_RATIOS
         */
        if (pw_is_atm) {
            /*
             * TODO
             */
            ratio = 0x0;
        } else if (pw_is_slm) {
            WUWATCH_RDMSRL(MSR_IA32_IACORE_TURBO_RATIOS, res);
            ratio = res & 0x1F; // Bits 4:0
        } else {
            WUWATCH_RDMSRL(MSR_TURBO_RATIO_LIMIT, res);
            ratio = res & 0xff; // Bits 7:0
        }
        pw_max_turbo_ratio = ratio;
        pw_pr_debug("MAX TURBO RATIO = %u\n", (u32)pw_max_turbo_ratio);
    }
    /*
     * Extract SCU F/W version (if possible)
     */
    {
        pw_do_extract_scu_fw_version();
        printk(KERN_INFO "SCU F/W version = %X.%X\n", PW_GET_SCU_FW_MAJOR(pw_scu_fw_major_minor), PW_GET_SCU_FW_MINOR(pw_scu_fw_major_minor));
    }

    OUTPUT(3, KERN_INFO "Sizeof node = %lu\n", sizeof(tnode_t));
    OUTPUT(3, KERN_INFO "Sizeof per_cpu_t = %lu\n", sizeof(per_cpu_t));

    startJIFF = jiffies;

    if (pw_init_data_structures()) {
        return -ERROR;
    }

    if (false) {
        test_wlock_mappings();
        pw_destroy_data_structures();
        return -ERROR;
    }

    /*
    {
        disable_auto_demote(NULL);
        smp_call_function(disable_auto_demote, NULL, 1);
        printk(KERN_INFO "DISABLED AUTO-DEMOTE!\n");
    }
    */
    /*
     * Check Arch flags (ANY_THREAD, AUTO_DEMOTE etc.)
     */
    {
        check_arch_flags();
    }
    {
        // enable_ref();
    }

    {
        /*
         * Check if kernel-space call stack generation
         * is possible.
         */
#ifdef CONFIG_FRAME_POINTER
        OUTPUT(0, KERN_INFO "Frame pointer ON!\n");
        INTERNAL_STATE.have_kernel_frame_pointers = true;
#else
        printk(KERN_INFO "**********************************************************************************************************\n");
        printk(KERN_INFO "Error: kernel NOT compiled with frame pointers -- NO KERNEL-SPACE TIMER CALL TRACES WILL BE GENERATED!\n");
        printk(KERN_INFO "**********************************************************************************************************\n");
        INTERNAL_STATE.have_kernel_frame_pointers = false;
#endif
    }

    /*
     * "Register" the device-specific special character file here.
     */
    {
        if( (ret = pw_register_dev()) < 0) {
            goto err_ret_post_init;
        }
        /*
         * ...and then the matrix device file.
         */
        if (mt_register_dev()) {
            goto err_ret_post_pw_reg;
        }
    }

    /*
     * Probes required to cache (kernel) timer
     * callstacks need to be inserted, regardless
     * of collection status.
     */
    {
        // register_timer_callstack_probes();
        register_permanent_probes();
    }
    /*
     * Register SUSPEND/RESUME notifier.
     */
    {
        register_pm_notifier(&pw_alrm_pm_suspend_notifier);
    }

#if 0
    {
        register_all_probes();
    }
#endif

    printk(KERN_INFO "\n--------------------------------------------------------------------------------------------\n");
    printk(KERN_INFO "START Initialized the SOCWatch driver\n");
#ifdef CONFIG_X86_WANT_INTEL_MID
    printk(KERN_INFO "SOC Identifier = %u, Stepping = %u\n", intel_mid_identify_cpu(), intel_mid_soc_stepping());
#endif
    printk(KERN_INFO "--------------------------------------------------------------------------------------------\n");

    return SUCCESS;

err_ret_post_pw_reg:
    pw_unregister_dev();

err_ret_post_init:
    pw_destroy_data_structures();
    // restore_ref();
    /*
    {
        enable_auto_demote(NULL);
        smp_call_function(enable_auto_demote, NULL, 1);
        printk(KERN_INFO "ENABLED AUTO-DEMOTE!\n");
    }
    */

    return ret;
};

static void __exit cleanup_hooks(void)
{
    unsigned long elapsedJIFF = 0, collectJIFF = 0;
    int num_timers = 0, num_irqs = 0;

    {
        mt_unregister_dev();
        pw_unregister_dev();
    }

    /*
     * Unregister the suspend notifier.
     */
    {
        unregister_pm_notifier(&pw_alrm_pm_suspend_notifier);
    }

    /*
     * Probes required to cache (kernel) timer
     * callstacks need to be removed, regardless
     * of collection status.
     */
    {
        // unregister_timer_callstack_probes();
        unregister_permanent_probes();
    }

#if 1
    if(IS_COLLECTING()){
        // unregister_all_probes();
        unregister_non_pausable_probes();
        unregister_pausable_probes();
    }
    else if(IS_SLEEPING()){
        unregister_pausable_probes();
    }
#else
    /*
     * Forcibly unregister -- used in debugging.
     */
    {
        unregister_all_probes();
    }
#endif


    {
        num_timers = get_num_timers();
#if DO_CACHE_IRQ_DEV_NAME_MAPPINGS
        num_irqs = get_num_irq_mappings();
#endif
    }

    {
        pw_destroy_data_structures();
    }

    {
        // restore_ref();
    }
    /*
    {
        enable_auto_demote(NULL);
        smp_call_function(enable_auto_demote, NULL, 1);
        printk(KERN_INFO "ENABLED AUTO-DEMOTE!\n");
    }
    */

    /*
     * Collect some statistics: total execution time.
     */
    stopJIFF = jiffies;
    if(stopJIFF < startJIFF){
        OUTPUT(0, KERN_INFO "WARNING: jiffies counter has WRAPPED AROUND!\n");
        elapsedJIFF = 0; // avoid messy NAN when dividing
    }else{
        elapsedJIFF = stopJIFF - startJIFF;
    }

    /*
     * Collect some collection statistics: total collection time.
     */
    if(INTERNAL_STATE.collectionStopJIFF < INTERNAL_STATE.collectionStartJIFF){
        OUTPUT(0, KERN_INFO "WARNING: jiffies counter has WRAPPED AROUND!\n");
        collectJIFF = 0;
    }else{
        collectJIFF = INTERNAL_STATE.collectionStopJIFF - INTERNAL_STATE.collectionStartJIFF;
    }

    printk(KERN_INFO "\n--------------------------------------------------------------------------------------------\n");

    printk(KERN_INFO "STOP Terminated the SOCWatch driver.\n");
#if DO_PRINT_COLLECTION_STATS
    printk(KERN_INFO "Total time elapsed = %u msecs, Total collection time = %u msecs\n", jiffies_to_msecs(elapsedJIFF), jiffies_to_msecs(collectJIFF));

    printk(KERN_INFO "Total # timers = %d, Total # irq mappings = %d\n", num_timers, num_irqs);

#if DO_OVERHEAD_MEASUREMENTS
    {
        timer_init_print_cumulative_overhead_params("TIMER_INIT");
        timer_expire_print_cumulative_overhead_params("TIMER_EXPIRE");
        timer_insert_print_cumulative_overhead_params("TIMER_INSERT");
        tps_print_cumulative_overhead_params("TPS");
        tps_lite_print_cumulative_overhead_params("TPS_LITE");
        tpf_print_cumulative_overhead_params("TPF");
        inter_common_print_cumulative_overhead_params("INTER_COMMON");
        irq_insert_print_cumulative_overhead_params("IRQ_INSERT");
        find_irq_node_i_print_cumulative_overhead_params("FIND_IRQ_NODE_I");
        exit_helper_print_cumulative_overhead_params("EXIT_HELPER");
        timer_delete_print_cumulative_overhead_params("TIMER_DELETE");
        sys_enter_helper_i_print_cumulative_overhead_params("SYS_ENTER_HELPER_I");
        sys_exit_helper_i_print_cumulative_overhead_params("SYS_EXIT_HELPER_I");
        /*
         * Also print stats on timer entries.
         */
        printk(KERN_INFO "# TIMER ENTRIES = %d\n", atomic_read(&num_timer_entries));
        /*
         * And some mem debugging stats.
         */
        printk(KERN_INFO "TOTAL # BYTES ALLOCED = %llu, CURR # BYTES ALLOCED = %llu, MAX # BYTES ALLOCED = %llu\n", TOTAL_NUM_BYTES_ALLOCED(), CURR_NUM_BYTES_ALLOCED(), MAX_NUM_BYTES_ALLOCED());
    }
#endif // DO_OVERHEAD_MEASUREMENTS
#endif // DO_PRINT_COLLECTION_STATS

    printk(KERN_INFO "--------------------------------------------------------------------------------------------\n");
};

module_init(init_hooks);
module_exit(cleanup_hooks);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(MOD_AUTHOR);
MODULE_DESCRIPTION(MOD_DESC);
