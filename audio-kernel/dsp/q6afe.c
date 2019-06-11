/* Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <dsp/msm_audio_ion.h>
#include <dsp/apr_audio-v2.h>
#include <dsp/audio_cal_utils.h>
#include <dsp/q6afe-v2.h>
#include <dsp/q6audio-v2.h>
#include <dsp/q6common.h>
#include <dsp/q6core.h>
#include <dsp/msm-audio-event-notify.h>
#include <dsp/apr_elliptic.h>
#include <ipc/apr_tal.h>
#include "adsp_err.h"
#include "q6afecal-hwdep.h"
#ifdef CONFIG_SND_SOC_TFA9874_FOR_DAVI
#include "../asoc/codecs/tfa98xx/inc/tfa_platform_interface_definition.h"
#endif

#ifdef CONFIG_MSM_CSPL
#include <dsp/msm-cirrus-playback.h>
#endif

#define WAKELOCK_TIMEOUT	5000
enum {
	AFE_COMMON_RX_CAL = 0,
	AFE_COMMON_TX_CAL,
	AFE_LSM_TX_CAL,
	AFE_AANC_CAL,
	AFE_FB_SPKR_PROT_CAL,
	AFE_HW_DELAY_CAL,
	AFE_SIDETONE_CAL,
	AFE_SIDETONE_IIR_CAL,
	AFE_TOPOLOGY_CAL,
	AFE_LSM_TOPOLOGY_CAL,
	AFE_CUST_TOPOLOGY_CAL,
	AFE_FB_SPKR_PROT_TH_VI_CAL,
	AFE_FB_SPKR_PROT_EX_VI_CAL,
	MAX_AFE_CAL_TYPES
};

enum fbsp_state {
	FBSP_INCORRECT_OP_MODE,
	FBSP_INACTIVE,
	FBSP_WARMUP,
	FBSP_IN_PROGRESS,
	FBSP_SUCCESS,
	FBSP_FAILED,
	MAX_FBSP_STATE
};

static char fbsp_state[MAX_FBSP_STATE][50] = {
	[FBSP_INCORRECT_OP_MODE] = "incorrect operation mode",
	[FBSP_INACTIVE] = "port not started",
	[FBSP_WARMUP] = "waiting for warmup",
	[FBSP_IN_PROGRESS] = "in progress state",
	[FBSP_SUCCESS] = "success",
	[FBSP_FAILED] = "failed"
};

enum v_vali_state {
	V_VALI_FAILED,
	V_VALI_SUCCESS,
	V_VALI_INCORRECT_OP_MODE,
	V_VALI_INACTIVE,
	V_VALI_WARMUP,
	V_VALI_IN_PROGRESS,
	MAX_V_VALI_STATE
};

enum {
	USE_CALIBRATED_R0TO,
	USE_SAFE_R0TO
};

enum {
	QUICK_CALIB_DISABLE,
	QUICK_CALIB_ENABLE
};

enum {
	Q6AFE_MSM_SPKR_PROCESSING = 0,
	Q6AFE_MSM_SPKR_CALIBRATION,
	Q6AFE_MSM_SPKR_FTM_MODE,
	Q6AFE_MSM_SPKR_V_VALI_MODE
};

struct wlock {
	struct wakeup_source ws;
};

static struct wlock wl;

struct afe_ctl {
	void *apr;
	atomic_t state;
	atomic_t status;
	wait_queue_head_t wait[AFE_MAX_PORTS];
	wait_queue_head_t wait_wakeup;
	wait_queue_head_t lpass_core_hw_wait;
	uint32_t lpass_hw_core_client_hdl;
	void (*tx_cb)(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv);
	void (*rx_cb)(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv);
	void *tx_private_data;
	void *rx_private_data;
	uint32_t mmap_handle;

	void (*pri_spdif_tx_cb)(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv);
	void (*sec_spdif_tx_cb)(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv);
	void *pri_spdif_tx_private_data;
	void *sec_spdif_tx_private_data;
	int pri_spdif_config_change;
	int sec_spdif_config_change;
	struct work_struct afe_spdif_work;

	int	topology[AFE_MAX_PORTS];
	struct cal_type_data *cal_data[MAX_AFE_CAL_TYPES];

	atomic_t mem_map_cal_handles[MAX_AFE_CAL_TYPES];
	atomic_t mem_map_cal_index;
	u32 afe_cal_mode[AFE_MAX_PORTS];

	u16 dtmf_gen_rx_portid;
	struct audio_cal_info_spk_prot_cfg	prot_cfg;
	struct afe_spkr_prot_calib_get_resp	calib_data;
	struct audio_cal_info_sp_th_vi_ftm_cfg	th_ftm_cfg;
	struct audio_cal_info_sp_th_vi_v_vali_cfg	v_vali_cfg;
	struct audio_cal_info_sp_ex_vi_ftm_cfg	ex_ftm_cfg;
	struct afe_sp_th_vi_get_param_resp	th_vi_resp;
	struct afe_sp_th_vi_v_vali_get_param_resp	th_vi_v_vali_resp;
	struct afe_sp_ex_vi_get_param_resp	ex_vi_resp;
	struct afe_sp_rx_tmax_xmax_logging_resp	xt_logging_resp;
	struct afe_av_dev_drift_get_param_resp	av_dev_drift_resp;
	int vi_tx_port;
	int vi_rx_port;
	uint32_t afe_sample_rates[AFE_MAX_PORTS];
	struct aanc_data aanc_info;
	struct mutex afe_cmd_lock;
	int set_custom_topology;
	int dev_acdb_id[AFE_MAX_PORTS];
	routing_cb rt_cb;
	struct audio_uevent_data *uevent_data;
	/* cal info for AFE */
	struct afe_fw_info *fw_data;
	u32 island_mode[AFE_MAX_PORTS];
	struct vad_config vad_cfg[AFE_MAX_PORTS];
	struct work_struct afe_dc_work;
#ifdef CONFIG_SND_SOC_TFA9874_FOR_DAVI
	struct rtac_cal_block_data tfa_cal;
	atomic_t tfa_state;
#endif /*CONFIG_SND_SOC_TFA9874_FOR_DAVI*/
	struct notifier_block event_notifier;
	/* FTM spk params */
	uint32_t initial_cal;
	uint32_t v_vali_flag;
};

static atomic_t afe_ports_mad_type[SLIMBUS_PORT_LAST - SLIMBUS_0_RX];
static unsigned long afe_configured_cmd;

static struct afe_ctl this_afe;

#define TIMEOUT_MS 1000
#define Q6AFE_MAX_VOLUME 0x3FFF

static int pcm_afe_instance[2];
static int proxy_afe_instance[2];
bool afe_close_done[2] = {true, true};

#define SIZEOF_CFG_CMD(y) \
		(sizeof(struct apr_hdr) + sizeof(u16) + (sizeof(struct y)))

static int afe_get_cal_hw_delay(int32_t path,
				struct audio_cal_hw_delay_entry *entry);
static int remap_cal_data(struct cal_block_data *cal_block, int cal_index);

#ifdef CONFIG_MSM_CSPL
struct afe_cspl_state cspl_afe = {
	.apr= &this_afe.apr,
	.status= &this_afe.status,
	.state= &this_afe.state,
	.wait= this_afe.wait,
	.timeout_ms= TIMEOUT_MS,
};
EXPORT_SYMBOL(cspl_afe);
#endif

int afe_get_spk_initial_cal(void)
{
	return this_afe.initial_cal;
}

void afe_get_spk_r0(int *spk_r0)
{
	uint16_t i = 0;

	for (; i < SP_V2_NUM_MAX_SPKRS; i++)
		spk_r0[i] = this_afe.prot_cfg.r0[i];
}

void afe_get_spk_t0(int *spk_t0)
{
	uint16_t i = 0;

	for (; i < SP_V2_NUM_MAX_SPKRS; i++)
		spk_t0[i] = this_afe.prot_cfg.t0[i];
}

int afe_get_spk_v_vali_flag(void)
{
	return this_afe.v_vali_flag;
}

void afe_get_spk_v_vali_sts(int *spk_v_vali_sts)
{
	uint16_t i = 0;

	for (; i < SP_V2_NUM_MAX_SPKRS; i++)
		spk_v_vali_sts[i] =
			this_afe.th_vi_v_vali_resp.param.status[i];
}

void afe_set_spk_initial_cal(int initial_cal)
{
	this_afe.initial_cal = initial_cal;
}

void afe_set_spk_v_vali_flag(int v_vali_flag)
{
	this_afe.v_vali_flag = v_vali_flag;
}

int afe_get_topology(int port_id)
{
	int topology;
	int port_index = afe_get_port_index(port_id);

	if ((port_index < 0) || (port_index >= AFE_MAX_PORTS)) {
		pr_err("%s: Invalid port index %d\n", __func__, port_index);
		topology = -EINVAL;
		goto done;
	}

	topology = this_afe.topology[port_index];
done:
	return topology;
}


/**
 * afe_set_aanc_info -
 *        Update AFE AANC info
 *
 * @q6_aanc_info: AFE AANC info params
 *
 */
void afe_set_aanc_info(struct aanc_data *q6_aanc_info)
{
	this_afe.aanc_info.aanc_active = q6_aanc_info->aanc_active;
	this_afe.aanc_info.aanc_rx_port = q6_aanc_info->aanc_rx_port;
	this_afe.aanc_info.aanc_tx_port = q6_aanc_info->aanc_tx_port;

	pr_debug("%s: aanc active is %d rx port is 0x%x, tx port is 0x%x\n",
		__func__,
		this_afe.aanc_info.aanc_active,
		this_afe.aanc_info.aanc_rx_port,
		this_afe.aanc_info.aanc_tx_port);
}
EXPORT_SYMBOL(afe_set_aanc_info);

static void afe_callback_debug_print(struct apr_client_data *data)
{
	uint32_t *payload;

	payload = data->payload;

	if (data->payload_size >= 8)
		pr_debug("%s: code = 0x%x PL#0[0x%x], PL#1[0x%x], size = %d\n",
			__func__, data->opcode, payload[0], payload[1],
			data->payload_size);
	else if (data->payload_size >= 4)
		pr_debug("%s: code = 0x%x PL#0[0x%x], size = %d\n",
			__func__, data->opcode, payload[0],
			data->payload_size);
	else
		pr_debug("%s: code = 0x%x, size = %d\n",
			__func__, data->opcode, data->payload_size);
}

static void av_dev_drift_afe_cb_handler(uint32_t opcode, uint32_t *payload,
					uint32_t payload_size)
{
	u32 param_id;
	size_t expected_size =
		sizeof(u32) + sizeof(struct afe_param_id_dev_timing_stats);

	/* Get param ID depending on command type */
	param_id = (opcode == AFE_PORT_CMDRSP_GET_PARAM_V3) ? payload[3] :
							      payload[2];
	if (param_id != AFE_PARAM_ID_DEV_TIMING_STATS) {
		pr_err("%s: Unrecognized param ID %d\n", __func__, param_id);
		return;
	}

	switch (opcode) {
	case AFE_PORT_CMDRSP_GET_PARAM_V2:
		expected_size += sizeof(struct param_hdr_v1);
		if (payload_size < expected_size) {
			pr_err("%s: Error: received size %d, expected size %zu\n",
			       __func__, payload_size, expected_size);
			return;
		}
		/* Repack response to add IID */
		this_afe.av_dev_drift_resp.status = payload[0];
		this_afe.av_dev_drift_resp.pdata.module_id = payload[1];
		this_afe.av_dev_drift_resp.pdata.instance_id = INSTANCE_ID_0;
		this_afe.av_dev_drift_resp.pdata.param_id = payload[2];
		this_afe.av_dev_drift_resp.pdata.param_size = payload[3];
		memcpy(&this_afe.av_dev_drift_resp.timing_stats, &payload[4],
		       sizeof(struct afe_param_id_dev_timing_stats));
		break;
	case AFE_PORT_CMDRSP_GET_PARAM_V3:
		expected_size += sizeof(struct param_hdr_v3);
		if (payload_size < expected_size) {
			pr_err("%s: Error: received size %d, expected size %zu\n",
			       __func__, payload_size, expected_size);
			return;
		}
		memcpy(&this_afe.av_dev_drift_resp, payload,
				sizeof(this_afe.av_dev_drift_resp));
		break;
	default:
		pr_err("%s: Unrecognized command %d\n", __func__, opcode);
		return;
	}

	if (!this_afe.av_dev_drift_resp.status) {
		atomic_set(&this_afe.state, 0);
	} else {
		pr_debug("%s: av_dev_drift_resp status: %d", __func__,
			 this_afe.av_dev_drift_resp.status);
		atomic_set(&this_afe.state, -1);
	}
}

static int32_t sp_make_afe_callback(uint32_t opcode, uint32_t *payload,
				    uint32_t payload_size)
{
	struct param_hdr_v3 param_hdr;
	u32 *data_dest = NULL;
	u32 *data_start = NULL;
	size_t expected_size = sizeof(u32);

	memset(&param_hdr, 0, sizeof(param_hdr));

	/* Set command specific details */
	switch (opcode) {
	case AFE_PORT_CMDRSP_GET_PARAM_V2:
		if (payload_size < (5 * sizeof(uint32_t))) {
			pr_err("%s: Error: size %d is less than expected\n",
				__func__, payload_size);
			return -EINVAL;
		}
		expected_size += sizeof(struct param_hdr_v1);
		param_hdr.module_id = payload[1];
		param_hdr.instance_id = INSTANCE_ID_0;
		param_hdr.param_id = payload[2];
		param_hdr.param_size = payload[3];
		data_start = &payload[4];
		break;
	case AFE_PORT_CMDRSP_GET_PARAM_V3:
		if (payload_size < (6 * sizeof(uint32_t))) {
			pr_err("%s: Error: size %d is less than expected\n",
				__func__, payload_size);
			return -EINVAL;
		}
		expected_size += sizeof(struct param_hdr_v3);
		memcpy(&param_hdr, &payload[1], sizeof(struct param_hdr_v3));
		data_start = &payload[5];
		break;
	default:
		pr_err("%s: Unrecognized command %d\n", __func__, opcode);
		return -EINVAL;
	}

	switch (param_hdr.param_id) {
	case AFE_PARAM_ID_CALIB_RES_CFG_V2:
		expected_size += sizeof(struct asm_calib_res_cfg);
		data_dest = (u32 *) &this_afe.calib_data;
		break;
	case AFE_PARAM_ID_SP_V2_TH_VI_FTM_PARAMS:
		expected_size += sizeof(struct afe_sp_th_vi_ftm_params);
		data_dest = (u32 *) &this_afe.th_vi_resp;
		break;
	case AFE_PARAM_ID_SP_V2_TH_VI_V_VALI_PARAMS:
		expected_size += sizeof(struct afe_sp_th_vi_v_vali_params);
		data_dest = (u32 *) &this_afe.th_vi_v_vali_resp;
		break;
	case AFE_PARAM_ID_SP_V2_EX_VI_FTM_PARAMS:
		expected_size += sizeof(struct afe_sp_ex_vi_ftm_params);
		data_dest = (u32 *) &this_afe.ex_vi_resp;
		break;
	case AFE_PARAM_ID_SP_RX_TMAX_XMAX_LOGGING:
		expected_size += sizeof(
				struct afe_sp_rx_tmax_xmax_logging_param);
		data_dest = (u32 *) &this_afe.xt_logging_resp;
		break;
	default:
		pr_err("%s: Unrecognized param ID %d\n", __func__,
		       param_hdr.param_id);
		return -EINVAL;
	}

	if (payload_size < expected_size) {
		pr_err("%s: Error: received size %d, expected size %zu for param %d\n",
		       __func__, payload_size, expected_size,
		       param_hdr.param_id);
		return -EINVAL;
	}

	data_dest[0] = payload[0];
	memcpy(&data_dest[1], &param_hdr, sizeof(struct param_hdr_v3));
	memcpy(&data_dest[5], data_start, param_hdr.param_size);

	if (!data_dest[0]) {
		atomic_set(&this_afe.state, 0);
	} else {
		pr_debug("%s: status: %d", __func__, data_dest[0]);
		atomic_set(&this_afe.state, -1);
	}

	return 0;
}

static void afe_notify_dc_presence(void)
{
	pr_debug("%s: DC detected\n", __func__);
	msm_aud_evt_notifier_call_chain(MSM_AUD_DC_EVENT, NULL);

	schedule_work(&this_afe.afe_dc_work);
}

static void afe_notify_dc_presence_work_fn(struct work_struct *work)
{
	int ret = 0;
	char event[] = "DC_PRESENCE=TRUE";

	ret = q6core_send_uevent(this_afe.uevent_data, event);
	if (ret)
		pr_err("%s: Send UEvent %s failed :%d\n",
		       __func__, event, ret);
}

static int afe_aud_event_notify(struct notifier_block *self,
				unsigned long action, void *data)
{
	switch (action) {
	case SWR_WAKE_IRQ_REGISTER:
		afe_send_cmd_wakeup_register(data, true);
		break;
	case SWR_WAKE_IRQ_DEREGISTER:
		afe_send_cmd_wakeup_register(data, false);
		break;
	default:
		pr_err("%s: invalid event type: %lu\n", __func__, action);
		return -EINVAL;
	}

	return 0;
}

static void afe_notify_spdif_fmt_update_work_fn(struct work_struct *work)
{
	int ret = 0;
	char event_pri[] = "PRI_SPDIF_TX=MEDIA_CONFIG_CHANGE";
	char event_sec[] = "SEC_SPDIF_TX=MEDIA_CONFIG_CHANGE";

	if (this_afe.pri_spdif_config_change) {
		this_afe.pri_spdif_config_change = 0;
		ret = q6core_send_uevent(this_afe.uevent_data, event_pri);
		if (ret)
			pr_err("%s: Send UEvent %s failed :%d\n",
			       __func__, event_pri, ret);
	}
	if (this_afe.sec_spdif_config_change) {
		this_afe.sec_spdif_config_change = 0;
		ret = q6core_send_uevent(this_afe.uevent_data, event_sec);
		if (ret)
			pr_err("%s: Send UEvent %s failed :%d\n",
			       __func__, event_sec, ret);
	}
}

static void afe_notify_spdif_fmt_update(void *payload)
{
	struct afe_port_mod_evt_rsp_hdr *evt_pl;

	evt_pl = (struct afe_port_mod_evt_rsp_hdr *)payload;
	if (evt_pl->port_id == AFE_PORT_ID_PRIMARY_SPDIF_TX)
		this_afe.pri_spdif_config_change = 1;
	else
		this_afe.sec_spdif_config_change = 1;

	schedule_work(&this_afe.afe_spdif_work);
}

static bool afe_token_is_valid(uint32_t token)
{
	if (token >= AFE_MAX_PORTS) {
		pr_err("%s: token %d is invalid.\n", __func__, token);
		return false;
	}
	return true;
}

static int32_t afe_callback(struct apr_client_data *data, void *priv)
{
	if (!data) {
		pr_err("%s: Invalid param data\n", __func__);
		return -EINVAL;
	}
	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: reset event = %d %d apr[%pK]\n",
			__func__,
			data->reset_event, data->reset_proc, this_afe.apr);

		cal_utils_clear_cal_block_q6maps(MAX_AFE_CAL_TYPES,
			this_afe.cal_data);

		/* Reset the custom topology mode: to resend again to AFE. */
		mutex_lock(&this_afe.cal_data[AFE_CUST_TOPOLOGY_CAL]->lock);
		this_afe.set_custom_topology = 1;
		mutex_unlock(&this_afe.cal_data[AFE_CUST_TOPOLOGY_CAL]->lock);
		rtac_clear_mapping(AFE_RTAC_CAL);

		if (this_afe.apr) {
			apr_reset(this_afe.apr);
			atomic_set(&this_afe.state, 0);
			this_afe.apr = NULL;
			rtac_set_afe_handle(this_afe.apr);
		}

		/*
		 * Pass reset events to proxy driver, if cb is registered
		 */
		if (data->payload_size < sizeof(uint32_t)) {
			pr_err("%s: Error: size %d is less than expected\n",
				__func__, data->payload_size);
			return -EINVAL;
		}

		if (this_afe.tx_cb) {
			this_afe.tx_cb(data->opcode, data->token,
					data->payload,
					this_afe.tx_private_data);
			this_afe.tx_cb = NULL;
		}
		if (this_afe.rx_cb) {
			this_afe.rx_cb(data->opcode, data->token,
					data->payload,
					this_afe.rx_private_data);
			this_afe.rx_cb = NULL;
		}

		return 0;
	}
	afe_callback_debug_print(data);
	if (data->opcode == AFE_PORT_CMDRSP_GET_PARAM_V2 ||
	    data->opcode == AFE_PORT_CMDRSP_GET_PARAM_V3) {
		uint32_t *payload = data->payload;
		uint32_t param_id;
		uint32_t param_id_pos = 0;

#ifdef CONFIG_MSM_CSPL
		if (crus_afe_callback(data->payload, data->payload_size) == 0)
			return 0;
#endif
		if (!payload || (data->token >= AFE_MAX_PORTS)) {
			pr_err("%s: Error: size %d payload %pK token %d\n",
				__func__, data->payload_size,
				payload, data->token);
			return -EINVAL;
		}

		if (data->opcode == AFE_PORT_CMDRSP_GET_PARAM_V3)
			param_id_pos = 4;
		else
			param_id_pos = 3;

		if (data->payload_size >= param_id_pos * sizeof(uint32_t))
				param_id = payload[param_id_pos - 1];
		else {
			pr_err("%s: Error: size %d is less than expected\n",
				__func__, data->payload_size);
			return -EINVAL;
		}

		if (param_id == AFE_PARAM_ID_DEV_TIMING_STATS) {
			av_dev_drift_afe_cb_handler(data->opcode, data->payload,
						    data->payload_size);
		} else {
			if (rtac_make_afe_callback(data->payload,
						   data->payload_size))
				return 0;

#ifdef CONFIG_SND_SOC_TFA9874_FOR_DAVI
			if (atomic_read(&this_afe.tfa_state) == 1 &&
				data->payload_size == sizeof(uint32_t)) {

				atomic_set(&this_afe.status, payload[0]);
				if (payload[0])
					atomic_set(&this_afe.state, -1);
				else
					atomic_set(&this_afe.state, 0);

				atomic_set(&this_afe.tfa_state, 0);
				wake_up(&this_afe.wait[data->token]);

				return 0;
			}
#endif /*CONFIG_SND_SOC_TFA9874_FOR_DAVI*/

			if (sp_make_afe_callback(data->opcode, data->payload,
						 data->payload_size))
				return -EINVAL;
		}
		if (afe_token_is_valid(data->token))
			wake_up(&this_afe.wait[data->token]);
		else
			return -EINVAL;
	} else if (data->opcode == ULTRASOUND_OPCODE) {
		if (NULL != data->payload)
			elliptic_process_apr_payload(data->payload);
		else
			pr_err("[EXPORT_SYMBOLLUS]: payload ptr is Invalid");
	} else if (data->opcode == AFE_EVENT_MBHC_DETECTION_SW_WA) {
		msm_aud_evt_notifier_call_chain(SWR_WAKE_IRQ_EVENT, NULL);
	} else if (data->opcode ==
			AFE_CMD_RSP_REMOTE_LPASS_CORE_HW_VOTE_REQUEST) {
		uint32_t *payload = data->payload;

		pr_debug("%s: LPASS_CORE_HW_VOTE_REQUEST handle %d\n",
			 __func__, payload[0]);
		this_afe.lpass_hw_core_client_hdl = payload[0];
		atomic_set(&this_afe.state, 0);
		atomic_set(&this_afe.status, 0);
		wake_up(&this_afe.lpass_core_hw_wait);
	} else if (data->payload_size) {
		uint32_t *payload;
		uint16_t port_id = 0;

		payload = data->payload;
		if (data->opcode == APR_BASIC_RSP_RESULT) {
			if (data->payload_size < (2 * sizeof(uint32_t))) {
				pr_err("%s: Error: size %d is less than expected\n",
					__func__, data->payload_size);
				return -EINVAL;
			}
			pr_debug("%s:opcode = 0x%x cmd = 0x%x status = 0x%x token=%d\n",
				__func__, data->opcode,
				payload[0], payload[1], data->token);
			/* payload[1] contains the error status for response */
			if (payload[1] != 0) {
				atomic_set(&this_afe.status, payload[1]);
				pr_err("%s: cmd = 0x%x returned error = 0x%x\n",
					__func__, payload[0], payload[1]);
			}
			switch (payload[0]) {
			case AFE_PORT_CMD_SET_PARAM_V2:
			case AFE_PORT_CMD_SET_PARAM_V3:
				if (rtac_make_afe_callback(payload,
							   data->payload_size))
					return 0;
			case AFE_PORT_CMD_DEVICE_STOP:
			case AFE_PORT_CMD_DEVICE_START:
			case AFE_PSEUDOPORT_CMD_START:
			case AFE_PSEUDOPORT_CMD_STOP:
			case AFE_SERVICE_CMD_SHARED_MEM_MAP_REGIONS:
			case AFE_SERVICE_CMD_SHARED_MEM_UNMAP_REGIONS:
			case AFE_SERVICE_CMD_UNREGISTER_RT_PORT_DRIVER:
			case AFE_PORTS_CMD_DTMF_CTL:
			case AFE_SVC_CMD_SET_PARAM:
			case AFE_SVC_CMD_SET_PARAM_V2:
			case AFE_PORT_CMD_MOD_EVENT_CFG:
				atomic_set(&this_afe.state, 0);
				if (afe_token_is_valid(data->token))
					wake_up(&this_afe.wait[data->token]);
				else
					return -EINVAL;
				break;
			case AFE_SERVICE_CMD_REGISTER_RT_PORT_DRIVER:
				break;
			case AFE_PORT_DATA_CMD_RT_PROXY_PORT_WRITE_V2:
				port_id = RT_PROXY_PORT_001_TX;
				break;
			case AFE_PORT_DATA_CMD_RT_PROXY_PORT_READ_V2:
				port_id = RT_PROXY_PORT_001_RX;
				break;
			case AFE_CMD_ADD_TOPOLOGIES:
				atomic_set(&this_afe.state, 0);
				if (afe_token_is_valid(data->token))
					wake_up(&this_afe.wait[data->token]);
				else
					return -EINVAL;
				pr_debug("%s: AFE_CMD_ADD_TOPOLOGIES cmd 0x%x\n",
						__func__, payload[1]);
				break;
			case AFE_PORT_CMD_GET_PARAM_V2:
			case AFE_PORT_CMD_GET_PARAM_V3:
				/*
				 * Should only come here if there is an APR
				 * error or malformed APR packet. Otherwise
				 * response will be returned as
				 * AFE_PORT_CMDRSP_GET_PARAM_V2/3
				 */
				pr_debug("%s: AFE Get Param opcode 0x%x token 0x%x src %d dest %d\n",
					__func__, data->opcode, data->token,
					data->src_port, data->dest_port);
				if (payload[1] != 0) {
					pr_err("%s: AFE Get Param failed with error %d\n",
					       __func__, payload[1]);
					if (rtac_make_afe_callback(
						    payload,
						    data->payload_size))
						return 0;
				}
				atomic_set(&this_afe.state, payload[1]);
				if (afe_token_is_valid(data->token))
					wake_up(&this_afe.wait[data->token]);
				else
					return -EINVAL;
				break;
			case AFE_CMD_REMOTE_LPASS_CORE_HW_VOTE_REQUEST:
			case AFE_CMD_REMOTE_LPASS_CORE_HW_DEVOTE_REQUEST:
				atomic_set(&this_afe.state, 0);
				wake_up(&this_afe.lpass_core_hw_wait);
				break;
			case AFE_SVC_CMD_EVENT_CFG:
				atomic_set(&this_afe.state, payload[1]);
				wake_up(&this_afe.wait_wakeup);
				break;
			default:
				pr_err("%s: Unknown cmd 0x%x\n", __func__,
						payload[0]);
				break;
			}
		} else if (data->opcode ==
				AFE_SERVICE_CMDRSP_SHARED_MEM_MAP_REGIONS) {
			pr_debug("%s: mmap_handle: 0x%x, cal index %d\n",
				 __func__, payload[0],
				 atomic_read(&this_afe.mem_map_cal_index));
			if (atomic_read(&this_afe.mem_map_cal_index) != -1)
				atomic_set(&this_afe.mem_map_cal_handles[
					atomic_read(
					&this_afe.mem_map_cal_index)],
					(uint32_t)payload[0]);
			else
				this_afe.mmap_handle = payload[0];
			atomic_set(&this_afe.state, 0);
			if (afe_token_is_valid(data->token))
				wake_up(&this_afe.wait[data->token]);
			else
				return -EINVAL;
		} else if (data->opcode == AFE_EVENT_RT_PROXY_PORT_STATUS) {
			port_id = (uint16_t)(0x0000FFFF & payload[0]);
		} else if (data->opcode == AFE_PORT_MOD_EVENT) {
			u32 flag_dc_presence[2];
			uint32_t *payload = data->payload;
			struct afe_port_mod_evt_rsp_hdr *evt_pl =
				(struct afe_port_mod_evt_rsp_hdr *)payload;

			if (!payload || (data->token >= AFE_MAX_PORTS)) {
				pr_err("%s: Error: size %d payload %pK token %d\n",
					__func__, data->payload_size,
					payload, data->token);
				return -EINVAL;
			}
			if ((evt_pl->module_id == AFE_MODULE_SPEAKER_PROTECTION_V2_EX_VI) &&
			    (evt_pl->event_id == AFE_PORT_SP_DC_DETECTION_EVENT) &&
			    (evt_pl->payload_size == sizeof(flag_dc_presence))) {

				memcpy(&flag_dc_presence,
					(uint8_t *)payload +
					sizeof(struct afe_port_mod_evt_rsp_hdr),
					evt_pl->payload_size);
				if (flag_dc_presence[0] == 1 ||
					flag_dc_presence[1] == 1) {
					afe_notify_dc_presence();
				}
			} else if (evt_pl->port_id == AFE_PORT_ID_PRIMARY_SPDIF_TX) {
				if (this_afe.pri_spdif_tx_cb) {
					this_afe.pri_spdif_tx_cb(data->opcode,
						data->token, data->payload,
						this_afe.pri_spdif_tx_private_data);
				}
				afe_notify_spdif_fmt_update(data->payload);
			} else if (evt_pl->port_id == AFE_PORT_ID_SECONDARY_SPDIF_TX) {
				if (this_afe.sec_spdif_tx_cb) {
					this_afe.sec_spdif_tx_cb(data->opcode,
						data->token, data->payload,
						this_afe.sec_spdif_tx_private_data);
				}
				afe_notify_spdif_fmt_update(data->payload);
			} else {
				pr_debug("%s: mod ID = 0x%x event_id = 0x%x\n",
						__func__, evt_pl->module_id,
						evt_pl->event_id);
			}
		}
		pr_debug("%s: port_id = 0x%x\n", __func__, port_id);
		switch (port_id) {
		case RT_PROXY_PORT_001_TX: {
			if (this_afe.tx_cb) {
				this_afe.tx_cb(data->opcode, data->token,
					data->payload,
					this_afe.tx_private_data);
			}
			break;
		}
		case RT_PROXY_PORT_001_RX: {
			if (this_afe.rx_cb) {
				this_afe.rx_cb(data->opcode, data->token,
					data->payload,
					this_afe.rx_private_data);
			}
			break;
		}
		default:
			pr_debug("%s: default case 0x%x\n", __func__, port_id);
			break;
		}
	}
	return 0;
}

/**
 * afe_get_port_type -
 *        Retrieve AFE port type whether RX or TX
 *
 * @port_id: AFE Port ID number
 *
 * Returns RX/TX type.
 */
int afe_get_port_type(u16 port_id)
{
	int ret = MSM_AFE_PORT_TYPE_RX;

	switch (port_id) {
	case VOICE_RECORD_RX:
	case VOICE_RECORD_TX:
		ret = MSM_AFE_PORT_TYPE_TX;
		break;
	case VOICE_PLAYBACK_TX:
	case VOICE2_PLAYBACK_TX:
		ret = MSM_AFE_PORT_TYPE_RX;
		break;
	default:
		/* Odd numbered ports are TX and Rx are Even numbered */
		if (port_id & 0x1)
			ret = MSM_AFE_PORT_TYPE_TX;
		else
			ret = MSM_AFE_PORT_TYPE_RX;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(afe_get_port_type);

int afe_sizeof_cfg_cmd(u16 port_id)
{
	int ret_size;

	switch (port_id) {
	case PRIMARY_I2S_RX:
	case PRIMARY_I2S_TX:
	case SECONDARY_I2S_RX:
	case SECONDARY_I2S_TX:
	case MI2S_RX:
	case MI2S_TX:
	case AFE_PORT_ID_PRIMARY_MI2S_RX:
	case AFE_PORT_ID_PRIMARY_MI2S_TX:
	case AFE_PORT_ID_QUATERNARY_MI2S_RX:
	case AFE_PORT_ID_QUATERNARY_MI2S_TX:
	case AFE_PORT_ID_QUINARY_MI2S_RX:
	case AFE_PORT_ID_QUINARY_MI2S_TX:
		ret_size = SIZEOF_CFG_CMD(afe_param_id_i2s_cfg);
		break;
	case HDMI_RX:
	case DISPLAY_PORT_RX:
		ret_size =
		SIZEOF_CFG_CMD(afe_param_id_hdmi_multi_chan_audio_cfg);
		break;
	case AFE_PORT_ID_PRIMARY_SPDIF_RX:
	case AFE_PORT_ID_PRIMARY_SPDIF_TX:
	case AFE_PORT_ID_SECONDARY_SPDIF_RX:
	case AFE_PORT_ID_SECONDARY_SPDIF_TX:
		ret_size =
		SIZEOF_CFG_CMD(afe_param_id_spdif_cfg_v2);
		break;
	case SLIMBUS_0_RX:
	case SLIMBUS_0_TX:
	case SLIMBUS_1_RX:
	case SLIMBUS_1_TX:
	case SLIMBUS_2_RX:
	case SLIMBUS_2_TX:
	case SLIMBUS_3_RX:
	case SLIMBUS_3_TX:
	case SLIMBUS_4_RX:
	case SLIMBUS_4_TX:
	case SLIMBUS_5_RX:
	case SLIMBUS_5_TX:
	case SLIMBUS_6_RX:
	case SLIMBUS_6_TX:
	case SLIMBUS_7_RX:
	case SLIMBUS_7_TX:
	case SLIMBUS_8_RX:
	case SLIMBUS_8_TX:
	case SLIMBUS_9_RX:
	case SLIMBUS_9_TX:
		ret_size = SIZEOF_CFG_CMD(afe_param_id_slimbus_cfg);
		break;
	case VOICE_PLAYBACK_TX:
	case VOICE2_PLAYBACK_TX:
	case VOICE_RECORD_RX:
	case VOICE_RECORD_TX:
		ret_size = SIZEOF_CFG_CMD(afe_param_id_pseudo_port_cfg);
		break;
	case RT_PROXY_PORT_001_RX:
	case RT_PROXY_PORT_001_TX:
		ret_size = SIZEOF_CFG_CMD(afe_param_id_rt_proxy_port_cfg);
		break;
	case AFE_PORT_ID_USB_RX:
	case AFE_PORT_ID_USB_TX:
		ret_size = SIZEOF_CFG_CMD(afe_param_id_usb_audio_cfg);
		break;
	case AFE_PORT_ID_WSA_CODEC_DMA_RX_0:
	case AFE_PORT_ID_WSA_CODEC_DMA_TX_0:
	case AFE_PORT_ID_WSA_CODEC_DMA_RX_1:
	case AFE_PORT_ID_WSA_CODEC_DMA_TX_1:
	case AFE_PORT_ID_WSA_CODEC_DMA_TX_2:
	case AFE_PORT_ID_VA_CODEC_DMA_TX_0:
	case AFE_PORT_ID_VA_CODEC_DMA_TX_1:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_0:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_0:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_1:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_1:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_2:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_2:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_3:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_3:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_4:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_4:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_5:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_5:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_6:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_7:
		ret_size = SIZEOF_CFG_CMD(afe_param_id_cdc_dma_cfg_t);
		break;
	case AFE_PORT_ID_PRIMARY_PCM_RX:
	case AFE_PORT_ID_PRIMARY_PCM_TX:
	case AFE_PORT_ID_SECONDARY_PCM_RX:
	case AFE_PORT_ID_SECONDARY_PCM_TX:
	case AFE_PORT_ID_TERTIARY_PCM_RX:
	case AFE_PORT_ID_TERTIARY_PCM_TX:
	case AFE_PORT_ID_QUATERNARY_PCM_RX:
	case AFE_PORT_ID_QUATERNARY_PCM_TX:
	case AFE_PORT_ID_QUINARY_PCM_RX:
	case AFE_PORT_ID_QUINARY_PCM_TX:
	case AFE_PORT_ID_SENARY_PCM_RX:
	case AFE_PORT_ID_SENARY_PCM_TX:
	default:
		pr_debug("%s: default case 0x%x\n", __func__, port_id);
		ret_size = SIZEOF_CFG_CMD(afe_param_id_pcm_cfg);
		break;
	}
	return ret_size;
}

/**
 * afe_q6_interface_prepare -
 *        wrapper API to check Q6 AFE registered to APR otherwise registers
 *
 * Returns 0 on success or error on failure.
 */
int afe_q6_interface_prepare(void)
{
	int ret = 0;

	pr_debug("%s:\n", __func__);

	if (this_afe.apr == NULL) {
		this_afe.apr = apr_register("ADSP", "AFE", afe_callback,
			0xFFFFFFFF, &this_afe);
		if (this_afe.apr == NULL) {
			pr_err("%s: Unable to register AFE\n", __func__);
			ret = -ENETRESET;
		}
		rtac_set_afe_handle(this_afe.apr);
	}
	return ret;
}
EXPORT_SYMBOL(afe_q6_interface_prepare);

/*
 * afe_apr_send_pkt : returns 0 on success, negative otherwise.
 */
static int afe_apr_send_pkt(void *data, wait_queue_head_t *wait)
{
	int ret;

	if (wait)
		atomic_set(&this_afe.state, 1);
	atomic_set(&this_afe.status, 0);
	ret = apr_send_pkt(this_afe.apr, data);
	if (ret > 0) {
		if (wait) {
			ret = wait_event_timeout(*wait,
					(atomic_read(&this_afe.state) == 0),
					msecs_to_jiffies(2 * TIMEOUT_MS));
			if (!ret) {
				ret = -ETIMEDOUT;
			} else if (atomic_read(&this_afe.status) > 0) {
				pr_err("%s: DSP returned error[%s]\n", __func__,
					adsp_err_get_err_str(atomic_read(
					&this_afe.status)));
				ret = adsp_err_get_lnx_err_code(
						atomic_read(&this_afe.status));
			} else {
				ret = 0;
			}
		} else {
			ret = 0;
		}
	} else if (ret == 0) {
		pr_err("%s: packet not transmitted\n", __func__);
		/* apr_send_pkt can return 0 when nothing is transmitted */
		ret = -EINVAL;
	}

	pr_debug("%s: leave %d\n", __func__, ret);
	return ret;
}

#ifdef CONFIG_MSM_CSPL
int afe_apr_send_pkt_crus(void *data, int index, int set)
{
	pr_info("[CSPL] %s: index = %d, set=%d, data = %p\n",
		__func__, index, set, data);

	if (set)
		return afe_apr_send_pkt(data, &this_afe.wait[index]);
	else /* get */
		return afe_apr_send_pkt(data, 0);
}

EXPORT_SYMBOL(afe_apr_send_pkt_crus);
#endif

/* This function shouldn't be called directly. Instead call q6afe_set_params. */
static int q6afe_set_params_v2(u16 port_id, int index,
			       struct mem_mapping_hdr *mem_hdr,
			       u8 *packed_param_data, u32 packed_data_size)
{
	struct afe_port_cmd_set_param_v2 *set_param = NULL;
	uint32_t size = sizeof(struct afe_port_cmd_set_param_v2);
	int rc = 0;

	if (packed_param_data != NULL)
		size += packed_data_size;
	set_param = kzalloc(size, GFP_KERNEL);
	if (set_param == NULL)
		return -ENOMEM;

	set_param->apr_hdr.hdr_field =
		APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, APR_HDR_LEN(APR_HDR_SIZE),
			      APR_PKT_VER);
	set_param->apr_hdr.pkt_size = size;
	set_param->apr_hdr.src_port = 0;
	set_param->apr_hdr.dest_port = 0;
	set_param->apr_hdr.token = index;
	set_param->apr_hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	set_param->port_id = port_id;
	if (packed_data_size > U16_MAX) {
		pr_err("%s: Invalid data size for set params V2 %d\n", __func__,
		       packed_data_size);
		rc = -EINVAL;
		goto done;
	}
	set_param->payload_size = packed_data_size;
	if (mem_hdr != NULL) {
		set_param->mem_hdr = *mem_hdr;
	} else if (packed_param_data != NULL) {
		memcpy(&set_param->param_data, packed_param_data,
		       packed_data_size);
	} else {
		pr_err("%s: Both memory header and param data are NULL\n",
		       __func__);
		rc = -EINVAL;
		goto done;
	}

	rc = afe_apr_send_pkt(set_param, &this_afe.wait[index]);
done:
	kfree(set_param);
	return rc;
}

/* This function shouldn't be called directly. Instead call q6afe_set_params. */
static int q6afe_set_params_v3(u16 port_id, int index,
			       struct mem_mapping_hdr *mem_hdr,
			       u8 *packed_param_data, u32 packed_data_size)
{
	struct afe_port_cmd_set_param_v3 *set_param = NULL;
	uint32_t size = sizeof(struct afe_port_cmd_set_param_v3);
	int rc = 0;

	if (packed_param_data != NULL)
		size += packed_data_size;
	set_param = kzalloc(size, GFP_KERNEL);
	if (set_param == NULL)
		return -ENOMEM;

	set_param->apr_hdr.hdr_field =
		APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, APR_HDR_LEN(APR_HDR_SIZE),
			      APR_PKT_VER);
	set_param->apr_hdr.pkt_size = size;
	set_param->apr_hdr.src_port = 0;
	set_param->apr_hdr.dest_port = 0;
	set_param->apr_hdr.token = index;
	set_param->apr_hdr.opcode = AFE_PORT_CMD_SET_PARAM_V3;
	set_param->port_id = port_id;
	set_param->payload_size = packed_data_size;
	if (mem_hdr != NULL) {
		set_param->mem_hdr = *mem_hdr;
	} else if (packed_param_data != NULL) {
		memcpy(&set_param->param_data, packed_param_data,
		       packed_data_size);
	} else {
		pr_err("%s: Both memory header and param data are NULL\n",
		       __func__);
		rc = -EINVAL;
		goto done;
	}

	rc = afe_apr_send_pkt(set_param, &this_afe.wait[index]);
done:
	kfree(set_param);
	return rc;
}

static int q6afe_set_params(u16 port_id, int index,
			    struct mem_mapping_hdr *mem_hdr,
			    u8 *packed_param_data, u32 packed_data_size)
{
	int ret = 0;

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	port_id = q6audio_get_port_id(port_id);
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: Not a valid port id = 0x%x ret %d\n", __func__,
		       port_id, ret);
		return -EINVAL;
	}

	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid\n", __func__, index);
		return -EINVAL;
	}

	if (q6common_is_instance_id_supported())
		return q6afe_set_params_v3(port_id, index, mem_hdr,
					   packed_param_data, packed_data_size);
	else
		return q6afe_set_params_v2(port_id, index, mem_hdr,
					   packed_param_data, packed_data_size);
}

static int q6afe_pack_and_set_param_in_band(u16 port_id, int index,
					    struct param_hdr_v3 param_hdr,
					    u8 *param_data)
{
	u8 *packed_param_data = NULL;
	int packed_data_size = sizeof(union param_hdrs) + param_hdr.param_size;
	int ret;

	packed_param_data = kzalloc(packed_data_size, GFP_KERNEL);
	if (packed_param_data == NULL)
		return -ENOMEM;

	ret = q6common_pack_pp_params(packed_param_data, &param_hdr, param_data,
				      &packed_data_size);
	if (ret) {
		pr_err("%s: Failed to pack param header and data, error %d\n",
		       __func__, ret);
		goto fail_cmd;
	}

	ret = q6afe_set_params(port_id, index, NULL, packed_param_data,
			       packed_data_size);

fail_cmd:
	kfree(packed_param_data);
	return ret;
}

static int q6afe_set_aanc_level(void)
{
	struct param_hdr_v3 param_hdr;
	struct afe_param_id_aanc_noise_reduction aanc_noise_level;
	int ret = 0;
	uint16_t tx_port = 0;

	if (!this_afe.aanc_info.aanc_active)
		return -EINVAL;

	pr_debug("%s: level: %d\n", __func__, this_afe.aanc_info.level);
	memset(&aanc_noise_level, 0, sizeof(aanc_noise_level));
	aanc_noise_level.minor_version = 1;
	aanc_noise_level.ad_beta = this_afe.aanc_info.level;

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AFE_MODULE_AANC;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_AANC_NOISE_REDUCTION;
	param_hdr.param_size = sizeof(struct afe_param_id_aanc_noise_reduction);

	tx_port = this_afe.aanc_info.aanc_tx_port;
	ret = q6afe_pack_and_set_param_in_band(tx_port,
					       q6audio_get_port_index(tx_port),
					       param_hdr,
					       (u8 *) &aanc_noise_level);
	if (ret)
		pr_err("%s: AANC noise level enable failed for tx_port 0x%x ret %d\n",
			__func__, tx_port, ret);
	return ret;
}

/**
 * afe_set_aanc_noise_level - controls aanc noise reduction strength
 *
 * @level: Noise level to be controlled
 *
 * Returns 0 on success or error on failure.
 */
int afe_set_aanc_noise_level(int level)
{
	int ret = 0;

	if (this_afe.aanc_info.level == level)
		return ret;

	mutex_lock(&this_afe.afe_cmd_lock);
	this_afe.aanc_info.level = level;
	ret = q6afe_set_aanc_level();
	mutex_unlock(&this_afe.afe_cmd_lock);

	return ret;
}
EXPORT_SYMBOL(afe_set_aanc_noise_level);

/* This function shouldn't be called directly. Instead call q6afe_get_param. */
static int q6afe_get_params_v2(u16 port_id, int index,
			       struct mem_mapping_hdr *mem_hdr,
			       struct param_hdr_v3 *param_hdr)
{
	struct afe_port_cmd_get_param_v2 afe_get_param;
	u32 param_size = param_hdr->param_size;

	memset(&afe_get_param, 0, sizeof(afe_get_param));
	afe_get_param.apr_hdr.hdr_field =
		APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, APR_HDR_LEN(APR_HDR_SIZE),
			      APR_PKT_VER);
	afe_get_param.apr_hdr.pkt_size = sizeof(afe_get_param) + param_size;
	afe_get_param.apr_hdr.src_port = 0;
	afe_get_param.apr_hdr.dest_port = 0;
	afe_get_param.apr_hdr.token = index;
	afe_get_param.apr_hdr.opcode = AFE_PORT_CMD_GET_PARAM_V2;
	afe_get_param.port_id = port_id;
	afe_get_param.payload_size = sizeof(struct param_hdr_v1) + param_size;
	if (mem_hdr != NULL)
		afe_get_param.mem_hdr = *mem_hdr;
	/* Set MID and PID in command */
	afe_get_param.module_id = param_hdr->module_id;
	afe_get_param.param_id = param_hdr->param_id;
	/* Set param header in payload */
	afe_get_param.param_hdr.module_id = param_hdr->module_id;
	afe_get_param.param_hdr.param_id = param_hdr->param_id;
	afe_get_param.param_hdr.param_size = param_size;

	return afe_apr_send_pkt(&afe_get_param, &this_afe.wait[index]);
}

/* This function shouldn't be called directly. Instead call q6afe_get_param. */
static int q6afe_get_params_v3(u16 port_id, int index,
			       struct mem_mapping_hdr *mem_hdr,
			       struct param_hdr_v3 *param_hdr)
{
	struct afe_port_cmd_get_param_v3 afe_get_param;

	memset(&afe_get_param, 0, sizeof(afe_get_param));
	afe_get_param.apr_hdr.hdr_field =
		APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, APR_HDR_LEN(APR_HDR_SIZE),
			      APR_PKT_VER);
	afe_get_param.apr_hdr.pkt_size = sizeof(afe_get_param);
	afe_get_param.apr_hdr.src_port = 0;
	afe_get_param.apr_hdr.dest_port = 0;
	afe_get_param.apr_hdr.token = index;
	afe_get_param.apr_hdr.opcode = AFE_PORT_CMD_GET_PARAM_V3;
	afe_get_param.port_id = port_id;
	if (mem_hdr != NULL)
		afe_get_param.mem_hdr = *mem_hdr;
	/* Set param header in command, no payload in V3 */
	afe_get_param.param_hdr = *param_hdr;

	return afe_apr_send_pkt(&afe_get_param, &this_afe.wait[index]);
}

/*
 * Calling functions copy param data directly from this_afe. Do not copy data
 * back to caller here.
 */
static int q6afe_get_params(u16 port_id, struct mem_mapping_hdr *mem_hdr,
			    struct param_hdr_v3 *param_hdr)
{
	int index;
	int ret;

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	port_id = q6audio_get_port_id(port_id);
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: Not a valid port id = 0x%x ret %d\n", __func__,
		       port_id, ret);
		return -EINVAL;
	}

	index = q6audio_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid\n", __func__, index);
		return -EINVAL;
	}

	if (q6common_is_instance_id_supported())
		return q6afe_get_params_v3(port_id, index, NULL, param_hdr);
	else
		return q6afe_get_params_v2(port_id, index, NULL, param_hdr);
}

/*
 * This function shouldn't be called directly. Instead call
 * q6afe_svc_set_params.
 */
static int q6afe_svc_set_params_v1(int index, struct mem_mapping_hdr *mem_hdr,
				   u8 *packed_param_data, u32 packed_data_size)
{
	struct afe_svc_cmd_set_param_v1 *svc_set_param = NULL;
	uint32_t size = sizeof(struct afe_svc_cmd_set_param_v1);
	int rc = 0;

	if (packed_param_data != NULL)
		size += packed_data_size;
	svc_set_param = kzalloc(size, GFP_KERNEL);
	if (svc_set_param == NULL)
		return -ENOMEM;

	svc_set_param->apr_hdr.hdr_field =
		APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, APR_HDR_LEN(APR_HDR_SIZE),
			      APR_PKT_VER);
	svc_set_param->apr_hdr.pkt_size = size;
	svc_set_param->apr_hdr.src_port = 0;
	svc_set_param->apr_hdr.dest_port = 0;
	svc_set_param->apr_hdr.token = index;
	svc_set_param->apr_hdr.opcode = AFE_SVC_CMD_SET_PARAM;
	svc_set_param->payload_size = packed_data_size;

	if (mem_hdr != NULL) {
		/* Out of band case. */
		svc_set_param->mem_hdr = *mem_hdr;
	} else if (packed_param_data != NULL) {
		/* In band case. */
		memcpy(&svc_set_param->param_data, packed_param_data,
		       packed_data_size);
	} else {
		pr_err("%s: Both memory header and param data are NULL\n",
		       __func__);
		rc = -EINVAL;
		goto done;
	}

	rc = afe_apr_send_pkt(svc_set_param, &this_afe.wait[index]);
done:
	kfree(svc_set_param);
	return rc;
}

/*
 * This function shouldn't be called directly. Instead call
 * q6afe_svc_set_params.
 */
static int q6afe_svc_set_params_v2(int index, struct mem_mapping_hdr *mem_hdr,
				   u8 *packed_param_data, u32 packed_data_size)
{
	struct afe_svc_cmd_set_param_v2 *svc_set_param = NULL;
	uint16_t size = sizeof(struct afe_svc_cmd_set_param_v2);
	int rc = 0;

	if (packed_param_data != NULL)
		size += packed_data_size;
	svc_set_param = kzalloc(size, GFP_KERNEL);
	if (svc_set_param == NULL)
		return -ENOMEM;

	svc_set_param->apr_hdr.hdr_field =
		APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, APR_HDR_LEN(APR_HDR_SIZE),
			      APR_PKT_VER);
	svc_set_param->apr_hdr.pkt_size = size;
	svc_set_param->apr_hdr.src_port = 0;
	svc_set_param->apr_hdr.dest_port = 0;
	svc_set_param->apr_hdr.token = index;
	svc_set_param->apr_hdr.opcode = AFE_SVC_CMD_SET_PARAM_V2;
	svc_set_param->payload_size = packed_data_size;

	if (mem_hdr != NULL) {
		/* Out of band case. */
		svc_set_param->mem_hdr = *mem_hdr;
	} else if (packed_param_data != NULL) {
		/* In band case. */
		memcpy(&svc_set_param->param_data, packed_param_data,
		       packed_data_size);
	} else {
		pr_err("%s: Both memory header and param data are NULL\n",
		       __func__);
		rc = -EINVAL;
		goto done;
	}

	rc = afe_apr_send_pkt(svc_set_param, &this_afe.wait[index]);
done:
	kfree(svc_set_param);
	return rc;
}

static int q6afe_svc_set_params(int index, struct mem_mapping_hdr *mem_hdr,
				u8 *packed_param_data, u32 packed_data_size)
{
	int ret;

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	if (q6common_is_instance_id_supported())
		return q6afe_svc_set_params_v2(index, mem_hdr,
					       packed_param_data,
					       packed_data_size);
	else
		return q6afe_svc_set_params_v1(index, mem_hdr,
					       packed_param_data,
					       packed_data_size);
}

static int q6afe_svc_pack_and_set_param_in_band(int index,
						struct param_hdr_v3 param_hdr,
						u8 *param_data)
{
	u8 *packed_param_data = NULL;
	u32 packed_data_size =
		sizeof(struct param_hdr_v3) + param_hdr.param_size;
	int ret = 0;

	packed_param_data = kzalloc(packed_data_size, GFP_KERNEL);
	if (!packed_param_data)
		return -ENOMEM;

	ret = q6common_pack_pp_params(packed_param_data, &param_hdr, param_data,
				      &packed_data_size);
	if (ret) {
		pr_err("%s: Failed to pack parameter header and data, error %d\n",
		       __func__, ret);
		goto done;
	}

	ret = q6afe_svc_set_params(index, NULL, packed_param_data,
				   packed_data_size);

done:
	kfree(packed_param_data);
	return ret;
}

static int afe_send_cal_block(u16 port_id, struct cal_block_data *cal_block)
{
	struct mem_mapping_hdr mem_hdr;
	int payload_size = 0;
	int result = 0;

	memset(&mem_hdr, 0, sizeof(mem_hdr));

	if (!cal_block) {
		pr_debug("%s: No AFE cal to send!\n", __func__);
		result = -EINVAL;
		goto done;
	}
	if (cal_block->cal_data.size <= 0) {
		pr_debug("%s: AFE cal has invalid size!\n", __func__);
		result = -EINVAL;
		goto done;
	}

	payload_size = cal_block->cal_data.size;
	mem_hdr.data_payload_addr_lsw =
		lower_32_bits(cal_block->cal_data.paddr);
	mem_hdr.data_payload_addr_msw =
		msm_audio_populate_upper_32_bits(cal_block->cal_data.paddr);
	mem_hdr.mem_map_handle = cal_block->map_data.q6map_handle;

	pr_debug("%s: AFE cal sent for device port = 0x%x, cal size = %zd, cal addr = 0x%pK\n",
		__func__, port_id,
		cal_block->cal_data.size, &cal_block->cal_data.paddr);

	result = q6afe_set_params(port_id, q6audio_get_port_index(port_id),
				  &mem_hdr, NULL, payload_size);
	if (result)
		pr_err("%s: AFE cal for port 0x%x failed %d\n",
		       __func__, port_id, result);

done:
	return result;
}


static int afe_send_custom_topology_block(struct cal_block_data *cal_block)
{
	int	result = 0;
	int	index = 0;
	struct cmd_set_topologies afe_cal;

	if (!cal_block) {
		pr_err("%s: No AFE SVC cal to send!\n", __func__);
		return -EINVAL;
	}
	if (cal_block->cal_data.size <= 0) {
		pr_err("%s: AFE SVC cal has invalid size: %zd!\n",
		__func__, cal_block->cal_data.size);
		return -EINVAL;
	}

	afe_cal.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	afe_cal.hdr.pkt_size = sizeof(afe_cal);
	afe_cal.hdr.src_port = 0;
	afe_cal.hdr.dest_port = 0;
	afe_cal.hdr.token = index;
	afe_cal.hdr.opcode = AFE_CMD_ADD_TOPOLOGIES;

	afe_cal.payload_size = cal_block->cal_data.size;
	afe_cal.payload_addr_lsw =
		lower_32_bits(cal_block->cal_data.paddr);
	afe_cal.payload_addr_msw =
		msm_audio_populate_upper_32_bits(cal_block->cal_data.paddr);
	afe_cal.mem_map_handle = cal_block->map_data.q6map_handle;

	pr_debug("%s:cmd_id:0x%x calsize:%zd memmap_hdl:0x%x caladdr:0x%pK",
		__func__, AFE_CMD_ADD_TOPOLOGIES, cal_block->cal_data.size,
		afe_cal.mem_map_handle, &cal_block->cal_data.paddr);

	result = afe_apr_send_pkt(&afe_cal, &this_afe.wait[index]);
	if (result)
		pr_err("%s: AFE send topology for command 0x%x failed %d\n",
		       __func__, AFE_CMD_ADD_TOPOLOGIES, result);

	return result;
}

static void afe_send_custom_topology(void)
{
	struct cal_block_data   *cal_block = NULL;
	int cal_index = AFE_CUST_TOPOLOGY_CAL;
	int ret;

	if (this_afe.cal_data[cal_index] == NULL) {
		pr_err("%s: cal_index %d not allocated!\n",
			__func__, cal_index);
		return;
	}
	mutex_lock(&this_afe.cal_data[cal_index]->lock);

	if (!this_afe.set_custom_topology)
		goto unlock;
	this_afe.set_custom_topology = 0;
	cal_block = cal_utils_get_only_cal_block(this_afe.cal_data[cal_index]);
	if (cal_block == NULL || cal_utils_is_cal_stale(cal_block)) {
		pr_err("%s cal_block not found!!\n", __func__);
		goto unlock;
	}

	pr_debug("%s: Sending cal_index cal %d\n", __func__, cal_index);

	ret = remap_cal_data(cal_block, cal_index);
	if (ret) {
		pr_err("%s: Remap_cal_data failed for cal %d!\n",
			__func__, cal_index);
		goto unlock;
	}
	ret = afe_send_custom_topology_block(cal_block);
	if (ret < 0) {
		pr_err("%s: No cal sent for cal_index %d! ret %d\n",
			__func__, cal_index, ret);
		goto unlock;
	}
	pr_debug("%s:sent custom topology for AFE\n", __func__);
unlock:
	mutex_unlock(&this_afe.cal_data[cal_index]->lock);
}

static int afe_spk_ramp_dn_cfg(int port)
{
	struct param_hdr_v3 param_info;
	int ret = -EINVAL;

	memset(&param_info, 0, sizeof(param_info));

	if (afe_get_port_type(port) != MSM_AFE_PORT_TYPE_RX) {
		pr_debug("%s: port doesn't match 0x%x\n", __func__, port);
		return 0;
	}
	if (this_afe.prot_cfg.mode == MSM_SPKR_PROT_DISABLED ||
		(this_afe.vi_rx_port != port)) {
		pr_debug("%s: spkr protection disabled port 0x%x %d 0x%x\n",
				__func__, port, ret, this_afe.vi_rx_port);
		return 0;
	}
	param_info.module_id = AFE_MODULE_FB_SPKR_PROT_V2_RX;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = AFE_PARAM_ID_FBSP_PTONE_RAMP_CFG;
	param_info.param_size = 0;

	ret = q6afe_pack_and_set_param_in_band(port,
					       q6audio_get_port_index(port),
					       param_info, NULL);
	if (ret) {
		pr_err("%s: Failed to set speaker ramp duration param, err %d\n",
		       __func__, ret);
		goto fail_cmd;
	}

	/* dsp needs atleast 15ms to ramp down pilot tone*/
	usleep_range(15000, 15010);
	ret = 0;
fail_cmd:
	pr_debug("%s: config.pdata.param_id 0x%x status %d\n", __func__,
		 param_info.param_id, ret);
	return ret;
}

static int afe_spk_prot_prepare(int src_port, int dst_port, int param_id,
				union afe_spkr_prot_config *prot_config)
{
	struct param_hdr_v3 param_info;
	int ret = -EINVAL;

	memset(&param_info, 0, sizeof(param_info));

	ret = q6audio_validate_port(src_port);
	if (ret < 0) {
		pr_err("%s: Invalid src port 0x%x ret %d", __func__, src_port,
		       ret);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = q6audio_validate_port(dst_port);
	if (ret < 0) {
		pr_err("%s: Invalid dst port 0x%x ret %d", __func__,
				dst_port, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	switch (param_id) {
	case AFE_PARAM_ID_FBSP_MODE_RX_CFG:
	case AFE_PARAM_ID_SP_RX_LIMITER_TH:
		param_info.module_id = AFE_MODULE_FB_SPKR_PROT_V2_RX;
		break;
	case AFE_PARAM_ID_FEEDBACK_PATH_CFG:
		this_afe.vi_tx_port = src_port;
		this_afe.vi_rx_port = dst_port;
		param_info.module_id = AFE_MODULE_FEEDBACK;
		break;
	/*
	 * AFE_PARAM_ID_SPKR_CALIB_VI_PROC_CFG_V2 is same as
	 * AFE_PARAM_ID_SP_V2_TH_VI_MODE_CFG. V_VALI_CFG uses
	 * same module TH_VI.
	 */
	case AFE_PARAM_ID_SPKR_CALIB_VI_PROC_CFG_V2:
	case AFE_PARAM_ID_SP_V2_TH_VI_FTM_CFG:
	case AFE_PARAM_ID_SP_V2_TH_VI_V_VALI_CFG:
		param_info.module_id = AFE_MODULE_SPEAKER_PROTECTION_V2_TH_VI;
		break;
	case AFE_PARAM_ID_SP_V2_EX_VI_MODE_CFG:
	case AFE_PARAM_ID_SP_V2_EX_VI_FTM_CFG:
		param_info.module_id = AFE_MODULE_SPEAKER_PROTECTION_V2_EX_VI;
		break;
#ifdef CONFIG_SND_SOC_TFA9874_FOR_DAVI
	case AFE_PARAM_ID_TFADSP_RX_CFG:
	case AFE_PARAM_ID_TFADSP_RX_SET_BYPASS:
		param_info.module_id = AFE_MODULE_ID_TFADSP_RX;
		break;
	case AFE_PARAM_ID_TFADSP_TX_SET_ENABLE:
		param_info.module_id = AFE_MODULE_ID_TFADSP_TX;
		break;
#endif	/*CONFIG_SND_SOC_TFA9874_FOR_DAVI*/
	default:
		pr_err("%s: default case 0x%x\n", __func__, param_id);
		goto fail_cmd;
	}

	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = param_id;
	param_info.param_size = sizeof(union afe_spkr_prot_config);

	ret = q6afe_pack_and_set_param_in_band(src_port,
					       q6audio_get_port_index(src_port),
					       param_info, (u8 *) prot_config);
	if (ret)
		pr_err("%s: port = 0x%x param = 0x%x failed %d\n", __func__,
		       src_port, param_id, ret);

fail_cmd:
	pr_debug("%s: config.pdata.param_id 0x%x status %d 0x%x\n", __func__,
		 param_info.param_id, ret, src_port);
	return ret;
}

static int afe_spkr_prot_reg_event_cfg(u16 port_id)
{
	struct afe_port_cmd_event_cfg *config;
	struct afe_port_cmd_mod_evt_cfg_payload pl;
	int index;
	int ret;
	int num_events = 1;
	int cmd_size = sizeof(struct afe_port_cmd_event_cfg) +
		(num_events * sizeof(struct afe_port_cmd_mod_evt_cfg_payload));

	config = kzalloc(cmd_size, GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	index = q6audio_get_port_index(port_id);
	if (index < 0) {
		pr_err("%s: Invalid index number: %d\n", __func__, index);
		ret = -EINVAL;
		goto fail_idx;
	}

	memset(&pl, 0, sizeof(pl));
	pl.module_id = AFE_MODULE_SPEAKER_PROTECTION_V2_EX_VI;
	pl.event_id = AFE_PORT_SP_DC_DETECTION_EVENT;
	pl.reg_flag = AFE_MODULE_REGISTER_EVENT_FLAG;


	config->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	config->hdr.pkt_size = cmd_size;
	config->hdr.src_port = 0;
	config->hdr.dest_port = 0;
	config->hdr.token = index;

	config->hdr.opcode = AFE_PORT_CMD_MOD_EVENT_CFG;
	config->port_id = q6audio_get_port_id(port_id);
	config->num_events = num_events;
	config->version = 1;
	memcpy(config->payload, &pl, sizeof(pl));
	atomic_set(&this_afe.state, 1);
	atomic_set(&this_afe.status, 0);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) config);
	if (ret < 0) {
		pr_err("%s: port = 0x%x failed %d\n",
			__func__, port_id, ret);
		goto fail_cmd;
	}
	ret = wait_event_timeout(this_afe.wait[index],
		(atomic_read(&this_afe.state) == 0),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	if (atomic_read(&this_afe.status) > 0) {
		pr_err("%s: config cmd failed [%s]\n",
			__func__, adsp_err_get_err_str(
			atomic_read(&this_afe.status)));
		ret = adsp_err_get_lnx_err_code(
				atomic_read(&this_afe.status));
		goto fail_idx;
	}
	ret = 0;
fail_cmd:
	pr_debug("%s: config.opcode 0x%x status %d\n",
		__func__, config->hdr.opcode, ret);

fail_idx:
	kfree(config);
	return ret;
}

afe_ultrasound_state_t elus_afe = {
	.ptr_apr= &this_afe.apr,
	.ptr_status= &this_afe.status,
	.ptr_state= &this_afe.state,
	.ptr_wait= this_afe.wait,
	.timeout_ms= TIMEOUT_MS,
};
EXPORT_SYMBOL(elus_afe);

static void afe_send_cal_spkr_prot_tx(int port_id)
{
	union afe_spkr_prot_config afe_spk_config;

	if (this_afe.cal_data[AFE_FB_SPKR_PROT_CAL] == NULL ||
	    this_afe.cal_data[AFE_FB_SPKR_PROT_TH_VI_CAL] == NULL ||
	    this_afe.cal_data[AFE_FB_SPKR_PROT_EX_VI_CAL] == NULL)
		return;

	mutex_lock(&this_afe.cal_data[AFE_FB_SPKR_PROT_CAL]->lock);
	if ((this_afe.prot_cfg.mode != MSM_SPKR_PROT_DISABLED) &&
		(this_afe.vi_tx_port == port_id)) {
		if (this_afe.prot_cfg.mode ==
			MSM_SPKR_PROT_CALIBRATION_IN_PROGRESS) {
			afe_spk_config.vi_proc_cfg.operation_mode =
					Q6AFE_MSM_SPKR_CALIBRATION;
			afe_spk_config.vi_proc_cfg.quick_calib_flag =
					this_afe.prot_cfg.quick_calib_flag;
		} else {
			afe_spk_config.vi_proc_cfg.operation_mode =
					Q6AFE_MSM_SPKR_PROCESSING;
		}

		if (this_afe.th_ftm_cfg.mode == MSM_SPKR_PROT_IN_FTM_MODE)
			afe_spk_config.vi_proc_cfg.operation_mode =
					    Q6AFE_MSM_SPKR_FTM_MODE;
		else if (this_afe.v_vali_cfg.mode ==
					MSM_SPKR_PROT_IN_V_VALI_MODE)
			afe_spk_config.vi_proc_cfg.operation_mode =
					    Q6AFE_MSM_SPKR_V_VALI_MODE;
		afe_spk_config.vi_proc_cfg.minor_version = 1;
		afe_spk_config.vi_proc_cfg.r0_cali_q24[SP_V2_SPKR_1] =
			(uint32_t) this_afe.prot_cfg.r0[SP_V2_SPKR_1];
		afe_spk_config.vi_proc_cfg.r0_cali_q24[SP_V2_SPKR_2] =
			(uint32_t) this_afe.prot_cfg.r0[SP_V2_SPKR_2];
		afe_spk_config.vi_proc_cfg.t0_cali_q6[SP_V2_SPKR_1] =
			(uint32_t) this_afe.prot_cfg.t0[SP_V2_SPKR_1];
		afe_spk_config.vi_proc_cfg.t0_cali_q6[SP_V2_SPKR_2] =
			(uint32_t) this_afe.prot_cfg.t0[SP_V2_SPKR_2];
		if (this_afe.prot_cfg.mode != MSM_SPKR_PROT_NOT_CALIBRATED) {
			struct asm_spkr_calib_vi_proc_cfg *vi_proc_cfg;

			vi_proc_cfg = &afe_spk_config.vi_proc_cfg;
			vi_proc_cfg->r0_t0_selection_flag[SP_V2_SPKR_1] =
					    USE_CALIBRATED_R0TO;
			vi_proc_cfg->r0_t0_selection_flag[SP_V2_SPKR_2] =
					    USE_CALIBRATED_R0TO;
		} else {
			struct asm_spkr_calib_vi_proc_cfg *vi_proc_cfg;

			vi_proc_cfg = &afe_spk_config.vi_proc_cfg;
			vi_proc_cfg->r0_t0_selection_flag[SP_V2_SPKR_1] =
							    USE_SAFE_R0TO;
			vi_proc_cfg->r0_t0_selection_flag[SP_V2_SPKR_2] =
							    USE_SAFE_R0TO;
		}
		if (afe_spk_prot_prepare(port_id, 0,
			AFE_PARAM_ID_SPKR_CALIB_VI_PROC_CFG_V2,
				&afe_spk_config))
			pr_err("%s: SPKR_CALIB_VI_PROC_CFG failed\n",
				__func__);
	}
	mutex_unlock(&this_afe.cal_data[AFE_FB_SPKR_PROT_CAL]->lock);

	mutex_lock(&this_afe.cal_data[AFE_FB_SPKR_PROT_TH_VI_CAL]->lock);
	if ((this_afe.th_ftm_cfg.mode == MSM_SPKR_PROT_IN_FTM_MODE) &&
	    (this_afe.vi_tx_port == port_id)) {
		afe_spk_config.th_vi_ftm_cfg.minor_version = 1;
		afe_spk_config.th_vi_ftm_cfg.wait_time_ms[SP_V2_SPKR_1] =
			this_afe.th_ftm_cfg.wait_time[SP_V2_SPKR_1];
		afe_spk_config.th_vi_ftm_cfg.wait_time_ms[SP_V2_SPKR_2] =
			this_afe.th_ftm_cfg.wait_time[SP_V2_SPKR_2];
		afe_spk_config.th_vi_ftm_cfg.ftm_time_ms[SP_V2_SPKR_1] =
			this_afe.th_ftm_cfg.ftm_time[SP_V2_SPKR_1];
		afe_spk_config.th_vi_ftm_cfg.ftm_time_ms[SP_V2_SPKR_2] =
			this_afe.th_ftm_cfg.ftm_time[SP_V2_SPKR_2];

		if (afe_spk_prot_prepare(port_id, 0,
					 AFE_PARAM_ID_SP_V2_TH_VI_FTM_CFG,
					 &afe_spk_config))
			pr_err("%s: th vi ftm cfg failed\n", __func__);
		this_afe.th_ftm_cfg.mode = MSM_SPKR_PROT_DISABLED;
	} else if ((this_afe.v_vali_cfg.mode ==
			MSM_SPKR_PROT_IN_V_VALI_MODE) &&
		   (this_afe.vi_tx_port == port_id)) {
		afe_spk_config.th_vi_v_vali_cfg.minor_version = 1;
		afe_spk_config.th_vi_v_vali_cfg.wait_time_ms[SP_V2_SPKR_1] =
			this_afe.v_vali_cfg.wait_time[SP_V2_SPKR_1];
		afe_spk_config.th_vi_v_vali_cfg.wait_time_ms[SP_V2_SPKR_2] =
			this_afe.v_vali_cfg.wait_time[SP_V2_SPKR_2];
		afe_spk_config.th_vi_v_vali_cfg.vali_time_ms[SP_V2_SPKR_1] =
			this_afe.v_vali_cfg.vali_time[SP_V2_SPKR_1];
		afe_spk_config.th_vi_v_vali_cfg.vali_time_ms[SP_V2_SPKR_2] =
			this_afe.v_vali_cfg.vali_time[SP_V2_SPKR_2];

		if (afe_spk_prot_prepare(port_id, 0,
					 AFE_PARAM_ID_SP_V2_TH_VI_V_VALI_CFG,
					 &afe_spk_config))
			pr_err("%s: th vi v-vali cfg failed\n", __func__);

		this_afe.v_vali_cfg.mode = MSM_SPKR_PROT_DISABLED;
	}
	mutex_unlock(&this_afe.cal_data[AFE_FB_SPKR_PROT_TH_VI_CAL]->lock);

	mutex_lock(&this_afe.cal_data[AFE_FB_SPKR_PROT_EX_VI_CAL]->lock);
	if ((this_afe.ex_ftm_cfg.mode == MSM_SPKR_PROT_IN_FTM_MODE) &&
	    (this_afe.vi_tx_port == port_id)) {
		afe_spk_config.ex_vi_mode_cfg.minor_version = 1;
		afe_spk_config.ex_vi_mode_cfg.operation_mode =
						Q6AFE_MSM_SPKR_FTM_MODE;
		if (afe_spk_prot_prepare(port_id, 0,
					 AFE_PARAM_ID_SP_V2_EX_VI_MODE_CFG,
					 &afe_spk_config))
			pr_err("%s: ex vi mode cfg failed\n", __func__);

		afe_spk_config.ex_vi_ftm_cfg.minor_version = 1;
		afe_spk_config.ex_vi_ftm_cfg.wait_time_ms[SP_V2_SPKR_1] =
			this_afe.ex_ftm_cfg.wait_time[SP_V2_SPKR_1];
		afe_spk_config.ex_vi_ftm_cfg.wait_time_ms[SP_V2_SPKR_2] =
			this_afe.ex_ftm_cfg.wait_time[SP_V2_SPKR_2];
		afe_spk_config.ex_vi_ftm_cfg.ftm_time_ms[SP_V2_SPKR_1] =
			this_afe.ex_ftm_cfg.ftm_time[SP_V2_SPKR_1];
		afe_spk_config.ex_vi_ftm_cfg.ftm_time_ms[SP_V2_SPKR_2] =
			this_afe.ex_ftm_cfg.ftm_time[SP_V2_SPKR_2];

		if (afe_spk_prot_prepare(port_id, 0,
					 AFE_PARAM_ID_SP_V2_EX_VI_FTM_CFG,
					 &afe_spk_config))
			pr_err("%s: ex vi ftm cfg failed\n", __func__);
		this_afe.ex_ftm_cfg.mode = MSM_SPKR_PROT_DISABLED;
	}
	mutex_unlock(&this_afe.cal_data[AFE_FB_SPKR_PROT_EX_VI_CAL]->lock);

	/* Register for DC detection event if speaker protection is enabled */
	if (this_afe.prot_cfg.mode != MSM_SPKR_PROT_DISABLED &&
		(this_afe.vi_tx_port == port_id)) {
		afe_spkr_prot_reg_event_cfg(port_id);
	}
}

static void afe_send_cal_spkr_prot_rx(int port_id)
{
	union afe_spkr_prot_config afe_spk_config;
	union afe_spkr_prot_config afe_spk_limiter_config;

	if (this_afe.cal_data[AFE_FB_SPKR_PROT_CAL] == NULL)
		goto done;

	mutex_lock(&this_afe.cal_data[AFE_FB_SPKR_PROT_CAL]->lock);

	if (this_afe.prot_cfg.mode != MSM_SPKR_PROT_DISABLED &&
		(this_afe.vi_rx_port == port_id)) {
		if (this_afe.prot_cfg.mode ==
			MSM_SPKR_PROT_CALIBRATION_IN_PROGRESS)
			afe_spk_config.mode_rx_cfg.mode =
				Q6AFE_MSM_SPKR_CALIBRATION;
		else
			afe_spk_config.mode_rx_cfg.mode =
				Q6AFE_MSM_SPKR_PROCESSING;
		afe_spk_config.mode_rx_cfg.minor_version = 1;
		if (afe_spk_prot_prepare(port_id, 0,
			AFE_PARAM_ID_FBSP_MODE_RX_CFG,
			&afe_spk_config))
			pr_err("%s: RX MODE_VI_PROC_CFG failed\n",
				   __func__);

		if (afe_spk_config.mode_rx_cfg.mode ==
			Q6AFE_MSM_SPKR_PROCESSING) {
			if (this_afe.prot_cfg.sp_version >=
				AFE_API_VERSION_SUPPORT_SPV3) {
				afe_spk_limiter_config.limiter_th_cfg.
					minor_version = 1;
				afe_spk_limiter_config.limiter_th_cfg.
				lim_thr_per_calib_q27[SP_V2_SPKR_1] =
				this_afe.prot_cfg.limiter_th[SP_V2_SPKR_1];
				afe_spk_limiter_config.limiter_th_cfg.
				lim_thr_per_calib_q27[SP_V2_SPKR_2] =
				this_afe.prot_cfg.limiter_th[SP_V2_SPKR_2];
				if (afe_spk_prot_prepare(port_id, 0,
					AFE_PARAM_ID_SP_RX_LIMITER_TH,
					&afe_spk_limiter_config))
					pr_err("%s: SP_RX_LIMITER_TH failed.\n",
						__func__);
			} else {
				pr_debug("%s: SPv3 failed to apply on AFE API version=%d.\n",
					__func__,
					this_afe.prot_cfg.sp_version);
			}
		}
	}
	mutex_unlock(&this_afe.cal_data[AFE_FB_SPKR_PROT_CAL]->lock);
done:
	return;
}

static int afe_send_hw_delay(u16 port_id, u32 rate)
{
	struct audio_cal_hw_delay_entry delay_entry;
	struct afe_param_id_device_hw_delay_cfg hw_delay;
	struct param_hdr_v3 param_info;
	int ret = -EINVAL;

	pr_debug("%s:\n", __func__);

	memset(&delay_entry, 0, sizeof(delay_entry));
	memset(&param_info, 0, sizeof(param_info));

	delay_entry.sample_rate = rate;
	if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_TX)
		ret = afe_get_cal_hw_delay(TX_DEVICE, &delay_entry);
	else if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_RX)
		ret = afe_get_cal_hw_delay(RX_DEVICE, &delay_entry);

	/*
	 * HW delay is only used for IMS calls to sync audio with video
	 * It is only needed for devices & sample rates used for IMS video
	 * calls. Values are received from ACDB calbration files
	 */
	if (ret != 0) {
		pr_debug("%s: debug: HW delay info not available %d\n",
			__func__, ret);
		goto fail_cmd;
	}

	param_info.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = AFE_PARAM_ID_DEVICE_HW_DELAY;
	param_info.param_size = sizeof(hw_delay);

	hw_delay.delay_in_us = delay_entry.delay_usec;
	hw_delay.device_hw_delay_minor_version =
		AFE_API_VERSION_DEVICE_HW_DELAY;

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_info, (u8 *) &hw_delay);
	if (ret)
		pr_err("%s: AFE hw delay for port 0x%x failed %d\n",
		       __func__, port_id, ret);

fail_cmd:
	pr_debug("%s: port_id 0x%x rate %u delay_usec %d status %d\n",
	__func__, port_id, rate, delay_entry.delay_usec, ret);
	return ret;
}

static struct cal_block_data *afe_find_cal_topo_id_by_port(
			struct cal_type_data *cal_type, u16 port_id)
{
	struct list_head		*ptr, *next;
	struct cal_block_data	*cal_block = NULL;
	int32_t path;
	struct audio_cal_info_afe_top *afe_top;
	int afe_port_index = q6audio_get_port_index(port_id);

	if (afe_port_index < 0)
		goto err_exit;

	list_for_each_safe(ptr, next,
		&cal_type->cal_blocks) {
		cal_block = list_entry(ptr,
			struct cal_block_data, list);
		/* Skip cal_block if it is already marked stale */
		if (cal_utils_is_cal_stale(cal_block))
			continue;
		path = ((afe_get_port_type(port_id) ==
			MSM_AFE_PORT_TYPE_TX)?(TX_DEVICE):(RX_DEVICE));
		afe_top =
		(struct audio_cal_info_afe_top *)cal_block->cal_info;
		if (afe_top->path == path) {
			if (this_afe.dev_acdb_id[afe_port_index] > 0) {
				if (afe_top->acdb_id ==
				    this_afe.dev_acdb_id[afe_port_index]) {
					pr_debug("%s: top_id:%x acdb_id:%d afe_port_id:%d\n",
						 __func__, afe_top->topology,
						 afe_top->acdb_id,
						 q6audio_get_port_id(port_id));
					return cal_block;
				}
			} else {
				pr_debug("%s: top_id:%x acdb_id:%d afe_port:%d\n",
				 __func__, afe_top->topology, afe_top->acdb_id,
				 q6audio_get_port_id(port_id));
				return cal_block;
			}
		}
	}

err_exit:
	return NULL;
}

/*
 * Retrieving cal_block will mark cal_block as stale.
 * Hence it cannot be reused or resent unless the flag
 * is reset.
 */
static int afe_get_cal_topology_id(u16 port_id, u32 *topology_id,
				   int cal_type_index)
{
	int ret = 0;

	struct cal_block_data   *cal_block = NULL;
	struct audio_cal_info_afe_top   *afe_top_info = NULL;

	if (this_afe.cal_data[cal_type_index] == NULL) {
		pr_err("%s: cal_type %d not initialized\n", __func__,
			cal_type_index);
		return -EINVAL;
	}
	if (topology_id == NULL) {
		pr_err("%s: topology_id is NULL\n", __func__);
		return -EINVAL;
	}
	*topology_id = 0;

	mutex_lock(&this_afe.cal_data[cal_type_index]->lock);
	cal_block = afe_find_cal_topo_id_by_port(
		this_afe.cal_data[cal_type_index], port_id);
	if (cal_block == NULL) {
		pr_err("%s: cal_type %d not initialized for this port %d\n",
			__func__, cal_type_index, port_id);
		ret = -EINVAL;
		goto unlock;
	}

	afe_top_info = ((struct audio_cal_info_afe_top *)
		cal_block->cal_info);
	if (!afe_top_info->topology) {
		pr_err("%s: invalid topology id : [%d, %d]\n",
		       __func__, afe_top_info->acdb_id, afe_top_info->topology);
		ret = -EINVAL;
		goto unlock;
	}
	*topology_id = (u32)afe_top_info->topology;
	cal_utils_mark_cal_used(cal_block);

	pr_debug("%s: port_id = %u acdb_id = %d topology_id = %u ret=%d\n",
		__func__, port_id, afe_top_info->acdb_id,
		afe_top_info->topology, ret);
unlock:
	mutex_unlock(&this_afe.cal_data[cal_type_index]->lock);
	return ret;
}

static int afe_send_port_topology_id(u16 port_id)
{
	struct afe_param_id_set_topology_cfg topology;
	struct param_hdr_v3 param_info;
	u32 topology_id = 0;
	int index = 0;
	int ret = 0;

	memset(&topology, 0, sizeof(topology));
	memset(&param_info, 0, sizeof(param_info));
	index = q6audio_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		return -EINVAL;
	}

	ret = afe_get_cal_topology_id(port_id, &topology_id, AFE_TOPOLOGY_CAL);
	if (ret < 0) {
		pr_debug("%s: Check for LSM topology\n", __func__);
		ret = afe_get_cal_topology_id(port_id, &topology_id,
					      AFE_LSM_TOPOLOGY_CAL);
	}
	if (ret || !topology_id) {
		pr_debug("%s: AFE port[%d] get_cal_topology[%d] invalid!\n",
				__func__, port_id, topology_id);
		goto done;
	}

	param_info.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = AFE_PARAM_ID_SET_TOPOLOGY;
	param_info.param_size = sizeof(topology);

	topology.minor_version = AFE_API_VERSION_TOPOLOGY_V1;
	topology.topology_id = topology_id;

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_info, (u8 *) &topology);
	if (ret) {
		pr_err("%s: AFE set topology id enable for port 0x%x failed %d\n",
			__func__, port_id, ret);
		goto done;
	}

	this_afe.topology[index] = topology_id;
	rtac_update_afe_topology(port_id);
done:
	pr_debug("%s: AFE set topology id 0x%x  enable for port 0x%x ret %d\n",
			__func__, topology_id, port_id, ret);
	return ret;

}


static int afe_get_island_mode(u16 port_id, u32 *island_mode)
{
	int ret = 0;
	int index = 0;
	*island_mode = 0;

	index = q6audio_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		return -EINVAL;
	}

	*island_mode = this_afe.island_mode[index];
	return ret;
}

/*
 * afe_send_port_island_mode -
 *         for sending island mode to AFE
 *
 * @port_id: AFE port id number
 *
 * Returns 0 on success or error on failure.
 */
int afe_send_port_island_mode(u16 port_id)
{
	struct afe_param_id_island_cfg_t island_cfg;
	struct param_hdr_v3 param_info;
	u32 island_mode = 0;
	int ret = 0;

	memset(&island_cfg, 0, sizeof(island_cfg));
	memset(&param_info, 0, sizeof(param_info));

	ret = afe_get_island_mode(port_id, &island_mode);
	if (ret) {
		pr_err("%s: AFE port[%d] get island mode is invalid!\n",
				__func__, port_id);
		return ret;
	}
	param_info.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = AFE_PARAM_ID_ISLAND_CONFIG;
	param_info.param_size = sizeof(island_cfg);

	island_cfg.island_cfg_minor_version = AFE_API_VERSION_ISLAND_CONFIG;
	island_cfg.island_enable = island_mode;

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_info, (u8 *) &island_cfg);
	if (ret) {
		pr_err("%s: AFE set island mode enable for port 0x%x failed %d\n",
			__func__, port_id, ret);
		return ret;
	}
	pr_debug("%s: AFE set island mode 0x%x  enable for port 0x%x ret %d\n",
			__func__, island_mode, port_id, ret);
	return ret;
}
EXPORT_SYMBOL(afe_send_port_island_mode);

static int afe_get_vad_preroll_cfg(u16 port_id, u32 *preroll_cfg)
{
	int ret = 0;
	int index = 0;
	*preroll_cfg = 0;

	index = q6audio_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		return -EINVAL;
	}

	*preroll_cfg = this_afe.vad_cfg[index].pre_roll;
	return ret;
}

static int afe_send_port_vad_cfg_params(u16 port_id)
{
	struct afe_param_id_vad_cfg_t vad_cfg;
	struct param_hdr_v3 param_info;
	u32 pre_roll_cfg = 0;
	struct firmware_cal *hwdep_cal = NULL;
	int ret = 0;

	memset(&vad_cfg, 0, sizeof(vad_cfg));
	memset(&param_info, 0, sizeof(param_info));

	ret = afe_get_vad_preroll_cfg(port_id, &pre_roll_cfg);
	if (ret) {
		pr_err("%s: AFE port[%d] get preroll cfg is invalid!\n",
				__func__, port_id);
		return ret;
	}
	param_info.module_id = AFE_MODULE_VAD;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = AFE_PARAM_ID_VAD_CFG;
	param_info.param_size = sizeof(vad_cfg);

	vad_cfg.vad_cfg_minor_version = AFE_API_VERSION_VAD_CFG;
	vad_cfg.pre_roll_in_ms = pre_roll_cfg;

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_info, (u8 *) &vad_cfg);
	if (ret) {
		pr_err("%s: AFE set vad cfg for port 0x%x failed %d\n",
			__func__, port_id, ret);
		return ret;
	}

	memset(&param_info, 0, sizeof(param_info));

	hwdep_cal = q6afecal_get_fw_cal(this_afe.fw_data, Q6AFE_VAD_CORE_CAL);
	if (!hwdep_cal) {
		pr_err("%s: error in retrieving vad core calibration",
			__func__);
		return -EINVAL;
	}

	param_info.module_id = AFE_MODULE_VAD;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = AFE_PARAM_ID_VAD_CORE_CFG;
	param_info.param_size = hwdep_cal->size;

	ret = q6afe_pack_and_set_param_in_band(port_id,
					q6audio_get_port_index(port_id),
					param_info, (u8 *) hwdep_cal->data);
	if (ret) {
		pr_err("%s: AFE set vad cfg for port 0x%x failed %d\n",
			__func__, port_id, ret);
		return ret;
	}
	pr_debug("%s: AFE set preroll cfg %d vad core cfg  port 0x%x ret %d\n",
			__func__, pre_roll_cfg, port_id, ret);
	return ret;
}

static int remap_cal_data(struct cal_block_data *cal_block, int cal_index)
{
	int ret = 0;

	if (cal_block->map_data.dma_buf == NULL) {
		pr_err("%s: No ION allocation for cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	if ((cal_block->map_data.map_size > 0) &&
		(cal_block->map_data.q6map_handle == 0)) {
		atomic_set(&this_afe.mem_map_cal_index, cal_index);
		ret = afe_cmd_memory_map(cal_block->cal_data.paddr,
				cal_block->map_data.map_size);
		atomic_set(&this_afe.mem_map_cal_index, -1);
		if (ret < 0) {
			pr_err("%s: mmap did not work! size = %zd ret %d\n",
				__func__,
				cal_block->map_data.map_size, ret);
			pr_debug("%s: mmap did not work! addr = 0x%pK, size = %zd\n",
				__func__,
				&cal_block->cal_data.paddr,
				cal_block->map_data.map_size);
			goto done;
		}
		cal_block->map_data.q6map_handle = atomic_read(&this_afe.
			mem_map_cal_handles[cal_index]);
	}
done:
	return ret;
}

static struct cal_block_data *afe_find_cal(int cal_index, int port_id)
{
	struct list_head *ptr, *next;
	struct cal_block_data *cal_block = NULL;
	struct audio_cal_info_afe *afe_cal_info = NULL;
	int afe_port_index = q6audio_get_port_index(port_id);

	pr_debug("%s: cal_index %d port_id %d port_index %d\n", __func__,
		  cal_index, port_id, afe_port_index);
	if (afe_port_index < 0) {
		pr_err("%s: Error getting AFE port index %d\n",
			__func__, afe_port_index);
		goto exit;
	}

	list_for_each_safe(ptr, next,
			   &this_afe.cal_data[cal_index]->cal_blocks) {
		cal_block = list_entry(ptr, struct cal_block_data, list);
		afe_cal_info = cal_block->cal_info;
		if ((afe_cal_info->acdb_id ==
		     this_afe.dev_acdb_id[afe_port_index]) &&
		    (afe_cal_info->sample_rate ==
		     this_afe.afe_sample_rates[afe_port_index])) {
			pr_debug("%s: cal block is a match, size is %zd\n",
				 __func__, cal_block->cal_data.size);
			goto exit;
		}
	}
	pr_debug("%s: no matching cal_block found\n", __func__);
	cal_block = NULL;

exit:
	return cal_block;
}

static int send_afe_cal_type(int cal_index, int port_id)
{
	struct cal_block_data		*cal_block = NULL;
	int ret;
	int afe_port_index = q6audio_get_port_index(port_id);

	pr_debug("%s:\n", __func__);

	if (this_afe.cal_data[cal_index] == NULL) {
		pr_warn("%s: cal_index %d not allocated!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	if (afe_port_index < 0) {
		pr_err("%s: Error getting AFE port index %d\n",
			__func__, afe_port_index);
		ret = -EINVAL;
		goto done;
	}

	mutex_lock(&this_afe.cal_data[cal_index]->lock);

	if (((cal_index == AFE_COMMON_RX_CAL) ||
	     (cal_index == AFE_COMMON_TX_CAL) ||
	     (cal_index == AFE_LSM_TX_CAL)) &&
	    (this_afe.dev_acdb_id[afe_port_index] > 0))
		cal_block = afe_find_cal(cal_index, port_id);
	else
		cal_block = cal_utils_get_only_cal_block(
				this_afe.cal_data[cal_index]);

	if (cal_block == NULL || cal_utils_is_cal_stale(cal_block)) {
		pr_err("%s cal_block not found!!\n", __func__);
		ret = -EINVAL;
		goto unlock;
	}

	pr_debug("%s: Sending cal_index cal %d\n", __func__, cal_index);

	ret = remap_cal_data(cal_block, cal_index);
	if (ret) {
		pr_err("%s: Remap_cal_data failed for cal %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto unlock;
	}
	ret = afe_send_cal_block(port_id, cal_block);
	if (ret < 0)
		pr_debug("%s: No cal sent for cal_index %d, port_id = 0x%x! ret %d\n",
			__func__, cal_index, port_id, ret);

	cal_utils_mark_cal_used(cal_block);

unlock:
	mutex_unlock(&this_afe.cal_data[cal_index]->lock);
done:
	return ret;
}

void afe_send_cal(u16 port_id)
{
	int ret;

	pr_debug("%s: port_id=0x%x\n", __func__, port_id);

	if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_TX) {
		afe_send_cal_spkr_prot_tx(port_id);
		ret = send_afe_cal_type(AFE_COMMON_TX_CAL, port_id);
		if (ret < 0)
			send_afe_cal_type(AFE_LSM_TX_CAL, port_id);
	} else if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_RX) {
		send_afe_cal_type(AFE_COMMON_RX_CAL, port_id);
		afe_send_cal_spkr_prot_rx(port_id);
	}
}

int afe_turn_onoff_hw_mad(u16 mad_type, u16 enable)
{
	struct afe_param_hw_mad_ctrl mad_enable_param;
	struct param_hdr_v3 param_info;
	int ret;

	pr_debug("%s: enter\n", __func__);

	memset(&mad_enable_param, 0, sizeof(mad_enable_param));
	memset(&param_info, 0, sizeof(param_info));
	param_info.module_id = AFE_MODULE_HW_MAD;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = AFE_PARAM_ID_HW_MAD_CTRL;
	param_info.param_size = sizeof(mad_enable_param);

	mad_enable_param.minor_version = 1;
	mad_enable_param.mad_type = mad_type;
	mad_enable_param.mad_enable = enable;

	ret = q6afe_pack_and_set_param_in_band(SLIMBUS_5_TX, IDX_GLOBAL_CFG,
					       param_info,
					       (u8 *) &mad_enable_param);
	if (ret)
		pr_err("%s: AFE_PARAM_ID_HW_MAD_CTRL failed %d\n", __func__,
		       ret);
	return ret;
}

static int afe_send_slimbus_slave_cfg(
	struct afe_param_cdc_slimbus_slave_cfg *sb_slave_cfg)
{
	struct param_hdr_v3 param_hdr;
	int ret;

	pr_debug("%s: enter\n", __func__);

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AFE_MODULE_CDC_DEV_CFG;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_CDC_SLIMBUS_SLAVE_CFG;
	param_hdr.param_size = sizeof(struct afe_param_cdc_slimbus_slave_cfg);

	ret = q6afe_svc_pack_and_set_param_in_band(IDX_GLOBAL_CFG, param_hdr,
						   (u8 *) sb_slave_cfg);
	if (ret)
		pr_err("%s: AFE_PARAM_ID_CDC_SLIMBUS_SLAVE_CFG failed %d\n",
		       __func__, ret);

	pr_debug("%s: leave %d\n", __func__, ret);
	return ret;
}

static int afe_send_codec_reg_page_config(
	struct afe_param_cdc_reg_page_cfg *cdc_reg_page_cfg)
{
	struct param_hdr_v3 param_hdr;
	int ret;

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AFE_MODULE_CDC_DEV_CFG;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_CDC_REG_PAGE_CFG;
	param_hdr.param_size = sizeof(struct afe_param_cdc_reg_page_cfg);

	ret = q6afe_svc_pack_and_set_param_in_band(IDX_GLOBAL_CFG, param_hdr,
						   (u8 *) cdc_reg_page_cfg);
	if (ret)
		pr_err("%s: AFE_PARAM_ID_CDC_REG_PAGE_CFG failed %d\n",
		       __func__, ret);

	return ret;
}

static int afe_send_codec_reg_config(
	struct afe_param_cdc_reg_cfg_data *cdc_reg_cfg)
{
	u8 *packed_param_data = NULL;
	u32 packed_data_size = 0;
	u32 single_param_size = 0;
	u32 max_data_size = 0;
	u32 max_single_param = 0;
	struct param_hdr_v3 param_hdr;
	int idx = 0;
	int ret = -EINVAL;

	memset(&param_hdr, 0, sizeof(param_hdr));
	max_single_param = sizeof(struct param_hdr_v3) +
			   sizeof(struct afe_param_cdc_reg_cfg);
	max_data_size = APR_MAX_BUF - sizeof(struct afe_svc_cmd_set_param_v2);
	packed_param_data = kzalloc(max_data_size, GFP_KERNEL);
	if (!packed_param_data)
		return -ENOMEM;

	/* param_hdr is the same for all params sent, set once at top */
	param_hdr.module_id = AFE_MODULE_CDC_DEV_CFG;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_CDC_REG_CFG;
	param_hdr.param_size = sizeof(struct afe_param_cdc_reg_cfg);

	while (idx < cdc_reg_cfg->num_registers) {
		memset(packed_param_data, 0, max_data_size);
		packed_data_size = 0;
		single_param_size = 0;

		while (packed_data_size + max_single_param < max_data_size &&
		       idx < cdc_reg_cfg->num_registers) {
			ret = q6common_pack_pp_params(
				packed_param_data + packed_data_size,
				&param_hdr, (u8 *) &cdc_reg_cfg->reg_data[idx],
				&single_param_size);
			if (ret) {
				pr_err("%s: Failed to pack parameters with error %d\n",
				       __func__, ret);
				goto done;
			}
			packed_data_size += single_param_size;
			idx++;
		}

		ret = q6afe_svc_set_params(IDX_GLOBAL_CFG, NULL,
					   packed_param_data, packed_data_size);
		if (ret) {
			pr_err("%s: AFE_PARAM_ID_CDC_REG_CFG failed %d\n",
				__func__, ret);
			break;
		}
	}
done:
	kfree(packed_param_data);
	return ret;
}

static int afe_init_cdc_reg_config(void)
{
	struct param_hdr_v3 param_hdr;
	int ret;

	pr_debug("%s: enter\n", __func__);
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AFE_MODULE_CDC_DEV_CFG;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_CDC_REG_CFG_INIT;

	ret = q6afe_svc_pack_and_set_param_in_band(IDX_GLOBAL_CFG, param_hdr,
						   NULL);
	if (ret)
		pr_err("%s: AFE_PARAM_ID_CDC_INIT_REG_CFG failed %d\n",
		       __func__, ret);

	return ret;
}

static int afe_send_slimbus_slave_port_cfg(
	struct afe_param_slimbus_slave_port_cfg *slim_slave_config, u16 port_id)
{
	struct param_hdr_v3 param_hdr;
	int ret;

	pr_debug("%s: enter, port_id =  0x%x\n", __func__, port_id);
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AFE_MODULE_HW_MAD;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.reserved = 0;
	param_hdr.param_id = AFE_PARAM_ID_SLIMBUS_SLAVE_PORT_CFG;
	param_hdr.param_size = sizeof(struct afe_param_slimbus_slave_port_cfg);

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr,
					       (u8 *) slim_slave_config);
	if (ret)
		pr_err("%s: AFE_PARAM_ID_SLIMBUS_SLAVE_PORT_CFG failed %d\n",
			__func__, ret);

	pr_debug("%s: leave %d\n", __func__, ret);
	return ret;
}
static int afe_aanc_port_cfg(void *apr, uint16_t tx_port, uint16_t rx_port)
{
	struct afe_param_aanc_port_cfg aanc_port_cfg;
	struct param_hdr_v3 param_hdr;
	int ret = 0;

	pr_debug("%s: tx_port 0x%x, rx_port 0x%x\n",
		__func__, tx_port, rx_port);

	pr_debug("%s: AANC sample rate tx rate: %d rx rate %d\n", __func__,
		 this_afe.aanc_info.aanc_tx_port_sample_rate,
		 this_afe.aanc_info.aanc_rx_port_sample_rate);

	memset(&aanc_port_cfg, 0, sizeof(aanc_port_cfg));
	memset(&param_hdr, 0, sizeof(param_hdr));

	/*
	 * If aanc tx sample rate or rx sample rate is zero, skip aanc
	 * configuration as AFE resampler will fail for invalid sample
	 * rates.
	 */
	if (!this_afe.aanc_info.aanc_tx_port_sample_rate ||
	    !this_afe.aanc_info.aanc_rx_port_sample_rate) {
		return -EINVAL;
	}

	param_hdr.module_id = AFE_MODULE_AANC;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_AANC_PORT_CONFIG;
	param_hdr.param_size = sizeof(struct afe_param_aanc_port_cfg);

	aanc_port_cfg.aanc_port_cfg_minor_version =
		AFE_API_VERSION_AANC_PORT_CONFIG;
	aanc_port_cfg.tx_port_sample_rate =
		this_afe.aanc_info.aanc_tx_port_sample_rate;
	aanc_port_cfg.tx_port_channel_map[0] = AANC_TX_VOICE_MIC;
	aanc_port_cfg.tx_port_channel_map[1] = AANC_TX_NOISE_MIC;
	aanc_port_cfg.tx_port_channel_map[2] = AANC_TX_ERROR_MIC;
	aanc_port_cfg.tx_port_channel_map[3] = AANC_TX_MIC_UNUSED;
	aanc_port_cfg.tx_port_channel_map[4] = AANC_TX_MIC_UNUSED;
	aanc_port_cfg.tx_port_channel_map[5] = AANC_TX_MIC_UNUSED;
	aanc_port_cfg.tx_port_channel_map[6] = AANC_TX_MIC_UNUSED;
	aanc_port_cfg.tx_port_channel_map[7] = AANC_TX_MIC_UNUSED;
	aanc_port_cfg.tx_port_num_channels = 3;
	aanc_port_cfg.rx_path_ref_port_id = rx_port;
	aanc_port_cfg.ref_port_sample_rate =
		this_afe.aanc_info.aanc_rx_port_sample_rate;

	ret = q6afe_pack_and_set_param_in_band(tx_port,
					       q6audio_get_port_index(tx_port),
					       param_hdr,
					       (u8 *) &aanc_port_cfg);
	if (ret)
		pr_err("%s: AFE AANC port config failed for tx_port 0x%x, rx_port 0x%x ret %d\n",
		       __func__, tx_port, rx_port, ret);
	else
		q6afe_set_aanc_level();

	return ret;
}

static int afe_aanc_mod_enable(void *apr, uint16_t tx_port, uint16_t enable)
{
	struct afe_mod_enable_param mod_enable;
	struct param_hdr_v3 param_hdr;
	int ret = 0;

	pr_debug("%s: tx_port 0x%x\n", __func__, tx_port);

	memset(&mod_enable, 0, sizeof(mod_enable));
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AFE_MODULE_AANC;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_ENABLE;
	param_hdr.param_size = sizeof(struct afe_mod_enable_param);

	mod_enable.enable = enable;
	mod_enable.reserved = 0;

	ret = q6afe_pack_and_set_param_in_band(tx_port,
					       q6audio_get_port_index(tx_port),
					       param_hdr, (u8 *) &mod_enable);
	if (ret)
		pr_err("%s: AFE AANC enable failed for tx_port 0x%x ret %d\n",
			__func__, tx_port, ret);
	return ret;
}

static int afe_send_bank_selection_clip(
		struct afe_param_id_clip_bank_sel *param)
{
	struct param_hdr_v3 param_hdr;
	int ret;

	if (!param) {
		pr_err("%s: Invalid params", __func__);
		return -EINVAL;
	}
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AFE_MODULE_CDC_DEV_CFG;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_CLIP_BANK_SEL_CFG;
	param_hdr.param_size = sizeof(struct afe_param_id_clip_bank_sel);

	ret = q6afe_svc_pack_and_set_param_in_band(IDX_GLOBAL_CFG, param_hdr,
						   (u8 *) param);
	if (ret)
		pr_err("%s: AFE_PARAM_ID_CLIP_BANK_SEL_CFG failed %d\n",
		__func__, ret);
	return ret;
}
int afe_send_aanc_version(
	struct afe_param_id_cdc_aanc_version *version_cfg)
{
	struct param_hdr_v3 param_hdr;
	int ret;

	pr_debug("%s: enter\n", __func__);
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AFE_MODULE_CDC_DEV_CFG;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_CDC_AANC_VERSION;
	param_hdr.param_size = sizeof(struct afe_param_id_cdc_aanc_version);

	ret = q6afe_svc_pack_and_set_param_in_band(IDX_GLOBAL_CFG, param_hdr,
						   (u8 *) version_cfg);
	if (ret)
		pr_err("%s: AFE_PARAM_ID_CDC_AANC_VERSION failed %d\n",
		__func__, ret);
	return ret;
}

/**
 * afe_port_set_mad_type -
 *        to update mad type
 *
 * @port_id: AFE port id number
 * @mad_type: MAD type enum value
 *
 * Returns 0 on success or error on failure.
 */
int afe_port_set_mad_type(u16 port_id, enum afe_mad_type mad_type)
{
	int i;

	if (port_id == AFE_PORT_ID_TERTIARY_MI2S_TX ||
		port_id == AFE_PORT_ID_INT3_MI2S_TX ||
		port_id == AFE_PORT_ID_TX_CODEC_DMA_TX_3) {
		mad_type = MAD_SW_AUDIO;
		return 0;
	}

	i = port_id - SLIMBUS_0_RX;
	if (i < 0 || i >= ARRAY_SIZE(afe_ports_mad_type)) {
		pr_err("%s: Invalid port_id 0x%x\n", __func__, port_id);
		return -EINVAL;
	}
	atomic_set(&afe_ports_mad_type[i], mad_type);
	return 0;
}
EXPORT_SYMBOL(afe_port_set_mad_type);

/**
 * afe_port_get_mad_type -
 *        to retrieve mad type
 *
 * @port_id: AFE port id number
 *
 * Returns valid enum value on success or MAD_HW_NONE on failure.
 */
enum afe_mad_type afe_port_get_mad_type(u16 port_id)
{
	int i;

	if (port_id == AFE_PORT_ID_TERTIARY_MI2S_TX ||
		port_id == AFE_PORT_ID_INT3_MI2S_TX ||
		port_id == AFE_PORT_ID_TX_CODEC_DMA_TX_3)
		return MAD_SW_AUDIO;

	i = port_id - SLIMBUS_0_RX;
	if (i < 0 || i >= ARRAY_SIZE(afe_ports_mad_type)) {
		pr_debug("%s: Non Slimbus port_id 0x%x\n", __func__, port_id);
		return MAD_HW_NONE;
	}
	return (enum afe_mad_type) atomic_read(&afe_ports_mad_type[i]);
}
EXPORT_SYMBOL(afe_port_get_mad_type);

/**
 * afe_set_config -
 *        to configure AFE session with
 *        specified configuration for given config type
 *
 * @config_type: config type
 * @config_data: configuration to pass to AFE session
 * @arg: argument used in specific config types
 *
 * Returns 0 on success or error value on port start failure.
 */
int afe_set_config(enum afe_config_type config_type, void *config_data, int arg)
{
	int ret;

	pr_debug("%s: enter config_type %d\n", __func__, config_type);
	ret = afe_q6_interface_prepare();
	if (ret) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	switch (config_type) {
	case AFE_SLIMBUS_SLAVE_CONFIG:
		ret = afe_send_slimbus_slave_cfg(config_data);
		if (!ret)
			ret = afe_init_cdc_reg_config();
		else
			pr_err("%s: Sending slimbus slave config failed %d\n",
			       __func__, ret);
		break;
	case AFE_CDC_REGISTERS_CONFIG:
		ret = afe_send_codec_reg_config(config_data);
		break;
	case AFE_SLIMBUS_SLAVE_PORT_CONFIG:
		ret = afe_send_slimbus_slave_port_cfg(config_data, arg);
		break;
	case AFE_AANC_VERSION:
		ret = afe_send_aanc_version(config_data);
		break;
	case AFE_CLIP_BANK_SEL:
		ret = afe_send_bank_selection_clip(config_data);
		break;
	case AFE_CDC_CLIP_REGISTERS_CONFIG:
		ret = afe_send_codec_reg_config(config_data);
		break;
	case AFE_CDC_REGISTER_PAGE_CONFIG:
		ret = afe_send_codec_reg_page_config(config_data);
		break;
	default:
		pr_err("%s: unknown configuration type %d",
			__func__, config_type);
		ret = -EINVAL;
	}

	if (!ret)
		set_bit(config_type, &afe_configured_cmd);

	return ret;
}
EXPORT_SYMBOL(afe_set_config);

/*
 * afe_clear_config - If SSR happens ADSP loses AFE configs, let AFE driver know
 *		      about the state so client driver can wait until AFE is
 *		      reconfigured.
 */
void afe_clear_config(enum afe_config_type config)
{
	clear_bit(config, &afe_configured_cmd);
}
EXPORT_SYMBOL(afe_clear_config);

bool afe_has_config(enum afe_config_type config)
{
	return !!test_bit(config, &afe_configured_cmd);
}

int afe_send_spdif_clk_cfg(struct afe_param_id_spdif_clk_cfg *cfg,
		u16 port_id)
{
	struct afe_param_id_spdif_clk_cfg clk_cfg;
	struct param_hdr_v3 param_hdr;
	int ret = 0;

	if (!cfg) {
		pr_err("%s: Error, no configuration data\n", __func__);
		return -EINVAL;
	}

	memset(&clk_cfg, 0, sizeof(clk_cfg));
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_SPDIF_CLK_CONFIG;
	param_hdr.param_size = sizeof(struct afe_param_id_spdif_clk_cfg);

	pr_debug("%s: Minor version = 0x%x clk val = %d clk root = 0x%x port id = 0x%x\n",
		__func__, clk_cfg.clk_cfg_minor_version, clk_cfg.clk_value,
		clk_cfg.clk_root, q6audio_get_port_id(port_id));

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) &clk_cfg);
	if (ret < 0)
		pr_err("%s: AFE send clock config for port 0x%x failed ret = %d\n",
				__func__, port_id, ret);
	return ret;
}

/**
 * afe_send_spdif_ch_status_cfg -
 *        to configure AFE session with
 *        specified channel status configuration
 *
 * @ch_status_cfg: channel status configutation
 * @port_id: AFE port id number
 *
 * Returns 0 on success or error value on port start failure.
 */
int afe_send_spdif_ch_status_cfg(struct afe_param_id_spdif_ch_status_cfg
		*ch_status_cfg,	u16 port_id)
{
	struct param_hdr_v3 param_hdr;
	int ret = 0;

	if (!ch_status_cfg) {
		pr_err("%s: Error, no configuration data\n", __func__);
		return -EINVAL;
	}

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_SPDIF_CLK_CONFIG;
	param_hdr.param_size = sizeof(struct afe_param_id_spdif_ch_status_cfg);

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) ch_status_cfg);
	if (ret < 0)
		pr_err("%s: AFE send channel status for port 0x%x failed ret = %d\n",
				__func__, port_id, ret);
	return ret;
}
EXPORT_SYMBOL(afe_send_spdif_ch_status_cfg);

int afe_send_cmd_wakeup_register(void *handle, bool enable)
{
	struct afe_svc_cmd_evt_cfg_payload wakeup_irq;
	int ret = 0;

	pr_debug("%s: enter\n", __func__);

	wakeup_irq.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					    APR_HDR_LEN(APR_HDR_SIZE),
					    APR_PKT_VER);
	wakeup_irq.hdr.pkt_size = sizeof(wakeup_irq);
	wakeup_irq.hdr.src_port = 0;
	wakeup_irq.hdr.dest_port = 0;
	wakeup_irq.hdr.token = 0x0;
	wakeup_irq.hdr.opcode = AFE_SVC_CMD_EVENT_CFG;
	wakeup_irq.event_id = AFE_EVENT_ID_MBHC_DETECTION_SW_WA;
	wakeup_irq.reg_flag = enable;
	pr_debug("%s: cmd wakeup register opcode[0x%x] register:%d\n",
		 __func__, wakeup_irq.hdr.opcode, wakeup_irq.reg_flag);

	ret = afe_apr_send_pkt(&wakeup_irq, &this_afe.wait_wakeup);
	if (ret)
		pr_err("%s: AFE wakeup command register %d failed %d\n",
			__func__, enable, ret);

	return ret;
}
EXPORT_SYMBOL(afe_send_cmd_wakeup_register);

static int afe_send_cmd_port_start(u16 port_id)
{
	struct afe_port_cmd_device_start start;
	int ret, index;

	pr_debug("%s: enter\n", __func__);
	index = q6audio_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		return -EINVAL;
	}
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: port id: 0x%x ret %d\n", __func__, port_id, ret);
		return -EINVAL;
	}

	start.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					    APR_HDR_LEN(APR_HDR_SIZE),
					    APR_PKT_VER);
	start.hdr.pkt_size = sizeof(start);
	start.hdr.src_port = 0;
	start.hdr.dest_port = 0;
	start.hdr.token = index;
	start.hdr.opcode = AFE_PORT_CMD_DEVICE_START;
	start.port_id = q6audio_get_port_id(port_id);
	pr_debug("%s: cmd device start opcode[0x%x] port id[0x%x]\n",
		 __func__, start.hdr.opcode, start.port_id);

	ret = afe_apr_send_pkt(&start, &this_afe.wait[index]);
	if (ret)
		pr_err("%s: AFE enable for port 0x%x failed %d\n", __func__,
		       port_id, ret);

	return ret;
}

static int afe_aanc_start(uint16_t tx_port_id, uint16_t rx_port_id)
{
	int ret;

	pr_debug("%s:  Tx port is 0x%x, Rx port is 0x%x\n",
		 __func__, tx_port_id, rx_port_id);
	ret = afe_aanc_port_cfg(this_afe.apr, tx_port_id, rx_port_id);
	if (ret) {
		pr_err("%s: Send AANC Port Config failed %d\n",
			__func__, ret);
		goto fail_cmd;
	}
	send_afe_cal_type(AFE_AANC_CAL, tx_port_id);

fail_cmd:
	return ret;
}

/**
 * afe_spdif_port_start - to configure AFE session with
 * specified port configuration
 *
 * @port_id: AFE port id number
 * @spdif_port: spdif port configutation
 * @rate: sampling rate of port
 *
 * Returns 0 on success or error value on port start failure.
 */
int afe_spdif_port_start(u16 port_id, struct afe_spdif_port_config *spdif_port,
		u32 rate)
{
	struct param_hdr_v3 param_hdr;
	uint16_t port_index;
	int ret = 0;

	if (!spdif_port) {
		pr_err("%s: Error, no configuration data\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	pr_debug("%s: port id: 0x%x\n", __func__, port_id);

	memset(&param_hdr, 0, sizeof(param_hdr));
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: port id: 0x%x ret %d\n", __func__, port_id, ret);
		return -EINVAL;
	}

	afe_send_cal(port_id);
	afe_send_hw_delay(port_id, rate);

	param_hdr.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_SPDIF_CONFIG;
	param_hdr.param_size = sizeof(struct afe_spdif_port_config);

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) spdif_port);
	if (ret) {
		pr_err("%s: AFE enable for port 0x%x failed ret = %d\n",
			__func__, port_id, ret);
		goto fail_cmd;
	}

	port_index = afe_get_port_index(port_id);
	if ((port_index >= 0) && (port_index < AFE_MAX_PORTS)) {
		this_afe.afe_sample_rates[port_index] = rate;
	} else {
		pr_err("%s: Invalid port index %d\n", __func__, port_index);
		ret = -EINVAL;
		goto fail_cmd;
	}

	if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_RX) {
		ret = afe_send_spdif_ch_status_cfg(&spdif_port->ch_status,
						   port_id);
		if (ret < 0) {
			pr_err("%s: afe send failed %d\n", __func__, ret);
			goto fail_cmd;
		}
	}

	return afe_send_cmd_port_start(port_id);

fail_cmd:
	return ret;
}
EXPORT_SYMBOL(afe_spdif_port_start);

/**
 * afe_spdif_reg_event_cfg -
 *         register for event from AFE spdif port
 *
 * @port_id: Port ID to register event
 * @reg_flag: register or unregister
 * @cb: callback function to invoke for events from module
 * @private_data: private data to sent back in callback fn
 *
 * Returns 0 on success or error on failure
 */
int afe_spdif_reg_event_cfg(u16 port_id, u16 reg_flag,
		void (*cb)(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv),
		void *private_data)
{
	struct afe_port_cmd_event_cfg *config;
	struct afe_port_cmd_mod_evt_cfg_payload pl;
	int index;
	int ret;
	int num_events = 1;
	int cmd_size = sizeof(struct afe_port_cmd_event_cfg) +
		(num_events * sizeof(struct afe_port_cmd_mod_evt_cfg_payload));

	config = kzalloc(cmd_size, GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	if (port_id == AFE_PORT_ID_PRIMARY_SPDIF_TX) {
		this_afe.pri_spdif_tx_cb = cb;
		this_afe.pri_spdif_tx_private_data = private_data;
	} else if (port_id == AFE_PORT_ID_SECONDARY_SPDIF_TX) {
		this_afe.sec_spdif_tx_cb = cb;
		this_afe.sec_spdif_tx_private_data = private_data;
	} else {
		pr_err("%s: wrong port id 0x%x\n", __func__, port_id);
		ret = -EINVAL;
		goto fail_idx;
	}

	index = q6audio_get_port_index(port_id);
	if (index < 0) {
		pr_err("%s: Invalid index number: %d\n", __func__, index);
		ret = -EINVAL;
		goto fail_idx;
	}

	memset(&pl, 0, sizeof(pl));
	pl.module_id = AFE_MODULE_CUSTOM_EVENTS;
	pl.event_id = AFE_PORT_FMT_UPDATE_EVENT;
	pl.reg_flag = reg_flag;

	config->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	config->hdr.pkt_size = cmd_size;
	config->hdr.src_port = 1;
	config->hdr.dest_port = 1;
	config->hdr.token = index;

	config->hdr.opcode = AFE_PORT_CMD_MOD_EVENT_CFG;
	config->port_id = q6audio_get_port_id(port_id);
	config->num_events = num_events;
	config->version = 1;
	memcpy(config->payload, &pl, sizeof(pl));
	atomic_set(&this_afe.state, 1);
	atomic_set(&this_afe.status, 0);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) config);
	if (ret < 0) {
		pr_err("%s: port = 0x%x failed %d\n",
			__func__, port_id, ret);
		goto fail_cmd;
	}
	ret = wait_event_timeout(this_afe.wait[index],
		(atomic_read(&this_afe.state) == 0),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	if (atomic_read(&this_afe.status) > 0) {
		pr_err("%s: config cmd failed [%s]\n",
			__func__, adsp_err_get_err_str(
			atomic_read(&this_afe.status)));
		ret = adsp_err_get_lnx_err_code(
				atomic_read(&this_afe.status));
		goto fail_idx;
	}
	ret = 0;
fail_cmd:
	pr_debug("%s: config.opcode 0x%x status %d\n",
		__func__, config->hdr.opcode, ret);

fail_idx:
	kfree(config);
	return ret;
}
EXPORT_SYMBOL(afe_spdif_reg_event_cfg);

int afe_send_slot_mapping_cfg(
	struct afe_param_id_slot_mapping_cfg *slot_mapping_cfg,
	u16 port_id)
{
	struct param_hdr_v3 param_hdr;
	int ret = 0;

	if (!slot_mapping_cfg) {
		pr_err("%s: Error, no configuration data\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: port id: 0x%x\n", __func__, port_id);

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AFE_MODULE_TDM;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_PORT_SLOT_MAPPING_CONFIG;
	param_hdr.param_size = sizeof(struct afe_param_id_slot_mapping_cfg);

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr,
					       (u8 *) slot_mapping_cfg);
	if (ret < 0)
		pr_err("%s: AFE send slot mapping for port 0x%x failed ret = %d\n",
				__func__, port_id, ret);
	return ret;
}

int afe_send_custom_tdm_header_cfg(
	struct afe_param_id_custom_tdm_header_cfg *custom_tdm_header_cfg,
	u16 port_id)
{
	struct param_hdr_v3 param_hdr;
	int ret = 0;

	if (!custom_tdm_header_cfg) {
		pr_err("%s: Error, no configuration data\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: port id: 0x%x\n", __func__, port_id);

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AFE_MODULE_TDM;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_CUSTOM_TDM_HEADER_CONFIG;
	param_hdr.param_size =
		sizeof(struct afe_param_id_custom_tdm_header_cfg);

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr,
					       (u8 *) custom_tdm_header_cfg);
	if (ret < 0)
		pr_err("%s: AFE send custom tdm header for port 0x%x failed ret = %d\n",
				__func__, port_id, ret);
	return ret;
}

/**
 * afe_tdm_port_start - to configure AFE session with
 * specified port configuration
 *
 * @port_id: AFE port id number
 * @tdm_port: TDM port configutation
 * @rate: sampling rate of port
 * @num_groups: number of TDM groups
 *
 * Returns 0 on success or error value on port start failure.
 */
int afe_tdm_port_start(u16 port_id, struct afe_tdm_port_config *tdm_port,
		       u32 rate, u16 num_groups)
{
	struct param_hdr_v3 param_hdr;
	int index = 0;
	uint16_t port_index = 0;
	enum afe_mad_type mad_type = MAD_HW_NONE;
	int ret = 0;

	if (!tdm_port) {
		pr_err("%s: Error, no configuration data\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: port id: 0x%x\n", __func__, port_id);

	memset(&param_hdr, 0, sizeof(param_hdr));
	index = q6audio_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		return -EINVAL;
	}
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: port id: 0x%x ret %d\n", __func__, port_id, ret);
		return -EINVAL;
	}

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	if ((index >= 0) && (index < AFE_MAX_PORTS)) {
		this_afe.afe_sample_rates[index] = rate;

		if (this_afe.rt_cb)
			this_afe.dev_acdb_id[index] = this_afe.rt_cb(port_id);
	}

	port_index = afe_get_port_index(port_id);

	if (q6core_get_avcs_api_version_per_service(
		APRV2_IDS_SERVICE_ID_ADSP_AFE_V) >= AFE_API_VERSION_V4) {
		/* send VAD configuration if enabled */
		if (this_afe.vad_cfg[port_index].is_enable) {
			ret = afe_send_port_vad_cfg_params(port_id);
			if (ret) {
				pr_err("%s: afe send VAD config failed %d\n",
					__func__, ret);
				goto fail_cmd;
			}
		}
	}

	/* Also send the topology id here: */
	if (!(this_afe.afe_cal_mode[port_index] == AFE_CAL_MODE_NONE)) {
		/* One time call: only for first time */
		afe_send_custom_topology();
		afe_send_port_topology_id(port_id);
		afe_send_cal(port_id);
		afe_send_hw_delay(port_id, rate);
	}

	/* Start SW MAD module */
	mad_type = afe_port_get_mad_type(port_id);
	pr_debug("%s: port_id 0x%x, mad_type %d\n", __func__, port_id,
		 mad_type);
	if (mad_type != MAD_HW_NONE && mad_type != MAD_SW_AUDIO) {
		if (!afe_has_config(AFE_CDC_REGISTERS_CONFIG) ||
			!afe_has_config(AFE_SLIMBUS_SLAVE_CONFIG)) {
			pr_err("%s: AFE isn't configured yet for\n"
				"HW MAD try Again\n", __func__);
				ret = -EAGAIN;
				goto fail_cmd;
		}
		ret = afe_turn_onoff_hw_mad(mad_type, true);
		if (ret) {
			pr_err("%s: afe_turn_onoff_hw_mad failed %d\n",
			       __func__, ret);
			goto fail_cmd;
		}
	}

	param_hdr.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_TDM_CONFIG;
	param_hdr.param_size = sizeof(struct afe_param_id_tdm_cfg);

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr,
					       (u8 *) &tdm_port->tdm);
	if (ret) {
		pr_err("%s: AFE enable for port 0x%x failed ret = %d\n",
				__func__, port_id, ret);
		goto fail_cmd;
	}

	port_index = afe_get_port_index(port_id);
	if ((port_index >= 0) && (port_index < AFE_MAX_PORTS)) {
		this_afe.afe_sample_rates[port_index] = rate;
	} else {
		pr_err("%s: Invalid port index %d\n", __func__, port_index);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = afe_send_slot_mapping_cfg(&tdm_port->slot_mapping,
					port_id);
	if (ret < 0) {
		pr_err("%s: afe send failed %d\n", __func__, ret);
		goto fail_cmd;
	}

	if (tdm_port->custom_tdm_header.header_type) {
		ret = afe_send_custom_tdm_header_cfg(
			&tdm_port->custom_tdm_header, port_id);
		if (ret < 0) {
			pr_err("%s: afe send failed %d\n", __func__, ret);
			goto fail_cmd;
		}
	}

	ret = afe_send_cmd_port_start(port_id);

fail_cmd:
	return ret;
}
EXPORT_SYMBOL(afe_tdm_port_start);

/**
 * afe_set_cal_mode -
 *         set cal mode for AFE calibration
 *
 * @port_id: AFE port id number
 * @afe_cal_mode: AFE calib mode
 *
 */
void afe_set_cal_mode(u16 port_id, enum afe_cal_mode afe_cal_mode)
{
	uint16_t port_index;

	port_index = afe_get_port_index(port_id);
	this_afe.afe_cal_mode[port_index] = afe_cal_mode;
}
EXPORT_SYMBOL(afe_set_cal_mode);

/**
 * afe_set_vad_cfg -
 *         set configuration for VAD
 *
 * @port_id: AFE port id number
 * @vad_enable: enable/disable vad
 * @preroll_config: Preroll configuration
 *
 */
void afe_set_vad_cfg(u32 vad_enable, u32 preroll_config,
		     u32 port_id)
{
	uint16_t port_index;

	port_index = afe_get_port_index(port_id);
	this_afe.vad_cfg[port_index].is_enable = vad_enable;
	this_afe.vad_cfg[port_index].pre_roll = preroll_config;
}
EXPORT_SYMBOL(afe_set_vad_cfg);

/**
 * afe_get_island_mode_cfg -
 *         get island mode configuration
 *
 * @port_id: AFE port id number
 * @enable_flag: Enable or Disable
 *
 */
void afe_get_island_mode_cfg(u16 port_id, u32 *enable_flag)
{
	uint16_t port_index;

	if (enable_flag) {
		port_index = afe_get_port_index(port_id);
		*enable_flag = this_afe.island_mode[port_index];
	}
}
EXPORT_SYMBOL(afe_get_island_mode_cfg);

/**
 * afe_set_island_mode_cfg -
 *         set island mode configuration
 *
 * @port_id: AFE port id number
 * @enable_flag: Enable or Disable
 *
 */
void afe_set_island_mode_cfg(u16 port_id, u32 enable_flag)
{
	uint16_t port_index;

	port_index = afe_get_port_index(port_id);
	this_afe.island_mode[port_index] = enable_flag;

}
EXPORT_SYMBOL(afe_set_island_mode_cfg);

/**
 * afe_set_routing_callback -
 *         Update callback function for routing
 *
 * @cb: callback function to update with
 *
 */
void afe_set_routing_callback(routing_cb cb)
{
	this_afe.rt_cb = cb;
}
EXPORT_SYMBOL(afe_set_routing_callback);

int afe_port_send_usb_dev_param(u16 port_id, union afe_port_config *afe_config)
{
	struct afe_param_id_usb_audio_dev_params usb_dev;
	struct afe_param_id_usb_audio_dev_lpcm_fmt lpcm_fmt;
	struct afe_param_id_usb_audio_svc_interval svc_int;
	struct param_hdr_v3 param_hdr;
	int ret = 0, index = 0;

	if (!afe_config) {
		pr_err("%s: Error, no configuration data\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	index = q6audio_get_port_index(port_id);

	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		return -EINVAL;
	}
	memset(&usb_dev, 0, sizeof(usb_dev));
	memset(&lpcm_fmt, 0, sizeof(lpcm_fmt));
	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_hdr.instance_id = INSTANCE_ID_0;

	param_hdr.param_id = AFE_PARAM_ID_USB_AUDIO_DEV_PARAMS;
	param_hdr.param_size = sizeof(usb_dev);
	usb_dev.cfg_minor_version = AFE_API_MINOR_VERSION_USB_AUDIO_CONFIG;
	usb_dev.dev_token = afe_config->usb_audio.dev_token;

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) &usb_dev);
	if (ret) {
		pr_err("%s: AFE device param cmd failed %d\n",
			__func__, ret);
		goto exit;
	}

	param_hdr.param_id = AFE_PARAM_ID_USB_AUDIO_DEV_LPCM_FMT;
	param_hdr.param_size = sizeof(lpcm_fmt);
	lpcm_fmt.cfg_minor_version = AFE_API_MINOR_VERSION_USB_AUDIO_CONFIG;
	lpcm_fmt.endian = afe_config->usb_audio.endian;

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) &lpcm_fmt);
	if (ret) {
		pr_err("%s: AFE device param cmd LPCM_FMT failed %d\n",
			__func__, ret);
		goto exit;
	}
	param_hdr.param_id = AFE_PARAM_ID_USB_AUDIO_SVC_INTERVAL;
	param_hdr.param_size = sizeof(svc_int);
	svc_int.cfg_minor_version =
		AFE_API_MINOR_VERSION_USB_AUDIO_CONFIG;
	svc_int.svc_interval = afe_config->usb_audio.service_interval;

	pr_debug("%s: AFE device param cmd sending SVC_INTERVAL %d\n",
			__func__, svc_int.svc_interval);

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) &svc_int);

	if (ret) {
		pr_err("%s: AFE device param cmd svc_interval failed %d\n",
			__func__, ret);
		ret = -EINVAL;
		goto exit;
	}
exit:
	return ret;
}

static int q6afe_send_dec_config(u16 port_id,
			union afe_port_config afe_config,
			struct afe_dec_config *cfg,
			u32 format,
			u16 afe_in_channels, u16 afe_in_bit_width)
{
	struct afe_dec_media_fmt_t dec_media_fmt;
	struct avs_dec_depacketizer_id_param_t dec_depkt_id_param;
	struct avs_dec_congestion_buffer_param_t dec_buffer_id_param;
	struct afe_enc_dec_imc_info_param_t imc_info_param;
	struct afe_port_media_type_t media_type;
	struct param_hdr_v3 param_hdr;
	int ret;
	u32 dec_fmt;

	memset(&dec_depkt_id_param, 0, sizeof(dec_depkt_id_param));
	memset(&dec_media_fmt, 0, sizeof(dec_media_fmt));
	memset(&imc_info_param, 0, sizeof(imc_info_param));
	memset(&media_type, 0, sizeof(media_type));
	memset(&param_hdr, 0, sizeof(param_hdr));

	param_hdr.module_id = AFE_MODULE_ID_DECODER;
	param_hdr.instance_id = INSTANCE_ID_0;

	pr_debug("%s: sending AFE_DECODER_PARAM_ID_DEPACKETIZER to DSP payload\n",
		  __func__);
	param_hdr.param_id = AFE_DECODER_PARAM_ID_DEPACKETIZER_ID;
	param_hdr.param_size = sizeof(struct avs_dec_depacketizer_id_param_t);
	dec_depkt_id_param.dec_depacketizer_id =
					       AFE_MODULE_ID_DEPACKETIZER_COP_V1;
	if (cfg->format == ENC_CODEC_TYPE_LDAC)
		dec_depkt_id_param.dec_depacketizer_id =
					       AFE_MODULE_ID_DEPACKETIZER_COP;
	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr,
					       (u8 *) &dec_depkt_id_param);
	if (ret) {
		pr_err("%s: AFE_DECODER_PARAM_ID_DEPACKETIZER for port 0x%x failed %d\n",
			__func__, port_id, ret);
		goto exit;
	}

	switch (cfg->format) {
	case ASM_MEDIA_FMT_SBC:
	case ASM_MEDIA_FMT_AAC_V2:
	case ASM_MEDIA_FMT_MP3:
		if (port_id == SLIMBUS_9_TX) {
			dec_buffer_id_param.max_nr_buffers  = 200;
			dec_buffer_id_param.pre_buffer_size = 200;
		} else {
			dec_buffer_id_param.max_nr_buffers  = 0;
			dec_buffer_id_param.pre_buffer_size = 0;
		}
		pr_debug("%s: sending AFE_DECODER_PARAM_ID_CONGESTION_BUFFER_SIZE to DSP payload\n",
			  __func__);
		param_hdr.param_id =
			AFE_DECODER_PARAM_ID_CONGESTION_BUFFER_SIZE;
		param_hdr.param_size =
			sizeof(struct avs_dec_congestion_buffer_param_t);
		dec_buffer_id_param.version = 0;
		ret = q6afe_pack_and_set_param_in_band(port_id,
						q6audio_get_port_index(port_id),
						param_hdr,
						(u8 *) &dec_buffer_id_param);
		if (ret) {
			pr_err("%s: AFE_DECODER_PARAM_ID_CONGESTION_BUFFER_SIZE for port 0x%x failed %d\n",
				__func__, port_id, ret);
			goto exit;
		}
		break;
	default:
		pr_debug("%s:sending AFE_ENCDEC_PARAM_ID_DEC_TO_ENC_COMMUNICATION to DSP payload\n",
			  __func__);
		param_hdr.param_id =
			AFE_ENCDEC_PARAM_ID_DEC_TO_ENC_COMMUNICATION;
		param_hdr.param_size =
			sizeof(struct afe_enc_dec_imc_info_param_t);
		imc_info_param.imc_info = cfg->abr_dec_cfg.imc_info;
		ret = q6afe_pack_and_set_param_in_band(port_id,
						q6audio_get_port_index(port_id),
						param_hdr,
						(u8 *) &imc_info_param);
		if (ret) {
			pr_err("%s: AFE_ENCDEC_PARAM_ID_DEC_TO_ENC_COMMUNICATION for port 0x%x failed %d\n",
				__func__, port_id, ret);
			goto exit;
		}
		break;
	}

	pr_debug("%s:Sending AFE_API_VERSION_PORT_MEDIA_TYPE to DSP", __func__);
	param_hdr.module_id = AFE_MODULE_PORT;
	param_hdr.param_id = AFE_PARAM_ID_PORT_MEDIA_TYPE;
	param_hdr.param_size = sizeof(struct afe_port_media_type_t);
	media_type.minor_version = AFE_API_VERSION_PORT_MEDIA_TYPE;
	switch (cfg->format) {
	case ASM_MEDIA_FMT_AAC_V2:
		media_type.sample_rate =
			cfg->data.aac_config.sample_rate;
		break;
	default:
		media_type.sample_rate =
			afe_config.slim_sch.sample_rate;
	}
	if (afe_in_bit_width)
		media_type.bit_width = afe_in_bit_width;
	else
		media_type.bit_width = afe_config.slim_sch.bit_width;

	if (afe_in_channels)
		media_type.num_channels = afe_in_channels;
	else
		media_type.num_channels = afe_config.slim_sch.num_channels;
	media_type.data_format = AFE_PORT_DATA_FORMAT_PCM;
	media_type.reserved = 0;

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) &media_type);
	if (ret) {
		pr_err("%s: AFE_API_VERSION_PORT_MEDIA_TYPE for port 0x%x failed %d\n",
			__func__, port_id, ret);
		goto exit;
	}

	if (format != ASM_MEDIA_FMT_SBC && format != ASM_MEDIA_FMT_AAC_V2) {
		pr_debug("%s:Unsuppported dec format. Ignore AFE config %u\n",
				__func__, format);
		goto exit;
	}
	pr_debug("%s: sending AFE_DECODER_PARAM_ID_DEC_MEDIA_FMT to DSP payload\n",
		  __func__);
	param_hdr.module_id = AFE_MODULE_ID_DECODER;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_DECODER_PARAM_ID_DEC_FMT_ID;
	param_hdr.param_size = sizeof(dec_fmt);
	dec_fmt = format;
	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) &dec_fmt);
	if (ret) {
		pr_err("%s: AFE_DECODER_PARAM_ID_DEC_MEDIA_FMT for port 0x%x failed %d\n",
			__func__, port_id, ret);
		goto exit;
	}

	switch (cfg->format) {
	case ASM_MEDIA_FMT_AAC_V2:
		param_hdr.param_size = sizeof(struct afe_dec_media_fmt_t);

		pr_debug("%s:send AFE_DECODER_PARAM_ID DEC_MEDIA_FMT to DSP payload\n",
			 __func__);
		param_hdr.param_id = AVS_DECODER_PARAM_ID_DEC_MEDIA_FMT;
		dec_media_fmt.dec_media_config = cfg->data;
		ret = q6afe_pack_and_set_param_in_band(port_id,
						q6audio_get_port_index(port_id),
						param_hdr,
						(u8 *) &dec_media_fmt);
		if (ret) {
			pr_err("%s: AFE_DECODER_PARAM_ID DEC_MEDIA_FMT for port 0x%x failed %d\n",
				__func__, port_id, ret);
			goto exit;
		}
		break;
	default:
		pr_debug("%s:No need to send DEC_MEDIA_FMT to DSP payload\n",
			 __func__);
	}

exit:
	return ret;
}

static int q6afe_send_enc_config(u16 port_id,
				 union afe_enc_config_data *cfg, u32 format,
				 union afe_port_config afe_config,
				 u16 afe_in_channels, u16 afe_in_bit_width,
				 u32 scrambler_mode, u32 mono_mode)
{
	u32 enc_fmt;
	struct afe_enc_cfg_blk_param_t enc_blk_param;
	struct afe_param_id_aptx_sync_mode sync_mode_param;
	struct afe_id_aptx_adaptive_enc_init aptx_adaptive_enc_init;
	struct avs_enc_packetizer_id_param_t enc_pkt_id_param;
	struct avs_enc_set_scrambler_param_t enc_set_scrambler_param;
	struct afe_enc_level_to_bitrate_map_param_t map_param;
	struct afe_enc_dec_imc_info_param_t imc_info_param;
	struct asm_aac_frame_size_control_t frame_ctl_param;
	struct afe_port_media_type_t media_type;
	struct aptx_channel_mode_param_t channel_mode_param;
	struct param_hdr_v3 param_hdr;
	int ret;

	pr_debug("%s:update DSP for enc format = %d\n", __func__, format);

	memset(&enc_blk_param, 0, sizeof(enc_blk_param));
	memset(&sync_mode_param, 0, sizeof(sync_mode_param));
	memset(&aptx_adaptive_enc_init, 0, sizeof(aptx_adaptive_enc_init));
	memset(&enc_pkt_id_param, 0, sizeof(enc_pkt_id_param));
	memset(&enc_set_scrambler_param, 0, sizeof(enc_set_scrambler_param));
	memset(&map_param, 0, sizeof(map_param));
	memset(&imc_info_param, 0, sizeof(imc_info_param));
	memset(&frame_ctl_param, 0, sizeof(frame_ctl_param));
	memset(&media_type, 0, sizeof(media_type));
	memset(&param_hdr, 0, sizeof(param_hdr));

	if (format != ASM_MEDIA_FMT_SBC && format != ASM_MEDIA_FMT_AAC_V2 &&
		format != ASM_MEDIA_FMT_APTX && format != ASM_MEDIA_FMT_APTX_HD &&
		format != ASM_MEDIA_FMT_CELT && format != ASM_MEDIA_FMT_LDAC &&
		format != ASM_MEDIA_FMT_APTX_ADAPTIVE) {
		pr_err("%s:Unsuppported enc format. Ignore AFE config\n",
				__func__);
		return 0;
	}

	param_hdr.module_id = AFE_MODULE_ID_ENCODER;
	param_hdr.instance_id = INSTANCE_ID_0;

	param_hdr.param_id = AFE_ENCODER_PARAM_ID_ENC_FMT_ID;
	param_hdr.param_size = sizeof(enc_fmt);
	enc_fmt = format;
	pr_debug("%s:sending AFE_ENCODER_PARAM_ID_ENC_FMT_ID payload\n",
		 __func__);
	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) &enc_fmt);
	if (ret) {
		pr_err("%s:unable to send AFE_ENCODER_PARAM_ID_ENC_FMT_ID",
				__func__);
		goto exit;
	}

	if (format == ASM_MEDIA_FMT_LDAC) {
		param_hdr.param_size = sizeof(struct afe_enc_cfg_blk_param_t)
					    - sizeof(struct afe_abr_enc_cfg_t);
		enc_blk_param.enc_cfg_blk_size =
				sizeof(union afe_enc_config_data)
					- sizeof(struct afe_abr_enc_cfg_t);
	} else if (format == ASM_MEDIA_FMT_AAC_V2) {
		param_hdr.param_size = sizeof(enc_blk_param)
				- sizeof(struct asm_aac_frame_size_control_t);
		enc_blk_param.enc_cfg_blk_size =
				sizeof(enc_blk_param.enc_blk_config)
				- sizeof(struct asm_aac_frame_size_control_t);
	} else {
		param_hdr.param_size = sizeof(struct afe_enc_cfg_blk_param_t);
		enc_blk_param.enc_cfg_blk_size =
			sizeof(union afe_enc_config_data);
	}
	pr_debug("%s:send AFE_ENCODER_PARAM_ID_ENC_CFG_BLK to DSP payload\n",
		 __func__);
	param_hdr.param_id = AFE_ENCODER_PARAM_ID_ENC_CFG_BLK;
	enc_blk_param.enc_blk_config = *cfg;
	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr,
					       (u8 *) &enc_blk_param);
	if (ret) {
		pr_err("%s: AFE_ENCODER_PARAM_ID_ENC_CFG_BLK for port 0x%x failed %d\n",
			__func__, port_id, ret);
		goto exit;
	}

	if (format == ASM_MEDIA_FMT_AAC_V2) {
		uint32_t frame_size_ctl_value = enc_blk_param.enc_blk_config.
				aac_config.frame_ctl.ctl_value;
		if (frame_size_ctl_value > 0) {
			param_hdr.param_id =
				AFE_PARAM_ID_AAC_FRM_SIZE_CONTROL;
			param_hdr.param_size = sizeof(frame_ctl_param);
			frame_ctl_param.ctl_type = enc_blk_param.
				enc_blk_config.aac_config.frame_ctl.ctl_type;
			frame_ctl_param.ctl_value = frame_size_ctl_value;
			pr_debug("%s: send AFE_PARAM_ID_AAC_FRM_SIZE_CONTROL\n",
				  __func__);
			ret = q6afe_pack_and_set_param_in_band(port_id,
					q6audio_get_port_index(port_id),
					param_hdr,
					(u8 *) &frame_ctl_param);
			if (ret) {
				pr_err("%s: AAC_FRM_SIZE_CONTROL failed %d\n",
					__func__, ret);
				goto exit;
			}
		}
	}

	if (format == ASM_MEDIA_FMT_APTX) {
		pr_debug("%s: sending AFE_PARAM_ID_APTX_SYNC_MODE to DSP",
			__func__);
		param_hdr.param_id = AFE_PARAM_ID_APTX_SYNC_MODE;
		param_hdr.param_size =
			sizeof(struct afe_param_id_aptx_sync_mode);
		sync_mode_param.sync_mode =
			enc_blk_param.enc_blk_config.aptx_config.
				aptx_v2_cfg.sync_mode;
		ret = q6afe_pack_and_set_param_in_band(port_id,
					q6audio_get_port_index(port_id),
					param_hdr,
					(u8 *) &sync_mode_param);
		if (ret) {
			pr_err("%s: AFE_PARAM_ID_APTX_SYNC_MODE for port 0x%x failed %d\n",
				__func__, port_id, ret);
			goto exit;
		}
	}
	if (format == ASM_MEDIA_FMT_APTX_ADAPTIVE) {
		pr_debug("%s: sending AFE_ID_APTX_ADAPTIVE_ENC_INIT to DSP\n",
			__func__);
		param_hdr.param_id = AFE_ID_APTX_ADAPTIVE_ENC_INIT;
		param_hdr.param_size =
			sizeof(struct afe_id_aptx_adaptive_enc_init);
		aptx_adaptive_enc_init =
			enc_blk_param.enc_blk_config.aptx_ad_config.
				aptx_ad_cfg;
		ret = q6afe_pack_and_set_param_in_band(port_id,
					q6audio_get_port_index(port_id),
					param_hdr,
					(u8 *) &aptx_adaptive_enc_init);
		if (ret) {
			pr_err("%s: AFE_ID_APTX_ADAPTIVE_ENC_INIT for port 0x%x failed %d\n",
				__func__, port_id, ret);
			goto exit;
		}
	}
	pr_debug("%s:sending AFE_ENCODER_PARAM_ID_PACKETIZER to DSP\n",
		 __func__);
	param_hdr.param_id = AFE_ENCODER_PARAM_ID_PACKETIZER_ID;
	param_hdr.param_size = sizeof(struct avs_enc_packetizer_id_param_t);
	enc_pkt_id_param.enc_packetizer_id = AFE_MODULE_ID_PACKETIZER_COP;
	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr,
					       (u8 *) &enc_pkt_id_param);
	if (ret) {
		pr_err("%s: AFE_ENCODER_PARAM_ID_PACKETIZER for port 0x%x failed %d\n",
			__func__, port_id, ret);
		goto exit;
	}

	pr_debug("%s:sending AFE_ENCODER_PARAM_ID_ENABLE_SCRAMBLING mode= %d to DSP payload\n",
		  __func__, scrambler_mode);
	param_hdr.param_id = AFE_ENCODER_PARAM_ID_ENABLE_SCRAMBLING;
	param_hdr.param_size = sizeof(struct avs_enc_set_scrambler_param_t);
	enc_set_scrambler_param.enable_scrambler = scrambler_mode;
	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr,
					       (u8 *) &enc_set_scrambler_param);
	if (ret) {
		pr_err("%s: AFE_ENCODER_PARAM_ID_ENABLE_SCRAMBLING for port 0x%x failed %d\n",
			__func__, port_id, ret);
		goto exit;
	}

	if (format == ASM_MEDIA_FMT_APTX) {
		pr_debug("%s:sending CAPI_V2_PARAM_ID_APTX_ENC_SWITCH_TO_MONO mode= %d to DSP payload\n",
			__func__, mono_mode);
		param_hdr.param_id = CAPI_V2_PARAM_ID_APTX_ENC_SWITCH_TO_MONO;
		param_hdr.param_size = sizeof(channel_mode_param);
		channel_mode_param.channel_mode = mono_mode;
		ret = q6afe_pack_and_set_param_in_band(port_id,
				q6audio_get_port_index(port_id),
							param_hdr,
					(u8 *) &channel_mode_param);

		if (ret) {
			pr_err("%s: CAPI_V2_PARAM_ID_APTX_ENC_SWITCH_TO_MONO for port 0x%x failed %d\n",
				__func__, port_id, ret);
		}
	}

	if ((format == ASM_MEDIA_FMT_LDAC &&
	     cfg->ldac_config.abr_config.is_abr_enabled) ||
	     format == ASM_MEDIA_FMT_APTX_ADAPTIVE) {
		pr_debug("%s:sending AFE_ENCODER_PARAM_ID_BIT_RATE_LEVEL_MAP to DSP payload",
			__func__);
		param_hdr.param_id = AFE_ENCODER_PARAM_ID_BIT_RATE_LEVEL_MAP;
		param_hdr.param_size =
			sizeof(struct afe_enc_level_to_bitrate_map_param_t);
		map_param.mapping_table =
			cfg->ldac_config.abr_config.mapping_info;
		ret = q6afe_pack_and_set_param_in_band(port_id,
						q6audio_get_port_index(port_id),
						param_hdr,
						(u8 *) &map_param);
		if (ret) {
			pr_err("%s: AFE_ENCODER_PARAM_ID_BIT_RATE_LEVEL_MAP for port 0x%x failed %d\n",
				__func__, port_id, ret);
			goto exit;
		}

		pr_debug("%s: sending AFE_ENCDEC_PARAM_ID_DEC_TO_ENC_COMMUNICATION to DSP payload",
				__func__);
		param_hdr.param_id =
			AFE_ENCDEC_PARAM_ID_DEC_TO_ENC_COMMUNICATION;
		param_hdr.param_size =
			sizeof(struct afe_enc_dec_imc_info_param_t);
		if (format == ASM_MEDIA_FMT_APTX_ADAPTIVE)
			imc_info_param.imc_info =
			cfg->aptx_ad_config.abr_cfg.imc_info;
		else
			imc_info_param.imc_info =
			cfg->ldac_config.abr_config.imc_info;
		ret = q6afe_pack_and_set_param_in_band(port_id,
						q6audio_get_port_index(port_id),
						param_hdr,
						(u8 *) &imc_info_param);
		if (ret) {
			pr_err("%s: AFE_ENCDEC_PARAM_ID_DEC_TO_ENC_COMMUNICATION for port 0x%x failed %d\n",
					__func__, port_id, ret);
			goto exit;
		}
	}

	pr_debug("%s:Sending AFE_API_VERSION_PORT_MEDIA_TYPE to DSP", __func__);
	param_hdr.module_id = AFE_MODULE_PORT;
	param_hdr.param_id = AFE_PARAM_ID_PORT_MEDIA_TYPE;
	param_hdr.param_size = sizeof(struct afe_port_media_type_t);
	media_type.minor_version = AFE_API_VERSION_PORT_MEDIA_TYPE;
	if (format == ASM_MEDIA_FMT_LDAC)
		media_type.sample_rate =
			cfg->ldac_config.custom_config.sample_rate;
	else if (format == ASM_MEDIA_FMT_APTX_ADAPTIVE)
		media_type.sample_rate =
			cfg->aptx_ad_config.custom_cfg.sample_rate;
	else
		media_type.sample_rate =
			afe_config.slim_sch.sample_rate;

	if (afe_in_bit_width)
		media_type.bit_width = afe_in_bit_width;
	else
		media_type.bit_width = afe_config.slim_sch.bit_width;

	if (afe_in_channels)
		media_type.num_channels = afe_in_channels;
	else
		media_type.num_channels = afe_config.slim_sch.num_channels;
	media_type.data_format = AFE_PORT_DATA_FORMAT_PCM;
	media_type.reserved = 0;

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) &media_type);
	if (ret) {
		pr_err("%s: AFE_API_VERSION_PORT_MEDIA_TYPE for port 0x%x failed %d\n",
			__func__, port_id, ret);
		goto exit;
	}

exit:
	return ret;
}

int afe_set_tws_channel_mode(u16 port_id, u32 channel_mode)
{
	struct aptx_channel_mode_param_t channel_mode_param;
	struct param_hdr_v3 param_info;
	int ret = 0;

	memset(&param_info, 0, sizeof(param_info));
	memset(&channel_mode_param, 0, sizeof(channel_mode_param));

	param_info.module_id = AFE_MODULE_ID_ENCODER;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = CAPI_V2_PARAM_ID_APTX_ENC_SWITCH_TO_MONO;
	param_info.param_size = sizeof(channel_mode_param);

	channel_mode_param.channel_mode = channel_mode;

	ret = q6afe_pack_and_set_param_in_band(port_id,
			q6audio_get_port_index(port_id),
						param_info,
				(u8 *) &channel_mode_param);
	if (ret)
		pr_err("%s: AFE set channel mode cfg for port 0x%x failed %d\n",
			 __func__, port_id, ret);

	return ret;
}
EXPORT_SYMBOL(afe_set_tws_channel_mode);

static int __afe_port_start(u16 port_id, union afe_port_config *afe_config,
			    u32 rate, u16 afe_in_channels, u16 afe_in_bit_width,
			    union afe_enc_config_data *enc_cfg,
			    u32 codec_format, u32 scrambler_mode, u32 mono_mode,
			    struct afe_dec_config *dec_cfg)
{
	union afe_port_config port_cfg;
	struct param_hdr_v3 param_hdr;
	int ret = 0;
	int cfg_type;
	int index = 0;
	enum afe_mad_type mad_type;
	uint16_t port_index;

	memset(&param_hdr, 0, sizeof(param_hdr));
	memset(&port_cfg, 0, sizeof(port_cfg));

	if (!afe_config) {
		pr_err("%s: Error, no configuration data\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	if ((port_id == RT_PROXY_DAI_001_RX) ||
		(port_id == RT_PROXY_DAI_002_TX)) {
		pr_debug("%s: before incrementing pcm_afe_instance %d port_id 0x%x\n",
			__func__,
			pcm_afe_instance[port_id & 0x1], port_id);
		port_id = VIRTUAL_ID_TO_PORTID(port_id);
		pcm_afe_instance[port_id & 0x1]++;
		return 0;
	}
	if ((port_id == RT_PROXY_DAI_002_RX) ||
			(port_id == RT_PROXY_DAI_001_TX)) {
		pr_debug("%s: before incrementing proxy_afe_instance %d port_id 0x%x\n",
			__func__,
			proxy_afe_instance[port_id & 0x1], port_id);

		if (!afe_close_done[port_id & 0x1]) {
			/*close pcm dai corresponding to the proxy dai*/
			afe_close(port_id - 0x10);
			pcm_afe_instance[port_id & 0x1]++;
			pr_debug("%s: reconfigure afe port again\n", __func__);
		}
		proxy_afe_instance[port_id & 0x1]++;
		afe_close_done[port_id & 0x1] = false;
		port_id = VIRTUAL_ID_TO_PORTID(port_id);
	}

	pr_debug("%s: port id: 0x%x\n", __func__, port_id);

	index = q6audio_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		return -EINVAL;
	}
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: port id: 0x%x ret %d\n", __func__, port_id, ret);
		return -EINVAL;
	}

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	if ((index >= 0) && (index < AFE_MAX_PORTS)) {
		this_afe.afe_sample_rates[index] = rate;

		if (this_afe.rt_cb)
			this_afe.dev_acdb_id[index] = this_afe.rt_cb(port_id);
	}

	mutex_lock(&this_afe.afe_cmd_lock);
	port_index = afe_get_port_index(port_id);

	if (q6core_get_avcs_api_version_per_service(
		APRV2_IDS_SERVICE_ID_ADSP_AFE_V) >= AFE_API_VERSION_V4) {
		/* send VAD configuration if is enabled */
		if (this_afe.vad_cfg[port_index].is_enable) {
			ret = afe_send_port_vad_cfg_params(port_id);
			if (ret) {
				pr_err("%s: afe send VAD config failed %d\n",
					__func__, ret);
				goto fail_cmd;
			}
		}
	}

	/* Also send the topology id here: */
	if (!(this_afe.afe_cal_mode[port_index] == AFE_CAL_MODE_NONE)) {
		/* One time call: only for first time */
		afe_send_custom_topology();
		afe_send_port_topology_id(port_id);
		afe_send_cal(port_id);
		afe_send_hw_delay(port_id, rate);
	}

	/* Start SW MAD module */
	mad_type = afe_port_get_mad_type(port_id);
	pr_debug("%s: port_id 0x%x, mad_type %d\n", __func__, port_id,
		 mad_type);
	if (mad_type != MAD_HW_NONE && mad_type != MAD_SW_AUDIO) {
		if (!afe_has_config(AFE_CDC_REGISTERS_CONFIG) ||
			!afe_has_config(AFE_SLIMBUS_SLAVE_CONFIG)) {
			pr_err("%s: AFE isn't configured yet for\n"
				"HW MAD try Again\n", __func__);
				ret = -EAGAIN;
				goto fail_cmd;
		}
		ret = afe_turn_onoff_hw_mad(mad_type, true);
		if (ret) {
			pr_err("%s: afe_turn_onoff_hw_mad failed %d\n",
			       __func__, ret);
			goto fail_cmd;
		}
	}

	if ((this_afe.aanc_info.aanc_active) &&
	    (this_afe.aanc_info.aanc_tx_port == port_id)) {
		this_afe.aanc_info.aanc_tx_port_sample_rate = rate;
		port_index =
			afe_get_port_index(this_afe.aanc_info.aanc_rx_port);
		if ((port_index >= 0) && (port_index < AFE_MAX_PORTS)) {
			this_afe.aanc_info.aanc_rx_port_sample_rate =
				this_afe.afe_sample_rates[port_index];
		} else {
			pr_err("%s: Invalid port index %d\n",
				__func__, port_index);
			ret = -EINVAL;
			goto fail_cmd;
		}
		ret = afe_aanc_start(this_afe.aanc_info.aanc_tx_port,
				this_afe.aanc_info.aanc_rx_port);
		pr_debug("%s: afe_aanc_start ret %d\n", __func__, ret);
	}

	if ((port_id == AFE_PORT_ID_USB_RX) ||
	    (port_id == AFE_PORT_ID_USB_TX)) {
		ret = afe_port_send_usb_dev_param(port_id, afe_config);
		if (ret) {
			pr_err("%s: AFE device param for port 0x%x failed %d\n",
				   __func__, port_id, ret);
			ret = -EINVAL;
			goto fail_cmd;
		}
	}

	switch (port_id) {
	case AFE_PORT_ID_PRIMARY_PCM_RX:
	case AFE_PORT_ID_PRIMARY_PCM_TX:
	case AFE_PORT_ID_SECONDARY_PCM_RX:
	case AFE_PORT_ID_SECONDARY_PCM_TX:
	case AFE_PORT_ID_TERTIARY_PCM_RX:
	case AFE_PORT_ID_TERTIARY_PCM_TX:
	case AFE_PORT_ID_QUATERNARY_PCM_RX:
	case AFE_PORT_ID_QUATERNARY_PCM_TX:
	case AFE_PORT_ID_QUINARY_PCM_RX:
	case AFE_PORT_ID_QUINARY_PCM_TX:
	case AFE_PORT_ID_SENARY_PCM_RX:
	case AFE_PORT_ID_SENARY_PCM_TX:
		cfg_type = AFE_PARAM_ID_PCM_CONFIG;
		break;
	case PRIMARY_I2S_RX:
	case PRIMARY_I2S_TX:
	case SECONDARY_I2S_RX:
	case SECONDARY_I2S_TX:
	case MI2S_RX:
	case MI2S_TX:
	case AFE_PORT_ID_PRIMARY_MI2S_RX:
	case AFE_PORT_ID_PRIMARY_MI2S_TX:
	case AFE_PORT_ID_SECONDARY_MI2S_RX:
	case AFE_PORT_ID_SECONDARY_MI2S_RX_SD1:
	case AFE_PORT_ID_SECONDARY_MI2S_TX:
	case AFE_PORT_ID_TERTIARY_MI2S_RX:
	case AFE_PORT_ID_TERTIARY_MI2S_TX:
	case AFE_PORT_ID_QUATERNARY_MI2S_RX:
	case AFE_PORT_ID_QUATERNARY_MI2S_TX:
	case AFE_PORT_ID_QUINARY_MI2S_RX:
	case AFE_PORT_ID_QUINARY_MI2S_TX:
	case AFE_PORT_ID_SENARY_MI2S_TX:
	case AFE_PORT_ID_INT0_MI2S_RX:
	case AFE_PORT_ID_INT0_MI2S_TX:
	case AFE_PORT_ID_INT1_MI2S_RX:
	case AFE_PORT_ID_INT1_MI2S_TX:
	case AFE_PORT_ID_INT2_MI2S_RX:
	case AFE_PORT_ID_INT2_MI2S_TX:
	case AFE_PORT_ID_INT3_MI2S_RX:
	case AFE_PORT_ID_INT3_MI2S_TX:
	case AFE_PORT_ID_INT4_MI2S_RX:
	case AFE_PORT_ID_INT4_MI2S_TX:
	case AFE_PORT_ID_INT5_MI2S_RX:
	case AFE_PORT_ID_INT5_MI2S_TX:
	case AFE_PORT_ID_INT6_MI2S_RX:
	case AFE_PORT_ID_INT6_MI2S_TX:
		cfg_type = AFE_PARAM_ID_I2S_CONFIG;
		break;
	case HDMI_RX:
	case DISPLAY_PORT_RX:
		cfg_type = AFE_PARAM_ID_HDMI_CONFIG;
		break;
	case VOICE_PLAYBACK_TX:
	case VOICE2_PLAYBACK_TX:
	case VOICE_RECORD_RX:
	case VOICE_RECORD_TX:
		cfg_type = AFE_PARAM_ID_PSEUDO_PORT_CONFIG;
		break;
	case SLIMBUS_0_RX:
	case SLIMBUS_0_TX:
	case SLIMBUS_1_RX:
	case SLIMBUS_1_TX:
	case SLIMBUS_2_RX:
	case SLIMBUS_2_TX:
	case SLIMBUS_3_RX:
	case SLIMBUS_3_TX:
	case SLIMBUS_4_RX:
	case SLIMBUS_4_TX:
	case SLIMBUS_5_RX:
	case SLIMBUS_5_TX:
	case SLIMBUS_6_RX:
	case SLIMBUS_6_TX:
	case SLIMBUS_7_RX:
	case SLIMBUS_7_TX:
	case SLIMBUS_8_RX:
	case SLIMBUS_8_TX:
	case SLIMBUS_9_RX:
	case SLIMBUS_9_TX:
		cfg_type = AFE_PARAM_ID_SLIMBUS_CONFIG;
		break;
	case AFE_PORT_ID_USB_RX:
	case AFE_PORT_ID_USB_TX:
		cfg_type = AFE_PARAM_ID_USB_AUDIO_CONFIG;
		break;
	case RT_PROXY_PORT_001_RX:
	case RT_PROXY_PORT_001_TX:
		cfg_type = AFE_PARAM_ID_RT_PROXY_CONFIG;
		break;
	case INT_BT_SCO_RX:
	case INT_BT_A2DP_RX:
	case INT_BT_SCO_TX:
	case INT_FM_RX:
	case INT_FM_TX:
		cfg_type = AFE_PARAM_ID_INTERNAL_BT_FM_CONFIG;
		break;
	case AFE_PORT_ID_WSA_CODEC_DMA_RX_0:
	case AFE_PORT_ID_WSA_CODEC_DMA_TX_0:
	case AFE_PORT_ID_WSA_CODEC_DMA_RX_1:
	case AFE_PORT_ID_WSA_CODEC_DMA_TX_1:
	case AFE_PORT_ID_WSA_CODEC_DMA_TX_2:
	case AFE_PORT_ID_VA_CODEC_DMA_TX_0:
	case AFE_PORT_ID_VA_CODEC_DMA_TX_1:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_0:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_0:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_1:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_1:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_2:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_2:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_3:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_3:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_4:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_4:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_5:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_5:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_6:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_7:
		cfg_type = AFE_PARAM_ID_CODEC_DMA_CONFIG;
		break;
	default:
		pr_err("%s: Invalid port id 0x%x\n", __func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	param_hdr.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = cfg_type;
	param_hdr.param_size = sizeof(union afe_port_config);

	port_cfg = *afe_config;
	if (((enc_cfg != NULL) || (dec_cfg != NULL)) &&
	    (codec_format != ASM_MEDIA_FMT_NONE) &&
	    (cfg_type == AFE_PARAM_ID_SLIMBUS_CONFIG)) {
		port_cfg.slim_sch.data_format =
			AFE_SB_DATA_FORMAT_GENERIC_COMPRESSED;
	}
	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) &port_cfg);
	if (ret) {
		pr_err("%s: AFE enable for port 0x%x failed %d\n",
			__func__, port_id, ret);
		goto fail_cmd;
	}

	if ((codec_format != ASM_MEDIA_FMT_NONE) &&
	    (cfg_type == AFE_PARAM_ID_SLIMBUS_CONFIG)) {
		if (enc_cfg != NULL) {
			pr_debug("%s: Found AFE encoder support for SLIMBUS format = %d\n",
						__func__, codec_format);
			ret = q6afe_send_enc_config(port_id, enc_cfg,
						    codec_format, *afe_config,
						    afe_in_channels,
						    afe_in_bit_width,
						    scrambler_mode, mono_mode);
			if (ret) {
				pr_err("%s: AFE encoder config for port 0x%x failed %d\n",
					__func__, port_id, ret);
				goto fail_cmd;
			}
		}
		if (dec_cfg != NULL) {
			pr_debug("%s: Found AFE decoder support for SLIMBUS format = %d\n",
				  __func__, codec_format);
			ret = q6afe_send_dec_config(port_id, *afe_config,
						    dec_cfg, codec_format,
						    afe_in_channels,
						    afe_in_bit_width);
			if (ret) {
				pr_err("%s: AFE decoder config for port 0x%x failed %d\n",
					 __func__, port_id, ret);
				goto fail_cmd;
			}
		}
	}

	port_index = afe_get_port_index(port_id);
	if ((port_index >= 0) && (port_index < AFE_MAX_PORTS)) {
		/*
		 * If afe_port_start() for tx port called before
		 * rx port, then aanc rx sample rate is zero. So,
		 * AANC state machine in AFE will not get triggered.
		 * Make sure to check whether aanc is active during
		 * afe_port_start() for rx port and if aanc rx
		 * sample rate is zero, call afe_aanc_start to configure
		 * aanc with valid sample rates.
		 */
		if (this_afe.aanc_info.aanc_active &&
		    !this_afe.aanc_info.aanc_rx_port_sample_rate) {
			this_afe.aanc_info.aanc_rx_port_sample_rate =
				this_afe.afe_sample_rates[port_index];
			ret = afe_aanc_start(this_afe.aanc_info.aanc_tx_port,
					this_afe.aanc_info.aanc_rx_port);
			pr_debug("%s: afe_aanc_start ret %d\n", __func__, ret);
		}
	} else {
		pr_err("%s: Invalid port index %d\n", __func__, port_index);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = afe_send_cmd_port_start(port_id);
#if CONFIG_MSM_CSPL
	if (ret == 0)
		crus_afe_port_start(port_id);
#endif


fail_cmd:
	mutex_unlock(&this_afe.afe_cmd_lock);
	return ret;
}

/**
 * afe_port_start - to configure AFE session with
 * specified port configuration
 *
 * @port_id: AFE port id number
 * @afe_config: port configutation
 * @rate: sampling rate of port
 *
 * Returns 0 on success or error value on port start failure.
 */
int afe_port_start(u16 port_id, union afe_port_config *afe_config,
		   u32 rate)
{
	return __afe_port_start(port_id, afe_config, rate,
				0, 0, NULL, ASM_MEDIA_FMT_NONE, 0, 0, NULL);
}
EXPORT_SYMBOL(afe_port_start);

/**
 * afe_port_start_v2 - to configure AFE session with
 * specified port configuration and encoder /decoder params
 *
 * @port_id: AFE port id number
 * @afe_config: port configutation
 * @rate: sampling rate of port
 * @enc_cfg: AFE enc configuration information to setup encoder
 * @afe_in_channels: AFE input channel configuration, this needs
 *  update only if input channel is differ from AFE output
 * @dec_cfg: AFE dec configuration information to set up decoder
 *
 * Returns 0 on success or error value on port start failure.
 */
int afe_port_start_v2(u16 port_id, union afe_port_config *afe_config,
		      u32 rate, u16 afe_in_channels, u16 afe_in_bit_width,
		      struct afe_enc_config *enc_cfg,
		      struct afe_dec_config *dec_cfg)
{
	int ret = 0;

	if (enc_cfg != NULL)
		ret = __afe_port_start(port_id, afe_config, rate,
					afe_in_channels, afe_in_bit_width,
					&enc_cfg->data, enc_cfg->format,
					enc_cfg->scrambler_mode,
					enc_cfg->mono_mode, dec_cfg);
	else if (dec_cfg != NULL)
		ret = __afe_port_start(port_id, afe_config, rate,
					afe_in_channels, afe_in_bit_width,
					NULL, dec_cfg->format, 0, 0, dec_cfg);

	return ret;
}
EXPORT_SYMBOL(afe_port_start_v2);

int afe_get_port_index(u16 port_id)
{
	switch (port_id) {
	case PRIMARY_I2S_RX: return IDX_PRIMARY_I2S_RX;
	case PRIMARY_I2S_TX: return IDX_PRIMARY_I2S_TX;
	case AFE_PORT_ID_PRIMARY_PCM_RX:
		return IDX_AFE_PORT_ID_PRIMARY_PCM_RX;
	case AFE_PORT_ID_PRIMARY_PCM_TX:
		return IDX_AFE_PORT_ID_PRIMARY_PCM_TX;
	case AFE_PORT_ID_SECONDARY_PCM_RX:
		return IDX_AFE_PORT_ID_SECONDARY_PCM_RX;
	case AFE_PORT_ID_SECONDARY_PCM_TX:
		return IDX_AFE_PORT_ID_SECONDARY_PCM_TX;
	case AFE_PORT_ID_TERTIARY_PCM_RX:
		return IDX_AFE_PORT_ID_TERTIARY_PCM_RX;
	case AFE_PORT_ID_TERTIARY_PCM_TX:
		return IDX_AFE_PORT_ID_TERTIARY_PCM_TX;
	case AFE_PORT_ID_QUATERNARY_PCM_RX:
		return IDX_AFE_PORT_ID_QUATERNARY_PCM_RX;
	case AFE_PORT_ID_QUATERNARY_PCM_TX:
		return IDX_AFE_PORT_ID_QUATERNARY_PCM_TX;
	case AFE_PORT_ID_QUINARY_PCM_RX:
		return IDX_AFE_PORT_ID_QUINARY_PCM_RX;
	case AFE_PORT_ID_QUINARY_PCM_TX:
		return IDX_AFE_PORT_ID_QUINARY_PCM_TX;
	case AFE_PORT_ID_SENARY_PCM_RX:
		return IDX_AFE_PORT_ID_SENARY_PCM_RX;
	case AFE_PORT_ID_SENARY_PCM_TX:
		return IDX_AFE_PORT_ID_SENARY_PCM_TX;
	case SECONDARY_I2S_RX: return IDX_SECONDARY_I2S_RX;
	case SECONDARY_I2S_TX: return IDX_SECONDARY_I2S_TX;
	case MI2S_RX: return IDX_MI2S_RX;
	case MI2S_TX: return IDX_MI2S_TX;
	case HDMI_RX: return IDX_HDMI_RX;
	case DISPLAY_PORT_RX: return IDX_DISPLAY_PORT_RX;
	case AFE_PORT_ID_PRIMARY_SPDIF_RX: return IDX_PRIMARY_SPDIF_RX;
	case AFE_PORT_ID_PRIMARY_SPDIF_TX: return IDX_PRIMARY_SPDIF_TX;
	case AFE_PORT_ID_SECONDARY_SPDIF_RX: return IDX_SECONDARY_SPDIF_RX;
	case AFE_PORT_ID_SECONDARY_SPDIF_TX: return IDX_SECONDARY_SPDIF_TX;
	case RSVD_2: return IDX_RSVD_2;
	case RSVD_3: return IDX_RSVD_3;
	case DIGI_MIC_TX: return IDX_DIGI_MIC_TX;
	case VOICE_RECORD_RX: return IDX_VOICE_RECORD_RX;
	case VOICE_RECORD_TX: return IDX_VOICE_RECORD_TX;
	case VOICE_PLAYBACK_TX: return IDX_VOICE_PLAYBACK_TX;
	case VOICE2_PLAYBACK_TX: return IDX_VOICE2_PLAYBACK_TX;
	case SLIMBUS_0_RX: return IDX_SLIMBUS_0_RX;
	case SLIMBUS_0_TX: return IDX_SLIMBUS_0_TX;
	case SLIMBUS_1_RX: return IDX_SLIMBUS_1_RX;
	case SLIMBUS_1_TX: return IDX_SLIMBUS_1_TX;
	case SLIMBUS_2_RX: return IDX_SLIMBUS_2_RX;
	case SLIMBUS_2_TX: return IDX_SLIMBUS_2_TX;
	case SLIMBUS_3_RX: return IDX_SLIMBUS_3_RX;
	case SLIMBUS_3_TX: return IDX_SLIMBUS_3_TX;
	case INT_BT_SCO_RX: return IDX_INT_BT_SCO_RX;
	case INT_BT_SCO_TX: return IDX_INT_BT_SCO_TX;
	case INT_BT_A2DP_RX: return IDX_INT_BT_A2DP_RX;
	case INT_FM_RX: return IDX_INT_FM_RX;
	case INT_FM_TX: return IDX_INT_FM_TX;
	case RT_PROXY_PORT_001_RX: return IDX_RT_PROXY_PORT_001_RX;
	case RT_PROXY_PORT_001_TX: return IDX_RT_PROXY_PORT_001_TX;
	case SLIMBUS_4_RX: return IDX_SLIMBUS_4_RX;
	case SLIMBUS_4_TX: return IDX_SLIMBUS_4_TX;
	case SLIMBUS_5_RX: return IDX_SLIMBUS_5_RX;
	case SLIMBUS_5_TX: return IDX_SLIMBUS_5_TX;
	case SLIMBUS_6_RX: return IDX_SLIMBUS_6_RX;
	case SLIMBUS_6_TX: return IDX_SLIMBUS_6_TX;
	case SLIMBUS_7_RX: return IDX_SLIMBUS_7_RX;
	case SLIMBUS_7_TX: return IDX_SLIMBUS_7_TX;
	case SLIMBUS_8_RX: return IDX_SLIMBUS_8_RX;
	case SLIMBUS_8_TX: return IDX_SLIMBUS_8_TX;
	case SLIMBUS_9_RX: return IDX_SLIMBUS_9_RX;
	case SLIMBUS_9_TX: return IDX_SLIMBUS_9_TX;
	case AFE_PORT_ID_USB_RX: return IDX_AFE_PORT_ID_USB_RX;
	case AFE_PORT_ID_USB_TX: return IDX_AFE_PORT_ID_USB_TX;
	case AFE_PORT_ID_PRIMARY_MI2S_RX:
		return IDX_AFE_PORT_ID_PRIMARY_MI2S_RX;
	case AFE_PORT_ID_PRIMARY_MI2S_TX:
		return IDX_AFE_PORT_ID_PRIMARY_MI2S_TX;
	case AFE_PORT_ID_QUATERNARY_MI2S_RX:
		return IDX_AFE_PORT_ID_QUATERNARY_MI2S_RX;
	case AFE_PORT_ID_QUATERNARY_MI2S_TX:
		return IDX_AFE_PORT_ID_QUATERNARY_MI2S_TX;
	case AFE_PORT_ID_SECONDARY_MI2S_RX:
		return IDX_AFE_PORT_ID_SECONDARY_MI2S_RX;
	case AFE_PORT_ID_SECONDARY_MI2S_TX:
		return IDX_AFE_PORT_ID_SECONDARY_MI2S_TX;
	case AFE_PORT_ID_TERTIARY_MI2S_RX:
		return IDX_AFE_PORT_ID_TERTIARY_MI2S_RX;
	case AFE_PORT_ID_TERTIARY_MI2S_TX:
		return IDX_AFE_PORT_ID_TERTIARY_MI2S_TX;
	case AFE_PORT_ID_SECONDARY_MI2S_RX_SD1:
		return IDX_AFE_PORT_ID_SECONDARY_MI2S_RX_SD1;
	case AFE_PORT_ID_QUINARY_MI2S_RX:
		return IDX_AFE_PORT_ID_QUINARY_MI2S_RX;
	case AFE_PORT_ID_QUINARY_MI2S_TX:
		return IDX_AFE_PORT_ID_QUINARY_MI2S_TX;
	case AFE_PORT_ID_SENARY_MI2S_TX:
		return IDX_AFE_PORT_ID_SENARY_MI2S_TX;
	case AFE_PORT_ID_PRIMARY_TDM_RX:
		return IDX_AFE_PORT_ID_PRIMARY_TDM_RX_0;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		return IDX_AFE_PORT_ID_PRIMARY_TDM_TX_0;
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
		return IDX_AFE_PORT_ID_PRIMARY_TDM_RX_1;
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
		return IDX_AFE_PORT_ID_PRIMARY_TDM_TX_1;
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
		return IDX_AFE_PORT_ID_PRIMARY_TDM_RX_2;
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
		return IDX_AFE_PORT_ID_PRIMARY_TDM_TX_2;
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
		return IDX_AFE_PORT_ID_PRIMARY_TDM_RX_3;
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
		return IDX_AFE_PORT_ID_PRIMARY_TDM_TX_3;
	case AFE_PORT_ID_PRIMARY_TDM_RX_4:
		return IDX_AFE_PORT_ID_PRIMARY_TDM_RX_4;
	case AFE_PORT_ID_PRIMARY_TDM_TX_4:
		return IDX_AFE_PORT_ID_PRIMARY_TDM_TX_4;
	case AFE_PORT_ID_PRIMARY_TDM_RX_5:
		return IDX_AFE_PORT_ID_PRIMARY_TDM_RX_5;
	case AFE_PORT_ID_PRIMARY_TDM_TX_5:
		return IDX_AFE_PORT_ID_PRIMARY_TDM_TX_5;
	case AFE_PORT_ID_PRIMARY_TDM_RX_6:
		return IDX_AFE_PORT_ID_PRIMARY_TDM_RX_6;
	case AFE_PORT_ID_PRIMARY_TDM_TX_6:
		return IDX_AFE_PORT_ID_PRIMARY_TDM_TX_6;
	case AFE_PORT_ID_PRIMARY_TDM_RX_7:
		return IDX_AFE_PORT_ID_PRIMARY_TDM_RX_7;
	case AFE_PORT_ID_PRIMARY_TDM_TX_7:
		return IDX_AFE_PORT_ID_PRIMARY_TDM_TX_7;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
		return IDX_AFE_PORT_ID_SECONDARY_TDM_RX_0;
	case AFE_PORT_ID_SECONDARY_TDM_TX:
		return IDX_AFE_PORT_ID_SECONDARY_TDM_TX_0;
	case AFE_PORT_ID_SECONDARY_TDM_RX_1:
		return IDX_AFE_PORT_ID_SECONDARY_TDM_RX_1;
	case AFE_PORT_ID_SECONDARY_TDM_TX_1:
		return IDX_AFE_PORT_ID_SECONDARY_TDM_TX_1;
	case AFE_PORT_ID_SECONDARY_TDM_RX_2:
		return IDX_AFE_PORT_ID_SECONDARY_TDM_RX_2;
	case AFE_PORT_ID_SECONDARY_TDM_TX_2:
		return IDX_AFE_PORT_ID_SECONDARY_TDM_TX_2;
	case AFE_PORT_ID_SECONDARY_TDM_RX_3:
		return IDX_AFE_PORT_ID_SECONDARY_TDM_RX_3;
	case AFE_PORT_ID_SECONDARY_TDM_TX_3:
		return IDX_AFE_PORT_ID_SECONDARY_TDM_TX_3;
	case AFE_PORT_ID_SECONDARY_TDM_RX_4:
		return IDX_AFE_PORT_ID_SECONDARY_TDM_RX_4;
	case AFE_PORT_ID_SECONDARY_TDM_TX_4:
		return IDX_AFE_PORT_ID_SECONDARY_TDM_TX_4;
	case AFE_PORT_ID_SECONDARY_TDM_RX_5:
		return IDX_AFE_PORT_ID_SECONDARY_TDM_RX_5;
	case AFE_PORT_ID_SECONDARY_TDM_TX_5:
		return IDX_AFE_PORT_ID_SECONDARY_TDM_TX_5;
	case AFE_PORT_ID_SECONDARY_TDM_RX_6:
		return IDX_AFE_PORT_ID_SECONDARY_TDM_RX_6;
	case AFE_PORT_ID_SECONDARY_TDM_TX_6:
		return IDX_AFE_PORT_ID_SECONDARY_TDM_TX_6;
	case AFE_PORT_ID_SECONDARY_TDM_RX_7:
		return IDX_AFE_PORT_ID_SECONDARY_TDM_RX_7;
	case AFE_PORT_ID_SECONDARY_TDM_TX_7:
		return IDX_AFE_PORT_ID_SECONDARY_TDM_TX_7;
	case AFE_PORT_ID_TERTIARY_TDM_RX:
		return IDX_AFE_PORT_ID_TERTIARY_TDM_RX_0;
	case AFE_PORT_ID_TERTIARY_TDM_TX:
		return IDX_AFE_PORT_ID_TERTIARY_TDM_TX_0;
	case AFE_PORT_ID_TERTIARY_TDM_RX_1:
		return IDX_AFE_PORT_ID_TERTIARY_TDM_RX_1;
	case AFE_PORT_ID_TERTIARY_TDM_TX_1:
		return IDX_AFE_PORT_ID_TERTIARY_TDM_TX_1;
	case AFE_PORT_ID_TERTIARY_TDM_RX_2:
		return IDX_AFE_PORT_ID_TERTIARY_TDM_RX_2;
	case AFE_PORT_ID_TERTIARY_TDM_TX_2:
		return IDX_AFE_PORT_ID_TERTIARY_TDM_TX_2;
	case AFE_PORT_ID_TERTIARY_TDM_RX_3:
		return IDX_AFE_PORT_ID_TERTIARY_TDM_RX_3;
	case AFE_PORT_ID_TERTIARY_TDM_TX_3:
		return IDX_AFE_PORT_ID_TERTIARY_TDM_TX_3;
	case AFE_PORT_ID_TERTIARY_TDM_RX_4:
		return IDX_AFE_PORT_ID_TERTIARY_TDM_RX_4;
	case AFE_PORT_ID_TERTIARY_TDM_TX_4:
		return IDX_AFE_PORT_ID_TERTIARY_TDM_TX_4;
	case AFE_PORT_ID_TERTIARY_TDM_RX_5:
		return IDX_AFE_PORT_ID_TERTIARY_TDM_RX_5;
	case AFE_PORT_ID_TERTIARY_TDM_TX_5:
		return IDX_AFE_PORT_ID_TERTIARY_TDM_TX_5;
	case AFE_PORT_ID_TERTIARY_TDM_RX_6:
		return IDX_AFE_PORT_ID_TERTIARY_TDM_RX_6;
	case AFE_PORT_ID_TERTIARY_TDM_TX_6:
		return IDX_AFE_PORT_ID_TERTIARY_TDM_TX_6;
	case AFE_PORT_ID_TERTIARY_TDM_RX_7:
		return IDX_AFE_PORT_ID_TERTIARY_TDM_RX_7;
	case AFE_PORT_ID_TERTIARY_TDM_TX_7:
		return IDX_AFE_PORT_ID_TERTIARY_TDM_TX_7;
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
		return IDX_AFE_PORT_ID_QUATERNARY_TDM_RX_0;
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
		return IDX_AFE_PORT_ID_QUATERNARY_TDM_TX_0;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_1:
		return IDX_AFE_PORT_ID_QUATERNARY_TDM_RX_1;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_1:
		return IDX_AFE_PORT_ID_QUATERNARY_TDM_TX_1;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_2:
		return IDX_AFE_PORT_ID_QUATERNARY_TDM_RX_2;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_2:
		return IDX_AFE_PORT_ID_QUATERNARY_TDM_TX_2;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_3:
		return IDX_AFE_PORT_ID_QUATERNARY_TDM_RX_3;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_3:
		return IDX_AFE_PORT_ID_QUATERNARY_TDM_TX_3;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_4:
		return IDX_AFE_PORT_ID_QUATERNARY_TDM_RX_4;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_4:
		return IDX_AFE_PORT_ID_QUATERNARY_TDM_TX_4;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_5:
		return IDX_AFE_PORT_ID_QUATERNARY_TDM_RX_5;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_5:
		return IDX_AFE_PORT_ID_QUATERNARY_TDM_TX_5;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_6:
		return IDX_AFE_PORT_ID_QUATERNARY_TDM_RX_6;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_6:
		return IDX_AFE_PORT_ID_QUATERNARY_TDM_TX_6;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_7:
		return IDX_AFE_PORT_ID_QUATERNARY_TDM_RX_7;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_7:
		return IDX_AFE_PORT_ID_QUATERNARY_TDM_TX_7;
	case AFE_PORT_ID_QUINARY_TDM_RX:
		return IDX_AFE_PORT_ID_QUINARY_TDM_RX_0;
	case AFE_PORT_ID_QUINARY_TDM_TX:
		return IDX_AFE_PORT_ID_QUINARY_TDM_TX_0;
	case AFE_PORT_ID_QUINARY_TDM_RX_1:
		return IDX_AFE_PORT_ID_QUINARY_TDM_RX_1;
	case AFE_PORT_ID_QUINARY_TDM_TX_1:
		return IDX_AFE_PORT_ID_QUINARY_TDM_TX_1;
	case AFE_PORT_ID_QUINARY_TDM_RX_2:
		return IDX_AFE_PORT_ID_QUINARY_TDM_RX_2;
	case AFE_PORT_ID_QUINARY_TDM_TX_2:
		return IDX_AFE_PORT_ID_QUINARY_TDM_TX_2;
	case AFE_PORT_ID_QUINARY_TDM_RX_3:
		return IDX_AFE_PORT_ID_QUINARY_TDM_RX_3;
	case AFE_PORT_ID_QUINARY_TDM_TX_3:
		return IDX_AFE_PORT_ID_QUINARY_TDM_TX_3;
	case AFE_PORT_ID_QUINARY_TDM_RX_4:
		return IDX_AFE_PORT_ID_QUINARY_TDM_RX_4;
	case AFE_PORT_ID_QUINARY_TDM_TX_4:
		return IDX_AFE_PORT_ID_QUINARY_TDM_TX_4;
	case AFE_PORT_ID_QUINARY_TDM_RX_5:
		return IDX_AFE_PORT_ID_QUINARY_TDM_RX_5;
	case AFE_PORT_ID_QUINARY_TDM_TX_5:
		return IDX_AFE_PORT_ID_QUINARY_TDM_TX_5;
	case AFE_PORT_ID_QUINARY_TDM_RX_6:
		return IDX_AFE_PORT_ID_QUINARY_TDM_RX_6;
	case AFE_PORT_ID_QUINARY_TDM_TX_6:
		return IDX_AFE_PORT_ID_QUINARY_TDM_TX_6;
	case AFE_PORT_ID_QUINARY_TDM_RX_7:
		return IDX_AFE_PORT_ID_QUINARY_TDM_RX_7;
	case AFE_PORT_ID_QUINARY_TDM_TX_7:
		return IDX_AFE_PORT_ID_QUINARY_TDM_TX_7;
	case AFE_PORT_ID_INT0_MI2S_RX:
		return IDX_AFE_PORT_ID_INT0_MI2S_RX;
	case AFE_PORT_ID_INT0_MI2S_TX:
		return IDX_AFE_PORT_ID_INT0_MI2S_TX;
	case AFE_PORT_ID_INT1_MI2S_RX:
		return IDX_AFE_PORT_ID_INT1_MI2S_RX;
	case AFE_PORT_ID_INT1_MI2S_TX:
		return IDX_AFE_PORT_ID_INT1_MI2S_TX;
	case AFE_PORT_ID_INT2_MI2S_RX:
		return IDX_AFE_PORT_ID_INT2_MI2S_RX;
	case AFE_PORT_ID_INT2_MI2S_TX:
		return IDX_AFE_PORT_ID_INT2_MI2S_TX;
	case AFE_PORT_ID_INT3_MI2S_RX:
		return IDX_AFE_PORT_ID_INT3_MI2S_RX;
	case AFE_PORT_ID_INT3_MI2S_TX:
		return IDX_AFE_PORT_ID_INT3_MI2S_TX;
	case AFE_PORT_ID_INT4_MI2S_RX:
		return IDX_AFE_PORT_ID_INT4_MI2S_RX;
	case AFE_PORT_ID_INT4_MI2S_TX:
		return IDX_AFE_PORT_ID_INT4_MI2S_TX;
	case AFE_PORT_ID_INT5_MI2S_RX:
		return IDX_AFE_PORT_ID_INT5_MI2S_RX;
	case AFE_PORT_ID_INT5_MI2S_TX:
		return IDX_AFE_PORT_ID_INT5_MI2S_TX;
	case AFE_PORT_ID_INT6_MI2S_RX:
		return IDX_AFE_PORT_ID_INT6_MI2S_RX;
	case AFE_PORT_ID_INT6_MI2S_TX:
		return IDX_AFE_PORT_ID_INT6_MI2S_TX;
	case AFE_PORT_ID_VA_CODEC_DMA_TX_0:
		return IDX_AFE_PORT_ID_VA_CODEC_DMA_TX_0;
	case AFE_PORT_ID_VA_CODEC_DMA_TX_1:
		return IDX_AFE_PORT_ID_VA_CODEC_DMA_TX_1;
	case AFE_PORT_ID_WSA_CODEC_DMA_RX_0:
		return IDX_AFE_PORT_ID_WSA_CODEC_DMA_RX_0;
	case AFE_PORT_ID_WSA_CODEC_DMA_TX_0:
		return IDX_AFE_PORT_ID_WSA_CODEC_DMA_TX_0;
	case AFE_PORT_ID_WSA_CODEC_DMA_RX_1:
		return IDX_AFE_PORT_ID_WSA_CODEC_DMA_RX_1;
	case AFE_PORT_ID_WSA_CODEC_DMA_TX_1:
		return IDX_AFE_PORT_ID_WSA_CODEC_DMA_TX_1;
	case AFE_PORT_ID_WSA_CODEC_DMA_TX_2:
		return IDX_AFE_PORT_ID_WSA_CODEC_DMA_TX_2;
	case AFE_PORT_ID_RX_CODEC_DMA_RX_0:
		return IDX_AFE_PORT_ID_RX_CODEC_DMA_RX_0;
	case AFE_PORT_ID_TX_CODEC_DMA_TX_0:
		return IDX_AFE_PORT_ID_TX_CODEC_DMA_TX_0;
	case AFE_PORT_ID_RX_CODEC_DMA_RX_1:
		return IDX_AFE_PORT_ID_RX_CODEC_DMA_RX_1;
	case AFE_PORT_ID_TX_CODEC_DMA_TX_1:
		return IDX_AFE_PORT_ID_TX_CODEC_DMA_TX_1;
	case AFE_PORT_ID_RX_CODEC_DMA_RX_2:
		return IDX_AFE_PORT_ID_RX_CODEC_DMA_RX_2;
	case AFE_PORT_ID_TX_CODEC_DMA_TX_2:
		return IDX_AFE_PORT_ID_TX_CODEC_DMA_TX_2;
	case AFE_PORT_ID_RX_CODEC_DMA_RX_3:
		return IDX_AFE_PORT_ID_RX_CODEC_DMA_RX_3;
	case AFE_PORT_ID_TX_CODEC_DMA_TX_3:
		return IDX_AFE_PORT_ID_TX_CODEC_DMA_TX_3;
	case AFE_PORT_ID_RX_CODEC_DMA_RX_4:
		return IDX_AFE_PORT_ID_RX_CODEC_DMA_RX_4;
	case AFE_PORT_ID_TX_CODEC_DMA_TX_4:
		return IDX_AFE_PORT_ID_TX_CODEC_DMA_TX_4;
	case AFE_PORT_ID_RX_CODEC_DMA_RX_5:
		return IDX_AFE_PORT_ID_RX_CODEC_DMA_RX_5;
	case AFE_PORT_ID_TX_CODEC_DMA_TX_5:
		return IDX_AFE_PORT_ID_TX_CODEC_DMA_TX_5;
	case AFE_PORT_ID_RX_CODEC_DMA_RX_6:
		return IDX_AFE_PORT_ID_RX_CODEC_DMA_RX_6;
	case AFE_PORT_ID_RX_CODEC_DMA_RX_7:
		return IDX_AFE_PORT_ID_RX_CODEC_DMA_RX_7;
	default:
		pr_err("%s: port 0x%x\n", __func__, port_id);
		return -EINVAL;
	}
}

/**
 * afe_open -
 *         command to open AFE port
 *
 * @port_id: AFE port id
 * @afe_config: AFE port config to pass
 * @rate: sample rate
 *
 * Returns 0 on success or error on failure
 */
int afe_open(u16 port_id,
		union afe_port_config *afe_config, int rate)
{
	struct afe_port_cmd_device_start start;
	union afe_port_config port_cfg;
	struct param_hdr_v3 param_hdr;
	int ret = 0;
	int cfg_type;
	int index = 0;

	memset(&param_hdr, 0, sizeof(param_hdr));
	memset(&start, 0, sizeof(start));
	memset(&port_cfg, 0, sizeof(port_cfg));

	if (!afe_config) {
		pr_err("%s: Error, no configuration data\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	pr_err("%s: port_id 0x%x rate %d\n", __func__, port_id, rate);

	index = q6audio_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		return -EINVAL;
	}
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: Invalid port 0x%x ret %d", __func__, port_id, ret);
		return -EINVAL;
	}

	if ((port_id == RT_PROXY_DAI_001_RX) ||
		(port_id == RT_PROXY_DAI_002_TX)) {
		pr_err("%s: wrong port 0x%x\n", __func__, port_id);
		return -EINVAL;
	}
	if ((port_id == RT_PROXY_DAI_002_RX) ||
		(port_id == RT_PROXY_DAI_001_TX))
		port_id = VIRTUAL_ID_TO_PORTID(port_id);

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return -EINVAL;
	}

	if ((index >= 0) && (index < AFE_MAX_PORTS)) {
		this_afe.afe_sample_rates[index] = rate;

		if (this_afe.rt_cb)
			this_afe.dev_acdb_id[index] = this_afe.rt_cb(port_id);
	}

	/* Also send the topology id here: */
	afe_send_custom_topology(); /* One time call: only for first time  */
	afe_send_port_topology_id(port_id);

	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: Failed : Invalid Port id = 0x%x ret %d\n",
			__func__, port_id, ret);
		return -EINVAL;
	}
	mutex_lock(&this_afe.afe_cmd_lock);

	switch (port_id) {
	case PRIMARY_I2S_RX:
	case PRIMARY_I2S_TX:
		cfg_type = AFE_PARAM_ID_I2S_CONFIG;
		break;
	case AFE_PORT_ID_PRIMARY_PCM_RX:
	case AFE_PORT_ID_PRIMARY_PCM_TX:
	case AFE_PORT_ID_SECONDARY_PCM_RX:
	case AFE_PORT_ID_SECONDARY_PCM_TX:
	case AFE_PORT_ID_TERTIARY_PCM_RX:
	case AFE_PORT_ID_TERTIARY_PCM_TX:
	case AFE_PORT_ID_QUATERNARY_PCM_RX:
	case AFE_PORT_ID_QUATERNARY_PCM_TX:
	case AFE_PORT_ID_QUINARY_PCM_RX:
	case AFE_PORT_ID_QUINARY_PCM_TX:
	case AFE_PORT_ID_SENARY_PCM_RX:
	case AFE_PORT_ID_SENARY_PCM_TX:
		cfg_type = AFE_PARAM_ID_PCM_CONFIG;
		break;
	case SECONDARY_I2S_RX:
	case SECONDARY_I2S_TX:
	case AFE_PORT_ID_PRIMARY_MI2S_RX:
	case AFE_PORT_ID_PRIMARY_MI2S_TX:
	case AFE_PORT_ID_QUATERNARY_MI2S_RX:
	case AFE_PORT_ID_QUATERNARY_MI2S_TX:
	case MI2S_RX:
	case MI2S_TX:
	case AFE_PORT_ID_QUINARY_MI2S_RX:
	case AFE_PORT_ID_QUINARY_MI2S_TX:
	case AFE_PORT_ID_SENARY_MI2S_TX:
		cfg_type = AFE_PARAM_ID_I2S_CONFIG;
		break;
	case HDMI_RX:
	case DISPLAY_PORT_RX:
		cfg_type = AFE_PARAM_ID_HDMI_CONFIG;
		break;
	case AFE_PORT_ID_PRIMARY_SPDIF_RX:
	case AFE_PORT_ID_PRIMARY_SPDIF_TX:
	case AFE_PORT_ID_SECONDARY_SPDIF_RX:
	case AFE_PORT_ID_SECONDARY_SPDIF_TX:
		cfg_type = AFE_PARAM_ID_SPDIF_CONFIG;
		break;
	case SLIMBUS_0_RX:
	case SLIMBUS_0_TX:
	case SLIMBUS_1_RX:
	case SLIMBUS_1_TX:
	case SLIMBUS_2_RX:
	case SLIMBUS_2_TX:
	case SLIMBUS_3_RX:
	case SLIMBUS_3_TX:
	case SLIMBUS_4_RX:
	case SLIMBUS_4_TX:
	case SLIMBUS_5_RX:
	case SLIMBUS_6_RX:
	case SLIMBUS_6_TX:
	case SLIMBUS_7_RX:
	case SLIMBUS_7_TX:
	case SLIMBUS_8_RX:
	case SLIMBUS_8_TX:
	case SLIMBUS_9_RX:
	case SLIMBUS_9_TX:
		cfg_type = AFE_PARAM_ID_SLIMBUS_CONFIG;
		break;
	case AFE_PORT_ID_USB_RX:
	case AFE_PORT_ID_USB_TX:
		cfg_type = AFE_PARAM_ID_USB_AUDIO_CONFIG;
		break;
	case AFE_PORT_ID_WSA_CODEC_DMA_RX_0:
	case AFE_PORT_ID_WSA_CODEC_DMA_TX_0:
	case AFE_PORT_ID_WSA_CODEC_DMA_RX_1:
	case AFE_PORT_ID_WSA_CODEC_DMA_TX_1:
	case AFE_PORT_ID_WSA_CODEC_DMA_TX_2:
	case AFE_PORT_ID_VA_CODEC_DMA_TX_0:
	case AFE_PORT_ID_VA_CODEC_DMA_TX_1:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_0:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_0:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_1:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_1:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_2:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_2:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_3:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_3:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_4:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_4:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_5:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_5:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_6:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_7:
		cfg_type = AFE_PARAM_ID_CODEC_DMA_CONFIG;
		break;
	default:
		pr_err("%s: Invalid port id 0x%x\n",
			__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	param_hdr.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = cfg_type;
	param_hdr.param_size = sizeof(union afe_port_config);
	port_cfg = *afe_config;

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) &port_cfg);
	if (ret) {
		pr_err("%s: AFE enable for port 0x%x opcode[0x%x]failed %d\n",
			__func__, port_id, cfg_type, ret);
		goto fail_cmd;
	}
	start.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	start.hdr.pkt_size = sizeof(start);
	start.hdr.src_port = 0;
	start.hdr.dest_port = 0;
	start.hdr.token = index;
	start.hdr.opcode = AFE_PORT_CMD_DEVICE_START;
	start.port_id = q6audio_get_port_id(port_id);
	pr_debug("%s: cmd device start opcode[0x%x] port id[0x%x]\n",
		__func__, start.hdr.opcode, start.port_id);

	ret = afe_apr_send_pkt(&start, &this_afe.wait[index]);
	if (ret) {
		pr_err("%s: AFE enable for port 0x%x failed %d\n", __func__,
				port_id, ret);
		goto fail_cmd;
	}

fail_cmd:
	mutex_unlock(&this_afe.afe_cmd_lock);
	return ret;
}
EXPORT_SYMBOL(afe_open);

/**
 * afe_loopback -
 *         command to set loopback between AFE ports
 *
 * @enable: enable or disable loopback
 * @rx_port: AFE RX port ID
 * @tx_port: AFE TX port ID
 *
 * Returns 0 on success or error on failure
 */
int afe_loopback(u16 enable, u16 rx_port, u16 tx_port)
{
	struct afe_loopback_cfg_v1 lb_param;
	struct param_hdr_v3 param_hdr;
	int ret = 0;

	memset(&lb_param, 0, sizeof(lb_param));
	memset(&param_hdr, 0, sizeof(param_hdr));

	if (rx_port == MI2S_RX)
		rx_port = AFE_PORT_ID_PRIMARY_MI2S_RX;
	if (tx_port == MI2S_TX)
		tx_port = AFE_PORT_ID_PRIMARY_MI2S_TX;

	param_hdr.module_id = AFE_MODULE_LOOPBACK;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_LOOPBACK_CONFIG;
	param_hdr.param_size = sizeof(struct afe_loopback_cfg_v1);

	lb_param.dst_port_id = rx_port;
	lb_param.routing_mode = LB_MODE_DEFAULT;
	lb_param.enable = (enable ? 1 : 0);
	lb_param.loopback_cfg_minor_version = AFE_API_VERSION_LOOPBACK_CONFIG;

	ret = q6afe_pack_and_set_param_in_band(tx_port,
					       q6audio_get_port_index(tx_port),
					       param_hdr, (u8 *) &lb_param);
	if (ret)
		pr_err("%s: AFE loopback failed %d\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(afe_loopback);

/**
 * afe_loopback_gain -
 *         command to set gain for AFE loopback
 *
 * @port_id: AFE port id
 * @volume: gain value to set
 *
 * Returns 0 on success or error on failure
 */
int afe_loopback_gain(u16 port_id, u16 volume)
{
	struct afe_loopback_gain_per_path_param set_param;
	struct param_hdr_v3 param_hdr;
	int ret = 0;

	memset(&set_param, 0, sizeof(set_param));
	memset(&param_hdr, 0, sizeof(param_hdr));

	if (this_afe.apr == NULL) {
		this_afe.apr = apr_register("ADSP", "AFE", afe_callback,
					0xFFFFFFFF, &this_afe);
		pr_debug("%s: Register AFE\n", __func__);
		if (this_afe.apr == NULL) {
			pr_err("%s: Unable to register AFE\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_afe_handle(this_afe.apr);
	}

	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: Failed : Invalid Port id = 0x%x ret %d\n",
			__func__, port_id, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	/* RX ports numbers are even .TX ports numbers are odd. */
	if (port_id % 2 == 0) {
		pr_err("%s: Failed : afe loopback gain only for TX ports. port_id %d\n",
				__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	pr_debug("%s: port 0x%x volume %d\n", __func__, port_id, volume);

	param_hdr.module_id = AFE_MODULE_LOOPBACK;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_LOOPBACK_GAIN_PER_PATH;
	param_hdr.param_size = sizeof(struct afe_loopback_gain_per_path_param);
	set_param.rx_port_id = port_id;
	set_param.gain = volume;

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) &set_param);
	if (ret)
		pr_err("%s: AFE param set failed for port 0x%x ret %d\n",
					__func__, port_id, ret);

fail_cmd:
	return ret;
}
EXPORT_SYMBOL(afe_loopback_gain);

int afe_pseudo_port_start_nowait(u16 port_id)
{
	struct afe_pseudoport_start_command start;
	int ret = 0;

	pr_debug("%s: port_id=0x%x\n", __func__, port_id);
	if (this_afe.apr == NULL) {
		pr_err("%s: AFE APR is not registered\n", __func__);
		return -ENODEV;
	}


	start.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	start.hdr.pkt_size = sizeof(start);
	start.hdr.src_port = 0;
	start.hdr.dest_port = 0;
	start.hdr.token = 0;
	start.hdr.opcode = AFE_PSEUDOPORT_CMD_START;
	start.port_id = port_id;
	start.timing = 1;

	ret = afe_apr_send_pkt(&start, NULL);
	if (ret) {
		pr_err("%s: AFE enable for port 0x%x failed %d\n",
		       __func__, port_id, ret);
		return ret;
	}
	return 0;
}

int afe_start_pseudo_port(u16 port_id)
{
	int ret = 0;
	struct afe_pseudoport_start_command start;
	int index = 0;

	pr_debug("%s: port_id = 0x%x\n", __func__, port_id);

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	index = q6audio_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		return -EINVAL;
	}
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: Invalid port 0x%x ret %d",
			__func__, port_id, ret);
		return -EINVAL;
	}

	start.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	start.hdr.pkt_size = sizeof(start);
	start.hdr.src_port = 0;
	start.hdr.dest_port = 0;
	start.hdr.token = 0;
	start.hdr.opcode = AFE_PSEUDOPORT_CMD_START;
	start.port_id = port_id;
	start.timing = 1;
	start.hdr.token = index;

	ret = afe_apr_send_pkt(&start, &this_afe.wait[index]);
	if (ret)
		pr_err("%s: AFE enable for port 0x%x failed %d\n",
		       __func__, port_id, ret);
	return ret;
}

int afe_pseudo_port_stop_nowait(u16 port_id)
{
	int ret = 0;
	struct afe_pseudoport_stop_command stop;
	int index = 0;

	pr_debug("%s: port_id = 0x%x\n", __func__, port_id);

	if (this_afe.apr == NULL) {
		pr_err("%s: AFE is already closed\n", __func__);
		return -EINVAL;
	}
	index = q6audio_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		return -EINVAL;
	}
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: Invalid port 0x%x ret %d",
			__func__, port_id, ret);
		return -EINVAL;
	}

	stop.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	stop.hdr.pkt_size = sizeof(stop);
	stop.hdr.src_port = 0;
	stop.hdr.dest_port = 0;
	stop.hdr.token = 0;
	stop.hdr.opcode = AFE_PSEUDOPORT_CMD_STOP;
	stop.port_id = port_id;
	stop.reserved = 0;
	stop.hdr.token = index;

	ret = afe_apr_send_pkt(&stop, NULL);
	if (ret)
		pr_err("%s: AFE close failed %d\n", __func__, ret);

	return ret;
}

int afe_port_group_set_param(u16 group_id,
	union afe_port_group_config *afe_group_config)
{
	struct param_hdr_v3 param_hdr;
	int cfg_type;
	int ret;

	if (!afe_group_config) {
		pr_err("%s: Error, no configuration data\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: group id: 0x%x\n", __func__, group_id);

	memset(&param_hdr, 0, sizeof(param_hdr));

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	switch (group_id) {
	case AFE_GROUP_DEVICE_ID_PRIMARY_TDM_RX:
	case AFE_GROUP_DEVICE_ID_PRIMARY_TDM_TX:
	case AFE_GROUP_DEVICE_ID_SECONDARY_TDM_RX:
	case AFE_GROUP_DEVICE_ID_SECONDARY_TDM_TX:
	case AFE_GROUP_DEVICE_ID_TERTIARY_TDM_RX:
	case AFE_GROUP_DEVICE_ID_TERTIARY_TDM_TX:
	case AFE_GROUP_DEVICE_ID_QUATERNARY_TDM_RX:
	case AFE_GROUP_DEVICE_ID_QUATERNARY_TDM_TX:
	case AFE_GROUP_DEVICE_ID_QUINARY_TDM_RX:
	case AFE_GROUP_DEVICE_ID_QUINARY_TDM_TX:
		cfg_type = AFE_PARAM_ID_GROUP_DEVICE_TDM_CONFIG;
		break;
	default:
		pr_err("%s: Invalid group id 0x%x\n", __func__, group_id);
		return -EINVAL;
	}

	param_hdr.module_id = AFE_MODULE_GROUP_DEVICE;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = cfg_type;
	param_hdr.param_size = sizeof(union afe_port_group_config);

	ret = q6afe_svc_pack_and_set_param_in_band(IDX_GLOBAL_CFG, param_hdr,
						   (u8 *) afe_group_config);
	if (ret)
		pr_err("%s: AFE_PARAM_ID_GROUP_DEVICE_CFG failed %d\n",
			__func__, ret);

	return ret;
}

/**
 * afe_port_group_enable -
 *         command to enable AFE port group
 *
 * @group_id: group ID for AFE port group
 * @afe_group_config: config for AFE group
 * @enable: flag to indicate enable or disable
 *
 * Returns 0 on success or error on failure
 */
int afe_port_group_enable(u16 group_id,
	union afe_port_group_config *afe_group_config,
	u16 enable)
{
	struct afe_group_device_enable group_enable;
	struct param_hdr_v3 param_hdr;
	int ret;

	pr_debug("%s: group id: 0x%x enable: %d\n", __func__,
		group_id, enable);

	memset(&group_enable, 0, sizeof(group_enable));
	memset(&param_hdr, 0, sizeof(param_hdr));

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	if (enable) {
		ret = afe_port_group_set_param(group_id, afe_group_config);
		if (ret < 0) {
			pr_err("%s: afe send failed %d\n", __func__, ret);
			return ret;
		}
	}

	param_hdr.module_id = AFE_MODULE_GROUP_DEVICE;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_GROUP_DEVICE_ENABLE;
	param_hdr.param_size = sizeof(struct afe_group_device_enable);
	group_enable.group_id = group_id;
	group_enable.enable = enable;

	ret = q6afe_svc_pack_and_set_param_in_band(IDX_GLOBAL_CFG, param_hdr,
						   (u8 *) &group_enable);
	if (ret)
		pr_err("%s: AFE_PARAM_ID_GROUP_DEVICE_ENABLE failed %d\n",
			__func__, ret);

	return ret;
}
EXPORT_SYMBOL(afe_port_group_enable);

int afe_stop_pseudo_port(u16 port_id)
{
	int ret = 0;
	struct afe_pseudoport_stop_command stop;
	int index = 0;

	pr_debug("%s: port_id = 0x%x\n", __func__, port_id);

	if (this_afe.apr == NULL) {
		pr_err("%s: AFE is already closed\n", __func__);
		return -EINVAL;
	}

	index = q6audio_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		return -EINVAL;
	}
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: Invalid port 0x%x ret %d\n",
			__func__, port_id, ret);
		return -EINVAL;
	}

	stop.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	stop.hdr.pkt_size = sizeof(stop);
	stop.hdr.src_port = 0;
	stop.hdr.dest_port = 0;
	stop.hdr.token = 0;
	stop.hdr.opcode = AFE_PSEUDOPORT_CMD_STOP;
	stop.port_id = port_id;
	stop.reserved = 0;
	stop.hdr.token = index;

	ret = afe_apr_send_pkt(&stop, &this_afe.wait[index]);
	if (ret)
		pr_err("%s: AFE close failed %d\n", __func__, ret);

	return ret;
}

/**
 * afe_req_mmap_handle -
 *         Retrieve AFE memory map handle
 *
 * @ac: AFE audio client
 *
 * Returns memory map handle
 */
uint32_t afe_req_mmap_handle(struct afe_audio_client *ac)
{
	return ac->mem_map_handle;
}
EXPORT_SYMBOL(afe_req_mmap_handle);

/**
 * q6afe_audio_client_alloc -
 *         Assign new AFE audio client
 *
 * @priv: privata data to hold for audio client
 *
 * Returns ac pointer on success or NULL on failure
 */
struct afe_audio_client *q6afe_audio_client_alloc(void *priv)
{
	struct afe_audio_client *ac;
	int lcnt = 0;

	ac = kzalloc(sizeof(struct afe_audio_client), GFP_KERNEL);
	if (!ac)
		return NULL;

	ac->priv = priv;

	init_waitqueue_head(&ac->cmd_wait);
	INIT_LIST_HEAD(&ac->port[0].mem_map_handle);
	INIT_LIST_HEAD(&ac->port[1].mem_map_handle);
	pr_debug("%s: mem_map_handle list init'ed\n", __func__);
	mutex_init(&ac->cmd_lock);
	for (lcnt = 0; lcnt <= OUT; lcnt++) {
		mutex_init(&ac->port[lcnt].lock);
		spin_lock_init(&ac->port[lcnt].dsp_lock);
	}
	atomic_set(&ac->cmd_state, 0);

	return ac;
}
EXPORT_SYMBOL(q6afe_audio_client_alloc);

/**
 * q6afe_audio_client_buf_alloc_contiguous -
 *         Allocate contiguous shared buffers
 *
 * @dir: RX or TX direction of AFE port
 * @ac: AFE audio client handle
 * @bufsz: size of each shared buffer
 * @bufcnt: number of buffers
 *
 * Returns 0 on success or error on failure
 */
int q6afe_audio_client_buf_alloc_contiguous(unsigned int dir,
			struct afe_audio_client *ac,
			unsigned int bufsz,
			unsigned int bufcnt)
{
	int cnt = 0;
	int rc = 0;
	struct afe_audio_buffer *buf;
	size_t len;

	if (!(ac) || ((dir != IN) && (dir != OUT))) {
		pr_err("%s: ac %pK dir %d\n", __func__, ac, dir);
		return -EINVAL;
	}

	pr_debug("%s: bufsz[%d]bufcnt[%d]\n",
			__func__,
			bufsz, bufcnt);

	if (ac->port[dir].buf) {
		pr_debug("%s: buffer already allocated\n", __func__);
		return 0;
	}
	mutex_lock(&ac->cmd_lock);
	buf = kzalloc(((sizeof(struct afe_audio_buffer))*bufcnt),
			GFP_KERNEL);

	if (!buf) {
		pr_err("%s: null buf\n", __func__);
		mutex_unlock(&ac->cmd_lock);
		goto fail;
	}

	ac->port[dir].buf = buf;

	rc = msm_audio_ion_alloc(&buf[0].dma_buf,
				bufsz * bufcnt,
				&buf[0].phys, &len,
				&buf[0].data);
	if (rc) {
		pr_err("%s: audio ION alloc failed, rc = %d\n",
			__func__, rc);
		mutex_unlock(&ac->cmd_lock);
		goto fail;
	}

	buf[0].used = dir ^ 1;
	buf[0].size = bufsz;
	buf[0].actual_size = bufsz;
	cnt = 1;
	while (cnt < bufcnt) {
		if (bufsz > 0) {
			buf[cnt].data =  buf[0].data + (cnt * bufsz);
			buf[cnt].phys =  buf[0].phys + (cnt * bufsz);
			if (!buf[cnt].data) {
				pr_err("%s: Buf alloc failed\n",
							__func__);
				mutex_unlock(&ac->cmd_lock);
				goto fail;
			}
			buf[cnt].used = dir ^ 1;
			buf[cnt].size = bufsz;
			buf[cnt].actual_size = bufsz;
			pr_debug("%s:  data[%pK]phys[%pK][%pK]\n", __func__,
				   buf[cnt].data,
				   &buf[cnt].phys,
				   &buf[cnt].phys);
		}
		cnt++;
	}
	ac->port[dir].max_buf_cnt = cnt;
	mutex_unlock(&ac->cmd_lock);
	return 0;
fail:
	pr_err("%s: jump fail\n", __func__);
	q6afe_audio_client_buf_free_contiguous(dir, ac);
	return -EINVAL;
}
EXPORT_SYMBOL(q6afe_audio_client_buf_alloc_contiguous);

/**
 * afe_memory_map -
 *         command to map shared buffers to AFE
 *
 * @dma_addr_p: DMA physical address
 * @dma_buf_sz: shared DMA buffer size
 * @ac: AFE audio client handle
 *
 * Returns 0 on success or error on failure
 */
int afe_memory_map(phys_addr_t dma_addr_p, u32 dma_buf_sz,
			struct afe_audio_client *ac)
{
	int ret = 0;

	mutex_lock(&this_afe.afe_cmd_lock);
	ac->mem_map_handle = 0;
	ret = afe_cmd_memory_map(dma_addr_p, dma_buf_sz);
	if (ret < 0) {
		pr_err("%s: afe_cmd_memory_map failed %d\n",
			__func__, ret);

		mutex_unlock(&this_afe.afe_cmd_lock);
		return ret;
	}
	ac->mem_map_handle = this_afe.mmap_handle;
	mutex_unlock(&this_afe.afe_cmd_lock);

	return ret;
}
EXPORT_SYMBOL(afe_memory_map);

int afe_cmd_memory_map(phys_addr_t dma_addr_p, u32 dma_buf_sz)
{
	int ret = 0;
	int cmd_size = 0;
	void    *payload = NULL;
	void    *mmap_region_cmd = NULL;
	struct afe_service_cmd_shared_mem_map_regions *mregion = NULL;
	struct  afe_service_shared_map_region_payload *mregion_pl = NULL;
	int index = 0;

	pr_debug("%s:\n", __func__);

	if (this_afe.apr == NULL) {
		this_afe.apr = apr_register("ADSP", "AFE", afe_callback,
					0xFFFFFFFF, &this_afe);
		pr_debug("%s: Register AFE\n", __func__);
		if (this_afe.apr == NULL) {
			pr_err("%s: Unable to register AFE\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_afe_handle(this_afe.apr);
	}
	if (dma_buf_sz % SZ_4K != 0) {
		/*
		 * The memory allocated by msm_audio_ion_alloc is always 4kB
		 * aligned, ADSP expects the size to be 4kB aligned as well
		 * so re-adjusts the  buffer size before passing to ADSP.
		 */
		dma_buf_sz = PAGE_ALIGN(dma_buf_sz);
	}

	cmd_size = sizeof(struct afe_service_cmd_shared_mem_map_regions)
		+ sizeof(struct afe_service_shared_map_region_payload);

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (!mmap_region_cmd)
		return -ENOMEM;

	mregion = (struct afe_service_cmd_shared_mem_map_regions *)
							mmap_region_cmd;
	mregion->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	mregion->hdr.pkt_size = cmd_size;
	mregion->hdr.src_port = 0;
	mregion->hdr.dest_port = 0;
	mregion->hdr.token = 0;
	mregion->hdr.opcode = AFE_SERVICE_CMD_SHARED_MEM_MAP_REGIONS;
	mregion->mem_pool_id = ADSP_MEMORY_MAP_SHMEM8_4K_POOL;
	mregion->num_regions = 1;
	mregion->property_flag = 0x00;
	/* Todo */
	index = mregion->hdr.token = IDX_RSVD_2;

	payload = ((u8 *) mmap_region_cmd +
		   sizeof(struct afe_service_cmd_shared_mem_map_regions));

	mregion_pl = (struct afe_service_shared_map_region_payload *)payload;

	mregion_pl->shm_addr_lsw = lower_32_bits(dma_addr_p);
	mregion_pl->shm_addr_msw = msm_audio_populate_upper_32_bits(dma_addr_p);
	mregion_pl->mem_size_bytes = dma_buf_sz;

	pr_debug("%s: dma_addr_p 0x%pK , size %d\n", __func__,
					&dma_addr_p, dma_buf_sz);
	atomic_set(&this_afe.state, 1);
	atomic_set(&this_afe.status, 0);
	this_afe.mmap_handle = 0;
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) mmap_region_cmd);
	if (ret < 0) {
		pr_err("%s: AFE memory map cmd failed %d\n",
		       __func__, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_afe.wait[index],
				 (atomic_read(&this_afe.state) == 0),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	if (atomic_read(&this_afe.status) > 0) {
		pr_err("%s: config cmd failed [%s]\n",
			__func__, adsp_err_get_err_str(
			atomic_read(&this_afe.status)));
		ret = adsp_err_get_lnx_err_code(
				atomic_read(&this_afe.status));
		goto fail_cmd;
	}

	kfree(mmap_region_cmd);
	return 0;
fail_cmd:
	kfree(mmap_region_cmd);
	pr_err("%s: fail_cmd\n", __func__);
	return ret;
}

int afe_cmd_memory_map_nowait(int port_id, phys_addr_t dma_addr_p,
		u32 dma_buf_sz)
{
	int ret = 0;
	int cmd_size = 0;
	void    *payload = NULL;
	void    *mmap_region_cmd = NULL;
	struct afe_service_cmd_shared_mem_map_regions *mregion = NULL;
	struct  afe_service_shared_map_region_payload *mregion_pl = NULL;
	int index = 0;

	pr_debug("%s:\n", __func__);

	if (this_afe.apr == NULL) {
		this_afe.apr = apr_register("ADSP", "AFE", afe_callback,
					0xFFFFFFFF, &this_afe);
		pr_debug("%s: Register AFE\n", __func__);
		if (this_afe.apr == NULL) {
			pr_err("%s: Unable to register AFE\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_afe_handle(this_afe.apr);
	}
	index = q6audio_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		return -EINVAL;
	}
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: Invalid port 0x%x ret %d",
			__func__, port_id, ret);
		return -EINVAL;
	}

	cmd_size = sizeof(struct afe_service_cmd_shared_mem_map_regions)
		+ sizeof(struct afe_service_shared_map_region_payload);

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (!mmap_region_cmd)
		return -ENOMEM;

	mregion = (struct afe_service_cmd_shared_mem_map_regions *)
						mmap_region_cmd;
	mregion->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	mregion->hdr.pkt_size = sizeof(mregion);
	mregion->hdr.src_port = 0;
	mregion->hdr.dest_port = 0;
	mregion->hdr.token = 0;
	mregion->hdr.opcode = AFE_SERVICE_CMD_SHARED_MEM_MAP_REGIONS;
	mregion->mem_pool_id = ADSP_MEMORY_MAP_SHMEM8_4K_POOL;
	mregion->num_regions = 1;
	mregion->property_flag = 0x00;

	payload = ((u8 *) mmap_region_cmd +
		sizeof(struct afe_service_cmd_shared_mem_map_regions));
	mregion_pl = (struct afe_service_shared_map_region_payload *)payload;

	mregion_pl->shm_addr_lsw = lower_32_bits(dma_addr_p);
	mregion_pl->shm_addr_msw = msm_audio_populate_upper_32_bits(dma_addr_p);
	mregion_pl->mem_size_bytes = dma_buf_sz;

	ret = afe_apr_send_pkt(mmap_region_cmd, NULL);
	if (ret)
		pr_err("%s: AFE memory map cmd failed %d\n",
		       __func__, ret);
	kfree(mmap_region_cmd);
	return ret;
}

/**
 * q6afe_audio_client_buf_free_contiguous -
 *         frees the shared contiguous memory
 *
 * @dir: RX or TX direction of port
 * @ac: AFE audio client handle
 *
 */
int q6afe_audio_client_buf_free_contiguous(unsigned int dir,
			struct afe_audio_client *ac)
{
	struct afe_audio_port_data *port;
	int cnt = 0;

	mutex_lock(&ac->cmd_lock);
	port = &ac->port[dir];
	if (!port->buf) {
		pr_err("%s: buf is null\n", __func__);
		mutex_unlock(&ac->cmd_lock);
		return 0;
	}
	cnt = port->max_buf_cnt - 1;

	if (port->buf[0].data) {
		pr_debug("%s: data[%pK], phys[%pK], dma_buf[%pK]\n",
			__func__,
			port->buf[0].data,
			&port->buf[0].phys,
			port->buf[0].dma_buf);
		msm_audio_ion_free(port->buf[0].dma_buf);
		port->buf[0].dma_buf = NULL;
	}

	while (cnt >= 0) {
		port->buf[cnt].data = NULL;
		port->buf[cnt].phys = 0;
		cnt--;
	}
	port->max_buf_cnt = 0;
	kfree(port->buf);
	port->buf = NULL;
	mutex_unlock(&ac->cmd_lock);
	return 0;
}
EXPORT_SYMBOL(q6afe_audio_client_buf_free_contiguous);

/**
 * q6afe_audio_client_free -
 *         frees the audio client from AFE
 *
 * @ac: AFE audio client handle
 *
 */
void q6afe_audio_client_free(struct afe_audio_client *ac)
{
	int loopcnt;
	struct afe_audio_port_data *port;

	if (!ac) {
		pr_err("%s: audio client is NULL\n", __func__);
		return;
	}
	for (loopcnt = 0; loopcnt <= OUT; loopcnt++) {
		port = &ac->port[loopcnt];
		if (!port->buf)
			continue;
		pr_debug("%s: loopcnt = %d\n", __func__, loopcnt);
		q6afe_audio_client_buf_free_contiguous(loopcnt, ac);
	}
	kfree(ac);
}
EXPORT_SYMBOL(q6afe_audio_client_free);

/**
 * afe_cmd_memory_unmap -
 *         command to unmap memory for AFE shared buffer
 *
 * @mem_map_handle: memory map handle to be unmapped
 *
 * Returns 0 on success or error on failure
 */
int afe_cmd_memory_unmap(u32 mem_map_handle)
{
	int ret = 0;
	struct afe_service_cmd_shared_mem_unmap_regions mregion;
	int index = 0;

	pr_debug("%s: handle 0x%x\n", __func__, mem_map_handle);

	if (this_afe.apr == NULL) {
		this_afe.apr = apr_register("ADSP", "AFE", afe_callback,
					0xFFFFFFFF, &this_afe);
		pr_debug("%s: Register AFE\n", __func__);
		if (this_afe.apr == NULL) {
			pr_err("%s: Unable to register AFE\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_afe_handle(this_afe.apr);
	}

	mregion.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	mregion.hdr.pkt_size = sizeof(mregion);
	mregion.hdr.src_port = 0;
	mregion.hdr.dest_port = 0;
	mregion.hdr.token = 0;
	mregion.hdr.opcode = AFE_SERVICE_CMD_SHARED_MEM_UNMAP_REGIONS;
	mregion.mem_map_handle = mem_map_handle;

	/* Todo */
	index = mregion.hdr.token = IDX_RSVD_2;

	atomic_set(&this_afe.status, 0);
	ret = afe_apr_send_pkt(&mregion, &this_afe.wait[index]);
	if (ret)
		pr_err("%s: AFE memory unmap cmd failed %d\n",
		       __func__, ret);

	return ret;
}
EXPORT_SYMBOL(afe_cmd_memory_unmap);

int afe_cmd_memory_unmap_nowait(u32 mem_map_handle)
{
	int ret = 0;
	struct afe_service_cmd_shared_mem_unmap_regions mregion;

	pr_debug("%s: handle 0x%x\n", __func__, mem_map_handle);

	if (this_afe.apr == NULL) {
		this_afe.apr = apr_register("ADSP", "AFE", afe_callback,
					0xFFFFFFFF, &this_afe);
		pr_debug("%s: Register AFE\n", __func__);
		if (this_afe.apr == NULL) {
			pr_err("%s: Unable to register AFE\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_afe_handle(this_afe.apr);
	}

	mregion.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	mregion.hdr.pkt_size = sizeof(mregion);
	mregion.hdr.src_port = 0;
	mregion.hdr.dest_port = 0;
	mregion.hdr.token = 0;
	mregion.hdr.opcode = AFE_SERVICE_CMD_SHARED_MEM_UNMAP_REGIONS;
	mregion.mem_map_handle = mem_map_handle;

	ret = afe_apr_send_pkt(&mregion, NULL);
	if (ret)
		pr_err("%s: AFE memory unmap cmd failed %d\n",
			__func__, ret);
	return ret;
}

/**
 * afe_register_get_events -
 *         register for events from proxy port
 *
 * @port_id: Port ID to register events
 * @cb: callback function to invoke for events from proxy port
 * @private_data: private data to sent back in callback fn
 *
 * Returns 0 on success or error on failure
 */
int afe_register_get_events(u16 port_id,
		void (*cb)(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv),
		void *private_data)
{
	int ret = 0;
	struct afe_service_cmd_register_rt_port_driver rtproxy;

	pr_debug("%s: port_id: 0x%x\n", __func__, port_id);

	if (this_afe.apr == NULL) {
		this_afe.apr = apr_register("ADSP", "AFE", afe_callback,
					0xFFFFFFFF, &this_afe);
		pr_debug("%s: Register AFE\n", __func__);
		if (this_afe.apr == NULL) {
			pr_err("%s: Unable to register AFE\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_afe_handle(this_afe.apr);
	}
	if ((port_id == RT_PROXY_DAI_002_RX) ||
		(port_id == RT_PROXY_DAI_001_TX)) {
		port_id = VIRTUAL_ID_TO_PORTID(port_id);
	} else {
		pr_err("%s: wrong port id 0x%x\n", __func__, port_id);
		return -EINVAL;
	}

	if (port_id == RT_PROXY_PORT_001_TX) {
		this_afe.tx_cb = cb;
		this_afe.tx_private_data = private_data;
	} else if (port_id == RT_PROXY_PORT_001_RX) {
		this_afe.rx_cb = cb;
		this_afe.rx_private_data = private_data;
	}

	rtproxy.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	rtproxy.hdr.pkt_size = sizeof(rtproxy);
	rtproxy.hdr.src_port = 1;
	rtproxy.hdr.dest_port = 1;
	rtproxy.hdr.opcode = AFE_SERVICE_CMD_REGISTER_RT_PORT_DRIVER;
	rtproxy.port_id = port_id;
	rtproxy.reserved = 0;

	ret = afe_apr_send_pkt(&rtproxy, NULL);
	if (ret)
		pr_err("%s: AFE  reg. rtproxy_event failed %d\n",
			   __func__, ret);
	return ret;
}
EXPORT_SYMBOL(afe_register_get_events);

/**
 * afe_unregister_get_events -
 *         unregister for events from proxy port
 *
 * @port_id: Port ID to unregister events
 *
 * Returns 0 on success or error on failure
 */
int afe_unregister_get_events(u16 port_id)
{
	int ret = 0;
	struct afe_service_cmd_unregister_rt_port_driver rtproxy;
	int index = 0;

	pr_debug("%s:\n", __func__);

	if (this_afe.apr == NULL) {
		this_afe.apr = apr_register("ADSP", "AFE", afe_callback,
					0xFFFFFFFF, &this_afe);
		pr_debug("%s: Register AFE\n", __func__);
		if (this_afe.apr == NULL) {
			pr_err("%s: Unable to register AFE\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_afe_handle(this_afe.apr);
	}

	if ((port_id == RT_PROXY_DAI_002_RX) ||
		(port_id == RT_PROXY_DAI_001_TX)) {
		port_id = VIRTUAL_ID_TO_PORTID(port_id);
	} else {
		pr_err("%s: wrong port id 0x%x\n", __func__, port_id);
		return -EINVAL;
	}

	index = q6audio_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		return -EINVAL;
	}
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: Invalid port 0x%x ret %d", __func__, port_id, ret);
		return -EINVAL;
	}

	rtproxy.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	rtproxy.hdr.pkt_size = sizeof(rtproxy);
	rtproxy.hdr.src_port = 0;
	rtproxy.hdr.dest_port = 0;
	rtproxy.hdr.token = 0;
	rtproxy.hdr.opcode = AFE_SERVICE_CMD_UNREGISTER_RT_PORT_DRIVER;
	rtproxy.port_id = port_id;
	rtproxy.reserved = 0;

	rtproxy.hdr.token = index;

	if (port_id == RT_PROXY_PORT_001_TX) {
		this_afe.tx_cb = NULL;
		this_afe.tx_private_data = NULL;
	} else if (port_id == RT_PROXY_PORT_001_RX) {
		this_afe.rx_cb = NULL;
		this_afe.rx_private_data = NULL;
	}

	ret = afe_apr_send_pkt(&rtproxy, &this_afe.wait[index]);
	if (ret)
		pr_err("%s: AFE enable Unreg. rtproxy_event failed %d\n",
			   __func__, ret);
	return ret;
}
EXPORT_SYMBOL(afe_unregister_get_events);

/**
 * afe_rt_proxy_port_write -
 *         command for AFE RT proxy port write
 *
 * @buf_addr_p: Physical buffer address with
 *           playback data to proxy port
 * @mem_map_handle: memory map handle of write buffer
 * @bytes: number of bytes to write
 *
 * Returns 0 on success or error on failure
 */
int afe_rt_proxy_port_write(phys_addr_t buf_addr_p,
		u32 mem_map_handle, int bytes)
{
	int ret = 0;
	struct afe_port_data_cmd_rt_proxy_port_write_v2 afecmd_wr;

	if (this_afe.apr == NULL) {
		pr_err("%s: register to AFE is not done\n", __func__);
		ret = -ENODEV;
		return ret;
	}
	pr_debug("%s: buf_addr_p = 0x%pK bytes = %d\n", __func__,
						&buf_addr_p, bytes);

	afecmd_wr.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	afecmd_wr.hdr.pkt_size = sizeof(afecmd_wr);
	afecmd_wr.hdr.src_port = 0;
	afecmd_wr.hdr.dest_port = 0;
	afecmd_wr.hdr.token = 0;
	afecmd_wr.hdr.opcode = AFE_PORT_DATA_CMD_RT_PROXY_PORT_WRITE_V2;
	afecmd_wr.port_id = RT_PROXY_PORT_001_TX;
	afecmd_wr.buffer_address_lsw = lower_32_bits(buf_addr_p);
	afecmd_wr.buffer_address_msw =
			msm_audio_populate_upper_32_bits(buf_addr_p);
	afecmd_wr.mem_map_handle = mem_map_handle;
	afecmd_wr.available_bytes = bytes;
	afecmd_wr.reserved = 0;

	ret = afe_apr_send_pkt(&afecmd_wr, NULL);
	if (ret)
		pr_err("%s: AFE rtproxy write to port 0x%x failed %d\n",
			   __func__, afecmd_wr.port_id, ret);
	return ret;

}
EXPORT_SYMBOL(afe_rt_proxy_port_write);

/**
 * afe_rt_proxy_port_read -
 *         command for AFE RT proxy port read
 *
 * @buf_addr_p: Physical buffer address to fill read data
 * @mem_map_handle: memory map handle for buffer read
 * @bytes: number of bytes to read
 *
 * Returns 0 on success or error on failure
 */
int afe_rt_proxy_port_read(phys_addr_t buf_addr_p,
		u32 mem_map_handle, int bytes)
{
	int ret = 0;
	struct afe_port_data_cmd_rt_proxy_port_read_v2 afecmd_rd;

	if (this_afe.apr == NULL) {
		pr_err("%s: register to AFE is not done\n", __func__);
		ret = -ENODEV;
		return ret;
	}
	pr_debug("%s: buf_addr_p = 0x%pK bytes = %d\n", __func__,
						&buf_addr_p, bytes);

	afecmd_rd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	afecmd_rd.hdr.pkt_size = sizeof(afecmd_rd);
	afecmd_rd.hdr.src_port = 0;
	afecmd_rd.hdr.dest_port = 0;
	afecmd_rd.hdr.token = 0;
	afecmd_rd.hdr.opcode = AFE_PORT_DATA_CMD_RT_PROXY_PORT_READ_V2;
	afecmd_rd.port_id = RT_PROXY_PORT_001_RX;
	afecmd_rd.buffer_address_lsw = lower_32_bits(buf_addr_p);
	afecmd_rd.buffer_address_msw =
				msm_audio_populate_upper_32_bits(buf_addr_p);
	afecmd_rd.available_bytes = bytes;
	afecmd_rd.mem_map_handle = mem_map_handle;

	ret = afe_apr_send_pkt(&afecmd_rd, NULL);
	if (ret)
		pr_err("%s: AFE rtproxy read  cmd to port 0x%x failed %d\n",
			   __func__, afecmd_rd.port_id, ret);
	return ret;
}
EXPORT_SYMBOL(afe_rt_proxy_port_read);

#ifdef CONFIG_DEBUG_FS
static struct dentry *debugfs_afelb;
static struct dentry *debugfs_afelb_gain;

static int afe_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	pr_info("%s: debug intf %s\n", __func__, (char *) file->private_data);
	return 0;
}

static int afe_get_parameters(char *buf, long int *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtoul(token, base, &param1[cnt]) != 0) {
				pr_err("%s: kstrtoul failed\n",
					__func__);
				return -EINVAL;
			}

			token = strsep(&buf, " ");
		} else {
			pr_err("%s: token NULL\n", __func__);
			return -EINVAL;
		}
	}
	return 0;
}
#define AFE_LOOPBACK_ON (1)
#define AFE_LOOPBACK_OFF (0)
static ssize_t afe_debug_write(struct file *filp,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char *lb_str = filp->private_data;
	char lbuf[32];
	int rc;
	unsigned long param[5];

	if (cnt > sizeof(lbuf) - 1) {
		pr_err("%s: cnt %zd size %zd\n", __func__, cnt, sizeof(lbuf)-1);
		return -EINVAL;
	}

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc) {
		pr_err("%s: copy from user failed %d\n", __func__, rc);
		return -EFAULT;
	}

	lbuf[cnt] = '\0';

	if (!strcmp(lb_str, "afe_loopback")) {
		rc = afe_get_parameters(lbuf, param, 3);
		if (!rc) {
			pr_info("%s: %lu %lu %lu\n", lb_str, param[0], param[1],
				param[2]);

			if ((param[0] != AFE_LOOPBACK_ON) && (param[0] !=
				AFE_LOOPBACK_OFF)) {
				pr_err("%s: Error, parameter 0 incorrect\n",
					__func__);
				rc = -EINVAL;
				goto afe_error;
			}
			if ((q6audio_validate_port(param[1]) < 0) ||
			    (q6audio_validate_port(param[2])) < 0) {
				pr_err("%s: Error, invalid afe port\n",
					__func__);
			}
			if (this_afe.apr == NULL) {
				pr_err("%s: Error, AFE not opened\n", __func__);
				rc = -EINVAL;
			} else {
				rc = afe_loopback(param[0], param[1], param[2]);
			}
		} else {
			pr_err("%s: Error, invalid parameters\n", __func__);
			rc = -EINVAL;
		}

	} else if (!strcmp(lb_str, "afe_loopback_gain")) {
		rc = afe_get_parameters(lbuf, param, 2);
		if (!rc) {
			pr_info("%s: %s %lu %lu\n",
				__func__, lb_str, param[0], param[1]);

			rc = q6audio_validate_port(param[0]);
			if (rc < 0) {
				pr_err("%s: Error, invalid afe port %d %lu\n",
					__func__, rc, param[0]);
				rc = -EINVAL;
				goto afe_error;
			}

			if (param[1] > 100) {
				pr_err("%s: Error, volume should be 0 to 100 percentage param = %lu\n",
					__func__, param[1]);
				rc = -EINVAL;
				goto afe_error;
			}

			param[1] = (Q6AFE_MAX_VOLUME * param[1]) / 100;

			if (this_afe.apr == NULL) {
				pr_err("%s: Error, AFE not opened\n", __func__);
				rc = -EINVAL;
			} else {
				rc = afe_loopback_gain(param[0], param[1]);
			}
		} else {
			pr_err("%s: Error, invalid parameters\n", __func__);
			rc = -EINVAL;
		}
	}

afe_error:
	if (rc == 0)
		rc = cnt;
	else
		pr_err("%s: rc = %d\n", __func__, rc);

	return rc;
}

static const struct file_operations afe_debug_fops = {
	.open = afe_debug_open,
	.write = afe_debug_write
};

static void config_debug_fs_init(void)
{
	debugfs_afelb = debugfs_create_file("afe_loopback",
	0664, NULL, (void *) "afe_loopback",
	&afe_debug_fops);

	debugfs_afelb_gain = debugfs_create_file("afe_loopback_gain",
	0664, NULL, (void *) "afe_loopback_gain",
	&afe_debug_fops);
}
static void config_debug_fs_exit(void)
{
	debugfs_remove(debugfs_afelb);
	debugfs_remove(debugfs_afelb_gain);
}
#else
static void config_debug_fs_init(void)
{
}
static void config_debug_fs_exit(void)
{
}
#endif

/**
 * afe_set_dtmf_gen_rx_portid -
 *         Set port_id for DTMF tone generation
 *
 * @port_id: AFE port id
 * @set: set or reset port id value for dtmf gen
 *
 */
void afe_set_dtmf_gen_rx_portid(u16 port_id, int set)
{
	if (set)
		this_afe.dtmf_gen_rx_portid = port_id;
	else if (this_afe.dtmf_gen_rx_portid == port_id)
		this_afe.dtmf_gen_rx_portid = -1;
}
EXPORT_SYMBOL(afe_set_dtmf_gen_rx_portid);

/**
 * afe_dtmf_generate_rx - command to generate AFE DTMF RX
 *
 * @duration_in_ms: Duration in ms for dtmf tone
 * @high_freq: Higher frequency for dtmf
 * @low_freq: lower frequency for dtmf
 * @gain: Gain value for DTMF tone
 *
 * Returns 0 on success, appropriate error code otherwise
 */
int afe_dtmf_generate_rx(int64_t duration_in_ms,
			 uint16_t high_freq,
			 uint16_t low_freq, uint16_t gain)
{
	int ret = 0;
	int index = 0;
	struct afe_dtmf_generation_command cmd_dtmf;

	pr_debug("%s: DTMF AFE Gen\n", __func__);

	if (afe_validate_port(this_afe.dtmf_gen_rx_portid) < 0) {
		pr_err("%s: Failed : Invalid Port id = 0x%x\n",
		       __func__, this_afe.dtmf_gen_rx_portid);
		ret = -EINVAL;
		goto fail_cmd;
	}

	if (this_afe.apr == NULL) {
		this_afe.apr = apr_register("ADSP", "AFE", afe_callback,
					    0xFFFFFFFF, &this_afe);
		pr_debug("%s: Register AFE\n", __func__);
		if (this_afe.apr == NULL) {
			pr_err("%s: Unable to register AFE\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_afe_handle(this_afe.apr);
	}

	pr_debug("%s: dur=%lld: hfreq=%d lfreq=%d gain=%d portid=0x%x\n",
		__func__,
		duration_in_ms, high_freq, low_freq, gain,
		this_afe.dtmf_gen_rx_portid);

	cmd_dtmf.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cmd_dtmf.hdr.pkt_size = sizeof(cmd_dtmf);
	cmd_dtmf.hdr.src_port = 0;
	cmd_dtmf.hdr.dest_port = 0;
	cmd_dtmf.hdr.token = 0;
	cmd_dtmf.hdr.opcode = AFE_PORTS_CMD_DTMF_CTL;
	cmd_dtmf.duration_in_ms = duration_in_ms;
	cmd_dtmf.high_freq = high_freq;
	cmd_dtmf.low_freq = low_freq;
	cmd_dtmf.gain = gain;
	cmd_dtmf.num_ports = 1;
	cmd_dtmf.port_ids = q6audio_get_port_id(this_afe.dtmf_gen_rx_portid);

	atomic_set(&this_afe.state, 1);
	atomic_set(&this_afe.status, 0);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &cmd_dtmf);
	if (ret < 0) {
		pr_err("%s: AFE DTMF failed for num_ports:%d ids:0x%x\n",
		       __func__, cmd_dtmf.num_ports, cmd_dtmf.port_ids);
		ret = -EINVAL;
		goto fail_cmd;
	}
	index = q6audio_get_port_index(this_afe.dtmf_gen_rx_portid);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = wait_event_timeout(this_afe.wait[index],
		(atomic_read(&this_afe.state) == 0),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	if (atomic_read(&this_afe.status) > 0) {
		pr_err("%s: config cmd failed [%s]\n",
			__func__, adsp_err_get_err_str(
			atomic_read(&this_afe.status)));
		ret = adsp_err_get_lnx_err_code(
				atomic_read(&this_afe.status));
		goto fail_cmd;
	}
	return 0;

fail_cmd:
	pr_err("%s: failed %d\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(afe_dtmf_generate_rx);

static int afe_sidetone_iir(u16 tx_port_id)
{
	int ret;
	uint16_t size = 0;
	int cal_index = AFE_SIDETONE_IIR_CAL;
	int iir_pregain = 0;
	int iir_num_biquad_stages = 0;
	int iir_enable;
	struct cal_block_data *cal_block;
	int mid;
	struct afe_mod_enable_param enable;
	struct afe_sidetone_iir_filter_config_params filter_data;
	struct param_hdr_v3 param_hdr;
	u8 *packed_param_data = NULL;
	u32 packed_param_size = 0;
	u32 single_param_size = 0;
	struct audio_cal_info_sidetone_iir *st_iir_cal_info = NULL;

	memset(&enable, 0, sizeof(enable));
	memset(&filter_data, 0, sizeof(filter_data));
	memset(&param_hdr, 0, sizeof(param_hdr));

	if (this_afe.cal_data[cal_index] == NULL) {
		pr_err("%s: cal data is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	mutex_lock(&this_afe.cal_data[cal_index]->lock);
	cal_block = cal_utils_get_only_cal_block(this_afe.cal_data[cal_index]);
	if (cal_block == NULL || cal_utils_is_cal_stale(cal_block)) {
		pr_err("%s: cal_block not found\n ", __func__);
		mutex_unlock(&this_afe.cal_data[cal_index]->lock);
		ret = -EINVAL;
		goto done;
	}

	/* Cache data from cal block while inside lock to reduce locked time */
	st_iir_cal_info =
		(struct audio_cal_info_sidetone_iir *) cal_block->cal_info;
	iir_pregain = st_iir_cal_info->pregain;
	iir_enable = st_iir_cal_info->iir_enable;
	iir_num_biquad_stages = st_iir_cal_info->num_biquad_stages;
	mid = st_iir_cal_info->mid;

	/*
	 * calculate the actual size of payload based on no of stages
	 * enabled in calibration
	 */
	size = (MAX_SIDETONE_IIR_DATA_SIZE / MAX_NO_IIR_FILTER_STAGE) *
		iir_num_biquad_stages;
	/*
	 * For an odd number of stages, 2 bytes of padding are
	 * required at the end of the payload.
	 */
	if (iir_num_biquad_stages % 2) {
		pr_debug("%s: adding 2 to size:%d\n", __func__, size);
		size = size + 2;
	}
	memcpy(&filter_data.iir_config, &st_iir_cal_info->iir_config, size);
	mutex_unlock(&this_afe.cal_data[cal_index]->lock);

	packed_param_size =
		sizeof(param_hdr) * 2 + sizeof(enable) + sizeof(filter_data);
	packed_param_data = kzalloc(packed_param_size, GFP_KERNEL);
	if (!packed_param_data)
		return -ENOMEM;
	packed_param_size = 0;

	/*
	 * Set IIR enable params
	 */
	param_hdr.module_id = mid;
	param_hdr.param_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_ENABLE;
	param_hdr.param_size = sizeof(enable);
	enable.enable = iir_enable;
	ret = q6common_pack_pp_params(packed_param_data, &param_hdr,
				      (u8 *) &enable, &single_param_size);
	if (ret) {
		pr_err("%s: Failed to pack param data, error %d\n", __func__,
		       ret);
		goto done;
	}
	packed_param_size += single_param_size;

	/*
	 * Set IIR filter config params
	 */
	param_hdr.module_id = mid;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_SIDETONE_IIR_FILTER_CONFIG;
	param_hdr.param_size = sizeof(filter_data.num_biquad_stages) +
			       sizeof(filter_data.pregain) + size;
	filter_data.num_biquad_stages = iir_num_biquad_stages;
	filter_data.pregain = iir_pregain;
	ret = q6common_pack_pp_params(packed_param_data + packed_param_size,
				      &param_hdr, (u8 *) &filter_data,
				      &single_param_size);
	if (ret) {
		pr_err("%s: Failed to pack param data, error %d\n", __func__,
		       ret);
		goto done;
	}
	packed_param_size += single_param_size;

	pr_debug("%s: tx(0x%x)mid(0x%x)iir_en(%d)stg(%d)gain(0x%x)size(%d)\n",
		 __func__, tx_port_id, mid, enable.enable,
		 filter_data.num_biquad_stages, filter_data.pregain,
		 param_hdr.param_size);

	ret = q6afe_set_params(tx_port_id, q6audio_get_port_index(tx_port_id),
			       NULL, packed_param_data, packed_param_size);
	if (ret)
		pr_err("%s: AFE sidetone failed for tx_port(0x%x)\n",
			 __func__, tx_port_id);

done:
	kfree(packed_param_data);
	return ret;
}

static int afe_sidetone(u16 tx_port_id, u16 rx_port_id, bool enable)
{
	int ret;
	int cal_index = AFE_SIDETONE_CAL;
	int sidetone_gain;
	int sidetone_enable;
	struct cal_block_data *cal_block;
	int mid = 0;
	struct afe_loopback_sidetone_gain gain_data;
	struct loopback_cfg_data cfg_data;
	struct param_hdr_v3 param_hdr;
	u8 *packed_param_data = NULL;
	u32 packed_param_size = 0;
	u32 single_param_size = 0;
	struct audio_cal_info_sidetone *st_cal_info = NULL;

	if (this_afe.cal_data[cal_index] == NULL) {
		pr_err("%s: cal data is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	memset(&gain_data, 0, sizeof(gain_data));
	memset(&cfg_data, 0, sizeof(cfg_data));
	memset(&param_hdr, 0, sizeof(param_hdr));

	packed_param_size =
		sizeof(param_hdr) * 2 + sizeof(gain_data) + sizeof(cfg_data);
	packed_param_data = kzalloc(packed_param_size, GFP_KERNEL);
	if (!packed_param_data)
		return -ENOMEM;
	packed_param_size = 0;

	mutex_lock(&this_afe.cal_data[cal_index]->lock);
	cal_block = cal_utils_get_only_cal_block(this_afe.cal_data[cal_index]);
	if (cal_block == NULL || cal_utils_is_cal_stale(cal_block)) {
		pr_err("%s: cal_block not found\n", __func__);
		mutex_unlock(&this_afe.cal_data[cal_index]->lock);
		ret = -EINVAL;
		goto done;
	}

	/* Cache data from cal block while inside lock to reduce locked time */
	st_cal_info = (struct audio_cal_info_sidetone *) cal_block->cal_info;
	sidetone_gain = st_cal_info->gain;
	sidetone_enable = st_cal_info->enable;
	mid = st_cal_info->mid;
	mutex_unlock(&this_afe.cal_data[cal_index]->lock);

	/* Set gain data. */
	param_hdr.module_id = AFE_MODULE_LOOPBACK;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_LOOPBACK_GAIN_PER_PATH;
	param_hdr.param_size = sizeof(struct afe_loopback_sidetone_gain);
	gain_data.rx_port_id = rx_port_id;
	gain_data.gain = sidetone_gain;
	ret = q6common_pack_pp_params(packed_param_data, &param_hdr,
				      (u8 *) &gain_data, &single_param_size);
	if (ret) {
		pr_err("%s: Failed to pack param data, error %d\n", __func__,
		       ret);
		goto done;
	}
	packed_param_size += single_param_size;

	/* Set configuration data. */
	param_hdr.module_id = AFE_MODULE_LOOPBACK;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_LOOPBACK_CONFIG;
	param_hdr.param_size = sizeof(struct loopback_cfg_data);
	cfg_data.loopback_cfg_minor_version = AFE_API_VERSION_LOOPBACK_CONFIG;
	cfg_data.dst_port_id = rx_port_id;
	cfg_data.routing_mode = LB_MODE_SIDETONE;
	cfg_data.enable = enable;
	ret = q6common_pack_pp_params(packed_param_data + packed_param_size,
				      &param_hdr, (u8 *) &cfg_data,
				      &single_param_size);
	if (ret) {
		pr_err("%s: Failed to pack param data, error %d\n", __func__,
		       ret);
		goto done;
	}
	packed_param_size += single_param_size;

	pr_debug("%s rx(0x%x) tx(0x%x) enable(%d) mid(0x%x) gain(%d) sidetone_enable(%d)\n",
		  __func__, rx_port_id, tx_port_id,
		  enable, mid, sidetone_gain, sidetone_enable);

	ret = q6afe_set_params(tx_port_id, q6audio_get_port_index(tx_port_id),
			       NULL, packed_param_data, packed_param_size);
	if (ret)
		pr_err("%s: AFE sidetone send failed for tx_port:%d rx_port:%d ret:%d\n",
			__func__, tx_port_id, rx_port_id, ret);

done:
	kfree(packed_param_data);
	return ret;
}

int afe_sidetone_enable(u16 tx_port_id, u16 rx_port_id, bool enable)
{
	int ret;
	int index;

	index = q6audio_get_port_index(rx_port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		ret = -EINVAL;
		goto done;
	}
	if (q6audio_validate_port(rx_port_id) < 0) {
		pr_err("%s: Invalid port 0x%x\n",
				__func__, rx_port_id);
		ret = -EINVAL;
		goto done;
	}
	index = q6audio_get_port_index(tx_port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		ret = -EINVAL;
		goto done;
	}
	if (q6audio_validate_port(tx_port_id) < 0) {
		pr_err("%s: Invalid port 0x%x\n",
				__func__, tx_port_id);
		ret = -EINVAL;
		goto done;
	}
	if (enable) {
		ret = afe_sidetone_iir(tx_port_id);
		if (ret)
			goto done;
	}

	ret = afe_sidetone(tx_port_id, rx_port_id, enable);

done:
	return ret;
}

/**
 * afe_set_display_stream - command to update AFE dp port params
 *
 * @rx_port_id: AFE port id
 * @stream_idx: dp controller stream index
 * @ctl_idx: dp controller index
 *
 * Returns 0 on success, appropriate error code otherwise
 */
int afe_set_display_stream(u16 rx_port_id, u32 stream_idx, u32 ctl_idx)
{
	int ret;
	struct param_hdr_v3 param_hdr;
	u32 packed_param_size = 0;
	u8 *packed_param_data = NULL;
	struct afe_display_stream_idx stream_data;
	struct afe_display_ctl_idx ctl_data;
	u32 single_param_size = 0;

	memset(&param_hdr, 0, sizeof(param_hdr));
	memset(&stream_data, 0, sizeof(stream_data));
	memset(&ctl_data, 0, sizeof(ctl_data));

	packed_param_size =
		sizeof(param_hdr) * 2 + sizeof(stream_data) + sizeof(ctl_data);
	packed_param_data = kzalloc(packed_param_size, GFP_KERNEL);
	if (!packed_param_data)
		return -ENOMEM;
	packed_param_size = 0;

	/* Set stream index */
	param_hdr.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_HDMI_DP_MST_VID_IDX_CFG;
	param_hdr.param_size = sizeof(struct afe_display_stream_idx);
	stream_data.minor_version = 1;
	stream_data.stream_idx = stream_idx;
	ret = q6common_pack_pp_params(packed_param_data, &param_hdr,
				      (u8 *) &stream_data, &single_param_size);
	if (ret) {
		pr_err("%s: Failed to pack param data, error %d\n", __func__,
		       ret);
		goto done;
	}
	packed_param_size += single_param_size;

	/* Set controller dptx index */
	param_hdr.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_HDMI_DPTX_IDX_CFG;
	param_hdr.param_size = sizeof(struct afe_display_ctl_idx);
	ctl_data.minor_version = 1;
	ctl_data.ctl_idx = ctl_idx;
	ret = q6common_pack_pp_params(packed_param_data + packed_param_size,
				      &param_hdr, (u8 *) &ctl_data,
				      &single_param_size);
	if (ret) {
		pr_err("%s: Failed to pack param data, error %d\n", __func__,
		       ret);
		goto done;
	}
	packed_param_size += single_param_size;

	pr_debug("%s: rx(0x%x) stream(%d) controller(%d)\n",
		 __func__, rx_port_id, stream_idx, ctl_idx);

	ret = q6afe_set_params(rx_port_id, q6audio_get_port_index(rx_port_id),
			       NULL, packed_param_data, packed_param_size);
	if (ret)
		pr_err("%s: AFE display stream send failed for rx_port:%d ret:%d\n",
			__func__, rx_port_id, ret);

done:
	kfree(packed_param_data);
	return ret;

}
EXPORT_SYMBOL(afe_set_display_stream);

int afe_validate_port(u16 port_id)
{
	int ret;

	switch (port_id) {
	case PRIMARY_I2S_RX:
	case PRIMARY_I2S_TX:
	case AFE_PORT_ID_PRIMARY_PCM_RX:
	case AFE_PORT_ID_PRIMARY_PCM_TX:
	case AFE_PORT_ID_SECONDARY_PCM_RX:
	case AFE_PORT_ID_SECONDARY_PCM_TX:
	case AFE_PORT_ID_TERTIARY_PCM_RX:
	case AFE_PORT_ID_TERTIARY_PCM_TX:
	case AFE_PORT_ID_QUATERNARY_PCM_RX:
	case AFE_PORT_ID_QUATERNARY_PCM_TX:
	case AFE_PORT_ID_QUINARY_PCM_RX:
	case AFE_PORT_ID_QUINARY_PCM_TX:
	case SECONDARY_I2S_RX:
	case SECONDARY_I2S_TX:
	case MI2S_RX:
	case MI2S_TX:
	case HDMI_RX:
	case DISPLAY_PORT_RX:
	case AFE_PORT_ID_PRIMARY_SPDIF_RX:
	case AFE_PORT_ID_PRIMARY_SPDIF_TX:
	case AFE_PORT_ID_SECONDARY_SPDIF_RX:
	case AFE_PORT_ID_SECONDARY_SPDIF_TX:
	case RSVD_2:
	case RSVD_3:
	case DIGI_MIC_TX:
	case VOICE_RECORD_RX:
	case VOICE_RECORD_TX:
	case VOICE_PLAYBACK_TX:
	case VOICE2_PLAYBACK_TX:
	case SLIMBUS_0_RX:
	case SLIMBUS_0_TX:
	case SLIMBUS_1_RX:
	case SLIMBUS_1_TX:
	case SLIMBUS_2_RX:
	case SLIMBUS_2_TX:
	case SLIMBUS_3_RX:
	case INT_BT_SCO_RX:
	case INT_BT_SCO_TX:
	case INT_BT_A2DP_RX:
	case INT_FM_RX:
	case INT_FM_TX:
	case RT_PROXY_PORT_001_RX:
	case RT_PROXY_PORT_001_TX:
	case SLIMBUS_4_RX:
	case SLIMBUS_4_TX:
	case SLIMBUS_5_RX:
	case SLIMBUS_6_RX:
	case SLIMBUS_6_TX:
	case SLIMBUS_7_RX:
	case SLIMBUS_7_TX:
	case SLIMBUS_8_RX:
	case SLIMBUS_8_TX:
	case SLIMBUS_9_RX:
	case SLIMBUS_9_TX:
	case AFE_PORT_ID_USB_RX:
	case AFE_PORT_ID_USB_TX:
	case AFE_PORT_ID_PRIMARY_MI2S_RX:
	case AFE_PORT_ID_PRIMARY_MI2S_TX:
	case AFE_PORT_ID_SECONDARY_MI2S_RX:
	case AFE_PORT_ID_SECONDARY_MI2S_TX:
	case AFE_PORT_ID_QUATERNARY_MI2S_RX:
	case AFE_PORT_ID_QUATERNARY_MI2S_TX:
	case AFE_PORT_ID_TERTIARY_MI2S_RX:
	case AFE_PORT_ID_TERTIARY_MI2S_TX:
	case AFE_PORT_ID_QUINARY_MI2S_RX:
	case AFE_PORT_ID_QUINARY_MI2S_TX:
	case AFE_PORT_ID_SENARY_MI2S_TX:
	case AFE_PORT_ID_PRIMARY_TDM_RX:
	case AFE_PORT_ID_PRIMARY_TDM_TX:
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
	case AFE_PORT_ID_PRIMARY_TDM_RX_4:
	case AFE_PORT_ID_PRIMARY_TDM_TX_4:
	case AFE_PORT_ID_PRIMARY_TDM_RX_5:
	case AFE_PORT_ID_PRIMARY_TDM_TX_5:
	case AFE_PORT_ID_PRIMARY_TDM_RX_6:
	case AFE_PORT_ID_PRIMARY_TDM_TX_6:
	case AFE_PORT_ID_PRIMARY_TDM_RX_7:
	case AFE_PORT_ID_PRIMARY_TDM_TX_7:
	case AFE_PORT_ID_SECONDARY_TDM_RX:
	case AFE_PORT_ID_SECONDARY_TDM_TX:
	case AFE_PORT_ID_SECONDARY_TDM_RX_1:
	case AFE_PORT_ID_SECONDARY_TDM_TX_1:
	case AFE_PORT_ID_SECONDARY_TDM_RX_2:
	case AFE_PORT_ID_SECONDARY_TDM_TX_2:
	case AFE_PORT_ID_SECONDARY_TDM_RX_3:
	case AFE_PORT_ID_SECONDARY_TDM_TX_3:
	case AFE_PORT_ID_SECONDARY_TDM_RX_4:
	case AFE_PORT_ID_SECONDARY_TDM_TX_4:
	case AFE_PORT_ID_SECONDARY_TDM_RX_5:
	case AFE_PORT_ID_SECONDARY_TDM_TX_5:
	case AFE_PORT_ID_SECONDARY_TDM_RX_6:
	case AFE_PORT_ID_SECONDARY_TDM_TX_6:
	case AFE_PORT_ID_SECONDARY_TDM_RX_7:
	case AFE_PORT_ID_SECONDARY_TDM_TX_7:
	case AFE_PORT_ID_TERTIARY_TDM_RX:
	case AFE_PORT_ID_TERTIARY_TDM_TX:
	case AFE_PORT_ID_TERTIARY_TDM_RX_1:
	case AFE_PORT_ID_TERTIARY_TDM_TX_1:
	case AFE_PORT_ID_TERTIARY_TDM_RX_2:
	case AFE_PORT_ID_TERTIARY_TDM_TX_2:
	case AFE_PORT_ID_TERTIARY_TDM_RX_3:
	case AFE_PORT_ID_TERTIARY_TDM_TX_3:
	case AFE_PORT_ID_TERTIARY_TDM_RX_4:
	case AFE_PORT_ID_TERTIARY_TDM_TX_4:
	case AFE_PORT_ID_TERTIARY_TDM_RX_5:
	case AFE_PORT_ID_TERTIARY_TDM_TX_5:
	case AFE_PORT_ID_TERTIARY_TDM_RX_6:
	case AFE_PORT_ID_TERTIARY_TDM_TX_6:
	case AFE_PORT_ID_TERTIARY_TDM_RX_7:
	case AFE_PORT_ID_TERTIARY_TDM_TX_7:
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_1:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_1:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_2:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_2:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_3:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_3:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_4:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_4:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_5:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_5:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_6:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_6:
	case AFE_PORT_ID_QUATERNARY_TDM_RX_7:
	case AFE_PORT_ID_QUATERNARY_TDM_TX_7:
	case AFE_PORT_ID_QUINARY_TDM_RX:
	case AFE_PORT_ID_QUINARY_TDM_TX:
	case AFE_PORT_ID_QUINARY_TDM_RX_1:
	case AFE_PORT_ID_QUINARY_TDM_TX_1:
	case AFE_PORT_ID_QUINARY_TDM_RX_2:
	case AFE_PORT_ID_QUINARY_TDM_TX_2:
	case AFE_PORT_ID_QUINARY_TDM_RX_3:
	case AFE_PORT_ID_QUINARY_TDM_TX_3:
	case AFE_PORT_ID_QUINARY_TDM_RX_4:
	case AFE_PORT_ID_QUINARY_TDM_TX_4:
	case AFE_PORT_ID_QUINARY_TDM_RX_5:
	case AFE_PORT_ID_QUINARY_TDM_TX_5:
	case AFE_PORT_ID_QUINARY_TDM_RX_6:
	case AFE_PORT_ID_QUINARY_TDM_TX_6:
	case AFE_PORT_ID_QUINARY_TDM_RX_7:
	case AFE_PORT_ID_QUINARY_TDM_TX_7:
	case AFE_PORT_ID_INT0_MI2S_RX:
	case AFE_PORT_ID_INT1_MI2S_RX:
	case AFE_PORT_ID_INT2_MI2S_RX:
	case AFE_PORT_ID_INT3_MI2S_RX:
	case AFE_PORT_ID_INT4_MI2S_RX:
	case AFE_PORT_ID_INT5_MI2S_RX:
	case AFE_PORT_ID_INT6_MI2S_RX:
	case AFE_PORT_ID_INT0_MI2S_TX:
	case AFE_PORT_ID_INT1_MI2S_TX:
	case AFE_PORT_ID_INT2_MI2S_TX:
	case AFE_PORT_ID_INT3_MI2S_TX:
	case AFE_PORT_ID_INT4_MI2S_TX:
	case AFE_PORT_ID_INT5_MI2S_TX:
	case AFE_PORT_ID_INT6_MI2S_TX:
	case AFE_PORT_ID_WSA_CODEC_DMA_RX_0:
	case AFE_PORT_ID_WSA_CODEC_DMA_TX_0:
	case AFE_PORT_ID_WSA_CODEC_DMA_RX_1:
	case AFE_PORT_ID_WSA_CODEC_DMA_TX_1:
	case AFE_PORT_ID_WSA_CODEC_DMA_TX_2:
	case AFE_PORT_ID_VA_CODEC_DMA_TX_0:
	case AFE_PORT_ID_VA_CODEC_DMA_TX_1:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_0:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_0:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_1:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_1:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_2:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_2:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_3:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_3:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_4:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_4:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_5:
	case AFE_PORT_ID_TX_CODEC_DMA_TX_5:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_6:
	case AFE_PORT_ID_RX_CODEC_DMA_RX_7:
	{
		ret = 0;
		break;
	}

	default:
		pr_err("%s: default ret 0x%x\n", __func__, port_id);
		ret = -EINVAL;
	}

	return ret;
}

int afe_convert_virtual_to_portid(u16 port_id)
{
	int ret;

	/*
	 * if port_id is virtual, convert to physical..
	 * if port_id is already physical, return physical
	 */
	if (afe_validate_port(port_id) < 0) {
		if (port_id == RT_PROXY_DAI_001_RX ||
		    port_id == RT_PROXY_DAI_001_TX ||
		    port_id == RT_PROXY_DAI_002_RX ||
		    port_id == RT_PROXY_DAI_002_TX) {
			ret = VIRTUAL_ID_TO_PORTID(port_id);
		} else {
			pr_err("%s: wrong port 0x%x\n",
				__func__, port_id);
			ret = -EINVAL;
		}
	} else
		ret = port_id;

	return ret;
}
int afe_port_stop_nowait(int port_id)
{
	struct afe_port_cmd_device_stop stop;
	int ret = 0;

	if (this_afe.apr == NULL) {
		pr_err("%s: AFE is already closed\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	pr_debug("%s: port_id = 0x%x\n", __func__, port_id);
	port_id = q6audio_convert_virtual_to_portid(port_id);

	stop.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	stop.hdr.pkt_size = sizeof(stop);
	stop.hdr.src_port = 0;
	stop.hdr.dest_port = 0;
	stop.hdr.token = 0;
	stop.hdr.opcode = AFE_PORT_CMD_DEVICE_STOP;
	stop.port_id = port_id;
	stop.reserved = 0;

	ret = afe_apr_send_pkt(&stop, NULL);
	if (ret)
		pr_err("%s: AFE close failed %d\n", __func__, ret);

fail_cmd:
	return ret;

}

/**
 * afe_close - command to close AFE port
 *
 * @port_id: AFE port id
 *
 * Returns 0 on success, appropriate error code otherwise
 */
int afe_close(int port_id)
{
	struct afe_port_cmd_device_stop stop;
	enum afe_mad_type mad_type;
	int ret = 0;
	int index = 0;
	uint16_t port_index;

	if (this_afe.apr == NULL) {
		pr_err("%s: AFE is already closed\n", __func__);
		if ((port_id == RT_PROXY_DAI_001_RX) ||
		    (port_id == RT_PROXY_DAI_002_TX))
			pcm_afe_instance[port_id & 0x1] = 0;
		if ((port_id == RT_PROXY_DAI_002_RX) ||
		    (port_id == RT_PROXY_DAI_001_TX))
			proxy_afe_instance[port_id & 0x1] = 0;
		afe_close_done[port_id & 0x1] = true;
		ret = -EINVAL;
		goto fail_cmd;
	}
	pr_debug("%s: port_id = 0x%x\n", __func__, port_id);
	if ((port_id == RT_PROXY_DAI_001_RX) ||
			(port_id == RT_PROXY_DAI_002_TX)) {
		pr_debug("%s: before decrementing pcm_afe_instance %d\n",
			__func__, pcm_afe_instance[port_id & 0x1]);
		port_id = VIRTUAL_ID_TO_PORTID(port_id);
		pcm_afe_instance[port_id & 0x1]--;
		if ((!(pcm_afe_instance[port_id & 0x1] == 0 &&
			proxy_afe_instance[port_id & 0x1] == 0)) ||
			afe_close_done[port_id & 0x1] == true)
			return 0;

		afe_close_done[port_id & 0x1] = true;
	}

	if ((port_id == RT_PROXY_DAI_002_RX) ||
		(port_id == RT_PROXY_DAI_001_TX)) {
		pr_debug("%s: before decrementing proxy_afe_instance %d\n",
			__func__, proxy_afe_instance[port_id & 0x1]);
		port_id = VIRTUAL_ID_TO_PORTID(port_id);
		proxy_afe_instance[port_id & 0x1]--;
		if ((!(pcm_afe_instance[port_id & 0x1] == 0 &&
			proxy_afe_instance[port_id & 0x1] == 0)) ||
			afe_close_done[port_id & 0x1] == true)
			return 0;

		afe_close_done[port_id & 0x1] = true;
	}

	port_id = q6audio_convert_virtual_to_portid(port_id);
	index = q6audio_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		return -EINVAL;
	}
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_warn("%s: Not a valid port id 0x%x ret %d\n",
			__func__, port_id, ret);
		return -EINVAL;
	}

	mad_type = afe_port_get_mad_type(port_id);
	pr_debug("%s: port_id 0x%x, mad_type %d\n", __func__, port_id,
		 mad_type);
	if (mad_type != MAD_HW_NONE && mad_type != MAD_SW_AUDIO) {
		pr_debug("%s: Turn off MAD\n", __func__);
		ret = afe_turn_onoff_hw_mad(mad_type, false);
		if (ret) {
			pr_err("%s: afe_turn_onoff_hw_mad failed %d\n",
			       __func__, ret);
			return ret;
		}
	} else {
		pr_debug("%s: Not a MAD port\n", __func__);
	}

	port_index = afe_get_port_index(port_id);
	if ((port_index >= 0) && (port_index < AFE_MAX_PORTS)) {
		this_afe.afe_sample_rates[port_index] = 0;
		this_afe.topology[port_index] = 0;
		this_afe.dev_acdb_id[port_index] = 0;
	} else {
		pr_err("%s: port %d\n", __func__, port_index);
		ret = -EINVAL;
		goto fail_cmd;
	}

	if ((port_id == this_afe.aanc_info.aanc_tx_port) &&
	    (this_afe.aanc_info.aanc_active)) {
		memset(&this_afe.aanc_info, 0x00, sizeof(this_afe.aanc_info));
		ret = afe_aanc_mod_enable(this_afe.apr, port_id, 0);
		if (ret)
			pr_err("%s: AFE mod disable failed %d\n",
				__func__, ret);
	}

	/*
	 * even if ramp down configuration failed it is not serious enough to
	 * warrant bailaing out.
	 */
	if (afe_spk_ramp_dn_cfg(port_id) < 0)
		pr_err("%s: ramp down configuration failed\n", __func__);

	stop.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	stop.hdr.pkt_size = sizeof(stop);
	stop.hdr.src_port = 0;
	stop.hdr.dest_port = 0;
	stop.hdr.token = index;
	stop.hdr.opcode = AFE_PORT_CMD_DEVICE_STOP;
	stop.port_id = q6audio_get_port_id(port_id);
	stop.reserved = 0;

	ret = afe_apr_send_pkt(&stop, &this_afe.wait[index]);
	if (ret)
		pr_err("%s: AFE close failed %d\n", __func__, ret);

#if CONFIG_MSM_CSPL
    crus_afe_port_close(port_id);
#endif

fail_cmd:
	return ret;
}
EXPORT_SYMBOL(afe_close);

int afe_set_digital_codec_core_clock(u16 port_id,
				struct afe_digital_clk_cfg *cfg)
{
	struct afe_digital_clk_cfg clk_cfg;
	struct param_hdr_v3 param_hdr;
	int ret = 0;

	if (!cfg) {
		pr_err("%s: clock cfg is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&clk_cfg, 0, sizeof(clk_cfg));
	memset(&param_hdr, 0, sizeof(param_hdr));

	/*default rx port is taken to enable the codec digital clock*/
	param_hdr.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_INTERNAL_DIGIATL_CDC_CLK_CONFIG;
	param_hdr.param_size = sizeof(struct afe_digital_clk_cfg);
	clk_cfg = *cfg;

	pr_debug("%s: Minor version =0x%x clk val = %d\n"
		 "clk root = 0x%x resrv = 0x%x\n",
		 __func__, cfg->i2s_cfg_minor_version, cfg->clk_val,
		 cfg->clk_root, cfg->reserved);

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) &clk_cfg);
	if (ret < 0)
		pr_err("%s: AFE enable for port 0x%x ret %d\n", __func__,
		       port_id, ret);
	return ret;
}

/**
 * afe_set_lpass_clock - Enable AFE lpass clock
 *
 * @port_id: AFE port id
 * @cfg: pointer to clk set struct
 *
 * Returns 0 on success, appropriate error code otherwise
 */
int afe_set_lpass_clock(u16 port_id, struct afe_clk_cfg *cfg)
{
	struct afe_clk_cfg clk_cfg;
	struct param_hdr_v3 param_hdr;
	int ret = 0;

	if (!cfg) {
		pr_err("%s: clock cfg is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&clk_cfg, 0, sizeof(clk_cfg));
	memset(&param_hdr, 0, sizeof(param_hdr));

	ret = q6audio_is_digital_pcm_interface(port_id);
	if (ret < 0) {
		pr_err("%s: q6audio_is_digital_pcm_interface fail %d\n",
			__func__, ret);
		return -EINVAL;
	}

	mutex_lock(&this_afe.afe_cmd_lock);
	param_hdr.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_LPAIF_CLK_CONFIG;
	param_hdr.param_size = sizeof(clk_cfg);
	clk_cfg = *cfg;

	pr_debug("%s: Minor version =0x%x clk val1 = %d\n"
		 "clk val2 = %d, clk src = 0x%x\n"
		 "clk root = 0x%x clk mode = 0x%x resrv = 0x%x\n"
		 "port id = 0x%x\n",
		 __func__, cfg->i2s_cfg_minor_version,
		 cfg->clk_val1, cfg->clk_val2, cfg->clk_src,
		 cfg->clk_root, cfg->clk_set_mode,
		 cfg->reserved, q6audio_get_port_id(port_id));

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) &clk_cfg);
	if (ret < 0)
		pr_err("%s: AFE enable for port 0x%x ret %d\n",
		       __func__, port_id, ret);

	mutex_unlock(&this_afe.afe_cmd_lock);
	return ret;
}
EXPORT_SYMBOL(afe_set_lpass_clock);

/**
 * afe_set_lpass_clk_cfg - Set AFE clk config
 *
 * @index: port index
 * @cfg: pointer to clk set struct
 *
 * Returns 0 on success, appropriate error code otherwise
 */
int afe_set_lpass_clk_cfg(int index, struct afe_clk_set *cfg)
{
	struct param_hdr_v3 param_hdr;
	int ret = 0;

	if (!cfg) {
		pr_err("%s: clock cfg is NULL\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: index[%d] invalid!\n", __func__, index);
		return -EINVAL;
	}

	memset(&param_hdr, 0, sizeof(param_hdr));

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err_ratelimited("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	mutex_lock(&this_afe.afe_cmd_lock);
	param_hdr.module_id = AFE_MODULE_CLOCK_SET;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_CLOCK_SET;
	param_hdr.param_size = sizeof(struct afe_clk_set);


	pr_debug("%s: Minor version =0x%x clk id = %d\n"
		 "clk freq (Hz) = %d, clk attri = 0x%x\n"
		 "clk root = 0x%x clk enable = 0x%x\n",
		 __func__, cfg->clk_set_minor_version,
		 cfg->clk_id, cfg->clk_freq_in_hz, cfg->clk_attri,
		 cfg->clk_root, cfg->enable);

	ret = q6afe_svc_pack_and_set_param_in_band(index, param_hdr,
						   (u8 *) cfg);
	if (ret < 0)
		pr_err_ratelimited("%s: AFE clk cfg failed with ret %d\n",
		       __func__, ret);

	mutex_unlock(&this_afe.afe_cmd_lock);
	return ret;
}
EXPORT_SYMBOL(afe_set_lpass_clk_cfg);

/**
 * afe_set_lpass_clock_v2 - Enable AFE lpass clock
 *
 * @port_id: AFE port id
 * @cfg: pointer to clk set struct
 *
 * Returns 0 on success, appropriate error code otherwise
 */
int afe_set_lpass_clock_v2(u16 port_id, struct afe_clk_set *cfg)
{
	int index = 0;
	int ret = 0;

	index = q6audio_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		return -EINVAL;
	}
	ret = q6audio_is_digital_pcm_interface(port_id);
	if (ret < 0) {
		pr_err("%s: q6audio_is_digital_pcm_interface fail %d\n",
			__func__, ret);
		return -EINVAL;
	}

	ret = afe_set_lpass_clk_cfg(index, cfg);
	if (ret)
		pr_err("%s: afe_set_lpass_clk_cfg_v2 failed %d\n",
			__func__, ret);

	return ret;
}
EXPORT_SYMBOL(afe_set_lpass_clock_v2);

int afe_set_lpass_internal_digital_codec_clock(u16 port_id,
			struct afe_digital_clk_cfg *cfg)
{
	struct afe_digital_clk_cfg clk_cfg;
	struct param_hdr_v3 param_hdr;
	int ret = 0;

	if (!cfg) {
		pr_err("%s: clock cfg is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&clk_cfg, 0, sizeof(clk_cfg));
	memset(&param_hdr, 0, sizeof(param_hdr));

	ret = q6audio_is_digital_pcm_interface(port_id);
	if (ret < 0) {
		pr_err("%s: q6audio_is_digital_pcm_interface fail %d\n",
			__func__, ret);
		return -EINVAL;
	}

	param_hdr.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_INTERNAL_DIGIATL_CDC_CLK_CONFIG;
	param_hdr.param_size = sizeof(clk_cfg);
	clk_cfg = *cfg;

	pr_debug("%s: Minor version =0x%x clk val = %d\n"
		 "clk root = 0x%x resrv = 0x%x port id = 0x%x\n",
		 __func__, cfg->i2s_cfg_minor_version,
		 cfg->clk_val, cfg->clk_root, cfg->reserved,
		 q6audio_get_port_id(port_id));

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) &clk_cfg);
	if (ret < 0)
		pr_err("%s: AFE enable for port 0x0x%x ret %d\n",
		       __func__, port_id, ret);

	return ret;
}

int afe_enable_lpass_core_shared_clock(u16 port_id, u32 enable)
{
	struct afe_param_id_lpass_core_shared_clk_cfg clk_cfg;
	struct param_hdr_v3 param_hdr;
	int ret = 0;

	memset(&clk_cfg, 0, sizeof(clk_cfg));
	memset(&param_hdr, 0, sizeof(param_hdr));

	ret = q6audio_is_digital_pcm_interface(port_id);
	if (ret < 0) {
		pr_err("%s: q6audio_is_digital_pcm_interface fail %d\n",
		       __func__, ret);
		return -EINVAL;
	}

	mutex_lock(&this_afe.afe_cmd_lock);
	param_hdr.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_LPASS_CORE_SHARED_CLOCK_CONFIG;
	param_hdr.param_size = sizeof(clk_cfg);
	clk_cfg.lpass_core_shared_clk_cfg_minor_version =
		AFE_API_VERSION_LPASS_CORE_SHARED_CLK_CONFIG;
	clk_cfg.enable = enable;

	pr_debug("%s: port id = %d, enable = %d\n",
		 __func__, q6audio_get_port_id(port_id), enable);

	ret = q6afe_pack_and_set_param_in_band(port_id,
					       q6audio_get_port_index(port_id),
					       param_hdr, (u8 *) &clk_cfg);
	if (ret < 0)
		pr_err("%s: AFE enable for port 0x%x ret %d\n",
		       __func__, port_id, ret);

	mutex_unlock(&this_afe.afe_cmd_lock);
	return ret;
}

int q6afe_check_osr_clk_freq(u32 freq)
{
	int ret = 0;

	switch (freq) {
	case Q6AFE_LPASS_OSR_CLK_12_P288_MHZ:
	case Q6AFE_LPASS_OSR_CLK_9_P600_MHZ:
	case Q6AFE_LPASS_OSR_CLK_8_P192_MHZ:
	case Q6AFE_LPASS_OSR_CLK_6_P144_MHZ:
	case Q6AFE_LPASS_OSR_CLK_4_P096_MHZ:
	case Q6AFE_LPASS_OSR_CLK_3_P072_MHZ:
	case Q6AFE_LPASS_OSR_CLK_2_P048_MHZ:
	case Q6AFE_LPASS_OSR_CLK_1_P536_MHZ:
	case Q6AFE_LPASS_OSR_CLK_1_P024_MHZ:
	case Q6AFE_LPASS_OSR_CLK_768_kHZ:
	case Q6AFE_LPASS_OSR_CLK_512_kHZ:
		break;
	default:
		pr_err("%s: default freq 0x%x\n",
			__func__, freq);
		ret = -EINVAL;
	}
	return ret;
}

static int afe_get_sp_th_vi_v_vali_data(
		struct afe_sp_th_vi_v_vali_get_param *th_vi_v_vali)
{
	struct param_hdr_v3 param_hdr;
	int port = SLIMBUS_4_TX;
	int ret = -EINVAL;

	if (!th_vi_v_vali) {
		pr_err("%s: Invalid params\n", __func__);
		goto done;
	}
	if (this_afe.vi_tx_port != -1)
		port = this_afe.vi_tx_port;

	memset(&param_hdr, 0, sizeof(param_hdr));

	param_hdr.module_id = AFE_MODULE_SPEAKER_PROTECTION_V2_TH_VI;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_SP_V2_TH_VI_V_VALI_PARAMS;
	param_hdr.param_size = sizeof(struct afe_sp_th_vi_v_vali_params);

	ret = q6afe_get_params(port, NULL, &param_hdr);
	if (ret) {
		pr_err("%s: Failed to get TH VI V-Vali data\n", __func__);
		goto done;
	}

	th_vi_v_vali->pdata = param_hdr;
	memcpy(&th_vi_v_vali->param, &this_afe.th_vi_v_vali_resp.param,
		sizeof(this_afe.th_vi_v_vali_resp.param));
	pr_debug("%s:  Vrms %d %d status %d %d\n", __func__,
		 th_vi_v_vali->param.vrms_q24[SP_V2_SPKR_1],
		 th_vi_v_vali->param.vrms_q24[SP_V2_SPKR_2],
		 th_vi_v_vali->param.status[SP_V2_SPKR_1],
		 th_vi_v_vali->param.status[SP_V2_SPKR_2]);
	ret = 0;
done:
	return ret;
}

int afe_get_sp_th_vi_ftm_data(struct afe_sp_th_vi_get_param *th_vi)
{
	struct param_hdr_v3 param_hdr;
	int port = SLIMBUS_4_TX;
	int ret = -EINVAL;

	if (!th_vi) {
		pr_err("%s: Invalid params\n", __func__);
		goto done;
	}
	if (this_afe.vi_tx_port != -1)
		port = this_afe.vi_tx_port;

	memset(&param_hdr, 0, sizeof(param_hdr));

	param_hdr.module_id = AFE_MODULE_SPEAKER_PROTECTION_V2_TH_VI;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_SP_V2_TH_VI_FTM_PARAMS;
	param_hdr.param_size = sizeof(struct afe_sp_th_vi_ftm_params);

	ret = q6afe_get_params(port, NULL, &param_hdr);
	if (ret) {
		pr_err("%s: Failed to get TH VI FTM data\n", __func__);
		goto done;
	}

	th_vi->pdata = param_hdr;
	memcpy(&th_vi->param, &this_afe.th_vi_resp.param,
		sizeof(this_afe.th_vi_resp.param));
	pr_debug("%s: DC resistance %d %d temp %d %d status %d %d\n",
		 __func__, th_vi->param.dc_res_q24[SP_V2_SPKR_1],
		 th_vi->param.dc_res_q24[SP_V2_SPKR_2],
		 th_vi->param.temp_q22[SP_V2_SPKR_1],
		 th_vi->param.temp_q22[SP_V2_SPKR_2],
		 th_vi->param.status[SP_V2_SPKR_1],
		 th_vi->param.status[SP_V2_SPKR_2]);
	ret = 0;
done:
	return ret;
}

int afe_get_sp_ex_vi_ftm_data(struct afe_sp_ex_vi_get_param *ex_vi)
{
	struct param_hdr_v3 param_hdr;
	int port = SLIMBUS_4_TX;
	int ret = -EINVAL;

	if (!ex_vi) {
		pr_err("%s: Invalid params\n", __func__);
		goto done;
	}
	if (this_afe.vi_tx_port != -1)
		port = this_afe.vi_tx_port;

	memset(&param_hdr, 0, sizeof(param_hdr));

	param_hdr.module_id = AFE_MODULE_SPEAKER_PROTECTION_V2_EX_VI;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_SP_V2_EX_VI_FTM_PARAMS;
	param_hdr.param_size = sizeof(struct afe_sp_ex_vi_ftm_params);

	ret = q6afe_get_params(port, NULL, &param_hdr);
	if (ret < 0) {
		pr_err("%s: get param port 0x%x param id[0x%x]failed %d\n",
		       __func__, port, param_hdr.param_id, ret);
		goto done;
	}

	ex_vi->pdata = param_hdr;
	memcpy(&ex_vi->param, &this_afe.ex_vi_resp.param,
		sizeof(this_afe.ex_vi_resp.param));
	pr_debug("%s: freq %d %d resistance %d %d qfactor %d %d state %d %d\n",
		 __func__, ex_vi->param.freq_q20[SP_V2_SPKR_1],
		 ex_vi->param.freq_q20[SP_V2_SPKR_2],
		 ex_vi->param.resis_q24[SP_V2_SPKR_1],
		 ex_vi->param.resis_q24[SP_V2_SPKR_2],
		 ex_vi->param.qmct_q24[SP_V2_SPKR_1],
		 ex_vi->param.qmct_q24[SP_V2_SPKR_2],
		 ex_vi->param.status[SP_V2_SPKR_1],
		 ex_vi->param.status[SP_V2_SPKR_2]);
	ret = 0;
done:
	return ret;
}

/**
 * afe_get_sp_rx_tmax_xmax_logging_data -
 *       command to get excursion logging data from DSP
 *
 * @xt_logging: excursion logging params
 * @port: AFE port ID
 *
 * Returns 0 on success or error on failure
 */
int afe_get_sp_rx_tmax_xmax_logging_data(
			struct afe_sp_rx_tmax_xmax_logging_param *xt_logging,
			u16 port_id)
{
	struct param_hdr_v3 param_hdr;
	int ret = -EINVAL;

	if (!xt_logging) {
		pr_err("%s: Invalid params\n", __func__);
		goto done;
	}

	memset(&param_hdr, 0, sizeof(param_hdr));

	param_hdr.module_id = AFE_MODULE_FB_SPKR_PROT_V2_RX;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_SP_RX_TMAX_XMAX_LOGGING;
	param_hdr.param_size = sizeof(struct afe_sp_rx_tmax_xmax_logging_param);

	ret = q6afe_get_params(port_id, NULL, &param_hdr);
	if (ret < 0) {
		pr_err("%s: get param port 0x%x param id[0x%x]failed %d\n",
		       __func__, port_id, param_hdr.param_id, ret);
		goto done;
	}

	memcpy(xt_logging, &this_afe.xt_logging_resp.param,
		sizeof(this_afe.xt_logging_resp.param));
	pr_debug("%s: max_excursion %d %d count_exceeded_excursion %d %d max_temperature %d %d count_exceeded_temperature %d %d\n",
		 __func__, xt_logging->max_excursion[SP_V2_SPKR_1],
		 xt_logging->max_excursion[SP_V2_SPKR_2],
		 xt_logging->count_exceeded_excursion[SP_V2_SPKR_1],
		 xt_logging->count_exceeded_excursion[SP_V2_SPKR_2],
		 xt_logging->max_temperature[SP_V2_SPKR_1],
		 xt_logging->max_temperature[SP_V2_SPKR_2],
		 xt_logging->count_exceeded_temperature[SP_V2_SPKR_1],
		 xt_logging->count_exceeded_temperature[SP_V2_SPKR_2]);
	ret = 0;
done:
	return ret;
}
EXPORT_SYMBOL(afe_get_sp_rx_tmax_xmax_logging_data);

/**
 * afe_get_av_dev_drift -
 *       command to retrieve AV drift
 *
 * @timing_stats: timing stats to be updated with AV drift values
 * @port: AFE port ID
 *
 * Returns 0 on success or error on failure
 */
int afe_get_av_dev_drift(struct afe_param_id_dev_timing_stats *timing_stats,
			 u16 port)
{
	struct param_hdr_v3 param_hdr;
	int ret = -EINVAL;

	if (!timing_stats) {
		pr_err("%s: Invalid params\n", __func__);
		goto exit;
	}

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_DEV_TIMING_STATS;
	param_hdr.param_size = sizeof(struct afe_param_id_dev_timing_stats);

	ret = q6afe_get_params(port, NULL, &param_hdr);
	if (ret < 0) {
		pr_err("%s: get param port 0x%x param id[0x%x] failed %d\n",
		       __func__, port, param_hdr.param_id, ret);
		goto exit;
	}

	memcpy(timing_stats, &this_afe.av_dev_drift_resp.timing_stats,
	       param_hdr.param_size);
	ret = 0;
exit:
	return ret;
}
EXPORT_SYMBOL(afe_get_av_dev_drift);

int afe_spk_prot_get_calib_data(struct afe_spkr_prot_get_vi_calib *calib_resp)
{
	struct param_hdr_v3 param_hdr;
	int port = SLIMBUS_4_TX;
	int ret = -EINVAL;

	if (!calib_resp) {
		pr_err("%s: Invalid params\n", __func__);
		goto fail_cmd;
	}
	if (this_afe.vi_tx_port != -1)
		port = this_afe.vi_tx_port;

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = AFE_MODULE_FB_SPKR_PROT_VI_PROC_V2;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = AFE_PARAM_ID_CALIB_RES_CFG_V2;
	param_hdr.param_size = sizeof(struct afe_spkr_prot_get_vi_calib);

	ret = q6afe_get_params(port, NULL, &param_hdr);
	if (ret < 0) {
		pr_err("%s: get param port 0x%x param id[0x%x]failed %d\n",
		       __func__, port, param_hdr.param_id, ret);
		goto fail_cmd;
	}
	memcpy(&calib_resp->res_cfg, &this_afe.calib_data.res_cfg,
		sizeof(this_afe.calib_data.res_cfg));
	pr_info("%s: state %s resistance %d %d\n", __func__,
		fbsp_state[calib_resp->res_cfg.th_vi_ca_state],
		calib_resp->res_cfg.r0_cali_q24[SP_V2_SPKR_1],
		calib_resp->res_cfg.r0_cali_q24[SP_V2_SPKR_2]);
	ret = 0;
fail_cmd:
	return ret;
}

/**
 * afe_spk_prot_feed_back_cfg -
 *       command to setup spk protection feedback config
 *
 * @src_port: source port id
 * @dst_port: destination port id
 * @l_ch: left speaker active or not
 * @r_ch: right speaker active or not
 * @enable: flag to enable or disable
 *
 * Returns 0 on success or error on failure
 */
int afe_spk_prot_feed_back_cfg(int src_port, int dst_port,
	int l_ch, int r_ch, u32 enable)
{
	int ret = -EINVAL;
	union afe_spkr_prot_config prot_config;
	int index = 0;

	if (!enable) {
		pr_debug("%s: Disable Feedback tx path", __func__);
		this_afe.vi_tx_port = -1;
		this_afe.vi_rx_port = -1;
		return 0;
	}

	if ((q6audio_validate_port(src_port) < 0) ||
		(q6audio_validate_port(dst_port) < 0)) {
		pr_err("%s: invalid ports src 0x%x dst 0x%x",
			__func__, src_port, dst_port);
		goto fail_cmd;
	}
	if (!l_ch && !r_ch) {
		pr_err("%s: error ch values zero\n", __func__);
		goto fail_cmd;
	}
	pr_debug("%s: src_port 0x%x  dst_port 0x%x l_ch %d r_ch %d\n",
		 __func__, src_port, dst_port, l_ch, r_ch);
	memset(&prot_config, 0, sizeof(prot_config));
	prot_config.feedback_path_cfg.dst_portid =
		q6audio_get_port_id(dst_port);
	if (l_ch) {
		prot_config.feedback_path_cfg.chan_info[index++] = 1;
		prot_config.feedback_path_cfg.chan_info[index++] = 2;
	}
	if (r_ch) {
		prot_config.feedback_path_cfg.chan_info[index++] = 3;
		prot_config.feedback_path_cfg.chan_info[index++] = 4;
	}
	prot_config.feedback_path_cfg.num_channels = index;
	pr_debug("%s no of channels: %d\n", __func__, index);
	prot_config.feedback_path_cfg.minor_version = 1;
	ret = afe_spk_prot_prepare(src_port, dst_port,
			AFE_PARAM_ID_FEEDBACK_PATH_CFG, &prot_config);
fail_cmd:
	return ret;
}
EXPORT_SYMBOL(afe_spk_prot_feed_back_cfg);

static int get_cal_type_index(int32_t cal_type)
{
	int ret = -EINVAL;

	switch (cal_type) {
	case AFE_COMMON_RX_CAL_TYPE:
		ret = AFE_COMMON_RX_CAL;
		break;
	case AFE_COMMON_TX_CAL_TYPE:
		ret = AFE_COMMON_TX_CAL;
		break;
	case AFE_LSM_TX_CAL_TYPE:
		ret = AFE_LSM_TX_CAL;
		break;
	case AFE_AANC_CAL_TYPE:
		ret = AFE_AANC_CAL;
		break;
	case AFE_HW_DELAY_CAL_TYPE:
		ret = AFE_HW_DELAY_CAL;
		break;
	case AFE_FB_SPKR_PROT_CAL_TYPE:
		ret = AFE_FB_SPKR_PROT_CAL;
		break;
	case AFE_SIDETONE_CAL_TYPE:
		ret = AFE_SIDETONE_CAL;
		break;
	case AFE_SIDETONE_IIR_CAL_TYPE:
		ret = AFE_SIDETONE_IIR_CAL;
		break;
	case AFE_TOPOLOGY_CAL_TYPE:
		ret = AFE_TOPOLOGY_CAL;
		break;
	case AFE_LSM_TOPOLOGY_CAL_TYPE:
		ret = AFE_LSM_TOPOLOGY_CAL;
		break;
	case AFE_CUST_TOPOLOGY_CAL_TYPE:
		ret = AFE_CUST_TOPOLOGY_CAL;
		break;
	default:
		pr_err("%s: invalid cal type %d!\n", __func__, cal_type);
	}
	return ret;
}

int afe_alloc_cal(int32_t cal_type, size_t data_size,
						void *data)
{
	int				ret = 0;
	int				cal_index;

	cal_index = get_cal_type_index(cal_type);
	pr_debug("%s: cal_type = %d cal_index = %d\n",
		  __func__, cal_type, cal_index);

	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	mutex_lock(&this_afe.afe_cmd_lock);
	ret = cal_utils_alloc_cal(data_size, data,
		this_afe.cal_data[cal_index], 0, NULL);
	if (ret < 0) {
		pr_err("%s: cal_utils_alloc_block failed, ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
		ret = -EINVAL;
		mutex_unlock(&this_afe.afe_cmd_lock);
		goto done;
	}
	mutex_unlock(&this_afe.afe_cmd_lock);
done:
	return ret;
}

static int afe_dealloc_cal(int32_t cal_type, size_t data_size,
							void *data)
{
	int ret = 0;
	int cal_index;

	pr_debug("%s:\n", __func__);

	cal_index = get_cal_type_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	ret = cal_utils_dealloc_cal(data_size, data,
		this_afe.cal_data[cal_index]);
	if (ret < 0) {
		pr_err("%s: cal_utils_dealloc_block failed, ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
		ret = -EINVAL;
		goto done;
	}
done:
	return ret;
}

static int afe_set_cal(int32_t cal_type, size_t data_size,
						void *data)
{
	int ret = 0;
	int cal_index;

	pr_debug("%s:\n", __func__);

	cal_index = get_cal_type_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	ret = cal_utils_set_cal(data_size, data,
		this_afe.cal_data[cal_index], 0, NULL);
	if (ret < 0) {
		pr_err("%s: cal_utils_set_cal failed, ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
		ret = -EINVAL;
		goto done;
	}

	if (cal_index == AFE_CUST_TOPOLOGY_CAL) {
		mutex_lock(&this_afe.cal_data[AFE_CUST_TOPOLOGY_CAL]->lock);
		this_afe.set_custom_topology = 1;
		pr_debug("%s:[AFE_CUSTOM_TOPOLOGY] ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
		mutex_unlock(&this_afe.cal_data[AFE_CUST_TOPOLOGY_CAL]->lock);
	}

done:
	return ret;
}

static struct cal_block_data *afe_find_hw_delay_by_path(
			struct cal_type_data *cal_type, int path)
{
	struct list_head *ptr, *next;
	struct cal_block_data *cal_block = NULL;

	pr_debug("%s:\n", __func__);

	list_for_each_safe(ptr, next,
		&cal_type->cal_blocks) {

		cal_block = list_entry(ptr,
			struct cal_block_data, list);

		if (cal_utils_is_cal_stale(cal_block))
			continue;

		if (((struct audio_cal_info_hw_delay *)cal_block->cal_info)
			->path == path) {
			return cal_block;
		}
	}
	return NULL;
}

static int afe_get_cal_hw_delay(int32_t path,
				struct audio_cal_hw_delay_entry *entry)
{
	int ret = 0;
	int i;
	struct cal_block_data		*cal_block = NULL;
	struct audio_cal_hw_delay_data	*hw_delay_info = NULL;

	pr_debug("%s:\n", __func__);

	if (this_afe.cal_data[AFE_HW_DELAY_CAL] == NULL) {
		pr_err("%s: AFE_HW_DELAY_CAL not initialized\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	if (entry == NULL) {
		pr_err("%s: entry is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	if ((path >= MAX_PATH_TYPE) || (path < 0)) {
		pr_err("%s: bad path: %d\n",
		       __func__, path);
		ret = -EINVAL;
		goto done;
	}

	mutex_lock(&this_afe.cal_data[AFE_HW_DELAY_CAL]->lock);
	cal_block = afe_find_hw_delay_by_path(
		this_afe.cal_data[AFE_HW_DELAY_CAL], path);
	if (cal_block == NULL)
		goto unlock;

	hw_delay_info = &((struct audio_cal_info_hw_delay *)
		cal_block->cal_info)->data;
	if (hw_delay_info->num_entries > MAX_HW_DELAY_ENTRIES) {
		pr_err("%s: invalid num entries: %d\n",
		       __func__, hw_delay_info->num_entries);
		ret = -EINVAL;
		goto unlock;
	}

	for (i = 0; i < hw_delay_info->num_entries; i++) {
		if (hw_delay_info->entry[i].sample_rate ==
			entry->sample_rate) {
			entry->delay_usec = hw_delay_info->entry[i].delay_usec;
			break;
		}
	}
	if (i == hw_delay_info->num_entries) {
		pr_err("%s: Unable to find delay for sample rate %d\n",
		       __func__, entry->sample_rate);
		ret = -EFAULT;
		goto unlock;
	}

	cal_utils_mark_cal_used(cal_block);
	pr_debug("%s: Path = %d samplerate = %u usec = %u status %d\n",
		 __func__, path, entry->sample_rate, entry->delay_usec, ret);
unlock:
	mutex_unlock(&this_afe.cal_data[AFE_HW_DELAY_CAL]->lock);
done:
	return ret;
}

static int afe_set_cal_sp_th_vi_v_vali_cfg(int32_t cal_type, size_t data_size,
					void *data)
{
	int ret = 0;
	struct audio_cal_type_sp_th_vi_v_vali_cfg *cal_data = data;

	if (cal_data == NULL || data_size != sizeof(*cal_data))
		goto done;

	memcpy(&this_afe.v_vali_cfg, &cal_data->cal_info,
		sizeof(this_afe.v_vali_cfg));
done:
	return ret;
}

static int afe_set_cal_sp_th_vi_ftm_cfg(int32_t cal_type, size_t data_size,
					void *data)
{
	int ret = 0;
	struct audio_cal_type_sp_th_vi_ftm_cfg *cal_data = data;

	if (cal_data == NULL || data_size != sizeof(*cal_data))
		goto done;

	memcpy(&this_afe.th_ftm_cfg, &cal_data->cal_info,
		sizeof(this_afe.th_ftm_cfg));
done:
	return ret;
}

static int afe_set_cal_sp_th_vi_cfg(int32_t cal_type, size_t data_size,
				    void *data)
{
	int ret = 0;
	struct audio_cal_type_sp_th_vi_ftm_cfg *cal_data = data;
	uint32_t mode;

	if (cal_data == NULL ||
	    this_afe.cal_data[AFE_FB_SPKR_PROT_TH_VI_CAL] == NULL)
		goto done;

	mutex_lock(&this_afe.cal_data[AFE_FB_SPKR_PROT_TH_VI_CAL]->lock);
	mode = cal_data->cal_info.mode;
	pr_debug("%s: cal_type = %d, mode = %d\n", __func__, cal_type, mode);
	if (mode == MSM_SPKR_PROT_IN_FTM_MODE) {
		ret = afe_set_cal_sp_th_vi_ftm_cfg(cal_type,
						data_size, data);
	} else if (mode == MSM_SPKR_PROT_IN_V_VALI_MODE) {
		ret = afe_set_cal_sp_th_vi_v_vali_cfg(cal_type,
						data_size, data);
	}
	mutex_unlock(&this_afe.cal_data[AFE_FB_SPKR_PROT_TH_VI_CAL]->lock);
done:
	return ret;
}

static int afe_set_cal_sp_ex_vi_ftm_cfg(int32_t cal_type, size_t data_size,
					void *data)
{
	int ret = 0;
	struct audio_cal_type_sp_ex_vi_ftm_cfg *cal_data = data;

	if (this_afe.cal_data[AFE_FB_SPKR_PROT_EX_VI_CAL] == NULL ||
	    cal_data == NULL ||
	    data_size != sizeof(*cal_data))
		goto done;

	pr_debug("%s: cal_type = %d\n", __func__, cal_type);
	mutex_lock(&this_afe.cal_data[AFE_FB_SPKR_PROT_EX_VI_CAL]->lock);
	memcpy(&this_afe.ex_ftm_cfg, &cal_data->cal_info,
		sizeof(this_afe.ex_ftm_cfg));
	mutex_unlock(&this_afe.cal_data[AFE_FB_SPKR_PROT_EX_VI_CAL]->lock);
done:
	return ret;
}

static int afe_set_cal_fb_spkr_prot(int32_t cal_type, size_t data_size,
								void *data)
{
	int ret = 0;
	struct audio_cal_type_fb_spk_prot_cfg	*cal_data = data;

	pr_debug("%s:\n", __func__);

	if (this_afe.cal_data[AFE_FB_SPKR_PROT_CAL] == NULL)
		goto done;
	if (cal_data == NULL)
		goto done;
	if (data_size != sizeof(*cal_data))
		goto done;

	if (cal_data->cal_info.mode == MSM_SPKR_PROT_CALIBRATION_IN_PROGRESS)
		__pm_wakeup_event(&wl.ws, jiffies_to_msecs(WAKELOCK_TIMEOUT));
	mutex_lock(&this_afe.cal_data[AFE_FB_SPKR_PROT_CAL]->lock);
	memcpy(&this_afe.prot_cfg, &cal_data->cal_info,
		sizeof(this_afe.prot_cfg));
	mutex_unlock(&this_afe.cal_data[AFE_FB_SPKR_PROT_CAL]->lock);
done:
	return ret;
}

static int afe_get_cal_sp_th_vi_v_vali_param(int32_t cal_type, size_t data_size,
					  void *data)
{
	int i, ret = 0;
	struct audio_cal_type_sp_th_vi_v_vali_param *cal_data = data;
	struct afe_sp_th_vi_v_vali_get_param th_vi_v_vali;

	if (this_afe.cal_data[AFE_FB_SPKR_PROT_TH_VI_CAL] == NULL ||
	    cal_data == NULL ||
	    data_size != sizeof(*cal_data))
		goto done;

	for (i = 0; i < SP_V2_NUM_MAX_SPKRS; i++) {
		cal_data->cal_info.status[i] = -EINVAL;
		cal_data->cal_info.vrms_q24[i] = -1;
	}
	if (!afe_get_sp_th_vi_v_vali_data(&th_vi_v_vali)) {
		for (i = 0; i < SP_V2_NUM_MAX_SPKRS; i++) {
			pr_debug("%s: v-vali param status = %d\n",
				  __func__, th_vi_v_vali.param.status[i]);
			if (th_vi_v_vali.param.status[i] ==
					V_VALI_IN_PROGRESS) {
				cal_data->cal_info.status[i] = -EAGAIN;
			} else if (th_vi_v_vali.param.status[i] ==
					V_VALI_SUCCESS) {
				cal_data->cal_info.status[i] = V_VALI_SUCCESS;
				cal_data->cal_info.vrms_q24[i] =
					th_vi_v_vali.param.vrms_q24[i];
			}
		}
	}
	this_afe.v_vali_flag = 0;
done:
	return ret;
}

static int afe_get_cal_sp_th_vi_ftm_param(int32_t cal_type, size_t data_size,
					  void *data)
{
	int i, ret = 0;
	struct audio_cal_type_sp_th_vi_param *cal_data = data;
	struct afe_sp_th_vi_get_param th_vi;

	if (this_afe.cal_data[AFE_FB_SPKR_PROT_TH_VI_CAL] == NULL ||
	    cal_data == NULL ||
	    data_size != sizeof(*cal_data))
		goto done;

	for (i = 0; i < SP_V2_NUM_MAX_SPKRS; i++) {
		cal_data->cal_info.status[i] = -EINVAL;
		cal_data->cal_info.r_dc_q24[i] = -1;
		cal_data->cal_info.temp_q22[i] = -1;
	}
	if (!afe_get_sp_th_vi_ftm_data(&th_vi)) {
		for (i = 0; i < SP_V2_NUM_MAX_SPKRS; i++) {
			pr_debug("%s: ftm param status = %d\n",
				  __func__, th_vi.param.status[i]);
			if (th_vi.param.status[i] == FBSP_IN_PROGRESS) {
				cal_data->cal_info.status[i] = -EAGAIN;
			} else if (th_vi.param.status[i] == FBSP_SUCCESS) {
				cal_data->cal_info.status[i] = 0;
				cal_data->cal_info.r_dc_q24[i] =
					th_vi.param.dc_res_q24[i];
				cal_data->cal_info.temp_q22[i] =
					th_vi.param.temp_q22[i];
			}
		}
	}
done:
	return ret;
}

static int afe_get_cal_sp_th_vi_param(int32_t cal_type, size_t data_size,
				      void *data)
{
	struct audio_cal_type_sp_th_vi_param *cal_data = data;
	uint32_t mode;
	int ret = 0;

	if (cal_data == NULL ||
	    this_afe.cal_data[AFE_FB_SPKR_PROT_TH_VI_CAL] == NULL)
		return 0;

	mutex_lock(&this_afe.cal_data[AFE_FB_SPKR_PROT_TH_VI_CAL]->lock);
	mode = cal_data->cal_info.mode;
	pr_debug("%s: cal_type = %d,mode = %d\n", __func__, cal_type, mode);
	if (mode == MSM_SPKR_PROT_IN_V_VALI_MODE)
		ret = afe_get_cal_sp_th_vi_v_vali_param(cal_type,
						data_size, data);
	else
		ret = afe_get_cal_sp_th_vi_ftm_param(cal_type,
						data_size, data);
	mutex_unlock(&this_afe.cal_data[AFE_FB_SPKR_PROT_TH_VI_CAL]->lock);
	return ret;
}

static int afe_get_cal_sp_ex_vi_ftm_param(int32_t cal_type, size_t data_size,
					  void *data)
{
	int i, ret = 0;
	struct audio_cal_type_sp_ex_vi_param *cal_data = data;
	struct afe_sp_ex_vi_get_param ex_vi;

	pr_debug("%s: cal_type = %d\n", __func__, cal_type);
	if (this_afe.cal_data[AFE_FB_SPKR_PROT_EX_VI_CAL] == NULL ||
	    cal_data == NULL ||
	    data_size != sizeof(*cal_data))
		goto done;

	mutex_lock(&this_afe.cal_data[AFE_FB_SPKR_PROT_EX_VI_CAL]->lock);
	for (i = 0; i < SP_V2_NUM_MAX_SPKRS; i++) {
		cal_data->cal_info.status[i] = -EINVAL;
		cal_data->cal_info.freq_q20[i] = -1;
		cal_data->cal_info.resis_q24[i] = -1;
		cal_data->cal_info.qmct_q24[i] = -1;
	}
	if (!afe_get_sp_ex_vi_ftm_data(&ex_vi)) {
		for (i = 0; i < SP_V2_NUM_MAX_SPKRS; i++) {
			pr_debug("%s: ftm param status = %d\n",
				  __func__, ex_vi.param.status[i]);
			if (ex_vi.param.status[i] == FBSP_IN_PROGRESS) {
				cal_data->cal_info.status[i] = -EAGAIN;
			} else if (ex_vi.param.status[i] == FBSP_SUCCESS) {
				cal_data->cal_info.status[i] = 0;
				cal_data->cal_info.freq_q20[i] =
					ex_vi.param.freq_q20[i];
				cal_data->cal_info.resis_q24[i] =
					ex_vi.param.resis_q24[i];
				cal_data->cal_info.qmct_q24[i] =
					ex_vi.param.qmct_q24[i];
			}
		}
	}
	mutex_unlock(&this_afe.cal_data[AFE_FB_SPKR_PROT_EX_VI_CAL]->lock);
done:
	return ret;
}

static int afe_get_cal_fb_spkr_prot(int32_t cal_type, size_t data_size,
								void *data)
{
	int ret = 0;
	struct audio_cal_type_fb_spk_prot_status *cal_data = data;
	struct afe_spkr_prot_get_vi_calib calib_resp;

	pr_debug("%s:\n", __func__);

	if (this_afe.cal_data[AFE_FB_SPKR_PROT_CAL] == NULL)
		goto done;
	if (cal_data == NULL)
		goto done;
	if (data_size != sizeof(*cal_data))
		goto done;

	mutex_lock(&this_afe.cal_data[AFE_FB_SPKR_PROT_CAL]->lock);
	if (this_afe.prot_cfg.mode == MSM_SPKR_PROT_CALIBRATED) {
		cal_data->cal_info.r0[SP_V2_SPKR_1] =
			this_afe.prot_cfg.r0[SP_V2_SPKR_1];
		cal_data->cal_info.r0[SP_V2_SPKR_2] =
			this_afe.prot_cfg.r0[SP_V2_SPKR_2];
		cal_data->cal_info.status = 0;
	} else if (this_afe.prot_cfg.mode ==
			MSM_SPKR_PROT_CALIBRATION_IN_PROGRESS) {
		/*Call AFE to query the status*/
		cal_data->cal_info.status = -EINVAL;
		cal_data->cal_info.r0[SP_V2_SPKR_1] = -1;
		cal_data->cal_info.r0[SP_V2_SPKR_2] = -1;
		if (!afe_spk_prot_get_calib_data(&calib_resp)) {
			if (calib_resp.res_cfg.th_vi_ca_state ==
							FBSP_IN_PROGRESS)
				cal_data->cal_info.status = -EAGAIN;
			else if (calib_resp.res_cfg.th_vi_ca_state ==
							FBSP_SUCCESS) {
				cal_data->cal_info.status = 0;
				cal_data->cal_info.r0[SP_V2_SPKR_1] =
				calib_resp.res_cfg.r0_cali_q24[SP_V2_SPKR_1];
				cal_data->cal_info.r0[SP_V2_SPKR_2] =
				calib_resp.res_cfg.r0_cali_q24[SP_V2_SPKR_2];
			}
		}
		if (!cal_data->cal_info.status) {
			this_afe.prot_cfg.mode =
				MSM_SPKR_PROT_CALIBRATED;
			this_afe.prot_cfg.r0[SP_V2_SPKR_1] =
				cal_data->cal_info.r0[SP_V2_SPKR_1];
			this_afe.prot_cfg.r0[SP_V2_SPKR_2] =
				cal_data->cal_info.r0[SP_V2_SPKR_2];
		}
	} else {
		/*Indicates calibration data is invalid*/
		cal_data->cal_info.status = -EINVAL;
		cal_data->cal_info.r0[SP_V2_SPKR_1] = -1;
		cal_data->cal_info.r0[SP_V2_SPKR_2] = -1;
	}
	this_afe.initial_cal = 0;
	mutex_unlock(&this_afe.cal_data[AFE_FB_SPKR_PROT_CAL]->lock);
	__pm_relax(&wl.ws);
done:
	return ret;
}

static int afe_map_cal_data(int32_t cal_type,
				struct cal_block_data *cal_block)
{
	int ret = 0;
	int cal_index;

	pr_debug("%s:\n", __func__);

	cal_index = get_cal_type_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	atomic_set(&this_afe.mem_map_cal_index, cal_index);
	ret = afe_cmd_memory_map(cal_block->cal_data.paddr,
			cal_block->map_data.map_size);
	atomic_set(&this_afe.mem_map_cal_index, -1);
	if (ret < 0) {
		pr_err("%s: mmap did not work! size = %zd ret %d\n",
			__func__,
			cal_block->map_data.map_size, ret);
		pr_debug("%s: mmap did not work! addr = 0x%pK, size = %zd\n",
			__func__,
			&cal_block->cal_data.paddr,
			cal_block->map_data.map_size);
		goto done;
	}
	cal_block->map_data.q6map_handle = atomic_read(&this_afe.
		mem_map_cal_handles[cal_index]);
done:
	return ret;
}

static int afe_unmap_cal_data(int32_t cal_type,
				struct cal_block_data *cal_block)
{
	int ret = 0;
	int cal_index;

	pr_debug("%s:\n", __func__);

	cal_index = get_cal_type_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	if (cal_block == NULL) {
		pr_err("%s: Cal block is NULL!\n",
						__func__);
		goto done;
	}

	if (cal_block->map_data.q6map_handle == 0) {
		pr_err("%s: Map handle is NULL, nothing to unmap\n",
				__func__);
		goto done;
	}

	atomic_set(&this_afe.mem_map_cal_handles[cal_index],
		cal_block->map_data.q6map_handle);
	atomic_set(&this_afe.mem_map_cal_index, cal_index);
	ret = afe_cmd_memory_unmap_nowait(
		cal_block->map_data.q6map_handle);
	atomic_set(&this_afe.mem_map_cal_index, -1);
	if (ret < 0) {
		pr_err("%s: unmap did not work! cal_type %i ret %d\n",
			__func__, cal_index, ret);
	}
	cal_block->map_data.q6map_handle = 0;
done:
	return ret;
}

static void afe_delete_cal_data(void)
{
	pr_debug("%s:\n", __func__);

	cal_utils_destroy_cal_types(MAX_AFE_CAL_TYPES, this_afe.cal_data);
}

static int afe_init_cal_data(void)
{
	int ret = 0;
	struct cal_type_info	cal_type_info[] = {
		{{AFE_COMMON_RX_CAL_TYPE,
		{afe_alloc_cal, afe_dealloc_cal, NULL,
		afe_set_cal, NULL, NULL} },
		{afe_map_cal_data, afe_unmap_cal_data,
		cal_utils_match_buf_num} },

		{{AFE_COMMON_TX_CAL_TYPE,
		{afe_alloc_cal, afe_dealloc_cal, NULL,
		afe_set_cal, NULL, NULL} },
		{afe_map_cal_data, afe_unmap_cal_data,
		cal_utils_match_buf_num} },

		{{AFE_LSM_TX_CAL_TYPE,
		{afe_alloc_cal, afe_dealloc_cal, NULL,
		afe_set_cal, NULL, NULL} },
		{afe_map_cal_data, afe_unmap_cal_data,
		cal_utils_match_buf_num} },

		{{AFE_AANC_CAL_TYPE,
		{afe_alloc_cal, afe_dealloc_cal, NULL,
		afe_set_cal, NULL, NULL} },
		{afe_map_cal_data, afe_unmap_cal_data,
		cal_utils_match_buf_num} },

		{{AFE_FB_SPKR_PROT_CAL_TYPE,
		{NULL, NULL, NULL, afe_set_cal_fb_spkr_prot,
		afe_get_cal_fb_spkr_prot, NULL} },
		{NULL, NULL, cal_utils_match_buf_num} },

		{{AFE_HW_DELAY_CAL_TYPE,
		{NULL, NULL, NULL,
		afe_set_cal, NULL, NULL} },
		{NULL, NULL, cal_utils_match_buf_num} },

		{{AFE_SIDETONE_CAL_TYPE,
		{NULL, NULL, NULL,
		afe_set_cal, NULL, NULL} },
		{NULL, NULL, cal_utils_match_buf_num} },

		{{AFE_SIDETONE_IIR_CAL_TYPE,
		{NULL, NULL, NULL,
		afe_set_cal, NULL, NULL} },
		{NULL, NULL, cal_utils_match_buf_num} },

		{{AFE_TOPOLOGY_CAL_TYPE,
		{NULL, NULL, NULL,
		afe_set_cal, NULL, NULL} },
		{NULL, NULL,
		cal_utils_match_buf_num} },

		{{AFE_LSM_TOPOLOGY_CAL_TYPE,
		{NULL, NULL, NULL,
		afe_set_cal, NULL, NULL} },
		{NULL, NULL,
		cal_utils_match_buf_num} },

		{{AFE_CUST_TOPOLOGY_CAL_TYPE,
		{afe_alloc_cal, afe_dealloc_cal, NULL,
		afe_set_cal, NULL, NULL} },
		{afe_map_cal_data, afe_unmap_cal_data,
		cal_utils_match_buf_num} },

		{{AFE_FB_SPKR_PROT_TH_VI_CAL_TYPE,
		{NULL, NULL, NULL, afe_set_cal_sp_th_vi_cfg,
		afe_get_cal_sp_th_vi_param, NULL} },
		{NULL, NULL, cal_utils_match_buf_num} },

		{{AFE_FB_SPKR_PROT_EX_VI_CAL_TYPE,
		{NULL, NULL, NULL, afe_set_cal_sp_ex_vi_ftm_cfg,
		afe_get_cal_sp_ex_vi_ftm_param, NULL} },
		{NULL, NULL, cal_utils_match_buf_num} },
	};
	pr_debug("%s:\n", __func__);

	ret = cal_utils_create_cal_types(MAX_AFE_CAL_TYPES, this_afe.cal_data,
		cal_type_info);
	if (ret < 0) {
		pr_err("%s: could not create cal type! %d\n",
			__func__, ret);
		ret = -EINVAL;
		goto err;
	}

	return ret;
err:
	afe_delete_cal_data();
	return ret;
}

int afe_map_rtac_block(struct rtac_cal_block_data *cal_block)
{
	int result = 0;

	pr_debug("%s:\n", __func__);

	if (cal_block == NULL) {
		pr_err("%s: cal_block is NULL!\n",
			__func__);
		result = -EINVAL;
		goto done;
	}

	if (cal_block->cal_data.paddr == 0) {
		pr_debug("%s: No address to map!\n",
			__func__);
		result = -EINVAL;
		goto done;
	}

	if (cal_block->map_data.map_size == 0) {
		pr_debug("%s: map size is 0!\n",
			__func__);
		result = -EINVAL;
		goto done;
	}

	result = afe_cmd_memory_map(cal_block->cal_data.paddr,
		cal_block->map_data.map_size);
	if (result < 0) {
		pr_err("%s: afe_cmd_memory_map failed for addr = 0x%pK, size = %d, err %d\n",
			__func__, &cal_block->cal_data.paddr,
			cal_block->map_data.map_size, result);
		return result;
	}
	cal_block->map_data.map_handle = this_afe.mmap_handle;

done:
	return result;
}

int afe_unmap_rtac_block(uint32_t *mem_map_handle)
{
	int result = 0;

	pr_debug("%s:\n", __func__);

	if (mem_map_handle == NULL) {
		pr_err("%s: Map handle is NULL, nothing to unmap\n",
			__func__);
		goto done;
	}

	if (*mem_map_handle == 0) {
		pr_debug("%s: Map handle is 0, nothing to unmap\n",
			__func__);
		goto done;
	}

	result = afe_cmd_memory_unmap(*mem_map_handle);
	if (result) {
		pr_err("%s: AFE memory unmap failed %d, handle 0x%x\n",
		     __func__, result, *mem_map_handle);
		goto done;
	} else {
		*mem_map_handle = 0;
	}

done:
	return result;
}

static void afe_release_uevent_data(struct kobject *kobj)
{
	struct audio_uevent_data *data = container_of(kobj,
		struct audio_uevent_data, kobj);

	kfree(data);
}

#ifdef CONFIG_SND_SOC_TFA9874_FOR_DAVI

int send_tfa_cal_apr(void *buf, int cmd_size, bool bRead)
{
	int32_t result, port_id = AFE_PORT_ID_TFADSP_RX;
	uint32_t port_index = 0, payload_size = 0;
	size_t len;
	struct rtac_cal_block_data *tfa_cal = &(this_afe.tfa_cal);
	struct mem_mapping_hdr mem_hdr;
	struct param_hdr_v3  param_hdr;

	pr_debug("%s\n", __func__);

	memset(&mem_hdr, 0x00, sizeof(mem_hdr));
	memset(&param_hdr, 0x00, sizeof(param_hdr));

	if (0 == tfa_cal->map_data.dma_buf ) {
		/*Minimal chunk size is 4K*/
		tfa_cal->map_data.map_size = SZ_4K;
		result = msm_audio_ion_alloc(&(tfa_cal->map_data.dma_buf),
								tfa_cal->map_data.map_size,
								&(tfa_cal->cal_data.paddr),
								&len,
								&(tfa_cal->cal_data.kvaddr));
		if (result < 0) {
			pr_err("%s: allocate buffer failed! ret = %d\n",
				__func__, result);
			goto err;
		}
		pr_debug("%s: paddr 0x%pK, kvaddr 0x%pK, map_size 0x%x\n",
				__func__,
				&tfa_cal->cal_data.paddr,
				tfa_cal->cal_data.kvaddr,
				tfa_cal->map_data.map_size);
	}

	if (0 == tfa_cal->map_data.map_handle ) {
		result = afe_map_rtac_block(tfa_cal);
		if (result < 0) {
			pr_err("%s: map buffer failed! ret = %d\n",
				__func__, result);
			goto err;
		}
	}

	port_index = q6audio_get_port_index(port_id);
	if (port_index >= AFE_MAX_PORTS) {
		pr_err("%s: Invalid AFE port = 0x%x\n", __func__, port_id);
		goto err;
	}

	if (cmd_size > (SZ_4K - sizeof(struct param_hdr_v3))) {
		pr_err("%s: Invalid payload size = %d\n", __func__, cmd_size);
		result = -EINVAL;
		goto err;
	}

	/* Pack message header with data */
	param_hdr.module_id = AFE_MODULE_ID_TFADSP_RX;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_size = cmd_size;

	if (!bRead) {
		param_hdr.param_id = AFE_PARAM_ID_TFADSP_RX_CFG;

		q6common_pack_pp_params(tfa_cal->cal_data.kvaddr,
							&param_hdr,
							buf,
							&payload_size);
		tfa_cal->cal_data.size = payload_size;
	}
	else {
		param_hdr.param_id = AFE_PARAM_ID_TFADSP_RX_GET_RESULT;
		tfa_cal->cal_data.size = cmd_size + sizeof(struct param_hdr_v3) ;
	}

	/*Send/Get package to/from ADSP*/
	mem_hdr.data_payload_addr_lsw =
		lower_32_bits(tfa_cal->cal_data.paddr);
	mem_hdr.data_payload_addr_msw =
		msm_audio_populate_upper_32_bits(tfa_cal->cal_data.paddr);
	mem_hdr.mem_map_handle =
		tfa_cal->map_data.map_handle;

	pr_debug("%s: Sending tfa_cal port = 0x%x, cal size = %zd, cal addr = 0x%pK\n",
		__func__, port_id, tfa_cal->cal_data.size, &tfa_cal->cal_data.paddr);

	result = afe_q6_interface_prepare();
	if (result != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, result);
		goto err;
	}

	if (!bRead) {
		if (q6common_is_instance_id_supported())
			result = q6afe_set_params_v3(port_id, port_index, &mem_hdr, NULL, payload_size);
		else
			result = q6afe_set_params_v2(port_id, port_index, &mem_hdr, NULL, payload_size);
	} else {
		int8_t *resp = (int8_t *)tfa_cal->cal_data.kvaddr;

		atomic_set(&this_afe.tfa_state, 1);
		if (q6common_is_instance_id_supported()){
			result = q6afe_get_params_v3(port_id, port_index, &mem_hdr, &param_hdr);
			resp += sizeof(struct param_hdr_v3);
		}
		else {
			result = q6afe_get_params_v2(port_id, port_index, &mem_hdr, &param_hdr);
			resp += sizeof(struct param_hdr_v1);
		}

		if (result) {
			pr_err("%s: get response from port 0x%x failed %d\n", __func__, port_id, result);
			goto err;
		}
		else {
			/*Copy response data to command buffer*/
			memcpy(buf,  resp,  cmd_size);
		}
	}

err:
	return result;
}
EXPORT_SYMBOL(send_tfa_cal_apr);

void send_tfa_cal_unmap_memory(void)
{
	int result = 0;

	if (this_afe.tfa_cal.map_data.map_handle) {
		result = afe_unmap_rtac_block(&this_afe.tfa_cal.map_data.map_handle);

		/*Force to remap after unmap failed*/
		if (result)
			this_afe.tfa_cal.map_data.map_handle = 0;
	}
}
EXPORT_SYMBOL(send_tfa_cal_unmap_memory);

int send_tfa_cal_in_band(void *buf, int cmd_size)
{
	union afe_spkr_prot_config afe_spk_config;
	int32_t port_id = AFE_PORT_ID_TFADSP_RX;

	if (cmd_size > sizeof(afe_spk_config))
		return -EINVAL;

	memcpy(&afe_spk_config, buf, cmd_size);

	if (afe_spk_prot_prepare(port_id, 0,
			AFE_PARAM_ID_TFADSP_RX_CFG,
			&afe_spk_config)) {
			pr_err("%s: AFE_PARAM_ID_TFADSP_RX_CFG failed\n",
				   __func__);
	}

	return 0;
}
EXPORT_SYMBOL(send_tfa_cal_in_band);

int send_tfa_cal_set_bypass(void *buf, int cmd_size)
{
	union afe_spkr_prot_config afe_spk_config;
	int32_t port_id = AFE_PORT_ID_TFADSP_RX;

	if (cmd_size > sizeof(afe_spk_config))
		return -EINVAL;

	memcpy(&afe_spk_config, buf, cmd_size);

	if (afe_spk_prot_prepare(port_id, 0,
			AFE_PARAM_ID_TFADSP_RX_SET_BYPASS,
			&afe_spk_config)) {
		pr_err("%s: AFE_PARAM_ID_TFADSP_RX_SET_BYPASS failed\n",
				   __func__);
	}

	return 0;
}
EXPORT_SYMBOL(send_tfa_cal_set_bypass);

int send_tfa_cal_set_tx_enable(void *buf, int cmd_size)
{
	union afe_spkr_prot_config afe_spk_config;
	int32_t port_id = AFE_PORT_ID_TFADSP_TX;

	if (cmd_size > sizeof(afe_spk_config))
		return -EINVAL;

	memcpy(&afe_spk_config, buf, cmd_size);

	if (afe_spk_prot_prepare(port_id, 0,
			AFE_PARAM_ID_TFADSP_TX_SET_ENABLE,
			&afe_spk_config)) {
		pr_err("%s: AFE_PARAM_ID_TFADSP_TX_SET_ENABLE failed\n",
				   __func__);
	}

	return 0;
}
EXPORT_SYMBOL(send_tfa_cal_set_tx_enable);

#endif /*CONFIG_SND_SOC_TFA9874_FOR_DAVI*/

int __init afe_init(void)
{
	int i = 0, ret;

	atomic_set(&this_afe.state, 0);
	atomic_set(&this_afe.status, 0);
	atomic_set(&this_afe.mem_map_cal_index, -1);
	this_afe.apr = NULL;
	this_afe.dtmf_gen_rx_portid = -1;
	this_afe.mmap_handle = 0;
	this_afe.vi_tx_port = -1;
	this_afe.vi_rx_port = -1;
	this_afe.prot_cfg.mode = MSM_SPKR_PROT_DISABLED;
	this_afe.th_ftm_cfg.mode = MSM_SPKR_PROT_DISABLED;
	this_afe.ex_ftm_cfg.mode = MSM_SPKR_PROT_DISABLED;
	mutex_init(&this_afe.afe_cmd_lock);
	for (i = 0; i < AFE_MAX_PORTS; i++) {
		this_afe.afe_cal_mode[i] = AFE_CAL_MODE_DEFAULT;
		this_afe.afe_sample_rates[i] = 0;
		this_afe.dev_acdb_id[i] = 0;
		this_afe.island_mode[i] = 0;
		this_afe.vad_cfg[i].is_enable = 0;
		this_afe.vad_cfg[i].pre_roll = 0;
		init_waitqueue_head(&this_afe.wait[i]);
	}
	init_waitqueue_head(&this_afe.wait_wakeup);
	init_waitqueue_head(&this_afe.lpass_core_hw_wait);
	wakeup_source_init(&wl.ws, "spkr-prot");
	ret = afe_init_cal_data();
	if (ret)
		pr_err("%s: could not init cal data! %d\n", __func__, ret);

	config_debug_fs_init();

	this_afe.uevent_data = kzalloc(sizeof(*(this_afe.uevent_data)), GFP_KERNEL);
	if (!this_afe.uevent_data)
		return -ENOMEM;

	/*
	 * Set release function to cleanup memory related to kobject
	 * before initializing the kobject.
	 */
	this_afe.uevent_data->ktype.release = afe_release_uevent_data;
	q6core_init_uevent_data(this_afe.uevent_data, "q6afe_uevent");

	INIT_WORK(&this_afe.afe_dc_work, afe_notify_dc_presence_work_fn);
	INIT_WORK(&this_afe.afe_spdif_work,
		  afe_notify_spdif_fmt_update_work_fn);

	this_afe.event_notifier.notifier_call  = afe_aud_event_notify;
	msm_aud_evt_blocking_register_client(&this_afe.event_notifier);

	return 0;
}

void afe_exit(void)
{
#ifdef CONFIG_SND_SOC_TFA9874_FOR_DAVI
	afe_unmap_rtac_block(&this_afe.tfa_cal.map_data.map_handle);
#endif /*CONFIG_SND_SOC_TFA9874_FOR_DAVI*/

	if (this_afe.apr) {
		apr_reset(this_afe.apr);
		atomic_set(&this_afe.state, 0);
		this_afe.apr = NULL;
		rtac_set_afe_handle(this_afe.apr);
	}

	q6core_destroy_uevent_data(this_afe.uevent_data);

	afe_delete_cal_data();

	config_debug_fs_exit();
	mutex_destroy(&this_afe.afe_cmd_lock);
	wakeup_source_trash(&wl.ws);
}

/*
 * afe_cal_init_hwdep -
 *        Initiliaze AFE HW dependent Node
 *
 * @card: pointer to sound card
 *
 */
int afe_cal_init_hwdep(void *card)
{
	int ret = 0;

	this_afe.fw_data = kzalloc(sizeof(*(this_afe.fw_data)),
				      GFP_KERNEL);
	if (!this_afe.fw_data)
		return -ENOMEM;

	set_bit(Q6AFE_VAD_CORE_CAL, this_afe.fw_data->cal_bit);
	ret = q6afe_cal_create_hwdep(this_afe.fw_data, Q6AFE_HWDEP_NODE, card);
	if (ret < 0) {
		pr_err("%s: couldn't create hwdep for AFE %d\n", __func__, ret);
		return ret;
	}
	return ret;
}
EXPORT_SYMBOL(afe_cal_init_hwdep);

/*
 * afe_vote_lpass_core_hw -
 *        Voting for lpass core hardware
 *
 * @hw_block_id: ID of hw block to vote for
 * @client_name: Name of the client
 * @client_handle: Handle for the client
 *
 */
int afe_vote_lpass_core_hw(uint32_t hw_block_id, char *client_name,
			uint32_t *client_handle)
{
	struct afe_cmd_remote_lpass_core_hw_vote_request hw_vote_cfg;
	struct afe_cmd_remote_lpass_core_hw_vote_request *cmd_ptr =
						&hw_vote_cfg;
	int ret = 0;

	if (!client_handle) {
		pr_err("%s: Invalid client_handle\n", __func__);
		return -EINVAL;
	}

	if (!client_name) {
		pr_err("%s: Invalid client_name\n", __func__);
		*client_handle = 0;
		return -EINVAL;
	}

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	mutex_lock(&this_afe.afe_cmd_lock);

	memset(cmd_ptr, 0, sizeof(hw_vote_cfg));

	cmd_ptr->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
	cmd_ptr->hdr.pkt_size = sizeof(hw_vote_cfg);
	cmd_ptr->hdr.src_port = 0;
	cmd_ptr->hdr.dest_port = 0;
	cmd_ptr->hdr.token = 0;
	cmd_ptr->hdr.opcode = AFE_CMD_REMOTE_LPASS_CORE_HW_VOTE_REQUEST;
	cmd_ptr->hw_block_id = hw_block_id;
	strlcpy(cmd_ptr->client_name, client_name,
			sizeof(cmd_ptr->client_name));

	pr_debug("%s: lpass core hw vote opcode[0x%x] hw id[0x%x]\n",
		__func__, cmd_ptr->hdr.opcode, cmd_ptr->hw_block_id);

	*client_handle = 0;
	atomic_set(&this_afe.status, 0);
	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) cmd_ptr);
	if (ret < 0) {
		pr_err("%s: lpass core hw vote failed %d\n",
			__func__, ret);
		goto done;
	}

	ret = wait_event_timeout(this_afe.lpass_core_hw_wait,
		(atomic_read(&this_afe.state) == 0),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: timeout. waited for lpass core hw vote\n",
			__func__);
		ret = -ETIMEDOUT;
		goto done;
	} else {
		/* set ret to 0 as no timeout happened */
		ret = 0;
	}

	if (atomic_read(&this_afe.status) > 0) {
		pr_err("%s: lpass core hw vote cmd failed [%s]\n",
			__func__, adsp_err_get_err_str(
			atomic_read(&this_afe.status)));
		ret = adsp_err_get_lnx_err_code(
				atomic_read(&this_afe.status));
		goto done;
	}

	*client_handle = this_afe.lpass_hw_core_client_hdl;
	pr_debug("%s: lpass_hw_core_client_hdl %d\n", __func__,
		this_afe.lpass_hw_core_client_hdl);
done:
	mutex_unlock(&this_afe.afe_cmd_lock);
	return ret;
}
EXPORT_SYMBOL(afe_vote_lpass_core_hw);

/*
 * afe_unvote_lpass_core_hw -
 *        unvoting for lpass core hardware
 *
 * @hw_block_id: ID of hw block to vote for
 * @client_handle: Handle for the client
 *
 */
int afe_unvote_lpass_core_hw(uint32_t hw_block_id, uint32_t client_handle)
{
	struct afe_cmd_remote_lpass_core_hw_devote_request hw_vote_cfg;
	struct afe_cmd_remote_lpass_core_hw_devote_request *cmd_ptr =
						&hw_vote_cfg;
	int ret = 0;

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	mutex_lock(&this_afe.afe_cmd_lock);

	memset(cmd_ptr, 0, sizeof(hw_vote_cfg));

	cmd_ptr->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
	cmd_ptr->hdr.pkt_size = sizeof(hw_vote_cfg);
	cmd_ptr->hdr.src_port = 0;
	cmd_ptr->hdr.dest_port = 0;
	cmd_ptr->hdr.token = 0;
	cmd_ptr->hdr.opcode = AFE_CMD_REMOTE_LPASS_CORE_HW_DEVOTE_REQUEST;
	cmd_ptr->hw_block_id = hw_block_id;
	cmd_ptr->client_handle = client_handle;

	pr_debug("%s: lpass core hw devote opcode[0x%x] hw id[0x%x]\n",
		__func__, cmd_ptr->hdr.opcode, cmd_ptr->hw_block_id);

	if (cmd_ptr->client_handle <= 0) {
		pr_err("%s: invalid client handle\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	atomic_set(&this_afe.status, 0);
	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) cmd_ptr);
	if (ret < 0) {
		pr_err("%s: lpass core hw devote failed %d\n",
			__func__, ret);
		goto done;
	}

	ret = wait_event_timeout(this_afe.lpass_core_hw_wait,
		(atomic_read(&this_afe.state) == 0),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: timeout. waited for lpass core hw devote\n",
			__func__);
		ret = -ETIMEDOUT;
		goto done;
	} else {
		/* set ret to 0 as no timeout happened */
		ret = 0;
	}

	if (atomic_read(&this_afe.status) > 0) {
		pr_err("%s: lpass core hw devote cmd failed [%s]\n",
			__func__, adsp_err_get_err_str(
			atomic_read(&this_afe.status)));
		ret = adsp_err_get_lnx_err_code(
				atomic_read(&this_afe.status));
	}

done:
	mutex_unlock(&this_afe.afe_cmd_lock);
	return ret;
}
EXPORT_SYMBOL(afe_unvote_lpass_core_hw);

