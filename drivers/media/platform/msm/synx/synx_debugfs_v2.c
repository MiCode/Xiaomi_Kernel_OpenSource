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
	SYNX_INFO | SYNX_DBG;
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
	struct synx_client *client = NULL;
	struct synx_handle_coredata *synx_data;
	struct synx_coredata *synx_obj;
	char *dbuf, *cur, *end;

	int i = 0;
	int j = 0;
	int status = SYNX_STATE_INVALID;
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

	spin_lock_bh(&dev->native->metadata_map_lock);
	hash_for_each(synx_dev->native->client_metadata_map,
		i, client, node) {
		cur += scnprintf(cur, end - cur,
			"=============== session %08u ===============\n",
			client->id);
		cur += scnprintf(cur, end - cur,
			"session name ::: %s\n",
			client->name);
		spin_lock_bh(&client->handle_map_lock);
		hash_for_each(client->handle_map, j, synx_data, node) {
			synx_obj = synx_util_obtain_object(synx_data);

			if (synx_columns & NAME_COLUMN)
				cur += scnprintf(cur, end - cur,
					"|%10s|", synx_obj->name);
			if (synx_columns & ID_COLUMN)
				cur += scnprintf(cur, end - cur,
					"|%10u|", synx_data->key);
			if (synx_columns & STATE_COLUMN) {
				status =
					synx_util_get_object_status(synx_obj);
				cur += scnprintf(cur, end - cur,
					"|%10d|", status);
			}
			if (synx_columns & FENCE_COLUMN)
				cur += scnprintf(cur, end - cur,
					"|%pK|", synx_obj->fence);
			if (synx_columns & COREDATA_COLUMN)
				cur += scnprintf(cur, end - cur,
					"|%pK|", synx_obj);
			if (synx_columns & GLOBAL_COLUMN)
				cur += scnprintf(cur, end - cur,
					"|%10u|", synx_obj->global_idx);
			if (synx_columns & BOUND_COLUMN)
				cur += scnprintf(cur, end - cur,
					"|%11d|", synx_obj->num_bound_synxs);
			if ((synx_columns & BOUND_COLUMN) &&
				(synx_obj->num_bound_synxs > 0)) {
				cur += scnprintf(
					cur, end - cur, "\nBound synx: ");
				populate_bound_rows(synx_obj,
					cur,
					end);
			}
			cur += scnprintf(cur, end - cur, "\n");
			synx_util_put_object(synx_obj);
		}
		spin_unlock_bh(&client->handle_map_lock);
		cur += scnprintf(cur, end - cur,
			"\n================================================\n\n");
	}
	spin_unlock_bh(&synx_dev->native->metadata_map_lock);

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
