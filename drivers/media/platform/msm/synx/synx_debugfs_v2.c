// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/io.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "synx_api.h"
#include "synx_debugfs_v2.h"
#include "synx_util_v2.h"

#define MAX_DBG_BUF_SIZE (36 * SYNX_MAX_OBJS)

struct dentry *my_direc;

int synx_columns = NAME_COLUMN | ID_COLUMN |
	STATE_COLUMN | GLOBAL_COLUMN;
EXPORT_SYMBOL(synx_columns);

int synx_debug = SYNX_ERR | SYNX_WARN |
	SYNX_INFO;
EXPORT_SYMBOL(synx_debug);

void populate_bound_rows(
	struct synx_coredata *row, char *cur, char *end)
{
	int j;

	for (j = 0; j < row->num_bound_synxs; j++)
		cur += scnprintf(cur, end - cur,
			"\n\tID: %d",
			row->bound_synxs[j].external_desc.id);
}
static ssize_t synx_table_read(struct file *file,
		char *buf,
		size_t count,
		loff_t *ppos)
{
	struct synx_device *dev = file->private_data;
	struct error_node *err_node, *err_node_tmp;
	char *dbuf, *cur, *end;
	int rc = SYNX_SUCCESS;
	ssize_t len = 0;

	dbuf = kzalloc(MAX_DBG_BUF_SIZE, GFP_KERNEL);
	if (!dbuf)
		return -ENOMEM;

	/* dump client details */
	cur = dbuf;
	end = cur + MAX_DBG_BUF_SIZE;
	if (synx_columns & NAME_COLUMN)
		cur += scnprintf(cur, end - cur, "|   Name   |");
	if (synx_columns & ID_COLUMN)
		cur += scnprintf(cur, end - cur, "|    ID    |");
	if (synx_columns & STATE_COLUMN)
		cur += scnprintf(cur, end - cur, "|   Status   |");
	if (synx_columns & FENCE_COLUMN)
		cur += scnprintf(cur, end - cur, "|   Fence   |");
	if (synx_columns & COREDATA_COLUMN)
		cur += scnprintf(cur, end - cur, "|   Coredata |");
	if (synx_columns & GLOBAL_COLUMN)
		cur += scnprintf(cur, end - cur, "|   Coredata |");
	if (synx_columns & BOUND_COLUMN)
		cur += scnprintf(cur, end - cur, "|   Bound   |");
	cur += scnprintf(cur, end - cur, "\n");

	rc = synx_global_dump_shared_memory();
	if (rc) {
		cur += scnprintf(cur, end - cur,
			"Err %d: Failed to dump global shared mem\n", rc);
	}

	if (synx_columns & ERROR_CODES && !list_empty(
		&dev->error_list)) {
		cur += scnprintf(
			cur, end - cur, "\nError(s): ");

		mutex_lock(&dev->error_lock);
		list_for_each_entry_safe(
			 err_node, err_node_tmp,
			 &dev->error_list, node) {
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
	return 0;
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
		dprintk(SYNX_ERR, "Failed to create debugfs for synx\n");
		return NULL;
	}

	debugfs_create_u32("debug_level", 0644, dir, &synx_debug);
	debugfs_create_u32("column_level", 0644, dir, &synx_columns);

	if (!debugfs_create_file("synx_table",
		0644, dir, dev, &synx_table_fops)) {
		dprintk(SYNX_ERR, "Failed to create debugfs file for synx\n");
		return NULL;
	}

	return dir;
}

void synx_remove_debugfs_dir(struct synx_device *dev)
{
	debugfs_remove_recursive(dev->debugfs_root);
}
