/*
 * drivers/thermal/tmp006.c
 *
 * Driver for TMP006, Skin temperature monitoring device
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/thermal.h>
#include <mach/thermal.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>

/* Register Address */

#define SENSOR_VOLTAGE	0X00
#define AMBIENT_TEMP	0X01
#define CONFIGURATION	0X02
#define MANUFACTURE_ID	0XFE
#define DEVICE_ID	0XFF

#define MAX_STR_PRINT	50
#define TMP006_POLL_INT 9000 /* 9 seconds */

#define TMP006_TRANS_CORRECT
#define TMP006_FILTER_OUTPUT

struct tmp006_data {
	struct i2c_client *client;
	struct regmap *regmap;
	struct thermal_zone_device *thz_dev;
};

static const struct regmap_range tmp006_readable_ranges[] = {
	regmap_reg_range(SENSOR_VOLTAGE, CONFIGURATION),
	regmap_reg_range(MANUFACTURE_ID, DEVICE_ID),
};

static const struct regmap_access_table tmp006_readable_table = {
	.yes_ranges = tmp006_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(tmp006_readable_ranges),
};

struct regmap_range tmp006_writable_ranges[] = {
	regmap_reg_range(CONFIGURATION, CONFIGURATION),
};

struct regmap_access_table tmp006_writable_table = {
	.yes_ranges = tmp006_writable_ranges,
	.n_yes_ranges = ARRAY_SIZE(tmp006_writable_ranges),
};

static const struct regmap_range tmp006_volatile_ranges[] = {
	regmap_reg_range(SENSOR_VOLTAGE, CONFIGURATION),
};

static const struct regmap_access_table tmp006_volatile_table = {
	.no_ranges = tmp006_volatile_ranges,
	.n_no_ranges = ARRAY_SIZE(tmp006_volatile_ranges),
};

static const struct regmap_config tmp006_regmap_config = {
	.reg_bits		= 8,
	.val_bits		= 16,
	.max_register		= DEVICE_ID,
	.rd_table		= &tmp006_readable_table,
	.wr_table		= &tmp006_writable_table,
	.volatile_table		= &tmp006_volatile_table,
};

static u16 tmp006_write_word_data(const struct i2c_client *client, u8 command,
					u16 write_data)
{
	struct tmp006_data *data = i2c_get_clientdata(client);

	return regmap_write(data->regmap, command, write_data);
}

static u16 tmp006_read_word_data(const struct i2c_client *client, u8 command)
{
	struct tmp006_data *data = i2c_get_clientdata(client);
	u32 read_word = 0;
	int ret;

	ret = regmap_read(data->regmap, command, &read_word);
	return (u16) read_word;
}

/* Manufacture id for tmp006 is 0x5449*/
static u16 tmp006_get_manufacture_id(const struct i2c_client *client)
{
	return tmp006_read_word_data(client, MANUFACTURE_ID);
}

/* Device id for tmp006 is 0x0067*/
static u16 tmp006_get_device_id(const struct i2c_client *client)
{
	return tmp006_read_word_data(client, DEVICE_ID);
}

static int tmp006_bind(struct thermal_zone_device *thermal,
			struct thermal_cooling_device *cdev)
{
	return 0;
}

static int tmp006_unbind(struct thermal_zone_device *thermal,
			  struct thermal_cooling_device *cdev)
{
	return 0;
}

static int tmp006_get_trip_temp(struct thermal_zone_device *thz,
				int trip, unsigned long *temp)
{
	*temp = 9000;
	return 0;
}

static int tmp006_get_trip_type(struct thermal_zone_device *thz,
				int trip, enum thermal_trip_type *type)
{
	return THERMAL_TRIP_PASSIVE;
}

/*
*  This function calculates the power 4 of a 16 bit number
*  and returns only the 32 bit msbs.
*
*  It first calculates n squared.
*  It then splits the squared value to two 16 bit numbers.
*  The output is approximated as following
*  n2 = n * n
*  n2 = a + b
*  n4 = a2 + 2ab + b2
*/
static s32 tmp006_Power4(s32 n)
{

	s32 lsb;
	s32 msb;
	s32 n2;

	n2 = n * n;
	lsb = n2 & 0x00003FFF;
	msb = n2 >> 14;
	return (msb * msb) + (((msb * lsb) + ((lsb * lsb) >> 15)) >> 13);
}

/*
*  Calculate 4th order square root.
*  This function calculates its output using binary search.
*  Takes 14 iterations.
*/
static s32 tmp006_Sqrt4(s32 n)
{
	s32 val = 0x00000040;
	s32 val2;
	s32 val4;
	/* Binary search to find the sqrt
	 (14 most significant bits with 1 LSB = 0.03125C)*/

	/* Bit 14, The first branch in the binary tree can be predefined */
	val4 = 16777216;
	if (val4 > n)
		val = 0x00;
	/* Bit 13 */
	val = val | 0x0020;	/* Enable the next bit */
	val2 = val * val;
	val4 = val2 * val2;	/* Calculate the power 4 */
	if (val4 > n)
		val = val & 0xFFDF;
	/* Bit 12 */
	val = val | 0x0010;
	val2 = val * val;
	val4 = val2 * val2;
	if (val4 > n)
		val = val & 0xFFEF;
	/* Bit 11 */
	val = val | 0x0008;
	val2 = val * val;
	val4 = val2 * val2;
	if (val4 > n)
		val = val & 0xFFF7;
	/* Bit 10 */
	val = val | 0x0004;
	val2 = val * val;
	val4 = val2 * val2;
	if (val4 > n)
		val = val & 0xFFFB;
	/* Bit 9 */
	val = val | 0x0002;
	val2 = val * val;
	val4 = val2 * val2;
	if (val4 > n)
		val = val & 0xFFFD;
	/* Bit 8 */
	val = val | 0x0001;
	val2 = val * val;
	val4 = val2 * val2;
	if (val4 > n)
		val = val & 0xFFFE;
	/* Shift the 7 msb calculated so far to the left */
	val = val << 7;
	/* Bit 7 */
	val = val | 0x0040;
	/*
	 After the first 7 bits, the tmp006_Power4 function
	 is used to avoid running out of bits.
	*/
	val4 = tmp006_Power4(val);
	if (val4 > n)
		val = val & 0xFFBF;
	/* Bit 6 */
	val = val | 0x0020;
	val4 = tmp006_Power4(val);
	if (val4 > n)
		val = val & 0xFFDF;
	/* Bit 5 */
	val = val | 0x0010;
	val4 = tmp006_Power4(val);
	if (val4 > n)
		val = val & 0xFFEF;
	/* Bit 4 */
	val = val | 0x0008;
	val4 = tmp006_Power4(val);
	if (val4 > n)
		val = val & 0xFFF7;
	/* Bit 3 */
	val = val | 0x0004;
	val4 = tmp006_Power4(val);
	if (val4 > n)
		val = val & 0xFFFB;
	/* Bit 2 */
	val = val | 0x0002;
	val4 = tmp006_Power4(val);
	if (val4 > n)
		val = val & 0xFFFD;
	/* Bit 1 */
	val = val | 0x0001;
	val4 = tmp006_Power4(val);
	if (val4 > n)
		val = val & 0xFFFE;

	return val;
}


/*
*   Returns a two's complement binary number, where 1 LSB = 0.03125°C.
*   We must first perform a right shift of 2 bits on the result before
*   multiplying by 0.03125°C.
*/
static s16 tmp006_Calculate(s16 vout, s16 tdie)
{
	/* Constants */
	/* (0.15625e-6 / s0) (Must be a positive number) */
	s32 s0_inv = 2297794;
	/* a1 * 2^18 (limited to +/- 0.12) */
	s32 a1 = 459;
	/* a2 * 2^24 (limited to +/- 0.0019) */
	s32 a2 = -336;
	/* b0 / 0.15625e-6 */
	s32 b0 = -192;
	/* (b1 / 0.15625e-6) * 2^10 (limited to +/- 4.9e-6) */
	s32 b1 = -3932;
	/* (b2 / 0.15625e-6) * 2^18 (limited to +/- 0.019e-6) */
	s32 b2 = 419;

	/* Filter constants */
	s32 d1 = 3277;	/* d1*2^14 */
	s32 d2 = 9830;	/* = d2*2^14 */
	s32 e1 = 1638;	/* = e1*2^14 */
	s32 e2 = 13108;	/* = e2*2^14 */

	/* Transient correction constant
	   (f1 / 0.15625e-6)*2^7*(2*d1)/(1-d1)
	*/
	s32 f1 = 131072;

	/* Variables */
	s32 s;
	s32 tdie_32;	/* 32 bit die temperature reading */
	s32 vout_32;	/* 32 bit die temperature reading */
	s32 dtref;	/* difference to reference temperature */
	s32 dtref2;	/* difference to reference temperature squared */
	s32 fvout;	/* Offset and transient corrected output */
	s32 lsb;	/* Used to capture lower bits of a number */
	s32 msb;	/* Used to capture upper bits of a number */
	s32 tobj;	/* Output */
	s32 tdie_slope = 0;	/* Assume initial slope to be zero */

	static s32 tdie_filtered;
	static s32 tdie_previous;
	static s32 tobj_filtered;
	static s32 tobj_previous;
	static s32 first_run = 1;

	vout_32 = (s32)vout;

	/* Make sure negative numbers are handled correctly */
	if (vout_32 >= 32768)
		vout_32 = vout_32 - 65536;

	tdie_32 = (s32)tdie >> 2;	/* Get the 14 msb bits */
	dtref = tdie_32 - 800;	/* 800 = 25 / 0.03125 (Calculate Tdie - Tref) */
	dtref2 = dtref * dtref;	/* (Tdie - Tref) ^ 2 */

	/* 8741 = 273.15 / 0.03125 (Convert to Kelvin) */
	tdie_32 = tdie_32 + 8741;

#ifdef TMP006_TRANS_CORRECT
	/* Transient correction */
	if (first_run) {
		tdie_previous = tdie_32;
		tdie_filtered = tdie_32 << 16;
#ifndef TMP006_FILTER_OUTPUT
		first_run = 0;
#endif
	}

	lsb = tdie_filtered & 0x00003FFF;	/* Get 14 lsb bits */
	msb = tdie_filtered >> 14;	/* Get 16 msb bits */

	tdie_filtered = ((d1 * ((tdie_32 + tdie_previous) << 2))
			+ (d2 * msb) + ((d2 * lsb) >> 14));
	tdie_slope = ((tdie_32 << 16) - tdie_filtered) >> 9;
	tdie_previous = tdie_32;

	lsb = (dtref2 & 0x00007FFF);	/* Get the lsb bits */
	msb = dtref2 >> 15;	/* Get the msb bits */
	fvout = ((vout_32 - b0) << 7) - ((((b1 * dtref) >> 2) + ((msb * b2))
		+ ((lsb * b2) >> 15) - ((tdie_slope * f1) >> 6)) >> 6);
#else
	/* No transient correction */
	lsb = (dtref2 & 0x00007FFF);	/* Get the lsb bits */
	msb = dtref2 >> 15;	/* Get the msb bits */
	fvout = ((vout_32 - b0) << 7) - ((((b1 * dtref) >> 2) +
		((msb * b2)) + ((lsb * b2) >> 15)) >> 6);
#endif
	/* Core equation */
	s = (1 << 15) + ((((dtref * a1) >> 4) + (msb * a2) +
		((lsb * a2) >> 15)) >> 4);
	msb = s0_inv / s;	/* Divide the numbers */
	lsb = ((s0_inv % s) << 8) / s;	/* Get the remainder */
	fvout = (fvout * msb) + ((fvout * lsb) >> 8);
	tobj = tmp006_Sqrt4(tmp006_Power4(tdie_32) + (fvout)) - 8741;

#ifdef TMP006_FILTER_OUTPUT
	/* Output filtering */
	if (first_run) {
		tobj_previous = tobj;
		tobj_filtered = tobj << 14;
		first_run = 0;	/* Not the first run anymore */
	}

	lsb = tobj_filtered & 0x00003FFF;
	msb = tobj_filtered >> 14;

	tobj_filtered = (e1 * (tobj + tobj_previous)) + (e2 * msb) +
		((e2 * lsb) >> 14);
	tobj_previous = tobj;

	return tobj_filtered >> 12;
#else
	return tobj << 2;
#endif

}

static int tmp006_get_temp(struct thermal_zone_device *thermal,
				unsigned long *t)
{
	struct tmp006_data *data = thermal->devdata;
	struct i2c_client *client = data->client;
	u16 cv = 0x0;
	u16 sv = 0x0;
	u16 at = 0x0;
	s32 ret = 0x0;
	int tk = 0;

	cv = tmp006_read_word_data(client, CONFIGURATION);
	ret  = tmp006_write_word_data(client, CONFIGURATION, 0x7500);
	do {
		cv = tmp006_read_word_data(client, CONFIGURATION);
	} while (!(cv & 0x0080));
	sv = tmp006_read_word_data(client, SENSOR_VOLTAGE);
	at = tmp006_read_word_data(client, AMBIENT_TEMP);
	tk = tmp006_Calculate(sv, at);
	*t = tk;
	dev_info(&client->dev,
		"Sensor voltage = %x ambient temp = %x Target object temp=%x\n",
		sv, at, tk);
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops tmp006_dev_ops = {
	.bind = tmp006_bind,
	.unbind = tmp006_unbind,
	.get_temp = tmp006_get_temp,
	.get_trip_type = tmp006_get_trip_type,
	.get_trip_temp = tmp006_get_trip_temp,
};

static int tmp006_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = 0;
	struct tmp006_data *data;

	data = devm_kzalloc(&client->dev, sizeof(struct tmp006_data),
				GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->client = client;
	data->regmap = devm_regmap_init_i2c(client, &tmp006_regmap_config);
	if (IS_ERR(data->regmap)) {
		err = PTR_ERR(data->regmap);
		dev_err(&client->dev,
			"%s(): regmap allocation failed with err %d\n",
			__func__, err);
		return err;
	}
	i2c_set_clientdata(client, data);
	data->thz_dev = thermal_zone_device_register("tmp006", 1, 0, data,
			&tmp006_dev_ops, NULL, 0, TMP006_POLL_INT);
	if (IS_ERR(data->thz_dev)) {
		err = PTR_ERR(data->thz_dev);
		dev_err(&client->dev,
		"\n thermal_zone_device_register error err=%d ", err);
		return err;
	}
	return 0;
}

static int tmp006_remove(struct i2c_client *client)
{
	struct tmp006_data *data = i2c_get_clientdata(client);

	thermal_zone_device_unregister(data->thz_dev);
	return 0;
}

static const struct i2c_device_id tmp006_id[] = {
	{"tmp006", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, tmp006_id);

static struct i2c_driver tmp006_driver = {
	.driver = {
		.name = "tmp006",
		.owner = THIS_MODULE,
	},
	.probe = tmp006_probe,
	.remove = tmp006_remove,
	.id_table = tmp006_id,
};

module_i2c_driver(tmp006_driver);

MODULE_DESCRIPTION("Skin Temperature Sensor driver for tmp006");
MODULE_AUTHOR("Preetham Chandru Ramchandra");
MODULE_LICENSE("GPL v2");
