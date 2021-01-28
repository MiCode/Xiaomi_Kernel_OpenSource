/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Sagy Shih <sagy.shih@mediatek.com>
 */

#ifndef __PASR_API_H__
#define __PASR_API_H__

#define MAX_RANKS	MAX_RK

struct basic_dram_setting {
	unsigned int channel_nr;
	/* per-channel information */
	struct {
		/* per-rank information */
		struct {
			bool valid_rank;
			unsigned int rank_size; /* unit: 1 Gb*/
			unsigned int segment_nr;
		} rank[MAX_RK];
	} channel[MAX_CH];
};

void acquire_dram_setting(struct basic_dram_setting *pasrdpd);

#endif /* __PASR_API_H__ */
