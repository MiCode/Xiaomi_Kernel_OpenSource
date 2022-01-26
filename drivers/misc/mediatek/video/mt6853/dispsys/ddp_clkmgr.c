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

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/types.h>
#include "mt-plat/sync_write.h"

#include "ddp_reg.h"
#include "ddp_info.h"
#include "ddp_log.h"
#include "primary_display.h"
#include "ddp_clkmgr.h"

#define DRV_Reg32(addr) INREG32(addr)
#define clk_readl(addr) DRV_Reg32(addr)
#define clk_writel(addr, val) mt_reg_sync_writel(val, addr)
#define clk_setl(addr, val) \
	mt_reg_sync_writel(clk_readl(addr) | (val), addr)
#define clk_clrl(addr, val) \
	mt_reg_sync_writel(clk_readl(addr) & ~(val), addr)


/*
 * display clk table
 * -- by chip
 *	struct clk *pclk;
 *	const char *clk_name;
 *	int refcnt;
 *	unsigned int belong_to; 1: main display 2: externel display
 *						3: virtual display
 *	enum DISP_MODULE_ENUM module_id;
 */

static struct ddp_clk ddp_clks[MAX_DISP_CLK_CNT] = {
	{NULL, "MMSYS_MTCMOS", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_SMI_COMMON", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_SMI_GALS", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_SMI_INFRA", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_SMI_IOMMU", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_DISP_OVL0", 0, (1), DISP_MODULE_OVL0},
	{NULL, "MMSYS_DISP_OVL0_2L", 0, (1|1<<2), DISP_MODULE_OVL0_2L},
	{NULL, "MMSYS_DISP_RDMA0", 0, (1), DISP_MODULE_RDMA0},
	{NULL, "MMSYS_DISP_WDMA0", 0, (1|1<<2), DISP_MODULE_WDMA0},
	{NULL, "MMSYS_DISP_COLOR0", 0, (1), DISP_MODULE_COLOR0},
	{NULL, "MMSYS_DISP_CCORR0", 0, (1), DISP_MODULE_CCORR0},
	{NULL, "MMSYS_DISP_AAL0", 0, (1), DISP_MODULE_AAL0},
	{NULL, "MMSYS_DISP_GAMMA0", 0, (1), DISP_MODULE_GAMMA0},
	{NULL, "MMSYS_DISP_POSTMASK0", 0, (1), DISP_MODULE_POSTMASK0},
	{NULL, "MMSYS_DISP_DITHER0", 0, (1), DISP_MODULE_DITHER0},
	{NULL, "MMSYS_DSI0_MM_CK", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_DSI0_IF_CK", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_26M", 0, (1), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_DISP_MUTEX0", 0, (1), DISP_MODULE_MUTEX},
	{NULL, "MMSYS_DISP_CONFIG", 0, (1), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_DISP_RSZ0", 0, (1), DISP_MODULE_RSZ0},
	{NULL, "APMIXED_MIPI_26M", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "TOP_MUX_DISP_PWM", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "DISP_PWM", 0, (1), DISP_MODULE_PWM0},
	{NULL, "TOP_26M", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "TOP_UNIVPLL2_D4", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "TOP_ULPOSC1_D2", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "TOP_ULPOSC1_D8", 0, (0), DISP_MODULE_UNKNOWN},
};

static void __iomem *ddp_apmixed_base;
static unsigned int parsed_apmixed;
static int apmixed_refcnt;

const char *ddp_get_clk_name(unsigned int n)
{
	if (n >= MAX_DISP_CLK_CNT) {
		DDPERR("DISPSYS CLK id=%d is more than MAX_DISP_CLK_CNT\n",
			n);
		return NULL;
	}

	return ddp_clks[n].clk_name;
}

int ddp_set_clk_handle(struct clk *pclk, unsigned int n)
{
	int ret = 0;

	if (n >= MAX_DISP_CLK_CNT) {
		DDPERR("DISPSYS CLK id=%d is more than MAX_DISP_CLK_CNT\n",
			n);
		return -1;
	}

	ddp_clks[n].pclk = pclk;
	DDPMSG("ddp_clk[%d] %p\n", n, ddp_clks[n].pclk);
	return ret;
}

int ddp_clk_check(void)
{
	int i;
	int ret = 0;

	for (i = 0; i < MAX_DISP_CLK_CNT; i++) {
		if (ddp_clks[i].refcnt != 0)
			ret++;

		DDPDBG("%s: %s is %s refcnt=%d\n",
			__func__,
			ddp_clks[i].clk_name,
			ddp_clks[i].refcnt == 0 ? "off" : "on",
			ddp_clks[i].refcnt);
	}

	DDPDBG("%s: mipitx pll clk is %s refcnt=%d\n", __func__,
		apmixed_refcnt == 0 ? "off" : "on", apmixed_refcnt);
	return ret;
}

int ddp_clk_prepare_enable(enum DDP_CLK_ID id)
{
	int ret = 0;

	DDPDBG("%s: clkid = %d\n", __func__, id);

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		return ret;

	if (id >= MAX_DISP_CLK_CNT) {
		DDPERR("DISPSYS CLK id=%d is more than MAX_DISP_CLK_CNT\n",
			id);
		return -1;
	}

	if (ddp_clks[id].pclk == NULL) {
		DDPERR("DISPSYS CLK %d NULL\n", id);
		return -1;
	}

	ret = clk_prepare_enable(ddp_clks[id].pclk);
	ddp_clks[id].refcnt++;
	if (ret)
		DDPERR("DISPSYS CLK prepare failed: errno %d\n",
			ret);

	return ret;
}

int ddp_clk_disable_unprepare(enum DDP_CLK_ID id)
{
	int ret = 0;

	DDPDBG("%s, clkid = %d\n", __func__,
		id);

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		return ret;

	if (id >= MAX_DISP_CLK_CNT) {
		DDPERR("DISPSYS CLK id=%d is more than MAX_DISP_CLK_CNT\n",
			id);
		return -1;
	}

	if (ddp_clks[id].pclk == NULL) {
		DDPERR("DISPSYS CLK %d NULL\n", id);
		return -1;
	}
	clk_disable_unprepare(ddp_clks[id].pclk);
	ddp_clks[id].refcnt--;

	return ret;
}

int ddp_clk_set_parent(enum DDP_CLK_ID id, enum DDP_CLK_ID parent)
{
	if (id >= MAX_DISP_CLK_CNT) {
		DDPERR("DISPSYS CLK id=%d is more than MAX_DISP_CLK_CNT\n",
			id);
		return -1;
	}

	if (parent >= MAX_DISP_CLK_CNT) {
		DDPERR("DISPSYS CLK parent=%d is more than MAX_DISP_CLK_CNT\n",
			parent);
		return -1;
	}

	if ((ddp_clks[id].pclk == NULL) ||
		(ddp_clks[parent].pclk == NULL)) {
		DDPERR("DISPSYS CLK %d or parent %d NULL\n", id, parent);
		return -1;
	}

	return clk_set_parent(ddp_clks[id].pclk, ddp_clks[parent].pclk);
}

static int __ddp_set_mipi26m(int idx, int en)
{
#if 0
	if (en) {
		DISP_REG_SET_FIELD(NULL, FLD_PLL_MIPI_DSI0_26M_CK_EN,
			APMIXEDSYS_PLL_BASE + APMIXED_PLL_CON0, 1);
		apmixed_refcnt++;
	} else {
		DISP_REG_SET_FIELD(NULL, FLD_PLL_MIPI_DSI0_26M_CK_EN,
			APMIXEDSYS_PLL_BASE + APMIXED_PLL_CON0, 0);
		apmixed_refcnt--;
	}
#else
	if (en) {
		ddp_clk_prepare_enable(MIPI_26M);
		apmixed_refcnt++;
	} else {
		ddp_clk_disable_unprepare(MIPI_26M);
		apmixed_refcnt--;
	}
#endif
	return 0;
}

int ddp_set_mipi26m(enum DISP_MODULE_ENUM module, int en)
{
	int ret = 0;

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		return ret;

	ret = ddp_parse_apmixed_base();
	if (ret)
		return -1;

	if (module == DISP_MODULE_DSI0 || module == DISP_MODULE_DSIDUAL)
		__ddp_set_mipi26m(0, en);
	if (module == DISP_MODULE_DSI1 || module == DISP_MODULE_DSIDUAL)
		__ddp_set_mipi26m(1, en);

	DDPMSG("%s en=%d, val=0x%x\n",
		__func__, en,
		clk_readl(APMIXEDSYS_PLL_BASE + APMIXED_PLL_CON0));

	return ret;
}

int ddp_parse_apmixed_base(void)
{
	int ret = 0;
	struct device_node *node;

	if (parsed_apmixed)
		return ret;

	node = of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
	if (!node) {
		DDPERR("[DDP_APMIXED] DISP find apmixed node failed\n");
		return -1;
	}

	ddp_apmixed_base = of_iomap(node, 0);
	if (!ddp_apmixed_base) {
		DDPERR("[DDP_APMIXED] DISP apmixed base failed\n");
		return -1;
	}

	parsed_apmixed = 1;
	return ret;
}

static unsigned int _is_main_module(struct ddp_clk *pclk)
{
	return (pclk->belong_to & 0x1);
}


/*
 * ddp_main_modules_clk_on
 *
 * success: ret = 0
 * error: ret = -1
 */
int ddp_main_modules_clk_on(void)
{
	unsigned int i = 0;
	int ret = 0;
	enum DISP_MODULE_ENUM module;
	struct DDP_MODULE_DRIVER *module_driver;

	DISPFUNC();
	/* --TOP CLK-- */
	ddp_clk_prepare_enable(CLK_MM_MTCMOS);
	ddp_clk_prepare_enable(CLK_SMI_COMMON);
	ddp_clk_prepare_enable(CLK_SMI_GALS);
	ddp_clk_prepare_enable(CLK_SMI_INFRA);
	ddp_clk_prepare_enable(CLK_SMI_IOMMU);
	ddp_clk_prepare_enable(CLK_MM_26M);
	ddp_clk_prepare_enable(CLK_DISP_CONFIG);
	ddp_clk_prepare_enable(CLK_DISP_MUTEX0);
	/* --MODULE CLK-- */
	for (i = 0; i < MAX_DISP_CLK_CNT; i++) {
		if (!_is_main_module(&ddp_clks[i]))
			continue;

		module = ddp_clks[i].module_id;
		module_driver = ddp_get_module_driver(module);
		if (module != DISP_MODULE_UNKNOWN &&
			module_driver != 0) {
			/* module driver power on */
			if (module_driver->power_on != 0 &&
				module_driver->power_off != 0) {
				module_driver->power_on(module, NULL);
			} else {
				DDPERR("[%s] %s no power on(off) function\n",
					__func__, ddp_get_module_name(module));
				ret = -1;
			}
		}
	}

	/* DISP_DSI */
	module = _get_dst_module_by_lcm(primary_get_lcm());
	if (module == DISP_MODULE_UNKNOWN)
		ret = -1;
	else
		ddp_get_module_driver(module)->power_on(module, NULL);

	pr_info("CG0 0x%x, CG1 0x%x\n",
		clk_readl(DISP_REG_CONFIG_MMSYS_CG_CON0),
		clk_readl(DISP_REG_CONFIG_MMSYS_CG_CON1));
	return ret;
}

/* ddp_main_modules_clk_on
 *
 * success: ret = 0
 * error: ret = -1
 */
int ddp_main_modules_clk_off(void)
{
	unsigned int i = 0;
	int ret = 0;
	enum DISP_MODULE_ENUM module;
	struct DDP_MODULE_DRIVER *module_driver;

	DISPFUNC();
	/* --MODULE CLK-- */
	for (i = 0; i < MAX_DISP_CLK_CNT; i++) {
		if (!_is_main_module(&ddp_clks[i]))
			continue;

		module = ddp_clks[i].module_id;
		if (module != DISP_MODULE_UNKNOWN
			&& ddp_get_module_driver(module) != 0) {
			/* module driver power off */
			module_driver = ddp_get_module_driver(module);
			if (module_driver->power_on != 0 &&
				module_driver->power_off != 0) {
				pr_info("%s power_off\n",
					ddp_get_module_name(module));
				module_driver->power_off(module, NULL);
			} else {
				DDPERR("[%s] %s no power on(off) function\n",
					__func__, ddp_get_module_name(module));
				ret = -1;
			}
		}
	}

	/* DISP_DSI */
	module = _get_dst_module_by_lcm(primary_get_lcm());
	if (module == DISP_MODULE_UNKNOWN)
		ret = -1;
	else
		ddp_get_module_driver(module)->power_off(module, NULL);


	/* --TOP CLK-- */
	ddp_clk_disable_unprepare(CLK_DISP_MUTEX0);
	ddp_clk_disable_unprepare(CLK_DISP_CONFIG);
	ddp_clk_disable_unprepare(CLK_MM_26M);
	ddp_clk_disable_unprepare(CLK_SMI_IOMMU);
	ddp_clk_disable_unprepare(CLK_SMI_INFRA);
	ddp_clk_disable_unprepare(CLK_SMI_GALS);
	ddp_clk_disable_unprepare(CLK_SMI_COMMON);
	ddp_clk_disable_unprepare(CLK_MM_MTCMOS);

	pr_info("CG0 0x%x, CG1 0x%x\n",
		clk_readl(DISP_REG_CONFIG_MMSYS_CG_CON0),
		clk_readl(DISP_REG_CONFIG_MMSYS_CG_CON1));
	return ret;
}

int ddp_module_clk_enable(enum DISP_MODULE_TYPE_ENUM module_t)
{
	int ret = 0;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int number = 0;
	enum DISP_MODULE_ENUM module_id = DISP_MODULE_UNKNOWN;

	number = ddp_get_module_num_by_t(module_t);
	pr_info("[%s] module type = %d, module num on this type = %d\n",
		__func__, module_t, number);
	for (i = 0; i < number; i++) {
		module_id = ddp_get_module_id_by_idx(module_t, i);
		for (j = 0; j < MAX_DISP_CLK_CNT; j++) {
			if (ddp_clks[j].module_id == module_id)
				ddp_clk_prepare_enable(j);
		}
	}

	return ret;
}

int ddp_module_clk_disable(enum DISP_MODULE_TYPE_ENUM module_t)
{
	int ret = 0;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int number = 0;
	enum DISP_MODULE_ENUM module_id = DISP_MODULE_UNKNOWN;

	number = ddp_get_module_num_by_t(module_t);
	pr_info("[%s] module type = %d, module num on this type = %d\n",
		__func__, module_t, number);
	for (i = 0; i < number; i++) {
		module_id = ddp_get_module_id_by_idx(module_t, i);
		for (j = 0; j < MAX_DISP_CLK_CNT; j++) {
			if (ddp_clks[j].module_id == module_id)
				ddp_clk_disable_unprepare(j);
		}
	}

	return ret;
}

enum DDP_CLK_ID ddp_get_module_clk_id(enum DISP_MODULE_ENUM module_id)
{
	unsigned int i = 0;

	for (i = 0; i < MAX_DISP_CLK_CNT; i++) {
		if (ddp_clks[i].module_id == module_id)
			return i;
	}

	return MAX_DISP_CLK_CNT;
}

void ddp_clk_force_on(unsigned int on)
{
	if (on) {
		ddp_clk_prepare_enable(CLK_MM_MTCMOS);
		ddp_clk_prepare_enable(CLK_SMI_COMMON);
		ddp_clk_prepare_enable(CLK_SMI_GALS);
		ddp_clk_prepare_enable(CLK_SMI_INFRA);
		ddp_clk_prepare_enable(CLK_SMI_IOMMU);

		ddp_clk_prepare_enable(CLK_MM_26M);
		ddp_clk_prepare_enable(CLK_DISP_CONFIG);
		ddp_clk_prepare_enable(CLK_DISP_MUTEX0);
	} else {
		ddp_clk_prepare_enable(CLK_DISP_MUTEX0);
		ddp_clk_prepare_enable(CLK_DISP_CONFIG);
		ddp_clk_disable_unprepare(CLK_MM_26M);

		ddp_clk_disable_unprepare(CLK_SMI_IOMMU);
		ddp_clk_disable_unprepare(CLK_SMI_INFRA);
		ddp_clk_disable_unprepare(CLK_SMI_GALS);
		ddp_clk_disable_unprepare(CLK_SMI_COMMON);
		ddp_clk_disable_unprepare(CLK_MM_MTCMOS);
	}
}

int ddp_clk_enable_by_module(enum DISP_MODULE_ENUM module)
{
	int ret = 0;

	switch (module) {
	case DISP_MODULE_OVL0:
		ddp_clk_prepare_enable(CLK_DISP_OVL0);
		break;
	case DISP_MODULE_OVL0_2L:
		ddp_clk_prepare_enable(CLK_DISP_OVL0_2L);
		break;
	case DISP_MODULE_RDMA0:
		ddp_clk_prepare_enable(CLK_DISP_RDMA0);
		break;
	case DISP_MODULE_WDMA0:
		ddp_clk_prepare_enable(CLK_DISP_WDMA0);
		break;
	case DISP_MODULE_COLOR0:
		ddp_clk_prepare_enable(CLK_DISP_COLOR0);
		break;
	case DISP_MODULE_CCORR0:
		ddp_clk_prepare_enable(CLK_DISP_CCORR0);
		break;
	case DISP_MODULE_AAL0:
		ddp_clk_prepare_enable(CLK_DISP_AAL0);
		break;
	case DISP_MODULE_GAMMA0:
		ddp_clk_prepare_enable(CLK_DISP_GAMMA0);
		break;
	case DISP_MODULE_DITHER0:
		ddp_clk_prepare_enable(CLK_DISP_DITHER0);
		break;
	case DISP_MODULE_RSZ0:
		ddp_clk_prepare_enable(CLK_DISP_RSZ0);
		break;
	case DISP_MODULE_POSTMASK0:
		ddp_clk_prepare_enable(CLK_DISP_POSTMASK0);
		break;
	case DISP_MODULE_MUTEX:
		ddp_clk_prepare_enable(CLK_DISP_MUTEX0);
		break;
	case DISP_MODULE_DSI0:
		ddp_clk_prepare_enable(CLK_DSI0_MM_CLK);
		ddp_clk_prepare_enable(CLK_DSI0_IF_CLK);
		break;
	default:
		DDPERR("invalid module id=%d\n", module);
		ret = -1;
		break;
	}
	return ret;
}

int ddp_clk_disable_by_module(enum DISP_MODULE_ENUM module)
{
	int ret = 0;

	switch (module) {
	case DISP_MODULE_OVL0:
		ddp_clk_disable_unprepare(CLK_DISP_OVL0);
		break;
	case DISP_MODULE_OVL0_2L:
		ddp_clk_disable_unprepare(CLK_DISP_OVL0_2L);
		break;
	case DISP_MODULE_RDMA0:
		ddp_clk_disable_unprepare(CLK_DISP_RDMA0);
		break;
	case DISP_MODULE_WDMA0:
		ddp_clk_disable_unprepare(CLK_DISP_WDMA0);
		break;
	case DISP_MODULE_COLOR0:
		ddp_clk_disable_unprepare(CLK_DISP_COLOR0);
		break;
	case DISP_MODULE_CCORR0:
		ddp_clk_disable_unprepare(CLK_DISP_CCORR0);
		break;
	case DISP_MODULE_AAL0:
		ddp_clk_disable_unprepare(CLK_DISP_AAL0);
		break;
	case DISP_MODULE_GAMMA0:
		ddp_clk_disable_unprepare(CLK_DISP_GAMMA0);
		break;
	case DISP_MODULE_DITHER0:
		ddp_clk_disable_unprepare(CLK_DISP_DITHER0);
		break;
	case DISP_MODULE_RSZ0:
		ddp_clk_disable_unprepare(CLK_DISP_RSZ0);
		break;
	case DISP_MODULE_POSTMASK0:
		ddp_clk_disable_unprepare(CLK_DISP_POSTMASK0);
		break;
	case DISP_MODULE_MUTEX:
		ddp_clk_disable_unprepare(CLK_DISP_MUTEX0);
		break;
	case DISP_MODULE_DSI0:
		ddp_clk_disable_unprepare(CLK_DSI0_IF_CLK);
		ddp_clk_disable_unprepare(CLK_DSI0_MM_CLK);
		break;
	default:
		DDPERR("invalid module id=%d\n", module);
		ret = -1;
		break;
	}
	return ret;
}
