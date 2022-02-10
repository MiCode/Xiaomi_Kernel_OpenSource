// SPDX-License-Identifier: GPL-2.0
/*
 * musb_dr.c - dual role switch and host glue layer
 *
 * Copyright (C) 2021 MediaTek Inc.
 *
 * Author: Macpaul Lin <macpaul.lin@mediatek.com>
 */

#include <linux/of_platform.h>

#include <usb20.h>
#include <musb_dr.h>
#include <musb_host.h>
#include <musb_gadget.h>
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <musb_debug.h>
#endif

#if IS_ENABLED(CONFIG_MTK_BASE_POWER)
#include "mtk_spm_resource_req.h"
#endif

#define USB2_PORT 2

enum mt_usb_vbus_id_state {
	MUSB_ID_FLOAT = 1,
	MUSB_ID_GROUND,
	MUSB_VBUS_OFF,
	MUSB_VBUS_VALID,
};

static char *mailbox_state_string(enum mt_usb_vbus_id_state state)
{
	switch (state) {
	case MUSB_ID_FLOAT:
		return "ID_FLOAT";
	case MUSB_ID_GROUND:
		return "ID_GROUND";
	case MUSB_VBUS_OFF:
		return "VBUS_OFF";
	case MUSB_VBUS_VALID:
		return "VBUS_VALID";
	default:
		return "UNKNOWN";
	}
}

int mt_usb_set_vbus(struct otg_switch_mtk *otg_sx, int is_on)
{
	struct mt_usb_glue *glue =
		container_of(otg_sx, struct mt_usb_glue, otg_sx);
	struct musb *musb = glue->mtk_musb;
	struct regulator *vbus = otg_sx->vbus;
	int ret;

	/* vbus is optional */
	if (!vbus)
		return 0;

	dev_dbg(musb->controller, "%s: turn %s\n", __func__, is_on ? "on" : "off");

	if (is_on) {
		ret = regulator_enable(vbus);
		if (ret) {
			dev_info(musb->controller, "vbus regulator enable failed\n");
			return ret;
		}
	} else {
		regulator_disable(vbus);
	}

	return 0;
}
EXPORT_SYMBOL(mt_usb_set_vbus);

static void mt_usb_gadget_disconnect(struct musb *musb)
{
	/* notify gadget driver */
	if (musb->g.speed == USB_SPEED_UNKNOWN)
		return;

	if (musb->gadget_driver && musb->gadget_driver->disconnect) {
		musb->gadget_driver->disconnect(&musb->g);
		musb->g.speed = USB_SPEED_UNKNOWN;
	}

	usb_gadget_set_state(&musb->g, USB_STATE_NOTATTACHED);
}

/*
 * switch to host: -> MUSB_VBUS_OFF --> MUSB_ID_GROUND
 * switch to device: -> MUSB_ID_FLOAT --> MUSB_VBUS_VALID
 */
static void mt_usb_set_mailbox(struct otg_switch_mtk *otg_sx,
	enum mt_usb_vbus_id_state status)
{
	struct mt_usb_glue *glue =
		container_of(otg_sx, struct mt_usb_glue, otg_sx);
	struct musb *musb = glue->mtk_musb;
	int i;

	dev_info(musb->controller, "mailbox %s\n", mailbox_state_string(status));
	switch (status) {
	case MUSB_ID_GROUND:
		mt_usb_set_vbus(otg_sx, 1);
		musb->is_ready = true;
		otg_sx->sw_state |= MUSB_ID_GROUND;
		mt_usb_host_connect(0);
		break;
	case MUSB_ID_FLOAT:
		mt_usb_host_disconnect(0);
		musb->is_ready = false;
		/* turn off VBUS until do_host_work switch to DEV mode */
		for (i = 0; i < 6; i++) {
			if (!musb->is_host)
				break;
			mdelay(50);
		}
		mt_usb_set_vbus(otg_sx, 0);
		otg_sx->sw_state &= ~MUSB_ID_GROUND;
		break;
	case MUSB_VBUS_OFF:
		/* ToDo or fix: killing any outstanding requests */
		mt_usb_set_vbus(otg_sx, false);
		musb->usb_connected = 0;
		musb->is_host = false;
		mt_usb_disconnect(); /* sync to UI */
		mt_usb_gadget_disconnect(musb); /* sync to UI */
		otg_sx->sw_state &= ~MUSB_VBUS_VALID;
		break;
	case MUSB_VBUS_VALID:
		mt_usb_set_vbus(otg_sx, true);
		/* avoid suspend when works as device */
		otg_sx->sw_state |= MUSB_VBUS_VALID;
		musb->usb_connected = 1;
		mt_usb_connect();
		break;
	default:
		dev_info(musb->controller, "invalid state\n");
	}
}

static void mt_usb_id_work(struct work_struct *work)
{
	struct otg_switch_mtk *otg_sx =
		container_of(work, struct otg_switch_mtk, id_work);

	if (otg_sx->id_event)
		mt_usb_set_mailbox(otg_sx, MUSB_ID_GROUND);
	else
		mt_usb_set_mailbox(otg_sx, MUSB_ID_FLOAT);
}

static void mt_usb_vbus_work(struct work_struct *work)
{
	struct otg_switch_mtk *otg_sx =
		container_of(work, struct otg_switch_mtk, vbus_work);

	if (otg_sx->vbus_event)
		mt_usb_set_mailbox(otg_sx, MUSB_VBUS_VALID);
	else
		mt_usb_set_mailbox(otg_sx, MUSB_VBUS_OFF);
}

/*
 * @mt_usb_id_notifier is called in atomic context, but @mt_usb_set_mailbox
 * may sleep, so use work queue here
 */
static int mt_usb_id_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct otg_switch_mtk *otg_sx =
		container_of(nb, struct otg_switch_mtk, id_nb);

	otg_sx->id_event = event;
	schedule_work(&otg_sx->id_work);

	return NOTIFY_DONE;
}

static int mt_usb_vbus_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct otg_switch_mtk *otg_sx =
		container_of(nb, struct otg_switch_mtk, vbus_nb);

	otg_sx->vbus_event = event;
	schedule_work(&otg_sx->vbus_work);

	return NOTIFY_DONE;
}

static int mt_usb_extcon_register(struct otg_switch_mtk *otg_sx)
{
	struct mt_usb_glue *glue =
		container_of(otg_sx, struct mt_usb_glue, otg_sx);
	struct musb *musb = glue->mtk_musb;
	struct extcon_dev *edev = otg_sx->edev;
	int ret;

	/* extcon is optional */
	if (!edev)
		return 0;

	otg_sx->vbus_nb.notifier_call = mt_usb_vbus_notifier;
	ret = devm_extcon_register_notifier(musb->controller, edev, EXTCON_USB,
					&otg_sx->vbus_nb);
	if (ret < 0) {
		dev_info(musb->controller, "failed to register notifier for USB\n");
		return ret;
	}

	otg_sx->id_nb.notifier_call = mt_usb_id_notifier;
	ret = devm_extcon_register_notifier(musb->controller, edev, EXTCON_USB_HOST,
					&otg_sx->id_nb);
	if (ret < 0) {
		dev_info(musb->controller, "failed to register notifier for USB-HOST\n");
		return ret;
	}

	dev_dbg(musb->controller, "EXTCON_USB: %d, EXTCON_USB_HOST: %d\n",
		extcon_get_state(edev, EXTCON_USB),
		extcon_get_state(edev, EXTCON_USB_HOST));

	/* default as host, switch to device mode if needed */
	if (extcon_get_state(edev, EXTCON_USB_HOST) == false)
		mt_usb_set_mailbox(otg_sx, MUSB_ID_FLOAT);
	if (extcon_get_state(edev, EXTCON_USB) == true)
		mt_usb_set_mailbox(otg_sx, MUSB_VBUS_VALID);

	return 0;
}

/*
 * We provide an interface via debugfs to switch between host and device modes
 * depending on user input.
 * This is useful in special cases, such as uses TYPE-A receptacle but also
 * wants to support dual-role mode.
 */
void mt_usb_mode_switch(struct musb *musb, int to_host)
{
	struct mt_usb_glue *glue =
		container_of(&musb, struct mt_usb_glue, mtk_musb);
	struct otg_switch_mtk *otg_sx = &glue->otg_sx;

	if (to_host) {
		mt_usb_set_mailbox(otg_sx, MUSB_VBUS_OFF);
		mt_usb_set_mailbox(otg_sx, MUSB_ID_GROUND);
	} else {
		mt_usb_set_mailbox(otg_sx, MUSB_ID_FLOAT);
		mt_usb_set_mailbox(otg_sx, MUSB_VBUS_VALID);
	}
}
EXPORT_SYMBOL(mt_usb_mode_switch);

static int mt_usb_role_sx_set(struct usb_role_switch *sw, enum usb_role role)
{
	struct mt_usb_glue *glue = usb_role_switch_get_drvdata(sw);
	struct otg_switch_mtk *otg_sx = &glue->otg_sx;
	struct device *dev = glue->dev;
	bool id_event, vbus_event;
	static bool first_init = true;

	dev_info(dev, "role_sx_set role %d, latest_role: %d\n",
		role, otg_sx->latest_role);

	/* Avoid transit from HOST -> DEV with NONE state */
	if ((role == USB_ROLE_DEVICE && otg_sx->latest_role == USB_ROLE_HOST) ||
		(role == USB_ROLE_HOST && otg_sx->latest_role == USB_ROLE_DEVICE)) {
		DBG(0, "force USB_ROLE_NONE transit state.\n");
		mt_usb_role_sx_set(sw, USB_ROLE_NONE);
	}

	otg_sx->latest_role = role;

	if (otg_sx->op_mode != MUSB_DR_OPERATION_NORMAL) {
		dev_info(dev, "op_mode %d, skip set role\n", otg_sx->op_mode);
		return 0;
	}

	id_event = (role == USB_ROLE_HOST);
	vbus_event = (role == USB_ROLE_DEVICE);

#if IS_ENABLED(CONFIG_MTK_UART_USB_SWITCH)
	in_uart_mode = usb_phy_check_in_uart_mode();
	if (in_uart_mode) {
		DBG(0, "At UART mode. Switch to USB is not support\n");
		mt_usb_set_mailbox(otg_sx, MUSB_VBUS_OFF);
		phy_set_mode(glue->phy, PHY_MODE_INVALID);
		return 0;
	}
#endif

	if (!!(otg_sx->sw_state & MUSB_VBUS_VALID) ^ vbus_event) {
		if (vbus_event) {
			dev_info(dev, "%s: if vbus_event true\n", __func__);
			/* phy_set_mode(glue->phy, PHY_MODE_USB_DEVICE); */
			/* PHY mode will be set in do_connection_work */
			set_usb_phy_clear();
			phy_power_on(glue->phy);
			mt_usb_set_mailbox(otg_sx, MUSB_VBUS_VALID);
		} else {
			mt_usb_set_mailbox(otg_sx, MUSB_VBUS_OFF);
			dev_info(dev, "%s: if vbus_event false\n", __func__);
			phy_power_off(glue->phy);
		}
	}

	if (!!(otg_sx->sw_state & MUSB_ID_GROUND) ^ id_event) {
		if (id_event) {
			dev_info(dev, "%s: if id_event true\n", __func__);

			phy_power_on(glue->phy);

			/* PHY mode will be set in host_connect work */
			mt_usb_set_mailbox(otg_sx, MUSB_ID_GROUND);
		} else {
			/*
			 * add this for reduce boot 200ms
			 * and add delay 200ms for plugout
			 */
			if (!first_init)
				mdelay(200);
			else
				first_init = false;

			/* PHY mode will be set in host_disconnect work */
			mt_usb_set_mailbox(otg_sx, MUSB_ID_FLOAT);
			phy_power_off(glue->phy);
		}
	}

	return 0;
}

static enum usb_role mt_usb_role_sx_get(struct usb_role_switch *sw)
{
	struct mt_usb_glue *glue = usb_role_switch_get_drvdata(sw);
	struct musb *musb = glue->mtk_musb;
	enum usb_role role;

	role = musb->is_host ? USB_ROLE_HOST : USB_ROLE_DEVICE;

	return role;
}

static int mt_usb_role_sw_register(struct otg_switch_mtk *otg_sx)
{
	struct usb_role_switch_desc role_sx_desc = { 0 };
	struct mt_usb_glue *glue =
		container_of(otg_sx, struct mt_usb_glue, otg_sx);
	struct musb *musb = glue->mtk_musb;

	if (!otg_sx->role_sw_used)
		return 0;

	role_sx_desc.set = mt_usb_role_sx_set;
	role_sx_desc.get = mt_usb_role_sx_get;
	role_sx_desc.fwnode = dev_fwnode(glue->dev);
	role_sx_desc.driver_data = glue;
	otg_sx->role_sw = usb_role_switch_register(glue->dev, &role_sx_desc);

	if (IS_ERR(otg_sx->role_sw))
		return PTR_ERR(otg_sx->role_sw);

	mt_usb_role_sx_set(otg_sx->role_sw, USB_ROLE_NONE);
	musb->usb_connected = 0;

	return 0;
}

static ssize_t cmode_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct musb *mtk_musb = dev_get_drvdata(dev);
	struct otg_switch_mtk *otg_sx = mtk_musb->otg_sx;
	enum usb_role role = otg_sx->latest_role;

	int mode;

	if (kstrtoint(buf, 10, &mode))
		return -EINVAL;

	dev_info(dev, "store cmode %d op_mode %d\n", mode, otg_sx->op_mode);

	if (otg_sx->op_mode != mode) {
		/* set switch role */
		switch (mode) {
		case MUSB_DR_OPERATION_NONE:
			otg_sx->latest_role = USB_ROLE_NONE;
			break;
		case MUSB_DR_OPERATION_NORMAL:
			/* switch usb role to latest role */
			break;
		case MUSB_DR_OPERATION_HOST:
			otg_sx->latest_role = USB_ROLE_HOST;
			break;
		case MUSB_DR_OPERATION_DEVICE:
			otg_sx->latest_role = USB_ROLE_DEVICE;
			break;
		default:
			return -EINVAL;
		}
		/* switch operation mode to normal temporarily */
		otg_sx->op_mode = MUSB_DR_OPERATION_NORMAL;
		/* switch usb role */
		mt_usb_role_sx_set(otg_sx->role_sw, otg_sx->latest_role);
		/* update operation mode */
		otg_sx->op_mode = mode;
		/* restore role */
		otg_sx->latest_role = role;
	}

	return count;
}

static ssize_t cmode_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct musb *mtk_musb = dev_get_drvdata(dev);
	struct otg_switch_mtk *otg_sx = mtk_musb->otg_sx;

	return sprintf(buf, "%d\n", otg_sx->op_mode);
}
static DEVICE_ATTR_RW(cmode);

static struct attribute *mt_usb_dr_attrs[] = {
	&dev_attr_cmode.attr,
	NULL
};

static const struct attribute_group mt_usb_dr_group = {
	.attrs = mt_usb_dr_attrs,
};

int mt_usb_otg_switch_init(struct mt_usb_glue *glue)
{
	struct otg_switch_mtk *otg_sx = &glue->otg_sx;
	struct musb *mtk_musb = glue->mtk_musb;
	int ret = 0;

	/* we need to keep otg_sx here for cmode operations */
	mtk_musb->otg_sx = otg_sx;

	INIT_WORK(&otg_sx->id_work, mt_usb_id_work);
	INIT_WORK(&otg_sx->vbus_work, mt_usb_vbus_work);

	/* default as host, update state */
	otg_sx->sw_state = mtk_musb->is_host ?
				MUSB_ID_GROUND : MUSB_VBUS_VALID;

	/* initial operation mode */
	otg_sx->op_mode = MUSB_DR_OPERATION_NORMAL;

	ret = sysfs_create_group(&mtk_musb->controller->kobj, &mt_usb_dr_group);
	if (ret)
		dev_info(mtk_musb->controller, "error creating sysfs attributes\n");

#if IS_ENABLED(CONFIG_DEBUG_FS)
	if (otg_sx->manual_drd_enabled)
		musb_dr_debugfs_init(mtk_musb);
#endif
	else if (otg_sx->role_sw_used)
		ret = mt_usb_role_sw_register(otg_sx);
	else
		ret = mt_usb_extcon_register(otg_sx);

	return ret;
}
EXPORT_SYMBOL(mt_usb_otg_switch_init);

void mt_usb_otg_switch_exit(struct mt_usb_glue *glue)
{
	struct otg_switch_mtk *otg_sx = &glue->otg_sx;
	struct musb *mtk_musb = glue->mtk_musb;

	cancel_work_sync(&otg_sx->id_work);
	cancel_work_sync(&otg_sx->vbus_work);
	usb_role_switch_unregister(otg_sx->role_sw);
	sysfs_remove_group(&mtk_musb->controller->kobj, &mt_usb_dr_group);
}
