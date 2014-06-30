/*
 * extcon-tsu6111.c - TSU6111 extcon driver
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
#include <linux/delay.h>
#include <linux/extcon/extcon-tsu6111.h>

#define TSU_REG_CONTROL			0x02
#define TSU_REG_DEVICETYPE1		0x0A
#define TSU_REG_MANUALSW1		0x13


#define STAT_CHRG_TYPE_MASK		0xFC

#define TSU_CHARGE_CUR_DCP		2000
#define TSU_CHARGE_CUR_CDP		1500
#define TSU_CHARGE_CUR_SDP_100		100
#define TSU_CHARGE_CUR_SDP_500		500

#define TSU6111_EXTCON_USB		"USB"
#define TSU6111_EXTCON_SDP		"CHARGER_USB_SDP"
#define TSU6111_EXTCON_DCP		"CHARGER_USB_DCP"
#define TSU6111_EXTCON_CDP		"CHARGER_USB_CDP"

#define TSU6111_DET_SDP			0x04
#define TSU6111_DET_CDP			0x20
#define TSU6111_DET_DCP			0x40
#define TSU6111_DET_OTG			0x80

static const char *tsu6111_extcon_cable[] = {
	TSU6111_EXTCON_SDP,
	TSU6111_EXTCON_DCP,
	TSU6111_EXTCON_CDP,
	NULL,
};

struct tsu6111_chip {
	struct i2c_client	*client;
	struct tsu6111_pdata	*pdata;
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
	bool			a_bus_drop;
	bool			vbus_drive;
};

static struct tsu6111_chip *chip_ptr;

static int tsu6111_write_reg(struct i2c_client *client,
		int reg, int value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int tsu6111_read_reg(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int tsu6111_detect_dev(struct tsu6111_chip *chip)
{
	struct i2c_client *client = chip->client;
	static bool notify_otg, notify_charger;
	static char *cable;
	static struct power_supply_cable_props cable_props;
	int cfg, ret, vbus_mask = 0;
	u8 chrg_type;
	bool vbus_attach = false;

	dev_info(&chip->client->dev, "%s\n", __func__);
	/*
	 * get VBUS status from external IC like
	 * PMIC or Charger as TSU6111 chip can not
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
		tsu6111_write_reg(client, TSU_REG_MANUALSW1, 0x00);
		goto notify_otg_em;
	}

	/* dont proceed with charger detection in host mode */
	if (chip->id_short) {
		/*
		 * only after reading the status register
		 * MUX path is being closed. And by default
		 * MUX is to connected Host mode path.
		 */
		return ret;
	}

	mdelay(100);

	ret = tsu6111_read_reg(client, TSU_REG_DEVICETYPE1);
	if (ret < 0)
		goto dev_det_i2c_failed;
	else
		cfg = ret;

	dev_info(&client->dev, "Cfg:%x menual_sw:%x\n", cfg,
			tsu6111_read_reg(client, TSU_REG_MANUALSW1));

	chrg_type = cfg;
	chip->is_sdp = false;
	if (chrg_type == TSU6111_DET_SDP) {
		dev_info(&chip->client->dev,
				"SDP cable connecetd\n");
		notify_otg = true;
		vbus_mask = 1;
		notify_charger = true;
		chip->is_sdp = true;
		cable = TSU6111_EXTCON_SDP;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_SDP;
		if (chip->pdata->charging_compliance_override)
			cable_props.ma = TSU_CHARGE_CUR_SDP_500;
		else
			cable_props.ma = TSU_CHARGE_CUR_SDP_100;
	} else if (chrg_type == TSU6111_DET_CDP) {
		dev_info(&chip->client->dev,
				"CDP cable connecetd\n");
		notify_otg = true;
		vbus_mask = 1;
		notify_charger = true;
		cable = TSU6111_EXTCON_CDP;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_CDP;
		cable_props.ma = TSU_CHARGE_CUR_CDP;
	} else if (chrg_type == TSU6111_DET_DCP) {
		dev_info(&chip->client->dev,
				"DCP/SE1 cable connecetd\n");
		notify_charger = true;
		cable = TSU6111_EXTCON_DCP;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_DCP;
		cable_props.ma = TSU_CHARGE_CUR_DCP;
		if (!wake_lock_active(&chip->wakelock))
			wake_lock(&chip->wakelock);
	} else if (chrg_type == 0x80) {
		dev_info(&chip->client->dev,
                                "Enter USB host mode\n");
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
						USB_EVENT_VBUS, &vbus_mask);
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
		/* Let's see if what really needed is host mode */
		ret = tsu6111_read_reg(client, TSU_REG_DEVICETYPE1);
		if (ret < 0)
			goto dev_det_i2c_failed;
		if (ret == TSU6111_DET_OTG) { /* Enter host mode */
			atomic_notifier_call_chain(&chip->otg->notifier,
				USB_EVENT_ID, &vbus_mask);
		}
	} else {
		if (notify_otg) {
			/* close mux path to enable device mode */
			tsu6111_write_reg(client, TSU_REG_MANUALSW1, 0x6c);
			atomic_notifier_call_chain(&chip->otg->notifier,
						USB_EVENT_VBUS, &vbus_mask);
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

static irqreturn_t tsu6111_irq_handler(int irq, void *data)
{
	struct tsu6111_chip *chip = data;

	pm_runtime_get_sync(&chip->client->dev);

	dev_info(&chip->client->dev, "TSU USB INT!\n");

	tsu6111_detect_dev(chip);

	pm_runtime_put_sync(&chip->client->dev);
	return IRQ_HANDLED;
}

static void tsu6111_otg_event_worker(struct work_struct *work)
{
	struct tsu6111_chip *chip =
	    container_of(work, struct tsu6111_chip, otg_work);
	int ret;

	pm_runtime_get_sync(&chip->client->dev);

	if (chip->id_short) {
		ret = chip->pdata->enable_vbus();
		msleep(50);
		/* enable mux2 path for host */
		tsu6111_write_reg(chip->client, TSU_REG_MANUALSW1, 0x24);
	} else
		ret = chip->pdata->disable_vbus();
	if (ret < 0)
		dev_warn(&chip->client->dev, "id vbus control failed\n");

	pm_runtime_put_sync(&chip->client->dev);
}

static int tsu6111_handle_otg_notification(struct notifier_block *nb,
				   unsigned long event, void *param)
{
	struct tsu6111_chip *chip =
	    container_of(nb, struct tsu6111_chip, id_nb);
	struct power_supply_cable_props cable_props;
	int *val = (int *)param;

	if (!val || ((event != USB_EVENT_ID) &&
			(event != USB_EVENT_ENUMERATED)))
		return NOTIFY_DONE;

	dev_info(&chip->client->dev,
		"[OTG notification]evt:%lu val:%d\n", event, *val);

	switch (event) {
	case USB_EVENT_ID:
		/*
		 * in case of ID short(*id = 0)
		 * enable vbus else disable vbus.
		 */
		if (*val)
			chip->id_short = false;
		else
			chip->id_short = true;
		schedule_work(&chip->otg_work);
		break;
	case USB_EVENT_ENUMERATED:
		/*
		 * ignore cable plug/unplug events as TSU
		 * had already send those event notifications.
		 * Also only handle notifications for SDP case.
		 */
		/* No need to change SDP inlimit based on enumeration status
		 * if platform can voilate charging_compliance.
		 */
		if (chip->pdata->charging_compliance_override ||
			 !chip->is_sdp ||
			(*val == TSU_CHARGE_CUR_SDP_100))
			break;
		/*
		 * if current limit is < 100mA
		 * treat it as suspend event.
		 */
		if (*val < TSU_CHARGE_CUR_SDP_100)
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

static void tsu6111_pwrsrc_event_worker(struct work_struct *work)
{
	struct tsu6111_chip *chip =
	    container_of(work, struct tsu6111_chip, vbus_work);
	int ret;

	pm_runtime_get_sync(&chip->client->dev);

	ret = tsu6111_detect_dev(chip);

	pm_runtime_put_sync(&chip->client->dev);
}

static int tsu6111_handle_pwrsrc_notification(struct notifier_block *nb,
				   unsigned long event, void *param)
{
	struct tsu6111_chip *chip =
	    container_of(nb, struct tsu6111_chip, vbus_nb);

	dev_info(&chip->client->dev, "[PWRSRC notification]: %lu\n", event);

	schedule_work(&chip->vbus_work);

	return NOTIFY_OK;
}

static int tsu6111_irq_init(struct tsu6111_chip *chip)
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
	if (!id) {
		dev_err(dev, "%s: no acpi dev match\n", __func__);
		return -ENODEV;
	}
	gpio = devm_gpiod_get_index(dev, "tsu6111_int", 0);
	if (IS_ERR(gpio)) {
		dev_err(dev, "acpi gpio get index failed\n");
		return PTR_ERR(gpio);
	}
	ret = gpiod_to_irq(gpio);
	if (ret < 0) {
		dev_err(dev, "%s: invalid irq for the gpio\n", __func__);
		return ret;
	} else
		dev_err(dev, "%s: irq = %d\n", __func__, ret);

	/* get irq number */
	chip->client->irq = ret;
	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
				tsu6111_irq_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"tsu6111", chip);
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

static int tsu6111_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	const struct acpi_device_id *acpi_id;
	struct tsu6111_chip *chip;
	struct device *dev;
	int ret = 0, id_val = -1;
	int val = 0;

	chip = kzalloc(sizeof(struct tsu6111_chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	dev = &client->dev;

	if (!ACPI_HANDLE(dev)) {
		dev_err(&client->dev, "ACPI_HANDLE is invalid\n");
		return -ENODEV;
	}

	acpi_id = acpi_match_device(dev->driver->acpi_match_table, dev);
	if (!acpi_id) {
		dev_err(&client->dev, "null id while ACPI_HANDLE is valid\n");
		return -ENODEV;
	}

	chip->client = client;
	chip->pdata = (struct tsu6111_pdata *)acpi_id->driver_data;

	i2c_set_clientdata(client, chip);
	wake_lock_init(&chip->wakelock, WAKE_LOCK_SUSPEND,
						"tsu6111_wakelock");

	/* register with extcon */
	chip->edev = kzalloc(sizeof(struct extcon_dev), GFP_KERNEL);
	if (!chip->edev) {
		dev_err(&client->dev, "mem alloc failed\n");
		ret = -ENOMEM;
		goto extcon_mem_failed;
	}
	chip->edev->name = "tsu6111";
	chip->edev->supported_cable = tsu6111_extcon_cable;
	ret = extcon_dev_register(chip->edev);
	if (ret) {
		dev_err(&client->dev, "extcon registration failed!!\n");
		goto extcon_reg_failed;
	}

	/* register for EXTCON USB notification */
	INIT_WORK(&chip->vbus_work, tsu6111_pwrsrc_event_worker);
	chip->vbus_nb.notifier_call = tsu6111_handle_pwrsrc_notification;
	ret = extcon_register_interest(&chip->cable_obj, NULL,
			TSU6111_EXTCON_USB, &chip->vbus_nb);

	/* OTG notification */
	chip->otg = usb_get_phy(USB_PHY_TYPE_USB2);
	if (!chip->otg) {
		dev_warn(&client->dev, "Failed to get otg transceiver!!\n");
		goto otg_reg_failed;
	}

	INIT_WORK(&chip->otg_work, tsu6111_otg_event_worker);
	chip->id_nb.notifier_call = tsu6111_handle_otg_notification;
	ret = usb_register_notifier(chip->otg, &chip->id_nb);
	if (ret) {
		dev_err(&chip->client->dev,
			"failed to register otg notifier\n");
		goto id_reg_failed;
	}

	/* unmask interrupt */
	val = tsu6111_read_reg(chip->client, TSU_REG_CONTROL);
	val = val & 0xfe;
	tsu6111_write_reg(chip->client, TSU_REG_CONTROL, val);

	ret = tsu6111_irq_init(chip);
	if (ret) {
		/* In case of failure hardcode to USB device mode */
		val = tsu6111_read_reg(chip->client, TSU_REG_CONTROL);
		val = val & 0xFB;
		tsu6111_write_reg(chip->client, TSU_REG_CONTROL, val);
		tsu6111_write_reg(chip->client, TSU_REG_MANUALSW1, 0x6c);
		goto intr_reg_failed;
	}
	chip_ptr = chip;

	if (chip->otg->get_id_status) {
		ret = chip->otg->get_id_status(chip->otg, &id_val);
		if (ret < 0) {
			dev_warn(&client->dev,
				"otg get ID status failed:%d\n", ret);
			ret = 0;
		}
	}

	/* Set manual switching */
	val = tsu6111_read_reg(chip->client, TSU_REG_CONTROL);
	val = val & 0xFB;
	tsu6111_write_reg(chip->client, TSU_REG_CONTROL, val);
	/* open all switches, set correct status later by tsu6111_detect_dev */
	tsu6111_write_reg(chip->client, TSU_REG_MANUALSW1, 0x00);

	if (!id_val && !chip->id_short)
		atomic_notifier_call_chain(&chip->otg->notifier,
						USB_EVENT_ID, &id_val);
	else
		tsu6111_detect_dev(chip);

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
	wake_lock_destroy(&chip->wakelock);
	kfree(chip);
	return ret;
}

static int tsu6111_remove(struct i2c_client *client)
{
	struct tsu6111_chip *chip = i2c_get_clientdata(client);

	free_irq(client->irq, chip);
	usb_put_phy(chip->otg);
	extcon_dev_unregister(chip->edev);
	wake_lock_destroy(&chip->wakelock);
	kfree(chip->edev);
	pm_runtime_get_noresume(&chip->client->dev);
	kfree(chip);
	return 0;
}

static void tsu6111_shutdown(struct i2c_client *client)
{
	dev_dbg(&client->dev, "tsu6111 shutdown\n");

	if (client->irq > 0)
		disable_irq(client->irq);
	return;
}

static int tsu6111_suspend(struct device *dev)
{
	struct tsu6111_chip *chip = dev_get_drvdata(dev);

	if (chip->client->irq > 0)
		disable_irq(chip->client->irq);

	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int tsu6111_resume(struct device *dev)
{
	struct tsu6111_chip *chip = dev_get_drvdata(dev);

	if (chip->client->irq > 0)
		enable_irq(chip->client->irq);

	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int tsu6111_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int tsu6111_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int tsu6111_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static const struct dev_pm_ops tsu6111_pm_ops = {
		SET_SYSTEM_SLEEP_PM_OPS(tsu6111_suspend,
				tsu6111_resume)
		SET_RUNTIME_PM_OPS(tsu6111_runtime_suspend,
				tsu6111_runtime_resume,
				tsu6111_runtime_idle)
};

static struct tsu6111_pdata tsu_drvdata = {
#ifdef CONFIG_CHARGER_BQ24192
	.enable_vbus = bq24192_vbus_enable,
	.disable_vbus = bq24192_vbus_disable,
	.is_vbus_online = bq24192_vbus_status,
#else
	.enable_vbus = dummy_vubs_enable,
	.disable_vbus = dummy_vbus_disable,
	.is_vbus_online = dummy_vbus_status,
#endif
	.charging_compliance_override = true,
};

static const struct i2c_device_id tsu6111_id[] = {
	{"TSUM6111", (kernel_ulong_t)&tsu_drvdata},
	{}
};
MODULE_DEVICE_TABLE(i2c, tsu6111_id);

static const struct acpi_device_id acpi_tsu6111_id[] = {
	{"TSUM6111", (kernel_ulong_t)&tsu_drvdata},
	{}
};
MODULE_DEVICE_TABLE(acpi, acpi_tsu6111_id);

static struct i2c_driver tsu6111_i2c_driver = {
	.driver = {
		.name = "TSUM6111",
		.owner	= THIS_MODULE,
		.pm	= &tsu6111_pm_ops,
		.acpi_match_table = ACPI_PTR(acpi_tsu6111_id),
	},
	.probe = tsu6111_probe,
	.remove = tsu6111_remove,
	.id_table = tsu6111_id,
	.shutdown = tsu6111_shutdown,
};

static int __init tsu6111_extcon_init(void)
{
	int ret = i2c_add_driver(&tsu6111_i2c_driver);
	return ret;
}
late_initcall(tsu6111_extcon_init);

static void __exit tsu6111_extcon_exit(void)
{
	i2c_del_driver(&tsu6111_i2c_driver);
}
module_exit(tsu6111_extcon_exit);

MODULE_AUTHOR("Fei Yang <fei.yang@intel.com>");
MODULE_DESCRIPTION("TSU6111 extcon driver");
MODULE_LICENSE("GPL");
