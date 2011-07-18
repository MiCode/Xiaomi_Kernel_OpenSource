/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/bitops.h>
#include <mach/qdsp6v2/rtac.h>
#include <mach/qdsp6v2/audio_acdb.h>
#include <sound/apr_audio.h>
#include <sound/q6afe.h>
#include <mach/qdsp6v2/audio_dev_ctl.h>

#define TIMEOUT_MS 1000
#define AUDIO_RX 0x0
#define AUDIO_TX 0x1
#define ASM_MAX_SESSION 0x8 /* To do: define in a header */
#define RESET_COPP_ID 99
#define INVALID_COPP_ID 0xFF

struct adm_ctl {
	void *apr;
	atomic_t copp_id[AFE_MAX_PORTS];
	atomic_t copp_cnt[AFE_MAX_PORTS];
	atomic_t copp_stat[AFE_MAX_PORTS];
	unsigned long sessions[AFE_MAX_PORTS];
	wait_queue_head_t wait;
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
			for (i = 0; i < AFE_MAX_PORTS; i++) {
				atomic_set(&this_adm.copp_id[i],
							RESET_COPP_ID);
				atomic_set(&this_adm.copp_cnt[i], 0);
				atomic_set(&this_adm.copp_stat[i], 0);
			}
			this_adm.apr = NULL;
		}
		return 0;
	}

	pr_debug("%s: code = 0x%x %x %x size = %d\n", __func__,
			data->opcode, payload[0], payload[1],
					data->payload_size);

	if (data->payload_size) {
		index = afe_get_port_index(data->token);
		pr_debug("%s: Port ID %d, index %d\n", __func__,
			data->token, index);

		if (data->opcode == APR_BASIC_RSP_RESULT) {
			pr_debug("APR_BASIC_RSP_RESULT\n");
			switch (payload[0]) {
			case ADM_CMD_SET_PARAMS:
#ifdef CONFIG_MSM8X60_RTAC
				if (rtac_make_adm_callback(payload,
						data->payload_size))
					break;
#endif
			case ADM_CMD_COPP_CLOSE:
			case ADM_CMD_MEMORY_MAP:
			case ADM_CMD_MEMORY_UNMAP:
			case ADM_CMD_MEMORY_MAP_REGIONS:
			case ADM_CMD_MEMORY_UNMAP_REGIONS:
			case ADM_CMD_MATRIX_MAP_ROUTINGS:
				pr_debug("ADM_CMD_MATRIX_MAP_ROUTINGS\n");
				atomic_set(&this_adm.copp_stat[index], 1);
				wake_up(&this_adm.wait);
				break;
			default:
				pr_err("%s: Unknown Cmd: 0x%x\n", __func__,
								payload[0]);
				break;
			}
			return 0;
		}

		switch (data->opcode) {
		case ADM_CMDRSP_COPP_OPEN: {
			struct adm_copp_open_respond *open = data->payload;
			if (open->copp_id == INVALID_COPP_ID) {
				pr_err("%s: invalid coppid rxed %d\n",
					__func__, open->copp_id);
				atomic_set(&this_adm.copp_stat[index], 1);
				wake_up(&this_adm.wait);
				break;
			}
			atomic_set(&this_adm.copp_id[index], open->copp_id);
			atomic_set(&this_adm.copp_stat[index], 1);
			pr_debug("%s: coppid rxed=%d\n", __func__,
							open->copp_id);
			wake_up(&this_adm.wait);
			}
			break;
#ifdef CONFIG_MSM8X60_RTAC
		case ADM_CMDRSP_GET_PARAMS:
			pr_debug("ADM_CMDRSP_GET_PARAMS\n");
			rtac_make_adm_callback(payload,
				data->payload_size);
			break;
#endif
		default:
			pr_err("%s: Unknown cmd:0x%x\n", __func__,
							data->opcode);
			break;
		}
	}
	return 0;
}

void send_cal(int port_id, struct acdb_cal_block *aud_cal)
{
	s32				result;
	struct adm_set_params_command	adm_params;
	int index = afe_get_port_index(port_id);

	pr_debug("%s: Port id %d, index %d\n", __func__, port_id, index);

	if (!aud_cal || aud_cal->cal_size == 0) {
		pr_err("%s: No calibration data to send!\n", __func__);
		goto done;
	}

	adm_params.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(20), APR_PKT_VER);
	adm_params.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(adm_params));
	adm_params.hdr.src_svc = APR_SVC_ADM;
	adm_params.hdr.src_domain = APR_DOMAIN_APPS;
	adm_params.hdr.src_port = port_id;
	adm_params.hdr.dest_svc = APR_SVC_ADM;
	adm_params.hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_params.hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	adm_params.hdr.token = port_id;
	adm_params.hdr.opcode = ADM_CMD_SET_PARAMS;
	adm_params.payload = aud_cal->cal_paddr;
	adm_params.payload_size = aud_cal->cal_size;

	atomic_set(&this_adm.copp_stat[index], 0);
	pr_debug("%s: Sending SET_PARAMS payload = 0x%x, size = %d\n",
		__func__, adm_params.payload, adm_params.payload_size);
	result = apr_send_pkt(this_adm.apr, (uint32_t *)&adm_params);
	if (result < 0) {
		pr_err("%s: Set params failed port = %d payload = 0x%x\n",
			__func__, port_id, aud_cal->cal_paddr);
		goto done;
	}
	/* Wait for the callback */
	result = wait_event_timeout(this_adm.wait,
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!result)
		pr_err("%s: Set params timed out port = %d, payload = 0x%x\n",
			__func__, port_id, aud_cal->cal_paddr);
done:
	return;
}

void send_adm_cal(int port_id, int path)
{
	s32			acdb_path;
	struct acdb_cal_block	aud_cal;

	pr_debug("%s\n", __func__);

	/* Maps audio_dev_ctrl path definition to ACDB definition */
	acdb_path = path - 1;
	if ((acdb_path >= NUM_AUDPROC_BUFFERS) ||
		(acdb_path < 0)) {
		pr_err("%s: Path is not RX or TX, path = %d\n",
			__func__, path);
		goto done;
	}

	pr_debug("%s: Sending audproc cal\n", __func__);
	get_audproc_cal(acdb_path, &aud_cal);
	send_cal(port_id, &aud_cal);

	pr_debug("%s: Sending audvol cal\n", __func__);
	get_audvol_cal(acdb_path, &aud_cal);
	send_cal(port_id, &aud_cal);
done:
	return;
}

/* This function issues routing command of ASM stream
 * to ADM mixer associated with a particular AFE port
 */
int adm_cmd_map(int port_id, int session_id)
{
	struct adm_routings_command route;
	int ret = 0;
	int index = afe_get_port_index(port_id);

	pr_debug("%s: port %x session %x\n", __func__, port_id, session_id);

	if (!atomic_read(&this_adm.copp_cnt[index]))
		return 0;

	route.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	route.hdr.pkt_size = sizeof(route);
	route.hdr.src_svc = 0;
	route.hdr.src_domain = APR_DOMAIN_APPS;
	route.hdr.src_port = port_id;
	route.hdr.dest_svc = APR_SVC_ADM;
	route.hdr.dest_domain = APR_DOMAIN_ADSP;
	route.hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	route.hdr.token = port_id;
	route.hdr.opcode = ADM_CMD_MATRIX_MAP_ROUTINGS;
	route.num_sessions = 1;
	route.session[0].id = session_id;
	route.session[0].num_copps = 1;
	route.session[0].copp_id[0] =
			atomic_read(&this_adm.copp_id[index]);

	/* This rule can change */
	if ((port_id & 0x1))
		route.path = AUDIO_TX;
	else
		route.path = AUDIO_RX;

	atomic_set(&this_adm.copp_stat[index], 0);

	ret = apr_send_pkt(this_adm.apr, (uint32_t *)&route);
	if (ret < 0) {
		pr_err("%s: ADM routing for port %d failed\n",
					__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = wait_event_timeout(this_adm.wait,
				atomic_read(&this_adm.copp_stat[index]),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: ADM cmd Route failed for port %d\n",
					__func__, port_id);
		ret = -EINVAL;
	}

fail_cmd:
	return ret;
}

/* This function establish routing of ASM stream to a particular
 * ADM mixer that is routed to a particular hardware port
 * session id must be in range of 0 ~ 31.
 */
int adm_route_session(int port_id, uint session_id, int set)
{
	int rc = 0;
	int index;

	pr_debug("%s: port %x session %x set %x\n", __func__,
		port_id, session_id, set);

	index = afe_get_port_index(port_id);

	if (index >= AFE_MAX_PORTS) {
		pr_err("%s port idi[%d] out of limit[%d]\n", __func__,
						port_id, AFE_MAX_PORTS);
		return -ENODEV;
	}

	if (set) {
		set_bit(session_id, &this_adm.sessions[index]);
		rc = adm_cmd_map(port_id, session_id); /* not thread safe */
	} else /* Not sure how to deroute yet */
		clear_bit(session_id, &this_adm.sessions[index]);

	return rc;
}

/* This function instantiates a mixer in QDSP6 audio path for
 * given audio hardware port. Topology should be made part
 * of audio calibration
 */
int adm_open_mixer(int port_id, int path, int rate,
	int channel_mode, int topology) {
	struct adm_copp_open_command open;
	int ret = 0;
	u32 i;
	int index;

	pr_debug("%s: port %d path:%d rate:%d mode:%d\n", __func__,
				port_id, path, rate, channel_mode);

	if (afe_validate_port(port_id) < 0) {
		pr_err("%s port idi[%d] is invalid\n", __func__, port_id);
		return -ENODEV;
	}

	index = afe_get_port_index(port_id);
	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
	}

	if (atomic_read(&this_adm.copp_cnt[index]) == 0) {

		open.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		open.hdr.pkt_size = sizeof(open);
		open.hdr.src_svc = APR_SVC_ADM;
		open.hdr.src_domain = APR_DOMAIN_APPS;
		open.hdr.src_port = port_id;
		open.hdr.dest_svc = APR_SVC_ADM;
		open.hdr.dest_domain = APR_DOMAIN_ADSP;
		open.hdr.dest_port = port_id;
		open.hdr.token = port_id;
		open.hdr.opcode = ADM_CMD_COPP_OPEN;

		open.mode = path;
		open.endpoint_id1 = port_id;
		open.endpoint_id2 = 0xFFFF;

		/* convert path to acdb path */
		if (path == PLAYBACK)
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

		open.channel_config = channel_mode & 0x00FF;
		open.rate  = rate;

		pr_debug("%s: channel_config=%d port_id=%d rate=%d\
			topology_id=0x%X\n", __func__, open.channel_config,\
			open.endpoint_id1, open.rate,\
			open.topology_id);

		atomic_set(&this_adm.copp_stat[index], 0);

		ret = apr_send_pkt(this_adm.apr, (uint32_t *)&open);
		if (ret < 0) {
			pr_err("%s:ADM enable for port %d failed\n",
						__func__, port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}
		/* Wait for the callback with copp id */
		ret = wait_event_timeout(this_adm.wait,
			atomic_read(&this_adm.copp_stat[index]),
			msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s ADM open failed for port %d\n", __func__,
								port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}
	}
	atomic_inc(&this_adm.copp_cnt[index]);

	/* Set up routing for cached session */
	for (i = find_first_bit(&this_adm.sessions[index], ASM_MAX_SESSION);
	     i < ASM_MAX_SESSION; i = find_next_bit(&this_adm.sessions[index],
	     ASM_MAX_SESSION, i + 1))
		adm_cmd_map(port_id, i); /* Not thread safe */

fail_cmd:
	return ret;
}

int adm_open(int port_id, int path, int rate, int channel_mode, int topology)
{
	struct adm_copp_open_command	open;
	int ret = 0;
	int index;

	pr_debug("%s: port %d path:%d rate:%d mode:%d\n", __func__,
				port_id, path, rate, channel_mode);

	if (afe_validate_port(port_id) < 0) {
		pr_err("%s port idi[%d] is invalid\n", __func__, port_id);
		return -ENODEV;
	}

	index = afe_get_port_index(port_id);
	pr_debug("%s: Port ID %d, index %d\n", __func__, port_id, index);

	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
#ifdef CONFIG_MSM8X60_RTAC
		rtac_set_adm_handle(this_adm.apr);
#endif
	}


	/* Create a COPP if port id are not enabled */
	if (atomic_read(&this_adm.copp_cnt[index]) == 0) {

		open.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		open.hdr.pkt_size = sizeof(open);
		open.hdr.src_svc = APR_SVC_ADM;
		open.hdr.src_domain = APR_DOMAIN_APPS;
		open.hdr.src_port = port_id;
		open.hdr.dest_svc = APR_SVC_ADM;
		open.hdr.dest_domain = APR_DOMAIN_ADSP;
		open.hdr.dest_port = port_id;
		open.hdr.token = port_id;
		open.hdr.opcode = ADM_CMD_COPP_OPEN;

		open.mode = path;
		open.endpoint_id1 = port_id;
		open.endpoint_id2 = 0xFFFF;

		/* convert path to acdb path */
		if (path == PLAYBACK)
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

		open.channel_config = channel_mode & 0x00FF;
		open.rate  = rate;

		pr_debug("%s: channel_config=%d port_id=%d rate=%d\
			topology_id=0x%X\n", __func__, open.channel_config,\
			open.endpoint_id1, open.rate,\
			open.topology_id);

		atomic_set(&this_adm.copp_stat[index], 0);

		ret = apr_send_pkt(this_adm.apr, (uint32_t *)&open);
		if (ret < 0) {
			pr_err("%s:ADM enable for port %d failed\n",
						__func__, port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}
		/* Wait for the callback with copp id */
		ret = wait_event_timeout(this_adm.wait,
			atomic_read(&this_adm.copp_stat[index]),
			msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s ADM open failed for port %d\n", __func__,
								port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}
	}
	atomic_inc(&this_adm.copp_cnt[index]);
	return 0;

fail_cmd:

	return ret;
}

int adm_matrix_map(int session_id, int path, int num_copps,
			unsigned int *port_id, int copp_id)
{
	struct adm_routings_command	route;
	int ret = 0, i = 0;
	/* Assumes port_ids have already been validated during adm_open */
	int index = afe_get_port_index(copp_id);

	pr_debug("%s: session 0x%x path:%d num_copps:%d port_id[0]:%d\n",
		 __func__, session_id, path, num_copps, port_id[0]);

	route.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	route.hdr.pkt_size = sizeof(route);
	route.hdr.src_svc = 0;
	route.hdr.src_domain = APR_DOMAIN_APPS;
	route.hdr.src_port = copp_id;
	route.hdr.dest_svc = APR_SVC_ADM;
	route.hdr.dest_domain = APR_DOMAIN_ADSP;
	route.hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	route.hdr.token = copp_id;
	route.hdr.opcode = ADM_CMD_MATRIX_MAP_ROUTINGS;
	route.num_sessions = 1;
	route.session[0].id = session_id;
	route.session[0].num_copps = num_copps;

	for (i = 0; i < num_copps; i++) {
		int tmp;
		tmp = afe_get_port_index(port_id[i]);

		pr_debug("%s: port_id[%d]: %d, index: %d\n", __func__, i,
			 port_id[i], tmp);

		route.session[0].copp_id[i] =
					atomic_read(&this_adm.copp_id[tmp]);
	}
	if (num_copps % 2)
		route.session[0].copp_id[i] = 0;

	switch (path) {
	case 0x1:
		route.path = AUDIO_RX;
		break;
	case 0x2:
	case 0x3:
		route.path = AUDIO_TX;
		break;
	default:
		pr_err("%s: Wrong path set[%d]\n", __func__, path);
		break;
	}
	atomic_set(&this_adm.copp_stat[index], 0);

	ret = apr_send_pkt(this_adm.apr, (uint32_t *)&route);
	if (ret < 0) {
		pr_err("%s: ADM routing for port %d failed\n",
					__func__, port_id[0]);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = wait_event_timeout(this_adm.wait,
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

#ifdef CONFIG_MSM8X60_RTAC
	for (i = 0; i < num_copps; i++)
		rtac_add_adm_device(port_id[i], session_id);
#endif
	return 0;

fail_cmd:

	return ret;
}

int adm_memory_map_regions(uint32_t *buf_add, uint32_t mempool_id,
				uint32_t *bufsz, uint32_t bufcnt)
{
	struct  adm_cmd_memory_map_regions *mmap_regions = NULL;
	struct  adm_memory_map_regions *mregions = NULL;
	void    *mmap_region_cmd = NULL;
	void    *payload = NULL;
	int     ret = 0;
	int     i = 0;
	int     cmd_size = 0;

	pr_info("%s\n", __func__);
	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
#ifdef CONFIG_MSM8X60_RTAC
		rtac_set_adm_handle(this_adm.apr);
#endif
	}

	cmd_size = sizeof(struct adm_cmd_memory_map_regions)
			+ sizeof(struct adm_memory_map_regions) * bufcnt;

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (!mmap_region_cmd) {
		pr_err("%s: allocate mmap_region_cmd failed\n", __func__);
		return -ENOMEM;
	}
	mmap_regions = (struct adm_cmd_memory_map_regions *)mmap_region_cmd;
	mmap_regions->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
								APR_PKT_VER);
	mmap_regions->hdr.pkt_size = cmd_size;
	mmap_regions->hdr.src_port = 0;
	mmap_regions->hdr.dest_port = 0;
	mmap_regions->hdr.token = 0;
	mmap_regions->hdr.opcode = ADM_CMD_MEMORY_MAP_REGIONS;
	mmap_regions->mempool_id = mempool_id & 0x00ff;
	mmap_regions->nregions = bufcnt & 0x00ff;
	pr_debug("%s: map_regions->nregions = %d\n", __func__,
				mmap_regions->nregions);
	payload = ((u8 *) mmap_region_cmd +
				sizeof(struct adm_cmd_memory_map_regions));
	mregions = (struct adm_memory_map_regions *)payload;

	for (i = 0; i < bufcnt; i++) {
		mregions->phys = buf_add[i];
		mregions->buf_size = bufsz[i];
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

	ret = wait_event_timeout(this_adm.wait,
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

int adm_memory_unmap_regions(uint32_t *buf_add, uint32_t *bufsz,
						uint32_t bufcnt)
{
	struct  adm_cmd_memory_unmap_regions *unmap_regions = NULL;
	struct  adm_memory_unmap_regions *mregions = NULL;
	void    *unmap_region_cmd = NULL;
	void    *payload = NULL;
	int     ret = 0;
	int     i = 0;
	int     cmd_size = 0;

	pr_info("%s\n", __func__);

	if (this_adm.apr == NULL) {
		pr_err("%s APR handle NULL\n", __func__);
		return -EINVAL;
	}

	cmd_size = sizeof(struct adm_cmd_memory_unmap_regions)
			+ sizeof(struct adm_memory_unmap_regions) * bufcnt;

	unmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (!unmap_region_cmd) {
		pr_err("%s: allocate unmap_region_cmd failed\n", __func__);
		return -ENOMEM;
	}
	unmap_regions = (struct adm_cmd_memory_unmap_regions *)
						unmap_region_cmd;
	unmap_regions->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
							APR_PKT_VER);
	unmap_regions->hdr.pkt_size = cmd_size;
	unmap_regions->hdr.src_port = 0;
	unmap_regions->hdr.dest_port = 0;
	unmap_regions->hdr.token = 0;
	unmap_regions->hdr.opcode = ADM_CMD_MEMORY_UNMAP_REGIONS;
	unmap_regions->nregions = bufcnt & 0x00ff;
	unmap_regions->reserved = 0;
	pr_debug("%s: unmap_regions->nregions = %d\n", __func__,
				unmap_regions->nregions);
	payload = ((u8 *) unmap_region_cmd +
			sizeof(struct adm_cmd_memory_unmap_regions));
	mregions = (struct adm_memory_unmap_regions *)payload;

	for (i = 0; i < bufcnt; i++) {
		mregions->phys = buf_add[i];
		++mregions;
	}
	atomic_set(&this_adm.copp_stat[0], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *) unmap_region_cmd);
	if (ret < 0) {
		pr_err("%s: mmap_regions op[0x%x]rc[%d]\n", __func__,
				unmap_regions->hdr.opcode, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_adm.wait,
			atomic_read(&this_adm.copp_stat[0]), 5 * HZ);
	if (!ret) {
		pr_err("%s: timeout. waited for memory_unmap\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
fail_cmd:
	kfree(unmap_region_cmd);
	return ret;
}

#ifdef CONFIG_MSM8X60_RTAC
int adm_get_copp_id(int port_id)
{
	pr_debug("%s\n", __func__);

	if (port_id < 0) {
		pr_err("%s: invalid port_id = %d\n", __func__, port_id);
		return -EINVAL;
	}

	return atomic_read(&this_adm.copp_id[port_id]);
}
#endif

int adm_close(int port_id)
{
	struct apr_hdr close;

	int ret = 0;
	int index = afe_get_port_index(port_id);

	pr_info("%s port_id=%d index %d\n", __func__, port_id, index);

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
		close.opcode = ADM_CMD_COPP_CLOSE;

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

		ret = wait_event_timeout(this_adm.wait,
				atomic_read(&this_adm.copp_stat[index]),
				msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: ADM cmd Route failed for port %d\n",
						__func__, port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}

#ifdef CONFIG_MSM8X60_RTAC
		rtac_remove_adm_device(port_id);
#endif
	}

fail_cmd:
	return ret;
}

static int __init adm_init(void)
{
	int i = 0;
	init_waitqueue_head(&this_adm.wait);
	this_adm.apr = NULL;

	for (i = 0; i < AFE_MAX_PORTS; i++) {
		atomic_set(&this_adm.copp_id[i], RESET_COPP_ID);
		atomic_set(&this_adm.copp_cnt[i], 0);
		atomic_set(&this_adm.copp_stat[i], 0);
	}
	return 0;
}

device_initcall(adm_init);
