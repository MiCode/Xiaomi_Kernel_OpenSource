/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/clk/msm-clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/apr_audio-v2.h>
#include <sound/q6afe-v2.h>
#include <sound/msm-dai-q6-v2.h>
#include <linux/qdsp6v2/audio-anc-dev-mgr.h>
#include <linux/qdsp6v2/sdsp_anc.h>

#define LPM_START_ADDR      (0x9120000 + 60*1024)
#define LPM_LENGTH          (4*1024)

enum {
	ANC_DEV_PORT_REFS = 0,
	ANC_DEV_PORT_ANC_SPKR,
	ANC_DEV_PORT_ANC_MIC,
	ANC_DEV_PORT_MAX,
};

struct anc_tdm_port_cfg_info {
	u16 port_id;
	struct afe_param_id_tdm_cfg port_cfg;
};

struct anc_tdm_group_set_info {
	struct afe_param_id_group_device_tdm_cfg gp_cfg;
	uint32_t num_tdm_group_ports;
	struct afe_clk_set tdm_clk_set;
	uint32_t clk_mode;
};

struct anc_dev_drv_info {
	uint32_t state;
	uint32_t algo_module_id;
};

struct anc_dev_port_cfg_info {
	uint32_t port_id;
	uint32_t sample_rate;
	uint32_t num_channels;
	uint32_t bit_width;
};

static struct aud_msvc_param_id_dev_anc_mic_spkr_layout_info
			anc_mic_spkr_layout;

static struct anc_dev_port_cfg_info anc_port_cfg[ANC_DEV_PORT_MAX];

static struct anc_tdm_group_set_info anc_dev_tdm_gp_set[IDX_GROUP_TDM_MAX];

static struct anc_tdm_port_cfg_info anc_dev_tdm_port_cfg[IDX_TDM_MAX];

static struct anc_dev_drv_info this_anc_dev_info;

static int anc_dev_get_free_tdm_gp_cfg_idx(void)
{
	int idx = -1;
	int i;

	for (i = 0; i < IDX_GROUP_TDM_MAX; i++) {
		if (anc_dev_tdm_gp_set[i].gp_cfg.group_id == 0) {
			idx = i;
			break;
		}
	}

	return idx;
}

static int anc_dev_get_free_tdm_port_cfg_idx(void)
{
	int idx = -1;
	int i;

	for (i = 0; i < IDX_TDM_MAX; i++) {
		if (anc_dev_tdm_port_cfg[i].port_id == 0) {
			idx = i;
			break;
		}
	}

	return idx;
}

static u16 get_group_id_from_port_id(int32_t port_id)
{
	u16 gp_id = AFE_PORT_INVALID;

	switch (port_id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
	case AFE_PORT_ID_PRIMARY_TDM_RX_4:
	case AFE_PORT_ID_PRIMARY_TDM_RX_5:
	case AFE_PORT_ID_PRIMARY_TDM_RX_6:
	case AFE_PORT_ID_PRIMARY_TDM_RX_7:
		gp_id = AFE_GROUP_DEVICE_ID_PRIMARY_TDM_RX;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
	case AFE_PORT_ID_SECONDARY_TDM_RX_1:
	case AFE_PORT_ID_SECONDARY_TDM_RX_2:
	case AFE_PORT_ID_SECONDARY_TDM_RX_3:
	case AFE_PORT_ID_SECONDARY_TDM_RX_4:
	case AFE_PORT_ID_SECONDARY_TDM_RX_5:
	case AFE_PORT_ID_SECONDARY_TDM_RX_6:
	case AFE_PORT_ID_SECONDARY_TDM_RX_7:
		gp_id = AFE_GROUP_DEVICE_ID_SECONDARY_TDM_RX;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX:
	case AFE_PORT_ID_TERTIARY_TDM_RX_1:
	case AFE_PORT_ID_TERTIARY_TDM_RX_2:
	case AFE_PORT_ID_TERTIARY_TDM_RX_3:
	case AFE_PORT_ID_TERTIARY_TDM_RX_4:
	case AFE_PORT_ID_TERTIARY_TDM_RX_5:
	case AFE_PORT_ID_TERTIARY_TDM_RX_6:
	case AFE_PORT_ID_TERTIARY_TDM_RX_7:
		gp_id = AFE_GROUP_DEVICE_ID_TERTIARY_TDM_RX;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_1:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_2:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_3:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_4:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_5:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_6:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_7:
		gp_id = AFE_GROUP_DEVICE_ID_QUATERNARY_TDM_RX;
		break;
	default:
		break;
	}

	return gp_id;
}

static int anc_dev_get_matched_tdm_gp_cfg_idx(u16 gp_id)
{
	int idx = -1;
	int i;

	for (i = 0; i < IDX_GROUP_TDM_MAX; i++) {
		if (anc_dev_tdm_gp_set[i].gp_cfg.group_id == gp_id) {
			idx = i;
			break;
		}
	}

	return idx;
}

static int anc_dev_get_matched_tdm_port_cfg_idx(u16 port_id)
{
	int idx = -1;
	int i;

	for (i = 0; i < IDX_TDM_MAX; i++) {
		if (anc_dev_tdm_port_cfg[i].port_id == port_id) {
			idx = i;
			break;
		}
	}

	return idx;
}

static int anc_dev_tdm_set_clk(
		struct anc_tdm_group_set_info *gp_set_data,
		u16 port_id, bool enable)
{
	int rc = 0;

	switch (gp_set_data->gp_cfg.group_id) {
	case AFE_GROUP_DEVICE_ID_PRIMARY_TDM_RX:
	case AFE_GROUP_DEVICE_ID_PRIMARY_TDM_TX:
		if (gp_set_data->clk_mode) {
			gp_set_data->tdm_clk_set.clk_id =
				Q6AFE_LPASS_CLK_ID_PRI_TDM_IBIT;
		} else
			gp_set_data->tdm_clk_set.clk_id =
				Q6AFE_LPASS_CLK_ID_PRI_TDM_EBIT;
		break;
	case AFE_GROUP_DEVICE_ID_SECONDARY_TDM_RX:
	case AFE_GROUP_DEVICE_ID_SECONDARY_TDM_TX:
		if (gp_set_data->clk_mode) {
			gp_set_data->tdm_clk_set.clk_id =
				Q6AFE_LPASS_CLK_ID_SEC_TDM_IBIT;
		} else
			gp_set_data->tdm_clk_set.clk_id =
				Q6AFE_LPASS_CLK_ID_SEC_TDM_EBIT;
		break;
	case AFE_GROUP_DEVICE_ID_TERTIARY_TDM_RX:
	case AFE_GROUP_DEVICE_ID_TERTIARY_TDM_TX:
		if (gp_set_data->clk_mode) {
			gp_set_data->tdm_clk_set.clk_id =
				Q6AFE_LPASS_CLK_ID_TER_TDM_IBIT;
		} else
			gp_set_data->tdm_clk_set.clk_id =
				Q6AFE_LPASS_CLK_ID_TER_TDM_EBIT;
		break;
	case AFE_GROUP_DEVICE_ID_QUATERNARY_TDM_RX:
	case AFE_GROUP_DEVICE_ID_QUATERNARY_TDM_TX:
		if (gp_set_data->clk_mode) {
			gp_set_data->tdm_clk_set.clk_id =
				Q6AFE_LPASS_CLK_ID_QUAD_TDM_IBIT;
		} else
			gp_set_data->tdm_clk_set.clk_id =
				Q6AFE_LPASS_CLK_ID_QUAD_TDM_EBIT;
		break;
	default:
		pr_err("%s: port id 0x%x not supported\n",
			__func__, port_id);
		return -EINVAL;
	}
	gp_set_data->tdm_clk_set.enable = enable;

	rc = afe_set_lpass_clock_v2(port_id,
		&gp_set_data->tdm_clk_set);

	if (rc < 0)
		pr_err("%s: afe lpass clock failed, err:%d\n",
			__func__, rc);

	return rc;
}

static int anc_dev_port_start(int32_t which_port)
{
	int rc = 0;
	int pt_idx;

	struct afe_tdm_port_config tdm_cfg;

	pt_idx =
	anc_dev_get_matched_tdm_port_cfg_idx(anc_port_cfg[which_port].port_id);

	if (pt_idx == -1) {
		rc = -EINVAL;
		goto rtn;
	}

	tdm_cfg.tdm = anc_dev_tdm_port_cfg[pt_idx].port_cfg;

	tdm_cfg.tdm.num_channels = anc_port_cfg[which_port].num_channels;
	tdm_cfg.tdm.sample_rate = anc_port_cfg[which_port].sample_rate;
	tdm_cfg.tdm.bit_width = anc_port_cfg[which_port].bit_width;

	tdm_cfg.tdm.nslots_per_frame = anc_port_cfg[which_port].num_channels;
	tdm_cfg.tdm.slot_width = anc_port_cfg[which_port].bit_width;
	tdm_cfg.tdm.slot_mask =
		((1 << anc_port_cfg[which_port].num_channels) - 1);

	pr_debug("%s: port_id %x num_channels %x  bit_width %x sample_rate %x nslots_per_frame %x slot_width %x slot_mask %x!\n",
			__func__,
			anc_port_cfg[which_port].port_id,
			tdm_cfg.tdm.num_channels,
			tdm_cfg.tdm.bit_width,
			tdm_cfg.tdm.sample_rate,
			tdm_cfg.tdm.nslots_per_frame,
			tdm_cfg.tdm.slot_width,
			tdm_cfg.tdm.slot_mask);

	rc = anc_if_tdm_port_start(anc_port_cfg[which_port].port_id,
								&tdm_cfg);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: fail to open ANC port from SDSP 0x%x\n",
			__func__, anc_port_cfg[which_port].port_id);
		goto rtn;
	}

rtn:
	return rc;
}

static int anc_dev_port_stop(int32_t which_port)
{
	int rc = 0;

	rc = anc_if_tdm_port_stop(anc_port_cfg[which_port].port_id);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: fail to stop ANC port from SDSP 0x%x\n",
			__func__, anc_port_cfg[which_port].port_id);
	}

	return rc;
}

int msm_anc_dev_set_info(void *info_p, int32_t anc_cmd)
{
	int rc = -EINVAL;

	switch (anc_cmd) {
	case ANC_CMD_ALGO_MODULE: {
		struct audio_anc_algo_module_info *module_info_p =
		(struct audio_anc_algo_module_info *)info_p;

		rc = 0;

		if (this_anc_dev_info.state)
			rc = anc_if_set_algo_module_id(
			anc_port_cfg[ANC_DEV_PORT_ANC_SPKR].port_id,
			module_info_p->module_id);
		else
			this_anc_dev_info.algo_module_id =
			module_info_p->module_id;
		break;
	}
	case ANC_CMD_ALGO_CALIBRATION: {
		rc = -EINVAL;
		if (this_anc_dev_info.state)
			rc = anc_if_set_algo_module_cali_data(
			anc_port_cfg[ANC_DEV_PORT_ANC_SPKR].port_id,
			info_p);
		else
			pr_err("%s: ANC is not running yet\n",
				__func__);
		break;
	}
	default:
		pr_err("%s: ANC cmd wrong\n",
			__func__);
		break;
	}

	return rc;
}

int msm_anc_dev_get_info(void *info_p, int32_t anc_cmd)
{
	int rc = -EINVAL;

	switch (anc_cmd) {
	case ANC_CMD_ALGO_CALIBRATION: {
		if (this_anc_dev_info.state)
			rc = anc_if_get_algo_module_cali_data(
			anc_port_cfg[ANC_DEV_PORT_ANC_SPKR].port_id,
			info_p);
		else
			pr_err("%s: ANC is not running yet\n",
				__func__);
		break;
	}
	default:
		pr_err("%s: ANC cmd wrong\n",
			__func__);
		break;
	}

	return rc;
}

int msm_anc_dev_start(void)
{
	int rc = 0;
	u16 group_id;
	int gp_idx, pt_idx;
	union afe_port_group_config anc_dev_gp_cfg;
	struct afe_tdm_port_config tdm_cfg;

	pr_debug("%s: ANC devices start in!\n", __func__);

	memset(&tdm_cfg, 0, sizeof(tdm_cfg));

	/*
	 * Refs port for ADSP
	 * 1. enable clk
	 * 2. group cfg and enable
	 * 3. Refs port cfg and start
	 */

	group_id =
	get_group_id_from_port_id(anc_port_cfg[ANC_DEV_PORT_REFS].port_id);

	gp_idx = anc_dev_get_matched_tdm_gp_cfg_idx(group_id);

	if (gp_idx == -1) {
		rc = -EINVAL;
		pr_err("%s: anc_dev_get_matched_tdm_gp_cfg_idx() failed with group_id 0x%x\n",
				__func__, group_id);
		goto rtn;
	} else {
		rc = anc_dev_tdm_set_clk(&anc_dev_tdm_gp_set[gp_idx],
			(u16)anc_port_cfg[ANC_DEV_PORT_REFS].port_id, true);

		if (IS_ERR_VALUE(rc)) {
			pr_err("%s: fail to enable AFE clk 0x%x\n",
			__func__,
			anc_port_cfg[ANC_DEV_PORT_REFS].port_id);
				goto rtn;
		}

		anc_dev_gp_cfg.tdm_cfg = anc_dev_tdm_gp_set[gp_idx].gp_cfg;

		anc_dev_gp_cfg.tdm_cfg.group_device_cfg_minor_version =
		AFE_API_VERSION_GROUP_DEVICE_TDM_CONFIG;
		anc_dev_gp_cfg.tdm_cfg.num_channels =
		anc_port_cfg[ANC_DEV_PORT_REFS].num_channels;
		anc_dev_gp_cfg.tdm_cfg.bit_width =
		anc_port_cfg[ANC_DEV_PORT_REFS].bit_width;
		anc_dev_gp_cfg.tdm_cfg.sample_rate =
		anc_port_cfg[ANC_DEV_PORT_REFS].sample_rate;
		anc_dev_gp_cfg.tdm_cfg.nslots_per_frame =
		anc_port_cfg[ANC_DEV_PORT_REFS].num_channels;
		anc_dev_gp_cfg.tdm_cfg.slot_width =
		anc_port_cfg[ANC_DEV_PORT_REFS].bit_width;
		anc_dev_gp_cfg.tdm_cfg.slot_mask =
		((1 << anc_port_cfg[ANC_DEV_PORT_REFS].num_channels) - 1);

		pr_debug("%s: refs_port_id %x\n", __func__,
			anc_port_cfg[ANC_DEV_PORT_REFS].port_id);

		pr_debug("%s: anc_dev_gp_cfg num_channels %x  bit_width %x sample_rate %x nslots_per_frame %x slot_width %x slot_mask %x!\n",
			__func__,
			anc_dev_gp_cfg.tdm_cfg.num_channels,
			anc_dev_gp_cfg.tdm_cfg.bit_width,
			anc_dev_gp_cfg.tdm_cfg.sample_rate,
			anc_dev_gp_cfg.tdm_cfg.nslots_per_frame,
			anc_dev_gp_cfg.tdm_cfg.slot_width,
			anc_dev_gp_cfg.tdm_cfg.slot_mask);

		rc = afe_port_group_enable(group_id,
				&anc_dev_gp_cfg, true);
		if (IS_ERR_VALUE(rc)) {
			pr_err("%s: fail to enable AFE group 0x%x\n",
				__func__, group_id);
			goto rtn;
		}

		pt_idx =
		anc_dev_get_matched_tdm_port_cfg_idx(
			anc_port_cfg[ANC_DEV_PORT_REFS].port_id);

		if (pt_idx == -1) {
			rc = -EINVAL;
			pr_err("%s: anc_dev_get_matched_tdm_port_cfg_idx() failed with port_id 0x%x\n",
			__func__,
			anc_port_cfg[ANC_DEV_PORT_REFS].port_id);
			goto rtn;
		}

		tdm_cfg.tdm = anc_dev_tdm_port_cfg[pt_idx].port_cfg;

		tdm_cfg.tdm.num_channels =
		anc_port_cfg[ANC_DEV_PORT_REFS].num_channels;
		tdm_cfg.tdm.sample_rate =
		anc_port_cfg[ANC_DEV_PORT_REFS].sample_rate;
		tdm_cfg.tdm.bit_width =
		anc_port_cfg[ANC_DEV_PORT_REFS].bit_width;

		tdm_cfg.tdm.nslots_per_frame =
		anc_dev_gp_cfg.tdm_cfg.nslots_per_frame;
		tdm_cfg.tdm.slot_width = anc_dev_gp_cfg.tdm_cfg.slot_width;
		tdm_cfg.tdm.slot_mask = anc_dev_gp_cfg.tdm_cfg.slot_mask;

		rc = afe_tdm_port_start(anc_port_cfg[ANC_DEV_PORT_REFS].port_id,
			&tdm_cfg,
			anc_port_cfg[ANC_DEV_PORT_REFS].sample_rate, 0);
		if (IS_ERR_VALUE(rc)) {
			afe_port_group_enable(group_id,
					&anc_dev_gp_cfg, false);

				anc_dev_tdm_set_clk(&anc_dev_tdm_gp_set[gp_idx],
			(u16)anc_port_cfg[ANC_DEV_PORT_REFS].port_id, false);

			pr_err("%s: fail to open AFE port 0x%x\n",
			__func__,
			anc_port_cfg[ANC_DEV_PORT_REFS].port_id);
			goto rtn;
		}

	}

	rc = anc_if_set_anc_mic_spkr_layout(
		anc_port_cfg[ANC_DEV_PORT_REFS].port_id,
		&anc_mic_spkr_layout);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: fail to pass ANC MIC and SPKR layout info to SDSP 0x%x\n",
		__func__,
		anc_port_cfg[ANC_DEV_PORT_REFS].port_id);
		goto rtn;
	}

	rc = anc_if_share_resource(
	anc_port_cfg[ANC_DEV_PORT_REFS].port_id, 4, 3,
	LPM_START_ADDR, LPM_LENGTH);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: fail to assign lpass resource to SDSP 0x%x\n",
		__func__,
		anc_port_cfg[ANC_DEV_PORT_REFS].port_id);
		goto rtn;
	}

	rc = anc_if_config_ref(anc_port_cfg[ANC_DEV_PORT_REFS].port_id,
			anc_port_cfg[ANC_DEV_PORT_REFS].sample_rate,
			anc_port_cfg[ANC_DEV_PORT_REFS].bit_width,
			anc_port_cfg[ANC_DEV_PORT_REFS].num_channels);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: fail to refs port cfg in SDSP 0x%x\n",
		__func__,
		anc_port_cfg[ANC_DEV_PORT_REFS].port_id);
		goto rtn;
	}

	if (this_anc_dev_info.algo_module_id != 0)
		rc = anc_if_set_algo_module_id(
		anc_port_cfg[ANC_DEV_PORT_ANC_SPKR].port_id,
		this_anc_dev_info.algo_module_id);

	group_id = get_group_id_from_port_id(
			anc_port_cfg[ANC_DEV_PORT_ANC_SPKR].port_id);

	gp_idx = anc_dev_get_matched_tdm_gp_cfg_idx(group_id);

	if (gp_idx == -1) {
		rc = -EINVAL;
			pr_err("%s: anc_dev_get_matched_tdm_gp_cfg_idx() failed with group_id 0x%x\n",
				__func__, group_id);
		goto rtn;
	} else {
		rc = anc_dev_tdm_set_clk(&anc_dev_tdm_gp_set[gp_idx],
			(u16)anc_port_cfg[ANC_DEV_PORT_ANC_SPKR].port_id, true);

		if (IS_ERR_VALUE(rc)) {
			pr_err("%s: fail to enable AFE clk 0x%x\n",
			__func__,
			anc_port_cfg[ANC_DEV_PORT_ANC_SPKR].port_id);
			goto rtn;
		}
	}

	rc = anc_dev_port_start(ANC_DEV_PORT_ANC_MIC);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: fail to enable ANC MIC Port 0x%x\n",
		__func__,
		anc_port_cfg[ANC_DEV_PORT_ANC_MIC].port_id);
		goto rtn;
	}

	rc = anc_dev_port_start(ANC_DEV_PORT_ANC_SPKR);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: fail to enable ANC SPKR Port 0x%x\n",
		__func__,
		anc_port_cfg[ANC_DEV_PORT_ANC_SPKR].port_id);
		goto rtn;
	}

	this_anc_dev_info.state = 1;

	pr_debug("%s: ANC devices start successfully!\n", __func__);

rtn:
	return rc;
}

int msm_anc_dev_stop(void)
{
	int rc = 0;
	u16 group_id;
	int gp_idx;

	anc_dev_port_stop(ANC_DEV_PORT_ANC_SPKR);
	anc_dev_port_stop(ANC_DEV_PORT_ANC_MIC);

	group_id = get_group_id_from_port_id(
	anc_port_cfg[ANC_DEV_PORT_ANC_SPKR].port_id);

	gp_idx = anc_dev_get_matched_tdm_gp_cfg_idx(group_id);

	if (gp_idx == -1) {
		rc = -EINVAL;
		goto rtn;
	} else {
		rc = anc_dev_tdm_set_clk(&anc_dev_tdm_gp_set[gp_idx],
		(u16)anc_port_cfg[ANC_DEV_PORT_ANC_SPKR].port_id, false);

		if (IS_ERR_VALUE(rc)) {
			pr_err("%s: fail to disable AFE clk 0x%x\n",
			__func__,
			anc_port_cfg[ANC_DEV_PORT_ANC_SPKR].port_id);
		}
	}

	group_id =
	get_group_id_from_port_id(anc_port_cfg[ANC_DEV_PORT_REFS].port_id);

	gp_idx = anc_dev_get_matched_tdm_gp_cfg_idx(group_id);

	if (gp_idx == -1) {
		rc = -EINVAL;
		goto rtn;
	}

	afe_close(anc_port_cfg[ANC_DEV_PORT_REFS].port_id);

	afe_port_group_enable(group_id, NULL, false);

	anc_dev_tdm_set_clk(&anc_dev_tdm_gp_set[gp_idx],
			(u16)anc_port_cfg[ANC_DEV_PORT_REFS].port_id, false);

	this_anc_dev_info.state = 0;
	this_anc_dev_info.algo_module_id = 0;

	pr_debug("%s: ANC devices stop successfully!\n", __func__);

rtn:
	return rc;
}


static int msm_anc_tdm_dev_group_cfg_info(
		struct platform_device *pdev,
		struct device_node *ctx_node)
{
	int rc = 0;
	const uint32_t *port_id_array = NULL;
	uint32_t num_tdm_group_ports = 0;
	uint32_t array_length = 0;
	int i = 0;
	int gp_idx = anc_dev_get_free_tdm_gp_cfg_idx();

	if ((gp_idx < 0) || (gp_idx > IDX_GROUP_TDM_MAX)) {
		dev_err(&pdev->dev, "%s: could not get abaiable tdm group cfg slot\n",
		__func__);
		rc = -EINVAL;
		goto rtn;
	}

	/* extract tdm group info into static */
	rc = of_property_read_u32(ctx_node,
		"qcom,msm-cpudai-tdm-group-id",
		(u32 *)&anc_dev_tdm_gp_set[gp_idx].gp_cfg.group_id);
	if (rc) {
		dev_err(&pdev->dev, "%s: Group ID from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-group-id");
		goto rtn;
	}

	dev_dbg(&pdev->dev, "%s: dev_name: %s group_id: 0x%x\n",
		__func__, dev_name(&pdev->dev),
		anc_dev_tdm_gp_set[gp_idx].gp_cfg.group_id);

	rc = of_property_read_u32(ctx_node,
		"qcom,msm-cpudai-tdm-group-num-ports",
		&num_tdm_group_ports);
	if (rc) {
		dev_err(&pdev->dev, "%s: Group Num Ports from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-group-num-ports");
		goto rtn;
	}
	dev_dbg(&pdev->dev, "%s: Group Num Ports from DT file 0x%x\n",
		__func__, num_tdm_group_ports);

	if (num_tdm_group_ports > AFE_GROUP_DEVICE_NUM_PORTS) {
		dev_err(&pdev->dev, "%s Group Num Ports %d greater than Max %d\n",
			__func__, num_tdm_group_ports,
			AFE_GROUP_DEVICE_NUM_PORTS);
		rc = -EINVAL;
		goto rtn;
	}

	port_id_array = of_get_property(ctx_node,
		"qcom,msm-cpudai-tdm-group-port-id",
		&array_length);
	if (port_id_array == NULL) {
		dev_err(&pdev->dev, "%s port_id_array is not valid\n",
			__func__);
		rc = -EINVAL;
		goto rtn;
	}
	if (array_length != sizeof(uint32_t) * num_tdm_group_ports) {
		dev_err(&pdev->dev, "%s array_length is %d, expected is %zd\n",
			__func__, array_length,
			sizeof(uint32_t) * num_tdm_group_ports);
		rc = -EINVAL;
		goto rtn;
	}

	for (i = 0; i < num_tdm_group_ports; i++)
		anc_dev_tdm_gp_set[gp_idx].gp_cfg.port_id[i] =
			(u16)be32_to_cpu(port_id_array[i]);
	/* Unused index should be filled with 0 or AFE_PORT_INVALID */
	for (i = num_tdm_group_ports;
			i < AFE_GROUP_DEVICE_NUM_PORTS; i++)
		anc_dev_tdm_gp_set[gp_idx].gp_cfg.port_id[i] = AFE_PORT_INVALID;

	anc_dev_tdm_gp_set[gp_idx].num_tdm_group_ports = num_tdm_group_ports;

	/* extract tdm clk info into static */
	rc = of_property_read_u32(ctx_node,
		"qcom,msm-cpudai-tdm-clk-rate",
		&anc_dev_tdm_gp_set[gp_idx].tdm_clk_set.clk_freq_in_hz);
	if (rc) {
		dev_err(&pdev->dev, "%s: Clk Rate from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-clk-rate");
		goto rtn;
	}
	dev_dbg(&pdev->dev, "%s: Clk Rate from DT file %d\n",
	__func__,
	anc_dev_tdm_gp_set[gp_idx].tdm_clk_set.clk_freq_in_hz);

	anc_dev_tdm_gp_set[gp_idx].tdm_clk_set.clk_set_minor_version =
	Q6AFE_LPASS_CLK_CONFIG_API_VERSION;
	anc_dev_tdm_gp_set[gp_idx].tdm_clk_set.clk_attri =
	Q6AFE_LPASS_CLK_ATTRIBUTE_INVERT_COUPLE_NO;
	anc_dev_tdm_gp_set[gp_idx].tdm_clk_set.clk_root =
	Q6AFE_LPASS_CLK_ROOT_DEFAULT;


	/* extract tdm clk attribute into static */
	if (of_find_property(ctx_node,
			"qcom,msm-cpudai-tdm-clk-attribute", NULL)) {
		rc = of_property_read_u16(ctx_node,
			"qcom,msm-cpudai-tdm-clk-attribute",
			&anc_dev_tdm_gp_set[gp_idx].tdm_clk_set.clk_attri);
		if (rc) {
			dev_err(&pdev->dev, "%s: No Clk attribute in DT file %s\n",
			__func__,
			"qcom,msm-cpudai-tdm-clk-attribute");
			goto rtn;
		}
	} else {
		dev_dbg(&pdev->dev, "%s: Clk Attribute from DT file %d\n",
		__func__,
		anc_dev_tdm_gp_set[gp_idx].tdm_clk_set.clk_attri);
	}

	/* extract tdm clk src master/slave info into static */
	rc = of_property_read_u32(ctx_node,
		"qcom,msm-cpudai-tdm-clk-internal",
		&anc_dev_tdm_gp_set[gp_idx].clk_mode);
	if (rc) {
		dev_err(&pdev->dev, "%s: Clk id from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-clk-internal");
		goto rtn;
	}
	dev_dbg(&pdev->dev, "%s: Clk id from DT file %d\n",
		__func__, anc_dev_tdm_gp_set[gp_idx].clk_mode);

rtn:
	return rc;
}


static int msm_anc_tdm_dev_port_cfg_info(
		struct platform_device *pdev,
		struct device_node *ctx_node)
{
	int rc = 0;
	u32 tdm_dev_id = 0;
	int pt_idx = anc_dev_get_free_tdm_port_cfg_idx();
	struct device_node *tdm_parent_node = NULL;

	if ((pt_idx < 0) || (pt_idx > IDX_TDM_MAX)) {
		dev_err(&pdev->dev, "%s: could not get abaiable tdm port cfg slot\n",
		__func__);
		rc = -EINVAL;
		goto rtn;
	}

	/* retrieve device/afe id */
	rc = of_property_read_u32(ctx_node,
		"qcom,msm-cpudai-tdm-dev-id",
		&tdm_dev_id);
	if (rc) {
		dev_err(&pdev->dev, "%s: Device ID missing in DT file\n",
			__func__);
		goto rtn;
	}
	if ((tdm_dev_id < AFE_PORT_ID_TDM_PORT_RANGE_START) ||
		(tdm_dev_id > AFE_PORT_ID_TDM_PORT_RANGE_END)) {
		dev_err(&pdev->dev, "%s: Invalid TDM Device ID 0x%x in DT file\n",
			__func__, tdm_dev_id);
		rc = -ENXIO;
		goto rtn;
	}
	anc_dev_tdm_port_cfg[pt_idx].port_id = tdm_dev_id;

	dev_dbg(&pdev->dev, "%s: dev_name: %s dev_id: 0x%x\n",
		__func__, dev_name(&pdev->dev), tdm_dev_id);

	/* TDM CFG */
	tdm_parent_node = of_get_parent(ctx_node);
	rc = of_property_read_u32(tdm_parent_node,
		"qcom,msm-cpudai-tdm-sync-mode",
		(u32 *)&anc_dev_tdm_port_cfg[pt_idx].port_cfg.sync_mode);
	if (rc) {
		dev_err(&pdev->dev, "%s: Sync Mode from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-sync-mode");
		goto rtn;
	}
	dev_dbg(&pdev->dev, "%s: Sync Mode from DT file 0x%x\n",
		__func__, anc_dev_tdm_port_cfg[pt_idx].port_cfg.sync_mode);

	rc = of_property_read_u32(tdm_parent_node,
		"qcom,msm-cpudai-tdm-sync-src",
		(u32 *)&anc_dev_tdm_port_cfg[pt_idx].port_cfg.sync_src);
	if (rc) {
		dev_err(&pdev->dev, "%s: Sync Src from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-sync-src");
		goto rtn;
	}
	dev_dbg(&pdev->dev, "%s: Sync Src from DT file 0x%x\n",
		__func__, anc_dev_tdm_port_cfg[pt_idx].port_cfg.sync_src);

	rc = of_property_read_u32(tdm_parent_node,
	"qcom,msm-cpudai-tdm-data-out",
	(u32 *)&anc_dev_tdm_port_cfg[pt_idx].port_cfg.ctrl_data_out_enable);
	if (rc) {
		dev_err(&pdev->dev, "%s: Data Out from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-data-out");
		goto rtn;
	}
	dev_dbg(&pdev->dev, "%s: Data Out from DT file 0x%x\n",
	__func__,
	anc_dev_tdm_port_cfg[pt_idx].port_cfg.ctrl_data_out_enable);

	rc = of_property_read_u32(tdm_parent_node,
	"qcom,msm-cpudai-tdm-invert-sync",
	(u32 *)&anc_dev_tdm_port_cfg[pt_idx].port_cfg.ctrl_invert_sync_pulse);
	if (rc) {
		dev_err(&pdev->dev, "%s: Invert Sync from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-invert-sync");
		goto rtn;
	}
	dev_dbg(&pdev->dev, "%s: Invert Sync from DT file 0x%x\n",
	__func__,
	anc_dev_tdm_port_cfg[pt_idx].port_cfg.ctrl_invert_sync_pulse);

	rc = of_property_read_u32(tdm_parent_node,
	"qcom,msm-cpudai-tdm-data-delay",
	(u32 *)&anc_dev_tdm_port_cfg[pt_idx].port_cfg.ctrl_sync_data_delay);
	if (rc) {
		dev_err(&pdev->dev, "%s: Data Delay from DT file %s\n",
			__func__, "qcom,msm-cpudai-tdm-data-delay");
		goto rtn;
	}
	dev_dbg(&pdev->dev, "%s: Data Delay from DT file 0x%x\n",
	__func__,
	anc_dev_tdm_port_cfg[pt_idx].port_cfg.ctrl_sync_data_delay);

	/* TDM CFG -- set default */
	anc_dev_tdm_port_cfg[pt_idx].port_cfg.data_format = AFE_LINEAR_PCM_DATA;
	anc_dev_tdm_port_cfg[pt_idx].port_cfg.tdm_cfg_minor_version =
		AFE_API_VERSION_TDM_CONFIG;

	msm_anc_tdm_dev_group_cfg_info(pdev, tdm_parent_node);

	return 0;

rtn:
	return rc;
}

static int msm_anc_dev_probe(struct platform_device *pdev)
{
	int rc = 0;

	u32 port_id = 0;
	const uint32_t *layout_array = NULL;
	uint32_t num_anc_io = 0;
	uint32_t array_length = 0;
	int i = 0;
	uint32_t sample_rate = 0;
	uint32_t num_channels = 0;
	uint32_t bit_width = 0;

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,refs-port-id",
		(u32 *)&port_id);
	if (rc) {
		dev_err(&pdev->dev, "%s: ANC refs-port-id DT file %s\n",
			__func__, "qcom,refs-port-id");
		goto rtn;
	}

	anc_port_cfg[ANC_DEV_PORT_REFS].port_id = port_id;

	dev_dbg(&pdev->dev, "%s: refs-port-id 0x%x\n",
		__func__, port_id);

	port_id = 0;
	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,spkr-port-id",
		(u32 *)&port_id);
	if (rc) {
		dev_err(&pdev->dev, "%s: ANC spkr-port-id DT file %s\n",
			__func__, "qcom,spkr-port-id");
		goto rtn;
	}

	anc_port_cfg[ANC_DEV_PORT_ANC_SPKR].port_id = port_id;

	dev_dbg(&pdev->dev, "%s: spkr-port-id 0x%x\n",
		__func__, port_id);

	port_id = 0;
	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mic-port-id",
		(u32 *)&port_id);
	if (rc) {
		dev_err(&pdev->dev, "%s: ANC mic-port-id DT file %s\n",
			__func__, "qcom,mic-port-id");
		goto rtn;
	}

	anc_port_cfg[ANC_DEV_PORT_ANC_MIC].port_id = port_id;

	dev_dbg(&pdev->dev, "%s: mic-port-id 0x%x\n",
		__func__, port_id);

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,sample-rate",
		(u32 *)&sample_rate);
	if (rc) {
		dev_err(&pdev->dev, "%s: ANC sample rate DT file %s\n",
			__func__, "qcom,sample-rate");
		goto rtn;
	}

	dev_dbg(&pdev->dev, "%s: ANC sample rate 0x%x\n",
		__func__, sample_rate);

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,num-channels",
		(u32 *)&num_channels);
	if (rc) {
		dev_err(&pdev->dev, "%s: ANC num channels DT file %s\n",
			__func__, "qcom,num-channels");
		goto rtn;
	}

	dev_dbg(&pdev->dev, "%s: ANC num channel 0x%x\n",
		__func__, num_channels);

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,bit-width",
		(u32 *)&bit_width);
	if (rc) {
		dev_err(&pdev->dev, "%s: ANC bit width DT file %s\n",
			__func__, "qcom,bit-width");
		goto rtn;
	}

	dev_dbg(&pdev->dev, "%s: ANC bit width 0x%x\n",
		__func__, bit_width);

	for (i = 0; i < ANC_DEV_PORT_MAX; i++) {
		anc_port_cfg[i].sample_rate = sample_rate;
		anc_port_cfg[i].num_channels = num_channels;
		anc_port_cfg[i].bit_width = bit_width;
	}

	memset(&anc_mic_spkr_layout, 0, sizeof(anc_mic_spkr_layout));

	anc_mic_spkr_layout.minor_version = 1;

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,num-anc-mic",
		(u32 *)&num_anc_io);
	if (rc) {
		dev_err(&pdev->dev, "%s: ANC num_anc_mic DT file %s\n",
			__func__, "qcom,num-anc-mic");
		goto rtn;
	}

	layout_array = of_get_property(pdev->dev.of_node,
		"qcom,anc-mic-array",
		&array_length);
	if (layout_array == NULL) {
		dev_err(&pdev->dev, "%s layout_array is not valid\n",
			__func__);
		rc = -EINVAL;
		goto rtn;
	}
	if (array_length != sizeof(uint32_t) * num_anc_io) {
		dev_err(&pdev->dev, "%s array_length is %d, expected is %zd\n",
			__func__, array_length,
			sizeof(uint32_t) * num_anc_io);
		rc = -EINVAL;
		goto rtn;
	}

	anc_mic_spkr_layout.num_anc_mic = num_anc_io;

	for (i = 0; i < num_anc_io; i++)
		anc_mic_spkr_layout.mic_layout_array[i] =
			(u16)be32_to_cpu(layout_array[i]);

	num_anc_io = 0;
	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,num-anc-spkr",
		(u32 *)&num_anc_io);
	if (rc) {
		dev_err(&pdev->dev, "%s: ANC num_anc_mic DT file %s\n",
			__func__, "qcom,num-anc-spkr");
		goto rtn;
	}

	layout_array = of_get_property(pdev->dev.of_node,
		"qcom,anc-spkr-array",
		&array_length);
	if (layout_array == NULL) {
		dev_err(&pdev->dev, "%s layout_array is not valid\n",
			__func__);
		rc = -EINVAL;
		goto rtn;
	}
	if (array_length != sizeof(uint32_t) * num_anc_io) {
		dev_err(&pdev->dev, "%s array_length is %d, expected is %zd\n",
			__func__, array_length,
			sizeof(uint32_t) * num_anc_io);
		rc = -EINVAL;
		goto rtn;
	}

	anc_mic_spkr_layout.num_anc_spkr = num_anc_io;

	for (i = 0; i < num_anc_io; i++)
		anc_mic_spkr_layout.spkr_layout_array[i] =
			(u16)be32_to_cpu(layout_array[i]);

	dev_dbg(&pdev->dev, "%s: num_anc_mic 0x%x\n",
		__func__, anc_mic_spkr_layout.num_anc_mic);

	dev_dbg(&pdev->dev, "%s: num_anc_spkr 0x%x\n",
		__func__, anc_mic_spkr_layout.num_anc_spkr);

	num_anc_io = 0;
	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,num-add-mic-signal",
		(u32 *)&num_anc_io);
	if (rc) {
		dev_err(&pdev->dev, "%s: ANC num_add_mic_signal DT file %s\n",
			__func__, "qcom,num-add-mic-signal");
		goto rtn;
	}

	anc_mic_spkr_layout.num_add_mic_signal = num_anc_io;

	num_anc_io = 0;
	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,num-add-spkr-signal",
		(u32 *)&num_anc_io);
	if (rc) {
		dev_err(&pdev->dev, "%s: ANC num_add_spkr_signal DT file %s\n",
			__func__, "qcom,num-add-spkr-signal");
		goto rtn;
	}

	anc_mic_spkr_layout.num_add_spkr_signal = num_anc_io;

	dev_dbg(&pdev->dev, "%s: num_add_mic_signal 0x%x\n",
		__func__, anc_mic_spkr_layout.num_add_mic_signal);

	dev_dbg(&pdev->dev, "%s: num_add_spkr_signal 0x%x\n",
		__func__, anc_mic_spkr_layout.num_add_spkr_signal);

	/* TDM group CFG and TDM port CFG */
	{
		struct device_node *ctx_node = NULL;

		ctx_node = of_parse_phandle(pdev->dev.of_node,
			"qcom,refs-tdm-rx", 0);
		if (!ctx_node) {
			pr_err("%s Could not find refs-tdm-rx info\n",
				__func__);
			return -EINVAL;
		}

		rc = msm_anc_tdm_dev_port_cfg_info(pdev, ctx_node);
		if (IS_ERR_VALUE(rc)) {
			pr_err("%s: fail to probe TDM group info\n",
			__func__);
		}

		ctx_node = of_parse_phandle(pdev->dev.of_node,
			"qcom,spkr-tdm-rx", 0);
		if (!ctx_node) {
			pr_err("%s Could not find spkr-tdm-rx info\n",
				__func__);
			return -EINVAL;
		}

		rc = msm_anc_tdm_dev_port_cfg_info(pdev, ctx_node);
		if (IS_ERR_VALUE(rc)) {
			pr_err("%s: fail to probe TDM group info\n",
			__func__);
		}

		ctx_node = of_parse_phandle(pdev->dev.of_node,
			"qcom,mic-tdm-tx", 0);
		if (!ctx_node) {
			pr_err("%s Could not find mic-tdm-tx info\n",
				__func__);
			return -EINVAL;
		}

		rc = msm_anc_tdm_dev_port_cfg_info(pdev, ctx_node);
		if (IS_ERR_VALUE(rc)) {
			pr_err("%s: fail to probe TDM group info\n",
			__func__);
		}
	}

	rc = msm_anc_dev_create(pdev);

rtn:
	return rc;
}

static int msm_anc_dev_remove(struct platform_device *pdev)
{
	return msm_anc_dev_destroy(pdev);
}

static const struct of_device_id msm_anc_dev_dt_match[] = {
	{ .compatible = "qcom,msm-ext-anc", },
	{}
};

MODULE_DEVICE_TABLE(of, msm_anc_dev_dt_match);

static struct platform_driver msm_anc_dev = {
	.probe  = msm_anc_dev_probe,
	.remove = msm_anc_dev_remove,
	.driver = {
		.name = "msm-ext-anc",
		.owner = THIS_MODULE,
		.of_match_table = msm_anc_dev_dt_match,
	},
};

int msm_anc_dev_init(void)
{
	int rc = 0;

	memset(&anc_dev_tdm_gp_set, 0, sizeof(anc_dev_tdm_gp_set));
	memset(&anc_dev_tdm_port_cfg, 0, sizeof(anc_dev_tdm_port_cfg));
	memset(&anc_port_cfg, 0, sizeof(anc_port_cfg));
	memset(&this_anc_dev_info, 0, sizeof(this_anc_dev_info));

	rc = platform_driver_register(&msm_anc_dev);
	if (rc)
	pr_err("%s: fail to register msm ANC device driver\n",
	__func__);

	return rc;
}

int msm_anc_dev_deinit(void)
{
	platform_driver_unregister(&msm_anc_dev);
	return 0;
}

