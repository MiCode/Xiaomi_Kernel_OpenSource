/*
 * dc_xpwr_pwrsrc.c - Intel Dollar Cove(X-power) Power Source Detect Driver
 *
 * Copyright (C) 2014 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/usb/phy.h>
#include <linux/usb/otg.h>
#include <linux/notifier.h>
#include <linux/extcon.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/extcon/extcon-dc-pwrsrc.h>

#define DC_PS_STAT_REG			0x00
#define PS_STAT_VBUS_TRIGGER		(1 << 0)
#define PS_STAT_BAT_CHRG_DIR		(1 << 2)
#define PS_STAT_VBUS_ABOVE_VHOLD	(1 << 3)
#define PS_STAT_VBUS_VALID		(1 << 4)
#define PS_STAT_VBUS_PRESENT		(1 << 5)

#define DC_BC_GLOBAL_REG		0x2C
#define BC_GLOBAL_RUN			(1 << 0)
#define BC_GLOBAL_DET_STAT		(1 << 2)
#define BC_GLOBAL_DBP_TOUT		(1 << 3)
#define BC_GLOBAL_VLGC_COM_SEL		(1 << 4)
#define BC_GLOBAL_DCD_TOUT_MASK		0x60
#define DC_GLOBAL_DCD_TOUT_300MS	0x0
#define DC_GLOBAL_DCD_TOUT_100MS	0x1
#define DC_GLOBAL_DCD_TOUT_500MS	0x2
#define DC_GLOBAL_DCD_TOUT_900MS	0x3
#define BC_GLOBAL_DCD_DET_SEL		(1 << 7)

#define DC_BC_VBUS_CNTL_REG		0x2D
#define VBUS_CNTL_DPDM_PD_EN		(1 << 4)
#define VBUS_CNTL_DPDM_FD_EN		(1 << 5)
#define VBUS_CNTL_FIRST_PO_STAT		(1 << 6)

#define DC_BC_USB_STAT_REG		0x2E
#define USB_STAT_BUS_STAT_MASK		0xf
#define USB_STAT_BUS_STAT_POS		0
#define USB_STAT_BUS_STAT_ATHD		0x0
#define USB_STAT_BUS_STAT_CONN		0x1
#define USB_STAT_BUS_STAT_SUSP		0x2
#define USB_STAT_BUS_STAT_CONF		0x3
#define USB_STAT_USB_SS_MODE		(1 << 4)
#define USB_STAT_DEAD_BAT_DET		(1 << 6)
#define USB_STAT_DBP_UNCFG		(1 << 7)

#define DC_BC_DET_STAT_REG		0x2F
#define DET_STAT_MASK			0xE0
#define DET_STAT_POS			5
#define DET_STAT_SDP			0x1
#define DET_STAT_CDP			0x2
#define DET_STAT_DCP			0x2

#define DC_PS_BOOT_REASON_REG		0x2

#define DC_PWRSRC_IRQ_CFG_REG		0x40
#define PWRSRC_IRQ_CFG_MASK		0x1C

#define DC_BC12_IRQ_CFG_REG		0x45
#define BC12_IRQ_CFG_MASK		0x3

#define DC_XPWR_CHARGE_CUR_DCP		2000
#define DC_XPWR_CHARGE_CUR_CDP		1500
#define DC_XPWR_CHARGE_CUR_SDP		100

#define DC_PWRSRC_INTR_NUM		4
#define PWRSRC_DRV_NAME			"dollar_cove_pwrsrc"

#define PWRSRC_EXTCON_CABLE_USB		"USB"

enum {
	VBUS_FALLING_IRQ = 0,
	VBUS_RISING_IRQ,
	MV_CHNG_IRQ,
	BC_USB_CHNG_IRQ,
};

static const char *dc_extcon_cable[] = {
	PWRSRC_EXTCON_CABLE_USB,
	NULL,
};

struct dc_pwrsrc_info {
	struct platform_device *pdev;
	struct dc_xpwr_pwrsrc_pdata *pdata;
	int irq[DC_PWRSRC_INTR_NUM];
	struct extcon_dev *edev;
	struct usb_phy		*otg;
	struct notifier_block	id_nb;
	struct wake_lock	wakelock;
	bool is_sdp;
	bool id_short;
};

static char *pwr_up_down_info[] = {
	/* bit 0 */ "Last wake caused by user pressing the power button",
	/* bit 2 */ "Last wake caused by a charger insertion",
	/* bit 1 */ "Last wake caused by a battery insertion",
	/* bit 3 */ "Last wake caused by SOC initiated global reset",
	/* bit 4 */ "Last wake caused by cold reset",
	/* bit 5 */ "Last shutdown caused by PMIC UVLO threshold",
	/* bit 6 */ "Last shutdown caused by SOC initiated cold off",
	/* bit 7 */ "Last shutdown caused by user pressing the power button",
	NULL,
};

/* Decode and log the given "reset source indicator" register, then clear it */
static void dc_xpwr_pwrsrc_log_rsi(struct platform_device *pdev,
					char **pwrsrc_rsi_info,
					int reg_s)
{
	char *rsi_info = pwrsrc_rsi_info[0];
	int val, i = 0;
	int bit_select, clear_mask = 0x0;

	val = intel_soc_pmic_readb(reg_s);
	while (rsi_info) {
		bit_select = (1 << i);
		if (val & bit_select) {
			dev_dbg(&pdev->dev, "%s\n", rsi_info);
			clear_mask |= bit_select;
		}
		rsi_info = pwrsrc_rsi_info[++i];
	}

	/* Clear the register value for next reboot (write 1 to clear bit) */
	intel_soc_pmic_writeb(reg_s, clear_mask);
}

int dc_xpwr_vbus_on_status(void)
{
	int ret;

	ret = intel_soc_pmic_readb(DC_PS_STAT_REG);
	if (ret < 0)
		return ret;

	if (ret & PS_STAT_VBUS_PRESENT)
		return  1;

	return 0;
}
EXPORT_SYMBOL(dc_xpwr_vbus_on_status);

static int handle_pwrsrc_event(struct dc_pwrsrc_info *info)
{
	if (dc_xpwr_vbus_on_status()) {
		dev_info(&info->pdev->dev, "VBUS attach\n");
		if (info->edev)
			extcon_set_cable_state(info->edev,
					PWRSRC_EXTCON_CABLE_USB, true);
	} else {
		dev_info(&info->pdev->dev, "VBUS dettach\n");
		if (info->edev)
			extcon_set_cable_state(info->edev,
					PWRSRC_EXTCON_CABLE_USB, false);
	}

	return 0;
}

static int handle_chrg_det_event(struct dc_pwrsrc_info *info)
{
	static bool notify_otg, notify_charger;
	static struct power_supply_cable_props cable_props;
	int stat, cfg, ret, vbus_mask = 0;
	u8 chrg_type;
	bool vbus_attach = false;

	ret = intel_soc_pmic_readb(DC_PS_STAT_REG);
	if (ret < 0) {
		dev_err(&info->pdev->dev, "get vbus stat error\n");
		return ret;
	}

	if ((ret & PS_STAT_VBUS_PRESENT) && !info->id_short) {
		dev_dbg(&info->pdev->dev, "VBUS present\n");
		vbus_attach = true;
	} else {
		dev_dbg(&info->pdev->dev, "VBUS NOT present\n");
		vbus_attach = false;
		cable_props.ma = 0;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_DISCONNECT;
		goto notify_otg_em;
	}

	/* check charger detection completion status */
	ret = intel_soc_pmic_readb(DC_BC_GLOBAL_REG);
	if (ret < 0)
		goto dev_det_ret;
	else
		cfg = ret;
	if (cfg & BC_GLOBAL_DET_STAT) {
		dev_dbg(&info->pdev->dev, "charger detection not complete\n");
		goto dev_det_ret;
	}

	ret = intel_soc_pmic_readb(DC_BC_DET_STAT_REG);
	if (ret < 0)
		goto dev_det_ret;
	else
		stat = ret;

	dev_dbg(&info->pdev->dev, "Stat:%x, Cfg:%x\n", stat, cfg);

	chrg_type = (stat & DET_STAT_MASK) >> DET_STAT_POS;
	info->is_sdp = false;

	if (chrg_type == DET_STAT_SDP) {
		dev_dbg(&info->pdev->dev,
				"SDP cable connecetd\n");
		notify_otg = true;
		vbus_mask = 1;
		notify_charger = true;
		info->is_sdp = true;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_SDP;
		cable_props.ma = DC_XPWR_CHARGE_CUR_SDP;
	} else if (chrg_type == DET_STAT_CDP) {
		dev_dbg(&info->pdev->dev,
				"CDP cable connecetd\n");
		notify_otg = true;
		vbus_mask = 1;
		notify_charger = true;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_CDP;
		cable_props.ma = DC_XPWR_CHARGE_CUR_CDP;
	} else if (chrg_type == DET_STAT_DCP) {
		dev_dbg(&info->pdev->dev,
				"DCP cable connecetd\n");
		notify_charger = true;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_DCP;
		cable_props.ma = DC_XPWR_CHARGE_CUR_DCP;
		if (!wake_lock_active(&info->wakelock))
			wake_lock(&info->wakelock);
	} else {
		dev_warn(&info->pdev->dev,
			"disconnect or unknown or ID event\n");
		cable_props.ma = 0;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_DISCONNECT;
	}

notify_otg_em:
	if (!vbus_attach) {	/* disconnevt event */
		if (notify_otg) {
			atomic_notifier_call_chain(&info->otg->notifier,
						USB_EVENT_VBUS, &vbus_mask);
			notify_otg = false;
		}
		if (notify_charger) {
			atomic_notifier_call_chain(&power_supply_notifier,
					PSY_CABLE_EVENT, &cable_props);
			notify_charger = false;
		}
		if (wake_lock_active(&info->wakelock))
			wake_unlock(&info->wakelock);
	} else {
		if (notify_otg) {
			/*
			 * TODO:close mux path to switch
			 * b/w device mode and host mode.
			 */
			atomic_notifier_call_chain(&info->otg->notifier,
						USB_EVENT_VBUS, &vbus_mask);
		}

		if (notify_charger)
			atomic_notifier_call_chain(&power_supply_notifier,
					PSY_CABLE_EVENT, &cable_props);
	}

	return 0;

dev_det_ret:
	if (ret < 0)
		dev_err(&info->pdev->dev, "BC Mod detection error\n");
	return ret;
}

static irqreturn_t dc_xpwr_pwrsrc_isr(int irq, void *data)
{
	struct dc_pwrsrc_info *info = data;
	int i, ret;

	for (i = 0; i < DC_PWRSRC_INTR_NUM; i++) {
		if (info->irq[i] == irq)
			break;
	}

	if (i >= DC_PWRSRC_INTR_NUM) {
		dev_warn(&info->pdev->dev, "spurious interrupt!!\n");
		return IRQ_NONE;
	}

	if (info->pdata->en_chrg_det)
		ret = handle_chrg_det_event(info);
	else
		ret = handle_pwrsrc_event(info);
	if (ret < 0)
		dev_warn(&info->pdev->dev, "error in PWRSRC INT handling\n");

	return IRQ_HANDLED;
}

static int dc_pwrsrc_handle_otg_notification(struct notifier_block *nb,
				   unsigned long event, void *param)
{
	struct dc_pwrsrc_info *info =
	    container_of(nb, struct dc_pwrsrc_info, id_nb);
	struct power_supply_cable_props cable_props;
	int *val = (int *)param;

	if (!val || ((event != USB_EVENT_ID) &&
			(event != USB_EVENT_ENUMERATED)))
		return NOTIFY_DONE;

	dev_info(&info->pdev->dev,
		"[OTG notification]evt:%lu val:%d\n", event, *val);

	switch (event) {
	case USB_EVENT_ID:
		/*
		 * in case of ID short(*id = 0)
		 * enable vbus else disable vbus.
		 */
		if (*val)
			info->id_short = false;
		else
			info->id_short = true;
		break;
	case USB_EVENT_ENUMERATED:
		/*
		 * ignore cable plug/unplug events as SMSC
		 * had already send those event notifications.
		 * Also only handle notifications for SDP case.
		 */
		if (!*val || !info->is_sdp ||
			(*val == DC_XPWR_CHARGE_CUR_SDP))
			break;
		/*
		 * if current limit is < 100mA
		 * treat it as suspend event.
		 */
		if (*val < DC_XPWR_CHARGE_CUR_SDP)
			cable_props.chrg_evt =
					POWER_SUPPLY_CHARGER_EVENT_SUSPEND;
		else
			cable_props.chrg_evt =
					POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_SDP;
		cable_props.ma = *val;
		atomic_notifier_call_chain(&power_supply_notifier,
					PSY_CABLE_EVENT, &cable_props);
		break;
	default:
		dev_warn(&info->pdev->dev, "invalid OTG event\n");
	}

	return NOTIFY_OK;
}

static int pwrsrc_otg_registration(struct dc_pwrsrc_info *info)
{
	int ret;

	/* OTG notification */
	info->otg = usb_get_phy(USB_PHY_TYPE_USB2);
	if (IS_ERR(info->otg)) {
		dev_warn(&info->pdev->dev, "Failed to get otg transceiver!!\n");
		return PTR_ERR(info->otg);
	}

	info->id_nb.notifier_call = dc_pwrsrc_handle_otg_notification;
	ret = usb_register_notifier(info->otg, &info->id_nb);
	if (ret) {
		dev_err(&info->pdev->dev,
			"failed to register otg notifier\n");
		return ret;
	}

	return 0;
}

static int pwrsrc_extcon_registration(struct dc_pwrsrc_info *info)
{
	int ret = 0;

	/* register with extcon */
	info->edev = kzalloc(sizeof(struct extcon_dev), GFP_KERNEL);
	if (!info->edev) {
		dev_err(&info->pdev->dev, "mem alloc failed\n");
		return -ENOMEM;
	}
	info->edev->name = "dc_pwrsrc";
	info->edev->supported_cable = dc_extcon_cable;
	ret = extcon_dev_register(info->edev);
	if (ret) {
		dev_err(&info->pdev->dev, "extcon registration failed!!\n");
		kfree(info->edev);
	} else {
		dev_dbg(&info->pdev->dev, "extcon registration success!!\n");
	}

	return ret;
}

static int dc_xpwr_pwrsrc_probe(struct platform_device *pdev)
{
	struct dc_pwrsrc_info *info;
	int ret, i;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "mem alloc failed\n");
		return -ENOMEM;
	}

	info->pdev = pdev;
	info->pdata = pdev->dev.platform_data;
	if (!info->pdata)
		return -ENODEV;

	platform_set_drvdata(pdev, info);

	dc_xpwr_pwrsrc_log_rsi(pdev, pwr_up_down_info,
				DC_PS_BOOT_REASON_REG);

	if (info->pdata->en_chrg_det) {
		wake_lock_init(&info->wakelock, WAKE_LOCK_SUSPEND,
						"pwrsrc_wakelock");
		ret = pwrsrc_otg_registration(info);
	} else {
		ret = pwrsrc_extcon_registration(info);
	}
	if (ret < 0)
		return ret;

	for (i = 0; i < DC_PWRSRC_INTR_NUM; i++) {
		info->irq[i] = platform_get_irq(pdev, i);
		ret = request_threaded_irq(info->irq[i],
				NULL, dc_xpwr_pwrsrc_isr,
				IRQF_ONESHOT, PWRSRC_DRV_NAME, info);
		if (ret) {
			dev_err(&pdev->dev, "request_irq fail :%d err:%d\n",
							info->irq[i], ret);
			goto intr_reg_failed;
		}
	}

	/* Unmask VBUS interrupt */
	intel_soc_pmic_writeb(DC_PWRSRC_IRQ_CFG_REG, PWRSRC_IRQ_CFG_MASK);
	if (info->pdata->en_chrg_det) {
		/* unmask the BC1.2 complte interrupts */
		intel_soc_pmic_writeb(DC_BC12_IRQ_CFG_REG, BC12_IRQ_CFG_MASK);
		/* enable the charger detection logic */
		intel_soc_pmic_setb(DC_BC_GLOBAL_REG, BC_GLOBAL_RUN);
	}

	if (info->pdata->en_chrg_det)
		ret = handle_chrg_det_event(info);
	else
		ret = handle_pwrsrc_event(info);
	if (ret < 0)
		dev_warn(&info->pdev->dev, "error in PWRSRC evt handling\n");

	return 0;

intr_reg_failed:
	for (; i > 0; i--)
		free_irq(info->irq[i - 1], info);
	if (info->pdata->en_chrg_det) {
		usb_put_phy(info->otg);
	} else {
		extcon_dev_unregister(info->edev);
		kfree(info->edev);
	}
	return ret;
}

static int dc_xpwr_pwrsrc_remove(struct platform_device *pdev)
{
	struct dc_pwrsrc_info *info = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < DC_PWRSRC_INTR_NUM; i++)
		free_irq(info->irq[i], info);
	if (info->pdata->en_chrg_det) {
		usb_put_phy(info->otg);
	} else {
		extcon_dev_unregister(info->edev);
		kfree(info->edev);
	}
	return 0;
}

#ifdef CONFIG_PM
static int dc_xpwr_pwrsrc_suspend(struct device *dev)
{
	return 0;
}

static int dc_xpwr_pwrsrc_resume(struct device *dev)
{
	return 0;
}
#else
#define dc_xpwr_pwrsrc_suspend		NULL
#define dc_xpwr_pwrsrc_resume		NULL
#endif

static const struct dev_pm_ops dc_xpwr_pwrsrc_driver_pm_ops = {
	.suspend	= dc_xpwr_pwrsrc_suspend,
	.resume		= dc_xpwr_pwrsrc_resume,
};

static struct platform_driver dc_xpwr_pwrsrc_driver = {
	.probe = dc_xpwr_pwrsrc_probe,
	.remove = dc_xpwr_pwrsrc_remove,
	.driver = {
		.name = PWRSRC_DRV_NAME,
		.pm = &dc_xpwr_pwrsrc_driver_pm_ops,
	},
};

static int __init dc_xpwr_pwrsrc_init(void)
{
	return platform_driver_register(&dc_xpwr_pwrsrc_driver);
}
device_initcall(dc_xpwr_pwrsrc_init);

static void __exit dc_xpwr_pwrsrc_exit(void)
{
	platform_driver_unregister(&dc_xpwr_pwrsrc_driver);
}
module_exit(dc_xpwr_pwrsrc_exit);

MODULE_AUTHOR("Ramakrishna Pallala <ramakrishna.pallala@intel.com>");
MODULE_DESCRIPTION("DollarCove(X-power) Power Source Detect Driver");
MODULE_LICENSE("GPL");
