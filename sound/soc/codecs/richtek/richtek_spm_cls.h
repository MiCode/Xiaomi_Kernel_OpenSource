/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __LINUX_RICHTEK_BIGDATA_CLASS_H__
#define __LINUX_RICHTEK_BIGDATA_CLASS_H__

#include <linux/completion.h>
#include <linux/mutex.h>

#define RICHTEK_SPM_MAX_PARAM_ITEMS	(30)
#define RICHTEK_SPM_MAGIC	(5526789)

struct richtek_spm_classdev;

struct richtek_spm_device_ops {
	int (*suspend)(struct richtek_spm_classdev *ptc);
	int (*resume)(struct richtek_spm_classdev *ptc);
	int (*pre_calib)(struct richtek_spm_classdev *ptc);
	int (*post_calib)(struct richtek_spm_classdev *ptc);
	int (*pre_vvalid)(struct richtek_spm_classdev *ptc);
	int (*post_vvalid)(struct richtek_spm_classdev *ptc);
};

struct richtek_spm_classdev {
	const char *name;
	struct device *dev;
	const struct richtek_spm_device_ops *ops;
	const struct attribute_group **groups;
	struct mutex var_lock;
	uint8_t id;
	/* internal use for algorithm */
	s32 rspk;
	s32 pcb_trace;
	s32 pwr;
	u32 spkidx;
	s32 tmax;
	s32 tmaxcnt;
	s32 xmax;
	s32 xmaxcnt;
	s32 boot_on_xmax;
	s32 boot_on_tmax;
	u32 max_pwr;
	u32 min_pwr;
	u8 calibrated:1;
	u8 calib_running:1;
};

/* API List */
extern int devm_richtek_spm_classdev_register(struct device *parent,
	struct richtek_spm_classdev *rdc);
extern int richtek_spm_classdev_register(struct device *parent,
	struct richtek_spm_classdev *rdc);
extern void richtek_spm_classdev_unregister(
	struct richtek_spm_classdev *rdc);

extern int richtek_spm_classdev_trigger_ampoff(
	struct richtek_spm_classdev *rdc);

#endif /* __LINUX_RICHTEK_BIGDATA_CLASS_H__ */
