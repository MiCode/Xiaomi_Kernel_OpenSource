// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 * Author: Kuan-Ying Lee <Kuan-Ying.Lee@mediatek.com>
 */

#include "mkp_api.h"

const uint64_t subscribe = 0x6d6b7021;	/* mkp! */
uint64_t *grant_ticket __ro_after_init;

/* Preparation for grant ticket */
bool __init prepare_grant_ticket(void)
{
	struct page *tpage;
	void *taddr;

	tpage = vmalloc_to_page((void *)&subscribe);
	taddr = vmap(&tpage, 1, VM_MAP, PAGE_KERNEL);
	if (!taddr)
		return false;

	grant_ticket = (uint64_t *)((uint64_t)taddr + ((uint64_t)&subscribe & ~PAGE_MASK));

	return true;
}

void __init mkp_set_policy(u32 policy)
{
	// set policy control
	set_policy(policy);
}

int __init mkp_set_ext_policy(uint32_t policy)
{
	// set extended policy
	return set_ext_policy(policy);
}

int mkp_lookup_mapping_entry(uint32_t policy, uint32_t handle,
	unsigned long *entry_size, unsigned long *permission)
{
	int ret = -1;

	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return ret;

	ret = mkp_lookup_mapping_entry_hvc_call(policy,
		handle, entry_size, permission);
	return ret;
}

int mkp_request_new_policy(unsigned long policy_char)
{
	int ret = 0;

	ret = mkp_req_new_policy_hvc_call(policy_char);

	return ret;
}

int mkp_change_policy_action(uint32_t policy, unsigned long policy_char_action)
{
	int ret = 0;

	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return -1;
	if (policy_char_action & (~ACTION_BITS))
		return -1;

	mkp_policy_action[policy] &= (~ACTION_BITS);
	mkp_policy_action[policy] |= policy_char_action;

	ret = mkp_change_policy_action_hvc_call(policy, policy_char_action);

	if (ret == 0)
		return 0;
	else
		return -1;
}

int mkp_request_new_specified_policy(unsigned long policy_char, uint32_t specified_policy)
{
	int ret = 0;

	ret = mkp_req_new_specified_policy_hvc_call(policy_char, specified_policy);

	return ret;
}

uint32_t mkp_create_handle(uint32_t policy, unsigned long ipa, unsigned long size)
{
	uint32_t handle = 0;

	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return 0;

	// hvc call to get handle
	handle = mkp_create_handle_hvc_call(policy, ipa, size);

	return handle;
}
int mkp_destroy_handle(uint32_t policy, uint32_t handle)
{
	int ret = -1;

	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return ret;

	ret = mkp_destroy_handle_hvc_call(policy, handle);
	return ret;
}

uint32_t mkp_create_ro_sharebuf(uint32_t policy, unsigned long size, struct page **pages)
{
	uint32_t handle = 0;
	phys_addr_t ipa;
	int ret = -1;
	struct page *l_pages = NULL;
	unsigned int order = get_order(size);

	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return 0;

	l_pages = alloc_pages(GFP_KERNEL | __GFP_ZERO, order);
	if (l_pages == NULL)
		return 0;
	ipa = page_to_phys(l_pages);
	*pages = l_pages;
	handle = mkp_create_handle(policy, ipa, size);
	if (handle == 0) {
		__free_pages(l_pages, order);
		return 0;
	}

	ret = do_secure_ops(policy, handle, mkp_set_mapping_ro_hvc_call);
	if (ret == -1) {
		ret = mkp_destroy_handle(policy, handle);
		__free_pages(l_pages, order);
		return 0;
	}
	*pages = l_pages;
	return handle;
}

uint32_t mkp_create_wo_sharebuf(uint32_t policy, unsigned long size, struct page **pages)
{
	uint32_t handle = 0;
	phys_addr_t ipa;
	int ret = -1;
	struct page *l_pages = NULL;
	unsigned int order = get_order(size);

	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return 0;

	l_pages = alloc_pages(GFP_KERNEL | __GFP_ZERO, order);
	if (l_pages == NULL)
		return 0;
	ipa = page_to_phys(l_pages);
	*pages = l_pages;
	handle = mkp_create_handle(policy, ipa, size);
	if (handle == 0) {
		__free_pages(l_pages, order);
		return 0;
	}

	ret = do_secure_ops(policy, handle, mkp_set_mapping_rw_hvc_call);
	if (ret == -1) {
		ret = mkp_destroy_handle(policy, handle);
		__free_pages(l_pages, order);
		return 0;
	}
	*pages = l_pages;
	return handle;
}

int mkp_configure_sharebuf(uint32_t policy, uint32_t handle, uint32_t type,
	unsigned long nr_entries, unsigned long size)
{
	int ret = -1;

	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return ret;

	ret = mkp_configure_sharebuf_hvc_call(policy, handle,
		type, nr_entries, size);
	return ret;
}
int mkp_update_sharebuf_1_argu(uint32_t policy, uint32_t handle, unsigned long index, unsigned long a1)
{
	int ret = -1;

	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return ret;

	ret = mkp_update_sharebuf_1_argu_hvc_call(policy, handle, index, a1);
	return ret;
}
int mkp_update_sharebuf_2_argu(uint32_t policy, uint32_t handle, unsigned long index, unsigned long a1, unsigned long a2)
{
	int ret = -1;

	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return ret;

	ret = mkp_update_sharebuf_2_argu_hvc_call(policy, handle, index, a1, a2);
	return ret;
}
int mkp_update_sharebuf_3_argu(uint32_t policy, uint32_t handle, unsigned long index, unsigned long a1, unsigned long a2, unsigned long a3)
{
	int ret = -1;

	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return ret;

	ret = mkp_update_sharebuf_3_argu_hvc_call(policy, handle, index, a1, a2, a3);
	return ret;
}
int mkp_update_sharebuf_4_argu(uint32_t policy, uint32_t handle, unsigned long index, unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4)
{
	int ret = -1;

	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return ret;

	ret = mkp_update_sharebuf_4_argu_hvc_call(policy, handle, index, a1, a2, a3, a4);
	return ret;
}
int mkp_update_sharebuf_5_argu(uint32_t policy, uint32_t handle, unsigned long index, 
	unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5)
{
	int ret = -1;

	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return ret;

	ret = mkp_update_sharebuf_5_argu_hvc_call(policy, handle, index, a1, a2, a3, a4, a5);
	return ret;
}
int mkp_update_sharebuf(uint32_t policy, uint32_t handle, unsigned long index/*tag*/, unsigned long ipa)
{
	int ret = -1;

	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return ret;

	ret = mkp_update_sharebuf_hvc_call(policy, handle, index, ipa);
	return ret;
}
