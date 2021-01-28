/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef __DDP_CLK_MGR_H__
#define __DDP_CLK_MGR_H__

#include "ddp_hal.h"

#include <linux/clk.h>


/**
 * display clk id
 * -- by chip
 */
enum DDP_CLK_ID {
	/* top clk */
	CLK_SMI_COMMON = 0,
	CLK_SMI_LARB0,
	CLK_SMI_LARB1,
	CLK_GALS_COMM0,
	CLK_GALS_COMM1,
	/* module clk */
	CLK_DISP_OVL0,
	CLK_DISP_OVL0_2L,
	CLK_DISP_OVL1_2L,
	CLK_DISP_RDMA0,
	CLK_DISP_RDMA1,
	CLK_DISP_WDMA0,
	CLK_DISP_COLOR0,
	CLK_DISP_CCORR0,
	CLK_DISP_AAL0,
	CLK_DISP_GAMMA0,
	CLK_DISP_DITHER0,
	CLK_DSI0_MM_CK,
	CLK_DSI0_IF_CK,
	CLK_DPI_MM_CK,
	CLK_DPI_IF_CK,
	CLK_MM_26M,
	CLK_DISP_RSZ,
	CLK_MUX_MM,
	CLK_MUX_DISP_PWM,
	CLK_DISP_PWM,
	CLK_26M,
	CLK_DISP_POSTMASK,
	CLK_DISP_OVL_FBDC,
	/* DPI */
	CLK_UNIVPLL_D3_D2,
	CLK_UNIVPLL_D3_D4,
	CLK_OSC_D2,
	CLK_OSC_D4,
	CLK_OSC_D16,
	CLK_MUX_DPI0,
	CLK_TVDPLL_D2,
	CLK_TVDPLL_D4,
	CLK_TVDPLL_D8,
	CLK_TVDPLL_D16,
	CLK_TVDPLL_CK,
	CLK_MIPID0_26M,

	MAX_DISP_CLK_CNT
};

struct ddp_clk {
	struct clk *pclk;
	/* clk_name: it should be synchronized with DT */
	const char *clk_name;
	int refcnt;
	/* bit 0: main display , bit 1: second display */
	unsigned int belong_to;
	enum DISP_MODULE_ENUM module_id;
};

const char *ddp_get_clk_name(unsigned int n);
int ddp_clk_set_handle(struct clk *pclk, unsigned int n);

int ddp_clk_prepare_enable(enum DDP_CLK_ID id);
int ddp_clk_disable_unprepare(enum DDP_CLK_ID id);
int ddp_clk_set_parent(enum DDP_CLK_ID id, enum DDP_CLK_ID parent);
int ddp_clk_set_mipi26m(enum DISP_MODULE_ENUM module, int en);
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
void ddp_clk_top_clk_switch(bool on);
int ddp_clk_check(void);
int ddp_ovl_dcm_reset(void);
int ddp_clk_enable_by_module(enum DISP_MODULE_ENUM module);
int ddp_clk_disable_by_module(enum DISP_MODULE_ENUM module);
#endif /* __DDP_CLK_MGR_H__ */
