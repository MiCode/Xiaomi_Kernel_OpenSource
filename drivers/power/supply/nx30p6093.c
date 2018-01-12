/* Copyright (c) 2017 The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/alarmtimer.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/rtc.h>

#define NX30P6093_ID_REG			0x0
#define NX30P6093_VENDOR_ID_MASK		GENMASK(7, 3)
#define NX30P6093_VENDOR_ID_SHIFT		3
#define NX30P6093_VERSION_ID_MASK		GENMASK(2, 0)

#define NX30P6093_ENABLE_REG			0x01
#define NX30P6093_DETECT_EN			BIT(6)

#define NX30P6093_STATUS_REG			0x02
#define NX30P6093_PWRON_STS			BIT(7)
#define NX30P6093_IMPEDANCE_MASK		GENMASK(6, 5)
#define NX30P6093_IMPEDANCE_SHIFT		5
#define NX30P6093_IMPEDANCE_GOOD_VAL		1
#define NX30P6093_IMPEDANCE_BAD_VAL		3

#define NX30P6093_INTR_MASK_REG			0x04
#define NX30P6093_OVER_TAG_STS_INTR_MASK	BIT(6)
#define NX30P6093_TMR_OUT_STS_INTR_MASK		BIT(5)

#define NX30P6093_VIN_ISOURCE_REG		0x06
#define NX30P6093_VIN_ISOURCE_MASK		GENMASK(3, 0)

#define NX30P6093_ISOURCE_TIMING_REG		0x07
#define NX30P6093_ISOURCE_TDET_MASK		GENMASK(7, 4)
#define NX30P6093_ISOURCE_TDET_SHIFT		4
#define NX30P6093_ISOURCE_TDUTY_MASK		GENMASK(3, 0)

#define NX30P6093_VIN_VOLTAGE_TAG_REG		0x09
#define NX30P6093_VIN_VOLTAGE_TAG_MASK	GENMASK(7, 0)

#define NX30P6093_SLEW_RATE_TUNE_REG		0x0f

/* short duration is 5sec and long duration is 5hrs */
#define NX30P6093_LONG_WAKEUP_SEC		18000
#define NX30P6093_SHORT_WAKEUP_MS		5000

/* Default Tduty = 5mins when always-on detection is configured */
#define NX30P6093_ISOURCE_ALWAYS_ON_TDUTY_MS	300000

/* configuration data */
#define NX30P6093_VIN_ISOURCE_VAL		0xd
#define NX30P6093_VIN_VOLTAGE_TAG_VAL		0xad
#define NX30P6093_ISOURCE_TDET_VAL		0x5
#define NX30P6093_ISOURCE_TDUTY_VAL		0x0
#define NX30P6093_ISOURCE_TDET_MS		10

struct nx30p6093_info {
	struct device		*dev;
	struct regmap		*regmap;
	struct power_supply	*usb_psy;
	struct alarm		alarm_timer;
	struct delayed_work	config_impedance_detect;
	struct mutex		lock;
	struct dentry		*debugfs;
	u8			tduty_val;
	int			irq;

	/* status data */
	bool			irq_waiting;
	bool			high_impedance;
	bool			detection_on;
	bool			always_on;
	bool			use_alarm;
	bool			suspended;

	/* timer configuration */
	u64			long_wakeup_ms;
	u64			short_wakeup_ms;
};

static const int nx30p6093_tduty_ms[] = {0, 10, 20, 50, 100, 200, 500, 1000,
					 2000, 3000, 6000, 12000, 30000, 60000,
					 120000, 300000};

static const struct regmap_config nx30p6093_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= NX30P6093_SLEW_RATE_TUNE_REG,
};

static int nx30p6093_dump_regs(struct nx30p6093_info *info)
{
	unsigned int val;
	int i, rc = 0;

	for (i = 0; i <= NX30P6093_SLEW_RATE_TUNE_REG; ++i) {
		rc = regmap_read(info->regmap, i, &val);
		if (rc < 0)
			return rc;
		pr_debug("NX30P6093(0x%02x) = 0x%02x\n", i, (uint8_t)val);
	}

	return rc;
}

static inline void nx30p6093_config_alarm(struct nx30p6093_info *info,
			u64 wakeup_ms)
{
	if (!info->use_alarm)
		return;

	alarm_start_relative(&info->alarm_timer, ms_to_ktime(wakeup_ms));
}

static int nx30p6093_impedance_detect(struct nx30p6093_info *info, bool enable)
{
	int rc = 0;

	if (enable == info->detection_on)
		return rc;

	rc = regmap_update_bits(info->regmap, NX30P6093_ENABLE_REG,
				NX30P6093_DETECT_EN,
				enable ? NX30P6093_DETECT_EN : 0);
	if (rc < 0) {
		pr_err("failed to %s VIN impedance detection, rc=%d\n",
			enable ? "enable" : "disable", rc);
		return rc;
	}

	/* wait for 3ms for HW activation and enters detection standby mode */
	usleep_range(3000, 3100);

	/* config Isource to VIN */
	rc = regmap_update_bits(info->regmap, NX30P6093_VIN_ISOURCE_REG,
				NX30P6093_VIN_ISOURCE_MASK,
				enable ? NX30P6093_VIN_ISOURCE_VAL : 0);
	if (rc < 0) {
		pr_err("failed to configure Vin Isource register, rc=%d\n", rc);
		return rc;
	}

	info->detection_on = enable;
	nx30p6093_dump_regs(info);

	return rc;
}

static int nx30p6093_read_impedance_status(struct nx30p6093_info *info)
{
	union power_supply_propval psp_val;
	unsigned int val;
	u8 impedance;
	int rc;

	/* Read status register */
	rc = regmap_read(info->regmap, NX30P6093_STATUS_REG, &val);
	if (rc < 0) {
		pr_err("failed to read status register, rc=%d\n", rc);
		return rc;
	}

	if (val & NX30P6093_PWRON_STS) {
		/* VBUS present */
		return rc;
	}

	impedance = (val & NX30P6093_IMPEDANCE_MASK)
			>> NX30P6093_IMPEDANCE_SHIFT;
	if (impedance == NX30P6093_IMPEDANCE_GOOD_VAL && info->high_impedance) {
		info->high_impedance = false;

		/* enable the type-C CC detection */
		psp_val.intval = 0;
		rc = power_supply_set_property(info->usb_psy,
					POWER_SUPPLY_PROP_MOISTURE_DETECTED,
					&psp_val);
	} else if (impedance == NX30P6093_IMPEDANCE_BAD_VAL) {
		info->high_impedance = true;

		/* disable the type-C CC detection */
		psp_val.intval = 1;
		rc = power_supply_set_property(info->usb_psy,
					POWER_SUPPLY_PROP_MOISTURE_DETECTED,
					&psp_val);
	}

	return rc;
}

static irqreturn_t nx30p6093_irq_handler(int irq, void *data)
{
	struct nx30p6093_info *info = data;

	mutex_lock(&info->lock);

	info->irq_waiting = true;
	if (info->suspended) {
		pr_debug("IRQ triggered before device-resume\n");
		disable_irq_nosync(irq);
		mutex_unlock(&info->lock);
		return IRQ_HANDLED;
	}
	info->irq_waiting = false;
	mutex_unlock(&info->lock);

	nx30p6093_read_impedance_status(info);

	if (info->high_impedance) {
		disable_irq_nosync(irq);
		/* set up next detection event */
		nx30p6093_config_alarm(info,
				NX30P6093_ISOURCE_ALWAYS_ON_TDUTY_MS);
	}

	return IRQ_HANDLED;
}

static int nx30p6093_trigger_impedance_detect(struct nx30p6093_info *info)
{
	int rc;

	if (!info->always_on) {
		rc = nx30p6093_impedance_detect(info, true);
		if (rc < 0) {
			pr_err("start impedance detection failed, rc=%d\n", rc);
			return rc;
		}

		/* wait for the detection complete(Tdet time). */
		usleep_range(NX30P6093_ISOURCE_TDET_MS * USEC_PER_MSEC,
			     NX30P6093_ISOURCE_TDET_MS * USEC_PER_MSEC + 100);
	}

	/* Read and process the detection result. */
	rc = nx30p6093_read_impedance_status(info);
	if (rc < 0)
		pr_err("impedance status read failed, rc=%d\n", rc);

	if (!info->always_on) {
		rc = nx30p6093_impedance_detect(info, false);
		if (rc < 0)
			pr_err("stop impedance detection failed, rc=%d\n", rc);
	}

	return rc;
}

static void nx30p6093_config_impedance_detect(struct work_struct *work)
{
	struct nx30p6093_info *info = container_of(work, struct nx30p6093_info,
						config_impedance_detect.work);
	u64 wakeup_ms = 0;

	mutex_lock(&info->lock);
	if (info->suspended) {
		/*
		 * Defer the work as the device is still in suspend state and
		 * not yet resumed.
		 */
		schedule_delayed_work(&info->config_impedance_detect,
				msecs_to_jiffies(500));
		mutex_unlock(&info->lock);
		return;
	}

	nx30p6093_trigger_impedance_detect(info);

	if (info->always_on) {
		if (info->high_impedance) {
			/*
			 * Bad impedance is not cleared yet.
			 * Set up a next detection event.
			 */
			nx30p6093_config_alarm(info,
					NX30P6093_ISOURCE_ALWAYS_ON_TDUTY_MS);
		} else {
			/* Bad impedance is cleared. Enable detection IRQ */
			enable_irq(info->irq);
		}
	} else {
		wakeup_ms = info->high_impedance ? info->short_wakeup_ms
						 : info->long_wakeup_ms;

		/* Set up a next detection event */
		nx30p6093_config_alarm(info, wakeup_ms);
	}

	mutex_unlock(&info->lock);
	pm_relax(info->dev);
}

static enum alarmtimer_restart
	nx30p6093_process_alarm_event(struct alarm *alarm, ktime_t now)
{
	struct nx30p6093_info *info = container_of(alarm, struct nx30p6093_info,
						alarm_timer);
	union power_supply_propval val;
	int rc;

	/* Read USB plugged-in */
	rc = power_supply_get_property(info->usb_psy, POWER_SUPPLY_PROP_PRESENT,
				       &val);
	if (rc < 0) {
		pr_err("read usb present failed, rc=%d\n", rc);
		return ALARMTIMER_RESTART;
	}

	if (val.intval) {
		/*
		 * usb present - skip impedance detection and set up
		 * next detection event.
		 */
		nx30p6093_config_alarm(info, info->long_wakeup_ms);
	} else {
		pm_stay_awake(info->dev);
		schedule_delayed_work(&info->config_impedance_detect, 0);
	}

	return ALARMTIMER_NORESTART;
}

static int nx30p6093_init_config(struct nx30p6093_info *info)
{
	int rc;
	u8 val;

	/* Enable OVER_TAG_STATUS interrupt if always-on detection enabled */
	rc = regmap_write(info->regmap, NX30P6093_INTR_MASK_REG,
			  info->always_on ? NX30P6093_OVER_TAG_STS_INTR_MASK
					  : 0);
	if (rc < 0) {
		pr_err("failed to enable timer out status interrupt, rc=%d\n",
			rc);
		return rc;
	}

	/* config Isource timing (Default: 10ms) */
	val = NX30P6093_ISOURCE_TDET_VAL << NX30P6093_ISOURCE_TDET_SHIFT;
	rc = regmap_update_bits(info->regmap, NX30P6093_ISOURCE_TIMING_REG,
				NX30P6093_ISOURCE_TDET_MASK, val);
	if (rc < 0) {
		pr_err("failed to configure Isource timing, rc=%d\n", rc);
		return rc;
	}

	/*
	 * config Isource Tduty timing;
	 * Default value:
	 *	1) One shot - if Periodic detection enabled
	 *	2) 5 mins   - if Always-on detection enabled
	 */
	rc = regmap_update_bits(info->regmap, NX30P6093_ISOURCE_TIMING_REG,
				NX30P6093_ISOURCE_TDUTY_MASK,
				info->always_on ? info->tduty_val
					: NX30P6093_ISOURCE_TDUTY_VAL);
	if (rc < 0) {
		pr_err("failed to configure Isource Tduty timing, rc=%d\n", rc);
		return rc;
	}

	/* config VIN voltage tag (Default: 0xad) */
	rc = regmap_update_bits(info->regmap, NX30P6093_VIN_VOLTAGE_TAG_REG,
				NX30P6093_VIN_VOLTAGE_TAG_MASK,
				NX30P6093_VIN_VOLTAGE_TAG_VAL);
	if (rc < 0) {
		pr_err("failed to configure Vin voltage tag register, rc=%d\n",
			rc);
		return rc;
	}

	return 0;
}

static int nx30p6093_dt_init(struct i2c_client *client,
			struct nx30p6093_info *info)
{
	struct device_node *of_node = client->dev.of_node;
	int i, tduty_ms;
	u32 long_wakeup_sec, short_wakeup_ms;

	if (of_property_read_bool(of_node, "nxp,always-on-detect")) {
		info->always_on = true;
		tduty_ms = NX30P6093_ISOURCE_ALWAYS_ON_TDUTY_MS;
		of_property_read_u32(of_node, "nxp,always-on-tduty-ms",
				     &tduty_ms);
		for (i = 0; i < ARRAY_SIZE(nx30p6093_tduty_ms); i++) {
			if (tduty_ms <= nx30p6093_tduty_ms[i]) {
				info->tduty_val = (uint8_t)i;
				break;
			}
		}

		if (!info->tduty_val) {
			pr_err("invalid nxp,always-on-tduty-ms = %d\n",
				tduty_ms);
			return -EINVAL;
		}
	} else {
		long_wakeup_sec = NX30P6093_LONG_WAKEUP_SEC;
		short_wakeup_ms = NX30P6093_SHORT_WAKEUP_MS;

		of_property_read_u32(of_node, "nxp,long-wakeup-sec",
				  &long_wakeup_sec);
		of_property_read_u32(of_node, "nxp,short-wakeup-ms",
				  &short_wakeup_ms);
		if (!long_wakeup_sec || !short_wakeup_ms) {
			pr_err("Invalid wakeup timings are configured\n");
			return -EINVAL;
		}

		info->long_wakeup_ms = long_wakeup_sec * MSEC_PER_SEC;
		info->short_wakeup_ms = short_wakeup_ms;
	}

	return 0;
}

static int nx30p6093_trigger_detect(struct seq_file *file, void *data)
{
	struct nx30p6093_info *info = file->private;

	if (info->always_on)
		return 0;

	mutex_lock(&info->lock);
	nx30p6093_trigger_impedance_detect(info);
	seq_printf(file, "%s impedance detected\n",
		   info->high_impedance ? "BAD" : "GOOD");
	mutex_unlock(&info->lock);

	return 0;
}

static int nx30p6093_trigger_detect_open(struct inode *inode, struct file *file)
{
	return single_open(file, nx30p6093_trigger_detect, inode->i_private);
}

static const struct file_operations nx30p6093_trigger_detect_fops = {
	.owner		= THIS_MODULE,
	.open		= nx30p6093_trigger_detect_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void nx30p6093_debugfs_init(struct nx30p6093_info *info)
{
	struct dentry *temp;

	/* debugfs */
	info->debugfs = debugfs_create_dir("nx30p6093", NULL);
	if (!info->debugfs) {
		pr_err("Couldn't create debug dir\n");
		return;
	}

	temp = debugfs_create_file("trigger_detection", 0644, info->debugfs,
				info, &nx30p6093_trigger_detect_fops);
	if (IS_ERR_OR_NULL(temp)) {
		pr_err("debugfs_nx30p6093_reg_addr_fops debugfs file creation failed\n");
		debugfs_remove_recursive(info->debugfs);
	}
}

#if CONFIG_PM
static int nx30p6093_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nx30p6093_info *info = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&info->config_impedance_detect);

	mutex_lock(&info->lock);
	info->suspended = true;
	mutex_unlock(&info->lock);

	return 0;
}

static int nx30p6093_suspend_noirq(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nx30p6093_info *info = i2c_get_clientdata(client);

	if (info->irq_waiting) {
		pr_err_ratelimited("Aborting suspend, an interrupt was detected while suspending\n");
		return -EBUSY;
	}
	return 0;
}

static int nx30p6093_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nx30p6093_info *info = i2c_get_clientdata(client);

	mutex_lock(&info->lock);
	info->suspended = false;
	if (info->irq_waiting) {
		mutex_unlock(&info->lock);
		nx30p6093_irq_handler(client->irq, info);
		enable_irq(client->irq);
	} else {
		mutex_unlock(&info->lock);
	}

	return 0;
}
#else
static int nx30p6093_suspend(struct device *dev)
{
	return 0;
}

static int nx30p6093_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops nx30p6093_pm_ops = {
	.suspend	= nx30p6093_suspend,
	.suspend_noirq	= nx30p6093_suspend_noirq,
	.resume		= nx30p6093_resume,
};

static int nx30p6093_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct nx30p6093_info *info;
	unsigned int val;
	int vendor_id, version_id, rc = 0;

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = &client->dev;
	info->regmap = devm_regmap_init_i2c(client, &nx30p6093_regmap_config);
	if (IS_ERR(info->regmap)) {
		pr_err("Error in allocating regmap, rc=%ld\n",
			PTR_ERR(info->regmap));
		return PTR_ERR(info->regmap);
	}

	rc = regmap_read(info->regmap, NX30P6093_ID_REG, &val);
	if (rc < 0) {
		pr_err("Unable to identify NX30P6093, rc=%d\n", rc);
		return rc;
	}
	vendor_id = (val & NX30P6093_VENDOR_ID_MASK)
			  >> NX30P6093_VENDOR_ID_SHIFT;
	version_id = val & NX30P6093_VERSION_ID_MASK;

	info->usb_psy = power_supply_get_by_name("usb");
	if (!info->usb_psy) {
		pr_err("USB psy not found\n");
		return -EPROBE_DEFER;
	}

	i2c_set_clientdata(client, info);
	mutex_init(&info->lock);
	INIT_DELAYED_WORK(&info->config_impedance_detect,
			  nx30p6093_config_impedance_detect);

	rc = nx30p6093_dt_init(client, info);
	if (rc < 0) {
		pr_err("device tree parsing failed, rc=%d\n", rc);
		return rc;
	}

	rc = nx30p6093_init_config(info);
	if (rc < 0) {
		pr_err("initial configuration programming failed, rc=%d\n", rc);
		return rc;
	}

	if (alarmtimer_get_rtcdev()) {
		/* Initialize alarm timer */
		info->use_alarm = true;
		alarm_init(&info->alarm_timer, ALARM_BOOTTIME,
				nx30p6093_process_alarm_event);
	} else {
		pr_err("alarm initialization failed\n");
		return -ENODEV;
	}

	if (info->always_on) {
		/* Moisture detect irq configuration */
		if (client->irq) {
			info->irq = client->irq;
			rc = devm_request_threaded_irq(&client->dev,
					client->irq, NULL,
					nx30p6093_irq_handler,
					IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
					client->name, info);
			if (rc < 0) {
				pr_err("Failed Moisture detect irq(%d) request, rc=%d\n",
					client->irq, rc);
				return rc;
			}
			enable_irq_wake(client->irq);
		} else {
			pr_err("Moisture detect irq not defined\n");
			return -EINVAL;
		}

		nx30p6093_impedance_detect(info, true);
	} else {
		/* Run impedance detection for first time */
		pm_stay_awake(info->dev);
		schedule_delayed_work(&info->config_impedance_detect, 0);
	}

	nx30p6093_debugfs_init(info);

	if (info->always_on)
		pr_info("NXP NX30P6093 Vendor(%d), Version(%d), configured to Always-on detection\n",
			vendor_id, version_id);
	else
		pr_info("NXP NX30P6093 Vendor(%d), Version(%d), configured to periodic detection with Short_wakeup = %llu ms and Long_wakeup = %llu sec\n",
			vendor_id, version_id, info->short_wakeup_ms,
			info->long_wakeup_ms / MSEC_PER_SEC);

	return 0;
}

static int nx30p6093_remove(struct i2c_client *client)
{
	struct nx30p6093_info *info = i2c_get_clientdata(client);

	debugfs_remove_recursive(info->debugfs);
	cancel_delayed_work_sync(&info->config_impedance_detect);

	if (info->use_alarm)
		alarm_cancel(&info->alarm_timer);

	return 0;
}

static const struct of_device_id nx30p6093_table[] = {
	{ .compatible = "nxp,nx30p6093" },
	{ },
};
MODULE_DEVICE_TABLE(of, nx30p6093_table);

static const struct i2c_device_id nx30p6093_id[] = {
	{"nx30p6093", -1},
	{ },
};
MODULE_DEVICE_TABLE(i2c, nx30p6093_id);

static struct i2c_driver nx30p6093_driver = {
	.driver = {
		.name = "nxp,nx30p6093",
		.owner = THIS_MODULE,
		.of_match_table = nx30p6093_table,
		.pm = &nx30p6093_pm_ops,
	},
	.probe = nx30p6093_probe,
	.remove = nx30p6093_remove,
	.id_table = nx30p6093_id,
};
module_i2c_driver(nx30p6093_driver);

MODULE_DESCRIPTION("NX30P6093 driver");
MODULE_LICENSE("GPL v2");
