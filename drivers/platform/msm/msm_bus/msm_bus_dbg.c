/* Copyright (c) 2010-2012, 2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "AXI: %s(): " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/msm-bus-board.h>
#include <linux/msm-bus.h>
#include "msm_bus_core.h"

#define CREATE_TRACE_POINTS
#include <trace/events/trace_msm_bus.h>

#define MAX_BUFF_SIZE 4096
#define FILL_LIMIT 128

static struct dentry *clients;
static struct dentry *dir;
static DEFINE_MUTEX(msm_bus_dbg_fablist_lock);
struct msm_bus_dbg_state {
	uint32_t cl;
	uint8_t enable;
	uint8_t current_index;
} clstate;

struct msm_bus_cldata {
	const struct msm_bus_scale_pdata *pdata;
	int index;
	uint32_t clid;
	int size;
	struct dentry *file;
	struct list_head list;
	char buffer[MAX_BUFF_SIZE];
};

struct msm_bus_fab_list {
	const char *name;
	int size;
	struct dentry *file;
	struct list_head list;
	char buffer[MAX_BUFF_SIZE];
};

LIST_HEAD(fabdata_list);
LIST_HEAD(cl_list);

/**
 * The following structures and funtions are used for
 * the test-client which can be created at run-time.
 */

static struct msm_bus_vectors init_vectors[1];
static struct msm_bus_vectors current_vectors[1];
static struct msm_bus_vectors requested_vectors[1];

static struct msm_bus_paths shell_client_usecases[] = {
	{
		.num_paths = ARRAY_SIZE(init_vectors),
		.vectors = init_vectors,
	},
	{
		.num_paths = ARRAY_SIZE(current_vectors),
		.vectors = current_vectors,
	},
	{
		.num_paths = ARRAY_SIZE(requested_vectors),
		.vectors = requested_vectors,
	},
};

static struct msm_bus_scale_pdata shell_client = {
	.usecase = shell_client_usecases,
	.num_usecases = ARRAY_SIZE(shell_client_usecases),
	.name = "test-client",
};

static void msm_bus_dbg_init_vectors(void)
{
	init_vectors[0].src = -1;
	init_vectors[0].dst = -1;
	init_vectors[0].ab = 0;
	init_vectors[0].ib = 0;
	current_vectors[0].src = -1;
	current_vectors[0].dst = -1;
	current_vectors[0].ab = 0;
	current_vectors[0].ib = 0;
	requested_vectors[0].src = -1;
	requested_vectors[0].dst = -1;
	requested_vectors[0].ab = 0;
	requested_vectors[0].ib = 0;
	clstate.enable = 0;
	clstate.current_index = 0;
}

static int msm_bus_dbg_update_cl_request(uint32_t cl)
{
	int ret = 0;

	if (clstate.current_index < 2)
		clstate.current_index = 2;
	else {
		clstate.current_index = 1;
		current_vectors[0].ab = requested_vectors[0].ab;
		current_vectors[0].ib = requested_vectors[0].ib;
	}

	if (clstate.enable) {
		MSM_BUS_DBG("Updating request for shell client, index: %d\n",
			clstate.current_index);
		ret = msm_bus_scale_client_update_request(clstate.cl,
			clstate.current_index);
	} else
		MSM_BUS_DBG("Enable bit not set. Skipping update request\n");

	return ret;
}

static void msm_bus_dbg_unregister_client(uint32_t cl)
{
	MSM_BUS_DBG("Unregistering shell client\n");
	msm_bus_scale_unregister_client(clstate.cl);
	clstate.cl = 0;
}

static uint32_t msm_bus_dbg_register_client(void)
{
	int ret = 0;

	if (init_vectors[0].src != requested_vectors[0].src) {
		MSM_BUS_DBG("Shell client master changed. Unregistering\n");
		msm_bus_dbg_unregister_client(clstate.cl);
	}
	if (init_vectors[0].dst != requested_vectors[0].dst) {
		MSM_BUS_DBG("Shell client slave changed. Unregistering\n");
		msm_bus_dbg_unregister_client(clstate.cl);
	}

	if (!clstate.enable) {
		MSM_BUS_DBG("Enable bit not set, skipping registration: cl "
			"%d\n",	clstate.cl);
		return 0;
	}

	if (clstate.cl) {
		MSM_BUS_DBG("Client  registered, skipping registration\n");
		return 0;
	}

	current_vectors[0].src = init_vectors[0].src;
	requested_vectors[0].src = init_vectors[0].src;
	current_vectors[0].dst = init_vectors[0].dst;
	requested_vectors[0].dst = init_vectors[0].dst;
	MSM_BUS_DBG("Registering shell client\n");
	ret = msm_bus_scale_register_client(&shell_client);
	return ret;
}

static int msm_bus_dbg_mas_get(void  *data, u64 *val)
{
	*val = init_vectors[0].src;
	MSM_BUS_DBG("Get master: %llu\n", *val);
	return 0;
}

static int msm_bus_dbg_mas_set(void  *data, u64 val)
{
	init_vectors[0].src = val;
	MSM_BUS_DBG("Set master: %llu\n", val);
	clstate.cl = msm_bus_dbg_register_client();
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(shell_client_mas_fops, msm_bus_dbg_mas_get,
	msm_bus_dbg_mas_set, "%llu\n");

static int msm_bus_dbg_slv_get(void  *data, u64 *val)
{
	*val = init_vectors[0].dst;
	MSM_BUS_DBG("Get slave: %llu\n", *val);
	return 0;
}

static int msm_bus_dbg_slv_set(void  *data, u64 val)
{
	init_vectors[0].dst = val;
	MSM_BUS_DBG("Set slave: %llu\n", val);
	clstate.cl = msm_bus_dbg_register_client();
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(shell_client_slv_fops, msm_bus_dbg_slv_get,
	msm_bus_dbg_slv_set, "%llu\n");

static int msm_bus_dbg_ab_get(void  *data, u64 *val)
{
	*val = requested_vectors[0].ab;
	MSM_BUS_DBG("Get ab: %llu\n", *val);
	return 0;
}

static int msm_bus_dbg_ab_set(void  *data, u64 val)
{
	requested_vectors[0].ab = val;
	MSM_BUS_DBG("Set ab: %llu\n", val);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(shell_client_ab_fops, msm_bus_dbg_ab_get,
	msm_bus_dbg_ab_set, "%llu\n");

static int msm_bus_dbg_ib_get(void  *data, u64 *val)
{
	*val = requested_vectors[0].ib;
	MSM_BUS_DBG("Get ib: %llu\n", *val);
	return 0;
}

static int msm_bus_dbg_ib_set(void  *data, u64 val)
{
	requested_vectors[0].ib = val;
	MSM_BUS_DBG("Set ib: %llu\n", val);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(shell_client_ib_fops, msm_bus_dbg_ib_get,
	msm_bus_dbg_ib_set, "%llu\n");

static int msm_bus_dbg_en_get(void  *data, u64 *val)
{
	*val = clstate.enable;
	MSM_BUS_DBG("Get enable: %llu\n", *val);
	return 0;
}

static int msm_bus_dbg_en_set(void  *data, u64 val)
{
	int ret = 0;

	clstate.enable = val;
	if (clstate.enable) {
		if (!clstate.cl) {
			MSM_BUS_DBG("client: %u\n", clstate.cl);
			clstate.cl = msm_bus_dbg_register_client();
			if (clstate.cl)
				ret = msm_bus_dbg_update_cl_request(clstate.cl);
		} else {
			MSM_BUS_DBG("update request for cl: %u\n", clstate.cl);
			ret = msm_bus_dbg_update_cl_request(clstate.cl);
		}
	}

	MSM_BUS_DBG("Set enable: %llu\n", val);
	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(shell_client_en_fops, msm_bus_dbg_en_get,
	msm_bus_dbg_en_set, "%llu\n");

/**
 * The following funtions are used for viewing the client data
 * and changing the client request at run-time
 */

static ssize_t client_data_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	int bsize = 0;
	uint32_t cl = (uint32_t)(uintptr_t)file->private_data;
	struct msm_bus_cldata *cldata = NULL;
	int found = 0;

	list_for_each_entry(cldata, &cl_list, list) {
		if (cldata->clid == cl) {
			found = 1;
			break;
		}
	}
	if (!found)
		return 0;

	bsize = cldata->size;
	return simple_read_from_buffer(buf, count, ppos,
		cldata->buffer, bsize);
}

static int client_data_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static const struct file_operations client_data_fops = {
	.open		= client_data_open,
	.read		= client_data_read,
};

struct dentry *msm_bus_dbg_create(const char *name, mode_t mode,
	struct dentry *dent, uint32_t clid)
{
	if (dent == NULL) {
		MSM_BUS_DBG("debugfs not ready yet\n");
		return NULL;
	}
	return debugfs_create_file(name, mode, dent, (void *)(uintptr_t)clid,
		&client_data_fops);
}

static int msm_bus_dbg_record_client(const struct msm_bus_scale_pdata *pdata,
	int index, uint32_t clid, struct dentry *file)
{
	struct msm_bus_cldata *cldata;

	cldata = kmalloc(sizeof(struct msm_bus_cldata), GFP_KERNEL);
	if (!cldata) {
		MSM_BUS_DBG("Failed to allocate memory for client data\n");
		return -ENOMEM;
	}
	cldata->pdata = pdata;
	cldata->index = index;
	cldata->clid = clid;
	cldata->file = file;
	cldata->size = 0;
	list_add_tail(&cldata->list, &cl_list);
	return 0;
}

static void msm_bus_dbg_free_client(uint32_t clid)
{
	struct msm_bus_cldata *cldata = NULL;

	list_for_each_entry(cldata, &cl_list, list) {
		if (cldata->clid == clid) {
			debugfs_remove(cldata->file);
			list_del(&cldata->list);
			kfree(cldata);
			break;
		}
	}
}

static int msm_bus_dbg_fill_cl_buffer(const struct msm_bus_scale_pdata *pdata,
	int index, uint32_t clid)
{
	int i = 0, j;
	char *buf = NULL;
	struct msm_bus_cldata *cldata = NULL;
	struct timespec ts;
	int found = 0;

	list_for_each_entry(cldata, &cl_list, list) {
		if (cldata->clid == clid) {
			found = 1;
			break;
		}
	}

	if (!found)
		return -ENOENT;

	if (cldata->file == NULL) {
		if (pdata->name == NULL) {
			MSM_BUS_DBG("Client doesn't have a name\n");
			return -EINVAL;
		}
		cldata->file = msm_bus_dbg_create(pdata->name, S_IRUGO,
			clients, clid);
	}

	if (cldata->size < (MAX_BUFF_SIZE - FILL_LIMIT))
		i = cldata->size;
	else {
		i = 0;
		cldata->size = 0;
	}
	buf = cldata->buffer;
	ts = ktime_to_timespec(ktime_get());
	i += scnprintf(buf + i, MAX_BUFF_SIZE - i, "\n%d.%d\n",
		(int)ts.tv_sec, (int)ts.tv_nsec);
	i += scnprintf(buf + i, MAX_BUFF_SIZE - i, "curr   : %d\n", index);
	i += scnprintf(buf + i, MAX_BUFF_SIZE - i, "masters: ");

	for (j = 0; j < pdata->usecase->num_paths; j++)
		i += scnprintf(buf + i, MAX_BUFF_SIZE - i, "%d  ",
			pdata->usecase[index].vectors[j].src);
	i += scnprintf(buf + i, MAX_BUFF_SIZE - i, "\nslaves : ");
	for (j = 0; j < pdata->usecase->num_paths; j++)
		i += scnprintf(buf + i, MAX_BUFF_SIZE - i, "%d  ",
			pdata->usecase[index].vectors[j].dst);
	i += scnprintf(buf + i, MAX_BUFF_SIZE - i, "\nab     : ");
	for (j = 0; j < pdata->usecase->num_paths; j++)
		i += scnprintf(buf + i, MAX_BUFF_SIZE - i, "%llu  ",
			pdata->usecase[index].vectors[j].ab);
	i += scnprintf(buf + i, MAX_BUFF_SIZE - i, "\nib     : ");
	for (j = 0; j < pdata->usecase->num_paths; j++)
		i += scnprintf(buf + i, MAX_BUFF_SIZE - i, "%llu  ",
			pdata->usecase[index].vectors[j].ib);
	i += scnprintf(buf + i, MAX_BUFF_SIZE - i, "\n");

	for (j = 0; j < pdata->usecase->num_paths; j++)
		trace_bus_update_request((int)ts.tv_sec, (int)ts.tv_nsec,
		pdata->name, index,
		pdata->usecase[index].vectors[j].src,
		pdata->usecase[index].vectors[j].dst,
		pdata->usecase[index].vectors[j].ab,
		pdata->usecase[index].vectors[j].ib);

	cldata->size = i;
	return i;
}

static int msm_bus_dbg_update_request(struct msm_bus_cldata *cldata, int index)
{
	int ret = 0;

	if ((index < 0) || (index > cldata->pdata->num_usecases)) {
		MSM_BUS_DBG("Invalid index!\n");
		return -EINVAL;
	}
	ret = msm_bus_scale_client_update_request(cldata->clid, index);
	return ret;
}

static ssize_t  msm_bus_dbg_update_request_write(struct file *file,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	struct msm_bus_cldata *cldata;
	unsigned long index = 0;
	int ret = 0;
	char *chid;
	char *buf = kmalloc((sizeof(char) * (cnt + 1)), GFP_KERNEL);
	int found = 0;

	if (!buf || IS_ERR(buf)) {
		MSM_BUS_ERR("Memory allocation for buffer failed\n");
		return -ENOMEM;
	}
	if (cnt == 0)
		return 0;
	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = '\0';
	chid = buf;
	MSM_BUS_DBG("buffer: %s\n size: %zu\n", buf, sizeof(ubuf));

	list_for_each_entry(cldata, &cl_list, list) {
		if (strnstr(chid, cldata->pdata->name, cnt)) {
			found = 1;
			cldata = cldata;
			strsep(&chid, " ");
			if (chid) {
				ret = strict_strtoul(chid, 10, &index);
				if (ret) {
					MSM_BUS_DBG("Index conversion"
						" failed\n");
					return -EFAULT;
				}
			} else {
				MSM_BUS_DBG("Error parsing input. Index not"
					" found\n");
				found = 0;
			}
			break;
		}
	}

	if (found)
		msm_bus_dbg_update_request(cldata, index);
	kfree(buf);
	return cnt;
}

/**
 * The following funtions are used for viewing the commit data
 * for each fabric
 */
static ssize_t fabric_data_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	struct msm_bus_fab_list *fablist = NULL;
	int bsize = 0;
	ssize_t ret;
	const char *name = file->private_data;
	int found = 0;

	mutex_lock(&msm_bus_dbg_fablist_lock);
	list_for_each_entry(fablist, &fabdata_list, list) {
		if (strcmp(fablist->name, name) == 0) {
			found = 1;
			break;
		}
	}
	if (!found)
		return -ENOENT;
	bsize = fablist->size;
	ret = simple_read_from_buffer(buf, count, ppos,
		fablist->buffer, bsize);
	mutex_unlock(&msm_bus_dbg_fablist_lock);
	return ret;
}

static const struct file_operations fabric_data_fops = {
	.open		= client_data_open,
	.read		= fabric_data_read,
};

static int msm_bus_dbg_record_fabric(const char *fabname, struct dentry *file)
{
	struct msm_bus_fab_list *fablist;
	int ret = 0;

	mutex_lock(&msm_bus_dbg_fablist_lock);
	fablist = kmalloc(sizeof(struct msm_bus_fab_list), GFP_KERNEL);
	if (!fablist) {
		MSM_BUS_DBG("Failed to allocate memory for commit data\n");
		ret =  -ENOMEM;
		goto err;
	}

	fablist->name = fabname;
	fablist->size = 0;
	list_add_tail(&fablist->list, &fabdata_list);
err:
	mutex_unlock(&msm_bus_dbg_fablist_lock);
	return ret;
}

static void msm_bus_dbg_free_fabric(const char *fabname)
{
	struct msm_bus_fab_list *fablist = NULL;

	mutex_lock(&msm_bus_dbg_fablist_lock);
	list_for_each_entry(fablist, &fabdata_list, list) {
		if (strcmp(fablist->name, fabname) == 0) {
			debugfs_remove(fablist->file);
			list_del(&fablist->list);
			kfree(fablist);
			break;
		}
	}
	mutex_unlock(&msm_bus_dbg_fablist_lock);
}

static int msm_bus_dbg_fill_fab_buffer(const char *fabname,
	void *cdata, int nmasters, int nslaves,
	int ntslaves)
{
	int i;
	char *buf = NULL;
	struct msm_bus_fab_list *fablist = NULL;
	struct timespec ts;
	int found = 0;

	mutex_lock(&msm_bus_dbg_fablist_lock);
	list_for_each_entry(fablist, &fabdata_list, list) {
		if (strcmp(fablist->name, fabname) == 0) {
			found = 1;
			break;
		}
	}
	if (!found)
		return -ENOENT;

	if (fablist->file == NULL) {
		MSM_BUS_DBG("Fabric dbg entry does not exist\n");
		mutex_unlock(&msm_bus_dbg_fablist_lock);
		return -EFAULT;
	}

	if (fablist->size < MAX_BUFF_SIZE - 256)
		i = fablist->size;
	else {
		i = 0;
		fablist->size = 0;
	}
	buf = fablist->buffer;
	mutex_unlock(&msm_bus_dbg_fablist_lock);
	ts = ktime_to_timespec(ktime_get());
	i += scnprintf(buf + i, MAX_BUFF_SIZE - i, "\n%d.%d\n",
		(int)ts.tv_sec, (int)ts.tv_nsec);

	msm_bus_rpm_fill_cdata_buffer(&i, buf, MAX_BUFF_SIZE, cdata,
		nmasters, nslaves, ntslaves);
	i += scnprintf(buf + i, MAX_BUFF_SIZE - i, "\n");
	mutex_lock(&msm_bus_dbg_fablist_lock);
	fablist->size = i;
	mutex_unlock(&msm_bus_dbg_fablist_lock);
	return 0;
}

static const struct file_operations msm_bus_dbg_update_request_fops = {
	.open = client_data_open,
	.write = msm_bus_dbg_update_request_write,
};

/**
 * msm_bus_dbg_client_data() - Add debug data for clients
 * @pdata: Platform data of the client
 * @index: The current index or operation to be performed
 * @clid: Client handle obtained during registration
 */
void msm_bus_dbg_client_data(struct msm_bus_scale_pdata *pdata, int index,
	uint32_t clid)
{
	struct dentry *file = NULL;

	if (index == MSM_BUS_DBG_REGISTER) {
		msm_bus_dbg_record_client(pdata, index, clid, file);
		if (!pdata->name) {
			MSM_BUS_DBG("Cannot create debugfs entry. Null name\n");
			return;
		}
	} else if (index == MSM_BUS_DBG_UNREGISTER) {
		msm_bus_dbg_free_client(clid);
		MSM_BUS_DBG("Client %d unregistered\n", clid);
	} else
		msm_bus_dbg_fill_cl_buffer(pdata, index, clid);
}
EXPORT_SYMBOL(msm_bus_dbg_client_data);

/**
 * msm_bus_dbg_commit_data() - Add commit data from fabrics
 * @fabname: Fabric name specified in platform data
 * @cdata: Commit Data
 * @nmasters: Number of masters attached to fabric
 * @nslaves: Number of slaves attached to fabric
 * @ntslaves: Number of tiered slaves attached to fabric
 * @op: Operation to be performed
 */
void msm_bus_dbg_commit_data(const char *fabname, void *cdata,
	int nmasters, int nslaves, int ntslaves, int op)
{
	struct dentry *file = NULL;

	if (op == MSM_BUS_DBG_REGISTER)
		msm_bus_dbg_record_fabric(fabname, file);
	else if (op == MSM_BUS_DBG_UNREGISTER)
		msm_bus_dbg_free_fabric(fabname);
	else
		msm_bus_dbg_fill_fab_buffer(fabname, cdata, nmasters,
			nslaves, ntslaves);
}
EXPORT_SYMBOL(msm_bus_dbg_commit_data);

static int __init msm_bus_debugfs_init(void)
{
	struct dentry *commit, *shell_client;
	struct msm_bus_fab_list *fablist;
	struct msm_bus_cldata *cldata = NULL;
	uint64_t val = 0;

	dir = debugfs_create_dir("msm-bus-dbg", NULL);
	if ((!dir) || IS_ERR(dir)) {
		MSM_BUS_ERR("Couldn't create msm-bus-dbg\n");
		goto err;
	}

	clients = debugfs_create_dir("client-data", dir);
	if ((!dir) || IS_ERR(dir)) {
		MSM_BUS_ERR("Couldn't create clients\n");
		goto err;
	}

	shell_client = debugfs_create_dir("shell-client", dir);
	if ((!dir) || IS_ERR(dir)) {
		MSM_BUS_ERR("Couldn't create clients\n");
		goto err;
	}

	commit = debugfs_create_dir("commit-data", dir);
	if ((!dir) || IS_ERR(dir)) {
		MSM_BUS_ERR("Couldn't create commit\n");
		goto err;
	}

	if (debugfs_create_file("update_request", S_IRUGO | S_IWUSR,
		shell_client, &val, &shell_client_en_fops) == NULL)
		goto err;
	if (debugfs_create_file("ib", S_IRUGO | S_IWUSR, shell_client, &val,
		&shell_client_ib_fops) == NULL)
		goto err;
	if (debugfs_create_file("ab", S_IRUGO | S_IWUSR, shell_client, &val,
		&shell_client_ab_fops) == NULL)
		goto err;
	if (debugfs_create_file("slv", S_IRUGO | S_IWUSR, shell_client,
		&val, &shell_client_slv_fops) == NULL)
		goto err;
	if (debugfs_create_file("mas", S_IRUGO | S_IWUSR, shell_client,
		&val, &shell_client_mas_fops) == NULL)
		goto err;
	if (debugfs_create_file("update-request", S_IRUGO | S_IWUSR,
		clients, NULL, &msm_bus_dbg_update_request_fops) == NULL)
		goto err;

	list_for_each_entry(cldata, &cl_list, list) {
		if (cldata->pdata->name == NULL) {
			MSM_BUS_DBG("Client name not found\n");
			continue;
		}
		cldata->file = msm_bus_dbg_create(cldata->
			pdata->name, S_IRUGO, clients, cldata->clid);
	}

	mutex_lock(&msm_bus_dbg_fablist_lock);
	list_for_each_entry(fablist, &fabdata_list, list) {
		fablist->file = debugfs_create_file(fablist->name, S_IRUGO,
			commit, (void *)fablist->name, &fabric_data_fops);
		if (fablist->file == NULL) {
			MSM_BUS_DBG("Cannot create files for commit data\n");
			goto err;
		}
	}
	mutex_unlock(&msm_bus_dbg_fablist_lock);

	msm_bus_dbg_init_vectors();
	return 0;
err:
	debugfs_remove_recursive(dir);
	return -ENODEV;
}
late_initcall(msm_bus_debugfs_init);

static void __exit msm_bus_dbg_teardown(void)
{
	struct msm_bus_fab_list *fablist = NULL, *fablist_temp;
	struct msm_bus_cldata *cldata = NULL, *cldata_temp;

	debugfs_remove_recursive(dir);
	list_for_each_entry_safe(cldata, cldata_temp, &cl_list, list) {
		list_del(&cldata->list);
		kfree(cldata);
	}
	mutex_lock(&msm_bus_dbg_fablist_lock);
	list_for_each_entry_safe(fablist, fablist_temp, &fabdata_list, list) {
		list_del(&fablist->list);
		kfree(fablist);
	}
	mutex_unlock(&msm_bus_dbg_fablist_lock);
}
module_exit(msm_bus_dbg_teardown);
MODULE_DESCRIPTION("Debugfs for msm bus scaling client");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Gagan Mac <gmac@codeaurora.org>");
