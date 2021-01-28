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

#ifndef __MT_EMI_MPU_H
#define __MT_EMI_MPU_H

#define EMI_MPU_START           (0x0)
#define EMI_MPU_END             (0x91C)

#define NO_PROTECTION   0
#define SEC_RW          1
#define SEC_RW_NSEC_R   2
#define SEC_RW_NSEC_W   3
#define SEC_R_NSEC_R    4
#define FORBIDDEN       5
#define SEC_R_NSEC_RW   6

#define EN_MPU_STR "ON"
#define DIS_MPU_STR "OFF"

#define EN_WP_STR "ON"
#define DIS_WP_STR "OFF"

#define MAX_CHANNELS  (1)
#define MAX_RANKS     (2)

/* Basic DRAM setting */
struct basic_dram_setting {
	/* Number of channels */
	unsigned channel_nr;
	/* Per-channel information */
	struct {
		/* Per-rank information */
		struct {
			/* Does this rank exist */
			bool valid_rank;
			/* Rank size - (in Gb)*/
			unsigned rank_size;
			/* Number of segments */
			unsigned segment_nr;
		} rank[MAX_RANKS];
	} channel[MAX_CHANNELS];
};

#define LOCK	1
#define UNLOCK	0

#define SET_ACCESS_PERMISSON(lock, d7, d6, d5, d4, d3, d2, d1, d0) \
((((unsigned int) d7) << 21) | (((unsigned int) d6) << 18) | (((unsigned int) d5) << 15) | \
(((unsigned int) d4)  << 12) | (((unsigned int) d3) <<  9) | (((unsigned int) d2) <<  6) | \
(((unsigned int) d1)  <<  3) | ((unsigned int) d0) | ((unsigned long long) lock << 26))

extern int emi_mpu_set_region_protection(unsigned long long start_addr,
unsigned long long end_addr, int region, unsigned int access_permission);
#if defined(CONFIG_MTKPASR)
extern void acquire_dram_setting(struct basic_dram_setting *pasrdpd);
#endif
extern void *mt_emi_reg_base_get(void);
extern int emi_mpu_get_violation_port(void);
extern unsigned int mt_emi_reg_read(unsigned int offset);
extern void mt_emi_reg_write(unsigned int data, unsigned int offset);
extern void dump_emi_latency(void);
#endif  /* !__MT_EMI_MPU_H */
