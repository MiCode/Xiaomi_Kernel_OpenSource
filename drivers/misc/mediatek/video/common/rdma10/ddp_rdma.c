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

#define LOG_TAG "RDMA"

#include <linux/delay.h>
#ifdef CONFIG_MTK_M4U
#include "m4u.h"
#endif
#include "ddp_log.h"
#include "ddp_reg.h"
#include "ddp_matrix_para.h"
#include "ddp_dump.h"
#include "lcm_drv.h"
#include "ddp_rdma.h"
#include "ddp_rdma_ex.h"


unsigned int rdma_index(enum DISP_MODULE_ENUM module)
{
	int idx = 0;

	switch (module) {
	case DISP_MODULE_RDMA0:
		idx = 0;
		break;
	case DISP_MODULE_RDMA1:
		idx = 1;
		break;
	case DISP_MODULE_RDMA2:
		idx = 2;
		break;
	default:
		DDPERR("invalid rdma module=%d\n", module);
	/* invalid module */
		ASSERT(0);
	}
	ASSERT((idx >= 0) && (idx < RDMA_INSTANCES));
	return idx;
}

void rdma_set_target_line(enum DISP_MODULE_ENUM module,
			  unsigned int line, void *handle)
{
	unsigned int idx = rdma_index(module);

	DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
		     DISP_REG_RDMA_TARGET_LINE, line);
}

int rdma_init(enum DISP_MODULE_ENUM module, void *handle)
{
	return rdma_clock_on(module, handle);
}

int rdma_deinit(enum DISP_MODULE_ENUM module, void *handle)
{
	return rdma_clock_off(module, handle);
}

void rdma_get_address(enum DISP_MODULE_ENUM module, unsigned long *addr)
{
	unsigned int idx = rdma_index(module);

	*addr = DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR +
			     DISP_RDMA_INDEX_OFFSET * idx);
}
