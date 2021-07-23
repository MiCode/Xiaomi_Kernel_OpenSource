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
#include <linux/slab.h>
#include <linux/list_sort.h>

#include "mdw_cmn.h"
#include "apusys_user.h"
#include "apusys_drv.h"
#include "scheduler.h"
#include "cmd_parser.h"
#include "memory_mgt.h"
#include "resource_mgt.h"
#include "memory_dump.h"

struct apusys_user_mem {
	struct apusys_kmem mem;
	struct list_head list;

};

struct apusys_user_dev {
	struct apusys_dev_info *dev_info;
	struct list_head list;
};

struct apusys_user_mgr {
	struct list_head list;
	struct mutex mtx;
};

struct apusys_user_log {
	unsigned char log_buf[DUMP_LOG_SIZE];
	struct mutex mtx;
};

struct apusys_user_statistics {
	struct list_head list;
	struct mutex mtx;
};


static struct apusys_user_mgr g_user_mgr;
static struct apusys_user_log g_user_log;
static struct apusys_user_statistics g_user_stat;

void apusys_user_dump(void *s_file)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_user *user = NULL;
	struct apusys_cmd *cmd = NULL;
	struct apusys_user_mem *u_mem = NULL;
	struct list_head *d_tmp = NULL, *d_list_ptr = NULL;
	struct list_head *c_tmp = NULL, *c_list_ptr = NULL;
	struct list_head *m_tmp = NULL, *m_list_ptr = NULL;
	struct apusys_user_dev *u_dev = NULL;
	struct apusys_res_table *tab = NULL;
	struct seq_file *s = (struct seq_file *)s_file;
	int u_count = 0;
	int d_count = 0;
	int m_count = 0;
	int c_count = 0;

#define LINEBAR \
	"|--------------------------------------------------------"\
	"---------------------------------------------------------|\n"

	mdw_con_info(s, LINEBAR);
	mdw_con_info(s, "|%-113s|\n",
		" apusys user table");
	mdw_con_info(s, LINEBAR);

	mutex_lock(&g_user_mgr.mtx);
	list_for_each_safe(list_ptr, tmp, &g_user_mgr.list) {
		user = list_entry(list_ptr, struct apusys_user, list);

		mdw_con_info(s, "| user (#%-3d)(%16s)%83s|\n",
			u_count, user->comm,
			"");
		mdw_con_info(s, "| id            = 0x%-94llx|\n",
			user->id);
		mdw_con_info(s, "| pid           = %-96d|\n",
			user->open_pid);
		mdw_con_info(s, "| tgid          = %-96d|\n",
			user->open_tgid);
		mdw_con_info(s, "| iova_size_max = %-96u|\n",
			user->iova_size_max);
		mdw_con_info(s, "| iova_size     = %-96u|\n",
			user->iova_size);
		mdw_con_info(s, LINEBAR);

		c_count = 0;
		d_count = 0;
		m_count = 0;

		/* cmd */
		mdw_con_info(s, "|%-10s|%-13s|%-33s|%-33s|%-20s|\n",
			" cmd",
			" priority",
			" uid",
			" id",
			" sc num");
		mdw_con_info(s, LINEBAR);
		list_for_each_safe(c_list_ptr, c_tmp, &user->cmd_list) {
			cmd = list_entry(c_list_ptr,
				struct apusys_cmd, u_list);
			mutex_lock(&cmd->mtx);

			mdw_con_info(s,
			"| #%-8d| %-12d| 0x%-30llx| 0x%-30llx| %-19u|\n",
				c_count,
				cmd->hdr->priority,
				cmd->hdr->uid,
				cmd->cmd_id,
				cmd->hdr->num_sc);
			mutex_unlock(&cmd->mtx);
			c_count++;
		}
		mdw_con_info(s, LINEBAR);

		/* mem */
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
		list_for_each_safe(m_list_ptr, m_tmp, &user->mem_list) {
			u_mem = list_entry(m_list_ptr,
				struct apusys_user_mem, list);

			mdw_con_info(s,
			"| #%-8d| %-5u| %-5u| 0x%-17llx| %-11d| 0x%-17x| 0x%-9x| 0x%-17llx|\n",
				m_count,
				u_mem->mem.mem_type,
				u_mem->mem.fd,
				u_mem->mem.uva,
				u_mem->mem.size,
				u_mem->mem.iova,
				u_mem->mem.iova_size,
				u_mem->mem.kva);
			m_count++;
		}
		mdw_con_info(s, LINEBAR);

		/* device */
		mdw_con_info(s, "|%-10s|%-6s|%-6s|%-20s|%-67s|\n",
			" dev",
			" type",
			" idx",
			" name",
			" devptr");
		mdw_con_info(s, LINEBAR);
		list_for_each_safe(d_list_ptr, d_tmp, &user->dev_list) {
			u_dev = list_entry(d_list_ptr,
				struct apusys_user_dev, list);
			tab = res_get_table(u_dev->dev_info->dev->dev_type);
			if (tab == NULL) {
				mdw_con_info(s, "miss resource table\n");
				break;
			}
			mdw_con_info(s, "| %-9d| %-5d| %-5d| %-19s| %-66p|\n",
				d_count,
				u_dev->dev_info->dev->dev_type,
				u_dev->dev_info->dev->idx,
				tab->name,
				u_dev->dev_info->dev);
			d_count++;
		}
		list_for_each_safe(d_list_ptr, d_tmp, &user->secdev_list) {
			u_dev = list_entry(d_list_ptr,
				struct apusys_user_dev, list);
			tab = res_get_table(u_dev->dev_info->dev->dev_type);
			if (tab == NULL) {
				mdw_con_info(s, "miss resource table\n");
				break;
			}
			mdw_con_info(s, "| %-9d| %-5d| %-5d| %-19s| %-66p|\n",
				d_count,
				u_dev->dev_info->dev->dev_type,
				u_dev->dev_info->dev->idx,
				tab->name,
				u_dev->dev_info->dev);
			d_count++;
		}
		mdw_con_info(s, LINEBAR);
		u_count++;
	}
	mutex_unlock(&g_user_mgr.mtx);

#undef LINEBAR
}

void apusys_user_show_log(void *s_file)
{
	struct seq_file *s = (struct seq_file *)s_file;

	mutex_lock(&g_user_log.mtx);
	seq_printf(s, "%s", g_user_log.log_buf);
	mutex_unlock(&g_user_log.mtx);
}




static int user_mem_cmp(void *priv, struct list_head *a,
					struct list_head *b)
{
	struct apusys_user *ua = NULL;
	struct apusys_user *ub = NULL;

	ua = list_entry(a, struct apusys_user, list);
	ub = list_entry(b, struct apusys_user, list);

	/**
	 * List_sort is a stable sort, so it is not necessary to distinguish
	 * the @a < @b and @a == @b cases.
	 */
	if (ua->open_pid > ub->open_pid)
		return 1;
	else
		return -1;

}

static int user_mem_size_cmp(void *priv, struct list_head *a,
					struct list_head *b)
{
	struct apusys_user *ua = NULL;
	struct apusys_user *ub = NULL;

	ua = list_entry(a, struct apusys_user, list);
	ub = list_entry(b, struct apusys_user, list);

	/**
	 * List_sort is a stable sort, so it is not necessary to distinguish
	 * the @a < @b and @a == @b cases.
	 */
	if (ua->iova_size > ub->iova_size)
		return 1;
	else
		return -1;

}
void apusys_user_print_log(void)
{

	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_user *user = NULL;
	struct apusys_user *u = NULL;
	struct apusys_user u_tmp;
	unsigned int total_size = 0;
	unsigned int percentage = 0;
	char *cur, *end;


	memset(&u_tmp, 0, sizeof(struct apusys_user));

	mutex_lock(&g_user_log.mtx);
	cur = g_user_log.log_buf;
	end = g_user_log.log_buf + DUMP_LOG_SIZE;

	mdw_drv_err("name, id, pid, tgid, iova_size, iova_size_max\n");
	DUMP_LOG(cur, end, "name, id, pid, tgid, iova_size, iova_size_max\n");

	mutex_lock(&g_user_mgr.mtx);
	mutex_lock(&g_user_stat.mtx);

	/* Sort by PID*/
	list_sort(NULL, &g_user_mgr.list, user_mem_cmp);
	mdw_drv_err("----- APUSYS user -----\n");
	DUMP_LOG(cur, end, "----- APUSYS user -----\n");
	list_for_each_safe(list_ptr, tmp, &g_user_mgr.list) {
		user = list_entry(list_ptr, struct apusys_user, list);

		total_size = total_size + user->iova_size;
		mdw_drv_err("%s, %llx, %d, %d, %u, %u\n",
				user->comm, user->id, user->open_pid,
				user->open_tgid, user->iova_size,
				user->iova_size_max);
		DUMP_LOG(cur, end, "%s, %llx, %d, %d, %u, %u\n",
				user->comm, user->id, user->open_pid,
				user->open_tgid, user->iova_size,
				user->iova_size_max);

		if (u_tmp.open_pid != user->open_pid) {

			u = kzalloc(sizeof(struct apusys_user), GFP_KERNEL);
			if (u == NULL)
				goto free_mutex;
			memcpy(u, user, sizeof(struct apusys_user));

			list_add_tail(&u->list, &g_user_stat.list);

			u_tmp.open_pid = user->open_pid;
		} else {
			if (u == NULL)
				goto free_mutex;
			u->iova_size = u->iova_size + user->iova_size;
			u->iova_size_max =
					u->iova_size_max + user->iova_size_max;
		}
	}

	if (!list_empty(&g_user_stat.list)) {
		/* Sort by PID*/
		list_sort(NULL, &g_user_stat.list, user_mem_size_cmp);

		mdw_drv_err("----- APUSYS statistics -----\n");
		DUMP_LOG(cur, end, "----- APUSYS statistics -----\n");

		list_for_each_safe(list_ptr, tmp, &g_user_stat.list) {
			u = list_entry(list_ptr, struct apusys_user, list);

			if (total_size != 0) {
				percentage = (uint64_t) u->iova_size * 100
					/ (uint64_t)total_size;
			} else {
				percentage = 0;
			}

			mdw_drv_err("%s, %llx, %d, %d, %u, %u, %u, %u%%\n",
				u->comm, u->id, u->open_pid,
				u->open_tgid, u->iova_size,
				u->iova_size_max, total_size, percentage);
			DUMP_LOG(cur, end,
					"%s, %llx, %d, %d, %u, %u, %u, %u%%\n",
				u->comm, u->id, u->open_pid,
				u->open_tgid, u->iova_size,
				u->iova_size_max, total_size, percentage);
		}

		/* The last one is the top memory user*/
		mdw_drv_err("----- APUSYS top user -----\n");
		DUMP_LOG(cur, end, "----- APUSYS top user -----\n");

		mdw_drv_err("%s, %llx, %d, %d, %u, %u, %u, %u%%\n",
				u->comm, u->id, u->open_pid,
				u->open_tgid, u->iova_size,
				u->iova_size_max, total_size, percentage);
		DUMP_LOG(cur, end, "%s, %llx, %d, %d, %u, %u, %u, %u%%\n",
				u->comm, u->id, u->open_pid,
				u->open_tgid, u->iova_size,
				u->iova_size_max, total_size, percentage);

		/*delete statistics list*/
		list_for_each_safe(list_ptr, tmp, &g_user_stat.list) {
			u = list_entry(list_ptr, struct apusys_user, list);
			list_del(list_ptr);
			kfree(u);
		}
	}


free_mutex:
	mutex_unlock(&g_user_stat.mtx);
	mutex_unlock(&g_user_mgr.mtx);
	mutex_unlock(&g_user_log.mtx);

}

int apusys_user_insert_cmd(struct apusys_user *u, void *icmd)
{
	struct apusys_cmd *cmd = (struct apusys_cmd *)icmd;

	if (u == NULL || icmd == NULL)
		return -EINVAL;

	/* add to user's list */
	mutex_lock(&u->cmd_mtx);
	list_add_tail(&cmd->u_list, &u->cmd_list);
	mutex_unlock(&u->cmd_mtx);

	return 0;
}

int apusys_user_delete_cmd(struct apusys_user *u, void *icmd)
{
	struct apusys_cmd *cmd = (struct apusys_cmd *) icmd;

	if (u == NULL || icmd == NULL)
		return -EINVAL;

	mutex_lock(&u->cmd_mtx);

	/* delete all sc */
	if (apusys_sched_del_cmd(cmd))
		mdw_drv_err("delete cmd(0x%llx) fail\n", cmd->cmd_id);

	list_del(&cmd->u_list);
	mutex_unlock(&u->cmd_mtx);

	return 0;
}

int apusys_user_get_cmd(struct apusys_user *u, void **icmd, uint64_t cmd_id)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_cmd *cmd = NULL;

	if (u == NULL || icmd == NULL)
		return -EINVAL;

	mutex_lock(&u->cmd_mtx);

	/* query list to find cmd in apusys user */
	list_for_each_safe(list_ptr, tmp, &u->cmd_list) {
		cmd = list_entry(list_ptr, struct apusys_cmd, u_list);
		if (cmd->cmd_id == cmd_id) {
			*icmd = (void *)cmd;
			mutex_unlock(&u->cmd_mtx);
			return 0;
		}
	}

	mutex_unlock(&u->cmd_mtx);
	return -ENODATA;
}

int apusys_user_insert_dev(struct apusys_user *u, void *idev_info)
{
	struct apusys_dev_info *dev_info = (struct apusys_dev_info *)idev_info;
	struct apusys_user_dev *user_dev = NULL;

	/* check argument */
	if (u == NULL || dev_info == NULL)
		return -EINVAL;

	/* alloc user dev */
	user_dev = kzalloc(sizeof(struct apusys_user_dev), GFP_KERNEL);
	if (user_dev == NULL)
		return -ENOMEM;

	/* init */
	INIT_LIST_HEAD(&user_dev->list);
	user_dev->dev_info = dev_info;

	/* add to user's list */
	mutex_lock(&u->dev_mtx);
	list_add_tail(&user_dev->list, &u->dev_list);
	mutex_unlock(&u->dev_mtx);

	mdw_drv_debug("insert dev(%p/%p/%d) to user(0x%llx) done\n",
		user_dev, dev_info->dev,
		dev_info->dev->dev_type,
		u->id);

	return 0;
}

int apusys_user_delete_dev(struct apusys_user *u, void *idev_info)
{
	struct apusys_dev_info *dev_info = (struct apusys_dev_info *)idev_info;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_user_dev *user_dev = NULL;

	/* check argument */
	if (u == NULL || dev_info == NULL)
		return -EINVAL;

	mdw_drv_debug("delete dev(%p/%d) from user(0x%llx)...\n",
		dev_info->dev, dev_info->dev->dev_type, u->id);

	mutex_lock(&u->dev_mtx);

	/* query list to find mem in apusys user */
	list_for_each_safe(list_ptr, tmp, &u->dev_list) {
		user_dev = list_entry(list_ptr, struct apusys_user_dev, list);
		if (user_dev->dev_info == dev_info) {
			list_del(&user_dev->list);
			user_dev->dev_info = NULL;
			kfree(user_dev);
			mutex_unlock(&u->dev_mtx);
			mdw_drv_debug("del dev(%p/%d) u(0x%llx) done\n",
				dev_info->dev, dev_info->dev->dev_type,
				u->id);
			return 0;
		}
	}

	mutex_unlock(&u->dev_mtx);

	mdw_drv_debug("delete dev(%p/%d) from user(0x%llx) fail\n",
		dev_info->dev, dev_info->dev->dev_type, u->id);
	return -ENODEV;
}

struct apusys_dev_info *apusys_user_get_dev(struct apusys_user *u,
	uint64_t hnd)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_user_dev *udev = NULL;

	if (u == NULL || hnd == 0)
		return NULL;

	mutex_lock(&u->dev_mtx);

	/* query list to find cmd in apusys user */
	list_for_each_safe(list_ptr, tmp, &u->dev_list) {
		udev = list_entry(list_ptr, struct apusys_user_dev, list);
		if ((uint64_t)udev->dev_info == hnd) {
			mdw_drv_debug("get device!!\n");
			mutex_unlock(&u->dev_mtx);
			return udev->dev_info;
		}
	}
	mutex_unlock(&u->dev_mtx);

	return NULL;
}

int apusys_user_insert_secdev(struct apusys_user *u, void *idev_info)
{
	struct apusys_dev_info *dev_info = (struct apusys_dev_info *)idev_info;
	struct apusys_user_dev *user_dev = NULL;

	/* check argument */
	if (u == NULL || dev_info == NULL)
		return -EINVAL;

	/* alloc user dev */
	user_dev = kzalloc(sizeof(struct apusys_user_dev), GFP_KERNEL);
	if (user_dev == NULL)
		return -ENOMEM;

	/* init */
	INIT_LIST_HEAD(&user_dev->list);
	user_dev->dev_info = dev_info;

	/* add to user's list */
	mutex_lock(&u->secdev_mtx);
	list_add_tail(&user_dev->list, &u->secdev_list);
	mutex_unlock(&u->secdev_mtx);

	mdw_drv_debug("insert sdev(%p/%p/%d) to user(0x%llx) done\n",
		user_dev, dev_info->dev,
		dev_info->dev->dev_type,
		u->id);

	return 0;
}

int apusys_user_delete_secdev(struct apusys_user *u, void *idev_info)
{
	struct apusys_dev_info *dev_info = (struct apusys_dev_info *)idev_info;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_user_dev *user_dev = NULL;

	/* check argument */
	if (u == NULL || dev_info == NULL)
		return -EINVAL;

	mdw_drv_debug("delete sdev(%p/%d) from user(0x%llx)...\n",
		dev_info->dev, dev_info->dev->dev_type, u->id);

	mutex_lock(&u->secdev_mtx);

	/* query list to find mem in apusys user */
	list_for_each_safe(list_ptr, tmp, &u->secdev_list) {
		user_dev = list_entry(list_ptr, struct apusys_user_dev, list);
		if (user_dev->dev_info == dev_info) {
			list_del(&user_dev->list);
			user_dev->dev_info = NULL;
			kfree(user_dev);
			mutex_unlock(&u->secdev_mtx);
			mdw_drv_debug("del dev(%p/%d) u(0x%llx) done\n",
				dev_info->dev, dev_info->dev->dev_type,
				u->id);
			return 0;
		}
	}

	mutex_unlock(&u->secdev_mtx);

	mdw_drv_debug("delete sdev(%p/%d) from user(0x%llx) fail\n",
		dev_info->dev, dev_info->dev->dev_type, u->id);
	return -ENODEV;
}

int apusys_user_delete_sectype(struct apusys_user *u, int dev_type)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_user_dev *user_dev = NULL;


	/* check argument */
	if (u == NULL)
		return -EINVAL;

	mdw_drv_debug("delete stype(%d) from user(0x%llx)...\n",
		dev_type, u->id);

	mutex_lock(&u->secdev_mtx);

	/* query list to find mem in apusys user */
	list_for_each_safe(list_ptr, tmp, &u->secdev_list) {
		mdw_lne_debug();
		user_dev = list_entry(list_ptr, struct apusys_user_dev, list);
		if (user_dev->dev_info->dev->dev_type == dev_type) {
			list_del(&user_dev->list);
			mdw_drv_debug("del stype(%p/%d) u(0x%llx) done\n",
				user_dev->dev_info->dev,
				user_dev->dev_info->dev->dev_type,
				u->id);
			/* put device back */
			if (put_device_lock(user_dev->dev_info)) {
				mdw_drv_err("put dev(%d/%d) fail\n",
					user_dev->dev_info->dev->dev_type,
					user_dev->dev_info->dev->idx);
			}
			user_dev->dev_info = NULL;
			kfree(user_dev);
		}
	}

	mutex_unlock(&u->secdev_mtx);
	return 0;
}

int apusys_user_insert_mem(struct apusys_user *u, struct apusys_kmem *mem)
{
	struct apusys_user_mem *user_mem = NULL;

	if (mem == NULL || u == NULL)
		return -EINVAL;

	user_mem = kzalloc(sizeof(struct apusys_user_mem), GFP_KERNEL);
	if (user_mem == NULL)
		return -ENOMEM;

	INIT_LIST_HEAD(&user_mem->list);

	memcpy(&user_mem->mem, mem, sizeof(struct apusys_kmem));

	mutex_lock(&u->mem_mtx);
	list_add_tail(&user_mem->list, &u->mem_list);
	mutex_unlock(&u->mem_mtx);

	u->iova_size = u->iova_size + user_mem->mem.iova_size;
	if (u->iova_size_max < u->iova_size)
		u->iova_size_max = u->iova_size;


	mdw_mem_debug("insert mem(%p/%d/%d/0x%llx/0x%x/%d) to u(0x%llx)\n",
		user_mem, user_mem->mem.mem_type,
		user_mem->mem.fd,
		user_mem->mem.kva, user_mem->mem.iova,
		user_mem->mem.size, u->id);
	mdw_mem_debug("user(%s), %llx, %d, %d, %u, %u\n",
			u->comm, u->id, u->open_pid, u->open_tgid,
			u->iova_size, u->iova_size_max);
	return 0;
}

int apusys_user_delete_mem(struct apusys_user *u, struct apusys_kmem *mem)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_user_mem *user_mem = NULL;

	if (mem == NULL || u == NULL)
		return -EINVAL;

	mdw_mem_debug("delete mem(%p/%d/%d/0x%llx/0x%x/%d) from user(0x%llx)\n",
		mem, mem->mem_type, mem->fd,
		mem->kva, mem->iova, mem->size,
		u->id);

	mutex_lock(&u->mem_mtx);

	/* query list to find mem in apusys user */
	list_for_each_safe(list_ptr, tmp, &u->mem_list) {
		user_mem = list_entry(list_ptr, struct apusys_user_mem, list);

		if (user_mem->mem.fd ==
			mem->fd &&
			user_mem->mem.mem_type == mem->mem_type) {
			/* delete memory struct */
			list_del(&user_mem->list);
			u->iova_size = u->iova_size - user_mem->mem.iova_size;
			kfree(user_mem);
			mdw_mem_debug("user(%s), %llx, %d, %d, %u, %u\n",
					u->comm, u->id, u->open_pid,
					u->open_tgid, u->iova_size,
					u->iova_size_max);
			mutex_unlock(&u->mem_mtx);

			//mdw_drv_debug("-\n");
			return 0;
		}
	}

	mutex_unlock(&u->mem_mtx);

	return -ENOMEM;
}

struct apusys_kmem *apusys_user_get_mem(struct apusys_user *u,
	int fd)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_user_mem *u_mem = NULL;

	if (u == NULL || fd <= 0)
		return NULL;

	mdw_mem_debug("fd = %d, u(0x%llx)\n", fd, u->id);

	mutex_lock(&u->mem_mtx);

	/* query list to find cmd in apusys user */
	list_for_each_safe(list_ptr, tmp, &u->mem_list) {
		u_mem = list_entry(list_ptr, struct apusys_user_mem, list);
		if (u_mem->mem.fd == fd) {
			mdw_mem_debug("get kmem from ulist\n");
			mutex_unlock(&u->mem_mtx);
			return &u_mem->mem;
		}
	}
	mutex_unlock(&u->mem_mtx);

	return NULL;
}

int apusys_create_user(struct apusys_user **iu)
{
	struct apusys_user *u = NULL;

	if (IS_ERR_OR_NULL(iu))
		return -EINVAL;

	u = kzalloc(sizeof(struct apusys_user), GFP_KERNEL);
	if (u == NULL)
		return -ENOMEM;

	u->open_pid = current->pid;
	u->open_tgid = current->tgid;
	get_task_comm(u->comm, current);
	u->id = (uint64_t)u;
	mutex_init(&u->cmd_mtx);
	INIT_LIST_HEAD(&u->cmd_list);
	mutex_init(&u->mem_mtx);
	INIT_LIST_HEAD(&u->mem_list);
	mutex_init(&u->dev_mtx);
	INIT_LIST_HEAD(&u->dev_list);
	mutex_init(&u->secdev_mtx);
	INIT_LIST_HEAD(&u->secdev_list);

	mdw_drv_debug("apusys user(0x%llx/%d/%d)\n",
		u->id,
		(int)u->open_pid,
		(int)u->open_tgid);

	*iu = u;

	/* add to user mgr's list */
	mutex_lock(&g_user_mgr.mtx);
	list_add_tail(&u->list, &g_user_mgr.list);
	mutex_unlock(&g_user_mgr.mtx);

	return 0;
}

int apusys_delete_user(struct apusys_user *u)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_user_mem *user_mem = NULL;
	struct apusys_user_dev *user_dev = NULL;
	struct apusys_dev_info *dev_info = NULL;
	struct apusys_cmd *cmd = NULL;
	unsigned long long dev_bit = 0;

	mdw_drv_debug("+\n");

	if (IS_ERR_OR_NULL(u))
		return -EINVAL;

	/* delete residual cmd */
	mutex_lock(&u->cmd_mtx);
	mdw_lne_debug();
	list_for_each_safe(list_ptr, tmp, &u->cmd_list) {
		cmd = list_entry(list_ptr, struct apusys_cmd, u_list);

		mdw_drv_warn("del pending cmd(0x%llx)\n", cmd->cmd_id);
		if (apusys_cmd_delete(cmd))
			mdw_drv_err("delete apusys cmd(%p) fail\n", cmd);
	}
	mutex_unlock(&u->cmd_mtx);

	/* delete residual allocated memory */
	mutex_lock(&u->mem_mtx);
	/* query list to find mem in apusys user */
	list_for_each_safe(list_ptr, tmp, &u->mem_list) {
		user_mem = list_entry(list_ptr, struct apusys_user_mem, list);
		/* delete memory */
		mdw_drv_warn("undeleted mem(%p/%d/%d/0x%llx/0x%x/%d)\n",
		user_mem, user_mem->mem.mem_type,
		user_mem->mem.fd,
		user_mem->mem.kva, user_mem->mem.iova,
		user_mem->mem.size);

		list_del(&user_mem->list);

		mdw_lne_debug();
		if (apusys_mem_release(&user_mem->mem)) {
			mdw_drv_err("free fail(%d/0x%llx/0x%x)\n",
			user_mem->mem.fd,
			user_mem->mem.kva,
			user_mem->mem.iova);
		}

		kfree(user_mem);
	}
	mutex_unlock(&u->mem_mtx);

	/* delete residual allocated dev */
	mutex_lock(&u->dev_mtx);
	list_for_each_safe(list_ptr, tmp, &u->dev_list) {
		user_dev = list_entry(list_ptr, struct apusys_user_dev, list);
		if (user_dev->dev_info != NULL) {
			if (put_device_lock(user_dev->dev_info)) {
				mdw_drv_err("put device(%p) user(0x%llx) fail\n",
					user_dev->dev_info->dev,
					u->id);
			}
		}
		list_del(&user_dev->list);
		kfree(user_dev);
	}
	mutex_unlock(&u->dev_mtx);

	/* delete residual secure dev */
	mutex_lock(&u->secdev_mtx);
	list_for_each_safe(list_ptr, tmp, &u->secdev_list) {
		user_dev = list_entry(list_ptr, struct apusys_user_dev, list);
		dev_info = user_dev->dev_info;
		if (dev_info != NULL) {

			/* power off and release secure mode before put dev */
			if (!(dev_bit & (1ULL << dev_info->dev->dev_type)) ||
				dev_info->dev->dev_type > APUSYS_DEVICE_RT) {
				if (res_secure_off(
					dev_info->dev->dev_type)) {
					mdw_drv_err("dev(%d) secmode off fail\n",
						dev_info->dev->dev_type);
				}
				dev_bit |= (1ULL << dev_info->dev->dev_type);
			}
			if (res_power_off(dev_info->dev->dev_type,
				dev_info->dev->idx)) {
				mdw_drv_err("sec pwroff dev(%d/%d) fail\n",
					dev_info->dev->dev_type,
					dev_info->dev->idx);
			}

			/* put device back to table */
			if (put_device_lock(dev_info)) {
				mdw_drv_err("put device(%p) user(0x%llx) fail\n",
					dev_info->dev,
					u->id);
			}
		}
		list_del(&user_dev->list);
		kfree(user_dev);
	}
	mutex_unlock(&u->secdev_mtx);


	mutex_lock(&g_user_mgr.mtx);
	list_del(&u->list);
	mutex_unlock(&g_user_mgr.mtx);

	kfree(u);

	mdw_drv_debug("-\n");

	return 0;
}

int apusys_user_init(void)
{
	memset(&g_user_mgr, 0, sizeof(struct apusys_user_mgr));
	memset(&g_user_log, 0, sizeof(struct apusys_user_log));
	memset(&g_user_stat, 0, sizeof(struct apusys_user_statistics));

	mutex_init(&g_user_mgr.mtx);
	mutex_init(&g_user_log.mtx);
	mutex_init(&g_user_stat.mtx);
	INIT_LIST_HEAD(&g_user_mgr.list);
	INIT_LIST_HEAD(&g_user_mgr.list);
	INIT_LIST_HEAD(&g_user_stat.list);
	return 0;
}

void apusys_user_destroy(void)
{
	mdw_lne_debug();
}
