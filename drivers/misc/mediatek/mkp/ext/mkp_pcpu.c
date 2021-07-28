// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/cpumask.h>
#include <linux/android_debug_symbols.h>
#include "mkp_api.h"
#include "debug.h"

DEBUG_SET_LEVEL(DEBUG_LEVEL_ERR);

#define POLICY_CHAR_FOR_PCPU	(NO_MAP_TO_DEVICE |	\
				 NO_UPGRADE_TO_EXEC |	\
				 HANDLE_PERMANENT |	\
				 ACTION_WARNING)

static int policy;

static int start_protect(void)
{
	unsigned long pcpu_unit_size = 0, ppcpu_unit_size = 0, prev_pcpu_offset = 0;
	unsigned long *p__per_cpu_start = NULL;
	unsigned long per_cpu_base_addr;
	int i, nr_cpu = 0;
	int handle;
	phys_addr_t phys_addr;
	int ret;

	/* Not suitable for NUMA */
	if (IS_ENABLED(CONFIG_NUMA))
		return -EPERM;

	/* Invalid policy */
	if (policy <= 0)
		return -EINVAL;

	/* Try to get __per_cpu_start */
	p__per_cpu_start = android_debug_symbol(ADS_PER_CPU_START);
	if (IS_ERR(p__per_cpu_start))
		return -EFAULT;

	for_each_possible_cpu(i) {
		/* Record pcpu unit size */
		if (prev_pcpu_offset != 0)
			pcpu_unit_size = __per_cpu_offset[i] - prev_pcpu_offset;

		/* Unexpected unit_size */
		if (ppcpu_unit_size != 0 && pcpu_unit_size != ppcpu_unit_size) {
			MKP_ERR("%s:%d unexpected unit size, prev(%lu) curr(%lu)\n",
				__func__, __LINE__, ppcpu_unit_size, pcpu_unit_size);
			goto err;
		}

		prev_pcpu_offset = __per_cpu_offset[i];
		ppcpu_unit_size = pcpu_unit_size;
		nr_cpu++;
	}

	per_cpu_base_addr = __per_cpu_offset[0] + (unsigned long)p__per_cpu_start;

	/* 1. Create handle */
	phys_addr = virt_to_phys((void *)per_cpu_base_addr);
	handle = mkp_create_handle(policy, (unsigned long)phys_addr,
			round_down((pcpu_unit_size * nr_cpu), PAGE_SIZE));
	if (handle == 0) {
		MKP_ERR("%s:%d: create handle fail\n", __func__, __LINE__);
		goto err;
	}

	/* 2. Map it as NX and apply setting according to NO_MAP_TO_DEVICE */
	ret = mkp_set_mapping_nx(policy, handle);
	if (ret != 0) {
		MKP_ERR("%s:%d: set_mapping_nx fail, ret(%d)\n", __func__, __LINE__, ret);
		goto err;
	}

	MKP_INFO("%s: completed\n", __func__);

err:
	return 0;
}

int __init mkp_protect_percpu_data(void)
{
	int ret;

	/* 0. Request new policy */
	policy = mkp_request_new_policy(POLICY_CHAR_FOR_PCPU);
	if (policy < 0)
		return -1;

	/* 0-1. After requesting new policy, please enable policy control */
	ret = mkp_set_ext_policy(policy);
	if (ret != 0)
		return -2;

	ret = start_protect();
	if (ret != 0)
		return -3;

	return 0;
}
