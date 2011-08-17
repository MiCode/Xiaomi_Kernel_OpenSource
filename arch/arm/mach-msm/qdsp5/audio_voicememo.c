/* arch/arm/mach-msm/qdsp5/audio_voicememo.c
 *
 * Voice Memo device
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
 *
 * This code is based in part on arch/arm/mach-msm/qdsp5/audio_mp3.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org.
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/msm_audio_voicememo.h>
#include <linux/slab.h>

#include <asm/atomic.h>
#include <asm/ioctls.h>
#include <mach/msm_rpcrouter.h>
#include <mach/debug_mm.h>

#include "audmgr.h"

#define SND_PROG_VERS "rs30000002:0x00020001"
#define SND_PROG 0x30000002
#define SND_VERS_COMP 0x00020001
#define SND_VERS2_COMP 0x00030001

#define SND_VOC_REC_START_PROC                  19
#define SND_VOC_REC_STOP_PROC                   20
#define SND_VOC_REC_PAUSE_PROC			21
#define SND_VOC_REC_RESUME_PROC                 22
#define SND_VOC_REC_PUT_BUF_PROC                23

#define SND_VOC_REC_AV_SYNC_CB_PTR_PROC 	9
#define SND_VOC_REC_CB_FUNC_TYPE_PROC 		10

#define REC_CLIENT_DATA		0x11223344
#define DATA_CB_FUNC_ID		0x12345678
#define AV_SYNC_CB_FUNC_ID	0x87654321
#define CLIENT_DATA		0xaabbccdd

#define RPC_TYPE_REQUEST 0
#define RPC_TYPE_REPLY 1

#define RPC_STATUS_FAILURE 0
#define RPC_STATUS_SUCCESS 1

#define RPC_VERSION 2

#define RPC_COMMON_HDR_SZ  (sizeof(uint32_t) * 2)
#define RPC_REQUEST_HDR_SZ (sizeof(struct rpc_request_hdr))
#define RPC_REPLY_HDR_SZ   (sizeof(uint32_t) * 3)
#define RPC_REPLY_SZ       (sizeof(uint32_t) * 6)

#define MAX_FRAME_SIZE 36 /* QCELP - 36, AMRNB - 32, EVRC - 24 */
#define MAX_REC_BUF_COUNT 5 /* Maximum supported voc rec buffers */
#define MAX_REC_BUF_SIZE (MAX_FRAME_SIZE * 10)
#define MAX_VOICEMEMO_BUF_SIZE  \
	((MAX_REC_BUF_SIZE)*MAX_REC_BUF_COUNT) /* 5 buffers for 200ms frame */
#define MSM_AUD_BUFFER_UPDATE_WAIT_MS 2000

enum rpc_voc_rec_status_type {
	RPC_VOC_REC_STAT_SUCCESS = 1,
	RPC_VOC_REC_STAT_DONE = 2,
	RPC_VOC_REC_STAT_AUTO_STOP = 4,
	RPC_VOC_REC_STAT_PAUSED = 8,
	RPC_VOC_REC_STAT_RESUMED = 16,
	RPC_VOC_REC_STAT_ERROR = 32,
	RPC_VOC_REC_STAT_BUFFER_ERROR = 64,
	RPC_VOC_REC_STAT_INVALID_PARAM = 128,
	RPC_VOC_REC_STAT_INT_TIME = 256,
	RPC_VOC_REC_STAT_DATA = 512,
	RPC_VOC_REC_STAT_NOT_READY = 1024,
	RPC_VOC_REC_STAT_INFORM_EVRC = 2048,
	RPC_VOC_REC_STAT_INFORM_13K = 4096,
	RPC_VOC_REC_STAT_INFORM_AMR = 8192,
	RPC_VOC_REC_STAT_INFORM_MAX = 65535
};

struct rpc_snd_voc_rec_start_args {
	uint32_t param_status; /* 1 = valid, 0 = not valid */
	uint32_t rec_type;
	uint32_t rec_interval_ms;
	uint32_t auto_stop_ms;
	uint32_t capability;
	uint32_t max_rate;
	uint32_t min_rate;
	uint32_t frame_format;
	uint32_t dtx_enable;
	uint32_t data_req_ms;
	uint32_t rec_client_data;

	uint32_t cb_func_id;
	uint32_t sync_cb_func_id;
	uint32_t client_data;
};

struct rpc_snd_voc_rec_put_buf_args {
	uint32_t buf;
	uint32_t num_bytes;
};

struct snd_voc_rec_start_msg {
	struct rpc_request_hdr hdr;
	struct rpc_snd_voc_rec_start_args args;
};

struct snd_voc_rec_put_buf_msg {
	struct rpc_request_hdr hdr;
	struct rpc_snd_voc_rec_put_buf_args args;
};

struct snd_voc_rec_av_sync_cb_func_data {
	uint32_t sync_cb_func_id;
	uint32_t status;  /* Pointer status (1 = valid, 0  = invalid) */
	uint32_t num_samples;
	uint32_t time_stamp[2];
	uint32_t lost_samples;
	uint32_t frame_index;
	uint32_t client_data;
};

struct snd_voc_rec_cb_func_fw_data {
	uint32_t fw_ptr_status; /* FW Pointer status (1=valid,0=invalid) */
	uint32_t rec_buffer_size;
	uint32_t data[MAX_REC_BUF_SIZE/4];
	uint32_t rec_buffer_size_copy;
	uint32_t rec_num_frames; /* Number of voice frames */
	uint32_t rec_length; /* Valid data in record buffer =
			      * data_req_ms amount of data */
	uint32_t client_data; /* A11 rec buffer pointer */
	uint32_t rw_ptr_status; /* RW Pointer status (1=valid,0=invalid) */
};

struct snd_voc_rec_cb_func_rw_data {
	uint32_t fw_ptr_status; /* FW Pointer status (1=valid,0=invalid) */
	uint32_t rw_ptr_status; /* RW Pointer status (1=valid,0=invalid) */
	uint32_t rec_buffer_size;
	uint32_t data[MAX_REC_BUF_SIZE/4];
	uint32_t rec_buffer_size_copy;
	uint32_t rec_num_frames; /* Number of voice frames */
	uint32_t rec_length; /* Valid data in record buffer =
			      * data_req_ms amount of data */
	uint32_t client_data; /* A11 rec buffer pointer */
};

struct snd_voc_rec_data_cb_func_data {
	uint32_t cb_func_id;
	uint32_t status; /* Pointer status (1 = valid, 0  = invalid) */
	uint32_t rec_status;

	union {
		struct snd_voc_rec_cb_func_fw_data fw_data;
		struct snd_voc_rec_cb_func_rw_data rw_data;
	} pkt;
};

struct buffer {
	void *data;
	unsigned size;
	unsigned used; /* Usage actual recorded data */
	unsigned addr;
	unsigned numframes;
};

struct audio_voicememo {
	uint32_t byte_count; /* Pass statistics to user space for
			      * time stamping */
	uint32_t frame_count;

	int opened;
	int enabled;
	int running;
	int stopped;
	int pause_resume;

	uint32_t rpc_prog;
	uint32_t rpc_ver;
	uint32_t rpc_xid;
	uint32_t rpc_status;

	struct mutex lock;
	struct mutex read_lock;
	struct mutex dsp_lock;
	wait_queue_head_t read_wait;
	wait_queue_head_t wait;

	struct buffer in[MAX_REC_BUF_COUNT];
	char *rec_buf_ptr;
	dma_addr_t phys;
	uint32_t rec_buf_size;
	uint8_t read_next;	/* index to input buffers to be read next */
	uint8_t fill_next;	/* index to buffer that should be filled as
				 * data comes from A9 */

	struct audmgr audmgr;

	struct msm_audio_voicememo_config voicememo_cfg;

	struct msm_rpc_endpoint *sndept;
	struct task_struct *task;
};

static struct audio_voicememo the_audio_voicememo;

static int audvoicememo_validate_usr_config(
		struct msm_audio_voicememo_config *config)
{
	int rc = -1; /* error */

	if (config->rec_type != RPC_VOC_REC_FORWARD &&
		config->rec_type != RPC_VOC_REC_REVERSE &&
		config->rec_type != RPC_VOC_REC_BOTH)
		goto done;

	/* QCELP, EVRC, AMR-NB only */
	if (config->capability != RPC_VOC_CAP_IS733 &&
		config->capability != RPC_VOC_CAP_IS127 &&
		config->capability != RPC_VOC_CAP_AMR)
		goto done;

	/* QCP, AMR format supported */
	if ((config->frame_format != RPC_VOC_PB_NATIVE_QCP) &&
		(config->frame_format != RPC_VOC_PB_AMR))
		goto done;

	if ((config->frame_format == RPC_VOC_PB_AMR) &&
		(config->capability != RPC_VOC_CAP_AMR))
		goto done;

	/* To make sure, max kernel buf size matches
	 * with max data request time */
	if (config->data_req_ms > ((MAX_REC_BUF_SIZE/MAX_FRAME_SIZE)*20))
		goto done;

	rc = 0;
done:
	return rc;
}

static void audvoicememo_flush_buf(struct audio_voicememo *audio)
{
	uint8_t index;

	for (index = 0; index < MAX_REC_BUF_COUNT; index++)
		audio->in[index].used = 0;

	audio->read_next = 0;
	audio->fill_next = 0;
}

static void audvoicememo_ioport_reset(struct audio_voicememo *audio)
{
	/* Make sure read/write thread are free from
	 * sleep and knowing that system is not able
	 * to process io request at the moment
	 */
	wake_up(&audio->read_wait);
	mutex_lock(&audio->read_lock);
	audvoicememo_flush_buf(audio);
	mutex_unlock(&audio->read_lock);
}

/* must be called with audio->lock held */
static int audvoicememo_enable(struct audio_voicememo *audio)
{
	struct audmgr_config cfg;
	struct snd_voc_rec_put_buf_msg bmsg;
	struct snd_voc_rec_start_msg msg;
	uint8_t index;
	uint32_t offset = 0;
	int rc;

	if (audio->enabled)
		return 0;

	/* Codec / method configure to audmgr client */
	cfg.tx_rate = RPC_AUD_DEF_SAMPLE_RATE_8000;
	cfg.rx_rate = RPC_AUD_DEF_SAMPLE_RATE_NONE;
	cfg.def_method = RPC_AUD_DEF_METHOD_RECORD;

	if (audio->voicememo_cfg.capability == RPC_VOC_CAP_IS733)
		cfg.codec = RPC_AUD_DEF_CODEC_VOC_13K;
	else if (audio->voicememo_cfg.capability == RPC_VOC_CAP_IS127)
		cfg.codec = RPC_AUD_DEF_CODEC_VOC_EVRC;
	else
		cfg.codec = RPC_AUD_DEF_CODEC_VOC_AMR; /* RPC_VOC_CAP_AMR */

	cfg.snd_method = RPC_SND_METHOD_VOICE;
	rc = audmgr_enable(&audio->audmgr, &cfg);

	if (rc < 0)
		return rc;

	/* Configure VOC Rec buffer */
	for (index = 0; index < MAX_REC_BUF_COUNT; index++) {
		audio->in[index].data = audio->rec_buf_ptr + offset;
		audio->in[index].addr = audio->phys + offset;
		audio->in[index].size = audio->rec_buf_size;
		audio->in[index].used = 0;
		audio->in[index].numframes = 0;
		offset += audio->rec_buf_size;
		bmsg.args.buf = (uint32_t) audio->in[index].data;
		bmsg.args.num_bytes = cpu_to_be32(audio->in[index].size);
		MM_DBG("rec_buf_ptr=0x%8x, rec_buf_size = 0x%8x\n",
				bmsg.args.buf, bmsg.args.num_bytes);

		msm_rpc_setup_req(&bmsg.hdr, audio->rpc_prog, audio->rpc_ver,
				SND_VOC_REC_PUT_BUF_PROC);
		audio->rpc_xid = bmsg.hdr.xid;
		audio->rpc_status = RPC_STATUS_FAILURE;
		msm_rpc_write(audio->sndept, &bmsg, sizeof(bmsg));
		rc = wait_event_timeout(audio->wait,
			audio->rpc_status != RPC_STATUS_FAILURE, 1 * HZ);
		if (rc == 0)
			goto err;
	}


	/* Start Recording */
	msg.args.param_status = cpu_to_be32(0x00000001);
	msg.args.rec_type = cpu_to_be32(audio->voicememo_cfg.rec_type);
	msg.args.rec_interval_ms =
		cpu_to_be32(audio->voicememo_cfg.rec_interval_ms);
	msg.args.auto_stop_ms = cpu_to_be32(audio->voicememo_cfg.auto_stop_ms);
	msg.args.capability = cpu_to_be32(audio->voicememo_cfg.capability);
	msg.args.max_rate = cpu_to_be32(audio->voicememo_cfg.max_rate);
	msg.args.min_rate = cpu_to_be32(audio->voicememo_cfg.min_rate);
	msg.args.frame_format = cpu_to_be32(audio->voicememo_cfg.frame_format);
	msg.args.dtx_enable = cpu_to_be32(audio->voicememo_cfg.dtx_enable);
	msg.args.data_req_ms = cpu_to_be32(audio->voicememo_cfg.data_req_ms);
	msg.args.rec_client_data = cpu_to_be32(REC_CLIENT_DATA);
	msg.args.cb_func_id = cpu_to_be32(DATA_CB_FUNC_ID);
	msg.args.sync_cb_func_id = cpu_to_be32(AV_SYNC_CB_FUNC_ID);
	msg.args.client_data = cpu_to_be32(CLIENT_DATA);

	msm_rpc_setup_req(&msg.hdr, audio->rpc_prog, audio->rpc_ver,
			SND_VOC_REC_START_PROC);

	audio->rpc_xid = msg.hdr.xid;
	audio->rpc_status = RPC_STATUS_FAILURE;
	msm_rpc_write(audio->sndept, &msg, sizeof(msg));
	rc = wait_event_timeout(audio->wait,
		audio->rpc_status != RPC_STATUS_FAILURE, 1 * HZ);
	if (rc == 0)
		goto err;

	audio->rpc_xid = 0;
	audio->enabled = 1;
	return 0;

err:
	audio->rpc_xid = 0;
	audmgr_disable(&audio->audmgr);
	MM_ERR("Fail\n");
	return -1;
}

/* must be called with audio->lock held */
static int audvoicememo_disable(struct audio_voicememo *audio)
{
	struct rpc_request_hdr rhdr;
	int rc = 0;
	if (audio->enabled) {
		msm_rpc_setup_req(&rhdr, audio->rpc_prog, audio->rpc_ver,
				SND_VOC_REC_STOP_PROC);
		rc = msm_rpc_write(audio->sndept, &rhdr, sizeof(rhdr));
		wait_event_timeout(audio->wait, audio->stopped == 0,
				1 * HZ);
		wake_up(&audio->read_wait);
		audmgr_disable(&audio->audmgr);
		audio->enabled = 0;
	}
	return 0;
}

/* RPC Reply Generator */
static void rpc_reply(struct msm_rpc_endpoint *ept, uint32_t xid)
{
	int rc = 0;
	uint8_t reply_buf[sizeof(struct rpc_reply_hdr)];
	struct rpc_reply_hdr *reply = (struct rpc_reply_hdr *)reply_buf;

	MM_DBG("inside\n");
	reply->xid = cpu_to_be32(xid);
	reply->type = cpu_to_be32(RPC_TYPE_REPLY); /* reply */
	reply->reply_stat = cpu_to_be32(RPCMSG_REPLYSTAT_ACCEPTED);

	reply->data.acc_hdr.accept_stat = cpu_to_be32(RPC_ACCEPTSTAT_SUCCESS);
	reply->data.acc_hdr.verf_flavor = 0;
	reply->data.acc_hdr.verf_length = 0;

	rc = msm_rpc_write(ept, reply_buf, sizeof(reply_buf));
	if (rc < 0)
		MM_ERR("could not write RPC response: %d\n", rc);
}

static void process_rpc_request(uint32_t proc, uint32_t xid,
		void *data, int len, void *private)
{
	struct audio_voicememo *audio = private;

	MM_DBG("inside\n");
	/* Sending Ack before processing the request
	 * to make sure A9 get response immediate
	 * However, if there is validation of request planned
	 * may be move this reply Ack at the end */
	rpc_reply(audio->sndept, xid);
	switch (proc) {
	case SND_VOC_REC_AV_SYNC_CB_PTR_PROC: {
		MM_DBG("AV Sync CB:func_id=0x%8x,status=0x%x\n",
			be32_to_cpu(( \
			(struct snd_voc_rec_av_sync_cb_func_data *)\
			data)->sync_cb_func_id),\
			be32_to_cpu(( \
			(struct snd_voc_rec_av_sync_cb_func_data *)\
			data)->status));
		break;
		}
	case SND_VOC_REC_CB_FUNC_TYPE_PROC: {
		struct snd_voc_rec_data_cb_func_data *datacb_data
			= (void *)(data);
		struct snd_voc_rec_put_buf_msg bmsg;
		uint32_t rec_status = be32_to_cpu(datacb_data->rec_status);

		MM_DBG("Data CB:func_id=0x%8x,status=0x%x,\
			rec_status=0x%x\n",
			be32_to_cpu(datacb_data->cb_func_id),\
			be32_to_cpu(datacb_data->status),\
			be32_to_cpu(datacb_data->rec_status));

		/* Data recorded */
		if ((rec_status == RPC_VOC_REC_STAT_DATA) ||
		(rec_status == RPC_VOC_REC_STAT_DONE)) {
			if (datacb_data->pkt.fw_data.fw_ptr_status &&
			be32_to_cpu(datacb_data->pkt.fw_data.rec_length)) {

				MM_DBG("Copy FW link:rec_buf_size \
				= 0x%08x, rec_length=0x%08x\n",
				be32_to_cpu( \
				datacb_data->pkt.fw_data. \
				rec_buffer_size_copy),\
				be32_to_cpu(datacb_data->pkt.fw_data. \
				rec_length));

				mutex_lock(&audio->dsp_lock);
				memcpy(audio->in[audio->fill_next].data, \
					&(datacb_data->pkt.fw_data.data[0]), \
				be32_to_cpu(
				datacb_data->pkt.fw_data.rec_length));
				audio->in[audio->fill_next].used =
				be32_to_cpu(
					datacb_data->pkt.fw_data.rec_length);
				audio->in[audio->fill_next].numframes =
				be32_to_cpu(
				datacb_data->pkt.fw_data.rec_num_frames);
				mutex_unlock(&audio->dsp_lock);
			} else if (datacb_data->pkt.rw_data.rw_ptr_status &&
			be32_to_cpu(datacb_data->pkt.rw_data.rec_length)) {
				MM_DBG("Copy RW link:rec_buf_size \
				=0x%08x, rec_length=0x%08x\n",
				be32_to_cpu( \
				datacb_data->pkt.rw_data. \
				rec_buffer_size_copy),\
				be32_to_cpu(datacb_data->pkt.rw_data. \
				rec_length));

				mutex_lock(&audio->dsp_lock);
				memcpy(audio->in[audio->fill_next].data, \
				&(datacb_data->pkt.rw_data.data[0]), \
				be32_to_cpu(
					datacb_data->pkt.rw_data.rec_length));
				audio->in[audio->fill_next].used =
				be32_to_cpu(
					datacb_data->pkt.rw_data.rec_length);
				audio->in[audio->fill_next].numframes =
				be32_to_cpu(
				datacb_data->pkt.rw_data.rec_num_frames);
				mutex_unlock(&audio->dsp_lock);
			}
			if (rec_status != RPC_VOC_REC_STAT_DONE) {
				/* Not end of record */
				bmsg.args.buf = \
				(uint32_t) audio->in[audio->fill_next].data;
				bmsg.args.num_bytes = \
				be32_to_cpu(audio->in[audio->fill_next].size);

				if (++audio->fill_next ==  MAX_REC_BUF_COUNT)
					audio->fill_next = 0;

				msm_rpc_setup_req(&bmsg.hdr, audio->rpc_prog,
				audio->rpc_ver, SND_VOC_REC_PUT_BUF_PROC);

				msm_rpc_write(audio->sndept, &bmsg,
				sizeof(bmsg));

				wake_up(&audio->read_wait);
			} else {
				/* Indication record stopped gracefully */
				MM_DBG("End Of Voice Record\n");
				wake_up(&audio->wait);
			}
		} else if (rec_status == RPC_VOC_REC_STAT_PAUSED) {
			MM_DBG(" Voice Record PAUSED\n");
			audio->pause_resume = 1;
		} else if (rec_status == RPC_VOC_REC_STAT_RESUMED) {
			MM_DBG(" Voice Record RESUMED\n");
			audio->pause_resume = 0;
		} else if ((rec_status == RPC_VOC_REC_STAT_ERROR) ||
		(rec_status == RPC_VOC_REC_STAT_INVALID_PARAM) ||
		(rec_status == RPC_VOC_REC_STAT_BUFFER_ERROR))
			MM_ERR("error recording =0x%8x\n",
				rec_status);
		else if (rec_status == RPC_VOC_REC_STAT_INT_TIME)
			MM_DBG("Frames recorded matches interval \
					callback time\n");
		else if (rec_status == RPC_VOC_REC_STAT_AUTO_STOP) {
			MM_DBG(" Voice Record AUTO STOP\n");
			wake_up(&audio->read_wait);
			audmgr_disable(&audio->audmgr);
			audio->stopped = 1;
			audvoicememo_ioport_reset(audio);
			audio->stopped = 0;
			audio->enabled = 0;
		}
			break;
		}
	default:
		MM_ERR("UNKNOWN PROC , proc = 0x%8x \n", proc);
	}
}

static int voicememo_rpc_thread(void *data)
{
	struct audio_voicememo *audio = data;
	struct rpc_request_hdr *hdr = NULL;
	uint32_t type;
	int len;

	MM_DBG("start\n");

	while (!kthread_should_stop()) {
		kfree(hdr);
		hdr = NULL;

		len = msm_rpc_read(audio->sndept, (void **) &hdr, -1, -1);
		MM_DBG("rpc_read len = 0x%x\n", len);
		if (len < 0) {
			MM_ERR("rpc read failed (%d)\n", len);
			break;
		}
		if (len < RPC_COMMON_HDR_SZ)
			continue;
		type = be32_to_cpu(hdr->type);
		if (type == RPC_TYPE_REPLY) {
			struct rpc_reply_hdr *rep = (void *) hdr;
			uint32_t status;
			if (len < RPC_REPLY_HDR_SZ)
				continue;
			status = be32_to_cpu(rep->reply_stat);
			if (status == RPCMSG_REPLYSTAT_ACCEPTED) {
				status =
				be32_to_cpu(rep->data.acc_hdr.accept_stat);

				/* Confirm major RPC success during open*/
				if ((audio->enabled == 0) &&
					(status == RPC_ACCEPTSTAT_SUCCESS) &&
					(audio->rpc_xid == rep->xid)) {
						audio->rpc_status = \
							RPC_STATUS_SUCCESS;
						wake_up(&audio->wait);
				}
				MM_DBG("rpc_reply status 0x%8x\n", status);
			} else {
				MM_ERR("rpc_reply denied!\n");
			}
			/* process reply */
			continue;
		} else if (type == RPC_TYPE_REQUEST) {
			if (len < RPC_REQUEST_HDR_SZ)
				continue;
			process_rpc_request(be32_to_cpu(hdr->procedure),
						be32_to_cpu(hdr->xid),
						(void *) (hdr + 1),
						len - sizeof(*hdr),
						audio);
		} else
			MM_ERR("Unexpected type (%d)\n", type);
	}
	MM_DBG("stop\n");
	kfree(hdr);
	hdr = NULL;

	return 0;
}

/* ------------------- device --------------------- */
static long audio_voicememo_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct audio_voicememo *audio = file->private_data;
	int rc = 0;

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;
		mutex_lock(&audio->dsp_lock);
		stats.byte_count = audio->byte_count;
		stats.sample_count = audio->frame_count;
		mutex_unlock(&audio->dsp_lock);
		if (copy_to_user((void *) arg, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;
	}

	mutex_lock(&audio->lock);
	switch (cmd) {
	case AUDIO_START: {
			MM_DBG("AUDIO_START\n");
			audio->byte_count = 0;
			audio->frame_count = 0;
			if (audio->voicememo_cfg.rec_type != RPC_VOC_REC_NONE)
				rc = audvoicememo_enable(audio);
			else
				rc = -EINVAL;
			MM_DBG("AUDIO_START rc %d\n", rc);
			break;
		}
	case AUDIO_STOP: {
			MM_DBG("AUDIO_STOP\n");
			rc = audvoicememo_disable(audio);
			audio->stopped = 1;
			audvoicememo_ioport_reset(audio);
			audio->stopped = 0;
			MM_DBG("AUDIO_STOP rc %d\n", rc);
			break;
		}
	case AUDIO_GET_CONFIG: {
			struct msm_audio_config cfg;
			MM_DBG("AUDIO_GET_CONFIG\n");
			cfg.buffer_size = audio->rec_buf_size;
			cfg.buffer_count = MAX_REC_BUF_COUNT;
			cfg.sample_rate = 8000; /* Voice Encoder works on 8k,
						 * Mono */
			cfg.channel_count = 1;
			cfg.type = 0;
			cfg.unused[0] = 0;
			cfg.unused[1] = 0;
			cfg.unused[2] = 0;
			if (copy_to_user((void *) arg, &cfg, sizeof(cfg)))
				rc = -EFAULT;
			else
				rc = 0;
			MM_DBG("AUDIO_GET_CONFIG rc %d\n", rc);
			break;
		}
	case AUDIO_GET_VOICEMEMO_CONFIG: {
			MM_DBG("AUDIO_GET_VOICEMEMO_CONFIG\n");
			if (copy_to_user((void *)arg, &audio->voicememo_cfg,
				sizeof(audio->voicememo_cfg)))
				rc = -EFAULT;
			else
				rc = 0;
			MM_DBG("AUDIO_GET_VOICEMEMO_CONFIG rc %d\n", rc);
			break;
		}
	case AUDIO_SET_VOICEMEMO_CONFIG: {
			struct msm_audio_voicememo_config usr_config;
			MM_DBG("AUDIO_SET_VOICEMEMO_CONFIG\n");
			if (copy_from_user
				(&usr_config, (void *)arg,
				sizeof(usr_config))) {
				rc = -EFAULT;
				break;
			}
			if (audvoicememo_validate_usr_config(&usr_config)
					== 0) {
				audio->voicememo_cfg = usr_config;
				rc = 0;
			} else
				rc = -EINVAL;
			MM_DBG("AUDIO_SET_VOICEMEMO_CONFIG rc %d\n", rc);
			break;
		}
	case AUDIO_PAUSE: {
			struct rpc_request_hdr rhdr;
			MM_DBG("AUDIO_PAUSE\n");
			if (arg == 1)
				msm_rpc_setup_req(&rhdr, audio->rpc_prog,
				audio->rpc_ver, SND_VOC_REC_PAUSE_PROC);
			else
				msm_rpc_setup_req(&rhdr, audio->rpc_prog,
				audio->rpc_ver, SND_VOC_REC_RESUME_PROC);

			rc = msm_rpc_write(audio->sndept, &rhdr, sizeof(rhdr));
			MM_DBG("AUDIO_PAUSE exit %d\n",	rc);
			break;
		}
	default:
		MM_ERR("IOCTL %d not supported\n", cmd);
		rc = -EINVAL;
	}
	mutex_unlock(&audio->lock);
	return rc;
}

static ssize_t audio_voicememo_read(struct file *file,
				char __user *buf,
				size_t count, loff_t *pos)
{
	struct audio_voicememo *audio = file->private_data;
	const char __user *start = buf;
	int rc = 0;

	mutex_lock(&audio->read_lock);

	MM_DBG("buff read =0x%8x \n", count);

	while (count > 0) {
		rc = wait_event_interruptible_timeout(audio->read_wait,
			(audio->in[audio->read_next].used > 0) ||
			(audio->stopped),
			msecs_to_jiffies(MSM_AUD_BUFFER_UPDATE_WAIT_MS));

		if (rc == 0) {
			rc = -ETIMEDOUT;
			break;
		} else if (rc < 0)
			break;

		if (audio->stopped) {
			rc = -EBUSY;
			break;
		}
		if (count < audio->in[audio->read_next].used) {
			/* Read must happen in frame boundary. Since driver does
			 * not split frames, read count must be greater or
			 * equal to size of existing frames to copy
			 */
			MM_DBG("read not in frame boundary\n");
			break;
		} else {
			mutex_lock(&audio->dsp_lock);
			dma_coherent_post_ops();
			if (copy_to_user
				(buf, audio->in[audio->read_next].data,
				audio->in[audio->read_next].used)) {
				MM_ERR("invalid addr %x \n", (unsigned int)buf);
				rc = -EFAULT;
				mutex_unlock(&audio->dsp_lock);
				break;
			}
			count -= audio->in[audio->read_next].used;
			audio->byte_count += audio->in[audio->read_next].used;
			audio->frame_count +=
			audio->in[audio->read_next].numframes;
			buf += audio->in[audio->read_next].used;
			audio->in[audio->read_next].used = 0;
			mutex_unlock(&audio->dsp_lock);
			if ((++audio->read_next) == MAX_REC_BUF_COUNT)
				audio->read_next = 0;
			if (audio->in[audio->read_next].used == 0)
				break;  /* No data ready at this moment
					 * Exit while loop to prevent
					 * output thread sleep too long
					 */
		}
	}
	mutex_unlock(&audio->read_lock);
	if (buf > start)
		rc = buf - start;
	MM_DBG("exit return =0x%8x\n", rc);
	return rc;
}

static ssize_t audio_voicememo_write(struct file *file,
				const char __user *buf,
				size_t count, loff_t *pos)
{
	return -EINVAL;
}

static int audio_voicememo_release(struct inode *inode, struct file *file)
{
	struct audio_voicememo *audio = file->private_data;

	mutex_lock(&audio->lock);
	audvoicememo_disable(audio);
	audvoicememo_flush_buf(audio);
	audio->opened = 0;
	mutex_unlock(&audio->lock);
	return 0;
}

static int audio_voicememo_open(struct inode *inode, struct file *file)
{
	struct audio_voicememo *audio = &the_audio_voicememo;
	int rc;

	mutex_lock(&audio->lock);
	if (audio->opened) {
		rc = -EBUSY;
		goto done;
	}

	rc = audmgr_open(&audio->audmgr);

	if (rc)
		goto done;

	/*Set default param to None*/
	memset(&audio->voicememo_cfg, 0, sizeof(audio->voicememo_cfg));

	file->private_data = audio;
	audio->opened = 1;
	rc = 0;
done:
	mutex_unlock(&audio->lock);
	return rc;
}

static const struct file_operations audio_fops = {
	.owner		= THIS_MODULE,
	.open		= audio_voicememo_open,
	.release	= audio_voicememo_release,
	.read		= audio_voicememo_read,
	.write		= audio_voicememo_write,
	.unlocked_ioctl	= audio_voicememo_ioctl,
};

struct miscdevice audio_voicememo_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_voicememo",
	.fops	= &audio_fops,
};

static int audio_voicememo_probe(struct platform_device *pdev)
{
	int rc;

	if ((pdev->id != (SND_VERS_COMP & RPC_VERSION_MAJOR_MASK)) &&
	    (pdev->id != (SND_VERS2_COMP & RPC_VERSION_MAJOR_MASK)))
		return -EINVAL;

	mutex_init(&the_audio_voicememo.lock);
	mutex_init(&the_audio_voicememo.read_lock);
	mutex_init(&the_audio_voicememo.dsp_lock);
	init_waitqueue_head(&the_audio_voicememo.read_wait);
	init_waitqueue_head(&the_audio_voicememo.wait);

	the_audio_voicememo.rec_buf_ptr = dma_alloc_coherent(NULL,
					MAX_VOICEMEMO_BUF_SIZE,
					&the_audio_voicememo.phys, GFP_KERNEL);
	if (the_audio_voicememo.rec_buf_ptr == NULL) {
		MM_ERR("error allocating memory\n");
		rc = -ENOMEM;
		return rc;
	}
	the_audio_voicememo.rec_buf_size = MAX_REC_BUF_SIZE;
	MM_DBG("rec_buf_ptr = 0x%8x, phys = 0x%8x \n",
		(uint32_t) the_audio_voicememo.rec_buf_ptr, \
		the_audio_voicememo.phys);

	the_audio_voicememo.sndept = msm_rpc_connect_compatible(SND_PROG,
					SND_VERS_COMP, MSM_RPC_UNINTERRUPTIBLE);
	if (IS_ERR(the_audio_voicememo.sndept)) {
		MM_DBG("connect failed with VERS \
				= %x, trying again with another API\n",
				SND_VERS_COMP);
		the_audio_voicememo.sndept = msm_rpc_connect_compatible(
					SND_PROG, SND_VERS2_COMP,
					MSM_RPC_UNINTERRUPTIBLE);
		if (IS_ERR(the_audio_voicememo.sndept)) {
			rc = PTR_ERR(the_audio_voicememo.sndept);
			the_audio_voicememo.sndept = NULL;
			MM_ERR("Failed to connect to snd svc\n");
			goto err;
		}
		the_audio_voicememo.rpc_ver = SND_VERS2_COMP;
	} else
		the_audio_voicememo.rpc_ver = SND_VERS_COMP;

	the_audio_voicememo.task = kthread_run(voicememo_rpc_thread,
					&the_audio_voicememo, "voicememo_rpc");
	if (IS_ERR(the_audio_voicememo.task)) {
		rc = PTR_ERR(the_audio_voicememo.task);
		the_audio_voicememo.task = NULL;
		msm_rpc_close(the_audio_voicememo.sndept);
		the_audio_voicememo.sndept = NULL;
		MM_ERR("Failed to create voicememo_rpc task\n");
		goto err;
	}
	the_audio_voicememo.rpc_prog = SND_PROG;

	return misc_register(&audio_voicememo_misc);
err:
	dma_free_coherent(NULL, MAX_VOICEMEMO_BUF_SIZE,
		the_audio_voicememo.rec_buf_ptr,
		the_audio_voicememo.phys);
	the_audio_voicememo.rec_buf_ptr = NULL;
	return rc;
}

static void __exit audio_voicememo_exit(void)
{
	/* Close the RPC connection to make thread to comeout */
	msm_rpc_close(the_audio_voicememo.sndept);
	the_audio_voicememo.sndept = NULL;
	kthread_stop(the_audio_voicememo.task);
	the_audio_voicememo.task = NULL;
	if (the_audio_voicememo.rec_buf_ptr)
		dma_free_coherent(NULL, MAX_VOICEMEMO_BUF_SIZE,
			the_audio_voicememo.rec_buf_ptr,
			the_audio_voicememo.phys);
	the_audio_voicememo.rec_buf_ptr = NULL;
	misc_deregister(&audio_voicememo_misc);
}

static char audio_voicememo_rpc_name[] = "rs00000000";

static struct platform_driver audio_voicememo_driver = {
	.probe = audio_voicememo_probe,
	.driver = {
		.owner = THIS_MODULE,
	},
 };

static int __init audio_voicememo_init(void)
{
	snprintf(audio_voicememo_rpc_name, sizeof(audio_voicememo_rpc_name),
			"rs%08x", SND_PROG);
	audio_voicememo_driver.driver.name = audio_voicememo_rpc_name;
	return platform_driver_register(&audio_voicememo_driver);
}

module_init(audio_voicememo_init);
module_exit(audio_voicememo_exit);

MODULE_DESCRIPTION("MSM Voice Memo driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("QUALCOMM");
