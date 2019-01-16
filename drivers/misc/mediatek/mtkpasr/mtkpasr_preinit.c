#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/memory.h>
#include <linux/memblock.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <mach/emi_mpu.h>

#ifdef CONFIG_ARM_LPAE
#include <mach/mt_lpae.h>
#endif

#define CONFIG_MTKPASR_MINDIESIZE_PFN		(0x20000)	/* 512MB */
#define MTKPASR_1GB_PFNS			(0x40000)	/* 1GB */
#define MTKPASR_2GB_PFNS			(0x80000)	/* 2GB */
#define MTKPASR_3GB_PFNS			(0xC0000)	/* 3GB */
#define MTKPASR_4GB_PFNS			(0x100000)	/* 4GB */
#define MTKPASR_DRAM_MINSIZE			MTKPASR_2GB_PFNS

#define MTKPASR_INVALID_TAG			(0xEFFFFFFF)

/* #define NO_UART_CONSOLE */
#ifndef NO_UART_CONSOLE
#define PRINT(len, string, args...)	printk(KERN_ALERT string, ##args)
#else
unsigned char mtkpasr_log_buf[4096];
static int log_stored;
#define PRINT(len, string, args...)	do { sprintf(mtkpasr_log_buf + log_stored, string, ##args); log_stored += len; } while (0)
#endif

/* Reserved possible PASR range */
struct view_pasr {
	unsigned long start_pfn;	/* The 1st pfn */
	unsigned long end_pfn;		/* The pfn after the last valid one */
};
static struct view_pasr rp_pasr_info[2];

/* Struct for parsing rank information (SW view) */
struct view_rank {
	unsigned long start_pfn;	/* The 1st pfn */
	unsigned long end_pfn;		/* The pfn after the last valid one */
	unsigned long bank_pfn_size;	/* Bank size in PFN */
	unsigned long valid_channel;	/* Channels: 0x00000101 means there are 2 valid channels - 1st & 2nd (MAX: 4 channels) */
};
static struct view_rank rank_info[MAX_RANKS];

/* Basic DRAM configuration */
static struct basic_dram_setting pasrdpd;
/* PASR/DPD imposed start rank */
static int mtkpasr_start_rank;
/* MAX Banksize in pfns (from SW view) */
unsigned long pasrbank_pfns = 0;
/*
 * We can't guarantee HIGHMEM zone is bank alignment, so we need another variable to represent it.
 * (mtkpasr_pfn_start, mtkpasr_pfn_end) is bank-alignment!
 */
static unsigned long mtkpasr_pfn_start;
static unsigned long mtkpasr_pfn_end;

/* Segment mask */
static unsigned long valid_segment = 0x0;

/* Set pageblock's mobility */
extern void set_pageblock_mobility(struct page *page, int mobility);

/* From dram_overclock.c */
extern bool pasr_is_valid(void)__attribute__((weak));
/* To confirm PASR is valid */
static inline bool could_do_mtkpasr(void)
{
	if (mtkpasr_start_rank == MTKPASR_INVALID_TAG)
		return false;

	if (pasr_is_valid)
		return pasr_is_valid();

	return false;
}

#ifdef CONFIG_ARM_LPAE
#define MAX_RANK_PFN	(0x1FFFFF)
#define MAX_KERNEL_PFN	(0x13FFFF)
#define MAX_KPFN_MASK	(0x0FFFFF)
#define KPFN_TO_VIRT	(0x100000)
static unsigned long __init virt_to_kernel_pfn(unsigned long virt)
{
	unsigned long ret = virt;

	if (enable_4G()) {
		if (virt > MAX_RANK_PFN) {
			ret = virt - KPFN_TO_VIRT;
		} else if (virt > MAX_KERNEL_PFN) {
			ret = virt & MAX_KPFN_MASK;
		}
	}

	return ret;
}
static unsigned long __init kernel_pfn_to_virt(unsigned long kpfn, bool is_end)
{
	unsigned long ret = kpfn;

	if (enable_4G()) {
		if (is_end) {
			ret = kpfn + KPFN_TO_VIRT;
		} else {
			ret = kpfn | KPFN_TO_VIRT;
		}
	}

	return ret;
}
static unsigned long __init rank_pfn_offset(void)
{
	unsigned long ret = ARCH_PFN_OFFSET;

	if (enable_4G()) {
		ret = KPFN_TO_VIRT;
	}

	return ret;
}
#else
#define virt_to_kernel_pfn(x)		(x)
#define kernel_pfn_to_virt(x, y)	(x)
#define rank_pfn_offset()		((unsigned long)ARCH_PFN_OFFSET)
#endif

/* Round up by "base" from "offset" */
static unsigned long __init round_up_base_offset(unsigned long input, unsigned long base, unsigned long offset)
{
	return ((input - offset + base - 1) / base) * base + offset;
}

/* Round down by "base" from "offset" */
static unsigned long __init round_down_base_offset(unsigned long input, unsigned long base, unsigned long offset)
{
	return ((input - offset) / base) * base + offset;
}

/*
 * Parse DRAM setting - transform DRAM setting to temporary bank structure.
 */
extern void acquire_dram_setting(struct basic_dram_setting *pasrdpd)__attribute__((weak));
static bool __init parse_dram_setting(unsigned long hint)
{
	int channel_num, chan, rank, check_segment_num;
	unsigned long valid_channel;
	unsigned long check_rank_size, rank_pfn, start_pfn = rank_pfn_offset();

	PRINT(29, "rank_pfn_offset() [0x%8lx]\n", rank_pfn_offset());

	if (acquire_dram_setting) {
		hint = 0;
		acquire_dram_setting(&pasrdpd);
		channel_num = pasrdpd.channel_nr;
		/* By ranks */
		for (rank = 0; rank < MAX_RANKS; ++rank) {
			rank_pfn = 0;
			rank_info[rank].valid_channel = 0x0;
			valid_channel = 0x1;
			check_rank_size = 0x0;
			check_segment_num = 0x0;
			for (chan = 0; chan < channel_num; ++chan) {
				if (pasrdpd.channel[chan].rank[rank].valid_rank) {
					rank_pfn += (pasrdpd.channel[chan].rank[rank].rank_size << (27 - PAGE_SHIFT));
					rank_info[rank].valid_channel |= valid_channel;
					/* Sanity check for rank size */
					if (!check_rank_size) {
						check_rank_size = pasrdpd.channel[chan].rank[rank].rank_size;
					} else {
						/* We only support ranks with equal size */
						if (check_rank_size != pasrdpd.channel[chan].rank[rank].rank_size) {
							return false;
						}
					}
					/* Sanity check for segment number */
					if (!check_segment_num) {
						check_segment_num = pasrdpd.channel[chan].rank[rank].segment_nr;
					} else {
						/* We only support ranks with equal segment number */
						if (check_segment_num != pasrdpd.channel[chan].rank[rank].segment_nr) {
							return false;
						}
					}
				}
				valid_channel <<= 8;
			}
			/* Have we found a valid rank */
			if (check_rank_size != 0 && check_segment_num != 0) {
				rank_info[rank].start_pfn = virt_to_kernel_pfn(start_pfn);
				rank_info[rank].end_pfn = virt_to_kernel_pfn(start_pfn + rank_pfn);
				rank_info[rank].bank_pfn_size = rank_pfn/check_segment_num;
				start_pfn = kernel_pfn_to_virt(rank_info[rank].end_pfn, true);
				PRINT(96, "Rank[%d] start_pfn[%8lu] end_pfn[%8lu] bank_pfn_size[%8lu] valid_channel[0x%-8lx]\n",
						rank, rank_info[rank].start_pfn, rank_info[rank].end_pfn,
						rank_info[rank].bank_pfn_size, rank_info[rank].valid_channel);
			} else {
				rank_info[rank].start_pfn = virt_to_kernel_pfn(rank_pfn_offset());
				rank_info[rank].end_pfn = virt_to_kernel_pfn(rank_pfn_offset());
				rank_info[rank].bank_pfn_size = 0;
				rank_info[rank].valid_channel = 0x0;
			}
			/* Calculate total pfns */
			hint += rank_pfn;
		}
	} else {
		/* Single channel, dual ranks, 8 segments per rank - Get a hint from system */
		rank_pfn = (hint + CONFIG_MTKPASR_MINDIESIZE_PFN - 1) & ~(CONFIG_MTKPASR_MINDIESIZE_PFN - 1);
		rank_pfn >>= 1;
		for (rank = 0; rank < 2; ++rank) {
			rank_info[rank].start_pfn = virt_to_kernel_pfn(start_pfn);
			rank_info[rank].end_pfn = virt_to_kernel_pfn(start_pfn + rank_pfn);
			rank_info[rank].bank_pfn_size = rank_pfn >> 3;
			rank_info[rank].valid_channel = 0x1;
			start_pfn = kernel_pfn_to_virt(rank_info[rank].end_pfn, true);
			PRINT(96, "(--)Rank[%d] start_pfn[%8lu] end_pfn[%8lu] bank_pfn_size[%8lu] valid_channel[0x%-8lx]\n",
					rank, rank_info[rank].start_pfn, rank_info[rank].end_pfn,
					rank_info[rank].bank_pfn_size, rank_info[rank].valid_channel);
		}
		/* Reset remaining ranks */
		for (; rank < MAX_RANKS; ++rank) {
			rank_info[rank].start_pfn = virt_to_kernel_pfn(rank_pfn_offset());
			rank_info[rank].end_pfn = virt_to_kernel_pfn(rank_pfn_offset());
			rank_info[rank].bank_pfn_size = 0;
			rank_info[rank].valid_channel = 0x0;
		}
	}

	/* Check whether it is suitable to enable PASR */
	if (hint < MTKPASR_DRAM_MINSIZE) {
		printk(KERN_ALERT "[MTKPASR] Total memory: %lu < 1GB\n", (hint << PAGE_SHIFT));
		return false;
	}

	return true;
}

/* Check whether it is a valid rank */
static bool __init is_valid_rank(int rank)
{
	/* Check start/end pfn */
	if (rank_info[rank].start_pfn == rank_info[rank].end_pfn) {
		return false;
	}

	/* Check valid_channel */
	if (rank_info[rank].valid_channel == 0x0) {
		return false;
	}

	return true;
}

#if 0
/* Show memblock */
void show_memblock(void)
{
	struct memblock_region *reg;
	phys_addr_t start;
	phys_addr_t end;

	for_each_memblock(memory, reg) {
		start = reg->base;
		end = start + reg->size;
		printk(KERN_EMERG"[PHY layout]kernel   :   0x%08llx - 0x%08llx (0x%08llx)\n",
				(unsigned long long)start,
				(unsigned long long)end - 1,
				(unsigned long long)reg->size);
	}

	for_each_memblock(reserved, reg) {
		start = reg->base;
		end = start + reg->size;
		printk(KERN_EMERG"[PHY layout]reserved   :   0x%08llx - 0x%08llx (0x%08llx)\n",
				(unsigned long long)start,
				(unsigned long long)end - 1,
				(unsigned long long)reg->size);

	}
}
#endif

#define PHYS_TO_PFN(x)	__phys_to_pfn(x)
/* Fill valid_segment */
static void __init mark_valid_segment(unsigned long start, unsigned long end, bool last)
{
	int num_segment, rank;
	unsigned long spfn, epfn;
	unsigned long rspfn, repfn;

	num_segment = 0;
	for (rank = 0; rank < MAX_RANKS; ++rank) {
		spfn = kernel_pfn_to_virt(start, false);
		epfn = kernel_pfn_to_virt(end, true);
		rspfn = kernel_pfn_to_virt(rank_info[rank].start_pfn, false);
		repfn = kernel_pfn_to_virt(rank_info[rank].end_pfn, true);
		if (is_valid_rank(rank)) {
			spfn = max(spfn, rspfn);
			if (repfn > spfn) {
				if (last) {
					spfn = round_down_base_offset(spfn, rank_info[rank].bank_pfn_size, rank_pfn_offset());		/* Round-down */
				} else {
					spfn = round_up_base_offset(spfn, rank_info[rank].bank_pfn_size, rank_pfn_offset());		/* Round-up */
				}
				epfn = min(epfn, repfn);
				while (epfn >= (spfn + rank_info[rank].bank_pfn_size)) {
					valid_segment |= (1 << ((spfn - rspfn) / rank_info[rank].bank_pfn_size + num_segment));
					spfn += rank_info[rank].bank_pfn_size;
				}
			}
			num_segment += (repfn - rspfn) / rank_info[rank].bank_pfn_size;
		}
	}
}

#if 0
/* Set page mobility to MIGRATE_MTKPASR */
static void __init set_page_mobility_mtkpasr(unsigned long start, unsigned long end, bool last)
{
	int rank;
	unsigned long spfn, epfn, espfn, vpfn, pfn;
	unsigned long rspfn, repfn;
	struct page *page;

	for (rank = 0; rank < MAX_RANKS; ++rank) {
		spfn = kernel_pfn_to_virt(start, false);
		epfn = kernel_pfn_to_virt(end, true);
		rspfn = kernel_pfn_to_virt(rank_info[rank].start_pfn, false);
		repfn = kernel_pfn_to_virt(rank_info[rank].end_pfn, true);
		if (is_valid_rank(rank)) {
			spfn = max(spfn, rspfn);
			if (repfn > spfn) {
				if (last) {
					spfn = round_down_base_offset(spfn, rank_info[rank].bank_pfn_size, rank_pfn_offset());		/* Round-down */
				} else {
					spfn = round_up_base_offset(spfn, rank_info[rank].bank_pfn_size, rank_pfn_offset());		/* Round-up */
				}
				epfn = min(epfn, repfn);
				espfn = spfn + rank_info[rank].bank_pfn_size;
				while (epfn >= espfn) {
					/* Set page mobility to MIGRATE_MTKPASR */
					for (vpfn = spfn; vpfn < espfn; vpfn++) {
						pfn = virt_to_kernel_pfn(vpfn);
						/* If invalid - Use pfn_valid instead of early_pfn_valid which depends on CONFIG_SPARSEMEM! */
						if (!pfn_valid(pfn))
							continue;
						/* Set it as MIGRATE_MTKPASR */
						page = pfn_to_page(pfn);
						if (!(pfn & (pageblock_nr_pages - 1)))
							set_pageblock_mobility(page, MIGRATE_MTKPASR);
					}
					spfn += rank_info[rank].bank_pfn_size;
					espfn = spfn + rank_info[rank].bank_pfn_size;
				}
			}
		}
	}
}
#endif

/* Fix to accommodate some feature-reserved memblocks */
static void __init fix_memblock_region(unsigned long *start, unsigned long *end)
{
	int rank;
	unsigned long total_pfn_size = 0;
	unsigned long rspfn, repfn, spfn, epfn;

	spfn = *start;
	epfn = *end;

	/* DVFS reserved */
	for (rank = 0; rank < MAX_RANKS; ++rank) {
		if (is_valid_rank(rank)) {
			rspfn = kernel_pfn_to_virt(rank_info[rank].start_pfn, false);
			repfn = kernel_pfn_to_virt(rank_info[rank].end_pfn, true);
			total_pfn_size += (repfn - rspfn);
		}
	}
	repfn = kernel_pfn_to_virt(rank_info[0].start_pfn, false) + (total_pfn_size >> 1);
	repfn = virt_to_kernel_pfn(repfn);
	if (spfn == (repfn + 1)) {
		spfn = repfn;
	}
	if (epfn == (repfn - 1)) {
		epfn = repfn;
	}

	/* Update Result */
	*start = spfn;
	*end = epfn;
}

/* Exclude memblock.reserved */
static void __init exclude_memblock_reserved(unsigned long *start, unsigned long *end)
{
	struct memblock_region *rreg;
	unsigned long rstart = 0;
	unsigned long rend = ~(unsigned long)0;

	/* Exclude kernel-reserved area */	
	for_each_memblock(reserved, rreg) {
		rstart = PHYS_TO_PFN(rreg->base);
		rend = PHYS_TO_PFN(rreg->base + rreg->size);
		if (rstart >= *start) {
			if (rend <= *end)	/* All-included */
				*start = rend;
			else if (rstart < *end)	/* Overlapped */
				*end = rstart;	
		} else {
			if (rend > *start) 	/* Overlapped */
				*start = rend;	
		}
	}
}

/* Fill valid_segment & set page mobility */
static void __init construct_mtkpasr_range(void)
{
	unsigned long vstart, vend;
	struct memblock_region *reg;
	unsigned long start = 0;
	unsigned long end = ~(unsigned long)0;
#ifdef CONFIG_MTKPASR_NO_LASTBANK
	unsigned long last_valid = 0;
#endif

	/* memblock should be sorted! */
	for_each_memblock(memory, reg) {
		vstart = mtkpasr_pfn_start;
		vend = mtkpasr_pfn_end;
		start = PHYS_TO_PFN(reg->base);
		end = PHYS_TO_PFN(reg->base + reg->size);

		/* Exclude memblock reserved */
		exclude_memblock_reserved(&start, &end);

		/* Fix memblock region */
		fix_memblock_region(&start, &end);
		/* Intersect */
		if (end > vstart && start < vend) {
			vstart = max(start, vstart);
#ifdef CONFIG_MTKPASR_NO_LASTBANK
			vstart = round_up_base_offset(vstart, pasrbank_pfns, rank_pfn_offset());
#endif
			vend = min(end, vend);
#ifdef CONFIG_MTKPASR_NO_LASTBANK
			vend = round_down_base_offset(vend, pasrbank_pfns, rank_pfn_offset());
#endif
			/* Mark valid segment */
			mark_valid_segment(vstart, vend, false);
			/* Set page mobility
			set_page_mobility_mtkpasr(vstart, vend, false);*/
#ifdef CONFIG_MTKPASR_NO_LASTBANK
			last_valid = vend;
#endif
		}
	}

	/* Last bank - TODO */

	/*    ,-------------
	 *    |             |
	 *    .-------------, -> Last valid kernel memblock	   (CONFIG_MTKPASR_NO_LASTBANK=y)
	 *    |          ---| -> mtkpasr_pfn_end (HERE)
	 *    .-------------, (AFTER ROUND-UP) new mtkpasr_pfn_end (CONFIG_MTKPASR_NO_LASTBANK=n)
	 */

#ifndef CONFIG_MTKPASR_NO_LASTBANK
	vend = mtkpasr_pfn_end;
	/* There still exists some region not initialized */
	if (end < vend) {
		vstart = max(end, mtkpasr_pfn_start);
		/* Mark valid segment */
		mark_valid_segment(vstart, vend, true);
		/* Set page mobility
		set_page_mobility_mtkpasr(vstart, vend, true);*/
	}
#else
	/* Update mtkpasr_pfn_end according to last_valid */
	mtkpasr_pfn_end = last_valid;
#endif
}

/* Mark those MTKPASRed pages as MOVABLE */
static void __init remove_needless_reserved(void)
{
	int index, order;
	struct list_head *curr, *tmp;
	struct page *spage;
	unsigned long flags;
	unsigned long pfn;
	unsigned long spfn, epfn;

	/* No PASR, remove all */
	if (mtkpasr_pfn_end == 0) {
		/* Search freelist */
		for (order = 0; order < MAX_ORDER; order++) {
			spin_lock_irqsave(&MTKPASR_ZONE->lock, flags);
			list_for_each_safe(curr, tmp, &MTKPASR_ZONE->free_area[order].free_list[MIGRATE_MTKPASR]) {
				spage = list_entry(curr, struct page, lru);
				/* Move it from original mobility to MIGRATE_MOVABLE */
				list_move(&spage->lru, &MTKPASR_ZONE->free_area[order].free_list[MIGRATE_MOVABLE]);
				/* Set it to MIGRATE_MOVABLE */
				set_pageblock_mobility(spage, MIGRATE_MOVABLE);

			}
			spin_unlock_irqrestore(&MTKPASR_ZONE->lock, flags);
		}
		/* Search inuse */
		for (index = 0; index < 2; index++) {
			spfn = rp_pasr_info[index].start_pfn;
			epfn = rp_pasr_info[index].end_pfn;
			spin_lock_irqsave(&MTKPASR_ZONE->lock, flags);
			for (pfn = spfn; pfn < epfn; pfn += pageblock_nr_pages) {
				spage = pfn_to_page(pfn);
				/* Set it to MIGRATE_MOVABLE */
				set_pageblock_mobility(spage, MIGRATE_MOVABLE);
			}
			spin_unlock_irqrestore(&MTKPASR_ZONE->lock, flags);
		}
	} else {
		/* Remove needless */
		for (index = 0; index < 2; index++) {
			spfn = rp_pasr_info[index].start_pfn;
			epfn = rp_pasr_info[index].end_pfn;
			if (spfn < epfn) {
				/* Search freelist */
				for (order = 0; order < MAX_ORDER; order++) {
					spin_lock_irqsave(&MTKPASR_ZONE->lock, flags);
					list_for_each_safe(curr, tmp, &MTKPASR_ZONE->free_area[order].free_list[MIGRATE_MTKPASR]) {
						spage = list_entry(curr, struct page, lru);
						pfn = page_to_pfn(spage);
						if ((pfn >= spfn && pfn < mtkpasr_pfn_start) || (pfn >= mtkpasr_pfn_end && pfn < epfn)) {
							/* Move it from original mobility to MIGRATE_MOVABLE */
							list_move(&spage->lru, &MTKPASR_ZONE->free_area[order].free_list[MIGRATE_MOVABLE]);
							/* Set it to MIGRATE_MOVABLE */
							set_pageblock_mobility(spage, MIGRATE_MOVABLE);
						}
					}
					spin_unlock_irqrestore(&MTKPASR_ZONE->lock, flags);
				}
				spin_lock_irqsave(&MTKPASR_ZONE->lock, flags);
				/* Search inuse */
				for (pfn = spfn; pfn < mtkpasr_pfn_start; pfn += pageblock_nr_pages) {
					spage = pfn_to_page(pfn);
					/* Set it to MIGRATE_MOVABLE */
					set_pageblock_mobility(spage, MIGRATE_MOVABLE);
				}
				for (pfn = mtkpasr_pfn_end; pfn < epfn; pfn += pageblock_nr_pages) {
					spage = pfn_to_page(pfn);
					/* Set it to MIGRATE_MOVABLE */
					set_pageblock_mobility(spage, MIGRATE_MOVABLE);
				}
				spin_unlock_irqrestore(&MTKPASR_ZONE->lock, flags);
			}
		}
	}
}

/*
 * Reserve a range for PASR operation. (MIGRATE_MTKPASR)
 */
void __init init_mtkpasr_range(struct zone *zone)
{
	struct memblock_region *reg;
	unsigned long start = 0;
	unsigned long end = ~(unsigned long)0;
	unsigned long min_start;
	struct page *page;

#ifdef CONFIG_HIGHMEM
	/* Start from HIGHMEM zone if we have CONFIG_HIGHMEM defined. */
	zone = zone + ZONE_HIGHMEM;
#else
	/* 64-bit kernel */
	zone = zone + ZONE_DMA;
#endif

	/* Sanity Check */
	if (zone != MTKPASR_ZONE) {
		mtkpasr_start_rank = MTKPASR_INVALID_TAG;
		return;
	}

	/* Min start pfn of PASR (~ 3/8 total DRAM size) */
	min_start = zone->zone_start_pfn + ((zone->spanned_pages * 3) >> 3);

	/* Reserve possible PASR range */
	for_each_memblock(memory, reg) {
		start = PHYS_TO_PFN(reg->base);
		end = PHYS_TO_PFN(reg->base + reg->size);

		/* Exclude memblock reserved */
		exclude_memblock_reserved(&start, &end);

		/* Check MAX range and swap them (START with (0,0)) */
		if ((end - start) > (rp_pasr_info[0].end_pfn - rp_pasr_info[0].start_pfn)) {
			rp_pasr_info[1].start_pfn = rp_pasr_info[0].start_pfn;
			rp_pasr_info[1].end_pfn = rp_pasr_info[0].end_pfn;
			rp_pasr_info[0].start_pfn = start;
			rp_pasr_info[0].end_pfn = end;
		} else if ((end - start) > (rp_pasr_info[1].end_pfn - rp_pasr_info[1].start_pfn)) { /* Sub-MAX */
			rp_pasr_info[1].start_pfn = start;
			rp_pasr_info[1].end_pfn = end;
		}
	}

	/* Should we remove sub-MAX */
	if (rp_pasr_info[1].start_pfn < rp_pasr_info[0].start_pfn) {
		/* smaller size & address, to remove it */
		rp_pasr_info[1].start_pfn = 0;
		rp_pasr_info[1].end_pfn = 0;
	}

	/* Do pre-reservation for PASR */
	for (start = 0; start < 2; start++) {
		/* Normalize and Remove rp_pasr_info which is beyond min_start */
		if (rp_pasr_info[start].end_pfn <= min_start) {
			rp_pasr_info[start].start_pfn = 0;
			rp_pasr_info[start].end_pfn = 0;
		} else if (rp_pasr_info[start].start_pfn < min_start) {
			rp_pasr_info[start].start_pfn = min_start;
		}
		/* pageblock_nr_pages alignment */
		rp_pasr_info[start].start_pfn = (rp_pasr_info[start].start_pfn + pageblock_nr_pages - 1) & ~(pageblock_nr_pages - 1); 
		rp_pasr_info[start].end_pfn = (rp_pasr_info[start].end_pfn) & ~(pageblock_nr_pages - 1); 
		/* Mark it as MIGRATE_MTKPASR */
		for (end = rp_pasr_info[start].start_pfn; end < rp_pasr_info[start].end_pfn; end++) {
			if (!pfn_valid(end))
				continue;
			/* Set it as MIGRATE_MTKPASR - no zone lock here! (zone is not completely ready) */
			page = pfn_to_page(end);
			if (!(end & (pageblock_nr_pages - 1)))
				set_pageblock_mobility(page, MIGRATE_MTKPASR);
		}
	}

	/* Sort rp_pasr_info by address */
	if (rp_pasr_info[1].start_pfn != 0) {
		if (rp_pasr_info[0].start_pfn > rp_pasr_info[1].start_pfn) {
			start = rp_pasr_info[0].start_pfn;
			end = rp_pasr_info[0].end_pfn;
			rp_pasr_info[0].start_pfn = rp_pasr_info[1].start_pfn;
			rp_pasr_info[0].end_pfn = rp_pasr_info[1].end_pfn;
			rp_pasr_info[1].start_pfn = start;
			rp_pasr_info[1].end_pfn = end;
		}
	}

	/* Shall we call memblock_reserve */
}

/*
 * We will set an offset on which active PASR will be imposed.
 * This is done by setting those pages as MIGRATE_MTKPASR type.
 * It only takes effect on HIGHMEM zone now!
 */
static bool __init initialize_mtkpasr_range(void)
{
	struct zone *zone;
	struct pglist_data *pgdat;
	int rank;
	unsigned long start_pfn;
	unsigned long end_pfn;
	unsigned long pfn_bank_alignment = 0;
	unsigned long shift_size = 0;

	/* Check whether our platform supports PASR */
	if (!could_do_mtkpasr()) {
		/* Can't support PASR */
		goto recover;
	}

	/* Indicate node */
	zone = MTKPASR_ZONE;
	pgdat = zone->zone_pgdat;

	/* Parsing DRAM setting */
	if (parse_dram_setting(pgdat->node_spanned_pages) == false) {
		/* Can't support PASR */
		goto recover;
	}

	/* Sanity check - Is this zone empty? */
	if (!populated_zone(zone)) {
		/* Can't support PASR */
		goto recover;
	}

	/* Mark the end pfn */
	end_pfn = zone->zone_start_pfn + zone->spanned_pages;

	/* Don't let end_pfn cross last rank to the beginning */
	for (rank = MAX_RANKS - 1; rank >= 0; --rank) {
		if (is_valid_rank(rank)) {
			end_pfn = min(end_pfn, rank_info[rank].end_pfn);
			break;
		}
	}

	/* Indicate the beginning pfn of PASR/DPD */
	for (rank = 0; rank < MAX_RANKS; ++rank) {
		if (is_valid_rank(rank)) {
			shift_size += (kernel_pfn_to_virt(rank_info[rank].end_pfn, true) - kernel_pfn_to_virt(rank_info[rank].start_pfn, false));
		}
	}

	/* Start from the half total DRAM size */
	start_pfn = rank_pfn_offset() + (shift_size >> 1);
	if (shift_size <= MTKPASR_1GB_PFNS) {
		start_pfn += (shift_size >> 4);
	}
	if (shift_size >= MTKPASR_2GB_PFNS) {
		start_pfn -= (shift_size >> 4);
	}
	if (shift_size >= MTKPASR_3GB_PFNS) {
		start_pfn -= (shift_size >> 4);
	}
	if (shift_size >= MTKPASR_4GB_PFNS) {
		start_pfn -= (shift_size >> 4);
	}

	/* Max start_pfn */
	start_pfn = max(start_pfn, kernel_pfn_to_virt(zone->zone_start_pfn, false));
	start_pfn = virt_to_kernel_pfn(start_pfn);

	/* Find out which rank "start_pfn" belongs to */
	for (rank = 0; rank < MAX_RANKS; ++rank) {
		if (kernel_pfn_to_virt(start_pfn, false) < kernel_pfn_to_virt(rank_info[rank].end_pfn, true) &&
				kernel_pfn_to_virt(start_pfn, false) >= kernel_pfn_to_virt(rank_info[rank].start_pfn, false)) {
			mtkpasr_start_rank = rank;
			pfn_bank_alignment = rank_info[rank].bank_pfn_size;
			break;
		}
	}

	/* Sanity check */
	if (!pfn_bank_alignment) {
		/* Can't support PASR */
		goto recover;
	}

	/* 1st attempted bank size */
	pasrbank_pfns = pfn_bank_alignment;

	/* Round up to bank alignment */
	start_pfn = round_up_base_offset(start_pfn, pfn_bank_alignment, ARCH_PFN_OFFSET);

	/* Find out which rank "end_pfn" belongs to */
	for (rank = 0; rank < MAX_RANKS; ++rank) {
		if (kernel_pfn_to_virt(end_pfn, true) <= kernel_pfn_to_virt(rank_info[rank].end_pfn, true) &&
				kernel_pfn_to_virt(end_pfn, true) > kernel_pfn_to_virt(rank_info[rank].start_pfn, false)) {
			pfn_bank_alignment = rank_info[rank].bank_pfn_size;
			break;
		}
	}

	/* Determine the final bank size */
	if (pasrbank_pfns < pfn_bank_alignment) {
		pasrbank_pfns = pfn_bank_alignment;
	}

	/* Find out MTKPASR Start/End PFN */
	mtkpasr_pfn_start = max(start_pfn, rp_pasr_info[0].start_pfn);
	if (rp_pasr_info[1].end_pfn != 0)
		mtkpasr_pfn_end	= min(end_pfn, rp_pasr_info[1].end_pfn);
	else
		mtkpasr_pfn_end	= min(end_pfn, rp_pasr_info[0].end_pfn);

	/* Round UP mtkpasr_pfn_start */
	mtkpasr_pfn_start = round_up_base_offset(mtkpasr_pfn_start, pfn_bank_alignment, ARCH_PFN_OFFSET);

	/* Round DOWN mtkpasr_pfn_end (a little tricky, affected by CONFIG_MTKPASR_NO_LASTBANK) */
	mtkpasr_pfn_end = round_down_base_offset(mtkpasr_pfn_end, pfn_bank_alignment, ARCH_PFN_OFFSET);

	/* Fix up - allow holes existing in the PASR range */
	construct_mtkpasr_range();

	PRINT(138, "[MTKPASR] @@@@@@ Start_pfn[%8lu] End_pfn[%8lu] (MTKPASR) start_pfn[%8lu] end_pfn[%8lu] Valid_segment[0x%8lx] @@@@@@\n",
			start_pfn, end_pfn, mtkpasr_pfn_start, mtkpasr_pfn_end, valid_segment);

	/* Put needless MIGRATE_MTKPASR pages back to buddy - TODO */
	remove_needless_reserved();
	return true;

recover:
	/* Recover pages with MIGRATE_MTKPASR flag to be MIGRATE_MOVABLE - TODO */
	PRINT(45, "Change page mobility from MTKPASR to MOVABLE\n");
	remove_needless_reserved();
	return false;
}

/* Reserve NOT-MIGRATE_MTKPASR pages in PASR range */
static void mtkpasr_reserve_reserved(void)
{
	int order = MAX_ORDER - 1, t;
	struct list_head *curr, *tmp;
	struct page *spage;
	unsigned long flags;
	unsigned long pfn;
	unsigned long fixed = 0;

	/* Move pages */
	for_each_migratetype_order(order, t) {
		if (t != MIGRATE_MTKPASR) {
			list_for_each_safe(curr, tmp, &MTKPASR_ZONE->free_area[order].free_list[t]) {
				spage = list_entry(curr, struct page, lru);
				pfn = page_to_pfn(spage);
				/* NON-MTKPASR in PASR range */
				if (pfn >= mtkpasr_pfn_start && pfn < mtkpasr_pfn_end) {
					printk(KERN_ALERT "\norder[%d] t[%d] pfn[%lu]\n", order, t, pfn);
					spin_lock_irqsave(&MTKPASR_ZONE->lock, flags);
					/* Move it from original mobility to MIGRATE_MTKPASR */
					list_move(&spage->lru, &MTKPASR_ZONE->free_area[order].free_list[MIGRATE_MTKPASR]);
					/* Set it to MIGRATE_MTKPASR */
					set_pageblock_mobility(spage, MIGRATE_MTKPASR);
					spin_unlock_irqrestore(&MTKPASR_ZONE->lock, flags);
					fixed++;
				}
			}
		}
	}
	
	printk(KERN_ALERT "[%s][%d] Fixed migrate types[%lu]\n",__func__,__LINE__,fixed);
}

/*
 * Helper of constructing Memory (Virtual) Rank & Bank Information -
 *
 * start_pfn	  - Pfn of the 1st page in that pasr range (Should be bank-aligned)
 * end_pfn	  - Pfn of the one after the last page in that pasr range (Should be bank-aligned)
 *		    (A hole may exist between end_pfn & bank-aligned(last_valid_pfn))
 * banks_per_rank - Number of banks in a rank
 *
 * Return    - The number of memory (virtual) banks, -1 means no valid range for PASR
 */
int __init compute_valid_pasr_range(unsigned long *start_pfn, unsigned long *end_pfn, int *num_ranks)
{
	int num_banks, rank, seg_num;
	unsigned long vseg;
	bool contain_rank;

	/* Initialize MTKPASR range */
	if (!initialize_mtkpasr_range()) {
		/* Can't support PASR */
		return -1;
	}
	
	/* Bitmap for valid_segment */
	vseg = valid_segment;

	/* Set PASR/DPD range */
	*start_pfn = mtkpasr_pfn_start;
	*end_pfn = mtkpasr_pfn_end;

	/* Compute number of banks & ranks*/
	num_banks = 0;
	*num_ranks = 0;
	for (rank = 0; rank < MAX_RANKS; ++rank) {
		if (is_valid_rank(rank)) {
			contain_rank = true;
			seg_num = (kernel_pfn_to_virt(rank_info[rank].end_pfn, true) - kernel_pfn_to_virt(rank_info[rank].start_pfn, false)) /
				rank_info[rank].bank_pfn_size;
			while (seg_num--) {
				if (vseg & 0x1) {
					num_banks++;
				} else {
					contain_rank = false;
				}
				vseg >>= 1;
			}
			if (contain_rank) {
				*num_ranks += 1;
			}
		}
	}

	/* No valid banks */
	if (num_banks == 0) {
		return -1;
	}

	/* Reserve NOT-MIGRATE_MTKPASR pages in PASR range */
	mtkpasr_reserve_reserved();

	return num_banks;
}

/*
 * Give bank, this function will return its (start_pfn, end_pfn) and corresponding rank
 * ("fully == true" means we should identify whether whole bank's rank is in a PASRDPD-imposed range)
 */
int __init query_bank_information(int bank, unsigned long *spfn, unsigned long *epfn, bool fully)
{
	int seg_num = 0, rank, num_segment = 0;
	unsigned long vseg = valid_segment, valid_mask;

	/* Reset */
	*spfn = 0;
	*epfn = 0;

	/* Which segment */
	do {
		if (vseg & 0x1) {
			if (!bank) {
				/* Found! */
				break;
			}
			bank--;
		}
		vseg >>= 1;
		seg_num++;
	} while (seg_num < BITS_PER_LONG);

	/* Sanity check */
	if (seg_num == BITS_PER_LONG) {
		return -1;
	}

	/* Which rank */
	vseg = valid_segment;
	for (rank = 0; rank < MAX_RANKS; ++rank) {
		if (is_valid_rank(rank)) {
			num_segment = (kernel_pfn_to_virt(rank_info[rank].end_pfn, true) - kernel_pfn_to_virt(rank_info[rank].start_pfn, false)) /
				rank_info[rank].bank_pfn_size;
			if (seg_num < num_segment) {
				*spfn = virt_to_kernel_pfn(kernel_pfn_to_virt(rank_info[rank].start_pfn, false) + seg_num * rank_info[rank].bank_pfn_size);
				*epfn = virt_to_kernel_pfn(kernel_pfn_to_virt(*spfn, false) + rank_info[rank].bank_pfn_size);
				/* Fixup to meet bank range definition */
				if (*epfn <= *spfn) {
					*epfn = kernel_pfn_to_virt(*epfn, true);
				}
				break;
			}
			seg_num -= num_segment;
			vseg >>= num_segment;
		}
	}

	/* Sanity check */
	if (rank == MAX_RANKS) {
		return -1;
	}

	/* Should acquire rank information according to "rank" */
	if (fully) {
		valid_mask = (1 << num_segment) - 1;
		if ((vseg & valid_mask) == valid_mask) {
			return rank;
		}
	}

	return -1;
}

/*
 * Translate sw bank to physical dram segment.
 * This will output different translation results depends on what dram model our platform uses.
 * non-interleaving(1-channel) vs. interleaving(n-channel, n > 1)
 *
 * Now it only supports full-interleaving translation.
 */
u32 __init pasr_bank_to_segment(unsigned long start_pfn, unsigned long end_pfn)
{
	int num_segment, rank;
	unsigned long spfn, epfn;
	unsigned long rspfn, repfn;

	spfn = kernel_pfn_to_virt(start_pfn, false);
	epfn = kernel_pfn_to_virt(end_pfn, true);
	rspfn = kernel_pfn_to_virt(rank_info[0].start_pfn, false);

	num_segment = 0;
	for (rank = 0; rank < MAX_RANKS; ++rank) {
		if (is_valid_rank(rank)) {
			rspfn = kernel_pfn_to_virt(rank_info[rank].start_pfn, false);
			repfn = kernel_pfn_to_virt(rank_info[rank].end_pfn, true);
			if (rspfn <= spfn && repfn >= epfn) {
				break;
			}
			num_segment += (repfn - rspfn) / rank_info[rank].bank_pfn_size;
			num_segment = (num_segment + 7) & ~(0x7);
		}
	}

	/* Sanity check */
	if (rank == MAX_RANKS) {
		return (0x1F);
	}

	return ((spfn - rspfn) / rank_info[rank].bank_pfn_size + num_segment);

	/*
	 *  Symmetric Interleaving
	 *  segment = (start_pfn - CONFIG_MEMPHYS_OFFSET) / pasrbank_pfns + dram_segment_offset_ch0;
	 *  // Dual-Channel   (n+n)
	 *  return segment | (segment << 8);
	 *  // Triple-Channel (n+n+n)
	 *  return segment | (segment << 8) | (segment << 16);
	 *  // Quad-Channel   (n+n+n+n)
	 *  return segment | (segment << 8) | (segment << 16) | (segment << 24);
	 */
}
