// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/slab.h>

#include "mdw_cmn.h"


static void mdw_dev_clear_cmd_func(struct work_struct *wk)
{
	struct mdw_device *mdev =
		container_of(wk, struct mdw_device, c_wk);
	struct mdw_cmd *c = NULL, *tmp = NULL;
	struct mdw_fpriv *mpriv = NULL;

	mutex_lock(&mdev->c_mtx);
	list_for_each_entry_safe(c, tmp, &mdev->d_cmds, d_node) {
		list_del(&c->d_node);
		mpriv = c->mpriv;
		mdw_cmd_delete(c);
	}
	mutex_unlock(&mdev->c_mtx);
}

int mdw_dev_init(struct mdw_device *mdev)
{
	int ret = -ENODEV;

	mdw_drv_info("mdw dev init type(%d-%u)\n",
		mdev->driver_type, mdev->mdw_ver);

	INIT_LIST_HEAD(&mdev->d_cmds);
	mutex_init(&mdev->c_mtx);
	INIT_WORK(&mdev->c_wk, &mdw_dev_clear_cmd_func);

	switch (mdev->driver_type) {
	case MDW_DRIVER_TYPE_PLATFORM:
		mdw_drv_err("not support platform probe\n");
		break;
	case MDW_DRIVER_TYPE_RPMSG:
		mdw_rv_set_func(mdev);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (mdev->dev_funcs)
		ret = mdev->dev_funcs->late_init(mdev);

	return ret;
}

void mdw_dev_deinit(struct mdw_device *mdev)
{
	if (mdev->dev_funcs) {
		mdev->dev_funcs->late_deinit(mdev);
		mdev->dev_funcs = NULL;
	}
}

void mdw_dev_session_create(struct mdw_fpriv *mpriv)
{
	struct mdw_device *mdev = mpriv->mdev;
	uint32_t i = 0;

	for (i = 0; i < MDW_DEV_MAX; i++) {
		if (mdev->adevs[i])
			mdev->adevs[i]->send_cmd(APUSYS_CMD_SESSION_CREATE, mpriv, mdev->adevs[i]);
	}
}

void mdw_dev_session_delete(struct mdw_fpriv *mpriv)
{
	struct mdw_device *mdev = mpriv->mdev;
	uint32_t i = 0;

	for (i = 0; i < MDW_DEV_MAX; i++) {
		if (mdev->adevs[i])
			mdev->adevs[i]->send_cmd(APUSYS_CMD_SESSION_DELETE, mpriv, mdev->adevs[i]);
	}
}

int mdw_dev_validation(struct mdw_fpriv *mpriv, uint32_t dtype,
	struct mdw_cmd *cmd, struct apusys_cmdbuf *cbs, uint32_t num)
{
	struct apusys_device *adev = NULL;
	struct apusys_cmd_valid_handle v_hnd;
	int ret = 0;

	if (dtype >= MDW_DEV_MAX)
		return -ENODEV;

	memset(&v_hnd, 0, sizeof(v_hnd));
	v_hnd.cmdbufs = cbs;
	v_hnd.num_cmdbufs = num;
	v_hnd.session = mpriv;
	v_hnd.cmd = cmd;
	adev = mpriv->mdev->adevs[dtype];
	if (adev)
		ret = adev->send_cmd(APUSYS_CMD_VALIDATE, &v_hnd, adev);

	return ret;
}

int apusys_register_device(struct apusys_device *adev)
{
	uint32_t type = 0;

	if (!mdw_dev) {
		pr_info("[apusys] apu mdw not ready\n");
		return -ENODEV;
	}

	if (!mdw_dev->dev_funcs) {
		pr_info("[apusys] apu mdw not inited\n");
		return -ENODEV;
	}

	type = adev->dev_type;
	if (type >= MDW_DEV_MAX) {
		pr_info("[apusys] invalid dev(%u)\n", type);
		return -EINVAL;
	}

	if (!mdw_dev->adevs[type])
		mdw_dev->adevs[type] = adev;

	return mdw_dev->dev_funcs->register_device(adev);
}

int apusys_unregister_device(struct apusys_device *adev)
{
	if (!mdw_dev) {
		pr_info("[apusys] apu mdw not ready\n");
		return -ENODEV;
	}

	if (!mdw_dev->dev_funcs) {
		pr_info("[apusys] apu mdw not inited\n");
		return -ENODEV;
	}

	return mdw_dev->dev_funcs->unregister_device(adev);
}
