/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#define LOG_TAG "INFO"
#include "ddp_info.h"
#include "ddp_debug.h"
#include "ddp_log.h"

#define DDP_MODULE_REG_RANGE	(0x1000)

static const char reg_magic[] = "no_regs_info";

/**
 * {
 *  module_id,
 *  module_type,
 *  module_name,
 *  can_connect,
 *  module_driver,
 *
 *  {
 *   reg_dt_name,
 *   reg_pa_check,
 *   reg_irq_check,
 *   irq_max_bit,
 *   reg_va,
 *   reg_irq
 *  }
 * },
 */
struct ddp_module ddp_modules[DISP_MODULE_NUM] = {
	[DISP_MODULE_OVL0] = {
		DISP_MODULE_OVL0,
		DISP_T_OVL,
		"ovl0",
		1,
		&ddp_driver_ovl,
		{"mediatek,disp_ovl0", 0x14008000, 281, 14, 0, 0}
	},

	[DISP_MODULE_OVL0_2L] = {
		DISP_MODULE_OVL0_2L,
		DISP_T_OVL,
		"ovl0_2l",
		1,
		&ddp_driver_ovl,
		{"mediatek,disp_ovl0_2l", 0x14009000, 282, 14, 0, 0}
	},

	[DISP_MODULE_OVL1_2L] = {
		DISP_MODULE_OVL1_2L,
		DISP_T_OVL,
		"ovl1_2l",
		1,
		&ddp_driver_ovl,
		{"mediatek,disp_ovl1_2l", 0x1400a000, 283, 14, 0, 0}
	},

	[DISP_MODULE_RDMA0] = {
		DISP_MODULE_RDMA0,
		DISP_T_RDMA,
		"rdma0",
		1,
		&ddp_driver_rdma,
		{"mediatek,disp_rdma0", 0x1400b000, 284, 7, 0, 0}
	},

	[DISP_MODULE_RDMA1] = {
		DISP_MODULE_RDMA1,
		DISP_T_RDMA,
		"rdma1",
		1,
		&ddp_driver_rdma,
		{"mediatek,disp_rdma1", 0x1400c000, 285, 7, 0, 0}
	},

	[DISP_MODULE_RDMA_VIRTUAL0] = {
		DISP_MODULE_RDMA_VIRTUAL0,
		DISP_T_UNKNOWN,
		"rdma_virtual0",
		1,
		NULL,
		{reg_magic,}
	},

	[DISP_MODULE_WDMA0] = {
		DISP_MODULE_WDMA0,
		DISP_T_WDMA,
		"wdma0",
		1,
		&ddp_driver_wdma,
		{"mediatek,disp_wdma0", 0x1400d000, 286, 1, 0, 0}
	},

	[DISP_MODULE_WDMA_VIRTUAL0] = {
		DISP_MODULE_WDMA_VIRTUAL0,
		DISP_T_UNKNOWN,
		"wdma_virtual0",
		1,
		NULL,
		{reg_magic,}
	},

	[DISP_MODULE_WDMA_VIRTUAL1] = {
		DISP_MODULE_WDMA_VIRTUAL1,
		DISP_T_UNKNOWN,
		"wdma_virtual1",
		1,
		NULL,
		{reg_magic,}
	},

	[DISP_MODULE_COLOR0] = {
		DISP_MODULE_COLOR0,
		DISP_T_COLOR,
		"color0",
		1,
		&ddp_driver_color,
		{"mediatek,disp_color0", 0x1400e000, 287, 0, 0, 0}
	},

	[DISP_MODULE_CCORR0] = {
		DISP_MODULE_CCORR0,
		DISP_T_CCORR,
		"ccorr0",
		1,
		&ddp_driver_ccorr,
		{"mediatek,disp_ccorr0", 0x1400f000, 288, 1, 0, 0}
	},

	[DISP_MODULE_AAL0] = {
		DISP_MODULE_AAL0,
		DISP_T_AAL,
		"aal0",
		1,
		&ddp_driver_aal,
		{"mediatek,disp_aal0", 0x14010000, 289, 1, 0, 0}
	},

	[DISP_MODULE_GAMMA0] = {
		DISP_MODULE_GAMMA0,
		DISP_T_GAMMA,
		"gamma0",
		1,
		&ddp_driver_gamma,
		{"mediatek,disp_gamma0", 0x14011000, 290, 0, 0, 0}
	},

	[DISP_MODULE_DITHER0] = {
		DISP_MODULE_DITHER0,
		DISP_T_DITHER,
		"dither0",
		1,
		&ddp_driver_dither,
		{"mediatek,disp_dither0", 0x14012000, 291, 0, 0, 0}
	},

	[DISP_MODULE_DSI0] = {
		DISP_MODULE_DSI0,
		DISP_T_DSI,
		"dsi0",
		1,
		&ddp_driver_dsi0,
		{"mediatek,dsi0", 0x14014000, 292, 15, 0, 0}
	},

	[DISP_MODULE_DSI1] = {
		DISP_MODULE_DSI1,
		DISP_T_DSI,
		"dsi1",
		0,
		NULL,
		{reg_magic,}
	},

	[DISP_MODULE_DSIDUAL] = {
		DISP_MODULE_DSIDUAL,
		DISP_T_DSI,
		"dsidual",
		0,
		NULL,
		{reg_magic,}
	},

	[DISP_MODULE_PWM0] = {
		DISP_MODULE_PWM0,
		DISP_T_PWM,
		"pwm0",
		0,
		&ddp_driver_pwm,
		{"mediatek,disp_pwm0", 0x1100e000, 183, 0, 0, 0}
	},

	[DISP_MODULE_CONFIG] = {
		DISP_MODULE_CONFIG,
		DISP_T_UNKNOWN,
		"config",
		0,
		NULL,
		{"mediatek,mmsys_config", 0x14000000, 0, 0, 0, 0}
	},

	[DISP_MODULE_MUTEX] = {
		DISP_MODULE_MUTEX,
		DISP_T_UNKNOWN,
		"mutex",
		0,
		NULL,
		{"mediatek,mm_mutex", 0x14016000, 273, 21, 0, 0}
	},

	[DISP_MODULE_SMI_COMMON] = {
		DISP_MODULE_SMI_COMMON,
		DISP_T_UNKNOWN,
		"smi_common",
		0,
		NULL,
		{"mediatek,smi_common", 0x14019000, 0, 0, 0, 0}
	},

	[DISP_MODULE_SMI_LARB0] = {
		DISP_MODULE_SMI_LARB0,
		DISP_T_UNKNOWN,
		"smi_larb0",
		0,
		NULL,
		{"mediatek,smi_larb0", 0x14017000, 296, 0, 0, 0}
	},

	[DISP_MODULE_SMI_LARB1] = {
		DISP_MODULE_SMI_LARB1,
		DISP_T_UNKNOWN,
		"smi_larb1",
		0,
		NULL,
		{"mediatek,smi_larb1", 0x14018000, 294, 0, 0, 0}
	},

	[DISP_MODULE_MIPI0] = {
		DISP_MODULE_MIPI0,
		DISP_T_UNKNOWN,
		"mipi0",
		0,
		NULL,
		{"mediatek,mipi_tx0", 0x11e50000, 0, 0, 0, 0}
	},

	[DISP_MODULE_MIPI1] = {
		DISP_MODULE_MIPI1,
		DISP_T_UNKNOWN,
		"mipi1",
		0,
		NULL,
		{reg_magic,}
	},

	[DISP_MODULE_RSZ0] = {
		DISP_MODULE_RSZ0,
		DISP_T_RSZ,
		"rsz0",
		1,
		&ddp_driver_rsz,
		{"mediatek,disp_rsz0", 0x1401a000, 297, 0, 0, 0}
	},

	[DISP_MODULE_DPI] = {
		DISP_MODULE_DPI,
		DISP_T_DPI,
		"dpi",
		1,
		NULL,
		{"mediatek,dpi0", 0x14015000, 293, 3, 0, 0}
	},

	[DISP_MODULE_DPI_VIRTUAL] = {
		DISP_MODULE_DPI_VIRTUAL,
		DISP_T_UNKNOWN,
		"dpi_virtual",
		1,
		NULL,
		{reg_magic,}
	},

	[DISP_MODULE_POSTMASK] = {
		DISP_MODULE_POSTMASK,
		DISP_T_POSTMASK,
		"postmask",
		1,
		&ddp_driver_postmask,
		{"mediatek,disp_postmask0", 0x14021000, 300, 13, 0, 0}
	},

	[DISP_MODULE_UNKNOWN] = {
		DISP_MODULE_UNKNOWN,
		DISP_T_UNKNOWN,
		"unknown",
		0,
		NULL,
		{reg_magic,}
	},
};

unsigned int is_ddp_module(enum DISP_MODULE_ENUM module)
{
	if (module >= 0 && module < DISP_MODULE_NUM)
		return 1;

	return 0;
}

unsigned int is_ddp_module_has_reg_info(enum DISP_MODULE_ENUM module)
{
	if (!is_ddp_module(module))
		return 0;

	if (strcmp(ddp_modules[module].reg_info.reg_dt_name, reg_magic))
		return 1;

	return 0;
}

const char *ddp_get_module_name(enum DISP_MODULE_ENUM module)
{
	if (is_ddp_module(module))
		return ddp_modules[module].module_name;

	DDPMSG("%s: invalid module id=%d\n", __func__, module);
	return "unknown-module";
}

unsigned int _can_connect(enum DISP_MODULE_ENUM module)
{
	if (is_ddp_module(module))
		return ddp_modules[module].can_connect;

	DDPMSG("%s: invalid module id=%d\n", __func__, module);
	return 0;
}

struct DDP_MODULE_DRIVER *ddp_get_module_driver(enum DISP_MODULE_ENUM module)
{
	if (is_ddp_module(module))
		return ddp_modules[module].module_driver;

	DDPMSG("%s: invalid module id=%d\n", __func__, module);
	return 0;
}

void ddp_set_module_driver(enum DISP_MODULE_ENUM module,
	struct DDP_MODULE_DRIVER *drv)
{
	if (!is_ddp_module(module))
		return;

	ddp_modules[module].module_driver = drv;
}

const char *ddp_get_module_dtname(enum DISP_MODULE_ENUM module)
{
	if (is_ddp_module(module))
		return ddp_modules[module].reg_info.reg_dt_name;

	DDPMSG("%s: invalid module id=%d\n", __func__, module);
	return "unknown";
}

unsigned int ddp_get_module_checkirq(enum DISP_MODULE_ENUM module)
{
	if (is_ddp_module_has_reg_info(module))
		return ddp_modules[module].reg_info.reg_irq_check;

	DDPMSG("%s: invalid module id=%d\n", __func__, module);
	return 0;
}

unsigned long ddp_get_module_pa(enum DISP_MODULE_ENUM module)
{
	if (is_ddp_module_has_reg_info(module))
		return ddp_modules[module].reg_info.reg_pa_check;

	DDPMSG("%s: invalid module id=%d\n", __func__, module);
	return 0;
}

unsigned int ddp_get_module_max_irq_bit(enum DISP_MODULE_ENUM module)
{
	if (is_ddp_module_has_reg_info(module))
		return ddp_modules[module].reg_info.irq_max_bit;

	DDPMSG("%s: invalid module id=%d\n", __func__, module);
	return 0;
}

unsigned int ddp_is_irq_enable(enum DISP_MODULE_ENUM module)
{
#if (defined(CONFIG_MTK_TEE_GP_SUPPORT) || \
	defined(CONFIG_TRUSTONIC_TEE_SUPPORT)) && \
	defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	if (module == DISP_MODULE_WDMA0)
		return 0;
#endif

	if (ddp_get_module_max_irq_bit(module))
		return 1;

	return 0;
}

void ddp_module_irq_disable(enum DISP_MODULE_ENUM module)
{
	if (is_ddp_module_has_reg_info(module)) {
		ddp_modules[module].reg_info.irq_max_bit = 0;
		return;
	}

	DDPMSG("%s: invalid module id=%d\n", __func__, module);
}

void ddp_set_module_va(enum DISP_MODULE_ENUM module, unsigned long va)
{
	if (is_ddp_module_has_reg_info(module)) {
		ddp_modules[module].reg_info.reg_va = va;
		return;
	}

	DDPMSG("%s: invalid module id=%d\n", __func__, module);
}

unsigned long ddp_get_module_va(enum DISP_MODULE_ENUM module)
{
	if (is_ddp_module_has_reg_info(module))
		return ddp_modules[module].reg_info.reg_va;

	return 0;
}

void ddp_info_set_module_irq(enum DISP_MODULE_ENUM module, unsigned int irq)
{
	if (is_ddp_module_has_reg_info(module)) {
		ddp_modules[module].reg_info.reg_irq = irq;
		return;
	}

	DDPMSG("%s: invalid module id=%d\n", __func__, module);
}

unsigned int ddp_get_module_irq(enum DISP_MODULE_ENUM module)
{
	if (is_ddp_module_has_reg_info(module))
		return ddp_modules[module].reg_info.reg_irq;

	DDPMSG("%s: invalid module id=%d\n", __func__, module);
	return 0;
}

unsigned int is_reg_addr_valid(unsigned int isVa, unsigned long addr)
{
	unsigned int i = 0;

	for (i = 0; i < DISP_MODULE_NUM; i++) {
		if ((isVa == 1) && (addr >= ddp_get_module_va(i)) &&
		    (addr < ddp_get_module_va(i) + DDP_MODULE_REG_RANGE))
			break;

		if ((isVa == 0) && (addr >= ddp_get_module_pa(i)) &&
		    (addr < ddp_get_module_pa(i) + DDP_MODULE_REG_RANGE))
			break;
	}

	if (i < DISP_MODULE_NUM) {
		DDPMSG("addr valid, isVa=0x%x, addr=0x%08lx, module=%s!\n",
		       isVa, addr, ddp_get_module_name(i));
		return i;
	}

	DDP_PR_ERR("%s: fail, isVa=0x%x, addr=0x%08lx!\n",
		   __func__, isVa, addr);
	return 0;
}

unsigned int ddp_get_module_num_by_t(enum DISP_MODULE_TYPE_ENUM module_t)
{
	int i;
	int cnt = 0;

	for (i = 0; i < DISP_MODULE_NUM; i++) {
		if (ddp_modules[i].module_type == module_t)
			cnt++;
	}

	return cnt;
}

enum DISP_MODULE_ENUM
ddp_get_module_id_by_idx(enum DISP_MODULE_TYPE_ENUM module_t, unsigned int idx)
{
	int i;
	int index = 0;

	for (i = 0; i < DISP_MODULE_NUM; i++) {
		if (ddp_modules[i].module_type == module_t)
			index++;
		if (index == (idx + 1))
			return i;
	}

	return DISP_MODULE_UNKNOWN;
}

const char *ddp_get_ioctl_name(enum DDP_IOCTL_NAME ioctl)
{
	switch (ioctl) {
	case DDP_SWITCH_DSI_MODE:
		return "DDP_SWITCH_DSI_MODE";
	case DDP_STOP_VIDEO_MODE:
		return "DDP_STOP_VIDEO_MODE";
	case DDP_BACK_LIGHT:
		return "DDP_BACK_LIGHT";
	case DDP_SWITCH_LCM_MODE:
		return "DDP_SWITCH_LCM_MODE";
	case DDP_DPI_FACTORY_TEST:
		return "DDP_DPI_FACTORY_TEST";
	case DDP_DSI_IDLE_CLK_CLOSED:
		return "DDP_DSI_IDLE_CLK_CLOSED";
	case DDP_DSI_IDLE_CLK_OPEN:
		return "DDP_DSI_IDLE_CLK_OPEN";
	case DDP_DSI_PORCH_CHANGE:
		return "DDP_DSI_PORCH_CHANGE";
	case DDP_PHY_CLK_CHANGE:
		return "DDP_PHY_CLK_CHANGE";
	case DDP_ENTER_ULPS:
		return "DDP_ENTER_ULPS";
	case DDP_EXIT_ULPS:
		return "DDP_EXIT_ULPS";
	case DDP_RDMA_GOLDEN_SETTING:
		return "DDP_RDMA_GOLDEN_SETTING";
	case DDP_OVL_GOLDEN_SETTING:
		return "DDP_OVL_GOLDEN_SETTING";
	case DDP_PARTIAL_UPDATE:
		return "DDP_PARTIAL_UPDATE";
	case DDP_UPDATE_PLL_CLK_ONLY:
		return "DDP_UPDATE_PLL_CLK_ONLY";
	case DDP_DSI_SW_INIT:
		return "DDP_DSI_SW_INIT";
	case DDP_DSI_MIPI_POWER_ON:
		return "DDP_DSI_MIPI_POWER_ON";
	default:
		break;
	}
	return "unknown-ioctl";
}
