// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/types.h>
#include <linux/io.h>
/* #include "mt-plat/sync_write.h" */

#include "ddp_reg.h"
#include "ddp_info.h"
#include "ddp_log.h"
#include "primary_display.h"
#include "ddp_clkmgr.h"

#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

#define DRV_Reg32(addr) INREG32(addr)
#define clk_readl(addr) DRV_Reg32(addr)
#define clk_writel(addr, val) writel(val, addr)
#define clk_setl(addr, val)  writel(clk_readl(addr) | (val), addr)
#define clk_clrl(addr, val)  writel(clk_readl(addr) & ~(val), addr)

/**
 * display clk table
 * -- by chip
 *	struct clk *pclk;
 *	const char *clk_name;
 *	int refcnt;
 *	unsigned int belong_to: bit 0: main display, bit 1: externel display
 *	enum DISP_MODULE_ENUM module_id;
 */
static struct ddp_clk ddp_clks[MAX_DISP_CLK_CNT] = {
	/* top clk */
	[CLK_MM_MTCMOS] = {
		NULL, "CLK_MM_MTCMOS", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_SMI_COMMON] = {
		NULL, "CLK_SMI_COMMON", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_SMI_LARB0] = {
		NULL, "CLK_SMI_LARB0", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_SMI_LARB1] = {
		NULL, "CLK_SMI_LARB1", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_GALS_COMM0] = {
		NULL, "CLK_GALS_COMM0", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_GALS_COMM1] = {
		NULL, "CLK_GALS_COMM1", 0, (0), DISP_MODULE_UNKNOWN},
	/* module clk */
	[CLK_DISP_OVL0] = {
		NULL, "CLK_DISP_OVL0", 0, (1), DISP_MODULE_OVL0},
	[CLK_DISP_OVL0_2L] = {
		NULL, "CLK_DISP_OVL0_2L", 0, (1), DISP_MODULE_OVL0_2L},
	[CLK_DISP_OVL1_2L] = {
		NULL, "CLK_DISP_OVL1_2L", 0, (1<<1|1<<2), DISP_MODULE_OVL1_2L},
	[CLK_DISP_RDMA0] = {
		NULL, "CLK_DISP_RDMA0", 0, (1), DISP_MODULE_RDMA0},
	[CLK_DISP_RDMA1] = {
		NULL, "CLK_DISP_RDMA1", 0, (1<<1), DISP_MODULE_RDMA1},
	[CLK_DISP_WDMA0] = {
		NULL, "CLK_DISP_WDMA0", 0, (1|1<<2), DISP_MODULE_WDMA0},
	[CLK_DISP_COLOR0] = {
		NULL, "CLK_DISP_COLOR0", 0, (1), DISP_MODULE_COLOR0},
	[CLK_DISP_CCORR0] = {
		NULL, "CLK_DISP_CCORR0", 0, (1), DISP_MODULE_CCORR0},
	[CLK_DISP_AAL0] = {
		NULL, "CLK_DISP_AAL0", 0, (1), DISP_MODULE_AAL0},
	[CLK_DISP_GAMMA0] = {
		NULL, "CLK_DISP_GAMMA0", 0, (1), DISP_MODULE_GAMMA0},
	[CLK_DISP_DITHER0] = {
		NULL, "CLK_DISP_DITHER0", 0, (1), DISP_MODULE_DITHER0},
	[CLK_DSI0_MM_CK] = {
		NULL, "CLK_DSI0_MM_CK", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_DSI0_IF_CK] = {
		NULL, "CLK_DSI0_IF_CK", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_DPI_MM_CK] = {
		NULL, "CLK_DPI_MM_CK", 0, (1<<1), DISP_MODULE_UNKNOWN},
	[CLK_DPI_IF_CK] = {
		NULL, "CLK_DPI_IF_CK", 0, (1<<1), DISP_MODULE_UNKNOWN},
	[CLK_MM_26M] = {
		NULL, "CLK_MM_26M", 0, (1), DISP_MODULE_UNKNOWN},
	[CLK_DISP_RSZ] = {
		NULL, "CLK_DISP_RSZ", 0, (1), DISP_MODULE_RSZ0},
	[CLK_MUX_MM] = {
		NULL, "CLK_MUX_MM", 0, (1), DISP_MODULE_UNKNOWN},
	[CLK_MUX_DISP_PWM] = {
		NULL, "CLK_MUX_DISP_PWM", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_DISP_PWM] = {
		NULL, "CLK_DISP_PWM", 0, (1), DISP_MODULE_PWM0},
	[CLK_26M] = {
		NULL, "CLK_26M", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_DISP_POSTMASK] = {
		NULL, "CLK_DISP_POSTMASK", 0, (1), DISP_MODULE_POSTMASK},
	[CLK_DISP_OVL_FBDC] = {
		NULL, "CLK_DISP_OVL_FBDC", 0, (1), DISP_MODULE_UNKNOWN},
	/* DPI */
	[CLK_UNIVPLL_D3_D2] = {
		NULL, "CLK_UNIVPLL_D3_D2", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_UNIVPLL_D3_D4] = {
		NULL, "CLK_UNIVPLL_D3_D4", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_OSC_D2] = {
		NULL, "CLK_OSC_D2", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_OSC_D4] = {
		NULL, "CLK_OSC_D4", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_OSC_D16] = {
		NULL, "CLK_OSC_D16", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_MUX_DPI0] = {
		NULL, "CLK_MUX_DPI0", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_TVDPLL_D2] = {
		NULL, "CLK_TVDPLL_D2", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_TVDPLL_D4] = {
		NULL, "CLK_TVDPLL_D4", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_TVDPLL_D8] = {
		NULL, "CLK_TVDPLL_D8", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_TVDPLL_D16] = {
		NULL, "CLK_TVDPLL_D16", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_TVDPLL_CK] = {
		NULL, "CLK_TVDPLL_CK", 0, (0), DISP_MODULE_UNKNOWN},
	[CLK_MIPID0_26M] = {
		NULL, "CLK_MIPID0_26M", 0, (0), DISP_MODULE_UNKNOWN},
};

static void __iomem *ddp_apmixed_base;
static unsigned int parsed_apmixed;
static int apmixed_refcnt;

const char *ddp_get_clk_name(unsigned int n)
{
	if (n >= MAX_DISP_CLK_CNT) {
		DDP_PR_ERR("DISPSYS CLK id=%d is more than MAX_DISP_CLK_CNT\n",
			   n);
		return NULL;
	}

	return ddp_clks[n].clk_name;
}

int ddp_clk_set_handle(struct clk *pclk, unsigned int n)
{
	int ret = 0;

	if (n >= MAX_DISP_CLK_CNT) {
		DDP_PR_ERR("DISPSYS CLK id=%d is more than MAX_DISP_CLK_CNT\n",
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
		DDP_PR_ERR("DISPSYS CLK id=%d is more than MAX_DISP_CLK_CNT\n",
			   id);
		return -1;
	}

	if (ddp_clks[id].pclk == NULL) {
		DDP_PR_ERR("DISPSYS CLK %d NULL\n", id);
		return -1;
	}

	ret = clk_prepare_enable(ddp_clks[id].pclk);
	ddp_clks[id].refcnt++;
	if (ret)
		DDP_PR_ERR("DISPSYS CLK prepare failed: errno %d\n", ret);

	return ret;
}

int ddp_clk_disable_unprepare(enum DDP_CLK_ID id)
{
	int ret = 0;

	DDPDBG("%s, clkid = %d\n", __func__, id);

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		return ret;

	if (id >= MAX_DISP_CLK_CNT) {
		DDP_PR_ERR("DISPSYS CLK id=%d is more than MAX_DISP_CLK_CNT\n",
			   id);
		return -1;
	}

	if (ddp_clks[id].pclk == NULL) {
		DDP_PR_ERR("DISPSYS CLK %d NULL\n", id);
		return -1;
	}
	clk_disable_unprepare(ddp_clks[id].pclk);
	ddp_clks[id].refcnt--;

	return ret;
}

int ddp_clk_set_parent(enum DDP_CLK_ID id, enum DDP_CLK_ID parent)
{
	if (id >= MAX_DISP_CLK_CNT) {
		DDP_PR_ERR("DISPSYS CLK id=%d >= MAX_DISP_CLK_CNT\n",
			   id);
		return -1;
	}

	if (parent >= MAX_DISP_CLK_CNT) {
		DDP_PR_ERR("DISPSYS CLK parent=%d >= MAX_DISP_CLK_CNT\n",
		       parent);
		return -1;
	}

	if ((ddp_clks[id].pclk == NULL) || (ddp_clks[parent].pclk == NULL)) {
		DDP_PR_ERR("DISPSYS CLK %d or parent %d NULL\n", id, parent);
		return -1;
	}

	return clk_set_parent(ddp_clks[id].pclk, ddp_clks[parent].pclk);
}

static int __ddp_set_mipi26m(int idx, int en)
{
	if (idx != 0)
		DDPMSG("%s: not support DSI%d for mt3967\n", __func__, idx);

	if (en) {
		ddp_clk_prepare_enable(CLK_MIPID0_26M);
		apmixed_refcnt++;
	} else {
		ddp_clk_disable_unprepare(CLK_MIPID0_26M);
		apmixed_refcnt--;
	}

	return 0;
}

int ddp_clk_set_mipi26m(enum DISP_MODULE_ENUM module, int en)
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

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6779-apmixed");
	if (!node) {
		DDP_PR_ERR("[DDP_APMIXED] DISP find apmixed node failed\n");
		return -1;
	}

	ddp_apmixed_base = of_iomap(node, 0);
	if (!ddp_apmixed_base) {
		DDP_PR_ERR("[DDP_APMIXED] DISP apmixed base failed\n");
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

void ddp_clk_top_clk_switch(bool on)
{
	if (on) {
		ddp_clk_prepare_enable(CLK_MM_MTCMOS);
#ifdef CONFIG_MTK_SMI_EXT
		smi_bus_prepare_enable(SMI_LARB0, "DISP");
		smi_bus_prepare_enable(SMI_LARB1, "DISP");
#else
		ddp_clk_prepare_enable(CLK_GALS_COMM0);
		ddp_clk_prepare_enable(CLK_GALS_COMM1);
		ddp_clk_prepare_enable(CLK_SMI_COMMON);
		ddp_clk_prepare_enable(CLK_SMI_LARB0);
		ddp_clk_prepare_enable(CLK_SMI_LARB1);
#endif
		ddp_clk_prepare_enable(CLK_MM_26M);
	} else {
		ddp_clk_disable_unprepare(CLK_MM_26M);
#ifdef CONFIG_MTK_SMI_EXT
		smi_bus_disable_unprepare(SMI_LARB0, "DISP");
		smi_bus_disable_unprepare(SMI_LARB1, "DISP");
#else
		ddp_clk_disable_unprepare(CLK_SMI_LARB0);
		ddp_clk_disable_unprepare(CLK_SMI_LARB1);
		ddp_clk_disable_unprepare(CLK_SMI_COMMON);
		ddp_clk_disable_unprepare(CLK_GALS_COMM1);
		ddp_clk_disable_unprepare(CLK_GALS_COMM0);
#endif
		ddp_clk_disable_unprepare(CLK_MM_MTCMOS);
	}
}

/**
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
	ddp_clk_top_clk_switch(true);

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
				DDP_PR_ERR("[%s]%s no power on(off) function\n",
					   __func__, ddp_get_module_name(m));
				ret = -1;
			}
		}
	}

	/* DISP_DSI */
	m = _get_dst_module_by_lcm(primary_get_lcm());
	if (m == DISP_MODULE_UNKNOWN)
		ret = -1;
	else if (is_ddp_module(m) &&
		ddp_get_module_driver(m)->power_on)
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
	ddp_clk_top_clk_switch(true);

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
				DDP_PR_ERR("[%s]%s no power on(off) function\n",
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
	ddp_clk_top_clk_switch(true);

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
				DDP_PR_ERR("[%s]%s no power on(off) function\n",
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
				DDP_PR_ERR("[%s]%s no power on(off) function\n",
					   __func__, ddp_get_module_name(m));
				ret = -1;
			}
		}
	}

	/* DISP_DSI */
	m = _get_dst_module_by_lcm(primary_get_lcm());
	if (m == DISP_MODULE_UNKNOWN)
		ret = -1;
	else
		ddp_get_module_driver(m)->power_off(m, NULL);

	/* --TOP CLK-- */
	ddp_clk_top_clk_switch(false);

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
				DDP_PR_ERR("[%s]%s no power on(off) function\n",
					   __func__, ddp_get_module_name(m));
				ret = -1;
			}
		}
	}

	/* --TOP CLK-- */
	ddp_clk_top_clk_switch(false);

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
				DDP_PR_ERR("[%s]%s no power on(off) function\n",
					   __func__, ddp_get_module_name(m));
				ret = -1;
			}
		}
	}

	/* --TOP CLK-- */
	ddp_clk_top_clk_switch(false);

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

int ddp_clk_enable_by_module(enum DISP_MODULE_ENUM module)
{
	int ret = 0;
	enum DDP_CLK_ID id = MAX_DISP_CLK_CNT;

	id = ddp_get_module_clk_id(module);
	ret = ddp_clk_prepare_enable(id);
	if (ret)
		DDP_PR_ERR("invalid module id=%d\n", module);

	return ret;
}

int ddp_clk_disable_by_module(enum DISP_MODULE_ENUM module)
{
	int ret = 0;
	enum DDP_CLK_ID id = MAX_DISP_CLK_CNT;

	id = ddp_get_module_clk_id(module);
	ret = ddp_clk_disable_unprepare(id);
	if (ret)
		DDP_PR_ERR("invalid module id=%d\n", module);

	return ret;
}
