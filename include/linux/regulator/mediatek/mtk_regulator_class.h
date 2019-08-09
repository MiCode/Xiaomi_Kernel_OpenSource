/*
 * mtk_regulator_class.h -- MTK Regulator driver support.
 * Copyright (C) 2017 MediaTek Inc.
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#ifndef __LINUX_REGULATOR_MTK_REGULATOR_CLASS_H_
#define __LINUX_REGULATOR_MTK_REGULATOR_CLASS_H_

#include <linux/kernel.h>
#include <linux/device.h>

struct mtk_simple_regulator_device {
	struct device dev;
};

enum mtk_simple_regulator_property {
	MREG_PROP_SET_RAMP_DELAY = 0,
};

union mtk_simple_regulator_propval {
	int32_t intval;
	uint32_t uintval;
	const char *strval;
};

extern struct mtk_simple_regulator_device *
mtk_simple_regulator_device_register(const char *name, struct device *parent,
	void *drvdata);

extern void mtk_simple_regulator_device_unregister(
	struct mtk_simple_regulator_device *mreg_dev);

extern struct mtk_simple_regulator_device *mtk_simple_regulator_get_dev_by_name(
	const char *name);

#define to_mreg_device(obj) \
	container_of(obj, struct mtk_simple_regulator_device, dev)

#endif /* __LINUX_REGULATOR_MTK_REGULATOR_CLASS_H_ */
