/* ***********************************************************************************************

  This file is provided under a dual BSD/GPLv2 license.  When using or 
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2005-2014 Intel Corporation. All rights reserved.

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

  BSD LICENSE 

  Copyright(c) 2005-2014 Intel Corporation. All rights reserved.
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


#ifndef _SOCPERFDRV_H_
#define _SOCPERFDRV_H_

#include <linux/kernel.h>
#include <linux/compat.h>
#include "lwpmudrv_defines.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_types.h"
#include "lwpmudrv_version.h"
#include "lwpmudrv_struct.h"


/*
 * Print macros for driver messages
 */

#if defined(MYDEBUG)
#define SOCPERF_PRINT_DEBUG(fmt,args...) { printk(KERN_INFO SOCPERF_MSG_PREFIX" [DEBUG] " fmt,##args); }
#else
#define SOCPERF_PRINT_DEBUG(fmt,args...) {;}
#endif

#define SOCPERF_PRINT(fmt,args...) { printk(KERN_INFO SOCPERF_MSG_PREFIX" " fmt,##args); }

#define SOCPERF_PRINT_WARNING(fmt,args...) { printk(KERN_ALERT SOCPERF_MSG_PREFIX" [Warning] " fmt,##args); }

#define SOCPERF_PRINT_ERROR(fmt,args...) { printk(KERN_CRIT SOCPERF_MSG_PREFIX" [ERROR] " fmt,##args); }

// Macro to return the thread group id
#define GET_CURRENT_TGID() (current->tgid)

#if defined(DRV_IA32) || defined(DRV_EM64T)
#define OVERFLOW_ARGS  U64*, U64*
#elif defined(DRV_IA64)
#define OVERFLOW_ARGS  U64*, U64*, U64*, U64*, U64*, U64*
#endif

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
    VOID (*read_lbrs)(PVOID);
    VOID (*clean_up)(PVOID);
    VOID (*hw_errata)(VOID);
    VOID (*read_power)(PVOID);
    U64  (*check_overflow_errata)(ECB, U32, U64);
    VOID (*read_counts)(PVOID, U32);
    U64  (*check_overflow_gp_errata)(ECB,U64*);
    VOID (*read_ro)(PVOID, U32, U32);
    U64  (*platform_info)(VOID);
    VOID (*trigger_read)(VOID);    // Counter reads triggered/initiated by User mode timer
    VOID (*read_current_data)(PVOID);
    VOID (*create_mem)(U32, U64*);
    VOID (*check_status)(U64*, U32*);
    VOID (*read_mem)(U64, U64*, U32);
    VOID (*stop_mem)(VOID);
};

extern DISPATCH dispatch;

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
extern LWPMU_DEVICE   device_uncore;
extern U64           *pmu_state;

// Handy macro
#define TSC_SKEW(this_cpu)     (tsc_info[this_cpu] - tsc_info[0])

#endif  
