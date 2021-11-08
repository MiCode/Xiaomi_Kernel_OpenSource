/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */

#ifndef __GH_HCALL_CORE_CTL_H
#define __GH_HCALL_CORE_CTL_H

#include <linux/err.h>
#include <linux/types.h>

#include <linux/gunyah/hcall_common.h>
#include <linux/gunyah/gh_common.h>
#include <asm/gunyah/hcall.h>

static inline int gh_hcall_vcpu_affinity_set(gh_capid_t vcpu_capid,
						uint32_t cpu_index)
{
	int ret;
	struct gh_hcall_resp _resp = {0};

	ret = _gh_hcall(0x603d,
			(struct gh_hcall_args){ vcpu_capid, cpu_index, -1 },
			&_resp);

	return ret;
}

static inline int gh_hcall_vpm_group_get_state(u64 vpmg_capid,
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

static inline int gh_hcall_vcpu_yield(uint64_t control, uint64_t arg)
{
	struct gh_hcall_resp _resp = { 0 };

	return _gh_hcall(0x603b, (struct gh_hcall_args){ control, arg }, &_resp);

}

#endif
