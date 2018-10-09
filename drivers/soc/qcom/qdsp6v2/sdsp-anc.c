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

#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/msm_audio_ion.h>
#include <linux/delay.h>
#include <sound/apr_audio-v2.h>
#include <sound/q6afe-v2.h>
#include <sound/q6audio-v2.h>
#include <sound/audio_cal_utils.h>
#include <sound/adsp_err.h>
#include <linux/qdsp6v2/apr_tal.h>

#include <linux/qdsp6v2/sdsp_anc.h>

#define TIMEOUT_MS 1000

struct anc_if_ctl {
	void *apr;
	atomic_t state;
	atomic_t status;
	wait_queue_head_t wait[AFE_MAX_PORTS];
	struct task_struct *task;
	struct anc_get_algo_module_cali_data_resp cali_data_resp;
	uint32_t mmap_handle;
	struct mutex afe_cmd_lock;
};

static struct anc_if_ctl this_anc_if;

static int32_t anc_get_param_callback(uint32_t *payload,
			uint32_t payload_size)
{
	if ((payload_size < (sizeof(uint32_t) +
		sizeof(this_anc_if.cali_data_resp.pdata))) ||
		(payload_size > sizeof(this_anc_if.cali_data_resp))) {
		pr_err("%s: Error: received size %d, calib_data size %zu\n",
			__func__, payload_size,
			sizeof(this_anc_if.cali_data_resp));
		return -EINVAL;
	}

	memcpy(&this_anc_if.cali_data_resp, payload,
		payload_size);
	if (!this_anc_if.cali_data_resp.status) {
		atomic_set(&this_anc_if.state, 0);
	} else {
		pr_debug("%s: calib resp status: %d", __func__,
			this_anc_if.cali_data_resp.status);
		atomic_set(&this_anc_if.state, -1);
	}

	return 0;
}

static void anc_if_callback_debug_print(struct apr_client_data *data)
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

static int32_t anc_if_callback(struct apr_client_data *data, void *priv)
{
	if (!data) {
		pr_err("%s: Invalid param data\n", __func__);
		return -EINVAL;
	}
	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: reset event = %d %d apr[%pK]\n",
			__func__,
			data->reset_event, data->reset_proc, this_anc_if.apr);

		if (this_anc_if.apr) {
			apr_reset(this_anc_if.apr);
			atomic_set(&this_anc_if.state, 0);
			this_anc_if.apr = NULL;
		}

		return 0;
	}
	anc_if_callback_debug_print(data);
	if (data->opcode == AFE_PORT_CMDRSP_GET_PARAM_V2) {
		u8 *payload = data->payload;

		if (!payload || (data->token >= AFE_MAX_PORTS)) {
			pr_err("%s: Error: size %d payload %pK token %d\n",
				__func__, data->payload_size,
				payload, data->token);
			return -EINVAL;
		}

		if (anc_get_param_callback(data->payload, data->payload_size))
			return -EINVAL;

		wake_up(&this_anc_if.wait[data->token]);

	} else if (data->payload_size) {
		uint32_t *payload;

		payload = data->payload;
		if (data->opcode == APR_BASIC_RSP_RESULT) {
			pr_debug("%s:opcode = 0x%x cmd = 0x%x status = 0x%x token=%d\n",
				__func__, data->opcode,
				payload[0], payload[1], data->token);
			/* payload[1] contains the error status for response */
			if (payload[1] != 0) {
				atomic_set(&this_anc_if.status, payload[1]);
				pr_err("%s: cmd = 0x%x returned error = 0x%x\n",
					__func__, payload[0], payload[1]);
			}
			switch (payload[0]) {
			case AFE_PORT_CMD_SET_PARAM_V2:
			case AFE_PORT_CMD_DEVICE_STOP:
			case AFE_PORT_CMD_DEVICE_START:
			case AFE_SERVICE_CMD_SHARED_MEM_MAP_REGIONS:
			case AFE_SERVICE_CMD_SHARED_MEM_UNMAP_REGIONS:
			case AFE_SVC_CMD_SET_PARAM:
				atomic_set(&this_anc_if.state, 0);
				wake_up(&this_anc_if.wait[data->token]);
				break;
			default:
				pr_err("%s: Unknown cmd 0x%x\n", __func__,
						payload[0]);
				break;
			}
		} else if (data->opcode ==
				AFE_SERVICE_CMDRSP_SHARED_MEM_MAP_REGIONS) {
			pr_err("%s: ANC mmap_handle: 0x%x\n",
			__func__, payload[0]);
			this_anc_if.mmap_handle = payload[0];
			atomic_set(&this_anc_if.state, 0);
			wake_up(&this_anc_if.wait[data->token]);
		}
	}
	return 0;
}

int anc_sdsp_interface_prepare(void)
{
	int ret = 0;

	pr_debug("%s:\n", __func__);

	if (this_anc_if.apr == NULL) {
		this_anc_if.apr = apr_register("SDSP", "MAS", anc_if_callback,
			0xFFFFFFFF, &this_anc_if);
		if (this_anc_if.apr == NULL) {
			pr_err("%s: Unable to register AFE\n", __func__);
			ret = -ENODEV;
		}
	}
	return ret;
}

/*
 * anc_if_apr_send_pkt : returns 0 on success, negative otherwise.
 */
static int anc_if_apr_send_pkt(void *data, wait_queue_head_t *wait)
{
	int ret;

	if (wait)
		atomic_set(&this_anc_if.state, 1);
	atomic_set(&this_anc_if.status, 0);
	ret = apr_send_pkt(this_anc_if.apr, data);
	if (ret > 0) {
		if (wait) {
			ret = wait_event_timeout(*wait,
					(atomic_read(&this_anc_if.state) == 0),
					msecs_to_jiffies(TIMEOUT_MS));
			if (!ret) {
				ret = -ETIMEDOUT;
			} else if (atomic_read(&this_anc_if.status) > 0) {
				pr_err("%s: DSP returned error[%s]\n", __func__,
					adsp_err_get_err_str(atomic_read(
					&this_anc_if.status)));
				ret = adsp_err_get_lnx_err_code(
			atomic_read(&this_anc_if.status));
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

static int anc_if_send_cmd_port_start(u16 port_id)
{
	struct afe_port_cmd_device_start start;
	int ret, index;

	pr_debug("%s: enter\n", __func__);
	index = q6audio_get_port_index(port_id);
	if (index < 0 || index > AFE_MAX_PORTS) {
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

	ret = anc_if_apr_send_pkt(&start, &this_anc_if.wait[index]);
	if (ret) {
		pr_err("%s: AFE enable for port 0x%x failed %d\n", __func__,
				port_id, ret);
	} else if (this_anc_if.task != current) {
		this_anc_if.task = current;
		pr_debug("task_name = %s pid = %d\n",
			this_anc_if.task->comm, this_anc_if.task->pid);
	}

	return ret;
}

int anc_if_send_cmd_port_stop(int port_id)
{
	struct afe_port_cmd_device_stop stop;
	int ret = 0;

	if (this_anc_if.apr == NULL) {
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

	ret = anc_if_apr_send_pkt(&stop, NULL);
	if (ret)
		pr_err("%s: AFE close failed %d\n", __func__, ret);

fail_cmd:
	return ret;

}

int anc_if_config_ref(u16 port_id, u32 sample_rate,
		u32 bit_width, u16 num_channel)
{
	struct anc_config_ref_command config;
	int ret = 0;
	int index;

	ret = anc_sdsp_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	index = q6audio_get_port_index(port_id);
	memset(&config, 0, sizeof(config));
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
	config.pdata.module_id = AUD_MSVC_MODULE_AUDIO_DEV_ANC_REFS;
	config.pdata.param_id = AUD_MSVC_PARAM_ID_DEV_ANC_REFS_CONFIG;
	config.pdata.param_size = sizeof(config.refs);
	config.refs.minor_version = AUD_MSVC_API_VERSION_DEV_ANC_REFS_CONFIG;
	config.refs.port_id = q6audio_get_port_id(port_id);
	config.refs.sample_rate = sample_rate;
	config.refs.bit_width = bit_width;
	config.refs.num_channel = num_channel;

	ret = anc_if_apr_send_pkt(&config, &this_anc_if.wait[index]);
	if (ret) {
		pr_err("%s: anc_if_config_ref for port 0x%x failed ret = %d\n",
				__func__, port_id, ret);
		pr_err("%s: anc_if_config_ref size of param is %lu\n",
				__func__, sizeof(config.refs));
	}

	return ret;
}

int anc_if_share_resource(u16 port_id, u16 rddma_idx, u16 wrdma_idx,
			u32 lpm_start_addr, u32 lpm_length)
{
	struct anc_share_resource_command config;
	int ret = 0;
	int index;

	ret = anc_sdsp_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	index = q6audio_get_port_index(port_id);
	memset(&config, 0, sizeof(config));
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
	config.pdata.module_id = AUD_MSVC_MODULE_AUDIO_DEV_RESOURCE_SHARE;
	config.pdata.param_id = AUD_MSVC_PARAM_ID_PORT_SHARE_RESOURCE_CONFIG;
	config.pdata.param_size = sizeof(config.resource);
	config.resource.minor_version =
	AUD_MSVC_API_VERSION_SHARE_RESOURCE_CONFIG;
	config.resource.rddma_idx = rddma_idx;
	config.resource.wrdma_idx = wrdma_idx;
	config.resource.lpm_start_addr = lpm_start_addr;
	config.resource.lpm_length = lpm_length;

	ret = anc_if_apr_send_pkt(&config, &this_anc_if.wait[index]);
	if (ret) {
		pr_err("%s: share resource for port 0x%x failed ret = %d\n",
				__func__, port_id, ret);
	}

	return ret;
}

int anc_if_tdm_port_start(u16 port_id, struct afe_tdm_port_config *tdm_port)
{
	struct aud_audioif_config_command config;
	int ret = 0;
	int index = 0;

	if (!tdm_port) {
		pr_err("%s: Error, no configuration data\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: port id: 0x%x\n", __func__, port_id);

	index = q6audio_get_port_index(port_id);
	if (index < 0 || index > AFE_MAX_PORTS) {
		pr_err("%s: AFE port index[%d] invalid!\n",
				__func__, index);
		return -EINVAL;
	}

	ret = anc_sdsp_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	memset(&config, 0, sizeof(config));
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
	config.pdata.param_id = AFE_PARAM_ID_TDM_CONFIG;
	config.pdata.param_size = sizeof(config.port);
	config.port.tdm = tdm_port->tdm;

	ret = anc_if_apr_send_pkt(&config, &this_anc_if.wait[index]);
	if (ret) {
		pr_err("%s: AFE enable for port 0x%x failed ret = %d\n",
				__func__, port_id, ret);
		goto fail_cmd;
	}

	ret = anc_if_send_cmd_port_start(port_id);

fail_cmd:
	return ret;
}

int anc_if_tdm_port_stop(u16 port_id)
{
	return anc_if_send_cmd_port_stop(port_id);
}

int anc_if_set_algo_module_id(u16 port_id, u32 module_id)
{
	int ret = 0;
	int index;

	ret = anc_sdsp_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	index = q6audio_get_port_index(port_id);

	{
		struct anc_set_algo_module_id_command config;

		memset(&config, 0, sizeof(config));
		config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		config.hdr.pkt_size = sizeof(config);
		config.hdr.src_port = 0;
		config.hdr.dest_port = 0;
		config.hdr.token = index;
		config.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
		config.param.port_id = q6audio_get_port_id(port_id);
		config.param.payload_size = sizeof(config) -
			sizeof(struct apr_hdr) -
			sizeof(config.param);
		config.param.payload_address_lsw = 0x00;
		config.param.payload_address_msw = 0x00;
		config.param.mem_map_handle = 0x00;
		config.pdata.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
		config.pdata.param_id =
		AUD_MSVC_PARAM_ID_PORT_ANC_ALGO_MODULE_ID;
		config.pdata.param_size = sizeof(config.set_algo_module_id);
		config.set_algo_module_id.minor_version = 1;
		config.set_algo_module_id.module_id = module_id;

		ret = anc_if_apr_send_pkt(&config, &this_anc_if.wait[index]);
		if (ret) {
			pr_err("%s: anc algo module ID for port 0x%x failed ret = %d\n",
					__func__, port_id, ret);
		}
	}

	return ret;
}

int anc_if_set_anc_mic_spkr_layout(u16 port_id,
struct aud_msvc_param_id_dev_anc_mic_spkr_layout_info *set_mic_spkr_layout_p)
{
	int ret = 0;
	int index;

	ret = anc_sdsp_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	index = q6audio_get_port_index(port_id);

	{
		struct anc_set_mic_spkr_layout_info_command config;

		memset(&config, 0, sizeof(config));
		config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		config.hdr.pkt_size = sizeof(config);
		config.hdr.src_port = 0;
		config.hdr.dest_port = 0;
		config.hdr.token = index;
		config.hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
		config.param.port_id = q6audio_get_port_id(port_id);
		config.param.payload_size = sizeof(config) -
			sizeof(struct apr_hdr) -
			sizeof(config.param);
		config.param.payload_address_lsw = 0x00;
		config.param.payload_address_msw = 0x00;
		config.param.mem_map_handle = 0x00;
		config.pdata.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
		config.pdata.param_id =
		AUD_MSVC_PARAM_ID_PORT_ANC_MIC_SPKR_LAYOUT_INFO;
		config.pdata.param_size = sizeof(config.set_mic_spkr_layout);

		memcpy(&config.set_mic_spkr_layout, set_mic_spkr_layout_p,
		sizeof(config.set_mic_spkr_layout));
		ret = anc_if_apr_send_pkt(&config, &this_anc_if.wait[index]);
		if (ret) {
			pr_err("%s: anc algo module ID for port 0x%x failed ret = %d\n",
					__func__, port_id, ret);
		}
	}

	return ret;
}


int anc_if_set_algo_module_cali_data(u16 port_id, void *data_p)
{
	int ret = 0;
	int index;

	ret = anc_sdsp_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	index = q6audio_get_port_index(port_id);

	{
		struct anc_set_algo_module_cali_data_command *cali_data_cfg_p;
		void *config_p = NULL;
		int cmd_size = 0;
		void *out_payload_p = NULL;
		uint32_t *in_payload_p = (uint32_t *)data_p;

		uint32_t module_id = *in_payload_p;
		uint32_t param_id = *(in_payload_p + 1);
		uint32_t payload_size = *(in_payload_p + 2);

		cmd_size = sizeof(struct anc_set_algo_module_cali_data_command)
		+ payload_size;
		config_p = kzalloc(cmd_size, GFP_KERNEL);
		if (!config_p) {
			ret = -ENOMEM;
			return ret;
		}

		memset(config_p, 0, cmd_size);
		out_payload_p = config_p
		+ sizeof(struct anc_set_algo_module_cali_data_command);

		cali_data_cfg_p =
		(struct anc_set_algo_module_cali_data_command *)config_p;

		cali_data_cfg_p->hdr.hdr_field =
		APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		cali_data_cfg_p->hdr.pkt_size = cmd_size;
		cali_data_cfg_p->hdr.src_port = 0;
		cali_data_cfg_p->hdr.dest_port = 0;
		cali_data_cfg_p->hdr.token = index;
		cali_data_cfg_p->hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
		cali_data_cfg_p->param.port_id = q6audio_get_port_id(port_id);
		cali_data_cfg_p->param.payload_size = cmd_size -
			sizeof(struct apr_hdr) -
			sizeof(struct aud_msvc_port_cmd_set_param_v2);
		cali_data_cfg_p->param.payload_address_lsw = 0x00;
		cali_data_cfg_p->param.payload_address_msw = 0x00;
		cali_data_cfg_p->param.mem_map_handle = 0x00;
		cali_data_cfg_p->pdata.module_id = module_id;
		cali_data_cfg_p->pdata.param_id = param_id;
		cali_data_cfg_p->pdata.param_size = payload_size;

		memcpy(out_payload_p, (in_payload_p + 3), payload_size);

		ret = anc_if_apr_send_pkt(cali_data_cfg_p,
			&this_anc_if.wait[index]);
		if (ret)
			pr_err("%s: anc algo module calibration data for port 0x%x failed ret = %d\n",
			__func__, port_id, ret);

		kfree(config_p);
	}

	return ret;
}

int anc_if_get_algo_module_cali_data(u16 port_id, void *data_p)
{
	int ret = 0;
	int index;

	ret = anc_sdsp_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	index = q6audio_get_port_index(port_id);

	{
		struct anc_get_algo_module_cali_data_command *cali_data_cfg_p;
		void *config_p = NULL;
		int cmd_size = 0;
		void *out_payload_p = NULL;
		uint32_t *in_payload_p = (uint32_t *)data_p;

		uint32_t module_id = *in_payload_p;
		uint32_t param_id = *(in_payload_p + 1);
		uint32_t payload_size = *(in_payload_p + 2);

		cmd_size = sizeof(struct anc_get_algo_module_cali_data_command)
		+ payload_size;
		config_p = kzalloc(cmd_size, GFP_KERNEL);
			if (!config_p) {
			ret = -ENOMEM;
			return ret;
		}

		memset(config_p, 0, cmd_size);
		out_payload_p = config_p +
		sizeof(struct anc_set_algo_module_cali_data_command);

		cali_data_cfg_p =
		(struct anc_get_algo_module_cali_data_command *)config_p;

		cali_data_cfg_p->hdr.hdr_field =
		APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		cali_data_cfg_p->hdr.pkt_size = cmd_size;
		cali_data_cfg_p->hdr.src_port = 0;
		cali_data_cfg_p->hdr.dest_port = 0;
		cali_data_cfg_p->hdr.token = index;
		cali_data_cfg_p->hdr.opcode = AFE_PORT_CMD_GET_PARAM_V2;
		cali_data_cfg_p->param.port_id = q6audio_get_port_id(port_id);
		cali_data_cfg_p->param.payload_size = cmd_size -
			sizeof(struct apr_hdr) -
			sizeof(struct aud_msvc_port_cmd_get_param_v2);
		cali_data_cfg_p->param.payload_address_lsw = 0x00;
		cali_data_cfg_p->param.payload_address_msw = 0x00;
		cali_data_cfg_p->param.mem_map_handle = 0x00;
		cali_data_cfg_p->param.module_id = module_id;
		cali_data_cfg_p->param.param_id = param_id;
		cali_data_cfg_p->pdata.param_size = 0;
		cali_data_cfg_p->pdata.module_id = 0;
		cali_data_cfg_p->pdata.param_id = 0;

		ret = anc_if_apr_send_pkt(cali_data_cfg_p,
		&this_anc_if.wait[index]);
		if (ret)
			pr_err("%s: anc algo module calibration data for port 0x%x failed ret = %d\n",
					__func__, port_id, ret);

		memcpy((in_payload_p + 3),
		&this_anc_if.cali_data_resp.payload[0], payload_size);

		*in_payload_p = this_anc_if.cali_data_resp.pdata.module_id;
		*(in_payload_p + 1) =
		this_anc_if.cali_data_resp.pdata.param_id;
		*(in_payload_p + 2) =
		this_anc_if.cali_data_resp.pdata.param_size;

		kfree(config_p);
	}

	return ret;
}

int anc_if_cmd_memory_map(int port_id, phys_addr_t dma_addr_p,
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

	ret = anc_sdsp_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	index = q6audio_get_port_index(port_id);
	if (index < 0 || index > AFE_MAX_PORTS) {
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
	if (!mmap_region_cmd) {
		ret = -ENOMEM;
		return ret;
	}

	mregion = (struct afe_service_cmd_shared_mem_map_regions *)
						mmap_region_cmd;
	mregion->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	mregion->hdr.pkt_size = cmd_size;
	mregion->hdr.src_port = 0;
	mregion->hdr.dest_port = 0;
	mregion->hdr.token = index;
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

	ret = anc_if_apr_send_pkt(mmap_region_cmd, &this_anc_if.wait[index]);
	if (ret)
		pr_err("%s: AFE memory map cmd failed %d\n",
				__func__, ret);
	kfree(mmap_region_cmd);
	return ret;
}

int anc_if_cmd_memory_unmap(int port_id, u32 mem_map_handle)
{
	int ret = 0;
	struct afe_service_cmd_shared_mem_unmap_regions mregion;
	int index = 0;

	pr_debug("%s: handle 0x%x\n", __func__, mem_map_handle);

	ret = anc_sdsp_interface_prepare();
	if (ret != 0) {
		pr_err("%s: Q6 interface prepare failed %d\n", __func__, ret);
		return ret;
	}

	index = q6audio_get_port_index(port_id);
	if (index < 0 || index > AFE_MAX_PORTS) {
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

	mregion.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	mregion.hdr.pkt_size = sizeof(mregion);
	mregion.hdr.src_port = 0;
	mregion.hdr.dest_port = 0;
	mregion.hdr.token = index;
	mregion.hdr.opcode = AFE_SERVICE_CMD_SHARED_MEM_UNMAP_REGIONS;
	mregion.mem_map_handle = mem_map_handle;

	ret = anc_if_apr_send_pkt(&mregion, &this_anc_if.wait[index]);
	if (ret)
		pr_err("%s: msvc memory unmap cmd failed %d\n",
				__func__, ret);

	return ret;
}

static int __init sdsp_anc_init(void)
{
	int i = 0, ret = 0;

	atomic_set(&this_anc_if.state, 0);
	atomic_set(&this_anc_if.status, 0);
	this_anc_if.apr = NULL;
	this_anc_if.mmap_handle = 0;
	mutex_init(&this_anc_if.afe_cmd_lock);
	for (i = 0; i < AFE_MAX_PORTS; i++)
		init_waitqueue_head(&this_anc_if.wait[i]);

	return ret;
}

static void __exit sdsp_anc_exit(void)
{
	mutex_destroy(&this_anc_if.afe_cmd_lock);
}

device_initcall(sdsp_anc_init);
__exitcall(sdsp_anc_exit);
