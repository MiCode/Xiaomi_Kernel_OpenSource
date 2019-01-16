/*
 * MTKPASR SW Module
 */

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/buffer_head.h>
#include <linux/device.h>
#include <linux/genhd.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/lzo.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/swap.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <linux/earlysuspend.h>
#include <linux/migrate.h>
#include "mtkpasr_drv.h"

/* #define NO_UART_CONSOLE */
#define MTKPASR_FAST_PATH

/* MTKPASR Information */
static struct zs_pool *mtkpasr_mem_pool;
static struct table *mtkpasr_table;
static u64 mtkpasr_disksize;		/* bytes */
static u32 mtkpasr_total_slots;
static u32 mtkpasr_free_slots;

/* Rank & Bank Information */
static struct mtkpasr_bank *mtkpasr_banks;
static struct mtkpasr_rank *mtkpasr_ranks;
static int num_banks;
static int num_ranks;
static unsigned long pages_per_bank;
static unsigned long mtkpasr_start_pfn;
static unsigned long mtkpasr_end_pfn;
static unsigned long mtkpasr_total_pfns;

/* Strategy control for PASR SW operation */
#ifdef CONFIG_MTKPASR_MAFL
static unsigned int mtkpasr_ops_invariant;
static unsigned int prev_mafl_count;
static unsigned int before_mafl_count;
#endif

/* For no-PASR-imposed banks */
static struct nopasr_bank *nopasr_banks;
static int num_nopasr_banks;

/* For page migration operation */
static LIST_HEAD(fromlist);
static LIST_HEAD(tolist);
static int fromlist_count;
static int tolist_count;
static unsigned long mtkpasr_migration_end;
static unsigned long mtkpasr_last_scan;
static int mtkpasr_admit_order;

/* Switch : Enabled by default */
int mtkpasr_enable = 1;
unsigned long mtkpasr_enable_sr = 1;

/* Receive PM notifier flag */
static bool pm_in_hibernation = false;

/* Debug filter */
#ifdef CONFIG_MT_ENG_BUILD
int mtkpasr_debug_level = 3;
#else
int mtkpasr_debug_level = 1;
#endif

/* Globals */
struct mtkpasr *mtkpasr_device;

/*------------------*/
/*-- page_alloc.c --*/
/*------------------*/

/* Find inuse & free pages */
extern int pasr_find_free_page(struct page *page, struct list_head *freelist);
/* Compute admit order for page allocation */
extern int pasr_compute_safe_order(void);

/* Banksize */
extern unsigned long pasrbank_pfns;

/*--------------*/
/*-- vmscan.c --*/
/*--------------*/

/* Isolate pages */
#ifdef CONFIG_MTKPASR_ALLEXTCOMP
extern int mtkpasr_isolate_page(struct page *page, int check_swap);
#else
extern int mtkpasr_isolate_page(struct page *page);
#endif
/* Drop pages in file/anon lrus! */
extern int mtkpasr_drop_page(struct page *page);

#ifdef NO_UART_CONSOLE
extern unsigned char mtkpasr_log_buf[4096];
#endif

#define MTKPASR_EXHAUSTED	(low_wmark_pages(MTKPASR_ZONE) + pageblock_nr_pages)
/* Show mem banks */
int mtkpasr_show_banks(char *buf)
{
	int i, j, len = 0, tmp;

	if (mtkpasr_device->init_done == 0)
		return sprintf(buf, "MTKPASR is not initialized!\n");

	/* Overview */
	tmp = sprintf(buf, "num_banks[%d] num_ranks[%d] mtkpasr_start_pfn[%ld] mtkpasr_end_pfn[%ld] mtkpasr_total_pfns[%ld]\n",
			num_banks, num_ranks, mtkpasr_start_pfn, mtkpasr_end_pfn, mtkpasr_total_pfns);
	buf += tmp;
	len += tmp;

	/* Show ranks & banks */
	for (i = 0; i < num_ranks; i++) {
		tmp = sprintf(buf, "Rank[%d] - start_bank[%d] end_bank[%d]\n", i, mtkpasr_ranks[i].start_bank, mtkpasr_ranks[i].end_bank);
		buf += tmp;
		len += tmp;
		for (j = mtkpasr_ranks[i].start_bank; j <= mtkpasr_ranks[i].end_bank; j++) {
			tmp = sprintf(buf, "  Bank[%d] - start_pfn[0x%lx] end_pfn[0x%lx] inmafl[%d] segment[%d]\n", j, mtkpasr_banks[j].start_pfn, mtkpasr_banks[j].end_pfn-1, mtkpasr_banks[j].inmafl, mtkpasr_banks[j].segment);
			buf += tmp;
			len += tmp;
		}
	}

	/* Show remaining banks */
	for (i = 0; i < num_banks; i++) {
		if (mtkpasr_banks[i].rank == NULL) {
			tmp = sprintf(buf, "Bank[%d] - start_pfn[0x%lx] end_pfn[0x%lx] inmafl[%d] segment[%d]\n", i, mtkpasr_banks[i].start_pfn, mtkpasr_banks[i].end_pfn-1, mtkpasr_banks[i].inmafl, mtkpasr_banks[i].segment);
			buf += tmp;
			len += tmp;
		}
	}

	/* Others */
	tmp = sprintf(buf, "Exhausted level[%ld]\n", (unsigned long)MTKPASR_EXHAUSTED);
	buf += tmp;
	len += tmp;

#ifdef NO_UART_CONSOLE
	memcpy(buf, mtkpasr_log_buf, 1024);
	len += 1024;
#endif

	return len;
}

static void mtkpasr_free_page(struct mtkpasr *mtkpasr, size_t index)
{
	unsigned long handle = mtkpasr_table[index].handle;

	if (unlikely(!handle)) {
		/*
		 * No memory is allocated for zero filled pages.
		 * Simply clear zero page flag.
		 */
		return;
	}

	zs_free(mtkpasr_mem_pool, handle);

	/* Reset related fields to make it consistent ! */
	mtkpasr_table[index].handle = 0;
	mtkpasr_table[index].size = 0;
	mtkpasr_table[index].obj = NULL;
}

/* 0x5D5D5D5D */
static void handle_zero_page(struct page *page)
{
	void *user_mem;

	user_mem = kmap_atomic(page);
	memset(user_mem, 0x5D, PAGE_SIZE);
	kunmap_atomic(user_mem);

	flush_dcache_page(page);
}

static int mtkpasr_read(struct mtkpasr *mtkpasr, u32 index, struct page *page)
{
	int ret;
	size_t clen;
	unsigned char *user_mem, *cmem;

	/* !! We encapsulate the page into mtkpasr_table !! */
	page = mtkpasr_table[index].obj;
	if (page == NULL) {
		mtkpasr_err("\n\n\n\nNull Page!\n\n\n\n");
		return 0;
	}

	/* Requested page is not present in compressed area */
	if (unlikely(!mtkpasr_table[index].handle)) {
		mtkpasr_err("Not present : page pfn[%ld]\n", page_to_pfn(page));
		handle_zero_page(page);
		return 0;
	}

	user_mem = kmap_atomic(page);
	clen = PAGE_SIZE;
	cmem = zs_map_object(mtkpasr_mem_pool, mtkpasr_table[index].handle, ZS_MM_RO);

	if (mtkpasr_table[index].size == PAGE_SIZE) {
		memcpy(user_mem, cmem, PAGE_SIZE);
		ret = LZO_E_OK;
	} else {
		ret = lzo1x_decompress_safe(cmem, mtkpasr_table[index].size, user_mem, &clen);
	}

	zs_unmap_object(mtkpasr_mem_pool, mtkpasr_table[index].handle);

	kunmap_atomic(user_mem);

	/* Should NEVER happen. Return bio error if it does. */
	if (unlikely(ret != LZO_E_OK)) {
		mtkpasr_err("Decompression failed! err=%d, page=%u, pfn=%lu\n", ret, index, page_to_pfn(page));
		/* Should be zero! */
		/* return 0; */	/* to free this slot */
	}

	/* Can't use it because maybe some pages w/o actual mapping
	flush_dcache_page(page); */

	/* Free this object */
	mtkpasr_free_page(mtkpasr, index);

	return 0;
}

static int mtkpasr_write(struct mtkpasr *mtkpasr, u32 index, struct page *page)
{
	int ret;
	size_t clen;
	unsigned long handle;
	unsigned char *user_mem, *cmem, *src;

	src = mtkpasr->compress_buffer;

	/*
	 * System overwrites unused sectors. Free memory associated
	 * with this sector now.
	 */

	user_mem = kmap_atomic(page);

	ret = lzo1x_1_compress(user_mem, PAGE_SIZE, src, &clen, mtkpasr->compress_workmem);

	kunmap_atomic(user_mem);

	if (unlikely(ret != LZO_E_OK)) {
		mtkpasr_err("Compression failed! err=%d\n", ret);
		ret = -EIO;
		goto out;
	}

	if (unlikely(clen > max_cmpr_size)) {
		src = NULL;
		clen = PAGE_SIZE;
	}

	handle = zs_malloc(mtkpasr_mem_pool, clen);
	if (!handle) {
		mtkpasr_err("Error allocating memory for compressed "
			"page: %u, size=%zu\n", index, clen);
		ret = -ENOMEM;
		goto out;
	}
	cmem = zs_map_object(mtkpasr_mem_pool, handle, ZS_MM_WO);

	if (clen == PAGE_SIZE)
		src = kmap_atomic(page);
	memcpy(cmem, src, clen);
	if (clen == PAGE_SIZE)
		kunmap_atomic(src);

	zs_unmap_object(mtkpasr_mem_pool, handle);

	/* Update global MTKPASR table */
	mtkpasr_table[index].handle = handle;
	mtkpasr_table[index].size = clen;
	mtkpasr_table[index].obj = page;

	return 0;

out:
	return ret;
}

/* This is the main entry for active memory compression for PASR */
/*
 * return 0 means success
 */
int mtkpasr_forward_rw(struct mtkpasr *mtkpasr, u32 index, struct page *page, int rw)
{
	int ret = -ENOMEM;

	if (rw == READ) {
		ret = mtkpasr_read(mtkpasr, index, page);
		mtkpasr_free_slots = (!!ret) ? mtkpasr_free_slots : (mtkpasr_free_slots + 1);
	} else {
		/* No free slot! */
		if (mtkpasr_free_slots == 0) {
			mtkpasr_log("No free slots!\n");
			return ret;
		}
		ret = mtkpasr_write(mtkpasr, index, page);
		mtkpasr_free_slots = (!!ret) ? mtkpasr_free_slots : (mtkpasr_free_slots - 1);
	}

	return ret;
}
EXPORT_SYMBOL(mtkpasr_forward_rw);

/* Acquire the number of free slots */
int mtkpasr_acquire_frees(void)
{
	return mtkpasr_free_slots;
}
EXPORT_SYMBOL(mtkpasr_acquire_frees);

/* Acquire the number of total slots */
int mtkpasr_acquire_total(void)
{
	return mtkpasr_total_slots;
}
EXPORT_SYMBOL(mtkpasr_acquire_total);

/* This is a recovery step for invalid PASR status */
void mtkpasr_reset_slots(void)
{
	size_t index;

	/* Free all pages that are still in this mtkpasr device */
	for (index = 0; index < mtkpasr_total_slots; index++) {
		unsigned long handle = mtkpasr_table[index].handle;
		if (!handle)
			continue;

		zs_free(mtkpasr_mem_pool, handle);

		/* Reset related fields to make it consistent ! */
		mtkpasr_table[index].handle = 0;
		mtkpasr_table[index].size = 0;
		mtkpasr_table[index].obj = NULL;

		/* Add it */
		mtkpasr_free_slots++;
	}

#ifdef CONFIG_MTKPASR_DEBUG
	if (mtkpasr_free_slots != mtkpasr_total_slots) {
		BUG();
	}
#endif
}

/*******************************/
/* MTKPASR Core Implementation */
/*******************************/

/* Helper function for page migration (Runnint under IRQ-disabled environment ) */
/* To avoid fragmentation through mtkpasr_admit_order */
static struct page *mtkpasr_alloc(struct page *migratepage, unsigned long data, int **result)
{
#ifdef MTKPASR_FAST_PATH/* FAST PATH */
	struct page *page = NULL, *end_page;
	struct zone *z;
	/*unsigned long flags;*/
	int found;
	int order;

	/* No admission on page allocation */
	if (unlikely(mtkpasr_admit_order < 0)) {
		return NULL;
	}
retry:
	/* We still have some free pages */
	if (!list_empty(&tolist)) {
		page = list_entry(tolist.next, struct page, lru);
		list_del(&page->lru);
#ifdef CONFIG_MTKPASR_DEBUG
		--tolist_count;
#endif
	} else {
		/* Check whether mtkpasr_last_scan meets the end */
		if (mtkpasr_last_scan < mtkpasr_migration_end) {
			mtkpasr_last_scan = mtkpasr_start_pfn - pageblock_nr_pages;
			/* Need to update allocation status to avoid HWT */
			mtkpasr_admit_order = -1;
			return NULL;
		}
		/* To collect free pages */
		page = pfn_to_page(mtkpasr_last_scan);
		end_page = pfn_to_page(mtkpasr_last_scan + pageblock_nr_pages);
		z = page_zone(page);
		while (page < end_page) {
			/* Lock this zone */
			/*** spin_lock_irqsave(&z->lock, flags); ***/
			local_irq_disable();
			/* Find free pages */
			if (!PageBuddy(page)) {
				/*** spin_unlock_irqrestore(&z->lock, flags); ***/
				++page;
				continue;
			}
			/* Is this ok? */
			order = PAGE_ORDER(page);
			if (order > mtkpasr_admit_order) {
				/*** spin_unlock_irqrestore(&z->lock, flags); ***/
				page += (1 << order);
				continue;
			}
			/* Found! */
			found = pasr_find_free_page(page, &tolist);
			/* Unlock this zone */
			/*** spin_unlock_irqrestore(&z->lock, flags); ***/
			/* Update found */
#ifdef CONFIG_MTKPASR_DEBUG
			tolist_count += found;
#endif
			page += found;
		}
		/* Update mtkpasr_last_scan*/
		mtkpasr_last_scan -= pageblock_nr_pages;

		/* Retry */
		goto retry;
	}

	return page;
#else
	/* With __GFP_HIGHMEM? */
	return alloc_pages(__GFP_HIGHMEM|GFP_ATOMIC, 0);
#endif
}

/* Return whether current system has enough free memory to save it from congestion */
#define SAFE_ORDER	(THREAD_SIZE_ORDER + 1)
static struct zone *nz;
static unsigned long safe_level;
static int pasr_check_free_safe(void)
{
	unsigned long free = zone_page_state(nz, NR_FREE_PAGES);

	/* Subtract order:0 and order:1 from total free number */
	free = free - MTKPASR_ZONE->free_area[0].nr_free - (MTKPASR_ZONE->free_area[1].nr_free << 1);

	/* Judgement */
	if (free > safe_level) {
		return 0;
	}

	return -1;
}

/*-- MTKPASR INTERNAL-USED PARAMETERS --*/

/* Map of in-use pages: For a bank with 128MB, we need 32 pages. */
static void *src_pgmap;
/* Maps for pages in external compression */
static unsigned long *extcomp;
/* With the size equal to src_pgmap */
static unsigned long *sorted_for_extcomp;
/* MTKPASR state */
static enum mtkpasr_phase mtkpasr_status = MTKPASR_OFF;
/* Atomic variable to indicate MTKPASR slot */
static atomic_t sloti;

#ifdef CONFIG_MTKPASR_MAFL
static unsigned long mafl_total_count;
unsigned long mtkpasr_show_page_reserved(void)
{
	return mafl_total_count;
}
bool mtkpasr_no_phaseone_ops(void)
{
	int safe_mtkpasr_pfns;

	safe_mtkpasr_pfns = mtkpasr_total_pfns >> 1;
	return ((prev_mafl_count == mafl_total_count) || (mafl_total_count > safe_mtkpasr_pfns));
}
bool mtkpasr_no_ops(void)
{
	return ((mafl_total_count == mtkpasr_total_pfns) || (mtkpasr_ops_invariant > MAX_OPS_INVARIANT));
}
#endif

/* Reset state to MTKPASR_OFF */
void mtkpasr_reset_state(void)
{
	mtkpasr_reset_slots();
	mtkpasr_status = MTKPASR_OFF;
}

/* Enabling SR or Turning off DPD. It should be called after syscore_resume
 * Actually, it only does reset on the status of all ranks & banks!
 *
 * Return - MTKPASR_WRONG_STATE (Should bypass it & go to next state)
 *	    MTKPASR_SUCCESS
 */
enum mtkpasr_phase mtkpasr_enablingSR(void)
{
	enum mtkpasr_phase result = MTKPASR_SUCCESS;
	int check_dpd = 0;
	int i, j;

	/* Sanity Check */
	if (mtkpasr_status != MTKPASR_ON && mtkpasr_status != MTKPASR_DPD_ON) {
		mtkpasr_err("Error Current State [%d]!\n", mtkpasr_status);
		return MTKPASR_WRONG_STATE;
	} else {
		check_dpd = (mtkpasr_status == MTKPASR_DPD_ON) ? 1 : 0;
		/* Go to MTKPASR_ENABLINGSR state */
		mtkpasr_status = MTKPASR_ENABLINGSR;
	}

	/* From which state */
	if (check_dpd) {
		for (i = 0; i < num_ranks; i++) {
			if (mtkpasr_ranks[i].inused == MTKPASR_DPDON) {
				/* Clear rank */
				mtkpasr_ranks[i].inused = 0;
				/* Clear all related banks */
				for (j = mtkpasr_ranks[i].start_bank; j <= mtkpasr_ranks[i].end_bank; j++) {
					mtkpasr_banks[j].inused = 0;
				}
				mtkpasr_info("Call DPDOFF API!\n");
			}
		}
	}

	/* Check all banks */
	for (i = 0; i < num_banks; i++) {
		if (mtkpasr_banks[i].inused == MTKPASR_SROFF) {
			/* Clear bank */
			mtkpasr_banks[i].inused = 0;
			mtkpasr_info("Call SPM SR/ON API on bank[%d]!\n", i);
		} else {
			mtkpasr_info("Bank[%d] free[%d]!\n", i, mtkpasr_banks[i].inused);
		}
	}

	/* Go to MTKPASR_EXITING state if success(Always being success!) */
	if (result == MTKPASR_SUCCESS) {
		mtkpasr_status = MTKPASR_EXITING;
	}

	return result;
}

/* Decompress all immediate data. It should be called right after mtkpasr_enablingSR
 *
 * Return - MTKPASR_WRONG_STATE
 *          MTKPASR_SUCCESS
 *          MTKPASR_FAIL (Some fatal error!)
 */
enum mtkpasr_phase mtkpasr_exiting(void)
{
	enum mtkpasr_phase result = MTKPASR_SUCCESS;
	int current_index;
	int ret = 0;
	struct mtkpasr *mtkpasr;
	int should_flush_cache = 0;
#ifdef CONFIG_MTKPASR_DEBUG
	int decompressed = 0;
#endif

	mtkpasr_info("\n");

	/* Sanity Check */
	if (mtkpasr_status != MTKPASR_EXITING) {
		mtkpasr_err("Error Current State [%d]!\n", mtkpasr_status);
		/*
		 * Failed to exit PASR!! - This will cause some user processes died unexpectedly!
		 * We don't do anything here because it is harmless to kernel.
		 */
		return MTKPASR_WRONG_STATE;
	}

	/* Main thread is here */
	mtkpasr = &mtkpasr_device[0];

	/* Do decompression */
	current_index = atomic_dec_return(&sloti);
	should_flush_cache = current_index;
	while (current_index >= 0) {
		ret = mtkpasr_forward_rw(mtkpasr, current_index, NULL, READ);
		/* Unsuccessful decompression */
		if (unlikely(ret)) {
			break;
		}
#ifdef CONFIG_MTKPASR_DEBUG
		++decompressed;
#endif
		/* Next */
		current_index = atomic_dec_return(&sloti);
	}

	/* Check decompression result */
	if (ret) {
		mtkpasr_err("Failed Decompression!\n");
		/*
		 * Failed to exit PASR!! - This will cause some user processes died unexpectedly!
		 * We don't do anything here because it is harmless to kernel.
		 */
		result = MTKPASR_FAIL;
	}

	/* Go to MTKPASR_OFF state if success */
	if (result == MTKPASR_SUCCESS) {
		mtkpasr_status = MTKPASR_OFF;
	}

	/* Check whether we should flush cache */
	if (should_flush_cache >= 0)
		flush_cache_all();

#ifdef CONFIG_MTKPASR_DEBUG
	mtkpasr_info("Decompressed pages [%d]\n", decompressed);
#endif
	return result;
}

/* If something error happens at MTKPASR_ENTERING/MTKPASR_DISABLINGSR, then call it */
void mtkpasr_restoring(void)
{
	mtkpasr_info("\n");

	/* Sanity Check */
	if (mtkpasr_status != MTKPASR_ENTERING && mtkpasr_status != MTKPASR_DISABLINGSR) {
		mtkpasr_err("Error Current State [%d]!\n", mtkpasr_status);
		return;
	} else {
		/* Go to MTKPASR_RESTORING state */
		mtkpasr_status = MTKPASR_RESTORING;
	}

	/* No matter which status it reaches, we only need to do is to reset all slots here!(Data stored is not corrupted!) */
	mtkpasr_reset_slots();

	/* Go to MTKPASR_OFF state */
	mtkpasr_status = MTKPASR_OFF;

	mtkpasr_info("(END)\n");
}

/*
 * Check whether current page is compressed
 * return 1 means found
 *        0       not found
 */
static int check_if_compressed(long start, long end, int pfn)
{
#ifndef CONFIG_64BIT
	long mid;
	int found = 0;

	/* Needed! */
	end = end - 1;

	/* Start to search */
	while (start <= end) {
		mid = (start + end) >> 1;
		if (pfn == extcomp[mid]) {
			found = 1;
			break;
		} else if (pfn > extcomp[mid]) {
			start = mid + 1;
		} else {
			end = mid - 1;
		}
	}

	return found;
#else
	return 0;
#endif
}

/* Return the number of inuse pages */
static u32 check_inused(unsigned long start, unsigned long end, long comp_start, long comp_end)
{
	int inused = 0;
	struct page *page;

	for (; start < end; start++) {
		page = pfn_to_page(start);
		if (page_count(page) != 0) {
			if (check_if_compressed(comp_start, comp_end, start) == 0) {
				++inused;
			}
		}
	}

	return inused;
}

/* Compute bank inused! */
static void compute_bank_inused(int all)
{
	int i;

#ifdef CONFIG_MTKPASR_MAFL
	/* fast path */
	if (mtkpasr_no_ops()) {
		for (i = 0; i < num_banks; i++) {
			if (mtkpasr_banks[i].inmafl == mtkpasr_banks[i].valid_pages) {
				mtkpasr_banks[i].inused = 0;
			} else {
				/* Rough estimation */
				mtkpasr_banks[i].inused = mtkpasr_banks[i].valid_pages - mtkpasr_banks[i].inmafl;
			}
		}
		goto fast_path;
	}
#endif

	/*
	 * Drain pcp LRU lists to free some "unused" pages!
	 * (During page migration, there may be some OLD pages be in pcp pagevec! To free them!)
	 * To call lru_add_drain();
	 *
	 * Drain pcp free lists to free some hot/cold pages into buddy!
	 * To call drain_local_pages(NULL);
	 */
	MTKPASR_FLUSH();

	/* Scan banks */
	for (i = 0; i < num_banks; i++) {
#ifdef CONFIG_MTKPASR_MAFL
		mtkpasr_banks[i].inused = check_inused(mtkpasr_banks[i].start_pfn + mtkpasr_banks[i].inmafl, mtkpasr_banks[i].end_pfn,
				mtkpasr_banks[i].comp_start, mtkpasr_banks[i].comp_end);
#else
		mtkpasr_banks[i].inused = check_inused(mtkpasr_banks[i].start_pfn, mtkpasr_banks[i].end_pfn,
				mtkpasr_banks[i].comp_start, mtkpasr_banks[i].comp_end);
#endif
	}

#ifdef CONFIG_MTKPASR_MAFL
fast_path:
#endif
	/* Should we compute no-PASR-imposed banks? */
	if (all != 0) {
		/* Excluding 1st nopasr_banks (Kernel resides here.)*/
		for (i = 1; i < num_nopasr_banks; i++) {
			nopasr_banks[i].inused = check_inused(nopasr_banks[i].start_pfn, nopasr_banks[i].end_pfn, 0, 0);
		}
	}
}

#ifdef CONFIG_MTKPASR_MAFL
/* Test whether it can be removed from buddy temporarily */
static void remove_bank_from_buddy(int bank)
{
	int has_extcomp = mtkpasr_banks[bank].comp_start - mtkpasr_banks[bank].comp_end;
	struct page *spage, *epage;
	struct zone *z;
	unsigned long flags;
	unsigned int order;
	int free_count;
	struct list_head *mafl;
	int *inmafl;

	/* mafl is full */
	inmafl = &mtkpasr_banks[bank].inmafl;
	if (*inmafl == mtkpasr_banks[bank].valid_pages) {
		return;
	}

	/* This bank can't be removed! Don't consider banks with external compression. */
	if (has_extcomp != 0) {
		return;
	}

	spage = pfn_to_page(mtkpasr_banks[bank].start_pfn);
	epage = pfn_to_page(mtkpasr_banks[bank].end_pfn);
	z = page_zone(spage);

	/* Lock this zone */
	spin_lock_irqsave(&z->lock, flags);

	/* Check whether remaining pages are in buddy */
	spage += *inmafl;
	while (spage < epage) {
		/* Not in buddy, exit */
		if (!PageBuddy(spage)) {
			spin_unlock_irqrestore(&z->lock, flags);
			return;
		}
		/* Check next page block */
		free_count = 1 << PAGE_ORDER(spage);
		spage += free_count;
	}

	/* Remove it from buddy to bank's mafl */
	mafl = &mtkpasr_banks[bank].mafl;
	spage = pfn_to_page(mtkpasr_banks[bank].start_pfn) + *inmafl;
	while (spage < epage) {
		/* Delete it from buddy */
		list_del(&spage->lru);
		order = PAGE_ORDER(spage);
		z->free_area[order].nr_free--;
		/* No removal on page block's order - rmv_PAGE_ORDER(spage); */
		__mod_zone_page_state(z, NR_FREE_PAGES, -(1UL << order));
		/* Add it to mafl */
		list_add_tail(&spage->lru, mafl);
		/* Check next page block */
		free_count = 1 << order;
		spage += free_count;
		/* Update statistics */
		*inmafl += free_count;
		mafl_total_count += free_count;
	}

	/* UnLock this zone */
	spin_unlock_irqrestore(&z->lock, flags);

#ifdef CONFIG_MTKPASR_DEBUG
	if (mtkpasr_banks[bank].inmafl != mtkpasr_banks[bank].valid_pages) {
		BUG();
	}
#endif
}

static bool mtkpasr_no_exhausted(int request_order)
{
	long free, exhausted_level = MTKPASR_EXHAUSTED;
	int order;

	free = zone_page_state(MTKPASR_ZONE, NR_FREE_PAGES);
	if (request_order > 0) {
		for (order = 0; order < request_order; ++order) {
			free -= MTKPASR_ZONE->free_area[order].nr_free << order;
		}
		exhausted_level >>= order;
	}

	return (free >= exhausted_level);
}

/* Early path to release mtkpasr reserved pages */
void try_to_release_mtkpasr_page(int request_order)
{
	int current_bank = 0;
	struct list_head *mafl = NULL;
	struct page *page;
	struct zone *z;
	unsigned long flags;
	unsigned int order;
	int free_count;

	/* We are in MTKPASR stage! */
	if (unlikely(current->flags & PF_MTKPASR)) {
		return;
	}

	/* Check whether it is empty */
	if (mafl_total_count <= 0) {
		return;
	}

	/* Test whether mtkpasr is under suitable level */
	if (mtkpasr_no_exhausted(request_order)) {
		return;
	}

	/* Try to release one page block */
	while (current_bank < num_banks) {
		mafl = &mtkpasr_banks[current_bank].mafl;
		if (!list_empty(mafl)) {
			break;
		}
		++current_bank;
		mafl = NULL;
	}

	/* Avoid uninitialized */
	if (mafl == NULL) {
		return;
	}

	/* Lock this zone */
	z = page_zone(pfn_to_page(mtkpasr_banks[current_bank].start_pfn));
	spin_lock_irqsave(&z->lock, flags);

	/* It may be empty here due to another release operation! */
	if (list_empty(mafl)) {
		spin_unlock_irqrestore(&z->lock, flags);
		return;
	}

	/* Put the last page block back - (Should be paired with remove_bank_from_buddy) */
	page = list_entry(mafl->prev, struct page, lru);
	list_del(&page->lru);
	order = PAGE_ORDER(page);

	/* Add to tail!! */
	list_add_tail(&page->lru, &z->free_area[order].free_list[MIGRATE_MTKPASR]);
	__mod_zone_page_state(z, NR_FREE_PAGES, 1UL << order);
	z->free_area[order].nr_free++;

	/* Update statistics */
	free_count = 1 << order;
	mtkpasr_banks[current_bank].inmafl -= free_count;
	mafl_total_count -= free_count;

	/* UnLock this zone */
	spin_unlock_irqrestore(&z->lock, flags);

	/* Sanity check */
	if (mtkpasr_banks[current_bank].inmafl < 0) {
		mtkpasr_info("BUG: Negative inmafl in bank[%d] Remaining MAFL [%ld]!\n", current_bank, mafl_total_count);
	}
}

/* Shrinking mtkpasr_banks[bank]'s mafl totally */
static void shrink_mafl_all(int bank)
{
	struct list_head *mafl = NULL;
	int *inmafl;
	struct page *page, *next;
	struct zone *z;
	unsigned long flags;
	unsigned int order;
#ifdef CONFIG_MTKPASR_DEBUG
	int free_count = 0;
#endif

	/* Sanity check */
	if (bank >= num_banks || bank < 0) {
		return;
	}

	mafl = &mtkpasr_banks[bank].mafl;
	if (list_empty(mafl)) {
		return;
	}

	z = page_zone(pfn_to_page(mtkpasr_banks[bank].start_pfn));

	/* Lock this zone */
	spin_lock_irqsave(&z->lock, flags);

	/* Current number in mafl */
	if (list_empty(mafl)) {
		spin_unlock_irqrestore(&z->lock, flags);
		return;
	}
	inmafl = &mtkpasr_banks[bank].inmafl;

	/* Put them back */
	list_for_each_entry_safe(page, next, mafl, lru) {
		list_del(&page->lru);
		order = PAGE_ORDER(page);
		/* Add to tail!! */
		list_add_tail(&page->lru, &z->free_area[order].free_list[MIGRATE_MTKPASR]);
		__mod_zone_page_state(z, NR_FREE_PAGES, 1UL << order);
		z->free_area[order].nr_free++;
#ifdef CONFIG_MTKPASR_DEBUG
		free_count += (1 << order);
#endif
	}

#ifdef CONFIG_MTKPASR_DEBUG
	/* Test whether they are equal */
	if (free_count != *inmafl) {
		mtkpasr_err("\n\n BANK[%d] free_count[%d] inmafl[%d]\n\n\n", bank, free_count, *inmafl);
		BUG();
	}
#endif

	/* Update statistics */
	mafl_total_count -= *inmafl;
	*inmafl = 0;

	/* UnLock this zone */
	spin_unlock_irqrestore(&z->lock, flags);
}

static int collect_free_pages_for_compacting_banks(struct mtkpasr_bank_cc *bcc);
static struct page *compacting_alloc(struct page *migratepage, unsigned long data, int **result);
static unsigned long putback_free_pages(struct list_head *freelist);
/* Shrink all mtkpasr memory */
void shrink_mtkpasr_all(void)
{
	int i;
	struct page *page, *end_page;
	int to_be_migrated = 1;
	struct mtkpasr_bank_cc bank_cc;

	/* No valid PASR range */
	if (num_banks <= 0) {
		return;
	}

	/* Go through all banks */
	for (i = 0; i < num_banks; i++) {
		shrink_mafl_all(i);
	}

	/* Move all LRU pages from normal to high */
	bank_cc.to_bank = i = 0;
	bank_cc.to_cursor = mtkpasr_banks[i].start_pfn;
	page = pfn_to_page(mtkpasr_start_pfn);
	end_page = pfn_to_page(mtkpasr_migration_end);
	while (page > end_page) {
		/* To isolate */
		if (page_count(page) != 0) {
			if (!mtkpasr_isolate_page(page)) {
				list_add(&page->lru, &fromlist);
				++fromlist_count;
				++to_be_migrated;
			}
			/* To migrate */
			if ((to_be_migrated % MTKPASR_CHECK_MIGRATE) == 0) {
				/* Collect free pages */
				while (collect_free_pages_for_compacting_banks(&bank_cc) < fromlist_count) {
					if (bank_cc.to_cursor == 0) {
						if (++i == num_banks) {
							goto done;
						}
						bank_cc.to_bank = i;
						bank_cc.to_cursor = mtkpasr_banks[i].start_pfn;
					} else {
						mtkpasr_err("Failed to collect free pages!\n");
						BUG();
					}
				}
				/* Start migration */
				if (MIGRATE_PAGES(&fromlist, compacting_alloc, 0) != 0) {
					putback_lru_pages(&fromlist);
				}
				fromlist_count = 0;
			}
		}
		/* Next one */
		page--;
	}

done:
	/* Clear fromlist */
	if (fromlist_count != 0) {
		putback_lru_pages(&fromlist);
		fromlist_count = 0;
	}

	/* Clear tolist */
	if (tolist_count != 0) {
		if (putback_free_pages(&tolist) != tolist_count) {
			mtkpasr_err("Should be the same!\n");
		}
		tolist_count = 0;
	}
}

void shrink_mtkpasr_late_resume(void)
{
	/* Check whether it is an early resume (No MTKPASR is triggered) */
	if (!is_mtkpasr_triggered()) {
		return;
	}

	/* Reset ops invariant */
	mtkpasr_ops_invariant = 0;

	/* Clear triggered */
	clear_mtkpasr_triggered();
}
#else /* CONFIG_MTKPASR_MAFL */

void shrink_mtkpasr_all(void) { do {} while (0); }
void shrink_mtkpasr_late_resume(void) { do {} while (0); }

#endif /* CONFIG_MTKPASR_MAFL */

/*
 * Scan Bank Information & disable its SR or DPD full rank if possible. It should be called after syscore_suspend
 * - sr , indicate which segment will be SR-offed.
 *   dpd, indicate which package will be dpd.
 *
 * Return - MTKPASR_WRONG_STATE
 *          MTKPASR_GET_WAKEUP
 *	    MTKPASR_SUCCESS
 */
enum mtkpasr_phase mtkpasr_disablingSR(u32 *sr, u32 *dpd)
{
	enum mtkpasr_phase result = MTKPASR_SUCCESS;
	int i, j;
	int enter_dpd = 0;
	u32 banksr = 0x0;	/* From SPM's specification, 0 means SR-on, 1 means SR-off */
	bool keep_ops = true;

	/* Reset SR */
	*sr = banksr;

	/* Sanity Check */
	if (mtkpasr_status != MTKPASR_DISABLINGSR) {
		mtkpasr_err("Error Current State [%d]!\n", mtkpasr_status);
		return MTKPASR_WRONG_STATE;
	}

	/* Any incoming wakeup sources? */
	if (CHECK_PENDING_WAKEUP) {
		mtkpasr_log("Pending Wakeup Sources!\n");
		return MTKPASR_GET_WAKEUP;
	}

	/* Scan banks */
	compute_bank_inused(0);

	for (i = 0; i < num_ranks; i++) {
		mtkpasr_ranks[i].inused = 0;
		for (j = mtkpasr_ranks[i].start_bank; j <= mtkpasr_ranks[i].end_bank; j++) {
			if (mtkpasr_banks[j].inused != 0) {
				++mtkpasr_ranks[i].inused;
			}
		}
	}

	/* Check whether a full rank is cleared */
	for (i = 0; i < num_ranks; i++) {
		if (mtkpasr_ranks[i].inused == 0) {
			mtkpasr_info("DPD!\n");
			/* Set MTKPASR_DPDON */
			mtkpasr_ranks[i].inused = MTKPASR_DPDON;
			/* Set all related banks as MTKPASR_RDPDON */
			for (j = mtkpasr_ranks[i].start_bank; j <= mtkpasr_ranks[i].end_bank; j++) {
#ifdef CONFIG_MTKPASR_MAFL
				/* Test whether it can be removed from buddy temporarily */
				remove_bank_from_buddy(j);
#endif
				mtkpasr_banks[j].inused = MTKPASR_RDPDON;
				/* Disable its SR */
				banksr = banksr | (0x1 << (mtkpasr_banks[j].segment /*& MTKPASR_SEGMENT_CH0*/));
			}
			enter_dpd = 1;
		}
	}

	/* Check whether "other" banks are cleared */
	for (i = 0; i < num_banks; i++) {
		if (mtkpasr_banks[i].inused == 0) {
			/* Set MTKPASR_SROFF */
			mtkpasr_banks[i].inused = MTKPASR_SROFF;
			/* Disable its SR */
			banksr = banksr | (0x1 << (mtkpasr_banks[i].segment /*& MTKPASR_SEGMENT_CH0*/));
#ifdef CONFIG_MTKPASR_MAFL
			/* Test whether it can be removed from buddy temporarily */
			remove_bank_from_buddy(i);
#endif
			mtkpasr_info("SPM SR/OFF[%d]!\n", i);
		} else {
			mtkpasr_log("Bank[%d] %s[%d]!\n", i, (mtkpasr_banks[i].inused == MTKPASR_RDPDON) ? "RDPDON":"inused", mtkpasr_banks[i].inused);
		}

		/* To check whether we should do aggressive PASR SW in the future(no external compression) */
		if (keep_ops) {
			if (mtkpasr_banks[i].comp_pos != 0) {
				keep_ops = false;
			}
		}
	}

	/* Go to MTKPASR_ON state if success */
	if (result == MTKPASR_SUCCESS) {
		mtkpasr_status = enter_dpd ? MTKPASR_DPD_ON : MTKPASR_ON;
		*sr = banksr;
	}

#ifdef CONFIG_MTKPASR_MAFL
	/* Update strategy control */
	if (before_mafl_count == mafl_total_count) {	/* Ops-invariant */
		/* It hints not to do ops */
		if (!keep_ops) {
			mtkpasr_ops_invariant = KEEP_NO_OPS;
		}
		/* Check whether it is hard to apply PASR :( */
		if (mtkpasr_ops_invariant != KEEP_NO_OPS) {
			++mtkpasr_ops_invariant;
			if (mtkpasr_ops_invariant > MAX_NO_OPS_INVARIANT) {
				mtkpasr_ops_invariant = MAX_OPS_INVARIANT;
			}
		}
	} else {
		mtkpasr_ops_invariant = 0;
	}
	prev_mafl_count = mafl_total_count;
#endif

	mtkpasr_log("Ops_invariant[%u] result [%s] mtkpasr_status [%s]\n", mtkpasr_ops_invariant,
			(result == MTKPASR_SUCCESS) ? "MTKPASR_SUCCESS" : "MTKPASR_FAIL",
			(enter_dpd == 1) ? "MTKPASR_DPD_ON" : "MTKPASR_ON");

	return result;
}

static struct page *compacting_alloc(struct page *migratepage, unsigned long data, int **result)
{
	struct page *page = NULL;

	if (!list_empty(&tolist)) {
		page = list_entry(tolist.next, struct page, lru);
		list_del(&page->lru);
		--tolist_count;
	}

	return page;
}

/* Collect free pages - Is it too long?? */
static int collect_free_pages_for_compacting_banks(struct mtkpasr_bank_cc *bcc)
{
	struct page *page, *end_page;
	struct zone *z;
	unsigned long flags;
	int found;

	/* Sanity check - 20131126 */
	if (bcc->to_cursor == 0) {
		return tolist_count;
	}

	/* We have enough free pages */
	if (tolist_count >= fromlist_count) {
		return tolist_count;
	}

	/* To gather free pages */
	page = pfn_to_page(bcc->to_cursor);
	z = page_zone(page);
	end_page = pfn_to_page(mtkpasr_banks[bcc->to_bank].end_pfn);
	while (page < end_page) {
		/* Lock this zone */
		spin_lock_irqsave(&z->lock, flags);
		/* Find free pages */
		if (!PageBuddy(page)) {
			spin_unlock_irqrestore(&z->lock, flags);
			++page;
			continue;
		}
		found = pasr_find_free_page(page, &tolist);
		/* Unlock this zone */
		spin_unlock_irqrestore(&z->lock, flags);
		/* Update found */
		tolist_count += found;
		page += found;
		/* Update to_cursor & inused */
		bcc->to_cursor += (page - pfn_to_page(bcc->to_cursor));
		BANK_INUSED(bcc->to_bank) += found;
		/* Enough free pages? */
		if (tolist_count >= fromlist_count) {
			break;
		}
		/* Is to bank full? */
		if (BANK_INUSED(bcc->to_bank) == mtkpasr_banks[bcc->to_bank].valid_pages) {
			bcc->to_cursor = 0;
			break;
		}
	}

	if (page == end_page) {
		mtkpasr_info("\"To\" bank[%d] is full!\n", bcc->to_bank);
		bcc->to_cursor = 0;
	}

	return tolist_count;
}

/* Release pages from freelist */
static unsigned long putback_free_pages(struct list_head *freelist)
{
	struct page *page, *next;
	unsigned long count = 0;

	list_for_each_entry_safe(page, next, freelist, lru) {
		list_del(&page->lru);
		__free_page(page);
		count++;
	}

	return count;
}

#define COMPACTING_COLLECT()										\
	{												\
		if (collect_free_pages_for_compacting_banks(&bank_cc) >= fromlist_count) {		\
			if (MIGRATE_PAGES(&fromlist, compacting_alloc, 0) != 0) {	\
				mtkpasr_log("(AC) Bank[%d] can't be cleared!\n", from);			\
				ret = -1;								\
				goto next;								\
			}										\
			/* Migration is done for this batch */						\
			fromlist_count = 0;								\
		} else {										\
			ret = 1;									\
			goto next;									\
		}											\
	}

/*
 * Migrate pages from "from" to "to"
 * =0, success
 * >0, success but no new free bank generated
 *     (Above two conditions will update cursors.)
 * <0, fail(only due to CHECK_PENDING_WAKEUP)
 */
static int compacting_banks(int from, int to, unsigned long *from_cursor, unsigned long *to_cursor)
{
	struct page *page;
	unsigned long fc = *from_cursor;
	unsigned long ec = mtkpasr_banks[from].end_pfn;
	int to_be_migrated = 0;
	int ret = 0;
	struct mtkpasr_bank_cc bank_cc = {
		.to_bank = to,
		.to_cursor = *to_cursor,
	};

	/* Any incoming wakeup sources? */
	if (CHECK_PENDING_WAKEUP) {
		mtkpasr_log("Pending Wakeup Sources!\n");
		return -EBUSY;
	}

	/* Do reset */
	if (list_empty(&fromlist)) {
		fromlist_count = 0;
	}
	if (list_empty(&tolist)) {
		tolist_count = 0;
	}

	/* Migrate MTKPASR_CHECK_MIGRATE pages per batch */
	while (fc < ec) {
		/* Any incoming wakeup sources? */
		if ((fc % MTKPASR_CHECK_ABORTED) == 0) {
			if (CHECK_PENDING_WAKEUP) {
				mtkpasr_log("Pending Wakeup Sources!\n");
				ret = -EBUSY;
				break;
			}
		}
		/* Scan inuse pages */
		page = pfn_to_page(fc);
		if (page_count(page) != 0) {
			/* Check whether this page is compressed */
			if (check_if_compressed(mtkpasr_banks[from].comp_start, mtkpasr_banks[from].comp_end, fc)) {
				++fc;
				continue;
			}
			/* To isolate it */
#ifdef CONFIG_MTKPASR_ALLEXTCOMP
			if (!mtkpasr_isolate_page(page, 0x0)) {
#else
			if (!mtkpasr_isolate_page(page)) {
#endif
				list_add(&page->lru, &fromlist);
				++fromlist_count;
				++to_be_migrated;
				--BANK_INUSED(from);
			} else {
				/* This bank can't be cleared! */
				mtkpasr_log("(BC) Bank[%d] can't be cleared!\n", from);
				ret = -1;
				break;
			}
		} else {
			++fc;
			continue;
		}
		/* To migrate */
		if ((to_be_migrated % MTKPASR_CHECK_MIGRATE) == 0) {
			if (!list_empty(&fromlist)) {
				COMPACTING_COLLECT();
			}
		}
		/* Is from bank empty (Earlier leaving condition than (fc == ec)) */
		if (BANK_INUSED(from) == 0) {
			fc = 0;
			if (!list_empty(&fromlist)) {
				ret = 1;
			} else {
				/* A new free bank is generated! */
				ret = 0;
			}
			break;
		}
		/* Update fc */
		++fc;
	}

	/* From bank is scanned completely (Should always be false!) */
	if (fc == ec) {
		mtkpasr_err("Should always be false!\n");
		fc = 0;
		if (!list_empty(&fromlist)) {
			ret = 1;
		} else {
			/* A new free bank is generated! */
			ret = 0;
		}
	}

	/* Complete remaining compacting */
	if (ret > 0 && !list_empty(&fromlist)) {
		COMPACTING_COLLECT();
	}

next:
	/* Should we put all pages in fromlist back */
	if (ret == -1) {
		/* We should put all pages from fromlist back */
		putback_lru_pages(&fromlist);
		fromlist_count = 0;								\
		/* We can't clear this bank. Go to the next one! */
		fc = 0;
		ret = 1;
	}

	/* Update cursors */
	*from_cursor = fc;
	*to_cursor = bank_cc.to_cursor;

	return ret;
}

/* Main entry of compacting banks (No compaction on the last one(modem...)) */
static enum mtkpasr_phase mtkpasr_compact_banks(int toget)
{
	int to_be_sorted = num_banks;
	int dsort_banks[to_be_sorted];
	int i, j, tmp, ret;
	unsigned long from_cursor = 0, to_cursor = 0;
	enum mtkpasr_phase result = MTKPASR_SUCCESS;

	/* Any incoming wakeup sources? */
	if (CHECK_PENDING_WAKEUP) {
		mtkpasr_log("Pending Wakeup Sources!\n");
		return MTKPASR_GET_WAKEUP;
	}

	/* Initialization */
	for (i = 0; i < to_be_sorted; ++i) {
		dsort_banks[i] = i;
	}

	/* Sorting banks */
	for (i = to_be_sorted; i > 1; --i) {
		for (j = 0; j < i-1; ++j) {
			/* By rank (descending) */
			if (BANK_RANK(dsort_banks[j]) < BANK_RANK(dsort_banks[j+1])) {
				continue;
			}
			/* By inused (descending) */
			if (BANK_INUSED(dsort_banks[j]) < BANK_INUSED(dsort_banks[j+1])) {
				tmp = dsort_banks[j];
				dsort_banks[j] = dsort_banks[j+1];
				dsort_banks[j+1] = tmp;
			}
		}
	}

#ifdef CONFIG_MTKPASR_DEBUG
	for (i = 0; i < to_be_sorted; ++i) {
		mtkpasr_info("[%d] - (%d) - inused(%d) - rank(%p)\n", i, dsort_banks[i], BANK_INUSED(dsort_banks[i]), BANK_RANK(dsort_banks[i]));
	}
#endif

	/* Go through banks */
	i = to_be_sorted - 1;	/* from-bank */
	j = 0;			/* to-bank */
	while (i > j) {
		/* Whether from-bank is empty */
		if (BANK_INUSED(dsort_banks[i]) == 0) {
			--i;
			continue;
		}
		/* Whether to-bank is full */
		if (BANK_INUSED(dsort_banks[j]) == mtkpasr_banks[dsort_banks[j]].valid_pages) {
			++j;
			continue;
		}
		/* Set compacting position if needed */
		if (!from_cursor) {
			from_cursor = mtkpasr_banks[dsort_banks[i]].start_pfn;
#ifdef CONFIG_MTKPASR_MAFL
			from_cursor += mtkpasr_banks[dsort_banks[i]].inmafl;
#endif
		}
		if (!to_cursor) {
#ifdef CONFIG_MTKPASR_MAFL
			/* Shrinking (remaining) mafl totally */
			shrink_mafl_all(dsort_banks[j]);
#endif
			to_cursor = mtkpasr_banks[dsort_banks[j]].start_pfn;
		}
		/* Start compaction on banks */
		ret = compacting_banks(dsort_banks[i], dsort_banks[j], &from_cursor, &to_cursor);
		if (ret >= 0) {
			if (!from_cursor) {
				--i;
			}
			if (!to_cursor) {
				++j;
			}
			if (!ret) {
				--toget;
			} else {
				continue;
			}
		} else {
			/* Error occurred! */
			mtkpasr_err("Error occurred during the compacting on banks!\n");
			if (ret == -EBUSY) {
				result = MTKPASR_GET_WAKEUP;
			} else {
				result = MTKPASR_FAIL;
#ifdef CONFIG_MTKPASR_DEBUG
				BUG();
#endif
			}
			break;
		}
		/* Should we stop the compaction! */
		if (!toget) {
			break;
		}
	}

	/* Putback & release pages if needed */
	putback_lru_pages(&fromlist);
	fromlist_count = 0;
	if (putback_free_pages(&tolist) != tolist_count) {
		mtkpasr_err("Should be the same!\n");
	}
	tolist_count = 0;

	return result;
}

/* Apply compaction on banks */
static enum mtkpasr_phase mtkpasr_compact(void)
{
	int i;
	int no_compact = 0;
	int total_free = 0;
	int free_banks;

	/* Scan banks for inused */
	compute_bank_inused(0);

	/* Any incoming wakeup sources? */
	if (CHECK_PENDING_WAKEUP) {
		mtkpasr_log("Pending Wakeup Sources!\n");
		return MTKPASR_GET_WAKEUP;
	}

	/* Check whether we should do compaction on banks */
	for (i = 0; i < num_banks; ++i) {
		total_free += (mtkpasr_banks[i].valid_pages - mtkpasr_banks[i].inused);
		if (mtkpasr_banks[i].inused == 0) {
			++no_compact;
		}
	}

	/* How many free banks we could get */
	free_banks = total_free / pages_per_bank;
	if (no_compact >= free_banks) {
		mtkpasr_info("No need to do compaction on banks!\n");
		/* No need to do compaction on banks */
		return MTKPASR_SUCCESS;
	}

	/* Actual compaction on banks */
	return mtkpasr_compact_banks(free_banks - no_compact);
}

/*
 * Return the number of pages which need to be compressed & do reset.
 *
 * src_map	- store the pages' pfns which need to be compressed.
 * start	- Start PFN of current scanned bank
 * end		- End PFN of current scanned bank
 * sorted	- Buffer for recording which bank(by offset) is externally compressed.
 */
static int pasr_scan_memory(void *src_map, unsigned long start, unsigned long end, unsigned long *sorted)
{
	unsigned long start_pfn;
	unsigned long end_pfn;
	struct page *page;
	unsigned long *pgmap = (unsigned long *)src_map;
	int need_compressed = 0;

	/* Initialize start/end */
	start_pfn = start;
	end_pfn = end;

	/* We don't need to go through following loop because there is no page to be processed. */
	if (start == end) {
		return need_compressed;
	}

	/* Start to scan inuse pages */
	do {
		page = pfn_to_page(start_pfn);
		if (page_count(page) != 0) {
			*pgmap++ = start_pfn;
			++need_compressed;
		}
	} while (++start_pfn < end_pfn);

#ifndef CONFIG_64BIT
	/* Clear sorted (we only need to clear (end-start) entries.)*/
	memset(sorted, 0, (end-start)*sizeof(unsigned long));
#endif

	mtkpasr_info("@@@ start_pfn[0x%lx] end_pfn[0x%lx] - to process[%d] @@@\n", start, end, need_compressed);

	return need_compressed;
}

#ifndef CONFIG_64BIT
/* Implementation of MTKPASR Direct Compression! DON'T MODIFY IT.  */
#define MTKPASR_DIRECT_COMPRESSION()							\
	{										\
		ret = 0;								\
		if (!trylock_page(page)) {						\
			mtkpasr_err("FL!\n");						\
			ret = -1;							\
			break;								\
		}									\
		if (likely(compressed < MTKPASR_MAX_EXTCOMP)) {				\
			/* Next MTKPASR slot */						\
			current_index = atomic_inc_return(&sloti);			\
			/* Forward page to MTKPASR pool */				\
			ret = mtkpasr_forward_rw(mtkpasr, current_index, page , WRITE);	\
			/* Unsuccessful compression? */					\
			if (unlikely(ret)) {						\
				unlock_page(page);					\
				atomic_dec(&sloti);					\
				mtkpasr_err("FFRW!\n");					\
				break;							\
			}								\
			/* Record the pfn for external compression */			\
			sorted_for_extcomp[page_to_pfn(page)-bank_start_pfn] = 1;	\
			++compressed;							\
		}									\
		unlock_page(page);							\
	}
#else
#define MTKPASR_DIRECT_COMPRESSION()	do { ret = 0; } while (0);
#endif

/*
 * Drop, Compress, Migration, Compaction
 *
 * Return - MTKPASR_WRONG_STATE
 *          MTKPASR_GET_WAKEUP
 *	    MTKPASR_SUCCESS
 */
enum mtkpasr_phase mtkpasr_entering(void)
{
#define SAFE_CONDITION_CHECK() {										\
		/* Is there any incoming ITRs from wake-up sources? If yes, then abort it. */			\
		if (CHECK_PENDING_WAKEUP) {									\
			mtkpasr_log("Pending Wakeup Sources!\n");						\
			ret = -EBUSY;										\
			break;											\
		}												\
		/* Check whether current system is safe! */							\
		if (unlikely(pasr_check_free_safe())) {								\
			mtkpasr_log("Unsafe System Status!\n");							\
			ret = -1;										\
			break;											\
		}												\
		/* Check whether mtkpasr_admit_order is not negative! */					\
		if (mtkpasr_admit_order < 0) {									\
			mtkpasr_log("mtkpasr_admit_order is negative!\n");					\
			ret = -1;										\
			break;											\
		}												\
	}

	int ret = 0;
	struct mtkpasr *mtkpasr;
	struct page *page;
	int current_bank, current_pos;
	unsigned long bank_start_pfn, bank_end_pfn;
#ifndef CONFIG_64BIT
	int current_index;
	unsigned long which_pfn;
#endif
#ifdef CONFIG_MTKPASR_DEBUG
	int drop_cnt, to_be_migrated, splitting, no_migrated;
#endif
	int compressed = 0, val;
	unsigned long *start;
	int pgmi;
	LIST_HEAD(to_migrate);
	LIST_HEAD(batch_to_migrate);
	enum mtkpasr_phase result = MTKPASR_SUCCESS;
	struct zone *zone;
	unsigned long isolated_file, isolated_anon;
	long bias_file, bias_anon;

	/* Sanity Check */
	if (mtkpasr_status != MTKPASR_OFF) {
		mtkpasr_err("Error Current State [%d]!\n", mtkpasr_status);
		return MTKPASR_WRONG_STATE;
	} else {
		/* Go to MTKPASR_ENTERING state */
		mtkpasr_status = MTKPASR_ENTERING;
	}

	/* Any incoming wakeup sources? */
	if (CHECK_PENDING_WAKEUP) {
		mtkpasr_log("Pending Wakeup Sources!\n");
		return MTKPASR_GET_WAKEUP;
	}

	/* Reset "CROSS-OPS" variables: extcomp position index, extcomp start & end positions */
	atomic_set(&sloti, -1);
#ifndef CONFIG_64BIT
	for (current_bank = 0; current_bank < num_banks; ++current_bank) {
		mtkpasr_banks[current_bank].comp_pos = 0;
	}
#endif

#ifdef CONFIG_MTKPASR_MAFL
	/* Set for verification of ops-invariant */
	before_mafl_count = mafl_total_count;

	/* Transition-invariant */
	if (prev_mafl_count == mafl_total_count) {
		if (mtkpasr_no_ops()) {
			/* Go to the next state */
			mtkpasr_status = MTKPASR_DISABLINGSR;
			goto fast_path;
		}
	} else {
		/* Transition is variant. Clear ops-invariant to proceed it.*/
		mtkpasr_ops_invariant = 0;
	}
#endif

	/* Preliminary work before actual PASR SW operation */
	MTKPASR_FLUSH();

	/*****************************/
	/* PASR SW Operation starts! */
	/*****************************/

#ifdef CONFIG_MTKPASR_DEBUG
	drop_cnt = to_be_migrated = splitting = no_migrated = 0;
#endif

	/* Record original number of zone isolated pages! IMPORTANT! */
	zone = MTKPASR_ZONE;
	isolated_file = zone_page_state(zone, NR_ISOLATED_FILE);
	isolated_anon = zone_page_state(zone, NR_ISOLATED_ANON);

	/* Indicate start bank */
#ifndef CONFIG_MTKPASR_RDIRECT
	current_bank = 0;
#else
	current_bank = num_banks - 1;
#endif

	/* Check whether current system is safe! */
	if (unlikely(pasr_check_free_safe())) {
		mtkpasr_log("Unsafe System Status!\n");
		goto no_safe;
	}

	/* Current admit order */
	mtkpasr_admit_order = pasr_compute_safe_order();
	mtkpasr_info("mtkpasr_admit_order is [%d]\n", mtkpasr_admit_order);

	/* Main thread is here */
	mtkpasr = &mtkpasr_device[0];

	/* Indicate mtkpasr_last_scan */
	mtkpasr_last_scan = mtkpasr_start_pfn - pageblock_nr_pages;

next_bank:
#ifndef CONFIG_64BIT
	/* Set start pos at extcomp */
	mtkpasr_banks[current_bank].comp_start = (s16)compressed;
#endif

	/* Scan MTKPASR-imposed pages */
#ifdef CONFIG_MTKPASR_MAFL
	bank_start_pfn = mtkpasr_banks[current_bank].start_pfn + mtkpasr_banks[current_bank].inmafl;
#else
	bank_start_pfn = mtkpasr_banks[current_bank].start_pfn;
#endif
	bank_end_pfn = mtkpasr_banks[current_bank].end_pfn;
	val = pasr_scan_memory(src_pgmap, bank_start_pfn, bank_end_pfn, sorted_for_extcomp);
	start = src_pgmap;

	/* Reset scan index */
	pgmi = -1;

	/* Reset ret!(Important) */
	ret = 0;

	/* Start compression, dropping & migration */
	current_pos = ++pgmi;
	while (current_pos < val) {
		/* Don't process CHECK_PENDING_WAKEUP in every loop(It maybe time-consuming!) */
		/* Don't process pasr_check_free_safe in every loop(It maybe time-consuming!) */
		if ((current_pos % MTKPASR_CHECK_MIGRATE) == 0) {
			SAFE_CONDITION_CHECK();
		}
		/* Query & process */
		page = pfn_to_page(start[current_pos]);
		if (page != NULL) {
			/* To compress: !PageLRU, PageUnevictable, !page_evictable */
			if (!PageLRU(page) || PageUnevictable(page) || !PAGE_EVICTABLE(page, NULL)) {
				MTKPASR_DIRECT_COMPRESSION();
				goto next_page;
			}
			/* To drop */
			ret = mtkpasr_drop_page(page);
			if (ret == 0) {
#ifdef CONFIG_MTKPASR_DEBUG
				++drop_cnt;
#endif
			} else if (ret == -EAGAIN) {
				ret = -1;
				break;
			} else {
				if (unlikely(ret == -EACCES)) {
					/* This kind of pages still in LRU or not evictable! */
					MTKPASR_DIRECT_COMPRESSION();
				} else {
					/* This kind of pages are removed from LRU! Link failedly-dropped pages together & prepare migration! */
					list_add_tail(&page->lru, &to_migrate);
#ifdef CONFIG_MTKPASR_DEBUG
					++to_be_migrated;
#endif
					/* Reset ret!(Important) */
					ret = 0;
				}
			}
		}
next_page:
		current_pos = ++pgmi;
	}

	/* To migrate */
	while (!ret && !list_empty(&to_migrate)) {
		struct list_head *this, *split_start;
		/* Select MTKPASR_CHECK_MIGRATE pages */
		current_pos = 0;
		split_start = to_migrate.next;
		list_for_each(this, &to_migrate) {
			++current_pos;
			if (current_pos == MTKPASR_CHECK_MIGRATE) {
				break;
			}
		}
		/* Check should be aborted */
		SAFE_CONDITION_CHECK();
		/* Split pages */
		if (current_pos == MTKPASR_CHECK_MIGRATE) {
			to_migrate.next = this->next;
			this->next->prev = &to_migrate;
			batch_to_migrate.next = split_start;
			split_start->prev = &batch_to_migrate;
			batch_to_migrate.prev = this;
			this->next = &batch_to_migrate;
			this = &batch_to_migrate;
		} else {
			this = &to_migrate;
		}
#ifdef CONFIG_MTKPASR_DEBUG
		splitting += current_pos;
#endif
		/* Migrate pages */
		if (MIGRATE_PAGES(this, mtkpasr_alloc, 0)) {
			/* Failed migration on remaining pages! No list add/remove operations! */
			list_for_each_entry(page, this, lru) {
#ifdef CONFIG_MTKPASR_DEBUG
				++no_migrated;
#endif
				MTKPASR_DIRECT_COMPRESSION();
				/* Clear ret here! No needs to leave this loop due to fail compression */
				ret = 0;
			}
			putback_lru_pages(this);
		}
		/* Check should be aborted */
		SAFE_CONDITION_CHECK();
	}

	/* Put remaining pages back! */
	putback_lru_pages(&batch_to_migrate);
	putback_lru_pages(&to_migrate);

#ifndef CONFIG_64BIT
	/* Set pos next to the last one at extcomp */
	mtkpasr_banks[current_bank].comp_end = (s16)compressed;
	mtkpasr_info("bank[%d] - comp_start[%d] comp_end[%d]\n",
			current_bank, mtkpasr_banks[current_bank].comp_start, mtkpasr_banks[current_bank].comp_end);

	/* Update extcomp if needed */
	if (mtkpasr_banks[current_bank].comp_start < mtkpasr_banks[current_bank].comp_end) {
		which_pfn = 0;
		bank_end_pfn = bank_end_pfn - bank_start_pfn;
		start = extcomp + mtkpasr_banks[current_bank].comp_start;
		do {
			/* Is this page compressed */
			if (sorted_for_extcomp[which_pfn] == 1) {
				*start++ = which_pfn + bank_start_pfn;
			}
		} while (++which_pfn < bank_end_pfn);

#ifdef CONFIG_MTKPASR_DEBUG
		/* Sanity check */
		if (start != (extcomp + compressed)) {
			mtkpasr_err("\n\n\n\n\n\n Oh no!\n\n\n\n\n\n");
		}
#endif
	}
#endif

	/* Check whether we should go to the next bank */
	/* Because PASR only takes effect on continuous PASR banks, we should add "!ret" to avoid unnecessary works */
#ifndef CONFIG_MTKPASR_RDIRECT
	if (!ret && (++current_bank) < num_banks)
#else
	if (!ret && (--current_bank) >= 0)
#endif
		goto next_bank;

	/* Updated to be the position next to the last occupied slot */
	atomic_inc(&sloti);

	/* Put remaining destination pages back! */
#ifdef CONFIG_MTKPASR_DEBUG
	if (putback_free_pages(&tolist) != tolist_count) {
		mtkpasr_err("Should be the same!\n");
	}
	tolist_count = 0;
#else
	putback_free_pages(&tolist);
#endif

	/* Check MTKPASR result */
	if (ret == -EBUSY) {
		mtkpasr_log("Failed MTKPASR due to pending wakeup source!\n");
		/* Some error handling: It means failed to enter PASR! - Need to enter MTKPASR_RESTORING */
		result = MTKPASR_GET_WAKEUP;
	} else if (ret == -1) {
		mtkpasr_log("Failed compression or no safe amount of free space! Go ahead to SPM!\n");
	}

no_safe:
	/* Go to MTKPASR_DISABLINGSR state if success */
	if (result == MTKPASR_SUCCESS) {
		/* Migrate to non-PASR range is not feasible(Means there may be some movable pages), so we should compact banks for PASR. */
		if (mtkpasr_admit_order < 0)
			result = mtkpasr_compact();
		/* Successful PASR ops */
		if (result == MTKPASR_SUCCESS) {
			/* Go to the next state */
			mtkpasr_status = MTKPASR_DISABLINGSR;
		}
		/* This should be called whether it is a successful PASR?? IMPORTANT! */
		flush_cache_all();
	}

	/* Recover zone isolate statistics to original ones! IMPORTANT! */
	bias_file = isolated_file - zone_page_state(zone, NR_ISOLATED_FILE);
	bias_anon = isolated_anon - zone_page_state(zone, NR_ISOLATED_ANON);
	mod_zone_page_state(zone, NR_ISOLATED_FILE, bias_file);
	mod_zone_page_state(zone, NR_ISOLATED_ANON, bias_anon);

#ifdef CONFIG_MTKPASR_DEBUG
	mtkpasr_info("dropped [%d] - compressed [%d] - to_be_migrated [%d] - splitting [%d] - no_migrated [%d]\n"
			, drop_cnt, compressed, to_be_migrated, splitting, no_migrated);
#endif

#ifdef CONFIG_MTKPASR_MAFL
fast_path:
#endif

	mtkpasr_log("result [%s]\n\n", (result == MTKPASR_SUCCESS) ? "MTKPASR_SUCCESS" : ((result == MTKPASR_FAIL) ? "MTKPASR_FAIL" : "MTKPASR_GET_WAKEUP"));

	return result;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
/* Early suspend/resume callbacks & descriptor */
static void mtkpasr_early_suspend(struct early_suspend *h)
{
	mtkpasr_info("\n");
}

static void mtkpasr_late_resume(struct early_suspend *h)
{
	mtkpasr_info("\n");
	shrink_mtkpasr_late_resume();
}

static struct early_suspend mtkpasr_early_suspend_desc = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1,
	.suspend = mtkpasr_early_suspend,
	.resume = mtkpasr_late_resume,
};
#endif

/* Reset */
void __mtkpasr_reset_device(struct mtkpasr *mtkpasr)
{
	mtkpasr->init_done = 0;

	/* Free various per-device buffers */
	if (mtkpasr->compress_workmem != NULL) {
		kfree(mtkpasr->compress_workmem);
		mtkpasr->compress_workmem = NULL;
	}
	if (mtkpasr->compress_buffer != NULL) {
		free_pages((unsigned long)mtkpasr->compress_buffer, 1);
		mtkpasr->compress_buffer = NULL;
	}
}

void mtkpasr_reset_device(struct mtkpasr *mtkpasr)
{
	down_write(&mtkpasr->init_lock);
	__mtkpasr_reset_device(mtkpasr);
	up_write(&mtkpasr->init_lock);
}

void mtkpasr_reset_global(void)
{
	mtkpasr_reset_slots();

	/* Free table */
	kfree(mtkpasr_table);
	mtkpasr_table = NULL;
	mtkpasr_disksize = 0;

	/* Destroy pool */
	zs_destroy_pool(mtkpasr_mem_pool);
	mtkpasr_mem_pool = NULL;
}

int mtkpasr_init_device(struct mtkpasr *mtkpasr)
{
	int ret;

	down_write(&mtkpasr->init_lock);

	if (mtkpasr->init_done) {
		up_write(&mtkpasr->init_lock);
		return 0;
	}

	mtkpasr->compress_workmem = kzalloc(LZO1X_MEM_COMPRESS, GFP_KERNEL);
	if (!mtkpasr->compress_workmem) {
		pr_err("Error allocating compressor working memory!\n");
		ret = -ENOMEM;
		goto fail;
	}

	mtkpasr->compress_buffer = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 1);
	if (!mtkpasr->compress_buffer) {
		pr_err("Error allocating compressor buffer space\n");
		ret = -ENOMEM;
		goto fail;
	}

	mtkpasr->init_done = 1;
	up_write(&mtkpasr->init_lock);

	pr_debug("Initialization done!\n");
	return 0;

fail:
	__mtkpasr_reset_device(mtkpasr);
	up_write(&mtkpasr->init_lock);
	pr_err("Initialization failed: err=%d\n", ret);
	return ret;
}

/* Adjust 1st rank to exclude Normal zone! */
static void __init mtkpasr_construct_bankrank(void)
{
	int bank, rank, hwrank, prev;
	unsigned long spfn, epfn;

	/******************************/
	/* PASR default imposed range */
	/******************************/

	/* Reset total pfns */
	mtkpasr_total_pfns = 0;

	/* Setup PASR/DPD bank/rank information */
	for (bank = 0, rank = -1, prev = -1; bank < num_banks; ++bank) {
		/* Construct basic bank/rank information */
		hwrank = query_bank_information(bank, &spfn, &epfn, true);
		mtkpasr_banks[bank].start_pfn = spfn;
		mtkpasr_banks[bank].end_pfn = epfn;
		mtkpasr_banks[bank].inused = 0;
		mtkpasr_banks[bank].segment = pasr_bank_to_segment(spfn, epfn);
		mtkpasr_banks[bank].comp_pos = 0;
		if (hwrank >= 0) {
			if (prev != hwrank) {
				++rank;
				/* Update rank */
				mtkpasr_ranks[rank].start_bank = (u16)bank;
				mtkpasr_ranks[rank].hw_rank = hwrank;
				mtkpasr_ranks[rank].inused = 0;
			}
			mtkpasr_banks[bank].rank = &mtkpasr_ranks[rank];
			mtkpasr_ranks[rank].end_bank = (u16)bank;		/* Update rank */
			prev = hwrank;
		} else {
			mtkpasr_banks[bank].rank = NULL;			/* No related rank! */
		}

#ifdef CONFIG_MTKPASR_MAFL
		/* Mark it As Free by removing pages from buddy allocator to its List */
		INIT_LIST_HEAD(&mtkpasr_banks[bank].mafl);
		mtkpasr_banks[bank].inmafl = 0;
#endif
		/*
		 * "Simple" adjustment on banks to exclude invalid PFNs
		 */
		spfn = mtkpasr_banks[bank].start_pfn;
		epfn = mtkpasr_banks[bank].end_pfn;

		/* Find out the 1st valid PFN */
		while (spfn < epfn) {
			if (pfn_valid(spfn)) {
				mtkpasr_banks[bank].start_pfn = spfn++;
				break;
			}
			++spfn;
		}

		/* From spfn, to find out the 1st invalid PFN */
		while (spfn < epfn) {
			if (!pfn_valid(spfn)) {
				mtkpasr_banks[bank].end_pfn = spfn;
				break;
			}
			++spfn;
		}

		/* Update valid_pages */
		mtkpasr_banks[bank].valid_pages = mtkpasr_banks[bank].end_pfn - mtkpasr_banks[bank].start_pfn;

		/* To fix mtkpasr_total_pfns(only contain valid pages) */
		mtkpasr_total_pfns += mtkpasr_banks[bank].valid_pages;

		mtkpasr_info("Bank[%d] - start_pfn[0x%lx] end_pfn[0x%lx] valid_pages[%u] rank[%p]\n",
				bank, mtkpasr_banks[bank].start_pfn, mtkpasr_banks[bank].end_pfn,
				mtkpasr_banks[bank].valid_pages, mtkpasr_banks[bank].rank);
	}

	/* To compute average pages per bank */
	pages_per_bank = mtkpasr_total_pfns / num_banks;

	/**************************/
	/* PASR non-imposed range */
	/**************************/

#ifdef CONFIG_MTKPASR_MAFL
	/* Try to remove some pages from buddy to enhance the PASR performance */
	compute_bank_inused(0);

	/* Reserve all first */
	for (bank = 0; bank < num_banks; bank++) {
		if (mtkpasr_banks[bank].inused == 0) {
			remove_bank_from_buddy(bank);
			pr_notice("(+)bank[%d]\n",bank);
		} else {
			pr_notice("(-)bank[%d] inused[%u]\n",bank,mtkpasr_banks[bank].inused);
		}
	}

	prev_mafl_count = mafl_total_count;
#endif

	/* Sanity check - confirm all pages in MTKPASR banks are MIGRATE_MTKPASR */
	rank = 0;
	for (bank = 0; bank < num_banks; bank++) {
		spfn = mtkpasr_banks[bank].start_pfn;
		epfn = mtkpasr_banks[bank].end_pfn;
		for (; spfn < epfn; spfn += pageblock_nr_pages)
			if (!is_migrate_mtkpasr(get_pageblock_migratetype(pfn_to_page(spfn))))
				rank++;
	}
	if (rank != 0)
		pr_alert("\n\n\n[%s][%d]: There is non-MIGRATE_MTKPASR page in MTKPASR range!!!\n\n\n",__func__,__LINE__);

	pr_notice("Non-MIGRATE_MTKPASR pages in MTKPASR range [%d]\n",rank);
}

#ifdef CONFIG_PM

static int mtkpasr_pm_event(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	switch(pm_event) {
	case PM_HIBERNATION_PREPARE: 	/* Going to hibernate */
		/* MTKPASR off */
		pm_in_hibernation = true;
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION: 	/* Hibernation finished */
		/* MTKPASR on */
		pm_in_hibernation = false;
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block mtkpasr_pm_notifier_block = {
	.notifier_call = mtkpasr_pm_event,
	.priority = 0,
};

extern unsigned long mtkpasr_triggered;
extern unsigned long failed_mtkpasr;
static int mtkpasr_syscore_suspend(void)
{
	enum mtkpasr_phase result;
	int ret = 0;
	int irq_disabled = 0;		/* MTKPASR_FLUSH -> drain_all_pages -> on_each_cpu_mask will enable local irq */

	IS_MTKPASR_ENABLED;

	/* If system is currently in hibernation, just return. */
	if (pm_in_hibernation == true) {
		mtkpasr_log("In hibernation!\n");
		return 0;
	}

	/* Setup SPM wakeup event firstly */
	spm_set_wakeup_src_check();
	
	/* Check whether we are in irq-disabled environment */
	if (irqs_disabled()) {
		irq_disabled = 1;
	}

	/* Count for every trigger */
	++mtkpasr_triggered;

	/* It will go to MTKPASR stage */
	current->flags |= PF_MTKPASR | PF_SWAPWRITE;
	
	/* RAM-to-RAM compression - State change: MTKPASR_OFF -> MTKPASR_ENTERING -> MTKPASR_DISABLINGSR */
	result = mtkpasr_entering();
	
	/* It will leave MTKPASR stage */
	current->flags &= ~(PF_MTKPASR | PF_SWAPWRITE);
	
	/* Any pending wakeup source? */
	if (result == MTKPASR_GET_WAKEUP) {
		mtkpasr_restoring();
		mtkpasr_err("PM: Failed to enter MTKPASR\n");
		++failed_mtkpasr;
		ret = -1;
	} else if (result == MTKPASR_WRONG_STATE) {
		mtkpasr_reset_state();
		mtkpasr_err("Wrong state!\n");
		++failed_mtkpasr;
	}

	/* Recover it to irq-disabled environment if needed */
	if (irq_disabled == 1) {
		if (!irqs_disabled()) {
			mtkpasr_log("IRQ is enabled! To disable it here!\n");
			arch_suspend_disable_irqs();
		}
	}

	return ret;
}

static void mtkpasr_syscore_resume(void)
{
	enum mtkpasr_phase result;
	
	/* If system is currently in hibernation, just return. */
	if (pm_in_hibernation == true) {
		mtkpasr_log("In hibernation!\n");
		return;
	}

	/* RAM-to-RAM decompression - State change: MTKPASR_EXITING -> MTKPASR_OFF */
	result = mtkpasr_exiting();

	if (result == MTKPASR_WRONG_STATE) {
		mtkpasr_reset_state();
		mtkpasr_err("Wrong state!\n");
	} else if (result == MTKPASR_FAIL) {
		printk(KERN_ERR"\n\n\n Some Fatal Error!\n\n\n");
	}
}

static struct syscore_ops mtkpasr_syscore_ops = {
	.suspend	= mtkpasr_syscore_suspend,
	.resume		= mtkpasr_syscore_resume,
};

static int __init mtkpasr_init_ops(void)
{
	if (!register_pm_notifier(&mtkpasr_pm_notifier_block))
		register_syscore_ops(&mtkpasr_syscore_ops);
	else
		mtkpasr_err("Failed to register pm notifier block\n");
		
	return 0;
}
#endif

/* mtkpasr initcall */
static int __init mtkpasr_init(void)
{
	int ret;

	/* MTKPASR table of slot information for external compression (MTKPASR_MAX_EXTCOMP) */
	mtkpasr_total_slots = MTKPASR_MAX_EXTCOMP + 1;	/* Leave a slot for buffering */
	mtkpasr_free_slots = mtkpasr_total_slots;
	mtkpasr_disksize = mtkpasr_total_slots << PAGE_SHIFT;
	mtkpasr_table = kzalloc(mtkpasr_total_slots * sizeof(*mtkpasr_table), GFP_KERNEL);
	if (!mtkpasr_table) {
		mtkpasr_err("Error allocating mtkpasr address table\n");
		ret = -ENOMEM;
		goto fail;
	}

	/* Create MTKPASR mempool. */
	mtkpasr_mem_pool = ZS_CREATE_POOL("mtkpasr", GFP_ATOMIC|__GFP_HIGHMEM);
	if (!mtkpasr_mem_pool) {
		mtkpasr_err("Error creating memory pool\n");
		ret = -ENOMEM;
		goto no_mem_pool;
	}

	/* We have only 1 mtkpasr device. */
	mtkpasr_device = kzalloc(sizeof(struct mtkpasr), GFP_KERNEL);
	if (!mtkpasr_device) {
		mtkpasr_err("Failed to create mtkpasr_device\n");
		ret = -ENOMEM;
		goto out;
	}

	/* Construct memory rank & bank information */
	num_banks = compute_valid_pasr_range(&mtkpasr_start_pfn, &mtkpasr_end_pfn, &num_ranks);
	if (num_banks < 0) {
		mtkpasr_err("No valid PASR range!\n");
		ret = -EINVAL;
		goto free_devices;
	}

	/* To allocate memory for src_pgmap if needed (corresponding to one bank size) */
	if (src_pgmap == NULL) {
		src_pgmap = (void *)__get_free_pages(GFP_KERNEL, get_order(pasrbank_pfns * sizeof(unsigned long)));
		if (src_pgmap == NULL) {
			mtkpasr_err("Failed to allocate (order:%d)(%ld) memory!\n", get_order(pasrbank_pfns * sizeof(unsigned long)), pasrbank_pfns);
			ret = -ENOMEM;
			goto free_devices;
		}
	}

	/* To allocate memory for keeping external compression information */
	if (extcomp == NULL && !!MTKPASR_MAX_EXTCOMP) {
		extcomp = (unsigned long *)__get_free_pages(GFP_KERNEL, get_order(MTKPASR_MAX_EXTCOMP * sizeof(unsigned long)));
		if (extcomp == NULL) {
			mtkpasr_err("Failed to allocate memory for extcomp!\n");
			ret = -ENOMEM;
			goto no_memory;
		}
		sorted_for_extcomp = (unsigned long *)__get_free_pages(GFP_KERNEL, get_order(pasrbank_pfns * sizeof(unsigned long)));
		if (sorted_for_extcomp == NULL) {
			free_pages((unsigned long)extcomp, get_order(MTKPASR_MAX_EXTCOMP * sizeof(unsigned long)));
			mtkpasr_err("Failed to allocate memory for extcomp!\n");
			ret = -ENOMEM;
			goto no_memory;
		}
	}

	/* Basic initialization */
	init_rwsem(&mtkpasr_device->init_lock);
	spin_lock_init(&mtkpasr_device->stat64_lock);
	mtkpasr_device->init_done = 0;

	/* Create working buffers */
	ret = mtkpasr_init_device(mtkpasr_device);
	if (ret < 0) {
		mtkpasr_err("Failed to initialize mtkpasr device\n");
		goto reset_devices;
	}

	/* Create SYSFS interface */
	ret = sysfs_create_group(power_kobj, &mtkpasr_attr_group);
	if (ret < 0) {
		mtkpasr_err("Error creating sysfs group\n");
		goto reset_devices;
	}

	/* mtkpasr_total_pfns = mtkpasr_end_pfn - mtkpasr_start_pfn; */
	mtkpasr_banks = kzalloc(num_banks * sizeof(struct mtkpasr_bank), GFP_KERNEL);
	if (!mtkpasr_banks) {
		mtkpasr_err("Error allocating mtkpasr banks information!\n");
		ret = -ENOMEM;
		goto free_banks_ranks;
	}
	mtkpasr_ranks = kzalloc(num_ranks * sizeof(struct mtkpasr_rank), GFP_KERNEL);
	if (!mtkpasr_ranks) {
		mtkpasr_err("Error allocating mtkpasr ranks information!\n");
		ret = -ENOMEM;
		goto free_banks_ranks;
	}

	mtkpasr_construct_bankrank();

	/* Indicate migration end */
	mtkpasr_migration_end = NODE_DATA(0)->node_start_pfn + pasrbank_pfns;

	mtkpasr_info("num_banks[%d] num_ranks[%d] mtkpasr_start_pfn[%ld] mtkpasr_end_pfn[%ld] mtkpasr_total_pfns[%ld]\n",
			num_banks, num_ranks, mtkpasr_start_pfn, mtkpasr_end_pfn, mtkpasr_total_pfns);

	/* Register early suspend/resume desc */
	register_early_suspend(&mtkpasr_early_suspend_desc);

#ifdef CONFIG_PM
	/* Register syscore_ops */
	mtkpasr_init_ops();
#endif

	/* Setup others */
	nz = &NODE_DATA(0)->node_zones[ZONE_NORMAL];
	safe_level = low_wmark_pages(nz);
#ifdef CONFIG_HIGHMEM
	safe_level += nz->lowmem_reserve[ZONE_HIGHMEM];
#endif
	safe_level = safe_level >> 2;

	return 0;

free_banks_ranks:
	if (mtkpasr_banks != NULL) {
		kfree(mtkpasr_banks);
		mtkpasr_banks = NULL;
	}
	if (mtkpasr_ranks != NULL) {
		kfree(mtkpasr_ranks);
		mtkpasr_ranks = NULL;
	}
	sysfs_remove_group(power_kobj, &mtkpasr_attr_group);

reset_devices:
	mtkpasr_reset_device(mtkpasr_device);
	if (extcomp != NULL) {
		free_pages((unsigned long)extcomp, get_order(MTKPASR_MAX_EXTCOMP * sizeof(unsigned long)));
		free_pages((unsigned long)sorted_for_extcomp, get_order(pasrbank_pfns * sizeof(unsigned long)));
	}

no_memory:
	free_pages((unsigned long)src_pgmap, get_order(pasrbank_pfns * sizeof(unsigned long)));

free_devices:
	kfree(mtkpasr_device);

out:
	zs_destroy_pool(mtkpasr_mem_pool);

no_mem_pool:
	kfree(mtkpasr_table);
	mtkpasr_table = NULL;
	mtkpasr_disksize = 0;

fail:
	/* Disable MTKPASR */
	mtkpasr_enable = 0;
	mtkpasr_enable_sr = 0;
	num_banks = 0;

	return ret;
}

static void __exit mtkpasr_exit(void)
{
	sysfs_remove_group(power_kobj, &mtkpasr_attr_group);
	if (mtkpasr_device->init_done)
		mtkpasr_reset_device(mtkpasr_device);

	mtkpasr_reset_global();

	kfree(mtkpasr_device);

	unregister_early_suspend(&mtkpasr_early_suspend_desc);

	pr_debug("Cleanup done!\n");
}
device_initcall_sync(mtkpasr_init);
module_exit(mtkpasr_exit);

MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION("MTK proprietary PASR driver");
