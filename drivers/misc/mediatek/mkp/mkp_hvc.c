// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 * Author: Kuan-Ying Lee <Kuan-Ying.Lee@mediatek.com>
 */

#include "mkp_hvc.h"
#include "debug.h"

DEBUG_SET_LEVEL(DEBUG_LEVEL_ERR);

static void mkp_smccc_hvc(unsigned long a0, unsigned long a1,
			  unsigned long a2, unsigned long a3,
			  unsigned long a4, unsigned long a5,
			  unsigned long a6, unsigned long a7,
			  struct arm_smccc_res *res)
{
	arm_smccc_hvc(a0, a1, a2, a3, a4, a5, a6, a7, res);
}

int mkp_set_mapping_ro_hvc_call(uint32_t policy, uint32_t handle)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_SET_MAPPING_RO);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, handle, 0, 0, 0, 0, 0, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);

	return res.a0 ? -1 : 0;
}

int mkp_set_mapping_rw_hvc_call(uint32_t policy, uint32_t handle)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_SET_MAPPING_RW);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, handle, 0, 0, 0, 0, 0, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);

	return res.a0 ? -1 : 0;
}

int mkp_set_mapping_nx_hvc_call(uint32_t policy, uint32_t handle)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_SET_MAPPING_NX);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, handle, 0, 0, 0, 0, 0, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);

	return res.a0 ? -1 : 0;
}

int mkp_set_mapping_x_hvc_call(uint32_t policy, uint32_t handle)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_SET_MAPPING_X);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, handle, 0, 0, 0, 0, 0, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);

	return res.a0 ? -1 : 0;
}

int mkp_clear_mapping_hvc_call(uint32_t policy, uint32_t handle)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_CLEAR_MAPPING);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, handle, 0, 0, 0, 0, 0, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);

	return res.a0 ? -1 : 0;
}

int mkp_lookup_mapping_entry_hvc_call(uint32_t policy, uint32_t handle,
	unsigned long *entry_size, unsigned long *permission)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_LOOKUP_MAPPING_ENTRY);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, handle, 0, 0, 0, 0, 0, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	if (res.a0)
		return -1;

	*entry_size = res.a1;
	*permission = res.a2;
	return 0;

}
int mkp_req_new_policy_hvc_call(unsigned long policy_char)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;
	uint32_t policy = 0;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_NEW_POLICY);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, policy_char, 0, 0, 0, 0, 0, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	policy = (uint32_t)res.a0;

	if (res.a0 == 0)
		return -1;

	return (int)policy;
}

int mkp_change_policy_action_hvc_call(uint32_t policy, unsigned long policy_char_action)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;
	int ret = -1;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_POLICY_ACTION);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, policy_char_action, 0, 0, 0, 0, 0, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	ret = (int)res.a0;

	/* Success: 0, Fail: other */
	return ret;
}

int mkp_req_new_specified_policy_hvc_call(unsigned long policy_char, uint32_t specified_policy)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;
	uint32_t policy = 0;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_NEW_SPECIFIED_POLICY);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, policy_char, specified_policy, 0, 0, 0, 0, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, specified_policy, res.a0, res.a1, res.a2, res.a3);
	policy = (uint32_t)res.a0;

	if (res.a0 == 0)
		return -1;

	/* Expect "policy == specified_policy" */
	if (policy != specified_policy)
		return -1;

	return (int)policy;
}

uint32_t mkp_create_handle_hvc_call(uint32_t policy,
	unsigned long ipa, unsigned long size)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;
	uint32_t handle = 0;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_CREATE_HANDLE);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, ipa, size, 0, 0, 0, 0, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	handle = (uint32_t)(res.a0);

	// if fail return 0
	// success return >0

	return handle;
}
int mkp_destroy_handle_hvc_call(uint32_t policy, uint32_t handle)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_DESTROY_HANDLE);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, handle, 0, 0, 0, 0, 0, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);

	return res.a0 ? -1 : 0;
}

int mkp_configure_sharebuf_hvc_call(uint32_t policy, uint32_t handle, uint32_t type,
	unsigned long nr_entries, unsigned long size)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_CONFIGURE_SHAREBUF);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, handle, type, nr_entries, size, 0, 0, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	return res.a0 ? -1 : 0;
}

int mkp_update_sharebuf_1_argu_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_UPDATE_SHAREBUF_1_ARGU);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, handle, index, a1, 0, 0, 0, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	return res.a0 ? -1 : 0;
}

int mkp_update_sharebuf_2_argu_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1, unsigned long a2)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_UPDATE_SHAREBUF_2_ARGU);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, handle, index, a1, a2, 0, 0, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	return res.a0 ? -1 : 0;
}

int mkp_update_sharebuf_3_argu_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1, unsigned long a2, unsigned long a3)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_UPDATE_SHAREBUF_3_ARGU);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, handle, index, a1, a2, a3, 0, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	return res.a0 ? -1 : 0;
}

int mkp_update_sharebuf_4_argu_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_UPDATE_SHAREBUF_4_ARGU);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, handle, index, a1, a2, a3, a4, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	return res.a0 ? -1 : 0;
}

int mkp_update_sharebuf_5_argu_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_UPDATE_SHAREBUF_5_ARGU);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, handle, index, a1, a2, a3, a4, a5, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	return res.a0 ? -1 : 0;
}

int mkp_update_sharebuf_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long ipa)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(policy, HVC_FUNC_UPDATE_SHAREBUF);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, handle, index, ipa, 0, 0, 0, 0, &res);
	MKP_DEBUG("%s:%d hvc_id:0x%x, policy:%d res:0x%lx 0x%lx 0x%lx 0x%lx\n", __func__, __LINE__,
		mkp_hvc_fast_call_id, policy, res.a0, res.a1, res.a2, res.a3);
	return res.a0 ? -1 : 0;
}

/* Pass some essential information to MKP service, return 0 if success. */
int __init mkp_setup_essential_hvc_call(unsigned long phys_offset,
		unsigned long fixaddr_top, unsigned long fixaddr_real_start)
{
	uint64_t phys_addr;
	static int is_executed;
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;
	int ret = -1;

	/* Get subscribe's IPA */
	phys_addr = (uint64_t)(vmalloc_to_pfn((void *)&subscribe) << PAGE_SHIFT) +
		    ((uint64_t)&subscribe & ~PAGE_MASK);

	if (is_executed == 0) {
		mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(0, HVC_FUNC_ESS_0);
		mkp_smccc_hvc(mkp_hvc_fast_call_id, phys_offset, fixaddr_top,
			      fixaddr_real_start, phys_addr, 0, 0, 0, &res);
		is_executed = 1;

		/* Success */
		if (res.a0 == 0)
			ret = 0;
	}

	return ret;
}

/* Tell MKP service it should start granting */
int __init mkp_start_granting_hvc_call(void)
{
	struct arm_smccc_res res;
	int mkp_hvc_fast_call_id;
	int ret = -1;

	mkp_hvc_fast_call_id = MKP_HVC_CALL_ID(0, HVC_FUNC_ESS_1);
	mkp_smccc_hvc(mkp_hvc_fast_call_id, 0, 0, 0, 0, 0, 0, 0, &res);

	/* Success */
	if (res.a0 == 0)
		ret = 0;

	return ret;
}
