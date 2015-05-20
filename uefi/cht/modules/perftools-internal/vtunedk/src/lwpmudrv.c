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
#include "lwpmudrv_version.h"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <asm/unistd.h>
#include <linux/compat.h>

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_ioctl.h"
#include "lwpmudrv_struct.h"
#include "inc/ecb_iterators.h"


#include "lwpmudrv_chipset.h"
#include "pci.h"

#include "apic.h"
#include "cpumon.h"
#include "lwpmudrv.h"
#include "utility.h"
#include "control.h"
#include "core2.h"
#include "pmi.h"

#include "output.h"
#include "linuxos.h"
#include "sys_info.h"
#include "eventmux.h"
#include "pebs.h"

#if defined(CONFIG_TRACING) && defined(CONFIG_TRACEPOINTS)
#define CREATE_TRACE_POINTS
#include <linux/string.h>
#include "lwpmudrv_trace.h"
#endif

MODULE_AUTHOR("Copyright (C) 2007-2014 Intel Corporation");
MODULE_VERSION(SEP_NAME"_"SEP_VERSION_STR);
MODULE_LICENSE("Dual BSD/GPL");

typedef struct LWPMU_DEV_NODE_S  LWPMU_DEV_NODE;
typedef        LWPMU_DEV_NODE   *LWPMU_DEV;

struct LWPMU_DEV_NODE_S {
  long              buffer;
  struct semaphore  sem;
  struct cdev       cdev;
};

#define LWPMU_DEV_buffer(dev)      (dev)->buffer
#define LWPMU_DEV_sem(dev)         (dev)->sem
#define LWPMU_DEV_cdev(dev)        (dev)->cdev

/* Global variables of the driver */
SEP_VERSION_NODE        drv_version;
U64                    *read_counter_info     = NULL;
VOID                  **PMU_register_data     = NULL;
U64                    *prev_counter_data     = NULL;
U64                     prev_counter_size     = 0;
VOID                  **desc_data             = NULL;
DISPATCH                dispatch              = NULL;
U64                     total_ram             = 0;
U32                     output_buffer_size    = OUTPUT_LARGE_BUFFER;
static  S32             em_groups_count       = 0;
static  S32             desc_count            = 0;
uid_t                   uid                   = 0;
EVENT_CONFIG            global_ec             = NULL;
DRV_CONFIG              pcfg                  = NULL;
volatile pid_t          control_pid           = 0;
volatile S32            abnormal_terminate    = 0;
U64                    *interrupt_counts      = NULL;
LBR                     lbr                   = NULL;
PWR                     pwr                   = NULL;
LWPMU_DEV               lwpmu_control         = NULL;
LWPMU_DEV               lwmod_control         = NULL;
LWPMU_DEV               lwsamp_control        = NULL;

/* needed for multiple uncores */
U32                     num_devices            = 0;
U32                     cur_device             = 0;
LWPMU_DEVICE            devices                = NULL;
static U32              uncore_em_factor       = 0;

#define UNCORE_EM_GROUP_SWAP_FACTOR   100

#if defined(DRV_USE_UNLOCKED_IOCTL)
static   struct mutex   ioctl_lock;
#endif
volatile int            in_finish_code         = 0;

#define  PMU_DEVICES            2   // pmu, mod

CHIPSET_CONFIG          pma               = NULL;
CS_DISPATCH             cs_dispatch       = NULL;
static S8              *cpu_mask_bits     = NULL;

/*
 *  Global data: Buffer control structure
 */
BUFFER_DESC      cpu_buf    = NULL;
BUFFER_DESC      module_buf = NULL;

static dev_t     lwpmu_DevNum;  /* the major and minor parts for SEP3 base */
static dev_t     lwsamp_DevNum; /* the major and minor parts for SEP3 percpu */

#if defined (DRV_ANDROID)
#define DRV_DEVICE_DELIMITER "_"
static struct class         *pmu_class   = NULL;
#endif

extern volatile int      config_done;

CPU_STATE          pcb                 = NULL;
size_t             pcb_size            = 0;
U32               *core_to_package_map = NULL;
U32                num_packages        = 0;
U64               *pmu_state           = NULL;
U64               *tsc_info            = NULL;
U64               *restore_bl_bypass        = NULL;
U32               **restore_ha_direct2core  = NULL;
U32               **restore_qpi_direct2core = NULL;
UNCORE_TOPOLOGY_INFO_NODE uncore_topology;

#if defined EMON
static U64              cpu0_TSC       = 0;
static U8              *prev_set_CR4   = 0;
#endif

#if !defined(DRV_USE_UNLOCKED_IOCTL)
#define MUTEX_INIT(lock)
#define MUTEX_LOCK(lock)
#define MUTEX_UNLOCK(lock)
#else
#define MUTEX_INIT(lock)     mutex_init(&(lock));
#define MUTEX_LOCK(lock)     mutex_lock(&(lock))
#define MUTEX_UNLOCK(lock)   mutex_unlock(&(lock))
#endif

/* ------------------------------------------------------------------------- */
/*!
 * @fn  void lwpmudrv_PWR_Info(IOCTL_ARGS arg)
 *
 * @param arg - Pointer to the IOCTL structure
 *
 * @return OS_STATUS
 *
 * @brief Make a copy of the Power control information that has been passed in.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_PWR_Info (
    IOCTL_ARGS    arg
)
{
    if (DRV_CONFIG_power_capture(pcfg) == FALSE) {
        SEP_PRINT_WARNING("lwpmudrv_PWR_Info: PWR capture has not been configured\n");
        return OS_SUCCESS;
    }

    // make sure size of incoming arg is correct
    if ((arg->w_len != sizeof(PWR_NODE))  || (arg->w_buf == NULL)) {
        SEP_PRINT_ERROR("lwpmudrv_PWR_Info: PWR capture has not been configured\n");
        return OS_FAULT;
    }

    //
    // First things first: Make a copy of the data for global use.
    //
    pwr = CONTROL_Allocate_Memory((int)arg->w_len);
    if (!pwr) {
        return OS_NO_MEM;
    }

    if (copy_from_user(pwr, arg->w_buf, arg->w_len)) {
        return OS_FAULT;
    }

    return OS_SUCCESS;
}

/*
 * @fn void lwpmudrv_Allocate_Restore_Buffer
 *
 * @param
 *
 * @return   OS_STATUE
 *
 * @brief    allocate buffer space to save/restore the data (for JKT, QPILL and HA register) before collection
 */
static OS_STATUS
lwpmudrv_Allocate_Restore_Buffer (
    VOID
)
{
    int i = 0;

    if (!restore_ha_direct2core) {
        restore_ha_direct2core  = CONTROL_Allocate_Memory(GLOBAL_STATE_num_cpus(driver_state) * sizeof(U32 *));
        if (!restore_ha_direct2core) {
            return OS_NO_MEM;
        }
        for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
            restore_ha_direct2core[i] = CONTROL_Allocate_Memory(MAX_BUSNO * sizeof(U32));
        }
    }
    if (!restore_qpi_direct2core) {
        restore_qpi_direct2core = CONTROL_Allocate_Memory(GLOBAL_STATE_num_cpus(driver_state) * sizeof(U32 *));
        if (!restore_qpi_direct2core) {
            return OS_NO_MEM;
        }
        for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
            restore_qpi_direct2core[i] = CONTROL_Allocate_Memory(2 * MAX_BUSNO * sizeof(U32));
        }
    }
    if (!restore_bl_bypass) {
        restore_bl_bypass  = CONTROL_Allocate_Memory(GLOBAL_STATE_num_cpus(driver_state) * sizeof(U64));
        if (!restore_bl_bypass) {
            return OS_NO_MEM;
        }
    }
    return OS_SUCCESS;
}

/*
 * @fn void lwpmudrv_Free_Restore_Buffer
 *
 * @param
 *
 * @return   OS_STATUE
 *
 * @brief    allocate buffer space to save/restore the data (for JKT, QPILL and HA register) before collection
 */
static OS_STATUS
lwpmudrv_Free_Restore_Buffer (
    VOID
)
{
    U32  i = 0;
    if (restore_ha_direct2core) {
        for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
              restore_ha_direct2core[i]= CONTROL_Free_Memory(restore_ha_direct2core[i]);
        }
        restore_ha_direct2core = CONTROL_Free_Memory(restore_ha_direct2core);
    }
    if (restore_qpi_direct2core) {
         for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
              restore_qpi_direct2core[i]= CONTROL_Free_Memory(restore_qpi_direct2core[i]);
        }
        restore_qpi_direct2core = CONTROL_Free_Memory(restore_qpi_direct2core);
    }
    if (restore_bl_bypass) {
        restore_bl_bypass = CONTROL_Free_Memory(restore_bl_bypass);
    }
    return OS_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Initialize_State(void)
 *
 * @param none
 *
 * @return OS_STATUS
 *
 * @brief  Allocates the memory needed at load time.  Initializes all the
 * @brief  necessary state variables with the default values.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Initialize_State (
    VOID
)
{
    /*
     *  Machine Initializations
     *  Abstract this information away into a separate entry point
     *
     *  Question:  Should we allow for the use of Hot-cpu
     *    add/subtract functionality while the driver is executing?
     */
    GLOBAL_STATE_num_cpus(driver_state)          = num_online_cpus();
    GLOBAL_STATE_active_cpus(driver_state)       = num_online_cpus();
    GLOBAL_STATE_cpu_count(driver_state)         = 0;
    GLOBAL_STATE_dpc_count(driver_state)         = 0;
    GLOBAL_STATE_num_em_groups(driver_state)     = 0;
    GLOBAL_STATE_current_phase(driver_state)     = DRV_STATE_UNINITIALIZED;

    SEP_PRINT_DEBUG("lwpmudrv_Initialize_State: num_cpus=%d, active_cpus=%d\n",
                    GLOBAL_STATE_num_cpus(driver_state),
                    GLOBAL_STATE_active_cpus(driver_state));

    return OS_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn  static void lwpmudrv_Fill_TSC_Info (PVOID param)
 *
 * @param param - pointer the buffer to fill in.
 *
 * @return none
 *
 * @brief  Read the TSC and write into the correct array slot.
 *
 * <I>Special Notes</I>
 */
atomic_t read_now;
static wait_queue_head_t read_tsc_now;
static VOID
lwpmudrv_Fill_TSC_Info (
    PVOID   param
)
{
    U32      this_cpu;

    preempt_disable();
    this_cpu = CONTROL_THIS_CPU();
    preempt_enable();
    //
    // Wait until all CPU's are ready to proceed
    // This will serve as a synchronization point to compute tsc skews.
    //

    if (atomic_read(&read_now) >= 1) {
        if (atomic_dec_and_test(&read_now) == FALSE) {
            wait_event_interruptible(read_tsc_now, (atomic_read(&read_now) >= 1));
        }
    }
    else {
        wake_up_interruptible_all(&read_tsc_now);
    }
    UTILITY_Read_TSC(&tsc_info[this_cpu]);
    SEP_PRINT_DEBUG("lwpmudrv_Fill_TSC_Info: this cpu %d --- tsc --- 0x%llx\n",
                    this_cpu, tsc_info[this_cpu]);

    return;
}


/*********************************************************************
 *  Internal Driver functions
 *     Should be called only from the lwpmudrv_DeviceControl routine
 *********************************************************************/

/* ------------------------------------------------------------------------- */
/*!
 * @fn static void lwpmudrv_Dump_Tracer(const char *)
 *
 * @param Name of the tracer
 *
 * @return void
 *
 * @brief  Function that handles the generation of markers into the ftrace stream
 *
 * <I>Special Notes</I>
 */
static void
lwpmudrv_Dump_Tracer (
    const char    *name,
    U64            tsc
)
{
#if defined(CONFIG_TRACING) && defined(CONFIG_TRACEPOINTS)
    if (tsc == 0) {
        preempt_disable();
        UTILITY_Read_TSC(&tsc);
        tsc -= TSC_SKEW(CONTROL_THIS_CPU());
        preempt_enable();
    }
    trace_lwpmudrv_marker(name, tsc);
#endif
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Version(IOCTL_ARGS arg)
 *
 * @param arg - pointer to the IOCTL_ARGS structure
 *
 * @return OS_STATUS
 *
 * @brief  Local function that handles the LWPMU_IOCTL_VERSION call.
 * @brief  Returns the version number of the kernel mode sampling.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Version (
    IOCTL_ARGS   arg
)
{
    OS_STATUS status;

    // Check if enough space is provided for collecting the data
    if ((arg->r_len != sizeof(U32))  || (arg->r_buf == NULL)) {
        return OS_FAULT;
    }

    status = put_user(SEP_VERSION_NODE_sep_version(&drv_version), (U32 *)arg->r_buf);

    return status;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Reserve(IOCTL_ARGS arg)
 *
 * @param arg - pointer to the IOCTL_ARGS structure
 *
 * @return OS_STATUS
 *
 * @brief
 * @brief  Local function that handles the LWPMU_IOCTL_RESERVE call.
 * @brief  Sets the state to RESERVED if possible.  Returns BUSY if unable
 * @brief  to reserve the PMU.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Reserve (
    IOCTL_ARGS    arg
)
{
    OS_STATUS  status = OS_SUCCESS;
    S32        prev_phase;

    // Check if enough space is provided for collecting the data
    if ((arg->r_len != sizeof(S32))  || (arg->r_buf == NULL)) {
        return OS_FAULT;
    }

    prev_phase = cmpxchg(&GLOBAL_STATE_current_phase(driver_state),
                         DRV_STATE_UNINITIALIZED,
                         DRV_STATE_RESERVED);

    SEP_PRINT_DEBUG("lwpmudrv_Reserve: states --- old_phase = %d; current_phase == %d\n",
            prev_phase, GLOBAL_STATE_current_phase(driver_state));

    status = put_user((prev_phase != DRV_STATE_UNINITIALIZED), (int*)arg->r_buf);

    return status;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static VOID lwpmudrv_Clean_Up(DRV_BOOL)
 *
 * @param  DRV_BOOL finish - Flag to call finish
 *
 * @return VOID
 *
 * @brief  Cleans up the memory allocation.
 *
 * <I>Special Notes</I>
 */
static VOID
lwpmudrv_Clean_Up (
    DRV_BOOL finish
)
{
    U32  i;

    if (DRV_CONFIG_use_pcl(pcfg) == TRUE) {
        pcfg = CONTROL_Free_Memory(pcfg);
        goto signal_end;
    }

    if (PMU_register_data) {
        for (i = 0; i < GLOBAL_STATE_num_em_groups(driver_state); i++) {
            CONTROL_Free_Memory(PMU_register_data[i]);
        }
    }

    if (devices) {
        U32           id;
        EVENT_CONFIG  ec;
        DRV_CONFIG    pcfg_unc     = NULL;
        DISPATCH      dispatch_unc = NULL;

        for (id = 0; id < num_devices; id++) {
            pcfg_unc = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[id]);
            dispatch_unc = LWPMU_DEVICE_dispatch(&devices[id]);
            if (pcfg_unc && dispatch_unc && dispatch_unc->fini) {
                SEP_PRINT_DEBUG("LWP: calling UNC Init\n");
                dispatch_unc->fini((PVOID *)&i);
            }

            if (LWPMU_DEVICE_PMU_register_data(&devices[id])) {
                ec =  LWPMU_DEVICE_ec(&devices[id]);
                for (i = 0; i < EVENT_CONFIG_num_groups_unc(ec); i++) {
                    CONTROL_Free_Memory(LWPMU_DEVICE_PMU_register_data(&devices[id])[i]);
                }
                LWPMU_DEVICE_PMU_register_data(&devices[id]) = CONTROL_Free_Memory(LWPMU_DEVICE_PMU_register_data(&devices[id]));
            }
            LWPMU_DEVICE_pcfg(&devices[id]) = CONTROL_Free_Memory(LWPMU_DEVICE_pcfg(&devices[id]));
            LWPMU_DEVICE_ec(&devices[id])   = CONTROL_Free_Memory(LWPMU_DEVICE_ec(&devices[id]));

            if (LWPMU_DEVICE_acc_per_thread(&devices[id])) {
                for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
                    LWPMU_DEVICE_acc_per_thread(&devices[id])[i] = CONTROL_Free_Memory(LWPMU_DEVICE_acc_per_thread(&devices[id])[i]);
                }
                LWPMU_DEVICE_acc_per_thread(&devices[id]) = CONTROL_Free_Memory(LWPMU_DEVICE_acc_per_thread(&devices[id]));
            }
            if (LWPMU_DEVICE_prev_val_per_thread(&devices[id])) {
                for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
                    LWPMU_DEVICE_prev_val_per_thread(&devices[id])[i] = CONTROL_Free_Memory(LWPMU_DEVICE_prev_val_per_thread(&devices[id])[i]);
                }
                LWPMU_DEVICE_prev_val_per_thread(&devices[id]) = CONTROL_Free_Memory(LWPMU_DEVICE_prev_val_per_thread(&devices[id]));
            }
        }
        devices = CONTROL_Free_Memory(devices);
    }

    if (desc_data) {
        for (i = 0; i < GLOBAL_STATE_num_descriptors(driver_state); i++) {
            CONTROL_Free_Memory(desc_data[i]);
        }
    }
    PMU_register_data       = CONTROL_Free_Memory(PMU_register_data);
    desc_data               = CONTROL_Free_Memory(desc_data);
    global_ec               = CONTROL_Free_Memory(global_ec);
    pcfg                    = CONTROL_Free_Memory(pcfg);
    if (restore_bl_bypass) {
        restore_bl_bypass = CONTROL_Free_Memory(restore_bl_bypass);
    }
    if (restore_qpi_direct2core) {
        for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
              restore_qpi_direct2core[i]= CONTROL_Free_Memory(restore_qpi_direct2core[i]);
        }
        restore_qpi_direct2core = CONTROL_Free_Memory(restore_qpi_direct2core);
    }
    if (restore_ha_direct2core) {
        for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
              restore_ha_direct2core[i]= CONTROL_Free_Memory(restore_ha_direct2core[i]);
        }
        restore_ha_direct2core = CONTROL_Free_Memory(restore_ha_direct2core);
    }

    lbr                     = CONTROL_Free_Memory(lbr);
    if (finish) {
        CONTROL_Invoke_Parallel(dispatch->fini, NULL); // must be done before pcb is freed
    }
    pcb                     = CONTROL_Free_Memory(pcb);
    pcb_size                = 0;
    pmu_state               = CONTROL_Free_Memory(pmu_state);
    cpu_mask_bits           = CONTROL_Free_Memory(cpu_mask_bits);

signal_end:
    GLOBAL_STATE_num_em_groups(driver_state)   = 0;
    GLOBAL_STATE_num_descriptors(driver_state) = 0;
    num_devices                                = 0;
    abnormal_terminate                         = 0;
    control_pid                                = 0;

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Initialize (PVOID in_buf, size_t in_buf_len)
 *
 * @param  in_buf       - pointer to the input buffer
 * @param  in_buf_len   - size of the input buffer
 *
 * @return OS_STATUS
 *
 * @brief  Local function that handles the LWPMU_IOCTL_INIT call.
 * @brief  Sets up the interrupt handler.
 * @brief  Set up the output buffers/files needed to make the driver
 * @brief  operational.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Initialize (
    PVOID         in_buf,
    size_t        in_buf_len
)
{
    U32        initialize;
    S32        cpu_num;
    int        status    = OS_SUCCESS;
    S8        *seed_name = NULL;

    SEP_PRINT_DEBUG("lwpmudrv_Initialize: Entered lwpmudrv_Initialize\n");

    if (in_buf == NULL) {
        return OS_FAULT;
    }

    initialize = cmpxchg(&GLOBAL_STATE_current_phase(driver_state),
                         DRV_STATE_RESERVED,
                         DRV_STATE_IDLE);

    if (initialize != DRV_STATE_RESERVED) {
        SEP_PRINT_ERROR("lwpmudrv_Initialize: Sampling is in progress, cannot start a new session.\n");
        return OS_IN_PROGRESS;
    }

    /*
     *   Program State Initializations
     */
    pcfg = CONTROL_Allocate_Memory(in_buf_len);
    if (!pcfg) {
        return OS_NO_MEM;
    }

    if (copy_from_user(pcfg, in_buf, in_buf_len)) {
        return OS_FAULT;
    }

    if (DRV_CONFIG_enable_cp_mode(pcfg)) {
        interrupt_counts = CONTROL_Allocate_Memory(GLOBAL_STATE_num_cpus(driver_state) *
                                                   DRV_CONFIG_num_events(pcfg) *
                                                   sizeof(U64));
        if (interrupt_counts == NULL) {
            return OS_FAULT;    // no memory?
        }
    }

    if (DRV_CONFIG_use_pcl(pcfg) == TRUE) {
        return OS_SUCCESS;
    }

    pcb_size = GLOBAL_STATE_num_cpus(driver_state)*sizeof(CPU_STATE_NODE);
    pcb      = CONTROL_Allocate_Memory(pcb_size);
    if (!pcb) {
        return OS_NO_MEM;
    }

    pmu_state = CONTROL_Allocate_KMemory(GLOBAL_STATE_num_cpus(driver_state)*sizeof(U64)*2);
    if (!pmu_state) {
        return OS_NO_MEM;
    }
    uncore_em_factor = 0;
    for (cpu_num = 0; cpu_num < GLOBAL_STATE_num_cpus(driver_state); cpu_num++) {
        CPU_STATE_accept_interrupt(&pcb[cpu_num])   = 1;
        CPU_STATE_initial_mask(&pcb[cpu_num])       = 1;
        CPU_STATE_group_swap(&pcb[cpu_num])         = 1;
        CPU_STATE_reset_mask(&pcb[cpu_num])         = 0;
        CPU_STATE_num_samples(&pcb[cpu_num])        = 0;
#if (defined(DRV_IA32) || defined(DRV_EM64T))
        CPU_STATE_last_p_state_valid(&pcb[cpu_num]) = FALSE;
#endif
    }

    if (dispatch == NULL) {
        dispatch = UTILITY_Configure_CPU(DRV_CONFIG_dispatch_id(pcfg));
        if (dispatch == NULL) {
            SEP_PRINT_ERROR("lwpmudrv_Initialize: dispatch pointer is NULL!\n");
            return OS_INVALID;
        }
    }

    SEP_PRINT_DEBUG("Config : size = %d\n", DRV_CONFIG_size(pcfg));
    SEP_PRINT_DEBUG("Config : counting_mode = %ld\n", DRV_CONFIG_counting_mode(pcfg));
    SEP_PRINT_DEBUG("Config : pebs_mode = %ld\n", DRV_CONFIG_pebs_mode(pcfg));
    SEP_PRINT_DEBUG("Config : pebs_capture = %ld\n", DRV_CONFIG_pebs_capture(pcfg));
    SEP_PRINT_DEBUG("Config : collect_lbrs = %ld\n", DRV_CONFIG_collect_lbrs(pcfg));

    control_pid = current->pid;
    SEP_PRINT_DEBUG("Control PID = %d\n", control_pid);

    if (DRV_CONFIG_counting_mode(pcfg) == FALSE) {
        if (cpu_buf == NULL) {
            cpu_buf = CONTROL_Allocate_Memory(GLOBAL_STATE_num_cpus(driver_state)*sizeof(BUFFER_DESC_NODE));
            if (!cpu_buf) {
                return OS_NO_MEM;
            }
        }
        /*
         * Allocate the output and control buffers for each CPU in the system
         * Allocate and set up the temp output files for each CPU in the system
         * Allocate and set up the temp outout file for detailing the Modules in the system
         */
        seed_name = DRV_CONFIG_seed_name(pcfg);
        DRV_CONFIG_seed_name(pcfg) = CONTROL_Allocate_Memory(DRV_CONFIG_seed_name_len(pcfg));
        if (!DRV_CONFIG_seed_name(pcfg)) {
            SEP_PRINT_ERROR("lwpmudrv_Initialize: memory allocation for seed_name failed.");
            return OS_NO_MEM;
        }
        if (copy_from_user(DRV_CONFIG_seed_name(pcfg), seed_name, DRV_CONFIG_seed_name_len(pcfg))) {
            SEP_PRINT_ERROR("lwpmudrv_Initialize: copy_from_user for seed name failed.");
            return OS_FAULT;
        }
        status = OUTPUT_Initialize(DRV_CONFIG_seed_name(pcfg), DRV_CONFIG_seed_name_len(pcfg));
        if (status != OS_SUCCESS) {
            GLOBAL_STATE_current_phase(driver_state) = DRV_STATE_UNINITIALIZED;
            lwpmudrv_Clean_Up(FALSE);
            return status;
        }

#if defined(DRV_USE_NMI)
        status = OUTPUT_Initialize_Timers();
        if (status != OS_SUCCESS) {
            GLOBAL_STATE_current_phase(driver_state) = DRV_STATE_UNINITIALIZED;
            lwpmudrv_Clean_Up(FALSE);
            return status;
        }
#endif
        SEP_PRINT_DEBUG("Config : seed_name = %s\n", DRV_CONFIG_seed_name(pcfg));
        SEP_PRINT_DEBUG("lwpmudrv_Initialize: After OUTPUT_Initialize\n");

        /*
         * Program the APIC and set up the interrupt handler
         */
        CPUMON_Install_Cpuhooks();
        SEP_PRINT_DEBUG("lwpmudrv_Initialize: Finished Installing cpu hooks\n");
        /*
         * Set up the call back routines based on architecture.
         */
        PEBS_Initialize(pcfg);

#if defined(DRV_EM64T)
        SYS_Get_GDT_Base((PVOID*)&gdt_desc);
#endif
        SEP_PRINT_DEBUG("lwpmudrv_Initialize: about to install module notification");
        LINUXOS_Install_Hooks();
    }

    return status;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Initialize_Num_Devices(IOCTL_ARGS arg)
 *
 * @param arg - pointer to the IOCTL_ARGS structure
 *
 * @return OS_STATUS
 *
 * @brief
 * @brief  Local function that handles the LWPMU_IOCTL_INIT_NUM_DEV call.
 * @brief  Init # uncore devices.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Initialize_Num_Devices (
    IOCTL_ARGS arg
)
{
    // Check if enough space is provided for collecting the data
    if ((arg->w_len != sizeof(U32))  || (arg->w_buf == NULL)) {
        return OS_FAULT;
    }

    if (copy_from_user(&num_devices, arg->w_buf, arg->w_len)) {
        return OS_FAULT;
    }
    /*
     *   Allocate memory for number of devices
     */
    if (num_devices != 0) {
        devices = CONTROL_Allocate_Memory(num_devices * sizeof(LWPMU_DEVICE_NODE));
        if (!devices) {
            SEP_PRINT_ERROR("lwpmudrv_Initialize_Num_Devices: unable to allocate memory for devices\n");
            return OS_NO_MEM;
        }
    }
    cur_device = 0;

    SEP_PRINT_DEBUG("lwpmudrv_Initialize_Num_Devices: num_devices=%d, devices=0x%p\n", num_devices, devices);

    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Initialize_UNC(PVOID in_buf, U32 in_buf_len)
 *
 * @param  in_buf       - pointer to the input buffer
 * @param  in_buf_len   - size of the input buffer
 *
 * @return OS_STATUS
 *
 * @brief  Local function that handles the LWPMU_IOCTL_INIT call.
 * @brief  Sets up the interrupt handler.
 * @brief  Set up the output buffers/files needed to make the driver
 * @brief  operational.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Initialize_UNC (
    PVOID         in_buf,
    U32           in_buf_len
)
{
    DRV_CONFIG  pcfg;

    SEP_PRINT_DEBUG("Entered lwpmudrv_Initialize_UNC\n");

    if (!devices) {
        SEP_PRINT_ERROR("lwpmudrv_Initialize_UNC: No devices allocated!");
        return OS_INVALID;
    }

    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_IDLE) {
        return OS_IN_PROGRESS;
    }
    /*
     *   Program State Initializations:
     *   Foreach device, copy over pcfg and configure dispatch table
     */
    if (cur_device >= num_devices) {
        SEP_PRINT_ERROR("No more devices to allocate!  Initial call to lwpmudrv_Init_Num_Devices incorrect.");
        return OS_FAULT;
    }
    if (in_buf == NULL) {
        return OS_FAULT;
    }
    if (in_buf_len != sizeof(DRV_CONFIG_NODE)) {
        SEP_PRINT_ERROR("Got in_buf_len=%d, expecting size=%d\n", in_buf_len, (int)sizeof(DRV_CONFIG_NODE));
        return OS_FAULT;
    }
    // allocate memory
    LWPMU_DEVICE_pcfg(&devices[cur_device]) = CONTROL_Allocate_Memory(sizeof(DRV_CONFIG_NODE));
    // copy over pcfg
    if (copy_from_user(LWPMU_DEVICE_pcfg(&devices[cur_device]), in_buf, in_buf_len)) {
        SEP_PRINT_ERROR("lwpmudrv_Initialize_UNC: Failed to copy from user");
        return OS_FAULT;
    }
    // configure dispatch from dispatch_id
    pcfg = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[cur_device]);
    if (!pcfg) {
        return OS_INVALID;
    }
    LWPMU_DEVICE_dispatch(&devices[cur_device]) = UTILITY_Configure_CPU(DRV_CONFIG_dispatch_id(pcfg));
    if (LWPMU_DEVICE_dispatch(&devices[cur_device]) == NULL) {
        SEP_PRINT_ERROR("Unable to configure CPU");
        return OS_FAULT;
    }

    LWPMU_DEVICE_em_groups_count(&devices[cur_device]) = 0;
    LWPMU_DEVICE_num_units(&devices[cur_device])       = 0;
    LWPMU_DEVICE_cur_group(&devices[cur_device])       = 0;

    SEP_PRINT_DEBUG("LWP: ebc unc = %d\n",DRV_CONFIG_event_based_counts(pcfg));
    SEP_PRINT_DEBUG("LWP Config : unc dispatch id   = %d\n", DRV_CONFIG_dispatch_id(pcfg));

    return OS_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Terminate(void)
 *
 * @param  none
 *
 * @return OS_STATUS
 *
 * @brief  Local function that handles the LWPMUDRV_IOCTL_TERMINATE call.
 * @brief  Cleans up the interrupt handler and resets the PMU state.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Terminate (
    VOID
)
{
    U32            previous_state;

    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_UNINITIALIZED) {
        return OS_SUCCESS;
    }

    previous_state = cmpxchg(&GLOBAL_STATE_current_phase(driver_state),
                             DRV_STATE_STOPPED,
                             DRV_STATE_UNINITIALIZED);
    if (previous_state != DRV_STATE_STOPPED) {
        SEP_PRINT_ERROR("lwpmudrv_Terminate: Sampling is in progress, cannot terminate.\n");
        return OS_IN_PROGRESS;
    }

    GLOBAL_STATE_current_phase(driver_state) = DRV_STATE_UNINITIALIZED;
    lwpmudrv_Clean_Up(TRUE);

    return OS_SUCCESS;
}

#ifdef EMON
/* ------------------------------------------------------------------------- */
/*!
 * @fn static void lwpmudrv_Switch_To_Next_Group(param)
 *
 * @param  none
 *
 * @return none
 *
 * @brief  Switch to the next event group for both core and uncore.
 * @brief  This function assumes an active collection is frozen
 * @brief  or no collection is active.
 *
 * <I>Special Notes</I>
 */
static VOID
lwpmudrv_Switch_To_Next_Group (
    VOID
)
{
    S32            cpuid;
    U32            i;
    CPU_STATE      pcpu;
    DRV_CONFIG     pcfg_unc;
    DISPATCH       dispatch_unc;

    for (cpuid = 0; cpuid < GLOBAL_STATE_num_cpus(driver_state); cpuid++) {
        pcpu     = &pcb[cpuid];
        CPU_STATE_current_group(pcpu)++;
        // make the event group list circular
        CPU_STATE_current_group(pcpu) %= EVENT_CONFIG_num_groups(global_ec);
    }

    if (num_devices) {
        for(i = 0; i < num_devices; i++) {
            pcfg_unc     = LWPMU_DEVICE_pcfg(&devices[i]);
            dispatch_unc = LWPMU_DEVICE_dispatch(&devices[i]);
            if (pcfg_unc && dispatch_unc) {
                LWPMU_DEVICE_cur_group(&devices[i])++;
                LWPMU_DEVICE_cur_group(&devices[i]) %= LWPMU_DEVICE_em_groups_count(&devices[i]);
            }
        }
    }

    return;
}
#endif

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwmpudrv_Get_Driver_State(IOCTL_ARGS arg)
 *
 * @param arg - pointer to the IOCTL_ARGS structure
 *
 * @return OS_STATUS
 *
 * @brief  Local function that handles the LWPMU_IOCTL_GET_Driver_State call.
 * @brief  Returns the current driver state.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Get_Driver_State (
    IOCTL_ARGS    arg
)
{
    OS_STATUS  status = OS_SUCCESS;

    // Check if enough space is provided for collecting the data
    if ((arg->r_len != sizeof(U32)) || (arg->r_buf == NULL)) {
        return OS_FAULT;
    }

    status = put_user(GLOBAL_STATE_current_phase(driver_state), (U32*)arg->r_buf);

    return status;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_Pause_Op(void)
 *
 * @param - none
 *
 * @return OS_STATUS
 *
 * @brief Pause the core/uncore collection
 *
 * <I>Special Notes</I>
 */
static VOID
lwpmudrv_Pause_Op (
    PVOID param
)
{
    U32 i;
    DRV_CONFIG pcfg_unc = NULL;
    DISPATCH   dispatch_unc = NULL;

    if (dispatch != NULL && dispatch->freeze != NULL &&
        DRV_CONFIG_use_pcl(pcfg) == FALSE) {
        dispatch->freeze(param);
    }

    for (i = 0; i < num_devices; i++) {
         pcfg_unc = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[i]);
         dispatch_unc = LWPMU_DEVICE_dispatch(&devices[i]);

         if (pcfg_unc                                &&
             (DRV_CONFIG_event_based_counts(pcfg_unc) || DRV_CONFIG_emon_mode(pcfg_unc)) &&
             dispatch_unc                            &&
             dispatch_unc->freeze) {
                SEP_PRINT_DEBUG("LWP: calling UNC Pause\n");

                dispatch_unc->freeze(&i);
         }
    }
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_Pause(void)
 *
 * @param - none
 *
 * @return OS_STATUS
 *
 * @brief Pause the collection
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Pause (
    VOID
)
{
    U32  previous_state;
    int  i;
    int  done = FALSE;

    if (!pcb || !pcfg) {
        SEP_PRINT_ERROR("lwpmudrv_Pause: pcb or pcfg pointer is NULL!");
        return OS_INVALID;
    }

    previous_state = cmpxchg(&GLOBAL_STATE_current_phase(driver_state),
                             DRV_STATE_RUNNING,
                             DRV_STATE_PAUSING);

    if (previous_state == DRV_STATE_RUNNING) {
        if (DRV_CONFIG_use_pcl(pcfg) == FALSE) {
            for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
                CPU_STATE_accept_interrupt(&pcb[i]) = 0;
            }
            while (!done) {
                done = TRUE;
                for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
                    if (atomic_read(&CPU_STATE_in_interrupt(&pcb[i]))) {
                        done = FALSE;
                    }
                }
            }
        }
        CONTROL_Invoke_Parallel(lwpmudrv_Pause_Op, NULL);
        /*
         * This means that the PAUSE state has been reached.
         */
        GLOBAL_STATE_current_phase(driver_state) = DRV_STATE_PAUSED;
    }

    return OS_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_Resume_Op(void)
 *
 * @param - none
 *
 * @return OS_STATUS
 *
 * @brief Resume the core/uncore collection
 *
 * <I>Special Notes</I>
 */
static VOID
lwpmudrv_Resume_Op (
    PVOID param
)
{
    U32 i;
    DRV_CONFIG pcfg_unc = NULL;
    DISPATCH   dispatch_unc = NULL;

    if (dispatch != NULL && dispatch->restart != NULL &&
        DRV_CONFIG_use_pcl(pcfg) == FALSE) {
        dispatch->restart((VOID *)(size_t)0);
    }

    for (i = 0; i < num_devices; i++) {
         pcfg_unc = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[i]);
         dispatch_unc = LWPMU_DEVICE_dispatch(&devices[i]);

         if (pcfg_unc                                &&
             (DRV_CONFIG_event_based_counts(pcfg_unc) || DRV_CONFIG_emon_mode(pcfg_unc)) &&
             dispatch_unc                            &&
             dispatch_unc->restart) {
                SEP_PRINT_DEBUG("LWP: calling UNC Resume\n");

                dispatch_unc->restart(&i);
         }
    }
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_Resume(void)
 *
 * @param - none
 *
 * @return OS_STATUS
 *
 * @brief Resume the collection
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Resume (
    VOID
)
{
    U32        previous_state;
    int        i;

    if (!pcb || !pcfg) {
        SEP_PRINT_ERROR("lwpmudrv_Resume: pcb or pcfg pointer is NULL!");
        return OS_INVALID;
    }

    /*
     * If we are in the process of pausing sampling, wait until the pause has been
     * completed.  Then start the Resume process.
     */
    do {
        previous_state = cmpxchg(&GLOBAL_STATE_current_phase(driver_state),
                                 DRV_STATE_PAUSED,
                                 DRV_STATE_RUNNING);
        /*
         *  This delay probably needs to be expanded a little bit more for large systems.
         *  For now, it is probably sufficient.
         */
        SYS_IO_Delay();
        SYS_IO_Delay();
    } while (previous_state == DRV_STATE_PAUSING);

    if (previous_state == DRV_STATE_PAUSED) {
        for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
            if (cpu_mask_bits) {
                CPU_STATE_accept_interrupt(&pcb[i]) = cpu_mask_bits[i] ? 1 : 0;
                CPU_STATE_group_swap(&pcb[i])       = 1;
            }
            else {
                CPU_STATE_accept_interrupt(&pcb[i]) = 1;
                CPU_STATE_group_swap(&pcb[i])       = 1;
            }
        }
        CONTROL_Invoke_Parallel(lwpmudrv_Resume_Op, NULL);
    }

    return OS_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Switch_Group(void)
 *
 * @param none
 *
 * @return OS_STATUS
 *
 * @brief Switch the current group that is being collected.
 *
 * <I>Special Notes</I>
 *     This routine is called from the user mode code to handle the multiple group
 *     situation.  4 distinct steps are taken:
 *     Step 1: Pause the sampling
 *     Step 2: Increment the current group count
 *     Step 3: Write the new group to the PMU
 *     Step 4: Resume sampling
 */
static OS_STATUS
lwpmudrv_Switch_Group (
    VOID
)
{
    S32            idx;
    CPU_STATE      pcpu;
    OS_STATUS      status        = OS_SUCCESS;
    U32            current_state = GLOBAL_STATE_current_phase(driver_state);
	
    if (!pcb || !pcfg) {
        SEP_PRINT_ERROR("lwpmudrv_Initialize_UNC: NULL pointer");
        return OS_INVALID;
    }

    if (current_state != DRV_STATE_RUNNING &&
        current_state != DRV_STATE_PAUSED) {
        return status;
    }
    status = lwpmudrv_Pause();
    for (idx = 0; idx < GLOBAL_STATE_num_cpus(driver_state); idx++) {
        pcpu = &pcb[idx];
        CPU_STATE_current_group(pcpu)++;
    }
    CONTROL_Invoke_Parallel(dispatch->write, (VOID *)0);
    if (pcfg && (DRV_CONFIG_start_paused(pcfg) == FALSE)) {
        lwpmudrv_Resume();
    }

    return status;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_Write_Uncore(void)
 *
 * @param - none
 *
 * @return OS_STATUS
 *
 * @brief Program the uncore collection
 *
 * <I>Special Notes</I>
 */
static VOID
lwpmudrv_Write_Uncore (
    PVOID param
)
{
    U32 i;
    DRV_CONFIG pcfg_unc = NULL;
    DISPATCH   dispatch_unc = NULL;

    for (i = 0; i < num_devices; i++) {
         pcfg_unc = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[i]);
         dispatch_unc = LWPMU_DEVICE_dispatch(&devices[i]);

         if (pcfg_unc && dispatch_unc && dispatch_unc->write) {
                SEP_PRINT_DEBUG("LWP: calling UNC Write\n");
                dispatch_unc->write(&i);
         }
    }
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_Write_Op(void)
 *
 * @param - none
 *
 * @return OS_STATUS
 *
 * @brief Program the core/uncore collection
 *
 * <I>Special Notes</I>
 */
static VOID
lwpmudrv_Write_Op (
    PVOID param
)
{
    if (dispatch != NULL && dispatch->write != NULL) {
        dispatch->write((VOID *)(size_t)0);
    }

    lwpmudrv_Write_Uncore(param);
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Uncore_Switch_Group(void)
 *
 * @param none
 *
 * @return OS_STATUS
 *
 * @brief Switch the current group that is being collected.
 *
 * <I>Special Notes</I>
 *     This routine is called from the user mode code to handle the multiple group
 *     situation.  4 distinct steps are taken:
 *     Step 1: Pause the sampling
 *     Step 2: Increment the current group count
 *     Step 3: Write the new group to the PMU
 *     Step 4: Resume sampling
 */
static OS_STATUS
lwpmudrv_Uncore_Switch_Group (
    VOID
)
{
    OS_STATUS      status        = OS_SUCCESS;
    U32            current_state = GLOBAL_STATE_current_phase(driver_state);
    U32            i             = 0;
    DRV_CONFIG     pcfg_unc;
    DISPATCH       dispatch_unc;

    if (!devices || !pcfg) {
        SEP_PRINT_ERROR("lwpmudrv_Uncore_Switch_Group: devices or pcfg pointer is NULL!");
        return OS_INVALID;
    }

    if (current_state != DRV_STATE_RUNNING &&
        current_state != DRV_STATE_PAUSED) {
        return status;
    }

    status = lwpmudrv_Pause();

    if (num_devices) {
        for(i = 0; i < num_devices; i++) {
            pcfg_unc     = LWPMU_DEVICE_pcfg(&devices[i]);
            dispatch_unc = LWPMU_DEVICE_dispatch(&devices[i]);
            if (pcfg_unc && dispatch_unc) {
                LWPMU_DEVICE_cur_group(&devices[i])++;
                LWPMU_DEVICE_cur_group(&devices[i]) %= LWPMU_DEVICE_em_groups_count(&devices[i]);
            }
        }
        CONTROL_Invoke_Parallel(lwpmudrv_Write_Uncore, NULL);
    }

    if (pcfg && (DRV_CONFIG_start_paused(pcfg) == FALSE)) {
        lwpmudrv_Resume();
    }

    return status;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_Trigger_Read(void)
 *
 * @param - none
 *
 * @return - OS_STATUS
 *
 * @brief Read the Counter Data.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Trigger_Read (
    VOID
)
{
    DRV_CONFIG pcfg_unc     = NULL;
    DISPATCH   dispatch_unc = NULL;
    U32        i;

    if (cs_dispatch && cs_dispatch->Trigger_Read) {
        cs_dispatch->Trigger_Read();
    }

    if (pcfg && DRV_CONFIG_use_pcl(pcfg) == TRUE) {
        return OS_SUCCESS;
    }

    for (i = 0; i < num_devices; i++) {
        pcfg_unc = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[i]);
        dispatch_unc = LWPMU_DEVICE_dispatch(&devices[i]);
        if (pcfg_unc  && dispatch_unc && dispatch_unc->trigger_read ) {
            dispatch_unc->trigger_read();
        }

        if (LWPMU_DEVICE_em_groups_count(&devices[i]) > 1) {
            uncore_em_factor++;
            if (uncore_em_factor == UNCORE_EM_GROUP_SWAP_FACTOR) {
                lwpmudrv_Uncore_Switch_Group();
                uncore_em_factor = 0;
            }
        }
    }

    return OS_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn static void lwmudrv_Read_Specific_TSC (PVOID param)
 *
 * @param param - pointer to the result
 *
 * @return none
 *
 * @brief  Read the tsc value in the current processor and
 * @brief  write the result into param.
 *
 * <I>Special Notes</I>
 */
static VOID
lwpmudrv_Read_Specific_TSC (
    PVOID  param
)
{
    U32 this_cpu;

    preempt_disable();
    this_cpu = CONTROL_THIS_CPU();
    if (this_cpu == 0) {
        UTILITY_Read_TSC((U64*)param);
    }
    preempt_enable();

    return;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_Init_PMU(void)
 *
 * @param - none
 *
 * @return - OS_STATUS
 *
 * @brief Initialize the PMU and the driver state in preparation for data collection.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Init_PMU (
    IOCTL_ARGS args
)
{
    DRV_CONFIG pcfg_unc = NULL;
    DISPATCH   dispatch_unc = NULL;
    U32        i;
    U32        emon_data_buffer_size  = 0;
    S32        max_groups_unc;
    OS_STATUS  status                 = OS_SUCCESS;

    if (args->w_len == 0 || args->w_buf == NULL) {
        return OS_NO_MEM;
    }

    if (copy_from_user(&emon_data_buffer_size, args->w_buf, sizeof(U32))) {
         SEP_PRINT_ERROR("lwpmudrv_Init_PMU: copy_from_user() failed.\n");
         return OS_FAULT;
    }
    prev_counter_size     = emon_data_buffer_size;

    if (!pcfg) {
        return OS_FAULT;
    }
    if (DRV_CONFIG_use_pcl(pcfg) == TRUE) {
        return OS_SUCCESS;
    }

    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_IDLE) {
        return OS_IN_PROGRESS;
    }

    if (GLOBAL_STATE_num_em_groups(driver_state) == 0) {
        SEP_PRINT_ERROR("Number of em groups is not set.\n");
        return OS_SUCCESS;
    }

    // set cur_device's total groups to max groups of all devices
    max_groups_unc = 0;
    for (i = 0; i < num_devices; i++) {
        if (max_groups_unc < LWPMU_DEVICE_em_groups_count(&devices[i])) {
            max_groups_unc = LWPMU_DEVICE_em_groups_count(&devices[i]);
        }
    }
    // now go back and up total groups for all devices
    for (i = 0; i < num_devices; i++) {
        if (LWPMU_DEVICE_em_groups_count(&devices[i]) < max_groups_unc) {
            LWPMU_DEVICE_em_groups_count(&devices[i]) = max_groups_unc;
        }
    }

    // allocate save/restore space before program the PMU
    lwpmudrv_Allocate_Restore_Buffer();

    // must be done after pcb is created and before PMU is first written to
    CONTROL_Invoke_Parallel(dispatch->init, NULL);


    for (i = 0; i < num_devices; i++) {
        pcfg_unc     = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[i]);
        dispatch_unc = LWPMU_DEVICE_dispatch(&devices[i]);
        if (pcfg_unc && dispatch_unc && dispatch_unc->init) {
            dispatch_unc->init((VOID *)&i);
        }
    }    

    //
    // Transfer the data into the PMU registers
    //
    CONTROL_Invoke_Parallel(lwpmudrv_Write_Op, NULL);

    SEP_PRINT_DEBUG("lwpmudrv_Init_PMU: IOCTL_Init_PMU - finished initial Write\n");

    if (DRV_CONFIG_counting_mode(pcfg) == TRUE || DRV_CONFIG_emon_mode(pcfg) == TRUE) {
        if (!read_counter_info) {
            read_counter_info = CONTROL_Allocate_Memory(emon_data_buffer_size);
            if (!read_counter_info) {
                return OS_NO_MEM;
            }
        }
        if (!prev_counter_data) {
            prev_counter_data = CONTROL_Allocate_Memory(emon_data_buffer_size);
            if (!prev_counter_data) {
                read_counter_info = CONTROL_Free_Memory(read_counter_info);
                return OS_NO_MEM;
            }
        }
    }

    return status;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn static void lwpmudrv_Read_MSR(pvoid param)
 *
 * @param param - pointer to the buffer to store the MSR counts
 *
 * @return none
 *
 * @brief
 * @brief  Read the U64 value at address in in_buf and
 * @brief  write the result into out_buf.
 *
 * <I>Special Notes</I>
 */
static VOID
lwpmudrv_Read_MSR (
    PVOID param
)
{
    U32       this_cpu;
    MSR_DATA  this_node;
    S64       reg_num;

    preempt_disable();
    this_cpu  = CONTROL_THIS_CPU();
    this_node = &msr_data[this_cpu];
    reg_num = MSR_DATA_addr(this_node);

    if (reg_num == 0) {
      preempt_enable();
      return;
    }

    MSR_DATA_value(this_node) = (U64)SYS_Read_MSR((U32)MSR_DATA_addr(this_node));
    preempt_enable();

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_Read_MSR_All_Cores(IOCTL_ARGS arg)
 *
 * @param arg - pointer to the IOCTL_ARGS structure
 *
 * @return OS_STATUS
 *
 * @brief  Read the U64 value at address into in_buf and write
 * @brief  the result into out_buf.
 * @brief  Returns OS_SUCCESS if the read across all cores succeed,
 * @brief  otherwise OS_FAULT.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Read_MSR_All_Cores (
    IOCTL_ARGS    arg
)
{
    U64            *val;
    S32             reg_num;
    S32             i;
    MSR_DATA        node;

    if ((arg->r_len != sizeof(U32))  || (arg->r_buf == NULL) || (arg->w_buf == NULL)) {
        return OS_FAULT;
    }

    val     = (U64 *)arg->w_buf;

    if (val == NULL)  {
        SEP_PRINT_ERROR("NULL out_buf\n");
        return OS_SUCCESS;
    }
    if (copy_from_user(&reg_num, arg->r_buf, sizeof(U32))) {
        SEP_PRINT_ERROR("lwpmudrv_Read_MSR_All_Cores: copy_from_user failed.\n");
        return OS_FAULT;
    }
    msr_data = CONTROL_Allocate_Memory(GLOBAL_STATE_num_cpus(driver_state)*sizeof(MSR_DATA_NODE));
    if (!msr_data) {
        return OS_NO_MEM;
    }

    for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
        node                = &msr_data[i];
        MSR_DATA_addr(node) = reg_num;
    }

    CONTROL_Invoke_Parallel(lwpmudrv_Read_MSR, (VOID *)(size_t)0);

    /* copy values to arg array? */
    if (arg->w_len < GLOBAL_STATE_num_cpus(driver_state)) {
        SEP_PRINT_ERROR("Not enough memory allocated in output buffer.\n");
        msr_data = CONTROL_Free_Memory(msr_data);
        return OS_SUCCESS;
    }
    for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
        node = &msr_data[i];
        if (copy_to_user(&val[i], (U64*)&MSR_DATA_value(node), sizeof(U64))) {
            return OS_FAULT;
        }
    }

    msr_data = CONTROL_Free_Memory(msr_data);

    return OS_SUCCESS;
}

#ifdef EMON
#endif

/* ------------------------------------------------------------------------- */
/*!
 * @fn static void lwpmudrv_Read_Data_Op(PVOID param)
 *
 * @param param   - dummy
 *
 * @return void
 *
 * @brief  Read all the core/uncore data counters at one shot
 *
 * <I>Special Notes</I>
 */
static void
lwpmudrv_Read_Data_Op (
    VOID*    param
)
{
    DISPATCH    dispatch_unc;
    U32         dev_idx;
    DRV_CONFIG  pcfg_unc;

    if (dispatch != NULL && dispatch->read_data != NULL) {
        dispatch->read_data(param);
    }

    if (devices == NULL) {
        return;
    }
    for (dev_idx = 0; dev_idx < num_devices; dev_idx++) {
        pcfg_unc      = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[dev_idx]);
        if (pcfg_unc == NULL) {
            continue;
        }
        if (!(DRV_CONFIG_emon_mode(pcfg_unc) || DRV_CONFIG_counting_mode(pcfg_unc))) {
            continue;
        }
        dispatch_unc  = LWPMU_DEVICE_dispatch(&devices[dev_idx]);
        if (dispatch_unc == NULL) {
            continue;
        }
        if (dispatch_unc->read_data == NULL) {
            continue;
        }
        dispatch_unc->read_data((VOID*)&dev_idx);
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_Read_MSRs(IOCTL_ARG arg)
 *
 * @param arg - pointer to the IOCTL_ARGS structure
 *
 * @return OS_STATUS
 *
 * @brief  Read all the programmed data counters and accumulate them
 * @brief  into a single buffer.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Read_MSRs (
    IOCTL_ARGS    arg
)
{
    if (arg->r_len == 0 || arg->r_buf == NULL ) {
        return OS_SUCCESS;
    }
    //
    // Transfer the data in the PMU registers to the output buffer
    //
    if (!read_counter_info) {
        read_counter_info = CONTROL_Allocate_Memory(arg->r_len);
        if (!read_counter_info) {
            return OS_NO_MEM;
        }
    }
    if (!prev_counter_data) {
        prev_counter_data = CONTROL_Allocate_Memory(arg->r_len);
        if (!prev_counter_data) {
            read_counter_info = CONTROL_Free_Memory(read_counter_info);
            return OS_NO_MEM;
        }
    }
    memset(read_counter_info, 0, arg->r_len);

    CONTROL_Invoke_Parallel(lwpmudrv_Read_Data_Op, NULL);

    if (copy_to_user(arg->r_buf, read_counter_info, arg->r_len)) {
        return OS_FAULT;
    }

    return OS_SUCCESS;
}

#ifdef EMON
/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Read_Counters_And_Switch_Group(IOCTL_ARGS arg)
 *
 * @param arg - pointer to the IOCTL_ARGS structure
 *
 * @return OS_STATUS
 *
 * @brief  Read / Store the counters and switch to the next valid group.
 *
 * <I>Special Notes</I>
 *     This routine is called from the user mode code to handle the multiple group
 *     situation.  11 distinct steps are taken:
 *     Step 1: Save the previously collected CPU 0 TSC value
 *     Step 2: Read CPU 0's TSC
 *     Step 3: Pause the counting PMUs
 *     Step 4: Calculate the difference between the TSCs and save in the first U64 of the buffer
 *     Step 5: Save previous buffer ptr and Increment the buffer
 *     Step 6: Read the currently programmed data PMUs and copy the data into the output buffer
 *             Restore the old buffer ptr.
 *     Step 7: Increment the current group count
 *     Step 8: Write the new group to the PMU
 *     Step 9: Reread prev_tsc value for next collection (so read MSRs time not included in report)
 *     Step 10: Resume the counting PMUs
 *
 */
static OS_STATUS
lwpmudrv_Read_Counters_And_Switch_Group (
    IOCTL_ARGS arg
)
{
    U64            prev_tsc, diff;
    U64           *pBuffer;
    char          *orig_r_buf_ptr;
    OS_STATUS      status        = OS_SUCCESS;

    if (arg->r_buf == NULL || arg->r_len == 0) {
        return OS_FAULT;
    }
    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_RUNNING) {
        return OS_SUCCESS;
    }
    // step 1
    prev_tsc = cpu0_TSC;

    // step 2
    // read CPU 0's tsc into the global var cpu0_TSC
    // if running on cpu 0, read the tsc directly, else schedule a dpc
    CONTROL_Invoke_Cpu (0, lwpmudrv_Read_Specific_TSC, &cpu0_TSC);

    // step 3
    // Counters should be frozen right after time stamped.
    status = lwpmudrv_Pause();

    // step 4
    // get tsc diff (i.e. clocks during this monitor interval)
    diff = cpu0_TSC - prev_tsc;

    // save diff in first slot in buffer
    pBuffer = (U64*)(arg->r_buf);
    if (copy_to_user(arg->r_buf, &diff, sizeof(U64))) {
        status = OS_FAULT;
    }

    // step 5
    orig_r_buf_ptr = arg->r_buf;
    pBuffer += 1;
    arg->r_buf = (char *)pBuffer;
    arg->r_len -= sizeof(U64);

    // step 6
    status = lwpmudrv_Read_MSRs(arg);
    arg->r_buf = orig_r_buf_ptr;
    arg->r_len += sizeof(U64);

    // step 7
    // for each processor, increment its current group number
    lwpmudrv_Switch_To_Next_Group();

    // step 8
    CONTROL_Invoke_Parallel(lwpmudrv_Write_Op, NULL);

    // step 9
    // reset cpu0_TSC for next collection interval
    CONTROL_Invoke_Cpu (0, lwpmudrv_Read_Specific_TSC, &cpu0_TSC);

    // step 10
    status = lwpmudrv_Resume();

    return status;
}

/*
 * @fn  static OS_STATUS lwpmudrv_Read_And_Reset_Counters(IOCTL_ARGS arg)
 *
 * @param arg - pointer to the IOCTL_ARGS structure
 *
 * @return OS_STATUS
 *
 * @brief  Read the current value of the counters, and reset them all to 0.
 *
 * <I>Special Notes</I>
 *     This routine is called from the user mode code to handle overflows
 *     It basically does the same as the lwpmudrv_Read_Counters_And_Switch_Group
 *     routine except it doesn't switch groups.
 *     Step 1: Save the previously collected CPU 0 TSC value
 *     Step 2: Read CPU 0's TSC
 *     Step 3: Pause the counting PMUs
 *     Step 4: Calculate the difference between the TSCs and save in the first U64 of the buffer
 *     Step 5: Save previous buffer ptr and Increment the buffer
 *     Step 6: Read the currently programmed data PMUs and copy the data into the output buffer
 *             Restore the old buffer ptr.
 *     Step 7: Write the new group to the PMU
 *     Step 8: Reread prev_tsc value for next collection (so read MSRs time not included in report)
 *     Step 9: Resume the counting PMUs
 */
static OS_STATUS
lwpmudrv_Read_And_Reset_Counters (
    IOCTL_ARGS arg
)
{
    U64            prev_tsc, diff;
    U64           *p_buffer;
    char          *orig_r_buf_ptr;
    OS_STATUS      status        = OS_SUCCESS;

    if (arg->r_buf == NULL || arg->r_len == 0) {
        return OS_FAULT;
    }
    // step 1
    prev_tsc = cpu0_TSC;

    // step 2
    // read CPU 0's tsc into the global var cpu0_TSC
    // if running on cpu 0, read the tsc directly, else schedule a dpc
    CONTROL_Invoke_Cpu (0, lwpmudrv_Read_Specific_TSC, &cpu0_TSC);

    // step 3
    // Counters should be frozen right after time stamped.
    status = lwpmudrv_Pause();

    // step 4
    // get tsc diff (i.e. clocks during this monitor interval)
    diff = cpu0_TSC - prev_tsc;

    // save diff in first slot in buffer
    p_buffer = (U64*)(arg->r_buf);
    if (copy_to_user(arg->r_buf, &diff, sizeof(U64))) {
        status = OS_FAULT;
    }

    // step 5
    orig_r_buf_ptr = arg->r_buf;
    p_buffer += 1;
    arg->r_buf = (char *)p_buffer;
    arg->r_len -= sizeof(U64);
    if (arg->r_buf == NULL || arg->r_len == 0) {
        return OS_INVALID;
    }

    // step 6
    status = lwpmudrv_Read_MSRs(arg);
    arg->r_buf = orig_r_buf_ptr;
    arg->r_len += sizeof(U64);

    if (arg->r_buf == NULL || arg->r_len == 0) {
        return OS_INVALID;
    }

    // step 7
    CONTROL_Invoke_Parallel(lwpmudrv_Write_Op, NULL);

    // step 8
    // reset cpu0_TSC for next collection interval
    CONTROL_Invoke_Cpu (0, lwpmudrv_Read_Specific_TSC, &cpu0_TSC);

    // step 9 
    status = lwpmudrv_Resume();

    return status;
}
#endif

/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_Set_Num_EM_Groups(IOCTL_ARGS arg)
 *
 * @param arg - pointer to the IOCTL_ARGS structure
 *
 * @return OS_STATUS
 *
 * @brief Configure the event multiplexing group.
 *
 * <I>Special Notes</I>
 *     None
 */
static OS_STATUS
lwpmudrv_Set_EM_Config (
    IOCTL_ARGS arg
)
{
    S32  cpu_num;

    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_IDLE) {
        return OS_IN_PROGRESS;
    }
    if (arg->w_buf == NULL) {
        SEP_PRINT_DEBUG("lwpmudrv_Set_EM_Config: set_num_em_groups got null pointer\n");
        return OS_NO_MEM;
    }
    if (arg->w_len != sizeof(EVENT_CONFIG_NODE)) {
        SEP_PRINT_ERROR("lwpmudrv_Set_EM_Config: Unknown size of value passed to IOCTL_EM_CONFIG: %lld\n", arg->w_len);
        return OS_NO_MEM;
    }
    global_ec = CONTROL_Allocate_Memory(sizeof(EVENT_CONFIG_NODE));
    if (!global_ec) {
        return OS_NO_MEM;
    }

    if (copy_from_user(global_ec, arg->w_buf, sizeof(EVENT_CONFIG_NODE))) {
        return OS_FAULT;
    }
    for (cpu_num = 0; cpu_num < GLOBAL_STATE_num_cpus(driver_state); cpu_num++) {
        CPU_STATE_trigger_count(&pcb[cpu_num])     = EVENT_CONFIG_em_factor(global_ec);
        CPU_STATE_trigger_event_num(&pcb[cpu_num]) = EVENT_CONFIG_em_event_num(global_ec) ;
    }
    GLOBAL_STATE_num_em_groups(driver_state) = EVENT_CONFIG_num_groups(global_ec);
    PMU_register_data = CONTROL_Allocate_Memory(GLOBAL_STATE_num_em_groups(driver_state) *
                                                sizeof(VOID *));
    EVENTMUX_Initialize(global_ec);

    em_groups_count = 0;

    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_Set_EM_Config_UNC(IOCTL_ARGS arg)
 *
 * @param arg - pointer to the IOCTL_ARGS structure
 *
 * @return OS_STATUS
 *
 * @brief  Set the number of em groups in the global state node.
 * @brief  Also, copy the EVENT_CONFIG struct that has been passed in,
 * @brief  into a global location for now.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Set_EM_Config_UNC (
    IOCTL_ARGS arg
)
{
    EVENT_CONFIG    ec;
    SEP_PRINT_DEBUG("enter lwpmudrv_Set_EM_Config_UNC\n");
    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_IDLE) {
        return OS_IN_PROGRESS;
    }

    SEP_PRINT_DEBUG("Num Groups UNCORE: %d\n", EVENT_CONFIG_num_groups_unc(global_ec));
    // allocate memory
    LWPMU_DEVICE_ec(&devices[cur_device]) = CONTROL_Allocate_Memory(sizeof(EVENT_CONFIG_NODE));
    if (copy_from_user(LWPMU_DEVICE_ec(&devices[cur_device]), arg->w_buf, arg->w_len)) {
        return OS_FAULT;
    }
    // configure num_groups from ec of the specific device
    ec = (EVENT_CONFIG)LWPMU_DEVICE_ec(&devices[cur_device]);
    LWPMU_DEVICE_PMU_register_data(&devices[cur_device]) = CONTROL_Allocate_Memory(EVENT_CONFIG_num_groups_unc(ec) *
                                                                                   sizeof(VOID *));

    LWPMU_DEVICE_em_groups_count(&devices[cur_device]) = 0;

    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_Configure_events(IOCTL_ARGS arg)
 *
 * @param arg - pointer to the IOCTL_ARGS structure
 *
 * @return OS_STATUS
 *
 * @brief  Copies one group of events into kernel space at
 * @brief  PMU_register_data[em_groups_count].
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Configure_Events (
    IOCTL_ARGS arg
)
{
    U32 group_id = 0;
    ECB ecb      = NULL;

    SEP_PRINT_DEBUG("lwpmudrv_Configure_Events: entered.\n");
    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_IDLE) {
        return OS_IN_PROGRESS;
    }

    if (em_groups_count >= GLOBAL_STATE_num_em_groups(driver_state)) {
        SEP_PRINT_ERROR("lwpmudrv_Configure_Events: Number of EM groups exceeded the initial configuration.");
        return OS_SUCCESS;
    }
    if (arg->w_buf == NULL || arg->w_len == 0) {
        return OS_FAULT;
    }

    ecb = CONTROL_Allocate_Memory(arg->w_len);
    if (!ecb) {
        SEP_PRINT_ERROR("lwpmudrv_Config_Events: could not allocate memory for ecb!\n");
        return OS_NO_MEM;
    }
    if (copy_from_user(ecb, arg->w_buf, arg->w_len)) {
        SEP_PRINT_ERROR("lwpmudrv_Config_Events: copy_from_user failed while copying ecb data.\n");
        CONTROL_Free_Memory(ecb);
        return OS_FAULT;
    }
    group_id                    = ECB_group_id(ecb);
    PMU_register_data[group_id] = ecb;
    em_groups_count             = group_id + 1;

    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_Configure_events_UNC(IOCTL_ARGS arg)
 *
 * @param arg - pointer to the IOCTL_ARGS structure
 *
 * @return OS_STATUS
 *
 * @brief  Make a copy of the uncore registers that need to be programmed
 * @brief  for the next event set used for event multiplexing
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Configure_Events_UNC (
    IOCTL_ARGS arg
)
{
    VOID              **PMU_register_data_unc;
    S32               em_groups_count_unc;
    ECB               ecb;
    S32               i;
    U32               j;
    EVENT_CONFIG      ec_unc;
    DRV_CONFIG        pcfg_unc;
    U32               group_id = 0;
    ECB               in_ecb   = NULL;

    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_IDLE) {
        return OS_IN_PROGRESS;
    }

    em_groups_count_unc = LWPMU_DEVICE_em_groups_count(&devices[cur_device]);
    PMU_register_data_unc = LWPMU_DEVICE_PMU_register_data(&devices[cur_device]);
    ec_unc                = LWPMU_DEVICE_ec(&devices[cur_device]);
    pcfg_unc              = LWPMU_DEVICE_pcfg(&devices[cur_device]);

    if (em_groups_count_unc >= (S32)EVENT_CONFIG_num_groups_unc(ec_unc)) {
        SEP_PRINT("Number of Uncore EM groups exceeded the initial configuration.");
        return OS_SUCCESS;
    }
     if (arg->w_buf == NULL || arg->w_len == 0) {
        return OS_FAULT;
    }
    //       size is in w_len, data is pointed to by w_buf
    //
    in_ecb = CONTROL_Allocate_Memory(arg->w_len);
    if (!in_ecb) {
        SEP_PRINT_ERROR("lwpmudrv_Configure_Events_UNC: could not allocate memory for ecb!\n");
        return OS_NO_MEM;
    }
    if (copy_from_user(in_ecb, arg->w_buf, arg->w_len)) {
        SEP_PRINT_ERROR("lwpmudrv_Configure_Events_UNC: copy_from_user of uncore ecb data failed.");
        CONTROL_Free_Memory(in_ecb);
        return OS_FAULT;
    }
    group_id = ECB_group_id(in_ecb);
    PMU_register_data_unc[group_id] = in_ecb;

    // at this point, we know the number of uncore events for this device,
    // so allocate the results buffer per thread for uncore only for SEP event based uncore counting
    if (DRV_CONFIG_event_based_counts(pcfg_unc)) {
        ecb = PMU_register_data_unc[group_id];
        LWPMU_DEVICE_num_events(&devices[cur_device]) = ECB_num_events(ecb);

        LWPMU_DEVICE_acc_per_thread(&devices[cur_device]) = CONTROL_Allocate_Memory(GLOBAL_STATE_num_cpus(driver_state) *
                                                                                sizeof(VOID *));
        LWPMU_DEVICE_prev_val_per_thread(&devices[cur_device]) = CONTROL_Allocate_Memory(GLOBAL_STATE_num_cpus(driver_state) *
                                                                                     sizeof(VOID *));
        for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
             LWPMU_DEVICE_acc_per_thread(&devices[cur_device])[i]      = CONTROL_Allocate_Memory((ECB_num_events(ecb) + 1)*sizeof(U64));
             LWPMU_DEVICE_prev_val_per_thread(&devices[cur_device])[i] = CONTROL_Allocate_Memory((ECB_num_events(ecb) + 1)*sizeof(U64));
             // initialize all values to 0
             for (j = 0; j < ECB_num_events(ecb) + 1; j++) {
                  LWPMU_DEVICE_acc_per_thread(&devices[cur_device])[i][j]      = 0LL;
                  LWPMU_DEVICE_prev_val_per_thread(&devices[cur_device])[i][j] = 0LL;
             }
        }
    }
    LWPMU_DEVICE_em_groups_count(&devices[cur_device]) = group_id + 1;

    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Set_Sample_Descriptors(IOCTL_ARGS arg)
 *
 * @param arg - pointer to the IOCTL_ARGS structure
 *
 * @return OS_STATUS
 *
 * @brief  Set the number of descriptor groups in the global state node.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Set_Sample_Descriptors (
    IOCTL_ARGS    arg
)
{
    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_IDLE) {
        return OS_IN_PROGRESS;
    }
    if (arg->w_len != sizeof(U32) || arg->w_buf == NULL) {
        SEP_PRINT_WARNING("Unknown size of Sample Descriptors\n");
        return OS_SUCCESS;
    }

    desc_count = 0;
    if (copy_from_user(&GLOBAL_STATE_num_descriptors(driver_state),
                       arg->w_buf,
                       sizeof(U32))) {
        return OS_FAULT;
    }

    desc_data  = CONTROL_Allocate_Memory(GLOBAL_STATE_num_descriptors(driver_state) *
                                                sizeof(VOID *));
    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_Configure_Descriptors(IOCTL_ARGS arg)
 *
 * @param arg - pointer to the IOCTL_ARGS structure
 * @return OS_STATUS
 *
 * @brief Make a copy of the descriptors that need to be read in order
 * @brief to configure a sample record.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Configure_Descriptors (
    IOCTL_ARGS    arg
)
{
    int uncopied;

    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_IDLE) {
        return OS_IN_PROGRESS;
    }

    if (desc_count >= GLOBAL_STATE_num_descriptors(driver_state)) {
        SEP_PRINT_WARNING("Number of descriptor groups exceeded the initial configuration.");
        return OS_SUCCESS;
    }

    if (arg->w_len == 0 || arg->w_buf == NULL) {
        return OS_FAULT;
    }

    //
    // First things first: Make a copy of the data for global use.
    //
    desc_data[desc_count] = CONTROL_Allocate_Memory(arg->w_len);
    uncopied = copy_from_user(desc_data[desc_count], arg->w_buf, arg->w_len);
    if (uncopied < 0) {
        // no-op ... eliminates "variable not used" compiler warning
    }
    SEP_PRINT_DEBUG("Added descriptor # %d\n", desc_count);
    desc_count++;

    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_LBR_Info(IOCTL_ARGS arg)
 *
 *
 * @param arg - pointer to the IOCTL_ARGS structure
 * @return OS_STATUS
 *
 * @brief Make a copy of the LBR information that is passed in.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_LBR_Info (
    IOCTL_ARGS    arg
)
{
    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_IDLE) {
        return OS_IN_PROGRESS;
    }

    if (DRV_CONFIG_collect_lbrs(pcfg) == FALSE) {
        SEP_PRINT_WARNING("lwpmudrv_LBR_Info: LBR capture has not been configured\n");
        return OS_SUCCESS;
    }

    if (arg->w_len == 0 || arg->w_buf == NULL) {
        return OS_FAULT;
    }

    //
    // First things first: Make a copy of the data for global use.
    //

    lbr = CONTROL_Allocate_Memory((int)arg->w_len);
    if (!lbr) {
        return OS_NO_MEM;
    }
    
    if (copy_from_user(lbr, arg->w_buf, arg->w_len)) {
        return OS_FAULT;
    }

    return OS_SUCCESS;
}

#ifdef EMON
#define CR4_PCE  0x00000100    //Performance-monitoring counter enable RDPMC
/* ------------------------------------------------------------------------- */
/*!
 * @fn static void lwpmudrv_Set_CR4_PCE_Bit(PVOID param)
 *
 * @param param - dummy parameter
 *
 * @return NONE
 *
 * @brief Set CR4's PCE bit on the logical processor
 *
 * <I>Special Notes</I>
 */
static VOID
lwpmudrv_Set_CR4_PCE_Bit (
    PVOID  param
)
{
    U32 this_cpu;
#if defined(DRV_IA32)
    U32 prev_CR4_value = 0;
    // remember if RDPMC bit previously set
    // and then enabled it
    __asm__("movl %%cr4,%%eax\n\t"
            "movl %%eax,%0\n"
            "orl  %1,%%eax\n\t"
            "movl %%eax,%%cr4\n"
            :"=irg" (prev_CR4_value)
            :"irg" (CR4_PCE)
            :"eax");
#endif
#if defined(DRV_EM64T)
    U64 prev_CR4_value = 0;
    // remember if RDPMC bit previously set
    // and then enabled it
    __asm__("movq %%cr4,%%rax\n\t"
            "movq %%rax,%0\n\t"
            "orq  %1,%%rax\n\t"
            "movq %%rax,%%cr4\n"
            :"=irg" (prev_CR4_value)
            :"irg" (CR4_PCE)
            :"rax");
#endif
    preempt_disable();
    this_cpu = CONTROL_THIS_CPU();
    preempt_enable();

    // if bit RDPMC bit was set before, 
    // set flag for when we clear it
    if (prev_CR4_value & CR4_PCE) {
        prev_set_CR4[this_cpu] = 1;
    }
    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn static void lwpmudrv_Clear_CR4_PCE_Bit(PVOID param)
 *
 * @param param - dummy parameter
 *
 * @return NONE
 *
 * @brief ClearSet CR4's PCE bit on the logical processor
 *
 * <I>Special Notes</I>
 */
static VOID
lwpmudrv_Clear_CR4_PCE_Bit (
    PVOID  param
)
{
    U32 this_cpu;
    preempt_disable();
    this_cpu = CONTROL_THIS_CPU();
    preempt_enable();

    // only clear the CR4 bit if it wasn't set
    // before we started
    if (!prev_set_CR4[this_cpu]) {
#if defined(DRV_IA32)
        __asm__("movl %%cr4,%%eax\n\t"
                "andl %0,%%eax\n\t"
                "movl %%eax,%%cr4\n"
                :
                :"irg" (~CR4_PCE)
                :"eax");
#endif
#if defined(DRV_EM64T)
        __asm__("movq %%cr4,%%rax\n\t"
                "andq %0,%%rax\n\t"
                "movq %%rax,%%cr4\n"
                :
                :"irg" (~CR4_PCE)
                :"rax");
#endif
    }
    return;
}
#endif


/* ------------------------------------------------------------------------- */
/*!
 * @fn static OS_STATUS lwpmudrv_Start(void)
 *
 * @param none
 *
 * @return OS_STATUS
 *
 * @brief  Local function that handles the LWPMU_IOCTL_START call.
 * @brief  Set up the OS hooks for process/thread/load notifications.
 * @brief  Write the initial set of MSRs.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Start (
    VOID
)
{
    OS_STATUS  status       = OS_SUCCESS;
    U32        previous_state;

    /*
     * To Do: Check for state == STATE_IDLE and only then enable sampling
     */
    previous_state = cmpxchg(&GLOBAL_STATE_current_phase(driver_state),
                             DRV_STATE_IDLE,
                             DRV_STATE_RUNNING);
    if (previous_state != DRV_STATE_IDLE) {
        SEP_PRINT_ERROR("lwpmudrv_Start: Unable to start sampling - State is %d\n",
                        GLOBAL_STATE_current_phase(driver_state));
        return OS_IN_PROGRESS;
    }
    if (DRV_CONFIG_use_pcl(pcfg) == TRUE) {
        if (DRV_CONFIG_start_paused(pcfg)) {
            GLOBAL_STATE_current_phase(driver_state) = DRV_STATE_PAUSED;
        }
        return status;
    }

    atomic_set(&read_now, GLOBAL_STATE_num_cpus(driver_state));
    init_waitqueue_head(&read_tsc_now);

#ifdef EMON
    prev_set_CR4 = CONTROL_Allocate_Memory(GLOBAL_STATE_num_cpus(driver_state) * sizeof(U8));
    CONTROL_Invoke_Parallel(lwpmudrv_Set_CR4_PCE_Bit, (PVOID)(size_t)0);
#endif // EMON
    CONTROL_Invoke_Parallel(lwpmudrv_Fill_TSC_Info, (PVOID)(size_t)0);

#ifdef EMON
    // initialize the cpu0_TSC var
    cpu0_TSC = tsc_info[0];
#endif

    if (DRV_CONFIG_start_paused(pcfg)) {
        GLOBAL_STATE_current_phase(driver_state) = DRV_STATE_PAUSED;
    }
    else if(dispatch != NULL && dispatch->restart != NULL) {
        CONTROL_Invoke_Parallel(lwpmudrv_Resume_Op, NULL);

        if (DRV_CONFIG_enable_chipset(pcfg) && cs_dispatch != NULL &&
            cs_dispatch->start_chipset != NULL) {
            cs_dispatch->start_chipset();
        }

        EVENTMUX_Start(global_ec);
        lwpmudrv_Dump_Tracer ("start", 0);
    }

    return status;
}

/*
 * @fn lwpmudrv_Prepare_Stop();
 *
 * @param        NONE
 * @return       OS_STATUS
 *
 * @brief  Local function that handles the LWPMUDRV_IOCTL_STOP call.
 * @brief  Cleans up the interrupt handler.
 */
static OS_STATUS
lwpmudrv_Prepare_Stop (
    VOID
)
{
    S32 i;
    S32 done                = FALSE;
    U32 current_state       = GLOBAL_STATE_current_phase(driver_state);

    SEP_PRINT_DEBUG("lwpmudrv_Prepare_Stop: About to stop sampling\n");
    GLOBAL_STATE_current_phase(driver_state) = DRV_STATE_PREPARE_STOP;

    if (current_state == DRV_STATE_UNINITIALIZED) {
        return OS_SUCCESS;
    }

    if (pcfg == NULL) {
        return OS_SUCCESS;
    }

    if (DRV_CONFIG_use_pcl(pcfg) == TRUE) {
        return OS_SUCCESS;
    }

    if (current_state != DRV_STATE_IDLE          &&
        current_state != DRV_STATE_RESERVED) {
        for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
            CPU_STATE_accept_interrupt(&pcb[i]) = 0;
        }
        while (!done) {
            done = TRUE;
            for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
                if (atomic_read(&CPU_STATE_in_interrupt(&pcb[i]))) {
                    done = FALSE;
                }
            }
        }
        CONTROL_Invoke_Parallel(lwpmudrv_Pause_Op, NULL);

        SEP_PRINT_DEBUG("lwpmudrv_Prepare_Stop: Outside of all interrupts\n");

        if (DRV_CONFIG_enable_chipset(pcfg)  &&
            cs_dispatch != NULL              &&
            cs_dispatch->stop_chipset != NULL ) {
            cs_dispatch->stop_chipset();
        }
    }

    if (pcfg == NULL) {
        return OS_SUCCESS;
    }

    /*
     * Clean up all the control registers
     */
    if ((dispatch != NULL) && (dispatch->cleanup != NULL)) {
        CONTROL_Invoke_Parallel(dispatch->cleanup, NULL);
        SEP_PRINT_DEBUG("Stop: Cleanup finished\n");
    }
    lwpmudrv_Free_Restore_Buffer();

#ifdef EMON
    CONTROL_Invoke_Parallel(lwpmudrv_Clear_CR4_PCE_Bit, (VOID *)(size_t)0);
    prev_set_CR4 = CONTROL_Free_Memory(prev_set_CR4);
#endif

    if (DRV_CONFIG_enable_chipset(pcfg) &&
        cs_dispatch && cs_dispatch->fini_chipset) {
        cs_dispatch->fini_chipset();
    }

    return OS_SUCCESS;
}

/*
 * @fn lwpmudrv_Finish_Stop();
 *
 * @param  NONE
 * @return OS_STATUS
 *
 * @brief  Local function that handles the LWPMUDRV_IOCTL_STOP call.
 * @brief  Cleans up the interrupt handler.
 */
static OS_STATUS
lwpmudrv_Finish_Stop (
    VOID
)
{
    U32        current_state = GLOBAL_STATE_current_phase(driver_state);
    S32        prev_value;
    OS_STATUS  status        = OS_SUCCESS;

    if(pcfg == NULL) {
        return OS_INVALID;
    }
    prev_value = cmpxchg(&in_finish_code, 0, 1);
    if (prev_value != 0) {
       return OS_SUCCESS;
    }
    if (DRV_CONFIG_counting_mode(pcfg) == FALSE) {
#if defined(DRV_USE_NMI)
        OUTPUT_Delete_Timers();
#endif
        if (abnormal_terminate == 0) {
            LINUXOS_Uninstall_Hooks();
            /*
             *  Make sure that the module buffers are not deallocated and that the module flush
             *  thread has not been terminated.
             */
            if (current_state != DRV_STATE_IDLE && current_state != DRV_STATE_RESERVED) {
                status = LINUXOS_Enum_Process_Modules(TRUE);
            }
        }
        OUTPUT_Flush();
        /*
         * Clean up the interrupt handler via the IDT
         */
        CPUMON_Remove_Cpuhooks();
        PEBS_Destroy(pcfg);
        EVENTMUX_Destroy(global_ec);
    }
    GLOBAL_STATE_current_phase(driver_state) = DRV_STATE_STOPPED;
    in_finish_code                           = 0;

    if (DRV_CONFIG_enable_cp_mode(pcfg)) {
        if (interrupt_counts) {
            S32 idx, cpu;
            for (cpu = 0; cpu < GLOBAL_STATE_num_cpus(driver_state); cpu++) {
                for(idx = 0; idx < DRV_CONFIG_num_events(pcfg); idx++) {
                    SEP_PRINT_DEBUG("Interrupt count: CPU %d, event %d = %lld\n", cpu, idx, interrupt_counts[cpu * DRV_CONFIG_num_events(pcfg) + idx]);
                }
            }
        }
    }

    read_counter_info = CONTROL_Free_Memory(read_counter_info);
    prev_counter_data = CONTROL_Free_Memory(prev_counter_data);
    lwpmudrv_Dump_Tracer ("stop", 0);
    return status;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Get_Normalized_TSC(IOCTL_ARGS arg)
 *
 * @param arg - Pointer to the IOCTL structure
 *
 * @return OS_STATUS
 *
 * @brief  Return the current value of the normalized TSC.
 *         NOTE: knocked off check for pcb == NULL in order to get a valid
 *         CPU frequency when running emon -v.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Get_Normalized_TSC (
    IOCTL_ARGS arg
)
{
    U64    tsc          = 0;
    U64    this_cpu     = 0;
    size_t size_to_copy = sizeof(U64);

    if (arg->r_len < size_to_copy || arg->r_buf == NULL) {
        return OS_FAULT;
    }

    preempt_disable();
    UTILITY_Read_TSC(&tsc);
    UTILITY_Read_TSC(&tsc);
    this_cpu = CONTROL_THIS_CPU();
    tsc -= TSC_SKEW(CONTROL_THIS_CPU());
    preempt_enable();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
    if (pcfg && DRV_CONFIG_use_pcl(pcfg) == TRUE) {
        preempt_disable();
        tsc = cpu_clock(this_cpu);
        preempt_enable();
    }
    else {
#endif
    tsc -= TSC_SKEW(this_cpu);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
    }
#endif
    if (copy_to_user(arg->r_buf, (VOID *)&tsc, size_to_copy)) {
        SEP_PRINT_DEBUG("lwpmudrv_Get_Normalized_TSC: copy_to_user() failed\n");
        return OS_FAULT;
    }
    lwpmudrv_Dump_Tracer ("marker", tsc);

    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Get_Num_Cores(IOCTL_ARGS arg)
 *
 * @param arg - Pointer to the IOCTL structure
 *
 * @return OS_STATUS
 *
 * @brief  Quickly return the (total) number of cpus in the system.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Get_Num_Cores (
    IOCTL_ARGS   arg
)
{
    OS_STATUS status = OS_SUCCESS;
    S32 num = GLOBAL_STATE_num_cpus(driver_state);
 
    if (arg->r_len != sizeof(S32) || arg->r_buf == NULL) {
        return OS_INVALID;
    }
 
    SEP_PRINT_DEBUG("lwpmudrv_Get_Num_Cores: Num_Cores is %d, out_buf is 0x%p\n", num, arg->r_buf);
    status = put_user(num, (S32*)arg->r_buf);
 
    return status;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Set_CPU_Mask(PVOID in_buf, U32 in_buf_size)
 *
 * @param in_buf      - pointer to the CPU mask buffer
 * @param in_buf_size - size of the CPU mask buffer
 *
 * @return OS_STATUS
 *
 * @brief  process the CPU mask as requested by the user
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Set_CPU_Mask (
    PVOID         in_buf,
    size_t        in_buf_len
)
{
    U32     cpu_count     = 0;

    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_IDLE) {
        return OS_IN_PROGRESS;
    }

    if (in_buf_len == 0 || in_buf == NULL) {
        return OS_INVALID;
    }

    cpu_mask_bits = CONTROL_Allocate_Memory((int)in_buf_len);
    if (!cpu_mask_bits) {
        return OS_NO_MEM;
    }

    if (copy_from_user(cpu_mask_bits, (S8*)in_buf, (int)in_buf_len)) {
        return OS_FAULT;
    }

    for (cpu_count = 0; cpu_count < (U32)GLOBAL_STATE_num_cpus(driver_state); cpu_count++) {
        CPU_STATE_accept_interrupt(&pcb[cpu_count]) = cpu_mask_bits[cpu_count] ? 1 : 0;
        CPU_STATE_initial_mask(&pcb[cpu_count    ]) = cpu_mask_bits[cpu_count] ? 1 : 0;
    }

    return OS_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Get_KERNEL_CS(IOCTL_ARGS arg)
 *
 * @param arg - Pointer to the IOCTL structure
 *
 * @return OS_STATUS
 *
 * @brief  Return the value of the Kernel symbol KERNEL_CS.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Get_KERNEL_CS (
    IOCTL_ARGS   arg
)
{
    OS_STATUS status = OS_SUCCESS;
    S32       num    = __KERNEL_CS;
 
    if (arg->r_len != sizeof(S32) || arg->r_buf == NULL) {
        return OS_INVALID;
    }
 
    SEP_PRINT_DEBUG("lwpmudrv_Get_KERNEL_CS is %d, out_buf is 0x%p\n", num, arg->r_buf);
    status = put_user(num, (S32*)arg->r_buf);

    return status;
}

/*
 * @fn lwpmudrv_Set_UID
 *
 * @param     IN   arg      - pointer to the output buffer
 * @return   OS_STATUS
 *
 * @brief  Receive the value of the UID of the collector process.
 */
static OS_STATUS
lwpmudrv_Set_UID (
    IOCTL_ARGS   arg
)
{
    OS_STATUS status = OS_SUCCESS;
 
    if (arg->w_len != sizeof(uid_t) || arg->w_buf == NULL) {
        return OS_INVALID;
    }
 
    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_IDLE) {
        return OS_IN_PROGRESS;
    }
 
    status = get_user(uid, (S32*)arg->w_buf);
    SEP_PRINT_DEBUG("lwpmudrv_Set_UID is %d\n", uid);

    return status;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Get_TSC_Skew_Info(IOCTL_ARGS arg)
 *
 * @param arg - Pointer to the IOCTL structure
 *
 * @return OS_STATUS
 * @brief  Return the current value of the TSC skew data
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Get_TSC_Skew_Info (
    IOCTL_ARGS arg
)
{
    S64    *skew_array;
    size_t  skew_array_len;
    S32     i;

    skew_array_len = GLOBAL_STATE_num_cpus(driver_state) * sizeof(U64);

    if (arg->r_len < skew_array_len || arg->r_buf == NULL) {
        SEP_PRINT_ERROR("lwpmudrv_Get_TSC_Skew_Info: Buffer too small in Get_TSC_Skew_Info: %lld\n",arg->r_len);
        return OS_INVALID;
    }

    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_STOPPED) {
        return OS_IN_PROGRESS;
    }

    SEP_PRINT_DEBUG("lwpmudrv_Get_TSC_Skew_Info dispatched with r_len=%lld\n", arg->r_len);

    skew_array = CONTROL_Allocate_Memory(skew_array_len);
    if (skew_array == NULL) {
        SEP_PRINT_ERROR("lwpmudrv_Get_TSC_Skew_Info: Unable to allocate memory\n");
        return OS_NO_MEM;
    }

    for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
        skew_array[i] = TSC_SKEW(i);
    }

    if (copy_to_user(arg->r_buf, skew_array, skew_array_len)) {
        skew_array = CONTROL_Free_Memory(skew_array);
        return OS_FAULT;
    }

    skew_array = CONTROL_Free_Memory(skew_array);
    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Collect_Sys_Config(IOCTL_ARGS arg)
 *
 * @param arg - Pointer to the IOCTL structure
 *
 * @return OS_STATUS
 *
 * @brief  Local function that handles the COLLECT_SYS_CONFIG call.
 * @brief  Builds and collects the SYS_INFO data needed.
 * @brief  Writes the result into the argument.
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Collect_Sys_Config (
    IOCTL_ARGS   arg
)
{
    OS_STATUS  status = OS_SUCCESS;
    U32 num = SYS_INFO_Build();
 
    if (arg->r_len < sizeof(S32) || arg->r_buf == NULL) {
        return OS_INVALID;
    }
 
    SEP_PRINT_DEBUG("lwpmudrv_Collect_Sys_Config: size of sys info is %d\n", num);
    status = put_user(num, (S32*)arg->r_buf);

    return status;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Sys_Config(IOCTL_ARGS arg)
 *
 * @param arg - Pointer to the IOCTL structure
 *
 * @return OS_STATUS
 *
 * @brief  Return the current value of the normalized TSC.
 *
 * @brief  Transfers the VTSA_SYS_INFO data back to the abstraction layer.
 * @brief  The out_buf should have enough space to handle the transfer.
 */
static OS_STATUS
lwpmudrv_Sys_Config (
    IOCTL_ARGS   arg
)
{
    if (arg->r_len == 0 || arg->r_buf == NULL) {
        return OS_INVALID;

    }

    SYS_INFO_Transfer(arg->r_buf, arg->r_len);

    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Samp_Read_Num_Of_Core_Counters(IOCTL_ARGS arg)
 *
 * @param arg - Pointer to the IOCTL structure
 *
 * @return OS_STATUS
 *
 * @brief  Read memory mapped i/o physical location
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Samp_Read_Num_Of_Core_Counters (
    IOCTL_ARGS   arg
)
{
    U64           rax, rbx, rcx, rdx,num_basic_functions;
    U32           val    = 0;
    OS_STATUS     status = OS_SUCCESS;

    if (arg->r_len == 0 || arg->r_buf == NULL) {
        return OS_INVALID;
    }

    UTILITY_Read_Cpuid(0x0,&num_basic_functions,&rbx, &rcx, &rdx);

    if (num_basic_functions >= 0xA) {
         UTILITY_Read_Cpuid(0xA,&rax,&rbx, &rcx, &rdx);
         val    = ((U32)(rax >> 8)) & 0xFF;
    }
    status = put_user(val, (S32*)arg->r_buf);
    SEP_PRINT_DEBUG("num of counter is %d\n",val);
    return status;
}



/* ------------------------------------------------------------------------- */
/*!
 * @fn  static DRV_BOOL lwpmudrv_Is_Physical_Address_Free(U32 physical_addrss)
 *
 * @param physical_address - physical address
 *
 * @return DRV_BOOL
 *
 * @brief  Check if physical address is available
 *
 * <I>Special Notes</I>
 */
static DRV_BOOL
lwpmudrv_Is_Physical_Address_Free (
    U32 physical_address
)
{
    U32 value;
    U32 new_value;
    U32 test_value;

    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_IDLE) {
        return FALSE;
    }
    if (physical_address == 0) {
        return FALSE;
    }
    // First attempt read
    //
    PCI_Read_From_Memory_Address(physical_address, &value);

    // Value must be 0xFFFFFFFFF or there is NO chance
    // that this memory location is available.
    //
    if (value != 0xFFFFFFFF) {
        return FALSE;
    }

    //
    // Try to write a bit to a zero (this probably
    // isn't too safe, but this is just for testing)
    //
    new_value = 0xFFFFFFFE;
    PCI_Write_To_Memory_Address(physical_address, new_value);
    PCI_Read_From_Memory_Address(physical_address, &test_value);

    // Write back original
    PCI_Write_To_Memory_Address(physical_address, value);

    if (new_value == test_value) {
        // The write appeared to change the
        // memory, it must be mapped already
        //
        return FALSE;
    }

    if (test_value == 0xFFFFFFFF) {
        // The write did not change the bit, so
        // apparently, this memory must not be mapped
        // to anything.
        //
        return TRUE;
    }

    return FALSE;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Samp_Find_Physical_Address(IOCTL_ARGS arg)
 *
 * @param arg - Pointer to the IOCTL structure
 *
 * @return OS_STATUS
 *
 * @brief  Find a free physical address
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Samp_Find_Physical_Address (
    IOCTL_ARGS    arg
)
{
    CHIPSET_PCI_SEARCH_ADDR_NODE user_addr;
    CHIPSET_PCI_SEARCH_ADDR      search_addr = (CHIPSET_PCI_SEARCH_ADDR)arg->w_buf;
    U32                          addr;

    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_IDLE) {
        return OS_IN_PROGRESS;
    }
    if (arg->r_len == 0 || arg->r_buf == NULL ||
        arg->w_len == 0 || arg->w_buf == NULL) {
        return OS_INVALID;
    }
    if (!search_addr) {
        return OS_FAULT;
    }

    if (!access_ok(VERIFY_WRITE, search_addr, sizeof(CHIPSET_PCI_SEARCH_ADDR_NODE))) {
        return OS_FAULT;
    }

    if (copy_from_user(&user_addr, search_addr, sizeof(CHIPSET_PCI_SEARCH_ADDR_NODE))) {
        SEP_PRINT_DEBUG("lwpmudrv_Samp_Find_Physical_Address: copy_from_user() failed\n");
        return OS_FAULT;
    }

    if (CHIPSET_PCI_SEARCH_ADDR_start(&user_addr) > CHIPSET_PCI_SEARCH_ADDR_stop(&user_addr)) {
        return OS_INVALID;
    }

    CHIPSET_PCI_SEARCH_ADDR_address(&user_addr) = 0;

    for (addr = CHIPSET_PCI_SEARCH_ADDR_start(&user_addr);
        addr <= CHIPSET_PCI_SEARCH_ADDR_stop(&user_addr);
        addr += CHIPSET_PCI_SEARCH_ADDR_increment(&user_addr)) {
        SEP_PRINT_DEBUG("lwpmudrv_Samp_Find_Physical_Address: addr=%x:",addr);
        if (lwpmudrv_Is_Physical_Address_Free(addr)) {
            CHIPSET_PCI_SEARCH_ADDR_address(&user_addr) = addr;
            break;
        }
    }

    if (copy_to_user(arg->r_buf, (VOID *) &user_addr, sizeof(CHIPSET_PCI_SEARCH_ADDR_NODE))) {
        SEP_PRINT_DEBUG("lwpmudrv_Samp_Find_Physical_Address: copy_to_user() failed\n");
        return OS_NO_MEM;
    }

    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Samp_Read_PCI_Config(IOCTL_ARGS arg)
 *
 * @param arg - Pointer to the IOCTL structure
 *
 * @return OS_STATUS
 *
 * @brief  Read the PCI Configuration Space
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Samp_Read_PCI_Config (
    IOCTL_ARGS    arg
)
{
    U32                     pci_address;
    CHIPSET_PCI_CONFIG      rd_pci = NULL;

    if (arg->r_len == 0 || arg->r_buf == NULL) {
        SEP_PRINT_ERROR("lwpmudrv_Samp_Read_PCI_Config: null read buffer\n");
        return OS_FAULT;
    }

    rd_pci = CONTROL_Allocate_Memory(arg->r_len);
    if (rd_pci == NULL) {
        SEP_PRINT_ERROR("lwpmudrv_Samp_Read_PCI_Config: unable to allocate local memory\n");
        return OS_NO_MEM;
    }

    if (copy_from_user(rd_pci, (CHIPSET_PCI_CONFIG)arg->w_buf, sizeof(CHIPSET_PCI_CONFIG_NODE))) {
        SEP_PRINT_ERROR("lwpmudrv_Samp_Read_PCI_Config: unable to read into local memory\n");
        rd_pci = CONTROL_Free_Memory(rd_pci);
        return OS_FAULT;
    }

    SEP_PRINT_DEBUG("lwpmudrv_Samp_Read_PCI_Config: reading PCI address:0x%x:0x%x:0x%x, offset 0x%x\n",
                CHIPSET_PCI_CONFIG_bus(rd_pci),
                CHIPSET_PCI_CONFIG_device(rd_pci),
                CHIPSET_PCI_CONFIG_function(rd_pci),
                CHIPSET_PCI_CONFIG_offset(rd_pci));

    pci_address = FORM_PCI_ADDR(CHIPSET_PCI_CONFIG_bus(rd_pci),
                            CHIPSET_PCI_CONFIG_device(rd_pci),
                            CHIPSET_PCI_CONFIG_function(rd_pci),
                            CHIPSET_PCI_CONFIG_offset(rd_pci));
    CHIPSET_PCI_CONFIG_value(rd_pci) = PCI_Read_Ulong(pci_address);

    if (copy_to_user(arg->r_buf, (VOID *) rd_pci, sizeof(CHIPSET_PCI_CONFIG_NODE))) {
        SEP_PRINT_ERROR("lwpmudrv_Samp_Read_PCI_Config: unable to copy to user\n");
        rd_pci = CONTROL_Free_Memory(rd_pci);
        return OS_FAULT;
    }

    SEP_PRINT_DEBUG("lwpmudrv_Samp_Read_PCI_Config: value at this PCI address:0x%x\n",
                    CHIPSET_PCI_CONFIG_value(rd_pci));

    rd_pci = CONTROL_Free_Memory(rd_pci);

    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Samp_Write_PCI_Config(IOCTL_ARGS arg)
 *
 * @param arg - Pointer to the IOCTL structure
 *
 * @return OS_STATUS
 *
 * @brief  Write to the PCI Configuration Space
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Samp_Write_PCI_Config (
    IOCTL_ARGS    arg
)
{
    U32                pci_address;
    CHIPSET_PCI_CONFIG wr_pci = NULL;

    if (arg->w_len == 0 || arg->w_buf == NULL) {
        SEP_PRINT_ERROR("lwpmudrv_Samp_Write_PCI_Config: null write buffer\n");
        return OS_FAULT;
    }

    // the following allows "sep -el -pc" to work, since the command must access the
    // the driver ioctls before driver is used for a collection
    if (! (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_UNINITIALIZED ||
           GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_IDLE)) {
        SEP_PRINT_ERROR("lwpmudrv_Samp_Write_PCI_Config: driver is non-idle or busy\n");
        return OS_IN_PROGRESS;
    }

    wr_pci = CONTROL_Allocate_Memory(arg->w_len);
    if (wr_pci == NULL) {
        SEP_PRINT_ERROR("lwpmudrv_Samp_Write_PCI_Config: unable to allocate local memory\n");
        return OS_NO_MEM;
    }
    if (copy_from_user(wr_pci, (CHIPSET_PCI_CONFIG)arg->w_buf, arg->w_len)) {
        SEP_PRINT_ERROR("lwpmudrv_Samp_Write_PCI_Config: copy_from_user() failed.\n")
        CONTROL_Free_Memory(wr_pci);
        return OS_FAULT;
    }

    SEP_PRINT_DEBUG("lwpmudrv_Samp_Write_PCI_Config: writing 0x%x to PCI address:0x%x:0x%x:0x%x, offset 0x%x\n",
                    CHIPSET_PCI_CONFIG_bus(wr_pci),
                    CHIPSET_PCI_CONFIG_device(wr_pci),
                    CHIPSET_PCI_CONFIG_function(wr_pci),
                    CHIPSET_PCI_CONFIG_offset(wr_pci));

    pci_address = FORM_PCI_ADDR(CHIPSET_PCI_CONFIG_bus(wr_pci),
                            CHIPSET_PCI_CONFIG_device(wr_pci),
                            CHIPSET_PCI_CONFIG_function(wr_pci),
                            CHIPSET_PCI_CONFIG_offset(wr_pci));
    PCI_Write_Ulong(pci_address, CHIPSET_PCI_CONFIG_value(wr_pci));
    CONTROL_Free_Memory(wr_pci);

    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Samp_Chipset_Init(IOCTL_ARGS arg)
 *
 * @param arg - Pointer to the IOCTL structure
 *
 * @return OS_STATUS
 *
 * @brief  Initialize the chipset cnfiguration
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Samp_Chipset_Init (
    IOCTL_ARGS    arg
)
{
    PVOID         in_buf     = arg->w_buf;
    U32           in_buf_len = arg->w_len;

    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_IDLE) {
        SEP_PRINT_ERROR("lwpmudrv_Samp_Chipset_Init: driver is currently busy!\n");
        return OS_IN_PROGRESS;
    }

    if (in_buf == NULL || in_buf_len == 0) {
        SEP_PRINT_ERROR("lwpmudrv_Samp_Chipset_Init: Chipset information passed in is null\n");
        return OS_NO_MEM;
    }

    // First things first: Make a copy of the data for global use.
    pma = CONTROL_Allocate_Memory(in_buf_len);

    if (pma == NULL) {
        SEP_PRINT_ERROR("lwpmudrv_Samp_Chipset_Init: unable to allocate memory\n");
        return OS_NO_MEM;
    }

    if (copy_from_user(pma, in_buf, in_buf_len)) {
        SEP_PRINT_ERROR("lwpmudrv_Samp_Chipset_Init: unable to copy from user\n");
        return OS_FAULT;
    }

#if defined(MY_DEBUG)

    SEP_PRINT("lwpmudrv_Samp_Chipset_Init: Chipset Configuration follows...\n");
    SEP_PRINT("pma->length=%d\n", CHIPSET_CONFIG_length(pma));
    SEP_PRINT("pma->version=%d\n", CHIPSET_CONFIG_major_version(pma));
    SEP_PRINT("pma->processor=%d\n", CHIPSET_CONFIG_processor(pma));
    SEP_PRINT("pma->mch_chipset=%d\n", CHIPSET_CONFIG_mch_chipset(pma));
    SEP_PRINT("pma->ich_chipset=%d\n", CHIPSET_CONFIG_ich_chipset(pma));
    SEP_PRINT("pma->gmch_chipset=%d\n", CHIPSET_CONFIG_gmch_chipset(pma));
    SEP_PRINT("pma->mother_board_time=%d\n", CHIPSET_CONFIG_motherboard_time(pma));
    SEP_PRINT("pma->host_proc_run=%d\n", CHIPSET_CONFIG_host_proc_run(pma));
    SEP_PRINT("pma->noa_chipset=%d\n", CHIPSET_CONFIG_noa_chipset(pma));
    SEP_PRINT("pma->bnb_chipset=%d\n", CHIPSET_CONFIG_bnb_chipset(pma));

    if (CHIPSET_CONFIG_mch_chipset(pma)) {
        SEP_PRINT("pma->mch->phys_add=0x%llx\n", CHIPSET_SEGMENT_physical_address(&CHIPSET_CONFIG_mch(pma)));
        SEP_PRINT("pma->mch->size=%d\n", CHIPSET_SEGMENT_size(&CHIPSET_CONFIG_mch(pma)));
        SEP_PRINT("pma->mch->num_counters=%d\n", CHIPSET_SEGMENT_num_counters(&CHIPSET_CONFIG_mch(pma)));
        SEP_PRINT("pma->mch->total_events=%d\n", CHIPSET_SEGMENT_total_events(&CHIPSET_CONFIG_mch(pma)));
    }

    if (CHIPSET_CONFIG_ich_chipset(pma)) {
        SEP_PRINT("pma->ich->phys_add=0x%llx\n", CHIPSET_SEGMENT_physical_address(&CHIPSET_CONFIG_ich(pma)));
        SEP_PRINT("pma->ich->size=%d\n", CHIPSET_SEGMENT_size(&CHIPSET_CONFIG_ich(pma)));
        SEP_PRINT("pma->ich->num_counters=%d\n", CHIPSET_SEGMENT_num_counters(&CHIPSET_CONFIG_ich(pma)));
        SEP_PRINT("pma->ich->total_events=%d\n", CHIPSET_SEGMENT_total_events(&CHIPSET_CONFIG_ich(pma)));
    }

    if (CHIPSET_CONFIG_gmch_chipset(pma)) {
        SEP_PRINT("pma->gmch->phys_add=0x%llx\n", CHIPSET_SEGMENT_physical_address(&CHIPSET_CONFIG_gmch(pma)));
        SEP_PRINT("pma->gmch->size=%d\n", CHIPSET_SEGMENT_size(&CHIPSET_CONFIG_gmch(pma)));
        SEP_PRINT("pma->gmch->num_counters=%d\n", CHIPSET_SEGMENT_num_counters(&CHIPSET_CONFIG_gmch(pma)));
        SEP_PRINT("pma->gmch->total_events=%d\n", CHIPSET_SEGMENT_total_events(&CHIPSET_CONFIG_gmch(pma)));
        SEP_PRINT("pma->gmch->read_register=0x%x\n", CHIPSET_SEGMENT_read_register(&CHIPSET_CONFIG_gmch(pma)));
        SEP_PRINT("pma->gmch->write_register=0x%x\n", CHIPSET_SEGMENT_write_register(&CHIPSET_CONFIG_gmch(pma)));
    }

#endif

    // Set up the global cs_dispatch table
    cs_dispatch = UTILITY_Configure_Chipset();
    if (cs_dispatch == NULL) {
        SEP_PRINT_ERROR("lwpmudrv_Samp_Chipset_Init: unknown chipset family\n");
        return OS_INVALID;
    }

    // Initialize chipset configuration
    if (cs_dispatch->init_chipset()) {
        SEP_PRINT_ERROR("lwpmudrv_Samp_Chipset_Init: failed to initialize the chipset\n");
        return OS_INVALID;
    }

    return OS_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Get_Platform_Info(IOCTL_ARGS arg)
 *
 * @param arg - Pointer to the IOCTL structure
 *
 * @return OS_STATUS
 *
 * @brief       Reads the MSR_PLATFORM_INFO register if present
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Get_Platform_Info (
    IOCTL_ARGS args
)
{
    U32                    size          = sizeof(DRV_PLATFORM_INFO_NODE);
    OS_STATUS              status        = OS_SUCCESS;
    DRV_PLATFORM_INFO      platform_data = NULL;
    U32                   *dispatch_ids  = NULL;
    DISPATCH               dispatch_ptr  = NULL;
    U32                    i             = 0;
    U32                    num_entries   = args->r_len/sizeof(U32); // # dispatch ids to process

    if (!platform_data) {
        platform_data = CONTROL_Allocate_Memory(sizeof(DRV_PLATFORM_INFO_NODE));
        if (!platform_data) {
            return OS_NO_MEM;
        }
    }

    memset(platform_data, 0, sizeof(DRV_PLATFORM_INFO_NODE));
    if (args->r_len > 0 && args->r_buf != NULL) {
        dispatch_ids = CONTROL_Allocate_Memory(args->r_len);
        if (!dispatch_ids) {
            return OS_NO_MEM;
        }

        status = copy_from_user(dispatch_ids, args->r_buf, args->r_len);
        if (status) {
            return status;
        }
        for (i = 0; i < num_entries; i++) {
            if (dispatch_ids[i] != -1) {
                dispatch_ptr = UTILITY_Configure_CPU(dispatch_ids[i]);
                if (dispatch_ptr &&
                    dispatch_ptr->platform_info) {
                    dispatch_ptr->platform_info((PVOID)platform_data);
                }
            }
        }
        dispatch_ids = CONTROL_Free_Memory(dispatch_ids);
    }
    else if (dispatch && dispatch->platform_info) {
        dispatch->platform_info((PVOID)platform_data);
    }

    if (args->w_len < size || args->w_buf == NULL) {
        return OS_FAULT;
    }

    status        = copy_to_user(args->w_buf, platform_data, size);
    platform_data = CONTROL_Free_Memory(platform_data);

    return status;
}
/* ------------------------------------------------------------------------- */
/*!
 * @fn          void lwpmudrv_Setup_Cpu_Topology (value)
 *
 * @brief       Sets up the per CPU state structures
 *
 * @param       IOCTL_ARGS args
 *
 * @return      OS_STATUS
 *
 * <I>Special Notes:</I>
 *              This function was added to support abstract dll creation. Use
 *              this function to set the value of abnormal_terminate ouside of
 *              sep_common.
 */
static OS_STATUS
lwpmudrv_Setup_Cpu_Topology (
    IOCTL_ARGS args
)
{
    S32               cpu_num;
    S32               iter;
    DRV_TOPOLOGY_INFO drv_topology, dt;

    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_IDLE) {
        return OS_IN_PROGRESS;
    }
    if (args->w_len == 0 || args->w_buf == NULL || pcb == NULL ) {
        SEP_PRINT_ERROR("topology information has been misconfigured\n");
        return OS_NO_MEM;
    }

    drv_topology = CONTROL_Allocate_Memory(args->w_len);
    if (drv_topology == NULL) {
        return OS_NO_MEM;
    }

    if (copy_from_user(drv_topology, (DRV_TOPOLOGY_INFO)(args->w_buf), args->w_len)) {
        return OS_FAULT;
    }
    /*
     *   Topology Initializations
     */
    num_packages = 0;
    for (iter = 0; iter < GLOBAL_STATE_num_cpus(driver_state); iter++) {
        dt                                         = &drv_topology[iter];
        cpu_num                                    = DRV_TOPOLOGY_INFO_cpu_number(dt);
        CPU_STATE_socket_master(&pcb[cpu_num])     = DRV_TOPOLOGY_INFO_socket_master(dt);
        num_packages                              += CPU_STATE_socket_master(&pcb[cpu_num]);
        CPU_STATE_core_master(&pcb[cpu_num])       = DRV_TOPOLOGY_INFO_core_master(dt);
        CPU_STATE_thr_master(&pcb[cpu_num])        = DRV_TOPOLOGY_INFO_thr_master(dt);
        CPU_STATE_cpu_module_num(&pcb[cpu_num])    = (U16)DRV_TOPOLOGY_INFO_cpu_module_num(&drv_topology[iter]);
        CPU_STATE_cpu_module_master(&pcb[cpu_num]) = (U16)DRV_TOPOLOGY_INFO_cpu_module_master(&drv_topology[iter]);
        CPU_STATE_system_master(&pcb[cpu_num])     = (iter)? 0 : 1;
        SEP_PRINT_DEBUG("cpu %d sm = %d cm = %d tm = %d\n",
                  cpu_num,
                  CPU_STATE_socket_master(&pcb[cpu_num]),
                  CPU_STATE_core_master(&pcb[cpu_num]),
                  CPU_STATE_thr_master(&pcb[cpu_num]));
    }
    drv_topology = CONTROL_Free_Memory(drv_topology);

    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Get_Num_Samples(IOCTL_ARGS arg)
 *
 * @param arg - Pointer to the IOCTL structure
 *
 * @return OS_STATUS
 *
 * @brief       Returns the number of samples collected during the current
 * @brief       sampling run
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Get_Num_Samples (
    IOCTL_ARGS args
)
{
    S32               cpu_num;
    U64               samples = 0;

    if (pcb == NULL) {
        SEP_PRINT_ERROR("PCB was not initialized\n");
        return OS_FAULT;
    }
    if (args->r_len == 0 || args->r_buf == NULL) {
        SEP_PRINT_ERROR("topology information has been misconfigured\n");
        return OS_NO_MEM;
    }

    for (cpu_num = 0; cpu_num < GLOBAL_STATE_num_cpus(driver_state); cpu_num++) {
        samples += CPU_STATE_num_samples(&pcb[cpu_num]);

        SEP_PRINT_DEBUG("Samples for cpu %d = %lld\n",
                        cpu_num,
                        CPU_STATE_num_samples(&pcb[cpu_num]));
    }
    SEP_PRINT_DEBUG("Total number of samples %lld\n", samples);
    return put_user(samples, (U64*)args->r_buf);
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Set_Device_Num_Units(IOCTL_ARGS arg)
 *
 * @param arg - Pointer to the IOCTL structure
 *
 * @return OS_STATUS
 *
 * @brief       Set the number of devices for the sampling run
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Set_Device_Num_Units (
    IOCTL_ARGS args
)
{
    SEP_PRINT_DEBUG("Entered lwpmudrv_Set_Device_Num_Units\n");

    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_IDLE) {

        return OS_SUCCESS;
    }
     if (args->w_len == 0 || args->w_buf == NULL) {
        return OS_NO_MEM;
    }

    if (copy_from_user(&(LWPMU_DEVICE_num_units(&devices[cur_device])),
                       args->w_buf,
                       sizeof(U32))) {
        SEP_PRINT_ERROR("lwpmudrv_Set_Device_Num_Units: copy_from_user() failed for device num units.\n");
        return OS_FAULT;
    }
    SEP_PRINT_DEBUG("LWP: num_units = %d cur_device = %d\n",
                    LWPMU_DEVICE_num_units(&devices[cur_device]),
                    cur_device);
    // on to the next device.
    cur_device++;

    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static OS_STATUS lwpmudrv_Get_Interval_Counts(IOCTL_ARGS arg)
 *
 * @param arg - Pointer to the IOCTL structure
 *
 * @return OS_STATUS
 *
 * @brief       Returns the number of samples collected during the current
 * @brief       sampling run
 *
 * <I>Special Notes</I>
 */
static OS_STATUS
lwpmudrv_Get_Interval_Counts (
    IOCTL_ARGS args
)
{
    if (!DRV_CONFIG_enable_cp_mode(pcfg)) {
        SEP_PRINT_ERROR("[lwpmudrv_Get_Interval_Counts] Not in CP mode!\n");
        return OS_FAULT;
    }
    if (pcb == NULL) {
        SEP_PRINT_ERROR("PCB was not initialized\n");
        return OS_FAULT;
    }
    if (args->r_len == 0 || args->r_buf == NULL) {
        SEP_PRINT_ERROR("Interval Counts information has been misconfigured\n");
        return OS_NO_MEM;
    }
    if (!interrupt_counts) {
        return OS_FAULT;
    }

    if (copy_to_user(args->r_buf, interrupt_counts, args->r_len)) {
        return OS_FAULT;
    }
    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          U64 lwpmudrv_Set_Uncore_Topology_Info_And_Scan
 *
 * @brief       Reads the MSR_PLATFORM_INFO register if present
 *
 * @param arg   Pointer to the IOCTL structure
 *
 * @return      status
 *
 * <I>Special Notes:</I>
 *              <NONE>
 */
static OS_STATUS
lwpmudrv_Set_Uncore_Topology_Info_And_Scan (
    IOCTL_ARGS args
)
{
    return OS_SUCCESS;
}
/* ------------------------------------------------------------------------- */
/*!
 * @fn          U64 lwpmudrv_Get_Uncore_Topology
 *
 * @brief       Reads the MSR_PLATFORM_INFO register if present
 *
 * @param arg   Pointer to the IOCTL structure
 *
 * @return      status
 *
 * <I>Special Notes:</I>
 *              <NONE>
 */
static OS_STATUS
lwpmudrv_Get_Uncore_Topology (
    IOCTL_ARGS args
)
{
    
    DISPATCH                          temp_dispatch;
    U32                               dev;
    U32                               temp_dispatch_id;
    static UNCORE_TOPOLOGY_INFO_NODE  req_uncore_topology;

    if (args->r_buf == NULL) {
        return OS_FAULT;
    }
    if (args->r_len != sizeof(UNCORE_TOPOLOGY_INFO_NODE)) {
        return OS_FAULT;
    }

    memset((char *)&req_uncore_topology, 0, sizeof(UNCORE_TOPOLOGY_INFO_NODE));
    if (copy_from_user(&req_uncore_topology, args->r_buf, args->r_len)) {
        return OS_FAULT;
    }

    for (dev = 0; dev < MAX_DEVICES; dev++) {
        // skip if user does not require to scan this device
        if (!UNCORE_TOPOLOGY_INFO_device_scan(&req_uncore_topology, dev)) {
            continue;
        }
        // skip if this device has been discovered
        if (UNCORE_TOPOLOGY_INFO_device_scan(&uncore_topology, dev)) {
            continue;
        }
        memcpy((U8 *)&(UNCORE_TOPOLOGY_INFO_device(&uncore_topology, dev)), 
               (U8 *)&(UNCORE_TOPOLOGY_INFO_device(&req_uncore_topology, dev)),
               sizeof(UNCORE_PCIDEV_NODE));
        temp_dispatch_id = UNCORE_TOPOLOGY_INFO_device_dispatch_id(&uncore_topology, dev);   
        temp_dispatch    = UTILITY_Configure_CPU(temp_dispatch_id);
        if (temp_dispatch && temp_dispatch->scan_for_uncore) {
	    temp_dispatch->scan_for_uncore((VOID*)&dev);
        }
    }

    if (copy_to_user(args->r_buf, &uncore_topology, args->r_len)) {
        return OS_FAULT;
    }

    return OS_SUCCESS;
}

/*******************************************************************************
 *  External Driver functions - Open
 *      This function is common to all drivers
 *******************************************************************************/

static int
lwpmu_Open (
    struct inode *inode,
    struct file  *filp
)
{
    SEP_PRINT_DEBUG("lwpmu_Open called on maj:%d, min:%d\n",
            imajor(inode), iminor(inode));
    filp->private_data = container_of(inode->i_cdev, LWPMU_DEV_NODE, cdev);

    return 0;
}

/*******************************************************************************
 *  External Driver functions
 *      These functions are registered into the file operations table that
 *      controls this device.
 *      Open, Close, Read, Write, Release
 *******************************************************************************/

static ssize_t
lwpmu_Read (
    struct file  *filp,
    char         *buf,
    size_t        count,
    loff_t       *f_pos
)
{
    unsigned long retval;

    /* Transfering data to user space */
    SEP_PRINT_DEBUG("lwpmu_Read dispatched with count=%d\n", (S32)count);
    if (copy_to_user(buf, &LWPMU_DEV_buffer(lwpmu_control), 1)) {
        retval = OS_FAULT;
        return retval;
    }
    /* Changing reading position as best suits */
    if (*f_pos == 0) {
        *f_pos+=1;
        return 1;
    }

    return 0;
}

static ssize_t
lwpmu_Write (
    struct file  *filp,
    const  char  *buf,
    size_t        count,
    loff_t       *f_pos
)
{
    unsigned long retval;

    SEP_PRINT_DEBUG("lwpmu_Write dispatched with count=%d\n", (S32)count);
    if (copy_from_user(&LWPMU_DEV_buffer(lwpmu_control), buf+count-1, 1)) {
        retval = OS_FAULT;
        return retval;
    }

    return 1;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  extern IOCTL_OP_TYPE lwpmu_Service_IOCTL(IOCTL_USE_NODE, filp, cmd, arg)
 *
 * @param   IOCTL_USE_INODE       - Used for pre 2.6.32 kernels
 * @param   struct   file   *filp - file pointer
 * @param   unsigned int     cmd  - IOCTL command
 * @param   unsigned long    arg  - args to the IOCTL command
 *
 * @return OS_STATUS
 *
 * @brief  SEP Worker function that handles IOCTL requests from the user mode.
 *
 * <I>Special Notes</I>
 */
extern IOCTL_OP_TYPE
lwpmu_Service_IOCTL (
    IOCTL_USE_INODE
    struct   file   *filp,
    unsigned int     cmd,
    IOCTL_ARGS_NODE  local_args
)
{
    int              status = OS_SUCCESS;

    if (cmd ==  DRV_OPERATION_GET_DRIVER_STATE) {
        SEP_PRINT_DEBUG("DRV_OPERATION_GET_DRIVER_STATE\n");
        status = lwpmudrv_Get_Driver_State(&local_args);
        return status;
    }

    MUTEX_LOCK(ioctl_lock);
    switch (cmd) {

       /*
        * Common IOCTL commands
        */

        case DRV_OPERATION_VERSION:
            SEP_PRINT_DEBUG("DRV_OPERATION_VERSION\n");
            status = lwpmudrv_Version(&local_args);
            break;

        case DRV_OPERATION_RESERVE:
            SEP_PRINT_DEBUG("DRV_OPERATION_RESERVE\n");
            status = lwpmudrv_Reserve(&local_args);
            break;

        case DRV_OPERATION_INIT:
            SEP_PRINT_DEBUG("DRV_OPERATION_INIT\n");
            status = lwpmudrv_Initialize(local_args.w_buf, local_args.w_len);
            break;

        case DRV_OPERATION_INIT_PMU:
            SEP_PRINT_DEBUG("DRV_OPERATION_INIT_PMU\n");
            status = lwpmudrv_Init_PMU(&local_args);
            break;

        case DRV_OPERATION_SET_CPU_MASK:
            SEP_PRINT_DEBUG("DRV_OPERATION_SET_CPU_MASK\n");
            status = lwpmudrv_Set_CPU_Mask(local_args.w_buf, local_args.w_len);
            break;

        case DRV_OPERATION_START:
            SEP_PRINT_DEBUG("DRV_OPERATION_START\n");
            status = lwpmudrv_Start();
            break;

        case DRV_OPERATION_STOP:
            SEP_PRINT_DEBUG("DRV_OPERATION_STOP\n");
            status = lwpmudrv_Prepare_Stop();
            break;

        case DRV_OPERATION_PAUSE:
            SEP_PRINT_DEBUG("DRV_OPERATION_PAUSE\n");
            status = lwpmudrv_Pause();
            break;

        case DRV_OPERATION_RESUME:
            SEP_PRINT_DEBUG("DRV_OPERATION_RESUME\n");
            status = lwpmudrv_Resume();
            break;

        case DRV_OPERATION_EM_GROUPS:
            SEP_PRINT_DEBUG("DRV_OPERATION_EM_GROUPS\n");
            status = lwpmudrv_Set_EM_Config(&local_args);
            break;

        case DRV_OPERATION_EM_CONFIG_NEXT:
            SEP_PRINT_DEBUG("DRV_OPERATION_EM_CONFIG_NEXT\n");
            status = lwpmudrv_Configure_Events(&local_args);
            break;

        case DRV_OPERATION_NUM_DESCRIPTOR:
            SEP_PRINT_DEBUG("DRV_OPERATION_NUM_DESCRIPTOR\n");
            status = lwpmudrv_Set_Sample_Descriptors(&local_args);
            break;

        case DRV_OPERATION_DESC_NEXT:
            SEP_PRINT_DEBUG("DRV_OPERATION_DESC_NEXT\n");
            status = lwpmudrv_Configure_Descriptors(&local_args);
            break;

        case DRV_OPERATION_GET_NORMALIZED_TSC:
            SEP_PRINT_DEBUG("DRV_OPERATION_GET_NORMALIZED_TSC\n");
            status = lwpmudrv_Get_Normalized_TSC(&local_args);
            break;

        case DRV_OPERATION_GET_NORMALIZED_TSC_STANDALONE:
            SEP_PRINT_DEBUG("DRV_OPERATION_GET_NORMALIZED_TSC_STANDALONE\n");
            status = lwpmudrv_Get_Normalized_TSC(&local_args);
            break;

        case DRV_OPERATION_NUM_CORES:
            SEP_PRINT_DEBUG("DRV_OPERATION_NUM_CORES\n");
            status = lwpmudrv_Get_Num_Cores(&local_args);
            break;

        case DRV_OPERATION_KERNEL_CS:
            SEP_PRINT_DEBUG("DRV_OPERATION_KERNEL_CS\n");
            status = lwpmudrv_Get_KERNEL_CS(&local_args);
            break;

        case DRV_OPERATION_SET_UID:
            SEP_PRINT_DEBUG("DRV_OPERATION_SET_UID\n");
            status = lwpmudrv_Set_UID(&local_args);
            break;

        case DRV_OPERATION_TSC_SKEW_INFO:
            SEP_PRINT_DEBUG("DRV_OPERATION_TSC_SKEW_INFO\n");
            status = lwpmudrv_Get_TSC_Skew_Info(&local_args);
            break;

        case DRV_OPERATION_COLLECT_SYS_CONFIG:
            SEP_PRINT_DEBUG("DRV_OPERATION_COLLECT_SYS_CONFIG\n");
            status = lwpmudrv_Collect_Sys_Config(&local_args);
            break;

        case DRV_OPERATION_GET_SYS_CONFIG:
            SEP_PRINT_DEBUG("DRV_OPERATION_GET_SYS_CONFIG\n");
            status = lwpmudrv_Sys_Config(&local_args);
            break;

        case DRV_OPERATION_TERMINATE:
            SEP_PRINT_DEBUG("DRV_OPERATION_TERMINATE\n");
            status = lwpmudrv_Terminate();
            break;

        case DRV_OPERATION_SET_CPU_TOPOLOGY:
            SEP_PRINT_DEBUG("DRV_OPERATION_SET_CPU_TOPOLOGY\n");
            status = lwpmudrv_Setup_Cpu_Topology(&local_args);
            break;

        case DRV_OPERATION_GET_NUM_CORE_CTRS:
            SEP_PRINT_DEBUG("DRV_OPERATION_GET_NUM_CORE_CTRS\n");
            status = lwpmudrv_Samp_Read_Num_Of_Core_Counters(&local_args);
            break;

        case DRV_OPERATION_GET_PLATFORM_INFO:
            SEP_PRINT_DEBUG("DRV_OPERATION_GET_PLATFORM_INFO\n");
            status = lwpmudrv_Get_Platform_Info(&local_args);
            break;

        case DRV_OPERATION_READ_MSRS:
            SEP_PRINT_DEBUG("DRV_OPERATION_READ_MSRs\n");
            status = lwpmudrv_Read_MSRs(&local_args);
            break;

        case DRV_OPERATION_SWITCH_GROUP:
            SEP_PRINT_DEBUG("DRV_OPERATION_SWITCH_GROUP\n");
            status = lwpmudrv_Switch_Group();
            break;

       /*
        * EMON-specific IOCTL commands
        */

        case DRV_OPERATION_READ_MSR:
            SEP_PRINT_DEBUG("DRV_OPERATION_READ_MSR\n");
            status = lwpmudrv_Read_MSR_All_Cores(&local_args);
            break;

#if defined(EMON)

        case DRV_OPERATION_READ_SWITCH_GROUP:
            SEP_PRINT_DEBUG("DRV_OPERATION_READ_SWITCH_GROUP\n");
            status = lwpmudrv_Read_Counters_And_Switch_Group(&local_args);
            break;

        case DRV_OPERATION_READ_AND_RESET:
            SEP_PRINT_DEBUG("DRV_OPERATION_READ_AND_RESET\n");
            status = lwpmudrv_Read_And_Reset_Counters(&local_args);
            break;
#endif


       /*
        * Platform-specific IOCTL commands (IA32 and Intel64)
        */

        case DRV_OPERATION_INIT_UNC:
            SEP_PRINT_DEBUG("DRV_OPERATION_INIT_UNC\n");
            status = lwpmudrv_Initialize_UNC(local_args.w_buf, local_args.w_len);
            break;

        case DRV_OPERATION_EM_GROUPS_UNC:
            SEP_PRINT_DEBUG("DRV_OPERATION_EM_GROUPS_UNC\n");
            status = lwpmudrv_Set_EM_Config_UNC(&local_args);
            break;

        case DRV_OPERATION_EM_CONFIG_NEXT_UNC:
            SEP_PRINT_DEBUG("DRV_OPERATION_EM_CONFIG_NEXT_UNC\n");
            status = lwpmudrv_Configure_Events_UNC(&local_args);
            break;

        case DRV_OPERATION_LBR_INFO:
            SEP_PRINT_DEBUG("DRV_OPERATION_LBR_INFO\n");
            status = lwpmudrv_LBR_Info(&local_args);
            break;

        case DRV_OPERATION_PWR_INFO:
            SEP_PRINT_DEBUG("DRV_OPERATION_PWR_INFO\n");
            status = lwpmudrv_PWR_Info(&local_args);
            break;

        case DRV_OPERATION_INIT_NUM_DEV:
            SEP_PRINT_DEBUG("DRV_OPERATION_INIT_NUM_DEV\n");
            status = lwpmudrv_Initialize_Num_Devices(&local_args);
            break;
        case DRV_OPERATION_GET_NUM_SAMPLES:
            SEP_PRINT_DEBUG("DRV_OPERATION_GET_NUM_SAMPLES\n");
            status = lwpmudrv_Get_Num_Samples(&local_args);
            break;

        case DRV_OPERATION_SET_DEVICE_NUM_UNITS:
            SEP_PRINT_DEBUG("DRV_OPERATION_SET_DEVICE_NUM_UNITS\n");
            status = lwpmudrv_Set_Device_Num_Units(&local_args);
            break;

        case DRV_OPERATION_TIMER_TRIGGER_READ:
            lwpmudrv_Trigger_Read();
            break;

        case DRV_OPERATION_GET_INTERVAL_COUNTS:
           SEP_PRINT_DEBUG("DRV_OPERATION_GET_INTERVAL_COUNTS\n");
           lwpmudrv_Get_Interval_Counts(&local_args);
           break;

        case DRV_OPERATION_SET_SCAN_UNCORE_TOPOLOGY_INFO:
            SEP_PRINT_DEBUG("LWPMUDRV_IOCTL_SET_SCAN_UNCORE_TOPOLOGY_INFO\n");
            status = lwpmudrv_Set_Uncore_Topology_Info_And_Scan(&local_args);
            break;

        case DRV_OPERATION_GET_UNCORE_TOPOLOGY:
            SEP_PRINT_DEBUG("LWPMUDRV_IOCTL_GET_UNCORE_TOPOLOGY\n");
            status = lwpmudrv_Get_Uncore_Topology(&local_args);
            break;

       /*
        * Graphics IOCTL commands
        */


       /*
        * Chipset IOCTL commands
        */

        case DRV_OPERATION_PCI_READ:
            {
                CHIPSET_PCI_ARG_NODE pci_data;

                SEP_PRINT_DEBUG("DRV_OPERATION_PCI_READ\n");
                if (copy_from_user(&pci_data, (CHIPSET_PCI_ARG)local_args.w_buf, sizeof(CHIPSET_PCI_ARG_NODE))) {
                    status = OS_FAULT;
                    goto cleanup;
                }

                status = PCI_Read_From_Memory_Address(CHIPSET_PCI_ARG_address(&pci_data),
                                               &CHIPSET_PCI_ARG_value(&pci_data));

                if (copy_to_user(local_args.r_buf, &pci_data, sizeof(CHIPSET_PCI_ARG_NODE))) {
                    status =  OS_FAULT;
                    goto cleanup;
                }

                break;
            }

        case DRV_OPERATION_PCI_WRITE:
            {
                CHIPSET_PCI_ARG_NODE pci_data;

                SEP_PRINT_DEBUG("DRV_OPERATION_PCI_WRITE\n");

                if (copy_from_user(&pci_data, (CHIPSET_PCI_ARG)local_args.w_buf, sizeof(CHIPSET_PCI_ARG_NODE))) {
                   status = OS_FAULT;
                    goto cleanup;
                }

                status = PCI_Write_To_Memory_Address(CHIPSET_PCI_ARG_address(&pci_data),
                                                     CHIPSET_PCI_ARG_value(&pci_data));
                break;
            }

        case DRV_OPERATION_FD_PHYS:
            SEP_PRINT_DEBUG("DRV_OPERATION_FD_PHYS\n");
            status = lwpmudrv_Samp_Find_Physical_Address(&local_args);
            break;

        case DRV_OPERATION_READ_PCI_CONFIG:
            SEP_PRINT_DEBUG("DRV_OPERATION_READ_PCI_CONFIG\n");
            status = lwpmudrv_Samp_Read_PCI_Config(&local_args);
            break;

        case DRV_OPERATION_WRITE_PCI_CONFIG:
            SEP_PRINT_DEBUG("DRV_OPERATION_WRITE_PCI_CONFIG\n");
            status = lwpmudrv_Samp_Write_PCI_Config(&local_args);
            break;

        case DRV_OPERATION_CHIPSET_INIT:
            SEP_PRINT_DEBUG("DRV_OPERATION_CHIPSET_INIT\n");
            SEP_PRINT_DEBUG("lwpmudrv_Device_Control: enable_chipset=%d\n",
                            (int)DRV_CONFIG_enable_chipset(pcfg));
            status = lwpmudrv_Samp_Chipset_Init(&local_args);
            break;

        case DRV_OPERATION_GET_CHIPSET_DEVICE_ID:
            SEP_PRINT_DEBUG("DRV_OPERATION_GET_CHIPSET_DEVICE_ID\n");
            status = lwpmudrv_Samp_Read_PCI_Config(&local_args);
            break;

       /*
        * if none of the above, treat as unknown/illegal IOCTL command
        */

        default:
            SEP_PRINT_ERROR("Unknown IOCTL number:%d\n", cmd);
            status = OS_ILLEGAL_IOCTL;
            break;
    }
cleanup:
    MUTEX_UNLOCK(ioctl_lock);
    if (cmd == DRV_OPERATION_STOP &&
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_PREPARE_STOP) {
        status = lwpmudrv_Finish_Stop();
        if (status == OS_SUCCESS) {
            // if stop was successful, relevant memory should have been freed,
            // so try to compact the memory tracker
            CONTROL_Memory_Tracker_Compaction();
        }
    }

    return status;
}

extern long
lwpmu_Device_Control (
    IOCTL_USE_INODE
    struct   file   *filp,
    unsigned int     cmd,
    unsigned long    arg
)
{
    int              status = OS_SUCCESS;
    IOCTL_ARGS_NODE  local_args;

#if !defined(DRV_USE_UNLOCKED_IOCTL)
    SEP_PRINT_DEBUG("lwpmu_DeviceControl(0x%x) called on inode maj:%d, min:%d\n",
            cmd, imajor(inode), iminor(inode));
#endif
    SEP_PRINT_DEBUG("type: %d, subcommand: %d\n", _IOC_TYPE(cmd), _IOC_NR(cmd));

    if (_IOC_TYPE(cmd) != LWPMU_IOC_MAGIC) {
        SEP_PRINT_ERROR("Unknown IOCTL magic:%d\n", _IOC_TYPE(cmd));
        return OS_ILLEGAL_IOCTL;
    }

    if (arg) {
        status = copy_from_user(&local_args, (IOCTL_ARGS)arg, sizeof(IOCTL_ARGS_NODE));
    }

    status = lwpmu_Service_IOCTL (IOCTL_USE_INODE filp, _IOC_NR(cmd), local_args);

    return status;
}

#if defined(HAVE_COMPAT_IOCTL) && defined(DRV_EM64T)
extern long
lwpmu_Device_Control_Compat (
    struct   file   *filp,
    unsigned int     cmd,
    unsigned long    arg
)
{
    int                     status = OS_SUCCESS;
    IOCTL_COMPAT_ARGS_NODE  local_args_compat;
    IOCTL_ARGS_NODE         local_args;

    memset(&local_args_compat, 0, sizeof(IOCTL_COMPAT_ARGS_NODE));
    SEP_PRINT_DEBUG("Compat: type: %d, subcommand: %d\n", _IOC_TYPE(cmd), _IOC_NR(cmd));

    if (_IOC_TYPE(cmd) != LWPMU_IOC_MAGIC) {
        SEP_PRINT_ERROR("Unknown IOCTL magic:%d\n", _IOC_TYPE(cmd));
        return OS_ILLEGAL_IOCTL;
    }

    if (arg) {
        status = copy_from_user(&local_args_compat, (IOCTL_COMPAT_ARGS)arg, sizeof(IOCTL_COMPAT_ARGS_NODE));
    }
    local_args.r_len = local_args_compat.r_len;
    local_args.w_len = local_args_compat.w_len;
    local_args.r_buf = (char *) compat_ptr(local_args_compat.r_buf);
    local_args.w_buf = (char *) compat_ptr(local_args_compat.w_buf);

    status = lwpmu_Service_IOCTL (filp, _IOC_NR(cmd), local_args);

    return status;
}
#endif

/*
 * @fn        LWPMUDRV_Abnormal_Terminate(void)
 *
 * @brief     This routine is called from linuxos_Exit_Task_Notify if the user process has
 *            been killed by an uncatchable signal (example kill -9).  The state variable
 *            abormal_terminate is set to 1 and the clean up routines are called.  In this
 *            code path the OS notifier hooks should not be unloaded.
 *
 * @param     None
 *
 * @return    OS_STATUS
 *
 * <I>Special Notes:</I>
 *     <none>
 */
extern int
LWPMUDRV_Abnormal_Terminate (
    void
)
{
    int              status = OS_SUCCESS;

    abnormal_terminate = 1;
    SEP_PRINT_DEBUG("Abnormal-Termination: Calling lwpmudrv_Prepare_Stop\n");
    status = lwpmudrv_Prepare_Stop();
    SEP_PRINT_DEBUG("Abnormal-Termination: Calling lwpmudrv_Finish_Stop\n");
    status = lwpmudrv_Finish_Stop();
    SEP_PRINT_DEBUG("Abnormal-Termination: Calling lwpmudrv_Terminate\n");
    status = lwpmudrv_Terminate();

    return status;
}


/*****************************************************************************************
 *
 *   Driver Entry / Exit functions that will be called on when the driver is loaded and
 *   unloaded
 *
 ****************************************************************************************/

/*
 * Structure that declares the usual file access functions
 * First one is for lwpmu_c, the control functions
 */
static struct file_operations lwpmu_Fops = {
    .owner =   THIS_MODULE,
    IOCTL_OP = lwpmu_Device_Control,
#if defined(HAVE_COMPAT_IOCTL) && defined(DRV_EM64T)
    .compat_ioctl = lwpmu_Device_Control_Compat,
#endif
    .read =    lwpmu_Read,
    .write =   lwpmu_Write,
    .open =    lwpmu_Open,
    .release = NULL,
    .llseek =  NULL,
};

/*
 * Second one is for lwpmu_m, the module notification functions
 */
static struct file_operations lwmod_Fops = {
    .owner =   THIS_MODULE,
    IOCTL_OP = NULL,                //None needed
    .read =    OUTPUT_Module_Read,
    .write =   NULL,                //No writing accepted
    .open =    lwpmu_Open,
    .release = NULL,
    .llseek =  NULL,
};

/*
 * Third one is for lwsamp_nn, the sampling functions
 */
static struct file_operations lwsamp_Fops = {
    .owner =   THIS_MODULE,
    IOCTL_OP = NULL,                //None needed
    .read =    OUTPUT_Sample_Read,
    .write =   NULL,                //No writing accepted
    .open =    lwpmu_Open,
    .release = NULL,
    .llseek =  NULL,
};

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static int lwpmudrv_setup_cdev(dev, fops, dev_number)
 *
 * @param LWPMU_DEV               dev  - pointer to the device object
 * @param struct file_operations *fops - pointer to the file operations struct
 * @param dev_t                   dev_number - major/monor device number
 *
 * @return OS_STATUS
 *
 * @brief  Set up the device object.
 *
 * <I>Special Notes</I>
 */
static int
lwpmu_setup_cdev (
    LWPMU_DEV               dev,
    struct file_operations *fops,
    dev_t                   dev_number
)
{
    cdev_init(&LWPMU_DEV_cdev(dev), fops);
    LWPMU_DEV_cdev(dev).owner = THIS_MODULE;
    LWPMU_DEV_cdev(dev).ops   = fops;

    return cdev_add(&LWPMU_DEV_cdev(dev), dev_number, 1);
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static int lwpmu_Load(void)
 *
 * @param none
 *
 * @return STATUS
 *
 * @brief  Load the driver module into the kernel.  Set up the driver object.
 * @brief  Set up the initial state of the driver and allocate the memory
 * @brief  needed to keep basic state information.
 */
static int
lwpmu_Load (
    VOID
)
{
    int        i, num_cpus;
    dev_t      lwmod_DevNum;
    OS_STATUS  status      = OS_SUCCESS;
#if defined (DRV_ANDROID)
    char       dev_name[MAXNAMELEN];
#endif

    CONTROL_Memory_Tracker_Init();

    /* Get one major device number and two minor numbers. */
    /*   The result is formatted as major+minor(0) */
    /*   One minor number is for control (lwpmu_c), */
    /*   the other (lwpmu_m) is for modules */
    SEP_PRINT_DEBUG("lwpmu driver loading...\n");
    SEP_PRINT_DEBUG("lwpmu driver about to register chrdev...\n");

    lwpmu_DevNum = MKDEV(0, 0);
    status = alloc_chrdev_region(&lwpmu_DevNum, 0, PMU_DEVICES, SEP_DRIVER_NAME);
    SEP_PRINT_DEBUG("result of alloc_chrdev_region is %d\n", status);
    if (status<0) {
        SEP_PRINT_ERROR("lwpmu driver failed to alloc chrdev_region!\n");
        return status;
    }
    SEP_PRINT_DEBUG("lwpmu driver: major number is %d\n", MAJOR(lwpmu_DevNum));
    status = lwpmudrv_Initialize_State();
    if (status<0) {
        SEP_PRINT_ERROR("lwpmu driver failed to initialize state!\n");
        return status;
    }
    num_cpus = GLOBAL_STATE_num_cpus(driver_state);
    SEP_PRINT_DEBUG("detected %d CPUs in lwpmudrv_Load\n", num_cpus);

    /* Allocate memory for the control structures */
    lwpmu_control = CONTROL_Allocate_Memory(sizeof(LWPMU_DEV_NODE));
    lwmod_control = CONTROL_Allocate_Memory(sizeof(LWPMU_DEV_NODE));
    lwsamp_control= CONTROL_Allocate_Memory(num_cpus*sizeof(LWPMU_DEV_NODE));

    if (!lwsamp_control || !lwpmu_control || !lwmod_control) {
        CONTROL_Free_Memory(lwpmu_control);
        CONTROL_Free_Memory(lwmod_control);
        CONTROL_Free_Memory(lwsamp_control);
        return OS_NO_MEM;
    }

    /* Register the file operations with the OS */

#if defined (DRV_ANDROID)
    pmu_class = class_create(THIS_MODULE, SEP_DRIVER_NAME);
    if (IS_ERR(pmu_class)) {
        SEP_PRINT_ERROR("Error registering SEP control class\n");
    }
    device_create(pmu_class, NULL, lwpmu_DevNum, NULL, SEP_DRIVER_NAME DRV_DEVICE_DELIMITER"c");
#endif

    status = lwpmu_setup_cdev(lwpmu_control,&lwpmu_Fops,lwpmu_DevNum);
    if (status) {
        SEP_PRINT_ERROR("Error %d adding lwpmu as char device\n", status);
        return status;
    }
    /* _c init was fine, now try _m */
    lwmod_DevNum = MKDEV(MAJOR(lwpmu_DevNum),MINOR(lwpmu_DevNum)+1);

#if defined (DRV_ANDROID)
    device_create(pmu_class, NULL, lwmod_DevNum, NULL, SEP_DRIVER_NAME DRV_DEVICE_DELIMITER"m");
#endif

    status       = lwpmu_setup_cdev(lwmod_control,&lwmod_Fops,lwmod_DevNum);
    if (status) {
        SEP_PRINT_ERROR("Error %d adding lwpmu as char device\n", status);
        cdev_del(&LWPMU_DEV_cdev(lwpmu_control));
        return status;
    }

    /* allocate one sampling device per cpu */
    lwsamp_DevNum = MKDEV(0, 0);
    status = alloc_chrdev_region(&lwsamp_DevNum, 0, num_cpus, SEP_SAMPLES_NAME);

    if (status<0) {
        SEP_PRINT_ERROR("lwpmu driver failed to alloc chrdev_region!\n");
        return status;
    }

    /* Register the file operations with the OS */
    for (i = 0; i < num_cpus; i++) {
#if defined (DRV_ANDROID)
        snprintf(dev_name, MAXNAMELEN, "%s%ss%d", SEP_DRIVER_NAME, DRV_DEVICE_DELIMITER, i);
        device_create(pmu_class, NULL, lwsamp_DevNum+i, NULL, dev_name);
#endif
        status = lwpmu_setup_cdev(lwsamp_control+i,
                                  &lwsamp_Fops,
                                  lwsamp_DevNum+i);
        if (status) {
            SEP_PRINT_ERROR("Error %d adding lwpmu as char device\n", status);
            return status;
        }
        else {
            SEP_PRINT_DEBUG("added sampling device %d\n", i);
        }
    }

    tsc_info            = CONTROL_Allocate_Memory(GLOBAL_STATE_num_cpus(driver_state)*sizeof(U64));
    atomic_set(&read_now, GLOBAL_STATE_num_cpus(driver_state));
    init_waitqueue_head(&read_tsc_now);
    CONTROL_Invoke_Parallel(lwpmudrv_Fill_TSC_Info, (PVOID)(size_t)0);

    pcb_size            = GLOBAL_STATE_num_cpus(driver_state)*sizeof(CPU_STATE_NODE);
    pcb                 = CONTROL_Allocate_Memory(pcb_size);
    core_to_package_map = CONTROL_Allocate_Memory(GLOBAL_STATE_num_cpus(driver_state)*sizeof(U32));
    SYS_INFO_Build();
    pcb                 = CONTROL_Free_Memory(pcb);
    pcb_size            = 0;
    if (total_ram <= OUTPUT_MEMORY_THRESHOLD) {
        output_buffer_size = OUTPUT_SMALL_BUFFER;
    }

    MUTEX_INIT(ioctl_lock);
    in_finish_code = 0;

    memset((char *)&uncore_topology, 0, sizeof(UNCORE_TOPOLOGY_INFO_NODE));

    /*
     *  Initialize the SEP driver version (done once at driver load time)
     */
    SEP_VERSION_NODE_major(&drv_version) = SEP_MAJOR_VERSION;
    SEP_VERSION_NODE_minor(&drv_version) = SEP_MINOR_VERSION;
    SEP_VERSION_NODE_api(&drv_version)   = SEP_API_VERSION;
    SEP_VERSION_NODE_update(&drv_version)= SEP_UPDATE_VERSION;

    //
    // Display driver version information
    //
    SEP_PRINT("PMU collection driver v%d.%d.%d%s%s has been loaded.\n",
              SEP_VERSION_NODE_major(&drv_version),
              SEP_VERSION_NODE_minor(&drv_version),
              SEP_VERSION_NODE_api(&drv_version),
              SEP_UPDATE_STRING,
              SEP_DRIVER_MODE);

    SEP_PRINT("Chipset support is enabled.\n");


    SEP_PRINT("IDT vector 0x%x will be used for handling PMU interrupts.\n", CPU_PERF_VECTOR);

    return status;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  static int lwpmu_Unload(void)
 *
 * @param none
 *
 * @return none
 *
 * @brief  Remove the driver module from the kernel.
 */
static VOID
lwpmu_Unload (
    VOID
)
{
    int i = 0;
    int num_cpus = GLOBAL_STATE_num_cpus(driver_state);

    SEP_PRINT_DEBUG("lwpmu driver unloading...\n");
    LINUXOS_Uninstall_Hooks();
    SYS_INFO_Destroy();
    OUTPUT_Destroy();
    cpu_buf             = CONTROL_Free_Memory(cpu_buf);
    module_buf          = CONTROL_Free_Memory(module_buf);
    pcb                 = CONTROL_Free_Memory(pcb);
    pcb_size            = 0;
    tsc_info            = CONTROL_Free_Memory(tsc_info);
    core_to_package_map = CONTROL_Free_Memory(core_to_package_map);

#if defined (DRV_ANDROID)
    unregister_chrdev(MAJOR(lwpmu_DevNum), SEP_DRIVER_NAME);
    device_destroy(pmu_class, lwpmu_DevNum);
    device_destroy(pmu_class, lwpmu_DevNum+1);
#endif

    cdev_del(&LWPMU_DEV_cdev(lwpmu_control));
    cdev_del(&LWPMU_DEV_cdev(lwmod_control));
    unregister_chrdev_region(lwpmu_DevNum, PMU_DEVICES);

#if defined (DRV_ANDROID)
    unregister_chrdev(MAJOR(lwsamp_DevNum), SEP_SAMPLES_NAME);
#endif

    for (i = 0; i < num_cpus; i++) {
#if defined (DRV_ANDROID)
        device_destroy(pmu_class, lwsamp_DevNum+i);
#endif
        cdev_del(&LWPMU_DEV_cdev(&lwsamp_control[i]));
    }

#if defined (DRV_ANDROID)
    class_destroy(pmu_class);
#endif

    unregister_chrdev_region(lwsamp_DevNum, num_cpus);
    lwpmu_control  = CONTROL_Free_Memory(lwpmu_control);
    lwmod_control  = CONTROL_Free_Memory(lwmod_control);
    lwsamp_control = CONTROL_Free_Memory(lwsamp_control);

    CONTROL_Memory_Tracker_Free();

    //
    // Display driver version information
    //
    SEP_PRINT("PMU collection driver v%d.%d.%d%s%s has been unloaded.\n",
              SEP_VERSION_NODE_major(&drv_version),
              SEP_VERSION_NODE_minor(&drv_version),
              SEP_VERSION_NODE_api(&drv_version),
              SEP_UPDATE_STRING,
              SEP_DRIVER_MODE);

    return;
}

/* Declaration of the init and exit functions */
module_init(lwpmu_Load);
module_exit(lwpmu_Unload);
