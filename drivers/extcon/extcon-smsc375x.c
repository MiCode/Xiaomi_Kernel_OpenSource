/*
 * extcon-smsc375x.c - SMSC375x extcon driver
 *
 * Copyright (C) 2013 Intel Corporation
 * Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/usb/otg.h>
#include <linux/notifier.h>
#include <linux/extcon.h>
#include <linux/pm_runtime.h>
#include <linux/acpi.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/extcon/extcon-smsc375x.h>

/* SMSC375x I2C registers */
#define SMSC375X_REG_STAT		0x00
#define SMSC375X_REG_CFG		0x01
#define SMSC375X_REG_CHRG_CFG		0x02
#define SMSC375X_REG_CHRG_STAT		0x03

/* Status */
#define STAT_OVLO_STAT		(1 << 0)
#define STAT_OVLO_LATCH		(1 << 1)
#define STAT_OVP_SWTCH_STAT	(1 << 2)
#define STAT_CUR_LMT_STAT	(1 << 3)
#define STAT_CHRG_DET_DONE	(1 << 4)
#define STAT_CHRG_TYPE_MASK	(7 << 5)
#define STAT_CHRG_TYPE_DCP	(1 << 5)
#define STAT_CHRG_TYPE_CDP	(2 << 5)
#define STAT_CHRG_TYPE_SDP	(3 << 5)
#define STAT_CHRG_TYPE_SE1L	(4 << 5)
#define STAT_CHRG_TYPE_SE1H	(5 << 5)

/* Config */
#define CFG_EN_OVP_SWITCH	(1 << 0)
#define CFG_EN_CUR_LMT		(1 << 1)
#define CFG_OVERRIDE_VBUS	(1 << 2)
#define CFG_OVERRIDE_CUR_LMT	(1 << 3)
#define CFG_EN_MUX1		(1 << 5)
#define CFG_EN_MUX2		(1 << 6)
#define CFG_SOFT_POR		(1 << 7)

/* Charger Config */
#define CHRG_CFG_EN_SNG_RX	(1 << 0)
#define CHRG_CFG_EN_CON_DET	(1 << 1)
#define CHRG_CFG_EN_VDAT_SRC	(1 << 2)
#define CHRG_CFG_EN_HOST_CHRG	(1 << 3)
#define CHRG_CFG_EN_IDAT_SINK	(1 << 4)
#define CHRG_CFG_EN_DP_PDOWN	(1 << 5)
#define CHRG_CFG_EN_DM_PDOWN	(1 << 6)
#define CHRG_CFG_I2C_CNTL	(1 << 7)


/* Charger Config */
#define CHRG_STAT_VDAT_DET	(1 << 0)
#define CHRG_STAT_DP_SNG_RX	(1 << 1)
#define CHRG_STAT_DM_SNG_RX	(1 << 2)
#define CHRG_STAT_RX_HIGH_CUR	(1 << 3)


#define SMSC_CHARGE_CUR_DCP		2000
#define SMSC_CHARGE_CUR_CDP		1500
#define SMSC_CHARGE_CUR_SDP_100		100
#define SMSC_CHARGE_CUR_SDP_500		500

#define SMSC375X_EXTCON_USB		"USB"
#define SMSC375X_EXTCON_SDP		"CHARGER_USB_SDP"
#define SMSC375X_EXTCON_DCP		"CHARGER_USB_DCP"
#define SMSC375X_EXTCON_CDP		"CHARGER_USB_CDP"

static const char *smsc375x_extcon_cable[] = {
	SMSC375X_EXTCON_SDP,
	SMSC375X_EXTCON_DCP,
	SMSC375X_EXTCON_CDP,
	NULL,
};

struct smsc375x_chip {
	struct i2c_client	*client;
	struct smsc375x_pdata	*pdata;
	struct usb_phy		*otg;
	struct work_struct	otg_work;
	struct notifier_block	id_nb;
	bool			id_short;
	struct extcon_specific_cable_nb cable_obj;
	struct notifier_block	vbus_nb;
	struct work_struct	vbus_work;
	struct extcon_dev	*edev;
	struct wake_lock	wakelock;
	bool			is_sdp;
};

static struct smsc375x_chip *chip_ptr;

static int smsc375x_write_reg(struct i2c_client *client,
		int reg, int value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int smsc375x_read_reg(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int smsc375x_detect_dev(struct smsc375x_chip *chip)
{
	struct i2c_client *client = chip->client;
	static bool notify_otg, notify_charger;
	static char *cable;
	static struct power_supply_cable_props cable_props;
	int stat, cfg, ret, vbus_mask = 0;
	u8 chrg_type;
	bool vbus_attach = false;

	dev_info(&chip->client->dev, "%s\n", __func__);
	/*
	 * get VBUS status from external IC like
	 * PMIC or Charger as SMSC375x chip can not
	 * be accessed with out VBUS.
	 */
	ret = chip->pdata->is_vbus_online();
	if (ret < 0) {
		dev_info(&chip->client->dev, "get vbus stat error\n");
		return ret;
	}

	if (ret) {
		dev_info(&chip->client->dev, "VBUS present\n");
		vbus_attach = true;
	} else {
		dev_info(&chip->client->dev, "VBUS NOT present\n");
		vbus_attach = false;
		cable_props.ma = 0;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_DISCONNECT;
		goto notify_otg_em;
	}

	/* dont proceed with charger detection in host mode */
	if (chip->id_short) {
		/*
		 * only after reading the status register
		 * MUX path is being closed. And by default
		 * MUX is to connected Host mode path.
		 */
		ret = smsc375x_read_reg(client, SMSC375X_REG_STAT);
		return ret;
	}
	/* check charger detection completion status */
	ret = smsc375x_read_reg(client, SMSC375X_REG_STAT);
	if (ret < 0)
		goto dev_det_i2c_failed;
	else
		stat = ret;

	if (!(stat & STAT_CHRG_DET_DONE)) {
		dev_info(&chip->client->dev, "DET failed");
		return -EOPNOTSUPP;
	}

	ret = smsc375x_read_reg(client, SMSC375X_REG_CFG);
	if (ret < 0)
		goto dev_det_i2c_failed;
	else
		cfg = ret;

	dev_info(&client->dev, "Stat:%x, Cfg:%x\n", stat, cfg);

	chrg_type = stat & STAT_CHRG_TYPE_MASK;
	chip->is_sdp = false;

	if (chrg_type == STAT_CHRG_TYPE_SDP) {
		dev_info(&chip->client->dev,
				"SDP cable connecetd\n");
		notify_otg = true;
		vbus_mask = 1;
		notify_charger = true;
		chip->is_sdp = true;
		cable = SMSC375X_EXTCON_SDP;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_SDP;
		if (chip->pdata->charging_compliance_override)
			cable_props.ma = SMSC_CHARGE_CUR_SDP_500;
		else
			cable_props.ma = SMSC_CHARGE_CUR_SDP_100;
	} else if (chrg_type == STAT_CHRG_TYPE_CDP) {
		dev_info(&chip->client->dev,
				"CDP cable connecetd\n");
		notify_otg = true;
		vbus_mask = 1;
		notify_charger = true;
		cable = SMSC375X_EXTCON_CDP;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_CDP;
		cable_props.ma = SMSC_CHARGE_CUR_CDP;
	} else if ((chrg_type == STAT_CHRG_TYPE_DCP) ||
			(chrg_type == STAT_CHRG_TYPE_SE1L) ||
			(chrg_type == STAT_CHRG_TYPE_SE1H)) {
		dev_info(&chip->client->dev,
				"DCP/SE1 cable connecetd\n");
		notify_charger = true;
		cable = SMSC375X_EXTCON_DCP;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_DCP;
		cable_props.ma = SMSC_CHARGE_CUR_DCP;
		if (!wake_lock_active(&chip->wakelock))
			wake_lock(&chip->wakelock);
	} else {
		dev_warn(&chip->client->dev,
			"disconnect or unknown or ID event\n");
		cable_props.ma = 0;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_DISCONNECT;
	}

notify_otg_em:
	if (!vbus_attach) {	/* disconnevt event */
		if (notify_otg) {
			atomic_notifier_call_chain(&chip->otg->notifier,
				vbus_mask ? USB_EVENT_VBUS : USB_EVENT_NONE,
				NULL);
			notify_otg = false;
		}
		if (notify_charger) {
			/*
			 * not supporting extcon events currently.
			 * extcon_set_cable_state(chip->edev, cable, false);
			 */
			atomic_notifier_call_chain(&power_supply_notifier,
					PSY_CABLE_EVENT, &cable_props);
			notify_charger = false;
			cable = NULL;
		}
		if (wake_lock_active(&chip->wakelock))
			wake_unlock(&chip->wakelock);
	} else {
		if (notify_otg) {
			/* close mux path to enable device mode */
			ret = smsc375x_write_reg(client, SMSC375X_REG_CFG,
					(cfg & ~CFG_EN_MUX1) | CFG_EN_MUX2);
			if (ret < 0)
				goto dev_det_i2c_failed;
			atomic_notifier_call_chain(&chip->otg->notifier,
				vbus_mask ? USB_EVENT_VBUS : USB_EVENT_NONE,
				NULL);
		}

		if (notify_charger) {
			/*
			 * not supporting extcon events currently.
			 * extcon_set_cable_state(chip->edev, cable, true);
			 */
			atomic_notifier_call_chain(&power_supply_notifier,
					PSY_CABLE_EVENT, &cable_props);
		}
	}

	return 0;

dev_det_i2c_failed:
	if (chip->pdata->is_vbus_online())
		dev_err(&chip->client->dev,
				"vbus present: i2c read failed:%d\n", ret);
	else
		dev_info(&chip->client->dev,
				"vbus removed: i2c read failed:%d\n", ret);
	return ret;
}

static irqreturn_t smsc375x_irq_handler(int irq, void *data)
{
	struct smsc375x_chip *chip = data;

	pm_runtime_get_sync(&chip->client->dev);

	dev_info(&chip->client->dev, "SMSC USB INT!\n");

	smsc375x_detect_dev(chip);

	pm_runtime_put_sync(&chip->client->dev);
	return IRQ_HANDLED;
}

static void smsc375x_otg_event_worker(struct work_struct *work)
{
	struct smsc375x_chip *chip =
	    container_of(work, struct smsc375x_chip, otg_work);
	int ret;

	pm_runtime_get_sync(&chip->client->dev);

	if (chip->id_short)
		ret = chip->pdata->enable_vbus();
	else
		ret = chip->pdata->disable_vbus();
	if (ret < 0)
		dev_warn(&chip->client->dev, "id vbus control failed\n");

	/*
	 * As we are not getting SMSC INT in case
	 * 5V boost enablement.
	 * Follwoing WA is added to enable Host mode
	 * on CR V2.1 by invoking the VBUS worker.
	 */
	msleep(5000);
	schedule_work(&chip->vbus_work);

	pm_runtime_put_sync(&chip->client->dev);
}

static int smsc375x_handle_otg_notification(struct notifier_block *nb,
				   unsigned long event, void *param)
{
	struct smsc375x_chip *chip =
	    container_of(nb, struct smsc375x_chip, id_nb);
	struct power_supply_cable_props cable_props;
	int *val = (int *)param;

	if ((event != USB_EVENT_ID) &&
		(event != USB_EVENT_NONE) &&
		(event != USB_EVENT_ENUMERATED))
		return NOTIFY_DONE;

	if ((event == USB_EVENT_ENUMERATED) && !param)
		return NOTIFY_DONE;

	dev_info(&chip->client->dev,
		"[OTG notification]evt:%lu val:%d\n", event,
				val ? *val : -1);

	switch (event) {
	case USB_EVENT_ID:
		/*
		 * in case of ID short(*id = 0)
		 * enable vbus else disable vbus.
		 */
		chip->id_short = true;
		schedule_work(&chip->otg_work);
		break;
	case USB_EVENT_NONE:
		chip->id_short = false;
		schedule_work(&chip->otg_work);
		break;
	case USB_EVENT_ENUMERATED:
		/*
		 * ignore cable plug/unplug events as SMSC
		 * had already send those event notifications.
		 * Also only handle notifications for SDP case.
		 */
		/* No need to change SDP inlimit based on enumeration status
		 * if platform can voilate charging_compliance.
		 */
		if (chip->pdata->charging_compliance_override ||
			 !chip->is_sdp ||
			(*val == SMSC_CHARGE_CUR_SDP_100))
			break;
		/*
		 * if current limit is < 100mA
		 * treat it as suspend event.
		 */
		if (*val < SMSC_CHARGE_CUR_SDP_100)
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
		dev_warn(&chip->client->dev, "invalid OTG event\n");
	}

	return NOTIFY_OK;
}

static void smsc375x_pwrsrc_event_worker(struct work_struct *work)
{
	struct smsc375x_chip *chip =
	    container_of(work, struct smsc375x_chip, vbus_work);
	int ret;

	pm_runtime_get_sync(&chip->client->dev);

	/*
	 * Sometimes SMSC INT triggering is only
	 * happening after reading the status bits.
	 * So we are reading the status register as WA
	 * to invoke teh MUX INT in case of connect events.
	 */
	if (!chip->pdata->is_vbus_online()) {
		ret = smsc375x_detect_dev(chip);
	} else {
		/**
		 * To guarantee SDP detection in SMSC, need 75mSec delay before
		 * sending an I2C command. So added 50mSec delay here.
		 */
		mdelay(50);
		ret = smsc375x_read_reg(chip->client, SMSC375X_REG_STAT);
	}
	if (ret < 0)
		dev_warn(&chip->client->dev, "pwrsrc evt error\n");

	pm_runtime_put_sync(&chip->client->dev);
}

static int smsc375x_handle_pwrsrc_notification(struct notifier_block *nb,
				   unsigned long event, void *param)
{
	struct smsc375x_chip *chip =
	    container_of(nb, struct smsc375x_chip, vbus_nb);

	dev_info(&chip->client->dev, "[PWRSRC notification]: %lu\n", event);

	schedule_work(&chip->vbus_work);

	return NOTIFY_OK;
}

static int smsc375x_irq_init(struct smsc375x_chip *chip)
{
	const struct acpi_device_id *id;
	struct i2c_client *client = chip->client;
	struct device *dev;
	struct gpio_desc *gpio;
	int ret;

	if (!client)
		return -EINVAL;
	dev = &client->dev;
	if (!ACPI_HANDLE(dev))
		return -ENODEV;

	id = acpi_match_device(dev->driver->acpi_match_table, dev);
	if (!id)
		return -ENODEV;
	gpio = devm_gpiod_get_index(dev, "smsc3750_int", 0);
	if (IS_ERR(gpio)) {
		dev_err(dev, "acpi gpio get index failed\n");
		return PTR_ERR(gpio);
	}
	ret = gpiod_to_irq(gpio);
	if (ret < 0)
		return ret;

	/* get irq number */
	chip->client->irq = ret;
	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
				smsc375x_irq_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"smsc375x", chip);
		if (ret) {
			dev_err(&client->dev, "failed to reqeust IRQ\n");
			return ret;
		}
		enable_irq_wake(client->irq);
	} else {
		dev_err(&client->dev, "IRQ not set\n");
		return -EINVAL;
	}

	return 0;
}

static int smsc375x_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct smsc375x_chip *chip;
	int ret = 0, id_val = -1;

	chip = kzalloc(sizeof(struct smsc375x_chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	chip->client = client;
#ifdef CONFIG_ACPI
	chip->pdata = (struct smsc375x_pdata *)id->driver_data;
#else
	chip->pdata = dev->platform_data;
#endif
	i2c_set_clientdata(client, chip);
	wake_lock_init(&chip->wakelock, WAKE_LOCK_SUSPEND,
						"smsc375x_wakelock");

	/* register with extcon */
	chip->edev = kzalloc(sizeof(struct extcon_dev), GFP_KERNEL);
	if (!chip->edev) {
		dev_err(&client->dev, "mem alloc failed\n");
		ret = -ENOMEM;
		goto extcon_mem_failed;
	}
	chip->edev->name = "smsc375x";
	chip->edev->supported_cable = smsc375x_extcon_cable;
	ret = extcon_dev_register(chip->edev);
	if (ret) {
		dev_err(&client->dev, "extcon registration failed!!\n");
		goto extcon_reg_failed;
	}

	/* register for EXTCON USB notification */
	INIT_WORK(&chip->vbus_work, smsc375x_pwrsrc_event_worker);
	chip->vbus_nb.notifier_call = smsc375x_handle_pwrsrc_notification;
	ret = extcon_register_interest(&chip->cable_obj, NULL,
			SMSC375X_EXTCON_USB, &chip->vbus_nb);

	/* OTG notification */
	chip->otg = usb_get_phy(USB_PHY_TYPE_USB2);
	if (!chip->otg) {
		dev_warn(&client->dev, "Failed to get otg transceiver!!\n");
		goto otg_reg_failed;
	}

	INIT_WORK(&chip->otg_work, smsc375x_otg_event_worker);
	chip->id_nb.notifier_call = smsc375x_handle_otg_notification;
	ret = usb_register_notifier(chip->otg, &chip->id_nb);
	if (ret) {
		dev_err(&chip->client->dev,
			"failed to register otg notifier\n");
		goto id_reg_failed;
	}

	ret = smsc375x_irq_init(chip);
	if (ret)
		goto intr_reg_failed;

	chip_ptr = chip;

	if (chip->otg->get_id_status) {
		ret = chip->otg->get_id_status(chip->otg, &id_val);
		if (ret < 0) {
			dev_warn(&client->dev,
				"otg get ID status failed:%d\n", ret);
			ret = 0;
		}
	}

	if (!id_val && !chip->id_short)
		atomic_notifier_call_chain(&chip->otg->notifier,
				USB_EVENT_ID, NULL);
	else
		smsc375x_detect_dev(chip);

	/* Init Runtime PM State */
	pm_runtime_put_noidle(&chip->client->dev);
	pm_schedule_suspend(&chip->client->dev, MSEC_PER_SEC);

	return 0;

intr_reg_failed:
	usb_unregister_notifier(chip->otg, &chip->id_nb);
id_reg_failed:
	usb_put_phy(chip->otg);
otg_reg_failed:
	extcon_dev_unregister(chip->edev);
extcon_reg_failed:
	kfree(chip->edev);
extcon_mem_failed:
	kfree(chip);
	return ret;
}

static int smsc375x_remove(struct i2c_client *client)
{
	struct smsc375x_chip *chip = i2c_get_clientdata(client);

	free_irq(client->irq, chip);
	usb_put_phy(chip->otg);
	extcon_dev_unregister(chip->edev);
	kfree(chip->edev);
	pm_runtime_get_noresume(&chip->client->dev);
	kfree(chip);
	return 0;
}

static void smsc375x_shutdown(struct i2c_client *client)
{
	dev_dbg(&client->dev, "smsc375x shutdown\n");

	if (client->irq > 0)
		disable_irq(client->irq);
	return;
}

static int smsc375x_suspend(struct device *dev)
{
	struct smsc375x_chip *chip = dev_get_drvdata(dev);

	if (chip->client->irq > 0)
		disable_irq(chip->client->irq);

	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int smsc375x_resume(struct device *dev)
{
	struct smsc375x_chip *chip = dev_get_drvdata(dev);

	if (chip->client->irq > 0)
		enable_irq(chip->client->irq);

	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int smsc375x_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int smsc375x_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int smsc375x_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static const struct dev_pm_ops smsc375x_pm_ops = {
		SET_SYSTEM_SLEEP_PM_OPS(smsc375x_suspend,
				smsc375x_resume)
		SET_RUNTIME_PM_OPS(smsc375x_runtime_suspend,
				smsc375x_runtime_resume,
				smsc375x_runtime_idle)
};

static struct smsc375x_pdata smsc_drvdata = {
	.enable_vbus = NULL,
	.disable_vbus = NULL,
	.is_vbus_online = NULL,
	.charging_compliance_override = false,
};

static const struct i2c_device_id smsc375x_id[] = {
	{"smsc375x", (kernel_ulong_t)&smsc_drvdata},
	{"SMSC3750", (kernel_ulong_t)&smsc_drvdata},
	{}
};
MODULE_DEVICE_TABLE(i2c, smsc375x_id);

static const struct acpi_device_id acpi_smsc375x_id[] = {
	{"SMSC3750", (kernel_ulong_t)&smsc_drvdata},
	{}
};
MODULE_DEVICE_TABLE(acpi, acpi_smsc375x_id);

static struct i2c_driver smsc375x_i2c_driver = {
	.driver = {
		.name = "smsc375x",
		.owner	= THIS_MODULE,
		.pm	= &smsc375x_pm_ops,
		.acpi_match_table = ACPI_PTR(acpi_smsc375x_id),
	},
	.probe = smsc375x_probe,
	.remove = smsc375x_remove,
	.id_table = smsc375x_id,
	.shutdown = smsc375x_shutdown,
};

/*
 * Module stuff
 */

static int __init smsc375x_extcon_init(void)
{
	int ret = i2c_add_driver(&smsc375x_i2c_driver);
	return ret;
}
late_initcall(smsc375x_extcon_init);

static void __exit smsc375x_extcon_exit(void)
{
	i2c_del_driver(&smsc375x_i2c_driver);
}
module_exit(smsc375x_extcon_exit);

MODULE_AUTHOR("Ramakrishna Pallala <ramakrishna.pallala@intel.com>");
MODULE_DESCRIPTION("SMSC375x extcon driver");
MODULE_LICENSE("GPL");
