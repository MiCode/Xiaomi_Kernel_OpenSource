/* Copyright (c) 2010-2011,2013-2016, The Linux Foundation. All rights reserved.
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

#include <linux/types.h>
#include "kgsl_device.h"

extern unsigned int kgsl_cff_dump_enable;

static inline bool kgsl_cffdump_flags_no_memzero(void) { return true; }

struct kgsl_device_private;

#ifdef CONFIG_QCOM_KGSL_CFF_DUMP

#define CFF_OP_WRITE_MEM        0x0000000b

void kgsl_cffdump_init(void);
void kgsl_cffdump_destroy(void);
void kgsl_cffdump_open(struct kgsl_device *device);
void kgsl_cffdump_close(struct kgsl_device *device);
void kgsl_cffdump_memcpy(struct kgsl_device *device, uint64_t gpuaddr,
		unsigned int *ptr, uint64_t sizebytes);
void kgsl_cffdump_syncmem(struct kgsl_device *, struct kgsl_mem_entry *,
	uint64_t offset, uint64_t sizebytes, bool clean_cache);
void kgsl_cffdump_memset(struct kgsl_device *device, uint64_t addr,
			unsigned char value, size_t sizebytes);
void kgsl_cffdump_regwrite(struct kgsl_device *device, uint addr,
	uint value);
void kgsl_cffdump_regpoll(struct kgsl_device *device, uint addr,
	uint value, uint mask);
bool kgsl_cffdump_parse_ibs(struct kgsl_device_private *dev_priv,
	const struct kgsl_memdesc *memdesc, uint64_t gpuaddr,
	uint64_t sizedwords, bool check_only);
void kgsl_cffdump_user_event(struct kgsl_device *device,
		unsigned int cff_opcode, unsigned int op1,
		unsigned int op2, unsigned int op3,
		unsigned int op4, unsigned int op5);

void kgsl_cffdump_memory_base(struct kgsl_device *device, unsigned int base,
			      unsigned int range, unsigned int gmemsize);

void kgsl_cffdump_hang(struct kgsl_device *device);
void kgsl_cffdump_debugfs_create(struct kgsl_device *device);
int kgsl_cff_dump_enable_set(void *data, u64 val);
int kgsl_cff_dump_enable_get(void *data, u64 *val);
int kgsl_cffdump_capture_ib_desc(struct kgsl_device *device,
				struct kgsl_context *context,
				struct kgsl_drawobj_cmd *cmdobj);

void kgsl_cffdump_printline(int id, uint opcode, uint op1, uint op2,
	uint op3, uint op4, uint op5);

static inline void kgsl_cffdump_write(struct kgsl_device *device,
		uint64_t gpuaddr, unsigned int value)
{
	if (!device || !device->cff_dump_enable)
		return;

	kgsl_cffdump_printline(-1, CFF_OP_WRITE_MEM, gpuaddr, value, 0, 0, 0);
}

#else

static inline void kgsl_cffdump_init(void)
{
	return;
}

static inline void kgsl_cffdump_destroy(void)
{
	return;
}

static inline void kgsl_cffdump_open(struct kgsl_device *device)
{
	return;
}

static inline void kgsl_cffdump_close(struct kgsl_device *device)
{
	return;
}

static inline void kgsl_cffdump_write(struct kgsl_device *device,
		uint64_t gpuaddr, unsigned int value)
{
	return;
}

static inline void kgsl_cffdump_memcpy(struct kgsl_device *device,
		uint64_t gupaddr, unsigned int *ptr, uint64_t sizebytes)
{
	return;
}

static inline void kgsl_cffdump_syncmem(struct kgsl_device *device,
		struct kgsl_mem_entry *entry, uint64_t offset,
		uint64_t sizebytes, bool clean_cache)
{
	return;
}

static inline void kgsl_cffdump_memset(struct kgsl_device *device,
		uint64_t addr, unsigned char ch, size_t sizebytes)
{
	return;
}

static inline void kgsl_cffdump_regwrite(struct kgsl_device *device, uint addr,
					 uint value)
{
	return;
}

static inline void kgsl_cffdump_regpoll(struct kgsl_device *device, uint addr,
		uint value, uint mask)
{
	return;
}

static inline bool kgsl_cffdump_parse_ibs(struct kgsl_device_private *dev_priv,
	const struct kgsl_memdesc *memdesc, uint64_t gpuaddr,
	uint64_t sizedwords, bool check_only)
{
	return false;
}

static inline void kgsl_cffdump_memory_base(struct kgsl_device *device,
		unsigned int base, unsigned int range, unsigned int gmemsize)
{
	return;
}

static inline void kgsl_cffdump_hang(struct kgsl_device *device)
{
	return;
}

static inline void kgsl_cffdump_debugfs_create(struct kgsl_device *device)
{
	return;
}

static inline void kgsl_cffdump_user_event(struct kgsl_device *device,
		unsigned int cff_opcode, unsigned int op1,
		unsigned int op2, unsigned int op3,
		unsigned int op4, unsigned int op5)
{
	return;
}

static inline int kgsl_cffdump_capture_ib_desc(struct kgsl_device *device,
				struct kgsl_context *context,
				struct kgsl_drawobj_cmd *cmdobj)
{
	return 0;
}

static inline int kgsl_cff_dump_enable_set(void *data, u64 val)
{
	return -ENODEV;
}

static inline int kgsl_cff_dump_enable_get(void *data, u64 *val)
{
	return -ENODEV;
}

#endif /* CONFIG_QCOM_KGSL_CFF_DUMP */
#endif /* __KGSL_CFFDUMP_H */
