/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/msm_audio_acdb.h>
#include <asm/atomic.h>
#include <mach/qdsp6v2/apr_audio.h>
#include <mach/qdsp6v2/q6asm.h>
#include <mach/qdsp6v2/q6afe.h>
#include <mach/qdsp6v2/rtac.h>
#include <sound/q6adm.h>


/* Max size of payload (buf size - apr header) */
#define MAX_PAYLOAD_SIZE		4076
#define RTAC_MAX_ACTIVE_DEVICES		4
#define RTAC_MAX_ACTIVE_VOICE_COMBOS	2
#define RTAC_MAX_ACTIVE_POPP		8
#define RTAC_BUF_SIZE			4096

#define TIMEOUT_MS	1000

/* APR data */
struct rtac_apr_data {
	void			*apr_handle;
	atomic_t		cmd_state;
	wait_queue_head_t	cmd_wait;
};

static struct rtac_apr_data	rtac_adm_apr_data;
static struct rtac_apr_data	rtac_asm_apr_data[SESSION_MAX+1];
static struct rtac_apr_data	rtac_voice_apr_data[RTAC_VOICE_MODES];


/* Dev ctrl info */
struct rtac_dev_ctrl_data {
	uint32_t	dev_id;
	uint32_t	afe_port;
};

struct rtac_dev_ctrl {
	uint32_t			num_of_dev;
	struct rtac_dev_ctrl_data	device[RTAC_MAX_ACTIVE_DEVICES];
};

static struct rtac_dev_ctrl	rtac_dev_ctl_data;


/* ADM info & APR */
struct rtac_adm_data {
	uint32_t	afe_port;
	uint32_t	copp;
	uint32_t	num_of_popp;
	uint32_t	popp[RTAC_MAX_ACTIVE_POPP];
};

struct rtac_adm {
	uint32_t		num_of_dev;
	struct rtac_adm_data	device[RTAC_MAX_ACTIVE_DEVICES];
};
static struct rtac_adm		rtac_adm_data;
static u32			rtac_adm_payload_size;
static u32			rtac_adm_user_buf_size;
static u8			*rtac_adm_buffer;


/* ASM APR */
static u32			rtac_asm_payload_size;
static u32			rtac_asm_user_buf_size;
static u8			*rtac_asm_buffer;


/* Voice info & APR */
struct rtac_voice_data {
	uint32_t	tx_dev_id;
	uint32_t	rx_dev_id;
	uint32_t	tx_afe_port;
	uint32_t	rx_afe_port;
	uint16_t	cvs_handle;
	uint16_t	cvp_handle;
};

struct rtac_voice {
	uint32_t		num_of_voice_combos;
	struct rtac_voice_data	voice[RTAC_MAX_ACTIVE_VOICE_COMBOS];
};

static struct rtac_voice	rtac_voice_data;
static u32			rtac_voice_payload_size;
static u32			rtac_voice_user_buf_size;
static u8			*rtac_voice_buffer;



struct mutex			rtac_dev_ctrl_mutex;
struct mutex			rtac_adm_mutex;
struct mutex			rtac_adm_apr_mutex;
struct mutex			rtac_asm_apr_mutex;
struct mutex			rtac_voice_mutex;
struct mutex			rtac_voice_apr_mutex;

static int rtac_open(struct inode *inode, struct file *f)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int rtac_release(struct inode *inode, struct file *f)
{
	pr_debug("%s\n", __func__);
	return 0;
}


/* Dev ctrl info */
void rtac_add_dev_ctrl_device(u32 dev_id, struct msm_snddev_info *dev_info)
{
	s32 i = 0;
	pr_debug("%s\n", __func__);

	mutex_lock(&rtac_dev_ctrl_mutex);
	if (rtac_dev_ctl_data.num_of_dev == RTAC_MAX_ACTIVE_DEVICES) {
		pr_err("%s, Can't add anymore RTAC devices!\n", __func__);
		goto done;
	}

	/* Check if device already added */
	if (rtac_dev_ctl_data.num_of_dev != 0) {
		for (; i < rtac_dev_ctl_data.num_of_dev; i++) {
			if (rtac_dev_ctl_data.device[i].dev_id == dev_id)
				goto done;
		}
	}

	/* Add device */
	rtac_dev_ctl_data.num_of_dev++;
	rtac_dev_ctl_data.device[i].dev_id = dev_id;
	rtac_dev_ctl_data.device[i].afe_port = dev_info->copp_id;
done:
	mutex_unlock(&rtac_dev_ctrl_mutex);
	return;
}

void shift_dev_ctrl_devices(u32 dev_idx)
{
	for (; dev_idx < rtac_dev_ctl_data.num_of_dev - 1; dev_idx++) {
		rtac_dev_ctl_data.device[dev_idx].dev_id =
			rtac_dev_ctl_data.device[dev_idx + 1].dev_id;
		rtac_dev_ctl_data.device[dev_idx].afe_port =
			rtac_dev_ctl_data.device[dev_idx + 1].afe_port;
	}
}

void rtac_remove_dev_ctrl_device(u32 dev_id)
{
	s32 i;
	pr_debug("%s\n", __func__);

	mutex_lock(&rtac_dev_ctrl_mutex);
	if (rtac_dev_ctl_data.num_of_dev == 0)
		goto done;

	/* look for device */
	for (i = 0; i < rtac_dev_ctl_data.num_of_dev; i++) {
		if (rtac_dev_ctl_data.device[i].dev_id == dev_id) {
			shift_dev_ctrl_devices(i);
			rtac_dev_ctl_data.device[i].dev_id = 0;
			rtac_dev_ctl_data.device[i].afe_port = 0;
			rtac_dev_ctl_data.num_of_dev--;
			break;
		}
	}
done:
	mutex_unlock(&rtac_dev_ctrl_mutex);
	return;
}

void update_rtac(u32 evt_id, u32 dev_id, struct msm_snddev_info *dev_info)
{
	switch (evt_id) {
	case AUDDEV_EVT_DEV_RDY:
		rtac_add_dev_ctrl_device(dev_id, dev_info);
		break;
	case AUDDEV_EVT_DEV_RLS:
		rtac_remove_dev_ctrl_device(dev_id);
		break;
	default:
		break;
	}
}


/* ADM Info */
void add_popp(u32 dev_idx, u32 port_id, u32 popp_id)
{
	u32 i = 0;

	for (; i < rtac_adm_data.device[dev_idx].num_of_popp; i++)
		if (rtac_adm_data.device[dev_idx].popp[i] == popp_id)
			goto done;


	if (rtac_adm_data.device[dev_idx].num_of_popp ==
			RTAC_MAX_ACTIVE_POPP) {
		pr_err("%s, Max POPP!\n", __func__);
		goto done;
	}
	rtac_adm_data.device[dev_idx].popp[
		rtac_adm_data.device[dev_idx].num_of_popp++] = popp_id;
done:
	return;
}

void rtac_add_adm_device(u32 port_id, u32 popp_id)
{
	u32 i = 0;
	pr_debug("%s\n", __func__);

	mutex_lock(&rtac_adm_mutex);
	if (rtac_adm_data.num_of_dev == RTAC_MAX_ACTIVE_DEVICES) {
		pr_err("%s, Can't add anymore RTAC devices!\n", __func__);
		goto done;
	}

	/* Check if device already added */
	if (rtac_adm_data.num_of_dev != 0) {
		for (; i < rtac_adm_data.num_of_dev; i++) {
			if (rtac_adm_data.device[i].afe_port == port_id)
				add_popp(i, port_id, popp_id);
				goto done;
		}
	}

	if (rtac_adm_data.device[i].num_of_popp == RTAC_MAX_ACTIVE_POPP) {
		pr_err("%s, Max POPP!\n", __func__);
		goto done;
	}

	/* Add device */
	rtac_adm_data.num_of_dev++;
	rtac_adm_data.device[i].afe_port = port_id;
	rtac_adm_data.device[i].copp = adm_get_copp_id(port_id);
	rtac_adm_data.device[i].popp[
		rtac_adm_data.device[i].num_of_popp++] = popp_id;
done:
	mutex_unlock(&rtac_adm_mutex);
	return;
}

void shift_adm_devices(u32 dev_idx)
{
	for (; dev_idx < rtac_adm_data.num_of_dev - 1; dev_idx++) {
		memcpy(&rtac_adm_data.device[dev_idx],
			&rtac_adm_data.device[dev_idx + 1],
			sizeof(rtac_adm_data.device[dev_idx]));
	}
}

void rtac_remove_adm_device(u32 port_id)
{
	s32 i;
	pr_debug("%s\n", __func__);

	mutex_lock(&rtac_adm_mutex);
	/* look for device */
	for (i = 0; i < rtac_adm_data.num_of_dev; i++) {
		if (rtac_adm_data.device[i].afe_port == port_id) {
			shift_adm_devices(i);
			memset(&rtac_adm_data.device[i], 0,
				sizeof(rtac_adm_data.device[i]));
			rtac_adm_data.num_of_dev--;
			break;
		}
	}
	mutex_unlock(&rtac_adm_mutex);
	return;
}


/* Voice Info */
void set_rtac_voice_data(int idx, struct voice_data *v)
{
	rtac_voice_data.voice[idx].tx_dev_id = v->dev_tx.dev_id;
	rtac_voice_data.voice[idx].rx_dev_id = v->dev_rx.dev_id;
	rtac_voice_data.voice[idx].tx_afe_port = v->dev_tx.dev_port_id;
	rtac_voice_data.voice[idx].rx_afe_port = v->dev_rx.dev_port_id;
	rtac_voice_data.voice[idx].cvs_handle = v->cvs_handle;
	rtac_voice_data.voice[idx].cvp_handle = v->cvp_handle;

}

void rtac_add_voice(struct voice_data *v)
{
	u32 i = 0;
	pr_debug("%s\n", __func__);
	mutex_lock(&rtac_voice_mutex);

	if (rtac_voice_data.num_of_voice_combos ==
			RTAC_MAX_ACTIVE_VOICE_COMBOS) {
		pr_err("%s, Can't add anymore RTAC devices!\n", __func__);
		goto done;
	}

	/* Check if device already added */
	if (rtac_voice_data.num_of_voice_combos != 0) {
		for (; i < rtac_voice_data.num_of_voice_combos; i++) {
			if (rtac_voice_data.voice[i].cvp_handle ==
							v->cvp_handle) {
				set_rtac_voice_data(i, v);
				goto done;
			}
		}
	}

	/* Add device */
	rtac_voice_data.num_of_voice_combos++;
	set_rtac_voice_data(i, v);
done:
	mutex_unlock(&rtac_voice_mutex);
	return;
}

void shift_voice_devices(u32 idx)
{
	for (; idx < rtac_voice_data.num_of_voice_combos - 1; idx++) {
		memcpy(&rtac_voice_data.voice[idx],
			&rtac_voice_data.voice[idx + 1],
			sizeof(rtac_voice_data.voice[idx]));
	}
}

void rtac_remove_voice(struct voice_data *v)
{
	u32 i = 0;
	pr_debug("%s\n", __func__);

	mutex_lock(&rtac_voice_mutex);
	/* look for device */
	for (i = 0; i < rtac_voice_data.num_of_voice_combos; i++) {
		if (rtac_voice_data.voice[i].cvp_handle == v->cvp_handle) {
			shift_voice_devices(i);
			rtac_voice_data.num_of_voice_combos--;
			memset(&rtac_voice_data.voice[
				rtac_voice_data.num_of_voice_combos], 0,
				sizeof(rtac_voice_data.voice
				[rtac_voice_data.num_of_voice_combos]));
			break;
		}
	}
	mutex_unlock(&rtac_voice_mutex);
	return;
}



/* ADM APR */
void rtac_set_adm_handle(void *handle)
{
	pr_debug("%s\n", __func__);

	mutex_lock(&rtac_adm_apr_mutex);
	rtac_adm_apr_data.apr_handle = handle;
	mutex_unlock(&rtac_adm_apr_mutex);
}

bool rtac_make_adm_callback(uint32_t *payload, u32 payload_size)
{
	if (atomic_read(&rtac_adm_apr_data.cmd_state) != 1)
		return false;

	pr_debug("%s\n", __func__);
	/* Offset data for in-band payload */
	rtac_copy_adm_payload_to_user(payload, payload_size);
	atomic_set(&rtac_adm_apr_data.cmd_state, 0);
	wake_up(&rtac_adm_apr_data.cmd_wait);
	return true;
}

void rtac_copy_adm_payload_to_user(void *payload, u32 payload_size)
{
	pr_debug("%s\n", __func__);
	rtac_adm_payload_size = payload_size;

	memcpy(rtac_adm_buffer, &payload_size, sizeof(u32));
	if (payload_size != 0) {
		if (payload_size > rtac_adm_user_buf_size) {
			pr_err("%s: Buffer set not big enough for "
				"returned data, buf size = %d, "
				"ret data = %d\n", __func__,
				rtac_adm_user_buf_size, payload_size);
			goto done;
		}
		memcpy(rtac_adm_buffer + sizeof(u32), payload, payload_size);
	}
done:
	return;
}

u32 send_adm_apr(void *buf, u32 opcode)
{
	s32				result;
	u32				count = 0;
	u32				bytes_returned = 0;
	u32				port_id = 0;
	u32				copp_id;
	u32				payload_size;
	struct apr_hdr			adm_params;
	pr_debug("%s\n", __func__);

	if (copy_from_user(&count, (void *)buf, sizeof(count))) {
			pr_err("%s: Copy to user failed! buf = 0x%x\n",
			       __func__, (unsigned int)buf);
			result = -EFAULT;
			goto done;
	}

	if (count <= 0) {
		pr_err("%s: Invalid buffer size = %d\n", __func__, count);
		goto done;
	}

	if (copy_from_user(&payload_size, buf + sizeof(u32), sizeof(u32))) {
		pr_err("%s: Could not copy payload size from user buffer\n",
			__func__);
		goto done;
	}


	if ((payload_size < 0) ||
		(payload_size > MAX_PAYLOAD_SIZE)) {

			pr_err("%s: Invalid payload size = %d\n",
				__func__, payload_size);
		goto done;
	}

	if (copy_from_user(&copp_id, buf + 2 * sizeof(u32), sizeof(u32))) {
		pr_err("%s: Could not copy port id from user buffer\n",
			__func__);
		goto done;
	}

	for (port_id = 0; port_id < AFE_MAX_PORTS; port_id++) {
		if (adm_get_copp_id(port_id) == copp_id)
			break;
	}
	if (port_id >= AFE_MAX_PORTS) {
		pr_err("%s: Invalid Port ID = %d\n", __func__, port_id);
		goto done;
	}

	mutex_lock(&rtac_adm_apr_mutex);
	if (rtac_adm_apr_data.apr_handle == NULL) {
		pr_err("%s: APR not initialized\n", __func__);
		goto err;
	}

	/* Set globals for copy of returned payload */
	rtac_adm_user_buf_size = count;
	/* Copy buffer to in-band payload */
	if (copy_from_user(rtac_adm_buffer + sizeof(adm_params),
			buf + 3 * sizeof(u32), payload_size)) {
		pr_err("%s: Could not copy payload from user buffer\n",
			__func__);
		goto err;
	}

	/* Pack header */
	adm_params.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(20), APR_PKT_VER);
	adm_params.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		payload_size);
	adm_params.src_svc = APR_SVC_ADM;
	adm_params.src_domain = APR_DOMAIN_APPS;
	adm_params.src_port = port_id;
	adm_params.dest_svc = APR_SVC_ADM;
	adm_params.dest_domain = APR_DOMAIN_ADSP;
	adm_params.dest_port = adm_get_copp_id(port_id);
	adm_params.token = port_id;
	adm_params.opcode = opcode;

	memcpy(rtac_adm_buffer, &adm_params, sizeof(adm_params));
	atomic_set(&rtac_adm_apr_data.cmd_state, 1);

	pr_debug("%s: Sending RTAC command size = %d\n",
		__func__, adm_params.pkt_size);

	result = apr_send_pkt(rtac_adm_apr_data.apr_handle,
				(uint32_t *)rtac_adm_buffer);
	if (result < 0) {
		pr_err("%s: Set params failed port = %d\n",
			__func__, port_id);
		goto err;
	}
	/* Wait for the callback */
	result = wait_event_timeout(rtac_adm_apr_data.cmd_wait,
		(atomic_read(&rtac_adm_apr_data.cmd_state) == 0),
		msecs_to_jiffies(TIMEOUT_MS));
	mutex_unlock(&rtac_adm_apr_mutex);
	if (!result) {
		pr_err("%s: Set params timed out port = %d\n",
			__func__, port_id);
		goto done;
	}

	if (rtac_adm_payload_size != 0) {
		if (copy_to_user(buf, rtac_adm_buffer,
				rtac_adm_payload_size + sizeof(u32))) {
			pr_err("%s: Could not copy buffer to user,"
				"size = %d\n", __func__, payload_size);
			goto done;
		}
	}

	/* Return data written for SET & data read for GET */
	if (opcode == ADM_CMD_GET_PARAMS)
		bytes_returned = rtac_adm_payload_size;
	else
		bytes_returned = payload_size;
done:
	return bytes_returned;
err:
	mutex_unlock(&rtac_adm_apr_mutex);
	return bytes_returned;
}


/* ASM APR */
void rtac_set_asm_handle(u32 session_id, void *handle)
{
	pr_debug("%s\n", __func__);

	mutex_lock(&rtac_asm_apr_mutex);
	rtac_asm_apr_data[session_id].apr_handle = handle;
	mutex_unlock(&rtac_asm_apr_mutex);
}

bool rtac_make_asm_callback(u32 session_id, uint32_t *payload,
	u32 payload_size)
{
	if (atomic_read(&rtac_asm_apr_data[session_id].cmd_state) != 1)
		return false;

	pr_debug("%s\n", __func__);
	/* Offset data for in-band payload */
	rtac_copy_asm_payload_to_user(payload, payload_size);
	atomic_set(&rtac_asm_apr_data[session_id].cmd_state, 0);
	wake_up(&rtac_asm_apr_data[session_id].cmd_wait);
	return true;
}

void rtac_copy_asm_payload_to_user(void *payload, u32 payload_size)
{
	pr_debug("%s\n", __func__);
	rtac_asm_payload_size = payload_size;

	memcpy(rtac_asm_buffer, &payload_size, sizeof(u32));
	if (payload_size) {
		if (payload_size > rtac_asm_user_buf_size) {
			pr_err("%s: Buffer set not big enough for "
				"returned data, buf size = %d, "
				"ret data = %d\n", __func__,
				rtac_asm_user_buf_size, payload_size);
			goto done;
		}
		memcpy(rtac_asm_buffer + sizeof(u32), payload, payload_size);
	}
done:
	return;
}

u32 send_rtac_asm_apr(void *buf, u32 opcode)
{
	s32				result;
	u32				count = 0;
	u32				bytes_returned = 0;
	u32				session_id = 0;
	u32				payload_size;
	struct apr_hdr			asm_params;
	pr_debug("%s\n", __func__);

	if (copy_from_user(&count, (void *)buf, sizeof(count))) {
			pr_err("%s: Copy to user failed! buf = 0x%x\n",
			       __func__, (unsigned int)buf);
			result = -EFAULT;
			goto done;
	}

	if (count <= 0) {
		pr_err("%s: Invalid buffer size = %d\n", __func__, count);
		goto done;
	}

	if (copy_from_user(&payload_size, buf + sizeof(u32), sizeof(u32))) {
		pr_err("%s: Could not copy payload size from user buffer\n",
			__func__);
		goto done;
	}

	if ((payload_size < 0) ||
		(payload_size > MAX_PAYLOAD_SIZE)) {

			pr_err("%s: Invalid payload size = %d\n",
				__func__, payload_size);
		goto done;
	}

	if (copy_from_user(&session_id, buf + 2 * sizeof(u32), sizeof(u32))) {
		pr_err("%s: Could not copy session id from user buffer\n",
			__func__);
		goto done;
	}
	if (session_id >= AFE_MAX_PORTS) {
		pr_err("%s: Invalid Session = %d\n", __func__, session_id);
		goto done;
	}

	mutex_lock(&rtac_asm_apr_mutex);
	if (rtac_asm_apr_data[session_id].apr_handle == NULL) {
		pr_err("%s: APR not initialized\n", __func__);
		goto err;
	}

	/* Set globals for copy of returned payload */
	rtac_asm_user_buf_size = count;

	/* Copy buffer to in-band payload */
	if (copy_from_user(rtac_asm_buffer + sizeof(asm_params),
			buf + 3 * sizeof(u32), payload_size)) {
		pr_err("%s: Could not copy payload from user buffer\n",
			__func__);
		goto err;
	}

	/* Pack header */
	asm_params.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(20), APR_PKT_VER);
	asm_params.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		payload_size);
	asm_params.src_svc = q6asm_get_apr_service_id(session_id);
	asm_params.src_domain = APR_DOMAIN_APPS;
	asm_params.src_port = (session_id << 8) | 0x0001;
	asm_params.dest_svc = APR_SVC_ASM;
	asm_params.dest_domain = APR_DOMAIN_ADSP;
	asm_params.dest_port = (session_id << 8) | 0x0001;
	asm_params.token = session_id;
	asm_params.opcode = opcode;

	memcpy(rtac_asm_buffer, &asm_params, sizeof(asm_params));
	atomic_set(&rtac_asm_apr_data[session_id].cmd_state, 1);

	pr_debug("%s: Sending RTAC command size = %d, session_id=%d\n",
		__func__, asm_params.pkt_size, session_id);

	result = apr_send_pkt(rtac_asm_apr_data[session_id].apr_handle,
				(uint32_t *)rtac_asm_buffer);
	if (result < 0) {
		pr_err("%s: Set params failed session = %d\n",
			__func__, session_id);
		goto err;
	}

	/* Wait for the callback */
	result = wait_event_timeout(rtac_asm_apr_data[session_id].cmd_wait,
		(atomic_read(&rtac_asm_apr_data[session_id].cmd_state) == 0),
		5 * HZ);
	mutex_unlock(&rtac_asm_apr_mutex);
	if (!result) {
		pr_err("%s: Set params timed out session = %d\n",
			__func__, session_id);
		goto done;
	}

	if (rtac_asm_payload_size != 0) {
		if (copy_to_user(buf, rtac_asm_buffer,
				rtac_asm_payload_size + sizeof(u32))) {
			pr_err("%s: Could not copy buffer to user,"
				"size = %d\n", __func__, payload_size);
			goto done;
		}
	}

	/* Return data written for SET & data read for GET */
	if (opcode == ASM_STREAM_CMD_GET_PP_PARAMS)
		bytes_returned = rtac_asm_payload_size;
	else
		bytes_returned = payload_size;
done:
	return bytes_returned;
err:
	mutex_unlock(&rtac_asm_apr_mutex);
	return bytes_returned;
}


/* Voice APR */
void rtac_set_voice_handle(u32 mode, void *handle)
{
	pr_debug("%s\n", __func__);

	mutex_lock(&rtac_voice_apr_mutex);
	rtac_voice_apr_data[mode].apr_handle = handle;
	mutex_unlock(&rtac_voice_apr_mutex);
}

bool rtac_make_voice_callback(u32 mode, uint32_t *payload, u32 payload_size)
{
	if ((atomic_read(&rtac_voice_apr_data[mode].cmd_state) != 1) ||
			(mode < 0) || (mode >= RTAC_VOICE_MODES))
		return false;

	pr_debug("%s\n", __func__);
	/* Offset data for in-band payload */
	rtac_copy_voice_payload_to_user(payload, payload_size);
	atomic_set(&rtac_voice_apr_data[mode].cmd_state, 0);
	wake_up(&rtac_voice_apr_data[mode].cmd_wait);
	return true;
}

void rtac_copy_voice_payload_to_user(void *payload, u32 payload_size)
{
	pr_debug("%s\n", __func__);
	rtac_voice_payload_size = payload_size;

	memcpy(rtac_voice_buffer, &payload_size, sizeof(u32));
	if (payload_size) {
		if (payload_size > rtac_voice_user_buf_size) {
			pr_err("%s: Buffer set not big enough for "
				"returned data, buf size = %d, "
				"ret data = %d\n", __func__,
				rtac_voice_user_buf_size, payload_size);
			goto done;
		}
		memcpy(rtac_voice_buffer + sizeof(u32), payload, payload_size);
	}
done:
	return;
}

u32 send_voice_apr(u32 mode, void *buf, u32 opcode)
{
	s32				result;
	u32				count = 0;
	u32				bytes_returned = 0;
	u32				payload_size;
	u16				dest_port;
	struct apr_hdr			voice_params;
	pr_debug("%s\n", __func__);

	if (copy_from_user(&count, (void *)buf, sizeof(count))) {
			pr_err("%s: Copy to user failed! buf = 0x%x\n",
			       __func__, (unsigned int)buf);
			result = -EFAULT;
			goto done;
	}

	if (count <= 0) {
		pr_err("%s: Invalid buffer size = %d\n", __func__, count);
		goto done;
	}

	if (copy_from_user(&payload_size, buf + sizeof(u32), sizeof(u32))) {
		pr_err("%s: Could not copy payload size from user buffer\n",
			__func__);
		goto done;
	}

	if ((payload_size < 0) ||
		(payload_size > MAX_PAYLOAD_SIZE)) {

			pr_err("%s: Invalid payload size = %d\n",
				__func__, payload_size);
		goto done;
	}

	if (copy_from_user(&dest_port, buf + 2 * sizeof(u32), sizeof(u32))) {
		pr_err("%s: Could not copy port id from user buffer\n",
			__func__);
		goto done;
	}

	if ((mode != RTAC_CVP) && (mode != RTAC_CVS)) {
		pr_err("%s: Invalid Mode for APR, mode = %d\n",
			__func__, mode);
		goto done;
	}

	mutex_lock(&rtac_voice_apr_mutex);
	if (rtac_voice_apr_data[mode].apr_handle == NULL) {
		pr_err("%s: APR not initialized\n", __func__);
		goto err;
	}

	/* Set globals for copy of returned payload */
	rtac_voice_user_buf_size = count;

	/* Copy buffer to in-band payload */
	if (copy_from_user(rtac_voice_buffer + sizeof(voice_params),
			buf + 3 * sizeof(u32), payload_size)) {
		pr_err("%s: Could not copy payload from user buffer\n",
			__func__);
		goto err;
	}

	/* Pack header */
	voice_params.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(20), APR_PKT_VER);
	voice_params.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		payload_size);
	voice_params.src_svc = 0;
	voice_params.src_domain = APR_DOMAIN_APPS;
	voice_params.src_port = 0;
	voice_params.dest_svc = 0;
	voice_params.dest_domain = APR_DOMAIN_MODEM;
	voice_params.dest_port = dest_port;
	voice_params.token = 0;
	voice_params.opcode = opcode;

	memcpy(rtac_voice_buffer, &voice_params, sizeof(voice_params));
	atomic_set(&rtac_voice_apr_data[mode].cmd_state, 1);

	pr_debug("%s: Sending RTAC command size = %d, opcode = %x\n",
		__func__, voice_params.pkt_size, opcode);

	result = apr_send_pkt(rtac_voice_apr_data[mode].apr_handle,
					(uint32_t *)rtac_voice_buffer);
	if (result < 0) {
		pr_err("%s: apr_send_pkt failed opcode = %x\n",
			__func__, opcode);
		goto err;
	}
	/* Wait for the callback */
	result = wait_event_timeout(rtac_voice_apr_data[mode].cmd_wait,
		(atomic_read(&rtac_voice_apr_data[mode].cmd_state) == 0),
		msecs_to_jiffies(TIMEOUT_MS));
	mutex_unlock(&rtac_voice_apr_mutex);
	if (!result) {
		pr_err("%s: apr_send_pkt timed out opcode = %x\n",
			__func__, opcode);
		goto done;
	}

	if (rtac_voice_payload_size != 0) {
		if (copy_to_user(buf, rtac_voice_buffer,
				rtac_voice_payload_size + sizeof(u32))) {
			pr_err("%s: Could not copy buffer to user,"
				"size = %d\n", __func__, payload_size);
			goto done;
		}
	}

	/* Return data written for SET & data read for GET */
	if (opcode == VOICE_CMD_GET_PARAM)
		bytes_returned = rtac_voice_payload_size;
	else
		bytes_returned = payload_size;
done:
	return bytes_returned;
err:
	mutex_unlock(&rtac_voice_apr_mutex);
	return bytes_returned;
}



static int rtac_ioctl(struct inode *inode, struct file *f,
		unsigned int cmd, unsigned long arg)
{
	s32 result = 0;
	pr_debug("%s\n", __func__);

	if (arg == 0) {
		pr_err("%s: No data sent to driver!\n", __func__);
		result = -EFAULT;
		goto done;
	}

	switch (cmd) {
	case AUDIO_GET_RTAC_DEV_CTRL_INFO:
		if (copy_to_user((void *)arg, &rtac_dev_ctl_data,
						sizeof(rtac_dev_ctl_data)))
			pr_err("%s: Could not copy to userspace!\n", __func__);
		else
			result = sizeof(rtac_dev_ctl_data);
		break;
	case AUDIO_GET_RTAC_ADM_INFO:
		if (copy_to_user((void *)arg, &rtac_adm_data,
						sizeof(rtac_adm_data)))
			pr_err("%s: Could not copy to userspace!\n", __func__);
		else
			result = sizeof(rtac_adm_data);
		break;
	case AUDIO_GET_RTAC_VOICE_INFO:
		if (copy_to_user((void *)arg, &rtac_voice_data,
						sizeof(rtac_voice_data)))
			pr_err("%s: Could not copy to userspace!\n", __func__);
		else
			result = sizeof(rtac_voice_data);
		break;
	case AUDIO_GET_RTAC_ADM_CAL:
		result = send_adm_apr((void *)arg, ADM_CMD_GET_PARAMS);
		break;
	case AUDIO_SET_RTAC_ADM_CAL:
		result = send_adm_apr((void *)arg, ADM_CMD_SET_PARAMS);
		break;
	case AUDIO_GET_RTAC_ASM_CAL:
		result = send_rtac_asm_apr((void *)arg,
			ASM_STREAM_CMD_GET_PP_PARAMS);
		break;
	case AUDIO_SET_RTAC_ASM_CAL:
		result = send_rtac_asm_apr((void *)arg,
			ASM_STREAM_CMD_SET_PP_PARAMS);
		break;
	case AUDIO_GET_RTAC_CVS_CAL:
		result = send_voice_apr(RTAC_CVS, (void *)arg,
			VOICE_CMD_GET_PARAM);
		break;
	case AUDIO_SET_RTAC_CVS_CAL:
		result = send_voice_apr(RTAC_CVS, (void *)arg,
			VOICE_CMD_SET_PARAM);
		break;
	case AUDIO_GET_RTAC_CVP_CAL:
		result = send_voice_apr(RTAC_CVP, (void *)arg,
			VOICE_CMD_GET_PARAM);
		break;
	case AUDIO_SET_RTAC_CVP_CAL:
		result = send_voice_apr(RTAC_CVP, (void *)arg,
			VOICE_CMD_SET_PARAM);
		break;
	default:
		pr_err("%s: Invalid IOCTL, command = %d!\n",
		       __func__, cmd);
	}
done:
	return result;
}


static const struct file_operations rtac_fops = {
	.owner = THIS_MODULE,
	.open = rtac_open,
	.release = rtac_release,
	.ioctl = rtac_ioctl,
};

struct miscdevice rtac_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_rtac",
	.fops	= &rtac_fops,
};

static int __init rtac_init(void)
{
	int i = 0;
	pr_debug("%s\n", __func__);

	/* Dev ctrl */
	memset(&rtac_dev_ctl_data, 0, sizeof(rtac_dev_ctl_data));
	mutex_init(&rtac_dev_ctrl_mutex);

	/* ADM */
	memset(&rtac_adm_data, 0, sizeof(rtac_adm_data));
	rtac_adm_apr_data.apr_handle = NULL;
	atomic_set(&rtac_adm_apr_data.cmd_state, 0);
	init_waitqueue_head(&rtac_adm_apr_data.cmd_wait);
	mutex_init(&rtac_adm_mutex);
	mutex_init(&rtac_adm_apr_mutex);

	rtac_adm_buffer = kmalloc(RTAC_BUF_SIZE, GFP_KERNEL);
	if (rtac_adm_buffer == NULL) {
		pr_err("%s: Could not allocate payload of size = %d\n",
			__func__, RTAC_BUF_SIZE);
		goto nomem;
	}

	/* ASM */
	for (i = 0; i < SESSION_MAX+1; i++) {
		rtac_asm_apr_data[i].apr_handle = NULL;
		atomic_set(&rtac_asm_apr_data[i].cmd_state, 0);
		init_waitqueue_head(&rtac_asm_apr_data[i].cmd_wait);
	}
	mutex_init(&rtac_asm_apr_mutex);

	rtac_asm_buffer = kmalloc(RTAC_BUF_SIZE, GFP_KERNEL);
	if (rtac_asm_buffer == NULL) {
		pr_err("%s: Could not allocate payload of size = %d\n",
			__func__, RTAC_BUF_SIZE);
		goto nomem;
	}

	/* Voice */
	memset(&rtac_voice_data, 0, sizeof(rtac_voice_data));
	for (i = 0; i < RTAC_VOICE_MODES; i++) {
		rtac_voice_apr_data[i].apr_handle = NULL;
		atomic_set(&rtac_voice_apr_data[i].cmd_state, 0);
		init_waitqueue_head(&rtac_voice_apr_data[i].cmd_wait);
	}
	mutex_init(&rtac_voice_mutex);
	mutex_init(&rtac_voice_apr_mutex);

	rtac_voice_buffer = kmalloc(RTAC_BUF_SIZE, GFP_KERNEL);
	if (rtac_voice_buffer == NULL) {
		pr_err("%s: Could not allocate payload of size = %d\n",
			__func__, RTAC_BUF_SIZE);
		goto nomem;
	}

	return misc_register(&rtac_misc);
nomem:
	return -ENOMEM;
}

module_init(rtac_init);

MODULE_DESCRIPTION("MSM 8x60 Real-Time Audio Calibration driver");
MODULE_LICENSE("GPL v2");
