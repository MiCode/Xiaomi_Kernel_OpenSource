/*
 * bq2477x-charger.c -- BQ24775 Charger driver
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Andy Park <andyp@nvidia.com>
 * Author: Syed Rafiuddin <srafiuddin@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power/bq2477x-charger.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/rtc.h>
#include <linux/alarmtimer.h>
#include <linux/sched/rt.h>

struct bq2477x_chip {
	struct device	*dev;
	struct power_supply	ac;
	struct regmap	*regmap;
	struct regmap	*regmap_word;
	struct mutex	mutex;
	int	irq;
	int	gpio;
	int	ac_online;
	int	dac_ichg;
	int	dac_v;
	int	dac_minsv;
	int	dac_iin;
	int	suspended;
	int	wdt_refresh_timeout;
	struct kthread_worker	bq_kworker;
	struct task_struct	*bq_kworker_task;
	struct kthread_work	bq_wdt_work;
};

/* Kthread scheduling parameters */
struct sched_param bq2477x_param = {
	.sched_priority = MAX_RT_PRIO - 1,
};

static const struct regmap_config bq2477x_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= BQ2477X_MAX_REGS,
};

static const struct regmap_config bq2477x_regmap_word_config = {
	.reg_bits	= 8,
	.val_bits	= 16,
	.max_register	= BQ2477X_MAX_REGS,
};

/* Charge current limit */
static const unsigned int dac_ichg[] = {
	64, 128, 256, 512, 1024, 2048, 4096,
};

/* Output charge regulation voltage */
static const unsigned int dac_v[] = {
	16, 32, 64, 128, 256, 512, 1024, 2048,
	4096, 8192, 16384,
};

/* Minimum system votlage */
static const unsigned int dac_minsv[] = {
	256, 512, 1024, 2048, 4096, 8192,
};

/* Setting input current */
static const unsigned int dac_iin[] = {
	64, 128, 256, 512, 1024, 2048, 4096,
};

static int bq2477x_read(struct bq2477x_chip *bq2477x,
	unsigned int reg, unsigned int *val)
{
	return regmap_read(bq2477x->regmap, reg, val);
}

static int bq2477x_read_word(struct bq2477x_chip *bq2477x,
	unsigned int reg, unsigned int *val)
{
	return regmap_read(bq2477x->regmap_word, reg, val);
}

static int bq2477x_write(struct bq2477x_chip *bq2477x,
	unsigned int reg, unsigned int val)
{
	return regmap_write(bq2477x->regmap, reg, val);
}

static int bq2477x_write_word(struct bq2477x_chip *bq2477x,
	unsigned int reg, unsigned int val)
{
	return regmap_write(bq2477x->regmap_word, reg, val);
}

static int bq2477x_update_bits(struct bq2477x_chip *bq2477x,
	unsigned int reg, unsigned int mask, unsigned int val)
{
	return regmap_update_bits(bq2477x->regmap, reg, mask, val);
}

static enum power_supply_property bq2477x_psy_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int bq2477x_ac_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct bq2477x_chip *bq2477x;

	bq2477x = container_of(psy, struct bq2477x_chip, ac);
	if (psp == POWER_SUPPLY_PROP_ONLINE)
		val->intval = bq2477x->ac_online;
	else
		return -EINVAL;
	return 0;
}

static int bq2477x_show_chip_version(struct bq2477x_chip *bq2477x)
{
	int ret;
	unsigned int val;

	ret = bq2477x_read(bq2477x, BQ2477X_DEVICE_ID_REG, &val);
	if (ret < 0) {
		dev_err(bq2477x->dev, "DEVICE_ID_REG read failed: %d\n", ret);
		return ret;
	}

	if (val == BQ24770_DEVICE_ID)
		dev_info(bq2477x->dev, "chip type BQ24770 detected\n");
	else if (val == BQ24773_DEVICE_ID)
		dev_info(bq2477x->dev, "chip type BQ24773 detected\n");
	else {
		dev_info(bq2477x->dev, "unrecognized chip type: 0x%4x\n", val);
		return -EINVAL;
	}
	return 0;
}

static int bq2477x_hw_init(struct bq2477x_chip *bq2477x)
{
	int ret = 0;
	unsigned int val;

	/* Configure control */
	ret = bq2477x_write(bq2477x, BQ2477X_CHARGE_OPTION_0_MSB,
			BQ2477X_CHARGE_OPTION_POR_MSB);
	if (ret < 0) {
		dev_err(bq2477x->dev, "CHARGE_OPTION_0 write failed %d\n", ret);
		return ret;
	}
	ret = bq2477x_write(bq2477x, BQ2477X_CHARGE_OPTION_0_LSB,
			BQ2477X_CHARGE_OPTION_POR_LSB);
	if (ret < 0) {
		dev_err(bq2477x->dev, "CHARGE_OPTION_0 write failed %d\n", ret);
		return ret;
	}

	ret = bq2477x_write_word(bq2477x, BQ2477X_MAX_CHARGE_VOLTAGE_LSB,
			(bq2477x->dac_v >> 8) | (bq2477x->dac_v << 8));
	if (ret < 0) {
		dev_err(bq2477x->dev, "CHARGE_VOLTAGE write failed %d\n", ret);
		return ret;
	}

	ret = bq2477x_write(bq2477x, BQ2477X_MIN_SYS_VOLTAGE,
			bq2477x->dac_minsv >> BQ2477X_MIN_SYS_VOLTAGE_SHIFT);
	if (ret < 0) {
		dev_err(bq2477x->dev, "MIN_SYS_VOLTAGE write failed %d\n", ret);
		return ret;
	}

	/* Configure setting input current */
	ret = bq2477x_write(bq2477x, BQ2477X_INPUT_CURRENT,
			bq2477x->dac_iin >> BQ2477X_INPUT_CURRENT_SHIFT);
	if (ret < 0) {
		dev_err(bq2477x->dev, "INPUT_CURRENT write failed %d\n", ret);
		return ret;
	}

	ret = bq2477x_write_word(bq2477x, BQ2477X_CHARGE_CURRENT_LSB,
			(bq2477x->dac_ichg >> 8) | (bq2477x->dac_ichg << 8));
	if (ret < 0) {
		dev_err(bq2477x->dev, "CHARGE_CURRENT write failed %d\n", ret);
		return ret;
	}

	return ret;
}

static void bq2477x_work_thread(struct kthread_work *work)
{
	struct bq2477x_chip *bq2477x = container_of(work,
					struct bq2477x_chip, bq_wdt_work);
	int ret;
	unsigned int val;

	for (;;) {
		ret = bq2477x_hw_init(bq2477x);
		if (ret < 0) {
			dev_err(bq2477x->dev, "Hardware init failed %d\n", ret);
			return;
		}

		ret = bq2477x_update_bits(bq2477x, BQ2477X_CHARGE_OPTION_0_MSB,
					BQ2477X_WATCHDOG_TIMER, 0x60);
		if (ret < 0) {
			dev_err(bq2477x->dev,
				"CHARGE_OPTION write failed %d\n", ret);
			return;
		}

		msleep(bq2477x->wdt_refresh_timeout * 1000);
	}
}

static int bq2477x_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct bq2477x_chip *bq2477x = NULL;
	struct bq2477x_platform_data *pdata;
	int ret = 0;

	pdata = client->dev.platform_data;
	if (!pdata) {
		dev_err(&client->dev, "No Platform data");
		return -EINVAL;
	}

	ret = gpio_request(pdata->gpio, "bq2477x-charger");
	if (ret) {
		dev_err(&client->dev, "Failed to request gpio pin: %d\n", ret);
		return ret;
	}

	ret = gpio_direction_output(pdata->gpio, 1);
	if (ret) {
		dev_err(&client->dev,
			"Failed to set gpio to output: %d\n", ret);
		goto gpio_err;
	}

	gpio_set_value(pdata->gpio, 1);

	msleep(20);

	bq2477x = devm_kzalloc(&client->dev, sizeof(*bq2477x), GFP_KERNEL);
	if (!bq2477x) {
		dev_err(&client->dev, "Memory allocation failed\n");
		ret = -ENOMEM;
		goto gpio_err;
	}
	bq2477x->dev = &client->dev;

	bq2477x->dac_ichg = pdata->dac_ichg;
	bq2477x->dac_v = pdata->dac_v;
	bq2477x->dac_minsv = pdata->dac_minsv;
	bq2477x->dac_iin = pdata->dac_iin;
	bq2477x->wdt_refresh_timeout = pdata->wdt_refresh_timeout;
	bq2477x->gpio = pdata->gpio;

	i2c_set_clientdata(client, bq2477x);
	bq2477x->irq = client->irq;
	mutex_init(&bq2477x->mutex);

	bq2477x->ac_online = 0;

	bq2477x->regmap = devm_regmap_init_i2c(client, &bq2477x_regmap_config);
	if (IS_ERR(bq2477x->regmap)) {
		ret = PTR_ERR(bq2477x->regmap);
		dev_err(&client->dev, "regmap init failed with err %d\n", ret);
		goto gpio_err;
	}

	bq2477x->regmap_word = devm_regmap_init_i2c(client,
					&bq2477x_regmap_word_config);
	if (IS_ERR(bq2477x->regmap_word)) {
		ret = PTR_ERR(bq2477x->regmap_word);
		dev_err(&client->dev,
			"regmap_word init failed with err %d\n", ret);
		goto gpio_err;
	}

	ret = bq2477x_show_chip_version(bq2477x);
	if (ret < 0) {
		dev_err(bq2477x->dev, "version read failed %d\n", ret);
		goto gpio_err;
	}

	bq2477x->ac.name	= "bq2477x-ac";
	bq2477x->ac.type	= POWER_SUPPLY_TYPE_MAINS;
	bq2477x->ac.get_property	= bq2477x_ac_get_property;
	bq2477x->ac.properties		= bq2477x_psy_props;
	bq2477x->ac.num_properties	= ARRAY_SIZE(bq2477x_psy_props);

	ret = power_supply_register(bq2477x->dev, &bq2477x->ac);
	if (ret < 0) {
		dev_err(bq2477x->dev,
			"AC power supply register failed %d\n", ret);
		goto gpio_err;
	}

	ret = bq2477x_hw_init(bq2477x);
	if (ret < 0) {
		dev_err(bq2477x->dev, "Hardware init failed %d\n", ret);
		goto psy_err;
	}

	init_kthread_worker(&bq2477x->bq_kworker);
	bq2477x->bq_kworker_task = kthread_run(kthread_worker_fn,
				&bq2477x->bq_kworker,
				dev_name(bq2477x->dev));
	if (IS_ERR(bq2477x->bq_kworker_task)) {
		ret = PTR_ERR(bq2477x->bq_kworker_task);
		dev_err(&client->dev, "Kworker task creation failed %d\n", ret);
		goto psy_err;
	}

	init_kthread_work(&bq2477x->bq_wdt_work, bq2477x_work_thread);
	sched_setscheduler(bq2477x->bq_kworker_task,
		SCHED_FIFO, &bq2477x_param);
	queue_kthread_work(&bq2477x->bq_kworker, &bq2477x->bq_wdt_work);

	dev_info(bq2477x->dev, "bq2477x charger registerd\n");

	return ret;

psy_err:
	power_supply_unregister(&bq2477x->ac);
gpio_err:
	gpio_free(bq2477x->gpio);
	return ret;
}

static int bq2477x_remove(struct i2c_client *client)
{
	struct bq2477x_chip *bq2477x = i2c_get_clientdata(client);
	flush_kthread_worker(&bq2477x->bq_kworker);
	kthread_stop(bq2477x->bq_kworker_task);
	power_supply_unregister(&bq2477x->ac);
	gpio_free(bq2477x->gpio);
	return 0;
}

static const struct i2c_device_id bq2477x_id[] = {
	{.name = "bq2477x",},
	{},
};
MODULE_DEVICE_TABLE(i2c, bq2477x_id);

static struct i2c_driver bq2477x_i2c_driver = {
	.driver = {
		.name   = "bq2477x",
		.owner  = THIS_MODULE,
	},
	.probe		= bq2477x_probe,
	.remove		= bq2477x_remove,
	.id_table	= bq2477x_id,
};

static int __init bq2477x_module_init(void)
{
	return i2c_add_driver(&bq2477x_i2c_driver);
}
subsys_initcall(bq2477x_module_init);

static void __exit bq2477x_cleanup(void)
{
	i2c_del_driver(&bq2477x_i2c_driver);
}
module_exit(bq2477x_cleanup);

MODULE_DESCRIPTION("BQ24775/BQ24777 battery charger driver");
MODULE_AUTHOR("Andy Park <andyp@nvidia.com>");
MODULE_AUTHOR("Syed Rafiuddin <srafiuddin@nvidia.com");
MODULE_LICENSE("GPL v2");

