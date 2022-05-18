// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/clk.h>
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

#include "apusys_secure.h"
#include "aputop_rpmsg.h"
#include "apu_top.h"
#include "mt6983_apupwr.h"
#include "mt6983_apupwr_prot.h"

#define LOCAL_DBG	(1)
#define RPC_ALIVE_DBG	(0)

/* Below reg_name has to 1-1 mapping DTS's name */
static const char *reg_name[APUPW_MAX_REGS] = {
	"sys_vlp", "sys_spm", "apu_rcx", "apu_vcore", "apu_md32_mbox",
	"apu_rpc", "apu_pcu", "apu_ao_ctl", "apu_pll", "apu_acc",
	"apu_are0", "apu_are1", "apu_are2",
	"apu_acx0", "apu_acx0_rpc_lite", "apu_acx1", "apu_acx1_rpc_lite",
};

static struct apu_power apupw;
static uint32_t g_opp_cfg_acx0;
static uint32_t g_opp_cfg_acx1;

static void aputop_dump_pwr_res(void);
static void aputop_dump_pwr_reg(struct device *dev);
#if APUPW_DUMP_FROM_APMCU
static void are_dump_config(int are_hw);
static void are_dump_entry(int are_hw);
#endif
/* regulator id */
static struct regulator *vapu_reg_id;
static struct regulator *vcore_reg_id;
static struct regulator *vsram_reg_id;

/* apu_top preclk */
static struct clk *clk_top_dsp_sel;		/* CONN */
static struct clk *clk_top_dsp1_sel;
static struct clk *clk_top_dsp2_sel;
static struct clk *clk_top_dsp3_sel;
static struct clk *clk_top_dsp4_sel;
static struct clk *clk_top_dsp5_sel;
static struct clk *clk_top_dsp6_sel;
static struct clk *clk_top_dsp7_sel;
static struct clk *clk_top_ipu_if_sel;		/* VCORE */

static uint32_t apusys_pwr_smc_call(struct device *dev, uint32_t smc_id,
		uint32_t a2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL, smc_id,
			a2, 0, 0, 0, 0, 0, &res);
	if (((int) res.a0) < 0)
		dev_info(dev, "%s: smc call %d return error(%d)\n",
				__func__,
				smc_id, res.a0);
	return res.a0;
}

static void aputop_check_pwr_data(void)
{
	uint32_t magicNum = 0x11223344;
	uint32_t clearNum = 0x55667788;
	uint32_t regValue = 0x0;

	apu_writel(magicNum, apupw.regs[apu_md32_mbox] + PWR_DBG_REG);
	regValue = apu_readl(apupw.regs[apu_md32_mbox] + PWR_DBG_REG);
	pr_info("%s mbox_dbg_reg readback = 0x%08x\n", __func__, regValue);
	apu_writel(clearNum, apupw.regs[apu_md32_mbox] + PWR_DBG_REG);
}

static void aputop_dump_rpc_data(void)
{
	char buf[32];
	int ret = 0;

	// reg dump for RPC
	memset(buf, 0, sizeof(buf));
	ret = snprintf(buf, 32, "phys 0x%08x: ",
			(u32)(apupw.phy_addr[apu_rpc]));

	if (ret > 0) {
		print_hex_dump(KERN_INFO, buf, DUMP_PREFIX_OFFSET, 16, 4,
			apupw.regs[apu_rpc], 0x20, true);
	} else {
		pr_info("%s %d snprintf fail %d\n", __func__, __LINE__, ret);
	}
}

static void aputop_dump_pll_data(void)
{
	// need to 1-1 in order mapping with array in __apu_pll_init func
	uint32_t pll_base_arr[] = {MNOC_PLL_BASE, UP_PLL_BASE};
	uint32_t pll_offset_arr[] = {
				PLL1UPLL_FHCTL_HP_EN, PLL1UPLL_FHCTL_RST_CON,
				PLL1UPLL_FHCTL_CLK_CON, PLL1UPLL_FHCTL0_CFG,
				PLL1U_PLL1_CON1, PLL1UPLL_FHCTL0_DDS};
	int base_arr_size = sizeof(pll_base_arr) / sizeof(uint32_t);
	int offset_arr_size = sizeof(pll_offset_arr) / sizeof(uint32_t);
	int pll_idx;
	int ofs_idx;
	uint32_t phy_addr = 0x0;
	char buf[256];
	int ret = 0;

	for (pll_idx = 0 ; pll_idx < base_arr_size ; pll_idx++) {

		memset(buf, 0, sizeof(buf));

		for (ofs_idx = 0 ; ofs_idx < offset_arr_size ; ofs_idx++) {

			phy_addr = apupw.phy_addr[apu_pll] +
				pll_base_arr[pll_idx] +
				pll_offset_arr[ofs_idx];

			ret = snprintf(buf + strlen(buf),
					sizeof(buf) - strlen(buf),
					" 0x%08x",
					apu_readl(apupw.regs[apu_pll] +
						pll_base_arr[pll_idx] +
						pll_offset_arr[ofs_idx]));
			if (ret <= 0)
				break;
		}

		if (ret <= 0)
			break;

		pr_info("%s pll_base:0x%08x = %s\n", __func__,
				apupw.phy_addr[apu_pll] + pll_base_arr[pll_idx],
				buf);
	}
}

static int check_if_rpc_alive(void)
{
#if RPC_ALIVE_DBG
	unsigned int regValue = 0x0;
	int bit_offset = 26; // [31:26] is reserved for debug

	regValue = apu_readl(apupw.regs[apu_rpc] + APU_RPC_TOP_SEL);
	pr_info("%s , before: APU_RPC_TOP_SEL = 0x%x\n", __func__, regValue);
	regValue |= (0x3a << bit_offset);
	apu_writel(regValue, apupw.regs[apu_rpc] + APU_RPC_TOP_SEL);

	regValue = 0x0;
	regValue = apu_readl(apupw.regs[apu_rpc] + APU_RPC_TOP_SEL);
	pr_info("%s , after: APU_RPC_TOP_SEL = 0x%x\n", __func__, regValue);

	apu_clearl((BIT(26) | BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31)),
					apupw.regs[apu_rpc] + APU_RPC_TOP_SEL);

	return ((regValue >> bit_offset) & 0x3f) == 0x3a ? 1 : 0;
#endif
	return 1;
}

#if APU_POWER_INIT
// WARNING: can not call this API after acc initial or you may cause bus hang !
static void dump_rpc_lite_reg(int line)
{
	pr_info("%s ln_%d acx%d APU_RPC_TOP_SEL=0x%08x\n",
			__func__, line, 0,
			apu_readl(apupw.regs[apu_acx0_rpc_lite]
				+ APU_RPC_TOP_SEL));

	if (CLUSTER_NUM == 2) {
		pr_info("%s ln_%d acx%d APU_RPC_TOP_SEL=0x%08x\n",
			__func__, line, 1,
			apu_readl(apupw.regs[apu_acx1_rpc_lite]
				+ APU_RPC_TOP_SEL));
	}
}
#endif

static int init_plat_pwr_res(struct platform_device *pdev)
{
	int ret_clk = 0, ret = 0;

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

	// devm_clk_get , not real prepare_clk
	PREPARE_CLK(clk_top_dsp_sel);
	PREPARE_CLK(clk_top_dsp1_sel);
	PREPARE_CLK(clk_top_dsp2_sel);
	PREPARE_CLK(clk_top_dsp3_sel);
	PREPARE_CLK(clk_top_dsp4_sel);
	PREPARE_CLK(clk_top_dsp5_sel);
	PREPARE_CLK(clk_top_dsp6_sel);
	PREPARE_CLK(clk_top_dsp7_sel);
	PREPARE_CLK(clk_top_ipu_if_sel);
	if (ret_clk < 0)
		return ret_clk;

	pr_info("%s %d --\n", __func__, __LINE__);

	return 0;
}

static void destroy_plat_pwr_res(void)
{
	UNPREPARE_CLK(clk_top_dsp_sel);
	UNPREPARE_CLK(clk_top_dsp1_sel);
	UNPREPARE_CLK(clk_top_dsp2_sel);
	UNPREPARE_CLK(clk_top_dsp3_sel);
	UNPREPARE_CLK(clk_top_dsp4_sel);
	UNPREPARE_CLK(clk_top_dsp5_sel);
	UNPREPARE_CLK(clk_top_dsp6_sel);
	UNPREPARE_CLK(clk_top_dsp7_sel);
	UNPREPARE_CLK(clk_top_ipu_if_sel);

	regulator_put(vapu_reg_id);
	regulator_put(vcore_reg_id);
	regulator_put(vsram_reg_id);
	vapu_reg_id = NULL;
	vcore_reg_id = NULL;
	vsram_reg_id = NULL;
}

#if (ENABLE_SOC_CLK_MUX || ENABLE_SW_BUCK_CTL)
static void plt_pwr_res_ctl(int enable)
{
#if ENABLE_SW_BUCK_CTL
	int ret;
#endif
#if ENABLE_SOC_CLK_MUX
	int ret_clk = 0;
#endif
	pr_info("%s %d ++\n", __func__, __LINE__);

	if (enable) {
#if ENABLE_SW_BUCK_CTL
		ret = regulator_enable(vapu_reg_id);
		if (ret < 0)
			pr_info("%s fail enable vapu : %d\n", __func__, ret);

		apu_writel(
			apu_readl(apupw.regs[sys_spm] + APUSYS_BUCK_ISOLATION)
			& ~(0x00000021),
			apupw.regs[sys_spm] + APUSYS_BUCK_ISOLATION);
#else
		pr_info("%s skip enable regulator since HW auto\n", __func__);
#endif

#if ENABLE_SOC_CLK_MUX
		// clk_prepare_enable
		ENABLE_CLK(clk_top_ipu_if_sel);
		ENABLE_CLK(clk_top_dsp_sel);
		ENABLE_CLK(clk_top_dsp1_sel);
		ENABLE_CLK(clk_top_dsp2_sel);
		ENABLE_CLK(clk_top_dsp3_sel);
		ENABLE_CLK(clk_top_dsp4_sel);
		ENABLE_CLK(clk_top_dsp5_sel);
		ENABLE_CLK(clk_top_dsp6_sel);
		ENABLE_CLK(clk_top_dsp7_sel);
		if (ret_clk < 0)
			pr_info("%s fail enable clk : %d\n", __func__, ret_clk);
#else
		pr_info("%s skip enable soc PLL since HW auto\n", __func__);
#endif

	} else {

#if ENABLE_SOC_CLK_MUX
		// clk_disable_unprepare
		DISABLE_CLK(clk_top_dsp7_sel);
		DISABLE_CLK(clk_top_dsp6_sel);
		DISABLE_CLK(clk_top_dsp5_sel);
		DISABLE_CLK(clk_top_dsp4_sel);
		DISABLE_CLK(clk_top_dsp3_sel);
		DISABLE_CLK(clk_top_dsp2_sel);
		DISABLE_CLK(clk_top_dsp1_sel);
		DISABLE_CLK(clk_top_dsp_sel);
		DISABLE_CLK(clk_top_ipu_if_sel);
#else
		pr_info("%s skip disable soc PLL since HW auto\n", __func__);
#endif

#if ENABLE_SW_BUCK_CTL
		apu_writel(
			apu_readl(apupw.regs[sys_spm] + APUSYS_BUCK_ISOLATION)
			| 0x00000021,
			apupw.regs[sys_spm] + APUSYS_BUCK_ISOLATION);

		ret = regulator_disable(vapu_reg_id);
		if (ret < 0)
			pr_info("%s fail disable vapu : %d\n", __func__, ret);
#else
		pr_info("%s skip disable regulator since HW auto\n", __func__);
#endif
	}

	pr_info("%s %d --\n", __func__, __LINE__);
}
#endif

#if APU_POWER_INIT
#if !APU_PWR_SOC_PATH
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

/*
 * default PLL output clock rate:
 * MVPU	540 MHz
 * MDLA	485 MHz
 * MNOC	388 MHz
 * uP	485 MHz
 */
static void __apu_pll_init(void)
{
	// need to 1-1 in order mapping to these two array
	uint32_t pll_base_arr[] = {MNOC_PLL_BASE, UP_PLL_BASE,
					MDLA_PLL_BASE, MVPU_PLL_BASE};
	int32_t pll_freq_out[] = {1480, 1850, 1850, 2040}; // MHz, then div 2
	uint32_t pcw_val, posdiv_val;
	int pll_arr_size = sizeof(pll_base_arr) / sizeof(uint32_t);
	int pll_idx;

	pr_info("PLL init %s %d ++\n", __func__, __LINE__);

	// Step4. Initial PLL setting

	for (pll_idx = 0 ; pll_idx < pll_arr_size ; pll_idx++) {
		// PCW value always from hopping function: ofs 0x100
		apu_setl(0x1 << 0, apupw.regs[apu_pll]
			+ pll_base_arr[pll_idx] + PLL1UPLL_FHCTL_HP_EN);
#if LOCAL_DBG
		pr_info("%s dbg setl 0x%08x to 0x%08x\n", __func__,
			0x1 << 0,
			apupw.phy_addr[apu_pll] +
			pll_base_arr[pll_idx] + PLL1UPLL_FHCTL_HP_EN);
#endif
		// Hopping function reset release: ofs 0x10C
		apu_setl(0x1 << 0, apupw.regs[apu_pll]
			+ pll_base_arr[pll_idx] + PLL1UPLL_FHCTL_RST_CON);
#if LOCAL_DBG
		pr_info("%s dbg setl 0x%08x to 0x%08x\n", __func__,
			0x1 << 0,
			apupw.phy_addr[apu_pll] +
			pll_base_arr[pll_idx] + PLL1UPLL_FHCTL_RST_CON);
#endif
		// Hopping function clock enable: ofs 0x108
		apu_setl(0x1 << 0, apupw.regs[apu_pll]
			+ pll_base_arr[pll_idx] + PLL1UPLL_FHCTL_CLK_CON);
#if LOCAL_DBG
		pr_info("%s dbg setl 0x%08x to 0x%08x\n", __func__,
			0x1 << 0,
			apupw.phy_addr[apu_pll] +
			pll_base_arr[pll_idx] + PLL1UPLL_FHCTL_CLK_CON);
#endif
		// Hopping function enable: ofs 0x114
		apu_setl((0x1 << 0) | (0x1 << 2), apupw.regs[apu_pll]
			+ pll_base_arr[pll_idx] + PLL1UPLL_FHCTL0_CFG);
#if LOCAL_DBG
		pr_info("%s dbg setl 0x%08x to 0x%08x\n", __func__,
			(0x1 << 0) | (0x1 << 2),
			apupw.phy_addr[apu_pll] +
			pll_base_arr[pll_idx] + PLL1UPLL_FHCTL0_CFG);
#endif
		posdiv_val = 0;
		pcw_val = 0;
		get_pll_pcw(pll_freq_out[pll_idx], &posdiv_val, &pcw_val);

		// POSTDIV: ofs 0x000C , [26:24] RG_PLL_POSDIV
		// 3'b000: /1 , 3'b001: /2 , 3'b010: /4
		// 3'b011: /8 , 3'b100: /16
		apu_clearl(((0x1 << 26) | (0x1 << 25) | (0x1 << 24)),
			apupw.regs[apu_pll] + pll_base_arr[pll_idx] + PLL1U_PLL1_CON1);

		apu_setl(((0x1 << 31) | (posdiv_val << 24) | pcw_val), apupw.regs[apu_pll]
			+ pll_base_arr[pll_idx] + PLL1U_PLL1_CON1);
#if LOCAL_DBG
		pr_info("%s dbg setl 0x%08x to 0x%08x\n", __func__,
			((0x1 << 31) | (posdiv_val << 24) | pcw_val),
			apupw.phy_addr[apu_pll] +
			pll_base_arr[pll_idx] + PLL1U_PLL1_CON1);
#endif
		// PCW register: ofs 0x011C
		// [31] FHCTL0_PLL_TGL_ORG
		// [21:0] FHCTL0_PLL_ORG set to PCW value
		apu_writel(((0x1 << 31) | pcw_val),
			apupw.regs[apu_pll]
			+ pll_base_arr[pll_idx] + PLL1UPLL_FHCTL0_DDS);
#if LOCAL_DBG
		pr_info("%s dbg writel 0x%08x to 0x%08x\n", __func__,
			((0x1 << 31) | pcw_val),
			apupw.phy_addr[apu_pll] +
			pll_base_arr[pll_idx] + PLL1UPLL_FHCTL0_DDS);
#endif
	}

	pr_info("PLL init %s %d --\n", __func__, __LINE__);
}

static void __apu_acc_init(void)
{
	char buf[32];

	pr_info("ACC init %s %d ++\n", __func__, __LINE__);

	// Step6. Initial ACC setting (@ACC)

	/* mnoc clk setting */
	pr_info("mnoc clk setting %s %d\n", __func__, __LINE__);
	// CGEN_SOC
	apu_writel(0x00000004, apupw.regs[apu_acc] + APU_ACC_CONFG_CLR0);
	// HW_CTRL_EN, SEL_APU_DIV2
	apu_writel(0x00008400, apupw.regs[apu_acc] + APU_ACC_CONFG_SET0);

	/* iommu clk setting */
	pr_info("iommu clk setting %s %d\n", __func__, __LINE__);
	// CGEN_SOC
	apu_writel(0x00000004, apupw.regs[apu_acc] + APU_ACC_CONFG_CLR1);
	// HW_CTRL_EN, SEL_APU_DIV2
	apu_writel(0x00008400, apupw.regs[apu_acc] + APU_ACC_CONFG_SET1);

	/* mvpu clk setting */
	pr_info("mvpu clk setting %s %d\n", __func__, __LINE__);
	// CGEN_SOC
	apu_writel(0x00000004, apupw.regs[apu_acc] + APU_ACC_CONFG_CLR2);
	// HW_CTRL_EN, SEL_APU_DIV2
	apu_writel(0x00008400, apupw.regs[apu_acc] + APU_ACC_CONFG_SET2);
	// CLK_REQ_SW_EN
	apu_writel(0x00000100, apupw.regs[apu_acc] + APU_ACC_AUTO_CTRL_SET2);

	/* mdla clk setting */
	pr_info("mdla clk setting %s %d\n", __func__, __LINE__);
	// CGEN_SOC
	apu_writel(0x00000004, apupw.regs[apu_acc] + APU_ACC_CONFG_CLR3);
	// HW_CTRL_EN, SEL_APU_DIV2
	apu_writel(0x00008400, apupw.regs[apu_acc] + APU_ACC_CONFG_SET3);
	// CLK_REQ_SW_EN
	apu_writel(0x00000100, apupw.regs[apu_acc] + APU_ACC_AUTO_CTRL_SET3);

	/* clk invert setting */
	pr_info("clk invert setting %s %d\n", __func__, __LINE__);
	// MVPU1_CLK_INV_EN MVPU3_CLK_INV_EN MVPU5_CLK_INV_EN
	// MDLA1_CLK_INV_EN MDLA3_CLK_INV_EN
	// MDLA5_CLK_INV_EN MDLA7_CLK_INV_EN
	apu_writel(0x0000AAA8, apupw.regs[apu_acc] + APU_ACC_CLK_INV_EN_SET);

	memset(buf, 0, sizeof(buf));
	snprintf(buf, 32, "phys 0x%08x: ", (u32)(apupw.phy_addr[apu_acc]));
	print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
			apupw.regs[apu_acc], 0x100, true);

	pr_info("ACC init %s %d --\n", __func__, __LINE__);
}
#endif // end of not APU_PWR_SOC_PATH

static void __apu_buck_off_cfg(void)
{
	// Step11. Roll back to Buck off stage

	pr_info("%s %d ++\n", __func__, __LINE__);

	// a. Setup Buck control signal
	//	The following setting need to in order,
	//	and wait 1uS before setup next control signal
	// APU_BUCK_PROT_REQ
	apu_writel(0x00004000, apupw.regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(10);
	// SRAM_AOC_LHENB
	apu_writel(0x00000010, apupw.regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(10);
	// SRAM_AOC_ISO
	apu_writel(0x00000040, apupw.regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(10);
	// APU_BUCK_ELS_EN
	apu_writel(0x00000400, apupw.regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(10);
	// APU_BUCK_RST_B
	apu_writel(0x00002000, apupw.regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(10);

	// b. Manually turn off Buck (by configuring register in PMIC)
	// move to probe last line
	pr_info("%s %d --\n", __func__, __LINE__);
}
#endif // APU_POWER_INIT

#if DEBUG_DUMP_REG
static void aputop_dump_all_reg(void)
{
	int idx = 0;
	char buf[32];

	for (idx = 0; idx < APUPW_MAX_REGS; idx++) {
		memset(buf, 0, sizeof(buf));
		snprintf(buf, 32, "phys 0x%08x ", (u32)(apupw.phy_addr[idx]));
		print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
				apupw.regs[idx], 0x1000, true);
	}
}
#endif

void mt6983_apu_dump_rpc_status(enum t_acx_id id, struct rpc_status_dump *dump)
{
	uint32_t status1 = 0x0;
	uint32_t status2 = 0x0;
	uint32_t status3 = 0x0;

	if (id == ACX0) {
		status1 = apu_readl(apupw.regs[apu_acx0_rpc_lite]
				+ APU_RPC_INTF_PWR_RDY);
		status2 = apu_readl(apupw.regs[apu_acx0]
				+ APU_ACX_CONN_CG_CON);
		pr_info(
		"%s ACX0 APU_RPC_INTF_PWR_RDY:0x%08x APU_ACX_CONN_CG_CON:0x%08x\n",
				__func__, status1, status2);

	} else if (id == ACX1) {
		status1 = apu_readl(apupw.regs[apu_acx1_rpc_lite]
				+ APU_RPC_INTF_PWR_RDY);
		status2 = apu_readl(apupw.regs[apu_acx1]
				+ APU_ACX_CONN_CG_CON);
		pr_info(
		"%s ACX1 APU_RPC_INTF_PWR_RDY:0x%08x APU_ACX_CONN_CG_CON:0x%08x\n",
				__func__, status1, status2);

	} else {
		status1 = apu_readl(apupw.regs[apu_rpc]
				+ APU_RPC_INTF_PWR_RDY);
		status2 = apu_readl(apupw.regs[apu_vcore]
				+ APUSYS_VCORE_CG_CON);
		status3 = apu_readl(apupw.regs[apu_rcx]
				+ APU_RCX_CG_CON);
		pr_info(
		"%s RCX APU_RPC_INTF_PWR_RDY:0x%08x APU_VCORE_CG_CON:0x%08x APU_RCX_CG_CON:0x%08x\n",
				__func__, status1, status2, status3);
		/*
		 * print_hex_dump(KERN_ERR, "rpc: ", DUMP_PREFIX_OFFSET,
		 *              16, 4, apupw.regs[apu_rpc], 0x100, 1);
		 */
	}

	if (!IS_ERR_OR_NULL(dump)) {
		dump->rpc_reg_status = status1;
		dump->conn_reg_status = status2;
		if (id == RCX)
			dump->vcore_reg_status = status3;
	}
}

#if APU_POWER_INIT
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

	pr_info("PCU init %s %d ++\n", __func__, __LINE__);

	// auto buck enable
	apu_writel((0x1 << 16), apupw.regs[apu_pcu] + APU_PCUTOP_CTRL_SET);

	// Step1. enable auto buck on/off function of command0
	// [0]: cmd0 enable auto ON, [4]: cmd0 enable auto OFF
	apu_writel(0x11, apupw.regs[apu_pcu] + APU_PCU_BUCK_STEP_SEL);

	// Step2. fill-in command0 for vapu auto buck ON
	apu_writel((en_set_offset << 16) | (0x1 << en_shift),
			apupw.regs[apu_pcu] + APU_PCU_BUCK_ON_DAT0_L);
	apu_writel((slave_id << 4) | (pmif_id << 3) | cmd_op_w,
			apupw.regs[apu_pcu] + APU_PCU_BUCK_ON_DAT0_H);

	// APU_PCU_BUCK_ON_DAT0_L=0x02410040
	// APU_PCU_BUCK_ON_DAT0_H=0x00000057

	pr_info("%s APU_PCU_BUCK_ON_DAT0_L=0x%08x\n", __func__,
		apu_readl(apupw.regs[apu_pcu] + APU_PCU_BUCK_ON_DAT0_L));
	pr_info("%s APU_PCU_BUCK_ON_DAT0_H=0x%08x\n", __func__,
		apu_readl(apupw.regs[apu_pcu] + APU_PCU_BUCK_ON_DAT0_H));

	// Step3. fill-in command0 for vapu auto buck OFF
	apu_writel((en_clr_offset << 16) | (0x1 << en_shift),
			apupw.regs[apu_pcu] + APU_PCU_BUCK_OFF_DAT0_L);
	apu_writel((slave_id << 4) | (pmif_id << 3) | cmd_op_w,
			apupw.regs[apu_pcu] + APU_PCU_BUCK_OFF_DAT0_H);
	pr_info("%s APU_PCU_BUCK_OFF_DAT0_L=0x%08x\n", __func__,
		apu_readl(apupw.regs[apu_pcu] + APU_PCU_BUCK_OFF_DAT0_L));
	pr_info("%s APU_PCU_BUCK_OFF_DAT0_H=0x%08x\n", __func__,
		apu_readl(apupw.regs[apu_pcu] + APU_PCU_BUCK_OFF_DAT0_H));

	// Step4. update buck settle time for vapu by SEL0
	apu_writel(VAPU_BUCK_ON_SETTLE_TIME,
			apupw.regs[apu_pcu] + APU_PCU_BUCK_ON_SLE0);

	pr_info("PCU init %s %d --\n", __func__, __LINE__);
}

static void __apu_rpclite_init(void)
{
	uint32_t sleep_type_offset[] = {0x0208, 0x020C, 0x0210, 0x0214,
					0x0218, 0x021C, 0x0220, 0x0224};
	enum apupw_reg rpc_lite_base[CLUSTER_NUM];
	int ofs_arr_size = sizeof(sleep_type_offset) / sizeof(uint32_t);
	int acx_idx, ofs_idx;

	pr_info("RPC-Lite init %s %d ++\n", __func__, __LINE__);

	rpc_lite_base[0] = apu_acx0_rpc_lite;

	if (CLUSTER_NUM == 2)
		rpc_lite_base[1] = apu_acx1_rpc_lite;

	for (acx_idx = 0 ; acx_idx < CLUSTER_NUM ; acx_idx++) {
		for (ofs_idx = 0 ; ofs_idx < ofs_arr_size ; ofs_idx++) {
			// Memory setting
			apu_clearl((0x1 << 1),
					apupw.regs[rpc_lite_base[acx_idx]]
					+ sleep_type_offset[ofs_idx]);
		}

		// Control setting
		apu_setl(0x0000009E, apupw.regs[rpc_lite_base[acx_idx]]
					+ APU_RPC_TOP_SEL);
	}

	dump_rpc_lite_reg(__LINE__);

	pr_info("RPC-Lite init %s %d --\n", __func__, __LINE__);
}

static void __apu_rpc_init(void)
{
	pr_info("RPC init %s %d ++\n", __func__, __LINE__);

	// Step7. RPC: memory types (sleep or PD type)
	// RPC: iTCM in uP need to setup to sleep type
	apu_clearl((0x1 << 1), apupw.regs[apu_rpc] + 0x0200);

	// Step9. RPCtop initial
	// RPC
	apu_setl(0x1800501E, apupw.regs[apu_rpc] + APU_RPC_TOP_SEL);
	// BUCK_PROT_SEL
	apu_setl((0x1 << 20), apupw.regs[apu_rpc] + APU_RPC_TOP_SEL_1);

	pr_info("RPC init %s %d --\n", __func__, __LINE__);
}

static int __apu_are_init(struct device *dev)
{
	u32 tmp = 0;
	int ret, val = 0;
	int are_id, idx = 0;
	uint32_t are_entry2_cfg_h[] = {0x000F0000, 0x000F0000, 0x000F0000};
	uint32_t are_entry2_cfg_l[] = {0x001F0705, 0x001F0707, 0x001F0707};

	// Step10. ARE initial

	/* TINFO="Wait for sare1 fsm to transition to IDLE" */
	// while ((apu_readl(apupw.regs[apu_are2] + 0x48) & 0x1) != 0x1);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_are2] + 0x48),
			val, (val & 0x1UL), 50, 10000);
	if (ret) {
		pr_info("%s timeout to wait sram1 fsm to idle, ret %d\n",
				__func__, ret);
		return -1;
	}

	pr_info("ARE init %s %d ++\n", __func__, __LINE__);

	for (are_id = apu_are0, idx = 0; are_id <= apu_are2; are_id++, idx++) {

		pr_info("%s are_id:%d\n", __func__, are_id);

		/* ARE entry 0 initial */
		apu_writel(0x01234567, apupw.regs[are_id]
						+ APU_ARE_ETRY0_SRAM_H);
		apu_writel(0x89ABCDEF, apupw.regs[are_id]
						+ APU_ARE_ETRY0_SRAM_L);

		/* ARE entry 1 initial */
		apu_writel(0xFEDCBA98, apupw.regs[are_id]
						+ APU_ARE_ETRY1_SRAM_H);
		apu_writel(0x76543210, apupw.regs[are_id]
						+ APU_ARE_ETRY1_SRAM_L);

		/* ARE entry 2 initial */
		apu_writel(are_entry2_cfg_h[idx], apupw.regs[are_id]
						+ APU_ARE_ETRY2_SRAM_H);
		apu_writel(are_entry2_cfg_l[idx], apupw.regs[are_id]
						+ APU_ARE_ETRY2_SRAM_L);

		/* dummy read ARE entry2 H/L sram */
		tmp = readl(apupw.regs[are_id] + APU_ARE_ETRY2_SRAM_H);
		tmp = readl(apupw.regs[are_id] + APU_ARE_ETRY2_SRAM_L);

		/* update ARE sram */
		apu_writel(0x00000004, apupw.regs[are_id] + APU_ARE_INI_CTRL);

		dev_info(dev, "%s ARE entry2_H phys 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[are_id] + APU_ARE_ETRY2_SRAM_H),
			readl(apupw.regs[are_id] + APU_ARE_ETRY2_SRAM_H));

		dev_info(dev, "%s ARE entry2_L phys 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[are_id] + APU_ARE_ETRY2_SRAM_L),
			readl(apupw.regs[are_id] + APU_ARE_ETRY2_SRAM_L));
	}

	pr_info("ARE init %s %d --\n", __func__, __LINE__);

	return 0;
}
#endif // APU_POWER_INIT

#if APUPW_DUMP_FROM_APMCU
static void are_dump_config(int are_hw)
{
	int entry = 0;
	int are_id = 0;

	if (are_hw == 0) {
		are_id = apu_are0;
	} else if (are_hw == 1) {
		are_id = apu_are1;
	} else {
		are_id = apu_are2;
	}

	pr_info("APU_ARE_DUMP are_hw:%d offset: 0x%03x = 0x%08x\n",
			are_hw, 0x4, readl(apupw.regs[are_id] + 0x4));
	pr_info("APU_ARE_DUMP are_hw:%d offset: 0x%03x = 0x%08x\n",
			are_hw, 0x40, readl(apupw.regs[are_id] + 0x40));
	pr_info("APU_ARE_DUMP are_hw:%d offset: 0x%03x = 0x%08x\n",
			are_hw, 0x44, readl(apupw.regs[are_id] + 0x44));
	pr_info("APU_ARE_DUMP are_hw:%d offset: 0x%03x = 0x%08x\n",
			are_hw, 0x48, readl(apupw.regs[are_id] + 0x48));
	pr_info("APU_ARE_DUMP are_hw:%d offset: 0x%03x = 0x%08x\n",
			are_hw, 0x4C, readl(apupw.regs[are_id] + 0x4C));

	for (entry = 0 ; entry <= 2 ; entry++) {
		pr_info(
		"APU_ARE_DUMP are_hw:%d cfg entry %d = H:0x%08x L:0x%08x\n",
			are_hw, entry,
			readl(apupw.regs[are_id] +
				APU_ARE_ETRY0_SRAM_H + entry * 4),
			readl(apupw.regs[are_id] +
				APU_ARE_ETRY0_SRAM_L + entry * 4));
	}
}

static void are_dump_entry(int are_hw)
{
	int are_id, are_entry_max_id;
	uint32_t reg, data;
	uint32_t target_data = 0x0;
	void *target_addr = 0x0;
	int entry, err_flag;

	if (are_hw == 0) {
		are_id = apu_are0;
		are_entry_max_id = 238;
	} else if (are_hw == 1) {
		are_id = apu_are1;
		are_entry_max_id = 210;
	} else {
		are_id = apu_are2;
		are_entry_max_id = 237;
	}

	for (entry = 3 ; entry <= are_entry_max_id ; entry++) {
		reg = readl(apupw.regs[are_id] +
				APU_ARE_ETRY0_SRAM_H + entry * 4);
		data = readl(apupw.regs[are_id] +
				APU_ARE_ETRY0_SRAM_L + entry * 4);
		err_flag = 0;
		target_addr = 0x0;
		target_data = 0x0;

		if (reg != 0x0) {
			//pr_info("%s: remapping 0x%08x\n", __func__, reg);
			target_addr = ioremap(reg, PAGE_SIZE);

			if (IS_ERR((void const *)target_addr)) {
				pr_info("%s: remap fail 0x%08x\n",
						__func__, reg);
			} else {
				target_data = readl(target_addr);
				iounmap(target_addr);
				if (target_data != data)
					err_flag = 1;

				pr_info(
					"APU_ARE_DUMP %d-%03d 0x%08x 0x%08x 0x%08x 0x%08x %d\n",
					are_hw, entry, reg, data,
					target_addr, target_data, err_flag);
			}
		}

	}
}
#endif

#if APMCU_REQ_RPC_SLEEP
// backup solution : send request for RPC sleep from APMCU
static int __apu_sleep_rpc_rcx(struct device *dev)
{
	uint32_t regValue;

	// REG_WAKEUP_CLR
	pr_info("%s step1. set REG_WAKEUP_CLR\n", __func__);
	apu_writel(0x00001000, apupw.regs[apu_rpc] + APU_RPC_TOP_CON);
	udelay(10);

	// mask RPC IRQ and bypass WFI
	pr_info("%s step2. mask RPC IRQ and bypass WFI\n", __func__);
	regValue = 0x0;
	regValue = apu_readl(apupw.regs[apu_rpc] + APU_RPC_TOP_SEL);
	regValue |= 0x9E;
	regValue |= (0x1 << 10);
	apu_writel(regValue, apupw.regs[apu_rpc] + APU_RPC_TOP_SEL);
	udelay(10);

	// clean up wakeup source (uP part), clear rpc irq
	pr_info("%s step3. clean wakeup/abort irq bit\n", __func__);
	regValue = 0x0;
	regValue = apu_readl(apupw.regs[apu_rpc] + APU_RPC_TOP_CON);
	regValue |= ((0x1 << 1) | (0x1 << 2));
	apu_writel(regValue, apupw.regs[apu_rpc] + APU_RPC_TOP_CON);
	udelay(10);
/*
	// FIXME : remove thie after SB
	// may need config this in FPGS environment
	pr_info("%s step4. ignore slp prot\n", __func__);
	regValue = 0x0;
	regValue = apu_readl(apupw.regs[apu_rpc] + 0x140);
	regValue |= (0x1 << 13);
	apu_writel(regValue, apupw.regs[apu_rpc] + 0x140);
	udelay(10);
*/
	// sleep request enable
	// CAUTION!! do NOT request sleep twice in succession
	// or system may crash (comments from DE)
	pr_info("%s Step5. sleep request\n", __func__);
	regValue = 0x0;
	regValue = apu_readl(apupw.regs[apu_rpc] + APU_RPC_TOP_CON);
	regValue |= 0x1;
	apu_writel(regValue, apupw.regs[apu_rpc] + APU_RPC_TOP_CON);

	udelay(100);

	dev_info(dev, "%s RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	return 0;
}
#endif

static int __apu_wake_rpc_rcx(struct device *dev)
{
	int ret = 0, val = 0;
	uint32_t cfg = 0x0;

	// check rpc register is correct or not
	cfg = apu_readl(apupw.regs[apu_rpc] + APU_RPC_TOP_SEL);
	if (cfg == RPC_TOP_SEL_HW_DEF ||
		(cfg != RPC_TOP_SEL_SW_CFG1 && cfg != RPC_TOP_SEL_SW_CFG2)) {
		ret = -EIO;
		pr_info("%s error return since RPC cfg is incorrect : 0x%08x\n",
				__func__, cfg);
		goto out;
	}

	dev_info(dev, "%s before wakeup RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_RCX,
			SMC_RCX_PWR_AFC_EN);

	/* wake up RPC */
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_RCX,
			SMC_RCX_PWR_WAKEUP_RPC);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL), 50, 10000);
	if (ret) {
		pr_info("%s polling RPC RDY timeout, ret %d\n", __func__, ret);
		goto out;
	}

	dev_info(dev, "%s after wakeup RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	/* polling FSM @RPC-lite to ensure RPC is in on/off stage */
	ret |= readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_STATUS),
			val, (val & (0x1 << 29)), 50, 10000);
	if (ret) {
		pr_info("%s polling ARE FSM timeout, ret %d\n", __func__, ret);
		goto out;
	}

	/* clear vcore/rcx cgs */
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_RCX,
			SMC_RCX_PWR_CG_EN);
out:
	return ret;
}

#if APU_POWER_BRING_UP
static int __apu_wake_rpc_acx(struct device *dev, enum t_acx_id acx_id)
{
	int ret = 0, val = 0;
	enum apupw_reg rpc_lite_base;
	enum apupw_reg acx_base;

	if (acx_id == ACX0) {
		rpc_lite_base = apu_acx0_rpc_lite;
		acx_base = apu_acx0;
	} else {
		rpc_lite_base = apu_acx1_rpc_lite;
		acx_base = apu_acx1;
	}

	dev_info(dev, "%s ctl p1:%d p2:%d\n",
			__func__, rpc_lite_base, acx_base);

	/* TINFO="Enable AFC enable" */
	apu_setl((0x1 << 16), apupw.regs[rpc_lite_base] + APU_RPC_TOP_SEL_1);

	/* wake acx rpc lite */
	apu_writel(0x00000100, apupw.regs[rpc_lite_base] + APU_RPC_TOP_CON);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL), 50, 10000);

	/* polling FSM @RPC-lite to ensure RPC is in on/off stage */
	ret |= readl_relaxed_poll_timeout_atomic(
			(apupw.regs[rpc_lite_base] + APU_RPC_STATUS),
			val, (val & (0x1 << 29)), 50, 10000);
	if (ret) {
		pr_info("%s wake up acx%d_rpc fail, ret %d\n",
				__func__, acx_id, ret);
		goto out;
	}

	dev_info(dev, "%s ACX%d APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
		__func__, acx_id,
		(u32)(apupw.phy_addr[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
		readl(apupw.regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY));

	/* clear acx0/1 CGs */
	apu_writel(0xFFFFFFFF, apupw.regs[acx_base] + APU_ACX_CONN_CG_CLR);

	dev_info(dev, "%s ACX%d APU_ACX_CONN_CG_CON 0x%x = 0x%x\n",
		__func__, acx_id,
		(u32)(apupw.phy_addr[acx_base] + APU_ACX_CONN_CG_CON),
		readl(apupw.regs[acx_base] + APU_ACX_CONN_CG_CON));
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

	// we support power only for bringup

	if (acx_id == ACX0) {
		rpc_lite_base = apu_acx0_rpc_lite;
		acx_base = apu_acx0;
	} else {
		rpc_lite_base = apu_acx1_rpc_lite;
		acx_base = apu_acx1;
	}

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
	default:
		goto out;
	}

	dev_info(dev, "%s ctl p1:%d p2:%d p3:0x%x p4:0x%x p5:0x%x p6:0x%x\n",
		__func__, rpc_lite_base, acx_base,
		dev_mtcmos_ctl, dev_mtcmos_chk, dev_cg_con, dev_cg_clr);

	/* config acx rpc lite */
	apu_writel(dev_mtcmos_ctl,
			apupw.regs[rpc_lite_base] + APU_RPC_SW_FIFO_WE);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
			val, (val & dev_mtcmos_chk) == dev_mtcmos_chk, 50, 200);
	if (ret) {
		pr_info("%s config acx%d_rpc 0x%x fail, ret %d\n",
				__func__, acx_id, dev_mtcmos_ctl, ret);
		goto out;
	}

	dev_info(dev, "%s ACX%d APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
		__func__, acx_id,
		(u32)(apupw.phy_addr[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
		readl(apupw.regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY));

	apu_writel(0xFFFFFFFF, apupw.regs[acx_base] + dev_cg_clr);

	dev_info(dev, "%s ACX%d dev%d CG_CON 0x%x = 0x%x\n",
		__func__, acx_id, dev_id,
		(u32)(apupw.phy_addr[acx_base] + dev_cg_con),
		readl(apupw.regs[acx_base] + dev_cg_con));
out:
	return ret;
}

#if !APU_PWR_SOC_PATH
static int __apu_on_mdla_mvpu_clk(void)
{
	int ret = 0, val = 0;

	/* turn on mvpu root clk src */
	apu_writel(0x00000200, apupw.regs[apu_acc] + APU_ACC_AUTO_CTRL_SET2);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acc] + APU_ACC_AUTO_STATUS2),
			val, (val & 0x20UL) == 0x20UL, 50, 10000);
	if (ret) {
		pr_info("%s turn on mvpu root clk fail, ret %d\n",
				__func__, ret);
		goto out;
	}

	/* turn on mdla root clk src */
	apu_writel(0x00000200, apupw.regs[apu_acc] + APU_ACC_AUTO_CTRL_SET3);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acc] + APU_ACC_AUTO_STATUS3),
			val, (val & 0x20UL) == 0x20UL, 50, 10000);
	if (ret) {
		pr_info("%s turn on mdla root clk fail, ret %d\n",
				__func__, ret);
		goto out;
	}

out:
	return ret;
}
#endif // !APU_PWR_SOC_PATH
#endif // APU_POWER_BRING_UP

#if SUPPORT_VSRAM_0P75_VB
static int aputop_acquire_hw_sema(struct device *dev)
{
	static int timeout_cnt_max;
	int timeout_cnt = 0;

	pr_info("%s +\n", __func__);

	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_RCX,
			SMC_RCX_PWR_HW_SEMA);
	udelay(10);

	while ((apu_readl(apupw.regs[sys_spm] + SPM_HW_SEMA_MASTER) & BIT(2))
			!= BIT(2)) {

		if (timeout_cnt++ >= HW_SEMA_TIMEOUT_CNT) {
			pr_info("%s acquire timeout! (%d)\n",
					__func__, timeout_cnt);
			return -1;
		}

		apusys_pwr_smc_call(dev,
				MTK_APUSYS_KERNEL_OP_APUSYS_PWR_RCX,
				SMC_RCX_PWR_HW_SEMA);
		udelay(10);
	}

	if (timeout_cnt > timeout_cnt_max)
		timeout_cnt_max = timeout_cnt;

	pr_info("%s (timeout_cnt:%d, timeout_cnt_max:%d) -\n",
			__func__, timeout_cnt, timeout_cnt_max);
	return 0;
}

static int aputop_release_hw_sema(struct device *dev)
{
	static int timeout_cnt_max;
	int timeout_cnt = 0;

	pr_info("%s +\n", __func__);

	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_RCX,
			SMC_RCX_PWR_HW_SEMA);
	udelay(10);

	while ((apu_readl(apupw.regs[sys_spm] + SPM_HW_SEMA_MASTER) & BIT(2))
			>> 2 != 0x0) {

		if (timeout_cnt++ >= HW_SEMA_TIMEOUT_CNT) {
			pr_info("%s release timeout! (%d)\n",
					__func__, timeout_cnt);
			return -1;
		}

		udelay(10);
	}

	if (timeout_cnt > timeout_cnt_max)
		timeout_cnt_max = timeout_cnt;

	pr_info("%s (timeout_cnt:%d, timeout_cnt_max:%d) -\n",
			__func__, timeout_cnt, timeout_cnt_max);

	return 0;
}
#endif

static void aputop_dump_pcu_data(struct device *dev)
{
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_DUMP,
			SMC_PWR_DUMP_PCU);
}

static int mt6983_apu_top_on(struct device *dev)
{
	int ret = 0;

	pr_info("%s +\n", __func__);

#if SUPPORT_VSRAM_0P75_VB
	if (aputop_acquire_hw_sema(dev)) {
		apupw_aee_warn("APUSYS_POWER", "APUSYS_POWER_WAKEUP_FAIL");
		return -1;
	}
#endif

#if (ENABLE_SOC_CLK_MUX || ENABLE_SW_BUCK_CTL)
	// FIXME: remove this since it should be auto ctl by RPC flow
	plt_pwr_res_ctl(1);
#endif
	ret = __apu_wake_rpc_rcx(dev);

	if (ret) {
		pr_info("%s fail to wakeup RPC, ret %d, rpc_alive:%d\n",
					__func__, ret, check_if_rpc_alive());
		aputop_dump_pwr_reg(dev);
		aputop_dump_pwr_res();
		aputop_dump_rpc_data();
		aputop_dump_pcu_data(dev);
		aputop_dump_pll_data();
		aputop_check_pwr_data();
#if APUPW_DUMP_FROM_APMCU
		are_dump_config(0);
		are_dump_config(1);
		are_dump_config(2);
#endif

#if SUPPORT_VSRAM_0P75_VB
		aputop_release_hw_sema(dev);
#endif
		if (ret == -EIO)
			apupw_aee_warn("APUSYS_POWER",
					"APUSYS_POWER_RPC_CFG_ERR");
		else
			apupw_aee_warn("APUSYS_POWER",
					"APUSYS_POWER_WAKEUP_FAIL");
		return -1;
	}

	// for refcnt ++ to avoid be auto turned off by regulator framework
	pr_info("%s enable vapu regulator\n", __func__);
	ret = regulator_enable(vapu_reg_id);
	if (ret < 0) {
		pr_info("%s fail enable vapu : %d\n", __func__, ret);
		return -1;
	}

	pr_info("%s -\n", __func__);
	return 0;
}

static int mt6983_apu_top_off(struct device *dev)
{
	int ret = 0, val = 0;
	int rpc_timeout_val = 500000; // 500 ms
	int polling_cnt = 0;
	int max_polling = 100;

	pr_info("%s +\n", __func__);

#if APMCU_REQ_RPC_SLEEP
	// backup solution : send request for RPC sleep from APMCU
	__apu_sleep_rpc_rcx(dev);
#else
	mt6983_pwr_flow_remote_sync(1); // tell remote side I am ready to off
#endif
	// blocking until sleep success or timeout, delay 50 us per round
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL) == 0x0, 50, rpc_timeout_val);
	if (ret) {
		pr_info("%s polling PWR RDY timeout\n", __func__);

	} else {
		ret = readl_relaxed_poll_timeout_atomic(
				(apupw.regs[apu_rpc] + APU_RPC_STATUS),
				val, (val & 0x1UL) == 0x1, 50, 10000);
		if (ret)
			pr_info("%s polling PWR STATUS timeout\n", __func__);

		if (vapu_reg_id) {
			while (1) {
				if (regulator_is_enabled(vapu_reg_id) == 0)
					break;

				if (polling_cnt++ >= max_polling) {
					pr_info("%s polling buck off timeout\n",
							__func__);
					break;
				}

				udelay(20);
			}
		}
	}

	if (ret) {
		pr_info(
		"%s timeout to wait RPC sleep (val:%d), ret %d, rpc_alive:%d\n",
			__func__, rpc_timeout_val, ret, check_if_rpc_alive());
		aputop_dump_pwr_reg(dev);
		aputop_dump_pwr_res();
		aputop_dump_rpc_data();
		aputop_dump_pcu_data(dev);
		aputop_dump_pll_data();
		aputop_check_pwr_data();
#if APUPW_DUMP_FROM_APMCU
		are_dump_config(0);
		are_dump_config(1);
		are_dump_config(2);
#endif

#if SUPPORT_VSRAM_0P75_VB
		aputop_release_hw_sema(dev);
#endif
		apupw_aee_warn("APUSYS_POWER", "APUSYS_POWER_SLEEP_TIMEOUT");
		return -1;
	}

	// mt6983_apu_dump_rpc_status(RCX, NULL);

	// for refcnt ++ to avoid be auto turned off by regulator framework
	pr_info("%s disable vapu regulator\n", __func__);
	ret = regulator_disable(vapu_reg_id);
	if (ret < 0) {
		pr_info("%s fail disable vapu : %d\n", __func__, ret);
		return -1;
	}

#if (ENABLE_SOC_CLK_MUX || ENABLE_SW_BUCK_CTL)
	// FIXME: remove this since it should be auto ctl by RPC flow
	plt_pwr_res_ctl(0);
#endif

#if SUPPORT_VSRAM_0P75_VB
	if (aputop_release_hw_sema(dev)) {
		apupw_aee_warn("APUSYS_POWER", "APUSYS_POWER_SLEEP_TIMEOUT");
		return -1;
	}
#endif
	pr_info("%s -\n", __func__);
	return 0;
}

#if APU_POWER_INIT
static void __apu_aoc_init(void)
{
	pr_info("AOC init %s %d ++\n", __func__, __LINE__);

	// Step1. Switch APU AOC control signal from SW register
	//	  to HW path (RPC)
	// rpc_sram_ctrl_mux_sel
	apu_setl((0x1 << 0), apupw.regs[sys_spm] + 0x414);
	// apu_vcore_off_iso_en
	apu_clearl((0x1 << 1), apupw.regs[sys_spm] + 0x414);
	// apu_are_req
	apu_setl((0x1 << 4), apupw.regs[sys_spm] + 0x414);

	// Step2. Manually disable Buck els enable @SOC
	//	  (disable SW mode to manually control Buck on/off)
	// vapu_ext_buck_iso
	apu_clearl((0x1 << 4), apupw.regs[sys_spm] + 0xf30);

	// Step3. Roll back to APU Buck on stage
	//	The following setting need to in order
	//	and wait 1uS before setup next control signal
	// APU_BUCK_ELS_EN
	apu_writel(0x00000800, apupw.regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(10);

	// APU_BUCK_RST_B
	apu_writel(0x00001000, apupw.regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(10);

	// APU_BUCK_PROT_REQ
	apu_writel(0x00008000, apupw.regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(10);

	// SRAM_AOC_ISO
	apu_writel(0x00000080, apupw.regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(10);

	pr_info("AOC init %s %d --\n", __func__, __LINE__);
}
#endif // APU_POWER_INIT

static int init_plat_chip_data(struct platform_device *pdev)
{
	struct plat_cfg_data plat_cfg;
	uint32_t aging_attr = 0x0;
	uint32_t vsram_vb_attr = 0x0;
	uint32_t misc_cfg_attr = 0x0;

	memset(&plat_cfg, 0, sizeof(plat_cfg));

	of_property_read_u32(pdev->dev.of_node, "aging_load", &aging_attr);
	of_property_read_u32(pdev->dev.of_node, "vsram_vb_en", &vsram_vb_attr);
	of_property_read_u32(pdev->dev.of_node, "misc_cfg", &misc_cfg_attr);

	plat_cfg.aging_flag = (aging_attr & 0xf);
	plat_cfg.hw_id = 0x0;
	plat_cfg.vsram_vb_en = (vsram_vb_attr & 0xf);
	plat_cfg.misc_cfg = (misc_cfg_attr & 0xf);

	pr_info("%s 0x%08x 0x%08x 0x%08x 0x%08x\n",
		__func__,
		plat_cfg.aging_flag, plat_cfg.hw_id,
		plat_cfg.vsram_vb_en, plat_cfg.misc_cfg);

	return mt6983_chip_data_remote_sync(&plat_cfg);
}

#if APU_POWER_INIT
static int init_hw_setting(struct device *dev)
{
	__apu_aoc_init();
	__apu_pcu_init();
	__apu_rpc_init();
	__apu_rpclite_init();
	__apu_are_init(dev);
#if !APU_PWR_SOC_PATH
	__apu_pll_init();
	__apu_acc_init();
#endif
	__apu_buck_off_cfg();

	return 0;
}
#endif // APU_POWER_INIT

static int init_reg_base(struct platform_device *pdev)
{
	struct resource *res;
	int idx = 0;

	pr_info("%s %d APUPW_MAX_REGS = %d\n",
			__func__, __LINE__, APUPW_MAX_REGS);

	for (idx = 0; idx < APUPW_MAX_REGS; idx++) {

		res = platform_get_resource_byname(
				pdev, IORESOURCE_MEM, reg_name[idx]);

		if (res == NULL) {
			pr_info("%s: get resource \"%s\" fail\n",
					__func__, reg_name[idx]);
			return -ENODEV;
		}

		pr_info("%s: get resource \"%s\" pass\n",
				__func__, reg_name[idx]);

		apupw.regs[idx] = ioremap(res->start,
				res->end - res->start + 1);

		if (IS_ERR_OR_NULL(apupw.regs[idx])) {
			pr_info("%s: %s remap base fail\n",
					__func__, reg_name[idx]);
			return -ENOMEM;
		}

		apupw.phy_addr[idx] = res->start;
	}

	return 0;
}

static int mt6983_apu_top_pb(struct platform_device *pdev)
{
#if APU_POWER_INIT
	int ret_clk = 0;
	int ret = 0;
#endif

	pr_info("%s fpga_type : %d\n", __func__, fpga_type);

	init_reg_base(pdev);

	init_plat_pwr_res(pdev);

#if APU_POWER_INIT
	// enable vapu buck
	ret = regulator_enable(vapu_reg_id);
	if (ret < 0) {
		pr_info("%s fail enable vapu : %d\n", __func__, ret);
		return -1;
	}

	// set vapu to default voltage
	regulator_set_voltage(vapu_reg_id, VAPU_DEF_VOLT, VAPU_DEF_VOLT);

	pr_info("%s vapu:%d (en:%d)\n", __func__,
			regulator_get_voltage(vapu_reg_id),
			regulator_is_enabled(vapu_reg_id));

	ENABLE_CLK(clk_top_ipu_if_sel);
	ENABLE_CLK(clk_top_dsp_sel);
	ENABLE_CLK(clk_top_dsp1_sel);
	ENABLE_CLK(clk_top_dsp2_sel);
	ENABLE_CLK(clk_top_dsp3_sel);
	ENABLE_CLK(clk_top_dsp4_sel);
	ENABLE_CLK(clk_top_dsp5_sel);
	ENABLE_CLK(clk_top_dsp6_sel);
	ENABLE_CLK(clk_top_dsp7_sel);

	// before apu power init, we have to ensure soc regulator/clk is ready
	init_hw_setting(&pdev->dev);

#if !APU_POWER_BRING_UP
	DISABLE_CLK(clk_top_dsp7_sel);
	DISABLE_CLK(clk_top_dsp6_sel);
	DISABLE_CLK(clk_top_dsp5_sel);
	DISABLE_CLK(clk_top_dsp4_sel);
	DISABLE_CLK(clk_top_dsp3_sel);
	DISABLE_CLK(clk_top_dsp2_sel);
	DISABLE_CLK(clk_top_dsp1_sel);
	DISABLE_CLK(clk_top_dsp_sel);
	DISABLE_CLK(clk_top_ipu_if_sel);

	// set vapu to default voltage
	regulator_set_voltage(vapu_reg_id, VAPU_DEF_VOLT, VAPU_DEF_VOLT);

	// disable vapu buck
	ret = regulator_disable(vapu_reg_id);
	if (ret < 0) {
		pr_info("%s fail disable vapu : %d\n", __func__, ret);
		return -1;
	}
#endif
	// Step12. After APUsys is finished, update the following register to 1,
	//	   ARE will use this information to ensure the SRAM in ARE is
	//	   trusted or not
	apu_setl(0x1 << 0, apupw.regs[sys_vlp] + APUSYS_AO_CTRL_ADDR);
#endif // APU_POWER_INIT

	mt6983_init_remote_data_sync(apupw.regs[apu_md32_mbox]);
	init_plat_chip_data(pdev);

#if APU_POWER_BRING_UP
#if !APU_PWR_SOC_PATH
	// power bring up shall use soc PLL
	__apu_on_mdla_mvpu_clk();
#endif
	switch (fpga_type) {
	default:
	case 0: // do not power on
		pr_info("%s bypass pre-power-ON\n", __func__);
		break;
	case 1: // power on : RCX_TOP/ACX0_TOP/ACX1_TOP/ACX0_MVPU0/ACX1_MVPU0
		pm_runtime_get_sync(&pdev->dev);
		__apu_wake_rpc_acx(&pdev->dev, ACX0);
		__apu_wake_rpc_acx(&pdev->dev, ACX1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, VPU0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, VPU0, 1);
		break;
	case 2: // power on : RCX_TOP/ACX0_TOP/ACX1_TOP/ACX0_MVPU0/ACX1_MDLA0
		pm_runtime_get_sync(&pdev->dev);
		__apu_wake_rpc_acx(&pdev->dev, ACX0);
		__apu_wake_rpc_acx(&pdev->dev, ACX1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, VPU0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, DLA0, 1);
		break;
	case 3: // power on : RCX_TOP/ACX0_TOP/ACX1_TOP/ACX0_MDLA0/ACX1_MDLA0
		pm_runtime_get_sync(&pdev->dev);
		__apu_wake_rpc_acx(&pdev->dev, ACX0);
		__apu_wake_rpc_acx(&pdev->dev, ACX1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, DLA0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, DLA0, 1);
		break;
	case 9: // power on : RCX_TOP/ACX0_TOP/ACX1_TOP/All_MVPU/ALL_MDLA
		pm_runtime_get_sync(&pdev->dev);
		__apu_wake_rpc_acx(&pdev->dev, ACX0);
		__apu_wake_rpc_acx(&pdev->dev, ACX1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, VPU0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, DLA0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, DLA1, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, VPU0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, DLA0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, DLA1, 1);
		break;
	}
#else
	pm_runtime_get_sync(&pdev->dev);
#endif // APU_POWER_BRING_UP

	aputop_dump_pwr_res();
	return 0;
}

static int mt6983_apu_top_rm(struct platform_device *pdev)
{
	int idx;

	pr_info("%s +\n", __func__);

	if (fpga_type != 0)
		pm_runtime_put_sync(&pdev->dev);

	destroy_plat_pwr_res();

	for (idx = 0; idx < APUPW_MAX_REGS; idx++)
		iounmap(apupw.regs[idx]);

	pr_info("%s -\n", __func__);

	return 0;
}

static int mt6983_apu_top_suspend(struct device *dev)
{
	g_opp_cfg_acx0 = apu_readl(
			apupw.regs[apu_md32_mbox] + ACX0_LIMIT_OPP_REG);
	g_opp_cfg_acx1 = apu_readl(
			apupw.regs[apu_md32_mbox] + ACX1_LIMIT_OPP_REG);

	pr_info("%s backup data 0x%08x 0x%08x\n", __func__,
			g_opp_cfg_acx0, g_opp_cfg_acx1);
	return 0;
}

static int mt6983_apu_top_resume(struct device *dev)
{
	pr_info("%s restore data 0x%08x 0x%08x\n", __func__,
			g_opp_cfg_acx0, g_opp_cfg_acx1);

	apu_writel(g_opp_cfg_acx0,
			apupw.regs[apu_md32_mbox] + ACX0_LIMIT_OPP_REG);
	apu_writel(g_opp_cfg_acx1,
			apupw.regs[apu_md32_mbox] + ACX1_LIMIT_OPP_REG);
	return 0;
}

static void aputop_dump_pwr_res(void)
{
	int vapu_en = 0, vapu_mode = 0;
	uint32_t vapu = 0;
	uint32_t vcore = 0;
	uint32_t vsram = 0;

	if (vapu_reg_id) {
		vapu = regulator_get_voltage(vapu_reg_id);
		vapu_en = regulator_is_enabled(vapu_reg_id);
		vapu_mode = regulator_get_mode(vapu_reg_id);
	}

	if (vcore_reg_id)
		vcore = regulator_get_voltage(vcore_reg_id);

	if (vsram_reg_id)
		vsram = regulator_get_voltage(vsram_reg_id);

	pr_info("%s vapu:%u(en:%d,mode:%d) vcore:%u vsram:%u\n",
			__func__, vapu, vapu_en, vapu_mode, vcore, vsram);

	// mt6983_apu_dump_rpc_status(RCX, NULL);
#if APU_POWER_BRING_UP
	mt6983_apu_dump_rpc_status(ACX0, NULL);
	mt6983_apu_dump_rpc_status(ACX1, NULL);
#endif
}

static void aputop_dump_pwr_reg(struct device *dev)
{
#if APUPW_DUMP_FROM_APMCU
	char buf[32];

	// reg dump for RPC
	memset(buf, 0, sizeof(buf));
	snprintf(buf, 32, "phys 0x%08x: ",
			(u32)(apupw.phy_addr[apu_rpc]));
	print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
			apupw.regs[apu_rpc], 0x300, true);

	// reg dump for PCU
	memset(buf, 0, sizeof(buf));
	snprintf(buf, 32, "phys 0x%08x: ",
			(u32)(apupw.phy_addr[apu_pcu]));
	print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
			apupw.regs[apu_pcu], 0x100, true);

	// reg dump for S.ARE0
	memset(buf, 0, sizeof(buf));
	snprintf(buf, 32, "phys 0x%08x: ",
			(u32)(apupw.phy_addr[apu_are0]));
	print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
			apupw.regs[apu_are0], 0x50, true);

	// reg dump for N.ARE
	memset(buf, 0, sizeof(buf));
	snprintf(buf, 32, "phys 0x%08x: ",
			(u32)(apupw.phy_addr[apu_are1]));
	print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
			apupw.regs[apu_are1], 0x50, true);

	// reg dump for S.ARE1
	memset(buf, 0, sizeof(buf));
	snprintf(buf, 32, "phys 0x%08x: ",
			(u32)(apupw.phy_addr[apu_are2]));
	print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
			apupw.regs[apu_are2], 0x50, true);
#else
	// dump reg in ATF log
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_DUMP,
			SMC_PWR_DUMP_ALL);
	// dump reg in AEE db
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_REGDUMP, 0);
#endif
}

static int mt6983_apu_top_func(struct platform_device *pdev,
		enum aputop_func_id func_id, struct aputop_func_param *aputop)
{
	pr_info("%s func_id : %d\n", __func__, aputop->func_id);

	switch (aputop->func_id) {
	case APUTOP_FUNC_PWR_OFF:
		pm_runtime_put_sync(&pdev->dev);
		break;
	case APUTOP_FUNC_PWR_ON:
		pm_runtime_get_sync(&pdev->dev);
		break;
	case APUTOP_FUNC_OPP_LIMIT_HAL:
		mt6983_aputop_opp_limit(aputop, OPP_LIMIT_HAL);
		break;
	case APUTOP_FUNC_OPP_LIMIT_DBG:
		mt6983_aputop_opp_limit(aputop, OPP_LIMIT_DEBUG);
		break;
	case APUTOP_FUNC_DUMP_REG:

		aputop_dump_pwr_res();
		aputop_dump_pwr_reg(&pdev->dev);

#if DEBUG_DUMP_REG
		aputop_dump_all_reg();
#endif
		break;
	case APUTOP_FUNC_DRV_CFG:
		mt6983_drv_cfg_remote_sync(aputop);
		break;
	case APUTOP_FUNC_IPI_TEST:
		test_ipi_wakeup_apu();
		break;
	case APUTOP_FUNC_ARE_DUMP1:
#if APUPW_DUMP_FROM_APMCU
		are_dump_config(0);
		are_dump_entry(0);
		are_dump_config(1);
		are_dump_entry(1);
#endif
		break;
	case APUTOP_FUNC_ARE_DUMP2:
#if APUPW_DUMP_FROM_APMCU
		are_dump_config(2);
		are_dump_entry(2);
#endif
		break;
	case APUTOP_FUNC_DBG_VSRAM_VB:
#if SUPPORT_VSRAM_0P75_VB
		pr_info("VSRAM_VB target : %d\n", aputop->param1);
		if (aputop->param1 == 750)
			apu_writel(750000,
				apupw.regs[apu_md32_mbox] + VSRAM_VB_SYNC_REG);
		else if (aputop->param1 == 700)
			apu_writel(700000,
				apupw.regs[apu_md32_mbox] + VSRAM_VB_SYNC_REG);
		else
			pr_info("VSRAM_VB over range ! plz enter 750 or 700\n");
#endif
		break;
	default:
		pr_info("%s invalid func_id : %d\n", __func__, aputop->func_id);
		return -EINVAL;
	}

	return 0;
}

const struct apupwr_plat_data mt6983_plat_data = {
	.plat_name = "mt6983_apupwr",
	.plat_aputop_on = mt6983_apu_top_on,
	.plat_aputop_off = mt6983_apu_top_off,
	.plat_aputop_pb = mt6983_apu_top_pb,
	.plat_aputop_rm = mt6983_apu_top_rm,
	.plat_aputop_suspend = mt6983_apu_top_suspend,
	.plat_aputop_resume = mt6983_apu_top_resume,
	.plat_aputop_func = mt6983_apu_top_func,
#if IS_ENABLED(CONFIG_DEBUG_FS)
	.plat_aputop_dbg_open = mt6983_apu_top_dbg_open,
	.plat_aputop_dbg_write = mt6983_apu_top_dbg_write,
#endif
	.plat_rpmsg_callback = mt6983_apu_top_rpmsg_cb,
	.bypass_pwr_on = 0,
	.bypass_pwr_off = 0,
};
