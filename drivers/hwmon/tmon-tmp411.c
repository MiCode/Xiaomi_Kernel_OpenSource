/*
 * Temperature Monitor Driver
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * drivers/hwmon/tmon-tmp411.c
 *
 */

/* Note: Copied temperature conversion code from TMP411 driver */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/sysfs.h>
#include <linux/platform_data/tmon_tmp411.h>

#define STATUS_REG			0x02
#define CONFIG_REG_READ			0x03
#define CONFIG_REG_WRITE		0x09
#define CONVERSION_REG_READ		0x04
#define CONVERSION_REG_WRITE		0x0a
#define TMON_OFFSET			0x40
#define TMON_STANDBY_MASK		0x40

#define MSB_LTEMP_REG			0x00
#define LSB_LTEMP_REG			0x15
#define MSB_RTEMP_REG			0x01
#define LSB_RTEMP_REG			0x10
#define CONFIG_RANGE			0x04

#define DEFAULT_TMON_POLLING_TIME	2000	/* Time in ms */
#define DEFAULT_TMON_DELTA_TEMP		4000	/* Temp. change to execute
						platform callback */
#define TMON_ERR			INT_MAX
#define TMON_NOCHANGE			(INT_MAX - 1)

static const u8 TMP411_TEMP_LOW_LIMIT_MSB_READ[2]	= { 0x06, 0x08 };
static const u8 TMP411_TEMP_LOW_LIMIT_MSB_WRITE[2]	= { 0x0C, 0x0E };
static const u8 TMP411_TEMP_LOW_LIMIT_LSB[2]		= { 0x17, 0x14 };
static const u8 TMP411_TEMP_HIGH_LIMIT_MSB_READ[2]	= { 0x05, 0x07 };
static const u8 TMP411_TEMP_HIGH_LIMIT_MSB_WRITE[2]	= { 0x0B, 0x0D };
static const u8 TMP411_TEMP_HIGH_LIMIT_LSB[2]		= { 0x16, 0x13 };
/* These are called the THERM limit / hysteresis / mask in the datasheet */
static const u8 TMP411_TEMP_CRIT_LIMIT[2]		= { 0x20, 0x19 };

static u16 temp_low_limit[2];
static u16 temp_high_limit[2];
static u8 temp_crit_limit[2];
static u8 conv_rate;

struct tmon_info {
	int mode;
	struct i2c_client *client;
	struct delayed_work tmon_work;
	struct tmon_plat_data *pdata;
};

#define device_attr(type)					\
static struct device_attribute dev_attr_##type = {		\
	.attr = {.name = __stringify(type),			\
	.mode = S_IWUSR | S_IRUGO },				\
	.show   = show_##type,					\
}

static int tmon_read(struct i2c_client *client, u8 reg, u8 *value)
{
	int tmp;

	tmp = i2c_smbus_read_byte_data(client, reg);
	if (tmp < 0)
		return -EINVAL;

	*value = tmp;

	return 0;
}

static int tmon_to_temp(u16 reg, u8 config)
{
	int temp = reg;

	if (config & CONFIG_RANGE)
		temp -= 64 * 256;

	return (temp * 625 + 80) / 160;
}

static int tmon_read_remote_temp(struct i2c_client *client,
					     int *ptemp)
{
	u8 config;
	u8 temp;
	int err;
	int temperature = 0;
	struct tmon_info *data = i2c_get_clientdata(client);

	err = tmon_read(client, CONFIG_REG_READ, &config);
	if (err)
		return err;
	err = tmon_read(client, MSB_RTEMP_REG, &temp);
	if (err)
		return err;
	temperature = temp << 8;
	err = tmon_read(client, LSB_RTEMP_REG, &temp);
	if (err)
		return err;
	temperature |= temp;

	if (data->pdata) {
		*ptemp = tmon_to_temp(temperature, config) +
			data->pdata->remote_offset;
	} else {
		return  -EINVAL;
	}

	return 0;
}

static int tmon_read_local_temp(struct i2c_client *client,
					    int *ptemp)
{
	u8 config;
	u8 temp;
	int err;
	int temperature = 0;
	err = tmon_read(client, CONFIG_REG_READ, &config);
	if (err)
		return err;
	err = tmon_read(client, MSB_LTEMP_REG, &temp);
	if (err)
		return err;
	temperature = temp << 8;
	err = tmon_read(client, LSB_LTEMP_REG, &temp);
	if (err)
		return err;
	temperature |= temp;

	*ptemp = tmon_to_temp(temperature, config);

	return 0;
}

static int tmon_check_local_temp(struct i2c_client *client,
					     u32 delta_temp)
{
	static int last_temp;
	int err;
	int curr_temp = 0;

	err = tmon_read_local_temp(client, &curr_temp);
	if (err)
		return TMON_ERR;

	if (abs(curr_temp - last_temp) >= delta_temp) {
		last_temp = curr_temp;
		return curr_temp;
	}

	return TMON_NOCHANGE;
}

static int tmon_check_remote_temp(struct i2c_client *client,
						 u32 delta_temp)
{
	static int last_temp;
	int err;
	int curr_temp = 0;
	err = tmon_read_remote_temp(client, &curr_temp);
	if (err)
		return TMON_ERR;

	if (abs(curr_temp - last_temp) >= delta_temp) {
		last_temp = curr_temp;
		return curr_temp;
	}

	return TMON_NOCHANGE;
}

static ssize_t show_remote_temp(struct device *dev,
				struct device_attribute *devattr, char *buf)
{
	int temperature = 0;

	tmon_read_remote_temp(to_i2c_client(dev), &temperature);

	return sprintf(buf, "%d\n", temperature);
}

static ssize_t show_local_temp(struct device *dev,
			       struct device_attribute *devattr, char *buf)
{
	int temperature = 0;

	tmon_read_local_temp(to_i2c_client(dev), &temperature);

	return sprintf(buf, "%d\n", temperature);
}

device_attr(remote_temp);
device_attr(local_temp);

static void tmon_update(struct work_struct *work)
{
	int ret;
	struct tmon_info *tmon_data =
	    container_of(to_delayed_work(work),
			 struct tmon_info,
			 tmon_work);
	struct tmon_plat_data *pdata = tmon_data->pdata;

	if (pdata->delta_time <= 0)
		pdata->delta_time = DEFAULT_TMON_POLLING_TIME;

	if (pdata->delta_temp <= 0)
		pdata->delta_temp = DEFAULT_TMON_DELTA_TEMP;

	ret =
	    tmon_check_local_temp(tmon_data->client,
					      pdata->delta_temp);

	if (ret != TMON_ERR && ret != TMON_NOCHANGE &&
		pdata->ltemp_dependent_reg_update)
		pdata->ltemp_dependent_reg_update(ret);

	ret = tmon_check_remote_temp(tmon_data->client,
						pdata->delta_temp);

	if (ret != TMON_ERR && ret != TMON_NOCHANGE &&
		pdata->utmip_temp_dep_update)
		pdata->utmip_temp_dep_update(ret, pdata->utmip_temp_bound);

	schedule_delayed_work(&tmon_data->tmon_work,
			      msecs_to_jiffies(pdata->delta_time));
}

static int __devinit tmon_tmp411_probe(struct i2c_client *client,
					    const struct i2c_device_id *id)
{
	int err;
	struct tmon_plat_data *tmon_pdata =
	    client->dev.platform_data;
	struct tmon_info *data;

	if (tmon_pdata == NULL) {
		dev_err(&client->dev, "no platform data\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter,
					I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "insufficient functionality!\n");
		return -ENODEV;
	}

	data = kzalloc(sizeof(struct tmon_info), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;
	i2c_set_clientdata(client, data);
	data->pdata = tmon_pdata;

	err = device_create_file(&client->dev, &dev_attr_local_temp);
	if (err) {
		kfree(data);
		return err;
	}

	err = device_create_file(&client->dev, &dev_attr_remote_temp);
	if (err) {
		kfree(data);
		device_remove_file(&client->dev, &dev_attr_local_temp);
		return err;
	}

	INIT_DELAYED_WORK(&data->tmon_work, tmon_update);

	schedule_delayed_work(&data->tmon_work,
			      msecs_to_jiffies(data->pdata->delta_time));

	dev_info(&client->dev, "Temperature Monitor enabled\n");
	return 0;
}

static int __devexit tmon_tmp411_remove(struct i2c_client *client)
{
	struct tmon_info *data = i2c_get_clientdata(client);

	cancel_delayed_work(&data->tmon_work);
	device_remove_file(&client->dev, &dev_attr_remote_temp);
	device_remove_file(&client->dev, &dev_attr_local_temp);

	kfree(data);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tmon_tmp411_suspend(struct device *dev)
{
	int i;
	struct i2c_client *client = to_i2c_client(dev);
	struct tmon_info *data = i2c_get_clientdata(client);

	/* save temperature limits for restore during resume */
	for (i = 0; i < 2; i++) {
		/*
		 * High byte must be read first immediately followed
		 * by the low byte
		 */
		temp_low_limit[i] = i2c_smbus_read_byte_data(client,
			TMP411_TEMP_LOW_LIMIT_MSB_READ[i]) << 8;

		temp_low_limit[i] |= i2c_smbus_read_byte_data(client,
			TMP411_TEMP_LOW_LIMIT_LSB[i]);


		temp_high_limit[i] = i2c_smbus_read_byte_data(client,
			TMP411_TEMP_HIGH_LIMIT_MSB_READ[i]) << 8;

		temp_high_limit[i] |= i2c_smbus_read_byte_data(client,
			TMP411_TEMP_HIGH_LIMIT_LSB[i]);

		temp_crit_limit[i] = i2c_smbus_read_byte_data(client,
			TMP411_TEMP_CRIT_LIMIT[i]);
	}
	conv_rate = i2c_smbus_read_byte_data(client, CONVERSION_REG_READ);

	cancel_delayed_work_sync(&data->tmon_work);
	return 0;
}

static int tmon_tmp411_resume(struct device *dev)
{
	int i;
	struct i2c_client *client = to_i2c_client(dev);
	struct tmon_info *data = i2c_get_clientdata(client);

	/*  Restore temperature limits */
	for (i = 0; i < 2; i++) {
		i2c_smbus_write_byte_data(client,
			TMP411_TEMP_HIGH_LIMIT_MSB_WRITE[i],
			temp_high_limit[i] >> 8);

		i2c_smbus_write_byte_data(client,
			TMP411_TEMP_HIGH_LIMIT_LSB[i],
			temp_high_limit[i] & 0xFF);

		i2c_smbus_write_byte_data(client,
			 TMP411_TEMP_LOW_LIMIT_MSB_WRITE[i],
			 temp_low_limit[i] >> 8);

		i2c_smbus_write_byte_data(client,
			TMP411_TEMP_LOW_LIMIT_LSB[i],
			temp_low_limit[i] & 0xFF);

		i2c_smbus_write_byte_data(client,
			TMP411_TEMP_CRIT_LIMIT[i],
			temp_crit_limit[i]);
	}
	i2c_smbus_write_byte_data(client, CONVERSION_REG_WRITE, conv_rate);

	schedule_delayed_work(&data->tmon_work,
				msecs_to_jiffies(data->pdata->delta_time));
	return 0;
}
#endif

static const struct dev_pm_ops tegra_tmp411_dev_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend = tmon_tmp411_suspend,
	.resume = tmon_tmp411_resume,
#endif
};

/* tmon-tmp411 driver struct */
static const struct i2c_device_id tmon_tmp411_id[] = {
	{"tmon-tmp411-sensor", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, tmon_tmp411_id);

static struct i2c_driver tmon_tmp411_driver = {
	.driver = {
			.name = "tmon-tmp411-sensor",
#ifdef CONFIG_PM_SLEEP
			.pm = &tegra_tmp411_dev_pm_ops,
#endif
		   },
	.probe = tmon_tmp411_probe,
	.remove = __devexit_p(tmon_tmp411_remove),
	.id_table = tmon_tmp411_id,
};

static int __init tmon_tmp411_module_init(void)
{
	return i2c_add_driver(&tmon_tmp411_driver);
}

static void __exit tmon_tmp411_module_exit(void)
{
	i2c_del_driver(&tmon_tmp411_driver);
}

module_init(tmon_tmp411_module_init);
module_exit(tmon_tmp411_module_exit);

MODULE_AUTHOR("Manoj Chourasia <mchourasia@nvidia.com>");
MODULE_DESCRIPTION("Temperature Monitor module");
MODULE_LICENSE("GPL");
