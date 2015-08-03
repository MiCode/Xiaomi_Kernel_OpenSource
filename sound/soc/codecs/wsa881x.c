/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/bitops.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/soundwire/soundwire.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include "wsa881x.h"
#include "wsa881x-temp-sensor.h"

#define WSA881X_ADDR_BITS	16
#define WSA881X_DATA_BITS	8

struct wsa_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *wsa_spkr_sus;
	struct pinctrl_state *wsa_spkr_act;
};

enum {
	G_18DB = 0,
	G_16P5DB,
	G_15DB,
	G_13P5DB,
	G_12DB,
	G_10P5DB,
	G_9DB,
	G_7P5DB,
	G_6DB,
	G_4P5DB,
	G_3DB,
	G_1P5DB,
	G_0DB,
};

enum {
	DISABLE = 0,
	ENABLE,
};

enum {
	SWR_DAC_PORT,
	SWR_COMP_PORT,
	SWR_BOOST_PORT,
	SWR_VISENSE_PORT,
};

struct swr_port {
	u8 port_id;
	u8 ch_mask;
	u32 ch_rate;
	u8 num_ch;
};

/*
 * Private data Structure for wsa881x. All parameters related to
 * WSA881X codec needs to be defined here.
 */
struct wsa881x_priv {
	struct regmap *regmap;
	struct device *dev;
	struct swr_device *swr_slave;
	struct snd_soc_codec *codec;
	bool comp_enable;
	bool boost_enable;
	bool visense_enable;
	struct swr_port port[WSA881X_MAX_SWR_PORTS];
	struct wsa_pinctrl_info pinctrl_info;
	int pd_gpio;
	struct wsa881x_tz_priv tz_pdata;
	int bg_cnt;
	struct mutex bg_lock;
};

#define SWR_SLV_MAX_REG_ADDR	0x390
#define SWR_SLV_START_REG_ADDR	0x40
#define SWR_SLV_MAX_BUF_LEN	20
#define BYTES_PER_LINE		12
#define SWR_SLV_RD_BUF_LEN	8
#define SWR_SLV_WR_BUF_LEN	32
#define SWR_SLV_MAX_DEVICES	2

static struct wsa881x_priv *dbgwsa881x;
static struct dentry *debugfs_wsa881x_dent;
static struct dentry *debugfs_peek;
static struct dentry *debugfs_poke;
static struct dentry *debugfs_reg_dump;
static unsigned int read_data;
static unsigned int devnum;

static int codec_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static int get_parameters(char *buf, u32 *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");
	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtou32(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else
			return -EINVAL;
	}
	return 0;
}

static bool is_swr_slv_reg_readable(int reg)
{
	bool ret = true;

	if (((reg > 0x46) && (reg < 0x4A)) ||
	    ((reg > 0x4A) && (reg < 0x50)) ||
	    ((reg > 0x55) && (reg < 0xE0)) ||
	    ((reg > 0xE0) && (reg < 0xF0)) ||
	    ((reg > 0xF0) && (reg < 0x100)) ||
	    ((reg > 0x105) && (reg < 0x120)) ||
	    ((reg > 0x128) && (reg < 0x130)) ||
	    ((reg > 0x138) && (reg < 0x200)) ||
	    ((reg > 0x205) && (reg < 0x220)) ||
	    ((reg > 0x228) && (reg < 0x230)) ||
	    ((reg > 0x238) && (reg < 0x300)) ||
	    ((reg > 0x305) && (reg < 0x320)) ||
	    ((reg > 0x328) && (reg < 0x330)) ||
	    ((reg > 0x338) && (reg < 0x400)) ||
	    ((reg > 0x405) && (reg < 0x420)))
		ret = false;

	return ret;
}

static ssize_t wsa881x_swrslave_reg_show(char __user *ubuf, size_t count,
					  loff_t *ppos)
{
	int i, reg_val, len;
	ssize_t total = 0;
	char tmp_buf[SWR_SLV_MAX_BUF_LEN];

	if (!ubuf || !ppos || (devnum == 0))
		return 0;

	for (i = (((int) *ppos / BYTES_PER_LINE) + SWR_SLV_START_REG_ADDR);
		i <= SWR_SLV_MAX_REG_ADDR; i++) {
		if (!is_swr_slv_reg_readable(i))
			continue;
		swr_read(dbgwsa881x->swr_slave, devnum,
			i, &reg_val, 1);
		len = snprintf(tmp_buf, 25, "0x%.3x: 0x%.2x\n", i,
			       (reg_val & 0xFF));
		if ((total + len) >= count - 1)
			break;
		if (copy_to_user((ubuf + total), tmp_buf, len)) {
			pr_err("%s: fail to copy reg dump\n", __func__);
			total = -EFAULT;
			goto copy_err;
		}
		*ppos += len;
		total += len;
	}

copy_err:
	return total;
}

static ssize_t codec_debug_read(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	char lbuf[SWR_SLV_RD_BUF_LEN];
	char *access_str;
	ssize_t ret_cnt;

	if (!count || !file || !ppos || !ubuf)
		return -EINVAL;

	access_str = file->private_data;
	if (*ppos < 0)
		return -EINVAL;

	if (!strcmp(access_str, "swrslave_peek")) {
		snprintf(lbuf, sizeof(lbuf), "0x%x\n", (read_data & 0xFF));
		ret_cnt = simple_read_from_buffer(ubuf, count, ppos, lbuf,
					       strnlen(lbuf, 7));
	} else if (!strcmp(access_str, "swrslave_reg_dump")) {
		ret_cnt = wsa881x_swrslave_reg_show(ubuf, count, ppos);
	} else {
		pr_err("%s: %s not permitted to read\n", __func__, access_str);
		ret_cnt = -EPERM;
	}
	return ret_cnt;
}

static ssize_t codec_debug_write(struct file *filp,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char lbuf[SWR_SLV_WR_BUF_LEN];
	int rc;
	u32 param[5];
	char *access_str;

	if (!filp || !ppos || !ubuf)
		return -EINVAL;

	access_str = filp->private_data;
	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';
	if (!strcmp(access_str, "swrslave_poke")) {
		/* write */
		rc = get_parameters(lbuf, param, 3);
		if ((param[0] <= SWR_SLV_MAX_REG_ADDR) && (param[1] <= 0xFF) &&
			(rc == 0))
			swr_write(dbgwsa881x->swr_slave, param[2],
				param[0], &param[1]);
		else
			rc = -EINVAL;
	} else if (!strcmp(access_str, "swrslave_peek")) {
		/* read */
		rc = get_parameters(lbuf, param, 2);
		if ((param[0] <= SWR_SLV_MAX_REG_ADDR) && (rc == 0))
			swr_read(dbgwsa881x->swr_slave, param[1],
				param[0], &read_data, 1);
		else
			rc = -EINVAL;
	} else if (!strcmp(access_str, "swrslave_reg_dump")) {
		/* reg dump */
		rc = get_parameters(lbuf, param, 1);
		if ((rc == 0) && (param[0] > 0) &&
		    (param[0] <= SWR_SLV_MAX_DEVICES))
			devnum = param[0];
		else
			rc = -EINVAL;
	}
	if (rc == 0)
		rc = cnt;
	else
		pr_err("%s: rc = %d\n", __func__, rc);

	return rc;
}

static const struct file_operations codec_debug_ops = {
	.open = codec_debug_open,
	.write = codec_debug_write,
	.read = codec_debug_read,
};

static int wsa881x_boost_ctrl(struct snd_soc_codec *codec, bool enable)
{
	dev_dbg(codec->dev, "%s: enable:%d\n", __func__, enable);
	if (enable)
		snd_soc_update_bits(codec, WSA881X_BOOST_EN_CTL, 0x80, 0x80);
	else
		snd_soc_update_bits(codec, WSA881X_BOOST_EN_CTL, 0x80, 0x00);
	/*
	 * 1.5ms sleep is needed after boost enable/disable as per
	 * HW requirement
	 */
	usleep_range(1500, 1510);
	return 0;
}

static int wsa881x_visense_txfe_ctrl(struct snd_soc_codec *codec, bool enable,
				     u8 isense1_gain, u8 isense2_gain,
				     u8 vsense_gain)
{
	u8 value = 0;
	dev_dbg(codec->dev,
		"%s: enable:%d, isense1 gain: %d, isense2 gain: %d, vsense_gain %d\n",
		__func__, enable, isense1_gain, isense2_gain, vsense_gain);

	if (enable) {
		snd_soc_update_bits(codec, WSA881X_SPKR_PROT_FE_VSENSE_VCM,
				    0x08, 0x00);
		snd_soc_update_bits(codec, WSA881X_SPKR_PROT_ATEST2,
				    0x08, 0x08);
		snd_soc_update_bits(codec, WSA881X_SPKR_PROT_ATEST2,
				    0x02, 0x02);
		value = ((isense2_gain << 6) | (isense1_gain << 4) |
			(vsense_gain << 3));
		snd_soc_update_bits(codec, WSA881X_SPKR_PROT_FE_GAIN,
				    0xF8, value);
		snd_soc_update_bits(codec, WSA881X_SPKR_PROT_FE_GAIN,
				    0x01, 0x01);
	} else {
		snd_soc_update_bits(codec, WSA881X_SPKR_PROT_FE_VSENSE_VCM,
				    0x08, 0x08);
		/*
		 * 200us sleep is needed after visense txfe disable as per
		 * HW requirement.
		 */
		usleep_range(200, 210);
		snd_soc_update_bits(codec, WSA881X_SPKR_PROT_FE_GAIN,
				    0x01, 0x00);
	}
	return 0;
}

static int wsa881x_visense_adc_ctrl(struct snd_soc_codec *codec, bool enable)
{

	dev_dbg(codec->dev, "%s: enable:%d\n", __func__, enable);
	snd_soc_update_bits(codec, WSA881X_ADC_EN_MODU_V, (0x01 << 7),
			    (enable << 7));
	snd_soc_update_bits(codec, WSA881X_ADC_EN_MODU_I, (0x01 << 7),
			    (enable << 7));
	return 0;
}

static int wsa881x_bandgap_ctrl(struct snd_soc_codec *codec, bool enable)
{
	struct wsa881x_priv *wsa881x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: enable:%d, bg_count:%d\n", __func__,
		enable, wsa881x->bg_cnt);
	mutex_lock(&wsa881x->bg_lock);
	if (enable) {
		++wsa881x->bg_cnt;
		if (wsa881x->bg_cnt == 1) {
			snd_soc_update_bits(codec, WSA881X_TEMP_OP,
					    0x08, 0x08);
			/* 400usec sleep is needed as per HW requirement */
			usleep_range(400, 410);
		}
	} else {
		--wsa881x->bg_cnt;
		if (wsa881x->bg_cnt <= 0) {
			WARN_ON(wsa881x->bg_cnt < 0);
			wsa881x->bg_cnt = 0;
			snd_soc_update_bits(codec, WSA881X_TEMP_OP, 0x08, 0x00);
		}
	}
	mutex_unlock(&wsa881x->bg_lock);
	return 0;
}

static int wsa881x_get_compander(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wsa881x_priv *wsa881x = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = wsa881x->comp_enable;
	return 0;
}

static int wsa881x_set_compander(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wsa881x_priv *wsa881x = snd_soc_codec_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: Compander enable current %d, new %d\n",
		 __func__, wsa881x->comp_enable, value);
	wsa881x->comp_enable = value;
	return 0;
}

static int wsa881x_get_boost(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wsa881x_priv *wsa881x = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = wsa881x->boost_enable;
	return 0;
}

static int wsa881x_set_boost(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wsa881x_priv *wsa881x = snd_soc_codec_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: Boost enable current %d, new %d\n",
		 __func__, wsa881x->boost_enable, value);
	wsa881x->boost_enable = value;
	return 0;
}

static int wsa881x_get_visense(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wsa881x_priv *wsa881x = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = wsa881x->visense_enable;
	return 0;
}

static int wsa881x_set_visense(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wsa881x_priv *wsa881x = snd_soc_codec_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: VIsense enable current %d, new %d\n",
		 __func__, wsa881x->visense_enable, value);
	wsa881x->visense_enable = value;
	return 0;
}

static const struct snd_kcontrol_new wsa881x_snd_controls[] = {
	SOC_SINGLE_EXT("COMP Switch", SND_SOC_NOPM, 0, 1, 0,
		wsa881x_get_compander, wsa881x_set_compander),

	SOC_SINGLE_EXT("BOOST Switch", SND_SOC_NOPM, 0, 1, 0,
		wsa881x_get_boost, wsa881x_set_boost),

	SOC_SINGLE_EXT("VISENSE Switch", SND_SOC_NOPM, 0, 1, 0,
		wsa881x_get_visense, wsa881x_set_visense),
};

static const struct snd_kcontrol_new swr_dac_port[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static int wsa881x_set_port(struct snd_soc_codec *codec, int port_idx,
			u8 *port_id, u8 *num_ch, u8 *ch_mask, u32 *ch_rate)
{
	struct wsa881x_priv *wsa881x = snd_soc_codec_get_drvdata(codec);

	*port_id = wsa881x->port[port_idx].port_id;
	*num_ch = wsa881x->port[port_idx].num_ch;
	*ch_mask = wsa881x->port[port_idx].ch_mask;
	*ch_rate = wsa881x->port[port_idx].ch_rate;
	return 0;
}

static int wsa881x_enable_swr_dac_port(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct wsa881x_priv *wsa881x = snd_soc_codec_get_drvdata(codec);
	u8 port_id[WSA881X_MAX_SWR_PORTS];
	u8 num_ch[WSA881X_MAX_SWR_PORTS];
	u8 ch_mask[WSA881X_MAX_SWR_PORTS];
	u32 ch_rate[WSA881X_MAX_SWR_PORTS];
	u8 num_port = 0;

	dev_dbg(codec->dev, "%s: event %d name %s\n", __func__,
		event, w->name);
	if (wsa881x == NULL)
		return -EINVAL;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wsa881x_set_port(codec, SWR_DAC_PORT,
				&port_id[num_port], &num_ch[num_port],
				&ch_mask[num_port], &ch_rate[num_port]);
		++num_port;

		if (wsa881x->comp_enable) {
			wsa881x_set_port(codec, SWR_COMP_PORT,
					&port_id[num_port], &num_ch[num_port],
					&ch_mask[num_port], &ch_rate[num_port]);
			++num_port;
		}
		if (wsa881x->boost_enable) {
			wsa881x_set_port(codec, SWR_BOOST_PORT,
					&port_id[num_port], &num_ch[num_port],
					&ch_mask[num_port], &ch_rate[num_port]);
			++num_port;
		}
		if (wsa881x->visense_enable) {
			wsa881x_set_port(codec, SWR_VISENSE_PORT,
					&port_id[num_port], &num_ch[num_port],
					&ch_mask[num_port], &ch_rate[num_port]);
			++num_port;
		}
		swr_connect_port(wsa881x->swr_slave, &port_id[0], num_port,
				&ch_mask[0], &ch_rate[0], &num_ch[0]);
		break;
	case SND_SOC_DAPM_POST_PMU:
		break;
	case SND_SOC_DAPM_PRE_PMD:
		break;
	case SND_SOC_DAPM_POST_PMD:
		port_id[num_port] = wsa881x->port[SWR_DAC_PORT].port_id;
		++num_port;
		if (wsa881x->comp_enable) {
			port_id[num_port] =
				wsa881x->port[SWR_COMP_PORT].port_id;
			++num_port;
		}
		if (wsa881x->boost_enable) {
			port_id[num_port] =
				wsa881x->port[SWR_BOOST_PORT].port_id;
			++num_port;
		}
		if (wsa881x->visense_enable) {
			port_id[num_port] =
				wsa881x->port[SWR_VISENSE_PORT].port_id;
			++num_port;
		}
		swr_disconnect_port(wsa881x->swr_slave, &port_id[0], num_port);
		break;
	default:
		break;
	}
	return 0;
}

static int wsa881x_rdac_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct wsa881x_priv *wsa881x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: %s %d boost %d visense %d\n", __func__,
		w->name, event,	wsa881x->boost_enable,
		wsa881x->visense_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_write(codec, WSA881X_CDC_DIG_CLK_CTL, 0x01);
		snd_soc_write(codec, WSA881X_CDC_ANA_CLK_CTL, 0x01);
		wsa881x_bandgap_ctrl(codec, ENABLE);
		if (wsa881x->boost_enable)
			wsa881x_boost_ctrl(codec, ENABLE);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (wsa881x->boost_enable)
			wsa881x_boost_ctrl(codec, DISABLE);
		snd_soc_write(codec, WSA881X_CDC_ANA_CLK_CTL, 0x00);
		snd_soc_write(codec, WSA881X_CDC_DIG_CLK_CTL, 0x00);
		wsa881x_bandgap_ctrl(codec, DISABLE);
		break;
	}
	return 0;
}

static int wsa881x_ramp_pa_gain(struct snd_soc_codec *codec,
				int min_gain, int max_gain, int udelay)
{
	int val;
	for (val = min_gain; max_gain <= val; val--) {
		snd_soc_update_bits(codec, WSA881X_SPKR_DRV_GAIN,
				    0xF0, val << 4);
		/*
		 * 1ms delay is needed for every step change in gain as per
		 * HW requirement.
		 */
		usleep_range(udelay, udelay+10);
	}
	return 0;
}

static int wsa881x_spkr_pa_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct wsa881x_priv *wsa881x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: %s %d\n", __func__, w->name, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, WSA881X_SPKR_DRV_GAIN, 0xF0, 0x40);
		snd_soc_update_bits(codec, WSA881X_SPKR_MISC_CTL1, 0x01, 0x01);
		snd_soc_update_bits(codec, WSA881X_ADC_EN_DET_TEST_I,
				    0x01, 0x01);
		snd_soc_update_bits(codec, WSA881X_ADC_EN_MODU_V, 0x02, 0x02);
		snd_soc_update_bits(codec, WSA881X_ADC_EN_DET_TEST_V,
				    0x10, 0x10);
		snd_soc_update_bits(codec, WSA881X_SPKR_PWRSTG_DBG, 0xE0, 0xA0);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/*
		 * 710us delay is needed after PA enable as per
		 * HW requirement.
		 */
		usleep_range(710, 720);
		snd_soc_update_bits(codec, WSA881X_SPKR_PWRSTG_DBG, 0xE0, 0x00);
		snd_soc_update_bits(codec, WSA881X_ADC_EN_DET_TEST_V,
				    0x10, 0x00);
		snd_soc_update_bits(codec, WSA881X_ADC_EN_MODU_V, 0x02, 0x00);
		snd_soc_update_bits(codec, WSA881X_ADC_EN_DET_TEST_I,
				    0x01, 0x00);
		/*
		 * 1ms delay is needed before change in gain as per
		 * HW requirement.
		 */
		usleep_range(1000, 1010);
		wsa881x_ramp_pa_gain(codec, G_12DB, G_13P5DB, 1000);
		snd_soc_update_bits(codec, WSA881X_ADC_SEL_IBIAS, 0x70, 0x40);
		if (wsa881x->visense_enable) {
			wsa881x_visense_txfe_ctrl(codec, ENABLE,
						0x00, 0x03, 0x01);
			snd_soc_update_bits(codec, WSA881X_ADC_EN_SEL_IBAIS,
					    0x07, 0x01);
			wsa881x_visense_adc_ctrl(codec, ENABLE);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (wsa881x->visense_enable) {
			wsa881x_visense_adc_ctrl(codec, DISABLE);
			wsa881x_visense_txfe_ctrl(codec, DISABLE,
						0x00, 0x01, 0x01);
		}
		break;
	}
	return 0;
}

static const struct snd_soc_dapm_widget wsa881x_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("IN"),

	SND_SOC_DAPM_MIXER_E("SWR DAC_Port", SND_SOC_NOPM, 0, 0, swr_dac_port,
		ARRAY_SIZE(swr_dac_port), wsa881x_enable_swr_dac_port,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("RDAC", NULL, WSA881X_SPKR_DAC_CTL, 7, 0,
		wsa881x_rdac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("SPKR PGA", WSA881X_SPKR_DRV_EN, 7, 0, NULL, 0,
			wsa881x_spkr_pa_event, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUTPUT("SPKR"),
};

static const struct snd_soc_dapm_route wsa881x_audio_map[] = {
	{"SWR DAC_Port", "Switch", "IN"},
	{"RDAC", NULL, "SWR DAC_Port"},
	{"SPKR PGA", NULL, "RDAC"},
	{"SPKR", NULL, "SPKR PGA"},
};

int wsa881x_set_channel_map(struct snd_soc_codec *codec, u8 *port, u8 num_port,
				unsigned int *ch_mask, unsigned int *ch_rate)
{
	struct wsa881x_priv *wsa881x = snd_soc_codec_get_drvdata(codec);
	int i;

	if (!port || !ch_mask || !ch_rate ||
		(num_port > WSA881X_MAX_SWR_PORTS)) {
		dev_err(codec->dev,
			"%s: Invalid port=%p, ch_mask=%p, ch_rate=%p\n",
			__func__, port, ch_mask, ch_rate);
		return -EINVAL;
	}
	for (i = 0; i < num_port; i++) {
		wsa881x->port[i].port_id = port[i];
		wsa881x->port[i].ch_mask = ch_mask[i];
		wsa881x->port[i].ch_rate = ch_rate[i];
		wsa881x->port[i].num_ch = __sw_hweight8(ch_mask[i]);
	}
	return 0;
}
EXPORT_SYMBOL(wsa881x_set_channel_map);

static void wsa881x_init(struct snd_soc_codec *codec)
{
	/* Bring out of analog reset */
	snd_soc_update_bits(codec, WSA881X_CDC_RST_CTL, 0x02, 0x02);
	/* Bring out of digital reset */
	snd_soc_update_bits(codec, WSA881X_CDC_RST_CTL, 0x01, 0x01);
	/* Set DAC polarity to Rising */
	snd_soc_update_bits(codec, WSA881X_SPKR_DAC_CTL, 0x02, 0x02);
	/* set Bias Ref ctrl to 1.225V */
	snd_soc_update_bits(codec, WSA881X_BIAS_REF_CTRL, 0x07, 0x00);
	snd_soc_update_bits(codec, WSA881X_SPKR_BBM_CTL, 0x02, 0x02);
	snd_soc_update_bits(codec, WSA881X_SPKR_MISC_CTL1, 0xC0, 0x00);
	snd_soc_update_bits(codec, WSA881X_SPKR_MISC_CTL2, 0x07, 0x04);
	snd_soc_update_bits(codec, WSA881X_SPKR_BIAS_INT, 0x0F, 0x0F);
	snd_soc_update_bits(codec, WSA881X_SPKR_PA_INT, 0xF0, 0x10);
	snd_soc_update_bits(codec, WSA881X_SPKR_PA_INT, 0x0F, 0x0E);
	snd_soc_update_bits(codec, WSA881X_BOOST_PS_CTL, 0x80, 0x00);
	snd_soc_update_bits(codec, WSA881X_BOOST_PRESET_OUT1, 0xF0, 0xB0);
	snd_soc_update_bits(codec, WSA881X_BOOST_PRESET_OUT2, 0xF0, 0x30);
	snd_soc_update_bits(codec, WSA881X_SPKR_DRV_EN, 0x0F, 0x0C);
	snd_soc_update_bits(codec, WSA881X_BOOST_CURRENT_LIMIT, 0x0F, 0x08);
	snd_soc_update_bits(codec, WSA881X_BOOST_ZX_CTL, 0x20, 0x00);
}

static int32_t wsa881x_resource_acquire(struct snd_soc_codec *codec,
						bool enable)
{
	return wsa881x_bandgap_ctrl(codec, enable);
}

static int wsa881x_probe(struct snd_soc_codec *codec)
{
	struct wsa881x_priv *wsa881x = snd_soc_codec_get_drvdata(codec);
	struct swr_device *dev;
	u8 devnum = 0;
	int ret;

	if (!wsa881x)
		return -EINVAL;

	dev = wsa881x->swr_slave;
	wsa881x->codec = codec;
	/*
	 * Add 5msec delay to provide sufficient time for
	 * soundwire auto enumeration of slave devices as
	 * as per HW requirement.
	 */
	usleep_range(5000, 5010);
	ret = swr_get_logical_dev_num(dev, dev->addr, &devnum);
	if (ret) {
		dev_err(codec->dev, "%s failed to get devnum, err:%d\n",
			__func__, ret);
		return ret;
	}
	dev->dev_num = devnum;
	codec->control_data = wsa881x->regmap;
	ret = snd_soc_codec_set_cache_io(codec, WSA881X_ADDR_BITS,
					WSA881X_DATA_BITS, SND_SOC_REGMAP);
	if (ret != 0) {
		dev_err(codec->dev, "%s: failed to set cache_io %d\n",
			__func__, ret);
	}
	mutex_init(&wsa881x->bg_lock);
	snprintf(wsa881x->tz_pdata.name, sizeof(wsa881x->tz_pdata.name),
		"%s.%x", "wsatz", (u8)dev->addr);
	wsa881x->bg_cnt = 0;
	wsa881x->tz_pdata.codec = codec;
	wsa881x->tz_pdata.dig_base = WSA881X_DIGITAL_BASE;
	wsa881x->tz_pdata.ana_base = WSA881X_ANALOG_BASE;
	wsa881x->tz_pdata.wsa_resource_acquire = wsa881x_resource_acquire;
	wsa881x_init_thermal(&wsa881x->tz_pdata);
	wsa881x_init(codec);

	return ret;
}

static int wsa881x_remove(struct snd_soc_codec *codec)
{
	struct wsa881x_priv *wsa881x = snd_soc_codec_get_drvdata(codec);

	if (wsa881x->tz_pdata.tz_dev)
		wsa881x_deinit_thermal(wsa881x->tz_pdata.tz_dev);
	mutex_destroy(&wsa881x->bg_lock);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_wsa881x = {
	.probe = wsa881x_probe,
	.remove = wsa881x_remove,
	.controls = wsa881x_snd_controls,
	.num_controls = ARRAY_SIZE(wsa881x_snd_controls),
	.dapm_widgets = wsa881x_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(wsa881x_dapm_widgets),
	.dapm_routes = wsa881x_audio_map,
	.num_dapm_routes = ARRAY_SIZE(wsa881x_audio_map),
};

static int wsa881x_gpio_ctrl(struct wsa881x_priv *wsa881x, bool enable)
{
	if (wsa881x->pd_gpio < 0) {
		dev_err(wsa881x->dev, "%s: gpio is not valid %d\n",
			__func__, wsa881x->pd_gpio);
		return -EINVAL;
	}
	gpio_direction_output(wsa881x->pd_gpio, enable);
	return 0;
}

static int wsa881x_gpio_init(struct swr_device *pdev)
{
	int ret = 0;
	struct wsa881x_priv *wsa881x;

	wsa881x = swr_get_dev_data(pdev);
	if (!wsa881x) {
		dev_err(&pdev->dev, "%s: wsa881x is NULL\n", __func__);
		return -EINVAL;
	}
	dev_dbg(&pdev->dev, "%s: gpio %d request with name %s\n",
		__func__, wsa881x->pd_gpio, dev_name(&pdev->dev));
	ret = gpio_request(wsa881x->pd_gpio, dev_name(&pdev->dev));
	if (ret)
		dev_err(&pdev->dev, "%s: Failed to request gpio %d, err: %d\n",
			__func__, wsa881x->pd_gpio, ret);
	return ret;
}

static int wsa881x_pinctrl_init(struct wsa881x_priv *wsa881x,
						struct swr_device *pdev)
{
	struct pinctrl *pinctrl;

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl)) {
			pr_err("%s: Unable to get pinctrl handle\n",
					__func__);
			return -EINVAL;
	}
	wsa881x->pinctrl_info.pinctrl = pinctrl;

	wsa881x->pinctrl_info.wsa_spkr_act = pinctrl_lookup_state(pinctrl,
							"wsa_spkr_sd_act");
	if (IS_ERR(wsa881x->pinctrl_info.wsa_spkr_act)) {
		pr_err("%s: Unable to get pinctrl disable state handle\n",
							__func__);
		return -EINVAL;
	}
	wsa881x->pinctrl_info.wsa_spkr_sus = pinctrl_lookup_state(pinctrl,
		"wsa_spkr_sd_sus");
	if (IS_ERR(wsa881x->pinctrl_info.wsa_spkr_sus)) {
		pr_err("%s: Unable to get pinctrl disable state handle\n",
							__func__);
		return -EINVAL;
	}
	return 0;
}

static int wsa881x_swr_probe(struct swr_device *pdev)
{
	int ret = 0;
	struct wsa881x_priv *wsa881x;

	wsa881x = devm_kzalloc(&pdev->dev, sizeof(struct wsa881x_priv),
			    GFP_KERNEL);
	if (!wsa881x) {
		dev_err(&pdev->dev, "%s: cannot create memory for wsa881x\n",
			__func__);
		return -ENOMEM;
	}
	swr_set_dev_data(pdev, wsa881x);

	wsa881x->regmap = devm_regmap_init_swr(pdev, &wsa881x_regmap_config);
	if (IS_ERR(wsa881x->regmap)) {
		ret = PTR_ERR(wsa881x->regmap);
		dev_err(&pdev->dev, "%s: regmap_init failed %d\n",
			__func__, ret);
		goto err;
	}
	wsa881x->swr_slave = pdev;

	ret = wsa881x_pinctrl_init(wsa881x, pdev);
	if (ret < 0) {
		wsa881x->pd_gpio = of_get_named_gpio(pdev->dev.of_node,
				"qcom,spkr-sd-n-gpio", 0);
		if (wsa881x->pd_gpio < 0) {
			dev_err(&pdev->dev, "%s: %s property is not found %d\n",
					__func__, "qcom,spkr-sd-n-gpio",
					wsa881x->pd_gpio);
			return -EINVAL;
		}
		dev_dbg(&pdev->dev, "%s: reset gpio %d\n", __func__,
				wsa881x->pd_gpio);
		ret = wsa881x_gpio_init(pdev);
		if (ret)
			goto err;
		wsa881x_gpio_ctrl(wsa881x, true);
	} else {
		ret = pinctrl_select_state(wsa881x->pinctrl_info.pinctrl,
				wsa881x->pinctrl_info.wsa_spkr_act);
		if (ret) {
			dev_dbg(&pdev->dev, "%s: pinctrl act failed for wsa\n",
					__func__);
		}
	}

	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_wsa881x,
				     NULL, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: Codec registration failed\n",
			__func__);
		goto err1;
	}
	if (!debugfs_wsa881x_dent) {
		dbgwsa881x = wsa881x;
		debugfs_wsa881x_dent = debugfs_create_dir(
						"wsa881x_swr_slave", 0);
		if (!IS_ERR(debugfs_wsa881x_dent)) {
			debugfs_peek = debugfs_create_file("swrslave_peek",
					S_IFREG | S_IRUGO, debugfs_wsa881x_dent,
					(void *) "swrslave_peek",
					&codec_debug_ops);

			debugfs_poke = debugfs_create_file("swrslave_poke",
					S_IFREG | S_IRUGO, debugfs_wsa881x_dent,
					(void *) "swrslave_poke",
					&codec_debug_ops);

			debugfs_reg_dump = debugfs_create_file(
						"swrslave_reg_dump",
						S_IFREG | S_IRUGO,
						debugfs_wsa881x_dent,
						(void *) "swrslave_reg_dump",
						&codec_debug_ops);
		}
	}
	return 0;

err1:
	if (wsa881x->pd_gpio)
		gpio_free(wsa881x->pd_gpio);
err:
	return ret;
}

static int wsa881x_swr_remove(struct swr_device *pdev)
{
	struct wsa881x_priv *wsa881x;

	wsa881x = swr_get_dev_data(pdev);
	debugfs_remove_recursive(debugfs_wsa881x_dent);
	if (!wsa881x) {
		dev_err(&pdev->dev, "%s: wsa881x is NULL\n", __func__);
		return -EINVAL;
	}
	snd_soc_unregister_codec(&pdev->dev);
	if (wsa881x->pd_gpio)
		gpio_free(wsa881x->pd_gpio);
	swr_set_dev_data(pdev, NULL);
	return 0;
}

static int wsa881x_swr_up(struct swr_device *pdev)
{
	int ret;
	struct wsa881x_priv *wsa881x;

	wsa881x = swr_get_dev_data(pdev);
	if (!wsa881x) {
		dev_err(&pdev->dev, "%s: wsa881x is NULL\n", __func__);
		return -EINVAL;
	}
	ret = wsa881x_gpio_ctrl(wsa881x, true);
	if (ret)
		dev_err(&pdev->dev, "%s: Failed to enable gpio\n", __func__);

	return ret;
}

static int wsa881x_swr_down(struct swr_device *pdev)
{
	struct wsa881x_priv *wsa881x;
	int ret;

	wsa881x = swr_get_dev_data(pdev);
	if (!wsa881x) {
		dev_err(&pdev->dev, "%s: wsa881x is NULL\n", __func__);
		return -EINVAL;
	}
	regcache_mark_dirty(wsa881x->regmap);
	ret = wsa881x_gpio_ctrl(wsa881x, false);
	if (ret)
		dev_err(&pdev->dev, "%s: Failed to disable gpio\n", __func__);

	return ret;
}

static int wsa881x_swr_reset(struct swr_device *pdev)
{
	struct wsa881x_priv *wsa881x;

	wsa881x = swr_get_dev_data(pdev);
	if (!wsa881x) {
		dev_err(&pdev->dev, "%s: wsa881x is NULL\n", __func__);
		return -EINVAL;
	}
	wsa881x->bg_cnt = 0;
	regcache_sync(wsa881x->regmap);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int wsa881x_swr_suspend(struct device *dev)
{
	dev_dbg(dev, "%s: system suspend\n", __func__);
	return 0;
}

static int wsa881x_swr_resume(struct device *dev)
{
	struct wsa881x_priv *wsa881x = swr_get_dev_data(to_swr_device(dev));

	if (!wsa881x) {
		dev_err(dev, "%s: wsa881x private data is NULL\n", __func__);
		return -EINVAL;
	}
	dev_dbg(dev, "%s: system resume\n", __func__);
	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops wsa881x_swr_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(wsa881x_swr_suspend, wsa881x_swr_resume)
};

static const struct swr_device_id wsa881x_swr_id[] = {
	{"wsa881x", 0},
	{}
};

static struct of_device_id wsa881x_swr_dt_match[] = {
	{
		.compatible = "qcom,wsa881x",
	},
	{}
};

static struct swr_driver wsa881x_codec_driver = {
	.driver = {
		.name = "wsa881x",
		.owner = THIS_MODULE,
		.pm = &wsa881x_swr_pm_ops,
		.of_match_table = wsa881x_swr_dt_match,
	},
	.probe = wsa881x_swr_probe,
	.remove = wsa881x_swr_remove,
	.id_table = wsa881x_swr_id,
	.device_up = wsa881x_swr_up,
	.device_down = wsa881x_swr_down,
	.reset_device = wsa881x_swr_reset,
};

static int __init wsa881x_codec_init(void)
{
	return swr_driver_register(&wsa881x_codec_driver);
}

static void __exit wsa881x_codec_exit(void)
{
	swr_driver_unregister(&wsa881x_codec_driver);
}

module_init(wsa881x_codec_init);
module_exit(wsa881x_codec_exit);

MODULE_DESCRIPTION("WSA881x Codec driver");
MODULE_LICENSE("GPL v2");
