/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/phy/phy.h>
#include <linux/phy/mediatek/mtk_usb_phy.h>
#include <linux/clk.h>
#include <linux/pm_qos.h>

#include "mtu3.h"
#include "mtu3_priv.h"

#define VCORE_OPP 1

static struct phy *mtk_phy;
static struct pm_qos_request vcore_pm_qos;

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

	if (host_mode) {
		if (pm_qos_request_active(&vcore_pm_qos)) {
			pm_qos_update_request(&vcore_pm_qos, VCORE_OPP);
			dev_info(ssusb->dev, "%s: Vcore QOS update %d\n", __func__,
								VCORE_OPP);
		} else {
			pm_qos_add_request(&vcore_pm_qos, PM_QOS_VCORE_OPP,
								VCORE_OPP);
			dev_info(ssusb->dev, "%s: Vcore QOS request %d\n", __func__,
								VCORE_OPP);
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
			dev_info(ssusb->dev, "%s: Vcore QOS remove\n",  __func__);
		} else
			dev_info(ssusb->dev, "%s: Vcore QOS remove again\n", __func__);

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
	/* do nothing */
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

