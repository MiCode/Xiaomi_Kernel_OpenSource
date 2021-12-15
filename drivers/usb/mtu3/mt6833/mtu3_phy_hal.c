/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/phy/phy.h>
#include <linux/phy/mediatek/mtk_usb_phy.h>
#include <linux/clk.h>

#include "mtu3.h"
#include "mtu3_priv.h"

static struct phy *mtk_phy;

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

int ssusb_dual_phy_power_on(struct ssusb_mtk *ssusb, bool host_mode)
{
	int ret;

	ret = phy_power_on(ssusb->phys[0]);

	if (host_mode) {
		if (!ret)
			usb_mtkphy_host_mode(ssusb->phys[0], true);
	}
	return ret;
}

void ssusb_dual_phy_power_off(struct ssusb_mtk *ssusb, bool host_mode)
{
	if (host_mode)
		usb_mtkphy_host_mode(ssusb->phys[0], false);

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

