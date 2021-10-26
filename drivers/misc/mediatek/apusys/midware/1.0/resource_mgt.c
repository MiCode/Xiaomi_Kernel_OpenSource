/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#ifdef CONFIG_PM_SLEEP
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#endif

#include "mdw_cmn.h"
#include "resource_mgt.h"
#include "cmd_parser.h"
#include "thread_pool.h"
#include "scheduler.h"
#include "sched_deadline.h"
#include "sched_normal.h"

extern struct dentry *apusys_dbg_device;
struct apusys_res_mgr g_res_mgr;

static char dev_type_string[APUSYS_DEVICE_RT][APUSYS_DEV_NAME_SIZE] = {
	"none",
	"sample",
	"mdla",
	"vpu",
	"edma",
	"wait",
};

#ifdef CONFIG_PM_SLEEP
static struct wakeup_source *apusys_secure_ws;
static uint32_t ws_count;
static struct mutex ws_mutex;
#endif

//----------------------------------------------
static void secure_ws_init(void)
{
#ifdef CONFIG_PM_SLEEP
	ws_count = 0;
	mutex_init(&ws_mutex);
	apusys_secure_ws = wakeup_source_register(NULL, "apusys_secure");
	if (!apusys_secure_ws)
		mdw_drv_err("apusys sched wakelock register fail!\n");
#else
	mdw_drv_debug("not support pm wakelock\n");
#endif
}

void secure_ws_lock(void)
{
#ifdef CONFIG_PM_SLEEP
	mutex_lock(&ws_mutex);
	//mdw_drv_debug("wakelock count(%d)\n", ws_count);
	if (apusys_secure_ws && !ws_count) {
		mdw_flw_debug("lock wakelock\n");
		__pm_stay_awake(apusys_secure_ws);
	}
	ws_count++;
	mutex_unlock(&ws_mutex);
#else
	mdw_flw_debug("not support pm wakelock\n");
#endif
}

void secure_ws_unlock(void)
{
#ifdef CONFIG_PM_SLEEP
	mutex_lock(&ws_mutex);
	ws_count--;
	if (apusys_secure_ws && !ws_count) {
		mdw_flw_debug("unlock wakelock\n");
		__pm_relax(apusys_secure_ws);
	}
	mutex_unlock(&ws_mutex);
#else
	mdw_flw_debug("not support pm wakelock\n");
#endif
}
struct mem_ctx_mgr {
	struct mutex mtx;
	unsigned long ctx[BITS_TO_LONGS(32)];
};

//----------------------------------------------
struct apusys_res_table *res_get_table(int type)
{
	if (type <= APUSYS_DEVICE_NONE || type >= APUSYS_DEVICE_MAX)
		return NULL;

	return g_res_mgr.tab[type];
}

struct apusys_res_mgr *res_get_mgr(void)
{
	return &g_res_mgr;
}

//----------------------------------------------
/*
 * input dev_type is device table's type,
 * allocated by resource mgt
 */
int res_dbg_tab_init(struct apusys_res_table *tab)
{
	int ret = 0;
	struct dentry *res_dbg_devq;

	/* check argument */
	if (tab == NULL)
		return -EINVAL;

	/* check queue dir */
	ret = IS_ERR_OR_NULL(apusys_dbg_device);
	if (ret) {
		mdw_drv_err("failed to get queue dir.\n");
		return -EINVAL;
	}

	/* check device and dbg dir */
	if (tab->dev_list[0].dev == NULL || tab->dbg_dir != NULL)
		return -ENODEV;

	/* create with dev type */
	tab->dbg_dir = debugfs_create_dir(tab->name,
		apusys_dbg_device);

	/* create queue */
	res_dbg_devq = debugfs_create_u32("queue", 0444,
		tab->dbg_dir, &tab->normal_task_num);

	return ret;
}

//----------------------------------------------
// cmd related functions
int res_get_device_num(int dev_type)
{
	struct apusys_res_table *tab = NULL;

	tab = res_get_table(dev_type);
	if (tab == NULL)
		return 0;

	return tab->dev_num;
}

uint64_t res_get_dev_support(void)
{
	uint64_t ret = 0;
	int res_max = 0, ret_max = 0;

	res_max = sizeof(g_res_mgr.dev_support);
	ret_max = sizeof(uint64_t);

	if (res_max > ret_max)
		mdw_drv_err("device support overflow\n");

	memcpy(&ret, g_res_mgr.dev_support,
		res_max < ret_max ? res_max : ret_max);

	return ret;
}

int insert_subcmd(struct apusys_subcmd *sc)
{
	struct apusys_res_table *tab = NULL;
	int ret = 0;

	/* get resource table */
	tab = res_get_table(sc->type);
	if (tab == NULL) {
		mdw_drv_err("no device(%d) available\n", sc->type);
		return -EINVAL;
	}

	mdw_flw_debug("insert 0x%llx-#%d to q(%d/%d/%llu)\n",
		sc->par_cmd->cmd_id,
		sc->idx,
		sc->type,
		sc->par_cmd->hdr->priority,
		sc->period);

	/* get type's queue */
	sc->state = CMD_STATE_READY;

	if (sc->period) { /* Deadline Queue */
		ret = deadline_task_insert(sc);
	} else { /* Priority Queue */
		ret = normal_task_insert(sc);
		if (ret) {
			mdw_drv_err("insert 0x%llx-#%d to nq(%d/%d/%llu)\n",
				sc->par_cmd->cmd_id,
				sc->idx,
				sc->type,
				sc->par_cmd->hdr->priority,
				sc->period);
		}
	}

	/* if insert success, mark cmd exist */
	if (!ret) {
		bitmap_set(g_res_mgr.cmd_exist, sc->type, 1);
		complete(&g_res_mgr.sched_comp);
	}

	return ret;
}

int insert_subcmd_lock(struct apusys_subcmd *sc)
{
	int ret = 0;

	mutex_lock(&g_res_mgr.mtx);
	/* delete subcmd node from ce list */
	ret = insert_subcmd(sc);
	mutex_unlock(&g_res_mgr.mtx);

	return ret;
}

int pop_subcmd(int type, struct apusys_subcmd **isc)
{
	struct apusys_res_table *tab = NULL;
	int ret = 0;
	struct apusys_subcmd *sc = NULL;

	/* get resource table */
	tab = res_get_table(type);
	if (tab == NULL) {
		mdw_drv_err("no device(%d) available\n", type);
		return -EINVAL;
	}

	sc = deadline_task_pop(type);
	if (sc) { /* pop cmd from deadline queue */
		mdw_flw_debug("pop 0x%llx-#%d from deadline_q(%d/%llu)\n",
				sc->par_cmd->cmd_id,
				sc->idx,
				sc->type,
				sc->deadline);
	} else { /* deadline queue is empty, pop cmd from priority queue */
		sc = normal_task_pop(type);
		if (sc) {
			mdw_flw_debug("pop 0x%llx-#%d from nq(%d/%d)\n",
				sc->par_cmd->cmd_id,
				sc->idx,
				sc->type,
				sc->par_cmd->hdr->priority);
		}
	}

	/* check if both deadline/priority queue are empty */
	if (normal_task_empty(type) && deadline_task_empty(type)) {
		mdw_flw_debug("device(%d) cmd empty\n", type);
		bitmap_clear(g_res_mgr.cmd_exist, type, 1);
	}

	/* assign subcmd */
	*isc = sc;
	if (sc == NULL) {
		mdw_drv_err("pop sc(%d) from queue fail\n", type);
		ret = -ENODATA;
	}

	return ret;
}

int delete_subcmd(struct apusys_subcmd *sc)
{
	int ret = 0;

	if (sc == NULL)
		return -EINVAL;

	mdw_flw_debug("remove 0x%llx-#%d q(%d/%d)\n",
		sc->par_cmd->cmd_id,
		sc->idx,
		sc->type,
		sc->par_cmd->hdr->priority);

	/* remove from queue */
	if (sc->period != 0)
		ret = deadline_task_remove(sc);
	else
		ret = normal_task_remove(sc);

	/* cmd_exist check */
	if (normal_task_empty(sc->type) && deadline_task_empty(sc->type))
		bitmap_clear(g_res_mgr.cmd_exist, sc->type, 1);

	if (ret) {
		mdw_drv_err("remove 0x%llx-#%d nq(%d/%d) fail\n",
			sc->par_cmd->cmd_id,
			sc->idx,
			sc->type,
			sc->par_cmd->hdr->priority);
	}

	return ret;
}

int delete_subcmd_lock(struct apusys_subcmd *sc)
{
	int ret = 0;

	mutex_lock(&g_res_mgr.mtx);
	ret = delete_subcmd(sc);
	mutex_unlock(&g_res_mgr.mtx);

	return ret;
}

//----------------------------------------------
// device related functions
int put_apusys_device(struct apusys_dev_info *dev_info)
{
	struct apusys_res_table *tab = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_dev_aquire *acq = NULL;
	//struct apusys_dev_info *dev_info = NULL;

	if (dev_info == NULL)
		return -EINVAL;

	if (dev_info->dev->dev_type >= APUSYS_DEVICE_MAX) {
		mdw_drv_err("put dev(%d/%d) invalid\n",
			dev_info->dev->dev_type, APUSYS_DEVICE_MAX);
		return -ENODEV;
	}

	tab = res_get_table(dev_info->dev->dev_type);
	/* can't find info, unlock and return fail */
	if (tab == NULL) {
		mdw_drv_err("no dev(%d) available\n", dev_info->dev->dev_type);
		return -ENODEV;
	}

	mdw_lne_debug();
	/* query list to find mem in apusys user */
	list_for_each_safe(list_ptr, tmp, &tab->acq_list) {
		acq = list_entry(list_ptr, struct apusys_dev_aquire, tab_list);
		mdw_flw_debug("dev(%s-#%d) has acquire(%p/%d), put device\n",
			dev_info->name, dev_info->dev->idx, acq, acq->is_done);
		if (acq->is_done == 0) {
			mdw_lne_debug();

			dev_info->cmd_id = 0;
			dev_info->sc_idx = 0;
			dev_info->cur_owner = acq->owner;
			acq->acq_num++;
			acq->acq_bitmap |= (1ULL << dev_info->dev->idx);
			mdw_flw_debug("dev(%s-#%d) add to acq(%d/%d)\n",
				dev_info->name, dev_info->dev->idx,
				acq->acq_num, acq->target_num);
			if (acq->acq_num == acq->target_num)
				acq->is_done = 1;
			list_add_tail(&dev_info->acq_list, &acq->dev_info_list);
			mdw_lne_debug();
			if (acq->is_done)
				complete(&acq->comp);
			complete(&g_res_mgr.sched_comp);
			return 0;
		}
		mdw_lne_debug();
	}

	/* get idle device from bitmap */
	mdw_flw_debug("put dev(%s-#%d/%d) ok\n",
		dev_info->name, dev_info->dev->idx, tab->dev_num);
	dev_info->cur_owner = APUSYS_DEV_OWNER_NONE;
	dev_info->cmd_id = 0;
	dev_info->sc_idx = 0;
	dev_info->is_deadline = false;
	bitmap_clear(tab->dev_status, dev_info->dev->idx, 1); // clear status
	tab->available_num++;

	bitmap_set(g_res_mgr.dev_exist, tab->dev_type, 1);
	complete(&g_res_mgr.sched_comp);

	mdw_lne_debug();

	return 0;
}

int put_device_lock(struct apusys_dev_info *dev_info)
{
	int ret = 0;

	mutex_lock(&g_res_mgr.mtx);
	ret = put_apusys_device(dev_info);
	mutex_unlock(&g_res_mgr.mtx);

	return ret;
}

int acq_device_try(struct apusys_dev_aquire *acq)
{
	struct apusys_res_table *tab = NULL;
	struct apusys_res_table *tmp_tab = NULL;

	unsigned long bit_idx = 0, idx = 0;
	unsigned long first_bit_idx = 0;
	unsigned long next_bit_idx = 0;

	int ret_num = 0, i = 0;
	unsigned long dev_status[BITS_TO_LONGS(APUSYS_DEV_TABLE_MAX)];


	/* check arguments */
	if (acq == NULL || acq->target_num > APUSYS_DEV_TABLE_MAX) {
		mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}

	acq->acq_num = 0;
	acq->acq_bitmap = 0;

	/* get apusys_res_table*/
	tab = res_get_table(acq->dev_type);
	/* can't find info, unlock and return fail */
	if (tab == NULL) {
		mdw_drv_err("no device(%d) available\n", acq->dev_type);
		return -EINVAL;
	}

	ret_num = acq->target_num < tab->available_num
		? acq->target_num : tab->available_num;


	bitmap_copy(dev_status, tab->dev_status, APUSYS_DEVICE_MAX);

	if (ret_num == 1) {
		first_bit_idx = tab->dev_num;
		if (acq->dev_type > APUSYS_DEVICE_RT)
			tmp_tab =
			  res_get_table(acq->dev_type - APUSYS_DEVICE_RT);
		else
			tmp_tab =
			  res_get_table(acq->dev_type + APUSYS_DEVICE_RT);

		if (tmp_tab)
			bitmap_or(dev_status, tab->dev_status,
			tmp_tab->dev_status, APUSYS_DEVICE_MAX);
		first_bit_idx = find_first_zero_bit(dev_status, tab->dev_num);
		if (first_bit_idx >= tab->dev_num)
			bitmap_copy(dev_status,
				tab->dev_status, APUSYS_DEVICE_MAX);
	}

	/* check dev num */
	if (acq->target_num > tab->dev_num) {
		mdw_drv_err("alloc wrong num(%d/%d) device(%d)\n",
			acq->target_num, tab->dev_num, acq->dev_type);
		return -EINVAL;
	}

	/* get idle device from bitmap */
	mdw_flw_debug("allocate device(%d) (%d/%d/%d)\n",
		acq->dev_type, acq->target_num,
		tab->available_num, tab->dev_num);
	for (i = 0; i < ret_num; i++) {
		bit_idx = find_first_zero_bit(dev_status, tab->dev_num);

		/* Avoid to preempt the same core */
		if (tab->dev_type > APUSYS_DEVICE_RT) {
			if (tab->last_idx == bit_idx) {
				next_bit_idx = find_next_zero_bit(
				dev_status, tab->dev_num, bit_idx + 1);
				if (next_bit_idx < tab->dev_num)
					bit_idx = next_bit_idx;
			}
			tab->last_idx = bit_idx;
		}

		mdw_flw_debug("dev(%d) idx(%lu/%u) available\n",
			acq->dev_type, bit_idx, tab->dev_num);
		if (bit_idx < tab->dev_num) {
			list_add_tail(&tab->dev_list[bit_idx].acq_list,
				&acq->dev_info_list);
			tab->dev_list[bit_idx].cur_owner = acq->owner;
			bitmap_set(tab->dev_status, bit_idx, 1);
			bitmap_set(dev_status, bit_idx, 1);
			tab->available_num--;

			acq->acq_num++;
			acq->acq_bitmap |= (1ULL << bit_idx);
		} else {
			mdw_drv_err("can't find device(%d)\n", acq->dev_type);
		}
	}

	/* if no dev available in table, clear dev status in resource mgr */
	idx = find_next_zero_bit(tab->dev_status, tab->dev_num, 0);
	if (idx >= tab->dev_num) {
		bitmap_clear(g_res_mgr.dev_exist, tab->dev_type, 1);
		mdw_flw_debug("clear res mgr dev(%d) status type\n",
			tab->dev_type);
	}

	return 0;
}

int acq_device_check(struct apusys_dev_aquire **iacq)
{
	int i = 0;
	struct apusys_res_table *tab = NULL;
	struct apusys_dev_aquire *acq = NULL;

	/* TODO, optimize acq list */
	mutex_lock(&g_res_mgr.mtx);
	mdw_lne_debug();
	for (i = 0; i < APUSYS_DEVICE_MAX; i++) {
		tab = res_get_table(i);
		if (tab == NULL)
			continue;

		if (!list_empty(&tab->acq_list)) {
			mdw_flw_debug("res tab(%d) has acq_list\n", i);
			acq = list_first_entry(&tab->acq_list,
				struct apusys_dev_aquire, tab_list);
			if (acq->is_done && acq->target_num == acq->acq_num
				&& acq->owner == APUSYS_DEV_OWNER_SCHEDULER) {
				mdw_flw_debug("res tab(%d) acq ready\n", i);
				list_del(&acq->tab_list);
				*iacq = acq;
				mutex_unlock(&g_res_mgr.mtx);
				return 0;
			}
		}
	}
	mutex_unlock(&g_res_mgr.mtx);
	mdw_lne_debug();

	return -ENODATA;
}

int acq_device_cancel(struct apusys_dev_aquire *acq)
{
	struct apusys_res_table *tab = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_dev_aquire *got = NULL;
	struct apusys_dev_info *dev_info = NULL;
	int flag = 0, ret = 0;

	if (acq == NULL)
		return -EINVAL;

	tab = res_get_table(acq->dev_type);
	if (tab == NULL)
		return -EINVAL;

	/* 1. get acq from tab list */
	list_for_each_safe(list_ptr, tmp, &tab->acq_list) {
		got = list_entry(list_ptr, struct apusys_dev_aquire, tab_list);
		if (got == acq) {
			flag = 1;
			list_del(&got->tab_list);
			break;
		}
	}

	/* 2. free all acquired device */
	if (flag) {
		list_for_each_safe(list_ptr, tmp, &acq->dev_info_list) {
			dev_info = list_entry(list_ptr,
				struct apusys_dev_info, acq_list);
			if (put_apusys_device(dev_info)) {
				mdw_drv_err("put dev(%s-#%d) fail\n",
					dev_info->name,
					dev_info->dev->idx);
				ret = -ENODEV;
			}
			list_del(&dev_info->acq_list);
		}
	} else {
		ret = -ENODEV;
	}

	acq->acq_num = 0;

	return ret;
}

int acq_device_async(struct apusys_dev_aquire *acq)
{
	struct apusys_res_table *tab = NULL;
	struct apusys_res_table *tmp_tab = NULL;
	int i = 0, bit_idx = 0, num = 0;
	unsigned long dev_status[BITS_TO_LONGS(APUSYS_DEV_TABLE_MAX)];

	if (acq == NULL)
		return -EINVAL;

	/* get apusys_res_table*/
	tab = res_get_table(acq->dev_type);
	/* can't find info, unlock and return fail */
	if (tab == NULL || acq->target_num <= 0 ||
		acq->acq_num != 0) {
		mdw_drv_err("invalid arg, dev(%d) num(%d/%d)\n",
			acq->dev_type,
			acq->target_num,
			acq->acq_num);
		return -EINVAL;
	}

	num = tab->available_num < (acq->target_num-acq->acq_num)
		? tab->available_num : (acq->target_num-acq->acq_num);

	bitmap_copy(dev_status, tab->dev_status, APUSYS_DEVICE_MAX);

	if (num == 1) {
		if (acq->dev_type > APUSYS_DEVICE_RT)
			tmp_tab =
			res_get_table(acq->dev_type - APUSYS_DEVICE_RT);
		else
			tmp_tab =
			res_get_table(acq->dev_type + APUSYS_DEVICE_RT);

		if (tmp_tab)
			bitmap_or(dev_status, tab->dev_status,
				tmp_tab->dev_status, APUSYS_DEVICE_MAX);
		bit_idx = find_first_zero_bit(dev_status, tab->dev_num);
		if (bit_idx >= tab->dev_num)
			bitmap_copy(dev_status,
				tab->dev_status, APUSYS_DEVICE_MAX);
	}

	/* check dev num */
	if (acq->target_num > tab->dev_num) {
		mdw_drv_err("alloc wrong num(%d/%d) device(%d)\n",
			acq->target_num, tab->dev_num, acq->dev_type);
		return -EINVAL;
	}

	init_completion(&acq->comp);
	INIT_LIST_HEAD(&acq->dev_info_list);
	acq->acq_num = 0;
	acq->acq_bitmap = 0;

	mdw_lne_debug();

	/* device table's available device is more than acquire target num */
	for (i = 0; i < num; i++) {
		bit_idx = find_first_zero_bit(dev_status, tab->dev_num);
		mdw_flw_debug("dev(%d-#%d/%u) available\n",
			acq->dev_type, bit_idx, tab->dev_num);
		if (bit_idx < tab->dev_num) {
			mdw_flw_debug("add to acquire list\n");
			list_add_tail(&tab->dev_list[bit_idx].acq_list,
				&acq->dev_info_list);
			bitmap_set(tab->dev_status, bit_idx, 1);
			bitmap_set(dev_status, bit_idx, 1);
			tab->dev_list[bit_idx].cur_owner = acq->owner;
			tab->available_num--;

			acq->acq_num++;
			acq->acq_bitmap |= (1ULL << bit_idx);
		} else {
			mdw_drv_err("can't find device(%d)\n", acq->dev_type);
			return -EINVAL;
		}
	}
	mdw_lne_debug();

	if (acq->acq_num < acq->target_num) {
		mdw_flw_debug("acquire device(%d/%d/%d) async...\n",
			acq->dev_type, acq->acq_num, acq->target_num);
		mdw_flw_debug("add to acquire list\n");
		list_add_tail(&acq->tab_list, &tab->acq_list);
	}
	mdw_lne_debug();

	return acq->acq_num;
}

int check_idle_dev(int type)
{
	struct apusys_res_table *tab = NULL;

	tab = res_get_table(type);
	if (tab == NULL)
		return -EINVAL;

	return 0;
}

int acq_device_sync(struct apusys_dev_aquire *acq)
{
	int ret = 0;
	unsigned long timeout = usecs_to_jiffies(2*1000*1000);

	if (acq == NULL)
		return -EINVAL;

	acq->acq_num = 0;
	mdw_lne_debug();

	mutex_lock(&g_res_mgr.mtx);

	ret = acq_device_async(acq);
	if (ret < 0) {
		mutex_unlock(&g_res_mgr.mtx);
		return ret;
	}
	mdw_lne_debug();

	/* if acquire == target, return ok */
	if (acq->acq_num == acq->target_num) {
		mdw_lne_debug();
		mutex_unlock(&g_res_mgr.mtx);
		return acq->acq_num;
	}
	mdw_lne_debug();

	mutex_unlock(&g_res_mgr.mtx);

	ret = wait_for_completion_timeout(&acq->comp, timeout);
	if (ret == 0) {
		mdw_drv_warn("acquire device(%d/%d/%d) timeout(%lu), ret(%d)\n",
			acq->dev_type, acq->acq_num,
			acq->target_num, timeout/1000/1000, ret);
	}

	mutex_lock(&g_res_mgr.mtx);
	if (acq->acq_num == acq->target_num) {
		mdw_flw_debug("acquire dev(%d/%d/%d) sync ok\n",
		acq->dev_type, acq->acq_num, acq->target_num);
		list_del(&acq->tab_list);
	} else {
		mdw_drv_warn("cancel acq(%d)...\n", acq->dev_type);
		if (acq_device_cancel(acq)) {
			mdw_drv_err("cancel acq(%d) fail\n", acq->dev_type);
			acq->acq_num = 0;
		}
	}
	mutex_unlock(&g_res_mgr.mtx);

	return acq->acq_num;
}

int res_power_on(int dev_type, uint32_t idx,
	uint32_t boost_val, uint32_t timeout)
{
	struct apusys_res_table *tab = NULL;
	struct apusys_dev_info *info = NULL;
	struct apusys_power_hnd pwr;
	int ret = 0;

	mdw_drv_debug("poweron dev(%d-#%u) boost(%u) timeout(%u)\n",
		dev_type, idx, boost_val, timeout);

	if (boost_val > 100) {
		mdw_drv_err("invalid boost val(%d)\n", boost_val);
		return -EINVAL;
	}

	tab = res_get_table(dev_type);
	if (tab == NULL) {
		mdw_drv_err("invalid device type(%d)\n", dev_type);
		return -EINVAL;
	}

	if (idx >= tab->dev_num) {
		mdw_drv_err("invalid device idx(%d-#%u/%d)\n",
			dev_type, idx, tab->dev_num);
		return -EINVAL;
	}

	info = &tab->dev_list[idx];
	if (info == NULL) {
		mdw_drv_err("can't find device info(%p) (%d-#%u)\n",
			info, dev_type, idx);
		return -EINVAL;
	}

	memset(&pwr, 0, sizeof(struct apusys_power_hnd));
	pwr.opp = 0;
	pwr.boost_val = boost_val;
	if (timeout > APUSYS_SETPOWER_TIMEOUT)
		pwr.timeout = APUSYS_SETPOWER_TIMEOUT;
	else
		pwr.timeout = timeout;
	ret = info->dev->send_cmd(APUSYS_CMD_POWERON, &pwr, info->dev);
	if (ret) {
		mdw_drv_err("poweron dev(%d-#%u) boostval(%d) fail(%d)\n",
			dev_type, idx, boost_val, ret);
	}

	return ret;
}

int res_power_off(int dev_type, uint32_t idx)
{
	struct apusys_res_table *tab = NULL;
	struct apusys_dev_info *info = NULL;
	struct apusys_power_hnd pwr;
	int ret = 0;

	mdw_drv_debug("powerdown dev(%d-#%u)\n", dev_type, idx);

	tab = res_get_table(dev_type);
	if (tab == NULL) {
		mdw_drv_err("invalid device type(%d)\n", dev_type);
		return -EINVAL;
	}

	if (idx >= tab->dev_num) {
		mdw_drv_err("invalid device idx(%d-#%u/%d)\n",
			dev_type, idx, tab->dev_num);
		return -EINVAL;
	}

	info = &tab->dev_list[idx];
	if (info == NULL) {
		mdw_drv_err("can't find device info(%p) (%d-#%u)\n",
			info, dev_type, idx);
		return -EINVAL;
	}

	memset(&pwr, 0, sizeof(struct apusys_power_hnd));
	ret = info->dev->send_cmd(APUSYS_CMD_POWERDOWN, &pwr, info->dev);
	if (ret) {
		mdw_drv_err("powerdown dev(%s-#%u) fail(%d)\n",
			info->name, idx, ret);
	}

	return ret;
}

int res_suspend_dev(void)
{
	int dev_type = 0, i = 0, j = 0, ret = 0, times = 30, wait_ms = 100;
	struct apusys_res_mgr *res_mgr = res_get_mgr();
	struct apusys_res_table *tab = NULL;


	/* check all device done then call suspend callback() */
	while (1) {
		dev_type = find_next_bit(res_mgr->dev_support,
			APUSYS_DEVICE_MAX, dev_type);
		if (dev_type >= APUSYS_DEVICE_MAX)
			break;

		tab = res_get_table(dev_type);
		if (tab == NULL) {
			mdw_drv_err("miss dev(%d)\n", dev_type);
			break;
		}

		/* check owner */
		for (i = 0; i < tab->dev_num; i++) {
			for (j = 0; j < times; j++) {
				mutex_lock(&g_res_mgr.mtx);
				if (tab->dev_list[i].cmd_id == 0) {
					mutex_unlock(&g_res_mgr.mtx);
					break;
				}
				mutex_unlock(&g_res_mgr.mtx);

				msleep(wait_ms);
			}
			if (j >= times) {
				mdw_drv_warn("dev(%s-#%d) busy too long\n",
					tab->dev_list[i].name, i);
			}
		}

		/* call suspend */
		for (i = 0; i < tab->dev_num; i++) {
			mutex_lock(&g_res_mgr.mtx);
			if (tab->dev_list[i].cur_owner ==
				APUSYS_DEV_OWNER_SECURE) {
				mdw_drv_warn("dev(%s-#%d) in secure, no poweroff\n",
					tab->dev_list[i].name,
					tab->dev_list[i].dev->idx);
				continue;
			}
			ret |= tab->dev_list[i].dev->send_cmd(
				APUSYS_CMD_SUSPEND, NULL, tab->dev_list[i].dev);
			if (ret) {
				mdw_drv_err("suspend dev(%s-#%d) fail(%d)\n",
					tab->name, i, ret);
			} else {
				mdw_drv_debug("suspend dev(%s-#%d) done\n",
					tab->name, i);
			}
			mutex_unlock(&g_res_mgr.mtx);
		}
		dev_type++;
	}

	return ret;
}

int res_resume_dev(void)
{
	int dev_type = 0, i = 0, ret = 0;
	struct apusys_res_mgr *res_mgr = res_get_mgr();
	struct apusys_res_table *tab = NULL;

	while (1) {
		dev_type = find_next_bit(res_mgr->dev_support,
			APUSYS_DEVICE_MAX, dev_type);

		if (dev_type >= APUSYS_DEVICE_MAX)
			break;

		tab = res_get_table(dev_type);
		if (tab == NULL) {
			mdw_drv_err("miss dev(%d)\n", dev_type);
			break;
		}

		for (i = 0; i < tab->dev_num; i++) {
			if (tab->dev_list[i].cmd_id != 0) {
				mdw_drv_warn("dev(%s-#%d)s(%llu/0x%llx)abnormal\n",
					tab->dev_list[i].name, i,
					tab->dev_list[i].cur_owner,
					tab->dev_list[i].cmd_id);
			}

			/* call resume */
			//mdw_drv_debug("resume dev(%d-#%d)...\n", dev_type, i);
			ret = tab->dev_list[i].dev->send_cmd(APUSYS_CMD_RESUME,
				NULL, tab->dev_list[i].dev);
			if (ret) {
				mdw_drv_err("resume dev(%s-#%d) fail(%d)\n",
					tab->dev_list[i].name, i, ret);
			} else {
				mdw_drv_debug("resume dev(%s-#%d) done\n",
					tab->dev_list[i].name, i);
			}
		}
		dev_type++;
	}

	return ret;
}

int res_load_firmware(int dev_type, uint32_t magic, const char *name,
	int idx, uint64_t kva, uint32_t iova, uint32_t size, int op)
{
	struct apusys_res_table *tab = NULL;
	struct apusys_dev_info *info = NULL;
	struct apusys_firmware_hnd hnd;
	int ret = 0;

	if (name == NULL)
		return -EINVAL;

	tab = res_get_table(dev_type);
	if (tab == NULL) {
		mdw_drv_err("invalid device type(%d)\n", dev_type);
		return -EINVAL;
	}

	if (idx >= tab->dev_num) {
		mdw_drv_err("invalid device idx(%s-#%d/%d)\n",
			tab->name, idx, tab->dev_num);
		return -EINVAL;
	}

	info = &tab->dev_list[idx];
	if (info == NULL) {
		mdw_drv_err("can't find device info(%p) (%d/%d)\n",
			info, dev_type, idx);
		return -EINVAL;
	}

	memset(&hnd, 0, sizeof(struct apusys_firmware_hnd));
	hnd.magic = magic;
	hnd.kva = kva;
	hnd.iova = iova;
	hnd.size = size;
	hnd.op = op;
	strncpy(hnd.name, name, sizeof(hnd.name)-1);

	ret = info->dev->send_cmd(APUSYS_CMD_FIRMWARE, &hnd, info->dev);
	if (ret) {
		mdw_drv_err("load(%d) dev(%d-#%d) fw(0x%x/0x%x) fail(%d)",
			op, dev_type, idx, iova, size, ret);
	}

	return ret;
}

int res_send_ucmd(int dev_type, int idx,
	uint64_t kva, uint32_t iova, uint32_t size)
{
	struct apusys_usercmd_hnd hnd;
	struct apusys_res_table *tab = NULL;
	struct apusys_dev_info *info = NULL;
	int ret = 0;

	tab = res_get_table(dev_type);
	if (tab == NULL) {
		mdw_drv_err("invalid device type(%d)\n", dev_type);
		return -EINVAL;
	}

	if (idx >= tab->dev_num) {
		mdw_drv_err("invalid device idx(%d-#%d/%d)\n",
			dev_type, idx, tab->dev_num);
		return -EINVAL;
	}

	info = &tab->dev_list[idx];
	if (info == NULL) {
		mdw_drv_err("can't find device info(%p) (%d/%d)\n",
			info, dev_type, idx);
		return -EINVAL;
	}

	memset(&hnd, 0, sizeof(struct apusys_usercmd_hnd));
	hnd.kva = kva;
	hnd.iova = iova;
	hnd.size = size;

	ret = info->dev->send_cmd(APUSYS_CMD_USER, &hnd, info->dev);
	if (ret) {
		mdw_drv_warn("send dev(%s-#%d) ucmd(0x%llx/0x%x/%d) fail(%d)",
			info->name, info->dev->idx,
			kva, iova, size, ret);
	}

	return ret;
}

void res_sched_enable(int enable)
{
	mutex_lock(&g_res_mgr.mtx);
	if (enable) {
		mdw_drv_debug("start apusys sched\n");
		g_res_mgr.sched_pause = 0;
	} else {
		mdw_drv_debug("pause apusys sched\n");
		g_res_mgr.sched_pause = 1;
	}
	mutex_unlock(&g_res_mgr.mtx);
}

int res_secure_on(int dev_type)
{
	struct apusys_res_table *tab = NULL;
	int ret = 0;

	tab = res_get_table(dev_type);
	if (tab == NULL) {
		mdw_drv_err("[sec]can't find dev(%d)\n", dev_type);
		return -ENODEV;
	}

	switch (dev_type) {
	case APUSYS_DEVICE_SAMPLE:
		ret = 0;
		break;

#ifdef CONFIG_MTK_GZ_SUPPORT_SDSP
	case APUSYS_DEVICE_VPU:
		ret = mtee_sdsp_enable(1);
		if (!ret)
			secure_ws_lock();
		break;
#endif
	default:
		mdw_drv_err("[sec]dev(%d) not support secure mode\n", dev_type);
		ret = -ENODEV;
		break;
	}

	return ret;
}

int res_secure_off(int dev_type)
{
	struct apusys_res_table *tab = NULL;
	int ret = 0;

	tab = res_get_table(dev_type);
	if (tab == NULL) {
		mdw_drv_err("secure control can't find dev(%d)\n", dev_type);
		return -ENODEV;
	}

	switch (dev_type) {
	case APUSYS_DEVICE_SAMPLE:
		ret = 0;
		mdw_drv_debug("dev(%d) secure mode off(%d)\n", dev_type, ret);
		break;

#ifdef CONFIG_MTK_GZ_SUPPORT_SDSP
	case APUSYS_DEVICE_VPU:
		secure_ws_unlock();
		ret = mtee_sdsp_enable(0);
		break;
#endif
	default:
		mdw_drv_err("dev(%d) not support secure mode\n", dev_type);
		ret = -ENODEV;
		break;
	}

	return ret;
}

int res_task_inc(struct apusys_subcmd *sc)
{
	struct apusys_res_table *tab = NULL;

	if (sc == NULL)
		return -EINVAL;

	tab = res_get_table(sc->type);
	if (tab == NULL)
		return -EINVAL;

	mutex_lock(&tab->mtx);
	if (sc->period)
		tab->deadline_task_num++;
	else
		tab->normal_task_num++;
	mutex_unlock(&tab->mtx);
	return 0;
}

int res_task_dec(struct apusys_subcmd *sc)
{
	struct apusys_res_table *tab = NULL;

	if (sc == NULL)
		return -EINVAL;

	tab = res_get_table(sc->type);
	if (tab == NULL)
		return -EINVAL;

	mutex_lock(&tab->mtx);
	if (sc->period)
		tab->deadline_task_num--;
	else
		tab->normal_task_num--;
	mutex_unlock(&tab->mtx);

	return 0;
}

void res_mgt_dump(void *s_file)
{
	struct apusys_res_table *tab = NULL;
	struct apusys_dev_info *info = NULL;
	struct seq_file *s = (struct seq_file *)s_file;
	int i = 0, j = 0;

#define LINEBAR \
	"|------------------------------------------"\
	"--------------------------------------------|\n"
#define S_LINEBAR \
	"|------------------------------------------"\
	"-------------------|\n"

	mdw_con_info(s, LINEBAR);
	mdw_con_info(s, "|%-86s|\n",
		" apusys device table");
	mdw_con_info(s, LINEBAR);

	mutex_lock(&g_res_mgr.mtx);

	/* query list to find apusys device table by type */
	for (i = 0; i < APUSYS_DEVICE_MAX; i++) {
		tab = res_get_table(i);
		if (tab == NULL)
			continue;

		for (j = 0; j < tab->dev_num; j++) {
			info = &tab->dev_list[j];
			if (info->cur_owner == APUSYS_DEV_OWNER_DISABLE) {
				if (j != 0) {
					mdw_con_info(s, "|%24s|%-9s#%-4d>%46s|\n",
						"",
						" <device ",
						j,
						"");
					mdw_con_info(s, "|%24s|-%61s|\n",
						"",
						" disable");
					mdw_con_info(s, "\n");
					mdw_con_info(s, "\n");
					mdw_con_info(s, LINEBAR);
					continue;
				}

				mdw_con_info(s, "|%-14s(%7d) |%-9s#%-4d>%46s|\n",
					" dev type ",
					tab->dev_type,
					" <device ",
					j,
					"");
				mdw_con_info(s, "|%-14s(%7d) |%-61s|\n",
					" core num ",
					tab->dev_num,
					" disable");
				mdw_con_info(s, "|%-14s(%7u) |%-61s|\n",
					" available",
					tab->available_num,
					"");
				mdw_con_info(s, "|%-14s(%3u/%3u) |%-61s|\n",
					" cmd queue",
					tab->normal_task_num,
					tab->deadline_task_num,
					"");
				mdw_con_info(s, LINEBAR);

				continue;
			}

			if (j == 0) {
				/* print tab info at dev #0 */
				mdw_con_info(s, "|%-14s(%7s) |%-9s#%-4d>%46s|\n",
					" dev name",
					tab->name,
					" <device ",
					j,
					"");
				mdw_con_info(s, "|%-14s(%7d) |%-18s= %-41d|\n",
					" dev type ",
					tab->dev_type,
					" device idx",
					info->dev->idx);
				mdw_con_info(s, "|%-14s(%7d) |%-18s= 0x%-39llx|\n",
					" core num ",
					tab->dev_num,
					" cmd id",
					info->cmd_id);
				mdw_con_info(s, "|%-14s(%7u) |%-18s= 0x%-39d|\n",
					" available",
					tab->available_num,
					" subcmd idx",
					info->sc_idx);
				mdw_con_info(s, "|%-14s(%3u/%3u) |%-18s= %-41llu|\n",
					" cmd queue",
					tab->normal_task_num,
					tab->deadline_task_num,
					" current owner",
					info->cur_owner);
			} else {
				mdw_con_info(s, "|%-24s|%-9s#%-4d>%-46s|\n",
					"",
					" <device ",
					j,
					"");
				mdw_con_info(s, "|%-24s|%-18s= %-41d|\n",
					"",
					" device idx",
					info->dev->idx);
				mdw_con_info(s, "|%-24s|%-18s= 0x%-39llx|\n",
					"",
					" cmd id",
					info->cmd_id);
				mdw_con_info(s, "|%-24s|%-18s= 0x%-39d|\n",
					"",
					" subcmd idx",
					info->sc_idx);
				mdw_con_info(s, "|%-24s|%-18s= %-41llu|\n",
					"",
					" current owner",
					info->cur_owner);
			}

			if (j >= tab->dev_num-1) {
				mdw_con_info(s, LINEBAR);
			} else {
				mdw_con_info(s, "|%-24s%s",
					"",
					S_LINEBAR);
			}
		}
	}

	mutex_unlock(&g_res_mgr.mtx);

#undef LINEBAR
#undef S_LINEBAR
}

//----------------------------------------------
int res_mgt_init(void)
{
	mdw_flw_debug("%s +\n", __func__);

	/* clear global variable */
	memset(&g_res_mgr, 0, sizeof(g_res_mgr));
	mutex_init(&g_res_mgr.mtx);
	init_completion(&g_res_mgr.sched_comp);
	secure_ws_init();

	mdw_flw_debug("%s done -\n", __func__);

	return 0;
}

int res_mgt_destroy(void)
{
	mdw_flw_debug("%s +\n", __func__);

	/* clear global variable */
	memset(&g_res_mgr, 0, sizeof(g_res_mgr));

	mdw_flw_debug("%s done -\n", __func__);
	return 0;
}

//----------------------------------------------
// export function
int apusys_register_device(struct apusys_device *dev)
{
	struct apusys_res_table *tab = NULL;
	int i = 0, ret = 0;

	mdw_flw_debug("register apusys device +\n");

	/* check argument ptr valid */
	if (dev == NULL) {
		mdw_drv_err("register dev nullptr\n");
		return -EINVAL;
	}

	/* check dev type valid */
	if (dev->dev_type < 0 || dev->dev_type >= APUSYS_DEVICE_MAX) {
		mdw_drv_err("register dev wrong type(%d)\n", dev->dev_type);
		return -EINVAL;
	};

	mutex_lock(&g_res_mgr.mtx);
	tab = res_get_table(dev->dev_type);
	if (tab != NULL) {
		/* device type already registered, add directly */
		mdw_drv_debug("add dev to type(%d) list\n", dev->dev_type);
		for (i = 0; i < APUSYS_DEV_TABLE_MAX; i++) {
			if (tab->dev_list[i].cur_owner
				!= APUSYS_DEV_OWNER_DISABLE)
				continue;

			tab->dev_list[i].dev = dev; // setup dev
			bitmap_clear(tab->dev_status, i, 1); // setup status

			/* setup devinfo name */
			snprintf(tab->dev_list[tab->dev_num].name,
			APUSYS_DEV_NAME_SIZE-1,
			"%s%s",
			dev_type_string[tab->dev_type % APUSYS_DEVICE_RT],
			tab->dev_type > APUSYS_DEVICE_RT ? "_rt":""
			);

			if (i >= tab->dev_num)
				tab->dev_num++;

			/* rewrite device's idx */
			if (dev->idx != i) {
				mdw_drv_warn("over write dev idx(%d->%d)\n",
					dev->idx,
					i);
				dev->idx = i;
			}

			tab->dev_list[i].cur_owner = APUSYS_DEV_OWNER_NONE;
			mdw_drv_debug("register dev(%d-#%d/%d/%p) ok\n",
				dev->dev_type, i,
				tab->dev_num, dev);

			break;
		}

		if (i >= APUSYS_DEV_TABLE_MAX) {
			mdw_drv_err("register dev(%d) fail(%d/%d), dev list full\n",
				dev->dev_type, i, tab->dev_num);
			mutex_unlock(&g_res_mgr.mtx);
			return -ENODEV;
		}
	} else {
		/* device type not registered yet, allocate table first */
		mdw_drv_debug("register device(%d)\n", dev->dev_type);
		tab = kzalloc(sizeof(struct apusys_res_table), GFP_KERNEL);
		tab->dev_list[tab->dev_num].dev = dev; // dev
		tab->dev_list[tab->dev_num].cur_owner = APUSYS_DEV_OWNER_NONE;
		bitmap_clear(tab->dev_status, tab->dev_num, 1); //status
		tab->dev_type = dev->dev_type;// type

		/* setup devinfo/devtable name */
		if (tab->dev_type < APUSYS_DEVICE_LAST) {
			snprintf(tab->name,
			APUSYS_DEV_NAME_SIZE-1,
			"%s%s",
			dev_type_string[tab->dev_type % APUSYS_DEVICE_RT],
			tab->dev_type > APUSYS_DEVICE_RT ? "_rt":""
			);
			snprintf(tab->dev_list[tab->dev_num].name,
			APUSYS_DEV_NAME_SIZE-1,
			"%s%s",
			dev_type_string[tab->dev_type % APUSYS_DEVICE_RT],
			tab->dev_type > APUSYS_DEVICE_RT ? "_rt":""
			);
		}

		/* check idx */
		if (dev->idx != tab->dev_num) {
			mdw_drv_warn("dev(%d) idx not match(%d/%d)\n",
				dev->dev_type, dev->idx, tab->dev_num);
			dev->idx = tab->dev_num;
		}

		tab->dev_num++; //dev number
		for (i = 0; i < APUSYS_DEV_TABLE_MAX; i++)
			INIT_LIST_HEAD(&tab->dev_list[i].acq_list);

		INIT_LIST_HEAD(&tab->acq_list);
		mutex_init(&tab->mtx);

		g_res_mgr.tab[tab->dev_type] = tab;
		normal_queue_init(tab->dev_type); /* init normal queue */
		bitmap_set(g_res_mgr.dev_support, tab->dev_type, 1);

		/* create queue length for query */
		if (res_dbg_tab_init(tab)) {
			mdw_drv_err("create queue length node(%d) fail\n",
			tab->dev_type);
		}

		deadline_queue_init(tab->dev_type); /* Init deadline queue */

		mdw_drv_debug("register dev(%d-#%d/%d/%p) ok\n",
			dev->dev_type, dev->idx,
			tab->dev_num, dev);
	}

	tab->available_num++;

	/* mark mgr type dev is available */
	bitmap_set(g_res_mgr.dev_exist, dev->dev_type, 1);

	mutex_unlock(&g_res_mgr.mtx);

	/* create thread to thread pool */
	for (i = 0; i <= dev->preempt_level; i++) {
		if (thread_pool_add_once()) {
			mdw_drv_err("create thread(%d) to pool fail\n", i);
			if (thread_pool_delete(i))
				mdw_drv_err("delete thread(%d) fail\n", i);

			ret = -EINVAL;
			goto allc_q_fail;
		}
	}

	mdw_flw_debug("register apusys device -\n");

	return 0;

allc_q_fail:
	/* release dev table*/
	g_res_mgr.tab[tab->dev_type] = NULL;
	kfree(tab);

	return ret;
}

int apusys_unregister_device(struct apusys_device *dev)
{
	struct apusys_res_table *tab = NULL;
	int ret = 0, i = 0, idx = 0;

	mdw_lne_debug();
	if (dev == NULL) {
		mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}

	mutex_lock(&g_res_mgr.mtx);

	/* TODO: should flush cmd from dev */
	tab = res_get_table(dev->dev_type);
	if (tab == NULL) {
		mdw_drv_err("can't find dev table(%d)\n", dev->dev_type);
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < APUSYS_DEV_TABLE_MAX; i++) {
		if (tab->dev_list[i].dev == dev) {
			mdw_drv_debug("get dev(%p)\n", dev);
			if (tab->dev_list[i].cur_owner ==
				APUSYS_DEV_OWNER_NONE) {
				mdw_drv_debug("dev(%p) is free, can release\n",
					tab->dev_list[i].dev);

				/* clear dev info */
				memset(&tab->dev_list[i], 0,
					sizeof(struct apusys_dev_info));
				/* delete from dev table */
				/* set status let schdeuler don't dispatch */
				bitmap_set(tab->dev_status, i, 1);
				tab->available_num--;
			} else {
				mdw_drv_err("dev(%p) is busy, release fail\n",
					dev);
				ret = -EINVAL;
			}
			break;
		}
	}

	if (i >= APUSYS_DEV_TABLE_MAX) {
		mdw_drv_err("can't find dev, release device fail\n");
		ret = -EINVAL;
	}

	/* if any dev available in table, clear dev status in resource mgr*/
	idx = find_next_zero_bit(tab->dev_status, tab->dev_num, 0);
	if (idx >= tab->dev_num) {
		bitmap_clear(g_res_mgr.dev_exist, tab->dev_type, 1);
		mdw_drv_debug("clear res mgr dev(%d) status type\n",
			tab->dev_type);
	}

out:
	mutex_unlock(&g_res_mgr.mtx);
	return ret;
}
