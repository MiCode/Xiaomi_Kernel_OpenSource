/*
 * ad5816.c - ad5816 focuser driver
 *
 * Copyright (c) 2012-2014, NVIDIA Corporation. All Rights Reserved.
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

/* Implementation
 * --------------
 * The board level details about the device need to be provided in the board
 * file with the <device>_platform_data structure.
 * Standard among NVC kernel drivers in this structure is:
 * .cfg = Use the NVC_CFG_ defines that are in nvc.h.
 *  Descriptions of the configuration options are with the defines.
 *      This value is typically 0.
 * .num = The number of the instance of the device.  This should start at 1 and
 *      and increment for each device on the board.  This number will be
 *      appended to the MISC driver name, Example: /dev/focuser.1
 *      If not used or 0, then nothing is appended to the name.
 * .sync = If there is a need to synchronize two devices, then this value is
 *       the number of the device instance (.num above) this device is to
 *       sync to.  For example:
 *       Device 1 platform entries =
 *       .num = 1,
 *       .sync = 2,
 *       Device 2 platfrom entries =
 *       .num = 2,
 *       .sync = 1,
 *       The above example sync's device 1 and 2.
 *       To disable sync, set .sync = 0.  Note that the .num = 0 device is not
 *       allowed to be synced to.
 *       This is typically used for stereo applications.
 * .dev_name = The MISC driver name the device registers as.  If not used,
 *       then the part number of the device is used for the driver name.
 *       If using the NVC user driver then use the name found in this
 *       driver under _default_pdata.
 * .gpio_count = The ARRAY_SIZE of the nvc_gpio_pdata table.
 * .gpio = A pointer to the nvc_gpio_pdata structure's platform GPIO data.
 *       The GPIO mechanism works by cross referencing the .gpio_type key
 *       among the nvc_gpio_pdata GPIO data and the driver's nvc_gpio_init
 *       GPIO data to build a GPIO table the driver can use.  The GPIO's
 *       defined in the device header file's _gpio_type enum are the
 *       gpio_type keys for the nvc_gpio_pdata and nvc_gpio_init structures.
 *       These need to be present in the board file's nvc_gpio_pdata
 *       structure for the GPIO's that are used.
 *       The driver's GPIO logic uses assert/deassert throughout until the
 *       low level _gpio_wr/rd calls where the .assert_high is used to
 *       convert the value to the correct signal level.
 *       See the GPIO notes in nvc.h for additional information.
 *
 * The following is specific to NVC kernel focus drivers:
 * .nvc = Pointer to the nvc_focus_nvc structure.  This structure needs to
 *      be defined and populated if overriding the driver defaults.
 * .cap = Pointer to the nvc_focus_cap structure.  This structure needs to
 *      be defined and populated if overriding the driver defaults.
 *
 * The following is specific to this NVC kernel focus driver:
 * .info = Pointer to the ad5816_pdata_info structure.  This structure does
 *       not need to be defined and populated unless overriding ROM data.
 *
 * Power Requirements:
 * The device's header file defines the voltage regulators needed with the
 * enumeration <device>_vreg.  The order these are enumerated is the order
 * the regulators will be enabled when powering on the device.  When the
 * device is powered off the regulators are disabled in descending order.
 * The <device>_vregs table in this driver uses the nvc_regulator_init
 * structure to define the regulator ID strings that go with the regulators
 * defined with <device>_vreg.  These regulator ID strings (or supply names)
 * will be used in the regulator_get function in the _vreg_init function.
 * The board power file and <device>_vregs regulator ID strings must match.
 */

#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <media/ad5816.h>
#include <linux/of.h>
#include <linux/of_device.h>

#define AD5816_ID			0x04
#define AD5816_FOCAL_LENGTH_FLOAT	(4.570f)
#define AD5816_FNUMBER_FLOAT		(2.8f)
#define AD5816_FOCAL_LENGTH			(0x40923D71) /* 4.570f */
#define AD5816_FNUMBER				(0x40333333) /* 2.8f */
#define AD5816_SLEW_RATE		1
#define AD5816_ACTUATOR_RANGE		1023
#define AD5816_SETTLETIME		20
#define AD5816_FOCUS_MACRO		536
#define AD5816_FOCUS_INFINITY		80
#define AD5816_POS_LOW_DEFAULT		0
#define AD5816_POS_HIGH_DEFAULT		1023
#define AD5816_POS_CLAMP		0x03ff
/* Need to decide exact value of VCM_THRESHOLD and its use */
/* define AD5816_VCM_THRESHOLD	20 */

/* Registers values */
#define SCL_LOW_REG_VAL			0xB6
#define CONTROL_REG_VAL			0x0A
#define MODE_REG_VAL			0x42
#define VCM_FREQ_REG_VAL		0x54

struct ad5816_info {
	struct i2c_client *i2c_client;
	struct ad5816_platform_data *pdata;
	struct miscdevice miscdev;
	struct list_head list;
	struct ad5816_power_rail power;
	struct ad5816_info *s_info;
	struct nvc_focus_nvc nvc;
	struct nvc_focus_cap cap;
	struct nv_focuser_config nv_config;
	struct ad5816_pdata_info config;
	unsigned long ltv_ms;
	atomic_t in_use;
	bool reset_flag;
	int pwr_dev;
	s32 pos;
	u16 dev_id;
	u8 s_mode;
	char devname[16];
};

/**
 * The following are default values
 */

static struct ad5816_pdata_info ad5816_default_info = {
	.pos_low = AD5816_POS_LOW_DEFAULT,
	.pos_high = AD5816_POS_HIGH_DEFAULT,
};

static struct nvc_focus_cap ad5816_default_cap = {
	.version = NVC_FOCUS_CAP_VER2,
	.slew_rate = AD5816_SLEW_RATE,
	.actuator_range = AD5816_ACTUATOR_RANGE,
	.settle_time = AD5816_SETTLETIME,
	.focus_macro = AD5816_FOCUS_MACRO,
	.focus_infinity = AD5816_FOCUS_INFINITY,
	.focus_hyper = AD5816_FOCUS_INFINITY,
};

static struct nvc_focus_nvc ad5816_default_nvc = {
	.focal_length = AD5816_FOCAL_LENGTH,
	.fnumber = AD5816_FNUMBER,
};

static struct ad5816_platform_data ad5816_default_pdata = {
	.cfg = 0,
	.num = 0,
	.sync = 0,
	.dev_name = "focuser",
};
static LIST_HEAD(ad5816_info_list);
static DEFINE_SPINLOCK(ad5816_spinlock);

static int ad5816_i2c_rd8(struct ad5816_info *info, u8 addr, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[2];
	buf[0] = reg;
	if (addr) {
		msg[0].addr = addr;
		msg[1].addr = addr;
	} else {
		msg[0].addr = info->i2c_client->addr;
		msg[1].addr = info->i2c_client->addr;
	}
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &buf[0];
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &buf[1];
	*val = 0;
	if (i2c_transfer(info->i2c_client->adapter, msg, 2) != 2)
		return -EIO;
	*val = buf[1];
	return 0;
}

static int ad5816_i2c_wr8(struct ad5816_info *info, u8 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[2];
	buf[0] = reg;
	buf[1] = val;
	msg.addr = info->i2c_client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = &buf[0];
	if (i2c_transfer(info->i2c_client->adapter, &msg, 1) != 1)
		return -EIO;
	return 0;
}

static int ad5816_i2c_rd16(struct ad5816_info *info, u8 reg, u16 *val)
{
	struct i2c_msg msg[2];
	u8 buf[3];
	buf[0] = reg;
	msg[0].addr = info->i2c_client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &buf[0];
	msg[1].addr = info->i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = &buf[1];
	if (i2c_transfer(info->i2c_client->adapter, msg, 2) != 2)
		return -EIO;
	*val = (((u16)buf[1] << 8) | (u16)buf[2]);
	return 0;
}

static int ad5816_i2c_wr16(struct ad5816_info *info, u8 reg, u16 val)
{
	struct i2c_msg msg;
	u8 buf[3];
	buf[0] = reg;
	buf[1] = (u8)(val >> 8);
	buf[2] = (u8)(val & 0xff);
	msg.addr = info->i2c_client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = &buf[0];
	if (i2c_transfer(info->i2c_client->adapter, &msg, 1) != 1)
		return -EIO;
	return 0;
}

void ad5816_set_arc_mode(struct ad5816_info *info)
{
	int err;

	/* disable SCL low detection */
	err = ad5816_i2c_wr8(info, SCL_LOW_DETECTION, SCL_LOW_REG_VAL);
	if (err)
		dev_err(&info->i2c_client->dev, "%s: Low detect write failed\n",
			__func__);

	/* set ARC enable */
	err = ad5816_i2c_wr8(info, CONTROL, CONTROL_REG_VAL);
	if (err)
		dev_err(&info->i2c_client->dev,
		"%s: CONTROL reg write failed\n", __func__);

	/* set the ARC RES2 */
	err = ad5816_i2c_wr8(info, MODE, MODE_REG_VAL);
	if (err)
		dev_err(&info->i2c_client->dev,
		"%s: MODE reg write failed\n", __func__);

	/* set the VCM_FREQ : Tres = 10.86ms fres = 92Hz */
	err = ad5816_i2c_wr8(info, VCM_FREQ, VCM_FREQ_REG_VAL);
	if (err)
		dev_err(&info->i2c_client->dev,
		"%s: VCM_FREQ reg write failed\n", __func__);
}

static int ad5816_pm_wr(struct ad5816_info *info, int pwr)
{
	int err = 0;
	if ((info->pdata->cfg & (NVC_CFG_OFF2STDBY | NVC_CFG_BOOT_INIT)) &&
		(pwr == NVC_PWR_OFF ||
		pwr == NVC_PWR_STDBY_OFF))
			pwr = NVC_PWR_STDBY;

	if (pwr == info->pwr_dev)
		return 0;

	switch (pwr) {
	case NVC_PWR_OFF_FORCE:
	case NVC_PWR_OFF:
		if (info->pdata && info->pdata->power_off)
			info->pdata->power_off(&info->power);
		break;
	case NVC_PWR_STDBY_OFF:
	case NVC_PWR_STDBY:
		if (info->pdata && info->pdata->power_off)
			info->pdata->power_off(&info->power);
		break;
	case NVC_PWR_COMM:
	case NVC_PWR_ON:
		if (info->pdata && info->pdata->power_on)
			info->pdata->power_on(&info->power);
		usleep_range(1000, 1020);
		ad5816_set_arc_mode(info);
		break;
	default:
		err = -EINVAL;
		break;
	}

	if (err < 0) {
		dev_err(&info->i2c_client->dev, "%s err %d\n", __func__, err);
		pwr = NVC_PWR_ERR;
	}

	info->pwr_dev = pwr;
	dev_dbg(&info->i2c_client->dev, "%s pwr_dev=%d\n", __func__,
		info->pwr_dev);

	return err;
}

static int ad5816_power_put(struct ad5816_power_rail *pw)
{
	if (unlikely(!pw))
		return -EFAULT;

	if (likely(pw->vdd))
		regulator_put(pw->vdd);

	if (likely(pw->vdd_i2c))
		regulator_put(pw->vdd_i2c);

	pw->vdd = NULL;
	pw->vdd_i2c = NULL;

	return 0;
}

static int ad5816_regulator_get(struct ad5816_info *info,
	struct regulator **vreg, char vreg_name[])
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = regulator_get(&info->i2c_client->dev, vreg_name);
	if (unlikely(IS_ERR(reg))) {
		dev_err(&info->i2c_client->dev, "%s %s ERR: %d\n",
			__func__, vreg_name, (int)reg);
		err = PTR_ERR(reg);
		reg = NULL;
	} else
		dev_dbg(&info->i2c_client->dev, "%s: %s\n",
			__func__, vreg_name);

	*vreg = reg;
	return err;
}

static int ad5816_power_get(struct ad5816_info *info)
{
	struct ad5816_power_rail *pw = &info->power;

	ad5816_regulator_get(info, &pw->vdd, "vdd");
	ad5816_regulator_get(info, &pw->vdd_i2c, "vdd_i2c");

	return 0;
}

static int ad5816_pm_dev_wr(struct ad5816_info *info, int pwr)
{
	if (pwr < info->pwr_dev)
		pwr = info->pwr_dev;
	return ad5816_pm_wr(info, pwr);
}

static void ad5816_pm_exit(struct ad5816_info *info)
{
	ad5816_pm_wr(info, NVC_PWR_OFF_FORCE);
	ad5816_power_put(&info->power);
}

static void ad5816_pm_init(struct ad5816_info *info)
{
	ad5816_power_get(info);
}

static int ad5816_reset(struct ad5816_info *info, u32 level)
{
	int err = 0;

	if (level == NVC_RESET_SOFT)
		err |= ad5816_i2c_wr8(info, CONTROL, 0x01); /* SW reset */
	else
		err = ad5816_pm_wr(info, NVC_PWR_OFF_FORCE);

	return err;
}

static int ad5816_dev_id(struct ad5816_info *info)
{
	u16 val = 0;
	int err;

	ad5816_pm_dev_wr(info, NVC_PWR_COMM);
	err = ad5816_i2c_rd16(info, IC_INFO, &val);
	if (!err) {
		dev_dbg(&info->i2c_client->dev, "%s found devId: %x\n",
			__func__, val);
		info->dev_id = 0;
		if ((val & 0xff00) == 0x2400) {
			info->dev_id = val;
		}
		if (!info->dev_id) {
			err = -ENODEV;
			dev_dbg(&info->i2c_client->dev, "%s No devId match\n",
				__func__);
		}
	}

	ad5816_pm_dev_wr(info, NVC_PWR_OFF);
	return err;
}

/**
 * Below are device specific functions.
 */

static int ad5816_position_rd(struct ad5816_info *info, unsigned *position)
{

	u16 pos = 0;
	u8 t1 = 0;
	int err = 0;

	err = ad5816_i2c_rd8(info, 0, VCM_CODE_MSB, &t1);
	pos = t1;
	err  = ad5816_i2c_rd8(info, 0, VCM_CODE_LSB, &t1);
	pos = (pos << 8) | t1;

	if (pos < info->config.pos_low)
		pos = info->config.pos_low;
	else if (pos > info->config.pos_high)
		pos = info->config.pos_high;

	*position = pos;

	return err;
}

static int ad5816_position_wr(struct ad5816_info *info, s32 position)
{
	struct timeval tv;
	unsigned long tvl;
	unsigned long dly;
	int err;

	if (position < info->config.pos_low || position > info->config.pos_high)
		err = -EINVAL;
	else {
		do_gettimeofday(&tv);
		tvl = ((unsigned long)tv.tv_sec * USEC_PER_SEC +
			tv.tv_usec) / USEC_PER_MSEC;
		if (tvl - info->ltv_ms < info->cap.settle_time) {
			dly = (tvl - info->ltv_ms) * USEC_PER_MSEC;
			dev_dbg(&info->i2c_client->dev,
				"%s not settled(%lu uS).\n", __func__, dly);
			usleep_range(dly, dly + 20);
		}
		err = ad5816_i2c_wr16(info, VCM_CODE_MSB,
			position & AD5816_POS_CLAMP);
	}

	if (err)
		dev_err(&info->i2c_client->dev, "%s ERROR: %d\n",
			__func__, err);
	else
		info->ltv_ms = tvl;
	return err;
}

static void ad5816_get_focuser_capabilities(struct ad5816_info *info)
{
	memset(&info->nv_config, 0, sizeof(info->nv_config));

	info->nv_config.focal_length = info->nvc.focal_length;
	info->nv_config.fnumber = info->nvc.fnumber;
	info->nv_config.max_aperture = info->nvc.fnumber;
	info->nv_config.range_ends_reversed = 0;

	info->nv_config.pos_working_low = AF_POS_INVALID_VALUE;
	info->nv_config.pos_working_high = AF_POS_INVALID_VALUE;

	info->nv_config.pos_actual_low = info->config.pos_low;
	info->nv_config.pos_actual_high = info->config.pos_high;

	info->nv_config.slew_rate = info->cap.slew_rate;
	info->nv_config.circle_of_confusion = -1;
	info->nv_config.num_focuser_sets = 1;
	info->nv_config.focuser_set[0].macro = info->cap.focus_macro;
	info->nv_config.focuser_set[0].hyper = info->cap.focus_hyper;
	info->nv_config.focuser_set[0].inf = info->cap.focus_infinity;
	info->nv_config.focuser_set[0].settle_time = info->cap.settle_time;
}

static int ad5816_set_focuser_capabilities(struct ad5816_info *info,
					struct nvc_param *params)
{
	if (copy_from_user(&info->nv_config,
		(const void __user *)params->p_value,
		sizeof(struct nv_focuser_config))) {
			dev_err(&info->i2c_client->dev,
			"%s Error: copy_from_user bytes %d\n",
			__func__, sizeof(struct nv_focuser_config));
			return -EFAULT;
	}

	/* set pre-set value, as currently ODM sets incorrect value */
	info->cap.settle_time = AD5816_SETTLETIME;

	dev_dbg(&info->i2c_client->dev,
		"%s: copy_from_user bytes %d info->cap.settle_time %d\n",
		__func__, sizeof(struct nv_focuser_config),
		info->cap.settle_time);

	return 0;
}

static int ad5816_param_rd(struct ad5816_info *info, unsigned long arg)
{
	struct nvc_param params;
	const void *data_ptr = NULL;
	u32 data_size = 0;
	u32 position;
	int err;
	if (copy_from_user(&params,
		(const void __user *)arg,
		sizeof(struct nvc_param))) {
		dev_err(&info->i2c_client->dev, "%s %d copy_from_user err\n",
			__func__, __LINE__);
		return -EFAULT;
	}
	if (info->s_mode == NVC_SYNC_SLAVE)
		info = info->s_info;
	switch (params.param) {
	case NVC_PARAM_LOCUS:
		err = ad5816_position_rd(info, &position);
		if (err && !(info->pdata->cfg & NVC_CFG_NOERR))
			return err;
		data_ptr = &position;
		data_size = sizeof(position);
		dev_dbg(&info->i2c_client->dev, "%s LOCUS: %d\n",
			__func__, position);
		break;
	case NVC_PARAM_FOCAL_LEN:
		info->nvc.focal_length = AD5816_FOCAL_LENGTH;
		data_ptr = &info->nvc.focal_length;
		data_size = sizeof(info->nvc.focal_length);
		break;
	case NVC_PARAM_MAX_APERTURE:
		data_ptr = &info->nvc.max_aperature;
		data_size = sizeof(info->nvc.max_aperature);
		dev_dbg(&info->i2c_client->dev, "%s MAX_APERTURE: %x\n",
				__func__, info->nvc.max_aperature);
		break;
	case NVC_PARAM_FNUMBER:
		data_ptr = &info->nvc.fnumber;
		data_size = sizeof(info->nvc.fnumber);
		dev_dbg(&info->i2c_client->dev, "%s FNUMBER: %u\n",
				__func__, info->nvc.fnumber);
		break;
	case NVC_PARAM_CAPS:
		/* send back just what's requested or our max size */
		ad5816_get_focuser_capabilities(info);
		data_ptr = &info->nv_config;
		data_size = sizeof(info->nv_config);
		dev_err(&info->i2c_client->dev, "%s CAPS\n", __func__);
		break;
	case NVC_PARAM_STS:
		/*data_ptr = &info->sts;
		data_size = sizeof(info->sts);*/
		dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
		break;
	case NVC_PARAM_STEREO:
		data_ptr = &info->s_mode;
		data_size = sizeof(info->s_mode);
		dev_err(&info->i2c_client->dev, "%s STEREO: %d\n", __func__,
			info->s_mode);
		break;
	default:
		dev_err(&info->i2c_client->dev,
			"%s unsupported parameter: %d\n",
			__func__, params.param);
		return -EINVAL;
	}
	if (params.sizeofvalue < data_size) {
		dev_err(&info->i2c_client->dev,
			"%s data size mismatch %d != %d Param: %d\n",
			__func__, params.sizeofvalue, data_size, params.param);
		return -EINVAL;
	}
	if (copy_to_user((void __user *)params.p_value, data_ptr, data_size)) {
		dev_err(&info->i2c_client->dev, "%s copy_to_user err line %d\n",
			__func__, __LINE__);
		return -EFAULT;
	}
	return 0;
}

static int ad5816_param_wr_s(struct ad5816_info *info,
		struct nvc_param *params, s32 s32val)
{
	int err = 0;

	switch (params->param) {
	case NVC_PARAM_LOCUS:
		dev_dbg(&info->i2c_client->dev, "%s LOCUS: %d\n",
			__func__, s32val);
		err = ad5816_position_wr(info, s32val);
		return err;
	case NVC_PARAM_RESET:
		err = ad5816_reset(info, s32val);
		dev_dbg(&info->i2c_client->dev, "%s RESET: %d\n",
			__func__, err);
		return err;
	case NVC_PARAM_SELF_TEST:
		err = 0;
		dev_dbg(&info->i2c_client->dev, "%s SELF_TEST: %d\n",
			__func__, err);
		return err;
	default:
		dev_dbg(&info->i2c_client->dev,
			"%s unsupported parameter: %d\n",
			__func__, params->param);
		return -EINVAL;
	}
}

static int ad5816_param_wr(struct ad5816_info *info, unsigned long arg)
{
	struct nvc_param params;
	u8 u8val;
	s32 s32val;
	int err = 0;
	if (copy_from_user(&params, (const void __user *)arg,
		sizeof(struct nvc_param))) {
		dev_err(&info->i2c_client->dev,
			"%s copy_from_user err line %d\n",
			__func__, __LINE__);
		return -EFAULT;
	}
	if (copy_from_user(&s32val,
		(const void __user *)params.p_value, sizeof(s32val))) {
		dev_err(&info->i2c_client->dev, "%s %d copy_from_user err\n",
			__func__, __LINE__);
		return -EFAULT;
	}
	u8val = (u8)s32val;
	/* parameters independent of sync mode */
	switch (params.param) {
	case NVC_PARAM_STEREO:
		dev_dbg(&info->i2c_client->dev, "%s STEREO: %d\n",
			__func__, u8val);
		if (u8val == info->s_mode)
			return 0;
		switch (u8val) {
		case NVC_SYNC_OFF:
			info->s_mode = u8val;
			break;
		case NVC_SYNC_MASTER:
			info->s_mode = u8val;
			break;
		case NVC_SYNC_SLAVE:
			if (info->s_info != NULL) {
				/* default slave lens position */
				err = ad5816_position_wr(info->s_info,
					info->s_info->cap.focus_infinity);
				if (!err) {
					info->s_mode = u8val;
					info->s_info->s_mode = u8val;
				} else {
					if (info->s_mode != NVC_SYNC_STEREO)
						ad5816_pm_wr(info->s_info,
						NVC_PWR_OFF);
						err = -EIO;
				}
			} else {
				err = -EINVAL;
			}
			break;
		case NVC_SYNC_STEREO:
			if (info->s_info != NULL) {
				/* sync power */
				info->s_info->pwr_dev = info->pwr_dev;
				/* move slave lens to master position */
				err = ad5816_position_wr(info->s_info,
					(s32)info->pos);
				if (!err) {
					info->s_mode = u8val;
					info->s_info->s_mode = u8val;
				} else {
					if (info->s_mode != NVC_SYNC_SLAVE)
						ad5816_pm_wr(info->s_info,
							NVC_PWR_OFF);
					err = -EIO;
				}
			} else {
				err = -EINVAL;
			}
			break;
		default:
			err = -EINVAL;
		}
		if (info->pdata->cfg & NVC_CFG_NOERR)
			return 0;
		return err;

	case NVC_PARAM_CAPS:
		if (ad5816_set_focuser_capabilities(info, &params)) {
			dev_err(&info->i2c_client->dev,
				"%s: Error: copy_from_user bytes %d\n",
				__func__, params.sizeofvalue);
			return -EFAULT;
		}
		return 0;

	default:
		/* parameters dependent on sync mode */
		switch (info->s_mode) {
		case NVC_SYNC_OFF:
		case NVC_SYNC_MASTER:
			return ad5816_param_wr_s(info, &params, s32val);
		case NVC_SYNC_SLAVE:
			return ad5816_param_wr_s(info->s_info, &params, s32val);
		case NVC_SYNC_STEREO:
			err = ad5816_param_wr_s(info, &params, s32val);
			if (!(info->pdata->cfg & NVC_CFG_SYNC_I2C_MUX))
				err |= ad5816_param_wr_s(info->s_info,
						&params,
						s32val);
			return err;
		default:
			dev_err(&info->i2c_client->dev, "%s %d internal err\n",
					__func__, __LINE__);
			return -EINVAL;
		}
	}
}

static long ad5816_ioctl(struct file *file,
					unsigned int cmd,
					unsigned long arg)
{
	struct ad5816_info *info = file->private_data;
	int pwr;
	int err = 0;
	switch (_IOC_NR(cmd)) {
	case _IOC_NR(NVC_IOCTL_PARAM_WR):
		ad5816_pm_dev_wr(info, NVC_PWR_ON);
		err = ad5816_param_wr(info, arg);
		ad5816_pm_dev_wr(info, NVC_PWR_OFF);
		return err;
	case _IOC_NR(NVC_IOCTL_PARAM_RD):
		ad5816_pm_dev_wr(info, NVC_PWR_ON);
		err = ad5816_param_rd(info, arg);
		ad5816_pm_dev_wr(info, NVC_PWR_OFF);
		return err;
	case _IOC_NR(NVC_IOCTL_PWR_WR):
		/* This is a Guaranteed Level of Service (GLOS) call */
		pwr = (int)arg * 2;
		dev_dbg(&info->i2c_client->dev, "%s PWR_WR: %d\n",
				__func__, pwr);
		err = ad5816_pm_dev_wr(info, pwr);
		return err;
	case _IOC_NR(NVC_IOCTL_PWR_RD):
		if (info->s_mode == NVC_SYNC_SLAVE)
			pwr = info->s_info->pwr_dev;
		else
			pwr = info->pwr_dev;
		dev_dbg(&info->i2c_client->dev, "%s PWR_RD: %d\n",
				__func__, pwr);
		if (copy_to_user((void __user *)arg,
			(const void *)&pwr, sizeof(pwr))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_to_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}
		return 0;
	default:
		dev_dbg(&info->i2c_client->dev, "%s unsupported ioctl: %x\n",
			__func__, cmd);
	}
	return -EINVAL;
}


static void ad5816_sdata_init(struct ad5816_info *info)
{
	/* set defaults */
	memcpy(&info->config, &ad5816_default_info, sizeof(info->config));
	memcpy(&info->nvc, &ad5816_default_nvc, sizeof(info->nvc));
	memcpy(&info->cap, &ad5816_default_cap, sizeof(info->cap));

	info->config.settle_time = AD5816_SETTLETIME;
	info->config.focal_length = AD5816_FOCAL_LENGTH_FLOAT;
	info->config.fnumber = AD5816_FNUMBER_FLOAT;
	info->config.pos_low = AD5816_POS_LOW_DEFAULT;
	info->config.pos_high = AD5816_POS_HIGH_DEFAULT;

	/* set to proper value */
	info->cap.actuator_range = info->config.pos_high - info->config.pos_low;

	/* set overrides if any */
	if (info->pdata->nvc) {
		if (info->pdata->nvc->fnumber)
			info->nvc.fnumber = info->pdata->nvc->fnumber;
		if (info->pdata->nvc->focal_length)
			info->nvc.focal_length = info->pdata->nvc->focal_length;
		if (info->pdata->nvc->max_aperature)
			info->nvc.max_aperature =
				info->pdata->nvc->max_aperature;
	}

	if (info->pdata->cap) {
		if (info->pdata->cap->actuator_range)
			info->cap.actuator_range =
				info->pdata->cap->actuator_range;
		if (info->pdata->cap->settle_time)
			info->cap.settle_time = info->pdata->cap->settle_time;
		if (info->pdata->cap->focus_macro)
			info->cap.focus_macro = info->pdata->cap->focus_macro;
		if (info->pdata->cap->focus_hyper)
			info->cap.focus_hyper = info->pdata->cap->focus_hyper;
		if (info->pdata->cap->focus_infinity)
			info->cap.focus_infinity =
				info->pdata->cap->focus_infinity;
	}
}

static int ad5816_sync_en(unsigned num, unsigned sync)
{
	struct ad5816_info *master = NULL;
	struct ad5816_info *slave = NULL;
	struct ad5816_info *pos = NULL;
	rcu_read_lock();
	list_for_each_entry_rcu(pos, &ad5816_info_list, list) {
		if (pos->pdata->num == num) {
			master = pos;
			break;
		}
	}
	pos = NULL;
	list_for_each_entry_rcu(pos, &ad5816_info_list, list) {
		if (pos->pdata->num == sync) {
			slave = pos;
			break;
		}
	}
	rcu_read_unlock();
	if (master != NULL)
		master->s_info = NULL;
	if (slave != NULL)
		slave->s_info = NULL;
	if (!sync)
		return 0; /* no err if sync disabled */
	if (num == sync)
		return -EINVAL; /* err if sync instance is itself */
	if ((master != NULL) && (slave != NULL)) {
		master->s_info = slave;
		slave->s_info = master;
	}
	return 0;
}

static int ad5816_sync_dis(struct ad5816_info *info)
{
	if (info->s_info != NULL) {
		info->s_info->s_mode = 0;
		info->s_info->s_info = NULL;
		info->s_mode = 0;
		info->s_info = NULL;
		return 0;
	}
	return -EINVAL;
}

static int ad5816_open(struct inode *inode, struct file *file)
{
	struct ad5816_info *info = NULL;
	struct ad5816_info *pos = NULL;
	int err;
	rcu_read_lock();
	list_for_each_entry_rcu(pos, &ad5816_info_list, list) {
		if (pos->miscdev.minor == iminor(inode)) {
			info = pos;
			break;
		}
	}
	rcu_read_unlock();
	if (!info)
		return -ENODEV;
	err = ad5816_sync_en(info->pdata->num, info->pdata->sync);
	if (err == -EINVAL)
		dev_err(&info->i2c_client->dev,
			"%s err: invalid num (%u) and sync (%u) instance\n",
			__func__, info->pdata->num, info->pdata->sync);
	if (atomic_xchg(&info->in_use, 1))
		return -EBUSY;
	if (info->s_info != NULL) {
		if (atomic_xchg(&info->s_info->in_use, 1))
			return -EBUSY;
	}
	file->private_data = info;
	ad5816_pm_dev_wr(info, NVC_PWR_ON);
	ad5816_position_wr(info, info->cap.focus_infinity);
	ad5816_pm_dev_wr(info, NVC_PWR_OFF);
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);

	return 0;
}

static int ad5816_release(struct inode *inode, struct file *file)
{
	struct ad5816_info *info = file->private_data;
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	ad5816_pm_wr(info, NVC_PWR_OFF);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	if (info->s_info != NULL)
		WARN_ON(!atomic_xchg(&info->s_info->in_use, 0));
	ad5816_sync_dis(info);
	return 0;
}

static const struct file_operations ad5816_fileops = {
	.owner = THIS_MODULE,
	.open = ad5816_open,
	.unlocked_ioctl = ad5816_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ad5816_ioctl,
#endif

	.release = ad5816_release,
};

static void ad5816_del(struct ad5816_info *info)
{
	ad5816_pm_exit(info);
	if ((info->s_mode == NVC_SYNC_SLAVE) ||
		(info->s_mode == NVC_SYNC_STEREO))
		ad5816_pm_exit(info->s_info);

	ad5816_sync_dis(info);
	spin_lock(&ad5816_spinlock);
	list_del_rcu(&info->list);
	spin_unlock(&ad5816_spinlock);
	synchronize_rcu();
}

static int ad5816_remove(struct i2c_client *client)
{
	struct ad5816_info *info = i2c_get_clientdata(client);
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	misc_deregister(&info->miscdev);
	ad5816_del(info);
	return 0;
}
static struct of_device_id ad5816_of_match[] = {
	{ .compatible = "nvidia,ad5816", },
	{ },
};

MODULE_DEVICE_TABLE(of, ad5816_of_match);

static int ad5816_focuser_power_on(struct ad5816_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->vdd || !pw->vdd_i2c)))
		return -EFAULT;

	err = regulator_enable(pw->vdd_i2c);
	if (unlikely(err))
		goto ad5816_vdd_i2c_fail;

	err = regulator_enable(pw->vdd);
	if (unlikely(err))
		goto ad5816_vdd_fail;

	return 0;

ad5816_vdd_fail:
	regulator_disable(pw->vdd_i2c);

ad5816_vdd_i2c_fail:
	pr_err("%s FAILED\n", __func__);

	return -ENODEV;
}

static int ad5816_focuser_power_off(struct ad5816_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pw->vdd || !pw->vdd_i2c)))
		return -EFAULT;

	regulator_disable(pw->vdd);
	regulator_disable(pw->vdd_i2c);

	return 0;
}
static struct ad5816_platform_data *ad5816_parse_dt(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct ad5816_platform_data *board_info_pdata;
	const struct of_device_id *match;

	match = of_match_device(ad5816_of_match, &client->dev);
	if (!match) {
		dev_err(&client->dev, "Failed to find matching dt id\n");
		return NULL;
	}

	board_info_pdata = devm_kzalloc(&client->dev, sizeof(*board_info_pdata),
			GFP_KERNEL);
	if (!board_info_pdata) {
		dev_err(&client->dev, "Failed to allocate pdata\n");
		return NULL;
	}

	/* init with default platform data values */
	memcpy(board_info_pdata, &ad5816_default_pdata,
		sizeof(*board_info_pdata));

	of_property_read_u32(np, "nvidia,cfg", &board_info_pdata->cfg);
	of_property_read_u32(np, "nvidia,num", &board_info_pdata->num);
	of_property_read_u32(np, "nvidia,sync", &board_info_pdata->sync);
	of_property_read_string(np, "nvidia,dev_name",
			&board_info_pdata->dev_name);

	board_info_pdata->power_on = ad5816_focuser_power_on;
	board_info_pdata->power_off = ad5816_focuser_power_off;

	return board_info_pdata;
}

static int ad5816_probe(
		struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct ad5816_info *info;
	int err;

	dev_dbg(&client->dev, "%s\n", __func__);
	pr_info("ad5816: probing focuser.\n");

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}
	info->i2c_client = client;

	if (client->dev.of_node) {
		info->pdata = ad5816_parse_dt(client);
	} else if (client->dev.platform_data) {
		info->pdata = client->dev.platform_data;
	} else {
		info->pdata = &ad5816_default_pdata;
		dev_dbg(&client->dev, "%s No platform data.  Using defaults.\n",
			__func__);
	}

	i2c_set_clientdata(client, info);
	INIT_LIST_HEAD(&info->list);
	spin_lock(&ad5816_spinlock);
	list_add_rcu(&info->list, &ad5816_info_list);
	spin_unlock(&ad5816_spinlock);
	ad5816_pm_init(info);
	ad5816_sdata_init(info);

	if (info->pdata->cfg & (NVC_CFG_NODEV | NVC_CFG_BOOT_INIT)) {
		err = ad5816_dev_id(info);
		if (err < 0) {
			dev_err(&client->dev, "%s device not found\n",
				__func__);
			ad5816_pm_wr(info, NVC_PWR_OFF);
			if (info->pdata->cfg & NVC_CFG_NODEV) {
				ad5816_del(info);
				return -ENODEV;
			}
		} else {
			dev_dbg(&client->dev, "%s device found\n", __func__);
			if (info->pdata->cfg & NVC_CFG_BOOT_INIT) {
				/* initial move causes full initialization */
				ad5816_pm_dev_wr(info, NVC_PWR_ON);
				ad5816_position_wr(info,
					info->cap.focus_infinity);
				ad5816_pm_dev_wr(info, NVC_PWR_OFF);
			}
			if (info->pdata->detect)
				info->pdata->detect(
					&info->dev_id, sizeof(info->dev_id));
		}
	}

	if (info->pdata->dev_name != 0)
		strncpy(info->devname, info->pdata->dev_name,
				sizeof(info->devname) - 1);
	else
		strncpy(info->devname, "ad5816", sizeof(info->devname) - 1);

	if (info->pdata->num)
		snprintf(info->devname, sizeof(info->devname),
			"%s.%u", info->devname, info->pdata->num);

	info->miscdev.name = info->devname;
	info->miscdev.fops = &ad5816_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&info->miscdev)) {
		dev_err(&client->dev, "%s unable to register misc device %s\n",
			__func__, info->devname);
		ad5816_del(info);
		return -ENODEV;
	}

	return 0;
}


static const struct i2c_device_id ad5816_id[] = {
	{ "ad5816", 0 },
	{ },
};


MODULE_DEVICE_TABLE(i2c, ad5816_id);

static struct i2c_driver ad5816_i2c_driver = {
	.driver = {
		.name = "ad5816",
		.owner = THIS_MODULE,
	},
	.id_table = ad5816_id,
	.probe = ad5816_probe,
	.remove = ad5816_remove,
};

static int __init ad5816_init(void)
{
	return i2c_add_driver(&ad5816_i2c_driver);
}

static void __exit ad5816_exit(void)
{
	i2c_del_driver(&ad5816_i2c_driver);
}

module_init(ad5816_init);
module_exit(ad5816_exit);
MODULE_LICENSE("GPL v2");
