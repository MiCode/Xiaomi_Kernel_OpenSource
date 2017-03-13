/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "CAM-REQ-MGR_UTIL %s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/random.h>
#include <media/cam_req_mgr.h>
#include "cam_req_mgr_util.h"

#ifdef CONFIG_CAM_REQ_MGR_UTIL_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

static struct cam_req_mgr_util_hdl_tbl *hdl_tbl;
static struct mutex hdl_tbl_mutex = __MUTEX_INITIALIZER(hdl_tbl_mutex);

int cam_req_mgr_util_init(void)
{
	int rc = 0;
	int bitmap_size;

	mutex_lock(&hdl_tbl_mutex);
	if (hdl_tbl) {
		rc = -EINVAL;
		pr_err("Hdl_tbl is already present\n");
		goto hdl_tbl_check_failed;
	}

	hdl_tbl = kzalloc(sizeof(*hdl_tbl), GFP_KERNEL);
	if (!hdl_tbl) {
		rc = -ENOMEM;
		goto hdl_tbl_alloc_failed;
	}

	bitmap_size = BITS_TO_LONGS(CAM_REQ_MGR_MAX_HANDLES) * sizeof(long);
	hdl_tbl->bitmap = kzalloc(sizeof(bitmap_size), GFP_KERNEL);
	if (!hdl_tbl->bitmap) {
		rc = -ENOMEM;
		goto bitmap_alloc_fail;
	}
	hdl_tbl->bits = bitmap_size * BITS_PER_BYTE;
	mutex_unlock(&hdl_tbl_mutex);

	return rc;

bitmap_alloc_fail:
	kfree(hdl_tbl);
	hdl_tbl = NULL;
hdl_tbl_alloc_failed:
hdl_tbl_check_failed:
	mutex_unlock(&hdl_tbl_mutex);
	return rc;
}

int cam_req_mgr_util_deinit(void)
{
	mutex_lock(&hdl_tbl_mutex);
	if (!hdl_tbl) {
		pr_err("Hdl tbl is NULL\n");
		mutex_unlock(&hdl_tbl_mutex);
		return -EINVAL;
	}

	kfree(hdl_tbl->bitmap);
	hdl_tbl->bitmap = NULL;
	kfree(hdl_tbl);
	hdl_tbl = NULL;
	mutex_unlock(&hdl_tbl_mutex);

	return 0;
}

int cam_req_mgr_util_free_hdls(void)
{
	int i = 0;

	mutex_lock(&hdl_tbl_mutex);
	if (!hdl_tbl) {
		pr_err("Hdl tbl is NULL\n");
		mutex_unlock(&hdl_tbl_mutex);
		return -EINVAL;
	}

	for (i = 0; i < CAM_REQ_MGR_MAX_HANDLES; i++) {
		if (hdl_tbl->hdl[i].state == HDL_ACTIVE) {
			pr_err("Dev handle = %x session_handle = %x\n",
				hdl_tbl->hdl[i].hdl_value,
				hdl_tbl->hdl[i].session_hdl);
			hdl_tbl->hdl[i].state = HDL_FREE;
			clear_bit(i, hdl_tbl->bitmap);
		}
	}
	bitmap_zero(hdl_tbl->bitmap, CAM_REQ_MGR_MAX_HANDLES);
	mutex_unlock(&hdl_tbl_mutex);

	return 0;
}

static int32_t cam_get_free_handle_index(void)
{
	int idx;

	idx = find_first_zero_bit(hdl_tbl->bitmap, hdl_tbl->bits);

	if (idx >= CAM_REQ_MGR_MAX_HANDLES || idx < 0)
		return -ENOSR;

	set_bit(idx, hdl_tbl->bitmap);

	return idx;
}

int32_t cam_create_session_hdl(void *priv)
{
	int idx;
	int rand = 0;
	int32_t handle = 0;

	mutex_lock(&hdl_tbl_mutex);
	if (!hdl_tbl) {
		pr_err("Hdl tbl is NULL\n");
		mutex_unlock(&hdl_tbl_mutex);
		return -EINVAL;
	}

	idx = cam_get_free_handle_index();
	if (idx < 0) {
		pr_err("Unable to create session handle\n");
		mutex_unlock(&hdl_tbl_mutex);
		return idx;
	}

	get_random_bytes(&rand, CAM_REQ_MGR_RND1_BYTES);
	handle = GET_DEV_HANDLE(rand, HDL_TYPE_SESSION, idx);
	hdl_tbl->hdl[idx].session_hdl = handle;
	hdl_tbl->hdl[idx].hdl_value = handle;
	hdl_tbl->hdl[idx].type = HDL_TYPE_SESSION;
	hdl_tbl->hdl[idx].state = HDL_ACTIVE;
	hdl_tbl->hdl[idx].priv = priv;
	hdl_tbl->hdl[idx].ops = NULL;
	mutex_unlock(&hdl_tbl_mutex);

	return handle;
}

int32_t cam_create_device_hdl(struct cam_create_dev_hdl *hdl_data)
{
	int idx;
	int rand = 0;
	int32_t handle;

	mutex_lock(&hdl_tbl_mutex);
	if (!hdl_tbl) {
		pr_err("Hdl tbl is NULL\n");
		mutex_unlock(&hdl_tbl_mutex);
		return -EINVAL;
	}

	idx = cam_get_free_handle_index();
	if (idx < 0) {
		pr_err("Unable to create device handle\n");
		mutex_unlock(&hdl_tbl_mutex);
		return idx;
	}

	get_random_bytes(&rand, CAM_REQ_MGR_RND1_BYTES);
	handle = GET_DEV_HANDLE(rand, HDL_TYPE_DEV, idx);
	hdl_tbl->hdl[idx].session_hdl = hdl_data->session_hdl;
	hdl_tbl->hdl[idx].hdl_value = handle;
	hdl_tbl->hdl[idx].type = HDL_TYPE_DEV;
	hdl_tbl->hdl[idx].state = HDL_ACTIVE;
	hdl_tbl->hdl[idx].priv = hdl_data->priv;
	hdl_tbl->hdl[idx].ops = hdl_data->ops;
	mutex_unlock(&hdl_tbl_mutex);

	return handle;
}

void *cam_get_device_priv(int32_t dev_hdl)
{
	int idx;
	int type;
	void *priv;

	mutex_lock(&hdl_tbl_mutex);
	if (!hdl_tbl) {
		pr_err("Hdl tbl is NULL\n");
		goto device_priv_fail;
	}

	idx = CAM_REQ_MGR_GET_HDL_IDX(dev_hdl);
	if (idx >= CAM_REQ_MGR_MAX_HANDLES) {
		pr_err("Invalid idx\n");
		goto device_priv_fail;
	}

	if (hdl_tbl->hdl[idx].state != HDL_ACTIVE) {
		pr_err("Invalid state\n");
		goto device_priv_fail;
	}

	type = CAM_REQ_MGR_GET_HDL_TYPE(dev_hdl);
	if (HDL_TYPE_DEV != type && HDL_TYPE_SESSION != type) {
		pr_err("Invalid type\n");
		goto device_priv_fail;
	}

	if (hdl_tbl->hdl[idx].hdl_value != dev_hdl) {
		pr_err("Invalid hdl\n");
		goto device_priv_fail;
	}

	priv = hdl_tbl->hdl[idx].priv;
	mutex_unlock(&hdl_tbl_mutex);

	return priv;

device_priv_fail:
	mutex_unlock(&hdl_tbl_mutex);
	return NULL;
}

void *cam_get_device_ops(int32_t dev_hdl)
{
	int idx;
	int type;
	void *ops;

	mutex_lock(&hdl_tbl_mutex);
	if (!hdl_tbl) {
		pr_err("Hdl tbl is NULL\n");
		goto device_ops_fail;
	}

	idx = CAM_REQ_MGR_GET_HDL_IDX(dev_hdl);
	if (idx >= CAM_REQ_MGR_MAX_HANDLES) {
		pr_err("Invalid idx\n");
		goto device_ops_fail;
	}

	if (hdl_tbl->hdl[idx].state != HDL_ACTIVE) {
		pr_err("Invalid state\n");
		goto device_ops_fail;
	}

	type = CAM_REQ_MGR_GET_HDL_TYPE(dev_hdl);
	if (HDL_TYPE_DEV != type && HDL_TYPE_SESSION != type) {
		pr_err("Invalid type\n");
		goto device_ops_fail;
	}

	if (hdl_tbl->hdl[idx].hdl_value != dev_hdl) {
		pr_err("Invalid hdl\n");
		goto device_ops_fail;
	}

	ops = hdl_tbl->hdl[idx].ops;
	mutex_unlock(&hdl_tbl_mutex);

	return ops;

device_ops_fail:
	mutex_unlock(&hdl_tbl_mutex);
	return NULL;
}

static int cam_destroy_hdl(int32_t dev_hdl, int dev_hdl_type)
{
	int idx;
	int type;

	mutex_lock(&hdl_tbl_mutex);
	if (!hdl_tbl) {
		pr_err("Hdl tbl is NULL\n");
		goto destroy_hdl_fail;
	}

	idx = CAM_REQ_MGR_GET_HDL_IDX(dev_hdl);
	if (idx >= CAM_REQ_MGR_MAX_HANDLES) {
		pr_err("Invalid idx\n");
		goto destroy_hdl_fail;
	}

	if (hdl_tbl->hdl[idx].state != HDL_ACTIVE) {
		pr_err("Invalid state\n");
		goto destroy_hdl_fail;
	}

	type = CAM_REQ_MGR_GET_HDL_TYPE(dev_hdl);
	if (type != dev_hdl_type) {
		pr_err("Invalid type %d, %d\n", type, dev_hdl_type);
		goto destroy_hdl_fail;
	}

	if (hdl_tbl->hdl[idx].hdl_value != dev_hdl) {
		pr_err("Invalid hdl\n");
		goto destroy_hdl_fail;
	}

	hdl_tbl->hdl[idx].state = HDL_FREE;
	clear_bit(idx, hdl_tbl->bitmap);
	mutex_unlock(&hdl_tbl_mutex);

	return 0;

destroy_hdl_fail:
	mutex_unlock(&hdl_tbl_mutex);
	return -EINVAL;
}

int cam_destroy_device_hdl(int32_t dev_hdl)
{
	return cam_destroy_hdl(dev_hdl, HDL_TYPE_DEV);
}

int cam_destroy_session_hdl(int32_t dev_hdl)
{
	return cam_destroy_hdl(dev_hdl, HDL_TYPE_SESSION);
}
