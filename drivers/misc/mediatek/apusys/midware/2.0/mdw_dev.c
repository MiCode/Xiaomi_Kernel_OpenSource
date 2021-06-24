// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/slab.h>

#include "mdw_cmn.h"
#include "mdw_export.h"

int mdw_dev_init(struct mdw_device *mdev)
{
	int ret = 0;

	mdw_drv_info("mdw dev init type(%d-%u)\n",
		mdev->driver_type, mdev->version);

	switch (mdev->driver_type) {
	case MDW_DRIVER_TYPE_PLATFORM:
		mdw_ap_set_func(mdev);
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

int mdw_dev_set_param(struct mdw_device *mdev, uint32_t idx, uint32_t val)
{
	if (!mdev->dev_funcs || idx >= MDW_PARAM_MAX) {
		mdw_drv_err("no dev func\n");
		return -ENODEV;
	}

	return mdev->dev_funcs->set_param(idx, val);
}

uint32_t mdw_dev_get_param(struct mdw_device *mdev, uint32_t idx)
{
	if (!mdev->dev_funcs || idx >= MDW_PARAM_MAX) {
		mdw_drv_err("no dev func\n");
		return 0;
	}

	return mdev->dev_funcs->get_param(idx);
}

int mdw_dev_lock(void)
{
	if (!mdw_dev)
		return mdw_dev->dev_funcs->lock();

	return -ENODEV;
}

int mdw_dev_unlock(void)
{
	if (!mdw_dev)
		return mdw_dev->dev_funcs->unlock();

	return -ENODEV;
}
