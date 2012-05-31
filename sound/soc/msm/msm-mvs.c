/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * All source code in this file is licensed under the following license except
 * where indicated.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org.
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <mach/msm_rpcrouter.h>
#include <mach/debug_mm.h>
#include "msm_audio_mvs.h"


static struct audio_mvs_info_type audio_mvs_info;

static struct snd_pcm_hardware msm_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.rates			= (SNDRV_PCM_RATE_8000),
	.rate_min		= 8000,
	.rate_max		= 8000,
	.channels_min		= 1,
	.channels_max		= 2,
	.buffer_bytes_max	= MVS_MAX_VOC_PKT_SIZE * MVS_MAX_Q_LEN,
	.period_bytes_min	= MVS_MAX_VOC_PKT_SIZE,
	.period_bytes_max	= MVS_MAX_VOC_PKT_SIZE,
	.periods_min		= MVS_MAX_Q_LEN,
	.periods_max		= MVS_MAX_Q_LEN,
	.fifo_size		= 0,
};

static void snd_pcm_mvs_timer(unsigned long data)
{
	struct audio_mvs_info_type *audio = &audio_mvs_info;
	MM_DBG("%s\n", __func__);
	if (audio->playback_start) {
		if (audio->ack_dl_count) {
			audio->pcm_playback_irq_pos += audio->pcm_count;
			audio->ack_dl_count--;
			snd_pcm_period_elapsed(audio->playback_substream);
		}
	}

	if (audio->capture_start) {
		if (audio->ack_ul_count) {
			audio->pcm_capture_irq_pos += audio->pcm_capture_count;
			audio->ack_ul_count--;
			snd_pcm_period_elapsed(audio->capture_substream);
		}
	}
	audio->timer.expires +=  audio->expiry_delta;
	add_timer(&audio->timer);
}

static int audio_mvs_setup_mvs(struct audio_mvs_info_type *audio)
{
	int rc = 0;
	struct audio_mvs_enable_msg enable_msg;
	MM_DBG("%s\n", __func__);

	/* Enable MVS. */

	memset(&enable_msg, 0, sizeof(enable_msg));
	audio->rpc_status = RPC_STATUS_FAILURE;
	enable_msg.enable_args.client_id = cpu_to_be32(MVS_CLIENT_ID_VOIP);
	enable_msg.enable_args.mode = cpu_to_be32(MVS_MODE_LINEAR_PCM);
	enable_msg.enable_args.ul_cb_func_id = (int) NULL;
	enable_msg.enable_args.dl_cb_func_id = (int) NULL;
	enable_msg.enable_args.context = cpu_to_be32(MVS_PKT_CONTEXT_ISR);

	msm_rpc_setup_req(&enable_msg.rpc_hdr, MVS_PROG,
			MVS_VERS, MVS_ENABLE_PROC);

	rc = msm_rpc_write(audio->rpc_endpt,
				&enable_msg, sizeof(enable_msg));

	if (rc >= 0) {
		MM_DBG("RPC write for enable done\n");

		rc = wait_event_timeout(audio->wait,
					(audio->rpc_status !=
					 RPC_STATUS_FAILURE), 1 * HZ);

		if (rc > 0) {
			MM_DBG("Wait event for enable succeeded\n");

			mutex_lock(&audio->lock);
			audio->mvs_mode = MVS_MODE_LINEAR_PCM;
			audio->frame_mode = MVS_FRAME_MODE_PCM_DL;
			audio->pcm_frame = 0;
			mutex_unlock(&audio->lock);
			rc = 0;

		} else
			MM_ERR("Wait event for enable failed %d\n", rc);
	} else
		MM_ERR("RPC write for enable failed %d\n", rc);
	return rc;
}

static void audio_mvs_rpc_reply(struct msm_rpc_endpoint *endpoint,
					uint32_t xid)
{
	int rc = 0;
	struct rpc_reply_hdr reply_hdr;
	MM_DBG("%s\n", __func__);

	memset(&reply_hdr, 0, sizeof(reply_hdr));
	reply_hdr.xid = cpu_to_be32(xid);
	reply_hdr.type = cpu_to_be32(RPC_TYPE_REPLY);
	reply_hdr.reply_stat = cpu_to_be32(RPCMSG_REPLYSTAT_ACCEPTED);
	reply_hdr.data.acc_hdr.accept_stat =
	    cpu_to_be32(RPC_ACCEPTSTAT_SUCCESS);
	reply_hdr.data.acc_hdr.verf_flavor = 0;
	reply_hdr.data.acc_hdr.verf_length = 0;

	rc = msm_rpc_write(endpoint, &reply_hdr, sizeof(reply_hdr));

	if (rc < 0)
		MM_ERR("RPC write for response failed %d\n", rc);
}

static void audio_mvs_process_rpc_request(uint32_t procedure, uint32_t xid,
					  void *data, uint32_t length,
					  struct audio_mvs_info_type *audio)
{

	int rc = 0;
	uint32_t index;
	MM_DBG("%s\n", __func__);
	switch (procedure) {
	case MVS_EVENT_CB_TYPE_PROC:{
		struct audio_mvs_cb_func_args *args = data;
		uint32_t event_type = be32_to_cpu(args->event);
		uint32_t cmd_status =
			be32_to_cpu(args->
				event_data.mvs_ev_command_type.cmd_status);
		uint32_t mode_status =
			be32_to_cpu(args->
				event_data.mvs_ev_mode_type.mode_status);
		audio_mvs_rpc_reply(audio->rpc_endpt, xid);
		if (be32_to_cpu(args->valid_ptr)) {
			if (event_type == AUDIO_MVS_COMMAND) {
				if (cmd_status == AUDIO_MVS_CMD_SUCCESS)
					audio->rpc_status = RPC_STATUS_SUCCESS;
				wake_up(&audio->wait);
			} else if (event_type == AUDIO_MVS_MODE) {
				if (mode_status != AUDIO_MVS_MODE_NOT_AVAIL) {
					audio->rpc_status =
					    RPC_STATUS_SUCCESS;
				}
				audio->prepare_ack++;
				wake_up(&audio->wait);
				wake_up(&audio->prepare_wait);
			} else {
				/*nothing to do */
			}
		} else
			MM_ERR("ALSA: CB event pointer not valid\n");
		break;
	}
	case MVS_PACKET_UL_FN_TYPE_PROC:{
			uint32_t *cb_data = data;
			uint32_t pkt_len ;
			struct audio_mvs_ul_reply ul_reply;
			MM_DBG("MVS_PACKET_UL_FN_TYPE_PROC\n");

			memset(&ul_reply, 0, sizeof(ul_reply));
			cb_data++;
			pkt_len = be32_to_cpu(*cb_data);
			cb_data++;
			if (audio->capture_enable) {
				audio_mvs_info.ack_ul_count++;
				mutex_lock(&audio->out_lock);
				index = audio->out_write % MVS_MAX_Q_LEN;
				memcpy(audio->out[index].voc_pkt, cb_data,
							pkt_len);
				audio->out[index].len = pkt_len;
				audio->out_write++;
				mutex_unlock(&audio->out_lock);
			}
			MM_DBG(" audio->out_read = %d audio->out write = %d\n",
				audio->out_read, audio->out_write);
			ul_reply.reply_hdr.xid = cpu_to_be32(xid);
			ul_reply.reply_hdr.type = cpu_to_be32(RPC_TYPE_REPLY);
			ul_reply.reply_hdr.reply_stat =
				cpu_to_be32(RPCMSG_REPLYSTAT_ACCEPTED);
			ul_reply.reply_hdr.data.acc_hdr.accept_stat =
				cpu_to_be32(RPC_ACCEPTSTAT_SUCCESS);
			ul_reply.reply_hdr.data.acc_hdr.verf_flavor = 0;
			ul_reply.reply_hdr.data.acc_hdr.verf_length = 0;
			ul_reply.valid_pkt_status_ptr = cpu_to_be32(0x00000001);
			ul_reply.pkt_status = cpu_to_be32(0x00000000);
			rc = msm_rpc_write(audio->rpc_endpt, &ul_reply,
				sizeof(ul_reply));
			wake_up(&audio->out_wait);
			if (rc < 0)
				MM_ERR("RPC write for UL response failed %d\n",
				rc);
			break;
	}
	case MVS_PACKET_DL_FN_TYPE_PROC:{
			struct audio_mvs_dl_reply dl_reply;
			MM_DBG("MVS_PACKET_DL_FN_TYPE_PROC\n");
			memset(&dl_reply, 0, sizeof(dl_reply));
			dl_reply.reply_hdr.xid = cpu_to_be32(xid);
			dl_reply.reply_hdr.type = cpu_to_be32(RPC_TYPE_REPLY);
			dl_reply.reply_hdr.reply_stat =
				cpu_to_be32(RPCMSG_REPLYSTAT_ACCEPTED);
			dl_reply.reply_hdr.data.acc_hdr.accept_stat =
				cpu_to_be32(RPC_ACCEPTSTAT_SUCCESS);
			dl_reply.reply_hdr.data.acc_hdr.verf_flavor = 0;
			dl_reply.reply_hdr.data.acc_hdr.verf_length = 0;
			mutex_lock(&audio->in_lock);
			if (audio->in_read < audio->in_write
					&& audio->dl_play) {
				index = audio->in_read % MVS_MAX_Q_LEN;
				memcpy(&dl_reply.voc_pkt,
						audio->in[index].voc_pkt,
						audio->in[index].len);
				audio->in_read++;
				audio_mvs_info.ack_dl_count++;
				dl_reply.pkt_status =
					cpu_to_be32(AUDIO_MVS_PKT_NORMAL);
				wake_up(&audio->in_wait);
			} else {
				dl_reply.pkt_status =
					cpu_to_be32(AUDIO_MVS_PKT_SLOW);
			}
			mutex_unlock(&audio->in_lock);
			MM_DBG(" audio->in_read = %d audio->in write = %d\n",
					audio->in_read, audio->in_write);
			dl_reply.valid_frame_info_ptr = cpu_to_be32(0x00000001);
			dl_reply.frame_mode = cpu_to_be32(audio->frame_mode);
			dl_reply.frame_mode_again =
				cpu_to_be32(audio->frame_mode);
			dl_reply.frame_info_hdr.frame_mode =
				cpu_to_be32(audio->frame_mode);
			dl_reply.frame_info_hdr.mvs_mode =
				cpu_to_be32(audio->mvs_mode);
			dl_reply.frame_info_hdr.buf_free_cnt = 0;
			dl_reply.pcm_frame = cpu_to_be32(audio->pcm_frame);
			dl_reply.pcm_mode = cpu_to_be32(audio->pcm_mode);
			dl_reply.valid_pkt_status_ptr = cpu_to_be32(0x00000001);
			rc = msm_rpc_write(audio->rpc_endpt, &dl_reply,
					sizeof(dl_reply));
			if (rc < 0)
				MM_ERR("RPC write for DL response failed %d\n",
				rc);
			break;
	}
	default:
		MM_ERR("Unknown CB type %d\n", procedure);
	}
}

static int audio_mvs_thread(void *data)
{
	struct audio_mvs_info_type *audio =  &audio_mvs_info;
	struct rpc_request_hdr *rpc_hdr = NULL;
	struct rpc_reply_hdr *rpc_reply = NULL;
	uint32_t reply_status = 0;
	uint32_t rpc_type;
	int rpc_hdr_len;
	MM_DBG("%s\n", __func__);

	while (!kthread_should_stop()) {
		rpc_hdr_len =
		    msm_rpc_read(audio->rpc_endpt, (void **)&rpc_hdr, -1, -1);
		if (rpc_hdr_len < 0) {
			MM_ERR("RPC read failed %d\n", rpc_hdr_len);
			break;
		} else if (rpc_hdr_len < RPC_COMMON_HDR_SZ)
			continue;
		else {
			rpc_type = be32_to_cpu(rpc_hdr->type);
			if (rpc_type == RPC_TYPE_REPLY) {
				if (rpc_hdr_len < RPC_REPLY_HDR_SZ)
					continue;
				rpc_reply = (void *)rpc_hdr;
				reply_status = be32_to_cpu(rpc_reply->
							reply_stat);
				if (reply_status != RPCMSG_REPLYSTAT_ACCEPTED) {
					/* If the command is not accepted,
					 * there will be no response callback.
					 * Wake the caller and report error. */
					audio->rpc_status =  RPC_STATUS_REJECT;
					wake_up(&audio->wait);
					MM_ERR("RPC reply status denied\n");
				}
			} else if (rpc_type == RPC_TYPE_REQUEST) {
				if (rpc_hdr_len < RPC_REQUEST_HDR_SZ)
					continue;
				MM_DBG("ALSA: kthread call procedure\n");
				audio_mvs_process_rpc_request(
					be32_to_cpu(rpc_hdr->procedure),
					be32_to_cpu(rpc_hdr->xid),
					(void *)(rpc_hdr + 1),
					(rpc_hdr_len - sizeof(*rpc_hdr)),
					audio);
			} else
				MM_ERR("Unexpected RPC type %d\n", rpc_type);
		}
		kfree(rpc_hdr);
		rpc_hdr = NULL;
	}
	return 0;
}

static int msm_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{

	struct audio_mvs_info_type *audio = &audio_mvs_info;
	MM_DBG("%s\n", __func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		if (substream->stream ==  SNDRV_PCM_STREAM_PLAYBACK)
			audio->playback_start = 1;
		else
			audio->capture_start = 1;
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		if (substream->stream ==  SNDRV_PCM_STREAM_PLAYBACK)
			audio->playback_start = 0;
		else
			audio->capture_start = 0;
		break;
	default:
		break;
	}
	return 0;
}

static int msm_pcm_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_mvs_info_type *audio = &audio_mvs_info;

	MM_DBG("%s\n", __func__);
	mutex_lock(&audio->lock);
	if (audio->state < AUDIO_MVS_OPENED) {
		audio->rpc_endpt =
			msm_rpc_connect_compatible(MVS_PROG,
					MVS_VERS,
					MSM_RPC_UNINTERRUPTIBLE);
		audio->state = AUDIO_MVS_OPENED;
	}

	if (IS_ERR(audio->rpc_endpt)) {
		MM_ERR("ALSA MVS RPC connect failed with version 0x%x\n",
			MVS_VERS);
		ret = PTR_ERR(audio->rpc_endpt);
		audio->rpc_endpt = NULL;
		goto err;
	} else {
		MM_DBG("ALSA MVS RPC connect succeeded\n");
		if (audio->playback_substream == NULL ||
			audio->capture_substream == NULL) {
				if (substream->stream ==
					SNDRV_PCM_STREAM_PLAYBACK) {
					audio->playback_substream =
						substream;
					runtime->hw = msm_pcm_hardware;
				} else if (substream->stream ==
					SNDRV_PCM_STREAM_CAPTURE) {
					audio->capture_substream =
						substream;
					runtime->hw = msm_pcm_hardware;
				}
		} else {
			ret  = -EPERM;
			goto err;
		}
		ret = snd_pcm_hw_constraint_integer(runtime,
				SNDRV_PCM_HW_PARAM_PERIODS);
		if (ret < 0) {
			MM_ERR("snd_pcm_hw_constraint_integer failed\n");
			if (!audio->instance) {
				msm_rpc_close(audio->rpc_endpt);
				audio->rpc_endpt = NULL;
			}
			goto err;
		}
			audio->instance++;
	}
err:
	mutex_unlock(&audio->lock);
	return ret;
}

static int msm_pcm_playback_copy(struct snd_pcm_substream *substream, int a,
				 snd_pcm_uframes_t hwoff, void __user *buf,
				 snd_pcm_uframes_t frames)
{
	int rc = 0;
	int count = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_mvs_info_type *audio = &audio_mvs_info;
	uint32_t index;
	MM_DBG("%s\n", __func__);
	if (audio->dl_play  == 1) {
		rc = wait_event_interruptible_timeout(audio->in_wait,
			(audio->in_write - audio->in_read  <= 3),
			100 * HZ);
		if (!rc) {
			MM_ERR("MVS: write time out\n");
			return -ETIMEDOUT;
		} else if (rc < 0) {
			MM_ERR("MVS: write was interrupted\n");
			return  -ERESTARTSYS;
		}
	}
	mutex_lock(&audio->in_lock);
	if (audio->state == AUDIO_MVS_ENABLED) {
		index = audio->in_write % MVS_MAX_Q_LEN;
		count = frames_to_bytes(runtime, frames);
		if (count <= MVS_MAX_VOC_PKT_SIZE) {
			rc = copy_from_user(audio->in[index].voc_pkt, buf,
						 count);
		 } else
			rc = -ENOMEM;
		if (!rc) {
			audio->in[index].len = count;
			audio->in_write++;
			rc = count;
			if (audio->in_write >= 3)
				audio->dl_play  = 1;
		} else {
			MM_ERR("Copy from user returned %d\n", rc);
			rc = -EFAULT;
		}

	} else {
		MM_ERR("Write performed in invalid state %d\n",
					audio->state);
		rc = -EINVAL;
	}
	mutex_unlock(&audio->in_lock);
	return rc;
}

static int msm_pcm_capture_copy(struct snd_pcm_substream *substream,
			int channel, snd_pcm_uframes_t hwoff,
			void __user *buf, snd_pcm_uframes_t frames)
{
	int rc = 0;
	int count = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_mvs_info_type *audio = &audio_mvs_info;
	uint32_t index = 0;

	MM_DBG("%s\n", __func__);

	/* Ensure the driver has been enabled. */
	if (audio->state != AUDIO_MVS_ENABLED) {
		MM_ERR("Read performed in invalid state %d\n", audio->state);
		return -EPERM;
	}
	rc = wait_event_interruptible_timeout(audio->out_wait,
		(audio->out_read < audio->out_write ||
		audio->state == AUDIO_MVS_CLOSING ||
		audio->state == AUDIO_MVS_CLOSED),
		100 * HZ);
	if (!rc) {
		MM_ERR("MVS: No UL data available\n");
		return -ETIMEDOUT;
	} else if (rc < 0) {
		MM_ERR("MVS: Read was interrupted\n");
		return  -ERESTARTSYS;
	}

	mutex_lock(&audio->out_lock);
	if (audio->state  == AUDIO_MVS_CLOSING
		|| audio->state == AUDIO_MVS_CLOSED) {
		rc = -EBUSY;
	} else {
		count = frames_to_bytes(runtime, frames);
		index = audio->out_read % MVS_MAX_Q_LEN;
		if (audio->out[index].len <= count) {
				rc = copy_to_user(buf,
				audio->out[index].voc_pkt,
				audio->out[index].len);
				if (rc == 0) {
					rc = audio->out[index].len;
					audio->out_read++;
				} else {
					MM_ERR("Copy to user %d\n", rc);
					rc = -EFAULT;
				}
		} else
			rc = -ENOMEM;
	}
	mutex_unlock(&audio->out_lock);
	return rc;
}

static int msm_pcm_copy(struct snd_pcm_substream *substream, int a,
			snd_pcm_uframes_t hwoff, void __user *buf,
			snd_pcm_uframes_t frames)
{
	int ret = 0;
	MM_DBG("%s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_copy(substream, a, hwoff, buf, frames);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_pcm_capture_copy(substream, a, hwoff, buf, frames);
	return ret;
}

static int msm_pcm_close(struct snd_pcm_substream *substream)
{
	int rc = 0;
	struct audio_mvs_info_type *audio = &audio_mvs_info;
	struct audio_mvs_release_msg release_msg;
	MM_DBG("%s\n", __func__);
	memset(&release_msg, 0, sizeof(release_msg));
	mutex_lock(&audio->lock);

	audio->instance--;
	wake_up(&audio->out_wait);

	if (!audio->instance) {
		if (audio->state == AUDIO_MVS_ENABLED) {
			audio->state = AUDIO_MVS_CLOSING;
			/* Release MVS. */
			release_msg.client_id = cpu_to_be32(MVS_CLIENT_ID_VOIP);
			msm_rpc_setup_req(&release_msg.rpc_hdr, audio->rpc_prog,
						 audio->rpc_ver,
						 MVS_RELEASE_PROC);
			audio->rpc_status = RPC_STATUS_FAILURE;
			rc = msm_rpc_write(audio->rpc_endpt, &release_msg,
					sizeof(release_msg));
			if (rc >= 0) {
				MM_DBG("RPC write for release done\n");
				rc = wait_event_timeout(audio->wait,
						(audio->rpc_status !=
						 RPC_STATUS_FAILURE), 1 * HZ);
				if (rc != 0) {
					MM_DBG
					("Wait event for release succeeded\n");
					rc = 0;
					kthread_stop(audio->task);
					audio->prepare_ack = 0;
					audio->task = NULL;
					del_timer_sync(&audio->timer);
				} else {
					MM_ERR
					("Wait event for release failed %d\n",
						 rc);
				}
			} else	{
				MM_ERR("RPC write for release failed %d\n", rc);
			}
		}
		audio->state = AUDIO_MVS_CLOSED;
		msm_rpc_close(audio->rpc_endpt);
		audio->rpc_endpt = NULL;
	}

	mutex_unlock(&audio->lock);

		wake_unlock(&audio->suspend_lock);
		pm_qos_update_request(&audio->pm_qos_req, PM_QOS_DEFAULT_VALUE);
		/* Release the IO buffers. */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mutex_lock(&audio->in_lock);
		audio->in_write = 0;
		audio->in_read = 0;
		audio->playback_enable = 0;
		audio->dl_play  = 0;
		audio->ack_dl_count = 0;
		memset(audio->in[0].voc_pkt, 0,
			 MVS_MAX_VOC_PKT_SIZE * MVS_MAX_Q_LEN);
		audio->in->len = 0;
		audio->playback_substream = NULL;
		mutex_unlock(&audio->in_lock);
	} else {
		mutex_lock(&audio->out_lock);
		audio->out_write = 0;
		audio->out_read = 0;
		audio->capture_enable = 0;
		audio->ack_ul_count = 0;
		memset(audio->out[0].voc_pkt, 0,
			 MVS_MAX_VOC_PKT_SIZE * MVS_MAX_Q_LEN);
		audio->out->len = 0;
		audio->capture_substream = NULL;
		mutex_unlock(&audio->out_lock);
	}
	return rc;
}

static int msm_mvs_pcm_setup(struct snd_pcm_substream *substream)
{
	int rc = 0;
	struct audio_mvs_acquire_msg acquire_msg;
	struct audio_mvs_info_type *audio = &audio_mvs_info;
	memset(&acquire_msg, 0, sizeof(acquire_msg));

	/*Create an Kthread */
	MM_DBG("ALSA MVS thread creating\n");
	if (!IS_ERR(audio->rpc_endpt)) {
		audio->task =
		    kthread_run(audio_mvs_thread, audio,
				"audio_alsa_mvs_thread");
		if (!IS_ERR(audio->task)) {
			MM_DBG("ALSA MVS thread create succeeded\n");
			audio->rpc_prog = MVS_PROG;
			audio->rpc_ver = MVS_VERS;
			/* Acquire MVS. */
			acquire_msg.acquire_args.client_id =
			    cpu_to_be32(MVS_CLIENT_ID_VOIP);
			acquire_msg.acquire_args.cb_func_id =
			    cpu_to_be32(MVS_CB_FUNC_ID);
			msm_rpc_setup_req(&acquire_msg.rpc_hdr,
					  audio->rpc_prog,
					  audio->rpc_ver,
					  MVS_ACQUIRE_PROC);
			audio->rpc_status = RPC_STATUS_FAILURE;
			rc = msm_rpc_write(audio->rpc_endpt,
					   &acquire_msg, sizeof(acquire_msg));
			if (rc >= 0) {
				MM_DBG("RPC write for acquire done\n");

				rc = wait_event_timeout(audio->wait,
							(audio->rpc_status !=
							 RPC_STATUS_FAILURE),
							1 * HZ);
				if (rc != 0) {
					audio->state =
						AUDIO_MVS_ACQUIRE;
					rc = 0;
					MM_DBG
					    ("MVS driver in acquire state\n");
				} else {
					MM_ERR
					    ("acquire Wait event failed %d\n",
						rc);
					rc = -EBUSY;
				}
			} else {
				MM_ERR("RPC write for acquire failed %d\n",
				       rc);
				rc = -EBUSY;
			}
		} else {
			MM_ERR("ALSA MVS thread create failed\n");
			rc = PTR_ERR(audio->task);
			audio->task = NULL;
			msm_rpc_close(audio->rpc_endpt);
			audio->rpc_endpt = NULL;
		}
	} else {
		MM_ERR("RPC connect is not setup with version 0x%x\n",
			MVS_VERS);
		rc = PTR_ERR(audio->rpc_endpt);
		audio->rpc_endpt = NULL;
	}
	/*mvs mode setup */
	if (audio->state == AUDIO_MVS_ACQUIRE)
		rc =  audio_mvs_setup_mvs(audio);
	else
		rc = -EBUSY;
	return rc;
}

static int msm_pcm_playback_prepare(struct snd_pcm_substream *substream)
{
	struct  audio_mvs_info_type *prtd = &audio_mvs_info;
	MM_DBG("%s\n", __func__);
	prtd->pcm_playback_irq_pos = 0;
	prtd->pcm_playback_buf_pos = 0;
	/* rate and channels are sent to audio driver */
	prtd->playback_enable = 1;
	return 0;
}

static int msm_pcm_capture_prepare(struct snd_pcm_substream *substream)
{
	struct  audio_mvs_info_type *prtd = &audio_mvs_info;
	prtd->pcm_capture_size  = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_capture_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_capture_irq_pos = 0;
	prtd->pcm_capture_buf_pos = 0;
	prtd->capture_enable = 1;
	return 0;
}


static int msm_pcm_prepare(struct snd_pcm_substream *substream)
{
	int rc = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_mvs_info_type *prtd = &audio_mvs_info;
	unsigned long expiry = 0;
	MM_DBG("%s\n", __func__);
	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);

	mutex_lock(&prtd->prepare_lock);
	if (prtd->state == AUDIO_MVS_ENABLED)
		goto enabled;
	else if (prtd->state == AUDIO_MVS_PREPARING)
		goto prepairing;
	else if (prtd->state == AUDIO_MVS_OPENED) {
		prtd->state = AUDIO_MVS_PREPARING;
		rc = msm_mvs_pcm_setup(substream);
	}
	if (!rc) {
		expiry = ((unsigned long)((prtd->pcm_count * 1000)
			/(runtime->rate * runtime->channels * 2)));
		expiry -= (expiry % 10);
		prtd->timer.expires = jiffies + (msecs_to_jiffies(expiry));
		prtd->expiry_delta = (msecs_to_jiffies(expiry));
		if (prtd->expiry_delta <= 2)
			prtd->expiry_delta = 1;
		setup_timer(&prtd->timer, snd_pcm_mvs_timer,
				 (unsigned long)prtd);
		prtd->ack_ul_count = 0;
		prtd->ack_dl_count = 0;
		add_timer(&prtd->timer);

	} else {
		MM_ERR("ALSA MVS setup is not done");
		rc =  -EPERM;
		prtd->state = AUDIO_MVS_OPENED;
		goto err;
	}

prepairing:
	rc = wait_event_interruptible(prtd->prepare_wait,
			(prtd->prepare_ack == 2));
	if (rc < 0) {
		MM_ERR("Wait event for prepare faild  rc  %d", rc);
		rc = -EINTR;
		prtd->state = AUDIO_MVS_OPENED;
		goto err;
	} else
		MM_DBG("Wait event for prepare succeeded\n");

	prtd->state = AUDIO_MVS_ENABLED;
enabled:
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		rc = msm_pcm_playback_prepare(substream);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		rc =  msm_pcm_capture_prepare(substream);
err:
	mutex_unlock(&prtd->prepare_lock);
	return rc;
}

int msm_mvs_pcm_hw_params(struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	MM_DBG("%s\n", __func__);
	if (substream->pcm->device & 1) {
		runtime->hw.info &= ~SNDRV_PCM_INFO_INTERLEAVED;
		runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;
	}
	return 0;
}

static snd_pcm_uframes_t
msm_pcm_playback_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_mvs_info_type *audio = &audio_mvs_info;

	if (audio->pcm_playback_irq_pos >= audio->pcm_size)
		audio->pcm_playback_irq_pos = 0;
	return bytes_to_frames(runtime, (audio->pcm_playback_irq_pos));
}

static snd_pcm_uframes_t
msm_pcm_capture_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_mvs_info_type *audio = &audio_mvs_info;

	if (audio->pcm_capture_irq_pos >= audio->pcm_capture_size)
		audio->pcm_capture_irq_pos = 0;
	return bytes_to_frames(runtime, (audio->pcm_capture_irq_pos));
}

static snd_pcm_uframes_t msm_pcm_pointer(struct snd_pcm_substream *substream)
{
	snd_pcm_uframes_t ret = 0;
	MM_DBG("%s\n", __func__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_pointer(substream);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_pcm_capture_pointer(substream);
	return ret;
}

static struct snd_pcm_ops msm_mvs_pcm_ops = {
	.open = msm_pcm_open,
	.copy = msm_pcm_copy,
	.hw_params = msm_mvs_pcm_hw_params,
	.close = msm_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.prepare = msm_pcm_prepare,
	.trigger = msm_pcm_trigger,
	.pointer = msm_pcm_pointer,

};

static int msm_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	int   i, ret, offset = 0;
	struct snd_pcm *pcm = rtd->pcm;

	audio_mvs_info.mem_chunk = kmalloc(
		2 * MVS_MAX_VOC_PKT_SIZE * MVS_MAX_Q_LEN, GFP_KERNEL);
	if (audio_mvs_info.mem_chunk != NULL) {
		audio_mvs_info.in_read = 0;
		audio_mvs_info.in_write = 0;
		audio_mvs_info.out_read = 0;
		audio_mvs_info.out_write = 0;
		for (i = 0; i < MVS_MAX_Q_LEN; i++) {
			audio_mvs_info.in[i].voc_pkt =
			audio_mvs_info.mem_chunk + offset;
			offset = offset + MVS_MAX_VOC_PKT_SIZE;
		}
		for (i = 0; i < MVS_MAX_Q_LEN; i++) {
			audio_mvs_info.out[i].voc_pkt =
				audio_mvs_info.mem_chunk + offset;
			offset = offset + MVS_MAX_VOC_PKT_SIZE;
		}
		audio_mvs_info.playback_substream = NULL;
		audio_mvs_info.capture_substream = NULL;
	} else {
		MM_ERR("MSM MVS kmalloc failed\n");
		return -ENODEV;
	}


	ret = snd_pcm_new_stream(pcm, SNDRV_PCM_STREAM_PLAYBACK, 1);
	if (ret)
		return ret;
	ret = snd_pcm_new_stream(pcm, SNDRV_PCM_STREAM_CAPTURE, 1);
	if (ret)
		return ret;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &msm_mvs_pcm_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &msm_mvs_pcm_ops);

	return 0;
}

struct snd_soc_platform_driver msm_mvs_soc_platform = {
	.ops		= &msm_mvs_pcm_ops,
	.pcm_new	= msm_pcm_new,
};
EXPORT_SYMBOL(msm_mvs_soc_platform);

static __devinit int msm_pcm_probe(struct platform_device *pdev)
{
	return snd_soc_register_platform(&pdev->dev,
				&msm_mvs_soc_platform);
}

static int msm_pcm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver msm_pcm_driver = {
	.driver = {
		.name = "msm-mvs-audio",
		.owner = THIS_MODULE,
	},
	.probe = msm_pcm_probe,
	.remove = __devexit_p(msm_pcm_remove),
};

static int __init msm_mvs_soc_platform_init(void)
{
	memset(&audio_mvs_info, 0, sizeof(audio_mvs_info));
	mutex_init(&audio_mvs_info.lock);
	mutex_init(&audio_mvs_info.prepare_lock);
	mutex_init(&audio_mvs_info.in_lock);
	mutex_init(&audio_mvs_info.out_lock);
	init_waitqueue_head(&audio_mvs_info.wait);
	init_waitqueue_head(&audio_mvs_info.prepare_wait);
	init_waitqueue_head(&audio_mvs_info.out_wait);
	init_waitqueue_head(&audio_mvs_info.in_wait);
	wake_lock_init(&audio_mvs_info.suspend_lock, WAKE_LOCK_SUSPEND,
				"audio_mvs_suspend");
	pm_qos_add_request(&audio_mvs_info.pm_qos_req, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);
	return platform_driver_register(&msm_pcm_driver);
}
module_init(msm_mvs_soc_platform_init);

static void __exit msm_mvs_soc_platform_exit(void)
{
	 platform_driver_unregister(&msm_pcm_driver);
}
module_exit(msm_mvs_soc_platform_exit);

MODULE_DESCRIPTION("MVS PCM module platform driver");
MODULE_LICENSE("GPL v2");
