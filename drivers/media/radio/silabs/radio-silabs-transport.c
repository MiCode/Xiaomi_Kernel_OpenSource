/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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


#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include "radio-silabs-transport.h"

#define DRIVER_NAME "silabs,si4705"

static struct fm_i2c_device_data *device_data;

int get_int_gpio_number(void)
{
	int fm_int_gpio = 0;

	if (device_data)
		fm_int_gpio  = device_data->int_gpio;
	return fm_int_gpio;
}

int silabs_fm_i2c_read(u8 *buf, u8 len)
{
	int i = 0, retval = 0;

	struct i2c_msg msgs[1];

	if (unlikely(buf == NULL)) {
		FMDERR("%s:buf is null", __func__);
		return -EINVAL;
	}

	msgs[0].addr = device_data->client->addr;
	msgs[0].len = len;
	msgs[0].flags = I2C_M_RD;
	msgs[0].buf = (void *)buf;

	for (i = 0; i < 2; i++)	{
		retval = i2c_transfer(device_data->client->adapter, msgs, 1);
		if (retval == 1)
			break;
	}

	return retval;
}


int silabs_fm_i2c_write(u8 *buf, u8 len)
{
	struct i2c_msg msgs[1];
	int i = 0, retval = 0;

	if (unlikely(buf == NULL)) {
		FMDERR("%s:buf is null", __func__);
		return -EINVAL;
	}

	msgs[0].addr = device_data->client->addr;
	msgs[0].len = len;
	msgs[0].flags = 0;
	msgs[0].buf = (u8 *)buf;

	for (i = 0; i < 2; i++)	{
		retval = i2c_transfer(device_data->client->adapter, msgs, 1);
		if (retval == 1)
			break;
	}

	return retval;
}

int silabs_fm_pinctrl_init(void)
{
	int retval = 0;

	device_data->fm_pinctrl = devm_pinctrl_get(&device_data->client->dev);
	if (IS_ERR_OR_NULL(device_data->fm_pinctrl)) {
		FMDERR("%s: target does not use pinctrl\n", __func__);
		retval = PTR_ERR(device_data->fm_pinctrl);
		return retval;
	}

	device_data->gpio_state_active
		= pinctrl_lookup_state(device_data->fm_pinctrl,
					"pmx_fm_active");
	if (IS_ERR_OR_NULL(device_data->gpio_state_active)) {
		FMDERR("%s: cannot get FM active state\n", __func__);
		retval = PTR_ERR(device_data->gpio_state_active);
		goto err_active_state;
	}

	device_data->gpio_state_suspend
		= pinctrl_lookup_state(device_data->fm_pinctrl,
					"pmx_fm_suspend");
	if (IS_ERR_OR_NULL(device_data->gpio_state_suspend)) {
		FMDERR("%s: cannot get FM suspend state\n", __func__);
		retval = PTR_ERR(device_data->gpio_state_suspend);
		goto err_suspend_state;
	}

	return retval;

err_suspend_state:
	device_data->gpio_state_suspend = 0;

err_active_state:
	device_data->gpio_state_active = 0;

	devm_pinctrl_put(device_data->fm_pinctrl);

	return retval;
}

int silabs_fm_pinctrl_select(bool on)
{
	struct pinctrl_state *pins_state;
	int ret;

	pins_state = on ? device_data->gpio_state_active
		: device_data->gpio_state_suspend;

	if (!IS_ERR_OR_NULL(pins_state)) {
		ret = pinctrl_select_state(device_data->fm_pinctrl, pins_state);
		if (ret) {
			FMDERR("%s: cannot set pin state\n", __func__);
			return ret;
		}
	} else
		FMDERR("%s: not a valid %s pin state\n", __func__,
			on ? "pmx_fm_active" : "pmx_fm_suspend");

	return 0;
}

static int fm_configure_gpios(int on)
{
	int rc = 0;
	int fm_reset_gpio = device_data->reset_gpio;
	int fm_int_gpio = device_data->int_gpio;
	int fm_status_gpio = device_data->status_gpio;

	if (on) {
		/* Turn ON sequence
		 * GPO1/status gpio configuration.
		 * Keep the GPO1 to high till device comes out of reset.
		 */
		if (fm_status_gpio > 0) {
			FMDERR("status gpio is provided, setting it to high\n");

			rc = gpio_direction_output(fm_status_gpio, 1);
			if (rc) {
				pr_err("unable to set the gpio %d direction(%d)\n",
						fm_status_gpio, rc);
				return rc;
			}
			/* Wait for the value to take effect on gpio. */
			msleep(100);
		}

		/* GPO2/Interrupt gpio configuration.
		 * Keep the GPO2 to low till device comes out of reset.
		 */
		rc = gpio_direction_output(fm_int_gpio, 0);
		if (rc) {
			pr_err("unable to set the gpio %d direction(%d)\n",
					fm_int_gpio, rc);
			return rc;
		}
		/* Wait for the value to take effect on gpio. */
		msleep(100);

		/* Reset pin configuration.
		 * write "0'' to make sure the chip is in reset.
		 */
		rc = gpio_direction_output(fm_reset_gpio, 0);
		if (rc) {
			pr_err("Unable to set direction\n");
			return rc;
		}
		/* Wait for the value to take effect on gpio. */
		msleep(100);
		/* write "1" to bring the chip out of reset.*/
		rc = gpio_direction_output(fm_reset_gpio, 1);
		if (rc) {
			pr_err("Unable to set direction\n");
			return rc;
		}
		/* Wait for the value to take effect on gpio. */
		msleep(100);

		rc = gpio_direction_input(fm_int_gpio);
		if (rc) {
			pr_err("unable to set the gpio %d direction(%d)\n",
					fm_int_gpio, rc);
			return rc;
		}

	} else {
		/*Turn OFF sequence */
		gpio_set_value(fm_reset_gpio, 0);

		rc = gpio_direction_input(fm_reset_gpio);
		if (rc)
			pr_err("Unable to set direction\n");
		/* Wait for some time for the value to take effect. */
		msleep(100);

		if (fm_status_gpio > 0) {
			rc = gpio_direction_input(fm_status_gpio);
			if (rc)
				pr_err("Unable to set dir for status gpio\n");
			msleep(100);
		}
	}
	return rc;
}

int silabs_fm_power_cfg(int on)
{
	int rc = 0;
	struct fm_power_vreg_data *vreg;
	if (on) {
		/* Turn ON sequence */
		vreg = device_data->dreg;
		if (!vreg)
			FMDERR("In %s, dreg is NULL\n", __func__);
		else {
			FMDBG("vreg is : %s", vreg->name);
			if (!vreg->is_enabled) {
				if (vreg->set_voltage_sup) {
					rc = regulator_set_voltage(vreg->reg,
							vreg->low_vol_level,
							vreg->high_vol_level);
					if (rc < 0) {
						FMDERR("set_vol(%s) fail %d\n",
								vreg->name, rc);
						return rc;
					}
				}

				rc = regulator_enable(vreg->reg);
				if (rc < 0) {
					FMDERR("reg enable(%s) failed. rc=%d\n",
							vreg->name, rc);
					return rc;
				}
				vreg->is_enabled = true;
			}
		}

		vreg = device_data->areg;
		if (!vreg)
			FMDERR("In %s, areg is NULL\n", __func__);
		else {
			FMDBG("vreg is : %s", vreg->name);
			if (!vreg->is_enabled) {
				if (vreg->set_voltage_sup) {
					rc = regulator_set_voltage(vreg->reg,
							vreg->low_vol_level,
							vreg->high_vol_level);
					if (rc < 0) {
						FMDERR("set_vol(%s) fail %d\n",
								vreg->name, rc);
						return rc;
					}
				}

				rc = regulator_enable(vreg->reg);
				if (rc < 0) {
					FMDERR("reg enable(%s) failed. rc=%d\n",
							vreg->name, rc);
					return rc;
				}
				vreg->is_enabled = true;
			}
		}

		/* If pinctrl is supported, select active state */
		if (device_data->fm_pinctrl) {
			rc = silabs_fm_pinctrl_select(true);
			if (rc)
				FMDERR("%s: error setting active pin state\n",
					__func__);
		}

		rc = fm_configure_gpios(on);
		if (rc < 0) {
			FMDERR("fm_power gpio config failed\n");
			return rc;
		}
	} else {
		/* Turn OFF sequence */
		vreg = device_data->dreg;
		if (!vreg)
			FMDERR("In %s, dreg is NULL\n", __func__);
		else {
			if (vreg->is_enabled) {
				rc = regulator_disable(vreg->reg);
				if (rc < 0) {
					FMDERR("reg disable(%s) fail. rc=%d\n",
							vreg->name, rc);
					return rc;
				}
				vreg->is_enabled = false;

				if (vreg->set_voltage_sup) {
					/* Set the min voltage to 0 */
					rc = regulator_set_voltage(vreg->reg,
							0,
							vreg->high_vol_level);
					if (rc < 0) {
						FMDERR("set_vol(%s) fail %d\n",
								vreg->name, rc);
						return rc;
					}
				}
			}
		}

		vreg = device_data->areg;
		if (!vreg)
			FMDERR("In %s, areg is NULL\n", __func__);
		else {
			if (vreg->is_enabled) {
				rc = regulator_disable(vreg->reg);
				if (rc < 0) {
					FMDERR("reg disable(%s) fail rc=%d\n",
							vreg->name, rc);
					return rc;
				}
				vreg->is_enabled = false;

				if (vreg->set_voltage_sup) {
					/* Set the min voltage to 0 */
					rc = regulator_set_voltage(vreg->reg,
							0,
							vreg->high_vol_level);
					if (rc < 0) {
						pr_err("set_vol(%s) fail %d\n",
								vreg->name, rc);
						return rc;
					}
				}
			}
		}

		rc = fm_configure_gpios(on);
		if (rc < 0) {
			pr_err("fm_power gpio config failed");
			return rc;
		}

		/* If pinctrl is supported, select suspend state */
		if (device_data->fm_pinctrl) {
			rc = silabs_fm_pinctrl_select(false);
			if (rc)
				FMDERR("%s: error setting suspend pin state\n",
					__func__);
		}
	}
	return rc;
}

static int silabs_parse_dt(struct device *dev,
			struct fm_i2c_device_data *device_data)
{
	int rc = 0;
	struct device_node *np = dev->of_node;
	device_data->reset_gpio = of_get_named_gpio(np,
						"silabs,reset-gpio", 0);
	if (device_data->reset_gpio < 0) {
		pr_err("silabs-reset-gpio not provided in device tree");
		return device_data->reset_gpio;
	}

	rc = gpio_request(device_data->reset_gpio, "fm_rst_gpio_n");
	if (rc) {
		pr_err("unable to request gpio %d (%d)\n",
				device_data->reset_gpio, rc);
		return rc;
	}

	device_data->int_gpio = of_get_named_gpio(np,
						"silabs,int-gpio",
						0);
	if (device_data->int_gpio < 0) {
		pr_err("silabs-int-gpio not provided in device tree");
		rc = device_data->int_gpio;
		goto err_int_gpio;
	}

	rc = gpio_request(device_data->int_gpio, "silabs_fm_int_n");
	if (rc) {
		pr_err("unable to request gpio %d (%d)\n",
				device_data->int_gpio, rc);
		goto err_int_gpio;
	}

	device_data->status_gpio = of_get_named_gpio(np,
						"silabs,status-gpio",
						0);
	if (device_data->status_gpio < 0) {
		FMDERR("silabs-status-gpio not provided in device tree");
	} else {

		rc = gpio_request(device_data->status_gpio, "silabs_fm_stat_n");
		if (rc) {
			pr_err("unable to request status gpio %d (%d)\n",
					device_data->status_gpio, rc);
			goto err_status_gpio;
		}
	}

	return rc;

err_status_gpio:
	gpio_free(device_data->int_gpio);
err_int_gpio:
	gpio_free(device_data->reset_gpio);

	return rc;
}

static int silabs_fm_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret = 0;
	struct regulator *temp_reg = NULL;

	if (!i2c_check_functionality(client->adapter,
					I2C_FUNC_I2C)) {
		FMDERR("%s: no support for i2c read/write byte data\n",
			__func__);
		return -EIO;
	}

	temp_reg = regulator_get(&client->dev, "va");

	if (IS_ERR(temp_reg)) {
		/* if analog voltage regulator, VA is not ready yet, return
		 * -EPROBE_DEFER to kernel so that probe will be called at
		 * later point of time.
		 */
		if (PTR_ERR(temp_reg) == -EPROBE_DEFER) {
			FMDERR("In %s, areg probe defer\n", __func__);
			return PTR_ERR(temp_reg);
		}
	}

	device_data = kzalloc(sizeof(struct fm_i2c_device_data), GFP_KERNEL);
	if (!device_data) {
		FMDERR("%s: allocating memory for device_data failed\n",
			__func__);

		regulator_put(temp_reg);
		return -ENOMEM;
	}

	ret = silabs_parse_dt(&client->dev, device_data);
	if (ret) {
		FMDERR("%s: Parsing DT failed(%d)", __func__, ret);
		regulator_put(temp_reg);
		kfree(device_data);
		return ret;
	}

	device_data->client = client;

	i2c_set_clientdata(client, device_data);

	if (!IS_ERR(temp_reg)) {
		device_data->areg = devm_kzalloc(&client->dev,
					sizeof(struct fm_power_vreg_data),
					GFP_KERNEL);
		if (!device_data->areg) {
			FMDERR("%s: allocating memory for areg failed\n",
				__func__);
			regulator_put(temp_reg);
			kfree(device_data);
			return -ENOMEM;
		}

		device_data->areg->reg = temp_reg;
		device_data->areg->name = "va";
		device_data->areg->low_vol_level = 3300000;
		device_data->areg->high_vol_level = 3300000;
		device_data->areg->is_enabled = 0;
	}

	temp_reg = regulator_get(&client->dev, "vdd");

	if (IS_ERR(temp_reg))
		FMDERR("In %s, vdd supply is not provided\n", __func__);
	else {
		device_data->dreg = devm_kzalloc(&client->dev,
					sizeof(struct fm_power_vreg_data),
					GFP_KERNEL);
		if (!device_data->dreg) {
			FMDERR("%s: allocating memory for dreg failed\n",
				__func__);
			ret = -ENOMEM;
			regulator_put(temp_reg);
			goto mem_alloc_fail;
		}

		device_data->dreg->reg = temp_reg;
		device_data->dreg->name = "vdd";
		device_data->dreg->low_vol_level = 1800000;
		device_data->dreg->high_vol_level = 1800000;
		device_data->dreg->is_enabled = 0;
	}

	/* Initialize pin control*/
	ret = silabs_fm_pinctrl_init();
	if (ret) {
		FMDERR("%s: silabs_fm_pinctrl_init returned %d\n",
				__func__, ret);
		/* if pinctrl is not supported, -EINVAL is returned*/
		if (ret == -EINVAL)
			ret = 0;
	} else
		FMDBG("silabs_fm_pinctrl_init success\n");

	return ret;

mem_alloc_fail:

	if (device_data->areg && device_data->areg->reg)
		regulator_put(device_data->areg->reg);

	devm_kfree(&client->dev, device_data->areg);
	kfree(device_data);
	return ret;
}

static int silabs_i2c_remove(struct i2c_client *client)
{
	struct fm_i2c_device_data *device_data = i2c_get_clientdata(client);

	if (device_data->dreg && device_data->dreg->reg)
		regulator_put(device_data->dreg->reg);

	if (device_data->areg && device_data->areg->reg)
		regulator_put(device_data->areg->reg);

	devm_kfree(&client->dev, device_data->areg);
	devm_kfree(&client->dev, device_data->dreg);
	kfree(device_data);
	return 0;
}

static const struct i2c_device_id silabs_i2c_id[] = {
	{ DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, silabs_i2c_id);

static struct of_device_id silabs_i2c_match_table[] = {
	{ .compatible = "silabs,si4705",},
	{ },
};

static struct i2c_driver silabs_i2c_driver = {
	.driver	= {
		.name	= DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = silabs_i2c_match_table,
	},
	.probe		= silabs_fm_i2c_probe,
	.remove		= silabs_i2c_remove,
	.id_table	= silabs_i2c_id,
};

static int __init silabs_i2c_init(void)
{
	return i2c_add_driver(&silabs_i2c_driver);
}
module_init(silabs_i2c_init);

static void __exit silabs_i2c_exit(void)
{
	i2c_del_driver(&silabs_i2c_driver);
}
module_exit(silabs_i2c_exit);

MODULE_DESCRIPTION("SiLabs FM driver");
MODULE_LICENSE("GPL v2");
