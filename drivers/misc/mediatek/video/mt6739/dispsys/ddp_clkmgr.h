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
	/* mmsys top clk */
	DISP_MTCMOS_CLK = 0,
	DISP0_SMI_COMMON,
	DISP0_SMI_LARB0,

	CLK_MM_GALS_COMM0,
	CLK_MM_GALS_COMM1,

	/* module clk */
	DISP0_DISP_OVL0,
	DISP0_DISP_RDMA0,
	DISP0_DISP_WDMA0,
	DISP0_DISP_COLOR0,
	DISP0_DISP_CCORR0,
	DISP0_DISP_AAL0,
	DISP0_DISP_GAMMA0,
	DISP0_DISP_DITHER0,
	DISP1_DSI0_MM_CLOCK,
	DISP1_DSI0_INTERFACE_CLOCK,
	DISP0_DISP_26M,
	MDP_WROT0,
	DISP_PWM,
	MUX_PWM,
	CLK26M,
	UNIVPLL2_D4,
	UNIVPLL2_D8,
	UNIVPLL3_D8,
	MAX_DISP_CLK_CNT
};

typedef struct {
	struct clk *pclk;
	const char *clk_name;
	int refcnt;
	unsigned int belong_to; /* bit 0: main display , bit 1: second display */
	enum DISP_MODULE_ENUM module_id;
} ddp_clk;

const char *ddp_get_clk_name(unsigned int n);
int ddp_set_clk_handle(struct clk *pclk, unsigned int n);
#if 0
int ddp_clk_prepare(enum DDP_CLK_ID id);
int ddp_clk_unprepare(enum DDP_CLK_ID id);
int ddp_clk_enable(enum DDP_CLK_ID id);
int ddp_clk_disable(enum DDP_CLK_ID id);
#endif
int ddp_clk_prepare_enable(enum DDP_CLK_ID id);
int ddp_clk_disable_unprepare(enum DDP_CLK_ID id);
int ddp_clk_set_parent(enum DDP_CLK_ID id, enum DDP_CLK_ID parent);
int ddp_set_mipi26m(enum DISP_MODULE_ENUM module, int en);
int ddp_parse_apmixed_base(void);
int ddp_main_modules_clk_on(void);
int ddp_main_modules_clk_off(void);
int ddp_ext_modules_clk_on(void);
int ddp_ext_modules_clk_off(void);
int ddp_ovl2mem_modules_clk_on(void);
int ddp_ovl2mem_modules_clk_off(void);
int ddp_module_clk_enable(enum DISP_MODULE_TYPE_ENUM module_t);
int ddp_module_clk_disable(enum DISP_MODULE_TYPE_ENUM module_t);
enum DDP_CLK_ID ddp_get_module_clk_id(enum DISP_MODULE_ENUM module_id);
void ddp_clk_force_on(unsigned int on);
extern void check_mm0_clk_sts(void);
void ddp_clk_tree_dump(void);

#endif
