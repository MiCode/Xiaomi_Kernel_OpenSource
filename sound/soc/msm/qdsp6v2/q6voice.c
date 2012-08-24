/*  Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <asm/mach-types.h>
#include <mach/qdsp6v2/audio_acdb.h>
#include <mach/qdsp6v2/rtac.h>
#include <mach/socinfo.h>

#include "sound/apr_audio-v2.h"
#include "sound/q6afe-v2.h"

#include "q6voice.h"

#define TIMEOUT_MS 200


#define CMD_STATUS_SUCCESS 0
#define CMD_STATUS_FAIL 1

#define VOC_PATH_PASSIVE 0
#define VOC_PATH_FULL 1
#define VOC_PATH_VOLTE_PASSIVE 2

/* CVP CAL Size: 245760 = 240 * 1024 */
#define CVP_CAL_SIZE 245760
/* CVS CAL Size: 49152 = 48 * 1024 */
#define CVS_CAL_SIZE 49152

static struct common_data common;

static int voice_send_enable_vocproc_cmd(struct voice_data *v);
static int voice_send_netid_timing_cmd(struct voice_data *v);
static int voice_send_attach_vocproc_cmd(struct voice_data *v);
static int voice_send_set_device_cmd(struct voice_data *v);
static int voice_send_disable_vocproc_cmd(struct voice_data *v);
static int voice_send_vol_index_cmd(struct voice_data *v);
static int voice_cvs_stop_playback(struct voice_data *v);
static int voice_cvs_start_playback(struct voice_data *v);
static int voice_cvs_start_record(struct voice_data *v, uint32_t rec_mode);
static int voice_cvs_stop_record(struct voice_data *v);

static int32_t qdsp_mvm_callback(struct apr_client_data *data, void *priv);
static int32_t qdsp_cvs_callback(struct apr_client_data *data, void *priv);
static int32_t qdsp_cvp_callback(struct apr_client_data *data, void *priv);

static u16 voice_get_mvm_handle(struct voice_data *v)
{
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return 0;
	}

	pr_debug("%s: mvm_handle %d\n", __func__, v->mvm_handle);

	return v->mvm_handle;
}

static void voice_set_mvm_handle(struct voice_data *v, u16 mvm_handle)
{
	pr_debug("%s: mvm_handle %d\n", __func__, mvm_handle);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return;
	}

	v->mvm_handle = mvm_handle;
}

static u16 voice_get_cvs_handle(struct voice_data *v)
{
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return 0;
	}

	pr_debug("%s: cvs_handle %d\n", __func__, v->cvs_handle);

	return v->cvs_handle;
}

static void voice_set_cvs_handle(struct voice_data *v, u16 cvs_handle)
{
	pr_debug("%s: cvs_handle %d\n", __func__, cvs_handle);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return;
	}

	v->cvs_handle = cvs_handle;
}

static u16 voice_get_cvp_handle(struct voice_data *v)
{
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return 0;
	}

	pr_debug("%s: cvp_handle %d\n", __func__, v->cvp_handle);

	return v->cvp_handle;
}

static void voice_set_cvp_handle(struct voice_data *v, u16 cvp_handle)
{
	pr_debug("%s: cvp_handle %d\n", __func__, cvp_handle);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return;
	}

	v->cvp_handle = cvp_handle;
}

uint16_t voc_get_session_id(char *name)
{
	u16 session_id = 0;

	if (name != NULL) {
		if (!strncmp(name, "Voice session", 13))
			session_id = common.voice[VOC_PATH_PASSIVE].session_id;
		else if (!strncmp(name, "VoLTE session", 13))
			session_id =
			common.voice[VOC_PATH_VOLTE_PASSIVE].session_id;
		else
			session_id = common.voice[VOC_PATH_FULL].session_id;

		pr_debug("%s: %s has session id 0x%x\n", __func__, name,
				session_id);
	}

	return session_id;
}

static struct voice_data *voice_get_session(u16 session_id)
{
	struct voice_data *v = NULL;

	if ((session_id >= SESSION_ID_BASE) &&
	    (session_id < SESSION_ID_BASE + MAX_VOC_SESSIONS)) {
		v = &common.voice[session_id - SESSION_ID_BASE];
	}

	pr_debug("%s: session_id 0x%x session handle 0x%x\n",
			 __func__, session_id, (unsigned int)v);

	return v;
}

static bool is_voice_session(u16 session_id)
{
	return (session_id == common.voice[VOC_PATH_PASSIVE].session_id);
}

static bool is_voip_session(u16 session_id)
{
	return (session_id == common.voice[VOC_PATH_FULL].session_id);
}

static bool is_volte_session(u16 session_id)
{
	return (session_id == common.voice[VOC_PATH_VOLTE_PASSIVE].session_id);
}

static int voice_apr_register(void)
{
	pr_debug("%s\n", __func__);

	mutex_lock(&common.common_lock);

	/* register callback to APR */
	if (common.apr_q6_mvm == NULL) {
		pr_debug("%s: Start to register MVM callback\n", __func__);

		common.apr_q6_mvm = apr_register("ADSP", "MVM",
						 qdsp_mvm_callback,
						 0xFFFFFFFF, &common);

		if (common.apr_q6_mvm == NULL) {
			pr_err("%s: Unable to register MVM\n", __func__);
			goto err;
		}
	}

	if (common.apr_q6_cvs == NULL) {
		pr_debug("%s: Start to register CVS callback\n", __func__);

		common.apr_q6_cvs = apr_register("ADSP", "CVS",
						 qdsp_cvs_callback,
						 0xFFFFFFFF, &common);

		if (common.apr_q6_cvs == NULL) {
			pr_err("%s: Unable to register CVS\n", __func__);
			goto err;
		}

	}

	if (common.apr_q6_cvp == NULL) {
		pr_debug("%s: Start to register CVP callback\n", __func__);

		common.apr_q6_cvp = apr_register("ADSP", "CVP",
						 qdsp_cvp_callback,
						 0xFFFFFFFF, &common);

		if (common.apr_q6_cvp == NULL) {
			pr_err("%s: Unable to register CVP\n", __func__);
			goto err;
		}

	}

	mutex_unlock(&common.common_lock);

	return 0;

err:
	if (common.apr_q6_cvs != NULL) {
		apr_deregister(common.apr_q6_cvs);
		common.apr_q6_cvs = NULL;
	}
	if (common.apr_q6_mvm != NULL) {
		apr_deregister(common.apr_q6_mvm);
		common.apr_q6_mvm = NULL;
	}

	mutex_unlock(&common.common_lock);

	return -ENODEV;
}

static int voice_send_dual_control_cmd(struct voice_data *v)
{
	int ret = 0;
	struct mvm_modem_dual_control_session_cmd mvm_voice_ctl_cmd;
	void *apr_mvm;
	u16 mvm_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_mvm = common.apr_q6_mvm;
	if (!apr_mvm) {
		pr_err("%s: apr_mvm is NULL.\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: VoLTE command to MVM\n", __func__);
	if (is_volte_session(v->session_id) ||
		is_voice_session(v->session_id)) {
		mvm_handle = voice_get_mvm_handle(v);
		mvm_voice_ctl_cmd.hdr.hdr_field = APR_HDR_FIELD(
						APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
		mvm_voice_ctl_cmd.hdr.pkt_size = APR_PKT_SIZE(
						APR_HDR_SIZE,
						sizeof(mvm_voice_ctl_cmd) -
						APR_HDR_SIZE);
		pr_debug("%s: send mvm Voice Ctl pkt size = %d\n",
			__func__, mvm_voice_ctl_cmd.hdr.pkt_size);
		mvm_voice_ctl_cmd.hdr.src_port = v->session_id;
		mvm_voice_ctl_cmd.hdr.dest_port = mvm_handle;
		mvm_voice_ctl_cmd.hdr.token = 0;
		mvm_voice_ctl_cmd.hdr.opcode =
					VSS_IMVM_CMD_SET_POLICY_DUAL_CONTROL;
		mvm_voice_ctl_cmd.voice_ctl.enable_flag = true;
		v->mvm_state = CMD_STATUS_FAIL;

		ret = apr_send_pkt(apr_mvm, (uint32_t *) &mvm_voice_ctl_cmd);
		if (ret < 0) {
			pr_err("%s: Error sending MVM Voice CTL CMD\n",
							__func__);
			ret = -EINVAL;
			goto fail;
		}
		ret = wait_event_timeout(v->mvm_wait,
				(v->mvm_state == CMD_STATUS_SUCCESS),
				msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);
			ret = -EINVAL;
			goto fail;
		}
	}
	ret = 0;
fail:
	return ret;
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
	apr_mvm = common.apr_q6_mvm;
	apr_cvs = common.apr_q6_cvs;
	apr_cvp = common.apr_q6_cvp;

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
		if (is_voice_session(v->session_id) ||
				is_volte_session(v->session_id)) {
			mvm_session_cmd.hdr.hdr_field = APR_HDR_FIELD(
						APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
			mvm_session_cmd.hdr.pkt_size = APR_PKT_SIZE(
						APR_HDR_SIZE,
						sizeof(mvm_session_cmd) -
						APR_HDR_SIZE);
			pr_debug("%s: send mvm create session pkt size = %d\n",
				 __func__, mvm_session_cmd.hdr.pkt_size);
			mvm_session_cmd.hdr.src_port = v->session_id;
			mvm_session_cmd.hdr.dest_port = 0;
			mvm_session_cmd.hdr.token = 0;
			mvm_session_cmd.hdr.opcode =
				VSS_IMVM_CMD_CREATE_PASSIVE_CONTROL_SESSION;
			if (is_volte_session(v->session_id)) {
				strlcpy(mvm_session_cmd.mvm_session.name,
				"default volte voice",
				sizeof(mvm_session_cmd.mvm_session.name));
			} else {
			strlcpy(mvm_session_cmd.mvm_session.name,
				"default modem voice",
				sizeof(mvm_session_cmd.mvm_session.name));
			}

			v->mvm_state = CMD_STATUS_FAIL;

			ret = apr_send_pkt(apr_mvm,
					(uint32_t *) &mvm_session_cmd);
			if (ret < 0) {
				pr_err("%s: Error sending MVM_CONTROL_SESSION\n",
				       __func__);
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
			mvm_session_cmd.hdr.src_port = v->session_id;
			mvm_session_cmd.hdr.dest_port = 0;
			mvm_session_cmd.hdr.token = 0;
			mvm_session_cmd.hdr.opcode =
				VSS_IMVM_CMD_CREATE_FULL_CONTROL_SESSION;
			strlcpy(mvm_session_cmd.mvm_session.name,
				"default voip",
				sizeof(mvm_session_cmd.mvm_session.name));

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
		if (is_voice_session(v->session_id) ||
			is_volte_session(v->session_id)) {
			pr_debug("%s: creating CVS passive session\n",
				 __func__);

			cvs_session_cmd.hdr.hdr_field = APR_HDR_FIELD(
						APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
			cvs_session_cmd.hdr.pkt_size =
						APR_PKT_SIZE(APR_HDR_SIZE,
						sizeof(cvs_session_cmd) -
						APR_HDR_SIZE);
			cvs_session_cmd.hdr.src_port = v->session_id;
			cvs_session_cmd.hdr.dest_port = 0;
			cvs_session_cmd.hdr.token = 0;
			cvs_session_cmd.hdr.opcode =
				VSS_ISTREAM_CMD_CREATE_PASSIVE_CONTROL_SESSION;
			if (is_volte_session(v->session_id)) {
				strlcpy(mvm_session_cmd.mvm_session.name,
				"default volte voice",
				sizeof(mvm_session_cmd.mvm_session.name));
			} else {
			strlcpy(cvs_session_cmd.cvs_session.name,
				"default modem voice",
				sizeof(cvs_session_cmd.cvs_session.name));
			}
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
			pr_debug("%s: creating CVS full session\n", __func__);

			cvs_full_ctl_cmd.hdr.hdr_field =
					APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);

			cvs_full_ctl_cmd.hdr.pkt_size =
					APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(cvs_full_ctl_cmd) -
					APR_HDR_SIZE);

			cvs_full_ctl_cmd.hdr.src_port = v->session_id;
			cvs_full_ctl_cmd.hdr.dest_port = 0;
			cvs_full_ctl_cmd.hdr.token = 0;
			cvs_full_ctl_cmd.hdr.opcode =
				VSS_ISTREAM_CMD_CREATE_FULL_CONTROL_SESSION;
			cvs_full_ctl_cmd.cvs_session.direction = 2;
			cvs_full_ctl_cmd.cvs_session.enc_media_type =
						common.mvs_info.media_type;
			cvs_full_ctl_cmd.cvs_session.dec_media_type =
						common.mvs_info.media_type;
			cvs_full_ctl_cmd.cvs_session.network_id =
					       common.mvs_info.network_type;
			strlcpy(cvs_full_ctl_cmd.cvs_session.name,
				"default q6 voice",
				sizeof(cvs_full_ctl_cmd.cvs_session.name));

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
			pr_debug("%s: Attach MVM to stream\n", __func__);

			attach_stream_cmd.hdr.hdr_field =
					APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);
			attach_stream_cmd.hdr.pkt_size =
					APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(attach_stream_cmd) -
					APR_HDR_SIZE);
			attach_stream_cmd.hdr.src_port = v->session_id;
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
	apr_mvm = common.apr_q6_mvm;
	apr_cvs = common.apr_q6_cvs;

	if (!apr_mvm || !apr_cvs) {
		pr_err("%s: apr_mvm or apr_cvs is NULL\n", __func__);
		return -EINVAL;
	}
	mvm_handle = voice_get_mvm_handle(v);
	cvs_handle = voice_get_cvs_handle(v);

	/* MVM, CVS sessions are destroyed only for Full control sessions. */
	if (is_voip_session(v->session_id)) {
		pr_debug("%s: MVM detach stream\n", __func__);

		/* Detach voice stream. */
		detach_stream.hdr.hdr_field =
					APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);
		detach_stream.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(detach_stream) - APR_HDR_SIZE);
		detach_stream.hdr.src_port = v->session_id;
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
		pr_debug("%s: CVS destroy session\n", __func__);

		cvs_destroy.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						      APR_HDR_LEN(APR_HDR_SIZE),
						      APR_PKT_VER);
		cvs_destroy.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(cvs_destroy) - APR_HDR_SIZE);
		cvs_destroy.src_port = v->session_id;
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
		mvm_destroy.src_port = v->session_id;
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
	apr_mvm = common.apr_q6_mvm;

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
		pr_debug("%s: pkt size = %d\n",
			 __func__, mvm_tty_mode_cmd.hdr.pkt_size);
		mvm_tty_mode_cmd.hdr.src_port = v->session_id;
		mvm_tty_mode_cmd.hdr.dest_port = mvm_handle;
		mvm_tty_mode_cmd.hdr.token = 0;
		mvm_tty_mode_cmd.hdr.opcode = VSS_ISTREAM_CMD_SET_TTY_MODE;
		mvm_tty_mode_cmd.tty_mode.mode = v->tty_mode;
		pr_debug("tty mode =%d\n", mvm_tty_mode_cmd.tty_mode.mode);

		v->mvm_state = CMD_STATUS_FAIL;
		ret = apr_send_pkt(apr_mvm, (uint32_t *) &mvm_tty_mode_cmd);
		if (ret < 0) {
			pr_err("%s: Error %d sending SET_TTY_MODE\n",
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
	return 0;
fail:
	return -EINVAL;
}

static int voice_set_dtx(struct voice_data *v)
{
	int ret = 0;
	void *apr_cvs;
	u16 cvs_handle;
	struct cvs_set_enc_dtx_mode_cmd cvs_set_dtx;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvs = common.apr_q6_cvs;

	if (!apr_cvs) {
		pr_err("%s: apr_cvs is NULL.\n", __func__);
		return -EINVAL;
	}

	cvs_handle = voice_get_cvs_handle(v);

	/* Set DTX */
	cvs_set_dtx.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					      APR_HDR_LEN(APR_HDR_SIZE),
					      APR_PKT_VER);
	cvs_set_dtx.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(cvs_set_dtx) - APR_HDR_SIZE);
	cvs_set_dtx.hdr.src_port = v->session_id;
	cvs_set_dtx.hdr.dest_port = cvs_handle;
	cvs_set_dtx.hdr.token = 0;
	cvs_set_dtx.hdr.opcode = VSS_ISTREAM_CMD_SET_ENC_DTX_MODE;
	cvs_set_dtx.dtx_mode.enable = common.mvs_info.dtx_mode;

	pr_debug("%s: Setting DTX %d\n", __func__, common.mvs_info.dtx_mode);

	v->cvs_state = CMD_STATUS_FAIL;

	ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_set_dtx);
	if (ret < 0) {
		pr_err("%s: Error %d sending SET_DTX\n", __func__, ret);
		return -EINVAL;
	}

	ret = wait_event_timeout(v->cvs_wait,
				 (v->cvs_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		return -EINVAL;
	}

	return 0;
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
	apr_cvs = common.apr_q6_cvs;

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
	cvs_set_media_cmd.hdr.src_port = v->session_id;
	cvs_set_media_cmd.hdr.dest_port = cvs_handle;
	cvs_set_media_cmd.hdr.token = 0;
	cvs_set_media_cmd.hdr.opcode = VSS_ISTREAM_CMD_SET_MEDIA_TYPE;
	cvs_set_media_cmd.media_type.tx_media_id = common.mvs_info.media_type;
	cvs_set_media_cmd.media_type.rx_media_id = common.mvs_info.media_type;

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
	switch (common.mvs_info.media_type) {
	case VSS_MEDIA_ID_EVRC_MODEM: {
		struct cvs_set_cdma_enc_minmax_rate_cmd cvs_set_cdma_rate;

		pr_debug("Setting EVRC min-max rate\n");

		cvs_set_cdma_rate.hdr.hdr_field =
					APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);
		cvs_set_cdma_rate.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				      sizeof(cvs_set_cdma_rate) - APR_HDR_SIZE);
		cvs_set_cdma_rate.hdr.src_port = v->session_id;
		cvs_set_cdma_rate.hdr.dest_port = cvs_handle;
		cvs_set_cdma_rate.hdr.token = 0;
		cvs_set_cdma_rate.hdr.opcode =
				VSS_ISTREAM_CMD_CDMA_SET_ENC_MINMAX_RATE;
		cvs_set_cdma_rate.cdma_rate.min_rate = common.mvs_info.rate;
		cvs_set_cdma_rate.cdma_rate.max_rate = common.mvs_info.rate;

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

		pr_debug("Setting AMR rate\n");

		cvs_set_amr_rate.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
		cvs_set_amr_rate.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				       sizeof(cvs_set_amr_rate) - APR_HDR_SIZE);
		cvs_set_amr_rate.hdr.src_port = v->session_id;
		cvs_set_amr_rate.hdr.dest_port = cvs_handle;
		cvs_set_amr_rate.hdr.token = 0;
		cvs_set_amr_rate.hdr.opcode =
					VSS_ISTREAM_CMD_VOC_AMR_SET_ENC_RATE;
		cvs_set_amr_rate.amr_rate.mode = common.mvs_info.rate;

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

		ret = voice_set_dtx(v);
		if (ret < 0)
			goto fail;

		break;
	}
	case VSS_MEDIA_ID_AMR_WB_MODEM: {
		struct cvs_set_amrwb_enc_rate_cmd cvs_set_amrwb_rate;

		pr_debug("Setting AMR WB rate\n");

		cvs_set_amrwb_rate.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
		cvs_set_amrwb_rate.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
						sizeof(cvs_set_amrwb_rate) -
						APR_HDR_SIZE);
		cvs_set_amrwb_rate.hdr.src_port = v->session_id;
		cvs_set_amrwb_rate.hdr.dest_port = cvs_handle;
		cvs_set_amrwb_rate.hdr.token = 0;
		cvs_set_amrwb_rate.hdr.opcode =
					VSS_ISTREAM_CMD_VOC_AMRWB_SET_ENC_RATE;
		cvs_set_amrwb_rate.amrwb_rate.mode = common.mvs_info.rate;

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

		ret = voice_set_dtx(v);
		if (ret < 0)
			goto fail;

		break;
	}
	case VSS_MEDIA_ID_G729:
	case VSS_MEDIA_ID_G711_ALAW:
	case VSS_MEDIA_ID_G711_MULAW: {
		ret = voice_set_dtx(v);

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
	apr_mvm = common.apr_q6_mvm;

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
	mvm_start_voice_cmd.src_port = v->session_id;
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
	apr_cvp = common.apr_q6_cvp;

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
	cvp_disable_cmd.src_port = v->session_id;
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
	apr_cvp = common.apr_q6_cvp;

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
	cvp_setdev_cmd.hdr.src_port = v->session_id;
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
	apr_mvm = common.apr_q6_mvm;

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
	mvm_stop_voice_cmd.src_port = v->session_id;
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


static int voice_setup_vocproc(struct voice_data *v)
{
	struct cvp_create_full_ctl_session_cmd cvp_session_cmd;
	int ret = 0;
	void *apr_cvp;
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvp = common.apr_q6_cvp;

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
	cvp_session_cmd.hdr.src_port = v->session_id;
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


	if (is_voip_session(v->session_id))
		voice_send_netid_timing_cmd(v);

	/* Start in-call music delivery if this feature is enabled */
	if (v->music_info.play_enable)
		voice_cvs_start_playback(v);

	/* Start in-call recording if this feature is enabled */
	if (v->rec_info.rec_enable)
		voice_cvs_start_record(v, v->rec_info.rec_mode);


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
	apr_cvp = common.apr_q6_cvp;

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
	cvp_enable_cmd.src_port = v->session_id;
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
	apr_mvm = common.apr_q6_mvm;

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
	mvm_set_network.hdr.src_port = v->session_id;
	mvm_set_network.hdr.dest_port = mvm_handle;
	mvm_set_network.hdr.token = 0;
	mvm_set_network.hdr.opcode = VSS_ICOMMON_CMD_SET_NETWORK;
	mvm_set_network.network.network_id = common.mvs_info.network_type;

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
	mvm_set_voice_timing.hdr.src_port = v->session_id;
	mvm_set_voice_timing.hdr.dest_port = mvm_handle;
	mvm_set_voice_timing.hdr.token = 0;
	mvm_set_voice_timing.hdr.opcode = VSS_ICOMMON_CMD_SET_VOICE_TIMING;
	mvm_set_voice_timing.timing.mode = 0;
	mvm_set_voice_timing.timing.enc_offset = 8000;
	if ((machine_is_apq8064_sim()) || (machine_is_msm8974_sim())) {
		pr_debug("%s: Machine is MSM8974 sim\n", __func__);
		mvm_set_voice_timing.timing.dec_req_offset = 0;
		mvm_set_voice_timing.timing.dec_offset = 18000;
	} else {
		mvm_set_voice_timing.timing.dec_req_offset = 3300;
		mvm_set_voice_timing.timing.dec_offset = 8300;
	}

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
	apr_mvm = common.apr_q6_mvm;

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
	mvm_a_vocproc_cmd.hdr.src_port = v->session_id;
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
	apr_mvm = common.apr_q6_mvm;
	apr_cvp = common.apr_q6_cvp;

	if (!apr_mvm || !apr_cvp) {
		pr_err("%s: apr_mvm or apr_cvp is NULL.\n", __func__);
		return -EINVAL;
	}
	mvm_handle = voice_get_mvm_handle(v);
	cvp_handle = voice_get_cvp_handle(v);

	/* stop playback or recording */
	v->music_info.force = 1;
	voice_cvs_stop_playback(v);
	voice_cvs_stop_record(v);
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
	mvm_d_vocproc_cmd.hdr.src_port = v->session_id;
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


	/* destrop cvp session */
	cvp_destroy_session_cmd.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	cvp_destroy_session_cmd.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_destroy_session_cmd) - APR_HDR_SIZE);
	pr_debug("cvp_destroy_session_cmd pkt size = %d\n",
		cvp_destroy_session_cmd.pkt_size);
	cvp_destroy_session_cmd.src_port = v->session_id;
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
	apr_cvs = common.apr_q6_cvs;

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
	cvs_mute_cmd.hdr.src_port = v->session_id;
	cvs_mute_cmd.hdr.dest_port = cvs_handle;
	cvs_mute_cmd.hdr.token = 0;
	cvs_mute_cmd.hdr.opcode = VSS_ISTREAM_CMD_SET_MUTE;
	cvs_mute_cmd.cvs_set_mute.direction = 0; /*tx*/
	cvs_mute_cmd.cvs_set_mute.mute_flag = v->dev_tx.mute;

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

static int voice_send_rx_device_mute_cmd(struct voice_data *v)
{
	struct cvp_set_mute_cmd cvp_mute_cmd;
	int ret = 0;
	void *apr_cvp;
	u16 cvp_handle;
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvp = common.apr_q6_cvp;

	if (!apr_cvp) {
		pr_err("%s: apr_cvp is NULL.\n", __func__);
		return -EINVAL;
	}
	cvp_handle = voice_get_cvp_handle(v);

	cvp_mute_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	cvp_mute_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(cvp_mute_cmd) - APR_HDR_SIZE);
	cvp_mute_cmd.hdr.src_port = v->session_id;
	cvp_mute_cmd.hdr.dest_port = cvp_handle;
	cvp_mute_cmd.hdr.token = 0;
	cvp_mute_cmd.hdr.opcode = VSS_IVOCPROC_CMD_SET_MUTE;
	cvp_mute_cmd.cvp_set_mute.direction = 1;
	cvp_mute_cmd.cvp_set_mute.mute_flag = v->dev_rx.mute;
	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvp, (uint32_t *) &cvp_mute_cmd);
	if (ret < 0) {
		pr_err("Fail in sending RX device mute cmd\n");
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
	apr_cvp = common.apr_q6_cvp;

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
	cvp_vol_cmd.hdr.src_port = v->session_id;
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

static int voice_cvs_start_record(struct voice_data *v, uint32_t rec_mode)
{
	int ret = 0;
	void *apr_cvs;
	u16 cvs_handle;

	struct cvs_start_record_cmd cvs_start_record;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvs = common.apr_q6_cvs;

	if (!apr_cvs) {
		pr_err("%s: apr_cvs is NULL.\n", __func__);
		return -EINVAL;
	}

	cvs_handle = voice_get_cvs_handle(v);

	if (!v->rec_info.recording) {
		cvs_start_record.hdr.hdr_field = APR_HDR_FIELD(
					APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);
		cvs_start_record.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				  sizeof(cvs_start_record) - APR_HDR_SIZE);
		cvs_start_record.hdr.src_port = v->session_id;
		cvs_start_record.hdr.dest_port = cvs_handle;
		cvs_start_record.hdr.token = 0;
		cvs_start_record.hdr.opcode = VSS_ISTREAM_CMD_START_RECORD;

		if (rec_mode == VOC_REC_UPLINK) {
			cvs_start_record.rec_mode.rx_tap_point =
						VSS_TAP_POINT_NONE;
			cvs_start_record.rec_mode.tx_tap_point =
						VSS_TAP_POINT_STREAM_END;
		} else if (rec_mode == VOC_REC_DOWNLINK) {
			cvs_start_record.rec_mode.rx_tap_point =
						VSS_TAP_POINT_STREAM_END;
			cvs_start_record.rec_mode.tx_tap_point =
						VSS_TAP_POINT_NONE;
		} else if (rec_mode == VOC_REC_BOTH) {
			cvs_start_record.rec_mode.rx_tap_point =
						VSS_TAP_POINT_STREAM_END;
			cvs_start_record.rec_mode.tx_tap_point =
						VSS_TAP_POINT_STREAM_END;
		} else {
			pr_err("%s: Invalid in-call rec_mode %d\n", __func__,
				rec_mode);

			ret = -EINVAL;
			goto fail;
		}

		v->cvs_state = CMD_STATUS_FAIL;

		ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_start_record);
		if (ret < 0) {
			pr_err("%s: Error %d sending START_RECORD\n", __func__,
				ret);

			goto fail;
		}

		ret = wait_event_timeout(v->cvs_wait,
				 (v->cvs_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));

		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);

			goto fail;
		}
		v->rec_info.recording = 1;
	} else {
		pr_debug("%s: Start record already sent\n", __func__);
	}

	return 0;

fail:
	return ret;
}

static int voice_cvs_stop_record(struct voice_data *v)
{
	int ret = 0;
	void *apr_cvs;
	u16 cvs_handle;
	struct apr_hdr cvs_stop_record;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvs = common.apr_q6_cvs;

	if (!apr_cvs) {
		pr_err("%s: apr_cvs is NULL.\n", __func__);
		return -EINVAL;
	}

	cvs_handle = voice_get_cvs_handle(v);

	if (v->rec_info.recording) {
		cvs_stop_record.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				  APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		cvs_stop_record.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				  sizeof(cvs_stop_record) - APR_HDR_SIZE);
		cvs_stop_record.src_port = v->session_id;
		cvs_stop_record.dest_port = cvs_handle;
		cvs_stop_record.token = 0;
		cvs_stop_record.opcode = VSS_ISTREAM_CMD_STOP_RECORD;

		v->cvs_state = CMD_STATUS_FAIL;

		ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_stop_record);
		if (ret < 0) {
			pr_err("%s: Error %d sending STOP_RECORD\n",
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
		v->rec_info.recording = 0;
	} else {
		pr_debug("%s: Stop record already sent\n", __func__);
	}

	return 0;

fail:

	return ret;
}

int voc_start_record(uint32_t port_id, uint32_t set)
{
	int ret = 0;
	int rec_mode = 0;
	u16 cvs_handle;
	int i, rec_set = 0;

	for (i = 0; i < MAX_VOC_SESSIONS; i++) {
		struct voice_data *v = &common.voice[i];
		pr_debug("%s: i:%d port_id: %d, set: %d\n",
			__func__, i, port_id, set);

		mutex_lock(&v->lock);
		rec_mode = v->rec_info.rec_mode;
		rec_set = set;
		if (set) {
			if ((v->rec_route_state.ul_flag != 0) &&
				(v->rec_route_state.dl_flag != 0)) {
				pr_debug("%s: i=%d, rec mode already set.\n",
					__func__, i);
				mutex_unlock(&v->lock);
				if (i < MAX_VOC_SESSIONS)
					continue;
				else
					return 0;
			}

			if (port_id == VOICE_RECORD_TX) {
				if ((v->rec_route_state.ul_flag == 0)
				&& (v->rec_route_state.dl_flag == 0)) {
					rec_mode = VOC_REC_UPLINK;
					v->rec_route_state.ul_flag = 1;
				} else if ((v->rec_route_state.ul_flag == 0)
					&& (v->rec_route_state.dl_flag != 0)) {
					voice_cvs_stop_record(v);
					rec_mode = VOC_REC_BOTH;
					v->rec_route_state.ul_flag = 1;
				}
			} else if (port_id == VOICE_RECORD_RX) {
				if ((v->rec_route_state.ul_flag == 0)
					&& (v->rec_route_state.dl_flag == 0)) {
					rec_mode = VOC_REC_DOWNLINK;
					v->rec_route_state.dl_flag = 1;
				} else if ((v->rec_route_state.ul_flag != 0)
					&& (v->rec_route_state.dl_flag == 0)) {
					voice_cvs_stop_record(v);
					rec_mode = VOC_REC_BOTH;
					v->rec_route_state.dl_flag = 1;
				}
			}
			rec_set = 1;
		} else {
			if ((v->rec_route_state.ul_flag == 0) &&
				(v->rec_route_state.dl_flag == 0)) {
				pr_debug("%s: i=%d, rec already stops.\n",
					__func__, i);
				mutex_unlock(&v->lock);
				if (i < MAX_VOC_SESSIONS)
					continue;
				else
					return 0;
			}

			if (port_id == VOICE_RECORD_TX) {
				if ((v->rec_route_state.ul_flag != 0)
					&& (v->rec_route_state.dl_flag == 0)) {
					v->rec_route_state.ul_flag = 0;
					rec_set = 0;
				} else if ((v->rec_route_state.ul_flag != 0)
					&& (v->rec_route_state.dl_flag != 0)) {
					voice_cvs_stop_record(v);
					v->rec_route_state.ul_flag = 0;
					rec_mode = VOC_REC_DOWNLINK;
					rec_set = 1;
				}
			} else if (port_id == VOICE_RECORD_RX) {
				if ((v->rec_route_state.ul_flag == 0)
					&& (v->rec_route_state.dl_flag != 0)) {
					v->rec_route_state.dl_flag = 0;
					rec_set = 0;
				} else if ((v->rec_route_state.ul_flag != 0)
					&& (v->rec_route_state.dl_flag != 0)) {
					voice_cvs_stop_record(v);
					v->rec_route_state.dl_flag = 0;
					rec_mode = VOC_REC_UPLINK;
					rec_set = 1;
				}
			}
		}
		pr_debug("%s: i=%d, mode =%d, set =%d\n", __func__,
			i, rec_mode, rec_set);
		cvs_handle = voice_get_cvs_handle(v);

		if (cvs_handle != 0) {
			if (rec_set)
				ret = voice_cvs_start_record(v, rec_mode);
			else
				ret = voice_cvs_stop_record(v);
		}

		/* Cache the value */
		v->rec_info.rec_enable = rec_set;
		v->rec_info.rec_mode = rec_mode;

		mutex_unlock(&v->lock);
	}

	return ret;
}

static int voice_cvs_start_playback(struct voice_data *v)
{
	int ret = 0;
	struct apr_hdr cvs_start_playback;
	void *apr_cvs;
	u16 cvs_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvs = common.apr_q6_cvs;

	if (!apr_cvs) {
		pr_err("%s: apr_cvs is NULL.\n", __func__);
		return -EINVAL;
	}

	cvs_handle = voice_get_cvs_handle(v);

	if (!v->music_info.playing && v->music_info.count) {
		cvs_start_playback.hdr_field = APR_HDR_FIELD(
					APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);
		cvs_start_playback.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvs_start_playback) - APR_HDR_SIZE);
		cvs_start_playback.src_port = v->session_id;
		cvs_start_playback.dest_port = cvs_handle;
		cvs_start_playback.token = 0;
		cvs_start_playback.opcode = VSS_ISTREAM_CMD_START_PLAYBACK;

		v->cvs_state = CMD_STATUS_FAIL;

		ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_start_playback);

		if (ret < 0) {
			pr_err("%s: Error %d sending START_PLAYBACK\n",
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

		v->music_info.playing = 1;
	} else {
		pr_debug("%s: Start playback already sent\n", __func__);
	}

	return 0;

fail:
	return ret;
}

static int voice_cvs_stop_playback(struct voice_data *v)
{
	 int ret = 0;
	 struct apr_hdr cvs_stop_playback;
	 void *apr_cvs;
	 u16 cvs_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	apr_cvs = common.apr_q6_cvs;

	if (!apr_cvs) {
		pr_err("%s: apr_cvs is NULL.\n", __func__);
		return -EINVAL;
	}

	cvs_handle = voice_get_cvs_handle(v);

	if (v->music_info.playing && ((!v->music_info.count) ||
						(v->music_info.force))) {
		cvs_stop_playback.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		cvs_stop_playback.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvs_stop_playback) - APR_HDR_SIZE);
		cvs_stop_playback.src_port = v->session_id;
		cvs_stop_playback.dest_port = cvs_handle;
		cvs_stop_playback.token = 0;

		cvs_stop_playback.opcode = VSS_ISTREAM_CMD_STOP_PLAYBACK;

		v->cvs_state = CMD_STATUS_FAIL;

		ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_stop_playback);
		if (ret < 0) {
			pr_err("%s: Error %d sending STOP_PLAYBACK\n",
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

		v->music_info.playing = 0;
		v->music_info.force = 0;
	} else {
		pr_debug("%s: Stop playback already sent\n", __func__);
	}

	return 0;

fail:
	return ret;
}

int voc_start_playback(uint32_t set)
{
	int ret = 0;
	u16 cvs_handle;
	int i;


	for (i = 0; i < MAX_VOC_SESSIONS; i++) {
		struct voice_data *v = &common.voice[i];

		mutex_lock(&v->lock);
		v->music_info.play_enable = set;
		if (set)
			v->music_info.count++;
		else
			v->music_info.count--;
		pr_debug("%s: music_info count =%d\n", __func__,
			v->music_info.count);

		cvs_handle = voice_get_cvs_handle(v);
		if (cvs_handle != 0) {
			if (set)
				ret = voice_cvs_start_playback(v);
			else
				ret = voice_cvs_stop_playback(v);
		}

		mutex_unlock(&v->lock);
	}

	return ret;
}

int voc_disable_cvp(uint16_t session_id)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	if (v->voc_state == VOC_RUN) {
		/* send cmd to dsp to disable vocproc */
		ret = voice_send_disable_vocproc_cmd(v);
		if (ret < 0) {
			pr_err("%s:  disable vocproc failed\n", __func__);
			goto fail;
		}


		v->voc_state = VOC_CHANGE;
	}

fail:	mutex_unlock(&v->lock);

	return ret;
}

int voc_enable_cvp(uint16_t session_id)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	if (v->voc_state == VOC_CHANGE) {
		ret = voice_send_set_device_cmd(v);
		if (ret < 0) {
			pr_err("%s:  set device failed\n", __func__);
			goto fail;
		}
		/* send tty mode if tty device is used */
		voice_send_tty_mode_cmd(v);

		v->voc_state = VOC_RUN;
	}

fail:
	mutex_unlock(&v->lock);

	return ret;
}

int voc_set_tx_mute(uint16_t session_id, uint32_t dir, uint32_t mute)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	v->dev_tx.mute = mute;

	if (v->voc_state == VOC_RUN)
		ret = voice_send_mute_cmd(v);

	mutex_unlock(&v->lock);

	return ret;
}

int voc_set_rx_device_mute(uint16_t session_id, uint32_t mute)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	v->dev_rx.mute = mute;

	if (v->voc_state == VOC_RUN)
		ret = voice_send_rx_device_mute_cmd(v);

	mutex_unlock(&v->lock);

	return ret;
}

int voc_get_rx_device_mute(uint16_t session_id)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	ret = v->dev_rx.mute;

	mutex_unlock(&v->lock);

	return ret;
}

int voc_set_tty_mode(uint16_t session_id, uint8_t tty_mode)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	v->tty_mode = tty_mode;

	mutex_unlock(&v->lock);

	return ret;
}

uint8_t voc_get_tty_mode(uint16_t session_id)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	ret = v->tty_mode;

	mutex_unlock(&v->lock);

	return ret;
}

int voc_set_widevoice_enable(uint16_t session_id, uint32_t wv_enable)
{
	struct voice_data *v = voice_get_session(session_id);
	u16 mvm_handle;
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	v->wv_enable = wv_enable;

	mvm_handle = voice_get_mvm_handle(v);


	mutex_unlock(&v->lock);

	return ret;
}

uint32_t voc_get_widevoice_enable(uint16_t session_id)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	ret = v->wv_enable;

	mutex_unlock(&v->lock);

	return ret;
}

int voc_set_pp_enable(uint16_t session_id, uint32_t module_id, uint32_t enable)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);
	if (module_id == MODULE_ID_VOICE_MODULE_ST)
		v->st_enable = enable;
	else if (module_id == MODULE_ID_VOICE_MODULE_FENS)
		v->fens_enable = enable;

	mutex_unlock(&v->lock);

	return ret;
}

int voc_get_pp_enable(uint16_t session_id, uint32_t module_id)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);
	if (module_id == MODULE_ID_VOICE_MODULE_ST)
		ret = v->st_enable;
	else if (module_id == MODULE_ID_VOICE_MODULE_FENS)
		ret = v->fens_enable;

	mutex_unlock(&v->lock);

	return ret;
}

int voc_set_rx_vol_index(uint16_t session_id, uint32_t dir, uint32_t vol_idx)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	v->dev_rx.volume = vol_idx;

	if (v->voc_state == VOC_RUN)
		ret = voice_send_vol_index_cmd(v);

	mutex_unlock(&v->lock);

	return ret;
}

int voc_set_rxtx_port(uint16_t session_id, uint32_t port_id, uint32_t dev_type)
{
	struct voice_data *v = voice_get_session(session_id);

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	pr_debug("%s: port_id=%d, type=%d\n", __func__, port_id, dev_type);

	mutex_lock(&v->lock);

	if (dev_type == DEV_RX)
		v->dev_rx.port_id = q6audio_get_port_id(port_id);
	else
		v->dev_tx.port_id = q6audio_get_port_id(port_id);

	mutex_unlock(&v->lock);

	return 0;
}

int voc_set_route_flag(uint16_t session_id, uint8_t path_dir, uint8_t set)
{
	struct voice_data *v = voice_get_session(session_id);

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	pr_debug("%s: path_dir=%d, set=%d\n", __func__, path_dir, set);

	mutex_lock(&v->lock);

	if (path_dir == RX_PATH)
		v->voc_route_state.rx_route_flag = set;
	else
		v->voc_route_state.tx_route_flag = set;

	mutex_unlock(&v->lock);

	return 0;
}

uint8_t voc_get_route_flag(uint16_t session_id, uint8_t path_dir)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return 0;
	}

	mutex_lock(&v->lock);

	if (path_dir == RX_PATH)
		ret = v->voc_route_state.rx_route_flag;
	else
		ret = v->voc_route_state.tx_route_flag;

	mutex_unlock(&v->lock);

	return ret;
}

int voc_end_voice_call(uint16_t session_id)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	if (v->voc_state == VOC_RUN) {
		ret = voice_destroy_vocproc(v);
		if (ret < 0)
			pr_err("%s:  destroy voice failed\n", __func__);
		voice_destroy_mvm_cvs_session(v);

		v->voc_state = VOC_RELEASE;
	}
	mutex_unlock(&v->lock);
	return ret;
}

int voc_start_voice_call(uint16_t session_id)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	if ((v->voc_state == VOC_INIT) ||
		(v->voc_state == VOC_RELEASE)) {
		ret = voice_apr_register();
		if (ret < 0) {
			pr_err("%s:  apr register failed\n", __func__);
			goto fail;
		}
		ret = voice_create_mvm_cvs_session(v);
		if (ret < 0) {
			pr_err("create mvm and cvs failed\n");
			goto fail;
		}
		ret = voice_send_dual_control_cmd(v);
		if (ret < 0) {
			pr_err("Err Dual command failed\n");
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

		v->voc_state = VOC_RUN;
	}
fail:	mutex_unlock(&v->lock);
	return ret;
}

void voc_register_mvs_cb(ul_cb_fn ul_cb,
			   dl_cb_fn dl_cb,
			   void *private_data)
{
	common.mvs_info.ul_cb = ul_cb;
	common.mvs_info.dl_cb = dl_cb;
	common.mvs_info.private_data = private_data;
}

void voc_config_vocoder(uint32_t media_type,
			  uint32_t rate,
			  uint32_t network_type,
			  uint32_t dtx_mode)
{
	common.mvs_info.media_type = media_type;
	common.mvs_info.rate = rate;
	common.mvs_info.network_type = network_type;
	common.mvs_info.dtx_mode = dtx_mode;
}

static int32_t qdsp_mvm_callback(struct apr_client_data *data, void *priv)
{
	uint32_t *ptr = NULL;
	struct common_data *c = NULL;
	struct voice_data *v = NULL;
	int i = 0;

	if ((data == NULL) || (priv == NULL)) {
		pr_err("%s: data or priv is NULL\n", __func__);
		return -EINVAL;
	}

	c = priv;

	pr_debug("%s: session_id 0x%x\n", __func__, data->dest_port);

	v = voice_get_session(data->dest_port);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		return -EINVAL;
	}

	pr_debug("%s: Payload Length = %d, opcode=%x\n", __func__,
		data->payload_size, data->opcode);

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: Reset event received in Voice service\n",
			 __func__);

		apr_reset(c->apr_q6_mvm);
		c->apr_q6_mvm = NULL;

		/* Sub-system restart is applicable to all sessions. */
		for (i = 0; i < MAX_VOC_SESSIONS; i++)
			c->voice[i].mvm_handle = 0;

		return 0;
	}

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		if (data->payload_size) {
			ptr = data->payload;

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
					pr_err("got NACK for sending MVM create session\n");
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
			case VSS_IWIDEVOICE_CMD_SET_WIDEVOICE:
			case VSS_IMVM_CMD_SET_POLICY_DUAL_CONTROL:
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
	uint32_t *ptr = NULL;
	struct common_data *c = NULL;
	struct voice_data *v = NULL;
	int i = 0;

	if ((data == NULL) || (priv == NULL)) {
		pr_err("%s: data or priv is NULL\n", __func__);
		return -EINVAL;
	}

	c = priv;

	pr_debug("%s: session_id 0x%x\n", __func__, data->dest_port);

	v = voice_get_session(data->dest_port);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		return -EINVAL;
	}

	pr_debug("%s: Payload Length = %d, opcode=%x\n", __func__,
		data->payload_size, data->opcode);

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: Reset event received in Voice service\n",
			 __func__);

		apr_reset(c->apr_q6_cvs);
		c->apr_q6_cvs = NULL;

		/* Sub-system restart is applicable to all sessions. */
		for (i = 0; i < MAX_VOC_SESSIONS; i++)
			c->voice[i].cvs_handle = 0;

		return 0;
	}

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		if (data->payload_size) {
			ptr = data->payload;

			/*response from  CVS */
			switch (ptr[0]) {
			case VSS_ISTREAM_CMD_CREATE_PASSIVE_CONTROL_SESSION:
			case VSS_ISTREAM_CMD_CREATE_FULL_CONTROL_SESSION:
				if (!ptr[1]) {
					pr_debug("%s: CVS handle is %d\n",
						 __func__, data->src_port);
					voice_set_cvs_handle(v, data->src_port);
				} else
					pr_err("got NACK for sending CVS create session\n");
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
			case VSS_ICOMMON_CMD_SET_UI_PROPERTY:
			case VSS_ISTREAM_CMD_START_PLAYBACK:
			case VSS_ISTREAM_CMD_STOP_PLAYBACK:
			case VSS_ISTREAM_CMD_START_RECORD:
			case VSS_ISTREAM_CMD_STOP_RECORD:
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				v->cvs_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvs_wait);
				break;
			case VOICE_CMD_SET_PARAM:
				break;
			default:
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				break;
			}
		}
	} else if (data->opcode == VSS_ISTREAM_EVT_SEND_ENC_BUFFER) {
		uint32_t *voc_pkt = data->payload;
		uint32_t pkt_len = data->payload_size;

		if (voc_pkt != NULL && c->mvs_info.ul_cb != NULL) {
			pr_debug("%s: Media type is 0x%x\n",
				 __func__, voc_pkt[0]);

			/* Remove media ID from payload. */
			voc_pkt++;
			pkt_len = pkt_len - 4;

			c->mvs_info.ul_cb((uint8_t *)voc_pkt,
					  pkt_len,
					  c->mvs_info.private_data);
		} else
			pr_err("%s: voc_pkt is 0x%x ul_cb is 0x%x\n",
			       __func__, (unsigned int)voc_pkt,
			       (unsigned int) c->mvs_info.ul_cb);
	} else if (data->opcode == VSS_ISTREAM_EVT_REQUEST_DEC_BUFFER) {
		struct cvs_send_dec_buf_cmd send_dec_buf;
		int ret = 0;
		uint32_t pkt_len = 0;

		if (c->mvs_info.dl_cb != NULL) {
			send_dec_buf.dec_buf.media_id = c->mvs_info.media_type;

			c->mvs_info.dl_cb(
				(uint8_t *)&send_dec_buf.dec_buf.packet_data,
				&pkt_len,
				c->mvs_info.private_data);

			send_dec_buf.hdr.hdr_field =
					APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);
			send_dec_buf.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
			       sizeof(send_dec_buf.dec_buf.media_id) + pkt_len);
			send_dec_buf.hdr.src_port = v->session_id;
			send_dec_buf.hdr.dest_port = voice_get_cvs_handle(v);
			send_dec_buf.hdr.token = 0;
			send_dec_buf.hdr.opcode =
					VSS_ISTREAM_EVT_SEND_DEC_BUFFER;

			ret = apr_send_pkt(c->apr_q6_cvs,
					   (uint32_t *) &send_dec_buf);
			if (ret < 0) {
				pr_err("%s: Error %d sending DEC_BUF\n",
				       __func__, ret);
				goto fail;
			}
		} else
			pr_debug("%s: dl_cb is NULL\n", __func__);
	} else if (data->opcode == VSS_ISTREAM_EVT_SEND_DEC_BUFFER) {
		pr_debug("Send dec buf resp\n");
	} else
		pr_debug("Unknown opcode 0x%x\n", data->opcode);

fail:
	return 0;
}

static int32_t qdsp_cvp_callback(struct apr_client_data *data, void *priv)
{
	uint32_t *ptr = NULL;
	struct common_data *c = NULL;
	struct voice_data *v = NULL;
	int i = 0;

	if ((data == NULL) || (priv == NULL)) {
		pr_err("%s: data or priv is NULL\n", __func__);
		return -EINVAL;
	}

	c = priv;

	v = voice_get_session(data->dest_port);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		return -EINVAL;
	}

	pr_debug("%s: Payload Length = %d, opcode=%x\n", __func__,
		data->payload_size, data->opcode);

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: Reset event received in Voice service\n",
			 __func__);

		apr_reset(c->apr_q6_cvp);
		c->apr_q6_cvp = NULL;

		/* Sub-system restart is applicable to all sessions. */
		for (i = 0; i < MAX_VOC_SESSIONS; i++)
			c->voice[i].cvp_handle = 0;

		return 0;
	}

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		if (data->payload_size) {
			ptr = data->payload;

			switch (ptr[0]) {
			case VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION:
			/*response from  CVP */
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				if (!ptr[1]) {
					voice_set_cvp_handle(v, data->src_port);
					pr_debug("cvphdl=%d\n", data->src_port);
				} else
					pr_err("got NACK from CVP create session response\n");
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
			case VSS_IVOCPROC_CMD_SET_MUTE:
				v->cvp_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvp_wait);
				break;
			case VOICE_CMD_SET_PARAM:
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
	int rc = 0, i = 0;

	memset(&common, 0, sizeof(struct common_data));

	/* set default value */
	common.default_mute_val = 1;  /* default is mute */
	common.default_vol_val = 0;
	common.default_sample_val = 8000;

	/* Initialize MVS info. */
	common.mvs_info.network_type = VSS_NETWORK_ID_DEFAULT;

	mutex_init(&common.common_lock);

	for (i = 0; i < MAX_VOC_SESSIONS; i++) {
		common.voice[i].session_id = SESSION_ID_BASE + i;

		/* initialize dev_rx and dev_tx */
		common.voice[i].dev_rx.volume = common.default_vol_val;
		common.voice[i].dev_rx.mute =  0;
		common.voice[i].dev_tx.mute = common.default_mute_val;

		common.voice[i].dev_tx.port_id = 0x100B;
		common.voice[i].dev_rx.port_id = 0x100A;
		common.voice[i].sidetone_gain = 0x512;

		common.voice[i].voc_state = VOC_INIT;

		init_waitqueue_head(&common.voice[i].mvm_wait);
		init_waitqueue_head(&common.voice[i].cvs_wait);
		init_waitqueue_head(&common.voice[i].cvp_wait);

		mutex_init(&common.voice[i].lock);
	}

	return rc;
}

device_initcall(voice_init);
