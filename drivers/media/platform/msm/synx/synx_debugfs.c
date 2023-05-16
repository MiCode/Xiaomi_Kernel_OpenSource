// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, 2022, The Linux Foundation. All rights reserved.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/list.h>

#include "synx_api.h"
#include "synx_private.h"
#include "synx_util.h"
#include "synx_debugfs.h"

#define MAX_DBG_BUF_SIZE (36 * SYNX_MAX_OBJS)

struct dentry *my_direc;
const char delim[] = ",";
int columns = NAME_COLUMN |
	BOUND_COLUMN | STATE_COLUMN | ERROR_CODES;

void populate_bound_rows(
	struct synx_table_row *row,
	char *cur,
	char *end)
{
	int j;
	int state = SYNX_STATE_INVALID;

	for (j = 0; j < row->num_bound_synxs;
		j++) {
		cur += scnprintf(cur, end - cur,
			"\n\tID: %d State: %s",
			row->bound_synxs[j].external_data->synx_obj,
			state);
	}
}
static ssize_t synx_table_read(struct file *file,
		char *buf,
		size_t count,
		loff_t *ppos)
{

	struct synx_device *dev = file->private_data;
	struct error_node *err_node, *err_node_tmp;
	struct synx_table_row *row;
	char *dbuf, *cur, *end;

	int i = 0;
	int state = SYNX_STATE_INVALID;
	ssize_t len = 0;

	dbuf = kzalloc(MAX_DBG_BUF_SIZE, GFP_KERNEL);
	if (!dbuf)
		return -ENOMEM;
	cur = dbuf;
	end = cur + MAX_DBG_BUF_SIZE;
	if (columns & NAME_COLUMN)
		cur += scnprintf(cur, end - cur, "|   Name   |");
	if (columns & BOUND_COLUMN)
		cur += scnprintf(cur, end - cur, "|   Bound   |");
	if (columns & STATE_COLUMN)
		cur += scnprintf(cur, end - cur, "|  Status  |");
	cur += scnprintf(cur, end - cur, "\n");
	for (i = 0; i < SYNX_MAX_OBJS; i++) {
		row = &dev->synx_table[i];

		if (!row->index)
			continue;

		mutex_lock(&dev->row_locks[row->index]);
		if (columns & NAME_COLUMN)
			cur += scnprintf(cur, end - cur,
				"|%10s|", row->name);
		if (columns & BOUND_COLUMN)
			cur += scnprintf(cur, end - cur,
				"|%11d|", row->num_bound_synxs);
		if (columns & STATE_COLUMN) {
			state = synx_status(row);
			cur += scnprintf(cur, end - cur,
				"|%10d|", state);
		}
		if ((columns & BOUND_COLUMN) &&
			(row->num_bound_synxs > 0)) {
			cur += scnprintf(
				cur, end - cur, "\nBound synx: ");
			populate_bound_rows(row,
				cur,
				end);
		}
		mutex_unlock(&dev->row_locks[row->index]);
		cur += scnprintf(cur, end - cur, "\n");
	}
	if (columns & ERROR_CODES && !list_empty(
		&synx_dev->synx_debug_head)) {
		cur += scnprintf(
			cur, end - cur, "\nError(s): ");

		spin_lock_bh(&synx_dev->synx_node_list_lock);
		list_for_each_entry_safe(
			err_node, err_node_tmp,
			&synx_dev->synx_debug_head,
			node) {
			if (err_node->timestamp != NULL) {
				cur += scnprintf(cur, end - cur,
				"\n\tTime: %s - ID: %d - Code: %d",
				err_node->timestamp,
				err_node->synx_obj,
				err_node->error_code);
			}
			list_del(&err_node->node);
			kfree(err_node);
		}
		spin_unlock_bh(&synx_dev->synx_node_list_lock);
	}

	cur += scnprintf(cur, end - cur,
			"\n=================================================\n");

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
	char *kbuffer = kzalloc(48, GFP_KERNEL);
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

struct dentry *init_synx_debug_dir(struct synx_device *dev)
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
	spin_lock_init(&dev->synx_node_list_lock);
	INIT_LIST_HEAD(&dev->synx_debug_head);
	return dir;
}
