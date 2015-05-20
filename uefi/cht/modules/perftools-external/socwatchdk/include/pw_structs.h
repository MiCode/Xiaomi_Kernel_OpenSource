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

/*
 * Description: file containing data structures used by the
 * power driver.
 */

#ifndef _DATA_STRUCTURES_H_
#define _DATA_STRUCTURES_H_ 1

#include "pw_types.h"

/*
 * Should we probe on syscall enters and exits?
 * We require this functionality to handle certain
 * device-driver related timers.
 * ********************************************************
 * WARNING: SETTING TO 1 will INVOLVE HIGH OVERHEAD!!!
 * ********************************************************
 */
#define DO_PROBE_ON_SYSCALL_ENTER_EXIT 0
#define DO_PROBE_ON_EXEC_SYSCALL DO_PROBE_ON_SYSCALL_ENTER_EXIT
/*
 * Do we use an RCU-based mechanism
 * to determine which output buffers
 * to write to?
 * Set to: "1" ==> YES
 *         "0" ==> NO
 * ************************************
 * CAUTION: RCU-based output buffer
 * selection is EXPERIMENTAL ONLY!!!
 * ************************************
 */
#define DO_RCU_OUTPUT_BUFFERS 0
/*
 * Do we force the device driver to
 * (periodically) flush its buffers?
 * Set to: "1" ==> YES
 *       : "0" ==> NO
 * ***********************************
 * UPDATE: This value is now tied to the
 * 'DO_RCU_OUTPUT_BUFFERS' flag value
 * because, for proper implementations
 * of buffer flushing, we MUST have
 * an RCU-synchronized output buffering
 * mechanism!!!
 * ***********************************
 */
#define DO_PERIODIC_BUFFER_FLUSH DO_RCU_OUTPUT_BUFFERS
/*
 * Do we use a TPS "epoch" counter to try and
 * order SCHED_WAKEUP samples and TPS samples?
 * (Required on many-core architectures that don't have
 * a synchronized TSC).
 */
#define DO_TPS_EPOCH_COUNTER 1
/*
 * Should the driver count number of dropped samples?
 */
#define DO_COUNT_DROPPED_SAMPLES 1
/*
 * Should we allow the driver to terminate the wuwatch userspace
 * application dynamically?
 * Used ONLY by the 'suspend_notifier' in cases when the driver
 * detects the application should exit because the device
 * was in ACPI S3 for longer than the collection time.
 * DISABLED, FOR NOW
 */
#define DO_ALLOW_DRIVER_TERMINATION_OF_WUWATCH 0


#define NUM_SAMPLES_PER_SEG 512
#define SAMPLES_PER_SEG_MASK 511 /* MUST be (NUM_SAMPLES_PER_SEG - 1) */
#if 1
    #define NUM_SEGS_PER_BUFFER 2 /* MUST be POW-of-2 */
    #define NUM_SEGS_PER_BUFFER_MASK 1 /* MUST be (NUM_SEGS_PER_BUFFER - 1) */
#else
    #define NUM_SEGS_PER_BUFFER 4 /* MUST be POW-of-2 */
    #define NUM_SEGS_PER_BUFFER_MASK 3 /* MUST be (NUM_SEGS_PER_BUFFER - 1) */
#endif

#define SEG_SIZE (NUM_SAMPLES_PER_SEG * sizeof(PWCollector_sample_t))

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

/*
 * The MAX number of entries in the "trace" array of the "k_sample_t" structure.
 * If the actual backtrace is longer, multiple
 * k_sample structs need to be chained together (see "sample_len" in
 * the "sample" struct).
 */
#define PW_TRACE_LEN 11
#define TRACE_LEN PW_TRACE_LEN // required by PERFRUN
/*
 * Max size of a module name. Ideally we'd
 * like to directly include "module.h" (which
 * defines this value), but this code
 * will be shared with Ring-3 code, which is
 * why we redefine it here.
 */
#define PW_MODULE_NAME_LEN (64 - sizeof(unsigned long))
/*
 * MAX size of each irq name (bytes).
 */
#define PW_IRQ_DEV_NAME_LEN 100
/*
 * MAX size of each proc name.
 */
#define PW_MAX_PROC_NAME_SIZE 16

/*
 * We remove this to avoid any hard coded information regarding hardware
 * Meta data samples will include this kind of information
 */
#if 1
/*
 * MAX number of logical subsystems in south complex.
 * for Medfield platform
 */
#define MFD_MAX_LSS_NUM_IN_SC 31
/*
 * MAX number of logical subsystems in south complex.
 * for Clovertrail platform.
 */
#define CLV_MAX_LSS_NUM_IN_SC 25
/*
 * MAX number of logical subsystems.
 * Choose whichever is the maximum among available platforms
 * defined above
 */
#define MAX_LSS_NUM_IN_SC 31

/*
 * MAX number of logical subsystems in north complex.
 * for Medfield platform
 */
#define MFD_MAX_LSS_NUM_IN_NC 9
/*
 * MAX number of logical subsystems in north complex.
 * for Clovertrail platform
 */
#define CLV_MAX_LSS_NUM_IN_NC 7
/*
 * MAX number of logical subsystems.
 * Choose whichever is the maximum among available platforms
 * defined above
 */
#define MAX_LSS_NUM_IN_NC 9
#endif

/*
 * MAX size of each wakelock name.
 */
#define PW_MAX_WAKELOCK_NAME_SIZE 76
/*
 * Device {short, long} names.
 * Used for MFLD.
 */
#define PW_MAX_DEV_SHORT_NAME_SIZE 10
#define PW_MAX_DEV_LONG_NAME_SIZE 80
/*
 * Package names used for Android OS.
 */
#define PW_MAX_PKG_NAME_SIZE 80
/*
 * Max # of 'd_residency' counters present
 * in a single 'd_residency_sample' instance.
 */
#define PW_MAX_DEVICES_PER_SAMPLE 2
/*
 * MAX number of mappings per block.
 */
#define PW_MAX_NUM_IRQ_MAPPINGS_PER_BLOCK 16
/*
 * MAX size of each irq name (bytes).
 */
#define PW_MAX_IRQ_NAME_SIZE 32
/*
 * Max # of available frequencies.
 */
#define PW_MAX_NUM_AVAILABLE_FREQUENCIES 16 // should be enough!
/*
 * MAX number of mappings per block.
 */
#define PW_MAX_NUM_PROC_MAPPINGS_PER_BLOCK 32
/*
 * MAX length of metadata names;
 */
#define PW_MAX_METADATA_NAME 80
/*
 * MAX number of GPU frequencies coded per *meta-data* sample; used for fixed-length samples only!
 */
#define PW_MAX_FREQS_PER_META_SAMPLE 54
/*
 * MAX number of C-state MSRs per C multi-msg
 */
#define PW_MAX_C_STATE_MSRS_PER_MESSAGE 6
/*
 * MAX number of C-state MSRs per fixed-size C meta sample
 */
#define PW_MAX_MSRS_PER_META_SAMPLE 9
/*
 * MAX length of C-state MSR name
 */
#define PW_MAX_C_MSR_NAME 6


#define PW_MAX_ELEMENTS_PER_BW_COMPONENT 12

/*
 * MSR counter stuff.
 *
 * Ultimately the list of MSRs to read (and the core MSR residency addresses)
 * will be specified by the "runss" tool (via the "PW_IOCTL_CONFIG" ioctl).
 *
 * For now, hardcoded to values for NHM.
 */
typedef enum {
    MPERF = 0, // C0
    APERF = 1, // C1
    C2 = 2,
    C3 = 3,
    C4 = 4,
    C5 = 5,
    C6 = 6,
    C7 = 7,
    C8 = 8,
    C9 = 9,
    C10 = 10,
    /* C11 = 11, */
    MAX_MSR_ADDRESSES
} c_state_t;

/*
 * Enumeration of possible sample types.
 */
typedef enum {
    FREE_SAMPLE = 0, /* Used (internally) to indicate a FREE entry */
    C_STATE = 1, /* Used for c-state samples */
    P_STATE = 2, /* Used for p-state samples */
    K_CALL_STACK = 3, /* Used for kernel-space call trace entries */
    M_MAP = 4, /* Used for module map info samples */
    IRQ_MAP = 5, /* Used for IRQ # <-> DEV name mapping samples */
    PROC_MAP = 6, /* Used for PID <-> PROC name mapping samples */
    S_RESIDENCY = 7, /* Used for S residency counter samples */
    S_STATE = 8, /* Used for S state samples */
    D_RESIDENCY = 9, /* Used for D residency counter samples */
    D_STATE = 10, /* Used for D state samples in north or south complex */
    TIMER_SAMPLE = 11,
    IRQ_SAMPLE = 12,
    WORKQUEUE_SAMPLE = 13,
    SCHED_SAMPLE = 14,
    IPI_SAMPLE = 15,
    TPE_SAMPLE = 16, /*  Used for 'trace_power_end' samples */
    W_STATE = 17, /* Used for kernel wakelock samples */
    DEV_MAP = 18, /* Used for NC and SC device # <-> DEV name mapping samples */
    C_STATE_MSR_SET = 19, /* Used to send an initial snapshot of the various C-state MSRs */
    U_STATE = 20, /* Used for user wakelock samples */
    TSC_POSIX_MONO_SYNC = 21, /* Used to sync TSC <-> posix CLOCK_MONOTONIC timers; REQUIRED for AXE support */ 
    CONSTANT_POOL_ENTRY = 22, /* Used to send constant pool information */
    PKG_MAP = 23, /* Used to send UID and package name mappings used for Android */
    CPUHOTPLUG_SAMPLE = 24, /* Used to note when a CPU goes online or offline in ARM systems */
    C_MULTI_MSG = 25, /* Used for c-state samples when multiple C-state MSRs have counted */
    THERMAL = 26, // Needs Meta Data???
    POWER_METER = 27, // Needs Meta Data
    GRAPHICS = 28, // Needs Meta Data + Subtypes (GFX_C_STATE, GFX_P_STATE)
    BANDWIDTH = 29, // Needs Meta Data + Subtypes(???)
    MODEM = 30, // Needs Meta Data
    U_CALL_STACK = 31, // Needs subtypes for Windows
    TIMER_RESOLUTION = 32, // Needs input from Windows team
    SYSTEM = 33, // ??? for meta data only
    META_DATA = 34, // Needs subtypes
    SUMMARY = 35, /* For summary and timeline trace samples; will have sub types */
    ACPI_S3 = 36, /* Used for ACPI S3 residency counter samples */
    GPUFREQ = 37, /* Used for GPU P-state metadata */
    THERMAL_COMP = 38, /* HACK! (GEH) Used for thermal component metadata for fixed-length samples only */
    BANDWIDTH_COMP = 39, /* HACK! (GEH) Used for bandwidth component/pathway metadata for fixed-length samples only */
    GPU_P_STATE = 40, /* HACK! (GEH) Used for GPU P-state for fixed-length samples only (temporary) */
    GPU_C_STATE = 41, /* Used for GPU C-state samples */
    FPS = 42, /* Used for FPS samples */
    DRAM_SELF_REFRESH = 43, /* Used for DRAM Self Refresh residency */
    DRAM_SELF_REFRESH_COMP = 44, /* Used for DRAM Self Refresh residency metadata for fixed-length samples only */
    S_RESIDENCY_STATES = 45, /* Used for S residency metadata for fixed-length samples only */
    MATRIX_MSG = 46, /* Used for Matrix messages */
    BANDWIDTH_ALL_APPROX = 47, /* Used for T-unit B/W messages */
    C_STATE_META = 48, /* HACK! Used for CPU C-state metadata for fixed-length samples only! (temporary) */
    GPU_C_STATE_META = 49, /* HACK! Used for GPU C-state metadata for fixed-length samples only! (temporary) */
    BANDWIDTH_MULTI = 50,
    BANDWIDTH_MULTI_META = 51, /* HACK! Used for elements of a BW compenent metadata for fixed-length samples only! (temporary) */
   SAMPLE_TYPE_END
} sample_type_t;
#define FOR_EACH_SAMPLE_TYPE(idx) for ( idx = C_STATE; idx < SAMPLE_TYPE_END; ++idx )


/*
 * Enumeration of possible C-state sample
 * types.
 */

typedef enum{
    PW_BREAK_TYPE_I = 0, // interrupt
    PW_BREAK_TYPE_T = 1, // timer
    PW_BREAK_TYPE_S = 2, // sched-switch
    PW_BREAK_TYPE_IPI = 3, // (LOC, RES, CALL, TLB)
    PW_BREAK_TYPE_W = 4, // workqueue
    PW_BREAK_TYPE_B = 5, // boundary
    PW_BREAK_TYPE_N = 6, // Not-a-break: used exclusively for CLTP support: DEBUGGING ONLY!
    PW_BREAK_TYPE_A = 7, // Abort
    PW_BREAK_TYPE_U = 8,  // unknown
    PW_BREAK_TYPE_END = 9 // EOF
}c_break_type_t;
#define FOR_EACH_WAKEUP_TYPE(idx) for ( idx = PW_BREAK_TYPE_I; idx < PW_BREAK_TYPE_END; ++idx )
/*
 * 's_wake_type_names' and 's_wake_type_long_names' are used mainly in Ring-3 debugging.
 */
#ifndef __KERNEL__
    static const char *s_wake_type_names[] = {"IRQ", "TIM", "SCHED", "IPI", "WRQ", "BDRY", "NONE", "ABRT", "UNK", "EOF"};
    static const char *s_wake_type_long_names[] = {"IRQ", "TIMER", "SCHEDULER", "IPI", "WORK QUEUE", "BOUNDARY", "NONE", "ABORT", "UNKNOWN", "EOF"};
#endif // __KERNEL__

#pragma pack(push) /* Store current alignment */
#pragma pack(2) /* Set new alignment -- 2 byte boundaries */
/*
 * MSRs may be "Thread" MSRs, "Core" MSRs, "Module" MSRs, "Package" MSRs or "GPU" MSRs.
 * (Examples: PC2 on Saltwell is a "Package" MSR, MC4 on SLM is a "Module" MSR)
 */
typedef enum pw_msr_type {
    PW_MSR_THREAD = 0,
    PW_MSR_CORE = 1,
    PW_MSR_MODULE = 2,
    PW_MSR_PACKAGE = 3,
    PW_MSR_GPU = 4
} pw_msr_type_t;
/*
 * Names corresponding to C-state MSR types.
 * Ring-3 Debugging ONLY!
 */
#ifndef __KERNEL__
    static const char *s_pw_msr_type_names[] = {"Thread", "Core", "Module", "Package", "GPU"};
#endif // __KERNEL__
/*
 * Specifier for GPU C-states.
 */
typedef enum pw_gpu_msr_subtype {
    PW_MSR_GPU_RENDER = 0,
    PW_MSR_GPU_MEDIA = 1
} pw_gpu_msr_subtype_t;
/*
 * Names corresponding to GPU C-state subtypes.
 * Ring-3 Debugging ONLY!
 */
#ifndef __KERNEL__
    static const char *s_pw_gpu_msr_subtype_names[] = {"RENDER", "MEDIA"};
#endif // __KERNEL__

/*
 * MSR specifiers
 */
typedef struct pw_msr_identifier {
    u8 subtype:4; // For "PC6" v "PC6C" differentiation etc.
    u8 type:4; // One of 'pw_msr_type_t'
    u8 depth; // Actual MSR number e.g. "CC2" will have a "2" here
} pw_msr_identifier_t;
/*
 * Struct to encode MSR addresses.
 */
typedef struct pw_msr_addr {
    pw_msr_identifier_t id; // MSR identifier
    u32 addr; // MSR address
} pw_msr_addr_t;
/*
 * Struct to encode MSR values.
 */
typedef struct pw_msr_val {
    pw_msr_identifier_t id; // MSR identifier
    u64 val; // MSR value
} pw_msr_val_t;
/*
 * Structure used by Ring-3 to tell the power driver which
 * MSRs to read.
 */
typedef struct pw_msr_info {
    u16 num_msr_addrs; // The number of MSR addresses pointed to by the 'data' field
    char data[1]; // The list of 'pw_msr_addr_t' instances to read.
} pw_msr_info_t;

#define PW_MSR_INFO_HEADER_SIZE() ( sizeof(pw_msr_info_t) - sizeof(char[1]) )

// #pragma pack(pop) /* Restore previous alignment */

/*
 * A c-state msg.
 */
#if 0
#pragma pack(push) /* Store current alignment */
#pragma pack(2) /* Set new alignment -- 2 byte boundaries */
#endif
/*
 * GU: moved "c_msg" to "c_multi_msg". Replaced with older version of "c_msg".
 * This is temporary ONLY!
 */
#if 0
typedef struct c_msg {
    u64 mperf;
    u64 wakeup_tsc; // The TSC when the wakeup event was handled.
    u64 wakeup_data; // Domain-specific wakeup data. Corresponds to "c_data" under old scheme.
    s32 wakeup_pid;
    s32 wakeup_tid;
    u32 tps_epoch;
    /*
     * In cases of timer-related wakeups, encode the CPU on which the timer was
     * initialized (and whose init TSC is encoded in the 'wakeup_data' field).
     */
    s16 timer_init_cpu;
    u8 wakeup_type; // instance of 'c_break_type_t'
    u8 req_state; // State requested by OS: "HINT" parameter passed to TPS probe
    u8 num_msrs; // the number of 'pw_msr_val_t' instances encoded in the 'data' field below
    u8 data[1]; // array of 'num_msrs' 'pw_msr_val_t' instances
} c_msg_t;
#endif
typedef struct c_msg {
    pw_msr_val_t cx_msr_val; // The value read from the C-state MSR at the wakeup point.
    u64 mperf;               // The value read from the C0 MSR 
    u64 wakeup_tsc;          // The TSC when the wakeup event was handled.
    /*
     * Domain-specific wakeup data. Corresponds to "c_data" under c_message_t fixed-length scheme.
     * Meaning is as follows for the following wakeup_types:
     *    PW_BREAK_TYPE_I: IRQ number (used as index to get IRQ name using i_message_t)
     *    PW_BREAK_TYPE_T: System call CPU TSC
     *    All other types: field is ignored.
     */
    u64 wakeup_data;
    /*
     * 's32' for wakeup_{pid,tid} is overkill: '/proc/sys/kernel/pid_max' is almost always
     * 32768, which will fit in 's16'. However, it IS user-configurable, so we must 
     * accomodate larger pids.
     */
    s32 wakeup_pid;
    s32 wakeup_tid;
    u32 tps_epoch; // Only used before post-processing
    /*
     * In cases of timer-related wakeups, encode the CPU on which the timer was
     * initialized (and whose init TSC is encoded in the 'wakeup_data' field).
     */
    s16 timer_init_cpu; // Only used before post-processing
    // pw_msr_identifier_t act_state_id;
    u8 wakeup_type; // instance of 'c_break_type_t'
    u8 req_state; // State requested by OS: "HINT" parameter passed to TPS probe. Only used before post-processing
    // u8 act_state; // State granted by hardware: the MSR that counted, and whose residency is encoded in the 'cx_res' field
} c_msg_t;

typedef struct c_multi_msg {
    u64 mperf;
    u64 wakeup_tsc; // The TSC when the wakeup event was handled.
    u64 wakeup_data; // Domain-specific wakeup data. Corresponds to "c_data" under old scheme.
    s32 wakeup_pid;
    s32 wakeup_tid;
    u32 tps_epoch;
    /*
     * In cases of timer-related wakeups, encode the CPU on which the timer was
     * initialized (and whose init TSC is encoded in the 'wakeup_data' field).
     */
    s16 timer_init_cpu;
    u8 wakeup_type; // instance of 'c_break_type_t'
    u8 req_state; // State requested by OS: "HINT" parameter passed to TPS probe
    u8 num_msrs; // the number of 'pw_msr_val_t' instances encoded in the 'data' field below
    u8 data[1]; // array of 'num_msrs' 'pw_msr_val_t' instances
} c_multi_msg_t;

#define C_MULTI_MSG_HEADER_SIZE() ( sizeof(c_multi_msg_t) - sizeof(char[1]) )



/*
 * A p-state sample: MUST be 54 bytes EXACTLY!!!
 */
typedef struct p_msg {
    /*
     * The frequency the OS requested during the LAST TPF, in KHz.
     */
    u32 prev_req_frequency;
    /*
     * The value of the IA32_PERF_STATUS register: multiply bits 12:8 (Atom) or 15:0 (big-core)
     * with the BUS clock frequency to get actual frequency the HW was executing at.
     */
    u16 perf_status_val;
    /*
     * We encode the frequency at the start and end of a collection in 'boundary'
     * messages. This flag is set for such messages. Only used before post-processing..
     */
    u16 is_boundary_sample;      // Only lsb is used for the flag, which when true indicates a 
                                 // begin or end boundary sample for the collection. These are generated
                                 // during post-processing.
    union {
        u64 unhalted_core_value; // The APERF value. Used only before post-processing.
        u32 frequency;           // The actual measured frequency in KHz. 
                                 // Used only for post-processed samples.
    };
    union {
        u64 unhalted_ref_value;  // The MPERF value. Used only before post-processing..
        u32 cx_time_rate;        // The fraction of time since the previous p_msg sample spent in a non-C0 state.
                                 // This value ranges from 0 (0% Cx time) to 1e9 (100% Cx time).
                                 // Used only for post-processed samples.
    };
} p_msg_t;

/*
 * The 'type' of the associated
 * 'u_sample'.
 */
typedef enum{
    PW_WAKE_ACQUIRE = 0, // Wake lock
    PW_WAKE_RELEASE = 1 // Wake unlock
}u_sample_type_t;

/*
 * The 'type' of the associated
 * 'u_sample'.
 */
typedef enum{
    PW_WAKE_PARTIAL = 0, // PARTIAL_WAKE_LOCK 
    PW_WAKE_FULL = 1, // FULL_WAKE_LOCK
    PW_WAKE_SCREEN_DIM = 2, // SCREEN_DIM_WAKE_LOCK
    PW_WAKE_SCREEN_BRIGHT = 3, // SCREEN_BRIGHT_WAKE_LOCK
    PW_WAKE_PROXIMITY_SCREEN_OFF = 4  // PROXIMITY_SCREEN_OFF_WAKE_LOCK
}u_sample_flag_t;

/*
 * Wakelock sample
 */
typedef struct{
    u_sample_type_t type;   // Either WAKE_ACQUIRE or WAKE_RELEASE 
    u_sample_flag_t flag;   // Wakelock flag
    pid_t pid, uid;
    u32 count;
    char tag[PW_MAX_WAKELOCK_NAME_SIZE]; // Wakelock tag
}u_sample_t;

/*
 * Generic "event" sample.
 */
typedef struct event_sample {
    u64 data[6];
} event_sample_t;

/*
 * The 'type' of the associated
 * 'd_residency_sample'.
 */
typedef enum {
    PW_NORTH_COMPLEX = 0,  // North complex
    PW_SOUTH_COMPLEX = 1,  // South complex
    PW_NOT_APPLICABLE = 2  // Not applicable
} device_type_t;

/*
 * Convenience for a 'string' data type.
 * Not strictly required.
 */
typedef struct pw_string_type pw_string_type_t;
struct pw_string_type {
    u16 len;
    // char data[1];
    char *data;
};
// TODO: ALL pointers need to be converted to "u64"!!!

/*
 * Meta data used to describe S-state residency.
 */
typedef struct s_res_meta_data s_res_meta_data_t;
struct s_res_meta_data {
    u8 num_states;           // The number of states available including S3.
    pw_string_type_t *names; // The list of state names e.g. S0i0, S0i1, S0i2, S0i3, S3 ...
                             // The order must be same as the order of values stored in residencies
};
#define S_RES_META_MSG_HEADER_SIZE (sizeof(s_res_meta_data_t) - sizeof(pw_string_type_t *))

/*
 * Platform state (a.k.a. S-state) residency counter sample
 */
typedef struct s_res_msg {
    u64 *residencies;  // Residencies in time (in unit of TSC ticks) for each state
                       // Array size is determined by num_states (defined in metadata)
                       // MUST be last entry in struct!
} s_res_msg_t;
#define S_RES_MSG_HEADER_SIZE (sizeof(s_res_msg_t) - sizeof(u64 *))

/*
 * Meta data used to describe D-state sample or residency
 * Device names are generated in dev_sample structure
 */
typedef struct d_state_meta_data d_state_meta_data_t;
struct d_state_meta_data {
    device_type_t dev_type;  // one of "device_type_t". Different complex may have different device states available.
    u8 num_states;           // The total number of states available
    pw_string_type_t *names; // The list of state names e.g. D0i0_AON, D0i0_ACG, D0i1, D0i3, D3_hot ...
                             // The order must be same as the order of values stored in residencies
};
#define D_STATE_META_MSG_HEADER_SIZE (sizeof(d_state_meta_data_t) - sizeof(pw_string_type_t *))

/*
 * Structure to return Dev # <-> Dev Name mappings.
 */
typedef struct dev_map_msg dev_map_msg_t;
struct dev_map_msg {
    u16 dev_num;  // Device ID
    u16 dev_type; // one of "device_type_t"
                            // The pair (dev_num, dev_type) is a unique ID for each device
    pw_string_type_t dev_short_name;
    pw_string_type_t dev_long_name;
};

typedef struct dev_map_meta_data dev_map_meta_data_t;
struct dev_map_meta_data {
    u16 num_devices;                // The number of 'dev_map_msg_t' instances in the 'device_mappings' array, below
    dev_map_msg_t *device_mappings; // A mapping of dev num <-> dev names; size is governed by 'num_devices'
};
#define DEV_MAP_MSG_META_MSG_HEADER_SIZE (sizeof(dev_map_meta_data_t) - sizeof(dev_map_msg_t *))

/*
 * 'D-state' information. Used for both residency and state samples.
 */
typedef struct d_state_msg d_state_msg_t;
typedef struct d_state_msg d_res_msg_t;
struct d_state_msg {
    u16 num_devices;        // Number of devices profiled (a subset of devices can be monitored)
    device_type_t dev_type; // one of "device_type_t"
    u16 *deviceIDs;         // Array of Device IDs profiled. Array size is determined by num_devices
    u64 *values;            // Array of Device residencies or states depending on sample type (sample_type_t). 
                            // If the sample type is D_RESIDENCY, 
                            // Array size is determined by num_devices * num_states (defined in metadata)
                            // Array have values in the following order in the unit of TSC ticks
                            // {D0 in Dev0, D1 in Dev0, D2 in Dev0, D0 in Dev1, D1 in Dev1 ...}
                            // Or the other way? Which one is better?
                            // If the sample type is D_STATE, 
                            // Array size is determined by num_devices * log(num_states) / 64 
};
#define D_STATE_MSG_HEADER_SIZE ( sizeof(d_state_msg_t) - sizeof(u16 *) - sizeof(u64 *) )
#define D_RES_MSG_HEADER_SIZE ( sizeof(d_res_msg_t) - sizeof(u16 *) - sizeof(u64 *) )

/*
 * The 'unit' of thermal data.
 */
typedef enum {
    PW_FAHRENHEIT = 0, 
    PW_CELCIUS = 1 
} thermal_unit_t;

/*
 * Meta data used to describe Thermal-states.
 */
typedef struct thermal_meta_data thermal_meta_data_t;
struct thermal_meta_data {
    u16 num_components;           // The number of components.
    thermal_unit_t thermal_unit;  // 0 is Fahrenheit and 1 is Celcius.
    pw_string_type_t *names;      // Names of components like Core, Skin, MSIC Die, SOC, ...
};
#define THERMAL_META_MSG_HEADER_SIZE (sizeof(thermal_meta_data_t) - sizeof(pw_string_type_t *))

/*
 * Thermal state sample
 */
typedef struct thermal_msg {
    u16 index;         // Array index to components defined in thermal_meta_data. Index must be [0, num_components)
    u16 temperatures;  // Thermal value in the unit defined in thermal_unit
} thermal_msg_t;

/*
 * Meta data used to describe GPU Frequency.
 */
typedef struct gpufreq_meta_data gpufreq_meta_data_t;
struct gpufreq_meta_data {
    u16 num_available_freqs; // Number of available gpu frequencies.
    u16 *available_freqs;    // List of all available frequncies. Max Length will be equal to the num_available_freq.
                             // The unit of frequency here is Mhz.
};
#define GPUFREQ_META_MSG_HEADER_SIZE (sizeof(gpufreq_meta_data_t) - sizeof(u16 *))

/*
 * GPU Frequency state sample
 */
typedef struct gpufreq_msg {
    u16 gpufrequency; // GPU frequency is stored here. Unit is MHz
} gpufreq_msg_t;

/*
 * Meta data used to describe Power-states.
 */
typedef struct power_meta_data power_meta_data_t;
struct power_meta_data {
    u16 num_components;      // The number of components.
    pw_string_type_t *names; // Names of components like IA Pkg, Gfx, SOC, ...
};

/*
 * Power state sample
 */
typedef struct power_msg {
    u16 index;    // Array index to components defined in power_meta_data. index must be [0, num_components)
    u32 currnt;   // Assume the unit is uA: GU: changed name from "current" to "currnt" to get the driver to compile
    u32 voltage;  // Assume the unit is uV
    u32 power;    // Assume the unit is uW
} power_msg_t;

/*
 * Meta data used to describe bandwidths.
 */
typedef struct bw_meta_data {
    u16 num_components;      // The number of components.
    pw_string_type_t *names; // Names of components like Core to DDR0, Core to DDR1, ISP, GFX, IO, DISPLAY ...
} bw_meta_data_t;
#define BANDWIDTH_META_MSG_HEADER_SIZE (sizeof(bw_meta_data_t) - sizeof(pw_string_type_t *))

/*
 * Bandwidth sample
 */
typedef struct bw_msg {
    u16 index;         // Array index to components defined in bw_meta_data. 
                       // Index must be [0, num_components)
    u64 read32_bytes;  // Total number of READ32 bytes for duration
    u64 write32_bytes; // Total number of WRITE32 bytes for duration   
    u64 read64_bytes;  // Total number of READ64 bytes for duration
    u64 write64_bytes; // Total number of WRITE64 bytes for duration   
    u64 duration;      // The unit should be TSC ticks.
} bw_msg_t;


typedef struct bw_multi_meta_data bw_multi_meta_data_t;
struct bw_multi_meta_data {
    u16 index;               // Array index to components defined in bw_meta_data. 
                             // Index must be [0, num_components)
                             // Currently, this is ALWAYS ZERO (because only one VISA metric can be collected at a time).
    u16 num_names;           // Size of 'names' array, below
    pw_string_type_t *names; // Individual names for each element in 'bw_multi_msg->data' e.g. "Read32", "WritePartial" "DDR-0 Rank-0 Read64"
};
#define BW_MULTI_META_MSG_HEADER_SIZE (sizeof(bw_multi_meta_data_t) - sizeof(pw_string_type_t *))

typedef struct bw_multi_msg bw_multi_msg_t;
struct bw_multi_msg {
    u16 index;          // Array index to components defined in bw_meta_data. 
                        // Index must be [0, num_components)
                        // Currently, this is ALWAYS ZERO (because only one VISA metric can be collected at a time).
    u16 num_data_elems; // Size of 'data' array, below
    u64 duration;       // In TSC ticks
    u64 p_data;          // Size of array == 'bw_multi_msg->num_data_elems' == 'bw_multi_meta_data->num_names'.
};
#define BW_MULTI_MSG_HEADER_SIZE() (sizeof(bw_multi_msg_t) - sizeof(u64))

typedef struct bw_multi_sample bw_multi_sample_t;
struct bw_multi_sample {
    u16 index;          // Array index to components defined in bw_meta_data. 
                        // Index must be [0, num_components)
                        // Currently, this is ALWAYS ZERO (because only one VISA metric can be collected at a time).
    u16 num_data_elems; // Size of 'data' array, below
    u64 duration;       // In TSC ticks
    u64 data[PW_MAX_ELEMENTS_PER_BW_COMPONENT];         // Size of array == 'bw_multi_meta_data->num_names'.
};


/*
 * Meta data used to describe FPS
 */
typedef struct fps_meta_data fps_meta_data_t;
struct fps_meta_data {
    u16 num_components; // The number of components including frames
    pw_string_type_t *names; // Names of components like FPS
};
#define FPS_META_MSG_HEADER_SIZE (sizeof(fps_meta_data_t) - sizeof(pw_string_type_t *))
 
/*
 * FPS sample
 */
typedef struct fps_msg {
    u32 frames;
} fps_msg_t;


typedef struct dram_srr_meta_data dram_srr_meta_data_t;
struct dram_srr_meta_data {
    u16 num_components;      // The number of components.
    pw_string_type_t *names; // Names of components like DUNIT0, DUNIT1...
};
#define DRAM_SRR_META_MSG_HEADER_SIZE (sizeof(dram_srr_meta_data_t) - sizeof(pw_string_type_t *))

typedef struct dram_srr_msg {
    u16 num_components;       // The number of components.
    u64 duration;             // The unit should be TSC ticks.
    u64 *residency_cpu_ticks; // Residency in terms of CPU clock ticks i.e. TSC
                              // Number of elements in array must be equal to num_components in meta data
                              // This field is for VTune visualization.    
    u64 *residency_soc_ticks; // Residency in terms of SOC clock ticks
                              // Number of elements in array must be equal to num_components in meta data
} dram_srr_msg_t;

/*
 * Kernel wakelock information.
 */
typedef struct constant_pool_msg {
    u16 entry_type; // one of 'W_STATE' for kernel mapping or 'U_STATE' for userspace mapping
    u16 entry_len;
    /*
     * We need to differentiate between the two types of 'W_STATE' constant-pool entries:
     * 1. Entries generated in Ring-3 (as a result of parsing the "/proc/wakelocks" file). These are generated at
     *    the START of a collection and have a 'w_sample_type_t' value of 'PW_WAKE_LOCK_INITAL'.
     * 2. Entries generated in Ring-0 DURING the collection.
     * All examples of (1) will have the MSB set to '1'. Examples of (2) will not be bitmasked in any way.
     */
    u32 entry_index;
    char entry[1]; // MUST be LAST entry in struct!
} constant_pool_msg_t;
#define PW_CONSTANT_POOL_MSG_HEADER_SIZE (sizeof(constant_pool_msg_t) - sizeof(char[1]))
#define PW_CONSTANT_POOL_INIT_ENTRY_MASK (1U << 31)
#define PW_SET_INITIAL_W_STATE_MAPPING_MASK(idx) ( (idx) | PW_CONSTANT_POOL_INIT_ENTRY_MASK )
#define PW_HAS_INITIAL_W_STATE_MAPPING_MASK(idx) ( (idx) & PW_CONSTANT_POOL_INIT_ENTRY_MASK ) /* MSB will be SET if 'PW_WAKE_LOCK_INITIAL' mapping */
#define PW_STRIP_INITIAL_W_STATE_MAPPING_MASK(idx) ( (idx) & ~PW_CONSTANT_POOL_INIT_ENTRY_MASK )

typedef struct w_wakelock_msg {
    u16 type; // one of 'w_sample_type_t'
    pid_t tid, pid;
    u32 constant_pool_index;
    u64 expires;
    char proc_name[PW_MAX_PROC_NAME_SIZE];
} w_wakelock_msg_t;

typedef struct u_wakelock_msg {
    u16 type; // One of 'u_sample_type_t'
    u16 flag; // One of 'u_sample_flag_t'
    pid_t pid, uid;
    u32 count;
    u32 constant_pool_index;
} u_wakelock_msg_t;

typedef struct i_sample i_msg_t;

typedef struct r_sample r_msg_t;

/*
 * TSC_POSIX_MONO_SYNC
 * TSC <-> Posix clock_gettime() sync messages.
 */
typedef struct tsc_posix_sync_msg {
    pw_u64_t tsc_val;
    pw_u64_t posix_mono_val;
} tsc_posix_sync_msg_t;

/*
 * Temp struct to hold C_STATE_MSR_SET samples.
 * Required only until we move away from using PWCollector_sample
 * instances to using PWCollector_msg instances in the power lib.
 * *********************************************************************
 * RESTRICTIONS: struct size MUST be LESS THAN or EQUAL to 112 bytes!!!
 * *********************************************************************
 */
typedef struct tmp_c_state_msr_set_sample {
    u16 num_msrs;
    pw_msr_val_t msr_vals[11]; // Each 'pw_msr_val_t' instance is 10 bytes wide.
} tmp_c_state_msr_set_sample_t;

/*
 * Information on the specific TYPE of a matrix message.
 */
typedef enum pw_mt_msg_type {
    PW_MG_MSG_NONE=0,
    PW_MT_MSG_INIT=1,
    PW_MT_MSG_POLL=2,
    PW_MT_MSG_TERM=3,
    PW_MT_MSG_END=4
} pw_mt_msg_type_t;
/*
 * Ring-3 Debugging: names for the above msg types.
 */
#ifndef __KERNEL__
    static const char *s_pw_mt_msg_type_names[] = {"NONE", "INIT", "POLL", "TERM", "END"};
#endif // __KERNEL__
/*
 * Encode information returned by the matrix driver.
 * Msg type == 'MATRIX_MSG'
 */
typedef struct pw_mt_msg pw_mt_msg_t;
struct pw_mt_msg {
    u16 data_type; // One of 'pw_mt_msg_type_t'
    u16 data_len;
    u64 timestamp;
    u64 p_data;
};
#define PW_MT_MSG_HEADER_SIZE() ( sizeof(pw_mt_msg_t) - sizeof(u64) )

/*
 * Summary structs: structs used for summary and trace timeline information.
 */
typedef struct pw_c_state_wakeup_info pw_c_state_wakeup_info_t;
struct pw_c_state_wakeup_info {
    pw_u16_t wakeup_type; // One of 'c_break_type_t'
    pw_s32_t wakeup_data; // Proc PID if wakeup_type == PW_BREAK_TYPE_T
                          // IRQ # if wakeup_type == PW_BREAK_TYPE_I
                          // Undefined otherwise
    pw_u32_t wakeup_count; // Number of times this timer/irq/other has woken up the system from the specified C-state
    pw_string_type_t wakeup_name; // Proc Name if wakeup_type == PW_BREAK_TYPE_T
                                  // Device # if wakeup_type == PW_BREAK_TYPE_I
                                  // Undefined otherwise
};

typedef struct c_state_summary_msg c_state_summary_msg_t;
struct c_state_summary_msg {
    float res_percent;
    pw_u32_t abort_count;
    pw_u32_t promotion_count;
    pw_u32_t wakeup_count; // The TOTAL number of wakeups for the given node and this C-state
    pw_msr_identifier_t id;
    pw_u16_t num_wakeup_infos; // The number of elements in the 'wakeup_infos' array, below
    pw_c_state_wakeup_info_t *wakeup_infos;
};

typedef struct c_node_summary_msg c_node_summary_msg_t;
struct c_node_summary_msg {
    pw_msr_type_t node_type; // Thread/Core/Module/Package
    pw_u16_t node_id;
    pw_u16_t num_c_states; // The number of elements in the 'c_states' array, below
    c_state_summary_msg_t *c_states;
};

typedef struct c_summary_msg c_summary_msg_t;
struct c_summary_msg {
    pw_u16_t num_c_nodes; // The number of elements in the 'c_nodes' array, below
    c_node_summary_msg_t *c_nodes;
};

typedef struct p_state_summary_msg p_state_summary_msg_t;
struct p_state_summary_msg {
    pw_u16_t freq_mhz; // The frequency, in MHz, whose residency rate is encoded in 'res_rate', below
    pw_u16_t res_rate; // The residency rate, obtained by multiplying the residency fraction by 1e4 i.e. 100% == 10000, 99.99% == 9999 etc.
};
/*
 * Macros to encode and decode residency rates. Used in
 * P-state "summary" structures.
 * Update: and also in S-residency and ACPI S3 "summary" structures.
 */
#define ENCODE_RES_RATE(r) (pw_u16_t)( (r) * 1e4 )
#define DECODE_RES_RATE(r) ( (float)(r) / 1e4 )


typedef struct p_node_summary_msg p_node_summary_msg_t;
struct p_node_summary_msg {
    pw_msr_type_t node_type; // Thread/Core/Module/Package
    pw_u16_t node_id;
    pw_u16_t num_p_states; // The number of elements in the 'p_states' array, below
    p_state_summary_msg_t *p_states;
};

typedef struct p_summary_msg p_summary_msg_t;
struct p_summary_msg {
    pw_u16_t num_p_nodes; // The number of elements in the 'p_nodes' array, below.
    p_node_summary_msg_t *p_nodes;
};

/*
 * Information on a single wakelock.
 */
typedef struct wlock_info wlock_info_t;
struct wlock_info {
    double total_lock_time_msecs; // double is GUARANTEED to be 64bits/8bytes
    pw_u32_t num_times_locked;
    // pw_u16_t lock_type; // One of 'W_STATE' for KERNEL wakelocks or 'U_STATE' for USER wakelocks
    pw_string_type_t name; // 'lock_type' == 'W_STATE' ==> Kernel wakelock name
                           // 'lock_type' == 'U_STATE' ==> Wakelock tag
};

/*
 * Information for all wakelocks.
 */
typedef struct wlock_summary_msg wlock_summary_msg_t;
struct wlock_summary_msg {
    pw_u32_t num_wlocks; // The number of elements in the 'wlocks' array, below
    wlock_info_t *wlocks; // The list of kernel or user wakelocks
};
#define WLOCK_SUMMARY_MSG_HEADER_SIZE (sizeof(wlock_summary_msg_t) - sizeof(wlock_info_t *))

#if 0
/*
 * Stub for kernel wakelock information.
 */
typedef struct kernel_wlock_map_summary_msg kernel_wlock_map_summary_msg_t;
struct kernel_wlock_map_summary_msg {
    // TODO
};
/*
 * Stub for user wakelock information.
 */
typedef struct user_wlock_map_summary_msg user_wlock_map_summary_msg_t;
struct user_wlock_map_summary_msg {
    // TODO
};

typedef struct wlock_map_summary_msg wlock_map_summary_msg_t;
struct wlock_map_summary_msg {
    pw_u16_t lock_type; // One of 'W_STATE' (for Kernel) or 'U_STATE' (for User) wakelocks
    pw_string_type_t lock_name; // Name of the wakelock
    pw_string_type_t proc_name; // Name of process taking/releasing the wakelock
    void *data; // If 'lock_type' == 'W_STATE' then ptr to 'kernel_wlock_map_summary_msg'
                // If 'lock_type' == 'U_STATE' then ptr to 'user_wlock_map_summary_msg'
};
#define WLOCK_MAP_SUMMARY_MSG_HEADER_SIZE (sizeof(wlock_map_summary_msg) - sizeof(void *)) 

typedef struct wlock_summary_msg wlock_summary_msg_t;
struct wlock_summary_msg {
    pw_u64_t lock_time_tscs; // Total time (in TSC ticks) when ANY wakelock was taken.
    pw_u16_t num_wlock_maps; // Number of instances in the 'maps' array, below
    wlock_map_summary_msg_t *maps; // Mappings for each wakelock that was taken/released in this interval
};
#define WLOCK_SUMMARY_MSG_HEADER_SIZE (sizeof(wlock_summary_msg) - sizeof(wlock_map_summary_msg_t *))
#endif

typedef struct thermal_node_summary_msg thermal_node_summary_msg_t;
struct thermal_node_summary_msg {
    pw_u16_t unit; // An instance of thermal_unit_t
    pw_u16_t index; // Array index to components defined in thermal_meta_data. Index must be [0, num_components)
    pw_u16_t min_temp, max_temp;
    float avg_temp;
};

typedef struct thermal_summary_msg thermal_summary_msg_t;
struct thermal_summary_msg {
    pw_u16_t num_thermal_nodes;
    thermal_node_summary_msg_t *thermal_nodes;
};


typedef struct gpu_p_state_summary_msg gpu_p_state_summary_msg_t;
struct gpu_p_state_summary_msg {
    pw_u16_t freq_mhz; // The frequency, in MHz, whose residency rate is encoded in 'res_rate', below
    pw_u16_t res_rate; // The residency rate, obtained by multiplying the residency fraction by 1e4 i.e. 100% == 10000, 99.99% == 9999 etc.
};

typedef struct gpu_p_summary_msg gpu_p_summary_msg_t;
struct gpu_p_summary_msg {
    pw_u16_t num_p_states; // The number of elements in the 'p_states' array, below
    gpu_p_state_summary_msg_t  *gpu_p_states;
};

/*
 * Bandwidth summaries are EXACTLY the same as regular 'BW' messages.
 */
typedef bw_msg_t bw_summary_msg_t;


typedef struct s_res_summary_msg s_res_summary_msg_t;
struct s_res_summary_msg {
    pw_u16_t num_states; // The number of elements in the 'res_rates' array below. MUST be same as 'num_states' in 's_res_meta_data'!
    pw_u16_t *res_rates; // The residency rate, obtained by multiplying the residency fraction by 1e4 i.e. 100% == 10000, 99.99% == 9999 etc.
};

typedef struct summary_msg summary_msg_t;
struct summary_msg {
    pw_u64_t start_tsc, stop_tsc; // The start and stop of the interval being summarized.
    pw_u16_t data_type; // The type of the payload; one of 'sample_type_t'
    pw_u16_t data_len; // The size of the payload
    void *p_data; // Pointer to the payload
};

/*
 * Meta-data specifiers, data structures etc.
 */

/*
 * Meta data used to describe a single C-state and its associated MSR.
 */
typedef struct pw_c_state_msr_meta_data pw_c_state_msr_meta_data_t;
struct pw_c_state_msr_meta_data {
    pw_msr_identifier_t id; // The MSR identifier for this C-state
    /*
     * On Big-cores, the hint that gets passed to mwait is basically the ACPI C-state
     * and differs from the actual Intel C-state (e.g. on SNB, mwait hint for (Intel)C7
     * is '4' ==> the power_start tracepoint will receive a hint of '4', which must then
     * be converted in Ring-3 to C7).
     * Note:
     * 1. An mwait hint of "zero" indicates "don't care" (e.g. package C6 on SLM cannot have
     * an mwait 'hint').
     * 2. This is (usually) equal to the C-state number on Android (needs investigation for SLM!!!)
     */
    u16 acpi_mwait_hint; // The mwait hint corresponding to this C-state; GU: changed to "u16" for alignment reasons
    u16 target_residency; // Target residency for this C-state
    /*
     * The "msr_name" field basically encodes the information present in "/sys/devices/system/cpu/cpu0/cpuidle/stateXXX/name"
     */
    pw_string_type_t msr_name; // The actual C-state name (e.g. "ATM-C6")
};
#define C_MSR_META_MSG_HEADER_SIZE (sizeof(pw_c_state_msr_meta_data) - sizeof(pw_string_type_t))

/*
 * Meta data used to describe C-states.
 */
typedef struct c_meta_data c_meta_data_t;
struct c_meta_data {
    /* 
     * GEH: Could we add an enum for processing unit (GPU, CPU, etc.) and add a field here to reference? Something like this:
     * proc_unit_t proc_unit; 
     */
    u16 num_c_states; // The number of 'pw_c_state_msr_meta_data' instances encoded in the 'data' field below.
                      // GU: Changed from u8 --> u16 for alignment
    pw_c_state_msr_meta_data_t *data; // An array of 'pw_c_state_msr_meta_data' instances, one per C-state
                                      // Length of the array is given by num_c_states.
                                      // For SW1 file, this array is contiguous in memory to the c_meta_data_t struct (inline):
                                      // e.g. pw_c_state_msr_meta_data_t data[num_c_state];
};
#define C_META_MSG_HEADER_SIZE (sizeof(c_meta_data_t) - sizeof(pw_c_state_msr_meta_data_t *))

/*
 * HACK! (JC)
 * Meta data used to describe a single C-state and its associated MSR as a fixed-length sample.
 */
typedef struct pw_c_state_msr_meta_sample pw_c_state_msr_meta_sample_t;
struct pw_c_state_msr_meta_sample {
    pw_msr_identifier_t id; // The MSR identifier for this C-state
    u16 acpi_mwait_hint; // The mwait hint corresponding to this C-state; GU: changed to "u16" for alignment reasons
    u16 target_residency; // Target residency for this C-state
    /*
     * The "msr_name" field basically encodes the information present in "/sys/devices/system/cpu/cpu0/cpuidle/stateXXX/name"
     */
    char msr_name[PW_MAX_C_MSR_NAME]; // The actual C-state name (e.g. "CC6, MC0, PC6")
};

/*
 * HACK! (JC)
 * Temporary fixed-length Structure used to describe a single C-state fixed-length sample and its associated MSR.
 * Multiple fixed-length samples may be chained to increase the available frequencies beyond PW_MAX_MSRS_PER_META_SAMPLE
 * Used for CPU & GPU meta samples
 */
typedef struct c_meta_sample c_meta_sample_t;
struct c_meta_sample {
    u16 num_c_states; // The number of 'pw_c_state_msr_meta_data' instances encoded in the 'data' field below.
    pw_c_state_msr_meta_sample_t data[PW_MAX_MSRS_PER_META_SAMPLE]; // An array of 'pw_c_state_msr_meta_sample' instances, one per C-state
                                      // Length of the array is given by num_c_states.
                                      // e.g. pw_c_state_msr_meta_sample_t data[num_c_states];
};

/*
 * Meta data used to describe P-state samples.
 */
typedef struct p_meta_data p_meta_data_t;
struct p_meta_data {
    /*
     * GEH: Could we add an enum for processing unit (GPU, CPU, etc.) and add a field here to reference? Something like this:
     * proc_unit_t proc_unit;
     */
    u16 num_available_freqs; // The # of frequencies in the 'data' field below; 256 freqs should be enough for anybody!
    u16 *available_freqs; // A (variable-length) array of 16bit frequencies, in MHz. 
                          // Length of array is given by 'num_available_freqs'
                          // For SW1 file, this array is contiguous in memory to the p_meta_data_t structure (inline):
                          // e.g. u16 available_freqs[num_available_freqs];
};
#define P_META_MSG_HEADER_SIZE (sizeof(p_meta_data_t) - sizeof(u16 *))


/*
 * Meta data used to describe the target system (OS+H/W). Basically, most of
 * the data that appears in the 'SYS_PARAMS' section of a current '.ww1' file
 * (excluding some that is C-state specific and some that is P-state specific, see above).
 * --------------------------------------------------------------------------------------------------------------------------------
 *  WARNING: SOME COMMENTS BELOW EXPOSE INTEL-PRIVATE DATA: REMOVE BEFORE DISTRIBUTING EXTERNALLY!!!
 * --------------------------------------------------------------------------------------------------------------------------------
 */
typedef struct system_meta_data system_meta_data_t;
#if 0
struct system_meta_data {
    u8 driver_version_major, driver_version_minor, driver_version_other;          // Driver version
    pw_string_type_t collector_name;                                              // Collector name e.g. SOCWatch for Android (NDK)
    u8 collector_version_major, collector_version_minor, collector_version_other; // Collector version
    u8 format_version_major, format_version_minor;                                // File format version
    u8 bounded;                                                                   // 1 for bounded and 0 for unbounded
    u16 customer_id, vendor_id, manufacturer_id, platform_id, hardware_id;        // Soft Platform IDs (SPID)
    float collection_time_seconds;                                                // Collection time, in seconds
    u64 start_tsc, stop_tsc;
    u64 start_timeval, stop_timeval;
    u64 start_time, stop_time;
    pw_string_type_t host_name;
    pw_string_type_t os_name;
    pw_string_type_t os_type;
    pw_string_type_t os_version;
    pw_string_type_t cpu_brand;
    u16 cpu_family, cpu_model, cpu_stepping;                                      // "u8" is probably enough for each of these!
    /*
     * --------------------------------------------------------------------------------------------------------------------------------
     *  WARNING: REMOVE THIS COMMENT BEFORE DISTRIBUTING CODE EXTERNALLY!!!
     * --------------------------------------------------------------------------------------------------------------------------------
     * We currently encode the rate at which the Cx MSRs tick within the config file. However, on SLM, the rate at which the Cx MSRs
     * tick is specified by the 'GUAR_RATIO', which is obtained from bits 21:16 of the PUNIT_CR_IACORE_RATIOS MSR (0x66a).
     */
    u16 cpu_c_states_clock_rate; // The rate at which the C-state MSRs tick.
    u8 msr_fsb_freq_value; // Encoding for bus frequency; needs a "switch" statement to retrieve the ACTUAL bus freq
    u8 perf_status_bits[2]; // Need a low and high value
    u32 turbo_threshold;
    u16 num_cpus;
    pw_string_type_t cpu_topology;
    u16 tsc_frequency_mhz;
    u8 was_any_thread_bit_set, was_auto_demote_enabled;
    u32 collection_switches;
    s32 profiled_app_pid;
    pw_string_type_t profiled_app_name;
    u64 number_of_samples_collected, number_of_samples_dropped;
    /*
     * Do we even need a "descendent_pids_list" anymore???
     */
    // descendent_pids_list; // ???
};
#endif
struct system_meta_data {
    /*
     * 64bit vars go here...
     */
    u64 start_tsc, stop_tsc;
    u64 start_timeval, stop_timeval;
    u64 start_time, stop_time;
    u64 number_of_samples_collected, number_of_samples_dropped;
    /*
     * 32bit vars go here...
     */
    float collection_time_seconds;                                                // Collection time, in seconds
    float bus_freq_mhz; // The bus frequency, in MHz
    u32 turbo_threshold;
    u32 collection_switches;
    s32 profiled_app_pid;
    s32 micro_patch_ver;
    /*
     * 16bit vars go here...
     */
    u16 customer_id, vendor_id, manufacturer_id, platform_id, hardware_id;        // Soft Platform IDs (SPID)
    u16 cpu_family, cpu_model, cpu_stepping;                                      // "u8" is probably enough for each of these!
    u16 cpu_c_states_clock_rate; // The rate at which the C-state MSRs tick.
    u16 num_cpus;
    u16 tsc_frequency_mhz;
    /*
     * 8bit vars go here...
     */
    u8 driver_version_major, driver_version_minor, driver_version_other;          // Driver version
    u8 collector_version_major, collector_version_minor, collector_version_other; // Collector version
    u8 format_version_major, format_version_minor;                                // File format version
    // u8 bound;                                                                  // 1 for bound and 0 for unbound
    u8 userspace_pointer_size_bytes;                                              // '4' for 32b userspace, '8' for 64b userspace
    /*
     * --------------------------------------------------------------------------------------------------------------------------------
     *  WARNING: REMOVE THIS COMMENT BEFORE DISTRIBUTING CODE EXTERNALLY!!!
     * --------------------------------------------------------------------------------------------------------------------------------
     * We currently encode the rate at which the Cx MSRs tick within the config file. However, on SLM, the rate at which the Cx MSRs
     * tick is specified by the 'GUAR_RATIO', which is obtained from bits 21:16 of the PUNIT_CR_IACORE_RATIOS MSR (0x66a).
     */
    // GU: replaced with "float bus_freq_mhz" value
    // u8 msr_fsb_freq_value; // Encoding for bus frequency; needs a "switch" statement to retrieve the ACTUAL bus freq
    u8 perf_status_bits[2]; // Need a low and high value
    u8 was_any_thread_bit_set, was_auto_demote_enabled;
    /*
     * Var-len vars go here...
     */
    pw_string_type_t collector_name;                                              // Collector name e.g. SOCWatch for Android (NDK)
    pw_string_type_t host_name;
    pw_string_type_t os_name;
    pw_string_type_t os_type;
    pw_string_type_t os_version;
    pw_string_type_t cpu_brand;
    pw_string_type_t cpu_topology;
    pw_string_type_t profiled_app_name;
};
#define SYSTEM_META_MSG_HEADER_SIZE (sizeof(system_meta_data) -  (8 * sizeof(pw_string_type_t))) /* 8 because we have 8 pw_string_type_t instances in this class */

typedef struct meta_data_msg meta_data_msg_t;
struct meta_data_msg {
    u16 data_len;  // Probably not required: this value can be derived from the "data_len" field of the PWCollector_msg struct!
    u16 data_type; // The type of payload encoded by 'data': one of 'sample_type_t'
                   // GU: Changed from u8 --> u16 for alignment

    void *data;    // For SW1 file, this is the payload:  one of *_meta_data_t corresponding to data_type (inline memory).
                   // For internal data, this is a pointer to the payload memory (not inline).
};
#define PW_META_MSG_HEADER_SIZE sizeof(meta_data_msg_t) - sizeof(void *)

/*
 * The main PWCollector_sample structure. 
 * are encoded in these.
 */
/*
 * "Final" message header. ALL Ring 0 --> Ring 3 (data) messages are encoded in these.
 * -------------------------------------------------------------------------------------------
 * MUST Set "cpuidx" to ZERO for payloads that don't require a cpu field (e.g. GFX C-states).
 * (cpuidx is included for all messages because it makes sorting data easier)
 * -------------------------------------------------------------------------------------------
 */
typedef struct PWCollector_msg PWCollector_msg_t; 
struct PWCollector_msg {
    u64 tsc;      // TSC of message.
                  // GEH: Is this equal to wakeup TSC for c_msg_t samples?
    u16 data_len; // length of payload message in bytes (not including this header) represented by p_data.
    u16 cpuidx;   // GEH: Need to define what this is for post-processed samples
    u8 data_type; // The type of payload encoded by 'p_data': one of 'sample_type_t'
    u8 padding;   // The compiler would have inserted it anyway!

    u64 p_data;   // For SW1 file, this is the payload: one of *_msg_t corresponding to data_type (inline memory).
                  // For internal data, this field is a pointer to the non-contiguous payload memory (not inline).
                  // GU: changed from "u8[1]" to "u64" to get the driver to compile
};
#define PW_MSG_HEADER_SIZE ( sizeof(PWCollector_msg_t) - sizeof(u64) )


#pragma pack(pop) /* Restore previous alignment */




/*
 * Structure used to encode C-state sample information.
 */
typedef struct c_sample {
    u16 break_type; // instance of 'c_break_type_t'
    u16 prev_state; // "HINT" parameter passed to TPS probe
    pid_t pid; // PID of process which caused the C-state break.
    pid_t tid; // TID of process which caused the C-state break.
    u32 tps_epoch; // Used to sync with SCHED_SAMPLE events
    /*
     * "c_data" is one of the following:
     * (1) If "break_type" == 'I' ==> "c_data" is the IRQ of the interrupt
     * that caused the C-state break.
     * (2) If "break_type" == 'D' || 'N' => "c_data" is the TSC that maps to the 
     * user-space call trace ID for the process which caused the C-state break.
     * (3) If "break_type" == 'U' ==> "c_data" is undefined.
     */
    u64 c_data;
    u64 c_state_res_counts[MAX_MSR_ADDRESSES];
} c_sample_t;

#define RES_COUNT(s,i) ( (s).c_state_res_counts[(i)] )

/*
 * Structure used to encode P-state transition information.
 *
 * UPDATE: For TURBO: for now, we only encode WHETHER the CPU is
 * about to TURBO-up; we don't include information on WHICH Turbo
 * frequency the CPU will execute at. See comments in struct below
 * for an explanation on why the 'frequency' field values are
 * unreliable in TURBO mode.
 */
typedef struct p_sample {
    /*
     * Field to encode the frequency
     * the CPU was ACTUALLY executing
     * at DURING THE PREVIOUS 
     * P-QUANTUM.
     */
    u32 frequency;
    /*
     * Field to encode the frequency
     * the OS requested DURING THE
     * PREVIOUS P-QUANTUM.
     */
    u32 prev_req_frequency;
    /*
     * We encode the frequency at the start
     * and end of a collection in 'boundary'
     * messages. This flag is set for such
     * messages.
     */
    u32 is_boundary_sample;
    u32 padding;
    /*
     * The APERF and MPERF values.
     */
    u64 unhalted_core_value, unhalted_ref_value;
} p_sample_t;


/*
 * Structure used to encode kernel-space call trace information.
 */
typedef struct k_sample {
    /*
     * "trace_len" indicates the number of entries in the "trace" array.
     * Note that the actual backtrace may be larger -- in which case the "sample_len"
     * field of the enclosing "struct PWCollector_sample" will be greater than 1.
     */
    u32 trace_len;
    /*
     * We can have root timers with non-zero tids.
     * Account for that possibility here.
     */
    pid_t tid;
    /*
     * The entry and exit TSC values for this kernel call stack.
     * MUST be equal to "[PWCollector_sample.tsc - 1, PWCollector_sample.tsc + 1]" respectively!
     */
    u64 entry_tsc, exit_tsc;
    /*
     * "trace" contains the kernel-space call trace.
     * Individual entries in the trace correspond to the various
     * return addresses in the call trace, shallowest address first.
     * For example: if trace is: "0x10 0x20 0x30 0x40" then 
     * the current function has a return address of 0x10, its calling function
     * has a return address of 0x20 etc.
     */
    u64 trace[TRACE_LEN];
} k_sample_t;


/*
 * Structure used to encode kernel-module map information.
 */
typedef struct m_sample {
    /*
     * Offset of current chunk, in case a kernel module is 
     * mapped in chunks. DEFAULTS TO ZERO!
     */
    u32 offset;
    /*
     * Compiler would have auto-padded this for us, but
     * we make that padding explicit just in case.
     */
    u32 padding_64b;
    /*
     * The starting addr (in HEX) for this module.
     */
    u64 start;
    /*
     * The ending addr (in HEX) for this module.
     */
    u64 end;
    /*
     * Module NAME. Note that this is NOT the full
     * path name. There currently exists no way
     * of extracting path names from the module
     * structure.
     */
    char name[PW_MODULE_NAME_LEN];
} m_sample_t;


/*
 * Structure used to encode IRQ # <-> DEV name
 * mapping information.
 */
typedef struct i_sample {
    /*
     * The IRQ #
     */
    int irq_num;
    /*
     * Device name corresponding
     * to 'irq_num'
     */
    char irq_name[PW_IRQ_DEV_NAME_LEN];
} i_sample_t;

/*
 * The 'type' of the associated
 * 'r_sample'.
 */
typedef enum r_sample_type {
    PW_PROC_FORK = 0, /* Sample encodes a process FORK */
    PW_PROC_EXIT = 1, /* Sample encodes a process EXIT */
    PW_PROC_EXEC = 2  /* Sample encodes an EXECVE system call */
} r_sample_type_t;

typedef struct r_sample {
    u32 type;
    pid_t tid, pid;
    char proc_name[PW_MAX_PROC_NAME_SIZE];
} r_sample_t;

/*
 * Temporary fixed-length meta data structure used to describe S-state residency.
 * (Plan to switch to variable length samples for everything later.)
 */
#define PW_MAX_PLATFORM_STATE_NAME_LEN 15
typedef struct s_residency_meta_sample s_residency_meta_sample_t;
struct s_residency_meta_sample {
    u8 num_states;            // The number of states available including S3.
    char state_names[6][PW_MAX_PLATFORM_STATE_NAME_LEN]; // The list of state names e.g. S0i0, S0i1, S0i2, S0i3, S3 ...
                              // The order must be same as the order of values stored in residencies
};

/*
 * Platform state (a.k.a. S state) residency counter sample
 */
typedef struct event_sample s_residency_sample_t;
/*
 * Platform state (a.k.a. S state) sample
 */
typedef struct s_state_sample {
    u32 state; // S-state
} s_state_sample_t;


typedef struct event_sample d_residency_t;

/*
 * Device state (a.k.a. D state) residency counter sample
 */
typedef struct d_residency_sample {
    u16 device_type;     // Either NORTH_COMPLEX or SOUTH_COMPLEX
    u16 num_sampled;
    u16 mask[PW_MAX_DEVICES_PER_SAMPLE]; // Each bit indicates whether LSS residency is counted or not.
                // 1 means "counted", 0 means "not counted"
                // The last byte indicates the number of LSSes sampled 
    d_residency_t d_residency_counters[PW_MAX_DEVICES_PER_SAMPLE]; // we can fit at most '2' samples in every 'PWCollector_sample_t'
} d_residency_sample_t;

/*
 * Device state (a.k.a. D state) sample from north or south complex
 */
typedef struct d_state_sample {
    char device_type;     // Either NORTH_COMPLEX or SOUTH_COMPLEX
    u32 states[4]; // Each device state is represented in 2 bits
} d_state_sample_t;

/*
 * The 'type' of the associated
 * 'w_sample'.
 */
typedef enum w_sample_type {
    PW_WAKE_LOCK = 0, // Wake lock
    PW_WAKE_UNLOCK = 1, // Wake unlock
    PW_WAKE_LOCK_TIMEOUT = 2, // Wake lock with timeout
    PW_WAKE_LOCK_INITIAL = 3, // Wake locks acquired before collection
    PW_WAKE_UNLOCK_ALL = 4 // All previously held wakelocks have been unlocked -- used in ACPI S3 notifications
} w_sample_type_t;

/*
 * Wakelock sample
 */
typedef struct w_sample {
    w_sample_type_t type;   // Wakelock type
    pid_t tid, pid;
    char name[PW_MAX_WAKELOCK_NAME_SIZE]; // Wakelock name
    u64 expires; // wakelock timeout in tsc if type is equal to PW_WAKE_LOCK_TIMEOUT,
                 // otherwise 0
    char proc_name[PW_MAX_PROC_NAME_SIZE]; // process name
} w_sample_t;

/*
 * Structure to return Dev # <-> Dev Name mappings.
 */
typedef struct dev_sample {
    u16 dev_num;  // Device ID
    device_type_t dev_type; // one of "device_type_t"
                            // The pair (dev_num, dev_type) is a unique ID for each device
    char dev_short_name[PW_MAX_DEV_SHORT_NAME_SIZE];
    char dev_long_name[PW_MAX_DEV_LONG_NAME_SIZE];
} dev_sample_t;

/*
 * Structure to return UID # <-> Package Name mappings.
 */
typedef struct pkg_sample {
    u32 uid;
    char pkg_name[PW_MAX_PKG_NAME_SIZE];
} pkg_sample_t;

/*
 * HACK! (GEH)
 * Temporary fixed-length structure used to describe a single GPU frequency (p-state) sample.
 * (Plan to switch to variable length samples for everything later.)
 */
typedef gpufreq_msg_t gpu_p_sample_t;

/*
 * HACK! (GEH)
 * Temporary fixed-length structure used to describe a single Thermal state sample.
 * (Plan to switch to variable length samples for everything later.)
 */
typedef thermal_msg_t thermal_sample_t;

/*
 * HACK! (GEH)
 * Temporary fixed-length structure used to describe a single Bandwidth sample.
 * (Plan to switch to variable length samples for everything later.)
 */
typedef bw_msg_t bw_sample_t;
// Number of 8-byte data fields in a bandwidth sample:  (subtract out index and duration fields)
#define PW_NUM_BANDWIDTH_COUNT_FIELDS  ( (sizeof(bw_sample_t) - sizeof(u16) - sizeof(u64) ) >> 3 )

/*
 * HACK! (GEH)
 * Temporary fixed-length Structure used to describe a single Thermal component.
 * (Plan to switch to variable length samples for everything later.)
 * Multiple fixed-length samples may be chained to increase the available frequencies beyond PW_MAX_FREQS_PER_META_SAMPLE
 * In this case, sample order determines frequency order for visualization.
 */
typedef struct gpu_freq_sample {
    u16 num_available_freqs; // Number of available gpu frequencies given in this sample.
    u16 available_freqs[PW_MAX_FREQS_PER_META_SAMPLE];    // List of all available frequencies.
                             // The unit of frequency here is Mhz.
} gpu_freq_sample_t;

/*
 * HACK! (GEH)
 * Temporary fixed-length Structure used to describe a single Thermal component.
 * (Plan to switch to variable length samples for everything later.)
 */
typedef struct thermal_comp_sample {
    u16 thermal_comp_num;         // index used for matching thermal component index in thermal_sample
    thermal_unit_t thermal_unit;  // 0 is Fahrenheit and 1 is Celcius.
    char thermal_comp_name[PW_MAX_METADATA_NAME];  // Name of component like Core, Skin, MSIC Die, SOC, ...
} thermal_comp_sample_t;

/*
 * HACK! (GEH)
 * Temporary fixed-length Structure used to describe a single Bandwidth component/pathway.
 * (Plan to switch to variable length samples for everything later.)
 */
typedef struct bw_comp_sample {
    u16 bw_comp_num;                        // Index used for matching bandwidth component index in bw_sample
    char bw_comp_name[PW_MAX_METADATA_NAME]; // Names of component/pathway like Core to DDR0, Core to DDR1, ISP, GFX, IO, DISPLAY ...
} bw_comp_sample_t;

/*
 * Temporary fixed-length Structure used to describe all elements of a single Bandwidth component/pathway.
 * (Plan to switch to variable length samples for everything later.)
 */
typedef struct bw_multi_meta_sample {
    u16 index;               // Array index to components defined in bw_meta_data. 
                             // Index must be [0, num_components)
                             // Currently, this is ALWAYS ZERO (because only one VISA metric can be collected at a time).
    u16 bw_comp_element_index;           // Index used for matching individual element of a bandwidth component
    char name[PW_MAX_METADATA_NAME]; // Individual names for each element in a component e.g. "Read32", "WritePartial" "DDR-0 Rank-0 Read64"
} bw_multi_meta_sample_t;

typedef struct dram_srr_comp_sample dram_srr_comp_sample_t;
struct dram_srr_comp_sample {
    u16 comp_idx;                             // Index used for matching bandwidth component index in bw_sample
    char comp_name[PW_MAX_METADATA_NAME]; // Names of components like DUNIT0, DUNIT1...
};

typedef struct dram_srr_sample {
    u16 index;                // The index of components matched with comp_idx in dram_srr_comp_sample.
    u64 duration;             // The unit should be TSC ticks.
    u64 residency_cpu_ticks;  // Residency in terms of CPU clock ticks i.e. TSC
                              // This field is for VTune visualization.    
    u64 residency_soc_ticks;  // Residency in terms of SOC clock ticks
} dram_srr_sample_t;

/*
 * The C/P/K/S sample structure.
 */
typedef struct PWCollector_sample {
    u32 cpuidx;
    u16 sample_type; // The type of the sample: one of "sample_type_t"
    /*
     * "sample_len" is useful when stitching together 
     * multiple PWCollector_sample instances.
     * This is used in cases where the kernel-space call 
     * trace is very large, and cannot fit within one K-sample.
     * We can stitch together a MAX of
     * 256 K-samples.
     */
    u16 sample_len;
    u64 tsc; // The TSC at which the measurement was taken
    union {
        c_sample_t c_sample;
        p_sample_t p_sample;
        k_sample_t k_sample;
        m_sample_t m_sample;
        i_sample_t i_sample;
        r_sample_t r_sample;
        s_residency_sample_t s_residency_sample;
        s_residency_meta_sample_t s_residency_meta_sample;
        s_state_sample_t s_state_sample;
        d_state_sample_t d_state_sample;
        d_residency_sample_t d_residency_sample;
        w_sample_t w_sample;
        u_sample_t u_sample;
        event_sample_t e_sample;
        dev_sample_t dev_sample;
        pkg_sample_t pkg_sample;
        /*
         * HACK!! (GEH)
         * Added bandwidth & thermal samples temporarily until switch entirely to variable-length msgs
         */
        thermal_sample_t thermal_sample;
        thermal_comp_sample_t thermal_comp_sample;
        bw_sample_t bw_sample;
        bw_comp_sample_t bw_comp_sample;
        gpu_p_sample_t gpu_p_sample;
        gpu_freq_sample_t gpu_freq_sample;
        dram_srr_sample_t dram_srr_sample;
        dram_srr_comp_sample_t dram_srr_comp_sample;
        /*
         * HACK HACK HACK!!!
         * Added for SLM compatibility.
         */
        c_msg_t c_msg;
        p_msg_t p_msg;
        tmp_c_state_msr_set_sample_t msr_set_sample;
        c_multi_msg_t c_multi_msg;
        /*
         * HACK!
         *
         */
        c_meta_sample_t c_meta_sample;
        c_meta_sample_t gpu_c_meta_sample;
        bw_multi_sample_t bw_multi_sample;
        bw_multi_meta_sample_t bw_multi_meta_sample;
    };
} PWCollector_sample_t;


typedef enum PWCollector_cmd {
    PW_START = 1,
    PW_DETACH = 2,
    PW_PAUSE = 3,
    PW_RESUME = 4,
    PW_STOP = 5,
    PW_CANCEL = 6,
    PW_SNAPSHOT = 7,
    PW_STATUS = 8,
    PW_MARK = 9
} PWCollector_cmd_t;

/*
 * UPDATE: Whenever a new type is added here,
 * the config parser (PWParser) needs to be updated accordingly.
 */ 
typedef enum power_data {
    PW_SLEEP = 0, /* DD should register all timer and sleep-related tracepoints */
    PW_KTIMER = 1, /* DD should collect kernel call stacks */
    PW_FREQ = 2, /* DD should collect P-state transition information */
    PW_PLATFORM_RESIDENCY = 3, /* DD should collect S-state residency information */
    PW_PLATFORM_STATE = 4, /* DD should collect S-state samples */
    PW_DEVICE_SC_RESIDENCY = 5, /* DD should collect South-Complex D-state residency information */
    PW_DEVICE_NC_STATE = 6, /* DD should collect North-Complex D-state samples */
    PW_DEVICE_SC_STATE = 7, /* DD should collect South-Complex D-state samples */
    PW_WAKELOCK_STATE = 8, /* DD should collect wakelock samples */
    PW_POWER_C_STATE = 9, /* DD should collect C-state samples */
    PW_THERMAL_CORE = 10, /* DD should collect Core temperature samples */
    PW_THERMAL_SOC_DTS = 11, /* DD should collect SOC_DTS readings samples */
    PW_THERMAL_SKIN = 12, /* DD should collect SKIN temperature samples */
    PW_THERMAL_MSIC = 13, /* DD should collect MSIC die temperature samples */
    PW_GPU_FREQ = 14,  /* DD should collect GPU Frequency samples */
    PW_BANDWIDTH_DRAM = 15, /* DD should collect DDR bandwidth samples */
    PW_BANDWIDTH_CORE = 16, /* DD should collect Core to DDR bandwidth samples */
    PW_BANDWIDTH_GPU = 17, /* DD should collect GPU to DDR bandwidth samples */
    PW_BANDWIDTH_DISP = 18, /* DD should collect Display to DDR bandwidth samples */
    PW_BANDWIDTH_ISP = 19, /* DD should collect ISP to DDR bandwidth samples */
    PW_BANDWIDTH_IO = 20, /* DD should collect IO bandwidth samples */
    PW_BANDWIDTH_SRR = 21, /* DD should collect DRAM Self Refresh residency samples */
    PW_GPU_C_STATE = 22, /* DD should collect GPU C-state samples */
    PW_FPS = 23, /* DD should collect FPS information */
    PW_ACPI_S3_STATE = 24, /* DD should collect ACPI S-state samples */
    PW_POWER_SNAPSHOT_C_STATE = 25, /* DD should collect SNAPSHOT C-state data */
    PW_BANDWIDTH_CORE_MODULE0 = 26, /* DD should collect Core on Module 0 to DDR bandwidth samples */
    PW_BANDWIDTH_CORE_MODULE1 = 27, /* DD should collect Core on Module 1 to DDR bandwidth samples */
    PW_BANDWIDTH_CORE_32BYTE = 28, /* DD should collect Core to DDR 32bytes bandwidth samples */
    PW_BANDWIDTH_CORE_64BYTE = 29, /* DD should collect Core to DDR 64bytes bandwidth samples */
    PW_BANDWIDTH_SRR_CH0 = 30, /* DD should collect Channel 0 DRAM Self Refresh residency samples */
    PW_BANDWIDTH_SRR_CH1 = 31, /* DD should collect Channel 1 DRAM Self Refresh residency samples */
    PW_BANDWIDTH_TUNIT = 32, /* DD should collect T-Unit bandwidth samples */
    PW_MAX_POWER_DATA_MASK /* Marker used to indicate MAX valid 'power_data_t' enum value -- NOT used by DD */
} power_data_t;

#define POWER_SLEEP_MASK (1ULL << PW_SLEEP)
#define POWER_KTIMER_MASK (1ULL << PW_KTIMER)
#define POWER_FREQ_MASK (1ULL << PW_FREQ)
#define POWER_S_RESIDENCY_MASK (1ULL << PW_PLATFORM_RESIDENCY)
#define POWER_S_STATE_MASK (1ULL << PW_PLATFORM_STATE)
#define POWER_D_SC_RESIDENCY_MASK (1ULL << PW_DEVICE_SC_RESIDENCY)
#define POWER_D_SC_STATE_MASK (1ULL << PW_DEVICE_SC_STATE)
#define POWER_D_NC_STATE_MASK (1ULL << PW_DEVICE_NC_STATE)
#define POWER_WAKELOCK_MASK (1ULL << PW_WAKELOCK_STATE)
#define POWER_C_STATE_MASK ( 1ULL << PW_POWER_C_STATE )
#define POWER_THERMAL_CORE_MASK (1ULL << PW_THERMAL_CORE)
#define POWER_THERMAL_SOC_DTS_MASK (1ULL << PW_THERMAL_SOC_DTS)
#define POWER_THERMAL_SKIN_MASK (1ULL << PW_THERMAL_SKIN)
#define POWER_THERMAL_MSIC_MASK (1ULL << PW_THERMAL_MSIC)
#define POWER_GPU_FREQ_MASK (1ULL << PW_GPU_FREQ)
#define POWER_BANDWIDTH_DRAM_MASK (1ULL << PW_BANDWIDTH_DRAM )
#define POWER_BANDWIDTH_CORE_MASK (1ULL << PW_BANDWIDTH_CORE )
#define POWER_BANDWIDTH_GPU_MASK (1ULL << PW_BANDWIDTH_GPU )
#define POWER_BANDWIDTH_DISP_MASK (1ULL << PW_BANDWIDTH_DISP )
#define POWER_BANDWIDTH_ISP_MASK (1ULL << PW_BANDWIDTH_ISP )
#define POWER_BANDWIDTH_IO_MASK (1ULL << PW_BANDWIDTH_IO )
#define POWER_BANDWIDTH_SRR_MASK (1ULL << PW_BANDWIDTH_SRR )
#define POWER_GPU_C_STATE_MASK (1ULL << PW_GPU_C_STATE )
#define POWER_FPS_MASK (1ULL << PW_FPS )
#define POWER_ACPI_S3_STATE_MASK (1ULL << PW_ACPI_S3_STATE )
#define POWER_SNAPSHOT_C_STATE_MASK (1ULL << PW_POWER_SNAPSHOT_C_STATE )
#define POWER_BANDWIDTH_CORE_MODULE0_MASK (1ULL << PW_BANDWIDTH_CORE_MODULE0 )
#define POWER_BANDWIDTH_CORE_MODULE1_MASK (1ULL << PW_BANDWIDTH_CORE_MODULE1)
#define POWER_BANDWIDTH_CORE_32BYTE_MASK (1ULL << PW_BANDWIDTH_CORE_32BYTE )
#define POWER_BANDWIDTH_CORE_64BYTE_MASK (1ULL << PW_BANDWIDTH_CORE_64BYTE )
#define POWER_BANDWIDTH_SRR_CH0_MASK (1ULL << PW_BANDWIDTH_SRR_CH0 )
#define POWER_BANDWIDTH_SRR_CH1_MASK (1ULL << PW_BANDWIDTH_SRR_CH1)
#define POWER_BANDWIDTH_TUNIT_MASK (1ULL << PW_BANDWIDTH_TUNIT )

#define SET_COLLECTION_SWITCH(m,s) ( (m) |= (1ULL << (s) ) )
#define RESET_COLLECTION_SWITCH(m,s) ( (m) &= ~(1ULL << (s) ) )
#define WAS_COLLECTION_SWITCH_SET(m, s) ( (m) & (1ULL << (s) ) )

/*
 * Platform-specific config struct.
 */
typedef struct platform_info {
    int residency_count_multiplier;
    int bus_clock_freq_khz;
    int coreResidencyMSRAddresses[MAX_MSR_ADDRESSES];
    int pkgResidencyMSRAddresses[MAX_MSR_ADDRESSES];
    u64 reserved[3];
} platform_info_t;

/*
 * Config Structure. Includes platform-specific
 * stuff and power switches.
 */
struct PWCollector_config {
    // int data;
    u64 data; // collection switches.
    u32 d_state_sample_interval;  // This is the knob to control the frequency of D-state data sampling 
    // to adjust their collection overhead in the unit of msec.
    platform_info_t info;
};


/*
 * Some constants used to describe kernel features
 * available to the power driver.
 */
#define PW_KERNEL_SUPPORTS_CALL_STACKS (1 << 0)
#define PW_KERNEL_SUPPORTS_CONFIG_TIMER_STATS (1 << 1)
#define PW_KERNEL_SUPPORTS_WAKELOCK_PATCH (1 << 2)
/*
 * Some constants used to describe arch features enabled
 */
#define PW_ARCH_ANY_THREAD_SET (1 << 0)
#define PW_ARCH_AUTO_DEMOTE_ENABLED (1 << 1)

/*
 * Structure to encode unsupported tracepoints and
 * kernel features that enable power collection.
 */
struct PWCollector_check_platform {
    char unsupported_tracepoints[4096];
    /*
     * Bitwise 'OR' of zero or more of the
     * 'PW_KERNEL_SUPPORTS_' constants described
     * above.
     */
    u32 supported_kernel_features;
    u32 supported_arch_features;
    u64 reserved[3];
};

/*
 * Structure to return status information.
 */
struct PWCollector_status {
    u32 num_cpus;
    u32 time;
    u32 c_breaks;
    u32 timer_c_breaks;
    u32 inters_c_breaks;
    u32 p_trans;
    u32 num_inters;
    u32 num_timers;
};

/*
 * Structure to return version information.
 */
struct PWCollector_version_info {
    int version;
    int inter;
    int other;
};

/*
 * Structure to return specific microcode
 * patch -- for MFLD development steppings.
 */
struct PWCollector_micro_patch_info {
    u32 patch_version;
};

/*
 * Helper struct for IRQ <-> DEV name mappings.
 */
typedef struct PWCollector_irq_mapping {
        int irq_num;
        char irq_name[PW_MAX_IRQ_NAME_SIZE];
} PWCollector_irq_mapping_t;
/*
 * Structure to return IRQ <-> DEV name mappings.
 */
struct PWCollector_irq_mapping_block {
        /*
         * INPUT param: if >= 0 ==> indicates
         * the client wants information for
         * a SPECIFIC IRQ (and does not want
         * ALL mappings).
         */
        int requested_irq_num;
        /*
         * OUTPUT param: records number of
         * valid entries in the 'mappings'
         * array.
         */
        int size;
        /*
         * INPUT/OUTPUT param: records from which IRQ
         * entry the client wants mapping info.
         * Required because the driver
         * may return LESS than the total number
         * of IRQ mappings, in which case the client
         * is expected to call this IOCTL
         * again, specifying the offset.
         */
        int offset;
        /*
         * The array of mappings.
         */
        PWCollector_irq_mapping_t mappings[PW_MAX_NUM_IRQ_MAPPINGS_PER_BLOCK];
};


typedef struct PWCollector_proc_mapping {
    pid_t pid, tid;
    char name[PW_MAX_PROC_NAME_SIZE];
} PWCollector_proc_mapping_t;
/*
 * Structure to return PID <-> PROC name mappings.
 */
struct PWCollector_proc_mapping_block {
    /*
     * OUTPUT param: records number of
     * valid entries in the 'mappings'
     * array.
     */
    int size;
    /*
     * INPUT/OUTPUT param: records from which PROC
     * entry the client wants mapping info.
     * may return LESS than the total number
     * of PROC mappings, in which case the client
     * is expected to call this IOCTL
     * again, specifying the offset.
     */
    int offset;
    /*
     * The array of mappings.
     */
    PWCollector_proc_mapping_t mappings[PW_MAX_NUM_PROC_MAPPINGS_PER_BLOCK];
};


/*
 * Structure to return TURBO frequency
 * threshold.
 */
struct PWCollector_turbo_threshold {
    u32 threshold_frequency;
};

/*
 * Structure to return 'available
 * frequencies' i.e. the list of
 * frequencies the processor
 * may execute at.
 */
struct PWCollector_available_frequencies {
    /*
     * Number of valid entries in the
     * 'frequencies' array -- supplied
     * by the DD.
     */
    u32 num_freqs;
    /*
     * List of available frequencies, in kHz.
     */
    u32 frequencies[PW_MAX_NUM_AVAILABLE_FREQUENCIES];
};

/*
 * Different IO mechanism types.
 */
typedef enum {
    PW_IO_MSR=0,
    PW_IO_IPC=1,
    PW_IO_MMIO=2,
    PW_IO_PCI=3,
    PW_IO_MAX=4
} pw_io_type_t;

typedef struct PWCollector_platform_res_info PWCollector_platform_res_info_t;
struct PWCollector_platform_res_info {
    /*
     * IPC commands for platform residency
     * Valid ONLY if 'collection_type' == 'PW_IO_IPC'
     * ('u32' is probably overkill for these, 'u16' should work just fine)
     */
    u32 ipc_start_command, ipc_start_sub_command; // START IPC command, sub-cmd
    u32 ipc_stop_command, ipc_stop_sub_command; // STOP IPC command, sub-cmd
    u32 ipc_dump_command, ipc_dump_sub_command; // DUMP IPC command, sub-cmd
    u16 num_addrs; // Number of 64b addresses encoded in the 'addrs' array, below
    u8 collection_type; // One of 'pw_io_type_t'
    u8 counter_size_in_bytes; // Usually either 4 (for 32b counters) or 8 (for 64b counters)
    char addrs[1]; // Array of 64bit addresses; size of array == 'num_addrs'
};
#define PW_PLATFORM_RES_INFO_HEADER_SIZE() (sizeof(PWCollector_platform_res_info_t) - sizeof(char[1]))

/*
 * Wrapper for ioctl arguments.
 * EVERY ioctl MUST use this struct!
 */
struct PWCollector_ioctl_arg {
    int in_len;
    int out_len;
    const char *in_arg;
    char *out_arg;
};

#endif // _DATA_STRUCTURES_H_
