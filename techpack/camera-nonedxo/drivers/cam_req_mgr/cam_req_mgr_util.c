// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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
#include "cam_debug_util.h"

static struct cam_req_mgr_util_hdl_tbl *hdl_tbl;
static DEFINE_SPINLOCK(hdl_tbl_lock);

int cam_req_mgr_util_init(void)
{
	int rc = 0;
	int bitmap_size;
	static struct cam_req_mgr_util_hdl_tbl *hdl_tbl_local;

	if (hdl_tbl) {
		rc = -EINVAL;
		CAM_ERR(CAM_CRM, "Hdl_tbl is already present");
		goto hdl_tbl_check_failed;
	}

	hdl_tbl_local = kzalloc(sizeof(*hdl_tbl), GFP_KERNEL);
	if (!hdl_tbl_local) {
		rc = -ENOMEM;
		goto hdl_tbl_alloc_failed;
	}
	spin_lock_bh(&hdl_tbl_lock);
	if (hdl_tbl) {
		spin_unlock_bh(&hdl_tbl_lock);
		rc = -EEXIST;
		kfree(hdl_tbl_local);
		goto hdl_tbl_check_failed;
	}
	hdl_tbl = hdl_tbl_local;
	spin_unlock_bh(&hdl_tbl_lock);

	bitmap_size = BITS_TO_LONGS(CAM_REQ_MGR_MAX_HANDLES_V2) * sizeof(long);
	hdl_tbl->bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	if (!hdl_tbl->bitmap) {
		rc = -ENOMEM;
		goto bitmap_alloc_fail;
	}
	hdl_tbl->bits = bitmap_size * BITS_PER_BYTE;

	return rc;

bitmap_alloc_fail:
	kfree(hdl_tbl);
	hdl_tbl = NULL;
hdl_tbl_alloc_failed:
hdl_tbl_check_failed:
	return rc;
}

int cam_req_mgr_util_deinit(void)
{
	spin_lock_bh(&hdl_tbl_lock);
	if (!hdl_tbl) {
		CAM_ERR(CAM_CRM, "Hdl tbl is NULL");
		spin_unlock_bh(&hdl_tbl_lock);
		return -EINVAL;
	}

	kfree(hdl_tbl->bitmap);
	hdl_tbl->bitmap = NULL;
	kfree(hdl_tbl);
	hdl_tbl = NULL;
	spin_unlock_bh(&hdl_tbl_lock);

	return 0;
}

int cam_req_mgr_util_free_hdls(void)
{
	int i = 0;

	spin_lock_bh(&hdl_tbl_lock);
	if (!hdl_tbl) {
		CAM_ERR(CAM_CRM, "Hdl tbl is NULL");
		spin_unlock_bh(&hdl_tbl_lock);
		return -EINVAL;
	}

	for (i = 0; i < CAM_REQ_MGR_MAX_HANDLES_V2; i++) {
		if (hdl_tbl->hdl[i].state == HDL_ACTIVE) {
			CAM_WARN(CAM_CRM, "Dev handle = %x session_handle = %x",
				hdl_tbl->hdl[i].hdl_value,
				hdl_tbl->hdl[i].session_hdl);
			hdl_tbl->hdl[i].state = HDL_FREE;
			clear_bit(i, hdl_tbl->bitmap);
		}
	}
	bitmap_zero(hdl_tbl->bitmap, CAM_REQ_MGR_MAX_HANDLES_V2);
	spin_unlock_bh(&hdl_tbl_lock);

	return 0;
}

static int32_t cam_get_free_handle_index(void)
{
	int idx;

	idx = find_first_zero_bit(hdl_tbl->bitmap, hdl_tbl->bits);

	if (idx >= CAM_REQ_MGR_MAX_HANDLES_V2 || idx < 0)
		return -ENOSR;

	set_bit(idx, hdl_tbl->bitmap);

	return idx;
}

int32_t cam_create_session_hdl(void *priv)
{
	int idx;
	int rand = 0;
	int32_t handle = 0;

	spin_lock_bh(&hdl_tbl_lock);
	if (!hdl_tbl) {
		CAM_ERR(CAM_CRM, "Hdl tbl is NULL");
		spin_unlock_bh(&hdl_tbl_lock);
		return -EINVAL;
	}

	idx = cam_get_free_handle_index();
	if (idx < 0) {
		CAM_ERR(CAM_CRM, "Unable to create session handle");
		spin_unlock_bh(&hdl_tbl_lock);
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
	spin_unlock_bh(&hdl_tbl_lock);

	return handle;
}

int32_t cam_create_device_hdl(struct cam_create_dev_hdl *hdl_data)
{
	int idx;
	int rand = 0;
	int32_t handle;

	spin_lock_bh(&hdl_tbl_lock);
	if (!hdl_tbl) {
		CAM_ERR(CAM_CRM, "Hdl tbl is NULL");
		spin_unlock_bh(&hdl_tbl_lock);
		return -EINVAL;
	}

	idx = cam_get_free_handle_index();
	if (idx < 0) {
		CAM_ERR(CAM_CRM, "Unable to create device handle");
		spin_unlock_bh(&hdl_tbl_lock);
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
	spin_unlock_bh(&hdl_tbl_lock);

	pr_debug("%s: handle = %x", __func__, handle);
	return handle;
}

void *cam_get_device_priv(int32_t dev_hdl)
{
	int idx;
	int type;
	void *priv;

	spin_lock_bh(&hdl_tbl_lock);
	if (!hdl_tbl) {
		CAM_ERR_RATE_LIMIT(CAM_CRM, "Hdl tbl is NULL");
		goto device_priv_fail;
	}

	idx = CAM_REQ_MGR_GET_HDL_IDX(dev_hdl);
	if (idx >= CAM_REQ_MGR_MAX_HANDLES_V2) {
		CAM_ERR_RATE_LIMIT(CAM_CRM, "Invalid idx");
		goto device_priv_fail;
	}

	if (hdl_tbl->hdl[idx].state != HDL_ACTIVE) {
		CAM_ERR_RATE_LIMIT(CAM_CRM, "Invalid state");
		goto device_priv_fail;
	}

	type = CAM_REQ_MGR_GET_HDL_TYPE(dev_hdl);
	if (HDL_TYPE_DEV != type && HDL_TYPE_SESSION != type) {
		CAM_ERR_RATE_LIMIT(CAM_CRM, "Invalid type");
		goto device_priv_fail;
	}

	if (hdl_tbl->hdl[idx].hdl_value != dev_hdl) {
		CAM_ERR_RATE_LIMIT(CAM_CRM, "Invalid hdl");
		goto device_priv_fail;
	}

	priv = hdl_tbl->hdl[idx].priv;
	spin_unlock_bh(&hdl_tbl_lock);

	return priv;

device_priv_fail:
	spin_unlock_bh(&hdl_tbl_lock);
	return NULL;
}

void *cam_get_device_ops(int32_t dev_hdl)
{
	int idx;
	int type;
	void *ops;

	spin_lock_bh(&hdl_tbl_lock);
	if (!hdl_tbl) {
		CAM_ERR(CAM_CRM, "Hdl tbl is NULL");
		goto device_ops_fail;
	}

	idx = CAM_REQ_MGR_GET_HDL_IDX(dev_hdl);
	if (idx >= CAM_REQ_MGR_MAX_HANDLES_V2) {
		CAM_ERR(CAM_CRM, "Invalid idx");
		goto device_ops_fail;
	}

	if (hdl_tbl->hdl[idx].state != HDL_ACTIVE) {
		CAM_ERR(CAM_CRM, "Invalid state");
		goto device_ops_fail;
	}

	type = CAM_REQ_MGR_GET_HDL_TYPE(dev_hdl);
	if (HDL_TYPE_DEV != type && HDL_TYPE_SESSION != type) {
		CAM_ERR(CAM_CRM, "Invalid type");
		goto device_ops_fail;
	}

	if (hdl_tbl->hdl[idx].hdl_value != dev_hdl) {
		CAM_ERR(CAM_CRM, "Invalid hdl");
		goto device_ops_fail;
	}

	ops = hdl_tbl->hdl[idx].ops;
	spin_unlock_bh(&hdl_tbl_lock);

	return ops;

device_ops_fail:
	spin_unlock_bh(&hdl_tbl_lock);
	return NULL;
}

static int cam_destroy_hdl(int32_t dev_hdl, int dev_hdl_type)
{
	int idx;
	int type;

	spin_lock_bh(&hdl_tbl_lock);
	if (!hdl_tbl) {
		CAM_ERR(CAM_CRM, "Hdl tbl is NULL");
		goto destroy_hdl_fail;
	}

	idx = CAM_REQ_MGR_GET_HDL_IDX(dev_hdl);
	if (idx >= CAM_REQ_MGR_MAX_HANDLES_V2) {
		CAM_ERR(CAM_CRM, "Invalid idx %d", idx);
		goto destroy_hdl_fail;
	}

	if (hdl_tbl->hdl[idx].state != HDL_ACTIVE) {
		CAM_ERR(CAM_CRM, "Invalid state");
		goto destroy_hdl_fail;
	}

	type = CAM_REQ_MGR_GET_HDL_TYPE(dev_hdl);
	if (type != dev_hdl_type) {
		CAM_ERR(CAM_CRM, "Invalid type %d, %d", type, dev_hdl_type);
		goto destroy_hdl_fail;
	}

	if (hdl_tbl->hdl[idx].hdl_value != dev_hdl) {
		CAM_ERR(CAM_CRM, "Invalid hdl");
		goto destroy_hdl_fail;
	}

	hdl_tbl->hdl[idx].state = HDL_FREE;
	hdl_tbl->hdl[idx].ops   = NULL;
	hdl_tbl->hdl[idx].priv  = NULL;
	clear_bit(idx, hdl_tbl->bitmap);
	spin_unlock_bh(&hdl_tbl_lock);

	return 0;

destroy_hdl_fail:
	spin_unlock_bh(&hdl_tbl_lock);
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
