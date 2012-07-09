/*
 * Common code to deal with the AUDPREPROC dsp task (audio preprocessing)
 *
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * Based on the audpp layer in arch/arm/mach-msm/qdsp5/audpp.c
 *
 * Copyright (C) 2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <mach/msm_adsp.h>
#include <mach/debug_mm.h>
#include <mach/qdsp5/qdsp5audpreproc.h>
#include <mach/qdsp5/qdsp5audreccmdi.h>
#include <mach/qdsp5v2/audio_acdbi.h>


static DEFINE_MUTEX(audpreproc_lock);

struct msm_adspenc_info {
	const char *module_name;
	unsigned module_queueids;
	int module_encid; /* streamid */
	int enc_formats; /* supported formats */
	int nr_codec_support; /* number of codec suported */
};

#define ENC_MODULE_INFO(name, queueids, encid, formats, nr_codec) \
	{.module_name = name, .module_queueids = queueids, \
	 .module_encid = encid, .enc_formats = formats, \
	 .nr_codec_support = nr_codec }

#ifdef CONFIG_MSM7X27A_AUDIO
#define ENC0_FORMAT ((1<<AUDREC_CMD_TYPE_1_INDEX_SBC)| \
		(1<<AUDREC_CMD_TYPE_0_INDEX_AAC)| \
		(1<<AUDREC_CMD_TYPE_0_INDEX_AMRNB)| \
		(1<<AUDREC_CMD_TYPE_0_INDEX_EVRC)| \
		(1<<AUDREC_CMD_TYPE_0_INDEX_QCELP))

#define ENC1_FORMAT (1<<AUDREC_CMD_TYPE_0_INDEX_WAV)
#else
#define ENC0_FORMAT ((1<<AUDREC_CMD_TYPE_0_INDEX_WAV)| \
		(1<<AUDREC_CMD_TYPE_1_INDEX_SBC)| \
		(1<<AUDREC_CMD_TYPE_0_INDEX_AAC)| \
		(1<<AUDREC_CMD_TYPE_0_INDEX_AMRNB)| \
		(1<<AUDREC_CMD_TYPE_0_INDEX_EVRC)| \
		(1<<AUDREC_CMD_TYPE_0_INDEX_QCELP))
#endif

#define MAX_ENC_COUNT 2
#define MAX_EVENT_CALLBACK_CLIENTS 2

struct msm_adspenc_database {
	unsigned num_enc;
	struct msm_adspenc_info *enc_info_list;
};

#ifdef CONFIG_MSM7X27A_AUDIO
static struct msm_adspenc_info enc_info_list[] = {
	ENC_MODULE_INFO("AUDRECTASK", \
			((QDSP_uPAudRecBitStreamQueue << 16)| \
			  QDSP_uPAudRecCmdQueue), 0, \
			(ENC0_FORMAT | (1 << MSM_ADSP_ENC_MODE_TUNNEL) | \
			(1 << MSM_ADSP_ENC_MODE_NON_TUNNEL)), 5),

	ENC_MODULE_INFO("AUDREC1TASK", \
			((QDSP_uPAudRec1BitStreamQueue << 16)| \
			  QDSP_uPAudRec1CmdQueue), 1, \
			(ENC1_FORMAT | (1 << MSM_ADSP_ENC_MODE_TUNNEL)), 1),
};
#else
static struct msm_adspenc_info enc_info_list[] = {
	ENC_MODULE_INFO("AUDRECTASK",
			((QDSP_uPAudRecBitStreamQueue << 16)| \
			  QDSP_uPAudRecCmdQueue), 0, \
			(ENC0_FORMAT | (1 << MSM_ADSP_ENC_MODE_TUNNEL)), 6),
};
#endif

static struct msm_adspenc_database msm_enc_database = {
	.num_enc = ARRAY_SIZE(enc_info_list),
	.enc_info_list = enc_info_list,
};

struct audpreproc_state {
	struct msm_adsp_module *mod;
	audpreproc_event_func func[MAX_ENC_COUNT];
	void *private[MAX_ENC_COUNT];
	struct mutex *lock;
	unsigned open_count;
	unsigned enc_inuse;
	struct audpreproc_event_callback *cb_tbl[MAX_EVENT_CALLBACK_CLIENTS];
};

static struct audrec_session_info session_info;

static struct audpreproc_state the_audpreproc_state = {
	.lock = &audpreproc_lock,
};

/* DSP preproc event handler */
static void audpreproc_dsp_event(void *data, unsigned id, size_t len,
			    void (*getevent)(void *ptr, size_t len))
{
	struct audpreproc_state *audpreproc = data;
	uint16_t msg[2];
	MM_ERR("audpreproc_dsp_event %id", id);

	getevent(msg, sizeof(msg));

	switch (id) {
	case AUDPREPROC_MSG_CMD_CFG_DONE_MSG:
		MM_DBG("type %d, status_flag %d\n", msg[0], msg[1]);
		if (audpreproc->func[0])
			audpreproc->func[0](
			audpreproc->private[0], id,
			&msg);
		break;
	case AUDPREPROC_MSG_ERROR_MSG_ID:
		MM_INFO("err_index %d\n", msg[0]);
		if (audpreproc->func[0])
			audpreproc->func[0](
			audpreproc->private[0], id,
			&msg);
		break;
	case ADSP_MESSAGE_ID:
		MM_DBG("Received ADSP event: module enable(audpreproctask)\n");
		if (audpreproc->func[0])
			audpreproc->func[0](
			audpreproc->private[0], id,
			&msg);
		break;
	case AUDPREPROC_MSG_FEAT_QUERY_DM_DONE:
	   {
	    uint16_t msg[3];
	    getevent(msg, sizeof(msg));
	    MM_INFO("RTC ACK --> %x %x %x\n", msg[0], msg[1], msg[2]);
	    acdb_rtc_set_err(msg[2]);
	   }
	break;
	default:
		MM_ERR("unknown event %d\n", id);
	}
	return;
}

static struct msm_adsp_ops adsp_ops = {
	.event = audpreproc_dsp_event,
};

/* EXPORTED API's */
int audpreproc_enable(int enc_id, audpreproc_event_func func, void *private)
{
	struct audpreproc_state *audpreproc = &the_audpreproc_state;
	int res = 0;
	uint16_t msg[2];
	int n = 0;
	MM_DBG("audpreproc_enable %d\n", enc_id);

	if (enc_id < 0 || enc_id > (MAX_ENC_COUNT - 1))
		return -EINVAL;

	mutex_lock(audpreproc->lock);
	if (audpreproc->func[enc_id]) {
		res = -EBUSY;
		goto out;
	}

	audpreproc->func[enc_id] = func;
	audpreproc->private[enc_id] = private;

	/* First client to enable preproc task */
	if (audpreproc->open_count++ == 0) {
		MM_DBG("Get AUDPREPROCTASK\n");
		res = msm_adsp_get("AUDPREPROCTASK", &audpreproc->mod,
				&adsp_ops, audpreproc);
		if (res < 0) {
			MM_ERR("Can not get AUDPREPROCTASK\n");
			audpreproc->open_count = 0;
			audpreproc->func[enc_id] = NULL;
			audpreproc->private[enc_id] = NULL;
			goto out;
		}
		if (msm_adsp_enable(audpreproc->mod)) {
			audpreproc->open_count = 0;
			audpreproc->func[enc_id] = NULL;
			audpreproc->private[enc_id] = NULL;
			msm_adsp_put(audpreproc->mod);
			audpreproc->mod = NULL;
			res = -ENODEV;
			goto out;
		}
	}
	msg[0] = AUDPREPROC_MSG_STATUS_FLAG_ENA;
	/* Generate audpre enabled message for registered clients */
	for (n = 0; n < MAX_EVENT_CALLBACK_CLIENTS; ++n) {
			if (audpreproc->cb_tbl[n] &&
					audpreproc->cb_tbl[n]->fn) {
				audpreproc->cb_tbl[n]->fn( \
						audpreproc->cb_tbl[n]->private,\
						AUDPREPROC_MSG_CMD_CFG_DONE_MSG,
						(void *) &msg);
			}
	}
	res = 0;
out:
	mutex_unlock(audpreproc->lock);
	return res;
}
EXPORT_SYMBOL(audpreproc_enable);


void audpreproc_disable(int enc_id, void *private)
{
	struct audpreproc_state *audpreproc = &the_audpreproc_state;
	uint16_t msg[2];
	int n = 0;

	if (enc_id < 0 || enc_id > (MAX_ENC_COUNT - 1))
		return;

	mutex_lock(audpreproc->lock);
	if (!audpreproc->func[enc_id])
		goto out;
	if (audpreproc->private[enc_id] != private)
		goto out;

	audpreproc->func[enc_id] = NULL;
	audpreproc->private[enc_id] = NULL;

	/* Last client then disable preproc task */
	if (--audpreproc->open_count == 0) {
		msm_adsp_disable(audpreproc->mod);
		MM_DBG("Put AUDPREPROCTASK\n");
		msm_adsp_put(audpreproc->mod);
		audpreproc->mod = NULL;
	}
	msg[0] = AUDPREPROC_MSG_STATUS_FLAG_DIS;
	/* Generate audpre enabled message for registered clients */
	for (n = 0; n < MAX_EVENT_CALLBACK_CLIENTS; ++n) {
			if (audpreproc->cb_tbl[n] &&
					audpreproc->cb_tbl[n]->fn) {
				audpreproc->cb_tbl[n]->fn( \
						audpreproc->cb_tbl[n]->private,\
						AUDPREPROC_MSG_CMD_CFG_DONE_MSG,
						(void *) &msg);
			}
	}
out:
	mutex_unlock(audpreproc->lock);
	return;
}
EXPORT_SYMBOL(audpreproc_disable);

int audpreproc_update_audrec_info(
			struct audrec_session_info *audrec_session_info)
{
	if (!audrec_session_info) {
		MM_ERR("error in audrec session info address\n");
		return -EINVAL;
	}
	if (audrec_session_info->session_id < MAX_ENC_COUNT) {
		memcpy(&session_info,
				audrec_session_info,
				sizeof(struct audrec_session_info));
		return 0;
	}
	return -EINVAL;
}
EXPORT_SYMBOL(audpreproc_update_audrec_info);

int get_audrec_session_info(struct audrec_session_info *info)
{
	if (!info) {
		MM_ERR("error in audrec session info address\n");
		return -EINVAL;
	}

	if (the_audpreproc_state.open_count == 0) {
		MM_ERR("No aud pre session active\n");
		return -EINVAL;
	}

	memcpy(info, &session_info, sizeof(struct audrec_session_info));

	return 0;
}
EXPORT_SYMBOL(get_audrec_session_info);

int audpreproc_register_event_callback(struct audpreproc_event_callback *ecb)
{
	struct audpreproc_state *audpreproc = &the_audpreproc_state;
	int i;

	for (i = 0; i < MAX_EVENT_CALLBACK_CLIENTS; ++i) {
		if (NULL == audpreproc->cb_tbl[i]) {
			audpreproc->cb_tbl[i] = ecb;
			return 0;
		}
	}
	return -EINVAL;
}
EXPORT_SYMBOL(audpreproc_register_event_callback);

int audpreproc_unregister_event_callback(struct audpreproc_event_callback *ecb)
{
	struct audpreproc_state *audpreproc = &the_audpreproc_state;
	int i;

	for (i = 0; i < MAX_EVENT_CALLBACK_CLIENTS; ++i) {
		if (ecb == audpreproc->cb_tbl[i]) {
			audpreproc->cb_tbl[i] = NULL;
			return 0;
		}
	}
	return -EINVAL;
}
/* enc_type = supported encode format *
 * like pcm, aac, sbc, evrc, qcelp, amrnb etc ... *
 */
int audpreproc_aenc_alloc(unsigned enc_type, const char **module_name,
		     unsigned *queue_ids)
{
	struct audpreproc_state *audpreproc = &the_audpreproc_state;
	int encid = -1, idx, lidx, mode, codec;
	int codecs_supported, min_codecs_supported;

	mutex_lock(audpreproc->lock);
	/* Represents in bit mask */
	mode = ((enc_type & AUDPREPROC_MODE_MASK) << 16);
	codec = (1 << (enc_type & AUDPREPROC_CODEC_MASK));

	lidx = msm_enc_database.num_enc;
	min_codecs_supported = sizeof(unsigned int) * 8;
	MM_DBG("mode = 0x%08x codec = 0x%08x\n", mode, codec);

	for (idx = lidx-1; idx >= 0; idx--) {
		/* encoder free and supports the format */
		if (!(audpreproc->enc_inuse & (1 << (idx))) &&
		((mode & msm_enc_database.enc_info_list[idx].enc_formats)
		== mode) && ((codec &
		msm_enc_database.enc_info_list[idx].enc_formats)
		== codec)){
			/* Check supports minimum number codecs */
			codecs_supported =
			msm_enc_database.enc_info_list[idx].nr_codec_support;
			if (codecs_supported < min_codecs_supported) {
				lidx = idx;
				min_codecs_supported = codecs_supported;
			}
		}
	}

	if (lidx < msm_enc_database.num_enc) {
		audpreproc->enc_inuse |= (1 << lidx);
		*module_name =
		    msm_enc_database.enc_info_list[lidx].module_name;
		*queue_ids =
		    msm_enc_database.enc_info_list[lidx].module_queueids;
		encid = msm_enc_database.enc_info_list[lidx].module_encid;
	}

	mutex_unlock(audpreproc->lock);
	return encid;
}
EXPORT_SYMBOL(audpreproc_aenc_alloc);

void audpreproc_aenc_free(int enc_id)
{
	struct audpreproc_state *audpreproc = &the_audpreproc_state;
	int idx;

	mutex_lock(audpreproc->lock);
	for (idx = 0; idx < msm_enc_database.num_enc; idx++) {
		if (msm_enc_database.enc_info_list[idx].module_encid ==
		    enc_id) {
			audpreproc->enc_inuse &= ~(1 << idx);
			break;
		}
	}
	mutex_unlock(audpreproc->lock);
	return;

}
EXPORT_SYMBOL(audpreproc_aenc_free);

int audpreproc_dsp_set_agc(
		audpreproc_cmd_cfg_agc_params *agc_cfg,
		unsigned len)
{
	return msm_adsp_write(the_audpreproc_state.mod,
			QDSP_uPAudPreProcCmdQueue, agc_cfg, len);
}
EXPORT_SYMBOL(audpreproc_dsp_set_agc);

int audpreproc_dsp_set_ns(
	audpreproc_cmd_cfg_ns_params *ns_cfg,
	unsigned len)
{
	return msm_adsp_write(the_audpreproc_state.mod,
			QDSP_uPAudPreProcCmdQueue, ns_cfg, len);
}
EXPORT_SYMBOL(audpreproc_dsp_set_ns);

int audpreproc_dsp_set_iir(
		audpreproc_cmd_cfg_iir_tuning_filter_params *iir_cfg,
		unsigned len)
{
	return msm_adsp_write(the_audpreproc_state.mod,
			QDSP_uPAudPreProcCmdQueue, iir_cfg, len);
}
EXPORT_SYMBOL(audpreproc_dsp_set_iir);

int audpreproc_send_preproccmdqueue(void *cmd, unsigned len)
{
	return msm_adsp_write(the_audpreproc_state.mod,
			QDSP_uPAudPreProcCmdQueue, cmd, len);
}
EXPORT_SYMBOL(audpreproc_send_preproccmdqueue);
