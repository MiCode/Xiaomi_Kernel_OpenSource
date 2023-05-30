// SPDX-License-Identifier: GPL-2.0

/*
 * cs35l41.c -- CS35l41 ALSA SoC audio driver
 *
 * Copyright 2017-2020 Cirrus Logic, Inc.
 *
 * Author:	David Rhodes	<david.rhodes@cirrus.com>
 *		Brian Austin	<brian.austin@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#define xDEBUG
#define FAST_SWITCH_WORKAROUND

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/gpio.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/of_irq.h>
#include <linux/completion.h>
#include <linux/spi/spi.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/timekeeping.h>

#include "wm_adsp.h"
#include "cs35l41.h"
#include <sound/cs35l41.h>

static const char * const cs35l41_supplies[] = {
	"VA",
	"VP",
};

struct cs35l41_pll_sysclk_config {
	int freq;
	int clk_cfg;
};

static const struct cs35l41_pll_sysclk_config cs35l41_pll_sysclk[] = {
	{ 32768,	0x00 },
	{ 8000,		0x01 },
	{ 11025,	0x02 },
	{ 12000,	0x03 },
	{ 16000,	0x04 },
	{ 22050,	0x05 },
	{ 24000,	0x06 },
	{ 32000,	0x07 },
	{ 44100,	0x08 },
	{ 48000,	0x09 },
	{ 88200,	0x0A },
	{ 96000,	0x0B },
	{ 128000,	0x0C },
	{ 176400,	0x0D },
	{ 192000,	0x0E },
	{ 256000,	0x0F },
	{ 352800,	0x10 },
	{ 384000,	0x11 },
	{ 512000,	0x12 },
	{ 705600,	0x13 },
	{ 750000,	0x14 },
	{ 768000,	0x15 },
	{ 1000000,	0x16 },
	{ 1024000,	0x17 },
	{ 1200000,	0x18 },
	{ 1411200,	0x19 },
	{ 1500000,	0x1A },
	{ 1536000,	0x1B },
	{ 2000000,	0x1C },
	{ 2048000,	0x1D },
	{ 2400000,	0x1E },
	{ 2822400,	0x1F },
	{ 3000000,	0x20 },
	{ 3072000,	0x21 },
	{ 3200000,	0x22 },
	{ 4000000,	0x23 },
	{ 4096000,	0x24 },
	{ 4800000,	0x25 },
	{ 5644800,	0x26 },
	{ 6000000,	0x27 },
	{ 6144000,	0x28 },
	{ 6250000,	0x29 },
	{ 6400000,	0x2A },
	{ 6500000,	0x2B },
	{ 6750000,	0x2C },
	{ 7526400,	0x2D },
	{ 8000000,	0x2E },
	{ 8192000,	0x2F },
	{ 9600000,	0x30 },
	{ 11289600,	0x31 },
	{ 12000000,	0x32 },
	{ 12288000,	0x33 },
	{ 12500000,	0x34 },
	{ 12800000,	0x35 },
	{ 13000000,	0x36 },
	{ 13500000,	0x37 },
	{ 19200000,	0x38 },
	{ 22579200,	0x39 },
	{ 24000000,	0x3A },
	{ 24576000,	0x3B },
	{ 25000000,	0x3C },
	{ 25600000,	0x3D },
	{ 26000000,	0x3E },
	{ 27000000,	0x3F },
};

struct cs35l41_fs_mon_config {
	int freq;
	unsigned int fs1;
	unsigned int fs2;
};

static const struct cs35l41_fs_mon_config cs35l41_fs_mon[] = {
	{ 32768,	2254,	3754 },
	{ 8000,		9220,	15364 },
	{ 11025,	6148,	10244 },
	{ 12000,	6148,	10244 },
	{ 16000,	4612,	7684 },
	{ 22050,	3076,	5124 },
	{ 24000,	3076,	5124 },
	{ 32000,	2308,	3844 },
	{ 44100,	1540,	2564 },
	{ 48000,	1540,	2564 },
	{ 88200,	772,	1284 },
	{ 96000,	772,	1284 },
	{ 128000,	580,	964 },
	{ 176400,	388,	644 },
	{ 192000,	388,	644 },
	{ 256000,	292,	484 },
	{ 352800,	196,	324 },
	{ 384000,	196,	324 },
	{ 512000,	148,	244 },
	{ 705600,	100,	164 },
	{ 750000,	100,	164 },
	{ 768000,	100,	164 },
	{ 1000000,	76,	124 },
	{ 1024000,	76,	124 },
	{ 1200000,	64,	104 },
	{ 1411200,	52,	84 },
	{ 1500000,	52,	84 },
	{ 1536000,	52,	84 },
	{ 2000000,	40,	64 },
	{ 2048000,	40,	64 },
	{ 2400000,	34,	54 },
	{ 2822400,	28,	44 },
	{ 3000000,	28,	44 },
	{ 3072000,	28,	44 },
	{ 3200000,	27,	42 },
	{ 4000000,	22,	34 },
	{ 4096000,	22,	34 },
	{ 4800000,	19,	29 },
	{ 5644800,	16,	24 },
	{ 6000000,	16,	24 },
	{ 6144000,	16,	24 },
};

static const unsigned char cs35l41_bst_k1_table[4][5] = {
	{0x24, 0x32, 0x32, 0x4F, 0x57},
	{0x24, 0x32, 0x32, 0x4F, 0x57},
	{0x40, 0x32, 0x32, 0x4F, 0x57},
	{0x40, 0x32, 0x32, 0x4F, 0x57}
};

static const unsigned char cs35l41_bst_k2_table[4][5] = {
	{0x24, 0x49, 0x66, 0xA3, 0xEA},
	{0x24, 0x49, 0x66, 0xA3, 0xEA},
	{0x48, 0x49, 0x66, 0xA3, 0xEA},
	{0x48, 0x49, 0x66, 0xA3, 0xEA}
};

static const unsigned char cs35l41_bst_slope_table[4] = {
					0x75, 0x6B, 0x3B, 0x28};

static int cs35l41_enter_hibernate(struct cs35l41_private *cs35l41);
static int cs35l41_exit_hibernate(struct cs35l41_private *cs35l41);
static int cs35l41_restore(struct cs35l41_private *cs35l41);
static int cs35l41_component_set_sysclk(struct snd_soc_component *component,
				int clk_id, int source, unsigned int freq,
				int dir);

static int cs35l41_get_fs_mon_config_index(int freq)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cs35l41_fs_mon); i++) {
		if (cs35l41_fs_mon[i].freq == freq)
			return i;
	}

	return -EINVAL;
}

static int cs35l41_dsp_power_ev(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);

	dev_info(cs35l41->dev, "%s: event = 0x%x\n", __func__, event);
	
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (cs35l41->halo_booted == false)
			wm_adsp_early_event(w, kcontrol, event);
		else
			cs35l41->dsp.booted = true;

		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (cs35l41->halo_booted == false) {
			cancel_delayed_work(&cs35l41->hb_work);
			mutex_lock(&cs35l41->hb_lock);
			cs35l41_exit_hibernate(cs35l41);
			mutex_unlock(&cs35l41->hb_lock);

			if (cs35l41->amp_hibernate !=
						CS35L41_HIBERNATE_INCOMPATIBLE)
				cs35l41->amp_hibernate =
						CS35L41_HIBERNATE_NOT_LOADED;

			wm_adsp_early_event(w, kcontrol, event);
			wm_adsp_event(w, kcontrol, event);
		}

		break;
	default:
		break;
	}

	return 0;
}

static int cs35l41_set_csplmboxcmd(struct cs35l41_private *cs35l41,
				   enum cs35l41_cspl_mboxcmd cmd);

static int cs35l41_dsp_load_ev(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);
	enum cs35l41_cspl_mboxcmd mboxcmd = CSPL_MBOX_CMD_NONE;
	enum cs35l41_cspl_mboxstate fw_status = CSPL_MBOX_STS_RUNNING;
	int ret = 0;

	dev_dbg(cs35l41->dev, "%s: event = 0x%x\n", __func__, event);
	
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (cs35l41->halo_booted == false) {
			wm_adsp_event(w, kcontrol, event);
			cs35l41->halo_booted = true;
		}

		if (cs35l41->dsp.running) {
			regmap_read(cs35l41->regmap, CS35L41_DSP_MBOX_2,
				    (unsigned int *)&fw_status);
			switch (fw_status) {
			case CSPL_MBOX_STS_RDY_FOR_REINIT:
				mboxcmd = CSPL_MBOX_CMD_REINIT;
				break;
			case CSPL_MBOX_STS_PAUSED:
				mboxcmd = CSPL_MBOX_CMD_RESUME;
				break;
			case CSPL_MBOX_STS_RUNNING:
				/*
				 * First time playing audio
				 * means fw_status is running
				 */
				mboxcmd = CSPL_MBOX_CMD_RESUME;
				break;
			default:
				dev_err(cs35l41->dev,
					"Firmware status is invalid(%u)\n",
					fw_status);
				break;
			}
			ret = cs35l41_set_csplmboxcmd(cs35l41, mboxcmd);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/*
		 * Workaround for PB 5.41.6 FW power consumption issue.
		 * FW sets bit FILT_GLOBAL_OVR, so it should be cleared
		 * after FW boot up
		 */
		regmap_update_bits(cs35l41->regmap, CS35L41_CTRL_OVRRIDE,
				CS35L41_FILT_GLOBAL_OVR_MASK, 0);
		break;
	default:
		break;
	}

	return ret;
}

static int cs35l41_halo_booted_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);

	dev_info(cs35l41->dev, "%s: get=%d\n", __func__,
		cs35l41->halo_booted);
	
	ucontrol->value.integer.value[0] = cs35l41->halo_booted;

	return 0;
}

static int cs35l41_halo_booted_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);

	dev_info(cs35l41->dev, "%s: put=%ld\n", __func__,
		ucontrol->value.integer.value[0]);

	cs35l41->halo_booted = ucontrol->value.integer.value[0];

	return 0;
}

#include "cs35l41-cal.c"
static int cs35l41_force_int_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = cs35l41->force_int;

	return 0;
}

static int cs35l41_force_int_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);
	bool force_int_changed = (cs35l41->force_int !=
		(bool)ucontrol->value.integer.value[0]);

	mutex_lock(&cs35l41->force_int_lock);

	cs35l41->force_int = ucontrol->value.integer.value[0];

	if (force_int_changed) {
		if (cs35l41->force_int) {
			disable_irq(cs35l41->irq);
			regmap_write(cs35l41->regmap, CS35L41_IRQ1_MASK1,
					CS35L41_INT1_MASK_FORCE);
			regmap_write(cs35l41->regmap, CS35L41_IRQ1_FRC1, 1);
		} else {
			regmap_write(cs35l41->regmap, CS35L41_IRQ1_FRC1, 0);
			regmap_write(cs35l41->regmap, CS35L41_IRQ1_MASK1,
					CS35L41_INT1_MASK_DEFAULT);
			regmap_write(cs35l41->regmap, CS35L41_IRQ1_STATUS1, 1);
			enable_irq(cs35l41->irq);
		}
	}

	mutex_unlock(&cs35l41->force_int_lock);

	return 0;
}

static int cs35l41_amp_reset_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int cs35l41_amp_reset_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(component);
	int ret, retries, timeout = 100;
	u32 status;

	if (!ucontrol->value.integer.value[0])
		return 0;

	dev_info(cs35l41->dev, "AMP reset requested\n");

	if (!cs35l41->reset_gpio) {
		dev_info(cs35l41->dev, "Reset GPIO is not configured\n");
		return 0;
	}

	disable_irq(cs35l41->irq);
	cs35l41->halo_booted = false;

	/* invalidate all cached values which have now been reset */
	regcache_cache_only(cs35l41->regmap, true);
	regcache_mark_dirty(cs35l41->regmap);

	/* Toggle reset pin */
	gpiod_set_value_cansleep(cs35l41->reset_gpio, 0);
	/* satisfy minimum reset pulse width spec */
	usleep_range(2000, 2100);
	gpiod_set_value_cansleep(cs35l41->reset_gpio, 1);
	usleep_range(2000, 2100);

	regcache_cache_only(cs35l41->regmap, false);

	do {
		if (timeout == 0) {
			dev_err(cs35l41->dev,
				"Timeout waiting for OTP_BOOT_DONE\n");
			return -EBUSY;
		}
		usleep_range(1000, 1100);
		regmap_read(cs35l41->regmap, CS35L41_IRQ1_STATUS4, &status);
		timeout--;
	} while (!(status & CS35L41_OTP_BOOT_DONE));

	/* Sync regmap */
	regcache_sync(cs35l41->regmap);

	/* Reset DSP */
	regmap_update_bits(cs35l41->regmap, CS35L41_DSP_CLK_CTRL,
			0x3, 0x2);
	regmap_update_bits(cs35l41->regmap,
			CS35L41_DSP1_CCM_CORE_CTRL,
			CS35L41_HALO_CORE_RESET, CS35L41_HALO_CORE_RESET);
	regmap_update_bits(cs35l41->regmap, CS35L41_DSP_CLK_CTRL,
			0x3, 0x3);

	/* Restore OTP and cached values */
	retries = 5;
	do {
		dev_info(cs35l41->dev, "cs35l41_restore attempt %d\n",
		 6 - retries);
		ret = cs35l41_restore(cs35l41);
		usleep_range(4000, 5000);
	} while (ret < 0 && --retries > 0);

	enable_irq(cs35l41->irq);

	if (retries < 0) {
		dev_err(cs35l41->dev, "Failed to reset AMP\n");
		return 0;
	}

	dev_dbg(cs35l41->dev, "cs35l41 restored in %d attempts\n",
			6 - retries);

	if (cs35l41->dsp.preloaded) {
		dev_info(cs35l41->dev, "Reload DSP\n");
		snd_soc_component_disable_pin(component, "DSP1 Preload");
		snd_soc_dapm_sync(dapm);
		usleep_range(4000, 5000);
		snd_soc_component_force_enable_pin(component, "DSP1 Preload");
		snd_soc_dapm_sync(dapm);
		flush_work(&cs35l41->dsp.boot_work);
	}
	return 0;
}

static int cs35l41_ccm_reset_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int cs35l41_ccm_reset_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);
	unsigned int val = 0;
	int ret = 0;

	val = ucontrol->value.integer.value[0];

	if (val) {
		ret = regmap_update_bits(cs35l41->regmap, CS35L41_DSP_CLK_CTRL,
			0x3, 0x2);
		ret = regmap_update_bits(cs35l41->regmap,
			CS35L41_DSP1_CCM_CORE_CTRL,
			CS35L41_HALO_CORE_RESET, CS35L41_HALO_CORE_RESET);
		ret = regmap_update_bits(cs35l41->regmap, CS35L41_DSP_CLK_CTRL,
			0x3, 0x3);
	}

	return 0;
}

static int cs35l41_hibernate_force_wake_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = cs35l41->hibernate_force_wake;

	return 0;
}

static int cs35l41_hibernate_force_wake_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);
	bool valid_transition = cs35l41->hibernate_force_wake !=
				ucontrol->value.integer.value[0];
	unsigned int amp_active;

	regmap_read(cs35l41->regmap, CS35L41_PWR_CTRL1, &amp_active);

	if (cs35l41->amp_hibernate == CS35L41_HIBERNATE_AWAKE ||
		(cs35l41->amp_hibernate == CS35L41_HIBERNATE_NOT_LOADED &&
		 cs35l41->dsp.running)) {
		cs35l41->hibernate_force_wake =
					ucontrol->value.integer.value[0];
		if (!cs35l41->hibernate_force_wake && valid_transition &&
			!(amp_active & CS35L41_GLOBAL_EN_MASK)) {
			/* return to standby */
			queue_delayed_work(cs35l41->wq, &cs35l41->hb_work,
					msecs_to_jiffies(100));
		}
	} else if (cs35l41->amp_hibernate == CS35L41_HIBERNATE_STANDBY) {
		cs35l41->hibernate_force_wake =
					ucontrol->value.integer.value[0];
		if (cs35l41->hibernate_force_wake && valid_transition) {
			/* wake from standby */
			cancel_delayed_work(&cs35l41->hb_work);
			mutex_lock(&cs35l41->hb_lock);
			cs35l41_exit_hibernate(cs35l41);
			mutex_unlock(&cs35l41->hb_lock);
		}
	}

	return 0;
}

static const char *cs35l41_fast_switch_text[] = {
	"fast_switch1.txt",
	"fast_switch2.txt",
	"fast_switch3.txt",
	"fast_switch4.txt",
	"fast_switch5.txt",
};

static int cs35l41_fast_switch_en_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = cs35l41->fast_switch_en;

	return 0;
}

static int cs35l41_do_fast_switch(struct cs35l41_private *cs35l41)
{
	char val_str[CS35L41_BUFSIZE];
	const char *fw_name = NULL;
	const struct firmware *fw = NULL;
	int ret;
	unsigned int i, j, k;
	s32 data_ctl_len, val;
	__be32 *data_ctl_buf, cmd_ctl, st_ctl;
	bool fw_running	= false;

	data_ctl_buf = NULL;

	fw_name	= cs35l41->fast_switch_names[cs35l41->fast_switch_file_idx];
	dev_dbg(cs35l41->dev, "fw_name:%s\n", fw_name);
	ret = request_firmware(&fw, fw_name, cs35l41->dev);
	if (ret < 0) {
		dev_err(cs35l41->dev, "Failed to request firmware:%s\n",
			fw_name);
		return -EIO;
	}

	/* Parse number of data words in file */
	for (i = 0, j = 0; (char)fw->data[i] != ','; i++) {
		if ((char)fw->data[i] == ' ') {
			/* Skip white space */
		} else {
			/* fw->data[i] must be numerical digit */
			if (j < CS35L41_BUFSIZE - 1) {
				val_str[j] = fw->data[i];
				j++;
			} else {
				dev_err(cs35l41->dev, "Invalid input\n");
				ret = -EINVAL;
				goto exit;
			}
		}
	}
	/* points to beginning of next number */
	i++;
	val_str[j] = '\0';
	ret = kstrtos32(val_str, 10, &data_ctl_len);
	if (ret < 0) {
		dev_err(cs35l41->dev, "kstrtos32 failed (%d) val_str:%s\n",
			ret, val_str);
		goto exit;
	}

	dev_dbg(cs35l41->dev, "data_ctl_len:%u\n", data_ctl_len);

	data_ctl_buf = kcalloc(1, data_ctl_len * sizeof(s32), GFP_KERNEL);
	if (!data_ctl_buf) {
		ret = -ENOMEM;
		goto exit;
	}

	data_ctl_buf[0]	= cpu_to_be32(data_ctl_len);

	/* i continues from end of previous loop */
	for (j = 0, k = 1; i <= fw->size; i++) {
		if (i == fw->size || (char)fw->data[i] == ',') {
			/*
			 * Reached end of parameter
			 * delimited either by ',' or end of file
			 * Parse number and write parameter
			 */
			val_str[j] = '\0';
			ret = kstrtos32(val_str, 10, &val);
			if (ret < 0) {
				dev_err(cs35l41->dev,
					"kstrtos32 failed (%d) val_str:%s\n",
					ret, val_str);
				goto exit;
			}
			data_ctl_buf[k] = cpu_to_be32(val);
			j = 0;
			k++;
		} else if ((char)fw->data[i] == ' ') {
			/* Skip white space */
		} else {
			/* fw->data[i] must be numerical digit */
			if (j < CS35L41_BUFSIZE - 1) {
				val_str[j] = fw->data[i];
				j++;
			} else {
				dev_err(cs35l41->dev, "Invalid input\n");
				ret = -EINVAL;
				goto exit;
			}
		}
	}

	wm_adsp_write_ctl(&cs35l41->dsp, "CSPL_UPDATE_PARAMS_CONFIG",
			  WMFW_ADSP2_YM, 0xcd,
			  data_ctl_buf, data_ctl_len * sizeof(s32));

	dev_dbg(cs35l41->dev,
		"Wrote %u reg for CSPL_UPDATE_PARAMS_CONFIG\n", data_ctl_len);

#ifdef DEBUG
	wm_adsp_read_ctl(&cs35l41->dsp, "CSPL_UPDATE_PARAMS_CONFIG",
			 WMFW_ADSP2_YM, 0xcd,
			 data_ctl_buf, data_ctl_len * sizeof(s32));
	dev_dbg(cs35l41->dev, "read CSPL_UPDATE_PARAMS_CONFIG:\n");
	for (i = 0; i < data_ctl_len; i++)
		dev_dbg(cs35l41->dev, "%u\n", be32_to_cpu(data_ctl_buf[i]));
#endif

	cmd_ctl = cpu_to_be32(CSPL_CMD_UPDATE_PARAM);
	wm_adsp_write_ctl(&cs35l41->dsp, "CSPL_COMMAND",
			  WMFW_ADSP2_XM, 0xcd, &cmd_ctl, sizeof(s32));

	/* Verify CSPL COMMAND */
	for (i = 0; i < 5; i++) {
		wm_adsp_read_ctl(&cs35l41->dsp, "CSPL_STATE",
				 WMFW_ADSP2_XM, 0xcd, &st_ctl,
				 sizeof(s32));
		if (be32_to_cpu(st_ctl) == CSPL_ST_RUNNING) {
			dev_dbg(cs35l41->dev,
				"CSPL STATE == RUNNING (%u attempt)\n", i);
			fw_running = true;
			break;
		}

		usleep_range(100, 200);
	}

	if (!fw_running) {
		dev_err(cs35l41->dev, "CSPL_STATE (%d) is not running\n",
			st_ctl);
		ret = -1;
		goto exit;
	}
exit:
	kfree(data_ctl_buf);
	release_firmware(fw);
	return ret;
}

static int cs35l41_fast_switch_en_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;

	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);

#if !defined(FAST_SWITCH_WORKAROUND)
	if (!cs35l41->fast_switch_en && ucontrol->value.integer.value[0])
		/*
		 * Rising on fast switch enable
		 * Perform fast use case switching
		 */
		ret = cs35l41_do_fast_switch(cs35l41);
#endif

	cs35l41->fast_switch_en = ucontrol->value.integer.value[0];

	return ret;
}

static int cs35l41_fast_switch_file_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);
	struct soc_enum *soc_enum;
	unsigned int i = ucontrol->value.enumerated.item[0];

	soc_enum = (struct soc_enum *)kcontrol->private_value;

	if (i >= soc_enum->items) {
		dev_err(cs35l41->dev, "Invalid mixer input (%u)\n", i);
		return -EINVAL;
	}

#if defined(FAST_SWITCH_WORKAROUND)
	if (!cs35l41->dsp.running) {
		dev_err(cs35l41->dev, "DSP not running\n");
		return 0;
	}

	if ((i != cs35l41->fast_switch_file_idx) && cs35l41->fast_switch_en) {
		int ret;

		cs35l41->fast_switch_file_idx = i;
		ret = cs35l41_do_fast_switch(cs35l41);
		dev_dbg(cs35l41->dev, "%s: fast switch %s\n", __func__,
			ret ? "fail" : "success");
	}
#endif

	return 0;
}

static int cs35l41_fast_switch_file_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = cs35l41->fast_switch_file_idx;

	return 0;
}

static const DECLARE_TLV_DB_RANGE(dig_vol_tlv,
		0, 0, TLV_DB_SCALE_ITEM(TLV_DB_GAIN_MUTE, 0, 1),
		1, CS35L41_MAX_PCM_VOL, TLV_DB_MINMAX_ITEM(-10200, 1200));
static DECLARE_TLV_DB_SCALE(amp_gain_tlv, 0, 1, 1);

static const struct snd_kcontrol_new dre_ctrl =
	SOC_DAPM_SINGLE("Switch", CS35L41_PWR_CTRL3, 20, 1, 0);

static const struct snd_kcontrol_new vbstmon_out_ctrl =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const char * const cs35l41_pcm_sftramp_text[] =  {
	"Off", ".5ms", "1ms", "2ms", "4ms", "8ms", "15ms", "30ms"};

static SOC_ENUM_SINGLE_DECL(pcm_sft_ramp,
			    CS35L41_AMP_DIG_VOL_CTRL, 0,
			    cs35l41_pcm_sftramp_text);

static int cs35l41_reload_tuning_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = cs35l41->reload_tuning;

	return 0;
}

static int cs35l41_reload_tuning_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);

	cs35l41->reload_tuning = ucontrol->value.integer.value[0];

	return 0;
}

static bool cs35l41_is_csplmboxsts_correct(enum cs35l41_cspl_mboxcmd cmd,
					   enum cs35l41_cspl_mboxstate sts)
{
	switch (cmd) {
	case CSPL_MBOX_CMD_NONE:
	case CSPL_MBOX_CMD_UNKNOWN_CMD:
		return true;
	case CSPL_MBOX_CMD_PAUSE:
		return (sts == CSPL_MBOX_STS_PAUSED);
	case CSPL_MBOX_CMD_RESUME:
		return (sts == CSPL_MBOX_STS_RUNNING);
	case CSPL_MBOX_CMD_REINIT:
		return (sts == CSPL_MBOX_STS_RUNNING);
	case CSPL_MBOX_CMD_STOP_PRE_REINIT:
		return (sts == CSPL_MBOX_STS_RDY_FOR_REINIT);
	default:
		return false;
	}
}

static int cs35l41_set_csplmboxcmd(struct cs35l41_private *cs35l41,
				   enum cs35l41_cspl_mboxcmd cmd)
{
	int ret = 0;
	unsigned int sts, i;
	bool ack = false;

	/* Reset DSP sticky bit */
	regmap_write(cs35l41->regmap, CS35L41_IRQ2_STATUS2,
		     1 << CS35L41_CSPL_MBOX_CMD_DRV_SHIFT);

	/* Reset AP sticky bit */
	regmap_write(cs35l41->regmap, CS35L41_IRQ1_STATUS2,
		     1 << CS35L41_CSPL_MBOX_CMD_FW_SHIFT);

	/*
	 * Set mailbox cmd
	 */
	/* Unmask DSP INT */
	regmap_update_bits(cs35l41->regmap, CS35L41_IRQ2_MASK2,
			   1 << CS35L41_CSPL_MBOX_CMD_DRV_SHIFT, 0);
	regmap_write(cs35l41->regmap, CS35L41_CSPL_MBOX_CMD_DRV, cmd);

	/* Poll for DSP ACK */
	for (i = 0; i < 5; i++) {
		usleep_range(1000, 1100);
		ret = regmap_read(cs35l41->regmap, CS35L41_IRQ1_STATUS2, &sts);
		if (ret < 0) {
			dev_err(cs35l41->dev, "regmap_read failed (%d)\n", ret);
			continue;
		}
		if (sts & (1 << CS35L41_CSPL_MBOX_CMD_FW_SHIFT)) {
			dev_dbg(cs35l41->dev,
				"%u: Received ACK in EINT for mbox cmd (%d)\n",
				i, cmd);
			regmap_write(cs35l41->regmap, CS35L41_IRQ1_STATUS2,
			     1 << CS35L41_CSPL_MBOX_CMD_FW_SHIFT);
			ack = true;
			break;
		}
	}

	if (!ack) {
		dev_err(cs35l41->dev,
			"Timeout waiting for DSP to set mbox cmd\n");
		ret = -ETIMEDOUT;
	}

	/* Mask DSP INT */
	regmap_update_bits(cs35l41->regmap, CS35L41_IRQ2_MASK2,
			   1 << CS35L41_CSPL_MBOX_CMD_DRV_SHIFT,
			   1 << CS35L41_CSPL_MBOX_CMD_DRV_SHIFT);

	if (regmap_read(cs35l41->regmap,
			CS35L41_CSPL_MBOX_STS, &sts) < 0) {
		dev_err(cs35l41->dev, "Failed to read %u\n",
			CS35L41_CSPL_MBOX_STS);
		ret = -EACCES;
	}

	if (!cs35l41_is_csplmboxsts_correct(cmd,
					    (enum cs35l41_cspl_mboxstate)sts)) {
		dev_err(cs35l41->dev,
			"Failed to set mailbox(cmd: %u, sts: %u)\n", cmd, sts);
		ret = -ENOMSG;
	}

	return ret;
}

static void cs35l41_abort_ramp(struct cs35l41_private *cs35l41)
{
	if (!work_busy(&cs35l41->vol_ctl.ramp_work))
		return;
	atomic_set(&cs35l41->vol_ctl.ramp_abort, 1);
	cancel_work_sync(&cs35l41->vol_ctl.ramp_work);
	flush_workqueue(cs35l41->vol_ctl.ramp_wq);
	atomic_set(&cs35l41->vol_ctl.ramp_abort, 0);
}

static int cs35l41_put_output_dev(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;
	struct soc_enum *soc_enum;
	unsigned int i = ucontrol->value.enumerated.item[0];

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	soc_enum = (struct soc_enum *)kcontrol->private_value;

	if (i >= soc_enum->items) {
		dev_err(component->dev,
			"Invalid mixer input (%u)\n", i);
		return -EINVAL;
	}

	if (atomic_read(&cs35l41->vol_ctl.playback) &&
	    cs35l41->vol_ctl.auto_ramp_timeout > 0 &&
	    cs35l41->vol_ctl.output_dev == CS35L41_OUTPUT_DEV_RCV &&
	    soc_enum->values[i] == CS35L41_OUTPUT_DEV_SPK) {
		/*
		 * While audio is playing,
		 * auto volume ramp is enabled,
		 * output device changes from RCV to SPK.
		 * In this case, perform volume ramp.
		 */
		cs35l41_abort_ramp(cs35l41);
		queue_work(cs35l41->vol_ctl.ramp_wq,
			   &cs35l41->vol_ctl.ramp_work);
	}

	cs35l41->vol_ctl.output_dev = soc_enum->values[i];

	return 0;
}

static int cs35l41_get_output_dev(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;
	int ret = 0;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = cs35l41->vol_ctl.output_dev;

	return ret;
}

static const char *virt_text[] = { "None", "Ref"};
static SOC_ENUM_SINGLE_DECL(virt_enum, SND_SOC_NOPM, 2, virt_text);
static const struct snd_kcontrol_new virt_mux =
	SOC_DAPM_ENUM("Virt Connect", virt_enum);
static const struct snd_kcontrol_new amp_enable_ctrl =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);
static const char * const cs35l41_pcm_source_texts[] = {"None", "ASP", "DSP"};
static const unsigned int cs35l41_pcm_source_values[] = {0x00, 0x08, 0x32};
static SOC_VALUE_ENUM_SINGLE_DECL(cs35l41_pcm_source_enum,
				CS35L41_DAC_PCM1_SRC,
				0, CS35L41_ASP_SOURCE_MASK,
				cs35l41_pcm_source_texts,
				cs35l41_pcm_source_values);

static const struct snd_kcontrol_new pcm_source_mux =
	SOC_DAPM_ENUM("PCM Source", cs35l41_pcm_source_enum);

static const char * const cs35l41_tx_input_texts[] = {"Zero", "ASPRX1",
							"ASPRX2", "VMON",
							"IMON", "VPMON",
							"VBSTMON",
							"DSPTX1", "DSPTX2"};
static const unsigned int cs35l41_tx_input_values[] = {0x00,
						CS35L41_INPUT_SRC_ASPRX1,
						CS35L41_INPUT_SRC_ASPRX2,
						CS35L41_INPUT_SRC_VMON,
						CS35L41_INPUT_SRC_IMON,
						CS35L41_INPUT_SRC_VPMON,
						CS35L41_INPUT_SRC_VBSTMON,
						CS35L41_INPUT_DSP_TX1,
						CS35L41_INPUT_DSP_TX2};

static SOC_VALUE_ENUM_SINGLE_DECL(cs35l41_asptx1_enum,
				CS35L41_ASP_TX1_SRC,
				0, CS35L41_ASP_SOURCE_MASK,
				cs35l41_tx_input_texts,
				cs35l41_tx_input_values);

static const struct snd_kcontrol_new asp_tx1_mux =
	SOC_DAPM_ENUM("ASPTX1 SRC", cs35l41_asptx1_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(cs35l41_asptx2_enum,
				CS35L41_ASP_TX2_SRC,
				0, CS35L41_ASP_SOURCE_MASK,
				cs35l41_tx_input_texts,
				cs35l41_tx_input_values);

static const struct snd_kcontrol_new asp_tx2_mux =
	SOC_DAPM_ENUM("ASPTX2 SRC", cs35l41_asptx2_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(cs35l41_asptx3_enum,
				CS35L41_ASP_TX3_SRC,
				0, CS35L41_ASP_SOURCE_MASK,
				cs35l41_tx_input_texts,
				cs35l41_tx_input_values);

static const struct snd_kcontrol_new asp_tx3_mux =
	SOC_DAPM_ENUM("ASPTX3 SRC", cs35l41_asptx3_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(cs35l41_asptx4_enum,
				CS35L41_ASP_TX4_SRC,
				0, CS35L41_ASP_SOURCE_MASK,
				cs35l41_tx_input_texts,
				cs35l41_tx_input_values);

static const struct snd_kcontrol_new asp_tx4_mux =
	SOC_DAPM_ENUM("ASPTX4 SRC", cs35l41_asptx4_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(cs35l41_dsprx1_enum,
				CS35L41_DSP1_RX1_SRC,
				0, CS35L41_ASP_SOURCE_MASK,
				cs35l41_tx_input_texts,
				cs35l41_tx_input_values);

static const struct snd_kcontrol_new dsp_rx1_mux =
	SOC_DAPM_ENUM("DSPRX1 SRC", cs35l41_dsprx1_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(cs35l41_dsprx2_enum,
				CS35L41_DSP1_RX2_SRC,
				0, CS35L41_ASP_SOURCE_MASK,
				cs35l41_tx_input_texts,
				cs35l41_tx_input_values);

static const struct snd_kcontrol_new dsp_rx2_mux =
	SOC_DAPM_ENUM("DSPRX2 SRC", cs35l41_dsprx2_enum);

static void cs35l41_set_vol(int vol, struct cs35l41_private *cs35l41)
{
	unsigned int val;
	int ret;

	mutex_lock(&cs35l41->vol_ctl.vol_mutex);

	if (vol < 0 || vol > CS35L41_MAX_PCM_VOL) {
		dev_err(cs35l41->dev,
			"Invalid PCM VOLUME %d\n", vol);
		goto exit;
	}

	if (vol < CS35L41_ZERO_PCM_VOL)
		/* PCM Volume is attenuation */
		val = (unsigned int)(vol + CS35L41_AMP_PCM_VOL_MUTE);
	else
		/* CS35L41_ZERO_PCM_VOL <= dig_vol <= CS35L41_MAX_PCM_VOL */
		val = (unsigned int)(vol - CS35L41_ZERO_PCM_VOL);
	ret = regmap_update_bits(cs35l41->regmap, CS35L41_AMP_DIG_VOL_CTRL,
				 CS35L41_AMP_PCM_VOL_MASK,
				 val << CS35L41_AMP_PCM_VOL_SHIFT);
	if (ret < 0)
		dev_err(cs35l41->dev,
			"Failed to set PCM VOLUME %d\n", ret);

exit:
	mutex_unlock(&cs35l41->vol_ctl.vol_mutex);
}

static int cs35l41_vol_ramp0(struct cs35l41_private *cs35l41,
			     long final_x, long init_y, long final_y)
{
	long curr_x = 0;
	long curr_y = init_y;
	long delta_x = final_x;
	long delta_y = final_y - init_y;
	long step_x, step_y;
	int ret = 0;

	if (final_x < 0 || final_y < 0 || init_y < 0) {
		ret = -EINVAL;
		goto exit;
	}

	if (final_y <= init_y) {
		dev_info(cs35l41->dev, "Vol ramp slope is not positive\n");
		cs35l41_set_vol((int)init_y, cs35l41);
		usleep_range(final_x, final_x + 1);
		cs35l41_set_vol((int)final_y, cs35l41);
		goto exit;
	}

	step_y = 1;	/* 1/8 dB, min IC supported step */
	step_x = delta_x / delta_y;	/* in micro-seconds */
	if (step_x == 0)
		/* Take care of case where delta_x < delta_y */
		step_x = 1;

	dev_dbg(cs35l41->dev, "vol ramp delta x:%ld delta y:%ld step x:%ld\n",
		delta_x, delta_y, step_x);
	while (1) {
		if (atomic_read(&cs35l41->vol_ctl.ramp_abort)) {
			ret = -EINTR;
			goto exit;
		}
		if (curr_x >= final_x) {
			/* Delay is complete */
			cs35l41_set_vol((int)final_y, cs35l41);
			break;
		}
		if (curr_y == final_y) {
			/* Volume ramp is complete */
			usleep_range(final_x - curr_x, final_x - curr_x + 1);
			break;
		}
		cs35l41_set_vol((int)curr_y, cs35l41);
		curr_y += step_y;
		curr_x += step_x;
		usleep_range(step_x, step_x + 1);
	}
exit:
	return ret;
}

static void cs35l41_vol_ramp(struct work_struct *wk)
{
	struct cs35l41_vol_ctl *vol_ctl;
	struct cs35l41_private *cs35l41;
	long final_x_knee, final_x_end;
	long init_y_knee, init_y_end, final_y_end;
	int ret = 0;

	vol_ctl = container_of(wk, struct cs35l41_vol_ctl, ramp_work);
	cs35l41 = container_of(vol_ctl, struct cs35l41_private, vol_ctl);
	atomic_set(&cs35l41->vol_ctl.vol_ramp, 1);
	/*
	 * vol_ramp must be true at this point,
	 * which guarantee ramp_init_att, ramp_knee_att,
	 * ramp_knee_time, ramp_end_time cannot be changed
	 */
	final_x_knee = (long)(cs35l41->vol_ctl.ramp_knee_time) * 1000;	/* us */
	final_x_end = (long)(cs35l41->vol_ctl.ramp_end_time) * 1000;
	/* 1/8 dB minimum step */
	init_y_knee = (long)(cs35l41->vol_ctl.dig_vol -
			     cs35l41->vol_ctl.ramp_init_att * 8);
	if (init_y_knee < 0)
		/* Hit floor */
		init_y_knee = 0;
	init_y_end = (long)(cs35l41->vol_ctl.dig_vol -
			    cs35l41->vol_ctl.ramp_knee_att * 8);
	if (init_y_end < 0)
		/* Hit floor */
		init_y_end = 0;
	final_y_end = (long)cs35l41->vol_ctl.dig_vol;
	if (final_y_end < 0)
		/* Hit floor */
		final_y_end = 0;
	ret = cs35l41_vol_ramp0(cs35l41, final_x_knee, init_y_knee, init_y_end);
	if (ret == -EINTR) {
		/* Abort ramp */
		cs35l41_set_vol(cs35l41->vol_ctl.dig_vol, cs35l41);
		goto exit;
	}
	ret = cs35l41_vol_ramp0(cs35l41, final_x_end, init_y_end, final_y_end);
	if (ret == -EINTR) {
		/* Abort ramp */
		cs35l41_set_vol(cs35l41->vol_ctl.dig_vol, cs35l41);
		goto exit;
	}
exit:
	atomic_set(&cs35l41->vol_ctl.vol_ramp, 0);
	/* Make manual ramp one-shot */
	atomic_set(&cs35l41->vol_ctl.manual_ramp, 0);

#ifdef CONFIG_SWITCH_PROTECTION
	cs35l41_spksw_safety_check(cs35l41);
#endif
}

static int cs35l41_get_vol(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = (long)cs35l41->vol_ctl.dig_vol;

	return 0;
}

static int cs35l41_put_vol(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;
	int ret = 0;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	if (ucontrol->value.integer.value[0] < 0 ||
	    ucontrol->value.integer.value[0] > CS35L41_MAX_PCM_VOL)
		return -EINVAL;

	if (atomic_read(&cs35l41->vol_ctl.vol_ramp) == 0) {
		cs35l41->vol_ctl.dig_vol =
			(int)ucontrol->value.integer.value[0];
		cs35l41_set_vol(cs35l41->vol_ctl.dig_vol, cs35l41);
	}

	return ret;
}

static int cs35l41_get_ramp_status(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
		(long)atomic_read(&cs35l41->vol_ctl.vol_ramp);

	return 0;
}

static int cs35l41_put_ramp_status(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	dev_info(cs35l41->dev,
		 "Volume ramp status cannot be set\n");
	return 0;
}

static int cs35l41_get_manual_ramp(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
		(long)atomic_read(&cs35l41->vol_ctl.manual_ramp);

	return 0;
}

static int cs35l41_put_manual_ramp(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	if (ucontrol->value.integer.value[0] < 0 ||
	    ucontrol->value.integer.value[0] > 1)
		return -EINVAL;

	if (atomic_read(&cs35l41->vol_ctl.manual_ramp) == 0 &&
	    ucontrol->value.integer.value[0] == 1) {
		/* Rising edge */
		if (atomic_read(&cs35l41->vol_ctl.playback)) {
			/* Stop existing ramp and start new ramp */
			cs35l41_abort_ramp(cs35l41);
			queue_work(cs35l41->vol_ctl.ramp_wq,
				   &cs35l41->vol_ctl.ramp_work);
		}
		/*
		 * In else case,
		 * let DAPM event handle ramp on playback start.
		 */
	} else if (atomic_read(&cs35l41->vol_ctl.manual_ramp) == 1 &&
		   ucontrol->value.integer.value[0] == 0) {
		/* Falling edge */
		if (atomic_read(&cs35l41->vol_ctl.playback))
			/* Stop existing ramp */
			cs35l41_abort_ramp(cs35l41);
	}
	atomic_set(&cs35l41->vol_ctl.manual_ramp,
		   ucontrol->value.integer.value[0]);

	return 0;
}

static int cs35l41_get_init_attenuation(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = (long)cs35l41->vol_ctl.ramp_init_att;
	return 0;
}

static int cs35l41_put_init_attenuation(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	if (ucontrol->value.integer.value[0] < 0 ||
	    ucontrol->value.integer.value[0] > CS35L41_MAX_VOL_ATT)
		return -EINVAL;

	if (atomic_read(&cs35l41->vol_ctl.vol_ramp) == 0)
		cs35l41->vol_ctl.ramp_init_att =
			(int)ucontrol->value.integer.value[0];
	return 0;
}

static int cs35l41_get_knee_attenuation(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = (long)cs35l41->vol_ctl.ramp_knee_att;
	return 0;
}

static int cs35l41_put_knee_attenuation(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	if (ucontrol->value.integer.value[0] < 0 ||
	    ucontrol->value.integer.value[0] > CS35L41_MAX_VOL_ATT)
		return -EINVAL;

	if (atomic_read(&cs35l41->vol_ctl.vol_ramp) == 0)
		cs35l41->vol_ctl.ramp_knee_att =
			(int)ucontrol->value.integer.value[0];
	return 0;
}

static int cs35l41_get_ramp_end_time(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = (long)cs35l41->vol_ctl.ramp_end_time;
	return 0;
}

static int cs35l41_put_ramp_end_time(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	if (ucontrol->value.integer.value[0] < 0 ||
	    ucontrol->value.integer.value[0] > CS35L41_MAX_AUTO_RAMP_TIMEOUT)
		return -EINVAL;

	if (atomic_read(&cs35l41->vol_ctl.vol_ramp) == 0)
		cs35l41->vol_ctl.ramp_end_time =
			(unsigned int)ucontrol->value.integer.value[0];
	return 0;
}

static int cs35l41_get_auto_ramp_timeout(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
		(long)cs35l41->vol_ctl.auto_ramp_timeout;
	return 0;
}

static int cs35l41_put_auto_ramp_timeout(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	if (ucontrol->value.integer.value[0] < 0 ||
	    ucontrol->value.integer.value[0] > CS35L41_MAX_AUTO_RAMP_TIMEOUT)
		return -EINVAL;

	cs35l41->vol_ctl.auto_ramp_timeout =
		(unsigned int)ucontrol->value.integer.value[0];

	return 0;
}

static int cs35l41_get_ramp_knee_time(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private	*cs35l41;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
		(long)cs35l41->vol_ctl.ramp_knee_time;
	return 0;
}

static int cs35l41_put_ramp_knee_time(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l41_private *cs35l41;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l41 = snd_soc_component_get_drvdata(component);

	if (ucontrol->value.integer.value[0] < 0 ||
	    ucontrol->value.integer.value[0] > CS35L41_MAX_AUTO_RAMP_TIMEOUT)
		return -EINVAL;

	if (atomic_read(&cs35l41->vol_ctl.vol_ramp) == 0)
		cs35l41->vol_ctl.ramp_knee_time =
			(unsigned int)ucontrol->value.integer.value[0];
	return 0;
}

static const char * const cs35l41_output_dev_text[] = {
	"Speaker",
	"Receiver",
};

/* Ensure SPK and RCV defined values match array index */
static const unsigned int cs35l41_output_dev_val[] = {
	CS35L41_OUTPUT_DEV_SPK,
	CS35L41_OUTPUT_DEV_RCV,
};

static SOC_VALUE_ENUM_SINGLE_DECL(cs35l41_output_dev, SND_SOC_NOPM, 0, 0,
				  cs35l41_output_dev_text,
				  cs35l41_output_dev_val);

static const struct snd_kcontrol_new cs35l41_aud_controls[] = {
	SOC_SINGLE_RANGE_EXT_TLV("Digital PCM Volume", SND_SOC_NOPM, 0, 0,
				 CS35L41_MAX_PCM_VOL, 0, cs35l41_get_vol,
				 cs35l41_put_vol, dig_vol_tlv),
	SOC_SINGLE_TLV("AMP PCM Gain", CS35L41_AMP_GAIN_CTRL, 5, 0x14, 0,
			amp_gain_tlv),
	SOC_SINGLE_RANGE("ASPTX1 Slot Position", CS35L41_SP_FRAME_TX_SLOT, 0,
			 0, 7, 0),
	SOC_SINGLE_RANGE("ASPTX2 Slot Position", CS35L41_SP_FRAME_TX_SLOT, 8,
			 0, 7, 0),
	SOC_SINGLE_RANGE("ASPTX3 Slot Position", CS35L41_SP_FRAME_TX_SLOT, 16,
			 0, 7, 0),
	SOC_SINGLE_RANGE("ASPTX4 Slot Position", CS35L41_SP_FRAME_TX_SLOT, 24,
			 0, 7, 0),
	SOC_SINGLE_RANGE("ASPRX1 Slot Position", CS35L41_SP_FRAME_RX_SLOT, 0,
			 0, 7, 0),
	SOC_SINGLE_RANGE("ASPRX2 Slot Position", CS35L41_SP_FRAME_RX_SLOT, 8,
			 0, 7, 0),
	SOC_ENUM("PCM Soft Ramp", pcm_sft_ramp),
	SOC_SINGLE_EXT("DSP Booted", SND_SOC_NOPM, 0, 1, 0,
			cs35l41_halo_booted_get, cs35l41_halo_booted_put),
	SOC_SINGLE_EXT("AMP Reset", SND_SOC_NOPM, 0, 1, 0,
			cs35l41_amp_reset_get, cs35l41_amp_reset_put),
	SOC_SINGLE_EXT("CCM Reset", CS35L41_DSP1_CCM_CORE_CTRL, 0, 1, 0,
			cs35l41_ccm_reset_get, cs35l41_ccm_reset_put),
	SOC_SINGLE_EXT("Force Interrupt", SND_SOC_NOPM, 0, 1, 0,
			cs35l41_force_int_get, cs35l41_force_int_put),
	SOC_SINGLE_EXT("Hibernate Force Wake", SND_SOC_NOPM, 0, 1, 0,
			cs35l41_hibernate_force_wake_get,
			cs35l41_hibernate_force_wake_put),
	SOC_SINGLE_EXT("Fast Use Case Switch Enable", SND_SOC_NOPM, 0, 1, 0,
		    cs35l41_fast_switch_en_get, cs35l41_fast_switch_en_put),
	SOC_SINGLE_EXT("Firmware Reload Tuning", SND_SOC_NOPM, 0, 1, 0,
			cs35l41_reload_tuning_get, cs35l41_reload_tuning_put),
	SOC_SINGLE("GLOBAL_EN from GPIO Control", CS35L41_PWR_CTRL1, 8, 1, 0),
	SOC_SINGLE("Boost Converter Enable", CS35L41_PWR_CTRL2, 4, 3, 0),
	SOC_SINGLE("Noise Gate", CS35L41_NG_CFG, 0, 0x3FFF, 0),
	SOC_SINGLE("Boost Class-H Tracking Enable",
					CS35L41_BSTCVRT_VCTRL2, 0, 1, 0),
	SOC_SINGLE("Boost Target Voltage", CS35L41_BSTCVRT_VCTRL1, 0, 0xAA, 0),
	WM_ADSP2_PRELOAD_SWITCH("DSP1", 1),
	WM_ADSP_FW_CONTROL("DSP1", 0),
	SOC_SINGLE_BOOL_EXT("Safety Volume Ramp Status", 0,
			    cs35l41_get_ramp_status, cs35l41_put_ramp_status),
	SOC_SINGLE_BOOL_EXT("Manual Ramp Control", 0,
			    cs35l41_get_manual_ramp, cs35l41_put_manual_ramp),
	SOC_SINGLE_EXT("Initial Ramp Volume Attenuation",
		       SND_SOC_NOPM, 0, CS35L41_MAX_VOL_ATT, 0,
		       cs35l41_get_init_attenuation,
		       cs35l41_put_init_attenuation),
	SOC_SINGLE_EXT("Knee Ramp Volume Attenuation",
		       SND_SOC_NOPM, 0, CS35L41_MAX_VOL_ATT, 0,
		       cs35l41_get_knee_attenuation,
		       cs35l41_put_knee_attenuation),
	SOC_SINGLE_EXT("Ramp Knee Time", SND_SOC_NOPM, 0,
		       CS35L41_MAX_AUTO_RAMP_TIMEOUT, 0,
		       cs35l41_get_ramp_knee_time, cs35l41_put_ramp_knee_time),
	SOC_SINGLE_EXT("Ramp End Time", SND_SOC_NOPM, 0,
		       CS35L41_MAX_AUTO_RAMP_TIMEOUT, 0,
		       cs35l41_get_ramp_end_time, cs35l41_put_ramp_end_time),
	SOC_SINGLE_EXT("Auto Ramp Safety Timeout", SND_SOC_NOPM, 0,
		       CS35L41_MAX_AUTO_RAMP_TIMEOUT, 0,
		       cs35l41_get_auto_ramp_timeout,
		       cs35l41_put_auto_ramp_timeout),
	SOC_VALUE_ENUM_EXT("Audio Output Device", cs35l41_output_dev,
			   cs35l41_get_output_dev, cs35l41_put_output_dev),
    /* Calibration */
	SOC_SINGLE_EXT("Calibration Resistance", SND_SOC_NOPM, 0, 0xFFFFFF, 0,
		       cs35l41_calr_get, cs35l41_calr_put),
	SOC_SINGLE_EXT("Ambient Temperature", SND_SOC_NOPM, 0, 0xFFFFFF, 0,
		       cs35l41_ambient_get, cs35l41_ambient_put),
	SOC_SINGLE_EXT("AMP Active Status", SND_SOC_NOPM, 0, 1, 0,
            cs35l41_amp_active_status_get, cs35l41_amp_active_status_put),
	SOC_SINGLE_EXT("Spksw Gpio Swtich", SND_SOC_NOPM, 0, 0x1, 0,
		       cs35l41_spksw_gpio_get, cs35l41_spksw_gpio_put),
	SND_SOC_BYTES_TLV("AMP WR Registers", REG_VALUE_SIZE,
			cs35l41_rw_registers_get, cs35l41_rw_registers_put),
};

static const struct cs35l41_otp_map_element_t *cs35l41_find_otp_map(u32 otp_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cs35l41_otp_map_map); i++) {
		if (cs35l41_otp_map_map[i].id == otp_id)
			return &cs35l41_otp_map_map[i];
	}

	return NULL;
}
static void cs35l41_debug_dump(struct cs35l41_private *cs35l41)
{
	int value;

	regmap_read(cs35l41->regmap, CS35L41_TEST_KEY_CTL, &value);
	dev_err(cs35l41->dev, "OTP:Read reg = %x, value = %x\n", CS35L41_IRQ1_MASK1, value);
	regmap_read(cs35l41->regmap, CS35L41_IRQ1_MASK1, &value);
	dev_err(cs35l41->dev, "OTP:Read reg = %x, value = %x\n", CS35L41_IRQ1_MASK1, value);
}

static int cs35l41_otp_unpack(void *data)
{
	struct cs35l41_private *cs35l41 = data;
	u32 *otp_mem = NULL;
	int i;
	int bit_offset, word_offset;
	unsigned int bit_sum = 8;
	u32 otp_val, otp_id_reg;
	const struct cs35l41_otp_map_element_t *otp_map_match = NULL;
	const struct cs35l41_otp_packed_element_t *otp_map = NULL;
	int ret;
	int otp_retry_cnt = 0;
	struct spi_device *spi = NULL;
	u32 orig_spi_freq = 0;

	otp_mem = kmalloc_array(32, sizeof(*otp_mem), GFP_KERNEL);
	if (!otp_mem)
		return -ENOMEM;

	ret = regmap_read(cs35l41->regmap, CS35L41_OTPID, &otp_id_reg);
	if (ret < 0) {
		dev_err(cs35l41->dev, "Read OTP ID failed\n");
		ret = -EINVAL;
		goto err_otp_unpack;
	}

	otp_map_match = cs35l41_find_otp_map(otp_id_reg);

	if (otp_map_match == NULL) {
		dev_err(cs35l41->dev, "OTP Map matching ID %d not found\n",
				otp_id_reg);
		ret = -EINVAL;
		goto err_otp_unpack;
	}

	if (cs35l41->bus_spi) {
		spi = to_spi_device(cs35l41->dev);
		orig_spi_freq = spi->max_speed_hz;
		spi->max_speed_hz = CS35L41_SPI_MAX_FREQ_OTP;
		spi_setup(spi);
	}

	ret = regmap_bulk_read(cs35l41->regmap, CS35L41_OTP_MEM0, otp_mem,
						CS35L41_OTP_SIZE_WORDS);
	if (ret < 0) {
		dev_err(cs35l41->dev, "Read OTP Mem failed\n");
		ret = -EINVAL;
		goto err_otp_unpack;
	}

	if (cs35l41->bus_spi) {
		spi->max_speed_hz = orig_spi_freq;
		spi_setup(spi);
	}

	otp_map = otp_map_match->map;

	bit_offset = otp_map_match->bit_offset;
	word_offset = otp_map_match->word_offset;

	dev_info(cs35l41->dev, "OTP: reg = %x, id = %d\n",
			otp_id_reg, otp_map_match->id);

	ret = regmap_write(cs35l41->regmap, CS35L41_TEST_KEY_CTL, 0x00000055);
	if (ret < 0) {
		dev_err(cs35l41->dev, "Write Unlock key failed 1/2\n");
		ret = -EINVAL;
		goto err_otp_unpack;
	}
	ret = regmap_write(cs35l41->regmap, CS35L41_TEST_KEY_CTL, 0x000000AA);
	if (ret < 0) {
		dev_err(cs35l41->dev, "Write Unlock key failed 2/2\n");
		ret = -EINVAL;
		goto err_otp_unpack;
	}

	for (i = 0; i < otp_map_match->num_elements; i++) {
		//dev_dbg(cs35l41->dev, "bitoffset= %d, word_offset=%d, bit_sum mod 32=%d\n",
		//		bit_offset, word_offset, bit_sum % 32);
		if (bit_offset + otp_map[i].size - 1 >= 32) {
			otp_val = (otp_mem[word_offset] &
					GENMASK(31, bit_offset)) >>
					bit_offset;
			otp_val |= (otp_mem[++word_offset] &
					GENMASK(bit_offset +
						otp_map[i].size - 33, 0)) <<
					(32 - bit_offset);
			bit_offset += otp_map[i].size - 32;
		} else {
			otp_val = (otp_mem[word_offset] &
				GENMASK(bit_offset + otp_map[i].size - 1,
					bit_offset)) >>	bit_offset;
			bit_offset += otp_map[i].size;
		}
		bit_sum += otp_map[i].size;

		if (bit_offset == 32) {
			bit_offset = 0;
			word_offset++;
		}

otp_retry:
		if (otp_map[i].reg != 0) {
			ret = regmap_update_bits(cs35l41->regmap,
						otp_map[i].reg,
						GENMASK(otp_map[i].shift +
							otp_map[i].size - 1,
						otp_map[i].shift),
						otp_val << otp_map[i].shift);
			if (ret < 0) {
				dev_info(cs35l41->dev,
						"OTP[%d]: bitoffset=%d, word_offset=%d, bit_sum mod 32=%d\n"
						"reg=0x%x, size=%d, shift=%d\n",
						i, bit_offset,word_offset, bit_sum % 32,
						otp_map[i].reg, otp_map[i].size,
						otp_map[i].shift);
				cs35l41_debug_dump(cs35l41);
				dev_err(cs35l41->dev, "Write OTP val failed\n");
				ret = -EINVAL;
				if(otp_retry_cnt++ < 5) {
					usleep_range(100000, 100100);
					goto otp_retry;
				}
			}
		}
		if(ret < 0 || otp_retry_cnt) {
			dev_err(cs35l41->dev, "OTP retry %d but failed!\n", otp_retry_cnt);
			goto err_otp_unpack;
		}
	}

	ret = regmap_write(cs35l41->regmap, CS35L41_TEST_KEY_CTL, 0x000000CC);
	if (ret < 0) {
		dev_err(cs35l41->dev, "Write Lock key failed 1/2\n");
		ret = -EINVAL;
		goto err_otp_unpack;
	}
	ret = regmap_write(cs35l41->regmap, CS35L41_TEST_KEY_CTL, 0x00000033);
	if (ret < 0) {
		dev_err(cs35l41->dev, "Write Lock key failed 2/2\n");
		ret = -EINVAL;
		goto err_otp_unpack;
	}
	ret = 0;

err_otp_unpack:
	kfree(otp_mem);
	return ret;
}

static irqreturn_t cs35l41_irq(int irq, void *data)
{
	struct cs35l41_private *cs35l41 = data;
	unsigned int status[4] = {0, 0, 0, 0};
	unsigned int masks[4] = {0, 0, 0, 0};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(status); i++) {
		regmap_read(cs35l41->regmap,
			    CS35L41_IRQ1_STATUS1 + (i * CS35L41_REGSTRIDE),
			    &status[i]);
		regmap_read(cs35l41->regmap,
			    CS35L41_IRQ1_MASK1 + (i * CS35L41_REGSTRIDE),
			    &masks[i]);
	}

	/* Check to see if unmasked bits are active */
	if (!(status[0] & ~masks[0]) && !(status[1] & ~masks[1]) &&
		!(status[2] & ~masks[2]) && !(status[3] & ~masks[3]))
		return IRQ_NONE;

	if (status[3] & CS35L41_OTP_BOOT_DONE) {
		regmap_update_bits(cs35l41->regmap, CS35L41_IRQ1_MASK4,
				CS35L41_OTP_BOOT_DONE, CS35L41_OTP_BOOT_DONE);
	}

	/*
	 * The following interrupts require a
	 * protection release cycle to get the
	 * speaker out of Safe-Mode.
	 */
	if (status[0] & CS35L41_AMP_SHORT_ERR) {
		dev_crit(cs35l41->dev, "Amp short error\n");
		if (cs35l41->amp_short == 0)
			cs35l41->amp_short = 1;
		regmap_write(cs35l41->regmap, CS35L41_IRQ1_STATUS1,
					CS35L41_AMP_SHORT_ERR);
		regmap_write(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN, 0);
		regmap_update_bits(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN,
					CS35L41_AMP_SHORT_ERR_RLS,
					CS35L41_AMP_SHORT_ERR_RLS);
		regmap_update_bits(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN,
					CS35L41_AMP_SHORT_ERR_RLS, 0);
	}

	if (status[0] & CS35L41_TEMP_WARN) {
		dev_crit(cs35l41->dev, "Over temperature warning\n");
		regmap_write(cs35l41->regmap, CS35L41_IRQ1_STATUS1,
					CS35L41_TEMP_WARN);
		regmap_write(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN, 0);
		regmap_update_bits(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN,
					CS35L41_TEMP_WARN_ERR_RLS,
					CS35L41_TEMP_WARN_ERR_RLS);
		regmap_update_bits(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN,
					CS35L41_TEMP_WARN_ERR_RLS, 0);
	}

	if (status[0] & CS35L41_TEMP_ERR) {
		dev_crit(cs35l41->dev, "Over temperature error\n");
		regmap_write(cs35l41->regmap, CS35L41_IRQ1_STATUS1,
					CS35L41_TEMP_ERR);
		regmap_write(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN, 0);
		regmap_update_bits(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN,
					CS35L41_TEMP_ERR_RLS,
					CS35L41_TEMP_ERR_RLS);
		regmap_update_bits(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN,
					CS35L41_TEMP_ERR_RLS, 0);
	}

	if (status[0] & CS35L41_BST_OVP_ERR) {
		dev_crit(cs35l41->dev, "VBST Over Voltage error\n");
		regmap_update_bits(cs35l41->regmap, CS35L41_PWR_CTRL2,
					CS35L41_BST_EN_MASK, 0);
		regmap_write(cs35l41->regmap, CS35L41_IRQ1_STATUS1,
					CS35L41_BST_OVP_ERR);
		regmap_write(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN, 0);
		regmap_update_bits(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN,
					CS35L41_BST_OVP_ERR_RLS,
					CS35L41_BST_OVP_ERR_RLS);
		regmap_update_bits(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN,
					CS35L41_BST_OVP_ERR_RLS, 0);
		regmap_update_bits(cs35l41->regmap, CS35L41_PWR_CTRL2,
					CS35L41_BST_EN_MASK,
					CS35L41_BST_EN_DEFAULT <<
					CS35L41_BST_EN_SHIFT);
	}

	if (status[0] & CS35L41_BST_DCM_UVP_ERR) {
		dev_crit(cs35l41->dev, "DCM VBST Under Voltage Error\n");
		regmap_update_bits(cs35l41->regmap, CS35L41_PWR_CTRL2,
					CS35L41_BST_EN_MASK, 0);
		regmap_write(cs35l41->regmap, CS35L41_IRQ1_STATUS1,
					CS35L41_BST_DCM_UVP_ERR);
		regmap_write(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN, 0);
		regmap_update_bits(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN,
					CS35L41_BST_UVP_ERR_RLS,
					CS35L41_BST_UVP_ERR_RLS);
		regmap_update_bits(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN,
					CS35L41_BST_UVP_ERR_RLS, 0);
		regmap_update_bits(cs35l41->regmap, CS35L41_PWR_CTRL2,
					CS35L41_BST_EN_MASK,
					CS35L41_BST_EN_DEFAULT <<
					CS35L41_BST_EN_SHIFT);
	}

	if (status[0] & CS35L41_BST_SHORT_ERR) {
		dev_crit(cs35l41->dev, "LBST error: powering off!\n");
		regmap_update_bits(cs35l41->regmap, CS35L41_PWR_CTRL2,
					CS35L41_BST_EN_MASK, 0);
		regmap_write(cs35l41->regmap, CS35L41_IRQ1_STATUS1,
					CS35L41_BST_SHORT_ERR);
		regmap_write(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN, 0);
		regmap_update_bits(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN,
					CS35L41_BST_SHORT_ERR_RLS,
					CS35L41_BST_SHORT_ERR_RLS);
		regmap_update_bits(cs35l41->regmap, CS35L41_PROTECT_REL_ERR_IGN,
					CS35L41_BST_SHORT_ERR_RLS, 0);
		regmap_update_bits(cs35l41->regmap, CS35L41_PWR_CTRL2,
					CS35L41_BST_EN_MASK,
					CS35L41_BST_EN_DEFAULT <<
					CS35L41_BST_EN_SHIFT);
	}

	return IRQ_HANDLED;
}

static const struct reg_sequence cs35l41_pup_patch[] = {
	{0x00000040, 0x00000055},
	{0x00000040, 0x000000AA},
	{0x00002084, 0x002F1AA0},
	{0x00000040, 0x000000CC},
	{0x00000040, 0x00000033},
};

static const struct reg_sequence cs35l41_pdn_patch[] = {
	{0x00000040, 0x00000055},
	{0x00000040, 0x000000AA},
	{0x00002084, 0x002F1AA3},
	{0x00000040, 0x000000CC},
	{0x00000040, 0x00000033},
};

static void cs35l41_hibernate_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct cs35l41_private *cs35l41 =
		container_of(dwork, struct cs35l41_private, hb_work);

	mutex_lock(&cs35l41->hb_lock);
	cs35l41_enter_hibernate(cs35l41);
	mutex_unlock(&cs35l41->hb_lock);
}

static int cs35l41_hibernate(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);
	int ret = 0;

	if (!cs35l41->dsp.running ||
	     cs35l41->amp_hibernate == CS35L41_HIBERNATE_INCOMPATIBLE ||
	     cs35l41->hibernate_force_wake)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		cancel_delayed_work(&cs35l41->hb_work);
		mutex_lock(&cs35l41->hb_lock);
		ret = cs35l41_exit_hibernate(cs35l41);
		mutex_unlock(&cs35l41->hb_lock);
		break;
	case SND_SOC_DAPM_POST_PMD:
		queue_delayed_work(cs35l41->wq, &cs35l41->hb_work,
					msecs_to_jiffies(2000));
		break;
	default:
		dev_err(cs35l41->dev, "Invalid event = 0x%x\n", event);
		ret = -EINVAL;
	}
	return ret;
}

static bool cs35l41_need_auto_vol_ramp(struct cs35l41_private *cs35l41)
{
	bool ramp = false;
	ktime_t curr_timestamp;
	s64 dev_timeout = (s64)cs35l41->vol_ctl.auto_ramp_timeout * 1000000;
	s64 elapsed_time;

	if (cs35l41->vol_ctl.prev_active_dev == CS35L41_OUTPUT_DEV_RCV &&
	    cs35l41->vol_ctl.output_dev == CS35L41_OUTPUT_DEV_SPK) {
		if (cs35l41->vol_ctl.auto_ramp_timeout == 0) {
			/* Never ramp */
		} else if (cs35l41->vol_ctl.auto_ramp_timeout ==
			   CS35L41_MAX_AUTO_RAMP_TIMEOUT) {
			/* Always ramp */
			ramp = true;
		} else {
			/*
			 * Guaranteed:
			 * 0 < auto_ramp_timeout < CS35L41_MAX_AUTO_RAMP_TIMEOUT
			 */
			curr_timestamp = ktime_get();
			elapsed_time = ktime_to_ns(curr_timestamp) -
				ktime_to_ns(cs35l41->vol_ctl.dev_timestamp);
			if (elapsed_time < dev_timeout)
				ramp = true;
			dev_dbg(cs35l41->dev,
				"elapsed_time:%lld dev_timeout:%lld\n",
				elapsed_time, dev_timeout);
		}
	}

	return ramp;
}

static int cs35l41_main_amp_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);
	enum cs35l41_cspl_mboxcmd mboxcmd = CSPL_MBOX_CMD_NONE;
	int ret = 0;
	int i;
	bool pdn;
	unsigned int val;

	dev_dbg(cs35l41->dev, "%s: event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		regmap_multi_reg_write_bypassed(cs35l41->regmap,
					cs35l41_pup_patch,
					ARRAY_SIZE(cs35l41_pup_patch));

		regmap_update_bits(cs35l41->regmap, CS35L41_PWR_CTRL1,
				CS35L41_GLOBAL_EN_MASK,
				1 << CS35L41_GLOBAL_EN_SHIFT);

		usleep_range(1000, 1100);

#ifdef CONFIG_SWITCH_PROTECTION
		cs35l41_spksw_safety_check(cs35l41);
#endif

		atomic_set(&cs35l41->vol_ctl.playback, 1);
		if (atomic_read(&cs35l41->vol_ctl.manual_ramp) ||
		    cs35l41_need_auto_vol_ramp(cs35l41))
			/* Enable volume ramp */
			queue_work(cs35l41->vol_ctl.ramp_wq,
				   &cs35l41->vol_ctl.ramp_work);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (cs35l41->dsp.running) {
			if (cs35l41->reload_tuning) {
				mboxcmd = CSPL_MBOX_CMD_STOP_PRE_REINIT;
				/*
				 * Reset reload_tuning, so driver does not
				 * continuously reload tuning file
				 */
				cs35l41->reload_tuning = false;
			} else {
				mboxcmd = CSPL_MBOX_CMD_PAUSE;
			}
			ret = cs35l41_set_csplmboxcmd(cs35l41, mboxcmd);
		}

		regmap_read(cs35l41->regmap, CS35L41_PWR_CTRL1, &val);
		if (val & CS35L41_GLOBAL_EN_MASK) {
			regmap_update_bits(cs35l41->regmap, CS35L41_PWR_CTRL1,
					CS35L41_GLOBAL_EN_MASK, 0);

			pdn = false;
			for (i = 0; i < 100; i++) {
				regmap_read(cs35l41->regmap,
					CS35L41_IRQ1_STATUS1,
					&val);
				if (val & CS35L41_PDN_DONE_MASK) {
					pdn = true;
					break;
				}
				usleep_range(1000, 1100);
			}

			if (!pdn)
				dev_warn(cs35l41->dev, "PDN failed\n");

			regmap_write(cs35l41->regmap, CS35L41_IRQ1_STATUS1,
					CS35L41_PDN_DONE_MASK);
		}

		regmap_multi_reg_write_bypassed(cs35l41->regmap,
					cs35l41_pdn_patch,
					ARRAY_SIZE(cs35l41_pdn_patch));
		atomic_set(&cs35l41->vol_ctl.playback, 0);
		cs35l41_abort_ramp(cs35l41);
		cs35l41->vol_ctl.prev_active_dev = cs35l41->vol_ctl.output_dev;
		if (cs35l41->vol_ctl.output_dev == CS35L41_OUTPUT_DEV_RCV &&
		    cs35l41->vol_ctl.auto_ramp_timeout > 0 &&
		    cs35l41->vol_ctl.auto_ramp_timeout <
		    CS35L41_MAX_AUTO_RAMP_TIMEOUT)
			/* Auto Receiver Timeout is used */
			cs35l41->vol_ctl.dev_timestamp = ktime_get();
		break;
	default:
		dev_err(cs35l41->dev, "Invalid event = 0x%x\n", event);
		ret = -EINVAL;
	}
	return ret;
}

static const struct snd_soc_dapm_widget cs35l41_dapm_widgets[] = {

	SND_SOC_DAPM_SPK("DSP1 Preload", NULL),
	SND_SOC_DAPM_SUPPLY_S("DSP1 Preloader", 100,
				SND_SOC_NOPM, 0, 0, cs35l41_dsp_power_ev,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUT_DRV_E("DSP1", SND_SOC_NOPM, 0, 0, NULL, 0,
				cs35l41_dsp_load_ev,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUTPUT("SPK"),

	SND_SOC_DAPM_AIF_IN("ASPRX1", NULL, 0, CS35L41_SP_ENABLES, 16, 0),
	SND_SOC_DAPM_AIF_IN("ASPRX2", NULL, 0, CS35L41_SP_ENABLES, 17, 0),
	SND_SOC_DAPM_AIF_OUT("ASPTX1", NULL, 0, CS35L41_SP_ENABLES, 0, 0),
	SND_SOC_DAPM_AIF_OUT("ASPTX2", NULL, 0, CS35L41_SP_ENABLES, 1, 0),
	SND_SOC_DAPM_AIF_OUT("ASPTX3", NULL, 0, CS35L41_SP_ENABLES, 2, 0),
	SND_SOC_DAPM_AIF_OUT("ASPTX4", NULL, 0, CS35L41_SP_ENABLES, 3, 0),

	SND_SOC_DAPM_ADC("VMON ADC", NULL, CS35L41_PWR_CTRL2, 12, 0),
	SND_SOC_DAPM_ADC("IMON ADC", NULL, CS35L41_PWR_CTRL2, 13, 0),
	SND_SOC_DAPM_ADC("VPMON ADC", NULL, CS35L41_PWR_CTRL2, 8, 0),
	SND_SOC_DAPM_ADC("VBSTMON ADC", NULL, CS35L41_PWR_CTRL2, 9, 0),
	SND_SOC_DAPM_ADC("TEMPMON ADC", NULL, CS35L41_PWR_CTRL2, 10, 0),
	SND_SOC_DAPM_ADC("CLASS H", NULL, CS35L41_PWR_CTRL3, 4, 0),

	SND_SOC_DAPM_OUT_DRV_E("Main AMP", CS35L41_PWR_CTRL2, 0, 0, NULL, 0,
				cs35l41_main_amp_event,
				SND_SOC_DAPM_POST_PMD |	SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY("Hibernate",  SND_SOC_NOPM, 0, 0,
			    cs35l41_hibernate,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_INPUT("VP"),
	SND_SOC_DAPM_INPUT("VBST"),
	SND_SOC_DAPM_INPUT("ISENSE"),
	SND_SOC_DAPM_INPUT("VSENSE"),
	SND_SOC_DAPM_INPUT("TEMP"),

	SND_SOC_DAPM_MUX("ASPTX Ref", SND_SOC_NOPM, 0, 0, &virt_mux),
	SND_SOC_DAPM_MUX("ASP TX1 Source", SND_SOC_NOPM, 0, 0, &asp_tx1_mux),
	SND_SOC_DAPM_MUX("ASP TX2 Source", SND_SOC_NOPM, 0, 0, &asp_tx2_mux),
	SND_SOC_DAPM_MUX("ASP TX3 Source", SND_SOC_NOPM, 0, 0, &asp_tx3_mux),
	SND_SOC_DAPM_MUX("ASP TX4 Source", SND_SOC_NOPM, 0, 0, &asp_tx4_mux),
	SND_SOC_DAPM_MUX("DSP RX1 Source", SND_SOC_NOPM, 0, 0, &dsp_rx1_mux),
	SND_SOC_DAPM_MUX("DSP RX2 Source", SND_SOC_NOPM, 0, 0, &dsp_rx2_mux),
	SND_SOC_DAPM_MUX("PCM Source", SND_SOC_NOPM, 0, 0, &pcm_source_mux),
	SND_SOC_DAPM_SWITCH("DRE", SND_SOC_NOPM, 0, 0, &dre_ctrl),
	SND_SOC_DAPM_SWITCH("VBSTMON Output", SND_SOC_NOPM, 0, 0,
						&vbstmon_out_ctrl),
	SND_SOC_DAPM_SWITCH("AMP Enable", SND_SOC_NOPM, 0, 1, &amp_enable_ctrl),
};

static const struct snd_soc_dapm_route cs35l41_audio_map[] = {

	{ "DSP1", NULL, "DSP1 Preloader" },
	{ "DSP1 Preload", NULL, "DSP1 Preloader" },

	{"DSP RX1 Source", "VMON", "VMON ADC"},
	{"DSP RX1 Source", "IMON", "IMON ADC"},
	{"DSP RX1 Source", "VPMON", "VPMON ADC"},
	{"DSP RX1 Source", "DSPTX1", "DSP1"},
	{"DSP RX1 Source", "DSPTX2", "DSP1"},
	{"DSP RX1 Source", "ASPRX1", "ASPRX1"},
	{"DSP RX1 Source", "ASPRX2", "ASPRX2"},
	{"DSP RX1 Source", "Zero", "ASPRX1"},
	{"DSP1", NULL, "DSP RX1 Source"},

	{"DSP RX2 Source", "VMON", "VMON ADC"},
	{"DSP RX2 Source", "IMON", "IMON ADC"},
	{"DSP RX2 Source", "VPMON", "VPMON ADC"},
	{"DSP RX2 Source", "DSPTX1", "DSP1"},
	{"DSP RX2 Source", "DSPTX2", "DSP1"},
	{"DSP RX2 Source", "ASPRX1", "ASPRX1"},
	{"DSP RX2 Source", "ASPRX2", "ASPRX2"},
	{"DSP RX2 Source", "Zero", "ASPRX1"},
	{"DSP1", NULL, "DSP RX2 Source"},

	{"ASP TX1 Source", "VMON", "VMON ADC"},
	{"ASP TX1 Source", "IMON", "IMON ADC"},
	{"ASP TX1 Source", "VPMON", "VPMON ADC"},
	{"ASP TX1 Source", "VBSTMON", "VBSTMON ADC"},
	{"ASP TX1 Source", "DSPTX1", "DSP1"},
	{"ASP TX1 Source", "DSPTX2", "DSP1"},
	{"ASP TX1 Source", "ASPRX1", "ASPRX1" },
	{"ASP TX1 Source", "ASPRX2", "ASPRX2" },
	{"ASP TX2 Source", "VMON", "VMON ADC"},
	{"ASP TX2 Source", "IMON", "IMON ADC"},
	{"ASP TX2 Source", "VPMON", "VPMON ADC"},
	{"ASP TX2 Source", "VBSTMON", "VBSTMON ADC"},
	{"ASP TX2 Source", "DSPTX1", "DSP1"},
	{"ASP TX2 Source", "DSPTX2", "DSP1"},
	{"ASP TX2 Source", "ASPRX1", "ASPRX1" },
	{"ASP TX2 Source", "ASPRX2", "ASPRX2" },
	{"ASP TX3 Source", "VMON", "VMON ADC"},
	{"ASP TX3 Source", "IMON", "IMON ADC"},
	{"ASP TX3 Source", "VPMON", "VPMON ADC"},
	{"ASP TX3 Source", "VBSTMON", "VBSTMON ADC"},
	{"ASP TX3 Source", "DSPTX1", "DSP1"},
	{"ASP TX3 Source", "DSPTX2", "DSP1"},
	{"ASP TX3 Source", "ASPRX1", "ASPRX1" },
	{"ASP TX3 Source", "ASPRX2", "ASPRX2" },
	{"ASP TX4 Source", "VMON", "VMON ADC"},
	{"ASP TX4 Source", "IMON", "IMON ADC"},
	{"ASP TX4 Source", "VPMON", "VPMON ADC"},
	{"ASP TX4 Source", "VBSTMON", "VBSTMON ADC"},
	{"ASP TX4 Source", "DSPTX1", "DSP1"},
	{"ASP TX4 Source", "DSPTX2", "DSP1"},
	{"ASP TX4 Source", "ASPRX1", "ASPRX1" },
	{"ASP TX4 Source", "ASPRX2", "ASPRX2" },
	{"ASPTX1", NULL, "ASP TX1 Source"},
	{"ASPTX2", NULL, "ASP TX2 Source"},
	{"ASPTX3", NULL, "ASP TX3 Source"},
	{"ASPTX4", NULL, "ASP TX4 Source"},
	{"AMP Capture", NULL, "ASPTX1"},
	{"AMP Capture", NULL, "ASPTX2"},
	{"AMP Capture", NULL, "ASPTX3"},
	{"AMP Capture", NULL, "ASPTX4"},

	{"VMON ADC", NULL, "ASPRX1"},
	{"IMON ADC", NULL, "ASPRX1"},
	{"VPMON ADC", NULL, "ASPRX1"},
	{"TEMPMON ADC", NULL, "ASPRX1"},
	{"VBSTMON ADC", NULL, "ASPRX1"},

	{"VBSTMON Output", "Switch", "VBST"},
	{"CLASS H", NULL, "VBSTMON Output"},
	{"VBSTMON ADC", NULL, "VBSTMON Output"},

	{"DSP1", NULL, "IMON ADC"},
	{"DSP1", NULL, "VMON ADC"},
	{"DSP1", NULL, "VBSTMON ADC"},
	{"DSP1", NULL, "VPMON ADC"},
	{"DSP1", NULL, "TEMPMON ADC"},

	{"AMP Enable", "Switch", "AMP Playback"},
	{"ASPRX1", NULL, "AMP Enable"},
	{"ASPRX2", NULL, "AMP Enable"},
	{"DRE", "Switch", "CLASS H"},
	{"ASPTX Ref", "Ref", "ASPTX1"},
	{"ASPTX Ref", "Ref", "ASPTX2"},
	{"Main AMP", NULL, "ASPTX Ref"},
	{"Main AMP", NULL, "CLASS H"},
	{"Main AMP", NULL, "DRE"},
	{"SPK", NULL, "Main AMP"},
	{"SPK", NULL, "Hibernate"},

	{"PCM Source", "ASP", "ASPRX1"},
	{"PCM Source", "DSP", "DSP1"},
	{"CLASS H", NULL, "PCM Source"},

};

static const struct wm_adsp_region cs35l41_dsp1_regions[] = {
	{ .type = WMFW_HALO_PM_PACKED,	.base = CS35L41_DSP1_PMEM_0 },
	{ .type = WMFW_HALO_XM_PACKED,	.base = CS35L41_DSP1_XMEM_PACK_0 },
	{ .type = WMFW_HALO_YM_PACKED,	.base = CS35L41_DSP1_YMEM_PACK_0 },
	{. type = WMFW_ADSP2_XM,	.base = CS35L41_DSP1_XMEM_UNPACK24_0},
	{. type = WMFW_ADSP2_YM,	.base = CS35L41_DSP1_YMEM_UNPACK24_0},
};

static int cs35l41_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct cs35l41_private *cs35l41 =
			snd_soc_component_get_drvdata(codec_dai->component);
	unsigned int asp_fmt, lrclk_fmt, sclk_fmt, clock_mode;

	dev_dbg(cs35l41->dev, "%s: fmt = 0x%x\n", __func__, fmt);
	
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		clock_mode = 1;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		clock_mode = 0;
		break;
	default:
		dev_warn(cs35l41->dev,
			"%s: Mixed master mode unsupported\n", __func__);
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		asp_fmt = 0;
		cs35l41->i2s_mode = false;
		break;
	case SND_SOC_DAIFMT_I2S:
		asp_fmt = 2;
		cs35l41->i2s_mode = true;
		break;
	default:
		dev_warn(cs35l41->dev,
			"%s: Invalid or unsupported DAI format\n", __func__);
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_IF:
		lrclk_fmt = 1;
		sclk_fmt = 0;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		lrclk_fmt = 0;
		sclk_fmt = 1;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		lrclk_fmt = 1;
		sclk_fmt = 1;
		break;
	case SND_SOC_DAIFMT_NB_NF:
		lrclk_fmt = 0;
		sclk_fmt = 0;
		break;
	default:
		dev_warn(cs35l41->dev,
			"%s: Invalid DAI clock INV\n", __func__);
		return -EINVAL;
	}

	cs35l41->reset_cache.clock_mode = clock_mode;
	cs35l41->reset_cache.asp_fmt = asp_fmt;
	cs35l41->reset_cache.lrclk_fmt = lrclk_fmt;
	cs35l41->reset_cache.sclk_fmt = sclk_fmt;
	/* Amp is in hibernation. Cached values will be applied at wakeup. */
	if (cs35l41->amp_hibernate == CS35L41_HIBERNATE_STANDBY)
		return 0;

	regmap_update_bits(cs35l41->regmap, CS35L41_SP_FORMAT,
					CS35L41_ASP_FMT_MASK,
					asp_fmt << CS35L41_ASP_FMT_SHIFT);

	regmap_update_bits(cs35l41->regmap, CS35L41_SP_FORMAT,
				CS35L41_SCLK_MSTR_MASK,
				clock_mode << CS35L41_SCLK_MSTR_SHIFT);
	regmap_update_bits(cs35l41->regmap, CS35L41_SP_FORMAT,
				CS35L41_LRCLK_MSTR_MASK,
				clock_mode << CS35L41_LRCLK_MSTR_SHIFT);

	cs35l41->lrclk_fmt = lrclk_fmt;
	cs35l41->sclk_fmt = sclk_fmt;

	regmap_update_bits(cs35l41->regmap, CS35L41_SP_FORMAT,
				CS35L41_LRCLK_INV_MASK,
				lrclk_fmt << CS35L41_LRCLK_INV_SHIFT);
	regmap_update_bits(cs35l41->regmap, CS35L41_SP_FORMAT,
				CS35L41_SCLK_INV_MASK,
				sclk_fmt << CS35L41_SCLK_INV_SHIFT);

	return 0;
}

struct cs35l41_global_fs_config {
	int rate;
	int fs_cfg;
};

static const struct cs35l41_global_fs_config cs35l41_fs_rates[] = {
	{ 12000,	0x01 },
	{ 24000,	0x02 },
	{ 48000,	0x03 },
	{ 96000,	0x04 },
	{ 192000,	0x05 },
	{ 11025,	0x09 },
	{ 22050,	0x0A },
	{ 44100,	0x0B },
	{ 88200,	0x0C },
	{ 176400,	0x0D },
	{ 8000,		0x11 },
	{ 16000,	0x12 },
	{ 32000,	0x13 },
};

static int cs35l41_pcm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct cs35l41_private *cs35l41 =
			snd_soc_component_get_drvdata(dai->component);
	int i;
	unsigned int rate = params_rate(params);
	u8 asp_width, asp_wl;

	for (i = 0; i < ARRAY_SIZE(cs35l41_fs_rates); i++) {
		if (rate == cs35l41_fs_rates[i].rate)
			break;
	}

	if (i >= ARRAY_SIZE(cs35l41_fs_rates)) {
		dev_err(cs35l41->dev, "%s: Unsupported rate: %u\n",
						__func__, rate);
		return -EINVAL;
	}

	asp_wl = params_width(params);
	asp_width = params_physical_width(params);

	dev_info_once(cs35l41->dev, "%s: rate=%u, asp_wl=%u, asp_width=%u\n",
		__func__, rate, asp_wl, asp_width);
	dev_dbg(cs35l41->dev, "%s: rate=%u, asp_wl=%u, asp_width=%u\n",
		__func__, rate, asp_wl, asp_width);

	cs35l41->reset_cache.asp_wl = asp_wl;
	cs35l41->reset_cache.asp_width = asp_width;
	if (i < ARRAY_SIZE(cs35l41_fs_rates))
		cs35l41->reset_cache.fs_cfg = cs35l41_fs_rates[i].fs_cfg;

	/* Amp is in hibernation. Cached values will be applied at wakeup. */
	if (cs35l41->amp_hibernate == CS35L41_HIBERNATE_STANDBY)
		return 0;

	if (i < ARRAY_SIZE(cs35l41_fs_rates))
		regmap_update_bits(cs35l41->regmap, CS35L41_GLOBAL_CLK_CTRL,
			CS35L41_GLOBAL_FS_MASK,
			cs35l41_fs_rates[i].fs_cfg << CS35L41_GLOBAL_FS_SHIFT);

	cs35l41_component_set_sysclk(dai->component, 0, 0, 2 * rate * asp_width, 0);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		regmap_update_bits(cs35l41->regmap, CS35L41_SP_FORMAT,
				CS35L41_ASP_WIDTH_RX_MASK,
				asp_width << CS35L41_ASP_WIDTH_RX_SHIFT);
		regmap_update_bits(cs35l41->regmap, CS35L41_SP_RX_WL,
				CS35L41_ASP_RX_WL_MASK,
				asp_wl << CS35L41_ASP_RX_WL_SHIFT);
		if (cs35l41->i2s_mode) {
			regmap_update_bits(cs35l41->regmap,
					CS35L41_SP_FRAME_RX_SLOT,
					CS35L41_ASP_RX1_SLOT_MASK,
					((cs35l41->pdata.right_channel) ? 1 : 0)
					 << CS35L41_ASP_RX1_SLOT_SHIFT);
			regmap_update_bits(cs35l41->regmap,
					CS35L41_SP_FRAME_RX_SLOT,
					CS35L41_ASP_RX2_SLOT_MASK,
					((cs35l41->pdata.right_channel) ? 0 : 1)
					 << CS35L41_ASP_RX2_SLOT_SHIFT);
		}
	//} else {/* Note: To keep RX and TX using same data width */
		regmap_update_bits(cs35l41->regmap, CS35L41_SP_FORMAT,
				CS35L41_ASP_WIDTH_TX_MASK,
				asp_width << CS35L41_ASP_WIDTH_TX_SHIFT);
		regmap_update_bits(cs35l41->regmap, CS35L41_SP_TX_WL,
				CS35L41_ASP_TX_WL_MASK,
				asp_wl << CS35L41_ASP_TX_WL_SHIFT);
	}

	return 0;
}

static int cs35l41_get_clk_config(int freq)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cs35l41_pll_sysclk); i++) {
		if (cs35l41_pll_sysclk[i].freq == freq)
			return cs35l41_pll_sysclk[i].clk_cfg;
	}

	return -EINVAL;
}

static const unsigned int cs35l41_src_rates[] = {
	8000, 12000, 11025, 16000, 22050, 24000, 32000,
	44100, 48000, 88200, 96000, 176400, 192000
};

static const struct snd_pcm_hw_constraint_list cs35l41_constraints = {
	.count = ARRAY_SIZE(cs35l41_src_rates),
	.list = cs35l41_src_rates,
};

static int cs35l41_pcm_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	cs35l41_set_dai_fmt(dai, SND_SOC_DAIFMT_CBS_CFS|SND_SOC_DAIFMT_I2S);
	if (substream->runtime)
		return snd_pcm_hw_constraint_list(substream->runtime, 0,
				SNDRV_PCM_HW_PARAM_RATE, &cs35l41_constraints);
	return 0;
}

static int cs35l41_component_set_sysclk(struct snd_soc_component *component,
				int clk_id, int source, unsigned int freq,
				int dir)
{
	struct cs35l41_private *cs35l41 =
				       snd_soc_component_get_drvdata(component);

	dev_info_once(cs35l41->dev, "%s:0x430 clk_id=%d, source=%d, freq=%u, dir=%d\n",
		__func__, clk_id, source, freq, dir);
	dev_dbg(cs35l41->dev, "%s:0x430 clk_id=%d, source=%d, freq=%u, dir=%d\n",
		__func__, clk_id, source, freq, dir);
	
	cs35l41->extclk_freq = freq;

	switch (clk_id) {
	case 0:
		cs35l41->clksrc = CS35L41_PLLSRC_SCLK;
		break;
	case 1:
		cs35l41->clksrc = CS35L41_PLLSRC_LRCLK;
		break;
	case 2:
		cs35l41->clksrc = CS35L41_PLLSRC_PDMCLK;
		break;
	case 3:
		cs35l41->clksrc = CS35L41_PLLSRC_SELF;
		break;
	case 4:
		cs35l41->clksrc = CS35L41_PLLSRC_MCLK;
		break;
	default:
		dev_err(cs35l41->dev, "Invalid CLK Config\n");
		return -EINVAL;
	}

	cs35l41->extclk_cfg = cs35l41_get_clk_config(freq);

	if (cs35l41->extclk_cfg < 0) {
		dev_err(cs35l41->dev, "Invalid CLK Config: %d, freq: %u\n",
			cs35l41->extclk_cfg, freq);
		return -EINVAL;
	}

	cs35l41->reset_cache.extclk_cfg = true;
	/* Amp is in hibernation. Clock config will be restored at wakeup. */
	if (cs35l41->amp_hibernate == CS35L41_HIBERNATE_STANDBY)
		return 0;

	regmap_update_bits(cs35l41->regmap, CS35L41_PLL_CLK_CTRL,
			CS35L41_PLL_OPENLOOP_MASK,
			1 << CS35L41_PLL_OPENLOOP_SHIFT);
	regmap_update_bits(cs35l41->regmap, CS35L41_PLL_CLK_CTRL,
			CS35L41_REFCLK_FREQ_MASK,
			cs35l41->extclk_cfg << CS35L41_REFCLK_FREQ_SHIFT);
	regmap_update_bits(cs35l41->regmap, CS35L41_PLL_CLK_CTRL,
			CS35L41_PLL_CLK_EN_MASK,
			0 << CS35L41_PLL_CLK_EN_SHIFT);
	regmap_update_bits(cs35l41->regmap, CS35L41_PLL_CLK_CTRL,
			CS35L41_PLL_CLK_SEL_MASK, cs35l41->clksrc);
	regmap_update_bits(cs35l41->regmap, CS35L41_PLL_CLK_CTRL,
			CS35L41_PLL_OPENLOOP_MASK,
			0 << CS35L41_PLL_OPENLOOP_SHIFT);
	regmap_update_bits(cs35l41->regmap, CS35L41_PLL_CLK_CTRL,
			CS35L41_PLL_CLK_EN_MASK,
			1 << CS35L41_PLL_CLK_EN_SHIFT);
	//regmap_write(cs35l41->regmap, CS35L41_PLL_CLK_CTRL, 0x430);

	return 0;
}

static int cs35l41_dai_set_sysclk(struct snd_soc_dai *dai,
					int clk_id, unsigned int freq, int dir)
{
	struct cs35l41_private *cs35l41 =
				  snd_soc_component_get_drvdata(dai->component);
	int fsIndex;
	unsigned int fs1_val;
	unsigned int fs2_val;
	unsigned int val;

	dev_dbg(cs35l41->dev, "%s: clk_id=%d, freq=%u, dir=%d\n",
		__func__, clk_id, freq, dir);

	/* Need the SCLK Frequency regardless of sysclk source */
	cs35l41->sclk = freq;

	dev_dbg(cs35l41->dev, "Set DAI sysclk %d\n", freq);
	if (cs35l41->sclk <= 6144000) {
		/* Use the lookup table */
		fsIndex = cs35l41_get_fs_mon_config_index(freq);
		if (fsIndex < 0) {
			dev_err(cs35l41->dev, "Invalid CLK Config freq: %u\n", freq);
			return -EINVAL;
		}

		fs1_val = cs35l41_fs_mon[fsIndex].fs1;
		fs2_val = cs35l41_fs_mon[fsIndex].fs2;
	} else {
		/* Use hard-coded values */
		fs1_val = 0x10;
		fs2_val = 0x24;
	}

	val = fs1_val;
	val |= (fs2_val << CS35L41_FS2_WINDOW_SHIFT) & CS35L41_FS2_WINDOW_MASK;
	regmap_write(cs35l41->regmap, CS35L41_TST_FS_MON0, val);

	return 0;
}

static int cs35l41_boost_config(struct cs35l41_private *cs35l41,
		int boost_ind, int boost_cap, int boost_ipk)
{
	int ret;
	unsigned char bst_lbst_val, bst_cbst_range, bst_ipk_scaled;
	struct regmap *regmap = cs35l41->regmap;
	struct device *dev = cs35l41->dev;

	switch (boost_ind) {
	case 1000:	/* 1.0 uH */
		bst_lbst_val = 0;
		break;
	case 1200:	/* 1.2 uH */
		bst_lbst_val = 1;
		break;
	case 1500:	/* 1.5 uH */
		bst_lbst_val = 2;
		break;
	case 2200:	/* 2.2 uH */
		bst_lbst_val = 3;
		break;
	default:
		dev_err(dev, "Invalid boost inductor value: %d nH\n",
				boost_ind);
		return -EINVAL;
	}

	switch (boost_cap) {
	case 0 ... 19:
		bst_cbst_range = 0;
		break;
	case 20 ... 50:
		bst_cbst_range = 1;
		break;
	case 51 ... 100:
		bst_cbst_range = 2;
		break;
	case 101 ... 200:
		bst_cbst_range = 3;
		break;
	default:	/* 201 uF and greater */
		bst_cbst_range = 4;
	}

	ret = regmap_update_bits(regmap, CS35L41_BSTCVRT_COEFF,
			CS35L41_BST_K1_MASK,
			cs35l41_bst_k1_table[bst_lbst_val][bst_cbst_range]
				<< CS35L41_BST_K1_SHIFT);
	if (ret) {
		dev_err(dev, "Failed to write boost K1 coefficient\n");
		return ret;
	}

	ret = regmap_update_bits(regmap, CS35L41_BSTCVRT_COEFF,
			CS35L41_BST_K2_MASK,
			cs35l41_bst_k2_table[bst_lbst_val][bst_cbst_range]
				<< CS35L41_BST_K2_SHIFT);
	if (ret) {
		dev_err(dev, "Failed to write boost K2 coefficient\n");
		return ret;
	}

	ret = regmap_update_bits(regmap, CS35L41_BSTCVRT_SLOPE_LBST,
			CS35L41_BST_SLOPE_MASK,
			cs35l41_bst_slope_table[bst_lbst_val]
				<< CS35L41_BST_SLOPE_SHIFT);
	if (ret) {
		dev_err(dev, "Failed to write boost slope coefficient\n");
		return ret;
	}

	ret = regmap_update_bits(regmap, CS35L41_BSTCVRT_SLOPE_LBST,
			CS35L41_BST_LBST_VAL_MASK,
			bst_lbst_val << CS35L41_BST_LBST_VAL_SHIFT);
	if (ret) {
		dev_err(dev, "Failed to write boost inductor value\n");
		return ret;
	}

	if ((boost_ipk < 1600) || (boost_ipk > 4500)) {
		dev_err(dev, "Invalid boost inductor peak current: %d mA\n",
				boost_ipk);
		return -EINVAL;
	}
	bst_ipk_scaled = ((boost_ipk - 1600) / 50) + 0x10;

	ret = regmap_update_bits(regmap, CS35L41_BSTCVRT_PEAK_CUR,
			CS35L41_BST_IPK_MASK,
			bst_ipk_scaled << CS35L41_BST_IPK_SHIFT);
	if (ret) {
		dev_err(dev, "Failed to write boost inductor peak current\n");
		return ret;
	}

	return 0;
}

static int cs35l41_set_pdata(struct cs35l41_private *cs35l41)
{
	struct cs35l41_classh_cfg *classh = &cs35l41->pdata.classh_config;
	int ret;

	/* Set Platform Data */
	/* Required */
	if (cs35l41->pdata.bst_ipk &&
			cs35l41->pdata.bst_ind && cs35l41->pdata.bst_cap) {
		ret = cs35l41_boost_config(cs35l41, cs35l41->pdata.bst_ind,
					cs35l41->pdata.bst_cap,
					cs35l41->pdata.bst_ipk);
		if (ret) {
			dev_err(cs35l41->dev, "Error in Boost DT config\n");
			return ret;
		}
	} else {
		dev_err(cs35l41->dev, "Incomplete Boost component DT config\n");
		return -EINVAL;
	}

	/* Optional */
	if (cs35l41->pdata.sclk_frc)
		regmap_update_bits(cs35l41->regmap, CS35L41_SP_FORMAT,
				CS35L41_SCLK_FRC_MASK,
				cs35l41->pdata.sclk_frc <<
				CS35L41_SCLK_FRC_SHIFT);

	if (cs35l41->pdata.lrclk_frc)
		regmap_update_bits(cs35l41->regmap, CS35L41_SP_FORMAT,
				CS35L41_LRCLK_FRC_MASK,
				cs35l41->pdata.lrclk_frc <<
				CS35L41_LRCLK_FRC_SHIFT);

	if (cs35l41->pdata.amp_gain_zc)
		regmap_update_bits(cs35l41->regmap, CS35L41_AMP_GAIN_CTRL,
				CS35L41_AMP_GAIN_ZC_MASK,
				cs35l41->pdata.amp_gain_zc <<
				CS35L41_AMP_GAIN_ZC_SHIFT);

	if (cs35l41->pdata.bst_vctrl)
		regmap_update_bits(cs35l41->regmap, CS35L41_BSTCVRT_VCTRL1,
				CS35L41_BST_CTL_MASK, cs35l41->pdata.bst_vctrl);

	if (cs35l41->pdata.temp_warn_thld)
		regmap_update_bits(cs35l41->regmap, CS35L41_DTEMP_WARN_THLD,
				CS35L41_TEMP_THLD_MASK,
				cs35l41->pdata.temp_warn_thld);

	if (cs35l41->pdata.dout_hiz <= CS35L41_ASP_DOUT_HIZ_MASK &&
	    cs35l41->pdata.dout_hiz >= 0)
		regmap_update_bits(cs35l41->regmap, CS35L41_SP_HIZ_CTRL,
				CS35L41_ASP_DOUT_HIZ_MASK,
				cs35l41->pdata.dout_hiz);

	if (cs35l41->pdata.invert_pcm)
		regmap_update_bits(cs35l41->regmap, CS35L41_AMP_DIG_VOL_CTRL,
				CS35L41_AMP_INV_PCM_MASK,
				cs35l41->pdata.invert_pcm <<
				CS35L41_AMP_INV_PCM_SHIFT);

	if (cs35l41->pdata.dsp_ng_enable) {
		regmap_update_bits(cs35l41->regmap,
				CS35L41_MIXER_NGATE_CH1_CFG,
				CS35L41_DSP_NG_ENABLE_MASK,
				CS35L41_DSP_NG_ENABLE_MASK);
		regmap_update_bits(cs35l41->regmap,
				CS35L41_MIXER_NGATE_CH2_CFG,
				CS35L41_DSP_NG_ENABLE_MASK,
				CS35L41_DSP_NG_ENABLE_MASK);

		if (cs35l41->pdata.dsp_ng_pcm_thld) {
			regmap_update_bits(cs35l41->regmap,
				CS35L41_MIXER_NGATE_CH1_CFG,
				CS35L41_DSP_NG_THLD_MASK,
				cs35l41->pdata.dsp_ng_pcm_thld);
			regmap_update_bits(cs35l41->regmap,
				CS35L41_MIXER_NGATE_CH2_CFG,
				CS35L41_DSP_NG_THLD_MASK,
				cs35l41->pdata.dsp_ng_pcm_thld);
		}

		if (cs35l41->pdata.dsp_ng_delay) {
			regmap_update_bits(cs35l41->regmap,
				CS35L41_MIXER_NGATE_CH1_CFG,
				CS35L41_DSP_NG_DELAY_MASK,
				cs35l41->pdata.dsp_ng_delay <<
				CS35L41_DSP_NG_DELAY_SHIFT);
			regmap_update_bits(cs35l41->regmap,
				CS35L41_MIXER_NGATE_CH2_CFG,
				CS35L41_DSP_NG_DELAY_MASK,
				cs35l41->pdata.dsp_ng_delay <<
				CS35L41_DSP_NG_DELAY_SHIFT);
		}
	}

	if (cs35l41->pdata.hw_ng_sel)
		regmap_update_bits(cs35l41->regmap,
				CS35L41_NG_CFG,
				CS35L41_HW_NG_SEL_MASK,
				cs35l41->pdata.hw_ng_sel <<
				CS35L41_HW_NG_SEL_SHIFT);

	if (cs35l41->pdata.hw_ng_thld)
		regmap_update_bits(cs35l41->regmap,
				CS35L41_NG_CFG,
				CS35L41_HW_NG_THLD_MASK,
				cs35l41->pdata.hw_ng_thld <<
				CS35L41_HW_NG_THLD_SHIFT);

	if (cs35l41->pdata.hw_ng_delay)
		regmap_update_bits(cs35l41->regmap,
				CS35L41_NG_CFG,
				CS35L41_HW_NG_DLY_MASK,
				cs35l41->pdata.hw_ng_delay <<
				CS35L41_HW_NG_DLY_SHIFT);

	if (classh->classh_algo_enable) {
		if (classh->classh_bst_override)
			regmap_update_bits(cs35l41->regmap,
					CS35L41_BSTCVRT_VCTRL2,
					CS35L41_BST_CTL_SEL_MASK,
					CS35L41_BST_CTL_SEL_REG);
		if (classh->classh_bst_max_limit)
			regmap_update_bits(cs35l41->regmap,
					CS35L41_BSTCVRT_VCTRL2,
					CS35L41_BST_LIM_MASK,
					classh->classh_bst_max_limit <<
					CS35L41_BST_LIM_SHIFT);
		if (classh->classh_mem_depth)
			regmap_update_bits(cs35l41->regmap,
					CS35L41_CLASSH_CFG,
					CS35L41_CH_MEM_DEPTH_MASK,
					classh->classh_mem_depth <<
					CS35L41_CH_MEM_DEPTH_SHIFT);
		if (classh->classh_headroom)
			regmap_update_bits(cs35l41->regmap,
					CS35L41_CLASSH_CFG,
					CS35L41_CH_HDRM_CTL_MASK,
					classh->classh_headroom <<
					CS35L41_CH_HDRM_CTL_SHIFT);
		if (classh->classh_release_rate)
			regmap_update_bits(cs35l41->regmap,
					CS35L41_CLASSH_CFG,
					CS35L41_CH_REL_RATE_MASK,
					classh->classh_release_rate <<
					CS35L41_CH_REL_RATE_SHIFT);
		if (classh->classh_wk_fet_delay)
			regmap_update_bits(cs35l41->regmap,
					CS35L41_WKFET_CFG,
					CS35L41_CH_WKFET_DLY_MASK,
					classh->classh_wk_fet_delay <<
					CS35L41_CH_WKFET_DLY_SHIFT);
		if (classh->classh_wk_fet_thld)
			regmap_update_bits(cs35l41->regmap,
					CS35L41_WKFET_CFG,
					CS35L41_CH_WKFET_THLD_MASK,
					classh->classh_wk_fet_thld <<
					CS35L41_CH_WKFET_THLD_SHIFT);
	}

	return 0;
}

static int cs35l41_component_probe(struct snd_soc_component *component)
{
	struct cs35l41_private *cs35l41 = snd_soc_component_get_drvdata(component);
	struct snd_kcontrol_new *kcontrol;
	int ret = 0;

	dev_info(cs35l41->dev, "cs35l41_component_probe E\n");
	component->regmap = cs35l41->regmap;

	cs35l41_set_pdata(cs35l41);
	wm_adsp2_component_probe(&cs35l41->dsp, component);

	cs35l41_ignore_suspend_widgets(component);

	/* Add run-time mixer control for fast use case switch */
	kcontrol = kzalloc(sizeof(*kcontrol), GFP_KERNEL);
	if (!kcontrol) {
		ret = -ENOMEM;
		goto exit;
	}

	kcontrol->name = "Fast Use Case Delta File";
	kcontrol->iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	kcontrol->info = snd_soc_info_enum_double;
	kcontrol->get = cs35l41_fast_switch_file_get;
	kcontrol->put = cs35l41_fast_switch_file_put;
	kcontrol->private_value =
			  (unsigned long)&cs35l41->fast_switch_enum;
	ret = snd_soc_add_component_controls(component, kcontrol, 1);
	if (ret < 0)
		dev_err(cs35l41->dev,
		       "snd_soc_add_codec_controls failed (%d)\n", ret);
	kfree(kcontrol);

	/* Move to the extended standby state */
	regmap_multi_reg_write_bypassed(cs35l41->regmap,
				cs35l41_pdn_patch,
				ARRAY_SIZE(cs35l41_pdn_patch));
exit:
	return ret;
}

static int cs35l41_irq_gpio_config(struct cs35l41_private *cs35l41)
{
	struct cs35l41_irq_cfg *irq_gpio_cfg1 = &cs35l41->pdata.irq_config1;
	struct cs35l41_irq_cfg *irq_gpio_cfg2 = &cs35l41->pdata.irq_config2;
	int irq_pol = IRQF_TRIGGER_NONE;

	if (irq_gpio_cfg1->is_present) {
		if (irq_gpio_cfg1->irq_pol_inv)
			regmap_update_bits(cs35l41->regmap,
						CS35L41_GPIO1_CTRL1,
						CS35L41_GPIO_POL_MASK,
						CS35L41_GPIO_POL_MASK);
		if (irq_gpio_cfg1->irq_out_en)
			regmap_update_bits(cs35l41->regmap,
						CS35L41_GPIO1_CTRL1,
						CS35L41_GPIO_DIR_MASK,
						0);
		if (irq_gpio_cfg1->irq_src_sel)
			regmap_update_bits(cs35l41->regmap,
						CS35L41_GPIO_PAD_CONTROL,
						CS35L41_GPIO1_CTRL_MASK,
						irq_gpio_cfg1->irq_src_sel <<
						CS35L41_GPIO1_CTRL_SHIFT);
	}

	if (irq_gpio_cfg2->is_present) {
		if (irq_gpio_cfg2->irq_pol_inv)
			regmap_update_bits(cs35l41->regmap,
						CS35L41_GPIO2_CTRL1,
						CS35L41_GPIO_POL_MASK,
						CS35L41_GPIO_POL_MASK);
		if (irq_gpio_cfg2->irq_out_en)
			regmap_update_bits(cs35l41->regmap,
						CS35L41_GPIO2_CTRL1,
						CS35L41_GPIO_DIR_MASK,
						0);
		if (irq_gpio_cfg2->irq_src_sel)
			regmap_update_bits(cs35l41->regmap,
						CS35L41_GPIO_PAD_CONTROL,
						CS35L41_GPIO2_CTRL_MASK,
						irq_gpio_cfg2->irq_src_sel <<
						CS35L41_GPIO2_CTRL_SHIFT);
	}

	if (irq_gpio_cfg2->irq_src_sel ==
			(CS35L41_GPIO_CTRL_ACTV_LO | CS35L41_VALID_PDATA))
		irq_pol = IRQF_TRIGGER_LOW;
	else if (irq_gpio_cfg2->irq_src_sel ==
			(CS35L41_GPIO_CTRL_ACTV_HI | CS35L41_VALID_PDATA))
		irq_pol = IRQF_TRIGGER_HIGH;

	return irq_pol;
}

static void cs35l41_component_remove(struct snd_soc_component *component)
{
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);

	wm_adsp2_component_remove(&cs35l41->dsp, component);
}

static const struct snd_soc_dai_ops cs35l41_ops = {
	.startup = cs35l41_pcm_startup,
	.set_fmt = cs35l41_set_dai_fmt,
	.hw_params = cs35l41_pcm_hw_params,
	.set_sysclk = cs35l41_dai_set_sysclk,
};

static struct snd_soc_dai_driver cs35l41_dai[] = {
	{
		.name = "cs35l41-pcm",
		.id = 0,
		.playback = {
			.stream_name = "AMP Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_KNOT,
			.formats = CS35L41_RX_FORMATS,
		},
		.capture = {
			.stream_name = "AMP Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_KNOT,
			.formats = CS35L41_TX_FORMATS,
		},
		.ops = &cs35l41_ops,
		.symmetric_rates = 1,
	},
};

static const struct snd_soc_component_driver soc_component_dev_cs35l41 = {
	.probe = cs35l41_component_probe,
	.remove = cs35l41_component_remove,

	.dapm_widgets = cs35l41_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(cs35l41_dapm_widgets),
	.dapm_routes = cs35l41_audio_map,
	.num_dapm_routes = ARRAY_SIZE(cs35l41_audio_map),

	.controls = cs35l41_aud_controls,
	.num_controls = ARRAY_SIZE(cs35l41_aud_controls),
	.set_sysclk = cs35l41_component_set_sysclk,
};



static int cs35l41_handle_of_data(struct device *dev,
				  struct cs35l41_platform_data *pdata,
				  struct cs35l41_private *cs35l41)
{
	struct device_node *np = dev->of_node;
	unsigned int val;
	int ret;
	size_t	num_fast_switch;
	struct device_node *sub_node;
	struct cs35l41_classh_cfg *classh_config = &pdata->classh_config;
	struct cs35l41_irq_cfg *irq_gpio1_config = &pdata->irq_config1;
	struct cs35l41_irq_cfg *irq_gpio2_config = &pdata->irq_config2;
	unsigned int i;

	if (!np)
		return 0;

	cs35l41->dt_name = devm_kstrdup_const(cs35l41->dev, np->name,
					      GFP_KERNEL);

	ret = of_property_count_strings(np, "cirrus,fast-switch");
	if (ret < 0) {
		/*
		 * Device tree does not provide file name.
		 * Use default value
		 */
		num_fast_switch = ARRAY_SIZE(cs35l41_fast_switch_text);
		cs35l41->fast_switch_enum.items	=
			ARRAY_SIZE(cs35l41_fast_switch_text);
		cs35l41->fast_switch_enum.texts	= cs35l41_fast_switch_text;
		cs35l41->fast_switch_names = cs35l41_fast_switch_text;
	} else {
		/* Device tree provides file name */
		num_fast_switch = (size_t)ret;
		dev_info(dev, "num_fast_switch:%zu\n", num_fast_switch);
		cs35l41->fast_switch_names =
			devm_kmalloc(dev, num_fast_switch * sizeof(char *),
				     GFP_KERNEL);
		if (!cs35l41->fast_switch_names)
			return -ENOMEM;
		of_property_read_string_array(np, "cirrus,fast-switch",
					      cs35l41->fast_switch_names,
					      num_fast_switch);
		for (i = 0; i < num_fast_switch; i++) {
			dev_info(dev, "%d:%s\n", i,
				 cs35l41->fast_switch_names[i]);
		}
		cs35l41->fast_switch_enum.items	= num_fast_switch;
		cs35l41->fast_switch_enum.texts	= cs35l41->fast_switch_names;
	}

	cs35l41->fast_switch_enum.reg = SND_SOC_NOPM;
	cs35l41->fast_switch_enum.shift_l = 0;
	cs35l41->fast_switch_enum.shift_r = 0;
	cs35l41->fast_switch_enum.mask =
		roundup_pow_of_two(num_fast_switch) - 1;

	pdata->right_channel = of_property_read_bool(np,
					"cirrus,right-channel-amp");
	pdata->sclk_frc = of_property_read_bool(np,
					"cirrus,sclk-force-output");
	pdata->lrclk_frc = of_property_read_bool(np,
					"cirrus,lrclk-force-output");
	pdata->amp_gain_zc = of_property_read_bool(np,
					"cirrus,amp-gain-zc");
	pdata->tuning_has_prefix = of_property_read_bool(np,
					"cirrus,tuning-has-prefix");
	pdata->invert_pcm = of_property_read_bool(np,
					"cirrus,invert-pcm");

	pdata->fwname_use_revid = of_property_read_bool(np,
					"cirrus,fwname-use-revid");

	if (of_property_read_u32(np, "cirrus,temp-warn_threshold", &val) >= 0)
		pdata->temp_warn_thld = val | CS35L41_VALID_PDATA;

	ret = of_property_read_u32(np, "cirrus,boost-ctl-millivolt", &val);
	if (ret >= 0) {
		if (val < 2550 || val > 11000) {
			dev_err(dev,
				"Invalid Boost Voltage %u mV\n", val);
			return -EINVAL;
		}
		pdata->bst_vctrl = ((val - 2550) / 100) + 1;
	}

	ret = of_property_read_u32(np, "cirrus,boost-peak-milliamp", &val);
	if (ret >= 0)
		pdata->bst_ipk = val;

	ret = of_property_read_u32(np, "cirrus,boost-ind-nanohenry", &val);
	if (ret >= 0)
		pdata->bst_ind = val;

	ret = of_property_read_u32(np, "cirrus,boost-cap-microfarad", &val);
	if (ret >= 0)
		pdata->bst_cap = val;

	ret = of_property_read_u32(np, "cirrus,asp-sdout-hiz", &val);
	if (ret >= 0)
		pdata->dout_hiz = val;
	else
		pdata->dout_hiz = -1;

	pdata->dsp_ng_enable = of_property_read_bool(np,
					"cirrus,dsp-noise-gate-enable");
	if (of_property_read_u32(np,
				"cirrus,dsp-noise-gate-threshold", &val) >= 0)
		pdata->dsp_ng_pcm_thld = val | CS35L41_VALID_PDATA;
	if (of_property_read_u32(np, "cirrus,dsp-noise-gate-delay", &val) >= 0)
		pdata->dsp_ng_delay = val | CS35L41_VALID_PDATA;

	if (of_property_read_u32(np, "cirrus,hw-noise-gate-select", &val) >= 0)
		pdata->hw_ng_sel = val | CS35L41_VALID_PDATA;
	if (of_property_read_u32(np,
				"cirrus,hw-noise-gate-threshold", &val) >= 0)
		pdata->hw_ng_thld = val | CS35L41_VALID_PDATA;
	if (of_property_read_u32(np, "cirrus,hw-noise-gate-delay", &val) >= 0)
		pdata->hw_ng_delay = val | CS35L41_VALID_PDATA;

	sub_node = of_get_child_by_name(np, "cirrus,classh-internal-algo");
	classh_config->classh_algo_enable = sub_node ? true : false;

	if (classh_config->classh_algo_enable) {
		classh_config->classh_bst_override =
			of_property_read_bool(sub_node,
				"cirrus,classh-bst-override");

		ret = of_property_read_u32(sub_node,
					   "cirrus,classh-bst-max-limit",
					   &val);
		if (ret >= 0) {
			val |= CS35L41_VALID_PDATA;
			classh_config->classh_bst_max_limit = val;
		}

		ret = of_property_read_u32(sub_node, "cirrus,classh-mem-depth",
					   &val);
		if (ret >= 0) {
			val |= CS35L41_VALID_PDATA;
			classh_config->classh_mem_depth = val;
		}

		ret = of_property_read_u32(sub_node,
					"cirrus,classh-release-rate", &val);
		if (ret >= 0)
			classh_config->classh_release_rate = val;

		ret = of_property_read_u32(sub_node, "cirrus,classh-headroom",
					   &val);
		if (ret >= 0) {
			val |= CS35L41_VALID_PDATA;
			classh_config->classh_headroom = val;
		}

		ret = of_property_read_u32(sub_node,
					"cirrus,classh-wk-fet-delay", &val);
		if (ret >= 0) {
			val |= CS35L41_VALID_PDATA;
			classh_config->classh_wk_fet_delay = val;
		}

		ret = of_property_read_u32(sub_node,
					"cirrus,classh-wk-fet-thld", &val);
		if (ret >= 0)
			classh_config->classh_wk_fet_thld = val;
	}
	of_node_put(sub_node);

	/* GPIO1 Pin Config */
	sub_node = of_get_child_by_name(np, "cirrus,gpio-config1");
	irq_gpio1_config->is_present = sub_node ? true : false;
	if (irq_gpio1_config->is_present) {
		irq_gpio1_config->irq_pol_inv = of_property_read_bool(sub_node,
						"cirrus,gpio-polarity-invert");
		irq_gpio1_config->irq_out_en = of_property_read_bool(sub_node,
						"cirrus,gpio-output-enable");
		ret = of_property_read_u32(sub_node, "cirrus,gpio-src-select",
					&val);
		if (ret >= 0) {
			val |= CS35L41_VALID_PDATA;
			irq_gpio1_config->irq_src_sel = val;
		}
	}
	of_node_put(sub_node);

	/* GPIO2 Pin Config */
	sub_node = of_get_child_by_name(np, "cirrus,gpio-config2");
	irq_gpio2_config->is_present = sub_node ? true : false;
	if (irq_gpio2_config->is_present) {
		irq_gpio2_config->irq_pol_inv = of_property_read_bool(sub_node,
						"cirrus,gpio-polarity-invert");
		irq_gpio2_config->irq_out_en = of_property_read_bool(sub_node,
						"cirrus,gpio-output-enable");
		ret = of_property_read_u32(sub_node, "cirrus,gpio-src-select",
					&val);
		if (ret >= 0) {
			val |= CS35L41_VALID_PDATA;
			irq_gpio2_config->irq_src_sel = val;
		}
	}
	of_node_put(sub_node);

	pdata->hibernate_enable = of_property_read_bool(np,
					"cirrus,hibernate-enable");

	return 0;
}

static const struct reg_sequence cs35l41_reva0_errata_patch[] = {
	{0x00000040,			0x00005555},
	{0x00000040,			0x0000AAAA},
	{0x00003854,			0x05180240},
	{CS35L41_VIMON_SPKMON_RESYNC,	0x00000000},
	{0x00004310,			0x00000000},
	{CS35L41_VPVBST_FS_SEL,		0x00000000},
	{CS35L41_OTP_TRIM_30,		0x9091A1C8},
	{0x00003014,			0x0200EE0E},
	{CS35L41_BSTCVRT_DCM_CTRL,	0x00000051},
	{0x00000054,			0x00000004},
	{CS35L41_IRQ1_DB3,		0x00000000},
	{CS35L41_IRQ2_DB3,		0x00000000},
	{CS35L41_DSP1_YM_ACCEL_PL0_PRI,	0x00000000},
	{CS35L41_DSP1_XM_ACCEL_PL0_PRI,	0x00000000},
	{0x00000040,			0x0000CCCC},
	{0x00000040,			0x00003333},
};

static const struct reg_sequence cs35l41_revb0_errata_patch[] = {
	{0x00000040,			0x00005555},
	{0x00000040,			0x0000AAAA},
	{CS35L41_VIMON_SPKMON_RESYNC,	0x00000000},
	{0x00004310,			0x00000000},
	{CS35L41_VPVBST_FS_SEL,		0x00000000},
	{CS35L41_BSTCVRT_DCM_CTRL,	0x00000051},
	{CS35L41_DSP1_YM_ACCEL_PL0_PRI,	0x00000000},
	{CS35L41_DSP1_XM_ACCEL_PL0_PRI,	0x00000000},
	{0x00000040,			0x0000CCCC},
	{0x00000040,			0x00003333},
};

static const struct reg_sequence cs35l41_revb2_errata_patch[] = {
	{0x00000040,			0x00005555},
	{0x00000040,			0x0000AAAA},
	{CS35L41_VIMON_SPKMON_RESYNC,	0x00000000},
	{0x00004310,			0x00000000},
	{CS35L41_VPVBST_FS_SEL,		0x00000000},
	{CS35L41_BSTCVRT_DCM_CTRL,	0x00000051},
	{CS35L41_DSP1_YM_ACCEL_PL0_PRI,	0x00000000},
	{CS35L41_DSP1_XM_ACCEL_PL0_PRI,	0x00000000},
	{0x00000040,			0x0000CCCC},
	{0x00000040,			0x00003333},
};

static const struct reg_sequence cs35l41_fs_errata_patch[] = {
	{0x02B80080,			0x00000001},
	{0x02B80088,			0x00000001},
	{0x02B80090,			0x00000001},
	{0x02B80098,			0x00000001},
	{0x02B800A0,			0x00000001},
	{0x02B800A8,			0x00000001},
	{0x02B800B0,			0x00000001},
	{0x02B800B8,			0x00000001},
	{0x02B80280,			0x00000001},
	{0x02B80288,			0x00000001},
	{0x02B80290,			0x00000001},
	{0x02B80298,			0x00000001},
	{0x02B802A0,			0x00000001},
	{0x02B802A8,			0x00000001},
	{0x02B802B0,			0x00000001},
	{0x02B802B8,			0x00000001},
};

static int cs35l41_dsp_init(struct cs35l41_private *cs35l41)
{
	struct wm_adsp *dsp;
	int ret;

	dsp = &cs35l41->dsp;
	dsp->num = 1;
	dsp->type = WMFW_HALO;
	dsp->rev = 0;
	dsp->fw = 9; /* 9 is WM_ADSP_FW_SPK_PROT in wm_adsp.c */
	dsp->dev = cs35l41->dev;
	dsp->regmap = cs35l41->regmap;
	dsp->tuning_has_prefix = cs35l41->pdata.tuning_has_prefix;

	if (!cs35l41->pdata.fwname_use_revid)
		dsp->part = "cs35l41";

	dsp->base = CS35L41_DSP1_CTRL_BASE;
	dsp->base_sysinfo = CS35L41_DSP1_SYS_ID;
	dsp->mem = cs35l41_dsp1_regions;
	dsp->num_mems = ARRAY_SIZE(cs35l41_dsp1_regions);
	dsp->lock_regions = 0xFFFFFFFF;

	regmap_multi_reg_write(cs35l41->regmap,
			cs35l41_fs_errata_patch,
			ARRAY_SIZE(cs35l41_fs_errata_patch));

	mutex_init(&cs35l41->rate_lock);
	ret = wm_halo_init(dsp, &cs35l41->rate_lock);
	if (ret != 0) {
		dev_err(cs35l41->dev, "wm_halo_init failed\n");
		goto err;
	}

	cs35l41->halo_booted = false;

	ret = regmap_write(cs35l41->regmap, CS35L41_DSP1_RX5_SRC,
					CS35L41_INPUT_SRC_VPMON);
	if (ret < 0) {
		dev_err(cs35l41->dev, "Write INPUT_SRC_VPMON failed\n");
		goto err_dsp;
	}
	ret = regmap_write(cs35l41->regmap, CS35L41_DSP1_RX6_SRC,
					CS35L41_INPUT_SRC_CLASSH);
	if (ret < 0) {
		dev_err(cs35l41->dev, "Write INPUT_SRC_CLASSH failed\n");
		goto err_dsp;
	}
	ret = regmap_write(cs35l41->regmap, CS35L41_DSP1_RX7_SRC,
					CS35L41_INPUT_SRC_TEMPMON);
	if (ret < 0) {
		dev_err(cs35l41->dev, "Write INPUT_SRC_TEMPMON failed\n");
		goto err_dsp;
	}
	ret = regmap_write(cs35l41->regmap, CS35L41_DSP1_RX8_SRC,
					CS35L41_INPUT_SRC_RSVD);
	if (ret < 0) {
		dev_err(cs35l41->dev, "Write INPUT_SRC_RSVD failed\n");
		goto err_dsp;
	}

	return 0;

err_dsp:
	wm_adsp2_remove(dsp);
err:
	mutex_destroy(&cs35l41->rate_lock);
	return ret;
}


static int cs35l41_enter_hibernate(struct cs35l41_private *cs35l41)
{
	int i;

	dev_dbg(cs35l41->dev, "%s: hibernate state %d\n",
		__func__, cs35l41->amp_hibernate);

	if (cs35l41->amp_hibernate == CS35L41_HIBERNATE_STANDBY)
		return 0;

	/* read all ctl regs */
	for (i = 0; i < CS35L41_CTRL_CACHE_SIZE; i++)
		regmap_read(cs35l41->regmap, cs35l41_ctl_cache_regs[i],
			    &cs35l41->ctl_cache[i]);

	/* Disable interrupts */
	regmap_write(cs35l41->regmap, CS35L41_IRQ1_MASK1, 0xFFFFFFFF);
	disable_irq(cs35l41->irq);

	/* Reset DSP sticky bit */
	regmap_write(cs35l41->regmap, CS35L41_IRQ2_STATUS2,
			1 << CS35L41_CSPL_MBOX_CMD_DRV_SHIFT);

	/* Reset AP sticky bit */
	regmap_write(cs35l41->regmap, CS35L41_IRQ1_STATUS2,
			1 << CS35L41_CSPL_MBOX_CMD_FW_SHIFT);

	regmap_write(cs35l41->regmap, CS35L41_WAKESRC_CTL, 0x0088);
	regmap_write(cs35l41->regmap, CS35L41_WAKESRC_CTL, 0x0188);

	regmap_write(cs35l41->regmap, CS35L41_CSPL_MBOX_CMD_DRV,
			CSPL_MBOX_CMD_HIBERNATE);

	regcache_cache_only(cs35l41->regmap, true);

	cs35l41->amp_hibernate = CS35L41_HIBERNATE_STANDBY;
	return 0;
}

static int cs35l41_wait_for_pwrmgt_sts(struct cs35l41_private *cs35l41)
{
	int i, ret = 0;
	unsigned int wrpend_sts = 0x2;

	for (i = 0; (i < 10) && (wrpend_sts & 0x2); i++)
		ret = regmap_read(cs35l41->regmap, CS35L41_PWRMGT_STS,
				  &wrpend_sts);
	return ret;
}

static int cs35l41_exit_hibernate(struct cs35l41_private *cs35l41)
{
	int timeout = 20, ret;
	unsigned int status;
	int retries = 5, i;
	u32 *p_trim_data;

	dev_dbg(cs35l41->dev, "%s: hibernate state %d\n",
		__func__, cs35l41->amp_hibernate);

	if (cs35l41->amp_hibernate != CS35L41_HIBERNATE_STANDBY)
		return 0;

	/* update any regs that changed while in cache-only mode */
	for (i = 0; i < CS35L41_CTRL_CACHE_SIZE; i++)
		regmap_read(cs35l41->regmap, cs35l41_ctl_cache_regs[i],
			    &cs35l41->ctl_cache[i]);

	regcache_cache_only(cs35l41->regmap, false);

	do {
		do {
			ret = regmap_write(cs35l41->regmap,
					   CS35L41_CSPL_MBOX_CMD_DRV,
					   CSPL_MBOX_CMD_OUT_OF_HIBERNATE);
			if (ret < 0)
				dev_dbg(cs35l41->dev,
					"%s: wakeup write fail\n", __func__);

			usleep_range(100, 200);

			ret = regmap_read(cs35l41->regmap,
					  CS35L41_CSPL_MBOX_STS, &status);
			if (ret < 0)
				dev_err(cs35l41->dev,
					"%s: mbox status read fail\n",
					__func__);

		} while (status != CSPL_MBOX_STS_PAUSED && --timeout > 0);

		if (timeout != 0) {
			dev_dbg(cs35l41->dev, "wake success at cycle %d\n",
				timeout);
			break;
		}

		dev_err(cs35l41->dev, "hibernate wake failed\n");

		cs35l41_wait_for_pwrmgt_sts(cs35l41);
		regmap_write(cs35l41->regmap, CS35L41_WAKESRC_CTL, 0x0088);

		cs35l41_wait_for_pwrmgt_sts(cs35l41);
		regmap_write(cs35l41->regmap, CS35L41_WAKESRC_CTL, 0x0188);

		cs35l41_wait_for_pwrmgt_sts(cs35l41);
		regmap_write(cs35l41->regmap, CS35L41_PWRMGT_CTL, 0x3);

		timeout = 10;

	} while (--retries > 0);

	/* Reset DSP sticky bit */
	regmap_write(cs35l41->regmap, CS35L41_IRQ2_STATUS2,
			1 << CS35L41_CSPL_MBOX_CMD_DRV_SHIFT);

	/* Reset AP sticky bit */
	regmap_write(cs35l41->regmap, CS35L41_IRQ1_STATUS2,
			1 << CS35L41_CSPL_MBOX_CMD_FW_SHIFT);

	cs35l41->amp_hibernate = CS35L41_HIBERNATE_AWAKE;

	/* invalidate all cached values which have now been reset */
	regcache_drop_region(cs35l41->regmap, CS35L41_DEVID,
					CS35L41_MIXER_NGATE_CH2_CFG);

	/* sync all control regs to cache value */
	for (i = 0; i < CS35L41_CTRL_CACHE_SIZE; i++)
		regmap_write(cs35l41->regmap,
				cs35l41_ctl_cache_regs[i],
				cs35l41->ctl_cache[i]);

	regmap_write(cs35l41->regmap, CS35L41_TEST_KEY_CTL, 0x00000055);
	regmap_write(cs35l41->regmap, CS35L41_TEST_KEY_CTL, 0x000000AA);

	/* trim with cache values */
	p_trim_data = cs35l41->trim_cache;
	for (i = 0; i < CS35L41_TRIM_CACHE_REGIONS; i++) {
		regmap_raw_write(cs35l41->regmap,
				cs35l41_trim_cache_regs[i].reg,
				p_trim_data, cs35l41_trim_cache_regs[i].size *
				sizeof(u32));
		p_trim_data += cs35l41_trim_cache_regs[i].size;
	}

	regmap_write(cs35l41->regmap, CS35L41_TEST_KEY_CTL, 0x000000CC);
	regmap_write(cs35l41->regmap, CS35L41_TEST_KEY_CTL, 0x00000033);

	retries = 5;

	do {
		dev_dbg(cs35l41->dev, "cs35l41_restore attempt %d\n",
		 6 - retries);
		ret = cs35l41_restore(cs35l41);
		usleep_range(4000, 5000);
	} while (ret < 0 && --retries > 0);

	if (retries <= 0)
		dev_err(cs35l41->dev, "Failed to exit from hibernate\n");
	else
		dev_dbg(cs35l41->dev, "cs35l41 restored in %d attempts\n",
			6 - retries);

	enable_irq(cs35l41->irq);

	return ret;
}

/* Restore amp state after hibernate */
static int cs35l41_restore(struct cs35l41_private *cs35l41)
{
	int ret;
	u32 regid, reg_revid, mtl_revid, chipid_match;

	ret = regmap_read(cs35l41->regmap, CS35L41_DEVID, &regid);
	if (ret < 0) {
		dev_err(cs35l41->dev, "%s: Get Device ID fail\n", __func__);
		return -ENODEV;
	}

	ret = regmap_read(cs35l41->regmap, CS35L41_REVID, &reg_revid);
	if (ret < 0) {
		dev_err(cs35l41->dev, "Get Revision ID failed\n");
		return -ENODEV;
	}

	mtl_revid = reg_revid & CS35L41_MTLREVID_MASK;
	chipid_match = (mtl_revid % 2) ? CS35L41R_CHIP_ID : CS35L41_CHIP_ID;
	if (regid != chipid_match) {
		dev_err(cs35l41->dev, "CS35L41 Device ID (%X). Expected ID %X\n",
			regid, chipid_match);
		return -ENODEV;
	}

	cs35l41_irq_gpio_config(cs35l41);

	regmap_write(cs35l41->regmap, CS35L41_IRQ1_MASK1,
		CS35L41_INT1_MASK_DEFAULT);

	regmap_write(cs35l41->regmap,
		     CS35L41_DSP1_RX5_SRC, CS35L41_INPUT_SRC_VPMON);
	regmap_write(cs35l41->regmap,
		     CS35L41_DSP1_RX6_SRC, CS35L41_INPUT_SRC_CLASSH);
	regmap_write(cs35l41->regmap,
		     CS35L41_DSP1_RX7_SRC, CS35L41_INPUT_SRC_TEMPMON);
	regmap_write(cs35l41->regmap,
		     CS35L41_DSP1_RX8_SRC, CS35L41_INPUT_SRC_RSVD);

	switch (reg_revid) {
	case CS35L41_REVID_A0:
		ret = regmap_multi_reg_write(cs35l41->regmap,
				cs35l41_reva0_errata_patch,
				ARRAY_SIZE(cs35l41_reva0_errata_patch));
		if (ret < 0) {
			dev_err(cs35l41->dev,
				"Failed to apply A0 errata patch %d\n", ret);
		}
		break;
	case CS35L41_REVID_B0:
		ret = regmap_multi_reg_write(cs35l41->regmap,
				cs35l41_revb0_errata_patch,
				ARRAY_SIZE(cs35l41_revb0_errata_patch));
		if (ret < 0) {
			dev_err(cs35l41->dev,
				"Failed to apply B0 errata patch %d\n", ret);
		}
		break;
	case CS35L41_REVID_B2:
		ret = regmap_multi_reg_write(cs35l41->regmap,
				cs35l41_revb2_errata_patch,
				ARRAY_SIZE(cs35l41_revb2_errata_patch));
		if (ret < 0) {
			dev_err(cs35l41->dev,
				"Failed to apply B2 errata patch %d\n", ret);
		}
		break;
	}

	dev_dbg(cs35l41->dev, "Restored CS35L41 (%x), Revision: %02X\n",
		regid, reg_revid);

	cs35l41_set_pdata(cs35l41);

	/* Restore cached values set by ALSA during or before amp reset */
	regmap_update_bits(cs35l41->regmap,
			CS35L41_SP_FRAME_RX_SLOT,
			CS35L41_ASP_RX1_SLOT_MASK,
			((cs35l41->pdata.right_channel) ? 1 : 0)
			<< CS35L41_ASP_RX1_SLOT_SHIFT);
	regmap_update_bits(cs35l41->regmap,
			CS35L41_SP_FRAME_RX_SLOT,
			CS35L41_ASP_RX2_SLOT_MASK,
			((cs35l41->pdata.right_channel) ? 0 : 1)
			<< CS35L41_ASP_RX2_SLOT_SHIFT);

	if (cs35l41->reset_cache.extclk_cfg) {
	/* These values are already cached in cs35l41_private struct */
		if (cs35l41->clksrc == CS35L41_PLLSRC_SCLK)
			regmap_update_bits(cs35l41->regmap,
					   CS35L41_SP_RATE_CTRL, 0x3F,
					   cs35l41->extclk_cfg);

		regmap_update_bits(cs35l41->regmap, CS35L41_PLL_CLK_CTRL,
				CS35L41_PLL_OPENLOOP_MASK,
				1 << CS35L41_PLL_OPENLOOP_SHIFT);
		regmap_update_bits(cs35l41->regmap, CS35L41_PLL_CLK_CTRL,
				CS35L41_REFCLK_FREQ_MASK,
				cs35l41->extclk_cfg <<
				CS35L41_REFCLK_FREQ_SHIFT);
		regmap_update_bits(cs35l41->regmap, CS35L41_PLL_CLK_CTRL,
				CS35L41_PLL_CLK_EN_MASK,
				0 << CS35L41_PLL_CLK_EN_SHIFT);
		regmap_update_bits(cs35l41->regmap, CS35L41_PLL_CLK_CTRL,
				CS35L41_PLL_CLK_SEL_MASK, cs35l41->clksrc);
		regmap_update_bits(cs35l41->regmap, CS35L41_PLL_CLK_CTRL,
				CS35L41_PLL_OPENLOOP_MASK,
				0 << CS35L41_PLL_OPENLOOP_SHIFT);
		regmap_update_bits(cs35l41->regmap, CS35L41_PLL_CLK_CTRL,
				CS35L41_PLL_CLK_EN_MASK,
				1 << CS35L41_PLL_CLK_EN_SHIFT);
		//regmap_write(cs35l41->regmap, CS35L41_PLL_CLK_CTRL, 0x430);
	}

	if (cs35l41->reset_cache.asp_width >= 0) {
		regmap_update_bits(cs35l41->regmap, CS35L41_SP_FORMAT,
				CS35L41_ASP_WIDTH_RX_MASK,
				cs35l41->reset_cache.asp_width <<
					CS35L41_ASP_WIDTH_RX_SHIFT);
		regmap_update_bits(cs35l41->regmap, CS35L41_SP_FORMAT,
				CS35L41_ASP_WIDTH_TX_MASK,
				cs35l41->reset_cache.asp_width <<
					CS35L41_ASP_WIDTH_TX_SHIFT);
	}

	if (cs35l41->reset_cache.asp_wl >= 0) {
		regmap_update_bits(cs35l41->regmap, CS35L41_SP_RX_WL,
				CS35L41_ASP_RX_WL_MASK,
				cs35l41->reset_cache.asp_wl <<
					CS35L41_ASP_RX_WL_SHIFT);
		regmap_update_bits(cs35l41->regmap, CS35L41_SP_TX_WL,
				CS35L41_ASP_TX_WL_MASK,
				cs35l41->reset_cache.asp_wl <<
					CS35L41_ASP_TX_WL_SHIFT);
	}

	if (cs35l41->reset_cache.asp_fmt >= 0)
		regmap_update_bits(cs35l41->regmap, CS35L41_SP_FORMAT,
			CS35L41_ASP_FMT_MASK,
			cs35l41->reset_cache.asp_fmt << CS35L41_ASP_FMT_SHIFT);

	if (cs35l41->reset_cache.lrclk_fmt >= 0)
		regmap_update_bits(cs35l41->regmap, CS35L41_SP_FORMAT,
				CS35L41_LRCLK_INV_MASK,
				cs35l41->reset_cache.lrclk_fmt <<
				CS35L41_LRCLK_INV_SHIFT);

	if (cs35l41->reset_cache.sclk_fmt >= 0)
		regmap_update_bits(cs35l41->regmap, CS35L41_SP_FORMAT,
				CS35L41_SCLK_INV_MASK,
				cs35l41->reset_cache.sclk_fmt <<
				CS35L41_SCLK_INV_SHIFT);

	if (cs35l41->reset_cache.clock_mode >= 0) {
		regmap_update_bits(cs35l41->regmap, CS35L41_SP_FORMAT,
			CS35L41_SCLK_MSTR_MASK,
			cs35l41->reset_cache.clock_mode <<
			CS35L41_SCLK_MSTR_SHIFT);
		regmap_update_bits(cs35l41->regmap, CS35L41_SP_FORMAT,
			CS35L41_LRCLK_MSTR_MASK,
			cs35l41->reset_cache.clock_mode <<
			CS35L41_LRCLK_MSTR_SHIFT);
	}

	if (cs35l41->reset_cache.fs_cfg >= 0)
		regmap_update_bits(cs35l41->regmap, CS35L41_GLOBAL_CLK_CTRL,
			CS35L41_GLOBAL_FS_MASK,
			cs35l41->reset_cache.fs_cfg << CS35L41_GLOBAL_FS_SHIFT);

	return 0;
}

static int cs35l41_preset_sample_rate(struct cs35l41_private *cs35l41, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cs35l41_fs_rates); i++) {
		if (rate == cs35l41_fs_rates[i].rate)
			break;
	}

	if (i >= ARRAY_SIZE(cs35l41_fs_rates)) {
		dev_err(cs35l41->dev, "%s: Invalid sample rate = %d\n", __func__, rate);
		return -EINVAL;
	}
	regmap_update_bits(cs35l41->regmap, CS35L41_GLOBAL_CLK_CTRL,
			CS35L41_GLOBAL_FS_MASK,
			cs35l41_fs_rates[i].fs_cfg << CS35L41_GLOBAL_FS_SHIFT);

	return 0;
}

static ssize_t cs35l41_amp_short_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct cs35l41_private *cs35l41 = dev_get_drvdata(dev);
	int ret;
	ret = kstrtouint(buf, 0, &cs35l41->amp_short);
	return 0;
}

static ssize_t cs35l41_amp_short_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct cs35l41_private *cs35l41 = dev_get_drvdata(dev);
	ssize_t len = 0;
	len += snprintf(buf + len, PAGE_SIZE - len,
				"amp short : %d\n", cs35l41->amp_short);
	return len;
}

static DEVICE_ATTR(amp_short, S_IWUSR | S_IRUGO,
	cs35l41_amp_short_show, cs35l41_amp_short_store);
static struct attribute * cs35l41_attributes[] = {
	&dev_attr_amp_short.attr,
	NULL
};
static struct attribute_group cs35l41_attribute_group = {
	.attrs = cs35l41_attributes,
};

int cs35l41_probe(struct cs35l41_private *cs35l41,
				struct cs35l41_platform_data *pdata)
{
	int ret;
	u32 regid, reg_revid, i, mtl_revid, int_status, chipid_match;
	int timeout = 100;
	int irq_pol = 0;
	u32 *p_trim_data;

	dev_info(cs35l41->dev, "cs35l41 probe E\n");

	cs35l41->fast_switch_en = false;
	cs35l41->fast_switch_file_idx = 0;
	cs35l41->reload_tuning = false;

	for (i = 0; i < ARRAY_SIZE(cs35l41_supplies); i++)
		cs35l41->supplies[i].supply = cs35l41_supplies[i];

	cs35l41->num_supplies = ARRAY_SIZE(cs35l41_supplies);

	ret = devm_regulator_bulk_get(cs35l41->dev, cs35l41->num_supplies,
					cs35l41->supplies);
	if (ret != 0) {
		dev_err(cs35l41->dev,
			"Failed to request core supplies: %d\n",
			ret);
		return ret;
	}

	if (pdata) {
		cs35l41->pdata = *pdata;
	} else if (cs35l41->dev->of_node) {
		ret = cs35l41_handle_of_data(cs35l41->dev, &cs35l41->pdata,
					     cs35l41);
		if (ret != 0) {
			ret = -ENODEV;
			goto err;
		}
	} else {
#ifdef CONFIG_ACPI

#else
		ret = -ENODEV;
		goto err;
#endif
	}

	ret = regulator_bulk_enable(cs35l41->num_supplies, cs35l41->supplies);
	if (ret != 0) {
		dev_err(cs35l41->dev,
			"Failed to enable core supplies: %d\n", ret);
		return ret;
	}

	/* returning NULL can be an option if in stereo mode */
	cs35l41->reset_gpio = devm_gpiod_get_optional(cs35l41->dev, "reset",
							GPIOD_OUT_LOW);
	if (IS_ERR(cs35l41->reset_gpio)) {
		ret = PTR_ERR(cs35l41->reset_gpio);
		cs35l41->reset_gpio = NULL;
		if (ret == -EBUSY) {
			dev_info(cs35l41->dev,
				 "Reset line busy, assuming shared reset\n");
		} else {
			dev_err(cs35l41->dev,
				"Failed to get reset GPIO: %d\n", ret);
			goto err;
		}
	}
	if (cs35l41->reset_gpio) {
		/* satisfy minimum reset pulse width spec */
		usleep_range(2000, 2100);
		gpiod_set_value_cansleep(cs35l41->reset_gpio, 1);
		printk(KERN_INFO "[CSPL] Set rest gpio can sleep\n");
	}

	usleep_range(2000, 2100);

    /* Add a switch for L2 project to switch between differnt modes */
	cs35l41_spksw_gpio_init(cs35l41);

	do {
		if (timeout == 0) {
			dev_err(cs35l41->dev,
				"Timeout waiting for OTP_BOOT_DONE\n");
			ret = -EBUSY;
			goto err;
		}
		usleep_range(1000, 1100);
		regmap_read(cs35l41->regmap, CS35L41_IRQ1_STATUS4, &int_status);
		timeout--;
	} while (!(int_status & CS35L41_OTP_BOOT_DONE));

	regmap_read(cs35l41->regmap, CS35L41_IRQ1_STATUS3, &int_status);
	if (int_status & CS35L41_OTP_BOOT_ERR) {
		dev_err(cs35l41->dev, "OTP Boot error\n");
		ret = -EINVAL;
		goto err;
	}

	ret = regmap_read(cs35l41->regmap, CS35L41_DEVID, &regid);
	if (ret < 0) {
		dev_err(cs35l41->dev, "Get Device ID failed\n");
		goto err;
	}

	ret = regmap_read(cs35l41->regmap, CS35L41_REVID, &reg_revid);
	if (ret < 0) {
		dev_err(cs35l41->dev, "Get Revision ID failed\n");
		goto err;
	}

	mtl_revid = reg_revid & CS35L41_MTLREVID_MASK;

	/* CS35L41 will have even MTLREVID
	 * CS35L41R will have odd MTLREVID
	 */
	chipid_match = (mtl_revid % 2) ? CS35L41R_CHIP_ID : CS35L41_CHIP_ID;
	if (regid != chipid_match) {
		dev_err(cs35l41->dev, "CS35L41 Device ID (%X). Expected ID %X\n",
			regid, chipid_match);
		ret = -ENODEV;
		goto err;
	}

	irq_pol = cs35l41_irq_gpio_config(cs35l41);

	mutex_init(&cs35l41->vol_ctl.vol_mutex);
	cs35l41->vol_ctl.dig_vol = 0;
	cs35l41->vol_ctl.ramp_init_att = 0;
	cs35l41->vol_ctl.ramp_knee_att = 0;
	cs35l41->vol_ctl.ramp_knee_time = 0;
	cs35l41->vol_ctl.ramp_end_time = 0;
	atomic_set(&cs35l41->vol_ctl.playback, 0);
	atomic_set(&cs35l41->vol_ctl.vol_ramp, 0);
	atomic_set(&cs35l41->vol_ctl.manual_ramp, 0);
	atomic_set(&cs35l41->vol_ctl.ramp_abort, 0);
	cs35l41->vol_ctl.auto_ramp_timeout = 0;
	cs35l41->vol_ctl.output_dev = CS35L41_OUTPUT_DEV_SPK;
	cs35l41->vol_ctl.prev_active_dev = CS35L41_OUTPUT_DEV_SPK;
	cs35l41->vol_ctl.ramp_wq =
		create_singlethread_workqueue("cs35l41_ramp");
	INIT_WORK(&cs35l41->vol_ctl.ramp_work, cs35l41_vol_ramp);

	ret = devm_request_threaded_irq(cs35l41->dev, cs35l41->irq, NULL,
			cs35l41_irq, IRQF_ONESHOT | IRQF_SHARED | irq_pol,
			"cs35l41", cs35l41);

	/* CS35L41 needs INT for PDN_DONE */
	if (ret != 0) {
		dev_err(cs35l41->dev, "Failed to request IRQ: %d\n", ret);
		//ret = -ENODEV;
		//goto err;
	}

	/* Set interrupt masks for critical errors */
	regmap_write(cs35l41->regmap, CS35L41_IRQ1_MASK1,
			CS35L41_INT1_MASK_DEFAULT);

	mutex_init(&cs35l41->force_int_lock);

	if (!cs35l41->pdata.fwname_use_revid)
		cs35l41->dsp.part = cs35l41->dt_name;

	switch (reg_revid) {
	case CS35L41_REVID_A0:
		cs35l41->amp_hibernate = CS35L41_HIBERNATE_INCOMPATIBLE;
		if (cs35l41->pdata.fwname_use_revid)
			cs35l41->dsp.part = "cs35l41-revA";
		ret = regmap_multi_reg_write(cs35l41->regmap,
				cs35l41_reva0_errata_patch,
				ARRAY_SIZE(cs35l41_reva0_errata_patch));
		if (ret < 0) {
			dev_err(cs35l41->dev,
				"Failed to apply A0 errata patch %d\n", ret);
			goto err;
		}
		break;
	case CS35L41_REVID_B0:
		cs35l41->amp_hibernate = CS35L41_HIBERNATE_INCOMPATIBLE;
		if (cs35l41->pdata.fwname_use_revid)
			cs35l41->dsp.part = "cs35l41-revB0";
		ret = regmap_multi_reg_write(cs35l41->regmap,
				cs35l41_revb0_errata_patch,
				ARRAY_SIZE(cs35l41_revb0_errata_patch));
		if (ret < 0) {
			dev_err(cs35l41->dev,
				"Failed to apply B0 errata patch %d\n", ret);
			goto err;
		}
		break;
	case CS35L41_REVID_B2:
		if (cs35l41->pdata.fwname_use_revid)
			cs35l41->dsp.part = "cs35l41-revB2";
		ret = regmap_multi_reg_write(cs35l41->regmap,
				cs35l41_revb2_errata_patch,
				ARRAY_SIZE(cs35l41_revb2_errata_patch));
		if (ret < 0) {
			dev_err(cs35l41->dev,
				"Failed to apply B2 errata patch %d\n", ret);
			goto err;
		}

		if (cs35l41->pdata.hibernate_enable)
			cs35l41->amp_hibernate = CS35L41_HIBERNATE_NOT_LOADED;
		else
			cs35l41->amp_hibernate = CS35L41_HIBERNATE_INCOMPATIBLE;

		cs35l41->reset_cache.extclk_cfg = false;
		cs35l41->reset_cache.asp_wl = -1;
		cs35l41->reset_cache.asp_width = -1;
		cs35l41->reset_cache.asp_fmt = -1;
		cs35l41->reset_cache.sclk_fmt = -1;
		cs35l41->reset_cache.clock_mode = -1;
		cs35l41->reset_cache.lrclk_fmt = -1;
		cs35l41->reset_cache.fs_cfg = -1;
		break;
	}

	ret = cs35l41_otp_unpack(cs35l41);
	if (ret < 0) {
		dev_err(cs35l41->dev, "OTP Unpack failed\n");
		goto err_otp;
	}

	/* read all trim regs */
	p_trim_data = cs35l41->trim_cache;
	for (i = 0; i < CS35L41_TRIM_CACHE_REGIONS; i++) {
		regmap_raw_read(cs35l41->regmap,
				cs35l41_trim_cache_regs[i].reg,
				p_trim_data, cs35l41_trim_cache_regs[i].size *
				sizeof(u32));
		p_trim_data += cs35l41_trim_cache_regs[i].size;
	}

	ret = regmap_write(cs35l41->regmap, CS35L41_DSP1_CCM_CORE_CTRL, 0);
	if (ret < 0) {
		dev_err(cs35l41->dev, "Write CCM_CORE_CTRL failed\n");
		goto err_otp;
	}

	ret = cs35l41_dsp_init(cs35l41);
	if (ret < 0) {
		dev_err(cs35l41->dev, "%s: dsp_init failed\n",
			__func__);
		goto err_otp;
	}

	ret = snd_soc_register_component(cs35l41->dev,
					&soc_component_dev_cs35l41,
					cs35l41_dai, ARRAY_SIZE(cs35l41_dai));
	if (ret < 0) {
		dev_err(cs35l41->dev, "%s: Register codec failed\n", __func__);
		goto err_dsp;
	}

	dev_info(cs35l41->dev, "Cirrus Logic CS35L41 (%x), Revision: %02X\n",
			regid, reg_revid);

	cs35l41->wq = create_singlethread_workqueue("cs35l41");
	if (cs35l41->wq == NULL) {
		ret = -ENOMEM;
		goto err_codec;
	}

	INIT_DELAYED_WORK(&cs35l41->hb_work, cs35l41_hibernate_work);
	mutex_init(&cs35l41->hb_lock);

	// Initial Brownout parameter
	ret = regmap_update_bits(cs35l41->regmap, CS35L41_PWR_CTRL3, 0x1000, 0x1000);
#if defined(CONFIG_TARGET_PRODUCT_ZEUS) // L2
	ret = regmap_write(cs35l41->regmap, CS35L41_VPBR_CFG, 0x02005309);
#elif defined(CONFIG_TARGET_PRODUCT_CUPID) // L3
	ret = regmap_write(cs35l41->regmap, CS35L41_VPBR_CFG, 0x02005306);
#elif defined(CONFIG_TARGET_PRODUCT_INGRES) // L10
	ret = regmap_write(cs35l41->regmap, CS35L41_PWR_CTRL3, 0x01001010);
	ret = regmap_write(cs35l41->regmap, CS35L41_VPBR_CFG, 0x02AA130A);
#else
	ret = regmap_write(cs35l41->regmap, CS35L41_VPBR_CFG, 0x02005306);
#endif

	/* Workarounds for specific project: */
	// Preset sample rate
	cs35l41_preset_sample_rate(cs35l41, 96000);

	ret = sysfs_create_group(&cs35l41->dev->kobj, &cs35l41_attribute_group);
	if (ret < 0) {
		dev_err(cs35l41->dev, "failed to create sysfs attr files");
	}

	dev_info(cs35l41->dev, "cs35l41 probe X\n");

	return 0;
err_codec:
	snd_soc_unregister_component(cs35l41->dev);
err_dsp:
	wm_adsp2_remove(&cs35l41->dsp);
err_otp:
	destroy_workqueue(cs35l41->vol_ctl.ramp_wq);
	mutex_destroy(&cs35l41->force_int_lock);
	mutex_destroy(&cs35l41->vol_ctl.vol_mutex);
err:
	regulator_bulk_disable(cs35l41->num_supplies, cs35l41->supplies);
	return ret;
}

int cs35l41_remove(struct cs35l41_private *cs35l41)
{
	destroy_workqueue(cs35l41->wq);
	sysfs_remove_group(&cs35l41->dev->kobj, &cs35l41_attribute_group);
	mutex_destroy(&cs35l41->hb_lock);
	destroy_workqueue(cs35l41->vol_ctl.ramp_wq);
	mutex_destroy(&cs35l41->vol_ctl.vol_mutex);
	regmap_write(cs35l41->regmap, CS35L41_IRQ1_MASK1, 0xFFFFFFFF);
	mutex_destroy(&cs35l41->force_int_lock);
	wm_adsp2_remove(&cs35l41->dsp);
	mutex_destroy(&cs35l41->rate_lock);
	regulator_bulk_disable(cs35l41->num_supplies, cs35l41->supplies);
	snd_soc_unregister_component(cs35l41->dev);
	return 0;
}

MODULE_DESCRIPTION("ASoC CS35L41 driver");
MODULE_AUTHOR("David Rhodes, Cirrus Logic Inc, <david.rhodes@cirrus.com>");
MODULE_LICENSE("GPL");
