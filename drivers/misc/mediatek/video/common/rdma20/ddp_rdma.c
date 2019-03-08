// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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


unsigned long rdma_base_addr(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_RDMA0:
		return DDP_REG_BASE_DISP_RDMA0;
	case DISP_MODULE_RDMA1:
		return DDP_REG_BASE_DISP_RDMA1;
	case DISP_MODULE_RDMA2:
		return DDP_REG_BASE_DISP_RDMA2;
	default:
		DDPERR("invalid rdma module=%d\n", module);
	}
	return 0;
}

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
		ASSERT(0);
	}
	ASSERT((idx >= 0) && (idx < RDMA_INSTANCES));
	return idx;
}

void rdma_set_target_line(enum DISP_MODULE_ENUM module,
					unsigned int line, void *handle)
{
	unsigned long base_addr = rdma_base_addr(module);

	DISP_REG_SET(handle, base_addr + DISP_REG_RDMA_TARGET_LINE, line);
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
	unsigned long base_addr = rdma_base_addr(module);

	*addr = DISP_REG_GET(base_addr + DISP_REG_RDMA_MEM_START_ADDR);
}

