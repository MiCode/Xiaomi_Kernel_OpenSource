// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/io.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "synx_api.h"
#include "synx_debugfs.h"
#include "synx_util.h"

#define MAX_DBG_BUF_SIZE (36 * SYNX_MAX_OBJS)
#define BUF_SIZE 64

struct dentry *my_direc;
static const char delim[] = ",";
int columns = NAME_COLUMN | ID_COLUMN |
	BOUND_COLUMN | STATE_COLUMN | ERROR_CODES;

void populate_bound_rows(
	struct synx_coredata *row,
	char *cur,
	char *end)
{
	int j;

	for (j = 0; j < row->num_bound_synxs;
		j++) {
		cur += scnprintf(cur, end - cur,
			"\n\tID: %d",
			row->bound_synxs[j].external_desc.id[0]);
	}
}
static ssize_t synx_table_read(struct file *file,
		char *buf,
		size_t count,
		loff_t *ppos)
{

	struct synx_device *dev = file->private_data;
	struct error_node *err_node, *err_node_tmp;
	struct synx_client *client;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *row;
	char *dbuf, *cur, *end;

	int i = 0;
	int j = 0;
	int status = SYNX_STATE_INVALID;
	ssize_t len = 0;

	dbuf = kzalloc(MAX_DBG_BUF_SIZE, GFP_KERNEL);
	if (!dbuf)
		return -ENOMEM;

	cur = dbuf;
	end = cur + MAX_DBG_BUF_SIZE;
	if (columns & NAME_COLUMN)
		cur += scnprintf(cur, end - cur, "|   Name   |");
	if (columns & ID_COLUMN)
		cur += scnprintf(cur, end - cur, "|    ID    |");
	if (columns & BOUND_COLUMN)
		cur += scnprintf(cur, end - cur, "|   Bound   |");
	if (columns & STATE_COLUMN)
		cur += scnprintf(cur, end - cur, "|   Status   |");
	cur += scnprintf(cur, end - cur, "\n");

	for (j = 0; j < SYNX_MAX_CLIENTS; j++) {
		struct synx_client_metadata *client_meta;

		mutex_lock(&synx_dev->dev_table_lock);
		client_meta = &synx_dev->client_table[j];
		if (!client_meta->client) {
			mutex_unlock(&synx_dev->dev_table_lock);
			continue;
		}
		kref_get(&client_meta->refcount);
		client = client_meta->client;
		mutex_unlock(&synx_dev->dev_table_lock);

		cur += scnprintf(cur, end - cur,
			"=============== session %08u ===============\n",
			client->id);
		cur += scnprintf(cur, end - cur,
			"session name ::: %s\n",
			client->name);
		for (i = 0; i < SYNX_MAX_OBJS; i++) {
			synx_data = synx_util_acquire_handle(client, i);
			row = synx_util_obtain_object(synx_data);
			if (!row || !row->fence)
				continue;

			mutex_lock(&row->obj_lock);
			if (columns & NAME_COLUMN)
				cur += scnprintf(cur, end - cur,
					"|%10s|", row->name);
			if (columns & ID_COLUMN)
				cur += scnprintf(cur, end - cur,
					"|%10d|", i);
			if (columns & BOUND_COLUMN)
				cur += scnprintf(cur, end - cur,
					"|%11d|", row->num_bound_synxs);
			if (columns & STATE_COLUMN) {
				status =
					synx_util_get_object_status(row);
				cur += scnprintf(cur, end - cur,
					"|%10d|", status);
			}
			if ((columns & BOUND_COLUMN) &&
				(row->num_bound_synxs > 0)) {
				cur += scnprintf(
					cur, end - cur, "\nBound synx: ");
				populate_bound_rows(row,
					cur,
					end);
			}
			mutex_unlock(&row->obj_lock);
			cur += scnprintf(cur, end - cur, "\n");
			synx_util_release_handle(synx_data);
		}
		cur += scnprintf(cur, end - cur,
			"\n================================================\n\n");
		synx_put_client(client);
	}

	if (columns & ERROR_CODES && !list_empty(
		&dev->error_list)) {
		cur += scnprintf(
			cur, end - cur, "\nError(s): ");

		mutex_lock(&dev->error_lock);
		list_for_each_entry_safe(
			err_node, err_node_tmp,
			&dev->error_list,
			node) {
			cur += scnprintf(cur, end - cur,
			"\n\tTime: %s - ID: %d - Code: %d",
			err_node->timestamp,
			err_node->h_synx,
			err_node->error_code);
			list_del(&err_node->node);
			kfree(err_node);
		}
		mutex_unlock(&dev->error_lock);
	}

	len = simple_read_from_buffer(buf, count, ppos,
		dbuf, cur - dbuf);
	kfree(dbuf);
	return len;
}

static ssize_t synx_table_write(struct file *file,
		const char __user *buf,
		size_t count,
		loff_t *ppos)
{
	char *ptr;
	char *kbuffer = kzalloc(BUF_SIZE, GFP_KERNEL);
	int stat = -1;

	if (!kbuffer)
		return -ENOMEM;
	stat = copy_from_user(kbuffer, buf, 48);
	if (stat != 0) {
		kfree(kbuffer);
		return -EFAULT;
	}
	while ((ptr = strsep(&kbuffer, delim)) != NULL) {
		ptr += '\0';
		if (strcmp(ptr, "bound\n") == 0)
			columns = columns ^ BOUND_COLUMN;
		else if (strcmp(ptr, "name\n") == 0)
			columns = columns ^ NAME_COLUMN;
		else if (strcmp(ptr, "synxid\n") == 0)
			columns = columns ^ ID_COLUMN;
		else if (strcmp(ptr, "status\n") == 0)
			columns = columns ^ STATE_COLUMN;
		else if (strcmp(ptr, "errors\n") == 0)
			columns = columns ^ ERROR_CODES;
	}
	kfree(kbuffer);
	return count;
}

static const struct file_operations synx_table_fops = {
	.owner = THIS_MODULE,
	.read = synx_table_read,
	.write = synx_table_write,
	.open = simple_open,
};

struct dentry *synx_init_debugfs_dir(struct synx_device *dev)
{
	struct dentry *dir = NULL;

	dir = debugfs_create_dir("synx_debug", NULL);

	if (!dir) {
		pr_debug("Failed to create debugfs for synx\n");
		return NULL;
	}
	if (!debugfs_create_file("synx_table",
		0644, dir, dev, &synx_table_fops)) {
		pr_debug("Failed to create debugfs file for synx\n");
		return NULL;
	}
	mutex_init(&dev->error_lock);
	INIT_LIST_HEAD(&dev->error_list);
	return dir;
}

void synx_remove_debugfs_dir(struct synx_device *dev)
{
	debugfs_remove_recursive(dev->debugfs_root);
}
