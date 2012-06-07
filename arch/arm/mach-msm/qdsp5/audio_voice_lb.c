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

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <mach/debug_mm.h>
#include <mach/msm_rpcrouter.h>

#include "audmgr_new.h"

#define VOICELOOPBACK_PROG	0x300000B8
#define VOICELOOP_VERS	0x00010001

#define VOICELOOPBACK_START_PROC 2
#define VOICELOOPBACK_STOP_PROC 3

#define RPC_TYPE_REQUEST 0
#define RPC_TYPE_REPLY 1

#define RPC_STATUS_FAILURE 0
#define RPC_STATUS_SUCCESS 1
#define RPC_STATUS_REJECT 1

#define RPC_COMMON_HDR_SZ  (sizeof(uint32_t) * 2)
#define RPC_REQUEST_HDR_SZ (sizeof(struct rpc_request_hdr))
#define RPC_REPLY_HDR_SZ   (sizeof(uint32_t) * 3)

#define MAX_LEN 32

struct audio {
	struct msm_rpc_endpoint *rpc_endpt;
	uint32_t rpc_prog;
	uint32_t rpc_ver;
	uint32_t rpc_status;
	struct audmgr audmgr;

	struct dentry *dentry;

	struct mutex lock;

	struct task_struct *task;

	wait_queue_head_t wait;
	int enabled;
	int thread_exit;
};

static struct audio the_audio;

static int audio_voice_loopback_thread(void *data)
{
	struct audio *audio = data;
	struct rpc_request_hdr *rpc_hdr = NULL;
	int rpc_hdr_len;

	MM_DBG("\n");

	while (!kthread_should_stop()) {
		if (rpc_hdr != NULL) {
			kfree(rpc_hdr);
			rpc_hdr = NULL;
		}

		if (audio->thread_exit)
			break;

		rpc_hdr_len = msm_rpc_read(audio->rpc_endpt,
					       (void **) &rpc_hdr,
					       -1,
					       -1);
		if (rpc_hdr_len < 0) {
			MM_ERR("RPC read failed %d\n", rpc_hdr_len);
			break;
		} else if (rpc_hdr_len < RPC_COMMON_HDR_SZ) {
			continue;
		} else {
			uint32_t rpc_type = be32_to_cpu(rpc_hdr->type);
			if (rpc_type == RPC_TYPE_REPLY) {
				struct rpc_reply_hdr *rpc_reply =
					 (void *) rpc_hdr;
				uint32_t reply_status;

				reply_status =
					be32_to_cpu(rpc_reply->reply_stat);

				if (reply_status == RPC_ACCEPTSTAT_SUCCESS)
					audio->rpc_status = \
							RPC_STATUS_SUCCESS;
				else {
					audio->rpc_status = \
							RPC_STATUS_REJECT;
					MM_ERR("RPC reply status denied\n");
				}
				wake_up(&audio->wait);
			} else {
				MM_ERR("Unexpected RPC type %d\n", rpc_type);
			}
		}
	}
	kfree(rpc_hdr);
	rpc_hdr = NULL;

	MM_DBG("Audio Voice Looopback thread stopped\n");

	return 0;
}

static int audio_voice_loopback_start(struct audio *audio)
{
	int rc = 0;
	struct audmgr_config cfg;
	struct rpc_request_hdr rpc_hdr;

	MM_DBG("\n");

	cfg.tx_rate = RPC_AUD_DEF_SAMPLE_RATE_8000;
	cfg.rx_rate = RPC_AUD_DEF_SAMPLE_RATE_8000;
	cfg.def_method = RPC_AUD_DEF_METHOD_VOICE;
	cfg.codec = RPC_AUD_DEF_CODEC_VOC_CDMA;
	cfg.snd_method = RPC_SND_METHOD_VOICE;
	rc = audmgr_enable(&audio->audmgr, &cfg);
	if (rc < 0) {
		MM_ERR("audmgr open failed, freeing instance\n");
		rc = -EINVAL;
		goto done;
	}

	memset(&rpc_hdr, 0, sizeof(rpc_hdr));

	msm_rpc_setup_req(&rpc_hdr,
			audio->rpc_prog,
			audio->rpc_ver,
			VOICELOOPBACK_START_PROC);

	audio->rpc_status = RPC_STATUS_FAILURE;
	rc = msm_rpc_write(audio->rpc_endpt,
			   &rpc_hdr,
			   sizeof(rpc_hdr));
	if (rc >= 0) {
		rc = wait_event_timeout(audio->wait,
			(audio->rpc_status != RPC_STATUS_FAILURE),
			1 * HZ);
		if (rc > 0) {
			if (audio->rpc_status != RPC_STATUS_SUCCESS) {
				MM_ERR("Start loopback failed %d\n", rc);
				rc = -EBUSY;
			} else {
				rc = 0;
			}
		} else {
			MM_ERR("Wait event for acquire failed %d\n", rc);
			rc = -EBUSY;
		}
	} else {
		audmgr_disable(&audio->audmgr);
		MM_ERR("RPC write for start loopback failed %d\n", rc);
		rc = -EBUSY;
	}
done:
	return rc;
}

static int audio_voice_loopback_stop(struct audio *audio)
{
	int rc = 0;
	struct rpc_request_hdr rpc_hdr;

	MM_DBG("\n");

	memset(&rpc_hdr, 0, sizeof(rpc_hdr));

	msm_rpc_setup_req(&rpc_hdr,
			  audio->rpc_prog,
			  audio->rpc_ver,
			  VOICELOOPBACK_STOP_PROC);

	audio->rpc_status = RPC_STATUS_FAILURE;
	audio->thread_exit = 1;
	rc = msm_rpc_write(audio->rpc_endpt,
			   &rpc_hdr,
			   sizeof(rpc_hdr));
	if (rc >= 0) {

		rc = wait_event_timeout(audio->wait,
				(audio->rpc_status != RPC_STATUS_FAILURE),
				1 * HZ);
		if (rc > 0) {
			MM_DBG("Wait event for release succeeded\n");
			rc = 0;
		} else {
			MM_ERR("Wait event for release failed %d\n", rc);
		}
	} else {
		MM_ERR("RPC write for release failed %d\n", rc);
	}

	audmgr_disable(&audio->audmgr);

	return rc;
}

static int audio_voice_loopback_open(struct audio *audio_info)
{
	int rc = 0;

	MM_DBG("\n");

	rc = audmgr_open(&audio_info->audmgr);
	if (rc) {
		MM_ERR("audmgr open failed, freeing instance\n");
		rc = -EINVAL;
		goto done;
	}

	audio_info->rpc_endpt = msm_rpc_connect_compatible(VOICELOOPBACK_PROG,
			VOICELOOP_VERS,
			MSM_RPC_UNINTERRUPTIBLE);
	if (IS_ERR(audio_info->rpc_endpt)) {
		MM_ERR("VOICE LOOPBACK RPC connect\
				failed ver 0x%x\n",
				VOICELOOP_VERS);
		rc = PTR_ERR(audio_info->rpc_endpt);
		audio_info->rpc_endpt = NULL;
		rc = -EINVAL;
	} else {
		MM_DBG("VOICE LOOPBACK connect succeeded ver 0x%x\n",
				VOICELOOP_VERS);
		audio_info->thread_exit = 0;
		audio_info->task = kthread_run(audio_voice_loopback_thread,
				audio_info,
				"audio_voice_loopback");
		if (IS_ERR(audio_info->task)) {
			MM_ERR("voice loopback thread create failed\n");
			rc = PTR_ERR(audio_info->task);
			audio_info->task = NULL;
			msm_rpc_close(audio_info->rpc_endpt);
			audio_info->rpc_endpt = NULL;
			rc = -EINVAL;
		}
		audio_info->rpc_prog = VOICELOOPBACK_PROG;
		audio_info->rpc_ver = VOICELOOP_VERS;
	}
done:
	return rc;
}

static int audio_voice_loopback_close(struct audio *audio_info)
{
	MM_DBG("\n");
	msm_rpc_close(audio_info->rpc_endpt);
	audio_info->rpc_endpt = NULL;
	audmgr_close(&audio_info->audmgr);
	audio_info->task = NULL;
	return 0;
}

static ssize_t audio_voice_loopback_debug_write(struct file *file,
				const char __user *buf,
				size_t cnt, loff_t *ppos)
{
	char lbuf[MAX_LEN];
	int rc = 0;

	if (cnt > (MAX_LEN - 1))
		return -EINVAL;

	memset(&lbuf[0], 0, sizeof(lbuf));

	rc = copy_from_user(lbuf, buf, cnt);
	if (rc) {
		MM_ERR("Unable to copy data from user space\n");
		return -EFAULT;
	}

	lbuf[cnt] = '\0';

	if (!strncmp(&lbuf[0], "1", cnt-1)) {
		mutex_lock(&the_audio.lock);
		if (!the_audio.enabled) {
			rc = audio_voice_loopback_open(&the_audio);
			if (!rc) {
				rc = audio_voice_loopback_start(&the_audio);
				if (rc < 0) {
					the_audio.enabled = 0;
					audio_voice_loopback_close(&the_audio);
				} else {
					the_audio.enabled = 1;
				}
			}
		}
		mutex_unlock(&the_audio.lock);
	} else if (!strncmp(lbuf, "0", cnt-1)) {
		mutex_lock(&the_audio.lock);
		if (the_audio.enabled) {
			audio_voice_loopback_stop(&the_audio);
			audio_voice_loopback_close(&the_audio);
			the_audio.enabled = 0;
		}
		mutex_unlock(&the_audio.lock);
	} else {
		rc = -EINVAL;
	}

	if (rc == 0) {
		rc = cnt;
	} else {
		MM_INFO("rc = %d\n", rc);
		MM_INFO("\nWrong command: Use =>\n");
		MM_INFO("-------------------------\n");
		MM_INFO("To Start Loopback:: echo \"1\">/sys/kernel/debug/\
			voice_loopback\n");
		MM_INFO("To Stop Loopback:: echo \"0\">/sys/kernel/debug/\
			voice_loopback\n");
		MM_INFO("------------------------\n");
	}

	return rc;
}

static ssize_t audio_voice_loopback_debug_open(struct inode *inode,
		struct file *file)
{
	file->private_data = inode->i_private;
	MM_DBG("Audio Voiceloop debugfs opened\n");
	return 0;
}

static const struct file_operations voice_loopback_debug_fops = {
	.write = audio_voice_loopback_debug_write,
	.open = audio_voice_loopback_debug_open,
};

static int __init audio_init(void)
{
	int rc = 0;
	memset(&the_audio, 0, sizeof(the_audio));

	mutex_init(&the_audio.lock);

	init_waitqueue_head(&the_audio.wait);

	the_audio.dentry = debugfs_create_file("voice_loopback",
			S_IFREG | S_IRUGO,
			NULL,
			NULL, &voice_loopback_debug_fops);
	if (IS_ERR(the_audio.dentry))
		MM_ERR("debugfs_create_file failed\n");

	return rc;
}
late_initcall(audio_init);
