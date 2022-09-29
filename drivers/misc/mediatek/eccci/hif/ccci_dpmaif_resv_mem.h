/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __CCCI__DPMA_RESV_MEM_H__
#define __CCCI__DPMA_RESV_MEM_H__

#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/dmapool.h>



void ccci_dpmaif_resv_mem_init(void);

int ccci_dpmaif_get_resv_cache_mem(void **vir_base,
		dma_addr_t *phy_base, unsigned int size);

int ccci_dpmaif_get_resv_nocache_mem(void **vir_base,
		dma_addr_t *phy_base, unsigned int size);


#endif				/* __CCCI__DPMA_RESV_MEM_H__ */
