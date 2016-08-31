/*
 * Cellwise CW201x Fuel gauge driver
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Shardar Shariff Md <smohammed@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power/cw201x_battery.h>
#include <linux/power/battery-charger-gauge-comm.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>

#define CW201x_REG_VERSION             0x0
#define CW201x_REG_VCELL               0x2
#define CW201x_REG_SOC                 0x4
#define CW201x_REG_RRT_ALERT           0x6
#define CW201x_REG_CONFIG              0x8
#define CW201x_REG_MODE                0xA
#define CW201x_REG_BATINFO             0x10

#define CW201x_MODE_SLEEP_MASK         (0x3<<6)
#define CW201x_MODE_SLEEP              (0x3<<6)
#define CW201x_MODE_NORMAL             (0x0)
#define CW201x_MODE_QUICK_START        (0x3<<4)
#define CW201x_MODE_RESTART_MASK       (0xF)
#define CW201x_MODE_RESTART_VAL        (0xF)

#define CW201x_CONFIG_UPDATE_FLG       (0x1 << 1)
#define CW201x_CONFIG_ATHD_MASK        (0x1F << 3)
#define CW201x_CONFIG_UPDATE_MASK      (0x2)
#define CW201x_ATHD_SHIFT              3

#define CW201x_ALERT_FLAG_BIT          (0x1 << 7)
#define CW201x_TIME_TO_EMPTY_MASK      (0x1FFF)

#define CW201x_BATTERY_FULL            100
#define CW201x_BATTERY_LOW             15
#define CW201x_WORK_DELAY              30

/* Voltage resolution is 305uV per each
   vcell bit */
#define CW201x_VOLTAGE_RESOL_MULTI     312
#define CW201x_VOLTAGE_RESOL_DIVIDER   1024

/* Total time to stabilize the chip is 3 sec
   or if soc != 255 which indicates chip is
   stabiized, so making 30 loops * 100 msec
   and every loop checks if its stabilized.
 */
#define CW201x_STABILIZE_SLEEP_LOOP    30
#define CW201x_STABILIZE_SLEEP_TIME    100
#define CW201x_NON_STABILIZE_SOC       255

/* Get middle value in 3 values */
#define MID3(a, b, c) ((max((a), (b)) < (c)) ? max((a), (b)) :\
	((min((a), (b)) < (c)) ? (c) : min((a), (b))))

struct cw201x_chip {
	struct i2c_client *client;
	struct delayed_work work;
	struct power_supply battery;
	struct cw201x_platform_data *pdata;
	struct battery_gauge_dev *bg_dev;
	struct regmap *regmap;

	/* battery voltage */
	int vcell;
	/* battery capacity */
	int soc;
	/* State Of Charge */
	int status;
	/* battery health */
	int health;
	/* battery capacity */
	int capacity_level;
	/* Time to empty */
	int time_to_empty;

	int lasttime_soc;
	int lasttime_status;
	int charge_complete;
};

static const struct regmap_config cw201x_regmap_config = {
	.reg_bits       = 8,
	.val_bits       = 8,
	.max_register   = CW201x_MAX_REGS,
};

static int cw201x_read(struct cw201x_chip *chip, u8 reg,
		void *reg_val, int len)
{
	int ret;
	u16 val;

	ret = regmap_raw_read(chip->regmap, reg, reg_val, len);
	if (ret < 0) {
		dev_err(&chip->client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}

	if (len == 2) {
		val = *(u16 *)reg_val;
		*(u16 *)reg_val = swab16(val & 0xffff);
	}

	return ret;
}

static int cw201x_update_battery_profile(struct cw201x_chip *chip)
{
	int ret;
	int loop;
	u8 profile_tbl[CW201x_PROFILE_SIZE];
	bool change;

	/* update new battery profile tbl*/
	ret = regmap_bulk_write(chip->regmap, CW201x_REG_BATINFO,
			&chip->pdata->battery_profile_tbl[0],
			CW201x_PROFILE_SIZE);
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"battery_profile write err %d\n", ret);
		return ret;
	}

	/* readback */
	ret = regmap_bulk_read(chip->regmap, CW201x_REG_BATINFO,
				&profile_tbl[0], CW201x_PROFILE_SIZE);
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"battery_profile read err %d\n", ret);
		return ret;
	}

	/* check battery profile */
	for (loop = 0; loop < CW201x_PROFILE_SIZE; loop++) {
		if (profile_tbl[loop] !=
				chip->pdata->battery_profile_tbl[loop]) {
			dev_err(&chip->client->dev,
					"battery_profile mismatch\n");
			return -EIO;
		}
	}

	/* set update flag to use new battery profile */
	ret = regmap_update_bits_check(chip->regmap, CW201x_REG_CONFIG,
		CW201x_CONFIG_UPDATE_MASK, CW201x_CONFIG_UPDATE_FLG, &change);
	if (ret) {
		dev_err(&chip->client->dev,
			"update flag not set. chng?:%d, err %d\n", change, ret);
		return ret;
	}

	/* To reset the chip, update reset bits with 0xF and then with 0*/
	ret = regmap_update_bits(chip->regmap, CW201x_REG_CONFIG,
			CW201x_MODE_RESTART_MASK, CW201x_MODE_RESTART_VAL);
	if (ret) {
		dev_err(&chip->client->dev,
				"reset chip failed, err %d\n", ret);
		return ret;
	}

	msleep(20);
	ret = regmap_update_bits(chip->regmap, CW201x_REG_CONFIG,
			CW201x_MODE_RESTART_MASK, 0);
	if (ret) {
		dev_err(&chip->client->dev,
				"reset chip failed, err %d\n", ret);
		return ret;
	}

	return 0;
}

static int cw201x_initialize(struct cw201x_chip *chip)
{
	int ret;
	int loop;
	u8 alert_threshold;
	u8 reg_val;
	u8 profile_tbl[CW201x_PROFILE_SIZE];

	/* Read config reg. to check if chip is in sleep mode */
	ret = cw201x_read(chip, CW201x_REG_MODE,
			&reg_val, sizeof(reg_val));
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"mode reg. read failed, err %d\n", ret);
		return ret;
	}

	/* Wake up if chip is in sleep mode */
	if ((reg_val & CW201x_MODE_SLEEP_MASK) == CW201x_MODE_SLEEP) {
		reg_val = CW201x_MODE_NORMAL;
		ret = regmap_write(chip->regmap, CW201x_REG_MODE, reg_val);
		if (ret < 0) {
			dev_err(&chip->client->dev,
					"wake up failed, err %d\n", ret);
			return ret;
		}
	}

	/* Read config reg. to check if alert threshold is configured */
	ret = cw201x_read(chip, CW201x_REG_CONFIG,
			&reg_val, sizeof(reg_val));
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"config reg. read failed, err %d\n", ret);
		return ret;
	}

	alert_threshold = (chip->pdata->alert_threshold << CW201x_ATHD_SHIFT);
	if ((reg_val & CW201x_CONFIG_ATHD_MASK) != alert_threshold) {
		/* Setting the new ATHD */
		reg_val &= ~CW201x_CONFIG_ATHD_MASK;  /* clear ATHD */
		reg_val |= alert_threshold;  /* set ATHD */
		ret = regmap_write(chip->regmap, CW201x_REG_CONFIG,
				reg_val);
		if (ret < 0) {
			dev_err(&chip->client->dev,
					"updating athd failed, err %d\n", ret);
			return ret;
		}
	}

	/* Read config reg to check the update flag */
	ret = cw201x_read(chip, CW201x_REG_CONFIG,
			&reg_val, sizeof(reg_val));
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"config reg. read failed, err %d\n", ret);
		return ret;
	}

	if (!(reg_val & CW201x_CONFIG_UPDATE_FLG)) {
		/* update flag is not set, load the battery profile */
		ret = cw201x_update_battery_profile(chip);
		if (ret < 0) {
			dev_err(&chip->client->dev,
					"batt. profile updation failed,err %d\n",
					ret);
			return ret;
		}
		dev_info(&chip->client->dev, "battery profile loaded\n");
	} else {
		/* update flag is set, read and compare battery profile */
		ret = regmap_bulk_read(chip->regmap, CW201x_REG_BATINFO,
				&profile_tbl[0], CW201x_PROFILE_SIZE);
		if (ret < 0) {
			dev_err(&chip->client->dev,
					"read battery_profile: err %d\n", ret);
			return ret;
		}
		for (loop = 0; loop < CW201x_PROFILE_SIZE; loop++) {
			if (profile_tbl[loop] !=
					chip->pdata->battery_profile_tbl[loop])
				break;
		}
		if (loop != CW201x_PROFILE_SIZE) {
			/* update flag is set, but battery profile mismatch */
			ret = cw201x_update_battery_profile(chip);
			if (ret < 0) {
				dev_err(&chip->client->dev,
						"batt. profile updation failed, err %d\n",
						ret);
				return ret;
			}
		} else
			dev_info(&chip->client->dev, "battery profile matched\n");
	}

	/* Loop if soc = 0xFF or till 3 sec for chip to stablize*/
	for (loop = 0; loop < CW201x_STABILIZE_SLEEP_LOOP; loop++) {
		ret = cw201x_read(chip, CW201x_REG_SOC,
				&reg_val, sizeof(reg_val));
		if (ret < 0) {
			dev_err(&chip->client->dev,
					"stabilize check failed,err %d\n", ret);
			return ret;
		}
		if (reg_val != CW201x_NON_STABILIZE_SOC)
			break;

		msleep(CW201x_STABILIZE_SLEEP_TIME);
	}

	return 0;
}

static int cw201x_get_vcell(struct cw201x_chip *chip)
{
	int ret;
	u16 vcell_1, vcell_2, vcell_3, vcell;
	int voltage;

	/* Read vcell 3 times */
	ret = cw201x_read(chip, CW201x_REG_VCELL,
			&vcell_1, sizeof(vcell_1));
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"vcell reg. read_1 failed, err %d\n", ret);
		return ret;
	}

	ret = cw201x_read(chip, CW201x_REG_VCELL,
			&vcell_2, sizeof(vcell_2));
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"vcell reg. read_2 failed, err %d\n", ret);
		return ret;
	}

	ret = cw201x_read(chip, CW201x_REG_VCELL,
			&vcell_3, sizeof(vcell_3));
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"vcell reg. read_3 failed, err %d\n", ret);
		return ret;
	}

	/* Use the middle value of 3 vcells read */
	vcell = MID3(vcell_1, vcell_2, vcell_3);

	voltage = (vcell * CW201x_VOLTAGE_RESOL_MULTI) /
		CW201x_VOLTAGE_RESOL_DIVIDER;

	/* Conver to milli volts */
	voltage = voltage * 1000;
	chip->vcell = voltage;

	return 0;
}

static int cw201x_update_time_to_empty(struct cw201x_chip *chip)
{
	int ret;
	u16 time_to_empty;

	ret = cw201x_read(chip, CW201x_REG_RRT_ALERT,
			&time_to_empty, sizeof(time_to_empty));
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"tte reg. read failed, err %d\n", ret);
		return ret;
	}

	chip->time_to_empty = (time_to_empty & CW201x_TIME_TO_EMPTY_MASK);

	return 0;
}

static int cw201x_get_soc(struct cw201x_chip *chip)
{
	int ret;
	u8 reg_val;

	ret = cw201x_read(chip, CW201x_REG_SOC,
			&reg_val, sizeof(reg_val));
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"soc reg. read failed, err %d\n", ret);
		return ret;
	}

	chip->soc = reg_val;

	if (chip->soc >= CW201x_BATTERY_FULL && chip->charge_complete != 1)
		chip->soc = CW201x_BATTERY_FULL-1;

	if (chip->status == POWER_SUPPLY_STATUS_FULL && chip->charge_complete) {
		chip->soc = CW201x_BATTERY_FULL;
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		chip->health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (chip->soc < CW201x_BATTERY_LOW) {
		chip->status = chip->lasttime_status;
		chip->health = POWER_SUPPLY_HEALTH_DEAD;
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	} else {
		chip->status = chip->lasttime_status;
		chip->health = POWER_SUPPLY_HEALTH_GOOD;
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	}

	return 0;
}

static void cw201x_work(struct work_struct *work)
{
	struct cw201x_chip *chip;
	int ret;

	chip = container_of(work, struct cw201x_chip, work.work);

	ret = cw201x_get_vcell(chip);
	if (ret)
		dev_err(&chip->client->dev,
				"get_vcell failed, err %d\n", ret);

	ret = cw201x_get_soc(chip);
	if (ret)
		dev_err(&chip->client->dev,
				"get_soc failed, err %d\n", ret);

	ret = cw201x_update_time_to_empty(chip);
	if (ret)
		dev_err(&chip->client->dev,
				"update_time_to_empty failed, err %d\n", ret);

	if ((chip->soc != chip->lasttime_soc) ||
			(chip->status != chip->lasttime_status)) {
		chip->lasttime_soc = chip->soc;
		power_supply_changed(&chip->battery);
	}

	schedule_delayed_work(&chip->work, CW201x_WORK_DELAY);
}

static int cw2015_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct cw201x_chip *chip = container_of(psy,
			struct cw201x_chip, battery);
	int ret = 0;
	int temp;

	switch (psp) {
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->status;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chip->vcell;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chip->soc;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = chip->capacity_level;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		val->intval = chip->time_to_empty;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = battery_gauge_get_battery_temperature(chip->bg_dev,
				&temp);
		if (ret < 0)
			return -EINVAL;
		val->intval = temp;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->vcell ? 1 : 0;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int cw201x_check_battery(struct cw201x_chip *chip)
{
	u8 version;
	int ret;

	ret = cw201x_read(chip, CW201x_REG_VERSION,
			&version, sizeof(version));
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"version reg. read failed, err %d\n", ret);
		return ret;
	}

	dev_info(&chip->client->dev,
			"CW201x Fuel-Gauge Ver 0x%x\n", version);
	return 0;
}

static irqreturn_t cw201x_irq(int id, void *dev)
{
	struct cw201x_chip *chip = dev;
	struct i2c_client *client = chip->client;
	u8 reg_val;
	int ret;

	ret = cw201x_read(chip, CW201x_REG_RRT_ALERT,
			&reg_val, sizeof(reg_val));
	if (ret < 0) {
		dev_err(&client->dev,
				"alert reg. read failed\n");
		return IRQ_HANDLED;
	}

	if (reg_val & CW201x_ALERT_FLAG_BIT) {
		dev_err(&client->dev,
				"alert flag set, battery capacity is Low\n");
		ret = cw201x_get_vcell(chip);
		if (ret)
			dev_err(&client->dev,
					"get_vcell failed, err %d\n", ret);
		ret = cw201x_get_soc(chip);
		if (ret)
			dev_err(&client->dev,
					"get_soc failed, err %d\n", ret);
		chip->lasttime_soc = chip->soc;
		power_supply_changed(&chip->battery);
		/* Clear the ALERT flag */
		reg_val = reg_val & ~(CW201x_ALERT_FLAG_BIT);
		ret = regmap_write(chip->regmap,
				CW201x_REG_RRT_ALERT, reg_val);
		if (ret < 0) {
			dev_err(&client->dev,
					"clearing the alert flag failed\n");
			return IRQ_HANDLED;
		}
	}

	return IRQ_HANDLED;
}

static enum power_supply_property cw2015_battery_props[] = {
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_PRESENT,
};

static int cw201x_update_battery_status(struct battery_gauge_dev *bg_dev,
		enum battery_charger_status status)
{
	struct cw201x_chip *chip = battery_gauge_get_drvdata(bg_dev);

	if (status == BATTERY_CHARGING)
		chip->status = POWER_SUPPLY_STATUS_CHARGING;
	else if (status == BATTERY_CHARGING_DONE) {
		chip->charge_complete = 1;
		chip->soc = CW201x_BATTERY_FULL;
		chip->status = POWER_SUPPLY_STATUS_FULL;
		power_supply_changed(&chip->battery);
		return 0;
	} else {
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
		chip->charge_complete = 0;
	}
	chip->lasttime_status = chip->status;
	power_supply_changed(&chip->battery);
	return 0;
}


static struct battery_gauge_ops cw201x_bg_ops = {
	.update_battery_status = cw201x_update_battery_status,
};

static struct battery_gauge_info cw201x_bgi = {
	.cell_id = 0,
	.bg_ops = &cw201x_bg_ops,
};

static struct cw201x_platform_data *cw201x_parse_dt(struct device *dev)
{
	struct cw201x_platform_data *pdata;
	struct device_node *np = dev->of_node;
	u32 val, val_array[CW201x_PROFILE_SIZE];
	int ret, i;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	ret = of_property_read_string(np, "tz-name", &pdata->tz_name);
	if (ret < 0)
		return ERR_PTR(ret);

	ret = of_property_read_u32(np, "alert-threshold", &val);
	if (ret < 0)
		return ERR_PTR(ret);

	pdata->alert_threshold = val;

	ret = of_property_read_u32_array(np, "profile-tbl", val_array,
			CW201x_PROFILE_SIZE);
	if (ret < 0)
		return ERR_PTR(ret);

	for (i = 0; i < CW201x_PROFILE_SIZE; i++)
		pdata->battery_profile_tbl[i] = val_array[i];

	return pdata;
}

static int cw201x_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct cw201x_chip *chip;
	int ret;
	u8 reg_val;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "fail to allocate memory\n");
		return -ENOMEM;
	}

	chip->client = client;

	if (client->dev.of_node) {
		chip->pdata = cw201x_parse_dt(&client->dev);
		if (IS_ERR(chip->pdata))
			return PTR_ERR(chip->pdata);
	} else {
		chip->pdata = client->dev.platform_data;
		if (!chip->pdata) {
			dev_err(&client->dev, "no platform data\n");
			return -ENODATA;
		}
	}

	i2c_set_clientdata(client, chip);

	chip->regmap = devm_regmap_init_i2c(client, &cw201x_regmap_config);
	if (IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_err(&client->dev, "regmap init failed with err %d\n", ret);
		goto error;
	}

	ret = cw201x_check_battery(chip);
	if (ret < 0) {
		dev_err(&client->dev, "unable to find CW201x device\n");
		ret = -ENODEV;
		goto error;
	}

	ret = cw201x_initialize(chip);
	if (ret)  {
		dev_err(&client->dev,
				"Error: Initializing fuel-gauge\n");
		goto error;
	}

	chip->battery.name = "battery";
	chip->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.properties = cw2015_battery_props;
	chip->battery.num_properties = ARRAY_SIZE(cw2015_battery_props);
	chip->battery.get_property = cw2015_get_property;
	chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
	chip->lasttime_status = POWER_SUPPLY_STATUS_DISCHARGING;
	chip->charge_complete = 0;


	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret < 0) {
		dev_err(&client->dev,
				"power supply register failed, err %d\n", ret);
		goto error;
	}

	cw201x_bgi.tz_name = chip->pdata->tz_name;
	chip->bg_dev = battery_gauge_register(&client->dev,
			&cw201x_bgi, chip);
	if (IS_ERR(chip->bg_dev)) {
		ret = PTR_ERR(chip->bg_dev);
		dev_err(&client->dev,
				"battery gauge register failed, err %d\n", ret);
		goto bg_err;
	}

	INIT_DEFERRABLE_WORK(&chip->work, cw201x_work);
	schedule_delayed_work(&chip->work, 0);

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
				cw201x_irq,
				IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
				chip->battery.name, chip);
		if (!ret) {
			/* Clear the Alert bit if already set */
			ret = cw201x_read(chip,
					CW201x_REG_RRT_ALERT,
					&reg_val, sizeof(reg_val));
			if (ret < 0)
				goto irq_clear_err;

			if (reg_val & CW201x_ALERT_FLAG_BIT) {
				/* Alert flag set, clear it */
				reg_val &= ~CW201x_ALERT_FLAG_BIT;
				ret = regmap_write(chip->regmap,
						CW201x_REG_RRT_ALERT, reg_val);
				if (ret < 0) {
					dev_err(&client->dev,
						"clearing the alert flag failed\n");
					goto irq_clear_err;
				}
			}
		} else {
			dev_err(&client->dev,
					"request IRQ %d fail,err = %d\n",
					client->irq, ret);
			client->irq = 0;
			goto irq_reg_error;
		}
	}
	device_set_wakeup_capable(&client->dev, 1);
	dev_info(&client->dev,
			"cw201x driver probe success\n");
	return 0;

irq_clear_err:
	free_irq(client->irq, chip);
irq_reg_error:
	cancel_delayed_work_sync(&chip->work);
bg_err:
	power_supply_unregister(&chip->battery);
error:
	return ret;
}

static int cw201x_remove(struct i2c_client *client)
{
	struct cw201x_chip *chip = i2c_get_clientdata(client);

	if (client->irq)
		free_irq(client->irq, chip);

	battery_gauge_unregister(chip->bg_dev);
	power_supply_unregister(&chip->battery);
	cancel_delayed_work_sync(&chip->work);
	return 0;
}

static void cw201x_shutdown(struct i2c_client *client)
{
	struct cw201x_chip *chip = i2c_get_clientdata(client);

	if (client->irq)
		disable_irq(client->irq);

	cancel_delayed_work_sync(&chip->work);
}

#ifdef CONFIG_PM_SLEEP
static int cw201x_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cw201x_chip *chip = i2c_get_clientdata(client);

	if (device_may_wakeup(&chip->client->dev))
		enable_irq_wake(chip->client->irq);
	cancel_delayed_work_sync(&chip->work);
	return 0;
}

static int cw201x_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cw201x_chip *chip = i2c_get_clientdata(client);

	schedule_delayed_work(&chip->work, CW201x_WORK_DELAY);
	if (device_may_wakeup(&chip->client->dev))
		disable_irq_wake(chip->client->irq);
	return 0;
}
#endif
static SIMPLE_DEV_PM_OPS(cw201x_pm_ops, cw201x_suspend, cw201x_resume);

static const struct of_device_id cw201x_dt_match[] = {
	{ .compatible = "cw,cw201x" },
	{ },
};
MODULE_DEVICE_TABLE(of, cw201x_dt_match);

static const struct i2c_device_id cw201x_id[] = {
	{ "cw201x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cw201x_id);

static struct i2c_driver cw201x_i2c_driver = {
	.driver = {
		.name   = "cw201x",
		.of_match_table = of_match_ptr(cw201x_dt_match),
		.pm     = &cw201x_pm_ops,
	},
	.probe          = cw201x_probe,
	.remove         = cw201x_remove,
	.id_table       = cw201x_id,
	.shutdown       = cw201x_shutdown,
};

static int __init cw201x_init(void)
{
	return i2c_add_driver(&cw201x_i2c_driver);
}
fs_initcall_sync(cw201x_init);

static void __exit cw201x_exit(void)
{
	i2c_del_driver(&cw201x_i2c_driver);
}
module_exit(cw201x_exit);

MODULE_AUTHOR("Shardar Shariff Md <smohammed@nvidia.com>");
MODULE_DESCRIPTION("CW201x Fuel Gauge");
MODULE_LICENSE("GPL v2");
