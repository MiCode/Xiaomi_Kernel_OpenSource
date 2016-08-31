/*
 * dw9718.c - dw9718 focuser driver
 *
 * Copyright (c) 2013-2014, NVIDIA Corporation. All Rights Reserved.
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
 * .info = Pointer to the dw9718_pdata_info structure.  This structure does
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
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <media/dw9718.h>

#define ENABLE_DEBUGFS_INTERFACE

#define dw9718_ID			0x04
#define dw9718_FOCAL_LENGTH_FLOAT	(4.570f)
#define dw9718_FNUMBER_FLOAT		(2.8f)
#define dw9718_FOCAL_LENGTH		(0x40923D71) /* 4.570f */
#define dw9718_FNUMBER			(0x40333333) /* 2.8f */
#define dw9718_SLEW_RATE		0x080002
#define dw9718_ACTUATOR_RANGE		1023
#define dw9718_SETTLETIME		10
#define dw9718_FOCUS_MACRO		620
#define dw9718_FOCUS_INFINITY		70
#define dw9718_POS_LOW_DEFAULT		0
#define dw9718_POS_HIGH_DEFAULT		1023
#define dw9718_POS_CLAMP		0x03ff
/* Need to decide exact value of VCM_THRESHOLD and its use */
/* define dw9718_VCM_THRESHOLD	20 */

struct dw9718_info {
	struct i2c_client *i2c_client;
	struct dw9718_platform_data *pdata;
	struct miscdevice miscdev;
	struct list_head list;
	struct dw9718_power_rail power;
	struct dw9718_info *s_info;
	struct nvc_focus_nvc nvc;
	struct nvc_focus_cap cap;
	struct nv_focuser_config nv_config;
	atomic_t in_use;
	bool reset_flag;
	int pwr_dev;
	int status;
	u32 cur_pos;
	u8 s_mode;
	char devname[16];
};

/**
 * The following are default values
 */
static struct nvc_focus_cap dw9718_default_cap = {
	.version = NVC_FOCUS_CAP_VER2,
	.slew_rate = dw9718_SLEW_RATE,
	.actuator_range = dw9718_ACTUATOR_RANGE,
	.settle_time = dw9718_SETTLETIME,
	.focus_macro = dw9718_FOCUS_MACRO,
	.focus_infinity = dw9718_FOCUS_INFINITY,
	.focus_hyper = dw9718_FOCUS_INFINITY,
};

static struct nvc_focus_nvc dw9718_default_nvc = {
	.focal_length = dw9718_FOCAL_LENGTH,
	.fnumber = dw9718_FNUMBER,
	.max_aperature = dw9718_FNUMBER,
};

static struct dw9718_platform_data dw9718_default_pdata = {
	.cfg = 0,
	.num = 0,
	.sync = 0,
	.dev_name = "focuser",
};
static LIST_HEAD(dw9718_info_list);
static DEFINE_SPINLOCK(dw9718_spinlock);

static int dw9718_i2c_wr8(struct dw9718_info *info, u8 reg, u8 val)
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

static int dw9718_i2c_wr16(struct dw9718_info *info, u8 reg, u16 val)
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

static int dw9718_i2c_rd8(struct dw9718_info *info, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[2];
	buf[0] = reg;
	msg[0].addr = info->i2c_client->addr;
	msg[1].addr = info->i2c_client->addr;
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

/**
 * Below are device specific functions.
 */
static int dw9718_position_wr(struct dw9718_info *info, s32 position)
{
	int err;

	dev_dbg(&info->i2c_client->dev, "%s %d\n", __func__, position);
	position &= dw9718_POS_CLAMP;
	err = dw9718_i2c_wr16(info, DW9718_VCM_CODE_MSB, position);
	if (!err)
		info->cur_pos = position;
	else
		dev_err(&info->i2c_client->dev, "%s: ERROR set position %d",
			__func__, position);
	return err;
}

int dw9718_set_arc_mode(struct dw9718_info *info)
{
	int err;
	u32 sr = info->nv_config.slew_rate;

	dev_dbg(&info->i2c_client->dev, "%s %x\n", __func__, sr);
	/* set ARC enable */
	err = dw9718_i2c_wr8(info, DW9718_CONTROL, (sr >> 16) & 0xFF);
	if (err) {
		dev_err(&info->i2c_client->dev,
		"%s: CONTROL reg write failed\n", __func__);
		goto set_arc_mode_done;
	}
	usleep_range(80, 100);

	/* set the ARC RES2 */
	err = dw9718_i2c_wr8(info, DW9718_SWITCH_MODE, (sr >> 8) & 0xFF);
	if (err) {
		dev_err(&info->i2c_client->dev,
		"%s: MODE write failed\n", __func__);
		goto set_arc_mode_done;
	}

	err = dw9718_i2c_wr8(info, DW9718_SACT, sr & 0XFF);
	if (err) {
		dev_err(&info->i2c_client->dev,
		"%s: RES write failed\n", __func__);
		goto set_arc_mode_done;
	}

set_arc_mode_done:
	return err;
}

static int dw9718_pm_wr(struct dw9718_info *info, int pwr)
{
	int err = 0;
	if ((info->pdata->cfg & (NVC_CFG_OFF2STDBY | NVC_CFG_BOOT_INIT)) &&
		(pwr == NVC_PWR_OFF || pwr == NVC_PWR_STDBY_OFF))
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
		dw9718_set_arc_mode(info);
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

static int dw9718_power_put(struct dw9718_power_rail *pw)
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

static int dw9718_regulator_get(struct dw9718_info *info,
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

static int dw9718_power_get(struct dw9718_info *info)
{
	struct dw9718_power_rail *pw = &info->power;

	dw9718_regulator_get(info, &pw->vdd, "vdd");
	dw9718_regulator_get(info, &pw->vdd_i2c, "vdd_i2c");

	return 0;
}

static int dw9718_pm_dev_wr(struct dw9718_info *info, int pwr)
{
	return dw9718_pm_wr(info, pwr);
}

static void dw9718_pm_exit(struct dw9718_info *info)
{
	dw9718_pm_wr(info, NVC_PWR_OFF_FORCE);
	dw9718_power_put(&info->power);
}

static int dw9718_reset(struct dw9718_info *info, u32 level)
{
	int err = 0;

	if (level == NVC_RESET_SOFT) {
		err = dw9718_i2c_wr8(info, DW9718_POWER_DN, 0x01);
		usleep_range(200, 220);
		err |= dw9718_i2c_wr8(info, DW9718_POWER_DN, 0x00);
		usleep_range(100, 120);
	} else
		err = dw9718_pm_wr(info, NVC_PWR_OFF_FORCE);

	return err;
}

static int dw9718_detect(struct dw9718_info *info)
{
	u8 val = 0;
	int err;

	dw9718_pm_dev_wr(info, NVC_PWR_COMM);
	err = dw9718_i2c_rd8(info, 0, &val);
	dw9718_pm_dev_wr(info, NVC_PWR_OFF);
	return err;
}

static void dw9718_get_focuser_capabilities(struct dw9718_info *info)
{
	memset(&info->nv_config, 0, sizeof(info->nv_config));

	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	info->nv_config.focal_length = info->nvc.focal_length;
	info->nv_config.fnumber = info->nvc.fnumber;
	info->nv_config.max_aperture = info->nvc.fnumber;
	info->nv_config.range_ends_reversed = 0;

	info->nv_config.pos_working_low = info->cap.focus_infinity;
	info->nv_config.pos_working_high = info->cap.focus_macro;
	info->nv_config.pos_actual_low = dw9718_POS_LOW_DEFAULT;
	info->nv_config.pos_actual_high = dw9718_POS_HIGH_DEFAULT;

	info->nv_config.slew_rate = info->cap.slew_rate;
	info->nv_config.circle_of_confusion = -1;
	info->nv_config.num_focuser_sets = 1;
	info->nv_config.focuser_set[0].macro = info->cap.focus_macro;
	info->nv_config.focuser_set[0].hyper = info->cap.focus_hyper;
	info->nv_config.focuser_set[0].inf = info->cap.focus_infinity;
	info->nv_config.focuser_set[0].settle_time = info->cap.settle_time;
}

static int dw9718_set_focuser_capabilities(struct dw9718_info *info,
					struct nvc_param *params)
{
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	if (copy_from_user(&info->nv_config,
		(const void __user *)params->p_value,
		sizeof(struct nv_focuser_config))) {
			dev_err(&info->i2c_client->dev,
			"%s Error: copy_from_user bytes %d\n",
			__func__, sizeof(struct nv_focuser_config));
			return -EFAULT;
	}

	/* set pre-set value, as currently ODM sets incorrect value */
	if (info->pdata->cap->settle_time)
		info->cap.settle_time = info->pdata->cap->settle_time;
	else
		info->cap.settle_time = dw9718_SETTLETIME;

	dev_dbg(&info->i2c_client->dev,
		"%s: copy_from_user bytes %d info->cap.settle_time %d\n",
		__func__, sizeof(struct nv_focuser_config),
		info->cap.settle_time);

	return 0;
}

static int dw9718_param_rd(struct dw9718_info *info, unsigned long arg)
{
	struct nvc_param params;
	const void *data_ptr = NULL;
	u32 data_size = 0;

	dev_dbg(&info->i2c_client->dev, "%s %lx\n", __func__, arg);
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
		data_ptr = &info->cur_pos;
		data_size = sizeof(info->cur_pos);
		dev_dbg(&info->i2c_client->dev, "%s LOCUS: %d\n",
			__func__, info->cur_pos);
		break;
	case NVC_PARAM_FOCAL_LEN:
		info->nvc.focal_length = dw9718_FOCAL_LENGTH;
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
		dw9718_get_focuser_capabilities(info);
		data_ptr = &info->nv_config;
		data_size = sizeof(info->nv_config);
		dev_err(&info->i2c_client->dev, "%s CAPS\n", __func__);
		break;
	case NVC_PARAM_STS:
		data_ptr = &info->status;
		data_size = sizeof(info->status);
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

static int dw9718_param_wr_s(struct dw9718_info *info,
		struct nvc_param *params, s32 s32val)
{
	int err = 0;

	switch (params->param) {
	case NVC_PARAM_LOCUS:
		dev_dbg(&info->i2c_client->dev, "%s LOCUS: %d\n",
			__func__, s32val);
		err = dw9718_position_wr(info, s32val);
		break;
	case NVC_PARAM_RESET:
		err = dw9718_reset(info, s32val);
		dev_dbg(&info->i2c_client->dev, "%s RESET\n", __func__);
		break;
	case NVC_PARAM_SELF_TEST:
		dev_dbg(&info->i2c_client->dev, "%s SELF_TEST\n", __func__);
		break;
	default:
		dev_dbg(&info->i2c_client->dev,
			"%s unsupported parameter: %d\n",
			__func__, params->param);
		err = -EINVAL;
		break;
	}

	if (err)
		dev_err(&info->i2c_client->dev, "ERROR! %d\n", err);
	return err;
}

static int dw9718_param_wr(struct dw9718_info *info, unsigned long arg)
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
				err = dw9718_position_wr(info->s_info,
					info->s_info->cap.focus_infinity);
				if (!err) {
					info->s_mode = u8val;
					info->s_info->s_mode = u8val;
				} else {
					if (info->s_mode != NVC_SYNC_STEREO)
						dw9718_pm_wr(info->s_info,
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
				err = dw9718_position_wr(info->s_info,
					(s32)info->cur_pos);
				if (!err) {
					info->s_mode = u8val;
					info->s_info->s_mode = u8val;
				} else {
					if (info->s_mode != NVC_SYNC_SLAVE)
						dw9718_pm_wr(info->s_info,
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
		if (dw9718_set_focuser_capabilities(info, &params)) {
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
			return dw9718_param_wr_s(info, &params, s32val);
		case NVC_SYNC_SLAVE:
			return dw9718_param_wr_s(info->s_info, &params, s32val);
		case NVC_SYNC_STEREO:
			err = dw9718_param_wr_s(info, &params, s32val);
			if (!(info->pdata->cfg & NVC_CFG_SYNC_I2C_MUX))
				err |= dw9718_param_wr_s(info->s_info,
						&params, s32val);
			return err;
		default:
			dev_err(&info->i2c_client->dev, "%s %d internal err\n",
					__func__, __LINE__);
			return -EINVAL;
		}
	}
}

static long dw9718_ioctl(struct file *file,
					unsigned int cmd,
					unsigned long arg)
{
	struct dw9718_info *info = file->private_data;
	int pwr;
	int err = 0;
	switch (_IOC_NR(cmd)) {
	case _IOC_NR(NVC_IOCTL_PARAM_WR):
		dw9718_pm_dev_wr(info, NVC_PWR_ON);
		err = dw9718_param_wr(info, arg);
		return err;
	case _IOC_NR(NVC_IOCTL_PARAM_RD):
		err = dw9718_param_rd(info, arg);
		return err;
	case _IOC_NR(NVC_IOCTL_PWR_WR):
		/* This is a Guaranteed Level of Service (GLOS) call */
		pwr = (int)arg * 2;
		dev_dbg(&info->i2c_client->dev, "%s PWR_WR: %d\n",
				__func__, pwr);
		err = dw9718_pm_dev_wr(info, pwr);
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


static void dw9718_sdata_init(struct dw9718_info *info)
{
	/* set defaults */
	memcpy(&info->nvc, &dw9718_default_nvc, sizeof(info->nvc));
	memcpy(&info->cap, &dw9718_default_cap, sizeof(info->cap));

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
		if (info->pdata->cap->slew_rate)
			info->cap.slew_rate = info->pdata->cap->slew_rate;
		if (info->pdata->cap->focus_macro)
			info->cap.focus_macro = info->pdata->cap->focus_macro;
		if (info->pdata->cap->focus_hyper)
			info->cap.focus_hyper = info->pdata->cap->focus_hyper;
		if (info->pdata->cap->focus_infinity)
			info->cap.focus_infinity =
				info->pdata->cap->focus_infinity;
	}
}

static int dw9718_sync_en(unsigned num, unsigned sync)
{
	struct dw9718_info *master = NULL;
	struct dw9718_info *slave = NULL;
	struct dw9718_info *pos = NULL;
	rcu_read_lock();
	list_for_each_entry_rcu(pos, &dw9718_info_list, list) {
		if (pos->pdata->num == num) {
			master = pos;
			break;
		}
	}
	pos = NULL;
	list_for_each_entry_rcu(pos, &dw9718_info_list, list) {
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

static int dw9718_sync_dis(struct dw9718_info *info)
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

static int dw9718_open(struct inode *inode, struct file *file)
{
	struct dw9718_info *info = NULL;
	struct dw9718_info *pos = NULL;
	int err;
	rcu_read_lock();
	list_for_each_entry_rcu(pos, &dw9718_info_list, list) {
		if (pos->miscdev.minor == iminor(inode)) {
			info = pos;
			break;
		}
	}
	rcu_read_unlock();
	if (!info)
		return -ENODEV;
	err = dw9718_sync_en(info->pdata->num, info->pdata->sync);
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
	dw9718_pm_dev_wr(info, NVC_PWR_ON);
	dw9718_position_wr(info, info->cap.focus_infinity);
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);

	return 0;
}

static int dw9718_release(struct inode *inode, struct file *file)
{
	struct dw9718_info *info = file->private_data;
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	dw9718_pm_wr(info, NVC_PWR_OFF);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	if (info->s_info != NULL)
		WARN_ON(!atomic_xchg(&info->s_info->in_use, 0));
	dw9718_sync_dis(info);
	return 0;
}

static const struct file_operations dw9718_fileops = {
	.owner = THIS_MODULE,
	.open = dw9718_open,
	.unlocked_ioctl = dw9718_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = dw9718_ioctl,
#endif
	.release = dw9718_release,
};

static void dw9718_del(struct dw9718_info *info)
{
	dw9718_pm_exit(info);
	if ((info->s_mode == NVC_SYNC_SLAVE) ||
		(info->s_mode == NVC_SYNC_STEREO))
		dw9718_pm_exit(info->s_info);

	dw9718_sync_dis(info);
	spin_lock(&dw9718_spinlock);
	list_del_rcu(&info->list);
	spin_unlock(&dw9718_spinlock);
	synchronize_rcu();
}

static int dw9718_remove(struct i2c_client *client)
{
	struct dw9718_info *info = i2c_get_clientdata(client);
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	misc_deregister(&info->miscdev);
	dw9718_del(info);
	return 0;
}

static int nvc_debugfs_init(const char *dir_name,
	struct dentry **d_entry, struct dentry **f_entry, void *info);

static int dw9718_probe(
		struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct dw9718_info *info;
	int err;
	dev_dbg(&client->dev, "%s\n", __func__);
	pr_info("dw9718: probing focuser.\n");

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}
	info->i2c_client = client;
	if (client->dev.platform_data) {
		info->pdata = client->dev.platform_data;
	} else {
		info->pdata = &dw9718_default_pdata;
		dev_dbg(&client->dev, "%s No platform data.  Using defaults.\n",
			__func__);
	}

	i2c_set_clientdata(client, info);
	INIT_LIST_HEAD(&info->list);
	spin_lock(&dw9718_spinlock);
	list_add_rcu(&info->list, &dw9718_info_list);
	spin_unlock(&dw9718_spinlock);
	dw9718_power_get(info);
	dw9718_sdata_init(info);
	if (info->pdata->cfg & (NVC_CFG_NODEV | NVC_CFG_BOOT_INIT)) {
		err = dw9718_detect(info);
		if (err < 0) {
			dev_err(&client->dev, "%s device not found\n",
				__func__);
			if (info->pdata->cfg & NVC_CFG_NODEV) {
				dw9718_del(info);
				return -ENODEV;
			}
		} else {
			dev_dbg(&client->dev, "%s device found\n", __func__);
			if (info->pdata->cfg & NVC_CFG_BOOT_INIT) {
				/* initial move causes full initialization */
				dw9718_pm_dev_wr(info, NVC_PWR_ON);
				dw9718_position_wr(
					info, info->cap.focus_infinity);
				dw9718_pm_dev_wr(info, NVC_PWR_OFF);
			}
			if (info->pdata->detect)
				info->pdata->detect(NULL, 0);
		}
	}

	if (info->pdata->dev_name != 0)
		strncpy(info->devname, info->pdata->dev_name,
			sizeof(info->devname) - 1);
	else
		strncpy(info->devname, "dw9718", sizeof(info->devname) - 1);

	if (info->pdata->num)
		snprintf(info->devname, sizeof(info->devname),
			"%s.%u", info->devname, info->pdata->num);

	info->miscdev.name = info->devname;
	info->miscdev.fops = &dw9718_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&info->miscdev)) {
		dev_err(&client->dev, "%s unable to register misc device %s\n",
			__func__, info->devname);
		dw9718_del(info);
		return -ENODEV;
	}

	nvc_debugfs_init(
		info->miscdev.this_device->kobj.name, NULL, NULL, info);

	return 0;
}

#ifdef ENABLE_DEBUGFS_INTERFACE
static int nvc_status_show(struct seq_file *s, void *data)
{
	struct dw9718_info *k_info = s->private;
	struct nv_focuser_config *pcfg = &k_info->nv_config;
	struct nvc_focus_cap *pcap = &k_info->cap;

	pr_info("%s\n", __func__);

	seq_printf(s, "focuser status:\n"
		"    Limit          = (%04d - %04d)\n"
		"    Range          = (%04d - %04d)\n"
		"    Current Pos    = %04d\n"
		"    Settle time    = %04d\n"
		"    Macro          = %04d\n"
		"    Infinity       = %04d\n"
		"    Hyper          = %04d\n"
		"    SlewRate       = 0x%06x\n"
		,
		pcfg->pos_actual_low, pcfg->pos_actual_high,
		pcfg->pos_working_low, pcfg->pos_working_high,
		k_info->cur_pos,
		pcap->settle_time,
		pcap->focus_macro,
		pcap->focus_infinity,
		pcap->focus_hyper,
		pcfg->slew_rate
		);

	return 0;
}

static ssize_t nvc_attr_set(struct file *s,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct dw9718_info *k_info =
		((struct seq_file *)s->private_data)->private;
	struct nv_focuser_config *pcfg = &k_info->nv_config;
	char buf[24];
	int buf_size;
	int err;
	u32 val = 0;

	pr_info("%s (%d)\n", __func__, count);

	if (!user_buf || count <= 1)
		return -EFAULT;

	memset(buf, 0, sizeof(buf));
	buf_size = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (sscanf(buf + 1, "0x%x", &val) == 1)
		goto set_attr;
	if (sscanf(buf + 1, "0X%x", &val) == 1)
		goto set_attr;
	if (sscanf(buf + 1, "%d", &val) == 1)
		goto set_attr;

	pr_err("SYNTAX ERROR: %s\n", buf);
	return -EFAULT;

set_attr:
	pr_info("new data = %x\n", val);
	switch (buf[0]) {
	case 'p':
		pr_info("new pos = %d\n", val);
		err = dw9718_position_wr(k_info, val);
		if (err)
			pr_err("ERROR set position %x\n", val);
		break;
	case 'h':
		if (val <= pcfg->pos_working_low || val >= 1024) {
			pr_info("new pos_high(%d) out of range\n",
				val);
			break;
		}
		pr_info("new pos_high = %d\n", val);
		pcfg->pos_working_high = val;
		break;
	case 'l':
		if (val >= pcfg->pos_working_high) {
			pr_info("new pos_low(%d) out of range\n",
				val);
			break;
		}
		pr_info("new pos_low = %d\n", val);
		pcfg->pos_working_low = val;
		break;
	case 'm':
		pr_info("new vcm mode = %x\n", val);
		pcfg->slew_rate = val;
		dw9718_set_arc_mode(k_info);
		break;
	}

	return count;
}

static int nvc_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, nvc_status_show, inode->i_private);
}

static const struct file_operations nvc_debugfs_fops = {
	.open = nvc_debugfs_open,
	.read = seq_read,
	.write = nvc_attr_set,
	.llseek = seq_lseek,
	.release = single_release,
};

static int nvc_debugfs_init(const char *dir_name,
	struct dentry **d_entry, struct dentry **f_entry, void *info)
{
	struct dentry *dp, *fp;

	dp = debugfs_create_dir(dir_name, NULL);
	if (dp == NULL) {
		pr_info("%s: debugfs create dir failed\n", __func__);
		return -ENOMEM;
	}

	fp = debugfs_create_file("d", S_IRUGO|S_IWUSR,
		dp, info, &nvc_debugfs_fops);
	if (!fp) {
		pr_info("%s: debugfs create file failed\n", __func__);
		debugfs_remove_recursive(dp);
		return -ENOMEM;
	}

	if (d_entry)
		*d_entry = dp;
	if (f_entry)
		*f_entry = fp;
	return 0;
}
#else
static int nvc_debugfs_init(const char *dir_name,
	struct dentry **d_entry, struct dentry **f_entry, void *info)
{
	return 0;
}
#endif


static const struct i2c_device_id dw9718_id[] = {
	{ "dw9718", 0 },
	{ },
};


MODULE_DEVICE_TABLE(i2c, dw9718_id);

static struct i2c_driver dw9718_i2c_driver = {
	.driver = {
		.name = "dw9718",
		.owner = THIS_MODULE,
	},
	.id_table = dw9718_id,
	.probe = dw9718_probe,
	.remove = dw9718_remove,
};

static int __init dw9718_init(void)
{
	return i2c_add_driver(&dw9718_i2c_driver);
}

static void __exit dw9718_exit(void)
{
	i2c_del_driver(&dw9718_i2c_driver);
}

module_init(dw9718_init);
module_exit(dw9718_exit);
MODULE_LICENSE("GPL v2");
