/* SPDX-License-Identifier: GPL-2.0 */
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
#define QOS_SHARE_REC_VER               0x0
#define QOS_SHARE_CURR_IDX              0x20
#define QOS_SHARE_HIST_BW               0x24
#define QOS_SHARE_HIST_DATA_BW          0xA4

extern int qos_share_init_sram(void __iomem *regs, unsigned int bound);
extern u32 qos_share_sram_read(u32 id);
#endif
