/*
 * MAX77665_F.c - MAX77665_F flash/torch kernel driver
 *
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Implementation
 * --------------
 * The board level details about the device need to be provided in the board
 * file with the max77665_platform_data structure.
 * Standard among NVC kernel drivers in this structure is:
 * .cfg = Use the NVC_CFG_ defines that are in nvc_torch.h.
 *        Descriptions of the configuration options are with the defines.
 *        This value is typically 0.
 * .num = The number of the instance of the device.  This should start at 1 and
 *        and increment for each device on the board.  This number will be
 *        appended to the MISC driver name, Example: /dev/torch.1
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
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/mfd/max77665.h>
#include <media/nvc.h>
#include <media/max77665-flash.h>

#define MAX77665_F_RW_FLASH_FLED1CURR		0x00
#define MAX77665_F_RW_FLASH_FLED2CURR		0x01
#define MAX77665_F_RW_TORCH_FLEDCURR		0x02
#define MAX77665_F_RW_TORCH_TIMER		0x03
#define MAX77665_F_RW_FLASH_TIMER		0x04
#define MAX77665_F_RW_FLED_ENABLE		0x05
#define MAX77665_F_RW_MAXFLASH_VOLT		0x06
#define MAX77665_F_RW_MAXFLASH_TIMER		0x07
#define MAX77665_F_RO_MAXFLASH_FLED1STAT	0x08
#define MAX77665_F_RO_MAXFLASH_FLED2STAT	0x09
#define MAX77665_F_RW_BOOST_MODE		0x0A
#define MAX77665_F_RW_BOOST_VOLT		0x0B
#define MAX77665_F_RO_BOOST_VOLTRD		0x0C
#define MAX77665_F_RC_FLASH_INTSTAT		0x0E
#define MAX77665_F_RW_FLASH_INTMASK		0x0F
#define MAX77665_F_RO_FLASH_STATUS		0x10

#define MAX77665_PMIC_CHG_CNFG_01		0xB7
#define CHG_CNFG_01_DEFAULT_MODE		0x0C

#define FIELD(x, y)			((x) << (y))
#define FMASK(x)			FIELD(0x03, (x))

#define TORCH_TIMER_SAFETY_DIS		0x1

#define TIMER_ONESHOT			0x0
#define TIMER_MAX			0x1

#define TORCH_TIMER_CTL_MASK		(FIELD(TIMER_MAX, 7) | \
					FIELD(TORCH_TIMER_SAFETY_DIS, 6))

#define BOOST_FLASH_MODE_OFF		0x0
#define BOOST_FLASH_MODE_LED1		0x1
#define BOOST_FLASH_MODE_LED2		0x2
#define BOOST_FLASH_MODE_BOTH		0x3
#define BOOST_FLASH_MODE_FIXED		0x4

#define BOOST_MODE_ONELED		0x0
#define BOOST_MODE_TWOLED		0x1

#define LED2_TORCH_MODE_SHIFT		0
#define LED1_TORCH_MODE_SHIFT		2
#define LED2_FLASH_MODE_SHIFT		4
#define LED1_FLASH_MODE_SHIFT		6

#define LED1_TORCH_TRIG_MASK		FMASK(LED1_TORCH_MODE_SHIFT)
#define LED2_TORCH_TRIG_MASK		FMASK(LED2_TORCH_MODE_SHIFT)
#define LED1_FLASH_TRIG_MASK		FMASK(LED1_FLASH_MODE_SHIFT)
#define LED2_FLASH_TRIG_MASK		FMASK(LED2_FLASH_MODE_SHIFT)

/* TO DO: Need to confirm with maxim these trigger settings */
#define TRIG_MODE_OFF			0x00
#define TRIG_MODE_FLASHEN		0x01
#define TRIG_MODE_TORCHEN		0x02
#define TRIG_MODE_I2C			0x03

#define TORCH_TRIG_BY_I2C	\
			(FIELD(TRIG_MODE_I2C, LED2_TORCH_MODE_SHIFT) | \
			FIELD(TRIG_MODE_I2C, LED1_TORCH_MODE_SHIFT))

#define TORCH_TRIG_BY_FLASHEN	\
			(FIELD(TRIG_MODE_FLASHEN, LED2_TORCH_MODE_SHIFT) | \
			FIELD(TRIG_MODE_FLASHEN, LED1_TORCH_MODE_SHIFT))

#define TORCH_TRIG_BY_TORCHEN	\
			(FIELD(TRIG_MODE_TORCHEN, LED2_TORCH_MODE_SHIFT) | \
			FIELD(TRIG_MODE_TORCHEN, LED1_TORCH_MODE_SHIFT))

#define FLASH_TRIG_BY_FLASHEN	\
			(FIELD(TRIG_MODE_FLASHEN, LED2_FLASH_MODE_SHIFT) | \
			FIELD(TRIG_MODE_FLASHEN, LED1_FLASH_MODE_SHIFT))

#define FLASH_TRIG_BY_TORCHEN	\
			(FIELD(TRIG_MODE_TORCHEN, LED2_FLASH_MODE_SHIFT) | \
			FIELD(TRIG_MODE_TORCHEN, LED1_FLASH_MODE_SHIFT))

#define MAXFLASH_DISABLE		0
#define MAXFLASH_ENABLE			1

#define MAXFLASH_VOLT_HYS_FLOOR		100 /* mV */
#define MAXFLASH_VOLT_HYS_CEILING	300 /* mV */
#define MAXFLASH_VOLT_HYS_STEP		100 /* mV */

#define MAXFLASH_V_TH_FLOOR		2400 /* mV */
#define MAXFLASH_V_TH_CEILING		3400 /* mV */
#define MAXFLASH_V_TH_STEP		33 /* mV */

#define MAXFLASH_TIMER_STEP		256 /* uS */

#define BOOST_VOLT_FLOOR		3300 /* mV */
#define BOOST_VOLT_CEILING		5500 /* mV */
#define BOOST_VOLT_STEP			25 /* mV */

#define MAX77665_F_MAX_FLASH_LEVEL	((1 << 6) + 1)
#define MAX77665_F_MAX_TORCH_LEVEL	((1 << 4) + 1)

#define MAX77665_F_LEVEL_OFF		0xFFFF

#define MAX77665_F_MAX_FLASH_CURRENT(x)    \
	DIV_ROUND_UP(((x) * MAX77665_F_MAX_FLASH_LEVEL), 1000)
#define MAX77665_F_MAX_TORCH_CURRENT(x) \
	DIV_ROUND_UP(((x) * MAX77665_F_MAX_TORCH_LEVEL), 1000)

#define SUSTAINTIME_DEF			558
#define DEFAULT_FLASH_TMR_DUR		((SUSTAINTIME_DEF * 10 - 1) / 625)

#define MAX77665_F_FLASH_TIMER_NUM	16
#define MAX77665_F_TORCH_TIMER_NUM	17

/* minimium debounce time 600uS */
#define RECHARGEFACTOR_DEF		600

#define MAXFLASH_MODE_NONE		0
#define MAXFLASH_MODE_TORCH		1
#define MAXFLASH_MODE_FLASH		2

#define max77665_f_flash_cap_size \
			(sizeof(struct nvc_torch_flash_capabilities_v1) \
			+ sizeof(struct nvc_torch_lumi_level_v1) \
			* MAX77665_F_MAX_FLASH_LEVEL)
#define max77665_f_flash_timeout_size \
			(sizeof(struct nvc_torch_timer_capabilities_v1) \
			+ sizeof(struct nvc_torch_timeout_v1) \
			* MAX77665_F_FLASH_TIMER_NUM)
#define max77665_f_max_flash_cap_size (max77665_f_flash_cap_size * 2 \
			+ max77665_f_flash_timeout_size * 2)

#define max77665_f_torch_cap_size \
			(sizeof(struct nvc_torch_torch_capabilities_v1) \
			+ sizeof(struct nvc_torch_lumi_level_v1) \
			* MAX77665_F_MAX_TORCH_LEVEL)
#define max77665_f_torch_timeout_size \
			(sizeof(struct nvc_torch_timer_capabilities_v1) \
			+ sizeof(struct nvc_torch_timeout_v1) \
			* MAX77665_F_TORCH_TIMER_NUM)
#define max77665_f_max_torch_cap_size (max77665_f_torch_timeout_size * 2\
			+ max77665_f_torch_timeout_size * 2)

#define GET_CURRENT_BY_INDEX(c)	((c) * 125 / 8)		/* mul 15.625 mA */
#define GET_INDEX_BY_CURRENT(c)	((c) * 8 / 125)		/* div by 15.625 mA */

struct max77665_f_caps_struct {
	u32 curr_step_uA;
	u32 max_peak_curr_mA;
	u32 max_torch_curr_mA;
	u32 max_total_current_mA;
};

struct max77665_f_reg_cache {
	bool regs_stale;
	u8 led1_curr;
	u8 led2_curr;
	u8 led_tcurr;
	u8 leds_en;
	u8 t_timer;
	u8 f_timer;
	u8 m_flash;
	u8 m_timing;
	u8 boost_control;
	u8 boost_vout_flash;
	u8 pmic_chg_cnfg01;
};

struct max77665_f_state_regs {
	u8 fled1_status;
	u8 fled2_status;
	u8 boost_volt;
	u8 flash_state;
	u8 progress_state;
};

struct max77665_f_info {
	struct device *dev;
	struct miscdevice miscdev;
	struct dentry *d_max77665_f;
	struct mutex mutex;
	struct max77665_f_power_rail pwr_rail;
	struct max77665_f_platform_data *pdata;
	struct nvc_torch_capability_query query;
	struct nvc_torch_flash_capabilities_v1 *flash_cap[2];
	struct nvc_torch_timer_capabilities_v1 *flash_timeouts[2];
	struct nvc_torch_torch_capabilities_v1 *torch_cap[2];
	struct nvc_torch_timer_capabilities_v1 *torch_timeouts[2];
	struct max77665_f_config config;
	struct max77665_f_reg_cache regs;
	struct max77665_f_state_regs states;
	struct regmap *regmap;
	atomic_t in_use;
	int flash_cap_size;
	int torch_cap_size;
	int pwr_state;
	u8 max_flash[2];
	u8 max_torch[2];
	u8 fled_settings;
	u8 op_mode;
	u8 power_is_on;
	u8 ftimer_mode;
	u8 ttimer_mode;
	u8 new_ftimer;
	u8 new_ttimer;
	char devname[16];
};

static const struct max77665_f_caps_struct max77665_f_caps = {
	15625,
	MAX77665_F_MAX_FLASH_CURRENT(15625),
	MAX77665_F_MAX_TORCH_CURRENT(15625),
	625 * 2
};

static struct nvc_torch_lumi_level_v1 max77665_f_def_flash_levels[] = {
	{0, 15625 * 1},
	{1, 15625 * 2},
	{2, 15625 * 3},
	{3, 15625 * 4},
	{4, 15625 * 5},
	{5, 15625 * 6},
	{6, 15625 * 7},
	{7, 15625 * 8},
	{8, 15625 * 9},
	{9, 15625 * 10},
	{10, 15625 * 11},
	{11, 15625 * 12},
	{12, 15625 * 13},
	{13, 15625 * 14},
	{14, 15625 * 15},
	{15, 15625 * 16},
	{16, 15625 * 17},
	{17, 15625 * 18},
	{18, 15625 * 19},
	{19, 15625 * 20},
	{20, 15625 * 21},
	{21, 15625 * 22},
	{22, 15625 * 23},
	{23, 15625 * 24},
	{24, 15625 * 25},
	{25, 15625 * 26},
	{26, 15625 * 27},
	{27, 15625 * 28},
	{28, 15625 * 29},
	{29, 15625 * 30},
	{30, 15625 * 31},
	{31, 15625 * 32},
};

static struct max77665_f_platform_data max77665_f_default_pdata = {
	.config		= {
			.led_mask = 3, /* both LEDs enabled */
			.synchronized_led = false,
			.torch_trigger_mode = 3,
			.flash_on_torch = false,
			.flash_mode = 2,
			.torch_mode = 1,
			.adaptive_mode = 2,
			.max_peak_current_mA = 1000,
			.max_torch_current_mA = 250,
			.max_peak_duration_ms = 0,
			.max_flash_threshold_mV = 0,
			.led_config[0] = {
				.flash_torch_ratio = 10000,
				.granularity = 1000,
				.flash_levels =
					ARRAY_SIZE(max77665_f_def_flash_levels),
				.lumi_levels = max77665_f_def_flash_levels,
				},
			.led_config[1] = {
				.flash_torch_ratio = 10000,
				.granularity = 1000,
				.flash_levels =
					ARRAY_SIZE(max77665_f_def_flash_levels),
				.lumi_levels = max77665_f_def_flash_levels,
				},
			},
	.cfg		= 0,
	.num		= 0,
	.sync		= 0,
	.dev_name	= "torch",
	.pinstate	= {0x0000, 0x0000},
};

/* torch timer duration settings in uS */
#define MAX77665_F_TORCH_TIMER_FOREVER	0xFFFFFFFF
static u32 max77665_f_torch_timer[] = {
	262000, 524000, 786000, 1048000,
	1572000, 2096000, 2620000, 3144000,
	4193000, 5242000, 6291000, 7340000,
	9437000, 11534000, 13631000, 15728000,
	MAX77665_F_TORCH_TIMER_FOREVER
};

static inline int max77665_f_reg_wr(struct max77665_f_info *info,
		u8 reg, u8 val, bool refresh)
{
	dev_dbg(info->dev, "%s: %02x - %02x, %s %s\n", __func__, reg, val,
		info->regs.regs_stale ? "STALE" : "NONE",
		refresh ? "REFRESH" : "NONE");
	if (likely(info->regs.regs_stale || refresh))
		return regmap_write(info->regmap, reg, val);
	return 0;
}

static inline int max77665_f_reg_raw_wr(struct max77665_f_info *info,
		u8 reg, u8 *val, u8 num)
{
	dev_dbg(info->dev, "%s: %02x - %02x %02x ...\n",
		__func__, reg, val[0], val[1]);
	return regmap_raw_write(info->regmap, reg, val, num);
}

static int max77665_f_set_leds(struct max77665_f_info *info,
		u8 mask, u8 curr1, u8 curr2)
{
	int err = 0;
	u8 fled_en = 0;
	u8 t_curr = 0;
	u8 regs[6];

	memset(regs, 0, sizeof(regs));
	if (info->op_mode == MAXFLASH_MODE_NONE) {
		err = max77665_f_reg_wr(
			info, MAX77665_F_RW_FLED_ENABLE, 0, true);
		if (!err) {
			info->regs.leds_en = 0;
		}
		goto set_leds_end;
	}

	if (mask & 1) {
		if (info->op_mode == MAXFLASH_MODE_FLASH) {
			if (curr1 > info->max_flash[0])
				curr1 = info->max_flash[0];
			fled_en |= (info->fled_settings & LED1_FLASH_TRIG_MASK);
			regs[0] = curr1;
		} else {
			if (curr1 > info->max_torch[0])
				curr1 = info->max_torch[0];
			fled_en |= (info->fled_settings & LED1_TORCH_TRIG_MASK);
			t_curr = curr1;
		}
	}

	if (mask & 2) {
		if (info->op_mode == MAXFLASH_MODE_FLASH) {
			if (curr2 > info->max_flash[1])
				curr2 = info->max_flash[1];
			fled_en |= (info->fled_settings & LED2_FLASH_TRIG_MASK);
			regs[1] = curr2;
		} else {
			if (curr2 > info->max_torch[1])
				curr2 = info->max_torch[1];
			fled_en |= (info->fled_settings & LED2_TORCH_TRIG_MASK);
			t_curr |= curr2 << 4;
		}
	}

	/* if any led is set as flash, update the flash timer register */
	if (fled_en & (LED1_FLASH_TRIG_MASK | LED2_FLASH_TRIG_MASK))
		regs[4] = (info->ftimer_mode & FIELD(TIMER_MAX, 7)) |
				info->new_ftimer;

	/* if any led is set as torch, update the torch timer register */
	if (fled_en & (LED1_TORCH_TRIG_MASK | LED2_TORCH_TRIG_MASK)) {
		regs[3] = (info->ttimer_mode & TORCH_TIMER_CTL_MASK) |
				(info->new_ttimer & 0x0f);
		regs[2] = t_curr;
	}

	regs[5] = fled_en;
	if ((info->regs.led1_curr != regs[0]) ||
		(info->regs.led2_curr != regs[1]) ||
		(info->regs.led_tcurr != regs[2]) ||
		(info->regs.t_timer != regs[3]) ||
		(info->regs.f_timer != regs[4]) ||
		(info->regs.leds_en != regs[5]))
		info->regs.regs_stale = true;

	if (info->regs.regs_stale) {
		err = max77665_f_reg_raw_wr(info,
			MAX77665_F_RW_FLASH_FLED1CURR, regs, sizeof(regs));

		if (!err) {
			info->regs.led1_curr = regs[0];
			info->regs.led2_curr = regs[1];
			info->regs.led_tcurr = regs[2];
			info->regs.t_timer = regs[3];
			info->regs.f_timer = regs[4];
			info->regs.leds_en = regs[5];
			info->regs.regs_stale = false;
		}
	}

set_leds_end:
	if (err)
		dev_err(info->dev, "%s ERROR: %d\n", __func__, err);
	else
		dev_dbg(info->dev,
			"%s led %x f: %02x %02x %02x, t: %02x %02x, en = %x\n",
			__func__, mask, curr1, curr2, info->regs.f_timer,
			info->regs.led_tcurr, info->regs.t_timer, fled_en);
	return err;
}

static inline int max77665_f_get_boost_volt(u16 mV)
{
	if (mV <= BOOST_VOLT_FLOOR)
		return 0;
	if (mV >= BOOST_VOLT_CEILING)
		return 0x64;

	return (mV - BOOST_VOLT_FLOOR) / BOOST_VOLT_STEP + 0x0C;
}

static void max77665_f_update_config(struct max77665_f_info *info)
{
	struct max77665_f_config *pcfg = &info->config;
	struct max77665_f_config *pcfg_cust;
	int i;

	memcpy(pcfg, &max77665_f_default_pdata.config, sizeof(*pcfg));
	if (!info->pdata) {
		info->pdata = &max77665_f_default_pdata;
		dev_dbg(info->dev, "%s No platform data.  Using defaults.\n",
			__func__);
		goto update_end;
	}
	pcfg_cust = &info->pdata->config;

	pcfg->flash_on_torch = pcfg_cust->flash_on_torch;

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

	if (pcfg_cust->boost_vout_flash_mV)
		pcfg->boost_vout_flash_mV = pcfg_cust->boost_vout_flash_mV;

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

	for (i = 0; i < 2; i++) {
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
		info->fled_settings = TORCH_TRIG_BY_FLASHEN;
		break;
	case 2: /* triggered by TORCHEN */
		info->fled_settings = TORCH_TRIG_BY_TORCHEN;
		break;
	case 3: /* triggered by serial interface */
		info->fled_settings = TORCH_TRIG_BY_I2C;
		break;
	default:
		dev_err(info->dev, "%s: unrecognized torch trigger mode.\n",
			__func__);
		dev_err(info->dev, "use default i2c mode.\n");
		info->fled_settings = TORCH_TRIG_BY_I2C;
		break;
	}

	/* How FLASH is triggered */
	if (pcfg->flash_on_torch) /* triggered by TORCHEN */
		info->fled_settings |= FLASH_TRIG_BY_TORCHEN;
	else /* triggered by FLASHEN */
		info->fled_settings |= FLASH_TRIG_BY_FLASHEN;

	if (1 == pcfg->adaptive_mode)
		info->regs.boost_control = BOOST_FLASH_MODE_FIXED;
	else {
		if (pcfg->led_mask > 3) {
			dev_dbg(info->dev, "%s invalid led mask = %d\n",
				__func__, pcfg->led_mask);
			info->regs.boost_control = BOOST_FLASH_MODE_BOTH;
		} else
			info->regs.boost_control = pcfg->led_mask;
	}

	/* both FLED1/FLED2 are enabled */
	if (info->regs.boost_control == FIELD(BOOST_FLASH_MODE_BOTH, 0))
		info->regs.boost_control |= FIELD(BOOST_MODE_TWOLED, 7);

	info->regs.pmic_chg_cnfg01 = CHG_CNFG_01_DEFAULT_MODE;
	info->regs.boost_vout_flash =
		max77665_f_get_boost_volt(pcfg->boost_vout_flash_mV);

	info->ftimer_mode = (pcfg->flash_mode == 1) ?
			FIELD(TIMER_ONESHOT, 7) : FIELD(TIMER_MAX, 7);

	switch (pcfg->torch_mode) {
	case 1:
		info->ttimer_mode = FIELD(TIMER_MAX, 7) |
					FIELD(TORCH_TIMER_SAFETY_DIS, 6);
		break;
	case 2:
		info->ttimer_mode = FIELD(TIMER_ONESHOT, 7);
		break;
	case 3:
	default:
		info->ttimer_mode = FIELD(TIMER_MAX, 7);
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

		info->regs.m_flash = FIELD(MAXFLASH_ENABLE, 7) |
		((pcfg->max_flash_threshold_mV - MAXFLASH_V_TH_FLOOR) /
				MAXFLASH_V_TH_STEP);
		info->regs.m_flash |= (pcfg->max_flash_hysteresis_mV +
		MAXFLASH_VOLT_HYS_STEP / 2) / MAXFLASH_VOLT_HYS_STEP;
	}

	if (pcfg->max_flash_lbdly_f_uS)
		info->regs.m_timing =
		FIELD(pcfg->max_flash_lbdly_f_uS / MAXFLASH_TIMER_STEP, 0);

	if (pcfg->max_flash_lbdly_r_uS)
		info->regs.m_timing |=
		FIELD(pcfg->max_flash_lbdly_r_uS / MAXFLASH_TIMER_STEP, 3);
}

static int max77665_f_update_settings(struct max77665_f_info *info)
{
	int err = 0;

	info->regs.regs_stale = true;
	err |= max77665_f_reg_wr(info, MAX77665_PMIC_CHG_CNFG_01,
				info->regs.pmic_chg_cnfg01, false);

	err |= max77665_f_reg_wr(info, MAX77665_F_RW_BOOST_MODE,
				info->regs.boost_control, false);

	err |= max77665_f_reg_wr(info, MAX77665_F_RW_BOOST_VOLT,
				info->regs.boost_vout_flash, false);

	err |= max77665_f_reg_wr(info, MAX77665_F_RW_MAXFLASH_VOLT,
				info->regs.m_flash, false);

	err |= max77665_f_reg_wr(info, MAX77665_F_RW_MAXFLASH_TIMER,
				info->regs.m_timing, false);

	err |= max77665_f_set_leds(info, info->config.led_mask,
				info->regs.led1_curr, info->regs.led2_curr);

	info->regs.regs_stale = false;
	return err;
}

static int max77665_f_configure(struct max77665_f_info *info, bool update)
{
	struct max77665_f_config *pcfg = &info->config;
	struct nvc_torch_capability_query *pqry = &info->query;
	struct nvc_torch_flash_capabilities_v1	*pfcap = NULL;
	struct nvc_torch_torch_capabilities_v1	*ptcap = NULL;
	struct nvc_torch_timer_capabilities_v1	*ptmcap = NULL;
	struct nvc_torch_lumi_level_v1		*plvls = NULL;
	int val, i, j;

	if (pcfg->max_peak_current_mA > max77665_f_caps.max_peak_curr_mA ||
		!pcfg->max_peak_current_mA) {
		dev_notice(info->dev, "invalid max_peak_current_mA: %d,",
				pcfg->max_peak_current_mA);
		dev_notice(info->dev, " changed to %d\n",
				max77665_f_caps.max_peak_curr_mA);
		pcfg->max_peak_current_mA = max77665_f_caps.max_peak_curr_mA;
	}

	/* number of leds enabled */
	i = 1;
	/* in synchronize mode, both leds are considered as 1 */
	if (!pcfg->synchronized_led && (info->config.led_mask & 3) == 3)
		i = 2;
	pqry->flash_num = i;
	pqry->torch_num = i;

	val = pcfg->max_peak_current_mA * i;
	if (val > max77665_f_caps.max_total_current_mA)
		val = max77665_f_caps.max_total_current_mA;

	if (!pcfg->max_total_current_mA || pcfg->max_total_current_mA > val)
		pcfg->max_total_current_mA = val;
	pcfg->max_peak_current_mA =
		pcfg->max_total_current_mA / i;

	if (pcfg->max_torch_current_mA > max77665_f_caps.max_torch_curr_mA ||
		!pcfg->max_torch_current_mA) {
		dev_notice(info->dev, "invalid max_torch_current_mA: %d,",
				pcfg->max_torch_current_mA);
		dev_notice(info->dev, " changed to %d\n",
				max77665_f_caps.max_torch_curr_mA);
		pcfg->max_torch_current_mA =
			max77665_f_caps.max_torch_curr_mA;
	}

	pqry->version = NVC_TORCH_CAPABILITY_VER_1;
	pqry->led_attr = 0;
	for (i = 0; i < pqry->flash_num; i++) {
		pfcap = info->flash_cap[i];
		pfcap->version = NVC_TORCH_CAPABILITY_VER_1;
		pfcap->led_idx = i;
		pfcap->attribute = 0;
		pfcap->granularity = pcfg->led_config[i].granularity;
		pfcap->timeout_num = MAX77665_F_FLASH_TIMER_NUM;
		ptmcap = info->flash_timeouts[i];
		pfcap->timeout_off = (void *)ptmcap - (void *)pfcap;
		pfcap->flash_torch_ratio =
				pcfg->led_config[i].flash_torch_ratio;

		plvls = pcfg->led_config[i].lumi_levels;
		pfcap->levels[0].guidenum = MAX77665_F_LEVEL_OFF;
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
			ptmcap->timeouts[j].timeout = 62500 * (j + 1);
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
		ptcap->timeout_num = MAX77665_F_TORCH_TIMER_NUM;
		ptmcap = info->torch_timeouts[i];
		ptcap->timeout_off = (void *)ptmcap - (void *)ptcap;

		plvls = pcfg->led_config[i].lumi_levels;
		ptcap->levels[0].guidenum = MAX77665_F_LEVEL_OFF;
		ptcap->levels[0].luminance = 0;
		for (j = 1; j < pcfg->led_config[i].flash_levels + 1; j++) {
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
			ptmcap->timeouts[j].timeout = max77665_f_torch_timer[j];
			dev_dbg(info->dev, "t: %02d - %d uS\n", j,
				ptmcap->timeouts[j].timeout);
		}
	}

	if (update && (info->pwr_state == NVC_PWR_COMM ||
			info->pwr_state == NVC_PWR_ON))
		return max77665_f_update_settings(info);

	return 0;
}

static int max77665_f_strobe(struct max77665_f_info *info, int t_on)
{
	u32 gpio = info->pdata->gpio_strobe & 0xffff;
	u32 lact = (info->pdata->gpio_strobe & 0xffff0000) ? 1 : 0;
	return gpio_direction_output(gpio, lact ^ (t_on & 1));
}

static int max77665_f_enter_offmode(struct max77665_f_info *info, bool op_off)
{
	int err;

	mutex_lock(&info->mutex);
	if (op_off) {
		info->op_mode = MAXFLASH_MODE_NONE;
		err = max77665_f_set_leds(info, 3, 0, 0);
	} else {
		err = max77665_f_reg_wr(
			info, MAX77665_F_RW_FLED_ENABLE, 0, true);
	}
	mutex_unlock(&info->mutex);
	return err;
}

#ifdef CONFIG_PM
static int max77665_f_suspend(struct platform_device *pdev, pm_message_t msg)
{
	struct max77665_f_info *info = dev_get_drvdata(&pdev->dev);

	dev_info(&pdev->dev, "Suspending\n");
	info->regs.regs_stale = true;

	return 0;
}

static int max77665_f_resume(struct platform_device *pdev)
{
	struct max77665_f_info *info = dev_get_drvdata(&pdev->dev);

	dev_info(&pdev->dev, "Resuming\n");
	info->regs.regs_stale = true;

	return 0;
}

static void max77665_f_shutdown(struct platform_device *pdev)
{
	struct max77665_f_info *info = dev_get_drvdata(&pdev->dev);

	dev_info(&pdev->dev, "Shutting down\n");

	max77665_f_enter_offmode(info, true);
	info->regs.regs_stale = true;
}
#endif

static int max77665_f_power_off(struct max77665_f_info *info)
{
	struct max77665_f_power_rail *pw = &info->pwr_rail;
	int err = 0;

	if (!info->power_is_on)
		return 0;

	if (info->pdata && info->pdata->poweroff_callback)
		err = info->pdata->poweroff_callback(pw);

	if (IS_ERR_VALUE(err))
		return err;

	/* the call back function already handles the power off sequence */
	if (err) {
		err = 0;
		goto max77665_f_poweroff_done;
	}

	if (pw->i2c)
		regulator_disable(pw->i2c);

	if (pw->vbus)
		regulator_disable(pw->vbus);

	if (pw->vio)
		regulator_disable(pw->vio);

max77665_f_poweroff_done:
	info->power_is_on = 0;
	return err;
}

static int max77665_f_power_on(struct max77665_f_info *info)
{
	struct max77665_f_power_rail *pw = &info->pwr_rail;
	int err = 0;

	if (info->power_is_on)
		return 0;

	if (info->pdata && info->pdata->poweron_callback)
		err = info->pdata->poweron_callback(pw);

	if (IS_ERR_VALUE(err))
		return err;

	/* the call back function already handles the power on sequence */
	if (err) {
		err = 0;
		goto max77665_f_poweron_sync;
	}

	if (pw->vio) {
		err = regulator_enable(pw->vio);
		if (err) {
			dev_err(info->dev, "%s vio err\n", __func__);
			goto max77665_f_poweron_vio_fail;
		}
	}

	if (pw->vbus) {
		err = regulator_enable(pw->vbus);
		if (err) {
			dev_err(info->dev, "%s vbus err\n", __func__);
			goto max77665_f_poweron_vbus_fail;
		}
	}

	if (pw->i2c) {
		err = regulator_enable(pw->i2c);
		if (err) {
			dev_err(info->dev, "%s i2c err\n", __func__);
			goto max77665_f_poweron_i2c_fail;
		}
	}

max77665_f_poweron_sync:
	info->power_is_on = 1;
	err = max77665_f_update_settings(info);
	if (err) {
		max77665_f_power_off(info);
		return err;
	}

	return 0;

max77665_f_poweron_i2c_fail:
	if (pw->vbus)
		regulator_disable(pw->vbus);
max77665_f_poweron_vbus_fail:
	if (pw->vio)
		regulator_disable(pw->vio);
max77665_f_poweron_vio_fail:
	if (info->pdata && info->pdata->poweroff_callback)
		info->pdata->poweroff_callback(pw);
	return err;
}

static int max77665_f_power_set(struct max77665_f_info *info, int pwr)
{
	int err = 0;

	if (pwr == info->pwr_state)
		return 0;

	switch (pwr) {
	case NVC_PWR_OFF:
		max77665_f_enter_offmode(info, true);
		if ((info->pdata->cfg & NVC_CFG_OFF2STDBY) ||
			(info->pdata->cfg & NVC_CFG_BOOT_INIT))
			pwr = NVC_PWR_STDBY;
		else
			err = max77665_f_power_off(info);
		break;
	case NVC_PWR_STDBY_OFF:
		if ((info->pdata->cfg & NVC_CFG_OFF2STDBY) ||
			(info->pdata->cfg & NVC_CFG_BOOT_INIT))
			pwr = NVC_PWR_STDBY;
		else
			err = max77665_f_power_on(info);
		break;
	case NVC_PWR_STDBY:
		err = max77665_f_power_on(info);
		err |= max77665_f_enter_offmode(info, false);
		break;

	case NVC_PWR_COMM:
	case NVC_PWR_ON:
		err = max77665_f_power_on(info);
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

static inline int max77665_f_power_user_set(
	struct max77665_f_info *info, int pwr)
{
	int err = 0;

	if (!pwr || (pwr > NVC_PWR_ON))
		return 0;

	err = max77665_f_power_set(info, pwr);
	if (info->pdata->cfg & NVC_CFG_NOERR)
		return 0;

	return err;
}

static int max77665_f_get_param(struct max77665_f_info *info, long arg)
{
	struct nvc_param params;
	struct nvc_torch_pin_state pinstate;
	const void *data_ptr = NULL;
	u32 data_size = 0;
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
			return -EINVAL;
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
			return -EINVAL;
		}
		data_ptr = info->torch_cap[params.variant];
		data_size = info->torch_cap_size;
		break;
	case NVC_PARAM_FLASH_LEVEL:
		if (params.variant >= info->query.flash_num) {
			dev_err(info->dev,
				"%s unsupported flash index.\n", __func__);
			return -EINVAL;
		}
		if (params.variant > 0)
			reg = info->regs.led2_curr;
		else
			reg = info->regs.led1_curr;
		data_ptr = &reg;
		data_size = sizeof(reg);
		dev_dbg(info->dev, "%s FLASH_LEVEL %d\n", __func__, reg);
		break;
	case NVC_PARAM_TORCH_LEVEL:
		reg = info->regs.led_tcurr;
		if (params.variant >= info->query.torch_num) {
			dev_err(info->dev, "%s unsupported torch index.\n",
				__func__);
			return -EINVAL;
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
		return -EINVAL;
	}

	dev_dbg(info->dev, "%s data size user %d vs local %d\n",
			__func__, params.sizeofvalue, data_size);
	if (params.sizeofvalue < data_size) {
		dev_err(info->dev, "%s data size mismatch\n", __func__);
		return -EINVAL;
	}

	if (copy_to_user((void __user *)params.p_value,
			 data_ptr, data_size)) {
		dev_err(info->dev, "%s copy_to_user err line %d\n",
				__func__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

static int max77665_f_get_levels(struct max77665_f_info *info,
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

	if (plevels->levels[0] == MAX77665_F_LEVEL_OFF)
		plevels->ledmask &= ~1;
	if (plevels->levels[1] == MAX77665_F_LEVEL_OFF)
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

static int max77665_f_set_param(struct max77665_f_info *info, long arg)
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
		max77665_f_get_levels(info, &params, true, &led_levels);
		info->new_ftimer = led_levels.timeout & 0X0F;
		curr1 = led_levels.levels[0];
		curr2 = led_levels.levels[1];
		err = max77665_f_set_leds(info,
			led_levels.ledmask, curr1, curr2);
		return err;
	case NVC_PARAM_TORCH_LEVEL:
		max77665_f_get_levels(info, &params, false, &led_levels);
		info->new_ftimer = led_levels.timeout & 0X0F;
		curr1 = led_levels.levels[0];
		curr2 = led_levels.levels[1];
		err = max77665_f_set_leds(info,
			led_levels.ledmask, curr1, curr2);
		return err;
	case NVC_PARAM_FLASH_PIN_STATE:
		if (copy_from_user(&val, (const void __user *)params.p_value,
			   sizeof(val))) {
			dev_err(info->dev, "%s %d copy_from_user err\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		dev_dbg(info->dev, "%s FLASH_PIN_STATE: %d\n",
				__func__, val);
		return max77665_f_strobe(info, val);
	default:
		dev_err(info->dev, "%s unsupported parameter: %d\n",
				__func__, params.param);
		return -EINVAL;
	}
}

static long max77665_f_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	struct max77665_f_info *info = file->private_data;
	int pwr;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(NVC_IOCTL_PARAM_WR):
		return max77665_f_set_param(info, arg);
	case _IOC_NR(NVC_IOCTL_PARAM_RD):
		return max77665_f_get_param(info, arg);
	case _IOC_NR(NVC_IOCTL_PWR_WR):
		/* This is a Guaranteed Level of Service (GLOS) call */
		pwr = (int)arg * 2;
		dev_dbg(info->dev, "%s PWR_WR: %d\n", __func__, pwr);
		return max77665_f_power_user_set(info, pwr);
	case _IOC_NR(NVC_IOCTL_PWR_RD):
		pwr = info->pwr_state / 2;
		dev_dbg(info->dev, "%s PWR_RD: %d\n", __func__, pwr);
		if (copy_to_user(
			(void __user *)arg, (const void *)&pwr, sizeof(pwr))) {
			dev_err(info->dev, "%s copy_to_user err line %d\n",
					__func__, __LINE__);
			return -EFAULT;
		}

		return 0;
	default:
		dev_err(info->dev, "%s unsupported ioctl: %x\n",
				__func__, cmd);
		return -EINVAL;
	}
}

static int max77665_f_open(struct inode *inode, struct file *file)
{
	struct miscdevice	*miscdev = file->private_data;
	struct max77665_f_info *info;

	info = container_of(miscdev, struct max77665_f_info, miscdev);
	if (!info)
		return -ENODEV;

	if (atomic_xchg(&info->in_use, 1))
		return -EBUSY;

	file->private_data = info;
	dev_dbg(info->dev, "%s\n", __func__);
	return 0;
}

static int max77665_f_release(struct inode *inode, struct file *file)
{
	struct max77665_f_info *info = file->private_data;

	dev_dbg(info->dev, "%s\n", __func__);
	max77665_f_power_set(info, NVC_PWR_OFF);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	return 0;
}

static int max77665_f_power_put(struct max77665_f_power_rail *pw)
{
	if (likely(pw->vbus))
		regulator_put(pw->vbus);

	if (likely(pw->vio))
		regulator_put(pw->vio);

	if (likely(pw->i2c))
		regulator_put(pw->i2c);

	pw->vio = NULL;
	pw->vbus = NULL;
	pw->i2c = NULL;

	return 0;
}

static int max77665_f_regulator_get(struct max77665_f_info *info,
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

static int max77665_f_power_get(struct max77665_f_info *info)
{
	struct max77665_f_power_rail *pw = &info->pwr_rail;
	int err;

	err = max77665_f_regulator_get(info, &pw->vbus, "vbus"); /* 3.7v */
	err |= max77665_f_regulator_get(info, &pw->vio, "vio"); /* 1.8v */
	err |= max77665_f_regulator_get(info, &pw->i2c, "vi2c"); /* 1.8v */

	return err;
}

static const struct file_operations max77665_f_fileops = {
	.owner = THIS_MODULE,
	.open = max77665_f_open,
	.unlocked_ioctl = max77665_f_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = max77665_f_ioctl,
#endif
	.release = max77665_f_release,
};

static void max77665_f_del(struct max77665_f_info *info)
{
	max77665_f_power_set(info, NVC_PWR_OFF);
	max77665_f_power_put(&info->pwr_rail);
}

static int max77665_f_remove(struct platform_device *pdev)
{
	struct max77665_f_info *info = dev_get_drvdata(&pdev->dev);

	dev_dbg(info->dev, "%s\n", __func__);
	misc_deregister(&info->miscdev);
	max77665_f_del(info);
	if (info->d_max77665_f)
		debugfs_remove_recursive(info->d_max77665_f);

	return 0;
}

static int max77665_f_debugfs_init(struct max77665_f_info *info);

static void max77665_f_caps_layout(struct max77665_f_info *info)
{
#define MAX77665_FLASH_CAP_TIMEOUT_SIZE \
	(max77665_f_flash_cap_size + max77665_f_flash_timeout_size)
#define MAX77665_TORCH_CAP_TIMEOUT_SIZE \
	(max77665_f_torch_cap_size + max77665_f_torch_timeout_size)
	void *start_ptr = (void *)info + sizeof(*info);

	info->flash_cap[0] = start_ptr;
	info->flash_timeouts[0] = start_ptr + max77665_f_flash_cap_size;

	start_ptr += MAX77665_FLASH_CAP_TIMEOUT_SIZE;
	info->flash_cap[1] = start_ptr;
	info->flash_timeouts[1] = start_ptr + max77665_f_flash_cap_size;

	info->flash_cap_size = MAX77665_FLASH_CAP_TIMEOUT_SIZE;

	start_ptr += MAX77665_FLASH_CAP_TIMEOUT_SIZE;
	info->torch_cap[0] = start_ptr;
	info->torch_timeouts[0] = start_ptr + max77665_f_torch_cap_size;

	start_ptr += MAX77665_TORCH_CAP_TIMEOUT_SIZE;
	info->torch_cap[1] = start_ptr;
	info->torch_timeouts[1] = start_ptr + max77665_f_torch_cap_size;

	info->torch_cap_size = MAX77665_TORCH_CAP_TIMEOUT_SIZE;
	dev_dbg(info->dev, "%s: %d(%d + %d), %d(%d + %d)\n", __func__,
		info->flash_cap_size, max77665_f_flash_cap_size,
		max77665_f_flash_timeout_size, info->torch_cap_size,
		max77665_f_torch_cap_size, max77665_f_torch_timeout_size);
}

static int max77665_f_probe(struct platform_device *pdev)
{
	struct max77665_f_info *info;
	struct max77665 *maxim;

	dev_info(&pdev->dev, "%s\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(*info) +
			max77665_f_max_flash_cap_size +
			max77665_f_max_torch_cap_size,
			GFP_KERNEL);
	if (info == NULL) {
		dev_err(&pdev->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	info->dev = &pdev->dev;
	maxim = dev_get_drvdata(info->dev->parent);
	info->regmap = maxim->regmap[MAX77665_I2C_SLAVE_PMIC];
	if (unlikely(!info->regmap)) {
		dev_err(&pdev->dev, "%s: max77665 platform error\n", __func__);
		return -EFAULT;
	}

	max77665_f_power_get(info);

	if (pdev->dev.platform_data) {
		info->pdata = pdev->dev.platform_data;
		dev_dbg(&pdev->dev, "pdata: %s\n", info->pdata->dev_name);
	} else
		dev_notice(&pdev->dev, "%s NO platform data\n", __func__);

	max77665_f_caps_layout(info);

	max77665_f_update_config(info);

	/* flash mode */
	info->op_mode = MAXFLASH_MODE_NONE;

	max77665_f_configure(info, false);

	dev_set_drvdata(info->dev, info);
	mutex_init(&info->mutex);

	if (info->pdata->dev_name != NULL)
		strncpy(info->devname, info->pdata->dev_name,
			sizeof(info->devname) - 1);
	else
		strncpy(info->devname, "max77665_f", sizeof(info->devname) - 1);

	if (info->pdata->num)
		snprintf(info->devname, sizeof(info->devname), "%s.%u",
				info->devname, info->pdata->num);

	info->miscdev.name = info->devname;
	info->miscdev.fops = &max77665_f_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&info->miscdev)) {
		dev_err(&pdev->dev, "%s unable to register misc device %s\n",
				__func__, info->devname);
		max77665_f_del(info);
		return -ENODEV;
	}

	max77665_f_debugfs_init(info);
	return 0;
}

static int max77665_f_status_show(struct seq_file *s, void *data)
{
	struct max77665_f_info *info = s->private;

	dev_info(info->dev, "%s\n", __func__);

	seq_printf(s, "max77665_f status:\n"
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
		info->regs.led1_curr,
		info->regs.led2_curr,
		info->op_mode == MAXFLASH_MODE_FLASH ? "FLASH" :
		info->op_mode == MAXFLASH_MODE_TORCH ? "TORCH" : "NONE",
		info->fled_settings,
		info->regs.f_timer,
		info->regs.t_timer,
		info->pdata->pinstate.mask,
		info->pdata->pinstate.values,
		info->config.max_peak_current_mA
		);

	return 0;
}

static ssize_t max77665_f_attr_set(struct file *s,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct max77665_f_info *info =
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
	case 'c': /* change led 1/2 current settings */
		max77665_f_set_leds(info, info->config.led_mask,
			val & 0xff, (val >> 8) & 0xff);
		break;
	case 'l': /* enable/disable led 1/2 */
		info->config.led_mask = val;
		max77665_f_configure(info, false);
		break;
	case 'm': /* change pinstate setting */
		info->pdata->pinstate.mask = (val >> 16) & 0xffff;
		info->pdata->pinstate.values = val & 0xffff;
		break;
	case 'f': /* modify flash timeout reg */
		info->new_ftimer = val & 0X0F;
		max77665_f_set_leds(info, info->config.led_mask,
			info->regs.led1_curr, info->regs.led2_curr);
		break;
	case 'p':
		if (val)
			max77665_f_power_set(info, NVC_PWR_ON);
		else
			max77665_f_power_set(info, NVC_PWR_OFF);
		break;
	case 'k':
		if (val & 0xffff)
			info->config.max_peak_current_mA = val & 0xffff;
		max77665_f_configure(info, true);
		break;
	case 'x':
		info->op_mode = (val & 0x300) >> 8;
		if (val & 0xff)
			info->fled_settings = val & 0xff;
		max77665_f_configure(info, true);
		break;
	case 'g':
		info->pdata->gpio_strobe = val & 0xffff;
		max77665_f_strobe(info, (val >> 16) & 1);
		break;
	}

	return count;
}

static int max77665_f_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, max77665_f_status_show, inode->i_private);
}

static const struct file_operations max77665_f_debugfs_fops = {
	.open = max77665_f_debugfs_open,
	.read = seq_read,
	.write = max77665_f_attr_set,
	.llseek = seq_lseek,
	.release = single_release,
};

static int max77665_f_debugfs_init(struct max77665_f_info *info)
{
	struct dentry *d;

	info->d_max77665_f = debugfs_create_dir(
		info->miscdev.this_device->kobj.name, NULL);
	if (info->d_max77665_f == NULL) {
		dev_err(info->dev, "%s: debugfs mk dir failed\n", __func__);
		return -ENOMEM;
	}

	d = debugfs_create_file("d", S_IRUGO|S_IWUSR, info->d_max77665_f,
		(void *)info, &max77665_f_debugfs_fops);
	if (!d) {
		dev_err(info->dev, "%s: debugfs mk file failed\n", __func__);
		debugfs_remove_recursive(info->d_max77665_f);
		info->d_max77665_f = NULL;
	}

	return -EFAULT;
}

static const struct platform_device_id max77665_flash_id[] = {
	{ "max77665-flash", 0 },
	{ },
};

static struct platform_driver max77665_flash_drv = {
	.driver = {
		.name = "max77665-flash",
		.owner = THIS_MODULE,
	},
	.id_table = max77665_flash_id,
	.probe = max77665_f_probe,
	.remove = max77665_f_remove,
#ifdef CONFIG_PM
	.shutdown = max77665_f_shutdown,
	.suspend  = max77665_f_suspend,
	.resume   = max77665_f_resume,
#endif
};

module_platform_driver(max77665_flash_drv);
MODULE_DESCRIPTION("MAXIM MAX77665_F flash/torch driver");
MODULE_AUTHOR("Charlie Huang <chahuang@nvidia.com>");
MODULE_LICENSE("GPL");
