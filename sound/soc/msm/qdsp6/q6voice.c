/*  Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <mach/qdsp6v2/audio_acdb.h>
#include "sound/apr_audio.h"
#include "sound/q6afe.h"
#include "q6voice.h"

#define TIMEOUT_MS 3000

#define CMD_STATUS_SUCCESS 0
#define CMD_STATUS_FAIL 1

#define VOC_PATH_PASSIVE 0
#define VOC_PATH_FULL 1

static struct voice_data voice;

static int voice_send_enable_vocproc_cmd(struct voice_data *v);
static int voice_send_netid_timing_cmd(struct voice_data *v);
static int voice_send_attach_vocproc_cmd(struct voice_data *v);
static int voice_send_set_device_cmd(struct voice_data *v);
static int voice_send_disable_vocproc_cmd(struct voice_data *v);
static int voice_send_vol_index_cmd(struct voice_data *v);
static int voice_send_cvp_map_memory_cmd(struct voice_data *v);
static int voice_send_cvp_unmap_memory_cmd(struct voice_data *v);
static int voice_send_cvs_map_memory_cmd(struct voice_data *v);
static int voice_send_cvs_unmap_memory_cmd(struct voice_data *v);
static int voice_send_cvs_register_cal_cmd(struct voice_data *v);
static int voice_send_cvs_deregister_cal_cmd(struct voice_data *v);
static int voice_send_cvp_register_cal_cmd(struct voice_data *v);
static int voice_send_cvp_deregister_cal_cmd(struct voice_data *v);
static int voice_send_cvp_register_vol_cal_table_cmd(struct voice_data *v);
static int voice_send_cvp_deregister_vol_cal_table_cmd(struct voice_data *v);



static int32_t qdsp_mvm_callback(struct apr_client_data *data, void *priv);
static int32_t qdsp_cvs_callback(struct apr_client_data *data, void *priv);
static int32_t qdsp_cvp_callback(struct apr_client_data *data, void *priv);

static u16 voice_get_mvm_handle(struct voice_data *v)
{
	u16 mvm_handle = 0;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	if (v->voc_path == VOC_PATH_PASSIVE)
		mvm_handle = v->mvm_passive_handle;
	else
		mvm_handle = v->mvm_full_handle;

	pr_debug("%s: mvm_handle %d\n", __func__, mvm_handle);

	return mvm_handle;
}

static void voice_set_mvm_handle(struct voice_data *v, u16 mvm_handle)
{
	pr_debug("%s: mvm_handle %d\n", __func__, mvm_handle);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return;
	}

	if (v->voc_path == VOC_PATH_PASSIVE)
		v->mvm_passive_handle = mvm_handle;
	else
		v->mvm_full_handle = mvm_handle;
}

static u16 voice_get_cvs_handle(struct voice_data *v)
{
	u16 cvs_handle = 0;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	if (v->voc_path == VOC_PATH_PASSIVE)
		cvs_handle = v->cvs_passive_handle;
	else
		cvs_handle = v->cvs_full_handle;

	pr_debug("%s: cvs_handle %d\n", __func__, cvs_handle);

	return cvs_handle;
}

static void voice_set_cvs_handle(struct voice_data *v, u16 cvs_handle)
{
	pr_debug("%s: cvs_handle %d\n", __func__, cvs_handle);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return;
	}
	if (v->voc_path == VOC_PATH_PASSIVE)
		v->cvs_passive_handle = cvs_handle;
	else
		v->cvs_full_handle = cvs_handle;
}

static u16 voice_get_cvp_handle(struct voice_data *v)
{
	u16 cvp_handle = 0;

	if (v->voc_path == VOC_PATH_PASSIVE)
		cvp_handle = v->cvp_passive_handle;
	else
		cvp_handle = v->cvp_full_handle;

	pr_debug("%s: cvp_handle %d\n", __func__, cvp_handle);

	return cvp_handle;
}

static void voice_set_cvp_handle(struct voice_data *v, u16 cvp_handle)
{
	pr_debug("%s: cvp_handle %d\n", __func__, cvp_handle);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return;
	}
	if (v->voc_path == VOC_PATH_PASSIVE)
		v->cvp_passive_handle = cvp_handle;
	else
		v->cvp_full_handle = cvp_handle;
}

static int voice_apr_register(struct voice_data *v)
{
	void *apr_mvm, *apr_cvs, *apr_cvp;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_mvm = v->apr_q6_mvm;
	apr_cvs = v->apr_q6_cvs;
	apr_cvp = v->apr_q6_cvp;

	pr_debug("into voice_apr_register_callback\n");
	/* register callback to APR */
	if (apr_mvm == NULL) {
		pr_debug("start to register MVM callback\n");

		apr_mvm = apr_register("ADSP", "MVM",
					qdsp_mvm_callback,
					0xFFFFFFFF, v);

		if (apr_mvm == NULL) {
			pr_err("Unable to register MVM\n");
			goto err;
		}
		v->apr_q6_mvm = apr_mvm;
	}

	if (apr_cvs == NULL) {
		pr_debug("start to register CVS callback\n");

		apr_cvs = apr_register("ADSP", "CVS",
					qdsp_cvs_callback,
					0xFFFFFFFF, v);

		if (apr_cvs == NULL) {
			pr_err("Unable to register CVS\n");
			goto err;
		}
		v->apr_q6_cvs = apr_cvs;
	}

	if (apr_cvp == NULL) {
		pr_debug("start to register CVP callback\n");

		apr_cvp = apr_register("ADSP", "CVP",
					qdsp_cvp_callback,
					0xFFFFFFFF, v);

		if (apr_cvp == NULL) {
			pr_err("Unable to register CVP\n");
			goto err;
		}
		v->apr_q6_cvp = apr_cvp;
	}
	return 0;

err:
	if (v->apr_q6_cvs != NULL) {
		apr_deregister(apr_cvs);
		v->apr_q6_cvs = NULL;
	}
	if (v->apr_q6_mvm != NULL) {
		apr_deregister(apr_mvm);
		v->apr_q6_mvm = NULL;
	}

	return -ENODEV;
}

static int voice_create_mvm_cvs_session(struct voice_data *v)
{
	int ret = 0;
	struct mvm_create_ctl_session_cmd mvm_session_cmd;
	struct cvs_create_passive_ctl_session_cmd cvs_session_cmd;
	struct cvs_create_full_ctl_session_cmd cvs_full_ctl_cmd;
	struct mvm_attach_stream_cmd attach_stream_cmd;
	void *apr_mvm, *apr_cvs, *apr_cvp;
	u16 mvm_handle, cvs_handle, cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_mvm = v->apr_q6_mvm;
	apr_cvs = v->apr_q6_cvs;
	apr_cvp = v->apr_q6_cvp;

	if (!apr_mvm || !apr_cvs || !apr_cvp) {
		pr_err("%s: apr_mvm or apr_cvs or apr_cvp is NULL\n", __func__);
		return -EINVAL;
	}
	mvm_handle = voice_get_mvm_handle(v);
	cvs_handle = voice_get_cvs_handle(v);
	cvp_handle = voice_get_cvp_handle(v);

	pr_debug("%s: mvm_hdl=%d, cvs_hdl=%d\n", __func__,
		mvm_handle, cvs_handle);
	/* send cmd to create mvm session and wait for response */

	if (!mvm_handle) {
		if (v->voc_path == VOC_PATH_PASSIVE) {
			mvm_session_cmd.hdr.hdr_field = APR_HDR_FIELD(
						APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
			mvm_session_cmd.hdr.pkt_size = APR_PKT_SIZE(
						APR_HDR_SIZE,
						sizeof(mvm_session_cmd) -
						APR_HDR_SIZE);
			pr_debug("send mvm create session pkt size = %d\n",
				mvm_session_cmd.hdr.pkt_size);
			mvm_session_cmd.hdr.src_port = 0;
			mvm_session_cmd.hdr.dest_port = 0;
			mvm_session_cmd.hdr.token = 0;
			mvm_session_cmd.hdr.opcode =
				VSS_IMVM_CMD_CREATE_PASSIVE_CONTROL_SESSION;
			strncpy(mvm_session_cmd.mvm_session.name,
				"default modem voice", SESSION_NAME_LEN);

			v->mvm_state = CMD_STATUS_FAIL;

			ret = apr_send_pkt(apr_mvm,
					(uint32_t *) &mvm_session_cmd);
			if (ret < 0) {
				pr_err("Error sending MVM_CONTROL_SESSION\n");
				goto fail;
			}
			ret = wait_event_timeout(v->mvm_wait,
					(v->mvm_state == CMD_STATUS_SUCCESS),
					msecs_to_jiffies(TIMEOUT_MS));
			if (!ret) {
				pr_err("%s: wait_event timeout\n", __func__);
				goto fail;
			}
		} else {
			pr_debug("%s: creating MVM full ctrl\n", __func__);
			mvm_session_cmd.hdr.hdr_field =
					APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
			mvm_session_cmd.hdr.pkt_size =
					APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(mvm_session_cmd) -
					APR_HDR_SIZE);
			mvm_session_cmd.hdr.src_port = 0;
			mvm_session_cmd.hdr.dest_port = 0;
			mvm_session_cmd.hdr.token = 0;
			mvm_session_cmd.hdr.opcode =
				VSS_IMVM_CMD_CREATE_FULL_CONTROL_SESSION;
			strncpy(mvm_session_cmd.mvm_session.name,
				"default voip", SESSION_NAME_LEN);

			v->mvm_state = CMD_STATUS_FAIL;

			ret = apr_send_pkt(apr_mvm,
					(uint32_t *) &mvm_session_cmd);
			if (ret < 0) {
				pr_err("Fail in sending MVM_CONTROL_SESSION\n");
				goto fail;
			}
			ret = wait_event_timeout(v->mvm_wait,
					 (v->mvm_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
			if (!ret) {
				pr_err("%s: wait_event timeout\n", __func__);
				goto fail;
			}
		}
		/* Get the created MVM handle. */
		mvm_handle = voice_get_mvm_handle(v);
	}
	/* send cmd to create cvs session */
	if (!cvs_handle) {
		if (v->voc_path == VOC_PATH_PASSIVE) {
			pr_debug("creating CVS passive session\n");

			cvs_session_cmd.hdr.hdr_field = APR_HDR_FIELD(
						APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
			cvs_session_cmd.hdr.pkt_size =
						APR_PKT_SIZE(APR_HDR_SIZE,
						sizeof(cvs_session_cmd) -
						APR_HDR_SIZE);
			cvs_session_cmd.hdr.src_port = 0;
			cvs_session_cmd.hdr.dest_port = 0;
			cvs_session_cmd.hdr.token = 0;
			cvs_session_cmd.hdr.opcode =
				VSS_ISTREAM_CMD_CREATE_PASSIVE_CONTROL_SESSION;
			strncpy(cvs_session_cmd.cvs_session.name,
				"default modem voice", SESSION_NAME_LEN);

			v->cvs_state = CMD_STATUS_FAIL;

			ret = apr_send_pkt(apr_cvs,
					(uint32_t *) &cvs_session_cmd);
			if (ret < 0) {
				pr_err("Fail in sending STREAM_CONTROL_SESSION\n");
				goto fail;
			}
			ret = wait_event_timeout(v->cvs_wait,
					 (v->cvs_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
			if (!ret) {
				pr_err("%s: wait_event timeout\n", __func__);
				goto fail;
			}
			/* Get the created CVS handle. */
			cvs_handle = voice_get_cvs_handle(v);

		} else {
			pr_debug("creating CVS full session\n");

			cvs_full_ctl_cmd.hdr.hdr_field =
					APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);

			cvs_full_ctl_cmd.hdr.pkt_size =
					APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(cvs_full_ctl_cmd) -
					APR_HDR_SIZE);

			cvs_full_ctl_cmd.hdr.src_port = 0;
			cvs_full_ctl_cmd.hdr.dest_port = 0;
			cvs_full_ctl_cmd.hdr.token = 0;
			cvs_full_ctl_cmd.hdr.opcode =
				VSS_ISTREAM_CMD_CREATE_FULL_CONTROL_SESSION;
			cvs_full_ctl_cmd.cvs_session.direction = 2;
			cvs_full_ctl_cmd.cvs_session.enc_media_type =
							v->mvs_info.media_type;
			cvs_full_ctl_cmd.cvs_session.dec_media_type =
							v->mvs_info.media_type;
			cvs_full_ctl_cmd.cvs_session.network_id =
						       v->mvs_info.network_type;
			strncpy(cvs_full_ctl_cmd.cvs_session.name,
			       "default q6 voice", 16);

			v->cvs_state = CMD_STATUS_FAIL;

			ret = apr_send_pkt(apr_cvs,
					   (uint32_t *) &cvs_full_ctl_cmd);

			if (ret < 0) {
				pr_err("%s: Err %d sending CREATE_FULL_CTRL\n",
					__func__, ret);
				goto fail;
			}
			ret = wait_event_timeout(v->cvs_wait,
					(v->cvs_state == CMD_STATUS_SUCCESS),
					msecs_to_jiffies(TIMEOUT_MS));
			if (!ret) {
				pr_err("%s: wait_event timeout\n", __func__);
				goto fail;
			}
			/* Get the created CVS handle. */
			cvs_handle = voice_get_cvs_handle(v);

			/* Attach MVM to CVS. */
			pr_debug("Attach MVM to stream\n");

			attach_stream_cmd.hdr.hdr_field =
					APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);
			attach_stream_cmd.hdr.pkt_size =
					APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(attach_stream_cmd) -
					APR_HDR_SIZE);
			attach_stream_cmd.hdr.src_port = 0;
			attach_stream_cmd.hdr.dest_port = mvm_handle;
			attach_stream_cmd.hdr.token = 0;
			attach_stream_cmd.hdr.opcode =
						VSS_IMVM_CMD_ATTACH_STREAM;
			attach_stream_cmd.attach_stream.handle = cvs_handle;

			v->mvm_state = CMD_STATUS_FAIL;
			ret = apr_send_pkt(apr_mvm,
					   (uint32_t *) &attach_stream_cmd);
			if (ret < 0) {
				pr_err("%s: Error %d sending ATTACH_STREAM\n",
				       __func__, ret);
				goto fail;
			}
			ret = wait_event_timeout(v->mvm_wait,
					 (v->mvm_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
			if (!ret) {
				pr_err("%s: wait_event timeout\n", __func__);
				goto fail;
			}
		}
	}
	return 0;

fail:
	return -EINVAL;
}

static int voice_destroy_mvm_cvs_session(struct voice_data *v)
{
	int ret = 0;
	struct mvm_detach_stream_cmd detach_stream;
	struct apr_hdr mvm_destroy;
	struct apr_hdr cvs_destroy;
	void *apr_mvm, *apr_cvs;
	u16 mvm_handle, cvs_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_mvm = v->apr_q6_mvm;
	apr_cvs = v->apr_q6_cvs;

	if (!apr_mvm || !apr_cvs) {
		pr_err("%s: apr_mvm or apr_cvs is NULL\n", __func__);
		return -EINVAL;
	}
	mvm_handle = voice_get_mvm_handle(v);
	cvs_handle = voice_get_cvs_handle(v);

	/* MVM, CVS sessions are destroyed only for Full control sessions. */
	if (v->voc_path == VOC_PATH_FULL) {
		pr_debug("MVM detach stream\n");

		/* Detach voice stream. */
		detach_stream.hdr.hdr_field =
					APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);
		detach_stream.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(detach_stream) - APR_HDR_SIZE);
		detach_stream.hdr.src_port = 0;
		detach_stream.hdr.dest_port = mvm_handle;
		detach_stream.hdr.token = 0;
		detach_stream.hdr.opcode = VSS_IMVM_CMD_DETACH_STREAM;
		detach_stream.detach_stream.handle = cvs_handle;

		v->mvm_state = CMD_STATUS_FAIL;

		ret = apr_send_pkt(apr_mvm, (uint32_t *) &detach_stream);
		if (ret < 0) {
			pr_err("%s: Error %d sending DETACH_STREAM\n",
			       __func__, ret);
			goto fail;
		}
		ret = wait_event_timeout(v->mvm_wait,
					 (v->mvm_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait event timeout\n", __func__);
			goto fail;
		}
		/* Destroy CVS. */
		pr_debug("CVS destroy session\n");

		cvs_destroy.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						      APR_HDR_LEN(APR_HDR_SIZE),
						      APR_PKT_VER);
		cvs_destroy.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(cvs_destroy) - APR_HDR_SIZE);
		cvs_destroy.src_port = 0;
		cvs_destroy.dest_port = cvs_handle;
		cvs_destroy.token = 0;
		cvs_destroy.opcode = APRV2_IBASIC_CMD_DESTROY_SESSION;

		v->cvs_state = CMD_STATUS_FAIL;

		ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_destroy);
		if (ret < 0) {
			pr_err("%s: Error %d sending CVS DESTROY\n",
			       __func__, ret);
			goto fail;
		}
		ret = wait_event_timeout(v->cvs_wait,
					 (v->cvs_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait event timeout\n", __func__);

			goto fail;
		}
		cvs_handle = 0;
		voice_set_cvs_handle(v, cvs_handle);

		/* Destroy MVM. */
		pr_debug("MVM destroy session\n");

		mvm_destroy.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						      APR_HDR_LEN(APR_HDR_SIZE),
						      APR_PKT_VER);
		mvm_destroy.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					    sizeof(mvm_destroy) - APR_HDR_SIZE);
		mvm_destroy.src_port = 0;
		mvm_destroy.dest_port = mvm_handle;
		mvm_destroy.token = 0;
		mvm_destroy.opcode = APRV2_IBASIC_CMD_DESTROY_SESSION;

		v->mvm_state = CMD_STATUS_FAIL;

		ret = apr_send_pkt(apr_mvm, (uint32_t *) &mvm_destroy);
		if (ret < 0) {
			pr_err("%s: Error %d sending MVM DESTROY\n",
			       __func__, ret);

			goto fail;
		}
		ret = wait_event_timeout(v->mvm_wait,
					 (v->mvm_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait event timeout\n", __func__);

			goto fail;
		}
		mvm_handle = 0;
		voice_set_mvm_handle(v, mvm_handle);
	}
	return 0;
fail:
	return -EINVAL;
}

static int voice_send_tty_mode_cmd(struct voice_data *v)
{
	int ret = 0;
	struct mvm_set_tty_mode_cmd mvm_tty_mode_cmd;
	void *apr_mvm;
	u16 mvm_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_mvm = v->apr_q6_mvm;

	if (!apr_mvm) {
		pr_err("%s: apr_mvm is NULL.\n", __func__);
		return -EINVAL;
	}
	mvm_handle = voice_get_mvm_handle(v);

	if (v->tty_mode) {
		/* send tty mode cmd to mvm */
		mvm_tty_mode_cmd.hdr.hdr_field = APR_HDR_FIELD(
						APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
		mvm_tty_mode_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
						sizeof(mvm_tty_mode_cmd) -
						APR_HDR_SIZE);
		pr_debug("pkt size = %d\n", mvm_tty_mode_cmd.hdr.pkt_size);
		mvm_tty_mode_cmd.hdr.src_port = 0;
		mvm_tty_mode_cmd.hdr.dest_port = mvm_handle;
		mvm_tty_mode_cmd.hdr.token = 0;
		mvm_tty_mode_cmd.hdr.opcode = VSS_ISTREAM_CMD_SET_TTY_MODE;
		mvm_tty_mode_cmd.tty_mode.mode = v->tty_mode;
		pr_debug("tty mode =%d\n", mvm_tty_mode_cmd.tty_mode.mode);

		v->mvm_state = CMD_STATUS_FAIL;
		ret = apr_send_pkt(apr_mvm, (uint32_t *) &mvm_tty_mode_cmd);
		if (ret < 0) {
			pr_err("Fail: sending VSS_ISTREAM_CMD_SET_TTY_MODE\n");
			goto fail;
		}
		ret = wait_event_timeout(v->mvm_wait,
					 (v->mvm_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);
			goto fail;
		}
	}
	return 0;
fail:
	return -EINVAL;
}

static int voice_config_cvs_vocoder(struct voice_data *v)
{
	int ret = 0;
	void *apr_cvs;
	u16 cvs_handle;
	/* Set media type. */
	struct cvs_set_media_type_cmd cvs_set_media_cmd;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvs = v->apr_q6_cvs;

	if (!apr_cvs) {
		pr_err("%s: apr_cvs is NULL.\n", __func__);
		return -EINVAL;
	}

	cvs_handle = voice_get_cvs_handle(v);

	cvs_set_media_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	cvs_set_media_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				      sizeof(cvs_set_media_cmd) - APR_HDR_SIZE);
	cvs_set_media_cmd.hdr.src_port = 0;
	cvs_set_media_cmd.hdr.dest_port = cvs_handle;
	cvs_set_media_cmd.hdr.token = 0;
	cvs_set_media_cmd.hdr.opcode = VSS_ISTREAM_CMD_SET_MEDIA_TYPE;
	cvs_set_media_cmd.media_type.tx_media_id = v->mvs_info.media_type;
	cvs_set_media_cmd.media_type.rx_media_id = v->mvs_info.media_type;

	v->cvs_state = CMD_STATUS_FAIL;

	ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_set_media_cmd);
	if (ret < 0) {
		pr_err("%s: Error %d sending SET_MEDIA_TYPE\n",
		       __func__, ret);

		goto fail;
	}
	ret = wait_event_timeout(v->cvs_wait,
				 (v->cvs_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);

		goto fail;
	}
	/* Set encoder properties. */
	switch (v->mvs_info.media_type) {
	case VSS_MEDIA_ID_EVRC_MODEM: {
		struct cvs_set_cdma_enc_minmax_rate_cmd cvs_set_cdma_rate;

		pr_debug("Setting EVRC min-max rate\n");

		cvs_set_cdma_rate.hdr.hdr_field =
					APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);
		cvs_set_cdma_rate.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				      sizeof(cvs_set_cdma_rate) - APR_HDR_SIZE);
		cvs_set_cdma_rate.hdr.src_port = 0;
		cvs_set_cdma_rate.hdr.dest_port = cvs_handle;
		cvs_set_cdma_rate.hdr.token = 0;
		cvs_set_cdma_rate.hdr.opcode =
				VSS_ISTREAM_CMD_CDMA_SET_ENC_MINMAX_RATE;
		cvs_set_cdma_rate.cdma_rate.min_rate = v->mvs_info.rate;
		cvs_set_cdma_rate.cdma_rate.max_rate = v->mvs_info.rate;

		v->cvs_state = CMD_STATUS_FAIL;

		ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_set_cdma_rate);
		if (ret < 0) {
			pr_err("%s: Error %d sending SET_EVRC_MINMAX_RATE\n",
			       __func__, ret);
			goto fail;
		}
		ret = wait_event_timeout(v->cvs_wait,
					 (v->cvs_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);

			goto fail;
		}
		break;
	}
	case VSS_MEDIA_ID_AMR_NB_MODEM: {
		struct cvs_set_amr_enc_rate_cmd cvs_set_amr_rate;
		struct cvs_set_enc_dtx_mode_cmd cvs_set_dtx;

		pr_debug("Setting AMR rate\n");

		cvs_set_amr_rate.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
		cvs_set_amr_rate.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				       sizeof(cvs_set_amr_rate) - APR_HDR_SIZE);
		cvs_set_amr_rate.hdr.src_port = 0;
		cvs_set_amr_rate.hdr.dest_port = cvs_handle;
		cvs_set_amr_rate.hdr.token = 0;
		cvs_set_amr_rate.hdr.opcode =
					VSS_ISTREAM_CMD_VOC_AMR_SET_ENC_RATE;
		cvs_set_amr_rate.amr_rate.mode = v->mvs_info.rate;

		v->cvs_state = CMD_STATUS_FAIL;

		ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_set_amr_rate);
		if (ret < 0) {
			pr_err("%s: Error %d sending SET_AMR_RATE\n",
			       __func__, ret);
			goto fail;
		}
		ret = wait_event_timeout(v->cvs_wait,
					 (v->cvs_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);
			goto fail;
		}
		/* Disable DTX */
		pr_debug("Disabling DTX\n");

		cvs_set_dtx.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						      APR_HDR_LEN(APR_HDR_SIZE),
						      APR_PKT_VER);
		cvs_set_dtx.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(cvs_set_dtx) - APR_HDR_SIZE);
		cvs_set_dtx.hdr.src_port = 0;
		cvs_set_dtx.hdr.dest_port = cvs_handle;
		cvs_set_dtx.hdr.token = 0;
		cvs_set_dtx.hdr.opcode = VSS_ISTREAM_CMD_SET_ENC_DTX_MODE;
		cvs_set_dtx.dtx_mode.enable = 0;

		v->cvs_state = CMD_STATUS_FAIL;

		ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_set_dtx);
		if (ret < 0) {
			pr_err("%s: Error %d sending SET_DTX\n",
			       __func__, ret);
			goto fail;
		}
		ret = wait_event_timeout(v->cvs_wait,
					 (v->cvs_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);
			goto fail;
		}
		break;
	}
	case VSS_MEDIA_ID_AMR_WB_MODEM: {
		struct cvs_set_amrwb_enc_rate_cmd cvs_set_amrwb_rate;
		struct cvs_set_enc_dtx_mode_cmd cvs_set_dtx;

		pr_debug("Setting AMR WB rate\n");

		cvs_set_amrwb_rate.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
		cvs_set_amrwb_rate.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
						sizeof(cvs_set_amrwb_rate) -
						APR_HDR_SIZE);
		cvs_set_amrwb_rate.hdr.src_port = 0;
		cvs_set_amrwb_rate.hdr.dest_port = cvs_handle;
		cvs_set_amrwb_rate.hdr.token = 0;
		cvs_set_amrwb_rate.hdr.opcode =
					VSS_ISTREAM_CMD_VOC_AMRWB_SET_ENC_RATE;
		cvs_set_amrwb_rate.amrwb_rate.mode = v->mvs_info.rate;

		v->cvs_state = CMD_STATUS_FAIL;

		ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_set_amrwb_rate);
		if (ret < 0) {
			pr_err("%s: Error %d sending SET_AMRWB_RATE\n",
			       __func__, ret);
			goto fail;
		}
		ret = wait_event_timeout(v->cvs_wait,
					 (v->cvs_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);
			goto fail;
		}
		/* Disable DTX */
		pr_debug("Disabling DTX\n");

		cvs_set_dtx.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						      APR_HDR_LEN(APR_HDR_SIZE),
						      APR_PKT_VER);
		cvs_set_dtx.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				       sizeof(cvs_set_dtx) - APR_HDR_SIZE);
		cvs_set_dtx.hdr.src_port = 0;
		cvs_set_dtx.hdr.dest_port = cvs_handle;
		cvs_set_dtx.hdr.token = 0;
		cvs_set_dtx.hdr.opcode = VSS_ISTREAM_CMD_SET_ENC_DTX_MODE;
		cvs_set_dtx.dtx_mode.enable = 0;

		v->cvs_state = CMD_STATUS_FAIL;

		ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_set_dtx);
		if (ret < 0) {
			pr_err("%s: Error %d sending SET_DTX\n",
			       __func__, ret);
			goto fail;
		}
		ret = wait_event_timeout(v->cvs_wait,
					 (v->cvs_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);
			goto fail;
		}
		break;
	}
	case VSS_MEDIA_ID_G729:
	case VSS_MEDIA_ID_G711_ALAW:
	case VSS_MEDIA_ID_G711_MULAW: {
		struct cvs_set_enc_dtx_mode_cmd cvs_set_dtx;
		/* Disable DTX */
		pr_debug("Disabling DTX\n");

		cvs_set_dtx.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						      APR_HDR_LEN(APR_HDR_SIZE),
						      APR_PKT_VER);
		cvs_set_dtx.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					    sizeof(cvs_set_dtx) - APR_HDR_SIZE);
		cvs_set_dtx.hdr.src_port = 0;
		cvs_set_dtx.hdr.dest_port = cvs_handle;
		cvs_set_dtx.hdr.token = 0;
		cvs_set_dtx.hdr.opcode = VSS_ISTREAM_CMD_SET_ENC_DTX_MODE;
		cvs_set_dtx.dtx_mode.enable = 0;

		v->cvs_state = CMD_STATUS_FAIL;

		ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_set_dtx);
		if (ret < 0) {
			pr_err("%s: Error %d sending SET_DTX\n",
			       __func__, ret);
			goto fail;
		}
		ret = wait_event_timeout(v->cvs_wait,
					 (v->cvs_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);
			goto fail;
		}
		break;
	}
	default:
		/* Do nothing. */
		break;
	}
	return 0;

fail:
	return -EINVAL;
}

static int voice_send_start_voice_cmd(struct voice_data *v)
{
	struct apr_hdr mvm_start_voice_cmd;
	int ret = 0;
	void *apr_mvm;
	u16 mvm_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_mvm = v->apr_q6_mvm;

	if (!apr_mvm) {
		pr_err("%s: apr_mvm is NULL.\n", __func__);
		return -EINVAL;
	}
	mvm_handle = voice_get_mvm_handle(v);

	mvm_start_voice_cmd.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	mvm_start_voice_cmd.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(mvm_start_voice_cmd) - APR_HDR_SIZE);
	pr_debug("send mvm_start_voice_cmd pkt size = %d\n",
				mvm_start_voice_cmd.pkt_size);
	mvm_start_voice_cmd.src_port = 0;
	mvm_start_voice_cmd.dest_port = mvm_handle;
	mvm_start_voice_cmd.token = 0;
	mvm_start_voice_cmd.opcode = VSS_IMVM_CMD_START_VOICE;

	v->mvm_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_mvm, (uint32_t *) &mvm_start_voice_cmd);
	if (ret < 0) {
		pr_err("Fail in sending VSS_IMVM_CMD_START_VOICE\n");
		goto fail;
	}
	ret = wait_event_timeout(v->mvm_wait,
				 (v->mvm_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;
fail:
	return -EINVAL;
}

static int voice_send_disable_vocproc_cmd(struct voice_data *v)
{
	struct apr_hdr cvp_disable_cmd;
	int ret = 0;
	void *apr_cvp;
	u16 cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvp = v->apr_q6_cvp;

	if (!apr_cvp) {
		pr_err("%s: apr regist failed\n", __func__);
		return -EINVAL;
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* disable vocproc and wait for respose */
	cvp_disable_cmd.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	cvp_disable_cmd.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_disable_cmd) - APR_HDR_SIZE);
	pr_debug("cvp_disable_cmd pkt size = %d, cvp_handle=%d\n",
		cvp_disable_cmd.pkt_size, cvp_handle);
	cvp_disable_cmd.src_port = 0;
	cvp_disable_cmd.dest_port = cvp_handle;
	cvp_disable_cmd.token = 0;
	cvp_disable_cmd.opcode = VSS_IVOCPROC_CMD_DISABLE;

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvp, (uint32_t *) &cvp_disable_cmd);
	if (ret < 0) {
		pr_err("Fail in sending VSS_IVOCPROC_CMD_DISABLE\n");
		goto fail;
	}
	ret = wait_event_timeout(v->cvp_wait,
			(v->cvp_state == CMD_STATUS_SUCCESS),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	return 0;
fail:
	return -EINVAL;
}

static int voice_send_set_device_cmd(struct voice_data *v)
{
	struct cvp_set_device_cmd  cvp_setdev_cmd;
	int ret = 0;
	void *apr_cvp;
	u16 cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvp = v->apr_q6_cvp;

	if (!apr_cvp) {
		pr_err("%s: apr_cvp is NULL.\n", __func__);
		return -EINVAL;
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* set device and wait for response */
	cvp_setdev_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	cvp_setdev_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_setdev_cmd) - APR_HDR_SIZE);
	pr_debug(" send create cvp setdev, pkt size = %d\n",
			cvp_setdev_cmd.hdr.pkt_size);
	cvp_setdev_cmd.hdr.src_port = 0;
	cvp_setdev_cmd.hdr.dest_port = cvp_handle;
	cvp_setdev_cmd.hdr.token = 0;
	cvp_setdev_cmd.hdr.opcode = VSS_IVOCPROC_CMD_SET_DEVICE;

	/* Use default topology if invalid value in ACDB */
	cvp_setdev_cmd.cvp_set_device.tx_topology_id =
				get_voice_tx_topology();
	if (cvp_setdev_cmd.cvp_set_device.tx_topology_id == 0)
		cvp_setdev_cmd.cvp_set_device.tx_topology_id =
				VSS_IVOCPROC_TOPOLOGY_ID_TX_SM_ECNS;

	cvp_setdev_cmd.cvp_set_device.rx_topology_id =
				get_voice_rx_topology();
	if (cvp_setdev_cmd.cvp_set_device.rx_topology_id == 0)
		cvp_setdev_cmd.cvp_set_device.rx_topology_id =
				VSS_IVOCPROC_TOPOLOGY_ID_RX_DEFAULT;
	cvp_setdev_cmd.cvp_set_device.tx_port_id = v->dev_tx.port_id;
	cvp_setdev_cmd.cvp_set_device.rx_port_id = v->dev_rx.port_id;
	pr_debug("topology=%d , tx_port_id=%d, rx_port_id=%d\n",
		cvp_setdev_cmd.cvp_set_device.tx_topology_id,
		cvp_setdev_cmd.cvp_set_device.tx_port_id,
		cvp_setdev_cmd.cvp_set_device.rx_port_id);

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvp, (uint32_t *) &cvp_setdev_cmd);
	if (ret < 0) {
		pr_err("Fail in sending VOCPROC_FULL_CONTROL_SESSION\n");
		goto fail;
	}
	pr_debug("wait for cvp create session event\n");
	ret = wait_event_timeout(v->cvp_wait,
			(v->cvp_state == CMD_STATUS_SUCCESS),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	return 0;
fail:
	return -EINVAL;
}

static int voice_send_stop_voice_cmd(struct voice_data *v)
{
	struct apr_hdr mvm_stop_voice_cmd;
	int ret = 0;
	void *apr_mvm;
	u16 mvm_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_mvm = v->apr_q6_mvm;

	if (!apr_mvm) {
		pr_err("%s: apr_mvm is NULL.\n", __func__);
		return -EINVAL;
	}
	mvm_handle = voice_get_mvm_handle(v);

	mvm_stop_voice_cmd.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	mvm_stop_voice_cmd.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(mvm_stop_voice_cmd) - APR_HDR_SIZE);
	pr_debug("send mvm_stop_voice_cmd pkt size = %d\n",
				mvm_stop_voice_cmd.pkt_size);
	mvm_stop_voice_cmd.src_port = 0;
	mvm_stop_voice_cmd.dest_port = mvm_handle;
	mvm_stop_voice_cmd.token = 0;
	mvm_stop_voice_cmd.opcode = VSS_IMVM_CMD_STOP_VOICE;

	v->mvm_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_mvm, (uint32_t *) &mvm_stop_voice_cmd);
	if (ret < 0) {
		pr_err("Fail in sending VSS_IMVM_CMD_STOP_VOICE\n");
		goto fail;
	}
	ret = wait_event_timeout(v->mvm_wait,
				 (v->mvm_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	return 0;
fail:
	return -EINVAL;
}

static int voice_send_cvs_register_cal_cmd(struct voice_data *v)
{
	struct cvs_register_cal_data_cmd cvs_reg_cal_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	void *apr_cvs;
	u16 cvs_handle;

	/* get the cvs cal data */
	get_all_vocstrm_cal(&cal_block);
	if (cal_block.cal_size == 0)
		goto fail;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvs = v->apr_q6_cvs;

	if (!apr_cvs) {
		pr_err("%s: apr_cvs is NULL.\n", __func__);
		return -EINVAL;
	}
	cvs_handle = voice_get_cvs_handle(v);

	/* fill in the header */
	cvs_reg_cal_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvs_reg_cal_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvs_reg_cal_cmd) - APR_HDR_SIZE);
	cvs_reg_cal_cmd.hdr.src_port = 0;
	cvs_reg_cal_cmd.hdr.dest_port = cvs_handle;
	cvs_reg_cal_cmd.hdr.token = 0;
	cvs_reg_cal_cmd.hdr.opcode = VSS_ISTREAM_CMD_REGISTER_CALIBRATION_DATA;

	cvs_reg_cal_cmd.cvs_cal_data.phys_addr = cal_block.cal_paddr;
	cvs_reg_cal_cmd.cvs_cal_data.mem_size = cal_block.cal_size;

	v->cvs_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_reg_cal_cmd);
	if (ret < 0) {
		pr_err("Fail: sending cvs cal,\n");
		goto fail;
	}
	ret = wait_event_timeout(v->cvs_wait,
			(v->cvs_state == CMD_STATUS_SUCCESS),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;
fail:
	return -EINVAL;

}

static int voice_send_cvs_deregister_cal_cmd(struct voice_data *v)
{
	struct cvs_deregister_cal_data_cmd cvs_dereg_cal_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	void *apr_cvs;
	u16 cvs_handle;

	get_all_vocstrm_cal(&cal_block);
	if (cal_block.cal_size == 0)
		return 0;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvs = v->apr_q6_cvs;

	if (!apr_cvs) {
		pr_err("%s: apr_cvs is NULL.\n", __func__);
		return -EINVAL;
	}
	cvs_handle = voice_get_cvs_handle(v);

	/* fill in the header */
	cvs_dereg_cal_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvs_dereg_cal_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvs_dereg_cal_cmd) - APR_HDR_SIZE);
	cvs_dereg_cal_cmd.hdr.src_port = 0;
	cvs_dereg_cal_cmd.hdr.dest_port = cvs_handle;
	cvs_dereg_cal_cmd.hdr.token = 0;
	cvs_dereg_cal_cmd.hdr.opcode =
			VSS_ISTREAM_CMD_DEREGISTER_CALIBRATION_DATA;

	v->cvs_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_dereg_cal_cmd);
	if (ret < 0) {
		pr_err("Fail: sending cvs cal,\n");
		goto fail;
	}
	ret = wait_event_timeout(v->cvs_wait,
			(v->cvs_state == CMD_STATUS_SUCCESS),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;
fail:
	return -EINVAL;

}

static int voice_send_cvp_map_memory_cmd(struct voice_data *v)
{
	struct vss_map_memory_cmd cvp_map_mem_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	void *apr_cvp;
	u16 cvp_handle;

	/* get all cvp cal data */
	get_all_cvp_cal(&cal_block);
	if (cal_block.cal_size == 0)
		goto fail;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvp = v->apr_q6_cvp;

	if (!apr_cvp) {
		pr_err("%s: apr_cvp is NULL.\n", __func__);
		return -EINVAL;
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_map_mem_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_map_mem_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_map_mem_cmd) - APR_HDR_SIZE);
	cvp_map_mem_cmd.hdr.src_port = 0;
	cvp_map_mem_cmd.hdr.dest_port = cvp_handle;
	cvp_map_mem_cmd.hdr.token = 0;
	cvp_map_mem_cmd.hdr.opcode = VSS_ICOMMON_CMD_MAP_MEMORY;

	pr_debug("%s, phy_addr:%d, mem_size:%d\n", __func__,
		cal_block.cal_paddr, cal_block.cal_size);
	cvp_map_mem_cmd.vss_map_mem.phys_addr = cal_block.cal_paddr;
	cvp_map_mem_cmd.vss_map_mem.mem_size = cal_block.cal_size;
	cvp_map_mem_cmd.vss_map_mem.mem_pool_id =
				VSS_ICOMMON_MAP_MEMORY_SHMEM8_4K_POOL;

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvp, (uint32_t *) &cvp_map_mem_cmd);
	if (ret < 0) {
		pr_err("Fail: sending cvp cal,\n");
		goto fail;
	}
	ret = wait_event_timeout(v->cvp_wait,
			(v->cvp_state == CMD_STATUS_SUCCESS),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;
fail:
	return -EINVAL;

}

static int voice_send_cvp_unmap_memory_cmd(struct voice_data *v)
{
	struct vss_unmap_memory_cmd cvp_unmap_mem_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	void *apr_cvp;
	u16 cvp_handle;

	get_all_cvp_cal(&cal_block);
	if (cal_block.cal_size == 0)
		return 0;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvp = v->apr_q6_cvp;

	if (!apr_cvp) {
		pr_err("%s: apr_cvp is NULL.\n", __func__);
		return -EINVAL;
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_unmap_mem_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_unmap_mem_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_unmap_mem_cmd) - APR_HDR_SIZE);
	cvp_unmap_mem_cmd.hdr.src_port = 0;
	cvp_unmap_mem_cmd.hdr.dest_port = cvp_handle;
	cvp_unmap_mem_cmd.hdr.token = 0;
	cvp_unmap_mem_cmd.hdr.opcode = VSS_ICOMMON_CMD_UNMAP_MEMORY;

	cvp_unmap_mem_cmd.vss_unmap_mem.phys_addr = cal_block.cal_paddr;

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvp, (uint32_t *) &cvp_unmap_mem_cmd);
	if (ret < 0) {
		pr_err("Fail: sending cvp cal,\n");
		goto fail;
	}
	ret = wait_event_timeout(v->cvp_wait,
			(v->cvp_state == CMD_STATUS_SUCCESS),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;
fail:
	return -EINVAL;

}

static int voice_send_cvs_map_memory_cmd(struct voice_data *v)
{
	struct vss_map_memory_cmd cvs_map_mem_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	void *apr_cvs;
	u16 cvs_handle;

	/* get all cvs cal data */
	get_all_vocstrm_cal(&cal_block);
	if (cal_block.cal_size == 0)
		goto fail;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvs = v->apr_q6_cvs;

	if (!apr_cvs) {
		pr_err("%s: apr_cvs is NULL.\n", __func__);
		return -EINVAL;
	}
	cvs_handle = voice_get_cvs_handle(v);

	/* fill in the header */
	cvs_map_mem_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvs_map_mem_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvs_map_mem_cmd) - APR_HDR_SIZE);
	cvs_map_mem_cmd.hdr.src_port = 0;
	cvs_map_mem_cmd.hdr.dest_port = cvs_handle;
	cvs_map_mem_cmd.hdr.token = 0;
	cvs_map_mem_cmd.hdr.opcode = VSS_ICOMMON_CMD_MAP_MEMORY;

	pr_debug("%s, phys_addr: %d, mem_size: %d\n", __func__,
		cal_block.cal_paddr, cal_block.cal_size);
	cvs_map_mem_cmd.vss_map_mem.phys_addr = cal_block.cal_paddr;
	cvs_map_mem_cmd.vss_map_mem.mem_size = cal_block.cal_size;
	cvs_map_mem_cmd.vss_map_mem.mem_pool_id =
				VSS_ICOMMON_MAP_MEMORY_SHMEM8_4K_POOL;

	v->cvs_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_map_mem_cmd);
	if (ret < 0) {
		pr_err("Fail: sending cvs cal,\n");
		goto fail;
	}
	ret = wait_event_timeout(v->cvs_wait,
			(v->cvs_state == CMD_STATUS_SUCCESS),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;
fail:
	return -EINVAL;

}

static int voice_send_cvs_unmap_memory_cmd(struct voice_data *v)
{
	struct vss_unmap_memory_cmd cvs_unmap_mem_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	void *apr_cvs;
	u16 cvs_handle;

	get_all_vocstrm_cal(&cal_block);
	if (cal_block.cal_size == 0)
		return 0;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvs = v->apr_q6_cvs;

	if (!apr_cvs) {
		pr_err("%s: apr_cvs is NULL.\n", __func__);
		return -EINVAL;
	}
	cvs_handle = voice_get_cvs_handle(v);

	/* fill in the header */
	cvs_unmap_mem_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvs_unmap_mem_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvs_unmap_mem_cmd) - APR_HDR_SIZE);
	cvs_unmap_mem_cmd.hdr.src_port = 0;
	cvs_unmap_mem_cmd.hdr.dest_port = cvs_handle;
	cvs_unmap_mem_cmd.hdr.token = 0;
	cvs_unmap_mem_cmd.hdr.opcode = VSS_ICOMMON_CMD_UNMAP_MEMORY;

	cvs_unmap_mem_cmd.vss_unmap_mem.phys_addr = cal_block.cal_paddr;

	v->cvs_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_unmap_mem_cmd);
	if (ret < 0) {
		pr_err("Fail: sending cvs cal,\n");
		goto fail;
	}
	ret = wait_event_timeout(v->cvs_wait,
			(v->cvs_state == CMD_STATUS_SUCCESS),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;
fail:
	return -EINVAL;

}

static int voice_send_cvp_register_cal_cmd(struct voice_data *v)
{
	struct cvp_register_cal_data_cmd cvp_reg_cal_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	void *apr_cvp;
	u16 cvp_handle;

      /* get the cvp cal data */
	get_all_vocproc_cal(&cal_block);
	if (cal_block.cal_size == 0)
		goto fail;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvp = v->apr_q6_cvp;

	if (!apr_cvp) {
		pr_err("%s: apr_cvp is NULL.\n", __func__);
		return -EINVAL;
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_reg_cal_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_reg_cal_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_reg_cal_cmd) - APR_HDR_SIZE);
	cvp_reg_cal_cmd.hdr.src_port = 0;
	cvp_reg_cal_cmd.hdr.dest_port = cvp_handle;
	cvp_reg_cal_cmd.hdr.token = 0;
	cvp_reg_cal_cmd.hdr.opcode = VSS_IVOCPROC_CMD_REGISTER_CALIBRATION_DATA;

	cvp_reg_cal_cmd.cvp_cal_data.phys_addr = cal_block.cal_paddr;
	cvp_reg_cal_cmd.cvp_cal_data.mem_size = cal_block.cal_size;

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvp, (uint32_t *) &cvp_reg_cal_cmd);
	if (ret < 0) {
		pr_err("Fail: sending cvp cal,\n");
		goto fail;
	}
	ret = wait_event_timeout(v->cvp_wait,
			(v->cvp_state == CMD_STATUS_SUCCESS),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;
fail:
	return -EINVAL;

}

static int voice_send_cvp_deregister_cal_cmd(struct voice_data *v)
{
	struct cvp_deregister_cal_data_cmd cvp_dereg_cal_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	void *apr_cvp;
	u16 cvp_handle;

	get_all_vocproc_cal(&cal_block);
	if (cal_block.cal_size == 0)
		return 0;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvp = v->apr_q6_cvp;

	if (!apr_cvp) {
		pr_err("%s: apr_cvp is NULL.\n", __func__);
		return -EINVAL;
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_dereg_cal_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_dereg_cal_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_dereg_cal_cmd) - APR_HDR_SIZE);
	cvp_dereg_cal_cmd.hdr.src_port = 0;
	cvp_dereg_cal_cmd.hdr.dest_port = cvp_handle;
	cvp_dereg_cal_cmd.hdr.token = 0;
	cvp_dereg_cal_cmd.hdr.opcode =
			VSS_IVOCPROC_CMD_DEREGISTER_CALIBRATION_DATA;

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvp, (uint32_t *) &cvp_dereg_cal_cmd);
	if (ret < 0) {
		pr_err("Fail: sending cvp cal,\n");
		goto fail;
	}
	ret = wait_event_timeout(v->cvp_wait,
			(v->cvp_state == CMD_STATUS_SUCCESS),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;
fail:
	return -EINVAL;

}

static int voice_send_cvp_register_vol_cal_table_cmd(struct voice_data *v)
{
	struct cvp_register_vol_cal_table_cmd cvp_reg_cal_tbl_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	void *apr_cvp;
	u16 cvp_handle;

	/* get the cvp vol cal data */
	get_all_vocvol_cal(&cal_block);
	if (cal_block.cal_size == 0)
		goto fail;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvp = v->apr_q6_cvp;

	if (!apr_cvp) {
		pr_err("%s: apr_cvp is NULL.\n", __func__);
		return -EINVAL;
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_reg_cal_tbl_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_reg_cal_tbl_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_reg_cal_tbl_cmd) - APR_HDR_SIZE);
	cvp_reg_cal_tbl_cmd.hdr.src_port = 0;
	cvp_reg_cal_tbl_cmd.hdr.dest_port = cvp_handle;
	cvp_reg_cal_tbl_cmd.hdr.token = 0;
	cvp_reg_cal_tbl_cmd.hdr.opcode =
				VSS_IVOCPROC_CMD_REGISTER_VOLUME_CAL_TABLE;

	cvp_reg_cal_tbl_cmd.cvp_vol_cal_tbl.phys_addr = cal_block.cal_paddr;
	cvp_reg_cal_tbl_cmd.cvp_vol_cal_tbl.mem_size = cal_block.cal_size;

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvp, (uint32_t *) &cvp_reg_cal_tbl_cmd);
	if (ret < 0) {
		pr_err("Fail: sending cvp cal table,\n");
		goto fail;
	}
	ret = wait_event_timeout(v->cvp_wait,
			(v->cvp_state == CMD_STATUS_SUCCESS),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;
fail:
	return -EINVAL;

}

static int voice_send_cvp_deregister_vol_cal_table_cmd(struct voice_data *v)
{
	struct cvp_deregister_vol_cal_table_cmd cvp_dereg_cal_tbl_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	void *apr_cvp;
	u16 cvp_handle;

	get_all_vocvol_cal(&cal_block);
	if (cal_block.cal_size == 0)
		return 0;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvp = v->apr_q6_cvp;

	if (!apr_cvp) {
		pr_err("%s: apr_cvp is NULL.\n", __func__);
		return -EINVAL;
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_dereg_cal_tbl_cmd.hdr.hdr_field = APR_HDR_FIELD(
						APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	cvp_dereg_cal_tbl_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_dereg_cal_tbl_cmd) - APR_HDR_SIZE);
	cvp_dereg_cal_tbl_cmd.hdr.src_port = 0;
	cvp_dereg_cal_tbl_cmd.hdr.dest_port = cvp_handle;
	cvp_dereg_cal_tbl_cmd.hdr.token = 0;
	cvp_dereg_cal_tbl_cmd.hdr.opcode =
				VSS_IVOCPROC_CMD_DEREGISTER_VOLUME_CAL_TABLE;

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvp, (uint32_t *) &cvp_dereg_cal_tbl_cmd);
	if (ret < 0) {
		pr_err("Fail: sending cvp cal table,\n");
		goto fail;
	}
	ret = wait_event_timeout(v->cvp_wait,
			(v->cvp_state == CMD_STATUS_SUCCESS),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;
fail:
	return -EINVAL;

}
static int voice_setup_vocproc(struct voice_data *v)
{
	struct cvp_create_full_ctl_session_cmd cvp_session_cmd;
	int ret = 0;
	void *apr_cvp;
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvp = v->apr_q6_cvp;

	if (!apr_cvp) {
		pr_err("%s: apr_cvp is NULL.\n", __func__);
		return -EINVAL;
	}

	/* create cvp session and wait for response */
	cvp_session_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	cvp_session_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_session_cmd) - APR_HDR_SIZE);
	pr_debug(" send create cvp session, pkt size = %d\n",
		cvp_session_cmd.hdr.pkt_size);
	cvp_session_cmd.hdr.src_port = 0;
	cvp_session_cmd.hdr.dest_port = 0;
	cvp_session_cmd.hdr.token = 0;
	cvp_session_cmd.hdr.opcode =
			VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION;

	/* Use default topology if invalid value in ACDB */
	cvp_session_cmd.cvp_session.tx_topology_id =
				get_voice_tx_topology();
	if (cvp_session_cmd.cvp_session.tx_topology_id == 0)
		cvp_session_cmd.cvp_session.tx_topology_id =
			VSS_IVOCPROC_TOPOLOGY_ID_TX_SM_ECNS;

	cvp_session_cmd.cvp_session.rx_topology_id =
				get_voice_rx_topology();
	if (cvp_session_cmd.cvp_session.rx_topology_id == 0)
		cvp_session_cmd.cvp_session.rx_topology_id =
			VSS_IVOCPROC_TOPOLOGY_ID_RX_DEFAULT;

	cvp_session_cmd.cvp_session.direction = 2; /*tx and rx*/
	cvp_session_cmd.cvp_session.network_id = VSS_NETWORK_ID_DEFAULT;
	cvp_session_cmd.cvp_session.tx_port_id = v->dev_tx.port_id;
	cvp_session_cmd.cvp_session.rx_port_id = v->dev_rx.port_id;

	pr_debug("topology=%d net_id=%d, dir=%d tx_port_id=%d, rx_port_id=%d\n",
		cvp_session_cmd.cvp_session.tx_topology_id,
		cvp_session_cmd.cvp_session.network_id,
		cvp_session_cmd.cvp_session.direction,
		cvp_session_cmd.cvp_session.tx_port_id,
		cvp_session_cmd.cvp_session.rx_port_id);

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvp, (uint32_t *) &cvp_session_cmd);
	if (ret < 0) {
		pr_err("Fail in sending VOCPROC_FULL_CONTROL_SESSION\n");
		goto fail;
	}
	ret = wait_event_timeout(v->cvp_wait,
				 (v->cvp_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	/* send cvs cal */
	ret = voice_send_cvs_map_memory_cmd(v);
	if (!ret)
		voice_send_cvs_register_cal_cmd(v);

	/* send cvp and vol cal */
	ret = voice_send_cvp_map_memory_cmd(v);
	if (!ret) {
		voice_send_cvp_register_cal_cmd(v);
		voice_send_cvp_register_vol_cal_table_cmd(v);
	}

	/* enable vocproc */
	ret = voice_send_enable_vocproc_cmd(v);
	if (ret < 0)
		goto fail;

	/* attach vocproc */
	ret = voice_send_attach_vocproc_cmd(v);
	if (ret < 0)
		goto fail;

	/* send tty mode if tty device is used */
	voice_send_tty_mode_cmd(v);

	if (v->voc_path == VOC_PATH_FULL)
		voice_send_netid_timing_cmd(v);

	return 0;

fail:
	return -EINVAL;
}

static int voice_send_enable_vocproc_cmd(struct voice_data *v)
{
	int ret = 0;
	struct apr_hdr cvp_enable_cmd;
	void *apr_cvp;
	u16 cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvp = v->apr_q6_cvp;

	if (!apr_cvp) {
		pr_err("%s: apr_cvp is NULL.\n", __func__);
		return -EINVAL;
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* enable vocproc and wait for respose */
	cvp_enable_cmd.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	cvp_enable_cmd.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_enable_cmd) - APR_HDR_SIZE);
	pr_debug("cvp_enable_cmd pkt size = %d, cvp_handle=%d\n",
		cvp_enable_cmd.pkt_size, cvp_handle);
	cvp_enable_cmd.src_port = 0;
	cvp_enable_cmd.dest_port = cvp_handle;
	cvp_enable_cmd.token = 0;
	cvp_enable_cmd.opcode = VSS_IVOCPROC_CMD_ENABLE;

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvp, (uint32_t *) &cvp_enable_cmd);
	if (ret < 0) {
		pr_err("Fail in sending VSS_IVOCPROC_CMD_ENABLE\n");
		goto fail;
	}
	ret = wait_event_timeout(v->cvp_wait,
				(v->cvp_state == CMD_STATUS_SUCCESS),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	return 0;
fail:
	return -EINVAL;
}

static int voice_send_netid_timing_cmd(struct voice_data *v)
{
	int ret = 0;
	void *apr_mvm;
	u16 mvm_handle;
	struct mvm_set_network_cmd mvm_set_network;
	struct mvm_set_voice_timing_cmd mvm_set_voice_timing;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_mvm = v->apr_q6_mvm;

	if (!apr_mvm) {
		pr_err("%s: apr_mvm is NULL.\n", __func__);
		return -EINVAL;
	}
	mvm_handle = voice_get_mvm_handle(v);

	ret = voice_config_cvs_vocoder(v);
	if (ret < 0) {
		pr_err("%s: Error %d configuring CVS voc",
					__func__, ret);
		goto fail;
	}
	/* Set network ID. */
	pr_debug("Setting network ID\n");

	mvm_set_network.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	mvm_set_network.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(mvm_set_network) - APR_HDR_SIZE);
	mvm_set_network.hdr.src_port = 0;
	mvm_set_network.hdr.dest_port = mvm_handle;
	mvm_set_network.hdr.token = 0;
	mvm_set_network.hdr.opcode = VSS_ICOMMON_CMD_SET_NETWORK;
	mvm_set_network.network.network_id = v->mvs_info.network_type;

	v->mvm_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_mvm, (uint32_t *) &mvm_set_network);
	if (ret < 0) {
		pr_err("%s: Error %d sending SET_NETWORK\n", __func__, ret);
		goto fail;
	}

	ret = wait_event_timeout(v->mvm_wait,
				(v->mvm_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	/* Set voice timing. */
	 pr_debug("Setting voice timing\n");

	mvm_set_voice_timing.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	mvm_set_voice_timing.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
						sizeof(mvm_set_voice_timing) -
						APR_HDR_SIZE);
	mvm_set_voice_timing.hdr.src_port = 0;
	mvm_set_voice_timing.hdr.dest_port = mvm_handle;
	mvm_set_voice_timing.hdr.token = 0;
	mvm_set_voice_timing.hdr.opcode = VSS_ICOMMON_CMD_SET_VOICE_TIMING;
	mvm_set_voice_timing.timing.mode = 0;
	mvm_set_voice_timing.timing.enc_offset = 8000;
	mvm_set_voice_timing.timing.dec_req_offset = 3300;
	mvm_set_voice_timing.timing.dec_offset = 8300;

	v->mvm_state = CMD_STATUS_FAIL;

	ret = apr_send_pkt(apr_mvm, (uint32_t *) &mvm_set_voice_timing);
	if (ret < 0) {
		pr_err("%s: Error %d sending SET_TIMING\n", __func__, ret);
		goto fail;
	}

	ret = wait_event_timeout(v->mvm_wait,
				(v->mvm_state == CMD_STATUS_SUCCESS),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	return 0;
fail:
	return -EINVAL;
}

static int voice_send_attach_vocproc_cmd(struct voice_data *v)
{
	int ret = 0;
	struct mvm_attach_vocproc_cmd mvm_a_vocproc_cmd;
	void *apr_mvm;
	u16 mvm_handle, cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_mvm = v->apr_q6_mvm;

	if (!apr_mvm) {
		pr_err("%s: apr_mvm is NULL.\n", __func__);
		return -EINVAL;
	}
	mvm_handle = voice_get_mvm_handle(v);
	cvp_handle = voice_get_cvp_handle(v);

	/* attach vocproc and wait for response */
	mvm_a_vocproc_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	mvm_a_vocproc_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(mvm_a_vocproc_cmd) - APR_HDR_SIZE);
	pr_debug("send mvm_a_vocproc_cmd pkt size = %d\n",
		mvm_a_vocproc_cmd.hdr.pkt_size);
	mvm_a_vocproc_cmd.hdr.src_port = 0;
	mvm_a_vocproc_cmd.hdr.dest_port = mvm_handle;
	mvm_a_vocproc_cmd.hdr.token = 0;
	mvm_a_vocproc_cmd.hdr.opcode = VSS_IMVM_CMD_ATTACH_VOCPROC;
	mvm_a_vocproc_cmd.mvm_attach_cvp_handle.handle = cvp_handle;

	v->mvm_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_mvm, (uint32_t *) &mvm_a_vocproc_cmd);
	if (ret < 0) {
		pr_err("Fail in sending VSS_IMVM_CMD_ATTACH_VOCPROC\n");
		goto fail;
	}
	ret = wait_event_timeout(v->mvm_wait,
				 (v->mvm_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	return 0;
fail:
	return -EINVAL;
}

static int voice_destroy_vocproc(struct voice_data *v)
{
	struct mvm_detach_vocproc_cmd mvm_d_vocproc_cmd;
	struct apr_hdr cvp_destroy_session_cmd;
	int ret = 0;
	void *apr_mvm, *apr_cvp;
	u16 mvm_handle, cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_mvm = v->apr_q6_mvm;
	apr_cvp = v->apr_q6_cvp;

	if (!apr_mvm || !apr_cvp) {
		pr_err("%s: apr_mvm or apr_cvp is NULL.\n", __func__);
		return -EINVAL;
	}
	mvm_handle = voice_get_mvm_handle(v);
	cvp_handle = voice_get_cvp_handle(v);

	/* send stop voice cmd */
	voice_send_stop_voice_cmd(v);

	/* detach VOCPROC and wait for response from mvm */
	mvm_d_vocproc_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	mvm_d_vocproc_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(mvm_d_vocproc_cmd) - APR_HDR_SIZE);
	pr_debug("mvm_d_vocproc_cmd  pkt size = %d\n",
		mvm_d_vocproc_cmd.hdr.pkt_size);
	mvm_d_vocproc_cmd.hdr.src_port = 0;
	mvm_d_vocproc_cmd.hdr.dest_port = mvm_handle;
	mvm_d_vocproc_cmd.hdr.token = 0;
	mvm_d_vocproc_cmd.hdr.opcode = VSS_IMVM_CMD_DETACH_VOCPROC;
	mvm_d_vocproc_cmd.mvm_detach_cvp_handle.handle = cvp_handle;

	v->mvm_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_mvm, (uint32_t *) &mvm_d_vocproc_cmd);
	if (ret < 0) {
		pr_err("Fail in sending VSS_IMVM_CMD_DETACH_VOCPROC\n");
		goto fail;
	}
	ret = wait_event_timeout(v->mvm_wait,
				 (v->mvm_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	/* deregister cvp and vol cal */
	voice_send_cvp_deregister_vol_cal_table_cmd(v);
	voice_send_cvp_deregister_cal_cmd(v);
	voice_send_cvp_unmap_memory_cmd(v);

	/* deregister cvs cal */
	voice_send_cvs_deregister_cal_cmd(v);
	voice_send_cvs_unmap_memory_cmd(v);

	/* destrop cvp session */
	cvp_destroy_session_cmd.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	cvp_destroy_session_cmd.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_destroy_session_cmd) - APR_HDR_SIZE);
	pr_debug("cvp_destroy_session_cmd pkt size = %d\n",
		cvp_destroy_session_cmd.pkt_size);
	cvp_destroy_session_cmd.src_port = 0;
	cvp_destroy_session_cmd.dest_port = cvp_handle;
	cvp_destroy_session_cmd.token = 0;
	cvp_destroy_session_cmd.opcode = APRV2_IBASIC_CMD_DESTROY_SESSION;

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvp, (uint32_t *) &cvp_destroy_session_cmd);
	if (ret < 0) {
		pr_err("Fail in sending APRV2_IBASIC_CMD_DESTROY_SESSION\n");
		goto fail;
	}
	ret = wait_event_timeout(v->cvp_wait,
				 (v->cvp_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	cvp_handle = 0;
	voice_set_cvp_handle(v, cvp_handle);

	return 0;

fail:
	return -EINVAL;
}

static int voice_send_mute_cmd(struct voice_data *v)
{
	struct cvs_set_mute_cmd cvs_mute_cmd;
	int ret = 0;
	void *apr_cvs;
	u16 cvs_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvs = v->apr_q6_cvs;

	if (!apr_cvs) {
		pr_err("%s: apr_cvs is NULL.\n", __func__);
		return -EINVAL;
	}
	cvs_handle = voice_get_cvs_handle(v);

	/* send mute/unmute to cvs */
	cvs_mute_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	cvs_mute_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(cvs_mute_cmd) - APR_HDR_SIZE);
	cvs_mute_cmd.hdr.src_port = 0;
	cvs_mute_cmd.hdr.dest_port = cvs_handle;
	cvs_mute_cmd.hdr.token = 0;
	cvs_mute_cmd.hdr.opcode = VSS_ISTREAM_CMD_SET_MUTE;
	cvs_mute_cmd.cvs_set_mute.direction = 0; /*tx*/
	cvs_mute_cmd.cvs_set_mute.mute_flag = v->dev_tx.mute;

	pr_info(" mute value =%d\n", cvs_mute_cmd.cvs_set_mute.mute_flag);
	v->cvs_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_mute_cmd);
	if (ret < 0) {
		pr_err("Fail: send STREAM SET MUTE\n");
		goto fail;
	}
	ret = wait_event_timeout(v->cvs_wait,
				 (v->cvs_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret)
		pr_err("%s: wait_event timeout\n", __func__);

	return 0;
fail:
	return -EINVAL;
}

static int voice_send_vol_index_cmd(struct voice_data *v)
{
	struct cvp_set_rx_volume_index_cmd cvp_vol_cmd;
	int ret = 0;
	void *apr_cvp;
	u16 cvp_handle;
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvp = v->apr_q6_cvp;

	if (!apr_cvp) {
		pr_err("%s: apr_cvp is NULL.\n", __func__);
		return -EINVAL;
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* send volume index to cvp */
	cvp_vol_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	cvp_vol_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(cvp_vol_cmd) - APR_HDR_SIZE);
	cvp_vol_cmd.hdr.src_port = 0;
	cvp_vol_cmd.hdr.dest_port = cvp_handle;
	cvp_vol_cmd.hdr.token = 0;
	cvp_vol_cmd.hdr.opcode = VSS_IVOCPROC_CMD_SET_RX_VOLUME_INDEX;
	cvp_vol_cmd.cvp_set_vol_idx.vol_index = v->dev_rx.volume;
	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvp, (uint32_t *) &cvp_vol_cmd);
	if (ret < 0) {
		pr_err("Fail in sending RX VOL INDEX\n");
		return -EINVAL;
	}
	ret = wait_event_timeout(v->cvp_wait,
				 (v->cvp_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		return -EINVAL;
	}
	return 0;
}

int voc_disable_cvp(void)
{
	struct voice_data *v = &voice;
	int ret = 0;

	mutex_lock(&v->lock);

	if (v->voc_state == VOC_RUN) {
		afe_sidetone(v->dev_tx.port_id, v->dev_rx.port_id, 0, 0);
		/* send cmd to dsp to disable vocproc */
		ret = voice_send_disable_vocproc_cmd(v);
		if (ret < 0) {
			pr_err("%s:  disable vocproc failed\n", __func__);
			goto fail;
		}

		/* deregister cvp and vol cal */
		voice_send_cvp_deregister_vol_cal_table_cmd(v);
		voice_send_cvp_deregister_cal_cmd(v);
		voice_send_cvp_unmap_memory_cmd(v);

		v->voc_state = VOC_CHANGE;
	}

fail:	mutex_unlock(&v->lock);

	return ret;
}

int voc_enable_cvp(void)
{
	struct voice_data *v = &voice;
	struct sidetone_cal sidetone_cal_data;
	int ret = 0;

	mutex_lock(&v->lock);

	if (v->voc_state == VOC_CHANGE) {
		ret = voice_send_set_device_cmd(v);
		if (ret < 0) {
			pr_err("%s:  set device failed\n", __func__);
			goto fail;
		}
		/* send cvp and vol cal */
		ret = voice_send_cvp_map_memory_cmd(v);
		if (!ret) {
			voice_send_cvp_register_cal_cmd(v);
			voice_send_cvp_register_vol_cal_table_cmd(v);
		}
		ret = voice_send_enable_vocproc_cmd(v);
		if (ret < 0) {
			pr_err("enable vocproc failed\n");
			goto fail;

		}
		/* send tty mode if tty device is used */
		voice_send_tty_mode_cmd(v);

		get_sidetone_cal(&sidetone_cal_data);
		ret = afe_sidetone(v->dev_tx.port_id, v->dev_rx.port_id,
						sidetone_cal_data.enable,
						sidetone_cal_data.gain);

		if (ret < 0)
			pr_err("AFE command sidetone failed\n");

		v->voc_state = VOC_RUN;
	}

fail:
	mutex_unlock(&v->lock);

	return ret;
}

int voc_set_tx_mute(uint32_t dir, uint32_t mute)
{
	struct voice_data *v = &voice;
	int ret = 0;

	mutex_lock(&v->lock);

	v->dev_tx.mute = mute;

	if (v->voc_state == VOC_RUN)
		ret = voice_send_mute_cmd(v);

	mutex_unlock(&v->lock);

	return ret;
}

int voc_set_tty_mode(uint8_t tty_mode)
{
	struct voice_data *v = &voice;
	int ret = 0;

	mutex_lock(&v->lock);

	v->tty_mode = tty_mode;

	mutex_unlock(&v->lock);

	return ret;
}

uint8_t voc_get_tty_mode(void)
{
	struct voice_data *v = &voice;
	int ret = 0;

	mutex_lock(&v->lock);

	ret = v->tty_mode;

	mutex_unlock(&v->lock);

	return ret;
}

int voc_set_rx_vol_index(uint32_t dir, uint32_t vol_idx)
{
	struct voice_data *v = &voice;
	int ret = 0;

	mutex_lock(&v->lock);

	v->dev_rx.volume = vol_idx;

	if (v->voc_state == VOC_RUN)
		ret = voice_send_vol_index_cmd(v);

	mutex_unlock(&v->lock);

	return ret;
}

void voc_set_rxtx_port(uint32_t port_id, uint32_t dev_type)
{
	struct voice_data *v = &voice;

	pr_debug(" port_id=%d, type=%d\n", port_id, dev_type);

	mutex_lock(&v->lock);

	if (dev_type == DEV_RX)
		v->dev_rx.port_id = port_id;
	else
		v->dev_tx.port_id = port_id;

	mutex_unlock(&v->lock);

	return;
}

void voc_set_route_flag(uint8_t path_dir, uint8_t set)
{
	struct voice_data *v = &voice;

	pr_debug("path_dir=%d, set=%d\n", path_dir, set);

	mutex_lock(&v->lock);

	if (path_dir == RX_PATH)
		v->voc_route_state.rx_route_flag = set;
	else
		v->voc_route_state.tx_route_flag = set;

	mutex_unlock(&v->lock);

	return;
}

uint8_t voc_get_route_flag(uint8_t path_dir)
{
	struct voice_data *v = &voice;
	int ret = 0;

	mutex_lock(&v->lock);

	if (path_dir == RX_PATH)
		ret = v->voc_route_state.rx_route_flag;
	else
		ret = v->voc_route_state.tx_route_flag;

	mutex_unlock(&v->lock);

	return ret;
}

int voc_end_voice_call(void)
{
	struct voice_data *v = &voice;
	int ret = 0;

	mutex_lock(&v->lock);

	if (v->voc_state == VOC_RUN) {
		afe_sidetone(v->dev_tx.port_id, v->dev_rx.port_id, 0, 0);
		ret = voice_destroy_vocproc(v);
		if (ret < 0)
			pr_err("%s:  destroy voice failed\n", __func__);
		voice_destroy_mvm_cvs_session(v);

		v->voc_state = VOC_RELEASE;
	}
	mutex_unlock(&v->lock);
	return ret;
}

int voc_start_voice_call(void)
{
	struct voice_data *v = &voice;
	struct sidetone_cal sidetone_cal_data;
	int ret = 0;

	mutex_lock(&v->lock);

	if ((v->voc_state == VOC_INIT) ||
		(v->voc_state == VOC_RELEASE)) {
		ret = voice_apr_register(v);
		if (ret < 0) {
			pr_err("%s:  apr register failed\n", __func__);
			goto fail;
		}
		ret = voice_create_mvm_cvs_session(v);
		if (ret < 0) {
			pr_err("create mvm and cvs failed\n");
			goto fail;
		}
		ret = voice_setup_vocproc(v);
		if (ret < 0) {
			pr_err("setup voice failed\n");
			goto fail;
		}
		ret = voice_send_start_voice_cmd(v);
		if (ret < 0) {
			pr_err("start voice failed\n");
			goto fail;
		}
		get_sidetone_cal(&sidetone_cal_data);
		ret = afe_sidetone(v->dev_tx.port_id,
					v->dev_rx.port_id,
					sidetone_cal_data.enable,
					sidetone_cal_data.gain);
		if (ret < 0)
			pr_err("AFE command sidetone failed\n");

		v->voc_state = VOC_RUN;
	}

fail:	mutex_unlock(&v->lock);
	return ret;
}

int voc_set_voc_path_full(uint32_t set)
{
	int rc = 0;

	pr_debug("set voc path: %d\n", set);

	mutex_lock(&voice.lock);

	if (voice.voc_state == VOC_INIT || voice.voc_state == VOC_RELEASE) {
		if (set)
			voice.voc_path = VOC_PATH_FULL;
		else
			voice.voc_path = VOC_PATH_PASSIVE;
	} else {
		pr_err("%s: Invalid voc path set to %d, in state %d\n",
		       __func__, set, voice.voc_state);
		rc = -EPERM;
	}

	mutex_unlock(&voice.lock);

	return rc;
}
EXPORT_SYMBOL(voc_set_voc_path_full);

void voc_register_mvs_cb(ul_cb_fn ul_cb,
			   dl_cb_fn dl_cb,
			   void *private_data)
{
	voice.mvs_info.ul_cb = ul_cb;
	voice.mvs_info.dl_cb = dl_cb;
	voice.mvs_info.private_data = private_data;
}

void voc_config_vocoder(uint32_t media_type,
			  uint32_t rate,
			  uint32_t network_type)
{
	voice.mvs_info.media_type = media_type;
	voice.mvs_info.rate = rate;
	voice.mvs_info.network_type = network_type;
}

static int32_t qdsp_mvm_callback(struct apr_client_data *data, void *priv)
{
	uint32_t *ptr;
	struct voice_data *v;

	if ((data == NULL) || (priv == NULL)) {
		pr_err("%s: data or priv is NULL\n", __func__);
		return -EINVAL;
	}
	v = priv;

	pr_debug("%s: Payload Length = %d, opcode=%x\n", __func__,
		data->payload_size, data->opcode);

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		if (data->payload_size) {
			ptr = data->payload;

			pr_info("%x %x\n", ptr[0], ptr[1]);
			/* ping mvm service ACK */
			switch (ptr[0]) {
			case VSS_IMVM_CMD_CREATE_PASSIVE_CONTROL_SESSION:
			case VSS_IMVM_CMD_CREATE_FULL_CONTROL_SESSION:
				/* Passive session is used for CS call
				 * Full session is used for VoIP call. */
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				if (!ptr[1]) {
					pr_debug("%s: MVM handle is %d\n",
						 __func__, data->src_port);
					voice_set_mvm_handle(v, data->src_port);
				} else
					pr_err("got NACK for sending \
						MVM create session \n");
				v->mvm_state = CMD_STATUS_SUCCESS;
				wake_up(&v->mvm_wait);
				break;
			case VSS_IMVM_CMD_START_VOICE:
			case VSS_IMVM_CMD_ATTACH_VOCPROC:
			case VSS_IMVM_CMD_STOP_VOICE:
			case VSS_IMVM_CMD_DETACH_VOCPROC:
			case VSS_ISTREAM_CMD_SET_TTY_MODE:
			case APRV2_IBASIC_CMD_DESTROY_SESSION:
			case VSS_IMVM_CMD_ATTACH_STREAM:
			case VSS_IMVM_CMD_DETACH_STREAM:
			case VSS_ICOMMON_CMD_SET_NETWORK:
			case VSS_ICOMMON_CMD_SET_VOICE_TIMING:
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				v->mvm_state = CMD_STATUS_SUCCESS;
				wake_up(&v->mvm_wait);
				break;
			default:
				pr_debug("%s: not match cmd = 0x%x\n",
					__func__, ptr[0]);
				break;
			}
		}
	}

	return 0;
}

static int32_t qdsp_cvs_callback(struct apr_client_data *data, void *priv)
{
	uint32_t *ptr;
	struct voice_data *v;

	if ((data == NULL) || (priv == NULL)) {
		pr_err("%s: data or priv is NULL\n", __func__);
		return -EINVAL;
	}
	v = priv;

	pr_debug("%s: Payload Length = %d, opcode=%x\n", __func__,
		data->payload_size, data->opcode);

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		if (data->payload_size) {
			ptr = data->payload;

			pr_info("%x %x\n", ptr[0], ptr[1]);
			/*response from  CVS */
			switch (ptr[0]) {
			case VSS_ISTREAM_CMD_CREATE_PASSIVE_CONTROL_SESSION:
			case VSS_ISTREAM_CMD_CREATE_FULL_CONTROL_SESSION:
				if (!ptr[1]) {
					pr_debug("%s: CVS handle is %d\n",
						 __func__, data->src_port);
					voice_set_cvs_handle(v, data->src_port);
				} else
					pr_err("got NACK for sending \
						CVS create session \n");
				v->cvs_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvs_wait);
				break;
			case VSS_ISTREAM_CMD_SET_MUTE:
			case VSS_ISTREAM_CMD_SET_MEDIA_TYPE:
			case VSS_ISTREAM_CMD_VOC_AMR_SET_ENC_RATE:
			case VSS_ISTREAM_CMD_VOC_AMRWB_SET_ENC_RATE:
			case VSS_ISTREAM_CMD_SET_ENC_DTX_MODE:
			case VSS_ISTREAM_CMD_CDMA_SET_ENC_MINMAX_RATE:
			case APRV2_IBASIC_CMD_DESTROY_SESSION:
			case VSS_ISTREAM_CMD_REGISTER_CALIBRATION_DATA:
			case VSS_ISTREAM_CMD_DEREGISTER_CALIBRATION_DATA:
			case VSS_ICOMMON_CMD_MAP_MEMORY:
			case VSS_ICOMMON_CMD_UNMAP_MEMORY:
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				v->cvs_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvs_wait);
				break;
			default:
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				break;
			}
		}
	} else if (data->opcode == VSS_ISTREAM_EVT_SEND_ENC_BUFFER) {
		uint32_t *voc_pkt = data->payload;
		uint32_t pkt_len = data->payload_size;

		if (voc_pkt != NULL && v->mvs_info.ul_cb != NULL) {
			pr_debug("%s: Media type is 0x%x\n",
				 __func__, voc_pkt[0]);

			/* Remove media ID from payload. */
			voc_pkt++;
			pkt_len = pkt_len - 4;

			v->mvs_info.ul_cb((uint8_t *)voc_pkt,
					  pkt_len,
					  v->mvs_info.private_data);
		} else
			pr_err("%s: voc_pkt is 0x%x ul_cb is 0x%x\n",
			       __func__, (unsigned int)voc_pkt,
			       (unsigned int) v->mvs_info.ul_cb);
	} else if (data->opcode == VSS_ISTREAM_EVT_REQUEST_DEC_BUFFER) {
		struct cvs_send_dec_buf_cmd send_dec_buf;
		int ret = 0;
		uint32_t pkt_len = 0;

		if (v->mvs_info.dl_cb != NULL) {
			send_dec_buf.dec_buf.media_id = v->mvs_info.media_type;

			v->mvs_info.dl_cb(
				(uint8_t *)&send_dec_buf.dec_buf.packet_data,
				&pkt_len,
				v->mvs_info.private_data);

			send_dec_buf.hdr.hdr_field =
					APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);
			send_dec_buf.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
			       sizeof(send_dec_buf.dec_buf.media_id) + pkt_len);
			send_dec_buf.hdr.src_port = 0;
			send_dec_buf.hdr.dest_port = voice_get_cvs_handle(v);
			send_dec_buf.hdr.token = 0;
			send_dec_buf.hdr.opcode =
					VSS_ISTREAM_EVT_SEND_DEC_BUFFER;

			ret = apr_send_pkt(v->apr_q6_cvs,
					   (uint32_t *) &send_dec_buf);
			if (ret < 0) {
				pr_err("%s: Error %d sending DEC_BUF\n",
				       __func__, ret);
				goto fail;
			}
		} else
			pr_debug("%s: dl_cb is NULL\n", __func__);
	} else if (data->opcode == VSS_ISTREAM_EVT_SEND_DEC_BUFFER)
		pr_debug("Send dec buf resp\n");

	else
		pr_debug("Unknown opcode 0x%x\n", data->opcode);

fail:
	return 0;
}

static int32_t qdsp_cvp_callback(struct apr_client_data *data, void *priv)
{
	uint32_t *ptr;
	struct voice_data *v;

	if ((data == NULL) || (priv == NULL)) {
		pr_err("%s: data or priv is NULL\n", __func__);
		return -EINVAL;
	}
	v = priv;

	pr_debug("%s: Payload Length = %d, opcode=%x\n", __func__,
		data->payload_size, data->opcode);

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		if (data->payload_size) {
			ptr = data->payload;

			pr_info("%x %x\n", ptr[0], ptr[1]);

			switch (ptr[0]) {
			case VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION:
			/*response from  CVP */
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				if (!ptr[1]) {
					voice_set_cvp_handle(v, data->src_port);
					pr_debug("cvphdl=%d\n", data->src_port);
				} else
					pr_err("got NACK from CVP create \
						session response\n");
				v->cvp_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvp_wait);
				break;
			case VSS_IVOCPROC_CMD_SET_DEVICE:
			case VSS_IVOCPROC_CMD_SET_RX_VOLUME_INDEX:
			case VSS_IVOCPROC_CMD_ENABLE:
			case VSS_IVOCPROC_CMD_DISABLE:
			case APRV2_IBASIC_CMD_DESTROY_SESSION:
			case VSS_IVOCPROC_CMD_REGISTER_VOLUME_CAL_TABLE:
			case VSS_IVOCPROC_CMD_DEREGISTER_VOLUME_CAL_TABLE:
			case VSS_IVOCPROC_CMD_REGISTER_CALIBRATION_DATA:
			case VSS_IVOCPROC_CMD_DEREGISTER_CALIBRATION_DATA:
			case VSS_ICOMMON_CMD_MAP_MEMORY:
			case VSS_ICOMMON_CMD_UNMAP_MEMORY:
				v->cvp_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvp_wait);
				break;
			default:
				pr_debug("%s: not match cmd = 0x%x\n",
					__func__, ptr[0]);
				break;
			}
		}
	}
	return 0;
}


static int __init voice_init(void)
{
	int rc = 0;
	struct voice_data *v = &voice;

	/* set default value */
	v->default_mute_val = 1;  /* default is mute */
	v->default_vol_val = 0;
	v->default_sample_val = 8000;
	v->sidetone_gain = 0x512;

	/* initialize dev_rx and dev_tx */
	memset(&v->dev_tx, 0, sizeof(struct device_data));
	memset(&v->dev_rx, 0, sizeof(struct device_data));
	v->dev_rx.volume = v->default_vol_val;
	v->dev_tx.mute = v->default_mute_val;

	v->dev_tx.port_id = 1;
	v->dev_rx.port_id = 0;

	v->tty_mode = 0;

	v->voc_state = VOC_INIT;
	v->voc_path = VOC_PATH_PASSIVE;
	init_waitqueue_head(&v->mvm_wait);
	init_waitqueue_head(&v->cvs_wait);
	init_waitqueue_head(&v->cvp_wait);

	mutex_init(&v->lock);

	v->mvm_full_handle = 0;
	v->mvm_passive_handle = 0;
	v->cvs_full_handle = 0;
	v->cvs_passive_handle = 0;
	v->cvp_full_handle = 0;
	v->cvp_passive_handle = 0;

	v->apr_q6_mvm = NULL;
	v->apr_q6_cvs = NULL;
	v->apr_q6_cvp = NULL;

	/* Initialize MVS info. */
	memset(&v->mvs_info, 0, sizeof(v->mvs_info));
	v->mvs_info.network_type = VSS_NETWORK_ID_DEFAULT;

	return rc;
}

device_initcall(voice_init);
