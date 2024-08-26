#include <linux/module.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>

#include "battery_auth_class.h"

enum {
	MAIN_SUPPLY = 0,
	SECEON_SUPPLY,
	THIRD_SUPPLY,
	MAX_SUPPLY,
};

static const char *auth_device_name[] = {
	"main_suppiler",
	"second_supplier",
	"third_supplier",
	"unknown",
};

struct auth_data {
	struct auth_device *auth_dev[MAX_SUPPLY];

	struct power_supply *verify_psy;
	struct power_supply_desc desc;

	struct delayed_work dwork;

	bool auth_result;
	u8 batt_id;
	struct batt_info_dev *batt_info;
};

static struct auth_data *g_info;

static enum power_supply_property verify_props[] = {
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static int verify_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_AUTHENTIC:
		val->intval = true;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "unknown";
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#define AUTHENTIC_COUNT_MAX 3

int batt_auth_get_batt_id(struct batt_info_dev *batt_info)
{
	return 0;
}

char* batt_auth_get_batt_name(struct batt_info_dev *batt_info)
{
	return battery_name_txt[BATTERY_VENDOR_UNKNOW];
}

int batt_auth_get_chip_ok(struct batt_info_dev *batt_info)
{
	return true;
}

struct batt_info_ops batt_info_ops = {
	.get_batt_id = batt_auth_get_batt_id,
	.get_batt_name = batt_auth_get_batt_name,
	.get_chip_ok = batt_auth_get_chip_ok,
};

static int __init auth_battery_init(void)
{
	int i = 0;
	struct auth_data *info;
	struct power_supply_config cfg = { };

	pr_info("%s enter\n", __func__);

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	for (i = 0; i < MAX_SUPPLY; i++) {
		info->auth_dev[i] = get_batt_auth_by_name(auth_device_name[i]);
		if (!info->auth_dev[i])
			break;
	}
	cfg.drv_data = info;
	info->desc.name = "batt_verify";
	info->desc.type = POWER_SUPPLY_TYPE_BATTERY;
	info->desc.properties = verify_props;
	info->desc.num_properties = ARRAY_SIZE(verify_props);
	info->desc.get_property = verify_get_property;
	info->verify_psy =
	    power_supply_register(NULL, &(info->desc), &cfg);
	if (!(info->verify_psy)) {
		pr_err("%s register verify psy fail\n", __func__);
	}
	
	info->auth_result = true;
	pr_err("%s probe success\n", __func__);
	return 0;
}

static void __exit auth_battery_exit(void)
{

	power_supply_unregister(g_info->verify_psy);

	kfree(g_info);
}

module_init(auth_battery_init);
module_exit(auth_battery_exit);
MODULE_LICENSE("GPL");