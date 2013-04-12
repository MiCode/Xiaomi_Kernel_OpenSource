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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <mach/msm_smd.h>
#include <mach/qdsp6v2/apr.h>
#include <mach/qdsp6v2/q6core.h>

#define TIMEOUT_MS 1000

static struct apr_svc *apr_handle_q;
static struct apr_svc *apr_handle_m;
static struct apr_svc *core_handle_q;

static int32_t query_adsp_ver;
static wait_queue_head_t adsp_version_wait;
static uint32_t adsp_version;

static wait_queue_head_t bus_bw_req_wait;
static u32 bus_bw_resp_received;

static struct dentry *dentry;
static char l_buf[4096];

static int32_t aprv2_core_fn_q(struct apr_client_data *data, void *priv)
{
	struct adsp_get_version *payload;
	uint32_t *payload1;
	struct adsp_service_info *svc_info;
	int i;

	pr_info("core msg: payload len = %u, apr resp opcode = 0x%X\n",
		data->payload_size, data->opcode);

	switch (data->opcode) {

	case APR_BASIC_RSP_RESULT:{

		if (data->payload_size == 0) {
			pr_err("%s: APR_BASIC_RSP_RESULT No Payload ",
					__func__);
			return 0;
		}

		payload1 = data->payload;

		switch (payload1[0]) {

		case ADSP_CMD_SET_POWER_COLLAPSE_STATE:
			pr_info("Cmd = ADSP_CMD_SET_POWER_COLLAPSE_STATE status[0x%x]\n",
								payload1[1]);
			break;
		case ADSP_CMD_REMOTE_BUS_BW_REQUEST:
			pr_info("%s: cmd = ADSP_CMD_REMOTE_BUS_BW_REQUEST status = 0x%x\n",
							__func__, payload1[1]);

			bus_bw_resp_received = 1;
			wake_up(&bus_bw_req_wait);
			break;
		case ADSP_CMD_SET_DTS_MODEL_ID:
			pr_debug("ADSP_CMD_SET_DTS_MODEL_ID status[0x%x]\n",
					payload1[1]);
			break;
		default:
			pr_err("Invalid cmd rsp[0x%x][0x%x]\n",
					payload1[0], payload1[1]);
			break;
		}
		break;
	}
	case ADSP_GET_VERSION_RSP:{
		if (data->payload_size) {
			payload = data->payload;
			if (query_adsp_ver == 1) {
				query_adsp_ver = 0;
				adsp_version  = payload->build_id;
				wake_up(&adsp_version_wait);
			}
			svc_info = (struct adsp_service_info *)
			((char *)payload + sizeof(struct adsp_get_version));
			pr_info("----------------------------------------\n");
			pr_info("Build id          = %x\n", payload->build_id);
			pr_info("Number of services= %x\n", payload->svc_cnt);
			pr_info("----------------------------------------\n");
			for (i = 0; i < payload->svc_cnt; i++) {
				pr_info("svc-id[%d]\tver[%x.%x]\n",
					svc_info[i].svc_id,
					(svc_info[i].svc_ver & 0xFFFF0000)
					>> 16,
					(svc_info[i].svc_ver & 0xFFFF));
			}
			pr_info("-----------------------------------------\n");
		} else
			pr_info("zero payload for ADSP_GET_VERSION_RSP\n");
		break;
	}
	case RESET_EVENTS:{
		pr_debug("Reset event received in Core service");
		apr_reset(core_handle_q);
		core_handle_q = NULL;
		break;
	}

	default:
		pr_err("Message id from adsp core svc: %d\n", data->opcode);
		break;
	}

	return 0;
}

static int32_t aprv2_debug_fn_q(struct apr_client_data *data, void *priv)
{
	pr_debug("Q6_Payload Length = %d\n", data->payload_size);
	if (memcmp(data->payload, l_buf + 20, data->payload_size))
		pr_info("FAIL: %d\n", data->payload_size);
	else
		pr_info("SUCCESS: %d\n", data->payload_size);
	return 0;
}

static int32_t aprv2_debug_fn_m(struct apr_client_data *data, void *priv)
{
	pr_info("M_Payload Length = %d\n", data->payload_size);
	return 0;
}

static ssize_t apr_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	pr_debug("apr debugfs opened\n");
	return 0;
}

void core_open(void)
{
	if (core_handle_q == NULL) {
		core_handle_q = apr_register("ADSP", "CORE",
					aprv2_core_fn_q, 0xFFFFFFFF, NULL);
	}
	pr_debug("Open_q %p\n", core_handle_q);
	if (core_handle_q == NULL)
		pr_err("%s: Unable to register CORE\n", __func__);
}

int core_req_bus_bandwith(u16 bus_id, u32 ab_bps, u32 ib_bps)
{
	struct adsp_cmd_remote_bus_bw_request bus_bw_req;
	int ret;

	pr_debug("%s: bus_id %u ab_bps %u ib_bps %u\n",
			__func__, bus_id, ab_bps, ib_bps);

	core_open();
	if (core_handle_q == NULL) {
		pr_info("%s: apr registration for CORE failed\n", __func__);
		return -ENODEV;
	}

	bus_bw_req.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	bus_bw_req.hdr.pkt_size = sizeof(struct adsp_cmd_remote_bus_bw_request);

	bus_bw_req.hdr.src_port = 0;
	bus_bw_req.hdr.dest_port = 0;
	bus_bw_req.hdr.token = 0;
	bus_bw_req.hdr.opcode = ADSP_CMD_REMOTE_BUS_BW_REQUEST;

	bus_bw_req.bus_identifier = bus_id;
	bus_bw_req.reserved = 0;
	bus_bw_req.ab_bps = ab_bps;
	bus_bw_req.ib_bps = ib_bps;

	bus_bw_resp_received = 0;
	ret = apr_send_pkt(core_handle_q, (uint32_t *) &bus_bw_req);
	if (ret < 0) {
		pr_err("%s: CORE bus bw request failed\n", __func__);
		goto fail_cmd;
	}

	ret = wait_event_timeout(bus_bw_req_wait, (bus_bw_resp_received == 1),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -ETIME;
		goto fail_cmd;
	}

	return 0;

fail_cmd:
	return ret;
}

uint32_t core_get_adsp_version(void)
{
	struct apr_hdr *hdr;
	int32_t rc = 0, ret = 0;
	core_open();
	if (core_handle_q) {
		hdr = (struct apr_hdr *)l_buf;
		hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_EVENT,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		hdr->pkt_size = APR_PKT_SIZE(APR_HDR_SIZE, 0);
		hdr->src_port = 0;
		hdr->dest_port = 0;
		hdr->token = 0;
		hdr->opcode = ADSP_GET_VERSION;

		apr_send_pkt(core_handle_q, (uint32_t *)l_buf);
		query_adsp_ver = 1;
		pr_info("Write_q\n");
		ret = wait_event_timeout(adsp_version_wait,
					(query_adsp_ver == 0),
					msecs_to_jiffies(TIMEOUT_MS));
		rc = adsp_version;
		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);
			rc = -ENODEV;
		}
	} else
		pr_info("apr registration failed\n");
	return rc;
}
EXPORT_SYMBOL(core_get_adsp_version);

static ssize_t apr_debug_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	int len;
	static int t_len;

	len = count > 63 ? 63 : count;
	if (copy_from_user(l_buf + 20 , buf, len)) {
		pr_info("Unable to copy data from user space\n");
		return -EFAULT;
	}
	l_buf[len + 20] = 0;
	if (l_buf[len + 20 - 1] == '\n') {
		l_buf[len + 20 - 1] = 0;
		len--;
	}
	if (!strncmp(l_buf + 20, "open_q", 64)) {
		apr_handle_q = apr_register("ADSP", "TEST", aprv2_debug_fn_q,
							0xFFFFFFFF, NULL);
		pr_info("Open_q %p\n", apr_handle_q);
	} else if (!strncmp(l_buf + 20, "open_m", 64)) {
		apr_handle_m = apr_register("MODEM", "TEST", aprv2_debug_fn_m,
							0xFFFFFFFF, NULL);
		pr_info("Open_m %p\n", apr_handle_m);
	} else if (!strncmp(l_buf + 20, "write_q", 64)) {
		struct apr_hdr *hdr;

		t_len++;
		t_len = t_len % 450;
		if (!t_len % 99)
			msleep(2000);
		hdr = (struct apr_hdr *)l_buf;
		hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_EVENT,
					APR_HDR_LEN(20), APR_PKT_VER);
		hdr->pkt_size = APR_PKT_SIZE(20, t_len);
		hdr->src_port = 0;
		hdr->dest_port = 0;
		hdr->token = 0;
		hdr->opcode = 0x12345678;
		memset(l_buf + 20, 9, 4060);

		apr_send_pkt(apr_handle_q, (uint32_t *)l_buf);
		pr_debug("Write_q\n");
	} else if (!strncmp(l_buf + 20, "write_m", 64)) {
		struct apr_hdr *hdr;

		hdr = (struct apr_hdr *)l_buf;
		hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_EVENT,
					APR_HDR_LEN(20), APR_PKT_VER);
		hdr->pkt_size = APR_PKT_SIZE(20, 8);
		hdr->src_port = 0;
		hdr->dest_port = 0;
		hdr->token = 0;
		hdr->opcode = 0x12345678;
		memset(l_buf + 30, 9, 4060);

		apr_send_pkt(apr_handle_m, (uint32_t *)l_buf);
		pr_info("Write_m\n");
	} else if (!strncmp(l_buf + 20, "write_q4", 64)) {
		struct apr_hdr *hdr;

		hdr = (struct apr_hdr *)l_buf;
		hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_EVENT,
					APR_HDR_LEN(20), APR_PKT_VER);
		hdr->pkt_size = APR_PKT_SIZE(20, 4076);
		hdr->src_port = 0;
		hdr->dest_port = 0;
		hdr->token = 0;
		hdr->opcode = 0x12345678;
		memset(l_buf + 30, 9, 4060);

		apr_send_pkt(apr_handle_q, (uint32_t *)l_buf);
		pr_info("Write_q\n");
	} else if (!strncmp(l_buf + 20, "write_m4", 64)) {
		struct apr_hdr *hdr;

		hdr = (struct apr_hdr *)l_buf;
		hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_EVENT,
					APR_HDR_LEN(20), APR_PKT_VER);
		hdr->pkt_size = APR_PKT_SIZE(20, 4076);
		hdr->src_port = 0;
		hdr->dest_port = 0;
		hdr->token = 0;
		hdr->opcode = 0x12345678;
		memset(l_buf + 30, 9, 4060);

		apr_send_pkt(apr_handle_m, (uint32_t *)l_buf);
		pr_info("Write_m\n");
	} else if (!strncmp(l_buf + 20, "close", 64)) {
		if (apr_handle_q)
			apr_deregister(apr_handle_q);
	} else if (!strncmp(l_buf + 20, "loaded", 64)) {
		apr_set_q6_state(APR_SUBSYS_LOADED);
	} else if (!strncmp(l_buf + 20, "boom", 64)) {
		q6audio_dsp_not_responding();
	} else if (!strncmp(l_buf + 20, "dsp_ver", 64)) {
			core_get_adsp_version();
	} else if (!strncmp(l_buf + 20, "en_pwr_col", 64)) {
		struct adsp_power_collapse pc;

		core_open();
		if (core_handle_q) {
			pc.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_EVENT,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
			pc.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
						sizeof(uint32_t));
			pc.hdr.src_port = 0;
			pc.hdr.dest_port = 0;
			pc.hdr.token = 0;
			pc.hdr.opcode = ADSP_CMD_SET_POWER_COLLAPSE_STATE;
			pc.power_collapse = 0x00000000;
			apr_send_pkt(core_handle_q, (uint32_t *)&pc);
			pr_info("Write_q :enable power collapse\n");
		}
	} else if (!strncmp(l_buf + 20, "dis_pwr_col", 64)) {
		struct adsp_power_collapse pc;

		core_open();
		if (core_handle_q) {
			pc.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_EVENT,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
			pc.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
							sizeof(uint32_t));
			pc.hdr.src_port = 0;
			pc.hdr.dest_port = 0;
			pc.hdr.token = 0;
			pc.hdr.opcode = ADSP_CMD_SET_POWER_COLLAPSE_STATE;
			pc.power_collapse = 0x00000001;
			apr_send_pkt(core_handle_q, (uint32_t *)&pc);
			pr_info("Write_q:disable power collapse\n");
		}
	} else
		pr_info("Unknown Command\n");

	return count;
}

uint32_t core_set_dts_model_id(uint32_t id_size, uint8_t *id)
{
	struct adsp_dts_modelid payload;
	int rc = 0;
	pr_debug("core_set_dts_model_id(): Enter\n");
	core_open();
	if (core_handle_q) {
		payload.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_EVENT,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		payload.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(uint32_t)+id_size);
		payload.hdr.src_port = 0;
		payload.hdr.dest_port = 0;
		payload.hdr.token = 0;
		payload.hdr.opcode = ADSP_CMD_SET_DTS_MODEL_ID;
		payload.model_ID_size = id_size;
		memcpy(payload.model_ID, id, id_size+1);
		pr_debug("Send DTS sec opcode=%x modelID = %s, size=%d\n",
			payload.hdr.opcode, (char *)payload.model_ID,
			payload.model_ID_size);
		rc = apr_send_pkt(core_handle_q, (uint32_t *)&payload);
		if (rc < 0)
			pr_err("%s: SET_DTS_DTS_MODEL_ID failed op[0x%x]rc[%d]\n",
				__func__, payload.hdr.opcode, rc);
	}
	pr_debug("core_set_dts_model_id(): Exit\n");
	return rc;
}

static const struct file_operations apr_debug_fops = {
	.write = apr_debug_write,
	.open = apr_debug_open,
};

static int __init core_init(void)
{
	init_waitqueue_head(&bus_bw_req_wait);
	bus_bw_resp_received = 0;

	query_adsp_ver = 0;
	init_waitqueue_head(&adsp_version_wait);
	adsp_version = 0;

	core_handle_q = NULL;

#ifdef CONFIG_DEBUG_FS
	dentry = debugfs_create_file("apr", S_IFREG | S_IRUGO | S_IWUSR
		| S_IWGRP, NULL, (void *) NULL, &apr_debug_fops);
#endif /* CONFIG_DEBUG_FS */

	return 0;
}

device_initcall(core_init);
