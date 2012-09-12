/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <mach/qdsp6v2/audio_acdb.h>
#include <sound/apr_audio-v2.h>
#include <sound/q6afe-v2.h>

#include <sound/q6audio-v2.h>


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
};

static struct afe_ctl this_afe;

static struct acdb_cal_block afe_cal_addr[MAX_AUDPROC_TYPES];

#define TIMEOUT_MS 1000
#define Q6AFE_MAX_VOLUME 0x3FFF

#define SIZEOF_CFG_CMD(y) \
		(sizeof(struct apr_hdr) + sizeof(u16) + (sizeof(struct y)))

static int32_t afe_callback(struct apr_client_data *data, void *priv)
{
	if (data->opcode == RESET_EVENTS) {
		pr_debug("q6afe: reset event = %d %d apr[%p]\n",
			data->reset_event, data->reset_proc, this_afe.apr);
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
	pr_debug("%s:opcode = 0x%x cmd = 0x%x status = 0x%x\n",
				__func__, data->opcode,
				((uint32_t *)(data->payload))[0],
				((uint32_t *)(data->payload))[1]);
	if (data->payload_size) {
		uint32_t *payload;
		uint16_t port_id = 0;
		payload = data->payload;
		pr_debug("%s:opcode = 0x%x cmd = 0x%x status = 0x%x token=%d\n",
					__func__, data->opcode,
					payload[0], payload[1], data->token);
		/* payload[1] contains the error status for response */
		if (payload[1] != 0) {
			atomic_set(&this_afe.status, -1);
			pr_err("%s: cmd = 0x%x returned error = 0x%x\n",
					__func__, payload[0], payload[1]);
		}
		if (data->opcode == APR_BASIC_RSP_RESULT) {
			switch (payload[0]) {
			case AFE_PORT_CMD_DEVICE_STOP:
			case AFE_PORT_CMD_DEVICE_START:
			case AFE_PORT_CMD_SET_PARAM_V2:
			case AFE_PSEUDOPORT_CMD_START:
			case AFE_PSEUDOPORT_CMD_STOP:
			case AFE_SERVICE_CMD_SHARED_MEM_MAP_REGIONS:
			case AFE_SERVICE_CMD_SHARED_MEM_UNMAP_REGIONS:
			case AFE_SERVICE_CMD_UNREGISTER_RT_PORT_DRIVER:
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
	case PCM_RX:
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
	case RT_PROXY_PORT_001_RX:
		ret = MSM_AFE_PORT_TYPE_RX;
		break;

	case PRIMARY_I2S_TX:
	case PCM_TX:
	case SECONDARY_I2S_TX:
	case MI2S_TX:
	case DIGI_MIC_TX:
	case VOICE_RECORD_TX:
	case SLIMBUS_0_TX:
	case SLIMBUS_1_TX:
	case SLIMBUS_2_TX:
	case SLIMBUS_3_TX:
	case SLIMBUS_4_TX:
	case INT_FM_TX:
	case VOICE_RECORD_RX:
	case INT_BT_SCO_TX:
	case RT_PROXY_PORT_001_TX:
		ret = MSM_AFE_PORT_TYPE_TX;
		break;

	default:
		WARN_ON(1);
		pr_err("%s: invalid port id %d\n", __func__, port_id);
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
		ret_size = SIZEOF_CFG_CMD(afe_param_id_slimbus_cfg);
		break;
	case RT_PROXY_PORT_001_RX:
	case RT_PROXY_PORT_001_TX:
		ret_size = SIZEOF_CFG_CMD(afe_param_id_rt_proxy_port_cfg);
		break;
	case PCM_RX:
	case PCM_TX:
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
static void afe_send_cal_block(int32_t path, u16 port_id)
{
	/* To come back */
}

void afe_send_cal(u16 port_id)
{
	pr_debug("%s\n", __func__);

	if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_TX)
		afe_send_cal_block(TX_CAL, port_id);
	else if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_RX)
		afe_send_cal_block(RX_CAL, port_id);
}

int afe_port_start(u16 port_id, union afe_port_config *afe_config,
	u32 rate) /* This function is no blocking */
{
	struct afe_port_cmd_device_start start;
	struct afe_audioif_config_command config;
	int ret;
	int cfg_type;
	int index = 0;

	if (!afe_config) {
		pr_err("%s: Error, no configuration data\n", __func__);
		ret = -EINVAL;
		return ret;
	}
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
	if (IS_ERR_VALUE(ret))
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
		cfg_type = AFE_PARAM_ID_PCM_CONFIG;
		break;
	case PCM_RX:
	case PCM_TX:
		cfg_type = AFE_PARAM_ID_HDMI_CONFIG;
		break;
	case SECONDARY_I2S_RX:
	case SECONDARY_I2S_TX:
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
	config.param.payload_size = sizeof(config) - sizeof(struct apr_hdr) -
				    sizeof(config.param);
	config.param.payload_address_lsw = 0x00;
	config.param.payload_address_msw = 0x00;
	config.param.mem_map_handle = 0x00;
	config.pdata.module_id = AFE_MODULE_AUDIO_DEV_INTERFACE;
	config.pdata.param_id = cfg_type;
	config.pdata.param_size = sizeof(config.port);

	config.port = *afe_config;

	atomic_set(&this_afe.state, 1);
	atomic_set(&this_afe.status, 0);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &config);
	if (ret < 0) {
		pr_err("%s: AFE enable for port %d failed\n", __func__,
				port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = wait_event_timeout(this_afe.wait[index],
				(atomic_read(&this_afe.state) == 0),
					msecs_to_jiffies(TIMEOUT_MS));

	if (!ret) {
		pr_err("%s: wait_event timeout IF CONFIG\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	if (atomic_read(&this_afe.status) != 0) {
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	/* send AFE cal */
	afe_send_cal(port_id);

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

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &start);

	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: AFE enable for port %d failed\n", __func__,
				port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_afe.wait[index],
				(atomic_read(&this_afe.state) == 0),
					msecs_to_jiffies(TIMEOUT_MS));

	if (!ret) {
		pr_err("%s: wait_event timeout PORT START\n", __func__);
		 ret = -EINVAL;
		goto fail_cmd;
	}
	if (this_afe.task != current)
		this_afe.task = current;

	pr_debug("task_name = %s pid = %d\n",
		this_afe.task->comm, this_afe.task->pid);

	return 0;

fail_cmd:
	return ret;
}

int afe_get_port_index(u16 port_id)
{
	switch (port_id) {
	case PRIMARY_I2S_RX: return IDX_PRIMARY_I2S_RX;
	case PRIMARY_I2S_TX: return IDX_PRIMARY_I2S_TX;
	case PCM_RX: return IDX_PCM_RX;
	case PCM_TX: return IDX_PCM_TX;
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
	case PCM_RX:
	case PCM_TX:
		cfg_type = AFE_PARAM_ID_PCM_CONFIG;
		break;
	case SECONDARY_I2S_RX:
	case SECONDARY_I2S_TX:
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
	atomic_set(&this_afe.state, 1);
	atomic_set(&this_afe.status, 0);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &config);
	if (ret < 0) {
		pr_err("%s: AFE enable for port %d opcode[0x%x]failed\n",
			__func__, port_id, cfg_type);
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
	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &start);
	if (ret < 0) {
		pr_err("%s: AFE enable for port %d failed\n", __func__,
				port_id);
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

	return 0;
fail_cmd:
	return ret;
}

int afe_loopback(u16 enable, u16 rx_port, u16 tx_port)
{
	struct afe_loopback_cfg_v1 lb_cmd;
	int ret = 0;
	int index = 0;

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
	atomic_set(&this_afe.state, 1);

	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &lb_cmd);
	if (ret < 0) {
		pr_err("%s: AFE loopback failed\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	pr_debug("%s: waiting for this_afe.wait[%d]\n", __func__, index);
	ret = wait_event_timeout(this_afe.wait[index],
				 (atomic_read(&this_afe.state) == 0),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
	}
done:
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

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &set_param);
	if (ret < 0) {
		pr_err("%s: AFE param set failed for port %d\n",
					__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_afe.wait[index],
				 (atomic_read(&this_afe.state) == 0),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (ret < 0) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	return 0;
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

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &start);
	if (ret < 0) {
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
	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &start);
	if (ret < 0) {
		pr_err("%s: AFE enable for port %d failed %d\n",
		       __func__, port_id, ret);
		return -EINVAL;
	}

	ret = wait_event_timeout(this_afe.wait[index],
				 (atomic_read(&this_afe.state) == 0),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		return -EINVAL;
	}

	return 0;
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
	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &stop);
	if (ret < 0) {
		pr_err("%s: AFE close failed %d\n", __func__, ret);
		return -EINVAL;
	}

	return 0;

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
	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &stop);
	if (ret < 0) {
		pr_err("%s: AFE close failed %d\n", __func__, ret);
		return -EINVAL;
	}

	ret = wait_event_timeout(this_afe.wait[index],
				 (atomic_read(&this_afe.state) == 0),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		return -EINVAL;
	}

	return 0;
}

/*bharath, memory map handle needs to be stored by AFE client */
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
	mregion->hdr.pkt_size = sizeof(mregion);
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

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) mmap_region_cmd);
	if (ret < 0) {
		pr_err("%s: AFE memory map cmd failed %d\n",
		       __func__, ret);
		ret = -EINVAL;
		return ret;
	}

	ret = wait_event_timeout(this_afe.wait[index],
				 (atomic_read(&this_afe.state) == 0),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	return 0;
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

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) mmap_region_cmd);
	if (ret < 0) {
		pr_err("%s: AFE memory map cmd failed %d\n",
		       __func__, ret);
		ret = -EINVAL;
		return ret;
	}
	return 0;
}

int afe_cmd_memory_unmap(u32 mem_map_handle)
{
	int ret = 0;
	struct afe_service_cmd_shared_mem_unmap_regions mregion;
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

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &mregion);
	if (ret < 0) {
		pr_err("%s: AFE memory unmap cmd failed %d\n",
		       __func__, ret);
		ret = -EINVAL;
		return ret;
	}

	ret = wait_event_timeout(this_afe.wait[index],
				 (atomic_read(&this_afe.state) == 0),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
		return ret;
	}
	return 0;
}

int afe_cmd_memory_unmap_nowait(u32 mem_map_handle)
{
	int ret = 0;
	struct afe_service_cmd_shared_mem_unmap_regions mregion;

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

	mregion.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	mregion.hdr.pkt_size = sizeof(mregion);
	mregion.hdr.src_port = 0;
	mregion.hdr.dest_port = 0;
	mregion.hdr.token = 0;
	mregion.hdr.opcode = AFE_SERVICE_CMD_SHARED_MEM_UNMAP_REGIONS;
	mregion.mem_map_handle = mem_map_handle;

	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &mregion);
	if (ret < 0) {
		pr_err("%s: AFE memory unmap cmd failed %d\n",
			__func__, ret);
		ret = -EINVAL;
	}
	return 0;
}

int afe_register_get_events(u16 port_id,
		void (*cb) (uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv),
		void *private_data)
{
	int ret = 0;
	struct afe_service_cmd_register_rt_port_driver rtproxy;

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

	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &rtproxy);
	if (ret < 0) {
		pr_err("%s: AFE  reg. rtproxy_event failed %d\n",
			   __func__, ret);
		ret = -EINVAL;
		return ret;
	}
	return 0;
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
	index = q6audio_get_port_index(port_id);
	if (q6audio_validate_port(port_id) < 0)
		return -EINVAL;

	if ((port_id == RT_PROXY_DAI_002_RX) ||
		(port_id == RT_PROXY_DAI_001_TX))
		port_id = VIRTUAL_ID_TO_PORTID(port_id);
	else
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

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &rtproxy);
	if (ret < 0) {
		pr_err("%s: AFE enable Unreg. rtproxy_event failed %d\n",
			   __func__, ret);
		ret = -EINVAL;
		return ret;
	}

	ret = wait_event_timeout(this_afe.wait[index],
				 (atomic_read(&this_afe.state) == 0),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
		return ret;
	}
	return 0;
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

	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &afecmd_wr);
	if (ret < 0) {
		pr_err("%s: AFE rtproxy write to port 0x%x failed %d\n",
			   __func__, afecmd_wr.port_id, ret);
		ret = -EINVAL;
		return ret;
	}
	return 0;

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

	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &afecmd_rd);
	if (ret < 0) {
		pr_err("%s: AFE rtproxy read  cmd to port 0x%x failed %d\n",
			   __func__, afecmd_rd.port_id, ret);
		ret = -EINVAL;
		return ret;
	}
	return 0;
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
	S_IFREG | S_IWUGO, NULL, (void *) "afe_loopback",
	&afe_debug_fops);

	debugfs_afelb_gain = debugfs_create_file("afe_loopback_gain",
	S_IFREG | S_IWUGO, NULL, (void *) "afe_loopback_gain",
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

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &cmd_sidetone);
	if (ret < 0) {
		pr_err("%s: AFE sidetone failed for tx_port:%d rx_port:%d\n",
					__func__, tx_port_id, rx_port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_afe.wait[index],
		(atomic_read(&this_afe.state) == 0),
			msecs_to_jiffies(TIMEOUT_MS));
	if (ret < 0) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return ret;
}

int afe_validate_port(u16 port_id)
{
	int ret;

	switch (port_id) {
	case PRIMARY_I2S_RX:
	case PRIMARY_I2S_TX:
	case PCM_RX:
	case PCM_TX:
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

	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &stop);

	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: AFE close failed\n", __func__);
		ret = -EINVAL;
	}

fail_cmd:
	return ret;

}

int afe_close(int port_id)
{
	struct afe_port_cmd_device_stop stop;
	int ret = 0;
	int index = 0;


	if (this_afe.apr == NULL) {
		pr_err("AFE is already closed\n");
		ret = -EINVAL;
		goto fail_cmd;
	}
	pr_debug("%s: port_id=%d\n", __func__, port_id);

	index = q6audio_get_port_index(port_id);
	if (q6audio_validate_port(port_id) < 0)
		return -EINVAL;

	port_id = q6audio_convert_virtual_to_portid(port_id);

	stop.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	stop.hdr.pkt_size = sizeof(stop);
	stop.hdr.src_port = 0;
	stop.hdr.dest_port = 0;
	stop.hdr.token = index;
	stop.hdr.opcode = AFE_PORT_CMD_DEVICE_STOP;
	stop.port_id = q6audio_get_port_id(port_id);
	stop.reserved = 0;

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &stop);

	if (ret < 0) {
		pr_err("%s: AFE close failed\n", __func__);
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
fail_cmd:
	return ret;
}

static int __init afe_init(void)
{
	int i = 0;
	atomic_set(&this_afe.state, 0);
	atomic_set(&this_afe.status, 0);
	this_afe.apr = NULL;
	for (i = 0; i < AFE_MAX_PORTS; i++)
		init_waitqueue_head(&this_afe.wait[i]);

	config_debug_fs_init();
	return 0;
}

static void __exit afe_exit(void)
{
	int i;

	config_debug_fs_exit();
	for (i = 0; i < MAX_AUDPROC_TYPES; i++) {
		if (afe_cal_addr[i].cal_paddr != 0)
			afe_cmd_memory_unmap_nowait(
				afe_cal_addr[i].cal_paddr);
	}
}

device_initcall(afe_init);
__exitcall(afe_exit);
