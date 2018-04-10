/* Copyright (c) 2014-2015, 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#define DRIVER_NAME "msm_sharedmem"
#define pr_fmt(fmt) DRIVER_NAME ": %s: " fmt, __func__

#include <linux/err.h>
#include <linux/module.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/debugfs.h>
#include <soc/qcom/msm_qmi_interface.h>
#include "sharedmem_qmi.h"
#include "remote_filesystem_access_v01.h"

#define RFSA_SERVICE_INSTANCE_NUM 1
#define SHARED_ADDR_ENTRY_NAME_MAX_LEN 10

struct shared_addr_entry {
	u32 id;
	u64 address;
	u32 size;
	u64 request_count;
	bool is_addr_dynamic;
	char name[SHARED_ADDR_ENTRY_NAME_MAX_LEN + 1];
};

struct shared_addr_list {
	struct list_head node;
	struct shared_addr_entry entry;
};

static struct shared_addr_list list;

static struct qmi_handle *sharedmem_qmi_svc_handle;
static void sharedmem_qmi_svc_recv_msg(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_recv_msg, sharedmem_qmi_svc_recv_msg);
static struct workqueue_struct *sharedmem_qmi_svc_workqueue;
static struct dentry *dir_ent;

static u32 rfsa_count;
static u32 rmts_count;

static DECLARE_RWSEM(sharedmem_list_lock); /* declare list lock semaphore */

static struct work_struct sharedmem_qmi_init_work;

static struct msg_desc rfsa_get_buffer_addr_req_desc = {
	.max_msg_len = RFSA_GET_BUFF_ADDR_REQ_MSG_MAX_LEN_V01,
	.msg_id = QMI_RFSA_GET_BUFF_ADDR_REQ_MSG_V01,
	.ei_array = rfsa_get_buff_addr_req_msg_v01_ei,
};

static struct msg_desc rfsa_get_buffer_addr_resp_desc = {
	.max_msg_len = RFSA_GET_BUFF_ADDR_RESP_MSG_MAX_LEN_V01,
	.msg_id = QMI_RFSA_GET_BUFF_ADDR_RESP_MSG_V01,
	.ei_array = rfsa_get_buff_addr_resp_msg_v01_ei,
};

void sharedmem_qmi_add_entry(struct sharemem_qmi_entry *qmi_entry)
{
	struct shared_addr_list *list_entry;

	list_entry = kzalloc(sizeof(*list_entry), GFP_KERNEL);

	/* If we cannot add the entry log the failure and bail */
	if (list_entry == NULL) {
		pr_err("Alloc of new list entry failed\n");
		return;
	}

	/* Copy as much of the client name that can fit in the entry. */
	strlcpy(list_entry->entry.name, qmi_entry->client_name,
		sizeof(list_entry->entry.name));

	/* Setup the rest of the entry. */
	list_entry->entry.id = qmi_entry->client_id;
	list_entry->entry.address = qmi_entry->address;
	list_entry->entry.size = qmi_entry->size;
	list_entry->entry.is_addr_dynamic = qmi_entry->is_addr_dynamic;
	list_entry->entry.request_count = 0;

	down_write(&sharedmem_list_lock);
	list_add_tail(&(list_entry->node), &(list.node));
	up_write(&sharedmem_list_lock);
	pr_debug("Added new entry to list\n");

}

static int get_buffer_for_client(u32 id, u32 size, u64 *address)
{
	int result = -ENOENT;
	int client_found = 0;
	struct list_head *curr_node;
	struct shared_addr_list *list_entry;

	if (size == 0)
		return -ENOMEM;

	down_read(&sharedmem_list_lock);

	list_for_each(curr_node, &list.node) {
		list_entry = list_entry(curr_node, struct shared_addr_list,
					node);
		if (list_entry->entry.id == id) {
			if (list_entry->entry.size >= size) {
				*address = list_entry->entry.address;
				list_entry->entry.request_count++;
				result = 0;
			} else {
				pr_err("Shared mem req too large for id=%u\n",
					id);
				result = -ENOMEM;
			}
			client_found = 1;
			break;
		}
	}

	up_read(&sharedmem_list_lock);

	if (client_found != 1) {
		pr_err("Unknown client id %u\n", id);
		result = -ENOENT;
	}
	return result;
}

static int sharedmem_qmi_get_buffer(void *conn_h, void *req_handle, void *req)
{
	struct rfsa_get_buff_addr_req_msg_v01 *get_buffer_req;
	struct rfsa_get_buff_addr_resp_msg_v01 get_buffer_resp;
	int result;
	u64 address = 0;

	get_buffer_req = (struct rfsa_get_buff_addr_req_msg_v01 *)req;
	pr_debug("req->client_id = 0x%X and req->size = %d\n",
		get_buffer_req->client_id, get_buffer_req->size);

	result = get_buffer_for_client(get_buffer_req->client_id,
					get_buffer_req->size, &address);
	if (result != 0)
		return result;

	if (address == 0) {
		pr_err("Entry found for client id= 0x%X but address is zero\n",
			get_buffer_req->client_id);
		return -ENOMEM;
	}

	memset(&get_buffer_resp, 0, sizeof(get_buffer_resp));
	get_buffer_resp.address_valid = 1;
	get_buffer_resp.address = address;
	get_buffer_resp.resp.result = QMI_RESULT_SUCCESS_V01;

	result = qmi_send_resp_from_cb(sharedmem_qmi_svc_handle, conn_h,
				req_handle,
				&rfsa_get_buffer_addr_resp_desc,
				&get_buffer_resp,
				sizeof(get_buffer_resp));
	return result;
}


static int sharedmem_qmi_connect_cb(struct qmi_handle *handle, void *conn_h)
{
	if (sharedmem_qmi_svc_handle != handle || !conn_h)
		return -EINVAL;
	return 0;
}

static int sharedmem_qmi_disconnect_cb(struct qmi_handle *handle, void *conn_h)
{
	if (sharedmem_qmi_svc_handle != handle || !conn_h)
		return -EINVAL;
	return 0;
}

static int sharedmem_qmi_req_desc_cb(unsigned int msg_id,
				struct msg_desc **req_desc)
{
	int rc;

	switch (msg_id) {
	case QMI_RFSA_GET_BUFF_ADDR_REQ_MSG_V01:
		*req_desc = &rfsa_get_buffer_addr_req_desc;
		rc = sizeof(struct rfsa_get_buff_addr_req_msg_v01);
		break;

	default:
		rc = -ENOTSUPP;
		break;
	}
	return rc;
}

static int sharedmem_qmi_req_cb(struct qmi_handle *handle, void *conn_h,
				void *req_handle, unsigned int msg_id,
				void *req)
{
	int rc = -ENOTSUPP;

	if (sharedmem_qmi_svc_handle != handle || !conn_h)
		return -EINVAL;

	if (msg_id == QMI_RFSA_GET_BUFF_ADDR_REQ_MSG_V01)
		rc = sharedmem_qmi_get_buffer(conn_h, req_handle, req);

	return rc;
}

#define DEBUG_BUF_SIZE (2048)
static char *debug_buffer;
static u32 debug_data_size;
static struct mutex dbg_buf_lock;

static ssize_t debug_read(struct file *file, char __user *buf,
			  size_t count, loff_t *file_pos)
{
	return simple_read_from_buffer(buf, count, file_pos, debug_buffer,
					debug_data_size);
}

static u32 fill_debug_info(char *buffer, u32 buffer_size)
{
	u32 size = 0;
	struct list_head *curr_node;
	struct shared_addr_list *list_entry;

	memset(buffer, 0, buffer_size);
	size += scnprintf(buffer + size, buffer_size - size, "\n");

	down_read(&sharedmem_list_lock);
	list_for_each(curr_node, &list.node) {
		list_entry = list_entry(curr_node, struct shared_addr_list,
					node);
		size += scnprintf(buffer + size, buffer_size - size,
				"Client_name: %s\n", list_entry->entry.name);
		size += scnprintf(buffer + size, buffer_size - size,
				"Client_id: 0x%08X\n", list_entry->entry.id);
		size += scnprintf(buffer + size, buffer_size - size,
				"Buffer Size: 0x%08X (%d)\n",
				list_entry->entry.size,
				list_entry->entry.size);
		size += scnprintf(buffer + size, buffer_size - size,
				"Address: 0x%016llX\n",
				list_entry->entry.address);
		size += scnprintf(buffer + size, buffer_size - size,
				"Address Allocation: %s\n",
				(list_entry->entry.is_addr_dynamic ?
				"Dynamic" : "Static"));
		size += scnprintf(buffer + size, buffer_size - size,
				"Request count: %llu\n",
				list_entry->entry.request_count);
		size += scnprintf(buffer + size, buffer_size - size, "\n\n");
	}
	up_read(&sharedmem_list_lock);

	size += scnprintf(buffer + size, buffer_size - size,
			"RFSA server start count = %u\n", rfsa_count);
	size += scnprintf(buffer + size, buffer_size - size,
			"RMTS server start count = %u\n", rmts_count);

	size += scnprintf(buffer + size, buffer_size - size, "\n");
	return size;
}

static int debug_open(struct inode *inode, struct file *file)
{
	u32 buffer_size;

	mutex_lock(&dbg_buf_lock);
	if (debug_buffer != NULL) {
		mutex_unlock(&dbg_buf_lock);
		return -EBUSY;
	}
	buffer_size = DEBUG_BUF_SIZE;
	debug_buffer = kzalloc(buffer_size, GFP_KERNEL);
	if (debug_buffer == NULL) {
		mutex_unlock(&dbg_buf_lock);
		return -ENOMEM;
	}
	debug_data_size = fill_debug_info(debug_buffer, buffer_size);
	mutex_unlock(&dbg_buf_lock);
	return 0;
}

static int debug_close(struct inode *inode, struct file *file)
{
	mutex_lock(&dbg_buf_lock);
	kfree(debug_buffer);
	debug_buffer = NULL;
	debug_data_size = 0;
	mutex_unlock(&dbg_buf_lock);
	return 0;
}

static const struct file_operations debug_ops = {
	.read = debug_read,
	.open = debug_open,
	.release = debug_close,
};

static int rfsa_increment(void *data, u64 val)
{
	if (rfsa_count != ~0)
		rfsa_count++;
	return 0;
}

static int rmts_increment(void *data, u64 val)
{
	if (rmts_count != ~0)
		rmts_count++;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(rfsa_fops, NULL, rfsa_increment, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(rmts_fops, NULL, rmts_increment, "%llu\n");

static void debugfs_init(void)
{
	struct dentry *f_ent;

	mutex_init(&dbg_buf_lock);
	dir_ent = debugfs_create_dir("rmt_storage", NULL);
	if (IS_ERR(dir_ent)) {
		pr_err("Failed to create debug_fs directory\n");
		return;
	}

	f_ent = debugfs_create_file("info", 0400, dir_ent, NULL, &debug_ops);
	if (IS_ERR(f_ent)) {
		pr_err("Failed to create debug_fs info file\n");
		return;
	}

	f_ent = debugfs_create_file("rfsa", 0200, dir_ent, NULL, &rfsa_fops);
	if (IS_ERR(f_ent)) {
		pr_err("Failed to create debug_fs rfsa file\n");
		return;
	}

	f_ent = debugfs_create_file("rmts", 0200, dir_ent, NULL, &rmts_fops);
	if (IS_ERR(f_ent)) {
		pr_err("Failed to create debug_fs rmts file\n");
		return;
	}
}

static void debugfs_exit(void)
{
	debugfs_remove_recursive(dir_ent);
	mutex_destroy(&dbg_buf_lock);
}

static void sharedmem_qmi_svc_recv_msg(struct work_struct *work)
{
	int rc;

	do {
		pr_debug("Notified about a Receive Event\n");
	} while ((rc = qmi_recv_msg(sharedmem_qmi_svc_handle)) == 0);

	if (rc != -ENOMSG)
		pr_err("Error receiving message\n");
}

static void sharedmem_qmi_notify(struct qmi_handle *handle,
		enum qmi_event_type event, void *priv)
{
	switch (event) {
	case QMI_RECV_MSG:
		queue_delayed_work(sharedmem_qmi_svc_workqueue,
				   &work_recv_msg, 0);
		break;
	default:
		break;
	}
}

static struct qmi_svc_ops_options sharedmem_qmi_ops_options = {
	.version = 1,
	.service_id = RFSA_SERVICE_ID_V01,
	.service_vers = RFSA_SERVICE_VERS_V01,
	.service_ins = RFSA_SERVICE_INSTANCE_NUM,
	.connect_cb = sharedmem_qmi_connect_cb,
	.disconnect_cb = sharedmem_qmi_disconnect_cb,
	.req_desc_cb = sharedmem_qmi_req_desc_cb,
	.req_cb = sharedmem_qmi_req_cb,
};


static void sharedmem_register_qmi(void)
{
	int rc;

	sharedmem_qmi_svc_workqueue =
		create_singlethread_workqueue("sharedmem_qmi_work");
	if (!sharedmem_qmi_svc_workqueue)
		return;

	sharedmem_qmi_svc_handle = qmi_handle_create(sharedmem_qmi_notify,
							NULL);
	if (!sharedmem_qmi_svc_handle) {
		pr_err("Creating sharedmem_qmi qmi handle failed\n");
		destroy_workqueue(sharedmem_qmi_svc_workqueue);
		return;
	}
	rc = qmi_svc_register(sharedmem_qmi_svc_handle,
				&sharedmem_qmi_ops_options);
	if (rc < 0) {
		pr_err("Registering sharedmem_qmi failed %d\n", rc);
		qmi_handle_destroy(sharedmem_qmi_svc_handle);
		destroy_workqueue(sharedmem_qmi_svc_workqueue);
		return;
	}
	pr_info("qmi init successful\n");
}

static void sharedmem_qmi_init_worker(struct work_struct *work)
{
	sharedmem_register_qmi();
	debugfs_init();
}

int sharedmem_qmi_init(void)
{
	INIT_LIST_HEAD(&list.node);
	INIT_WORK(&sharedmem_qmi_init_work, sharedmem_qmi_init_worker);
	schedule_work(&sharedmem_qmi_init_work);
	return 0;
}

void sharedmem_qmi_exit(void)
{
	qmi_svc_unregister(sharedmem_qmi_svc_handle);
	flush_workqueue(sharedmem_qmi_svc_workqueue);
	qmi_handle_destroy(sharedmem_qmi_svc_handle);
	destroy_workqueue(sharedmem_qmi_svc_workqueue);
	debugfs_exit();
}
