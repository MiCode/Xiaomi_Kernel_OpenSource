// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include "msm_cvp_common.h"
#include "cvp_hfi_api.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_core.h"
#include "msm_cvp_dsp.h"

void cvp_dump_fence_queue(struct msm_cvp_inst *inst)
{
	struct cvp_fence_queue *q;
	struct cvp_fence_command *f;
	struct synx_session ssid;
	int i;

	q = &inst->fence_cmd_queue;
	ssid = inst->synx_session_id;
	mutex_lock(&q->lock);
	dprintk(CVP_WARN, "inst %x fence q mode %d, ssid %d\n",
			hash32_ptr(inst->session), q->mode, ssid.client_id);

	dprintk(CVP_WARN, "fence cmdq wait list:\n");
	list_for_each_entry(f, &q->wait_list, list) {
		dprintk(CVP_WARN, "frame pkt type 0x%x\n", f->pkt->packet_type);
		for (i = 0; i < f->output_index; i++)
			dprintk(CVP_WARN, "idx %d client hdl %d, state %d\n",
				i, f->synx[i],
				synx_get_status(ssid, f->synx[i]));

	}

	dprintk(CVP_WARN, "fence cmdq schedule list:\n");
	list_for_each_entry(f, &q->sched_list, list) {
		dprintk(CVP_WARN, "frame pkt type 0x%x\n", f->pkt->packet_type);
		for (i = 0; i < f->output_index; i++)
			dprintk(CVP_WARN, "idx %d client hdl %d, state %d\n",
				i, f->synx[i],
				synx_get_status(ssid, f->synx[i]));

	}
	mutex_unlock(&q->lock);
}

int cvp_import_synx(struct msm_cvp_inst *inst, struct cvp_fence_command *fc,
		u32 *fence)
{
	int rc = 0;
	int i;
	struct cvp_fence_type *fs;
	struct synx_import_params params;
	s32 h_synx;
	struct synx_session ssid;

	if (fc->signature != 0xFEEDFACE) {
		dprintk(CVP_ERR, "%s Deprecated synx path\n", __func__);
		return -EINVAL;
	}

	fs = (struct cvp_fence_type *)fence;
	ssid = inst->synx_session_id;

	for (i = 0; i < fc->num_fences; ++i) {
		h_synx = fs[i].h_synx;

		if (h_synx) {
			params.h_synx = h_synx;
			params.secure_key = fs[i].secure_key;
			params.new_h_synx = &fc->synx[i];

			rc = synx_import(ssid, &params);
			if (rc) {
				dprintk(CVP_ERR,
					"%s: %d synx_import failed\n",
					__func__, h_synx);
				return rc;
			}
		}
	}

	return 0;
}

int cvp_release_synx(struct msm_cvp_inst *inst, struct cvp_fence_command *fc)
{
	int rc = 0;
	int i;
	s32 h_synx;
	struct synx_session ssid;

	if (fc->signature != 0xFEEDFACE) {
		dprintk(CVP_ERR, "%s deprecated synx_path\n", __func__);
		return -EINVAL;
	}

	ssid = inst->synx_session_id;
	for (i = 0; i < fc->num_fences; ++i) {
		h_synx = fc->synx[i];
		if (h_synx) {
			rc = synx_release(ssid, h_synx);
			if (rc)
				dprintk(CVP_ERR,
				"%s: synx_release %d, %d failed\n",
				__func__, h_synx, i);
		}
	}
	return rc;
}

static int cvp_cancel_synx_impl(struct msm_cvp_inst *inst,
			enum cvp_synx_type type,
			struct cvp_fence_command *fc,
			int synx_state)
{
	int rc = 0;
	int i;
	int h_synx;
	struct synx_session ssid;
	int start = 0, end = 0;

	ssid = inst->synx_session_id;

	if (type == CVP_INPUT_SYNX) {
		start = 0;
		end = fc->output_index;
	} else if (type == CVP_OUTPUT_SYNX) {
		start = fc->output_index;
		end = fc->num_fences;
	} else {
		dprintk(CVP_ERR, "%s Incorrect synx type\n", __func__);
		return -EINVAL;
	}

	for (i = start; i < end; ++i) {
		h_synx = fc->synx[i];
		if (h_synx) {
			rc = synx_signal(ssid, h_synx, synx_state);
			dprintk(CVP_SYNX, "Cancel synx %d session %llx\n",
					h_synx, inst);
			if (rc)
				dprintk(CVP_ERR,
					"%s: synx_signal %d %d %d failed\n",
				__func__, h_synx, i, synx_state);
		}
	}

	return rc;


}

int cvp_cancel_synx(struct msm_cvp_inst *inst, enum cvp_synx_type type,
		struct cvp_fence_command *fc, int synx_state)
{
	if (fc->signature != 0xFEEDFACE) {
		dprintk(CVP_ERR, "%s deprecated synx path\n", __func__);
			return -EINVAL;
		}

	return cvp_cancel_synx_impl(inst, type, fc, synx_state);
}

static int cvp_wait_synx(struct synx_session ssid, u32 *synx, u32 num_synx,
		u32 *synx_state)
{
	int i = 0, rc = 0;
	unsigned long timeout_ms = 1000;
	int h_synx;

	while (i < num_synx) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_wait(ssid, h_synx, timeout_ms);
			if (rc) {
				*synx_state = synx_get_status(ssid, h_synx);
				if (*synx_state == SYNX_STATE_SIGNALED_CANCEL) {
					dprintk(CVP_SYNX,
					"%s: synx_wait %d cancel %d state %d\n",
					current->comm, i, rc, *synx_state);
				} else {
					dprintk(CVP_ERR,
					"%s: synx_wait %d failed %d state %d\n",
					current->comm, i, rc, *synx_state);
					*synx_state = SYNX_STATE_SIGNALED_ERROR;
				}
				return rc;
			}
			dprintk(CVP_SYNX, "Wait synx %d returned succes\n",
					h_synx);
		}
		++i;
	}
	return rc;
}

static int cvp_signal_synx(struct synx_session ssid, u32 *synx, u32 num_synx,
		u32 synx_state)
{
	int i = 0, rc = 0;
	int h_synx;

	while (i < num_synx) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_signal(ssid, h_synx, synx_state);
			if (rc) {
				dprintk(CVP_ERR,
					"%s: synx_signal %d %d failed\n",
					current->comm, h_synx, i);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
			dprintk(CVP_SYNX, "Signaled synx %d\n", h_synx);
		}
		++i;
	}
	return rc;
}

int cvp_synx_ops(struct msm_cvp_inst *inst, enum cvp_synx_type type,
		struct cvp_fence_command *fc, u32 *synx_state)
{
	struct synx_session ssid;

	ssid = inst->synx_session_id;

	if (fc->signature != 0xFEEDFACE) {
		dprintk(CVP_ERR, "%s deprecated synx, type %d\n", __func__);
				return -EINVAL;
	}

	if (type == CVP_INPUT_SYNX) {
		return cvp_wait_synx(ssid, fc->synx, fc->output_index,
				synx_state);
	} else if (type == CVP_OUTPUT_SYNX) {
		return cvp_signal_synx(ssid, &fc->synx[fc->output_index],
				(fc->num_fences - fc->output_index),
				*synx_state);
	} else {
		dprintk(CVP_ERR, "%s Incorrect SYNX type\n", __func__);
		return -EINVAL;
	}
}

