/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_MEM_RSC_H__
#define __MTK_APU_MDW_MEM_RSC_H__

#include <linux/dma-buf.h>

/* memory type */
enum {
	APUSYS_MEMORY_NONE,

	APUSYS_MEMORY_CODE,
	APUSYS_MEMORY_DATA,
	APUSYS_MEMORY_MAX = 64, //total support 64 different memory
};

struct apusys_memory {
	struct device *dev;
	int mem_type;
};


int mdw_mem_rsc_register(struct device *dev, int type);
int mdw_mem_rsc_unregister(int type);

int mdw_mem_rsc_init(void);
void mdw_mem_rsc_deinit(void);
struct device *mdw_mem_rsc_get_dev(int type);

#endif
