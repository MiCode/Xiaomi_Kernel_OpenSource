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


#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/regmap.h>

#include "mtu3.h"
#include "mtu3_hal.h"
#include "mtu3_dr.h"

#include <linux/phy/mediatek/mtk_usb_phy.h>

/* mt8173 etc */
#define PERI_WK_CTRL1	0x4
#define WC1_IS_C(x)	(((x) & 0xf) << 26)  /* cycle debounce */
#define WC1_IS_EN	BIT(25)
#define WC1_IS_P	BIT(6)  /* polarity for ip sleep */

/* mt8183 */
#define PERI_WK_CTRL0	0x0
#define WC0_IS_C(x)	(((x) & 0xfU) << 28) /* cycle debounce */
#define WC0_IS_P	BIT(12) /* polarity */
#define WC0_IS_EN	BIT(6)

/* mt2712 etc */
#define PERI_SSUSB_SPM_CTRL	0x0
#define SSC_IP_SLEEP_EN	BIT(4)
#define SSC_SPM_INT_EN		BIT(1)

enum ssusb_uwk_vers {
	SSUSB_UWK_V1 = 1,
	SSUSB_UWK_V2,
	SSUSB_UWK_V1_1 = 101, /* specific revision 1.01 */
};

/*
 * ip-sleep wakeup mode:
 * all clocks can be turn off, but power domain should be kept on
 */
static void ssusb_wakeup_ip_sleep_set(struct ssusb_mtk *ssusb, bool enable)
{
	u32 reg, msk, val;

	dev_info(ssusb->dev, "%s %d\n", __func__, enable);

	switch (ssusb->uwk_vers) {
	case SSUSB_UWK_V1:
		reg = ssusb->uwk_reg_base + PERI_WK_CTRL1;
		msk = WC1_IS_EN | WC1_IS_C(0xf) | WC1_IS_P;
		val = enable ? (WC1_IS_EN | WC1_IS_C(0x8)) : 0;
		break;
	case SSUSB_UWK_V2:
		reg = ssusb->uwk_reg_base + PERI_SSUSB_SPM_CTRL;
		msk = SSC_IP_SLEEP_EN | SSC_SPM_INT_EN;
		val = enable ? msk : 0;
		break;
	case SSUSB_UWK_V1_1:
		reg = ssusb->uwk_reg_base + PERI_WK_CTRL0;
		msk = WC0_IS_EN | WC0_IS_C(0xf) | WC0_IS_P;
		val = enable ? (WC0_IS_EN | WC0_IS_C(0x8)) : 0;
		break;
	default:
		return;
	}
	regmap_update_bits(ssusb->uwk, reg, msk, val);
}

int ssusb_wakeup_of_property_parse(struct ssusb_mtk *ssusb,
				struct device_node *dn)
{
	struct of_phandle_args args;
	int ret;

	/* wakeup function is optional */
	ssusb->wakeup_en = of_property_read_bool(dn, "wakeup-source");
	if (!ssusb->wakeup_en)
		return 0;

	ret = of_parse_phandle_with_fixed_args(dn,
				"mediatek,syscon-wakeup", 2, 0, &args);
	if (ret)
		return ret;

	ssusb->uwk_reg_base = args.args[0];
	ssusb->uwk_vers = args.args[1];
	ssusb->uwk = syscon_node_to_regmap(args.np);
	of_node_put(args.np);
	dev_info(ssusb->dev, "uwk - reg:0x%x, version:%d\n",
			ssusb->uwk_reg_base, ssusb->uwk_vers);

	return PTR_ERR_OR_ZERO(ssusb->uwk);
}

void ssusb_wakeup_set(struct ssusb_mtk *ssusb, bool enable)
{
	if (ssusb->wakeup_en)
		ssusb_wakeup_ip_sleep_set(ssusb, enable);
}

static void host_ports_num_get(struct ssusb_mtk *ssusb)
{
	u32 xhci_cap;

	xhci_cap = mtu3_readl(ssusb->ippc_base, U3D_SSUSB_IP_XHCI_CAP);
	ssusb->u2_ports = SSUSB_IP_XHCI_U2_PORT_NUM(xhci_cap);
	ssusb->u3_ports = SSUSB_IP_XHCI_U3_PORT_NUM(xhci_cap);

	dev_dbg(ssusb->dev, "host - u2_ports:%d, u3_ports:%d\n",
		 ssusb->u2_ports, ssusb->u3_ports);
	if (ssusb->u3ports_disable)
		ssusb->u3_ports = 0;
}


/* only configure ports will be used later */
int ssusb_host_enable(struct ssusb_mtk *ssusb)
{
	void __iomem *ibase = ssusb->ippc_base;
	int num_u3p = ssusb->u3_ports;
	int num_u2p = ssusb->u2_ports;
	u32 check_clk;
	u32 value;
	int i;

	/* power on host ip */
	mtu3_clrbits(ibase, U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);

	/* power on and enable all u3 ports */
	for (i = 0; i < num_u3p; i++) {
		value = mtu3_readl(ibase, SSUSB_U3_CTRL(i));
		value &= ~(SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS);
		value |= SSUSB_U3_PORT_HOST_SEL;
		mtu3_writel(ibase, SSUSB_U3_CTRL(i), value);
	}

	/* power on and enable all u2 ports */
	for (i = 0; i < num_u2p; i++) {
		value = mtu3_readl(ibase, SSUSB_U2_CTRL(i));
		value &= ~(SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_DIS);
		value |= SSUSB_U2_PORT_HOST_SEL;
		mtu3_writel(ibase, SSUSB_U2_CTRL(i), value);
	}

	check_clk = SSUSB_XHCI_RST_B_STS;
	if (num_u3p)
		check_clk = SSUSB_U3_MAC_RST_B_STS;

	return ssusb_check_clocks(ssusb, check_clk);
}

int ssusb_host_disable(struct ssusb_mtk *ssusb, bool suspend)
{
	void __iomem *ibase = ssusb->ippc_base;
	int num_u3p = ssusb->u3_ports;
	int num_u2p = ssusb->u2_ports;
	u32 value;
	int ret;
	int i;

	/* power down and disable all u3 ports */
	for (i = 0; i < num_u3p; i++) {
		value = mtu3_readl(ibase, SSUSB_U3_CTRL(i));
		value |= SSUSB_U3_PORT_PDN;
		value |= suspend ? 0 : SSUSB_U3_PORT_DIS;
		mtu3_writel(ibase, SSUSB_U3_CTRL(i), value);
	}

	/* power down and disable all u2 ports */
	for (i = 0; i < num_u2p; i++) {
		value = mtu3_readl(ibase, SSUSB_U2_CTRL(i));
		value |= SSUSB_U2_PORT_PDN;
		value |= suspend ? 0 : SSUSB_U2_PORT_DIS;
		mtu3_writel(ibase, SSUSB_U2_CTRL(i), value);
	}

	/* power down host ip */
	mtu3_setbits(ibase, U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);

	if (!suspend)
		return 0;

	/* wait for host ip to sleep */
	ret = readl_poll_timeout(ibase + U3D_SSUSB_IP_PW_STS1, value,
			  (value & SSUSB_IP_SLEEP_STS), 100, 100000);
	if (ret)
		dev_info(ssusb->dev, "ip sleep failed!!!\n");

	return ret;
}

static void ssusb_host_setup(struct ssusb_mtk *ssusb)
{
	host_ports_num_get(ssusb);
}

static void ssusb_host_cleanup(struct ssusb_mtk *ssusb)
{
}

/*
 * If host supports multiple ports, the VBUSes(5V) of ports except port0
 * which supports OTG are better to be enabled by default in DTS.
 * Because the host driver will keep link with devices attached when system
 * enters suspend mode, so no need to control VBUSes after initialization.
 */
int ssusb_host_init(struct ssusb_mtk *ssusb, struct device_node *parent_dn)
{
	ssusb_host_setup(ssusb);
	return 0;
}

void ssusb_host_exit(struct ssusb_mtk *ssusb)
{
	ssusb_host_cleanup(ssusb);
}

