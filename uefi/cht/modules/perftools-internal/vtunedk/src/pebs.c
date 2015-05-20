/*COPYRIGHT**
    Copyright (C) 2005-2014 Intel Corporation.  All Rights Reserved.

    This file is part of SEP Development Kit

    SEP Development Kit is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    SEP Development Kit is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SEP Development Kit; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
**COPYRIGHT*/

#include "lwpmudrv_defines.h"
#include <linux/version.h>
#include <linux/percpu.h>

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_struct.h"
#include "lwpmudrv.h"
#include "control.h"
#include "core2.h"
#include "utility.h"
#include "pebs.h"

static PEBS_DISPATCH  pebs_dispatch           = NULL;
static PVOID          pebs_global_memory      = NULL;
static size_t         pebs_global_memory_size = 0;

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID pebs_Corei7_Initialize_Threshold (dts, pebs_record_size)
 *
 * @brief       The nehalem specific initialization
 *
 * @param       NONE
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 */
static VOID
pebs_Corei7_Initialize_Threshold (
    DTS_BUFFER_EXT   dts,
    U32              pebs_record_size
)
{
    DTS_BUFFER_EXT_pebs_threshold(dts)  = DTS_BUFFER_EXT_pebs_base(dts) + pebs_record_size;

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID pebs_Corei7_Overflow ()
 *
 * @brief       The Nehalem specific overflow check
 *
 * @param       NONE
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *    Check the global overflow field of the buffer descriptor.
 *    Precise events can be allocated on any of the 4 general purpose
 *    registers.
 */
static U64
pebs_Corei7_Overflow (
    S32  this_cpu,
    U64  overflow_status
)
{
    DTS_BUFFER_EXT   dtes     = CPU_STATE_dts_buffer(&pcb[this_cpu]);
    U8               status   = FALSE;
    PEBS_REC_EXT     pebs_base;

    if (!dtes) {
        return overflow_status;
    }
    status = (U8)((dtes) && (DTS_BUFFER_EXT_pebs_index(dtes) != DTS_BUFFER_EXT_pebs_base(dtes)));
    if (status) {
        pebs_base = (PEBS_REC_EXT)(UIOP)DTS_BUFFER_EXT_pebs_base(dtes);
        overflow_status  |= PEBS_REC_EXT_glob_perf_overflow(pebs_base);
    }

    return overflow_status;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID pebs_Core2_Initialize_Threshold (dts, pebs_record_size)
 *
 * @brief       The Core2 specific initialization
 *
 * @param       NONE
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 */
static VOID
pebs_Core2_Initialize_Threshold (
    DTS_BUFFER_EXT   dts,
    U32              pebs_record_size
)
{
    DTS_BUFFER_EXT_pebs_threshold(dts)  = DTS_BUFFER_EXT_pebs_base(dts);

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID pebs_Core2_Overflow (dts, pebs_record_size)
 *
 * @brief       The Core2 specific overflow check
 *
 * @param       NONE
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *    Check the base and the index fields of the circular buffer, if they are
 *    not the same, then a precise event has overflowed.  Precise events are
 *    allocated only on register#0.
 */
static U64
pebs_Core2_Overflow (
    S32  this_cpu,
    U64  overflow_status
)
{
    DTS_BUFFER_EXT   dtes     = CPU_STATE_dts_buffer(&pcb[this_cpu]);
    U8               status   = FALSE;

    if (!dtes) {
        return overflow_status;
    }
    status = (U8)((dtes) && (DTS_BUFFER_EXT_pebs_index(dtes) != DTS_BUFFER_EXT_pebs_base(dtes)));
    if (status) {
        // Merom allows only for general purpose register 0 to be precise capable
        overflow_status  |= 0x1;
    }

    return overflow_status;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID pebs_Modify_IP (sample, is_64bit_addr)
 *
 * @brief       Change the IP field in the sample to that in the PEBS record
 *
 * @param       sample        - sample buffer
 * @param       is_64bit_addr - are we in a 64 bit module
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              <NONE>
 */
static VOID
pebs_Modify_IP (
    void        *sample,
    DRV_BOOL     is_64bit_addr
)
{
    SampleRecordPC  *psamp = sample;
    DTS_BUFFER_EXT   dtes  = CPU_STATE_dts_buffer(&pcb[CONTROL_THIS_CPU()]);

    if (dtes && psamp) {
        S8   *pebs_base  = (S8 *)(UIOP)DTS_BUFFER_EXT_pebs_base(dtes);
        S8   *pebs_index = (S8 *)(UIOP)DTS_BUFFER_EXT_pebs_index(dtes);
        SEP_PRINT_DEBUG("In PEBS Fill Buffer: cpu %d\n", CONTROL_THIS_CPU());
        if (pebs_base != pebs_index) {
            PEBS_REC_EXT  pb = (PEBS_REC_EXT)pebs_base;
            if (is_64bit_addr) {
                SAMPLE_RECORD_iip(psamp)    = PEBS_REC_EXT_linear_ip(pb);
                SAMPLE_RECORD_ipsr(psamp)   = PEBS_REC_EXT_r_flags(pb);
            }
            else {
                SAMPLE_RECORD_eip(psamp)    = PEBS_REC_EXT_linear_ip(pb) & 0xFFFFFFFF;
                SAMPLE_RECORD_eflags(psamp) = PEBS_REC_EXT_r_flags(pb) & 0xFFFFFFFF;
            }
        }
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID pebs_Modify_IP_With_Eventing_IP (sample, is_64bit_addr)
 *
 * @brief       Change the IP field in the sample to that in the PEBS record
 *
 * @param       sample        - sample buffer
 * @param       is_64bit_addr - are we in a 64 bit module
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              <NONE>
 */
static VOID
pebs_Modify_IP_With_Eventing_IP (
    void        *sample,
    DRV_BOOL     is_64bit_addr
)
{
    SampleRecordPC  *psamp = sample;
    DTS_BUFFER_EXT   dtes  = CPU_STATE_dts_buffer(&pcb[CONTROL_THIS_CPU()]);

    if (dtes && psamp) {
        S8   *pebs_base  = (S8 *)(UIOP)DTS_BUFFER_EXT_pebs_base(dtes);
        S8   *pebs_index = (S8 *)(UIOP)DTS_BUFFER_EXT_pebs_index(dtes);
        SEP_PRINT_DEBUG("In PEBS Fill Buffer: cpu %d\n", CONTROL_THIS_CPU());
        if (pebs_base != pebs_index) {
            PEBS_REC_EXT1  pb = (PEBS_REC_EXT1)pebs_base;
            if (is_64bit_addr) {
                SAMPLE_RECORD_iip(psamp)    = PEBS_REC_EXT1_eventing_ip(pb);
                SAMPLE_RECORD_ipsr(psamp)   = PEBS_REC_EXT1_r_flags(pb);
            }
            else {
                SAMPLE_RECORD_eip(psamp)    = PEBS_REC_EXT1_eventing_ip(pb) & 0xFFFFFFFF;
                SAMPLE_RECORD_eflags(psamp) = PEBS_REC_EXT1_r_flags(pb) & 0xFFFFFFFF;
            }
        }
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID pebs_Modify_TSC (sample)
 *
 * @brief       Change the TSC field in the sample to that in the PEBS record
 *
 * @param       sample        - sample buffer
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              <NONE>
 */
static VOID
pebs_Modify_TSC (
    void        *sample
)
{
    SampleRecordPC  *psamp = sample;
    DTS_BUFFER_EXT   dtes  = CPU_STATE_dts_buffer(&pcb[CONTROL_THIS_CPU()]);
    S8              *pebs_base, *pebs_index;
    PEBS_REC_EXT2    pb;

    if (dtes && psamp) {
        pebs_base  = (S8 *)(UIOP)DTS_BUFFER_EXT_pebs_base(dtes);
        pebs_index = (S8 *)(UIOP)DTS_BUFFER_EXT_pebs_index(dtes);
        SEP_PRINT_DEBUG("In PEBS Fill Buffer: cpu %d\n", CONTROL_THIS_CPU());
        if (pebs_base != pebs_index) {
            pb = (PEBS_REC_EXT2)pebs_base;
            SAMPLE_RECORD_tsc(psamp) = PEBS_REC_EXT2_tsc(pb);
        }
    }

    return;
}

/*
 * Initialize the pebs micro dispatch tables
 */
PEBS_DISPATCH_NODE  core2_pebs =
{
     pebs_Core2_Initialize_Threshold,
     pebs_Core2_Overflow,
     pebs_Modify_IP,
     NULL
};

PEBS_DISPATCH_NODE  core2p_pebs =
{
     pebs_Corei7_Initialize_Threshold,
     pebs_Core2_Overflow,
     pebs_Modify_IP,
     NULL
};

PEBS_DISPATCH_NODE  corei7_pebs =
{
     pebs_Corei7_Initialize_Threshold,
     pebs_Corei7_Overflow,
     pebs_Modify_IP,
     NULL
};

PEBS_DISPATCH_NODE  haswell_pebs =
{
     pebs_Corei7_Initialize_Threshold,
     pebs_Corei7_Overflow,
     pebs_Modify_IP_With_Eventing_IP,
     NULL
};

PEBS_DISPATCH_NODE  perfver4_pebs =
{
     pebs_Corei7_Initialize_Threshold,
     pebs_Corei7_Overflow,
     pebs_Modify_IP_With_Eventing_IP,
     pebs_Modify_TSC
};

#define PER_CORE_BUFFER_SIZE(record_size)  (sizeof(DTS_BUFFER_EXT_NODE) +  2 * (record_size) + 64)

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID* pebs_Alloc_DTS_Buffer (VOID)
 *
 * @brief       Allocate buffers used for latency and pebs sampling
 *
 * @param       NONE
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              Allocate the memory needed to hold the DTS and PEBS records buffer.
 *              This routine is called by a thread that corresponds to a single core
 */
static VOID*
pebs_Alloc_DTS_Buffer (
    U32 pebs_record_size
)
{
    UIOP            pebs_base;
    U32             buffer_size;
    U32             dts_size;
    VOID           *dts_buffer;
    DTS_BUFFER_EXT  dts;
    int             this_cpu;

    /*
     * one PEBS record... need 2 records so that
     * threshold can be less than absolute max
     */
    preempt_disable();
    this_cpu = CONTROL_THIS_CPU();
    preempt_enable();
    dts_size = sizeof(DTS_BUFFER_EXT_NODE);

    /*
     * account for extra bytes to align PEBS base to cache line boundary
     */
    buffer_size = PER_CORE_BUFFER_SIZE(pebs_record_size);
    dts_buffer = (char *)pebs_global_memory + (this_cpu * buffer_size);
    if (!dts_buffer) {
        SEP_PRINT_ERROR("Failed to allocate space for DTS buffer.\n");
        return NULL;
    }

    pebs_base = (UIOP)(dts_buffer) + dts_size;

    //  Make 32 byte aligned
    if ((pebs_base & 0x000001F) != 0x0) {
        pebs_base = ALIGN_32(pebs_base);
    }

    /*
     * Program the DTES Buffer for Precise EBS.
     * Set PEBS buffer for one PEBS record
     */
    dts = (DTS_BUFFER_EXT)dts_buffer;

    DTS_BUFFER_EXT_base(dts)            = 0;
    DTS_BUFFER_EXT_index(dts)           = 0;
    DTS_BUFFER_EXT_max(dts)             = 0;
    DTS_BUFFER_EXT_threshold(dts)       = 0;
    DTS_BUFFER_EXT_pebs_base(dts)       = pebs_base;
    DTS_BUFFER_EXT_pebs_index(dts)      = pebs_base;
    DTS_BUFFER_EXT_pebs_max(dts)        = pebs_base + 2 * pebs_record_size;

    pebs_dispatch->initialize_threshold(dts, pebs_record_size);

    SEP_PRINT_DEBUG("base --- %p\n", DTS_BUFFER_EXT_pebs_base(dts));
    SEP_PRINT_DEBUG("index --- %p\n", DTS_BUFFER_EXT_pebs_index(dts));
    SEP_PRINT_DEBUG("max --- %p\n", DTS_BUFFER_EXT_pebs_max(dts));
    SEP_PRINT_DEBUG("threahold --- %p\n", DTS_BUFFER_EXT_pebs_threshold(dts));
    SEP_PRINT_DEBUG("DTES buffer allocated for PEBS: %p\n", dts_buffer);

    return dts_buffer;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID* pebs_Allocate_Buffers (VOID *params)
 *
 * @brief       Allocate memory and set up MSRs in preparation for PEBS
 *
 * @param       NONE
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              Set up the DS area and program the DS_AREA msrs in preparation
 *              for a PEBS run.  Save away the old value in the DS_AREA.
 *              This routine is called via the parallel thread call.
 */
static VOID
pebs_Allocate_Buffers (
    VOID  *params
)
{
    U64         value;
    CPU_STATE   pcpu = &pcb[CONTROL_THIS_CPU()];
    U32         pebs_record_size;

    pebs_record_size = *((U32*)(params));
    SYS_Write_MSR(IA32_PEBS_ENABLE, 0LL);
    value = SYS_Read_MSR(IA32_MISC_ENABLE);
    if ((value & 0x80) && !(value & 0x1000)) {
        CPU_STATE_old_dts_buffer(pcpu) = (PVOID)(UIOP)SYS_Read_MSR(IA32_DS_AREA);
        CPU_STATE_dts_buffer(pcpu)     = pebs_Alloc_DTS_Buffer(pebs_record_size);
        SEP_PRINT_DEBUG("Old dts buffer - %p\n", CPU_STATE_old_dts_buffer(pcpu));
        SEP_PRINT_DEBUG("New dts buffer - %p\n", CPU_STATE_dts_buffer(pcpu));
        SYS_Write_MSR(IA32_DS_AREA, (U64)(UIOP)CPU_STATE_dts_buffer(pcpu));
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID pebs_Dellocate_Buffers (VOID *params)
 *
 * @brief       Clean up PEBS buffers and restore older values into the DS_AREA
 *
 * @param       NONE
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              Clean up the DS area and all restore state prior to the sampling run
 *              This routine is called via the parallel thread call.
 */
static VOID
pebs_Deallocate_Buffers (
    VOID  *params
)
{
    CPU_STATE   pcpu = &pcb[CONTROL_THIS_CPU()];

    SEP_PRINT_DEBUG("Entered deallocate buffers\n");
    SYS_Write_MSR(IA32_DS_AREA, (U64)(UIOP)CPU_STATE_old_dts_buffer(pcpu));
    CPU_STATE_dts_buffer(pcpu) = NULL;

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          U64 PEBS_Overflowed (this_cpu, overflow_status)
 *
 * @brief       Figure out if the PEBS event caused an overflow
 *
 * @param       this_cpu        -- the current cpu
 *              overflow_status -- current value of the global overflow status
 *
 * @return      updated overflow_status
 *
 * <I>Special Notes:</I>
 *              Figure out if the PEBS area has data that need to be transferred
 *              to the output sample.
 *              Update the overflow_status that is passed and return this value.
 *              The overflow_status defines the events/status to be read
 */
extern U64
PEBS_Overflowed (
    S32  this_cpu,
    U64  overflow_status
)
{
    return pebs_dispatch->overflow(this_cpu, overflow_status);
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID PEBS_Reset_Index (this_cpu)
 *
 * @brief       Reset the PEBS index pointer
 *
 * @param       this_cpu        -- the current cpu
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              reset index to next PEBS record to base of buffer
 */
extern VOID
PEBS_Reset_Index (
    S32    this_cpu
)
{
    DTS_BUFFER_EXT   dtes = CPU_STATE_dts_buffer(&pcb[this_cpu]);

    if (dtes) {
        SEP_PRINT_DEBUG("PEBS Reset Index: %d\n", this_cpu);
        DTS_BUFFER_EXT_pebs_index(dtes) = DTS_BUFFER_EXT_pebs_base(dtes);
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID PEBS_Modify_IP (sample, is_64bit_addr)
 *
 * @brief       Change the IP field in the sample to that in the PEBS record
 *
 * @param       sample        - sample buffer
 * @param       is_64bit_addr - are we in a 64 bit module
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              <NONE>
 */
extern VOID
PEBS_Modify_IP (
    void        *sample,
    DRV_BOOL     is_64bit_addr
)
{
    pebs_dispatch->modify_ip(sample, is_64bit_addr);
    return;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID PEBS_Modify_TSC (sample)
 *
 * @brief       Change the TSC field in the sample to that in the PEBS record
 *
 * @param       sample        - sample buffer
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              <NONE>
 */
extern VOID
PEBS_Modify_TSC (
    void        *sample
)
{
    if (pebs_dispatch->modify_tsc != NULL) {
        pebs_dispatch->modify_tsc(sample);
    }
    return;
}



/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID PEBS_Fill_Buffer (S8 *buffer, EVENT_CONFIG ec)
 *
 * @brief       Fill the buffer with the pebs data
 *
 * @param       buffer  -  area to write the data into
 *              ec      -  current event config
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              <NONE>
 */
extern VOID
PEBS_Fill_Buffer (
    S8           *buffer,
    EVENT_DESC    evt_desc,
    DRV_BOOL      virt_phys_translation_ena
)
{
    DTS_BUFFER_EXT   dtes       = CPU_STATE_dts_buffer(&pcb[CONTROL_THIS_CPU()]);
    DEAR_INFO_NODE   dear_info  = {0};
    PEBS_REC_EXT1    pebs_base_ext1;
    PEBS_REC_EXT2    pebs_base_ext2;

    if (dtes) {
        S8   *pebs_base  = (S8 *)(UIOP)DTS_BUFFER_EXT_pebs_base(dtes);
        S8   *pebs_index = (S8 *)(UIOP)DTS_BUFFER_EXT_pebs_index(dtes);
        SEP_PRINT_DEBUG("In PEBS Fill Buffer: cpu %d\n", CONTROL_THIS_CPU());
        if (pebs_base != pebs_index){
            if (EVENT_DESC_pebs_offset(evt_desc)) {
                SEP_PRINT_DEBUG("PEBS buffer has data available\n");
                memcpy(buffer + EVENT_DESC_pebs_offset(evt_desc),
                       pebs_base,
                       EVENT_DESC_pebs_size(evt_desc));
            }
            if (EVENT_DESC_eventing_ip_offset(evt_desc)) {
                pebs_base_ext1 = (PEBS_REC_EXT1)pebs_base;
                *(U64*)(buffer + EVENT_DESC_eventing_ip_offset(evt_desc)) = PEBS_REC_EXT1_eventing_ip(pebs_base_ext1);
            }
            if (EVENT_DESC_hle_offset(evt_desc)) {
                pebs_base_ext1 = (PEBS_REC_EXT1)pebs_base;
                *(U64*)(buffer + EVENT_DESC_hle_offset(evt_desc)) = PEBS_REC_EXT1_hle_info(pebs_base_ext1);
            }
            if (EVENT_DESC_latency_offset_in_sample(evt_desc)) {
                memcpy(&dear_info,
                        pebs_base + EVENT_DESC_latency_offset_in_pebs_record(evt_desc),
                        EVENT_DESC_latency_size_from_pebs_record(evt_desc));
                DEAR_INFO_nodeid(&dear_info) = 0xFFFF;
                memcpy(buffer + EVENT_DESC_latency_offset_in_sample(evt_desc),
                       &dear_info,
                       sizeof(DEAR_INFO_NODE) );
            }
            if (EVENT_DESC_pebs_tsc_offset(evt_desc)) {
                pebs_base_ext2 = (PEBS_REC_EXT2)pebs_base;
                *(U64*)(buffer + EVENT_DESC_pebs_tsc_offset(evt_desc)) = PEBS_REC_EXT2_tsc(pebs_base_ext2);
            }
        }
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID PEBS_Initialize (DRV_CONFIG pcfg)
 *
 * @brief       Initialize the pebs buffers
 *
 * @param       pcfg  -  Driver Configuration
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *              If the user is asking for PEBS information.  Allocate the DS area
 */
extern VOID
PEBS_Initialize (
    DRV_CONFIG  pcfg
)
{
    U32 pebs_record_size =0;

    if (DRV_CONFIG_pebs_mode(pcfg)) {
        switch (DRV_CONFIG_pebs_mode(pcfg)) {
            case 1:
                SEP_PRINT_DEBUG("Set up the Core2 dispatch table\n");
                pebs_dispatch = &core2_pebs;
                pebs_record_size = sizeof(PEBS_REC_NODE);
                break;
            case 2:
                SEP_PRINT_DEBUG("Set up the Nehalem dispatch\n");
                pebs_dispatch = &corei7_pebs;
                pebs_record_size = sizeof(PEBS_REC_EXT_NODE);
                break;
            case 3:
                SEP_PRINT_DEBUG("Set up the Core2 (PNR) dispatch table\n");
                pebs_dispatch = &core2p_pebs;
                pebs_record_size = sizeof(PEBS_REC_NODE);
                break;
            case 4:
                SEP_PRINT_DEBUG("Set up the Haswell dispatch table\n");
                pebs_dispatch = &haswell_pebs;
                pebs_record_size = sizeof(PEBS_REC_EXT1_NODE);
                break;                
            case 5:
                SEP_PRINT_DEBUG("Set up the Perf version4 dispatch table\n");
                pebs_dispatch = &perfver4_pebs;
                pebs_record_size = sizeof(PEBS_REC_EXT2_NODE);
                break;                
            default:
                SEP_PRINT_DEBUG("Unknown PEBS type. Will not collect PEBS information\n");
                break;
        }
        if (pebs_dispatch) {
            pebs_global_memory_size = GLOBAL_STATE_num_cpus(driver_state) * PER_CORE_BUFFER_SIZE(pebs_record_size);
            pebs_global_memory = (PVOID)CONTROL_Allocate_KMemory(pebs_global_memory_size);
            CONTROL_Invoke_Parallel(pebs_Allocate_Buffers, (VOID *)&pebs_record_size);
        }
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          VOID PEBS_Destroy (DRV_CONFIG pcfg)
 *
 * @brief       Clean up the pebs related buffers
 *
 * @param       pcfg  -  Driver Configuration
 *
 * @return      NONE
 *
 * <I>Special Notes:</I>
 *             Deallocated the DS area used for PEBS capture
 */
extern VOID
PEBS_Destroy (
    DRV_CONFIG  pcfg
)
{
    if (DRV_CONFIG_pebs_mode(pcfg)) {
        CONTROL_Invoke_Parallel(pebs_Deallocate_Buffers, (VOID *)(size_t)0);
        pebs_global_memory = CONTROL_Free_Memory(pebs_global_memory);
        pebs_global_memory_size = 0;
    }

    return;
}
