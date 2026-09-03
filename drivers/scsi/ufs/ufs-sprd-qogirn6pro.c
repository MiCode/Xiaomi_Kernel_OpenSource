// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Host Controller driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 Unisoc, Inc.
 *
 */

#include <linux/delay.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/nvmem-consumer.h>
#include <linux/reset.h>
#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
#include <linux/sprd_sip_svc.h>
#endif

#include "ufshcd.h"
#include "ufshcd-pltfrm.h"
#include "ufs-sprd.h"
#include "ufs-sprd-qogirn6pro.h"
#include "ufs-sprd-ioctl.h"
#include "ufs-sprd-rpmb.h"
#include "ufs-sprd-bootdevice.h"
#include "ufs-sprd-debug.h"
#include "ufs-sprd-pwr-debug.h"

enum SPRD_N6P_UFS_DBG_INDEX {
	N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_80804040,
	N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_40402020,
	N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_20201010,
	N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_08080808,
	N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_04040404,
	N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_02020202,
	N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_01010101,
	N6P_UFS_AON_DBG_MOD12_SIG_ARRAY1,
	N6P_UFS_AON_DBG_MOD12_SIG_ARRAY2,
	N6P_UFS_AON_DBG_MOD12_SIG_ARRAY3,
	N6P_UFS_AON_DBG_MOD12_SIG_ARRAY4,
	N6P_UFS_AON_DBG_MOD12_SIG_ARRAY5,
	N6P_UFS_AON_DBG_MOD12_SIG_ARRAY6,
	N6P_UFS_AON_DBG_MOD12_SIG_ARRAY7,

	N6P_UFS_AP_DBG_BUS_15,
	N6P_UFS_AP_DBG_BUS_16,
	N6P_UFS_AP_DBG_BUS_17,
	N6P_UFS_AP_DBG_BUS_21,
	N6P_UFS_AP_DBG_BUS_22,
	N6P_UFS_AP_DBG_BUS_23,

	N6P_UFS_UNIPRO_UIC_D08C,
	N6P_UFS_UNIPRO_UIC_D08D,
	N6P_UFS_UNIPRO_UIC_D08E,
	N6P_UFS_UNIPRO_UIC_D092,
	N6P_UFS_UNIPRO_UIC_D093,
	N6P_UFS_UNIPRO_UIC_D094,
	N6P_UFS_UNIPRO_UIC_D095,
	N6P_UFS_UNIPRO_UIC_D096,
	N6P_UFS_UNIPRO_UIC_D097,
	N6P_UFS_UNIPRO_UIC_D09D,

	N6P_UFS_VDDEMMCORE_VOLTAGE,
	N6P_UFS_VDDSRAM_VOLTAGE,
	N6P_UFS_VDDUFS1V2_VOLTAGE,

	N6P_UFS_DBG_REGS_MAX
};

static const char *n6p_dbg_regs_name[N6P_UFS_DBG_REGS_MAX] = {
	/* N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_80804040 */
	"[DBGBUS]AON_DBG_MOD12_SIG_ARRAY0_80804040",
	/* N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_40402020 */
	"[DBGBUS]AON_DBG_MOD12_SIG_ARRAY0_40402020",
	/* N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_20201010 */
	"[DBGBUS]AON_DBG_MOD12_SIG_ARRAY0_20201010",
	/* N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_08080808 */
	"[DBGBUS]AON_DBG_MOD12_SIG_ARRAY0_08080808",
	/* N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_04040404 */
	"[DBGBUS]AON_DBG_MOD12_SIG_ARRAY0_04040404",
	/* N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_02020202 */
	"[DBGBUS]AON_DBG_MOD12_SIG_ARRAY0_02020202",
	/* N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_01010101 */
	"[DBGBUS]AON_DBG_MOD12_SIG_ARRAY0_01010101",
	/* N6P_UFS_AON_DBG_MOD12_SIG_ARRAY1 */
	"[DBGBUS]AON_DBG_MOD12_SIG_ARRAY1",
	/* N6P_UFS_AON_DBG_MOD12_SIG_ARRAY2 */
	"[DBGBUS]AON_DBG_MOD12_SIG_ARRAY2",
	/* N6P_UFS_AON_DBG_MOD12_SIG_ARRAY3 */
	"[DBGBUS]AON_DBG_MOD12_SIG_ARRAY3",
	/* N6P_UFS_AON_DBG_MOD12_SIG_ARRAY4 */
	"[DBGBUS]AON_DBG_MOD12_SIG_ARRAY4",
	/* N6P_UFS_AON_DBG_MOD12_SIG_ARRAY5 */
	"[DBGBUS]AON_DBG_MOD12_SIG_ARRAY5",
	/* N6P_UFS_AON_DBG_MOD12_SIG_ARRAY6 */
	"[DBGBUS]AON_DBG_MOD12_SIG_ARRAY6",
	/* N6P_UFS_AON_DBG_MOD12_SIG_ARRAY7 */
	"[DBGBUS]AON_DBG_MOD12_SIG_ARRAY7",

	/* N6P_UFS_AP_DBG_BUS_15] */
	"[DBGBUS]MonitorUP[31:0]",
	/* N6P_UFS_AP_DBG_BUS_16] */
	"[DBGBUS]MonitorUP[63:32]",
	/* N6P_UFS_AP_DBG_BUS_17] */
	"[DBGBUS]MonitorUP[79:64]",
	/* N6P_UFS_AP_DBG_BUS_21] */
	"[DBGBUS]Monitor[31:0]",
	/* N6P_UFS_AP_DBG_BUS_22] */
	"[DBGBUS]Monitor[63:32]",
	/* N6P_UFS_AP_DBG_BUS_23] */
	"[DBGBUS]Monitor[79:64]",

	/* N6P_UFS_UNIPRO_UIC_D08C] */
	"[UIC][D08C]VS_DebugAODomain",
	/* N6P_UFS_UNIPRO_UIC_D08D] */
	"[UIC][D08D]VS_DebugAdapt",
	/* N6P_UFS_UNIPRO_UIC_D08E] */
	"[UIC][D08E]VS_DebugL15ci",
	/* N6P_UFS_UNIPRO_UIC_D092] */
	"[UIC][D092]VS_DebugTxByteCount",
	/* N6P_UFS_UNIPRO_UIC_D093] */
	"[UIC][D093]VS_DebugRxByteCount",
	/* N6P_UFS_UNIPRO_UIC_D094] */
	"[UIC][D094]VS_DebugInvalidByteEnable",
	/* N6P_UFS_UNIPRO_UIC_D095] */
	"[UIC][D095]VS_DebugLinkStartup",
	/* N6P_UFS_UNIPRO_UIC_D096] */
	"[UIC][D096]VS_DebugPwrChg",
	/* N6P_UFS_UNIPRO_UIC_D097] */
	"[UIC][D097]VS_DebugStates",
	/* N6P_UFS_UNIPRO_UIC_D09D] */
	"[UIC][D09D]VS_DebugCounterOverflow",

	/* N6P_UFS_VDDEMMCORE_VOLTAGE] */
	"[V]vddemmcore",
	/* N6P_UFS_VDDSRAM_VOLTAGE] */
	"[V]vddsram",
	/* N6P_UFS_VDDUFS1V2_VOLTAGE] */
	"[V]vddufs1v2",
};

static void ufs_sprd_get_debug_regs(struct ufs_hba *hba, enum ufs_event_type evt, void *data)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9620_data *priv =
		(struct ufs_sprd_ums9620_data *) host->ufs_priv_data;
	struct ufs_dbg_hist *d = ufs_sprd_get_dbg_hist();
	struct ufs_dbg_pkg *p = &d->pkg[d->pos];
	u32 *v = p->val_array;
	unsigned long flags;

	if (!p->val_array)
		return;

	if (!priv->syssel_reg && !priv->anlg_phy_g12) {
		dev_warn(hba->dev, "can't get ufs debug bus base.\n");
		return;
	}

	spin_lock_irqsave(&ufs_dbg_regs_lock, flags);
	d->pos = (d->pos + 1) % MAX_UFS_DBG_HIST;
	spin_unlock_irqrestore(&ufs_dbg_regs_lock, flags);

	p->id = evt;
	p->data = *(u32 *)data;
	p->time = local_clock();
	memset(v, 0, sizeof(u32) * N6P_UFS_DBG_REGS_MAX);

	/* read debugbus START */
	writel(0x6, priv->syssel_reg);
	writel(0xD, priv->syssel_reg + 0xc);

	writel(0x1, priv->syssel_reg + 0x10);
	writel(0xffffffff, priv->anlg_phy_g12 + 0x201c);
	writel(0x80804040, priv->anlg_phy_g12 + 0x101c);
	v[N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_80804040] = readl(priv->syssel_reg + 0x208);

	writel(0xffffffff, priv->anlg_phy_g12 + 0x201c);
	writel(0x40402020, priv->anlg_phy_g12 + 0x101c);
	v[N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_40402020] = readl(priv->syssel_reg + 0x208);

	writel(0xffffffff, priv->anlg_phy_g12 + 0x201c);
	writel(0x20201010, priv->anlg_phy_g12 + 0x101c);
	v[N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_20201010] = readl(priv->syssel_reg + 0x208);

	writel(0xffffffff, priv->anlg_phy_g12 + 0x201c);
	writel(0x08080808, priv->anlg_phy_g12 + 0x101c);
	v[N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_08080808] = readl(priv->syssel_reg + 0x208);

	writel(0xffffffff, priv->anlg_phy_g12 + 0x201c);
	writel(0x04040404, priv->anlg_phy_g12 + 0x101c);
	v[N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_04040404] = readl(priv->syssel_reg + 0x208);

	writel(0xffffffff, priv->anlg_phy_g12 + 0x201c);
	writel(0x02020202, priv->anlg_phy_g12 + 0x101c);
	v[N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_02020202] = readl(priv->syssel_reg + 0x208);

	writel(0xffffffff, priv->anlg_phy_g12 + 0x201c);
	writel(0x01010101, priv->anlg_phy_g12 + 0x101c);
	v[N6P_UFS_AON_DBG_MOD12_SIG_ARRAY0_01010101] = readl(priv->syssel_reg + 0x208);

	writel(0x2, priv->syssel_reg + 0x10);
	v[N6P_UFS_AON_DBG_MOD12_SIG_ARRAY1] = readl(priv->syssel_reg + 0x208);

	writel(0x3, priv->syssel_reg + 0x10);
	v[N6P_UFS_AON_DBG_MOD12_SIG_ARRAY2] = readl(priv->syssel_reg + 0x208);

	writel(0x4, priv->syssel_reg + 0x10);
	v[N6P_UFS_AON_DBG_MOD12_SIG_ARRAY3] = readl(priv->syssel_reg + 0x208);

	writel(0x5, priv->syssel_reg + 0x10);
	v[N6P_UFS_AON_DBG_MOD12_SIG_ARRAY4] = readl(priv->syssel_reg + 0x208);

	writel(0x6, priv->syssel_reg + 0x10);
	v[N6P_UFS_AON_DBG_MOD12_SIG_ARRAY5] = readl(priv->syssel_reg + 0x208);

	writel(0x7, priv->syssel_reg + 0x10);
	v[N6P_UFS_AON_DBG_MOD12_SIG_ARRAY6] = readl(priv->syssel_reg + 0x208);

	writel(0x8, priv->syssel_reg + 0x10);
	v[N6P_UFS_AON_DBG_MOD12_SIG_ARRAY7] = readl(priv->syssel_reg + 0x208);

	writel(0x0, priv->syssel_reg);
	writel(0x0, priv->syssel_reg + 0xc);

	writel(0x10, priv->syssel_reg + 0x10);
	v[N6P_UFS_AP_DBG_BUS_15] = readl(priv->syssel_reg + 0x208);
	writel(0x11, priv->syssel_reg + 0x10);
	v[N6P_UFS_AP_DBG_BUS_16] = readl(priv->syssel_reg + 0x208);
	writel(0x12, priv->syssel_reg + 0x10);
	v[N6P_UFS_AP_DBG_BUS_17] = readl(priv->syssel_reg + 0x208);

	writel(0x16, priv->syssel_reg + 0x10);
	v[N6P_UFS_AP_DBG_BUS_21] = readl(priv->syssel_reg + 0x208);
	writel(0x17, priv->syssel_reg + 0x10);
	v[N6P_UFS_AP_DBG_BUS_22] = readl(priv->syssel_reg + 0x208);
	writel(0x18, priv->syssel_reg + 0x10);
	v[N6P_UFS_AP_DBG_BUS_23] = readl(priv->syssel_reg + 0x208);
	/* read debugbus END */

	if (!preempt_count()) {
		p->preempt = false;

		if (!hba->active_uic_cmd) {
			p->active_uic_cmd = false;

			/* read unipro attr START */
			ufshcd_dme_get(hba, UIC_ARG_MIB(0xd08c), &v[N6P_UFS_UNIPRO_UIC_D08C]);
			ufshcd_dme_get(hba, UIC_ARG_MIB(0xd08d), &v[N6P_UFS_UNIPRO_UIC_D08D]);
			ufshcd_dme_get(hba, UIC_ARG_MIB(0xd08e), &v[N6P_UFS_UNIPRO_UIC_D08E]);
			ufshcd_dme_get(hba, UIC_ARG_MIB(0xd092), &v[N6P_UFS_UNIPRO_UIC_D092]);
			ufshcd_dme_get(hba, UIC_ARG_MIB(0xd093), &v[N6P_UFS_UNIPRO_UIC_D093]);
			ufshcd_dme_get(hba, UIC_ARG_MIB(0xd094), &v[N6P_UFS_UNIPRO_UIC_D094]);
			ufshcd_dme_get(hba, UIC_ARG_MIB(0xd095), &v[N6P_UFS_UNIPRO_UIC_D095]);
			ufshcd_dme_get(hba, UIC_ARG_MIB(0xd096), &v[N6P_UFS_UNIPRO_UIC_D096]);
			ufshcd_dme_get(hba, UIC_ARG_MIB(0xd097), &v[N6P_UFS_UNIPRO_UIC_D097]);
			ufshcd_dme_get(hba, UIC_ARG_MIB(0xd09d), &v[N6P_UFS_UNIPRO_UIC_D09D]);
			/* read unipro attr END */
		} else {
			p->active_uic_cmd = true;
		}

		/* read VOLTAGE START */
		v[N6P_UFS_VDDEMMCORE_VOLTAGE] = regulator_get_voltage(hba->vreg_info.vcc->reg);
		v[N6P_UFS_VDDSRAM_VOLTAGE] = regulator_get_voltage(priv->vddsram);
		v[N6P_UFS_VDDUFS1V2_VOLTAGE] = regulator_get_voltage(priv->vdd_mphy);
		/* read VOLTAGE END */
	} else {
		p->preempt = true;
	}
}

static int ufs_sprd_get_reg_from_dt(struct device *dev,
				  struct ufs_sprd_ums9620_data *priv)
{
	int ret = 0;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &priv->phy_sram_ext_ld_done,
				      "phy_sram_ext_ld_done");
	if (ret < 0)
		return ret;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &priv->phy_sram_bypass,
				      "phy_sram_bypass");
	if (ret < 0)
		return ret;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &priv->phy_sram_init_done,
				      "phy_sram_init_done");
	if (ret < 0)
		return ret;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &priv->aon_apb_ufs_clk_en,
				      "aon_apb_ufs_clk_en");
	if (ret < 0)
		return ret;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &priv->ufsdev_refclk_en,
				      "ufsdev_refclk_en");
	if (ret < 0)
		return ret;

	ret = ufs_sprd_get_syscon_reg(dev->of_node,
					&priv->usb31pllv_ref2mphy_en,
				      "usb31pllv_ref2mphy_en");
	return ret;
}

static int ufs_sprd_priv_parse_dt(struct device *dev,
				  struct ufs_hba *hba,
				  struct ufs_sprd_host *host)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ufs_sprd_ums9620_data *priv =
		(struct ufs_sprd_ums9620_data *) host->ufs_priv_data;
	int ret = 0;

	priv->ufs_lane_calib_data1 = ufs_efuse_calib_data(pdev,
							  "ufs_cali_lane1");
	if (priv->ufs_lane_calib_data1 == -EPROBE_DEFER) {
		dev_err(&pdev->dev,
			"%s:get ufs_lane_calib_data1 failed!\n", __func__);
		ret =  -EPROBE_DEFER;
		goto out_variant_clear;
	}

	dev_err(&pdev->dev, "%s: ufs_lane_calib_data1: %x\n",
		__func__, priv->ufs_lane_calib_data1);

	priv->ufs_lane_calib_data0 = ufs_efuse_calib_data(pdev,
							  "ufs_cali_lane0");
	if (priv->ufs_lane_calib_data0 == -EPROBE_DEFER) {
		dev_err(&pdev->dev,
			"%s:get ufs_lane_calib_data1 failed!\n", __func__);
		ret =  -EPROBE_DEFER;
		goto out_variant_clear;
	}

	dev_err(&pdev->dev, "%s: ufs_lane_calib_data0: %x\n",
		__func__, priv->ufs_lane_calib_data0);

	priv->vdd_mphy = devm_regulator_get(dev, "vdd-mphy");
	ret = regulator_enable(priv->vdd_mphy);
	if (ret)
		return -ENODEV;

	priv->vddsram = devm_regulator_get(dev, "vdd-hba");
	if (IS_ERR(priv->vddsram)) {
		dev_err(&pdev->dev, "get vdd-hba regulator failed\n");
		return -ENODEV;
	}

	ret = ufs_sprd_get_reg_from_dt(dev, priv);
	if (ret < 0)
		return -ENODEV;

	priv->hclk = devm_clk_get(&pdev->dev, "ufs_hclk");
	if (IS_ERR(priv->hclk)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: ufs_pclk\n");
			 priv->hclk = NULL;
	}

	priv->hclk_source = devm_clk_get(&pdev->dev, "ufs_hclk_source");
	if (IS_ERR(priv->hclk_source)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: ufs_hclk_source\n");
			 priv->hclk_source = NULL;
	}

	priv->rco_100M = devm_clk_get(&pdev->dev, "ufs_rco_100M");
	if (IS_ERR(priv->rco_100M)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: rco_100M\n");
			 priv->rco_100M = NULL;
	}

	priv->aon_apb_ufs_rst = devm_reset_control_get(dev, "ufsdev_soft_rst");
	if (IS_ERR(priv->aon_apb_ufs_rst)) {
		dev_err(dev, "%s get ufsdev_soft_rst failed, err%ld\n",
			__func__, PTR_ERR(priv->aon_apb_ufs_rst));
		priv->aon_apb_ufs_rst = NULL;
		return -ENODEV;
	}

	priv->ap_ahb_ufs_rst = devm_reset_control_get(dev, "ufs_soft_rst");
	if (IS_ERR(priv->ap_ahb_ufs_rst)) {
		dev_err(dev, "%s get ufs_soft_rst failed, err%ld\n",
			__func__, PTR_ERR(priv->ap_ahb_ufs_rst));
		priv->ap_ahb_ufs_rst = NULL;
		return -ENODEV;
	}

	priv->syssel_reg = devm_ioremap(dev, REG_DEBUG_BUS_SYSSEL, 0x210);
	if (IS_ERR(priv->syssel_reg)) {
		pr_err("error to ioremap ufs debug bus base.");
		priv->syssel_reg = NULL;
	}

	priv->anlg_phy_g12 = devm_ioremap(dev, REG_ANLG_PHY_G12, 0x3000);
	if (IS_ERR(priv->anlg_phy_g12)) {
		pr_err("error to ioremap ufs anlg_phy_g12.");
		priv->anlg_phy_g12 = NULL;
	}

	return 0;

out_variant_clear:
	return ret;
}

static int ufs_sprd_priv_pre_init(struct device *dev,
				  struct ufs_hba *hba,
				  struct ufs_sprd_host *host)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
	struct ufs_sprd_ums9620_data *priv =
		(struct ufs_sprd_ums9620_data *) host->ufs_priv_data;
	struct sprd_sip_svc_handle *svc_handle;

	ret = reset_control_assert(priv->ap_ahb_ufs_rst);
	if (ret) {
		dev_err(host->hba->dev, "%s assert ufs_soft_rst failed, ret = %d!\n",
				__func__, ret);
		return -ENODEV;
	}

	usleep_range(1000, 1100);

	ret = reset_control_deassert(priv->ap_ahb_ufs_rst);
	if (ret) {
		dev_err(host->hba->dev, "%s deassert ufs_soft_rst failed, ret = %d!\n",
				__func__, ret);
		return -ENODEV;
	}

	ufshcd_writel(hba, CONTROLLER_ENABLE, REG_CONTROLLER_ENABLE);
	if ((ufshcd_readl(hba, REG_UFS_CCAP) & (1 << 27)))
		ufshcd_writel(hba, (CRYPTO_GENERAL_ENABLE | CONTROLLER_ENABLE),
			      REG_CONTROLLER_ENABLE);
	svc_handle = sprd_sip_svc_get_handle();
	if (!svc_handle) {
		pr_err("%s: failed to get svc handle\n", __func__);
		return -ENODEV;
	}

	ret = svc_handle->storage_ops.ufs_crypto_enable();
	pr_err("smc: enable cfg, ret:0x%x", ret);
#endif

	return ret;
}

static int ufs_sprd_check_stat_after_suspend(struct ufs_hba *hba,
						enum ufs_notify_change_status status)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9620_data *priv =
		(struct ufs_sprd_ums9620_data *) host->ufs_priv_data;
	u32 ufs_pwr_gate = -1;
	u32 monitor = -1;

	if (!priv->syssel_reg) {
		dev_warn(hba->dev, "can't get ufs debug bus base.\n");
		return 0;
	}

	writel(0x6, priv->syssel_reg);
	writel(0x9, priv->syssel_reg + 0xc);
	writel(0xd1, priv->syssel_reg + 0x10);
	ufs_pwr_gate = readl(priv->syssel_reg + 0x208);
	if (unlikely((ufs_pwr_gate & 0x10000000) != 0x0))
		goto check_monitor;
	else
		goto out;


check_monitor:
	writel(0x0, priv->syssel_reg);
	writel(0x0, priv->syssel_reg + 0xc);
	writel(0x18, priv->syssel_reg + 0x10);
	monitor = readl(priv->syssel_reg + 0x208);
	if (monitor == 0) {
		dev_err(hba->dev, "ufs_pwr_gate:0x%x,monitor:0x%x\n", ufs_pwr_gate, monitor);
		return -EAGAIN;
	}

out:
	return 0;
}

static int ufs_sprd_init(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct ufs_sprd_host *host;
	int ret = 0;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	host->ufs_priv_data = devm_kzalloc(dev,
				 sizeof(struct ufs_sprd_ums9620_data),
				 GFP_KERNEL);
	if (!host->ufs_priv_data)
		return -ENOMEM;

	host->hba = hba;
	ufshcd_set_variant(hba, host);

	host->check_stat_after_suspend = ufs_sprd_check_stat_after_suspend;

	host->caps |= UFS_SPRD_CAP_ACC_FORBIDDEN_AFTER_H8_EE;

	hba->caps |= UFSHCD_CAP_CLK_GATING |
		UFSHCD_CAP_CRYPTO |
		UFSHCD_CAP_HIBERN8_WITH_CLK_GATING |
		UFSHCD_CAP_WB_EN |
		UFSHCD_CAP_H8_ULP;
	hba->quirks |= UFSHCD_QUIRK_BROKEN_UFS_HCI_VERSION |
		UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS;

	ret = ufs_sprd_priv_parse_dt(dev, hba, host);
	if (ret < 0)
		return ret;

	ret = ufs_sprd_priv_pre_init(dev, hba, host);
	if (ret < 0)
		return ret;

	hba->host->hostt->ioctl = ufshcd_sprd_ioctl;
#ifdef CONFIG_COMPAT
	hba->host->hostt->compat_ioctl = ufshcd_sprd_ioctl;
#endif

	ufs_sprd_get_gic_reg(hba);
	ufs_sprd_dbg_regs_hist_register(hba, N6P_UFS_DBG_REGS_MAX, n6p_dbg_regs_name);
	ufs_sprd_debug_init(hba);

	return 0;
}

static void ufs_sprd_exit(struct ufs_hba *hba)
{
	int err = 0;
	struct device *dev = hba->dev;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9620_data *priv =
		(struct ufs_sprd_ums9620_data *) host->ufs_priv_data;

	regmap_update_bits(priv->aon_apb_ufs_clk_en.regmap,
			   priv->aon_apb_ufs_clk_en.reg,
			   priv->aon_apb_ufs_clk_en.mask,
			   0);

	err = regulator_disable(priv->vdd_mphy);
	if (err)
		pr_err("disable vdd_mphy failed ret =0x%x!\n", err);

	devm_kfree(dev, host->ufs_priv_data);
	devm_kfree(dev, host);
	hba->priv = NULL;
}

static u32 ufs_sprd_get_ufs_hci_version(struct ufs_hba *hba)
{
	return UFSHCI_VERSION_30;
}

static int ufs_sprd_hw_init(struct ufs_hba *hba)
{
	int ret;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9620_data *priv =
		(struct ufs_sprd_ums9620_data *) host->ufs_priv_data;

	dev_info(host->hba->dev, "ufs hardware reset!\n");

	clk_set_parent(priv->hclk, priv->hclk_source);
	ufshcd_writel(hba, 0x100, REG_HCLKDIV);

	regmap_update_bits(priv->phy_sram_ext_ld_done.regmap,
			   priv->phy_sram_ext_ld_done.reg,
			   priv->phy_sram_ext_ld_done.mask,
			   priv->phy_sram_ext_ld_done.mask);

	regmap_update_bits(priv->phy_sram_bypass.regmap,
			   priv->phy_sram_bypass.reg,
			   priv->phy_sram_bypass.mask,
			   priv->phy_sram_bypass.mask);

	ret = reset_control_assert(priv->aon_apb_ufs_rst);
	if (ret) {
		dev_err(host->hba->dev, "%s assert ufsdev_soft_rst failed, ret = %d!\n",
				__func__, ret);
		goto out;
	}

	ret = reset_control_assert(priv->ap_ahb_ufs_rst);
	if (ret) {
		dev_err(host->hba->dev, "%s assert ufs_soft_rst failed, ret = %d!\n",
				__func__, ret);
		goto out;
	}

	usleep_range(1000, 1100);

	ret = reset_control_deassert(priv->aon_apb_ufs_rst);
	if (ret) {
		dev_err(host->hba->dev, "%s deassert ufsdev_soft_rst failed, ret = %d!\n",
				__func__, ret);
		goto out;
	}

	ret = reset_control_deassert(priv->ap_ahb_ufs_rst);
	if (ret) {
		dev_err(host->hba->dev, "%s deassert ufs_soft_rst failed, ret = %d!\n",
				__func__, ret);
		goto out;
	}

	ufs_sprd_update_err_cnt(host->hba, 0, UFS_SPRD_RESET);

out:
	return ret;
}

static int ufs_sprd_phy_sram_init_done(struct ufs_hba *hba)
{
	int ret = 0;
	uint32_t val = 0;
	uint32_t retry = 10;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9620_data *priv =
		(struct ufs_sprd_ums9620_data *) host->ufs_priv_data;

	do {
		ret = regmap_read(priv->phy_sram_init_done.regmap,
				  priv->phy_sram_init_done.reg, &val);
		if (ret < 0)
			return ret;

		if ((val&0x1) == 0x1) {
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGADDRLSB), 0x1c);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGADDRMSB), 0x40);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGWRLSB), 0x04);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGWRMSB), 0x00);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGRDWRSEL), 0x01);
			ufshcd_dme_set(hba, UIC_ARG_MIB(VS_MPHYCFGUPDT), 0x01);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGADDRLSB), 0x1c);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGADDRMSB), 0x41);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGWRLSB), 0x04);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGWRMSB), 0x00);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGRDWRSEL), 0x01);
			ufshcd_dme_set(hba, UIC_ARG_MIB(VS_MPHYCFGUPDT), 0x01);

			return 0;
		} else {
			udelay(1000);
			retry--;
		}
	} while (retry > 0);
		return -1;
}

static int ufs_sprd_phy_init(struct ufs_hba *hba)
{
	int ret = 0;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9620_data *priv =
		(struct ufs_sprd_ums9620_data *) host->ufs_priv_data;

	ufshcd_dme_set(hba, UIC_ARG_MIB(CBREFCLKCTRL2), 0x90);
	ufshcd_dme_set(hba, UIC_ARG_MIB(CBCRCTRL), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RXSQCONTROL,
		       UIC_ARG_MPHY_RX_GEN_SEL_INDEX(0)), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RXSQCONTROL,
		       UIC_ARG_MPHY_RX_GEN_SEL_INDEX(1)), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(VS_MPHYCFGUPDT), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(CBRATESEL), 0x01);

	ret = ufs_sprd_phy_sram_init_done(hba);
	if (ret)
		return ret;

	ufshcd_dme_set(hba, UIC_ARG_MIB(VS_MPHYCFGUPDT), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xaf);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118),
		       (priv->ufs_lane_calib_data0 >> 24) & 0xff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb1);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb8);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x40);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118),
		       (priv->ufs_lane_calib_data0 >> 24) & 0xff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xaf);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118),
		       (priv->ufs_lane_calib_data1 >> 24) & 0xff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb1);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb8);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x41);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118),
		       (priv->ufs_lane_calib_data1 >> 24) & 0xff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xaf);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118),
		       (priv->ufs_lane_calib_data0 >> 16) & 0xff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb1);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x02);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb8);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x10);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x40);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118),
		       (priv->ufs_lane_calib_data0 >> 16) & 0xff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xaf);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118),
		       (priv->ufs_lane_calib_data1 >> 16) & 0xff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb1);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x02);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb8);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0xb0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x11);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8116), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8117), 0x41);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8118),
		       (priv->ufs_lane_calib_data1 >> 16) & 0xff);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x8119), 0x00);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0x811c), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(0xd085), 0x01);

	regmap_update_bits(priv->phy_sram_ext_ld_done.regmap,
			   priv->phy_sram_ext_ld_done.reg,
			   priv->phy_sram_ext_ld_done.mask,
			   0);

	/* add ultra low power H8 function */
	if (hba->caps & UFSHCD_CAP_H8_ULP)
		ufshcd_dme_set(hba, UIC_ARG_MIB(CBUPLH8), ULP_H8_EN);

	ufshcd_dme_set(hba, UIC_ARG_MIB(VS_MPHYDISABLE), 0x0);

	return ret;
}

static int ufs_sprd_hce_enable_notify(struct ufs_hba *hba,
				      enum ufs_notify_change_status status)
{
	int err = 0;
	int ret = 0;
	struct sprd_sip_svc_handle *svc_handle;

	switch (status) {
	case PRE_CHANGE:
		/* Do hardware reset before host controller enable. */
		err = ufs_sprd_hw_init(hba);
		if (err) {
			dev_err(hba->dev, "%s: ufs hardware init failed!\n", __func__);
			return err;
		}
		hba->capabilities &= ~MASK_AUTO_HIBERN8_SUPPORT;
		hba->ahit = 0;
		hba->clk_gating.delay_ms = 10;

#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
		ufshcd_writel(hba, CONTROLLER_ENABLE, REG_CONTROLLER_ENABLE);
		svc_handle = sprd_sip_svc_get_handle();
		if (!svc_handle) {
			pr_err("%s: failed to get svc handle\n", __func__);
			return -ENODEV;
		}

		ret = svc_handle->storage_ops.ufs_crypto_enable();
		pr_err("smc: enable cfg, ret:0x%x", ret);
#endif
		break;
	case POST_CHANGE:
		err = ufs_sprd_phy_init(hba);
		if (err)
			dev_err(hba->dev, "Phy setup failed (%d)\n", err);
		break;
	default:
		dev_err(hba->dev, "%s: invalid status %d\n", __func__, status);
		err = -EINVAL;
		break;
	}

	return err;
}

static int ufs_sprd_pwr_change_notify(struct ufs_hba *hba,
		enum ufs_notify_change_status status,
		struct ufs_pa_layer_attr *desired_pwr_mode,
		struct ufs_pa_layer_attr *final_params)
{
	int err = 0;

	if (!final_params) {
		pr_err("%s: incoming final_params is NULL\n", __func__);
		err = -EINVAL;
		goto out;
	}

	switch (status) {
	case PRE_CHANGE:
		memcpy(final_params, desired_pwr_mode,
			sizeof(struct ufs_pa_layer_attr));
		if (final_params->gear_rx == UFS_HS_G4) {
			ufshcd_dme_peer_set(hba, UIC_ARG_MIB(PA_PeerRxHsAdaptInitial), 0x10);
			ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXHSADAPTTYPE), PA_INITIAL_ADAPT);
		} else
			ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXHSADAPTTYPE), PA_NO_ADAPT);
		/* err==0 using dev_req_params,err!=0 using dev_max_params */
		err = -EPERM;
		break;
	case POST_CHANGE:
		if (ufshcd_is_auto_hibern8_supported(hba))
			hba->ahit = AUTO_H8_IDLE_TIME_10MS;
		break;
	default:
		err = -EINVAL;
		break;
	}
	ufs_sprd_pwr_change_compare(hba, status, final_params, &err);

out:
	return err;
}

static void ufs_sprd_hibern8_notify(struct ufs_hba *hba,
				    enum uic_cmd_dme cmd,
				    enum ufs_notify_change_status status)
{
	u32 set;
	unsigned long flags;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9620_data *priv =
		(struct ufs_sprd_ums9620_data *) host->ufs_priv_data;

	switch (status) {
	case PRE_CHANGE:
		if (cmd == UIC_CMD_DME_HIBER_ENTER) {
			spin_lock_irqsave(hba->host->host_lock, flags);
			set = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);
			set &= ~UIC_COMMAND_COMPL;
			ufshcd_writel(hba, set, REG_INTERRUPT_ENABLE);
			spin_unlock_irqrestore(hba->host->host_lock, flags);

			clk_set_parent(priv->hclk, priv->rco_100M);
			ufshcd_writel(hba, 0x64, REG_HCLKDIV);
		}

		if (cmd == UIC_CMD_DME_HIBER_EXIT) {
			clk_set_parent(priv->hclk, priv->hclk_source);
			ufshcd_writel(hba, 0x100, REG_HCLKDIV);
		}
		break;
	case POST_CHANGE:
		if (cmd == UIC_CMD_DME_HIBER_EXIT) {
			spin_lock_irqsave(hba->host->host_lock, flags);
			set = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);
			set |= UIC_COMMAND_COMPL;
			ufshcd_writel(hba, set, REG_INTERRUPT_ENABLE);
			spin_unlock_irqrestore(hba->host->host_lock, flags);
		}
		break;
	default:
		break;
	}
}

static int ufs_sprd_device_reset(struct ufs_hba *hba)
{
	if (sprd_ufs_debug_is_supported(hba) == TRUE)
		ufshcd_common_trace(hba, UFS_TRACE_RESET_AND_RESTORE, NULL);

	return 0;
}

static void ufs_sprd_fixup_dev_quirks(struct ufs_hba *hba)
{
	/* vendor UFS UID info decode. */
	ufshcd_decode_ufs_uid(hba);
}

static int ufs_sprd_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op,
					enum ufs_notify_change_status status)
{
	unsigned long flags;

	/* disable auto h8 before ssu */
	if (status == PRE_CHANGE) {
		if (ufshcd_is_auto_hibern8_supported(hba)) {
			spin_lock_irqsave(hba->host->host_lock, flags);
			ufshcd_writel(hba, 0, REG_AUTO_HIBERNATE_IDLE_TIMER);
			spin_unlock_irqrestore(hba->host->host_lock, flags);
		}
	}

	return 0;
}

static void ufs_sprd_dbg_register_dump(struct ufs_hba *hba)
{
	u32 data = 0;

	sprd_ufs_print_err_cnt(hba);
	ufs_sprd_get_debug_regs(hba, UFS_EVT_CNT, &data);
	sprd_ufs_debug_err_dump(hba);
}

static int ufs_sprd_setup_clocks(struct ufs_hba *hba, bool on,
				 enum ufs_notify_change_status status)
{
	int err = 0;
	struct ufs_clk_dbg clk_tmp = {};
	struct ufs_sprd_ums9620_data *priv = NULL;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	if (host != NULL)
		priv = (struct ufs_sprd_ums9620_data *) host->ufs_priv_data;

	if (sprd_ufs_debug_is_supported(hba) == TRUE) {
		clk_tmp.status = status;
		clk_tmp.on = on;
		ufshcd_common_trace(hba, UFS_TRACE_CLK_GATE, &clk_tmp);
	}

	switch (status) {
	case PRE_CHANGE:
		/* synopsys spec requires that refclk must be opened before cfg_eb */
		if ((priv != NULL) && ufshcd_is_link_hibern8(hba) && (on == true)) {
			regmap_update_bits(priv->ufsdev_refclk_en.regmap,
				priv->ufsdev_refclk_en.reg,
				priv->ufsdev_refclk_en.mask,
				priv->ufsdev_refclk_en.mask);

			regmap_update_bits(priv->usb31pllv_ref2mphy_en.regmap,
				priv->usb31pllv_ref2mphy_en.reg,
				priv->usb31pllv_ref2mphy_en.mask,
				priv->usb31pllv_ref2mphy_en.mask);
		}

		if ((priv != NULL) && ufshcd_is_link_hibern8(hba) && (on == false)) {
			usleep_range(1000, 1100);
			regmap_update_bits(priv->ufsdev_refclk_en.regmap,
				priv->ufsdev_refclk_en.reg,
				priv->ufsdev_refclk_en.mask,
				0);

			regmap_update_bits(priv->usb31pllv_ref2mphy_en.regmap,
				priv->usb31pllv_ref2mphy_en.reg,
				priv->usb31pllv_ref2mphy_en.mask,
				0);
		}
		break;
	case POST_CHANGE:
		break;
	default:
		break;
	}

	return err;
}

static void ufs_sprd_update_evt_hist(struct ufs_hba *hba,
		enum ufs_event_type evt, void *data)
{
	struct ufs_evt_dbg evt_tmp = {};

	ufs_sprd_update_uic_err_cnt(hba, *(u32 *)data, evt);

	switch (evt) {
	case UFS_EVT_PA_ERR:
		ufs_sprd_update_err_cnt(hba, *(u32 *)data, UFS_LINE_RESET);
		break;
	default:
		break;
	}

	if (sprd_ufs_debug_is_supported(hba) == TRUE) {
		evt_tmp.id = evt;
		evt_tmp.val = *(u32 *)data;
		ufshcd_common_trace(hba, UFS_TRACE_EVT, &evt_tmp);
	}

	if (evt != UFS_EVT_DEV_RESET && evt != UFS_EVT_HOST_RESET)
		ufs_sprd_get_debug_regs(hba, evt, data);
}

static int ufs_sprd_program_key(struct ufs_hba *hba,
			      const union ufs_crypto_cfg_entry *cfg, int slot)
{
	int i;
	u32 slot_offset = hba->crypto_cfg_register + slot * sizeof(*cfg);
	int err = 0;

	if (hba->curr_dev_pwr_mode == UFS_POWERDOWN_PWR_MODE)
		goto out;

	/* Ensure that CFGE is cleared before programming the key */
	ufshcd_writel(hba, 0, slot_offset + 16 * sizeof(cfg->reg_val[0]));
	for (i = 0; i < 16; i++) {
		ufshcd_writel(hba, le32_to_cpu(cfg->reg_val[i]),
			      slot_offset + i * sizeof(cfg->reg_val[0]));
	}
	/* Write dword 17 */
	ufshcd_writel(hba, le32_to_cpu(cfg->reg_val[17]),
		      slot_offset + 17 * sizeof(cfg->reg_val[0]));
	/* Dword 16 must be written last */
	ufshcd_writel(hba, le32_to_cpu(cfg->reg_val[16]),
		      slot_offset + 16 * sizeof(cfg->reg_val[0]));
out:
	return err;
}

static int ufs_sprd_apply_dev_quirks(struct ufs_hba *hba)
{
	int ret = 0;
	u32 ulp_value;
	u32 granularity, peer_granularity;
	u32 pa_tactivate, peer_pa_tactivate;
	u32 pa_tactivate_us, peer_pa_tactivate_us, max_pa_tactivate_us;
	u8 gran_to_us_table[] = {1, 4, 8, 16, 32, 100};
	u32 new_pa_tactivate, new_peer_pa_tactivate;

	if (!(hba->caps & UFSHCD_CAP_H8_ULP))
		return 0;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(CBUPLH8), &ulp_value);
	if (ret)
		goto out;

	if (!(ulp_value & ULP_H8_EN)) {
		dev_err(hba->dev, "%s: ulp value : %x\n", __func__, ulp_value);
		return 0;
	}

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_GRANULARITY),
				  &granularity);
	if (ret)
		goto out;

	ret = ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_GRANULARITY),
				  &peer_granularity);
	if (ret)
		goto out;

	if ((granularity < PA_GRANULARITY_MIN_VAL) ||
	    (granularity > PA_GRANULARITY_MAX_VAL)) {
		dev_err(hba->dev, "%s: invalid host PA_GRANULARITY %d",
			__func__, granularity);
		return -EINVAL;
	}

	if ((peer_granularity < PA_GRANULARITY_MIN_VAL) ||
	    (peer_granularity > PA_GRANULARITY_MAX_VAL)) {
		dev_err(hba->dev, "%s: invalid device PA_GRANULARITY %d",
			__func__, peer_granularity);
		return -EINVAL;
	}

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_TACTIVATE), &pa_tactivate);
	if (ret)
		goto out;

	ret = ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_TACTIVATE),
				  &peer_pa_tactivate);
	if (ret)
		goto out;

	dev_info(hba->dev, "%s pre: %d,%d,%d,%d",
		 __func__, peer_pa_tactivate,
		 peer_granularity, pa_tactivate, granularity);

	pa_tactivate_us = pa_tactivate * gran_to_us_table[granularity - 1];
	peer_pa_tactivate_us = peer_pa_tactivate *
			gran_to_us_table[peer_granularity - 1];
	max_pa_tactivate_us = (pa_tactivate_us > peer_pa_tactivate_us) ?
			pa_tactivate_us : peer_pa_tactivate_us;

	new_peer_pa_tactivate = (max_pa_tactivate_us + ULP_TACTIVATE_COMP_TIME) /
			gran_to_us_table[peer_granularity - 1];

	ret = ufshcd_dme_peer_set(hba, UIC_ARG_MIB(PA_TACTIVATE),
				  new_peer_pa_tactivate);
	if (ret) {
		dev_err(hba->dev, "%s: peer_pa_tactivate set err ", __func__);
		goto out;
	}

	new_pa_tactivate = (max_pa_tactivate_us + ULP_TACTIVATE_COMP_TIME) /
			gran_to_us_table[granularity - 1];
	ret = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TACTIVATE),
			     new_pa_tactivate);
	if (ret) {
		dev_err(hba->dev, "%s: pa_tactivate set err ", __func__);
		goto out;
	}

	dev_info(hba->dev, "%s post: %d,%d,%d,%d",
		 __func__, new_peer_pa_tactivate,
		 peer_granularity, new_pa_tactivate, granularity);

out:
	return ret;
}

const struct ufs_hba_variant_ops ufs_hba_sprd_ums9620_vops = {
	.name = "sprd,ufshc-ums9620",
	.init = ufs_sprd_init,
	.exit = ufs_sprd_exit,
	.get_ufs_hci_version = ufs_sprd_get_ufs_hci_version,
	.setup_clocks = ufs_sprd_setup_clocks,
	.hce_enable_notify = ufs_sprd_hce_enable_notify,
	.pwr_change_notify = ufs_sprd_pwr_change_notify,
	.hibern8_notify = ufs_sprd_hibern8_notify,
	.fixup_dev_quirks = ufs_sprd_fixup_dev_quirks,
	.dbg_register_dump = ufs_sprd_dbg_register_dump,
	.device_reset = ufs_sprd_device_reset,
	.suspend = ufs_sprd_suspend,
	.event_notify = ufs_sprd_update_evt_hist,
	.program_key = ufs_sprd_program_key,
	.apply_dev_quirks = ufs_sprd_apply_dev_quirks,
};
EXPORT_SYMBOL(ufs_hba_sprd_ums9620_vops);
