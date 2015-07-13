/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c/ti_drv2667.h>
#include "../staging/android/timed_output.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#define DRV2667_SUS_LEVEL	1
#endif

#define DRV2667_STATUS_REG	0x00
#define DRV2667_CNTL1_REG	0x01
#define DRV2667_CNTL2_REG	0x02
#define DRV2667_WAV_SEQ3_REG	0x03
#define DRV2667_FIFO_REG	0x0B
#define DRV2667_PAGE_REG	0xFF

#define DRV2667_STANDBY_MASK	0xBF
#define DRV2667_INPUT_MUX_MASK	0x04
#define DRV2667_GAIN_MASK	0xFC
#define DRV2667_GAIN_SHIFT	0
#define DRV2667_TIMEOUT_MASK	0xF3
#define DRV2667_TIMEOUT_SHIFT	2
#define DRV2667_GO_MASK		0x01
#define DRV2667_FIFO_SIZE	100
#define DRV2667_VIB_START_VAL	0x7F
#define DRV2667_REG_PAGE_ID	0x00
#define DRV2667_FIFO_CHUNK_MS	10
#define DRV2667_BYTES_PER_MS	8

#define DRV2667_WAV_SEQ_ID_IDX		0
#define DRV2667_WAV_SEQ_REP_IDX		6
#define DRV2667_WAV_SEQ_FREQ_IDX	8
#define DRV2667_WAV_SEQ_FREQ_MIN	8
#define DRV2667_WAV_SEQ_DUR_IDX		9

#define DRV2667_MIN_IDLE_TIMEOUT_MS	5
#define DRV2667_MAX_IDLE_TIMEOUT_MS	20

#define DRV2667_VTG_MIN_UV	3000000
#define DRV2667_VTG_MAX_UV	5500000
#define DRV2667_VTG_CURR_UA	24000
#define DRV2667_I2C_VTG_MIN_UV	1800000
#define DRV2667_I2C_VTG_MAX_UV	1800000
#define DRV2667_I2C_CURR_UA	9630

/* supports 3 modes in digital - fifo, ram and wave */
enum drv2667_modes {
	FIFO_MODE = 0,
	RAM_SEQ_MODE,
	WAV_SEQ_MODE,
	ANALOG_MODE,
};

struct drv2667_data {
	struct i2c_client *client;
	struct timed_output_dev dev;
	struct hrtimer timer;
	struct work_struct work;
	struct mutex lock;
	struct regulator *vdd;
	struct regulator *vdd_i2c;
	u32 max_runtime_ms;
	u32 runtime_left;
	u8 buf[DRV2667_FIFO_SIZE + 1];
	u8 cntl2_val;
	enum drv2667_modes mode;
	u32 time_chunk_ms;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend es;
#endif
};

static int drv2667_read_reg(struct i2c_client *client, u32 reg)
{
	int rc;

	rc = i2c_smbus_read_byte_data(client, reg);
	if (rc < 0)
		dev_err(&client->dev, "i2c reg read for 0x%x failed\n", reg);
	return rc;
}

static int drv2667_write_reg(struct i2c_client *client, u32 reg, u8 val)
{
	int rc;

	rc = i2c_smbus_write_byte_data(client, reg, val);
	if (rc < 0)
		dev_err(&client->dev, "i2c reg write for 0x%xfailed\n", reg);

	return rc;
}

static void drv2667_dump_regs(struct drv2667_data *data, char *label)
{
	dev_dbg(&data->client->dev,
		"%s: reg0x00 = 0x%x, reg0x01 = 0x%x reg0x02 = 0x%x", label,
		drv2667_read_reg(data->client, DRV2667_STATUS_REG),
		drv2667_read_reg(data->client, DRV2667_CNTL1_REG),
		drv2667_read_reg(data->client, DRV2667_CNTL2_REG));
}

static void drv2667_worker(struct work_struct *work)
{
	struct drv2667_data *data;
	int rc = 0;
	u8 val;

	data = container_of(work, struct drv2667_data, work);

	if (data->mode == WAV_SEQ_MODE) {
		/* clear go bit */
		val = data->cntl2_val & ~DRV2667_GO_MASK;
		rc = drv2667_write_reg(data->client, DRV2667_CNTL2_REG, val);
		if (rc < 0) {
			dev_err(&data->client->dev, "i2c send msg failed\n");
			return;
		}
		/* restart wave if runtime is left */
		if (data->runtime_left) {
			val = data->cntl2_val | DRV2667_GO_MASK;
			rc = drv2667_write_reg(data->client,
						DRV2667_CNTL2_REG, val);
		}
	} else if (data->mode == FIFO_MODE) {
		/* data is played at 8khz */
		if (data->runtime_left < data->time_chunk_ms)
			val = data->runtime_left * DRV2667_BYTES_PER_MS;
		else
			val = data->time_chunk_ms * DRV2667_BYTES_PER_MS;

		rc = i2c_master_send(data->client, data->buf, val + 1);
	}

	if (rc < 0)
		dev_err(&data->client->dev, "i2c send message failed\n");
}

static void drv2667_enable(struct timed_output_dev *dev, int runtime)
{
	struct drv2667_data *data = container_of(dev, struct drv2667_data, dev);
	unsigned long time_ms;

	if (runtime > data->max_runtime_ms) {
		dev_dbg(&data->client->dev, "Invalid runtime\n");
		runtime = data->max_runtime_ms;
	}

	mutex_lock(&data->lock);
	hrtimer_cancel(&data->timer);
	data->runtime_left = runtime;
	if (data->runtime_left < data->time_chunk_ms)
		time_ms = runtime * NSEC_PER_MSEC;
	else
		time_ms = data->time_chunk_ms * NSEC_PER_MSEC;
	hrtimer_start(&data->timer, ktime_set(0, time_ms), HRTIMER_MODE_REL);
	schedule_work(&data->work);
	mutex_unlock(&data->lock);
}

static int drv2667_get_time(struct timed_output_dev *dev)
{
	struct drv2667_data *data = container_of(dev, struct drv2667_data, dev);

	if (hrtimer_active(&data->timer))
		return	data->runtime_left +
			ktime_to_ms(hrtimer_get_remaining(&data->timer));
	return 0;
}

static enum hrtimer_restart drv2667_timer(struct hrtimer *timer)
{
	struct drv2667_data *data;
	int time_ms;

	data = container_of(timer, struct drv2667_data, timer);
	if (data->runtime_left <= data->time_chunk_ms) {
		data->runtime_left = 0;
		schedule_work(&data->work);
		return HRTIMER_NORESTART;
	}

	data->runtime_left -= data->time_chunk_ms;
	if (data->runtime_left < data->time_chunk_ms)
		time_ms = data->runtime_left * NSEC_PER_MSEC;
	else
		time_ms = data->time_chunk_ms * NSEC_PER_MSEC;

	hrtimer_forward_now(&data->timer, ktime_set(0, time_ms));
	schedule_work(&data->work);
	return HRTIMER_RESTART;
}

static int drv2667_vreg_config(struct drv2667_data *data, bool on)
{
	int rc = 0;

	if (!on)
		goto deconfig_vreg;

	data->vdd = regulator_get(&data->client->dev, "vdd");
	if (IS_ERR(data->vdd)) {
		rc = PTR_ERR(data->vdd);
		dev_err(&data->client->dev, "unable to request vdd\n");
		return rc;
	}

	if (regulator_count_voltages(data->vdd) > 0) {
		rc = regulator_set_voltage(data->vdd,
				DRV2667_VTG_MIN_UV, DRV2667_VTG_MAX_UV);
		if (rc < 0) {
			dev_err(&data->client->dev,
				"vdd set voltage failed(%d)\n", rc);
			goto put_vdd;
		}
	}

	data->vdd_i2c = regulator_get(&data->client->dev, "vdd-i2c");
	if (IS_ERR(data->vdd_i2c)) {
		rc = PTR_ERR(data->vdd_i2c);
		dev_err(&data->client->dev, "unable to request vdd for i2c\n");
		goto reset_vdd_volt;
	}

	if (regulator_count_voltages(data->vdd_i2c) > 0) {
		rc = regulator_set_voltage(data->vdd_i2c,
			DRV2667_I2C_VTG_MIN_UV, DRV2667_I2C_VTG_MAX_UV);
		if (rc < 0) {
			dev_err(&data->client->dev,
				"vdd_i2c set voltage failed(%d)\n", rc);
			goto put_vdd_i2c;
		}
	}

	return rc;

deconfig_vreg:
	if (regulator_count_voltages(data->vdd_i2c) > 0)
		regulator_set_voltage(data->vdd_i2c, 0, DRV2667_I2C_VTG_MAX_UV);
put_vdd_i2c:
	regulator_put(data->vdd_i2c);
reset_vdd_volt:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, DRV2667_VTG_MAX_UV);
put_vdd:
	regulator_put(data->vdd);
	return rc;
}

static int reg_set_optimum_mode_check(struct regulator *reg, int load_uA)
{
	return (regulator_count_voltages(reg) > 0) ?
		regulator_set_optimum_mode(reg, load_uA) : 0;
}


static int drv2667_vreg_on(struct drv2667_data *data, bool on)
{
	int rc = 0;

	if (!on)
		goto vreg_off;

	rc = reg_set_optimum_mode_check(data->vdd, DRV2667_VTG_CURR_UA);
	if (rc < 0) {
		dev_err(&data->client->dev,
			"Regulator vdd set_opt failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_enable(data->vdd);
	if (rc < 0) {
		dev_err(&data->client->dev, "enable vdd failed\n");
		return rc;
	}

	rc = reg_set_optimum_mode_check(data->vdd_i2c, DRV2667_I2C_CURR_UA);
	if (rc < 0) {
		dev_err(&data->client->dev,
			"Regulator vdd_i2c set_opt failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_enable(data->vdd_i2c);
	if (rc < 0) {
		dev_err(&data->client->dev, "enable vdd_i2c failed\n");
		goto disable_vdd;
	}

	return rc;
vreg_off:
	regulator_disable(data->vdd_i2c);
disable_vdd:
	regulator_disable(data->vdd);
	return rc;
}

#ifdef CONFIG_PM
static int drv2667_suspend(struct device *dev)
{
	struct drv2667_data *data = dev_get_drvdata(dev);
	u8 val;
	int rc;

	hrtimer_cancel(&data->timer);
	cancel_work_sync(&data->work);

	/* set standby */
	val = data->cntl2_val | ~DRV2667_STANDBY_MASK;
	rc = drv2667_write_reg(data->client, DRV2667_CNTL2_REG, val);
	if (rc < 0)
		dev_err(dev, "unable to set standby\n");

	/* turn regulators off */
	drv2667_vreg_on(data, false);
	return 0;
}

static int drv2667_resume(struct device *dev)
{
	struct drv2667_data *data = dev_get_drvdata(dev);
	int rc;

	/* turn regulators on */
	rc = drv2667_vreg_on(data, true);
	if (rc < 0) {
		dev_err(dev, "unable to turn regulators on\n");
		return rc;
	}

	/* clear standby */
	rc = drv2667_write_reg(data->client,
			DRV2667_CNTL2_REG, data->cntl2_val);
	if (rc < 0) {
		dev_err(dev, "unable to clear standby\n");
		goto vreg_off;
	}

	return 0;
vreg_off:
	drv2667_vreg_on(data, false);
	return rc;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void drv2667_early_suspend(struct early_suspend *es)
{
	struct drv2667_data *data = container_of(es, struct drv2667_data, es);

	drv2667_suspend(&data->client->dev);
}

static void drv2667_late_resume(struct early_suspend *es)
{
	struct drv2667_data *data = container_of(es, struct drv2667_data, es);

	drv2667_resume(&data->client->dev);
}
#endif

static const struct dev_pm_ops drv2667_pm_ops = {
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = drv2667_suspend,
	.resume = drv2667_resume,
#endif
};
#endif

#ifdef CONFIG_OF
static int drv2667_parse_dt(struct device *dev, struct drv2667_pdata *pdata)
{
	struct property *prop;
	int rc;
	u32 temp;

	rc = of_property_read_string(dev->of_node, "ti,label", &pdata->name);
	/* set vibrator as default name */
	if (rc < 0)
		pdata->name = "vibrator";

	rc = of_property_read_u32(dev->of_node, "ti,gain", &temp);
	/* set gain as 0 */
	if (rc < 0)
		pdata->gain = 0;
	else
		pdata->gain = (u8) temp;

	rc = of_property_read_u32(dev->of_node, "ti,mode", &temp);
	/* set FIFO mode as default */
	if (rc < 0)
		pdata->mode = FIFO_MODE;
	else
		pdata->mode = (u8) temp;

	/* read wave sequence */
	if (pdata->mode == WAV_SEQ_MODE) {
		prop = of_find_property(dev->of_node, "ti,wav-seq", &temp);
		if (!prop) {
			dev_err(dev, "wav seq data not found");
			return -ENODEV;
		} else if (temp != DRV2667_WAV_SEQ_LEN) {
			dev_err(dev, "Invalid length of wav seq data\n");
			return -EINVAL;
		}
		memcpy(pdata->wav_seq, prop->value, DRV2667_WAV_SEQ_LEN);
	}

	rc = of_property_read_u32(dev->of_node, "ti,idle-timeout-ms", &temp);
	/* configure minimum idle timeout */
	if (rc < 0)
		pdata->idle_timeout_ms = DRV2667_MIN_IDLE_TIMEOUT_MS;
	else
		pdata->idle_timeout_ms = (u8) temp;

	rc = of_property_read_u32(dev->of_node, "ti,max-runtime-ms",
						&pdata->max_runtime_ms);
	/* configure one sec as default time */
	if (rc < 0)
		pdata->max_runtime_ms = MSEC_PER_SEC;

	return 0;
}
#else
static int drv2667_parse_dt(struct device *dev, struct drv2667_pdata *pdata)
{
	return -ENODEV;
}
#endif

static int drv2667_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct drv2667_data *data;
	struct drv2667_pdata *pdata;
	int rc, i;
	u8 val, fifo_seq_val, reg;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c is not supported\n");
		return -EIO;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct drv2667_pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		/* parse DT */
		rc = drv2667_parse_dt(&client->dev, pdata);
		if (rc) {
			dev_err(&client->dev, "DT parsing failed\n");
			return rc;
		}
	} else {
		pdata = client->dev.platform_data;
		if (!pdata) {
			dev_err(&client->dev, "invalid pdata\n");
			return -EINVAL;
		}
	}

	data = devm_kzalloc(&client->dev, sizeof(struct drv2667_data),
					GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	i2c_set_clientdata(client, data);

	data->client = client;
	data->max_runtime_ms = pdata->max_runtime_ms;
	mutex_init(&data->lock);
	INIT_WORK(&data->work, drv2667_worker);
	hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->timer.function = drv2667_timer;
	data->mode = pdata->mode;

	/* configure voltage regulators */
	rc = drv2667_vreg_config(data, true);
	if (rc) {
		dev_err(&client->dev, "unable to configure regulators\n");
		goto destroy_mutex;
	}

	/* turn on voltage regulators */
	rc = drv2667_vreg_on(data, true);
	if (rc) {
		dev_err(&client->dev, "unable to turn on regulators\n");
		goto deconfig_vreg;
	}

	rc = drv2667_read_reg(client, DRV2667_CNTL2_REG);
	if (rc < 0)
		goto vreg_off;

	/* set timeout, clear standby */
	val = (u8) rc;

	if (pdata->idle_timeout_ms < DRV2667_MIN_IDLE_TIMEOUT_MS ||
		pdata->idle_timeout_ms > DRV2667_MAX_IDLE_TIMEOUT_MS ||
		(pdata->idle_timeout_ms % DRV2667_MIN_IDLE_TIMEOUT_MS)) {
		dev_err(&client->dev, "Invalid idle timeout\n");
		goto vreg_off;
	}

	val = (val & DRV2667_TIMEOUT_MASK) |
		((pdata->idle_timeout_ms / DRV2667_MIN_IDLE_TIMEOUT_MS - 1) <<
		DRV2667_TIMEOUT_SHIFT);

	val &= DRV2667_STANDBY_MASK;

	rc = drv2667_write_reg(client, DRV2667_CNTL2_REG, val);
	if (rc < 0)
		goto vreg_off;

	/* cache control2 val */
	data->cntl2_val = val;

	/* program drv2667 registers */
	rc = drv2667_read_reg(client, DRV2667_CNTL1_REG);
	if (rc < 0)
		goto vreg_off;

	/* gain and input mode */
	val = (u8) rc;

	/* remove this check after adding support for these modes */
	if (data->mode == ANALOG_MODE || data->mode == RAM_SEQ_MODE) {
		dev_err(&data->client->dev, "Mode not supported\n");
		goto vreg_off;
	} else
		val &= ~DRV2667_INPUT_MUX_MASK; /* set digital mode */

	val = (val & DRV2667_GAIN_MASK) | (pdata->gain << DRV2667_GAIN_SHIFT);

	rc = drv2667_write_reg(client, DRV2667_CNTL1_REG, val);
	if (rc < 0)
		goto vreg_off;

	if (data->mode == FIFO_MODE) {
		/* Load a predefined pattern for FIFO mode */
		data->buf[0] = DRV2667_FIFO_REG;
		fifo_seq_val = DRV2667_VIB_START_VAL;

		for (i = 1; i < DRV2667_FIFO_SIZE - 1; i++, fifo_seq_val++)
			data->buf[i] = fifo_seq_val;

		data->time_chunk_ms = DRV2667_FIFO_CHUNK_MS;
	} else if (data->mode == WAV_SEQ_MODE) {
		u8 freq, rep, dur;

		/* program wave sequence from pdata */
		/* id to wave sequence 3, set page */
		rc = drv2667_write_reg(client, DRV2667_WAV_SEQ3_REG,
				pdata->wav_seq[DRV2667_WAV_SEQ_ID_IDX]);
		if (rc < 0)
			goto vreg_off;

		/* set page to wave form sequence */
		rc = drv2667_write_reg(client, DRV2667_PAGE_REG,
				pdata->wav_seq[DRV2667_WAV_SEQ_ID_IDX]);
		if (rc < 0)
			goto vreg_off;

		/* program waveform sequence */
		for (reg = 0, i = 0; i < DRV2667_WAV_SEQ_LEN - 1; i++, reg++) {
			rc = drv2667_write_reg(client, reg,
						pdata->wav_seq[i+1]);
			if (rc < 0)
				goto vreg_off;
		}

		/* set page back to normal register space */
		rc = drv2667_write_reg(client, DRV2667_PAGE_REG,
					DRV2667_REG_PAGE_ID);
		if (rc < 0)
			goto vreg_off;

		freq = pdata->wav_seq[DRV2667_WAV_SEQ_FREQ_IDX];
		rep = pdata->wav_seq[DRV2667_WAV_SEQ_REP_IDX];
		dur = pdata->wav_seq[DRV2667_WAV_SEQ_DUR_IDX];

		data->time_chunk_ms = (rep * dur * MSEC_PER_SEC) /
				(freq *	DRV2667_WAV_SEQ_FREQ_MIN);
	}

	drv2667_dump_regs(data, "new");

	/* register with timed output class */
	data->dev.name = pdata->name;
	data->dev.get_time = drv2667_get_time;
	data->dev.enable = drv2667_enable;

	rc = timed_output_dev_register(&data->dev);
	if (rc) {
		dev_err(&client->dev, "unable to register with timed_output\n");
		goto vreg_off;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->es.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + DRV2667_SUS_LEVEL;
	data->es.suspend = drv2667_early_suspend;
	data->es.resume = drv2667_late_resume;
	register_early_suspend(&data->es);
#endif
	return 0;

vreg_off:
	drv2667_vreg_on(data, false);
deconfig_vreg:
	drv2667_vreg_config(data, false);
destroy_mutex:
	mutex_destroy(&data->lock);
	return rc;
}

static int drv2667_remove(struct i2c_client *client)
{
	struct drv2667_data *data = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->es);
#endif
	mutex_destroy(&data->lock);
	timed_output_dev_unregister(&data->dev);
	hrtimer_cancel(&data->timer);
	cancel_work_sync(&data->work);
	drv2667_vreg_on(data, false);
	drv2667_vreg_config(data, false);

	return 0;
}

static const struct i2c_device_id drv2667_id_table[] = {
	{"drv2667", 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, drv2667_id_table);

#ifdef CONFIG_OF
static const struct of_device_id drv2667_of_id_table[] = {
	{.compatible = "ti,drv2667"},
	{ },
};
#else
#define drv2667_of_id_table NULL
#endif

static struct i2c_driver drv2667_i2c_driver = {
	.driver = {
		.name = "drv2667",
		.owner = THIS_MODULE,
		.of_match_table = drv2667_of_id_table,
#ifdef CONFIG_PM
		.pm = &drv2667_pm_ops,
#endif
	},
	.probe = drv2667_probe,
	.remove = drv2667_remove,
	.id_table = drv2667_id_table,
};

module_i2c_driver(drv2667_i2c_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TI DRV2667 chip driver");
