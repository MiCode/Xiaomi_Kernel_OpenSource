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

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/memory.h>
#include <linux/memblock.h>
#include <linux/printk.h>
#include <linux/sched.h>
#ifdef CONFIG_MTK_EMI
#include <mt_emi_api.h>
#else
#include <mach/emi_mpu.h>
#endif
#include <mt-plat/mtk_lpae.h>
#include "mtkpasr_drv.h"

/* Struct for parsing rank information (SW view) */
struct view_rank {
	/* The 1st pfn (kernel pfn) */
	unsigned long start_pfn;
	/* The pfn after the last valid one (kernel pfn) */
	unsigned long end_pfn;
	/* Bank size in PFN (should be power of 2) */
	unsigned long bank_pfn_size;
	/*
	 * [0..15]: the number of segments
	 * [16..BITS_PER_LONG-1]: channel configuration
	 * Ex. 0x00030008 means there are 8 segments,
	 *     2 valid channels, 1st & 2nd.
	 */
	unsigned long channel_segments;
};

#define RANK_START_CHANNEL	(0x10000)
#define RANK_SEGMENTS(rank)	\
	(((struct view_rank *)rank)->channel_segments & 0xFFFF)
#define SEGMENTS_PER_RANK	(8)
#define SEGMENT_TO_RANK(seg)	(seg / SEGMENTS_PER_RANK)

static struct view_rank rank_info[MAX_RANKS];

/* Basic DRAM configuration */
static struct basic_dram_setting pasrdpd;

/* MTKPASR control variables */
static unsigned int channel_count;
static unsigned long max_segment_pfns;		/* Max segment in size PFNs */
static unsigned long max_segment_base;
/* Effective segment number per channel (related to max_segment_base) */
static unsigned long channel_segnum;
static unsigned int rank_count;
/* 0x0000AABB. BB for rank0, AA for rank1. */
static unsigned long mtkpasr_segment_bits;
/* Bitmask of DRAM segments in a channel */
static unsigned long fullch_segment_bitmask;
/* Total DRAM size in PFN */
static unsigned long total_pfns;

/* Query bitmask of DRAM segments in a channel */
unsigned long query_channel_segment_bits(void)
{
	return fullch_segment_bitmask;
}

/* Virtual <-> Kernel PFN helpers */
#define MAX_RANK_PFN	(0x1FFFFF)
#define MAX_KERNEL_PFN	(0x13FFFF)
#define MAX_KPFN_MASK	(0x0FFFFF)
#define KPFN_TO_VIRT	(0x100000)
static unsigned long __init virt_to_kernel_pfn(unsigned long virt)
{
	unsigned long ret = virt;

	if (enable_4G()) {
		if (virt > MAX_RANK_PFN)
			ret = virt - KPFN_TO_VIRT;
		else if (virt > MAX_KERNEL_PFN)
			ret = virt & MAX_KPFN_MASK;
	}

	return ret;
}
static unsigned long __init kernel_pfn_to_virt(unsigned long kpfn)
{
	unsigned long ret = kpfn;

	if (enable_4G())
		ret = kpfn | KPFN_TO_VIRT;

	return ret;
}
static unsigned long __init rank_pfn_offset(void)
{
	unsigned long ret = ARCH_PFN_OFFSET;

	if (enable_4G())
		ret = KPFN_TO_VIRT;

	return ret;
}

/*
 * Check DRAM configuration -
 * transform DRAM setting to temporary bank structure.
 * Return 0 on success, -1 on error.
 */
static int __init check_dram_configuration(void)
{
#define RANK_SIZE(chan, rank)	pasrdpd.channel[chan].rank[rank].rank_size
#define SEGMENT_NR(chan, rank)	pasrdpd.channel[chan].rank[rank].segment_nr

	int chan, rank, check_segment_num;
	unsigned long channel_segments;
	unsigned long check_rank_size, rank_pfn, start_pfn = rank_pfn_offset();

	/* Acquire basic DRAM setting */
	acquire_dram_setting(&pasrdpd);

	/* Parse DRAM setting */
	channel_count = pasrdpd.channel_nr;
	for (rank = 0; rank < MAX_RANKS; ++rank) {
		rank_pfn = 0;
		rank_info[rank].channel_segments = 0x0;
		channel_segments = RANK_START_CHANNEL;
		check_rank_size = 0x0;
		check_segment_num = 0x0;
		for (chan = 0; chan < channel_count; ++chan) {
			if (pasrdpd.channel[chan].rank[rank].valid_rank) {
				/* # Gb -> # pages */
				rank_pfn += (RANK_SIZE(chan, rank)
						<< (27 - PAGE_SHIFT));
				rank_info[rank].channel_segments |=
					channel_segments;
				/* Sanity check for rank size */
				if (!check_rank_size) {
					check_rank_size = RANK_SIZE(chan, rank);
				} else {
					/*
					 * We only support ranks with equal size
					 */
					if (check_rank_size !=
							RANK_SIZE(chan, rank))
						return -1;
				}
				/* Sanity check for segment number */
				if (!check_segment_num) {
					check_segment_num =
						SEGMENT_NR(chan, rank);
				} else {
					/*
					 * We only support ranks with equal
					 * segment number.
					 */
					if (check_segment_num !=
							SEGMENT_NR(chan, rank))
						return -1;
				}
			}
			channel_segments <<= 1;
		}

		/* Have we found a valid rank */
		if (check_rank_size != 0 && check_segment_num != 0) {
			rank_info[rank].start_pfn =
				virt_to_kernel_pfn(start_pfn);
			rank_info[rank].end_pfn =
				virt_to_kernel_pfn(start_pfn + rank_pfn);
			rank_info[rank].bank_pfn_size =
				rank_pfn/check_segment_num;
			rank_info[rank].channel_segments +=
				check_segment_num;
			start_pfn = kernel_pfn_to_virt(rank_info[rank].end_pfn);
			MTKPASR_PRINT(
			"Rank[%d] start_pfn[%8lu] end_pfn[%8lu] bank_pfn_size[%8lu] channel_segments[0x%lx]\n",
			rank, rank_info[rank].start_pfn,
			rank_info[rank].end_pfn, rank_info[rank].bank_pfn_size,
			rank_info[rank].channel_segments);
			rank_count++;
		} else {
			rank_info[rank].start_pfn =
				virt_to_kernel_pfn(rank_pfn_offset());
			rank_info[rank].end_pfn =
				virt_to_kernel_pfn(rank_pfn_offset());
			rank_info[rank].bank_pfn_size = 0;
			rank_info[rank].channel_segments = 0x0;
		}

		/* Update total_pfns */
		total_pfns += rank_pfn;

		/* Update DRAM segment bitmask of a channel */
		fullch_segment_bitmask =
			(fullch_segment_bitmask << SEGMENTS_PER_RANK) |
			((1 << check_segment_num) - 1);
	}

	return 0;

#undef RANK_SIZE
#undef SEGMENT_NR
}

/*
 * Check whether it is a valid rank
 */
static bool is_valid_rank(int rank)
{
	/* Check start/end pfn */
	if (rank_info[rank].start_pfn == rank_info[rank].end_pfn)
		return false;

	/* Check channel_segments */
	if (rank_info[rank].channel_segments == 0x0)
		return false;

	return true;
}

/* Compute effective segment base */
static void __init init_rank_effect_segment_base(void)
{
	int rank;
	unsigned long segment_base;

	/* Query max segment size */
	for (rank = 0; rank < MAX_RANKS; ++rank) {
		if (!is_valid_rank(rank))
			continue;
		if (max_segment_pfns < rank_info[rank].bank_pfn_size)
			max_segment_pfns = rank_info[rank].bank_pfn_size;
	}

	/* Query effective segment base */
	for (rank = 0; rank < MAX_RANKS; ++rank) {
		if (!is_valid_rank(rank))
			continue;
		segment_base = max_segment_pfns / rank_info[rank].bank_pfn_size;
		if (max_segment_base < segment_base)
			max_segment_base = segment_base;
	}

	/* Set effective segment number per channel */
	for (rank = 0; rank < MAX_RANKS; ++rank) {
		if (!is_valid_rank(rank))
			continue;
		segment_base = max_segment_pfns / rank_info[rank].bank_pfn_size;
		channel_segnum += SEGMENTS_PER_RANK * max_segment_base /
			segment_base;
	}
}

/*
 * Fill mtkpasr_segment_bits
 */
static void __init find_mtkpasr_valid_segment(unsigned long *start,
		unsigned long *end)
{
	int num_segment, rank;
	unsigned long spfn = 0, epfn = 0, max_start, min_end;
	unsigned long rspfn, repfn;
	unsigned long bank_pfn_size;
	bool virtual;

	/* Sanity check */
	if (*start == *end)
		return;

	num_segment = 0;
	max_start = *start;
	min_end = *end;
	for (rank = 0; rank < MAX_RANKS;
			++rank, num_segment += SEGMENTS_PER_RANK) {

		/* Is it a valid rank */
		if (!is_valid_rank(rank))
			continue;

		/* At least 1 bank size  */
		bank_pfn_size = rank_info[rank].bank_pfn_size;
		if ((*start + bank_pfn_size) > *end)
			continue;

		/*
		 * If rank's start_pfn > rank's end_pfn,
		 * then compare them in virtual
		 */
		if (rank_info[rank].start_pfn > rank_info[rank].end_pfn) {
			spfn = kernel_pfn_to_virt(*start);
			epfn = kernel_pfn_to_virt(*end);
			rspfn = kernel_pfn_to_virt(rank_info[rank].start_pfn);
			repfn = kernel_pfn_to_virt(rank_info[rank].end_pfn);
			virtual = true;
		} else {
			spfn = *start;
			epfn = *end;
			rspfn = rank_info[rank].start_pfn;
			repfn = rank_info[rank].end_pfn;
			virtual = false;
		}

		/* Update mtkpasr range */
		spfn = round_up(spfn, bank_pfn_size);
		epfn = round_down(epfn, bank_pfn_size);
		if (virtual) {
			max_start = max(virt_to_kernel_pfn(spfn), max_start);
			min_end = min(virt_to_kernel_pfn(epfn), min_end);
		} else {
			max_start = max(spfn, max_start);
			min_end = min(epfn, min_end);
		}

		/* Check overlapping */
		if (epfn <= spfn) {
			/* spfn ~ repfn */
			while (repfn >= (spfn + bank_pfn_size)) {
				mtkpasr_segment_bits |= (1 <<
						((spfn - rspfn) / bank_pfn_size
						 + num_segment));
				spfn += bank_pfn_size;
			}
			/* rspfn ~ epfn */
			while (epfn >= (rspfn + bank_pfn_size)) {
				epfn -= bank_pfn_size;
				mtkpasr_segment_bits |= (1 <<
						((epfn - rspfn) / bank_pfn_size
						 + num_segment));
			}
		} else {
			/* spfn ~ epfn */
			spfn = max(spfn, rspfn);
			epfn = min(epfn, repfn);
			while (epfn >= (spfn + bank_pfn_size)) {
				mtkpasr_segment_bits |= (1 <<
						((spfn - rspfn) / bank_pfn_size
						 + num_segment));
				spfn += bank_pfn_size;
			}
		}
	}

	/* Feedback PASR-masked range */
	if ((spfn == 0) && (epfn == 0)) {
		*start = max_start;
		*end = max_start;
	} else {
		*start = max_start;
		*end = min_end;
	}
}

/*
 * We will set an offset on which active PASR will be imposed.
 * This is done by acquiring CMA's base and size
 * (Both start_pfn, end_pfn are kernel pfns)
 * Return <0 means "fail to init pasr range"
 *        >0 means "the number of valid banks"
 */
int __init mtkpasr_init_range(unsigned long start_pfn, unsigned long end_pfn,
		unsigned long *bank_pfns)
{
	int ret = -99;
	unsigned long vseg, seg_num = 0;

	/* Invalid paramenters */
	if (start_pfn >= end_pfn)
		goto out;

	/* Check DRAM configuration */
	ret = check_dram_configuration();
	if (ret < 0)
		goto out;

	/* Initialize effective segment base */
	init_rank_effect_segment_base();

	MTKPASR_PRINT("start_pfn[%8lu] end_pfn[%8lu]\n", start_pfn, end_pfn);

	/* Map PASR start/end kernel pfn to DRAM segments */
	find_mtkpasr_valid_segment(&start_pfn, &end_pfn);

	/* How many segments */
	vseg = mtkpasr_segment_bits;
	ret = 0;
	do {
		if (vseg & 0x1)
			ret++;
		vseg >>= 1;
	} while (++seg_num < BITS_PER_LONG);

	MTKPASR_PRINT(
	"Start_pfn[%8lu] End_pfn[%8lu] Valid_segment[0x%8lx] Segments[%u]\n",
	start_pfn, end_pfn, mtkpasr_segment_bits, ret);

	/* Return max pfns per bank */
	*bank_pfns =
		max(rank_info[0].bank_pfn_size, rank_info[1].bank_pfn_size);
out:
	return ret;
}

/*
 * Give bank, this function will return its (start_pfn, end_pfn)
 * and corresponding rank.
 * Return -1 means no valid banks, ranks
 *        0  means no corresponding rank
 *        >0 means there are corresponding bank, rank
 *        (Caller should subtract 1 to get the correct rank number)
 */
int __init query_bank_rank_information(int bank, unsigned long *spfn,
		unsigned long *epfn, int *segn)
{
	int seg_num = 0, rank, num_segment = 0;
	unsigned long vseg = mtkpasr_segment_bits, vmask;
	bool virtual;

	/* Reset */
	*spfn = 0;
	*epfn = 0;

	/* Which segment */
	do {
		if (vseg & 0x1) {
			/* Found! */
			if (!bank)
				break;
			bank--;
		}
		vseg >>= 1;
		seg_num++;
	} while (seg_num < BITS_PER_LONG);

	/* Sanity check */
	if (seg_num == BITS_PER_LONG)
		return -1;

	/* Corresponding segment */
	*segn = seg_num;

	/* Which rank */
	vseg = mtkpasr_segment_bits;
	for (rank = 0; rank < MAX_RANKS; ++rank) {
		if (is_valid_rank(rank)) {
			if (rank_info[rank].start_pfn > rank_info[rank].end_pfn)
				virtual = true;
			else
				virtual = false;
			num_segment = RANK_SEGMENTS(&rank_info[rank]);
			if (seg_num < num_segment) {
				*spfn = rank_info[rank].start_pfn +
					seg_num * rank_info[rank].bank_pfn_size;
				*epfn = *spfn + rank_info[rank].bank_pfn_size;
				break;
			}
			/* Next rank */
			seg_num -= SEGMENTS_PER_RANK;
			vseg >>= SEGMENTS_PER_RANK;
		}
	}

	/* epfn should be larger than spfn */
	if (*epfn <= *spfn)
		return -2;

	/* Sanity check */
	if (rank == MAX_RANKS)
		return -3;

	/* Convert to actual kernel pfn */
	if (virtual) {
		*spfn = virt_to_kernel_pfn(*spfn);
		*epfn = virt_to_kernel_pfn(*epfn);

		/* epfn should be larger than spfn */
		if (*epfn <= *spfn)
			*epfn = kernel_pfn_to_virt(*epfn);
	}

	/* Query rank information */
	vmask = (1 << num_segment) - 1;
	if ((vseg & vmask) == vmask)
		return (rank + 1);

	return 0;
}

/* Query the number of channel */
#ifdef DEBUG_FOR_CHANNEL_SWITCH
unsigned int get_channel_num(void)
#else
unsigned int __init get_channel_num(void)
#endif
{
	return channel_count;
}

/*
 * Check whether chconfig is a valid configuration,
 * and return the number of downgradation, -1 for error
 */
static int verify_channel_config(unsigned int chconfig)
{
	int ret = -1, downgrade = 0;

	for (;;) {
		/* Check for validness */
		if (chconfig == channel_count) {
			ret = 0;
			break;
		}

		/* Check for invalidness */
		if (chconfig > channel_count)
			break;

		/* Next possible channel configuration */
		chconfig <<= 1;
		downgrade += 1;
	}

	/* Got a proper downgrade level */
	if (!ret)
		return downgrade;

	return ret;
}

/* #undef DEBUG_FOR_CHANNEL_SWITCH */
/* Translate to proper PASR setting by chconfig */
static void __fill_pasr_on_by_chconfig(int downgrade, struct pasrvec *pasrvec,
		unsigned long opon)
{
#define AXIS_MAX	(4)
	int chgrp_clone_max, clone, chgrp;
	int channel_batch, element_size, i;
	int total_segnum = 0, rank, fill_element_bits, accu_element_bits;
	uint16_t element[AXIS_MAX][AXIS_MAX], tmp, mask;
	int cbi, which_channel;

	/* Sanity check */
	if (((total_pfns >> downgrade) << downgrade) != total_pfns) {
		MTKPASR_PRINT(
		"%s: can't downgrade, total_pfns[%lu] level[%d]\n",
		__func__, total_pfns, downgrade);
		return;
	}

	/* Set axis and check whether it exceeds AXIS_MAX */
	chgrp_clone_max = 1 << downgrade;
	if (chgrp_clone_max > AXIS_MAX) {
		MTKPASR_PRINT("%s: downgrade too much, level[%d] MAX[%d]\n",
				__func__, downgrade, AXIS_MAX);
		return;
	}

	/* Reset element */
	for (chgrp = 0; chgrp < chgrp_clone_max; chgrp++)
		for (clone = 0; clone < chgrp_clone_max; clone++)
			element[chgrp][clone] = 0;

	/* Set the size of batch and element */
	channel_batch = channel_count >> downgrade;
	element_size = channel_segnum >> downgrade;

	pr_info(
	"%s: downgrade[%d] opon[%lx] chgrp_clone_max[%d] channel_batch[%d] element_size[%d]\n",
	__func__, downgrade, opon, chgrp_clone_max,
	channel_batch, element_size);

	/* How many segments in total */
	for (rank = 0; rank < MAX_RANKS; rank++) {
		if (!is_valid_rank(rank))
			continue;
		total_segnum += SEGMENTS_PER_RANK;
	}

	/* Fill 1st channel group */
	clone = 0;
	accu_element_bits = 0;
	for (i = 0; i < total_segnum; i++) {
		rank = SEGMENT_TO_RANK(i);
		fill_element_bits = max_segment_pfns /
			rank_info[rank].bank_pfn_size;
		fill_element_bits = max_segment_base /
			fill_element_bits;
		/* Check whether it is PASR-masked and fill in element */
		if ((opon >> i) & 0x1) {
			while (fill_element_bits-- > 0) {
				element[0][clone] = element[0][clone] |
					(0x1 << accu_element_bits);
				accu_element_bits += 1;
				if (accu_element_bits == element_size) {
					clone++;
					accu_element_bits = 0;
				}
			}
		} else {
			accu_element_bits += fill_element_bits;
			if (accu_element_bits >= element_size) {
				clone++;
				accu_element_bits -= element_size;
			}
		}
	}

#ifdef DEBUG_FOR_CHANNEL_SWITCH
	for (chgrp = 0; chgrp < chgrp_clone_max; chgrp++)
		for (clone = 0; clone < chgrp_clone_max; clone++)
			pr_info("(a)%s: [%d][%d] = %x\n", __func__,
					chgrp, clone, element[chgrp][clone]);
#endif
	/* Copy to other channel group */
	for (clone = 0; clone < chgrp_clone_max; clone++)
		for (chgrp = 1; chgrp < chgrp_clone_max; chgrp++)
			element[chgrp][clone] = element[0][clone];

#ifdef DEBUG_FOR_CHANNEL_SWITCH
	for (chgrp = 0; chgrp < chgrp_clone_max; chgrp++)
		for (clone = 0; clone < chgrp_clone_max; clone++)
			pr_info("(b)%s: [%d][%d] = %x\n", __func__,
					chgrp, clone, element[chgrp][clone]);
#endif
	/* Mapping translation */
	for (chgrp = 0; chgrp < chgrp_clone_max; chgrp++) {
		for (clone = chgrp + 1; clone < chgrp_clone_max; clone++) {
			if (chgrp == clone)
				continue;
			tmp = element[chgrp][clone];
			element[chgrp][clone] = element[clone][chgrp];
			element[clone][chgrp] = tmp;
		}
	}

#ifdef DEBUG_FOR_CHANNEL_SWITCH
	for (chgrp = 0; chgrp < chgrp_clone_max; chgrp++)
		for (clone = 0; clone < chgrp_clone_max; clone++)
			pr_info("(c)%s: [%d][%d] = %x\n", __func__,
					chgrp, clone, element[chgrp][clone]);
#endif
	/* Fill PASR vector (by channel_batch) */
	for (chgrp = 0; chgrp < chgrp_clone_max; chgrp++) {
		clone = 0;
		accu_element_bits = 0;
		for (i = 0; i < total_segnum; i++) {
			rank = SEGMENT_TO_RANK(i);
			fill_element_bits = max_segment_pfns /
				rank_info[rank].bank_pfn_size;
			fill_element_bits = max_segment_base /
				fill_element_bits;
			mask = (0x1 << fill_element_bits) - 1;

#ifdef DEBUG_FOR_CHANNEL_SWITCH
			pr_info(
			"(+)%s: rank[%d] fill_element_bits[%d] mask[%x]\n",
			__func__, rank, fill_element_bits, mask);
#endif
			/* Compose a valid setting */
			tmp = 0x0;
			while (fill_element_bits-- > 0) {
				tmp = (tmp << 1) |
					((element[chgrp][clone]
					  >> accu_element_bits) & 0x1);
				accu_element_bits += 1;
				if (accu_element_bits == element_size) {
					clone++;
					accu_element_bits = 0;
				}
			}

#ifdef DEBUG_FOR_CHANNEL_SWITCH
			pr_info(
			"(.)%s: chgrp[%d] clone[%d] accu_element_bits[%d] tmp[%x]\n",
			__func__, chgrp, clone, accu_element_bits, tmp);
#endif
			/* Is it matched with mask */
			if ((tmp & mask) == mask)
				tmp = 0x1;
			else
				tmp = 0x0;

#ifdef DEBUG_FOR_CHANNEL_SWITCH
			pr_info("(.)%s: tmp[%x]\n", __func__, tmp);
#endif
			/* Update PASR vector */
			which_channel = chgrp * channel_batch;
			for (cbi = 0; cbi < channel_batch; cbi++) {
				pasrvec[which_channel].channel = which_channel;
				pasrvec[which_channel].pasr_on |= (tmp << i);
				which_channel += 1;
			}
		}
	}
}

/*
 * Query PASR masked with specified channel configuration
 * @chconfig: current stable channel configuration
 * @pasrvec: PASR setting according to chconfig
 * @opon: PASR setting according to original full channel configuration
 *
 * return 0 for success, < 0 for error
 */
int fill_pasr_on_by_chconfig(unsigned int chconfig, struct pasrvec *pasrvec,
		unsigned long opon)
{
	int i, downgrade = 0;

	/* We can't support 0 channel or null pasrvec */
	if (chconfig == 0 || pasrvec == NULL)
		return -EINVAL;

	/* Reset pasrvec */
	memset(pasrvec, 0, channel_count * sizeof(struct pasrvec));

	/* If opon is all zero, then just back */
	if (opon == 0)
		return 0;

	/* Use original channel configuration */
	if (chconfig == channel_count || chconfig == USE_ORIG_CHCONFIG) {
		for (i = 0; i < channel_count; i++) {
			pasrvec[i].channel = i;
			pasrvec[i].pasr_on = opon;
		}
	} else {
		/* Doesn't support 4GB mode for translation, just return */
		if (enable_4G())
			return 0;

		/* Is it a valid chconfig */
		downgrade = verify_channel_config(chconfig);
		if (downgrade < 0)
			return -EPERM;

		/* Start to translate */
		__fill_pasr_on_by_chconfig(downgrade, pasrvec, opon);
	}

	return 0;
}
