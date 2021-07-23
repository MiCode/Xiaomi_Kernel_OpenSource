/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS
#include "private/tmem_pr_fmt.h" PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#include "private/mld_helper.h"
#include "private/tmem_error.h"
#include "private/tmem_utils.h"
#include "private/tmem_priv.h"
#include "private/tmem_device.h"

#define REGMGR_LOCK() mutex_lock(&mgr_desc->lock)
#define REGMGR_UNLOCK() mutex_unlock(&mgr_desc->lock)

static int trusted_mem_region_poweron(struct trusted_mem_device *mem_device)
{
	int ret;
	u64 region_pa;
	u32 region_size;
	struct ssmr_operations *ssmr_ops = mem_device->ssmr_ops;
	struct peer_mgr_desc *peer_mgr = mem_device->peer_mgr;
	u32 ssmr_feature_id = mem_device->ssmr_feature_id;
	struct trusted_driver_operations *drv_ops = mem_device->peer_ops;
	struct trusted_peer_session *peer_mgr_data = &peer_mgr->peer_mgr_data;
	bool is_session_keep_alive =
		mem_device->configs.session_keep_alive_enable;

	ret = ssmr_ops->offline(&region_pa, &region_size, ssmr_feature_id,
				mem_device->dev_desc);
	if (ret) {
		pr_err("SSMR offline failed!\n");
		goto err_get_ssmr_failed;
	}

	ret = peer_mgr->mgr_sess_open(drv_ops, peer_mgr_data,
				      mem_device->dev_desc);
	if (ret && (ret != TMEM_MGR_SESSION_IS_ALREADY_OPEN)) {
		pr_err("open trusted mem session failed!\n");
		goto err_open_mtee_failed;
	}

	ret = peer_mgr->mgr_sess_mem_add(region_pa, region_size, drv_ops,
					 peer_mgr_data, mem_device->dev_desc);
	if (ret) {
		pr_err("add memory to trusted mem failed!\n");
		goto err_add_mem_failed;
	}

	return TMEM_OK;

err_add_mem_failed:
	peer_mgr->mgr_sess_close(is_session_keep_alive, drv_ops, peer_mgr_data,
				 mem_device->dev_desc);
err_open_mtee_failed:
	ssmr_ops->online(ssmr_feature_id, mem_device->dev_desc);
err_get_ssmr_failed:

	return ret;
}

static int trusted_mem_region_poweroff(struct trusted_mem_device *mem_device)
{
	int ret;
	struct ssmr_operations *ssmr_ops = mem_device->ssmr_ops;
	struct peer_mgr_desc *peer_mgr = mem_device->peer_mgr;
	u32 ssmr_feature_id = mem_device->ssmr_feature_id;
	struct trusted_driver_operations *drv_ops = mem_device->peer_ops;
	struct trusted_peer_session *peer_mgr_data = &peer_mgr->peer_mgr_data;
	bool is_session_keep_alive =
		mem_device->configs.session_keep_alive_enable;

	ret = peer_mgr->mgr_sess_mem_remove(drv_ops, peer_mgr_data,
					    mem_device->dev_desc);
	if (ret) {
		pr_err("reclaim memory from trusted mem failed!\n");
		return ret;
	}

	ret = peer_mgr->mgr_sess_close(is_session_keep_alive, drv_ops,
				       peer_mgr_data, mem_device->dev_desc);
	if (ret && (ret != TMEM_MGR_SESSION_IS_ALREADY_CLOSE)) {
		pr_err("close trusted mem session failed!\n");
		return ret;
	}

	ret = ssmr_ops->online(ssmr_feature_id, mem_device->dev_desc);
	if (ret) {
		pr_err("SSMR online failed!\n");
		return ret;
	}

	return TMEM_OK;
}

static bool is_region_on(enum REGMGR_REGION_STATE state)
{
	return (state == REGMGR_REGION_STATE_ON);
}

static void set_region_state(struct region_mgr_desc *mgr_desc,
			     enum REGMGR_REGION_STATE set_state)
{
	mgr_desc->state = set_state;

	if (mgr_desc->state == REGMGR_REGION_STATE_ON)
		mgr_desc->online_acc_count++;

#ifdef TCORE_PROFILING_SUPPORT
#ifdef TCORE_PROFILING_AUTO_DUMP
	if (mgr_desc->state == REGMGR_REGION_STATE_OFF)
		trusted_mem_core_profile_dump(mgr_desc->mem_device);
#endif
#endif

#if defined(CONFIG_MTK_SVP_DISABLE_SODI)
	if (mgr_desc->state == REGMGR_REGION_STATE_ON)
		spm_enable_sodi(false);
	else
		spm_enable_sodi(true);
#endif
}

static int regmgr_try_on(struct region_mgr_desc *mgr_desc,
			 enum TRUSTED_MEM_TYPE try_mem_type)
{
	struct trusted_mem_device *mem_device =
		get_trusted_mem_device(try_mem_type);

	pr_debug("%s:%d\n", __func__, __LINE__);

	if (is_region_on(mgr_desc->state)) {
		pr_debug("trusted mem is already onlined\n");
		return TMEM_OK;
	}

	if (trusted_mem_region_poweron(mem_device)) {
		pr_err("trusted mem poweron failed!\n");
		return TMEM_REGION_POWER_ON_FAILED;
	}

	pr_debug("set device:%d to busy\n", try_mem_type);

	mgr_desc->active_mem_type = try_mem_type;
	mgr_desc->mem_device = mem_device;
	mem_device->is_device_busy = true;
	set_region_state(mgr_desc, REGMGR_REGION_STATE_ON);
	return TMEM_OK;
}

static int regmgr_try_off(struct region_mgr_desc *mgr_desc)
{
	struct trusted_mem_device *mem_device =
		(struct trusted_mem_device *)mgr_desc->mem_device;

	pr_debug("%s:%d\n", __func__, __LINE__);

	if (!is_region_on(mgr_desc->state)) {
		pr_debug("trusted mem is already offlined\n");
		return TMEM_OK;
	}

	if (trusted_mem_region_poweroff(mem_device)) {
		pr_err("trusted mem poweroff failed!\n");
		return TMEM_REGION_POWER_OFF_FAILED;
	}

	pr_debug("set device:%d to idle\n", mem_device->mem_type);

	mem_device->is_device_busy = false;
	mgr_desc->active_mem_type = TRUSTED_MEM_INVALID;
	set_region_state(mgr_desc, REGMGR_REGION_STATE_OFF);
	return TMEM_OK;
}

static void regmgr_trigger_defer_off_work(struct region_mgr_desc *mgr_desc)
{
	queue_delayed_work(mgr_desc->defer_off_wq, &mgr_desc->defer_off_work,
			   msecs_to_jiffies(mgr_desc->defer_off_delay_ms));
}

static void regmgr_cancel_defer_off_work(struct delayed_work *work)
{
	cancel_delayed_work_sync(work);
}

static void regmgr_defer_off_handler(struct work_struct *work)
{
	struct region_mgr_desc *mgr_desc =
		container_of(work, struct region_mgr_desc, defer_off_work.work);

	REGMGR_LOCK();
	if (IS_ZERO(mgr_desc->valid_ref_count))
		regmgr_try_off(mgr_desc);
	REGMGR_UNLOCK();
}

bool get_device_busy_status(struct trusted_mem_device *mem_device)
{
	struct region_mgr_desc *mgr_desc = mem_device->reg_mgr;
	bool ret;

	REGMGR_LOCK();
	ret = mem_device->is_device_busy;
	REGMGR_UNLOCK();
	return ret;
}

bool is_regmgr_region_on(struct region_mgr_desc *mgr_desc)
{
	bool ret;

	REGMGR_LOCK();
	ret = is_region_on(mgr_desc->state);
	REGMGR_UNLOCK();
	return ret;
}

u64 get_regmgr_region_online_cnt(struct region_mgr_desc *mgr_desc)
{
	return mgr_desc->online_acc_count;
}

u32 get_regmgr_region_ref_cnt(struct region_mgr_desc *mgr_desc)
{
	return mgr_desc->valid_ref_count;
}

void regmgr_region_ref_inc(struct region_mgr_desc *mgr_desc,
			   enum TRUSTED_MEM_TYPE try_mem_type)
{
	REGMGR_LOCK();

	mgr_desc->valid_ref_count++;

	/* make sure reg is on in case if ref cnt is increased
	 * later than reg off
	 * - alloc (reg on)
	 * - free (success & ref cnt dec)
	 * - free (reg off) ==> reg will be off here
	 * - alloc (success & ref cnt inc) ==> we try to on here
	 */
	regmgr_try_on(mgr_desc, try_mem_type);

	REGMGR_UNLOCK();
}

void regmgr_region_ref_dec(struct region_mgr_desc *mgr_desc)
{
	REGMGR_LOCK();
	if (IS_ZERO(mgr_desc->valid_ref_count))
		pr_err("invalid regmgr ref cnt decrease\n");
	else
		mgr_desc->valid_ref_count--;
	REGMGR_UNLOCK();
}


int regmgr_online(struct region_mgr_desc *mgr_desc,
		  enum TRUSTED_MEM_TYPE try_mem_type)
{
	int ret = TMEM_OK;

	regmgr_cancel_defer_off_work(&mgr_desc->defer_off_work);

	REGMGR_LOCK();
	ret = regmgr_try_on(mgr_desc, try_mem_type);
	REGMGR_UNLOCK();
	return ret;
}

int regmgr_offline(struct region_mgr_desc *mgr_desc)
{
	regmgr_trigger_defer_off_work(mgr_desc);
	return TMEM_OK;
}

struct region_mgr_desc *
create_reg_mgr_desc(enum TRUSTED_MEM_TYPE register_type,
		    struct trusted_mem_device *mem_device)
{
	struct region_mgr_desc *t_mgr_desc;
	char wq_name[32];

	t_mgr_desc = mld_kmalloc(sizeof(struct region_mgr_desc), GFP_KERNEL);
	if (INVALID(t_mgr_desc)) {
		pr_err("%s:%d out of memory!\n", __func__, __LINE__);
		return NULL;
	}

	INIT_DELAYED_WORK(&t_mgr_desc->defer_off_work,
			  regmgr_defer_off_handler);
	mutex_init(&t_mgr_desc->lock);

	t_mgr_desc->state = REGMGR_REGION_STATE_OFF;
	t_mgr_desc->defer_off_delay_ms = REGMGR_REGION_DEFER_OFF_DELAY_MS;
	t_mgr_desc->online_acc_count = 0;
	t_mgr_desc->valid_ref_count = 0;
	t_mgr_desc->mem_device = NULL;
	t_mgr_desc->active_mem_type = TRUSTED_MEM_INVALID;

	snprintf(wq_name, 32, "tmem_regmgr_defer_off_%d", register_type);
	t_mgr_desc->defer_off_wq = create_singlethread_workqueue(wq_name);

	return t_mgr_desc;
}
