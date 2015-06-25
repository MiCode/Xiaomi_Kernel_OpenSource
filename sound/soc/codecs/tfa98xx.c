/*
 *                  Copyright (c), NXP Semiconductors
 *
 *                     (C)NXP Semiconductors
 *
 * NXP reserves the right to make changes without notice at any time.
 * This code is distributed in the hope that it will be useful,
 * but NXP makes NO WARRANTY, expressed, implied or statutory, including but
 * not limited to any implied warranty of MERCHANTABILITY or FITNESS FOR ANY
 * PARTICULAR PURPOSE, or that the use will not infringe any third party patent,
 * copyright or trademark. NXP must not be liable for any loss or damage
 * arising from its use. (c) PLMA, NXP Semiconductors.
 */

#define DEBUG
#define pr_fmt(fmt) "%s(%s): " fmt, __func__, tfa98xx->fw.name
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>
#include <sound/tfa98xx.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "tfa98xx-core.h"
#include "tfa98xx_regs.h"
#include "tfa_container.h"
#include "tfa_dsp.h"

#define I2C_RETRY_DELAY		5 /* ms */
#define I2C_RETRIES		    5
#define PLL_SYNC_RETRIES	10
#define MTPB_RETRIES		5

/* SNDRV_PCM_RATE_KNOT -> 12000, 24000 Hz, limit with constraint list */
#define TFA98XX_RATES (SNDRV_PCM_RATE_8000_48000 | SNDRV_PCM_RATE_KNOT)
#define TFA98XX_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE)

#define FIRMWARE_NAME_SIZE     128
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

int tfa98xx_bulk_write_raw(struct snd_soc_codec *codec, const u8 *data, u8 count)
{
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	int ret;

    /*
	pr_debug("%s() %d\n", __func__, count);
	{
		int i;
		for (i=0; i < count; i++)
			pr_debug(" 0x%x", *((u8*)data+i));
	}
      */
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

int tfa98xx_bulk_write(struct snd_soc_codec *codec, unsigned int reg,
				const void *data, size_t len)
{
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	u8 chunk_buf[TFA98XX_MAX_I2C_SIZE + 1];
	int offset = 0;
	int ret = 0;
	/* first byte is mem address */
	int remaining_bytes = len;
	int chunk_size = TFA98XX_MAX_I2C_SIZE;

    //pr_debug("%s() %x %d\n", __func__, reg, len);

	chunk_buf[0] = reg & 0xff;

	mutex_lock(&tfa98xx->i2c_rw_lock);

	while ((remaining_bytes > 0)) {
		if (remaining_bytes < chunk_size)
			chunk_size = remaining_bytes;

		memcpy(chunk_buf + 1, data + offset, chunk_size);
		ret = tfa98xx_bulk_write_raw(codec, chunk_buf, chunk_size + 1);
		offset = offset + chunk_size;
		remaining_bytes = remaining_bytes - chunk_size;
	}

	mutex_unlock(&tfa98xx->i2c_rw_lock);

	return ret;
}

/*
 * ASOC controls
 */

static const struct snd_soc_dapm_widget tfa98xx_dapm_widgets[] = {
	SND_SOC_DAPM_MIXER("NXP Output Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
};

static const struct snd_soc_dapm_route tfa98xx_dapm_routes[] = {
	{"NXP Output Mixer", NULL, "Playback"},
};

static void tfa98xx_monitor(struct work_struct *work)
{
	struct tfa98xx *tfa98xx = container_of(work, struct tfa98xx, delay_work.work);
	u16 status;

    //pr_debug("%s()\n", __func__);

	mutex_lock(&tfa98xx->dsp_init_lock);

	/*
	 * first check if the device is power up
	 * check IC status bits: cold start, amp switching, speaker error
	 * and DSP watch dog bit to re init
	 */
	if (!tfaRunIsPwdn(tfa98xx)){
	status = snd_soc_read(tfa98xx->codec, TFA98XX_STATUSREG);

         /* This one is not realy a DSP crash and should just generate a warning */
		if (!(TFA98XX_STATUSREG_SWS & status))
		pr_err("ERROR: AMP_SWS\n");

		if (TFA98XX_STATUSREG_SPKS & status)
	        pr_err("ERROR: SPKS\n");

	pr_debug("SYS_STATUS: 0x%04x\n", status);
	if ((TFA98XX_STATUSREG_ACS & status) ||
	    (TFA98XX_STATUSREG_WDS & status)) {

		tfa98xx->dsp_init = TFA98XX_DSP_INIT_RECOVER;

		if (TFA98XX_STATUSREG_ACS & status)
			pr_err("ERROR: ACS\n");
		if (TFA98XX_STATUSREG_WDS & status)
			pr_err("ERROR: WDS\n");

		/* schedule init now if the clocks are up and stable */
		if ((status & TFA98XX_STATUS_UP_MASK) == TFA98XX_STATUS_UP_MASK)
			queue_work(tfa98xx->tfa98xx_wq, &tfa98xx->init_work);
	}

	/* else just reschedule */
	queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->delay_work, 5*HZ);
	}
	mutex_unlock(&tfa98xx->dsp_init_lock);
}


static void tfa98xx_dsp_init(struct work_struct *work)
{
	struct tfa98xx *tfa98xx = container_of(work, struct tfa98xx, init_work);
    int tries = 0;
#define COLD_STARTUP_TRY_COUNT (3)

	pr_debug("%s()\n", __func__);

	mutex_lock(&tfa98xx->dsp_init_lock);
	if (tfa98xx->dsp_init == TFA98XX_DSP_INIT_RECOVER){
		pr_err("Error detected by monitor, need recover");
		if (!tfaRunSpeakerBoost(tfa98xx, 1)){
		   tfa98xx->dsp_init = TFA98XX_DSP_INIT_DONE;
	}else {
	    tfa98xx->dsp_init = TFA98XX_DSP_INIT_PENDING;
            pr_err("Force speaker boost failed");
		}
	}
	else
	{
	    /* start the DSP using the latest profile / vstep */
		do {
			if (!tfaRunstart(tfa98xx, tfa98xx->profile, tfa98xx->vstep)){
	             tfa98xx->dsp_init = TFA98XX_DSP_INIT_DONE;
				 break;
	        }
			pr_err("Force speaker boost failed, tries = %d\n",tries);
		    tries++;
		}while (tries < COLD_STARTUP_TRY_COUNT);
		if (tries >= COLD_STARTUP_TRY_COUNT) {
		   pr_err("Tried tfaRunstart failed\n");
		}
	}

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

static int tfa98xx_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);

	pr_debug("state: %d\n", mute);

	mutex_lock(&tfa98xx->dsp_init_lock);
	if (mute) {
		cancel_delayed_work_sync(&tfa98xx->delay_work);

		tfaRunstop(tfa98xx);
	} else {
		/*
		 * start monitor thread to check IC status bit 5secs, and
		 * re-init IC to recover.
		 */
		queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->delay_work, HZ);
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
		queue_work(tfa98xx->tfa98xx_wq, &tfa98xx->init_work);
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
 * SysFS support
 */

/*
 * Helpers for profile selction controls
 */
int tfa98xx_get_profile(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tfa98xx->profile;

//	pr_debug("%s: profile %d\n", tfa98xx->fw.name, tfa98xx->profile);

	return 0;
}

int tfa98xx_set_profile(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);

	if (tfa98xx->profile == ucontrol->value.integer.value[0])
		return 0;

	tfa98xx->profile = ucontrol->value.integer.value[0];

	pr_debug("%s: profile %d\n", tfa98xx->fw.name, tfa98xx->profile);

	return 0;
}

int tfa98xx_info_profile(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_info *uinfo)
{
	struct tfaprofile *profiles = (struct tfaprofile *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = tfa98xx->profile_count;

//	pr_debug("%s: profile %d / %d\n", tfa98xx->fw.name, tfa98xx->profile, tfa98xx->profile_count);

	if (uinfo->value.enumerated.item > tfa98xx->profile_count - 1)
		uinfo->value.enumerated.item = tfa98xx->profile_count - 1;

	strcpy(uinfo->value.enumerated.name,
		profiles[uinfo->value.enumerated.item].name);

	return 0;
}

/*
 * Helpers for volume through vstep controls
 */
int tfa98xx_get_vol(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct tfaprofile *profiles = (struct tfaprofile *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	int index = tfa98xx->profile;
	struct tfaprofile *prof = &profiles[index];


	ucontrol->value.integer.value[0] = prof->vsteps - prof->vstep - 1;

	pr_debug("%s: %d/%d\n", prof->name, prof->vstep, prof->vsteps - 1);

	return 0;
}

int tfa98xx_set_vol(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct tfaprofile *profiles = (struct tfaprofile *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	int index = tfa98xx->profile;
	struct tfaprofile *prof = &profiles[index];

	if (prof->vstep == prof->vsteps - ucontrol->value.integer.value[0] - 1)
		return 0;


	prof->vstep = prof->vsteps - ucontrol->value.integer.value[0] - 1;

	if (prof->vstep < 0)
		prof->vstep = 0;

	pr_debug("%s: %d/%d\n", prof->name, prof->vstep, prof->vsteps - 1);

	return 1;
}

int tfa98xx_info_vol(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_info *uinfo)
{
	struct tfaprofile *profiles = (struct tfaprofile *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	struct tfaprofile *prof = &profiles[tfa98xx->profile];

	pr_debug("%s [0..%d]\n", prof->name, prof->vsteps - 1);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = prof->vsteps - 1;

	return 0;
}

#define MAX_CONTROL_NAME	32

char prof_name[MAX_CONTROL_NAME];
char vol_name[MAX_CONTROL_NAME];

static struct snd_kcontrol_new tfa98xx_controls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = prof_name,
		.info = tfa98xx_info_profile,
		.get = tfa98xx_get_profile,
		.put = tfa98xx_set_profile,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = vol_name,
		.info = tfa98xx_info_vol,
		.get = tfa98xx_get_vol,
		.put = tfa98xx_set_vol,
	}
};


static const struct snd_soc_dai_ops tfa98xx_ops = {
	.hw_params	= tfa98xx_hw_params,
	.digital_mute	= tfa98xx_mute,
	.set_fmt	= tfa98xx_set_dai_fmt,
	.set_sysclk	= tfa98xx_set_dai_sysclk,
	.startup	= tfa98xx_startup,
	.shutdown	= tfa98xx_shutdown,
	.trigger	= tfa98xx_trigger,
};

static struct snd_soc_dai_driver tfa98xx_dai = {
	.name = "tfa98xx_codec",
	.playback = {
		     .stream_name = "Playback",
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

	codec->control_data = tfa98xx->regmap;
	tfa98xx->codec = codec;
	codec->cache_bypass = true;

	ret = snd_soc_codec_set_cache_io(codec, 8, 16, SND_SOC_REGMAP);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}
	tfa98xx->rev = snd_soc_read(codec, TFA98XX_REVISIONNUMBER);
	dev_info(codec->dev, "ID revision 0x%04x\n", tfa98xx->rev);
	tfa98xx->rev = snd_soc_read(codec, TFA98XX_REVISIONNUMBER);
	dev_info(codec->dev, "ID revision 0x%04x\n", tfa98xx->rev);

	snd_soc_dapm_new_controls(&codec->dapm, tfa98xx_dapm_widgets,
				  ARRAY_SIZE(tfa98xx_dapm_widgets));

	snd_soc_dapm_add_routes(&codec->dapm, tfa98xx_dapm_routes,
				ARRAY_SIZE(tfa98xx_dapm_routes));

	snd_soc_dapm_new_widgets(&codec->dapm);
	snd_soc_dapm_sync(&codec->dapm);

	ret = tfa98xx_cnt_loadfile(tfa98xx, 0);
	if(ret)
		return ret;

	tfa98xx->profile_current = -1;

	/* Overwrite kcontrol values that need container information */
	tfa98xx_controls[0].private_value = (unsigned long)tfa98xx->profiles;
	tfa98xx_controls[1].private_value = (unsigned long)tfa98xx->profiles;
	scnprintf(prof_name, MAX_CONTROL_NAME, "%s Profile", tfa98xx->fw.name);
	scnprintf(vol_name, MAX_CONTROL_NAME, "%s Master Volume", tfa98xx->fw.name);

	snd_soc_add_codec_controls(codec, tfa98xx_controls, ARRAY_SIZE(tfa98xx_controls));

	dev_info(codec->dev, "tfa98xx codec registered");

	return 0;
}

static int tfa98xx_remove(struct snd_soc_codec *codec)
{
	dev_info(codec->dev, "tfa98xx codec removed");
	return 0;
}

static struct snd_soc_codec_driver tfa98xx_soc_codec = {
	.probe = tfa98xx_probe,
	.remove = tfa98xx_remove,
};

static const struct regmap_config tfa98xx_regmap = {
	.reg_bits = 8,
	.val_bits = 16,

	.max_register = TFA98XX_MAX_REGISTER,
	.cache_type = REGCACHE_RBTREE,
};

static int tfa98xx_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct tfa98xx *tfa98xx;
	int ret;

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
	mutex_init(&tfa98xx->i2c_rw_lock);

    // customer's option
#if 0
	/* enable regulator */
	tfa98xx->vdd = regulator_get(&i2c->dev, "tfa_vdd");
	if (IS_ERR(tfa98xx->vdd)) {
		pr_err("%s() Error getting vdd regulator.\n", __func__);
		ret = PTR_ERR(tfa98xx->vdd);
		goto reg_get_fail;
	}

	regulator_set_voltage(tfa98xx->vdd, 1800000, 1800000);

	ret = regulator_enable(tfa98xx->vdd);
	if (ret < 0) {
		pr_err("%s() Error enabling vdd regulator %d:", __func__, ret);
		goto reg_enable_fail;
	}
#endif

	/* setup work queue, will be used to initial DSP on first boot up */
	tfa98xx->tfa98xx_wq = create_singlethread_workqueue("tfa98xx");
	if (tfa98xx->tfa98xx_wq == NULL) {
		ret = -ENOMEM;
		goto wq_fail;
	}

	INIT_WORK(&tfa98xx->init_work, tfa98xx_dsp_init);
	INIT_DELAYED_WORK(&tfa98xx->delay_work, tfa98xx_monitor);

	/* register codec */
	ret = snd_soc_register_codec(&i2c->dev, &tfa98xx_soc_codec,
				     &tfa98xx_dai, 1);
	if (ret < 0) {
		pr_err("%s: Error registering tfa98xx codec", __func__);
		goto codec_fail;
	}

	pr_info("tfa98xx probed successfully!");

	return ret;

codec_fail:
	destroy_workqueue(tfa98xx->tfa98xx_wq);
wq_fail:
	snd_soc_unregister_codec(&i2c->dev);
// customer's option
#if 0
reg_enable_fail:
	regulator_disable(tfa98xx->vdd);
	regulator_put(tfa98xx->vdd);

reg_get_fail:
#endif
	return ret;
}

static int tfa98xx_i2c_remove(struct i2c_client *client)
{
	struct tfa98xx *tfa98xx = i2c_get_clientdata(client);

	snd_soc_unregister_codec(&client->dev);
#if 1
	regulator_disable(tfa98xx->vdd);
	regulator_put(tfa98xx->vdd);
#endif

	destroy_workqueue(tfa98xx->tfa98xx_wq);
	return 0;
}

static const struct i2c_device_id tfa98xx_i2c_id[] = {
	{ "tfa98xx", 0 },
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

static struct i2c_driver tfa98xx_i2c_driver = {
	.driver = {
		.name = "tfa98xx",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tfa98xx_match_tbl),
	},
	.probe =    tfa98xx_i2c_probe,
	.remove =   tfa98xx_i2c_remove,
	.id_table = tfa98xx_i2c_id,
};

module_i2c_driver(tfa98xx_i2c_driver);

MODULE_DESCRIPTION("ASoC tfa98xx codec driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("NXP");
