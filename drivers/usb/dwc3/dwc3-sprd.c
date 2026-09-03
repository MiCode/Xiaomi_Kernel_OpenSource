/**
 * dwc3-sprd.c - Spreadtrum DWC3 Specific Glue layer
 *
 * Copyright (c) 2018 Spreadtrum Co., Ltd.
 *		http://www.spreadtrum.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/usb/gadget.h>
#include <linux/usb/phy.h>
#include <linux/usb/sprd_commonphy.h>
#include <linux/usb/sprd_typec.h>
#include <linux/usb/usb_phy_generic.h>
#include <linux/wait.h>
#include <linux/extcon.h>
#include <linux/regmap.h>
#include <linux/usb/sprd_usbm.h>
#include <linux/usb/role.h>
#include <dt-bindings/soc/sprd,qogirn6pro-mask.h>
#include <dt-bindings/soc/sprd,qogirn6pro-regs.h>
#include <dt-bindings/mfd/sprd,ump9620-mask.h>
#include <dt-bindings/mfd/sprd,ump9620-regs.h>

#include "sprd/core.h"
#include "sprd/gadget.h"
#include "sprd/io.h"

#define ID			0
#define B_SESS_VLD		1
#define B_SUSPEND		2
#define A_SUSPEND		3
#define A_RECOVER		4
#define A_AUDIO			5

#define CHARGER_DETECT_DONE			BIT(0)
#define DWC3_CHG_MAX_WAIT_BC1P2_COUNT		5
#define VBUS_REG_CHECK_DELAY			(msecs_to_jiffies(1000))
#define DWC3_RUNTIME_CHECK_DELAY		(msecs_to_jiffies(100))
#define DWC3_UDC_START_CHECK_DELAY		(msecs_to_jiffies(50))
#define DWC3_USB_ENABLE_CHECK_DELAY		(msecs_to_jiffies(50))
#define DWC3_CHG_DETECT_DELAY			(msecs_to_jiffies(500))
#define DWC3_SPRD_CHG_MAX_REDETECT_COUNT	3

#define DWC3_AUTOSUSPEND_DELAY 1000

#undef dev_dbg
#define dev_dbg dev_info

enum dwc3_id_state {
	DWC3_ID_GROUND = 0,
	DWC3_ID_FLOAT,
};

enum dwc3_drd_state {
	DRD_STATE_UNDEFINED = 0,
	DRD_STATE_IDLE,
	DRD_STATE_PERIPHERAL,
	DRD_STATE_PERIPHERAL_SUSPEND,
	DRD_STATE_HOST_IDLE,
	DRD_STATE_HOST,
	DRD_STATE_HOST_AUDIO,
};

enum usb_chg_detect_state {
	USB_CHG_STATE_UNDETECT = 0,
	USB_CHG_STATE_DETECTED,
	USB_CHG_STATE_RETRY_DETECT,
	USB_CHG_STATE_RETRY_DETECTED,
};

static const char *const state_names[] = {
	[DRD_STATE_UNDEFINED] = "undefined",
	[DRD_STATE_IDLE] = "idle",
	[DRD_STATE_PERIPHERAL] = "peripheral",
	[DRD_STATE_PERIPHERAL_SUSPEND] = "peripheral_suspend",
	[DRD_STATE_HOST_IDLE] = "host_idle",
	[DRD_STATE_HOST] = "host",
	[DRD_STATE_HOST_AUDIO] = "host_audio",
};

const char *dwc3_drd_state_string(enum dwc3_drd_state state)
{
	if (state >= ARRAY_SIZE(state_names))
		return "UNKNOWN";

	return state_names[state];
}

struct dwc3_sprd {
	struct device		*dev;
	void __iomem		*base;
	struct platform_device	*dwc3;
	int			irq;

	struct clk		*core_clk;
	struct clk		*ref_clk;
	struct clk		*susp_clk;
	struct clk		*ipa_usb_ref_clk;
	struct clk		*ipa_usb_ref_parent;
	struct clk		*ipa_usb_ref_default;
	struct clk		*ipa_dpu1_clk;
	struct clk		*ipa_dptx_clk;
	struct clk		*ipa_tca_clk;
	struct clk		*ipa_usb31pll_clk;

	struct usb_phy		*hs_phy;
	struct usb_phy		*ss_phy;
	struct extcon_dev	*edev;
	struct extcon_dev	*id_edev;
	struct notifier_block	vbus_nb;
	struct notifier_block	id_nb;
	struct notifier_block	audio_nb;
	struct regulator	*vbus;

	struct wakeup_source	*wake_lock;
	spinlock_t		lock;

	enum dwc3_id_state	id_state;
	enum dwc3_drd_state	drd_state;
	enum usb_chg_detect_state	chg_state;
	enum usb_charger_type	chg_type;
	enum usb_dr_mode	glue_dr_mode;
	bool			vbus_active;
	bool			charging_mode;
	bool			is_audio_dev;
	bool			gadget_suspended;
	bool			in_restart;
	bool			host_recover;
	bool			use_pdhub_c2c;
	bool			vddgen1_cleared;

	atomic_t		runtime_suspended;
	atomic_t		pm_suspended;
	int			wait_chg_detect_count;
	int			retry_chg_detect_count;
	int			start_host_retry_count;
	int			usb_data_enabled;
	unsigned long		inputs;
	struct workqueue_struct *dwc3_wq;
	struct workqueue_struct *sm_usb_wq;
	struct work_struct	evt_prepare_work;
	struct delayed_work	hotplug_sm_work;
	struct delayed_work	chg_detect_work;
	struct mutex		suspend_resume_mutex;

	struct usb_role_switch *dev_role_sw;
	struct dev_pm_ops	dwc3_pm_ops;
	struct dev_pm_ops	xhci_pm_ops;

	struct regmap		*pmic;
	struct regmap		*pmu_apb;
	struct regmap		*ipa_apb;
	struct regmap		*ipa_dispc1_glb_apb;
	struct gpio_desc	*gpiod_pme_wakeup;
	int			wakeup_irq;
	bool			support_suspend;
};

struct sprd_dwc3_usb_udc {
	struct usb_gadget_driver	*driver;
	struct usb_gadget		*gadget;
	struct device			dev;
	struct list_head		list;
	bool				vbus;
	bool				started;
};

#define DWC3_SUSPEND_COUNT	100
#define DWC3_UDC_START_COUNT	1000
#define DWC3_START_TIMEOUT	200
#define DWC3_EXTCON_DELAY	1000

static int boot_charging;
static bool boot_calibration;
static int dwc3_probe_finish;

static ssize_t maximum_speed_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct dwc3_sprd *sdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc;

	if (!sdwc)
		return -EINVAL;

	dwc = platform_get_drvdata(sdwc->dwc3);
	if (!dwc)
		return -EINVAL;

	return sprintf(buf, "%s\n", usb_speed_string(dwc->gadget->max_speed));
}

static ssize_t maximum_speed_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct dwc3_sprd *sdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc;
	u32 max_speed;

	if (!sdwc)
		return -EINVAL;

	if (kstrtouint(buf, 0, &max_speed))
		return -EINVAL;

	if (max_speed <= USB_SPEED_UNKNOWN || max_speed > USB_SPEED_SUPER_PLUS)
		return -EINVAL;

	dwc = platform_get_drvdata(sdwc->dwc3);
	if (!dwc)
		return -EINVAL;

	dwc->gadget->max_speed = max_speed;
	return size;
}
static DEVICE_ATTR_RW(maximum_speed);

static ssize_t current_speed_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct dwc3_sprd *sdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc;

	if (!sdwc)
		return -EINVAL;

	dwc = platform_get_drvdata(sdwc->dwc3);
	if (!dwc)
		return -EINVAL;

	return sprintf(buf, "%s\n", usb_speed_string(dwc->gadget->speed));
}
static DEVICE_ATTR_RO(current_speed);

static struct attribute *dwc3_sprd_attrs[] = {
	&dev_attr_maximum_speed.attr,
	&dev_attr_current_speed.attr,
	NULL
};
ATTRIBUTE_GROUPS(dwc3_sprd);

static struct class *usb_notify_class;
static struct device *usb_notify_dev;
static ssize_t usb_data_enabled_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct dwc3_sprd *sdwc = dev_get_drvdata(dev);
	int usb_data_enabled_flag = sdwc->usb_data_enabled;

	return sprintf(buf, "%d\n", usb_data_enabled_flag);
}

static ssize_t usb_data_enabled_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	int value = 0;
	int ret = 0;
	unsigned long flags;
	struct dwc3_sprd *sdwc = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 10, &value);
	if (ret) {
		dev_err(dev, "input err:%d\n", ret);
		return count;
	}
	spin_lock_irqsave(&sdwc->lock, flags);
	dev_info(dev, "usb_data_enabled input: %d current: %d\n",
			value, sdwc->usb_data_enabled);

	if (sdwc->usb_data_enabled != value) {
		sdwc->usb_data_enabled = value;
		queue_work(sdwc->dwc3_wq, &sdwc->evt_prepare_work);
	}

	spin_unlock_irqrestore(&sdwc->lock, flags);

	return count;
}
static  DEVICE_ATTR(usb_data_enabled, 0640,
			       usb_data_enabled_show, usb_data_enabled_store);

static struct attribute *usb_data_control_attrs[] = {
	&dev_attr_usb_data_enabled.attr,
	NULL
};

static const struct attribute_group usb_data_control_group = {
	.attrs = usb_data_control_attrs,
};

static int dwc3_sprd_usb_notify_init(struct platform_device *pdev, void *data)
{
	int ret = 0;

	usb_notify_class = class_create(THIS_MODULE, "usb_notify");
	if (IS_ERR_OR_NULL(usb_notify_class)) {
		dev_err(&pdev->dev, "usb_notify class create err.\n");
		ret = PTR_ERR(usb_notify_class);
		goto out;
	}

	usb_notify_dev =
		device_create(usb_notify_class, &pdev->dev, 0, NULL, "usb_control");
	if (IS_ERR_OR_NULL(usb_notify_dev)) {
		dev_err(&pdev->dev, "usb_notify class create err.\n");
		ret = PTR_ERR(usb_notify_dev);
		class_destroy(usb_notify_class);
		goto out;
	}

	ret = sysfs_create_group(&usb_notify_dev->kobj, &usb_data_control_group);
	if (ret) {
		dev_err(&pdev->dev, "sysfs create err. ret:%d\n", ret);
		device_destroy(usb_notify_class, usb_notify_dev->devt);
		class_destroy(usb_notify_class);
		goto out;
	}

	dev_set_drvdata(usb_notify_dev, data);
	dev_info(&pdev->dev, "[%s] --\n", __func__);

out:
	return ret;
}

static void dwc3_sprd_usb_notify_exit(struct platform_device *pdev)
{
	if (usb_notify_dev) {
		sysfs_remove_group(&usb_notify_dev->kobj, &usb_data_control_group);
		device_destroy(usb_notify_class, usb_notify_dev->devt);
	}

	if (usb_notify_class)
		class_destroy(usb_notify_class);

	dev_info(&pdev->dev, "[%s] --\n", __func__);
}

static u32 sdwc_readl(void __iomem *base, u32 offset)
{
	u32 value;
	value = readl(base + offset - DWC3_GLOBALS_REGS_START);
	return value;
}

static void sdwc_writel(void __iomem *base, u32 offset, u32 value)
{
	writel(value, base + offset - DWC3_GLOBALS_REGS_START);
}

static void dwc3_flush_all_events(struct dwc3_sprd *sdwc)
{
	struct dwc3 *dwc = platform_get_drvdata(sdwc->dwc3);
	struct dwc3_event_buffer *evt;
	unsigned long flags;
	u32 reg;

	/* Skip remaining events on disconnect */
	spin_lock_irqsave(&dwc->lock, flags);

	reg = sdwc_readl(dwc->regs, DWC3_GEVNTSIZ(0));
	reg |= DWC3_GEVNTSIZ_INTMASK;
	sdwc_writel(dwc->regs, DWC3_GEVNTSIZ(0), reg);

	evt = dwc->ev_buf;
	evt->lpos = (evt->lpos + evt->count) % DWC3_EVENT_BUFFERS_SIZE;
	evt->count = 0;
	evt->flags &= ~DWC3_EVENT_PENDING;
	spin_unlock_irqrestore(&dwc->lock, flags);
}

static int dwc3_sprd_charger_mode(void)
{
	struct device_node *cmdline_node;
	const char *cmdline, *mode;
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmdline);

	if (ret) {
		pr_err("Can't not parse bootargs\n");
		return 0;
	}

	mode = strstr(cmdline, "androidboot.mode=charger");

	if (mode)
		return 1;
	else {
		mode = strstr(cmdline, "sprdboot.mode=charger");
		if (mode)
			return 1;
		else
			return 0;
	}

}

static int dwc3_sprd_calibration_mode(void)
{
	struct device_node *cmdline_node;
	const char *cmdline;
	int ret;
	bool mode;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmdline);

	if (ret) {
		pr_err("Can't not parse bootargs\n");
		return 0;
	}

	mode = (strstr(cmdline, "androidboot.mode=cali") != NULL) ||
	       (strstr(cmdline, "androidboot.mode=autotest") != NULL) ||
	       (strstr(cmdline, "sprdboot.mode=cali") != NULL) ||
	       (strstr(cmdline, "sprdboot.mode=autotest") != NULL);

	if (mode)
		return 1;
	else
		return 0;
}

#if IS_ENABLED(CONFIG_SPRD_REDRIVER_PTN38003A)
extern int ptn38003a_mode_usb32_set(unsigned int enable);
#endif
/*if HW didn't have ptn38003a, we should limit usb speed to 3.0 */
static void adjust_dwc3_max_speed(struct dwc3_sprd *sdwc)
{
	struct dwc3 *dwc = platform_get_drvdata(sdwc->dwc3);
	u32 reg;
	unsigned long flags;

#if IS_ENABLED(CONFIG_SPRD_REDRIVER_PTN38003A)
	if (ptn38003a_mode_usb32_set(1)) {
		spin_lock_irqsave(&dwc->lock, flags);
		reg = sdwc_readl(dwc->regs, DWC3_DCFG);
		reg &= ~(DWC3_DCFG_SPEED_MASK);
		reg |= DWC3_DCFG_SUPERSPEED;
		sdwc_writel(dwc->regs, DWC3_DCFG, reg);
		reg = sdwc_readl(dwc->regs, DWC3_DSTS);
		spin_unlock_irqrestore(&dwc->lock, flags);
		dev_info(dwc->dev, "limit dwc3 max speed to usb30, DWC3_DSTS: 0x%x\n", reg);
	} else {
		dev_info(dwc->dev, "Donnot limit dwc3 max speed!\n");
	}
#endif

	if (dwc3_sprd_calibration_mode()) {
		spin_lock_irqsave(&dwc->lock, flags);
		reg = sdwc_readl(dwc->regs, DWC3_DCFG);
		reg &= ~(DWC3_DCFG_SPEED_MASK);
		reg |= DWC3_DCFG_HIGHSPEED;
		sdwc_writel(dwc->regs, DWC3_DCFG, reg);
		reg = sdwc_readl(dwc->regs, DWC3_DSTS);
		spin_unlock_irqrestore(&dwc->lock, flags);
		dev_info(dwc->dev, "set dwc3 max speed to hs in cali mode, DWC3_DSTS: 0x%x\n", reg);
	}
}

static int dwc3_sprd_is_udc_start(struct dwc3_sprd *sdwc)
{
	struct dwc3 *dwc = platform_get_drvdata(sdwc->dwc3);
	unsigned long flags;

	spin_lock_irqsave(&dwc->lock, flags);
	if (!dwc->gadget_driver) {
		spin_unlock_irqrestore(&dwc->lock, flags);
		return 0;
	}

	spin_unlock_irqrestore(&dwc->lock, flags);
	return 1;
}

static void dwc3_sprd_chg_detect_work(struct work_struct *work)
{
	struct dwc3_sprd *sdwc =
		container_of(work, struct dwc3_sprd, chg_detect_work.work);
	struct usb_phy *usb_phy = sdwc->ss_phy;
	unsigned long flags;
	enum usb_charger_type	chg_type = UNKNOWN_TYPE;
	unsigned long delay = 0;
	bool rework = false;

	spin_lock_irqsave(&sdwc->lock, flags);

	switch (sdwc->chg_state) {
	case USB_CHG_STATE_UNDETECT:
		if (!sdwc->vbus_active) {
			dev_info(sdwc->dev, "dwc3:line%d: vbus_active 0\n", __LINE__);
			break;
		}

		if (boot_charging) {
			dev_info(sdwc->dev, "boot charging mode enter!\n");
			sdwc->charging_mode = true;
			sdwc->hs_phy->last_event = USB_EVENT_CHARGER;
			sdwc->ss_phy->last_event = USB_EVENT_CHARGER;
		}

		spin_unlock_irqrestore(&sdwc->lock, flags);
		if (usb_phy->charger_detect)
			chg_type = usb_phy->charger_detect(usb_phy);
		spin_lock_irqsave(&sdwc->lock, flags);

		if (!sdwc->vbus_active) {
			dev_info(sdwc->dev, "dwc3:line%d: vbus_active 0\n", __LINE__);
			break;
		}

		if (!(usb_phy->flags & CHARGER_DETECT_DONE)
		    && sdwc->wait_chg_detect_count < DWC3_CHG_MAX_WAIT_BC1P2_COUNT) {
			dev_info(sdwc->dev, "dwc3:wait bc1.2 done");
			sdwc->wait_chg_detect_count++;
			rework = true;
			delay = DWC3_CHG_DETECT_DELAY;
			break;
		}

		sdwc->chg_type = chg_type;
		sdwc->chg_state = USB_CHG_STATE_DETECTED;
		fallthrough;
	case USB_CHG_STATE_DETECTED:
		dev_info(sdwc->dev, "charger = %d\n", sdwc->chg_type);
		if (sdwc->chg_type == UNKNOWN_TYPE) {
			dev_info(sdwc->dev, "charge detect finished\n");
			sdwc->charging_mode = true;
			sdwc->hs_phy->last_event = USB_EVENT_CHARGER;
			sdwc->ss_phy->last_event = USB_EVENT_CHARGER;
		} else if (sdwc->chg_type == SDP_TYPE ||
				   sdwc->chg_type == CDP_TYPE) {
			dev_info(sdwc->dev, "charge detect finished with %d\n",
								sdwc->chg_type);
			sdwc->hs_phy->last_event = USB_EVENT_ENUMERATED;
			sdwc->ss_phy->last_event = USB_EVENT_ENUMERATED;
			queue_work(sdwc->dwc3_wq, &sdwc->evt_prepare_work);
		} else {
			dev_info(sdwc->dev, "charge detect finished\n");
			sdwc->hs_phy->last_event = USB_EVENT_CHARGER;
			sdwc->ss_phy->last_event = USB_EVENT_CHARGER;
			sdwc->charging_mode = true;
		}
		break;
	default:
		break;
	}

	spin_unlock_irqrestore(&sdwc->lock, flags);

	if (rework)
		queue_delayed_work(sdwc->sm_usb_wq, &sdwc->chg_detect_work, delay);
}

static int dwc3_host_prepare(struct device *dev);
static int dwc3_core_prepare(struct device *dev);

static void dwc3_sprd_override_pm_ops(struct device *dev, struct dev_pm_ops *pm_ops,
				bool is_host)
{
	if (!dev->driver)
		return;

	(*pm_ops) = (*dev->driver->pm);
	pm_ops->prepare = is_host ? dwc3_host_prepare : dwc3_core_prepare;
	dev->driver->pm = pm_ops;
}

/**
 * dwc3_sprd_otg_start_peripheral -  bind/unbind the peripheral controller.
 *
 * @mdwc: Pointer to the dwc3_sprd structure.
 * @on:   Turn ON/OFF the gadget.
 *
 * Returns 0 on success otherwise negative errno.
 */
static int dwc3_sprd_otg_start_peripheral(struct dwc3_sprd *sdwc, int on)
{
	struct dwc3 *dwc = platform_get_drvdata(sdwc->dwc3);

	if (on) {
		dev_info(sdwc->dev, "%s: turn on gadget %s, keep-awake in peripheral\n",
					__func__, dwc->gadget->name);

		__pm_stay_awake(sdwc->wake_lock);
		usb_phy_vbus_off(sdwc->ss_phy);
		pm_runtime_get_sync(dwc->dev);
		/* phy set vbus connected after phy_init*/
		usb_phy_notify_connect(sdwc->ss_phy, 0);
		usb_role_switch_set_role(dwc->role_sw, USB_ROLE_DEVICE);
		if (dwc->dr_mode == USB_DR_MODE_OTG)
			flush_work(&dwc->drd_work);
		usb_udc_vbus_handler(dwc->gadget, true);
		usb_gadget_set_state(dwc->gadget, USB_STATE_ATTACHED);
		/* 1. modify usb speed to fs while cali mode *
		 * 2. if HW didn't have ptn38003a, we should *
		 * limit usb speed to 3.0		     *
		 */
		adjust_dwc3_max_speed(sdwc);

		sdwc->glue_dr_mode = USB_DR_MODE_PERIPHERAL;
	} else {
		struct sprd_dwc3_usb_udc *sprd_udc;
		dev_info(sdwc->dev, "%s: turn off gadget %s\n",
					__func__, dwc->gadget->name);

		__pm_relax(sdwc->wake_lock);
		/* phy set vbus disconnected */
		usb_phy_notify_disconnect(sdwc->ss_phy, 0);
		/* dwc3 has enough get a disconnect irq*/
		msleep(20);
		dev_info(sdwc->dev, "dwc->connected %d\n", dwc->connected);

		dwc3_flush_all_events(sdwc);

		sprd_udc = (struct sprd_dwc3_usb_udc *)dwc->gadget->udc;
		if (sprd_udc)
			sprd_udc->vbus = false;
		dwc->gadget->ops->pullup(dwc->gadget, 0);

		usb_role_switch_set_role(dwc->role_sw, USB_ROLE_DEVICE);
		/*dp/dm change to others*/
		if (sdwc->use_pdhub_c2c)
			call_sprd_usbphy_event_notifiers(SPRD_USBPHY_EVENT_TYPEC,
					false, NULL);
		usb_gadget_set_state(dwc->gadget, USB_STATE_NOTATTACHED);
		pm_runtime_put_sync(dwc->dev);
		sdwc->glue_dr_mode = USB_DR_MODE_UNKNOWN;
	}

	return 0;
}

/**
 * dwc3_sprd_otg_start_host -  helper function for starting/stoping the host
 * controller driver.
 *
 * @mdwc: Pointer to the dwc3_sprd structure.
 * @on: start / stop the host controller driver.
 *
 * Returns 0 on success otherwise negative errno.
 */
static int dwc3_sprd_otg_start_host(struct dwc3_sprd *sdwc, int on)
{
	int ret;
	struct dwc3 *dwc = platform_get_drvdata(sdwc->dwc3);

	if (!sdwc->vbus) {
		sdwc->vbus = devm_regulator_get(sdwc->dev, "vbus");
		if (IS_ERR_OR_NULL(sdwc->vbus)) {
			if (!sdwc->vbus)
				return -EPERM;
			else
				return -EPROBE_DEFER;
		}
	}

	if (on) {
		dev_info(sdwc->dev, "%s: turn on host\n", __func__);
		if (!sdwc->use_pdhub_c2c) {
			if (!regulator_is_enabled(sdwc->vbus)) {
				ret = regulator_enable(sdwc->vbus);
				if (ret) {
					dev_err(sdwc->dev,
						"Failed to enable vbus: %d\n", ret);
					return ret;
				}
			}
		}

		usb_phy_vbus_on(sdwc->ss_phy);
		pm_runtime_get_sync(dwc->dev);
		usb_role_switch_set_role(dwc->role_sw, USB_ROLE_HOST);
		if (dwc->dr_mode == USB_DR_MODE_OTG)
			flush_work(&dwc->drd_work);
#if IS_ENABLED(CONFIG_SPRD_REDRIVER_PTN38003A)
		adjust_dwc3_max_speed(sdwc);
#endif

		dwc3_sprd_override_pm_ops(&dwc->xhci->dev, &sdwc->xhci_pm_ops, true);
		sdwc->glue_dr_mode = USB_DR_MODE_HOST;
	} else {
		dev_info(sdwc->dev, "%s: turn off host\n", __func__);
		if (!sdwc->use_pdhub_c2c) {
			if (regulator_is_enabled(sdwc->vbus)) {
				ret = regulator_disable(sdwc->vbus);
				if (ret)
					dev_err(sdwc->dev,
						"Failed to disable vbus: %d\n", ret);
			}
		}

		usb_role_switch_set_role(dwc->role_sw, USB_ROLE_DEVICE);
		flush_work(&dwc->drd_work);
		usb_phy_vbus_off(sdwc->ss_phy);
		/*dp/dm change to others*/
		if (sdwc->use_pdhub_c2c)
			call_sprd_usbphy_event_notifiers(SPRD_USBPHY_EVENT_TYPEC,
					false, NULL);
		pm_runtime_put_sync(dwc->dev);

		sdwc->glue_dr_mode = USB_DR_MODE_UNKNOWN;
	}

	return 0;
}

/**
 * dwc3_ext_event_notify - callback to handle events from external transceiver
 *
 * Returns 0 on success
 */
static void dwc3_sprd_ext_event_notify(struct dwc3_sprd *sdwc)
{
	unsigned long flags;
	/* Flush processing any pending events before handling new ones */
	flush_delayed_work(&sdwc->hotplug_sm_work);

	spin_lock_irqsave(&sdwc->lock, flags);
	dev_info(sdwc->dev,
			"ext event: id %d, vbus %d, b_susp %d, a_recover %d, a_audio %d\n",
			sdwc->id_state, sdwc->vbus_active,
			sdwc->gadget_suspended, sdwc->host_recover,
			sdwc->is_audio_dev);

	if (sdwc->id_state == DWC3_ID_FLOAT)
		set_bit(ID, &sdwc->inputs);
	else
		clear_bit(ID, &sdwc->inputs);

	if (sdwc->vbus_active && !sdwc->in_restart)
		set_bit(B_SESS_VLD, &sdwc->inputs);
	else
		clear_bit(B_SESS_VLD, &sdwc->inputs);

	if (sdwc->gadget_suspended)
		set_bit(B_SUSPEND, &sdwc->inputs);
	else
		clear_bit(B_SUSPEND, &sdwc->inputs);

	if (sdwc->is_audio_dev)
		set_bit(A_AUDIO, &sdwc->inputs);
	else
		clear_bit(A_AUDIO, &sdwc->inputs);

	if (sdwc->host_recover) {
		set_bit(A_RECOVER, &sdwc->inputs);
		sdwc->host_recover = false;
	}
	spin_unlock_irqrestore(&sdwc->lock, flags);

	queue_delayed_work(sdwc->sm_usb_wq, &sdwc->hotplug_sm_work, 0);
}

static void dwc3_sprd_evt_prepare_work(struct work_struct *work)
{
	struct dwc3_sprd *sdwc =
		container_of(work, struct dwc3_sprd, evt_prepare_work);
	unsigned long flags;

	dev_dbg(sdwc->dev, "%s enter\n", __func__);

	spin_lock_irqsave(&sdwc->lock, flags);
	if (atomic_read(&sdwc->pm_suspended)) {
		/*
		 * delay start hotplug_sm_work in pm suspend state
		 * musb_sprd_pm_resume will kick the state machine later.
		 */
		spin_unlock_irqrestore(&sdwc->lock, flags);
		dev_info(sdwc->dev, "delay start hotplug_sm_work in pm suspend state\n");
		return;
	}

	if (sdwc->vbus_active) {
		if (sdwc->chg_state != USB_CHG_STATE_DETECTED &&
			sdwc->chg_state != USB_CHG_STATE_RETRY_DETECTED) {
			spin_unlock_irqrestore(&sdwc->lock, flags);
			dev_info(sdwc->dev, "vbus charger detect not finished\n");
			return;
		}
	}

	if (sdwc->charging_mode || boot_charging) {
		spin_unlock_irqrestore(&sdwc->lock, flags);
		dev_info(sdwc->dev, "don't need start hotplug_sm_work in charging mode\n");
		if (sdwc->chg_type == SDP_TYPE)
			usb_phy_set_charger_current(sdwc->ss_phy, 500);
		return;
	}
	spin_unlock_irqrestore(&sdwc->lock, flags);

	dwc3_sprd_ext_event_notify(sdwc);
}

static int dwc3_sprd_vbus_notifier(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct dwc3_sprd *sdwc = container_of(nb, struct dwc3_sprd, vbus_nb);
	unsigned long flags;

	/* In usb audio mode, we turn off dwc3, but still keep the vbus on.
	 * It should income invalid vbus notifier, filter them
	 */
	spin_lock_irqsave(&sdwc->lock, flags);
	if (sdwc->is_audio_dev) {
		spin_unlock_irqrestore(&sdwc->lock, flags);
		dev_info(sdwc->dev, "ignore vbus state in audio dev mode.\n");
		return NOTIFY_DONE;
	}

	if (sdwc->id_state == DWC3_ID_GROUND) {
		spin_unlock_irqrestore(&sdwc->lock, flags);
		dev_info(sdwc->dev, "ignore vbus state in id ground mode.\n");
		return NOTIFY_DONE;
	}

	if (sdwc->vbus_active == event) {
		spin_unlock_irqrestore(&sdwc->lock, flags);
		dev_info(sdwc->dev, "ignore repeated vbus active event.\n");
		return NOTIFY_DONE;
	}

	if (sdwc->use_pdhub_c2c && sc27xx_get_dr_swap_executing() &&
		!sc27xx_get_current_status_detach_or_attach()) {
		spin_unlock_irqrestore(&sdwc->lock, flags);
		dev_info(sdwc->dev, "ignore dr_swap: vbus, typec has been out.\n");
		return NOTIFY_DONE;
	}

	dev_info(sdwc->dev, "vbus:%ld event received\n", event);

	sdwc->vbus_active = event;

	if (sdwc->vbus_active && sdwc->chg_state == USB_CHG_STATE_UNDETECT) {
		if (sdwc->use_pdhub_c2c && sc27xx_get_dr_swap_executing()) {
			sdwc->chg_state = USB_CHG_STATE_DETECTED;
		} else {
			sdwc->hs_phy->last_event = USB_EVENT_VBUS;
			sdwc->ss_phy->last_event = USB_EVENT_VBUS;
			spin_unlock_irqrestore(&sdwc->lock, flags);
			queue_delayed_work(sdwc->sm_usb_wq, &sdwc->chg_detect_work, 0);
			return NOTIFY_DONE;
		}
	}

	if (!sdwc->vbus_active) {
		sdwc->hs_phy->last_event = USB_EVENT_NONE;
		sdwc->ss_phy->last_event = USB_EVENT_NONE;
		spin_unlock_irqrestore(&sdwc->lock, flags);
		flush_delayed_work(&sdwc->chg_detect_work);
		spin_lock_irqsave(&sdwc->lock, flags);
		sdwc->chg_state = USB_CHG_STATE_UNDETECT;
		sdwc->charging_mode = false;
		sdwc->wait_chg_detect_count = 0;
		sdwc->retry_chg_detect_count = 0;
	}

	spin_unlock_irqrestore(&sdwc->lock, flags);
	queue_work(sdwc->dwc3_wq, &sdwc->evt_prepare_work);

	return NOTIFY_DONE;
}

static int dwc3_sprd_id_notifier(struct notifier_block *nb,
				 unsigned long event, void *data)
{
	struct dwc3_sprd *sdwc = container_of(nb, struct dwc3_sprd, id_nb);
	enum dwc3_id_state id;
	unsigned long flags;

	id = event ? DWC3_ID_GROUND : DWC3_ID_FLOAT;

	spin_lock_irqsave(&sdwc->lock, flags);
	if (sdwc->id_state == id) {
		spin_unlock_irqrestore(&sdwc->lock, flags);
		return NOTIFY_DONE;
	}

	if (sdwc->use_pdhub_c2c && sc27xx_get_dr_swap_executing() &&
		!sc27xx_get_current_status_detach_or_attach()) {
		spin_unlock_irqrestore(&sdwc->lock, flags);
		dev_info(sdwc->dev, "ignore dr_swap: id, typec has been out.\n");
		return NOTIFY_DONE;
	}

	dev_info(sdwc->dev, "host:%ld (id:%d) event received\n", event, id);

	sdwc->id_state = id;

	if (sdwc->id_state == DWC3_ID_GROUND) {
		sdwc->hs_phy->last_event = USB_EVENT_ID;
		sdwc->ss_phy->last_event = USB_EVENT_ID;
	} else {
		if (sdwc->use_pdhub_c2c && sc27xx_get_dr_swap_executing()) {
			sdwc->hs_phy->last_event = USB_EVENT_ID;
			sdwc->ss_phy->last_event = USB_EVENT_ID;
		} else {
			sdwc->hs_phy->last_event = USB_EVENT_NONE;
			sdwc->ss_phy->last_event = USB_EVENT_NONE;
		}
		if (sdwc->is_audio_dev) {
			sdwc->is_audio_dev = false;
			/* notify musb to stop */
			call_sprd_usbm_event_notifiers(SPRD_USBM_EVENT_HOST_MUSB, false, NULL);
		}
	}

	sdwc->chg_state = USB_CHG_STATE_UNDETECT;
	sdwc->charging_mode = false;
	sdwc->retry_chg_detect_count = 0;
	spin_unlock_irqrestore(&sdwc->lock, flags);
	queue_work(sdwc->dwc3_wq, &sdwc->evt_prepare_work);
	return NOTIFY_DONE;
}

static void dwc3_sprd_detect_cable(struct dwc3_sprd *sdwc)
{
	unsigned long flags;
	struct extcon_dev *id_ext = sdwc->id_edev ? sdwc->id_edev : sdwc->edev;

	spin_lock_irqsave(&sdwc->lock, flags);
	if (extcon_get_state(id_ext, EXTCON_USB_HOST) == true) {
		dev_info(sdwc->dev, "host connection detected from ID GPIO.\n");
		sdwc->id_state = DWC3_ID_GROUND;
		sdwc->hs_phy->last_event = USB_EVENT_ID;
		sdwc->ss_phy->last_event = USB_EVENT_ID;
		queue_work(sdwc->dwc3_wq, &sdwc->evt_prepare_work);
	} else if (extcon_get_state(sdwc->edev, EXTCON_USB) == true) {
		dev_info(sdwc->dev, "device connection detected from VBUS GPIO.\n");
		sdwc->vbus_active = true;
		sdwc->hs_phy->last_event = USB_EVENT_VBUS;
		sdwc->ss_phy->last_event = USB_EVENT_VBUS;
		if (sdwc->vbus_active &&
			sdwc->chg_state == USB_CHG_STATE_UNDETECT) {
			queue_delayed_work(sdwc->sm_usb_wq,
					   &sdwc->chg_detect_work,
					   0);
			spin_unlock_irqrestore(&sdwc->lock, flags);
			return;
		}
		queue_work(sdwc->dwc3_wq, &sdwc->evt_prepare_work);
	}
	spin_unlock_irqrestore(&sdwc->lock, flags);
}

static int dwc3_sprd_audio_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct dwc3_sprd *sdwc = container_of(nb, struct dwc3_sprd, audio_nb);
	unsigned long flags;

	dev_info(sdwc->dev, "audio:%ld event received\n", event);

	/* dwc3 only need to proccess "false" event */
	if (!event) {
		pm_wakeup_event(sdwc->dev, 500);
		spin_lock_irqsave(&sdwc->lock, flags);
		sdwc->is_audio_dev = true;
		spin_unlock_irqrestore(&sdwc->lock, flags);

		/* kick dwc3 work for audio dev */
		queue_work(sdwc->dwc3_wq, &sdwc->evt_prepare_work);
	}
	return NOTIFY_DONE;
}

static int dwc3_sprd_clk_probe(struct device *dev, struct dwc3_sprd *sdwc)
{
	int ret = 0;

	sdwc->ipa_dpu1_clk = devm_clk_get(dev, "ipa_dpu1_clk");
	if (IS_ERR(sdwc->ipa_dpu1_clk)) {
		dev_err(dev, "no dpu1 clk specified\n");
		return PTR_ERR(sdwc->ipa_dpu1_clk);
	} else {
		ret = clk_prepare_enable(sdwc->ipa_dpu1_clk);
		if (ret)
			dev_err(dev, "ipa-dpu1-clock enable failed\n");
	}

	sdwc->ipa_dptx_clk = devm_clk_get(dev, "ipa_dptx_clk");
	if (IS_ERR(sdwc->ipa_dptx_clk)) {
		dev_err(dev, "no dptx clk specified\n");
		return PTR_ERR(sdwc->ipa_dptx_clk);
	} else {
		ret = clk_prepare_enable(sdwc->ipa_dptx_clk);
		if (ret)
			dev_err(dev, "ipa-dptx-clock enable failed\n");
	}

	sdwc->ipa_tca_clk = devm_clk_get(dev, "ipa_tca_clk");
	if (IS_ERR(sdwc->ipa_tca_clk)) {
		dev_err(dev, "no tca clk specified\n");
		return PTR_ERR(sdwc->ipa_tca_clk);
	} else {
		ret = clk_prepare_enable(sdwc->ipa_tca_clk);
		if (ret)
			dev_err(dev, "ipa-tca-clock enable failed\n");
	}

	sdwc->ipa_usb31pll_clk = devm_clk_get(dev, "ipa_usb31pll_clk");
	if (IS_ERR(sdwc->ipa_usb31pll_clk)) {
		dev_err(dev, "no usb31pll clk specified\n");
		return PTR_ERR(sdwc->ipa_usb31pll_clk);
	} else {
		ret = clk_prepare_enable(sdwc->ipa_usb31pll_clk);
		if (ret)
			dev_err(dev, "ipa-usb31pll-clock enable failed\n");
	}

	return ret;
}

static int usb_clk_prepare_enable(struct dwc3_sprd *sdwc)
{
	if (clk_prepare_enable(sdwc->ipa_dpu1_clk))
		dev_err(sdwc->dev, "ipa dpu1 clk enable error.\n");
	if (clk_prepare_enable(sdwc->ipa_dptx_clk))
		dev_err(sdwc->dev, "ipa dptx clk enable error.\n");
	if (clk_prepare_enable(sdwc->ipa_tca_clk))
		dev_err(sdwc->dev, "ipa tca clk enable error.\n");
	if (clk_prepare_enable(sdwc->ipa_usb31pll_clk))
		dev_err(sdwc->dev, "ipa usb31pll clk enable error.\n");
	return 0;
}

static int usb_clk_prepare_disable(struct dwc3_sprd *sdwc)
{

	clk_disable_unprepare(sdwc->ipa_dpu1_clk);
	clk_disable_unprepare(sdwc->ipa_dptx_clk);
	clk_disable_unprepare(sdwc->ipa_tca_clk);
	clk_disable_unprepare(sdwc->ipa_usb31pll_clk);
	return 0;
}

/**
 * dwc3_sprd_hotplug_sm_work - workqueue function.
 *
 * @w: Pointer to the dwc3 otg workqueue
 *
 * NOTE: After any change in drd_state, we must reschdule the state machine.
 */
static void dwc3_sprd_hotplug_sm_work(struct work_struct *work)
{
	struct dwc3_sprd *sdwc =
		container_of(work, struct dwc3_sprd, hotplug_sm_work.work);
	struct dwc3 *dwc = NULL;
	bool rework = false;
	int ret = 0;
	unsigned long delay = 0;
	const char *state;

	if (sdwc->dwc3)
		dwc = platform_get_drvdata(sdwc->dwc3);

	if (!dwc) {
		dev_err(sdwc->dev, "dwc is NULL.\n");
		return;
	}

	state = dwc3_drd_state_string(sdwc->drd_state);
	dev_info(sdwc->dev, "%s state\n", state);

	/* Check OTG state */
	switch (sdwc->drd_state) {
	case DRD_STATE_UNDEFINED:
		dwc3_sprd_override_pm_ops(dwc->dev, &sdwc->dwc3_pm_ops, false);
		/* enable dwc3 core runtime */
		pm_runtime_allow(dwc->dev);

		pm_runtime_set_active(sdwc->dev);
		pm_runtime_use_autosuspend(sdwc->dev);
		pm_runtime_set_autosuspend_delay(sdwc->dev,
						 DWC3_AUTOSUSPEND_DELAY);
		pm_runtime_enable(sdwc->dev);
		pm_runtime_get_noresume(sdwc->dev);
		pm_runtime_mark_last_busy(sdwc->dev);
		pm_runtime_put_autosuspend(sdwc->dev);

		device_init_wakeup(sdwc->dev, true);
		/* put controller and phy in suspend if no cable connected */
		if (test_bit(ID, &sdwc->inputs) &&
				!test_bit(B_SESS_VLD, &sdwc->inputs)) {
			dwc3_sprd_detect_cable(sdwc);
			sdwc->drd_state = DRD_STATE_IDLE;
			break;
		}

		dev_dbg(sdwc->dev, "Exit UNDEF");
		sdwc->drd_state = DRD_STATE_IDLE;
		fallthrough;
	case DRD_STATE_IDLE:
		/* if /sys/class/usb_notify/usb_control/usb_data_enabled is
		 * 0, don't setup usb connection */
		if (!sdwc->usb_data_enabled) {
			dev_info(sdwc->dev, "usb_data_enabled = 0, wait\n");
			rework = true;
			delay = DWC3_USB_ENABLE_CHECK_DELAY;
			break;
		}

		if (!test_bit(ID, &sdwc->inputs)) {
			dev_dbg(sdwc->dev, "!id\n");
			if (!pm_runtime_suspended(dwc->dev)) {
				dev_info(sdwc->dev, "waiting dwc3 suspended\n");
				rework = true;
				delay = DWC3_RUNTIME_CHECK_DELAY;
			} else {
				sdwc->drd_state = DRD_STATE_HOST_IDLE;
				rework = true;
			}
		} else if (test_bit(B_SESS_VLD, &sdwc->inputs)) {
			dev_dbg(sdwc->dev, "b_sess_vld\n");
			/*
			 * The follow ensure that UDC be setted as 25100000.dwc3
			 * when phone startup with hub plug in. Or UDC would be
			 * setted as musb_hdrc.1.auto
			 */
			if (!dwc3_sprd_is_udc_start(sdwc)) {
				dev_info(sdwc->dev, "waiting dwc3 udc start\n");
				rework = true;
				delay = DWC3_UDC_START_CHECK_DELAY;
				break;
			} else if (!pm_runtime_suspended(dwc->dev)) {
				dev_info(sdwc->dev, "waiting dwc3 suspended\n");
				rework = true;
				delay = DWC3_RUNTIME_CHECK_DELAY;
			} else {
				/*
				 * Increment pm usage count upon cable connect. Count
				 * is decremented in DRD_STATE_PERIPHERAL state on
				 * cable disconnect or in bus suspend.
				 */
				pm_runtime_get_sync(sdwc->dev);
				if (sdwc->use_pdhub_c2c)
					call_sprd_usbphy_event_notifiers(SPRD_USBPHY_EVENT_TYPEC,
						true, NULL);
				dwc3_sprd_otg_start_peripheral(sdwc, 1);
				sdwc->drd_state = DRD_STATE_PERIPHERAL;
				rework = true;
			}
		} else {
			dev_dbg(sdwc->dev, "Cable disconnected\n");
		}
		break;
	case DRD_STATE_PERIPHERAL:
		if (!test_bit(B_SESS_VLD, &sdwc->inputs) ||
				!test_bit(ID, &sdwc->inputs)) {
			dev_dbg(sdwc->dev, "!id || !bsv\n");
			sdwc->drd_state = DRD_STATE_IDLE;
			dwc3_sprd_otg_start_peripheral(sdwc, 0);
			/*
			 * Decrement pm usage count upon cable disconnect
			 * which was incremented upon cable connect in
			 * DRD_STATE_IDLE state
			 */
			pm_runtime_put_sync(sdwc->dev);
			rework = true;
		} else if (0 == sdwc->usb_data_enabled) {
			dev_info(sdwc->dev, "usb_data_enabled == 0");
			sdwc->drd_state = DRD_STATE_IDLE;
			dwc3_sprd_otg_start_peripheral(sdwc, 0);
			/*
			 * Decrement pm usage count upon cable disconnect
			 * which was incremented upon cable connect in
			 * DRD_STATE_IDLE state
			 */
			pm_runtime_put_sync(sdwc->dev);
			rework = true;
		} else if (test_bit(B_SUSPEND, &sdwc->inputs) &&
			test_bit(B_SESS_VLD, &sdwc->inputs)) {
			dev_dbg(sdwc->dev, "BPER bsv && susp\n");
			sdwc->drd_state = DRD_STATE_PERIPHERAL_SUSPEND;
			/*
			 * Decrement pm usage count upon bus suspend.
			 * Count was incremented either upon cable
			 * connect in DRD_STATE_IDLE or host
			 * initiated resume after bus suspend in
			 * DRD_STATE_PERIPHERAL_SUSPEND state
			 */
			pm_runtime_mark_last_busy(sdwc->dev);
			pm_runtime_put_autosuspend(sdwc->dev);
		}
		break;
	case DRD_STATE_PERIPHERAL_SUSPEND:
		if (!test_bit(B_SESS_VLD, &sdwc->inputs) ||
				!test_bit(ID, &sdwc->inputs)) {
			dev_dbg(sdwc->dev, "BSUSP: !id || !bsv\n");
			sdwc->drd_state = DRD_STATE_IDLE;
			dwc3_sprd_otg_start_peripheral(sdwc, 0);
		} else if (!test_bit(B_SUSPEND, &sdwc->inputs)) {
			dev_dbg(sdwc->dev, "BSUSP !susp\n");
			sdwc->drd_state = DRD_STATE_PERIPHERAL;
			/*
			 * Increment pm usage count upon host
			 * initiated resume. Count was decremented
			 * upon bus suspend in
			 * DRD_STATE_PERIPHERAL state.
			 */
			pm_runtime_get_sync(sdwc->dev);
		}
		break;
	case DRD_STATE_HOST_IDLE:
		/* Switch to A-Device*/
		if (test_bit(ID, &sdwc->inputs)) {
			dev_dbg(sdwc->dev, "id\n");
			sdwc->drd_state = DRD_STATE_IDLE;
			sdwc->start_host_retry_count = 0;
			rework = true;
		} else {
			/*switch dpdm to phy*/
			if (sdwc->use_pdhub_c2c)
				call_sprd_usbphy_event_notifiers(SPRD_USBPHY_EVENT_TYPEC,
					true, NULL);
			ret = dwc3_sprd_otg_start_host(sdwc, 1);
			if ((ret == -EPROBE_DEFER) &&
				sdwc->start_host_retry_count < 3) {
				/*
				 * Get regulator failed as regulator driver is
				 * not up yet. Will try to start host after 1sec
				 */
				dev_dbg(sdwc->dev, "Unable to get vbus regulator. Retrying...\n");
				delay = VBUS_REG_CHECK_DELAY;
				rework = true;
				sdwc->start_host_retry_count++;
			} else if (ret) {
				dev_err(sdwc->dev, "unable to start host\n");
				goto ret;
			} else {
				sdwc->drd_state = DRD_STATE_HOST;
			}
		}
		break;
	case DRD_STATE_HOST:
		if (test_bit(ID, &sdwc->inputs)) {
			dev_dbg(sdwc->dev, "id\n");
			dwc3_sprd_otg_start_host(sdwc, 0);
			sdwc->drd_state = DRD_STATE_IDLE;
			sdwc->start_host_retry_count = 0;
			rework = true;
		} else if (test_bit(A_AUDIO, &sdwc->inputs)) {
			unsigned long flags;

			dev_dbg(sdwc->dev, "A_AUDIO\n");
			dwc3_sprd_otg_start_host(sdwc, 0);

			/* start musb */
			spin_lock_irqsave(&sdwc->lock, flags);
			/* to make sure id is still grounded */
			if (sdwc->id_state == DWC3_ID_GROUND)
				call_sprd_usbm_event_notifiers(SPRD_USBM_EVENT_HOST_MUSB,
											true, NULL);
			spin_unlock_irqrestore(&sdwc->lock, flags);

			sdwc->drd_state = DRD_STATE_HOST_AUDIO;
			sdwc->start_host_retry_count = 0;
			rework = true;
		} else {
			dev_dbg(sdwc->dev, "still in a_host state. Resuming root hub.\n");
			if (dwc)
				pm_runtime_resume(&dwc->xhci->dev);
		}
		break;
	case DRD_STATE_HOST_AUDIO:
		if (test_bit(ID, &sdwc->inputs) ||
			!test_bit(A_AUDIO, &sdwc->inputs)) {
			sdwc->drd_state = DRD_STATE_IDLE;
			rework = true;
			sdwc->glue_dr_mode = USB_DR_MODE_UNKNOWN;
			dev_dbg(sdwc->dev, "A_AUDIO exit\n");
		}
		break;
	default:
		dev_err(sdwc->dev, "%s: invalid otg-state\n", __func__);

	}

	if (rework)
		queue_delayed_work(sdwc->sm_usb_wq, &sdwc->hotplug_sm_work, delay);

ret:
	return;
}

int dwc3_sprd_probe_finish(void)
{
	return dwc3_probe_finish;
}
EXPORT_SYMBOL_GPL(dwc3_sprd_probe_finish);

static irqreturn_t dwc3_sprd_pme_wakeup_irq(int irq, void *data)
{
	struct dwc3_sprd *sdwc = data;
	struct dwc3 *dwc;

	dwc = platform_get_drvdata(sdwc->dwc3);
	dev_info(sdwc->dev, "usb3 pme wakeup came.\n");
	return IRQ_HANDLED;
}

static int dwc3_sprd_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node, *dwc3_node;

	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	struct device *dev = &pdev->dev;
	struct dwc3_sprd *sdwc;
	struct dwc3 *dwc;
	const char *usb_mode;
	int ret;

	if (!node) {
		dev_err(dev, "can not find device node\n");
		return -ENODEV;
	}

	sdwc = devm_kzalloc(dev, sizeof(*sdwc), GFP_KERNEL);
	if (!sdwc)
		return -ENOMEM;

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");
	if (!regmap_np) {
		dev_info(dev, "unable to get syscon node\n");
	} else {
		regmap_pdev = of_find_device_by_node(regmap_np);
		if (!regmap_pdev) {
			of_node_put(regmap_np);
			dev_warn(dev, "unable to get syscon platform device\n");
			sdwc->pmic = NULL;
		} else {
			sdwc->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
			if (!sdwc->pmic)
				dev_warn(dev, "unable to get pmic regmap device\n");
		}
	}

	sdwc->pmu_apb = syscon_regmap_lookup_by_phandle(dev->of_node,
							"sprd,syscon-pmu-apb");
	if (IS_ERR(sdwc->pmu_apb)) {
		dev_err(dev, "failed to map pmu apb registers (via syscon)\n");
		sdwc->pmu_apb = NULL;
	}

	sdwc->ipa_apb = syscon_regmap_lookup_by_phandle(dev->of_node,
						       "sprd,syscon-ipa-apb");
	if (IS_ERR(sdwc->ipa_apb)) {
		dev_err(dev, "failed to map ipa apb registers (via syscon)\n");
		sdwc->ipa_apb = NULL;
	}

	sdwc->ipa_dispc1_glb_apb = syscon_regmap_lookup_by_phandle(dev->of_node,
						       "sprd,syscon-ipa-dispc1-glb-apb");
	if (IS_ERR(sdwc->ipa_apb)) {
		dev_err(dev, "failed to map ipa dispc1 registers (via syscon)\n");
		sdwc->ipa_dispc1_glb_apb = NULL;
	}

	ret = dma_coerce_mask_and_coherent(dev, DMA_BIT_MASK(BITS_PER_LONG));
	if (ret)
		return ret;

	dwc3_node = of_get_next_available_child(node, NULL);
	if (!dwc3_node) {
		dev_err(dev, "failed to find dwc3 child\n");
		return PTR_ERR(dwc3_node);
	}

	sdwc->dwc3_wq = alloc_ordered_workqueue("dwc3_wq", 0);
	if (!sdwc->dwc3_wq) {
		pr_err("%s: Unable to create workqueue dwc3_wq\n", __func__);
		return -ENOMEM;
	}

	/*
	 * Create an ordered freezable workqueue for hotplug so that it gets
	 * scheduled only after pm_resume has happened completely. This helps
	 * in avoiding race conditions between xhci_plat_resume and
	 * xhci_runtime_resume and also between hcd disconnect and xhci_resume.
	 */
	sdwc->sm_usb_wq = alloc_ordered_workqueue("k_sm_usb", WQ_FREEZABLE);
	if (!sdwc->sm_usb_wq) {
		destroy_workqueue(sdwc->dwc3_wq);
		return -ENOMEM;
	}

	if (dwc3_sprd_clk_probe(dev, sdwc))
		goto err_ipa_clk;

	sdwc->hs_phy = devm_usb_get_phy_by_phandle(dev,
			"usb-phy", 0);
	if (IS_ERR(sdwc->hs_phy)) {
		dev_err(dev, "unable to get usb2.0 phy device\n");
		ret = PTR_ERR(sdwc->hs_phy);
		goto err_ipa_clk;
	}
	sdwc->ss_phy = devm_usb_get_phy_by_phandle(dev,
			"usb-phy", 1);
	if (IS_ERR(sdwc->ss_phy)) {
		dev_err(dev, "unable to get usb3.0 phy device\n");
		ret = PTR_ERR(sdwc->ss_phy);
		goto err_ipa_clk;
	}

	if (IS_ENABLED(CONFIG_USB_DWC3_DUAL_ROLE) ||
		IS_ENABLED(CONFIG_USB_DWC3_HOST)) {
		sdwc->vbus = devm_regulator_get(dev, "vbus");
		if (IS_ERR(sdwc->vbus)) {
			dev_warn(dev, "unable to get vbus supply\n");
			sdwc->vbus = NULL;
		}
	}

	sdwc->ipa_usb_ref_clk = devm_clk_get(dev, "ipa_usb_ref");
	if (IS_ERR(sdwc->ipa_usb_ref_clk)) {
		dev_warn(dev, "usb3 can't get the ipa usb ref clock\n");
		sdwc->ipa_usb_ref_clk = NULL;
	}

	sdwc->ipa_usb_ref_parent = devm_clk_get(dev, "ipa_usb_ref_source");
	if (IS_ERR(sdwc->ipa_usb_ref_parent)) {
		dev_warn(dev, "usb can't get the ipa usb ref source\n");
		sdwc->ipa_usb_ref_parent = NULL;
	}

	sdwc->ipa_usb_ref_default = devm_clk_get(dev, "ipa_usb_ref_default");
	if (IS_ERR(sdwc->ipa_usb_ref_default)) {
		dev_warn(dev, "usb can't get the ipa usb ref default\n");
		sdwc->ipa_usb_ref_default = NULL;
	}

	if (sdwc->ipa_usb_ref_clk && sdwc->ipa_usb_ref_parent)
		clk_set_parent(sdwc->ipa_usb_ref_clk, sdwc->ipa_usb_ref_parent);

	/* perpare clock */
	sdwc->core_clk = devm_clk_get(dev, "core_clk");
	if (IS_ERR(sdwc->core_clk)) {
		dev_warn(dev, "no core clk specified\n");
		sdwc->core_clk = NULL;
	} else {
		ret = clk_prepare_enable(sdwc->core_clk);
		if (ret) {
			dev_err(dev, "core-clock enable failed\n");
			goto err_ipa_clk;
		}
	}
	sdwc->ref_clk = devm_clk_get(dev, "ref_clk");
	if (IS_ERR(sdwc->ref_clk)) {
		dev_warn(dev, "no ref clk specified\n");
		sdwc->ref_clk = NULL;
	} else {
		ret = clk_prepare_enable(sdwc->ref_clk);
		if (ret) {
			dev_err(dev, "ref-clock enable failed\n");
			goto err_core_clk;
		}
	}
	sdwc->susp_clk = devm_clk_get(dev, "susp_clk");
	if (IS_ERR(sdwc->susp_clk)) {
		dev_warn(dev, "no suspend clk specified\n");
		sdwc->susp_clk = NULL;
	} else {
		ret = clk_prepare_enable(sdwc->susp_clk);
		if (ret) {
			dev_err(dev, "suspend-clock enable failed\n");
			goto err_ref_clk;
		}
	}

	if (IS_ENABLED(CONFIG_USB_DWC3_GADGET))
		usb_mode = "PERIPHERAL";
	else if (IS_ENABLED(CONFIG_USB_DWC3_HOST))
		usb_mode = "HOST";
	else
		usb_mode = "DRD";

	usb_phy_init(sdwc->hs_phy);
	usb_phy_init(sdwc->ss_phy);

	ret = devm_of_platform_populate(&pdev->dev);
	if (ret) {
		dev_err(dev, "failed to add create dwc3 core\n");
		goto err_susp_clk;
	}

	sdwc->dwc3 = of_find_device_by_node(dwc3_node);
	of_node_put(dwc3_node);
	if (!sdwc->dwc3) {
		dev_err(dev, "failed to get dwc3 platform device\n");
		ret = PTR_ERR(sdwc->dwc3);
		goto err_susp_clk;
	}

	dwc = platform_get_drvdata(sdwc->dwc3);
	if (!dwc) {
		dev_err(dev, "failed to add create dwc3 core ,try again\n");
		ret = -EPROBE_DEFER;
		goto err_susp_clk;
	}

	dwc->glue_phy = sdwc->ss_phy;
	sdwc->usb_data_enabled = true;
	ret = dwc3_sprd_usb_notify_init(pdev, sdwc);
	if (ret) {
		dev_err(dev, "usb_notify_init err %d \n", ret);
	}
	INIT_WORK(&sdwc->evt_prepare_work, dwc3_sprd_evt_prepare_work);
	INIT_DELAYED_WORK(&sdwc->hotplug_sm_work, dwc3_sprd_hotplug_sm_work);
	INIT_DELAYED_WORK(&sdwc->chg_detect_work, dwc3_sprd_chg_detect_work);

	boot_charging = dwc3_sprd_charger_mode();
	boot_calibration = dwc3_sprd_calibration_mode();
	mutex_init(&sdwc->suspend_resume_mutex);
	spin_lock_init(&sdwc->lock);
	sdwc->dev = dev;

	/* get vbus/id gpios extcon device */
	if (of_property_read_bool(node, "extcon")) {
		struct device_node *extcon_node;

		sdwc->edev = extcon_get_edev_by_phandle(sdwc->dev, 0);
		if (IS_ERR(sdwc->edev)) {
			dev_err(dev, "failed to find vbus extcon device.\n");
			ret = PTR_ERR(sdwc->edev);
			goto err_susp_clk;
		}

		sdwc->vbus_nb.notifier_call = dwc3_sprd_vbus_notifier;
		ret = extcon_register_notifier(sdwc->edev, EXTCON_USB,
						   &sdwc->vbus_nb);
		if (ret) {
			dev_err(dev,
				"failed to register extcon USB notifier.\n");
			goto err_susp_clk;
		}

		sdwc->id_edev = extcon_get_edev_by_phandle(sdwc->dev, 1);
		if (IS_ERR(sdwc->id_edev)) {
			sdwc->id_edev = NULL;
			dev_info(dev, "No separate ID extcon device.\n");
		}

		sdwc->id_nb.notifier_call = dwc3_sprd_id_notifier;
		if (sdwc->id_edev)
			ret = extcon_register_notifier(sdwc->id_edev,
					 EXTCON_USB_HOST, &sdwc->id_nb);
		else
			ret = extcon_register_notifier(sdwc->edev,
					 EXTCON_USB_HOST, &sdwc->id_nb);
		if (ret) {
			dev_err(dev,
			"failed to register extcon USB HOST notifier.\n");
			goto err_extcon_vbus;
		}

		extcon_node = of_parse_phandle(node, "extcon", 0);
		if (!extcon_node) {
			dev_err(dev, "failed to find extcon node.\n");
			goto err_extcon_id;
		}
		sdwc->id_state = DWC3_ID_FLOAT;
	} else {
		/*
		 * In some cases, FPGA, USB Core and PHY may be always powered
		 * on.
		 */
		sdwc->vbus_active = true;

		if (boot_calibration) {
			sdwc->id_state = DWC3_ID_FLOAT;
			sdwc->vbus_active = true;
		} else {
			if (IS_ENABLED(CONFIG_USB_DWC3_HOST) ||
			    IS_ENABLED(CONFIG_USB_DWC3_DUAL_ROLE)) {
				sdwc->id_state = DWC3_ID_GROUND;
			} else {
				sdwc->id_state = DWC3_ID_FLOAT;
				sdwc->vbus_active = true;
			}
		}

		dev_info(dev, "DWC3 is always running as %s\n",
			 sdwc->id_state == DWC3_ID_GROUND ? "HOST" : "DEVICE");
	}

	sdwc->gpiod_pme_wakeup = devm_gpiod_get_index(dev, "pme-wakeup", 0, GPIOD_IN);
	if (IS_ERR(sdwc->gpiod_pme_wakeup)) {
		dev_warn(dev, "get gpiod_pme_wakeup error.\n");
		goto err_extcon_id;
	} else {
		sdwc->support_suspend = true;
		sdwc->wakeup_irq = gpiod_to_irq(sdwc->gpiod_pme_wakeup);
		if (sdwc->wakeup_irq < 0) {
			dev_warn(dev, "cannot get usb wakeup irq.\n");
			goto err_extcon_id;
		}

		ret = devm_request_threaded_irq(dev, sdwc->wakeup_irq,
						dwc3_sprd_pme_wakeup_irq,
						NULL,
						IRQF_TRIGGER_RISING,
						"usb3_pme_wakeup", sdwc);
		if (ret < 0) {
			dev_warn(dev, "cannot request usb pme wakeup irq.\n");
			goto err_extcon_id;
		}
		enable_irq_wake(sdwc->wakeup_irq);
	}

	sdwc->audio_nb.notifier_call = dwc3_sprd_audio_notifier;
	ret = register_sprd_usbm_notifier(&sdwc->audio_nb, SPRD_USBM_EVENT_HOST_DWC3);
	if (ret) {
		dev_err(sdwc->dev, "failed to register usb event\n");
		goto err_extcon_id;
	}

	platform_set_drvdata(pdev, sdwc);

	ret = sysfs_create_groups(&sdwc->dev->kobj, dwc3_sprd_groups);
	if (ret) {
		dev_err(sdwc->dev, "failed to create dwc3 attributes\n");
		goto err_extcon_id;
	}

	sdwc->use_pdhub_c2c = of_property_read_bool(node, "use_pdhub_c2c");
	sdwc->wake_lock = wakeup_source_create("dwc3-sprd");
	wakeup_source_add(sdwc->wake_lock);

	atomic_set(&sdwc->runtime_suspended, 0);
	dwc3_sprd_ext_event_notify(sdwc);

	dwc3_probe_finish = 1;
	dev_info(sdwc->dev, "sprd dwc3 probe finish!\n");

	return 0;

err_extcon_id:
	if (sdwc->edev)
		extcon_unregister_notifier(sdwc->edev, EXTCON_USB_HOST,
					   &sdwc->id_nb);
err_extcon_vbus:
	if (sdwc->edev)
		extcon_unregister_notifier(sdwc->edev, EXTCON_USB,
					   &sdwc->vbus_nb);

err_susp_clk:
	clk_disable_unprepare(sdwc->susp_clk);
err_ref_clk:
	clk_disable_unprepare(sdwc->ref_clk);
err_core_clk:
	clk_disable_unprepare(sdwc->core_clk);
err_ipa_clk:
	usb_clk_prepare_disable(sdwc);

	destroy_workqueue(sdwc->dwc3_wq);
	destroy_workqueue(sdwc->sm_usb_wq);
	return ret;
}

static int dwc3_sprd_remove_child(struct device *dev, void *data)
{
	device_unregister(dev);
	return 0;
}

static int dwc3_sprd_remove(struct platform_device *pdev)
{
	struct dwc3_sprd *sdwc = platform_get_drvdata(pdev);

	dwc3_sprd_usb_notify_exit(pdev);
	device_for_each_child(&pdev->dev, NULL, dwc3_sprd_remove_child);

	clk_disable_unprepare(sdwc->core_clk);
	clk_disable_unprepare(sdwc->ref_clk);
	clk_disable_unprepare(sdwc->susp_clk);
	usb_clk_prepare_disable(sdwc);

	usb_phy_shutdown(sdwc->hs_phy);
	usb_phy_shutdown(sdwc->ss_phy);

	sysfs_remove_groups(&sdwc->dev->kobj, dwc3_sprd_groups);
	if (sdwc->edev) {
		extcon_unregister_notifier(sdwc->edev, EXTCON_USB,
					   &sdwc->vbus_nb);
		extcon_unregister_notifier(sdwc->edev, EXTCON_USB_HOST,
					   &sdwc->id_nb);
	}

	destroy_workqueue(sdwc->dwc3_wq);
	destroy_workqueue(sdwc->sm_usb_wq);

	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	device_wakeup_disable(&pdev->dev);
	return 0;
}

static void dwc3_sprd_enable(struct dwc3_sprd *sdwc)
{
	if (usb_clk_prepare_enable(sdwc))
		dev_err(sdwc->dev, "usb clk enable error.\n");

	if (sdwc->ipa_usb_ref_clk && sdwc->ipa_usb_ref_parent)
		clk_set_parent(sdwc->ipa_usb_ref_clk, sdwc->ipa_usb_ref_parent);

	if (clk_prepare_enable(sdwc->core_clk))
		dev_err(sdwc->dev, "core clk enable error.\n");
	if (clk_prepare_enable(sdwc->ref_clk))
		dev_err(sdwc->dev, "ref clk enable error.\n");
	if (clk_prepare_enable(sdwc->susp_clk))
		dev_err(sdwc->dev, "susp clk enable error.\n");

	usb_phy_init(sdwc->hs_phy);
	usb_phy_init(sdwc->ss_phy);
}

static void dwc3_sprd_disable(struct dwc3_sprd *sdwc)
{
	usb_phy_shutdown(sdwc->hs_phy);
	usb_phy_shutdown(sdwc->ss_phy);
	clk_disable_unprepare(sdwc->susp_clk);
	clk_disable_unprepare(sdwc->ref_clk);
	clk_disable_unprepare(sdwc->core_clk);
	usb_clk_prepare_disable(sdwc);
	/*
	 * set usb ref clock to default when dwc3 was not used,
	 * or else the clock can't be really switch to another
	 * parent within dwc3_sprd_enable.
	 */
	if (sdwc->ipa_usb_ref_clk && sdwc->ipa_usb_ref_default)
		clk_set_parent(sdwc->ipa_usb_ref_clk,
			       sdwc->ipa_usb_ref_default);
}

static int dwc3_sprd_suspend(struct dwc3_sprd *sdwc)
{
	struct dwc3 *dwc = platform_get_drvdata(sdwc->dwc3);
	struct dwc3_event_buffer *evt;

	dev_info(sdwc->dev, "%s: enter\n", __func__);

	mutex_lock(&sdwc->suspend_resume_mutex);
	if (atomic_read(&sdwc->runtime_suspended)) {
		dev_info(sdwc->dev, "%s: Already suspended\n", __func__);
		mutex_unlock(&sdwc->suspend_resume_mutex);
		return 0;
	}

	if (sdwc->glue_dr_mode == USB_DR_MODE_HOST) {
		evt = dwc->ev_buf;
		if ((evt->flags & DWC3_EVENT_PENDING)) {
			dev_info(sdwc->dev,
				"%s: %d device events pending, abort suspend\n",
				__func__, evt->count / 4);
			mutex_unlock(&sdwc->suspend_resume_mutex);
			return -EBUSY;
		}
	}

	if (!sdwc->vbus_active && (dwc->dr_mode == USB_DR_MODE_OTG) &&
		sdwc->drd_state == DRD_STATE_PERIPHERAL) {
		dev_info(sdwc->dev,
			"%s: cable disconnected while not in idle otg state\n",
			__func__);
		mutex_unlock(&sdwc->suspend_resume_mutex);
		return -EBUSY;
	}

	dwc3_sprd_disable(sdwc);

	if (!sdwc->support_suspend)
		__pm_relax(sdwc->wake_lock);
	atomic_set(&sdwc->runtime_suspended, 1);
	mutex_unlock(&sdwc->suspend_resume_mutex);

	return 0;
}

static int dwc3_sprd_resume(struct dwc3_sprd *sdwc)
{
	dev_info(sdwc->dev, "%s: enter\n", __func__);

	mutex_lock(&sdwc->suspend_resume_mutex);
	if (!atomic_read(&sdwc->runtime_suspended)) {
		dev_info(sdwc->dev, "%s: Already resumed\n", __func__);
		mutex_unlock(&sdwc->suspend_resume_mutex);
		return 0;
	}

	if (!sdwc->support_suspend)
		__pm_stay_awake(sdwc->wake_lock);

	dwc3_sprd_enable(sdwc);

	atomic_set(&sdwc->runtime_suspended, 0);
	mutex_unlock(&sdwc->suspend_resume_mutex);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dwc3_sprd_pm_suspend(struct device *dev)
{
	u32 reg = 0;
	int ret = 0, retry = 10;
	u32 msk = MASK_PMU_APB_CURRENT_POWER_STATE_U2PMU |
		MASK_PMU_APB_CURRENT_POWER_STATE_U3PMU;
	struct dwc3_sprd *sdwc = dev_get_drvdata(dev);

	dev_info(dev, "%s: enter\n", __func__);
	if (atomic_read(&sdwc->runtime_suspended))
		goto runtime_suspended;

	if (!sdwc->pmu_apb || !sdwc->pmic) {
		dev_err(dev, "sdwc->pmu_apb & sdwc->pmic are NULL!\n");
		goto runtime_suspended;
	}

	ret |= regmap_read(sdwc->pmu_apb, REG_PMU_APB_SNPS_USBC_SLP_CTRL, &reg);
	reg |= MASK_PMU_APB_SNPS_USBC_HIBER_REQ;
	reg &= ~MASK_PMU_APB_SNPS_USBC_HIBER_STAT_BYP;
	ret |= regmap_write(sdwc->pmu_apb, REG_PMU_APB_SNPS_USBC_SLP_CTRL, reg);
	do {
		ret |= regmap_read(sdwc->pmu_apb, REG_PMU_APB_SNPS_USBC_SLP_CTRL, &reg);
		msleep(20);
		dev_err(dev, "USBC_SLP_CTRL 0x%4x\n", reg);
	} while (((reg & msk) != msk) && --retry);
	if (retry == 0) {
		dev_err(dev, "enter hibernation fail, check host SRE bit.\n");
		ret |= regmap_read(sdwc->pmu_apb, REG_PMU_APB_SNPS_USBC_SLP_CTRL, &reg);
		reg &= ~MASK_PMU_APB_SNPS_USBC_HIBER_REQ;
		reg |= MASK_PMU_APB_SNPS_USBC_HIBER_STAT_BYP;
		ret |= regmap_write(sdwc->pmu_apb, REG_PMU_APB_SNPS_USBC_SLP_CTRL, reg);
		return -ETIMEDOUT;
	}

	clk_disable_unprepare(sdwc->ref_clk);
	clk_disable_unprepare(sdwc->core_clk);
	usb_clk_prepare_disable(sdwc);

	/*
	 * set usb ref clock to default when dwc3 was not used,
	 * or else the clock can't be really switch to another
	 * parent within dwc3_sprd_enable.
	 */
	if (sdwc->ipa_usb_ref_clk && sdwc->ipa_usb_ref_default)
		clk_set_parent(sdwc->ipa_usb_ref_clk, sdwc->ipa_usb_ref_default);

	/* vddgen1, check it everytime we try to sleep. */
	ret |= regmap_read(sdwc->pmic, REG_ANA_SLP_DCDC_PD_CTRL, &reg);
	if (reg & MASK_ANA_SLP_DCDCGEN1_PD_EN) {
		reg &= ~MASK_ANA_SLP_DCDCGEN1_PD_EN;
		ret |= regmap_write(sdwc->pmic, REG_ANA_SLP_DCDC_PD_CTRL, reg);
		sdwc->vddgen1_cleared = 1;
	}

runtime_suspended:
	atomic_set(&sdwc->pm_suspended, 1);

	return ret;
}

static int dwc3_sprd_pm_resume(struct device *dev)
{
	u32 reg = 0;
	int ret = 0, retry = 10;
	u32 msk = MASK_PMU_APB_CURRENT_POWER_STATE_U2PMU |
		MASK_PMU_APB_CURRENT_POWER_STATE_U3PMU;
	struct dwc3_sprd *sdwc = dev_get_drvdata(dev);

	dev_info(dev, "%s: enter\n", __func__);
	if (atomic_read(&sdwc->runtime_suspended))
		goto is_runtime_suspended;

	if (!sdwc->pmu_apb || !sdwc->pmic || !sdwc->ipa_apb) {
		dev_err(dev, "sdwc->pmu_apb & sdwc->pmic are NULL!\n");
		goto is_runtime_suspended;
	}

	do {
		ret |= regmap_read(sdwc->pmu_apb, REG_PMU_APB_PLL_CTRL_STATE_3, &reg);
		usleep_range(10, 20);
	} while (((reg & MASK_PMU_APB_ST_USB31PLLV_STATE) != 0x40) && --retry);
	if (retry == 0)
		dev_err(dev, "USB31PLL did not open, error.\n");

	if (usb_clk_prepare_enable(sdwc))
		dev_err(sdwc->dev, "usb clk enable error.\n");
	if (sdwc->ipa_usb_ref_clk && sdwc->ipa_usb_ref_parent)
		clk_set_parent(sdwc->ipa_usb_ref_clk, sdwc->ipa_usb_ref_parent);
	if (clk_prepare_enable(sdwc->core_clk))
		dev_err(dev, "core-clock enable failed\n");
	if (clk_prepare_enable(sdwc->ref_clk))
		dev_err(dev, "ref-clock enable failed\n");

	ret = regmap_read(sdwc->pmu_apb, REG_PMU_APB_SNPS_USBC_SLP_CTRL, &reg);
	reg &= ~(MASK_PMU_APB_SNPS_USBC_HIBER_REQ | MASK_PMU_APB_SNPS_USBC_HIBER_STAT_BYP);
	ret = regmap_write(sdwc->pmu_apb, REG_PMU_APB_SNPS_USBC_SLP_CTRL, reg);

	retry = 20;
	do {
		ret |= regmap_read(sdwc->pmu_apb, REG_PMU_APB_SNPS_USBC_SLP_CTRL, &reg);
		usleep_range(20, 40);
		dev_info(dev, "USBC_SLP_CTRL 0x%4x\n", reg);
	} while (((reg & msk) != 0) && --retry);
	if (!retry) {
		/* make a reset pulse to make sure we could exit from hibernation */
		ret |= regmap_read(sdwc->ipa_apb, REG_IPA_APB_IPA_RST, &reg);
		reg |= MASK_IPA_APB_USB_SOFT_RST;
		ret |= regmap_write(sdwc->ipa_apb, REG_IPA_APB_IPA_RST, reg);
		usleep_range(100, 200);
		reg &= ~MASK_IPA_APB_USB_SOFT_RST;
		ret |= regmap_write(sdwc->ipa_apb, REG_IPA_APB_IPA_RST, reg);
		retry = 30;
		do {
			ret |= regmap_read(sdwc->pmu_apb, REG_PMU_APB_SNPS_USBC_SLP_CTRL, &reg);
			usleep_range(100, 200);
			dev_info(dev, "USBC_SLP_CTRL 0x%4x\n", reg);
		} while ((reg & msk) && --retry);

		ret |= regmap_read(sdwc->pmu_apb, REG_PMU_APB_SNPS_USBC_SLP_CTRL, &reg);
		if ((reg & msk) != 0)
			dev_err(dev, "exit from hibernation error, can't recovery.\n");
	}
	ret |= regmap_read(sdwc->ipa_apb, REG_IPA_APB_USB_CTL1, &reg);
	reg |= MASK_IPA_APB_USB31_BUS_CLK_BYPASS;
	ret |= regmap_write(sdwc->ipa_apb, REG_IPA_APB_USB_CTL1, reg);
	msleep(20);
	/* make sure we could deep when runtime suspended, write back hiber bypass bit */
	ret |= regmap_read(sdwc->pmu_apb, REG_PMU_APB_SNPS_USBC_SLP_CTRL, &reg);
	reg |= MASK_PMU_APB_SNPS_USBC_HIBER_STAT_BYP;
	ret |= regmap_write(sdwc->pmu_apb, REG_PMU_APB_SNPS_USBC_SLP_CTRL, reg);

	/* recovery vddgen1 config */
	if (sdwc->vddgen1_cleared) {
		ret |= regmap_read(sdwc->pmic, REG_ANA_SLP_DCDC_PD_CTRL, &reg);
		reg |= MASK_ANA_SLP_DCDCGEN1_PD_EN;
		ret |= regmap_write(sdwc->pmic, REG_ANA_SLP_DCDC_PD_CTRL, reg);
		sdwc->vddgen1_cleared = 0;
	}

is_runtime_suspended:
	/* kick in hotplug state machine */
	atomic_set(&sdwc->pm_suspended, 0);
	queue_work(sdwc->dwc3_wq, &sdwc->evt_prepare_work);
	return ret;
}

static int dwc3_host_prepare(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 1;

	return 0;
}

static int dwc3_core_prepare(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 1;

	return 0;
}

#endif

#ifdef CONFIG_PM
static int dwc3_sprd_runtime_suspend(struct device *dev)
{
	struct dwc3_sprd *sdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(sdwc->dwc3);

	dev_info(dev, "%s: enter\n", __func__);
	if (dwc)
		device_init_wakeup(dwc->dev, false);

	return dwc3_sprd_suspend(sdwc);
}

static int dwc3_sprd_runtime_resume(struct device *dev)
{
	struct dwc3_sprd *sdwc = dev_get_drvdata(dev);

	dev_info(dev, "%s: enter\n", __func__);
	return dwc3_sprd_resume(sdwc);
}

static int dwc3_sprd_runtime_idle(struct device *dev)
{
	dev_info(dev, "%s: enter\n", __func__);
	return 0;
}
#endif

static const struct dev_pm_ops dwc3_sprd_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		dwc3_sprd_pm_suspend,
		dwc3_sprd_pm_resume)

	SET_RUNTIME_PM_OPS(
		dwc3_sprd_runtime_suspend,
		dwc3_sprd_runtime_resume,
		dwc3_sprd_runtime_idle)
};

static const struct of_device_id sprd_dwc3_match[] = {
	{ .compatible = "sprd,roc1-dwc3" },
	{ .compatible = "sprd,orca-dwc3" },
	{ .compatible = "sprd,qogirn6pro-dwc3" },
	{},
};
MODULE_DEVICE_TABLE(of, sprd_dwc3_match);

static struct platform_driver dwc3_sprd_driver = {
	.probe		= dwc3_sprd_probe,
	.remove		= dwc3_sprd_remove,
	.driver		= {
		.name	= "dwc3-sprd",
		.of_match_table = sprd_dwc3_match,
		.pm = &dwc3_sprd_dev_pm_ops,
	},
};

module_platform_driver(dwc3_sprd_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 SPRD Glue Layer");
