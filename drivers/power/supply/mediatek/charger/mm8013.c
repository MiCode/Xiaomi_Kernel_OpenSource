/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <mt-plat/v1/mtk_battery.h>
#include "mtk_battery_internal.h"
#include <linux/time.h>

#define DRIVER_VERSION                      "1.0.0"

#define REG_CONTROL                         0x00
#define REG_TEMPERATURE                     0x06
#define REG_VOLTAGE                         0x08
#define REG_FLAGS                           0x0a
#define REG_CURRENT_AVG                     0x14
#define REG_CYCLECOUNT                      0x2a
#define REG_SOC                             0x2c
#define REG_CHARGEVOLT                      0x30
#define REG_CURRENT                         0x72
#define REG_BLOCKDATAOFFSET                 0x3e
#define REG_BLOCKDATA                       0x40

struct mm8013_chip {
	struct i2c_client *client;
	struct delayed_work battery_update_work;
	struct mutex i2c_rw_lock;
};

struct mm8013_chip *_chip;

static int mm8013_write_reg(struct i2c_client *client, u8 reg, u16 value)
{
	int ret;
	struct mm8013_chip *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->i2c_rw_lock);
	mdelay(4);
	ret = i2c_smbus_write_word_data(client, reg, value);
	if (ret < 0)
		dev_info(&client->dev, "%s: err %d\n", __func__, ret);
	mutex_unlock(&chip->i2c_rw_lock);

	return ret;
}

static int mm8013_read_reg(struct i2c_client *client, u8 reg)
{
	int ret;
	struct mm8013_chip *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->i2c_rw_lock);
	mdelay(4);
	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0)
		dev_info(&client->dev, "%s: err %d\n", __func__, ret);
	mutex_unlock(&chip->i2c_rw_lock);

	return ret;
}

static int mm8013_soc(struct mm8013_chip *chip, int *val)
{
	int soc = mm8013_read_reg(chip->client, REG_SOC);

	if (soc < 0)
		return soc;

	if (soc > 100)
		soc = 100;
	*val = soc;
	return 0;
}

static int mm8013_voltage(struct mm8013_chip *chip, int *val)
{
	int volt = mm8013_read_reg(chip->client, REG_VOLTAGE);

	if (volt < 0)
		return volt;

	*val = volt;
	return 0;
}

static int mm8013_current_avg(struct mm8013_chip *chip, int *val)
{
	int curr = mm8013_read_reg(chip->client, REG_CURRENT_AVG);

	if (curr < 0)
		return curr;

	if (curr > 32767)
		curr -= 65536;

	*val = curr*10;

	return 0;
}
static int mm8013_current(struct mm8013_chip *chip, int *val)
{
	int curr = mm8013_read_reg(chip->client, REG_CURRENT);

	if (curr < 0)
		return curr;

	if (curr > 32767)
		curr -= 65536;

	*val = curr*10;

	return 0;
}

static int mm8013_temperature(struct mm8013_chip *chip, int *val)
{
	int temp = mm8013_read_reg(chip->client, REG_TEMPERATURE);

	if (temp < 0)
		return temp;

	*val = temp - 2731;
	return 0;
}

static int mm8013_cyclecount(struct mm8013_chip *chip, int *val)
{
	int count = mm8013_read_reg(chip->client, REG_CYCLECOUNT);

	if (count < 0)
		return count;

	*val = count;
	return 0;
}

static int mm8013_fgstatus(struct mm8013_chip *chip)
{
	int ret;
	int st;
	int cycle;
	int volt;
	//bool bat_maintain;
	int req = 0;

	//Full Charge bit check
	ret = mm8013_read_reg(chip->client, REG_FLAGS);
	if (ret < 0) {
		dev_info(&chip->client->dev, "[%s] get REG_FLAGS failed!\n", __func__);
		return ret;
	}
	st = ret & 0x0200;
	dev_info(&chip->client->dev, "[%s] get REG_FLAGS<%.4x>!\n", __func__, st);
	if (st != 0x0200)
		return ret;

	//Rtc battery maintain flag check
	//ret = rtc_get_bat_maintain(&bat_maintain);
	//if (ret) {
	//	dev_info(&chip->client->dev, "[%s] get RTC bat maintain flag failed!\n", __func__);
	//	return ret;
	//}
	//dev_info(&chip->client->dev, "[%s] bat maintain flag<%d/%s>!\n", __func__,
	//		bat_maintain, bat_maintain?"enable":"disable");
	//if (!bat_maintain) {
	//	return 0;
	//}
	//CycleCount check
	ret = mm8013_read_reg(chip->client, REG_CYCLECOUNT);
	if (ret < 0) {
		dev_info(&chip->client->dev, "[%s] get REG_CYCLECOUNT failed!\n", __func__);
		return ret;
	}
	cycle = ret;
	dev_info(&chip->client->dev, "[%s] get REG_CYCLECOUNT<%d>!\n", __func__, cycle);

	//ChargeVoltage check
	ret = mm8013_read_reg(chip->client, REG_CHARGEVOLT);
	if (ret < 0) {
		dev_info(&chip->client->dev, "[%s] get REG_CHARGEVOLT failed!\n", __func__);
		return ret;
	}
	volt = ret;
	dev_info(&chip->client->dev, "[%s] get REG_CHARGEVOLT<%d>!\n", __func__, ret);

	if (cycle < 150) {
		if (volt != 4430)
			req = 4430;
	} else if (cycle < 400) {
		if (volt != 4380)
			req = 4380;
	} else {
		if (volt != 4230)
			req = 4230;
	}

	if (req != 0) {
		ret = mm8013_write_reg(chip->client, REG_CHARGEVOLT, req);
		if (ret < 0) {
			dev_info(&chip->client->dev, "[%s] set REG_CHARGEVOLT<%d> failed!\n",
					__func__, ret);
			return ret;
		}
		ret = mm8013_read_reg(chip->client, REG_CHARGEVOLT);
		if (ret < 0) {
			dev_info(&chip->client->dev, "[%s] get REG_CHARGEVOLT<%d> failed!\n",
				__func__, ret);
			return ret;
		}
		dev_info(&chip->client->dev, "[%s] update REG_CHARGEVOLT<req:%d ret:%d>!\n",
				__func__, req, ret);
		if (ret != req)
			return ret;
	}

	return 0;
}

static int mm8013_checkdevice(struct mm8013_chip *chip)
{
	int ret;

	ret = mm8013_write_reg(chip->client, 0x0, 0x0008);
	if (ret < 0)
		return ret;

	ret = mm8013_read_reg(chip->client, 0x0);
	if (ret < 0)
		return ret;

	if (ret == 0x0102) {
		ret = 2;
		/* hardwareinfo_set_prop(HARDWARE_BATTERY_ID, "P98980AA1_SCUD_4V35_7500MAH"); */
	} else if (ret == 0x0101) {
		ret = 1;
		/* hardwareinfo_set_prop(HARDWARE_BATTERY_ID, "P98980AA1_SUNWODA_4V35_7500MAH"); */
	} else {
		ret = 0;
	}

	dev_info(&chip->client->dev, "%s battery id:%d\n", __func__, ret);
	return ret;
}

int mm8013_get_info(enum power_supply_property info_type, int *val)
{
	int ret;

	if (!_chip)
		return -1;

	switch (info_type) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = mm8013_voltage(_chip, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = mm8013_soc(_chip, val);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = mm8013_cyclecount(_chip, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = mm8013_current(_chip, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = mm8013_current_avg(_chip, val);
			break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = mm8013_temperature(_chip, val);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static int force_get_tbat_mm8013(bool update, int *temp)
{
	static int pre_bat_temp = -1000;
	static struct timespec pre_time;
	struct timespec ctime, dtime;
	int bat_temp;
	int ret;

	if (update || pre_bat_temp == -1000) {
		ret = mm8013_get_info(POWER_SUPPLY_PROP_TEMP, &bat_temp);
		if (ret)
			return ret;
		if (pre_bat_temp == -1000) {
			get_monotonic_boottime(&pre_time);
		} else {
			get_monotonic_boottime(&ctime);
			dtime = timespec_sub(ctime, pre_time);

			if (((dtime.tv_sec <= 20) &&
				(abs(pre_bat_temp-bat_temp) >= 50)) || bat_temp >= 580) {
				bm_err("[%s] battery temperature %d, raise from %d in %d sec!\n",
				__func__, bat_temp, pre_bat_temp, dtime.tv_sec);
				WARN_ON_ONCE(1);
			}
		}
		pre_bat_temp = bat_temp;
		*temp = bat_temp;
		pre_time = ctime;

	} else {
		*temp = pre_bat_temp;
	}

	return 0;
}

/* keep 100% after battery full, and hold until battery voltage
 * below 4250mV. To avoid capacity jump, only 1% change is
 * allowed in 1 minute.
 */
bool battery_adjust_capacity(int *capacity)
{
	static bool keep_full;
	static int last_capacity = -1;
	static struct timespec pre_time;
	struct timespec ctime, dtime;
	int avg_value;

	if (battery_main.BAT_STATUS == POWER_SUPPLY_STATUS_FULL
		&& *capacity == 100) {
		keep_full = 1;
	} else if (battery_main.BAT_STATUS == POWER_SUPPLY_STATUS_CHARGING) {
		if (keep_full && battery_main.BAT_batt_vol > 4250)
			*capacity = 100;
		else if (keep_full && battery_main.BAT_batt_vol <= 4250)
			keep_full = 1;
	} else {
		keep_full = 1;
	}

	if (last_capacity == -1) {
		get_monotonic_boottime(&pre_time);
		last_capacity = *capacity;
	} else if (keep_full) {
		get_monotonic_boottime(&pre_time);
		last_capacity = 100;
	} else if (last_capacity != *capacity) {
		get_monotonic_boottime(&ctime);
		dtime = timespec_sub(ctime, pre_time);
		if (battery_main.BAT_STATUS == POWER_SUPPLY_STATUS_FULL && *capacity < 100) {
			if (dtime.tv_sec >= 10) {
				*capacity = last_capacity+(last_capacity < 100?1:0);
				pre_time = ctime;
				last_capacity = *capacity;
			} else {
				*capacity = last_capacity;
			}
		} else {
			if (dtime.tv_sec >= 60) {
				mm8013_current_avg(_chip, &avg_value);
				if (avg_value < 0 && last_capacity > *capacity) {
					*capacity = last_capacity - 1;
				} else if (battery_main.BAT_STATUS == POWER_SUPPLY_STATUS_CHARGING
							&& *capacity > last_capacity) {
					*capacity = last_capacity + 1;
				} else {
					*capacity = last_capacity;
				}
				pre_time = ctime;
				last_capacity = *capacity;
			} else {
				*capacity = last_capacity;
			}
		}
	}

	return false;
}


static void mm8013_battery_update_work(struct work_struct *work)
{
	struct mm8013_chip *chip = container_of(to_delayed_work(work),
			struct mm8013_chip, battery_update_work);
	int battery_temp, cycle_count, capacity;

	bm_notice("[%s] enter\n", __func__);

	force_get_tbat_mm8013(1, &battery_temp);
	gm.fixed_bat_tmp = (battery_temp / 10);
	battery_main.BAT_batt_temp = gm.fixed_bat_tmp;

	if (!mm8013_get_info(POWER_SUPPLY_PROP_CYCLE_COUNT, &cycle_count))
		gm.bat_cycle = cycle_count;

	if (!mm8013_get_info(POWER_SUPPLY_PROP_CAPACITY, &capacity)) {
		battery_adjust_capacity(&capacity);
		battery_main.BAT_CAPACITY = capacity;
		gm.ui_soc = capacity;
		gm.fixed_uisoc = capacity;
		gm.soc = capacity;
		if (capacity >= 100)
			gm.is_force_full = 1;
	}

	bm_notice("ftmp(%d),btmp(%d),cycle(%d),ui_soc(%d),fixed_uisoc(%d),soc(%d)\n",
	gm.fixed_bat_tmp, battery_main.BAT_batt_temp, gm.bat_cycle,
	gm.ui_soc, gm.fixed_uisoc, gm.soc);

	battery_update(&battery_main);
	mm8013_fgstatus(chip);
	schedule_delayed_work(&chip->battery_update_work, msecs_to_jiffies(5*1000));
}

static int mm8013_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mm8013_chip *chip;
	int ret;

	dev_info(&client->dev, "[%s] enter, start\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_info(&client->dev, "[%s] not support i2c word ops!\n", __func__);
		return -EIO;
	}
	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	mutex_init(&chip->i2c_rw_lock);
	chip->client = client;

	i2c_set_clientdata(client, chip);

	ret = mm8013_checkdevice(chip);
	if (ret < 0) {
		dev_info(&client->dev, "[%s] failed to access\n", __func__);
		if (ret == -EREMOTEIO)
			ret = -EPROBE_DEFER;
		return ret;
	}
	switch (ret) {
	case 1:
		dev_info(&client->dev, "%s battery id:1\n", __func__);
		break;
	case 2:
		dev_info(&client->dev, "%s battery id:2\n", __func__);
		break;
	default:
		dev_info(&client->dev, "%s battery id:0\n", __func__);
		break;
	}

	_chip = chip;
	INIT_DELAYED_WORK(&chip->battery_update_work, mm8013_battery_update_work);
	schedule_delayed_work(&chip->battery_update_work, msecs_to_jiffies(5*1000));

	dev_info(&client->dev, "%s ok\n", __func__);
	return 0;
}

static int mm8013_remove(struct i2c_client *client)
{
	struct mm8013_chip *chip = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);
	kfree(chip);
	return 0;
}

static void mm8013_shutdown(struct i2c_client *client)
{
	int ret;

	ret = mm8013_write_reg(client, REG_CONTROL, 0x0013);
	if (ret < 0)
		return;

	dev_info(&client->dev, "%s shutdown\n", __func__, ret);
}
static const struct i2c_device_id mm8013_id[] = {
	{ "mm8013", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, mm8013_id);

static const struct of_device_id mm8013_match_table[] = {
	{ .compatible = "mediatek,mm8013",},
	{ },
};

static struct i2c_driver mm8013_i2c_driver = {
	.driver    = {
		.name  = "mm8013",
		.of_match_table = mm8013_match_table,
	},
	.probe     = mm8013_probe,
	.remove    = mm8013_remove,
	.id_table  = mm8013_id,
	.shutdown = mm8013_shutdown,
};
static inline int mm8013_i2c_init(void)
{
	return i2c_add_driver(&mm8013_i2c_driver);
}
static int __init mm8013_init(void)
{
	return mm8013_i2c_init();
}
module_init(mm8013_init);

static void __exit mm8013_exit(void)
{
	i2c_del_driver(&mm8013_i2c_driver);
}
module_exit(mm8013_exit);
MODULE_AUTHOR("qilong.zhang@archmind.com");
MODULE_DESCRIPTION("I2C bus driver for mm8013X gauge");
MODULE_LICENSE("GPL v2");
