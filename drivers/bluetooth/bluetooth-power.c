/* Copyright (c) 2009-2010, 2013-2016 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/*
 * Bluetooth Power Switch Module
 * controls power to external Bluetooth device
 * with interface to power management device
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/bluetooth-power.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <net/cnss.h>

#define BT_PWR_DBG(fmt, arg...)  pr_debug("%s: " fmt "\n" , __func__ , ## arg)
#define BT_PWR_INFO(fmt, arg...) pr_info("%s: " fmt "\n" , __func__ , ## arg)
#define BT_PWR_ERR(fmt, arg...)  pr_err("%s: " fmt "\n" , __func__ , ## arg)


static struct of_device_id bt_power_match_table[] = {
	{	.compatible = "qca,ar3002" },
	{	.compatible = "qca,qca6174" },
	{	.compatible = "qca,wcn3990" },
	{}
};

static struct bluetooth_power_platform_data *bt_power_pdata;
static struct platform_device *btpdev;
static bool previous;

static int bt_vreg_init(struct bt_power_vreg_data *vreg)
{
	int rc = 0;
	struct device *dev = &btpdev->dev;

	BT_PWR_DBG("vreg_get for : %s", vreg->name);

	/* Get the regulator handle */
	vreg->reg = regulator_get(dev, vreg->name);
	if (IS_ERR(vreg->reg)) {
		rc = PTR_ERR(vreg->reg);
		pr_err("%s: regulator_get(%s) failed. rc=%d\n",
			__func__, vreg->name, rc);
		goto out;
	}

	if ((regulator_count_voltages(vreg->reg) > 0)
			&& (vreg->low_vol_level) && (vreg->high_vol_level))
		vreg->set_voltage_sup = 1;

out:
	return rc;
}

static int bt_vreg_enable(struct bt_power_vreg_data *vreg)
{
	int rc = 0;

	BT_PWR_DBG("vreg_en for : %s", vreg->name);

	if (!vreg->is_enabled) {
		if (vreg->set_voltage_sup) {
			rc = regulator_set_voltage(vreg->reg,
						vreg->low_vol_level,
						vreg->high_vol_level);
			if (rc < 0) {
				BT_PWR_ERR("vreg_set_vol(%s) failed rc=%d\n",
						vreg->name, rc);
				goto out;
			}
		}

		if (vreg->load_uA >= 0) {
			rc = regulator_set_optimum_mode(vreg->reg,
					vreg->load_uA);
			if (rc < 0) {
				BT_PWR_ERR("vreg_set_mode(%s) failed rc=%d\n",
						vreg->name, rc);
				goto out;
			}
		}

		rc = regulator_enable(vreg->reg);
		if (rc < 0) {
			BT_PWR_ERR("regulator_enable(%s) failed. rc=%d\n",
					vreg->name, rc);
			goto out;
		}
		vreg->is_enabled = true;
	}
out:
	return rc;
}

static int bt_vreg_disable(struct bt_power_vreg_data *vreg)
{
	int rc = 0;

	if (!vreg)
		return rc;

	BT_PWR_DBG("vreg_disable for : %s", vreg->name);

	if (vreg->is_enabled) {
		rc = regulator_disable(vreg->reg);
		if (rc < 0) {
			BT_PWR_ERR("regulator_disable(%s) failed. rc=%d\n",
					vreg->name, rc);
			goto out;
		}
		vreg->is_enabled = false;

		if (vreg->set_voltage_sup) {
			/* Set the min voltage to 0 */
			rc = regulator_set_voltage(vreg->reg, 0,
					vreg->high_vol_level);
			if (rc < 0) {
				BT_PWR_ERR("vreg_set_vol(%s) failed rc=%d\n",
						vreg->name, rc);
				goto out;
			}
		}
		if (vreg->load_uA >= 0) {
			rc = regulator_set_optimum_mode(vreg->reg, 0);
			if (rc < 0) {
				BT_PWR_ERR("vreg_set_mode(%s) failed rc=%d\n",
						vreg->name, rc);
			}
		}
	}
out:
	return rc;
}

static int bt_configure_vreg(struct bt_power_vreg_data *vreg)
{
	int rc = 0;

	BT_PWR_DBG("config %s", vreg->name);

	/* Get the regulator handle for vreg */
	if (!(vreg->reg)) {
		rc = bt_vreg_init(vreg);
		if (rc < 0)
			return rc;
	}
	rc = bt_vreg_enable(vreg);

	return rc;
}

static int bt_configure_gpios(int on)
{
	int rc = 0;
	int bt_reset_gpio = bt_power_pdata->bt_gpio_sys_rst;

	BT_PWR_DBG("bt_gpio= %d on: %d", bt_reset_gpio, on);

	if (on) {
		rc = gpio_request(bt_reset_gpio, "bt_sys_rst_n");
		if (rc) {
			BT_PWR_ERR("unable to request gpio %d (%d)\n",
					bt_reset_gpio, rc);
			return rc;
		}

		rc = gpio_direction_output(bt_reset_gpio, 0);
		if (rc) {
			BT_PWR_ERR("Unable to set direction\n");
			return rc;
		}
		msleep(50);
		rc = gpio_direction_output(bt_reset_gpio, 1);
		if (rc) {
			BT_PWR_ERR("Unable to set direction\n");
			return rc;
		}
		msleep(50);
	} else {
		gpio_set_value(bt_reset_gpio, 0);
		msleep(100);
	}
	return rc;
}

static int bluetooth_power(int on)
{
	int rc = 0;

	BT_PWR_DBG("on: %d", on);

	if (on) {
		if (bt_power_pdata->bt_vdd_io) {
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_io);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddio config failed");
				goto out;
			}
		}
		if (bt_power_pdata->bt_vdd_xtal) {
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_xtal);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddxtal config failed");
				goto vdd_xtal_fail;
			}
		}
		if (bt_power_pdata->bt_vdd_core) {
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_core);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddcore config failed");
				goto vdd_core_fail;
			}
		}
		if (bt_power_pdata->bt_vdd_pa) {
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_pa);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddpa config failed");
				goto vdd_pa_fail;
			}
		}
		if (bt_power_pdata->bt_vdd_ldo) {
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_ldo);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddldo config failed");
				goto vdd_ldo_fail;
			}
		}
		if (bt_power_pdata->bt_chip_pwd) {
			rc = bt_configure_vreg(bt_power_pdata->bt_chip_pwd);
			if (rc < 0) {
				BT_PWR_ERR("bt_power chippwd config failed");
				goto chip_pwd_fail;
			}
		}
		if (bt_power_pdata->bt_gpio_sys_rst) {
			rc = bt_configure_gpios(on);
			if (rc < 0) {
				BT_PWR_ERR("bt_power gpio config failed");
				goto gpio_fail;
			}
		}
	} else {
		bt_configure_gpios(on);
gpio_fail:
		if (bt_power_pdata->bt_gpio_sys_rst)
			gpio_free(bt_power_pdata->bt_gpio_sys_rst);
		bt_vreg_disable(bt_power_pdata->bt_chip_pwd);
chip_pwd_fail:
		bt_vreg_disable(bt_power_pdata->bt_vdd_ldo);
vdd_ldo_fail:
		bt_vreg_disable(bt_power_pdata->bt_vdd_pa);
vdd_pa_fail:
		bt_vreg_disable(bt_power_pdata->bt_vdd_core);
vdd_core_fail:
		bt_vreg_disable(bt_power_pdata->bt_vdd_xtal);
vdd_xtal_fail:
		bt_vreg_disable(bt_power_pdata->bt_vdd_io);
	}
out:
	return rc;
}

static int bluetooth_toggle_radio(void *data, bool blocked)
{
	int ret = 0;
	int (*power_control)(int enable);

	power_control =
		((struct bluetooth_power_platform_data *)data)->bt_power_setup;

	if (previous != blocked)
		ret = (*power_control)(!blocked);
	if (!ret)
		previous = blocked;
	return ret;
}

static const struct rfkill_ops bluetooth_power_rfkill_ops = {
	.set_block = bluetooth_toggle_radio,
};

#if defined(CONFIG_CNSS) && defined(CONFIG_CLD_LL_CORE)
static ssize_t enable_extldo(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int ret;
	bool enable = false;
	struct cnss_platform_cap cap;

	ret = cnss_get_platform_cap(&cap);
	if (ret) {
		BT_PWR_ERR("Platform capability info from CNSS not available!");
		enable = false;
	} else if (!ret && (cap.cap_flag & CNSS_HAS_EXTERNAL_SWREG)) {
		enable = true;
	}
	return snprintf(buf, 6, "%s", (enable ? "true" : "false"));
}
#else
static ssize_t enable_extldo(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, 6, "%s", "false");
}
#endif

static DEVICE_ATTR(extldo, S_IRUGO, enable_extldo, NULL);

static int bluetooth_power_rfkill_probe(struct platform_device *pdev)
{
	struct rfkill *rfkill;
	int ret;

	rfkill = rfkill_alloc("bt_power", &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			      &bluetooth_power_rfkill_ops,
			      pdev->dev.platform_data);

	if (!rfkill) {
		dev_err(&pdev->dev, "rfkill allocate failed\n");
		return -ENOMEM;
	}

	/* add file into rfkill0 to handle LDO27 */
	ret = device_create_file(&pdev->dev, &dev_attr_extldo);
	if (ret < 0)
		BT_PWR_ERR("device create file error!");

	/* force Bluetooth off during init to allow for user control */
	rfkill_init_sw_state(rfkill, 1);
	previous = 1;

	ret = rfkill_register(rfkill);
	if (ret) {
		dev_err(&pdev->dev, "rfkill register failed=%d\n", ret);
		rfkill_destroy(rfkill);
		return ret;
	}

	platform_set_drvdata(pdev, rfkill);

	return 0;
}

static void bluetooth_power_rfkill_remove(struct platform_device *pdev)
{
	struct rfkill *rfkill;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	rfkill = platform_get_drvdata(pdev);
	if (rfkill)
		rfkill_unregister(rfkill);
	rfkill_destroy(rfkill);
	platform_set_drvdata(pdev, NULL);
}

#define MAX_PROP_SIZE 32
static int bt_dt_parse_vreg_info(struct device *dev,
		struct bt_power_vreg_data **vreg_data, const char *vreg_name)
{
	int len, ret = 0;
	const __be32 *prop;
	char prop_name[MAX_PROP_SIZE];
	struct bt_power_vreg_data *vreg;
	struct device_node *np = dev->of_node;

	BT_PWR_DBG("vreg dev tree parse for %s", vreg_name);

	snprintf(prop_name, MAX_PROP_SIZE, "%s-supply", vreg_name);
	if (of_parse_phandle(np, prop_name, 0)) {
		vreg = devm_kzalloc(dev, sizeof(*vreg), GFP_KERNEL);
		if (!vreg) {
			dev_err(dev, "No memory for vreg: %s\n", vreg_name);
			ret = -ENOMEM;
			goto err;
		}

		vreg->name = vreg_name;

		/* Parse voltage-level from each node */
		snprintf(prop_name, MAX_PROP_SIZE,
				"%s-voltage-level", vreg_name);
		prop = of_get_property(np, prop_name, &len);
		if (!prop || (len != (2 * sizeof(__be32)))) {
			dev_warn(dev, "%s %s property\n",
				prop ? "invalid format" : "no", prop_name);
		} else {
			vreg->low_vol_level = be32_to_cpup(&prop[0]);
			vreg->high_vol_level = be32_to_cpup(&prop[1]);
		}

		/* Parse current-level from each node */
		snprintf(prop_name, MAX_PROP_SIZE,
				"%s-current-level", vreg_name);
		ret = of_property_read_u32(np, prop_name, &vreg->load_uA);
		if (ret < 0) {
			BT_PWR_DBG("%s property is not valid\n", prop_name);
			vreg->load_uA = -1;
			ret = 0;
		}

		*vreg_data = vreg;
		BT_PWR_DBG("%s: vol=[%d %d]uV, current=[%d]uA\n",
			vreg->name, vreg->low_vol_level,
			vreg->high_vol_level,
			vreg->load_uA);
	} else
		BT_PWR_INFO("%s: is not provided in device tree", vreg_name);

err:
	return ret;
}

static int bt_power_populate_dt_pinfo(struct platform_device *pdev)
{
	int rc;

	BT_PWR_DBG("");

	if (!bt_power_pdata)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		bt_power_pdata->bt_gpio_sys_rst =
			of_get_named_gpio(pdev->dev.of_node,
						"qca,bt-reset-gpio", 0);
		if (bt_power_pdata->bt_gpio_sys_rst < 0) {
			BT_PWR_ERR("bt-reset-gpio not provided in device tree");
			return bt_power_pdata->bt_gpio_sys_rst;
		}
		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_core,
					"qca,bt-vdd-core");
		if (rc < 0)
			return rc;

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_io,
					"qca,bt-vdd-io");
		if (rc < 0)
			return rc;

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_xtal,
					"qca,bt-vdd-xtal");
		if (rc < 0)
			return rc;

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_pa,
					"qca,bt-vdd-pa");
		if (rc < 0)
			return rc;

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_ldo,
					"qca,bt-vdd-ldo");
		if (rc < 0)
			return rc;

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_chip_pwd,
					"qca,bt-chip-pwd");
		if (rc < 0)
			return rc;

	}

	bt_power_pdata->bt_power_setup = bluetooth_power;

	return 0;
}

static int bt_power_probe(struct platform_device *pdev)
{
	int ret = 0;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	bt_power_pdata =
		kzalloc(sizeof(struct bluetooth_power_platform_data),
			GFP_KERNEL);

	if (!bt_power_pdata) {
		BT_PWR_ERR("Failed to allocate memory");
		return -ENOMEM;
	}

	if (pdev->dev.of_node) {
		ret = bt_power_populate_dt_pinfo(pdev);
		if (ret < 0) {
			BT_PWR_ERR("Failed to populate device tree info");
			goto free_pdata;
		}
		pdev->dev.platform_data = bt_power_pdata;
	} else if (pdev->dev.platform_data) {
		/* Optional data set to default if not provided */
		if (!((struct bluetooth_power_platform_data *)
			(pdev->dev.platform_data))->bt_power_setup)
			((struct bluetooth_power_platform_data *)
				(pdev->dev.platform_data))->bt_power_setup =
						bluetooth_power;

		memcpy(bt_power_pdata, pdev->dev.platform_data,
			sizeof(struct bluetooth_power_platform_data));
	} else {
		BT_PWR_ERR("Failed to get platform data");
		goto free_pdata;
	}

	if (bluetooth_power_rfkill_probe(pdev) < 0)
		goto free_pdata;

	btpdev = pdev;

	return 0;

free_pdata:
	kfree(bt_power_pdata);
	return ret;
}

static int bt_power_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s\n", __func__);

	bluetooth_power_rfkill_remove(pdev);

	if (bt_power_pdata->bt_chip_pwd->reg)
		regulator_put(bt_power_pdata->bt_chip_pwd->reg);

	kfree(bt_power_pdata);

	return 0;
}

static struct platform_driver bt_power_driver = {
	.probe = bt_power_probe,
	.remove = bt_power_remove,
	.driver = {
		.name = "bt_power",
		.owner = THIS_MODULE,
		.of_match_table = bt_power_match_table,
	},
};

static int __init bluetooth_power_init(void)
{
	int ret;

	ret = platform_driver_register(&bt_power_driver);
	return ret;
}

static void __exit bluetooth_power_exit(void)
{
	platform_driver_unregister(&bt_power_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM Bluetooth power control driver");
MODULE_VERSION("1.40");

module_init(bluetooth_power_init);
module_exit(bluetooth_power_exit);
