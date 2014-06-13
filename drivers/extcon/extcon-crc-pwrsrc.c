/*
 * intel_crystalcove_pwrsrc.c - Intel Crystal Cove Power Source Detect Driver
 *
 * Copyright (C) 2013 Intel Corporation
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
 * Author: Kannappan R <r.kannappan@intel.com>
 *	Ramakrishna Pallala <ramakrishna.pallala@intel.com>
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
#include <linux/notifier.h>
#include <linux/extcon.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/extcon/extcon-crc-pwrsrc.h>

#define CRYSTALCOVE_PWRSRCIRQ_REG	0x03
#define CRYSTALCOVE_MPWRSRCIRQS0_REG	0x0F
#define CRYSTALCOVE_MPWRSRCIRQSX_REG	0x10
#define CRYSTALCOVE_SPWRSRC_REG		0x1E
#define CRYSTALCOVE_RESETSRC0_REG	0x20
#define CRYSTALCOVE_RESETSRC1_REG	0x21
#define CRYSTALCOVE_WAKESRC_REG		0x22

#define PWRSRC_VBUS_DET			(1 << 0)
#define PWRSRC_DCIN_DET			(1 << 1)
#define PWRSRC_BAT_DET			(1 << 2)

#define CRYSTALCOVE_VBUSCNTL_REG	0x6C
#define VBUSCNTL_EN			(1 << 0)
#define VBUSCNTL_SEL			(1 << 1)

#define PWRSRC_EXTCON_CABLE_AC		"CHARGER_AC"
#define PWRSRC_EXTCON_CABLE_USB		"USB"
#define PWRSRC_DRV_NAME			"crystal_cove_pwrsrc"

/*
 * Crystal Cove PMIC can not do USB type detection.
 * So if we find extcon USB_SDP cable then unregister
 * the pwrsrc extcon device as BYT platform is not
 * supporting AC and USB simultaneously.
 */
#define EXTCON_CABLE_SDP		"CHARGER_USB_SDP"

static const char *byt_extcon_cable[] = {
	PWRSRC_EXTCON_CABLE_AC,
	PWRSRC_EXTCON_CABLE_USB,
	NULL,
};

struct pwrsrc_info {
	struct platform_device *pdev;
	int irq;
	struct usb_phy *otg;
	struct extcon_dev *edev;
	struct notifier_block	nb;
};

static char *pwrsrc_resetsrc0_info[] = {
	/* bit 0 */ "Last shutdown caused by SOC reporting a thermal event",
	/* bit 1 */ "Last shutdown caused by critical PMIC temperature",
	/* bit 2 */ "Last shutdown caused by critical system temperature",
	/* bit 3 */ "Last shutdown caused by critical battery temperature",
	/* bit 4 */ "Last shutdown caused by VSYS under voltage",
	/* bit 5 */ "Last shutdown caused by VSYS over voltage",
	/* bit 6 */ "Last shutdown caused by battery removal",
	NULL,
};

static char *pwrsrc_resetsrc1_info[] = {
	/* bit 0 */ "Last shutdown caused by VCRIT threshold",
	/* bit 1 */ "Last shutdown caused by BATID reporting battery removal",
	/* bit 2 */ "Last shutdown caused by user pressing the power button",
	NULL,
};

static char *pwrsrc_wakesrc_info[] = {
	/* bit 0 */ "Last wake caused by user pressing the power button",
	/* bit 1 */ "Last wake caused by a battery insertion",
	/* bit 2 */ "Last wake caused by a USB charger insertion",
	/* bit 3 */ "Last wake caused by an adapter insertion",
	NULL,
};

/* Decode and log the given "reset source indicator" register, then clear it */
static void crystalcove_pwrsrc_log_rsi(struct platform_device *pdev,
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
			dev_info(&pdev->dev, "%s\n", rsi_info);
			clear_mask |= bit_select;
		}
		rsi_info = pwrsrc_rsi_info[++i];
	}

	/* Clear the register value for next reboot (write 1 to clear bit) */
	intel_soc_pmic_writeb(reg_s, clear_mask);
}

/*
 * D1 ensures SW control: D1[0]  = HW mode, D1[1] = SW mode
 * D0 control the VBUS: D0[0] = disable VBUS, D0[1] = enable VBUS
 */
int crystal_cove_enable_vbus(void)
{
	int ret;

	ret = intel_soc_pmic_writeb(CRYSTALCOVE_VBUSCNTL_REG, 0x03);
	return ret;
}
EXPORT_SYMBOL(crystal_cove_enable_vbus);

int crystal_cove_disable_vbus(void)
{
	int ret;

	ret = intel_soc_pmic_writeb(CRYSTALCOVE_VBUSCNTL_REG, 0x02);
	return ret;
}
EXPORT_SYMBOL(crystal_cove_disable_vbus);

int crystal_cove_vbus_on_status(void)
{
	int ret;

	ret = intel_soc_pmic_readb(CRYSTALCOVE_SPWRSRC_REG);
	if (ret < 0)
		return ret;

	if (ret & PWRSRC_VBUS_DET)
		return  1;

	return 0;
}
EXPORT_SYMBOL(crystal_cove_vbus_on_status);

static void handle_pwrsrc_event(struct pwrsrc_info *info, int pwrsrcirq)
{
	int spwrsrc, mask;

	spwrsrc = intel_soc_pmic_readb(CRYSTALCOVE_SPWRSRC_REG);
	if (spwrsrc < 0)
		goto pmic_read_fail;

	if (pwrsrcirq & PWRSRC_VBUS_DET) {
		if (spwrsrc & PWRSRC_VBUS_DET) {
			dev_dbg(&info->pdev->dev, "VBUS attach event\n");
			mask = 1;
			if (info->edev)
				extcon_set_cable_state(info->edev,
						PWRSRC_EXTCON_CABLE_USB, true);
		} else {
			dev_dbg(&info->pdev->dev, "VBUS detach event\n");
			mask = 0;
			if (info->edev)
				extcon_set_cable_state(info->edev,
						PWRSRC_EXTCON_CABLE_USB, false);
		}
		/* notify OTG driver */
		if (info->otg)
			atomic_notifier_call_chain(&info->otg->notifier,
				USB_EVENT_VBUS, &mask);
	} else if (pwrsrcirq & PWRSRC_DCIN_DET) {
		if (spwrsrc & PWRSRC_DCIN_DET) {
			dev_dbg(&info->pdev->dev, "ADP attach event\n");
			if (info->edev)
				extcon_set_cable_state(info->edev,
						PWRSRC_EXTCON_CABLE_AC, true);
		} else {
			dev_dbg(&info->pdev->dev, "ADP detach event\n");
			if (info->edev)
				extcon_set_cable_state(info->edev,
						PWRSRC_EXTCON_CABLE_AC, false);
		}
	} else if (pwrsrcirq & PWRSRC_BAT_DET) {
		if (spwrsrc & PWRSRC_BAT_DET)
			dev_dbg(&info->pdev->dev, "Battery attach event\n");
		else
			dev_dbg(&info->pdev->dev, "Battery detach event\n");
	} else {
		dev_dbg(&info->pdev->dev, "event none or spurious\n");
	}

	return;

pmic_read_fail:
	dev_err(&info->pdev->dev, "SPWRSRC read failed:%d\n", spwrsrc);
	return;
}

static irqreturn_t crystalcove_pwrsrc_isr(int irq, void *data)
{
	struct pwrsrc_info *info = data;
	int pwrsrcirq;

	pwrsrcirq = intel_soc_pmic_readb(CRYSTALCOVE_PWRSRCIRQ_REG);
	if (pwrsrcirq < 0) {
		dev_err(&info->pdev->dev, "PWRSRCIRQ read failed\n");
		goto pmic_irq_fail;
	}

	dev_dbg(&info->pdev->dev, "pwrsrcirq=%x\n", pwrsrcirq);
	handle_pwrsrc_event(info, pwrsrcirq);

pmic_irq_fail:
	intel_soc_pmic_writeb(CRYSTALCOVE_PWRSRCIRQ_REG, pwrsrcirq);
	return IRQ_HANDLED;
}

static int pwrsrc_extcon_dev_reg_callback(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct pwrsrc_info *info = container_of(nb, struct pwrsrc_info, nb);
	int mask = 0;

	/* check if there is other extcon cables */
	if (extcon_num_of_cable_devs(EXTCON_CABLE_SDP)) {
		dev_info(&info->pdev->dev, "unregistering otg device\n");
		/* Send VBUS disconnect as another cable detection
		 * driver registered to extcon framework and notifies
		 * OTG on cable connect */
		if (info->otg)
			atomic_notifier_call_chain(&info->otg->notifier,
				USB_EVENT_VBUS, &mask);
		/* Set VBUS supply mode to SW control mode */
		intel_soc_pmic_writeb(CRYSTALCOVE_VBUSCNTL_REG, 0x02);
		if (info->nb.notifier_call) {
			extcon_dev_unregister_notify(&info->nb);
			info->nb.notifier_call = NULL;
		}
		if (info->otg) {
			usb_put_phy(info->otg);
			info->otg = NULL;
		}
	}

	return NOTIFY_OK;
}

static int pwrsrc_extcon_registration(struct pwrsrc_info *info)
{
	int ret = 0;

	/* register with extcon */
	info->edev = kzalloc(sizeof(struct extcon_dev), GFP_KERNEL);
	if (!info->edev) {
		dev_err(&info->pdev->dev, "mem alloc failed\n");
		ret = -ENOMEM;
		goto pwrsrc_extcon_fail;
	}
	info->edev->name = "BYT-Charger";
	info->edev->supported_cable = byt_extcon_cable;
	ret = extcon_dev_register(info->edev);
	if (ret) {
		dev_err(&info->pdev->dev, "extcon registration failed!!\n");
		kfree(info->edev);
		goto pwrsrc_extcon_fail;
	}

	if (extcon_num_of_cable_devs(EXTCON_CABLE_SDP)) {
		dev_info(&info->pdev->dev,
			"extcon device is already registered\n");
		/* Set VBUS supply mode to SW control mode */
		intel_soc_pmic_writeb(CRYSTALCOVE_VBUSCNTL_REG, 0x02);
	} else {
		/* Workaround: Set VBUS supply mode to HW control mode */
		intel_soc_pmic_writeb(CRYSTALCOVE_VBUSCNTL_REG, 0x00);

		/* OTG notification */
		info->otg = usb_get_phy(USB_PHY_TYPE_USB2);
		if (IS_ERR_OR_NULL(info->otg)) {
			info->otg = NULL;
			dev_warn(&info->pdev->dev, "Failed to get otg transceiver!!\n");
			extcon_dev_unregister(info->edev);
			kfree(info->edev);
			ret = -ENODEV;
			goto pwrsrc_extcon_fail;
		}
		info->nb.notifier_call = &pwrsrc_extcon_dev_reg_callback;
		extcon_dev_register_notify(&info->nb);
	}

pwrsrc_extcon_fail:
	return ret;
}

static int crystalcove_pwrsrc_probe(struct platform_device *pdev)
{
	struct pwrsrc_info *info;
	int ret, pwrsrcirq = 0x0;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "mem alloc failed\n");
		return -ENOMEM;
	}

	info->pdev = pdev;
	info->irq = platform_get_irq(pdev, 0);
	platform_set_drvdata(pdev, info);

	/* Log reason for last reset and wake events */
	crystalcove_pwrsrc_log_rsi(pdev, pwrsrc_resetsrc0_info,
				CRYSTALCOVE_RESETSRC0_REG);
	crystalcove_pwrsrc_log_rsi(pdev, pwrsrc_resetsrc1_info,
				CRYSTALCOVE_RESETSRC1_REG);
	crystalcove_pwrsrc_log_rsi(pdev, pwrsrc_wakesrc_info,
				CRYSTALCOVE_WAKESRC_REG);

	ret = pwrsrc_extcon_registration(info);
	if (ret)
		goto extcon_reg_failed;

	/* check if device is already connected */
	if (info->otg)
		pwrsrcirq |= PWRSRC_VBUS_DET;
	if (info->edev)
		pwrsrcirq |= PWRSRC_DCIN_DET;
	if (pwrsrcirq)
		handle_pwrsrc_event(info, pwrsrcirq);

	ret = request_threaded_irq(info->irq, NULL, crystalcove_pwrsrc_isr,
				IRQF_ONESHOT | IRQF_NO_SUSPEND,
				PWRSRC_DRV_NAME, info);
	if (ret) {
		dev_err(&pdev->dev, "unable to register irq %d\n", info->irq);
		goto intr_teg_failed;
	}

	/* unmask the PWRSRC interrupts */
	intel_soc_pmic_writeb(CRYSTALCOVE_MPWRSRCIRQS0_REG, 0x00);
	intel_soc_pmic_writeb(CRYSTALCOVE_MPWRSRCIRQSX_REG, 0x00);

	return 0;

intr_teg_failed:
	if (info->otg)
		usb_put_phy(info->otg);
	if (info->edev) {
		extcon_dev_unregister(info->edev);
		kfree(info->edev);
	}
extcon_reg_failed:
	kfree(info);
	return ret;
}

static int crystalcove_pwrsrc_remove(struct platform_device *pdev)
{
	struct pwrsrc_info *info = platform_get_drvdata(pdev);

	free_irq(info->irq, info);
	if (info->otg)
		usb_put_phy(info->otg);
	if (info->edev) {
		extcon_dev_unregister(info->edev);
		kfree(info->edev);
	}
	kfree(info);
	return 0;
}

#ifdef CONFIG_PM
static int crystalcove_pwrsrc_suspend(struct device *dev)
{
	return 0;
}

static int crystalcove_pwrsrc_resume(struct device *dev)
{
	return 0;
}
#else
#define crystalcove_pwrsrc_suspend		NULL
#define crystalcove_pwrsrc_resume		NULL
#endif

static const struct dev_pm_ops crystalcove_pwrsrc_driver_pm_ops = {
	.suspend	= crystalcove_pwrsrc_suspend,
	.resume		= crystalcove_pwrsrc_resume,
};

static struct platform_driver crystalcove_pwrsrc_driver = {
	.probe = crystalcove_pwrsrc_probe,
	.remove = crystalcove_pwrsrc_remove,
	.driver = {
		.name = PWRSRC_DRV_NAME,
		.pm = &crystalcove_pwrsrc_driver_pm_ops,
	},
};

static int __init crystalcove_pwrsrc_init(void)
{
	return platform_driver_register(&crystalcove_pwrsrc_driver);
}
device_initcall(crystalcove_pwrsrc_init);

static void __exit crystalcove_pwrsrc_exit(void)
{
	platform_driver_unregister(&crystalcove_pwrsrc_driver);
}
module_exit(crystalcove_pwrsrc_exit);

MODULE_AUTHOR("Kannappan R <r.kannappan@intel.com>");
MODULE_AUTHOR("Ramakrishna Pallala <ramakrishna.pallala@intel.com>");
MODULE_DESCRIPTION("CrystalCove Power Source Detect Driver");
MODULE_LICENSE("GPL");
