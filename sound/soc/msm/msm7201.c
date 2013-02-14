/* linux/sound/soc/msm/msm7201.c
 *
 * Copyright (c) 2008-2009, 2011, 2012 The Linux Foundation. All rights reserved.
 *
 * All source code in this file is licensed under the following license except
 * where indicated.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org.
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/msm_audio.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/tlv.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <asm/dma.h>
#include <linux/dma-mapping.h>

#include "msm-pcm.h"
#include <asm/mach-types.h>
#include <mach/msm_rpcrouter.h>

static struct msm_rpc_endpoint *snd_ep;
static uint32_t snd_mute_ear_mute;
static uint32_t snd_mute_mic_mute;

struct msm_snd_rpc_ids {
	unsigned long   prog;
	unsigned long   vers;
	unsigned long   rpc_set_snd_device;
	unsigned long	rpc_set_device_vol;
	struct cad_devices_type device;
};

struct rpc_cad_set_device_args {
	struct cad_devices_type device;
	uint32_t ear_mute;
	uint32_t mic_mute;

	uint32_t cb_func;
	uint32_t client_data;
};

struct rpc_cad_set_volume_args {
	struct cad_devices_type device;
	uint32_t method;
	uint32_t volume;

	uint32_t cb_func;
	uint32_t client_data;
};

static struct msm_snd_rpc_ids snd_rpc_ids;

static struct platform_device *msm_audio_snd_device;

static int snd_msm_volume_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1; /* Volume Param, in dB */
	uinfo->value.integer.min = MIN_DB;
	uinfo->value.integer.max = MAX_DB;
	return 0;
}

static int snd_msm_volume_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	spin_lock_irq(&the_locks.mixer_lock);
	ucontrol->value.integer.value[0] = msm_vol_ctl.volume;
	spin_unlock_irq(&the_locks.mixer_lock);
	return 0;
}

static int snd_msm_volume_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int change;
	int volume;

	volume = ucontrol->value.integer.value[0];
	spin_lock_irq(&the_locks.mixer_lock);
	change = (msm_vol_ctl.volume != volume);
	if (change) {
		msm_vol_ctl.volume = volume;
		msm_audio_volume_update(PCMPLAYBACK_DECODERID,
				msm_vol_ctl.volume, msm_vol_ctl.pan);
	}
	spin_unlock_irq(&the_locks.mixer_lock);
	return 0;
}

static int snd_msm_device_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 4; /* Device */

	/*
	 * The number of devices supported is 26 (0 to 25)
	 */
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 36;
	return 0;
}

static int snd_msm_device_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0]
		= (uint32_t)snd_rpc_ids.device.rx_device;
	ucontrol->value.integer.value[1]
		= (uint32_t)snd_rpc_ids.device.tx_device;
	ucontrol->value.integer.value[2] = snd_mute_ear_mute;
	ucontrol->value.integer.value[3] = snd_mute_mic_mute;
	return 0;
}

int msm_snd_init_rpc_ids(void)
{
	snd_rpc_ids.prog	= 0x30000002;
	snd_rpc_ids.vers	= 0x00030003;
	/*
	 * The magic number 2 corresponds to the rpc call
	 * index for snd_set_device
	 */
	snd_rpc_ids.rpc_set_snd_device = 40;
	snd_rpc_ids.rpc_set_device_vol = 39;
	return 0;
}

int msm_snd_rpc_connect(void)
{
	if (snd_ep) {
		printk(KERN_INFO "%s: snd_ep already connected\n", __func__);
		return 0;
	}

	/* Initialize rpc ids */
	if (msm_snd_init_rpc_ids()) {
		pr_err("%s: snd rpc ids initialization failed\n"
			, __func__);
		return -ENODATA;
	}

	snd_ep = msm_rpc_connect_compatible(snd_rpc_ids.prog,
				snd_rpc_ids.vers, 0);
	if (IS_ERR(snd_ep)) {
		pr_err("%s: failed (compatible VERS = %ld)\n",
				__func__, snd_rpc_ids.vers);
		snd_ep = NULL;
		return -EAGAIN;
	}
	return 0;
}

int msm_snd_rpc_close(void)
{
	int rc = 0;

	if (IS_ERR(snd_ep)) {
		pr_err("%s: snd handle unavailable, rc = %ld\n",
				__func__, PTR_ERR(snd_ep));
		return -EAGAIN;
	}

	rc = msm_rpc_close(snd_ep);
	snd_ep = NULL;

	if (rc < 0) {
		pr_err("%s: close rpc failed! rc = %d\n",
				__func__, rc);
		return -EAGAIN;
	} else
		printk(KERN_INFO "rpc close success\n");

	return rc;
}

static int snd_msm_device_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	struct snd_cad_set_device_msg {
		struct rpc_request_hdr hdr;
		struct rpc_cad_set_device_args args;
	} dmsg;

	snd_rpc_ids.device.rx_device
		= (int)ucontrol->value.integer.value[0];
	snd_rpc_ids.device.tx_device
		= (int)ucontrol->value.integer.value[1];
	snd_rpc_ids.device.pathtype = CAD_DEVICE_PATH_RX_TX;

	dmsg.args.device.rx_device
		= cpu_to_be32(snd_rpc_ids.device.rx_device);
	dmsg.args.device.tx_device
		= cpu_to_be32(snd_rpc_ids.device.tx_device);
	dmsg.args.device.pathtype = cpu_to_be32(CAD_DEVICE_PATH_RX_TX);
	dmsg.args.ear_mute = cpu_to_be32(ucontrol->value.integer.value[2]);
	dmsg.args.mic_mute = cpu_to_be32(ucontrol->value.integer.value[3]);
	if (!(dmsg.args.ear_mute == SND_MUTE_MUTED ||
		dmsg.args.ear_mute == SND_MUTE_UNMUTED) ||
		(!(dmsg.args.mic_mute == SND_MUTE_MUTED ||
		dmsg.args.ear_mute == SND_MUTE_UNMUTED))) {
		pr_err("snd_cad_ioctl set device: invalid mute status\n");
		rc = -EINVAL;
		return rc;
	}
	dmsg.args.cb_func = -1;
	dmsg.args.client_data = 0;

	rc = msm_rpc_call(snd_ep, snd_rpc_ids.rpc_set_snd_device ,
			&dmsg, sizeof(dmsg), 5 * HZ);

	if (rc < 0) {
		pr_err("%s: snd rpc call failed! rc = %d\n",
			__func__, rc);
	} else {
		printk(KERN_INFO "snd device connected\n");
		snd_mute_ear_mute = ucontrol->value.integer.value[2];
		snd_mute_mic_mute = ucontrol->value.integer.value[3];
		pr_err("%s: snd_mute_ear_mute =%d, snd_mute_mic_mute = %d\n",
				__func__, snd_mute_ear_mute, snd_mute_mic_mute);
	}

	return rc;
}

static int snd_msm_device_vol_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1; /* Device/Volume */

	/*
	 * The volume ranges from (0 to 6)
	 */
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 6;
	return 0;
}

static int snd_msm_device_vol_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;

	struct snd_cad_set_volume_msg {
		struct rpc_request_hdr hdr;
		struct rpc_cad_set_volume_args args;
	} vmsg;

	vmsg.args.device.rx_device
		= cpu_to_be32(snd_rpc_ids.device.rx_device);
	vmsg.args.device.tx_device
		= cpu_to_be32(snd_rpc_ids.device.tx_device);
	vmsg.args.method = cpu_to_be32(SND_METHOD_VOICE);
	vmsg.args.volume = cpu_to_be32(ucontrol->value.integer.value[0]);
	vmsg.args.cb_func = -1;
	vmsg.args.client_data = 0;

	rc = msm_rpc_call(snd_ep, snd_rpc_ids.rpc_set_device_vol ,
			&vmsg, sizeof(vmsg), 5 * HZ);

	if (rc < 0) {
		pr_err("%s: snd rpc call failed! rc = %d\n",
			__func__, rc);
	} else {
		pr_debug("%s:rx device [%d]", __func__,
			snd_rpc_ids.device.rx_device);
		pr_debug("%s:tx device [%d]", __func__,
			snd_rpc_ids.device.tx_device);
		pr_debug("%s:volume set to [%ld]\n", __func__,
			snd_rpc_ids.rpc_set_device_vol);
	}

	return rc;
}

/* Supported range -50dB to 18dB */
static const DECLARE_TLV_DB_LINEAR(db_scale_linear, -5000, 1800);

#define MSM_EXT(xname, xindex, fp_info, fp_get, fp_put, addr) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
  .name = xname, .index = xindex, \
  .info = fp_info,\
  .get = fp_get, .put = fp_put, \
  .private_value = addr, \
}

#define MSM_EXT_TLV(xname, xindex, fp_info, fp_get, fp_put, addr, tlv_array) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .access = (SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
		SNDRV_CTL_ELEM_ACCESS_READWRITE), \
  .name = xname, .index = xindex, \
  .info = fp_info,\
  .get = fp_get, .put = fp_put, .tlv.p = tlv_array, \
  .private_value = addr, \
}

static struct snd_kcontrol_new snd_msm_controls[] = {
	MSM_EXT_TLV("PCM Playback Volume", 0, snd_msm_volume_info, \
	snd_msm_volume_get, snd_msm_volume_put, 0, db_scale_linear),
	MSM_EXT("device", 0, snd_msm_device_info, snd_msm_device_get, \
						 snd_msm_device_put, 0),
	MSM_EXT("Device Volume", 0, snd_msm_device_vol_info, NULL, \
						 snd_msm_device_vol_put, 0),
};

static int msm_new_mixer(struct snd_soc_codec *codec)
{
	unsigned int idx;
	int err;

	pr_err("msm_soc: ALSA MSM Mixer Setting\n");
	strcpy(codec->card->snd_card->mixername, "MSM Mixer");
	for (idx = 0; idx < ARRAY_SIZE(snd_msm_controls); idx++) {
		err = snd_ctl_add(codec->card->snd_card,
				snd_ctl_new1(&snd_msm_controls[idx], NULL));
		if (err < 0)
			return err;
	}
	return 0;
}

static int msm_soc_dai_init(
	struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_soc_codec *codec = rtd->codec;

	mutex_init(&the_locks.lock);
	mutex_init(&the_locks.write_lock);
	mutex_init(&the_locks.read_lock);
	spin_lock_init(&the_locks.read_dsp_lock);
	spin_lock_init(&the_locks.write_dsp_lock);
	spin_lock_init(&the_locks.mixer_lock);
	init_waitqueue_head(&the_locks.eos_wait);
	init_waitqueue_head(&the_locks.write_wait);
	init_waitqueue_head(&the_locks.read_wait);
	msm_vol_ctl.volume = MSM_PLAYBACK_DEFAULT_VOLUME;
	msm_vol_ctl.pan = MSM_PLAYBACK_DEFAULT_PAN;

	ret = msm_new_mixer(codec);
	if (ret < 0) {
		pr_err("msm_soc: ALSA MSM Mixer Fail\n");
	}

	return ret;
}

static struct snd_soc_dai_link msm_dai[] = {
{
	.name = "MSM Primary I2S",
	.stream_name = "DSP 1",
	.cpu_dai_name = "msm-cpu-dai.0",
	.platform_name = "msm-dsp-audio.0",
	.codec_name = "msm-codec-dai.0",
	.codec_dai_name = "msm-codec-dai",
	.init   = &msm_soc_dai_init,
},
};

static struct snd_soc_card snd_soc_card_msm = {
	.name		= "msm-audio",
	.dai_link	= msm_dai,
	.num_links = ARRAY_SIZE(msm_dai),
};

static int __init msm_audio_init(void)
{
	int ret;

	msm_audio_snd_device = platform_device_alloc("soc-audio", -1);
	if (!msm_audio_snd_device)
		return -ENOMEM;

	platform_set_drvdata(msm_audio_snd_device, &snd_soc_card_msm);
	ret = platform_device_add(msm_audio_snd_device);
	if (ret) {
		platform_device_put(msm_audio_snd_device);
		return ret;
	}

	ret = msm_snd_rpc_connect();
	snd_mute_ear_mute = 0;
	snd_mute_mic_mute = 0;

	return ret;
}

static void __exit msm_audio_exit(void)
{
	msm_snd_rpc_close();
	platform_device_unregister(msm_audio_snd_device);
}

module_init(msm_audio_init);
module_exit(msm_audio_exit);

MODULE_DESCRIPTION("PCM module");
MODULE_LICENSE("GPL v2");
