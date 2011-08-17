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
 *
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/msm_audio_mvs.h>
#include <linux/slab.h>
#include <mach/msm_rpcrouter.h>

#define MVS_PROG 0x30000014
#define MVS_VERS 0x00030001
#define MVS_VERS_COMP_VER4 0x00040001
#define MVS_VERS_COMP_VER5 0x00050001

#define MVS_CLIENT_ID_VOIP 0x00000003

#define MVS_ACQUIRE_PROC 4
#define MVS_ENABLE_PROC 5
#define MVS_RELEASE_PROC 6
#define MVS_AMR_SET_AMR_MODE_PROC 7
#define MVS_AMR_SET_AWB_MODE_PROC 8
#define MVS_VOC_SET_FRAME_RATE_PROC 10
#define MVS_GSM_SET_DTX_MODE_PROC 11
#define MVS_G729A_SET_MODE_PROC 12
#define MVS_G711_GET_MODE_PROC 14
#define MVS_G711_SET_MODE_PROC 15
#define MVS_G711A_GET_MODE_PROC 16
#define MVS_G711A_SET_MODE_PROC 17
#define MVS_G722_SET_MODE_PROC 20
#define MVS_G722_GET_MODE_PROC 21
#define MVS_SET_DTX_MODE_PROC 22

#define MVS_EVENT_CB_TYPE_PROC 1
#define MVS_PACKET_UL_FN_TYPE_PROC 2
#define MVS_PACKET_DL_FN_TYPE_PROC 3

#define MVS_CB_FUNC_ID 0xAAAABBBB
#define MVS_UL_CB_FUNC_ID 0xBBBBCCCC
#define MVS_DL_CB_FUNC_ID 0xCCCCDDDD

#define MVS_FRAME_MODE_VOC_TX 1
#define MVS_FRAME_MODE_VOC_RX 2
#define MVS_FRAME_MODE_AMR_UL 3
#define MVS_FRAME_MODE_AMR_DL 4
#define MVS_FRAME_MODE_GSM_UL 5
#define MVS_FRAME_MODE_GSM_DL 6
#define MVS_FRAME_MODE_HR_UL 7
#define MVS_FRAME_MODE_HR_DL 8
#define MVS_FRAME_MODE_G711_UL 9
#define MVS_FRAME_MODE_G711_DL 10
#define MVS_FRAME_MODE_PCM_UL 13
#define MVS_FRAME_MODE_PCM_DL 14
#define MVS_FRAME_MODE_G729A_UL 17
#define MVS_FRAME_MODE_G729A_DL 18
#define MVS_FRAME_MODE_G711A_UL 19
#define MVS_FRAME_MODE_G711A_DL 20
#define MVS_FRAME_MODE_G722_UL 21
#define MVS_FRAME_MODE_G722_DL 22



#define MVS_PKT_CONTEXT_ISR 0x00000001

#define RPC_TYPE_REQUEST 0
#define RPC_TYPE_REPLY 1

#define RPC_STATUS_FAILURE 0
#define RPC_STATUS_SUCCESS 1
#define RPC_STATUS_REJECT 1

#define RPC_COMMON_HDR_SZ  (sizeof(uint32_t) * 2)
#define RPC_REQUEST_HDR_SZ (sizeof(struct rpc_request_hdr))
#define RPC_REPLY_HDR_SZ   (sizeof(uint32_t) * 3)

enum audio_mvs_state_type {
	AUDIO_MVS_CLOSED,
	AUDIO_MVS_OPENED,
	AUDIO_MVS_STARTED,
	AUDIO_MVS_STOPPED
};

enum audio_mvs_event_type {
	AUDIO_MVS_COMMAND,
	AUDIO_MVS_MODE,
	AUDIO_MVS_NOTIFY
};

enum audio_mvs_cmd_status_type {
	AUDIO_MVS_CMD_FAILURE,
	AUDIO_MVS_CMD_BUSY,
	AUDIO_MVS_CMD_SUCCESS
};

enum audio_mvs_mode_status_type {
	AUDIO_MVS_MODE_NOT_AVAIL,
	AUDIO_MVS_MODE_INIT,
	AUDIO_MVS_MODE_READY
};

enum audio_mvs_pkt_status_type {
	AUDIO_MVS_PKT_NORMAL,
	AUDIO_MVS_PKT_FAST,
	AUDIO_MVS_PKT_SLOW
};

/* Parameters required for MVS acquire. */
struct rpc_audio_mvs_acquire_args {
	uint32_t client_id;
	uint32_t cb_func_id;
};

struct audio_mvs_acquire_msg {
	struct rpc_request_hdr rpc_hdr;
	struct rpc_audio_mvs_acquire_args acquire_args;
};

/* Parameters required for MVS enable. */
struct rpc_audio_mvs_enable_args {
	uint32_t client_id;
	uint32_t mode;
	uint32_t ul_cb_func_id;
	uint32_t dl_cb_func_id;
	uint32_t context;
};

struct audio_mvs_enable_msg {
	struct rpc_request_hdr rpc_hdr;
	struct rpc_audio_mvs_enable_args enable_args;
};

/* Parameters required for MVS release. */
struct audio_mvs_release_msg {
	struct rpc_request_hdr rpc_hdr;
	uint32_t client_id;
};

/* Parameters required for setting AMR mode. */
struct audio_mvs_set_amr_mode_msg {
	struct rpc_request_hdr rpc_hdr;
	uint32_t amr_mode;
};

/* Parameters required for setting DTX. */
struct audio_mvs_set_dtx_mode_msg {
	struct rpc_request_hdr rpc_hdr;
	uint32_t dtx_mode;
};

/* Parameters required for setting EVRC mode. */
struct audio_mvs_set_voc_mode_msg {
	struct rpc_request_hdr rpc_hdr;
	uint32_t max_rate;
	uint32_t min_rate;
};

/* Parameters for G711 mode */
struct audio_mvs_set_g711_mode_msg {
	struct rpc_request_hdr rpc_hdr;
	uint32_t g711_mode;
};

/* Parameters for G729 mode */
struct audio_mvs_set_g729_mode_msg {
	struct rpc_request_hdr rpc_hdr;
	uint32_t g729_mode;
};

/* Parameters for G722 mode */
struct audio_mvs_set_g722_mode_msg {
	struct rpc_request_hdr rpc_hdr;
	uint32_t g722_mode;
};


/* Parameters for G711A mode */
struct audio_mvs_set_g711A_mode_msg {
	struct rpc_request_hdr rpc_hdr;
	uint32_t g711A_mode;
};

/* Parameters for EFR FR and HR mode */
struct audio_mvs_set_efr_mode_msg {
	struct rpc_request_hdr rpc_hdr;
	uint32_t efr_mode;
};

union audio_mvs_event_data {
	struct mvs_ev_command_type {
		uint32_t event;
		uint32_t client_id;
		uint32_t cmd_status;
	} mvs_ev_command_type;

	struct mvs_ev_mode_type {
		uint32_t event;
		uint32_t client_id;
		uint32_t mode_status;
		uint32_t mode;
	} mvs_ev_mode_type;

	struct mvs_ev_notify_type {
		uint32_t event;
		uint32_t client_id;
		uint32_t buf_dir;
		uint32_t max_frames;
	} mvs_ev_notify_type;
};

struct audio_mvs_cb_func_args {
	uint32_t cb_func_id;
	uint32_t valid_ptr;
	uint32_t event;
	union audio_mvs_event_data event_data;
};

struct audio_mvs_frame_info_hdr {
	uint32_t frame_mode;
	uint32_t mvs_mode;
	uint16_t buf_free_cnt;
};

struct audio_mvs_ul_reply {
	struct rpc_reply_hdr reply_hdr;
	uint32_t valid_pkt_status_ptr;
	uint32_t pkt_status;
};

struct audio_mvs_dl_cb_func_args {
	uint32_t cb_func_id;

	uint32_t valid_ptr;
	uint32_t frame_mode;
	uint32_t frame_mode_ignore;

	struct audio_mvs_frame_info_hdr frame_info_hdr;

	uint32_t amr_frame;
	uint32_t amr_mode;
};
/*general codec parameters includes AMR, G711A, PCM
G729, VOC and HR vocoders
*/
struct gnr_cdc_param {
	uint32_t param1;
	uint32_t param2;
	uint32_t valid_pkt_status_ptr;
	uint32_t pkt_status;
};
/*G711 codec parameter*/
struct g711_param {
	uint32_t param1;
	uint32_t valid_pkt_status_ptr;
	uint32_t pkt_status;
};

union codec_param {
	struct gnr_cdc_param gnr_arg;
	struct g711_param g711_arg;
};

struct audio_mvs_dl_reply {
	struct rpc_reply_hdr reply_hdr;

	uint32_t voc_pkt[MVS_MAX_VOC_PKT_SIZE/4];

	uint32_t valid_frame_info_ptr;
	uint32_t frame_mode;
	uint32_t frame_mode_again;

	struct audio_mvs_frame_info_hdr frame_info_hdr;
	union codec_param cdc_param;
};

struct audio_mvs_buf_node {
	struct list_head list;
	struct msm_audio_mvs_frame frame;
};

/* Each buffer is 20 ms, queue holds 200 ms of data. */
#define MVS_MAX_Q_LEN 10

struct audio_mvs_info_type {
	enum audio_mvs_state_type state;
	uint32_t frame_mode;
	uint32_t mvs_mode;
	uint32_t buf_free_cnt;
	uint32_t rate_type;
	uint32_t dtx_mode;

	struct msm_rpc_endpoint *rpc_endpt;
	uint32_t rpc_prog;
	uint32_t rpc_ver;
	uint32_t rpc_status;

	uint8_t *mem_chunk;

	struct list_head in_queue;
	struct list_head free_in_queue;

	struct list_head out_queue;
	struct list_head free_out_queue;

	struct task_struct *task;

	wait_queue_head_t wait;
	wait_queue_head_t mode_wait;
	wait_queue_head_t out_wait;

	struct mutex lock;
	struct mutex in_lock;
	struct mutex out_lock;

	struct wake_lock suspend_lock;
	struct wake_lock idle_lock;
};

static struct audio_mvs_info_type audio_mvs_info;

static int audio_mvs_setup_mode(struct audio_mvs_info_type *audio)
{
	int rc = 0;

	pr_debug("%s:\n", __func__);

	switch (audio->mvs_mode) {
	case MVS_MODE_AMR:
	case MVS_MODE_AMR_WB: {
		struct audio_mvs_set_amr_mode_msg set_amr_mode_msg;
		struct audio_mvs_set_dtx_mode_msg set_dtx_mode_msg;

		/* Set AMR mode. */
		memset(&set_amr_mode_msg, 0, sizeof(set_amr_mode_msg));
		set_amr_mode_msg.amr_mode = cpu_to_be32(audio->rate_type);

		if (audio->mvs_mode == MVS_MODE_AMR) {
			msm_rpc_setup_req(&set_amr_mode_msg.rpc_hdr,
					  audio->rpc_prog,
					  audio->rpc_ver,
					  MVS_AMR_SET_AMR_MODE_PROC);
		} else {
			msm_rpc_setup_req(&set_amr_mode_msg.rpc_hdr,
					  audio->rpc_prog,
					  audio->rpc_ver,
					  MVS_AMR_SET_AWB_MODE_PROC);
		}

		audio->rpc_status = RPC_STATUS_FAILURE;
		rc = msm_rpc_write(audio->rpc_endpt,
				   &set_amr_mode_msg,
				   sizeof(set_amr_mode_msg));

		if (rc >= 0) {
			pr_debug("%s: RPC write for set amr mode done\n",
					__func__);

			/* Save the MVS configuration information. */
			audio->frame_mode = MVS_FRAME_MODE_AMR_DL;

			/* Disable DTX. */
			memset(&set_dtx_mode_msg, 0, sizeof(set_dtx_mode_msg));
			set_dtx_mode_msg.dtx_mode = cpu_to_be32(0);

			msm_rpc_setup_req(&set_dtx_mode_msg.rpc_hdr,
					  audio->rpc_prog,
					  audio->rpc_ver,
					  MVS_SET_DTX_MODE_PROC);

			audio->rpc_status = RPC_STATUS_FAILURE;
			rc = msm_rpc_write(audio->rpc_endpt,
					   &set_dtx_mode_msg,
					   sizeof(set_dtx_mode_msg));

			if (rc >= 0) {
				pr_debug("%s: RPC write for set dtx done\n",
						 __func__);

				rc = 0;
			}
		} else {
			pr_err("%s: RPC write for set amr mode failed %d\n",
			       __func__, rc);
		}
		break;
	}
	case MVS_MODE_PCM:
	case MVS_MODE_LINEAR_PCM: {
		/* PCM does not have any params to be set.
		Save the MVS configuration information. */
		audio->rate_type = MVS_AMR_MODE_UNDEF;
		audio->frame_mode = MVS_FRAME_MODE_PCM_DL;
		break;
	}
	case MVS_MODE_IS127:
	case MVS_MODE_IS733:
	case MVS_MODE_4GV_NB:
	case MVS_MODE_4GV_WB: {
		struct audio_mvs_set_voc_mode_msg set_voc_mode_msg;

		/* Set EVRC mode. */
		memset(&set_voc_mode_msg, 0, sizeof(set_voc_mode_msg));
		set_voc_mode_msg.min_rate = cpu_to_be32(audio->rate_type);
		set_voc_mode_msg.max_rate = cpu_to_be32(audio->rate_type);

		msm_rpc_setup_req(&set_voc_mode_msg.rpc_hdr,
				  audio->rpc_prog,
				  audio->rpc_ver,
				  MVS_VOC_SET_FRAME_RATE_PROC);

		audio->rpc_status = RPC_STATUS_FAILURE;
		rc = msm_rpc_write(audio->rpc_endpt,
				   &set_voc_mode_msg,
				   sizeof(set_voc_mode_msg));

		if (rc >= 0) {
			pr_debug("%s: RPC write for set voc mode done\n",
					__func__);

			/* Save the MVS configuration information. */
			audio->frame_mode = MVS_FRAME_MODE_VOC_RX;

			rc = 0;
		} else {
			pr_err("%s: RPC write for set voc mode failed %d\n",
			       __func__, rc);
		}
		break;
	}
	case MVS_MODE_G711: {
		struct audio_mvs_set_g711_mode_msg set_g711_mode_msg;

		/* Set G711 mode. */
		memset(&set_g711_mode_msg, 0, sizeof(set_g711_mode_msg));
		set_g711_mode_msg.g711_mode = cpu_to_be32(audio->rate_type);

		pr_debug("%s: mode of g711:%d\n",
			       __func__, set_g711_mode_msg.g711_mode);

		msm_rpc_setup_req(&set_g711_mode_msg.rpc_hdr,
				 audio->rpc_prog,
				 audio->rpc_ver,
				 MVS_G711_SET_MODE_PROC);

		audio->rpc_status = RPC_STATUS_FAILURE;
		rc = msm_rpc_write(audio->rpc_endpt,
				  &set_g711_mode_msg,
				  sizeof(set_g711_mode_msg));

		if (rc >= 0) {
			pr_debug("%s: RPC write for set g711 mode done\n",
					__func__);
			/* Save the MVS configuration information. */
			audio->frame_mode = MVS_FRAME_MODE_G711_DL;

			rc = 0;
		} else {
		       pr_err("%s: RPC write for set g711 mode failed %d\n",
			      __func__, rc);
		}
		break;
	}
	case MVS_MODE_G729A: {
		struct audio_mvs_set_g729_mode_msg set_g729_mode_msg;

		/* Set G729 mode. */
		memset(&set_g729_mode_msg, 0, sizeof(set_g729_mode_msg));
		set_g729_mode_msg.g729_mode = cpu_to_be32(audio->dtx_mode);

		pr_debug("%s: mode of g729:%d\n",
			       __func__, set_g729_mode_msg.g729_mode);

		msm_rpc_setup_req(&set_g729_mode_msg.rpc_hdr,
				 audio->rpc_prog,
				 audio->rpc_ver,
				 MVS_G729A_SET_MODE_PROC);

		audio->rpc_status = RPC_STATUS_FAILURE;
		rc = msm_rpc_write(audio->rpc_endpt,
				  &set_g729_mode_msg,
				  sizeof(set_g729_mode_msg));

		if (rc >= 0) {
			pr_debug("%s: RPC write for set g729 mode done\n",
			       __func__);

			/* Save the MVS configuration information. */
			audio->frame_mode = MVS_FRAME_MODE_G729A_DL;

			rc = 0;
		} else {
		       pr_err("%s: RPC write for set g729 mode failed %d\n",
			      __func__, rc);
		}
		break;
	}
	case MVS_MODE_G722: {
		struct audio_mvs_set_g722_mode_msg set_g722_mode_msg;

		/* Set G722 mode. */
		memset(&set_g722_mode_msg, 0, sizeof(set_g722_mode_msg));
		set_g722_mode_msg.g722_mode = cpu_to_be32(audio->rate_type);

		pr_debug("%s: mode of g722:%d\n",
		      __func__, set_g722_mode_msg.g722_mode);

		msm_rpc_setup_req(&set_g722_mode_msg.rpc_hdr,
			audio->rpc_prog,
			audio->rpc_ver,
			MVS_G722_SET_MODE_PROC);

		audio->rpc_status = RPC_STATUS_FAILURE;
		rc = msm_rpc_write(audio->rpc_endpt,
			 &set_g722_mode_msg,
			 sizeof(set_g722_mode_msg));

		if (rc >= 0) {
			pr_debug("%s: RPC write for set g722 mode done\n",
			__func__);

			/* Save the MVS configuration information. */
			audio->frame_mode = MVS_FRAME_MODE_G722_DL;

			rc = 0;
		}
		break;
	}
	case MVS_MODE_G711A: {
		struct audio_mvs_set_g711A_mode_msg set_g711A_mode_msg;
		struct audio_mvs_set_dtx_mode_msg set_dtx_mode_msg;

		/* Set G711A mode. */
		memset(&set_g711A_mode_msg, 0, sizeof(set_g711A_mode_msg));
		set_g711A_mode_msg.g711A_mode = cpu_to_be32(audio->rate_type);

		pr_debug("%s: mode of g711A:%d\n",
		       __func__, set_g711A_mode_msg.g711A_mode);

		msm_rpc_setup_req(&set_g711A_mode_msg.rpc_hdr,
			 audio->rpc_prog,
			 audio->rpc_ver,
			 MVS_G711A_SET_MODE_PROC);

		audio->rpc_status = RPC_STATUS_FAILURE;
		rc = msm_rpc_write(audio->rpc_endpt,
			  &set_g711A_mode_msg,
			  sizeof(set_g711A_mode_msg));

		if (rc >= 0) {
			pr_debug("%s: RPC write for set g711A mode done\n",
				       __func__);

			/* Save the MVS configuration information. */
			audio->frame_mode = MVS_FRAME_MODE_G711A_DL;
			/* Set DTX MODE. */
			memset(&set_dtx_mode_msg, 0, sizeof(set_dtx_mode_msg));
			set_dtx_mode_msg.dtx_mode =
				cpu_to_be32((audio->dtx_mode));

			msm_rpc_setup_req(&set_dtx_mode_msg.rpc_hdr,
					  audio->rpc_prog,
					  audio->rpc_ver,
					  MVS_SET_DTX_MODE_PROC);

			audio->rpc_status = RPC_STATUS_FAILURE;
			rc = msm_rpc_write(audio->rpc_endpt,
					   &set_dtx_mode_msg,
					   sizeof(set_dtx_mode_msg));

			if (rc >= 0) {
				pr_debug("%s: RPC write for set dtx done\n",
						 __func__);

				rc = 0;
			}
			rc = 0;
		} else {
		pr_err("%s: RPC write for set g711A mode failed %d\n",
		      __func__, rc);
		}
		break;
	}
	case MVS_MODE_EFR:
	case MVS_MODE_FR:
	case MVS_MODE_HR: {
		struct audio_mvs_set_efr_mode_msg set_efr_mode_msg;

		/* Set G729 mode. */
		memset(&set_efr_mode_msg, 0, sizeof(set_efr_mode_msg));
		set_efr_mode_msg.efr_mode = cpu_to_be32(audio->dtx_mode);

		pr_debug("%s: mode of EFR, FR and HR:%d\n",
			       __func__, set_efr_mode_msg.efr_mode);

		msm_rpc_setup_req(&set_efr_mode_msg.rpc_hdr,
				 audio->rpc_prog,
				 audio->rpc_ver,
				 MVS_GSM_SET_DTX_MODE_PROC);

		audio->rpc_status = RPC_STATUS_FAILURE;
		rc = msm_rpc_write(audio->rpc_endpt,
				  &set_efr_mode_msg,
				  sizeof(set_efr_mode_msg));

		if (rc >= 0) {
			pr_debug("%s: RPC write for set EFR, FR and HR mode done\n",
			       __func__);

			/* Save the MVS configuration information. */
			if ((audio->mvs_mode == MVS_MODE_EFR) ||
				(audio->mvs_mode == MVS_MODE_FR))
				audio->frame_mode = MVS_FRAME_MODE_GSM_DL;
			if (audio->mvs_mode == MVS_MODE_HR)
				audio->frame_mode = MVS_FRAME_MODE_HR_DL;

			rc = 0;
		} else {
		       pr_err("%s: RPC write for set EFR, FR and HR mode failed %d\n",
			      __func__, rc);
		}
		break;
	}
	default:
		rc = -EINVAL;
		pr_err("Default case\n");
	}
	return rc;
}

static int audio_mvs_setup(struct audio_mvs_info_type *audio)
{
	int rc = 0;
	struct audio_mvs_enable_msg enable_msg;

	pr_debug("%s:\n", __func__);

	/* Enable MVS. */
	memset(&enable_msg, 0, sizeof(enable_msg));
	enable_msg.enable_args.client_id = cpu_to_be32(MVS_CLIENT_ID_VOIP);
	enable_msg.enable_args.mode = cpu_to_be32(audio->mvs_mode);
	enable_msg.enable_args.ul_cb_func_id = cpu_to_be32(MVS_UL_CB_FUNC_ID);
	enable_msg.enable_args.dl_cb_func_id = cpu_to_be32(MVS_DL_CB_FUNC_ID);
	enable_msg.enable_args.context = cpu_to_be32(MVS_PKT_CONTEXT_ISR);

	msm_rpc_setup_req(&enable_msg.rpc_hdr,
			  audio->rpc_prog,
			  audio->rpc_ver,
			  MVS_ENABLE_PROC);

	audio->rpc_status = RPC_STATUS_FAILURE;
	rc = msm_rpc_write(audio->rpc_endpt, &enable_msg, sizeof(enable_msg));

	if (rc >= 0) {
		pr_debug("%s: RPC write for enable done\n", __func__);

		rc = wait_event_timeout(audio->mode_wait,
				(audio->rpc_status != RPC_STATUS_FAILURE),
				10 * HZ);

		if (rc > 0) {
			pr_debug("%s: Wait event for enable succeeded\n",
				 __func__);
			rc = audio_mvs_setup_mode(audio);
			if (rc < 0) {
				pr_err("%s: Unknown MVS mode %d\n",
				       __func__, audio->mvs_mode);
			}
			pr_err("rc value after mode setup: %d\n", rc);
		} else {
			pr_err("%s: Wait event for enable failed %d\n",
			       __func__, rc);
		}
	} else {
		pr_err("%s: RPC write for enable failed %d\n", __func__, rc);
	}

	return rc;
}

static int audio_mvs_start(struct audio_mvs_info_type *audio)
{
	int rc = 0;
	struct audio_mvs_acquire_msg acquire_msg;

	pr_info("%s:\n", __func__);

	/* Prevent sleep. */
	wake_lock(&audio->suspend_lock);
	wake_lock(&audio->idle_lock);

	/* Acquire MVS. */
	memset(&acquire_msg, 0, sizeof(acquire_msg));
	acquire_msg.acquire_args.client_id = cpu_to_be32(MVS_CLIENT_ID_VOIP);
	acquire_msg.acquire_args.cb_func_id = cpu_to_be32(MVS_CB_FUNC_ID);

	msm_rpc_setup_req(&acquire_msg.rpc_hdr,
			  audio->rpc_prog,
			  audio->rpc_ver,
			  MVS_ACQUIRE_PROC);

	audio->rpc_status = RPC_STATUS_FAILURE;
	rc = msm_rpc_write(audio->rpc_endpt,
			   &acquire_msg,
			   sizeof(acquire_msg));

	if (rc >= 0) {
		pr_debug("%s: RPC write for acquire done\n", __func__);

		rc = wait_event_timeout(audio->wait,
			(audio->rpc_status != RPC_STATUS_FAILURE),
			1 * HZ);

		if (rc > 0) {

			rc = audio_mvs_setup(audio);

			if (rc == 0)
				audio->state = AUDIO_MVS_STARTED;

		} else {
			pr_err("%s: Wait event for acquire failed %d\n",
			       __func__, rc);

			rc = -EBUSY;
		}
	} else {
		pr_err("%s: RPC write for acquire failed %d\n", __func__, rc);

		rc = -EBUSY;
	}

	return rc;
}

static int audio_mvs_stop(struct audio_mvs_info_type *audio)
{
	int rc = 0;
	struct audio_mvs_release_msg release_msg;

	pr_info("%s:\n", __func__);

	/* Release MVS. */
	memset(&release_msg, 0, sizeof(release_msg));
	release_msg.client_id = cpu_to_be32(MVS_CLIENT_ID_VOIP);

	msm_rpc_setup_req(&release_msg.rpc_hdr,
			  audio->rpc_prog,
			  audio->rpc_ver,
			  MVS_RELEASE_PROC);

	audio->rpc_status = RPC_STATUS_FAILURE;
	rc = msm_rpc_write(audio->rpc_endpt, &release_msg, sizeof(release_msg));

	if (rc >= 0) {
		pr_debug("%s: RPC write for release done\n", __func__);

		rc = wait_event_timeout(audio->mode_wait,
				(audio->rpc_status != RPC_STATUS_FAILURE),
				1 * HZ);

		if (rc > 0) {
			pr_debug("%s: Wait event for release succeeded\n",
				 __func__);

			audio->state = AUDIO_MVS_STOPPED;

			/* Un-block read in case it is waiting for data. */
			wake_up(&audio->out_wait);
			rc = 0;
		} else {
			pr_err("%s: Wait event for release failed %d\n",
			       __func__, rc);
		}
	} else {
		pr_err("%s: RPC write for release failed %d\n", __func__, rc);
	}

	/* Allow sleep. */
	wake_unlock(&audio->suspend_lock);
	wake_unlock(&audio->idle_lock);

	return rc;
}

static void audio_mvs_process_rpc_request(uint32_t procedure,
					  uint32_t xid,
					  void *data,
					  uint32_t length,
					  struct audio_mvs_info_type *audio)
{
	int rc = 0;

	pr_debug("%s:\n", __func__);

	switch (procedure) {
	case MVS_EVENT_CB_TYPE_PROC: {
		struct audio_mvs_cb_func_args *args = data;
		struct rpc_reply_hdr reply_hdr;

		pr_debug("%s: MVS CB CB_FUNC_ID 0x%x\n",
			 __func__, be32_to_cpu(args->cb_func_id));

		if (be32_to_cpu(args->valid_ptr)) {
			uint32_t event_type = be32_to_cpu(args->event);

			pr_debug("%s: MVS CB event type %d\n",
				 __func__, be32_to_cpu(args->event));

			if (event_type == AUDIO_MVS_COMMAND) {
				uint32_t cmd_status = be32_to_cpu(
			args->event_data.mvs_ev_command_type.cmd_status);

				pr_debug("%s: MVS CB command status %d\n",
					 __func__, cmd_status);

				if (cmd_status == AUDIO_MVS_CMD_SUCCESS) {
					audio->rpc_status = RPC_STATUS_SUCCESS;
					wake_up(&audio->wait);
				}

			} else if (event_type == AUDIO_MVS_MODE) {
				uint32_t mode_status = be32_to_cpu(
				args->event_data.mvs_ev_mode_type.mode_status);

				pr_debug("%s: MVS CB mode status %d\n",
					 __func__, mode_status);

				if (mode_status == AUDIO_MVS_MODE_READY) {
					audio->rpc_status = RPC_STATUS_SUCCESS;
					wake_up(&audio->mode_wait);
				}
			} else {
				pr_err("%s: MVS CB unknown event type %d\n",
				       __func__, event_type);
			}
		} else {
			pr_err("%s: MVS CB event pointer not valid\n",
			       __func__);
		}

		/* Send ack to modem. */
		memset(&reply_hdr, 0, sizeof(reply_hdr));
		reply_hdr.xid = cpu_to_be32(xid);
		reply_hdr.type = cpu_to_be32(RPC_TYPE_REPLY);
		reply_hdr.reply_stat = cpu_to_be32(RPCMSG_REPLYSTAT_ACCEPTED);

		reply_hdr.data.acc_hdr.accept_stat = cpu_to_be32(
			RPC_ACCEPTSTAT_SUCCESS);
		reply_hdr.data.acc_hdr.verf_flavor = 0;
		reply_hdr.data.acc_hdr.verf_length = 0;

		rc = msm_rpc_write(audio->rpc_endpt,
				   &reply_hdr,
				   sizeof(reply_hdr));

		if (rc < 0)
			pr_err("%s: RPC write for response failed %d\n",
			       __func__, rc);

		break;
	}

	case MVS_PACKET_UL_FN_TYPE_PROC: {
		uint32_t *args = data;
		uint32_t pkt_len;
		uint32_t frame_mode;
		struct audio_mvs_ul_reply ul_reply;
		struct audio_mvs_buf_node *buf_node = NULL;

		pr_debug("%s: MVS UL CB_FUNC_ID 0x%x\n",
			 __func__, be32_to_cpu(*args));
		args++;

		pkt_len = be32_to_cpu(*args);
		pr_debug("%s: UL pkt_len %d\n", __func__, pkt_len);
		args++;

		/* Copy the vocoder packets. */
		mutex_lock(&audio->out_lock);

		if (!list_empty(&audio->free_out_queue)) {
			buf_node = list_first_entry(&audio->free_out_queue,
						    struct audio_mvs_buf_node,
						    list);
			list_del(&buf_node->list);

			memcpy(&buf_node->frame.voc_pkt[0], args, pkt_len);
			buf_node->frame.len = pkt_len;
			pkt_len = ALIGN(pkt_len, 4);
			args = args + pkt_len/4;

			pr_debug("%s: UL valid_ptr 0x%x\n",
				 __func__, be32_to_cpu(*args));
			args++;

			frame_mode = be32_to_cpu(*args);
			pr_debug("%s: UL frame_mode %d\n",
				 __func__, frame_mode);
			args++;

			pr_debug("%s: UL frame_mode %d\n",
				 __func__, be32_to_cpu(*args));
			args++;

			pr_debug("%s: UL frame_mode %d\n",
				 __func__, be32_to_cpu(*args));
			args++;

			pr_debug("%s: UL mvs_mode %d\n",
				 __func__, be32_to_cpu(*args));
			args++;

			pr_debug("%s: UL buf_free_cnt %d\n",
				 __func__, be32_to_cpu(*args));
			args++;

			if (frame_mode == MVS_FRAME_MODE_AMR_UL) {
				/* Extract AMR frame type. */
				buf_node->frame.frame_type = be32_to_cpu(*args);

				pr_debug("%s: UL AMR frame_type %d\n",
					 __func__, be32_to_cpu(*args));
			} else if ((frame_mode == MVS_FRAME_MODE_PCM_UL) ||
				   (frame_mode == MVS_FRAME_MODE_VOC_TX)) {
				/* PCM and EVRC don't have frame_type */
				buf_node->frame.frame_type = 0;
			} else if (frame_mode == MVS_FRAME_MODE_G711_UL) {
				/* Extract G711 frame type. */
				buf_node->frame.frame_type = be32_to_cpu(*args);

				pr_debug("%s: UL G711 frame_type %d\n",
					__func__, be32_to_cpu(*args));
			} else if (frame_mode == MVS_FRAME_MODE_G729A_UL) {
				/* Extract G729 frame type. */
				buf_node->frame.frame_type = be32_to_cpu(*args);

				pr_debug("%s: UL G729 frame_type %d\n",
					__func__, be32_to_cpu(*args));
			} else if (frame_mode == MVS_FRAME_MODE_G722_UL) {
				/* Extract G722 frame type. */
				buf_node->frame.frame_type = be32_to_cpu(*args);

				pr_debug("%s: UL G722 frame_type %d\n",
				       __func__, be32_to_cpu(*args));
			} else if (frame_mode == MVS_FRAME_MODE_G711A_UL) {
				/* Extract G711A frame type. */
				buf_node->frame.frame_type = be32_to_cpu(*args);

				pr_debug("%s: UL G711A frame_type %d\n",
				       __func__, be32_to_cpu(*args));
			} else if ((frame_mode == MVS_FRAME_MODE_GSM_UL) ||
				   (frame_mode == MVS_FRAME_MODE_HR_UL)) {
				/* Extract EFR, FR and HR frame type. */
				buf_node->frame.frame_type = be32_to_cpu(*args);

				pr_debug("%s: UL EFR,FR,HR frame_type %d\n",
					__func__, be32_to_cpu(*args));
			} else {
				pr_debug("%s: UL Unknown frame mode %d\n",
				       __func__, frame_mode);
			}

			list_add_tail(&buf_node->list, &audio->out_queue);
		} else {
			pr_err("%s: UL data dropped, read is slow\n", __func__);
		}

		mutex_unlock(&audio->out_lock);

		wake_up(&audio->out_wait);

		/* Send UL message accept to modem. */
		memset(&ul_reply, 0, sizeof(ul_reply));
		ul_reply.reply_hdr.xid = cpu_to_be32(xid);
		ul_reply.reply_hdr.type = cpu_to_be32(RPC_TYPE_REPLY);
		ul_reply.reply_hdr.reply_stat = cpu_to_be32(
			RPCMSG_REPLYSTAT_ACCEPTED);

		ul_reply.reply_hdr.data.acc_hdr.accept_stat = cpu_to_be32(
			RPC_ACCEPTSTAT_SUCCESS);
		ul_reply.reply_hdr.data.acc_hdr.verf_flavor = 0;
		ul_reply.reply_hdr.data.acc_hdr.verf_length = 0;

		ul_reply.valid_pkt_status_ptr = cpu_to_be32(0x00000001);
		ul_reply.pkt_status = cpu_to_be32(0x00000000);

		rc = msm_rpc_write(audio->rpc_endpt,
				   &ul_reply,
				   sizeof(ul_reply));

		if (rc < 0)
			pr_err("%s: RPC write for UL response failed %d\n",
			       __func__, rc);

		break;
	}

	case MVS_PACKET_DL_FN_TYPE_PROC: {
		struct audio_mvs_dl_cb_func_args *args = data;
		struct audio_mvs_dl_reply dl_reply;
		uint32_t frame_mode;
		struct audio_mvs_buf_node *buf_node = NULL;

		pr_debug("%s: MVS DL CB CB_FUNC_ID 0x%x\n",
			 __func__, be32_to_cpu(args->cb_func_id));

		frame_mode = be32_to_cpu(args->frame_mode);
		pr_debug("%s: DL frame_mode %d\n", __func__, frame_mode);

		/* Prepare and send the DL packets to modem. */
		memset(&dl_reply, 0, sizeof(dl_reply));
		dl_reply.reply_hdr.xid = cpu_to_be32(xid);
		dl_reply.reply_hdr.type = cpu_to_be32(RPC_TYPE_REPLY);
		dl_reply.reply_hdr.reply_stat = cpu_to_be32(
			RPCMSG_REPLYSTAT_ACCEPTED);

		dl_reply.reply_hdr.data.acc_hdr.accept_stat = cpu_to_be32(
			RPC_ACCEPTSTAT_SUCCESS);
		dl_reply.reply_hdr.data.acc_hdr.verf_flavor = 0;
		dl_reply.reply_hdr.data.acc_hdr.verf_length = 0;

		mutex_lock(&audio->in_lock);

		if (!list_empty(&audio->in_queue)) {
			buf_node = list_first_entry(&audio->in_queue,
						    struct audio_mvs_buf_node,
						    list);
			list_del(&buf_node->list);

			memcpy(&dl_reply.voc_pkt,
			       &buf_node->frame.voc_pkt[0],
			       buf_node->frame.len);

			pr_debug("%s:frame mode %d\n", __func__, frame_mode);
			if (frame_mode == MVS_FRAME_MODE_AMR_DL) {
				dl_reply.cdc_param.gnr_arg.param1 = cpu_to_be32(
					buf_node->frame.frame_type);
				dl_reply.cdc_param.gnr_arg.param2 =
						cpu_to_be32(audio->rate_type);
				dl_reply.cdc_param.\
						gnr_arg.valid_pkt_status_ptr =
							cpu_to_be32(0x00000001);
				dl_reply.cdc_param.gnr_arg.pkt_status =
					cpu_to_be32(AUDIO_MVS_PKT_NORMAL);
			} else if (frame_mode == MVS_FRAME_MODE_PCM_DL) {
				dl_reply.cdc_param.gnr_arg.param1 = 0;
				dl_reply.cdc_param.gnr_arg.param2 = 0;
				dl_reply.cdc_param.\
						gnr_arg.valid_pkt_status_ptr =
							cpu_to_be32(0x00000001);
				dl_reply.cdc_param.gnr_arg.pkt_status =
					cpu_to_be32(AUDIO_MVS_PKT_NORMAL);
			} else if (frame_mode == MVS_FRAME_MODE_VOC_RX) {
				dl_reply.cdc_param.gnr_arg.param1 =
						cpu_to_be32(audio->rate_type);
				dl_reply.cdc_param.gnr_arg.param2 = 0;
				dl_reply.cdc_param.\
						gnr_arg.valid_pkt_status_ptr =
							cpu_to_be32(0x00000001);
				dl_reply.cdc_param.gnr_arg.pkt_status =
					cpu_to_be32(AUDIO_MVS_PKT_NORMAL);
			} else if (frame_mode == MVS_FRAME_MODE_G711_DL) {
				dl_reply.cdc_param.g711_arg.param1 =
				cpu_to_be32(buf_node->frame.frame_type);
				dl_reply.cdc_param.\
						g711_arg.valid_pkt_status_ptr =
							cpu_to_be32(0x00000001);
				dl_reply.cdc_param.g711_arg.pkt_status =
					cpu_to_be32(AUDIO_MVS_PKT_NORMAL);
			} else if (frame_mode == MVS_FRAME_MODE_G729A_DL) {
				dl_reply.cdc_param.gnr_arg.param1 = cpu_to_be32(
				       buf_node->frame.frame_type);
				dl_reply.cdc_param.gnr_arg.param2 =
						cpu_to_be32(audio->rate_type);
				dl_reply.cdc_param.\
						gnr_arg.valid_pkt_status_ptr =
							cpu_to_be32(0x00000001);
				dl_reply.cdc_param.gnr_arg.pkt_status =
					cpu_to_be32(AUDIO_MVS_PKT_NORMAL);
			} else if (frame_mode == MVS_FRAME_MODE_G722_DL) {
				dl_reply.cdc_param.gnr_arg.param1 = cpu_to_be32(
				      buf_node->frame.frame_type);
				dl_reply.cdc_param.gnr_arg.param2 =
						cpu_to_be32(audio->rate_type);
				dl_reply.cdc_param.\
						gnr_arg.valid_pkt_status_ptr =
							cpu_to_be32(0x00000001);
				dl_reply.cdc_param.gnr_arg.pkt_status =
					cpu_to_be32(AUDIO_MVS_PKT_NORMAL);
			} else if (frame_mode == MVS_FRAME_MODE_G711A_DL) {
				dl_reply.cdc_param.gnr_arg.param1 = cpu_to_be32(
				       buf_node->frame.frame_type);
				dl_reply.cdc_param.gnr_arg.param2 =
						cpu_to_be32(audio->rate_type);
				dl_reply.cdc_param.\
						gnr_arg.valid_pkt_status_ptr =
							cpu_to_be32(0x00000001);
				dl_reply.cdc_param.gnr_arg.pkt_status =
					cpu_to_be32(AUDIO_MVS_PKT_NORMAL);
			} else if ((frame_mode == MVS_FRAME_MODE_GSM_DL) ||
				   (frame_mode == MVS_FRAME_MODE_HR_DL)) {
				dl_reply.cdc_param.gnr_arg.param1 = cpu_to_be32(
				       buf_node->frame.frame_type);
				dl_reply.cdc_param.gnr_arg.param2 =
						cpu_to_be32(audio->rate_type);
				dl_reply.cdc_param.\
						gnr_arg.valid_pkt_status_ptr =
							cpu_to_be32(0x00000001);
				dl_reply.cdc_param.gnr_arg.pkt_status =
					cpu_to_be32(AUDIO_MVS_PKT_NORMAL);
			} else {
				pr_err("%s: DL Unknown frame mode %d\n",
				       __func__, frame_mode);
			}
			list_add_tail(&buf_node->list, &audio->free_in_queue);
		} else {
			pr_debug("%s: No DL data available to send to MVS\n",
				 __func__);
			if (frame_mode == MVS_FRAME_MODE_G711_DL) {
				dl_reply.cdc_param.\
						g711_arg.valid_pkt_status_ptr =
							cpu_to_be32(0x00000001);
				dl_reply.cdc_param.g711_arg.pkt_status =
						cpu_to_be32(AUDIO_MVS_PKT_SLOW);
			} else {
				dl_reply.cdc_param.\
						gnr_arg.valid_pkt_status_ptr =
							cpu_to_be32(0x00000001);
				dl_reply.cdc_param.gnr_arg.pkt_status =
						cpu_to_be32(AUDIO_MVS_PKT_SLOW);
			}
		}

		mutex_unlock(&audio->in_lock);

		dl_reply.valid_frame_info_ptr = cpu_to_be32(0x00000001);

		dl_reply.frame_mode = cpu_to_be32(audio->frame_mode);
		dl_reply.frame_mode_again = cpu_to_be32(audio->frame_mode);

		dl_reply.frame_info_hdr.frame_mode =
			cpu_to_be32(audio->frame_mode);
		dl_reply.frame_info_hdr.mvs_mode = cpu_to_be32(audio->mvs_mode);
		dl_reply.frame_info_hdr.buf_free_cnt = 0;

		rc = msm_rpc_write(audio->rpc_endpt,
				   &dl_reply,
				   sizeof(dl_reply));

		if (rc < 0)
			pr_err("%s: RPC write for DL response failed %d\n",
			       __func__, rc);

		break;
	}

	default:
		pr_err("%s: Unknown CB type %d\n", __func__, procedure);
	}
}

static int audio_mvs_thread(void *data)
{
	struct audio_mvs_info_type *audio = data;
	struct rpc_request_hdr *rpc_hdr = NULL;

	pr_info("%s:\n", __func__);

	while (!kthread_should_stop()) {

		int rpc_hdr_len = msm_rpc_read(audio->rpc_endpt,
					       (void **) &rpc_hdr,
					       -1,
					       -1);

		if (rpc_hdr_len < 0) {
			pr_err("%s: RPC read failed %d\n",
			       __func__, rpc_hdr_len);

			break;
		} else if (rpc_hdr_len < RPC_COMMON_HDR_SZ) {
			continue;
		} else {
			uint32_t rpc_type = be32_to_cpu(rpc_hdr->type);
			if (rpc_type == RPC_TYPE_REPLY) {
				struct rpc_reply_hdr *rpc_reply =
					(void *) rpc_hdr;
				uint32_t reply_status;

				if (rpc_hdr_len < RPC_REPLY_HDR_SZ)
					continue;

				reply_status =
					be32_to_cpu(rpc_reply->reply_stat);

				if (reply_status != RPCMSG_REPLYSTAT_ACCEPTED) {
					/* If the command is not accepted, there
					 * will be no response callback. Wake
					 * the caller and report error. */
					audio->rpc_status = RPC_STATUS_REJECT;

					wake_up(&audio->wait);

					pr_err("%s: RPC reply status denied\n",
					       __func__);
				}
			} else if (rpc_type == RPC_TYPE_REQUEST) {
				if (rpc_hdr_len < RPC_REQUEST_HDR_SZ)
					continue;

				audio_mvs_process_rpc_request(
					be32_to_cpu(rpc_hdr->procedure),
					be32_to_cpu(rpc_hdr->xid),
					(void *) (rpc_hdr + 1),
					(rpc_hdr_len - sizeof(*rpc_hdr)),
					audio);
			} else {
				pr_err("%s: Unexpected RPC type %d\n",
				       __func__, rpc_type);
			}
		}

		kfree(rpc_hdr);
		rpc_hdr = NULL;
	}

	pr_info("%s: MVS thread stopped\n", __func__);

	return 0;
}

static int audio_mvs_alloc_buf(struct audio_mvs_info_type *audio)
{
	int i = 0;
	struct audio_mvs_buf_node *buf_node = NULL;
	struct list_head *ptr = NULL;
	struct list_head *next = NULL;

	pr_debug("%s:\n", __func__);

	/* Allocate input buffers. */
	for (i = 0; i < MVS_MAX_Q_LEN; i++) {
			buf_node = kmalloc(sizeof(struct audio_mvs_buf_node),
					   GFP_KERNEL);

			if (buf_node != NULL) {
				list_add_tail(&buf_node->list,
					      &audio->free_in_queue);
			} else {
				pr_err("%s: No memory for IO buffers\n",
				       __func__);
				goto err;
			}
			buf_node = NULL;
	}

	/* Allocate output buffers. */
	for (i = 0; i < MVS_MAX_Q_LEN; i++) {
			buf_node = kmalloc(sizeof(struct audio_mvs_buf_node),
					   GFP_KERNEL);

			if (buf_node != NULL) {
				list_add_tail(&buf_node->list,
					      &audio->free_out_queue);
			} else {
				pr_err("%s: No memory for IO buffers\n",
				       __func__);
				goto err;
			}
			buf_node = NULL;
	}

	return 0;

err:
	list_for_each_safe(ptr, next, &audio->free_in_queue) {
		buf_node = list_entry(ptr, struct audio_mvs_buf_node, list);
		list_del(&buf_node->list);
		kfree(buf_node);
		buf_node = NULL;
	}

	ptr = next = NULL;
	list_for_each_safe(ptr, next, &audio->free_out_queue) {
		buf_node = list_entry(ptr, struct audio_mvs_buf_node, list);
		list_del(&buf_node->list);
		kfree(buf_node);
		buf_node = NULL;
	}

	return -ENOMEM;
}

static void audio_mvs_free_buf(struct audio_mvs_info_type *audio)
{
	struct list_head *ptr = NULL;
	struct list_head *next = NULL;
	struct audio_mvs_buf_node *buf_node = NULL;

	pr_debug("%s:\n", __func__);

	mutex_lock(&audio->in_lock);
	/* Free input buffers. */
	list_for_each_safe(ptr, next, &audio->in_queue) {
		buf_node = list_entry(ptr, struct audio_mvs_buf_node, list);
		list_del(&buf_node->list);
		kfree(buf_node);
		buf_node = NULL;
	}

	ptr = next = NULL;
	/* Free free_input buffers. */
	list_for_each_safe(ptr, next, &audio->free_in_queue) {
		buf_node = list_entry(ptr, struct audio_mvs_buf_node, list);
		list_del(&buf_node->list);
		kfree(buf_node);
		buf_node = NULL;
	}
	mutex_unlock(&audio->in_lock);

	mutex_lock(&audio->out_lock);
	ptr = next = NULL;
	/* Free output buffers. */
	list_for_each_safe(ptr, next, &audio->out_queue) {
		buf_node = list_entry(ptr, struct audio_mvs_buf_node, list);
		list_del(&buf_node->list);
		kfree(buf_node);
		buf_node = NULL;
	}

	/* Free free_ioutput buffers. */
	ptr = next = NULL;
	list_for_each_safe(ptr, next, &audio->free_out_queue) {
		buf_node = list_entry(ptr, struct audio_mvs_buf_node, list);
		list_del(&buf_node->list);
		kfree(buf_node);
		buf_node = NULL;
	}
	mutex_unlock(&audio->out_lock);
}

static int audio_mvs_open(struct inode *inode, struct file *file)
{
	int rc = 0;

	pr_info("%s:\n", __func__);

	mutex_lock(&audio_mvs_info.lock);

	if (audio_mvs_info.state == AUDIO_MVS_CLOSED) {

		if (audio_mvs_info.task != NULL ||
			audio_mvs_info.rpc_endpt != NULL) {
			rc = audio_mvs_alloc_buf(&audio_mvs_info);

			if (rc == 0) {
				audio_mvs_info.state = AUDIO_MVS_OPENED;
				file->private_data = &audio_mvs_info;
			}
		}  else {
			pr_err("%s: MVS thread and RPC end point do not exist\n",
				   __func__);

			rc = -ENODEV;
		}
	} else {
		pr_err("%s: MVS driver exists, state %d\n",
		       __func__, audio_mvs_info.state);

		rc = -EBUSY;
	}

	mutex_unlock(&audio_mvs_info.lock);

	return rc;
}

static int audio_mvs_release(struct inode *inode, struct file *file)
{

	struct audio_mvs_info_type *audio = file->private_data;

	pr_info("%s:\n", __func__);

	mutex_lock(&audio->lock);
	if (audio->state == AUDIO_MVS_STARTED)
		audio_mvs_stop(audio);
	audio_mvs_free_buf(audio);
	audio->state = AUDIO_MVS_CLOSED;
	mutex_unlock(&audio->lock);

	pr_debug("%s: Release done\n", __func__);
	return 0;
}

static ssize_t audio_mvs_read(struct file *file,
			      char __user *buf,
			      size_t count,
			      loff_t *pos)
{
	int rc = 0;
	struct audio_mvs_buf_node *buf_node = NULL;
	struct audio_mvs_info_type *audio = file->private_data;

	pr_debug("%s:\n", __func__);

	rc = wait_event_interruptible_timeout(audio->out_wait,
			(!list_empty(&audio->out_queue) ||
			 audio->state == AUDIO_MVS_STOPPED),
			1 * HZ);

	if (rc > 0) {
		mutex_lock(&audio->out_lock);
		if ((audio->state == AUDIO_MVS_STARTED) &&
		    (!list_empty(&audio->out_queue))) {

			if (count >= sizeof(struct msm_audio_mvs_frame)) {
				buf_node = list_first_entry(&audio->out_queue,
						struct audio_mvs_buf_node,
						list);
				list_del(&buf_node->list);

				rc = copy_to_user(buf,
					&buf_node->frame,
					sizeof(struct msm_audio_mvs_frame));

				if (rc == 0) {
					rc = buf_node->frame.len +
					    sizeof(buf_node->frame.frame_type) +
					    sizeof(buf_node->frame.len);
				} else {
					pr_err("%s: Copy to user retuned %d",
					       __func__, rc);

					rc = -EFAULT;
				}

				list_add_tail(&buf_node->list,
					      &audio->free_out_queue);
			} else {
				pr_err("%s: Read count %d < sizeof(frame) %d",
				       __func__, count,
				       sizeof(struct msm_audio_mvs_frame));

				rc = -ENOMEM;
			}
		} else {
			pr_err("%s: Read performed in state %d\n",
			       __func__, audio->state);

			rc = -EPERM;
		}
		mutex_unlock(&audio->out_lock);

	} else if (rc == 0) {
		pr_err("%s: No UL data available\n", __func__);

		rc = -ETIMEDOUT;
	} else {
		pr_err("%s: Read was interrupted\n", __func__);

		rc = -ERESTARTSYS;
	}

	return rc;
}

static ssize_t audio_mvs_write(struct file *file,
			       const char __user *buf,
			       size_t count,
			       loff_t *pos)
{
	int rc = 0;
	struct audio_mvs_buf_node *buf_node = NULL;
	struct audio_mvs_info_type *audio = file->private_data;

	pr_debug("%s:\n", __func__);

	mutex_lock(&audio->in_lock);
	if (audio->state == AUDIO_MVS_STARTED) {
		if (count <= sizeof(struct msm_audio_mvs_frame)) {
			if (!list_empty(&audio->free_in_queue)) {
				buf_node =
					list_first_entry(&audio->free_in_queue,
						struct audio_mvs_buf_node,
						list);
				list_del(&buf_node->list);

				rc = copy_from_user(&buf_node->frame,
						    buf,
						    count);

				list_add_tail(&buf_node->list,
					      &audio->in_queue);
			} else {
				pr_err("%s: No free DL buffs\n", __func__);
			}
		} else {
			pr_err("%s: Write count %d < sizeof(frame) %d",
			       __func__, count,
			       sizeof(struct msm_audio_mvs_frame));

			rc = -ENOMEM;
		}
	} else {
		pr_err("%s: Write performed in invalid state %d\n",
		       __func__, audio->state);

		rc = -EPERM;
	}
	mutex_unlock(&audio->in_lock);

	return rc;
}

static long audio_mvs_ioctl(struct file *file,
			    unsigned int cmd,
			    unsigned long arg)
{
	int rc = 0;

	struct audio_mvs_info_type *audio = file->private_data;

	pr_info("%s:\n", __func__);

	switch (cmd) {
	case AUDIO_GET_MVS_CONFIG: {
		struct msm_audio_mvs_config config;

		pr_debug("%s: IOCTL GET_MVS_CONFIG\n", __func__);

		mutex_lock(&audio->lock);
		config.mvs_mode = audio->mvs_mode;
		config.rate_type = audio->rate_type;
		mutex_unlock(&audio->lock);

		rc = copy_to_user((void *)arg, &config, sizeof(config));
		if (rc == 0)
			rc = sizeof(config);
		else
			pr_err("%s: Config copy failed %d\n", __func__, rc);

		break;
	}

	case AUDIO_SET_MVS_CONFIG: {
		struct msm_audio_mvs_config config;

		pr_debug("%s: IOCTL SET_MVS_CONFIG\n", __func__);

		rc = copy_from_user(&config, (void *)arg, sizeof(config));
		if (rc == 0) {
			mutex_lock(&audio->lock);

			if (audio->state == AUDIO_MVS_OPENED) {
				audio->mvs_mode = config.mvs_mode;
				audio->rate_type = config.rate_type;
				audio->dtx_mode = config.dtx_mode;
			} else {
				pr_err("%s: Set confg called in state %d\n",
				       __func__, audio->state);

				rc = -EPERM;
			}

			mutex_unlock(&audio->lock);
		} else {
			pr_err("%s: Config copy failed %d\n", __func__, rc);
		}

		break;
	}

	case AUDIO_START: {
		pr_debug("%s: IOCTL START\n", __func__);

		mutex_lock(&audio->lock);

		if (audio->state == AUDIO_MVS_OPENED ||
		    audio->state == AUDIO_MVS_STOPPED) {
			rc = audio_mvs_start(audio);

			if (rc != 0)
				audio_mvs_stop(audio);
		} else {
			pr_err("%s: Start called in invalid state %d\n",
			       __func__, audio->state);

			rc = -EPERM;
		}

		mutex_unlock(&audio->lock);

		break;
	}

	case AUDIO_STOP: {
		pr_debug("%s: IOCTL STOP\n", __func__);

		mutex_lock(&audio->lock);

		if (audio->state == AUDIO_MVS_STARTED) {
			rc = audio_mvs_stop(audio);
		} else {
			pr_err("%s: Stop called in invalid state %d\n",
			       __func__, audio->state);

			rc = -EPERM;
		}

		mutex_unlock(&audio->lock);
		break;
	}

	default: {
		pr_err("%s: Unknown IOCTL %d\n", __func__, cmd);
	}
	}

	return rc;
}

static const struct file_operations audio_mvs_fops = {
	.owner = THIS_MODULE,
	.open = audio_mvs_open,
	.release = audio_mvs_release,
	.read = audio_mvs_read,
	.write = audio_mvs_write,
	.unlocked_ioctl = audio_mvs_ioctl
};

struct miscdevice audio_mvs_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_mvs",
	.fops = &audio_mvs_fops
};

static int __init audio_mvs_init(void)
{
	int rc;

	pr_info("%s:\n", __func__);

	memset(&audio_mvs_info, 0, sizeof(audio_mvs_info));
	mutex_init(&audio_mvs_info.lock);
	mutex_init(&audio_mvs_info.in_lock);
	mutex_init(&audio_mvs_info.out_lock);

	init_waitqueue_head(&audio_mvs_info.wait);
	init_waitqueue_head(&audio_mvs_info.mode_wait);
	init_waitqueue_head(&audio_mvs_info.out_wait);

	INIT_LIST_HEAD(&audio_mvs_info.in_queue);
	INIT_LIST_HEAD(&audio_mvs_info.free_in_queue);
	INIT_LIST_HEAD(&audio_mvs_info.out_queue);
	INIT_LIST_HEAD(&audio_mvs_info.free_out_queue);

	wake_lock_init(&audio_mvs_info.suspend_lock,
		       WAKE_LOCK_SUSPEND,
		       "audio_mvs_suspend");
	wake_lock_init(&audio_mvs_info.idle_lock,
		       WAKE_LOCK_IDLE,
		       "audio_mvs_idle");

	audio_mvs_info.rpc_endpt = msm_rpc_connect_compatible(MVS_PROG,
					MVS_VERS_COMP_VER5,
					MSM_RPC_UNINTERRUPTIBLE);

	if (IS_ERR(audio_mvs_info.rpc_endpt)) {
		pr_err("%s: MVS RPC connect failed ver 0x%x\n", __func__,
				MVS_VERS_COMP_VER5);
		audio_mvs_info.rpc_endpt = msm_rpc_connect_compatible(MVS_PROG,
					MVS_VERS_COMP_VER4,
					MSM_RPC_UNINTERRUPTIBLE);
		if (IS_ERR(audio_mvs_info.rpc_endpt)) {
			pr_err("%s: MVS RPC connect failed ver 0x%x\n",
				__func__, MVS_VERS_COMP_VER4);
			audio_mvs_info.rpc_endpt =
				msm_rpc_connect_compatible(MVS_PROG,
				MVS_VERS,
				MSM_RPC_UNINTERRUPTIBLE);
			if (IS_ERR(audio_mvs_info.rpc_endpt)) {
				pr_err("%s: MVS RPC connect failed ver 0x%x\n",
				   __func__, MVS_VERS);
				rc = PTR_ERR(audio_mvs_info.rpc_endpt);
				audio_mvs_info.rpc_endpt = NULL;
				goto done;
			} else {
				pr_debug("%s: MVS RPC connect succeeded ver\
					0x%x\n", __func__, MVS_VERS);
				audio_mvs_info.rpc_prog = MVS_PROG;
				audio_mvs_info.rpc_ver = MVS_VERS;
			}
		} else {
			pr_debug("%s: MVS RPC connect succeeded ver 0x%x\n",
				__func__, MVS_VERS_COMP_VER4);
			audio_mvs_info.rpc_prog = MVS_PROG;
			audio_mvs_info.rpc_ver = MVS_VERS_COMP_VER4;
		}
	} else {
		pr_debug("%s: MVS RPC connect succeeded ver 0x%x\n", __func__,
			MVS_VERS_COMP_VER5);
		audio_mvs_info.rpc_prog = MVS_PROG;
		audio_mvs_info.rpc_ver = MVS_VERS_COMP_VER5;
	}
	audio_mvs_info.task = kthread_run(audio_mvs_thread,
					  &audio_mvs_info,
					  "audio_mvs");
	if (IS_ERR(audio_mvs_info.task)) {
		pr_err("%s: MVS thread create failed\n",  __func__);
		rc = PTR_ERR(audio_mvs_info.task);
		audio_mvs_info.task = NULL;
		msm_rpc_close(audio_mvs_info.rpc_endpt);
		audio_mvs_info.rpc_endpt = NULL;
		goto done;
	}

	rc = misc_register(&audio_mvs_misc);
done:
	return rc;
}

static void __exit audio_mvs_exit(void)
{
	pr_info("%s:\n", __func__);

	misc_deregister(&audio_mvs_misc);
}

module_init(audio_mvs_init);
module_exit(audio_mvs_exit);

MODULE_DESCRIPTION("MSM MVS driver");
MODULE_LICENSE("GPL v2");

