/*
 * ad5816.c - a NVC kernel driver for focuser device ad5816.
 *
 * Copyright (c) 2011-2013 NVIDIA Corporation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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

#define AD5816_ID			0x04
#define AD5816_FOCAL_LENGTH_FLOAT	(4.570f)
#define AD5816_FNUMBER_FLOAT		(2.8f)
#define AD5816_FOCAL_LENGTH		(0x40923D71) /* 4.570f */
#define AD5816_FNUMBER			(0x40333333) /* 2.8f */
#define AD5816_ACTUATOR_RANGE		1023
#define AD5816_SETTLETIME		20
#define AD5816_FOCUS_MACRO		536
#define AD5816_FOCUS_INFINITY		80
#define AD5816_POS_LOW_DEFAULT		0
#define AD5816_POS_HIGH_DEFAULT		1023
#define AD5816_POS_CLAMP		0x03ff

#define AD5816_CONTROL_RING		0x02
#define AD5816_MODE_ARC15		0x40
#define AD5816_MODE_ARC10		0x00
#define AD5816_MODE_ARC20		0x01

#define AD5816_DRV_MODE_SWITCHED	(0x0 << 1)
#define AD5816_DRV_MODE_LINEAR		(0x1 << 1)

/* Need to decide exact value of VCM_THRESHOLD and its use */
/* define AD5816_VCM_THRESHOLD	20 */

struct ad5816_info {
	struct device *dev;
	struct i2c_client *i2c_client;
	struct ad5816_platform_data *pdata;
	struct miscdevice miscdev;
	struct list_head list;
	struct ad5816_power_rail power;
	struct nvc_focus_nvc nvc;
	struct nvc_focus_cap cap;
	struct nv_focuser_config nv_config;
	struct nv_focuser_config cfg_usr;
	atomic_t in_use;
	bool reset_flag;
	int pwr_dev;
	int pwr_api;
	s32 pos;
	u16 dev_id;
};

/**
 * The following are default values
 */

static struct nvc_focus_cap ad5816_default_cap = {
	.version = NVC_FOCUS_CAP_VER2,
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
	u32 sr = info->nv_config.slew_rate;
	int err;

	/* disable SCL low detection */
	err = ad5816_i2c_wr8(info, SCL_LOW_DETECTION, 0xb6);
	if (err)
		dev_err(info->dev, "%s: Low detect write failed\n",
			__func__);

	/* set ARC enable */
	err = ad5816_i2c_wr8(info, CONTROL, (sr >> 16) & 0xff);
	if (err)
		dev_err(info->dev, "%s: CONTROL reg write failed\n", __func__);

	/* set the ARC RES mode */
	err = ad5816_i2c_wr8(info, MODE, (sr >> 8) & 0xff);
	if (err)
		dev_err(info->dev, "%s: MODE reg write failed\n", __func__);

	/* set the VCM_FREQ */
	err = ad5816_i2c_wr8(info, VCM_FREQ, sr & 0xff);
	if (err)
		dev_err(info->dev, "%s: VCM_FREQ reg write failed\n", __func__);
}

static int ad5816_position_wr(struct ad5816_info *info, u16 position)
{
	int err = ad5816_i2c_wr16(
		info, VCM_CODE_MSB, position & AD5816_POS_CLAMP);

	if (!err)
		info->pos = position & AD5816_POS_CLAMP;
	return err;
}

static int ad5816_pm_wr(struct ad5816_info *info, int pwr)
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
		usleep_range(1000, 1020);
		ad5816_set_arc_mode(info);
		ad5816_position_wr(info, (u16)info->nv_config.pos_working_low);
		break;
	default:
		err = -EINVAL;
		break;
	}

	if (err < 0) {
		dev_err(info->dev, "%s err %d\n", __func__, err);
		pwr = NVC_PWR_ERR;
	}

	info->pwr_dev = pwr;
	dev_dbg(info->dev, "%s pwr_dev=%d\n", __func__, info->pwr_dev);

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

	reg = regulator_get(info->dev, vreg_name);
	if (unlikely(IS_ERR(reg))) {
		dev_err(info->dev, "%s %s ERR: %d\n",
			__func__, vreg_name, (int)reg);
		err = PTR_ERR(reg);
		reg = NULL;
	} else
		dev_dbg(info->dev, "%s: %s\n", __func__, vreg_name);

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

static int ad5816_pm_api_wr(struct ad5816_info *info, int pwr)
{
	int err = 0;

	if (!pwr || (pwr > NVC_PWR_ON))
		return 0;

	if (pwr > info->pwr_dev)
		err = ad5816_pm_wr(info, pwr);
	if (!err)
		info->pwr_api = pwr;
	else
		info->pwr_api = NVC_PWR_ERR;
	if (info->pdata->cfg & NVC_CFG_NOERR)
		return 0;

	return err;
}

static int ad5816_pm_dev_wr(struct ad5816_info *info, int pwr)
{
	if (pwr < info->pwr_api)
		pwr = info->pwr_api;
	return ad5816_pm_wr(info, pwr);
}

static inline void ad5816_pm_exit(struct ad5816_info *info)
{
	ad5816_pm_dev_wr(info, NVC_PWR_OFF_FORCE);
	ad5816_power_put(&info->power);
}

static inline void ad5816_pm_init(struct ad5816_info *info)
{
	ad5816_power_get(info);
}

static int ad5816_reset(struct ad5816_info *info, u32 level)
{
	int err = 0;

	if (level == NVC_RESET_SOFT)
		err |= ad5816_i2c_wr8(info, CONTROL, 0x01); /* SW reset */
	else
		err = ad5816_pm_dev_wr(info, NVC_PWR_OFF_FORCE);

	err |= ad5816_pm_wr(info, info->pwr_api);
	return err;
}

static int ad5816_dev_id(struct ad5816_info *info)
{
	u16 val = 0;
	int err;

	err = ad5816_i2c_rd16(info, IC_INFO, &val);
	if (!err) {
		dev_dbg(info->dev, "%s found devId: %x\n", __func__, val);
		info->dev_id = 0;
		if ((val & 0xff00) == 0x2400)
			info->dev_id = val;

		if (!info->dev_id) {
			err = -ENODEV;
			dev_dbg(info->dev, "%s No devId match\n", __func__);
		}
	}

	return err;
}

static void ad5816_dump_focuser_capabilities(struct ad5816_info *info)
{
	dev_dbg(info->dev, "%s:\n", __func__);
	dev_dbg(info->dev, "focal_length:               0x%x\n",
		info->nv_config.focal_length);
	dev_dbg(info->dev, "fnumber:                    0x%x\n",
		info->nv_config.fnumber);
	dev_dbg(info->dev, "max_aperture:               0x%x\n",
		info->nv_config.max_aperture);
	dev_dbg(info->dev, "pos_working_low:            %d\n",
		info->nv_config.pos_working_low);
	dev_dbg(info->dev, "pos_working_high:           %d\n",
		info->nv_config.pos_working_high);
	dev_dbg(info->dev, "pos_actual_low:             %d\n",
		info->nv_config.pos_actual_low);
	dev_dbg(info->dev, "pos_actual_high:            %d\n",
		info->nv_config.pos_actual_high);
	dev_dbg(info->dev, "slew_rate:                  0x%x\n",
		info->nv_config.slew_rate);
	dev_dbg(info->dev, "circle_of_confusion:        %d\n",
		info->nv_config.circle_of_confusion);
	dev_dbg(info->dev, "num_focuser_sets:           %d\n",
		info->nv_config.num_focuser_sets);
	dev_dbg(info->dev, "focuser_set[0].macro:       %d\n",
		info->nv_config.focuser_set[0].macro);
	dev_dbg(info->dev, "focuser_set[0].hyper:       %d\n",
		info->nv_config.focuser_set[0].hyper);
	dev_dbg(info->dev, "focuser_set[0].inf:         %d\n",
		info->nv_config.focuser_set[0].inf);
	dev_dbg(info->dev, "focuser_set[0].settle_time: %d\n",
		info->nv_config.focuser_set[0].settle_time);
}

static void ad5816_get_focuser_capabilities(struct ad5816_info *info)
{
	memset(&info->nv_config, 0, sizeof(info->nv_config));

	info->nv_config.focal_length = info->nvc.focal_length;
	info->nv_config.fnumber = info->nvc.fnumber;
	info->nv_config.max_aperture = info->nvc.fnumber;
	info->nv_config.range_ends_reversed = 0;

	info->nv_config.pos_working_low = info->cap.focus_infinity;
	info->nv_config.pos_working_high = info->cap.focus_macro;

	info->nv_config.pos_actual_low = AD5816_POS_LOW_DEFAULT;
	info->nv_config.pos_actual_high = AD5816_POS_HIGH_DEFAULT;

	info->nv_config.circle_of_confusion = -1;
	info->nv_config.num_focuser_sets = 1;
	info->nv_config.focuser_set[0].macro = info->cap.focus_macro;
	info->nv_config.focuser_set[0].hyper = info->cap.focus_hyper;
	info->nv_config.focuser_set[0].inf = info->cap.focus_infinity;
	info->nv_config.focuser_set[0].settle_time = info->cap.settle_time;
	if (info->pdata && info->pdata->arc_mode &&
		(info->pdata->arc_mode != 0xff)) {
		dev_dbg(info->dev, "arc_mode: %x, freq: %d\n",
			info->pdata->arc_mode, info->pdata->lens_freq);
		info->nv_config.slew_rate = AD5816_CONTROL_RING << 16;
		switch (info->pdata->arc_mode) {
		case 1:
			info->nv_config.slew_rate |= AD5816_MODE_ARC10 << 8;
			break;
		case 2:
			info->nv_config.slew_rate |= AD5816_MODE_ARC20 << 8;
			break;
		case 3:
			info->nv_config.slew_rate |= AD5816_MODE_ARC15 << 8;
			break;
		default:
			info->nv_config.slew_rate = 0;
			dev_err(info->dev,
				"%s ERROR: unrecognized focuser arc mode %x!\n",
				__func__, info->pdata->arc_mode);
			break;
		}
		/* Fres = 1S/(51.2uS x (VCM_FREQ + 128)) */
		/* VCM_FREQ = 1000000 / (51.2 * Fres) - 128 */
		if (info->nv_config.slew_rate && info->pdata->lens_freq)
			info->nv_config.slew_rate |= (800000000 / 512 +
				info->pdata->lens_freq / 2 - 1) /
				info->pdata->lens_freq / 8 - 128;
	}

	/* set drive mode to linear */
	info->nv_config.slew_rate |= AD5816_DRV_MODE_LINEAR << 8;

	ad5816_dump_focuser_capabilities(info);
}

static int ad5816_set_focuser_capabilities(struct ad5816_info *info,
					struct nvc_param *params)
{
	if (copy_from_user(&info->cfg_usr,
		(const void __user *)params->p_value, sizeof(info->cfg_usr))) {
			dev_err(info->dev, "%s Err: copy_from_user bytes %d\n",
			__func__, sizeof(info->cfg_usr));
			return -EFAULT;
	}

	if (info->cfg_usr.focal_length)
		info->nv_config.focal_length = info->cfg_usr.focal_length;
	if (info->cfg_usr.fnumber)
		info->nv_config.fnumber = info->cfg_usr.fnumber;
	if (info->cfg_usr.max_aperture)
		info->nv_config.max_aperture = info->cfg_usr.max_aperture;

	if (info->cfg_usr.pos_working_low != AF_POS_INVALID_VALUE)
		info->nv_config.pos_working_low = info->cfg_usr.pos_working_low;
	if (info->cfg_usr.pos_working_high != AF_POS_INVALID_VALUE)
		info->nv_config.pos_working_high =
			info->cfg_usr.pos_working_high;
	if (info->cfg_usr.pos_actual_low != AF_POS_INVALID_VALUE)
		info->nv_config.pos_actual_low = info->cfg_usr.pos_actual_low;
	if (info->cfg_usr.pos_actual_high != AF_POS_INVALID_VALUE)
		info->nv_config.pos_actual_high = info->cfg_usr.pos_actual_high;

	if (info->cfg_usr.circle_of_confusion != AF_POS_INVALID_VALUE)
		info->nv_config.circle_of_confusion =
			info->cfg_usr.circle_of_confusion;

	if (info->cfg_usr.focuser_set[0].macro != AF_POS_INVALID_VALUE)
		info->nv_config.focuser_set[0].macro =
			info->cfg_usr.focuser_set[0].macro;
	if (info->cfg_usr.focuser_set[0].hyper != AF_POS_INVALID_VALUE)
		info->nv_config.focuser_set[0].hyper =
			info->cfg_usr.focuser_set[0].hyper;
	if (info->cfg_usr.focuser_set[0].inf != AF_POS_INVALID_VALUE)
		info->nv_config.focuser_set[0].inf =
			info->cfg_usr.focuser_set[0].inf;
	if (info->cfg_usr.focuser_set[0].settle_time != AF_POS_INVALID_VALUE)
		info->nv_config.focuser_set[0].settle_time =
			info->cfg_usr.focuser_set[0].settle_time;

	dev_dbg(info->dev,
		"%s: copy_from_user bytes %d info->cap.settle_time %d\n",
		__func__, sizeof(struct nv_focuser_config),
		info->cap.settle_time);

	ad5816_dump_focuser_capabilities(info);
	return 0;
}

static int ad5816_param_rd(struct ad5816_info *info, unsigned long arg)
{
	struct nvc_param params;
	const void *data_ptr = NULL;
	u32 data_size = 0;
	int err = 0;

	if (copy_from_user(&params,
		(const void __user *)arg,
		sizeof(struct nvc_param))) {
		dev_err(info->dev, "%s %d copy_from_user err\n",
			__func__, __LINE__);
		return -EFAULT;
	}

	switch (params.param) {
	case NVC_PARAM_LOCUS:
		data_ptr = &info->pos;
		data_size = sizeof(info->pos);
		dev_dbg(info->dev, "%s LOCUS: %d\n", __func__, info->pos);
		break;
	case NVC_PARAM_FOCAL_LEN:
		data_ptr = &info->nv_config.focal_length;
		data_size = sizeof(info->nv_config.focal_length);
		break;
	case NVC_PARAM_MAX_APERTURE:
		data_ptr = &info->nv_config.max_aperture;
		data_size = sizeof(info->nv_config.max_aperture);
		dev_dbg(info->dev, "%s MAX_APERTURE: %x\n",
			__func__, info->nv_config.max_aperture);
		break;
	case NVC_PARAM_FNUMBER:
		data_ptr = &info->nv_config.fnumber;
		data_size = sizeof(info->nv_config.fnumber);
		dev_dbg(info->dev, "%s FNUMBER: %u\n",
			__func__, info->nv_config.fnumber);
		break;
	case NVC_PARAM_CAPS:
		/* send back just what's requested or our max size */
		data_ptr = &info->nv_config;
		data_size = sizeof(info->nv_config);
		dev_err(info->dev, "%s CAPS\n", __func__);
		break;
	case NVC_PARAM_STS:
		/*data_ptr = &info->sts;
		data_size = sizeof(info->sts);*/
		dev_dbg(info->dev, "%s\n", __func__);
		break;
	case NVC_PARAM_STEREO:
	default:
		dev_err(info->dev, "%s unsupported parameter: %d\n",
			__func__, params.param);
		err = -EINVAL;
		break;
	}
	if (!err && params.sizeofvalue < data_size) {
		dev_err(info->dev,
			"%s data size mismatch %d != %d Param: %d\n",
			__func__, params.sizeofvalue, data_size, params.param);
		return -EINVAL;
	}
	if (!err && copy_to_user((void __user *)params.p_value,
		data_ptr, data_size)) {
		dev_err(info->dev,
			"%s copy_to_user err line %d\n", __func__, __LINE__);
		return -EFAULT;
	}
	return err;
}

static int ad5816_param_wr(struct ad5816_info *info, unsigned long arg)
{
	struct nvc_param params;
	u8 u8val;
	u32 u32val;
	int err = 0;

	if (copy_from_user(&params, (const void __user *)arg,
		sizeof(struct nvc_param))) {
		dev_err(info->dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
		return -EFAULT;
	}
	if (copy_from_user(&u32val,
		(const void __user *)params.p_value, sizeof(u32val))) {
		dev_err(info->dev, "%s %d copy_from_user err\n",
			__func__, __LINE__);
		return -EFAULT;
	}
	u8val = (u8)u32val;

	/* parameters independent of sync mode */
	switch (params.param) {
	case NVC_PARAM_CAPS:
		if (ad5816_set_focuser_capabilities(info, &params)) {
			dev_err(info->dev,
				"%s: Error: copy_from_user bytes %d\n",
				__func__, params.sizeofvalue);
			err = -EFAULT;
		}
		break;
	case NVC_PARAM_LOCUS:
		dev_dbg(info->dev, "%s LOCUS: %d\n", __func__, u32val);
		err = ad5816_position_wr(info, (u16)u32val);
		break;
	case NVC_PARAM_RESET:
		err = ad5816_reset(info, u32val);
		dev_dbg(info->dev, "%s RESET: %d\n", __func__, err);
		break;
	case NVC_PARAM_SELF_TEST:
		err = 0;
		dev_dbg(info->dev, "%s SELF_TEST: %d\n", __func__, err);
		break;
	default:
		dev_dbg(info->dev, "%s unsupported parameter: %d\n",
			__func__, params.param);
		err = -EINVAL;
		break;
	}
	return err;
}

static long ad5816_ioctl(struct file *file,
					unsigned int cmd,
					unsigned long arg)
{
	struct ad5816_info *info = file->private_data;
	int pwr;
	int err = 0;
	switch (cmd) {
	case NVC_IOCTL_PARAM_WR:
		err = ad5816_param_wr(info, arg);
		return err;
	case NVC_IOCTL_PARAM_RD:
		err = ad5816_param_rd(info, arg);
		return err;
	case NVC_IOCTL_PWR_WR:
		/* This is a Guaranteed Level of Service (GLOS) call */
		pwr = (int)arg * 2;
		dev_dbg(info->dev, "%s PWR_WR: %d\n", __func__, pwr);
		err = ad5816_pm_api_wr(info, pwr);
		return err;
	case NVC_IOCTL_PWR_RD:
		pwr = info->pwr_api / 2;
		dev_dbg(info->dev, "%s PWR_RD: %d\n", __func__, pwr);
		if (copy_to_user((void __user *)arg,
			(const void *)&pwr, sizeof(pwr))) {
			dev_err(info->dev, "%s copy_to_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}
		return 0;
	default:
		dev_dbg(info->dev, "%s unsupported ioctl: %x\n", __func__, cmd);
	}
	return -EINVAL;
}


static void ad5816_sdata_init(struct ad5816_info *info)
{
	/* set defaults */
	memcpy(&info->nvc, &ad5816_default_nvc, sizeof(info->nvc));
	memcpy(&info->cap, &ad5816_default_cap, sizeof(info->cap));

	/* set to proper value */
	info->cap.actuator_range =
		AD5816_POS_HIGH_DEFAULT - AD5816_POS_LOW_DEFAULT;

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

	ad5816_get_focuser_capabilities(info);
}

static int ad5816_open(struct inode *inode, struct file *file)
{
	struct ad5816_info *info = NULL;
	struct ad5816_info *pos = NULL;

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

	if (atomic_xchg(&info->in_use, 1))
		return -EBUSY;
	file->private_data = info;

	dev_dbg(info->dev, "%s\n", __func__);
	return 0;
}

static int ad5816_release(struct inode *inode, struct file *file)
{
	struct ad5816_info *info = file->private_data;
	dev_dbg(info->dev, "%s\n", __func__);
	ad5816_pm_wr(info, NVC_PWR_OFF);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	return 0;
}

static const struct file_operations ad5816_fileops = {
	.owner = THIS_MODULE,
	.open = ad5816_open,
	.unlocked_ioctl = ad5816_ioctl,
	.release = ad5816_release,
};

static void ad5816_del(struct ad5816_info *info)
{
	ad5816_pm_exit(info);
	spin_lock(&ad5816_spinlock);
	list_del_rcu(&info->list);
	spin_unlock(&ad5816_spinlock);
	synchronize_rcu();
}

static int ad5816_remove(struct i2c_client *client)
{
	struct ad5816_info *info = i2c_get_clientdata(client);
	dev_dbg(info->dev, "%s\n", __func__);
	misc_deregister(&info->miscdev);
	ad5816_del(info);
	return 0;
}

static int ad5816_probe(
		struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct ad5816_info *info;
	char dname[16];
	int err;

	dev_dbg(&client->dev, "%s\n", __func__);
	pr_info("ad5816: probing focuser.\n");

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}
	info->i2c_client = client;
	info->dev = &client->dev;
	if (client->dev.platform_data)
		info->pdata = client->dev.platform_data;
	else {
		info->pdata = &ad5816_default_pdata;
		dev_dbg(info->dev, "%s No platform data. Using defaults.\n",
			__func__);
	}

	i2c_set_clientdata(client, info);
	INIT_LIST_HEAD(&info->list);
	spin_lock(&ad5816_spinlock);
	list_add_rcu(&info->list, &ad5816_info_list);
	spin_unlock(&ad5816_spinlock);
	ad5816_pm_init(info);
	info->pwr_api = NVC_PWR_OFF;
	if (info->pdata->cfg & (NVC_CFG_NODEV | NVC_CFG_BOOT_INIT)) {
		ad5816_pm_dev_wr(info, NVC_PWR_COMM);
		err = ad5816_dev_id(info);
		ad5816_pm_dev_wr(info, NVC_PWR_OFF);
		if (err < 0) {
			dev_err(info->dev, "%s device not found\n",
				__func__);
			if (info->pdata->cfg & NVC_CFG_NODEV) {
				ad5816_del(info);
				return -ENODEV;
			}
		} else {
			dev_dbg(info->dev, "%s device found\n", __func__);
			if (info->pdata->cfg & NVC_CFG_BOOT_INIT) {
				/* initial move causes full initialization */
				ad5816_pm_wr(info, NVC_PWR_ON);
				ad5816_position_wr(info,
					(u16)info->nv_config.pos_working_low);
				ad5816_pm_wr(info, NVC_PWR_OFF);
			}
			if (info->pdata->detect)
				info->pdata->detect(
					&info->dev_id, sizeof(info->dev_id));
		}
	}

	ad5816_sdata_init(info);

	if (info->pdata->dev_name != 0)
		strcpy(dname, info->pdata->dev_name);
	else
		strcpy(dname, "ad5816");

	if (info->pdata->num)
		snprintf(dname, sizeof(dname),
			"%s.%u", dname, info->pdata->num);

	info->miscdev.name = dname;
	info->miscdev.fops = &ad5816_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&info->miscdev)) {
		dev_err(info->dev, "%s unable to register misc device %s\n",
			__func__, dname);
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

module_i2c_driver(ad5816_i2c_driver);
MODULE_LICENSE("GPL v2");
