/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <asm/cacheflush.h>
#include <mach/memory.h>
#include <mach/scm.h>
#include <mach/msm_dcvs_scm.h>
#include <trace/events/mpdcvs_trace.h>

#define DCVS_CMD_REGISTER_CORE		2
#define DCVS_CMD_SET_ALGO_PARAM		3
#define DCVS_CMD_EVENT			4
#define DCVS_CMD_INIT			5
#define DCVS_CMD_SET_POWER_PARAM	6

struct scm_register_core {
	uint32_t core_id;
	phys_addr_t core_param_phy;
};

struct scm_algo {
	uint32_t core_id;
	phys_addr_t algo_phy;
};

struct scm_init {
	uint32_t phy;
	uint32_t size;
};

struct scm_pwr_param {
	uint32_t	core_id;
	phys_addr_t	pwr_param_phy;
	phys_addr_t	freq_phy;
	phys_addr_t	coeffs_phy;
};

struct msm_algo_param {
	enum msm_dcvs_algo_param_type		type;
	union {
		struct msm_dcvs_algo_param	dcvs_param;
		struct msm_mpd_algo_param	mpd_param;
	} u;
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

static void __msm_dcvs_flush_cache(void *v, size_t size)
{
	__cpuc_flush_dcache_area(v, size);
	outer_flush_range(virt_to_phys(v), virt_to_phys(v) + size);
}

int msm_dcvs_scm_register_core(uint32_t core_id,
		struct msm_dcvs_core_param *param)
{
	int ret = 0;
	struct scm_register_core reg_data;
	struct msm_dcvs_core_param *p = NULL;

	p = kzalloc(PAGE_ALIGN(sizeof(struct msm_dcvs_core_param)), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	memcpy(p, param, sizeof(struct msm_dcvs_core_param));

	reg_data.core_id = core_id;
	reg_data.core_param_phy = virt_to_phys(p);

	__msm_dcvs_flush_cache(p, sizeof(struct msm_dcvs_core_param));

	ret = scm_call(SCM_SVC_DCVS, DCVS_CMD_REGISTER_CORE,
			&reg_data, sizeof(reg_data), NULL, 0);

	kfree(p);

	return ret;
}
EXPORT_SYMBOL(msm_dcvs_scm_register_core);

int msm_dcvs_scm_set_algo_params(uint32_t core_id,
		struct msm_dcvs_algo_param *param)
{
	int ret = 0;
	struct scm_algo algo;
	struct msm_algo_param *p = NULL;

	p = kzalloc(PAGE_ALIGN(sizeof(struct msm_algo_param)), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	p->type = MSM_DCVS_ALGO_DCVS_PARAM;
	memcpy(&p->u.dcvs_param, param, sizeof(struct msm_dcvs_algo_param));

	algo.core_id = core_id;
	algo.algo_phy = virt_to_phys(p);

	__msm_dcvs_flush_cache(p, sizeof(struct msm_algo_param));

	ret = scm_call(SCM_SVC_DCVS, DCVS_CMD_SET_ALGO_PARAM,
			&algo, sizeof(algo), NULL, 0);

	kfree(p);

	return ret;
}
EXPORT_SYMBOL(msm_dcvs_scm_set_algo_params);

int msm_mpd_scm_set_algo_params(struct msm_mpd_algo_param *param)
{
	int ret = 0;
	struct scm_algo algo;
	struct msm_algo_param *p = NULL;

	p = kzalloc(PAGE_ALIGN(sizeof(struct msm_algo_param)), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	p->type = MSM_DCVS_ALGO_MPD_PARAM;
	memcpy(&p->u.mpd_param, param, sizeof(struct msm_mpd_algo_param));

	algo.core_id = 0;
	algo.algo_phy = virt_to_phys(p);

	__msm_dcvs_flush_cache(p, sizeof(struct msm_algo_param));

	ret = scm_call(SCM_SVC_DCVS, DCVS_CMD_SET_ALGO_PARAM,
			&algo, sizeof(algo), NULL, 0);

	kfree(p);

	return ret;
}
EXPORT_SYMBOL(msm_mpd_scm_set_algo_params);

int msm_dcvs_scm_set_power_params(uint32_t core_id,
		struct msm_dcvs_power_params *pwr_param,
		struct msm_dcvs_freq_entry *freq_entry,
		struct msm_dcvs_energy_curve_coeffs *coeffs)
{
	int ret = 0;
	struct scm_pwr_param pwr;
	struct msm_dcvs_power_params *pwrt = NULL;
	struct msm_dcvs_freq_entry *freqt = NULL;
	struct msm_dcvs_energy_curve_coeffs *coefft = NULL;

	pwrt = kzalloc(PAGE_ALIGN(sizeof(struct msm_dcvs_power_params)),
			GFP_KERNEL);
	if (!pwrt)
		return -ENOMEM;

	freqt = kzalloc(PAGE_ALIGN(sizeof(struct msm_dcvs_freq_entry)
				* pwr_param->num_freq),
			GFP_KERNEL);
	if (!freqt) {
		kfree(pwrt);
		return -ENOMEM;
	}

	coefft = kzalloc(PAGE_ALIGN(
				sizeof(struct msm_dcvs_energy_curve_coeffs)),
				GFP_KERNEL);
	if (!coefft) {
		kfree(pwrt);
		kfree(freqt);
		return -ENOMEM;
	}

	memcpy(pwrt, pwr_param, sizeof(struct msm_dcvs_power_params));
	memcpy(freqt, freq_entry,
			sizeof(struct msm_dcvs_freq_entry)*pwr_param->num_freq);
	memcpy(coefft, coeffs, sizeof(struct msm_dcvs_energy_curve_coeffs));

	__msm_dcvs_flush_cache(pwrt, sizeof(struct msm_dcvs_power_params));
	__msm_dcvs_flush_cache(freqt,
		sizeof(struct msm_dcvs_freq_entry) * pwr_param->num_freq);
	__msm_dcvs_flush_cache(coefft,
				sizeof(struct msm_dcvs_energy_curve_coeffs));

	pwr.core_id = core_id;
	pwr.pwr_param_phy = virt_to_phys(pwrt);
	pwr.freq_phy = virt_to_phys(freqt);
	pwr.coeffs_phy = virt_to_phys(coefft);

	ret = scm_call(SCM_SVC_DCVS, DCVS_CMD_SET_POWER_PARAM,
			&pwr, sizeof(pwr), NULL, 0);

	kfree(pwrt);
	kfree(freqt);
	kfree(coefft);

	return ret;
}
EXPORT_SYMBOL(msm_dcvs_scm_set_power_params);

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

	trace_msm_dcvs_scm_event(core_id, (int)event_id, param0, param1,
							*ret0, *ret1);

	return ret;
}
EXPORT_SYMBOL(msm_dcvs_scm_event);
