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

#ifndef SECMEM_H
#define SECMEM_H

#include "secmem_plat.h"

#define SECMEM_NAME     "secmem"

#define MAX_NAME_SIZE   32

struct secmem_param {
	u32 alignment;  /* IN */
	u32 size;       /* IN */
	u32 refcount;   /* INOUT */
	u32 sec_handle; /* OUT */
#ifdef SECMEM_DEBUG_DUMP
	uint32_t id;
	uint8_t owner[MAX_NAME_SIZE];
	uint32_t owner_len;
#endif
};

#define SECMEM_IOC_MAGIC      'T'
#define SECMEM_MEM_ALLOC      _IOWR(SECMEM_IOC_MAGIC, 1, struct secmem_param)
#define SECMEM_MEM_REF        _IOWR(SECMEM_IOC_MAGIC, 2, struct secmem_param)
#define SECMEM_MEM_UNREF      _IOWR(SECMEM_IOC_MAGIC, 3, struct secmem_param)
#define SECMEM_MEM_ALLOC_TBL  _IOWR(SECMEM_IOC_MAGIC, 4, struct secmem_param)
#define SECMEM_MEM_UNREF_TBL  _IOWR(SECMEM_IOC_MAGIC, 5, struct secmem_param)
#define SECMEM_MEM_USAGE_DUMP _IOWR(SECMEM_IOC_MAGIC, 6, struct secmem_param)
#ifdef SECMEM_DEBUG_DUMP
#define SECMEM_MEM_DUMP_INFO  _IOWR(SECMEM_IOC_MAGIC, 7, struct secmem_param)
#endif

#define SECMEM_IOC_MAXNR      (10)


#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP)
/* SVP CMA API */
extern int svp_region_offline(phys_addr_t *pa, unsigned long *size);
extern int svp_region_online(void);
extern void spm_enable_sodi(bool);
#endif

#ifdef SECMEM_KERNEL_API
/* APIs for ION */
int secmem_api_alloc(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle, uint8_t *owner, uint32_t id);
int secmem_api_alloc_zero(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle, uint8_t *owner, uint32_t id);
int secmem_api_alloc_pa(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle, uint8_t *owner, uint32_t id);
int secmem_api_unref(u32 sec_handle, uint8_t *owner, uint32_t id);
int secmem_api_unref_pa(u32 sec_handle, uint8_t *owner, uint32_t id);
#endif /* SECMEM_KERNEL_API */

#endif				/* end of SECMEM_H */
