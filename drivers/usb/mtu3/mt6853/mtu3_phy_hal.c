// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/phy/phy.h>
#include <linux/phy/mediatek/mtk_usb_phy.h>
#include <linux/pm_qos.h>
#include <linux/regmap.h>

#include "mtu3.h"
#include "mtu3_hal.h"
#include "mtu3_priv.h"

#define USBPHYACR0		(0x0000)
#define USBPHYACR0_MASK		(0xffffffff)
#define U2PHYDTM1		(0x006C)
#define FORCE_LINESTATE_C(x)	(((x) & 0x1) << 14)
#define RG_LINESTATE_C(x)	(((x) & 0x3) << 6)

#define RG_USBPLL_192M_OPP_EN	(0x304)
#define RG_USBPLL_DIV13_C(x)	(((x) & 0x1) << 21)

#define USBPLL_FS	(2704000000)
#define USBPLL_HS	(2496000000)
#define U2PLL_FS	(0x00466fae)
#define U2PLL_HS	(0x00463c6e)

#define DIV13_TRY_TIMES 3
#define VCORE_OPP 0

static struct phy *mtk_phy;
struct pm_qos_request vcore_pm_qos;

#if !defined(CONFIG_USB_MU3D_DRV)
void Charger_Detect_Init(void)
{
	if (mtu3_cable_mode == CABLE_MODE_FORCEON) {
		pr_info("%s-, SKIP\n", __func__);
		return;
	}

	if (mtk_phy)
		usb_mtkphy_switch_to_bc11(mtk_phy, true);
}

void Charger_Detect_Release(void)
{
	if (mtu3_cable_mode == CABLE_MODE_FORCEON) {
		pr_info("%s-, SKIP\n", __func__);
		return;
	}

	if (mtk_phy)
		usb_mtkphy_switch_to_bc11(mtk_phy, false);
}
#endif

void usb_dpdm_pulldown(bool enable)
{
#ifdef CONFIG_MTK_TYPEC_WATER_DETECT
	if (mtk_phy) {
		pr_info("%s: pulldown=%d\n", __func__, enable);
		usb_mtkphy_dpdm_pulldown(mtk_phy, enable);
	}
#endif
}

void ssusb_switch_usbpll_div13(struct ssusb_mtk *ssusb, bool on)
{
	struct regmap *ap_regmap;
	struct device_node *node = ssusb->dev->of_node;
	int div13_try = DIV13_TRY_TIMES;

	ap_regmap = syscon_regmap_lookup_by_phandle(node, "apmixed");
	if (IS_ERR(ap_regmap)) {
		pr_info("failed to get ap_regmap\n");
		return;
	}

	udelay(20);

	if (on) {
		regmap_update_bits(ap_regmap, RG_USBPLL_192M_OPP_EN,
				RG_USBPLL_DIV13_C(0x1), RG_USBPLL_DIV13_C(0x1));
	} else {
		while (div13_try--) {
			regmap_update_bits(ap_regmap, RG_USBPLL_192M_OPP_EN,
				RG_USBPLL_DIV13_C(0x1), RG_USBPLL_DIV13_C(0x0));
			regmap_update_bits(ap_regmap, RG_USBPLL_192M_OPP_EN,
				RG_USBPLL_DIV13_C(0x1), RG_USBPLL_DIV13_C(0x1));
		}
		regmap_update_bits(ap_regmap, RG_USBPLL_192M_OPP_EN,
				RG_USBPLL_DIV13_C(0x1), RG_USBPLL_DIV13_C(0x0));
	}

	udelay(20);
}

static void ssusb_switch_phy_to_fs(struct ssusb_mtk *ssusb, bool is_fs)
{
	struct phy *phy = ssusb->phys[0];

	clk_disable(ssusb->ref_clk);

	ssusb_switch_usbpll_div13(ssusb, false);

	if (is_fs) {
		clk_set_rate(ssusb->ref_clk, USBPLL_FS);
		u3phywrite32(phy, USBPHYACR0, USBPHYACR0_MASK, U2PLL_FS);
	} else {
		clk_set_rate(ssusb->ref_clk, USBPLL_HS);
		u3phywrite32(phy, USBPHYACR0, USBPHYACR0_MASK, U2PLL_HS);
	}

	clk_enable(ssusb->ref_clk);

	ssusb_switch_usbpll_div13(ssusb, true);
}

int ssusb_dual_phy_power_on(struct ssusb_mtk *ssusb, bool host_mode)
{
	int ret;

	if (host_mode) {
		if (pm_qos_request_active(&vcore_pm_qos)) {
			pm_qos_update_request(&vcore_pm_qos, VCORE_OPP);
			pr_info("%s: Vcore QOS update %d\n", __func__,
								VCORE_OPP);
		} else {
			pm_qos_add_request(&vcore_pm_qos, PM_QOS_VCORE_OPP,
								VCORE_OPP);
			pr_info("%s: Vcore QOS request\n", __func__);
		}
	}

	ret = phy_power_on(ssusb->phys[0]);

	if (host_mode) {
		if (!ret)
			usb_mtkphy_host_mode(ssusb->phys[0], true);
	}
	return ret;
}

void ssusb_dual_phy_power_off(struct ssusb_mtk *ssusb, bool host_mode)
{
	if (host_mode) {
		if (pm_qos_request_active(&vcore_pm_qos)) {
			pm_qos_remove_request(&vcore_pm_qos);
			pr_info("%s: Vcore QOS remove\n",  __func__);
		} else
			pr_info("%s: Vcore QOS remove again\n", __func__);
		usb_mtkphy_host_mode(ssusb->phys[0], false);
	}

	phy_power_off(ssusb->phys[0]);
}

bool ssusb_u3loop_back_test(struct ssusb_mtk *ssusb)
{
	int ret;
	void __iomem *ibase = ssusb->ippc_base;

	ret = clk_prepare_enable(ssusb->sys_clk);
	if (ret) {
		dev_info(ssusb->dev, "failed to enable sys_clk\n");
		return ret;
	}
	mtu3_clrbits(ibase, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
	mtu3_clrbits(ibase, U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);
	mtu3_clrbits(ibase, U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);
	mtu3_clrbits(ibase, U3D_SSUSB_IP_PW_CTRL3, SSUSB_IP_PCIE_PDN);
	mtu3_clrbits(ibase, SSUSB_U3_CTRL(0),
		(SSUSB_U3_PORT_DIS | SSUSB_U3_PORT_PDN));
	mdelay(10);

	if (usb_mtkphy_u3_loop_back_test(ssusb->phys[0]) > 0)
		ret = true;
	else
		ret = false;

	mtu3_setbits(ibase, U3D_SSUSB_IP_PW_CTRL3, SSUSB_IP_PCIE_PDN);

	clk_disable_unprepare(ssusb->sys_clk);

	return ret;
}

void ssusb_set_phy_mode(int speed)
{
	struct ssusb_mtk *ssusb;
	void __iomem *ibase;
	struct phy *phy;
	u32 value;

	ssusb = get_ssusb();
	if (!ssusb || !ssusb->phys[0]) {
		pr_info("ssusb not ready\n");
		return;
	}

	pr_info("%s speed=%d\n", __func__, speed);

	ibase = ssusb->ippc_base;
	phy = ssusb->phys[0];

	if (speed == DEV_SPEED_INACTIVE) {
		ssusb_switch_phy_to_fs(ssusb, false);
		return;
	}

	u3phywrite32(phy, U2PHYDTM1, RG_LINESTATE_C(0x3),
			RG_LINESTATE_C(0x1));
	u3phywrite32(phy, U2PHYDTM1, FORCE_LINESTATE_C(0x1),
				 FORCE_LINESTATE_C(0x1));

	mtu3_setbits(ibase, U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);

	if (readl_poll_timeout_atomic(ibase + U3D_SSUSB_IP_PW_STS1,
			value, (value & SSUSB_IP_SLEEP_STS), 100, 100000))
		pr_info("ip sleep failed\n");

	ssusb_switch_phy_to_fs(ssusb, (speed == DEV_SPEED_FULL));

	u3phywrite32(phy, U2PHYDTM1, FORCE_LINESTATE_C(0x1),
			FORCE_LINESTATE_C(0x0));

	mtu3_clrbits(ibase, U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);
	udelay(200);
}

void phy_hal_init(struct phy *phy)
{
	mtk_phy = phy;
	mtu3_phy_init_debugfs(mtk_phy);
}

void phy_hal_exit(struct phy *phy)
{
	mtu3_phy_exit_debugfs();
	mtk_phy = NULL;
}

