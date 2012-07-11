/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/wait.h>

#include <mach/qdsp6v2/audio_acdb.h>
#include <mach/qdsp6v2/rtac.h>

#include <sound/apr_audio-v2.h>
#include <mach/qdsp6v2/apr.h>
#include <sound/q6adm-v2.h>
#include <sound/q6audio-v2.h>
#include <sound/q6afe-v2.h>

#define TIMEOUT_MS 1000

#define RESET_COPP_ID 99
#define INVALID_COPP_ID 0xFF

struct adm_ctl {
	void *apr;
	atomic_t copp_id[Q6_AFE_MAX_PORTS];
	atomic_t copp_cnt[Q6_AFE_MAX_PORTS];
	atomic_t copp_stat[Q6_AFE_MAX_PORTS];
	u32      mem_map_handle[Q6_AFE_MAX_PORTS];
	wait_queue_head_t wait[Q6_AFE_MAX_PORTS];
};

static struct adm_ctl			this_adm;

static int32_t adm_callback(struct apr_client_data *data, void *priv)
{
	uint32_t *payload;
	int i, index;
	payload = data->payload;

	if (data->opcode == RESET_EVENTS) {
		pr_debug("adm_callback: Reset event is received: %d %d apr[%p]\n",
				data->reset_event, data->reset_proc,
				this_adm.apr);
		if (this_adm.apr) {
			apr_reset(this_adm.apr);
			for (i = 0; i < Q6_AFE_MAX_PORTS; i++) {
				atomic_set(&this_adm.copp_id[i],
							RESET_COPP_ID);
				atomic_set(&this_adm.copp_cnt[i], 0);
				atomic_set(&this_adm.copp_stat[i], 0);
			}
			this_adm.apr = NULL;
		}
		return 0;
	}

	pr_debug("%s: code = 0x%x PL#0[%x], PL#1[%x], size = %d\n", __func__,
			data->opcode, payload[0], payload[1],
					data->payload_size);

	if (data->payload_size) {
		index = q6audio_get_port_index(data->token);
		if (index < 0 || index >= Q6_AFE_MAX_PORTS) {
			pr_err("%s: invalid port idx %d token %d\n",
					__func__, index, data->token);
			return 0;
		}
		if (data->opcode == APR_BASIC_RSP_RESULT) {
			pr_debug("APR_BASIC_RSP_RESULT id %x\n", payload[0]);
			switch (payload[0]) {
			case ADM_CMD_SET_PP_PARAMS_V5:
				if (rtac_make_adm_callback(
					payload, data->payload_size))
					pr_debug("%s: payload[0]: 0x%x\n",
						__func__, payload[0]);
					break;
			case ADM_CMD_DEVICE_CLOSE_V5:
			case ADM_CMD_SHARED_MEM_UNMAP_REGIONS:
			case ADM_CMD_SHARED_MEM_MAP_REGIONS:
			case ADM_CMD_MATRIX_MAP_ROUTINGS_V5:
				pr_debug("ADM_CMD_MATRIX_MAP_ROUTINGS\n");
				atomic_set(&this_adm.copp_stat[index], 1);
				wake_up(&this_adm.wait[index]);
				break;
			default:
				pr_err("%s: Unknown Cmd: 0x%x\n", __func__,
								payload[0]);
				break;
			}
			return 0;
		}

		switch (data->opcode) {
		case ADM_CMDRSP_DEVICE_OPEN_V5: {
			struct adm_cmd_rsp_device_open_v5 *open =
			(struct adm_cmd_rsp_device_open_v5 *)data->payload;
			if (open->copp_id == INVALID_COPP_ID) {
				pr_err("%s: invalid coppid rxed %d\n",
					__func__, open->copp_id);
				atomic_set(&this_adm.copp_stat[index], 1);
				wake_up(&this_adm.wait[index]);
				break;
			}
			atomic_set(&this_adm.copp_id[index], open->copp_id);
			atomic_set(&this_adm.copp_stat[index], 1);
			pr_debug("%s: coppid rxed=%d\n", __func__,
							open->copp_id);
			wake_up(&this_adm.wait[index]);
			}
			break;
		case ADM_CMD_GET_PP_PARAMS_V5:
			pr_debug("%s: ADM_CMD_GET_PP_PARAMS_V5\n", __func__);
			rtac_make_adm_callback(payload,
				data->payload_size);
			break;
		default:
			pr_err("%s: Unknown cmd:0x%x\n", __func__,
							data->opcode);
			break;
		}
	}
	return 0;
}

/* TODO: send_adm_cal_block function to be defined
	when calibration available for 8974 */
static void send_adm_cal(int port_id, int path)
{
	/* function to be defined when calibration available for 8974 */
	pr_debug("%s\n", __func__);
}

int adm_connect_afe_port(int mode, int session_id, int port_id)
{
	struct adm_cmd_connect_afe_port_v5	cmd;
	int ret = 0;
	int index;

	pr_debug("%s: port %d session id:%d mode:%d\n", __func__,
				port_id, session_id, mode);

	port_id = afe_convert_virtual_to_portid(port_id);

	if (afe_validate_port(port_id) < 0) {
		pr_err("%s port idi[%d] is invalid\n", __func__, port_id);
		return -ENODEV;
	}
	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_adm_handle(this_adm.apr);
	}
	index = afe_get_port_index(port_id);
	pr_debug("%s: Port ID %d, index %d\n", __func__, port_id, index);

	cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cmd.hdr.pkt_size = sizeof(cmd);
	cmd.hdr.src_svc = APR_SVC_ADM;
	cmd.hdr.src_domain = APR_DOMAIN_APPS;
	cmd.hdr.src_port = port_id;
	cmd.hdr.dest_svc = APR_SVC_ADM;
	cmd.hdr.dest_domain = APR_DOMAIN_ADSP;
	cmd.hdr.dest_port = port_id;
	cmd.hdr.token = port_id;
	cmd.hdr.opcode = ADM_CMD_CONNECT_AFE_PORT_V5;

	cmd.mode = mode;
	cmd.session_id = session_id;
	cmd.afe_port_id = port_id;

	atomic_set(&this_adm.copp_stat[index], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *)&cmd);
	if (ret < 0) {
		pr_err("%s:ADM enable for port %d failed\n",
					__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	/* Wait for the callback with copp id */
	ret = wait_event_timeout(this_adm.wait[index],
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s ADM connect AFE failed for port %d\n", __func__,
							port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	atomic_inc(&this_adm.copp_cnt[index]);
	return 0;

fail_cmd:

	return ret;
}

int adm_open(int port_id, int path, int rate, int channel_mode, int topology)
{
	struct adm_cmd_device_open_v5	open;
	int ret = 0;
	int index;
	int tmp_port = q6audio_get_port_id(port_id);

	pr_debug("%s: port %d path:%d rate:%d mode:%d\n", __func__,
				port_id, path, rate, channel_mode);

	port_id = q6audio_convert_virtual_to_portid(port_id);

	if (q6audio_validate_port(port_id) < 0) {
		pr_err("%s port idi[%d] is invalid\n", __func__, port_id);
		return -ENODEV;
	}

	index = q6audio_get_port_index(port_id);
	pr_debug("%s: Port ID %d, index %d\n", __func__, port_id, index);

	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_adm_handle(this_adm.apr);
	}


	/* Create a COPP if port id are not enabled */
	if (atomic_read(&this_adm.copp_cnt[index]) == 0) {

		open.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		open.hdr.pkt_size = sizeof(open);
		open.hdr.src_svc = APR_SVC_ADM;
		open.hdr.src_domain = APR_DOMAIN_APPS;
		open.hdr.src_port = tmp_port;
		open.hdr.dest_svc = APR_SVC_ADM;
		open.hdr.dest_domain = APR_DOMAIN_ADSP;
		open.hdr.dest_port = tmp_port;
		open.hdr.token = port_id;
		open.hdr.opcode = ADM_CMD_DEVICE_OPEN_V5;

		open.mode_of_operation = path;
		/* Reserved for future use, need to set this to 0 */
		open.flags = 0x00;
		open.endpoint_id_1 = tmp_port;
		open.endpoint_id_2 = 0xFFFF;

		/* convert path to acdb path */
		if (path == ADM_PATH_PLAYBACK)
			open.topology_id = get_adm_rx_topology();
		else {
			open.topology_id = get_adm_tx_topology();
			if ((open.topology_id ==
				VPM_TX_SM_ECNS_COPP_TOPOLOGY) ||
			    (open.topology_id ==
				VPM_TX_DM_FLUENCE_COPP_TOPOLOGY))
				rate = 16000;
		}

		if (open.topology_id  == 0)
			open.topology_id = topology;

		open.dev_num_channel = channel_mode & 0x00FF;
		open.bit_width = 16;
		open.sample_rate  = rate;
		memset(open.dev_channel_mapping, 0, 8);

		if (channel_mode == 1)	{
			open.dev_channel_mapping[0] = PCM_CHANNEL_FC;
		} else if (channel_mode == 2) {
			open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
		} else if (channel_mode == 6) {
			open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
			open.dev_channel_mapping[2] = PCM_CHANNEL_LFE;
			open.dev_channel_mapping[3] = PCM_CHANNEL_FC;
			open.dev_channel_mapping[4] = PCM_CHANNEL_LB;
			open.dev_channel_mapping[5] = PCM_CHANNEL_RB;
		} else {
			pr_err("%s invalid num_chan %d\n", __func__,
					channel_mode);
			return -EINVAL;
		}

		pr_debug("%s: port_id=%d rate=%d"
			"topology_id=0x%X\n", __func__,	open.endpoint_id_1, \
				  open.sample_rate, open.topology_id);

		atomic_set(&this_adm.copp_stat[index], 0);

		ret = apr_send_pkt(this_adm.apr, (uint32_t *)&open);
		if (ret < 0) {
			pr_err("%s:ADM enable for port %d for[%d] failed\n",
						__func__, tmp_port, port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}
		/* Wait for the callback with copp id */
		ret = wait_event_timeout(this_adm.wait[index],
			atomic_read(&this_adm.copp_stat[index]),
			msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s ADM open failed for port %d"
			"for [%d]\n", __func__, tmp_port, port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}
	}
	atomic_inc(&this_adm.copp_cnt[index]);
	return 0;

fail_cmd:

	return ret;
}


int adm_multi_ch_copp_open(int port_id, int path, int rate, int channel_mode,
				int topology)
{
	int ret = 0;

	ret = adm_open(port_id, path, rate, channel_mode, topology);

	return ret;
}

int adm_matrix_map(int session_id, int path, int num_copps,
			unsigned int *port_id, int copp_id)
{
	struct adm_cmd_matrix_map_routings_v5	*route;
	struct adm_session_map_node_v5 *node;
	uint32_t *copps_list;
	int cmd_size = 0;
	int ret = 0, i = 0;
	void *payload = NULL;
	void *matrix_map = NULL;

	/* Assumes port_ids have already been validated during adm_open */
	int index = q6audio_get_port_index(copp_id);
	if (index < 0 || index >= Q6_AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d token %d\n",
					__func__, index, copp_id);
		return 0;
	}
	cmd_size = (sizeof(struct adm_cmd_matrix_map_routings_v5) +
			sizeof(struct adm_session_map_node_v5) +
			(sizeof(uint32_t) * num_copps));
	matrix_map = kzalloc(cmd_size, GFP_KERNEL);
	if (matrix_map == NULL) {
		pr_err("%s: Mem alloc failed\n", __func__);
		ret = -EINVAL;
		return ret;
	}
	route = (struct adm_cmd_matrix_map_routings_v5 *)matrix_map;

	pr_debug("%s: session 0x%x path:%d num_copps:%d port_id[0] :%d coppid[%d]\n",
		 __func__, session_id, path, num_copps, port_id[0], copp_id);

	route->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	route->hdr.pkt_size = cmd_size;
	route->hdr.src_svc = 0;
	route->hdr.src_domain = APR_DOMAIN_APPS;
	route->hdr.src_port = copp_id;
	route->hdr.dest_svc = APR_SVC_ADM;
	route->hdr.dest_domain = APR_DOMAIN_ADSP;
	route->hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	route->hdr.token = copp_id;
	route->hdr.opcode = ADM_CMD_MATRIX_MAP_ROUTINGS_V5;
	route->num_sessions = 1;

	switch (path) {
	case 0x1:
		route->matrix_id = ADM_MATRIX_ID_AUDIO_RX;
		break;
	case 0x2:
	case 0x3:
		route->matrix_id = ADM_MATRIX_ID_AUDIO_TX;
		break;
	default:
		pr_err("%s: Wrong path set[%d]\n", __func__, path);
		break;
	}
	payload = ((u8 *)matrix_map +
			sizeof(struct adm_cmd_matrix_map_routings_v5));
	node = (struct adm_session_map_node_v5 *)payload;

	node->session_id = session_id;
	node->num_copps = num_copps;
	payload = (u8 *)node + sizeof(struct adm_session_map_node_v5);
	copps_list = (uint32_t *)payload;
	for (i = 0; i < num_copps; i++) {
		int tmp;
		port_id[i] = q6audio_convert_virtual_to_portid(port_id[i]);

		tmp = q6audio_get_port_index(port_id[i]);


		if (tmp >= 0 && tmp < Q6_AFE_MAX_PORTS)
			copps_list[i] =
					atomic_read(&this_adm.copp_id[tmp]);
		pr_debug("%s: port_id[%d]: %d, index: %d act coppid[0x%x]\n",
			__func__, i, port_id[i], tmp,
			atomic_read(&this_adm.copp_id[tmp]));
	}
	atomic_set(&this_adm.copp_stat[index], 0);

	ret = apr_send_pkt(this_adm.apr, (uint32_t *)matrix_map);
	if (ret < 0) {
		pr_err("%s: ADM routing for port %d failed\n",
					__func__, port_id[0]);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = wait_event_timeout(this_adm.wait[index],
				atomic_read(&this_adm.copp_stat[index]),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: ADM cmd Route failed for port %d\n",
					__func__, port_id[0]);
		ret = -EINVAL;
		goto fail_cmd;
	}
	for (i = 0; i < num_copps; i++)
		send_adm_cal(port_id[i], path);

fail_cmd:
	kfree(matrix_map);
	return ret;
}

int adm_memory_map_regions(int port_id,
		uint32_t *buf_add, uint32_t mempool_id,
		uint32_t *bufsz, uint32_t bufcnt)
{
	struct  avs_cmd_shared_mem_map_regions *mmap_regions = NULL;
	struct  avs_shared_map_region_payload *mregions = NULL;
	void    *mmap_region_cmd = NULL;
	void    *payload = NULL;
	int     ret = 0;
	int     i = 0;
	int     cmd_size = 0;
	int     index = 0;

	pr_debug("%s\n", __func__);
	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_adm_handle(this_adm.apr);
	}

	port_id = q6audio_convert_virtual_to_portid(port_id);

	if (q6audio_validate_port(port_id) < 0) {
		pr_err("%s port id[%d] is invalid\n", __func__, port_id);
		return -ENODEV;
	}

	index = q6audio_get_port_index(port_id);

	cmd_size = sizeof(struct avs_cmd_shared_mem_map_regions)
			+ sizeof(struct avs_shared_map_region_payload)
			* bufcnt;

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (!mmap_region_cmd) {
		pr_err("%s: allocate mmap_region_cmd failed\n", __func__);
		return -ENOMEM;
	}
	mmap_regions = (struct avs_cmd_shared_mem_map_regions *)mmap_region_cmd;
	mmap_regions->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
								APR_PKT_VER);
	mmap_regions->hdr.pkt_size = cmd_size;
	mmap_regions->hdr.src_port = 0;
	mmap_regions->hdr.dest_port = 0;
	mmap_regions->hdr.token = 0;
	mmap_regions->hdr.opcode = ADM_CMD_SHARED_MEM_MAP_REGIONS;
	mmap_regions->mem_pool_id = ADSP_MEMORY_MAP_SHMEM8_4K_POOL & 0x00ff;
	mmap_regions->num_regions = bufcnt & 0x00ff;
	mmap_regions->property_flag = 0x00;

	pr_debug("%s: map_regions->num_regions = %d\n", __func__,
				mmap_regions->num_regions);
	payload = ((u8 *) mmap_region_cmd +
				sizeof(struct avs_cmd_shared_mem_map_regions));
	mregions = (struct avs_shared_map_region_payload *)payload;

	for (i = 0; i < bufcnt; i++) {
		mregions->shm_addr_lsw = buf_add[i];
		mregions->shm_addr_msw = 0x00;
		mregions->mem_size_bytes = bufsz[i];
		++mregions;
	}

	atomic_set(&this_adm.copp_stat[0], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *) mmap_region_cmd);
	if (ret < 0) {
		pr_err("%s: mmap_regions op[0x%x]rc[%d]\n", __func__,
					mmap_regions->hdr.opcode, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_adm.wait[index],
			atomic_read(&this_adm.copp_stat[0]), 5 * HZ);
	if (!ret) {
		pr_err("%s: timeout. waited for memory_map\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
fail_cmd:
	kfree(mmap_region_cmd);
	return ret;
}

int adm_memory_unmap_regions(int32_t port_id, uint32_t *buf_add,
			uint32_t *bufsz, uint32_t bufcnt)
{
	struct  avs_cmd_shared_mem_unmap_regions unmap_regions;
	int     ret = 0;
	int     cmd_size = 0;
	int     index = 0;

	pr_debug("%s\n", __func__);

	if (this_adm.apr == NULL) {
		pr_err("%s APR handle NULL\n", __func__);
		return -EINVAL;
	}
	port_id = q6audio_convert_virtual_to_portid(port_id);

	if (q6audio_validate_port(port_id) < 0) {
		pr_err("%s port idi[%d] is invalid\n", __func__, port_id);
		return -ENODEV;
	}

	index = q6audio_get_port_index(port_id);

	unmap_regions.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
							APR_PKT_VER);
	unmap_regions.hdr.pkt_size = cmd_size;
	unmap_regions.hdr.src_port = 0;
	unmap_regions.hdr.dest_port = 0;
	unmap_regions.hdr.token = 0;
	unmap_regions.hdr.opcode = ADM_CMD_SHARED_MEM_UNMAP_REGIONS;
	unmap_regions.mem_map_handle = this_adm.mem_map_handle[index];
	atomic_set(&this_adm.copp_stat[0], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *) &unmap_regions);
	if (ret < 0) {
		pr_err("%s: mmap_regions op[0x%x]rc[%d]\n", __func__,
				unmap_regions.hdr.opcode, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_adm.wait[index],
			atomic_read(&this_adm.copp_stat[0]), 5 * HZ);
	if (!ret) {
		pr_err("%s: timeout. waited for memory_unmap\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
fail_cmd:
	return ret;
}

int adm_get_copp_id(int port_index)
{
	pr_debug("%s\n", __func__);

	if (port_index < 0) {
		pr_err("%s: invalid port_id = %d\n", __func__, port_index);
		return -EINVAL;
	}

	return atomic_read(&this_adm.copp_id[port_index]);
}

int adm_close(int port_id)
{
	struct apr_hdr close;

	int ret = 0;
	int index = 0;

	port_id = q6audio_convert_virtual_to_portid(port_id);

	index = q6audio_get_port_index(port_id);
	if (q6audio_validate_port(port_id) < 0)
		return -EINVAL;

	pr_debug("%s port_id=%d index %d\n", __func__, port_id, index);

	if (!(atomic_read(&this_adm.copp_cnt[index]))) {
		pr_err("%s: copp count for port[%d]is 0\n", __func__, port_id);

		goto fail_cmd;
	}
	atomic_dec(&this_adm.copp_cnt[index]);
	if (!(atomic_read(&this_adm.copp_cnt[index]))) {

		close.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		close.pkt_size = sizeof(close);
		close.src_svc = APR_SVC_ADM;
		close.src_domain = APR_DOMAIN_APPS;
		close.src_port = port_id;
		close.dest_svc = APR_SVC_ADM;
		close.dest_domain = APR_DOMAIN_ADSP;
		close.dest_port = atomic_read(&this_adm.copp_id[index]);
		close.token = port_id;
		close.opcode = ADM_CMD_DEVICE_CLOSE_V5;

		atomic_set(&this_adm.copp_id[index], RESET_COPP_ID);
		atomic_set(&this_adm.copp_stat[index], 0);


		pr_debug("%s:coppid %d portid=%d index=%d coppcnt=%d\n",
				__func__,
				atomic_read(&this_adm.copp_id[index]),
				port_id, index,
				atomic_read(&this_adm.copp_cnt[index]));

		ret = apr_send_pkt(this_adm.apr, (uint32_t *)&close);
		if (ret < 0) {
			pr_err("%s ADM close failed\n", __func__);
			ret = -EINVAL;
			goto fail_cmd;
		}

		ret = wait_event_timeout(this_adm.wait[index],
				atomic_read(&this_adm.copp_stat[index]),
				msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: ADM cmd Route failed for port %d\n",
						__func__, port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}

		rtac_remove_adm_device(port_id);
	}

fail_cmd:
	return ret;
}

static int __init adm_init(void)
{
	int i = 0;
	this_adm.apr = NULL;

	for (i = 0; i < Q6_AFE_MAX_PORTS; i++) {
		atomic_set(&this_adm.copp_id[i], RESET_COPP_ID);
		atomic_set(&this_adm.copp_cnt[i], 0);
		atomic_set(&this_adm.copp_stat[i], 0);
		init_waitqueue_head(&this_adm.wait[i]);
	}
	return 0;
}

device_initcall(adm_init);
