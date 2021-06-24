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
#include "mt6893_apupwr.h"

/* Below reg_name has to 1-1 mapping DTS's name */
static const char *reg_name[APUPW_MAX_REGS] = {
	"sys_spm", "apu_vcore", "apu_conn", "apu_rpc",
};

static struct apu_power apupw;

/* regulator id */
static struct regulator *vvpu_reg_id;
static struct regulator *vcore_reg_id;
static struct regulator *vsram_reg_id;
/* apu_top preclk */
static struct clk *clk_top_dsp_sel;		/* CONN */
static struct clk *clk_top_ipu_if_sel;		/* VCORE */

static unsigned int ref_count;

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

static int mt6893_apu_top_on(struct device *dev)
{
	int ret, tmp;
	int ret_clk = 0;

	/* pr_info("%s +\n", __func__); */

	if (ref_count++ > 0)
		return 0;

	ret = regulator_enable(vvpu_reg_id);
	if (ret < 0)
		return ret;

	ret = regulator_enable(vsram_reg_id);
	if (ret < 0)
		return ret;

	/* pr_info("%s SPM: Release IPU external buck iso\n", __func__); */
	apu_writel(apu_readl(apupw.regs[sys_spm] + APUSYS_BUCK_ISOLATION)
		& ~(0x00000021), apupw.regs[sys_spm] + APUSYS_BUCK_ISOLATION);

	/* pr_info("%s clk_enable (pre_clk)\n", __func__); */
	ENABLE_CLK(clk_top_ipu_if_sel);
	ENABLE_CLK(clk_top_dsp_sel);
	if (ret_clk < 0)
		return ret_clk;

	// pr_info("%s SPM:  subsys power on (APU_CONN/APU_VCORE)\n", __func__);
	apu_writel(apu_readl(
			apupw.regs[sys_spm] + APUSYS_SPM_CROSS_WAKE_M01_REQ)
			| APMCU_WAKEUP_APU,
			apupw.regs[sys_spm] + APUSYS_SPM_CROSS_WAKE_M01_REQ);

	/* pr_info("%s SPM: wait until PWR_ACK = 1\n", __func__); */
	ret = readl_poll_timeout(
			apupw.regs[sys_spm] + APUSYS_OTHER_PWR_STATUS,
			tmp, (tmp & (0x1UL << 5)) == (0x1UL << 5),
			MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0)
		return ret;

	/* pr_info("%s RPC: wait until PWR ON complete = 1\n", __func__); */
	ret = readl_poll_timeout(
			apupw.regs[apu_rpc] + APUSYS_RPC_INTF_PWR_RDY,
			tmp, (tmp & (0x1UL << 0)) == (0x1UL << 0),
			MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0)
		return ret;

	/* pr_info("%s bus/sleep protect CG on\n", __func__); */
	apu_writel(0xFFFFFFFF, apupw.regs[apu_vcore] + APUSYS_VCORE_CG_CLR);
	apu_writel(0xFFFFFFFF, apupw.regs[apu_conn] + APUSYS_CONN_CG_CLR);

	pr_info(
		"%s spm_wake_bit: 0x%x, rpcValue: 0x%x\n",
		__func__,
		apu_readl(apupw.regs[sys_spm] + APUSYS_SPM_CROSS_WAKE_M01_REQ),
		apu_readl(apupw.regs[apu_rpc] + APUSYS_RPC_INTF_PWR_RDY));

	/* pr_info("%s -\n", __func__); */
	return 0;
}

static int mt6893_apu_top_off(struct device *dev)
{
	int ret, tmp;

	/* pr_info("%s +\n", __func__); */

	if (ref_count > 0)
		ref_count--;
	else
		return 0;

	/* pr_info("%s bus/sleep protect CG on\n", __func__); */
	apu_writel(0xFFFFFFFF, apupw.regs[apu_vcore] + APUSYS_VCORE_CG_CLR);
	apu_writel(0xFFFFFFFF, apupw.regs[apu_conn] + APUSYS_CONN_CG_CLR);

	// pr_info("%s SPM: subsys power off (APU_CONN/APU_VCORE)\n", __func__);
	apu_writel(apu_readl(
			apupw.regs[sys_spm] + APUSYS_SPM_CROSS_WAKE_M01_REQ)
			& ~APMCU_WAKEUP_APU,
			apupw.regs[sys_spm] + APUSYS_SPM_CROSS_WAKE_M01_REQ);

	/* pr_info("%s RPC:  sleep request enable\n", __func__); */
	apu_writel(apu_readl(apupw.regs[apu_rpc] + APUSYS_RPC_TOP_CON) | 0x1,
			apupw.regs[apu_rpc] + APUSYS_RPC_TOP_CON);

	/* pr_info("%s RPC: wait until PWR down complete = 0\n", __func__); */
	ret = readl_poll_timeout(
			apupw.regs[apu_rpc] + APUSYS_RPC_INTF_PWR_RDY,
			tmp, (tmp & (0x1UL << 0)) == 0x0,
			MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0)
		return ret;

	/* pr_info("%s SPM: wait until PWR_ACK = 0\n", __func__); */
	ret = readl_poll_timeout(
			apupw.regs[sys_spm] + APUSYS_OTHER_PWR_STATUS,
			tmp, (tmp & (0x1UL << 5)) == 0x0,
			MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0)
		return ret;

	pr_info(
		"%s spm_wake_bit: 0x%x, rpcValue: 0x%x\n",
		__func__,
		apu_readl(apupw.regs[sys_spm] + APUSYS_SPM_CROSS_WAKE_M01_REQ),
		apu_readl(apupw.regs[apu_rpc] + APUSYS_RPC_INTF_PWR_RDY));

	/* pr_info("%s scpsys_clk_disable (pre_clk)\n", __func__); */
	DISABLE_CLK(clk_top_ipu_if_sel);
	DISABLE_CLK(clk_top_dsp_sel);

	/* pr_info("%s SPM: Enable IPU external buck iso\n", __func__); */
	apu_writel(apu_readl(
		apupw.regs[sys_spm] + APUSYS_BUCK_ISOLATION) & 0x00000021,
		apupw.regs[sys_spm] + APUSYS_BUCK_ISOLATION);

	ret = regulator_disable(vsram_reg_id);
	if (ret < 0)
		return ret;

	ret = regulator_disable(vvpu_reg_id);
	if (ret < 0)
		return ret;

	/* pr_info("%s -\n", __func__); */
	return 0;
}

static int init_hw_setting(void* param)
{
	unsigned int regValue = 0x0;

	/*
	 * set memory type to PD or sleep group
	 * sw_type register for each memory group, set to PD mode default
	 */
	apu_writel(0xFF, apupw.regs[apu_rpc] + APUSYS_RPC_SW_TYPE0); // APUTOP
	apu_writel(0x7, apupw.regs[apu_rpc] + APUSYS_RPC_SW_TYPE2); // VPU0
	apu_writel(0x7, apupw.regs[apu_rpc] + APUSYS_RPC_SW_TYPE3); // VPU1
	apu_writel(0x7, apupw.regs[apu_rpc] + APUSYS_RPC_SW_TYPE4); // VPU1
	apu_writel(0x7, apupw.regs[apu_rpc] + APUSYS_RPC_SW_TYPE6); // MDLA0
	apu_writel(0x7, apupw.regs[apu_rpc] + APUSYS_RPC_SW_TYPE7); // MDLA1

	// mask RPC IRQ and bypass WFI
	regValue = apu_readl(apupw.regs[apu_rpc] + APUSYS_RPC_TOP_SEL);
	regValue |= 0x9E;
	regValue |= BIT(10);
	apu_writel(regValue, apupw.regs[apu_rpc] + APUSYS_RPC_TOP_SEL);

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

static int init_plat_pwr_res(struct platform_device *pdev)
{
	int ret = 0;
	int ret_clk = 0;

	//Vvpu Buck
	vvpu_reg_id = regulator_get(&pdev->dev, "vvpu");
	if (!vvpu_reg_id) {
		pr_info("regulator_get vvpu_reg_id failed\n");
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

static int mt6893_apu_top_pb(struct platform_device *pdev)
{
	init_reg_base(pdev);
	init_plat_pwr_res(pdev);

	// FIXME: remove me since there is no need to power on before hw init
	pm_runtime_get_sync(&pdev->dev);

	init_hw_setting(NULL);

	// FIXME: remove me since there is no need to power on before hw init
	pm_runtime_put_sync(&pdev->dev);

	/* pr_info("%s -\n", __func__); */

	return 0;

}

static int mt6893_apu_top_rm(struct platform_device *pdev)
{
	int idx;

	pr_info("%s +\n", __func__);

	UNPREPARE_CLK(clk_top_dsp_sel);
	UNPREPARE_CLK(clk_top_ipu_if_sel);

	regulator_put(vvpu_reg_id);
	regulator_put(vsram_reg_id);
	vvpu_reg_id = NULL;
	vsram_reg_id = NULL;

	for (idx = 0; idx < APUPW_MAX_REGS; idx++)
		iounmap(apupw.regs[idx]);

	pr_info("%s -\n", __func__);

	return 0;
}

static void aputop_dump_pwr_res(void)
{
	unsigned int vvpu = 0;
	unsigned int vcore = 0;
	unsigned int vsram = 0;

	if (vvpu_reg_id)
		vvpu = regulator_get_voltage(vvpu_reg_id);

	if (vcore_reg_id)
		vcore = regulator_get_voltage(vcore_reg_id);

	if (vsram_reg_id)
		vsram = regulator_get_voltage(vsram_reg_id);

	pr_info("%s dsp_freq:%d ipuif_freq:%d \n",
			__func__,
			clk_get_rate(clk_top_dsp_sel),
			clk_get_rate(clk_top_ipu_if_sel));

	pr_info("%s vvpu:%u vcore:%u vsram:%u\n",
			__func__, vvpu, vcore, vsram);

	pr_info(
	"%s APU_RPC_INTF_PWR_RDY:0x%08x APU_VCORE_CG_CON:0x%08x APU_CONN_CG_CON:0x%08x\n",
		__func__,
		apu_readl(apupw.regs[apu_rpc] + APUSYS_RPC_INTF_PWR_RDY),
		apu_readl(apupw.regs[apu_vcore] + APUSYS_VCORE_CG_CON),
		apu_readl(apupw.regs[apu_conn] + APUSYS_CONN_CG_CON));
}

static int mt6893_apu_top_func(struct platform_device *pdev,
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

const struct apupwr_plat_data mt6893_plat_data = {
	.plat_name = "mt6893_apupwr",
	.plat_aputop_on = mt6893_apu_top_on,
	.plat_aputop_off = mt6893_apu_top_off,
	.plat_aputop_pb = mt6893_apu_top_pb,
	.plat_aputop_rm = mt6893_apu_top_rm,
	.plat_aputop_func = mt6893_apu_top_func,
	.bypass_pwr_on = APU_POWER_BRING_UP,
	.bypass_pwr_off = APU_POWER_BRING_UP,
};
