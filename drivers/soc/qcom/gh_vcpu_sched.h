/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __GH_VCPU_SCHED_H
#define __GH_VCPU_SCHED_H

#include <linux/err.h>
#include <linux/types.h>

#include <linux/gunyah/hcall_common.h>
#include <linux/gunyah/gh_common.h>
#include <asm/gunyah/hcall.h>

struct gh_hcall_vcpu_run_resp {
	uint64_t vcpu_state;
	uint64_t vcpu_suspend_state;
	uint64_t state_data_0;
	uint64_t state_data_1;
	uint64_t state_data_2;
};

static inline int gh_hcall_vcpu_run(gh_capid_t vcpu_capid, uint64_t resume_data_0,
					uint64_t resume_data_1, uint64_t resume_data_2,
					struct gh_hcall_vcpu_run_resp *resp)
{
	int ret;
	struct gh_hcall_resp _resp = {0};

	ret = _gh_hcall(0x6065,
			(struct gh_hcall_args){ vcpu_capid, resume_data_0,
						resume_data_1, resume_data_2, 0 }, &_resp);

	resp->vcpu_state = _resp.resp1;
	resp->vcpu_suspend_state = _resp.resp2;
	resp->state_data_0 = _resp.resp3;
	resp->state_data_1 = _resp.resp4;
	resp->state_data_2 = _resp.resp5;

	return ret;
}

static inline int gh_hcall_vpm_group_get_state(gh_capid_t vpmg_capid,
						uint64_t *vpmg_state)
{
	int ret;
	struct gh_hcall_resp _resp = {0};

	ret = _gh_hcall(0x6045,
			(struct gh_hcall_args){ vpmg_capid, 0 },
			&_resp);
	*vpmg_state = _resp.resp1;

	return ret;
}
#endif
