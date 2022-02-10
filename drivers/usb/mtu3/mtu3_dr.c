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

#if defined(CONFIG_MACH_MT6779)
#include <linux/soc/mediatek/mtk-pm-qos.h>
#endif

#define USB2_PORT 2
#define USB3_PORT 3

#if defined(CONFIG_MACH_MT6779)
#define VCORE_OPP 1 //0:0.825V 1:0.725V 2:0.65V
struct mtk_pm_qos_request vcore_pm_qos;
#endif

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
	if (!ssusb->otg_switch.is_u3_drd) {
		mtu3_setbits(ssusb->mac_base, U3D_DEVICE_CONTROL, DC_SESSION);
		mtu3_setbits(ssusb->mac_base, U3D_POWER_MANAGEMENT, SOFT_CONN);
	}
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

static void ssusb_gadget_disconnect(struct mtu3 *mtu)
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

	dev_info(ssusb->dev, "mailbox %s\n", mailbox_state_string(status));
	mtu3_dbg_trace(ssusb->dev, "mailbox %s", mailbox_state_string(status));

	switch (status) {
	case MTU3_ID_GROUND:
		switch_port_to_host(ssusb);
		ssusb_set_vbus(otg_sx, 1);
		ssusb_set_noise_still_tr(ssusb);
		ssusb->is_host = true;
		otg_sx->sw_state |= MTU3_SW_ID_GROUND;
		break;
	case MTU3_ID_FLOAT:
		ssusb->is_host = false;
		ssusb_set_vbus(otg_sx, 0);
		switch_port_to_device(ssusb);
		otg_sx->sw_state &= ~MTU3_SW_ID_GROUND;
		break;
	case MTU3_VBUS_OFF:
		/* killing any outstanding requests */
		spin_lock_irqsave(&mtu->lock, flags);
		mtu3_nuke_all_ep(mtu);
		spin_unlock_irqrestore(&mtu->lock, flags);
		mtu3_stop(mtu);
		pm_relax(ssusb->dev);
		ssusb_set_force_vbus(ssusb, false);
		ssusb_gadget_disconnect(mtu);
		otg_sx->sw_state &= ~MTU3_SW_VBUS_VALID;
		break;
	case MTU3_VBUS_VALID:
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

static void ssusb_phy_set_mode(struct ssusb_mtk *ssusb, enum phy_mode mode)
{
	int i;
	int ret;

	for (i = 0; i < ssusb->num_phys; i++) {
		ret = phy_set_mode_ext(ssusb->phys[i], mode, 0);
		if (ret)
			dev_info(ssusb->dev, "in %s, phy_set_mode_ext fail\n",
				__func__);
	}
}

#if defined(CONFIG_MACH_MT6779)
static void ssusb_set_vcore(bool enable)
{
	if (enable) {
		mtk_pm_qos_update_request(&vcore_pm_qos, VCORE_OPP);
		pr_info("%s: Vcore Qos update %d\n", __func__,
				VCORE_OPP);
	} else {
		mtk_pm_qos_update_request(&vcore_pm_qos,
				MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE);
		pr_info("%s: Vcore QOS update default\n", __func__);
	}
}
#endif

static int ssusb_role_sw_set(struct device *dev, enum usb_role role)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;
	struct mtu3 *mtu = ssusb->u3d;
	bool id_event, vbus_event;
	static bool first_init = true;

	dev_info(dev, "role_sw_set role %d\n", role);

	otg_sx->latest_role = role;

	if (otg_sx->op_mode != MTU3_DR_OPERATION_NORMAL) {
		dev_info(dev, "op_mode %d, skip set role\n", otg_sx->op_mode);
		return 0;
	}

	id_event = (role == USB_ROLE_HOST);
	vbus_event = (role == USB_ROLE_DEVICE);

	if (!!(otg_sx->sw_state & MTU3_SW_VBUS_VALID) ^ vbus_event) {
		if (vbus_event) {
			if (ssusb->spm_mgr)
				ssusb_set_power_resource(ssusb,
					MTU3_RESOURCE_ALL);
			if (ssusb->clk_mgr) {
				ssusb_clks_enable(ssusb);
				ssusb_phy_set_mode(ssusb, PHY_MODE_USB_DEVICE);
				ssusb_phy_power_on(ssusb);
				ssusb_ip_sw_reset(ssusb);
				 mtu3_device_enable(mtu);
			}
			ssusb_set_mailbox(otg_sx, MTU3_VBUS_VALID);
		} else {
			ssusb_set_mailbox(otg_sx, MTU3_VBUS_OFF);
			if (ssusb->clk_mgr) {
				mtu3_device_disable(mtu);
				ssusb_ip_sleep(ssusb);
				ssusb_phy_power_off(ssusb);
				ssusb_clks_disable(ssusb);
			}
			if (ssusb->spm_mgr)
				ssusb_set_power_resource(ssusb,
					MTU3_RESOURCE_NONE);
		}
	}

	if (!!(otg_sx->sw_state & MTU3_SW_ID_GROUND) ^ id_event) {
		if (id_event) {
			if (ssusb->spm_mgr)
				ssusb_set_power_resource(ssusb,
					MTU3_RESOURCE_ALL);
			if (ssusb->clk_mgr) {
				#if defined(CONFIG_MACH_MT6779)
				ssusb_set_vcore(true);
				#endif
				ssusb_clks_enable(ssusb);
				ssusb_phy_set_mode(ssusb, PHY_MODE_USB_HOST);
				ssusb_phy_power_on(ssusb);
				ssusb_ip_sw_reset(ssusb);
				ssusb_host_enable(ssusb);
				/* register host driver */
				of_platform_populate(dev->of_node,
						NULL, NULL, dev);
			}
			ssusb_set_force_mode(ssusb, MTU3_DR_FORCE_HOST);
			ssusb_set_mailbox(otg_sx, MTU3_ID_GROUND);
		} else {
			/*
			 * add this for reduce boot 200ms
			 * and add delay 200ms for plugout
			 */
			if (!first_init)
				mdelay(200);
			else
				first_init = false;

			ssusb_set_force_mode(ssusb, MTU3_DR_FORCE_DEVICE);
			if (ssusb->clk_mgr) {
				/* unregister host driver */
				of_platform_depopulate(dev);
				ssusb_set_mailbox(otg_sx, MTU3_ID_FLOAT);
				ssusb_ip_sleep(ssusb);
				ssusb_phy_power_off(ssusb);
				ssusb_clks_disable(ssusb);
				#if defined(CONFIG_MACH_MT6779)
				ssusb_set_vcore(false);
				#endif
			} else
				ssusb_set_mailbox(otg_sx, MTU3_ID_FLOAT);
			if (ssusb->spm_mgr)
				ssusb_set_power_resource(ssusb,
					MTU3_RESOURCE_NONE);
		}
	}

	return 0;
}

static enum usb_role ssusb_role_sw_get(struct device *dev)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	enum usb_role role;

	role = ssusb->is_host ? USB_ROLE_HOST : USB_ROLE_DEVICE;

	return role;
}

static int ssusb_role_sw_register(struct otg_switch_mtk *otg_sx)
{
	struct usb_role_switch_desc role_sx_desc = { 0 };
	struct ssusb_mtk *ssusb =
		container_of(otg_sx, struct ssusb_mtk, otg_switch);

	if (!otg_sx->role_sw_used)
		return 0;

	role_sx_desc.set = ssusb_role_sw_set;
	role_sx_desc.get = ssusb_role_sw_get;
	otg_sx->role_sw = usb_role_switch_register(ssusb->dev, &role_sx_desc);

	if (IS_ERR(otg_sx->role_sw))
		return PTR_ERR(otg_sx->role_sw);

	/* default to role none */
	ssusb_role_sw_set(ssusb->dev, USB_ROLE_NONE);

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

	dev_info(dev, "store cmode %d op_mode %d\n", mode, otg_sx->op_mode);

	if (otg_sx->op_mode != mode) {
		/* set switch role */
		switch (mode) {
		case MTU3_DR_OPERATION_NONE:
			otg_sx->latest_role = USB_ROLE_NONE;
			break;
		case MTU3_DR_OPERATION_NORMAL:
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
		otg_sx->op_mode = MTU3_DR_OPERATION_NORMAL;
		/* switch usb role */
		ssusb_role_sw_set(ssusb->dev, otg_sx->latest_role);
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
	otg_sx->op_mode = MTU3_DR_OPERATION_NORMAL;

	ret = sysfs_create_group(&ssusb->dev->kobj, &ssusb_dr_group);
	if (ret)
		dev_info(ssusb->dev, "error creating sysfs attributes\n");

	#if defined(CONFIG_MACH_MT6779)
	/* add vcore quest */
	mtk_pm_qos_add_request(&vcore_pm_qos, MTK_PM_QOS_VCORE_OPP,
			MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE);
	pr_info("%s: add default Vcore QOS request\n", __func__);
	#endif

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
