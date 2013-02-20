/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/msm_audio.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#include <mach/qdsp6v2/audio_dev_ctl.h>
#include <mach/dal.h>
#include <mach/qdsp6v2/q6voice.h>
#include <mach/qdsp6v2/rtac.h>
#include <mach/qdsp6v2/audio_acdb.h>

#include "q6core.h"


#define TIMEOUT_MS 3000
#define SNDDEV_CAP_TTY 0x20
#define CMD_STATUS_SUCCESS 0
#define CMD_STATUS_FAIL 1

/* Voice session creates passive control sessions for MVM and CVS. */
#define VOC_PATH_PASSIVE 0

/* VoIP session creates full control sessions for MVM and CVS. */
#define VOC_PATH_FULL 1

#define ADSP_VERSION_CVD 0x60300000

#define BUFFER_PAYLOAD_SIZE 4000

#define VOC_REC_NONE 0xFF

struct common_data common;

static bool is_adsp_support_cvd(void)
{
	return (common.adsp_version >= ADSP_VERSION_CVD);
}
static int voice_send_enable_vocproc_cmd(struct voice_data *v);
static int voice_send_netid_timing_cmd(struct voice_data *v);

static void *voice_get_apr_mvm(void)
{
	void *apr_mvm = NULL;

	if (common.voc_path == VOC_PATH_PASSIVE &&
		!(is_adsp_support_cvd()))
		apr_mvm = common.apr_mvm;
	else
		apr_mvm = common.apr_q6_mvm;

	pr_debug("%s: apr_mvm 0x%x\n", __func__, (unsigned int)apr_mvm);

	return apr_mvm;
}

static void voice_set_apr_mvm(void *apr_mvm)
{
	pr_debug("%s: apr_mvm 0x%x\n", __func__, (unsigned int)apr_mvm);

	if (common.voc_path == VOC_PATH_PASSIVE &&
		!(is_adsp_support_cvd()))
		common.apr_mvm = apr_mvm;
	else
		common.apr_q6_mvm = apr_mvm;
}

static void *voice_get_apr_cvs(void)
{
	void *apr_cvs = NULL;

	if (common.voc_path == VOC_PATH_PASSIVE &&
		!(is_adsp_support_cvd()))
		apr_cvs = common.apr_cvs;
	else
		apr_cvs = common.apr_q6_cvs;

	pr_debug("%s: apr_cvs 0x%x\n", __func__, (unsigned int)apr_cvs);

	return apr_cvs;
}

static void voice_set_apr_cvs(void *apr_cvs)
{
	pr_debug("%s: apr_cvs 0x%x\n", __func__, (unsigned int)apr_cvs);

	if (common.voc_path == VOC_PATH_PASSIVE &&
		!(is_adsp_support_cvd()))
		common.apr_cvs = apr_cvs;
	else
		common.apr_q6_cvs = apr_cvs;
	rtac_set_voice_handle(RTAC_CVS, apr_cvs);
}

static void *voice_get_apr_cvp(void)
{
	void *apr_cvp = NULL;

	if (common.voc_path == VOC_PATH_PASSIVE &&
		!(is_adsp_support_cvd()))
		apr_cvp = common.apr_cvp;
	else
		apr_cvp = common.apr_q6_cvp;

	pr_debug("%s: apr_cvp 0x%x\n", __func__, (unsigned int)apr_cvp);

	return apr_cvp;
}

static void voice_set_apr_cvp(void *apr_cvp)
{
	pr_debug("%s: apr_cvp 0x%x\n", __func__, (unsigned int)apr_cvp);

	if (common.voc_path == VOC_PATH_PASSIVE &&
		!(is_adsp_support_cvd()))
		common.apr_cvp = apr_cvp;
	else
		common.apr_q6_cvp = apr_cvp;
	rtac_set_voice_handle(RTAC_CVP, apr_cvp);
}

static u16 voice_get_mvm_handle(struct voice_data *v)
{
	pr_debug("%s: mvm_handle %d\n", __func__, v->mvm_handle);

	return v->mvm_handle;
}

static void voice_set_mvm_handle(struct voice_data *v, u16 mvm_handle)
{
	pr_debug("%s: session 0x%x, mvm_handle %d\n",
		 __func__, (unsigned int)v, mvm_handle);

	v->mvm_handle = mvm_handle;
}

static u16 voice_get_cvs_handle(struct voice_data *v)
{
	pr_debug("%s: cvs_handle %d\n", __func__, v->cvs_handle);

	return v->cvs_handle;
}

static void voice_set_cvs_handle(struct voice_data *v, u16 cvs_handle)
{
	pr_debug("%s: session 0x%x, cvs_handle %d\n",
		 __func__, (unsigned int)v, cvs_handle);

	v->cvs_handle = cvs_handle;
}

static u16 voice_get_cvp_handle(struct voice_data *v)
{
	pr_debug("%s: cvp_handle %d\n", __func__, v->cvp_handle);

	return v->cvp_handle;
}

static void voice_set_cvp_handle(struct voice_data *v, u16 cvp_handle)
{
	pr_debug("%s: session 0x%x, cvp_handle %d\n",
		 __func__, (unsigned int)v, cvp_handle);

	v->cvp_handle = cvp_handle;
}

u16 voice_get_session_id(const char *name)
{
	u16 session_id = 0;

	if (name != NULL) {
		if (!strncmp(name, "Voice session", 13))
			session_id = common.voice[VOC_PATH_PASSIVE].session_id;
		else
			session_id = common.voice[VOC_PATH_FULL].session_id;
	}

	pr_debug("%s: %s has session id 0x%x\n", __func__, name, session_id);

	return session_id;
}

static struct voice_data *voice_get_session(u16 session_id)
{
	struct voice_data *v = NULL;

	if (session_id == 0) {
		mutex_lock(&common.common_lock);

		pr_debug("%s: NULL id, voc_path is %d\n",
			 __func__, common.voc_path);

		if (common.voc_path == VOC_PATH_PASSIVE)
			v = &common.voice[VOC_PATH_PASSIVE];
		else
			v = &common.voice[VOC_PATH_FULL];

		mutex_unlock(&common.common_lock);
	} else if ((session_id >= SESSION_ID_BASE) &&
		   (session_id < SESSION_ID_BASE + MAX_VOC_SESSIONS))  {
		v = &common.voice[session_id - SESSION_ID_BASE];
	} else {
		pr_err("%s: Invalid session_id 0x%x\n", __func__, session_id);
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

static void voice_auddev_cb_function(u32 evt_id,
			union auddev_evt_data *evt_payload,
			void *private_data);

static int32_t modem_mvm_callback(struct apr_client_data *data, void *priv);
static int32_t modem_cvs_callback(struct apr_client_data *data, void *priv);
static int32_t modem_cvp_callback(struct apr_client_data *data, void *priv);

static int voice_apr_register(void)
{
	int rc = 0;
	void *apr_mvm;
	void *apr_cvs;
	void *apr_cvp;

	if (common.adsp_version == 0) {
		common.adsp_version = core_get_adsp_version();
		pr_info("adsp_ver fetched:%x\n", common.adsp_version);
	}

	mutex_lock(&common.common_lock);

	apr_mvm = voice_get_apr_mvm();
	apr_cvs = voice_get_apr_cvs();
	apr_cvp = voice_get_apr_cvp();


	pr_debug("into voice_apr_register_callback\n");
	/* register callback to APR */
	if (apr_mvm == NULL) {
		pr_debug("start to register MVM callback\n");

		if (common.voc_path == VOC_PATH_PASSIVE &&
			!(is_adsp_support_cvd())) {
			apr_mvm = apr_register("MODEM", "MVM",
					       modem_mvm_callback, 0xFFFFFFFF,
					       &common);
		} else {
			apr_mvm = apr_register("ADSP", "MVM",
					       modem_mvm_callback, 0xFFFFFFFF,
					       &common);
		}

		if (apr_mvm == NULL) {
			pr_err("Unable to register MVM %d\n",
						is_adsp_support_cvd());
			rc = -ENODEV;
			goto done;
		}

		voice_set_apr_mvm(apr_mvm);
	}

	if (apr_cvs == NULL) {
		pr_debug("start to register CVS callback\n");

		if (common.voc_path == VOC_PATH_PASSIVE &&
			!(is_adsp_support_cvd())) {
			apr_cvs = apr_register("MODEM", "CVS",
					       modem_cvs_callback, 0xFFFFFFFF,
					       &common);
		} else {
			apr_cvs = apr_register("ADSP", "CVS",
					       modem_cvs_callback, 0xFFFFFFFF,
					       &common);
		}

		if (apr_cvs == NULL) {
			pr_err("Unable to register CVS %d\n",
							is_adsp_support_cvd());
			rc = -ENODEV;
			goto err;
		}

		voice_set_apr_cvs(apr_cvs);
	}

	if (apr_cvp == NULL) {
		pr_debug("start to register CVP callback\n");

		if (common.voc_path == VOC_PATH_PASSIVE &&
			!(is_adsp_support_cvd())) {
			apr_cvp = apr_register("MODEM", "CVP",
					       modem_cvp_callback, 0xFFFFFFFF,
					       &common);
		} else {
			apr_cvp = apr_register("ADSP", "CVP",
					       modem_cvp_callback, 0xFFFFFFFF,
					       &common);
	}

		if (apr_cvp == NULL) {
			pr_err("Unable to register CVP %d\n",
							is_adsp_support_cvd());
			rc = -ENODEV;
			goto err1;
		}

		voice_set_apr_cvp(apr_cvp);
	}

	mutex_unlock(&common.common_lock);

	return 0;

err1:
	apr_deregister(apr_cvs);
	apr_cvs = NULL;
	voice_set_apr_cvs(apr_cvs);
err:
	apr_deregister(apr_mvm);
	apr_mvm = NULL;
	voice_set_apr_mvm(apr_mvm);

done:
	mutex_unlock(&common.common_lock);

	return rc;
}

static int voice_create_mvm_cvs_session(struct voice_data *v)
{
	int ret = 0;
	struct mvm_create_ctl_session_cmd mvm_session_cmd;
	struct cvs_create_passive_ctl_session_cmd cvs_session_cmd;
	struct cvs_create_full_ctl_session_cmd cvs_full_ctl_cmd;
	struct mvm_attach_stream_cmd attach_stream_cmd;
	void *apr_mvm = voice_get_apr_mvm();
	void *apr_cvs = voice_get_apr_cvs();
	void *apr_cvp = voice_get_apr_cvp();
	u16 mvm_handle = voice_get_mvm_handle(v);
	u16 cvs_handle = voice_get_cvs_handle(v);
	u16 cvp_handle = voice_get_cvp_handle(v);

	pr_info("%s:\n", __func__);

	/* start to ping if modem service is up */
	pr_debug("in voice_create_mvm_cvs_session, mvm_hdl=%d, cvs_hdl=%d\n",
					mvm_handle, cvs_handle);
	/* send cmd to create mvm session and wait for response */

	if (!mvm_handle) {
		if (is_voice_session(v->session_id)) {
			mvm_session_cmd.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
			mvm_session_cmd.hdr.pkt_size =
				APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(mvm_session_cmd) - APR_HDR_SIZE);
			pr_debug("Send mvm create session pkt size = %d\n",
				mvm_session_cmd.hdr.pkt_size);
			mvm_session_cmd.hdr.src_port = v->session_id;
			mvm_session_cmd.hdr.dest_port = 0;
			mvm_session_cmd.hdr.token = 0;
			pr_debug("%s: Creating MVM passive ctrl\n", __func__);
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
			mvm_session_cmd.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
			mvm_session_cmd.hdr.pkt_size =
				APR_PKT_SIZE(APR_HDR_SIZE,
				       sizeof(mvm_session_cmd) - APR_HDR_SIZE);
			pr_debug("Send mvm create session pkt size = %d\n",
				mvm_session_cmd.hdr.pkt_size);
			mvm_session_cmd.hdr.src_port = v->session_id;
			mvm_session_cmd.hdr.dest_port = 0;
			mvm_session_cmd.hdr.token = 0;
			pr_debug("%s: Creating MVM full ctrl\n", __func__);
			mvm_session_cmd.hdr.opcode =
				VSS_IMVM_CMD_CREATE_FULL_CONTROL_SESSION;
			strncpy(mvm_session_cmd.mvm_session.name,
				"default voip", SESSION_NAME_LEN);

			v->mvm_state = CMD_STATUS_FAIL;

			ret = apr_send_pkt(apr_mvm,
					   (uint32_t *) &mvm_session_cmd);
			if (ret < 0) {
				pr_err("Error sending MVM_FULL_CTL_SESSION\n");
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
		if (is_voice_session(v->session_id)) {
			pr_info("%s:creating CVS passive session\n", __func__);

		cvs_session_cmd.hdr.hdr_field = APR_HDR_FIELD(
						APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		cvs_session_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(cvs_session_cmd) - APR_HDR_SIZE);
		pr_info("send stream create session pkt size = %d\n",
					cvs_session_cmd.hdr.pkt_size);
		cvs_session_cmd.hdr.src_port = v->session_id;
		cvs_session_cmd.hdr.dest_port = 0;
		cvs_session_cmd.hdr.token = 0;
		cvs_session_cmd.hdr.opcode =
				VSS_ISTREAM_CMD_CREATE_PASSIVE_CONTROL_SESSION;
		strncpy(cvs_session_cmd.cvs_session.name,
			"default modem voice", SESSION_NAME_LEN);

		v->cvs_state = CMD_STATUS_FAIL;

		pr_info("%s: CVS create\n", __func__);
		ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_session_cmd);
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
			pr_info("%s:creating CVS full session\n", __func__);

			cvs_full_ctl_cmd.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					      APR_HDR_LEN(APR_HDR_SIZE),
					      APR_PKT_VER);

			cvs_full_ctl_cmd.hdr.pkt_size =
				APR_PKT_SIZE(APR_HDR_SIZE,
				       sizeof(cvs_full_ctl_cmd) - APR_HDR_SIZE);

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
			strncpy(cvs_full_ctl_cmd.cvs_session.name,
				"default voip", SESSION_NAME_LEN);

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
			pr_info("%s: Attach MVM to stream\n", __func__);

			attach_stream_cmd.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					      APR_HDR_LEN(APR_HDR_SIZE),
					      APR_PKT_VER);

			attach_stream_cmd.hdr.pkt_size =
				APR_PKT_SIZE(APR_HDR_SIZE,
				      sizeof(attach_stream_cmd) - APR_HDR_SIZE);
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
	apr_deregister(apr_mvm);
	apr_mvm = NULL;
	voice_set_apr_mvm(apr_mvm);

	apr_deregister(apr_cvs);
	apr_cvs = NULL;
	voice_set_apr_cvs(apr_cvs);

	apr_deregister(apr_cvp);
	apr_cvp = NULL;
	voice_set_apr_cvp(apr_cvp);

	cvp_handle = 0;
	voice_set_cvp_handle(v, cvp_handle);

	cvs_handle = 0;
	voice_set_cvs_handle(v, cvs_handle);

	return -EINVAL;
}

static int voice_destroy_mvm_cvs_session(struct voice_data *v)
{
	int ret = 0;
	struct mvm_detach_stream_cmd detach_stream;
	struct apr_hdr mvm_destroy;
	struct apr_hdr cvs_destroy;
	void *apr_mvm = voice_get_apr_mvm();
	void *apr_cvs = voice_get_apr_cvs();
	u16 mvm_handle = voice_get_mvm_handle(v);
	u16 cvs_handle = voice_get_cvs_handle(v);

	/* MVM, CVS sessions are destroyed only for Full control sessions. */
	if (is_voip_session(v->session_id)) {
		pr_info("%s: MVM detach stream\n", __func__);

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
		pr_info("%s: CVS destroy session\n", __func__);

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
		pr_info("%s: MVM destroy session\n", __func__);

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

fail:
	return 0;
}

static int voice_send_tty_mode_to_modem(struct voice_data *v)
{
	struct msm_snddev_info *dev_tx_info;
	struct msm_snddev_info *dev_rx_info;
	int tty_mode = 0;
	int ret = 0;
	struct mvm_set_tty_mode_cmd mvm_tty_mode_cmd;
	void *apr_mvm = voice_get_apr_mvm();
	u16 mvm_handle = voice_get_mvm_handle(v);

	dev_rx_info = audio_dev_ctrl_find_dev(v->dev_rx.dev_id);
	if (IS_ERR(dev_rx_info)) {
		pr_err("bad dev_id %d\n", v->dev_rx.dev_id);
		goto done;
	}

	dev_tx_info = audio_dev_ctrl_find_dev(v->dev_tx.dev_id);
	if (IS_ERR(dev_tx_info)) {
		pr_err("bad dev_id %d\n", v->dev_tx.dev_id);
		goto done;
	}

	if ((dev_rx_info->capability & SNDDEV_CAP_TTY) &&
		(dev_tx_info->capability & SNDDEV_CAP_TTY))
		tty_mode = 3; /* FULL */
	else if (!(dev_tx_info->capability & SNDDEV_CAP_TTY) &&
		(dev_rx_info->capability & SNDDEV_CAP_TTY))
		tty_mode = 2; /* VCO */
	else if ((dev_tx_info->capability & SNDDEV_CAP_TTY) &&
		!(dev_rx_info->capability & SNDDEV_CAP_TTY))
		tty_mode = 1; /* HCO */

	if (tty_mode) {
		/* send tty mode cmd to mvm */
		mvm_tty_mode_cmd.hdr.hdr_field = APR_HDR_FIELD(
			APR_MSG_TYPE_SEQ_CMD, APR_HDR_LEN(APR_HDR_SIZE),
								APR_PKT_VER);
		mvm_tty_mode_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
			sizeof(mvm_tty_mode_cmd) - APR_HDR_SIZE);
		pr_debug("pkt size = %d\n", mvm_tty_mode_cmd.hdr.pkt_size);
		mvm_tty_mode_cmd.hdr.src_port = v->session_id;
		mvm_tty_mode_cmd.hdr.dest_port = mvm_handle;
		mvm_tty_mode_cmd.hdr.token = 0;
		mvm_tty_mode_cmd.hdr.opcode = VSS_ISTREAM_CMD_SET_TTY_MODE;
		mvm_tty_mode_cmd.tty_mode.mode = tty_mode;
		pr_info("tty mode =%d\n", mvm_tty_mode_cmd.tty_mode.mode);

		v->mvm_state = CMD_STATUS_FAIL;
		pr_info("%s: MVM set tty\n", __func__);
		ret = apr_send_pkt(apr_mvm, (uint32_t *) &mvm_tty_mode_cmd);
		if (ret < 0) {
			pr_err("Fail: sending VSS_ISTREAM_CMD_SET_TTY_MODE\n");
			goto done;
		}
		ret = wait_event_timeout(v->mvm_wait,
					 (v->mvm_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);
			goto done;
		}
	}
	return 0;
done:
	return -EINVAL;
}

static int voice_send_cvs_cal_to_modem(struct voice_data *v)
{
	struct apr_hdr cvs_cal_cmd_hdr;
	uint32_t *cmd_buf;
	struct acdb_cal_data cal_data;
	struct acdb_atomic_cal_block *cal_blk;
	int32_t cal_size_per_network;
	uint32_t *cal_data_per_network;
	int index = 0;
	int ret = 0;
	void *apr_cvs = voice_get_apr_cvs();
	u16 cvs_handle = voice_get_cvs_handle(v);

	/* fill the header */
	cvs_cal_cmd_hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvs_cal_cmd_hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvs_cal_cmd_hdr) - APR_HDR_SIZE);
	cvs_cal_cmd_hdr.src_port = v->session_id;
	cvs_cal_cmd_hdr.dest_port = cvs_handle;
	cvs_cal_cmd_hdr.token = 0;
	cvs_cal_cmd_hdr.opcode =
		VSS_ISTREAM_CMD_CACHE_CALIBRATION_DATA;

	pr_debug("voice_send_cvs_cal_to_modem\n");
	/* get the cvs cal data */
	get_vocstrm_cal(&cal_data);
	if (cal_data.num_cal_blocks == 0) {
		pr_err("%s: No calibration data to send!\n", __func__);
		goto done;
	}

	/* send cvs cal to modem */
	cmd_buf = kzalloc((sizeof(struct apr_hdr) + BUFFER_PAYLOAD_SIZE),
								GFP_KERNEL);
	if (!cmd_buf) {
		pr_err("No memory is allocated.\n");
		return -ENOMEM;
	}
	pr_debug("----- num_cal_blocks=%d\n", (s32)cal_data.num_cal_blocks);
	cal_blk = cal_data.cal_blocks;
	pr_debug("cal_blk =%x\n", (uint32_t)cal_data.cal_blocks);

	for (; index < cal_data.num_cal_blocks; index++) {
		cal_size_per_network = atomic_read(&cal_blk[index].cal_size);
		pr_debug(" cal size =%d\n", cal_size_per_network);
		if (cal_size_per_network >= BUFFER_PAYLOAD_SIZE)
			pr_err("Cal size is too big\n");
		cal_data_per_network =
			(u32 *)atomic_read(&cal_blk[index].cal_kvaddr);
		pr_debug(" cal data=%x\n", (uint32_t)cal_data_per_network);
		cvs_cal_cmd_hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
			cal_size_per_network);
		pr_debug("header size =%d,  pkt_size =%d\n",
			APR_HDR_SIZE, cvs_cal_cmd_hdr.pkt_size);
		memcpy(cmd_buf, &cvs_cal_cmd_hdr,  APR_HDR_SIZE);
		memcpy(cmd_buf + (APR_HDR_SIZE / sizeof(uint32_t)),
			cal_data_per_network, cal_size_per_network);
		pr_debug("send cvs cal: index =%d\n", index);
		v->cvs_state = CMD_STATUS_FAIL;
		ret = apr_send_pkt(apr_cvs, cmd_buf);
		if (ret < 0) {
			pr_err("Fail: sending cvs cal, idx=%d\n", index);
			continue;
		}
		ret = wait_event_timeout(v->cvs_wait,
					 (v->cvs_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);
			return -EINVAL;
		}
	}
	kfree(cmd_buf);
done:
	return 0;
}

static int voice_send_cvp_cal_to_modem(struct voice_data *v)
{
	struct apr_hdr cvp_cal_cmd_hdr;
	uint32_t *cmd_buf;
	struct acdb_cal_data cal_data;
	struct acdb_atomic_cal_block *cal_blk;
	int32_t cal_size_per_network;
	uint32_t *cal_data_per_network;
	int index = 0;
	int ret = 0;
	void *apr_cvp = voice_get_apr_cvp();
	u16 cvp_handle = voice_get_cvp_handle(v);


	/* fill the header */
	cvp_cal_cmd_hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_cal_cmd_hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_cal_cmd_hdr) - APR_HDR_SIZE);
	cvp_cal_cmd_hdr.src_port = v->session_id;
	cvp_cal_cmd_hdr.dest_port = cvp_handle;
	cvp_cal_cmd_hdr.token = 0;
	cvp_cal_cmd_hdr.opcode =
		VSS_IVOCPROC_CMD_CACHE_CALIBRATION_DATA;

	/* get cal data */
	get_vocproc_cal(&cal_data);
	if (cal_data.num_cal_blocks == 0) {
		pr_err("%s: No calibration data to send!\n", __func__);
		goto done;
	}

	/* send cal to modem */
	cmd_buf = kzalloc((sizeof(struct apr_hdr) + BUFFER_PAYLOAD_SIZE),
								GFP_KERNEL);
	if (!cmd_buf) {
		pr_err("No memory is allocated.\n");
		return -ENOMEM;
	}
	pr_debug("----- num_cal_blocks=%d\n", (s32)cal_data.num_cal_blocks);
	cal_blk = cal_data.cal_blocks;
	pr_debug(" cal_blk =%x\n", (uint32_t)cal_data.cal_blocks);

	for (; index < cal_data.num_cal_blocks; index++) {
		cal_size_per_network = atomic_read(&cal_blk[index].cal_size);
		if (cal_size_per_network >= BUFFER_PAYLOAD_SIZE)
			pr_err("Cal size is too big\n");
		pr_debug(" cal size =%d\n", cal_size_per_network);
		cal_data_per_network =
			(u32 *)atomic_read(&cal_blk[index].cal_kvaddr);
		pr_debug(" cal data=%x\n", (uint32_t)cal_data_per_network);

		cvp_cal_cmd_hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
			cal_size_per_network);
		memcpy(cmd_buf, &cvp_cal_cmd_hdr,  APR_HDR_SIZE);
		memcpy(cmd_buf + (APR_HDR_SIZE / sizeof(*cmd_buf)),
			cal_data_per_network, cal_size_per_network);
		pr_debug("Send cvp cal\n");
		v->cvp_state = CMD_STATUS_FAIL;
		pr_info("%s: CVP calib\n", __func__);
		ret = apr_send_pkt(apr_cvp, cmd_buf);
		if (ret < 0) {
			pr_err("Fail: sending cvp cal, idx=%d\n", index);
			continue;
		}
		ret = wait_event_timeout(v->cvp_wait,
					 (v->cvp_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);
			return -EINVAL;
		}
	}
	kfree(cmd_buf);
done:
	return 0;
}

static int voice_send_cvp_vol_tbl_to_modem(struct voice_data *v)
{
	struct apr_hdr cvp_vol_cal_cmd_hdr;
	uint32_t *cmd_buf;
	struct acdb_cal_data cal_data;
	struct acdb_atomic_cal_block *cal_blk;
	int32_t cal_size_per_network;
	uint32_t *cal_data_per_network;
	int index = 0;
	int ret = 0;
	void *apr_cvp = voice_get_apr_cvp();
	u16 cvp_handle = voice_get_cvp_handle(v);


	/* fill the header */
	cvp_vol_cal_cmd_hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_vol_cal_cmd_hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_vol_cal_cmd_hdr) - APR_HDR_SIZE);
	cvp_vol_cal_cmd_hdr.src_port = v->session_id;
	cvp_vol_cal_cmd_hdr.dest_port = cvp_handle;
	cvp_vol_cal_cmd_hdr.token = 0;
	cvp_vol_cal_cmd_hdr.opcode =
		VSS_IVOCPROC_CMD_CACHE_VOLUME_CALIBRATION_TABLE;

	/* get cal data */
	get_vocvol_cal(&cal_data);
	if (cal_data.num_cal_blocks == 0) {
		pr_err("%s: No calibration data to send!\n", __func__);
		goto done;
	}

	/* send cal to modem */
	cmd_buf = kzalloc((sizeof(struct apr_hdr) + BUFFER_PAYLOAD_SIZE),
								GFP_KERNEL);
	if (!cmd_buf) {
		pr_err("No memory is allocated.\n");
		return -ENOMEM;
	}
	pr_debug("----- num_cal_blocks=%d\n", (s32)cal_data.num_cal_blocks);
	cal_blk = cal_data.cal_blocks;
	pr_debug("Cal_blk =%x\n", (uint32_t)cal_data.cal_blocks);

	for (; index < cal_data.num_cal_blocks; index++) {
		cal_size_per_network = atomic_read(&cal_blk[index].cal_size);
		cal_data_per_network =
			(u32 *)atomic_read(&cal_blk[index].cal_kvaddr);

		pr_debug("Cal size =%d, index=%d\n", cal_size_per_network,
			index);
		pr_debug("Cal data=%x\n", (uint32_t)cal_data_per_network);
		cvp_vol_cal_cmd_hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
			cal_size_per_network);
		memcpy(cmd_buf, &cvp_vol_cal_cmd_hdr,  APR_HDR_SIZE);
		memcpy(cmd_buf + (APR_HDR_SIZE / sizeof(uint32_t)),
			cal_data_per_network, cal_size_per_network);
		pr_debug("Send vol table\n");

		v->cvp_state = CMD_STATUS_FAIL;
		ret = apr_send_pkt(apr_cvp, cmd_buf);
		if (ret < 0) {
			pr_err("Fail: sending cvp vol cal, idx=%d\n", index);
			continue;
		}
		ret = wait_event_timeout(v->cvp_wait,
					 (v->cvp_state == CMD_STATUS_SUCCESS),
			msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);
			return -EINVAL;
		}
	}
	kfree(cmd_buf);
done:
	return 0;
}

static int voice_set_dtx(struct voice_data *v)
{
	int ret = 0;
	void *apr_cvs = voice_get_apr_cvs();
	u16 cvs_handle = voice_get_cvs_handle(v);

	/* Set DTX */
	struct cvs_set_enc_dtx_mode_cmd cvs_set_dtx = {
		.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					      APR_HDR_LEN(APR_HDR_SIZE),
					      APR_PKT_VER),
		.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(cvs_set_dtx) - APR_HDR_SIZE),
		.hdr.src_port = v->session_id,
		.hdr.dest_port = cvs_handle,
		.hdr.token = 0,
		.hdr.opcode = VSS_ISTREAM_CMD_SET_ENC_DTX_MODE,
		.dtx_mode.enable = common.mvs_info.dtx_mode,
	};

	pr_debug("%s: Setting DTX %d\n", __func__, common.mvs_info.dtx_mode);

	v->cvs_state = CMD_STATUS_FAIL;

	ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_set_dtx);
	if (ret < 0) {
		pr_err("%s: Error %d sending SET_DTX\n", __func__, ret);

		goto done;
	}

	ret = wait_event_timeout(v->cvs_wait,
				 (v->cvs_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);

		ret = -EINVAL;
	}

done:
	return ret;
}

static int voice_config_cvs_vocoder(struct voice_data *v)
{
	int ret = 0;
	void *apr_cvs = voice_get_apr_cvs();
	u16 cvs_handle = voice_get_cvs_handle(v);

	/* Set media type. */
	struct cvs_set_media_type_cmd cvs_set_media_cmd;

	pr_info("%s: Setting media type\n", __func__);

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

		goto done;
	}

	ret = wait_event_timeout(v->cvs_wait,
				 (v->cvs_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	/* Set encoder properties. */
	switch (common.mvs_info.media_type) {
	case VSS_MEDIA_ID_13K_MODEM:
	case VSS_MEDIA_ID_4GV_NB_MODEM:
	case VSS_MEDIA_ID_4GV_WB_MODEM:
	case VSS_MEDIA_ID_EVRC_MODEM: {
		struct cvs_set_cdma_enc_minmax_rate_cmd cvs_set_cdma_rate;

		pr_info("%s: Setting CDMA min-max rate\n", __func__);

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
		cvs_set_cdma_rate.cdma_rate.min_rate =
				common.mvs_info.q_min_max_rate.min_rate;
		cvs_set_cdma_rate.cdma_rate.max_rate =
				common.mvs_info.q_min_max_rate.max_rate;

		v->cvs_state = CMD_STATUS_FAIL;

		ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_set_cdma_rate);
		if (ret < 0) {
			pr_err("%s: Error %d sending CDMA_SET_ENC_MINMAX_RATE\n",
			       __func__, ret);

			goto done;
		}

		ret = wait_event_timeout(v->cvs_wait,
					 (v->cvs_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);

			ret = -EINVAL;
			goto done;
		}

		if ((common.mvs_info.media_type == VSS_MEDIA_ID_4GV_NB_MODEM) ||
		(common.mvs_info.media_type == VSS_MEDIA_ID_4GV_WB_MODEM))
			ret = voice_set_dtx(v);

		break;
	}

	case VSS_MEDIA_ID_AMR_NB_MODEM: {
		struct cvs_set_amr_enc_rate_cmd cvs_set_amr_rate;

		pr_info("%s: Setting AMR rate\n", __func__);

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

			goto done;
		}

		ret = wait_event_timeout(v->cvs_wait,
					 (v->cvs_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);

			ret = -EINVAL;
			goto done;
		}

		ret = voice_set_dtx(v);

		break;
	}

	case VSS_MEDIA_ID_AMR_WB_MODEM: {
		struct cvs_set_amrwb_enc_rate_cmd cvs_set_amrwb_rate;

		pr_info("%s: Setting AMR WB rate\n", __func__);

		cvs_set_amrwb_rate.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					      APR_HDR_LEN(APR_HDR_SIZE),
					      APR_PKT_VER);
		cvs_set_amrwb_rate.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				     sizeof(cvs_set_amrwb_rate) - APR_HDR_SIZE);
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

			goto done;
		}

		ret = wait_event_timeout(v->cvs_wait,
					 (v->cvs_state == CMD_STATUS_SUCCESS),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait_event timeout\n", __func__);

			ret = -EINVAL;
			goto done;
		}

		ret = voice_set_dtx(v);

		break;
	}

	case VSS_MEDIA_ID_EFR_MODEM:
	case VSS_MEDIA_ID_FR_MODEM:
	case VSS_MEDIA_ID_HR_MODEM:
	case VSS_MEDIA_ID_G729:
	case VSS_MEDIA_ID_G711_ALAW:
	case VSS_MEDIA_ID_G711_MULAW: {
		ret = voice_set_dtx(v);

		break;
	}

	default: {
		/* Do nothing. */
	}
	}

done:
	return ret;
}

static int voice_send_start_voice_cmd(struct voice_data *v)
{
	struct apr_hdr mvm_start_voice_cmd;
	int ret = 0;
	void *apr_mvm = voice_get_apr_mvm();
	u16 mvm_handle = voice_get_mvm_handle(v);

	mvm_start_voice_cmd.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	mvm_start_voice_cmd.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(mvm_start_voice_cmd) - APR_HDR_SIZE);
	pr_info("send mvm_start_voice_cmd pkt size = %d\n",
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

static int voice_disable_vocproc(struct voice_data *v)
{
	struct apr_hdr cvp_disable_cmd;
	int ret = 0;
	void *apr_cvp = voice_get_apr_cvp();
	u16 cvp_handle = voice_get_cvp_handle(v);

	/* disable vocproc and wait for respose */
	cvp_disable_cmd.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
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
	rtac_remove_voice(v->cvs_handle);

	return 0;
fail:
	return -EINVAL;
}

static int voice_set_device(struct voice_data *v)
{
	struct cvp_set_device_cmd  cvp_setdev_cmd;
	struct msm_snddev_info *dev_tx_info;
	int ret = 0;
	void *apr_cvp = voice_get_apr_cvp();
	u16 cvp_handle = voice_get_cvp_handle(v);


	/* set device and wait for response */
	cvp_setdev_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_setdev_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_setdev_cmd) - APR_HDR_SIZE);
	pr_debug(" send create cvp setdev, pkt size = %d\n",
			cvp_setdev_cmd.hdr.pkt_size);
	cvp_setdev_cmd.hdr.src_port = v->session_id;
	cvp_setdev_cmd.hdr.dest_port = cvp_handle;
	cvp_setdev_cmd.hdr.token = 0;
	cvp_setdev_cmd.hdr.opcode = VSS_IVOCPROC_CMD_SET_DEVICE;

	dev_tx_info = audio_dev_ctrl_find_dev(v->dev_tx.dev_id);
	if (IS_ERR(dev_tx_info)) {
		pr_err("bad dev_id %d\n", v->dev_tx.dev_id);
		goto fail;
	}

	cvp_setdev_cmd.cvp_set_device.tx_topology_id =
				get_voice_tx_topology();
	if (cvp_setdev_cmd.cvp_set_device.tx_topology_id == 0) {
		if (dev_tx_info->channel_mode > 1)
			cvp_setdev_cmd.cvp_set_device.tx_topology_id =
				VSS_IVOCPROC_TOPOLOGY_ID_TX_DM_FLUENCE;
		else
			cvp_setdev_cmd.cvp_set_device.tx_topology_id =
				VSS_IVOCPROC_TOPOLOGY_ID_TX_SM_ECNS;
	}

	/* Use default topology if invalid value in ACDB */
	cvp_setdev_cmd.cvp_set_device.rx_topology_id =
				get_voice_rx_topology();
	if (cvp_setdev_cmd.cvp_set_device.rx_topology_id == 0)
		cvp_setdev_cmd.cvp_set_device.rx_topology_id =
			VSS_IVOCPROC_TOPOLOGY_ID_RX_DEFAULT;
	cvp_setdev_cmd.cvp_set_device.tx_port_id = v->dev_tx.dev_port_id;
	cvp_setdev_cmd.cvp_set_device.rx_port_id = v->dev_rx.dev_port_id;
	pr_info("topology=%d , tx_port_id=%d, rx_port_id=%d\n",
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

	/* send cvs cal */
	voice_send_cvs_cal_to_modem(v);

	/* send cvp cal */
	voice_send_cvp_cal_to_modem(v);

	/* send cvp vol table cal */
	voice_send_cvp_vol_tbl_to_modem(v);

	/* enable vocproc and wait for respose */
	voice_send_enable_vocproc_cmd(v);

	/* send tty mode if tty device is used */
	voice_send_tty_mode_to_modem(v);

	if (is_voip_session(v->session_id))
		voice_send_netid_timing_cmd(v);

	rtac_add_voice(v->cvs_handle, v->cvp_handle,
		v->dev_rx.dev_port_id, v->dev_tx.dev_port_id,
		v->session_id);

	return 0;
fail:
	return -EINVAL;
}

static int voice_send_stop_voice_cmd(struct voice_data *v)
{
	struct apr_hdr mvm_stop_voice_cmd;
	int ret = 0;
	void *apr_mvm = voice_get_apr_mvm();
	u16 mvm_handle = voice_get_mvm_handle(v);

	mvm_stop_voice_cmd.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	mvm_stop_voice_cmd.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(mvm_stop_voice_cmd) - APR_HDR_SIZE);
	pr_info("send mvm_stop_voice_cmd pkt size = %d\n",
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

static int voice_setup_modem_voice(struct voice_data *v)
{
	struct cvp_create_full_ctl_session_cmd cvp_session_cmd;
	int ret = 0;
	struct msm_snddev_info *dev_tx_info;
	void *apr_cvp = voice_get_apr_cvp();

	/* create cvp session and wait for response */
	cvp_session_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_session_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_session_cmd) - APR_HDR_SIZE);
	pr_info(" send create cvp session, pkt size = %d\n",
				cvp_session_cmd.hdr.pkt_size);
	cvp_session_cmd.hdr.src_port = v->session_id;
	cvp_session_cmd.hdr.dest_port = 0;
	cvp_session_cmd.hdr.token = 0;
	cvp_session_cmd.hdr.opcode =
		VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION;

	dev_tx_info = audio_dev_ctrl_find_dev(v->dev_tx.dev_id);
	if (IS_ERR(dev_tx_info)) {
		pr_err("bad dev_id %d\n", v->dev_tx.dev_id);
		goto fail;
	}

	/* Use default topology if invalid value in ACDB */
	cvp_session_cmd.cvp_session.tx_topology_id =
				get_voice_tx_topology();
	if (cvp_session_cmd.cvp_session.tx_topology_id == 0) {
		if (dev_tx_info->channel_mode > 1)
			cvp_session_cmd.cvp_session.tx_topology_id =
				VSS_IVOCPROC_TOPOLOGY_ID_TX_DM_FLUENCE;
		else
			cvp_session_cmd.cvp_session.tx_topology_id =
				VSS_IVOCPROC_TOPOLOGY_ID_TX_SM_ECNS;
	}

	cvp_session_cmd.cvp_session.rx_topology_id =
				get_voice_rx_topology();
	if (cvp_session_cmd.cvp_session.rx_topology_id == 0)
		cvp_session_cmd.cvp_session.rx_topology_id =
			VSS_IVOCPROC_TOPOLOGY_ID_RX_DEFAULT;

	cvp_session_cmd.cvp_session.direction = 2; /*tx and rx*/
	cvp_session_cmd.cvp_session.network_id = VSS_NETWORK_ID_DEFAULT;
	cvp_session_cmd.cvp_session.tx_port_id = v->dev_tx.dev_port_id;
	cvp_session_cmd.cvp_session.rx_port_id = v->dev_rx.dev_port_id;
	pr_info("topology=%d net_id=%d, dir=%d tx_port_id=%d, rx_port_id=%d\n",
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
	pr_debug("wait for cvp create session event\n");
	ret = wait_event_timeout(v->cvp_wait,
				 (v->cvp_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	/* send cvs cal */
	voice_send_cvs_cal_to_modem(v);

	/* send cvp cal */
	voice_send_cvp_cal_to_modem(v);

	/* send cvp vol table cal */
	voice_send_cvp_vol_tbl_to_modem(v);

	return 0;

fail:
	return -EINVAL;
}

static int voice_send_enable_vocproc_cmd(struct voice_data *v)
{
	int ret = 0;
	struct apr_hdr cvp_enable_cmd;

	u16 cvp_handle = voice_get_cvp_handle(v);
	void *apr_cvp = voice_get_apr_cvp();

	/* enable vocproc and wait for respose */
	cvp_enable_cmd.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
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
	void *apr_mvm = voice_get_apr_mvm();
	struct mvm_set_network_cmd mvm_set_network;
	struct mvm_set_voice_timing_cmd mvm_set_voice_timing;
	u16 mvm_handle = voice_get_mvm_handle(v);

	ret = voice_config_cvs_vocoder(v);
	if (ret < 0) {
		pr_err("%s: Error %d configuring CVS voc",
					__func__, ret);
		goto fail;
	}
	/* Set network ID. */
	pr_debug("%s: Setting network ID\n", __func__);

	mvm_set_network.hdr.hdr_field =
			APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
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
	 pr_debug("%s: Setting voice timing\n", __func__);

	mvm_set_voice_timing.hdr.hdr_field =
			APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	mvm_set_voice_timing.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
			sizeof(mvm_set_voice_timing) - APR_HDR_SIZE);
	mvm_set_voice_timing.hdr.src_port = v->session_id;
	mvm_set_voice_timing.hdr.dest_port = mvm_handle;
	mvm_set_voice_timing.hdr.token = 0;
	mvm_set_voice_timing.hdr.opcode =
			VSS_ICOMMON_CMD_SET_VOICE_TIMING;
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

static int voice_attach_vocproc(struct voice_data *v)
{
	int ret = 0;
	struct mvm_attach_vocproc_cmd mvm_a_vocproc_cmd;
	void *apr_mvm = voice_get_apr_mvm();
	u16 mvm_handle = voice_get_mvm_handle(v);
	u16 cvp_handle = voice_get_cvp_handle(v);

	/* send enable vocproc */
	voice_send_enable_vocproc_cmd(v);

	/* attach vocproc and wait for response */
	mvm_a_vocproc_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	mvm_a_vocproc_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(mvm_a_vocproc_cmd) - APR_HDR_SIZE);
	pr_info("send mvm_a_vocproc_cmd pkt size = %d\n",
				mvm_a_vocproc_cmd.hdr.pkt_size);
	mvm_a_vocproc_cmd.hdr.src_port = v->session_id;
	mvm_a_vocproc_cmd.hdr.dest_port = mvm_handle;
	mvm_a_vocproc_cmd.hdr.token = 0;
	mvm_a_vocproc_cmd.hdr.opcode = VSS_ISTREAM_CMD_ATTACH_VOCPROC;
	mvm_a_vocproc_cmd.mvm_attach_cvp_handle.handle = cvp_handle;

	v->mvm_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_mvm, (uint32_t *) &mvm_a_vocproc_cmd);
	if (ret < 0) {
		pr_err("Fail in sending VSS_ISTREAM_CMD_ATTACH_VOCPROC\n");
		goto fail;
	}
	ret = wait_event_timeout(v->mvm_wait,
				 (v->mvm_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	/* send tty mode if tty device is used */
	voice_send_tty_mode_to_modem(v);

	if (is_voip_session(v->session_id))
		voice_send_netid_timing_cmd(v);

	rtac_add_voice(v->cvs_handle, v->cvp_handle,
		v->dev_rx.dev_port_id, v->dev_tx.dev_port_id,
		v->session_id);

	return 0;
fail:
	return -EINVAL;
}

static int voice_destroy_modem_voice(struct voice_data *v)
{
	struct mvm_detach_vocproc_cmd mvm_d_vocproc_cmd;
	struct apr_hdr cvp_destroy_session_cmd;
	int ret = 0;
	void *apr_mvm = voice_get_apr_mvm();
	void *apr_cvp = voice_get_apr_cvp();
	u16 mvm_handle = voice_get_mvm_handle(v);
	u16 cvp_handle = voice_get_cvp_handle(v);

	/* detach VOCPROC and wait for response from mvm */
	mvm_d_vocproc_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	mvm_d_vocproc_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(mvm_d_vocproc_cmd) - APR_HDR_SIZE);
	pr_info("mvm_d_vocproc_cmd  pkt size = %d\n",
				mvm_d_vocproc_cmd.hdr.pkt_size);
	mvm_d_vocproc_cmd.hdr.src_port = v->session_id;
	mvm_d_vocproc_cmd.hdr.dest_port = mvm_handle;
	mvm_d_vocproc_cmd.hdr.token = 0;
	mvm_d_vocproc_cmd.hdr.opcode = VSS_ISTREAM_CMD_DETACH_VOCPROC;
	mvm_d_vocproc_cmd.mvm_detach_cvp_handle.handle = cvp_handle;

	v->mvm_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_mvm, (uint32_t *) &mvm_d_vocproc_cmd);
	if (ret < 0) {
		pr_err("Fail in sending VSS_ISTREAM_CMD_DETACH_VOCPROC\n");
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
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_destroy_session_cmd.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_destroy_session_cmd) - APR_HDR_SIZE);
	pr_info("cvp_destroy_session_cmd pkt size = %d\n",
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
	rtac_remove_voice(v->cvs_handle);
	cvp_handle = 0;
	voice_set_cvp_handle(v, cvp_handle);

	return 0;

fail:
	return -EINVAL;
}

static int voice_send_mute_cmd_to_modem(struct voice_data *v)
{
	struct cvs_set_mute_cmd cvs_mute_cmd;
	int ret = 0;
	void *apr_cvs = voice_get_apr_cvs();
	u16 cvs_handle = voice_get_cvs_handle(v);

	/* send mute/unmute to cvs */
	cvs_mute_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvs_mute_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvs_mute_cmd) - APR_HDR_SIZE);
	cvs_mute_cmd.hdr.src_port = v->session_id;
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

fail:
	return 0;
}

static int voice_send_vol_index_to_modem(struct voice_data *v)
{
	struct cvp_set_rx_volume_index_cmd cvp_vol_cmd;
	int ret = 0;
	void *apr_cvp = voice_get_apr_cvp();
	u16 cvp_handle = voice_get_cvp_handle(v);

	/* send volume index to cvp */
	cvp_vol_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_vol_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_vol_cmd) - APR_HDR_SIZE);
	cvp_vol_cmd.hdr.src_port = v->session_id;
	cvp_vol_cmd.hdr.dest_port = cvp_handle;
	cvp_vol_cmd.hdr.token = 0;
	cvp_vol_cmd.hdr.opcode =
		VSS_IVOCPROC_CMD_SET_RX_VOLUME_INDEX;
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
	void *apr_cvs = voice_get_apr_cvs();
	u16 cvs_handle = voice_get_cvs_handle(v);
	struct cvs_start_record_cmd cvs_start_record;

	pr_debug("%s: Start record %d\n", __func__, rec_mode);

	cvs_start_record.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				  APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvs_start_record.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				  sizeof(cvs_start_record) - APR_HDR_SIZE);
	cvs_start_record.hdr.src_port = v->session_id;
	cvs_start_record.hdr.dest_port = cvs_handle;
	cvs_start_record.hdr.token = 0;
	cvs_start_record.hdr.opcode = VSS_ISTREAM_CMD_START_RECORD;

	if (rec_mode == VOC_REC_UPLINK) {
		cvs_start_record.rec_mode.rx_tap_point = VSS_TAP_POINT_NONE;
		cvs_start_record.rec_mode.tx_tap_point =
						VSS_TAP_POINT_STREAM_END;
	} else if (rec_mode == VOC_REC_DOWNLINK) {
		cvs_start_record.rec_mode.rx_tap_point =
						VSS_TAP_POINT_STREAM_END;
		cvs_start_record.rec_mode.tx_tap_point = VSS_TAP_POINT_NONE;
	} else if (rec_mode == VOC_REC_BOTH) {
		cvs_start_record.rec_mode.rx_tap_point =
						VSS_TAP_POINT_STREAM_END;
		cvs_start_record.rec_mode.tx_tap_point =
						VSS_TAP_POINT_STREAM_END;
	} else {
		pr_err("%s: Invalid in-call rec_mode %d\n", __func__, rec_mode);

		ret = -EINVAL;
		goto fail;
	}

	v->cvs_state = CMD_STATUS_FAIL;

	ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_start_record);
	if (ret < 0) {
		pr_err("%s: Error %d sending START_RECORD\n", __func__, ret);

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
	return ret;
}

static int voice_cvs_stop_record(struct voice_data *v)
{
	int ret = 0;
	void *apr_cvs = voice_get_apr_cvs();
	u16 cvs_handle = voice_get_cvs_handle(v);
	struct apr_hdr cvs_stop_record;

	pr_debug("%s: Stop record\n", __func__);

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
		pr_err("%s: Error %d sending STOP_RECORD\n", __func__, ret);

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
	return ret;
}

int voice_start_record(uint32_t rec_mode, uint32_t set)
{
	int ret = 0, i;
	u16 cvs_handle;

	pr_debug("%s: rec_mode %d, set %d\n", __func__, rec_mode, set);

	for (i = 0; i < MAX_VOC_SESSIONS; i++) {
		struct voice_data *v = &common.voice[i];

		mutex_lock(&v->lock);

		cvs_handle = voice_get_cvs_handle(v);

		if (cvs_handle != 0) {
			if (set)
				ret = voice_cvs_start_record(v, rec_mode);
			else
				ret = voice_cvs_stop_record(v);
		} else {
			/* Cache the value for later. */
			v->rec_info.pending = set;
			v->rec_info.rec_mode = rec_mode;
		}

		mutex_unlock(&v->lock);
	}

	return ret;
}

static int voice_cvs_start_playback(struct voice_data *v)
{
	int ret = 0;
	void *apr_cvs = voice_get_apr_cvs();
	u16 cvs_handle = voice_get_cvs_handle(v);
	struct apr_hdr cvs_start_playback;

	pr_debug("%s: Start playback\n", __func__);

	cvs_start_playback.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
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

	return 0;

fail:
	return ret;
}

static int voice_cvs_stop_playback(struct voice_data *v)
{
	int ret = 0;
	void *apr_cvs = voice_get_apr_cvs();
	u16 cvs_handle = voice_get_cvs_handle(v);
	struct apr_hdr cvs_stop_playback;

	pr_debug("%s: Stop playback\n", __func__);

	if (v->music_info.playing) {
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
	} else {
		pr_err("%s: Stop playback already sent\n", __func__);
	}

	return 0;

fail:
	return ret;
}

int voice_start_playback(uint32_t set)
{
	int ret = 0, i;
	u16 cvs_handle;

	pr_debug("%s: Start playback %d\n", __func__, set);

	for (i = 0; i < MAX_VOC_SESSIONS; i++) {
		struct voice_data *v = &common.voice[i];

		mutex_lock(&v->lock);

		cvs_handle = voice_get_cvs_handle(v);

		if (cvs_handle != 0) {
			if (set)
				ret = voice_cvs_start_playback(v);
			else
				ret = voice_cvs_stop_playback(v);
		} else {
			/* Cache the value for later. */
			pr_debug("%s: Caching ICP value", __func__);

			v->music_info.pending = set;
		}

		mutex_unlock(&v->lock);
	}

	return ret;
}

static void voice_auddev_cb_function(u32 evt_id,
			union auddev_evt_data *evt_payload,
			void *private_data)
{
		struct common_data *c = private_data;
		struct voice_data *v = NULL;

	struct sidetone_cal sidetone_cal_data;
	int rc = 0, i = 0;
	int rc1 = 0;

	pr_info("auddev_cb_function, evt_id=%d,\n", evt_id);

	if (evt_payload == NULL) {
		pr_err("%s: evt_payload is NULL pointer\n", __func__);
		return;
	}

	switch (evt_id) {
	case AUDDEV_EVT_START_VOICE:
		v = voice_get_session(evt_payload->voice_session_id);
		if (v == NULL) {
			pr_err("%s: v is NULL\n", __func__);
			return;
		}

		mutex_lock(&v->lock);

		if ((v->voc_state == VOC_INIT) ||
				(v->voc_state == VOC_RELEASE)) {
			v->v_call_status = VOICE_CALL_START;
			if ((v->dev_rx.enabled == VOICE_DEV_ENABLED)
				&& (v->dev_tx.enabled == VOICE_DEV_ENABLED)) {
				rc = voice_apr_register();
				if (rc < 0) {
					pr_err("%s: voice apr registration"
						"failed\n", __func__);
					mutex_unlock(&v->lock);
					return;
				}
				rc1 = voice_create_mvm_cvs_session(v);
				if (rc1 < 0) {
					pr_err("%s: create mvm-cvs failed\n",
								__func__);
					msleep(100);
					rc = voice_apr_register();
					if (rc < 0) {
						mutex_unlock(&v->lock);
						pr_err("%s: voice apr regn"
							"failed\n", __func__);
						return;
					}
					rc1 = voice_create_mvm_cvs_session(v);
					if (rc1 < 0) {
						mutex_unlock(&v->lock);
						pr_err("%s:Retry mvmcvs "
								"failed\n",
								__func__);
						return;
					}
				}
				voice_setup_modem_voice(v);
				voice_attach_vocproc(v);
				voice_send_start_voice_cmd(v);
				get_sidetone_cal(&sidetone_cal_data);
				msm_snddev_enable_sidetone(
					v->dev_rx.dev_id,
					sidetone_cal_data.enable,
					sidetone_cal_data.gain);
				v->voc_state = VOC_RUN;

				/* Start in-call recording if command was
				 * pending. */
				if (v->rec_info.pending) {
					voice_cvs_start_record(v,
						v->rec_info.rec_mode);

					v->rec_info.pending = 0;
				}

				/* Start in-call music delivery if command was
				 * pending. */
				if (v->music_info.pending) {
					voice_cvs_start_playback(v);

					v->music_info.pending = 0;
				}
			}
		}

		mutex_unlock(&v->lock);
		break;
	case AUDDEV_EVT_DEV_CHG_VOICE:
		/* Device change is applicable to all sessions. */
		for (i = 0; i < MAX_VOC_SESSIONS; i++) {
			v = &c->voice[i];

			if (v->dev_rx.enabled == VOICE_DEV_ENABLED)
				msm_snddev_enable_sidetone(v->dev_rx.dev_id,
							   0, 0);

			v->dev_rx.enabled = VOICE_DEV_DISABLED;
			v->dev_tx.enabled = VOICE_DEV_DISABLED;

			mutex_lock(&v->lock);

			if (v->voc_state == VOC_RUN) {
				/* send cmd to modem to do voice device
				 * change */
				voice_disable_vocproc(v);
				v->voc_state = VOC_CHANGE;
			}

			mutex_unlock(&v->lock);
		}
		break;
	case AUDDEV_EVT_DEV_RDY:
		/* Device change is applicable to all sessions. */
		for (i = 0; i < MAX_VOC_SESSIONS; i++) {
			v = &c->voice[i];

			mutex_lock(&v->lock);

			if (v->voc_state == VOC_CHANGE) {
				/* get port Ids */
				if (evt_payload->voc_devinfo.dev_type ==
								      DIR_RX) {
					v->dev_rx.dev_port_id =
					   evt_payload->voc_devinfo.dev_port_id;
					v->dev_rx.sample =
					    evt_payload->voc_devinfo.dev_sample;
					v->dev_rx.dev_id =
						evt_payload->voc_devinfo.dev_id;
					v->dev_rx.enabled = VOICE_DEV_ENABLED;
				} else {
					v->dev_tx.dev_port_id =
					   evt_payload->voc_devinfo.dev_port_id;
					v->dev_tx.sample =
					    evt_payload->voc_devinfo.dev_sample;
					v->dev_tx.enabled = VOICE_DEV_ENABLED;
					v->dev_tx.dev_id =
						evt_payload->voc_devinfo.dev_id;
				}
				if ((v->dev_rx.enabled == VOICE_DEV_ENABLED) &&
				    (v->dev_tx.enabled == VOICE_DEV_ENABLED)) {
					voice_set_device(v);
					get_sidetone_cal(&sidetone_cal_data);
					msm_snddev_enable_sidetone(
						v->dev_rx.dev_id,
						sidetone_cal_data.enable,
						sidetone_cal_data.gain);
					v->voc_state = VOC_RUN;
				}
			} else if ((v->voc_state == VOC_INIT) ||
				   (v->voc_state == VOC_RELEASE)) {
				/* get AFE ports */
				if (evt_payload->voc_devinfo.dev_type ==
								       DIR_RX) {
					/* get rx port id */
					v->dev_rx.dev_port_id =
					   evt_payload->voc_devinfo.dev_port_id;
					v->dev_rx.sample =
					    evt_payload->voc_devinfo.dev_sample;
					v->dev_rx.dev_id =
						evt_payload->voc_devinfo.dev_id;
					v->dev_rx.enabled = VOICE_DEV_ENABLED;
				} else {
					/* get tx port id */
					v->dev_tx.dev_port_id =
					   evt_payload->voc_devinfo.dev_port_id;
					v->dev_tx.sample =
					    evt_payload->voc_devinfo.dev_sample;
					v->dev_tx.dev_id =
						evt_payload->voc_devinfo.dev_id;
					v->dev_tx.enabled = VOICE_DEV_ENABLED;
				}
				if ((v->dev_rx.enabled == VOICE_DEV_ENABLED) &&
				    (v->dev_tx.enabled == VOICE_DEV_ENABLED) &&
				    (v->v_call_status == VOICE_CALL_START)) {
					rc = voice_apr_register();
					if (rc < 0) {
						pr_err("%s: voice apr"
						       "registration failed\n",
						       __func__);
						mutex_unlock(&v->lock);
						return;
					}
					voice_create_mvm_cvs_session(v);
					voice_setup_modem_voice(v);
					voice_attach_vocproc(v);
					voice_send_start_voice_cmd(v);
					get_sidetone_cal(&sidetone_cal_data);
					msm_snddev_enable_sidetone(
						v->dev_rx.dev_id,
						sidetone_cal_data.enable,
						sidetone_cal_data.gain);
					v->voc_state = VOC_RUN;

					/* Start in-call recording if command
					 * was pending. */
					if (v->rec_info.pending) {
						voice_cvs_start_record(v,
							v->rec_info.rec_mode);

						v->rec_info.pending = 0;
					}

					/* Start in-call music delivery if
					 * command was pending. */
					if (v->music_info.pending) {
						voice_cvs_start_playback(v);

						v->music_info.pending = 0;
					}
				}
			}

			mutex_unlock(&v->lock);
		}
	break;
	case AUDDEV_EVT_DEVICE_VOL_MUTE_CHG:
		v = voice_get_session(
				     evt_payload->voc_vm_info.voice_session_id);
		if (v == NULL) {
			pr_err("%s: v is NULL\n", __func__);
			return;
		}

		/* cache the mute and volume index value */
		if (evt_payload->voc_vm_info.dev_type == DIR_TX) {
			v->dev_tx.mute =
				evt_payload->voc_vm_info.dev_vm_val.mute;

			mutex_lock(&v->lock);

			if (v->voc_state == VOC_RUN)
				voice_send_mute_cmd_to_modem(v);

			mutex_unlock(&v->lock);
		} else {
			v->dev_rx.volume =
					evt_payload->voc_vm_info.dev_vm_val.vol;

			mutex_lock(&v->lock);

			if (v->voc_state == VOC_RUN)
				voice_send_vol_index_to_modem(v);

			mutex_unlock(&v->lock);
		}
		break;
	case AUDDEV_EVT_REL_PENDING:
		/* Device change is applicable to all sessions. */
		for (i = 0; i < MAX_VOC_SESSIONS; i++) {
			v = &c->voice[i];

			mutex_lock(&v->lock);

			if (v->voc_state == VOC_RUN) {
				voice_disable_vocproc(v);
				v->voc_state = VOC_CHANGE;
			}

			mutex_unlock(&v->lock);

			if (evt_payload->voc_devinfo.dev_type == DIR_RX)
				v->dev_rx.enabled = VOICE_DEV_DISABLED;
			else
				v->dev_tx.enabled = VOICE_DEV_DISABLED;
		}

		break;
	case AUDDEV_EVT_END_VOICE:
		v = voice_get_session(evt_payload->voice_session_id);
		if (v == NULL) {
			pr_err("%s: v is NULL\n", __func__);
			return;
		}

		/* recover the tx mute and rx volume to the default values */
		v->dev_tx.mute = c->default_mute_val;
		v->dev_rx.volume = c->default_vol_val;
		if (v->dev_rx.enabled == VOICE_DEV_ENABLED)
			msm_snddev_enable_sidetone(v->dev_rx.dev_id, 0, 0);

		mutex_lock(&v->lock);

		if (v->voc_state == VOC_RUN) {
			/* call stop modem voice */
			voice_send_stop_voice_cmd(v);
			voice_destroy_modem_voice(v);
			voice_destroy_mvm_cvs_session(v);
			v->voc_state = VOC_RELEASE;
		} else if (v->voc_state == VOC_CHANGE) {
			voice_send_stop_voice_cmd(v);
			voice_destroy_mvm_cvs_session(v);
			v->voc_state = VOC_RELEASE;
		}

		mutex_unlock(&v->lock);

		v->v_call_status = VOICE_CALL_END;

		break;
	default:
		pr_err("UNKNOWN EVENT\n");
	}
	return;
}
EXPORT_SYMBOL(voice_auddev_cb_function);

int voice_set_voc_path_full(uint32_t set)
{
	pr_info("%s: %d\n", __func__, set);

	mutex_lock(&common.common_lock);

	if (set)
		common.voc_path = VOC_PATH_FULL;
	else
		common.voc_path = VOC_PATH_PASSIVE;

	mutex_unlock(&common.common_lock);

	return 0;
}
EXPORT_SYMBOL(voice_set_voc_path_full);

void voice_register_mvs_cb(ul_cb_fn ul_cb,
			   dl_cb_fn dl_cb,
			   void *private_data)
{
	common.mvs_info.ul_cb = ul_cb;
	common.mvs_info.dl_cb = dl_cb;
	common.mvs_info.private_data = private_data;
}

void voice_config_vocoder(uint32_t media_type,
			  uint32_t rate,
			  uint32_t network_type,
			  uint32_t dtx_mode,
			  struct q_min_max_rate q_min_max_rate)
{
	common.mvs_info.media_type = media_type;
	common.mvs_info.rate = rate;
	common.mvs_info.network_type = network_type;
	common.mvs_info.dtx_mode = dtx_mode;
	common.mvs_info.q_min_max_rate = q_min_max_rate;
}

static int32_t modem_mvm_callback(struct apr_client_data *data, void *priv)
{
	uint32_t *ptr;
	struct common_data *c = priv;
	struct voice_data *v = NULL;
	int i = 0;

	pr_debug("%s: session_id 0x%x\n", __func__, data->dest_port);

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s:Reset event received in Voice service\n",
					__func__);
		apr_reset(c->apr_mvm);
		c->apr_mvm = NULL;
		apr_reset(c->apr_q6_mvm);
		c->apr_q6_mvm = NULL;

		/* Sub-system restart is applicable to all sessions. */
		for (i = 0; i < MAX_VOC_SESSIONS; i++)
			c->voice[i].mvm_handle = 0;

		return 0;
	}

	v = voice_get_session(data->dest_port);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: common data 0x%x, session 0x%x\n",
		 __func__, (unsigned int)c, (unsigned int)v);
	pr_debug("%s: Payload Length = %d, opcode=%x\n", __func__,
				data->payload_size, data->opcode);

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		if (data->payload_size) {
			ptr = data->payload;

			pr_info("%x %x\n", ptr[0], ptr[1]);
			/* ping mvm service ACK */

			if (ptr[0] ==
			 VSS_IMVM_CMD_CREATE_PASSIVE_CONTROL_SESSION ||
			ptr[0] ==
			    VSS_IMVM_CMD_CREATE_FULL_CONTROL_SESSION) {
				/* Passive session is used for voice call
				 * through modem. Full session is used for voice
				 * call through Q6. */
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				if (!ptr[1]) {
					pr_debug("%s: MVM handle is %d\n",
						 __func__, data->src_port);

					voice_set_mvm_handle(v, data->src_port);
				} else
					pr_info("got NACK for sending \
							MVM create session \n");
				v->mvm_state = CMD_STATUS_SUCCESS;
				wake_up(&v->mvm_wait);
			} else if (ptr[0] == VSS_IMVM_CMD_START_VOICE) {
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				v->mvm_state = CMD_STATUS_SUCCESS;
				wake_up(&v->mvm_wait);
			} else if (ptr[0] == VSS_ISTREAM_CMD_ATTACH_VOCPROC) {
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				v->mvm_state = CMD_STATUS_SUCCESS;
				wake_up(&v->mvm_wait);
			} else if (ptr[0] == VSS_IMVM_CMD_STOP_VOICE) {
				v->mvm_state = CMD_STATUS_SUCCESS;
				wake_up(&v->mvm_wait);
			} else if (ptr[0] == VSS_ISTREAM_CMD_DETACH_VOCPROC) {
				v->mvm_state = CMD_STATUS_SUCCESS;
				wake_up(&v->mvm_wait);
			} else if (ptr[0] == VSS_ISTREAM_CMD_SET_TTY_MODE) {
				v->mvm_state = CMD_STATUS_SUCCESS;
				wake_up(&v->mvm_wait);
			} else if (ptr[0] == APRV2_IBASIC_CMD_DESTROY_SESSION) {
				pr_debug("%s: DESTROY resp\n", __func__);

				v->mvm_state = CMD_STATUS_SUCCESS;
				wake_up(&v->mvm_wait);
			} else if (ptr[0] == VSS_IMVM_CMD_ATTACH_STREAM) {
				pr_debug("%s: ATTACH_STREAM resp 0x%x\n",
					__func__, ptr[1]);

				v->mvm_state = CMD_STATUS_SUCCESS;
				wake_up(&v->mvm_wait);
			} else if (ptr[0] == VSS_IMVM_CMD_DETACH_STREAM) {
				pr_debug("%s: DETACH_STREAM resp 0x%x\n",
					__func__, ptr[1]);

				v->mvm_state = CMD_STATUS_SUCCESS;
				wake_up(&v->mvm_wait);
			} else if (ptr[0] == VSS_ICOMMON_CMD_SET_NETWORK) {
				pr_debug("%s: SET_NETWORK resp 0x%x\n",
					__func__, ptr[1]);

				v->mvm_state = CMD_STATUS_SUCCESS;
				wake_up(&v->mvm_wait);
			} else if (ptr[0] == VSS_ICOMMON_CMD_SET_VOICE_TIMING) {
				pr_debug("%s: SET_VOICE_TIMING resp 0x%x\n",
					__func__, ptr[1]);

				v->mvm_state = CMD_STATUS_SUCCESS;
				wake_up(&v->mvm_wait);
			} else
				pr_debug("%s: not match cmd = 0x%x\n",
					__func__, ptr[0]);
		}
	}

	return 0;
}

static int32_t modem_cvs_callback(struct apr_client_data *data, void *priv)
{
	uint32_t *ptr;
	struct common_data *c = priv;
	struct voice_data *v = NULL;
	int i = 0;

	pr_debug("%s: session_id 0x%x\n", __func__, data->dest_port);

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s:Reset event received in Voice service\n",
					__func__);
		apr_reset(c->apr_cvs);
		c->apr_cvs = NULL;
		apr_reset(c->apr_q6_cvs);
		c->apr_q6_cvs = NULL;

		/* Sub-system restart is applicable to all sessions. */
		for (i = 0; i < MAX_VOC_SESSIONS; i++)
			c->voice[i].cvs_handle = 0;

		return 0;
	}

	v = voice_get_session(data->dest_port);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: common data 0x%x, session 0x%x\n",
		 __func__, (unsigned int)c, (unsigned int)v);
	pr_debug("%s: Payload Length = %d, opcode=%x\n", __func__,
					data->payload_size, data->opcode);

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		if (data->payload_size) {
			ptr = data->payload;

			pr_info("%x %x\n", ptr[0], ptr[1]);
			/*response from modem CVS */
			if (ptr[0] ==
			VSS_ISTREAM_CMD_CREATE_PASSIVE_CONTROL_SESSION ||
			    ptr[0] ==
			    VSS_ISTREAM_CMD_CREATE_FULL_CONTROL_SESSION) {
				if (!ptr[1]) {
					pr_debug("%s: CVS handle is %d\n",
						 __func__, data->src_port);
					voice_set_cvs_handle(v, data->src_port);
				} else
					pr_info("got NACK for sending \
							CVS create session \n");
				v->cvs_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvs_wait);
			} else if (ptr[0] ==
				VSS_ISTREAM_CMD_CACHE_CALIBRATION_DATA) {
				v->cvs_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvs_wait);
			} else if (ptr[0] ==
					VSS_ISTREAM_CMD_SET_MUTE) {
				v->cvs_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvs_wait);
			} else if (ptr[0] == VSS_ISTREAM_CMD_SET_MEDIA_TYPE) {
				pr_debug("%s: SET_MEDIA resp 0x%x\n",
					 __func__, ptr[1]);

				v->cvs_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvs_wait);
			} else if (ptr[0] ==
				   VSS_ISTREAM_CMD_VOC_AMR_SET_ENC_RATE) {
				pr_debug("%s: SET_AMR_RATE resp 0x%x\n",
					 __func__, ptr[1]);

				v->cvs_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvs_wait);
			} else if (ptr[0] ==
				   VSS_ISTREAM_CMD_VOC_AMRWB_SET_ENC_RATE) {
				pr_debug("%s: SET_AMR_WB_RATE resp 0x%x\n",
					 __func__, ptr[1]);

				v->cvs_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvs_wait);
			} else if (ptr[0] == VSS_ISTREAM_CMD_SET_ENC_DTX_MODE) {
				pr_debug("%s: SET_DTX resp 0x%x\n",
					 __func__, ptr[1]);

				v->cvs_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvs_wait);
			} else if (ptr[0] ==
				   VSS_ISTREAM_CMD_CDMA_SET_ENC_MINMAX_RATE) {
				pr_debug("%s: SET_CDMA_RATE resp 0x%x\n",
					 __func__, ptr[1]);

				v->cvs_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvs_wait);
			} else if (ptr[0] == APRV2_IBASIC_CMD_DESTROY_SESSION) {
				pr_debug("%s: DESTROY resp\n", __func__);

				v->cvs_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvs_wait);
			} else if (ptr[0] == VSS_ISTREAM_CMD_START_RECORD) {
				pr_debug("%s: START_RECORD resp 0x%x\n",
					 __func__, ptr[1]);

					v->cvs_state = CMD_STATUS_SUCCESS;
					wake_up(&v->cvs_wait);
			} else if (ptr[0] == VSS_ISTREAM_CMD_STOP_RECORD) {
				pr_debug("%s: STOP_RECORD resp 0x%x\n",
					 __func__, ptr[1]);

					v->cvs_state = CMD_STATUS_SUCCESS;
					wake_up(&v->cvs_wait);
			} else if (ptr[0] == VOICE_CMD_SET_PARAM) {
				rtac_make_voice_callback(RTAC_CVS, ptr,
					data->payload_size);
			} else if (ptr[0] == VSS_ISTREAM_CMD_START_PLAYBACK) {
				pr_debug("%s: START_PLAYBACK resp 0x%x\n",
					 __func__, ptr[1]);

					v->cvs_state = CMD_STATUS_SUCCESS;
					wake_up(&v->cvs_wait);
			} else if (ptr[0] == VSS_ISTREAM_CMD_STOP_PLAYBACK) {
				pr_debug("%s: STOP_PLAYBACK resp 0x%x\n",
					 __func__, ptr[1]);

					v->cvs_state = CMD_STATUS_SUCCESS;
					wake_up(&v->cvs_wait);
			} else
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
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
			} else {
				pr_err("%s: voc_pkt is 0x%x ul_cb is 0x%x\n",
				       __func__, (unsigned int)voc_pkt,
				       (unsigned int)c->mvs_info.ul_cb);
			}
	} else if (data->opcode == VSS_ISTREAM_EVT_SEND_DEC_BUFFER) {
			pr_debug("%s: Send dec buf resp\n", __func__);
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

			ret = apr_send_pkt(voice_get_apr_cvs(),
					   (uint32_t *) &send_dec_buf);
			if (ret < 0) {
				pr_err("%s: Error %d sending DEC_BUF\n",
				       __func__, ret);
				goto fail;
			}
		} else {
			pr_err("%s: ul_cb is NULL\n", __func__);
		}
	} else if (data->opcode ==  VOICE_EVT_GET_PARAM_ACK) {
		rtac_make_voice_callback(RTAC_CVS, data->payload,
					data->payload_size);
	} else {
		pr_debug("%s: Unknown opcode 0x%x\n", __func__, data->opcode);
	}

fail:
	return 0;
}

static int32_t modem_cvp_callback(struct apr_client_data *data, void *priv)
{
	uint32_t *ptr;
	struct common_data *c = priv;
	struct voice_data *v = NULL;
	int i = 0;

	pr_debug("%s: session_id 0x%x\n", __func__, data->dest_port);

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s:Reset event received in Voice service\n",
					__func__);
		apr_reset(c->apr_cvp);
		c->apr_cvp = NULL;
		apr_reset(c->apr_q6_cvp);
		c->apr_q6_cvp = NULL;

		/* Sub-system restart is applicable to all sessions. */
		for (i = 0; i < MAX_VOC_SESSIONS; i++)
			c->voice[i].cvp_handle = 0;

		return 0;
	}

	v = voice_get_session(data->dest_port);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: common data 0x%x, session 0x%x\n",
		 __func__, (unsigned int)c, (unsigned int)v);
	pr_debug("%s: Payload Length = %d, opcode=%x\n", __func__,
				data->payload_size, data->opcode);

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		if (data->payload_size) {
			ptr = data->payload;

			pr_info("%x %x\n", ptr[0], ptr[1]);
			/*response from modem CVP */
			if (ptr[0] ==
				VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION) {
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				if (!ptr[1]) {
					voice_set_cvp_handle(v, data->src_port);
					pr_debug("cvphdl=%d\n", data->src_port);
				} else
					pr_info("got NACK from CVP create \
						session response\n");
				v->cvp_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvp_wait);
			} else if (ptr[0] ==
				VSS_IVOCPROC_CMD_CACHE_CALIBRATION_DATA) {
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				v->cvp_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvp_wait);
			} else if (ptr[0] == VSS_IVOCPROC_CMD_SET_DEVICE) {
				v->cvp_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvp_wait);
			} else if (ptr[0] ==
					VSS_IVOCPROC_CMD_SET_RX_VOLUME_INDEX) {
				v->cvp_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvp_wait);
			} else if (ptr[0] == VSS_IVOCPROC_CMD_ENABLE) {
				v->cvp_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvp_wait);
			} else if (ptr[0] == VSS_IVOCPROC_CMD_DISABLE) {
				v->cvp_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvp_wait);
			} else if (ptr[0] == APRV2_IBASIC_CMD_DESTROY_SESSION) {
				v->cvp_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvp_wait);
			} else if (ptr[0] ==
				VSS_IVOCPROC_CMD_CACHE_VOLUME_CALIBRATION_TABLE
				) {

				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				v->cvp_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvp_wait);
			} else if (ptr[0] == VOICE_CMD_SET_PARAM) {
				rtac_make_voice_callback(RTAC_CVP, ptr,
					data->payload_size);
			} else
				pr_debug("%s: not match cmd = 0x%x\n",
							__func__, ptr[0]);
		}
	} else if (data->opcode ==  VOICE_EVT_GET_PARAM_ACK) {
		rtac_make_voice_callback(RTAC_CVP, data->payload,
			data->payload_size);
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

	common.voc_path = VOC_PATH_PASSIVE;

	/* Initialize MVS info. */
	common.mvs_info.network_type = VSS_NETWORK_ID_DEFAULT;

	mutex_init(&common.common_lock);

	for (i = 0; i < MAX_VOC_SESSIONS; i++) {
		common.voice[i].session_id = SESSION_ID_BASE + i;

		common.voice[i].dev_rx.volume = common.default_vol_val;
		common.voice[i].dev_tx.mute = common.default_mute_val;

		common.voice[i].voc_state = VOC_INIT;

		common.voice[i].rec_info.rec_mode = VOC_REC_NONE;

		init_waitqueue_head(&common.voice[i].mvm_wait);
		init_waitqueue_head(&common.voice[i].cvs_wait);
		init_waitqueue_head(&common.voice[i].cvp_wait);

		mutex_init(&common.voice[i].lock);

	}

	common.device_events = AUDDEV_EVT_DEV_CHG_VOICE |
			AUDDEV_EVT_DEV_RDY |
			AUDDEV_EVT_REL_PENDING |
			AUDDEV_EVT_START_VOICE |
			AUDDEV_EVT_END_VOICE |
			AUDDEV_EVT_DEVICE_VOL_MUTE_CHG |
			AUDDEV_EVT_FREQ_CHG;

	pr_debug("to register call back\n");
	/* register callback to auddev */
	auddev_register_evt_listner(common.device_events, AUDDEV_CLNT_VOC,
				0, voice_auddev_cb_function, &common);

	return rc;
}

device_initcall(voice_init);
