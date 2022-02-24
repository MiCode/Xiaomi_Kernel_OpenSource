// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_QOS_SHARE_H__
#define __MTK_QOS_SHARE_H__

#define HIST_NUM 8
#define BW_TYPE  4

struct qos_rec_data {
	/* 32 bytes */
	unsigned int rec_version;
	unsigned int reserved[7];

	/* 4 + (8 * 4  * 4) * 2 = 260 bytes */
	unsigned int current_hist;
	unsigned int bw_hist[HIST_NUM][BW_TYPE];
	unsigned int data_bw_hist[HIST_NUM][BW_TYPE];

	/* remaining size = 3804 bytes */
};

extern int qos_init_rec_share(void);
extern unsigned int qos_rec_get_hist_bw(unsigned int idx,
										unsigned int type);
extern unsigned int qos_rec_get_hist_data_bw(unsigned int idx,
										unsigned int type);
extern unsigned int qos_rec_get_hist_idx(void);
#endif
