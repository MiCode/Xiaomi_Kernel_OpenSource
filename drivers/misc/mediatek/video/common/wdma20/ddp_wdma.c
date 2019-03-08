// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define LOG_TAG "WDMA"
#include "ddp_log.h"
#include <linux/delay.h>
#include "ddp_reg.h"
#include "ddp_matrix_para.h"
#include "ddp_info.h"
#include "ddp_wdma.h"
#include "ddp_wdma_ex.h"
#include "primary_display.h"
#ifdef CONFIG_MTK_M4U
#include "m4u.h"
#endif

#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))


unsigned long wdma_base_addr(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_WDMA0:
		return DDP_REG_BASE_DISP_WDMA0;
	case DISP_MODULE_WDMA1:
		return DDP_REG_BASE_DISP_WDMA1;
	default:
		DDPERR("invalid wdma module=%d\n", module);
	}
	return 0;
}

unsigned int wdma_index(enum DISP_MODULE_ENUM module)
{
	int idx = 0;

	switch (module) {
	case DISP_MODULE_WDMA0:
		idx = 0;
		break;
	case DISP_MODULE_WDMA1:
		idx = 1;
		break;
	default:
		/* invalid module */
		DDPERR("[DDP] error: invalid wdma module=%d\n", module);
		ASSERT(0);
	}
	return idx;
}

int wdma_stop(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned long base_addr = wdma_base_addr(module);

	DISP_REG_SET(handle, base_addr + DISP_REG_WDMA_INTEN, 0x00);
	DISP_REG_SET(handle, base_addr + DISP_REG_WDMA_EN, 0x00);
	DISP_REG_SET(handle, base_addr + DISP_REG_WDMA_INTSTA, 0x00);

	return 0;
}

int wdma_reset(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int delay_cnt = 0;
	unsigned int idx = wdma_index(module);
	unsigned long base_addr = wdma_base_addr(module);

	/* trigger soft reset */
	DISP_REG_SET(handle, base_addr + DISP_REG_WDMA_RST, 0x01);
	if (!handle) {
		while ((DISP_REG_GET(base_addr + DISP_REG_WDMA_FLOW_CTRL_DBG) &
			0x1) == 0) {
			delay_cnt++;
			udelay(10);
			if (delay_cnt > 2000) {
				DDPERR("wdma%d reset timeout!\n", idx);
				break;
			}
		}
	} else {
		/* add comdq polling */
	}
	/* trigger soft reset */
	DISP_REG_SET(handle, base_addr + DISP_REG_WDMA_RST, 0x0);

	return 0;
}

unsigned int ddp_wdma_get_cur_addr(enum DISP_MODULE_ENUM module)
{
	unsigned long base_addr = wdma_base_addr(module);

	return INREG32(base_addr + DISP_REG_WDMA_DST_ADDR0);
}
