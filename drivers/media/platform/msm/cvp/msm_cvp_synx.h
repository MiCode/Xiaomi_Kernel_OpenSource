/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_CVP_SYNX_H_
#define _MSM_CVP_SYNX_H_

#include <linux/types.h>
#include <media/msm_cvp_private.h>
#include "cvp_comm_def.h"


struct cvp_fence_queue {
	struct mutex lock;
	enum queue_state state;
	enum op_mode mode;
	struct list_head wait_list;
	wait_queue_head_t wq;
	struct list_head sched_list;
};

struct cvp_fence_type {
	s32 h_synx;
	u32 secure_key;
};

struct cvp_fence_command {
	struct list_head list;
	u64 frame_id;
	enum op_mode mode;
	u32 signature;
	u32 num_fences;
	u32 output_index;
	u32 type;
	u32 synx[MAX_HFI_FENCE_SIZE/2];
	struct cvp_hfi_cmd_session_hdr *pkt;
};

enum cvp_synx_type {
	CVP_UINIT_SYNX,
	CVP_INPUT_SYNX,
	CVP_OUTPUT_SYNX,
	CVP_INVALID_SYNX,
};

int cvp_import_synx(struct msm_cvp_inst *inst, struct cvp_fence_command *fc,
		u32 *fence);
int cvp_release_synx(struct msm_cvp_inst *inst, struct cvp_fence_command *fc);
int cvp_cancel_synx(struct msm_cvp_inst *inst, enum cvp_synx_type type,
		struct cvp_fence_command *fc, int synx_state);
int cvp_synx_ops(struct msm_cvp_inst *inst, enum cvp_synx_type type,
		struct cvp_fence_command *fc, u32 *synx_state);
void cvp_dump_fence_queue(struct msm_cvp_inst *inst);
#endif
