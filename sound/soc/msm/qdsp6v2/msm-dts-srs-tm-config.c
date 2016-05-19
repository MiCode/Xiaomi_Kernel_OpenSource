/* Copyright (c) 2012-2014, 2016, The Linux Foundation. All rights reserved.
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

#include <linux/err.h>
#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/msm_audio_ion.h>
#include <sound/control.h>
#include <sound/q6adm-v2.h>
#include <sound/asound.h>
#include <sound/msm-dts-eagle.h>
#include "msm-dts-srs-tm-config.h"
#include "msm-pcm-routing-v2.h"

static int srs_port_id[AFE_MAX_PORTS] = {-1};
static int srs_copp_idx[AFE_MAX_PORTS] = {-1};
static union srs_trumedia_params_u msm_srs_trumedia_params;
static struct ion_client *ion_client;
static struct ion_handle *ion_handle;
static struct param_outband po;
static atomic_t ref_cnt;
#define ION_MEM_SIZE	(8 * 1024)

static int set_port_id(int port_id, int copp_idx)
{
	int index = adm_validate_and_get_port_index(port_id);
	if (index < 0) {
		pr_err("%s: Invalid port idx %d port_id %#x\n", __func__, index,
			port_id);
		return -EINVAL;
	}
	srs_port_id[index] = port_id;
	srs_copp_idx[index] = copp_idx;
	return 0;
}

static void msm_dts_srs_tm_send_params(__s32 port_id, __u32 techs)
{
	__s32 index = adm_validate_and_get_port_index(port_id);
	if (index < 0) {
		pr_err("%s: Invalid port idx %d port_id 0x%x\n",
			__func__, index, port_id);
		return;
	}
	if ((srs_copp_idx[index] < 0) ||
	    (srs_copp_idx[index] >= MAX_COPPS_PER_PORT)) {
		pr_debug("%s: send params called before copp open. so, caching\n",
			 __func__);
		return;
	}
	pr_debug("SRS %s: called, port_id = %d, techs flags = %u\n",
		__func__, port_id, techs);
	/* force all if techs is set to 1 */
	if (techs == 1)
		techs = 0xFFFFFFFF;

	if (techs & (1 << SRS_ID_WOWHD))
		srs_trumedia_open(port_id, srs_copp_idx[index], SRS_ID_WOWHD,
			(void *)&msm_srs_trumedia_params.srs_params.wowhd);
	if (techs & (1 << SRS_ID_CSHP))
		srs_trumedia_open(port_id, srs_copp_idx[index], SRS_ID_CSHP,
			(void *)&msm_srs_trumedia_params.srs_params.cshp);
	if (techs & (1 << SRS_ID_HPF))
		srs_trumedia_open(port_id, srs_copp_idx[index], SRS_ID_HPF,
			(void *)&msm_srs_trumedia_params.srs_params.hpf);
	if (techs & (1 << SRS_ID_AEQ))
		srs_trumedia_open(port_id, srs_copp_idx[index], SRS_ID_AEQ,
			(void *)&msm_srs_trumedia_params.srs_params.aeq);
	if (techs & (1 << SRS_ID_HL))
		srs_trumedia_open(port_id, srs_copp_idx[index], SRS_ID_HL,
			(void *)&msm_srs_trumedia_params.srs_params.hl);
	if (techs & (1 << SRS_ID_GEQ))
		srs_trumedia_open(port_id, srs_copp_idx[index], SRS_ID_GEQ,
			(void *)&msm_srs_trumedia_params.srs_params.geq);
	if (techs & (1 << SRS_ID_GLOBAL))
		srs_trumedia_open(port_id, srs_copp_idx[index], SRS_ID_GLOBAL,
			(void *)&msm_srs_trumedia_params.srs_params.global);
}


static int msm_dts_srs_trumedia_control_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int msm_dts_srs_trumedia_control_set_(int port_id,
					    struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{

	__u16 offset, value, max = sizeof(msm_srs_trumedia_params) >> 1;
	if (SRS_CMD_UPLOAD ==
		(ucontrol->value.integer.value[0] & SRS_CMD_UPLOAD)) {
		__u32 techs = ucontrol->value.integer.value[0] & 0xFF;
		__s32 index = adm_validate_and_get_port_index(port_id);
		if (index < 0) {
			pr_err("%s: Invalid port idx %d port_id 0x%x\n",
					__func__, index, port_id);
				return -EINVAL;
			}
		pr_debug("SRS %s: send params request, flag = %u\n",
					__func__, techs);
		if (srs_port_id[index] >= 0 && techs)
			msm_dts_srs_tm_send_params(port_id, techs);
		return 0;
	}
	offset = (__u16)((ucontrol->value.integer.value[0] &
			SRS_PARAM_OFFSET_MASK) >> 16);
	value = (__u16)(ucontrol->value.integer.value[0] &
			SRS_PARAM_VALUE_MASK);
	if (offset < max) {
		msm_srs_trumedia_params.raw_params[offset] = value;
		pr_debug("SRS %s: index set... (max %d, requested %d, value 0x%X)\n",
			 __func__, max, offset, value);
	} else {
		pr_err("SRS %s: index out of bounds! (max %d, requested %d)\n",
		       __func__, max, offset);
	}
	return 0;
}

static int msm_dts_srs_trumedia_control_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int ret, port_id;

	pr_debug("SRS control normal called\n");
	msm_pcm_routing_acquire_lock();
	port_id = SLIMBUS_0_RX;
	ret = msm_dts_srs_trumedia_control_set_(port_id, kcontrol, ucontrol);
	msm_pcm_routing_release_lock();
	return ret;
}

static int msm_dts_srs_trumedia_control_i2s_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int ret, port_id;

	pr_debug("SRS control I2S called\n");
	msm_pcm_routing_acquire_lock();
	port_id = PRIMARY_I2S_RX;
	ret = msm_dts_srs_trumedia_control_set_(port_id, kcontrol, ucontrol);
	msm_pcm_routing_release_lock();
	return ret;
}

static int msm_dts_srs_trumedia_control_mi2s_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int ret, port_id;

	pr_debug("SRS control MI2S called\n");
	msm_pcm_routing_acquire_lock();
	port_id = AFE_PORT_ID_PRIMARY_MI2S_RX;
	ret = msm_dts_srs_trumedia_control_set_(port_id, kcontrol, ucontrol);
	msm_pcm_routing_release_lock();
	return ret;
}

static int msm_dts_srs_trumedia_control_hdmi_set(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	int ret, port_id;

	pr_debug("SRS control HDMI called\n");
	msm_pcm_routing_acquire_lock();
	port_id = HDMI_RX;
	ret = msm_dts_srs_trumedia_control_set_(port_id, kcontrol, ucontrol);
	msm_pcm_routing_release_lock();
	return ret;
}

static const struct snd_kcontrol_new lpa_srs_trumedia_controls[] = {
	{.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "SRS TruMedia",
	.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |
			SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.info = snd_soc_info_volsw,
	.get = msm_dts_srs_trumedia_control_get,
	.put = msm_dts_srs_trumedia_control_set,
	.private_value = ((unsigned long)&(struct soc_mixer_control)
	{.reg = SND_SOC_NOPM,
	.rreg = SND_SOC_NOPM,
	.shift = 0,
	.rshift = 0,
	.max = 0xFFFFFFFF,
	.platform_max = 0xFFFFFFFF,
	.invert = 0
	})
	}
};

static const struct snd_kcontrol_new lpa_srs_trumedia_controls_hdmi[] = {
	{.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "SRS TruMedia HDMI",
	.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |
			SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.info = snd_soc_info_volsw,
	.get = msm_dts_srs_trumedia_control_get,
	.put = msm_dts_srs_trumedia_control_hdmi_set,
	.private_value = ((unsigned long)&(struct soc_mixer_control)
	{.reg = SND_SOC_NOPM,
	.rreg = SND_SOC_NOPM,
	.shift = 0,
	.rshift = 0,
	.max = 0xFFFFFFFF,
	.platform_max = 0xFFFFFFFF,
	.invert = 0
	})
	}
};

static const struct snd_kcontrol_new lpa_srs_trumedia_controls_i2s[] = {
	{.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "SRS TruMedia I2S",
	.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |
		SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.info = snd_soc_info_volsw,
	.get = msm_dts_srs_trumedia_control_get,
	.put = msm_dts_srs_trumedia_control_i2s_set,
	.private_value = ((unsigned long)&(struct soc_mixer_control)
	{.reg = SND_SOC_NOPM,
	.rreg = SND_SOC_NOPM,
	.shift = 0,
	.rshift = 0,
	.max = 0xFFFFFFFF,
	.platform_max = 0xFFFFFFFF,
	.invert = 0
	})
	}
};

static const struct snd_kcontrol_new lpa_srs_trumedia_controls_mi2s[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "SRS TruMedia MI2S",
		.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |
			SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = snd_soc_info_volsw,
		.get = msm_dts_srs_trumedia_control_get,
		.put = msm_dts_srs_trumedia_control_mi2s_set,
		.private_value = ((unsigned long)&(struct soc_mixer_control)
		{
			.reg = SND_SOC_NOPM,
			.rreg = SND_SOC_NOPM,
			.shift = 0,
			.rshift = 0,
			.max = 0xFFFFFFFF,
			.platform_max = 0xFFFFFFFF,
			.invert = 0
		})
	}
};

void msm_dts_srs_tm_add_controls(struct snd_soc_platform *platform)
{
	snd_soc_add_platform_controls(platform,
				lpa_srs_trumedia_controls,
			ARRAY_SIZE(lpa_srs_trumedia_controls));

	snd_soc_add_platform_controls(platform,
				lpa_srs_trumedia_controls_hdmi,
			ARRAY_SIZE(lpa_srs_trumedia_controls_hdmi));

	snd_soc_add_platform_controls(platform,
				lpa_srs_trumedia_controls_i2s,
			ARRAY_SIZE(lpa_srs_trumedia_controls_i2s));
	snd_soc_add_platform_controls(platform,
				lpa_srs_trumedia_controls_mi2s,
			ARRAY_SIZE(lpa_srs_trumedia_controls_mi2s));
}

static int reg_ion_mem(void)
{
	int rc;
	rc = msm_audio_ion_alloc("SRS_TRUMEDIA", &ion_client, &ion_handle,
				 ION_MEM_SIZE, &po.paddr, (size_t *)&po.size,
				 &po.kvaddr);
	if (rc != 0)
		pr_err("%s: failed to allocate memory.\n", __func__);
		pr_debug("%s: exited ion_client = %pK, ion_handle = %pK, phys_addr = %lu, length = %d, vaddr = %pK, rc = 0x%x\n",
			__func__, ion_client, ion_handle, (long)po.paddr,
			(unsigned int)po.size, po.kvaddr, rc);
	return rc;
}

void msm_dts_srs_tm_ion_memmap(struct param_outband *po_)
{
	if (po.kvaddr == NULL) {
		pr_debug("%s: callingreg_ion_mem()\n", __func__);
		reg_ion_mem();
	}
	po_->size = ION_MEM_SIZE;
	po_->kvaddr = po.kvaddr;
	po_->paddr = po.paddr;
}

static void unreg_ion_mem(void)
{
	msm_audio_ion_free(ion_client, ion_handle);
	po.kvaddr = NULL;
	po.paddr = 0;
	po.size = 0;
}

void msm_dts_srs_tm_deinit(int port_id)
{
	set_port_id(port_id, -1);
	atomic_dec(&ref_cnt);
	if (po.kvaddr != NULL) {
		if (!atomic_read(&ref_cnt)) {
			pr_debug("%s: calling unreg_ion_mem()\n", __func__);
			unreg_ion_mem();
		}
	}
	return;
}

void msm_dts_srs_tm_init(int port_id, int copp_idx)
{
	int cur_ref_cnt = 0;

	if (set_port_id(port_id, copp_idx) < 0) {
		pr_err("%s: Invalid port_id: %d\n", __func__, port_id);
		return;
	}

	cur_ref_cnt = atomic_read(&ref_cnt);
	atomic_inc(&ref_cnt);
	if (!cur_ref_cnt && po.kvaddr == NULL) {
		pr_debug("%s: calling reg_ion_mem()\n", __func__);
		if (reg_ion_mem() != 0) {
			atomic_dec(&ref_cnt);
			po.kvaddr = NULL;
			return;
		}
	}
	msm_dts_srs_tm_send_params(port_id, 1);
	return;
}
