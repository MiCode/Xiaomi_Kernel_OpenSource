// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS
#include "private/tmem_pr_fmt.h" PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/version.h>
#include <linux/sizes.h>

#include "private/tmem_error.h"
#include "private/tmem_utils.h"
#include "private/tmem_device.h"
#include "private/tmem_priv.h"
#include "private/tmem_cfg.h"

static bool is_invalid_hooks(struct trusted_mem_device *mem_device)
{
	if (unlikely(INVALID(mem_device)))
		return true;
	if (unlikely(INVALID(mem_device->peer_mgr)))
		return true;
	if (unlikely(INVALID(mem_device->reg_mgr)))
		return true;
#ifdef TCORE_PROFILING_SUPPORT
	if (unlikely(INVALID(mem_device->profile_mgr)))
		return true;
#endif

	if (unlikely(INVALID(mem_device->peer_ops)))
		return true;
	if (unlikely(INVALID(mem_device->ssmr_ops)))
		return true;

	return false;
}

int tmem_core_session_open(enum TRUSTED_MEM_TYPE mem_type)
{
	struct trusted_mem_device *mem_device =
		get_trusted_mem_device(mem_type);

	if (unlikely(is_invalid_hooks(mem_device))) {
		pr_err("%s:%d %d:mem device may not be registered!\n", __func__,
		       __LINE__, mem_type);
		return TMEM_OPERATION_NOT_REGISTERED;
	}

	return mem_device->peer_mgr->mgr_sess_open(
		mem_device->peer_ops, &mem_device->peer_mgr->peer_mgr_data,
		mem_device->dev_desc);
}

int tmem_core_session_close(enum TRUSTED_MEM_TYPE mem_type)
{
	struct trusted_mem_device *mem_device =
		get_trusted_mem_device(mem_type);

	if (unlikely(is_invalid_hooks(mem_device))) {
		pr_err("%s:%d %d:mem device may not be registered!\n", __func__,
		       __LINE__, mem_type);
		return TMEM_OPERATION_NOT_REGISTERED;
	}

	return mem_device->peer_mgr->mgr_sess_close(
		mem_device->configs.session_keep_alive_enable,
		mem_device->peer_ops, &mem_device->peer_mgr->peer_mgr_data,
		mem_device->dev_desc);
}

int tmem_core_ssmr_allocate(enum TRUSTED_MEM_TYPE mem_type)
{
	u64 region_pa;
	u32 region_size;
	struct trusted_mem_device *mem_device =
		get_trusted_mem_device(mem_type);

	if (unlikely(is_invalid_hooks(mem_device))) {
		pr_err("%s:%d %d:mem device may not be registered!\n", __func__,
		       __LINE__, mem_type);
		return TMEM_OPERATION_NOT_REGISTERED;
	}

	return mem_device->ssmr_ops->offline(&region_pa, &region_size,
					     mem_device->ssmr_feature_id,
					     mem_device->dev_desc);
}

int tmem_core_ssmr_release(enum TRUSTED_MEM_TYPE mem_type)
{
	struct trusted_mem_device *mem_device =
		get_trusted_mem_device(mem_type);

	if (unlikely(is_invalid_hooks(mem_device))) {
		pr_err("%s:%d %d:mem device may not be registered!\n", __func__,
		       __LINE__, mem_type);
		return TMEM_OPERATION_NOT_REGISTERED;
	}

	return mem_device->ssmr_ops->online(mem_device->ssmr_feature_id,
					    mem_device->dev_desc);
}

static int min_chunk_size_check(enum TRUSTED_MEM_TYPE mem_type, u32 *size,
				struct trusted_mem_configs *cfg)
{
	if (cfg->minimal_chunk_size < cfg->phys_limit_min_alloc_size) {
		pr_err("wrong minimal phys size: 0x%x, expected sz:0x%x\n",
		       cfg->minimal_chunk_size, cfg->phys_limit_min_alloc_size);
		return TMEM_INVALID_PHYICAL_MIN_CHUNK_SIZE;
	}

	if (is_mtee_mchunks(mem_type)) {
		/* adjust size to multiple of minimal_chunk_size */
		u32 adjust_size = (((*size - 1) / cfg->minimal_chunk_size) + 1)
				  * cfg->minimal_chunk_size;
		pr_debug("change size from 0x%x to 0x%x\n", *size, adjust_size);
		*size = adjust_size;
	} else if (*size < cfg->minimal_chunk_size) {
		pr_err("wrong minimal dev size: 0x%x, expected sz:0x%x\n", size,
		       cfg->minimal_chunk_size);
		return TMEM_INVALID_DEVICE_MIN_CHUNK_SIZE;
	}

	return TMEM_OK;
}

static u32 get_ordered_size(enum TRUSTED_MEM_TYPE mem_type, u32 size,
			    struct trusted_mem_configs *cfg)
{
	int order = 0, order_size = 0;
	int blk_size = cfg->minimal_chunk_size;

	if (size < cfg->minimal_chunk_size)
		return cfg->minimal_chunk_size;

	if (is_mtee_mchunks(mem_type))
		return cfg->minimal_chunk_size;

	while (1) {
		order_size = blk_size << order;
		if (order_size >= size)
			break;
		order++;
	}

	return order_size;
}

static inline void alignment_update(enum TRUSTED_MEM_TYPE mem_type,
				    u32 *alignment, u32 size,
				    struct trusted_mem_configs *cfg)
{
	int ordered_size = get_ordered_size(mem_type, size, cfg);

	pr_debug("change alignment from 0x%x to 0x%x\n", *alignment,
		 ordered_size);
	*alignment = ordered_size;
}

static int alignment_adjust(enum TRUSTED_MEM_TYPE mem_type, u32 *alignment,
			    u32 size, struct trusted_mem_configs *cfg)
{
	if ((*alignment == 0) || (*alignment > size))
		alignment_update(mem_type, alignment, size, cfg);

	if (*alignment < size) {
#ifdef TMEM_SMALL_ALIGNMENT_AUTO_ADJUST
		alignment_update(mem_type, alignment, size, cfg);
#else
		pr_err("wrong requested alignment: 0x%x, sz:0x%x\n", *alignment,
		       size);
		return TMEM_INVALID_ALIGNMENT_REQUEST;
#endif
	}

	return TMEM_OK;
}

static int
parameter_checks_with_alignment_adjust(enum TRUSTED_MEM_TYPE mem_type,
				       u32 *alignment, u32 *size,
				       struct trusted_mem_configs *cfg)
{
	int ret;

	if (cfg->min_size_check_enable) {
		ret = min_chunk_size_check(mem_type, size, cfg);
		if (ret)
			return ret;
	}

	if (cfg->alignment_check_enable) {
		ret = alignment_adjust(mem_type, alignment, *size, cfg);
		if (ret)
			return ret;
	}
	return TMEM_OK;
}

int tmem_core_alloc_chunk(enum TRUSTED_MEM_TYPE mem_type, u32 alignment,
			  u32 size, u32 *refcount, u32 *sec_handle, u8 *owner,
			  u32 id, u32 clean)
{
	int ret = TMEM_OK;
	struct trusted_mem_device *mem_device =
		get_trusted_mem_device(mem_type);
	struct trusted_mem_configs *mem_cfg;

	if (unlikely(is_invalid_hooks(mem_device))) {
		pr_err("%s:%d %d:mem device may not be registered!\n", __func__,
		       __LINE__, mem_type);
		return TMEM_OPERATION_NOT_REGISTERED;
	}

	if (IS_ZERO(size)) {
		pr_err("[%d] invalid size: sz:0x%x\n", mem_type, size);
		return TMEM_GENERAL_ERROR;
	}

	pr_debug("[%d] alloc sz req is %d (0x%x), align 0x%x, clean: %d\n",
		 mem_type, size, size, alignment, clean);

	mem_cfg = &mem_device->configs;
	ret = parameter_checks_with_alignment_adjust(mem_type, &alignment,
						     &size, mem_cfg);
	if (unlikely(ret))
		return ret;

	ret = regmgr_online(mem_device->reg_mgr, mem_device->mem_type);
	if (unlikely(ret))
		return ret;

	ret = mem_device->peer_mgr->mgr_sess_mem_alloc(
		alignment, size, refcount, sec_handle, owner, id, clean,
		mem_device->peer_ops, &mem_device->peer_mgr->peer_mgr_data,
		mem_device->dev_desc);
	if (unlikely(ret)) {
		pr_err("[%d] alloc chunk failed:%d, sz:0x%x, align:0x%x\n",
		       mem_type, ret, size, alignment);
		regmgr_offline(mem_device->reg_mgr);
		return ret;
	}

	pr_debug("[%d] allocated handle is 0x%x\n", mem_type, *sec_handle);
	regmgr_region_ref_inc(mem_device->reg_mgr, mem_device->mem_type);
	return TMEM_OK;
}

int tmem_core_unref_chunk(enum TRUSTED_MEM_TYPE mem_type, u32 sec_handle,
			  u8 *owner, u32 id)
{
	int ret = TMEM_OK;
	struct trusted_mem_device *mem_device =
		get_trusted_mem_device(mem_type);

	if (unlikely(is_invalid_hooks(mem_device))) {
		pr_err("%s:%d %d:mem device may not be registered!\n", __func__,
		       __LINE__, mem_type);
		return TMEM_OPERATION_NOT_REGISTERED;
	}

	pr_debug("[%d] free handle is 0x%x\n", mem_type, sec_handle);

	if (unlikely(!is_regmgr_region_on(mem_device->reg_mgr))) {
		pr_err("[%d] regmgr region is still not online!\n", mem_type);
		return TMEM_REGION_IS_NOT_READY_BEFORE_MEM_FREE_OPERATION;
	}

	ret = mem_device->peer_mgr->mgr_sess_mem_free(
		sec_handle, owner, id, mem_device->peer_ops,
		&mem_device->peer_mgr->peer_mgr_data, mem_device->dev_desc);
	if (unlikely(ret)) {
		pr_err("[%d] free chunk failed!\n", mem_type);
		return ret;
	}

	regmgr_region_ref_dec(mem_device->reg_mgr);
	regmgr_offline(mem_device->reg_mgr);
	return TMEM_OK;
}

bool tmem_core_is_regmgr_region_on(enum TRUSTED_MEM_TYPE mem_type)
{
	struct trusted_mem_device *mem_device =
		get_trusted_mem_device(mem_type);
	bool is_phy_region_on;
	bool is_dev_busy;

	if (unlikely(INVALID(mem_device)))
		return false;

	is_phy_region_on = is_regmgr_region_on(mem_device->reg_mgr);
	is_dev_busy = get_device_busy_status(mem_device);

	pr_debug("device:%d is %s(%d) (phys state:%d, active mem:%d)\n",
		 mem_type, is_dev_busy ? "busy" : "not busy", is_dev_busy,
		 is_phy_region_on, mem_device->reg_mgr->active_mem_type);
	return is_dev_busy;
}

u64 tmem_core_get_regmgr_region_online_cnt(enum TRUSTED_MEM_TYPE mem_type)
{
	struct trusted_mem_device *mem_device =
		get_trusted_mem_device(mem_type);

	if (unlikely(INVALID(mem_device)))
		return 0;

	return get_regmgr_region_online_cnt(mem_device->reg_mgr);
}

u32 tmem_core_get_regmgr_region_ref_cnt(enum TRUSTED_MEM_TYPE mem_type)
{
	struct trusted_mem_device *mem_device =
		get_trusted_mem_device(mem_type);

	if (unlikely(INVALID(mem_device)))
		return 0;

	return get_regmgr_region_ref_cnt(mem_device->reg_mgr);
}

int tmem_core_regmgr_online(enum TRUSTED_MEM_TYPE mem_type)
{
	struct trusted_mem_device *mem_device =
		get_trusted_mem_device(mem_type);

	if (unlikely(INVALID(mem_device)))
		return TMEM_OPERATION_NOT_REGISTERED;

	return regmgr_online(mem_device->reg_mgr, mem_device->mem_type);
}

int tmem_core_regmgr_offline(enum TRUSTED_MEM_TYPE mem_type)
{
	struct trusted_mem_device *mem_device =
		get_trusted_mem_device(mem_type);

	if (unlikely(INVALID(mem_device)))
		return TMEM_OPERATION_NOT_REGISTERED;

	return regmgr_offline(mem_device->reg_mgr);
}

bool tmem_core_is_device_registered(enum TRUSTED_MEM_TYPE mem_type)
{
	struct trusted_mem_device *mem_device =
		get_trusted_mem_device(mem_type);

	if (unlikely(INVALID(mem_device)))
		return false;

	return true;
}

u32 tmem_core_get_min_chunk_size(enum TRUSTED_MEM_TYPE mem_type)
{
	struct trusted_mem_device *mem_device =
		get_trusted_mem_device(mem_type);

	if (unlikely(INVALID(mem_device)))
		return SZ_4K;

	return mem_device->configs.minimal_chunk_size;
}

static int get_max_pool_size(enum TRUSTED_MEM_TYPE mem_type)
{
	switch (mem_type) {
	case TRUSTED_MEM_SVP:
		return SZ_256M;
	case TRUSTED_MEM_PROT:
		return SZ_128M;
	case TRUSTED_MEM_WFD:
		return SZ_64M;
	case TRUSTED_MEM_2D_FR:
		return SZ_16M;
	case TRUSTED_MEM_HAPP:
		return SZ_16M;
	case TRUSTED_MEM_HAPP_EXTRA:
		return (SZ_32M + SZ_64M);
	case TRUSTED_MEM_SDSP:
		return SZ_16M;
	case TRUSTED_MEM_SDSP_SHARED:
		return SZ_16M;
	default:
		return SZ_4K;
	}
}

u32 tmem_core_get_max_pool_size(enum TRUSTED_MEM_TYPE mem_type)
{
	struct trusted_mem_device *mem_device =
		get_trusted_mem_device(mem_type);
	u32 mem_size;

	if (unlikely(INVALID(mem_device)))
		return SZ_4K;

	mem_size = mem_device->peer_mgr->peer_mgr_data.mem_size_runtime;
	if (IS_ZERO(mem_size))
		return get_max_pool_size(mem_type);
	return mem_size;
}

bool tmem_core_get_region_info(enum TRUSTED_MEM_TYPE mem_type, u64 *pa,
			       u32 *size)
{
	struct trusted_mem_device *mem_device =
		get_trusted_mem_device(mem_type);

	if (unlikely(INVALID(mem_device)))
		return false;

	*pa = mem_device->peer_mgr->peer_mgr_data.mem_pa_start;
	*size = mem_device->peer_mgr->peer_mgr_data.mem_size;

	pr_debug("[%d] region pa: 0x%llx, sz: 0x%x\n", mem_type, *pa, *size);
	return true;
}

bool is_mtee_mchunks(enum TRUSTED_MEM_TYPE mem_type)
{
	switch (mem_type) {
	case TRUSTED_MEM_PROT:
	case TRUSTED_MEM_HAPP:
	case TRUSTED_MEM_HAPP_EXTRA:
	case TRUSTED_MEM_SDSP:
		return true;
	case TRUSTED_MEM_SDSP_SHARED:
#if IS_ENABLED(CONFIG_MTK_SDSP_SHARED_MEM_SUPPORT)                             \
	&& (IS_ENABLED(CONFIG_MTK_SDSP_SHARED_PERM_MTEE_TEE)                   \
	    || IS_ENABLED(CONFIG_MTK_SDSP_SHARED_PERM_VPU_MTEE_TEE))
		return true;
#else
		return false;
#endif
	default:
		return false;
	}
}
