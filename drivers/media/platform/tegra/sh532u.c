/*
 * SH532U focuser driver.
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
 * file with the <device>_platform_data structure.
 * Standard among NVC kernel drivers in this structure is:
 * .cfg = Use the NVC_CFG_ defines that are in nvc.h.
 *        Descriptions of the configuration options are with the defines.
 *        This value is typically 0.
 * .num = The number of the instance of the device.  This should start at 1 and
 *        and increment for each device on the board.  This number will be
 *        appended to the MISC driver name, Example: /dev/focuser.1
 *        If not used or 0, then nothing is appended to the name.
 * .sync = If there is a need to synchronize two devices, then this value is
 *         the number of the device instance (.num above) this device is to
 *         sync to.  For example:
 *         Device 1 platform entries =
 *         .num = 1,
 *         .sync = 2,
 *         Device 2 platfrom entries =
 *         .num = 2,
 *         .sync = 1,
 *         The above example sync's device 1 and 2.
 *         To disable sync, set .sync = 0.  Note that the .num = 0 device is
 *         not allowed to be synced to.
 *         This is typically used for stereo applications.
 * .dev_name = The MISC driver name the device registers as.  If not used,
 *             then the part number of the device is used for the driver name.
 *             If using the NVC user driver then use the name found in this
 *             driver under _default_pdata.
 * .gpio_count = The ARRAY_SIZE of the nvc_gpio_pdata table.
 * .gpio = A pointer to the nvc_gpio_pdata structure's platform GPIO data.
 *         The GPIO mechanism works by cross referencing the .gpio_type key
 *         among the nvc_gpio_pdata GPIO data and the driver's nvc_gpio_init
 *         GPIO data to build a GPIO table the driver can use.  The GPIO's
 *         defined in the device header file's _gpio_type enum are the
 *         gpio_type keys for the nvc_gpio_pdata and nvc_gpio_init structures.
 *         These need to be present in the board file's nvc_gpio_pdata
 *         structure for the GPIO's that are used.
 *         The driver's GPIO logic uses assert/deassert throughout until the
 *         low level _gpio_wr/rd calls where the .assert_high is used to
 *         convert the value to the correct signal level.
 *         See the GPIO notes in nvc.h for additional information.
 *
 * The following is specific to NVC kernel focus drivers:
 * .nvc = Pointer to the nvc_focus_nvc structure.  This structure needs to
 *        be defined and populated if overriding the driver defaults.
 * .cap = Pointer to the nvc_focus_cap structure.  This structure needs to
 *        be defined and populated if overriding the driver defaults.
 *
 * The following is specific to this NVC kernel focus driver:
 * .info = Pointer to the sh532u_pdata_info structure.  This structure does
 *         not need to be defined and populated unless overriding ROM data.
.* .i2c_addr_rom = The I2C address of the onboard ROM.
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
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/gpio.h>
#include <linux/module.h>

#include <media/nvc.h>
#include <media/sh532u.h>

#define SH532U_ID		0x0532
#define SH532U_STARTUP_DELAY_MS	10
/* defaults if no ROM data */
#define SH532U_HYPERFOCAL_RATIO 1836 /* 41.2f/224.4f Ratio source: SEMCO */
/* _HYPERFOCAL_RATIO is multiplied and _HYPERFOCAL_DIV divides for float */
#define SH532U_HYPERFOCAL_DIV	10000
#define SH532U_FOCAL_LENGTH	0x408D70A4
#define SH532U_FNUMBER		0x40333333
#define SH532U_MAX_APERATURE	0x3FCA0EA1
#define SH532U_ACTUATOR_RANGE	1000
#define SH532U_SETTLETIME	30
#define SH532U_FOCUS_MACRO	950
#define SH532U_FOCUS_HYPER	250
#define SH532U_FOCUS_INFINITY	50
#define SH532U_TIMEOUT_MS	200
#define SH532U_POS_LOW_DEFAULT	0xA000
#define SH532U_POS_HIGH_DEFAULT	0x6000
#define SH532U_POS_PHYSICAL_MIN	(~0x7FFF)
#define SH532U_POS_PHYSICAL_MAX	(0x7FFF)
#define SH532U_SLEW_RATE			1
#define SH532U_POS_TRANSLATE		0
#define SH532U_POS_SIGN_CHANGER		(-1)

static u8 sh532u_ids[] = {
	0xF0,
};

static struct nvc_gpio_init sh532u_gpios[] = {
	{ SH532U_GPIO_RESET, GPIOF_OUT_INIT_LOW, "reset", false, true, },
	{ SH532U_GPIO_I2CMUX, 0, "i2c_mux", 0, false, },
	{ SH532U_GPIO_GP1, 0, "gp1", 0, false, },
	{ SH532U_GPIO_GP2, 0, "gp2", 0, false, },
	{ SH532U_GPIO_GP3, 0, "gp3", 0, false, },
};

static struct nvc_regulator_init sh532u_vregs[] = {
	{ SH532U_VREG_AVDD, "avdd", },
	{ SH532U_VREG_DVDD, "dvdd", },
};

struct sh532u_info {
	atomic_t in_use;
	struct i2c_client *i2c_client;
	struct sh532u_platform_data *pdata;
	struct miscdevice miscdev;
	struct list_head list;
	struct nvc_gpio gpio[ARRAY_SIZE(sh532u_gpios)];
	struct nvc_regulator vreg[ARRAY_SIZE(sh532u_vregs)];
	int pwr_api;
	int pwr_dev;
	u8 s_mode;
	struct sh532u_info *s_info;
	u8 id_minor;
	unsigned i2c_addr_rom;
	struct nvc_focus_nvc nvc;
	struct nvc_focus_cap cap;
	struct nv_focuser_config config;
	enum nvc_focus_sts sts;
	struct sh532u_pdata_info cfg;
	bool reset_flag;
	bool init_cal_flag;
	s16 abs_base;
	u32 abs_range;
	u32 pos_rel;
	s16 pos_abs;
	long pos_time_wr;
	char devname[16];
};

static struct sh532u_pdata_info sh532u_default_info = {
	.move_timeoutms	= SH532U_TIMEOUT_MS,
	.focus_hyper_ratio = SH532U_HYPERFOCAL_RATIO,
	.focus_hyper_div = SH532U_HYPERFOCAL_DIV,
};

static struct nvc_focus_cap sh532u_default_cap = {
	.version	= NVC_FOCUS_CAP_VER2,
	.actuator_range = SH532U_ACTUATOR_RANGE,
	.settle_time	= SH532U_SETTLETIME,
	.focus_macro	= SH532U_FOCUS_MACRO,
	.focus_hyper	= SH532U_FOCUS_HYPER,
	.focus_infinity	= SH532U_FOCUS_INFINITY,
	.slew_rate	  = SH532U_SLEW_RATE,
	.position_translate = SH532U_POS_TRANSLATE,
};

static struct nvc_focus_nvc sh532u_default_nvc = {
	.focal_length	= SH532U_FOCAL_LENGTH,
	.fnumber	= SH532U_FNUMBER,
	.max_aperature	= SH532U_MAX_APERATURE,
};

static struct sh532u_platform_data sh532u_default_pdata = {
	.cfg		= 0,
	.num		= 0,
	.sync		= 0,
	.dev_name	= "focuser",
	.i2c_addr_rom	= 0x50,
};

static u32 sh532u_a2buf[] = {
	0x0018019c,
	0x0018019d,
	0x0000019e,
	0x007f0192,
	0x00000194,
	0x00f00184,
	0x00850187,
	0x0000018a,
	0x00fd7187,
	0x007f7183,
	0x0008025a,
	0x05042218,
	0x80010216,
	0x000601a0,
	0x00808183,
	0xffffffff
};

static LIST_HEAD(sh532u_info_list);
static DEFINE_SPINLOCK(sh532u_spinlock);


static int sh532u_i2c_rd8(struct sh532u_info *info, u8 addr, u8 reg, u8 *val)
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

static int sh532u_i2c_wr8(struct sh532u_info *info, u8 reg, u8 val)
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

static int sh532u_i2c_rd16(struct sh532u_info *info, u8 reg, u16 *val)
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


static int sh532u_i2c_wr16(struct sh532u_info *info, u8 reg, u16 val)
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

static int sh532u_i2c_rd32(struct sh532u_info *info, u8 addr, u8 reg, u32 *val)
{
	struct i2c_msg msg[2];
	u8 buf[5];

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
	msg[1].len = 4;
	msg[1].buf = &buf[1];
	if (i2c_transfer(info->i2c_client->adapter, msg, 2) != 2)
		return -EIO;

	*val = (((u32)buf[4] << 24) | ((u32)buf[3] << 16) |
			((u32)buf[2] << 8) | ((u32)buf[1]));
	return 0;
}

static int sh532u_gpio_wr(struct sh532u_info *info,
			  enum sh532u_gpio i,
			  int val) /* val: 0=deassert, 1=assert */
{
	int err = -EINVAL;

	if (info->gpio[i].flag) {
		if (val)
			val = 1;
		if (!info->gpio[i].active_high)
			val = !val;
		val &= 1;
		err = val;
		gpio_set_value_cansleep(info->gpio[i].gpio, val);
		dev_dbg(&info->i2c_client->dev, "%s %u %d\n",
			__func__, info->gpio[i].gpio, val);
	}
	return err; /* return value written or error */
}

static int sh532u_gpio_reset(struct sh532u_info *info, int val)
{
	int err = 0;

	if (val) {
		if (!info->reset_flag) {
			info->reset_flag = true;
			err = sh532u_gpio_wr(info, SH532U_GPIO_RESET, 1);
			if (err < 0)
				return 0; /* flag no reset */

			mdelay(1);
			sh532u_gpio_wr(info, SH532U_GPIO_RESET, 0);
			mdelay(SH532U_STARTUP_DELAY_MS); /* startup delay */
			err = 1; /* flag that a reset was done */
		}
	} else {
		info->reset_flag = false;
	}
	return err;
}

static void sh532u_gpio_able(struct sh532u_info *info, int val)
{
	if (val) {
		sh532u_gpio_wr(info, SH532U_GPIO_GP1, val);
		sh532u_gpio_wr(info, SH532U_GPIO_GP2, val);
		sh532u_gpio_wr(info, SH532U_GPIO_GP3, val);
	} else {
		sh532u_gpio_wr(info, SH532U_GPIO_GP3, val);
		sh532u_gpio_wr(info, SH532U_GPIO_GP2, val);
		sh532u_gpio_wr(info, SH532U_GPIO_GP1, val);
	}
}

static void sh532u_gpio_exit(struct sh532u_info *info)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(sh532u_gpios); i++) {
		if (info->gpio[i].flag && info->gpio[i].own) {
			gpio_free(info->gpio[i].gpio);
			info->gpio[i].own = false;
		}
	}
}

static void sh532u_gpio_init(struct sh532u_info *info)
{
	char label[32];
	unsigned long flags;
	unsigned type;
	unsigned i;
	unsigned j;
	int err;

	for (i = 0; i < ARRAY_SIZE(sh532u_gpios); i++)
		info->gpio[i].flag = false;
	if (!info->pdata->gpio_count || !info->pdata->gpio)
		return;

	for (i = 0; i < ARRAY_SIZE(sh532u_gpios); i++) {
		type = sh532u_gpios[i].gpio_type;
		for (j = 0; j < info->pdata->gpio_count; j++) {
			if (type == info->pdata->gpio[j].gpio_type)
				break;
		}
		if (j == info->pdata->gpio_count)
			continue;

		info->gpio[type].gpio = info->pdata->gpio[j].gpio;
		info->gpio[type].flag = true;
		if (sh532u_gpios[i].use_flags) {
			flags = sh532u_gpios[i].flags;
			info->gpio[type].active_high =
						   sh532u_gpios[i].active_high;
		} else {
			info->gpio[type].active_high =
					      info->pdata->gpio[j].active_high;
			if (info->gpio[type].active_high)
				flags = GPIOF_OUT_INIT_LOW;
			else
				flags = GPIOF_OUT_INIT_HIGH;
		}
		if (!info->pdata->gpio[j].init_en)
			continue;

		snprintf(label, sizeof(label), "sh532u_%u_%s",
			 info->pdata->num, sh532u_gpios[i].label);
		err = gpio_request_one(info->gpio[type].gpio, flags, label);
		if (err) {
			dev_err(&info->i2c_client->dev, "%s ERR %s %u\n",
				__func__, label, info->gpio[type].gpio);
		} else {
			info->gpio[type].own = true;
			dev_dbg(&info->i2c_client->dev, "%s %s %u\n",
				__func__, label, info->gpio[type].gpio);
		}
	}
}

static int sh532u_vreg_dis(struct sh532u_info *info,
			   enum sh532u_vreg i)
{
	int err = 0;

	if (info->vreg[i].vreg_flag && (info->vreg[i].vreg != NULL)) {
		err = regulator_disable(info->vreg[i].vreg);
		if (!err)
			dev_dbg(&info->i2c_client->dev, "%s: %s\n",
				__func__, info->vreg[i].vreg_name);
		else
			dev_err(&info->i2c_client->dev, "%s %s ERR\n",
				__func__, info->vreg[i].vreg_name);
	}
	info->vreg[i].vreg_flag = false;
	return err;
}

static int sh532u_vreg_dis_all(struct sh532u_info *info)
{
	unsigned i;
	int err = 0;

	for (i = ARRAY_SIZE(sh532u_vregs); i > 0; i--)
		err |= sh532u_vreg_dis(info, (i - 1));
	return err;
}

static int sh532u_vreg_en(struct sh532u_info *info,
			  enum sh532u_vreg i)
{
	int err = 0;

	if (!info->vreg[i].vreg_flag && (info->vreg[i].vreg != NULL)) {
		err = regulator_enable(info->vreg[i].vreg);
		if (!err) {
			dev_dbg(&info->i2c_client->dev, "%s: %s\n",
				__func__, info->vreg[i].vreg_name);
			info->vreg[i].vreg_flag = true;
			err = 1; /* flag regulator state change */
		} else {
			dev_err(&info->i2c_client->dev, "%s %s ERR\n",
				__func__, info->vreg[i].vreg_name);
		}
	}
	return err;
}

static int sh532u_vreg_en_all(struct sh532u_info *info)
{
	unsigned i;
	int err = 0;

	for (i = 0; i < ARRAY_SIZE(sh532u_vregs); i++)
		err |= sh532u_vreg_en(info, i);
	return err;
}

static void sh532u_vreg_exit(struct sh532u_info *info)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(sh532u_vregs); i++) {
		regulator_put(info->vreg[i].vreg);
		info->vreg[i].vreg = NULL;
	}
}

static int sh532u_vreg_init(struct sh532u_info *info)
{
	unsigned i;
	unsigned j;
	int err = 0;

	for (i = 0; i < ARRAY_SIZE(sh532u_vregs); i++) {
		j = sh532u_vregs[i].vreg_num;
		info->vreg[j].vreg_name = sh532u_vregs[i].vreg_name;
		info->vreg[j].vreg_flag = false;
		info->vreg[j].vreg = regulator_get(&info->i2c_client->dev,
						   info->vreg[j].vreg_name);
		if (IS_ERR(info->vreg[j].vreg)) {
			if (PTR_ERR(info->vreg[j].vreg) != -ENODEV)
				dev_dbg(&info->i2c_client->dev,
					"%s %s ERR: %d\n",
					__func__, info->vreg[j].vreg_name,
					(int)info->vreg[j].vreg);
			else
				dev_info(&info->i2c_client->dev,
					 "%s no regulator found for %s. "
					 "This board may not have an "
					 "independent %s regulator.\n",
					 __func__, info->vreg[j].vreg_name,
					 info->vreg[j].vreg_name);
			err |= PTR_ERR(info->vreg[j].vreg);
			info->vreg[j].vreg = NULL;
		} else {
			dev_dbg(&info->i2c_client->dev, "%s: %s\n",
				__func__, info->vreg[j].vreg_name);
		}
	}
	return err;
}

static int sh532u_pm_wr(struct sh532u_info *info, int pwr)
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
		err = sh532u_vreg_dis_all(info);
		sh532u_gpio_able(info, 0);
		sh532u_gpio_reset(info, 0);
		break;

	case NVC_PWR_STDBY_OFF:
	case NVC_PWR_STDBY:
		err = sh532u_vreg_en_all(info);
		sh532u_gpio_able(info, 1);
		sh532u_gpio_reset(info, 1);
		err |= sh532u_i2c_wr8(info, STBY_211, 0x80);
		err |= sh532u_i2c_wr8(info, CLKSEL_211, 0x38);
		err |= sh532u_i2c_wr8(info, CLKSEL_211, 0x39);
		break;

	case NVC_PWR_COMM:
	case NVC_PWR_ON:
		err = sh532u_vreg_en_all(info);
		sh532u_gpio_able(info, 1);
		sh532u_gpio_reset(info, 1);
		err |= sh532u_i2c_wr8(info, CLKSEL_211, 0x38);
		err |= sh532u_i2c_wr8(info, CLKSEL_211, 0x34);
		err |= sh532u_i2c_wr8(info, STBY_211, 0xF0);
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
	dev_dbg(&info->i2c_client->dev, "%s pwr_dev=%d\n",
		__func__, info->pwr_dev);
	if (err > 0)
		return 0;

	return err;
}

static int sh532u_pm_wr_s(struct sh532u_info *info, int pwr)
{
	int err1 = 0;
	int err2 = 0;

	if ((info->s_mode == NVC_SYNC_OFF) ||
			(info->s_mode == NVC_SYNC_MASTER) ||
			(info->s_mode == NVC_SYNC_STEREO))
		err1 = sh532u_pm_wr(info, pwr);
	if ((info->s_mode == NVC_SYNC_SLAVE) ||
			(info->s_mode == NVC_SYNC_STEREO))
		err2 = sh532u_pm_wr(info->s_info, pwr);
	return err1 | err2;
}

static int sh532u_pm_api_wr(struct sh532u_info *info, int pwr)
{
	int err = 0;

	if (!pwr || (pwr > NVC_PWR_ON))
		return 0;

	if (pwr > info->pwr_dev)
		err = sh532u_pm_wr_s(info, pwr);
	if (!err)
		info->pwr_api = pwr;
	else
		info->pwr_api = NVC_PWR_ERR;
	if (info->pdata->cfg & NVC_CFG_NOERR)
		return 0;

	return err;
}

static int sh532u_pm_dev_wr(struct sh532u_info *info, int pwr)
{
	if (pwr < info->pwr_api)
		pwr = info->pwr_api;
	if (info->sts == NVC_FOCUS_STS_WAIT_FOR_MOVE_END)
		pwr = NVC_PWR_ON;
	return sh532u_pm_wr(info, pwr);
}

static void sh532u_pm_exit(struct sh532u_info *info)
{
	sh532u_pm_wr(info, NVC_PWR_OFF_FORCE);
	sh532u_vreg_exit(info);
	sh532u_gpio_exit(info);
}

static void sh532u_pm_init(struct sh532u_info *info)
{
	sh532u_gpio_init(info);
	sh532u_vreg_init(info);
}

static int sh532u_reset(struct sh532u_info *info, u32 level)
{
	int err;

	if (level == NVC_RESET_SOFT) {
		err = sh532u_pm_wr(info, NVC_PWR_COMM);
		err |= sh532u_i2c_wr8(info, SFTRST_211, 0xFF); /* SW reset */
		mdelay(1);
		err |= sh532u_i2c_wr8(info, SFTRST_211, 0);
	} else {
		err = sh532u_pm_wr(info, NVC_PWR_OFF_FORCE);
	}
	err |= sh532u_pm_wr(info, info->pwr_api);
	return err;
}

static int sh532u_dev_id(struct sh532u_info *info)
{
	u8 val = 0;
	unsigned i;
	int err;

	sh532u_pm_dev_wr(info, NVC_PWR_COMM);
	err = sh532u_i2c_rd8(info, 0, HVCA_DEVICE_ID, &val);
	if (!err) {
		dev_dbg(&info->i2c_client->dev, "%s found devId: %x\n",
			__func__, val);
		info->id_minor = 0;
		for (i = 0; i < ARRAY_SIZE(sh532u_ids); i++) {
			if (val == sh532u_ids[i]) {
				info->id_minor = val;
				break;
			}
		}
		if (!info->id_minor) {
			err = -ENODEV;
			dev_dbg(&info->i2c_client->dev, "%s No devId match\n",
				__func__);
		}
	}
	sh532u_pm_dev_wr(info, NVC_PWR_OFF);
	return err;
}

static void sh532u_sts_rd(struct sh532u_info *info)
{
	u8 us_tmp;
	u16 us_smv_fin;
	int err;

	if (info->sts == NVC_FOCUS_STS_INITIALIZING)
		return;

	info->sts = NVC_FOCUS_STS_NO_DEVICE; /* assume I2C err */
	err = sh532u_i2c_rd8(info, 0, STMVEN_211, &us_tmp);
	err |= sh532u_i2c_rd16(info, RZ_211H, &us_smv_fin);
	if (err)
		return;

	/* StepMove Error Handling, Unexpected Position */
	if ((us_smv_fin == 0x7FFF) || (us_smv_fin == 0x8001))
		/* Stop StepMove Operation */
		sh532u_i2c_wr8(info, STMVEN_211, us_tmp & 0xFE);
	if (us_tmp & STMVEN_ON) {
		err = sh532u_i2c_rd8(info, 0, MSSET_211, &us_tmp);
		if (!err) {
			if (us_tmp & CHTGST_ON)
				info->sts = NVC_FOCUS_STS_WAIT_FOR_SETTLE;
			else
				info->sts = NVC_FOCUS_STS_LENS_SETTLED;
		}
	} else {
		info->sts = NVC_FOCUS_STS_WAIT_FOR_MOVE_END;
	}
}

static s16 sh532u_rel2abs(struct sh532u_info *info, s32 rel_position)
{
	s16 abs_pos;

	if (rel_position > info->cap.actuator_range)
		rel_position = info->cap.actuator_range;
	if (info->cap.position_translate)  {
		rel_position = info->cap.actuator_range - rel_position;
		if (rel_position) {
			rel_position *= info->abs_range;
			rel_position /= info->cap.actuator_range;
		}
		abs_pos = (s16)(info->abs_base + rel_position);
	} else {
		abs_pos = rel_position * SH532U_POS_SIGN_CHANGER;
	}

	if (abs_pos < info->cfg.limit_low)
		abs_pos = info->cfg.limit_low;
	if (abs_pos > info->cfg.limit_high)
		abs_pos = info->cfg.limit_high;

	dev_dbg(&info->i2c_client->dev, "%s: rel_position %d returning abs_pos %d\n",
			__func__, rel_position, abs_pos);

	return abs_pos;
}

static u32 sh532u_abs2rel(struct sh532u_info *info, s16 abs_position)
{
	u32 rel_pos;

	if (abs_position > info->cfg.limit_high)
		abs_position = info->cfg.limit_high;
	if (abs_position < info->abs_base)
		abs_position = info->abs_base;

	if (info->cap.position_translate) {
		rel_pos = (u32)(abs_position - info->abs_base);
		rel_pos *= info->cap.actuator_range;
		rel_pos /= info->abs_range;

		if (rel_pos > info->cap.actuator_range)
			rel_pos = info->cap.actuator_range;
		rel_pos = info->cap.actuator_range - rel_pos;
	} else {
		rel_pos = abs_position * SH532U_POS_SIGN_CHANGER;
	}
	dev_dbg(&info->i2c_client->dev, "%s: abs_position %d returning rel_pos %d",
			__func__, abs_position, rel_pos);

	return rel_pos;
}

static int sh532u_abs_pos_rd(struct sh532u_info *info, s16 *position)
{
	int err;
	u16 abs_pos = 0;

	err = sh532u_i2c_rd16(info, RZ_211H, &abs_pos);
	*position = (s16)abs_pos;
	return err;
}

static int sh532u_rel_pos_rd(struct sh532u_info *info, s32 *position)
{
	s16 abs_pos;
	long msec;
	int pos;
	int err;

	err = sh532u_abs_pos_rd(info, &abs_pos);
	if (err)
		return -EINVAL;

	if ((abs_pos >= (info->pos_abs - STMV_SIZE)) &&
			(abs_pos <= (info->pos_abs + STMV_SIZE))) {
		pos = (int)info->pos_rel;
	} else {
		msec = jiffies;
		msec -= info->pos_time_wr;
		msec = msec * 1000 / HZ;
		sh532u_sts_rd(info);
		if ((info->sts == NVC_FOCUS_STS_LENS_SETTLED) ||
				(msec > info->cfg.move_timeoutms)) {
			pos = (int)info->pos_rel;
		} else {
			pos = (int)sh532u_abs2rel(info, abs_pos);
			if ((pos == (info->pos_rel - 1)) ||
					(pos == (info->pos_rel + 1)))
				pos = (int)info->pos_rel;
		}
	}
	if (info->cap.position_translate) {
		if (pos < 0)
			pos = 0;
	}
	*position = pos;
	return 0;
}

static void sh532u_calibration_caps(struct sh532u_info *info)
{
	s16 abs_top;
	u32 rel_range;
	u32 rel_lo;
	u32 rel_hi;
	u32 step;
	u32 loop_limit;
	u32 i;

	/*
	 * calculate relative and absolute positions
	 * Note that relative values, what upper SW uses, are the
	 * abstraction of HW (absolute) values.
	 * |<--limit_low                                  limit_high-->|
	 * | |<-------------------_ACTUATOR_RANGE------------------->| |
	 *              -focus_inf                        -focus_mac
	 *   |<---RI--->|                                 |<---RM--->|
	 *   -abs_base  -pos_low                          -pos_high  -abs_top
	 *
	 * The pos_low and pos_high are fixed absolute positions and correspond
	 * to the relative focus_infinity and focus_macro, respectively.  We'd
	 * like to have "wiggle" room (RI and RM) around these relative
	 * positions so the loop below finds the best fit for RI and RM without
	 * passing the absolute limits.
	 * We want our _ACTUATOR_RANGE to be infinity on the 0 end and macro
	 * on the max end.  However, the focuser HW is opposite this.
	 * Therefore we use the rel(ative)_lo/hi variables in the calculation
	 * loop and assign them the focus_infinity and focus_macro values.
	 */
	rel_lo = (info->cap.actuator_range - info->cap.focus_macro);
	rel_hi = info->cap.focus_infinity;
	info->abs_range = (u32)(info->cfg.pos_high - info->cfg.pos_low);
	loop_limit = (rel_lo > rel_hi) ? rel_lo : rel_hi;
	dev_dbg(&info->i2c_client->dev, "%s: rel_lo %d rel_hi %d loop_limit %d\n",
				__func__, rel_lo, rel_hi, loop_limit);
	for (i = 0; i <= loop_limit; i++) {
		rel_range = info->cap.actuator_range - (rel_lo + rel_hi);
		step = info->abs_range / rel_range;
		info->abs_base = info->cfg.pos_low - (step * rel_lo);
		abs_top = info->cfg.pos_high + (step * rel_hi);
		if (info->abs_base < info->cfg.limit_low) {
			if (rel_lo > 0)
				rel_lo--;
		}
		if (abs_top > info->cfg.limit_high) {
			if (rel_hi > 0)
				rel_hi--;
		}
		if (info->abs_base >= info->cfg.limit_low &&
					abs_top <= info->cfg.limit_high)
			break;
	}
	dev_dbg(&info->i2c_client->dev, "%s: info->abs_range %d abs_base %d abs_top %d\n",
			__func__, info->abs_range, info->abs_base, abs_top);

	if (!info->cap.position_translate && info->abs_range)
		info->cap.actuator_range = info->abs_range;

	info->cap.focus_hyper = info->abs_range;
	info->abs_range = (u32)(abs_top - info->abs_base);
	/* calculate absolute hyperfocus position */
	info->cap.focus_hyper *= info->cfg.focus_hyper_ratio;
	info->cap.focus_hyper /= info->cfg.focus_hyper_div;
	abs_top = (s16)(info->cfg.pos_high - info->cap.focus_hyper);

	/* update actual relative positions */
	info->cap.focus_hyper = sh532u_abs2rel(info, abs_top);
	dev_dbg(&info->i2c_client->dev, "%s: focus_hyper abs %d rel %d\n",
			__func__, abs_top, info->cap.focus_hyper);

	info->cap.focus_infinity = sh532u_abs2rel(info, info->cfg.pos_high);
	dev_dbg(&info->i2c_client->dev, "%s: focus_infinity abs %d rel %d\n",
			__func__, info->cfg.pos_high, info->cap.focus_infinity);

	info->cap.focus_macro = sh532u_abs2rel(info, info->cfg.pos_low);
	dev_dbg(&info->i2c_client->dev, "%s: focus_macro abs %d rel %d\n",
			__func__, info->cfg.pos_low, info->cap.focus_macro);

	dev_dbg(&info->i2c_client->dev, "%s: Version %d actuator_range %d "
			"settle_time %d position_traslate %d\n",
			__func__, info->cap.version, info->cap.actuator_range,
			info->cap.settle_time, info->cap.position_translate);
}

static int sh532u_calibration(struct sh532u_info *info, bool use_defaults)
{
	u8 reg;
	int err;
	int ret = 0;

	if (info->init_cal_flag) {
		dev_dbg(&info->i2c_client->dev, "%s: Already initialized"
				"Returning\n", __func__);
		return 0;
	}

	/*
	 * Get Inf1, Mac1
	 * Inf1 and Mac1 are the mechanical limit position.
	 * Inf1: top limit.
	 * Mac1: bottom limit.
	 */
	err = sh532u_i2c_rd8(info, info->i2c_addr_rom, addrMac1, &reg);
	if (!err && (reg != 0) && (reg != 0xFF))
		info->cfg.limit_low = (reg<<8) & 0xff00;
	ret = err;
	err = sh532u_i2c_rd8(info, info->i2c_addr_rom, addrInf1, &reg);
	if (!err && (reg != 0) && (reg != 0xFF))
		info->cfg.limit_high = (reg<<8) & 0xff00;
	ret |= err;
	/*
	 * Get Inf2, Mac2
	 * Inf2 and Mac2 are the calibration data for SEMCO AF lens.
	 * Inf2: Best focus (lens position) when object distance is 1.2M.
	 * Mac2: Best focus (lens position) when object distance is 10cm.
	 */
	err = sh532u_i2c_rd8(info, info->i2c_addr_rom, addrMac2, &reg);
	if (!err && (reg != 0) && (reg != 0xFF))
		info->cfg.pos_low = (reg << 8) & 0xff00;
	ret |= err;
	err = sh532u_i2c_rd8(info, info->i2c_addr_rom, addrInf2, &reg);
	if (!err && (reg != 0) && (reg != 0xFF))
		info->cfg.pos_high = (reg << 8) & 0xff00;
	ret |= err;
	/* set overrides */
	if (info->pdata->info) {
		if (info->pdata->info->pos_low)
			info->cfg.pos_low = info->pdata->info->pos_low;
		if (info->pdata->info->pos_high)
			info->cfg.pos_high = info->pdata->info->pos_high;
		if (info->pdata->info->limit_low)
			info->cfg.limit_low = info->pdata->info->limit_low;
		if (info->pdata->info->limit_high)
			info->cfg.limit_high = info->pdata->info->limit_high;
		if (info->pdata->info->move_timeoutms)
			info->cfg.move_timeoutms =
					info->pdata->info->move_timeoutms;
		if (info->pdata->info->focus_hyper_ratio)
			info->cfg.focus_hyper_ratio =
					info->pdata->info->focus_hyper_ratio;
		if (info->pdata->info->focus_hyper_div)
			info->cfg.focus_hyper_div =
					info->pdata->info->focus_hyper_div;
	}
	/*
	 * There is known to be many sh532u devices with no EPROM data.
	 * Using default data is known to reduce the sh532u performance since
	 * the defaults may no where be close to the correct values that
	 * should be used.  However, we don't want to prevent the camera from
	 * starting due to the lack of the EPROM data.
	 * The following truth table shows the action to take at this point:
	 * DFLT = the use_defaults flag (used after multiple attempts)
	 * I2C = the I2C transactions to get the data.
	 * DATA = the needed data either from the EPROM or board file.
	 * DFLT   I2C   DATA   Action
	 * --------------------------
	 *  0     FAIL  FAIL   Exit with -EIO
	 *  0     FAIL  PASS   Continue to calculations
	 *  0     PASS  FAIL   Use defaults
	 *  0     PASS  PASS   Continue to calculations
	 *  1     FAIL  FAIL   Use defaults
	 *  1     FAIL  PASS   Continue to calculations
	 *  1     PASS  FAIL   Use defaults
	 *  1     PASS  PASS   Continue to calculations
	 */
	/* err = DATA where FAIL = 1 */
	if (!info->cfg.pos_low || info->cfg.pos_high <= info->cfg.pos_low ||
		!info->cfg.limit_low ||
		info->cfg.limit_high <= info->cfg.limit_low)
		err = 1;
	else
		err = 0;
	/* Exit with -EIO */
	if (!use_defaults && ret && err) {
		dev_err(&info->i2c_client->dev, "%s ERR\n", __func__);
		return -EIO;
	}

	/* Use defaults */
	if (err) {
		info->cfg.pos_low = SH532U_POS_LOW_DEFAULT;
		info->cfg.pos_high = SH532U_POS_HIGH_DEFAULT;
		info->cfg.limit_low = SH532U_POS_LOW_DEFAULT;
		info->cfg.limit_high = SH532U_POS_HIGH_DEFAULT;
		dev_err(&info->i2c_client->dev, "%s ERR: ERPOM data is void!  "
			"Focuser will use defaults that will cause "
			"reduced functionality!\n", __func__);
	}
	if (info->cfg.pos_low < info->cfg.limit_low)
		info->cfg.pos_low = info->cfg.limit_low;
	if (info->cfg.pos_high > info->cfg.limit_high)
		info->cfg.pos_high = info->cfg.limit_high;
	dev_dbg(&info->i2c_client->dev, "%s pos_low=%d\n",
		__func__, (int)info->cfg.pos_low);
	dev_dbg(&info->i2c_client->dev, "%s pos_high=%d\n",
		__func__, (int)info->cfg.pos_high);
	dev_dbg(&info->i2c_client->dev, "%s limit_low=%d\n",
		__func__, (int)info->cfg.limit_low);
	dev_dbg(&info->i2c_client->dev, "%s limit_high=%d\n",
		__func__, (int)info->cfg.limit_high);

	sh532u_calibration_caps(info);
	info->init_cal_flag = 1;
	dev_dbg(&info->i2c_client->dev, "%s complete\n", __func__);
	return 0;
}

	/* Write 1 byte data to the HVCA Drive IC by data type */
static int sh532u_hvca_wr1(struct sh532u_info *info,
			   u8 ep_type, u8 ep_data1, u8 ep_addr)
{
	u8 us_data;
	int err = 0;

	switch (ep_type & 0xF0) {
	case DIRECT_MODE:
		us_data = ep_data1;
		break;

	case INDIRECT_EEPROM:
		err = sh532u_i2c_rd8(info,
				     info->i2c_addr_rom,
				     ep_data1,
				     &us_data);
		break;

	case INDIRECT_HVCA:
		err = sh532u_i2c_rd8(info, 0, ep_data1, &us_data);
		break;

	case MASK_AND:
		err = sh532u_i2c_rd8(info, 0, ep_addr, &us_data);
		us_data &= ep_data1;
		break;

	case MASK_OR:
		err = sh532u_i2c_rd8(info, 0, ep_addr, &us_data);
		us_data |= ep_data1;
		break;

	default:
		err = -EINVAL;
	}
	if (!err)
		err = sh532u_i2c_wr8(info, ep_addr, us_data);
	return err;
}

	/* Write 2 byte data to the HVCA Drive IC by data type */
static int sh532u_hvca_wr2(struct sh532u_info *info, u8 ep_type,
				u8 ep_data1, u8 ep_data2, u8 ep_addr)
{
	u8 uc_data1;
	u8 uc_data2;
	u16 us_data;
	int err = 0;

	switch (ep_type & 0xF0) {
	case DIRECT_MODE:
		us_data = (((u16)ep_data1 << 8) & 0xFF00) |
			((u16)ep_data2 & 0x00FF);
		break;

	case INDIRECT_EEPROM:
		err = sh532u_i2c_rd8(info,
				     info->i2c_addr_rom,
				     ep_data1,
				     &uc_data1);
		err |= sh532u_i2c_rd8(info,
				      info->i2c_addr_rom,
				      ep_data2,
				      &uc_data2);
		us_data = (((u16)uc_data1 << 8) & 0xFF00) |
				((u16)uc_data2 & 0x00FF);
		break;

	case INDIRECT_HVCA:
		err = sh532u_i2c_rd8(info, 0, ep_data1, &uc_data1);
		err |= sh532u_i2c_rd8(info, 0, ep_data2, &uc_data2);
		us_data = (((u16)uc_data1 << 8) & 0xFF00) |
				((u16)uc_data2 & 0x00FF);
		break;

	case MASK_AND:
		err = sh532u_i2c_rd16(info, ep_addr, &us_data);
		us_data &= ((((u16)ep_data1 << 8) & 0xFF00) |
			    ((u16)ep_data2 & 0x00FF));
		break;

	case MASK_OR:
		err = sh532u_i2c_rd16(info, ep_addr, &us_data);
		us_data |= ((((u16)ep_data1 << 8) & 0xFF00) |
			    ((u16)ep_data2 & 0x00FF));
		break;

	default:
		err = -EINVAL;
	}
	if (!err)
		err = sh532u_i2c_wr16(info, ep_addr, us_data);
	return err;
}

static int sh532u_dev_init(struct sh532u_info *info)
{
	int eeprom_reg;
	unsigned eeprom_data = 0;
	u8 ep_addr;
	u8 ep_type;
	u8 ep_data1;
	u8 ep_data2;
	int err;
	int ret = 0;

	err = sh532u_i2c_rd8(info, 0, SWTCH_211, &ep_data1);
	if (err)
		return err; /* exit if unable to communicate with device */

	ep_data2 = ep_data1;
	err |= sh532u_i2c_rd8(info, 0, ANA1_211, &ep_data1);
	ep_data2 |= ep_data1;
	if (!err && ep_data2)
		return 0; /* Already initialized */

	info->sts = NVC_FOCUS_STS_INITIALIZING;
	for (eeprom_reg = 0x30; eeprom_reg <= 0x013C; eeprom_reg += 4) {
		if (eeprom_reg > 0xFF) {
			/* use hardcoded data instead */
			eeprom_data = sh532u_a2buf[(eeprom_reg & 0xFF) / 4];
		} else {
			err = (sh532u_i2c_rd32(info,
					    info->i2c_addr_rom,
					    eeprom_reg & 0xFF,
					    &eeprom_data));
			if (err) {
				ret |= err;
				continue;
			}
		}

		/* HVCA Address to write eeprom Data1,Data2 by the Data type */
		ep_addr = (u8)(eeprom_data & 0x000000ff);
		ep_type = (u8)((eeprom_data & 0x0000ff00) >> 8);
		ep_data1 = (u8)((eeprom_data & 0x00ff0000) >> 16);
		ep_data2 = (u8)((eeprom_data & 0xff000000) >> 24);
		if (ep_addr == 0xFF)
			break;

		if (ep_addr == 0xDD) {
			mdelay((unsigned int)((ep_data1 << 8) | ep_data2));
		} else {
			if ((ep_type & 0x0F) == DATA_1BYTE) {
				err = sh532u_hvca_wr1(info,
						      ep_type,
						      ep_data1,
						      ep_addr);
			} else {
				err = sh532u_hvca_wr2(info,
						      ep_type,
						      ep_data1,
						      ep_data2,
						      ep_addr);
			}
		}
		ret |= err;
	}

	err = ret;
	if (err)
		dev_err(&info->i2c_client->dev, "%s programming err=%d\n",
			__func__, err);
	err |= sh532u_calibration(info, false);
	info->sts = NVC_FOCUS_STS_LENS_SETTLED;
	return err;
}

static int sh532u_pos_abs_wr(struct sh532u_info *info, s16 tar_pos)
{
	s16 cur_pos;
	s16 move_step;
	u16 move_distance;
	int err;

	sh532u_pm_dev_wr(info, NVC_PWR_ON);
	err = sh532u_dev_init(info);
	if (err)
		return err;

	/* Read Current Position */
	err = sh532u_abs_pos_rd(info, &cur_pos);
	if (err)
		return err;

	dev_dbg(&info->i2c_client->dev, "%s cur_pos=%d tar_pos=%d\n",
		__func__, (int)cur_pos, (int)tar_pos);
	info->sts = NVC_FOCUS_STS_WAIT_FOR_MOVE_END;
	/* Check move distance to Target Position */
	move_distance = abs((int)cur_pos - (int)tar_pos);
	/* if move distance is shorter than MS1Z12(=Step width) */
	if (move_distance <= STMV_SIZE) {
		err = sh532u_i2c_wr8(info, MSSET_211,
				     (INI_MSSET_211 | 0x01));
		err |= sh532u_i2c_wr16(info, MS1Z22_211H, tar_pos);
	} else {
		if (cur_pos < tar_pos)
			move_step = STMV_SIZE;
		else
			move_step = -STMV_SIZE;
		/* Set StepMove Target Positon */
		err = sh532u_i2c_wr16(info, MS1Z12_211H, move_step);
		err |= sh532u_i2c_wr16(info, STMVENDH_211, tar_pos);
		/* Start StepMove */
		err |= sh532u_i2c_wr8(info, STMVEN_211,
				      (STMCHTG_ON |
				       STMSV_ON |
				       STMLFF_OFF |
				       STMVEN_ON));
	}
	dev_dbg(&info->i2c_client->dev, "%s: position %d\n", __func__, tar_pos);
	return err;
}

static int sh532u_move_wait(struct sh532u_info *info)
{
	u16 us_smv_fin;
	u8 moveTime;
	u8 ucParMod;
	u8 tmp;
	int err;

	moveTime = 0;
	do {
		mdelay(1);
		err = sh532u_i2c_rd8(info, 0, STMVEN_211, &ucParMod);
		err |= sh532u_i2c_rd16(info, RZ_211H, &us_smv_fin);
		if (err)
			return err;

		/* StepMove Error Handling, Unexpected Position */
		if ((us_smv_fin == 0x7FFF) || (us_smv_fin == 0x8001)) {
			/* Stop StepMove Operation */
			err = sh532u_i2c_wr8(info, STMVEN_211,
					     ucParMod & 0xFE);
			if (err)
				return err;
		}

		moveTime++;
		/* Wait StepMove operation end */
	} while ((ucParMod & STMVEN_ON) && (moveTime < 50));

	moveTime = 0;
	if ((ucParMod & 0x08) == STMCHTG_ON) {
		mdelay(5);
		do {
			mdelay(1);
			moveTime++;
			err = sh532u_i2c_rd8(info, 0, MSSET_211, &tmp);
			if (err)
				return err;

		} while ((tmp & CHTGST_ON) && (moveTime < 15));
	}
	return err;
}

static int sh532u_move_pulse(struct sh532u_info *info, s16 position)
{
	int err;

	err = sh532u_pos_abs_wr(info, position);
	err |= sh532u_move_wait(info);
	return err;
}

static int sh532u_hvca_pos_init(struct sh532u_info *info)
{
	s16 limit_bottom;
	s16 limit_top;
	int err;

	limit_bottom = (((int)info->cfg.limit_low * 5) >> 3) & 0xFFC0;
	if (limit_bottom < info->cfg.limit_low)
		limit_bottom = info->cfg.limit_low;
	limit_top = (((int)info->cfg.limit_high * 5) >> 3) & 0xFFC0;
	if (limit_top > info->cfg.limit_high)
		limit_top = info->cfg.limit_high;
	err = sh532u_move_pulse(info, limit_bottom);
	err |= sh532u_move_pulse(info, limit_top);
	err |= sh532u_move_pulse(info, info->cfg.pos_high);
	return err;
}

static int sh532u_pos_rel_wr(struct sh532u_info *info, s32 position)
{
	s16 abs_pos;

	if (info->cap.position_translate) {
		if (position > info->cap.actuator_range) {
			dev_err(&info->i2c_client->dev, "%s invalid position %d\n",
				__func__, position);
			return -EINVAL;
		}
	}
	abs_pos = sh532u_rel2abs(info, position);

	info->pos_rel = position;
	info->pos_abs = abs_pos;
	info->pos_time_wr = jiffies;
	return sh532u_pos_abs_wr(info, abs_pos);
}

static void sh532u_get_focuser_capabilities(struct sh532u_info *info)
{
	memset(&info->config, 0, sizeof(info->config));

	info->config.focal_length = info->nvc.focal_length;
	info->config.fnumber = info->nvc.fnumber;
	info->config.max_aperture = info->nvc.fnumber;
	info->config.range_ends_reversed = (SH532U_POS_SIGN_CHANGER == -1)
								? 1 : 0;
	/*
	 * We do not use pos_working_low and pos_working_high
	 * in the kernel driver. It is OK to set them to invalid
	 * as the caller will reconstruct them from
	 * focuser_set[0].inf and focuser_set[0].macro
	 */
	info->config.pos_working_low = AF_POS_INVALID_VALUE;
	info->config.pos_working_high = AF_POS_INVALID_VALUE;

	info->config.pos_actual_low = SH532U_POS_PHYSICAL_MIN;
	info->config.pos_actual_high = SH532U_POS_PHYSICAL_MAX;
	info->config.slew_rate = info->cap.slew_rate;
	info->config.circle_of_confusion = -1;

	/*
	 * These need to be passed up once we have the EEPROM/OTP read
	 * routines in teh kernel. These need to be passed up much earlier on.
	 * Till we have these routines, we pass them up as part of the get call.
	 */
	info->config.num_focuser_sets = 1;
	info->config.focuser_set[0].posture = 's';
	info->config.focuser_set[0].macro = info->cap.focus_macro;
	info->config.focuser_set[0].hyper = info->cap.focus_hyper;
	info->config.focuser_set[0].inf = info->cap.focus_infinity;
	info->config.focuser_set[0].hysteresis = INT_MAX;
	info->config.focuser_set[0].settle_time = info->cap.settle_time;
	info->config.focuser_set[0].num_dist_pairs = 0;

	dev_dbg(&info->i2c_client->dev, "%s: pos_actual_low %d pos_actual_high %d "
		" settle_time %d\n", __func__, info->config.pos_actual_low,
		info->config.pos_actual_high, info->cap.settle_time);

}


static int sh532u_set_focuser_capabilities(struct sh532u_info *info,
					struct nvc_param *params)
{
	static struct nv_focuser_config config;

	/* backup the current contents of dev->focuser_info->config for
	 * selective restore later on */
	memcpy(&config, &info->config,
		sizeof(struct nv_focuser_config));

	if (copy_from_user(&info->config, (const void __user *)params->p_value,
		params->sizeofvalue)) {
		dev_err(&info->i2c_client->dev, "%s Error: copy_from_user bytes %d\n",
				__func__, params->sizeofvalue);
		return -EFAULT;
	}

	/* Now do selectively restore members that are overwriten */

	/* Unconditionally restore actual low and high positions */
	info->config.pos_actual_low = config.pos_actual_low;
	info->config.pos_actual_high = config.pos_actual_high;

	if (info->config.pos_working_low == AF_POS_INVALID_VALUE)
		info->config.pos_working_low = config.pos_working_low;

	if (info->config.pos_working_high == AF_POS_INVALID_VALUE)
		info->config.pos_working_high = config.pos_working_high;

	if (info->config.slew_rate == INT_MAX)
		info->config.slew_rate = config.slew_rate;
	info->cap.slew_rate = info->config.slew_rate;

	if (info->config.focuser_set[0].settle_time  == INT_MAX)
		info->config.focuser_set[0].settle_time =
					config.focuser_set[0].settle_time;
	info->cap.settle_time = info->config.focuser_set[0].settle_time;

	if (info->config.focuser_set[0].macro == AF_POS_INVALID_VALUE)
		info->config.focuser_set[0].macro = config.focuser_set[0].macro;
	info->cap.focus_macro = info->config.focuser_set[0].macro;

	if (info->config.focuser_set[0].hyper == AF_POS_INVALID_VALUE)
		info->config.focuser_set[0].hyper = config.focuser_set[0].hyper;
	info->cap.focus_hyper = info->config.focuser_set[0].hyper;

	if (info->config.focuser_set[0].inf == AF_POS_INVALID_VALUE)
		info->config.focuser_set[0].inf = config.focuser_set[0].inf;
	info->cap.focus_infinity = info->config.focuser_set[0].inf;

	dev_dbg(&info->i2c_client->dev, "%s: copy_from_user bytes %d\n",
			__func__,  params->sizeofvalue);
	return 0;
}


static int sh532u_param_rd(struct sh532u_info *info, unsigned long arg)
{
	struct nvc_param params;
	const void *data_ptr;
	u32 data_size = 0;
	s32 position;
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
		sh532u_pm_dev_wr(info, NVC_PWR_COMM);
		err = sh532u_rel_pos_rd(info, &position);
		if (err && !(info->pdata->cfg & NVC_CFG_NOERR))
			return -EINVAL;

		data_ptr = &position;
		data_size = sizeof(position);
		sh532u_pm_dev_wr(info, NVC_PWR_STDBY);
		dev_dbg(&info->i2c_client->dev, "%s LOCUS: %d\n",
			__func__, position);
		break;

	case NVC_PARAM_FOCAL_LEN:
		data_ptr = &info->nvc.focal_length;
		data_size = sizeof(info->nvc.focal_length);
		dev_dbg(&info->i2c_client->dev, "%s FOCAL_LEN: %x\n",
			__func__, info->nvc.focal_length);
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
		sh532u_pm_dev_wr(info, NVC_PWR_COMM);
		err = sh532u_calibration(info, true);
		sh532u_pm_dev_wr(info, NVC_PWR_STDBY);
		if (err)
			return -EIO;
		dev_dbg(&info->i2c_client->dev, "%s: NVC_PARAM_CAPS: params.param %d "
				"params.sizeofvalue %d\n",
				__func__, params.param, params.sizeofvalue);

		sh532u_get_focuser_capabilities(info);

		data_ptr = &info->config;
		data_size = params.sizeofvalue;
		break;

	case NVC_PARAM_STS:
		data_ptr = &info->sts;
		data_size = sizeof(info->sts);
		dev_dbg(&info->i2c_client->dev, "%s STS: %d\n",
			__func__, info->sts);
		break;

	case NVC_PARAM_STEREO:
		data_ptr = &info->s_mode;
		data_size = sizeof(info->s_mode);
		dev_dbg(&info->i2c_client->dev, "%s STEREO: %d\n",
			__func__, info->s_mode);
		break;

	default:
		dev_dbg(&info->i2c_client->dev,
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

	if (copy_to_user((void __user *)params.p_value,
			 data_ptr,
			 data_size)) {
		dev_err(&info->i2c_client->dev,
			"%s copy_to_user err line %d\n", __func__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

static int sh532u_param_wr_s(struct sh532u_info *info,
			     struct nvc_param *params,
			     u32 u32val)
{
	u8 u8val;
	int err;

	u8val = (u8)u32val;
	switch (params->param) {
	case NVC_PARAM_LOCUS:
		dev_dbg(&info->i2c_client->dev, "%s LOCUS: %d\n",
			__func__, (s32) u32val);
		err = sh532u_pos_rel_wr(info, (s32) u32val);
		return err;

	case NVC_PARAM_RESET:
		err = sh532u_reset(info, u32val);
		dev_dbg(&info->i2c_client->dev, "%s RESET: %d\n",
			__func__, err);
		return err;

	case NVC_PARAM_SELF_TEST:
		err = sh532u_hvca_pos_init(info);
		dev_dbg(&info->i2c_client->dev, "%s SELF_TEST: %d\n",
			__func__, err);
		return err;

	case NVC_PARAM_CAPS:
		dev_dbg(&info->i2c_client->dev, "%s CAPS. Error. sh532u_param_wr "
				"should be called instead\n", __func__);
		return -EFAULT;

	default:
		dev_dbg(&info->i2c_client->dev,
			"%s unsupported parameter: %d\n",
			__func__, params->param);
		return -EINVAL;
	}
}

static int sh532u_param_wr(struct sh532u_info *info, unsigned long arg)
{
	struct nvc_param params;
	u8 u8val;
	u32 u32val;
	int err = 0;

	if (copy_from_user(&params, (const void __user *)arg,
			   sizeof(struct nvc_param))) {
		dev_err(&info->i2c_client->dev,
			"%s copy_from_user err line %d\n", __func__, __LINE__);
		return -EFAULT;
	}

	if (copy_from_user(&u32val, (const void __user *)params.p_value,
			   sizeof(u32val))) {
		dev_err(&info->i2c_client->dev, "%s %d copy_from_user err\n",
			__func__, __LINE__);
		return -EFAULT;
	}

	u8val = (u8)u32val;
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
			sh532u_gpio_wr(info, SH532U_GPIO_I2CMUX, 0);
			if (info->s_info != NULL) {
				info->s_info->s_mode = u8val;
				sh532u_pm_wr(info->s_info, NVC_PWR_OFF);
			}
			break;

		case NVC_SYNC_MASTER:
			info->s_mode = u8val;
			sh532u_gpio_wr(info, SH532U_GPIO_I2CMUX, 0);
			if (info->s_info != NULL)
				info->s_info->s_mode = u8val;
			break;

		case NVC_SYNC_SLAVE:
			if (info->s_info != NULL) {
				/* default slave lens position */
				err = sh532u_pos_rel_wr(info->s_info,
					     info->s_info->cap.focus_infinity);
				if (!err) {
					info->s_mode = u8val;
					info->s_info->s_mode = u8val;
					sh532u_gpio_wr(info,
						       SH532U_GPIO_I2CMUX, 0);
				} else {
					if (info->s_mode != NVC_SYNC_STEREO)
						sh532u_pm_wr(info->s_info,
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
				info->s_info->pwr_api = info->pwr_api;
				/* move slave lens to master position */
				err = sh532u_pos_rel_wr(info->s_info,
							 info->pos_rel);
				if (!err) {
					info->s_mode = u8val;
					info->s_info->s_mode = u8val;
					sh532u_gpio_wr(info,
						       SH532U_GPIO_I2CMUX, 1);
				} else {
					if (info->s_mode != NVC_SYNC_SLAVE)
						sh532u_pm_wr(info->s_info,
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
		if (sh532u_set_focuser_capabilities(info, &params)) {
			dev_err(&info->i2c_client->dev, "%s: Error: copy_from_user bytes %d\n",
					__func__, params.sizeofvalue);
			return -EFAULT;
		}
		return 0;

	default:
	/* parameters dependent on sync mode */
		switch (info->s_mode) {
		case NVC_SYNC_OFF:
		case NVC_SYNC_MASTER:
			return sh532u_param_wr_s(info, &params, u32val);

		case NVC_SYNC_SLAVE:
			return sh532u_param_wr_s(info->s_info, &params,
						 u32val);

		case NVC_SYNC_STEREO:
			err = sh532u_param_wr_s(info, &params, u32val);
			if (!(info->pdata->cfg & NVC_CFG_SYNC_I2C_MUX))
				err |= sh532u_param_wr_s(info->s_info,
							 &params,
							 u32val);
			return err;

		default:
			dev_err(&info->i2c_client->dev, "%s %d internal err\n",
				__func__, __LINE__);
			return -EINVAL;
		}
	}
}

static long sh532u_ioctl(struct file *file,
			 unsigned int cmd,
			 unsigned long arg)
{
	struct sh532u_info *info = file->private_data;
	int pwr;
	int err;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(NVC_IOCTL_PARAM_WR):
		err = sh532u_param_wr(info, arg);
		return err;

	case _IOC_NR(NVC_IOCTL_PARAM_RD):
		err = sh532u_param_rd(info, arg);
		return err;

	case _IOC_NR(NVC_IOCTL_PWR_WR):
		/* This is a Guaranteed Level of Service (GLOS) call */
		pwr = (int)arg * 2;
		dev_dbg(&info->i2c_client->dev, "%s PWR_WR: %d\n",
			__func__, pwr);
		err = sh532u_pm_api_wr(info, pwr);
		return err;

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
		dev_dbg(&info->i2c_client->dev, "%s unsupported ioctl: %x\n",
			__func__, cmd);
	}

	return -EINVAL;
}

static void sh532u_sdata_init(struct sh532u_info *info)
{
	/* set defaults */
	memcpy(&info->cfg, &sh532u_default_info, sizeof(info->cfg));
	memcpy(&info->nvc, &sh532u_default_nvc, sizeof(info->nvc));
	memcpy(&info->cap, &sh532u_default_cap, sizeof(info->cap));
	if (info->pdata->i2c_addr_rom)
		info->i2c_addr_rom = info->pdata->i2c_addr_rom;
	else
		info->i2c_addr_rom = sh532u_default_pdata.i2c_addr_rom;
	/* set overrides if any */
	if (info->pdata->nvc) {
		if (info->pdata->nvc->fnumber)
			info->nvc.fnumber = info->pdata->nvc->fnumber;
		if (info->pdata->nvc->focal_length)
			info->nvc.focal_length =
					info->pdata->nvc->focal_length;
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

static int sh532u_sync_en(unsigned num, unsigned sync)
{
	struct sh532u_info *master = NULL;
	struct sh532u_info *slave = NULL;
	struct sh532u_info *pos = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(pos, &sh532u_info_list, list) {
		if (pos->pdata->num == num) {
			master = pos;
			break;
		}
	}
	pos = NULL;
	list_for_each_entry_rcu(pos, &sh532u_info_list, list) {
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

static int sh532u_sync_dis(struct sh532u_info *info)
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

static int sh532u_open(struct inode *inode, struct file *file)
{
	struct sh532u_info *info = NULL;
	struct sh532u_info *pos = NULL;
	int err;

	rcu_read_lock();
	list_for_each_entry_rcu(pos, &sh532u_info_list, list) {
		if (pos->miscdev.minor == iminor(inode)) {
			info = pos;
			break;
		}
	}
	rcu_read_unlock();
	if (!info)
		return -ENODEV;

	err = sh532u_sync_en(info->pdata->num, info->pdata->sync);
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
	sh532u_pos_rel_wr(info, info->cap.focus_infinity);
	sh532u_pm_wr_s(info, NVC_PWR_OFF);
	return 0;
}

static int sh532u_release(struct inode *inode, struct file *file)
{
	struct sh532u_info *info = file->private_data;

	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	sh532u_pm_wr_s(info, NVC_PWR_OFF);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	if (info->s_info != NULL)
		WARN_ON(!atomic_xchg(&info->s_info->in_use, 0));
	sh532u_sync_dis(info);
	return 0;
}

static const struct file_operations sh532u_fileops = {
	.owner = THIS_MODULE,
	.open = sh532u_open,
	.unlocked_ioctl = sh532u_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = sh532u_ioctl,
#endif
	.release = sh532u_release,
};

static void sh532u_del(struct sh532u_info *info)
{
	sh532u_pm_exit(info);
	if ((info->s_mode == NVC_SYNC_SLAVE) ||
					     (info->s_mode == NVC_SYNC_STEREO))
		sh532u_pm_exit(info->s_info);
	sh532u_sync_dis(info);
	spin_lock(&sh532u_spinlock);
	list_del_rcu(&info->list);
	spin_unlock(&sh532u_spinlock);
	synchronize_rcu();
}

static int sh532u_remove(struct i2c_client *client)
{
	struct sh532u_info *info = i2c_get_clientdata(client);

	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	misc_deregister(&info->miscdev);
	sh532u_del(info);
	return 0;
}

static int sh532u_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct sh532u_info *info;
	int err;

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	info->i2c_client = client;
	if (client->dev.platform_data) {
		info->pdata = client->dev.platform_data;
	} else {
		info->pdata = &sh532u_default_pdata;
		dev_dbg(&client->dev,
			"%s No platform data.  Using defaults.\n", __func__);
	}
	i2c_set_clientdata(client, info);
	INIT_LIST_HEAD(&info->list);
	spin_lock(&sh532u_spinlock);
	list_add_rcu(&info->list, &sh532u_info_list);
	spin_unlock(&sh532u_spinlock);
	sh532u_pm_init(info);
	sh532u_sdata_init(info);
	if (info->pdata->cfg & (NVC_CFG_NODEV | NVC_CFG_BOOT_INIT)) {
		err = sh532u_dev_id(info);
		if (err < 0) {
			if (info->pdata->cfg & NVC_CFG_NODEV) {
				sh532u_del(info);
				return -ENODEV;
			} else {
				dev_err(&client->dev, "%s dev %x not found\n",
					__func__, SH532U_ID);
			}
		} else {
			dev_dbg(&client->dev, "%s device found\n", __func__);
			sh532u_pm_dev_wr(info, NVC_PWR_ON);
			sh532u_calibration(info, false);
			if (info->pdata->cfg & NVC_CFG_BOOT_INIT)
				/* initial move causes full initialization */
				sh532u_pos_rel_wr(info,
						  info->cap.focus_infinity);
		}
		sh532u_pm_dev_wr(info, NVC_PWR_OFF);
	}

	if (info->pdata->dev_name != 0)
		strncpy(info->devname, info->pdata->dev_name,
			sizeof(info->devname) - 1);
	else
		strncpy(info->devname, "sh532u", sizeof(info->devname) - 1);

	if (info->pdata->num)
		snprintf(info->devname, sizeof(info->devname), "%s.%u",
			 info->devname, info->pdata->num);

	info->miscdev.name = info->devname;
	info->miscdev.fops = &sh532u_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&info->miscdev)) {
		dev_err(&client->dev, "%s unable to register misc device %s\n",
			__func__, info->devname);
		sh532u_del(info);
		return -ENODEV;
	}

	return 0;
}

static const struct i2c_device_id sh532u_id[] = {
	{ "sh532u", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, sh532u_id);

static struct i2c_driver sh532u_i2c_driver = {
	.driver = {
		.name = "sh532u",
		.owner = THIS_MODULE,
	},
	.id_table = sh532u_id,
	.probe = sh532u_probe,
	.remove = sh532u_remove,
};

static int __init sh532u_init(void)
{
	return i2c_add_driver(&sh532u_i2c_driver);
}

static void __exit sh532u_exit(void)
{
	i2c_del_driver(&sh532u_i2c_driver);
}

module_init(sh532u_init);
module_exit(sh532u_exit);
MODULE_LICENSE("GPL");
