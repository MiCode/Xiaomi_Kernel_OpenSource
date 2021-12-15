/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#define MTK_LP_KERNFS_IDIOTYPE	(1<<0UL)

int mtk_lp_kernfs_create_group(struct kobject *kobj,
				      struct attribute_group *grp);

int mtk_lp_kernfs_create_file(struct kernfs_node *parent,
				  struct kernfs_node **node,
				  unsigned int flag,
				  const char *name, umode_t mode,
				  void *attr);

int mtk_lp_kernfs_remove_file(struct kernfs_node *node);

size_t get_mtk_lp_kernfs_bufsz_max(void);

#endif
