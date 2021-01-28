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

#ifndef __DDP_CLK_MGR_H__
#define __DDP_CLK_MGR_H__

#include "ddp_hal.h"

#include <linux/clk.h>


/* display clk id
 * -- by chip
 */
enum DDP_CLK_ID {
	/* top clk */
	CLK_MM_MTCMOS = 0,
	CLK_SMI_COMMON,
	CLK_SMI_LARB0,
	CLK_GALS_COMM0,
	CLK_GALS_COMM1,
	/* module clk */
	CLK_DISP_OVL0,
	CLK_DISP_OVL0_2L,
	CLK_DISP_RDMA0,
	CLK_DISP_WDMA0,
	CLK_DISP_COLOR0,
	CLK_DISP_CCORR0,
	CLK_DISP_AAL0,
	CLK_DISP_GAMMA0,
	CLK_DISP_DITHER0,
	CLK_DSI0_MM_CLK,
	CLK_DSI0_IF_CLK,
	CLK_IMG_DL_RELAY,
	CLK_MM_26M,
	CLK_DISP_RSZ0,
	MIPI_26M,
	/* PWM clk */
	MUX_PWM,
	DISP_PWM,
	CLK26M,
	UNIVPLL2_D4,
	ULPOSC1_D2,
	ULPOSC1_D8,
	MAX_DISP_CLK_CNT
};

struct ddp_clk {
	struct clk *pclk;
	const char *clk_name;
	int refcnt;
	/* bit 0: main display , bit 1: second display */
	unsigned int belong_to;
	enum DISP_MODULE_ENUM module_id;
};

const char *ddp_get_clk_name(unsigned int n);
int ddp_set_clk_handle(struct clk *pclk, unsigned int n);

int ddp_clk_prepare_enable(enum DDP_CLK_ID id);
int ddp_clk_disable_unprepare(enum DDP_CLK_ID id);
int ddp_clk_set_parent(enum DDP_CLK_ID id, enum DDP_CLK_ID parent);
int ddp_set_mipi26m(enum DISP_MODULE_ENUM module, int en);
int ddp_parse_apmixed_base(void);
int ddp_main_modules_clk_on(void);
int ddp_main_modules_clk_off(void);
int ddp_module_clk_enable(enum DISP_MODULE_TYPE_ENUM module_t);
int ddp_module_clk_disable(enum DISP_MODULE_TYPE_ENUM module_t);
enum DDP_CLK_ID ddp_get_module_clk_id(enum DISP_MODULE_ENUM module_id);
void ddp_clk_force_on(unsigned int on);
int ddp_clk_check(void);
int ddp_clk_enable_by_module(enum DISP_MODULE_ENUM module);
int ddp_clk_disable_by_module(enum DISP_MODULE_ENUM module);
#endif
