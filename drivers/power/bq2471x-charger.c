/*
 * bq2471x-charger.c -- BQ24715 Charger driver
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
#include <linux/sched/rt.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power/bq2471x-charger.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/rtc.h>
#include <linux/alarmtimer.h>
#include <linux/power/battery-charger-gauge-comm.h>

struct bq2471x_chip {
	struct device	*dev;
	struct power_supply	ac;
	struct regmap	*regmap;
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
	int	gpio_active_low;
	struct kthread_worker	bq_kworker;
	struct task_struct	*bq_kworker_task;
	struct kthread_work	bq_wdt_work;
	struct battery_charger_dev	*bc_dev;
};

/* Kthread scheduling parameters */
struct sched_param bq2471x_param = {
	.sched_priority = MAX_RT_PRIO - 1,
};

static const struct regmap_config bq2471x_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 16,
	.max_register	= BQ2471X_MAX_REGS,
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

static int bq2471x_read(struct bq2471x_chip *bq2471x,
	unsigned int reg, unsigned int *val)
{
	return regmap_read(bq2471x->regmap, reg, val);
}

static int bq2471x_write(struct bq2471x_chip *bq2471x,
	unsigned int reg, unsigned int val)
{
	return regmap_write(bq2471x->regmap, reg, val);
}

static int bq2471x_update_bits(struct bq2471x_chip *bq2471x,
	unsigned int reg, unsigned int mask, unsigned int val)
{
	return regmap_update_bits(bq2471x->regmap, reg, mask, val);
}

static uint16_t convert_endianness(uint16_t val)
{
	return (val >> 8) | (val << 8);
}

static enum power_supply_property bq2471x_psy_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int bq2471x_ac_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct bq2471x_chip *bq2471x;

	bq2471x = container_of(psy, struct bq2471x_chip, ac);
	if (psp == POWER_SUPPLY_PROP_ONLINE)
		val->intval = bq2471x->ac_online;
	else
		return -EINVAL;
	return 0;
}

static int bq2471x_check_manufacturer(struct bq2471x_chip *bq2471x)
{
	int ret;
	unsigned int val;

	ret = bq2471x_read(bq2471x, BQ2471X_MANUFACTURER_ID_REG, &val);
	if (ret < 0) {
		dev_err(bq2471x->dev,
			"MANUFACTURER_ID_REG read failed: %d\n", ret);
		return ret;
	}

	if ((val & BQ2471X_MANUFACTURER_ID) == BQ2471X_MANUFACTURER_ID)
		dev_info(bq2471x->dev,
			"correct manufacturer id detected: 0x%4x\n", val);
	else {
		dev_info(bq2471x->dev,
			"wrong manufactuerer id detected: 0x%4x\n", val);
		return -EINVAL;
	}
	return 0;
}

static int bq2471x_show_chip_version(struct bq2471x_chip *bq2471x)
{
	int ret;
	unsigned int val;

	ret = bq2471x_read(bq2471x, BQ2471X_DEVICE_ID_REG, &val);
	if (ret < 0) {
		dev_err(bq2471x->dev, "DEVICE_ID_REG read failed: %d\n", ret);
		return ret;
	}

	if ((val & BQ24715_DEVICE_ID) == BQ24715_DEVICE_ID)
		dev_info(bq2471x->dev, "chip type BQ24715 detected\n");
	else if ((val & BQ24717_DEVICE_ID) == BQ24717_DEVICE_ID)
		dev_info(bq2471x->dev, "chip type BQ24717 detected\n");
	else {
		dev_info(bq2471x->dev, "unrecognized chip type: 0x%4x\n", val);
		return -EINVAL;
	}
	return 0;
}

static int bq2471x_hw_init(struct bq2471x_chip *bq2471x)
{
	int ret = 0;

	/* Configure control */
	ret = bq2471x_write(bq2471x, BQ2471X_CHARGE_OPTION,
			BQ2471X_CHARGE_OPTION_POR);
	if (ret < 0) {
		dev_err(bq2471x->dev, "CHARGE_OPTION write failed %d\n", ret);
		return ret;
	}

	/* Configure output charge regulation voltage */
	ret = bq2471x_write(bq2471x, BQ2471X_MAX_CHARGE_VOLTAGE,
			convert_endianness(bq2471x->dac_v));
	if (ret < 0) {
		dev_err(bq2471x->dev, "CHARGE_VOLTAGE write failed %d\n", ret);
		return ret;
	}

	ret = bq2471x_write(bq2471x, BQ2471X_MIN_SYS_VOLTAGE,
			convert_endianness(bq2471x->dac_minsv));
	if (ret < 0) {
		dev_err(bq2471x->dev, "MIN_SYS_VOLTAGE write failed %d\n", ret);
		return ret;
	}

	/* Configure setting input current */
	ret = bq2471x_write(bq2471x, BQ2471X_INPUT_CURRENT,
			convert_endianness(bq2471x->dac_iin));
	if (ret < 0) {
		dev_err(bq2471x->dev, "INPUT_CURRENT write failed %d\n", ret);
		return ret;
	}

	/* Configure charge current limit */
	ret = bq2471x_write(bq2471x, BQ2471X_CHARGE_CURRENT,
			convert_endianness(bq2471x->dac_ichg));
	if (ret < 0) {
		dev_err(bq2471x->dev, "CHARGE_CURRENT write failed %d\n", ret);
		return ret;
	}

	return ret;
}

static void bq2471x_work_thread(struct kthread_work *work)
{
	struct bq2471x_chip *bq2471x = container_of(work,
					struct bq2471x_chip, bq_wdt_work);
	int ret;

	for (;;) {
		ret = bq2471x_hw_init(bq2471x);
		if (ret < 0) {
			dev_err(bq2471x->dev, "Hardware init failed %d\n", ret);
			return;
		}
		ret = bq2471x_update_bits(bq2471x, BQ2471X_CHARGE_OPTION,
					BQ2471X_WATCHDOG_TIMER, 0x6000);
		if (ret < 0) {
			dev_err(bq2471x->dev,
				"CHARGE_OPTION write failed %d\n", ret);
			return;
		}
		msleep(bq2471x->wdt_refresh_timeout * 1000);
	}
}

static struct battery_charger_info bq2471x_charger_bci = {
	.cell_id = 0,
};

static irqreturn_t bq2471x_charger_irq(int irq, void *data)
{
	struct bq2471x_chip *bq2471x = data;

	bq2471x->ac_online =
		gpio_get_value_cansleep(bq2471x->gpio);
	bq2471x->ac_online ^=
		bq2471x->gpio_active_low;

	power_supply_changed(&bq2471x->ac);

	return IRQ_HANDLED;
}

static int bq2471x_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct bq2471x_chip *bq2471x;
	struct bq2471x_platform_data *pdata;
	int ret = 0;

	pdata = client->dev.platform_data;
	if (!pdata) {
		dev_err(&client->dev, "No Platform data");
		return -EINVAL;
	}

	ret = gpio_request(pdata->gpio, "bq2471x-charger");
	if (ret) {
		dev_err(&client->dev, "Failed to request gpio pin: %d\n", ret);
		return ret;
	}

	bq2471x = devm_kzalloc(&client->dev, sizeof(*bq2471x), GFP_KERNEL);
	if (!bq2471x) {
		dev_err(&client->dev, "Memory allocation failed\n");
		ret = -ENOMEM;
		goto gpio_err;
	}

	bq2471x->ac.name	= "bq2471x-ac";
	bq2471x->ac.type	= POWER_SUPPLY_TYPE_MAINS;
	bq2471x->ac.get_property	= bq2471x_ac_get_property;
	bq2471x->ac.properties		= bq2471x_psy_props;
	bq2471x->ac.num_properties	= ARRAY_SIZE(bq2471x_psy_props);

	bq2471x->gpio = pdata->gpio;
	bq2471x->dev = &client->dev;

	ret = power_supply_register(bq2471x->dev, &bq2471x->ac);
	if (ret < 0) {
		dev_err(bq2471x->dev,
			"AC power supply register failed %d\n", ret);
		goto gpio_err;
	}

	if (pdata->charge_broadcast_mode) {
		bq2471x->bc_dev = battery_charger_register(bq2471x->dev,
					&bq2471x_charger_bci, bq2471x);

		ret = battery_charger_set_current_broadcast(bq2471x->bc_dev);
		if (ret < 0) {
			dev_err(&client->dev,
				"Failed to enable charging through"
				"fuel gauge broadcasts %d\n", ret);
			return ret;
		}

		ret = gpio_direction_input(bq2471x->gpio);
		bq2471x->irq = gpio_to_irq(bq2471x->gpio);;
		bq2471x->gpio_active_low = pdata->gpio_active_low;

		bq2471x->ac_online =
			gpio_get_value_cansleep(bq2471x->gpio);
		bq2471x->ac_online ^=
			bq2471x->gpio_active_low;

		power_supply_changed(&bq2471x->ac);

		ret = request_any_context_irq(bq2471x->irq, bq2471x_charger_irq,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
					dev_name(bq2471x->dev), bq2471x);

		if (ret < 0)
			dev_err(&client->dev, "Failed to request irq..\n");

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

	bq2471x->dac_ichg = pdata->dac_ichg;
	bq2471x->dac_v = pdata->dac_v;
	bq2471x->dac_minsv = pdata->dac_minsv;
	bq2471x->dac_iin = pdata->dac_iin;
	bq2471x->wdt_refresh_timeout = pdata->wdt_refresh_timeout;

	i2c_set_clientdata(client, bq2471x);
	bq2471x->irq = client->irq;
	mutex_init(&bq2471x->mutex);

	bq2471x->ac_online = 0;

	bq2471x->regmap = devm_regmap_init_i2c(client, &bq2471x_regmap_config);
	if (IS_ERR(bq2471x->regmap)) {
		ret = PTR_ERR(bq2471x->regmap);
		dev_err(&client->dev, "regmap init failed with err %d\n", ret);
		goto gpio_err;
	}

	ret = bq2471x_check_manufacturer(bq2471x);
	if (ret < 0) {
		dev_err(bq2471x->dev, "manufacturer check failed %d\n", ret);
		goto gpio_err;
	}

	ret = bq2471x_show_chip_version(bq2471x);
	if (ret < 0) {
		dev_err(bq2471x->dev, "version read failed %d\n", ret);
		goto gpio_err;
	}



	ret = bq2471x_hw_init(bq2471x);
	if (ret < 0) {
		dev_err(bq2471x->dev, "Hardware init failed %d\n", ret);
		goto psy_err;
	}

	init_kthread_worker(&bq2471x->bq_kworker);
	bq2471x->bq_kworker_task = kthread_run(kthread_worker_fn,
				&bq2471x->bq_kworker,
				dev_name(bq2471x->dev));
	if (IS_ERR(bq2471x->bq_kworker_task)) {
		ret = PTR_ERR(bq2471x->bq_kworker_task);
		dev_err(&client->dev, "Kworker task creation failed %d\n", ret);
		goto psy_err;
	}

	init_kthread_work(&bq2471x->bq_wdt_work, bq2471x_work_thread);
	sched_setscheduler(bq2471x->bq_kworker_task,
		SCHED_FIFO, &bq2471x_param);
	queue_kthread_work(&bq2471x->bq_kworker, &bq2471x->bq_wdt_work);


	dev_info(bq2471x->dev, "bq2471x charger registerd\n");

	return ret;

psy_err:
	power_supply_unregister(&bq2471x->ac);
gpio_err:
	gpio_free(bq2471x->gpio);
	return ret;
}

static int bq2471x_remove(struct i2c_client *client)
{
	struct bq2471x_chip *bq2471x = i2c_get_clientdata(client);
	flush_kthread_worker(&bq2471x->bq_kworker);
	kthread_stop(bq2471x->bq_kworker_task);
	power_supply_unregister(&bq2471x->ac);
	gpio_free(bq2471x->gpio);
	return 0;
}

static const struct i2c_device_id bq2471x_id[] = {
	{.name = "bq2471x",},
	{},
};
MODULE_DEVICE_TABLE(i2c, bq2471x_id);

static struct i2c_driver bq2471x_i2c_driver = {
	.driver = {
		.name   = "bq2471x",
		.owner  = THIS_MODULE,
	},
	.probe		= bq2471x_probe,
	.remove		= bq2471x_remove,
	.id_table	= bq2471x_id,
};

module_i2c_driver(bq2471x_i2c_driver);

MODULE_DESCRIPTION("BQ24715/BQ24717 battery charger driver");
MODULE_AUTHOR("Andy Park <andyp@nvidia.com>");
MODULE_AUTHOR("Syed Rafiuddin <srafiuddin@nvidia.com");
MODULE_LICENSE("GPL v2");

