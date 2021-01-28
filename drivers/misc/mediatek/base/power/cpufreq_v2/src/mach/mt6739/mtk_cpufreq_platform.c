/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_MTK_FREQ_HOPPING
#include "mtk_freqhopping_drv.h"
#endif
#include "mtk_cpufreq_platform.h"
#include "mtk_cpufreq_hybrid.h"
#include "mtk_devinfo.h"

/* For feature enable decision */
#ifndef CONFIG_FPGA_EARLY_PORTING
#include "upmu_common.h"
#endif

#ifndef CONFIG_MTK_FREQ_HOPPING
#define FH_PLL0 0
#endif

static struct regulator *regulator_proc1;
static struct regulator *regulator_sram1;

static unsigned long apmixed_base	= 0x1000c000;
static unsigned long topckgen_ao_base	= 0x1001b000;
static unsigned long topckgen_base	= 0x10000000;

#define APMIXED_NODE		"mediatek,apmixed"
#define TOPCKGEN_AO_NODE	"mediatek,topckgen_ao"
#define TOPCKGEN_NODE		"mediatek,topckgen"

#define ARMPLL_LL_CON1		(apmixed_base + 0x204)		/* ARMPLL1 [26:24] [21:0]*/
#define CKDIV1_LL_CFG		(topckgen_ao_base + 0x02c)	/* MP0_PLL_DIVIDER [4:0]*/
#define CKDIV1_LL_MUXSEL	(topckgen_ao_base)			/* MP0_PLL_DIVIDER [11:8]*/
#define CLK_MISC_CFG_0		(topckgen_base + 0x104)		/*[4:4]*/

struct mt_cpu_dvfs cpu_dvfs[NR_MT_CPU_DVFS] = {
	[MT_CPU_DVFS_LL] = {
		.name		= __stringify(MT_CPU_DVFS_LL),
		.id		= MT_CPU_DVFS_LL,
		.cpu_id		= 0,
		.idx_normal_max_opp = -1,
		.idx_opp_ppm_base = 15,
		.idx_opp_ppm_limit = 0,
		.Vproc_buck_id	= CPU_DVFS_VPROC1,
		.Vsram_buck_id	= CPU_DVFS_VSRAM1,
		.Pll_id		= PLL_LL_CLUSTER,
	},
};

static int set_cur_volt_proc1_cpu(struct buck_ctrl_t *buck_p, unsigned int volt)
{
	unsigned int max_volt = MAX_VPROC_VOLT + PMIC_STEP;

	return regulator_set_voltage(regulator_proc1, volt * 10, max_volt * 10);
}

static unsigned int get_cur_volt_proc1_cpu(struct buck_ctrl_t *buck_p)
{
	unsigned int rdata;

	rdata = regulator_get_voltage(regulator_proc1) / 10;

	return rdata;
}

static unsigned int mt6357_volt_transfer2pmicval(unsigned int volt)
{
	return ((volt - 51875) + PMIC_STEP - 1) / PMIC_STEP;
}

static unsigned int mt6357_volt_transfer2volt(unsigned int val)
{
	return val * PMIC_STEP + 51875;
}

static unsigned int mt6357_vproc1_settletime(unsigned int old_volt, unsigned int new_volt)
{
	/* 6.25mv/0.34us */
	if (new_volt > old_volt)
		return ((new_volt - old_volt) + 1000 - 1) / 1000 + PMIC_CMD_DELAY_TIME;
	else
		return ((old_volt - new_volt) + 450 - 1) / 450 + PMIC_CMD_DELAY_TIME;
}

static int set_cur_volt_sram1_cpu(struct buck_ctrl_t *buck_p, unsigned int volt)
{
	unsigned int max_volt = MAX_VSRAM_VOLT + PMIC_STEP;

	return regulator_set_voltage(regulator_sram1, volt * 10, max_volt * 10);
}

static unsigned int get_cur_volt_sram1_cpu(struct buck_ctrl_t *buck_p)
{
	unsigned int rdata;

	rdata = regulator_get_voltage(regulator_sram1) / 10;

	return rdata;
}

static unsigned int mt6357_vsram1_settletime(unsigned int old_volt, unsigned int new_volt)
{
	if (new_volt > old_volt)
		return ((new_volt - old_volt) + 1000 - 1) / 1000 + PMIC_CMD_DELAY_TIME;
	else
		return ((old_volt - new_volt) + 450 - 1) / 450 + PMIC_CMD_DELAY_TIME;
}

/* upper layer CANNOT use 'set' function in secure path */
static struct buck_ctrl_ops buck_ops_mt6357_vproc1 = {
	.get_cur_volt		= get_cur_volt_proc1_cpu,
	.set_cur_volt		= set_cur_volt_proc1_cpu,
	.transfer2pmicval	= mt6357_volt_transfer2pmicval,
	.transfer2volt		= mt6357_volt_transfer2volt,
	.settletime		= mt6357_vproc1_settletime,
};

static struct buck_ctrl_ops buck_ops_mt6357_vsram1 = {
	.get_cur_volt		= get_cur_volt_sram1_cpu,
	.set_cur_volt		= set_cur_volt_sram1_cpu,
	.transfer2pmicval	= mt6357_volt_transfer2pmicval,
	.transfer2volt		= mt6357_volt_transfer2volt,
	.settletime		= mt6357_vsram1_settletime,
};

struct buck_ctrl_t buck_ctrl[NR_MT_BUCK] = {
	[CPU_DVFS_VPROC1] = {
		.name		= __stringify(BUCK_MT6357_VPROC),
		.buck_id	= CPU_DVFS_VPROC1,
		.buck_ops	= &buck_ops_mt6357_vproc1,
	},

	[CPU_DVFS_VSRAM1] = {
		.name		= __stringify(BUCK_MT6357_VSRAM),
		.buck_id	= CPU_DVFS_VSRAM1,
		.buck_ops	= &buck_ops_mt6357_vsram1,
	},
};

/* PMIC Part */
void prepare_pmic_config(struct mt_cpu_dvfs *p)
{
}

int __attribute__((weak)) sync_dcm_set_mp0_freq(unsigned int mhz)
{
	return 0;
}

int __attribute__((weak)) sync_dcm_set_mp1_freq(unsigned int mhz)
{
	return 0;
}

int __attribute__((weak)) sync_dcm_set_mp2_freq(unsigned int mhz)
{
	return 0;
}

int __attribute__((weak)) sync_dcm_set_cci_freq(unsigned int mhz)
{
	return 0;
}

/* PLL Part */
void prepare_pll_addr(enum mt_cpu_dvfs_pll_id pll_id)
{
	struct pll_ctrl_t *pll_p = id_to_pll_ctrl(pll_id);

	if (pll_p == NULL)
		return;

	pll_p->armpll_addr = (unsigned int *)ARMPLL_LL_CON1;

	pll_p->armpll_div_addr = (unsigned int *)CKDIV1_LL_CFG;
}

unsigned int _cpu_dds_calc(unsigned int khz)
{
	unsigned int dds;

	dds = ((khz / 1000) << 14) / 26;

	return dds;
}

static void adjust_armpll_dds(struct pll_ctrl_t *pll_p, unsigned int vco, unsigned int pos_div)
{
	unsigned int dds;
	unsigned int val;
	/* vco as frqeuency? */
	dds = _GET_BITS_VAL_(21:0, _cpu_dds_calc(vco));

	val = cpufreq_read(pll_p->armpll_addr) & ~(_BITMASK_(21:0));
	val |= dds;

	cpufreq_write(pll_p->armpll_addr, val | _BIT_(31) /* CHG */);
	udelay(PLL_SETTLE_TIME);
}

static void adjust_posdiv(struct pll_ctrl_t *pll_p, unsigned int pos_div)
{
	unsigned int sel;

	sel = (pos_div == 1 ? 0 :
	       pos_div == 2 ? 1 :
	       pos_div == 4 ? 2 : 0);

	cpufreq_write_mask(pll_p->armpll_addr, 26:24, sel);
	udelay(POS_SETTLE_TIME);
}

static void adjust_clkdiv(struct pll_ctrl_t *pll_p, unsigned int clk_div)
{
	unsigned int sel;

	sel = (clk_div == 1 ? 8 :
	       clk_div == 2 ? 10 :
	       clk_div == 4 ? 11 : 8);

	cpufreq_write_mask(pll_p->armpll_div_addr, 4:0, sel);
}

unsigned char get_posdiv(struct pll_ctrl_t *pll_p)
{
	unsigned char sel, cur_posdiv;

	sel = _GET_BITS_VAL_(26:24, cpufreq_read(pll_p->armpll_addr));
	cur_posdiv = (sel == 0 ? 1 :
		sel == 1 ? 2 :
		sel == 2 ? 4 : 1);

	return cur_posdiv;
}

unsigned char get_clkdiv(struct pll_ctrl_t *pll_p)
{
	unsigned char sel, cur_clkdiv;

	sel = _GET_BITS_VAL_(4:0, cpufreq_read(pll_p->armpll_div_addr));
	cur_clkdiv = (sel == 8 ? 1 :
		sel == 10 ? 2 :
		sel == 11 ? 4 : 1);

	return cur_clkdiv;
}

static void adjust_freq_hopping(struct pll_ctrl_t *pll_p, unsigned int dds)
{
#ifdef CONFIG_MTK_FREQ_HOPPING
	mt_dfs_general_pll(pll_p->hopping_id, dds);
#endif
}

/* Frequency API */
static unsigned int pll_to_clk(unsigned int pll_f, unsigned int ckdiv1)
{
	unsigned int freq = pll_f;

	switch (ckdiv1) {
	case 8:
		break;
	case 9:
		freq = freq * 3 / 4;
		break;
	case 10:
		freq = freq * 2 / 4;
		break;
	case 11:
		freq = freq * 1 / 4;
		break;
	case 16:
		break;
	case 17:
		freq = freq * 4 / 5;
		break;
	case 18:
		freq = freq * 3 / 5;
		break;
	case 19:
		freq = freq * 2 / 5;
		break;
	case 20:
		freq = freq * 1 / 5;
		break;
	case 24:
		break;
	case 25:
		freq = freq * 5 / 6;
		break;
	case 26:
		freq = freq * 4 / 6;
		break;
	case 27:
		freq = freq * 3 / 6;
		break;
	case 28:
		freq = freq * 2 / 6;
		break;
	case 29:
		freq = freq * 1 / 6;
		break;
	default:
		break;
	}

	return freq;
}

static unsigned int _cpu_freq_calc(unsigned int con1, unsigned int ckdiv1)
{
	unsigned int freq;
	unsigned int posdiv;

	posdiv = _GET_BITS_VAL_(26:24, con1);

	con1 &= _BITMASK_(21:0);
	freq = ((con1 * 26) >> 14) * 1000;

	switch (posdiv) {
	case 0:
		break;
	case 1:
		freq = freq / 2;
		break;
	case 2:
		freq = freq / 4;
		break;
	case 3:
		freq = freq / 8;
		break;
	default:
		freq = freq / 16;
		break;
	};

	return pll_to_clk(freq, ckdiv1);
}

unsigned int get_cur_phy_freq(struct pll_ctrl_t *pll_p)
{
	unsigned int con1;
	unsigned int ckdiv1;
	unsigned int cur_khz;

	con1 = cpufreq_read(pll_p->armpll_addr);
	ckdiv1 = cpufreq_read(pll_p->armpll_div_addr);
	ckdiv1 = _GET_BITS_VAL_(4:0, ckdiv1);

	cur_khz = _cpu_freq_calc(con1, ckdiv1);

	cpufreq_ver_dbg("@%s: (%s) = cur_khz = %u, con1[0x%p] = 0x%x, ckdiv1_val = 0x%x\n",
		    __func__, pll_p->name, cur_khz, pll_p->armpll_addr, con1, ckdiv1);

	return cur_khz;
}

static void _cpu_clock_switch(struct pll_ctrl_t *pll_p, enum top_ckmuxsel sel)
{
	switch (sel) {
	case TOP_CKMUXSEL_CLKSQ:
	case TOP_CKMUXSEL_ARMPLL:
		cpufreq_write_mask(CKDIV1_LL_MUXSEL, 11 : 8, sel);
		cpufreq_write_mask(CLK_MISC_CFG_0, 4 : 4, 0x0);
		break;
	case TOP_CKMUXSEL_MAINPLL:
		cpufreq_write_mask(CLK_MISC_CFG_0, 4 : 4, 0x1);
		udelay(3);
		cpufreq_write_mask(CKDIV1_LL_MUXSEL, 11 : 8, sel);
		break;
	default:
		break;
	}
}


static enum top_ckmuxsel _get_cpu_clock_switch(struct pll_ctrl_t *pll_p)
{
	return _GET_BITS_VAL_(11:8, cpufreq_read(CKDIV1_LL_MUXSEL));
}

/* upper layer CANNOT use 'set' function in secure path */
static struct pll_ctrl_ops pll_ops_ll = {
	.get_cur_freq		= get_cur_phy_freq,
	.set_armpll_dds		= adjust_armpll_dds,
	.set_armpll_posdiv	= adjust_posdiv,
	.set_armpll_clkdiv	= adjust_clkdiv,
	.set_freq_hopping	= adjust_freq_hopping,
	.clksrc_switch		= _cpu_clock_switch,
	.get_clksrc		= _get_cpu_clock_switch,
	.set_sync_dcm		= sync_dcm_set_mp0_freq,
};

struct pll_ctrl_t pll_ctrl[NR_MT_PLL] = {
	[PLL_LL_CLUSTER] = {
		.name		= __stringify(PLL_LL_CLUSTER),
		.pll_id		= PLL_LL_CLUSTER,
		.hopping_id	= FH_PLL0,	/* ARMPLL1 */
		.pll_ops	= &pll_ops_ll,
	},
};

/* Always put action cpu at last */
struct hp_action_tbl cpu_dvfs_hp_action[] = {
	{
		.action		= CPUFREQ_CPU_DOWN_PREPARE,
		.cluster	= MT_CPU_DVFS_LL,
		.trigged_core	= 1,
		.hp_action_cfg[MT_CPU_DVFS_LL].action_id = FREQ_LOW,
	},
};

unsigned int nr_hp_action = ARRAY_SIZE(cpu_dvfs_hp_action);

int mt_cpufreq_regulator_map(struct platform_device *pdev)
{
	int r;

	regulator_proc1 = regulator_get(&pdev->dev, "vproc");
	if (GEN_DB_ON(IS_ERR(regulator_proc1), "vproc Get Failed"))
		return -ENODEV;

	/* already on, no need to wait for settle */
	regulator_sram1 = regulator_get(&pdev->dev, "vsram_proc");
	if (GEN_DB_ON(IS_ERR(regulator_sram1), "Vsram_proc Get Failed"))
		return -ENODEV;

	r = regulator_enable(regulator_sram1);
	if (GEN_DB_ON(r, "Vsram_proc Enable Failed"))
		return -EPERM;

	return 0;
}

int mt_cpufreq_dts_map(void)
{
	struct device_node *node;

	/* apmixed_base */
	node = of_find_compatible_node(NULL, NULL, APMIXED_NODE);
	if (GEN_DB_ON(!node, "APMIXED Not Found"))
		return -ENODEV;

	apmixed_base = (unsigned long)of_iomap(node, 0);
	if (GEN_DB_ON(!apmixed_base, "APMIXED Map Failed"))
		return -ENOMEM;

	/* topckgen_ao_base */
	node = of_find_compatible_node(NULL, NULL, TOPCKGEN_AO_NODE);
	if (GEN_DB_ON(!node, "TOPCKGEN_AO Not Found"))
		return -ENODEV;

	topckgen_ao_base = (unsigned long)of_iomap(node, 0);
	if (GEN_DB_ON(!topckgen_ao_base, "TOPCKGEN_AO Map Failed"))
		return -ENOMEM;

	/* topckgen_base */
	node = of_find_compatible_node(NULL, NULL, TOPCKGEN_NODE);
	if (GEN_DB_ON(!node, "TOPCKGEN Not Found"))
		return -ENODEV;

	topckgen_base = (unsigned long)of_iomap(node, 0);
	if (GEN_DB_ON(!topckgen_base, "TOPCKGEN Map Failed"))
		return -ENOMEM;

	return 0;
}

#define SEG_EFUSE 30
#define TURBO_EFUSE 29 /* 590 */

unsigned int _mt_cpufreq_get_cpu_level(void)
{
	unsigned int lv = CPU_LEVEL_0;
	unsigned int seg_code = 0;
	unsigned int turbo_code = 0;

	seg_code = get_devinfo_with_index(SEG_EFUSE);

	turbo_code = get_devinfo_with_index(TURBO_EFUSE);
	turbo_code = _GET_BITS_VAL_(21:20, turbo_code);

	if ((seg_code == 0x80 || seg_code == 0x88 || seg_code == 0x0
		|| seg_code == 0x08 || seg_code == 0x90) && turbo_code == 0x3)
		lv = CPU_LEVEL_1;
	else if ((seg_code == 0x48 || seg_code == 0x40 || seg_code == 0xC8
		|| seg_code == 0xC0 || seg_code == 0xD0) && turbo_code == 0x3)
		lv = CPU_LEVEL_1;
	else if (seg_code == 0xAE || seg_code == 0x2E)
		lv = CPU_LEVEL_2;
	else
		lv = CPU_LEVEL_0;

	return lv;
}

unsigned int _mt_cpufreq_disable_feature(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	if (PMIC_LP_CHIP_VER() == 1)
		return 1;
	else
		return 0;
#else
	return 1;
#endif
}
