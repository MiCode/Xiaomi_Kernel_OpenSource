/*
 * Copyright (C) NXP Semiconductors (PLMA)
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s(%s): " fmt, __func__, tfa98xx->fw.name
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/acpi.h>
#include <sound/tfa98xx.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/firmware.h>

#include "tfa98xx-core.h"
#include "tfa98xx-regs.h"
#include "tfa_container.h"
#include "tfa_dsp.h"

#define I2C_RETRY_DELAY		5 /* ms */
#define I2C_RETRIES		5
#define PLL_SYNC_RETRIES	10
#define MTPB_RETRIES		5

#define MAX_CONTROL_NAME	32

/* SNDRV_PCM_RATE_KNOT -> 12000, 24000 Hz, limit with constraint list */
#define TFA98XX_RATES (SNDRV_PCM_RATE_8000_48000 | SNDRV_PCM_RATE_KNOT)
#define TFA98XX_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE)

#define TFA98XX_STATUS_UP_MASK	(TFA98XX_STATUSREG_PLLS | \
				 TFA98XX_STATUSREG_CLKS | \
				 TFA98XX_STATUSREG_VDDS | \
				 TFA98XX_STATUSREG_AREFS)

/*
 * I2C Read/Write Functions
 */

int tfa98xx_i2c_read(struct i2c_client *tfa98xx_client,	u8 reg, u8 *value,
		     int len)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
			.addr = tfa98xx_client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = tfa98xx_client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = value,
		},
	};

	do {
		err = i2c_transfer(tfa98xx_client->adapter, msgs,
							ARRAY_SIZE(msgs));
		if (err != ARRAY_SIZE(msgs))
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

	if (err != ARRAY_SIZE(msgs)) {
		dev_err(&tfa98xx_client->dev, "read transfer error %d\n" , err);
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

int tfa98xx_bulk_write_raw(struct snd_soc_codec *codec, const u8 *data,
				u8 count)
{
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	int ret;

	ret = i2c_master_send(tfa98xx->i2c, data, count);
	if (ret == count) {
		return 0;
	} else if (ret < 0) {
		pr_err("Error I2C send %d\n", ret);
		return ret;
	} else {
		pr_err("Error I2C send size mismatch %d\n", ret);
		return -EIO;
	}
}

static void tfa98xx_monitor(struct work_struct *work)
{
	struct tfa98xx *tfa98xx = container_of(work, struct tfa98xx,
						delay_work.work);
	u16 val;

	pr_debug("%s()\n", __func__);

	if (!tfa98xx->monitor_status) {
		pr_info("stop triggering monitor workqueue\n");
		return;
	}

	mutex_lock(&tfa98xx->dsp_init_lock);

	/*
	 * check IC status bits: cold start, amp switching, speaker error
	 * and DSP watch dog bit to re init
	 */
	val = snd_soc_read(tfa98xx->codec, TFA98XX_STATUSREG);
	pr_debug("SYS_STATUS: 0x%04x\n", val);
	if ((TFA98XX_STATUSREG_ACS & val) ||
	    (TFA98XX_STATUSREG_WDS & val)) {
		tfa98xx->dsp_init = TFA98XX_DSP_INIT_RECOVER;

		if (TFA98XX_STATUSREG_ACS & val)
			pr_err("ERROR: ACS\n");
		if (TFA98XX_STATUSREG_WDS & val)
			pr_err("ERROR: WDS\n");

		/* schedule init now if the clocks are up and stable */
		if ((val & TFA98XX_STATUS_UP_MASK) == TFA98XX_STATUS_UP_MASK)
			queue_work(tfa98xx->tfa98xx_wq, &tfa98xx->init_work);
	}

	mutex_unlock(&tfa98xx->dsp_init_lock);

	/* else just reschedule */
	queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->delay_work, 5*HZ);


}


static void tfa98xx_dsp_init(struct work_struct *work)
{
	struct tfa98xx *tfa98xx = container_of(work, struct tfa98xx, init_work);

	pr_debug("profile %d, vstep %d\n", tfa98xx->profile_ctl,
		 tfa98xx->vstep_ctl);

	mutex_lock(&tfa98xx->dsp_init_lock);

	/* start the DSP using the latest profile / vstep */
	if (!tfa98xx_dsp_start(tfa98xx, tfa98xx->profile_ctl,
			       tfa98xx->vstep_ctl))
		tfa98xx->dsp_init = TFA98XX_DSP_INIT_DONE;

	mutex_unlock(&tfa98xx->dsp_init_lock);
}

/*
 * ASOC OPS
*/

static u32 tfa98xx_asrc_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

static struct snd_pcm_hw_constraint_list constraints_12_24 = {
	.list   = tfa98xx_asrc_rates,
	.count  = ARRAY_SIZE(tfa98xx_asrc_rates),
};

static int tfa98xx_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct tfa98xx *tfa98xx =
				snd_soc_codec_get_drvdata(codec_dai->codec);

	pr_debug("%s() freq %d, dir %d\n", __func__, freq, dir);

	tfa98xx->sysclk = freq;
	return 0;
}

static int tfa98xx_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct tfa98xx *tfa98xx =
				snd_soc_codec_get_drvdata(codec_dai->codec);
	u16 val;

	pr_debug("\n");

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		/* default value */
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
	default:
		/* only supports Slave mode */
		pr_err("tfa98xx: invalid DAI master/slave interface\n");
		return -EINVAL;
	}
	val = snd_soc_read(codec, TFA98XX_AUDIOREG);
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		/* default value */
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		val &= ~(TFA98XX_FORMAT_MASK);
		val |= TFA98XX_FORMAT_LSB;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		val &= ~(TFA98XX_FORMAT_MASK);
		val |= TFA98XX_FORMAT_MSB;
		break;
	default:
		pr_err("tfa98xx: invalid DAI interface format\n");
		return -EINVAL;
	}

	snd_soc_write(codec, TFA98XX_AUDIOREG, val);

	return 0;
}

static int tfa98xx_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);

	pr_debug("Store rate: %d\n", params_rate(params));

	/* Store rate for further use during DSP init */
	tfa98xx->rate = params_rate(params);

	return 0;
}

static int tfa98xx_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);

	pr_debug("state: %d\n", mute);

	mutex_lock(&tfa98xx->dsp_init_lock);

	if (mute) {
		cancel_delayed_work_sync(&tfa98xx->delay_work);

		/*
		 * need to wait for amp to stop switching, to minimize
		 * pop, else I2S clk is going away too soon interrupting
		 * the dsp from smothering the amp pop while turning it
		 * off, It shouldn't take more than 50 ms for the amp
		 * switching to stop.
		 */
		tfa98xx_dsp_stop(tfa98xx);
	} else {
		/*
		 * start monitor thread to check IC status bit 5secs, and
		 * re-init IC to recover.
		 */
		queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->delay_work,
				   HZ);
	}

	mutex_unlock(&tfa98xx->dsp_init_lock);

	return 0;
}

static int tfa98xx_startup(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);

	pr_debug("\n");
	if (NULL == substream->runtime) {
		pr_warn("runtime is NULL. I think it is offload now.\n");
		return 0;
	}

	snd_pcm_hw_constraint_list(substream->runtime, 0,
				   SNDRV_PCM_HW_PARAM_RATE,
				   &constraints_12_24);

	return 0;
}

static void tfa98xx_shutdown(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);

	pr_debug("\n");
}

/* Trigger callback is atomic function, It gets called when pcm is started */

static int tfa98xx_trigger(struct snd_pcm_substream *substream, int cmd,
			   struct snd_soc_dai *dai)
{
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(dai->codec);
	int ret = 0;

	pr_debug("cmd: %d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/*
		 * To initialize dsp all the I2S clocks must be up and running.
		 * so that the DSP's internal PLL can sync up and memory becomes
		 * accessible. Trigger callback is called when pcm write starts,
		 * so this should be the place where DSP is initialized
		 */
		/* queue_work(tfa98xx->tfa98xx_wq, &tfa98xx->init_work); */
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/*
 * ASOC controls
 */

static const struct snd_soc_dapm_widget tfa98xx_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("I2S1"),
	SND_SOC_DAPM_MIXER("NXP Output Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
};

static char dapm_name[MAX_CONTROL_NAME];
static const struct snd_soc_dapm_route tfa98xx_dapm_routes[] = {
	{"NXP Output Mixer", NULL, dapm_name},
};


/*
 * Helpers for profile selection controls
 */
int tfa98xx_get_profile_ctl(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tfa98xx->profile_ctl;

	/* pr_debug("%s: profile %d\n", tfa98xx->fw.name, tfa98xx->profile); */

	return 0;
}

int tfa98xx_set_profile_ctl(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	struct tfaprofile *profiles =
				(struct tfaprofile *)kcontrol->private_value;
	struct tfaprofile *prof_old = &profiles[tfa98xx->profile_current];
	struct tfaprofile *prof_new;

	if (profiles == NULL) {
		pr_err("No profiles\n");
		return -EINVAL;
	}

	if (tfa98xx->profile_ctl == ucontrol->value.integer.value[0])
		return 0;

	tfa98xx->profile_ctl = ucontrol->value.integer.value[0];
	prof_new = &profiles[tfa98xx->profile_ctl];

	pr_debug("active profile %d, new profile %d\n",
		 tfa98xx->profile_current, tfa98xx->profile_ctl);

	/*
	   adjust the vstep value based on the number of volume steps of the
	   new profile.
	*/
	prof_new->vstep =  (int)(tfa98xx->vstep_ctl * prof_new->vsteps
				/ prof_old->vsteps);

	pr_debug("active vstep %d, new vstep %d\n", prof_old->vstep,
		 prof_new->vstep);

	if (tfa98xx_is_amp_running(tfa98xx)) {
		/*
		   When switching profile when the amplifier is running,
		   there might be pops because of the mute/unmute during
		   profile switch.
		 */
		pr_debug("Warning: switching profile while amplifier is running\n");
		tfa98xx_dsp_start(tfa98xx, tfa98xx->profile_ctl,
				  prof_new->vstep);
	}

	prof_old->vstep = tfa98xx->vstep_ctl;
	tfa98xx->vstep_ctl = prof_new->vstep;

	return 0;
}

int tfa98xx_info_profile_ctl(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	struct tfaprofile *profiles =
				(struct tfaprofile *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);

	if (profiles == NULL) {
		pr_err("No profiles\n");
		return  -EINVAL;
	}

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = tfa98xx->profile_count;

	if (uinfo->value.enumerated.item > tfa98xx->profile_count - 1)
		uinfo->value.enumerated.item = tfa98xx->profile_count - 1;

	strcpy(uinfo->value.enumerated.name,
	       profiles[uinfo->value.enumerated.item].name);

	return 0;
}

/*
 * Helpers for volume through vstep controls
 */
int tfa98xx_get_vol_ctl(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct tfaprofile *profiles =
				(struct tfaprofile *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	int index = tfa98xx->profile_ctl;
	struct tfaprofile *prof = &profiles[index];


	if (profiles == NULL) {
		pr_err("No profiles\n");
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = prof->vsteps - prof->vstep - 1;

	pr_debug("%s: %d/%d\n", prof->name, prof->vstep, prof->vsteps - 1);

	return 0;
}

int tfa98xx_set_vol_ctl(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct tfaprofile *profiles =
				(struct tfaprofile *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	int index = tfa98xx->profile_ctl;
	struct tfaprofile *prof = &profiles[index];

	if (profiles == NULL) {
		pr_err("No profiles\n");
		return -EINVAL;
	}

	if (prof->vstep == prof->vsteps - ucontrol->value.integer.value[0] - 1)
		return 0;


	prof->vstep = prof->vsteps - ucontrol->value.integer.value[0] - 1;

	if (prof->vstep < 0)
		prof->vstep = 0;

	pr_debug("%s: %d/%d\n", prof->name, prof->vstep, prof->vsteps - 1);

	if (tfa98xx_is_amp_running(tfa98xx))
		tfa98xx_dsp_start(tfa98xx, tfa98xx->profile_ctl, prof->vstep);

	tfa98xx->vstep_ctl = prof->vstep;

	return 1;
}

int tfa98xx_info_vol_ctl(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_info *uinfo)
{
	struct tfaprofile *profiles =
				(struct tfaprofile *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	struct tfaprofile *prof = &profiles[tfa98xx->profile_ctl];

	if (profiles == NULL) {
		pr_err("No profiles\n");
		return -EINVAL;
	}

	pr_debug("%s [0..%d]\n", prof->name, prof->vsteps - 1);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = prof->vsteps - 1;

	return 0;
}

int tfa98xx_get_stop_ctl(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] =
		tfa98xx_is_pwdn(tfa98xx) ? 1 : 0;
	return 0;
}

int tfa98xx_set_stop_ctl(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);

	pr_debug("%ld\n", ucontrol->value.integer.value[0]);

	if ((ucontrol->value.integer.value[0] != 0) &&
	    !tfa98xx_is_pwdn(tfa98xx)) {
		cancel_delayed_work_sync(&tfa98xx->delay_work);
		tfa98xx_dsp_stop(tfa98xx);
	} else if ((ucontrol->value.integer.value[0] == 0) &&
	    tfa98xx_is_pwdn(tfa98xx)) {
		queue_work(tfa98xx->tfa98xx_wq, &tfa98xx->init_work);
		queue_delayed_work(tfa98xx->tfa98xx_wq,
				&tfa98xx->delay_work, HZ);
	}

	return 1;
}

static char prof_name[MAX_CONTROL_NAME];
static char vol_name[MAX_CONTROL_NAME];
static char stop_name[MAX_CONTROL_NAME];

static struct snd_kcontrol_new tfa98xx_controls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = prof_name,
		.info = tfa98xx_info_profile_ctl,
		.get = tfa98xx_get_profile_ctl,
		.put = tfa98xx_set_profile_ctl,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = vol_name,
		.info = tfa98xx_info_vol_ctl,
		.get = tfa98xx_get_vol_ctl,
		.put = tfa98xx_set_vol_ctl,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = stop_name,
		.info = snd_soc_info_bool_ext,
		.get = tfa98xx_get_stop_ctl,
		.put = tfa98xx_set_stop_ctl,
	}
};


static const struct snd_soc_dai_ops tfa98xx_ops = {
	.hw_params	= tfa98xx_hw_params,
	.set_fmt	= tfa98xx_set_dai_fmt,
	.set_sysclk	= tfa98xx_set_dai_sysclk,
	.startup	= tfa98xx_startup,
	.shutdown	= tfa98xx_shutdown,
	.trigger	= tfa98xx_trigger,
};


static struct snd_soc_dai_driver tfa98xx_dai = {
	.name = "tfa98xx_codec",
	.playback = {
		.stream_name = NULL,
		.channels_min = 1,
		.channels_max = 2,
		.rates = TFA98XX_RATES,
		.formats = TFA98XX_FORMATS,},
	.ops = &tfa98xx_ops,
	.symmetric_rates = 1,
};

static int tfa98xx_probe(struct snd_soc_codec *codec)
{
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	int ret;
	u16 rev;

	codec->control_data = tfa98xx->regmap;
	tfa98xx->codec = codec;
	codec->cache_bypass = true;

	ret = snd_soc_codec_set_cache_io(codec, 8, 16, SND_SOC_REGMAP);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	/*
	 * some device require a dummy read in order to generate
	 * i2c clocks when accessing the device for the first time
	 */
	snd_soc_read(codec, TFA98XX_REVISIONNUMBER);

	rev = snd_soc_read(codec, TFA98XX_REVISIONNUMBER);
	dev_dbg(codec->dev, "ID revision 0x%04x\n", rev);
	tfa98xx->rev = rev & 0xff;
	tfa98xx->subrev = (rev >> 8) & 0xff;

	ret = tfa98xx_cnt_loadfile(tfa98xx, 0);
	if (ret != 0)
		return ret;

	snd_soc_dapm_new_controls(&codec->dapm, tfa98xx_dapm_widgets,
				  ARRAY_SIZE(tfa98xx_dapm_widgets));

	scnprintf(dapm_name, MAX_CONTROL_NAME, "%s Playback",
			dev_name(&tfa98xx->i2c->dev));
	snd_soc_dapm_add_routes(&codec->dapm, tfa98xx_dapm_routes,
				ARRAY_SIZE(tfa98xx_dapm_routes));

	tfa98xx->profile_current = 0;
	tfa98xx->vstep_current = 0;
	tfa98xx->profile_ctl = 0;
	tfa98xx->vstep_ctl = 0;

	/* Overwrite kcontrol values that need container information */
	tfa98xx_controls[0].private_value = (unsigned long)tfa98xx->profiles,
	tfa98xx_controls[1].private_value = (unsigned long)tfa98xx->profiles,
	scnprintf(prof_name, MAX_CONTROL_NAME, "%s Profile", tfa98xx->fw.name);
	scnprintf(vol_name, MAX_CONTROL_NAME, "%s Master Volume",
		  tfa98xx->fw.name);
	scnprintf(stop_name, MAX_CONTROL_NAME, "%s Stop", tfa98xx->fw.name);

	snd_soc_add_codec_controls(codec, tfa98xx_controls,
				   ARRAY_SIZE(tfa98xx_controls));

	dev_dbg(codec->dev, "tfa98xx codec(%s) registered",
		dev_name(&tfa98xx->i2c->dev));

	return 0;
}

static int tfa98xx_remove(struct snd_soc_codec *codec)
{
	dev_info(codec->dev, "tfa98xx codec removed");
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	if (tfa98xx->fw.base)
		kfree(tfa98xx->fw.base);
	return 0;
}

static struct snd_soc_codec_driver tfa98xx_soc_codec = {
	.probe = tfa98xx_probe,
	.remove = tfa98xx_remove,
};

#ifdef CONFIG_SND_SOC_TFA98XX_DEBUG
static ssize_t tfa98xx_reg_write(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);

	pr_debug("count = %d\n", count);

	if (count != 1) {
		pr_debug("invalid register address");
		return -EINVAL;
	}

	tfa98xx->reg = buf[0];

	return 1;
}

static ssize_t tfa98xx_rw_write(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	u8 *data;
	int ret;

	pr_debug("count = %d\n", count);

	data = kmalloc(count+1, GFP_KERNEL);
	if (data == NULL) {
		pr_debug("can not allocate memory\n");
		return  -ENOMEM;
	}

	data[0] = tfa98xx->reg;
	memcpy(&data[1], buf, count);
	ret = i2c_master_send(tfa98xx->i2c, data, count+1);

	kfree(data);

	return ret;
}

static ssize_t tfa98xx_rw_read(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	struct i2c_msg msgs[] = {
		{
			.addr = tfa98xx->i2c->addr,
			.flags = 0,
			.len = 1,
			.buf = &tfa98xx->reg,
		},
		{
			.addr = tfa98xx->i2c->addr,
			.flags = I2C_M_RD,
			.len = count,
			.buf = buf,
		},
	};
	int ret;

	pr_debug("count = %d\n", count);

	ret = i2c_transfer(tfa98xx->i2c->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0)
		return ret;

	/* ret contains the number of i2c messages send */
	return 1 + ((ret > 1) ? count : 0);
}

static ssize_t tfa98xx_dspmsg_write(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	int ret = 0;

	pr_debug("count = %d\n", count);

	ret = tfa98xx_dsp_msg_write(tfa98xx, count, buf);
	if (ret)
		return ret;

	return count;
}

static ssize_t tfa98xx_dspmsg_read(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	int ret = 0;

	pr_debug("count = %d\n", count);

	ret = tfa98xx_dsp_msg_read(tfa98xx, count, buf);
	if (ret)
		return ret;

	return count;
}

static ssize_t tfa98xx_dspmsg_retries_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	int val, tmp, status = 0;

	status = sscanf(buf, "%d%n", &val, &tmp);
	if (status < 0 || status == 0) {
		pr_debug("Error setting dsp_msg_retries\n");
		return -EINVAL;
	}
	tfa98xx->dsp_msg_retries = val;

	pr_debug("dsp_msg_retries = %d\n", tfa98xx->dsp_msg_retries);

	return size;
}

static ssize_t tfa98xx_dspmsg_retries_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	ssize_t status = 0;

	status += sprintf(&buf[status], "%d\n", tfa98xx->dsp_msg_retries);

	return status;
}


static struct bin_attribute dev_attr_rw = {
	.attr = {
		.name = "rw",
		.mode = S_IRUSR | S_IWUSR,
	},
	.size = 0,
	.read = tfa98xx_rw_read,
	.write = tfa98xx_rw_write,
};

static struct bin_attribute dev_attr_reg = {
	.attr = {
		.name = "reg",
		.mode = S_IWUSR,
	},
	.size = 0,
	.read = NULL,
	.write = tfa98xx_reg_write,
};

static struct bin_attribute dev_attr_dspmsg = {
	.attr = {
		.name = "dspmsg",
		.mode = S_IRUSR | S_IWUSR,
	},
	.size = 0,
	.read = tfa98xx_dspmsg_read,
	.write = tfa98xx_dspmsg_write,
};

static DEVICE_ATTR(dspmsg_retries, S_IRUSR | S_IWUSR,
		tfa98xx_dspmsg_retries_show, tfa98xx_dspmsg_retries_set);
#endif

static ssize_t tfa98xx_monitor_status_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	int val, tmp, status = 0;

	status = sscanf(buf, "%d%n", &val, &tmp);
	if (status < 0 || status == 0) {
		pr_debug("Error setting monitor status.\n");
		return -EINVAL;
	}
	if (val) {
		tfa98xx->monitor_status = 1;
	} else {
		tfa98xx->monitor_status = 0;
	}

	pr_debug("monitor_status = %d\n", tfa98xx->monitor_status);

	return size;
}

static ssize_t tfa98xx_monitor_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	ssize_t status = 0;

	status += sprintf(&buf[status], "%d\n", tfa98xx->monitor_status);

	return status;
}

static DEVICE_ATTR(monitor, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH,
		tfa98xx_monitor_status_show, tfa98xx_monitor_status_store);

static const struct regmap_config tfa98xx_regmap = {
	.reg_bits = 8,
	.val_bits = 16,
	.max_register = TFA98XX_MAX_REGISTER,
	.cache_type = REGCACHE_NONE,
};

static int tfa98xx_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct tfa98xx *tfa98xx;
	int ret;
	struct snd_soc_dai_driver *dai_drv;
	char stream_name[MAX_CONTROL_NAME];

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	tfa98xx = devm_kzalloc(&i2c->dev, sizeof(struct tfa98xx),
			       GFP_KERNEL);
	if (tfa98xx == NULL)
		return -ENOMEM;

	tfa98xx->i2c = i2c;
	tfa98xx->dsp_init = TFA98XX_DSP_INIT_PENDING;

	tfa98xx->regmap = devm_regmap_init_i2c(i2c, &tfa98xx_regmap);
	if (IS_ERR(tfa98xx->regmap)) {
		ret = PTR_ERR(tfa98xx->regmap);
		dev_err(&i2c->dev, "Failed to allocate regmap: %d\n", ret);
		return ret;
	}

	i2c_set_clientdata(i2c, tfa98xx);
	mutex_init(&tfa98xx->dsp_init_lock);

	/* work queue will be used to load DSP fw on first audio playback */
	tfa98xx->tfa98xx_wq = create_singlethread_workqueue("tfa98xx");
	if (tfa98xx->tfa98xx_wq == NULL) {
		ret = -ENOMEM;
		goto wq_fail;
	}

	INIT_WORK(&tfa98xx->init_work, tfa98xx_dsp_init);
	INIT_DELAYED_WORK(&tfa98xx->delay_work, tfa98xx_monitor);

#ifdef CONFIG_SND_SOC_TFA98XX_DEBUG
	/* Register the sysfs files for debugging */
	ret = device_create_bin_file(&i2c->dev, &dev_attr_rw);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs files\n");
	ret = device_create_bin_file(&i2c->dev, &dev_attr_reg);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs files\n");
	ret = device_create_bin_file(&i2c->dev, &dev_attr_dspmsg);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs files\n");

	tfa98xx->dsp_msg_retries = 50;
	ret = device_create_file(&i2c->dev, &dev_attr_dspmsg_retries);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs files\n");


#endif
	tfa98xx->monitor_status = 1;

	/* register codec */
	dai_drv = devm_kzalloc(&i2c->dev, sizeof(struct snd_soc_dai_driver),
				GFP_KERNEL);
	if (dai_drv == NULL) {
		ret = -ENOMEM;
		goto codec_fail;
	}

	*dai_drv = tfa98xx_dai;
	scnprintf(stream_name, MAX_CONTROL_NAME, "%s Playback",
		dev_name(&i2c->dev));
	dai_drv->playback.stream_name = kstrdup(stream_name, GFP_KERNEL);
	if (dai_drv->playback.stream_name == NULL) {
		ret = -ENOMEM;
		goto codec_fail;
	}

	ret = snd_soc_register_codec(&i2c->dev, &tfa98xx_soc_codec,
			dai_drv, 1);
	if (ret < 0) {
		pr_err("%s: Error registering tfa98xx codec", __func__);
		goto codec_fail;
	}

	ret = device_create_file(&i2c->dev, &dev_attr_monitor);
	if (ret)
		dev_info(&i2c->dev, "error creating monitor sysfs files\n");

	pr_debug("tfa98xx probed successfully!");

	return ret;

codec_fail:
	destroy_workqueue(tfa98xx->tfa98xx_wq);
wq_fail:
	snd_soc_unregister_codec(&i2c->dev);

	return ret;
}

static int tfa98xx_i2c_remove(struct i2c_client *client)
{
	struct tfa98xx *tfa98xx = i2c_get_clientdata(client);

	device_remove_file(&client->dev, &dev_attr_monitor);

#ifdef CONFIG_SND_SOC_TFA98XX_DEBUG
	device_remove_bin_file(&client->dev, &dev_attr_dspmsg);
	device_remove_bin_file(&client->dev, &dev_attr_reg);
	device_remove_bin_file(&client->dev, &dev_attr_rw);
	device_remove_file(&client->dev, &dev_attr_dspmsg_retries);
#endif
	snd_soc_unregister_codec(&client->dev);
	destroy_workqueue(tfa98xx->tfa98xx_wq);
	return 0;
}

static const struct i2c_device_id tfa98xx_i2c_id[] = {
	{ "tfa98xx", 0 },
#ifdef CONFIG_ACPI
	{ "i2c-NXP9890:00", 0},
	{ "i2c-NXP9890:01", 0},
#endif
	{ }
};
MODULE_DEVICE_TABLE(i2c, tfa98xx_i2c_id);

#ifdef CONFIG_OF
static struct of_device_id tfa98xx_match_tbl[] = {
	{ .compatible = "nxp,tfa98xx" },
	{ },
};
MODULE_DEVICE_TABLE(of, tfa98xx_match_tbl);
#endif

#ifdef CONFIG_ACPI
static struct acpi_device_id tfa98xx_acpi_match[] = {
	{ "TFA98xx", 0 },
	{ "NXP9890", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, tfa98xx_acpi_match);
#endif

static struct i2c_driver tfa98xx_i2c_driver = {
	.driver = {
		.name = "tfa98xx",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tfa98xx_match_tbl),
#endif
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(tfa98xx_acpi_match),
#endif
	},
	.probe =    tfa98xx_i2c_probe,
	.remove =   tfa98xx_i2c_remove,
	.id_table = tfa98xx_i2c_id,
};

module_i2c_driver(tfa98xx_i2c_driver);

MODULE_DESCRIPTION("ASoC tfa98xx codec driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("NXP");
