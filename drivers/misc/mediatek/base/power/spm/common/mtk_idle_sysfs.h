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

#ifndef __MTK_IDLE_SYSFS__
#define __MTK_IDLE_SYSFS__

typedef ssize_t (*f_mtk_idle_sysfs_show)(char *ToUserBuf
			, size_t sz, void *priv);
typedef ssize_t (*f_mtk_idle_sysfs_write)(char *FromUserBuf
			, size_t sz, void *priv);

struct mtk_idle_sysfs_op {
	f_mtk_idle_sysfs_show	fs_read;
	f_mtk_idle_sysfs_write	fs_write;
	void *priv;
};

struct mtk_idle_sysfs_handle {
	void *_current;
};

/*Creat the root entry for mtk idle systme fs*/
int mtk_idle_sysfs_entry_create(void);

/*Add the child file node to mtk idle system*/
int mtk_idle_sysfs_entry_node_add(const char *name, int mode
			, const struct mtk_idle_sysfs_op *op
			, struct mtk_idle_sysfs_handle *node);

/*Get the mtk idle system fs root entry handle*/
int mtk_idle_sysfs_entry_root_get(struct mtk_idle_sysfs_handle **handle);

int mtk_idle_sysfs_entry_func_create(const char *name, int mode
			, struct mtk_idle_sysfs_handle *parent
			, struct mtk_idle_sysfs_handle *handle);

int mtk_idle_sysfs_entry_func_node_add(const char *name
		, int mode, const struct mtk_idle_sysfs_op *op
		, struct mtk_idle_sysfs_handle *parent
		, struct mtk_idle_sysfs_handle *node);
#endif
