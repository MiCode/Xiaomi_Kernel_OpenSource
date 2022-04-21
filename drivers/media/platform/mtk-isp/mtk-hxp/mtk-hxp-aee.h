/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_HXP_AEE_H
#define MTK_HXP_AEE_H

#define HXP_AEE_MAX_BUFFER_SIZE		(16384)

// Forward declaration
struct mtk_hxp;

struct proc_info {
	uint8_t buffer[HXP_AEE_MAX_BUFFER_SIZE];
	size_t size;
	size_t count;
};

struct hxp_aee {
	struct proc_dir_entry *entry;
	struct proc_dir_entry *daemon;
	struct proc_dir_entry *kernel;
	struct proc_dir_entry *stream;
	struct proc_info data[3];
};

int hxp_aee_init(struct mtk_hxp *hxp_dev);

int hxp_aee_uninit(struct mtk_hxp *hxp_dev);

#endif  // MTK_HXP_AEE_H
