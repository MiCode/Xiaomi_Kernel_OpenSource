/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MTK_LP_KERNFS_H__
#define __MTK_LP_KERNFS_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>

#include <linux/list.h>
#include <linux/kernfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>


int mtk_lp_kernfs_create_group(struct kobject *kobj
						, struct attribute_group *grp);

int mtk_lp_kernfs_create_file(struct kernfs_node *parent
		, const struct attribute *attr);

size_t get_mtk_lp_kernfs_bufsz_max(void);

#endif
