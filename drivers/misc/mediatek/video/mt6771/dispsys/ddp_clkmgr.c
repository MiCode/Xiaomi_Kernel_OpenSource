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
#define clk_setl(addr, val) mt_reg_sync_writel(clk_readl(addr) | (val), addr)
#define clk_clrl(addr, val) mt_reg_sync_writel(clk_readl(addr) & ~(val), addr)


/*
 * display clk table
 * -- by chip
 *	struct clk *pclk;
 *	const char *clk_name;
 *	int refcnt;
 *	unsigned int belong_to; 1: main display 2 externel display
 *	enum DISP_MODULE_ENUM module_id;
 */
static struct ddp_clk ddp_clks[MAX_DISP_CLK_CNT] = {
	{NULL, "MMSYS_MTCMOS", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_SMI_COMMON", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_SMI_LARB0", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_SMI_LARB1", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_GALS_COMM0", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_GALS_COMM1", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_DISP_OVL0", 0, (1), DISP_MODULE_OVL0},
	{NULL, "MMSYS_DISP_OVL0_2L", 0, (1), DISP_MODULE_OVL0_2L},
	{NULL, "MMSYS_DISP_OVL1_2L", 0, (1<<1|1<<2), DISP_MODULE_OVL1_2L},
	{NULL, "MMSYS_DISP_RDMA0", 0, (1), DISP_MODULE_RDMA0},
	{NULL, "MMSYS_DISP_RDMA1", 0, (1<<1), DISP_MODULE_RDMA1},
	{NULL, "MMSYS_DISP_WDMA0", 0, (1|1<<2), DISP_MODULE_WDMA0},
	{NULL, "MMSYS_DISP_COLOR0", 0, (1), DISP_MODULE_COLOR0},
	{NULL, "MMSYS_DISP_CCORR0", 0, (1), DISP_MODULE_CCORR0},
	{NULL, "MMSYS_DISP_AAL0", 0, (1), DISP_MODULE_AAL0},
	{NULL, "MMSYS_DISP_GAMMA0", 0, (1), DISP_MODULE_GAMMA0},
	{NULL, "MMSYS_DISP_DITHER0", 0, (1), DISP_MODULE_DITHER0},
	{NULL, "MMSYS_DSI0_MM_CK", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_DSI0_IF_CK", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_DPI_MM_CK", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_DPI_IF_CK", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_DBI_MM_CK", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_DBI_IF_CK", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_26M", 0, (1), DISP_MODULE_UNKNOWN},
	{NULL, "MMSYS_DISP_RSZ", 0, (1), DISP_MODULE_RSZ0},
	{NULL, "TOP_MUX_MM", 0, (1), DISP_MODULE_UNKNOWN},
	{NULL, "TOP_MUX_DISP_PWM", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "DISP_PWM", 0, (1), DISP_MODULE_PWM0},
	{NULL, "TOP_26M", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "TOP_UNIVPLL_D3_D2", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "TOP_UNIVPLL_D3_D4", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "TOP_OSC_D2", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "TOP_OSC_D4", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "TOP_OSC_D16", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "MUX_DPI0", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "TVDPLL_D2", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "TVDPLL_D4", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "TVDPLL_D8", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "TVDPLL_D16", 0, (0), DISP_MODULE_UNKNOWN},
	{NULL, "DPI_CK", 0, (0), DISP_MODULE_UNKNOWN},
};

static void __iomem *ddp_apmixed_base;
static unsigned int parsed_apmixed;
static int apmixed_refcnt;

const char *ddp_get_clk_name(unsigned int n)
{
	if (n >= MAX_DISP_CLK_CNT) {
		DDPERR("DISPSYS CLK id=%d is more than MAX_DISP_CLK_CNT\n", n);
		return NULL;
	}

	return ddp_clks[n].clk_name;
}

int ddp_set_clk_handle(struct clk *pclk, unsigned int n)
{
	int ret = 0;

	if (n >= MAX_DISP_CLK_CNT) {
		DDPERR("DISPSYS CLK id=%d is more than MAX_DISP_CLK_CNT\n", n);
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
		       __func__, ddp_clks[i].clk_name,
		       ddp_clks[i].refcnt == 0 ? "off" : "on",
		       ddp_clks[i].refcnt);
	}

	DDPDBG("%s: mipitx pll clk is %s refcnt=%d\n",
	       __func__, apmixed_refcnt == 0 ? "off" : "on", apmixed_refcnt);
	return ret;
}

int ddp_clk_prepare_enable(enum DDP_CLK_ID id)
{
	int ret = 0;

	DDPDBG("%s, clkid = %d\n", __func__, id);

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		return ret;

	if (id >= MAX_DISP_CLK_CNT) {
		DDPERR("DISPSYS CLK id=%d is more than MAX_DISP_CLK_CNT\n", id);
		return -1;
	}

	if (ddp_clks[id].pclk == NULL) {
		DDPERR("DISPSYS CLK %d NULL\n", id);
		return -1;
	}

	ret = clk_prepare_enable(ddp_clks[id].pclk);
	ddp_clks[id].refcnt++;
	if (ret)
		DDPERR("DISPSYS CLK prepare failed: errno %d\n", ret);

	return ret;
}

int ddp_clk_disable_unprepare(enum DDP_CLK_ID id)
{
	int ret = 0;

	DDPDBG("%s, clkid = %d\n", __func__, id);

	if (id == DISP0_SMI_LARB0)
		ddp_check_smi_status();

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		return ret;

	if (id >= MAX_DISP_CLK_CNT) {
		DDPERR("DISPSYS CLK id=%d is more than MAX_DISP_CLK_CNT\n", id);
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
		DDPERR("DISPSYS CLK id=%d is more than MAX_DISP_CLK_CNT\n", id);
		return -1;
	}

	if (parent >= MAX_DISP_CLK_CNT) {
		DDPERR("DISPSYS CLK parent=%d is more than MAX_DISP_CLK_CNT\n",
		       parent);
		return -1;
	}

	if ((ddp_clks[id].pclk == NULL) || (ddp_clks[parent].pclk == NULL)) {
		DDPERR("DISPSYS CLK %d or parent %d NULL\n", id, parent);
		return -1;
	}

	return clk_set_parent(ddp_clks[id].pclk, ddp_clks[parent].pclk);
}

static int __ddp_set_mipi26m(int idx, int en)
{
	if (en) {
		/* mipi_26m_en(idx, en) is provided by clock manager
		 * the first arg idx : 0 means display, 1 means camera
		 * the second arg en : 0 means disable, 1 means enable
		 */
		mipi_26m_en(0, 1);
		apmixed_refcnt++;
	} else {
		mipi_26m_en(0, 0);
		apmixed_refcnt--;
	}

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

	DDPMSG("%s en=%d, val=0x%x\n", __func__, en,
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

static unsigned int _is_ext_module(struct ddp_clk *pclk)
{
	return (pclk->belong_to & 0x2);
}

static unsigned int _is_ovl2mem_module(struct ddp_clk *pclk)
{
	return (pclk->belong_to & 0x4);
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
	enum DISP_MODULE_ENUM m;
	struct DDP_MODULE_DRIVER *m_drv;

	DISPFUNC();
	/* --TOP CLK-- */
	ddp_clk_prepare_enable(DISP_MTCMOS_CLK);
	ddp_clk_prepare_enable(DISP0_SMI_COMMON);
	ddp_clk_prepare_enable(DISP0_SMI_LARB0);
	ddp_clk_prepare_enable(DISP0_SMI_LARB1);
	ddp_clk_prepare_enable(CLK_MM_GALS_COMM0);
	ddp_clk_prepare_enable(CLK_MM_GALS_COMM1);
	ddp_clk_prepare_enable(DISP0_DISP_26M);

	/* --MODULE CLK-- */
	for (i = 0; i < MAX_DISP_CLK_CNT; i++) {
		if (!_is_main_module(&ddp_clks[i]))
			continue;

		m = ddp_clks[i].module_id;
		m_drv = ddp_get_module_driver(m);
		if (m != DISP_MODULE_UNKNOWN && m_drv) {
			/* module driver power on */
			if (m_drv->power_on && m_drv->power_off) {
				m_drv->power_on(m, NULL);
			} else {
				DDPERR("[%s] %s no power on(off) function\n",
				       __func__, ddp_get_module_name(m));
				ret = -1;
			}
		}
	}

	/* DISP_DSI */
	m = _get_dst_module_by_lcm(primary_get_lcm());
	if (m == DISP_MODULE_UNKNOWN)
		ret = -1;
	else if (ddp_get_module_driver(m))
		ddp_get_module_driver(m)->power_on(m, NULL);

	pr_info("CG0 0x%x, CG1 0x%x\n",
		clk_readl(DISP_REG_CONFIG_MMSYS_CG_CON0),
		clk_readl(DISP_REG_CONFIG_MMSYS_CG_CON1));
	return ret;
}

int ddp_ext_modules_clk_on(void)
{
	unsigned int i = 0;
	int ret = 0;
	enum DISP_MODULE_ENUM m;
	struct DDP_MODULE_DRIVER *m_drv;

	DISPFUNC();
	/* --TOP CLK-- */
	ddp_clk_prepare_enable(DISP_MTCMOS_CLK);
	ddp_clk_prepare_enable(DISP0_SMI_COMMON);
	ddp_clk_prepare_enable(DISP0_SMI_LARB0);
	ddp_clk_prepare_enable(DISP0_SMI_LARB1);
	ddp_clk_prepare_enable(CLK_MM_GALS_COMM0);
	ddp_clk_prepare_enable(CLK_MM_GALS_COMM1);
	ddp_clk_prepare_enable(DISP0_DISP_26M);

	/* --MODULE CLK-- */
	for (i = 0; i < MAX_DISP_CLK_CNT; i++) {
		if (!_is_ext_module(&ddp_clks[i]))
			continue;

		m = ddp_clks[i].module_id;
		m_drv = ddp_get_module_driver(m);
		if (m != DISP_MODULE_UNKNOWN && m_drv) {
			/* module driver power on */
			if (m_drv->power_on && m_drv->power_off) {
				m_drv->power_on(m, NULL);
			} else {
				DDPERR("[%s] %s no power on(off) function\n",
				       __func__, ddp_get_module_name(m));
				ret = -1;
			}
		}
	}

	pr_info("CG0 0x%x, CG1 0x%x\n",
		clk_readl(DISP_REG_CONFIG_MMSYS_CG_CON0),
		clk_readl(DISP_REG_CONFIG_MMSYS_CG_CON1));
	return ret;
}

int ddp_ovl2mem_modules_clk_on(void)
{
	unsigned int i = 0;
	int ret = 0;
	enum DISP_MODULE_ENUM m;
	struct DDP_MODULE_DRIVER *m_drv;

	DISPFUNC();
	/* --TOP CLK-- */
	ddp_clk_prepare_enable(DISP_MTCMOS_CLK);
	ddp_clk_prepare_enable(DISP0_SMI_COMMON);
	ddp_clk_prepare_enable(DISP0_SMI_LARB0);
	ddp_clk_prepare_enable(DISP0_SMI_LARB1);
	ddp_clk_prepare_enable(CLK_MM_GALS_COMM0);
	ddp_clk_prepare_enable(CLK_MM_GALS_COMM1);
	ddp_clk_prepare_enable(DISP0_DISP_26M);

	/* --MODULE CLK-- */
	for (i = 0; i < MAX_DISP_CLK_CNT; i++) {
		if (!_is_ovl2mem_module(&ddp_clks[i]))
			continue;

		m = ddp_clks[i].module_id;
		m_drv = ddp_get_module_driver(m);
		if (m != DISP_MODULE_UNKNOWN && m_drv) {
			/* module driver power on */
			if (m_drv->power_on && m_drv->power_off) {
				m_drv->power_on(m, NULL);
			} else {
				DDPERR("[%s] %s no power on(off) function\n",
				       __func__, ddp_get_module_name(m));
				ret = -1;
			}
		}
	}

	pr_info("CG0 0x%x, CG1 0x%x\n",
		clk_readl(DISP_REG_CONFIG_MMSYS_CG_CON0),
		clk_readl(DISP_REG_CONFIG_MMSYS_CG_CON1));
	return ret;
}

/**
 * ddp_main_modules_clk_on
 *
 * success: ret = 0
 * error: ret = -1
 */
int ddp_main_modules_clk_off(void)
{
	unsigned int i = 0;
	int ret = 0;
	enum DISP_MODULE_ENUM m;
	struct DDP_MODULE_DRIVER *m_drv;

	DISPFUNC();
	/* --MODULE CLK-- */
	for (i = 0; i < MAX_DISP_CLK_CNT; i++) {
		if (!_is_main_module(&ddp_clks[i]))
			continue;

		m = ddp_clks[i].module_id;
		if (m != DISP_MODULE_UNKNOWN && ddp_get_module_driver(m)) {
			/* module driver power off */
			m_drv = ddp_get_module_driver(m);
			if (m_drv->power_on && m_drv->power_off) {
				pr_info("%s power_off\n",
					ddp_get_module_name(m));
				m_drv->power_off(m, NULL);
			} else {
				DDPERR("[%s] %s no power on(off) function\n",
					__func__, ddp_get_module_name(m));
				ret = -1;
			}
		}
	}

	/* DISP_DSI */
	m = _get_dst_module_by_lcm(primary_get_lcm());
	if (m == DISP_MODULE_UNKNOWN)
		ret = -1;
	else if (ddp_get_module_driver(m))
		ddp_get_module_driver(m)->power_off(m, NULL);

	/* --TOP CLK-- */
	ddp_clk_disable_unprepare(DISP0_DISP_26M);
	ddp_clk_disable_unprepare(CLK_MM_GALS_COMM1);
	ddp_clk_disable_unprepare(CLK_MM_GALS_COMM0);

	ddp_clk_disable_unprepare(DISP0_SMI_LARB0);
	ddp_clk_disable_unprepare(DISP0_SMI_LARB1);
	ddp_clk_disable_unprepare(DISP0_SMI_COMMON);
	ddp_clk_disable_unprepare(DISP_MTCMOS_CLK);

	pr_info("CG0 0x%x, CG1 0x%x\n",
		clk_readl(DISP_REG_CONFIG_MMSYS_CG_CON0),
		clk_readl(DISP_REG_CONFIG_MMSYS_CG_CON1));
	return ret;
}

int ddp_ext_modules_clk_off(void)
{
	unsigned int i = 0;
	int ret = 0;
	enum DISP_MODULE_ENUM m;
	struct DDP_MODULE_DRIVER *m_drv;

	DISPFUNC();
	/* --MODULE CLK-- */
	for (i = 0; i < MAX_DISP_CLK_CNT; i++) {
		if (!_is_ext_module(&ddp_clks[i]))
			continue;

		m = ddp_clks[i].module_id;
		if (m != DISP_MODULE_UNKNOWN && ddp_get_module_driver(m)) {
			/* module driver power off */
			m_drv = ddp_get_module_driver(m);
			if (m_drv->power_on && m_drv->power_off) {
				pr_info("%s power_off\n",
					ddp_get_module_name(m));
				m_drv->power_off(m, NULL);
			} else {
				DDPERR("[%s] %s no power on(off) function\n",
				       __func__, ddp_get_module_name(m));
				ret = -1;
			}
		}
	}

	/* --TOP CLK-- */
	ddp_clk_disable_unprepare(DISP0_DISP_26M);
	ddp_clk_disable_unprepare(CLK_MM_GALS_COMM1);
	ddp_clk_disable_unprepare(CLK_MM_GALS_COMM0);
	ddp_clk_disable_unprepare(DISP0_SMI_LARB0);
	ddp_clk_disable_unprepare(DISP0_SMI_LARB1);
	ddp_clk_disable_unprepare(DISP0_SMI_COMMON);
	ddp_clk_disable_unprepare(DISP_MTCMOS_CLK);

	pr_info("CG0 0x%x, CG1 0x%x\n",
		clk_readl(DISP_REG_CONFIG_MMSYS_CG_CON0),
		clk_readl(DISP_REG_CONFIG_MMSYS_CG_CON1));
	return ret;
}

int ddp_ovl2mem_modules_clk_off(void)
{
	unsigned int i = 0;
	int ret = 0;
	enum DISP_MODULE_ENUM m;
	struct DDP_MODULE_DRIVER *m_drv;

	DISPFUNC();
	/* --MODULE CLK-- */
	for (i = 0; i < MAX_DISP_CLK_CNT; i++) {
		if (!_is_ovl2mem_module(&ddp_clks[i]))
			continue;

		m = ddp_clks[i].module_id;
		if (m != DISP_MODULE_UNKNOWN && ddp_get_module_driver(m)) {
			m_drv = ddp_get_module_driver(m);
			/* module driver power off */
			if (m_drv->power_on && m_drv->power_off) {
				pr_info("%s power_off\n",
					ddp_get_module_name(m));
				m_drv->power_off(m, NULL);
			} else {
				DDPERR("[%s] %s no power on(off) function\n",
				       __func__, ddp_get_module_name(m));
				ret = -1;
			}
		}
	}

	/* --TOP CLK-- */
	ddp_clk_disable_unprepare(DISP0_DISP_26M);
	ddp_clk_disable_unprepare(CLK_MM_GALS_COMM1);
	ddp_clk_disable_unprepare(CLK_MM_GALS_COMM0);
	ddp_clk_disable_unprepare(DISP0_SMI_LARB0);
	ddp_clk_disable_unprepare(DISP0_SMI_LARB1);
	ddp_clk_disable_unprepare(DISP0_SMI_COMMON);
	ddp_clk_disable_unprepare(DISP_MTCMOS_CLK);

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
		ddp_clk_prepare_enable(DISP_MTCMOS_CLK);
		/*ddp_clk_prepare_enable(TOP_26M);*/
		ddp_clk_prepare_enable(DISP0_SMI_COMMON);
		ddp_clk_prepare_enable(DISP0_SMI_LARB0);
		/*ddp_clk_prepare_enable(DISP0_SMI_LARB1);*/
		ddp_clk_prepare_enable(CLK_MM_GALS_COMM0);
		ddp_clk_prepare_enable(CLK_MM_GALS_COMM1);
		ddp_clk_prepare_enable(DISP0_DISP_26M);
	} else {
		ddp_clk_disable_unprepare(DISP0_DISP_26M);
		ddp_clk_disable_unprepare(CLK_MM_GALS_COMM1);
		ddp_clk_disable_unprepare(CLK_MM_GALS_COMM0);
		/*ddp_clk_disable_unprepare(DISP0_SMI_LARB1);*/
		ddp_clk_disable_unprepare(DISP0_SMI_LARB0);
		ddp_clk_disable_unprepare(DISP0_SMI_COMMON);
		/*ddp_clk_disable_unprepare(TOP_26M);*/
		ddp_clk_disable_unprepare(DISP_MTCMOS_CLK);
	}
}

int ddp_ovl_dcm_reset(void)
{
	unsigned int reg = 0;

	DDPDBG("%s\n", __func__);
	/* hw workaround : begin */
	/* if the mmsys_clk restart, we should do it for all the ovl modules*/
	/*  gce event 32/33/34 workaround */

	reg = DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0);

	if ((reg & (1 << 20)) != 0) {
		/*if the ovl cg is already on, skip it.*/
		ddp_clk_prepare_enable(DISP0_DISP_OVL0);
		/*ovl0 dcm*/
		DISP_REG_SET(NULL,
			DISP_REG_CONFIG_MMSYS_HW_DCM_DIS_SET0, 1<<14);
		DISP_REG_SET(NULL,
			DISP_REG_CONFIG_MMSYS_HW_DCM_DIS_CLR0, 1<<14);
		ddp_clk_disable_unprepare(DISP0_DISP_OVL0);
	}

	if ((reg & (1 << 21)) != 0) {
		ddp_clk_prepare_enable(DISP0_DISP_OVL0_2L);
		/*ovl0_2l dcm*/
		DISP_REG_SET(NULL,
			DISP_REG_CONFIG_MMSYS_HW_DCM_DIS_SET0, 1<<15);
		DISP_REG_SET(NULL,
			DISP_REG_CONFIG_MMSYS_HW_DCM_DIS_CLR0, 1<<15);
		ddp_clk_disable_unprepare(DISP0_DISP_OVL0_2L);
	}

	if ((reg & (1 << 22)) != 0) {
		ddp_clk_prepare_enable(DISP0_DISP_OVL1_2L);
		/*ovl1_2l dcm*/
		DISP_REG_SET(NULL,
			DISP_REG_CONFIG_MMSYS_HW_DCM_DIS_SET0, 1<<16);
		DISP_REG_SET(NULL,
			DISP_REG_CONFIG_MMSYS_HW_DCM_DIS_CLR0, 1<<16);
		ddp_clk_disable_unprepare(DISP0_DISP_OVL1_2L);
	}

	/* hw workaround : end */

	return 0;
}

int ddp_clk_enable_by_module(enum DISP_MODULE_ENUM module)
{
	int ret = 0;

	switch (module) {
	case DISP_MODULE_OVL0:
		ddp_clk_prepare_enable(DISP0_DISP_OVL0);
		break;
	case DISP_MODULE_OVL0_2L:
		ddp_clk_prepare_enable(DISP0_DISP_OVL0_2L);
		break;
	case DISP_MODULE_OVL1_2L:
		ddp_clk_prepare_enable(DISP0_DISP_OVL1_2L);
		break;
	case DISP_MODULE_RDMA0:
		ddp_clk_prepare_enable(DISP0_DISP_RDMA0);
		break;
	case DISP_MODULE_RDMA1:
		ddp_clk_prepare_enable(DISP0_DISP_RDMA1);
		break;
	case DISP_MODULE_WDMA0:
		ddp_clk_prepare_enable(DISP0_DISP_WDMA0);
		break;
	case DISP_MODULE_COLOR0:
		ddp_clk_prepare_enable(DISP0_DISP_COLOR0);
		break;
	case DISP_MODULE_CCORR0:
		ddp_clk_prepare_enable(DISP0_DISP_CCORR0);
		break;
	case DISP_MODULE_AAL0:
		ddp_clk_prepare_enable(DISP0_DISP_AAL0);
		break;
	case DISP_MODULE_GAMMA0:
		ddp_clk_prepare_enable(DISP0_DISP_GAMMA0);
		break;
	case DISP_MODULE_DITHER0:
		ddp_clk_prepare_enable(DISP0_DISP_DITHER0);
		break;
	case DISP_MODULE_RSZ0:
		ddp_clk_prepare_enable(DISP0_DISP_RSZ);
		break;
	default:
		DDPPR_ERR("invalid module id=%d\n", module);
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
		ddp_clk_disable_unprepare(DISP0_DISP_OVL0);
		break;
	case DISP_MODULE_OVL0_2L:
		ddp_clk_disable_unprepare(DISP0_DISP_OVL0_2L);
		break;
	case DISP_MODULE_OVL1_2L:
		ddp_clk_disable_unprepare(DISP0_DISP_OVL1_2L);
		break;
	case DISP_MODULE_RDMA0:
		ddp_clk_disable_unprepare(DISP0_DISP_RDMA0);
		break;
	case DISP_MODULE_RDMA1:
		ddp_clk_disable_unprepare(DISP0_DISP_RDMA1);
		break;
	case DISP_MODULE_WDMA0:
		ddp_clk_disable_unprepare(DISP0_DISP_WDMA0);
		break;
	case DISP_MODULE_COLOR0:
		ddp_clk_disable_unprepare(DISP0_DISP_COLOR0);
		break;
	case DISP_MODULE_CCORR0:
		ddp_clk_disable_unprepare(DISP0_DISP_CCORR0);
		break;
	case DISP_MODULE_AAL0:
		ddp_clk_disable_unprepare(DISP0_DISP_AAL0);
		break;
	case DISP_MODULE_GAMMA0:
		ddp_clk_disable_unprepare(DISP0_DISP_GAMMA0);
		break;
	case DISP_MODULE_DITHER0:
		ddp_clk_disable_unprepare(DISP0_DISP_DITHER0);
		break;
	case DISP_MODULE_RSZ0:
		ddp_clk_disable_unprepare(DISP0_DISP_RSZ);
		break;
	default:
		DDPPR_ERR("invalid module id=%d\n", module);
		ret = -1;
		break;
	}
	return ret;
}
