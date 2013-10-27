/*  Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/msm_audio_ion.h>

#include <asm/mach-types.h>
#include <mach/socinfo.h>
#include <mach/qdsp6v2/apr_tal.h>

#include "sound/apr_audio-v2.h"
#include "sound/q6afe-v2.h"

#include "audio_acdb.h"
#include "q6voice.h"


#define TIMEOUT_MS 200


#define CMD_STATUS_SUCCESS 0
#define CMD_STATUS_FAIL 1

/* CVP CAL Size: 245760 = 240 * 1024 */
#define CVP_CAL_SIZE 245760
/* CVS CAL Size: 49152 = 48 * 1024 */
#define CVS_CAL_SIZE 49152

enum {
	VOC_TOKEN_NONE,
	VOIP_MEM_MAP_TOKEN,
	VOC_CAL_MEM_MAP_TOKEN,
	VOC_RTAC_MEM_MAP_TOKEN
};

static struct common_data common;
static bool module_initialized;

static int voice_send_enable_vocproc_cmd(struct voice_data *v);
static int voice_send_netid_timing_cmd(struct voice_data *v);
static int voice_send_attach_vocproc_cmd(struct voice_data *v);
static int voice_send_set_device_cmd(struct voice_data *v);
static int voice_send_disable_vocproc_cmd(struct voice_data *v);
static int voice_send_vol_step_cmd(struct voice_data *v);
static int voice_send_mvm_unmap_memory_physical_cmd(struct voice_data *v,
						    uint32_t mem_handle);
static int voice_send_mvm_cal_network_cmd(struct voice_data *v);
static int voice_send_mvm_media_type_cmd(struct voice_data *v);
static int voice_send_cvs_data_exchange_mode_cmd(struct voice_data *v);
static int voice_send_cvs_packet_exchange_config_cmd(struct voice_data *v);
static int voice_set_packet_exchange_mode_and_config(uint32_t session_id,
						     uint32_t mode);

static int voice_send_cvs_register_cal_cmd(struct voice_data *v);
static int voice_send_cvs_deregister_cal_cmd(struct voice_data *v);
static int voice_send_cvp_register_dev_cfg_cmd(struct voice_data *v);
static int voice_send_cvp_deregister_dev_cfg_cmd(struct voice_data *v);
static int voice_send_cvp_register_cal_cmd(struct voice_data *v);
static int voice_send_cvp_deregister_cal_cmd(struct voice_data *v);
static int voice_send_cvp_register_vol_cal_cmd(struct voice_data *v);
static int voice_send_cvp_deregister_vol_cal_cmd(struct voice_data *v);

static int voice_cvs_stop_playback(struct voice_data *v);
static int voice_cvs_start_playback(struct voice_data *v);
static int voice_cvs_start_record(struct voice_data *v, uint32_t rec_mode);
static int voice_cvs_stop_record(struct voice_data *v);

static int32_t qdsp_mvm_callback(struct apr_client_data *data, void *priv);
static int32_t qdsp_cvs_callback(struct apr_client_data *data, void *priv);
static int32_t qdsp_cvp_callback(struct apr_client_data *data, void *priv);

static int voice_send_set_pp_enable_cmd(struct voice_data *v,
					uint32_t module_id, int enable);
static int is_cal_memory_allocated(void);
static int is_voip_memory_allocated(void);
static int voice_alloc_cal_mem_map_table(void);
static int voice_alloc_rtac_mem_map_table(void);
static int voice_alloc_oob_shared_mem(void);
static int voice_free_oob_shared_mem(void);
static int voice_alloc_oob_mem_table(void);
static int voice_alloc_and_map_cal_mem(struct voice_data *v);
static int voice_alloc_and_map_oob_mem(struct voice_data *v);

static struct voice_data *voice_get_session_by_idx(int idx);

static void voice_itr_init(struct voice_session_itr *itr,
			   u32 session_id)
{
	if (itr == NULL)
		return;
	itr->session_idx = voice_get_idx_for_session(session_id);
	if (session_id == ALL_SESSION_VSID)
		itr->cur_idx = 0;
	else
		itr->cur_idx = itr->session_idx;

}

static bool voice_itr_get_next_session(struct voice_session_itr *itr,
					struct voice_data **voice)
{
	bool ret = false;

	if (itr == NULL)
		return false;
	pr_debug("%s : cur idx = %d session idx = %d",
			 __func__, itr->cur_idx, itr->session_idx);

	if (itr->cur_idx <= itr->session_idx) {
		ret = true;
		*voice = voice_get_session_by_idx(itr->cur_idx);
		itr->cur_idx++;
	} else {
		*voice = NULL;
	}

	return ret;
}

static bool voice_is_valid_session_id(uint32_t session_id)
{
	bool ret = false;

	switch (session_id) {
	case VOICE_SESSION_VSID:
	case VOICE2_SESSION_VSID:
	case VOLTE_SESSION_VSID:
	case VOIP_SESSION_VSID:
	case QCHAT_SESSION_VSID:
	case ALL_SESSION_VSID:
		ret = true;
		break;
	default:
		pr_err("%s: Invalid session_id : %x\n", __func__, session_id);

		break;
	}

	return ret;
}

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

char *voc_get_session_name(u32 session_id)
{
	char *session_name = NULL;

	if (session_id == common.voice[VOC_PATH_PASSIVE].session_id) {
		session_name = VOICE_SESSION_NAME;
	} else if (session_id ==
			common.voice[VOC_PATH_VOLTE_PASSIVE].session_id) {
		session_name = VOLTE_SESSION_NAME;
	} else if (session_id ==
			common.voice[VOC_PATH_QCHAT_PASSIVE].session_id) {
		session_name = QCHAT_SESSION_NAME;
	} else if (session_id == common.voice[VOC_PATH_FULL].session_id) {
		session_name = VOIP_SESSION_NAME;
	}
	return session_name;
}

uint32_t voc_get_session_id(char *name)
{
	u32 session_id = 0;

	if (name != NULL) {
		if (!strncmp(name, "Voice session", 13))
			session_id = common.voice[VOC_PATH_PASSIVE].session_id;
		else if (!strncmp(name, "Voice2 session", 14))
			session_id =
			common.voice[VOC_PATH_VOICE2_PASSIVE].session_id;
		else if (!strncmp(name, "VoLTE session", 13))
			session_id =
			common.voice[VOC_PATH_VOLTE_PASSIVE].session_id;
		else if (!strncmp(name, "QCHAT session", 13))
			session_id =
			common.voice[VOC_PATH_QCHAT_PASSIVE].session_id;
		else
			session_id = common.voice[VOC_PATH_FULL].session_id;

		pr_debug("%s: %s has session id 0x%x\n", __func__, name,
				session_id);
	}

	return session_id;
}

static struct voice_data *voice_get_session(u32 session_id)
{
	struct voice_data *v = NULL;

	switch (session_id) {
	case VOICE_SESSION_VSID:
		v = &common.voice[VOC_PATH_PASSIVE];
		break;

	case VOICE2_SESSION_VSID:
		v = &common.voice[VOC_PATH_VOICE2_PASSIVE];
		break;

	case VOLTE_SESSION_VSID:
		v = &common.voice[VOC_PATH_VOLTE_PASSIVE];
		break;

	case VOIP_SESSION_VSID:
		v = &common.voice[VOC_PATH_FULL];
		break;

	case QCHAT_SESSION_VSID:
		v = &common.voice[VOC_PATH_QCHAT_PASSIVE];
		break;

	case ALL_SESSION_VSID:
		break;

	default:
		pr_err("%s: Invalid session_id : %x\n", __func__, session_id);

		break;
	}

	pr_debug("%s:session_id 0x%x session handle 0x%x\n",
		__func__, session_id, (unsigned int)v);

	return v;
}

int voice_get_idx_for_session(u32 session_id)
{
	int idx = 0;

	switch (session_id) {
	case VOICE_SESSION_VSID:
		idx = VOC_PATH_PASSIVE;
		break;

	case VOICE2_SESSION_VSID:
		idx = VOC_PATH_VOICE2_PASSIVE;
		break;

	case VOLTE_SESSION_VSID:
		idx = VOC_PATH_VOLTE_PASSIVE;
		break;

	case VOIP_SESSION_VSID:
		idx = VOC_PATH_FULL;
		break;

	case QCHAT_SESSION_VSID:
		idx = VOC_PATH_QCHAT_PASSIVE;
		break;

	case ALL_SESSION_VSID:
		idx = MAX_VOC_SESSIONS - 1;
		break;

	default:
		pr_err("%s: Invalid session_id : %x\n", __func__, session_id);

		break;
	}

	return idx;
}

static struct voice_data *voice_get_session_by_idx(int idx)
{
	return ((idx < 0 || idx >= MAX_VOC_SESSIONS) ?
				NULL : &common.voice[idx]);
}

static bool is_voip_session(u32 session_id)
{
	return (session_id == common.voice[VOC_PATH_FULL].session_id);
}

static bool is_volte_session(u32 session_id)
{
	return (session_id == common.voice[VOC_PATH_VOLTE_PASSIVE].session_id);
}

static bool is_voice2_session(u32 session_id)
{
	return (session_id == common.voice[VOC_PATH_VOICE2_PASSIVE].session_id);
}

static bool is_qchat_session(u32 session_id)
{
	return (session_id == common.voice[VOC_PATH_QCHAT_PASSIVE].session_id);
}

static bool is_voc_state_active(int voc_state)
{
	if ((voc_state == VOC_RUN) ||
		(voc_state == VOC_CHANGE) ||
		(voc_state == VOC_STANDBY))
		return true;

	return false;
}

static void voc_set_error_state(uint16_t reset_proc)
{
	struct voice_data *v = NULL;
	int i;

	for (i = 0; i < MAX_VOC_SESSIONS; i++) {
		if (reset_proc == APR_DEST_MODEM  && i == VOC_PATH_FULL)
			continue;

		v = &common.voice[i];
		if (v != NULL)
			v->voc_state = VOC_ERROR;
	}
}

static bool is_other_session_active(u32 session_id)
{
	int i;
	bool ret = false;

	/* Check if there is other active session except the input one */
	for (i = 0; i < MAX_VOC_SESSIONS; i++) {
		if (common.voice[i].session_id == session_id)
			continue;

		if (is_voc_state_active(common.voice[i].voc_state)) {
			ret = true;
			break;
		}
	}
	pr_debug("%s: ret %d\n", __func__, ret);

	return ret;
}

static bool is_voice_app_id(u32 session_id)
{
	return (((session_id & APP_ID_MASK) >> APP_ID_SHIFT) ==
						VSID_APP_CS_VOICE);
}

static void init_session_id(void)
{
	common.voice[VOC_PATH_PASSIVE].session_id = VOICE_SESSION_VSID;
	common.voice[VOC_PATH_VOLTE_PASSIVE].session_id = VOLTE_SESSION_VSID;
	common.voice[VOC_PATH_VOICE2_PASSIVE].session_id = VOICE2_SESSION_VSID;
	common.voice[VOC_PATH_FULL].session_id = VOIP_SESSION_VSID;
	common.voice[VOC_PATH_QCHAT_PASSIVE].session_id = QCHAT_SESSION_VSID;
}

static int voice_apr_register(void)
{
	void *modem_mvm, *modem_cvs, *modem_cvp;

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

		/*
		 * Register with modem for SSR callback. The APR handle
		 * is not stored since it is used only to receive notifications
		 * and not for communication
		 */
		modem_mvm = apr_register("MODEM", "MVM",
						qdsp_mvm_callback,
						0xFFFFFFFF, &common);
		if (modem_mvm == NULL)
			pr_err("%s: Unable to register MVM for MODEM\n",
					__func__);
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
		rtac_set_voice_handle(RTAC_CVS, common.apr_q6_cvs);
		/*
		 * Register with modem for SSR callback. The APR handle
		 * is not stored since it is used only to receive notifications
		 * and not for communication
		 */
		modem_cvs = apr_register("MODEM", "CVS",
						qdsp_cvs_callback,
						0xFFFFFFFF, &common);
		 if (modem_cvs == NULL)
			pr_err("%s: Unable to register CVS for MODEM\n",
					__func__);

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
		rtac_set_voice_handle(RTAC_CVP, common.apr_q6_cvp);
		/*
		 * Register with modem for SSR callback. The APR handle
		 * is not stored since it is used only to receive notifications
		 * and not for communication
		 */
		modem_cvp = apr_register("MODEM", "CVP",
						qdsp_cvp_callback,
						0xFFFFFFFF, &common);
		if (modem_cvp == NULL)
			pr_err("%s: Unable to register CVP for MODEM\n",
					__func__);

	}

	mutex_unlock(&common.common_lock);

	return 0;

err:
	if (common.apr_q6_cvs != NULL) {
		apr_deregister(common.apr_q6_cvs);
		common.apr_q6_cvs = NULL;
		rtac_set_voice_handle(RTAC_CVS, NULL);
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
	pr_debug("%s: Send Dual Control command to MVM\n", __func__);
	if (!is_voip_session(v->session_id)) {
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
		mvm_voice_ctl_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
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
		if (!is_voip_session(v->session_id)) {
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
			mvm_session_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
			mvm_session_cmd.hdr.dest_port = 0;
			mvm_session_cmd.hdr.token = 0;
			mvm_session_cmd.hdr.opcode =
				VSS_IMVM_CMD_CREATE_PASSIVE_CONTROL_SESSION;
			if (is_volte_session(v->session_id)) {
				strlcpy(mvm_session_cmd.mvm_session.name,
				"default volte voice",
				sizeof(mvm_session_cmd.mvm_session.name));
			} else if (is_voice2_session(v->session_id)) {
				strlcpy(mvm_session_cmd.mvm_session.name,
				VOICE2_SESSION_VSID_STR,
				sizeof(mvm_session_cmd.mvm_session.name));
			} else if (is_qchat_session(v->session_id)) {
				strlcpy(mvm_session_cmd.mvm_session.name,
				QCHAT_SESSION_VSID_STR,
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
			mvm_session_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
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
		if (!is_voip_session(v->session_id)) {
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
			cvs_session_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
			cvs_session_cmd.hdr.dest_port = 0;
			cvs_session_cmd.hdr.token = 0;
			cvs_session_cmd.hdr.opcode =
				VSS_ISTREAM_CMD_CREATE_PASSIVE_CONTROL_SESSION;
			if (is_volte_session(v->session_id)) {
				strlcpy(cvs_session_cmd.cvs_session.name,
				"default volte voice",
				sizeof(cvs_session_cmd.cvs_session.name));
			} else if (is_voice2_session(v->session_id)) {
				strlcpy(cvs_session_cmd.cvs_session.name,
				VOICE2_SESSION_VSID_STR,
				sizeof(cvs_session_cmd.cvs_session.name));
			} else if (is_qchat_session(v->session_id)) {
				strlcpy(cvs_session_cmd.cvs_session.name,
				QCHAT_SESSION_VSID_STR,
				sizeof(cvs_session_cmd.cvs_session.name));
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

			cvs_full_ctl_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
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
			attach_stream_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
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
		pr_debug("%s: MVM detach stream, VOC_STATE: %d\n", __func__,
				v->voc_state);

		/* Detach voice stream. */
		detach_stream.hdr.hdr_field =
					APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);
		detach_stream.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(detach_stream) - APR_HDR_SIZE);
		detach_stream.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
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

		/* Unmap memory */
		if (v->shmem_info.mem_handle != 0) {
			ret = voice_send_mvm_unmap_memory_physical_cmd(v,
						v->shmem_info.mem_handle);
			if (ret < 0) {
				pr_err("%s Memory_unmap for voip failed %d\n",
				       __func__, ret);

				goto fail;
			}
			v->shmem_info.mem_handle = 0;
		}
	}

	if (is_voip_session(v->session_id) ||
	    is_qchat_session(v->session_id) ||
	    v->voc_state == VOC_ERROR) {
		/* Destroy CVS. */
		pr_debug("%s: CVS destroy session\n", __func__);

		cvs_destroy.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						      APR_HDR_LEN(APR_HDR_SIZE),
						      APR_PKT_VER);
		cvs_destroy.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(cvs_destroy) - APR_HDR_SIZE);
		cvs_destroy.src_port =
				voice_get_idx_for_session(v->session_id);
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

		/* Unmap physical memory for calibration */
		pr_debug("%s: cal_mem_handle %d\n", __func__,
			 common.cal_mem_handle);

		if (!is_other_session_active(v->session_id) &&
					    (common.cal_mem_handle != 0)) {
			ret = voice_send_mvm_unmap_memory_physical_cmd(v,
							common.cal_mem_handle);
			if (ret < 0) {
				pr_err("%s Fail at cal mem unmap %d\n",
				       __func__, ret);

				goto fail;
			}
			common.cal_mem_handle = 0;
		}

		/* Destroy MVM. */
		pr_debug("MVM destroy session\n");

		mvm_destroy.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						      APR_HDR_LEN(APR_HDR_SIZE),
						      APR_PKT_VER);
		mvm_destroy.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					    sizeof(mvm_destroy) - APR_HDR_SIZE);
		mvm_destroy.src_port =
				voice_get_idx_for_session(v->session_id);
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
		mvm_tty_mode_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
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

static int voice_send_set_pp_enable_cmd(struct voice_data *v,
					uint32_t module_id, int enable)
{
	struct cvs_set_pp_enable_cmd cvs_set_pp_cmd;
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

	cvs_set_pp_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						     APR_HDR_LEN(APR_HDR_SIZE),
						     APR_PKT_VER);
	cvs_set_pp_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
						   sizeof(cvs_set_pp_cmd) -
						   APR_HDR_SIZE);
	cvs_set_pp_cmd.hdr.src_port = voice_get_idx_for_session(v->session_id);
	cvs_set_pp_cmd.hdr.dest_port = cvs_handle;
	cvs_set_pp_cmd.hdr.token = 0;
	cvs_set_pp_cmd.hdr.opcode = VSS_ICOMMON_CMD_SET_UI_PROPERTY;

	cvs_set_pp_cmd.vss_set_pp.module_id = module_id;
	cvs_set_pp_cmd.vss_set_pp.param_id = VOICE_PARAM_MOD_ENABLE;
	cvs_set_pp_cmd.vss_set_pp.param_size = MOD_ENABLE_PARAM_LEN;
	cvs_set_pp_cmd.vss_set_pp.reserved = 0;
	cvs_set_pp_cmd.vss_set_pp.enable = enable;
	cvs_set_pp_cmd.vss_set_pp.reserved_field = 0;
	pr_debug("voice_send_set_pp_enable_cmd, module_id=%d, enable=%d\n",
		module_id, enable);

	v->cvs_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_set_pp_cmd);
	if (ret < 0) {
		pr_err("Fail: sending cvs set pp enable,\n");
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
	cvs_set_dtx.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
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

static int voice_send_mvm_media_type_cmd(struct voice_data *v)
{
	struct vss_imvm_cmd_set_cal_media_type_t mvm_set_cal_media_type;
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

	mvm_set_cal_media_type.hdr.hdr_field =
					APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);
	mvm_set_cal_media_type.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(mvm_set_cal_media_type) -
					APR_HDR_SIZE);
	mvm_set_cal_media_type.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	mvm_set_cal_media_type.hdr.dest_port = mvm_handle;
	mvm_set_cal_media_type.hdr.token = 0;
	mvm_set_cal_media_type.hdr.opcode = VSS_IMVM_CMD_SET_CAL_MEDIA_TYPE;
	mvm_set_cal_media_type.media_id = common.mvs_info.media_type;
	pr_debug("%s: setting media_id as %x\n",
		 __func__ , mvm_set_cal_media_type.media_id);

	v->mvm_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_mvm, (uint32_t *) &mvm_set_cal_media_type);
	if (ret < 0) {
		pr_err("%s: Error %d sending media type\n", __func__, ret);
		goto fail;
	}

	ret = wait_event_timeout(v->mvm_wait,
				(v->mvm_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout %d\n", __func__, ret);
		goto fail;
	}
	return 0;
fail:
	return -EINVAL;
}

static int voice_send_dtmf_rx_detection_cmd(struct voice_data *v,
					    uint32_t enable)
{
	int ret = 0;
	void *apr_cvs;
	u16 cvs_handle;
	struct cvs_set_rx_dtmf_detection_cmd cvs_dtmf_rx_detection;

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

	/* Set SET_DTMF_RX_DETECTION */
	cvs_dtmf_rx_detection.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					      APR_HDR_LEN(APR_HDR_SIZE),
					      APR_PKT_VER);
	cvs_dtmf_rx_detection.hdr.pkt_size =
				APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvs_dtmf_rx_detection) - APR_HDR_SIZE);
	cvs_dtmf_rx_detection.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	cvs_dtmf_rx_detection.hdr.dest_port = cvs_handle;
	cvs_dtmf_rx_detection.hdr.token = 0;
	cvs_dtmf_rx_detection.hdr.opcode =
					VSS_ISTREAM_CMD_SET_RX_DTMF_DETECTION;
	cvs_dtmf_rx_detection.cvs_dtmf_det.enable = enable;

	v->cvs_state = CMD_STATUS_FAIL;

	ret = apr_send_pkt(apr_cvs, (uint32_t *) &cvs_dtmf_rx_detection);
	if (ret < 0) {
		pr_err("%s: Error %d sending SET_DTMF_RX_DETECTION\n",
		       __func__,
		       ret);
		return -EINVAL;
	}

	ret = wait_event_timeout(v->cvs_wait,
				 (v->cvs_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));

	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		return -EINVAL;
	}

	return ret;
}

void voc_disable_dtmf_det_on_active_sessions(void)
{
	struct voice_data *v = NULL;
	int i;
	for (i = 0; i < MAX_VOC_SESSIONS; i++) {
		v = &common.voice[i];
		if ((v->dtmf_rx_detect_en) &&
			is_voc_state_active(v->voc_state)) {

			pr_debug("disable dtmf det on ses_id=%d\n",
				 v->session_id);
			voice_send_dtmf_rx_detection_cmd(v, 0);
		}
	}
}

int voc_enable_dtmf_rx_detection(uint32_t session_id, uint32_t enable)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);
		return -EINVAL;
	}

	mutex_lock(&v->lock);
	v->dtmf_rx_detect_en = enable;

	if (is_voc_state_active(v->voc_state))
		ret = voice_send_dtmf_rx_detection_cmd(v,
						       v->dtmf_rx_detect_en);

	mutex_unlock(&v->lock);

	return ret;
}

int voc_alloc_cal_shared_memory(void)
{
	int rc = 0;

	mutex_lock(&common.common_lock);
	if (is_cal_memory_allocated()) {
		pr_debug("%s: Calibration shared buffer already allocated",
			 __func__);
	} else {
		/* Allocate memory for calibration memory map table. */
		rc = voice_alloc_cal_mem_map_table();
		if (rc < 0) {
			pr_err("%s: Failed to allocate cal memory, err=%d",
			       __func__, rc);
		}
	}
	mutex_unlock(&common.common_lock);

	return rc;
}

int voc_alloc_voip_shared_memory(void)
{
	int rc = 0;

	/* Allocate shared memory for OOB Voip */
	rc = voice_alloc_oob_shared_mem();
	if (rc < 0) {
		pr_err("%s: Failed to alloc shared memory for OOB rc:%d\n",
			   __func__, rc);
	} else {
		/* Allocate mem map table for OOB */
		rc = voice_alloc_oob_mem_table();
		if (rc < 0) {
			pr_err("%s: Failed to alloc mem map talbe rc:%d\n",
			       __func__, rc);

			voice_free_oob_shared_mem();
		}
	}

	return rc;
}

static int is_cal_memory_allocated(void)
{
	bool ret;

	if (common.cal_mem_map_table.client != NULL &&
	    common.cal_mem_map_table.handle != NULL)
		ret = true;
	else
		ret = false;

	return ret;
}


static int free_cal_map_table(void)
{
	int ret = 0;

	if ((common.cal_mem_map_table.client == NULL) ||
		(common.cal_mem_map_table.handle == NULL))
		goto done;

	ret = msm_audio_ion_free(common.cal_mem_map_table.client,
		common.cal_mem_map_table.handle);
	if (ret < 0) {
		pr_err("%s: msm_audio_ion_free failed:\n", __func__);
		ret = -EPERM;
	}

done:
	common.cal_mem_map_table.client = NULL;
	common.cal_mem_map_table.handle = NULL;
	return ret;
}

static int is_rtac_memory_allocated(void)
{
	bool ret;

	if (common.rtac_mem_map_table.client != NULL &&
	    common.rtac_mem_map_table.handle != NULL)
		ret = true;
	else
		ret = false;

	return ret;
}

static int free_rtac_map_table(void)
{
	int ret = 0;

	if ((common.rtac_mem_map_table.client == NULL) ||
		(common.rtac_mem_map_table.handle == NULL))
		goto done;

	ret = msm_audio_ion_free(common.rtac_mem_map_table.client,
		common.rtac_mem_map_table.handle);
	if (ret < 0) {
		pr_err("%s: msm_audio_ion_free failed:\n", __func__);
		ret = -EPERM;
	}

done:
	common.rtac_mem_map_table.client = NULL;
	common.rtac_mem_map_table.handle = NULL;
	return ret;
}


static int is_voip_memory_allocated(void)
{
	bool ret;
	struct voice_data *v = voice_get_session(
				common.voice[VOC_PATH_FULL].session_id);

	if (v == NULL) {
		pr_err("%s: v is NULL, session_id:%d\n", __func__,
		common.voice[VOC_PATH_FULL].session_id);

		ret = false;
		goto done;
	}

	mutex_lock(&common.common_lock);
	if (v->shmem_info.sh_buf.client != NULL &&
	    v->shmem_info.sh_buf.handle != NULL)
		ret = true;
	else
		ret = false;
	mutex_unlock(&common.common_lock);

done:
	return ret;
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
						sizeof(cvs_set_media_cmd) -
						APR_HDR_SIZE);
	cvs_set_media_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
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
	case VSS_MEDIA_ID_EVRC_MODEM:
	case VSS_MEDIA_ID_4GV_NB_MODEM:
	case VSS_MEDIA_ID_4GV_WB_MODEM:
	case VSS_MEDIA_ID_4GV_NW_MODEM: {
		struct cvs_set_cdma_enc_minmax_rate_cmd cvs_set_cdma_rate;

		pr_debug("Setting EVRC min-max rate\n");

		cvs_set_cdma_rate.hdr.hdr_field =
					APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);
		cvs_set_cdma_rate.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				      sizeof(cvs_set_cdma_rate) - APR_HDR_SIZE);
		cvs_set_cdma_rate.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
		cvs_set_cdma_rate.hdr.dest_port = cvs_handle;
		cvs_set_cdma_rate.hdr.token = 0;
		cvs_set_cdma_rate.hdr.opcode =
				VSS_ISTREAM_CMD_CDMA_SET_ENC_MINMAX_RATE;
		cvs_set_cdma_rate.cdma_rate.min_rate =
				common.mvs_info.evrc_min_rate;
		cvs_set_cdma_rate.cdma_rate.max_rate =
				common.mvs_info.evrc_max_rate;

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

		if (common.mvs_info.media_type != VSS_MEDIA_ID_EVRC_MODEM) {
			ret = voice_set_dtx(v);
			if (ret < 0)
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
		cvs_set_amr_rate.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
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
		cvs_set_amrwb_rate.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
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
	mvm_start_voice_cmd.src_port =
				voice_get_idx_for_session(v->session_id);
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
	cvp_disable_cmd.src_port =
				voice_get_idx_for_session(v->session_id);
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

static void voc_get_tx_rx_topology(struct voice_data *v,
				   uint32_t *tx_topology_id,
				   uint32_t *rx_topology_id)
{
	uint32_t tx_id = 0;
	uint32_t rx_id = 0;

	if (v->lch_mode == VOICE_LCH_START) {
		pr_debug("%s: Setting TX and RX topology to NONE for LCH\n",
			 __func__);

		tx_id = VSS_IVOCPROC_TOPOLOGY_ID_NONE;
		rx_id = VSS_IVOCPROC_TOPOLOGY_ID_NONE;
	} else {
		/* Use default topology if invalid value in ACDB */
		tx_id = get_voice_tx_topology();
		if (tx_id == 0)
			tx_id = VSS_IVOCPROC_TOPOLOGY_ID_TX_SM_ECNS;

		rx_id = get_voice_rx_topology();
		if (rx_id == 0)
			rx_id = VSS_IVOCPROC_TOPOLOGY_ID_RX_DEFAULT;
	}

	*tx_topology_id = tx_id;
	*rx_topology_id = rx_id;
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
	cvp_setdev_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	cvp_setdev_cmd.hdr.dest_port = cvp_handle;
	cvp_setdev_cmd.hdr.token = 0;
	cvp_setdev_cmd.hdr.opcode = VSS_IVOCPROC_CMD_SET_DEVICE_V2;

	voc_get_tx_rx_topology(v,
			&cvp_setdev_cmd.cvp_set_device_v2.tx_topology_id,
			&cvp_setdev_cmd.cvp_set_device_v2.rx_topology_id);

	cvp_setdev_cmd.cvp_set_device_v2.tx_port_id = v->dev_tx.port_id;
	cvp_setdev_cmd.cvp_set_device_v2.rx_port_id = v->dev_rx.port_id;
	cvp_setdev_cmd.cvp_set_device_v2.vocproc_mode =
				    VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING;
	cvp_setdev_cmd.cvp_set_device_v2.ec_ref_port_id =
				    VSS_IVOCPROC_PORT_ID_NONE;
	pr_debug("topology=%d , tx_port_id=%d, rx_port_id=%d\n",
		cvp_setdev_cmd.cvp_set_device_v2.tx_topology_id,
		cvp_setdev_cmd.cvp_set_device_v2.tx_port_id,
		cvp_setdev_cmd.cvp_set_device_v2.rx_port_id);

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
	mvm_stop_voice_cmd.src_port =
				voice_get_idx_for_session(v->session_id);
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
	memset(&cvs_reg_cal_cmd, 0, sizeof(cvs_reg_cal_cmd));

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	if (!common.apr_q6_cvs) {
		pr_err("%s: apr_cvs is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	if (!common.cal_mem_handle) {
		pr_err("%s: Cal mem handle is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	get_vocstrm_cal(&cal_block);
	if (cal_block.cal_size == 0) {
		pr_err("%s: CVS cal size is 0\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	cvs_reg_cal_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvs_reg_cal_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvs_reg_cal_cmd) - APR_HDR_SIZE);
	cvs_reg_cal_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	cvs_reg_cal_cmd.hdr.dest_port = voice_get_cvs_handle(v);
	cvs_reg_cal_cmd.hdr.token = 0;
	cvs_reg_cal_cmd.hdr.opcode =
				VSS_ISTREAM_CMD_REGISTER_CALIBRATION_DATA_V2;

	cvs_reg_cal_cmd.cvs_cal_data.cal_mem_handle = common.cal_mem_handle;
	cvs_reg_cal_cmd.cvs_cal_data.cal_mem_address = cal_block.cal_paddr;
	cvs_reg_cal_cmd.cvs_cal_data.cal_mem_size = cal_block.cal_size;

	/* Get the column info corresponding to CVS cal from ACDB. */
	get_voice_col_data(VOCSTRM_CAL, &cal_block);
	if (cal_block.cal_size == 0 ||
	    cal_block.cal_size >
	    sizeof(cvs_reg_cal_cmd.cvs_cal_data.column_info)) {
		pr_err("%s: Invalid VOCSTRM_CAL size %d\n",
		       __func__, cal_block.cal_size);

		ret = -EINVAL;
		goto done;
	}
	memcpy(&cvs_reg_cal_cmd.cvs_cal_data.column_info[0],
	       (void *) cal_block.cal_kvaddr,
	       cal_block.cal_size);

	v->cvs_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(common.apr_q6_cvs, (uint32_t *) &cvs_reg_cal_cmd);
	if (ret < 0) {
		pr_err("%s: Error %d registering CVS cal\n", __func__, ret);
		ret = -EINVAL;
		goto done;
	}
	ret = wait_event_timeout(v->cvs_wait,
				 (v->cvs_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: Command timeout\n", __func__);
		ret = -EINVAL;
		goto done;
	}

done:
	return ret;
}

static int voice_send_cvs_deregister_cal_cmd(struct voice_data *v)
{
	struct cvs_deregister_cal_data_cmd cvs_dereg_cal_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	memset(&cvs_dereg_cal_cmd, 0, sizeof(cvs_dereg_cal_cmd));

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	if (!common.apr_q6_cvs) {
		pr_err("%s: apr_cvs is NULL\n", __func__);

		ret = -EPERM;
		goto done;
	}

	if (!common.cal_mem_handle) {
		pr_err("%s: Cal mem handle is NULL\n", __func__);
		ret = -EPERM;
		goto done;
	}

	get_vocstrm_cal(&cal_block);
	if (cal_block.cal_size == 0) {
		pr_err("%s: CVS cal size is 0\n", __func__);
		ret = -EPERM;
		goto done;
	}

	cvs_dereg_cal_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvs_dereg_cal_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvs_dereg_cal_cmd) - APR_HDR_SIZE);
	cvs_dereg_cal_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	cvs_dereg_cal_cmd.hdr.dest_port = voice_get_cvs_handle(v);
	cvs_dereg_cal_cmd.hdr.token = 0;
	cvs_dereg_cal_cmd.hdr.opcode =
				VSS_ISTREAM_CMD_DEREGISTER_CALIBRATION_DATA;

	v->cvs_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(common.apr_q6_cvs, (uint32_t *) &cvs_dereg_cal_cmd);
	if (ret < 0) {
		pr_err("%s: Error %d de-registering CVS cal\n", __func__, ret);
		ret = -EINVAL;
		goto done;
	}
	ret = wait_event_timeout(v->cvs_wait,
				 (v->cvs_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: Command  timeout\n", __func__);
		ret = -EINVAL;
		goto done;
	}

done:
	return ret;

}

static int voice_send_cvp_register_dev_cfg_cmd(struct voice_data *v)
{
	struct cvp_register_dev_cfg_cmd cvp_reg_dev_cfg_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	memset(&cvp_reg_dev_cfg_cmd, 0, sizeof(cvp_reg_dev_cfg_cmd));

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	if (!common.apr_q6_cvp) {
		pr_err("%s: apr_cvp is NULL\n", __func__);

		ret = -EPERM;
		goto done;
	}

	if (!common.cal_mem_handle) {
		pr_err("%s: Cal mem handle is NULL\n", __func__);
		ret = -EPERM;
		goto done;
	}

	get_vocproc_dev_cfg_cal(&cal_block);
	if (cal_block.cal_size == 0) {
		pr_err("%s: CVP cal size is 0\n", __func__);
		ret = -EPERM;
		goto done;
	}

	cvp_reg_dev_cfg_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_reg_dev_cfg_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_reg_dev_cfg_cmd) - APR_HDR_SIZE);
	cvp_reg_dev_cfg_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	cvp_reg_dev_cfg_cmd.hdr.dest_port = voice_get_cvp_handle(v);
	cvp_reg_dev_cfg_cmd.hdr.token = 0;
	cvp_reg_dev_cfg_cmd.hdr.opcode =
					VSS_IVOCPROC_CMD_REGISTER_DEVICE_CONFIG;

	cvp_reg_dev_cfg_cmd.cvp_dev_cfg_data.mem_handle = common.cal_mem_handle;
	cvp_reg_dev_cfg_cmd.cvp_dev_cfg_data.mem_address = cal_block.cal_paddr;
	cvp_reg_dev_cfg_cmd.cvp_dev_cfg_data.mem_size = cal_block.cal_size;

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(common.apr_q6_cvp,
			   (uint32_t *) &cvp_reg_dev_cfg_cmd);
	if (ret < 0) {
		pr_err("%s: Error %d registering CVP dev cfg cal\n",
		       __func__, ret);
		ret = -EINVAL;
		goto done;
	}
	ret = wait_event_timeout(v->cvp_wait,
				 (v->cvp_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: Command timeout\n", __func__);
		ret = -EINVAL;
		goto done;
	}

done:
	return ret;
}

static int voice_send_cvp_deregister_dev_cfg_cmd(struct voice_data *v)
{
	struct cvp_deregister_dev_cfg_cmd cvp_dereg_dev_cfg_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	memset(&cvp_dereg_dev_cfg_cmd, 0, sizeof(cvp_dereg_dev_cfg_cmd));

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	if (!common.apr_q6_cvp) {
		pr_err("%s: apr_cvp is NULL\n", __func__);

		ret = -EPERM;
		goto done;
	}

	if (!common.cal_mem_handle) {
		pr_err("%s: Cal mem handle is NULL\n", __func__);
		ret = -EPERM;
		goto done;
	}

	get_vocproc_dev_cfg_cal(&cal_block);
	if (cal_block.cal_size == 0) {
		pr_err("%s: CVP cal size is 0\n", __func__);
		ret = -EPERM;
		goto done;
	}

	cvp_dereg_dev_cfg_cmd.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_dereg_dev_cfg_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_dereg_dev_cfg_cmd) - APR_HDR_SIZE);
	cvp_dereg_dev_cfg_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	cvp_dereg_dev_cfg_cmd.hdr.dest_port = voice_get_cvp_handle(v);
	cvp_dereg_dev_cfg_cmd.hdr.token = 0;
	cvp_dereg_dev_cfg_cmd.hdr.opcode =
				VSS_IVOCPROC_CMD_DEREGISTER_DEVICE_CONFIG;

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(common.apr_q6_cvp,
			   (uint32_t *) &cvp_dereg_dev_cfg_cmd);
	if (ret < 0) {
		pr_err("%s: Error %d de-registering CVP dev cfg cal\n",
		       __func__, ret);
		ret = -EINVAL;
		goto done;
	}
	ret = wait_event_timeout(v->cvp_wait,
				 (v->cvp_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: Command timeout\n", __func__);
		ret = -EINVAL;
		goto done;
	}

done:
	return ret;
}

static int voice_send_cvp_register_cal_cmd(struct voice_data *v)
{
	struct cvp_register_cal_data_cmd cvp_reg_cal_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	memset(&cvp_reg_cal_cmd, 0, sizeof(cvp_reg_cal_cmd));

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	if (!common.apr_q6_cvp) {
		pr_err("%s: apr_cvp is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	if (!common.cal_mem_handle) {
		pr_err("%s: Cal mem handle is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	get_vocproc_cal(&cal_block);
	if (cal_block.cal_size == 0) {
		pr_err("%s: CVP cal size is 0\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	cvp_reg_cal_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_reg_cal_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_reg_cal_cmd) - APR_HDR_SIZE);
	cvp_reg_cal_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	cvp_reg_cal_cmd.hdr.dest_port = voice_get_cvp_handle(v);
	cvp_reg_cal_cmd.hdr.token = 0;
	cvp_reg_cal_cmd.hdr.opcode =
				VSS_IVOCPROC_CMD_REGISTER_CALIBRATION_DATA_V2;

	cvp_reg_cal_cmd.cvp_cal_data.cal_mem_handle = common.cal_mem_handle;
	cvp_reg_cal_cmd.cvp_cal_data.cal_mem_address = cal_block.cal_paddr;
	cvp_reg_cal_cmd.cvp_cal_data.cal_mem_size = cal_block.cal_size;

	/* Get the column info corresponding to CVP cal from ACDB. */
	get_voice_col_data(VOCPROC_CAL, &cal_block);
	if (cal_block.cal_size == 0 ||
	    cal_block.cal_size >
	    sizeof(cvp_reg_cal_cmd.cvp_cal_data.column_info)) {
		pr_err("%s: Invalid VOCPROC_CAL size %d\n",
		       __func__, cal_block.cal_size);

		ret = -EINVAL;
		goto done;
	}

	memcpy(&cvp_reg_cal_cmd.cvp_cal_data.column_info[0],
	       (void *) cal_block.cal_kvaddr,
	       cal_block.cal_size);

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(common.apr_q6_cvp, (uint32_t *) &cvp_reg_cal_cmd);
	if (ret < 0) {
		pr_err("%s: Error %d registering CVP cal\n", __func__, ret);
		ret = -EINVAL;
		goto done;
	}
	ret = wait_event_timeout(v->cvp_wait,
				 (v->cvp_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: Command timeout\n", __func__);
		ret = -EINVAL;
		goto done;
	}

done:
	return ret;
}

static int voice_send_cvp_deregister_cal_cmd(struct voice_data *v)
{
	struct cvp_deregister_cal_data_cmd cvp_dereg_cal_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	memset(&cvp_dereg_cal_cmd, 0, sizeof(cvp_dereg_cal_cmd));

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	if (!common.apr_q6_cvp) {
		pr_err("%s: apr_cvp is NULL.\n", __func__);

		ret = -EPERM;
		goto done;
	}

	if (!common.cal_mem_handle) {
		pr_err("%s: Cal mem handle is NULL\n", __func__);
		ret = -EPERM;
		goto done;
	}

	get_vocproc_cal(&cal_block);
	if (cal_block.cal_size == 0) {
		pr_err("%s: CVP vol cal size is 0\n", __func__);
		ret = -EPERM;
		goto done;
	}

	cvp_dereg_cal_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_dereg_cal_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_dereg_cal_cmd) - APR_HDR_SIZE);
	cvp_dereg_cal_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	cvp_dereg_cal_cmd.hdr.dest_port = voice_get_cvp_handle(v);
	cvp_dereg_cal_cmd.hdr.token = 0;
	cvp_dereg_cal_cmd.hdr.opcode =
				VSS_IVOCPROC_CMD_DEREGISTER_CALIBRATION_DATA;

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(common.apr_q6_cvp, (uint32_t *) &cvp_dereg_cal_cmd);
	if (ret < 0) {
		pr_err("%s: Error %d de-registering CVP cal\n", __func__, ret);
		ret = -EINVAL;
		goto done;
	}
	ret = wait_event_timeout(v->cvp_wait,
				 (v->cvp_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: Command timeout\n", __func__);
		ret = -EINVAL;
		goto done;
	}

done:
	return -EINVAL;
}

static int voice_send_cvp_register_vol_cal_cmd(struct voice_data *v)
{
	struct cvp_register_vol_cal_data_cmd cvp_reg_vol_cal_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	memset(&cvp_reg_vol_cal_cmd, 0, sizeof(cvp_reg_vol_cal_cmd));

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	if (!common.apr_q6_cvp) {
		pr_err("%s: apr_cvp is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	if (!common.cal_mem_handle) {
		pr_err("%s: Cal mem handle is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	get_vocvol_cal(&cal_block);
	if (cal_block.cal_size == 0) {
		pr_err("%s: CVP vol cal size is 0\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	cvp_reg_vol_cal_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_reg_vol_cal_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_reg_vol_cal_cmd) - APR_HDR_SIZE);
	cvp_reg_vol_cal_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	cvp_reg_vol_cal_cmd.hdr.dest_port = voice_get_cvp_handle(v);
	cvp_reg_vol_cal_cmd.hdr.token = 0;
	cvp_reg_vol_cal_cmd.hdr.opcode =
			VSS_IVOCPROC_CMD_REGISTER_VOL_CALIBRATION_DATA;

	cvp_reg_vol_cal_cmd.cvp_vol_cal_data.cal_mem_handle =
							common.cal_mem_handle;
	cvp_reg_vol_cal_cmd.cvp_vol_cal_data.cal_mem_address =
							cal_block.cal_paddr;
	cvp_reg_vol_cal_cmd.cvp_vol_cal_data.cal_mem_size = cal_block.cal_size;

	/* Get the column info corresponding to CVP volume cal from ACDB. */
	get_voice_col_data(VOCVOL_CAL, &cal_block);
	if (cal_block.cal_size == 0 ||
	    cal_block.cal_size >
	    sizeof(cvp_reg_vol_cal_cmd.cvp_vol_cal_data.column_info)) {
		pr_err("%s: Invalid VOCVOL_CAL size %d\n",
		       __func__, cal_block.cal_size);

		ret = -EINVAL;
		goto done;
	}

	memcpy(&cvp_reg_vol_cal_cmd.cvp_vol_cal_data.column_info[0],
	       (void *) cal_block.cal_kvaddr,
	       cal_block.cal_size);

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(common.apr_q6_cvp,
			   (uint32_t *) &cvp_reg_vol_cal_cmd);
	if (ret < 0) {
		pr_err("%s: Error %d registering CVP vol cal\n", __func__, ret);
		ret = -EINVAL;
		goto done;
	}
	ret = wait_event_timeout(v->cvp_wait,
				 (v->cvp_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: Command timeout\n", __func__);
		ret = -EINVAL;
		goto done;
	}

done:
	return ret;
}

static int voice_send_cvp_deregister_vol_cal_cmd(struct voice_data *v)
{
	struct cvp_deregister_vol_cal_data_cmd cvp_dereg_vol_cal_cmd;
	struct acdb_cal_block cal_block;
	int ret = 0;
	memset(&cvp_dereg_vol_cal_cmd, 0, sizeof(cvp_dereg_vol_cal_cmd));

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	if (!common.apr_q6_cvp) {
		pr_err("%s: apr_cvp is NULL\n", __func__);

		ret = -EPERM;
		goto done;
	}

	if (!common.cal_mem_handle) {
		pr_err("%s: Cal mem handle is NULL\n", __func__);
		ret = -EPERM;
		goto done;
	}

	get_vocvol_cal(&cal_block);
	if (cal_block.cal_size == 0) {
		pr_err("%s: CVP vol cal size is 0\n", __func__);
		ret = -EPERM;
		goto done;
	}

	cvp_dereg_vol_cal_cmd.hdr.hdr_field =
			APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvp_dereg_vol_cal_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_dereg_vol_cal_cmd) - APR_HDR_SIZE);
	cvp_dereg_vol_cal_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	cvp_dereg_vol_cal_cmd.hdr.dest_port = voice_get_cvp_handle(v);
	cvp_dereg_vol_cal_cmd.hdr.token = 0;
	cvp_dereg_vol_cal_cmd.hdr.opcode =
			VSS_IVOCPROC_CMD_DEREGISTER_VOL_CALIBRATION_DATA;

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(common.apr_q6_cvp,
			   (uint32_t *) &cvp_dereg_vol_cal_cmd);
	if (ret < 0) {
		pr_err("%s: Error %d de-registering CVP vol cal\n",
		       __func__, ret);
		ret = -EINVAL;
		goto done;
	}
	ret = wait_event_timeout(v->cvp_wait,
				 (v->cvp_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: Command timeout\n", __func__);
		ret = -EINVAL;
		goto done;
	}

done:
	return ret;
}

static int voice_map_memory_physical_cmd(struct voice_data *v,
					 struct mem_map_table *table_info,
					 dma_addr_t phys,
					 uint32_t size,
					 uint32_t token)
{
	struct vss_imemory_cmd_map_physical_t mvm_map_phys_cmd;
	uint32_t *memtable;
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		goto fail;
	}

	if (!common.apr_q6_mvm) {
		pr_err("%s: apr_mvm is NULL.\n", __func__);

		goto fail;
	}

	if (!table_info->data) {
		pr_err("%s: memory table is NULL.\n", __func__);

		goto fail;
	}

	memtable = (uint32_t *) table_info->data;

	/*
	 * Store next table descriptor's address(64 bit) as NULL as there
	 * is only one memory block
	 */
	memtable[0] = (uint32_t)NULL;
	memtable[1] = (uint32_t)NULL;

	/* Store next table descriptor's size */
	memtable[2] = 0;

	/* Store shared mem add */
	memtable[3] = phys;
	memtable[4] = 0;

	/* Store shared memory size */
	memtable[5] = size;

	mvm_map_phys_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	mvm_map_phys_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(mvm_map_phys_cmd) - APR_HDR_SIZE);
	mvm_map_phys_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	mvm_map_phys_cmd.hdr.dest_port = voice_get_mvm_handle(v);
	mvm_map_phys_cmd.hdr.token = token;
	mvm_map_phys_cmd.hdr.opcode = VSS_IMEMORY_CMD_MAP_PHYSICAL;

	mvm_map_phys_cmd.table_descriptor.mem_address = table_info->phys;
	mvm_map_phys_cmd.table_descriptor.mem_size =
			sizeof(struct vss_imemory_block_t) +
			sizeof(struct vss_imemory_table_descriptor_t);
	mvm_map_phys_cmd.is_cached = true;
	mvm_map_phys_cmd.cache_line_size = 128;
	mvm_map_phys_cmd.access_mask = 3;
	mvm_map_phys_cmd.page_align = 4096;
	mvm_map_phys_cmd.min_data_width = 8;
	mvm_map_phys_cmd.max_data_width = 64;

	pr_debug("%s: next table desc: add: %lld, size: %d\n",
		 __func__, *((uint64_t *) memtable),
		 *(((uint32_t *) memtable) + 2));
	pr_debug("%s: phy add of of mem being mapped 0x%x, size: %d\n",
		 __func__, *(((uint32_t *) memtable) + 3),
		 *(((uint32_t *) memtable) + 5));

	v->mvm_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(common.apr_q6_mvm, (uint32_t *) &mvm_map_phys_cmd);
	if (ret < 0) {
		pr_err("%s: Error %d sending mvm map phy cmd\n", __func__, ret);

		goto fail;
	}

	ret = wait_event_timeout(v->mvm_wait,
				 (v->mvm_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: Command timeout\n", __func__);

		goto fail;
	}

	return 0;

fail:
	return -EINVAL;
}

static int voice_mem_map_cal_block(struct voice_data *v)
{
	int ret = 0;
	struct acdb_cal_block cal_block;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		return -EINVAL;
	}

	mutex_lock(&common.common_lock);
	if (common.cal_mem_handle != 0) {
		pr_debug("%s: Cal block already mem mapped\n", __func__);

		goto done;
	}

	/* Get the physical address of calibration memory block from ACDB. */
	get_voice_cal_allocation(&cal_block);

	if (!cal_block.cal_paddr) {
		pr_err("%s: Cal block not allocated\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	ret = voice_map_memory_physical_cmd(v,
					    &common.cal_mem_map_table,
					    cal_block.cal_paddr,
					    cal_block.cal_size,
					    VOC_CAL_MEM_MAP_TOKEN);

done:
	mutex_unlock(&common.common_lock);
	return ret;
}

static int voice_pause_voice_call(struct voice_data *v)
{
	struct apr_hdr	mvm_pause_voice_cmd;
	void		*apr_mvm;
	int		ret = 0;

	pr_debug("%s\n", __func__);

	if (v == NULL) {
		pr_err("%s: Voice data is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	apr_mvm = common.apr_q6_mvm;
	if (!apr_mvm) {
		pr_err("%s: apr_mvm is NULL.\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	mvm_pause_voice_cmd.hdr_field =
		APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	mvm_pause_voice_cmd.pkt_size =
		APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(mvm_pause_voice_cmd) - APR_HDR_SIZE);
	mvm_pause_voice_cmd.src_port =
			voice_get_idx_for_session(v->session_id);
	mvm_pause_voice_cmd.dest_port = voice_get_mvm_handle(v);
	mvm_pause_voice_cmd.token = 0;
	mvm_pause_voice_cmd.opcode = VSS_IMVM_CMD_PAUSE_VOICE;
	v->mvm_state = CMD_STATUS_FAIL;

	pr_debug("%s: send mvm_pause_voice_cmd pkt size = %d\n",
		__func__, mvm_pause_voice_cmd.pkt_size);

	ret = apr_send_pkt(apr_mvm,
		(uint32_t *)&mvm_pause_voice_cmd);
	if (ret < 0) {
		pr_err("Fail in sending VSS_IMVM_CMD_PAUSE_VOICE\n");

		ret = -EINVAL;
		goto done;
	}

	ret = wait_event_timeout(v->mvm_wait,
		(v->mvm_state == CMD_STATUS_SUCCESS),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: Command timeout\n", __func__);

		ret = -EINVAL;
		goto done;
	}

done:
	return ret;
}

int voc_register_vocproc_vol_table(void)
{
	int			result = 0;
	int			result2 = 0;
	int			i;
	struct voice_data	*v = NULL;

	pr_debug("%s\n", __func__);

	mutex_lock(&common.common_lock);
	for (i = 0; i < MAX_VOC_SESSIONS; i++) {
		v = &common.voice[i];

		mutex_lock(&v->lock);
		if (is_voc_state_active(v->voc_state)) {
			result2 = voice_send_cvp_register_vol_cal_cmd(v);
			if (result2 < 0) {
				pr_err("%s: Failed to register vocvol table for session 0x%x!\n",
					__func__, v->session_id);

				result = result2;
				/* Still try to register other sessions */
			}
		}
		mutex_unlock(&v->lock);
	}

	mutex_unlock(&common.common_lock);
	return result;
}

int voc_deregister_vocproc_vol_table(void)
{
	int			result = 0;
	int			success = 0;
	int			i;
	struct voice_data	*v = NULL;

	pr_debug("%s\n", __func__);

	mutex_lock(&common.common_lock);
	for (i = 0; i < MAX_VOC_SESSIONS; i++) {
		v = &common.voice[i];

		mutex_lock(&v->lock);
		if (is_voc_state_active(v->voc_state)) {
			result = voice_send_cvp_deregister_vol_cal_cmd(v);
			if (result < 0) {
				pr_err("%s: Failed to deregister vocvol table for session 0x%x!\n",
					__func__, v->session_id);

				mutex_unlock(&v->lock);
				mutex_unlock(&common.common_lock);
				if (success) {
					pr_err("%s: Try to re-register all deregistered sessions!\n",
						__func__);

					voc_register_vocproc_vol_table();
				}
				goto done;
			} else {
				success = 1;
			}
		}
		mutex_unlock(&v->lock);
	}
	mutex_unlock(&common.common_lock);
done:
	return result;
}

int voc_map_rtac_block(struct rtac_cal_block_data *cal_block)
{
	int			result = 0;
	struct voice_data	*v = NULL;

	pr_debug("%s\n", __func__);

	if (cal_block == NULL) {
		pr_err("%s: cal_block is NULL!\n",
			__func__);

		result = -EINVAL;
		goto done;
	}

	if (cal_block->cal_data.paddr == 0) {
		pr_debug("%s: No address to map!\n",
			__func__);

		result = -EINVAL;
		goto done;
	}

	if (cal_block->map_data.map_size == 0) {
		pr_debug("%s: map size is 0!\n",
			__func__);

		result = -EINVAL;
		goto done;
	}

	mutex_lock(&common.common_lock);
	/* use first session */
	v = &common.voice[0];
	mutex_lock(&v->lock);

	if (!is_rtac_memory_allocated()) {
		result = voice_alloc_rtac_mem_map_table();
		if (result < 0) {
			pr_err("%s: RTAC alloc mem map table did not work! addr = 0x%x, size = %d\n",
				__func__, cal_block->cal_data.paddr,
				cal_block->map_data.map_size);

			goto err;
		}
	}

	result = voice_map_memory_physical_cmd(v,
		&common.rtac_mem_map_table,
		(dma_addr_t)cal_block->cal_data.paddr,
		cal_block->map_data.map_size,
		VOC_RTAC_MEM_MAP_TOKEN);
	if (result < 0) {
		pr_err("%s: RTAC mmap did not work! addr = 0x%x, size = %d\n",
			__func__, cal_block->cal_data.paddr,
			cal_block->map_data.map_size);

		free_rtac_map_table();
		goto err;
	}

	cal_block->map_data.map_handle = common.rtac_mem_handle;
err:
	mutex_unlock(&v->lock);
	mutex_unlock(&common.common_lock);
done:
	return result;
}

int voc_unmap_rtac_block(uint32_t *mem_map_handle)
{
	int			result = 0;
	struct voice_data	*v = NULL;

	pr_debug("%s\n", __func__);

	if (mem_map_handle == NULL) {
		pr_debug("%s: Map handle is NULL, nothing to unmap\n",
			__func__);

		goto done;
	}

	if (*mem_map_handle == 0) {
		pr_debug("%s: Map handle is 0, nothing to unmap\n",
			__func__);

		goto done;
	}

	mutex_lock(&common.common_lock);
	/* use first session */
	v = &common.voice[0];
	mutex_lock(&v->lock);

	result = voice_send_mvm_unmap_memory_physical_cmd(
			v, *mem_map_handle);
	if (result) {
		pr_err("%s: voice_send_mvm_unmap_memory_physical_cmd Failed for session 0x%x!\n",
			__func__, v->session_id);
	} else {
		*mem_map_handle = 0;
		common.rtac_mem_handle = 0;
		free_rtac_map_table();
	}
	mutex_unlock(&v->lock);
	mutex_unlock(&common.common_lock);
done:
	return result;
}

int voc_unmap_cal_blocks(void)
{
	int			result = 0;
	int			result2 = 0;
	int			i;
	struct voice_data	*v = NULL;

	pr_debug("%s\n", __func__);

	mutex_lock(&common.common_lock);

	if (common.cal_mem_handle == 0)
		goto done;

	for (i = 0; i < MAX_VOC_SESSIONS; i++) {
		v = &common.voice[i];

		mutex_lock(&v->lock);
		if (is_voc_state_active(v->voc_state)) {
			result2 = voice_pause_voice_call(v);
			if (result2 < 0) {
				pr_err("%s: voice_pause_voice_call failed for session 0x%x, err %d!\n",
					__func__, v->session_id, result2);

				result = result2;
			}

			voice_send_cvp_deregister_vol_cal_cmd(v);
			voice_send_cvp_deregister_cal_cmd(v);
			voice_send_cvp_deregister_dev_cfg_cmd(v);
			voice_send_cvs_deregister_cal_cmd(v);

			result2 = voice_send_start_voice_cmd(v);
			if (result2) {
				pr_err("%s: voice_send_start_voice_cmd failed for session 0x%x, err %d!\n",
					__func__, v->session_id, result2);

				result = result2;
			}
		}

		if ((common.cal_mem_handle != 0) &&
			(!is_other_session_active(v->session_id))) {

			result2 = voice_send_mvm_unmap_memory_physical_cmd(
				v, common.cal_mem_handle);
			if (result2) {
				pr_err("%s: voice_send_mvm_unmap_memory_physical_cmd failed for session 0x%x, err %d!\n",
					__func__, v->session_id, result2);

				result = result2;
			} else {
				common.cal_mem_handle = 0;
				free_cal_map_table();
			}
		}
		mutex_unlock(&v->lock);
	}
done:
	mutex_unlock(&common.common_lock);
	return result;
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
	cvp_session_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	cvp_session_cmd.hdr.dest_port = 0;
	cvp_session_cmd.hdr.token = 0;
	cvp_session_cmd.hdr.opcode =
			VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION_V2;

	voc_get_tx_rx_topology(v,
			&cvp_session_cmd.cvp_session.tx_topology_id,
			&cvp_session_cmd.cvp_session.rx_topology_id);

	cvp_session_cmd.cvp_session.direction = 2; /*tx and rx*/
	cvp_session_cmd.cvp_session.tx_port_id = v->dev_tx.port_id;
	cvp_session_cmd.cvp_session.rx_port_id = v->dev_rx.port_id;
	cvp_session_cmd.cvp_session.profile_id =
					 VSS_ICOMMON_CAL_NETWORK_ID_NONE;
	cvp_session_cmd.cvp_session.vocproc_mode =
				 VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING;
	cvp_session_cmd.cvp_session.ec_ref_port_id =
						 VSS_IVOCPROC_PORT_ID_NONE;

	pr_debug("tx_topology: %d tx_port_id=%d, rx_port_id=%d, mode: 0x%x\n",
		cvp_session_cmd.cvp_session.tx_topology_id,
		cvp_session_cmd.cvp_session.tx_port_id,
		cvp_session_cmd.cvp_session.rx_port_id,
		cvp_session_cmd.cvp_session.vocproc_mode);
	pr_debug("rx_topology: %d, profile_id: 0x%x, pkt_size: %d\n",
		cvp_session_cmd.cvp_session.rx_topology_id,
		cvp_session_cmd.cvp_session.profile_id,
		cvp_session_cmd.hdr.pkt_size);

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

	voice_send_cvs_register_cal_cmd(v);
	voice_send_cvp_register_dev_cfg_cmd(v);
	voice_send_cvp_register_cal_cmd(v);
	voice_send_cvp_register_vol_cal_cmd(v);

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

	if (is_voip_session(v->session_id)) {
		ret = voice_send_mvm_cal_network_cmd(v);
		if (ret < 0)
			pr_err("%s: voice_send_mvm_cal_network_cmd: %d\n",
				__func__, ret);

		ret = voice_send_mvm_media_type_cmd(v);
		if (ret < 0)
			pr_err("%s: voice_send_mvm_media_type_cmd: %d\n",
				__func__, ret);

		voice_send_netid_timing_cmd(v);
	}

	/* enable slowtalk if st_enable is set */
	if (v->st_enable)
		voice_send_set_pp_enable_cmd(v,
					     MODULE_ID_VOICE_MODULE_ST,
					     v->st_enable);
	/* Start in-call music delivery if this feature is enabled */
	if (v->music_info.play_enable)
		voice_cvs_start_playback(v);

	/* Start in-call recording if this feature is enabled */
	if (v->rec_info.rec_enable)
		voice_cvs_start_record(v, v->rec_info.rec_mode);

	if (v->dtmf_rx_detect_en)
		voice_send_dtmf_rx_detection_cmd(v, v->dtmf_rx_detect_en);

	rtac_add_voice(voice_get_cvs_handle(v),
		voice_get_cvp_handle(v),
		v->dev_rx.port_id, v->dev_tx.port_id,
		v->session_id);

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
	cvp_enable_cmd.src_port =
				voice_get_idx_for_session(v->session_id);
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

static int voice_send_mvm_cal_network_cmd(struct voice_data *v)
{
	struct vss_imvm_cmd_set_cal_network_t mvm_set_cal_network;
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

	mvm_set_cal_network.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	mvm_set_cal_network.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(mvm_set_cal_network) - APR_HDR_SIZE);
	mvm_set_cal_network.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	mvm_set_cal_network.hdr.dest_port = mvm_handle;
	mvm_set_cal_network.hdr.token = 0;
	mvm_set_cal_network.hdr.opcode = VSS_IMVM_CMD_SET_CAL_NETWORK;
	mvm_set_cal_network.network_id = VSS_ICOMMON_CAL_NETWORK_ID_NONE;

	v->mvm_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_mvm, (uint32_t *) &mvm_set_cal_network);
	if (ret < 0) {
		pr_err("%s: Error %d sending SET_NETWORK\n", __func__, ret);
		goto fail;
	}

	ret = wait_event_timeout(v->mvm_wait,
				(v->mvm_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout %d\n", __func__, ret);
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
	mvm_set_network.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
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
	mvm_set_voice_timing.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
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
	mvm_a_vocproc_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
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
	/* If voice call is active during VoLTE, SRVCC happens.
	   Start recording on voice session if recording started during VoLTE.
	 */
	if (is_volte_session(v->session_id) &&
	    ((common.voice[VOC_PATH_PASSIVE].voc_state == VOC_RUN) ||
	     (common.voice[VOC_PATH_PASSIVE].voc_state == VOC_CHANGE))) {
		if (v->rec_info.rec_enable) {
			voice_cvs_start_record(
				&common.voice[VOC_PATH_PASSIVE],
				v->rec_info.rec_mode);
			common.srvcc_rec_flag = true;

			pr_debug("%s: switch recording, srvcc_rec_flag %d\n",
				 __func__, common.srvcc_rec_flag);
		}
	}
	/* send stop voice cmd */
	voice_send_stop_voice_cmd(v);

	/* send stop dtmf detecton cmd */
	if (v->dtmf_rx_detect_en)
		voice_send_dtmf_rx_detection_cmd(v, 0);

	/* reset LCH mode */
	v->lch_mode = 0;

	/* clear mute setting */
	v->dev_rx.dev_mute =  common.default_mute_val;
	v->dev_tx.dev_mute =  common.default_mute_val;
	v->stream_rx.stream_mute = common.default_mute_val;
	v->stream_tx.stream_mute = common.default_mute_val;

	/* detach VOCPROC and wait for response from mvm */
	mvm_d_vocproc_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	mvm_d_vocproc_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(mvm_d_vocproc_cmd) - APR_HDR_SIZE);
	pr_debug("mvm_d_vocproc_cmd  pkt size = %d\n",
		mvm_d_vocproc_cmd.hdr.pkt_size);
	mvm_d_vocproc_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
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

	voice_send_cvp_deregister_vol_cal_cmd(v);
	voice_send_cvp_deregister_cal_cmd(v);
	voice_send_cvp_deregister_dev_cfg_cmd(v);
	voice_send_cvs_deregister_cal_cmd(v);

	/* destrop cvp session */
	cvp_destroy_session_cmd.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	cvp_destroy_session_cmd.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_destroy_session_cmd) - APR_HDR_SIZE);
	pr_debug("cvp_destroy_session_cmd pkt size = %d\n",
		cvp_destroy_session_cmd.pkt_size);
	cvp_destroy_session_cmd.src_port =
				voice_get_idx_for_session(v->session_id);
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

	rtac_remove_voice(voice_get_cvs_handle(v));
	cvp_handle = 0;
	voice_set_cvp_handle(v, cvp_handle);
	return 0;
fail:
	return -EINVAL;
}

static int voice_send_mvm_unmap_memory_physical_cmd(struct voice_data *v,
						    uint32_t mem_handle)
{
	struct vss_imemory_cmd_unmap_t mem_unmap;
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

	mem_unmap.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	mem_unmap.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(mem_unmap) - APR_HDR_SIZE);
	mem_unmap.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	mem_unmap.hdr.dest_port = mvm_handle;
	mem_unmap.hdr.token = 0;
	mem_unmap.hdr.opcode = VSS_IMEMORY_CMD_UNMAP;
	mem_unmap.mem_handle = mem_handle;

	pr_debug("%s: mem_handle: ox%x\n", __func__, mem_unmap.mem_handle);

	v->mvm_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_mvm, (uint32_t *) &mem_unmap);
	if (ret < 0) {
		pr_err("mem_unmap op[0x%x]ret[%d]\n",
			mem_unmap.hdr.opcode, ret);
		goto fail;
	}

	ret = wait_event_timeout(v->mvm_wait,
				 (v->mvm_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout %d\n", __func__, ret);
		goto fail;
	}
	return 0;

fail:
	return ret;
}

static int voice_send_cvs_packet_exchange_config_cmd(struct voice_data *v)
{
	struct vss_istream_cmd_set_oob_packet_exchange_config_t
						 packet_exchange_config_pkt;
	int ret = 0;
	uint64_t *dec_buf;
	uint64_t *enc_buf;
	void *apr_cvs;
	u16 cvs_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	dec_buf = (uint64_t *)v->shmem_info.sh_buf.buf[0].phys;
	enc_buf = (uint64_t *)v->shmem_info.sh_buf.buf[1].phys;

	apr_cvs = common.apr_q6_cvs;

	if (!apr_cvs) {
		pr_err("%s: apr_cvs is NULL.\n", __func__);
		return -EINVAL;
	}
	cvs_handle = voice_get_cvs_handle(v);

	packet_exchange_config_pkt.hdr.hdr_field = APR_HDR_FIELD(
						APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	packet_exchange_config_pkt.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(packet_exchange_config_pkt) -
					 APR_HDR_SIZE);
	packet_exchange_config_pkt.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	packet_exchange_config_pkt.hdr.dest_port = cvs_handle;
	packet_exchange_config_pkt.hdr.token = 0;
	packet_exchange_config_pkt.hdr.opcode =
			 VSS_ISTREAM_CMD_SET_OOB_PACKET_EXCHANGE_CONFIG;
	packet_exchange_config_pkt.mem_handle = v->shmem_info.mem_handle;
	packet_exchange_config_pkt.dec_buf_addr = (uint32_t)dec_buf;
	packet_exchange_config_pkt.dec_buf_size = 4096;
	packet_exchange_config_pkt.enc_buf_addr = (uint32_t)enc_buf;
	packet_exchange_config_pkt.enc_buf_size = 4096;

	pr_debug("%s: dec buf: add %p, size %d, enc buf: add %p, size %d\n",
		__func__,
		dec_buf,
		packet_exchange_config_pkt.dec_buf_size,
		enc_buf,
		packet_exchange_config_pkt.enc_buf_size);

	v->cvs_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvs, (uint32_t *) &packet_exchange_config_pkt);
	if (ret < 0) {
		pr_err("Failed to send packet exchange config cmd %d\n", ret);
		goto fail;
	}

	ret = wait_event_timeout(v->cvs_wait,
				 (v->cvs_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret)
		pr_err("%s: wait_event timeout %d\n", __func__, ret);

	return 0;
fail:
	return -EINVAL;
}

static int voice_send_cvs_data_exchange_mode_cmd(struct voice_data *v)
{
	struct vss_istream_cmd_set_packet_exchange_mode_t data_exchange_pkt;
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

	data_exchange_pkt.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	data_exchange_pkt.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(data_exchange_pkt) - APR_HDR_SIZE);
	data_exchange_pkt.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	data_exchange_pkt.hdr.dest_port = cvs_handle;
	data_exchange_pkt.hdr.token = 0;
	data_exchange_pkt.hdr.opcode = VSS_ISTREAM_CMD_SET_PACKET_EXCHANGE_MODE;
	data_exchange_pkt.mode = VSS_ISTREAM_PACKET_EXCHANGE_MODE_OUT_OF_BAND;

	v->cvs_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvs, (uint32_t *) &data_exchange_pkt);
	if (ret < 0) {
		pr_err("Failed to send data exchange mode %d\n", ret);
		goto fail;
	}

	ret = wait_event_timeout(v->cvs_wait,
				 (v->cvs_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret)
		pr_err("%s: wait_event timeout %d\n", __func__, ret);

	return 0;
fail:
	return -EINVAL;
}

static int voice_send_stream_mute_cmd(struct voice_data *v, uint16_t direction,
				     uint16_t mute_flag, uint32_t ramp_duration)
{
	struct cvs_set_mute_cmd cvs_mute_cmd;
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		goto fail;
	}

	if (!common.apr_q6_cvs) {
		pr_err("%s: apr_cvs is NULL.\n", __func__);

		goto fail;
	}

	/* send mute/unmute to cvs */
	cvs_mute_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	cvs_mute_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(cvs_mute_cmd) - APR_HDR_SIZE);
	cvs_mute_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	cvs_mute_cmd.hdr.dest_port = voice_get_cvs_handle(v);
	cvs_mute_cmd.hdr.token = 0;
	cvs_mute_cmd.hdr.opcode = VSS_IVOLUME_CMD_MUTE_V2;
	cvs_mute_cmd.cvs_set_mute.direction = direction;
	cvs_mute_cmd.cvs_set_mute.mute_flag = mute_flag;
	cvs_mute_cmd.cvs_set_mute.ramp_duration_ms = ramp_duration;

	v->cvs_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(common.apr_q6_cvs, (uint32_t *) &cvs_mute_cmd);
	if (ret < 0) {
		pr_err("%s: Error %d sending stream mute\n", __func__, ret);

		goto fail;
	}
	ret = wait_event_timeout(v->cvs_wait,
				 (v->cvs_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: Command timeout\n", __func__);

		goto fail;
	}

	return 0;

fail:
	return -EINVAL;
}

static int voice_send_device_mute_cmd(struct voice_data *v, uint16_t direction,
				     uint16_t mute_flag, uint32_t ramp_duration)
{
	struct cvp_set_mute_cmd cvp_mute_cmd;
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		goto fail;
	}

	if (!common.apr_q6_cvp) {
		pr_err("%s: apr_cvp is NULL.\n", __func__);

		goto fail;
	}

	cvp_mute_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	cvp_mute_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(cvp_mute_cmd) - APR_HDR_SIZE);
	cvp_mute_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	cvp_mute_cmd.hdr.dest_port = voice_get_cvp_handle(v);
	cvp_mute_cmd.hdr.token = 0;
	cvp_mute_cmd.hdr.opcode = VSS_IVOLUME_CMD_MUTE_V2;
	cvp_mute_cmd.cvp_set_mute.direction = direction;
	cvp_mute_cmd.cvp_set_mute.mute_flag = mute_flag;
	cvp_mute_cmd.cvp_set_mute.ramp_duration_ms = ramp_duration;

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(common.apr_q6_cvp, (uint32_t *) &cvp_mute_cmd);
	if (ret < 0) {
		pr_err("%s: Error %d sending rx device cmd\n", __func__, ret);

		goto fail;
	}
	ret = wait_event_timeout(v->cvp_wait,
				 (v->cvp_state == CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: Command timeout\n", __func__);

		goto fail;
	}

	return 0;

fail:
	return -EINVAL;
}

static int voice_send_vol_step_cmd(struct voice_data *v)
{
	struct cvp_set_rx_volume_step_cmd cvp_vol_step_cmd;
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
	cvp_vol_step_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	cvp_vol_step_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvp_vol_step_cmd) - APR_HDR_SIZE);
	cvp_vol_step_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
	cvp_vol_step_cmd.hdr.dest_port = cvp_handle;
	cvp_vol_step_cmd.hdr.token = 0;
	cvp_vol_step_cmd.hdr.opcode = VSS_IVOLUME_CMD_SET_STEP;
	cvp_vol_step_cmd.cvp_set_vol_step.direction = VSS_IVOLUME_DIRECTION_RX;
	cvp_vol_step_cmd.cvp_set_vol_step.value = v->dev_rx.volume_step_value;
	cvp_vol_step_cmd.cvp_set_vol_step.ramp_duration_ms =
					v->dev_rx.volume_ramp_duration_ms;
	 pr_debug("%s step_value:%d, ramp_duration_ms:%d",
			__func__,
			cvp_vol_step_cmd.cvp_set_vol_step.value,
			cvp_vol_step_cmd.cvp_set_vol_step.ramp_duration_ms);

	v->cvp_state = CMD_STATUS_FAIL;
	ret = apr_send_pkt(apr_cvp, (uint32_t *) &cvp_vol_step_cmd);
	if (ret < 0) {
		pr_err("Fail in sending RX VOL step\n");
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
		cvs_start_record.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
		cvs_start_record.hdr.dest_port = cvs_handle;
		cvs_start_record.hdr.token = 0;
		cvs_start_record.hdr.opcode = VSS_IRECORD_CMD_START;

		cvs_start_record.rec_mode.port_id =
					VSS_IRECORD_PORT_ID_DEFAULT;
		if (rec_mode == VOC_REC_UPLINK) {
			cvs_start_record.rec_mode.rx_tap_point =
					VSS_IRECORD_TAP_POINT_NONE;
			cvs_start_record.rec_mode.tx_tap_point =
					VSS_IRECORD_TAP_POINT_STREAM_END;
		} else if (rec_mode == VOC_REC_DOWNLINK) {
			cvs_start_record.rec_mode.rx_tap_point =
					VSS_IRECORD_TAP_POINT_STREAM_END;
			cvs_start_record.rec_mode.tx_tap_point =
					VSS_IRECORD_TAP_POINT_NONE;
		} else if (rec_mode == VOC_REC_BOTH) {
			cvs_start_record.rec_mode.rx_tap_point =
					VSS_IRECORD_TAP_POINT_STREAM_END;
			cvs_start_record.rec_mode.tx_tap_point =
					VSS_IRECORD_TAP_POINT_STREAM_END;
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
		cvs_stop_record.src_port =
				voice_get_idx_for_session(v->session_id);
		cvs_stop_record.dest_port = cvs_handle;
		cvs_stop_record.token = 0;
		cvs_stop_record.opcode = VSS_IRECORD_CMD_STOP;

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

int voc_start_record(uint32_t port_id, uint32_t set, uint32_t session_id)
{
	int ret = 0;
	int rec_mode = 0;
	u16 cvs_handle;
	int rec_set = 0;
	struct voice_session_itr itr;
	struct voice_data *v = NULL;

	/* check if session_id is valid */
	if (!voice_is_valid_session_id(session_id)) {
		pr_err("%s: Invalid session id:%u\n", __func__,
		       session_id);

		return -EINVAL;
	}

	voice_itr_init(&itr, session_id);
	pr_debug("%s: session_id:%u\n", __func__, session_id);

	while (voice_itr_get_next_session(&itr, &v)) {
		if (v == NULL) {
			pr_err("%s: v is NULL, sessionid:%u\n", __func__,
				session_id);

			break;
		}
		pr_debug("%s: port_id: %d, set: %d, v: %p\n",
			 __func__, port_id, set, v);

		mutex_lock(&v->lock);
		rec_mode = v->rec_info.rec_mode;
		rec_set = set;
		if (set) {
			if ((v->rec_route_state.ul_flag != 0) &&
				(v->rec_route_state.dl_flag != 0)) {
				pr_debug("%s: rec mode already set.\n",
					__func__);

				mutex_unlock(&v->lock);
				continue;
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
				pr_debug("%s: rec already stops.\n",
					__func__);
				mutex_unlock(&v->lock);
				continue;
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
		pr_debug("%s: mode =%d, set =%d\n", __func__,
			 rec_mode, rec_set);
		cvs_handle = voice_get_cvs_handle(v);

		if (cvs_handle != 0) {
			if (rec_set)
				ret = voice_cvs_start_record(v, rec_mode);
			else
				ret = voice_cvs_stop_record(v);
		}

		/* During SRVCC, recording will switch from VoLTE session to
		   voice session.
		   Then stop recording, need to stop recording on voice session.
		 */
		if ((!rec_set) && common.srvcc_rec_flag) {
			pr_debug("%s, srvcc_rec_flag:%d\n",  __func__,
				 common.srvcc_rec_flag);

			voice_cvs_stop_record(&common.voice[VOC_PATH_PASSIVE]);
			common.srvcc_rec_flag = false;
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
	struct cvs_start_playback_cmd cvs_start_playback;
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
		cvs_start_playback.hdr.hdr_field = APR_HDR_FIELD(
					APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);
		cvs_start_playback.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvs_start_playback) - APR_HDR_SIZE);
		cvs_start_playback.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
		cvs_start_playback.hdr.dest_port = cvs_handle;
		cvs_start_playback.hdr.token = 0;
		cvs_start_playback.hdr.opcode = VSS_IPLAYBACK_CMD_START;
		cvs_start_playback.playback_mode.port_id =
						v->music_info.port_id;

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
		cvs_stop_playback.src_port =
				voice_get_idx_for_session(v->session_id);
		cvs_stop_playback.dest_port = cvs_handle;
		cvs_stop_playback.token = 0;

		cvs_stop_playback.opcode = VSS_IPLAYBACK_CMD_STOP;

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

int voc_start_playback(uint32_t set, uint16_t port_id)
{
	int ret = 0;
	u16 cvs_handle;

	struct voice_data *v = NULL;

	if (port_id == VOICE_PLAYBACK_TX)
		v = voice_get_session(voc_get_session_id(VOICE_SESSION_NAME));
	else if (port_id == VOICE2_PLAYBACK_TX)
		v = voice_get_session(voc_get_session_id(VOICE2_SESSION_NAME));

	if (v != NULL) {
		mutex_lock(&v->lock);
		v->music_info.port_id = port_id;
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
	} else {
		pr_err("%s: Invalid port_id 0x%x", __func__, port_id);
	}

	return ret;
}

int voc_disable_cvp(uint32_t session_id)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	if (v->voc_state == VOC_RUN) {
		rtac_remove_voice(voice_get_cvs_handle(v));
		/* send cmd to dsp to disable vocproc */
		ret = voice_send_disable_vocproc_cmd(v);
		if (ret < 0) {
			pr_err("%s:  disable vocproc failed\n", __func__);
			goto fail;
		}

		voice_send_cvp_deregister_vol_cal_cmd(v);
		voice_send_cvp_deregister_cal_cmd(v);
		voice_send_cvp_deregister_dev_cfg_cmd(v);

		v->voc_state = VOC_CHANGE;
	}

fail:	mutex_unlock(&v->lock);

	return ret;
}

int voc_enable_cvp(uint32_t session_id)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: Invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	if (v->voc_state == VOC_CHANGE) {
		ret = voice_send_set_device_cmd(v);
		if (ret < 0) {
			pr_err("%s:  Set device failed\n", __func__);
			goto fail;
		}

		voice_send_cvp_register_dev_cfg_cmd(v);
		voice_send_cvp_register_cal_cmd(v);
		voice_send_cvp_register_vol_cal_cmd(v);

		if (v->lch_mode == VOICE_LCH_START) {
			pr_debug("%s: TX and RX mute ON\n", __func__);

			voice_send_device_mute_cmd(v,
						VSS_IVOLUME_DIRECTION_TX,
						VSS_IVOLUME_MUTE_ON,
						DEFAULT_MUTE_RAMP_DURATION);
			voice_send_device_mute_cmd(v,
						VSS_IVOLUME_DIRECTION_RX,
						VSS_IVOLUME_MUTE_ON,
						DEFAULT_MUTE_RAMP_DURATION);
			/* Send unmute cmd as the TX stream
			 * might be muted previously
			 */
			voice_send_stream_mute_cmd(v,
						VSS_IVOLUME_DIRECTION_TX,
						VSS_IVOLUME_MUTE_OFF,
						DEFAULT_MUTE_RAMP_DURATION);
		} else if (v->lch_mode == VOICE_LCH_STOP) {
			pr_debug("%s: TX and RX mute OFF\n", __func__);

			voice_send_device_mute_cmd(v,
						VSS_IVOLUME_DIRECTION_TX,
						VSS_IVOLUME_MUTE_OFF,
						DEFAULT_MUTE_RAMP_DURATION);
			voice_send_device_mute_cmd(v,
						VSS_IVOLUME_DIRECTION_RX,
						VSS_IVOLUME_MUTE_OFF,
						DEFAULT_MUTE_RAMP_DURATION);
			/* Reset lch mode when VOICE_LCH_STOP is recieved */
			v->lch_mode = 0;
			/* Apply cached mute setting */
			voice_send_stream_mute_cmd(v,
				VSS_IVOLUME_DIRECTION_TX,
				v->stream_tx.stream_mute,
				v->stream_tx.stream_mute_ramp_duration_ms);
		} else {
			pr_debug("%s: Mute commands not sent for lch_mode=%d\n",
				 __func__, v->lch_mode);
		}

		ret = voice_send_enable_vocproc_cmd(v);
		if (ret < 0) {
			pr_err("%s: Enable vocproc failed %d\n", __func__, ret);

			goto fail;
		}

		/* Send tty mode if tty device is used */
		voice_send_tty_mode_cmd(v);
		/* enable slowtalk */
		if (v->st_enable)
			voice_send_set_pp_enable_cmd(v,
					     MODULE_ID_VOICE_MODULE_ST,
					     v->st_enable);
		rtac_add_voice(voice_get_cvs_handle(v),
			voice_get_cvp_handle(v),
			v->dev_rx.port_id, v->dev_tx.port_id,
			v->session_id);
		v->voc_state = VOC_RUN;
	}

fail:
	mutex_unlock(&v->lock);
	return ret;
}

static int voice_set_packet_exchange_mode_and_config(uint32_t session_id,
						 uint32_t mode)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);
		return -EINVAL;
	}

	if (v->voc_state != VOC_RUN)
		ret = voice_send_cvs_data_exchange_mode_cmd(v);

	if (ret) {
		pr_err("%s: Error voice_send_data_exchange_mode_cmd %d\n",
			__func__, ret);
		goto fail;
	}

	ret = voice_send_cvs_packet_exchange_config_cmd(v);
	if (ret) {
		pr_err("%s: Error: voice_send_packet_exchange_config_cmd %d\n",
			__func__, ret);
		goto fail;
	}

	return ret;
fail:
	return -EINVAL;
}

int voc_set_tx_mute(uint32_t session_id, uint32_t dir, uint32_t mute,
		    uint32_t ramp_duration)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	voice_itr_init(&itr, session_id);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			v->stream_tx.stream_mute = mute;
			v->stream_tx.stream_mute_ramp_duration_ms =
								ramp_duration;
			if (is_voc_state_active(v->voc_state) &&
				(v->lch_mode == 0))
				ret = voice_send_stream_mute_cmd(v,
				VSS_IVOLUME_DIRECTION_TX,
				v->stream_tx.stream_mute,
				v->stream_tx.stream_mute_ramp_duration_ms);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session_id 0x%x\n", __func__,
				session_id);

			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

int voc_set_rx_device_mute(uint32_t session_id, uint32_t mute,
					uint32_t ramp_duration)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	voice_itr_init(&itr, session_id);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			v->dev_rx.dev_mute = mute;
			v->dev_rx.dev_mute_ramp_duration_ms =
							ramp_duration;
			if (((v->voc_state == VOC_RUN) ||
				(v->voc_state == VOC_STANDBY)) &&
				(v->lch_mode == 0))
				ret = voice_send_device_mute_cmd(v,
						VSS_IVOLUME_DIRECTION_RX,
						v->dev_rx.dev_mute,
						ramp_duration);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session_id 0x%x\n", __func__,
				session_id);

			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

int voc_get_rx_device_mute(uint32_t session_id)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	ret = v->dev_rx.dev_mute;

	mutex_unlock(&v->lock);

	return ret;
}

int voc_set_tty_mode(uint32_t session_id, uint8_t tty_mode)
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

uint8_t voc_get_tty_mode(uint32_t session_id)
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

int voc_set_pp_enable(uint32_t session_id, uint32_t module_id, uint32_t enable)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	voice_itr_init(&itr, session_id);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			if (!(is_voice_app_id(v->session_id) ||
			      is_volte_session(v->session_id)))
				continue;

			mutex_lock(&v->lock);
			if (module_id == MODULE_ID_VOICE_MODULE_ST)
				v->st_enable = enable;

			if (v->voc_state == VOC_RUN) {
				if (module_id ==
				    MODULE_ID_VOICE_MODULE_ST)
					ret = voice_send_set_pp_enable_cmd(v,
						MODULE_ID_VOICE_MODULE_ST,
						enable);
			}
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session_id 0x%x\n", __func__,
								session_id);
			ret =  -EINVAL;
			break;
		}
	}

	return ret;
}

int voc_get_pp_enable(uint32_t session_id, uint32_t module_id)
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
	mutex_unlock(&v->lock);

	return ret;
}

int voc_set_rx_vol_step(uint32_t session_id, uint32_t dir, uint32_t vol_step,
			uint32_t ramp_duration)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	pr_debug("%s session id = %#x vol = %u", __func__, session_id,
		vol_step);

	voice_itr_init(&itr, session_id);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			v->dev_rx.volume_step_value = vol_step;
			v->dev_rx.volume_ramp_duration_ms = ramp_duration;
			if (is_voc_state_active(v->voc_state))
				ret = voice_send_vol_step_cmd(v);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session_id 0x%x\n", __func__,
				session_id);

			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

int voc_set_rxtx_port(uint32_t session_id, uint32_t port_id, uint32_t dev_type)
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

int voc_set_route_flag(uint32_t session_id, uint8_t path_dir, uint8_t set)
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

uint8_t voc_get_route_flag(uint32_t session_id, uint8_t path_dir)
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

int voc_end_voice_call(uint32_t session_id)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	if (v->voc_state == VOC_RUN || v->voc_state == VOC_ERROR ||
	    v->voc_state == VOC_CHANGE || v->voc_state == VOC_STANDBY) {

		pr_debug("%s: VOC_STATE: %d\n", __func__, v->voc_state);

		ret = voice_destroy_vocproc(v);
		if (ret < 0)
			pr_err("%s:  destroy voice failed\n", __func__);
		voice_destroy_mvm_cvs_session(v);

		v->voc_state = VOC_RELEASE;
	} else {
		pr_err("%s: Error: End voice called in state %d\n",
			__func__, v->voc_state);

		ret = -EINVAL;
	}

	mutex_unlock(&v->lock);
	return ret;
}

int voc_standby_voice_call(uint32_t session_id)
{
	struct voice_data *v = voice_get_session(session_id);
	struct apr_hdr mvm_standby_voice_cmd;
	void *apr_mvm;
	u16 mvm_handle;
	int ret = 0;

	pr_debug("%s: voc state=%d", __func__, v->voc_state);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}
	if (v->voc_state == VOC_RUN) {
		apr_mvm = common.apr_q6_mvm;
		if (!apr_mvm) {
			pr_err("%s: apr_mvm is NULL.\n", __func__);
			ret = -EINVAL;
			goto fail;
		}
		mvm_handle = voice_get_mvm_handle(v);
		mvm_standby_voice_cmd.hdr_field =
			APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		mvm_standby_voice_cmd.pkt_size =
			APR_PKT_SIZE(APR_HDR_SIZE,
			sizeof(mvm_standby_voice_cmd) - APR_HDR_SIZE);
		pr_debug("send mvm_standby_voice_cmd pkt size = %d\n",
			 mvm_standby_voice_cmd.pkt_size);
		mvm_standby_voice_cmd.src_port =
				voice_get_idx_for_session(v->session_id);
		mvm_standby_voice_cmd.dest_port = mvm_handle;
		mvm_standby_voice_cmd.token = 0;
		mvm_standby_voice_cmd.opcode = VSS_IMVM_CMD_STANDBY_VOICE;
		v->mvm_state = CMD_STATUS_FAIL;
		ret = apr_send_pkt(apr_mvm,
				(uint32_t *)&mvm_standby_voice_cmd);
		if (ret < 0) {
			pr_err("Fail in sending VSS_IMVM_CMD_STANDBY_VOICE\n");
			ret = -EINVAL;
			goto fail;
		}
		v->voc_state = VOC_STANDBY;
	}
fail:
	return ret;
}

int voc_set_lch(uint32_t session_id, enum voice_lch_mode lch_mode)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: Invalid session_id 0x%x\n", __func__, session_id);

		ret = -EINVAL;
		goto done;
	}

	mutex_lock(&v->lock);
	if (v->lch_mode == lch_mode) {
		pr_debug("%s: Session %d already in LCH mode %d\n",
				 __func__, session_id, lch_mode);

		mutex_unlock(&v->lock);
		goto done;
	}

	v->lch_mode = lch_mode;
	mutex_unlock(&v->lock);

	ret = voc_disable_cvp(session_id);
	if (ret < 0) {
		pr_err("%s: voc_disable_cvp failed ret=%d\n", __func__, ret);

		goto done;
	}

	/* Mute and topology_none will be set as part of voc_enable_cvp() */
	ret = voc_enable_cvp(session_id);
	if (ret < 0) {
		pr_err("%s: voc_enable_cvp failed ret=%d\n", __func__, ret);

		goto done;
	}

done:
	return ret;
}

int voc_resume_voice_call(uint32_t session_id)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	ret = voice_send_start_voice_cmd(v);
	if (ret < 0) {
		pr_err("Fail in sending START_VOICE\n");
		goto fail;
	}
	v->voc_state = VOC_RUN;
	return 0;
fail:
	return -EINVAL;
}

int voc_start_voice_call(uint32_t session_id)
{
	struct voice_data *v = voice_get_session(session_id);
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: invalid session_id 0x%x\n", __func__, session_id);

		return -EINVAL;
	}

	mutex_lock(&v->lock);

	if (v->voc_state == VOC_ERROR) {
		pr_debug("%s: VOC in ERR state\n", __func__);

		voice_destroy_mvm_cvs_session(v);
		v->voc_state = VOC_INIT;
	}

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

		/* Allocate cal mem if not already allocated and memory map
		 * the calibration memory block.
		 */
		ret = voice_alloc_and_map_cal_mem(v);
		if (ret < 0) {
			pr_debug("%s: Continue without calibration %d\n",
				 __func__, ret);
		}

		if (is_voip_session(session_id)) {
			/* Allocate oob mem if not already allocated and
			 * memory map the oob memory block.
			 */
			ret = voice_alloc_and_map_oob_mem(v);
			if (ret < 0) {
				pr_err("%s: voice_alloc_and_map_oob_mem() failed, ret:%d\n",
				       __func__, ret);

				goto fail;
			}

			ret = voice_set_packet_exchange_mode_and_config(
				session_id,
				VSS_ISTREAM_PACKET_EXCHANGE_MODE_OUT_OF_BAND);
			if (ret) {
				pr_err("%s: Err: exchange_mode_and_config  %d\n",
					__func__, ret);

				goto fail;
			}
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

		ret = voice_send_vol_step_cmd(v);
		if (ret < 0)
			pr_err("voice volume failed\n");

		ret = voice_send_stream_mute_cmd(v,
				VSS_IVOLUME_DIRECTION_TX,
				v->stream_tx.stream_mute,
				v->stream_tx.stream_mute_ramp_duration_ms);
		if (ret < 0)
			pr_err("voice mute failed\n");

		ret = voice_send_start_voice_cmd(v);
		if (ret < 0) {
			pr_err("start voice failed\n");
			goto fail;
		}

		v->voc_state = VOC_RUN;
	} else {
		pr_err("%s: Error: Start voice called in state %d\n",
			__func__, v->voc_state);

		ret = -EINVAL;
		goto fail;
	}
fail:
	mutex_unlock(&v->lock);
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

void voc_register_dtmf_rx_detection_cb(dtmf_rx_det_cb_fn dtmf_rx_ul_cb,
				       void *private_data)
{
	common.dtmf_info.dtmf_rx_ul_cb = dtmf_rx_ul_cb;
	common.dtmf_info.private_data = private_data;
}

void voc_config_vocoder(uint32_t media_type,
			uint32_t rate,
			uint32_t network_type,
			uint32_t dtx_mode,
			uint32_t evrc_min_rate,
			uint32_t evrc_max_rate)
{
	common.mvs_info.media_type = media_type;
	common.mvs_info.rate = rate;
	common.mvs_info.network_type = network_type;
	common.mvs_info.dtx_mode = dtx_mode;
	common.mvs_info.evrc_min_rate = evrc_min_rate;
	common.mvs_info.evrc_max_rate = evrc_max_rate;
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

	pr_debug("%s: Payload Length = %d, opcode=%x\n", __func__,
		data->payload_size, data->opcode);

	if (data->opcode == RESET_EVENTS) {

		if (data->reset_proc == APR_DEST_MODEM) {
			pr_debug("%s: Received MODEM reset event\n", __func__);

		} else {
			pr_debug("%s: Reset event received in Voice service\n",
				__func__);
			apr_reset(c->apr_q6_mvm);
			c->apr_q6_mvm = NULL;

			/* clean up memory handle */
			c->cal_mem_handle = 0;
			c->rtac_mem_handle = 0;
			rtac_clear_mapping(VOICE_RTAC_CAL);

			/* Sub-system restart is applicable to all sessions. */
			for (i = 0; i < MAX_VOC_SESSIONS; i++) {
				c->voice[i].mvm_handle = 0;
				c->voice[i].shmem_info.mem_handle = 0;
			}
		}
		/* clean up srvcc rec flag */
		c->srvcc_rec_flag = false;
		voc_set_error_state(data->reset_proc);
		return 0;
	}

	pr_debug("%s: session_id 0x%x\n", __func__, data->dest_port);

	v = voice_get_session_by_idx(data->dest_port);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		return -EINVAL;
	}

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		if (data->payload_size) {
			ptr = data->payload;

			pr_debug("%x %x\n", ptr[0], ptr[1]);
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
			case VSS_IMVM_CMD_SET_POLICY_DUAL_CONTROL:
			case VSS_IMVM_CMD_SET_CAL_NETWORK:
			case VSS_IMVM_CMD_SET_CAL_MEDIA_TYPE:
			case VSS_IMEMORY_CMD_MAP_PHYSICAL:
			case VSS_IMEMORY_CMD_UNMAP:
			case VSS_IMVM_CMD_PAUSE_VOICE:
			case VSS_IMVM_CMD_STANDBY_VOICE:
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
	} else if (data->opcode == VSS_IMEMORY_RSP_MAP) {
		pr_debug("%s, Revd VSS_IMEMORY_RSP_MAP response\n", __func__);

		if (data->payload_size && data->token == VOIP_MEM_MAP_TOKEN) {
			ptr = data->payload;
			if (ptr[0]) {
				v->shmem_info.mem_handle = ptr[0];
				pr_debug("%s: shared mem_handle: 0x[%x]\n",
					 __func__, v->shmem_info.mem_handle);
				v->mvm_state = CMD_STATUS_SUCCESS;
				wake_up(&v->mvm_wait);
			}
		} else if (data->payload_size &&
			   data->token == VOC_CAL_MEM_MAP_TOKEN) {
			ptr = data->payload;
			if (ptr[0]) {
				c->cal_mem_handle = ptr[0];

				pr_debug("%s: cal mem handle 0x%x\n",
					 __func__, c->cal_mem_handle);

				v->mvm_state = CMD_STATUS_SUCCESS;
				wake_up(&v->mvm_wait);
			}
		} else if (data->payload_size &&
				data->token == VOC_RTAC_MEM_MAP_TOKEN) {
			ptr = data->payload;
			if (ptr[0]) {
				c->rtac_mem_handle = ptr[0];

				pr_debug("%s: cal mem handle 0x%x\n",
					 __func__, c->rtac_mem_handle);

				v->mvm_state = CMD_STATUS_SUCCESS;
				wake_up(&v->mvm_wait);
			}
		} else {
			pr_err("%s: Unknown mem map token %d\n",
			       __func__, data->token);
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
	pr_debug("%s: Payload Length = %d, opcode=%x\n", __func__,
		data->payload_size, data->opcode);

	if (data->opcode == RESET_EVENTS) {
		if (data->reset_proc == APR_DEST_MODEM) {
			pr_debug("%s: Received Modem reset event\n", __func__);

		} else {
			pr_debug("%s: Reset event received in Voice service\n",
				 __func__);

			apr_reset(c->apr_q6_cvs);
			c->apr_q6_cvs = NULL;

			/* Sub-system restart is applicable to all sessions. */
			for (i = 0; i < MAX_VOC_SESSIONS; i++)
				c->voice[i].cvs_handle = 0;
		}

		voc_set_error_state(data->reset_proc);
		return 0;
	}

	v = voice_get_session_by_idx(data->dest_port);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		return -EINVAL;
	}

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		if (data->payload_size) {
			ptr = data->payload;

			pr_debug("%x %x\n", ptr[0], ptr[1]);
			if (ptr[1] != 0) {
				pr_err("%s: cmd = 0x%x returned error = 0x%x\n",
					__func__, ptr[0], ptr[1]);
			}
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
			case VSS_IVOLUME_CMD_MUTE_V2:
			case VSS_ISTREAM_CMD_SET_MEDIA_TYPE:
			case VSS_ISTREAM_CMD_VOC_AMR_SET_ENC_RATE:
			case VSS_ISTREAM_CMD_VOC_AMRWB_SET_ENC_RATE:
			case VSS_ISTREAM_CMD_SET_ENC_DTX_MODE:
			case VSS_ISTREAM_CMD_CDMA_SET_ENC_MINMAX_RATE:
			case APRV2_IBASIC_CMD_DESTROY_SESSION:
			case VSS_ISTREAM_CMD_REGISTER_CALIBRATION_DATA_V2:
			case VSS_ISTREAM_CMD_DEREGISTER_CALIBRATION_DATA:
			case VSS_ICOMMON_CMD_MAP_MEMORY:
			case VSS_ICOMMON_CMD_UNMAP_MEMORY:
			case VSS_ICOMMON_CMD_SET_UI_PROPERTY:
			case VSS_IPLAYBACK_CMD_START:
			case VSS_IPLAYBACK_CMD_STOP:
			case VSS_IRECORD_CMD_START:
			case VSS_IRECORD_CMD_STOP:
			case VSS_ISTREAM_CMD_SET_PACKET_EXCHANGE_MODE:
			case VSS_ISTREAM_CMD_SET_OOB_PACKET_EXCHANGE_CONFIG:
			case VSS_ISTREAM_CMD_SET_RX_DTMF_DETECTION:
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				v->cvs_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvs_wait);
				break;
			case VOICE_CMD_SET_PARAM:
				pr_debug("%s: VOICE_CMD_SET_PARAM\n", __func__);
				rtac_make_voice_callback(RTAC_CVS, ptr,
							data->payload_size);
				break;
			case VOICE_CMD_GET_PARAM:
				pr_debug("%s: VOICE_CMD_GET_PARAM\n",
					__func__);
				/* Should only come here if there is an APR */
				/* error or malformed APR packet. Otherwise */
				/* response will be returned as */
				/* VOICE_EVT_GET_PARAM_ACK */
				if (ptr[1] != 0) {
					pr_err("%s: CVP get param error = %d, resuming\n",
						__func__, ptr[1]);
					rtac_make_voice_callback(RTAC_CVP,
						data->payload,
						data->payload_size);
				}
				break;
			default:
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				break;
			}
		}
	} else if (data->opcode ==
			 VSS_ISTREAM_EVT_OOB_NOTIFY_ENC_BUFFER_READY) {
		int ret = 0;
		u16 cvs_handle;
		uint32_t *cvs_voc_pkt;
		struct cvs_enc_buffer_consumed_cmd send_enc_buf_consumed_cmd;
		void *apr_cvs;

		pr_debug("Encoder buffer is ready\n");

		apr_cvs = common.apr_q6_cvs;
		if (!apr_cvs) {
			pr_err("%s: apr_cvs is NULL\n", __func__);
			return -EINVAL;
		}
		cvs_handle = voice_get_cvs_handle(v);

		send_enc_buf_consumed_cmd.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
		send_enc_buf_consumed_cmd.hdr.pkt_size =
			APR_PKT_SIZE(APR_HDR_SIZE,
			sizeof(send_enc_buf_consumed_cmd) - APR_HDR_SIZE);

		send_enc_buf_consumed_cmd.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
		send_enc_buf_consumed_cmd.hdr.dest_port = cvs_handle;
		send_enc_buf_consumed_cmd.hdr.token = 0;
		send_enc_buf_consumed_cmd.hdr.opcode =
			VSS_ISTREAM_EVT_OOB_NOTIFY_ENC_BUFFER_CONSUMED;

		cvs_voc_pkt = v->shmem_info.sh_buf.buf[1].data;
		if (cvs_voc_pkt != NULL &&  common.mvs_info.ul_cb != NULL) {
			/* cvs_voc_pkt[0] contains tx timestamp */
			common.mvs_info.ul_cb((uint8_t *)&cvs_voc_pkt[3],
					      cvs_voc_pkt[2],
					      cvs_voc_pkt[0],
					      common.mvs_info.private_data);
		} else
			pr_err("%s: cvs_voc_pkt or ul_cb is NULL\n", __func__);

		ret = apr_send_pkt(apr_cvs,
			(uint32_t *) &send_enc_buf_consumed_cmd);
		if (ret < 0) {
			pr_err("%s: Err send ENC_BUF_CONSUMED_NOTIFY %d\n",
				__func__, ret);
			goto fail;
		}
	} else if (data->opcode == VSS_ISTREAM_EVT_SEND_ENC_BUFFER) {
		pr_debug("Recd VSS_ISTREAM_EVT_SEND_ENC_BUFFER\n");
	} else if (data->opcode ==
			 VSS_ISTREAM_EVT_OOB_NOTIFY_DEC_BUFFER_REQUEST) {
		int ret = 0;
		u16 cvs_handle;
		uint32_t *cvs_voc_pkt;
		struct cvs_dec_buffer_ready_cmd send_dec_buf;
		void *apr_cvs;
		apr_cvs = common.apr_q6_cvs;

		if (!apr_cvs) {
			pr_err("%s: apr_cvs is NULL\n", __func__);
			return -EINVAL;
		}
		cvs_handle = voice_get_cvs_handle(v);

		send_dec_buf.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);

		send_dec_buf.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(send_dec_buf) - APR_HDR_SIZE);

		send_dec_buf.hdr.src_port =
				voice_get_idx_for_session(v->session_id);
		send_dec_buf.hdr.dest_port = cvs_handle;
		send_dec_buf.hdr.token = 0;
		send_dec_buf.hdr.opcode =
				 VSS_ISTREAM_EVT_OOB_NOTIFY_DEC_BUFFER_READY;

		cvs_voc_pkt = (uint32_t *)(v->shmem_info.sh_buf.buf[0].data);
		if (cvs_voc_pkt != NULL && common.mvs_info.dl_cb != NULL) {
			/* Set timestamp to 0 and advance the pointer */
			cvs_voc_pkt[0] = 0;
			/* Set media_type and advance the pointer */
			cvs_voc_pkt[1] = common.mvs_info.media_type;
			common.mvs_info.dl_cb(
					      (uint8_t *)&cvs_voc_pkt[2],
					      common.mvs_info.private_data);
			ret = apr_send_pkt(apr_cvs, (uint32_t *) &send_dec_buf);
			if (ret < 0) {
				pr_err("%s: Err send DEC_BUF_READY_NOTIFI %d\n",
					__func__, ret);
				goto fail;
			}
		} else {
			pr_debug("%s: voc_pkt or dl_cb is NULL\n", __func__);
			goto fail;
		}
	} else if (data->opcode == VSS_ISTREAM_EVT_REQUEST_DEC_BUFFER) {
		pr_debug("Recd VSS_ISTREAM_EVT_REQUEST_DEC_BUFFER\n");
	} else if (data->opcode == VSS_ISTREAM_EVT_SEND_DEC_BUFFER) {
		pr_debug("Send dec buf resp\n");
	} else if (data->opcode == APR_RSP_ACCEPTED) {
		ptr = data->payload;
		if (ptr[0])
			pr_debug("%s: APR_RSP_ACCEPTED for 0x%x:\n",
				 __func__, ptr[0]);
	} else if (data->opcode == VSS_ISTREAM_EVT_NOT_READY) {
		pr_debug("Recd VSS_ISTREAM_EVT_NOT_READY\n");
	} else if (data->opcode == VSS_ISTREAM_EVT_READY) {
		pr_debug("Recd VSS_ISTREAM_EVT_READY\n");
	} else if (data->opcode ==  VOICE_EVT_GET_PARAM_ACK) {
		pr_debug("%s: VOICE_EVT_GET_PARAM_ACK\n", __func__);
		ptr = data->payload;
		if (ptr[0] != 0) {
			pr_err("%s: VOICE_EVT_GET_PARAM_ACK returned error = 0x%x\n",
				__func__, ptr[0]);
		}
		rtac_make_voice_callback(RTAC_CVS, data->payload,
					data->payload_size);
	}  else if (data->opcode == VSS_ISTREAM_EVT_RX_DTMF_DETECTED) {
		struct vss_istream_evt_rx_dtmf_detected *dtmf_rx_detected;
		uint32_t *voc_pkt = data->payload;
		uint32_t pkt_len = data->payload_size;

		if ((voc_pkt != NULL) &&
		    (pkt_len ==
			sizeof(struct vss_istream_evt_rx_dtmf_detected))) {

			dtmf_rx_detected =
			(struct vss_istream_evt_rx_dtmf_detected *) voc_pkt;
			pr_debug("RX_DTMF_DETECTED low_freq=%d high_freq=%d\n",
				 dtmf_rx_detected->low_freq,
				 dtmf_rx_detected->high_freq);
			if (c->dtmf_info.dtmf_rx_ul_cb)
				c->dtmf_info.dtmf_rx_ul_cb((uint8_t *)voc_pkt,
					voc_get_session_name(v->session_id),
					c->dtmf_info.private_data);
		} else {
			pr_err("Invalid packet\n");
		}
	}  else
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

	if (data->opcode == RESET_EVENTS) {
		if (data->reset_proc == APR_DEST_MODEM) {
			pr_debug("%s: Received Modem reset event\n", __func__);

		} else {
			pr_debug("%s: Reset event received in Voice service\n",
				 __func__);

			apr_reset(c->apr_q6_cvp);
			c->apr_q6_cvp = NULL;

			/* Sub-system restart is applicable to all sessions. */
			for (i = 0; i < MAX_VOC_SESSIONS; i++)
				c->voice[i].cvp_handle = 0;
		}

		voc_set_error_state(data->reset_proc);
		return 0;
	}

	v = voice_get_session_by_idx(data->dest_port);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		return -EINVAL;
	}

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		if (data->payload_size) {
			ptr = data->payload;

			pr_debug("%x %x\n", ptr[0], ptr[1]);
			if (ptr[1] != 0) {
				pr_err("%s: cmd = 0x%x returned error = 0x%x\n",
					__func__, ptr[0], ptr[1]);
			}
			switch (ptr[0]) {
			case VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION_V2:
			/*response from  CVP */
				pr_debug("%s: cmd = 0x%x\n", __func__, ptr[0]);
				if (!ptr[1]) {
					voice_set_cvp_handle(v, data->src_port);
					pr_debug("status: %d, cvphdl=%d\n",
						 ptr[1], data->src_port);
				} else
					pr_err("got NACK from CVP create session response\n");
				v->cvp_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvp_wait);
				break;
			case VSS_IVOCPROC_CMD_SET_DEVICE_V2:
			case VSS_IVOLUME_CMD_SET_STEP:
			case VSS_IVOCPROC_CMD_ENABLE:
			case VSS_IVOCPROC_CMD_DISABLE:
			case APRV2_IBASIC_CMD_DESTROY_SESSION:
			case VSS_IVOCPROC_CMD_REGISTER_VOL_CALIBRATION_DATA:
			case VSS_IVOCPROC_CMD_DEREGISTER_VOL_CALIBRATION_DATA:
			case VSS_IVOCPROC_CMD_REGISTER_CALIBRATION_DATA_V2:
			case VSS_IVOCPROC_CMD_DEREGISTER_CALIBRATION_DATA:
			case VSS_IVOCPROC_CMD_REGISTER_DEVICE_CONFIG:
			case VSS_IVOCPROC_CMD_DEREGISTER_DEVICE_CONFIG:
			case VSS_ICOMMON_CMD_MAP_MEMORY:
			case VSS_ICOMMON_CMD_UNMAP_MEMORY:
			case VSS_IVOLUME_CMD_MUTE_V2:
				v->cvp_state = CMD_STATUS_SUCCESS;
				wake_up(&v->cvp_wait);
				break;
			case VOICE_CMD_SET_PARAM:
				pr_debug("%s: VOICE_CMD_SET_PARAM\n", __func__);
				rtac_make_voice_callback(RTAC_CVP, ptr,
							data->payload_size);
				break;
			case VOICE_CMD_GET_PARAM:
				pr_debug("%s: VOICE_CMD_GET_PARAM\n",
					__func__);
				/* Should only come here if there is an APR */
				/* error or malformed APR packet. Otherwise */
				/* response will be returned as */
				/* VOICE_EVT_GET_PARAM_ACK */
				if (ptr[1] != 0) {
					pr_err("%s: CVP get param error = %d, resuming\n",
						__func__, ptr[1]);
					rtac_make_voice_callback(RTAC_CVP,
						data->payload,
						data->payload_size);
				}
				break;
			default:
				pr_debug("%s: not match cmd = 0x%x\n",
					__func__, ptr[0]);
				break;
			}
		}
	} else if (data->opcode ==  VOICE_EVT_GET_PARAM_ACK) {
		pr_debug("%s: VOICE_EVT_GET_PARAM_ACK\n", __func__);
		ptr = data->payload;
		if (ptr[0] != 0) {
			pr_err("%s: VOICE_EVT_GET_PARAM_ACK returned error = 0x%x\n",
				__func__, ptr[0]);
		}
		rtac_make_voice_callback(RTAC_CVP, data->payload,
			data->payload_size);
	}
	return 0;
}

static int voice_free_oob_shared_mem(void)
{
	int rc = 0;
	int cnt = 0;
	int bufcnt = NUM_OF_BUFFERS;
	struct voice_data *v = voice_get_session(
				common.voice[VOC_PATH_FULL].session_id);

	mutex_lock(&common.common_lock);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		rc = -EINVAL;
		goto done;
	}

	rc = msm_audio_ion_free(v->shmem_info.sh_buf.client,
				v->shmem_info.sh_buf.handle);
	v->shmem_info.sh_buf.client = NULL;
	v->shmem_info.sh_buf.handle = NULL;
	if (rc < 0) {
		pr_err("%s: Error:%d freeing memory\n", __func__, rc);

		goto done;
	}


	while (cnt < bufcnt) {
		v->shmem_info.sh_buf.buf[cnt].data =  NULL;
		v->shmem_info.sh_buf.buf[cnt].phys =  0;
		cnt++;
	}

	v->shmem_info.sh_buf.client = NULL;
	v->shmem_info.sh_buf.handle = NULL;

done:
	mutex_unlock(&common.common_lock);
	return rc;
}

static int voice_alloc_oob_shared_mem(void)
{
	int cnt = 0;
	int rc = 0;
	int len;
	void *mem_addr;
	dma_addr_t phys;
	int bufsz = BUFFER_BLOCK_SIZE;
	int bufcnt = NUM_OF_BUFFERS;
	struct voice_data *v = voice_get_session(
				common.voice[VOC_PATH_FULL].session_id);

	mutex_lock(&common.common_lock);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		rc = -EINVAL;
		goto done;
	}

	rc = msm_audio_ion_alloc("voip_client", &(v->shmem_info.sh_buf.client),
			&(v->shmem_info.sh_buf.handle),
			bufsz*bufcnt,
			(ion_phys_addr_t *)&phys, (size_t *)&len,
			&mem_addr);
	if (rc < 0) {
		pr_err("%s: audio ION alloc failed, rc = %d\n",
			__func__, rc);

		goto done;
	}

	while (cnt < bufcnt) {
		v->shmem_info.sh_buf.buf[cnt].data =  mem_addr  + (cnt * bufsz);
		v->shmem_info.sh_buf.buf[cnt].phys =  phys + (cnt * bufsz);
		v->shmem_info.sh_buf.buf[cnt].size = bufsz;
		cnt++;
	}

	pr_debug("%s buf[0].data:[%p], buf[0].phys:[%p], &buf[0].phys:[%p],\n",
		 __func__,
		(void *)v->shmem_info.sh_buf.buf[0].data,
		(void *)v->shmem_info.sh_buf.buf[0].phys,
		(void *)&v->shmem_info.sh_buf.buf[0].phys);
	pr_debug("%s: buf[1].data:[%p], buf[1].phys[%p], &buf[1].phys[%p]\n",
		__func__,
		(void *)v->shmem_info.sh_buf.buf[1].data,
		(void *)v->shmem_info.sh_buf.buf[1].phys,
		(void *)&v->shmem_info.sh_buf.buf[1].phys);

	memset((void *)v->shmem_info.sh_buf.buf[0].data, 0, (bufsz * bufcnt));

done:
	mutex_unlock(&common.common_lock);
	return rc;
}

static int voice_alloc_oob_mem_table(void)
{
	int rc = 0;
	int len;
	struct voice_data *v = voice_get_session(
				common.voice[VOC_PATH_FULL].session_id);

	mutex_lock(&common.common_lock);
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		rc = -EINVAL;
		goto done;
	}

	rc = msm_audio_ion_alloc("voip_client", &(v->shmem_info.memtbl.client),
				&(v->shmem_info.memtbl.handle),
				sizeof(struct vss_imemory_table_t),
				(ion_phys_addr_t *)&v->shmem_info.memtbl.phys,
				(size_t *)&len,
				&(v->shmem_info.memtbl.data));
	if (rc < 0) {
		pr_err("%s: audio ION alloc failed, rc = %d\n",
			__func__, rc);

		goto done;
	}

	v->shmem_info.memtbl.size = sizeof(struct vss_imemory_table_t);
	pr_debug("%s data[%p]phys[%p][%p]\n", __func__,
		 (void *)v->shmem_info.memtbl.data,
		 (void *)v->shmem_info.memtbl.phys,
		 (void *)&v->shmem_info.memtbl.phys);

done:
	mutex_unlock(&common.common_lock);
	return rc;
}

static int voice_alloc_cal_mem_map_table(void)
{
	int ret = 0;
	int len;

	ret = msm_audio_ion_alloc("voc_cal",
				&(common.cal_mem_map_table.client),
				&(common.cal_mem_map_table.handle),
				sizeof(struct vss_imemory_table_t),
			      (ion_phys_addr_t *)&common.cal_mem_map_table.phys,
				(size_t *) &len,
				&(common.cal_mem_map_table.data));
	if (ret < 0) {
		pr_err("%s: audio ION alloc failed, rc = %d\n",
			__func__, ret);
		goto done;
	}

	common.cal_mem_map_table.size = sizeof(struct vss_imemory_table_t);
	pr_debug("%s: data 0x%x phys 0x%x\n", __func__,
		 (unsigned int) common.cal_mem_map_table.data,
		 common.cal_mem_map_table.phys);

done:
	return ret;
}

static int voice_alloc_rtac_mem_map_table(void)
{
	int ret = 0;
	int len;

	ret = msm_audio_ion_alloc("voc_rtac_cal",
			&(common.rtac_mem_map_table.client),
			&(common.rtac_mem_map_table.handle),
			sizeof(struct vss_imemory_table_t),
			(ion_phys_addr_t *)&common.rtac_mem_map_table.phys,
			(size_t *) &len,
			&(common.rtac_mem_map_table.data));
	if (ret < 0) {
		pr_err("%s: audio ION alloc failed, rc = %d\n",
			__func__, ret);
		goto done;
	}

	common.rtac_mem_map_table.size = sizeof(struct vss_imemory_table_t);
	pr_debug("%s: data 0x%x phys 0x%x\n", __func__,
		 (unsigned int) common.rtac_mem_map_table.data,
		 common.rtac_mem_map_table.phys);

done:
	return ret;
}

static int voice_alloc_and_map_cal_mem(struct voice_data *v)
{
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		return -EINVAL;
	}

	ret = voc_alloc_cal_shared_memory();
	if (ret < 0) {
		pr_err("%s: Memory allocation of cal block failed %d\n",
			   __func__, ret);

		goto done;
	}

	/* Memory map the calibration memory block. */
	ret = voice_mem_map_cal_block(v);
	if (ret < 0) {
		pr_err("%s: Memory map of cal block failed %d\n",
			   __func__, ret);
	}

done:
	return ret;
}

static int voice_alloc_and_map_oob_mem(struct voice_data *v)
{
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);

		return -EINVAL;
	}

	if (!is_voip_memory_allocated()) {
		ret = voc_alloc_voip_shared_memory();
		if (ret < 0) {
			pr_err("%s: Failed to create voip oob memory %d\n",
				   __func__, ret);

			goto done;
		}
	}

	ret = voice_map_memory_physical_cmd(v,
			&v->shmem_info.memtbl,
			v->shmem_info.sh_buf.buf[0].phys,
			v->shmem_info.sh_buf.buf[0].size * NUM_OF_BUFFERS,
			VOIP_MEM_MAP_TOKEN);
	if (ret) {
		pr_err("%s: mvm_map_memory_phy failed %d\n",
			   __func__, ret);

		goto done;
	}

done:
	return ret;
}

int is_voc_initialized(void)
{
	return module_initialized;
}

static int __init voice_init(void)
{
	int rc = 0, i = 0;

	memset(&common, 0, sizeof(struct common_data));

	/* set default value */
	common.default_mute_val = 0;  /* default is un-mute */
	common.default_sample_val = 8000;
	common.default_vol_step_val = 0;
	common.default_vol_ramp_duration_ms = DEFAULT_VOLUME_RAMP_DURATION;
	common.default_mute_ramp_duration_ms = DEFAULT_MUTE_RAMP_DURATION;

	/* Initialize MVS info. */
	common.mvs_info.network_type = VSS_NETWORK_ID_DEFAULT;

	mutex_init(&common.common_lock);

	/* Initialize session id with vsid */
	init_session_id();

	for (i = 0; i < MAX_VOC_SESSIONS; i++) {

		/* initialize dev_rx and dev_tx */
		common.voice[i].dev_rx.dev_mute =  common.default_mute_val;
		common.voice[i].dev_tx.dev_mute =  common.default_mute_val;
		common.voice[i].dev_rx.volume_step_value =
					common.default_vol_step_val;
		common.voice[i].dev_rx.volume_ramp_duration_ms =
					common.default_vol_ramp_duration_ms;
		common.voice[i].dev_rx.dev_mute_ramp_duration_ms =
					common.default_mute_ramp_duration_ms;
		common.voice[i].dev_tx.dev_mute_ramp_duration_ms =
					common.default_mute_ramp_duration_ms;
		common.voice[i].stream_rx.stream_mute = common.default_mute_val;
		common.voice[i].stream_tx.stream_mute = common.default_mute_val;

		common.voice[i].dev_tx.port_id = 0x100B;
		common.voice[i].dev_rx.port_id = 0x100A;
		common.voice[i].sidetone_gain = 0x512;
		common.voice[i].dtmf_rx_detect_en = 0;
		common.voice[i].lch_mode = 0;

		common.voice[i].voc_state = VOC_INIT;

		init_waitqueue_head(&common.voice[i].mvm_wait);
		init_waitqueue_head(&common.voice[i].cvs_wait);
		init_waitqueue_head(&common.voice[i].cvp_wait);

		mutex_init(&common.voice[i].lock);
	}

	if (rc == 0)
		module_initialized = true;

	pr_debug("%s: rc=%d\n", __func__, rc);
	return rc;
}

device_initcall(voice_init);
