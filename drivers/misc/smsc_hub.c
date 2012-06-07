/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/smsc3503.h>
#include <linux/module.h>
#include <mach/msm_xo.h>

#define SMSC3503_I2C_ADDR 0x08
#define SMSC_GSBI_I2C_BUS_ID 10
static const unsigned short normal_i2c[] = {
SMSC3503_I2C_ADDR, I2C_CLIENT_END };

struct hsic_hub {
	struct regulator *hsic_hub_reg;
	struct device *dev;
	struct i2c_client *client;
	struct msm_xo_voter *xo_handle;
};
static struct hsic_hub *smsc_hub;

/* APIs for setting/clearing bits and for reading/writing values */
static inline int hsic_hub_get_u8(struct i2c_client *client, u8 reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		pr_err("%s:i2c_read8 failed\n", __func__);
	return ret;
}

static inline int hsic_hub_get_u16(struct i2c_client *client, u8 reg)
{
	int ret;

	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0)
		pr_err("%s:i2c_read16 failed\n", __func__);
	return ret;
}

static inline int hsic_hub_write_word_data(struct i2c_client *client, u8 reg,
						u16 value)
{
	int ret;

	ret = i2c_smbus_write_word_data(client, reg, value);
	if (ret)
		pr_err("%s:i2c_write16 failed\n", __func__);
	return ret;
}

static inline int hsic_hub_write_byte_data(struct i2c_client *client, u8 reg,
						u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret)
		pr_err("%s:i2c_write_byte_data failed\n", __func__);
	return ret;
}

static inline int hsic_hub_set_bits(struct i2c_client *client, u8 reg,
					u8 value)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		pr_err("%s:i2c_read_byte_data failed\n", __func__);
		return ret;
	}
	return i2c_smbus_write_byte_data(client, reg, (ret | value));
}

static inline int hsic_hub_clear_bits(struct i2c_client *client, u8 reg,
					u8 value)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		pr_err("%s:i2c_read_byte_data failed\n", __func__);
		return ret;
	}
	return i2c_smbus_write_byte_data(client, reg, (ret & ~value));
}

static int i2c_hsic_hub_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA |
				     I2C_FUNC_SMBUS_WORD_DATA))
		return -EIO;

	/* CONFIG_N bit in SP_ILOCK register has to be set before changing
	 * other registers to change default configuration of hsic hub.
	 */
	hsic_hub_set_bits(client, SMSC3503_SP_ILOCK, CONFIG_N);

	/* Can change default configuartion like VID,PID, strings etc
	 * by writing new values to hsic hub registers.
	 */
	hsic_hub_write_word_data(client, SMSC3503_VENDORID, 0x05C6);

	/* CONFIG_N bit in SP_ILOCK register has to be cleared for new
	 * values in registers to be effective after writing to
	 * other registers.
	 */
	hsic_hub_clear_bits(client, SMSC3503_SP_ILOCK, CONFIG_N);

	return 0;
}

static int i2c_hsic_hub_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id hsic_hub_id[] = {
	{"i2c_hsic_hub", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, hsichub_id);

static struct i2c_driver hsic_hub_driver = {
	.driver = {
		.name = "i2c_hsic_hub",
	},
	.probe    = i2c_hsic_hub_probe,
	.remove   = i2c_hsic_hub_remove,
	.id_table = hsic_hub_id,
};

#define HSIC_HUB_VDD_VOL_MIN	1650000 /* uV */
#define HSIC_HUB_VDD_VOL_MAX	1950000 /* uV */
#define HSIC_HUB_VDD_LOAD	36000	/* uA */
static int __devinit smsc_hub_probe(struct platform_device *pdev)
{
	int ret = 0;
	const struct smsc_hub_platform_data *pdata;
	struct i2c_adapter *i2c_adap;
	struct i2c_board_info i2c_info;

	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "No platform data\n");
		return -ENODEV;
	}

	pdata = pdev->dev.platform_data;
	if (!pdata->hub_reset)
		return -EINVAL;

	smsc_hub = kzalloc(sizeof(*smsc_hub), GFP_KERNEL);
	if (!smsc_hub)
		return -ENOMEM;

	smsc_hub->hsic_hub_reg = regulator_get(&pdev->dev, "EXT_HUB_VDDIO");
	if (IS_ERR(smsc_hub->hsic_hub_reg)) {
		dev_err(&pdev->dev, "unable to get ext hub vddcx\n");
		ret = PTR_ERR(smsc_hub->hsic_hub_reg);
		goto free_mem;
	}

	ret = gpio_request(pdata->hub_reset, "HSIC_HUB_RESET_GPIO");
	if (ret < 0) {
		dev_err(&pdev->dev, "gpio request failed for GPIO%d\n",
							pdata->hub_reset);
		goto gpio_req_fail;
	}

	ret = regulator_set_voltage(smsc_hub->hsic_hub_reg,
			HSIC_HUB_VDD_VOL_MIN,
			HSIC_HUB_VDD_VOL_MAX);
	if (ret) {
		dev_err(&pdev->dev, "unable to set the voltage"
				"for hsic hub reg\n");
		goto reg_set_voltage_fail;
	}

	ret = regulator_set_optimum_mode(smsc_hub->hsic_hub_reg,
				HSIC_HUB_VDD_LOAD);
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to set optimum mode of regulator:"
							"VDDCX\n");
		goto reg_optimum_mode_fail;
	}

	ret = regulator_enable(smsc_hub->hsic_hub_reg);
	if (ret) {
		dev_err(&pdev->dev, "unable to enable ext hub vddcx\n");
		goto reg_enable_fail;
	}

	smsc_hub->xo_handle = msm_xo_get(MSM_XO_TCXO_D1, "hsic_hub");
	if (IS_ERR(smsc_hub->xo_handle)) {
		dev_err(&pdev->dev, "not able to get the handle"
					 "for TCXO D1 buffer\n");
			goto disable_regulator;
	}

	ret = msm_xo_mode_vote(smsc_hub->xo_handle, MSM_XO_MODE_ON);
	if (ret) {
		dev_err(&pdev->dev, "failed to vote for TCXO"
			"D1 buffer\n");
		goto xo_vote_fail;
	}

	gpio_direction_output(pdata->hub_reset, 0);
	/* Hub reset should be asserted for minimum 2microsec
	 * before deasserting.
	 */
	udelay(5);
	gpio_direction_output(pdata->hub_reset, 1);

	ret = i2c_add_driver(&hsic_hub_driver);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to add I2C hsic_hub_driver\n");
		goto i2c_add_fail;
	}
	usleep_range(10000, 12000);
	i2c_adap = i2c_get_adapter(SMSC_GSBI_I2C_BUS_ID);

	if (!i2c_adap) {
		dev_err(&pdev->dev, "failed to get i2c adapter\n");
		i2c_del_driver(&hsic_hub_driver);
		goto i2c_add_fail;
	}

	memset(&i2c_info, 0, sizeof(struct i2c_board_info));
	strlcpy(i2c_info.type, "i2c_hsic_hub", I2C_NAME_SIZE);

	smsc_hub->client = i2c_new_probed_device(i2c_adap, &i2c_info,
						   normal_i2c, NULL);
	i2c_put_adapter(i2c_adap);
	if (!smsc_hub->client)
		dev_err(&pdev->dev, "failed to connect to smsc_hub"
			 "through I2C\n");

i2c_add_fail:
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return 0;

xo_vote_fail:
	msm_xo_put(smsc_hub->xo_handle);
disable_regulator:
	regulator_disable(smsc_hub->hsic_hub_reg);
reg_enable_fail:
	regulator_set_optimum_mode(smsc_hub->hsic_hub_reg, 0);
reg_optimum_mode_fail:
	regulator_set_voltage(smsc_hub->hsic_hub_reg, 0,
				HSIC_HUB_VDD_VOL_MIN);
reg_set_voltage_fail:
	gpio_free(pdata->hub_reset);
gpio_req_fail:
	regulator_put(smsc_hub->hsic_hub_reg);
free_mem:
	kfree(smsc_hub);

	return ret;
}

static int smsc_hub_remove(struct platform_device *pdev)
{
	const struct smsc_hub_platform_data *pdata;

	pdata = pdev->dev.platform_data;
	if (smsc_hub->client) {
		i2c_unregister_device(smsc_hub->client);
		smsc_hub->client = NULL;
		i2c_del_driver(&hsic_hub_driver);
	}
	pm_runtime_disable(&pdev->dev);
	msm_xo_put(smsc_hub->xo_handle);

	regulator_disable(smsc_hub->hsic_hub_reg);
	regulator_set_optimum_mode(smsc_hub->hsic_hub_reg, 0);
	regulator_set_voltage(smsc_hub->hsic_hub_reg, 0,
				HSIC_HUB_VDD_VOL_MIN);
	gpio_free(pdata->hub_reset);
	regulator_put(smsc_hub->hsic_hub_reg);
	kfree(smsc_hub);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int msm_smsc_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "SMSC HUB runtime idle\n");

	return 0;
}

static int smsc_hub_lpm_enter(struct device *dev)
{
	int ret;

	ret = msm_xo_mode_vote(smsc_hub->xo_handle, MSM_XO_MODE_OFF);
	if (ret) {
		pr_err("%s: failed to devote for TCXO"
			"D1 buffer%d\n", __func__, ret);
	}
	return ret;
}

static int smsc_hub_lpm_exit(struct device *dev)
{
	int ret;

	ret = msm_xo_mode_vote(smsc_hub->xo_handle, MSM_XO_MODE_ON);
	if (ret) {
		pr_err("%s: failed to vote for TCXO"
			"D1 buffer%d\n", __func__, ret);
	}
	return ret;
}
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops smsc_hub_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(smsc_hub_lpm_enter, smsc_hub_lpm_exit)
	SET_RUNTIME_PM_OPS(smsc_hub_lpm_enter, smsc_hub_lpm_exit,
				msm_smsc_runtime_idle)
};
#endif

static struct platform_driver smsc_hub_driver = {
	.driver = {
		.name = "msm_smsc_hub",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &smsc_hub_dev_pm_ops,
#endif
	},
	.remove = smsc_hub_remove,
};

static int __init smsc_hub_init(void)
{
	return platform_driver_probe(&smsc_hub_driver, smsc_hub_probe);
}

static void __exit smsc_hub_exit(void)
{
	platform_driver_unregister(&smsc_hub_driver);
}
subsys_initcall(smsc_hub_init);
module_exit(smsc_hub_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SMSC HSIC HUB driver");
