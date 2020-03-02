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
	DISP0_SMI_LARB1,
	CLK_MM_GALS_COMM0,
	CLK_MM_GALS_COMM1,
	/* module clk */
	DISP0_DISP_OVL0,
	DISP0_DISP_OVL0_2L,
	DISP0_DISP_OVL1_2L,
	DISP0_DISP_RDMA0,
	DISP0_DISP_RDMA1,
	DISP0_DISP_WDMA0,
	DISP0_DISP_COLOR0,
	DISP0_DISP_CCORR0,
	DISP0_DISP_AAL0,
	DISP0_DISP_GAMMA0,
	DISP0_DISP_DITHER0,
	DISP1_DSI0_MM_CLOCK,
	DISP1_DSI0_INTERFACE_CLOCK,
	DISP1_DPI_MM_CLOCK,
	DISP1_DPI_INTERFACE_CLOCK,
	DISP0_DBI_MM_CLOCK,
	DISP0_DBI_INTERFACE_CLOCK,
	DISP0_DISP_26M,
	DISP0_DISP_RSZ,
	/* topgen clock */
	TOP_MUX_MM,
	TOP_MUX_DISP_PWM,
	DISP_PWM,
	TOP_26M,
	TOP_UNIVPLL_D3_D2,
	TOP_UNIVPLL_D3_D4,
	TOP_OSC_D2,
	TOP_OSC_D4,
	TOP_OSC_D16,
	MUX_DPI0,
	TVDPLL_D2,
	TVDPLL_D4,  /* 30 */
	TVDPLL_D8,
	TVDPLL_D16,
	DPI_CK,
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
int ddp_ext_modules_clk_on(void);
int ddp_ext_modules_clk_off(void);
int ddp_ovl2mem_modules_clk_on(void);
int ddp_ovl2mem_modules_clk_off(void);
int ddp_module_clk_enable(enum DISP_MODULE_TYPE_ENUM module_t);
int ddp_module_clk_disable(enum DISP_MODULE_TYPE_ENUM module_t);
enum DDP_CLK_ID ddp_get_module_clk_id(enum DISP_MODULE_ENUM module_id);
void ddp_clk_force_on(unsigned int on);
int ddp_clk_check(void);
int ddp_ovl_dcm_reset(void);
int ddp_clk_enable_by_module(enum DISP_MODULE_ENUM module);
int ddp_clk_disable_by_module(enum DISP_MODULE_ENUM module);
extern void mipi_26m_en(unsigned int module_idx, int en);
#endif
