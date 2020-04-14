// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include "msm_cvp_common.h"
#include "cvp_hfi_api.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_core.h"
#include "msm_cvp_dsp.h"

static int cvp_import_synx_deprecate(struct msm_cvp_inst *inst, u32 type,
		u32 *fence, u32 *synx)
{
	int rc = 0;
	int i;
	int start = 0, end = 0;
	struct cvp_fence_type *f;
	struct synx_import_params params;
	s32 h_synx;
	struct synx_session ssid;

	f = (struct cvp_fence_type *)fence;
	ssid = inst->synx_session_id;

	switch (type) {
	case HFI_CMD_SESSION_CVP_DME_FRAME:
	{
		start = 0;
		end = HFI_DME_BUF_NUM;
		break;
	}
	case HFI_CMD_SESSION_CVP_FD_FRAME:
	{
		u32 in = fence[0];
		u32 out = fence[1];

		if (in > MAX_HFI_FENCE_SIZE || out > MAX_HFI_FENCE_SIZE
			|| in > MAX_HFI_FENCE_SIZE - out) {
			dprintk(CVP_ERR, "%s: failed!\n", __func__);
			rc = -EINVAL;
			return rc;
		}

		synx[0] = (in << 16) | out;
		start = 1;
		end = in + out + 1;
		break;
	}
	default:
		dprintk(CVP_ERR, "%s: unknown fence type\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	for (i = start; i < end; ++i) {
		h_synx = f[i].h_synx;

		if (h_synx) {
			params.h_synx = h_synx;
			params.secure_key = f[i].secure_key;
			params.new_h_synx = &synx[i];

			rc = synx_import(ssid, &params);
			if (rc) {
				dprintk(CVP_ERR,
					"%s: synx_import failed\n",
					__func__);
				return rc;
			}
		}
	}

	return rc;
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

	if (fc->signature != 0xFEEDFACE)
		return cvp_import_synx_deprecate(inst, fc->type, fence,
					fc->synx);

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
					"%s: synx_import failed\n",
					__func__);
				return rc;
			}
		}
	}

	return 0;
}

static int cvp_release_synx_deprecate(struct msm_cvp_inst *inst, u32 type,
				u32 *synx)
{
	int rc = 0;
	int i;
	s32 h_synx;
	struct synx_session ssid;
	int start = 0, end = 0;

	ssid = inst->synx_session_id;

	switch (type) {
	case HFI_CMD_SESSION_CVP_DME_FRAME:
	{
		start = 0;
		end = HFI_DME_BUF_NUM;

		break;
	}
	case HFI_CMD_SESSION_CVP_FD_FRAME:
	{
		u32 in = synx[0] >> 16;
		u32 out = synx[0] & 0xFFFF;

		start = 1;
		end = in + out + 1;

		break;
	}
	default:
		dprintk(CVP_ERR, "%s: unknown fence type\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	for (i = start; i < end; ++i) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_release(ssid, h_synx);
			if (rc)
				dprintk(CVP_ERR,
				"%s: synx_release %d failed\n",
				__func__, i);
		}
	}

	return rc;
}

int cvp_release_synx(struct msm_cvp_inst *inst, struct cvp_fence_command *fc)
{
	int rc = 0;
	int i;
	s32 h_synx;
	struct synx_session ssid;

	if (fc->signature != 0xFEEDFACE)
		return cvp_release_synx_deprecate(inst, fc->type, fc->synx);

	ssid = inst->synx_session_id;
	for (i = 0; i < fc->num_fences; ++i) {
		h_synx = fc->synx[i];
		if (h_synx) {
			rc = synx_release(ssid, h_synx);
			if (rc)
				dprintk(CVP_ERR,
				"%s: synx_release %d failed\n",
				__func__, i);
		}
	}
	return rc;
}

static int cvp_cancel_input_synx_deprecate(struct msm_cvp_inst *inst, u32 type,
				u32 *synx)
{
	int rc = 0;
	int i;
	int h_synx;
	struct synx_session ssid;
	int start = 0, end = 0;
	int synx_state = SYNX_STATE_SIGNALED_CANCEL;

	ssid = inst->synx_session_id;

	switch (type) {
	case HFI_CMD_SESSION_CVP_DME_FRAME:
	{
		start = 1;
		end = HFI_DME_BUF_NUM - 1;
		break;
	}
	case HFI_CMD_SESSION_CVP_FD_FRAME:
	{
		u32 in, out;

		in = synx[0] >> 16;
		out = synx[0] & 0xFFFF;

		start = 1;
		end = in + 1;
		break;
	}
	default:
		dprintk(CVP_ERR, "%s: unknown fence type\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	for (i = start; i < end; ++i) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_signal(ssid, h_synx, synx_state);
			if (rc && rc != -EALREADY) {
				dprintk(CVP_ERR, "%s: synx_signal %d failed\n",
				__func__, i);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
		}
	}

	return rc;
}

static int cvp_cancel_output_synx_deprecate(struct msm_cvp_inst *inst, u32 type,
					u32 *synx)
{
	int rc = 0;
	int i;
	int h_synx;
	struct synx_session ssid;
	int start = 0, end = 0;
	int synx_state = SYNX_STATE_SIGNALED_CANCEL;

	ssid = inst->synx_session_id;

	switch (type) {
	case HFI_CMD_SESSION_CVP_DME_FRAME:
	{
		start = FENCE_DME_OUTPUT_IDX;
		end = FENCE_DME_OUTPUT_IDX + 1;
		break;
	}
	case HFI_CMD_SESSION_CVP_FD_FRAME:
	{
		u32 in, out;

		in = synx[0] >> 16;
		out = synx[0] & 0xFFFF;

		start = in + 1;
		end = in + out + 1;
		break;
	}
	default:
		dprintk(CVP_ERR, "%s: unknown fence type\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	for (i = start; i < end; ++i) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_signal(ssid, h_synx, synx_state);
			if (rc) {
				dprintk(CVP_ERR, "%s: synx_signal %d failed\n",
				__func__, i);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
		}
	}

	return rc;
}

static int cvp_cancel_synx_impl(struct msm_cvp_inst *inst,
			enum cvp_synx_type type,
			struct cvp_fence_command *fc)
{
	int rc = 0;
	int i;
	int h_synx;
	struct synx_session ssid;
	int start = 0, end = 0;
	int synx_state = SYNX_STATE_SIGNALED_CANCEL;

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
			if (rc) {
				dprintk(CVP_ERR, "%s: synx_signal %d failed\n",
				__func__, i);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
		}
	}

	return rc;


}

int cvp_cancel_synx(struct msm_cvp_inst *inst, enum cvp_synx_type type,
		struct cvp_fence_command *fc)
{
	if (fc->signature != 0xFEEDFACE) {
		if (type == CVP_INPUT_SYNX)
			return cvp_cancel_input_synx_deprecate(inst, fc->type,
							fc->synx);
		else if (type == CVP_OUTPUT_SYNX)
			return cvp_cancel_output_synx_deprecate(inst, fc->type,
							fc->synx);
		else {
			dprintk(CVP_ERR, "Incorrect synx type %d\n", type);
			return -EINVAL;
		}
	}

	return cvp_cancel_synx_impl(inst, type, fc);
}

static int cvp_wait_dme_synx_deprecate(struct synx_session ssid, u32 *synx,
				u32 *synx_state)
{
	int i, rc = 0;
	int h_synx;
	unsigned long timeout_ms = 1000;

	i = 0;
	while (i < HFI_DME_BUF_NUM - 1) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_wait(ssid, h_synx, timeout_ms);
			if (rc) {
				dprintk(CVP_ERR, "%s %s: synx_wait %d failed\n",
					current->comm, __func__, i);
				*synx_state = SYNX_STATE_SIGNALED_ERROR;
				return -EINVAL;
			}
			/*
			 * Increase loop count to skip fence
			 * waiting on downscale image where i == 1.
			 */
			if (i == FENCE_DME_ICA_ENABLED_IDX)
				++i;
		}
		++i;
	}
	return rc;
}

static int cvp_signal_dme_synx_deprecate(struct synx_session ssid, u32 *synx,
				u32 synx_state)
{
	int rc = 0;
	int h_synx;

	if (synx[FENCE_DME_ICA_ENABLED_IDX]) {
		h_synx = synx[FENCE_DME_DS_IDX];

		rc = synx_signal(ssid, h_synx, synx_state);
		if (rc) {
			dprintk(CVP_ERR, "%s %s: synx_signal %d failed\n",
				current->comm, __func__, FENCE_DME_DS_IDX);
			synx_state = SYNX_STATE_SIGNALED_ERROR;
		}
	}

	h_synx = synx[FENCE_DME_OUTPUT_IDX];
	rc = synx_signal(ssid, h_synx, synx_state);
	if (rc)
		dprintk(CVP_ERR, "%s %s: synx_signal %d failed\n",
			current->comm, __func__, FENCE_DME_OUTPUT_IDX);

	return rc;
}

static int cvp_wait_fd_synx_deprecate(struct synx_session ssid, u32 *synx,
				u32 *synx_state)
{
	int i, rc = 0;
	unsigned long timeout_ms = 1000;
	int h_synx;
	u32 in;

	in = synx[0] >> 16;

	i = 1;
	while (i <= in) {
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
		}
		++i;
	}
	return rc;
}

static int cvp_signal_fd_synx_deprecate(struct synx_session ssid, u32 *synx,
				u32 synx_state)
{
	int i, rc = 0;
	u32 in, out;
	int h_synx;

	in = synx[0] >> 16;
	out = synx[0] & 0xFFFF;

	i = in + 1;
	while (i <= in + out) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_signal(ssid, h_synx, synx_state);
			if (rc) {
				dprintk(CVP_ERR, "%s: synx_signal %d failed\n",
				current->comm, i);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
		}
		++i;
	}

	return rc;
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
				dprintk(CVP_ERR, "%s: synx_signal %d failed\n",
				current->comm, i);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
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
		if (fc->type == HFI_CMD_SESSION_CVP_DME_FRAME) {
			if (type == CVP_INPUT_SYNX)
				return cvp_wait_dme_synx_deprecate(ssid,
						fc->synx, synx_state);
			else if (type == CVP_OUTPUT_SYNX)
				return cvp_signal_dme_synx_deprecate(ssid,
						fc->synx, *synx_state);
			else
				return -EINVAL;
		} else if (fc->type == HFI_CMD_SESSION_CVP_FD_FRAME) {
			if (type == CVP_INPUT_SYNX)
				return cvp_wait_fd_synx_deprecate(ssid,
						fc->synx, synx_state);
			else if (type == CVP_OUTPUT_SYNX)
				return cvp_signal_fd_synx_deprecate(ssid,
						fc->synx, *synx_state);
			else
				return -EINVAL;
		} else {
			dprintk(CVP_ERR, "%s Incorrect pkt type\n",
					__func__);
			return -EINVAL;
		}
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

