/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_SWPM_COMMON_KERNFS_H__
#define __MTK_SWPM_COMMON_KERNFS_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>

#include <linux/list.h>
#include <linux/kernfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>


int mtk_swpm_kernfs_create_group(struct kobject *kobj
						, struct attribute_group *grp);

int mtk_swpm_kernfs_create_file(struct kernfs_node *parent
		, const struct attribute *attr);

size_t get_mtk_swpm_kernfs_bufsz_max(void);

#endif
