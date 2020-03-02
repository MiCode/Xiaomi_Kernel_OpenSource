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

#ifndef PMEM_API_H
#define PMEM_API_H

/**********************************************************/
/*********** PMEM APIs for ION kernel driver **************/
/**********************************************************/
#ifdef CONFIG_MTK_PROT_MEM_SUPPORT
int pmem_api_alloc(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle,
		   u8 *owner, u32 id);
int pmem_api_alloc_zero(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle,
			u8 *owner, u32 id);
int pmem_api_unref(u32 sec_handle, u8 *owner, u32 id);
#endif

#endif /* end of PMEM_API_H */
