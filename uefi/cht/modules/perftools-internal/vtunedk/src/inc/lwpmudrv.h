/*
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
*/


#ifndef _LWPMUDRV_H_
#define _LWPMUDRV_H_

#include <linux/kernel.h>
#include <linux/compat.h>
#include "lwpmudrv_defines.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_types.h"
#include "lwpmudrv_version.h"
#include "lwpmudrv_struct.h"
#include "lwpmudrv_chipset.h"


/*
 * Print macros for driver messages
 */

#if defined(MYDEBUG)
#define SEP_PRINT_DEBUG(fmt,args...) { printk(KERN_INFO SEP_MSG_PREFIX" [DEBUG] " fmt,##args); }
#else
#define SEP_PRINT_DEBUG(fmt,args...) {;}
#endif

#define SEP_PRINT(fmt,args...) { printk(KERN_INFO SEP_MSG_PREFIX" " fmt,##args); }

#define SEP_PRINT_WARNING(fmt,args...) { printk(KERN_ALERT SEP_MSG_PREFIX" [Warning] " fmt,##args); }

#define SEP_PRINT_ERROR(fmt,args...) { printk(KERN_CRIT SEP_MSG_PREFIX" [ERROR] " fmt,##args); }

// Macro to return the thread group id
#define GET_CURRENT_TGID() (current->tgid)

#define OVERFLOW_ARGS  U64*, U64*

/*
 *  Dispatch table for virtualized functions.
 *  Used to enable common functionality for different
 *  processor microarchitectures
 */
typedef struct DISPATCH_NODE_S  DISPATCH_NODE;
typedef        DISPATCH_NODE   *DISPATCH;

struct DISPATCH_NODE_S {
    VOID (*init)(PVOID);
    VOID (*fini)(PVOID);
    VOID (*write)(PVOID);
    VOID (*freeze)(PVOID);
    VOID (*restart)(PVOID);
    VOID (*read_data)(PVOID);
    VOID (*check_overflow)(DRV_MASKS);
    VOID (*swap_group)(DRV_BOOL);
    U64  (*read_lbrs)(PVOID);
    VOID (*cleanup)(PVOID);
    VOID (*hw_errata)(VOID);
    VOID (*read_power)(PVOID);
    U64  (*check_overflow_errata)(ECB, U32, U64);
    VOID (*read_counts)(PVOID, U32);
    U64  (*check_overflow_gp_errata)(ECB,U64*);
    VOID (*read_ro)(PVOID, U32, U32);
    VOID (*platform_info)(PVOID);
    VOID (*trigger_read)(VOID);    // Counter reads triggered/initiated by User mode timer
    VOID (*scan_for_uncore)(PVOID);
};

extern DISPATCH dispatch;

/*
 *  Dispatch table for virtualized functions.
 *  Used to enable common functionality for different
 *  chipset types
 */
typedef struct CS_DISPATCH_NODE_S  CS_DISPATCH_NODE;
typedef        CS_DISPATCH_NODE   *CS_DISPATCH;
struct CS_DISPATCH_NODE_S {
    U32  (*init_chipset)(VOID);    // initialize chipset (must be called before the others!)
    VOID (*start_chipset)(VOID);   // start the chipset counters
    VOID (*read_counters)(PVOID);  // at interrupt time, read out the chipset counters
    VOID (*stop_chipset)(VOID);    // stop the chipset counters
    VOID (*fini_chipset)(VOID);    // clean up resources and reset chipset state (called last)
    VOID (*Trigger_Read)(VOID);    // GMCH counter reads triggered/initiated by User mode timer
};
extern CS_DISPATCH    cs_dispatch;

extern VOID         **PMU_register_data;
extern VOID         **desc_data;
extern U64           *prev_counter_data;
extern U64           *cur_counter_data;

/*!
 * @struct LWPMU_DEVICE_NODE_S
 * @brief  Struct to hold fields per device
 *           PMU_register_data_unc - MSR info
 *           dispatch_unc          - dispatch table
 *           em_groups_counts_unc  - # groups
 *           pcfg_unc              - config struct
 */
typedef struct LWPMU_DEVICE_NODE_S  LWPMU_DEVICE_NODE;
typedef        LWPMU_DEVICE_NODE   *LWPMU_DEVICE;

struct LWPMU_DEVICE_NODE_S {
    VOID       **PMU_register_data_unc;
    DISPATCH   dispatch_unc;
    S32        em_groups_count_unc;
    VOID       *pcfg_unc;
    U64        **acc_per_thread;
    U64        **prev_val_per_thread;
    U64        counter_mask;
    U64        num_events;
    U32        num_units;
    VOID       *ec;
    S32        cur_group;
};

#define LWPMU_DEVICE_PMU_register_data(dev)   (dev)->PMU_register_data_unc
#define LWPMU_DEVICE_dispatch(dev)            (dev)->dispatch_unc
#define LWPMU_DEVICE_em_groups_count(dev)     (dev)->em_groups_count_unc
#define LWPMU_DEVICE_pcfg(dev)                (dev)->pcfg_unc
#define LWPMU_DEVICE_acc_per_thread(dev)      (dev)->acc_per_thread
#define LWPMU_DEVICE_prev_val_per_thread(dev) (dev)->prev_val_per_thread
#define LWPMU_DEVICE_counter_mask(dev)        (dev)->counter_mask
#define LWPMU_DEVICE_num_events(dev)          (dev)->num_events
#define LWPMU_DEVICE_num_units(dev)           (dev)->num_units
#define LWPMU_DEVICE_ec(dev)                  (dev)->ec
#define LWPMU_DEVICE_cur_group(dev)           (dev)->cur_group

extern U32            num_devices;
extern U32            cur_devices;
extern LWPMU_DEVICE   devices;
extern U64           *pmu_state;

// Handy macro
#define TSC_SKEW(this_cpu)     (tsc_info[this_cpu] - tsc_info[0])

/*
 *  The IDT / GDT descriptor for use in identifying code segments
 */
#if defined(DRV_EM64T)
#pragma pack(push,1)
typedef struct _idtgdtDesc {
    U16    idtgdt_limit;
    PVOID  idtgdt_base;
} IDTGDT_DESC;
#pragma pack(pop)

extern IDTGDT_DESC         gdt_desc;
#endif

#endif  
