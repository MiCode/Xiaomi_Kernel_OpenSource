/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/memory_alloc.h>
#include <mach/memory.h>
#include <mach/scm.h>
#include <mach/msm_dcvs_scm.h>

#define DCVS_CMD_CREATE_GROUP		1
#define DCVS_CMD_REGISTER_CORE		2
#define DCVS_CMD_SET_ALGO_PARAM		3
#define DCVS_CMD_EVENT			4
#define DCVS_CMD_INIT			5

struct scm_register_core {
	uint32_t core_id;
	uint32_t group_id;
	phys_addr_t core_param_phy;
	phys_addr_t freq_phy;
};

struct scm_algo {
	uint32_t core_id;
	phys_addr_t algo_phy;
};

struct scm_init {
	uint32_t phy;
	uint32_t size;
};

int msm_dcvs_scm_init(size_t size)
{
	int ret = 0;
	struct scm_init init;
	uint32_t p = 0;

	/* Allocate word aligned non-cacheable memory */
	p = allocate_contiguous_ebi_nomap(size, 4);
	if (!p)
		return -ENOMEM;

	init.phy = p;
	init.size = size;

	ret = scm_call(SCM_SVC_DCVS, DCVS_CMD_INIT,
			&init, sizeof(init), NULL, 0);

	/* Not freed if the initialization succeeds */
	if (ret)
		free_contiguous_memory_by_paddr(p);

	return ret;
}
EXPORT_SYMBOL(msm_dcvs_scm_init);

int msm_dcvs_scm_create_group(uint32_t id)
{
	int ret = 0;

	ret = scm_call(SCM_SVC_DCVS, DCVS_CMD_CREATE_GROUP,
			&id, sizeof(uint32_t), NULL, 0);

	return ret;
}
EXPORT_SYMBOL(msm_dcvs_scm_create_group);

int msm_dcvs_scm_register_core(uint32_t core_id, uint32_t group_id,
		struct msm_dcvs_core_param *param,
		struct msm_dcvs_freq_entry *freq)
{
	int ret = 0;
	struct scm_register_core reg_data;
	struct msm_dcvs_core_param *p = NULL;
	struct msm_dcvs_freq_entry *f = NULL;

	p = kzalloc(PAGE_ALIGN(sizeof(struct msm_dcvs_core_param)), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	f = kzalloc(PAGE_ALIGN(sizeof(struct msm_dcvs_freq_entry) *
				param->num_freq), GFP_KERNEL);
	if (!f) {
		kfree(p);
		return -ENOMEM;
	}

	memcpy(p, param, sizeof(struct msm_dcvs_core_param));
	memcpy(f, freq, sizeof(struct msm_dcvs_freq_entry) * param->num_freq);

	reg_data.core_id = core_id;
	reg_data.group_id = group_id;
	reg_data.core_param_phy = virt_to_phys(p);
	reg_data.freq_phy = virt_to_phys(f);

	ret = scm_call(SCM_SVC_DCVS, DCVS_CMD_REGISTER_CORE,
			&reg_data, sizeof(reg_data), NULL, 0);

	kfree(f);
	kfree(p);

	return ret;
}
EXPORT_SYMBOL(msm_dcvs_scm_register_core);

int msm_dcvs_scm_set_algo_params(uint32_t core_id,
		struct msm_dcvs_algo_param *param)
{
	int ret = 0;
	struct scm_algo algo;
	struct msm_dcvs_algo_param *p = NULL;

	p = kzalloc(PAGE_ALIGN(sizeof(struct msm_dcvs_algo_param)), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	memcpy(p, param, sizeof(struct msm_dcvs_algo_param));

	algo.core_id = core_id;
	algo.algo_phy = virt_to_phys(p);

	ret = scm_call(SCM_SVC_DCVS, DCVS_CMD_SET_ALGO_PARAM,
			&algo, sizeof(algo), NULL, 0);

	kfree(p);

	return ret;
}
EXPORT_SYMBOL(msm_dcvs_scm_set_algo_params);

int msm_dcvs_scm_event(uint32_t core_id,
		enum msm_dcvs_scm_event event_id,
		uint32_t param0, uint32_t param1,
		uint32_t *ret0, uint32_t *ret1)
{
	int ret = -EINVAL;

	if (!ret0 || !ret1)
		return ret;

	ret = scm_call_atomic4_3(SCM_SVC_DCVS, DCVS_CMD_EVENT,
			core_id, event_id, param0, param1, ret0, ret1);

	return ret;
}
EXPORT_SYMBOL(msm_dcvs_scm_event);
