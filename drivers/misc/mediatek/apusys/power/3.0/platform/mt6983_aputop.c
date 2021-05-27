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
	"apu_rpc", "apu_pcu", "apu_ao_ctl", "apu_acc",
	"apu_are0", "apu_are1", "apu_are2",
	"apu_acx0", "apu_acx0_rpc_lite", "apu_acx1", "apu_acx1_rpc_lite",
};

static struct apu_power apupw;
static unsigned int ref_count;

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

	//vapu Buck
	vapu_reg_id = regulator_get(&pdev->dev, "vapu");
	if (!vapu_reg_id) {
		pr_info("regulator_get vapu_reg_id failed\n");
		return -ENOENT;
	}

	//Vcore
	vcore_reg_id = regulator_get(&pdev->dev, "vcore");
	if (!vcore_reg_id) {
		pr_info("regulator_get vcore_reg_id failed\n");
		return -ENOENT;
	}

	//Vsram
	vsram_reg_id = regulator_get(&pdev->dev, "vsram");
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

#endif

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

static void __apu_clk_init(void)
{
	char buf[32];

	/* mnoc clk setting */
	pr_info("mnoc clk setting %s %d \n", __func__, __LINE__);
	apu_writel(0x00000004, apupw.regs[apu_acc] + APU_ACC_CONFG_CLR0);
	apu_writel(0x00008000, apupw.regs[apu_acc] + APU_ACC_CONFG_SET0);

	/* iommu clk setting */
	pr_info("iommu clk setting %s %d \n", __func__, __LINE__);
	apu_writel(0x00000004, apupw.regs[apu_acc] + APU_ACC_CONFG_CLR1);
	apu_writel(0x00008000, apupw.regs[apu_acc] + APU_ACC_CONFG_SET1);

	/* mvpu clk setting */
	pr_info("mvpu clk setting %s %d \n", __func__, __LINE__);
	apu_writel(0x00000004, apupw.regs[apu_acc] + APU_ACC_CONFG_CLR2);
	apu_writel(0x00008000, apupw.regs[apu_acc] + APU_ACC_CONFG_SET2);
	apu_writel(0x00000100, apupw.regs[apu_acc] + APU_ACC_AUTO_CTRL_SET2);

	/* mdla clk setting */
	pr_info("mdla clk setting %s %d \n", __func__, __LINE__);
	apu_writel(0x00000004, apupw.regs[apu_acc] + APU_ACC_CONFG_CLR3);
	apu_writel(0x00008000, apupw.regs[apu_acc] + APU_ACC_CONFG_SET3);
	apu_writel(0x00000100, apupw.regs[apu_acc] + APU_ACC_AUTO_CTRL_SET3);

	/* clk invert setting */
	pr_info("clk invert setting %s %d \n", __func__, __LINE__);
	apu_writel(0x00000008, apupw.regs[apu_acc] + APU_ACC_CLK_INV_EN_SET);
	apu_writel(0x00000020, apupw.regs[apu_acc] + APU_ACC_CLK_INV_EN_SET);
	apu_writel(0x00000080, apupw.regs[apu_acc] + APU_ACC_CLK_INV_EN_SET);
	apu_writel(0x00000200, apupw.regs[apu_acc] + APU_ACC_CLK_INV_EN_SET);
	apu_writel(0x00000800, apupw.regs[apu_acc] + APU_ACC_CLK_INV_EN_SET);
	apu_writel(0x00002000, apupw.regs[apu_acc] + APU_ACC_CLK_INV_EN_SET);
	apu_writel(0x00008000, apupw.regs[apu_acc] + APU_ACC_CLK_INV_EN_SET);

	memset(buf, 0, sizeof(buf));
	snprintf(buf, 32, "phys 0x%08x: ", (u32)(apupw.phy_addr[apu_acc]));
	print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
			apupw.regs[apu_acc], 0x100, true);
}

static void __apu_rpc_init(void)
{
	apu_writel(0x00000800, apupw.regs[apu_rpc] + APU_RPC_HW_CON);
	apu_writel(0x00001000, apupw.regs[apu_rpc] + APU_RPC_HW_CON);
	apu_writel(0x00008000, apupw.regs[apu_rpc] + APU_RPC_HW_CON);
	apu_writel(0x00000080, apupw.regs[apu_rpc] + APU_RPC_HW_CON);

	/* rpc initial */
	apu_setl(0x0800501E, apupw.regs[apu_rpc] + APU_RPC_TOP_SEL);
	apu_setl(0x00100000, apupw.regs[apu_rpc] + APU_RPC_TOP_SEL_1);
	apu_setl(0x0000009E, apupw.regs[apu_acx0_rpc_lite] + APU_RPC_TOP_SEL);
	apu_setl(0x0000009E, apupw.regs[apu_acx1_rpc_lite] + APU_RPC_TOP_SEL);
}

static void __apu_are_init(struct device *dev)
{
	char buf[32];
	u32 tmp = 0;

	/* ARE entry 0 initial */
	pr_info("ARE init %s %d \n", __func__, __LINE__);
	apu_writel(0x01234567, apupw.regs[apu_are0] + APU_ARE_ETRY0_SRAM_H);
	apu_writel(0x89ABCDEF, apupw.regs[apu_are0] + APU_ARE_ETRY0_SRAM_L);
	apu_writel(0x01234567, apupw.regs[apu_are1] + APU_ARE_ETRY0_SRAM_H);
	apu_writel(0x89ABCDEF, apupw.regs[apu_are1] + APU_ARE_ETRY0_SRAM_L);
	apu_writel(0x01234567, apupw.regs[apu_are2] + APU_ARE_ETRY0_SRAM_H);
	apu_writel(0x89ABCDEF, apupw.regs[apu_are2] + APU_ARE_ETRY0_SRAM_L);

	/* ARE entry 1 initial */
	apu_writel(0xFEDCBA98, apupw.regs[apu_are0] + APU_ARE_ETRY1_SRAM_H);
	apu_writel(0x76543210, apupw.regs[apu_are0] + APU_ARE_ETRY1_SRAM_L);
	apu_writel(0xFEDCBA98, apupw.regs[apu_are1] + APU_ARE_ETRY1_SRAM_H);
	apu_writel(0x76543210, apupw.regs[apu_are1] + APU_ARE_ETRY1_SRAM_L);
	apu_writel(0xFEDCBA98, apupw.regs[apu_are2] + APU_ARE_ETRY1_SRAM_H);
	apu_writel(0x76543210, apupw.regs[apu_are2] + APU_ARE_ETRY1_SRAM_L);

	/* ARE entry 2 initial */
	apu_writel(0x000F0000, apupw.regs[apu_are0] + APU_ARE_ETRY2_SRAM_H);
	apu_writel(0x001F0705, apupw.regs[apu_are0] + APU_ARE_ETRY2_SRAM_L);
	apu_writel(0x000F0000, apupw.regs[apu_are1] + APU_ARE_ETRY2_SRAM_H);
	apu_writel(0x001F0707, apupw.regs[apu_are1] + APU_ARE_ETRY2_SRAM_L);
	apu_writel(0x000F0000, apupw.regs[apu_are2] + APU_ARE_ETRY2_SRAM_H);
	apu_writel(0x001F0706, apupw.regs[apu_are2] + APU_ARE_ETRY2_SRAM_L);

	/* dummy read ARE entry0/1/2 H/L sram */
	tmp = readl(apupw.regs[apu_are0] + APU_ARE_ETRY2_SRAM_H);
	tmp = readl(apupw.regs[apu_are0] + APU_ARE_ETRY2_SRAM_L);
	tmp = readl(apupw.regs[apu_are1] + APU_ARE_ETRY2_SRAM_H);
	tmp = readl(apupw.regs[apu_are1] + APU_ARE_ETRY2_SRAM_L);
	tmp = readl(apupw.regs[apu_are2] + APU_ARE_ETRY2_SRAM_H);
	tmp = readl(apupw.regs[apu_are2] + APU_ARE_ETRY2_SRAM_L);

	/* update ARE sram */
	pr_info("update ARE sram %s %d \n", __func__, __LINE__);
	apu_writel(0x00000004, apupw.regs[apu_are0] + APU_ARE_INI_CTRL);
	apu_writel(0x00000004, apupw.regs[apu_are1] + APU_ARE_INI_CTRL);
	apu_writel(0x00000004, apupw.regs[apu_are2] + APU_ARE_INI_CTRL);

	dev_info(dev, "%s ARE phys 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_are0] + APU_ARE_ETRY2_SRAM_H),
			readl(apupw.regs[apu_are0] + APU_ARE_ETRY2_SRAM_H));

	dev_info(dev, "%s ARE phys 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_are0] + APU_ARE_ETRY2_SRAM_L),
			readl(apupw.regs[apu_are0] + APU_ARE_ETRY2_SRAM_H));

	dev_info(dev, "%s ARE phys 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_are1] + APU_ARE_ETRY2_SRAM_H),
			readl(apupw.regs[apu_are1] + APU_ARE_ETRY2_SRAM_H));

	dev_info(dev, "%s ARE phys 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_are1] + APU_ARE_ETRY2_SRAM_L),
			readl(apupw.regs[apu_are1] + APU_ARE_ETRY2_SRAM_H));

	dev_info(dev, "%s ARE phys 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_are2] + APU_ARE_ETRY2_SRAM_H),
			readl(apupw.regs[apu_are2] + APU_ARE_ETRY2_SRAM_H));

	dev_info(dev, "%s ARE phys 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_are2] + APU_ARE_ETRY2_SRAM_L),
			readl(apupw.regs[apu_are2] + APU_ARE_ETRY2_SRAM_H));

	memset(buf, 0, sizeof(buf));
	snprintf(buf, 32, "phys 0x%08x: ", (u32)(apupw.phy_addr[apu_are0]));
	print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
			apupw.regs[apu_are0], 0x50, true);

	memset(buf, 0, sizeof(buf));
	snprintf(buf, 32, "phys 0x%08x: ", (u32)(apupw.phy_addr[apu_are1]));
	print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
			apupw.regs[apu_are1], 0x50, true);

	memset(buf, 0, sizeof(buf));
	snprintf(buf, 32, "phys 0x%08x: ", (u32)(apupw.phy_addr[apu_are2]));
	print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
			apupw.regs[apu_are2], 0x50, true);
}

static int __apu_wake_rpc_rcx(struct device *dev)
{
	int ret = 0, val = 0;

	/* wake up RPC */
	apu_writel(0x00000100, apupw.regs[apu_rpc] + APU_RPC_TOP_CON);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL),
			50, 200);

	if (ret) {
		pr_info("%s wake up apu_rpc fail, ret %d\n", __func__, ret);
		goto out;
	}

	dev_info(dev, "%s APU_RPC_INTF_PWR_RDY phys 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	/* clear vcore/rcx cgs */
	pr_info("clear vcore/rcx cgs %s %d \n", __func__, __LINE__);
	apu_writel(0xFFFFFFFF, apupw.regs[apu_vcore] + APUSYS_VCORE_CG_CLR);
	apu_writel(0xFFFFFFFF, apupw.regs[apu_rcx] + APU_RCX_CG_CLR);

	dev_info(dev, "%s VCORE phys 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_vcore] + APUSYS_VCORE_CG_CON),
			readl(apupw.regs[apu_vcore] + APUSYS_VCORE_CG_CON));

	dev_info(dev, "%s RCX phys 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_rcx] + APU_RCX_CG_CON),
			readl(apupw.regs[apu_rcx] + APU_RCX_CG_CON));
out:
	return ret;
}

static int __apu_wake_rpc_acx(struct device *dev)
{
	int ret = 0, val = 0;

	/* wake acx0 rpc lite */
	apu_writel(0x00000100, apupw.regs[apu_acx0_rpc_lite] + APU_RPC_TOP_CON);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acx0_rpc_lite] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL),
			50, 200);
	if (ret) {
		pr_info("%s wake up acx0_rpc fail, ret %d\n", __func__, ret);
		goto out;
	}

	/* wake acx1 rpc lite */
	apu_writel(0x00000100, apupw.regs[apu_acx1_rpc_lite] + APU_RPC_TOP_CON);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acx1_rpc_lite] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL),
			50, 200);
	if (ret) {
		pr_info("%s wake up acx1_rpc fail, ret %d\n", __func__, ret);
		goto out;
	}

	dev_info(dev, "%s APU_RPC_INTF_PWR_RDY phys 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	dev_info(dev, "%s apu_acx0_rpc phys 0x%x = 0x%x\n",
		__func__,
		(u32)(apupw.phy_addr[apu_acx0_rpc_lite] + APU_RPC_INTF_PWR_RDY),
		readl(apupw.regs[apu_acx0_rpc_lite] + APU_RPC_INTF_PWR_RDY));

	dev_info(dev, "%s apu_acx1_rpc phys 0x%x = 0x%x\n",
		__func__,
		(u32)(apupw.phy_addr[apu_acx1_rpc_lite] + APU_RPC_INTF_PWR_RDY),
		readl(apupw.regs[apu_acx1_rpc_lite] + APU_RPC_INTF_PWR_RDY));

	/* clear acx0/1 CGs */
	apu_writel(0xFFFFFFFF, apupw.regs[apu_acx0] + APU_ACX_CONN_CG_CLR);
	apu_writel(0xFFFFFFFF, apupw.regs[apu_acx1] + APU_ACX_CONN_CG_CLR);

	dev_info(dev, "%s apu_acx0 phys 0x%x = 0x%x\n",
		__func__,
		(u32)(apupw.phy_addr[apu_acx0] + APU_ACX_CONN_CG_CON),
		readl(apupw.regs[apu_acx0] + APU_ACX_CONN_CG_CON));

	dev_info(dev, "%s apu_acx1_rpc phys 0x%x = 0x%x\n",
		__func__,
		(u32)(apupw.phy_addr[apu_acx1] + APU_ACX_CONN_CG_CON),
		readl(apupw.regs[apu_acx1] + APU_ACX_CONN_CG_CON));

out:
	return ret;
}

static int __apu_on_mdla_mvpu_clk(void)
{
	int ret = 0, val = 0;

	/* turn on mvpu root clk src */
	apu_writel(0x00000200, apupw.regs[apu_acc] + APU_ACC_AUTO_CTRL_SET2);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acc] + APU_ACC_AUTO_STATUS2),
			val, (val & 0x20UL) == 0x20UL,
			50, 200);
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
			50, 200);
	if (ret) {
		pr_info("%s turn on mdla root clk fail, ret %d\n",
				__func__, ret);
		goto out;
	}

out:
	return ret;
}

static int __apu_wake_up_acx0_engines(struct device *dev)
{
	int ret = 0, val = 0;

	/* config acx0 rpc lite */
	apu_writel(0x00000012,
			apupw.regs[apu_acx0_rpc_lite] + APU_RPC_SW_FIFO_WE);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acx0_rpc_lite] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x4UL) == 0x4UL,
			50, 200);
	if (ret) {
		pr_info("%s config acx0_rpc 0x12 fail, ret %d\n", __func__, ret);
		goto out;
	}

	/* config acx0 rpc lite */
	apu_writel(0x00000013,
			apupw.regs[apu_acx0_rpc_lite] + APU_RPC_SW_FIFO_WE);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acx0_rpc_lite] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x8UL) == 0x8UL,
			50, 200);
	if (ret) {
		pr_info("%s config acx0_rpc 0x13 fail, ret %d\n", __func__, ret);
		goto out;
	}

	/* config acx0 rpc lite */
	apu_writel(0x00000014,
			apupw.regs[apu_acx0_rpc_lite] + APU_RPC_SW_FIFO_WE);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acx0_rpc_lite] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x10UL) == 0x10UL,
			50, 200);
	if (ret) {
		pr_info("%s config acx0_rpc 0x14 fail, ret %d\n", __func__, ret);
		goto out;
	}

	/* config acx0 rpc lite */
	apu_writel(0x00000016,
			apupw.regs[apu_acx0_rpc_lite] + APU_RPC_SW_FIFO_WE);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acx0_rpc_lite] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x40UL) == 0x40UL,
			50, 200);
	if (ret) {
		pr_info("%s config acx0_rpc 0x16 fail, ret %d\n", __func__, ret);
		goto out;
	}

	/* config acx0 rpc lite */
	apu_writel(0x00000017,
			apupw.regs[apu_acx0_rpc_lite] + APU_RPC_SW_FIFO_WE);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acx0_rpc_lite] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x80UL) == 0x80UL,
			50, 200);
	if (ret) {
		pr_info("%s config acx0_rpc 0x17 fail, ret %d\n", __func__, ret);
		goto out;
	}

	dev_info(dev, "%s apu_acx0_rpc_lite phys 0x%x = 0x%x\n",
		__func__,
		(u32)(apupw.phy_addr[apu_acx0_rpc_lite] + APU_RPC_INTF_PWR_RDY),
		readl(apupw.regs[apu_acx0_rpc_lite] + APU_RPC_INTF_PWR_RDY));

	apu_writel(0xFFFFFFFF, apupw.regs[apu_acx0] + APU_ACX_MVPU_CG_CLR);
	apu_writel(0xFFFFFFFF, apupw.regs[apu_acx0] + APU_ACX_MDLA0_CG_CLR);
	apu_writel(0xFFFFFFFF, apupw.regs[apu_acx0] + APU_ACX_MDLA1_CG_CLR);

out:
	return ret;
}

static int __apu_wake_up_acx1_engines(struct device *dev)
{
	int ret = 0, val = 0;

	/* config acx1 rpc lite */
	apu_writel(0x00000012,
			apupw.regs[apu_acx1_rpc_lite] + APU_RPC_SW_FIFO_WE);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acx1_rpc_lite] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x4UL) == 0x4UL,
			50, 200);
	if (ret) {
		pr_info("%s config acx1_rpc 0x12 fail, ret %d\n", __func__, ret);
		goto out;
	}

	/* config acx1 rpc lite */
	apu_writel(0x00000013,
			apupw.regs[apu_acx1_rpc_lite] + APU_RPC_SW_FIFO_WE);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acx1_rpc_lite] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x8UL) == 0x8UL,
			50, 200);
	if (ret) {
		pr_info("%s config acx1_rpc 0x13 fail, ret %d\n", __func__, ret);
		goto out;
	}

	/* config acx1 rpc lite */
	apu_writel(0x00000014,
			apupw.regs[apu_acx1_rpc_lite] + APU_RPC_SW_FIFO_WE);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acx1_rpc_lite] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x10UL) == 0x10UL,
			50, 200);
	if (ret) {
		pr_info("%s config acx1_rpc 0x14 fail, ret %d\n", __func__, ret);
		goto out;
	}

	/* config acx1 rpc lite */
	apu_writel(0x00000016,
			apupw.regs[apu_acx1_rpc_lite] + APU_RPC_SW_FIFO_WE);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acx1_rpc_lite] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x40UL) == 0x40UL,
			50, 200);
	if (ret) {
		pr_info("%s config acx1_rpc 0x16 fail, ret %d\n", __func__, ret);
		goto out;
	}

	/* config acx1 rpc lite */
	apu_writel(0x00000017,
			apupw.regs[apu_acx1_rpc_lite] + APU_RPC_SW_FIFO_WE);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_acx1_rpc_lite] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x80UL) == 0x80UL,
			50, 200);
	if (ret) {
		pr_info("%s config acx1_rpc 0x17 fail, ret %d\n", __func__, ret);
		goto out;
	}

	dev_info(dev, "%s apu_acx1_rpc_lite phys 0x%x = 0x%x\n",
		__func__,
		(u32)(apupw.phy_addr[apu_acx1_rpc_lite] + APU_RPC_INTF_PWR_RDY),
		readl(apupw.regs[apu_acx0_rpc_lite] + APU_RPC_INTF_PWR_RDY));

	apu_writel(0xFFFFFFFF, apupw.regs[apu_acx1] + APU_ACX_MVPU_CG_CLR);
	apu_writel(0xFFFFFFFF, apupw.regs[apu_acx1] + APU_ACX_MDLA0_CG_CLR);
	apu_writel(0xFFFFFFFF, apupw.regs[apu_acx1] + APU_ACX_MDLA1_CG_CLR);

out:
	return ret;
}

static int mt6983_apu_top_on(struct device *dev)
{
	/* pr_info("%s +\n", __func__); */

	if (ref_count++ > 0)
		return 0;

#if !CFG_FPGA
	plt_pwr_res_ctl(1);
#endif

	__apu_wake_rpc_rcx(dev);

	/* pr_info("%s -\n", __func__); */
	return 0;
}

static int mt6983_apu_top_off(struct device *dev)
{
	/* pr_info("%s +\n", __func__); */

	if (--ref_count > 0)
		return 0;

#if !CFG_FPGA
	plt_pwr_res_ctl(0);
#endif

	/* pr_info("%s -\n", __func__); */

	return 0;
}

static int init_hw_setting(struct device *dev)
{
	__apu_clk_init();
	__apu_rpc_init();
	__apu_are_init(dev);

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
			pr_info("%s: %s remap base 0x%x pass\n",
					__func__, reg_name[idx], res->start);

		apupw.phy_addr[idx] = res->start;
	}

	return 0;
}

static int mt6983_apu_top_pb(struct platform_device *pdev)
{
	int ret = 0;

	init_reg_base(pdev);
#if !CFG_FPGA
	init_plat_pwr_res(pdev);
#endif
	init_hw_setting(&pdev->dev);

#if APU_POWER_BRING_UP
	mt6983_apu_top_on(&pdev->dev);
	__apu_wake_rpc_acx(&pdev->dev);
	__apu_on_mdla_mvpu_clk();
	__apu_wake_up_acx0_engines(&pdev->dev);
	__apu_wake_up_acx1_engines(&pdev->dev);
#endif

	aputop_opp_limiter_init(apupw.regs[apu_md32_mbox]);

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
	unsigned int vapu = 0;
	unsigned int vcore = 0;
	unsigned int vsram = 0;

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

	pr_info(
	"%s APU_RPC_INTF_PWR_RDY:0x%08x APU_VCORE_CG_CON:0x%08x APU_CONN_CG_CON:0x%08x\n",
		__func__,
		apu_readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
		apu_readl(apupw.regs[apu_vcore] + APUSYS_VCORE_CG_CON),
		apu_readl(apupw.regs[apu_rcx] + APU_RCX_CG_CON));
}

static void aputop_pwr_cfg(struct aputop_func_param *aputop)
{

}

static void aputop_pwr_ut(struct aputop_func_param *aputop)
{

}

static int mt6983_apu_top_func(struct platform_device *pdev,
		enum aputop_func_id func_id, struct aputop_func_param *aputop)
{
	pr_info("%s func_id : %d\n", __func__, aputop->func_id);

	// TODO: add mutex lock here

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

	// TODO: add mutex unlock here

	return 0;
}

const struct apupwr_plat_data mt6983_plat_data = {
	.plat_name = "mt6983_apupwr",
	.plat_apu_top_on = mt6983_apu_top_on,
	.plat_apu_top_off = mt6983_apu_top_off,
	.plat_apu_top_pb = mt6983_apu_top_pb,
	.plat_apu_top_rm = mt6983_apu_top_rm,
	.plat_apu_top_func = mt6983_apu_top_func,
	.bypass_pwr_on = APU_POWER_BRING_UP,
	.bypass_pwr_off = APU_POWER_BRING_UP,
};
