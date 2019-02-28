/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef SECMEM_API_H
#define SECMEM_API_H

/**********************************************************/
/********** SECMEM APIs for ION kernel driver *************/
/**********************************************************/
#ifdef CONFIG_MTK_SECURE_MEM_SUPPORT
int secmem_api_alloc(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle,
		     uint8_t *owner, uint32_t id);
int secmem_api_alloc_zero(u32 alignment, u32 size, u32 *refcount,
			  u32 *sec_handle, uint8_t *owner, uint32_t id);
int secmem_api_unref(u32 sec_handle, uint8_t *owner, uint32_t id);
#endif

/**********************************************************/
/****** SECMEM EXTEND APIs for ION kernel driver **********/
/**********************************************************/
#ifdef CONFIG_MTK_SECURE_MEM_SUPPORT

enum SECMEM_VIRT_SHARE_REGION {
	SECMEM_VIRT_SHARE_REGION_SVP = 0,
	SECMEM_VIRT_SHARE_REGION_2D_FR = 1,
};

int secmem_api_alloc_ext(u32 alignment, u32 size, u32 *refcount,
			 u32 *sec_handle, uint8_t *owner, uint32_t id,
			 enum SECMEM_VIRT_SHARE_REGION region);
int secmem_api_alloc_zero_ext(u32 alignment, u32 size, u32 *refcount,
			      u32 *sec_handle, uint8_t *owner, uint32_t id,
			      enum SECMEM_VIRT_SHARE_REGION region);
int secmem_api_unref_ext(u32 sec_handle, uint8_t *owner, uint32_t id,
			 enum SECMEM_VIRT_SHARE_REGION region);
#endif

#endif /* end of SECMEM_API_H */
