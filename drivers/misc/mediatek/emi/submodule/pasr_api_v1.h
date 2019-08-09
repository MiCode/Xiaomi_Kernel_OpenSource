/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
