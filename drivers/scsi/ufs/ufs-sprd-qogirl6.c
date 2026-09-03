// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Host Controller driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 Unisoc, Inc.
 *
 */

#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/time.h>
#include <linux/sprd_soc_id.h>
#include <dt-bindings/soc/sprd,qogirl6-regs.h>
#include <linux/rpmb.h>
#include <linux/reset.h>
#include <linux/nvmem-consumer.h>

#include "ufshcd.h"
#include "ufshcd-pltfrm.h"
#include "ufs-sprd.h"
#include "ufs-sprd-qogirl6.h"
#include "ufs-sprd-ioctl.h"
#include "ufs-sprd-bootdevice.h"
#include "ufs-sprd-debug.h"
#include "ufs-sprd-pwr-debug.h"

enum SPRD_L6_UFS_DBG_INDEX {
	L6_UFS_AP_DBG_BUS_0,
	L6_UFS_AP_DBG_BUS_21,
	L6_UFS_AP_DBG_BUS_22,
	L6_UFS_AP_DBG_BUS_28,
	L6_UFS_AP_DBG_BUS_29,
	L6_UFS_AP_DBG_BUS_30,
	L6_UFS_AP_DBG_BUS_31,
	L6_UFS_AP_DBG_BUS_32,
	L6_UFS_AP_DCO_CAL_RESULT,
	L6_UFS_AP_DCO_CNT_RESULT,

	/* CBLane: Ls Mode */
	L6_UFS_AP_CBLANE_LS_MODE,

	/* TX Lane0: Ls Sleep */
	L6_UFS_AP_TX_LANE0_LS_SLEEP_1,
	L6_UFS_AP_TX_LANE0_LS_SLEEP_2,
	L6_UFS_AP_TX_LANE0_LS_SLEEP_3,

	/* RX Lane0: LS sleep */
	L6_UFS_AP_RX_LANE0_LS_SLEEP_1,
	L6_UFS_AP_RX_LANE0_LS_SLEEP_2,
	L6_UFS_AP_RX_LANE0_LS_SLEEP_3,

	/* TX Lane1: Hibern8 */
	L6_UFS_AP_TX_LANE1_LS_SLEEP_1,
	L6_UFS_AP_TX_LANE1_LS_SLEEP_2,
	L6_UFS_AP_TX_LANE1_LS_SLEEP_3,

	/* RX Lane1: Hibern8 */
	L6_UFS_AP_RX_LANE1_LS_SLEEP_1,
	L6_UFS_AP_RX_LANE1_LS_SLEEP_2,
	L6_UFS_AP_RX_LANE1_LS_SLEEP_3,

	L6_UFS_UNIPRO_UIC_D094,
	L6_UFS_UNIPRO_UIC_D095,
	L6_UFS_UNIPRO_UIC_D097,

	/* Regulator */
	L6_UFS_VDD1V85_VOLTAGE,
	L6_UFS_AVDD1V2_UFS_VOLTAGE,
	L6_UFS_AVDD1V8_UFS_VOLTAGE,
	L6_UFS_VDDEMMCCORE_VOLTAGE,
	L6_UFS_VDDCORE_UFS_VOLTAGE,
	L6_UFS_VDDMODEM_UFS_VOLTAGE,

	L6_UFS_DBG_REGS_MAX
};

static const char *l6_dbg_regs_name[L6_UFS_DBG_REGS_MAX] = {
	/* L6_UFS_AP_DBG_BUS_0 */
	"[DBGBUS]AP_DBG_BUS_0",
	/* L6_UFS_AP_DBG_BUS_21 */
	"[DBGBUS]Monitor[31:0]",
	/* L6_UFS_AP_DBG_BUS_22 */
	"[DBGBUS]Monitor[63:32]",
	/* L6_UFS_AP_DBG_BUS_28 */
	"[DBGBUS]Monitor[73:64]",
	/* L6_UFS_AP_DBG_BUS_29 */
	"[DBGBUS]MonitorUP[31:0]",
	/* L6_UFS_AP_DBG_BUS_30 */
	"[DBGBUS]MonitorUP[63:32]",
	/* L6_UFS_AP_DBG_BUS_31 */
	"[DBGBUS]MonitorUP[79:64]",
	/* L6_UFS_AP_DBG_BUS_32 */
	"[DBGBUS]AP_DBG_BUS_32",
	/* L6_UFS_AP_DCO_CAL_RESULT */
	"[DCO]AP_DCO_CAL_RESULT",
	/* L6_UFS_AP_DCO_CNT_RESULT */
	"[DCO]AP_DCO_CNT_RESULT",

	/* CBLane: Ls Mode */
	"[DBGPHY]L6_UFS_AP_CBLANE_LS_MODE",

	/* TX Lane0: Ls Sleep */
	"[DBGPHY]L6_UFS_AP_TX_LANE0_LS_SLEEP_1",
	"[DBGPHY]L6_UFS_AP_TX_LANE0_LS_SLEEP_2",
	"[DBGPHY]L6_UFS_AP_TX_LANE0_LS_SLEEP_3",

	/* RX Lane0: LS sleep */
	"[DBGPHY]L6_UFS_AP_RX_LANE0_LS_SLEEP_1",
	"[DBGPHY]L6_UFS_AP_RX_LANE0_LS_SLEEP_2",
	"[DBGPHY]L6_UFS_AP_RX_LANE0_LS_SLEEP_3",

	/* TX Lane1: Hibern8 */
	"[DBGPHY]L6_UFS_AP_TX_LANE1_LS_SLEEP_1",
	"[DBGPHY]L6_UFS_AP_TX_LANE1_LS_SLEEP_2",
	"[DBGPHY]L6_UFS_AP_TX_LANE1_LS_SLEEP_3",

	/* RX Lane1: Hibern8 */
	"[DBGPHY]L6_UFS_AP_RX_LANE1_LS_SLEEP_1",
	"[DBGPHY]L6_UFS_AP_RX_LANE1_LS_SLEEP_2",
	"[DBGPHY]L6_UFS_AP_RX_LANE1_LS_SLEEP_3",

	/* L6_UFS_UNIPRO_UIC_D094] */
	"[UIC][D094]VS_DebugInvalidByteEnable",
	/* L6_UFS_UNIPRO_UIC_D095] */
	"[UIC][D095]VS_DebugLinkStartup",
	/* L6_UFS_UNIPRO_UIC_D097] */
	"[UIC][D097]VS_DebugStates",

	/* Regulator */
	/* vddgen0 VDD1V85 */
	"[V]L6_UFS_VDD1V85_VOLTAGE",
	/* avdd12  AVDD1V2_UFS */
	"[V]L6_UFS_AVDD1V2_UFS_VOLTAGE",
	/* avdd18  AVDD1V8_UFS */
	"[V]L6_UFS_AVDD1V8_UFS_VOLTAGE",
	/* vddemmccore  VDDEMMCCORE */
	"[V]L6_UFS_VDDEMMCCORE_VOLTAGE",
	/* vddcore  VDDCORE */
	"[V]L6_UFS_VDDCORE_UFS_VOLTAGE",
	/* vddmodem  VDDMODEM */
	"[V]L6_UFS_VDDMODEM_UFS_VOLTAGE"
};

/*
 * ufs_sprd_rmwl - read modify write into a register
 * @base - base address
 * @mask - mask to apply on read value
 * @val - actual value to write
 * @reg - register address
 */
static inline void ufs_sprd_rmwl(void __iomem *base, u32 mask, u32 val, u32 reg)
{
	u32 tmp;

	tmp = readl((base) + (reg));
	tmp &= ~mask;
	tmp |= (val & mask);
	writel(tmp, (base) + (reg));
}

static void ufs_sprd_get_debug_regs(struct ufs_hba *hba, enum ufs_event_type evt, void *data)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;
	struct ufs_dbg_hist *d = ufs_sprd_get_dbg_hist();
	struct ufs_dbg_pkg *p = &d->pkg[d->pos];
	u32 *v = p->val_array;
	unsigned long flags;

	if (!p->val_array)
		return;

	if (!priv->dbg_apb_reg) {
		dev_warn(hba->dev, "can't get ufs debug bus base.\n");
		return;
	}

	spin_lock_irqsave(&ufs_dbg_regs_lock, flags);
	d->pos = (d->pos + 1) % MAX_UFS_DBG_HIST;
	spin_unlock_irqrestore(&ufs_dbg_regs_lock, flags);

	p->id = evt;
	p->data = *(u32 *)data;
	p->time = local_clock();
	memset(v, 0, sizeof(u32) * L6_UFS_DBG_REGS_MAX);

	writel(0x0, priv->dbg_apb_reg + 0x18);
	writel(0x1 << 8, priv->dbg_apb_reg + 0x1c);
	v[L6_UFS_AP_DBG_BUS_0] = readl(priv->dbg_apb_reg + 0x50);

	writel(0x16 << 8, priv->dbg_apb_reg + 0x1c);
	v[L6_UFS_AP_DBG_BUS_21] = readl(priv->dbg_apb_reg + 0x50);

	writel(0x17 << 8, priv->dbg_apb_reg + 0x1c);
	v[L6_UFS_AP_DBG_BUS_22] = readl(priv->dbg_apb_reg + 0x50);

	writel(0x1D << 8, priv->dbg_apb_reg + 0x1c);
	v[L6_UFS_AP_DBG_BUS_28] = readl(priv->dbg_apb_reg + 0x50);

	writel(0x1E << 8, priv->dbg_apb_reg + 0x1c);
	v[L6_UFS_AP_DBG_BUS_29] = readl(priv->dbg_apb_reg + 0x50);

	writel(0x1F << 8, priv->dbg_apb_reg + 0x1c);
	v[L6_UFS_AP_DBG_BUS_30] = readl(priv->dbg_apb_reg + 0x50);

	writel(0x20 << 8, priv->dbg_apb_reg + 0x1c);
	v[L6_UFS_AP_DBG_BUS_31] = readl(priv->dbg_apb_reg + 0x50);

	writel(0x21 << 8, priv->dbg_apb_reg + 0x1c);
	v[L6_UFS_AP_DBG_BUS_32] = readl(priv->dbg_apb_reg + 0x50);

	v[L6_UFS_AP_DCO_CAL_RESULT] = readl((priv->ufs_analog_reg) + MPHY_DIG_CFG62_LANE0) >> 24;
	v[L6_UFS_AP_DCO_CNT_RESULT] =
			(readl((priv->ufs_analog_reg) + MPHY_DIG_CFG66_LANE0) >> 18) & 0x1FF;

	/* CBLane: Ls Mode */
	//0x6456_C0C0[7:0] write 0x01, Read 0x6456_0064[7:0]
	ufs_sprd_rmwl(priv->ufs_analog_reg, GENMASK(7, 0),
			0x1 << 0, MPHY_DIG_CFG48_LANE0);
	v[L6_UFS_AP_CBLANE_LS_MODE] =
			(readl((priv->ufs_analog_reg) + MPHY_2T2R_APB_REG) >> 0) & 0xFF;
	/* TX Lane0: Ls Sleep */
	//0x6456_C0C0[15:8] write 0x01,  Read 0x6456_0064[23:16]
	ufs_sprd_rmwl(priv->ufs_analog_reg, GENMASK(15, 8),
			0x1 << 8, MPHY_DIG_CFG48_LANE0);
	v[L6_UFS_AP_TX_LANE0_LS_SLEEP_1] =
			(readl((priv->ufs_analog_reg) + MPHY_2T2R_APB_REG) >> 16) & 0xFF;
	//0x6456_C0C0[15:8] write 0x02, Read 0x6456_0064[23:16]
	ufs_sprd_rmwl(priv->ufs_analog_reg, GENMASK(15, 8),
			0x2 << 8, MPHY_DIG_CFG48_LANE0);
	v[L6_UFS_AP_TX_LANE0_LS_SLEEP_2] =
			(readl((priv->ufs_analog_reg) + MPHY_2T2R_APB_REG) >> 16) & 0xFF;
	//0x6456_C0C0[15:8] write 0x03, Read 0x6456_0064[23:16]
	ufs_sprd_rmwl(priv->ufs_analog_reg, GENMASK(15, 8),
			0x3 << 8, MPHY_DIG_CFG48_LANE0);
	v[L6_UFS_AP_TX_LANE0_LS_SLEEP_3] =
			(readl((priv->ufs_analog_reg) + MPHY_2T2R_APB_REG) >> 16) & 0xFF;

	/* RX Lane0: LS sleep */
	//0x6456_C0C0[23:16] write 0x10, Read 0x6456_0064[15:8]
	ufs_sprd_rmwl(priv->ufs_analog_reg, GENMASK(23, 16),
			0x10 << 16, MPHY_DIG_CFG48_LANE0);
	v[L6_UFS_AP_RX_LANE0_LS_SLEEP_1] =
			(readl((priv->ufs_analog_reg) + MPHY_2T2R_APB_REG) >> 8) & 0xFF;
	//0x6456_C0C0[23:16] write 0x11, Read 0x6456_0064[15:8]
	ufs_sprd_rmwl(priv->ufs_analog_reg, GENMASK(23, 16),
			0x11 << 16, MPHY_DIG_CFG48_LANE0);
	v[L6_UFS_AP_RX_LANE0_LS_SLEEP_2] =
			(readl((priv->ufs_analog_reg) + MPHY_2T2R_APB_REG) >> 8) & 0xFF;
	//0x6456_C0C0[23:16] write 0x12, Read 0x6456_0064[15:8]
	ufs_sprd_rmwl(priv->ufs_analog_reg, GENMASK(23, 16),
			0x12 << 16, MPHY_DIG_CFG48_LANE0);
	v[L6_UFS_AP_RX_LANE0_LS_SLEEP_3] =
			(readl((priv->ufs_analog_reg) + MPHY_2T2R_APB_REG) >> 8) & 0xFF;

	/* TX Lane1: Hibern8 */
	//0x6456_C8C0[15:8] write 0x01,  Read 0x6456_0068[19:12]
	ufs_sprd_rmwl(priv->ufs_analog_reg, GENMASK(15, 8),
			0x1 << 8, MPHY_DIG_CFG48_LANE1);
	v[L6_UFS_AP_TX_LANE1_LS_SLEEP_1] =
			(readl((priv->ufs_analog_reg) + MPHY_2T2R_APB_REG1) >> 12) & 0xFF;
	//0x6456_C8C0[15:8] write 0x02,  Read 0x6456_0068[19:12]
	ufs_sprd_rmwl(priv->ufs_analog_reg, GENMASK(15, 8),
			0x2 << 8, MPHY_DIG_CFG48_LANE1);
	v[L6_UFS_AP_TX_LANE1_LS_SLEEP_2] =
			(readl((priv->ufs_analog_reg) + MPHY_2T2R_APB_REG1) >> 12) & 0xFF;
	//0x6456_C8C0[15:8] write 0x03,  Read 0x6456_0068[19:12]
	ufs_sprd_rmwl(priv->ufs_analog_reg, GENMASK(15, 8),
			0x3 << 8, MPHY_DIG_CFG48_LANE1);
	v[L6_UFS_AP_TX_LANE1_LS_SLEEP_3] =
			(readl((priv->ufs_analog_reg) + MPHY_2T2R_APB_REG1) >> 12) & 0xFF;

	/* RX Lane1: Hibern8 */
	//0x6456_C8C0[23:16] write 0x10, Read 0x6456_0068[11:4]
	ufs_sprd_rmwl(priv->ufs_analog_reg, GENMASK(23, 16),
			0x10 << 16, MPHY_DIG_CFG48_LANE1);
	v[L6_UFS_AP_RX_LANE1_LS_SLEEP_1] =
			(readl((priv->ufs_analog_reg) + MPHY_2T2R_APB_REG1) >> 4) & 0xFF;
	//0x6456_C8C0[23:16] write 0x11, Read 0x6456_0068[11:4]
	ufs_sprd_rmwl(priv->ufs_analog_reg, GENMASK(23, 16),
			0x11 << 16, MPHY_DIG_CFG48_LANE1);
	v[L6_UFS_AP_RX_LANE1_LS_SLEEP_2] =
			(readl((priv->ufs_analog_reg) + MPHY_2T2R_APB_REG1) >> 4) & 0xFF;
	//0x6456_C8C0[23:16] write 0x12, Read 0x6456_0068[11:4]
	ufs_sprd_rmwl(priv->ufs_analog_reg, GENMASK(23, 16),
			0x12 << 16, MPHY_DIG_CFG48_LANE1);
	v[L6_UFS_AP_RX_LANE1_LS_SLEEP_3] =
			(readl((priv->ufs_analog_reg) + MPHY_2T2R_APB_REG1) >> 4) & 0xFF;

	if (!preempt_count()) {
		p->preempt = false;

		if (!hba->active_uic_cmd) {
			p->active_uic_cmd = false;

			/* read unipro attr START */
			ufshcd_dme_get(hba, UIC_ARG_MIB(0xd094), &v[L6_UFS_UNIPRO_UIC_D094]);
			ufshcd_dme_get(hba, UIC_ARG_MIB(0xd095), &v[L6_UFS_UNIPRO_UIC_D095]);
			ufshcd_dme_get(hba, UIC_ARG_MIB(0xd097), &v[L6_UFS_UNIPRO_UIC_D097]);
			/* read unipro attr END */
		} else {
			p->active_uic_cmd = true;
		}

		/* read VOLTAGE START */
		v[L6_UFS_VDD1V85_VOLTAGE] = regulator_get_voltage(priv->vddgen0);
		v[L6_UFS_AVDD1V2_UFS_VOLTAGE] = regulator_get_voltage(priv->avdd12);
		v[L6_UFS_AVDD1V8_UFS_VOLTAGE] = regulator_get_voltage(priv->avdd18);
		v[L6_UFS_VDDEMMCCORE_VOLTAGE] = regulator_get_voltage(hba->vreg_info.vcc->reg);
		v[L6_UFS_VDDCORE_UFS_VOLTAGE] = regulator_get_voltage(priv->vddcore);
		v[L6_UFS_VDDMODEM_UFS_VOLTAGE] = regulator_get_voltage(priv->vddmodem);
		/* read VOLTAGE END */

	} else {
		p->preempt = true;
	}

}

int syscon_get_args(struct device *dev, struct ufs_sprd_host *host)
{
	int ret = 0;
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &priv->aon_apb_ufs_en,
				      "aon_apb_ufs_en");
	if (ret < 0)
		return ret;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &priv->ap_ahb_ufs_clk,
				      "ap_ahb_ufs_clk");
	if (ret < 0)
		return ret;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &priv->ap_apb_ufs_en,
				      "ap_apb_ufs_en");
	if (ret < 0)
		return ret;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &priv->ufs_refclk_on,
				      "ufs_refclk_on");
	if (ret < 0)
		return ret;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &priv->ahb_ufs_lp,
				      "ahb_ufs_lp");
	if (ret < 0)
		return ret;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &priv->ahb_ufs_force_isol,
				      "ahb_ufs_force_isol");
	if (ret < 0)
		return ret;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &priv->ahb_ufs_cb,
				      "ahb_ufs_cb");
	if (ret < 0)
		return ret;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &priv->ahb_ufs_cb,
				      "ahb_ufs_cb");
	if (ret < 0)
		return ret;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &priv->ahb_ufs_ies_en,
				      "ahb_ufs_ies_en");
	if (ret < 0)
		return ret;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &priv->ahb_ufs_cg_pclkreq,
				      "ahb_ufs_cg_pclkreq");
	if (ret < 0)
		return ret;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &priv->ap_apb_cfg_frc_on,
				      "ap_apb_cfg_frc_on");
	if (ret < 0)
		return ret;

	return ret;
}

static inline int ufs_sprd_mask(void __iomem *base, u32 mask, u32 reg)
{
	u32 tmp;

	tmp = readl((base) + (reg));
	if (tmp & mask)
		return 1;
	else
		return 0;
}

static void ufs_remap_or(struct syscon_ufs *sysconufs)
{
	unsigned int value = 0;

	regmap_read(sysconufs->regmap,
		    sysconufs->reg, &value);
	value =	value | sysconufs->mask;
	regmap_write(sysconufs->regmap,
		     sysconufs->reg, value);
}

static int ufs_sprd_priv_parse_dt(struct device *dev,
					struct ufs_hba *hba,
					struct ufs_sprd_host *host)
{
	struct resource *res;
	struct platform_device *pdev = to_platform_device(dev);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	syscon_get_args(dev, host);

	priv->ap_apb_ufs_rst = devm_reset_control_get(dev, "ufs_rst");
	if (IS_ERR(priv->ap_apb_ufs_rst)) {
		dev_err(dev, "%s get ufs_rst failed, err%ld\n",
			__func__, PTR_ERR(priv->ap_apb_ufs_rst));
		priv->ap_apb_ufs_rst = NULL;
		return -ENODEV;
	}

	priv->ap_apb_ufs_glb_rst = devm_reset_control_get(dev, "ufs_glb_rst");
	if (IS_ERR(priv->ap_apb_ufs_glb_rst)) {
		dev_err(dev, "%s get ufs_glb_rst failed, err%ld\n",
			__func__, PTR_ERR(priv->ap_apb_ufs_glb_rst));
		priv->ap_apb_ufs_glb_rst = NULL;
		return -ENODEV;
	}

	priv->ufs_cali_lanes = ufs_efuse_calib_data(pdev,
						"ufs_cali_lanes");
	if (priv->ufs_cali_lanes == -EPROBE_DEFER) {
		dev_err(&pdev->dev,
			"%s:get ufs_cali_lanes failed!\n", __func__);
		return -EPROBE_DEFER;
	}

	priv->ufs_trimbg = 0xf & (priv->ufs_cali_lanes >> 0x0);
	priv->ufs_rxtrim = 0xf & (priv->ufs_cali_lanes >> 0x4);
	priv->ufs_txtrim = 0xf & (priv->ufs_cali_lanes >> 0x8);

	dev_err(&pdev->dev, "%s: ufs_cali_lanes: 0x%x,trimbg=0x%x,rxtrim=0x%x,txtrim=0x%x\n",
		__func__, priv->ufs_cali_lanes, priv->ufs_trimbg,
		priv->ufs_rxtrim, priv->ufs_txtrim);


	res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "ufs_analog_reg");
	if (!res) {
		dev_err(dev, "Missing ufs_analog_reg register resource\n");
		return -ENODEV;
	}

	priv->ufs_analog_reg = devm_ioremap(dev, res->start, resource_size(res));
	if (IS_ERR(priv->ufs_analog_reg)) {
		dev_err(dev, "%s: could not map ufs_analog_reg, err %ld\n",
			__func__, PTR_ERR(priv->ufs_analog_reg));
		priv->ufs_analog_reg = NULL;
		return -ENODEV;
	}

	res = platform_get_resource_byname(pdev,
			IORESOURCE_MEM, "aon_apb_reg");
	if (!res) {
		dev_err(dev, "Missing aon_apb_reg register resource\n");
		return -ENODEV;
	}

	priv->aon_apb_reg = devm_ioremap(dev, res->start,
			resource_size(res));
	if (IS_ERR(priv->aon_apb_reg)) {
		dev_err(dev, "%s: could not map aon_apb_reg, err %ld\n",
				__func__, PTR_ERR(priv->aon_apb_reg));
		priv->aon_apb_reg = NULL;
		return -ENODEV;
	}

	priv->dbg_apb_reg = devm_ioremap(dev, REG_DEBUG_APB_BASE, 0x100);
	if (IS_ERR(priv->dbg_apb_reg)) {
		pr_err("error to ioremap ufs debug bus base.\n");
		priv->dbg_apb_reg = NULL;
	}

	priv->vddgen0 = devm_regulator_get(dev, "vdd-vddgen0");
	if (IS_ERR(priv->vddgen0)) {
		dev_err(&pdev->dev, "get vdd-vddgen0 regulator failed\n");
		return -ENODEV;
	}

	priv->avdd12 = devm_regulator_get(dev, "vdd-avdd12");
	if (IS_ERR(priv->avdd12)) {
		dev_err(&pdev->dev, "get vdd-avdd12 regulator failed\n");
		return -ENODEV;
	}

	priv->avdd18 = devm_regulator_get(dev, "vdd-avdd18");
	if (IS_ERR(priv->avdd18)) {
		dev_err(&pdev->dev, "get vdd-avdd18 regulator failed\n");
		return -ENODEV;
	}

	priv->vddcore = devm_regulator_get(dev, "vdd-vddcore");
	if (IS_ERR(priv->vddcore)) {
		dev_err(&pdev->dev, "get vdd-vddcore regulator failed\n");
		return -ENODEV;
	}

	priv->vddmodem = devm_regulator_get(dev, "vdd-vddmodem");
	if (IS_ERR(priv->vddmodem)) {
		dev_err(&pdev->dev, "get vdd-vddmodem regulator failed\n");
		return -ENODEV;
	}

	return 0;
}

void ufs_sprd_reset_pre(struct ufs_sprd_host *host)
{
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	ufs_remap_or(&(priv->ap_ahb_ufs_clk));
	regmap_update_bits(priv->aon_apb_ufs_en.regmap,
			   priv->aon_apb_ufs_en.reg,
			   priv->aon_apb_ufs_en.mask,
			   priv->aon_apb_ufs_en.mask);
	regmap_update_bits(priv->ahb_ufs_lp.regmap,
			   priv->ahb_ufs_lp.reg,
			   priv->ahb_ufs_lp.mask,
			   priv->ahb_ufs_lp.mask);
	regmap_update_bits(priv->ahb_ufs_force_isol.regmap,
			   priv->ahb_ufs_force_isol.reg,
			   priv->ahb_ufs_force_isol.mask,
			   0);

	if (readl(priv->aon_apb_reg + REG_AON_APB_AON_VER_ID))
		regmap_update_bits(priv->ahb_ufs_ies_en.regmap,
				  priv->ahb_ufs_ies_en.reg,
				  priv->ahb_ufs_ies_en.mask,
				  priv->ahb_ufs_ies_en.mask);
}

int ufs_sprd_reset(struct ufs_sprd_host *host)
{
	int ret = 0;
	u32 aon_ver_id = 0;
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	ret = sprd_get_soc_id(AON_VER_ID, &aon_ver_id, 1);
	if (ret) {
		dev_err(host->hba->dev, "fail to get soc id, ret = %d.\n", ret);
		goto out;
	}

	dev_info(host->hba->dev, "ufs hardware reset!\n");
	/* TODO: HW reset will be simple in next version. */

	regmap_update_bits(priv->ap_apb_ufs_en.regmap,
			   priv->ap_apb_ufs_en.reg,
			   priv->ap_apb_ufs_en.mask,
			   0);

	/* ufs global reset */
	ret = reset_control_assert(priv->ap_apb_ufs_glb_rst);
	if (ret) {
		dev_err(host->hba->dev, "assert ufs_glb_rst failed, ret = %d!\n", ret);
		goto out;
	}
	usleep_range(10, 20);
	ret = reset_control_deassert(priv->ap_apb_ufs_glb_rst);
	if (ret) {
		dev_err(host->hba->dev, "deassert ufs_glb_rst failed, ret = %d!\n", ret);
		goto out;
	}

	/* Configs need strict squence. */
	regmap_update_bits(priv->ap_apb_ufs_en.regmap,
			   priv->ap_apb_ufs_en.reg,
			   priv->ap_apb_ufs_en.mask,
			   priv->ap_apb_ufs_en.mask);
	/* ahb enable */
	ufs_remap_or(&(priv->ap_ahb_ufs_clk));

	regmap_update_bits(priv->aon_apb_ufs_en.regmap,
			   priv->aon_apb_ufs_en.reg,
			   priv->aon_apb_ufs_en.mask,
			   priv->aon_apb_ufs_en.mask);

	/* cbline reset */
	regmap_update_bits(priv->ahb_ufs_cb.regmap,
			   priv->ahb_ufs_cb.reg,
			   priv->ahb_ufs_cb.mask,
			   priv->ahb_ufs_cb.mask);

	/* apb reset */
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_2T2R_APB_RESETN,
			0, MPHY_2T2R_APB_REG1);
	usleep_range(1000, 1100);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_2T2R_APB_RESETN,
			MPHY_2T2R_APB_RESETN, MPHY_2T2R_APB_REG1);


	/* phy config */
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_CDR_MONITOR_BYPASS_MASK,
			MPHY_CDR_MONITOR_BYPASS_ENABLE, MPHY_DIG_CFG7_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_CDR_MONITOR_BYPASS_MASK,
			MPHY_CDR_MONITOR_BYPASS_ENABLE, MPHY_DIG_CFG7_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_RXOFFSETCALDONEOVR_MASK,
			MPHY_RXOFFSETCALDONEOVR_ENABLE, MPHY_DIG_CFG20_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_RXOFFOVRVAL_MASK,
			MPHY_RXOFFOVRVAL_ENABLE, MPHY_DIG_CFG20_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_RXCFGG1_MASK,
			MPHY_RXCFGG1_VAL, MPHY_DIG_CFG49_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_RXCFGG1_MASK,
			MPHY_RXCFGG1_VAL, MPHY_DIG_CFG49_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_RXCFGG3_MASK,
			MPHY_RXCFGG3_VAL, MPHY_DIG_CFG51_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_RXCFGG3_MASK,
			MPHY_RXCFGG3_VAL, MPHY_DIG_CFG51_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, FIFO_ENABLE_MASK,
			FIFO_ENABLE_MASK, MPHY_LANE0_FIFO);
	ufs_sprd_rmwl(priv->ufs_analog_reg, FIFO_ENABLE_MASK,
			FIFO_ENABLE_MASK, MPHY_LANE1_FIFO);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_TACTIVATE_TIME_200US,
			MPHY_TACTIVATE_TIME_200US, MPHY_TACTIVATE_TIME_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_TACTIVATE_TIME_200US,
			MPHY_TACTIVATE_TIME_200US, MPHY_TACTIVATE_TIME_LANE1);

	/* mphy cfg: DCVS */
	ufs_sprd_rmwl(priv->ufs_analog_reg, BIT(18), BIT(18), MPHY_DIG_CFG0_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, BIT(18), BIT(18), MPHY_DIG_CFG0_LANE1);

	/* mphy cfg: LANE 0 manual afe */
	ufs_sprd_rmwl(priv->ufs_analog_reg, BIT(15), BIT(15), MPHY_DIG_CFG0_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, BIT(0), BIT(0), MPHY_DIG_CFG1_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, BIT(3), BIT(3), MPHY_DIG_CFG1_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MASK_OFFK_INIT_AFE,
		VAL_OFFK_INIT_AFE, MPHY_DIG_CFG0_LANE0);

	/* mphy cfg: LANE 1 manual afe */
	ufs_sprd_rmwl(priv->ufs_analog_reg, BIT(15), BIT(15), MPHY_DIG_CFG0_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, BIT(0), BIT(0), MPHY_DIG_CFG1_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, BIT(3), BIT(3), MPHY_DIG_CFG1_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MASK_OFFK_INIT_AFE,
		VAL_OFFK_INIT_AFE, MPHY_DIG_CFG0_LANE1);

	/* mphy cfg: config hs sync length */
	ufs_sprd_rmwl(priv->ufs_analog_reg, (MASK_HS_G2_SYNC_LENGTH | MASK_HS_G3_SYNC_LENGTH),
		(VAL_SYNC_LENGTH_CFG << 8 | VAL_SYNC_LENGTH_CFG), MPHY_DIG_CFG72_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MASK_HS_G1_SYNC_LENGTH,
		(VAL_SYNC_LENGTH_CFG << 8), MPHY_DIG_CFG70_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, (MASK_HS_G2_SYNC_LENGTH | MASK_HS_G3_SYNC_LENGTH),
		(VAL_SYNC_LENGTH_CFG << 8 | VAL_SYNC_LENGTH_CFG), MPHY_DIG_CFG72_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MASK_HS_G1_SYNC_LENGTH,
		(VAL_SYNC_LENGTH_CFG << 8), MPHY_DIG_CFG70_LANE1);

	/* mphy cfg: config hs step cycle */
	ufs_sprd_rmwl(priv->ufs_analog_reg, (MASK_RX_STEP4_CYCLE | MASK_RX_STEP3_CYCLE),
		(VAL_RX_STEP4_CYCLE | VAL_RX_STEP3_CYCLE), MPHY_DIG_CFG56_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, (MASK_RX_STEP2_CYCLE | MASK_RX_STEP1_CYCLE),
		(VAL_RX_STEP2_CYCLE | VAL_RX_STEP1_CYCLE), MPHY_DIG_CFG57_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, (MASK_RX_STEP4_CYCLE | MASK_RX_STEP3_CYCLE),
		(VAL_RX_STEP4_CYCLE | VAL_RX_STEP3_CYCLE), MPHY_DIG_CFG58_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, (MASK_RX_STEP2_CYCLE | MASK_RX_STEP1_CYCLE),
		(VAL_RX_STEP2_CYCLE | VAL_RX_STEP1_CYCLE), MPHY_DIG_CFG59_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, (MASK_RX_STEP4_CYCLE | MASK_RX_STEP3_CYCLE),
		(VAL_RX_STEP4_CYCLE | VAL_RX_STEP3_CYCLE), MPHY_DIG_CFG60_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, (MASK_RX_STEP2_CYCLE | MASK_RX_STEP1_CYCLE),
		(VAL_RX_STEP2_CYCLE | VAL_RX_STEP1_CYCLE), MPHY_DIG_CFG61_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, (MASK_RX_STEP4_CYCLE | MASK_RX_STEP3_CYCLE),
		(VAL_RX_STEP4_CYCLE | VAL_RX_STEP3_CYCLE), MPHY_DIG_CFG56_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, (MASK_RX_STEP2_CYCLE | MASK_RX_STEP1_CYCLE),
		(VAL_RX_STEP2_CYCLE | VAL_RX_STEP1_CYCLE), MPHY_DIG_CFG57_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, (MASK_RX_STEP4_CYCLE | MASK_RX_STEP3_CYCLE),
		(VAL_RX_STEP4_CYCLE | VAL_RX_STEP3_CYCLE), MPHY_DIG_CFG58_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, (MASK_RX_STEP2_CYCLE | MASK_RX_STEP1_CYCLE),
		(VAL_RX_STEP2_CYCLE | VAL_RX_STEP1_CYCLE), MPHY_DIG_CFG59_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, (MASK_RX_STEP4_CYCLE | MASK_RX_STEP3_CYCLE),
		(VAL_RX_STEP4_CYCLE | VAL_RX_STEP3_CYCLE), MPHY_DIG_CFG60_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, (MASK_RX_STEP2_CYCLE | MASK_RX_STEP1_CYCLE),
		(VAL_RX_STEP2_CYCLE | VAL_RX_STEP1_CYCLE), MPHY_DIG_CFG61_LANE1);

	/* cbline reset */
	regmap_update_bits(priv->ahb_ufs_cb.regmap,
			  priv->ahb_ufs_cb.reg,
			  priv->ahb_ufs_cb.mask,
			  0);

	/* enable refclk */
	regmap_update_bits(priv->ufs_refclk_on.regmap,
			  priv->ufs_refclk_on.reg,
			  priv->ufs_refclk_on.mask,
			  priv->ufs_refclk_on.mask);
	regmap_update_bits(priv->ahb_ufs_lp.regmap,
			  priv->ahb_ufs_lp.reg,
			  priv->ahb_ufs_lp.mask,
			  priv->ahb_ufs_lp.mask);
	regmap_update_bits(priv->ahb_ufs_force_isol.regmap,
			  priv->ahb_ufs_force_isol.reg,
			  priv->ahb_ufs_force_isol.mask,
			  0);

	/* ufs soft reset */
	ret = reset_control_assert(priv->ap_apb_ufs_rst);
	if (ret) {
		dev_err(host->hba->dev, "assert ufs_glb_rst failed, ret = %d!\n", ret);
		goto out;
	}
	usleep_range(10, 20);
	ret = reset_control_deassert(priv->ap_apb_ufs_rst);
	if (ret) {
		dev_err(host->hba->dev, "deassert ufs_glb_rst failed, ret = %d!\n", ret);
		goto out;
	}

	regmap_update_bits(priv->ahb_ufs_ies_en.regmap,
			  priv->ahb_ufs_ies_en.reg,
			  priv->ahb_ufs_ies_en.mask,
			  priv->ahb_ufs_ies_en.mask);
	ufs_remap_or(&(priv->ahb_ufs_cg_pclkreq));

	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_ANR_MPHY_CTRL2_REFCLKON_MASK,
			MPHY_ANR_MPHY_CTRL2_REFCLKON_VAL, MPHY_ANR_MPHY_CTRL2);
	usleep_range(1, 2);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_REG_SEL_CFG_0_REFCLKON_MASK,
			MPHY_REG_SEL_CFG_0_REFCLKON_VAL, MPHY_REG_SEL_CFG_0);
	usleep_range(1, 2);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_APB_REFCLK_AUTOH8_EN_MASK,
			MPHY_APB_REFCLK_AUTOH8_EN_VAL, MPHY_DIG_CFG14_LANE0);

	usleep_range(1, 2);
	if (aon_ver_id == AON_VER_UFS) {
		ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_APB_PLLTIMER_MASK,
				MPHY_APB_PLLTIMER_VAL, MPHY_DIG_CFG18_LANE0);
		ufs_sprd_rmwl(priv->ufs_analog_reg,
				MPHY_APB_HSTXSCLKINV1_MASK,
				MPHY_APB_HSTXSCLKINV1_VAL,
				MPHY_DIG_CFG19_LANE0);
	}

	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_APB_RX_CFGRXBIASLSENVAL_MASK,
			MPHY_APB_RX_CFGRXBIASLSENVAL_MASK, MPHY_DIG_CFG32_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_APB_RX_CFGRXBIASLSENOVR_MASK,
			MPHY_APB_RX_CFGRXBIASLSENOVR_MASK, MPHY_DIG_CFG32_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_APB_OVR_REG_LS_LDO_STABLE_MASK,
			MPHY_APB_OVR_REG_LS_LDO_STABLE_MASK, MPHY_DIG_CFG1_LANE0);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_APB_REG_LS_LDO_STABLE_MASK,
			MPHY_APB_REG_LS_LDO_STABLE_MASK, MPHY_DIG_CFG17_LANE0);

	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_APB_RX_CFGRXBIASLSENVAL_MASK,
			MPHY_APB_RX_CFGRXBIASLSENVAL_MASK, MPHY_DIG_CFG32_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_APB_RX_CFGRXBIASLSENOVR_MASK,
			MPHY_APB_RX_CFGRXBIASLSENOVR_MASK, MPHY_DIG_CFG32_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_APB_OVR_REG_LS_LDO_STABLE_MASK,
			MPHY_APB_OVR_REG_LS_LDO_STABLE_MASK, MPHY_DIG_CFG1_LANE1);
	ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_APB_REG_LS_LDO_STABLE_MASK,
			MPHY_APB_REG_LS_LDO_STABLE_MASK, MPHY_DIG_CFG17_LANE1);

out:
	return ret;
}

static int is_ufs_sprd_host_in_pwm(struct ufs_hba *hba)
{
	int ret = 0;
	u32 pwr_mode = 0;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_PWRMODE),
			     &pwr_mode);
	if (ret)
		goto out;
	if (((pwr_mode>>0)&0xf) == SLOWAUTO_MODE ||
		((pwr_mode>>0)&0xf) == SLOW_MODE     ||
		((pwr_mode>>4)&0xf) == SLOWAUTO_MODE ||
		((pwr_mode>>4)&0xf) == SLOW_MODE) {
		ret = SLOW_MODE | (SLOW_MODE << 4);
	}

out:
	return ret;
}

static int sprd_ufs_pwrchange(struct ufs_hba *hba)
{
	int ret;
	struct ufs_pa_layer_attr pwr_info;

	pwr_info.gear_rx = UFS_PWM_G1;
	pwr_info.gear_tx = UFS_PWM_G1;
	pwr_info.lane_rx = 1;
	pwr_info.lane_tx = 1;
	pwr_info.pwr_rx = SLOW_MODE;
	pwr_info.pwr_tx = SLOW_MODE;
	pwr_info.hs_rate = 0;

	ret = ufshcd_config_pwr_mode(hba, &(pwr_info));
	if (ret)
		goto out;
	if ((((hba->max_pwr_info.info.pwr_tx) << 4) |
		(hba->max_pwr_info.info.pwr_rx)) == HS_MODE_VAL)
		ret = ufshcd_config_pwr_mode(hba, &(hba->max_pwr_info.info));

out:
	return ret;

}

static void sprd_ufs_vh_send_uic(struct ufs_hba *hba, struct uic_command *ucmd, int str)
{
	/*
	 * Used for DCO cali.
	 * We will directly carry out LINK ops in the linkup PRE stage. Therefore,
	 * when the code reaches here, if DP already exists, there is no need to
	 * send the LINK cmd, which is converted to the peer get cmd. If DP doesn't
	 * exist, it indicates that the LINK transmisson fails int the PRE phase, and
	 * the LINK cmd will be resent.
	 */
	if (ucmd->command == UIC_CMD_DME_LINK_STARTUP && str == UFS_CMD_SEND &&
			(ufshcd_readl(hba, REG_CONTROLLER_STATUS) & DEVICE_PRESENT)) {
		ucmd->command = UIC_CMD_DME_PEER_GET;
		ucmd->argument1 = UIC_ARG_MIB(PA_AVAILRXDATALANES);
		ufshcd_writel(hba, ucmd->argument1, REG_UIC_COMMAND_ARG_1);
	}
}

/*
 * ufs_sprd_init - find other essential mmio bases
 * @hba: host controller instance
 * Returns 0 on success, non-zero value on failure
 */
static int ufs_sprd_init(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct ufs_sprd_host *host;
	int ret = 0;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	host->ufs_priv_data = devm_kzalloc(dev,
			sizeof(struct ufs_sprd_ums9230_data), GFP_KERNEL);
	if (!host->ufs_priv_data)
		return -ENOMEM;

	host->hba = hba;
	ufshcd_set_variant(hba, host);

	ret = ufs_sprd_priv_parse_dt(dev, hba, host);
	if (ret < 0)
		return ret;

	hba->host->hostt->ioctl = ufshcd_sprd_ioctl;
#ifdef CONFIG_COMPAT
	hba->host->hostt->compat_ioctl = ufshcd_sprd_ioctl;
#endif

	host->priv_vh_send_uic = sprd_ufs_vh_send_uic;

	hba->quirks |= UFSHCD_QUIRK_BROKEN_UFS_HCI_VERSION |
		       UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS;
	hba->caps |= UFSHCD_CAP_CLK_GATING | UFSHCD_CAP_CRYPTO |
		     UFSHCD_CAP_WB_EN | UFSHCD_CAP_HIBERN8_WITH_CLK_GATING;

	ufs_sprd_reset_pre(host);

	ufs_sprd_get_gic_reg(hba);
	ufs_sprd_dbg_regs_hist_register(hba, L6_UFS_DBG_REGS_MAX, l6_dbg_regs_name);
	ufs_sprd_debug_init(hba);

	return 0;
}

/*
 * ufs_sprd_hw_init - controller enable and reset
 * @hba: host controller instance
 */
int ufs_sprd_hw_init(struct ufs_hba *hba)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	return ufs_sprd_reset(host);
}

static void ufs_sprd_exit(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	devm_kfree(dev, host);
	hba->priv = NULL;
}

static u32 ufs_sprd_get_ufs_hci_version(struct ufs_hba *hba)
{
	return UFSHCI_VERSION_21;
}

static int ufs_sprd_hce_enable_notify(struct ufs_hba *hba,
				      enum ufs_notify_change_status status)
{
	int err = 0;
	unsigned long flags;

	switch (status) {
	case PRE_CHANGE:
		/* Do hardware reset before host controller enable. */
		err = ufs_sprd_hw_init(hba);
		if (err) {
			dev_err(hba->dev, "%s: ufs hardware init failed!\n", __func__);
			return err;
		}

		spin_lock_irqsave(hba->host->host_lock, flags);
		ufshcd_writel(hba, 0, REG_AUTO_HIBERNATE_IDLE_TIMER);
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		hba->capabilities &= ~MASK_AUTO_HIBERN8_SUPPORT;
		hba->ahit = 0;

		ufshcd_writel(hba, CONTROLLER_ENABLE, REG_CONTROLLER_ENABLE);
		break;
	case POST_CHANGE:
		ufshcd_writel(hba, CLKDIV, HCLKDIV_REG);
		break;
	default:
		dev_err(hba->dev, "%s: invalid status %d\n", __func__, status);
		err = -EINVAL;
		break;
	}

	return err;
}

static int ufs_sprd_apply_dev_quirks(struct ufs_hba *hba)
{
	int ret = 0;
	u32 granularity, peer_granularity;
	u32 pa_tactivate, peer_pa_tactivate;
	u32 pa_tactivate_us, peer_pa_tactivate_us, max_pa_tactivate_us;
	u8 gran_to_us_table[] = {1, 4, 8, 16, 32, 100};
	u32 new_pa_tactivate, new_peer_pa_tactivate;

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

	pa_tactivate_us = pa_tactivate * gran_to_us_table[granularity - 1];
	peer_pa_tactivate_us = peer_pa_tactivate *
			gran_to_us_table[peer_granularity - 1];
	max_pa_tactivate_us = (pa_tactivate_us > peer_pa_tactivate_us) ?
			pa_tactivate_us : peer_pa_tactivate_us;

	new_peer_pa_tactivate = (max_pa_tactivate_us + 400) /
			gran_to_us_table[peer_granularity - 1];

	ret = ufshcd_dme_peer_set(hba, UIC_ARG_MIB(PA_TACTIVATE),
				  new_peer_pa_tactivate);
	if (ret) {
		dev_err(hba->dev, "%s: peer_pa_tactivate set err ", __func__);
		goto out;
	}

	new_pa_tactivate = (max_pa_tactivate_us + 300) /
			gran_to_us_table[granularity - 1];
	ret = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TACTIVATE),
			     new_pa_tactivate);
	if (ret) {
		dev_err(hba->dev, "%s: pa_tactivate set err ", __func__);
		goto out;
	}

	dev_warn(hba->dev, "%s: %d,%d,%d,%d",
		 __func__, new_peer_pa_tactivate,
		 peer_granularity, new_pa_tactivate, granularity);

out:
	return ret;
}

static void ufs_sprd_dco_calibration(struct ufs_hba *hba, struct uic_command *ucmd)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9230_data *priv = (struct ufs_sprd_ums9230_data *) host->ufs_priv_data;
	int value = 0;
	int apb_dco_cal_result = 0;
	int apb_dco_cnt_result = 0;

	if (ucmd->command == UIC_CMD_DME_LINK_STARTUP) {
		while (1) {
			if (ktime_to_us(ktime_sub(ktime_get(),
					priv->last_linkup_time)) > WAIT_1MS_TIMEOUT) {
				value = readl((priv->ufs_analog_reg) + MPHY_DIG_CFG62_LANE0);
				apb_dco_cal_result = (value >> 24);
				break;
			}
		}

		if (apb_dco_cal_result < APB_DCO_CAL_RESULT_RANGE) {
			ufs_sprd_rmwl(priv->ufs_analog_reg,
				      MPHY_APB_REG_DCO_CTRLBIT,
				      MPHY_APB_REG_DCO_VALUE,
				      MPHY_DIG_CFG15_LANE0);
			ufs_sprd_rmwl(priv->ufs_analog_reg,
				      MPHY_APB_OVR_REG_DCO_CTRLBIT,
				      MPHY_APB_OVR_REG_DCO_VALUE,
				      MPHY_DIG_CFG1_LANE0);
		}

		value = readl((priv->ufs_analog_reg) + MPHY_DIG_CFG62_LANE0);
		apb_dco_cal_result = (value >> 24);
		value = readl((priv->ufs_analog_reg) + MPHY_DIG_CFG66_LANE0);
		apb_dco_cnt_result = (value >> 18) & 0x1FF;

		dev_err(hba->dev, "apb_dco_cal_result: 0x%x, apb_dco_cnt_result: 0x%x\n",
			   apb_dco_cal_result, apb_dco_cnt_result);
	}
}

static int ufs_sprd_wait_for_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	int ret;
	unsigned long flags;

	ufs_sprd_dco_calibration(hba, uic_cmd);

	lockdep_assert_held(&hba->uic_cmd_mutex);

	if (wait_for_completion_timeout(&uic_cmd->done, msecs_to_jiffies(500 /* MS */))) {
		ret = uic_cmd->argument2 & MASK_UIC_COMMAND_RESULT;
	} else {
		ret = -ETIMEDOUT;
		dev_err(hba->dev,
			"uic cmd 0x%x with arg3 0x%x completion timeout\n",
			uic_cmd->command, uic_cmd->argument3);

		if (!uic_cmd->cmd_active) {
			dev_err(hba->dev, "%s: UIC cmd has been completed, return the result\n",
				__func__);
			ret = uic_cmd->argument2 & MASK_UIC_COMMAND_RESULT;
		}
	}

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->active_uic_cmd = NULL;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return ret;
}

static inline void ufs_sprd_dispatch_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	lockdep_assert_held(&hba->uic_cmd_mutex);

	WARN_ON(hba->active_uic_cmd);

	hba->active_uic_cmd = uic_cmd;

	/* Write Args */
	ufshcd_writel(hba, uic_cmd->argument1, REG_UIC_COMMAND_ARG_1);
	ufshcd_writel(hba, uic_cmd->argument2, REG_UIC_COMMAND_ARG_2);
	ufshcd_writel(hba, uic_cmd->argument3, REG_UIC_COMMAND_ARG_3);

	ufs_sprd_uic_cmd_record(hba, uic_cmd, (int) UFS_CMD_SEND);

	/* Write UIC Cmd */
	ufshcd_writel(hba, uic_cmd->command & COMMAND_OPCODE_MASK, REG_UIC_COMMAND);

	if (uic_cmd->command == UIC_CMD_DME_LINK_STARTUP)
		priv->last_linkup_time = ktime_get();
}

static int ufs_sprd_send_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd,
		      bool completion)
{
	lockdep_assert_held(&hba->uic_cmd_mutex);
	lockdep_assert_held(hba->host->host_lock);

	if (!(ufshcd_readl(hba, REG_CONTROLLER_STATUS) & UIC_COMMAND_READY)) {
		dev_err(hba->dev,
			"Controller not ready to accept UIC commands\n");
		return -EIO;
	}

	if (completion)
		init_completion(&uic_cmd->done);

	uic_cmd->cmd_active = 1;
	ufs_sprd_dispatch_uic_cmd(hba, uic_cmd);

	return 0;
}

static int ufs_sprd_dme_link_startup(struct ufs_hba *hba)
{
	struct uic_command uic_cmd = {0};
	unsigned long flags;
	int retry = 5;
	int ret;

	uic_cmd.command = UIC_CMD_DME_LINK_STARTUP;

link_retry:
	ufshcd_hold(hba, false);
	mutex_lock(&hba->uic_cmd_mutex);
	usleep_range(1000, 1050);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = ufs_sprd_send_uic_cmd(hba, &uic_cmd, true);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	if (!ret)
		ret = ufs_sprd_wait_for_uic_cmd(hba, &uic_cmd);

	mutex_unlock(&hba->uic_cmd_mutex);
	ufshcd_release(hba);

	if (ret)
		dev_dbg(hba->dev, "dme-link-startup: error code %d\n", ret);

	if (!ret && !(ufshcd_readl(hba, REG_CONTROLLER_STATUS) & DEVICE_PRESENT)) {
		ufshcd_update_evt_hist(hba, UFS_EVT_LINK_STARTUP_FAIL, 0);
		dev_err(hba->dev, "%s: Device not present\n", __func__);
		ret = -ENXIO;
		goto out;
	}

	/*
	 * DME link lost indication is only received when link is up,
	 * but we can't be sure if the link is up until link startup
	 * succeeds. So reset the local Uni-Pro and try again.
	 */
	if (ret && ufshcd_hba_enable(hba)) {
		ufshcd_update_evt_hist(hba, UFS_EVT_LINK_STARTUP_FAIL, (u32)ret);
		goto out;
	}

	if (ret && retry--)
		goto link_retry;

out:
	return ret;
}

static int ufs_sprd_link_startup_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status status)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;
	int err = 0;

	switch (status) {
	case PRE_CHANGE:
		/* UFS device needs 32us PA_Saveconfig Time */
		ufshcd_dme_set(hba, UIC_ARG_MIB(VS_DEBUGSAVECONFIGTIME), 0x13);

		/*
		 * Some UFS devices (and may be host) have issues if LCC is
		 * enabled. So we are setting PA_Local_TX_LCC_Enable to 0
		 * before link startup which will make sure that both host
		 * and device TX LCC are disabled once link startup is
		 * completed.
		 */
		if (ufshcd_get_local_unipro_ver(hba) != UFS_UNIPRO_VER_1_41)
			err = ufshcd_dme_set(hba,
					UIC_ARG_MIB(PA_LOCAL_TX_LCC_ENABLE),
					0);

		ufs_sprd_dme_link_startup(hba);

		break;
	case POST_CHANGE:
		hba->clk_gating.delay_ms = 10;

		/* keep UFS cfgclk AON in runtime state */
		regmap_update_bits(priv->ap_apb_cfg_frc_on.regmap,
				   priv->ap_apb_cfg_frc_on.reg,
				   priv->ap_apb_cfg_frc_on.mask,
				   priv->ap_apb_cfg_frc_on.mask);

		/* keep UFS MPHY_ANA_POWERDOWN FRC in runtime state */
		ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_REG_SEL_CFG_0_ANA_POWERDOWN_MASK,
				MPHY_REG_SEL_CFG_0_ANA_POWERDOWN_VAL, MPHY_REG_SEL_CFG_0);
		ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_POWER_REG_ANA_POWERDOWN_MASK,
				0, MPHY_POWER_REG);

		break;
	default:
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
		memcpy(final_params, desired_pwr_mode, sizeof(struct ufs_pa_layer_attr));
		break;
	case POST_CHANGE:
		/* Set auto h8 ilde time to 10ms */
		//ufshcd_auto_hibern8_enable(hba);
		break;
	default:
		err = -EINVAL;
		break;
	}
	ufs_sprd_pwr_change_compare(hba, status, final_params, &err);

out:
	return err;
}

void ufs_set_hstxsclk(struct ufs_hba *hba)
{
	int ret;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	ret = ufs_sprd_mask(priv->ufs_analog_reg,
			MPHY_APB_HSTXSCLKINV1_MASK,
			MPHY_DIG_CFG19_LANE0);
	if (!ret) {
		ufs_sprd_rmwl(priv->ufs_analog_reg,
				MPHY_APB_HSTXSCLKINV1_MASK,
				MPHY_APB_HSTXSCLKINV1_VAL,
				MPHY_DIG_CFG19_LANE0);
		pr_err("ufs_pwm2hs set hstxsclk\n");
	}

}
static int sprd_ufs_pwmmode_change(struct ufs_hba *hba)
{
	int ret;
	struct ufs_pa_layer_attr pwr_info;

	ret = is_ufs_sprd_host_in_pwm(hba);
	if (ret == (SLOW_MODE|(SLOW_MODE<<4)))
		return 0;

	pwr_info.gear_rx = UFS_PWM_G3;
	pwr_info.gear_tx = UFS_PWM_G3;
	pwr_info.lane_rx = 2;
	pwr_info.lane_tx = 2;
	pwr_info.pwr_rx = SLOW_MODE;
	pwr_info.pwr_tx = SLOW_MODE;
	pwr_info.hs_rate = 0;

	ret = ufshcd_config_pwr_mode(hba, &(pwr_info));

	return ret;
}

int hibern8_exit_check(struct ufs_hba *hba,
				enum uic_cmd_dme cmd,
				enum ufs_notify_change_status status)
{
	int ret;
	u32 aon_ver_id = 0;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	ret = is_ufs_sprd_host_in_pwm(hba);
	if (ret == (SLOW_MODE|(SLOW_MODE<<4))) {
		ret = sprd_get_soc_id(AON_VER_ID, &aon_ver_id, 1);
		if (ret) {
			pr_err("fail to get soc id\n");
			return ret;
		}
		if (host->ioctl_status == UFS_IOCTL_AFC_EXIT || aon_ver_id == AON_VER_UFS) {
			ret = sprd_ufs_pwrchange(hba);
			if (ret) {
				pr_err("ufs_pwm2hs err");
			} else {
				ret = is_ufs_sprd_host_in_pwm(hba);
				if (ret == (SLOW_MODE|(SLOW_MODE<<4)) &&
						((((hba->max_pwr_info.info.pwr_tx) << 4) |
						  (hba->max_pwr_info.info.pwr_rx)) == HS_MODE_VAL))
					pr_err("ufs_pwm2hs fail");
				else {
					pr_err("ufs_pwm2hs succ\n");
					if (host->ioctl_status == UFS_IOCTL_AFC_EXIT)
						complete(&host->hs_async_done);
				}
			}
		}
	}
	return 0;

}

static void ufs_sprd_hibern8_notify(struct ufs_hba *hba,
		enum uic_cmd_dme cmd,
		enum ufs_notify_change_status status)
{
	int ret;
	unsigned long flags;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	switch (status) {
	case PRE_CHANGE:
		if (cmd == UIC_CMD_DME_HIBER_ENTER) {
			spin_lock_irqsave(hba->host->host_lock, flags);
			ufshcd_writel(hba, 0, REG_AUTO_HIBERNATE_IDLE_TIMER);
			spin_unlock_irqrestore(hba->host->host_lock, flags);
		}
		break;
	case POST_CHANGE:
		if (cmd == UIC_CMD_DME_HIBER_EXIT) {
			down_write(&hba->clk_scaling_lock);
			hba->caps &= ~UFSHCD_CAP_HIBERN8_WITH_CLK_GATING;
			if (host->ioctl_status == UFS_IOCTL_ENTER_MODE) {
				ret = sprd_ufs_pwmmode_change(hba);
				if (ret)
					pr_err("change pwm mode failed!\n");
				else {
					pr_err("ufs_2pwm succ");
					complete(&host->pwm_async_done);
				}
			} else {
				hibern8_exit_check(hba, cmd, status);
			}
			hba->caps |= UFSHCD_CAP_HIBERN8_WITH_CLK_GATING;
			up_write(&hba->clk_scaling_lock);
			/* Set auto h8 ilde time to 10ms */
			//ufshcd_auto_hibern8_enable(hba);
		}
		break;
	default:
		break;
	}
}

static void ufs_sprd_fixup_dev_quirks(struct ufs_hba *hba)
{
#ifdef CONFIG_SPRD_UFS_PROC_FS
	/* vendor UFS UID info decode. */
	ufshcd_decode_ufs_uid(hba);
#endif
}

static int ufs_sprd_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op,
						enum ufs_notify_change_status status)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	switch (status) {
	case PRE_CHANGE:
		break;
	case POST_CHANGE:
		hba->rpm_lvl = UFS_PM_LVL_1;
		hba->spm_lvl = UFS_PM_LVL_5;
		hba->uic_link_state = UIC_LINK_OFF_STATE;

		/* close UFS cfgclk AON in suspend state */
		regmap_update_bits(priv->ap_apb_cfg_frc_on.regmap,
				   priv->ap_apb_cfg_frc_on.reg,
				   priv->ap_apb_cfg_frc_on.mask,
				   0);

		/* restore UFS MPHY_ANA_POWERDOWN ctl by PMU */
		ufs_sprd_rmwl(priv->ufs_analog_reg, MPHY_REG_SEL_CFG_0_ANA_POWERDOWN_MASK,
				0, MPHY_REG_SEL_CFG_0);
		break;
	default:
		break;
	}
	return 0;
}

static int ufs_sprd_device_reset(struct ufs_hba *hba)
{
	if (sprd_ufs_debug_is_supported(hba))
		ufshcd_common_trace(hba, UFS_TRACE_RESET_AND_RESTORE, NULL);

	return 0;
}

static void ufs_sprd_dbg_register_dump(struct ufs_hba *hba)
{
	u32 data = 0;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct ufs_sprd_ums9230_data *priv =
		(struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

	sprd_ufs_print_err_cnt(hba);
	ufs_sprd_get_debug_regs(hba, UFS_EVT_CNT, &data);

	dev_err(hba->dev, "%s: MPHY_REG_SEL_CFG_0:0x%x,MPHY_POWER_REG:0x%x", __func__,
			readl(priv->ufs_analog_reg + MPHY_REG_SEL_CFG_0),
			readl(priv->ufs_analog_reg + MPHY_POWER_REG));

	sprd_ufs_debug_err_dump(hba);
}

static int ufs_sprd_setup_clocks(struct ufs_hba *hba, bool on,
				 enum ufs_notify_change_status status)
{
	struct ufs_clk_dbg clk_tmp = {};
	struct ufs_sprd_ums9230_data *priv = NULL;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	if (sprd_ufs_debug_is_supported(hba)) {
		clk_tmp.status = status;
		clk_tmp.on = on;
		ufshcd_common_trace(hba, UFS_TRACE_CLK_GATE, &clk_tmp);
	}

	if (on == false && status == PRE_CHANGE) {
		if (host != NULL)
			priv = (struct ufs_sprd_ums9230_data *) host->ufs_priv_data;

		if (!ufshcd_is_link_hibern8(hba) || priv == NULL) {
			dev_info(hba->dev, "%s during ufs init or setup clock not in h8", __func__);
			return 0;
		}

		/* mphy requires delay between h8 enter and close cfg clk */
		usleep_range(200, 210);
	}

	return 0;
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

	if (sprd_ufs_debug_is_supported(hba)) {
		evt_tmp.id = evt;
		evt_tmp.val = *(u32 *)data;
		ufshcd_common_trace(hba, UFS_TRACE_EVT, &evt_tmp);
	}

	if (evt != UFS_EVT_DEV_RESET && evt != UFS_EVT_HOST_RESET)
		ufs_sprd_get_debug_regs(hba, evt, data);
}

const struct ufs_hba_variant_ops ufs_hba_sprd_ums9230_vops = {
	.name = "sprd,ufshc-ums9230",
	.init = ufs_sprd_init,
	.exit = ufs_sprd_exit,
	.get_ufs_hci_version = ufs_sprd_get_ufs_hci_version,
	.setup_clocks = ufs_sprd_setup_clocks,
	.hce_enable_notify = ufs_sprd_hce_enable_notify,
	.link_startup_notify = ufs_sprd_link_startup_notify,
	.pwr_change_notify = ufs_sprd_pwr_change_notify,
	.hibern8_notify = ufs_sprd_hibern8_notify,
	.apply_dev_quirks = ufs_sprd_apply_dev_quirks,
	.fixup_dev_quirks = ufs_sprd_fixup_dev_quirks,
	.suspend = ufs_sprd_suspend,
	.dbg_register_dump = ufs_sprd_dbg_register_dump,
	.device_reset = ufs_sprd_device_reset,
	.event_notify = ufs_sprd_update_evt_hist,
};
EXPORT_SYMBOL(ufs_hba_sprd_ums9230_vops);
