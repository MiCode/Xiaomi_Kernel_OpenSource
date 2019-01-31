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

#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/pinctrl/consumer.h>
#include <linux/uaccess.h>
#include <linux/usb/otg.h>

#include "mtu3.h"
#include "mtu3_dr.h"
#include "mtu3_hal.h"

#define USB2_PORT 2
#define USB3_PORT 3

struct otg_switch_mtk *g_otg_sx;

enum mtu3_vbus_id_state {
	MTU3_ID_FLOAT = 1,
	MTU3_ID_GROUND,
	MTU3_VBUS_OFF,
	MTU3_VBUS_VALID,
};

enum {
	DUAL_PROP_HOST = 0,
	DUAL_PROP_DEVICE,
	DUAL_PROP_NONE,
};

static void toggle_opstate(struct ssusb_mtk *ssusb)
{
	mtu3_setbits(ssusb->mac_base, U3D_DEVICE_CONTROL, DC_SESSION);
}


bool usb_cable_connected(void)
{
	if (g_otg_sx)
		return (g_otg_sx->usb_mode == DUAL_PROP_DEVICE);
	else
		return false;
}

static void ssusb_ip_sw_reset(struct ssusb_mtk *ssusb)
{
	/* reset whole ip (xhci & u3d) */
	mtu3_setbits(ssusb->ippc_base, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
	udelay(1);
	mtu3_clrbits(ssusb->ippc_base, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
}

/* only port0 supports dual-role mode */
static int ssusb_port0_switch(struct ssusb_mtk *ssusb,
	int version, bool tohost)
{
	void __iomem *ibase = ssusb->ippc_base;
	u32 value;

	dev_dbg(ssusb->dev, "%s (switch u%d port0 to %s)\n", __func__,
		version, tohost ? "host" : "device");

	if (version == USB2_PORT) {
		/* 1. power off and disable u2 port0 */
		value = mtu3_readl(ibase, SSUSB_U2_CTRL(0));
		value |= SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_DIS;
		mtu3_writel(ibase, SSUSB_U2_CTRL(0), value);

		/* 2. power on, enable u2 port0 and select its mode */
		value = mtu3_readl(ibase, SSUSB_U2_CTRL(0));
		value &= ~(SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_DIS);
		value = tohost ? (value | SSUSB_U2_PORT_HOST_SEL) :
			(value & (~SSUSB_U2_PORT_HOST_SEL));
		mtu3_writel(ibase, SSUSB_U2_CTRL(0), value);
	} else {
		/* 1. power off and disable u3 port0 */
		value = mtu3_readl(ibase, SSUSB_U3_CTRL(0));
		value |= SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS;
		mtu3_writel(ibase, SSUSB_U3_CTRL(0), value);

		/* 2. power on, enable u3 port0 and select its mode */
		value = mtu3_readl(ibase, SSUSB_U3_CTRL(0));
		value &= ~(SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS);
		value = tohost ? (value | SSUSB_U3_PORT_HOST_SEL) :
			(value & (~SSUSB_U3_PORT_HOST_SEL));
		mtu3_writel(ibase, SSUSB_U3_CTRL(0), value);
	}

	return 0;
}

static void ssusb_ip_sleep(struct ssusb_mtk *ssusb)
{
	void __iomem *ibase = ssusb->ippc_base;

	/* Set below sequence to avoid power leakage */
	mtu3_setbits(ibase, SSUSB_U3_CTRL(0),
		(SSUSB_U3_PORT_DIS | SSUSB_U3_PORT_PDN));
	mtu3_setbits(ibase, SSUSB_U2_CTRL(0),
		SSUSB_U2_PORT_DIS | SSUSB_U2_PORT_PDN);
	mtu3_clrbits(ibase, SSUSB_U2_CTRL(0), SSUSB_U2_PORT_OTG_SEL);
	mtu3_setbits(ibase, U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);
	mtu3_setbits(ibase, U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);
	udelay(50);
	mtu3_setbits(ibase, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
}


static void switch_port_to_none(struct ssusb_mtk *ssusb)
{
	dev_info(ssusb->dev, "%s\n", __func__);

	if (ssusb->is_host)
		xhci_mtk_unregister_plat();

	ssusb_ip_sleep(ssusb);
	ssusb_dual_phy_power_off(ssusb, ssusb->is_host);
	ssusb_clk_off(ssusb, ssusb->is_host);
	if (ssusb->is_host)
		ssusb->is_host = false;
}

static void switch_port_to_host(struct ssusb_mtk *ssusb)
{
	int retval;

	u32 check_clk = 0;

	dev_info(ssusb->dev, "%s\n", __func__);
	ssusb_dual_phy_power_on(ssusb, true);
	ssusb_clk_on(ssusb, true);
	ssusb_ip_sw_reset(ssusb);
	ssusb_port0_switch(ssusb, USB2_PORT, true);

	if (ssusb->otg_switch.is_u3h_drd) {
		ssusb_port0_switch(ssusb, USB3_PORT, true);
		check_clk = SSUSB_U3_MAC_RST_B_STS;
	}

	ssusb_check_clocks(ssusb, check_clk);
	ssusb_host_enable(ssusb);
	retval = xhci_mtk_register_plat();
	if (retval < 0)
		switch_port_to_none(ssusb);
	else
		ssusb->is_host = true;

	/* after all clocks are stable */
}

static void switch_port_to_device(struct ssusb_mtk *ssusb)
{
	u32 check_clk = 0;
	struct mtu3 *mtu = ssusb->u3d;

	dev_info(ssusb->dev, "%s\n", __func__);
	ssusb->otg_switch.is_u3_drd = mtu3_speed;
	if (ssusb->otg_switch.is_u3_drd)
		mtu->max_speed = USB_SPEED_SUPER;
	else
		mtu->max_speed = USB_SPEED_HIGH;

	ssusb_dual_phy_power_on(ssusb, false);
	ssusb_clk_on(ssusb, false);
	ssusb_ip_sw_reset(ssusb);
	ssusb_port0_switch(ssusb, USB2_PORT, false);

	if (ssusb->otg_switch.is_u3_drd) {
		ssusb_port0_switch(ssusb, USB3_PORT, false);
		check_clk = SSUSB_U3_MAC_RST_B_STS;
	}

	ssusb_check_clocks(ssusb, check_clk);
	toggle_opstate(ssusb);
}


int ssusb_set_vbus(struct otg_switch_mtk *otg_sx, int is_on)
{
	return 0;
}

void ssusb_gadget_disconnect(struct mtu3 *mtu)
{
	/* notify gadget driver */
	if (mtu->g.speed == USB_SPEED_UNKNOWN)
		return;

	if (mtu->gadget_driver && mtu->gadget_driver->disconnect) {
		mtu->gadget_driver->disconnect(&mtu->g);
		mtu->g.speed = USB_SPEED_UNKNOWN;
	}

	usb_gadget_set_state(&mtu->g, USB_STATE_NOTATTACHED);
}

static void ssusb_set_mode(struct work_struct *work)
{
	struct otg_switch_mtk *otg_sx = container_of(to_delayed_work(work),
				struct otg_switch_mtk, dr_work);
	struct ssusb_mtk *ssusb =
		container_of(otg_sx, struct ssusb_mtk, otg_switch);
	struct mtu3 *mtu = ssusb->u3d;
	unsigned long flags;
	unsigned int usb_mode;

	spin_lock_irqsave(&otg_sx->dr_lock, flags);
	usb_mode = otg_sx->desire_usb_mode;
	spin_unlock_irqrestore(&otg_sx->dr_lock, flags);

	if (otg_sx->usb_mode != usb_mode) {
		otg_sx->usb_mode = usb_mode;

		mtu3_printk(K_CRIT, "ssusb_set_mode = %d\n", usb_mode);
		switch (usb_mode) {
		case DUAL_PROP_HOST:
			switch_port_to_host(ssusb);
			break;
		case DUAL_PROP_DEVICE:
			/* avoid suspend when works as device */
			switch_port_to_device(ssusb);
			mtu3_start(mtu);
			break;
		case DUAL_PROP_NONE:
			if (!ssusb->is_host) {
				mtu3_stop(mtu);
				/* notify gadget driver */
				ssusb_gadget_disconnect(mtu);
			}
			switch_port_to_none(ssusb);
			break;
		default:
			dev_err(ssusb->dev, "invalid state\n");
		}
	}
}


/*
 * switch to host: -> MTU3_VBUS_OFF --> MTU3_ID_GROUND
 * switch to device: -> MTU3_ID_FLOAT --> MTU3_VBUS_VALID
 */
static void ssusb_set_mailbox(struct otg_switch_mtk *otg_sx,
	enum mtu3_vbus_id_state status)
{
	struct ssusb_mtk *ssusb =
		container_of(otg_sx, struct ssusb_mtk, otg_switch);
	unsigned long flags;

	mtu3_printk(K_CRIT, "mailbox state(%d)\n", status);
	spin_lock_irqsave(&otg_sx->dr_lock, flags);
	switch (status) {
	case MTU3_ID_GROUND:
		otg_sx->desire_usb_mode = DUAL_PROP_HOST;
		break;
	case MTU3_VBUS_VALID:
		otg_sx->desire_usb_mode = DUAL_PROP_DEVICE;
		break;
	case MTU3_ID_FLOAT:
	case MTU3_VBUS_OFF:
		otg_sx->desire_usb_mode = DUAL_PROP_NONE;
		break;
	default:
		dev_err(ssusb->dev, "invalid state\n");
	}
	spin_unlock_irqrestore(&otg_sx->dr_lock, flags);
	queue_delayed_work(otg_sx->dr_workq,
			&otg_sx->dr_work, 0);
}

static int ssusb_id_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct otg_switch_mtk *otg_sx =
		container_of(nb, struct otg_switch_mtk, id_nb);

	if (event)
		ssusb_set_mailbox(otg_sx, MTU3_ID_GROUND);
	else
		ssusb_set_mailbox(otg_sx, MTU3_ID_FLOAT);

	return NOTIFY_DONE;
}

static int ssusb_vbus_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct otg_switch_mtk *otg_sx =
		container_of(nb, struct otg_switch_mtk, vbus_nb);

	if (event)
		ssusb_set_mailbox(otg_sx, MTU3_VBUS_VALID);
	else
		ssusb_set_mailbox(otg_sx, MTU3_VBUS_OFF);

	return NOTIFY_DONE;
}

static int ssusb_extcon_register(struct otg_switch_mtk *otg_sx)
{
	struct ssusb_mtk *ssusb =
		container_of(otg_sx, struct ssusb_mtk, otg_switch);
	struct extcon_dev *edev = otg_sx->edev;
	int ret;

	/* extcon is optional */
	if (!edev)
		return 0;

	otg_sx->vbus_nb.notifier_call = ssusb_vbus_notifier;
	ret = extcon_register_notifier(edev, EXTCON_USB,
					&otg_sx->vbus_nb);
	if (ret < 0)
		dev_err(ssusb->dev, "failed to register notifier for USB\n");

	otg_sx->id_nb.notifier_call = ssusb_id_notifier;
	ret = extcon_register_notifier(edev, EXTCON_USB_HOST,
					&otg_sx->id_nb);
	if (ret < 0)
		dev_err(ssusb->dev, "failed to register notifier for USB-HOST\n");

	dev_dbg(ssusb->dev, "EXTCON_USB: %d, EXTCON_USB_HOST: %d\n",
		extcon_get_state(edev, EXTCON_USB),
		extcon_get_state(edev, EXTCON_USB_HOST));

	/* switch to device mode if needed */
	if (extcon_get_state(edev, EXTCON_USB) == true)
		ssusb_set_mailbox(otg_sx, MTU3_VBUS_VALID);
	else if (extcon_get_state(edev, EXTCON_USB_HOST) == true)
		ssusb_set_mailbox(otg_sx, MTU3_ID_GROUND);
	return 0;

}

static void extcon_register_dwork(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct otg_switch_mtk *otg_sx = container_of(dwork,
		struct otg_switch_mtk, extcon_reg_dwork);

	otg_sx->dr_workq = create_singlethread_workqueue("usb_dr_workq");
	INIT_DELAYED_WORK(&otg_sx->dr_work, ssusb_set_mode);
	ssusb_extcon_register(otg_sx);
}

int ssusb_otg_switch_init(struct ssusb_mtk *ssusb)
{
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;

	otg_sx->usb_mode = DUAL_PROP_NONE;
	switch_port_to_none(ssusb);

	spin_lock_init(&otg_sx->dr_lock);

	INIT_DELAYED_WORK(&otg_sx->extcon_reg_dwork, extcon_register_dwork);

	ssusb_debugfs_init(ssusb);

	/* It is enough to delay 1s for waiting for host initialization */
	schedule_delayed_work(&otg_sx->extcon_reg_dwork, HZ);
	g_otg_sx = otg_sx;

	return 0;
}

void ssusb_otg_switch_exit(struct ssusb_mtk *ssusb)
{
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;

	cancel_delayed_work(&otg_sx->extcon_reg_dwork);

	if (otg_sx->edev) {
		extcon_unregister_notifier(otg_sx->edev,
			EXTCON_USB, &otg_sx->vbus_nb);
		extcon_unregister_notifier(otg_sx->edev,
			EXTCON_USB_HOST, &otg_sx->id_nb);
	}

	ssusb_debugfs_exit(ssusb);
	g_otg_sx = NULL;
}

