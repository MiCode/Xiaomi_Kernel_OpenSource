/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef	_USB_OSAI_H_
#define	_USB_OSAI_H_
#include <linux/delay.h>
#include <linux/spinlock_types.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/random.h>
#include <linux/slab.h>
#include "mu3d_hal_comm.h"
#include "mu3d_hal_hw.h"

#undef EXTERN

#ifdef _USB_OSAI_EXT_
#define EXTERN
#else
#define EXTERN \
extern
#endif

#define K_EMERG	(1<<7)
#define K_QMU	(1<<7)
#define K_ALET		(1<<6)
#define K_CRIT		(1<<5)
#define K_ERR		(1<<4)
#define K_WARNIN	(1<<3)
#define K_NOTICE	(1<<2)
#define K_INFO		(1<<1)
#define K_DEBUG	(1<<0)

/*Set the debug level at musb_core.c*/
extern u32 debug_level;

#ifdef USE_SSUSB_QMU
#define qmu_printk(level, fmt, args...) do { \
		if (debug_level & (level|K_QMU)) { \
			pr_err("[U3D][Q]" fmt, ## args); \
		} \
	} while (0)
#endif

#define os_printk(level, fmt, args...) do { \
		if (debug_level & level) { \
			pr_err("[U3D]" fmt, ## args); \
		} \
	} while (0)

#define OS_R_OK                    ((DEV_INT32)   0)

EXTERN spinlock_t _lock;
EXTERN DEV_INT32 os_reg_isr(DEV_UINT32 irq, irq_handler_t handler, void *isrbuffer);
/* USBIF */
EXTERN void os_free_isr(DEV_UINT32 irq, void *isrbuffer);
EXTERN void os_ms_delay(DEV_UINT32 ui4_delay);
EXTERN void os_us_delay(DEV_UINT32 ui4_delay);
EXTERN void os_ms_sleep(DEV_UINT32 ui4_sleep);

void os_memcpy(DEV_INT8 *pv_to, DEV_INT8 *pv_from, size_t z_l);
EXTERN void *os_memset(void *pv_to, DEV_UINT8 ui1_c, size_t z_l);
EXTERN void *os_mem_alloc(size_t z_size);

EXTERN void *os_phys_to_virt(void *paddr);

EXTERN void os_mem_free(void *pv_mem);
EXTERN void os_disableIrq(DEV_UINT32 irq);
EXTERN void os_disableIrq(DEV_UINT32 irq);
EXTERN void os_enableIrq(DEV_UINT32 irq);
EXTERN void os_clearIrq(DEV_UINT32 irq);
EXTERN void os_get_random_bytes(void *buf, DEV_INT32 nbytes);
EXTERN void os_disableDcache(void);
EXTERN void os_flushinvalidateDcache(void);
extern DEV_INT32 rand(void);
extern void HalFlushInvalidateDCache(void);


#undef EXTERN

#endif
