/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_HCP_AEE_H
#define MTK_HCP_AEE_H

#include <linux/mutex.h>

#define HCP_AEE_MAX_BUFFER_SIZE (512*1024)  // 512KB

// Forward declaration
struct mtk_hcp;

enum HCP_AEE_DB_FILE {
	HCP_AEE_PROC_FILE_DAEMON = 0,
	HCP_AEE_PROC_FILE_KERNEL = 1,
	HCP_AEE_PROC_FILE_STREAM = 2,
	HCP_AEE_PROC_FILE_NUM    = 3,
};

struct hcp_proc_data {
	struct mutex mtx;
	size_t sz;
	size_t cnt;
	uint8_t buf[HCP_AEE_MAX_BUFFER_SIZE];
};

struct hcp_aee_info {
	struct proc_dir_entry *entry;
	struct proc_dir_entry *daemon;
	struct proc_dir_entry *kernel;
	struct proc_dir_entry *stream;
	struct hcp_proc_data data[HCP_AEE_PROC_FILE_NUM];
};

int hcp_aee_init(struct mtk_hcp *hcp_dev);

int hcp_aee_uninit(struct mtk_hcp *hcp_dev);

#endif  // MTK_HCP_AEE_H
