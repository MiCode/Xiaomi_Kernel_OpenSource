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

#include "apu_top.h"
#include "mt6983_apupwr.h"
#include "mt6983_apupwr_prot.h"

/* Below reg_name has to 1-1 mapping DTS's name */
static const char *reg_name[APUPW_MAX_REGS] = {
	"sys_spm", "apu_rcx", "apu_vcore", "apu_md32_mbox",
	"apu_rpc", "apu_pcu", "apu_ao_ctl", "apu_pll", "apu_acc",
	"apu_are0", "apu_are1", "apu_are2",
	"apu_acx0", "apu_acx0_rpc_lite", "apu_acx1", "apu_acx1_rpc_lite",
};

static struct apu_power apupw;
static uint32_t ref_count;

static void aputop_dump_pwr_res(void);

#if !CFG_FPGA
/* regulator id */
static struct regulator *vapu_reg_id;
static struct regulator *vcore_reg_id;
static struct regulator *vsram_reg_id;
/* apu_top preclk */
static struct clk *clk_top_dsp_sel;		/* CONN */
static struct clk *clk_top_ipu_if_sel;		/* VCORE */

static int init_plat_pwr_res(struct platform_device *pdev)
{
	int ret = 0;
	int ret_clk = 0;

	// vapu Buck
	vapu_reg_id = regulator_get(&pdev->dev, "vapu");
	if (!vapu_reg_id) {
		pr_info("regulator_get vapu_reg_id failed\n");
		return -ENOENT;
	}

	// enable vapu buck
	regulator_enable(vapu_reg_id);

	// set vapu to 0.6v
	regulator_set_voltage(vapu_reg_id, 600000, 600000);

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

	//pre clk
	PREPARE_CLK(clk_top_dsp_sel);
	PREPARE_CLK(clk_top_ipu_if_sel);
	if (ret_clk < 0)
		return ret_clk;

	return 0;
}

static void destroy_plat_pwr_res(void)
{
	UNPREPARE_CLK(clk_top_dsp_sel);
	UNPREPARE_CLK(clk_top_ipu_if_sel);

	regulator_put(vapu_reg_id);
	regulator_put(vsram_reg_id);
	vapu_reg_id = NULL;
	vsram_reg_id = NULL;
}

static void plt_pwr_res_ctl(int enable)
{
	int ret;
	int ret_clk = 0;

	if (enable) {
		ret = regulator_enable(vapu_reg_id);
		if (ret < 0)
			return ret;

		apu_writel(
			apu_readl(apupw.regs[sys_spm] + APUSYS_BUCK_ISOLATION)
			& ~(0x00000021),
			apupw.regs[sys_spm] + APUSYS_BUCK_ISOLATION);

		ENABLE_CLK(clk_top_ipu_if_sel);
		ENABLE_CLK(clk_top_dsp_sel);
		if (ret_clk < 0)
			return ret_clk;

	} else {
		DISABLE_CLK(clk_top_dsp_sel);
		DISABLE_CLK(clk_top_ipu_if_sel);

		apu_writel(
			apu_readl(apupw.regs[sys_spm] + APUSYS_BUCK_ISOLATION)
			| 0x00000021,
			apupw.regs[sys_spm] + APUSYS_BUCK_ISOLATION);

		ret = regulator_disable(vapu_reg_id);
		if (ret < 0)
			return ret;
	}
}

static uint32_t get_pll_pcw(uint32_t clk_rate,
                uint32_t postdiv_val, uint32_t postdiv_reg)
{
        uint32_t fvco = clk_rate;
        uint32_t pcw_val;

        while (fvco <= 1500) {
                postdiv_val= postdiv_val << 1;
                postdiv_reg = postdiv_reg + 1;
                fvco = fvco << 1;
        }

        pcw_val = (fvco * 1 << 14) / 26;

        if (postdiv_reg == 0) { //Fvco * 2 with post_divider = 2
                pcw_val = pcw_val * 2;
                postdiv_val = postdiv_val << 1;
                postdiv_reg = postdiv_reg + 1;
        } //Post divider is 1 is not available

	pr_info("%s postdiv_reg:%d, pcw_val:%x\n",
			__func__, postdiv_reg, pcw_val);

        return pcw_val;
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
	uint32_t pll_base_arr[] = {MNOC_PLL_BASE, UP_PLL_BASE,
					MDLA_PLL_BASE, MVPU_PLL_BASE};
	uint32_t pll_freq_pcw[] = {0x0, 0x0, 0x0, 0x0};
	int pll_arr_size = sizeof(pll_base_arr) / sizeof(uint32_t);
	int pll_idx;

	pr_info("PLL init %s %d ++\n", __func__, __LINE__);

	// Step4. Initial PLL setting
	pll_freq_pcw[0] = get_pll_pcw(1552, 2, 0x1); // mnoc 388 MHz
	pll_freq_pcw[1] = get_pll_pcw(1940, 2, 0x1); // uP 485 MHz
	pll_freq_pcw[2] = get_pll_pcw(1940, 2, 0x1); // mdla 485 MHz
	pll_freq_pcw[3] = get_pll_pcw(2160, 2, 0x1); // mvpu 540 MHz

	for (pll_idx = 0 ; pll_idx < pll_arr_size ; pll_idx++) {
		// PCW value always from hopping function: ofs 0x100
		apu_setl(0x1 << 0, apupw.regs[apu_pll]
			+ pll_base_arr[pll_idx] + PLL1UPLL_FHCTL_HP_EN);
		// Hopping function clock enable: ofs 0x108
		apu_setl(0x1 << 0, apupw.regs[apu_pll]
			+ pll_base_arr[pll_idx] + PLL1UPLL_FHCTL_CLK_CON);
		// Hopping function enable: ofs 0x114
		apu_setl((0x1 << 0) | (0x1 << 2), apupw.regs[apu_pll]
			+ pll_base_arr[pll_idx] + PLL1UPLL_FHCTL0_CFG);

		// POSTDIV: ofs 0x000C , [26:24] RG_PLL_POSDIV
		// 3'b000: /1 , 3'b001: /2 , 3'b010: /4
		// 3'b011: /8 , 3'b100: /16
		apu_setl((0x1 << 24), apupw.regs[apu_pll]
			+ pll_base_arr[pll_idx] + PLL1U_PLL1_CON1);

		// PCW register: ofs 0x011C
		// [31] FHCTL0_PLL_TGL_ORG
		// [21:0] FHCTL0_PLL_ORG set to PCW value
		apu_writel(((0x1 << 31) | pll_freq_pcw[pll_idx]),
			apupw.regs[apu_pll]
			+ pll_base_arr[pll_idx] + PLL1UPLL_FHCTL0_DDS);
	}

	pr_info("PLL init %s %d --\n", __func__, __LINE__);
}

static void __apu_acc_init(void)
{
	char buf[32];

	pr_info("ACC init %s %d ++\n", __func__, __LINE__);

	// Step6. Initial ACC setting (@ACC)

	/* mnoc clk setting */
	pr_info("mnoc clk setting %s %d \n", __func__, __LINE__);
	// CGEN_SOC
	apu_writel(0x00000004, apupw.regs[apu_acc] + APU_ACC_CONFG_CLR0);
	// HW_CTRL_EN, SEL_APU_DIV2
	apu_writel(0x00008400, apupw.regs[apu_acc] + APU_ACC_CONFG_SET0);

	/* iommu clk setting */
	pr_info("iommu clk setting %s %d \n", __func__, __LINE__);
	// CGEN_SOC
	apu_writel(0x00000004, apupw.regs[apu_acc] + APU_ACC_CONFG_CLR1);
	// HW_CTRL_EN, SEL_APU_DIV2
	apu_writel(0x00008400, apupw.regs[apu_acc] + APU_ACC_CONFG_SET1);

	/* mvpu clk setting */
	pr_info("mvpu clk setting %s %d \n", __func__, __LINE__);
	// CGEN_SOC
	apu_writel(0x00000004, apupw.regs[apu_acc] + APU_ACC_CONFG_CLR2);
	// HW_CTRL_EN, SEL_APU_DIV2
	apu_writel(0x00008400, apupw.regs[apu_acc] + APU_ACC_CONFG_SET2);
	// CLK_REQ_SW_EN
	apu_writel(0x00000100, apupw.regs[apu_acc] + APU_ACC_AUTO_CTRL_SET2);

	/* mdla clk setting */
	pr_info("mdla clk setting %s %d \n", __func__, __LINE__);
	// CGEN_SOC
	apu_writel(0x00000004, apupw.regs[apu_acc] + APU_ACC_CONFG_CLR3);
	// HW_CTRL_EN, SEL_APU_DIV2
	apu_writel(0x00008400, apupw.regs[apu_acc] + APU_ACC_CONFG_SET3);
	// CLK_REQ_SW_EN
	apu_writel(0x00000100, apupw.regs[apu_acc] + APU_ACC_AUTO_CTRL_SET3);

	/* clk invert setting */
	pr_info("clk invert setting %s %d \n", __func__, __LINE__);
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

static int __apu_on_mdla_mvpu_clk(void)
{
	int ret = 0, val = 0;

	/* turn on mvpu root clk src */
	apu_writel(0x00000200, apupw.regs[apu_acc] + APU_ACC_AUTO_CTRL_SET2);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acc] + APU_ACC_AUTO_STATUS2),
			val, (val & 0x20UL) == 0x20UL,
			50, 10000);
	if (ret) {
		pr_info("%s turn on mvpu root clk fail, ret %d\n",
				__func__, ret);
		goto out;
	}

	/* turn on mdla root clk src */
	apu_writel(0x00000200, apupw.regs[apu_acc] + APU_ACC_AUTO_CTRL_SET3);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acc] + APU_ACC_AUTO_STATUS3),
			val, (val & 0x20UL) == 0x20UL,
			50, 10000);
	if (ret) {
		pr_info("%s turn on mdla root clk fail, ret %d\n",
				__func__, ret);
		goto out;
	}

out:
	return ret;
}
#endif // !CFG_FPGA

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

/*
 * low 32-bit data for PMIC control
 *	APU_PCU_PMIC_TAR_BUF1 (or APU_PCU_BUCK_ON_DAT0_L)
 *	[31:16] offset to update
 *	[15:00] data to update
 *
 * high 32-bit data for PMIC control
 *	APU_PCU_PMIC_TAR_BUF2 (or APU_PCU_BUCK_ON_DAT0_H)
 *	[2:0] cmd_op, read:0x3 , write:0x7
 *	[7:4] pmic slave id
 */
static void __apu_pcu_init(void)
{
	uint32_t cmd_op_w = 0x7;
	uint32_t slave_id = SUB_PMIC_ID;
	uint32_t en_set_offset = BUCK_VAPU_PMIC_REG_EN_SET_ADDR;
	uint32_t en_clr_offset = BUCK_VAPU_PMIC_REG_EN_CLR_ADDR;
	uint32_t en_shift = BUCK_VAPU_PMIC_REG_EN_SHIFT;

	pr_info("PCU init %s %d ++\n", __func__, __LINE__);

	// Step1. enable auto buck on/off function of command0
	// [0]: cmd0 enable auto ON, [4]: cmd0 enable auto OFF
	apu_writel(0x11, apupw.regs[apu_pcu] + APU_PCU_BUCK_STEP_SEL);

	// Step2. fill-in command0 for vapu auto buck ON
	apu_writel((en_set_offset << 16) + (0x1 << en_shift),
			apupw.regs[apu_pcu] + APU_PCU_BUCK_ON_DAT0_L);
	apu_writel((slave_id << 4) + cmd_op_w,
			apupw.regs[apu_pcu] + APU_PCU_BUCK_ON_DAT0_H);

	// Step3. fill-in command0 for vapu auto buck OFF
	apu_writel((en_clr_offset << 16) + (0x1 << en_shift),
			apupw.regs[apu_pcu] + APU_PCU_BUCK_OFF_DAT0_L);
	apu_writel((slave_id << 4) + cmd_op_w,
			apupw.regs[apu_pcu] + APU_PCU_BUCK_OFF_DAT0_H);

	// Step4. update buck settle time for vapu by SEL0
	apu_writel(VAPU_BUCK_ON_SETTLE_TIME,
			apupw.regs[apu_pcu] + APU_PCU_BUCK_ON_SLE0);

	pr_info("PCU init %s %d --\n", __func__, __LINE__);
}

static void __apu_rpc_init(void)
{
	pr_info("RPC init %s %d ++\n", __func__, __LINE__);

	// Step7. RPC: memory types (sleep or PD type)
	// RPC: iTCM in uP need to setup to sleep type
	apu_clearl((0x1 << 1), apupw.regs[apu_rpc] + 0x0200);

	// RPC-lite: iTCM in MVPU & MDLA need to setup to sleep type
	// ACX0
	apu_clearl((0x1 << 1), apupw.regs[apu_acx0_rpc_lite] + 0x0208);
	apu_clearl((0x1 << 1), apupw.regs[apu_acx0_rpc_lite] + 0x020C);
	apu_clearl((0x1 << 1), apupw.regs[apu_acx0_rpc_lite] + 0x0210);
	apu_clearl((0x1 << 1), apupw.regs[apu_acx0_rpc_lite] + 0x0214);
	apu_clearl((0x1 << 1), apupw.regs[apu_acx0_rpc_lite] + 0x0218);
	apu_clearl((0x1 << 1), apupw.regs[apu_acx0_rpc_lite] + 0x021C);
	apu_clearl((0x1 << 1), apupw.regs[apu_acx0_rpc_lite] + 0x0220);
	apu_clearl((0x1 << 1), apupw.regs[apu_acx0_rpc_lite] + 0x0224);
	// ACX1
	apu_clearl((0x1 << 1), apupw.regs[apu_acx1_rpc_lite] + 0x0208);
	apu_clearl((0x1 << 1), apupw.regs[apu_acx1_rpc_lite] + 0x020C);
	apu_clearl((0x1 << 1), apupw.regs[apu_acx1_rpc_lite] + 0x0210);
	apu_clearl((0x1 << 1), apupw.regs[apu_acx1_rpc_lite] + 0x0214);
	apu_clearl((0x1 << 1), apupw.regs[apu_acx1_rpc_lite] + 0x0218);
	apu_clearl((0x1 << 1), apupw.regs[apu_acx1_rpc_lite] + 0x021C);
	apu_clearl((0x1 << 1), apupw.regs[apu_acx1_rpc_lite] + 0x0220);
	apu_clearl((0x1 << 1), apupw.regs[apu_acx1_rpc_lite] + 0x0224);

	// Step9. RPCtop initial
	// RPC
	apu_setl(0x0800501E, apupw.regs[apu_rpc] + APU_RPC_TOP_SEL);
	// BUCK_PROT_SEL
	apu_setl((0x1 << 20), apupw.regs[apu_rpc] + APU_RPC_TOP_SEL_1);

	// RPC-Lite
	apu_setl(0x0000009E, apupw.regs[apu_acx0_rpc_lite] + APU_RPC_TOP_SEL);
	apu_setl(0x0000009E, apupw.regs[apu_acx1_rpc_lite] + APU_RPC_TOP_SEL);

	pr_info("RPC init %s %d --\n", __func__, __LINE__);
}

static int __apu_are_init(struct device *dev)
{
	u32 tmp = 0;
	int ret, val = 0;
	int are_id;
	uint32_t are_entry2_cfg_h[] = {0x000F0000, 0x000F0000, 0x000F0000};
	uint32_t are_entry2_cfg_l[] = {0x001F0705, 0x001F0707, 0x001F0707};

	// Step10. ARE initial

	/* TINFO="Wait for sare1 fsm to transition to IDLE" */
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_are2] + 0x48),
			val, (val & 0x1UL),
			50, 10000);
	if (ret) {
		pr_info("%s timeout to wait sram1 fsm to idle, ret %d\n",
				__func__, ret);
		return -1;
	}

	pr_info("ARE init %s %d ++\n", __func__, __LINE__);

	for (are_id = apu_are0 ; are_id <= apu_are2 ; are_id++) {
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
		apu_writel(are_entry2_cfg_h[are_id], apupw.regs[are_id]
						+ APU_ARE_ETRY2_SRAM_H);
		apu_writel(are_entry2_cfg_l[are_id], apupw.regs[are_id]
						+ APU_ARE_ETRY2_SRAM_L);

		/* dummy read ARE entry0/1/2 H/L sram */
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

static void __apu_dump_rpc_status(enum t_acx_id id)
{
	if (id == ACX0) {
		pr_info(
		"%s ACX0 APU_RPC_INTF_PWR_RDY:0x%08x APU_ACX_CONN_CG_CON:0x%08x\n",
		__func__,
		apu_readl(apupw.regs[apu_acx0_rpc_lite] + APU_RPC_INTF_PWR_RDY),
		apu_readl(apupw.regs[apu_acx0] + APU_ACX_CONN_CG_CON));

	} else if (id == ACX1) {
		pr_info(
		"%s ACX1 APU_RPC_INTF_PWR_RDY:0x%08x APU_ACX_CONN_CG_CON:0x%08x\n",
		__func__,
		apu_readl(apupw.regs[apu_acx1_rpc_lite] + APU_RPC_INTF_PWR_RDY),
		apu_readl(apupw.regs[apu_acx1] + APU_ACX_CONN_CG_CON));

	} else {
		pr_info(
		"%s RCX APU_RPC_INTF_PWR_RDY:0x%08x APU_VCORE_CG_CON:0x%08x APU_RCX_CG_CON:0x%08x\n",
		__func__,
		apu_readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
		apu_readl(apupw.regs[apu_vcore] + APUSYS_VCORE_CG_CON),
		apu_readl(apupw.regs[apu_rcx] + APU_RCX_CG_CON));
	}
}

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

	// FIXME : remove thie after SB
	// may need config this in FPGS environment
	pr_info("%s step4. ignore slp prot\n", __func__);
	regValue = 0x0;
	regValue = apu_readl(apupw.regs[apu_rpc] + 0x140);
	regValue |= (0x1 << 13);
	apu_writel(regValue, apupw.regs[apu_rpc] + 0x140);
	udelay(10);

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

static int __apu_wake_rpc_rcx(struct device *dev)
{
	int ret = 0, val = 0;

	/* wake up RPC */
	apu_writel(0x00000100, apupw.regs[apu_rpc] + APU_RPC_TOP_CON);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL),
			50, 10000);
	if (ret) {
		pr_info("%s wake up apu_rpc fail, ret %d\n", __func__, ret);
		goto out;
	}

	dev_info(dev, "%s RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	/* clear vcore/rcx cgs */
	pr_info("clear vcore/rcx cgs %s %d \n", __func__, __LINE__);
	apu_writel(0xFFFFFFFF, apupw.regs[apu_vcore] + APUSYS_VCORE_CG_CLR);
	apu_writel(0xFFFFFFFF, apupw.regs[apu_rcx] + APU_RCX_CG_CLR);

	dev_info(dev, "%s APUSYS_VCORE_CG_CON 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_vcore] + APUSYS_VCORE_CG_CON),
			readl(apupw.regs[apu_vcore] + APUSYS_VCORE_CG_CON));

	dev_info(dev, "%s APU_RCX_CG_CON 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_rcx] + APU_RCX_CG_CON),
			readl(apupw.regs[apu_rcx] + APU_RCX_CG_CON));
out:
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
	} else {
		rpc_lite_base = apu_acx1_rpc_lite;
		acx_base = apu_acx1;
	}

	dev_info(dev, "%s ctl p1:%d p2:%d\n",
			__func__, rpc_lite_base, acx_base);

	/* wake acx rpc lite */
	apu_writel(0x00000100, apupw.regs[rpc_lite_base] + APU_RPC_TOP_CON);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL),
			50, 10000);

	/* polling FSM @RPC-lite to ensure RPC is in on/off stage */
	ret |= readl_relaxed_poll_timeout_atomic(
			(apupw.regs[rpc_lite_base] + APU_RPC_STATUS),
			val, (val & (0x1 << 29)),
			50, 10000);
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
			val, (val & dev_mtcmos_chk) == dev_mtcmos_chk,
			50, 200);
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

static int mt6983_apu_top_on(struct device *dev)
{
	pr_info("%s +\n", __func__);

	if (ref_count++ > 0)
		return 0;

// FIXME: remove this since it should be auto ctl by RPC flow
#if !CFG_FPGA
	plt_pwr_res_ctl(1);
#endif

	__apu_wake_rpc_rcx(dev);

	pr_info("%s -\n", __func__);
	return 0;
}

static int mt6983_apu_top_off(struct device *dev)
{
	pr_info("%s +\n", __func__);

	// FIXME: remove this power off flow since it should be triggered by uP

	if (--ref_count > 0)
		return 0;

	__apu_sleep_rpc_rcx(dev);

#if !CFG_FPGA
	plt_pwr_res_ctl(0);
#endif
	pr_info("%s -\n", __func__);
	return 0;
}

static void __apu_aoc_init(void)
{
	pr_info("AOC init %s %d ++\n", __func__, __LINE__);

#if !CFG_FPGA
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
#endif

	// Step3. Roll back to APU Buck on stage
	// 	The following setting need to in order
	// 	and wait 1uS before setup next control signal
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

#if !CFG_FPGA
static void __apu_buck_off(void)
{
	// Step11. Roll back to Buck off stage
	// a. Setup Buck control signal
	// 	 The following setting need to in order,
	//  	 and wait 1uS before setup next control signal
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
	// set vapu to 0.6v
	regulator_set_voltage(vapu_reg_id, 600000, 600000);

	// disable vapu buck
	regulator_disable(vapu_reg_id);
}
#endif

static int init_hw_setting(struct device *dev)
{
	__apu_aoc_init();
#if !CFG_FPGA
	__apu_pll_init();
	__apu_acc_init();
#endif
	__apu_pcu_init();
	__apu_rpc_init();
	__apu_are_init(dev);

#ifdef AGING_MARGIN
	// TODO: update aging param into spare register,
	// then uP shall read it to aware need to handle aging or not
#endif

#if 0
	//*APU_ACC_CLK_EN_CLR = (0x1<<1);
	apu_writel((0x1<<1), apupw.regs[apu_acc] + 0xe4);

	//*EXT_BUCK_ISO &= ~(0x1<<0);
	value = apu_readl(apupw.regs[sys_spm] + 0x3ec);
	apu_writel(value & ~(0x1<<0), apupw.regs[sys_spm] + 0x3ec);
#endif

#if !CFG_FPGA
	__apu_buck_off();
#endif
	return 0;
}

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
		} else
			pr_info("%s: get resource \"%s\" pass\n",
					__func__, reg_name[idx]);

		apupw.regs[idx] = ioremap(res->start,
				res->end - res->start + 1);

		if (IS_ERR_OR_NULL(apupw.regs[idx])) {
			pr_info("%s: %s remap base fail\n",
					__func__, reg_name[idx]);
			return -ENOMEM;
		} else
			pr_info("%s: %s remap base 0x%x to 0x%x\n",
					__func__, reg_name[idx],
					res->start, apupw.regs[idx]);

		apupw.phy_addr[idx] = res->start;
	}

	return 0;
}

static int mt6983_apu_top_pb(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("%s fpga_type : %d\n", __func__, fpga_type);

	init_reg_base(pdev);

#if !CFG_FPGA
	init_plat_pwr_res(pdev);
#endif
	init_hw_setting(&pdev->dev);

#if APU_POWER_BRING_UP

#if !CFG_FPGA
	__apu_on_mdla_mvpu_clk();
#endif
	switch (fpga_type) {
	default:
	case 0:
		pr_info("%s bypass pre-power-ON\n", __func__);
		break;
	case 1:
		mt6983_apu_top_on(&pdev->dev);
		__apu_wake_rpc_acx(&pdev->dev, ACX0);
		__apu_wake_rpc_acx(&pdev->dev, ACX1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, VPU0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, VPU0, 1);
		break;
	case 2:
		mt6983_apu_top_on(&pdev->dev);
		__apu_wake_rpc_acx(&pdev->dev, ACX0);
		__apu_wake_rpc_acx(&pdev->dev, ACX1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, VPU0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, DLA0, 1);
		break;
	case 3:
		mt6983_apu_top_on(&pdev->dev);
		__apu_wake_rpc_acx(&pdev->dev, ACX0);
		__apu_wake_rpc_acx(&pdev->dev, ACX1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, DLA0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, DLA0, 1);
		break;
	}
#endif // APU_POWER_BRING_UP

	aputop_opp_limiter_init(apupw.regs[apu_md32_mbox]);
	aputop_dump_pwr_res();

	return ret;
}

static int mt6983_apu_top_rm(struct platform_device *pdev)
{
	int idx;

	pr_info("%s +\n", __func__);

#if !CFG_FPGA
	destroy_plat_pwr_res();
#endif

	for (idx = 0; idx < APUPW_MAX_REGS; idx++)
		iounmap(apupw.regs[idx]);

	pr_info("%s -\n", __func__);

	return 0;
}

static void aputop_dump_pwr_res(void)
{
#if !CFG_FPGA
	uint32_t vapu = 0;
	uint32_t vcore = 0;
	uint32_t vsram = 0;

	if (vapu_reg_id)
		vapu = regulator_get_voltage(vapu_reg_id);

	if (vcore_reg_id)
		vcore = regulator_get_voltage(vcore_reg_id);

	if (vsram_reg_id)
		vsram = regulator_get_voltage(vsram_reg_id);

	pr_info("%s dsp_freq:%d ipuif_freq:%d \n",
			__func__,
			clk_get_rate(clk_top_dsp_sel),
			clk_get_rate(clk_top_ipu_if_sel));

	pr_info("%s vapu:%u vcore:%u vsram:%u\n",
			__func__, vapu, vcore, vsram);
#endif

	__apu_dump_rpc_status(RCX);

#if APU_POWER_BRING_UP
	__apu_dump_rpc_status(ACX0);
	__apu_dump_rpc_status(ACX1);
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
	case APUTOP_FUNC_PWR_CFG:
		aputop_pwr_cfg(aputop);
		break;
	case APUTOP_FUNC_PWR_UT:
		aputop_pwr_ut(aputop);
		break;
	case APUTOP_FUNC_OPP_LIMIT_HAL:
		aputop_opp_limit(aputop, OPP_LIMIT_HAL);
		break;
	case APUTOP_FUNC_OPP_LIMIT_DBG:
		aputop_opp_limit(aputop, OPP_LIMIT_DEBUG);
		break;
	case APUTOP_FUNC_CURR_STATUS:
		aputop_curr_status(aputop);
		break;
	case APUTOP_FUNC_DUMP_REG:
		aputop_dump_pwr_res();
#if DEBUG_DUMP_REG
		aputop_dump_all_reg();
#endif
		break;
	default :
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
	.plat_aputop_func = mt6983_apu_top_func,
#if IS_ENABLED(CONFIG_DEBUG_FS)
	.plat_aputop_dbg_open = mt6983_apu_top_dbg_open,
	.plat_aputop_dbg_write = mt6983_apu_top_dbg_write,
#endif
	.bypass_pwr_on = 0,
	.bypass_pwr_off = 0,
};
