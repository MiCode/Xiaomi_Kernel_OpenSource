/*
 *  Copyright (C) 2012-2013 Samsung Electronics Co., Ltd.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SDFAT_DEFRAG_H
#define _SDFAT_DEFRAG_H

#ifdef	CONFIG_SDFAT_DFR

/* Tuning parameters */
#define	DFR_MIN_TIMEOUT		 (1 * HZ)	// Minimum timeout for forced-sync
#define	DFR_DEFAULT_TIMEOUT	 (10 * HZ)	// Default timeout for forced-sync

#define	DFR_DEFAULT_CLEAN_RATIO	 (50)	// Wake-up daemon when clean AU ratio under 50%
#define	DFR_DEFAULT_WAKEUP_RATIO (10)	// Wake-up daemon when clean AU ratio under 10%, regardless of frag_ratio

#define	DFR_DEFAULT_FRAG_RATIO	 (130)	// Wake-up daemon when frag_ratio over 130%

#define	DFR_DEFAULT_PACKING_RATIO	(10)	// Call allocator with PACKING flag, when clean AU ratio under 10%

#define	DFR_DEFAULT_STOP_RATIO		(98)	// Stop defrag_daemon when disk used ratio over 98%
#define	DFR_FULL_RATIO			(100)

#define	DFR_MAX_AU_MOVED		(16)	// Maximum # of AUs for a request


/* Debugging support*/
#define dfr_err(fmt, args...) pr_err("DFR: " fmt "\n", args)

#ifdef	CONFIG_SDFAT_DFR_DEBUG
#define dfr_debug(fmt, args...) pr_debug("DFR: " fmt "\n", args)
#else
#define dfr_debug(fmt, args...)
#endif


/* Error handling */
#define	ERR_HANDLE(err) {			\
	if (err) {				\
		dfr_debug("err %d", err);	\
		goto error;			\
	}					\
}

#define	ERR_HANDLE2(cond, err, val) {		\
	if (cond) {				\
		err = val;			\
		dfr_debug("err %d", err);	\
		goto error;			\
	}					\
}


/* Arguments IN-OUT */
#define IN
#define OUT
#define INOUT


/* Macros */
#define	GET64_HI(var64)			((unsigned int)((var64) >> 32))
#define	GET64_LO(var64)			((unsigned int)(((var64) << 32) >> 32))
#define	SET64_HI(dst64, var32)	{ (dst64) = ((loff_t)(var32) << 32) | ((dst64) & 0x00000000ffffffffLL); }
#define	SET64_LO(dst64, var32)	{ (dst64) = ((dst64) & 0xffffffff00000000LL) | ((var32) & 0x00000000ffffffffLL); }

#define	GET32_HI(var32)			((unsigned short)((var32) >> 16))
#define	GET32_LO(var32)			((unsigned short)(((var32) << 16) >> 16))
#define	SET32_HI(dst32, var16)	{ (dst32) = ((unsigned int)(var16) << 16) | ((dst32) & 0x0000ffff); }
#define	SET32_LO(dst32, var16)	{ (dst32) = ((dst32) & 0xffff0000) | ((unsigned int)(var16) & 0x0000ffff); }


/* FAT32 related */
#define	FAT32_EOF					(0x0fffffff)
#define	FAT32_RESERVED				(0x0ffffff7)
#define	FAT32_UNUSED_CLUS			(2)

#define	CLUS_PER_AU(sb)				( \
	(SDFAT_SB(sb)->options.amap_opt.sect_per_au) >> (SDFAT_SB(sb)->fsi.sect_per_clus_bits) \
)
#define	PAGES_PER_AU(sb)			( \
	((SDFAT_SB(sb)->options.amap_opt.sect_per_au) << ((sb)->s_blocksize_bits)) \
	>> PAGE_SHIFT \
)
#define	PAGES_PER_CLUS(sb)			((SDFAT_SB(sb)->fsi.cluster_size) >> PAGE_SHIFT)

#define	FAT32_CHECK_CLUSTER(fsi, clus, err) \
		{ \
			if (((clus) < FAT32_UNUSED_CLUS) || \
					((clus) > (fsi)->num_clusters) || \
					((clus) >= FAT32_RESERVED)) { \
				dfr_err("clus %08x, fsi->num_clusters %08x", (clus), (fsi)->num_clusters); \
				err = -EINVAL; \
			} else { \
				err = 0; \
			} \
		}


/* IOCTL_DFR_INFO */
struct defrag_info_arg {
	/* PBS info */
	unsigned int sec_sz;
	unsigned int clus_sz;
	unsigned long long total_sec;
	unsigned long long fat_offset_sec;
	unsigned int fat_sz_sec;
	unsigned int n_fat;
	unsigned int hidden_sectors;

	/* AU info */
	unsigned int sec_per_au;
};


/* IOC_DFR_TRAV */
#define	DFR_TRAV_HEADER_IDX			(0)

#define	DFR_TRAV_TYPE_HEADER		(0x0000000F)
#define	DFR_TRAV_TYPE_DIR			(1)
#define	DFR_TRAV_TYPE_FILE			(2)
#define	DFR_TRAV_TYPE_TEST			(DFR_TRAV_TYPE_HEADER | 0x10000000)

#define	DFR_TRAV_ROOT_IPOS			(0xFFFFFFFFFFFFFFFFLL)

struct defrag_trav_arg {
	int type;
	unsigned int start_clus;
	loff_t i_pos;
	char name[MAX_DOSNAME_BUF_SIZE];
	char dummy1;
	int dummy2;
};

#define	DFR_TRAV_STAT_DONE			(0x1)
#define	DFR_TRAV_STAT_MORE			(0x2)
#define	DFR_TRAV_STAT_ERR			(0xFF)

struct defrag_trav_header {
	int type;
	unsigned int start_clus;
	loff_t i_pos;
	char name[MAX_DOSNAME_BUF_SIZE];
	char stat;
	unsigned int nr_entries;
};


/* IOC_DFR_REQ */
#define	REQ_HEADER_IDX			(0)

#define	DFR_CHUNK_STAT_ERR		(0xFFFFFFFF)
#define	DFR_CHUNK_STAT_REQ		(0x1)
#define	DFR_CHUNK_STAT_WB		(0x2)
#define	DFR_CHUNK_STAT_FAT		(0x4)
#define	DFR_CHUNK_STAT_PREP		(DFR_CHUNK_STAT_REQ | DFR_CHUNK_STAT_WB | DFR_CHUNK_STAT_FAT)
#define	DFR_CHUNK_STAT_PASS		(0x0000000F)

struct defrag_chunk_header {
	int mode;
	unsigned int nr_chunks;
	loff_t dummy1;
	int dummy2[4];
	union {
		int *dummy3;
		int dummy4;
	};
	int dummy5;
};

struct defrag_chunk_info {
	int stat;
	/* File related */
	unsigned int f_clus;
	loff_t i_pos;
	/* Cluster related */
	unsigned int d_clus;
	unsigned int nr_clus;
	unsigned int prev_clus;
	unsigned int next_clus;
	union {
		void *dummy;
		/* req status */
		unsigned int new_idx;
	};
	/* AU related */
	unsigned int au_clus;
};


/* Global info */
#define	DFR_MODE_BACKGROUND		(0x1)
#define	DFR_MODE_FOREGROUND		(0x2)
#define DFR_MODE_ONESHOT		(0x4)
#define	DFR_MODE_BATCHED		(0x8)
#define	DFR_MODE_TEST			(DFR_MODE_BACKGROUND | 0x10000000)

#define	DFR_SB_STAT_IDLE		(0)
#define	DFR_SB_STAT_REQ			(1)
#define	DFR_SB_STAT_VALID		(2)

#define	DFR_INO_STAT_IDLE		(0)
#define	DFR_INO_STAT_REQ		(1)
struct defrag_info {
	struct mutex lock;
	atomic_t stat;
	struct defrag_chunk_info *chunks;
	unsigned int nr_chunks;
	struct list_head entry;
};


/* SPO test flags */
#define	DFR_SPO_NONE			(0)
#define	DFR_SPO_NORMAL			(1)
#define	DFR_SPO_DISCARD			(2)
#define	DFR_SPO_FAT_NEXT		(3)
#define	DFR_SPO_RANDOM			(4)


/* Extern functions */
int defrag_get_info(struct super_block *sb, struct defrag_info_arg *arg);

int defrag_scan_dir(struct super_block *sb, struct defrag_trav_arg *arg);

int defrag_validate_cluster(struct inode *inode, struct defrag_chunk_info *chunk, int skip_prev);
int defrag_reserve_clusters(struct super_block *sb, int nr_clus);
int defrag_mark_ignore(struct super_block *sb, unsigned int clus);
void defrag_unmark_ignore_all(struct super_block *sb);

int defrag_map_cluster(struct inode *inode, unsigned int clu_offset, unsigned int *clu);
void defrag_writepage_end_io(struct page *page);

void defrag_update_fat_prev(struct super_block *sb, int force);
void defrag_update_fat_next(struct super_block *sb);
void defrag_check_discard(struct super_block *sb);
int defrag_free_cluster(struct super_block *sb, unsigned int clus);

int defrag_check_defrag_required(struct super_block *sb, int *totalau, int *cleanau, int *fullau);
int defrag_check_defrag_on(struct inode *inode, loff_t start, loff_t end, int cancel, const char *caller);

#ifdef CONFIG_SDFAT_DFR_DEBUG
void defrag_spo_test(struct super_block *sb, int flag, const char *caller);
#endif

#endif	/* CONFIG_SDFAT_DFR */

#endif	/* _SDFAT_DEFRAG_H */

