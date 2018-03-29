/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef _MTKPASR_DRV_H_
#define _MTKPASR_DRV_H_

#define IN_RANGE(s, e, rs, re)	(s >= rs && e <= re)

/* Print wrapper */
#define MTKPASR_PRINT(args...)	do {} while (0) /* pr_alert(args) */

/*-- Data structures */

/* Bank information (1 PASR unit) */
struct mtkpasr_bank {
	unsigned long start_pfn;	/* The 1st pfn */
	unsigned long end_pfn;		/* The pfn after the last valid one */
	unsigned long free;		/* The number of free pages */
	int segment;			/* Corresponding to which segment */
	int rank;			/* Associated rank */
};

/* PASR masked with channel information */
struct pasrvec {
	unsigned long pasr_on;		/* LSB stands for the segment with the smallest order */
	int channel;			/* which channel */
};

/* #define DEBUG_FOR_CHANNEL_SWITCH */

/* Query bitmask of DRAM segments in a channel */
extern unsigned long query_channel_segment_bits(void);

/* MTKPASR internal functions */
extern int __init mtkpasr_init_range(unsigned long start_pfn, unsigned long end_pfn, unsigned long *bank_pfns);

/* Give bank, this function will return its (start_pfn, end_pfn) and corresponding rank */
extern int __init query_bank_rank_information(int bank, unsigned long *spfn, unsigned long *epfn, int *segn);

/* Query the number of channel */
#ifdef DEBUG_FOR_CHANNEL_SWITCH
extern unsigned int get_channel_num(void);
#else
extern unsigned int __init get_channel_num(void);
#endif

/* Query PASR masked with specified channel configuration */
#define USE_ORIG_CHCONFIG	(0xFFFFFFFF)
extern int fill_pasr_on_by_chconfig(unsigned int chconfig, struct pasrvec *pasrvec, unsigned long opon);

#endif
