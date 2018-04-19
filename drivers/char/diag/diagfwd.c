/* Copyright (c) 2008-2018, The Linux Foundation. All rights reserved.
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include <linux/diagchar.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/kmemleak.h>
#ifdef CONFIG_DIAG_OVER_USB
#include <linux/usb/usbdiag.h>
#endif
#include <soc/qcom/socinfo.h>
#include <soc/qcom/restart.h>
#include "diagmem.h"
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_peripheral.h"
#include "diagfwd_cntl.h"
#include "diagchar_hdlc.h"
#include "diag_dci.h"
#include "diag_masks.h"
#include "diag_usb.h"
#include "diag_mux.h"

#define STM_CMD_VERSION_OFFSET	4
#define STM_CMD_MASK_OFFSET	5
#define STM_CMD_DATA_OFFSET	6
#define STM_CMD_NUM_BYTES	7

#define STM_RSP_SUPPORTED_INDEX		7
#define STM_RSP_STATUS_INDEX		8
#define STM_RSP_NUM_BYTES		9

static int timestamp_switch;
module_param(timestamp_switch, int, 0644);

int wrap_enabled;
uint16_t wrap_count;
static struct diag_hdlc_decode_type *hdlc_decode;

#define DIAG_NUM_COMMON_CMD	1
static uint8_t common_cmds[DIAG_NUM_COMMON_CMD] = {
	DIAG_CMD_LOG_ON_DMND
};

static uint8_t hdlc_timer_in_progress;

/* Determine if this device uses a device tree */
#ifdef CONFIG_OF
static int has_device_tree(void)
{
	struct device_node *node;

	node = of_find_node_by_path("/");
	if (node) {
		of_node_put(node);
		return 1;
	}
	return 0;
}
#else
static int has_device_tree(void)
{
	return 0;
}
#endif

int chk_config_get_id(void)
{
	switch (socinfo_get_msm_cpu()) {
	case MSM_CPU_8X60:
		return APQ8060_TOOLS_ID;
	case MSM_CPU_8960:
	case MSM_CPU_8960AB:
		return AO8960_TOOLS_ID;
	case MSM_CPU_8064:
	case MSM_CPU_8064AB:
	case MSM_CPU_8064AA:
		return APQ8064_TOOLS_ID;
	case MSM_CPU_8930:
	case MSM_CPU_8930AA:
	case MSM_CPU_8930AB:
		return MSM8930_TOOLS_ID;
	case MSM_CPU_8974:
		return MSM8974_TOOLS_ID;
	case MSM_CPU_8625:
		return MSM8625_TOOLS_ID;
	case MSM_CPU_8084:
		return APQ8084_TOOLS_ID;
	case MSM_CPU_8916:
		return MSM8916_TOOLS_ID;
	case MSM_CPU_8939:
		return MSM8939_TOOLS_ID;
	case MSM_CPU_8994:
		return MSM8994_TOOLS_ID;
	case MSM_CPU_8226:
		return APQ8026_TOOLS_ID;
	case MSM_CPU_8909:
		return MSM8909_TOOLS_ID;
	case MSM_CPU_8992:
		return MSM8992_TOOLS_ID;
	case MSM_CPU_8996:
		return MSM_8996_TOOLS_ID;
	case MSM_CPU_8952:
		return MSM8952_TOOLS_ID;
	default:
		if (driver->use_device_tree) {
			if (machine_is_msm8974())
				return MSM8974_TOOLS_ID;
			else if (machine_is_apq8074())
				return APQ8074_TOOLS_ID;
			else
				return 0;
		} else {
			return 0;
		}
	}
}

/*
 * This will return TRUE for targets which support apps only mode and hence SSR.
 * This applies to 8960 and newer targets.
 */
int chk_apps_only(void)
{
	if (driver->use_device_tree)
		return 1;

	switch (socinfo_get_msm_cpu()) {
	case MSM_CPU_8960:
	case MSM_CPU_8960AB:
	case MSM_CPU_8064:
	case MSM_CPU_8064AB:
	case MSM_CPU_8064AA:
	case MSM_CPU_8930:
	case MSM_CPU_8930AA:
	case MSM_CPU_8930AB:
	case MSM_CPU_8627:
	case MSM_CPU_9615:
	case MSM_CPU_8974:
		return 1;
	default:
		return 0;
	}
}

/*
 * This will return TRUE for targets which support apps as master.
 * Thus, SW DLOAD and Mode Reset are supported on apps processor.
 * This applies to 8960 and newer targets.
 */
int chk_apps_master(void)
{
	if (driver->use_device_tree)
		return 1;
	else
		return 0;
}

int chk_polling_response(void)
{
	if (!(driver->polling_reg_flag) && chk_apps_master())
		/*
		 * If the apps processor is master and no other processor
		 * has registered to respond for polling
		 */
		return 1;
	else if (!(driver->diagfwd_cntl[PERIPHERAL_MODEM] &&
		   driver->diagfwd_cntl[PERIPHERAL_MODEM]->ch_open) &&
		 (driver->feature[PERIPHERAL_MODEM].rcvd_feature_mask))
		/*
		 * If the apps processor is not the master and the modem
		 * is not up or we did not receive the feature masks from Modem
		 */
		return 1;
	else
		return 0;
}

/*
 * This function should be called if you feel that the logging process may
 * need to be woken up. For instance, if the logging mode is MEMORY_DEVICE MODE
 * and while trying to read data from data channel there are no buffers
 * available to read the data into, then this function should be called to
 * determine if the logging process needs to be woken up.
 */
void chk_logging_wakeup(void)
{
	int i;
	int j;
	int pid = 0;

	for (j = 0; j < NUM_MD_SESSIONS; j++) {
		if (!driver->md_session_map[j])
			continue;
		pid = driver->md_session_map[j]->pid;

		/* Find the index of the logging process */
		for (i = 0; i < driver->num_clients; i++) {
			if (driver->client_map[i].pid != pid)
				continue;
			if (driver->data_ready[i] & USER_SPACE_DATA_TYPE)
				continue;
			/*
			 * At very high logging rates a race condition can
			 * occur where the buffers containing the data from
			 * a channel are all in use, but the data_ready flag
			 * is cleared. In this case, the buffers never have
			 * their data read/logged. Detect and remedy this
			 * situation.
			 */
			driver->data_ready[i] |= USER_SPACE_DATA_TYPE;
			pr_debug("diag: Force wakeup of logging process\n");
			wake_up_interruptible(&driver->wait_q);
			break;
		}
		/*
		 * Diag Memory Device is in normal. Check only for the first
		 * index as all the indices point to the same session
		 * structure.
		 */
		if ((driver->md_session_mask == DIAG_CON_ALL) && (j == 0))
			break;
	}
}

static void pack_rsp_and_send(unsigned char *buf, int len)
{
	int err;
	int retry_count = 0;
	uint32_t write_len = 0;
	unsigned long flags;
	unsigned char *rsp_ptr = driver->encoded_rsp_buf;
	struct diag_pkt_frame_t header;

	if (!rsp_ptr || !buf)
		return;

	if (len > DIAG_MAX_RSP_SIZE || len < 0) {
		pr_err("diag: In %s, invalid len %d, permissible len %d\n",
		       __func__, len, DIAG_MAX_RSP_SIZE);
		return;
	}

	/*
	 * Keep trying till we get the buffer back. It should probably
	 * take one or two iterations. When this loops till UINT_MAX, it
	 * means we did not get a write complete for the previous
	 * response.
	 */
	while (retry_count < UINT_MAX) {
		if (!driver->rsp_buf_busy)
			break;
		/*
		 * Wait for sometime and try again. The value 10000 was chosen
		 * empirically as an optimum value for USB to complete a write
		 */
		usleep_range(10000, 10100);
		retry_count++;

		/*
		 * There can be a race conditon that clears the data ready flag
		 * for responses. Make sure we don't miss previous wakeups for
		 * draining responses when we are in Memory Device Mode.
		 */
		if (driver->logging_mode == DIAG_MEMORY_DEVICE_MODE ||
				driver->logging_mode == DIAG_MULTI_MODE) {
			mutex_lock(&driver->md_session_lock);
			chk_logging_wakeup();
			mutex_unlock(&driver->md_session_lock);
		}
	}
	if (driver->rsp_buf_busy) {
		pr_err("diag: unable to get hold of response buffer\n");
		return;
	}

	driver->rsp_buf_busy = 1;
	header.start = CONTROL_CHAR;
	header.version = 1;
	header.length = len;
	memcpy(rsp_ptr, &header, sizeof(header));
	write_len += sizeof(header);
	memcpy(rsp_ptr + write_len, buf, len);
	write_len += len;
	*(uint8_t *)(rsp_ptr + write_len) = CONTROL_CHAR;
	write_len += sizeof(uint8_t);

	err = diag_mux_write(DIAG_LOCAL_PROC, rsp_ptr, write_len,
			     driver->rsp_buf_ctxt);
	if (err) {
		pr_err("diag: In %s, unable to write to mux, err: %d\n",
		       __func__, err);
		spin_lock_irqsave(&driver->rsp_buf_busy_lock, flags);
		driver->rsp_buf_busy = 0;
		spin_unlock_irqrestore(&driver->rsp_buf_busy_lock, flags);
	}
}

static void encode_rsp_and_send(unsigned char *buf, int len)
{
	struct diag_send_desc_type send = { NULL, NULL, DIAG_STATE_START, 0 };
	struct diag_hdlc_dest_type enc = { NULL, NULL, 0 };
	unsigned char *rsp_ptr = driver->encoded_rsp_buf;
	int err, retry_count = 0;
	unsigned long flags;

	if (!rsp_ptr || !buf)
		return;

	if (len > DIAG_MAX_RSP_SIZE || len < 0) {
		pr_err("diag: In %s, invalid len %d, permissible len %d\n",
		       __func__, len, DIAG_MAX_RSP_SIZE);
		return;
	}

	/*
	 * Keep trying till we get the buffer back. It should probably
	 * take one or two iterations. When this loops till UINT_MAX, it
	 * means we did not get a write complete for the previous
	 * response.
	 */
	while (retry_count < UINT_MAX) {
		if (!driver->rsp_buf_busy)
			break;
		/*
		 * Wait for sometime and try again. The value 10000 was chosen
		 * empirically as an optimum value for USB to complete a write
		 */
		usleep_range(10000, 10100);
		retry_count++;

		/*
		 * There can be a race conditon that clears the data ready flag
		 * for responses. Make sure we don't miss previous wakeups for
		 * draining responses when we are in Memory Device Mode.
		 */
		if (driver->logging_mode == DIAG_MEMORY_DEVICE_MODE ||
				driver->logging_mode == DIAG_MULTI_MODE) {
			mutex_lock(&driver->md_session_lock);
			chk_logging_wakeup();
			mutex_unlock(&driver->md_session_lock);
		}
	}

	if (driver->rsp_buf_busy) {
		pr_err("diag: unable to get hold of response buffer\n");
		return;
	}

	spin_lock_irqsave(&driver->rsp_buf_busy_lock, flags);
	driver->rsp_buf_busy = 1;
	spin_unlock_irqrestore(&driver->rsp_buf_busy_lock, flags);
	send.state = DIAG_STATE_START;
	send.pkt = buf;
	send.last = (void *)(buf + len - 1);
	send.terminate = 1;
	enc.dest = rsp_ptr;
	enc.dest_last = (void *)(rsp_ptr + DIAG_MAX_HDLC_BUF_SIZE - 1);
	diag_hdlc_encode(&send, &enc);
	driver->encoded_rsp_len = (int)(enc.dest - (void *)rsp_ptr);
	err = diag_mux_write(DIAG_LOCAL_PROC, rsp_ptr, driver->encoded_rsp_len,
			     driver->rsp_buf_ctxt);
	if (err) {
		pr_err("diag: In %s, Unable to write to device, err: %d\n",
			__func__, err);
		spin_lock_irqsave(&driver->rsp_buf_busy_lock, flags);
		driver->rsp_buf_busy = 0;
		spin_unlock_irqrestore(&driver->rsp_buf_busy_lock, flags);
	}
	memset(buf, '\0', DIAG_MAX_RSP_SIZE);
}

void diag_send_rsp(unsigned char *buf, int len)
{
	struct diag_md_session_t *session_info = NULL;
	uint8_t hdlc_disabled;

	mutex_lock(&driver->md_session_lock);
	session_info = diag_md_session_get_peripheral(APPS_DATA);
	if (session_info)
		hdlc_disabled = session_info->hdlc_disabled;
	else
		hdlc_disabled = driver->hdlc_disabled;
	mutex_unlock(&driver->md_session_lock);
	if (hdlc_disabled)
		pack_rsp_and_send(buf, len);
	else
		encode_rsp_and_send(buf, len);
}

void diag_update_pkt_buffer(unsigned char *buf, uint32_t len, int type)
{
	unsigned char *ptr = NULL;
	unsigned char *temp = buf;
	int *in_busy = NULL;
	uint32_t *length = NULL;
	uint32_t max_len = 0;

	if (!buf || len == 0) {
		pr_err("diag: In %s, Invalid ptr %pK and length %d\n",
		       __func__, buf, len);
		return;
	}

	switch (type) {
	case PKT_TYPE:
		ptr = driver->apps_req_buf;
		length = &driver->apps_req_buf_len;
		max_len = DIAG_MAX_REQ_SIZE;
		in_busy = &driver->in_busy_pktdata;
		break;
	case DCI_PKT_TYPE:
		ptr = driver->dci_pkt_buf;
		length = &driver->dci_pkt_length;
		max_len = DCI_BUF_SIZE;
		in_busy = &driver->in_busy_dcipktdata;
		break;
	default:
		pr_err("diag: Invalid type %d in %s\n", type, __func__);
		return;
	}

	mutex_lock(&driver->diagchar_mutex);
	if (CHK_OVERFLOW(ptr, ptr, ptr + max_len, len)) {
		memcpy(ptr, temp , len);
		*length = len;
		*in_busy = 1;
	} else {
		pr_alert("diag: In %s, no space for response packet, len: %d, type: %d\n",
			 __func__, len, type);
	}
	mutex_unlock(&driver->diagchar_mutex);
}

void diag_update_userspace_clients(unsigned int type)
{
	int i;

	mutex_lock(&driver->diagchar_mutex);
	for (i = 0; i < driver->num_clients; i++)
		if (driver->client_map[i].pid != 0)
			driver->data_ready[i] |= type;
	wake_up_interruptible(&driver->wait_q);
	mutex_unlock(&driver->diagchar_mutex);
}

void diag_update_md_clients(unsigned int type)
{
	int i, j;

	mutex_lock(&driver->diagchar_mutex);
	mutex_lock(&driver->md_session_lock);
	for (i = 0; i < NUM_MD_SESSIONS; i++) {
		if (driver->md_session_map[i] != NULL)
			for (j = 0; j < driver->num_clients; j++) {
				if (driver->client_map[j].pid != 0 &&
					driver->client_map[j].pid ==
					driver->md_session_map[i]->pid) {
					driver->data_ready[j] |= type;
					break;
				}
			}
	}
	mutex_unlock(&driver->md_session_lock);
	wake_up_interruptible(&driver->wait_q);
	mutex_unlock(&driver->diagchar_mutex);
}
void diag_update_sleeping_process(int process_id, int data_type)
{
	int i;

	mutex_lock(&driver->diagchar_mutex);
	for (i = 0; i < driver->num_clients; i++)
		if (driver->client_map[i].pid == process_id) {
			driver->data_ready[i] |= data_type;
			break;
		}
	wake_up_interruptible(&driver->wait_q);
	mutex_unlock(&driver->diagchar_mutex);
}

static int diag_send_data(struct diag_cmd_reg_t *entry, unsigned char *buf,
			  int len)
{
	if (!entry)
		return -EIO;

	if (entry->proc == APPS_DATA) {
		diag_update_pkt_buffer(buf, len, PKT_TYPE);
		diag_update_sleeping_process(entry->pid, PKT_TYPE);
		return 0;
	}

	return diagfwd_write(entry->proc, TYPE_CMD, buf, len);
}

void diag_process_stm_mask(uint8_t cmd, uint8_t data_mask, int data_type)
{
	int status = 0;
	if (data_type >= PERIPHERAL_MODEM && data_type <= PERIPHERAL_SENSORS) {
		if (driver->feature[data_type].stm_support) {
			status = diag_send_stm_state(data_type, cmd);
			if (status == 0)
				driver->stm_state[data_type] = cmd;
		}
		driver->stm_state_requested[data_type] = cmd;
	} else if (data_type == APPS_DATA) {
		driver->stm_state[data_type] = cmd;
		driver->stm_state_requested[data_type] = cmd;
	}
}

int diag_process_stm_cmd(unsigned char *buf, unsigned char *dest_buf)
{
	uint8_t version, mask, cmd;
	uint8_t rsp_supported = 0;
	uint8_t rsp_status = 0;
	int i;

	if (!buf || !dest_buf) {
		pr_err("diag: Invalid pointers buf: %pK, dest_buf %pK in %s\n",
		       buf, dest_buf, __func__);
		return -EIO;
	}

	version = *(buf + STM_CMD_VERSION_OFFSET);
	mask = *(buf + STM_CMD_MASK_OFFSET);
	cmd = *(buf + STM_CMD_DATA_OFFSET);

	/*
	 * Check if command is valid. If the command is asking for
	 * status, then the processor mask field is to be ignored.
	 */
	if ((version != 2) || (cmd > STATUS_STM) ||
		((cmd != STATUS_STM) && ((mask == 0) || (0 != (mask >> 4))))) {
		/* Command is invalid. Send bad param message response */
		dest_buf[0] = BAD_PARAM_RESPONSE_MESSAGE;
		for (i = 0; i < STM_CMD_NUM_BYTES; i++)
			dest_buf[i+1] = *(buf + i);
		return STM_CMD_NUM_BYTES+1;
	} else if (cmd != STATUS_STM) {
		if (mask & DIAG_STM_MODEM)
			diag_process_stm_mask(cmd, DIAG_STM_MODEM,
					      PERIPHERAL_MODEM);

		if (mask & DIAG_STM_LPASS)
			diag_process_stm_mask(cmd, DIAG_STM_LPASS,
					      PERIPHERAL_LPASS);

		if (mask & DIAG_STM_WCNSS)
			diag_process_stm_mask(cmd, DIAG_STM_WCNSS,
					      PERIPHERAL_WCNSS);

		if (mask & DIAG_STM_SENSORS)
			diag_process_stm_mask(cmd, DIAG_STM_SENSORS,
						PERIPHERAL_SENSORS);

		if (mask & DIAG_STM_APPS)
			diag_process_stm_mask(cmd, DIAG_STM_APPS, APPS_DATA);
	}

	for (i = 0; i < STM_CMD_NUM_BYTES; i++)
		dest_buf[i] = *(buf + i);

	/* Set mask denoting which peripherals support STM */
	if (driver->feature[PERIPHERAL_MODEM].stm_support)
		rsp_supported |= DIAG_STM_MODEM;

	if (driver->feature[PERIPHERAL_LPASS].stm_support)
		rsp_supported |= DIAG_STM_LPASS;

	if (driver->feature[PERIPHERAL_WCNSS].stm_support)
		rsp_supported |= DIAG_STM_WCNSS;

	if (driver->feature[PERIPHERAL_SENSORS].stm_support)
		rsp_supported |= DIAG_STM_SENSORS;

	rsp_supported |= DIAG_STM_APPS;

	/* Set mask denoting STM state/status for each peripheral/APSS */
	if (driver->stm_state[PERIPHERAL_MODEM])
		rsp_status |= DIAG_STM_MODEM;

	if (driver->stm_state[PERIPHERAL_LPASS])
		rsp_status |= DIAG_STM_LPASS;

	if (driver->stm_state[PERIPHERAL_WCNSS])
		rsp_status |= DIAG_STM_WCNSS;

	if (driver->stm_state[PERIPHERAL_SENSORS])
		rsp_status |= DIAG_STM_SENSORS;

	if (driver->stm_state[APPS_DATA])
		rsp_status |= DIAG_STM_APPS;

	dest_buf[STM_RSP_SUPPORTED_INDEX] = rsp_supported;
	dest_buf[STM_RSP_STATUS_INDEX] = rsp_status;

	return STM_RSP_NUM_BYTES;
}

int diag_process_time_sync_query_cmd(unsigned char *src_buf, int src_len,
				      unsigned char *dest_buf, int dest_len)
{
	int write_len = 0;
	struct diag_cmd_time_sync_query_req_t *req = NULL;
	struct diag_cmd_time_sync_query_rsp_t rsp;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d",
			__func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	req = (struct diag_cmd_time_sync_query_req_t *)src_buf;
	rsp.header.cmd_code = req->header.cmd_code;
	rsp.header.subsys_id = req->header.subsys_id;
	rsp.header.subsys_cmd_code = req->header.subsys_cmd_code;
	rsp.version = req->version;
	rsp.time_api = driver->uses_time_api;
	memcpy(dest_buf, &rsp, sizeof(rsp));
	write_len = sizeof(rsp);
	return write_len;
}

int diag_process_time_sync_switch_cmd(unsigned char *src_buf, int src_len,
				      unsigned char *dest_buf, int dest_len)
{
	uint8_t peripheral, status = 0;
	struct diag_cmd_time_sync_switch_req_t *req = NULL;
	struct diag_cmd_time_sync_switch_rsp_t rsp;
	struct diag_ctrl_msg_time_sync time_sync_msg;
	int msg_size = sizeof(struct diag_ctrl_msg_time_sync);
	int err = 0, write_len = 0;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d",
			__func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	req = (struct diag_cmd_time_sync_switch_req_t *)src_buf;
	rsp.header.cmd_code = req->header.cmd_code;
	rsp.header.subsys_id = req->header.subsys_id;
	rsp.header.subsys_cmd_code = req->header.subsys_cmd_code;
	rsp.version = req->version;
	rsp.time_api = req->time_api;
	if ((req->version > 1) || (req->time_api > 1) ||
					(req->persist_time > 0)) {
		dest_buf[0] = BAD_PARAM_RESPONSE_MESSAGE;
		rsp.time_api_status = 0;
		rsp.persist_time_status = PERSIST_TIME_NOT_SUPPORTED;
		memcpy(dest_buf + 1, &rsp, sizeof(rsp));
		write_len = sizeof(rsp) + 1;
		timestamp_switch = 0;
		return write_len;
	}

	time_sync_msg.ctrl_pkt_id = DIAG_CTRL_MSG_TIME_SYNC_PKT;
	time_sync_msg.ctrl_pkt_data_len = 5;
	time_sync_msg.version = 1;
	time_sync_msg.time_api = req->time_api;

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		err = diagfwd_write(peripheral, TYPE_CNTL, &time_sync_msg,
					msg_size);
		if (err && err != -ENODEV) {
			pr_err("diag: In %s, unable to write to peripheral: %d, type: %d, len: %d, err: %d\n",
				__func__, peripheral, TYPE_CNTL,
				msg_size, err);
			status |= (1 << peripheral);
		}
	}

	driver->time_sync_enabled = 1;
	driver->uses_time_api = req->time_api;

	switch (req->time_api) {
	case 0:
		timestamp_switch = 0;
		break;
	case 1:
		timestamp_switch = 1;
		break;
	default:
		timestamp_switch = 0;
		break;
	}

	rsp.time_api_status = status;
	rsp.persist_time_status = PERSIST_TIME_NOT_SUPPORTED;
	memcpy(dest_buf, &rsp, sizeof(rsp));
	write_len = sizeof(rsp);
	return write_len;
}

int diag_cmd_log_on_demand(unsigned char *src_buf, int src_len,
			   unsigned char *dest_buf, int dest_len)
{
	int write_len = 0;
	struct diag_log_on_demand_rsp_t header;

	if (!driver->diagfwd_cntl[PERIPHERAL_MODEM] ||
	    !driver->diagfwd_cntl[PERIPHERAL_MODEM]->ch_open ||
	    !driver->log_on_demand_support)
		return 0;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	header.cmd_code = DIAG_CMD_LOG_ON_DMND;
	header.log_code = *(uint16_t *)(src_buf + 1);
	header.status = 1;
	memcpy(dest_buf, &header, sizeof(struct diag_log_on_demand_rsp_t));
	write_len += sizeof(struct diag_log_on_demand_rsp_t);

	return write_len;
}

int diag_cmd_get_mobile_id(unsigned char *src_buf, int src_len,
			   unsigned char *dest_buf, int dest_len)
{
	int write_len = 0;
	struct diag_pkt_header_t *header = NULL;
	struct diag_cmd_ext_mobile_rsp_t rsp;

	if (!src_buf || src_len != sizeof(*header) || !dest_buf ||
	    dest_len < sizeof(rsp))
		return -EIO;

	header = (struct diag_pkt_header_t *)src_buf;
	rsp.header.cmd_code = header->cmd_code;
	rsp.header.subsys_id = header->subsys_id;
	rsp.header.subsys_cmd_code = header->subsys_cmd_code;
	rsp.version = 2;
	rsp.padding[0] = 0;
	rsp.padding[1] = 0;
	rsp.padding[2] = 0;
	rsp.family = 0;
	rsp.chip_id = (uint32_t)socinfo_get_id();

	memcpy(dest_buf, &rsp, sizeof(rsp));
	write_len += sizeof(rsp);

	return write_len;
}

int diag_check_common_cmd(struct diag_pkt_header_t *header)
{
	int i;

	if (!header)
		return -EIO;

	for (i = 0; i < DIAG_NUM_COMMON_CMD; i++) {
		if (header->cmd_code == common_cmds[i])
			return 1;
	}

	return 0;
}

static int diag_cmd_chk_stats(unsigned char *src_buf, int src_len,
			      unsigned char *dest_buf, int dest_len)
{
	int payload = 0;
	int write_len = 0;
	struct diag_pkt_header_t *header = NULL;
	struct diag_cmd_stats_rsp_t rsp;

	if (!src_buf || src_len < sizeof(struct diag_pkt_header_t) ||
	    !dest_buf || dest_len < sizeof(rsp))
		return -EINVAL;

	header = (struct diag_pkt_header_t *)src_buf;

	if (header->cmd_code != DIAG_CMD_DIAG_SUBSYS ||
	    header->subsys_id != DIAG_SS_DIAG)
		return -EINVAL;

	switch (header->subsys_cmd_code) {
	case DIAG_CMD_OP_GET_MSG_ALLOC:
		payload = driver->msg_stats.alloc_count;
		break;
	case DIAG_CMD_OP_GET_MSG_DROP:
		payload = driver->msg_stats.drop_count;
		break;
	case DIAG_CMD_OP_RESET_MSG_STATS:
		diag_record_stats(DATA_TYPE_F3, PKT_RESET);
		break;
	case DIAG_CMD_OP_GET_LOG_ALLOC:
		payload = driver->log_stats.alloc_count;
		break;
	case DIAG_CMD_OP_GET_LOG_DROP:
		payload = driver->log_stats.drop_count;
		break;
	case DIAG_CMD_OP_RESET_LOG_STATS:
		diag_record_stats(DATA_TYPE_LOG, PKT_RESET);
		break;
	case DIAG_CMD_OP_GET_EVENT_ALLOC:
		payload = driver->event_stats.alloc_count;
		break;
	case DIAG_CMD_OP_GET_EVENT_DROP:
		payload = driver->event_stats.drop_count;
		break;
	case DIAG_CMD_OP_RESET_EVENT_STATS:
		diag_record_stats(DATA_TYPE_EVENT, PKT_RESET);
		break;
	default:
		return -EINVAL;
	}

	memcpy(&rsp.header, header, sizeof(struct diag_pkt_header_t));
	rsp.payload = payload;
	write_len = sizeof(rsp);
	memcpy(dest_buf, &rsp, sizeof(rsp));

	return write_len;
}

static int diag_cmd_disable_hdlc(unsigned char *src_buf, int src_len,
				 unsigned char *dest_buf, int dest_len)
{
	struct diag_pkt_header_t *header = NULL;
	struct diag_cmd_hdlc_disable_rsp_t rsp;
	int write_len = 0;

	if (!src_buf || src_len < sizeof(*header) ||
	    !dest_buf || dest_len < sizeof(rsp)) {
		return -EIO;
	}

	header = (struct diag_pkt_header_t *)src_buf;
	if (header->cmd_code != DIAG_CMD_DIAG_SUBSYS ||
	    header->subsys_id != DIAG_SS_DIAG ||
	    header->subsys_cmd_code != DIAG_CMD_OP_HDLC_DISABLE) {
		return -EINVAL;
	}

	memcpy(&rsp.header, header, sizeof(struct diag_pkt_header_t));
	rsp.framing_version = 1;
	rsp.result = 0;
	write_len = sizeof(rsp);
	memcpy(dest_buf, &rsp, sizeof(rsp));

	return write_len;
}

void diag_send_error_rsp(unsigned char *buf, int len)
{
	/* -1 to accomodate the first byte 0x13 */
	if (len > (DIAG_MAX_RSP_SIZE - 1)) {
		pr_err("diag: cannot send err rsp, huge length: %d\n", len);
		return;
	}

	*(uint8_t *)driver->apps_rsp_buf = DIAG_CMD_ERROR;
	memcpy((driver->apps_rsp_buf + sizeof(uint8_t)), buf, len);
	diag_send_rsp(driver->apps_rsp_buf, len + 1);
}

int diag_process_apps_pkt(unsigned char *buf, int len, int pid)
{
	int i, p_mask = 0;
	int mask_ret;
	int write_len = 0;
	unsigned char *temp = NULL;
	struct diag_cmd_reg_entry_t entry;
	struct diag_cmd_reg_entry_t *temp_entry = NULL;
	struct diag_cmd_reg_t *reg_item = NULL;
	struct diag_md_session_t *info = NULL;

	if (!buf)
		return -EIO;

	/* Check if the command is a supported mask command */
	mask_ret = diag_process_apps_masks(buf, len, pid);
	if (mask_ret > 0) {
		diag_send_rsp(driver->apps_rsp_buf, mask_ret);
		return 0;
	}

	temp = buf;
	entry.cmd_code = (uint16_t)(*(uint8_t *)temp);
	temp += sizeof(uint8_t);
	entry.subsys_id = (uint16_t)(*(uint8_t *)temp);
	temp += sizeof(uint8_t);
	entry.cmd_code_hi = (uint16_t)(*(uint16_t *)temp);
	entry.cmd_code_lo = (uint16_t)(*(uint16_t *)temp);
	temp += sizeof(uint16_t);

	pr_debug("diag: In %s, received cmd %02x %02x %02x\n",
		 __func__, entry.cmd_code, entry.subsys_id, entry.cmd_code_hi);

	if (*buf == DIAG_CMD_LOG_ON_DMND && driver->log_on_demand_support &&
	    driver->feature[PERIPHERAL_MODEM].rcvd_feature_mask) {
		write_len = diag_cmd_log_on_demand(buf, len,
						   driver->apps_rsp_buf,
						   DIAG_MAX_RSP_SIZE);
		if (write_len > 0)
			diag_send_rsp(driver->apps_rsp_buf, write_len);
		return 0;
	}

	mutex_lock(&driver->cmd_reg_mutex);
	temp_entry = diag_cmd_search(&entry, ALL_PROC);
	if (temp_entry) {
		reg_item = container_of(temp_entry, struct diag_cmd_reg_t,
								entry);
		mutex_lock(&driver->md_session_lock);
		info = diag_md_session_get_pid(pid);
		if (info) {
			p_mask = info->peripheral_mask;
			mutex_unlock(&driver->md_session_lock);
			if (MD_PERIPHERAL_MASK(reg_item->proc) & p_mask)
				write_len = diag_send_data(reg_item, buf, len);
		} else {
			mutex_unlock(&driver->md_session_lock);
			if (MD_PERIPHERAL_MASK(reg_item->proc) &
				driver->logging_mask)
				diag_send_error_rsp(buf, len);
			else
				write_len = diag_send_data(reg_item, buf, len);
		}
		mutex_unlock(&driver->cmd_reg_mutex);
		return write_len;
	}
	mutex_unlock(&driver->cmd_reg_mutex);

#if defined(CONFIG_DIAG_OVER_USB)
	/* Check for the command/respond msg for the maximum packet length */
	if ((*buf == 0x4b) && (*(buf+1) == 0x12) &&
		(*(uint16_t *)(buf+2) == 0x0055)) {
		for (i = 0; i < 4; i++)
			*(driver->apps_rsp_buf+i) = *(buf+i);
		*(uint32_t *)(driver->apps_rsp_buf+4) = DIAG_MAX_REQ_SIZE;
		diag_send_rsp(driver->apps_rsp_buf, 8);
		return 0;
	} else if ((*buf == 0x4b) && (*(buf+1) == 0x12) &&
		(*(uint16_t *)(buf+2) == DIAG_DIAG_STM)) {
		len = diag_process_stm_cmd(buf, driver->apps_rsp_buf);
		if (len > 0) {
			diag_send_rsp(driver->apps_rsp_buf, len);
			return 0;
		}
		return len;
	}
	/* Check for time sync query command */
	else if ((*buf == DIAG_CMD_DIAG_SUBSYS) &&
		(*(buf+1) == DIAG_SS_DIAG) &&
		(*(uint16_t *)(buf+2) == DIAG_GET_TIME_API)) {
		write_len = diag_process_time_sync_query_cmd(buf, len,
							driver->apps_rsp_buf,
							DIAG_MAX_RSP_SIZE);
		if (write_len > 0)
			diag_send_rsp(driver->apps_rsp_buf, write_len);
		return 0;
	}
	/* Check for time sync switch command */
	else if ((*buf == DIAG_CMD_DIAG_SUBSYS) &&
		(*(buf+1) == DIAG_SS_DIAG) &&
		(*(uint16_t *)(buf+2) == DIAG_SET_TIME_API)) {
		write_len = diag_process_time_sync_switch_cmd(buf, len,
							driver->apps_rsp_buf,
							DIAG_MAX_RSP_SIZE);
		if (write_len > 0)
			diag_send_rsp(driver->apps_rsp_buf, write_len);
		return 0;
	}
	/* Check for download command */
	else if ((chk_apps_master()) && (*buf == 0x3A)) {
		/* send response back */
		driver->apps_rsp_buf[0] = *buf;
		diag_send_rsp(driver->apps_rsp_buf, 1);
		msleep(5000);
		/* call download API */
		msm_set_restart_mode(RESTART_DLOAD);
		printk(KERN_CRIT "diag: download mode set, Rebooting SoC..\n");
		kernel_restart(NULL);
		/* Not required, represents that command isnt sent to modem */
		return 0;
	}
	/* Check for polling for Apps only DIAG */
	else if ((*buf == 0x4b) && (*(buf+1) == 0x32) &&
		(*(buf+2) == 0x03)) {
		/* If no one has registered for polling */
		if (chk_polling_response()) {
			/* Respond to polling for Apps only DIAG */
			for (i = 0; i < 3; i++)
				driver->apps_rsp_buf[i] = *(buf+i);
			for (i = 0; i < 13; i++)
				driver->apps_rsp_buf[i+3] = 0;

			diag_send_rsp(driver->apps_rsp_buf, 16);
			return 0;
		}
	}
	/* Return the Delayed Response Wrap Status */
	else if ((*buf == 0x4b) && (*(buf+1) == 0x32) &&
		(*(buf+2) == 0x04) && (*(buf+3) == 0x0)) {
		memcpy(driver->apps_rsp_buf, buf, 4);
		driver->apps_rsp_buf[4] = wrap_enabled;
		diag_send_rsp(driver->apps_rsp_buf, 5);
		return 0;
	}
	/* Wrap the Delayed Rsp ID */
	else if ((*buf == 0x4b) && (*(buf+1) == 0x32) &&
		(*(buf+2) == 0x05) && (*(buf+3) == 0x0)) {
		wrap_enabled = true;
		memcpy(driver->apps_rsp_buf, buf, 4);
		driver->apps_rsp_buf[4] = wrap_count;
		diag_send_rsp(driver->apps_rsp_buf, 6);
		return 0;
	}
	/* Mobile ID Rsp */
	else if ((*buf == DIAG_CMD_DIAG_SUBSYS) &&
		(*(buf+1) == DIAG_SS_PARAMS) &&
		(*(buf+2) == DIAG_EXT_MOBILE_ID) && (*(buf+3) == 0x0))  {
			write_len = diag_cmd_get_mobile_id(buf, len,
						   driver->apps_rsp_buf,
						   DIAG_MAX_RSP_SIZE);
		if (write_len > 0) {
			diag_send_rsp(driver->apps_rsp_buf, write_len);
			return 0;
		}
	}
	 /*
	  * If the apps processor is master and no other
	  * processor has registered for polling command.
	  * If modem is not up and we have not received feature
	  * mask update from modem, in that case APPS should
	  * respond for 0X7C command
	  */
	else if (chk_apps_master() &&
		 !(driver->polling_reg_flag) &&
		 !(driver->diagfwd_cntl[PERIPHERAL_MODEM]->ch_open) &&
		 !(driver->feature[PERIPHERAL_MODEM].rcvd_feature_mask)) {
		/* respond to 0x0 command */
		if (*buf == 0x00) {
			for (i = 0; i < 55; i++)
				driver->apps_rsp_buf[i] = 0;

			diag_send_rsp(driver->apps_rsp_buf, 55);
			return 0;
		}
		/* respond to 0x7c command */
		else if (*buf == 0x7c) {
			driver->apps_rsp_buf[0] = 0x7c;
			for (i = 1; i < 8; i++)
				driver->apps_rsp_buf[i] = 0;
			/* Tools ID for APQ 8060 */
			*(int *)(driver->apps_rsp_buf + 8) =
							 chk_config_get_id();
			*(unsigned char *)(driver->apps_rsp_buf + 12) = '\0';
			*(unsigned char *)(driver->apps_rsp_buf + 13) = '\0';
			diag_send_rsp(driver->apps_rsp_buf, 14);
			return 0;
		}
	}
	write_len = diag_cmd_chk_stats(buf, len, driver->apps_rsp_buf,
				       DIAG_MAX_RSP_SIZE);
	if (write_len > 0) {
		diag_send_rsp(driver->apps_rsp_buf, write_len);
		return 0;
	}
	write_len = diag_cmd_disable_hdlc(buf, len, driver->apps_rsp_buf,
					  DIAG_MAX_RSP_SIZE);
	if (write_len > 0) {
		/*
		 * This mutex lock is necessary since we need to drain all the
		 * pending buffers from peripherals which may be HDLC encoded
		 * before disabling HDLC encoding on Apps processor.
		 */
		mutex_lock(&driver->hdlc_disable_mutex);
		diag_send_rsp(driver->apps_rsp_buf, write_len);
		/*
		 * Set the value of hdlc_disabled after sending the response to
		 * the tools. This is required since the tools is expecting a
		 * HDLC encoded reponse for this request.
		 */
		pr_debug("diag: In %s, disabling HDLC encoding\n",
		       __func__);
		mutex_lock(&driver->md_session_lock);
		info = diag_md_session_get_pid(pid);
		if (info)
			info->hdlc_disabled = 1;
		else
			driver->hdlc_disabled = 1;
		mutex_unlock(&driver->md_session_lock);
		diag_update_md_clients(HDLC_SUPPORT_TYPE);
		mutex_unlock(&driver->hdlc_disable_mutex);
		return 0;
	}
#endif

	/* We have now come to the end of the function. */
	if (chk_apps_only())
		diag_send_error_rsp(buf, len);

	return 0;
}

void diag_process_hdlc_pkt(void *data, unsigned len, int pid)
{
	int err = 0;
	int ret = 0;

	if (len > DIAG_MAX_HDLC_BUF_SIZE) {
		pr_err("diag: In %s, invalid length: %d\n", __func__, len);
		return;
	}

	mutex_lock(&driver->diag_hdlc_mutex);
	pr_debug("diag: In %s, received packet of length: %d, req_buf_len: %d\n",
		 __func__, len, driver->hdlc_buf_len);

	if (driver->hdlc_buf_len >= DIAG_MAX_REQ_SIZE) {
		pr_err("diag: In %s, request length is more than supported len. Dropping packet.\n",
		       __func__);
		goto fail;
	}

	hdlc_decode->dest_ptr = driver->hdlc_buf + driver->hdlc_buf_len;
	hdlc_decode->dest_size = DIAG_MAX_HDLC_BUF_SIZE - driver->hdlc_buf_len;
	hdlc_decode->src_ptr = data;
	hdlc_decode->src_size = len;
	hdlc_decode->src_idx = 0;
	hdlc_decode->dest_idx = 0;

	ret = diag_hdlc_decode(hdlc_decode);
	/*
	 * driver->hdlc_buf is of size DIAG_MAX_HDLC_BUF_SIZE. But the decoded
	 * packet should be within DIAG_MAX_REQ_SIZE.
	 */
	if (driver->hdlc_buf_len + hdlc_decode->dest_idx <= DIAG_MAX_REQ_SIZE) {
		driver->hdlc_buf_len += hdlc_decode->dest_idx;
	} else {
		pr_err_ratelimited("diag: In %s, Dropping packet. pkt_size: %d, max: %d\n",
				   __func__,
				   driver->hdlc_buf_len + hdlc_decode->dest_idx,
				   DIAG_MAX_REQ_SIZE);
		goto fail;
	}

	if (ret == HDLC_COMPLETE) {
		err = crc_check(driver->hdlc_buf, driver->hdlc_buf_len);
		if (err) {
			/* CRC check failed. */
			pr_err_ratelimited("diag: In %s, bad CRC. Dropping packet\n",
					   __func__);
			goto fail;
		}
		driver->hdlc_buf_len -= HDLC_FOOTER_LEN;

		if (driver->hdlc_buf_len < 1) {
			pr_err_ratelimited("diag: In %s, message is too short, len: %d, dest len: %d\n",
					   __func__, driver->hdlc_buf_len,
					   hdlc_decode->dest_idx);
			goto fail;
		}

		err = diag_process_apps_pkt(driver->hdlc_buf,
					    driver->hdlc_buf_len, pid);
		if (err < 0)
			goto fail;
	} else {
		goto end;
	}

	driver->hdlc_buf_len = 0;
	mutex_unlock(&driver->diag_hdlc_mutex);
	return;

fail:
	/*
	 * Tools needs to get a response in order to start its
	 * recovery algorithm. Send an error response if the
	 * packet is not in expected format.
	 */
	diag_send_error_rsp(driver->hdlc_buf, driver->hdlc_buf_len);
	driver->hdlc_buf_len = 0;
end:
	mutex_unlock(&driver->diag_hdlc_mutex);
}

static int diagfwd_mux_open(int id, int mode)
{
	uint8_t i;
	unsigned long flags;

	switch (mode) {
	case DIAG_USB_MODE:
		driver->usb_connected = 1;
		break;
	case DIAG_MEMORY_DEVICE_MODE:
		break;
	default:
		return -EINVAL;
	}

	if (driver->rsp_buf_busy) {
		/*
		 * When a client switches from callback mode to USB mode
		 * explicitly, there can be a situation when the last response
		 * is not drained to the user space application. Reset the
		 * in_busy flag in this case.
		 */
		spin_lock_irqsave(&driver->rsp_buf_busy_lock, flags);
		driver->rsp_buf_busy = 0;
		spin_unlock_irqrestore(&driver->rsp_buf_busy_lock, flags);
	}
	for (i = 0; i < NUM_PERIPHERALS; i++) {
		diagfwd_open(i, TYPE_DATA);
		diagfwd_open(i, TYPE_CMD);
	}
	queue_work(driver->diag_real_time_wq, &driver->diag_real_time_work);
	return 0;
}

static int diagfwd_mux_close(int id, int mode)
{
	uint8_t i;

	switch (mode) {
	case DIAG_USB_MODE:
		driver->usb_connected = 0;
		break;
	case DIAG_MEMORY_DEVICE_MODE:
		break;
	default:
		return -EINVAL;
	}

	if ((driver->logging_mode == DIAG_MULTI_MODE &&
		driver->md_session_mode == DIAG_MD_NONE) ||
		(driver->md_session_mode == DIAG_MD_PERIPHERAL)) {
		/*
		 * This case indicates that the USB is removed
		 * but there is a client running in background
		 * with Memory Device mode.
		 */
	} else {
		/*
		 * With sysfs parameter to clear masks set,
		 * peripheral masks are cleared on ODL exit and
		 * USB disconnection and buffers are not marked busy.
		 * This enables read and drop of stale packets.
		 *
		 * With sysfs parameter to clear masks cleared,
		 * masks are not cleared and buffers are to be marked
		 * busy to ensure traffic generated by peripheral
		 * are not read
		 */
		if (!(diag_mask_param())) {
			for (i = 0; i < NUM_PERIPHERALS; i++) {
				diagfwd_close(i, TYPE_DATA);
				diagfwd_close(i, TYPE_CMD);
			}
		}
		/* Re enable HDLC encoding */
		pr_debug("diag: In %s, re-enabling HDLC encoding\n",
		       __func__);
		mutex_lock(&driver->hdlc_disable_mutex);
		if (driver->md_session_mode == DIAG_MD_NONE)
			driver->hdlc_disabled = 0;
		mutex_unlock(&driver->hdlc_disable_mutex);
		queue_work(driver->diag_wq,
			&(driver->update_user_clients));
	}
	queue_work(driver->diag_real_time_wq,
		   &driver->diag_real_time_work);
	return 0;
}

static uint8_t hdlc_reset;

static void hdlc_reset_timer_start(int pid)
{
	struct diag_md_session_t *info = NULL;

	mutex_lock(&driver->md_session_lock);
	info = diag_md_session_get_pid(pid);
	if (!hdlc_timer_in_progress) {
		hdlc_timer_in_progress = 1;
		if (info)
			mod_timer(&info->hdlc_reset_timer,
			  jiffies + msecs_to_jiffies(200));
		else
			mod_timer(&driver->hdlc_reset_timer,
			  jiffies + msecs_to_jiffies(200));
	}
	mutex_unlock(&driver->md_session_lock);
}

static void hdlc_reset_timer_func(unsigned long data)
{
	pr_debug("diag: In %s, re-enabling HDLC encoding\n",
		       __func__);
	if (hdlc_reset) {
		driver->hdlc_disabled = 0;
		queue_work(driver->diag_wq,
			&(driver->update_user_clients));
	}
	hdlc_timer_in_progress = 0;
}

void diag_md_hdlc_reset_timer_func(unsigned long pid)
{
	struct diag_md_session_t *session_info = NULL;

	pr_debug("diag: In %s, re-enabling HDLC encoding\n",
		       __func__);
	if (hdlc_reset) {
		session_info = diag_md_session_get_pid(pid);
		if (session_info)
			session_info->hdlc_disabled = 0;
		queue_work(driver->diag_wq,
			&(driver->update_md_clients));
	}
	hdlc_timer_in_progress = 0;
}

static void diag_hdlc_start_recovery(unsigned char *buf, int len,
				     int pid)
{
	int i;
	static uint32_t bad_byte_counter;
	unsigned char *start_ptr = NULL;
	struct diag_pkt_frame_t *actual_pkt = NULL;
	struct diag_md_session_t *info = NULL;

	hdlc_reset = 1;
	hdlc_reset_timer_start(pid);

	actual_pkt = (struct diag_pkt_frame_t *)buf;
	for (i = 0; i < len; i++) {
		if (actual_pkt->start == CONTROL_CHAR &&
			actual_pkt->version == 1 &&
			actual_pkt->length < len &&
			(*(uint8_t *)(buf + sizeof(struct diag_pkt_frame_t) +
			actual_pkt->length) == CONTROL_CHAR)) {
				start_ptr = &buf[i];
				break;
		}
		bad_byte_counter++;
		if (bad_byte_counter > (DIAG_MAX_REQ_SIZE +
				sizeof(struct diag_pkt_frame_t) + 1)) {
			bad_byte_counter = 0;
			pr_err("diag: In %s, re-enabling HDLC encoding\n",
					__func__);
			mutex_lock(&driver->hdlc_disable_mutex);
			mutex_lock(&driver->md_session_lock);
			info = diag_md_session_get_pid(pid);
			if (info)
				info->hdlc_disabled = 0;
			else
				driver->hdlc_disabled = 0;
			mutex_unlock(&driver->md_session_lock);
			mutex_unlock(&driver->hdlc_disable_mutex);
			diag_update_md_clients(HDLC_SUPPORT_TYPE);

			return;
		}
	}

	if (start_ptr) {
		/* Discard any partial packet reads */
		mutex_lock(&driver->hdlc_recovery_mutex);
		driver->incoming_pkt.processing = 0;
		mutex_unlock(&driver->hdlc_recovery_mutex);
		diag_process_non_hdlc_pkt(start_ptr, len - i, pid);
	}
}

void diag_process_non_hdlc_pkt(unsigned char *buf, int len, int pid)
{
	int err = 0;
	uint16_t pkt_len = 0;
	uint32_t read_bytes = 0;
	const uint32_t header_len = sizeof(struct diag_pkt_frame_t);
	struct diag_pkt_frame_t *actual_pkt = NULL;
	unsigned char *data_ptr = NULL;
	struct diag_partial_pkt_t *partial_pkt = NULL;

	mutex_lock(&driver->hdlc_recovery_mutex);
	if (!buf || len <= 0) {
		mutex_unlock(&driver->hdlc_recovery_mutex);
		return;
	}
	partial_pkt = &driver->incoming_pkt;
	if (!partial_pkt->processing) {
		mutex_unlock(&driver->hdlc_recovery_mutex);
		goto start;
	}

	if (partial_pkt->remaining > len) {
		if ((partial_pkt->read_len + len) > partial_pkt->capacity) {
			pr_err("diag: Invalid length %d, %d received in %s\n",
			       partial_pkt->read_len, len, __func__);
			mutex_unlock(&driver->hdlc_recovery_mutex);
			goto end;
		}
		memcpy(partial_pkt->data + partial_pkt->read_len, buf, len);
		read_bytes += len;
		buf += read_bytes;
		partial_pkt->read_len += len;
		partial_pkt->remaining -= len;
	} else {
		if ((partial_pkt->read_len + partial_pkt->remaining) >
						partial_pkt->capacity) {
			pr_err("diag: Invalid length during partial read %d, %d received in %s\n",
			       partial_pkt->read_len,
			       partial_pkt->remaining, __func__);
			mutex_unlock(&driver->hdlc_recovery_mutex);
			goto end;
		}
		memcpy(partial_pkt->data + partial_pkt->read_len, buf,
						partial_pkt->remaining);
		read_bytes += partial_pkt->remaining;
		buf += read_bytes;
		partial_pkt->read_len += partial_pkt->remaining;
		partial_pkt->remaining = 0;
	}

	if (partial_pkt->remaining == 0) {
		actual_pkt = (struct diag_pkt_frame_t *)(partial_pkt->data);
		data_ptr = partial_pkt->data + header_len;
		if (*(uint8_t *)(data_ptr + actual_pkt->length) !=
						CONTROL_CHAR) {
			mutex_unlock(&driver->hdlc_recovery_mutex);
			diag_hdlc_start_recovery(buf, len, pid);
			mutex_lock(&driver->hdlc_recovery_mutex);
		}
		err = diag_process_apps_pkt(data_ptr,
					    actual_pkt->length, pid);
		if (err) {
			pr_err("diag: In %s, unable to process incoming data packet, err: %d\n",
			       __func__, err);
			mutex_unlock(&driver->hdlc_recovery_mutex);
			goto end;
		}
		partial_pkt->read_len = 0;
		partial_pkt->total_len = 0;
		partial_pkt->processing = 0;
		mutex_unlock(&driver->hdlc_recovery_mutex);
		goto start;
	}
	mutex_unlock(&driver->hdlc_recovery_mutex);
	goto end;

start:
	while (read_bytes < len) {
		actual_pkt = (struct diag_pkt_frame_t *)buf;
		pkt_len = actual_pkt->length;

		if (actual_pkt->start != CONTROL_CHAR) {
			diag_hdlc_start_recovery(buf, len, pid);
			diag_send_error_rsp(buf, len);
			goto end;
		}
		mutex_lock(&driver->hdlc_recovery_mutex);
		if (pkt_len + header_len > partial_pkt->capacity) {
			pr_err("diag: In %s, incoming data is too large for the request buffer %d\n",
			       __func__, pkt_len);
			mutex_unlock(&driver->hdlc_recovery_mutex);
			diag_hdlc_start_recovery(buf, len, pid);
			break;
		}
		if ((pkt_len + header_len) > (len - read_bytes)) {
			partial_pkt->read_len = len - read_bytes;
			partial_pkt->total_len = pkt_len + header_len;
			partial_pkt->remaining = partial_pkt->total_len -
						 partial_pkt->read_len;
			partial_pkt->processing = 1;
			memcpy(partial_pkt->data, buf, partial_pkt->read_len);
			mutex_unlock(&driver->hdlc_recovery_mutex);
			break;
		}
		data_ptr = buf + header_len;
		if (*(uint8_t *)(data_ptr + actual_pkt->length) !=
						CONTROL_CHAR) {
			mutex_unlock(&driver->hdlc_recovery_mutex);
			diag_hdlc_start_recovery(buf, len, pid);
			mutex_lock(&driver->hdlc_recovery_mutex);
		}
		else
			hdlc_reset = 0;
		err = diag_process_apps_pkt(data_ptr,
					    actual_pkt->length, pid);
		if (err) {
			mutex_unlock(&driver->hdlc_recovery_mutex);
			break;
		}
		read_bytes += header_len + pkt_len + 1;
		buf += header_len + pkt_len + 1; /* advance to next pkt */
		mutex_unlock(&driver->hdlc_recovery_mutex);
	}
end:
	return;
}

static int diagfwd_mux_read_done(unsigned char *buf, int len, int ctxt)
{
	if (!buf || len <= 0)
		return -EINVAL;

	if (!driver->hdlc_disabled)
		diag_process_hdlc_pkt(buf, len, 0);
	else
		diag_process_non_hdlc_pkt(buf, len, 0);

	diag_mux_queue_read(ctxt);
	return 0;
}

static int diagfwd_mux_write_done(unsigned char *buf, int len, int buf_ctxt,
				  int ctxt)
{
	unsigned long flags;
	int peripheral = -1;
	int type = -1;
	int num = -1;

	if (!buf || len < 0)
		return -EINVAL;

	peripheral = GET_BUF_PERIPHERAL(buf_ctxt);
	type = GET_BUF_TYPE(buf_ctxt);
	num = GET_BUF_NUM(buf_ctxt);

	switch (type) {
	case TYPE_DATA:
		if (peripheral >= 0 && peripheral < NUM_PERIPHERALS) {
			diagfwd_write_done(peripheral, type, num);
			diag_ws_on_copy(DIAG_WS_MUX);
		} else if (peripheral == APPS_DATA) {
			diagmem_free(driver, (unsigned char *)buf,
				     POOL_TYPE_HDLC);
			buf = NULL;
		} else {
			pr_err_ratelimited("diag: Invalid peripheral %d in %s, type: %d\n",
					   peripheral, __func__, type);
		}
		break;
	case TYPE_CMD:
		if (peripheral >= 0 && peripheral < NUM_PERIPHERALS) {
			diagfwd_write_done(peripheral, type, num);
		} else if (peripheral == APPS_DATA) {
			spin_lock_irqsave(&driver->rsp_buf_busy_lock, flags);
			driver->rsp_buf_busy = 0;
			driver->encoded_rsp_len = 0;
			spin_unlock_irqrestore(&driver->rsp_buf_busy_lock,
					       flags);
		} else {
			pr_err_ratelimited("diag: Invalid peripheral %d in %s, type: %d\n",
					   peripheral, __func__, type);
		}
		break;
	default:
		pr_err_ratelimited("diag: Incorrect data type %d, buf_ctxt: %d in %s\n",
				   type, buf_ctxt, __func__);
		break;
	}

	return 0;
}

static struct diag_mux_ops diagfwd_mux_ops = {
	.open = diagfwd_mux_open,
	.close = diagfwd_mux_close,
	.read_done = diagfwd_mux_read_done,
	.write_done = diagfwd_mux_write_done
};

int diagfwd_init(void)
{
	int ret;
	int i;

	wrap_enabled = 0;
	wrap_count = 0;
	driver->use_device_tree = has_device_tree();
	for (i = 0; i < DIAG_NUM_PROC; i++)
		driver->real_time_mode[i] = 1;
	driver->supports_separate_cmdrsp = 1;
	driver->supports_apps_hdlc_encoding = 1;
	mutex_init(&driver->diag_hdlc_mutex);
	mutex_init(&driver->diag_cntl_mutex);
	mutex_init(&driver->mode_lock);
	driver->encoded_rsp_buf = kzalloc(DIAG_MAX_HDLC_BUF_SIZE +
				APF_DIAG_PADDING, GFP_KERNEL);
	if (!driver->encoded_rsp_buf)
		goto err;
	kmemleak_not_leak(driver->encoded_rsp_buf);
	hdlc_decode = kzalloc(sizeof(struct diag_hdlc_decode_type),
			      GFP_KERNEL);
	if (!hdlc_decode)
		goto err;
	setup_timer(&driver->hdlc_reset_timer, hdlc_reset_timer_func, 0);
	kmemleak_not_leak(hdlc_decode);
	driver->encoded_rsp_len = 0;
	driver->rsp_buf_busy = 0;
	spin_lock_init(&driver->rsp_buf_busy_lock);
	driver->user_space_data_busy = 0;
	driver->hdlc_buf_len = 0;
	INIT_LIST_HEAD(&driver->cmd_reg_list);
	driver->cmd_reg_count = 0;
	mutex_init(&driver->cmd_reg_mutex);

	for (i = 0; i < NUM_PERIPHERALS; i++) {
		driver->feature[i].separate_cmd_rsp = 0;
		driver->feature[i].stm_support = DISABLE_STM;
		driver->feature[i].rcvd_feature_mask = 0;
		driver->feature[i].peripheral_buffering = 0;
		driver->feature[i].encode_hdlc = 0;
		driver->feature[i].mask_centralization = 0;
		driver->feature[i].log_on_demand = 0;
		driver->feature[i].sent_feature_mask = 0;
		driver->buffering_mode[i].peripheral = i;
		driver->buffering_mode[i].mode = DIAG_BUFFERING_MODE_STREAMING;
		driver->buffering_mode[i].high_wm_val = DEFAULT_HIGH_WM_VAL;
		driver->buffering_mode[i].low_wm_val = DEFAULT_LOW_WM_VAL;
	}

	for (i = 0; i < NUM_STM_PROCESSORS; i++) {
		driver->stm_state_requested[i] = DISABLE_STM;
		driver->stm_state[i] = DISABLE_STM;
	}

	if (driver->hdlc_buf == NULL) {
		driver->hdlc_buf = kzalloc(DIAG_MAX_HDLC_BUF_SIZE, GFP_KERNEL);
		if (!driver->hdlc_buf)
			goto err;
		kmemleak_not_leak(driver->hdlc_buf);
	}
	if (driver->user_space_data_buf == NULL)
		driver->user_space_data_buf = kzalloc(USER_SPACE_DATA,
							GFP_KERNEL);
	if (driver->user_space_data_buf == NULL)
		goto err;
	kmemleak_not_leak(driver->user_space_data_buf);
	if (driver->client_map == NULL &&
	    (driver->client_map = kzalloc
	     ((driver->num_clients) * sizeof(struct diag_client_map),
		   GFP_KERNEL)) == NULL)
		goto err;
	kmemleak_not_leak(driver->client_map);
	if (driver->data_ready == NULL &&
	     (driver->data_ready = kzalloc(driver->num_clients * sizeof(int)
							, GFP_KERNEL)) == NULL)
		goto err;
	kmemleak_not_leak(driver->data_ready);
	if (driver->apps_req_buf == NULL) {
		driver->apps_req_buf = kzalloc(DIAG_MAX_REQ_SIZE, GFP_KERNEL);
		if (!driver->apps_req_buf)
			goto err;
		kmemleak_not_leak(driver->apps_req_buf);
	}
	if (driver->dci_pkt_buf == NULL) {
		driver->dci_pkt_buf = kzalloc(DCI_BUF_SIZE, GFP_KERNEL);
		if (!driver->dci_pkt_buf)
			goto err;
		kmemleak_not_leak(driver->dci_pkt_buf);
	}
	if (driver->apps_rsp_buf == NULL) {
		driver->apps_rsp_buf = kzalloc(DIAG_MAX_RSP_SIZE, GFP_KERNEL);
		if (driver->apps_rsp_buf == NULL)
			goto err;
		kmemleak_not_leak(driver->apps_rsp_buf);
	}
	driver->diag_wq = create_singlethread_workqueue("diag_wq");
	if (!driver->diag_wq)
		goto err;
	ret = diag_mux_register(DIAG_LOCAL_PROC, DIAG_LOCAL_PROC,
				&diagfwd_mux_ops);
	if (ret) {
		pr_err("diag: Unable to register with USB, err: %d\n", ret);
		goto err;
	}

	return 0;
err:
	pr_err("diag: In %s, couldn't initialize diag\n", __func__);

	diag_usb_exit(DIAG_USB_LOCAL);
	kfree(driver->encoded_rsp_buf);
	kfree(driver->hdlc_buf);
	kfree(driver->client_map);
	kfree(driver->data_ready);
	kfree(driver->apps_req_buf);
	kfree(driver->dci_pkt_buf);
	kfree(driver->apps_rsp_buf);
	kfree(hdlc_decode);
	kfree(driver->user_space_data_buf);
	if (driver->diag_wq)
		destroy_workqueue(driver->diag_wq);
	return -ENOMEM;
}

void diagfwd_exit(void)
{
	kfree(driver->encoded_rsp_buf);
	kfree(driver->hdlc_buf);
	kfree(hdlc_decode);
	kfree(driver->client_map);
	kfree(driver->data_ready);
	kfree(driver->apps_req_buf);
	kfree(driver->dci_pkt_buf);
	kfree(driver->apps_rsp_buf);
	kfree(driver->user_space_data_buf);
	destroy_workqueue(driver->diag_wq);
}
