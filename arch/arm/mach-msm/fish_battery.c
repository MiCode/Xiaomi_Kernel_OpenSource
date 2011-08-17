/* arch/arm/mach-msm/fish_battery.c
 *
 * Copyright (C) 2008 Google, Inc.
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
 * based on: arch/arm/mach-msm/htc_battery.c
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>

static enum power_supply_property fish_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
};

static enum power_supply_property fish_power_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_list[] = {
	"battery",
};

static int fish_power_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val);

static int fish_battery_get_property(struct power_supply *psy,
				     enum power_supply_property psp,
				     union power_supply_propval *val);

static struct power_supply fish_power_supplies[] = {
	{
		.name = "battery",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = fish_battery_properties,
		.num_properties = ARRAY_SIZE(fish_battery_properties),
		.get_property = fish_battery_get_property,
	},
	{
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.supplied_to = supply_list,
		.num_supplicants = ARRAY_SIZE(supply_list),
		.properties = fish_power_properties,
		.num_properties = ARRAY_SIZE(fish_power_properties),
		.get_property = fish_power_get_property,
	},
};

static int fish_power_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (psy->type == POWER_SUPPLY_TYPE_MAINS)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int fish_battery_get_property(struct power_supply *psy,
				     enum power_supply_property psp,
				     union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_FULL;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = 100;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int fish_battery_probe(struct platform_device *pdev)
{
	int i;
	int rc;

	/* init power supplier framework */
	for (i = 0; i < ARRAY_SIZE(fish_power_supplies); i++) {
		rc = power_supply_register(&pdev->dev, &fish_power_supplies[i]);
		if (rc)
			pr_err("%s: Failed to register power supply (%d)\n",
			       __func__, rc);
	}

	return 0;
}

static struct platform_driver fish_battery_driver = {
	.probe	= fish_battery_probe,
	.driver	= {
		.name	= "fish_battery",
		.owner	= THIS_MODULE,
	},
};

static int __init fish_battery_init(void)
{
	platform_driver_register(&fish_battery_driver);
	return 0;
}

module_init(fish_battery_init);
MODULE_DESCRIPTION("Qualcomm fish battery driver");
MODULE_LICENSE("GPL");

