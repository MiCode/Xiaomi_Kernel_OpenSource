// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/vmalloc.h>
#ifdef CONFIG_PM_SLEEP
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#endif

#include "mdw_cmn.h"
#include "mdw_rsc.h"
#include "mdw_sched.h"
#include "mdw_cmd.h"


struct mdw_rsc_mgr {
	unsigned long dev_sup_bmp[BITS_TO_LONGS(APUSYS_DEVICE_MAX)];
	unsigned long dev_avl_bmp[BITS_TO_LONGS(APUSYS_DEVICE_MAX)];
	unsigned long cmd_avl_bmp[BITS_TO_LONGS(APUSYS_DEVICE_MAX)];
	struct mdw_rsc_tab **tabs;

	uint32_t preempt_policy;

	struct list_head r_list;
	struct mutex mtx;
};

static atomic_t sthd_group = ATOMIC_INIT(0);
static struct mdw_rsc_mgr rsc_mgr;

static char * const rsc_dev_name[] = {
	"none",
	"sample",
	"mdla",
	"vpu",
	"edma",
};

#ifdef CONFIG_PM_SLEEP
static struct wakeup_source *mdw_rsc_ws;
static uint32_t ws_cnt;
static struct mutex ws_mtx;
#endif

//----------------------------------------
static void mdw_rsc_ws_init(void)
{
#if defined CONFIG_PM_SLEEP
	char ws_name[16];

	if (snprintf(ws_name, sizeof(ws_name)-1, "apusys_secure") < 0) {
		mdw_drv_err("init rsc wakeup source fail\n");
		return;
	}
	ws_cnt = 0;
	mutex_init(&ws_mtx);
	mdw_rsc_ws = wakeup_source_register(NULL, (const char *)ws_name);
	if (!mdw_rsc_ws)
		mdw_drv_err("register ws lock fail!\n");
#else
	mdw_drv_debug("not support pm wakelock\n");
#endif
}

static void mdw_rsc_ws_destroy(void)
{
#if defined CONFIG_PM_SLEEP
	ws_cnt = 0;
	wakeup_source_unregister(mdw_rsc_ws);
#else
	mdw_drv_debug("not support pm wakelock\n");
#endif
}

void mdw_rsc_ws_lock(void)
{
#ifdef CONFIG_PM_SLEEP
	mutex_lock(&ws_mtx);
	if (mdw_rsc_ws && !ws_cnt) {
		mdw_flw_debug("lock\n");
		__pm_stay_awake(mdw_rsc_ws);
	}
	ws_cnt++;
	mutex_unlock(&ws_mtx);
#else
	mdw_flw_debug("not support pm wakelock\n");
#endif
}

void mdw_rsc_ws_unlock(void)
{
#ifdef CONFIG_PM_SLEEP
	mutex_lock(&ws_mtx);
	ws_cnt--;
	if (mdw_rsc_ws && !ws_cnt) {
		mdw_flw_debug("unlock\n");
		__pm_relax(mdw_rsc_ws);
	}
	mutex_unlock(&ws_mtx);
#else
	mdw_flw_debug("not support pm wakelock\n");
#endif
}

static int mdw_rsc_get_name(int type, char *name)
{
	int name_idx = 0;

	name_idx = type % APUSYS_DEVICE_RT;
	if (name_idx >= sizeof(rsc_dev_name)/sizeof(char *)) {
		mdw_drv_err("unknown dev(%d/%d) name\n", type, name_idx);
		return -ENODEV;
	}

	mdw_flw_debug("rsc dev name size = %lu/%s\n",
		(uint32_t)sizeof(rsc_dev_name)/sizeof(char *),
		rsc_dev_name[name_idx]);

	if (type >= APUSYS_DEVICE_RT) {
		if (snprintf(name, 32, "%s_rt", rsc_dev_name[name_idx]) < 0)
			return -EINVAL;
	} else {
		if (snprintf(name, 32, "%s", rsc_dev_name[name_idx]) < 0)
			return -EINVAL;
	}

	return 0;
}

static void mdw_rsc_delete_tab(struct mdw_rsc_tab *tab)
{
	mdw_queue_destroy(&tab->q);
	vfree(tab);
}

static struct mdw_rsc_tab *mdw_rsc_add_tab(int type)
{
	struct mdw_rsc_tab *tab = NULL;
	char name[32];

	tab = vzalloc(sizeof(struct mdw_rsc_tab));
	if (!tab)
		return NULL;

	/* assign rsc mgr value */
	rsc_mgr.tabs[type] = tab;
	tab->type = type;

	/* init device list */
	INIT_LIST_HEAD(&tab->list);

	/* init mutex */
	mutex_init(&tab->mtx);

	bitmap_set(rsc_mgr.dev_sup_bmp, type, 1);

	if (mdw_rsc_get_name(type, name))
		return NULL;

	strncpy(tab->name, name, sizeof(tab->name)-1);

	/* dbgfs */
	if (!mdw_dbg_device)
		goto init_queue;

	tab->dbg_dir = debugfs_create_dir(name,
		mdw_dbg_device);

	/* create debugfs */
	debugfs_create_u32("queue", 0444, tab->dbg_dir, &tab->q.normal_task_num);

init_queue:
	/* init queue */
	mdw_queue_init(&tab->q);

	return tab;
}

struct mdw_rsc_tab *mdw_rsc_get_tab(int type)
{
	if (type >= MDW_RSC_MAX_NUM || type < 0)
		return NULL;

	return rsc_mgr.tabs[type];
}

int mdw_rsc_avl_dev_num(int type)
{
	struct mdw_rsc_tab *tab = NULL;

	tab = mdw_rsc_get_tab(type);
	if (!tab)
		return -ENODEV;

	return tab->avl_num;
}

struct mdw_queue *mdw_rsc_get_queue(int type)
{
	struct mdw_rsc_tab *tab = NULL;

	tab = mdw_rsc_get_tab(type);
	if (!tab)
		return NULL;

	return &tab->q;
}

uint64_t mdw_rsc_get_avl_bmp(void)
{
	unsigned long bmp[BITS_TO_LONGS(APUSYS_DEVICE_MAX)];
	uint64_t b = 0;

	memset(&bmp, 0, sizeof(unsigned long) *
		BITS_TO_LONGS(APUSYS_DEVICE_MAX));

	bitmap_and(bmp, rsc_mgr.cmd_avl_bmp,
		rsc_mgr.dev_avl_bmp, APUSYS_DEVICE_MAX);
	bitmap_to_arr32((uint32_t *)&b, bmp, APUSYS_DEVICE_MAX);
	mdw_flw_debug("bmp(0x%llx)\n", b);

	return b;
}

void mdw_rsc_update_avl_bmp(int type)
{
	struct mdw_rsc_tab *tab;
	struct mdw_queue *mq;
	uint64_t cb = 0, db = 0;

	mutex_lock(&rsc_mgr.mtx);

	/* update dev bitmap */
	tab = mdw_rsc_get_tab(type);
	if (!tab)
		goto out;

	if (mdw_rsc_avl_dev_num(type))
		bitmap_set(rsc_mgr.dev_avl_bmp, type, 1);
	else
		bitmap_clear(rsc_mgr.dev_avl_bmp, type, 1);

	/* update cmd bitmap */
	mq = mdw_rsc_get_queue(type);
	if (!mq)
		goto out;

	if (mdw_queue_len(type, true) || mdw_queue_len(type, false))
		bitmap_set(rsc_mgr.cmd_avl_bmp, type, 1);
	else
		bitmap_clear(rsc_mgr.cmd_avl_bmp, type, 1);

	bitmap_to_arr32((uint32_t *)&cb,
		rsc_mgr.cmd_avl_bmp, APUSYS_DEVICE_MAX);
	bitmap_to_arr32((uint32_t *)&db,
		rsc_mgr.dev_avl_bmp, APUSYS_DEVICE_MAX);

	mdw_flw_debug("bmp: dev(0x%llx) cmd(0x%llx)\n", db, cb);

out:
	mutex_unlock(&rsc_mgr.mtx);
}

#define LINEBAR \
	"|------------------------------------------"\
	"--------------------------------------------|\n"
#define S_LINEBAR \
	"|------------------------------------------"\
	"-------------------|\n"
static void mdw_rsc_dump_tab(struct seq_file *s, struct mdw_rsc_tab *tab)
{
	int j = 0;
	struct mdw_dev_info *d = NULL;
	struct mdw_apu_sc *sc = NULL;

	mutex_lock(&tab->mtx);
	for (j = 0; j < tab->dev_num; j++) {
		d = tab->array[j];
		if (!d)
			continue;

		mutex_lock(&d->mtx);
		sc = (struct mdw_apu_sc *)d->sc;
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
				tab->type,
				" device idx",
				d->idx);
			mdw_con_info(s, "|%-14s(%7d) |%-18s= 0x%-39llx|\n",
				" core num ",
				tab->dev_num,
				" cmd id",
				sc == NULL ? 0 : sc->parent->kid);
			mdw_con_info(s, "|%-14s(%7u) |%-18s= 0x%-39d|\n",
				" available",
				tab->avl_num,
				" subcmd idx",
				sc == NULL ? 0 : sc->idx);
			mdw_con_info(s, "|%-14s(%3d/%3d) |%-18s= %-41d|\n",
				" cmd queue",
				mdw_queue_len(tab->type, false),
				mdw_queue_len(tab->type, true),
				" state ",
				d->state);
		} else {
			mdw_con_info(s, "|%-24s|%-9s#%-4d>%-46s|\n",
				"",
				" <device ",
				j,
				"");
			mdw_con_info(s, "|%-24s|%-18s= %-41d|\n",
				"",
				" device idx",
				d->idx);
			mdw_con_info(s, "|%-24s|%-18s= 0x%-39llx|\n",
				"",
				" cmd id",
				sc == NULL ? 0 : sc->parent->kid);
			mdw_con_info(s, "|%-24s|%-18s= 0x%-39d|\n",
				"",
				" subcmd idx",
				sc == NULL ? 0 : sc->idx);
			mdw_con_info(s, "|%-24s|%-18s= %-41d|\n",
				"",
				" state ",
				d->state);
		}

		if (j >= tab->dev_num-1) {
			mdw_con_info(s, LINEBAR);
		} else {
			mdw_con_info(s, "|%-24s%s",
				"",
				S_LINEBAR);
		}
		mutex_unlock(&d->mtx);
	}
	mutex_unlock(&tab->mtx);
}

void mdw_rsc_dump(struct seq_file *s)
{
	struct mdw_rsc_tab *tab = NULL;
	int i = 0;

	mdw_con_info(s, LINEBAR);
	mdw_con_info(s, "|%-86s|\n",
		" apusys device table");
	mdw_con_info(s, LINEBAR);

	mutex_lock(&rsc_mgr.mtx);

	/* query list to find apusys device table by type */
	for (i = 0; i < APUSYS_DEVICE_MAX; i++) {
		tab = mdw_rsc_get_tab(i);
		if (!tab)
			continue;
		mdw_rsc_dump_tab(s, tab);
	}

	mutex_unlock(&rsc_mgr.mtx);
}
#undef LINEBAR
#undef S_LINEBAR

static int mdw_rsc_dev_exec(struct mdw_dev_info *d, void *sc)
{
	if (!d)
		return -ENODEV;

	if (d->sc)
		mdw_drv_warn("dev(%s%d) res cmd\n", d->name, d->idx);
	mdw_flw_debug("dev(%s%d)\n", d->name, d->idx);
	mutex_lock(&d->mtx);
	d->sc = sc;
	mutex_unlock(&d->mtx);

	complete(&d->cmplt);

	return 0;
}

static int mdw_rsc_pwr_on(struct mdw_dev_info *d, int bst, int to)
{
	struct apusys_power_hnd h;

	memset(&h, 0, sizeof(h));
	h.boost_val = bst;
	h.timeout = to;

	return d->dev->send_cmd(APUSYS_CMD_POWERON, &h, d->dev);
}

static int mdw_rsc_pwr_off(struct mdw_dev_info *d)
{
	struct apusys_power_hnd h;

	memset(&h, 0, sizeof(h));
	return d->dev->send_cmd(APUSYS_CMD_POWERDOWN, &h, d->dev);
}

static int mdw_rsc_fw(struct mdw_dev_info *d, uint32_t magic, const char *name,
	uint64_t kva, uint32_t iova, uint32_t size, int op)
{
	struct apusys_firmware_hnd h;

	memset(&h, 0, sizeof(h));
	h.magic = magic;
	h.kva = kva;
	h.iova = iova;
	h.size = size;
	h.op = op;
	strncpy(h.name, name, sizeof(h.name)-1);

	return d->dev->send_cmd(APUSYS_CMD_FIRMWARE, &h, d->dev);
}

static int mdw_rsc_ucmd(struct mdw_dev_info *d,
	uint64_t kva, uint32_t iova, uint32_t size)
{
	struct apusys_usercmd_hnd h;

	memset(&h, 0, sizeof(h));
	h.kva = kva;
	h.iova = iova;
	h.size = size;

	return d->dev->send_cmd(APUSYS_CMD_USER, &h, d->dev);
}

static int mdw_rsc_sec_on(struct mdw_dev_info *d)
{
	int ret = 0;
	int type = d->type % APUSYS_DEVICE_RT;

	switch (type) {
	case APUSYS_DEVICE_SAMPLE:
		break;

#ifdef CONFIG_MTK_GZ_SUPPORT_SDSP
	case APUSYS_DEVICE_VPU:
		ret = mtee_sdsp_enable(1);
		if (!ret)
			mdw_rsc_ws_lock();
		break;
#endif
	default:
		mdw_drv_err("dev(%d) secure not support\n", type);
		ret = -ENODEV;
		break;
	}

	return ret;
}

static int mdw_rsc_sec_off(struct mdw_dev_info *d)
{
	int ret = 0;
	int type = d->type % APUSYS_DEVICE_RT;

	switch (type) {
	case APUSYS_DEVICE_SAMPLE:
		break;

#ifdef CONFIG_MTK_GZ_SUPPORT_SDSP
	case APUSYS_DEVICE_VPU:
		mdw_rsc_ws_unlock();
		ret = mtee_sdsp_enable(0);
		break;
#endif
	default:
	mdw_drv_err("dev(%d) secure not support\n", type);
		ret = -ENODEV;
		break;
	}

	return ret;
}

static int mdw_rsc_suspend(struct mdw_dev_info *d)
{
	struct mdw_rsc_tab *t = mdw_rsc_get_tab(d->type);
	int ret = 0;

	if (!t)
		return -ENODEV;

	mutex_lock(&t->mtx);
	if (d->state != MDW_DEV_INFO_STATE_IDLE) {
		mdw_drv_warn("dev(%s%d) busy(%d)\n", d->name, d->idx, d->state);
		ret = -EBUSY;
		goto out;
	}

	ret = d->dev->send_cmd(APUSYS_CMD_SUSPEND, NULL, d->dev);
out:
	mutex_unlock(&t->mtx);
	return ret;
}

static int mdw_rsc_resume(struct mdw_dev_info *d)
{
	return d->dev->send_cmd(APUSYS_CMD_RESUME, NULL, d->dev);
}

static int mdw_rsc_lock_dev(struct mdw_dev_info *d)
{
	struct mdw_rsc_tab *tab = NULL;
	int ret = 0;

	tab = mdw_rsc_get_tab(d->type);
	if (!tab)
		return -ENODEV;

	mutex_lock(&tab->mtx);
	if (d->state != MDW_DEV_INFO_STATE_IDLE) {
		ret = -EBUSY;
		goto out;
	}

	tab->avl_num--;
	list_del(&d->t_item);
	d->state = MDW_DEV_INFO_STATE_LOCK;
	mdw_drv_warn("dev(%s%d)\n", d->name, d->idx);

out:
	mutex_unlock(&tab->mtx);
	return ret;
}

static int mdw_rsc_unlock_dev(struct mdw_dev_info *d)
{
	struct mdw_rsc_tab *tab = NULL;
	int ret = 0;

	tab = mdw_rsc_get_tab(d->type);
	if (!tab)
		return -ENODEV;

	mutex_lock(&tab->mtx);
	if (d->state != MDW_DEV_INFO_STATE_LOCK) {
		ret = -EINVAL;
		goto out;
	}

	list_add_tail(&d->t_item, &tab->list);
	tab->avl_num++;
	d->state = MDW_DEV_INFO_STATE_IDLE;
	mdw_drv_warn("dev(%s%d)\n", d->name, d->idx);

out:
	mutex_unlock(&tab->mtx);
	return ret;

}

static int mdw_rsc_add_dev(struct apusys_device *dev)
{
	struct mdw_rsc_tab *tab;
	struct mdw_dev_info *d;
	char name[32], thd_name[32];
	int ret = 0;

	if (dev->dev_type >= APUSYS_DEVICE_MAX) {
		mdw_drv_err("invalid device idx(%d)\n", dev->dev_type);
		return -ENODEV;
	}
	if (mdw_rsc_get_name(dev->dev_type, name))
		return -ENODEV;

	/* get rscource table */
	tab = mdw_rsc_get_tab(dev->dev_type);
	if (!tab) {
		tab = mdw_rsc_add_tab(dev->dev_type);
		if (tab == NULL)
			return -ENODEV;
	}

	if (tab->dev_num >= MDW_RSC_TAB_DEV_MAX) {
		mdw_drv_err("rsc dev only support %d dev(%d)\n",
			MDW_RSC_TAB_DEV_MAX, tab->dev_num);
		return -ENODEV;
	}

	mutex_lock(&tab->mtx);

	/* new dev info */
	d = vzalloc(sizeof(struct mdw_dev_info));
	if (!d) {
		ret = -ENOMEM;
		goto out;
	}

	/* init dev info list item and insert to table */
	if (dev->idx != tab->dev_num) {
		mdw_drv_warn("dev overwrite idx(%d->%d)\n",
			dev->idx, tab->dev_num);
		dev->idx = tab->dev_num;
	}
	d->idx = dev->idx;
	if (d->idx >= MDW_RSC_TAB_DEV_MAX || d->idx < 0)
		goto fail_check_idx;
	d->dev = dev;
	d->type = dev->dev_type;
	/* setup device name and thread name */
	ret = snprintf(d->name, sizeof(d->name)-1, "%s", name);
	if (ret < 0)
		goto fail_set_name;
	ret = snprintf(thd_name, sizeof(thd_name)-1,
		"apusys_%s%d", name, d->idx);
	if (ret < 0)
		goto fail_set_name;

	ret = 0;
	list_add_tail(&d->t_item, &tab->list); // add list
	tab->array[d->idx] = d; // add array
	init_completion(&d->cmplt);
	init_completion(&d->thd_done);
	mutex_init(&d->mtx);

	tab->dev_num++;
	tab->avl_num++;

	d->exec = mdw_rsc_dev_exec;
	d->pwr_on = mdw_rsc_pwr_on;
	d->pwr_off = mdw_rsc_pwr_off;
	d->suspend = mdw_rsc_suspend;
	d->resume = mdw_rsc_resume;
	d->fw = mdw_rsc_fw;
	d->ucmd = mdw_rsc_ucmd;
	d->sec_on = mdw_rsc_sec_on;
	d->sec_off = mdw_rsc_sec_off;
	d->lock = mdw_rsc_lock_dev;
	d->unlock = mdw_rsc_unlock_dev;
	/* create kthd */
	d->thd = kthread_run(mdw_sched_dev_routine, d, thd_name);

	goto out;

fail_set_name:
fail_check_idx:
	vfree(d);
out:
	mutex_unlock(&tab->mtx);
	mdw_rsc_update_avl_bmp(dev->dev_type);
	mdw_flw_debug("register dev(%d-#%d) done\n", dev->dev_type, dev->idx);

	return ret;
}

static int mdw_rsc_delete_dev(struct mdw_dev_info *d)
{
	struct mdw_rsc_tab *tab = NULL;

	if (d->type >= APUSYS_DEVICE_MAX || d->type < 0 ||
		d->idx >= MDW_RSC_TAB_DEV_MAX || d->idx < 0)
		return -EINVAL;

	tab = mdw_rsc_get_tab(d->type);
	if (!tab)
		return -ENODEV;

	tab->array[d->idx] = NULL;
	d->stop = true;
	complete(&d->cmplt);
	wait_for_completion(&d->thd_done);
	tab->avl_num--;
	tab->dev_num--;
	list_del(&d->t_item);
	mdw_flw_debug("delete dev(%s%d) done\n", d->name, d->idx);
	vfree(d);

	return 0;
}

static int mdw_rsc_check_dev_state(struct mdw_dev_info *in)
{
	struct mdw_rsc_tab *tab = NULL;
	int type = 0;

	if (in->idx >= MDW_RSC_TAB_DEV_MAX ||
		in->idx < 0) {
		mdw_drv_err("dev(%s-#%d) idx over array size(%d)\n",
			in->name, in->idx, MDW_RSC_TAB_DEV_MAX);
		return -EINVAL;
	}

	if (in->type < APUSYS_DEVICE_RT)
		type = in->type + APUSYS_DEVICE_RT;
	else
		type = in->type % APUSYS_DEVICE_RT;

	tab = mdw_rsc_get_tab(type);
	if (!tab)
		return 0;

	return tab->array[in->idx]->state == MDW_DEV_INFO_STATE_IDLE
		? 0 : -EBUSY;
}

static struct mdw_dev_info *mdw_rsc_get_dev_sq(int type)
{
	int i = 0;
	struct mdw_rsc_tab *tab = NULL;
	struct mdw_dev_info *d = NULL, *first_d = NULL;

	tab = mdw_rsc_get_tab(type);
	if (!tab)
		return NULL;

	for (i = 0; i < tab->dev_num; i++) {
		if (tab->array[i]->state == MDW_DEV_INFO_STATE_IDLE) {
			d = tab->array[i];
			/* record first idle device */
			if (!first_d)
				first_d = d;

			/* check rt device state */
			if (!mdw_rsc_check_dev_state(d))
				break;
		}
		d = NULL;
	}

	/* if all device busy, assign first idle device */
	if (!d && first_d)
		d = first_d;
	/* remove device */
	if (d) {
		tab->avl_num--;
		list_del(&d->t_item);
		d->state = MDW_DEV_INFO_STATE_BUSY;
		mdw_flw_debug("dev(%s%d)\n", d->name, d->idx);
	}

	return d;
}

static int mdw_rsc_get_norm_prio(struct mdw_dev_info *in)
{
	struct mdw_rsc_tab *tab = NULL;
	struct mdw_dev_info *d = NULL;
	struct mdw_apu_sc *sc = NULL;
	int type = 0, prio = -ENOENT;

	if (in->idx >= MDW_RSC_TAB_DEV_MAX || in->idx < 0)
		return -EINVAL;

	type = in->type % APUSYS_DEVICE_RT;
	tab = mdw_rsc_get_tab(type);
	if (!tab)
		return -ENODEV;

	d = tab->array[in->idx];
	if (!d)
		return -ENODEV;

	mutex_lock(&d->mtx);
	if (d->sc) {
		sc = (struct mdw_apu_sc *)d->sc;
		prio = sc->parent->hdr->priority;
	}
	mutex_unlock(&d->mtx);

	return prio;
}

static struct mdw_dev_info *mdw_rsc_get_dev_rr(int type)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct mdw_rsc_tab *tab = NULL;
	struct mdw_dev_info *d = NULL, *d_tmp = NULL;
	int prio = 0, tmp_prio = 0;

	tab = mdw_rsc_get_tab(type);
	if (!tab)
		return NULL;

	/* check normal device state to make preempt prefer idle device */
	list_for_each_safe(list_ptr, tmp, &tab->list) {
		d = list_entry(list_ptr, struct mdw_dev_info, t_item);
		if (!mdw_rsc_check_dev_state(d))
			break;

		/* record if executing sc prio lower */
		tmp_prio = mdw_rsc_get_norm_prio(d);
		if (prio < tmp_prio) {
			prio = tmp_prio;
			d_tmp = d;
		}

		d = NULL;
	}

	if (d)
		goto lock_dev;

	/* no idle device, get device by current plcy*/
	switch (rsc_mgr.preempt_policy) {
	case MDW_PREEMPT_PLCY_RR_PRIORITY:
		if (d_tmp)
			d = d_tmp;
		else
			d = list_first_entry_or_null(&tab->list,
				struct mdw_dev_info, t_item);
		break;

	case MDW_PREEMPT_PLCY_RR_SIMPLE:
	default:
		d = list_first_entry_or_null(&tab->list,
			struct mdw_dev_info, t_item);
		break;
	}

lock_dev:
	if (d) {
		tab->avl_num--;
		list_del(&d->t_item);
		d->state = MDW_DEV_INFO_STATE_BUSY;
		mdw_flw_debug("dev(%s%d)\n", d->name, d->idx);
	}

	return d;
}

static void mdw_rsc_req_add(struct mdw_rsc_req *req)
{
	list_add_tail(&req->r_item, &rsc_mgr.r_list);
}

static void mdw_rsc_req_delete(struct mdw_rsc_req *req)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct mdw_rsc_req *tmp_req = NULL;

	list_for_each_safe(list_ptr, tmp, &rsc_mgr.r_list) {
		tmp_req = list_entry(list_ptr, struct mdw_rsc_req, r_item);
		if (tmp_req == req) {
			list_del(&req->r_item);
			return;
		}
	}
	mdw_drv_warn("req(%u/0x%llx) not in list\n",
		req->num[APUSYS_DEVICE_VPU], req->acq_bmp);
}

static void mdw_rsc_req_done(struct kref *ref)
{
	struct mdw_rsc_req *req =
			container_of(ref, struct mdw_rsc_req, ref);

	if (req->in_list == false)
		return;

	mdw_flw_debug("req(%p)\n", req);

	mdw_rsc_req_delete(req);
	complete(&req->complt);

	if (req->cb_async)
		req->cb_async(req);
}

static int mdw_rsc_req_add_dev(struct mdw_dev_info *d, struct mdw_rsc_req *req)
{
	if (d->type >= APUSYS_DEVICE_MAX || d->type < 0)
		return -EINVAL;

	mutex_lock(&req->mtx);
	/* check this dev is needed to req */
	if (!(req->acq_bmp & (1ULL << d->type)) ||
		req->get_num[d->type] >= req->num[d->type]) {
		mutex_unlock(&req->mtx);
		return -EINVAL;
	}
	mdw_flw_debug("put dev(%s%d) to req(%p), ref(%d)\n",
		d->name, d->idx, req, kref_read(&req->ref));

	/* add dev to req */
	list_add_tail(&d->r_item, &req->d_list);
	req->get_num[d->type]++;
	if (req->get_num[d->type] >= req->num[d->type])
		req->acq_bmp &= ~(1ULL << d->type);

	mutex_unlock(&req->mtx);

	/* check req done */
	kref_put(&req->ref, mdw_rsc_req_done);
	return 0;
}

int mdw_rsc_get_dev(struct mdw_rsc_req *req)
{
	struct mdw_dev_info *(*func)(int) = NULL;
	struct mdw_rsc_tab *tab = NULL;
	struct mdw_dev_info *d = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	int ret = 0, n = 0, i = 0, type = -1, get_total = 0;

	mdw_flw_debug("req(%p) bmp(0x%llx) total(%d) mode(%d) policy(%d)\n",
		req, req->acq_bmp, req->total_num, req->mode, req->policy);

	/* choose policy func ptr */
	if (req->policy == MDW_DEV_INFO_GET_POLICY_SEQ)
		func = mdw_rsc_get_dev_sq;
	else if (req->policy == MDW_DEV_INFO_GET_POLICY_RR)
		func = mdw_rsc_get_dev_rr;
	else
		return -EINVAL;

	init_completion(&req->complt);
	INIT_LIST_HEAD(&req->d_list);
	mutex_init(&req->mtx);
	req->in_list = false;
	refcount_set(&req->ref.refcount, req->total_num);
	mutex_lock(&rsc_mgr.mtx);

	while (1) {
		type = find_next_bit((unsigned long *)&req->acq_bmp,
			APUSYS_DEVICE_MAX, type + 1);
		mdw_flw_debug("dev(%d) bmp(0x%llx)\n", type, req->acq_bmp);
		if (type >= APUSYS_DEVICE_MAX || type < 0)
			break;

		tab = mdw_rsc_get_tab(type);
		if (!tab) {
			ret = -ENODEV;
			goto fail_get_tab;
		}

		if (req->num[type] > tab->dev_num) {
			ret = -EINVAL;
			goto fail_check_num;
		}

		mutex_lock(&tab->mtx);
		n = req->num[type] < tab->avl_num ?
			req->num[type] : tab->avl_num;
		for (i = 0; i < n; i++) {
			d = func(type);
			if (!d) {
				mdw_drv_warn("dev(%d) num conflict(%d/%d/%d/%d)\n",
					type, i, req->num[type],
					tab->avl_num, tab->dev_num);
				ret = -ENODEV;
				mutex_unlock(&tab->mtx);
				goto fail_first_get;
			}
			get_total++;
			if (mdw_rsc_req_add_dev(d, req))
				mdw_drv_err("req(%p) add dev(%s%d) fail\n",
					req, d->name, d->idx);
		}
		mutex_unlock(&tab->mtx);
		mdw_flw_debug("dev(%d) get(%d/%d)\n",
			type, req->get_num[type], req->num[type]);
	};

	mdw_flw_debug("bmp(0x%llx) get num(%d)\n", req->acq_bmp, get_total);

	if (req->mode == MDW_DEV_INFO_GET_MODE_TRY) {
		if (!get_total)
			ret = -ENODEV;
		mutex_unlock(&rsc_mgr.mtx);
		goto out;
	}

	if (req->acq_bmp) {
		/* add to rsc list */
		req->in_list = true;
		mdw_rsc_req_add(req);
		/* wait if sync mode */
		if (req->mode == MDW_DEV_INFO_GET_MODE_SYNC) {
			mutex_unlock(&rsc_mgr.mtx);
			ret = wait_for_completion_interruptible(&req->complt);
			mutex_lock(&rsc_mgr.mtx);
			if (ret) {
				mdw_drv_warn("wait sync completion(%d)\n", ret);
				mdw_rsc_req_delete(req);
				goto fail_wait_sync;
			}
		}
		if (req->mode == MDW_DEV_INFO_GET_MODE_ASYNC)
			ret = -EAGAIN;
	} else {
		/* call async cb if done */
		if (req->cb_async)
			req->cb_async(req);
	}

	mutex_unlock(&rsc_mgr.mtx);
	goto out;

fail_wait_sync:
fail_first_get:
fail_check_num:
fail_get_tab:
	mutex_unlock(&rsc_mgr.mtx);
	list_for_each_safe(list_ptr, tmp, &req->d_list) {
		d = list_entry(list_ptr, struct mdw_dev_info, r_item);
		list_del(&d->r_item);
		mdw_rsc_put_dev(d);
	}
out:
	mdw_rsc_update_avl_bmp(type);
	return ret;
}

static int mdw_rsc_put_dev_req(struct mdw_dev_info *d)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct mdw_rsc_req *req = NULL;
	int ret = -ENODATA;

	list_for_each_safe(list_ptr, tmp, &rsc_mgr.r_list) {
		req = list_entry(list_ptr, struct mdw_rsc_req, r_item);

		ret = mdw_rsc_req_add_dev(d, req);
		if (!ret)
			break;
	}

	return ret;
}

int mdw_rsc_put_dev(struct mdw_dev_info *d)
{
	struct mdw_rsc_tab *tab = NULL;
	int ret = 0;

	mdw_flw_debug("put dev(%s%d)...\n", d->name, d->idx);
	mutex_lock(&rsc_mgr.mtx);

	mutex_lock(&d->mtx);
	d->sc = NULL;
	mutex_unlock(&d->mtx);

	if (!list_empty(&rsc_mgr.r_list)) {
		if (!mdw_rsc_put_dev_req(d))
			goto out;
	}

	tab = mdw_rsc_get_tab(d->type);
	if (!tab) {
		ret = -ENODEV;
		goto out;
	}
	mdw_flw_debug("put dev(%s%d) to table\n", d->name, d->idx);

	mutex_lock(&tab->mtx);
	list_add_tail(&d->t_item, &tab->list);
	tab->avl_num++;
	d->state = MDW_DEV_INFO_STATE_IDLE;
	mutex_unlock(&tab->mtx);

out:
	mutex_unlock(&rsc_mgr.mtx);
	mdw_rsc_update_avl_bmp(d->type);
	mdw_sched(NULL);
	return ret;
}

struct mdw_dev_info *mdw_rsc_get_dinfo(int type, int idx)
{
	struct mdw_rsc_tab *tab = NULL;

	tab = mdw_rsc_get_tab(type);
	if (!tab)
		return NULL;

	if (idx >= tab->dev_num || idx < 0)
		return NULL;

	return tab->array[idx];
}

uint64_t mdw_rsc_get_dev_bmp(void)
{
	uint64_t b = 0;

	memcpy(&b, &rsc_mgr.dev_sup_bmp, sizeof(b));

	return b;
}

int mdw_rsc_get_dev_num(int type)
{
	struct mdw_rsc_tab *tab = NULL;

	tab = mdw_rsc_get_tab(type);
	if (!tab)
		return 0;

	return tab->dev_num;
}

void mdw_rsc_set_thd_group(void)
{
	struct file *fd;
	char buf[16];
	mm_segment_t oldfs;
	struct mdw_dev_info *d = NULL;
	int type = 0, idx = 0;

	if (atomic_read(&sthd_group))
		return;

	mdw_sched_set_thd_group();

	oldfs = get_fs();
	set_fs(get_ds());

	fd = filp_open(APUSYS_THD_TASK_FILE_PATH, O_WRONLY, 0);
	if (IS_ERR(fd)) {
		mdw_drv_debug("don't support low latency group\n");
		goto out;
	}

	mutex_lock(&rsc_mgr.mtx);
	for (type = 0; type < APUSYS_DEVICE_MAX; type++) {
		for (idx = 0; idx < mdw_rsc_get_dev_num(type); idx++) {
			d = mdw_rsc_get_dinfo(type, idx);
			if (!d)
				continue;
			memset(buf, 0, sizeof(buf));
			if (snprintf(buf, sizeof(buf)-1, "%d", d->thd->pid) < 0)
				goto fail_set_name;

			vfs_write(fd, (__force const char __user *)buf,
				sizeof(buf), &fd->f_pos);
			mdw_drv_debug("dev(%s%d) set thd(%d/%s)\n",
				d->name, d->idx, d->thd->pid, buf);
		}
	}
fail_set_name:
	mutex_unlock(&rsc_mgr.mtx);
	filp_close(fd, NULL);
out:
	set_fs(oldfs);
	atomic_inc(&sthd_group);
}

int mdw_rsc_set_preempt_plcy(uint32_t preempt_policy)
{
	if (preempt_policy >= MDW_PREEMPT_PLCY_MAX)
		return -EINVAL;

	rsc_mgr.preempt_policy = preempt_policy;

	return 0;
}

uint32_t mdw_rsc_get_preempt_plcy(void)
{
	return rsc_mgr.preempt_policy;
}

int mdw_rsc_init(void)
{
	memset(&rsc_mgr, 0, sizeof(rsc_mgr));
	rsc_mgr.tabs = vzalloc
		(sizeof(struct mdw_rsc_tab *) * APUSYS_DEVICE_MAX);

	bitmap_zero(rsc_mgr.cmd_avl_bmp, APUSYS_DEVICE_MAX);
	bitmap_zero(rsc_mgr.dev_avl_bmp, APUSYS_DEVICE_MAX);
	bitmap_zero(rsc_mgr.dev_sup_bmp, APUSYS_DEVICE_MAX);
	INIT_LIST_HEAD(&rsc_mgr.r_list);
	rsc_mgr.preempt_policy = MDW_PREEMPT_PLCY_RR_SIMPLE;
	mutex_init(&rsc_mgr.mtx);
	mdw_rsc_ws_init();

	return mdw_sched_init();
}

void mdw_rsc_exit(void)
{
	int type = 0, idx = 0;
	struct mdw_dev_info *d = NULL;
	struct mdw_rsc_tab *tab = NULL;

	mdw_sched_exit();

	for (type = 0; type < APUSYS_DEVICE_MAX; type++) {
		for (idx = 0; idx < mdw_rsc_get_dev_num(type); idx++) {
			d = mdw_rsc_get_dinfo(type, idx);
			if (!d)
				continue;
			if (mdw_rsc_delete_dev(d))
				mdw_drv_err("delete dev(%s%d) fail\n",
					d->name, d->idx);
		}
		tab = mdw_rsc_get_tab(type);
		if (tab)
			mdw_rsc_delete_tab(tab);
	}

	mdw_rsc_ws_destroy();
	vfree(rsc_mgr.tabs);
	mdw_flw_debug("\n");
}

int apusys_register_device(struct apusys_device *dev)
{
	int ret = 0;

	if (!dev)
		return -EINVAL;

	if (dev->dev_type > APUSYS_DEVICE_NONE &&
		dev->dev_type < APUSYS_DEVICE_MAX)
		ret = mdw_rsc_add_dev(dev);

	return ret;
}

int apusys_unregister_device(struct apusys_device *dev)
{
	struct mdw_rsc_tab *tab = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct mdw_dev_info *d = NULL;

	mdw_flw_debug("\n");

	tab = mdw_rsc_get_tab(dev->dev_type);
	if (!tab)
		return -ENODEV;

	mutex_lock(&tab->mtx);

	list_for_each_safe(list_ptr, tmp, &tab->list) {
		d = list_entry(list_ptr, struct mdw_dev_info, t_item);
		if (d->dev == dev)
			break;
		d = NULL;
	}

	if (d)
		mdw_rsc_delete_dev(d);

	mutex_unlock(&tab->mtx);

	return 0;
}
