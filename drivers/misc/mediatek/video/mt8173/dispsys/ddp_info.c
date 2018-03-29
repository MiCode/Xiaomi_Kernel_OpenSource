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

#define LOG_TAG "INFO"
#include"ddp_info.h"
#include"ddp_debug.h"
#include "ddp_log.h"

char *ddp_get_module_name(DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_UFOE:
		return "ufoe ";
	case DISP_MODULE_AAL:
		return "aal ";
	case DISP_MODULE_COLOR0:
		return "color0 ";
	case DISP_MODULE_COLOR1:
		return "color1 ";
	case DISP_MODULE_RDMA0:
		return "rdma0 ";
	case DISP_MODULE_RDMA1:
		return "rdma1 ";
	case DISP_MODULE_RDMA2:
		return "rdma2 ";
	case DISP_MODULE_WDMA0:
		return "wdma0 ";
	case DISP_MODULE_WDMA1:
		return "wdma1 ";
	case DISP_MODULE_OVL0:
		return "ovl0 ";
	case DISP_MODULE_OVL1:
		return "ovl1 ";
	case DISP_MODULE_GAMMA:
		return "gamma ";
	case DISP_MODULE_PWM0:
		return "pwm0 ";
	case DISP_MODULE_PWM1:
		return "pwm1 ";
	case DISP_MODULE_OD:
		return "od ";
	case DISP_MODULE_MERGE:
		return "merge ";
	case DISP_MODULE_SPLIT0:
		return "split0 ";
	case DISP_MODULE_SPLIT1:
		return "split1 ";
	case DISP_MODULE_DSI0:
		return "dsi0 ";
	case DISP_MODULE_DSI1:
		return "dsi1 ";
	case DISP_MODULE_DSIDUAL:
		return "dsidual ";
	case DISP_MODULE_DPI0:
		return "dpi0 ";
	case DISP_MODULE_DPI1:
		return "dpi1 ";
	case DISP_MODULE_SMI_COMMON:
		return "smi ";
	case DISP_MODULE_CONFIG:
		return "config ";
	case DISP_MODULE_CMDQ:
		return "cmdq ";
	case DISP_MODULE_MUTEX:
		return "mutex ";
	default:
		DDPDBG("invalid module id=%d\n", module);
		return "unknown";
	}
}

int ddp_get_module_max_irq_bit(DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_UFOE:
		return 0;
	case DISP_MODULE_AAL:
		return 1;
	case DISP_MODULE_COLOR0:
		return 2;
	case DISP_MODULE_COLOR1:
		return 2;
	case DISP_MODULE_RDMA0:
		return 5;
	case DISP_MODULE_RDMA1:
		return 5;
	case DISP_MODULE_RDMA2:
		return 5;
	case DISP_MODULE_WDMA0:
		return 1;
	case DISP_MODULE_WDMA1:
		return 1;
	case DISP_MODULE_OVL0:
		return 3;
	case DISP_MODULE_OVL1:
		return 3;
	case DISP_MODULE_GAMMA:
		return 0;
	case DISP_MODULE_PWM0:
		return 0;
	case DISP_MODULE_PWM1:
		return 0;
	case DISP_MODULE_OD:
		return 0;
	case DISP_MODULE_MERGE:
		return 0;
	case DISP_MODULE_SPLIT0:
		return 0;
	case DISP_MODULE_SPLIT1:
		return 0;
	case DISP_MODULE_DSI0:
		return 6;
	case DISP_MODULE_DSI1:
		return 6;
	case DISP_MODULE_DSIDUAL:
		return 6;
	case DISP_MODULE_DPI0:
		return 2;
	case DISP_MODULE_SMI_COMMON:
		return 0;
	case DISP_MODULE_CONFIG:
		return 0;
	case DISP_MODULE_CMDQ:
		return 0;
	case DISP_MODULE_MUTEX:
		return 14;
	default:
		DDPMSG("invalid module id=%d", module);
	}
	return 0;
}

unsigned int ddp_module_to_idx(int module)
{
	unsigned int id = 0;

	switch (module) {
	case DISP_MODULE_UFOE:
	case DISP_MODULE_AAL:
	case DISP_MODULE_COLOR0:
	case DISP_MODULE_RDMA0:
	case DISP_MODULE_WDMA0:
	case DISP_MODULE_OVL0:
	case DISP_MODULE_GAMMA:
	case DISP_MODULE_PWM0:
	case DISP_MODULE_OD:
	case DISP_MODULE_SPLIT0:
	case DISP_MODULE_DSI0:
	case DISP_MODULE_DPI0:
		id = 0;
		break;

	case DISP_MODULE_COLOR1:
	case DISP_MODULE_RDMA1:
	case DISP_MODULE_WDMA1:
	case DISP_MODULE_OVL1:
	case DISP_MODULE_PWM1:
	case DISP_MODULE_SPLIT1:
	case DISP_MODULE_DSI1:
		id = 1;
		break;
	case DISP_MODULE_RDMA2:
	case DISP_MODULE_DSIDUAL:
		id = 2;
		break;
	default:
		DDPERR("ddp_module_to_idx, module=0x%x\n", module);
	}

	return id;
}

DDP_MODULE_DRIVER *ddp_modules_driver[DISP_MODULE_NUM];

unsigned int ddp_driver_init(void)
{
	ddp_modules_driver[DISP_MODULE_OVL0] = &ddp_driver_ovl;
	ddp_modules_driver[DISP_MODULE_OVL1] = &ddp_driver_ovl;
	ddp_modules_driver[DISP_MODULE_RDMA0] = &ddp_driver_rdma;
	ddp_modules_driver[DISP_MODULE_RDMA1] = &ddp_driver_rdma;
	ddp_modules_driver[DISP_MODULE_RDMA2] = &ddp_driver_rdma;
	ddp_modules_driver[DISP_MODULE_WDMA0] = &ddp_driver_wdma;
	ddp_modules_driver[DISP_MODULE_WDMA1] = &ddp_driver_wdma;
	ddp_modules_driver[DISP_MODULE_COLOR0] = &ddp_driver_color;
	ddp_modules_driver[DISP_MODULE_COLOR1] = &ddp_driver_color;
	ddp_modules_driver[DISP_MODULE_AAL] = &ddp_driver_aal;
	ddp_modules_driver[DISP_MODULE_OD] = &ddp_driver_od;
	ddp_modules_driver[DISP_MODULE_GAMMA] = &ddp_driver_gamma;
	ddp_modules_driver[DISP_MODULE_SPLIT0] = &ddp_driver_split;
	ddp_modules_driver[DISP_MODULE_SPLIT1] = &ddp_driver_split;
	ddp_modules_driver[DISP_MODULE_UFOE] = &ddp_driver_ufoe;
	ddp_modules_driver[DISP_MODULE_DSI0] = &ddp_driver_dsi0;
	ddp_modules_driver[DISP_MODULE_DSI1] = &ddp_driver_dsi1;
	ddp_modules_driver[DISP_MODULE_DSIDUAL] = &ddp_driver_dsidual;
	ddp_modules_driver[DISP_MODULE_DPI0] = &ddp_driver_dpi0;
	ddp_modules_driver[DISP_MODULE_DPI1] = &ddp_driver_dpi1;
	ddp_modules_driver[DISP_MODULE_PWM0] = &ddp_driver_pwm;
	ddp_modules_driver[DISP_MODULE_PWM1] = &ddp_driver_pwm;

	return 0;
}
