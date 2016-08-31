/*
 * MAX77387.c - MAX77387 flash/torch kernel driver
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
 * file with the max77387_platform_data structure.
 * Standard among NVC kernel drivers in this structure is:
 * .cfg = Use the NVC_CFG_ defines that are in nvc_torch.h.
 *        Descriptions of the configuration options are with the defines.
 *        This value is typically 0.
 * .num = The number of the instance of the device.  This should start at 1 and
 *        and increment for each device on the board.  This number will be
 *        appended to the MISC driver name, Example: /dev/torch.1
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
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/regmap.h>
#include <media/nvc.h>
#include <media/max77387.h>

#define MAX77387_RO_CHIPID1		0x00
#define MAX77387_RO_CHIPID2		0x01
#define MAX77387_RO_FLASH_STATUS	0x02
#define MAX77387_RO_FLASH_STATUS2	0x03

#define MAX77387_RW_FLASH_FLED1CURR	0x04
#define MAX77387_RW_FLASH_FLED2CURR	0x05
#define MAX77387_RW_TORCH_FLED1CURR	0x06
#define MAX77387_RW_TORCH_FLED2CURR	0x07
#define MAX77387_RW_FLED_MODE		0x08

#define MAX77387_RW_TX1_MASK		0x09
#define MAX77387_RW_TX2_MASK		0x0A
#define MAX77387_RW_FLASH_RAMP		0x0B
#define MAX77387_RW_TORCH_RAMP		0x0C

#define MAX77387_RW_FLASH_TIMER		0x0D
#define MAX77387_RW_TORCH_TIMER		0x0E

#define MAX77387_RW_MAXFLASH_HYS_TH	0x10
#define MAX77387_RW_MAXFLASH_LB_TMR	0x11
#define MAX77387_RO_MAXFLASH_FLED1_IMIN	0x12
#define MAX77387_RO_MAXFLASH_FLED2_IMIN	0x13
#define MAX77387_RW_NTC			0x14

#define MAX77387_RW_DCDC_CNTL1		0X15
#define MAX77387_RW_DCDC_CNTL2		0X16
#define MAX77387_RW_DCDC_ILIM		0X17
#define MAX77387_RO_DCDC_OUT		0X18
#define MAX77387_RO_DCDC_MAX		0X19

#define FIELD(x, y)			((x) << (y))
#define FMASK(x)			FIELD(7, (x))

#define TORCH_TIMER_SAFETY_DIS		0x1

#define TIMER_ONESHOT			0x0
#define TIMER_MAX			0x1
#define TTIMER_DIS			0x1

#define TORCH_TIMER_CTL_MASK		(FIELD(TIMER_MAX, 7) | \
					FIELD(TORCH_TIMER_SAFETY_DIS, 6))

#define TORCH_MODE_SHIFT		3
#define FLASH_MODE_SHIFT		0

#define TORCH_TRIG_MASK			FMASK(TORCH_MODE_SHIFT)
#define FLASH_TRIG_MASK			FMASK(FLASH_MODE_SHIFT)

/* TO DO: Need to confirm with maxim these trigger settings */
#define TRIG_MODE_OFF			0x00
#define TRIG_MODE_FLASHEN		0x02
#define TRIG_MODE_TORCHEN		0x01
#define TRIG_MODE_FANDT			0x03
#define TRIG_MODE_FORT			0x04
#define TRIG_MODE_I2C			0x05

#define TORCH_TRIG_BY_I2C	FIELD(TRIG_MODE_I2C, TORCH_MODE_SHIFT)
#define TORCH_TRIG_BY_FLASHEN	FIELD(TRIG_MODE_FLASHEN, TORCH_MODE_SHIFT)
#define TORCH_TRIG_BY_TORCHEN	FIELD(TRIG_MODE_TORCHEN, TORCH_MODE_SHIFT)
#define TORCH_TRIG_BY_FANDT	FIELD(TRIG_MODE_FANDT, TORCH_MODE_SHIFT)
#define TORCH_TRIG_BY_FORT	FIELD(TRIG_MODE_FORT, TORCH_MODE_SHIFT)

#define FLASH_TRIG_BY_I2C	FIELD(TRIG_MODE_I2C, FLASH_MODE_SHIFT)
#define FLASH_TRIG_BY_FLASHEN	FIELD(TRIG_MODE_FLASHEN, FLASH_MODE_SHIFT)
#define FLASH_TRIG_BY_TORCHEN	FIELD(TRIG_MODE_TORCHEN, FLASH_MODE_SHIFT)
#define FLASH_TRIG_BY_FANDT	FIELD(TRIG_MODE_FANDT, FLASH_MODE_SHIFT)
#define FLASH_TRIG_BY_FORT	FIELD(TRIG_MODE_FORT, FLASH_MODE_SHIFT)

#define FLASH_ENABLE			FIELD(1, 7)
#define FLASH_CURRENT(x)		(FLASH_ENABLE | FIELD((x), 0))
#define TORCH_ENABLE			FIELD(1, 7)
#define TORCH_CURRENT(x)		(TORCH_ENABLE | FIELD((x), 1))

#define MAXFLASH_DISABLE		0
#define MAXFLASH_ENABLE			1

#define MAXFLASH_VOLT_HYS_FLOOR		100 /* mV */
#define MAXFLASH_VOLT_HYS_CEILING	300 /* mV */
#define MAXFLASH_VOLT_HYS_STEP		100 /* mV */

#define MAXFLASH_V_TH_FLOOR		2400 /* mV */
#define MAXFLASH_V_TH_CEILING		3400 /* mV */
#define MAXFLASH_V_TH_STEP		33 /* mV */

#define MAXFLASH_TIMER_STEP		256 /* uS */

#define MAX77387_LED_NUM		2
#define MAX77387_FLASH_LEVELS		(1 << 6)
#define MAX77387_MAX_FLASH_LEVEL	(MAX77387_FLASH_LEVELS + 1)
#define MAX77387_TORCH_LEVELS		(1 << 6)
#define MAX77387_MAX_TORCH_LEVEL	(MAX77387_TORCH_LEVELS + 1)

#define MAX77387_LEVEL_OFF		0xFFFF

#define MAX77387_MAX_FLASH_CURRENT(x)    \
	DIV_ROUND_UP(((x) * MAX77387_MAX_FLASH_LEVEL), 1000)
#define MAX77387_MAX_TORCH_CURRENT(x) \
	DIV_ROUND_UP(((x) * MAX77387_MAX_TORCH_LEVEL), 1000)

#define MAX77387_FLASH_TIMER_NUM	(1 << 7)
#define MAX77387_TORCH_TIMER_NUM	(1 << 5)
#define MAX77387_FTIMER_MASK		(MAX77387_FLASH_TIMER_NUM - 1)
#define MAX77387_TTIMER_MASK		(MAX77387_TORCH_TIMER_NUM - 1)

#define MAX77387_TX_MASK_ENABLE		FIELD(1, 7)

#define MAXFLASH_MODE_NONE		0
#define MAXFLASH_MODE_TORCH		1
#define MAXFLASH_MODE_FLASH		2

#define max77387_flash_cap_size \
			(sizeof(struct nvc_torch_flash_capabilities_v1) \
			+ sizeof(struct nvc_torch_lumi_level_v1) \
			* MAX77387_MAX_FLASH_LEVEL)
#define max77387_flash_timeout_size \
			(sizeof(struct nvc_torch_timer_capabilities_v1) \
			+ sizeof(struct nvc_torch_timeout_v1) \
			* MAX77387_FLASH_TIMER_NUM)
#define max77387_max_flash_cap_size (max77387_flash_cap_size * 2 \
			+ max77387_flash_timeout_size * 2)

#define max77387_torch_cap_size \
			(sizeof(struct nvc_torch_torch_capabilities_v1) \
			+ sizeof(struct nvc_torch_lumi_level_v1) \
			* MAX77387_MAX_TORCH_LEVEL)
#define max77387_torch_timeout_size \
			(sizeof(struct nvc_torch_timer_capabilities_v1) \
			+ sizeof(struct nvc_torch_timeout_v1) \
			* MAX77387_TORCH_TIMER_NUM)
#define max77387_max_torch_cap_size (max77387_torch_timeout_size * 2\
			+ max77387_torch_timeout_size * 2)

/* mul 15625 uA */
#define GET_CURRENT_UA_INDEX(c)	((c) * max77387_caps.curr_step_uA)
/* mul 15.625 mA */
#define GET_CURRENT_BY_INDEX(c)	((c) * max77387_caps.curr_step_uA / 1000)
/* div by 15.625 mA */
#define GET_INDEX_BY_CURRENT(c)	((c) * 1000 / max77387_caps.curr_step_uA)

struct max77387_caps_struct {
	u32 curr_step_uA;
	u32 max_peak_curr_mA;
	u32 max_torch_curr_mA;
	u32 max_total_current_mA;
};

struct max77387_settings {
	u8 fled_trig;
	u8 tx1_mask;
	u8 tx2_mask;
	u8 flash_ramp;
	u8 torch_ramp;
	u8 max_flash1;
	u8 max_flash2;
	u8 ntc;
	u8 dcdc_cntl1;
	u8 dcdc_cntl2;
	u8 dcdc_lim;
};

struct max77387_reg_cache {
	bool regs_stale;
	u8 led1_fcurr;
	u8 led2_fcurr;
	u8 led1_tcurr;
	u8 led2_tcurr;
	u8 leds_en;
	u8 tmask_led1;
	u8 tmask_led2;
	u8 framp;
	u8 tramp;
	u8 f_timer;
	u8 t_timer;
};

struct max77387_info {
	struct i2c_client *i2c_client;
	struct miscdevice miscdev;
	struct device *dev;
	struct dentry *d_max77387;
	struct mutex mutex;
	struct max77387_power_rail pwr_rail;
	struct max77387_platform_data *pdata;
	struct nvc_torch_capability_query query;
	struct nvc_torch_flash_capabilities_v1 *flash_cap[2];
	struct nvc_torch_timer_capabilities_v1 *flash_timeouts[2];
	struct nvc_torch_torch_capabilities_v1 *torch_cap[2];
	struct nvc_torch_timer_capabilities_v1 *torch_timeouts[2];
	struct max77387_config config;
	struct max77387_reg_cache regs;
	struct max77387_settings settings;
	struct regmap *regmap;
	atomic_t in_use;
	int flash_cap_size;
	int torch_cap_size;
	int pwr_state;
	u8 max_flash[2];
	u8 max_torch[2];
	u16 chip_id;
	u8 op_mode;
	u8 power_is_on;
	u8 ftimer_mode;
	u8 ttimer_mode;
	u8 new_timer;
	char devname[16];
};

static const struct max77387_caps_struct max77387_caps = {
	.curr_step_uA = 15625,
	.max_peak_curr_mA = MAX77387_MAX_FLASH_CURRENT(15625),
	.max_torch_curr_mA = MAX77387_MAX_TORCH_CURRENT(3906),
	.max_total_current_mA = 1000 * 2
};

static struct nvc_torch_lumi_level_v1
		max77387_def_flash_levels[MAX77387_FLASH_LEVELS];

static struct max77387_platform_data max77387_default_pdata = {
	.config		= {
			.led_mask = 3, /* both LEDs enabled */
			.synchronized_led = false,
			.flash_trigger_mode = 3,
			.torch_trigger_mode = 3,
			.flash_mode = 2,
			.torch_mode = 1,
			.adaptive_mode = 2,
			.tx1_mask_mA = 0,
			.tx2_mask_mA = 0,
			.flash_rampup_uS = 0,
			.flash_rampdn_uS = 0,
			.torch_rampup_uS = 0,
			.torch_rampdn_uS = 0,
			.max_peak_current_mA = 1000,
			.max_torch_current_mA = 250,
			.max_peak_duration_ms = 0,
			.max_flash_threshold_mV = 0,
			.led_config[0] = {
				.flash_torch_ratio = 10000,
				.granularity = 1000,
				.flash_levels =
					ARRAY_SIZE(max77387_def_flash_levels),
				.lumi_levels = max77387_def_flash_levels,
				},
			.led_config[1] = {
				.flash_torch_ratio = 10000,
				.granularity = 1000,
				.flash_levels =
					ARRAY_SIZE(max77387_def_flash_levels),
				.lumi_levels = max77387_def_flash_levels,
				},
			},
	.cfg		= 0,
	.num		= 0,
	.sync		= 0,
	.dev_name	= "torch",
	.pinstate	= {0x0000, 0x0000},
};

/* flash timer duration settings in uS */
static u32 max77387_flash_timer[MAX77387_FLASH_TIMER_NUM] = {
	128, 384, 640, 896, 1410, 1920, 2430, 2940,
};

/* torch timer duration settings in uS */
#define MAX77387_TORCH_TIMER_FOREVER	0xFFFFFFFF
static u32 max77387_torch_timer[MAX77387_TORCH_TIMER_NUM + 1] = {
	122880,
};

static bool rd_wr_reg_chk(struct device *dev, unsigned int reg)
{
	if (reg > MAX77387_RO_DCDC_MAX) {
		dev_err(dev, "%s: non-existing reg 0x%x\n", __func__, reg);
		return false;
	}
	return true;
}

static const struct regmap_config max77387_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = MAX77387_RO_DCDC_MAX,
	.writeable_reg = rd_wr_reg_chk,
	.readable_reg = rd_wr_reg_chk,
	.cache_type = REGCACHE_RBTREE,
};

static inline int max77387_reg_raw_rd(
	struct max77387_info *info, u8 reg, u8 *val, u8 num)
{
	int ret = -ENODEV;

	mutex_lock(&info->mutex);
	if (info->power_is_on)
		ret = regmap_raw_read(info->regmap, reg, val, num);
	else
		dev_err(info->dev, "%s: power is off.\n", __func__);
	mutex_unlock(&info->mutex);

	return ret;
}

static int max77387_reg_raw_wr(
	struct max77387_info *info, u8 reg, u8 *buf, u8 num)
{
	int ret = -ENODEV;

	dev_dbg(info->dev, "%s %x = %x %x\n", __func__, reg, buf[0], buf[1]);
	mutex_lock(&info->mutex);
	if (info->power_is_on)
		ret = regmap_raw_write(info->regmap, reg, buf, num);
	else
		dev_err(info->dev, "%s: power is off.\n", __func__);
	mutex_unlock(&info->mutex);

	return ret;
}

static int max77387_reg_wr(
	struct max77387_info *info, u8 reg, u8 val, bool refresh)
{
	int ret = -ENODEV;

	dev_dbg(info->dev,
		"%s: %02x - %02x, %s %s\n", __func__, reg, val,
		info->regs.regs_stale ? "STALE" : "NONE",
		refresh ? "REFRESH" : "NONE");

	if (unlikely(!info->regs.regs_stale && !refresh))
		return 0;
	mutex_lock(&info->mutex);
	if (info->power_is_on)
		ret = regmap_write(info->regmap, reg, val);
	else
		dev_err(info->dev, "%s: power is off.\n", __func__);
	mutex_unlock(&info->mutex);

	return ret;
}

static int max77387_set_leds(struct max77387_info *info,
		u8 mask, u8 curr1, u8 curr2)
{
	struct max77387_settings *pst = &info->settings;
	int err = 0;
	u8 fled_en = 0;
	u8 regs[11];

	memset(regs, 0, sizeof(regs));

	if (info->op_mode == MAXFLASH_MODE_NONE) {
		err = max77387_reg_raw_wr(
			info, MAX77387_RW_FLASH_FLED1CURR, regs, 5);
		if (!err) {
			info->regs.led1_fcurr = 0;
			info->regs.led2_fcurr = 0;
			info->regs.led1_tcurr = 0;
			info->regs.led2_tcurr = 0;
			info->regs.leds_en = 0;
			info->regs.regs_stale = false;
		}
		goto set_leds_end;
	}

	if (mask & 1) {
		if (info->op_mode == MAXFLASH_MODE_FLASH) {
			if (curr1 > info->max_flash[0])
				curr1 = info->max_flash[0];
			fled_en |= (pst->fled_trig & FLASH_TRIG_MASK);
			regs[0] = FLASH_CURRENT(curr1);
		} else {
			if (curr1 > info->max_torch[0])
				curr1 = info->max_torch[0];
			fled_en |= (pst->fled_trig & TORCH_TRIG_MASK);
			regs[2] = TORCH_CURRENT(curr1);
		}
	}

	if (mask & 2) {
		if (info->op_mode == MAXFLASH_MODE_FLASH) {
			if (curr2 > info->max_flash[1])
				curr2 = info->max_flash[1];
			fled_en |= (pst->fled_trig & FLASH_TRIG_MASK);
			regs[1] = FLASH_CURRENT(curr2);
		} else {
			if (curr2 > info->max_torch[1])
				curr2 = info->max_torch[1];
			fled_en |= (pst->fled_trig & TORCH_TRIG_MASK);
			regs[3] = TORCH_CURRENT(curr2);
		}
	}

	/* if any led is set as flash, update the flash timer register */
	if (info->op_mode == MAXFLASH_MODE_FLASH)
		regs[9] = (info->ftimer_mode & FIELD(TIMER_MAX, 7)) |
			FIELD((info->new_timer & MAX77387_FTIMER_MASK), 0);
	else
		regs[10] = (info->ttimer_mode & FIELD(TTIMER_DIS, 7)) |
			FIELD((info->new_timer & MAX77387_TTIMER_MASK), 2);

	regs[4] = fled_en;
	regs[5] = pst->tx1_mask;
	regs[6] = pst->tx2_mask;
	regs[7] = pst->flash_ramp;
	regs[8] = pst->torch_ramp;
	if ((info->regs.led1_fcurr != regs[0]) ||
		(info->regs.led2_fcurr != regs[1]) ||
		(info->regs.led1_tcurr != regs[2]) ||
		(info->regs.led2_tcurr != regs[3]) ||
		(info->regs.t_timer != regs[10]) ||
		(info->regs.f_timer != regs[9]) ||
		(info->regs.leds_en != regs[4]))
		info->regs.regs_stale = true;

	if (info->regs.regs_stale) {
		err = max77387_reg_raw_wr(
			info, MAX77387_RW_FLASH_FLED1CURR, regs, sizeof(regs));

		if (!err) {
			info->regs.led1_fcurr = regs[0];
			info->regs.led2_fcurr = regs[1];
			info->regs.led1_tcurr = regs[2];
			info->regs.led2_tcurr = regs[3];
			info->regs.t_timer = regs[10];
			info->regs.f_timer = regs[9];
			info->regs.leds_en = regs[4];
			info->regs.regs_stale = false;
		}
	}

set_leds_end:
	if (err)
		dev_err(info->dev, "%s ERROR: %d\n", __func__, err);
	else
		dev_dbg(info->dev,
			"%s led %x f: %02x %02x %02x, t: %02x %02x %02x, %x\n",
			__func__, mask, curr1, curr2, info->regs.f_timer,
			info->regs.led1_tcurr, info->regs.led2_tcurr,
			info->regs.t_timer, fled_en);
	return err;
}

static void max77387_update_config(struct max77387_info *info)
{
	struct max77387_settings *pst = &info->settings;
	struct max77387_config *pcfg = &info->config;
	struct max77387_config *pcfg_cust;
	int i;
	int delta;

	dev_dbg(info->dev, "%s +++\n", __func__);
	dev_dbg(info->dev, "max77387_def_flash_levels:\n");
	for (i = 0; i < ARRAY_SIZE(max77387_def_flash_levels); i++) {
		max77387_def_flash_levels[i].guidenum = i;
		max77387_def_flash_levels[i].luminance =
			GET_CURRENT_UA_INDEX(i + 1);
		dev_dbg(info->dev, "0x%02x - %d\n",
			i, max77387_def_flash_levels[i].luminance);
	}

	dev_dbg(info->dev, "max77387_flash_timer:\n");
	delta = 1024;
	for (i = 8; i < ARRAY_SIZE(max77387_flash_timer); i++) {
		max77387_flash_timer[i] = max77387_flash_timer[i - 1] + delta;
		if (i >= 0x3f)
			delta = 8192;
		else if (i >= 0x1f)
			delta = 4096;
		else if (i >= 0x0f)
			delta = 2048;
		dev_dbg(info->dev,
			"0x%02x - %06d\n", i, max77387_flash_timer[i]);
	}

	dev_dbg(info->dev, "max77387_torch_timer:\n");
	delta = 131072;
	for (i = 1; i < ARRAY_SIZE(max77387_torch_timer) - 1; i++) {
		max77387_torch_timer[i] = max77387_torch_timer[i - 1] + delta;
		if (i >= 0x0f)
			delta = 1048576;
		else if (i >= 0x7)
			delta = 524288;
		else if (i >= 3)
			delta = 262144;
		dev_dbg(info->dev,
			"0x%02x - %08d\n", i, max77387_torch_timer[i]);
	}
	max77387_torch_timer[ARRAY_SIZE(max77387_torch_timer) - 1] =
						MAX77387_TORCH_TIMER_FOREVER;

	memcpy(pcfg, &max77387_default_pdata.config, sizeof(*pcfg));
	if (!info->pdata) {
		info->pdata = &max77387_default_pdata;
		dev_dbg(info->dev, "%s No platform data.  Using defaults.\n",
			__func__);
		goto update_end;
	}
	pcfg_cust = &info->pdata->config;

	if (pcfg_cust->flash_trigger_mode)
		pcfg->flash_trigger_mode = pcfg_cust->flash_trigger_mode;

	if (pcfg_cust->torch_trigger_mode)
		pcfg->torch_trigger_mode = pcfg_cust->torch_trigger_mode;

	if (pcfg_cust->led_mask)
		pcfg->led_mask = pcfg_cust->led_mask;

	if (pcfg_cust->synchronized_led)
		pcfg->synchronized_led = pcfg_cust->synchronized_led;

	if (pcfg_cust->flash_mode)
		pcfg->flash_mode = pcfg_cust->flash_mode;

	if (pcfg_cust->torch_mode)
		pcfg->torch_mode = pcfg_cust->torch_mode;

	if (pcfg_cust->adaptive_mode)
		pcfg->adaptive_mode = pcfg_cust->adaptive_mode;

	if (pcfg_cust->max_total_current_mA)
		pcfg->max_total_current_mA = pcfg_cust->max_total_current_mA;

	if (pcfg_cust->max_peak_current_mA)
		pcfg->max_peak_current_mA = pcfg_cust->max_peak_current_mA;

	if (pcfg_cust->max_peak_duration_ms)
		pcfg->max_peak_duration_ms = pcfg_cust->max_peak_duration_ms;

	if (pcfg_cust->max_torch_current_mA)
		pcfg->max_torch_current_mA = pcfg_cust->max_torch_current_mA;

	if (pcfg_cust->max_flash_threshold_mV)
		pcfg->max_flash_threshold_mV =
				pcfg_cust->max_flash_threshold_mV;

	if (pcfg_cust->max_flash_hysteresis_mV)
		pcfg->max_flash_hysteresis_mV =
				pcfg_cust->max_flash_hysteresis_mV;

	if (pcfg_cust->max_flash_lbdly_f_uS)
		pcfg->max_flash_lbdly_f_uS =
				pcfg_cust->max_flash_lbdly_f_uS;

	if (pcfg_cust->max_flash_lbdly_r_uS)
		pcfg->max_flash_lbdly_r_uS =
				pcfg_cust->max_flash_lbdly_r_uS;

	if (pcfg_cust->tx1_mask_mA)
		pcfg->tx1_mask_mA = pcfg_cust->tx1_mask_mA;

	if (pcfg_cust->tx2_mask_mA)
		pcfg->tx2_mask_mA = pcfg_cust->tx2_mask_mA;

	if (pcfg_cust->flash_rampup_uS)
		pcfg->flash_rampup_uS = pcfg_cust->flash_rampup_uS;

	if (pcfg_cust->flash_rampdn_uS)
		pcfg->flash_rampdn_uS = pcfg_cust->flash_rampdn_uS;

	if (pcfg_cust->torch_rampup_uS)
		pcfg->torch_rampup_uS = pcfg_cust->torch_rampup_uS;

	if (pcfg_cust->torch_rampdn_uS)
		pcfg->torch_rampdn_uS = pcfg_cust->torch_rampdn_uS;

	if (pcfg_cust->def_ftimer)
		pcfg->torch_rampdn_uS = pcfg_cust->def_ftimer;

	for (i = 0; i < MAX77387_LED_NUM; i++) {
		if (pcfg_cust->led_config[i].flash_levels &&
			pcfg_cust->led_config[i].flash_torch_ratio &&
			pcfg_cust->led_config[i].granularity &&
			pcfg_cust->led_config[i].lumi_levels)
			memcpy(&pcfg->led_config[i], &pcfg_cust->led_config[i],
				sizeof(pcfg_cust->led_config[0]));
		else
			dev_notice(info->dev,
				"%s: invalid led config[%d].\n", __func__, i);
	}

update_end:
	/* FLED enable settings */
	/* How TORCH is triggered */
	switch (pcfg->torch_trigger_mode) {
	case 1: /* triggered by FLASHEN */
		pst->fled_trig = TORCH_TRIG_BY_FLASHEN;
		break;
	case 2: /* triggered by TORCHEN */
		pst->fled_trig = TORCH_TRIG_BY_TORCHEN;
		break;
	case 3: /* triggered by serial interface */
		pst->fled_trig = TORCH_TRIG_BY_I2C;
		break;
	case 4: /* triggered by FLASHEN and TORCHEN */
		pst->fled_trig = TORCH_TRIG_BY_FANDT;
		break;
	case 5: /* triggered by FLASHEN or TORCHEN */
		pst->fled_trig = TORCH_TRIG_BY_FORT;
		break;
	default:
		dev_err(info->dev, "%s: unrecognized torch trigger mode.\n",
			__func__);
		dev_err(info->dev, "use default i2c mode.\n");
		pst->fled_trig = TORCH_TRIG_BY_I2C;
		break;
	}

	/* How FLASH is triggered */
	switch (pcfg->flash_trigger_mode) {
	case 1: /* triggered by FLASHEN */
		pst->fled_trig |= FLASH_TRIG_BY_FLASHEN;
		break;
	case 2: /* triggered by TORCHEN */
		pst->fled_trig |= FLASH_TRIG_BY_TORCHEN;
		break;
	case 3: /* triggered by serial interface */
		pst->fled_trig |= FLASH_TRIG_BY_I2C;
		break;
	case 4: /* triggered by FLASHEN and TORCHEN */
		pst->fled_trig |= FLASH_TRIG_BY_FANDT;
		break;
	case 5: /* triggered by FLASHEN or TORCHEN */
		pst->fled_trig |= FLASH_TRIG_BY_FORT;
		break;
	default:
		dev_err(info->dev, "%s: unrecognized flash trigger mode.\n",
			__func__);
		dev_err(info->dev, "use default i2c mode.\n");
		pst->fled_trig |= FLASH_TRIG_BY_I2C;
		break;
	}


	info->ftimer_mode = (pcfg->flash_mode == 1) ?
			FIELD(TIMER_ONESHOT, 7) : FIELD(TIMER_MAX, 7);

	switch (pcfg->torch_mode) {
	case 1:
		info->ttimer_mode = FIELD(TTIMER_DIS, 7) |
					FIELD(TORCH_TIMER_SAFETY_DIS, 6);
		break;
	case 2:
		info->ttimer_mode = FIELD(TIMER_ONESHOT, 7);
		break;
	case 3:
	default:
		info->ttimer_mode = FIELD(TTIMER_DIS, 7);
		break;
	}

	if (pcfg->max_flash_threshold_mV) {
		if (pcfg->max_flash_threshold_mV < MAXFLASH_V_TH_FLOOR)
			pcfg->max_flash_threshold_mV = MAXFLASH_V_TH_FLOOR;
		else if (pcfg->max_flash_threshold_mV > MAXFLASH_V_TH_CEILING)
			pcfg->max_flash_threshold_mV = MAXFLASH_V_TH_CEILING;

		/* 0 - hysteresis disabled */
		if (pcfg->max_flash_hysteresis_mV) {
			if (pcfg->max_flash_hysteresis_mV <
				MAXFLASH_VOLT_HYS_FLOOR)
				pcfg->max_flash_hysteresis_mV =
					MAXFLASH_VOLT_HYS_FLOOR;
			else if (pcfg->max_flash_hysteresis_mV >
				 MAXFLASH_VOLT_HYS_CEILING)
				pcfg->max_flash_hysteresis_mV =
					MAXFLASH_VOLT_HYS_CEILING;
		}

		pst->max_flash1 = FIELD(MAXFLASH_ENABLE, 7) |
		((pcfg->max_flash_threshold_mV - MAXFLASH_V_TH_FLOOR) /
				MAXFLASH_V_TH_STEP);
		pst->max_flash1 |= (pcfg->max_flash_hysteresis_mV +
		MAXFLASH_VOLT_HYS_STEP / 2) / MAXFLASH_VOLT_HYS_STEP;
	}

	if (pcfg->max_flash_lbdly_f_uS)
		pst->max_flash1 =
		FIELD(pcfg->max_flash_lbdly_f_uS / MAXFLASH_TIMER_STEP, 0);

	if (pcfg->max_flash_lbdly_r_uS)
		pst->max_flash1 |=
		FIELD(pcfg->max_flash_lbdly_r_uS / MAXFLASH_TIMER_STEP, 3);
}

static int max77387_update_settings(struct max77387_info *info)
{
	struct max77387_settings *pst = &info->settings;
	int err = 0;
	u8 regs[8];

	info->regs.regs_stale = true;
	regs[0] = pst->tx1_mask;
	regs[1] = pst->tx2_mask;
	regs[2] = pst->flash_ramp;
	regs[3] = pst->torch_ramp;
	regs[4] = info->regs.f_timer;
	regs[5] = info->regs.t_timer;
	regs[6] = pst->max_flash1;
	regs[7] = pst->max_flash2;
	err = max77387_reg_raw_wr(
		info, MAX77387_RW_TX1_MASK, regs, sizeof(regs));

	regs[0] = pst->ntc;
	regs[1] = pst->dcdc_cntl1;
	regs[2] = pst->dcdc_cntl2;
	regs[3] = pst->dcdc_lim;
	err = max77387_reg_raw_wr(info, MAX77387_RW_NTC, regs, 5);

	if (info->op_mode == MAXFLASH_MODE_FLASH)
		err |= max77387_set_leds(info, info->config.led_mask,
				info->regs.led1_fcurr, info->regs.led2_fcurr);
	else
		err |= max77387_set_leds(info, info->config.led_mask,
				info->regs.led1_tcurr, info->regs.led2_tcurr);

	info->regs.regs_stale = false;
	return err;
}

static int max77387_configure(struct max77387_info *info, bool update)
{
	struct max77387_settings *pst = &info->settings;
	struct max77387_config *pcfg = &info->config;
	struct nvc_torch_capability_query *pqry = &info->query;
	struct nvc_torch_flash_capabilities_v1	*pfcap = NULL;
	struct nvc_torch_torch_capabilities_v1	*ptcap = NULL;
	struct nvc_torch_timer_capabilities_v1	*ptmcap = NULL;
	struct nvc_torch_lumi_level_v1		*plvls = NULL;
	int val, i, j;

	if (pcfg->max_peak_current_mA > max77387_caps.max_peak_curr_mA ||
		!pcfg->max_peak_current_mA) {
		dev_notice(info->dev, "invalid max_peak_current_mA: %d,",
				pcfg->max_peak_current_mA);
		dev_notice(info->dev, " changed to %d\n",
				max77387_caps.max_peak_curr_mA);
		pcfg->max_peak_current_mA = max77387_caps.max_peak_curr_mA;
	}

	pst->tx1_mask = 0;
	if (pcfg->tx1_mask_mA) {
		if (pcfg->tx1_mask_mA > 1000) {
			dev_notice(info->dev, "%s: tx1_mask OUT OF RANGE. %d\n",
				__func__, pcfg->tx1_mask_mA);
			pcfg->tx1_mask_mA = 1000;
		}
		pst->tx1_mask = (u8)((u32)pcfg->tx1_mask_mA * 1000 / 15625)
					| MAX77387_TX_MASK_ENABLE;
	}

	pst->tx1_mask = 0;
	if (pcfg->tx2_mask_mA) {
		if (pcfg->tx2_mask_mA > 1000) {
			dev_notice(info->dev, "%s: tx2_mask OUT OF RANGE. %d\n",
				__func__, pcfg->tx2_mask_mA);
			pcfg->tx2_mask_mA = 1000;
		}
		pst->tx2_mask = (u8)((u32)pcfg->tx2_mask_mA * 1000 / 15625)
					| MAX77387_TX_MASK_ENABLE;
	}

	if (pcfg->flash_rampup_uS > 32896) {
		dev_notice(info->dev, "%s: flash ramp up OUT OF RANGE. %d\n",
				__func__, pcfg->flash_rampup_uS);
		pcfg->flash_rampup_uS = 32896;
	}
	if (pcfg->flash_rampdn_uS > 32896) {
		dev_notice(info->dev, "%s: flash ramp up OUT OF RANGE. %d\n",
				__func__, pcfg->flash_rampdn_uS);
		pcfg->flash_rampdn_uS = 32896;
	}
	pst->flash_ramp = ((pcfg->flash_rampup_uS / 384) << 4) |
				pcfg->flash_rampdn_uS / 384;

	if (pcfg->torch_rampup_uS > 32896) {
		dev_notice(info->dev, "%s: torch ramp up OUT OF RANGE. %d\n",
				__func__, pcfg->torch_rampup_uS);
		pcfg->torch_rampup_uS = 32896;
	}
	if (pcfg->torch_rampdn_uS > 32896) {
		dev_notice(info->dev, "%s: torch ramp up OUT OF RANGE. %d\n",
				__func__, pcfg->torch_rampdn_uS);
		pcfg->torch_rampdn_uS = 32896;
	}
	pst->torch_ramp = ((pcfg->torch_rampup_uS / 384) << 4) |
				pcfg->torch_rampdn_uS / 384;

	/* number of leds enabled */
	i = 1;
	/* in synchronize mode, both leds are considered as 1 */
	if (!pcfg->synchronized_led && (info->config.led_mask & 3) == 3)
		i = 2;
	pqry->flash_num = i;
	pqry->torch_num = i;

	val = pcfg->max_peak_current_mA * i;
	if (val > max77387_caps.max_total_current_mA)
		val = max77387_caps.max_total_current_mA;

	if (!pcfg->max_total_current_mA || pcfg->max_total_current_mA > val)
		pcfg->max_total_current_mA = val;
	pcfg->max_peak_current_mA =
		pcfg->max_total_current_mA / i;

	if (pcfg->max_torch_current_mA > max77387_caps.max_torch_curr_mA ||
		!pcfg->max_torch_current_mA) {
		dev_notice(info->dev, "invalid max_torch_current_mA: %d,",
				pcfg->max_torch_current_mA);
		dev_notice(info->dev, " changed to %d\n",
				max77387_caps.max_torch_curr_mA);
		pcfg->max_torch_current_mA =
			max77387_caps.max_torch_curr_mA;
	}

	pqry->version = NVC_TORCH_CAPABILITY_VER_1;
	pqry->led_attr = 0;
	for (i = 0; i < pqry->flash_num; i++) {
		pfcap = info->flash_cap[i];
		pfcap->version = NVC_TORCH_CAPABILITY_VER_1;
		pfcap->led_idx = i;
		pfcap->attribute = 0;
		pfcap->granularity = pcfg->led_config[i].granularity;
		pfcap->timeout_num = ARRAY_SIZE(max77387_flash_timer);
		ptmcap = info->flash_timeouts[i];
		pfcap->timeout_off = (void *)ptmcap - (void *)pfcap;
		pfcap->flash_torch_ratio =
				pcfg->led_config[i].flash_torch_ratio;

		plvls = pcfg->led_config[i].lumi_levels;
		pfcap->levels[0].guidenum = MAX77387_LEVEL_OFF;
		pfcap->levels[0].luminance = 0;
		for (j = 1; j < pcfg->led_config[i].flash_levels + 1; j++) {
			if (GET_CURRENT_BY_INDEX(plvls[j - 1].guidenum) >
				pcfg->max_peak_current_mA)
				break;

			pfcap->levels[j].guidenum = plvls[j - 1].guidenum;
			pfcap->levels[j].luminance = plvls[j - 1].luminance;
			info->max_flash[i] = plvls[j - 1].guidenum;
			dev_dbg(info->dev, "%02d - %d\n",
				pfcap->levels[j].guidenum,
				pfcap->levels[j].luminance);
		}
		pfcap->numberoflevels = j;
		dev_dbg(info->dev,
			"%s flash#%d, attr: %x, levels: %d, g: %d, ratio: %d\n",
			__func__, pfcap->led_idx, pfcap->attribute,
			pfcap->numberoflevels, pfcap->granularity,
			pfcap->flash_torch_ratio);

		ptmcap->timeout_num = pfcap->timeout_num;
		for (j = 0; j < ptmcap->timeout_num; j++) {
			ptmcap->timeouts[j].timeout = max77387_flash_timer[j];
			dev_dbg(info->dev, "t: %02d - %d uS\n", j,
				ptmcap->timeouts[j].timeout);
		}
	}

	for (i = 0; i < pqry->torch_num; i++) {
		ptcap = info->torch_cap[i];
		ptcap->version = NVC_TORCH_CAPABILITY_VER_1;
		ptcap->led_idx = i;
		ptcap->attribute = 0;
		ptcap->granularity = pcfg->led_config[i].granularity;
		ptcap->timeout_num = ARRAY_SIZE(max77387_torch_timer);
		ptmcap = info->torch_timeouts[i];
		ptcap->timeout_off = (void *)ptmcap - (void *)ptcap;

		plvls = pcfg->led_config[i].lumi_levels;
		ptcap->levels[0].guidenum = MAX77387_LEVEL_OFF;
		ptcap->levels[0].luminance = 0;
		for (j = 1; j < ptcap->numberoflevels; j++) {
			if (GET_CURRENT_BY_INDEX(plvls[j - 1].guidenum) >
				pcfg->max_torch_current_mA)
				break;

			ptcap->levels[j].guidenum = plvls[j - 1].guidenum;
			ptcap->levels[j].luminance = plvls[j - 1].luminance;
			info->max_torch[i] = plvls[j - 1].guidenum;
			dev_dbg(info->dev, "%02d - %d\n",
				ptcap->levels[j].guidenum,
				ptcap->levels[j].luminance);
		}
		ptcap->numberoflevels = j;
		dev_dbg(info->dev, "torch#%d, attr: %x, levels: %d, g: %d\n",
			ptcap->led_idx, ptcap->attribute,
			ptcap->numberoflevels, ptcap->granularity);

		ptmcap->timeout_num = ptcap->timeout_num;
		for (j = 0; j < ptmcap->timeout_num; j++) {
			ptmcap->timeouts[j].timeout = max77387_torch_timer[j];
			dev_dbg(info->dev, "t: %02d - %d uS\n", j,
				ptmcap->timeouts[j].timeout);
		}
	}

	if (update && (info->pwr_state == NVC_PWR_COMM ||
			info->pwr_state == NVC_PWR_ON))
		return max77387_update_settings(info);

	return 0;
}

static int max77387_strobe(struct max77387_info *info, int t_on)
{
	u32 gpio = info->pdata->gpio_strobe & 0xffff;
	u32 lact = (info->pdata->gpio_strobe & 0xffff0000) ? 1 : 0;
	return gpio_direction_output(gpio, lact ^ (t_on & 1));
}

static int max77387_enter_offmode(struct max77387_info *info, bool op_off)
{
	int err = 0;

	if (op_off) {
		if (info->power_is_on) {
			info->op_mode = MAXFLASH_MODE_NONE;
			err = max77387_set_leds(info, 3, 0, 0);
		}
	} else {
		err = max77387_reg_wr(
			info, MAX77387_RW_FLED_MODE, 0, true);
	}
	return err;
}

#ifdef CONFIG_PM
static int max77387_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct max77387_info *info = i2c_get_clientdata(client);

	dev_info(info->dev, "Suspending\n");
	info->regs.regs_stale = true;

	return 0;
}

static int max77387_resume(struct i2c_client *client)
{
	struct max77387_info *info = i2c_get_clientdata(client);

	dev_info(info->dev, "Resuming\n");
	info->regs.regs_stale = true;

	return 0;
}

static void max77387_shutdown(struct i2c_client *client)
{
	struct max77387_info *info = i2c_get_clientdata(client);

	dev_info(info->dev, "Shutting down\n");

	max77387_enter_offmode(info, true);
	info->regs.regs_stale = true;
}
#endif

static int max77387_power_off(struct max77387_info *info)
{
	struct max77387_power_rail *pw = &info->pwr_rail;
	int err = 0;

	if (!info->power_is_on)
		return 0;

	mutex_lock(&info->mutex);

	if (info->pdata && info->pdata->poweroff_callback)
		err = info->pdata->poweroff_callback(pw);

	if (IS_ERR_VALUE(err)) {
		mutex_unlock(&info->mutex);
		return err;
	}

	/* the call back function already handles the power off sequence */
	if (err)
		err = 0;
	else {
		if (pw->vin)
			regulator_disable(pw->vin);
		if (pw->vdd)
			regulator_disable(pw->vdd);

		info->power_is_on = 0;
	}

	mutex_unlock(&info->mutex);
	return err;
}

static int max77387_power_on(struct max77387_info *info)
{
	struct max77387_power_rail *pw = &info->pwr_rail;
	int err = 0;

	if (info->power_is_on)
		return 0;

	mutex_lock(&info->mutex);

	if (info->pdata && info->pdata->poweron_callback)
		err = info->pdata->poweron_callback(pw);

	if (IS_ERR_VALUE(err))
		goto max77387_poweron_callback_fail;

	/* the call back function already handles the power on sequence */
	if (err)
		err = 0;
	else {
		if (pw->vdd) {
			err = regulator_enable(pw->vdd);
			if (err) {
				dev_err(info->dev, "%s vdd err\n", __func__);
				goto max77387_poweron_vdd_fail;
			}
		}
		if (pw->vin) {
			err = regulator_enable(pw->vin);
			if (err) {
				dev_err(info->dev, "%s vin err\n", __func__);
				goto max77387_poweron_vin_fail;
			}
		}
	}

	info->power_is_on = 1;

	mutex_unlock(&info->mutex);

	err = max77387_update_settings(info);
	if (err) {
		max77387_power_off(info);
		return err;
	}

	return 0;

max77387_poweron_vin_fail:
	if (pw->vdd)
		regulator_disable(pw->vdd);
max77387_poweron_vdd_fail:
	if (info->pdata && info->pdata->poweroff_callback)
		info->pdata->poweroff_callback(pw);
max77387_poweron_callback_fail:
	mutex_unlock(&info->mutex);
	return err;
}

static int max77387_power_set(struct max77387_info *info, int pwr)
{
	int err = 0;

	if (pwr == info->pwr_state)
		return 0;

	switch (pwr) {
	case NVC_PWR_OFF:
		max77387_enter_offmode(info, true);
		if ((info->pdata->cfg & NVC_CFG_OFF2STDBY) ||
			(info->pdata->cfg & NVC_CFG_BOOT_INIT))
			pwr = NVC_PWR_STDBY;
		else
			err = max77387_power_off(info);
		break;
	case NVC_PWR_STDBY_OFF:
		if ((info->pdata->cfg & NVC_CFG_OFF2STDBY) ||
			(info->pdata->cfg & NVC_CFG_BOOT_INIT))
			pwr = NVC_PWR_STDBY;
		else
			err = max77387_power_on(info);
		break;
	case NVC_PWR_STDBY:
		err = max77387_power_on(info);
		err |= max77387_enter_offmode(info, false);
		break;

	case NVC_PWR_COMM:
	case NVC_PWR_ON:
		err = max77387_power_on(info);
		break;

	default:
		err = -EINVAL;
		break;
	}

	if (err < 0) {
		dev_err(info->dev, "%s error\n", __func__);
		pwr = NVC_PWR_ERR;
	}
	info->pwr_state = pwr;
	if (err > 0)
		return 0;

	return err;
}

static inline int max77387_power_user_set(
	struct max77387_info *info, int pwr)
{
	int err = 0;

	if (!pwr || (pwr > NVC_PWR_ON))
		return 0;

	err = max77387_power_set(info, pwr);
	if (info->pdata->cfg & NVC_CFG_NOERR)
		return 0;

	return err;
}

static int max77387_dev_id(struct max77387_info *info)
{
	int err = 0;

	if (info->chip_id)
		return 0;
	err = max77387_power_on(info);
	if (err)
		return err;

	err = max77387_reg_raw_rd(info, MAX77387_RO_CHIPID1,
			(u8 *)&info->chip_id, sizeof(info->chip_id));
	if (!err)
		dev_info(info->dev, "%s: 0x%x detected.\n",
			__func__, info->chip_id);
	else
		dev_dbg(info->dev, "%s: not detected.\n", __func__);

	err = max77387_power_off(info);
	return err;
}

static int max77387_get_param(struct max77387_info *info, long arg)
{
	struct nvc_param params;
	struct nvc_torch_pin_state pinstate;
	const void *data_ptr = NULL;
	u32 data_size = 0;
	int err = 0;
	u8 reg;

	if (copy_from_user(&params, (const void __user *)arg,
			sizeof(struct nvc_param))) {
		dev_err(info->dev, "%s %d copy_from_user err\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	switch (params.param) {
	case NVC_PARAM_TORCH_QUERY:
		dev_dbg(info->dev, "%s QUERY\n", __func__);
		data_ptr = &info->query;
		data_size = sizeof(info->query);
		break;
	case NVC_PARAM_FLASH_EXT_CAPS:
		dev_dbg(info->dev, "%s EXT_FLASH_CAPS %d\n",
			__func__, params.variant);
		if (params.variant >= info->query.flash_num) {
			dev_err(info->dev, "%s unsupported flash index.\n",
				__func__);
			err = -EINVAL;
			break;
		}
		data_ptr = info->flash_cap[params.variant];
		data_size = info->flash_cap_size;
		break;
	case NVC_PARAM_TORCH_EXT_CAPS:
		dev_dbg(info->dev, "%s EXT_TORCH_CAPS %d\n",
			__func__, params.variant);
		if (params.variant >= info->query.torch_num) {
			dev_err(info->dev, "%s unsupported torch index.\n",
				__func__);
			err = -EINVAL;
			break;
		}
		data_ptr = info->torch_cap[params.variant];
		data_size = info->torch_cap_size;
		break;
	case NVC_PARAM_FLASH_LEVEL:
		if (params.variant >= info->query.flash_num) {
			dev_err(info->dev,
				"%s unsupported flash index.\n", __func__);
			err = -EINVAL;
			break;
		}
		if (params.variant > 0)
			reg = info->regs.led2_fcurr;
		else
			reg = info->regs.led1_fcurr;
		data_ptr = &reg;
		data_size = sizeof(reg);
		dev_dbg(info->dev, "%s FLASH_LEVEL %d\n", __func__, reg);
		break;
	case NVC_PARAM_TORCH_LEVEL:
		reg = info->regs.led1_tcurr;
		if (params.variant >= info->query.torch_num) {
			dev_err(info->dev, "%s unsupported torch index.\n",
				__func__);
			err = -EINVAL;
			break;
		}
		if (params.variant > 0)
			reg >>= 4;
		else
			reg &= 0x0F;
		data_ptr = &reg;
		data_size = sizeof(reg);
		dev_dbg(info->dev, "%s TORCH_LEVEL %d\n", __func__, reg);
		break;
	case NVC_PARAM_FLASH_PIN_STATE:
		pinstate = info->pdata->pinstate;
		if (info->op_mode != MAXFLASH_MODE_FLASH)
			pinstate.values ^= 0xffff;
		pinstate.values &= pinstate.mask;

		dev_dbg(info->dev, "%s FLASH_PIN_STATE: %x & %x\n",
				__func__, pinstate.mask, pinstate.values);
		data_ptr = &pinstate;
		data_size = sizeof(pinstate);
		break;
	default:
		dev_err(info->dev, "%s unsupported parameter: %d\n",
				__func__, params.param);
		err = -EINVAL;
	}

	dev_dbg(info->dev, "%s data size user %d vs local %d\n",
			__func__, params.sizeofvalue, data_size);
	if (!err && params.sizeofvalue < data_size) {
		dev_err(info->dev, "%s data size mismatch\n", __func__);
		err = -EINVAL;
	}

	if (!err && copy_to_user((void __user *)params.p_value,
			 data_ptr, data_size)) {
		dev_err(info->dev, "%s copy_to_user err line %d\n",
				__func__, __LINE__);
		err = -EFAULT;
	}

	return err;
}

static int max77387_get_levels(struct max77387_info *info,
			       struct nvc_param *params,
			       bool flash_mode,
			       struct nvc_torch_set_level_v1 *plevels)
{
	struct nvc_torch_timer_capabilities_v1 *p_tm;
	u8 op_mode;

	if (copy_from_user(plevels, (const void __user *)params->p_value,
			   sizeof(*plevels))) {
		dev_err(info->dev, "%s %d copy_from_user err\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	if (flash_mode) {
		dev_dbg(info->dev, "%s FLASH_LEVEL: %d %d %d\n",
			__func__, plevels->ledmask,
			plevels->levels[0], plevels->levels[1]);
		p_tm = info->flash_timeouts[0];
		op_mode = MAXFLASH_MODE_FLASH;
	} else {
		dev_dbg(info->dev, "%s TORCH_LEVEL: %d %d %d\n",
			__func__, plevels->ledmask,
			plevels->levels[0], plevels->levels[1]);
		p_tm = info->torch_timeouts[0];
		op_mode = MAXFLASH_MODE_TORCH;
	}

	if (plevels->timeout) {
		u16 i;
		for (i = 0; i < p_tm->timeout_num; i++) {
			plevels->timeout = i;
			if (plevels->timeout == p_tm->timeouts[i].timeout)
				break;
		}
	} else
		plevels->timeout = p_tm->timeout_num - 1;

	if (plevels->levels[0] == MAX77387_LEVEL_OFF)
		plevels->ledmask &= ~1;
	if (plevels->levels[1] == MAX77387_LEVEL_OFF)
		plevels->ledmask &= ~2;
	plevels->ledmask &= info->config.led_mask;

	if (!plevels->ledmask)
		info->op_mode = MAXFLASH_MODE_NONE;
	else {
		info->op_mode = op_mode;
		if (info->config.synchronized_led) {
			plevels->ledmask = 3;
			plevels->levels[1] = plevels->levels[0];
		}
	}

	dev_dbg(info->dev, "Return: %d - %d %d %d\n", info->op_mode,
		plevels->ledmask, plevels->levels[0], plevels->levels[1]);
	return 0;
}

static int max77387_set_param(struct max77387_info *info, long arg)
{
	struct nvc_param params;
	struct nvc_torch_set_level_v1 led_levels;
	u8 curr1;
	u8 curr2;
	u8 val;
	int err = 0;

	if (copy_from_user(
		&params, (const void __user *)arg, sizeof(struct nvc_param))) {
		dev_err(info->dev, "%s %d copy_from_user err\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	switch (params.param) {
	case NVC_PARAM_FLASH_LEVEL:
		max77387_get_levels(info, &params, true, &led_levels);
		if (led_levels.timeout == 0)
			info->new_timer = info->config.def_ftimer;
		else
			info->new_timer = led_levels.timeout;
		curr1 = led_levels.levels[0];
		curr2 = led_levels.levels[1];
		err = max77387_set_leds(info,
			led_levels.ledmask, curr1, curr2);
		break;
	case NVC_PARAM_TORCH_LEVEL:
		max77387_get_levels(info, &params, false, &led_levels);
		info->new_timer = led_levels.timeout;
		curr1 = led_levels.levels[0];
		curr2 = led_levels.levels[1];
		err = max77387_set_leds(info,
			led_levels.ledmask, curr1, curr2);
		break;
	case NVC_PARAM_FLASH_PIN_STATE:
		if (copy_from_user(&val, (const void __user *)params.p_value,
			   sizeof(val))) {
			dev_err(info->dev, "%s %d copy_from_user err\n",
				__func__, __LINE__);
			err = -EINVAL;
			break;
		}
		dev_dbg(info->dev, "%s FLASH_PIN_STATE: %d\n",
				__func__, val);
		err = max77387_strobe(info, val);
		break;
	default:
		dev_err(info->dev, "%s unsupported parameter: %d\n",
				__func__, params.param);
		err = -EINVAL;
		break;
	}

	return err;
}

static long max77387_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	struct max77387_info *info = file->private_data;
	int pwr;
	int err = 0;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(NVC_IOCTL_PARAM_WR):
		err = max77387_set_param(info, arg);
		break;
	case _IOC_NR(NVC_IOCTL_PARAM_RD):
		err = max77387_get_param(info, arg);
		break;
	case _IOC_NR(NVC_IOCTL_PWR_WR):
		/* This is a Guaranteed Level of Service (GLOS) call */
		pwr = (int)arg * 2;
		dev_dbg(info->dev, "%s PWR_WR: %d\n", __func__, pwr);
		err = max77387_power_user_set(info, pwr);
		break;
	case _IOC_NR(NVC_IOCTL_PWR_RD):
		pwr = info->pwr_state / 2;
		dev_dbg(info->dev, "%s PWR_RD: %d\n", __func__, pwr);
		if (copy_to_user(
			(void __user *)arg, (const void *)&pwr, sizeof(pwr))) {
			dev_err(info->dev, "%s copy_to_user err line %d\n",
					__func__, __LINE__);
			err = -EFAULT;
		}
		break;
	default:
		dev_err(info->dev, "%s unsupported ioctl: %x\n",
				__func__, cmd);
		err = -EINVAL;
		break;
	}

	return err;
}

static int max77387_open(struct inode *inode, struct file *file)
{
	struct miscdevice	*miscdev = file->private_data;
	struct max77387_info *info;

	info = container_of(miscdev, struct max77387_info, miscdev);
	if (!info)
		return -ENODEV;

	if (atomic_xchg(&info->in_use, 1))
		return -EBUSY;

	file->private_data = info;
	dev_dbg(info->dev, "%s\n", __func__);
	return 0;
}

static int max77387_release(struct inode *inode, struct file *file)
{
	struct max77387_info *info = file->private_data;

	dev_dbg(info->dev, "%s\n", __func__);
	max77387_power_set(info, NVC_PWR_OFF);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	return 0;
}

static int max77387_power_put(struct max77387_power_rail *pw)
{
	if (likely(pw->vin))
		regulator_put(pw->vin);

	if (likely(pw->vdd))
		regulator_put(pw->vdd);


	pw->vin = NULL;
	pw->vdd = NULL;

	return 0;
}

static int max77387_regulator_get(struct max77387_info *info,
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

static int max77387_power_get(struct max77387_info *info)
{
	struct max77387_power_rail *pw = &info->pwr_rail;
	int err;

	err = max77387_regulator_get(info, &pw->vin, "vin"); /* 3.3v */
	err |= max77387_regulator_get(info, &pw->vdd, "vdd"); /* 1.8v */
	info->pwr_state = NVC_PWR_OFF;

	return err;
};

static const struct file_operations max77387_fileops = {
	.owner = THIS_MODULE,
	.open = max77387_open,
	.unlocked_ioctl = max77387_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = max77387_ioctl,
#endif
	.release = max77387_release,
};

static void max77387_del(struct max77387_info *info)
{
	max77387_power_set(info, NVC_PWR_OFF);
	max77387_power_put(&info->pwr_rail);
}

static int max77387_remove(struct i2c_client *client)
{
	struct max77387_info *info = i2c_get_clientdata(client);

	dev_dbg(info->dev, "%s\n", __func__);
	misc_deregister(&info->miscdev);
	max77387_del(info);
	if (info->d_max77387)
		debugfs_remove_recursive(info->d_max77387);

	return 0;
}

static int max77387_debugfs_init(struct max77387_info *info);

static void max77387_caps_layout(struct max77387_info *info)
{
#define MAX77387_FLASH_CAP_TIMEOUT_SIZE \
	(max77387_flash_cap_size + max77387_flash_timeout_size)
#define MAX77387_TORCH_CAP_TIMEOUT_SIZE \
	(max77387_torch_cap_size + max77387_torch_timeout_size)
	void *start_ptr = (void *)info + sizeof(*info);

	info->flash_cap[0] = start_ptr;
	info->flash_timeouts[0] = start_ptr + max77387_flash_cap_size;

	start_ptr += MAX77387_FLASH_CAP_TIMEOUT_SIZE;
	info->flash_cap[1] = start_ptr;
	info->flash_timeouts[1] = start_ptr + max77387_flash_cap_size;

	info->flash_cap_size = MAX77387_FLASH_CAP_TIMEOUT_SIZE;

	start_ptr += MAX77387_FLASH_CAP_TIMEOUT_SIZE;
	info->torch_cap[0] = start_ptr;
	info->torch_timeouts[0] = start_ptr + max77387_torch_cap_size;

	start_ptr += MAX77387_TORCH_CAP_TIMEOUT_SIZE;
	info->torch_cap[1] = start_ptr;
	info->torch_timeouts[1] = start_ptr + max77387_torch_cap_size;

	info->torch_cap_size = MAX77387_TORCH_CAP_TIMEOUT_SIZE;
	dev_dbg(info->dev, "%s: %d(%d + %d), %d(%d + %d)\n", __func__,
		info->flash_cap_size, max77387_flash_cap_size,
		max77387_flash_timeout_size, info->torch_cap_size,
		max77387_torch_cap_size, max77387_torch_timeout_size);
}

static int max77387_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	struct max77387_info *info;

	dev_info(&client->dev, "%s\n", __func__);

	info = devm_kzalloc(&client->dev, sizeof(*info) +
			max77387_max_flash_cap_size +
			max77387_max_torch_cap_size,
			GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	info->regmap = devm_regmap_init_i2c(client, &max77387_regmap_config);
	if (IS_ERR(info->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(info->regmap));
		return -ENODEV;
	}

	info->i2c_client = client;
	info->dev = &client->dev;
	if (client->dev.platform_data) {
		info->pdata = client->dev.platform_data;
		dev_dbg(&client->dev, "pdata: %s\n", info->pdata->dev_name);
	} else
		dev_notice(&client->dev, "%s NO platform data\n", __func__);

	max77387_power_get(info);

	max77387_caps_layout(info);

	max77387_update_config(info);

	/* flash mode */
	info->op_mode = MAXFLASH_MODE_NONE;

	max77387_configure(info, false);

	i2c_set_clientdata(client, info);
	mutex_init(&info->mutex);

	max77387_dev_id(info);
	if ((info->pdata->cfg & NVC_CFG_NODEV) &&
		((info->chip_id & 0xff00) == 0x9100)) {
		max77387_del(info);
		return -ENODEV;
	}

	if (info->pdata->dev_name != NULL)
		strncpy(info->devname, info->pdata->dev_name,
			sizeof(info->devname) - 1);
	else
		strncpy(info->devname, "max77387", sizeof(info->devname) - 1);

	if (info->pdata->num)
		snprintf(info->devname, sizeof(info->devname), "%s.%u",
				info->devname, info->pdata->num);
	info->miscdev.name = info->devname;
	info->miscdev.fops = &max77387_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&info->miscdev)) {
		dev_err(&client->dev, "%s unable to register misc device %s\n",
				__func__, info->devname);
		max77387_del(info);
		return -ENODEV;
	}

	max77387_debugfs_init(info);
	return 0;
}

static int max77387_status_show(struct seq_file *s, void *data)
{
	struct max77387_info *info = s->private;

	dev_info(info->dev, "%s\n", __func__);

	seq_printf(s, "max77387 status:\n"
		"    Power State      = %01x\n"
		"    Led Mask         = %01x\n"
		"    Led1 Current     = 0x%02x\n"
		"    Led2 Current     = 0x%02x\n"
		"    Output Mode      = %s\n"
		"    Led Settings     = 0x%02x\n"
		"    Flash TimeOut    = 0x%02x\n"
		"    Torch TimeOut    = 0x%02x\n"
		"    PinState Mask    = 0x%04x\n"
		"    PinState Values  = 0x%04x\n"
		"    Max_Peak_Current = %dmA\n"
		,
		info->pwr_state,
		info->config.led_mask,
		info->regs.led1_fcurr,
		info->regs.led2_fcurr,
		info->op_mode == MAXFLASH_MODE_FLASH ? "FLASH" :
		info->op_mode == MAXFLASH_MODE_TORCH ? "TORCH" : "NONE",
		info->settings.fled_trig,
		info->regs.f_timer,
		info->regs.t_timer,
		info->pdata->pinstate.mask,
		info->pdata->pinstate.values,
		info->config.max_peak_current_mA
		);

	return 0;
}

static ssize_t max77387_attr_set(struct file *s,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct max77387_info *info =
		((struct seq_file *)s->private_data)->private;
	char buf[24];
	int buf_size;
	u32 val = 0;

	dev_info(info->dev, "%s\n", __func__);

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

	dev_err(info->dev, "SYNTAX ERROR: %s\n", buf);
	return -EFAULT;

set_attr:
	dev_info(info->dev, "new data = %x\n", val);
	switch (buf[0]) {
	/* enable/disable power */
	case 'p':
		if (val)
			max77387_power_set(info, NVC_PWR_ON);
		else
			max77387_power_set(info, NVC_PWR_OFF);
		break;
	/* enable/disable led 1/2 */
	case 'l':
		info->config.led_mask = val;
		max77387_configure(info, false);
		break;
	/* change led 1/2 current settings */
	case 'c':
		max77387_set_leds(info, info->config.led_mask,
			val & 0xff, (val >> 8) & 0xff);
		break;
	/* modify flash timeout reg */
	case 'f':
		info->new_timer = val;
		break;
	/* set led work mode/trigger mode */
	case 'x':
		info->op_mode = (val & 0x300) >> 8;
		info->settings.fled_trig = val & 0xff;
		break;
	/* set max_peak_current_mA */
	case 'k':
		if (val & 0xffff)
			info->config.max_peak_current_mA = val & 0xffff;
		max77387_configure(info, true);
		break;
	/* change pinstate setting */
	case 'm':
		info->pdata->pinstate.mask = (val >> 16) & 0xffff;
		info->pdata->pinstate.values = val & 0xffff;
		break;
	/* trigger an external flash/torch event */
	case 'g':
		info->pdata->gpio_strobe = val & 0xffff;
		max77387_strobe(info, (val >> 16) & 1);
		break;
	}

	return count;
}

static int max77387_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, max77387_status_show, inode->i_private);
}

static const struct file_operations max77387_debugfs_fops = {
	.open = max77387_debugfs_open,
	.read = seq_read,
	.write = max77387_attr_set,
	.llseek = seq_lseek,
	.release = single_release,
};

static int max77387_debugfs_init(struct max77387_info *info)
{
	struct dentry *d;

	info->d_max77387 = debugfs_create_dir(
		info->miscdev.this_device->kobj.name, NULL);
	if (info->d_max77387 == NULL) {
		dev_err(info->dev, "%s: debugfs mk dir failed\n", __func__);
		return -ENOMEM;
	}

	d = debugfs_create_file("d", S_IRUGO|S_IWUSR, info->d_max77387,
		(void *)info, &max77387_debugfs_fops);
	if (!d) {
		dev_err(info->dev, "%s: debugfs mk file failed\n", __func__);
		debugfs_remove_recursive(info->d_max77387);
		info->d_max77387 = NULL;
	}

	return -EFAULT;
}

static const struct i2c_device_id max77387_id[] = {
	{ "max77387", 0 },
	{ },
};

static struct i2c_driver max77387_drv = {
	.driver = {
		.name = "max77387",
		.owner = THIS_MODULE,
	},
	.id_table = max77387_id,
	.probe = max77387_probe,
	.remove = max77387_remove,
#ifdef CONFIG_PM
	.shutdown = max77387_shutdown,
	.suspend  = max77387_suspend,
	.resume   = max77387_resume,
#endif
};

module_i2c_driver(max77387_drv);

MODULE_DESCRIPTION("MAXIM MAX77387 flash/torch driver");
MODULE_AUTHOR("Charlie Huang <chahuang@nvidia.com>");
MODULE_LICENSE("GPL v2");
