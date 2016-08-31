/*
 * ssl3250a.c - ssl3250a flash/torch kernel driver
 *
 * Copyright (c) 2011-2014, NVIDIA Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

/* Implementation
 * --------------
 * The board level details about the device need to be provided in the board
 * file with the ssl3250a_platform_data structure.
 * Standard among NVC kernel drivers in this structure is:
 * .cfg = Use the NVC_CFG_ defines that are in nvc_torch.h.
 *        Descriptions of the configuration options are with the defines.
 *        This value is typically 0.
 * .num = The number of the instance of the device.  This should start at 1 and
 *        and increment for each device on the board.  This number will be
 *        appended to the MISC driver name, Example: /dev/ssl3250a.1
 * .sync = If there is a need to synchronize two devices, then this value is
 *         the number of the device instance this device is allowed to sync to.
 *         This is typically used for stereo applications.
 * .dev_name = The MISC driver name the device registers as.  If not used,
 *             then the part number of the device is used for the driver name.
 *             If using the NVC user driver then use the name found in this
 *             driver under _default_pdata.
 *
 * The following is specific to NVC kernel flash/torch drivers:
 * .pinstate = a pointer to the nvc_torch_pin_state structure.  This
 *             structure gives the details of which VI GPIO to use to trigger
 *             the flash.  The mask tells which pin and the values is the
 *             level.  For example, if VI GPIO pin 6 is used, then
 *             .mask = 0x0040
 *             .values = 0x0040
 *             If VI GPIO pin 0 is used, then
 *             .mask = 0x0001
 *             .values = 0x0001
 *             This is typically just one pin but there is some legacy
 *             here that insinuates more than one pin can be used.
 *             When the flash level is set, then the driver will return the
 *             value in values.  When the flash level is off, the driver will
 *             return 0 for the values to deassert the signal.
 *             If a VI GPIO is not used, then the mask and values must be set
 *             to 0.  The flash may then be triggered via I2C instead.
 *             However, a VI GPIO is strongly encouraged since it allows
 *             tighter timing with the picture taken as well as reduced power
 *             by asserting the trigger signal for only when needed.
 * .max_amp_torch = Is the maximum torch value allowed.  The value is 0 to
 *                  _MAX_TORCH_LEVEL.  This is to allow a limit to the amount
 *                  of amps used.  If left blank then _MAX_TORCH_LEVEL will be
 *                  used.
 * .max_amp_flash = Is the maximum flash value allowed.  The value is 0 to
 *                  _MAX_FLASH_LEVEL.  This is to allow a limit to the amount
 *                  of amps used.  If left blank then _MAX_FLASH_LEVEL will be
 *                  used.
 *
 * The following is specific to only this NVC kernel flash/torch driver:
 * .gpio_act = Is the GPIO needed to control the ACT signal.  If tied high,
 *             then this can be left blank.
 *
 * Power Requirements
 * The board power file must contain the following labels for the power
 * regulator(s) of this device:
 * "vdd_i2c" = the power regulator for the I2C power.
 * Note that this device is typically connected directly to the battery rail
 * and does not need a source power regulator (vdd).
 *
 * The above values should be all that is needed to use the device with this
 * driver.  Modifications of this driver should not be needed.
 */


#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>

#include <media/nvc.h>
#include <media/ssl3250a.h>

#define SSL3250A_REG_AMP	0x00
#define SSL3250A_REG_TMR	0x01
#define SSL3250A_REG_STRB	0x02
#define SSL3250A_REG_STS	0x03
#define ssl3250a_flash_cap_size	(sizeof(ssl3250a_flash_cap.numberoflevels) \
				+ (sizeof(ssl3250a_flash_cap.levels[0]) \
				* (SSL3250A_MAX_FLASH_LEVEL + 1)))
#define ssl3250a_torch_cap_size	(sizeof(ssl3250a_torch_cap.numberoflevels) \
				+ (sizeof(ssl3250a_torch_cap.guidenum[0]) \
				* (SSL3250A_MAX_TORCH_LEVEL + 1)))


static struct nvc_torch_flash_capabilities ssl3250a_flash_cap = {
	SSL3250A_MAX_FLASH_LEVEL + 1,
	{
		{ 0,   0xFFFFFFFF, 0 },
		{ 215, 820,        20 },
		{ 230, 820,        20 },
		{ 245, 820,        20 },
		{ 260, 820,        20 },
		{ 275, 820,        20 },
		{ 290, 820,        20 },
		{ 305, 820,        20 },
		{ 320, 820,        20 },
		{ 335, 820,        20 },
		{ 350, 820,        20 },
		{ 365, 820,        20 },
		{ 380, 820,        20 },
		{ 395, 820,        20 },
		{ 410, 820,        20 },
		{ 425, 820,        20 },
		{ 440, 820,        20 },
		{ 455, 820,        20 },
		{ 470, 820,        20 },
		{ 485, 820,        20 },
		{ 500, 820,        20 }
	}
};

static struct nvc_torch_torch_capabilities ssl3250a_torch_cap = {
	SSL3250A_MAX_TORCH_LEVEL + 1,
	{
		0,
		50,
		65,
		80,
		95,
		110,
		125,
		140,
		155,
		170,
		185,
		200
	}
};

struct ssl3250a_info {
	atomic_t in_use;
	struct i2c_client *i2c_client;
	struct ssl3250a_platform_data *pdata;
	struct miscdevice miscdev;
	struct list_head list;
	int pwr_api;
	int pwr_dev;
	struct nvc_regulator vreg_i2c;
	u8 s_mode;
	struct ssl3250a_info *s_info;
	char devname[16];
};

static struct nvc_torch_pin_state ssl3250a_default_pinstate = {
	.mask		= 0x0000,
	.values		= 0x0000,
};

static struct ssl3250a_platform_data ssl3250a_default_pdata = {
	.cfg		= 0,
	.num		= 0,
	.sync		= 0,
	.dev_name	= "torch",
	.pinstate	= &ssl3250a_default_pinstate,
	.max_amp_torch	= SSL3250A_MAX_TORCH_LEVEL,
	.max_amp_flash	= SSL3250A_MAX_FLASH_LEVEL,
};

static LIST_HEAD(ssl3250a_info_list);
static DEFINE_SPINLOCK(ssl3250a_spinlock);


static int ssl3250a_i2c_rd(struct ssl3250a_info *info, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[2];

	buf[0] = reg;
	msg[0].addr = info->i2c_client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &buf[0];
	msg[1].addr = info->i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &buf[1];
	*val = 0;
	if (i2c_transfer(info->i2c_client->adapter, msg, 2) != 2)
		return -EIO;

	*val = buf[1];
	return 0;
}

static int ssl3250a_i2c_wr(struct ssl3250a_info *info, u8 reg, u8 val)
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

static void ssl3250a_gpio_act(struct ssl3250a_info *info, int val)
{
	int prev_val;

	if (info->pdata->gpio_act) {
		prev_val = gpio_get_value(info->pdata->gpio_act);
		if (val != prev_val) {
			gpio_set_value(info->pdata->gpio_act, val);
			if (val)
				mdelay(1); /* delay for device startup */
		}
	}
}

static void ssl3250a_pm_regulator_put(struct nvc_regulator *sreg)
{
	regulator_put(sreg->vreg);
	sreg->vreg = NULL;
}

static int ssl3250a_pm_regulator_get(struct ssl3250a_info *info,
				     struct nvc_regulator *sreg,
				     char vreg_name[])
{
	int err = 0;

	sreg->vreg_flag = 0;
	sreg->vreg = regulator_get(&info->i2c_client->dev, vreg_name);
	if (WARN_ON(IS_ERR(sreg->vreg))) {
		dev_err(&info->i2c_client->dev,
			"%s err for regulator: %s err: %d\n",
			__func__, vreg_name, (int)sreg->vreg);
		err = PTR_ERR(sreg->vreg);
		sreg->vreg = NULL;
	} else {
		sreg->vreg_name = vreg_name;
		dev_dbg(&info->i2c_client->dev,
				"%s vreg_name: %s\n",
				__func__, sreg->vreg_name);
	}
	return err;
}

static int ssl3250a_pm_regulator_en(struct ssl3250a_info *info,
				    struct nvc_regulator *sreg)
{
	int err = 0;

	if (!sreg->vreg_flag && (sreg->vreg != NULL)) {
		err = regulator_enable(sreg->vreg);
		if (!err) {
			dev_dbg(&info->i2c_client->dev,
					"%s vreg_name: %s\n",
					__func__, sreg->vreg_name);
			sreg->vreg_flag = 1;
			err = 1; /* flag regulator state change */
		} else {
			dev_err(&info->i2c_client->dev,
					"%s err, regulator: %s\n",
					__func__, sreg->vreg_name);
		}
	}
	return err;
}

static int ssl3250a_pm_regulator_dis(struct ssl3250a_info *info,
				     struct nvc_regulator *sreg)
{
	int err = 0;

	if (sreg->vreg_flag && (sreg->vreg != NULL)) {
		err = regulator_disable(sreg->vreg);
		if (!err)
			dev_dbg(&info->i2c_client->dev,
					"%s vreg_name: %s\n",
					__func__, sreg->vreg_name);
		else
			dev_err(&info->i2c_client->dev,
					"%s err, regulator: %s\n",
					__func__, sreg->vreg_name);
	}
	sreg->vreg_flag = 0;
	return err;
}

static int ssl3250a_pm_wr(struct ssl3250a_info *info, int pwr)
{
	int err = 0;

	if (pwr == info->pwr_dev)
		return 0;

	switch (pwr) {
	case NVC_PWR_OFF:
		if ((info->pdata->cfg & NVC_CFG_OFF2STDBY) ||
			     (info->pdata->cfg & NVC_CFG_BOOT_INIT)) {
			pwr = NVC_PWR_STDBY;
		} else {
			ssl3250a_gpio_act(info, 0);
			err = ssl3250a_pm_regulator_dis(info, &info->vreg_i2c);
			break;
		}
	case NVC_PWR_STDBY_OFF:
		if ((info->pdata->cfg & NVC_CFG_OFF2STDBY) ||
			     (info->pdata->cfg & NVC_CFG_BOOT_INIT)) {
			pwr = NVC_PWR_STDBY;
		} else {
			ssl3250a_gpio_act(info, 0);
			err = ssl3250a_pm_regulator_en(info, &info->vreg_i2c);
			break;
		}
	case NVC_PWR_STDBY:
		err = ssl3250a_pm_regulator_en(info, &info->vreg_i2c);
		ssl3250a_gpio_act(info, 1);
		err |= ssl3250a_i2c_wr(info, SSL3250A_REG_AMP, 0x00);
		break;

	case NVC_PWR_COMM:
	case NVC_PWR_ON:
		err = ssl3250a_pm_regulator_en(info, &info->vreg_i2c);
		ssl3250a_gpio_act(info, 1);
		break;

	default:
		err = -EINVAL;
		break;
	}

	if (err < 0) {
		dev_err(&info->i2c_client->dev, "%s error\n", __func__);
		pwr = NVC_PWR_ERR;
	}
	info->pwr_dev = pwr;
	if (err > 0)
		return 0;

	return err;
}

static int ssl3250a_pm_wr_s(struct ssl3250a_info *info, int pwr)
{
	int err1 = 0;
	int err2 = 0;

	if ((info->s_mode == NVC_SYNC_OFF) ||
			(info->s_mode == NVC_SYNC_MASTER) ||
			(info->s_mode == NVC_SYNC_STEREO))
		err1 = ssl3250a_pm_wr(info, pwr);
	if ((info->s_mode == NVC_SYNC_SLAVE) ||
			(info->s_mode == NVC_SYNC_STEREO))
		err2 = ssl3250a_pm_wr(info->s_info, pwr);
	return err1 | err2;
}

static int ssl3250a_pm_api_wr(struct ssl3250a_info *info, int pwr)
{
	int err = 0;

	if (!pwr || (pwr > NVC_PWR_ON))
		return 0;

	if (pwr > info->pwr_dev)
		err = ssl3250a_pm_wr_s(info, pwr);
	if (!err)
		info->pwr_api = pwr;
	else
		info->pwr_api = NVC_PWR_ERR;
	if (info->pdata->cfg & NVC_CFG_NOERR)
		return 0;

	return err;
}

static int ssl3250a_pm_dev_wr(struct ssl3250a_info *info, int pwr)
{
	if (pwr < info->pwr_api)
		pwr = info->pwr_api;
	return ssl3250a_pm_wr(info, pwr);
}

static void ssl3250a_pm_exit(struct ssl3250a_info *info)
{
	ssl3250a_pm_wr_s(info, NVC_PWR_OFF);
	ssl3250a_pm_regulator_put(&info->vreg_i2c);
}

static void ssl3250a_pm_init(struct ssl3250a_info *info)
{
	ssl3250a_pm_regulator_get(info, &info->vreg_i2c, "vdd_i2c");
}

static int ssl3250a_dev_id(struct ssl3250a_info *info)
{
	u8 addr;
	u8 reg;
	int err;

	ssl3250a_pm_dev_wr(info, NVC_PWR_COMM);
	/* There isn't a device ID so we just check that all the registers
	 * equal their startup defaults, which in this case, is 0.
	 */
	for (addr = 0; addr < SSL3250A_REG_STS; addr++) {
		err = ssl3250a_i2c_rd(info, addr, &reg);
		if (err) {
			break;
		} else {
			if (reg) {
				err = -ENODEV;
				break;
			}
		}
	}
	ssl3250a_pm_dev_wr(info, NVC_PWR_OFF);
	return err;
}

static int ssl3250a_param_rd(struct ssl3250a_info *info, long arg)
{
	struct nvc_param params;
	struct nvc_torch_pin_state pinstate;
	const void *data_ptr;
	u32 data_size = 0;
	int err;
	u8 reg;

	if (copy_from_user(&params,
			(const void __user *)arg,
			sizeof(struct nvc_param))) {
		dev_err(&info->i2c_client->dev, "%s %d copy_from_user err\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	if (info->s_mode == NVC_SYNC_SLAVE)
		info = info->s_info;
	switch (params.param) {
	case NVC_PARAM_FLASH_CAPS:
		dev_dbg(&info->i2c_client->dev, "%s FLASH_CAPS\n", __func__);
		data_ptr = &ssl3250a_flash_cap;
		data_size = ssl3250a_flash_cap_size;
		break;

	case NVC_PARAM_FLASH_LEVEL:
		ssl3250a_pm_dev_wr(info, NVC_PWR_COMM);
		err = ssl3250a_i2c_rd(info, SSL3250A_REG_AMP, &reg);
		ssl3250a_pm_dev_wr(info, NVC_PWR_OFF);
		if (err < 0)
			return err;

		reg >>= 3; /*7:3=flash amps*/
		reg &= 0x1F; /*4:0=flash amps*/
		if (reg < 12) /*flash starts at 12*/
			reg = 0; /*<12=torch or off*/
		else
			reg -= 11; /*create flash index*/
		dev_dbg(&info->i2c_client->dev, "%s FLASH_LEVEL: %u\n",
			    __func__,
			    (unsigned)ssl3250a_flash_cap.levels[reg].guidenum);
		data_ptr = &ssl3250a_flash_cap.levels[reg].guidenum;
		data_size = sizeof(ssl3250a_flash_cap.levels[reg].guidenum);
		break;

	case NVC_PARAM_TORCH_CAPS:
		dev_dbg(&info->i2c_client->dev, "%s TORCH_CAPS\n", __func__);
		data_ptr = &ssl3250a_torch_cap;
		data_size = ssl3250a_torch_cap_size;
		break;

	case NVC_PARAM_TORCH_LEVEL:
		ssl3250a_pm_dev_wr(info, NVC_PWR_COMM);
		err = ssl3250a_i2c_rd(info, SSL3250A_REG_AMP, &reg);
		ssl3250a_pm_dev_wr(info, NVC_PWR_OFF);
		if (err < 0)
			return err;

		reg >>= 3; /*7:3=torch amps*/
		reg &= 0x1F; /*4:0=torch amps*/
		if (reg > 11) /*flash starts at 12*/
			reg = 0; /*>11=flash mode (torch off)*/
		dev_dbg(&info->i2c_client->dev, "%s TORCH_LEVEL: %u\n",
				__func__,
				(unsigned)ssl3250a_torch_cap.guidenum[reg]);
		data_ptr = &ssl3250a_torch_cap.guidenum[reg];
		data_size = sizeof(ssl3250a_torch_cap.guidenum[reg]);
		break;

	case NVC_PARAM_FLASH_PIN_STATE:
		pinstate.mask = info->pdata->pinstate->mask;
		ssl3250a_pm_dev_wr(info, NVC_PWR_COMM);
		err = ssl3250a_i2c_rd(info, SSL3250A_REG_AMP, &reg);
		ssl3250a_pm_dev_wr(info, NVC_PWR_OFF);
		if (err < 0)
			return err;

		reg >>= 3; /*7:3=flash amps*/
		reg &= 0x1F; /*4:0=flash amps*/
		if (reg < 12) /*flash starts at 12*/
			pinstate.values = 0; /*deassert strobe*/
		else
			/*assert strobe*/
			pinstate.values = info->pdata->pinstate->values;
		dev_dbg(&info->i2c_client->dev, "%s FLASH_PIN_STATE: %x&%x\n",
				__func__, pinstate.mask, pinstate.values);
		data_ptr = &pinstate;
		data_size = sizeof(struct nvc_torch_pin_state);
		break;

	case NVC_PARAM_STEREO:
		dev_dbg(&info->i2c_client->dev, "%s STEREO: %d\n",
				__func__, info->s_mode);
		data_ptr = &info->s_mode;
		data_size = sizeof(info->s_mode);
		break;

	default:
		dev_err(&info->i2c_client->dev,
				"%s unsupported parameter: %d\n",
				__func__, params.param);
		return -EINVAL;
	}

	if (params.sizeofvalue < data_size) {
		dev_err(&info->i2c_client->dev,
				"%s data size mismatch %d != %d\n",
				__func__, params.sizeofvalue, data_size);
		return -EINVAL;
	}

	if (copy_to_user((void __user *)params.p_value,
			 data_ptr,
			 data_size)) {
		dev_err(&info->i2c_client->dev,
				"%s copy_to_user err line %d\n",
				__func__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

static int ssl3250a_param_wr_s(struct ssl3250a_info *info,
			       struct nvc_param *params,
			       u8 val)
{
	int err;

	switch (params->param) {
	case NVC_PARAM_FLASH_LEVEL:
		dev_dbg(&info->i2c_client->dev, "%s FLASH_LEVEL: %d\n",
				__func__, val);
		ssl3250a_pm_dev_wr(info, NVC_PWR_ON);
		if (val > ssl3250a_default_pdata.max_amp_flash)
			val = ssl3250a_default_pdata.max_amp_flash;
		/*Amp limit values are in the board-sensors file.*/
		if (info->pdata->max_amp_flash &&
				(val > info->pdata->max_amp_flash))
			val = info->pdata->max_amp_flash;
		if (val) {
			val += 11; /*flash starts at 12*/
			val <<= 3; /*7:3=flash/torch amps*/
		}
		err = ssl3250a_i2c_wr(info, SSL3250A_REG_AMP, val);
		if (!val) /*turn pwr off if no flash && no pwr_api*/
			ssl3250a_pm_dev_wr(info, NVC_PWR_OFF);
		return err;

	case NVC_PARAM_TORCH_LEVEL:
		dev_dbg(&info->i2c_client->dev, "%s TORCH_LEVEL: %d\n",
				__func__, val);
		ssl3250a_pm_dev_wr(info, NVC_PWR_ON);
		if (val > ssl3250a_default_pdata.max_amp_torch)
			val = ssl3250a_default_pdata.max_amp_torch;
		/*Amp limit values are in the board-sensors file.*/
		if (info->pdata->max_amp_torch &&
				(val > info->pdata->max_amp_torch))
			val = info->pdata->max_amp_torch;
		if (val)
			val <<= 3; /*7:3=flash/torch amps*/
		err = ssl3250a_i2c_wr(info, SSL3250A_REG_AMP, val);
		if (!val) /*turn pwr off if no torch && no pwr_api*/
			ssl3250a_pm_dev_wr(info, NVC_PWR_OFF);
		return err;

	case NVC_PARAM_FLASH_PIN_STATE:
		dev_dbg(&info->i2c_client->dev, "%s FLASH_PIN_STATE: %d\n",
				__func__, val);
		if (val)
			val = 0x01; /*0:0=soft trigger*/
		return ssl3250a_i2c_wr(info, SSL3250A_REG_STRB, val);

	default:
		dev_err(&info->i2c_client->dev,
				"%s unsupported parameter: %d\n",
				__func__, params->param);
		return -EINVAL;
	}
}

static int ssl3250a_param_wr(struct ssl3250a_info *info, long arg)
{
	struct nvc_param params;
	u8 val;
	int err = 0;

	if (copy_from_user(&params,
				(const void __user *)arg,
				sizeof(struct nvc_param))) {
		dev_err(&info->i2c_client->dev, "%s %d copy_from_user err\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	if (copy_from_user(&val, (const void __user *)params.p_value,
			   sizeof(val))) {
		dev_err(&info->i2c_client->dev, "%s %d copy_from_user err\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	/* parameters independent of sync mode */
	switch (params.param) {
	case NVC_PARAM_STEREO:
		dev_dbg(&info->i2c_client->dev, "%s STEREO: %d\n",
				__func__, (int)val);
		if (val == info->s_mode)
			return 0;

		switch (val) {
		case NVC_SYNC_OFF:
			info->s_mode = val;
			if (info->s_info != NULL) {
				info->s_info->s_mode = val;
				ssl3250a_pm_wr(info->s_info, NVC_PWR_OFF);
			}
			break;

		case NVC_SYNC_MASTER:
			info->s_mode = val;
			if (info->s_info != NULL)
				info->s_info->s_mode = val;
			break;

		case NVC_SYNC_SLAVE:
		case NVC_SYNC_STEREO:
			if (info->s_info != NULL) {
				/* sync power */
				info->s_info->pwr_api = info->pwr_api;
				err = ssl3250a_pm_wr(info->s_info,
						     info->pwr_dev);
				if (!err) {
					info->s_mode = val;
					info->s_info->s_mode = val;
				} else {
					ssl3250a_pm_wr(info->s_info,
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

	default:
	/* parameters dependent on sync mode */
		switch (info->s_mode) {
		case NVC_SYNC_OFF:
		case NVC_SYNC_MASTER:
			return ssl3250a_param_wr_s(info, &params, val);

		case NVC_SYNC_SLAVE:
			return ssl3250a_param_wr_s(info->s_info,
						 &params,
						 val);

		case NVC_SYNC_STEREO:
			err = ssl3250a_param_wr_s(info, &params, val);
			if (!(info->pdata->cfg & NVC_CFG_SYNC_I2C_MUX))
				err |= ssl3250a_param_wr_s(info->s_info,
							 &params,
							 val);
			return err;

		default:
			dev_err(&info->i2c_client->dev, "%s %d internal err\n",
					__func__, __LINE__);
			return -EINVAL;
		}
	}
}

static long ssl3250a_ioctl(struct file *file,
			   unsigned int cmd,
			   unsigned long arg)
{
	struct ssl3250a_info *info = file->private_data;
	int pwr;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(NVC_IOCTL_PARAM_WR):
		return ssl3250a_param_wr(info, arg);

	case _IOC_NR(NVC_IOCTL_PARAM_RD):
		return ssl3250a_param_rd(info, arg);

	case _IOC_NR(NVC_IOCTL_PWR_WR):
		/* This is a Guaranteed Level of Service (GLOS) call */
		pwr = (int)arg * 2;
		dev_dbg(&info->i2c_client->dev, "%s PWR_WR: %d\n",
				__func__, pwr);
		return ssl3250a_pm_api_wr(info, pwr);

	case _IOC_NR(NVC_IOCTL_PWR_RD):
		if (info->s_mode == NVC_SYNC_SLAVE)
			pwr = info->s_info->pwr_api / 2;
		else
			pwr = info->pwr_api / 2;
		dev_dbg(&info->i2c_client->dev, "%s PWR_RD: %d\n",
				__func__, pwr);
		if (copy_to_user((void __user *)arg, (const void *)&pwr,
				 sizeof(pwr))) {
			dev_err(&info->i2c_client->dev,
					"%s copy_to_user err line %d\n",
					__func__, __LINE__);
			return -EFAULT;
		}

		return 0;

	default:
		dev_err(&info->i2c_client->dev, "%s unsupported ioctl: %x\n",
				__func__, cmd);
		return -EINVAL;
	}
}

static int ssl3250a_sync_en(int dev1, int dev2)
{
	struct ssl3250a_info *sync1 = NULL;
	struct ssl3250a_info *sync2 = NULL;
	struct ssl3250a_info *pos = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(pos, &ssl3250a_info_list, list) {
		if (pos->pdata->num == dev1) {
			sync1 = pos;
			break;
		}
	}
	pos = NULL;
	list_for_each_entry_rcu(pos, &ssl3250a_info_list, list) {
		if (pos->pdata->num == dev2) {
			sync2 = pos;
			break;
		}
	}
	rcu_read_unlock();
	if (sync1 != NULL)
		sync1->s_info = NULL;
	if (sync2 != NULL)
		sync2->s_info = NULL;
	if (!dev1 && !dev2)
		return 0; /* no err if default instance 0's used */

	if (dev1 == dev2)
		return -EINVAL; /* err if sync instance is itself */

	if ((sync1 != NULL) && (sync2 != NULL)) {
		sync1->s_info = sync2;
		sync2->s_info = sync1;
	}

	return 0;
}

static int ssl3250a_sync_dis(struct ssl3250a_info *info)
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

static int ssl3250a_open(struct inode *inode, struct file *file)
{
	struct ssl3250a_info *info = NULL;
	struct ssl3250a_info *pos = NULL;
	int err;

	rcu_read_lock();
	list_for_each_entry_rcu(pos, &ssl3250a_info_list, list) {
		if (pos->miscdev.minor == iminor(inode)) {
			info = pos;
			break;
		}
	}
	rcu_read_unlock();
	if (!info)
		return -ENODEV;

	err = ssl3250a_sync_en(info->pdata->num, info->pdata->sync);
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
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	return 0;
}

static int ssl3250a_release(struct inode *inode, struct file *file)
{
	struct ssl3250a_info *info = file->private_data;

	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	ssl3250a_pm_wr_s(info, NVC_PWR_OFF);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	if (info->s_info != NULL)
		WARN_ON(!atomic_xchg(&info->s_info->in_use, 0));
	ssl3250a_sync_dis(info);
	return 0;
}

static const struct file_operations ssl3250a_fileops = {
	.owner = THIS_MODULE,
	.open = ssl3250a_open,
	.unlocked_ioctl = ssl3250a_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ssl3250a_ioctl,
#endif
	.release = ssl3250a_release,
};

static void ssl3250a_del(struct ssl3250a_info *info)
{
	ssl3250a_pm_exit(info);
	ssl3250a_sync_dis(info);
	spin_lock(&ssl3250a_spinlock);
	list_del_rcu(&info->list);
	spin_unlock(&ssl3250a_spinlock);
	synchronize_rcu();
}

static int ssl3250a_remove(struct i2c_client *client)
{
	struct ssl3250a_info *info = i2c_get_clientdata(client);

	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	misc_deregister(&info->miscdev);
	ssl3250a_del(info);
	return 0;
}

static int ssl3250a_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct ssl3250a_info *info;
	int err;

	dev_dbg(&client->dev, "%s\n", __func__);
	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	info->i2c_client = client;
	if (client->dev.platform_data) {
		info->pdata = client->dev.platform_data;
	} else {
		info->pdata = &ssl3250a_default_pdata;
		dev_dbg(&client->dev,
				"%s No platform data.  Using defaults.\n",
				__func__);
	}
	i2c_set_clientdata(client, info);
	INIT_LIST_HEAD(&info->list);
	spin_lock(&ssl3250a_spinlock);
	list_add_rcu(&info->list, &ssl3250a_info_list);
	spin_unlock(&ssl3250a_spinlock);
	ssl3250a_pm_init(info);
	err = ssl3250a_dev_id(info);
	if (err < 0) {
		dev_err(&client->dev, "%s device not found\n", __func__);
		if (info->pdata->cfg & NVC_CFG_NODEV) {
			ssl3250a_del(info);
			return -ENODEV;
		}
	} else {
		dev_dbg(&client->dev, "%s device found\n", __func__);
	}

	if (info->pdata->dev_name != 0)
		strncpy(info->devname, info->pdata->dev_name,
			sizeof(info->devname) - 1);
	else
		strncpy(info->devname, "ssl3250a", sizeof(info->devname) - 1);

	if (info->pdata->num)
		snprintf(info->devname, sizeof(info->devname), "%s.%u",
			 info->devname, info->pdata->num);

	info->miscdev.name = info->devname;
	info->miscdev.fops = &ssl3250a_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&info->miscdev)) {
		dev_err(&client->dev, "%s unable to register misc device %s\n",
				__func__, info->devname);
		ssl3250a_del(info);
		return -ENODEV;
	}

	return 0;
}

static const struct i2c_device_id ssl3250a_id[] = {
	{ "ssl3250a", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, ssl3250a_id);

static struct i2c_driver ssl3250a_i2c_driver = {
	.driver = {
		.name = "ssl3250a",
		.owner = THIS_MODULE,
	},
	.id_table = ssl3250a_id,
	.probe = ssl3250a_probe,
	.remove = ssl3250a_remove,
};

static int __init ssl3250a_init(void)
{
	return i2c_add_driver(&ssl3250a_i2c_driver);
}

static void __exit ssl3250a_exit(void)
{
	i2c_del_driver(&ssl3250a_i2c_driver);
}

module_init(ssl3250a_init);
module_exit(ssl3250a_exit);
MODULE_LICENSE("GPL");
