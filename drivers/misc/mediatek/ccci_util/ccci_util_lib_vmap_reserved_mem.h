/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _CCCI_UTIL_LIB_VMAP_RESERVED_MEM_H_
#define _CCCI_UTIL_LIB_VMAP_RESERVED_MEM_H_
void *vmap_reserved_mem(phys_addr_t start, phys_addr_t size,
		pgprot_t prot);
#endif /* end of _CCCI_UTIL_LIB_VMAP_RESERVED_MEM_H_ */
