/*
 * LM3565.c - LM3565 flash/torch kernel driver
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.

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

#include <linux/kernel.h>
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
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/regmap.h>
#include <media/nvc.h>
#include <media/lm3565.h>

#define LM3565_REG_CHIPID		0x00
#define LM3565_REG_VERSION		0x01
#define LM3565_REG_SET_CURR		0x02
#define LM3565_REG_TXMASK		0x03
#define LM3565_REG_LOWVOLTAGE		0x04
#define LM3565_REG_FLASHTIMER		0x05
#define LM3565_REG_STROBE		0x06
#define LM3565_REG_MODE			0x07
#define LM3565_REG_FAULT		0x08
#define LM3565_REG_ADC_CNTL		0x09
#define LM3565_REG_ADC_INPUT		0x0A
#define LM3565_REG_ADC_LED		0x0B
#define LM3565_REG_MEM_START		0x0C
#define LM3565_REG_MEM_END		0x0D
#define LM3565_REG_MEM_DATA		0x0E
#define LM3565_REG_MEM_CMD		0x0F
#define LM3565_REG_STORAGE0		0x10
#define LM3565_REG_STORAGE_SIZE		0x10
#define LM3565_REG_MAX_ADDR		0x1F

#define LM3565_MODE_STDBY		0x0F
#define LM3565_MODE_TORCH		0x01
#define LM3565_MODE_FLASH		0x02

#define FIELD(x, y)			((x) << (y))

#define LM3565_MAX_ASSIST_CURRENT(x)    \
	DIV_ROUND_UP(((x) * 0xff * 0x7f / 0xff), 1000)
#define LM3565_MAX_INDICATOR_CURRENT(x) \
	DIV_ROUND_UP(((x) * 0xff * 0x3f / 0xff), 1000)

#define LM3565_MAX_FLASH_LEVEL		17
#define LM3565_MAX_TORCH_LEVEL		3
#define LM3565_FLASH_TIMER_NUM		256
#define LM3565_TORCH_TIMER_NUM		1

#define LM3565_LEVEL_OFF		0xFFFF

#define LM3565_MAX_FLASH_TIME_MS	512
#define FLASHTIME(x)			((x) / 2 - 1)

#define lm3565_max_flash_cap_size \
			(sizeof(struct nvc_torch_flash_capabilities_v1) \
			+ sizeof(struct nvc_torch_lumi_level_v1) \
			* (LM3565_MAX_FLASH_LEVEL))
#define lm3565_flash_timeout_size \
			(sizeof(struct nvc_torch_timer_capabilities_v1) \
			+ sizeof(struct nvc_torch_timeout_v1) \
			* LM3565_FLASH_TIMER_NUM)
#define lm3565_max_torch_cap_size \
			(sizeof(struct nvc_torch_torch_capabilities_v1) \
			+ sizeof(struct nvc_torch_lumi_level_v1) \
			* (LM3565_MAX_TORCH_LEVEL))
#define lm3565_torch_timeout_size \
			(sizeof(struct nvc_torch_timer_capabilities_v1) \
			+ sizeof(struct nvc_torch_timeout_v1) \
			* LM3565_TORCH_TIMER_NUM)

struct lm3565_caps_struct {
	char *name;
	u32 curr_base_uA;
	u32 curr_step_uA;
	u32 max_peak_curr_mA;
	u32 max_assist_curr_mA;
};

struct lm3565_reg_cache {
	u8 dev_id;
	u8 version;
	u8 led_curr;
	u8 txmask;
	u8 vlow;
	u8 ftime;
	u8 strobe;
	u8 outmode;
};

struct lm3565_info {
	struct i2c_client *i2c_client;
	struct miscdevice miscdev;
	struct device *dev;
	struct dentry *d_lm3565;
	struct list_head list;
	struct mutex mutex;
	struct lm3565_power_rail power;
	struct regmap *regmap;
	struct lm3565_platform_data *pdata;
	struct nvc_torch_capability_query query;
	struct nvc_torch_flash_capabilities_v1 flash_cap;
	struct nvc_torch_lumi_level_v1 flash_levels[LM3565_MAX_FLASH_LEVEL];
	struct nvc_torch_timer_capabilities_v1 flash_timeouts;
	struct nvc_torch_timeout_v1 flash_timeout_ary[LM3565_FLASH_TIMER_NUM];
	struct nvc_torch_torch_capabilities_v1 torch_cap;
	struct nvc_torch_lumi_level_v1 torch_levels[LM3565_MAX_TORCH_LEVEL];
	struct nvc_torch_timer_capabilities_v1 torch_timeouts;
	struct nvc_torch_timeout_v1 torch_timeout_ary[LM3565_TORCH_TIMER_NUM];
	struct lm3565_caps_struct caps;
	struct lm3565_config config;
	struct lm3565_reg_cache regs;
	atomic_t in_use;
	int pwr_state;
	u8 max_flash;
	u8 max_torch;
	u8 op_mode;
	u8 power_on;
	u8 new_timer;
	u8 flash_strobe;
};

static const struct i2c_device_id lm3565_id[] = {
	{ "lm3565", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, lm3565_id);

static LIST_HEAD(lm3565_info_list);
static DEFINE_SPINLOCK(lm3565_spinlock);

static struct lm3565_platform_data lm3565_default_pdata = {
	.cfg		= 0,
	.num		= 0,
	.dev_name	= "torch",
	.pinstate	= {0x0000, 0x0000},
};

static struct nvc_torch_capability_query lm3565_query = {
	.version = NVC_TORCH_CAPABILITY_VER_1,
	.flash_num = 1,
	.torch_num = 1,
	.led_attr = 0,
};

static const struct lm3565_caps_struct lm3565_caps = {
	"lm3565", 480000, 30000, 930, 60,
};

static struct nvc_torch_lumi_level_v1 lm3565_def_flash_levels[] = {
	{0, 480000},
	{1, 510000},
	{2, 540000},
	{3, 570000},
	{4, 600000},
	{5, 630000},
	{6, 660000},
	{7, 690000},
	{8, 720000},
	{9, 750000},
	{10, 780000},
	{11, 810000},
	{12, 840000},
	{13, 870000},
	{14, 900000},
	{15, 930000},
};

static const struct nvc_torch_lumi_level_v1 torch_levels[2] = {
	{0, 60000},
	{1, 90000}
};

/* translated from the default register values after power up */
static const struct lm3565_config default_cfg = {
	.txmask_current_mA = 0,
	.txmask_inductor_mA = 0,
	.vin_low_v_mV = 0,
	.vin_low_c_mA = 0,
	.strobe_type = 2,
	.max_peak_current_mA = 900,
	.max_sustained_current_mA = 0,
	.max_peak_duration_ms = 0,
	.min_current_mA = 0,
	.def_flash_time_mS = LM3565_MAX_FLASH_TIME_MS,
	.led_config = {
		.granularity = 1000,
		.flash_levels = ARRAY_SIZE(lm3565_def_flash_levels),
		.lumi_levels = lm3565_def_flash_levels,
	},
};

static const u16 v_in_low[] = {
	0, 3000, 3100, 3200, 3300, 3400, 3500, 3600, 3700
};

static const u16 c_in_low[] = {150, 180, 210, 240};

static const u16 tx_c_lut[] = {
	30, 60, 90, 120, 150, 180, 210, 240,
	270, 300, 330, 360, 390, 420, 450, 480
};

static const u16 tx_i_lut[] = {2300, 2600, 2900, 3300};

/* torch timer duration settings in uS */
#define LM3565_TORCH_TIMER_FOREVER	0xFFFFFFFF
static u32 lm3565_torch_timer[LM3565_TORCH_TIMER_NUM] = {
	LM3565_TORCH_TIMER_FOREVER
};

static int lm3565_power(struct lm3565_info *info, int pwr);
static int lm3565_debugfs_init(struct lm3565_info *info);

static const struct regmap_config lm3565_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

static inline int lm3565_reg_rd(
	struct lm3565_info *info, u8 reg, unsigned int *val)
{
	int ret = -ENODEV;

	mutex_lock(&info->mutex);
	if (info->power_on)
		ret = regmap_read(info->regmap, reg, val);
	else
		dev_err(info->dev, "%s: power is off.\n", __func__);
	mutex_unlock(&info->mutex);
	return ret;
}

static int lm3565_reg_raw_wr(struct lm3565_info *info, u8 reg, u8 *buf, u8 num)
{
	int ret = -ENODEV;

	dev_dbg(info->dev, "%s %x = %x %x\n", __func__, reg, buf[0], buf[1]);
	mutex_lock(&info->mutex);
	if (info->power_on)
		ret = regmap_raw_write(info->regmap, reg, buf, num);
	else
		dev_err(info->dev, "%s: power is off.\n", __func__);
	mutex_unlock(&info->mutex);
	return ret;
}

static int lm3565_reg_wr(struct lm3565_info *info, u8 reg, u8 val)
{
	int ret = -ENODEV;

	dev_dbg(info->dev, "%s\n", __func__);
	mutex_lock(&info->mutex);
	if (info->power_on)
		ret = regmap_write(info->regmap, reg, val);
	else
		dev_err(info->dev, "%s: power is off.\n", __func__);
	mutex_unlock(&info->mutex);
	return ret;
}

static int lm3565_get_current_mA(u8 curr)
{
	if (curr >= ARRAY_SIZE(lm3565_def_flash_levels))
		return -ENODEV;

	return lm3565_def_flash_levels[curr].luminance / 1000;
}

static int lm3565_get_lut_index(u16 val, const u16 *lut, int num)
{
	int idx;

	for (idx = num - 1; idx >= 0; idx--)
		if (val >= lut[idx])
			break;

	return idx;
}

static int lm3565_set_leds(struct lm3565_info *info, u8 curr)
{
	int err = 0;
	u8 regs[6];
	u8 om;
	u8 stb;

	if (info->op_mode == LM3565_MODE_STDBY) {
		err = lm3565_reg_wr(info, LM3565_REG_MODE, 0);
		if (!err) {
			info->regs.outmode = 0;
		}
		dev_dbg(info->dev, "%s led disabled\n", __func__);
		goto set_leds_end;
	}

	if (info->op_mode == LM3565_MODE_FLASH) {
		if (curr >= info->max_flash)
			curr = info->max_flash;
		om = 0x0b;
		stb = info->flash_strobe;
	} else {
		if (curr >= info->max_torch)
			curr = info->max_torch;
		curr <<= 4;
		om = 0x0a;
		stb = 0;
	}

	regs[0] = curr;
	regs[1] = info->regs.txmask;
	regs[2] = info->regs.vlow;
	regs[3] = info->new_timer;
	regs[4] = stb;
	regs[5] = om;
	err = lm3565_reg_raw_wr(info, LM3565_REG_SET_CURR, regs, sizeof(regs));
	if (!err) {
		info->regs.led_curr = curr;
		info->regs.ftime = info->new_timer;
		info->regs.outmode = om;
		info->regs.strobe = stb;
	}

	dev_dbg(info->dev, "%s %x %x control = %x %x\n",
		__func__, curr, info->new_timer, info->regs.strobe, om);
set_leds_end:
	return err;
}

static int lm3565_update_settings(struct lm3565_info *info)
{
	int err = 0;
	u8 regs[2];

	if (info->op_mode == LM3565_MODE_STDBY) {
		regs[0] = info->regs.txmask;
		regs[1] = info->regs.vlow;
		err = lm3565_reg_raw_wr(
			info, LM3565_REG_TXMASK, regs, sizeof(regs));
	}

	err |= lm3565_set_leds(info, info->regs.led_curr);

	dev_dbg(info->dev,
		"UP: tx: %x vlow: %x\n", info->regs.txmask, info->regs.vlow);
	return err;
}

static int lm3565_set_txmask(struct lm3565_info *info, bool upd)
{
	struct lm3565_config *p_cfg = &info->config;
	int err = 0;
	u8 tm;

	if (!p_cfg->txmask_current_mA && !p_cfg->txmask_inductor_mA)
		return 0;

	tm = lm3565_get_lut_index(
		p_cfg->txmask_current_mA, tx_c_lut, ARRAY_SIZE(tx_c_lut));
	info->regs.txmask = FIELD(1, 7) | FIELD(tm, 3);

	tm = lm3565_get_lut_index(
		p_cfg->txmask_inductor_mA, tx_i_lut, ARRAY_SIZE(tx_i_lut));
	info->regs.txmask |= FIELD(tm, 1);

	dev_dbg(info->dev, "%s 0x%02x\n", __func__, info->regs.txmask);

	if (upd)
		err = lm3565_reg_wr(info, LM3565_REG_TXMASK, info->regs.txmask);

	return err;
}

static int lm3565_set_vlow(struct lm3565_info *info, bool upd)
{
	struct lm3565_config *pcfg = &info->config;
	int err = 0;
	u8 vl;

	if (!pcfg->vin_low_v_mV && !pcfg->vin_low_c_mA)
		return 0;

	vl = lm3565_get_lut_index(
			pcfg->vin_low_v_mV, v_in_low, ARRAY_SIZE(v_in_low));
	info->regs.vlow = FIELD(1, 6) | FIELD(vl, 3);

	vl = lm3565_get_lut_index(
			pcfg->vin_low_c_mA, c_in_low, ARRAY_SIZE(c_in_low));
	info->regs.vlow |= FIELD(vl, 1);

	if (upd)
		err = lm3565_reg_wr(
			info, LM3565_REG_LOWVOLTAGE, info->regs.vlow);

	return err;
}

static void lm3565_config_init(struct lm3565_info *info)
{
	struct lm3565_config *pcfg = &info->config;
	struct lm3565_config *pcfg_cust = &info->pdata->config;

	dev_dbg(info->dev, "%s\n", __func__);

	memcpy(pcfg, &default_cfg, sizeof(info->config));

	if (pcfg_cust->strobe_type)
		pcfg->strobe_type = pcfg_cust->strobe_type;
	if (pcfg_cust->def_flash_time_mS)
		pcfg->def_flash_time_mS = pcfg_cust->def_flash_time_mS;
	if (pcfg->def_flash_time_mS > 512)
		pcfg->def_flash_time_mS = 512;

	if (pcfg_cust->vin_low_v_mV) {
		if (pcfg_cust->vin_low_v_mV == 0xffff)
			pcfg->vin_low_v_mV = 0;
		else
			pcfg->vin_low_v_mV = pcfg_cust->vin_low_v_mV;
	}

	if (pcfg_cust->txmask_current_mA)
		pcfg->txmask_current_mA = pcfg_cust->txmask_current_mA;

	if (pcfg_cust->txmask_inductor_mA)
		pcfg->txmask_inductor_mA = pcfg_cust->txmask_inductor_mA;

	if (pcfg_cust->max_peak_current_mA)
		pcfg->max_peak_current_mA = pcfg_cust->max_peak_current_mA;

	if (pcfg_cust->max_peak_duration_ms)
		pcfg->max_peak_duration_ms = pcfg_cust->max_peak_duration_ms;

	if (pcfg_cust->max_torch_current_mA)
		pcfg->max_torch_current_mA = pcfg_cust->max_torch_current_mA;

	if (pcfg_cust->max_sustained_current_mA)
		pcfg->max_sustained_current_mA =
			pcfg_cust->max_sustained_current_mA;

	if (pcfg_cust->min_current_mA)
		pcfg->min_current_mA = pcfg_cust->min_current_mA;

}

static inline void lm3565_strobe_init(struct lm3565_info *info)
{
	info->flash_strobe = (info->config.strobe_type == 1) ? 0xc0 :
			(info->config.strobe_type == 2) ? 0x80 : 0x00;
}

static int lm3565_configure(struct lm3565_info *info, bool update)
{
	struct lm3565_config *pcfg = &info->config;
	struct lm3565_caps_struct *pcap = &info->caps;
	struct nvc_torch_flash_capabilities_v1 *pfcap = &info->flash_cap;
	struct nvc_torch_torch_capabilities_v1 *ptcap = &info->torch_cap;
	struct nvc_torch_timer_capabilities_v1 *ptmcap;
	struct nvc_torch_lumi_level_v1 *plvls;
	int i;

	dev_dbg(info->dev, "%s\n", __func__);

	memcpy(&info->query, &lm3565_query, sizeof(info->query));

	lm3565_set_txmask(info, false);
	lm3565_set_vlow(info, false);

	lm3565_strobe_init(info);

	info->regs.ftime = FLASHTIME(pcfg->def_flash_time_mS);

	if (pcfg->max_peak_current_mA > pcap->max_peak_curr_mA ||
		!pcfg->max_peak_current_mA) {
		dev_notice(info->dev,
			"max_peak_current_mA of %d invalid changed to %d\n",
				pcfg->max_peak_current_mA,
				pcap->max_peak_curr_mA);
		pcfg->max_peak_current_mA = pcap->max_peak_curr_mA;
	}

	if (pcfg->max_sustained_current_mA > pcap->max_assist_curr_mA ||
		!pcfg->max_sustained_current_mA) {
		dev_notice(info->dev,
			"max_sustained_current_mA %d invalid changed to %d\n",
				pcfg->max_sustained_current_mA,
				pcap->max_assist_curr_mA);
		pcfg->max_sustained_current_mA =
			pcap->max_assist_curr_mA;
	}
	if ((1000 * pcfg->min_current_mA) < pcap->curr_step_uA) {
		pcfg->min_current_mA = pcap->curr_step_uA / 1000;
		dev_notice(info->dev,
			"min_current_mA lower than possible, increased to %d\n",
				pcfg->min_current_mA);
	}

	pfcap->version = NVC_TORCH_CAPABILITY_VER_1;
	pfcap->led_idx = 0;
	pfcap->attribute = 0;
	pfcap->granularity = pcfg->led_config.granularity;
	pfcap->levels[0].guidenum = LM3565_LEVEL_OFF;
	pfcap->levels[0].luminance = 0;
	plvls = pcfg->led_config.lumi_levels;
	for (i = 1; i < LM3565_MAX_FLASH_LEVEL; i++) {
		if (lm3565_get_current_mA(plvls[i - 1].guidenum) >
			pcfg->max_peak_current_mA)
			break;

		pfcap->levels[i].guidenum = plvls[i - 1].guidenum;
		pfcap->levels[i].luminance = plvls[i - 1].luminance;
		info->max_flash = plvls[i - 1].guidenum;
		dev_dbg(info->dev, "%02d - %d\n",
			pfcap->levels[i].guidenum, pfcap->levels[i].luminance);
	}
	pfcap->numberoflevels = i;

	ptmcap = &info->flash_timeouts;
	pfcap->timeout_off = (void *)ptmcap - (void *)pfcap;
	pfcap->timeout_num = LM3565_FLASH_TIMER_NUM;
	ptmcap->timeout_num = LM3565_FLASH_TIMER_NUM;
	for (i = 0; i < ptmcap->timeout_num; i++) {
		ptmcap->timeouts[i].timeout = 2000 * (i + 1);
		dev_dbg(info->dev,
			"t: %03d - %06d uS\n", i, ptmcap->timeouts[i].timeout);
	}

	ptcap->version = NVC_TORCH_CAPABILITY_VER_1;
	ptcap->led_idx = 0;
	ptcap->attribute = 0;
	ptcap->granularity = pcfg->led_config.granularity;
	ptcap->levels[0].guidenum = LM3565_LEVEL_OFF;
	ptcap->levels[0].luminance = 0;
	for (i = 1; i < LM3565_MAX_TORCH_LEVEL; i++) {
		if (torch_levels[i - 1].luminance / 1000 >
			pcfg->max_torch_current_mA)
			break;

		ptcap->levels[i].guidenum = torch_levels[i - 1].guidenum;
		ptcap->levels[i].luminance = torch_levels[i - 1].luminance;
		info->max_torch = torch_levels[i - 1].guidenum;
	}
	ptcap->numberoflevels = i;
	ptcap->timeout_num = LM3565_TORCH_TIMER_NUM;

	ptmcap = &info->torch_timeouts;
	ptcap->timeout_off = (void *)ptmcap - (void *)ptcap;
	ptmcap->timeout_num = ptcap->timeout_num;
	for (i = 0; i < ptmcap->timeout_num; i++) {
		ptmcap->timeouts[i].timeout = lm3565_torch_timer[i];
		dev_dbg(info->dev, "t: %02d - %d uS\n", i,
			ptmcap->timeouts[i].timeout);
	}

	if (update && info->pwr_state == NVC_PWR_ON)
		return lm3565_update_settings(info);

	return 0;
}

static int lm3565_strobe(struct lm3565_info *info, int t_on)
{
	struct nvc_gpio_pdata *sg = &info->pdata->strobe_gpio;

	if (!sg->gpio_type) {
		dev_err(info->dev, "%s strobe gpio undefined.\n", __func__);
		return -ENODEV;
	};

	if (!sg->active_high)
		t_on ^= 1;
	return gpio_direction_output(sg->gpio, (t_on & 1));
}

#ifdef CONFIG_PM
static int lm3565_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct lm3565_info *info = i2c_get_clientdata(client);

	dev_info(&client->dev, "Suspending %s\n", info->caps.name);

	return 0;
}

static int lm3565_resume(struct i2c_client *client)
{
	struct lm3565_info *info = i2c_get_clientdata(client);

	dev_info(&client->dev, "Resuming %s\n", info->caps.name);

	return 0;
}

static void lm3565_shutdown(struct i2c_client *client)
{
	struct lm3565_info *info = i2c_get_clientdata(client);

	dev_info(&client->dev, "Shutting down %s\n", info->caps.name);

	/* powier off chip to turn off led */
	lm3565_power(info, NVC_PWR_OFF);
}
#endif

static int lm3565_power_on(struct lm3565_info *info)
{
	struct lm3565_power_rail *power = &info->power;
	struct nvc_gpio *gp = &power->enable_gpio;
	int err = 0;

	dev_dbg(info->dev, "%s %d\n", __func__, info->power_on);
	if (info->power_on)
		return 0;

	mutex_lock(&info->mutex);
	if (gp->valid) {
		dev_dbg(info->dev, "gpio %d %d\n", gp->gpio, gp->active_high);
		err = gpio_direction_output(gp->gpio, gp->active_high ? 1 : 0);
		if (err) {
			dev_err(info->dev, "%s gpio err\n", __func__);
			goto power_on_end;
		}
	}
	if (power->v_in) {
		dev_dbg(info->dev, "regulator_enable power->v_in");
		err = regulator_enable(power->v_in);
		if (err) {
			dev_err(info->dev, "%s v_in err\n", __func__);
			goto power_on_end;
		}
	}

	if (power->v_i2c) {
		dev_dbg(info->dev, "regulator_enable power->v_i2c");
		err = regulator_enable(power->v_i2c);
		if (err) {
			dev_err(info->dev, "%s v_i2c err\n", __func__);
			if (power->v_in)
				regulator_disable(power->v_in);
			goto power_on_end;
		}
	}

	if (info->pdata && info->pdata->power_on_callback)
		err = info->pdata->power_on_callback(&info->power);

	if (!err) {
		usleep_range(1000, 1020);
		info->power_on = 1;
	}

power_on_end:
	mutex_unlock(&info->mutex);

	return err;
}

static int lm3565_power_off(struct lm3565_info *info)
{
	struct lm3565_power_rail *power = &info->power;
	struct nvc_gpio *gp = &power->enable_gpio;
	int err = 0;

	dev_dbg(info->dev, "%s %d\n", __func__, info->power_on);
	if (!info->power_on)
		return 0;

	mutex_lock(&info->mutex);
	if (info->pdata && info->pdata->power_off_callback)
		err = info->pdata->power_off_callback(&info->power);
	if (IS_ERR_VALUE(err))
		goto power_off_end;

	if (gp->valid) {
		err = gpio_direction_output(gp->gpio, gp->active_high ? 0 : 1);
		if (err) {
			dev_err(info->dev, "%s gpio err\n", __func__);
			goto power_off_end;
		}
	}

	if (power->v_in) {
		err = regulator_disable(power->v_in);
		if (err) {
			dev_err(info->dev, "%s vi_in err\n", __func__);
			goto power_off_end;
		}
	}

	if (power->v_i2c) {
		err = regulator_disable(power->v_i2c);
		if (err) {
			dev_err(info->dev, "%s vi_i2c err\n", __func__);
			goto power_off_end;
		}
	}
	info->power_on = 0;

power_off_end:
	mutex_unlock(&info->mutex);

	return err;
}

static int lm3565_power(struct lm3565_info *info, int pwr)
{
	int err = 0;

	dev_dbg(info->dev, "%s %d %d\n", __func__, pwr, info->pwr_state);
	if (pwr == info->pwr_state) /* power state no change */
		return 0;

	switch (pwr) {
	case NVC_PWR_OFF:
		err = lm3565_set_leds(info, 0);
		if ((info->pdata->cfg & NVC_CFG_OFF2STDBY) ||
			     (info->pdata->cfg & NVC_CFG_BOOT_INIT))
			pwr = NVC_PWR_STDBY;
		else
			err |= lm3565_power_off(info);
		break;
	case NVC_PWR_STDBY_OFF:
		err = lm3565_set_leds(info, 0);
		if ((info->pdata->cfg & NVC_CFG_OFF2STDBY) ||
			     (info->pdata->cfg & NVC_CFG_BOOT_INIT))
			pwr = NVC_PWR_STDBY;
		else
			err |= lm3565_power_on(info);
		break;
	case NVC_PWR_STDBY:
		err = lm3565_power_on(info);
		err |= lm3565_set_leds(info, 0);
		break;
	case NVC_PWR_COMM:
		err = lm3565_power_on(info);
		break;
	case NVC_PWR_ON:
		err = lm3565_power_on(info);
		if (!err)
			err = lm3565_update_settings(info);
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

static int lm3565_get_dev_id(struct lm3565_info *info)
{
	int pwr = info->pwr_state;
	unsigned int devid;
	unsigned int version;
	int err;

	dev_dbg(info->dev, "%s %02x %d\n", __func__, info->regs.dev_id, pwr);
	/* ChipID[7:3] is a fixed identification B0 */
	if ((info->regs.dev_id & 0x34) == 0x34)
		return 0;

	lm3565_power(info, NVC_PWR_COMM);
	err = lm3565_reg_rd(info, LM3565_REG_CHIPID, &devid);
	if (!err && (devid & 0x34) != 0x34)
		err = -ENODEV;
	else
		err |= lm3565_reg_rd(
			info, LM3565_REG_VERSION, &version);
	lm3565_power(info, pwr);

	dev_dbg(info->dev, "%s: %02x %02x err = %02x\n",
		__func__, devid, version, err);
	if (err)
		dev_err(info->dev, "%s failed.\n", __func__);
	else {
		info->regs.dev_id = (u8)devid;
		info->regs.version = (u8)version;
	}

	return err;
}

static int lm3565_user_get_param(struct lm3565_info *info, long arg)
{
	struct nvc_param params;
	struct nvc_torch_pin_state pinstate;
	const void *data_ptr = NULL;
	int err = 0;
	u32 data_size = 0;
	u8 reg;

	if (copy_from_user(&params,
			(const void __user *)arg,
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
		dev_dbg(info->dev, "%s EXT_FLASH_CAPS\n", __func__);
		if (params.variant >= info->query.flash_num) {
			dev_err(info->dev, "%s unsupported flash index %d.\n",
				__func__, params.variant);
			return -EINVAL;
		}
		data_ptr = &info->flash_cap;
		data_size = lm3565_max_flash_cap_size +
				lm3565_flash_timeout_size;
		break;
	case NVC_PARAM_TORCH_EXT_CAPS:
		dev_dbg(info->dev, "%s EXT_TORCH_CAPS\n", __func__);
		if (params.variant >= info->query.torch_num) {
			dev_err(info->dev, "%s unsupported torch index %d.\n",
				__func__, params.variant);
			return -EINVAL;
		}
		data_ptr = &info->torch_cap;
		data_size = lm3565_max_torch_cap_size +
				lm3565_torch_timeout_size;
		break;
	case NVC_PARAM_FLASH_LEVEL:
		dev_dbg(info->dev, "%s FLASH_LEVEL\n", __func__);
		reg = info->regs.led_curr;
		data_ptr = &reg;
		data_size = sizeof(reg);
		break;
	case NVC_PARAM_TORCH_LEVEL:
		dev_dbg(info->dev, "%s TORCH_LEVEL\n", __func__);
		reg = info->regs.led_curr;
		data_ptr = &reg;
		data_size = sizeof(reg);
		break;
	case NVC_PARAM_FLASH_PIN_STATE:
		dev_dbg(info->dev, "%s FLASH_CAPS\n", __func__);
		/* By default use Active Pin State Setting */
		pinstate = info->pdata->pinstate;
		if ((info->op_mode != LM3565_MODE_FLASH) ||
		    (!info->regs.led_curr))
			pinstate.values ^= 0xffff; /* Inactive Pin Setting */

		dev_dbg(info->dev, "%s FLASH_PIN_STATE: %x&%x\n",
				__func__, pinstate.mask, pinstate.values);
		data_ptr = &pinstate;
		data_size = sizeof(pinstate);
		break;
	default:
		dev_err(info->dev, "%s unsupported parameter: %d\n",
			__func__, params.param);
		err = -EINVAL;
		break;
	}

	if (!err && params.sizeofvalue < data_size) {
		dev_err(info->dev, "%s data size mismatch %d != %d\n",
			__func__, params.sizeofvalue, data_size);
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

static int lm3565_get_levels(struct lm3565_info *info,
			       struct nvc_param *params,
			       bool is_flash,
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

	if (is_flash) {
		dev_dbg(info->dev, "%s FLASH_LEVEL: %d %d\n",
			__func__, plevels->ledmask, plevels->levels[0]);
		op_mode = LM3565_MODE_FLASH;
		p_tm = &info->flash_timeouts;
	} else {
		dev_dbg(info->dev, "%s TORCH_LEVEL: %d %d\n",
			__func__, plevels->ledmask, plevels->levels[0]);
		op_mode = LM3565_MODE_TORCH;
		p_tm = &info->torch_timeouts;
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

	if (plevels->levels[0] == LM3565_LEVEL_OFF)
		plevels->ledmask = 0;

	if (!plevels->ledmask)
		info->op_mode = LM3565_MODE_STDBY;
	else
		info->op_mode = op_mode;

	dev_dbg(info->dev, "Return: %d - %d %d %d\n", info->op_mode,
		plevels->ledmask, plevels->levels[0], plevels->levels[1]);
	return 0;
}

static int lm3565_user_set_param(struct lm3565_info *info, long arg)
{
	struct nvc_param params;
	struct nvc_torch_set_level_v1 led_levels;
	int err = 0;
	u8 val;

	if (copy_from_user(&params,
				(const void __user *)arg,
				sizeof(struct nvc_param))) {
		dev_err(info->dev, "%s %d copy_from_user err\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	switch (params.param) {
	case NVC_PARAM_FLASH_LEVEL:
		lm3565_get_levels(info, &params, true, &led_levels);
		info->new_timer = led_levels.timeout;
		dev_dbg(info->dev, "%s FLASH_LEVEL: %d %d\n",
			__func__, led_levels.levels[0], info->new_timer);
		err = lm3565_set_leds(info, led_levels.levels[0]);
		break;
	case NVC_PARAM_TORCH_LEVEL:
		lm3565_get_levels(info, &params, false, &led_levels);
		info->new_timer = led_levels.timeout;
		dev_dbg(info->dev, "%s TORCH_LEVEL: %d %d\n",
			__func__, led_levels.levels[0], info->new_timer);
		err = lm3565_set_leds(info, led_levels.levels[0]);
		break;
	case NVC_PARAM_FLASH_PIN_STATE:
		if (copy_from_user(&val, (const void __user *)params.p_value,
			sizeof(val))) {
			dev_err(info->dev, "%s %d copy_from_user err\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		dev_dbg(info->dev, "%s FLASH_PIN_STATE: %d\n", __func__, val);
		err = lm3565_strobe(info, val);
		break;
	default:
		dev_err(info->dev, "%s unsupported parameter: %d\n",
			__func__, params.param);
		err = -EINVAL;
		break;
	}

	return err;
}

static long lm3565_ioctl(struct file *file,
			   unsigned int cmd,
			   unsigned long arg)
{
	struct lm3565_info *info = file->private_data;
	int pwr;
	int err = 0;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(NVC_IOCTL_PARAM_WR):
		err = lm3565_user_set_param(info, arg);
		break;
	case _IOC_NR(NVC_IOCTL_PARAM_RD):
		err = lm3565_user_get_param(info, arg);
		break;
	case _IOC_NR(NVC_IOCTL_PWR_WR):
		/* This is a Guaranteed Level of Service (GLOS) call */
		pwr = (int)arg * 2;
		dev_dbg(info->dev, "%s PWR_WR: %d\n", __func__, pwr);
		if (!pwr || (pwr > NVC_PWR_ON)) {
			/* Invalid Power State */
			dev_err(info->dev,
				"unsupported power state - %ld\n", arg);
			err = -EFAULT;
			break;
		}
		err = lm3565_power(info, pwr);
		if (info->pdata->cfg & NVC_CFG_NOERR)
			err = 0;
		break;
	case _IOC_NR(NVC_IOCTL_PWR_RD):
		pwr = info->pwr_state / 2;
		dev_dbg(info->dev, "%s PWR_RD: %d\n", __func__, pwr);
		if (copy_to_user((void __user *)arg, (const void *)&pwr,
				 sizeof(pwr))) {
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

static int lm3565_open(struct inode *inode, struct file *file)
{
	struct lm3565_info *info = NULL;
	struct lm3565_info *pos = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(pos, &lm3565_info_list, list) {
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

static int lm3565_release(struct inode *inode, struct file *file)
{
	struct lm3565_info *info = file->private_data;

	dev_dbg(info->dev, "%s\n", __func__);
	lm3565_power(info, NVC_PWR_OFF);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	return 0;
}

static int lm3565_power_put(struct lm3565_power_rail *pw)
{
	if (likely(pw->v_in))
		regulator_put(pw->v_in);

	if (likely(pw->v_i2c))
		regulator_put(pw->v_i2c);

	if (pw->enable_gpio.flag)
		gpio_free(pw->enable_gpio.gpio);

	memset(&pw->enable_gpio, 0, sizeof(pw->enable_gpio));

	pw->v_in = NULL;
	pw->v_i2c = NULL;

	return 0;
}

static int lm3565_regulator_get(struct lm3565_info *info,
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
		dev_dbg(info->dev, "%s: %s\n",
			__func__, vreg_name);

	*vreg = reg;
	return err;
}

static int lm3565_control_init(struct lm3565_info *info)
{
	struct nvc_gpio_pdata *gp = &info->pdata->enable_gpio;
	struct lm3565_power_rail *pw = &info->power;
	int ret = 0;

	dev_dbg(info->dev, "%s\n", __func__);
	lm3565_regulator_get(info, &pw->v_in, "vin"); /* 3.7v */
	lm3565_regulator_get(info, &pw->v_i2c, "vi2c"); /* 1.8v */
	info->pwr_state = NVC_PWR_OFF;
	if (gp->gpio_type) {
		dev_dbg(info->dev, "gpio: %d %d\n", gp->gpio, gp->active_high);
		pw->enable_gpio.gpio = gp->gpio;
		pw->enable_gpio.active_high = gp->active_high;
		if (gp->init_en) {
			ret = gpio_request(gp->gpio, "lm3565_enable");
			if (ret < 0) {
				dev_err(info->dev,
					"cannot request enable gpio %d\n",
					gp->gpio);
				goto control_init_end;
			}
			pw->enable_gpio.flag = true; /* requested */
			ret = gpio_direction_output(
				gp->gpio, gp->active_high ? 0 : 1);
			if (ret < 0) {
				dev_err(info->dev, "cannot init gpio %d\n",
					gp->gpio);
				goto control_init_end;
			}
			pw->enable_gpio.own = true;
		}
		pw->enable_gpio.valid = true;
	}

	gp = &info->pdata->strobe_gpio;
	if (gp->gpio_type) {
		dev_dbg(info->dev,
			"strobe: %d %d\n", gp->gpio, gp->active_high);
		if (gp->init_en) {
			ret = gpio_request(gp->gpio, "lm3565_strobe");
			if (ret < 0) {
				dev_err(info->dev,
					"cannot request strobe gpio: %d\n",
					gp->gpio);
				goto control_init_end;
			}
			ret = gpio_direction_output(
				gp->gpio, gp->active_high ? 0 : 1);
			if (ret < 0) {
				dev_err(info->dev, "cannot init gpio %d\n",
					gp->gpio);
				gpio_free(gp->gpio);
			}
		}
	}

control_init_end:
	if (ret < 0)
		lm3565_power_put(pw);

	return ret;
}

static inline void lm3565_strobe_free(struct lm3565_info *info)
{
	struct nvc_gpio_pdata *gp = &info->pdata->strobe_gpio;

	if (gp->gpio_type && gp->init_en)
		gpio_free(gp->gpio);
}

static const struct file_operations lm3565_fileops = {
	.owner = THIS_MODULE,
	.open = lm3565_open,
	.unlocked_ioctl = lm3565_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = lm3565_ioctl,
#endif
	.release = lm3565_release,
};

static void lm3565_del(struct lm3565_info *info)
{
	lm3565_power(info, NVC_PWR_OFF);
	lm3565_power_put(&info->power);
	lm3565_strobe_free(info);

	spin_lock(&lm3565_spinlock);
	list_del_rcu(&info->list);
	spin_unlock(&lm3565_spinlock);
	synchronize_rcu();
}

static int lm3565_remove(struct i2c_client *client)
{
	struct lm3565_info *info = i2c_get_clientdata(client);

	dev_dbg(info->dev, "%s\n", __func__);
	misc_deregister(&info->miscdev);
	lm3565_del(info);
	return 0;
}

static int lm3565_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct lm3565_info *info;
	char dname[16];
	int err;

	dev_dbg(&client->dev, "%s\n", __func__);
	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	info->regmap = devm_regmap_init_i2c(client, &lm3565_regmap_config);
	if (IS_ERR(info->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(info->regmap));
		return -ENODEV;
	}

	info->i2c_client = client;
	info->dev = &client->dev;
	if (client->dev.platform_data)
		info->pdata = client->dev.platform_data;
	else {
		info->pdata = &lm3565_default_pdata;
		dev_dbg(&client->dev,
			"%s No platform data.  Using defaults.\n", __func__);
	}

	memcpy(&info->caps, &lm3565_caps, sizeof(info->caps));

	lm3565_config_init(info);

	info->op_mode = LM3565_MODE_STDBY;

	lm3565_configure(info, false);

	i2c_set_clientdata(client, info);
	mutex_init(&info->mutex);
	INIT_LIST_HEAD(&info->list);
	spin_lock(&lm3565_spinlock);
	list_add_rcu(&info->list, &lm3565_info_list);
	spin_unlock(&lm3565_spinlock);

	lm3565_control_init(info);

	err = lm3565_get_dev_id(info);
	if (err < 0) {
		dev_err(&client->dev, "%s device not found\n", __func__);
		if (info->pdata->cfg & NVC_CFG_NODEV) {
			lm3565_del(info);
			return -ENODEV;
		}
	} else
		dev_info(&client->dev, "%s device %02x found\n",
			__func__, info->regs.dev_id);

	if (info->pdata->dev_name != 0)
		strcpy(dname, info->pdata->dev_name);
	else
		strcpy(dname, "lm3565");
	if (info->pdata->num)
		snprintf(dname, sizeof(dname), "%s.%u",
			 dname, info->pdata->num);

	info->miscdev.name = dname;
	info->miscdev.fops = &lm3565_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&info->miscdev)) {
		dev_err(&client->dev, "%s unable to register misc device %s\n",
				__func__, dname);
		lm3565_del(info);
		return -ENODEV;
	}

	lm3565_debugfs_init(info);
	return 0;
}

static int lm3565_status_show(struct seq_file *s, void *data)
{
	struct lm3565_info *k_info = s->private;
	struct lm3565_config *pcfg = &k_info->config;

	pr_info("%s\n", __func__);

	seq_printf(s, "lm3565 status:\n"
		"    Flash type: %s, bus %d, addr: 0x%02x\n\n"
		"    Power State      = %01x\n"
		"    Led Current      = 0x%02x\n"
		"    Flash Mode       = 0x%02x\n"
		"    Flash TimeOut    = 0x%02x\n"
		"    Flash Strobe     = 0x%02x\n"
		"    Max_Peak_Current = 0x%04dmA\n"
		"    TxMask_Current   = 0x%04dmA\n"
		"    TxMask_Inductor  = 0x%04dmA\n"
		"    VIN_low          = 0x%04dmV\n"
		"    CIN_low          = 0x%04dmA\n"
		"    PinState Mask    = 0x%04x\n"
		"    PinState Values  = 0x%04x\n"
		,
		(char *)lm3565_id[k_info->pdata->type + 1].name,
		k_info->i2c_client->adapter->nr,
		k_info->i2c_client->addr,
		k_info->pwr_state,
		k_info->regs.led_curr,
		k_info->op_mode, k_info->regs.ftime,
		pcfg->strobe_type,
		pcfg->max_peak_current_mA,
		pcfg->txmask_current_mA,
		pcfg->txmask_inductor_mA,
		pcfg->vin_low_v_mV,
		pcfg->vin_low_c_mA,
		k_info->pdata->pinstate.mask,
		k_info->pdata->pinstate.values
	);

	return 0;
}

static ssize_t lm3565_attr_set(struct file *s,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct lm3565_info *k_info =
		((struct seq_file *)s->private_data)->private;
	char buf[24];
	int buf_size;
	u32 val = 0;

	pr_info("%s\n", __func__);

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

	pr_info("SYNTAX ERROR: %s\n", buf);
	return -EFAULT;

set_attr:
	pr_info("new data = %x\n", val);
	switch (buf[0]) {
	/* enable/disable power */
	case 'p':
		if (val & 0xffff)
			lm3565_power(k_info, NVC_PWR_ON);
		else
			lm3565_power(k_info, NVC_PWR_OFF);
		break;
	/* change led current */
	case 'c':
		lm3565_set_leds(k_info, val & 0xff);
		break;
	/* modify flash timeout reg */
	case 'f':
		k_info->new_timer = val & 0xff;
		break;
	/* set led work mode/trigger mode */
	case 'x':
		if (val & 0xf)
			k_info->op_mode = val & 0xf;
		if (val & 0xf0) {
			k_info->config.strobe_type = (val & 0xf0) >> 4;
			lm3565_strobe_init(k_info);
		}
		break;
	/* set max_peak_current_mA */
	case 'k':
		if (val & 0xffff)
			k_info->config.max_peak_current_mA = val & 0xffff;
		lm3565_configure(k_info, true);
		break;
	/* change txmask settings */
	case 't':
		if (val & 0x0ffff)
			k_info->config.txmask_current_mA = val & 0x0ffff;
		if (val & 0x0ffff0000)
			k_info->config.txmask_inductor_mA =
						(val >> 16) & 0x0ffff;
		lm3565_set_txmask(k_info, true);
		break;
	/* change low voltage/current settings */
	case 'v':
		if (val & 0xffff)
			k_info->config.vin_low_v_mV = val & 0xffff;
		if (val & 0x0ffff0000)
			k_info->config.vin_low_c_mA =
						(val >> 16) & 0x0ffff;
		lm3565_set_vlow(k_info, true);
		break;
	/* change pinstate setting */
	case 'm':
		k_info->pdata->pinstate.mask = (val >> 16) & 0xffff;
		k_info->pdata->pinstate.values = val & 0xffff;
		break;
	/* trigger an external flash/torch event */
	case 'g':
		lm3565_strobe(k_info, val);
		break;
	}

	return count;
}

static int lm3565_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, lm3565_status_show, inode->i_private);
}

static const struct file_operations lm3565_debugfs_fops = {
	.open = lm3565_debugfs_open,
	.read = seq_read,
	.write = lm3565_attr_set,
	.llseek = seq_lseek,
	.release = single_release,
};

static int lm3565_debugfs_init(struct lm3565_info *info)
{
	struct dentry *d;

	info->d_lm3565 = debugfs_create_dir(
		info->miscdev.this_device->kobj.name, NULL);
	if (info->d_lm3565 == NULL) {
		pr_info("%s: debugfs create dir failed\n", __func__);
		return -ENOMEM;
	}

	d = debugfs_create_file("d", S_IRUGO|S_IWUSR, info->d_lm3565,
		(void *)info, &lm3565_debugfs_fops);
	if (!d) {
		pr_info("%s: debugfs create file failed\n", __func__);
		debugfs_remove_recursive(info->d_lm3565);
		info->d_lm3565 = NULL;
	}

	return -EFAULT;
}

static struct i2c_driver lm3565_driver = {
	.driver = {
		.name = "lm3565",
		.owner = THIS_MODULE,
	},
	.id_table = lm3565_id,
	.probe = lm3565_probe,
	.remove = lm3565_remove,
#ifdef CONFIG_PM
	.shutdown = lm3565_shutdown,
	.suspend  = lm3565_suspend,
	.resume   = lm3565_resume,
#endif
};

module_i2c_driver(lm3565_driver);

MODULE_DESCRIPTION("LM3565 flash/torch driver");
MODULE_AUTHOR("Charlie Huang <chahuang@nvidia.com>");
MODULE_LICENSE("GPL");
