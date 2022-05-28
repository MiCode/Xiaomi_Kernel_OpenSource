/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_AOV_DRV_H
#define MTK_AOV_DRV_H

#include <linux/cdev.h>
#include <linux/platform_device.h>

#include "mtk-aov-config.h"
#include "mtk-aov-core.h"
#include "mtk-aov-aee.h"

/**
 * struct mtk_aov - aov driver data
 * @aov_devno:           The aov_devno for aov init aov character device
 * @dev:                 aov struct device
 * @aov_cdev:            The point of aov character device.
 * @aov_class:           The class_create for create aov device
 * @aov_device:          aov struct device
 * @is_open:             the flag to indicate if aov device is open.
 * @op_mode:             operation in bypass or aov mode
 */
struct mtk_aov {
	struct aov_core core_info;
	struct aov_aee aee_info;

	dev_t aov_devno;
	struct device *dev;
	struct cdev aov_cdev;
	struct class *aov_class;
	struct device *aov_device;
	bool is_open;
	uint32_t op_mode;
};

#endif /* MTK_AOV_DRV_H */
