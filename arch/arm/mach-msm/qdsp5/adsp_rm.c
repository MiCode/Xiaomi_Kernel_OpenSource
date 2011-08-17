/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/msm_adsp.h>

#include <mach/qdsp5/qdsp5rmtcmdi.h>
#include <mach/qdsp5/qdsp5rmtmsg.h>
#include <mach/debug_mm.h>
#include "adsp.h"

#define MAX_CLIENTS		5
#define MAX_AUDIO_CLIENTS	5
#define MAX_RM_CLIENTS		MAX_AUDIO_CLIENTS

static char *rm_errs[] = {
			"",
			"PCM Blocks not Sufficient",
			"TASK is already occupied",
			"Concurrency not supported",
			"MIPS not sufficient"
			};
static struct client {
	wait_queue_head_t		wait;
	unsigned int			wait_state;
	struct aud_codec_config_ack	cfg_msg;
} rmclient[MAX_RM_CLIENTS];

static struct rm {
	struct msm_adsp_module		*mod;
	int				cnt;
	int				state;

	struct aud_codec_config_ack	cfg_msg;
	struct mutex			lock;
} rmtask;

static void rm_dsp_event(void *data, unsigned id, size_t len,
			void (*getevent) (void *ptr, size_t len));
static struct msm_adsp_ops rm_ops = {
	.event = rm_dsp_event,
};

int32_t get_adsp_resource(unsigned short client_id,
				void *cmd_buf, size_t cmd_size)
{
	int rc = 0;
	int client_idx;

	client_idx = ((client_id >> 8) * MAX_CLIENTS) + (client_id & 0xFF);
	if (client_idx >= MAX_RM_CLIENTS)
		return -EINVAL;

	mutex_lock(&rmtask.lock);
	if (rmtask.state != ADSP_STATE_ENABLED) {
		rc = msm_adsp_get("RMTASK", &rmtask.mod, &rm_ops, NULL);
		if (rc) {
			MM_ERR("Failed to get module RMTASK\n");
			mutex_unlock(&rmtask.lock);
			return rc;
		}
		rc = msm_adsp_enable(rmtask.mod);
		if (rc) {
			MM_ERR("RMTASK enable Failed\n");
			msm_adsp_put(rmtask.mod);
			mutex_unlock(&rmtask.lock);
			return rc;
		}
		rmtask.state = ADSP_STATE_ENABLED;
	}
	rmclient[client_idx].wait_state = -1;
	mutex_unlock(&rmtask.lock);
	msm_adsp_write(rmtask.mod, QDSP_apuRmtQueue, cmd_buf, cmd_size);
	rc = wait_event_interruptible_timeout(rmclient[client_idx].wait,
			rmclient[client_idx].wait_state != -1, 5 * HZ);
	mutex_lock(&rmtask.lock);
	if (unlikely(rc < 0)) {
		if (rc == -ERESTARTSYS)
			MM_ERR("wait_event_interruptible "
					"returned -ERESTARTSYS\n");
		else
			MM_ERR("wait_event_interruptible "
					"returned error\n");
		if (!rmtask.cnt)
			goto disable_rm;
		goto unlock;
	} else if (rc == 0) {
		MM_ERR("RMTASK Msg not received\n");
		rc = -ETIMEDOUT;
		if (!rmtask.cnt)
			goto disable_rm;
		goto unlock;
	}
	if (!(rmclient[client_idx].cfg_msg.enable)) {
		MM_ERR("Reason for failure: %s\n",
			rm_errs[rmclient[client_idx].cfg_msg.reason]);
		rc = -EBUSY;
		if (!rmtask.cnt)
			goto disable_rm;
		goto unlock;
	}
	rmtask.cnt++;
	mutex_unlock(&rmtask.lock);
	return 0;

disable_rm:
	msm_adsp_disable(rmtask.mod);
	msm_adsp_put(rmtask.mod);
	rmtask.state = ADSP_STATE_DISABLED;
unlock:
	mutex_unlock(&rmtask.lock);
	return rc;
}
EXPORT_SYMBOL(get_adsp_resource);

int32_t put_adsp_resource(unsigned short client_id, void *cmd_buf,
							size_t cmd_size)
{
	mutex_lock(&rmtask.lock);
	if (rmtask.state != ADSP_STATE_ENABLED) {
		mutex_unlock(&rmtask.lock);
		return 0;
	}

	msm_adsp_write(rmtask.mod, QDSP_apuRmtQueue, cmd_buf, cmd_size);
	rmtask.cnt--;
	if (!rmtask.cnt) {
		msm_adsp_disable(rmtask.mod);
		msm_adsp_put(rmtask.mod);
		rmtask.state = ADSP_STATE_DISABLED;
	}
	mutex_unlock(&rmtask.lock);
	return 0;
}
EXPORT_SYMBOL(put_adsp_resource);

static void rm_dsp_event(void *data, unsigned id, size_t len,
				void (*getevent) (void *ptr, size_t len))
{
	unsigned short client_id;
	int client_idx;

	MM_DBG("Msg ID = %d\n", id);

	switch (id) {
	case RMT_CODEC_CONFIG_ACK: {
		getevent(&rmtask.cfg_msg, sizeof(rmtask.cfg_msg));
		client_id = ((rmtask.cfg_msg.client_id << 8) |
						rmtask.cfg_msg.task_id);
		client_idx = ((client_id >> 8) * MAX_CLIENTS) +
						(client_id & 0xFF);
		memcpy(&rmclient[client_idx].cfg_msg, &rmtask.cfg_msg,
							sizeof(rmtask.cfg_msg));
		rmclient[client_idx].wait_state = 1;
		wake_up(&rmclient[client_idx].wait);
		break;
	}
	case RMT_DSP_OUT_OF_MIPS: {
		struct rmt_dsp_out_of_mips msg;
		getevent(&msg, sizeof(msg));
		MM_ERR("RMT_DSP_OUT_OF_MIPS: Not enough resorces in ADSP \
				to handle all sessions :%hx\n", msg.dec_info);
		break;
	}
	default:
		MM_DBG("Unknown Msg Id\n");
		break;
	}
}

void rmtask_init(void)
{
	int i;

	for (i = 0; i < MAX_RM_CLIENTS; i++)
		init_waitqueue_head(&rmclient[i].wait);
	mutex_init(&rmtask.lock);
}
