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

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) \
	|| defined(CONFIG_BLOWFISH_TEE_SUPPORT) \
	|| defined(CONFIG_MTK_TEE_GP_SUPPORT)
#include "secmem_plat.h"
#endif

#ifdef SECMEM_64BIT_PHYS_SUPPORT
#define SECMEM_NAME     "secmem64"
#else
#define SECMEM_NAME     "secmem32"
#endif

#define MAX_NAME_SIZE   32

struct secmem_param {

#ifdef SECMEM_64BIT_PHYS_SUPPORT
	u64 alignment;  /* IN */
	u64 size;       /* IN */
	u32 refcount;   /* INOUT */
	u64 sec_handle; /* OUT */
#else
	u32 alignment;  /* IN */
	u32 size;       /* IN */
	u32 refcount;   /* INOUT */
	u32 sec_handle; /* OUT */
#endif

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

#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SSMR)
/* SVP CMA API */
extern int secmem_region_offline(phys_addr_t *pa, unsigned long *size);
extern int secmem_region_offline64(phys_addr_t *pa, unsigned long *size);
extern int secmem_region_online(void);
extern void spm_enable_sodi(bool en);
#endif

#ifdef SECMEM_KERNEL_API
/* APIs for ION */
#if 0
int secmem_api_alloc(u64 alignment, u64 size, u32 *refcount, u64 *sec_handle,
		     uint8_t *owner, uint32_t id);
int secmem_api_alloc_zero(u64 alignment, u64 size, u32 *refcount,
			  u64 *sec_handle, uint8_t *owner, uint32_t id);
int secmem_api_unref(u64 sec_handle, uint8_t *owner, uint32_t id);
#else
int secmem_api_alloc(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle,
		     uint8_t *owner, uint32_t id);
int secmem_api_alloc_zero(u32 alignment, u32 size, u32 *refcount,
			  u32 *sec_handle, uint8_t *owner, uint32_t id);
int secmem_api_unref(u32 sec_handle, uint8_t *owner, uint32_t id);
#endif
#endif /* SECMEM_KERNEL_API */

#endif/* SECMEM_H */
