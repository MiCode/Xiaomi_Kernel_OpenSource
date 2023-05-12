#define pr_fmt(fmt)	"[slg] %s: " fmt, __func__

#include <linux/slab.h> /* kfree() */
#include <linux/module.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/string.h>

#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>
#include <linux/random.h>

#ifdef CONFIG_BUILD_QGKI
#include "hqsys_pcba.h"
#include "w1_slg.h"
#endif

#define ds_info	pr_err
#define ds_dbg	pr_err
#define ds_err	pr_err
#define ds_log	pr_err

enum {
	FIRST_SUPPLIER,
	SECOND_SUPPLIER,
	THIRD_SUPPLIER,
	UNKNOW_SUPPLIER,
};

#ifdef CONFIG_BUILD_QGKI
static const char *battery_id_name[] = {
	"First supplier",
	"Second supplier",
	"Third supplier",
	"Unknow",
};
#endif

static int slg_probe(struct platform_device *pdev);
static int slg_remove(struct platform_device *pdev);
bool slg_Auth_Result_b;
#ifdef CONFIG_BUILD_QGKI
extern int authenticate_battery(void);
extern PCBA_CONFIG get_huaqin_pcba_config(void);
#endif
struct mutex slg_cmd_lock;
struct slg_data {
	struct platform_device *pdev;
	struct device *dev;
	int version;
	struct power_supply *verify_psy;
	struct power_supply_desc verify_psy_d;
	struct delayed_work	authentic_work;
};

int slgbatid = UNKNOW_SUPPLIER;

static struct of_device_id slg_dt_match[] = {
	{ .compatible = "maxim,slg", },
	{}
};
static ssize_t slg_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	if(slg_Auth_Result_b){
		return scnprintf(buf, PAGE_SIZE, "Authenticate success.\n");
	} else {
		return scnprintf(buf, PAGE_SIZE, "Authenticate failed.\n");
	}
}

static DEVICE_ATTR(slg, S_IRUGO, slg_show, NULL);

static struct attribute *attributes[] = {
	&dev_attr_slg.attr,
	NULL
};
static const struct attribute_group attribute_group = {
	.attrs = attributes,
};
static struct platform_driver slg_driver =
{
	.driver = {
		.owner = THIS_MODULE,
		.name = "slg_battery_secrete",
		.of_match_table = slg_dt_match,
	},
	.probe = slg_probe,
	.remove = slg_remove,
};

#ifdef CONFIG_BUILD_QGKI
static enum power_supply_property verify_props[] = {
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_MODEL_NAME,
};

static int verify_get_property(struct power_supply *psy, enum power_supply_property psp,
					union power_supply_propval *val)
{
	//int ret;
	//unsigned int cycle_count;
	//unsigned char pagedata[16] = {0x00};
	//unsigned char buf[50];

	switch (psp) {
	case POWER_SUPPLY_PROP_AUTHENTIC:
		val->intval = slg_Auth_Result_b;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = battery_id_name[slgbatid];
		break;
	default:
		ds_err("unsupported property %d\n", psp);
		return -ENODATA;
	}

	return 0;
}

static int verify_set_property(struct power_supply *psy,
			       enum power_supply_property prop,
			       const union power_supply_propval *val)
{
	//int ret;
	//unsigned char buf[50];

	switch (prop) {
	default:
		ds_err("unsupported property %d\n", prop);
		return -ENODATA;
	}

	return 0;
}

static int verify_prop_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int verify_psy_register(struct slg_data *ds)
{
	struct power_supply_config verify_psy_cfg = {};

	ds->verify_psy_d.name = "batt_verify";
	ds->verify_psy_d.type = POWER_SUPPLY_TYPE_BATT_VERIFY;
	ds->verify_psy_d.properties = verify_props;
	ds->verify_psy_d.num_properties = ARRAY_SIZE(verify_props);
	ds->verify_psy_d.get_property = verify_get_property;
	ds->verify_psy_d.set_property = verify_set_property;
	ds->verify_psy_d.property_is_writeable = verify_prop_is_writeable;

	verify_psy_cfg.drv_data = ds;
	verify_psy_cfg.of_node = ds->dev->of_node;
	verify_psy_cfg.num_supplicants = 0;
	ds->verify_psy = devm_power_supply_register(ds->dev,
						&ds->verify_psy_d,
						&verify_psy_cfg);
	if (IS_ERR(ds->verify_psy)) {
		ds_err("Failed to register verify_psy");
		return PTR_ERR(ds->verify_psy);
	}

	ds_log("%s power supply register successfully\n", ds->verify_psy_d.name);
	return 0;
}

static void verify_psy_unregister(struct slg_data *ds)
{
	power_supply_unregister(ds->verify_psy);
}
#endif

// parse dts
static int slg_parse_dt(struct device *dev,
				struct slg_data *pdata)
{
	int error, val;
	struct device_node *np = dev->of_node;

	// parse version
	pdata->version = 0;
	error = of_property_read_u32(np, "slg,version", &val);
	if (error && (error != -EINVAL))
		ds_err("Unable to read bootloader address\n");
	else if (error != -EINVAL)
		pdata->version = val;

	return 0;
}

#ifdef CONFIG_BUILD_QGKI
static int authentic_period_ms = 5000;
#define AUTHENTIC_COUNT_MAX 5
int retry_authentic_times = 0;
#endif

static void authentic_work(struct work_struct *work)
{
#ifdef CONFIG_BUILD_QGKI
	//int rc;
	union power_supply_propval pval = {0,};

	struct slg_data *slg_data = container_of(work,
				struct slg_data,
				authentic_work.work);

	/*rc = power_supply_get_property(ds28e16_data->verify_psy,
					POWER_SUPPLY_PROP_AUTHEN_RESULT, &pval);*/
	pval.intval = authenticate_battery();
	ds_log("[Loren3]authentic result is %d\n", pval.intval);
	if (pval.intval != 0) {
		retry_authentic_times++;
		if (retry_authentic_times < AUTHENTIC_COUNT_MAX) {
			ds_log("battery authentic work begin to restart.\n");
			schedule_delayed_work(&slg_data->authentic_work,
				msecs_to_jiffies(authentic_period_ms));
		}

		if (retry_authentic_times == AUTHENTIC_COUNT_MAX) {
			ds_log("[Loren1]authentic result is %d\n", pval.intval);
			slg_Auth_Result_b = false;
			slgbatid = UNKNOW_SUPPLIER;
		} 
	} else {
			ds_log("[Loren2]authentic result is %d\n", pval.intval);
			slg_Auth_Result_b = true;
			slgbatid = THIRD_SUPPLIER;
	}
#endif
}

static int slg_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct slg_data *slg_data;

#ifdef CONFIG_BUILD_QGKI
	if ((get_huaqin_pcba_config() >= PCBA_UNKNOW) && (get_huaqin_pcba_config() <= PCBA_END) && (get_huaqin_pcba_config() % 0x10 != 3)){
		ds_dbg("Loren:No compatable phone!\n");
		return -ERANGE;
	}
#endif

    //mutex_init(&slg_cmd_lock);

	ds_log("%s entry.", __func__);
	ds_dbg("platform_device is %s", pdev->name);
	if (strcmp(pdev->name, "soc:maxim_slg") != 0)
	{
		ds_log("Loren debug strcmp.");
		return -ENODEV;
	}
		
	if (!pdev->dev.of_node || !of_device_is_available(pdev->dev.of_node))
	{
		ds_log("Loren debug of_device_is_available.");
		return -ENODEV;
	}

	if (pdev->dev.of_node) {
		slg_data = devm_kzalloc(&pdev->dev,
			sizeof(struct slg_data),
			GFP_KERNEL);
		if (!slg_data) {
			ds_err("Failed to allocate memory\n");
			return -ENOMEM;
		}

		retval = slg_parse_dt(&pdev->dev, slg_data);
		if (retval) {
			retval = -EINVAL;
			goto slg_parse_dt_err;
		}
	} else {
		slg_data = pdev->dev.platform_data;
	}

	if (!slg_data) {
		ds_err("No platform data found\n");
		return -EINVAL;
	}
	slg_data->dev = &pdev->dev;
	slg_data->pdev = pdev;
	platform_set_drvdata(pdev, slg_data);
	INIT_DELAYED_WORK(&slg_data->authentic_work, authentic_work);

#ifdef CONFIG_BUILD_QGKI
	retval = verify_psy_register(slg_data);
	if (retval) {
		ds_err("Failed to verify_psy_register, err:%d\n", retval);
		goto slg_psy_register_err;
	}
#endif
	retval = sysfs_create_group(&slg_data->dev->kobj, &attribute_group);
	if (retval) {
		ds_err("Failed to register sysfs, err:%d\n", retval);
		goto slg_create_group_err;
	}

	ds_log("Loren authenticate_battery start.");
#ifdef CONFIG_BUILD_QGKI
	retval =	authenticate_battery();
	if (retval != 0) {
		ds_log("Loren authenticate_battery failed,create schedule_delayed_work.");
		schedule_delayed_work(&slg_data->authentic_work,
				msecs_to_jiffies(500));
	}
#endif
	return 0;

slg_create_group_err:
dev_set_drvdata(slg_data->dev, NULL);
slg_parse_dt_err:
	kfree(slg_data);
#ifdef CONFIG_BUILD_QGKI
slg_psy_register_err:
	verify_psy_unregister(slg_data);
#endif
	return retval;
}

static 	int slg_remove(struct platform_device *pdev)
{
	struct slg_data *slg_data = platform_get_drvdata(pdev);

	kfree(slg_data);
	return 0;
}

static int slg_init(void)
{
	printk("slg_init \n");
	return platform_driver_register(&slg_driver);
}

static void slg_exit(void)
{
	printk("slg_exit \n");
	platform_driver_unregister(&slg_driver);
	return;
}

module_init(slg_init);
module_exit(slg_exit);

MODULE_AUTHOR("HQ Inc.");
MODULE_DESCRIPTION("slg driver");
MODULE_LICENSE("GPL");