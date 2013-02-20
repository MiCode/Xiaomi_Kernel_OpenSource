/*
 * Common code to deal with the AUDPREPROC dsp task (audio preprocessing)
 *
 * Copyright (c) 2011, The Linux Foundation. All rights reserved.
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
	struct mutex *lock;
	unsigned open_count;
	unsigned enc_inuse;
};

static struct audpreproc_state the_audpreproc_state = {
	.lock = &audpreproc_lock,
};

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
