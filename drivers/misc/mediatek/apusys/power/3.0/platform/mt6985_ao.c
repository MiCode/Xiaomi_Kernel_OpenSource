// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>

#include "apusys_secure.h"
#include "aputop_rpmsg.h"
#include "apu_top.h"
#include "mt6985_apupwr.h"
#include "mt6985_apupwr_prot.h"

#define LOCAL_DBG	(1)
#define RPC_ALIVE_DBG	(0)
static uint32_t ce_pwr_on[] = {
	0x31ba0001,
	0x32011900,
	0x0103c005,
	0x30ba0000,
	0x30818000,
	0x39120001,
	0x51390003,
	0x7603c007,
	0x7683c007,
	0x0003c001,
	0x30ba4000,
	0x30810000,
	0x38020001,
	0x50390003,
	0x70c3c007,
	0x50380003,
	0x7083c007,
	0x9303c00a,
	0x51390003,
	0x9103c00a,
	0x9083c00a,
	0x31ba0002,
	0x60000004,
	0x31ba0003,
	0x72c3c007,
	0x60000002,
	0x7303c007,
	0x60000006,
	0x31ba0004,
	0x73c3c007,
	0x0003c81d,
	0x30ba4000,
	0x38830001,
	0x00c3c81d,
	0x30babfff,
	0x3081ffff,
	0x38820001,
	0x00c3c81d,
	0x60000002,
	0x0003c81d,
	0x30ba1000,
	0x38830001,
	0x00c3c81d,
	0x30baefff,
	0x3081ffff,
	0x38820001,
	0x00c3c81d,
	0x60000002,
	0x8b83c00a,
	0x31ba0005,
	0x0003c001,
	0x30ba8000,
	0x30810000,
	0x38020001,
	0x50390003,
	0x7543c007,
	0x50380003,
	0x7503c007,
	0x9603c00a,
	0x31ba0006,
	0x7543c007,
	0x5139000c,
	0x7443c007,
	0x0003c001,
	0x30ba1000,
	0x30810000,
	0x38020001,
	0x50390003,
	0x7043c007,
	0x50380003,
	0x7003c007,
	0x9203c00a,
	0x5138000b,
	0x7043c007,
	0x0003c001,
	0x30ba0000,
	0x30818000,
	0x38020001,
	0x50390003,
	0x7443c007,
	0x50380003,
	0x7403c007,
	0x9283c00a,
	0x31ba0007,
	0x0003c001,
	0x30ba0000,
	0x30810800,
	0x38020001,
	0x50390003,
	0x74c3c007,
	0x50380003,
	0x7483c007,
	0x9683c00a,
	0x31ba0008,
	0x74c3c007,
	0x0003c81d,
	0x30ba0200,
	0x39030001,
	0x0143c81d,
	0x30bafdff,
	0x3081ffff,
	0x39020001,
	0x0143c81d,
	0x60000002,
	0x31ba0009,
	0x0003c81e,
	0x30ba0400,
	0x39030001,
	0x0143c81e,
	0x30bafbff,
	0x3081ffff,
	0x39020001,
	0x0143c81e,
	0x60000002,
	0x31ba000a,
	0x7103c002,
	0x9703c00a,
	0x9783c00a,
	0x31ba000b,
	0x28010000, //0x78 elements = 120 elements = 120 * 4 = 480 bytes
};
static uint32_t ce_pwr_on_sz = sizeof(ce_pwr_on);
static uint32_t ce_pwr_off[] = {
	0x31ba000b,
	0x32011900,
	0x03c3c002,
	0x8f03c00a,
	0x9783c00a,
	0x31ba000c,
	0x0003c001,
	0x30ba0000,
	0x30811000,
	0x38020001,
	0x50390009,
	0x0083c81e,
	0x313a0400,
	0x390b0002,
	0x0143c81e,
	0x313afbff,
	0x3101ffff,
	0x390a0002,
	0x0143c81e,
	0x50380008,
	0x0083c81d,
	0x313a0400,
	0x390b0002,
	0x0143c81d,
	0x313afbff,
	0x3101ffff,
	0x390a0002,
	0x0143c81d,
	0x60000002,
	0x31ba000d,
	0x0103c001,
	0x30ba0040,
	0x30810000,
	0x390a0002,
	0x38030002,
	0x50390009,
	0x0083c81d,
	0x313a0200,
	0x390b0002,
	0x0143c81d,
	0x313afdff,
	0x3101ffff,
	0x390a0002,
	0x0143c81d,
	0x50380008,
	0x0083c81e,
	0x313a0200,
	0x390b0002,
	0x0143c81e,
	0x313afdff,
	0x3101ffff,
	0x390a0002,
	0x0143c81e,
	0x60000002,
	0x31ba000e,
	0x7043c007,
	0x7443c007,
	0x0003c005,
	0x30ba0000,
	0x30818000,
	0x38020001,
	0x50390008,
	0x0003c001,
	0x30ba1000,
	0x30810000,
	0x38020001,
	0x50380002,
	0x8a03c00a,
	0x50380007,
	0x0003c001,
	0x30ba0000,
	0x30818000,
	0x38020001,
	0x50380002,
	0x8a83c00a,
	0x31ba000f,
	0x0003c001,
	0x30ba4000,
	0x30810000,
	0x39020001,
	0x51390003,
	0x73c3c007,
	0x51380002,
	0x7383c007,
	0x60000002,
	0x0003c001,
	0x30ba0000,
	0x30812000,
	0x38020001,
	0x50390009,
	0x0083c81d,
	0x31ba1000,
	0x398b0003,
	0x01c3c81d,
	0x31baefff,
	0x3181ffff,
	0x398a0003,
	0x01c3c81d,
	0x50380008,
	0x0083c81e,
	0x31ba1000,
	0x398b0003,
	0x01c3c81e,
	0x31baefff,
	0x3181ffff,
	0x398a0003,
	0x01c3c81e,
	0x60000002,
	0x51380002,
	0x9403c00a,
	0x31ba0010,
	0x0003c001,
	0x30ba0001,
	0x30810000,
	0x38020001,
	0x50390009,
	0x0083c81d,
	0x31ba4000,
	0x398b0003,
	0x01c3c81d,
	0x31babfff,
	0x3181ffff,
	0x398a0003,
	0x01c3c81d,
	0x50380008,
	0x0083c81e,
	0x31ba4000,
	0x398b0003,
	0x01c3c81e,
	0x31babfff,
	0x3181ffff,
	0x398a0003,
	0x01c3c81e,
	0x51390003,
	0x72c3c007,
	0x51380002,
	0x7283c007,
	0x60000002,
	0x31ba0011,
	0x70c3c007,
	0x8b03c00a,
	0x31ba0000,
	0x7643c007,
	0x76c3c007,
	0x77c3c007,
	0x51390003,
	0x7303c007,
	0x51380002,
	0x7343c007,
	0x28010000,
};
static uint32_t ce_pwr_off_sz = sizeof(ce_pwr_off);

static struct apu_power *papw;

/* regulator id */
static struct regulator *vapu_reg_id;
static struct regulator *vcore_reg_id;
static struct regulator *vsram_reg_id;

static void _apu_w_are(int entry, ulong reg, ulong data)
{
	ulong are_entry_addr;

	/* (address of entry) = register */
	are_entry_addr = (ulong)papw->regs[apu_are] + 4 * ARE_ENTRY(entry);
	apu_writel(reg, (void __iomem *)are_entry_addr);
	apu_writel(data, (void __iomem *)(are_entry_addr + 4));
}

static void aputop_dump_reg(enum apupw_reg idx, uint32_t offset, uint32_t size)
{
	char buf[32];

	// reg dump for RPC
	memset(buf, 0, sizeof(buf));
	snprintf(buf, 32, "phys 0x%08x: ",
			(u32)(papw->phy_addr[idx]) + offset);
	print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
			papw->regs[idx] + offset, size, true);
}


static int check_if_rpc_alive(void)
{
	unsigned int regValue = 0x0;
	int bit_offset = 26; // [31:26] is reserved for debug

	regValue = apu_readl(papw->regs[apu_rpc] + APU_RPC_TOP_SEL);
	pr_info("%s , before: APU_RPC_TOP_SEL = 0x%x\n", __func__, regValue);
	regValue |= (0x3a << bit_offset);
	apu_writel(regValue, papw->regs[apu_rpc] + APU_RPC_TOP_SEL);

	regValue = 0x0;
	regValue = apu_readl(papw->regs[apu_rpc] + APU_RPC_TOP_SEL);
	pr_info("%s , after: APU_RPC_TOP_SEL = 0x%x\n", __func__, regValue);

	apu_clearl((BIT(26) | BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31)),
					papw->regs[apu_rpc] + APU_RPC_TOP_SEL);

	return ((regValue >> bit_offset) & 0x3f) == 0x3a ? 1 : 0;
}

// WARNING: can not call this API after acc initial or you may cause bus hang !
static void dump_rpc_lite_reg(int line)
{
	pr_info("%s ln_%d acx%d APU_RPC_TOP_SEL=0x%08x\n",
			__func__, line, 0,
			apu_readl(papw->regs[apu_acx0_rpc_lite]
				+ APU_RPC_TOP_SEL));

	pr_info("%s ln_%d acx%d APU_RPC_TOP_SEL=0x%08x\n",
		__func__, line, 1,
		apu_readl(papw->regs[apu_acx1_rpc_lite]
			+ APU_RPC_TOP_SEL));

	pr_info("%s ln_%d acx%d APU_RPC_TOP_SEL=0x%08x\n",
		__func__, line, 2,
		apu_readl(papw->regs[apu_ncx_rpc_lite]
			+ APU_RPC_TOP_SEL));

}

static int init_plat_pwr_res(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("%s %d ++\n", __func__, __LINE__);
	// vapu Buck
	vapu_reg_id = regulator_get(&pdev->dev, "vapu");
	if (!vapu_reg_id) {
		pr_info("regulator_get vapu_reg_id failed\n");
		return -ENOENT;
	}

	// vcore
	vcore_reg_id = regulator_get(&pdev->dev, "vcore");
	if (!vcore_reg_id) {
		pr_info("regulator_get vcore_reg_id failed\n");
		return -ENOENT;
	}

	// vsram
	vsram_reg_id = regulator_get(&pdev->dev, "vsram_core");
	if (!vsram_reg_id) {
		pr_info("regulator_get vsram_reg_id failed\n");
		return -ENOENT;
	}

	// enable vapu buck
	ret = regulator_enable(vapu_reg_id);
	if (ret < 0) {
		pr_info("%s fail enable vapu : %d\n", __func__, ret);
		return -1;
	}

	pr_info("%s %d vapu = %d(uv, %s) --\n",
			__func__, __LINE__,
			regulator_get_voltage(vapu_reg_id),
		    regulator_is_enabled(vapu_reg_id) ? "enabled" : "disabled");
	return 0;
}

static void destroy_plat_pwr_res(void)
{
	int ret = 0;

	// disable vapu buck
	ret = regulator_disable(vapu_reg_id);
	if (ret < 0) {
		pr_info("%s fail disable vapu : %d\n", __func__, ret);
		return;
	}

	regulator_put(vcore_reg_id);
	regulator_put(vsram_reg_id);
	regulator_put(vapu_reg_id);
	vcore_reg_id = NULL;
	vsram_reg_id = NULL;
	vapu_reg_id = NULL;
}

static void get_pll_pcw(uint32_t clk_rate, uint32_t *r1, uint32_t *r2)
{
	unsigned int fvco = clk_rate;
	unsigned int pcw_val;
	unsigned int postdiv_val = 1;
	unsigned int postdiv_reg = 0;

	while (fvco <= 1500) {
		postdiv_val = postdiv_val << 1;
		postdiv_reg = postdiv_reg + 1;
		fvco = fvco << 1;
	}

	pcw_val = (fvco * 1 << 14) / 26;

	if (postdiv_reg == 0) { //Fvco * 2 with post_divider = 2
		pcw_val = pcw_val * 2;
		postdiv_val = postdiv_val << 1;
		postdiv_reg = postdiv_reg + 1;
	} //Post divider is 1 is not available

	*r1 = postdiv_reg;
	*r2 = pcw_val;
}

static void __apu_engine_acc_on(void)
{
	// need to 1-1 in order mapping to these two array
	uint32_t eng_acc[] = {MDLA_ACC_BASE, MVPU_ACC_BASE,
			      APS_ACC_BASE,};
	int eng_acc_arr_size = ARRAY_SIZE(eng_acc);
	ulong addr = 0;
	uint32_t val = 0;
	int ret = 0, acc_idx;

	for (acc_idx = 0; acc_idx < eng_acc_arr_size; acc_idx++) {
		addr = (ulong)papw->regs[apu_pll] + eng_acc[acc_idx] + APU_ACC_AUTO_CTRL_SET0;

		/* TINFO="[pllon/off]Step2: auto enable acc_idx clock" */
		apu_setl(1 << 9, (void __iomem *)addr);
		addr = (ulong)papw->regs[apu_pll] + eng_acc[acc_idx] + APU_ACC_AUTO_STATUS0;

		ret = readl_relaxed_poll_timeout_atomic((void *)addr, val,
							(val & (0x1UL << 5)), 50, 10000);
		if (ret)
			pr_info("%s %d wait acc-%d on fail, ret = %d\n",
			       __func__, __LINE__, acc_idx, ret);
	}
}

static void __apu_pll_init(void)
{
	// need to 1-1 in order mapping to these two array
	uint32_t pll_b[] = {MNOC_PLL_BASE, UP_PLL_BASE,
					MDLA_PLL_BASE, MVPU_PLL_BASE,
					APS_PLL_BASE,};
	int32_t pll_freq_out[] = {400, 400, 400, 400, 400}; // MHz
	uint32_t pcw_val, posdiv_val;
	int pll_arr_size = ARRAY_SIZE(pll_b);
	int pll_i, are_idx;

	// Step4. Initial PLL setting
	pr_info("PLL init %s %d --\n", __func__, __LINE__);

	/* Turn on RCX_AO ARE */
	apu_writel(1 << 21, papw->regs[apu_are]);
	apu_writel(1 << 21, papw->regs[apu_vcore] + 0x3000);

	/* write RCX_AO start entry/size */
	apu_writel(ARE_ENTRY(RCX_AO_BEGIN) | (ARE_ENTRIES(RCX_AO_BEGIN, RCX_AO_END) << 16),
			papw->regs[apu_are] + ARE_RCX_AO_CONFIG);

	are_idx = PLL_ENTRY_BEGIN;
	for (pll_i = 0 ; pll_i < pll_arr_size ; pll_i++) {
		// PCW value always from hopping function: ofs 0x300
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1CPLL_FHCTL_HP_EN,
			   0x1 << 0);

		// Hopping function reset release: ofs 0x30C
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1CPLL_FHCTL_RST_CON,
			   0x1 << 0);

		// Hopping function clock enable: ofs 0x308
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1CPLL_FHCTL_CLK_CON,
			   0x1 << 0);

		// Hopping function enable: ofs 0x314
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1CPLL_FHCTL0_CFG,
			   (0x1 << 0) | (0x1 << 2));

		posdiv_val = 0;
		pcw_val = 0;
		get_pll_pcw(pll_freq_out[pll_i], &posdiv_val, &pcw_val);

		// POSTDIV: ofs 0x20C , [26:24] RG_PLL_POSDIV
		// 3'b000: /1 , 3'b001: /2 , 3'b010: /4
		// 3'b011: /8 , 3'b100: /16
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1C_PLL1_CON1,
			   ((0x1 << 31) | (posdiv_val << 24) | pcw_val));

		// PCW register: ofs 0x31C
		// [31] FHCTL0_PLL_TGL_ORG
		// [21:0] FHCTL0_PLL_ORG set to PCW value
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1CPLL_FHCTL0_DDS,
			   ((0x1 << 31) | pcw_val));
	}
}

/* Cost 18 ARE entries, ARDCM(8) + ACC(4+6=10) */
static void __apu_acc_init(void)
{

	uint32_t top[] = {MNOC_ACC_BASE, UP_ACC_BASE};
	uint32_t eng[] = {MDLA_ACC_BASE, MVPU_ACC_BASE, APS_ACC_BASE,};
	int top_acc_arr_size = ARRAY_SIZE(top);
	int eng_acc_arr_size = ARRAY_SIZE(eng);
	int acc_idx;

	int are_idx = ACC_ENTRY_BEGIN;
	// Step6. Initial ACC setting (@ACC)

	for (acc_idx = 0 ; acc_idx < top_acc_arr_size ; acc_idx++) {
		// DCM_EN/DBC_EN
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + top[acc_idx]  + APU_ARDCM_CTRL1,
			   0x00000006);
		// APB_DCM_EN/APB_DBC_EN/APB_IDLE_FSEL_UPD_EN
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + top[acc_idx]  + APU_ARDCM_CTRL0,
			   0x00000016);
		// IDLE_FSEL/DBC_CNT
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + top[acc_idx]  + APU_ARDCM_CTRL1,
			   0x07F0F000);
		// APB_LOAD_TOG
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + top[acc_idx]  + APU_ARDCM_CTRL0,
			   0x00000020);

		// CGEN_SOC
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + top[acc_idx]  + APU_ACC_CONFG_CLR0,
			   0x00000004);
		// HW_CTRL_EN
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + top[acc_idx] + APU_ACC_CONFG_SET0,
			   0x00008000);
	}

	for (acc_idx = 0 ; acc_idx < eng_acc_arr_size ; acc_idx++) {
		// CGEN_SOC
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + eng[acc_idx]  + APU_ACC_CONFG_CLR0,
			   0x00000004);
		// HW_CTRL_EN
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + eng[acc_idx] + APU_ACC_CONFG_SET0,
			   0x00008000);
		// CLK_REQ_SW_EN
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + eng[acc_idx] + APU_ACC_AUTO_CTRL_SET0,
			   0x00000100);
	}
	_apu_w_are(are_idx++, (ulong)0x190E1040, 0x12345678);

}

static void __apu_buck_off_cfg(void)
{
	pr_info("%s %d ++\n", __func__, __LINE__);
	// Step12. After APUsys is finished, update the following register to 1,
	//     ARE will use this information to ensure the SRAM in ARE is
	//     trusted or not
	//     apusys_initial_done
	apu_setl(1 << 6, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	apu_setl(1 << 7, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	apu_setl(1 << 8, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	apu_setl(1 << 11, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	apu_setl(1 << 13, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);

	apu_clearl(1 << 6, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	apu_clearl(1 << 7, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	apu_clearl(1 << 8, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	apu_clearl(1 << 11, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	apu_clearl(1 << 13, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);

	pr_info("%s %d --\n", __func__, __LINE__);
}

/*
 * low 32-bit data for PMIC control
 *	APU_PCU_PMIC_TAR_BUF1 (or APU_PCU_BUCK_ON_DAT0_L)
 *	[31:16] offset to update
 *	[15:00] data to update
 *
 * high 32-bit data for PMIC control
 *	APU_PCU_PMIC_TAR_BUF2 (or APU_PCU_BUCK_ON_DAT0_H)
 *	[2:0] cmd_op, read:0x3 , write:0x7
 *	[3]: pmifid,
 *	[7:4]: slvid
 *	[8]: bytecnt
 */
static void __apu_pcu_init(void)
{
	uint32_t cmd_op_w = 0x7;
	uint32_t pmif_id = 0x0;
	uint32_t slave_id = SUB_PMIC_ID;
	uint32_t en_set_offset = BUCK_VAPU_PMIC_REG_EN_SET_ADDR;
	uint32_t en_clr_offset = BUCK_VAPU_PMIC_REG_EN_CLR_ADDR;
	uint32_t en_shift = BUCK_VAPU_PMIC_REG_EN_SHIFT;

	if (papw->env == FPGA)
		return;

	pr_info("PCU init %s %d ++\n", __func__, __LINE__);
	// auto buck enable
	apu_writel((0x1 << 16), papw->regs[apu_pcu] + APU_PCUTOP_CTRL_SET);

	// Step1. enable auto buck on/off function of command0
	// [0]: cmd0 enable auto ON, [4]: cmd0 enable auto OFF
	apu_writel(0x11, papw->regs[apu_pcu] + APU_PCU_BUCK_STEP_SEL);

	// Step2. fill-in command0 for vapu auto buck ON
	apu_writel((en_set_offset << 16) | (0x1 << en_shift),
			papw->regs[apu_pcu] + APU_PCU_BUCK_ON_DAT0_L);
	apu_writel((slave_id << 4) | (pmif_id << 3) | cmd_op_w,
			papw->regs[apu_pcu] + APU_PCU_BUCK_ON_DAT0_H);

	// APU_PCU_BUCK_ON_DAT0_L=0x02410040
	// APU_PCU_BUCK_ON_DAT0_H=0x00000057

	pr_info("%s APU_PCU_BUCK_ON_DAT0_L=0x%08x\n", __func__,
		apu_readl(papw->regs[apu_pcu] + APU_PCU_BUCK_ON_DAT0_L));
	pr_info("%s APU_PCU_BUCK_ON_DAT0_H=0x%08x\n", __func__,
		apu_readl(papw->regs[apu_pcu] + APU_PCU_BUCK_ON_DAT0_H));

	// Step3. fill-in command0 for vapu auto buck OFF
	apu_writel((en_clr_offset << 16) | (0x1 << en_shift),
			papw->regs[apu_pcu] + APU_PCU_BUCK_OFF_DAT0_L);
	apu_writel((slave_id << 4) | (pmif_id << 3) | cmd_op_w,
			papw->regs[apu_pcu] + APU_PCU_BUCK_OFF_DAT0_H);
	pr_info("%s APU_PCU_BUCK_OFF_DAT0_L=0x%08x\n", __func__,
		apu_readl(papw->regs[apu_pcu] + APU_PCU_BUCK_OFF_DAT0_L));
	pr_info("%s APU_PCU_BUCK_OFF_DAT0_H=0x%08x\n", __func__,
		apu_readl(papw->regs[apu_pcu] + APU_PCU_BUCK_OFF_DAT0_H));

	// Step4. update buck settle time for vapu by SEL0
	apu_writel(VAPU_BUCK_ON_SETTLE_TIME,
			papw->regs[apu_pcu] + APU_PCU_BUCK_ON_SLE0);

	pr_info("PCU init %s %d --\n", __func__, __LINE__);
}

static void __apu_rpclite_init(enum t_acx_id acx_idx)
{
	uint32_t sleep_type_offset[] = {0x0208, 0x020C,	0x0218, 0x021C};
	enum apupw_reg rpc_lite_base[CLUSTER_NUM];
	int ofs_arr_size = sizeof(sleep_type_offset) / sizeof(uint32_t);
	int ofs_idx;

	pr_info("%s %d ++\n", __func__, __LINE__);

	rpc_lite_base[ACX0] = apu_acx0_rpc_lite;
	rpc_lite_base[ACX1] = apu_acx1_rpc_lite;
	rpc_lite_base[NCX] = apu_ncx_rpc_lite;

	for (ofs_idx = 0 ; ((ofs_idx < ofs_arr_size) && (acx_idx != NCX)); ofs_idx++) {
		// Memory setting
		apu_clearl((0x1 << 1),
			   papw->regs[rpc_lite_base[acx_idx]] + sleep_type_offset[ofs_idx]);
	}
	// Control setting
	apu_setl(0x0000009E,
		 papw->regs[rpc_lite_base[acx_idx]] + APU_RPC_TOP_SEL);
	pr_info("%s %d ++\n", __func__, __LINE__);
}

static void __apu_rpclite_init_all(void)
{
	uint32_t sleep_type_offset[] = {0x0208, 0x020C,	0x0218, 0x021C};
	enum apupw_reg rpc_lite_base[CLUSTER_NUM];
	int ofs_arr_size = ARRAY_SIZE(sleep_type_offset);
	int acx_idx, ofs_idx;

	pr_info("%s %d ++\n", __func__, __LINE__);

	rpc_lite_base[ACX0] = apu_acx0_rpc_lite;
	rpc_lite_base[ACX1] = apu_acx1_rpc_lite;
	rpc_lite_base[NCX] = apu_ncx_rpc_lite;

	for (acx_idx = 0 ; acx_idx < CLUSTER_NUM ; acx_idx++) {
		for (ofs_idx = 0 ; ((ofs_idx < ofs_arr_size) && (acx_idx != NCX)); ofs_idx++) {
			// Memory setting
			apu_clearl((0x1 << 1),
					papw->regs[rpc_lite_base[acx_idx]]
					+ sleep_type_offset[ofs_idx]);
		}

		// Control setting
		apu_setl(0x0000009E, papw->regs[rpc_lite_base[acx_idx]]
					+ APU_RPC_TOP_SEL);
	}
	dump_rpc_lite_reg(__LINE__);
	pr_info("%s %d ++\n", __func__, __LINE__);
}

static void __apu_rpc_init(void)
{
	pr_info("RPC init %s %d ++\n", __func__, __LINE__);
	// Step7. RPC: memory types (sleep or PD type)
	// RPC: iTCM in uP need to setup to sleep type
	apu_clearl((0x1 << 1), papw->regs[apu_rpc] + 0x0200);

	// Step9. RPCtop initial
	/* 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 (bit offset)
	 *  1  0  1  1  1  0  0  0  0  0  0  0  0  0  0  0 --> 0xB800
	 * 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0 (bit offset)
	 *  1  1  0  1  0  1  0  0  0  0  0  0  1  1  1  1 --> 0xD40F
	 */
	apu_writel(0xB800D40F, papw->regs[apu_rpc] + APU_RPC_TOP_SEL);

	/* if PRC_HW, turn off CE wake up RPC */
	if (papw->rcx == RPC_HW)
		apu_clearl(1 << 10, papw->regs[apu_rpc] + APU_RPC_TOP_SEL);

	// BUCK_PROT_SEL
	apu_writel(0x1a4b0568, papw->regs[apu_rpc] + APU_RPC_TOP_SEL_1);

	pr_info("%s APU_RPC_TOP_SEL  0x%08x = 0x%08x\n",
			__func__,
			(u32)(papw->phy_addr[apu_rpc] + APU_RPC_TOP_SEL),
			readl(papw->regs[apu_rpc] + APU_RPC_TOP_SEL));

	pr_info("%s APU_RPC_TOP_SEL_1 0x%08x = 0x%08x\n",
			__func__,
			(u32)(papw->phy_addr[apu_rpc] + APU_RPC_TOP_SEL_1),
			readl(papw->regs[apu_rpc] + APU_RPC_TOP_SEL_1));

	pr_info("RPC init %s %d --\n", __func__, __LINE__);
}

static int __apu_are_init(struct device *dev)
{
	uint32_t entry = 0;
	char buf[512];

	pr_info("ARE init %s %d ++\n", __func__, __LINE__);

	/* clean all enable core as 0 */
	apu_clearl(0xFFF<<20, papw->regs[apu_are]);

	/* clean entry#1(0x19060000) ~ #16(0x19060040) */
	for (entry = 4; entry < 0x40; entry += 4)
		apu_writel(0, papw->regs[apu_are] + entry);

	if (papw->rcx == RPC_HW)
		return 0;

	/* Turn on CE enable */
	apu_writel(1<<23, papw->regs[apu_are]);

	/* fill in rpc power on/off firmware */
	memcpy(papw->regs[apu_are] + 0x2000, ce_pwr_on, ce_pwr_on_sz);
	memcpy(papw->regs[apu_are] + 0x2400, ce_pwr_off, ce_pwr_off_sz);

	/* fill in entry 4 */
	entry = (0x2400/4) << 16 | (0x2000/4);
	apu_writel(entry, papw->regs[apu_are] + 0x10);

	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "phys 0x%08x ", (u32)(papw->phy_addr[apu_are]));
	print_hex_dump(KERN_WARNING, buf, DUMP_PREFIX_OFFSET,
		       16, 4, papw->regs[apu_are], 0x20, 1);

	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "phys 0x%08x ", (u32)(papw->phy_addr[apu_are] + 0x2000));
	print_hex_dump(KERN_WARNING, buf, DUMP_PREFIX_OFFSET,
		       16, 4, papw->regs[apu_are] + 0x2000, ce_pwr_on_sz, 1);

	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "phys 0x%08x ", (u32)(papw->phy_addr[apu_are] + 0x2400));
	print_hex_dump(KERN_WARNING, buf, DUMP_PREFIX_OFFSET,
		       16, 4, papw->regs[apu_are] + 0x2400, ce_pwr_off_sz, 1);

	pr_info("ARE init %s %d --\n", __func__, __LINE__);
	return 0;
}


// backup solution : send request for RPC sleep from APMCU
static int __apu_off_rpc_rcx(struct device *dev)
{
	int ret = 0, val = 0;

	/* TINFO="APU_RPC_TOP_SEL[7] - BYPASS WFI" */
	apu_setl(1 << 7, papw->regs[apu_rpc] + APU_RPC_TOP_SEL);

	/* ONLY FPGA NEED, Ignore Sleep Protect Rdy */
	if (papw->env == FPGA)
		apu_setl(1 << 12, papw->regs[apu_rpc] + APU_RPC_MTCMOS_SW_CTRL0);

	/* SLEEP request */
	apu_setl(1, papw->regs[apu_rpc] + APU_RPC_TOP_CON);
	ret = readl_relaxed_poll_timeout_atomic(
			(papw->regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, !(val & 0x1UL), 50, 10000);
	if (ret)
		pr_info("%s polling RPC RDY timeout, ret %d\n", __func__, ret);

	aputop_dump_reg(apu_rpc, 0, 0x50);

	dev_info(dev, "%s APUSYS_VCORE_CG_CON 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_vcore] + APUSYS_VCORE_CG_CON),
			readl(papw->regs[apu_vcore] + APUSYS_VCORE_CG_CON));

	dev_info(dev, "%s APU_RCX_CG_CON 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_rcx] + APU_RCX_CG_CON),
			readl(papw->regs[apu_rcx] + APU_RCX_CG_CON));

	dev_info(dev, "%s APU_ARE_GCONFIG 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_vcore] + APU_ARE_GCONFIG),
			readl(papw->regs[apu_vcore] + APU_ARE_GCONFIG));

	dev_info(dev, "%s APU_ARE_STATUS 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_vcore] + APU_ARE_STATUS),
			readl(papw->regs[apu_vcore] + APU_ARE_STATUS));

	dev_info(dev, "%s APU_CE_IF_PC 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_vcore] + APU_CE_IF_PC),
			readl(papw->regs[apu_vcore] + APU_CE_IF_PC));
	return ret;
}


static int __apu_wake_rpc_rcx(struct device *dev)
{
	int ret = 0, val = 0;

	dev_info(dev, "%s Before wakeup RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(papw->regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	/* TINFO="Enable AFC enable" */
	apu_setl(0x1 << 16, papw->regs[apu_rpc] + APU_RPC_TOP_SEL_1);

	/* wake up RPC */
	apu_writel(0x00000100, papw->regs[apu_rpc] + APU_RPC_TOP_CON);
	ret = readl_relaxed_poll_timeout_atomic(
			(papw->regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL), 50, 10000);
	if (ret) {
		pr_info("%s polling RPC RDY timeout, val = 0x%x, ret %d\n", __func__, val, ret);
		goto out;
	}

	/* clear wakeup signal */
	apu_writel(0x1 << 12, papw->regs[apu_rpc] + APU_RPC_TOP_CON);

	dev_info(dev, "%s RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(papw->regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	/* polling FSM @RPC-lite to ensure RPC is in on/off stage */
	ret |= readl_relaxed_poll_timeout_atomic(
			(papw->regs[apu_rpc] + APU_RPC_STATUS),
			val, (val & (0x1 << 29)), 50, 10000);
	if (ret) {
		pr_info("%s polling ARE FSM timeout, ret %d\n", __func__, ret);
		goto out;
	}

	/* clear vcore/rcx cgs */
	apu_writel(0xFFFFFFFF, papw->regs[apu_vcore] + APUSYS_VCORE_CG_CLR);
	apu_writel(0xFFFFFFFF, papw->regs[apu_rcx] + APU_RCX_CG_CLR);

out:
	dev_info(dev, "%s APUSYS_VCORE_CG_CON 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_vcore] + APUSYS_VCORE_CG_CON),
			readl(papw->regs[apu_vcore] + APUSYS_VCORE_CG_CON));

	dev_info(dev, "%s APU_RCX_CG_CON 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_rcx] + APU_RCX_CG_CON),
			readl(papw->regs[apu_rcx] + APU_RCX_CG_CON));

	dev_info(dev, "%s APU_ARE_GCONFIG 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_vcore] + APU_ARE_GCONFIG),
			readl(papw->regs[apu_vcore] + APU_ARE_GCONFIG));

	dev_info(dev, "%s APU_ARE_STATUS 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_vcore] + APU_ARE_STATUS),
			readl(papw->regs[apu_vcore] + APU_ARE_STATUS));

	dev_info(dev, "%s APU_CE_IF_PC 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_vcore] + APU_CE_IF_PC),
			readl(papw->regs[apu_vcore] + APU_CE_IF_PC));
	return ret;
}


static int __apu_wake_rpc_acx(struct device *dev, enum t_acx_id acx_id)
{
	int ret = 0, val = 0;
	enum apupw_reg rpc_lite_base;
	enum apupw_reg acx_base;

	if (acx_id == ACX0) {
		rpc_lite_base = apu_acx0_rpc_lite;
		acx_base = apu_acx0;
	} else if (acx_id == ACX1) {
		rpc_lite_base = apu_acx1_rpc_lite;
		acx_base = apu_acx1;
	} else if (acx_id == NCX) {
		rpc_lite_base = apu_ncx_rpc_lite;
		acx_base = apu_ncx;
	} else {
		return -ENODEV;
	}

	if (rpc_lite_base < 0 || acx_base < 0)
		return -EINVAL;

	dev_info(dev, "%s ctl p1:%d p2:%d\n",
			__func__, rpc_lite_base, acx_base);

	/* TINFO="Enable AFC enable" */
	apu_setl((0x1 << 16), papw->regs[rpc_lite_base] + APU_RPC_TOP_SEL_1);

	/* wake acx rpc lite */
	apu_writel(0x00000100, papw->regs[rpc_lite_base] + APU_RPC_TOP_CON);
	ret = readl_relaxed_poll_timeout_atomic(
			(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL), 50, 10000);

	/* polling FSM @RPC-lite to ensure RPC is in on/off stage */
	ret |= readl_relaxed_poll_timeout_atomic(
			(papw->regs[rpc_lite_base] + APU_RPC_STATUS),
			val, (val & (0x1 << 29)), 50, 10000);
	if (ret) {
		pr_info("%s wake up acx%d_rpc fail, ret %d\n",
				__func__, acx_id, ret);
		goto out;
	}

	dev_info(dev, "%s ACX%d APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
		__func__, acx_id,
		(u32)(papw->phy_addr[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
		readl(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY));

	dev_info(dev, "%s ACX%d APU_ACX_CONN_CG_CON 0x%x = 0x%x\n",
		__func__, acx_id,
		(u32)(papw->phy_addr[acx_base] + APU_ACX_CONN_CG_CON),
		readl(papw->regs[acx_base] + APU_ACX_CONN_CG_CON));

	/* clear acx0/1 CGs */
	if (acx_id == NCX)
		apu_writel(0xFFFFFFFF, papw->regs[acx_base] + APU_NCX_CONN_CG_CLR);
	else
		apu_writel(0xFFFFFFFF, papw->regs[acx_base] + APU_ACX_CONN_CG_CLR);

	dev_info(dev, "%s ACX%d APU_ACX_CONN_CG_CON 0x%x = 0x%x\n",
		__func__, acx_id,
		(u32)(papw->phy_addr[acx_base] + APU_ACX_CONN_CG_CON),
		readl(papw->regs[acx_base] + APU_ACX_CONN_CG_CON));
out:
	return ret;
}

static int __apu_off_rpc_acx(struct device *dev, enum t_acx_id acx_id)
{
	int ret = 0, val = 0;
	enum apupw_reg rpc_lite_base;
	enum apupw_reg acx_base;
	uint32_t rpc_status;

	if (acx_id == ACX0) {
		rpc_lite_base = apu_acx0_rpc_lite;
		acx_base = apu_acx0;
	} else if (acx_id == ACX1) {
		rpc_lite_base = apu_acx1_rpc_lite;
		acx_base = apu_acx1;
	} else if (acx_id == NCX) {
		rpc_lite_base = apu_ncx_rpc_lite;
		acx_base = apu_ncx;
	} else {
		return -ENODEV;
	}

	if (rpc_lite_base < 0 || acx_base < 0)
		return -EINVAL;

	rpc_status = apu_readl(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY);
	/* if ACX0/ACX1/NCX alread off, just return */
	if (!(rpc_status & 0x1UL))
		goto out;

	/* Clear wakeup signal */
	apu_writel(1 << 12, papw->regs[rpc_lite_base] + APU_RPC_TOP_CON);

	/* Sleep request */
	apu_writel(1, papw->regs[rpc_lite_base] + APU_RPC_TOP_CON);
	ret = readl_relaxed_poll_timeout_atomic(
			(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
			val, !(val & 0x1UL), 50, 10000);

	if (ret)
		dev_info(dev, "%s timeout val = 0x%x, ret %d\n", __func__, val, ret);

	dev_info(dev, "%s ACX%d APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
		__func__, acx_id,
		(u32)(papw->phy_addr[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
		readl(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY));

	dev_info(dev, "%s ACX%d APU_ACX_CONN_CG_CON 0x%x = 0x%x\n",
		__func__, acx_id,
		(u32)(papw->phy_addr[acx_base] + APU_ACX_CONN_CG_CON),
		readl(papw->regs[acx_base] + APU_ACX_CONN_CG_CON));
out:
	return ret;
}

static int __apu_pwr_ctl_acx_engines(struct device *dev,
		enum t_acx_id acx_id, enum t_dev_id dev_id, int pwron)
{
	int ret = 0, val = 0;
	enum apupw_reg rpc_lite_base;
	enum apupw_reg acx_base;
	uint32_t dev_mtcmos_ctl, dev_cg_con, dev_cg_clr;
	uint32_t dev_mtcmos_chk;

	if (acx_id == ACX0) {
		rpc_lite_base = apu_acx0_rpc_lite;
		acx_base = apu_acx0;
	} else if (acx_id == ACX1) {
		rpc_lite_base = apu_acx1_rpc_lite;
		acx_base = apu_acx1;
	} else if (acx_id == NCX) {
		rpc_lite_base = apu_ncx_rpc_lite;
		acx_base = apu_ncx;
	} else {
		return -ENODEV;
	}

	if (rpc_lite_base < 0 || acx_base < 0)
		return -EINVAL;

	switch (dev_id) {
	case VPU0:
		dev_mtcmos_ctl = 0x00000012;
		dev_mtcmos_chk = 0x4UL;
		dev_cg_con = APU_ACX_MVPU_CG_CON;
		dev_cg_clr = APU_ACX_MVPU_CG_CLR;
		break;
	case DLA0:
		dev_mtcmos_ctl = 0x00000016;
		dev_mtcmos_chk = 0x40UL;
		dev_cg_con = APU_ACX_MDLA0_CG_CON;
		dev_cg_clr = APU_ACX_MDLA0_CG_CLR;
		break;
	case DLA1:
		dev_mtcmos_ctl = 0x00000017;
		dev_mtcmos_chk = 0x80UL;
		dev_cg_con = APU_ACX_MDLA1_CG_CON;
		dev_cg_clr = APU_ACX_MDLA1_CG_CLR;
		break;
	case APS:
		dev_mtcmos_ctl = 0x00000016;
		dev_mtcmos_chk = 0x40UL;
		dev_cg_con = APU_NCX_APS_CG_CON;
		dev_cg_clr = APU_NCX_APS_CG_CLR;
		break;
	default:
		goto out;
	}

	dev_info(dev, "%s ctl p1:%d p2:%d p3:0x%x p4:0x%x p5:0x%x p6:0x%x\n",
		__func__, rpc_lite_base, acx_base,
		dev_mtcmos_ctl, dev_mtcmos_chk, dev_cg_con, dev_cg_clr);

	if (pwron) {
		/* config acx rpc lite */
		apu_writel(dev_mtcmos_ctl,
				papw->regs[rpc_lite_base] + APU_RPC_SW_FIFO_WE);
		ret = readl_relaxed_poll_timeout_atomic(
				(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
				val, (val & dev_mtcmos_chk) == dev_mtcmos_chk, 50, 200);
		if (ret) {
			pr_info("%s on acx%d_rpc 0x%x fail, ret %d\n",
					__func__, acx_id, dev_mtcmos_ctl, ret);
			goto out;
		}
		dev_info(dev, "%s before engine on ACX%d dev%d CG_CON 0x%x = 0x%x\n",
			__func__, acx_id, dev_id,
			(u32)(papw->phy_addr[acx_base] + dev_cg_con),
			readl(papw->regs[acx_base] + dev_cg_con));

		apu_writel(0xFFFFFFFF, papw->regs[acx_base] + dev_cg_clr);
	} else {
		/* config acx rpc lite */
		apu_writel((dev_mtcmos_ctl & 0xFFFFFFEF),
				papw->regs[rpc_lite_base] + APU_RPC_SW_FIFO_WE);
		ret = readl_relaxed_poll_timeout_atomic(
				(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
				val, (val & dev_mtcmos_chk) != dev_mtcmos_chk, 50, 200);
		if (ret) {
			pr_info("%s off acx%d_rpc 0x%x fail, ret %d\n",
					__func__, acx_id, dev_mtcmos_ctl, ret);
			goto out;
		}
	}
	dev_info(dev, "%s ACX%d %s APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
		__func__, acx_id, pwron ? "on" : "off",
		(u32)(papw->phy_addr[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
		readl(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY));


	dev_info(dev, "%s ACX%d dev%d CG_CON 0x%x = 0x%x\n",
		__func__, acx_id, dev_id,
		(u32)(papw->phy_addr[acx_base] + dev_cg_con),
		readl(papw->regs[acx_base] + dev_cg_con));

out:
	return ret;
}

static void __apu_aoc_init(void)
{
	pr_info("AOC init %s %d ++\n", __func__, __LINE__);

	/* 1. Manually disable Buck els enable @SOC, vapu_ext_buck_iso */
	if (papw->env == AO) {
		apu_setl((0x1 << 4), papw->regs[sys_spm] + 0xF80);
		apu_clearl((0x1 << 1), papw->regs[sys_spm] + 0x414);
	}

	/*
	 * 2. Vsram AO clock enable
	 */
	apu_writel(0x00000001, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_CONFIG);
	udelay(1);

	/*
	 * 3. Switch APU AOC control signal from SW register to HW path (RPC)
	 */
	apu_setl(1 << 8, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	apu_setl(1 << 11, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	apu_setl(1 << 13, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	/* ---------------------------------------------------------------*/
	apu_clearl(1 << 8, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	apu_clearl(1 << 11, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	apu_clearl(1 << 13, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	// 4. Roll back to APU Buck on stage
	//  The following setting need to in order
	//  and wait 1uS before setup next control signal
	// APU_BUCK_ELS_EN
	apu_writel(0x00000800, papw->regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(1);

	// APU_BUCK_RST_B
	apu_writel(0x00001000, papw->regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(1);

	// APU_BUCK_PROT_REQ
	apu_writel(0x00008000, papw->regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(1);

	// SRAM_AOC_ISO
	apu_writel(0x00000080, papw->regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(1);

	/* DPSW_AOC_ISO */
	apu_writel(1 << 30, papw->regs[apu_rpc] + APU_RPC_HW_CON1);
	udelay(1);

	/* PLL_AOC_ISO_EN */
	apu_writel(1 << 9, papw->regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(1);
	pr_info("AOC init %s %d --\n", __func__, __LINE__);
}

static int init_hw_setting(struct device *dev)
{
	__apu_aoc_init();
	__apu_pcu_init();
	__apu_rpc_init();
	if (papw->env != FPGA) {
		__apu_rpclite_init_all();
	} else {
		if (fpga_type == 1) {
			__apu_rpclite_init(ACX0);
		} else if (fpga_type == 2) {
			__apu_rpclite_init(ACX0);
			__apu_rpclite_init(ACX1);
		} else if (fpga_type == 3) {
			__apu_rpclite_init(NCX);
		}
	}

	/*
	 * 1. clear all are entries and enable cores
	 * 2. if use ce_rpc, set up it
	 */
	__apu_are_init(dev);
	if (papw->env != FPGA) {
		__apu_pll_init();
		__apu_acc_init();
	}
	__apu_buck_off_cfg();
	return 0;
}

int mt6985_all_on(struct platform_device *pdev, struct apu_power *g_papw)
{
	papw = g_papw;
	if (papw->env == AO)
		init_plat_pwr_res(pdev);

	init_hw_setting(&pdev->dev);

	/* wake up RCX */
	if (__apu_wake_rpc_rcx(&pdev->dev)) {
		aputop_dump_reg(apu_pll, 0xa0c, 0x10);
		/* dump RCX_AO content */
		aputop_dump_reg(apu_are, 0,
			(0x40 + 4*ARE_ENTRIES(RCX_AO_BEGIN, RCX_AO_END)));
		check_if_rpc_alive();
		return -EIO;
	}

	aputop_dump_reg(apu_rpc, 0x0, 0x50);
	pm_runtime_get_sync(&pdev->dev);
	if (papw->env == AO) {
		/* wake up ACX/NCX */
		__apu_wake_rpc_acx(&pdev->dev, ACX0);
		__apu_wake_rpc_acx(&pdev->dev, ACX1);
		__apu_wake_rpc_acx(&pdev->dev, NCX);

		/* wake up Engines */
		__apu_engine_acc_on();
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, DLA0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, DLA1, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, VPU0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, DLA0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, DLA1, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, VPU0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, NCX, APS, 1);
	} else {
		switch (fpga_type) {
		default:
		case 0: // do not power on
			pr_info("%s only RCX power on\n", __func__);
			break;
		case 1: //1.ACX0 MDLA0 + ACX0 MDLA1
			__apu_wake_rpc_acx(&pdev->dev, ACX0);
			__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, DLA0, 1);
			__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, DLA1, 1);
			break;
		case 2: //2.ACX0 MVPU + ACX1 MVPU
			__apu_wake_rpc_acx(&pdev->dev, ACX0);
			__apu_wake_rpc_acx(&pdev->dev, ACX1);
			__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, VPU0, 1);
			__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, VPU0, 1);
			break;
		case 3: //NCX + ACX0 (empty 2MDLA & MVPU)
			__apu_wake_rpc_acx(&pdev->dev, NCX);
			__apu_pwr_ctl_acx_engines(&pdev->dev, NCX, APS, 1);
			break;
		}
	}

	return 0;
}

void mt6985_all_off(struct platform_device *pdev)
{
	if (papw->env == AO) {
		/* turn off Engines */
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, DLA0, 0);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, DLA1, 0);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, VPU0, 0);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, DLA0, 0);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, DLA1, 0);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, VPU0, 0);
		__apu_pwr_ctl_acx_engines(&pdev->dev, NCX, APS, 0);

		/* turn off ACX/NCX */
		__apu_off_rpc_acx(&pdev->dev, ACX0);
		__apu_off_rpc_acx(&pdev->dev, ACX1);
		__apu_off_rpc_acx(&pdev->dev, NCX);

	} else {
		switch (fpga_type) {
		default:
		case 0: // do not power on
			pr_info("%s bypass pre-power-ON\n", __func__);
			break;
		case 1: //1.ACX0 MDLA0 + ACX0 MDLA1
			__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, DLA0, 0);
			__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, DLA1, 0);
			__apu_off_rpc_acx(&pdev->dev, ACX0);
			break;
		case 2: //2.ACX0 MVPU + ACX1 MVPU
			__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, VPU0, 0);
			__apu_off_rpc_acx(&pdev->dev, ACX0);
			__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, VPU0, 0);
			__apu_off_rpc_acx(&pdev->dev, ACX1);
			break;
		case 3: //NCX + ACX0 (empty 2MDLA & MVPU)
			__apu_pwr_ctl_acx_engines(&pdev->dev, NCX, APS, 0);
			__apu_off_rpc_acx(&pdev->dev, NCX);
			break;
		}
	}
	/* turn off RCX */
	__apu_off_rpc_rcx(&pdev->dev);
	pm_runtime_put_sync(&pdev->dev);
	if (papw->env == AO)
		destroy_plat_pwr_res();
}
