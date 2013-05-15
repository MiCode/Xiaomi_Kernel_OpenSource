/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <mach/qdsp6v2/audio_acdb.h>
#include <sound/apr_audio.h>
#include <sound/q6afe.h>

struct afe_ctl {
	void *apr;
	atomic_t state;
	atomic_t status;
	wait_queue_head_t wait;
	struct task_struct *task;
	void (*tx_cb) (uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv);
	void (*rx_cb) (uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv);
	void *tx_private_data;
	void *rx_private_data;
	u16 dtmf_gen_rx_portid;
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
		send_sig(SIGUSR1, this_afe.task, 0);
		return 0;
	}
	if (data->payload_size) {
		uint32_t *payload;
		uint16_t port_id = 0;
		payload = data->payload;
		pr_debug("%s:opcode = 0x%x cmd = 0x%x status = 0x%x\n",
					__func__, data->opcode,
					payload[0], payload[1]);
		/* payload[1] contains the error status for response */
		if (payload[1] != 0) {
			atomic_set(&this_afe.status, -1);
			pr_err("%s: cmd = 0x%x returned error = 0x%x\n",
					__func__, payload[0], payload[1]);
		}
		if (data->opcode == APR_BASIC_RSP_RESULT) {
			switch (payload[0]) {
			case AFE_PORT_AUDIO_IF_CONFIG:
			case AFE_PORT_CMD_I2S_CONFIG:
			case AFE_PORT_MULTI_CHAN_HDMI_AUDIO_IF_CONFIG:
			case AFE_PORT_AUDIO_SLIM_SCH_CONFIG:
			case AFE_PORT_CMD_STOP:
			case AFE_PORT_CMD_START:
			case AFE_PORT_CMD_LOOPBACK:
			case AFE_PORT_CMD_SIDETONE_CTL:
			case AFE_PORT_CMD_SET_PARAM:
			case AFE_PSEUDOPORT_CMD_START:
			case AFE_PSEUDOPORT_CMD_STOP:
			case AFE_PORT_CMD_APPLY_GAIN:
			case AFE_SERVICE_CMD_MEMORY_MAP:
			case AFE_SERVICE_CMD_MEMORY_UNMAP:
			case AFE_SERVICE_CMD_UNREG_RTPORT:
			case AFE_PORTS_CMD_DTMF_CTL:
				atomic_set(&this_afe.state, 0);
				wake_up(&this_afe.wait);
				break;
			case AFE_SERVICE_CMD_REG_RTPORT:
				break;
			case AFE_SERVICE_CMD_RTPORT_WR:
				port_id = RT_PROXY_PORT_001_TX;
				break;
			case AFE_SERVICE_CMD_RTPORT_RD:
				port_id = RT_PROXY_PORT_001_RX;
				break;
			default:
				pr_err("Unknown cmd 0x%x\n",
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
	case SECONDARY_PCM_RX:
	case SECONDARY_I2S_RX:
	case MI2S_RX:
	case HDMI_RX:
	case SLIMBUS_0_RX:
	case SLIMBUS_1_RX:
	case SLIMBUS_2_RX:
	case SLIMBUS_3_RX:
	case INT_BT_SCO_RX:
	case INT_BT_A2DP_RX:
	case INT_FM_RX:
	case VOICE_PLAYBACK_TX:
	case RT_PROXY_PORT_001_RX:
	case SLIMBUS_4_RX:
	case PSEUDOPORT_01:
		ret = MSM_AFE_PORT_TYPE_RX;
		break;

	case PRIMARY_I2S_TX:
	case PCM_TX:
	case SECONDARY_PCM_TX:
	case SECONDARY_I2S_TX:
	case MI2S_TX:
	case DIGI_MIC_TX:
	case VOICE_RECORD_TX:
	case SLIMBUS_0_TX:
	case SLIMBUS_1_TX:
	case SLIMBUS_2_TX:
	case SLIMBUS_3_TX:
	case INT_FM_TX:
	case VOICE_RECORD_RX:
	case INT_BT_SCO_TX:
	case RT_PROXY_PORT_001_TX:
	case SLIMBUS_4_TX:
		ret = MSM_AFE_PORT_TYPE_TX;
		break;

	default:
		pr_err("%s: invalid port id %d\n", __func__, port_id);
		ret = -EINVAL;
	}

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
	case SECONDARY_PCM_RX:
	case SECONDARY_PCM_TX:
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
	case SLIMBUS_3_TX:
	case INT_BT_SCO_RX:
	case INT_BT_SCO_TX:
	case INT_BT_A2DP_RX:
	case INT_FM_RX:
	case INT_FM_TX:
	case RT_PROXY_PORT_001_RX:
	case RT_PROXY_PORT_001_TX:
	case SLIMBUS_4_RX:
	case SLIMBUS_4_TX:
	case PSEUDOPORT_01:
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

	/* if port_id is virtual, convert to physical..
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

int afe_get_port_index(u16 port_id)
{
	switch (port_id) {
	case PRIMARY_I2S_RX: return IDX_PRIMARY_I2S_RX;
	case PRIMARY_I2S_TX: return IDX_PRIMARY_I2S_TX;
	case PCM_RX: return IDX_PCM_RX;
	case PCM_TX: return IDX_PCM_TX;
	case SECONDARY_PCM_RX: return IDX_SECONDARY_PCM_RX;
	case SECONDARY_PCM_TX: return IDX_SECONDARY_PCM_TX;
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
	case PSEUDOPORT_01: return IDX_PSEUDOPORT_01;

	default: return -EINVAL;
	}
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
		ret_size = SIZEOF_CFG_CMD(afe_port_mi2s_cfg);
		break;
	case HDMI_RX:
		ret_size = SIZEOF_CFG_CMD(afe_port_hdmi_multi_ch_cfg);
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
		ret_size = SIZEOF_CFG_CMD(afe_port_slimbus_sch_cfg);
		break;
	case RT_PROXY_PORT_001_RX:
	case RT_PROXY_PORT_001_TX:
		ret_size = SIZEOF_CFG_CMD(afe_port_rtproxy_cfg);
		break;
	case PSEUDOPORT_01:
		ret_size = SIZEOF_CFG_CMD(afe_port_pseudo_cfg);
		break;
	case PCM_RX:
	case PCM_TX:
	case SECONDARY_PCM_RX:
	case SECONDARY_PCM_TX:
	default:
		ret_size = SIZEOF_CFG_CMD(afe_port_pcm_cfg);
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
		pr_debug("%s: Register AFE\n", __func__);
		if (this_afe.apr == NULL) {
			pr_err("%s: Unable to register AFE\n", __func__);
			ret = -ENODEV;
		}
	}
	return ret;
}

static void afe_send_cal_block(int32_t path, u16 port_id)
{
	int						result = 0;
	struct acdb_cal_block				cal_block;
	struct afe_port_cmd_set_param_no_payload	afe_cal;
	pr_debug("%s: path %d\n", __func__, path);

	get_afe_cal(path, &cal_block);
	if (cal_block.cal_size <= 0) {
		pr_debug("%s: No AFE cal to send!\n", __func__);
		goto done;
	}

	if ((afe_cal_addr[path].cal_paddr != cal_block.cal_paddr) ||
		(cal_block.cal_size > afe_cal_addr[path].cal_size)) {
		if (afe_cal_addr[path].cal_paddr != 0)
			afe_cmd_memory_unmap(
				afe_cal_addr[path].cal_paddr);

		afe_cmd_memory_map(cal_block.cal_paddr, cal_block.cal_size);
		afe_cal_addr[path].cal_paddr = cal_block.cal_paddr;
		afe_cal_addr[path].cal_size = cal_block.cal_size;
	}

	afe_cal.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	afe_cal.hdr.pkt_size = sizeof(afe_cal);
	afe_cal.hdr.src_port = 0;
	afe_cal.hdr.dest_port = 0;
	afe_cal.hdr.token = 0;
	afe_cal.hdr.opcode = AFE_PORT_CMD_SET_PARAM;
	afe_cal.port_id = port_id;
	afe_cal.payload_size = cal_block.cal_size;
	afe_cal.payload_address = cal_block.cal_paddr;

	pr_debug("%s: AFE cal sent for device port = %d, path = %d, "
		"cal size = %d, cal addr = 0x%x\n", __func__,
		port_id, path, cal_block.cal_size, cal_block.cal_paddr);

	atomic_set(&this_afe.state, 1);
	result = apr_send_pkt(this_afe.apr, (uint32_t *) &afe_cal);
	if (result < 0) {
		pr_err("%s: AFE cal for port %d failed\n",
			__func__, port_id);
	}

	result = wait_event_timeout(this_afe.wait,
				 (atomic_read(&this_afe.state) == 0),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!result) {
		pr_err("%s: wait_event timeout SET AFE CAL\n", __func__);
		goto done;
	}

	pr_debug("%s: AFE cal sent for path %d device!\n", __func__, path);
done:
	return;
}

void afe_send_cal(u16 port_id)
{
	pr_debug("%s\n", __func__);

	if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_TX)
		afe_send_cal_block(TX_CAL, port_id);
	else if (afe_get_port_type(port_id) == MSM_AFE_PORT_TYPE_RX)
		afe_send_cal_block(RX_CAL, port_id);
}

/* This function sends multi-channel HDMI configuration command and AFE
 * calibration which is only supported by QDSP6 on 8960 and onward.
 */
int afe_port_start(u16 port_id, union afe_port_config *afe_config,
		   u32 rate)
{
	struct afe_port_start_command start;
	struct afe_audioif_config_command config;
	int ret;

	if (!afe_config) {
		pr_err("%s: Error, no configuration data\n", __func__);
		ret = -EINVAL;
		return ret;
	}
	pr_debug("%s: %d %d\n", __func__, port_id, rate);

	if ((port_id == RT_PROXY_DAI_001_RX) ||
		(port_id == RT_PROXY_DAI_002_TX))
		return 0;
	if ((port_id == RT_PROXY_DAI_002_RX) ||
		(port_id == RT_PROXY_DAI_001_TX))
		port_id = VIRTUAL_ID_TO_PORTID(port_id);

	ret = afe_q6_interface_prepare();
	if (IS_ERR_VALUE(ret))
		return ret;

	if (port_id == HDMI_RX) {
		config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		config.hdr.pkt_size = afe_sizeof_cfg_cmd(port_id);
		config.hdr.src_port = 0;
		config.hdr.dest_port = 0;
		config.hdr.token = 0;
		config.hdr.opcode = AFE_PORT_MULTI_CHAN_HDMI_AUDIO_IF_CONFIG;
	} else {

		config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		config.hdr.pkt_size = afe_sizeof_cfg_cmd(port_id);
		config.hdr.src_port = 0;
		config.hdr.dest_port = 0;
		config.hdr.token = 0;
		switch (port_id) {
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
			config.hdr.opcode = AFE_PORT_AUDIO_SLIM_SCH_CONFIG;
		break;
		case MI2S_TX:
		case MI2S_RX:
		case SECONDARY_I2S_RX:
		case SECONDARY_I2S_TX:
		case PRIMARY_I2S_RX:
		case PRIMARY_I2S_TX:
			/* AFE_PORT_CMD_I2S_CONFIG command is not supported
			 * in the LPASS EL 1.0. So we have to distiguish
			 * which AFE command, AFE_PORT_CMD_I2S_CONFIG or
			 * AFE_PORT_AUDIO_IF_CONFIG	to use. If the format
			 * is L-PCM, the AFE_PORT_AUDIO_IF_CONFIG is used
			 * to make the backward compatible.
			 */
			pr_debug("%s: afe_config->mi2s.format = %d\n", __func__,
					 afe_config->mi2s.format);
			if (afe_config->mi2s.format == MSM_AFE_I2S_FORMAT_LPCM)
				config.hdr.opcode = AFE_PORT_AUDIO_IF_CONFIG;
			else
				config.hdr.opcode = AFE_PORT_CMD_I2S_CONFIG;
		break;
		case PSEUDOPORT_01:
			config.hdr.opcode = AFE_PORT_AUDIO_IF_CONFIG;
			pr_debug("%s, config, opcode=%x\n", __func__,
					config.hdr.opcode);
		break;
		default:
			config.hdr.opcode = AFE_PORT_AUDIO_IF_CONFIG;
		break;
		}
	}

	if (afe_validate_port(port_id) < 0) {

		pr_err("%s: Failed : Invalid Port id = %d\n", __func__,
				port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	config.port_id = port_id;
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

	ret = wait_event_timeout(this_afe.wait,
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
	start.hdr.token = 0;
	start.hdr.opcode = AFE_PORT_CMD_START;
	start.port_id = port_id;
	start.gain = 0x2000;
	start.sample_rate = rate;

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &start);

	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: AFE enable for port %d failed\n", __func__,
				port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_afe.wait,
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

/* This function should be used by 8660 exclusively */
int afe_open(u16 port_id, union afe_port_config *afe_config, int rate)
{
	struct afe_port_start_command start;
	struct afe_audioif_config_command config;
	int ret = 0;

	if (!afe_config) {
		pr_err("%s: Error, no configuration data\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	pr_debug("%s: %d %d\n", __func__, port_id, rate);

	if ((port_id == RT_PROXY_DAI_001_RX) ||
		(port_id == RT_PROXY_DAI_002_TX))
		return 0;
	if ((port_id == RT_PROXY_DAI_002_RX) ||
		(port_id == RT_PROXY_DAI_001_TX))
		port_id = VIRTUAL_ID_TO_PORTID(port_id);

	ret = afe_q6_interface_prepare();
	if (ret != 0)
		return ret;

	config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	config.hdr.pkt_size = afe_sizeof_cfg_cmd(port_id);
	config.hdr.src_port = 0;
	config.hdr.dest_port = 0;
	config.hdr.token = 0;
	switch (port_id) {
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
		config.hdr.opcode = AFE_PORT_AUDIO_SLIM_SCH_CONFIG;
	break;
	case MI2S_TX:
	case MI2S_RX:
	case SECONDARY_I2S_RX:
	case SECONDARY_I2S_TX:
	case PRIMARY_I2S_RX:
	case PRIMARY_I2S_TX:
		/* AFE_PORT_CMD_I2S_CONFIG command is not supported
		 * in the LPASS EL 1.0. So we have to distiguish
		 * which AFE command, AFE_PORT_CMD_I2S_CONFIG or
		 * AFE_PORT_AUDIO_IF_CONFIG	to use. If the format
		 * is L-PCM, the AFE_PORT_AUDIO_IF_CONFIG is used
		 * to make the backward compatible.
		 */
		pr_debug("%s: afe_config->mi2s.format = %d\n", __func__,
				 afe_config->mi2s.format);
		if (afe_config->mi2s.format == MSM_AFE_I2S_FORMAT_LPCM)
			config.hdr.opcode = AFE_PORT_AUDIO_IF_CONFIG;
		else
			config.hdr.opcode = AFE_PORT_CMD_I2S_CONFIG;
	break;
	default:
		config.hdr.opcode = AFE_PORT_AUDIO_IF_CONFIG;
	break;
	}

	if (afe_validate_port(port_id) < 0) {

		pr_err("%s: Failed : Invalid Port id = %d\n", __func__,
				port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	config.port_id = port_id;
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

	ret = wait_event_timeout(this_afe.wait,
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
	start.hdr.token = 0;
	start.hdr.opcode = AFE_PORT_CMD_START;
	start.port_id = port_id;
	start.gain = 0x2000;
	start.sample_rate = rate;

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &start);
	if (ret < 0) {
		pr_err("%s: AFE enable for port %d failed\n", __func__,
				port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = wait_event_timeout(this_afe.wait,
			(atomic_read(&this_afe.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
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

int afe_loopback(u16 enable, u16 dst_port, u16 src_port)
{
	struct afe_loopback_command lb_cmd;
	int ret = 0;

	ret = afe_q6_interface_prepare();
	if (ret != 0)
		return ret;

	if ((afe_get_port_type(dst_port) == MSM_AFE_PORT_TYPE_RX) &&
		(afe_get_port_type(src_port) == MSM_AFE_PORT_TYPE_RX))
		return afe_loopback_cfg(enable, dst_port, src_port,
					LB_MODE_EC_REF_VOICE_AUDIO);

	lb_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(20), APR_PKT_VER);
	lb_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
						sizeof(lb_cmd) - APR_HDR_SIZE);
	lb_cmd.hdr.src_port = 0;
	lb_cmd.hdr.dest_port = 0;
	lb_cmd.hdr.token = 0;
	lb_cmd.hdr.opcode = AFE_PORT_CMD_LOOPBACK;
	lb_cmd.tx_port_id = src_port;
	lb_cmd.rx_port_id = dst_port;
	lb_cmd.mode = 0xFFFF;
	lb_cmd.enable = (enable ? 1 : 0);
	atomic_set(&this_afe.state, 1);

	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &lb_cmd);
	if (ret < 0) {
		pr_err("%s: AFE loopback failed\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	ret = wait_event_timeout(this_afe.wait,
		(atomic_read(&this_afe.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
	}
done:
	return ret;
}

int afe_loopback_cfg(u16 enable, u16 dst_port, u16 src_port, u16 mode)
{
	struct afe_port_cmd_set_param lp_cfg;
	int ret = 0;

	ret = afe_q6_interface_prepare();
	if (ret != 0)
		return ret;

	pr_debug("%s: src_port %d, dst_port %d\n",
		  __func__, src_port, dst_port);

	lp_cfg.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	lp_cfg.hdr.pkt_size = sizeof(lp_cfg);
	lp_cfg.hdr.src_port = 0;
	lp_cfg.hdr.dest_port = 0;
	lp_cfg.hdr.token = 0;
	lp_cfg.hdr.opcode = AFE_PORT_CMD_SET_PARAM;

	lp_cfg.port_id = src_port;
	lp_cfg.payload_size	= sizeof(struct afe_param_payload_base) +
		sizeof(struct afe_param_loopback_cfg);
	lp_cfg.payload_address	= 0;

	lp_cfg.payload.base.module_id = AFE_MODULE_LOOPBACK;
	lp_cfg.payload.base.param_id	= AFE_PARAM_ID_LOOPBACK_CONFIG;
	lp_cfg.payload.base.param_size = sizeof(struct afe_param_loopback_cfg);
	lp_cfg.payload.base.reserved	= 0;

	lp_cfg.payload.param.loopback_cfg.loopback_cfg_minor_version =
			AFE_API_VERSION_LOOPBACK_CONFIG;
	lp_cfg.payload.param.loopback_cfg.dst_port_id = dst_port;
	lp_cfg.payload.param.loopback_cfg.routing_mode = mode;
	lp_cfg.payload.param.loopback_cfg.enable = enable;
	lp_cfg.payload.param.loopback_cfg.reserved = 0;

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &lp_cfg);
	if (ret < 0) {
		pr_err("%s: AFE loopback config failed for src_port %d, dst_port %d\n",
			   __func__, src_port, dst_port);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_afe.wait,
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

int afe_loopback_gain(u16 port_id, u16 volume)
{
	struct afe_port_cmd_set_param set_param;
	int ret = 0;

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

	if (afe_validate_port(port_id) < 0) {

		pr_err("%s: Failed : Invalid Port id = %d\n", __func__,
				port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	/* RX ports numbers are even .TX ports numbers are odd. */
	if (port_id % 2 == 0) {
		pr_err("%s: Failed : afe loopback gain only for TX ports."
			" port_id %d\n", __func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	pr_debug("%s: %d %hX\n", __func__, port_id, volume);

	set_param.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	set_param.hdr.pkt_size = sizeof(set_param);
	set_param.hdr.src_port = 0;
	set_param.hdr.dest_port = 0;
	set_param.hdr.token = 0;
	set_param.hdr.opcode = AFE_PORT_CMD_SET_PARAM;

	set_param.port_id		= port_id;
	set_param.payload_size	= sizeof(struct afe_param_payload_base) +
				  sizeof(struct afe_param_loopback_gain);
	set_param.payload_address	= 0;

	set_param.payload.base.module_id	= AFE_MODULE_ID_PORT_INFO;
	set_param.payload.base.param_id	= AFE_PARAM_ID_LOOPBACK_GAIN;
	set_param.payload.base.param_size =
		sizeof(struct afe_param_loopback_gain);
	set_param.payload.base.reserved	= 0;

	set_param.payload.param.loopback_gain.gain		= volume;
	set_param.payload.param.loopback_gain.reserved	= 0;

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &set_param);
	if (ret < 0) {
		pr_err("%s: AFE param set failed for port %d\n",
					__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_afe.wait,
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

int afe_apply_gain(u16 port_id, u16 gain)
{
	struct afe_port_gain_command set_gain;
	int ret = 0;

	if (this_afe.apr == NULL) {
		pr_err("%s: AFE is not opened\n", __func__);
		ret = -EPERM;
		goto fail_cmd;
	}

	if (afe_validate_port(port_id) < 0) {
		pr_err("%s: Failed : Invalid Port id = %d\n", __func__,
				port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	/* RX ports numbers are even .TX ports numbers are odd. */
	if (port_id % 2 == 0) {
		pr_err("%s: Failed : afe apply gain only for TX ports."
			" port_id %d\n", __func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	pr_debug("%s: %d %hX\n", __func__, port_id, gain);

	set_gain.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	set_gain.hdr.pkt_size = sizeof(set_gain);
	set_gain.hdr.src_port = 0;
	set_gain.hdr.dest_port = 0;
	set_gain.hdr.token = 0;
	set_gain.hdr.opcode = AFE_PORT_CMD_APPLY_GAIN;

	set_gain.port_id		= port_id;
	set_gain.gain	= gain;

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &set_gain);
	if (ret < 0) {
		pr_err("%s: AFE Gain set failed for port %d\n",
					__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_afe.wait,
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

int afe_pseudo_port_start_nowait(u16 port_id)
{
	int ret = 0;
	struct afe_pseudoport_start_command start;

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

	pr_debug("%s: port_id=%d\n", __func__, port_id);

	ret = afe_q6_interface_prepare();
	if (ret != 0)
		return ret;

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

	ret = wait_event_timeout(this_afe.wait,
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

	pr_debug("%s: port_id=%d\n", __func__, port_id);

	if (this_afe.apr == NULL) {
		pr_err("%s: AFE is already closed\n", __func__);
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

	pr_debug("%s: port_id=%d\n", __func__, port_id);

	if (this_afe.apr == NULL) {
		pr_err("%s: AFE is already closed\n", __func__);
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

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &stop);
	if (ret < 0) {
		pr_err("%s: AFE close failed %d\n", __func__, ret);
		return -EINVAL;
	}

	ret = wait_event_timeout(this_afe.wait,
				 (atomic_read(&this_afe.state) == 0),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		return -EINVAL;
	}

	return 0;
}

int afe_cmd_memory_map(u32 dma_addr_p, u32 dma_buf_sz)
{
	int ret = 0;
	struct afe_cmd_memory_map mregion;

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
	mregion.hdr.opcode = AFE_SERVICE_CMD_MEMORY_MAP;
	mregion.phy_addr = dma_addr_p;
	mregion.mem_sz = dma_buf_sz;
	mregion.mem_id = 0;
	mregion.rsvd = 0;

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &mregion);
	if (ret < 0) {
		pr_err("%s: AFE memory map cmd failed %d\n",
		       __func__, ret);
		ret = -EINVAL;
		return ret;
	}

	ret = wait_event_timeout(this_afe.wait,
				 (atomic_read(&this_afe.state) == 0),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	return 0;
}

int afe_cmd_memory_map_nowait(u32 dma_addr_p, u32 dma_buf_sz)
{
	int ret = 0;
	struct afe_cmd_memory_map mregion;

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
	mregion.hdr.opcode = AFE_SERVICE_CMD_MEMORY_MAP;
	mregion.phy_addr = dma_addr_p;
	mregion.mem_sz = dma_buf_sz;
	mregion.mem_id = 0;
	mregion.rsvd = 0;

	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &mregion);
	if (ret < 0) {
		pr_err("%s: AFE memory map cmd failed %d\n",
			__func__, ret);
		ret = -EINVAL;
	}
	return 0;
}

int afe_cmd_memory_unmap(u32 dma_addr_p)
{
	int ret = 0;
	struct afe_cmd_memory_unmap mregion;

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
	mregion.hdr.opcode = AFE_SERVICE_CMD_MEMORY_UNMAP;
	mregion.phy_addr = dma_addr_p;

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &mregion);
	if (ret < 0) {
		pr_err("%s: AFE memory unmap cmd failed %d\n",
		       __func__, ret);
		ret = -EINVAL;
		return ret;
	}

	ret = wait_event_timeout(this_afe.wait,
				 (atomic_read(&this_afe.state) == 0),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
		return ret;
	}
	return 0;
}

int afe_cmd_memory_unmap_nowait(u32 dma_addr_p)
{
	int ret = 0;
	struct afe_cmd_memory_unmap mregion;

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
	mregion.hdr.opcode = AFE_SERVICE_CMD_MEMORY_UNMAP;
	mregion.phy_addr = dma_addr_p;

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
	struct afe_cmd_reg_rtport rtproxy;

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
	rtproxy.hdr.token = 0;
	rtproxy.hdr.opcode = AFE_SERVICE_CMD_REG_RTPORT;
	rtproxy.port_id = port_id;
	rtproxy.rsvd = 0;

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
	struct afe_cmd_unreg_rtport rtproxy;

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

	rtproxy.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	rtproxy.hdr.pkt_size = sizeof(rtproxy);
	rtproxy.hdr.src_port = 0;
	rtproxy.hdr.dest_port = 0;
	rtproxy.hdr.token = 0;
	rtproxy.hdr.opcode = AFE_SERVICE_CMD_UNREG_RTPORT;
	rtproxy.port_id = port_id;
	rtproxy.rsvd = 0;

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

	ret = wait_event_timeout(this_afe.wait,
				 (atomic_read(&this_afe.state) == 0),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
		return ret;
	}
	return 0;
}

int afe_rt_proxy_port_write(u32 buf_addr_p, int bytes)
{
	int ret = 0;
	struct afe_cmd_rtport_wr afecmd_wr;

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
	afecmd_wr.hdr.opcode = AFE_SERVICE_CMD_RTPORT_WR;
	afecmd_wr.buf_addr = (uint32_t)buf_addr_p;
	afecmd_wr.port_id = RT_PROXY_PORT_001_TX;
	afecmd_wr.bytes_avail = bytes;
	afecmd_wr.rsvd = 0;

	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &afecmd_wr);
	if (ret < 0) {
		pr_err("%s: AFE rtproxy write to port 0x%x failed %d\n",
			   __func__, afecmd_wr.port_id, ret);
		ret = -EINVAL;
		return ret;
	}
	return 0;

}

int afe_rt_proxy_port_read(u32 buf_addr_p, int bytes)
{
	int ret = 0;
	struct afe_cmd_rtport_rd afecmd_rd;

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
	afecmd_rd.hdr.opcode = AFE_SERVICE_CMD_RTPORT_RD;
	afecmd_rd.buf_addr = (uint32_t)buf_addr_p;
	afecmd_rd.port_id = RT_PROXY_PORT_001_RX;
	afecmd_rd.bytes_avail = bytes;
	afecmd_rd.rsvd = 0;

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

			if (strict_strtoul(token, base, &param1[cnt]) != 0)
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

	if (!strcmp(lb_str, "afe_loopback")) {
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
			if ((afe_validate_port(param[1]) < 0) ||
			    (afe_validate_port(param[2])) < 0) {
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
			pr_info("%s %lu %lu\n", lb_str, param[0], param[1]);

			if (afe_validate_port(param[0]) < 0) {
				pr_err("%s: Error, invalid afe port\n",
					__func__);
				rc = -EINVAL;
				goto afe_error;
			}

			if (param[1] > 100) {
				pr_err("%s: Error, volume shoud be 0 to 100"
					" percentage param = %lu\n",
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
	cmd_dtmf.port_ids = this_afe.dtmf_gen_rx_portid;

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &cmd_dtmf);
	if (ret < 0) {
		pr_err("%s: AFE DTMF failed for num_ports:%d ids:%x\n",
		       __func__, cmd_dtmf.num_ports, cmd_dtmf.port_ids);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_afe.wait,
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

int afe_sidetone(u16 tx_port_id, u16 rx_port_id, u16 enable, uint16_t gain)
{
	struct afe_port_sidetone_command cmd_sidetone;
	int ret = 0;

	pr_info("%s: tx_port_id:%d rx_port_id:%d enable:%d gain:%d\n", __func__,
					tx_port_id, rx_port_id, enable, gain);
	cmd_sidetone.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cmd_sidetone.hdr.pkt_size = sizeof(cmd_sidetone);
	cmd_sidetone.hdr.src_port = 0;
	cmd_sidetone.hdr.dest_port = 0;
	cmd_sidetone.hdr.token = 0;
	cmd_sidetone.hdr.opcode = AFE_PORT_CMD_SIDETONE_CTL;
	cmd_sidetone.tx_port_id = tx_port_id;
	cmd_sidetone.rx_port_id = rx_port_id;
	cmd_sidetone.gain = gain;
	cmd_sidetone.enable = enable;

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &cmd_sidetone);
	if (ret < 0) {
		pr_err("%s: AFE sidetone failed for tx_port:%d rx_port:%d\n",
					__func__, tx_port_id, rx_port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_afe.wait,
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

int afe_port_stop_nowait(int port_id)
{
	struct afe_port_stop_command stop;
	int ret = 0;

	if (this_afe.apr == NULL) {
		pr_err("AFE is already closed\n");
		ret = -EINVAL;
		goto fail_cmd;
	}
	pr_debug("%s: port_id=%d\n", __func__, port_id);
	port_id = afe_convert_virtual_to_portid(port_id);

	stop.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	stop.hdr.pkt_size = sizeof(stop);
	stop.hdr.src_port = 0;
	stop.hdr.dest_port = 0;
	stop.hdr.token = 0;
	stop.hdr.opcode = AFE_PORT_CMD_STOP;
	stop.port_id = port_id;
	stop.reserved = 0;

	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &stop);

	if (ret == -ENETRESET) {
		pr_info("%s: Need to reset, calling APR deregister", __func__);
		return apr_deregister(this_afe.apr);
	} else if (IS_ERR_VALUE(ret)) {
		pr_err("%s: AFE close failed\n", __func__);
		ret = -EINVAL;
	}

fail_cmd:
	return ret;

}

int afe_close(int port_id)
{
	struct afe_port_stop_command stop;
	int ret = 0;

	if (this_afe.apr == NULL) {
		pr_err("AFE is already closed\n");
		ret = -EINVAL;
		goto fail_cmd;
	}
	pr_debug("%s: port_id=%d\n", __func__, port_id);
	if ((port_id == RT_PROXY_DAI_001_RX) ||
		(port_id == RT_PROXY_DAI_002_TX))
		return 0;
	port_id = afe_convert_virtual_to_portid(port_id);

	stop.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	stop.hdr.pkt_size = sizeof(stop);
	stop.hdr.src_port = 0;
	stop.hdr.dest_port = 0;
	stop.hdr.token = 0;
	stop.hdr.opcode = AFE_PORT_CMD_STOP;
	stop.port_id = port_id;
	stop.reserved = 0;

	atomic_set(&this_afe.state, 1);
	ret = apr_send_pkt(this_afe.apr, (uint32_t *) &stop);

	if (ret == -ENETRESET) {
		pr_info("%s: Need to reset, calling APR deregister", __func__);
		return apr_deregister(this_afe.apr);
	}

	if (ret < 0) {
		pr_err("%s: AFE close failed\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_afe.wait,
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
	init_waitqueue_head(&this_afe.wait);
	atomic_set(&this_afe.state, 0);
	atomic_set(&this_afe.status, 0);
	this_afe.apr = NULL;
	this_afe.dtmf_gen_rx_portid = -1;
#ifdef CONFIG_DEBUG_FS
	debugfs_afelb = debugfs_create_file("afe_loopback",
	S_IFREG | S_IWUGO, NULL, (void *) "afe_loopback",
	&afe_debug_fops);

	debugfs_afelb_gain = debugfs_create_file("afe_loopback_gain",
	S_IFREG | S_IWUGO, NULL, (void *) "afe_loopback_gain",
	&afe_debug_fops);


#endif
	return 0;
}

static void __exit afe_exit(void)
{
	int i;
#ifdef CONFIG_DEBUG_FS
	if (debugfs_afelb)
		debugfs_remove(debugfs_afelb);
	if (debugfs_afelb_gain)
		debugfs_remove(debugfs_afelb_gain);
#endif
	for (i = 0; i < MAX_AUDPROC_TYPES; i++) {
		if (afe_cal_addr[i].cal_paddr != 0)
			afe_cmd_memory_unmap_nowait(
				afe_cal_addr[i].cal_paddr);
	}
}

device_initcall(afe_init);
__exitcall(afe_exit);
