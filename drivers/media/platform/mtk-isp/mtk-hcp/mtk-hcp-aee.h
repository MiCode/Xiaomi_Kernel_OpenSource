/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_HCP_AEE_H
#define MTK_HCP_AEE_H

#define HCP_AEE_PROC_FILE_NUM (3)
#define HCP_AEE_MAX_BUFFER_SIZE (512*1024)  // 512KB

// Forward declaration
struct mtk_hcp;

struct proc_info {
	uint8_t buffer[HCP_AEE_MAX_BUFFER_SIZE];
	size_t size;
	size_t count;
};

struct hcp_aee {
	struct proc_dir_entry *entry;
	struct proc_dir_entry *daemon;
	struct proc_dir_entry *kernel;
	struct proc_dir_entry *stream;
	struct proc_info data[HCP_AEE_PROC_FILE_NUM];
};

int hcp_aee_init(struct mtk_hcp *hcp_dev);

int hcp_aee_uninit(struct mtk_hcp *hcp_dev);

#endif  // MTK_HCP_AEE_H
