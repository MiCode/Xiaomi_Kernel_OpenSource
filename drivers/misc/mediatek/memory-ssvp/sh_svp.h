/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/
#ifndef __SH_SVP_H__
#define __SH_SVP_H__

#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP)
int svp_region_offline(phys_addr_t *pa, unsigned long *size);
int svp_region_online(void);

extern struct single_cma_registration memory_ssvp_registration;

#ifdef CONFIG_TRUSTONIC_TEE_SUPPORT
extern int secmem_api_enable(u32 start, u32 size);
extern int secmem_api_disable(void);
extern int secmem_api_query(u32 *allocate_size);
#else
static inline int secmem_api_enable(u32 start, u32 size) {return 0; }
static inline int secmem_api_disable(void) {return 0; }
static inline int secmem_api_query(u32 *allocate_size) {return 0; }
#endif

#else
static inline int svp_region_offline(void) { return -ENOSYS; }
static inline int svp_region_online(void) { return -ENOSYS; }
#endif

extern struct cma *svp_contiguous_default_area;

#define SVP_REGION_IOC_MAGIC		'S'

#define SVP_REGION_IOC_ONLINE		_IOR(SVP_REGION_IOC_MAGIC, 2, int)
#define SVP_REGION_IOC_OFFLINE		_IOR(SVP_REGION_IOC_MAGIC, 4, int)

#define SVP_REGION_ACQUIRE			_IOR(SVP_REGION_IOC_MAGIC, 6, int)
#define SVP_REGION_RELEASE			_IOR(SVP_REGION_IOC_MAGIC, 8, int)

void show_pte(struct mm_struct *mm, unsigned long addr);

#endif
