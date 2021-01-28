/*
 * Copyright (C) 2020 MediaTek Inc.
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

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/list_sort.h>
#ifdef CONFIG_PM_SLEEP
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#endif

#include "apusys_drv.h"
#include "mdw_dbg.h"
#include "mdw_cmd.h"
#include "mdw_cmn.h"
#include "mdw_mem.h"
#include "mdw_usr.h"
#include "mdw_rsc.h"
#include "mdw_sched.h"
#include "mdw_trace.h"
#include "mdw_mem_aee.h"
#include "mdw_fence.h"

#define MDW_CMD_DEFAULT_TIMEOUT (30*1000) //30s


struct mdw_usr_stat {
	struct list_head list;
	struct mutex mtx;
};

#ifdef CONFIG_PM_SLEEP
static struct wakeup_source *mdw_usr_ws;
static uint32_t ws_cnt;
static struct mutex ws_mtx;
#endif

struct mdw_usr_mgr u_mgr;
static struct mdw_cmd_parser *cmd_parser;
static struct mdw_usr_stat u_stat;
static struct mdw_usr_mem_aee u_mem_aee;

#define LINEBAR \
	"|--------------------------------------------------------"\
	"---------------------------------------------------------|\n"
static int mdw_usr_pid_cmp(void *priv, struct list_head *a,
					struct list_head *b)
{
	struct mdw_usr *ua = NULL;
	struct mdw_usr *ub = NULL;

	ua = list_entry(a, struct mdw_usr, m_item);
	ub = list_entry(b, struct mdw_usr, m_item);

	/**
	 * List_sort is a stable sort, so it is not necessary to distinguish
	 * the @a < @b and @a == @b cases.
	 */
	if (ua->pid > ub->pid)
		return 1;

	return -1;

}

static int mdw_usr_mem_size_cmp(void *priv, struct list_head *a,
					struct list_head *b)
{
	struct mdw_usr *ua = NULL;
	struct mdw_usr *ub = NULL;

	ua = list_entry(a, struct mdw_usr, m_item);
	ub = list_entry(b, struct mdw_usr, m_item);

	/**
	 * List_sort is a stable sort, so it is not necessary to distinguish
	 * the @a < @b and @a == @b cases.
	 */
	if (ua->iova_size > ub->iova_size)
		return 1;

	return -1;

}
void mdw_usr_print_mem_usage(void)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct mdw_usr *user = NULL;
	struct mdw_usr *u = NULL;
	struct mdw_usr u_tmp;
	unsigned int total_size = 0;
	unsigned int percentage = 0;
	char *cur, *end;


	memset(&u_tmp, 0, sizeof(struct mdw_usr));

	mutex_lock(&u_mem_aee.mtx);
	cur = u_mem_aee.log_buf;
	end = u_mem_aee.log_buf + DUMP_LOG_SIZE;

	mdw_drv_err("name, id, pid, tgid, iova_size, iova_size_max\n");
	DUMP_LOG(cur, end, "name, id, pid, tgid, iova_size, iova_size_max\n");

	mutex_lock(&u_mgr.mtx);
	mutex_lock(&u_stat.mtx);

	/* Sort by PID*/
	list_sort(NULL, &u_mgr.list, mdw_usr_pid_cmp);
	mdw_drv_err("----- APUSYS user -----\n");
	DUMP_LOG(cur, end, "----- APUSYS user -----\n");
	list_for_each_safe(list_ptr, tmp, &u_mgr.list) {
		user = list_entry(list_ptr, struct mdw_usr, m_item);

		total_size = total_size + user->iova_size;
		mdw_drv_err("%s, %llx, %d, %d, %u, %u\n",
				user->comm, user->id, user->pid,
				user->tgid, user->iova_size,
				user->iova_size_max);
		DUMP_LOG(cur, end, "%s, %llx, %d, %d, %u, %u\n",
				user->comm, user->id, user->pid,
				user->tgid, user->iova_size,
				user->iova_size_max);

		if (u_tmp.pid != user->pid) {

			u = kzalloc(sizeof(struct mdw_usr), GFP_KERNEL);
			if (u == NULL)
				goto free_mutex;
			memcpy(u, user, sizeof(struct mdw_usr));

			//Force string end
			u->comm[TASK_COMM_LEN-1] = '\0';

			list_add_tail(&u->m_item, &u_stat.list);

			u_tmp.pid = user->pid;
		} else {
			if (u == NULL)
				goto free_mutex;
			u->iova_size = u->iova_size + user->iova_size;
			u->iova_size_max =
					u->iova_size_max + user->iova_size_max;
		}
	}

	if (!list_empty(&u_stat.list)) {
		/* Sort by PID*/
		list_sort(NULL, &u_stat.list, mdw_usr_mem_size_cmp);

		mdw_drv_err("----- APUSYS statistics -----\n");
		DUMP_LOG(cur, end, "----- APUSYS statistics -----\n");

		list_for_each_safe(list_ptr, tmp, &u_stat.list) {
			u = list_entry(list_ptr, struct mdw_usr, m_item);

			if (total_size != 0) {
				percentage = (uint64_t) u->iova_size * 100
					/ (uint64_t)total_size;
			} else {
				percentage = 0;
			}

		mdw_drv_err("%s, %llx, %d, %d, %u, %u, %u, %u%%\n",
			u->comm, u->id, u->pid,
			u->tgid, u->iova_size,
			u->iova_size_max, total_size, percentage);
		DUMP_LOG(cur, end, "%s, %llx, %d, %d, %u, %u, %u, %u%%\n",
			u->comm, u->id, u->pid,
			u->tgid, u->iova_size,
			u->iova_size_max, total_size, percentage);
		}

		/* The last one is the top memory user*/
		mdw_drv_err("----- APUSYS top user -----\n");
		DUMP_LOG(cur, end, "----- APUSYS top user -----\n");
		mdw_drv_err("%s, %llx, %d, %d, %u, %u, %u, %u%%\n",
				u->comm, u->id, u->pid,
				u->tgid, u->iova_size,
				u->iova_size_max, total_size, percentage);
		DUMP_LOG(cur, end, "%s, %llx, %d, %d, %u, %u, %u, %u%%\n",
				u->comm, u->id, u->pid,
				u->tgid, u->iova_size,
				u->iova_size_max, total_size, percentage);
		/*delete statistics list*/
		list_for_each_safe(list_ptr, tmp, &u_stat.list) {
			u = list_entry(list_ptr, struct mdw_usr, m_item);
			list_del(list_ptr);
			kfree(u);
		}
	}


free_mutex:
	mutex_unlock(&u_stat.mtx);
	mutex_unlock(&u_mgr.mtx);
	mutex_unlock(&u_mem_aee.mtx);
}

void mdw_usr_aee_mem(void *s_file)
{
	struct seq_file *s = (struct seq_file *)s_file;

	mutex_lock(&u_mem_aee.mtx);
	seq_printf(s, "%s", u_mem_aee.log_buf);
	mutex_unlock(&u_mem_aee.mtx);
}

static void mdw_usr_dump_sdev(struct seq_file *s, struct mdw_usr *u)
{
	int cnt = 0;
	struct mdw_dev_info *d = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	/* device */
	list_for_each_safe(list_ptr, tmp, &u->sdev_list) {
		d = list_entry(list_ptr, struct mdw_dev_info, u_item);

		mdw_con_info(s, "| %-9d| %-5d| %-5d| %-19s| %-66d|\n",
			cnt,
			d->type,
			d->idx,
			d->name,
			d->state);
		cnt++;
	}
	mdw_con_info(s, LINEBAR);
}

static void mdw_usr_dump_mem(struct seq_file *s, struct mdw_usr *u)
{
	int cnt = 0;
	struct mdw_mem *mm = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	mdw_con_info(s,
	"|%-10s|%-6s|%-6s|%-20s|%-12s|%-20s|%-12s|%-20s|\n",
		" mem",
		" type",
		" fd",
		" uva",
		" size",
		" iova",
		" iova size",
		" kva");
	mdw_con_info(s, LINEBAR);
	list_for_each_safe(list_ptr, tmp, &u->mem_list) {
		mm = list_entry(list_ptr, struct mdw_mem, u_item);

		mdw_con_info(s,
		"| #%-8d| %-5u| %-5u| 0x%-17llx| %-11d| 0x%-17x| 0x%-9x| 0x%-17llx|\n",
			cnt,
			mm->kmem.mem_type,
			mm->kmem.fd,
			mm->kmem.uva,
			mm->kmem.size,
			mm->kmem.iova,
			mm->kmem.iova_size,
			mm->kmem.kva);
		cnt++;
	}
	mdw_con_info(s, LINEBAR);
}

static void mdw_usr_dump_cmd(struct seq_file *s, struct mdw_usr *u)
{
	int cnt = 0;
	struct mdw_apu_cmd *c;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	mdw_con_info(s, "|%-10s|%-13s|%-33s|%-33s|%-20s|\n",
		" cmd",
		" priority",
		" uid",
		" id",
		" sc num");
	mdw_con_info(s, LINEBAR);
	list_for_each_safe(list_ptr, tmp, &u->cmd_list) {
		c = list_entry(list_ptr,
			struct mdw_apu_cmd, u_item);
		mutex_lock(&c->mtx);

		mdw_con_info(s,
		"| #%-8d| %-12d| 0x%-30llx| 0x%-30llx| %-19u|\n",
			cnt,
			c->hdr->priority,
			c->hdr->uid,
			c->kid,
			c->hdr->num_sc);
		mutex_unlock(&c->mtx);
		cnt++;
	}
	mdw_con_info(s, LINEBAR);
}

void mdw_usr_dump(struct seq_file *s)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct mdw_usr *u = NULL;
	int u_cnt = 0;

	mdw_con_info(s, LINEBAR);
	mdw_con_info(s, "|%-113s|\n",
		" apusys user table");
	mdw_con_info(s, LINEBAR);

	mutex_lock(&u_mgr.mtx);
	list_for_each_safe(list_ptr, tmp, &u_mgr.list) {
		u = list_entry(list_ptr, struct mdw_usr, m_item);
		mutex_lock(&u->mtx);

		mdw_con_info(s, "| user (#%-3d)(%-16s)%83s|\n",
			u_cnt, u->comm,
			"");
		mdw_con_info(s, "| pid           = %-96d|\n",
			u->pid);
		mdw_con_info(s, "| tgid          = %-96d|\n",
			u->tgid);
		mdw_con_info(s, "| iova size max = %-96u|\n",
			u->iova_size_max);
		mdw_con_info(s, "| iova size     = %-96u|\n",
			u->iova_size);
		mdw_con_info(s, LINEBAR);

		mdw_usr_dump_cmd(s, u);
		mdw_usr_dump_mem(s, u);
		mdw_usr_dump_sdev(s, u);

		mdw_con_info(s, LINEBAR);
		u_cnt++;
		mutex_unlock(&u->mtx);
	}
	mutex_unlock(&u_mgr.mtx);
}
#undef LINEBAR


//----------------------------------------
static void mdw_usr_ws_init(void)
{
#if defined CONFIG_PM_SLEEP
	char ws_name[16];

	if (snprintf(ws_name, sizeof(ws_name)-1, "apusys_usr") < 0) {
		mdw_drv_err("init rsc wakeup source fail\n");
		return;
	}
	ws_cnt = 0;
	mutex_init(&ws_mtx);
	mdw_usr_ws = wakeup_source_register(NULL, (const char *)ws_name);
	if (!mdw_usr_ws)
		mdw_drv_err("register ws lock fail!\n");
#else
	mdw_drv_debug("not support pm wakelock\n");
#endif
}

static void mdw_usr_ws_destroy(void)
{
#if defined CONFIG_PM_SLEEP
	ws_cnt = 0;
	wakeup_source_unregister(mdw_usr_ws);
#else
	mdw_drv_debug("not support pm wakelock\n");
#endif
}

void mdw_usr_ws_lock(void)
{
#ifdef CONFIG_PM_SLEEP
	mutex_lock(&ws_mtx);
	if (mdw_usr_ws && !ws_cnt) {
		mdw_flw_debug("lock\n");
		__pm_stay_awake(mdw_usr_ws);
	}
	ws_cnt++;
	mutex_unlock(&ws_mtx);
#else
	mdw_flw_debug("not support pm wakelock\n");
#endif
}

void mdw_usr_ws_unlock(void)
{
#ifdef CONFIG_PM_SLEEP
	mutex_lock(&ws_mtx);
	ws_cnt--;
	if (mdw_usr_ws && !ws_cnt) {
		mdw_flw_debug("unlock\n");
		__pm_relax(mdw_usr_ws);
	}
	mutex_unlock(&ws_mtx);
#else
	mdw_flw_debug("not support pm wakelock\n");
#endif
}

static struct mdw_mem *mdw_usr_mem_create(struct apusys_mem *um, int mem_op)
{
	int ret = 0;
	struct mdw_mem *mm = NULL;

	mm = vzalloc(sizeof(struct mdw_mem));
	if (!mm)
		return NULL;

	/* alloc mem */
	mdw_mem_u2k(um, &mm->kmem);
	switch (mem_op) {
	case APUSYS_MEM_PROP_ALLOC:
		ret = mdw_mem_alloc(mm);
		break;
	case APUSYS_MEM_PROP_IMPORT:
		ret = mdw_mem_import(mm);
		break;
	case APUSYS_MEM_PROP_MAP:
		ret = mdw_mem_map(mm);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (ret) {
		if (ret == -ENOMEM) {
			mdw_usr_print_mem_usage();
			//TODO Change to Tag
			apusys_aee_print("mem fail");
		}
		vfree(mm);
		mm = NULL;
	} else {
		mdw_mem_k2u(&mm->kmem, um);
	}

	return mm;
}

static int mdw_usr_mem_delete(struct mdw_mem *mm)
{
	int ret = 0;

	if (!mm)
		return -ENODATA;

	switch (mm->kmem.property) {
	case APUSYS_MEM_PROP_ALLOC:
		ret = mdw_mem_free(mm);
		break;
	case APUSYS_MEM_PROP_IMPORT:
		ret = mdw_mem_unimport(mm);
		break;
	case APUSYS_MEM_PROP_MAP:
		ret = mdw_mem_unmap(mm);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	vfree(mm);

	return ret;
}

int mdw_usr_mem_alloc(struct apusys_mem *um, struct mdw_usr *u)
{
	struct mdw_mem *mm = NULL;

	mm = mdw_usr_mem_create(um, APUSYS_MEM_PROP_ALLOC);
	if (!mm)
		return -ENOMEM;

	/* add to user list */
	mutex_lock(&u->mtx);
	list_add_tail(&mm->u_item, &u->mem_list);

	u->iova_size = u->iova_size + mm->kmem.iova_size;
	if (u->iova_size_max < u->iova_size)
		u->iova_size_max = u->iova_size;

	mutex_unlock(&u->mtx);

	return 0;
}

int mdw_usr_mem_free(struct apusys_mem *um, struct mdw_usr *u)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct mdw_mem *mm = NULL;

	/* get mem from usr list */
	mutex_lock(&u->mtx);
	list_for_each_safe(list_ptr, tmp, &u->mem_list) {
		mm = list_entry(list_ptr, struct mdw_mem, u_item);

		if (mm->kmem.fd == um->fd &&
			mm->kmem.mem_type == um->mem_type) {
			mdw_flw_debug("get mem fd(%d) type(%d)\n",
				mm->kmem.fd, mm->kmem.mem_type);
			list_del(&mm->u_item);
			break;
		}
		mm = NULL;
	}

	if (mm)
		u->iova_size = u->iova_size - mm->kmem.iova_size;

	mutex_unlock(&u->mtx);

	return mdw_usr_mem_delete(mm);
}

int mdw_usr_mem_import(struct apusys_mem *um, struct mdw_usr *u)
{
	struct mdw_mem *mm = NULL;

	mm = mdw_usr_mem_create(um, APUSYS_MEM_PROP_IMPORT);
	if (!mm)
		return -ENOMEM;

	/* add to user list */
	mutex_lock(&u->mtx);
	list_add_tail(&mm->u_item, &u->mem_list);

	u->iova_size = u->iova_size + mm->kmem.iova_size;
	if (u->iova_size_max < u->iova_size)
		u->iova_size_max = u->iova_size;

	mutex_unlock(&u->mtx);

	return 0;
}

int mdw_usr_mem_map(struct apusys_mem *um, struct mdw_usr *u)
{
	struct mdw_mem *mm = NULL;

	mm = mdw_usr_mem_create(um, APUSYS_MEM_PROP_MAP);
	if (!mm)
		return -ENOMEM;

	/* add to user list */
	mutex_lock(&u->mtx);
	list_add_tail(&mm->u_item, &u->mem_list);

	u->iova_size = u->iova_size + mm->kmem.iova_size;
	if (u->iova_size_max < u->iova_size)
		u->iova_size_max = u->iova_size;

	mutex_unlock(&u->mtx);

	return 0;
}

int mdw_usr_dev_sec_alloc(int type, struct mdw_usr *u)
{
	struct mdw_rsc_req req;
	struct mdw_dev_info *d = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	int ret = 0;

	if (type >= APUSYS_DEVICE_RT || type < 0)
		return -ENODEV;

	mutex_lock(&u->mtx);
	mdw_drv_info("alloc dev(%d)\n", type);

	/* alloc rt device */
	memset(&req, 0, sizeof(req));
	req.num[type] = mdw_rsc_get_dev_num(type);
	if (req.num[type])
		req.acq_bmp |= (1ULL << type);
	req.num[type + APUSYS_DEVICE_RT] =
		mdw_rsc_get_dev_num(type + APUSYS_DEVICE_RT);
	if (req.num[type + APUSYS_DEVICE_RT])
		req.acq_bmp |= (1ULL << (type + APUSYS_DEVICE_RT));

	req.total_num = req.num[type] + req.num[type + APUSYS_DEVICE_RT];
	req.mode = MDW_DEV_INFO_GET_MODE_SYNC;
	req.policy = MDW_DEV_INFO_GET_POLICY_RR;

	ret = mdw_rsc_get_dev(&req);
	if (ret) {
		mdw_drv_err("alloc dev(%d/0x%llx)fail(%d)\n",
			type, req.acq_bmp, ret);
		goto out;
	}

	/* power off & on device */
	list_for_each_safe(list_ptr, tmp, &req.d_list) {
		d = list_entry(list_ptr, struct mdw_dev_info, r_item);
		mdw_flw_debug("pwn on dev(%s%d)\n", d->name, d->idx);
		/* don't control power of rt device  */
		if (d->type >= APUSYS_DEVICE_RT)
			goto next;

		ret = d->pwr_off(d);
		if (ret) {
			mdw_drv_warn("pwr down dev(%s%d) fail(%d)\n",
				d->name, d->idx, ret);
			goto fail_pwr_down;
		}

		ret = d->pwr_on(d, 100, MDW_RSC_SET_PWR_ALLON);
		if (ret) {
			mdw_drv_warn("pwr on dev(%s%d) fail(%d)\n",
				d->name, d->idx, ret);
			goto fail_pwr_on;
		}
next:
		list_del(&d->r_item);
		list_add_tail(&d->u_item, &u->sdev_list);
	}

	/* secure on */
	d = mdw_rsc_get_dinfo(type, 0);
	if (!d) {
		mdw_drv_warn("no dev(%d/%d)\n", type, 0);
		ret =  -ENODEV;
		goto fail_sec_on;
	}
	ret = d->sec_on(d);
	if (ret) {
		mdw_drv_warn("sec dev(%s%d) fail(%d)\n", d->name, d->idx, ret);
		goto fail_sec_on;
	}

	goto out;

fail_sec_on:
fail_pwr_on:
fail_pwr_down:
	list_for_each_safe(list_ptr, tmp, &req.d_list) {
		d = list_entry(list_ptr, struct mdw_dev_info, r_item);
		if (d->type < APUSYS_DEVICE_RT)
			d->pwr_off(d);

		list_del(&d->r_item);
		mdw_rsc_put_dev(d);
	}

	list_for_each_safe(list_ptr, tmp, &u->sdev_list) {
		d = list_entry(list_ptr, struct mdw_dev_info, u_item);

		if (d->type != type && d->type != type + APUSYS_DEVICE_RT)
			continue;

		if (d->type < APUSYS_DEVICE_RT)
			d->pwr_off(d);
		list_del(&d->u_item);
		mdw_rsc_put_dev(d);
	}
out:
	mutex_unlock(&u->mtx);
	mdw_drv_info("alloc dev(%d) done(%d)\n", type, ret);
	return ret;
}

int mdw_usr_dev_sec_free(int type, struct mdw_usr *u)
{
	struct mdw_dev_info *d = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	int ret = 0;

	d = mdw_rsc_get_dinfo(type, 0);
	if (!d)
		return -ENODEV;

	mutex_lock(&u->mtx);
	mdw_drv_info("free dev(%d)\n", type);
	ret = d->sec_off(d);
	if (ret) {
		mdw_drv_warn("sec off dev(%s%d) fail(%d)\n",
			d->name, d->idx, ret);
		goto out;
	}
	list_for_each_safe(list_ptr, tmp, &u->sdev_list) {
		d = list_entry(list_ptr, struct mdw_dev_info, u_item);

		if (d->type != type && d->type != type + APUSYS_DEVICE_RT)
			continue;

		if (d->type < APUSYS_DEVICE_RT)
			ret = d->pwr_off(d);
		if (ret)
			mdw_drv_warn("pwr down dev(%s%d) fail(%d)\n",
				d->name, d->idx, ret);

		list_del(&d->u_item);
		mdw_rsc_put_dev(d);
	}

out:
	mutex_unlock(&u->mtx);
	mdw_drv_info("free dev(%d) done\n", type);
	return ret;
}

int mdw_usr_fw(struct apusys_ioctl_fw *f, int op)
{
	struct apusys_kmem km;
	struct mdw_dev_info *d = NULL;
	int ret = 0;

	memset(&km, 0, sizeof(km));

	km.fd = f->mem_fd;
	km.size = f->size;
	ret = mdw_mem_map_kva(&km);
	if (ret)
		goto out;
	ret = mdw_mem_map_iova(&km);
	if (ret)
		goto fail_map_iova;

	if (f->size + f->offset > km.iova_size) {
		ret = -EINVAL;
		goto fail_check_size;
	}

	d = mdw_rsc_get_dinfo(f->dev_type, f->idx);
	if (!d) {
		ret = -ENODEV;
		goto fail_get_dev;
	}

	ret = d->fw(d, f->magic, f->name, km.kva + f->offset,
		km.iova + f->offset, f->size, op);

fail_get_dev:
fail_check_size:
	mdw_mem_unmap_iova(&km);
fail_map_iova:
	mdw_mem_unmap_kva(&km);
out:
	return ret;
}

int mdw_usr_ucmd(struct apusys_ioctl_ucmd *uc)
{
	struct apusys_kmem km;
	struct mdw_dev_info *d = NULL;
	int ret = 0;

	memset(&km, 0, sizeof(km));

	km.fd = uc->mem_fd;
	km.size = uc->size;
	ret = mdw_mem_map_kva(&km);
	if (ret)
		goto out;
	ret = mdw_mem_map_iova(&km);
	if (ret)
		goto fail_map_iova;

	if (uc->size + uc->offset > km.iova_size) {
		ret = -EINVAL;
		goto fail_check_size;
	}

	d = mdw_rsc_get_dinfo(uc->dev_type, uc->idx);
	if (!d) {
		ret = -ENODEV;
		goto fail_get_dev;
	}

	ret = d->ucmd(d, km.kva, km.iova, uc->size);

fail_get_dev:
fail_check_size:
	mdw_mem_unmap_iova(&km);
fail_map_iova:
	mdw_mem_unmap_kva(&km);
out:
	return ret;
}

int mdw_usr_set_pwr(struct apusys_ioctl_power *pwr)
{
	struct mdw_dev_info *d = NULL;

	d = mdw_rsc_get_dinfo(pwr->dev_type, pwr->idx);
	if (!d)
		return -ENODEV;

	return d->pwr_on(d, pwr->boost_val, MDW_RSC_SET_PWR_TIMEOUT);
}

static int mdw_usr_par_apu_cmd(struct mdw_apu_cmd *c)
{
	struct mdw_apu_sc *sc;
	int ret = 0;

	mdw_trace_begin("cmd parse|cmd(0x%llx)", c->kid);

	while (1) {
		ret = cmd_parser->parse_cmd(c, &sc);
		/* check return value */
		if (ret < 0) { /* get sc fail */
			mdw_drv_err("parse cmd fail(%d)\n", ret);
			break;
		} else if (ret > 0) {
			if (sc)
				mdw_sched(sc);
		} else {
			if (sc)
				mdw_sched(sc);

			mdw_flw_debug("apu cmd(0x%llx) parse done(%d)\n",
				c->hdr->uid, ret);
			break;
		}
	}
	mdw_trace_end("cmd parse|cmd(0x%llx)", c->kid);

	return ret;
}

int mdw_usr_run_cmd_async(struct mdw_usr *u, struct apusys_ioctl_cmd *in)
{
	int ret = 0;
	struct mdw_apu_cmd *c = NULL;

	c = cmd_parser->create_cmd(in->mem_fd, in->size, in->offset, u);
	if (!c) {
		ret = -EINVAL;
		goto create_cmd_fail;
	}
	c->pid = u->pid;
	c->tgid = u->tgid;
	c->usr = u;

	in->cmd_id = c->kid;

	ret = mdw_usr_par_apu_cmd(c);
	if (ret)
		goto parse_cmd_fail;

	mutex_lock(&u->mtx);
	list_add_tail(&c->u_item, &u->cmd_list);
	mutex_unlock(&u->mtx);

	goto out;

parse_cmd_fail:
	cmd_parser->abort_cmd(c);
create_cmd_fail:
out:
	return ret;
}

int mdw_wait_cmd(struct mdw_apu_cmd *c)
{
	int ret = 0, retry = 100, retry_time = 50;
	unsigned long timeout = 0;

	mdw_usr_ws_lock();

	/* setup timeout */
	if (c->hdr->hard_limit)
		timeout = msecs_to_jiffies(c->hdr->hard_limit);
	else
		timeout = msecs_to_jiffies(MDW_CMD_DEFAULT_TIMEOUT);

rewait:
	ret = wait_for_completion_interruptible_timeout(&c->cmplt, timeout);
	if (ret == -ERESTARTSYS) { //restart
		if (retry) {
			if (!(retry % 20))
				mdw_drv_warn("cmd(0x%llx) rewait(%d)\n",
					c->kid, retry);

			retry--;
			msleep(retry_time);
			goto rewait;
		} else {
			mdw_drv_warn("cmd(0x%llx) leak\n", c->kid);
		}

	} else if (ret == 0) { //timeout
		mdw_drv_err("cmd(0x%llx) timeout ms(%u)\n",
			c->kid, jiffies_to_msecs(timeout));
		ret = -ETIME;

	} else if (ret < 0) { //error
		mdw_drv_err("cmd(0x%llx) fail(%d)\n", c->kid, ret);

	}

	/* Remove u_item anyway */
	mutex_lock(&c->usr->mtx);
	list_del(&c->u_item);
	if (c->file)
		c->file->private_data = NULL;
	mutex_unlock(&c->usr->mtx);

	if (ret < 0) { /* Wait fail handle */
		if (mdw_dbg_get_prop(MDW_DBG_PROP_CMD_TIMEOUT_AEE))
			mdw_dbg_aee("apusys midware wait timeout");
		cmd_parser->abort_cmd(c);
	} else { /* Wait done, delete cmd */
		if (cmd_parser->delete_cmd(c))
			mdw_drv_err("delete cmd fail\n");
		ret = 0;
	}

	mdw_usr_ws_unlock();

	return ret;
}


int mdw_usr_wait_cmd(struct mdw_usr *u, struct apusys_ioctl_cmd *in)
{
	struct mdw_apu_cmd *c = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	mdw_flw_debug("\n");

	/* get mem from usr list */
	mutex_lock(&u->mtx);
	list_for_each_safe(list_ptr, tmp, &u->cmd_list) {
		c = list_entry(list_ptr, struct mdw_apu_cmd, u_item);
		mdw_flw_debug("cmd(0x%llx/0x%llx) matching...\n",
			c->kid, in->cmd_id);

		if (c->kid == in->cmd_id)
			break;

		c = NULL;
	}
	mutex_unlock(&u->mtx);

	if (!c) {
		mdw_drv_err("no cmd(0x%llx) to wait\n", in->cmd_id);
		return -EINVAL;
	}
	mdw_flw_debug("wait cmd(0x%llx/0x%llx)\n", c->kid, c->hdr->uid);

	return mdw_wait_cmd(c);
}

int mdw_usr_run_cmd_sync(struct mdw_usr *u, struct apusys_ioctl_cmd *in)
{
	int ret = 0;

	ret = mdw_usr_run_cmd_async(u, in);
	if (ret)
		return ret;

	return mdw_usr_wait_cmd(u, in);
}

struct mdw_usr *mdw_usr_create(void)
{
	struct mdw_usr *u = NULL;

	/* setup thread group */
	mdw_rsc_set_thd_group();

	u = vzalloc(sizeof(struct mdw_usr));
	if (!u)
		return NULL;

	INIT_LIST_HEAD(&u->cmd_list);
	INIT_LIST_HEAD(&u->sdev_list);
	INIT_LIST_HEAD(&u->mem_list);
	mutex_init(&u->mtx);
	get_task_comm(u->comm, current);
	u->pid = current->pid;
	u->tgid = current->tgid;
	u->id = (uint64_t)u;

	mutex_lock(&u_mgr.mtx);
	list_add_tail(&u->m_item, &u_mgr.list);
	mutex_unlock(&u_mgr.mtx);

	return u;
}

void mdw_usr_destroy(struct mdw_usr *u)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct mdw_mem *mm = NULL;
	struct mdw_apu_cmd *c = NULL;
	struct mdw_dev_info *d = NULL;

	mutex_lock(&u_mgr.mtx);
	mutex_lock(&u->mtx);

	list_for_each_safe(list_ptr, tmp, &u->cmd_list) {
		c = list_entry(list_ptr, struct mdw_apu_cmd, u_item);
		list_del(&c->u_item);
		mdw_drv_warn("residual cmd(0x%llx)\n", c->kid);
		cmd_parser->abort_cmd(c);
	}

	list_for_each_safe(list_ptr, tmp, &u->mem_list) {
		mm = list_entry(list_ptr, struct mdw_mem, u_item);
		list_del(&mm->u_item);
		mdw_drv_warn("residual mem(0x%x)\n", mm->kmem.iova);
		mdw_usr_mem_delete(mm);
	}

	list_for_each_safe(list_ptr, tmp, &u->sdev_list) {
		d = list_entry(list_ptr, struct mdw_dev_info, u_item);
		list_del(&d->u_item);
		mdw_drv_warn("residual dev(%s%d)\n", d->name, d->idx);
		d->pwr_off(d);
		mdw_rsc_put_dev(d);
	}
	if (d)
		d->sec_off(d);

	mutex_unlock(&u->mtx);
	list_del(&u->m_item);
	mutex_unlock(&u_mgr.mtx);

	vfree(u);
}

int mdw_usr_init(void)
{
	/*mem aee init*/
	memset(&u_mem_aee, 0, sizeof(struct mdw_usr_mem_aee));
	mutex_init(&u_mem_aee.mtx);
	/*stat init*/
	memset(&u_stat, 0, sizeof(u_stat));
	INIT_LIST_HEAD(&u_stat.list);
	mutex_init(&u_stat.mtx);
	/*mgr init*/
	memset(&u_mgr, 0, sizeof(u_mgr));
	INIT_LIST_HEAD(&u_mgr.list);
	mutex_init(&u_mgr.mtx);
	mdw_usr_ws_init();

	cmd_parser = mdw_cmd_get_parser();
	if (!cmd_parser)
		return -ENODEV;

	return 0;
}

void mdw_usr_exit(void)
{
	mdw_usr_ws_destroy();
}
