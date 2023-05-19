// SPDX-License-Identifier: GPL-2.0
/*
 * mtu3_dr.c - dual role switch and host glue layer
 *
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 */

#include <linux/usb/role.h>
#include <linux/of_platform.h>
#include <linux/iopoll.h>

#include "mtu3.h"
#include "mtu3_dr.h"
#include "mtu3_debug.h"

#define USB2_PORT 2
#define USB3_PORT 3

enum mtu3_vbus_id_state {
	MTU3_ID_FLOAT = 1,
	MTU3_ID_GROUND,
	MTU3_VBUS_OFF,
	MTU3_VBUS_VALID,
};

static char *mailbox_state_string(enum mtu3_vbus_id_state state)
{
	switch (state) {
	case MTU3_ID_FLOAT:
		return "ID_FLOAT";
	case MTU3_ID_GROUND:
		return "ID_GROUND";
	case MTU3_VBUS_OFF:
		return "VBUS_OFF";
	case MTU3_VBUS_VALID:
		return "VBUS_VALID";
	default:
		return "UNKNOWN";
	}
}

static void toggle_opstate(struct ssusb_mtk *ssusb)
{
	mtu3_setbits(ssusb->mac_base, U3D_DEVICE_CONTROL, DC_SESSION);
	mtu3_setbits(ssusb->mac_base, U3D_POWER_MANAGEMENT, SOFT_CONN);
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
	int num_u3p = ssusb->u3_ports;
	int num_u2p = ssusb->u2_ports;
	u32 value;
	int ret;
	int i;

	/* power down and disable all u3 ports */
	for (i = 0; i < num_u3p; i++) {
		value = mtu3_readl(ibase, SSUSB_U3_CTRL(i));
		value |= SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS;
		mtu3_writel(ibase, SSUSB_U3_CTRL(i), value);

		mtu3_clrbits(ibase, SSUSB_U3_CTRL(i), SSUSB_U3_PORT_DUAL_MODE);
	}

	/* power down and disable all u2 ports */
	for (i = 0; i < num_u2p; i++) {
		value = mtu3_readl(ibase, SSUSB_U2_CTRL(i));
		value |= SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_DIS;
		mtu3_writel(ibase, SSUSB_U2_CTRL(i), value);

		mtu3_clrbits(ibase, SSUSB_U2_CTRL(i), SSUSB_U2_PORT_OTG_SEL);
	}

	/* power down device ip */
	mtu3_setbits(ibase, U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);
	/* power down host ip */
	mtu3_setbits(ibase, U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);

	/* wait for ip to sleep */
	ret = readl_poll_timeout(ibase + U3D_SSUSB_IP_PW_STS1, value,
			  (value & SSUSB_IP_SLEEP_STS), 100, 100000);
	if (ret)
		dev_info(ssusb->dev, "ip sleep failed!!!\n");
}

static void switch_port_to_on(struct ssusb_mtk *ssusb,
	bool is_on, enum phy_mode mode)
{
	if (!ssusb->clk_mgr)
		return;

	dev_info(ssusb->dev, "%s (%s) (%d)\n",
		__func__, is_on ? "on" : "off", mode);

	if (is_on) {
		ssusb_clks_enable(ssusb);
		ssusb_phy_power_on(ssusb);
		ssusb_phy_set_mode(ssusb, mode);
		ssusb_ip_sw_reset(ssusb);
	} else {
		ssusb_ip_sleep(ssusb);
		ssusb_phy_power_off(ssusb);
		ssusb_clks_disable(ssusb);
	}
}

static void switch_port_to_host(struct ssusb_mtk *ssusb)
{
	u32 check_clk = 0;

	dev_dbg(ssusb->dev, "%s\n", __func__);

	ssusb_port0_switch(ssusb, USB2_PORT, true);

	if (ssusb->otg_switch.is_u3_drd) {
		ssusb_port0_switch(ssusb, USB3_PORT, true);
		check_clk = SSUSB_U3_MAC_RST_B_STS;
	}

	ssusb_check_clocks(ssusb, check_clk);

	/* after all clocks are stable */
	toggle_opstate(ssusb);
}

static void switch_port_to_device(struct ssusb_mtk *ssusb)
{
	u32 check_clk = 0;

	dev_dbg(ssusb->dev, "%s\n", __func__);

	ssusb_port0_switch(ssusb, USB2_PORT, false);

	if (ssusb->otg_switch.is_u3_drd) {
		ssusb_port0_switch(ssusb, USB3_PORT, false);
		check_clk = SSUSB_U3_MAC_RST_B_STS;
	}

	ssusb_check_clocks(ssusb, check_clk);
}

static void ssusb_host_register(struct ssusb_mtk *ssusb, bool on)
{
	int ret;

	dev_info(ssusb->dev, "%s %d\n", __func__, on);

	if (!ssusb->xhci_pdrv)
		return;

	if (on) {
		ret = platform_driver_register(ssusb->xhci_pdrv);
		if (ret)
			dev_info(ssusb->dev, "register host driver fail\n");
	} else {
		platform_driver_unregister(ssusb->xhci_pdrv);
	}
}

int ssusb_set_vbus(struct otg_switch_mtk *otg_sx, int is_on)
{
	struct ssusb_mtk *ssusb =
		container_of(otg_sx, struct ssusb_mtk, otg_switch);
	struct regulator *vbus = otg_sx->vbus;
	int ret;

	/* vbus is optional */
	if (!vbus)
		return 0;

	dev_dbg(ssusb->dev, "%s: turn %s\n", __func__, is_on ? "on" : "off");

	if (is_on) {
		ret = regulator_enable(vbus);
		if (ret) {
			dev_err(ssusb->dev, "vbus regulator enable failed\n");
			return ret;
		}
	} else {
		regulator_disable(vbus);
	}

	return 0;
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
	struct mtu3 *mtu = ssusb->u3d;
	unsigned long flags;

	dev_dbg(ssusb->dev, "mailbox %s\n", mailbox_state_string(status));
	mtu3_dbg_trace(ssusb->dev, "mailbox %s", mailbox_state_string(status));

	switch (status) {
	case MTU3_ID_GROUND:
		if (ssusb->clk_mgr) {
			switch_port_to_on(ssusb, true, PHY_MODE_USB_HOST);
			ssusb_host_enable(ssusb);
			ssusb_set_power_state(ssusb, MTU3_STATE_POWER_ON);
		} else
			switch_port_to_host(ssusb);
		ssusb_set_force_mode(ssusb, MTU3_DR_FORCE_HOST);
		ssusb_host_register(ssusb, true);
		ssusb_set_noise_still_tr(ssusb);
		ssusb->is_host = true;
		otg_sx->sw_state |= MTU3_SW_ID_GROUND;
		break;
	case MTU3_ID_FLOAT:
		ssusb->is_host = false;
		ssusb_set_vbus(otg_sx, 0);
		ssusb_host_register(ssusb, false);
		ssusb_set_force_mode(ssusb, MTU3_DR_FORCE_NONE);
		if (ssusb->clk_mgr) {
			ssusb_set_power_state(ssusb, MTU3_STATE_POWER_OFF);
			ssusb_host_disable(ssusb, true);
			switch_port_to_on(ssusb, false, PHY_MODE_USB_OTG);
		} else
			switch_port_to_device(ssusb);
		otg_sx->sw_state &= ~MTU3_SW_ID_GROUND;
		break;
	case MTU3_VBUS_OFF:
		spin_lock_irqsave(&mtu->lock, flags);
		mtu3_nuke_all_ep(mtu);
		mtu3_stop(mtu);
		/* report disconnect */
		if (mtu->g.speed != USB_SPEED_UNKNOWN)
			mtu3_gadget_disconnect(mtu);
		spin_unlock_irqrestore(&mtu->lock, flags);
		pm_relax(ssusb->dev);
		ssusb_set_force_vbus(ssusb, false);
		if (ssusb->clk_mgr) {
			ssusb_set_power_state(ssusb, MTU3_STATE_POWER_OFF);
			mtu3_device_disable(mtu);
			switch_port_to_on(ssusb, false, PHY_MODE_USB_OTG);
		}
		otg_sx->sw_state &= ~MTU3_SW_VBUS_VALID;
		break;
	case MTU3_VBUS_VALID:
		if (ssusb->clk_mgr) {
			switch_port_to_on(ssusb, true, PHY_MODE_USB_DEVICE);
			mtu3_device_enable(mtu);
			ssusb_set_power_state(ssusb, MTU3_STATE_POWER_ON);
		} else
			switch_port_to_device(ssusb);
		ssusb_set_force_vbus(ssusb, true);
		/* avoid suspend when works as device */
		pm_stay_awake(ssusb->dev);
		mtu3_start(mtu);
		otg_sx->sw_state |= MTU3_SW_VBUS_VALID;
		break;
	default:
		dev_err(ssusb->dev, "invalid state\n");
	}
}

static void ssusb_id_work(struct work_struct *work)
{
	struct otg_switch_mtk *otg_sx =
		container_of(work, struct otg_switch_mtk, id_work);

	if (otg_sx->id_event)
		ssusb_set_mailbox(otg_sx, MTU3_ID_GROUND);
	else
		ssusb_set_mailbox(otg_sx, MTU3_ID_FLOAT);
}

static void ssusb_vbus_work(struct work_struct *work)
{
	struct otg_switch_mtk *otg_sx =
		container_of(work, struct otg_switch_mtk, vbus_work);

	if (otg_sx->vbus_event)
		ssusb_set_mailbox(otg_sx, MTU3_VBUS_VALID);
	else
		ssusb_set_mailbox(otg_sx, MTU3_VBUS_OFF);
}

/*
 * @ssusb_id_notifier is called in atomic context, but @ssusb_set_mailbox
 * may sleep, so use work queue here
 */
static int ssusb_id_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct otg_switch_mtk *otg_sx =
		container_of(nb, struct otg_switch_mtk, id_nb);

	otg_sx->id_event = event;
	schedule_work(&otg_sx->id_work);

	return NOTIFY_DONE;
}

static int ssusb_vbus_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct otg_switch_mtk *otg_sx =
		container_of(nb, struct otg_switch_mtk, vbus_nb);

	otg_sx->vbus_event = event;
	schedule_work(&otg_sx->vbus_work);

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
	ret = devm_extcon_register_notifier(ssusb->dev, edev, EXTCON_USB,
					&otg_sx->vbus_nb);
	if (ret < 0) {
		dev_err(ssusb->dev, "failed to register notifier for USB\n");
		return ret;
	}

	otg_sx->id_nb.notifier_call = ssusb_id_notifier;
	ret = devm_extcon_register_notifier(ssusb->dev, edev, EXTCON_USB_HOST,
					&otg_sx->id_nb);
	if (ret < 0) {
		dev_err(ssusb->dev, "failed to register notifier for USB-HOST\n");
		return ret;
	}

	dev_dbg(ssusb->dev, "EXTCON_USB: %d, EXTCON_USB_HOST: %d\n",
		extcon_get_state(edev, EXTCON_USB),
		extcon_get_state(edev, EXTCON_USB_HOST));

	/* default as host, switch to device mode if needed */
	if (extcon_get_state(edev, EXTCON_USB_HOST) == false)
		ssusb_set_mailbox(otg_sx, MTU3_ID_FLOAT);
	if (extcon_get_state(edev, EXTCON_USB) == true)
		ssusb_set_mailbox(otg_sx, MTU3_VBUS_VALID);

	return 0;
}

/*
 * We provide an interface via debugfs to switch between host and device modes
 * depending on user input.
 * This is useful in special cases, such as uses TYPE-A receptacle but also
 * wants to support dual-role mode.
 */
void ssusb_mode_switch(struct ssusb_mtk *ssusb, int to_host)
{
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;

	if (to_host) {
		ssusb_set_force_mode(ssusb, MTU3_DR_FORCE_HOST);
		ssusb_set_mailbox(otg_sx, MTU3_VBUS_OFF);
		ssusb_set_mailbox(otg_sx, MTU3_ID_GROUND);
	} else {
		ssusb_set_force_mode(ssusb, MTU3_DR_FORCE_DEVICE);
		ssusb_set_mailbox(otg_sx, MTU3_ID_FLOAT);
		ssusb_set_mailbox(otg_sx, MTU3_VBUS_VALID);
	}
}

void ssusb_set_force_mode(struct ssusb_mtk *ssusb,
			  enum mtu3_dr_force_mode mode)
{
	u32 value;

	value = mtu3_readl(ssusb->ippc_base, SSUSB_U2_CTRL(0));
	switch (mode) {
	case MTU3_DR_FORCE_DEVICE:
		value |= SSUSB_U2_PORT_FORCE_IDDIG | SSUSB_U2_PORT_RG_IDDIG;
		break;
	case MTU3_DR_FORCE_HOST:
		value |= SSUSB_U2_PORT_FORCE_IDDIG;
		value &= ~SSUSB_U2_PORT_RG_IDDIG;
		break;
	case MTU3_DR_FORCE_NONE:
		value &= ~(SSUSB_U2_PORT_FORCE_IDDIG | SSUSB_U2_PORT_RG_IDDIG);
		break;
	default:
		return;
	}
	mtu3_writel(ssusb->ippc_base, SSUSB_U2_CTRL(0), value);
}

static int ssusb_role_sw_set(struct usb_role_switch *sw, enum usb_role role)
{
	struct ssusb_mtk *ssusb = usb_role_switch_get_drvdata(sw);
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;
	u32 sw_state = otg_sx->sw_state;
	bool vbus_event, id_event;

	dev_info(ssusb->dev, "role_sw_set role %d\n", role);

	otg_sx->latest_role = role;

	if (otg_sx->op_mode != MTU3_DR_OPERATION_DUAL) {
		dev_info(ssusb->dev, "op_mode %d, skip set role\n", otg_sx->op_mode);
		return 0;
	}

	vbus_event = (role == USB_ROLE_DEVICE);
	id_event = (role == USB_ROLE_HOST);

	/* device role to host role */
	if (!!(sw_state & MTU3_SW_VBUS_VALID) && id_event) {
		ssusb_set_mailbox(otg_sx, MTU3_VBUS_OFF);
		ssusb_set_mailbox(otg_sx, MTU3_ID_GROUND);
		return 0;
	}

	/* host role to device role */
	if (!!(sw_state & MTU3_SW_ID_GROUND) && vbus_event) {
		ssusb_set_mailbox(otg_sx, MTU3_ID_FLOAT);
		ssusb_set_mailbox(otg_sx, MTU3_VBUS_VALID);
		return 0;
	}

	/* device role to none, none to device role */
	if (!!(sw_state & MTU3_SW_VBUS_VALID) ^ vbus_event) {
		if (vbus_event)
			ssusb_set_mailbox(otg_sx, MTU3_VBUS_VALID);
		else
			ssusb_set_mailbox(otg_sx, MTU3_VBUS_OFF);
	}

	/* host role to none, none to host role */
	if (!!(sw_state & MTU3_SW_ID_GROUND) ^ id_event) {
		if (id_event)
			ssusb_set_mailbox(otg_sx, MTU3_ID_GROUND);
		else {
			/* wait for device remove done, e.g. usb audio */
			mdelay(100);
			ssusb_set_mailbox(otg_sx, MTU3_ID_FLOAT);
		}
	}

	return 0;
}

static enum usb_role ssusb_role_sw_get(struct usb_role_switch *sw)
{
	struct ssusb_mtk *ssusb = usb_role_switch_get_drvdata(sw);
	enum usb_role role;

	role = ssusb->is_host ? USB_ROLE_HOST : USB_ROLE_DEVICE;

	return role;
}

static int ssusb_role_sw_register(struct otg_switch_mtk *otg_sx)
{
	struct usb_role_switch_desc role_sx_desc = { 0 };
	struct ssusb_mtk *ssusb =
		container_of(otg_sx, struct ssusb_mtk, otg_switch);
	int ret;

	if (!otg_sx->role_sw_used)
		return 0;

	role_sx_desc.set = ssusb_role_sw_set;
	role_sx_desc.get = ssusb_role_sw_get;
	role_sx_desc.fwnode = dev_fwnode(ssusb->dev);
	role_sx_desc.driver_data = ssusb;
	otg_sx->role_sw = usb_role_switch_register(ssusb->dev, &role_sx_desc);

	ret = PTR_ERR_OR_ZERO(otg_sx->role_sw);
	if (ret)
		return ret;

	if (ssusb->clk_mgr) {
		if (ssusb->is_host)
			ssusb_set_mailbox(otg_sx, MTU3_ID_FLOAT);
		else
			ssusb_set_mailbox(otg_sx, MTU3_VBUS_OFF);
	}

	return 0;
}

static ssize_t mode_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;
	enum usb_role role = otg_sx->latest_role;
	int mode;

	if (kstrtoint(buf, 10, &mode))
		return -EINVAL;

	dev_info(dev, "store mode %d op_mode %d\n", mode, otg_sx->op_mode);

	if (otg_sx->op_mode != mode) {
		/* set switch role */
		switch (mode) {
		case MTU3_DR_OPERATION_OFF:
			otg_sx->latest_role = USB_ROLE_NONE;
			break;
		case MTU3_DR_OPERATION_DUAL:
			/* switch usb role to latest role */
			break;
		case MTU3_DR_OPERATION_HOST:
			otg_sx->latest_role = USB_ROLE_HOST;
			break;
		case MTU3_DR_OPERATION_DEVICE:
			otg_sx->latest_role = USB_ROLE_DEVICE;
			break;
		default:
			return -EINVAL;
		}
		/* switch operation mode to normal temporarily */
		otg_sx->op_mode = MTU3_DR_OPERATION_DUAL;
		/* switch usb role */
		ssusb_role_sw_set(otg_sx->role_sw, otg_sx->latest_role);
		/* update operation mode */
		otg_sx->op_mode = mode;
		/* restore role */
		otg_sx->latest_role = role;
	}

	return count;
}

static ssize_t mode_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;

	return sprintf(buf, "%d\n", otg_sx->op_mode);
}
static DEVICE_ATTR_RW(mode);

static ssize_t max_speed_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	struct mtu3 *mtu = ssusb->u3d;
	int speed;

	if (!strncmp(buf, "super-speed-plus", 16))
		speed = USB_SPEED_SUPER_PLUS;
	else if (!strncmp(buf, "super-speed", 11))
		speed = USB_SPEED_SUPER;
	else if (!strncmp(buf, "high-speed", 10))
		speed = USB_SPEED_HIGH;
	else if (!strncmp(buf, "full-speed", 10))
		speed = USB_SPEED_FULL;
	else
		return -EFAULT;

	dev_info(dev, "store speed %s\n", buf);

	mtu->max_speed = speed;
	mtu->g.max_speed = speed;

	return count;
}

static ssize_t max_speed_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	struct mtu3 *mtu = ssusb->u3d;

	return sprintf(buf, "%s\n", usb_speed_string(mtu->max_speed));
}
static DEVICE_ATTR_RW(max_speed);

static ssize_t saving_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	struct mtu3 *mtu = ssusb->u3d;
	int mode;

	if (kstrtoint(buf, 10, &mode))
		return -EINVAL;

	if (mode < MTU3_EP_SLOT_DEFAULT || mode > MTU3_EP_SLOT_MAX)
		return -EINVAL;

	mtu->ep_slot_mode = mode;

	dev_info(dev, "slot mode %d\n", mtu->ep_slot_mode);

	return count;
}

static ssize_t saving_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	struct mtu3 *mtu = ssusb->u3d;

	return sprintf(buf, "%d\n", mtu->ep_slot_mode);
}
static DEVICE_ATTR_RW(saving);

static struct attribute *ssusb_dr_attrs[] = {
	&dev_attr_mode.attr,
	&dev_attr_max_speed.attr,
	&dev_attr_saving.attr,
	NULL
};

static const struct attribute_group ssusb_dr_group = {
	.attrs = ssusb_dr_attrs,
};

int ssusb_otg_switch_init(struct ssusb_mtk *ssusb)
{
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;
	int ret = 0;

	INIT_WORK(&otg_sx->id_work, ssusb_id_work);
	INIT_WORK(&otg_sx->vbus_work, ssusb_vbus_work);

	/* default as host, update state */
	otg_sx->sw_state = ssusb->is_host ?
			MTU3_SW_ID_GROUND : MTU3_SW_VBUS_VALID;

	/* initial operation mode */
	otg_sx->op_mode = MTU3_DR_OPERATION_DUAL;

	ret = sysfs_create_group(&ssusb->dev->kobj, &ssusb_dr_group);
	if (ret)
		dev_info(ssusb->dev, "error creating sysfs attributes\n");

	if (otg_sx->manual_drd_enabled)
		ssusb_dr_debugfs_init(ssusb);
	else if (otg_sx->role_sw_used)
		ret = ssusb_role_sw_register(otg_sx);
	else
		ret = ssusb_extcon_register(otg_sx);

	return ret;
}

void ssusb_otg_switch_exit(struct ssusb_mtk *ssusb)
{
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;

	cancel_work_sync(&otg_sx->id_work);
	cancel_work_sync(&otg_sx->vbus_work);
	usb_role_switch_unregister(otg_sx->role_sw);
	sysfs_remove_group(&ssusb->dev->kobj, &ssusb_dr_group);
}
