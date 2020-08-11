// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>

#include "../../../../soc/mediatek/mtk-scpsys.h"
#include "apu_top.h"

#define MTK_POLL_DELAY_US   10
#define MTK_POLL_TIMEOUT    USEC_PER_SEC

/* regulator id */
static struct regulator *vvpu_reg_id;
/* apu_top preclk */
static struct clk *clk_top_dsp_sel;		/* CONN */
static struct clk *clk_top_ipu_if_sel;		/* VCORE */

static unsigned int ref_count;

static int apu_top_on(void)
{
	int ret, tmp;
	int ret_clk = 0;

	/* pr_info("%s +\n", __func__); */

	if (ref_count++ > 0)
		return 0;

	ret = regulator_enable(vvpu_reg_id);
	if (ret < 0)
		return ret;

	/* pr_info("%s SPM: Release IPU external buck iso\n", __func__); */
	writel(readl(APUSYS_BUCK_ISOLATION) & ~(0x00000021),
		APUSYS_BUCK_ISOLATION);

	/* pr_info("%s clk_enable (pre_clk)\n", __func__); */
	ENABLE_CLK(clk_top_ipu_if_sel);
	ENABLE_CLK(clk_top_dsp_sel);
	if (ret_clk < 0)
		return ret_clk;

	/* pr_info("%s SPM:  subsys power on (APU_CONN/APU_VCORE)\n", __func__); */
	writel(readl(APUSYS_SPM_CROSS_WAKE_M01_REQ) | APMCU_WAKEUP_APU,
		APUSYS_SPM_CROSS_WAKE_M01_REQ);

	/* pr_info("%s SPM: wait until PWR_ACK = 1\n", __func__); */
	ret = readl_poll_timeout(APUSYS_OTHER_PWR_STATUS, tmp,
		(tmp & (0x1UL << 5)) == (0x1UL << 5),
		MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0)
		return ret;

	/* pr_info("%s RPC: wait until PWR ON complete = 1\n", __func__); */
	ret = readl_poll_timeout(APUSYS_RPC_INTF_PWR_RDY, tmp,
		(tmp & (0x1UL << 0)) == (0x1UL << 0),
		MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0)
		return ret;

	/* pr_info("%s bus/sleep protect CG on\n", __func__); */
	writel(0xFFFFFFFF, APUSYS_VCORE_CG_CLR);
	writel(0xFFFFFFFF, APUSYS_CONN_CG_CLR);

	pr_info(
		"%s spm_wake_bit: 0x%x, rpcValue: 0x%x\n",
		__func__,
		readl(APUSYS_SPM_CROSS_WAKE_M01_REQ),
		readl(APUSYS_RPC_INTF_PWR_RDY));

	/* pr_info("%s -\n", __func__); */
	return 0;
}

static int apu_top_off(void)
{
	int ret, tmp;

	/* pr_info("%s +\n", __func__); */

	if (ref_count > 0)
		ref_count--;
	else
		return 0;

	/* pr_info("%s bus/sleep protect CG on\n", __func__); */
	writel(0xFFFFFFFF, APUSYS_VCORE_CG_CLR);
	writel(0xFFFFFFFF, APUSYS_CONN_CG_CLR);

	/* pr_info("%s SPM:  subsys power off (APU_CONN/APU_VCORE)\n", __func__); */
	writel(readl(APUSYS_SPM_CROSS_WAKE_M01_REQ) &
		~APMCU_WAKEUP_APU, APUSYS_SPM_CROSS_WAKE_M01_REQ);

	/* pr_info("%s RPC:  sleep request enable\n", __func__); */
	writel(readl(APUSYS_RPC_TOP_CON) | 0x1, APUSYS_RPC_TOP_CON);

	/* pr_info("%s RPC: wait until PWR down complete = 0\n", __func__); */
	ret = readl_poll_timeout(APUSYS_RPC_INTF_PWR_RDY, tmp,
		(tmp & (0x1UL << 0)) == 0x0,
		MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0)
		return ret;

	/* pr_info("%s SPM: wait until PWR_ACK = 0\n", __func__); */
	ret = readl_poll_timeout(APUSYS_OTHER_PWR_STATUS, tmp,
		(tmp & (0x1UL << 5)) == 0x0,
		MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0)
		return ret;

	pr_info(
		"%s spm_wake_bit: 0x%x, rpcValue: 0x%x!\n",
		__func__,
		readl(APUSYS_SPM_CROSS_WAKE_M01_REQ),
		readl(APUSYS_RPC_INTF_PWR_RDY));

	/* pr_info("%s scpsys_clk_disable (pre_clk)\n", __func__); */
	DISABLE_CLK(clk_top_ipu_if_sel);
	DISABLE_CLK(clk_top_dsp_sel);

	/* pr_info("%s SPM: Enable IPU external buck iso\n", __func__); */
	writel(readl(APUSYS_BUCK_ISOLATION) & 0x00000021,
		APUSYS_BUCK_ISOLATION);

	ret = regulator_disable(vvpu_reg_id);
	if (ret < 0)
		return ret;

	/* pr_info("%s -\n", __func__); */
	return 0;
}

struct apu_callbacks apu_handle = {
	.apu_power_on = apu_top_on,
	.apu_power_off = apu_top_off,
};

static const struct of_device_id of_match_apu_top[] = {
	{ .compatible = "mediatek,apu_top"},
	{ /* end of list */},
};

static int apu_top_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *apu_spm_node = NULL;
	struct device_node *apu_conn_node = NULL;
	struct device_node *apu_vcore_node = NULL;
	void __iomem *apu_rpc_base_addr;
	void __iomem *apu_spm_base_addr;
	void __iomem *apu_conn_base_addr;
	void __iomem *apu_vcore_base_addr;
	unsigned int regValue = 0;
	int ret = 0;
	int ret_clk = 0;

	/* pr_info("%s +\n", __func__); */

	if (!node) {
		pr_info("get apu_top device node err\n");
		return -ENODEV;
	}

	vvpu_reg_id = regulator_get(&pdev->dev, "vvpu");
	if (!vvpu_reg_id) {
		pr_info("regulator_get vvpu_reg_id failed\n");
		return -ENOENT;
	}

	PREPARE_CLK(clk_top_dsp_sel);
	PREPARE_CLK(clk_top_ipu_if_sel);
	if (ret_clk < 0)
		return ret_clk;

	/* pr_info("register_apu_callback start\n"); */
	register_apu_callback(&apu_handle);

	/* pr_info("%s pm_runtime_enable start\n", __func__); */
	pm_runtime_enable(&pdev->dev);
	/* pr_info("%s pm_runtime_get_sync start\n", __func__); */
	pm_runtime_get_sync(&pdev->dev);

	apu_rpc_base_addr = of_iomap(node, 0);
	if (IS_ERR((void *)apu_rpc_base_addr)) {
		pr_info("Unable to iomap apu_rpc_base_addr\n");
		goto err_exit;
	}
	g_APUSYS_RPCTOP_BASE = apu_rpc_base_addr;

	// spm
	apu_spm_node = of_find_compatible_node(NULL, NULL,
		"mediatek,mt6873-scpsys");
	if (apu_spm_node) {
		apu_spm_base_addr = of_iomap(apu_spm_node, 0);
		if (IS_ERR((void *)apu_spm_base_addr)) {
			pr_info("Unable to iomap apu_spm_base_addr\n");
			goto err_exit;
		}
		g_APUSYS_SPM_BASE = apu_spm_base_addr;
	}

	// apusys conn
	apu_conn_node = of_find_compatible_node(NULL, NULL,
							"mediatek,mt8192-apu_conn");
	if (apu_conn_node) {
		apu_conn_base_addr = of_iomap(apu_conn_node, 0);

		if (IS_ERR((void *)apu_conn_base_addr)) {
			pr_info("Unable to iomap apu_conn_base_addr\n");
			goto err_exit;
		}
		g_APUSYS_CONN_BASE = apu_conn_base_addr;
	}

	// apusys vcore
	apu_vcore_node = of_find_compatible_node(NULL, NULL,
							"mediatek,mt8192-apu_vcore");
	if (apu_vcore_node) {
		apu_vcore_base_addr = of_iomap(apu_vcore_node, 0);

		if (IS_ERR((void *)apu_vcore_base_addr)) {
			pr_info("Unable to iomap apu_vcore_base_addr\n");
			goto err_exit;
		}
		g_APUSYS_VCORE_BASE = apu_vcore_base_addr;
	}

	/*
	 * set memory type to PD or sleep group
	 * sw_type register for each memory group, set to PD mode default
	 */
	writel(0xFF, APUSYS_RPC_SW_TYPE0); // APUTOP
	writel(0x7, APUSYS_RPC_SW_TYPE2);	// VPU0
	writel(0x7, APUSYS_RPC_SW_TYPE3);	// VPU1
	writel(0x3, APUSYS_RPC_SW_TYPE6);	// MDLA0

	// mask RPC IRQ and bypass WFI
	regValue = readl(APUSYS_RPC_TOP_SEL);
	regValue |= 0x9E;
	regValue |= BIT(10);
	writel(regValue, APUSYS_RPC_TOP_SEL);
	/* pr_info("%s pm_runtime_put_sync start\n", __func__); */
	pm_runtime_put_sync(&pdev->dev);
	/* pr_info("%s pm_runtime_get_sync start\n", __func__); */
	pm_runtime_get_sync(&pdev->dev);
	/* pr_info("%s pm_runtime_put_sync start\n", __func__); */
	pm_runtime_put_sync(&pdev->dev);

	/* pr_info("%s -\n", __func__); */

	return 0;

err_exit:
	g_APUSYS_RPCTOP_BASE = NULL;
	g_APUSYS_SPM_BASE = NULL;
	g_APUSYS_CONN_BASE = NULL;
	g_APUSYS_VCORE_BASE = NULL;

	return 0;
}

static int apu_top_remove(struct platform_device *pdev)
{
	pr_info("%s +\n", __func__);

	regulator_put(vvpu_reg_id);
	vvpu_reg_id = NULL;

	pr_info("%s -\n", __func__);

	return 0;
}

static struct platform_driver apu_top_drv = {
	.probe = apu_top_probe,
	.remove = apu_top_remove,
	.driver = {
		.name = "apu_top",
		.of_match_table = of_match_apu_top,
	},
};

static int __init apu_top_init(void)
{
	return platform_driver_register(&apu_top_drv);
}

static void __exit apu_top_exit(void)
{
	platform_driver_unregister(&apu_top_drv);
}

module_init(apu_top_init);
module_exit(apu_top_exit);
MODULE_LICENSE("GPL");
