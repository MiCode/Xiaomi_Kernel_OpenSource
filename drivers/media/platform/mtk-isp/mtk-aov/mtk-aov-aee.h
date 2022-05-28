/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_AOV_AEE_H
#define MTK_AOV_AEE_H

#define AOV_AEE_MAX_BUFFER_SIZE		(16384)

// Forward declaration
struct mtk_aov;

struct proc_info {
	uint8_t buffer[AOV_AEE_MAX_BUFFER_SIZE];
	size_t size;
	size_t count;
};

struct aov_aee {
	struct proc_dir_entry *entry;
	struct proc_dir_entry *daemon;
	struct proc_dir_entry *kernel;
	struct proc_dir_entry *stream;
	struct proc_info data[3];
};

int aov_aee_init(struct mtk_aov *aov_dev);

int aov_aee_uninit(struct mtk_aov *aov_dev);

#endif  // MTK_AOV_AEE_H
