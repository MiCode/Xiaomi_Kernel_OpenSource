/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __KGSL_CFFDUMP_H
#define __KGSL_CFFDUMP_H

#ifdef CONFIG_MSM_KGSL_CFF_DUMP

#include <linux/types.h>

#include "kgsl_device.h"

void kgsl_cffdump_init(void);
void kgsl_cffdump_destroy(void);
void kgsl_cffdump_open(enum kgsl_deviceid device_id);
void kgsl_cffdump_close(enum kgsl_deviceid device_id);
void kgsl_cffdump_syncmem(struct kgsl_device_private *dev_priv,
	const struct kgsl_memdesc *memdesc, uint physaddr, uint sizebytes,
	bool clean_cache);
void kgsl_cffdump_setmem(uint addr, uint value, uint sizebytes);
void kgsl_cffdump_regwrite(enum kgsl_deviceid device_id, uint addr,
	uint value);
void kgsl_cffdump_regpoll(enum kgsl_deviceid device_id, uint addr,
	uint value, uint mask);
bool kgsl_cffdump_parse_ibs(struct kgsl_device_private *dev_priv,
	const struct kgsl_memdesc *memdesc, uint gpuaddr, int sizedwords,
	bool check_only);
void kgsl_cffdump_user_event(unsigned int cff_opcode, unsigned int op1,
		unsigned int op2, unsigned int op3,
		unsigned int op4, unsigned int op5);
static inline bool kgsl_cffdump_flags_no_memzero(void) { return true; }

void kgsl_cffdump_memory_base(enum kgsl_deviceid device_id, unsigned int base,
			      unsigned int range, unsigned int gmemsize);

void kgsl_cffdump_hang(enum kgsl_deviceid device_id);

#else

#define kgsl_cffdump_init()					(void)0
#define kgsl_cffdump_destroy()					(void)0
#define kgsl_cffdump_open(device_id)				(void)0
#define kgsl_cffdump_close(device_id)				(void)0
#define kgsl_cffdump_syncmem(dev_priv, memdesc, addr, sizebytes, clean_cache) \
	(void) 0
#define kgsl_cffdump_setmem(addr, value, sizebytes)		(void)0
#define kgsl_cffdump_regwrite(device_id, addr, value)		(void)0
#define kgsl_cffdump_regpoll(device_id, addr, value, mask)	(void)0
#define kgsl_cffdump_parse_ibs(dev_priv, memdesc, gpuaddr, \
	sizedwords, check_only)					true
#define kgsl_cffdump_flags_no_memzero()				true
#define kgsl_cffdump_memory_base(base, range, gmemsize)		(void)0
#define kgsl_cffdump_hang(device_id)				(void)0
#define kgsl_cffdump_user_event(cff_opcode, op1, op2, op3, op4, op5) \
	(void)param

#endif /* CONFIG_MSM_KGSL_CFF_DUMP */

#endif /* __KGSL_CFFDUMP_H */
