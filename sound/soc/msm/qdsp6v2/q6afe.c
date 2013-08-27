/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include "audio_acdb.h"

enum {
	AFE_RX_CAL,
	AFE_TX_CAL,
	AFE_AANC_TX_CAL,
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

	struct acdb_cal_block afe_cal_addr[MAX_AFE_CAL_TYPES];
	atomic_t mem_map_cal_handles[MAX_AFE_CAL_TYPES];
	atomic_t mem_map_cal_index;
	u16 dtmf_gen_rx_portid;
	struct afe_spkr_prot_calib_get_resp calib_data;
	int vi_tx_port;
	uint32_t afe_sample_rates[AFE_MAX_PORTS];
	struct aanc_data aanc_info;
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

void afe_set_aanc_info(struct aanc_data *q6_aanc_info)
{
	pr_debug("%s\n", __func__);

	this_afe.aanc_info.aanc_active = q6_aanc_info->aanc_active;
	this_afe.aanc_info.aanc_rx_port = q6_aanc_info->aanc_rx_port;
	this_afe.aanc_info.aanc_tx_port = q6_aanc_info->aanc_tx_port;

	pr_debug("aanc active is %d rx port is %d, tx port is %d\n",
		this_afe.aanc_info.aanc_active,
		this_afe.aanc_info.aanc_rx_port,
		this_afe.aanc_info.aanc_tx_port);
}


static int32_t afe_callback(struct apr_client_data *data, void *priv)
{
	int i;

	if (!data) {
		pr_err("%s: Invalid param data\n", __func__);
		return -EINVAL;
	}
	if (data->opcode == RESET_EVENTS) {
		pr_debug("q6afe: reset event = %d %d apr[%p]\n",
			data->reset_event, data->reset_proc, this_afe.apr);

		for (i = 0; i < MAX_AFE_CAL_TYPES; i++) {
			this_afe.afe_cal_addr[i].cal_paddr = 0;
			this_afe.afe_cal_addr[i].cal_size = 0;
		}

		if (this_afe.apr) {
			apr_reset(this_afe.apr);
			atomic_set(&this_afe.state, 0);
			this_afe.apr = NULL;
		}
		/* send info to user */
		pr_debug("task_name = %s pid = %d\n",
			this_afe.task->comm, this_afe.task->pid);
		return 0;
	}
	pr_debug("%s:opcode = 0x%x cmd = 0x%x status = 0x%x size = %d\n",
			__func__, data->opcode,
			((uint32_t *)(data->payload))[0],
			((uint32_t *)(data->payload))[1],
			 data->payload_size);
	if (data->opcode == AFE_PORT_CMDRSP_GET_PARAM_V2) {
		u8 *payload = data->payload;
		if ((data->payload_size < sizeof(this_afe.calib_data))
			|| !payload || (data->token >= AFE_MAX_PORTS)) {
			pr_err("%s size %d payload %p token %d\n",
			__func__, data->payload_size, payload, data->token);
			return -EINVAL;
		}
		memcpy(&this_afe.calib_data, payload,
			   sizeof(this_afe.calib_data));
		if (!this_afe.calib_data.status) {
			atomic_set(&this_afe.state, 0);
			pr_err("%s rest %d state %x\n" , __func__
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
			case AFE_PORT_CMD_DEVICE_STOP:
			case AFE_PORT_CMD_DEVICE_START:
			case AFE_PORT_CMD_SET_PARAM_V2:
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
				pr_err("%s:Unknown cmd 0x%x\n", __func__,
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
				this_afe.mmap_handle = (uint32_t)payload[0];
			atomic_set(&this_afe.state, 0);
			wake_up(&this_afe.wait[data->token]);
		} else if (data->opcode == AFE_EVENT_RT_PROXY_PORT_STATUS) {
			port_id = (uint16_t)(0x0000FFFF & payload[0]);
		}
		pr_debug("%s:port_id = %x\n", __func__, port_id);
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
	case SLIMBUS_0_RX:
	case SLIMBUS_1_RX:
	case SLIMBUS_2_RX:
	case SLIMBUS_3_RX:
	case SLIMBUS_4_RX:
	case INT_BT_SCO_RX:
	case INT_BT_A2DP_RX:
	case INT_FM_RX:
	case VOICE_PLAYBACK_TX:
	case VOICE2_PLAYBACK_TX:
	case RT_PROXY_PORT_001_RX:
	case AUDIO_PORT_ID_I2S_RX:
	case AFE_PORT_ID_PRIMARY_MI2S_RX:
	case AFE_PORT_ID_SECONDARY_MI2S_RX:
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
		pr_err("%s: invalid port id %#x\n", __func__, port_id);
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
		ret_size = SIZEOF_CFG_CMD(afe_param_id_pcm_cfg);
		break;
	}
	return ret_size;
}

int afe_q6_interface_prepare(void)
{
	int ret = 0;

	pr_debug("%s:", __func__);

	if (this_afe.apr == NULL) {
		this_afe.apr = apr_register("ADSP", "AFE", afe_callback,
			0xFFFFFFFF, &this_afe);
		if (this_afe.apr == NULL) {
			pr_err("%s: Unable to register AFE\n", __func__);
			ret = -ENODEV;
		}
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
		/* apr_send_pkt can return 0 when nothing is transmitted */
		ret = -EINVAL;
	}

	pr_debug("%s: leave %d\n", __func__, ret);
	return ret;
}

static void afe_send_cal_block(int32_t path, u16 port_id)
{
	int						result = 0;
	int						index = 0;
	int						size = 4096;
	struct acdb_cal_block				cal_block;
	struct afe_audioif_config_command_no_payload	afe_cal;
	atomic_t *hptr;
	u32 handle;

	pr_debug("%s: path %d\n", __func__, path);
	if (path == AFE_AANC_TX_CAL) {
		get_aanc_cal(&cal_block);
	} else {
		get_afe_cal(path, &cal_block);
	}

	if (cal_block.cal_size <= 0) {
		pr_debug("%s: No AFE cal to send!\n", __func__);
		goto done;
	}

	if ((this_afe.afe_cal_addr[path].cal_paddr != cal_block.cal_paddr) ||
		(cal_block.cal_size > this_afe.afe_cal_addr[path].cal_size)) {
		atomic_set(&this_afe.mem_map_cal_index, path);
		if (this_afe.afe_cal_addr[path].cal_paddr != 0) {
			hptr = &this_afe.mem_map_cal_handles[path];
			handle = atomic_xchg(hptr, 0);
			if (!handle) {
				pr_err("%s: invalid NULL handle\n", __func__);
				result = -EINVAL;
				goto done;
			}
			result = afe_cmd_memory_unmap(handle);
			if (result) {
				WARN(1, "%s: AFE memory unmap failed %d, handle 0x%x\n",
				     __func__, result, handle);
				atomic_set(&this_afe.mem_map_cal_index, -1);
				goto done;
			}
		}

		result = afe_cmd_memory_map(cal_block.cal_paddr, size);
		if (result) {
			pr_err("%s: AFE memory map failed\n", __func__);
			atomic_set(&this_afe.mem_map_cal_index, -1);
			goto done;
		}
		atomic_set(&this_afe.mem_map_cal_index, -1);
		this_afe.afe_cal_addr[path].cal_paddr = cal_block.cal_paddr;
		this_afe.afe_cal_addr[path].cal_size = size;
	}

	index = q6audio_get_port_index(port_id);
	if (index < 0) {
		pr_debug("%s: AFE port index invalid!\n", __func__);
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
	afe_cal.param.payload_size = cal_block.cal_size;
	afe_cal.param.payload_address_lsw = cal_block.cal_paddr;
	afe_cal.param.payload_address_msw = 0;
	afe_cal.param.mem_map_handle =
			atomic_read(&this_afe.mem_map_cal_handles[path]);

	pr_debug("%s: AFE cal sent for device port = %d, path = %d, cal size = %d, cal addr = 0x%x\n",
		__func__, port_id, path,
		cal_block.cal_size, cal_block.cal_paddr);

	result = afe_apr_send_pkt(&afe_cal, &this_afe.wait[index]);
	if (result)
		pr_err("%s: AFE cal for port %d failed %d\n",
		       __func__, port_id, result);
	else
		pr_debug("%s: AFE cal sent for path %d device!\n", __func__,
			 path);
done:
	return;
}

int afe_unmap_cal_blocks(void)
{
	int	i;
	int	result = 0;
	int	result2 = 0;

	for (i = 0; i < MAX_AFE_CAL_TYPES; i++) {
		if (atomic_read(&this_afe.mem_map_cal_handles[i]) != 0) {

			atomic_set(&this_afe.mem_map_cal_index, i);
			result2 = afe_cmd_memory_unmap(atomic_read(
				&this_afe.mem_map_cal_handles[i]));
			if (result2 < 0) {
				pr_err("%s: unmap failed, err %d\n",
					__func__, result2);
				result = result2;
			} else {
				atomic_set(&this_afe.mem_map_cal_handles[i],
					0);
			}
			atomic_set(&this_afe.mem_map_cal_index, -1);

			this_afe.afe_cal_addr[i].cal_paddr = 0;
			this_afe.afe_cal_addr[i].cal_size = 0;
		}
	}
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
		pr_err("%s Invalid params\n", __func__);
		goto fail_cmd;
	}
	if ((q6audio_validate_port(port) < 0)) {
		pr_err("%s invalid port %d", __func__, port);
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
		pr_err("%s: Setting param for port %d param[0x%x]failed\n",
		 __func__, port, param_id);
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
	pr_debug("%s config.pdata.param_id %x status %d\n",
	__func__, config.pdata.param_id, ret);
	return ret;
}

static void afe_send_cal_spkr_prot_tx(int port_id)
{
	struct msm_spk_prot_cfg prot_cfg;
	union afe_spkr_prot_config afe_spk_config;

	/*Get spkr protection cfg data*/
	get_spk_protection_cfg(&prot_cfg);

	if ((prot_cfg.mode != MSM_SPKR_PROT_DISABLED) &&
		(this_afe.vi_tx_port == port_id)) {
		afe_spk_config.mode_rx_cfg.minor_version = 1;
		if (prot_cfg.mode == MSM_SPKR_PROT_CALIBRATION_IN_PROGRESS)
			afe_spk_config.mode_rx_cfg.mode =
			Q6AFE_MSM_SPKR_CALIBRATION;
		else
			afe_spk_config.mode_rx_cfg.mode =
			Q6AFE_MSM_SPKR_PROCESSING;
		if (afe_spk_prot_prepare(port_id,
			AFE_PARAM_ID_MODE_VI_PROC_CFG,
			&afe_spk_config))
			pr_err("%s TX VI_PROC_CFG failed\n", __func__);
		if (prot_cfg.mode != MSM_SPKR_PROT_NOT_CALIBRATED) {
			afe_spk_config.vi_proc_cfg.minor_version = 1;
			afe_spk_config.vi_proc_cfg.r0_cali_q24 =
			(uint32_t) prot_cfg.r0;
			afe_spk_config.vi_proc_cfg.t0_cali_q6 =
			(uint32_t) prot_cfg.t0;
			if (afe_spk_prot_prepare(port_id,
				AFE_PARAM_ID_SPKR_CALIB_VI_PROC_CFG,
				&afe_spk_config))
				pr_err("%s SPKR_CALIB_VI_PROC_CFG failed\n",
					__func__);
		}
	}
}

static void afe_send_cal_spkr_prot_rx(int port_id)
{
	struct msm_spk_prot_cfg prot_cfg;
	union afe_spkr_prot_config afe_spk_config;

	/*Get spkr protection cfg data*/
	get_spk_protection_cfg(&prot_cfg);

	if (prot_cfg.mode != MSM_SPKR_PROT_DISABLED) {
		if (prot_cfg.mode == MSM_SPKR_PROT_CALIBRATION_IN_PROGRESS)
			afe_spk_config.mode_rx_cfg.mode =
			Q6AFE_MSM_SPKR_CALIBRATION;
		else
			afe_spk_config.mode_rx_cfg.mode =
			Q6AFE_MSM_SPKR_PROCESSING;
		afe_spk_config.mode_rx_cfg.minor_version = 1;
		if (afe_spk_prot_prepare(port_id,
			AFE_PARAM_ID_FBSP_MODE_RX_CFG,
			&afe_spk_config))
			pr_err("%s RX MODE_VI_PROC_CFG failed\n",
				   __func__);
	}
}

static int afe_send_hw_delay(u16 port_id, u32 rate)
{
	struct hw_delay_entry delay_entry;
	struct afe_audioif_config_command config;
	int index = 0;
	int ret = -EINVAL;

	pr_debug("%s\n", __func__);

	delay_entry.sample_rate = rate;
	if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_TX)
		ret = get_hw_delay(TX_CAL, &delay_entry);
	else if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_RX)
		ret = get_hw_delay(RX_CAL, &delay_entry);

	if (ret != 0) {
		pr_warn("%s: Failed to get hw delay info\n", __func__);
		goto fail_cmd;
	}

	index = q6audio_get_port_index(port_id);
	if (index < 0) {
		pr_debug("%s: AFE port index invalid!\n", __func__);
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
		pr_err("%s: AFE hw delay for port %#x failed\n",
		       __func__, port_id);
		goto fail_cmd;
	} else if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}

fail_cmd:
	pr_debug("%s port_id %u rate %u delay_usec %d status %d\n",
	__func__, port_id, rate, delay_entry.delay_usec, ret);
	return ret;

}

void afe_send_cal(u16 port_id)
{
	pr_debug("%s\n", __func__);

	if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_TX) {
		afe_send_cal_spkr_prot_tx(port_id);
		afe_send_cal_block(AFE_TX_CAL, port_id);
	} else if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_RX) {
		afe_send_cal_spkr_prot_rx(port_id);
		afe_send_cal_block(AFE_RX_CAL, port_id);
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

	pr_debug("%s: enter\n", __func__);
	payload_size = sizeof(struct afe_param_cdc_reg_cfg_payload) *
		       cdc_reg_cfg->num_registers;
	pkt_size = sizeof(*config) + payload_size;

	pr_debug("%s: pkt_size %d, payload %d\n", __func__, pkt_size,
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
	pr_debug("%s: leave ret %d\n", __func__, ret);
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

	pr_debug("%s: leave ret %d\n", __func__, 0);
	return ret;
}

static int afe_send_slimbus_slave_port_cfg(
	struct afe_param_slimbus_slave_port_cfg *port_config, u16 port_id)
{
	int ret, index;
	struct afe_cmd_hw_mad_slimbus_slave_port_cfg config;

	pr_debug("%s: enter, port_id %u\n", __func__, port_id);
	index = q6audio_get_port_index(port_id);
	if (q6audio_validate_port(port_id) < 0) {
		pr_err("%s: port id: %#x\n", __func__, port_id);
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

	pr_debug("%s: tx_port %d, rx_port %d\n",
		__func__, tx_port, rx_port);

	ret = afe_q6_interface_prepare();
	if (ret != 0)
		return -EINVAL;

	index = q6audio_get_port_index(tx_port);
	if (q6audio_validate_port(tx_port) < 0) {
		pr_err("%s: port id: %#x\n", __func__, tx_port);
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
		pr_err("%s: AFE AANC port config failed for tx_port %d, rx_port %d\n",
			__func__, tx_port, rx_port);
	} else if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
	}

	pr_debug("%s: leave %d\n", __func__, ret);
	return ret;
}

static int afe_aanc_mod_enable(void *apr, uint16_t tx_port, uint16_t enable)
{
	struct afe_port_cmd_set_aanc_param cfg;
	int ret = 0;
	int index = 0;

	pr_debug("%s: tx_port %d\n",
		__func__, tx_port);

	ret = afe_q6_interface_prepare();
	if (ret != 0)
		return -EINVAL;

	index = q6audio_get_port_index(tx_port);
	if (q6audio_validate_port(tx_port) < 0) {
		pr_err("%s: port id: %#x\n", __func__, tx_port);
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
		pr_err("%s: AFE AANC enable failed for tx_port %d\n",
			__func__, tx_port);
	} else if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
	}
	pr_debug("%s: leave %d\n", __func__, ret);
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
	pr_debug("%s: leave ret %d\n", __func__, 0);
	return ret;
}

int afe_port_set_mad_type(u16 port_id, enum afe_mad_type mad_type)
{
	int i;

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
		pr_err("%s: unknown configuration type", __func__);
		ret = -EINVAL;
	}

	if (!ret)
		set_bit(config_type, &afe_configured_cmd);

	pr_debug("%s: leave ret %d\n", __func__, ret);
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

static int afe_send_cmd_port_start(u16 port_id)
{
	struct afe_port_cmd_device_start start;
	int ret, index;

	pr_debug("%s: enter\n", __func__);
	index = q6audio_get_port_index(port_id);
	if (q6audio_validate_port(port_id) < 0) {
		pr_err("%s: port id: %#x\n", __func__, port_id);
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
		pr_err("%s: AFE enable for port %#x failed %d\n", __func__,
		       port_id, ret);
	} else if (this_afe.task != current) {
		this_afe.task = current;
		pr_debug("task_name = %s pid = %d\n",
			 this_afe.task->comm, this_afe.task->pid);
	}

	pr_debug("%s: leave %d\n", __func__, ret);
	return ret;
}

static int afe_aanc_start(uint16_t tx_port_id, uint16_t rx_port_id)
{
	int ret;

	pr_debug("%s Tx port is %d, Rx port is %d\n",
		 __func__, tx_port_id, rx_port_id);
	ret = afe_aanc_port_cfg(this_afe.apr, tx_port_id, rx_port_id);
	if (ret) {
		pr_err("%s Send AANC Port Config failed %d\n",
			__func__, ret);
		goto fail_cmd;
	}
	afe_send_cal_block(AFE_AANC_TX_CAL, tx_port_id);

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
			" port_id %d\n", __func__,
			pcm_afe_instance[port_id & 0x1], port_id);
		port_id = VIRTUAL_ID_TO_PORTID(port_id);
		pcm_afe_instance[port_id & 0x1]++;
		return 0;
	}
	if ((port_id == RT_PROXY_DAI_002_RX) ||
			(port_id == RT_PROXY_DAI_001_TX)) {
		pr_debug("%s: before incrementing proxy_afe_instance %d"\
			" port_id %d\n", __func__,
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

	pr_debug("%s: port id: %#x\n", __func__, port_id);

	index = q6audio_get_port_index(port_id);
	if (q6audio_validate_port(port_id) < 0) {
		pr_err("%s: port id: %#x\n", __func__, port_id);
		return -EINVAL;
	}

	ret = afe_q6_interface_prepare();
	if (IS_ERR_VALUE(ret))
		return ret;

	afe_send_cal(port_id);
	afe_send_hw_delay(port_id, rate);

	/* Start SW MAD module */
	mad_type = afe_port_get_mad_type(port_id);
	pr_debug("%s: port_id 0x%x, mad_type %d\n", __func__, port_id,
		 mad_type);
	if (mad_type != MAD_HW_NONE) {
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
			ret = -EINVAL;
			goto fail_cmd;
		}
		ret = afe_aanc_start(this_afe.aanc_info.aanc_tx_port,
				this_afe.aanc_info.aanc_rx_port);
		pr_debug("%s afe_aanc_start ret %d\n", __func__, ret);
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
		pr_err("%s: AFE enable for port %#x failed\n", __func__,
				port_id);
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

	default: return -EINVAL;
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

	pr_err("%s: %d %d\n", __func__, port_id, rate);

	index = q6audio_get_port_index(port_id);
	if (q6audio_validate_port(port_id) < 0)
		return -EINVAL;

	if ((port_id == RT_PROXY_DAI_001_RX) ||
		(port_id == RT_PROXY_DAI_002_TX))
		return -EINVAL;
	if ((port_id == RT_PROXY_DAI_002_RX) ||
		(port_id == RT_PROXY_DAI_001_TX))
		port_id = VIRTUAL_ID_TO_PORTID(port_id);

	ret = afe_q6_interface_prepare();
	if (ret != 0)
		return ret;

	if (q6audio_validate_port(port_id) < 0) {
		pr_err("%s: Failed : Invalid Port id = %d\n", __func__,
				port_id);
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
		cfg_type = AFE_PARAM_ID_SLIMBUS_CONFIG;
		break;
	default:
		pr_err("%s: Invalid port id 0x%x\n", __func__, port_id);
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
	pr_debug("%s: param PL size=%d iparam_size[%d][%d %d %d %d] param_id[%x]\n",
		__func__, config.param.payload_size, config.pdata.param_size,
		sizeof(config), sizeof(config.param), sizeof(config.port),
		sizeof(struct apr_hdr), config.pdata.param_id);

	ret = afe_apr_send_pkt(&config, &this_afe.wait[index]);
	if (ret) {
		pr_err("%s: AFE enable for port %d opcode[0x%x]failed\n",
			__func__, port_id, cfg_type);
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
		pr_err("%s: AFE enable for port %d failed\n", __func__,
				port_id);
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
	if (ret != 0)
		return ret;

	index = q6audio_get_port_index(rx_port);
	if (q6audio_validate_port(rx_port) < 0)
		return -EINVAL;

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
	}

	if (q6audio_validate_port(port_id) < 0) {

		pr_err("%s: Failed : Invalid Port id = %d\n", __func__,
				port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	index = q6audio_get_port_index(port_id);
	if (q6audio_validate_port(port_id) < 0)
		return -EINVAL;

	/* RX ports numbers are even .TX ports numbers are odd. */
	if (port_id % 2 == 0) {
		pr_err("%s: Failed : afe loopback gain only for TX ports. port_id %d\n",
				__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	pr_debug("%s: %d %hX\n", __func__, port_id, volume);

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
		pr_err("%s: AFE param set failed for port %d\n",
					__func__, port_id);
		goto fail_cmd;
	}

fail_cmd:
	return ret;
}

int afe_pseudo_port_start_nowait(u16 port_id)
{
	struct afe_pseudoport_start_command start;
	int ret = 0;

	pr_debug("%s: port_id=%d\n", __func__, port_id);
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
		pr_err("%s: AFE enable for port %d failed %d\n",
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

	pr_debug("%s: port_id=%d\n", __func__, port_id);

	ret = afe_q6_interface_prepare();
	if (ret != 0)
		return ret;

	index = q6audio_get_port_index(port_id);
	if (q6audio_validate_port(port_id) < 0)
		return -EINVAL;

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
		pr_err("%s: AFE enable for port %d failed %d\n",
		       __func__, port_id, ret);
	return ret;
}

int afe_pseudo_port_stop_nowait(u16 port_id)
{
	int ret = 0;
	struct afe_pseudoport_stop_command stop;
	int index = 0;

	pr_debug("%s: port_id=%d\n", __func__, port_id);

	if (this_afe.apr == NULL) {
		pr_err("%s: AFE is already closed\n", __func__);
		return -EINVAL;
	}
	index = q6audio_get_port_index(port_id);
	if (q6audio_validate_port(port_id) < 0)
		return -EINVAL;

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

int afe_stop_pseudo_port(u16 port_id)
{
	int ret = 0;
	struct afe_pseudoport_stop_command stop;
	int index = 0;

	pr_debug("%s: port_id=%d\n", __func__, port_id);

	if (this_afe.apr == NULL) {
		pr_err("%s: AFE is already closed\n", __func__);
		return -EINVAL;
	}

	index = q6audio_get_port_index(port_id);
	if (q6audio_validate_port(port_id) < 0)
		return -EINVAL;

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
	int len;

	if (!(ac) || ((dir != IN) && (dir != OUT)))
		return -EINVAL;

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
		mutex_unlock(&ac->cmd_lock);
		goto fail;
	}

	ac->port[dir].buf = buf;

	rc = msm_audio_ion_alloc("audio_client", &buf[0].client,
				&buf[0].handle, bufsz*bufcnt,
				(ion_phys_addr_t *)&buf[0].phys, (size_t *)&len,
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
				pr_err("%s Buf alloc failed\n",
							__func__);
				mutex_unlock(&ac->cmd_lock);
				goto fail;
			}
			buf[cnt].used = dir ^ 1;
			buf[cnt].size = bufsz;
			buf[cnt].actual_size = bufsz;
			pr_debug("%s data[%p]phys[%pa][%p]\n", __func__,
				   (void *)buf[cnt].data,
				   &buf[cnt].phys,
				   (void *)&buf[cnt].phys);
		}
		cnt++;
	}
	ac->port[dir].max_buf_cnt = cnt;
	mutex_unlock(&ac->cmd_lock);
	return 0;
fail:
	q6afe_audio_client_buf_free_contiguous(dir, ac);
	return -EINVAL;
}

int afe_memory_map(u32 dma_addr_p, u32 dma_buf_sz, struct afe_audio_client *ac)
{
	int ret = 0;

	ac->mem_map_handle = 0;
	ret = afe_cmd_memory_map(dma_addr_p, dma_buf_sz);
	if (ret < 0) {
		pr_err("%s: afe_cmd_memory_map failed\n", __func__);
		return ret;
	}
	ac->mem_map_handle = this_afe.mmap_handle;
	return ret;
}

int afe_cmd_memory_map(u32 dma_addr_p, u32 dma_buf_sz)
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

	mregion_pl->shm_addr_lsw = dma_addr_p;
	mregion_pl->shm_addr_msw = 0x00;
	mregion_pl->mem_size_bytes = dma_buf_sz;

	pr_debug("%s: dma_addr_p 0x%x , size %d\n", __func__,
					dma_addr_p, dma_buf_sz);
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
	return ret;
}

int afe_cmd_memory_map_nowait(int port_id, u32 dma_addr_p, u32 dma_buf_sz)
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
	}
	index = q6audio_get_port_index(port_id);
	if (q6audio_validate_port(port_id) < 0)
		return -EINVAL;

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

	mregion_pl->shm_addr_lsw = dma_addr_p;
	mregion_pl->shm_addr_msw = 0x00;
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
		mutex_unlock(&ac->cmd_lock);
		return 0;
	}
	cnt = port->max_buf_cnt - 1;

	if (port->buf[0].data) {
		pr_debug("%s:data[%p]phys[%pa][%p] , client[%p] handle[%p]\n",
			__func__,
			(void *)port->buf[0].data,
			&port->buf[0].phys,
			(void *)&port->buf[0].phys,
			(void *)port->buf[0].client,
			(void *)port->buf[0].handle);
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
	if (!ac)
		return;
	for (loopcnt = 0; loopcnt <= OUT; loopcnt++) {
		port = &ac->port[loopcnt];
		if (!port->buf)
			continue;
		pr_debug("%s:loopcnt = %d\n", __func__, loopcnt);
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

	pr_debug("%s: port_id: %d\n", __func__, port_id);

	if (this_afe.apr == NULL) {
		this_afe.apr = apr_register("ADSP", "AFE", afe_callback,
					0xFFFFFFFF, &this_afe);
		pr_debug("%s: Register AFE\n", __func__);
		if (this_afe.apr == NULL) {
			pr_err("%s: Unable to register AFE\n", __func__);
			ret = -ENODEV;
			return ret;
		}
	}
	if ((port_id == RT_PROXY_DAI_002_RX) ||
		(port_id == RT_PROXY_DAI_001_TX))
		port_id = VIRTUAL_ID_TO_PORTID(port_id);
	else
		return -EINVAL;

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
	}

	if ((port_id == RT_PROXY_DAI_002_RX) ||
		(port_id == RT_PROXY_DAI_001_TX))
		port_id = VIRTUAL_ID_TO_PORTID(port_id);
	else
		return -EINVAL;

	index = q6audio_get_port_index(port_id);
	if (q6audio_validate_port(port_id) < 0)
		return -EINVAL;

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
		pr_err("%s:register to AFE is not done\n", __func__);
		ret = -ENODEV;
		return ret;
	}
	pr_debug("%s: buf_addr_p = 0x%08x bytes = %d\n", __func__,
						buf_addr_p, bytes);

	afecmd_wr.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	afecmd_wr.hdr.pkt_size = sizeof(afecmd_wr);
	afecmd_wr.hdr.src_port = 0;
	afecmd_wr.hdr.dest_port = 0;
	afecmd_wr.hdr.token = 0;
	afecmd_wr.hdr.opcode = AFE_PORT_DATA_CMD_RT_PROXY_PORT_WRITE_V2;
	afecmd_wr.port_id = RT_PROXY_PORT_001_TX;
	afecmd_wr.buffer_address_lsw = (uint32_t)buf_addr_p;
	afecmd_wr.buffer_address_msw = 0x00;
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
	pr_debug("%s: buf_addr_p = 0x%08x bytes = %d\n", __func__,
						buf_addr_p, bytes);

	afecmd_rd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	afecmd_rd.hdr.pkt_size = sizeof(afecmd_rd);
	afecmd_rd.hdr.src_port = 0;
	afecmd_rd.hdr.dest_port = 0;
	afecmd_rd.hdr.token = 0;
	afecmd_rd.hdr.opcode = AFE_PORT_DATA_CMD_RT_PROXY_PORT_READ_V2;
	afecmd_rd.port_id = RT_PROXY_PORT_001_RX;
	afecmd_rd.buffer_address_lsw = (uint32_t)buf_addr_p;
	afecmd_rd.buffer_address_msw = 0x00;
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
	pr_info("debug intf %s\n", (char *) file->private_data);
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

			if (kstrtoul(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else
			return -EINVAL;
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

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';

	if (!strncmp(lb_str, "afe_loopback", 12)) {
		rc = afe_get_parameters(lbuf, param, 3);
		if (!rc) {
			pr_info("%s %lu %lu %lu\n", lb_str, param[0], param[1],
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
			pr_info("%s %lu %lu\n", lb_str, param[0], param[1]);

			if (q6audio_validate_port(param[0]) < 0) {
				pr_err("%s: Error, invalid afe port\n",
					__func__);
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
		pr_err("%s: Failed : Invalid Port id = %d\n",
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
	}

	pr_debug("dur=%lld: hfreq=%d lfreq=%d gain=%d portid=%x\n",
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
		pr_err("%s: AFE DTMF failed for num_ports:%d ids:%x\n",
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
	return ret;
}

int afe_sidetone(u16 tx_port_id, u16 rx_port_id, u16 enable, uint16_t gain)
{
	struct afe_loopback_cfg_v1 cmd_sidetone;
	int ret = 0;
	int index = 0;

	pr_info("%s: tx_port_id:%d rx_port_id:%d enable:%d gain:%d\n", __func__,
					tx_port_id, rx_port_id, enable, gain);
	index = q6audio_get_port_index(rx_port_id);
	if (q6audio_validate_port(rx_port_id) < 0)
		return -EINVAL;

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
		pr_err("%s: AFE sidetone failed for tx_port:%d rx_port:%d\n",
					__func__, tx_port_id, rx_port_id);
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
	case AFE_PORT_ID_PRIMARY_MI2S_RX:
	{
		ret = 0;
		break;
	}

	default:
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
		    port_id == RT_PROXY_DAI_002_TX)
			ret = VIRTUAL_ID_TO_PORTID(port_id);
		else
			ret = -EINVAL;
	} else
		ret = port_id;

	return ret;
}
int afe_port_stop_nowait(int port_id)
{
	struct afe_port_cmd_device_stop stop;
	int ret = 0;

	if (this_afe.apr == NULL) {
		pr_err("AFE is already closed\n");
		ret = -EINVAL;
		goto fail_cmd;
	}
	pr_debug("%s: port_id=%d\n", __func__, port_id);
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
		pr_err("%s: AFE close failed\n", __func__);

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
		pr_err("AFE is already closed\n");
		ret = -EINVAL;
		goto fail_cmd;
	}
	pr_debug("%s: port_id=%d\n", __func__, port_id);
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
	if (q6audio_validate_port(port_id) < 0) {
		pr_warn("%s: Not a valid port id 0x0%x\n", __func__, port_id);
		return -EINVAL;
	}

	mad_type = afe_port_get_mad_type(port_id);
	pr_debug("%s: port_id 0x%x, mad_type %d\n", __func__, port_id,
		 mad_type);
	if (mad_type != MAD_HW_NONE) {
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
	if (q6audio_is_digital_pcm_interface(port_id) < 0)
		return -EINVAL;

	ret = afe_q6_interface_prepare();
	if (ret != 0)
		return ret;

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

	pr_debug("%s: Minor version =%x clk val1 = %d\n"
		 "clk val2 = %d, clk src = %x\n"
		 "clk root = %x clk mode = %x resrv = %x\n"
		 "port id = %x\n",
		 __func__, cfg->i2s_cfg_minor_version,
		 cfg->clk_val1, cfg->clk_val2, cfg->clk_src,
		 cfg->clk_root, cfg->clk_set_mode,
		 cfg->reserved, q6audio_get_port_id(port_id));

	atomic_set(&this_afe.state, 1);
	atomic_set(&this_afe.status, 0);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &clk_cfg);
	if (ret < 0) {
		pr_err("%s: AFE enable for port %d\n",
		       __func__, port_id);
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
	if (q6audio_is_digital_pcm_interface(port_id) < 0)
		return -EINVAL;

	ret = afe_q6_interface_prepare();
	if (ret != 0)
		return ret;

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

	pr_debug("%s: Minor version =%x clk val = %d\n"
		 "clk root = %x resrv = %x port id = %x\n",
		 __func__, cfg->i2s_cfg_minor_version,
		 cfg->clk_val, cfg->clk_root, cfg->reserved,
		 q6audio_get_port_id(port_id));

	atomic_set(&this_afe.state, 1);
	atomic_set(&this_afe.status, 0);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &clk_cfg);
	if (ret < 0) {
		pr_err("%s: AFE enable for port %d\n",
		       __func__, port_id);
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
		ret = -EINVAL;
	}
	return ret;
}

int afe_spk_prot_get_calib_data(struct afe_spkr_prot_get_vi_calib *calib_resp)
{
	int ret = -EINVAL;
	int index = 0, port = SLIMBUS_4_TX;

	if (!calib_resp) {
		pr_err("%s Invalid params\n", __func__);
		goto fail_cmd;
	}
	if ((q6audio_validate_port(port) < 0)) {
		pr_err("%s invalid port %d\n", __func__, port);
		goto fail_cmd;
	}
	index = q6audio_get_port_index(port);
	calib_resp->get_param.hdr.hdr_field =
	APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
	APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	calib_resp->get_param.hdr.pkt_size = sizeof(*calib_resp);
	calib_resp->get_param.hdr.src_port = 0;
	calib_resp->get_param.hdr.dest_port = 0;
	calib_resp->get_param.hdr.token = index;
	calib_resp->get_param.hdr.opcode =  AFE_PORT_CMD_GET_PARAM_V2;
	calib_resp->get_param.mem_map_handle = 0;
	calib_resp->get_param.module_id = AFE_MODULE_FB_SPKR_PROT_VI_PROC;
	calib_resp->get_param.param_id = AFE_PARAM_ID_CALIB_RES_CFG;
	calib_resp->get_param.payload_address_lsw = 0;
	calib_resp->get_param.payload_address_msw = 0;
	calib_resp->get_param.payload_size = sizeof(*calib_resp)
		- sizeof(calib_resp->get_param);
	calib_resp->get_param.port_id = q6audio_get_port_id(port);
	calib_resp->pdata.module_id = AFE_MODULE_FB_SPKR_PROT_VI_PROC;
	calib_resp->pdata.param_id = AFE_PARAM_ID_CALIB_RES_CFG;
	calib_resp->pdata.param_size = sizeof(calib_resp->res_cfg);
	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *)calib_resp);
	if (ret < 0) {
		pr_err("%s: get param port %d param id[0x%x]failed\n",
			   __func__, port, calib_resp->get_param.param_id);
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
	pr_debug("%s state %d resistance %d\n", __func__,
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
		pr_debug("%s Disable Feedback tx path", __func__);
		this_afe.vi_tx_port = -1;
		return 0;
	}

	if ((q6audio_validate_port(src_port) < 0) ||
		(q6audio_validate_port(dst_port) < 0)) {
		pr_err("%s invalid ports src %d dst %d",
			__func__, src_port, dst_port);
		goto fail_cmd;
	}
	if (!l_ch && !r_ch) {
		pr_err("%s error ch values zero\n", __func__);
		goto fail_cmd;
	}
	pr_debug("%s src_port %x  dst_port %x l_ch %d r_ch %d\n",
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

static int __init afe_init(void)
{
	int i = 0;
	atomic_set(&this_afe.state, 0);
	atomic_set(&this_afe.status, 0);
	atomic_set(&this_afe.mem_map_cal_index, -1);
	this_afe.apr = NULL;
	this_afe.dtmf_gen_rx_portid = -1;
	this_afe.mmap_handle = 0;
	this_afe.vi_tx_port = -1;
	for (i = 0; i < AFE_MAX_PORTS; i++)
		init_waitqueue_head(&this_afe.wait[i]);

	config_debug_fs_init();
	return 0;
}

static void __exit afe_exit(void)
{
	int i;
	atomic_t *hptr;
	u32 handle;

	config_debug_fs_exit();
	for (i = 0; i < ARRAY_SIZE(this_afe.mem_map_cal_handles); i++) {
		hptr = &this_afe.mem_map_cal_handles[i];
		handle = atomic_xchg(hptr, 0);
		if (handle != 0)
			afe_cmd_memory_unmap_nowait(handle);
	}
}

device_initcall(afe_init);
__exitcall(afe_exit);
