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
