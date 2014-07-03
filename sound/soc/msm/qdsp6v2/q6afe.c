/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/msm_audio_ion.h>
#include <sound/apr_audio-v2.h>
#include <sound/q6afe-v2.h>
#include <sound/q6audio-v2.h>
#include "msm-pcm-routing-v2.h"
#include "audio_cal_utils.h"

enum {
	AFE_COMMON_RX_CAL = 0,
	AFE_COMMON_TX_CAL,
	AFE_AANC_CAL,
	AFE_FB_SPKR_PROT_CAL,
	AFE_HW_DELAY_CAL,
	AFE_SIDETONE_CAL,
	MAX_AFE_CAL_TYPES
};

struct afe_ctl {
	void *apr;
	atomic_t state;
	atomic_t status;
	wait_queue_head_t wait[AFE_MAX_PORTS];
	struct task_struct *task;
	void (*tx_cb) (uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv);
	void (*rx_cb) (uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv);
	void *tx_private_data;
	void *rx_private_data;
	uint32_t mmap_handle;

	struct cal_type_data *cal_data[MAX_AFE_CAL_TYPES];

	atomic_t mem_map_cal_handles[MAX_AFE_CAL_TYPES];
	atomic_t mem_map_cal_index;

	u16 dtmf_gen_rx_portid;
	struct audio_cal_info_spk_prot_cfg	prot_cfg;
	struct afe_spkr_prot_calib_get_resp	calib_data;
	int vi_tx_port;
	uint32_t afe_sample_rates[AFE_MAX_PORTS];
	struct aanc_data aanc_info;
	struct mutex mem_map_lock;
};

static atomic_t afe_ports_mad_type[SLIMBUS_PORT_LAST - SLIMBUS_0_RX];
static unsigned long afe_configured_cmd;

static struct afe_ctl this_afe;

#define TIMEOUT_MS 1000
#define Q6AFE_MAX_VOLUME 0x3FFF

#define Q6AFE_MSM_SPKR_PROCESSING 0
#define Q6AFE_MSM_SPKR_CALIBRATION 1

static int pcm_afe_instance[2];
static int proxy_afe_instance[2];
bool afe_close_done[2] = {true, true};

#define SIZEOF_CFG_CMD(y) \
		(sizeof(struct apr_hdr) + sizeof(u16) + (sizeof(struct y)))

static int afe_get_cal_hw_delay(int32_t path,
				struct audio_cal_hw_delay_entry *entry);

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


static int32_t afe_callback(struct apr_client_data *data, void *priv)
{
	if (!data) {
		pr_err("%s: Invalid param data\n", __func__);
		return -EINVAL;
	}
	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: reset event = %d %d apr[%p]\n",
			__func__,
			data->reset_event, data->reset_proc, this_afe.apr);

		cal_utils_clear_cal_block_q6maps(MAX_AFE_CAL_TYPES,
			this_afe.cal_data);

		if (this_afe.apr) {
			apr_reset(this_afe.apr);
			atomic_set(&this_afe.state, 0);
			this_afe.apr = NULL;
			rtac_set_afe_handle(this_afe.apr);
		}
		/* send info to user */
		if (this_afe.task == NULL)
			this_afe.task = current;
		pr_debug("%s: task_name = %s pid = %d\n",
			__func__,
			this_afe.task->comm, this_afe.task->pid);
		return 0;
	}
	pr_debug("%s: opcode = 0x%x cmd = 0x%x status = 0x%x size = %d\n",
			__func__, data->opcode,
			((uint32_t *)(data->payload))[0],
			((uint32_t *)(data->payload))[1],
			 data->payload_size);
	if (data->opcode == AFE_PORT_CMDRSP_GET_PARAM_V2) {
		u8 *payload = data->payload;

		if (rtac_make_afe_callback(data->payload, data->payload_size))
			return 0;

		if ((data->payload_size < sizeof(this_afe.calib_data))
			|| !payload || (data->token >= AFE_MAX_PORTS)) {
			pr_err("%s: Error: size %d payload %p token %d\n",
				__func__, data->payload_size,
				payload, data->token);
			return -EINVAL;
		}
		memcpy(&this_afe.calib_data, payload,
			   sizeof(this_afe.calib_data));
		if (!this_afe.calib_data.status) {
			atomic_set(&this_afe.state, 0);
			pr_err("%s: rest = %d state = 0x%x\n", __func__
			, this_afe.calib_data.res_cfg.r0_cali_q24,
			this_afe.calib_data.res_cfg.th_vi_ca_state);
		} else
			atomic_set(&this_afe.state, -1);
		wake_up(&this_afe.wait[data->token]);
	} else if (data->payload_size) {
		uint32_t *payload;
		uint16_t port_id = 0;
		payload = data->payload;
		pr_debug("%s:opcode = 0x%x cmd = 0x%x status = 0x%x token=%d\n",
					__func__, data->opcode,
					payload[0], payload[1], data->token);
		if (data->opcode == APR_BASIC_RSP_RESULT) {
			/* payload[1] contains the error status for response */
			if (payload[1] != 0) {
				atomic_set(&this_afe.status, -1);
				pr_err("%s: cmd = 0x%x returned error = 0x%x\n",
					__func__, payload[0], payload[1]);
			}
			switch (payload[0]) {
			case AFE_PORT_CMD_SET_PARAM_V2:
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
				atomic_set(&this_afe.state, 0);
				wake_up(&this_afe.wait[data->token]);
				break;
			case AFE_SERVICE_CMD_REGISTER_RT_PORT_DRIVER:
				break;
			case AFE_PORT_DATA_CMD_RT_PROXY_PORT_WRITE_V2:
				port_id = RT_PROXY_PORT_001_TX;
				break;
			case AFE_PORT_DATA_CMD_RT_PROXY_PORT_READ_V2:
				port_id = RT_PROXY_PORT_001_RX;
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
			wake_up(&this_afe.wait[data->token]);
		} else if (data->opcode == AFE_EVENT_RT_PROXY_PORT_STATUS) {
			port_id = (uint16_t)(0x0000FFFF & payload[0]);
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
			pr_err("%s: default case 0x%x\n", __func__, port_id);
			break;
		}
	}
	return 0;
}


int afe_get_port_type(u16 port_id)
{
	int ret;

	switch (port_id) {
	case PRIMARY_I2S_RX:
	case AFE_PORT_ID_PRIMARY_PCM_RX:
	case SECONDARY_I2S_RX:
	case MI2S_RX:
	case HDMI_RX:
	case AFE_PORT_ID_SPDIF_RX:
	case SLIMBUS_0_RX:
	case SLIMBUS_1_RX:
	case SLIMBUS_2_RX:
	case SLIMBUS_3_RX:
	case SLIMBUS_4_RX:
	case SLIMBUS_6_RX:
	case INT_BT_SCO_RX:
	case INT_BT_A2DP_RX:
	case INT_FM_RX:
	case VOICE_PLAYBACK_TX:
	case VOICE2_PLAYBACK_TX:
	case RT_PROXY_PORT_001_RX:
	case AUDIO_PORT_ID_I2S_RX:
	case AFE_PORT_ID_PRIMARY_MI2S_RX:
	case AFE_PORT_ID_SECONDARY_MI2S_RX:
	case AFE_PORT_ID_SECONDARY_MI2S_RX_SD1:
	case AFE_PORT_ID_TERTIARY_MI2S_RX:
	case AFE_PORT_ID_QUATERNARY_MI2S_RX:
	case AFE_PORT_ID_SECONDARY_PCM_RX:
		ret = MSM_AFE_PORT_TYPE_RX;
		break;

	case PRIMARY_I2S_TX:
	case AFE_PORT_ID_PRIMARY_PCM_TX:
	case SECONDARY_I2S_TX:
	case MI2S_TX:
	case DIGI_MIC_TX:
	case VOICE_RECORD_TX:
	case SLIMBUS_0_TX:
	case SLIMBUS_1_TX:
	case SLIMBUS_2_TX:
	case SLIMBUS_3_TX:
	case SLIMBUS_4_TX:
	case SLIMBUS_5_TX:
	case SLIMBUS_6_TX:
	case INT_FM_TX:
	case VOICE_RECORD_RX:
	case INT_BT_SCO_TX:
	case RT_PROXY_PORT_001_TX:
	case AFE_PORT_ID_PRIMARY_MI2S_TX:
	case AFE_PORT_ID_SECONDARY_MI2S_TX:
	case AFE_PORT_ID_TERTIARY_MI2S_TX:
	case AFE_PORT_ID_QUATERNARY_MI2S_TX:
	case AFE_PORT_ID_SECONDARY_PCM_TX:
		ret = MSM_AFE_PORT_TYPE_TX;
		break;

	default:
		WARN_ON(1);
		pr_err("%s: Invalid port id = 0x%x\n",
			__func__, port_id);
		ret = -EINVAL;
	}

	return ret;
}

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
		ret_size = SIZEOF_CFG_CMD(afe_param_id_i2s_cfg);
		break;
	case HDMI_RX:
		ret_size =
		SIZEOF_CFG_CMD(afe_param_id_hdmi_multi_chan_audio_cfg);
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
	case AFE_PORT_ID_PRIMARY_PCM_RX:
	case AFE_PORT_ID_PRIMARY_PCM_TX:
	case AFE_PORT_ID_SECONDARY_PCM_RX:
	case AFE_PORT_ID_SECONDARY_PCM_TX:
	default:
		pr_err("%s: default case 0x%x\n", __func__, port_id);
		ret_size = SIZEOF_CFG_CMD(afe_param_id_pcm_cfg);
		break;
	}
	return ret_size;
}

int afe_q6_interface_prepare(void)
{
	int ret = 0;

	pr_debug("%s:\n", __func__);

	if (this_afe.apr == NULL) {
		this_afe.apr = apr_register("ADSP", "AFE", afe_callback,
			0xFFFFFFFF, &this_afe);
		if (this_afe.apr == NULL) {
			pr_err("%s: Unable to register AFE\n", __func__);
			ret = -ENODEV;
		}
		rtac_set_afe_handle(this_afe.apr);
	}
	return ret;
}

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
					msecs_to_jiffies(TIMEOUT_MS));
			if (ret)
				ret = 0;
			else
				ret = -ETIMEDOUT;
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

static int afe_send_cal_block(u16 port_id, struct cal_block_data *cal_block)
{
	int						result = 0;
	int						index = 0;
	struct afe_audioif_config_command_no_payload	afe_cal;

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

	index = q6audio_get_port_index(port_id);
	if (index < 0 || index > AFE_MAX_PORTS) {
		pr_debug("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		result = -EINVAL;
		goto done;
	}

	afe_cal.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	afe_cal.hdr.pkt_size = sizeof(afe_cal);
	afe_cal.hdr.src_port = 0;
	afe_cal.hdr.dest_port = 0;
	afe_cal.hdr.token = index;
	afe_cal.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	afe_cal.param.port_id = port_id;
	afe_cal.param.payload_size = cal_block->cal_data.size;
	afe_cal.param.payload_address_lsw =
		lower_32_bits(cal_block->cal_data.paddr);
	afe_cal.param.payload_address_msw =
		upper_32_bits(cal_block->cal_data.paddr);
	afe_cal.param.mem_map_handle = cal_block->map_data.q6map_handle;

	pr_debug("%s: AFE cal sent for device port = 0x%x, cal size = %zd, cal addr = 0x%pa\n",
		__func__, port_id,
		cal_block->cal_data.size, &cal_block->cal_data.paddr);

	result = afe_apr_send_pkt(&afe_cal, &this_afe.wait[index]);
	if (result)
		pr_err("%s: AFE cal for port 0x%x failed %d\n",
		       __func__, port_id, result);

done:
	return result;
}

static int afe_spk_prot_prepare(int port, int param_id,
		union afe_spkr_prot_config *prot_config)
{
	int ret = -EINVAL;
	int index = 0;
	struct afe_spkr_prot_config_command config;

	memset(&config, 0 , sizeof(config));
	if (!prot_config) {
		pr_err("%s: Invalid params\n", __func__);
		goto fail_cmd;
	}
	ret = q6audio_validate_port(port);
	if (ret < 0) {
		pr_err("%s: Invalid port 0x%x ret %d", __func__, port, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}
	index = q6audio_get_port_index(port);
	switch (param_id) {
	case AFE_PARAM_ID_FBSP_MODE_RX_CFG:
		config.pdata.module_id = AFE_MODULE_FB_SPKR_PROT_RX;
		break;
	case AFE_PARAM_ID_FEEDBACK_PATH_CFG:
		this_afe.vi_tx_port = port;
	case AFE_PARAM_ID_SPKR_CALIB_VI_PROC_CFG:
	case AFE_PARAM_ID_MODE_VI_PROC_CFG:
		config.pdata.module_id = AFE_MODULE_FB_SPKR_PROT_VI_PROC;
		break;
	default:
		pr_err("%s: default case 0x%x\n", __func__, param_id);
		goto fail_cmd;
		break;
	}

	config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	config.hdr.pkt_size = sizeof(config);
	config.hdr.src_port = 0;
	config.hdr.dest_port = 0;
	config.hdr.token = index;

	config.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	config.param.port_id = q6audio_get_port_id(port);
	config.param.payload_size = sizeof(config) - sizeof(config.hdr)
		- sizeof(config.param);
	config.pdata.param_id = param_id;
	config.pdata.param_size = sizeof(config.prot_config);
	config.prot_config = *prot_config;
	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &config);
	if (ret < 0) {
		pr_err("%s: port = 0x%x param = 0x%x failed %d\n",
		__func__, port, param_id, ret);
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
	if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = 0;
fail_cmd:
	pr_debug("%s: config.pdata.param_id 0x%x status %d\n",
	__func__, config.pdata.param_id, ret);
	return ret;
}

static void afe_send_cal_spkr_prot_tx(int port_id)
{
	union afe_spkr_prot_config afe_spk_config;

	if (this_afe.cal_data[AFE_FB_SPKR_PROT_CAL] == NULL)
		goto done;

	mutex_lock(&this_afe.cal_data[AFE_FB_SPKR_PROT_CAL]->lock);

	if ((this_afe.prot_cfg.mode != MSM_SPKR_PROT_DISABLED) &&
		(this_afe.vi_tx_port == port_id)) {
		afe_spk_config.mode_rx_cfg.minor_version = 1;
		if (this_afe.prot_cfg.mode ==
			MSM_SPKR_PROT_CALIBRATION_IN_PROGRESS)
			afe_spk_config.mode_rx_cfg.mode =
				Q6AFE_MSM_SPKR_CALIBRATION;
		else
			afe_spk_config.mode_rx_cfg.mode =
				Q6AFE_MSM_SPKR_PROCESSING;
		if (afe_spk_prot_prepare(port_id,
			AFE_PARAM_ID_MODE_VI_PROC_CFG,
			&afe_spk_config))
			pr_err("%s: TX VI_PROC_CFG failed\n", __func__);
		if (this_afe.prot_cfg.mode != MSM_SPKR_PROT_NOT_CALIBRATED) {
			afe_spk_config.vi_proc_cfg.minor_version = 1;
			afe_spk_config.vi_proc_cfg.r0_cali_q24 =
			(uint32_t) this_afe.prot_cfg.r0;
			afe_spk_config.vi_proc_cfg.t0_cali_q6 =
			(uint32_t) this_afe.prot_cfg.t0;
			if (afe_spk_prot_prepare(port_id,
				AFE_PARAM_ID_SPKR_CALIB_VI_PROC_CFG,
				&afe_spk_config))
				pr_err("%s: SPKR_CALIB_VI_PROC_CFG failed\n",
					__func__);
		}
	}
	mutex_unlock(&this_afe.cal_data[AFE_FB_SPKR_PROT_CAL]->lock);
done:
	return;
}

static void afe_send_cal_spkr_prot_rx(int port_id)
{
	union afe_spkr_prot_config afe_spk_config;

	if (this_afe.cal_data[AFE_FB_SPKR_PROT_CAL] == NULL)
		goto done;

	mutex_lock(&this_afe.cal_data[AFE_FB_SPKR_PROT_CAL]->lock);

	if (this_afe.prot_cfg.mode != MSM_SPKR_PROT_DISABLED) {
		if (this_afe.prot_cfg.mode ==
			MSM_SPKR_PROT_CALIBRATION_IN_PROGRESS)
			afe_spk_config.mode_rx_cfg.mode =
				Q6AFE_MSM_SPKR_CALIBRATION;
		else
			afe_spk_config.mode_rx_cfg.mode =
				Q6AFE_MSM_SPKR_PROCESSING;
		afe_spk_config.mode_rx_cfg.minor_version = 1;
		if (afe_spk_prot_prepare(port_id,
			AFE_PARAM_ID_FBSP_MODE_RX_CFG,
			&afe_spk_config))
			pr_err("%s: RX MODE_VI_PROC_CFG failed\n",
				   __func__);
	}
	mutex_unlock(&this_afe.cal_data[AFE_FB_SPKR_PROT_CAL]->lock);
done:
	return;
}

static int afe_send_hw_delay(u16 port_id, u32 rate)
{
	struct audio_cal_hw_delay_entry		delay_entry;
	struct afe_audioif_config_command	config;
	int index = 0;
	int ret = -EINVAL;

	pr_debug("%s:\n", __func__);

	delay_entry.sample_rate = rate;
	if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_TX)
		ret = afe_get_cal_hw_delay(TX_DEVICE, &delay_entry);
	else if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_RX)
		ret = afe_get_cal_hw_delay(RX_DEVICE, &delay_entry);

	if (ret != 0) {
		pr_debug("%s: Failed to get hw delay info %d\n", __func__, ret);
		goto fail_cmd;
	}

	index = q6audio_get_port_index(port_id);
	if (index < 0 || index > AFE_MAX_PORTS) {
		pr_debug("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		goto fail_cmd;
	}

	config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	config.hdr.pkt_size = sizeof(config);
	config.hdr.src_port = 0;
	config.hdr.dest_port = 0;
	config.hdr.token = index;

	config.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	config.param.port_id = q6audio_get_port_id(port_id);
	config.param.payload_size = sizeof(config) - sizeof(struct apr_hdr) -
				    sizeof(config.param);
	config.param.payload_address_lsw = 0x00;
	config.param.payload_address_msw = 0x00;
	config.param.mem_map_handle = 0x00;
	config.pdata.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	config.pdata.param_id = AFE_PARAM_ID_DEVICE_HW_DELAY;
	config.pdata.param_size = sizeof(config.port);

	config.port.hw_delay.delay_in_us = delay_entry.delay_usec;
	config.port.hw_delay.device_hw_delay_minor_version =
				AFE_API_VERSION_DEVICE_HW_DELAY;

	ret = afe_apr_send_pkt(&config, &this_afe.wait[index]);
	if (ret) {
		pr_err("%s: AFE hw delay for port 0x%x failed %d\n",
		       __func__, port_id, ret);
		goto fail_cmd;
	} else if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}

fail_cmd:
	pr_debug("%s: port_id 0x%x rate %u delay_usec %d status %d\n",
	__func__, port_id, rate, delay_entry.delay_usec, ret);
	return ret;

}

static void remap_cal_data(struct cal_block_data *cal_block, int cal_index)
{
	int ret = 0;

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
			pr_debug("%s: mmap did not work! addr = 0x%pa, size = %zd\n",
				__func__,
				&cal_block->cal_data.paddr,
				cal_block->map_data.map_size);
			goto done;
		}
		cal_block->map_data.q6map_handle = atomic_read(&this_afe.
			mem_map_cal_handles[cal_index]);
	}
done:
	return;
}

static void send_afe_cal_type(int cal_index, int port_id)
{
	struct cal_block_data		*cal_block = NULL;
	int ret;
	pr_debug("%s:\n", __func__);

	if (this_afe.cal_data[cal_index] == NULL) {
		pr_warn("%s: cal_index %d not allocated!\n",
			__func__, cal_index);
		goto done;
	}

	mutex_lock(&this_afe.cal_data[cal_index]->lock);
	cal_block = cal_utils_get_only_cal_block(this_afe.cal_data[cal_index]);
	if (cal_block == NULL)
		goto unlock;

	pr_debug("%s: Sending cal_index cal %d\n", __func__, cal_index);

	remap_cal_data(cal_block, cal_index);
	ret = afe_send_cal_block(port_id, cal_block);
	if (ret < 0)
		pr_debug("%s: No cal sent for cal_index %d, port_id = 0x%x! ret %d\n",
			__func__, cal_index, port_id, ret);
unlock:
	mutex_unlock(&this_afe.cal_data[cal_index]->lock);
done:
	return;
}

void afe_send_cal(u16 port_id)
{
	pr_debug("%s:\n", __func__);

	if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_TX) {
		afe_send_cal_spkr_prot_tx(port_id);
		send_afe_cal_type(AFE_COMMON_TX_CAL, port_id);
	} else if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_RX) {
		afe_send_cal_spkr_prot_rx(port_id);
		send_afe_cal_type(AFE_COMMON_RX_CAL, port_id);
	}
}

int afe_turn_onoff_hw_mad(u16 mad_type, u16 enable)
{
	int ret;
	struct afe_cmd_hw_mad_ctrl config;

	pr_debug("%s: enter\n", __func__);
	memset(&config, 0, sizeof(config));
	config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					     APR_HDR_LEN(APR_HDR_SIZE),
					     APR_PKT_VER);
	config.hdr.pkt_size = sizeof(config);
	config.hdr.src_port = 0;
	config.hdr.dest_port = 0;
	config.hdr.token = IDX_GLOBAL_CFG;
	config.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	config.param.port_id = SLIMBUS_5_TX;
	config.param.payload_size = sizeof(config) - sizeof(struct apr_hdr) -
				    sizeof(config.param);
	config.param.payload_address_lsw = 0x00;
	config.param.payload_address_msw = 0x00;
	config.param.mem_map_handle = 0x00;
	config.pdata.module_id = AFE_MODULE_HW_MAD;
	config.pdata.param_id = AFE_PARAM_ID_HW_MAD_CTRL;
	config.pdata.param_size = sizeof(config.payload);
	config.payload.minor_version = 1;
	config.payload.mad_type = mad_type;
	config.payload.mad_enable = enable;

	ret = afe_apr_send_pkt(&config, &this_afe.wait[IDX_GLOBAL_CFG]);
	if (ret)
		pr_err("%s: AFE_PARAM_ID_HW_MAD_CTRL failed %d\n", __func__,
		       ret);
	return ret;
}

static int afe_send_slimbus_slave_cfg(
	struct afe_param_cdc_slimbus_slave_cfg *sb_slave_cfg)
{
	int ret;
	struct afe_svc_cmd_sb_slave_cfg config;

	pr_debug("%s: enter\n", __func__);

	config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					     APR_HDR_LEN(APR_HDR_SIZE),
					     APR_PKT_VER);
	config.hdr.pkt_size = sizeof(config);
	config.hdr.src_port = 0;
	config.hdr.dest_port = 0;
	config.hdr.token = IDX_GLOBAL_CFG;
	config.hdr.opcode = AFE_SVC_CMD_SET_PARAM;
	config.param.payload_size = sizeof(config) - sizeof(struct apr_hdr) -
				    sizeof(config.param);
	config.param.payload_address_lsw = 0x00;
	config.param.payload_address_msw = 0x00;
	config.param.mem_map_handle = 0x00;
	config.pdata.module_id = AFE_MODULE_CDC_DEV_CFG;
	config.pdata.param_id = AFE_PARAM_ID_CDC_SLIMBUS_SLAVE_CFG;
	config.pdata.param_size =
	    sizeof(struct afe_param_cdc_slimbus_slave_cfg);
	config.sb_slave_cfg = *sb_slave_cfg;

	ret = afe_apr_send_pkt(&config, &this_afe.wait[IDX_GLOBAL_CFG]);
	if (ret)
		pr_err("%s: AFE_PARAM_ID_CDC_SLIMBUS_SLAVE_CFG failed %d\n",
		       __func__, ret);

	pr_debug("%s: leave %d\n", __func__, ret);
	return ret;
}

static int afe_send_codec_reg_config(
	struct afe_param_cdc_reg_cfg_data *cdc_reg_cfg)
{
	int i, ret;
	int pkt_size, payload_size;
	struct afe_svc_cmd_cdc_reg_cfg *config;
	struct afe_svc_cmd_set_param *param;

	payload_size = sizeof(struct afe_param_cdc_reg_cfg_payload) *
		       cdc_reg_cfg->num_registers;
	pkt_size = sizeof(*config) + payload_size;

	pr_debug("%s: pkt_size %d, payload_size %d\n", __func__, pkt_size,
		 payload_size);
	config = kzalloc(pkt_size, GFP_KERNEL);
	if (!config) {
		pr_warn("%s: Not enought memory, pkt_size %d\n", __func__,
			pkt_size);
		return -ENOMEM;
	}

	config->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					      APR_HDR_LEN(APR_HDR_SIZE),
					      APR_PKT_VER);
	config->hdr.pkt_size = pkt_size;
	config->hdr.src_port = 0;
	config->hdr.dest_port = 0;
	config->hdr.token = IDX_GLOBAL_CFG;
	config->hdr.opcode = AFE_SVC_CMD_SET_PARAM;

	param = &config->param;
	param->payload_size = payload_size;
	param->payload_address_lsw = 0x00;
	param->payload_address_msw = 0x00;
	param->mem_map_handle = 0x00;

	for (i = 0; i < cdc_reg_cfg->num_registers; i++) {
		config->reg_data[i].common.module_id = AFE_MODULE_CDC_DEV_CFG;
		config->reg_data[i].common.param_id = AFE_PARAM_ID_CDC_REG_CFG;
		config->reg_data[i].common.param_size =
		    sizeof(config->reg_data[i].reg_cfg);
		config->reg_data[i].reg_cfg = cdc_reg_cfg->reg_data[i];
	}

	ret = afe_apr_send_pkt(config, &this_afe.wait[IDX_GLOBAL_CFG]);
	if (ret)
		pr_err("%s: AFE_PARAM_ID_CDC_REG_CFG failed %d\n", __func__,
		       ret);

	kfree(config);
	return ret;
}

static int afe_init_cdc_reg_config(void)
{
	int ret;
	struct afe_svc_cmd_init_cdc_reg_cfg config;

	pr_debug("%s: enter\n", __func__);
	config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					     APR_HDR_LEN(APR_HDR_SIZE),
					     APR_PKT_VER);
	config.hdr.pkt_size = sizeof(config);
	config.hdr.src_port = 0;
	config.hdr.dest_port = 0;
	config.hdr.token = IDX_GLOBAL_CFG;
	config.hdr.opcode = AFE_SVC_CMD_SET_PARAM;

	config.param.payload_size = sizeof(struct afe_port_param_data_v2);
	config.param.payload_address_lsw = 0x00;
	config.param.payload_address_msw = 0x00;
	config.param.mem_map_handle = 0x00;

	config.init.module_id = AFE_MODULE_CDC_DEV_CFG;
	config.init.param_id = AFE_PARAM_ID_CDC_REG_CFG_INIT;
	config.init.param_size = 0;

	ret = afe_apr_send_pkt(&config, &this_afe.wait[IDX_GLOBAL_CFG]);
	if (ret) {
		pr_err("%s: AFE_PARAM_ID_CDC_INIT_REG_CFG failed %d\n",
		       __func__, ret);
	} else if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

static int afe_send_slimbus_slave_port_cfg(
	struct afe_param_slimbus_slave_port_cfg *port_config, u16 port_id)
{
	int ret, index;
	struct afe_cmd_hw_mad_slimbus_slave_port_cfg config;

	pr_debug("%s: enter, port_id =  0x%x\n", __func__, port_id);
	index = q6audio_get_port_index(port_id);
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: port id = 0x%x ret %d\n", __func__, port_id, ret);
		return -EINVAL;
	}

	config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					     APR_HDR_LEN(APR_HDR_SIZE),
					     APR_PKT_VER);
	config.hdr.pkt_size = sizeof(config);
	config.hdr.src_port = 0;
	config.hdr.dest_port = 0;
	config.hdr.token = index;
	config.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	config.param.port_id = port_id;
	config.param.payload_size = sizeof(config) - sizeof(struct apr_hdr) -
				    sizeof(config.param);
	config.param.payload_address_lsw = 0x00;
	config.param.payload_address_msw = 0x00;
	config.param.mem_map_handle = 0x00;
	config.pdata.module_id = AFE_MODULE_HW_MAD;
	config.pdata.param_id = AFE_PARAM_ID_SLIMBUS_SLAVE_PORT_CFG;
	config.pdata.param_size = sizeof(*port_config);
	config.sb_port_cfg = *port_config;

	ret = afe_apr_send_pkt(&config, &this_afe.wait[index]);
	if (ret) {
		pr_err("%s: AFE_PARAM_ID_SLIMBUS_SLAVE_PORT_CFG failed %d\n",
			__func__, ret);
	} else if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
	}
	pr_debug("%s: leave %d\n", __func__, ret);
	return ret;
}
static int afe_aanc_port_cfg(void *apr, uint16_t tx_port, uint16_t rx_port)
{
	struct afe_port_cmd_set_aanc_param cfg;
	int ret = 0;
	int index = 0;

	pr_debug("%s: tx_port 0x%x, rx_port 0x%x\n",
		__func__, tx_port, rx_port);

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return -EINVAL;
	}

	index = q6audio_get_port_index(tx_port);
	ret = q6audio_validate_port(tx_port);
	if (ret < 0) {
		pr_err("%s: port id: 0x%x ret %d\n", __func__, tx_port, ret);
		return -EINVAL;
	}

	cfg.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cfg.hdr.pkt_size = sizeof(cfg);
	cfg.hdr.src_port = 0;
	cfg.hdr.dest_port = 0;
	cfg.hdr.token = index;
	cfg.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;

	cfg.param.port_id = tx_port;
	cfg.param.payload_size        = sizeof(struct afe_port_param_data_v2) +
					sizeof(struct afe_param_aanc_port_cfg);
	cfg.param.payload_address_lsw     = 0;
	cfg.param.payload_address_msw     = 0;
	cfg.param.mem_map_handle	  = 0;

	cfg.pdata.module_id = AFE_MODULE_AANC;
	cfg.pdata.param_id    = AFE_PARAM_ID_AANC_PORT_CONFIG;
	cfg.pdata.param_size = sizeof(struct afe_param_aanc_port_cfg);
	cfg.pdata.reserved    = 0;

	cfg.data.aanc_port_cfg.aanc_port_cfg_minor_version =
		AFE_API_VERSION_AANC_PORT_CONFIG;
	cfg.data.aanc_port_cfg.tx_port_sample_rate =
		this_afe.aanc_info.aanc_tx_port_sample_rate;
	cfg.data.aanc_port_cfg.tx_port_channel_map[0] = AANC_TX_VOICE_MIC;
	cfg.data.aanc_port_cfg.tx_port_channel_map[1] = AANC_TX_NOISE_MIC;
	cfg.data.aanc_port_cfg.tx_port_channel_map[2] = AANC_TX_ERROR_MIC;
	cfg.data.aanc_port_cfg.tx_port_channel_map[3] = AANC_TX_MIC_UNUSED;
	cfg.data.aanc_port_cfg.tx_port_channel_map[4] = AANC_TX_MIC_UNUSED;
	cfg.data.aanc_port_cfg.tx_port_channel_map[5] = AANC_TX_MIC_UNUSED;
	cfg.data.aanc_port_cfg.tx_port_channel_map[6] = AANC_TX_MIC_UNUSED;
	cfg.data.aanc_port_cfg.tx_port_channel_map[7] = AANC_TX_MIC_UNUSED;
	cfg.data.aanc_port_cfg.tx_port_num_channels = 3;
	cfg.data.aanc_port_cfg.rx_path_ref_port_id = rx_port;
	cfg.data.aanc_port_cfg.ref_port_sample_rate =
		 this_afe.aanc_info.aanc_rx_port_sample_rate;

	ret = afe_apr_send_pkt((uint32_t *) &cfg, &this_afe.wait[index]);
	if (ret) {
		pr_err("%s: AFE AANC port config failed for tx_port 0x%x, rx_port 0x%x ret %d\n",
			__func__, tx_port, rx_port, ret);
	} else if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

static int afe_aanc_mod_enable(void *apr, uint16_t tx_port, uint16_t enable)
{
	struct afe_port_cmd_set_aanc_param cfg;
	int ret = 0;
	int index = 0;

	pr_debug("%s: tx_port 0x%x\n",
		__func__, tx_port);

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return -EINVAL;
	}

	index = q6audio_get_port_index(tx_port);
	ret = q6audio_validate_port(tx_port);
	if (ret < 0) {
		pr_err("%s: port id: 0x%x ret %d\n", __func__, tx_port, ret);
		return -EINVAL;
	}

	cfg.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cfg.hdr.pkt_size = sizeof(cfg);
	cfg.hdr.src_port = 0;
	cfg.hdr.dest_port = 0;
	cfg.hdr.token = index;
	cfg.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;

	cfg.param.port_id = tx_port;
	cfg.param.payload_size        = sizeof(struct afe_port_param_data_v2) +
					sizeof(struct afe_mod_enable_param);
	cfg.param.payload_address_lsw     = 0;
	cfg.param.payload_address_lsw     = 0;
	cfg.param.mem_map_handle          = 0;

	cfg.pdata.module_id = AFE_MODULE_AANC;
	cfg.pdata.param_id    = AFE_PARAM_ID_ENABLE;
	cfg.pdata.param_size = sizeof(struct afe_mod_enable_param);
	cfg.pdata.reserved    = 0;

	cfg.data.mod_enable.enable = enable;
	cfg.data.mod_enable.reserved = 0;

	ret = afe_apr_send_pkt((uint32_t *) &cfg, &this_afe.wait[index]);
	if (ret) {
		pr_err("%s: AFE AANC enable failed for tx_port 0x%x ret %d\n",
			__func__, tx_port, ret);
	} else if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
	}
	return ret;
}

static int afe_send_bank_selection_clip(
		struct afe_param_id_clip_bank_sel *param)
{
	int ret;
	struct afe_svc_cmd_set_clip_bank_selection config;
	if (!param) {
		pr_err("%s: Invalid params", __func__);
		return -EINVAL;
	}
	config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	config.hdr.pkt_size = sizeof(config);
	config.hdr.src_port = 0;
	config.hdr.dest_port = 0;
	config.hdr.token = IDX_GLOBAL_CFG;
	config.hdr.opcode = AFE_SVC_CMD_SET_PARAM;

	config.param.payload_size = sizeof(struct afe_port_param_data_v2) +
				sizeof(struct afe_param_id_clip_bank_sel);
	config.param.payload_address_lsw = 0x00;
	config.param.payload_address_msw = 0x00;
	config.param.mem_map_handle = 0x00;

	config.pdata.module_id = AFE_MODULE_CDC_DEV_CFG;
	config.pdata.param_id = AFE_PARAM_ID_CLIP_BANK_SEL_CFG;
	config.pdata.param_size =
		sizeof(struct afe_param_id_clip_bank_sel);
	config.bank_sel = *param;
	ret = afe_apr_send_pkt(&config, &this_afe.wait[IDX_GLOBAL_CFG]);
	if (ret) {
		pr_err("%s: AFE_PARAM_ID_CLIP_BANK_SEL_CFG failed %d\n",
		__func__, ret);
	} else if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EAGAIN;
	}
	return ret;
}
int afe_send_aanc_version(
	struct afe_param_id_cdc_aanc_version *version_cfg)
{
	int ret;
	struct afe_svc_cmd_cdc_aanc_version config;

	pr_debug("%s: enter\n", __func__);
	config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	config.hdr.pkt_size = sizeof(config);
	config.hdr.src_port = 0;
	config.hdr.dest_port = 0;
	config.hdr.token = IDX_GLOBAL_CFG;
	config.hdr.opcode = AFE_SVC_CMD_SET_PARAM;

	config.param.payload_size = sizeof(struct afe_port_param_data_v2) +
				sizeof(struct afe_param_id_cdc_aanc_version);
	config.param.payload_address_lsw = 0x00;
	config.param.payload_address_msw = 0x00;
	config.param.mem_map_handle = 0x00;

	config.pdata.module_id = AFE_MODULE_CDC_DEV_CFG;
	config.pdata.param_id = AFE_PARAM_ID_CDC_AANC_VERSION;
	config.pdata.param_size =
		sizeof(struct afe_param_id_cdc_aanc_version);
	config.version = *version_cfg;
	ret = afe_apr_send_pkt(&config, &this_afe.wait[IDX_GLOBAL_CFG]);
	if (ret) {
		pr_err("%s: AFE_PARAM_ID_CDC_AANC_VERSION failed %d\n",
		__func__, ret);
	} else if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
	ret = -EINVAL;
	}
	return ret;
}

int afe_port_set_mad_type(u16 port_id, enum afe_mad_type mad_type)
{
	int i;

	if (port_id == AFE_PORT_ID_TERTIARY_MI2S_TX) {
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

enum afe_mad_type afe_port_get_mad_type(u16 port_id)
{
	int i;

	if (port_id == AFE_PORT_ID_TERTIARY_MI2S_TX)
		return MAD_SW_AUDIO;

	i = port_id - SLIMBUS_0_RX;
	if (i < 0 || i >= ARRAY_SIZE(afe_ports_mad_type)) {
		pr_debug("%s: Non Slimbus port_id 0x%x\n", __func__, port_id);
		return MAD_HW_NONE;
	}
	return (enum afe_mad_type) atomic_read(&afe_ports_mad_type[i]);
}

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
	default:
		pr_err("%s: unknown configuration type %d",
			__func__, config_type);
		ret = -EINVAL;
	}

	if (!ret)
		set_bit(config_type, &afe_configured_cmd);

	return ret;
}

/*
 * afe_clear_config - If SSR happens ADSP loses AFE configs, let AFE driver know
 *		      about the state so client driver can wait until AFE is
 *		      reconfigured.
 */
void afe_clear_config(enum afe_config_type config)
{
	clear_bit(config, &afe_configured_cmd);
}

bool afe_has_config(enum afe_config_type config)
{
	return !!test_bit(config, &afe_configured_cmd);
}

int afe_send_spdif_clk_cfg(struct afe_param_id_spdif_clk_cfg *cfg,
		u16 port_id)
{
	struct afe_spdif_clk_config_command clk_cfg;
	int ret = 0;
	int index = 0;

	if (!cfg) {
		pr_err("%s: Error, no configuration data\n", __func__);
		ret = -EINVAL;
		return ret;
	}
	index = q6audio_get_port_index(port_id);
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: port id: 0x%x ret %d\n", __func__, port_id, ret);
		return -EINVAL;
	}

	ret = afe_q6_interface_prepare();
	if (ret) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}
	clk_cfg.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	clk_cfg.hdr.pkt_size = sizeof(clk_cfg);
	clk_cfg.hdr.src_port = 0;
	clk_cfg.hdr.dest_port = 0;
	clk_cfg.hdr.token = index;

	clk_cfg.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	clk_cfg.param.port_id = q6audio_get_port_id(port_id);
	clk_cfg.param.payload_address_lsw = 0x00;
	clk_cfg.param.payload_address_msw = 0x00;
	clk_cfg.param.mem_map_handle = 0x00;
	clk_cfg.pdata.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	clk_cfg.pdata.param_id = AFE_PARAM_ID_SPDIF_CLK_CONFIG;
	clk_cfg.pdata.param_size =  sizeof(clk_cfg.clk_cfg);
	clk_cfg.param.payload_size = sizeof(clk_cfg) - sizeof(struct apr_hdr)
		- sizeof(clk_cfg.param);
	clk_cfg.clk_cfg = *cfg;

	pr_debug("%s: Minor version = 0x%x clk val = %d\n"
			"clk root = 0x%x\n port id = 0x%x\n",
			__func__, cfg->clk_cfg_minor_version,
			cfg->clk_value, cfg->clk_root,
			q6audio_get_port_id(port_id));

	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &clk_cfg);
	if (ret < 0) {
		pr_err("%s: AFE send clock config for port 0x%x failed ret = %d\n",
				__func__, port_id, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_afe.wait[index],
			(atomic_read(&this_afe.state) == 0),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n",
				__func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}

fail_cmd:
	return ret;
}

int afe_send_spdif_ch_status_cfg(struct afe_param_id_spdif_ch_status_cfg
		*ch_status_cfg,	u16 port_id)
{
	struct afe_spdif_chstatus_config_command ch_status;
	int ret = 0;
	int index = 0;

	if (!ch_status_cfg) {
		pr_err("%s: Error, no configuration data\n", __func__);
		ret = -EINVAL;
		return ret;
	}
	index = q6audio_get_port_index(port_id);
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
	ch_status.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	ch_status.hdr.pkt_size = sizeof(ch_status_cfg);
	ch_status.hdr.src_port = 0;
	ch_status.hdr.dest_port = 0;
	ch_status.hdr.token = index;

	ch_status.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	ch_status.param.port_id = q6audio_get_port_id(port_id);
	ch_status.param.payload_address_lsw = 0x00;
	ch_status.param.payload_address_msw = 0x00;
	ch_status.param.mem_map_handle = 0x00;
	ch_status.pdata.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	ch_status.pdata.param_id = AFE_PARAM_ID_SPDIF_CLK_CONFIG;
	ch_status.pdata.param_size =  sizeof(ch_status.ch_status);
	ch_status.param.payload_size = sizeof(ch_status)
		- sizeof(struct apr_hdr) - sizeof(ch_status.param);
	ch_status.ch_status = *ch_status_cfg;

	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &ch_status);
	if (ret < 0) {
		pr_err("%s: AFE send channel status for port 0x%x failed ret = %d\n",
				__func__, port_id, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_afe.wait[index],
			(atomic_read(&this_afe.state) == 0),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n",
				__func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}

fail_cmd:
	return ret;
}



static int afe_send_cmd_port_start(u16 port_id)
{
	struct afe_port_cmd_device_start start;
	int ret, index;

	pr_debug("%s: enter\n", __func__);
	index = q6audio_get_port_index(port_id);
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
	if (ret) {
		pr_err("%s: AFE enable for port 0x%x failed %d\n", __func__,
		       port_id, ret);
	} else if (this_afe.task != current) {
		this_afe.task = current;
		pr_debug("task_name = %s pid = %d\n",
			 this_afe.task->comm, this_afe.task->pid);
	}

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

int afe_spdif_port_start(u16 port_id, struct afe_spdif_port_config *spdif_port,
		u32 rate)
{
	struct afe_audioif_config_command config;
	int ret = 0;
	int index = 0;
	uint16_t port_index;

	if (!spdif_port) {
		pr_err("%s: Error, no configuration data\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	pr_debug("%s: port id: 0x%x\n", __func__, port_id);

	index = q6audio_get_port_index(port_id);
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: port id: 0x%x ret %d\n", __func__, port_id, ret);
		return -EINVAL;
	}

	afe_send_cal(port_id);
	afe_send_hw_delay(port_id, rate);

	config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	config.hdr.pkt_size = sizeof(config);
	config.hdr.src_port = 0;
	config.hdr.dest_port = 0;
	config.hdr.token = index;
	config.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	config.param.port_id = q6audio_get_port_id(port_id);
	config.param.payload_size = sizeof(config) - sizeof(struct apr_hdr) -
		sizeof(config.param);
	config.param.payload_address_lsw = 0x00;
	config.param.payload_address_msw = 0x00;
	config.param.mem_map_handle = 0x00;
	config.pdata.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	config.pdata.param_id = AFE_PARAM_ID_SPDIF_CONFIG;
	config.pdata.param_size = sizeof(config.port);
	config.port.spdif = spdif_port->cfg;
	ret = afe_apr_send_pkt(&config, &this_afe.wait[index]);
	if (ret) {
		pr_err("%s: AFE enable for port 0x%x failed ret = %d\n",
				__func__, port_id, ret);
		goto fail_cmd;
	} else if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
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

	ret = afe_send_spdif_ch_status_cfg(&spdif_port->ch_status, port_id);
	if (ret < 0) {
		pr_err("%s: afe send failed %d\n", __func__, ret);
		goto fail_cmd;
	}

	return afe_send_cmd_port_start(port_id);

fail_cmd:
	return ret;
}

int afe_port_start(u16 port_id, union afe_port_config *afe_config,
	u32 rate) /* This function is no blocking */
{
	struct afe_audioif_config_command config;
	int ret = 0;
	int cfg_type;
	int index = 0;
	enum afe_mad_type mad_type;
	uint16_t port_index;

	if (!afe_config) {
		pr_err("%s: Error, no configuration data\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	if ((port_id == RT_PROXY_DAI_001_RX) ||
		(port_id == RT_PROXY_DAI_002_TX)) {
		pr_debug("%s: before incrementing pcm_afe_instance %d"\
			" port_id 0x%x\n", __func__,
			pcm_afe_instance[port_id & 0x1], port_id);
		port_id = VIRTUAL_ID_TO_PORTID(port_id);
		pcm_afe_instance[port_id & 0x1]++;
		return 0;
	}
	if ((port_id == RT_PROXY_DAI_002_RX) ||
			(port_id == RT_PROXY_DAI_001_TX)) {
		pr_debug("%s: before incrementing proxy_afe_instance %d"\
			" port_id 0x%x\n", __func__,
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

	afe_send_cal(port_id);
	afe_send_hw_delay(port_id, rate);

	/* Start SW MAD module */
	mad_type = afe_port_get_mad_type(port_id);
	pr_debug("%s: port_id 0x%x, mad_type %d\n", __func__, port_id,
		 mad_type);
	if (mad_type != MAD_HW_NONE && mad_type != MAD_SW_AUDIO) {
		if (!afe_has_config(AFE_CDC_REGISTERS_CONFIG) ||
			!afe_has_config(AFE_SLIMBUS_SLAVE_CONFIG)) {
				pr_err("%s: AFE isn't configured yet for\n"
					   "HW MAD try Again\n", __func__);
				return -EAGAIN;
		}
		ret = afe_turn_onoff_hw_mad(mad_type, true);
		if (ret) {
			pr_err("%s: afe_turn_onoff_hw_mad failed %d\n",
			       __func__, ret);
			return ret;
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
	config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	config.hdr.pkt_size = sizeof(config);
	config.hdr.src_port = 0;
	config.hdr.dest_port = 0;
	config.hdr.token = index;

	switch (port_id) {
	case AFE_PORT_ID_PRIMARY_PCM_RX:
	case AFE_PORT_ID_PRIMARY_PCM_TX:
	case AFE_PORT_ID_SECONDARY_PCM_RX:
	case AFE_PORT_ID_SECONDARY_PCM_TX:
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
		cfg_type = AFE_PARAM_ID_I2S_CONFIG;
		break;
	case HDMI_RX:
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
	case SLIMBUS_5_TX:
	case SLIMBUS_6_RX:
	case SLIMBUS_6_TX:
		cfg_type = AFE_PARAM_ID_SLIMBUS_CONFIG;
		break;
	case RT_PROXY_PORT_001_RX:
	case RT_PROXY_PORT_001_TX:
		cfg_type = AFE_PARAM_ID_RT_PROXY_CONFIG;
		break;
	case INT_BT_SCO_RX:
	case INT_BT_SCO_TX:
	case INT_FM_RX:
	case INT_FM_TX:
		cfg_type = AFE_PARAM_ID_INTERNAL_BT_FM_CONFIG;
		break;
	default:
		pr_err("%s: Invalid port id 0x%x\n", __func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	config.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	config.param.port_id = q6audio_get_port_id(port_id);
	config.param.payload_size = sizeof(config) - sizeof(struct apr_hdr) -
				    sizeof(config.param);
	config.param.payload_address_lsw = 0x00;
	config.param.payload_address_msw = 0x00;
	config.param.mem_map_handle = 0x00;
	config.pdata.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	config.pdata.param_id = cfg_type;
	config.pdata.param_size = sizeof(config.port);

	config.port = *afe_config;

	ret = afe_apr_send_pkt(&config, &this_afe.wait[index]);
	if (ret) {
		pr_err("%s: AFE enable for port 0x%x failed %d\n",
			__func__, port_id, ret);
		goto fail_cmd;
	} else if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
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
	return afe_send_cmd_port_start(port_id);

fail_cmd:
	return ret;
}

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
	case SECONDARY_I2S_RX: return IDX_SECONDARY_I2S_RX;
	case SECONDARY_I2S_TX: return IDX_SECONDARY_I2S_TX;
	case MI2S_RX: return IDX_MI2S_RX;
	case MI2S_TX: return IDX_MI2S_TX;
	case HDMI_RX: return IDX_HDMI_RX;
	case AFE_PORT_ID_SPDIF_RX: return IDX_SPDIF_RX;
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
	case SLIMBUS_5_TX: return IDX_SLIMBUS_5_TX;
	case SLIMBUS_6_RX: return IDX_SLIMBUS_6_RX;
	case SLIMBUS_6_TX: return IDX_SLIMBUS_6_TX;
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

	default:
		pr_err("%s: port 0x%x\n", __func__, port_id);
		return -EINVAL;
	}
}

int afe_open(u16 port_id,
		union afe_port_config *afe_config, int rate)
{
	struct afe_port_cmd_device_start start;
	struct afe_audioif_config_command config;
	int ret = 0;
	int cfg_type;
	int index = 0;

	if (!afe_config) {
		pr_err("%s: Error, no configuration data\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	pr_err("%s: port_id 0x%x rate %d\n", __func__, port_id, rate);

	index = q6audio_get_port_index(port_id);
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

	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: Failed : Invalid Port id = 0x%x ret %d\n",
			__func__, port_id, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	config.hdr.pkt_size = sizeof(config);
	config.hdr.src_port = 0;
	config.hdr.dest_port = 0;
	config.hdr.token = index;
	switch (port_id) {
	case PRIMARY_I2S_RX:
	case PRIMARY_I2S_TX:
		cfg_type = AFE_PARAM_ID_I2S_CONFIG;
		break;
	case AFE_PORT_ID_PRIMARY_PCM_RX:
	case AFE_PORT_ID_PRIMARY_PCM_TX:
	case AFE_PORT_ID_SECONDARY_PCM_RX:
	case AFE_PORT_ID_SECONDARY_PCM_TX:
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
		cfg_type = AFE_PARAM_ID_I2S_CONFIG;
		break;
	case HDMI_RX:
		cfg_type = AFE_PARAM_ID_HDMI_CONFIG;
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
	case SLIMBUS_6_RX:
	case SLIMBUS_6_TX:
		cfg_type = AFE_PARAM_ID_SLIMBUS_CONFIG;
		break;
	default:
		pr_err("%s: Invalid port id 0x%x\n",
			__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	config.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	config.param.port_id = q6audio_get_port_id(port_id);
	config.param.payload_size = sizeof(config) - sizeof(struct apr_hdr)
				 - sizeof(config.param);
	config.param.payload_address_lsw = 0x00;
	config.param.payload_address_msw = 0x00;
	config.param.mem_map_handle = 0x00;
	config.pdata.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	config.pdata.param_id = cfg_type;
	config.pdata.param_size =  sizeof(config.port);

	config.port = *afe_config;
	pr_debug("%s: param PL size=%d iparam_size[%d][%zd %zd %zd %zd] param_id[0x%x]\n",
		__func__, config.param.payload_size, config.pdata.param_size,
		sizeof(config), sizeof(config.param), sizeof(config.port),
		sizeof(struct apr_hdr), config.pdata.param_id);

	ret = afe_apr_send_pkt(&config, &this_afe.wait[index]);
	if (ret) {
		pr_err("%s: AFE enable for port 0x%x opcode[0x%x]failed %d\n",
			__func__, port_id, cfg_type, ret);
		goto fail_cmd;
	} else if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
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
	return ret;
}

int afe_loopback(u16 enable, u16 rx_port, u16 tx_port)
{
	struct afe_loopback_cfg_v1 lb_cmd;
	int ret = 0;
	int index = 0;

	if (rx_port == MI2S_RX)
		rx_port = AFE_PORT_ID_PRIMARY_MI2S_RX;
	if (tx_port == MI2S_TX)
		tx_port = AFE_PORT_ID_PRIMARY_MI2S_TX;

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	index = q6audio_get_port_index(rx_port);
	ret = q6audio_validate_port(rx_port);
	if (ret < 0) {
		pr_err("%s: Invalid port 0x%x ret %d", __func__, rx_port, ret);
		return -EINVAL;
	}

	lb_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(20), APR_PKT_VER);
	lb_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
						sizeof(lb_cmd) - APR_HDR_SIZE);
	lb_cmd.hdr.src_port = 0;
	lb_cmd.hdr.dest_port = 0;
	lb_cmd.hdr.token = index;
	lb_cmd.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	lb_cmd.param.port_id = tx_port;
	lb_cmd.param.payload_size = (sizeof(lb_cmd) - sizeof(struct apr_hdr) -
				     sizeof(struct afe_port_cmd_set_param_v2));
	lb_cmd.param.payload_address_lsw = 0x00;
	lb_cmd.param.payload_address_msw = 0x00;
	lb_cmd.param.mem_map_handle = 0x00;
	lb_cmd.pdata.module_id = AFE_MODULE_LOOPBACK;
	lb_cmd.pdata.param_id = AFE_PARAM_ID_LOOPBACK_CONFIG;
	lb_cmd.pdata.param_size = lb_cmd.param.payload_size -
				  sizeof(struct afe_port_param_data_v2);

	lb_cmd.dst_port_id = rx_port;
	lb_cmd.routing_mode = LB_MODE_DEFAULT;
	lb_cmd.enable = (enable ? 1 : 0);
	lb_cmd.loopback_cfg_minor_version = AFE_API_VERSION_LOOPBACK_CONFIG;

	ret = afe_apr_send_pkt(&lb_cmd, &this_afe.wait[index]);
	if (ret)
		pr_err("%s: AFE loopback failed %d\n", __func__, ret);
	return ret;
}

int afe_loopback_gain(u16 port_id, u16 volume)
{
	struct afe_loopback_gain_per_path_param set_param;
	int ret = 0;
	int index = 0;

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
	index = q6audio_get_port_index(port_id);
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: Invalid port 0x%x ret %d",
			__func__, port_id, ret);
		return -EINVAL;
	}

	/* RX ports numbers are even .TX ports numbers are odd. */
	if (port_id % 2 == 0) {
		pr_err("%s: Failed : afe loopback gain only for TX ports. port_id %d\n",
				__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	pr_debug("%s: port 0x%x volume %d\n", __func__, port_id, volume);

	set_param.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	set_param.hdr.pkt_size = sizeof(set_param);
	set_param.hdr.src_port = 0;
	set_param.hdr.dest_port = 0;
	set_param.hdr.token = index;
	set_param.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;

	set_param.param.port_id	= port_id;
	set_param.param.payload_size =
	    (sizeof(struct afe_loopback_gain_per_path_param) -
	     sizeof(struct apr_hdr) - sizeof(struct afe_port_cmd_set_param_v2));
	set_param.param.payload_address_lsw	= 0;
	set_param.param.payload_address_msw	= 0;
	set_param.param.mem_map_handle        = 0;

	set_param.pdata.module_id = AFE_MODULE_LOOPBACK;
	set_param.pdata.param_id = AFE_PARAM_ID_LOOPBACK_GAIN_PER_PATH;
	set_param.pdata.param_size =
	    (set_param.param.payload_size -
	     sizeof(struct afe_port_param_data_v2));
	set_param.rx_port_id = port_id;
	set_param.gain = volume;

	ret = afe_apr_send_pkt(&set_param, &this_afe.wait[index]);
	if (ret) {
		pr_err("%s: AFE param set failed for port 0x%x ret %d\n",
					__func__, port_id, ret);
		goto fail_cmd;
	}

fail_cmd:
	return ret;
}

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
		return -EINVAL;
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

int afe_port_group_set_param(u16 *port_id, int channel_count)
{
	int ret;
	struct afe_port_group_create config;

	pr_debug("%s: enter\n", __func__);

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	memset(&config, 0, sizeof(config));
	config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					     APR_HDR_LEN(APR_HDR_SIZE),
					     APR_PKT_VER);
	config.hdr.pkt_size = sizeof(config);
	config.hdr.src_port = 0;
	config.hdr.dest_port = 0;
	config.hdr.token = IDX_GLOBAL_CFG;
	config.hdr.opcode = AFE_SVC_CMD_SET_PARAM;

	config.param.payload_size = sizeof(config) - sizeof(struct apr_hdr) -
				    sizeof(config.param);
	config.param.payload_address_lsw = 0x00;
	config.param.payload_address_msw = 0x00;
	config.param.mem_map_handle = 0x00;
	config.pdata.module_id = AFE_MODULE_GROUP_DEVICE;
	config.pdata.param_id = AFE_PARAM_ID_GROUP_DEVICE_CFG;
	config.pdata.param_size = sizeof(struct afe_group_device_group_cfg);
	config.data.group_cfg.minor_version = 1;
	config.data.group_cfg.group_id = AFE_GROUP_DEVICE_ID_SECONDARY_MI2S_RX;
	config.data.group_cfg.port_id[0] = port_id[0];
	config.data.group_cfg.port_id[1] = port_id[1];
	config.data.group_cfg.port_id[2] = port_id[2];
	config.data.group_cfg.port_id[3] = port_id[3];
	config.data.group_cfg.port_id[4] = port_id[4];
	config.data.group_cfg.port_id[5] = port_id[5];
	config.data.group_cfg.port_id[6] = port_id[6];
	config.data.group_cfg.port_id[7] = port_id[7];
	config.data.group_cfg.num_channels = channel_count;

	ret = afe_apr_send_pkt(&config, &this_afe.wait[IDX_GLOBAL_CFG]);
	if (ret)
		pr_err("%s: AFE_PARAM_ID_GROUP_DEVICE_CFG failed %d\n",
			__func__, ret);
	return ret;
}

int afe_port_group_enable(u16 enable)
{
	int ret;
	struct afe_port_group_create config;

	pr_debug("%s: enter\n", __func__);
	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	memset(&config, 0, sizeof(config));
	config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					     APR_HDR_LEN(APR_HDR_SIZE),
					     APR_PKT_VER);
	config.hdr.pkt_size = sizeof(config);
	config.hdr.src_port = 0;
	config.hdr.dest_port = 0;
	config.hdr.token = IDX_GLOBAL_CFG;
	config.hdr.opcode = AFE_SVC_CMD_SET_PARAM;

	config.param.payload_size = sizeof(config) - sizeof(struct apr_hdr) -
				    sizeof(config.param);
	config.param.payload_address_lsw = 0x00;
	config.param.payload_address_msw = 0x00;
	config.param.mem_map_handle = 0x00;
	config.pdata.module_id = AFE_MODULE_GROUP_DEVICE;
	config.pdata.param_id = AFE_PARAM_ID_GROUP_DEVICE_ENABLE;
	config.pdata.param_size = sizeof(struct afe_group_device_enable);
	config.data.group_enable.group_id =
			AFE_GROUP_DEVICE_ID_SECONDARY_MI2S_RX;
	config.data.group_enable.enable = enable;

	ret = afe_apr_send_pkt(&config, &this_afe.wait[IDX_GLOBAL_CFG]);
	if (ret)
		pr_err("%s: AFE_PARAM_ID_ENABLE failed %d\n", __func__,
		       ret);
	return ret;
}

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

uint32_t afe_req_mmap_handle(struct afe_audio_client *ac)
{
	return ac->mem_map_handle;
}

struct afe_audio_client *q6afe_audio_client_alloc(void *priv)
{
	struct afe_audio_client *ac;
	int lcnt = 0;

	ac = kzalloc(sizeof(struct afe_audio_client), GFP_KERNEL);
	if (!ac) {
		pr_err("%s: cannot allocate audio client for afe\n", __func__);
		return NULL;
	}
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
		pr_err("%s: ac %p dir %d\n", __func__, ac, dir);
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

	rc = msm_audio_ion_alloc("audio_client", &buf[0].client,
				&buf[0].handle, bufsz*bufcnt,
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
			pr_debug("%s:  data[%p]phys[%pa][%p]\n", __func__,
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

int afe_memory_map(phys_addr_t dma_addr_p, u32 dma_buf_sz,
			struct afe_audio_client *ac)
{
	int ret = 0;

	mutex_lock(&this_afe.mem_map_lock);
	ac->mem_map_handle = 0;
	ret = afe_cmd_memory_map(dma_addr_p, dma_buf_sz);
	if (ret < 0) {
		pr_err("%s: afe_cmd_memory_map failed %d\n",
			__func__, ret);

		mutex_unlock(&this_afe.mem_map_lock);
		return ret;
	}
	ac->mem_map_handle = this_afe.mmap_handle;
	mutex_unlock(&this_afe.mem_map_lock);

	return ret;
}

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

	cmd_size = sizeof(struct afe_service_cmd_shared_mem_map_regions) \
		+ sizeof(struct afe_service_shared_map_region_payload);

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (!mmap_region_cmd) {
		pr_err("%s: allocate mmap_region_cmd failed\n", __func__);
		return -ENOMEM;
	}

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
	mregion_pl->shm_addr_msw = upper_32_bits(dma_addr_p);
	mregion_pl->mem_size_bytes = dma_buf_sz;

	pr_debug("%s: dma_addr_p 0x%pa , size %d\n", __func__,
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
	if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: Memory map cmd failed\n", __func__);
		ret = -EINVAL;
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
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: Invalid port 0x%x ret %d",
			__func__, port_id, ret);
		return -EINVAL;
	}

	cmd_size = sizeof(struct afe_service_cmd_shared_mem_map_regions)
		+ sizeof(struct afe_service_shared_map_region_payload);

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (!mmap_region_cmd) {
		pr_err("%s: allocate mmap_region_cmd failed\n", __func__);
		return -ENOMEM;
	}
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
	mregion_pl->shm_addr_msw = upper_32_bits(dma_addr_p);
	mregion_pl->mem_size_bytes = dma_buf_sz;

	ret = afe_apr_send_pkt(mmap_region_cmd, NULL);
	if (ret)
		pr_err("%s: AFE memory map cmd failed %d\n",
		       __func__, ret);
	kfree(mmap_region_cmd);
	return ret;
}
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
		pr_debug("%s: data[%p]phys[%pa][%p] , client[%p] handle[%p]\n",
			__func__,
			port->buf[0].data,
			&port->buf[0].phys,
			&port->buf[0].phys,
			port->buf[0].client,
			port->buf[0].handle);
		msm_audio_ion_free(port->buf[0].client, port->buf[0].handle);
		port->buf[0].client = NULL;
		port->buf[0].handle = NULL;
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
	return;
}

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
	if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: Memory unmap cmd failed\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

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

int afe_register_get_events(u16 port_id,
		void (*cb) (uint32_t opcode,
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

int afe_rt_proxy_port_write(u32 buf_addr_p, u32 mem_map_handle, int bytes)
{
	int ret = 0;
	struct afe_port_data_cmd_rt_proxy_port_write_v2 afecmd_wr;

	if (this_afe.apr == NULL) {
		pr_err("%s: register to AFE is not done\n", __func__);
		ret = -ENODEV;
		return ret;
	}
	pr_debug("%s: buf_addr_p = 0x%x bytes = %d\n", __func__,
						buf_addr_p, bytes);

	afecmd_wr.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	afecmd_wr.hdr.pkt_size = sizeof(afecmd_wr);
	afecmd_wr.hdr.src_port = 0;
	afecmd_wr.hdr.dest_port = 0;
	afecmd_wr.hdr.token = 0;
	afecmd_wr.hdr.opcode = AFE_PORT_DATA_CMD_RT_PROXY_PORT_WRITE_V2;
	afecmd_wr.port_id = RT_PROXY_PORT_001_TX;
	afecmd_wr.buffer_address_lsw = lower_32_bits(buf_addr_p);
	afecmd_wr.buffer_address_msw = upper_32_bits(buf_addr_p);
	afecmd_wr.mem_map_handle = mem_map_handle;
	afecmd_wr.available_bytes = bytes;
	afecmd_wr.reserved = 0;

	ret = afe_apr_send_pkt(&afecmd_wr, NULL);
	if (ret)
		pr_err("%s: AFE rtproxy write to port 0x%x failed %d\n",
			   __func__, afecmd_wr.port_id, ret);
	return ret;

}

int afe_rt_proxy_port_read(u32 buf_addr_p, u32 mem_map_handle, int bytes)
{
	int ret = 0;
	struct afe_port_data_cmd_rt_proxy_port_read_v2 afecmd_rd;

	if (this_afe.apr == NULL) {
		pr_err("%s: register to AFE is not done\n", __func__);
		ret = -ENODEV;
		return ret;
	}
	pr_debug("%s: buf_addr_p = 0x%x bytes = %d\n", __func__,
						buf_addr_p, bytes);

	afecmd_rd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	afecmd_rd.hdr.pkt_size = sizeof(afecmd_rd);
	afecmd_rd.hdr.src_port = 0;
	afecmd_rd.hdr.dest_port = 0;
	afecmd_rd.hdr.token = 0;
	afecmd_rd.hdr.opcode = AFE_PORT_DATA_CMD_RT_PROXY_PORT_READ_V2;
	afecmd_rd.port_id = RT_PROXY_PORT_001_RX;
	afecmd_rd.buffer_address_lsw = lower_32_bits(buf_addr_p);
	afecmd_rd.buffer_address_msw = upper_32_bits(buf_addr_p);
	afecmd_rd.available_bytes = bytes;
	afecmd_rd.mem_map_handle = mem_map_handle;

	ret = afe_apr_send_pkt(&afecmd_rd, NULL);
	if (ret)
		pr_err("%s: AFE rtproxy read  cmd to port 0x%x failed %d\n",
			   __func__, afecmd_rd.port_id, ret);
	return ret;
}

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

	if (!strncmp(lb_str, "afe_loopback", 12)) {
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

	} else if (!strncmp(lb_str, "afe_loopback_gain", 17)) {
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

			if (param[1] < 0 || param[1] > 100) {
				pr_err("%s: Error, volume shoud be 0 to 100 percentage param = %lu\n",
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
	S_IRUGO | S_IWUSR | S_IWGRP, NULL, (void *) "afe_loopback",
	&afe_debug_fops);

	debugfs_afelb_gain = debugfs_create_file("afe_loopback_gain",
	S_IRUGO | S_IWUSR | S_IWGRP, NULL, (void *) "afe_loopback_gain",
	&afe_debug_fops);
}
static void config_debug_fs_exit(void)
{
	if (debugfs_afelb)
		debugfs_remove(debugfs_afelb);
	if (debugfs_afelb_gain)
		debugfs_remove(debugfs_afelb_gain);
}
#else
static void config_debug_fs_init(void)
{
	return;
}
static void config_debug_fs_exit(void)
{
	return;
}
#endif

void afe_set_dtmf_gen_rx_portid(u16 port_id, int set)
{
	if (set)
		this_afe.dtmf_gen_rx_portid = port_id;
	else if (this_afe.dtmf_gen_rx_portid == port_id)
		this_afe.dtmf_gen_rx_portid = -1;
}

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
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &cmd_dtmf);
	if (ret < 0) {
		pr_err("%s: AFE DTMF failed for num_ports:%d ids:0x%x\n",
		       __func__, cmd_dtmf.num_ports, cmd_dtmf.port_ids);
		ret = -EINVAL;
		goto fail_cmd;
	}
	index = q6audio_get_port_index(this_afe.dtmf_gen_rx_portid);
	ret = wait_event_timeout(this_afe.wait[index],
		(atomic_read(&this_afe.state) == 0),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	return 0;

fail_cmd:
	pr_err("%s: failed %d\n", __func__, ret);
	return ret;
}

int afe_sidetone(u16 tx_port_id, u16 rx_port_id, u16 enable, uint16_t gain)
{
	struct afe_loopback_cfg_v1 cmd_sidetone;
	int ret = 0;
	int index = 0;

	pr_info("%s: tx_port_id: 0x%x rx_port_id: 0x%x enable:%d gain:%d\n",
			__func__, tx_port_id, rx_port_id, enable, gain);
	index = q6audio_get_port_index(rx_port_id);
	ret = q6audio_validate_port(rx_port_id);
	if (ret < 0) {
		pr_err("%s: Invalid port 0x%x %d", __func__, rx_port_id, ret);
		return -EINVAL;
	}

	cmd_sidetone.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cmd_sidetone.hdr.pkt_size = sizeof(cmd_sidetone);
	cmd_sidetone.hdr.src_port = 0;
	cmd_sidetone.hdr.dest_port = 0;
	cmd_sidetone.hdr.token = 0;
	cmd_sidetone.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	/* should it be rx or tx port id ?? , bharath*/
	cmd_sidetone.param.port_id = tx_port_id;
	/* size of data param & payload */
	cmd_sidetone.param.payload_size = (sizeof(cmd_sidetone) -
			sizeof(struct apr_hdr) -
			sizeof(struct afe_port_cmd_set_param_v2));
	cmd_sidetone.param.payload_address_lsw = 0x00;
	cmd_sidetone.param.payload_address_msw = 0x00;
	cmd_sidetone.param.mem_map_handle = 0x00;
	cmd_sidetone.pdata.module_id = AFE_MODULE_LOOPBACK;
	cmd_sidetone.pdata.param_id = AFE_PARAM_ID_LOOPBACK_CONFIG;
	/* size of actual payload only */
	cmd_sidetone.pdata.param_size =  cmd_sidetone.param.payload_size -
				sizeof(struct afe_port_param_data_v2);

	cmd_sidetone.loopback_cfg_minor_version =
					AFE_API_VERSION_LOOPBACK_CONFIG;
	cmd_sidetone.dst_port_id = rx_port_id;
	cmd_sidetone.routing_mode = LB_MODE_SIDETONE;
	cmd_sidetone.enable = enable;

	ret = afe_apr_send_pkt(&cmd_sidetone, &this_afe.wait[index]);
	if (ret)
		pr_err("%s: sidetone failed tx_port:0x%x rx_port:0x%x ret%d\n",
		__func__, tx_port_id, rx_port_id, ret);
	return ret;
}

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
	case SECONDARY_I2S_RX:
	case SECONDARY_I2S_TX:
	case MI2S_RX:
	case MI2S_TX:
	case HDMI_RX:
	case AFE_PORT_ID_SPDIF_RX:
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
	case SLIMBUS_6_RX:
	case SLIMBUS_6_TX:
	case AFE_PORT_ID_PRIMARY_MI2S_RX:
	case AFE_PORT_ID_PRIMARY_MI2S_TX:
	case AFE_PORT_ID_QUATERNARY_MI2S_RX:
	case AFE_PORT_ID_QUATERNARY_MI2S_TX:
	case AFE_PORT_ID_TERTIARY_MI2S_TX:
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
		if (!(pcm_afe_instance[port_id & 0x1] == 0 &&
			proxy_afe_instance[port_id & 0x1] == 0))
			return 0;
		else
			afe_close_done[port_id & 0x1] = true;
	}

	if ((port_id == RT_PROXY_DAI_002_RX) ||
		(port_id == RT_PROXY_DAI_001_TX)) {
		pr_debug("%s: before decrementing proxy_afe_instance %d\n",
			__func__, proxy_afe_instance[port_id & 0x1]);
		port_id = VIRTUAL_ID_TO_PORTID(port_id);
		proxy_afe_instance[port_id & 0x1]--;
		if (!(pcm_afe_instance[port_id & 0x1] == 0 &&
			proxy_afe_instance[port_id & 0x1] == 0))
			return 0;
		else
			afe_close_done[port_id & 0x1] = true;
	}

	port_id = q6audio_convert_virtual_to_portid(port_id);
	index = q6audio_get_port_index(port_id);
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

fail_cmd:
	return ret;
}

int afe_set_digital_codec_core_clock(u16 port_id,
				struct afe_digital_clk_cfg *cfg)
{
	struct afe_lpass_digital_clk_config_command clk_cfg;
	int index = 0;
	int ret = 0;

	if (!cfg) {
		pr_err("%s: clock cfg is NULL\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	clk_cfg.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	clk_cfg.hdr.pkt_size = sizeof(clk_cfg);
	clk_cfg.hdr.src_port = 0;
	clk_cfg.hdr.dest_port = 0;
	clk_cfg.hdr.token = index;

	clk_cfg.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	/*default rx port is taken to enable the codec digital clock*/
	clk_cfg.param.port_id = q6audio_get_port_id(port_id);
	clk_cfg.param.payload_size = sizeof(clk_cfg) - sizeof(struct apr_hdr)
						- sizeof(clk_cfg.param);
	clk_cfg.param.payload_address_lsw = 0x00;
	clk_cfg.param.payload_address_msw = 0x00;
	clk_cfg.param.mem_map_handle = 0x00;
	clk_cfg.pdata.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	clk_cfg.pdata.param_id = AFE_PARAM_ID_INTERNAL_DIGIATL_CDC_CLK_CONFIG;
	clk_cfg.pdata.param_size =  sizeof(clk_cfg.clk_cfg);
	clk_cfg.clk_cfg = *cfg;

	pr_debug("%s: Minor version =0x%x clk val = %d\n"
		 "clk root = 0x%x resrv = 0x%x\n",
		 __func__, cfg->i2s_cfg_minor_version,
		 cfg->clk_val, cfg->clk_root, cfg->reserved);

	atomic_set(&this_afe.state, 1);
	atomic_set(&this_afe.status, 0);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &clk_cfg);
	if (ret < 0) {
		pr_err("%s: AFE enable for port 0x%x ret %d\n",
		       __func__, port_id, ret);
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
	if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}

fail_cmd:
	return ret;
}

int afe_set_lpass_clock(u16 port_id, struct afe_clk_cfg *cfg)
{
	struct afe_lpass_clk_config_command clk_cfg;
	int index = 0;
	int ret = 0;

	if (!cfg) {
		pr_err("%s: clock cfg is NULL\n", __func__);
		ret = -EINVAL;
		return ret;
	}
	index = q6audio_get_port_index(port_id);
	ret = q6audio_is_digital_pcm_interface(port_id);
	if (ret < 0) {
		pr_err("%s: q6audio_is_digital_pcm_interface fail %d\n",
			__func__, ret);
		return -EINVAL;
	}

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	clk_cfg.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	clk_cfg.hdr.pkt_size = sizeof(clk_cfg);
	clk_cfg.hdr.src_port = 0;
	clk_cfg.hdr.dest_port = 0;
	clk_cfg.hdr.token = index;

	clk_cfg.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	clk_cfg.param.port_id = q6audio_get_port_id(port_id);
	clk_cfg.param.payload_size = sizeof(clk_cfg) - sizeof(struct apr_hdr)
						- sizeof(clk_cfg.param);
	clk_cfg.param.payload_address_lsw = 0x00;
	clk_cfg.param.payload_address_msw = 0x00;
	clk_cfg.param.mem_map_handle = 0x00;
	clk_cfg.pdata.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	clk_cfg.pdata.param_id = AFE_PARAM_ID_LPAIF_CLK_CONFIG;
	clk_cfg.pdata.param_size =  sizeof(clk_cfg.clk_cfg);
	clk_cfg.clk_cfg = *cfg;

	pr_debug("%s: Minor version =0x%x clk val1 = %d\n"
		 "clk val2 = %d, clk src = 0x%x\n"
		 "clk root = 0x%x clk mode = 0x%x resrv = 0x%x\n"
		 "port id = 0x%x\n",
		 __func__, cfg->i2s_cfg_minor_version,
		 cfg->clk_val1, cfg->clk_val2, cfg->clk_src,
		 cfg->clk_root, cfg->clk_set_mode,
		 cfg->reserved, q6audio_get_port_id(port_id));

	atomic_set(&this_afe.state, 1);
	atomic_set(&this_afe.status, 0);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &clk_cfg);
	if (ret < 0) {
		pr_err("%s: AFE enable for port 0x%x ret %d\n",
		       __func__, port_id, ret);
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
	if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}

fail_cmd:
	return ret;
}

int afe_set_lpass_internal_digital_codec_clock(u16 port_id,
			struct afe_digital_clk_cfg *cfg)
{
	struct afe_lpass_digital_clk_config_command clk_cfg;
	int index = 0;
	int ret = 0;

	if (!cfg) {
		pr_err("%s: clock cfg is NULL\n", __func__);
		ret = -EINVAL;
		return ret;
	}
	index = q6audio_get_port_index(port_id);
	ret = q6audio_is_digital_pcm_interface(port_id);
	if (ret < 0) {
		pr_err("%s: q6audio_is_digital_pcm_interface fail %d\n",
			__func__, ret);
		return -EINVAL;
	}

	ret = afe_q6_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	clk_cfg.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	clk_cfg.hdr.pkt_size = sizeof(clk_cfg);
	clk_cfg.hdr.src_port = 0;
	clk_cfg.hdr.dest_port = 0;
	clk_cfg.hdr.token = index;

	clk_cfg.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	clk_cfg.param.port_id = q6audio_get_port_id(port_id);
	clk_cfg.param.payload_size = sizeof(clk_cfg) - sizeof(struct apr_hdr)
						- sizeof(clk_cfg.param);
	clk_cfg.param.payload_address_lsw = 0x00;
	clk_cfg.param.payload_address_msw = 0x00;
	clk_cfg.param.mem_map_handle = 0x00;
	clk_cfg.pdata.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	clk_cfg.pdata.param_id = AFE_PARAM_ID_INTERNAL_DIGIATL_CDC_CLK_CONFIG;
	clk_cfg.pdata.param_size =  sizeof(clk_cfg.clk_cfg);
	clk_cfg.clk_cfg = *cfg;

	pr_debug("%s: Minor version =0x%x clk val = %d\n"
		 "clk root = 0x%x resrv = 0x%x port id = 0x%x\n",
		 __func__, cfg->i2s_cfg_minor_version,
		 cfg->clk_val, cfg->clk_root, cfg->reserved,
		 q6audio_get_port_id(port_id));

	atomic_set(&this_afe.state, 1);
	atomic_set(&this_afe.status, 0);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &clk_cfg);
	if (ret < 0) {
		pr_err("%s: AFE enable for port 0x0x%x ret %d\n",
		       __func__, port_id, ret);
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
	if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}

fail_cmd:
	return ret;
}

int q6afe_check_osr_clk_freq(u32 freq)
{
	int ret = 0;
	switch (freq) {
	case Q6AFE_LPASS_OSR_CLK_12_P288_MHZ:
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
		pr_err("%s: deafult freq 0x%x\n",
			__func__, freq);
		ret = -EINVAL;
	}
	return ret;
}

int afe_spk_prot_get_calib_data(struct afe_spkr_prot_get_vi_calib *calib_resp)
{
	int ret = -EINVAL;
	int index = 0, port = SLIMBUS_4_TX;

	if (!calib_resp) {
		pr_err("%s: Invalid params\n", __func__);
		goto fail_cmd;
	}
	ret = q6audio_validate_port(port);
	if (ret < 0) {
		pr_err("%s: invalid port 0x%x ret %d\n", __func__, port, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}
	index = q6audio_get_port_index(port);
	calib_resp->hdr.hdr_field =
	APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
	APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	calib_resp->hdr.pkt_size = sizeof(*calib_resp);
	calib_resp->hdr.src_port = 0;
	calib_resp->hdr.dest_port = 0;
	calib_resp->hdr.token = index;
	calib_resp->hdr.opcode =  AFE_PORT_CMD_GET_PARAM_V2;
	calib_resp->get_param.mem_map_handle = 0;
	calib_resp->get_param.module_id = AFE_MODULE_FB_SPKR_PROT_VI_PROC;
	calib_resp->get_param.param_id = AFE_PARAM_ID_CALIB_RES_CFG;
	calib_resp->get_param.payload_address_lsw = 0;
	calib_resp->get_param.payload_address_msw = 0;
	calib_resp->get_param.payload_size = sizeof(*calib_resp)
		- sizeof(calib_resp->get_param) - sizeof(calib_resp->hdr);
	calib_resp->get_param.port_id = q6audio_get_port_id(port);
	calib_resp->pdata.module_id = AFE_MODULE_FB_SPKR_PROT_VI_PROC;
	calib_resp->pdata.param_id = AFE_PARAM_ID_CALIB_RES_CFG;
	calib_resp->pdata.param_size = sizeof(calib_resp->res_cfg);
	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *)calib_resp);
	if (ret < 0) {
		pr_err("%s: get param port 0x%x param id[0x%x]failed %d\n",
			   __func__, port, calib_resp->get_param.param_id, ret);
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
	if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	memcpy(&calib_resp->res_cfg , &this_afe.calib_data.res_cfg,
		sizeof(this_afe.calib_data.res_cfg));
	pr_debug("%s: state %d resistance %d\n", __func__,
			 calib_resp->res_cfg.th_vi_ca_state,
			 calib_resp->res_cfg.r0_cali_q24);
	ret = 0;
fail_cmd:
	return ret;
}

int afe_spk_prot_feed_back_cfg(int src_port, int dst_port,
	int l_ch, int r_ch, u32 enable)
{
	int ret = -EINVAL;
	union afe_spkr_prot_config prot_config;
	int index = 0;

	if (!enable) {
		pr_debug("%s: Disable Feedback tx path", __func__);
		this_afe.vi_tx_port = -1;
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
	prot_config.feedback_path_cfg.minor_version = 1;
	ret = afe_spk_prot_prepare(src_port,
			AFE_PARAM_ID_FEEDBACK_PATH_CFG, &prot_config);
fail_cmd:
	return ret;
}

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
	pr_debug("%s:\n", __func__);

	cal_index = get_cal_type_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}

	ret = cal_utils_alloc_cal(data_size, data,
		this_afe.cal_data[cal_index], 0, NULL);
	if (ret < 0) {
		pr_err("%s: cal_utils_alloc_block failed, ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
		ret = -EINVAL;
		goto done;
	}
done:
	return ret;
}

static int afe_dealloc_cal(int32_t cal_type, size_t data_size,
							void *data)
{
	int				ret = 0;
	int				cal_index;
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
	int				ret = 0;
	int				cal_index;
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
done:
	return ret;
}

static struct cal_block_data *afe_find_hw_delay_by_path(
			struct cal_type_data *cal_type, int path)
{
	struct list_head		*ptr, *next;
	struct cal_block_data		*cal_block = NULL;
	pr_debug("%s:\n", __func__);

	list_for_each_safe(ptr, next,
		&cal_type->cal_blocks) {

		cal_block = list_entry(ptr,
			struct cal_block_data, list);

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
	pr_debug("%s: Path = %d samplerate = %u usec = %u status %d\n",
		 __func__, path, entry->sample_rate, entry->delay_usec, ret);
unlock:
	mutex_unlock(&this_afe.cal_data[AFE_HW_DELAY_CAL]->lock);
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

	mutex_lock(&this_afe.cal_data[AFE_FB_SPKR_PROT_CAL]->lock);
	memcpy(&this_afe.prot_cfg, &cal_data->cal_info,
		sizeof(this_afe.prot_cfg));
	mutex_unlock(&this_afe.cal_data[AFE_FB_SPKR_PROT_CAL]->lock);
done:
	return ret;
}

static int afe_get_cal_fb_spkr_prot(int32_t cal_type, size_t data_size,
								void *data)
{
	int ret = 0;
	struct audio_cal_type_fb_spk_prot_status	*cal_data = data;
	struct afe_spkr_prot_get_vi_calib		calib_resp;
	pr_debug("%s:\n", __func__);

	if (this_afe.cal_data[AFE_FB_SPKR_PROT_CAL] == NULL)
		goto done;
	if (cal_data == NULL)
		goto done;
	if (data_size != sizeof(*cal_data))
		goto done;

	mutex_lock(&this_afe.cal_data[AFE_FB_SPKR_PROT_CAL]->lock);
	if (this_afe.prot_cfg.mode == MSM_SPKR_PROT_CALIBRATED) {
			cal_data->cal_info.r0 = this_afe.prot_cfg.r0;
			cal_data->cal_info.status = 0;
	} else if (this_afe.prot_cfg.mode ==
				MSM_SPKR_PROT_CALIBRATION_IN_PROGRESS) {
		/*Call AFE to query the status*/
		cal_data->cal_info.status = -EINVAL;
		cal_data->cal_info.r0 = -1;
		if (!afe_spk_prot_get_calib_data(&calib_resp)) {
			if (calib_resp.res_cfg.th_vi_ca_state == 1)
				cal_data->cal_info.status = -EAGAIN;
			else if (calib_resp.res_cfg.th_vi_ca_state == 2) {
				cal_data->cal_info.status = 0;
				cal_data->cal_info.r0 =
					calib_resp.res_cfg.r0_cali_q24;
			}
		}
		if (!cal_data->cal_info.status) {
			this_afe.prot_cfg.mode =
				MSM_SPKR_PROT_CALIBRATED;
			this_afe.prot_cfg.r0 = cal_data->cal_info.r0;
		}
	} else {
		/*Indicates calibration data is invalid*/
		cal_data->cal_info.status = -EINVAL;
		cal_data->cal_info.r0 = -1;
	}
	mutex_unlock(&this_afe.cal_data[AFE_FB_SPKR_PROT_CAL]->lock);
done:
	return ret;
}

static bool afe_match_hw_delay_by_path(struct cal_block_data *cal_block,
							void *user_data)
{
	struct audio_cal_info_hw_delay	*block_cal_info = cal_block->cal_info;
	struct audio_cal_type_hw_delay	*data = user_data;
	pr_debug("%s:\n", __func__);

	if (block_cal_info->path == data->cal_info.path)
		return true;

	return false;
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
		pr_debug("%s: mmap did not work! addr = 0x%pa, size = %zd\n",
			__func__,
			&cal_block->cal_data.paddr,
			cal_block->map_data.map_size);
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

	return;
}

static int afe_init_cal_data(void)
{
	int ret = 0;
	struct cal_type_info	cal_type_info[] = {
		{{AFE_COMMON_RX_CAL_TYPE,
		{afe_alloc_cal, afe_dealloc_cal, NULL,
		afe_set_cal, NULL, NULL} },
		{afe_map_cal_data, afe_unmap_cal_data,
		cal_utils_match_ion_map} },

		{{AFE_COMMON_TX_CAL_TYPE,
		{afe_alloc_cal, afe_dealloc_cal, NULL,
		afe_set_cal, NULL, NULL} },
		{afe_map_cal_data, afe_unmap_cal_data,
		cal_utils_match_ion_map} },

		{{AFE_AANC_CAL_TYPE,
		{afe_alloc_cal, afe_dealloc_cal, NULL,
		afe_set_cal, NULL, NULL} },
		{afe_map_cal_data, afe_unmap_cal_data,
		cal_utils_match_ion_map} },

		{{AFE_FB_SPKR_PROT_CAL_TYPE,
		{NULL, NULL, NULL, afe_set_cal_fb_spkr_prot,
		afe_get_cal_fb_spkr_prot, NULL} },
		{NULL, NULL, cal_utils_match_only_block} },

		{{AFE_HW_DELAY_CAL_TYPE,
		{NULL, NULL, NULL,
		afe_set_cal, NULL, NULL} },
		{NULL, NULL, afe_match_hw_delay_by_path} },

		{{AFE_SIDETONE_CAL_TYPE,
		{NULL, NULL, NULL,
		afe_set_cal, NULL, NULL} },
		{NULL, NULL, cal_utils_match_only_block} }
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
	int	result = 0;
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
		pr_err("%s: afe_cmd_memory_map failed for addr = 0x%pa, size = %d, err %d\n",
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
	int	result = 0;
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

static int __init afe_init(void)
{
	int i = 0, ret;

	atomic_set(&this_afe.state, 0);
	atomic_set(&this_afe.status, 0);
	atomic_set(&this_afe.mem_map_cal_index, -1);
	this_afe.apr = NULL;
	this_afe.dtmf_gen_rx_portid = -1;
	this_afe.mmap_handle = 0;
	this_afe.vi_tx_port = -1;
	this_afe.prot_cfg.mode = MSM_SPKR_PROT_DISABLED;
	mutex_init(&this_afe.mem_map_lock);
	for (i = 0; i < AFE_MAX_PORTS; i++)
		init_waitqueue_head(&this_afe.wait[i]);

	ret = afe_init_cal_data();
	if (ret)
		pr_err("%s: could not init cal data! %d\n", __func__, ret);

	config_debug_fs_init();
	return 0;
}

static void __exit afe_exit(void)
{
	afe_delete_cal_data();

	config_debug_fs_exit();
}

device_initcall(afe_init);
__exitcall(afe_exit);
