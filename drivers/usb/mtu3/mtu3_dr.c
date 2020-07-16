/*
 * mtu3_dr.c - dual role switch and host glue layer
 *
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "mtu3.h"
#include "mtu3_dr.h"

#include <charger_class.h>
#include <mtk_boot_common.h>

#define USB2_PORT 2
#define USB3_PORT 3

enum mtu3_vbus_id_state {
	MTU3_ID_FLOAT = 1,
	MTU3_ID_GROUND,
	MTU3_VBUS_OFF,
	MTU3_VBUS_VALID,
};

struct extcon_dev *g_extcon_edev;

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

static void ssusb_dev_sw_reset(struct ssusb_mtk *ssusb)
{
	/* reset ssusb dev */
	mtu3_setbits(ssusb->ippc_base, U3D_SSUSB_DEV_RST_CTRL,
		SSUSB_DEV_SW_RST);
	udelay(1);
	mtu3_clrbits(ssusb->ippc_base, U3D_SSUSB_DEV_RST_CTRL,
		SSUSB_DEV_SW_RST);

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
		#ifdef CONFIG_MTK_CHARGER
		charger_dev_enable_otg(ssusb->chg_dev, true);
		#endif
		ret = regulator_enable(vbus);
		if (ret) {
			dev_err(ssusb->dev, "vbus regulator enable failed\n");
			return ret;
		}
	} else {
		#ifdef CONFIG_MTK_CHARGER
		charger_dev_enable_otg(ssusb->chg_dev, false);
		#endif
		regulator_disable(vbus);
	}

	return 0;
}

static void ssusb_set_force_mode(struct ssusb_mtk *ssusb,
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

static void clk_control_dwork(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ssusb_mtk *ssusb =
		container_of(dwork, struct ssusb_mtk, clk_ctl_dwork);

	ssusb_phy_power_off(ssusb);
	ssusb_clks_disable(ssusb);
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
	int ret = 0;
	struct platform_device *pdev = to_platform_device(ssusb->dev);
	struct device_node *node = pdev->dev.of_node;

	dev_dbg(ssusb->dev, "mailbox state(%d)\n", status);

	switch (status) {
	case MTU3_ID_GROUND:
		if (!ssusb->keep_ao) {
			ret = ssusb_clks_enable(ssusb);
			if (ret) {
				dev_err(ssusb->dev, "failed to enable clock\n");
				break;
			}
			ret = ssusb_phy_power_on(ssusb);
			if (ret) {
				dev_err(ssusb->dev, "failed to power on phy\n");
				goto err_power_on;
			}
			ssusb_ip_sw_reset(ssusb);
			ssusb_dev_sw_reset(ssusb);
			ssusb_set_force_mode(ssusb, MTU3_DR_FORCE_HOST);
			ret = ssusb_host_init(ssusb, node);
			if (ret) {
				dev_info(ssusb->dev, "failed to initialize host\n");
				goto err_host_init;
			}
		} else {
			ssusb_set_force_mode(ssusb, MTU3_DR_FORCE_HOST);
			switch_port_to_host(ssusb);
		}
		ssusb_set_vbus(otg_sx, 1);
		ssusb->is_host = true;
		mtu3_drp_to_host(mtu);
		break;
	case MTU3_ID_FLOAT:
		ssusb->is_host = false;
		ssusb_set_vbus(otg_sx, 0);
		switch_port_to_device(ssusb);
		mtu3_drp_to_none(mtu);
		if (!ssusb->keep_ao) {
			ssusb_host_exit(ssusb);
			schedule_delayed_work(&ssusb->clk_ctl_dwork, 2 * HZ);
			pm_wakeup_event(ssusb->dev, 3000);
		}
		break;
	case MTU3_VBUS_OFF:
		mtu3_stop(mtu);
		pm_relax(ssusb->dev);
		spin_lock_irqsave(&mtu->lock, flags);
		mtu3_gadget_disconnect(mtu);
		spin_unlock_irqrestore(&mtu->lock, flags);
		mtu3_drp_to_none(mtu);
		if (!ssusb->keep_ao) {
			ssusb_phy_power_off(ssusb);
			ssusb_clks_disable(ssusb);
		}
		break;
	case MTU3_VBUS_VALID:
		if (ssusb->is_host == true)
			break;
		if (!ssusb->keep_ao) {
			ret = ssusb_clks_enable(ssusb);
			if (ret) {
				dev_err(ssusb->dev, "failed to enable clock\n");
				break;
			}
			ret = ssusb_phy_power_on(ssusb);
			if (ret) {
				dev_err(ssusb->dev, "failed to power on phy\n");
				goto err_power_on;
			}
			ssusb_ip_sw_reset(ssusb);
			ssusb_dev_sw_reset(ssusb);
		}
		ssusb_set_force_mode(ssusb, MTU3_DR_FORCE_DEVICE);
		/* avoid suspend when works as device */
		switch_port_to_device(ssusb);
		pm_stay_awake(ssusb->dev);
		mtu3_start(mtu);
		break;
	default:
		dev_err(ssusb->dev, "invalid state\n");
	}

	return;

err_host_init:
	ssusb_phy_power_off(ssusb);
err_power_on:
	ssusb_clks_disable(ssusb);
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

	g_extcon_edev = otg_sx->edev;

	otg_sx->vbus_nb.notifier_call = ssusb_vbus_notifier;
	ret = devm_extcon_register_notifier(ssusb->dev, edev, EXTCON_USB,
					&otg_sx->vbus_nb);
	if (ret < 0)
		dev_err(ssusb->dev, "failed to register notifier for USB\n");

	otg_sx->id_nb.notifier_call = ssusb_id_notifier;
	ret = devm_extcon_register_notifier(ssusb->dev, edev, EXTCON_USB_HOST,
					&otg_sx->id_nb);
	if (ret < 0)
		dev_err(ssusb->dev, "failed to register notifier for USB-HOST\n");

	dev_dbg(ssusb->dev, "EXTCON_USB: %d, EXTCON_USB_HOST: %d\n",
		extcon_get_state(edev, EXTCON_USB),
		extcon_get_state(edev, EXTCON_USB_HOST));

	ssusb_set_vbus(otg_sx, 0);

	/* default as host, switch to device mode if needed */
	if (extcon_get_state(edev, EXTCON_USB_HOST) == true)
		ssusb_set_mailbox(otg_sx, MTU3_ID_GROUND);
	if (extcon_get_state(edev, EXTCON_USB) == true)
		ssusb_set_mailbox(otg_sx, MTU3_VBUS_VALID);

	return 0;
}

static void extcon_register_dwork(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct otg_switch_mtk *otg_sx =
	    container_of(dwork, struct otg_switch_mtk, extcon_reg_dwork);

	ssusb_extcon_register(otg_sx);
}

/*
 * We provide an interface via debugfs to switch between host and device modes
 * depending on user input.
 * This is useful in special cases, such as uses TYPE-A receptacle but also
 * wants to support dual-role mode.
 * It generates cable state changes by pulling up/down IDPIN and
 * notifies driver to switch mode by "extcon-usb-gpio".
 * NOTE: when use MICRO receptacle, should not enable this interface.
 */
static void ssusb_mode_manual_switch(struct ssusb_mtk *ssusb, int to_host)
{
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;

	if (to_host)
		pinctrl_select_state(otg_sx->id_pinctrl, otg_sx->id_ground);
	else
		pinctrl_select_state(otg_sx->id_pinctrl, otg_sx->id_float);
}


static int ssusb_mode_show(struct seq_file *sf, void *unused)
{
	struct ssusb_mtk *ssusb = sf->private;

	seq_printf(sf, "current mode: %s(%s drd)\n(echo device/host)\n",
		ssusb->is_host ? "host" : "device",
		ssusb->otg_switch.manual_drd_enabled ? "manual" : "auto");

	return 0;
}

static int ssusb_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, ssusb_mode_show, inode->i_private);
}

static ssize_t ssusb_mode_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *sf = file->private_data;
	struct ssusb_mtk *ssusb = sf->private;
	char buf[16];

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (!strncmp(buf, "host", 4) && !ssusb->is_host) {
		ssusb_mode_manual_switch(ssusb, 1);
	} else if (!strncmp(buf, "device", 6) && ssusb->is_host) {
		ssusb_mode_manual_switch(ssusb, 0);
	} else {
		dev_err(ssusb->dev, "wrong or duplicated setting\n");
		return -EINVAL;
	}

	return count;
}

static const struct file_operations ssusb_mode_fops = {
	.open = ssusb_mode_open,
	.write = ssusb_mode_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void ssusb_debugfs_init(struct ssusb_mtk *ssusb)
{
	struct dentry *root;
	struct dentry *file;

	root = debugfs_create_dir(dev_name(ssusb->dev), usb_debug_root);
	if (IS_ERR_OR_NULL(root)) {
		if (!root)
			dev_info(ssusb->dev, "create debugfs root failed\n");
		return;
	}
	ssusb->dbgfs_root = root;

	file = debugfs_create_file("mode", 0644, root,
			ssusb, &ssusb_mode_fops);
	if (!file)
		dev_dbg(ssusb->dev, "create debugfs mode failed\n");
}

static void ssusb_debugfs_exit(struct ssusb_mtk *ssusb)
{
	debugfs_remove_recursive(ssusb->dbgfs_root);
}

#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
static int mtu3_drp_get_prop(struct dual_role_phy_instance *drp_inst,
		enum dual_role_property prop, unsigned int *val)
{
	struct ssusb_mtk *ssusb = dual_role_get_drvdata(drp_inst);
	enum mtu3_dr_force_mode drp_state;
	int mode, pr, dr;
	int ret = 0;

	/*
	 * devm_dual_role_instance_register() may call this function,
	 * but haven't save ssusb into drp_inst->drv_data, so skip NULL value
	 */
	drp_state = ssusb ? ssusb->drp_state : MTU3_DR_FORCE_NONE;

	switch (drp_state) {
	case MTU3_DR_FORCE_DEVICE:
		mode = DUAL_ROLE_PROP_MODE_UFP;
		pr = DUAL_ROLE_PROP_PR_SNK;
		dr = DUAL_ROLE_PROP_DR_DEVICE;
		break;
	case MTU3_DR_FORCE_HOST:
		mode = DUAL_ROLE_PROP_MODE_DFP;
		pr = DUAL_ROLE_PROP_PR_SRC;
		dr = DUAL_ROLE_PROP_DR_HOST;
		break;
	case MTU3_DR_FORCE_NONE:
		/* fall through */
	default:
		mode = DUAL_ROLE_PROP_MODE_NONE;
		pr = DUAL_ROLE_PROP_PR_NONE;
		dr = DUAL_ROLE_PROP_DR_NONE;
		break;
	}

	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
		*val = mode;
		break;
	case DUAL_ROLE_PROP_PR:
		*val = pr;
		break;
	case DUAL_ROLE_PROP_DR:
		*val = dr;
		break;
	case DUAL_ROLE_PROP_VCONN_SUPPLY:
		*val = DUAL_ROLE_PROP_VCONN_SUPPLY_NO;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

void mtu3_drp_to_none(struct mtu3 *mtu3)
{
	struct ssusb_mtk *ssusb = mtu3->ssusb;

	ssusb->drp_state = MTU3_DR_FORCE_NONE;
	dual_role_instance_changed(ssusb->drp_inst);
}

void mtu3_drp_to_device(struct mtu3 *mtu3)
{
	struct ssusb_mtk *ssusb = mtu3->ssusb;

	ssusb->drp_state = MTU3_DR_FORCE_DEVICE;
	dual_role_instance_changed(ssusb->drp_inst);
}

void mtu3_drp_to_host(struct mtu3 *mtu3)
{
	struct ssusb_mtk *ssusb = mtu3->ssusb;

	ssusb->drp_state = MTU3_DR_FORCE_HOST;
	dual_role_instance_changed(ssusb->drp_inst);
}

static enum dual_role_property mtu3_dr_props[] = {
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
	/* DUAL_ROLE_PROP_VCONN_SUPPLY, */
};

static const struct dual_role_phy_desc mtu3_drp_desc = {
	.name = "dual-role-usb20",
	.supported_modes = DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP,
	.properties = mtu3_dr_props,
	.num_properties = ARRAY_SIZE(mtu3_dr_props),
	.get_property = mtu3_drp_get_prop,
};

/* provide typeC state, in fact it should be provided by typeC driver */
static int mtu3_drp_init(struct mtu3 *mtu3)
{
	struct dual_role_phy_instance *drp_inst;

	drp_inst = devm_dual_role_instance_register(mtu3->dev, &mtu3_drp_desc);
	if (IS_ERR(drp_inst)) {
		dev_err(mtu3->dev, "fail to register dual role instance\n");
		return PTR_ERR(drp_inst);
	}

	mtu3->ssusb->drp_inst = drp_inst;
	drp_inst->drv_data = mtu3->ssusb;

	return 0;
}
#endif

int ssusb_otg_switch_init(struct ssusb_mtk *ssusb)
{
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;

	INIT_DELAYED_WORK(&otg_sx->extcon_reg_dwork, extcon_register_dwork);

	#ifdef CONFIG_MTK_CHARGER
	ssusb->chg_dev = get_charger_by_name("primary_chg");
	#endif
	INIT_DELAYED_WORK(&ssusb->clk_ctl_dwork, clk_control_dwork);

	if (otg_sx->manual_drd_enabled)
		ssusb_debugfs_init(ssusb);

	/* It is enough to delay 1s for waiting for host initialization */
	schedule_delayed_work(&otg_sx->extcon_reg_dwork, HZ);
	#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
	mtu3_drp_init(ssusb->u3d);
	#endif

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

	if (otg_sx->manual_drd_enabled)
		ssusb_debugfs_exit(ssusb);
}

int ssusb_otg_detect(struct ssusb_mtk *ssusb)
{
	struct extcon_dev *edev = g_extcon_edev;
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;

	if (extcon_get_state(edev, EXTCON_USB_HOST) == true)
		ssusb_set_mailbox(otg_sx, MTU3_ID_GROUND);

	return 0;
}

bool mt_usb_is_device(void)
{
	int host_state = extcon_get_state(g_extcon_edev, EXTCON_USB_HOST);

	if (host_state == 1)
		return false;

	return true;
}
EXPORT_SYMBOL_GPL(mt_usb_is_device);
