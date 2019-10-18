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

#include "apusys_cmn.h"
#include "apusys_user.h"
#include "apusys_drv.h"
#include "cmd_parser.h"
#include "memory_mgt.h"
#include "resource_mgt.h"

struct apusys_user_mem {
	struct apusys_mem mem;
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

static struct apusys_user_mgr g_user_mgr;

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
	struct seq_file *s = (struct seq_file *)s_file;
	int u_count = 0;
	int d_count = 0;
	int m_count = 0;
	int c_count = 0;

#define LINEBAR \
	"|-------------------------------------------"\
	"-------------------------------------------|\n"

	LOG_CON(s, LINEBAR);
	LOG_CON(s, "|%-86s|\n",
		" apusys user table");
	LOG_CON(s, LINEBAR);

	mutex_lock(&g_user_mgr.mtx);
	list_for_each_safe(list_ptr, tmp, &g_user_mgr.list) {
		user = list_entry(list_ptr, struct apusys_user, list);

		LOG_CON(s, "| user (#%-3d)%74s|\n",
			u_count,
			"");
		LOG_CON(s, "| id   = 0x%-76llx|\n",
			user->id);
		LOG_CON(s, "| pid  = %-78d|\n",
			user->open_pid);
		LOG_CON(s, "| tgid = %-78d|\n",
			user->open_tgid);
		LOG_CON(s, LINEBAR);

		c_count = 0;
		d_count = 0;
		m_count = 0;

		/* cmd */
		LOG_CON(s, "|%-10s|%-20s|%-20s|%-20s|%-12s|\n",
			" cmd",
			" priority",
			" uid",
			" id",
			" sc num");
		LOG_CON(s, LINEBAR);
		list_for_each_safe(c_list_ptr, c_tmp, &user->cmd_list) {
			cmd = list_entry(c_list_ptr,
				struct apusys_cmd, u_list);
			mutex_lock(&cmd->mtx);

			LOG_CON(s,
			"| #%-8d| %-19d| 0x%-17llx| 0x%-17llx| %-11u|\n",
				c_count,
				cmd->hdr->priority,
				cmd->hdr->uid,
				cmd->cmd_id,
				cmd->hdr->num_sc);
			mutex_unlock(&cmd->mtx);
			c_count++;
		}
		LOG_CON(s, LINEBAR);

		/* mem */
		LOG_CON(s,
		"|%-10s|%-6s|%-6s|%-6s|%-20s|%-20s|%-12s|\n",
			" mem",
			" type",
			" fd",
			" size",
			" uva",
			" kva",
			" iova");
		LOG_CON(s, LINEBAR);
		list_for_each_safe(m_list_ptr, m_tmp, &user->mem_list) {
			u_mem = list_entry(m_list_ptr,
				struct apusys_user_mem, list);

			LOG_CON(s,
			"| #%-8d| %-5u| %-5u| %-5d| 0x%-17llx| 0x%-17llx| 0x%-9x|\n",
				m_count,
				u_mem->mem.mem_type,
				u_mem->mem.ion_data.ion_share_fd,
				u_mem->mem.size,
				u_mem->mem.uva,
				u_mem->mem.kva,
				u_mem->mem.iova);
			m_count++;
		}
		LOG_CON(s, LINEBAR);

		/* device */
		LOG_CON(s, "|%-10s|%-6s|%-6s|%-61s|\n",
			" dev",
			" type",
			" idx",
			" devptr");
		LOG_CON(s, LINEBAR);
		list_for_each_safe(d_list_ptr, d_tmp, &user->dev_list) {
			u_dev = list_entry(d_list_ptr,
				struct apusys_user_dev, list);
			LOG_CON(s, "| %-9d| %-5d| %-5d| %-60p|\n",
				d_count,
				u_dev->dev_info->dev->dev_type,
				u_dev->dev_info->dev->idx,
				u_dev->dev_info->dev);
			d_count++;
		}
		LOG_CON(s, LINEBAR);
		u_count++;
	}
	mutex_unlock(&g_user_mgr.mtx);

#undef LINEBAR
}

int apusys_user_insert_cmd(struct apusys_user *user, void *icmd)
{
	struct apusys_cmd *cmd = (struct apusys_cmd *)icmd;

	if (user == NULL || icmd == NULL)
		return -EINVAL;

	/* add to user's list */
	mutex_lock(&user->cmd_mtx);
	list_add_tail(&cmd->u_list, &user->cmd_list);
	mutex_unlock(&user->cmd_mtx);

	return 0;
}

int apusys_user_delete_cmd(struct apusys_user *user, void *icmd)
{
	struct apusys_cmd *cmd = (struct apusys_cmd *) icmd;
	struct apusys_subcmd *sc = NULL;
	int i = 0;

	if (user == NULL || icmd == NULL)
		return -EINVAL;

	mutex_lock(&user->cmd_mtx);

	/* delete all sc */
	mutex_lock(&cmd->sc_mtx);
	for (i = 0; i < cmd->hdr->num_sc; i++) {
		if (cmd->sc_list[i] != NULL) {
			if (apusys_subcmd_delete(cmd->sc_list[i]))
				LOG_ERR("delete subcmd fail(%p)\n", sc);
			cmd->sc_list[i] = NULL;
		}
	}

	list_del(&cmd->u_list);
	mutex_unlock(&cmd->sc_mtx);
	mutex_unlock(&user->cmd_mtx);

	return 0;
}

int apusys_user_get_cmd(struct apusys_user *user, void **icmd, uint64_t cmd_id)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_cmd *cmd = NULL;

	if (user == NULL || icmd == NULL)
		return -EINVAL;

	mutex_lock(&user->cmd_mtx);

	/* query list to find cmd in apusys user */
	list_for_each_safe(list_ptr, tmp, &user->cmd_list) {
		cmd = list_entry(list_ptr, struct apusys_cmd, u_list);
		if (cmd->cmd_id == cmd_id) {
			*icmd = (void *)cmd;
			break;
		}
	}
	mutex_unlock(&user->cmd_mtx);

	return 0;
}

int apusys_user_insert_dev(struct apusys_user *user, void *idev_info)
{
	struct apusys_dev_info *dev_info = (struct apusys_dev_info *)idev_info;
	struct apusys_user_dev *user_dev = NULL;

	/* check argument */
	if (user == NULL || dev_info == NULL)
		return -EINVAL;

	/* alloc user dev */
	user_dev = kzalloc(sizeof(struct apusys_user_dev), GFP_KERNEL);
	if (user_dev == NULL)
		return -ENOMEM;

	/* init */
	INIT_LIST_HEAD(&user_dev->list);
	user_dev->dev_info = dev_info;

	/* add to user's list */
	mutex_lock(&user->dev_mtx);
	list_add_tail(&user_dev->list, &user->dev_list);
	mutex_unlock(&user->dev_mtx);

	LOG_DEBUG("insert dev(%p/%p/%d) to user(%p/0x%llx) done\n",
		user_dev, dev_info->dev,
		dev_info->dev->dev_type, user,
		user->id);

	return 0;
}

int apusys_user_delete_dev(struct apusys_user *user, void *idev_info)
{
	struct apusys_dev_info *dev_info = (struct apusys_dev_info *)idev_info;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_user_dev *user_dev = NULL;

	/* check argument */
	if (user == NULL || dev_info == NULL)
		return -EINVAL;

	LOG_DEBUG("delete dev(%p/%d) from user(%p/0x%llx)...\n",
		dev_info->dev, dev_info->dev->dev_type, user, user->id);

	mutex_lock(&user->dev_mtx);

	/* query list to find mem in apusys user */
	list_for_each_safe(list_ptr, tmp, &user->dev_list) {
		user_dev = list_entry(list_ptr, struct apusys_user_dev, list);
		if (user_dev->dev_info == dev_info) {
			list_del(&user_dev->list);
			user_dev->dev_info = NULL;
			kfree(user_dev);
			mutex_unlock(&user->dev_mtx);
			LOG_DEBUG("del dev(%p/%d) u(%p/0x%llx) done\n",
				dev_info->dev, dev_info->dev->dev_type,
				user, user->id);
			return 0;
		}
	}

	mutex_unlock(&user->dev_mtx);

	LOG_DEBUG("delete dev(%p/%d) from user(%p/0x%llx) fail\n",
		dev_info->dev, dev_info->dev->dev_type, user, user->id);
	return -ENODEV;
}

struct apusys_dev_info *apusys_user_get_dev(struct apusys_user *user,
	uint64_t hnd)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_user_dev *udev = NULL;

	if (user == NULL || hnd == 0)
		return NULL;

	mutex_lock(&user->dev_mtx);

	/* query list to find cmd in apusys user */
	list_for_each_safe(list_ptr, tmp, &user->dev_list) {
		udev = list_entry(list_ptr, struct apusys_user_dev, list);
		if ((uint64_t)udev->dev_info == hnd) {
			LOG_DEBUG("get device!!\n");
			break;
		}
	}
	mutex_unlock(&user->dev_mtx);

	return udev->dev_info;
}


int apusys_user_insert_mem(struct apusys_user *user, struct apusys_mem *mem)
{
	struct apusys_user_mem *user_mem = NULL;

	if (mem == NULL || user == NULL)
		return -EINVAL;

	user_mem = kzalloc(sizeof(struct apusys_user_mem), GFP_KERNEL);
	if (user_mem == NULL)
		return -ENOMEM;

	INIT_LIST_HEAD(&user_mem->list);

	memcpy(&user_mem->mem, mem, sizeof(struct apusys_mem));

	mutex_lock(&user->mem_mtx);
	list_add_tail(&user_mem->list, &user->mem_list);
	mutex_unlock(&user->mem_mtx);

	LOG_DEBUG("insert mem(%p/%d/%d/0x%llx/0x%x/%d)\n",
		user_mem, user_mem->mem.mem_type,
		user_mem->mem.ion_data.ion_share_fd,
		user_mem->mem.kva, user_mem->mem.iova,
		user_mem->mem.size);

	return 0;
}

int apusys_user_delete_mem(struct apusys_user *user, struct apusys_mem *mem)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_user_mem *user_mem = NULL;

	if (mem == NULL || user == NULL)
		return -EINVAL;

	LOG_DEBUG("delete mem(%p/%d/%d/0x%llx/0x%x/%d) from user(%p/0x%llx)\n",
		mem, mem->mem_type, mem->ion_data.ion_share_fd,
		mem->kva, mem->iova, mem->size,
		user, user->id);

	mutex_lock(&user->mem_mtx);

	/* query list to find mem in apusys user */
	list_for_each_safe(list_ptr, tmp, &user->mem_list) {
		user_mem = list_entry(list_ptr, struct apusys_user_mem, list);

		if (user_mem->mem.ion_data.ion_share_fd ==
			mem->ion_data.ion_share_fd &&
			user_mem->mem.mem_type == mem->mem_type) {
			/* delete memory struct */
			list_del(&user_mem->list);
			kfree(user_mem);
			mutex_unlock(&user->mem_mtx);
			//LOG_DEBUG("-\n");
			return 0;
		}
	}

	mutex_unlock(&user->mem_mtx);

	return -ENOMEM;
}

int apusys_create_user(struct apusys_user **user)
{
	struct apusys_user *u = NULL;

	LOG_DEBUG("+\n");

	if (IS_ERR_OR_NULL(user))
		return -EINVAL;

	u = kzalloc(sizeof(struct apusys_user), GFP_KERNEL);
	if (u == NULL)
		return -ENOMEM;

	//LOG_DEBUG("create apusys user %p\n", u);

	u->open_pid = current->pid;
	u->open_tgid = current->tgid;
	u->id = (uint64_t)u;
	mutex_init(&u->cmd_mtx);
	INIT_LIST_HEAD(&u->cmd_list);
	mutex_init(&u->mem_mtx);
	INIT_LIST_HEAD(&u->mem_list);
	mutex_init(&u->dev_mtx);
	INIT_LIST_HEAD(&u->dev_list);

	LOG_DEBUG("apusys user(0x%llx/%d/%d)\n",
		u->id,
		(int)u->open_pid,
		(int)u->open_tgid);

	*user = u;

	/* add to user mgr's list */
	mutex_lock(&g_user_mgr.mtx);
	list_add_tail(&u->list, &g_user_mgr.list);
	mutex_unlock(&g_user_mgr.mtx);

	LOG_DEBUG("-\n");

	return 0;
}

int apusys_delete_user(struct apusys_user *user)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_user_mem *user_mem = NULL;
	struct apusys_user_dev *user_dev = NULL;
	struct apusys_cmd *cmd = NULL;

	LOG_DEBUG("+\n");

	if (IS_ERR_OR_NULL(user))
		return -EINVAL;

	/* delete all cmd from user */
	mutex_lock(&user->cmd_mtx);
	DEBUG_TAG;
	list_for_each_safe(list_ptr, tmp, &user->cmd_list) {
		cmd = list_entry(list_ptr, struct apusys_cmd, u_list);

		if (apusys_cmd_delete(cmd))
			LOG_ERR("delete apusys cmd(%p) fail\n", cmd);

	}
	mutex_unlock(&user->cmd_mtx);

	/* clean memory from user */
	mutex_lock(&user->mem_mtx);
	/* query list to find mem in apusys user */
	list_for_each_safe(list_ptr, tmp, &user->mem_list) {
		user_mem = list_entry(list_ptr, struct apusys_user_mem, list);
		/* delete memory */
		LOG_WARN("undeleted mem(%p/%d/%d/0x%llx/0x%x/%d)\n",
		user_mem, user_mem->mem.mem_type,
		user_mem->mem.ion_data.ion_share_fd,
		user_mem->mem.kva, user_mem->mem.iova,
		user_mem->mem.size);

		list_del(&user_mem->list);

		/* unmap buf */
		if (user_mem->mem.ctl_data.cmd == APUSYS_MAP) {
			/* unmap buf */
			switch (user_mem->mem.ctl_data.map_param.map_type) {
			case APUSYS_MAP_NONE:
				DEBUG_TAG;
				if (apusys_mem_free(&user_mem->mem)) {
					LOG_ERR("free fail(%d/0x%llx/0x%x)\n",
					user_mem->mem.ion_data.ion_share_fd,
					user_mem->mem.kva,
					user_mem->mem.iova);
				}
				break;
			case APUSYS_MAP_KVA:
				if (apusys_mem_unmap_kva(&user_mem->mem)) {
					LOG_ERR("unkva fail(%d/0x%llx/0x%x)\n",
					user_mem->mem.ion_data.ion_share_fd,
					user_mem->mem.kva,
					user_mem->mem.iova);
				}
				break;
			case APUSYS_MAP_IOVA:
				if (apusys_mem_unmap_iova(&user_mem->mem)) {
					LOG_ERR("uniova fail(%d/0x%llx/0x%x)\n",
					user_mem->mem.ion_data.ion_share_fd,
					user_mem->mem.kva,
					user_mem->mem.iova);
				}
				break;
			case APUSYS_MAP_PA:
				LOG_ERR("not support unmap pa\n");
				break;
			default:
				break;
			}
		}

		kfree(user_mem);
	}

	mutex_unlock(&user->mem_mtx);

	/* query list to find dev in apusys user */
	mutex_lock(&user->dev_mtx);

	list_for_each_safe(list_ptr, tmp, &user->dev_list) {
		user_dev = list_entry(list_ptr, struct apusys_user_dev, list);
		if (user_dev->dev_info != NULL) {
			if (put_device_lock(user_dev->dev_info)) {
				LOG_ERR("put device(%p) user(0x%llx) fail\n",
					user_dev->dev_info->dev,
					user->id);
			}
		}
		list_del(&user_dev->list);
		kfree(user_dev);
	}

	mutex_unlock(&user->dev_mtx);

	mutex_lock(&g_user_mgr.mtx);
	list_del(&user->list);
	mutex_unlock(&g_user_mgr.mtx);

	kfree(user);

	LOG_DEBUG("-\n");

	return 0;
}

int apusys_user_init(void)
{
	memset(&g_user_mgr, 0, sizeof(struct apusys_user_mgr));

	mutex_init(&g_user_mgr.mtx);
	INIT_LIST_HEAD(&g_user_mgr.list);

	return 0;
}

void apusys_user_destroy(void)
{
	DEBUG_TAG;
}
