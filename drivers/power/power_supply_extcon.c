/*
 * power_supply_extcon: Power supply detection through extcon.
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 * Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/power_supply_extcon.h>
#include <linux/slab.h>
#include <linux/extcon.h>

#define CHARGER_TYPE_DETECTION_DEFAULT_DEBOUNCE_TIME_MS		500

struct power_supply_extcon {
	struct device				*dev;
	struct extcon_dev			*edev;
	struct power_supply			ac;
	struct power_supply			usb;
	uint8_t					ac_online;
	uint8_t					usb_online;
	struct power_supply_extcon_plat_data	*pdata;
};

struct power_supply_cables {
	const char *name;
	long int event;
	struct power_supply_extcon	*psy_extcon;
	struct notifier_block nb;
	struct extcon_specific_cable_nb *extcon_dev;
	struct delayed_work extcon_notifier_work;
};

static struct power_supply_cables psy_cables[] = {
	{
		.name	= "USB",
	},
	{
		.name	= "TA",
	},
	{
		.name	= "Fast-charger",
	},
	{
		.name	= "Slow-charger",
	},
	{
		.name	= "Charge-downstream",
	},
};

static enum power_supply_property power_supply_extcon_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int power_supply_extcon_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	int online;
	int ret = 0;
	struct power_supply_extcon *psy_extcon;

	if (psy->type == POWER_SUPPLY_TYPE_MAINS) {
		psy_extcon = container_of(psy, struct power_supply_extcon, ac);
		online = psy_extcon->ac_online;
	} else if (psy->type == POWER_SUPPLY_TYPE_USB) {
		psy_extcon = container_of(psy, struct power_supply_extcon, usb);
		online = psy_extcon->usb_online;
	} else {
		return -EINVAL;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = online;
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static int power_supply_extcon_remove_cable(
		struct power_supply_extcon *psy_extcon,
		struct extcon_dev *edev)
{
	dev_info(psy_extcon->dev, "Charging cable removed\n");

	psy_extcon->ac_online = 0;
	psy_extcon->usb_online = 0;
	power_supply_changed(&psy_extcon->usb);
	power_supply_changed(&psy_extcon->ac);
	return 0;
}

static int power_supply_extcon_attach_cable(
		struct power_supply_extcon *psy_extcon,
		struct extcon_dev *edev)
{
	psy_extcon->usb_online = 0;
	psy_extcon->ac_online = 0;

	if (true == extcon_get_cable_state(edev, "USB")) {
		psy_extcon->usb_online = 1;
		dev_info(psy_extcon->dev, "USB charger cable detected\n");
	} else if (true == extcon_get_cable_state(edev, "Charge-downstream")) {
		psy_extcon->usb_online = 1;
		dev_info(psy_extcon->dev,
			"USB charger downstream cable detected\n");
	} else if (true == extcon_get_cable_state(edev, "TA")) {
		psy_extcon->ac_online = 1;
		dev_info(psy_extcon->dev, "USB TA cable detected\n");
	} else if (true == extcon_get_cable_state(edev, "Fast-charger")) {
		psy_extcon->ac_online = 1;
		dev_info(psy_extcon->dev, "USB Fast-charger cable detected\n");
	} else if (true == extcon_get_cable_state(edev, "Slow-charger")) {
		psy_extcon->ac_online = 1;
		dev_info(psy_extcon->dev, "USB Slow-charger cable detected\n");
	} else {
		dev_info(psy_extcon->dev, "Unknown cable detected\n");
	}

	power_supply_changed(&psy_extcon->usb);
	power_supply_changed(&psy_extcon->ac);
	return 0;
}

static void psy_extcon_extcon_handle_notifier(struct work_struct *w)
{
	struct power_supply_cables *cable = container_of(to_delayed_work(w),
			struct power_supply_cables, extcon_notifier_work);
	struct power_supply_extcon *psy_extcon = cable->psy_extcon;
	struct extcon_dev *edev = cable->extcon_dev->edev;

	if (cable->event == 0)
		power_supply_extcon_remove_cable(psy_extcon, edev);
	else if (cable->event == 1)
		power_supply_extcon_attach_cable(psy_extcon, edev);
}

static int psy_extcon_extcon_notifier(struct notifier_block *self,
		unsigned long event, void *ptr)
{
	struct power_supply_cables *cable = container_of(self,
		struct power_supply_cables, nb);

	cable->event = event;
	cancel_delayed_work(&cable->extcon_notifier_work);
	schedule_delayed_work(&cable->extcon_notifier_work,
	    msecs_to_jiffies(CHARGER_TYPE_DETECTION_DEFAULT_DEBOUNCE_TIME_MS));

	return NOTIFY_DONE;
}

static __devinit int psy_extcon_probe(struct platform_device *pdev)
{
	int ret = 0;
	uint8_t j;
	struct power_supply_extcon *psy_extcon;
	struct power_supply_extcon_plat_data *pdata = pdev->dev.platform_data;

	if (!pdata) {
		dev_err(&pdev->dev, "No platform data, exiting..\n");
		return -ENODEV;
	}

	psy_extcon = devm_kzalloc(&pdev->dev, sizeof(*psy_extcon), GFP_KERNEL);
	if (!psy_extcon) {
		dev_err(&pdev->dev, "failed to allocate memory status\n");
		return -ENOMEM;
	}

	psy_extcon->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, psy_extcon);

	psy_extcon->ac.name		= "ac";
	psy_extcon->ac.type		= POWER_SUPPLY_TYPE_MAINS;
	psy_extcon->ac.get_property	= power_supply_extcon_get_property;
	psy_extcon->ac.properties	= power_supply_extcon_props;
	psy_extcon->ac.num_properties	= ARRAY_SIZE(power_supply_extcon_props);
	ret = power_supply_register(psy_extcon->dev, &psy_extcon->ac);
	if (ret) {
		dev_err(psy_extcon->dev, "failed: power supply register\n");
		return ret;
	}

	psy_extcon->usb			= psy_extcon->ac;
	psy_extcon->usb.name		= "usb";
	psy_extcon->usb.type		= POWER_SUPPLY_TYPE_USB;
	ret = power_supply_register(psy_extcon->dev, &psy_extcon->usb);
	if (ret) {
		dev_err(psy_extcon->dev, "failed: power supply register\n");
		goto pwr_sply_error;
	}

	for (j = 0 ; j < ARRAY_SIZE(psy_cables); j++) {
		struct power_supply_cables *cable = &psy_cables[j];

		cable->extcon_dev =  devm_kzalloc(&pdev->dev,
					sizeof(struct extcon_specific_cable_nb),
					GFP_KERNEL);
		if (!cable->extcon_dev) {
			dev_err(&pdev->dev, "Malloc for extcon_dev failed\n");
			goto econ_err;
		}

		INIT_DELAYED_WORK(&cable->extcon_notifier_work,
					psy_extcon_extcon_handle_notifier);

		cable->psy_extcon = psy_extcon;
		cable->nb.notifier_call = psy_extcon_extcon_notifier;

		ret = extcon_register_interest(cable->extcon_dev,
				pdata->extcon_name,
				cable->name, &cable->nb);
		if (ret < 0)
			dev_err(psy_extcon->dev, "Cannot register for cable: %s\n",
					cable->name);
	}

	psy_extcon->edev = extcon_get_extcon_dev(pdata->extcon_name);
	if (!psy_extcon->edev)
			goto econ_err;

	power_supply_extcon_attach_cable(psy_extcon, psy_extcon->edev);
	dev_info(&pdev->dev, "%s() get success\n", __func__);
	return 0;

econ_err:
	power_supply_unregister(&psy_extcon->usb);
pwr_sply_error:
	power_supply_unregister(&psy_extcon->ac);
	return ret;
}

static int __devexit psy_extcon_remove(struct platform_device *pdev)
{
	struct power_supply_extcon *psy_extcon = platform_get_drvdata(pdev);

	power_supply_unregister(&psy_extcon->ac);
	power_supply_unregister(&psy_extcon->usb);
	return 0;
}

static struct platform_driver power_supply_extcon_driver = {
	.driver = {
		.name = "power-supply-extcon",
		.owner = THIS_MODULE,
	},
	.probe = psy_extcon_probe,
	.remove = __devexit_p(psy_extcon_remove),

};

static int __init psy_extcon_init(void)
{
	return platform_driver_register(&power_supply_extcon_driver);
}

static void __exit psy_extcon_exit(void)
{
	platform_driver_unregister(&power_supply_extcon_driver);
}

late_initcall(psy_extcon_init);
module_exit(psy_extcon_exit);

MODULE_DESCRIPTION("Power supply detection through extcon driver");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL v2");
