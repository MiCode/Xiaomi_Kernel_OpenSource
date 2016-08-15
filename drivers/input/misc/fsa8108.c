/* drivers/input/misc/fsa8108.c
 *
 * Copyright (C) 2016 XiaoMi, Inc.
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
 */
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <sound/jack.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_data/fsa8108.h>
#include <linux/wakelock.h>

/* defines same as in sound/soc/codecs/cs42l73.h */
#define CS42L73_MICBIAS_VMIN0 185000  /* uV */
#define CS42L73_MICBIAS_VMAX0 215000  /* uV */
#define CS42L73_MICBIAS_VMIN1 259000  /* uV */
#define CS42L73_MICBIAS_VMAX1 289000  /* uV */

struct fsa8108_data {
	struct input_dev *ipd;
	struct fsa8108_platform_data *pdata;
	struct regulator        *regulator;
	struct regmap           *regmap;
	bool reg_enable;
	int irq_gpio;
	int irq;
	struct wake_lock wake_lock;
};

/* set jack input device capability
  * ipd    : input device that need to set capability
  * pdata: from board file
  */
static void fsa8108_set_input_capability(struct input_dev *ipd,
							struct fsa8108_platform_data *pdata)
{
	int i = 0;
	struct fsa8108_intmask_event *fsa8108_event = pdata->fsa8108_event;

	if (pdata->fsa8108_event_size <= 0)
		pr_err("%s:Unable to set input device capability\n", __func__);

	for (i = 0; i < pdata->fsa8108_event_size; i++)
		input_set_capability(ipd, fsa8108_event[i].type, fsa8108_event[i].code);

}

static int fsa8108_write_reg(struct fsa8108_data *data, int reg, int val)
{
	int ret;
	struct regmap *map = data->regmap;

	ret = regmap_write(map, reg, val);
	return ret;
}

static int fsa8108_read_reg(struct fsa8108_data *data, int reg)
{
	unsigned int value;
	int ret;
	struct regmap *map =  data->regmap;

	ret = regmap_read(map, reg, &value);
	if (ret < 0)
		return ret;
	else
		return (int)value;
}

static int fsa8108_reset(struct i2c_client *client)
{
	int rc = 0;
	struct fsa8108_platform_data *pdata = client->dev.platform_data;

	rc = gpio_request(pdata->reset_gpio, "jack_rst_gpio");
	if (rc) {
		pr_err("%s:Unable to request jack_rst_gpio\n", __func__);
		goto fail_rqst_rst_gpio;
	}

	rc = gpio_direction_output(pdata->reset_gpio, 0);
	if (rc) {
		pr_err("%s:GPIO %d set_direction failed\n", __func__, pdata->reset_gpio);
		goto fail_cfg_rst_gpio;
	}

	mdelay(5);
	gpio_set_value(pdata->reset_gpio, 1);
	mdelay(5);
	gpio_set_value(pdata->reset_gpio, 0);
	mdelay(5);
	pr_info("%s: reset jack_rst_gpio success\n", __func__);

	return 0;

fail_cfg_rst_gpio:
	gpio_free(pdata->reset_gpio);
fail_rqst_rst_gpio:

	return rc;

}

static void fsa8108_report_event(struct fsa8108_data *dd, unsigned int mask,
					struct fsa8108_platform_data *pdata)
{
	u32 key_code = 0;
	u32 key_type = 0;
	bool key_value = 0;
	int i = 0;
	struct fsa8108_intmask_event *fsa8108_event = pdata->fsa8108_event;

	for (i = 0; i < pdata->fsa8108_event_size; i++) {
		if (fsa8108_event[i].intmask & mask) {
			key_code = fsa8108_event[i].code;
			key_value = fsa8108_event[i].value;
			key_type = fsa8108_event[i].type;
			switch (fsa8108_event[i].intmask) {
			case FSA8108_VOLUP_MASK:
			case FSA8108_VOLDOWN_MASK:
			case FSA8108_SW_MASK:
			case FSA8108_DSW_MASK:
				input_event(dd->ipd, key_type, key_code, 1);
				input_sync(dd->ipd);
				input_event(dd->ipd, key_type, key_code, 0);
				input_sync(dd->ipd);
				break;
			case FSA8108_LSW_MASK:
				input_event(dd->ipd, key_type, key_code, 1);
				input_sync(dd->ipd);
				msleep(2000);
				input_event(dd->ipd, key_type, key_code, 0);
				input_sync(dd->ipd);
				break;
			case FSA8108_LVOLUP_PRESS_MASK:
			case FSA8108_LVOLDOWN_PRESS_MASK:
			case FSA8108_LVOLUP_RELEASE_MASK:
			case FSA8108_LVOLDOWN_RELEASE_MASK:
			case FSA8108_3POLE_INSERTED_MASK:
			case FSA8108_4POLE_INSERTED_MASK:
			case FSA8108_DISCONNECT_MASK:
				input_event(dd->ipd, key_type, key_code,
						key_value);
				break;
			}
		}
	}

	input_sync(dd->ipd);
	return;
}

/* Set Control register based on plug 3pole or 4pole or unplug headset
  *
  * if you plug 3pole headset ,you must turn off LD0 output and MIC detection
  * or you can listen to noise.
  *
  * of course you must be turn on , when you unplug headset
  * or you can't detect 4pole headset.
  *
  * NOTE that : detail info ,you can look for fsa8108 specification
  */
static void fsa8108_set_control_register(struct fsa8108_data *dd, unsigned int mask)
{
	if (FSA8108_3POLE_INSERTED_MASK & mask) {
		fsa8108_write_reg(dd, CONTROL_REG,
				INSERT_3POLE_VALUE_CONTROL_REG);
		if (dd->regulator && dd->reg_enable) {
			if (!regulator_disable(dd->regulator))
				dd->reg_enable = false;
		}
	} else if (FSA8108_4POLE_INSERTED_MASK & mask) {
		if (dd->regulator) {
			msleep(20);	/* sleep 20ms to make sure apple-like headsets activated */
			pr_info("%s: regulator_set_voltage 0\n", __func__);
			regulator_set_voltage(dd->regulator,
					CS42L73_MICBIAS_VMIN0,
					CS42L73_MICBIAS_VMAX0);
		}
	} else if (FSA8108_DISCONNECT_MASK & mask) {
		fsa8108_write_reg(dd, CONTROL_REG, DEFAULT_VALUE_CONTROL_REG);
		if (dd->regulator) {
			if (!dd->reg_enable) {
				if (!regulator_enable(dd->regulator))
					dd->reg_enable = true;
			}
			pr_info("%s: regulator_set_voltage 1\n", __func__);
			regulator_set_voltage(dd->regulator,
					CS42L73_MICBIAS_VMIN1,
					CS42L73_MICBIAS_VMAX1);
		}
	}
}

static irqreturn_t fsa8108_irq_handle(int irq, void *dev_id)
{
	int reg2, reg3, reg;
	struct fsa8108_data *data = dev_id;

	struct fsa8108_platform_data* pdata = data->pdata;

	reg2 = fsa8108_read_reg(data, INTERRUPT_1_REG);
	reg3 = fsa8108_read_reg(data, INTERRUPT_2_REG);
	reg = reg2 | (reg3 << 8);
	pr_info("%s: reg2=0x%2x, reg3=0x%2x,reg=0x%4x\n", __func__, reg2, reg3,
		reg);

	wake_lock_timeout(&data->wake_lock, 2 * HZ);
	fsa8108_report_event(data, reg, pdata);

	fsa8108_set_control_register(data, reg);

	return IRQ_HANDLED;
}

static struct regmap_config fsa8108_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x10,
};
static int fsa8108_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int i, rc = 0;
	struct input_dev *ipd;
	struct fsa8108_platform_data *pdata;
	struct fsa8108_data *data_fsa8108;

	pr_info("fsa8108_probe:\n");
	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		pr_err("%s: invalid argument\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s: i2c check functionality error\n", __func__);
		rc = -ENODEV;
		return rc;
	}

	data_fsa8108 = kzalloc(sizeof(*data_fsa8108), GFP_KERNEL);
	if (data_fsa8108 == NULL) {
		pr_err("%s: fail to allocate data_fsa8108 \n", __func__);
		rc = -ENOMEM;
		return rc;
	}

	data_fsa8108->regmap = regmap_init_i2c(client, &fsa8108_regmap_config);
	if (IS_ERR(data_fsa8108->regmap)) {
		rc = PTR_ERR(data_fsa8108->regmap);
		pr_err("%s: fail to regmap,err=0x%x\n", __func__, rc);
		goto input_alloc_failed;
	}

	data_fsa8108->regulator = devm_regulator_get(&client->dev, pdata->supply);
	if (IS_ERR(data_fsa8108->regulator)) {
		/* go on, external mic bias is optional */
		pr_info("%s: no external mic bias(%s)", __func__, pdata->supply);
		data_fsa8108->regulator = NULL;
	} else {
		if (!regulator_enable(data_fsa8108->regulator)) {
			data_fsa8108->reg_enable = true;
			pr_info("%s: regulator_set_voltage 1\n", __func__);
			regulator_set_voltage(data_fsa8108->regulator, CS42L73_MICBIAS_VMIN1, CS42L73_MICBIAS_VMAX1);
		}
	}

	/*ic reset*/
	rc = fsa8108_reset(client);
	if (rc < 0) {
		pr_info("%s: fail to reset fsa8108", __func__);
		/*fail isn't fatal*/
	}

	/*judge the ic of fsa8108 is online, if offline return*/
	rc = fsa8108_read_reg(data_fsa8108, DEVICE_ID_REG);
	if (rc < 0) {
		pr_info("%s: can't detect fsa8108", __func__);
		goto reg_read_fail;
	}
	/*init reg*/
	for (i = 0; i < pdata->reg_map_size; i++)
		fsa8108_write_reg(data_fsa8108, pdata->reg_map[i].reg, pdata->reg_map[i].def);

	/* register input device*/
	ipd = input_allocate_device();
	if (ipd == NULL) {
		pr_err("%s: fail to allocate input device\n", __func__);
		rc = -ENOMEM;
		goto reg_read_fail;
	}

	ipd->name = "fsa8108_sw";
	data_fsa8108->ipd = ipd;

	/*set input device  capability*/
	fsa8108_set_input_capability(data_fsa8108->ipd, pdata);

	input_set_drvdata(ipd, data_fsa8108);
	rc = input_register_device(ipd);
	if (rc) {
		pr_err("%s:Unable to register input device\n", __func__);
		goto input_register_failed;
	}

	/* FSA8108 IRQ */
	rc = gpio_request(pdata->irq_gpio, "jack_irq_gpio");
	if (rc) {
		pr_err("%s:Unable to request jack_irq_gpio\n", __func__);
		goto fail_ir_gpio_req;
	}

	rc = gpio_direction_input(pdata->irq_gpio);
	if (rc) {
		pr_err("%s:GPIO %d set_direction failed\n", __func__, pdata->irq_gpio);
		goto fail_ir_gpio_req;
	}

	data_fsa8108->irq_gpio = pdata->irq_gpio;
	data_fsa8108->irq = gpio_to_irq(pdata->irq_gpio);
	data_fsa8108->pdata = pdata;
	dev_set_drvdata(&client->dev, data_fsa8108);
	wake_lock_init(&data_fsa8108->wake_lock, WAKE_LOCK_SUSPEND, "fsa8108_wlock");

	rc = request_threaded_irq(data_fsa8108->irq, NULL, fsa8108_irq_handle,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT,
		"jack_irq_handle", data_fsa8108);
	if (rc < 0) {
		pr_err("%s:could not request jack irq err=%d\n", __func__, rc);
		goto fail_ir_irq;
	} else
		enable_irq_wake(data_fsa8108->irq);

	return rc;

fail_ir_irq:
	wake_lock_destroy(&data_fsa8108->wake_lock);
	if (gpio_is_valid(data_fsa8108->irq_gpio))
		gpio_free(data_fsa8108->irq_gpio);
fail_ir_gpio_req:
	input_unregister_device(ipd);
input_register_failed:
	input_free_device(ipd);
reg_read_fail:
	regmap_exit(data_fsa8108->regmap);
input_alloc_failed:
	kfree(data_fsa8108);
	gpio_free(pdata->reset_gpio);
	return rc;
}

static int fsa8108_remove(struct i2c_client *client)
{
	struct fsa8108_data *data = dev_get_drvdata(&client->dev);

	free_irq(data->irq, data);

	input_unregister_device(data->ipd);
	input_free_device(data->ipd);

	gpio_free(data->irq_gpio);

	gpio_free(data->pdata->reset_gpio);
	regmap_exit(data->regmap);
	wake_lock_destroy(&data->wake_lock);

	kfree(data);
	return 0;
}

static const struct i2c_device_id fsa8108_i2c_id[] = {
	{ "fsa8108", 0 },
	{ }
};

static struct i2c_driver fsa8108_i2c_driver = {
	.driver = {
		.name = "fsa8108",
		.owner = THIS_MODULE,
	},
	.probe    = fsa8108_probe,
	.remove   = __devexit_p(fsa8108_remove),
	.id_table = fsa8108_i2c_id,
#ifdef CONFIG_TEGRA_I2C_RECOVERY
	.reset = fsa8108_reset,
#endif
};

static __init int fsa8108_i2c_init(void)
{
	return i2c_add_driver(&fsa8108_i2c_driver);
}

static __exit void fsa8108_i2c_exit(void)
{
	i2c_del_driver(&fsa8108_i2c_driver);
}

late_initcall(fsa8108_i2c_init);
module_exit(fsa8108_i2c_exit);

MODULE_DESCRIPTION("Driver for fsa8108 switch");
MODULE_LICENSE("GPL");
MODULE_ALIAS("i2c:fsa8108");
