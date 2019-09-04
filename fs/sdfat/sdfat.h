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

#ifndef _SDFAT_H
#define _SDFAT_H

#include <linux/buffer_head.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/nls.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/ratelimit.h>
#include <linux/version.h>
#include <linux/kobject.h>
#include "api.h"

#ifdef CONFIG_SDFAT_DFR
#include "dfr.h"
#endif

/*
 * sdfat error flags
 */
#define SDFAT_ERRORS_CONT	(1)    /* ignore error and continue */
#define SDFAT_ERRORS_PANIC	(2)    /* panic on error */
#define SDFAT_ERRORS_RO		(3)    /* remount r/o on error */

/*
 * sdfat allocator flags
 */
#define SDFAT_ALLOC_DELAY	(1)    /* Delayed allocation */
#define SDFAT_ALLOC_SMART	(2)    /* Smart allocation */

/*
 * sdfat allocator destination for smart allocation
 */
#define ALLOC_NOWHERE		(0)
#define ALLOC_COLD		(1)
#define ALLOC_HOT		(16)
#define ALLOC_COLD_ALIGNED	(1)
#define ALLOC_COLD_PACKING	(2)
#define ALLOC_COLD_SEQ		(4)

/*
 * sdfat nls lossy flag
 */
#define NLS_NAME_NO_LOSSY	(0x00) /* no lossy */
#define NLS_NAME_LOSSY		(0x01) /* just detected incorrect filename(s) */
#define NLS_NAME_OVERLEN	(0x02) /* the length is over than its limit */

/*
 * sdfat common MACRO
 */
#define CLUSTER_16(x)	((u16)((x) & 0xFFFFU))
#define CLUSTER_32(x)	((u32)((x) & 0xFFFFFFFFU))
#define CLUS_EOF	CLUSTER_32(~0)
#define CLUS_BAD	(0xFFFFFFF7U)
#define CLUS_FREE	(0)
#define CLUS_BASE	(2)
#define IS_CLUS_EOF(x)	((x) == CLUS_EOF)
#define IS_CLUS_BAD(x)	((x) == CLUS_BAD)
#define IS_CLUS_FREE(x)	((x) == CLUS_FREE)
#define IS_LAST_SECT_IN_CLUS(fsi, sec)				\
	((((sec) - (fsi)->data_start_sector + 1)		\
	& ((1 << (fsi)->sect_per_clus_bits) - 1)) == 0)

#define CLUS_TO_SECT(fsi, x)	\
	((((unsigned long long)(x) - CLUS_BASE) << (fsi)->sect_per_clus_bits) + (fsi)->data_start_sector)

#define SECT_TO_CLUS(fsi, sec)	\
	((u32)((((sec) - (fsi)->data_start_sector) >> (fsi)->sect_per_clus_bits) + CLUS_BASE))

/* variables defined at sdfat.c */
extern const char *FS_TYPE_STR[];

enum {
	FS_TYPE_AUTO,
	FS_TYPE_EXFAT,
	FS_TYPE_VFAT,
	FS_TYPE_MAX
};

/*
 * sdfat mount in-memory data
 */
struct sdfat_mount_options {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
	kuid_t fs_uid;
	kgid_t fs_gid;
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0) */
	uid_t fs_uid;
	gid_t fs_gid;
#endif
	unsigned short fs_fmask;
	unsigned short fs_dmask;
	unsigned short allow_utime; /* permission for setting the [am]time */
	unsigned short codepage;    /* codepage for shortname conversions */
	char *iocharset;            /* charset for filename input/display */
	struct {
		unsigned int pack_ratio;
		unsigned int sect_per_au;
		unsigned int misaligned_sect;
	} amap_opt;		    /* AMAP-related options (see amap.c) */

	unsigned char utf8;
	unsigned char casesensitive;
	unsigned char adj_hidsect;
	unsigned char tz_utc;
	unsigned char improved_allocation;
	unsigned char defrag;
	unsigned char symlink;      /* support symlink operation */
	unsigned char errors;       /* on error: continue, panic, remount-ro */
	unsigned char discard;      /* flag on if -o dicard specified and device support discard() */
	unsigned char fs_type;      /* fs_type that user specified */
	unsigned short adj_req;     /* support aligned mpage write */
};

#define SDFAT_HASH_BITS    8
#define SDFAT_HASH_SIZE    (1UL << SDFAT_HASH_BITS)

/*
 * SDFAT file system superblock in-memory data
 */
struct sdfat_sb_info {
	FS_INFO_T fsi;	/* private filesystem info */

	struct mutex s_vlock;   /* volume lock */
	int use_vmalloc;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
	int s_dirt;
	struct mutex s_lock;    /* superblock lock */
	int write_super_queued;			/* Write_super work is pending? */
	struct delayed_work write_super_work;   /* Work_queue data structrue for write_super() */
	spinlock_t work_lock;			/* Lock for WQ */
#endif
	struct super_block *host_sb;		/* sb pointer */
	struct sdfat_mount_options options;
	struct nls_table *nls_disk; /* Codepage used on disk */
	struct nls_table *nls_io;   /* Charset used for input and display */
	struct ratelimit_state ratelimit;

	spinlock_t inode_hash_lock;
	struct hlist_head inode_hashtable[SDFAT_HASH_SIZE];
	struct kobject sb_kobj;
#ifdef CONFIG_SDFAT_DBG_IOCTL
	long debug_flags;
#endif /* CONFIG_SDFAT_DBG_IOCTL */

#ifdef	CONFIG_SDFAT_DFR
	struct defrag_info dfr_info;
	struct completion dfr_complete;
	unsigned int *dfr_new_clus;
	int dfr_new_idx;
	unsigned int *dfr_page_wb;
	void **dfr_pagep;
	unsigned int dfr_hint_clus;
	unsigned int dfr_hint_idx;
	int dfr_reserved_clus;

#ifdef	CONFIG_SDFAT_DFR_DEBUG
	int dfr_spo_flag;
#endif  /* CONFIG_SDFAT_DFR_DEBUG */

#endif  /* CONFIG_SDFAT_DFR */

#ifdef CONFIG_SDFAT_TRACE_IO
	/* Statistics for allocator */
	unsigned int stat_n_pages_written;	/* # of written pages in total */
	unsigned int stat_n_pages_added;	/* # of added blocks in total */
	unsigned int stat_n_bdev_pages_written;	/* # of written pages owned by bdev inode */
	unsigned int stat_n_pages_confused;
#endif
	atomic_t stat_n_pages_queued;	/* # of pages in the request queue (approx.) */
};

/*
 * SDFAT file system inode in-memory data
 */
struct sdfat_inode_info {
	FILE_ID_T fid;
	char  *target;
	/* NOTE: i_size_ondisk is 64bits, so must hold ->inode_lock to access */
	loff_t i_size_ondisk;         /* physically allocated size */
	loff_t i_size_aligned;          /* block-aligned i_size (used in cont_write_begin) */
	loff_t i_pos;               /* on-disk position of directory entry or 0 */
	struct hlist_node i_hash_fat;    /* hash by i_location */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	struct rw_semaphore truncate_lock; /* protect bmap against truncate */
#endif
#ifdef	CONFIG_SDFAT_DFR
	struct defrag_info dfr_info;
#endif
	struct inode vfs_inode;
};

/*
 * FIXME : needs on-disk-slot in-memory data
 */

/* static inline functons */
static inline const char *sdfat_get_vol_type_str(unsigned int type)
{
	if (type == EXFAT)
		return "exfat";
	else if (type == FAT32)
		return "vfat:32";
	else if (type == FAT16)
		return "vfat:16";
	else if (type == FAT12)
		return "vfat:12";

	return "unknown";
}

static inline struct sdfat_sb_info *SDFAT_SB(struct super_block *sb)
{
	return (struct sdfat_sb_info *)sb->s_fs_info;
}

static inline struct sdfat_inode_info *SDFAT_I(struct inode *inode)
{
	return container_of(inode, struct sdfat_inode_info, vfs_inode);
}

/*
 * If ->i_mode can't hold S_IWUGO (i.e. ATTR_RO), we use ->i_attrs to
 * save ATTR_RO instead of ->i_mode.
 *
 * If it's directory and !sbi->options.rodir, ATTR_RO isn't read-only
 * bit, it's just used as flag for app.
 */
static inline int sdfat_mode_can_hold_ro(struct inode *inode)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(inode->i_sb);

	if (S_ISDIR(inode->i_mode))
		return 0;

	if ((~sbi->options.fs_fmask) & S_IWUGO)
		return 1;
	return 0;
}

/*
 * FIXME : needs to check symlink option.
 */
/* Convert attribute bits and a mask to the UNIX mode. */
static inline mode_t sdfat_make_mode(struct sdfat_sb_info *sbi,
					u32 attr, mode_t mode)
{
	if ((attr & ATTR_READONLY) && !(attr & ATTR_SUBDIR))
		mode &= ~S_IWUGO;

	if (attr & ATTR_SUBDIR)
		return (mode & ~sbi->options.fs_dmask) | S_IFDIR;
	else if (attr & ATTR_SYMLINK)
		return (mode & ~sbi->options.fs_dmask) | S_IFLNK;
	else
		return (mode & ~sbi->options.fs_fmask) | S_IFREG;
}

/* Return the FAT attribute byte for this inode */
static inline u32 sdfat_make_attr(struct inode *inode)
{
	u32 attrs = SDFAT_I(inode)->fid.attr;

	if (S_ISDIR(inode->i_mode))
		attrs |= ATTR_SUBDIR;
	if (sdfat_mode_can_hold_ro(inode) && !(inode->i_mode & S_IWUGO))
		attrs |= ATTR_READONLY;
	return attrs;
}

static inline void sdfat_save_attr(struct inode *inode, u32 attr)
{
	if (sdfat_mode_can_hold_ro(inode))
		SDFAT_I(inode)->fid.attr = attr & ATTR_RWMASK;
	else
		SDFAT_I(inode)->fid.attr = attr & (ATTR_RWMASK | ATTR_READONLY);
}

/* sdfat/statistics.c */
/* bigdata function */
#ifdef CONFIG_SDFAT_STATISTICS
extern int sdfat_statistics_init(struct kset *sdfat_kset);
extern void sdfat_statistics_uninit(void);
extern void sdfat_statistics_set_mnt(FS_INFO_T *fsi);
extern void sdfat_statistics_set_mnt_ro(void);
extern void sdfat_statistics_set_mkdir(u8 flags);
extern void sdfat_statistics_set_create(u8 flags);
extern void sdfat_statistics_set_rw(u8 flags, u32 clu_offset, s32 create);
extern void sdfat_statistics_set_trunc(u8 flags, CHAIN_T *clu);
extern void sdfat_statistics_set_vol_size(struct super_block *sb);
#else
static inline int sdfat_statistics_init(struct kset *sdfat_kset)
{
	return 0;
}
static inline void sdfat_statistics_uninit(void) {};
static inline void sdfat_statistics_set_mnt(FS_INFO_T *fsi) {};
static inline void sdfat_statistics_set_mnt_ro(void) {};
static inline void sdfat_statistics_set_mkdir(u8 flags) {};
static inline void sdfat_statistics_set_create(u8 flags) {};
static inline void sdfat_statistics_set_rw(u8 flags, u32 clu_offset, s32 create) {};
static inline void sdfat_statistics_set_trunc(u8 flags, CHAIN_T *clu) {};
static inline void sdfat_statistics_set_vol_size(struct super_block *sb) {};
#endif

/* sdfat/nls.c */
/* NLS management function */
s32  nls_cmp_sfn(struct super_block *sb, u8 *a, u8 *b);
s32  nls_cmp_uniname(struct super_block *sb, u16 *a, u16 *b);
s32  nls_uni16s_to_sfn(struct super_block *sb, UNI_NAME_T *p_uniname, DOS_NAME_T *p_dosname, s32 *p_lossy);
s32  nls_sfn_to_uni16s(struct super_block *sb, DOS_NAME_T *p_dosname, UNI_NAME_T *p_uniname);
s32  nls_uni16s_to_vfsname(struct super_block *sb, UNI_NAME_T *uniname, u8 *p_cstring, s32 len);
s32  nls_vfsname_to_uni16s(struct super_block *sb, const u8 *p_cstring,
			const s32 len, UNI_NAME_T *uniname, s32 *p_lossy);

/* sdfat/mpage.c */
#ifdef CONFIG_SDFAT_ALIGNED_MPAGE_WRITE
int sdfat_mpage_writepages(struct address_space *mapping,
			struct writeback_control *wbc, get_block_t *get_block);
#endif

/* sdfat/xattr.c */
#ifdef CONFIG_SDFAT_VIRTUAL_XATTR
void setup_sdfat_xattr_handler(struct super_block *sb);
extern int sdfat_setxattr(struct dentry *dentry, const char *name, const void *value, size_t size, int flags);
extern ssize_t sdfat_getxattr(struct dentry *dentry, const char *name, void *value, size_t size);
extern ssize_t sdfat_listxattr(struct dentry *dentry, char *list, size_t size);
extern int sdfat_removexattr(struct dentry *dentry, const char *name);
#else
static inline void setup_sdfat_xattr_handler(struct super_block *sb) {};
#endif

/* sdfat/misc.c */
extern void
__sdfat_fs_error(struct super_block *sb, int report, const char *fmt, ...)
	__printf(3, 4) __cold;
#define sdfat_fs_error(sb, fmt, args...)          \
	__sdfat_fs_error(sb, 1, fmt, ## args)
#define sdfat_fs_error_ratelimit(sb, fmt, args...) \
	__sdfat_fs_error(sb, __ratelimit(&SDFAT_SB(sb)->ratelimit), fmt, ## args)
extern void
__sdfat_msg(struct super_block *sb, const char *lv, int st, const char *fmt, ...)
	__printf(4, 5) __cold;
#define sdfat_msg(sb, lv, fmt, args...)          \
	__sdfat_msg(sb, lv, 0, fmt, ## args)
#define sdfat_log_msg(sb, lv, fmt, args...)          \
	__sdfat_msg(sb, lv, 1, fmt, ## args)
extern void sdfat_log_version(void);
extern void sdfat_time_fat2unix(struct sdfat_sb_info *sbi, struct timespec *ts,
				DATE_TIME_T *tp);
extern void sdfat_time_unix2fat(struct sdfat_sb_info *sbi, struct timespec *ts,
				DATE_TIME_T *tp);
extern TIMESTAMP_T *tm_now(struct sdfat_sb_info *sbi, TIMESTAMP_T *tm);

#ifdef CONFIG_SDFAT_DEBUG

#ifdef CONFIG_SDFAT_DBG_CAREFUL
void sdfat_debug_check_clusters(struct inode *inode);
#else
#define sdfat_debug_check_clusters(inode)
#endif /* CONFIG_SDFAT_DBG_CAREFUL */

#ifdef CONFIG_SDFAT_DBG_BUGON
#define sdfat_debug_bug_on(expr)        BUG_ON(expr)
#else
#define sdfat_debug_bug_on(expr)
#endif

#ifdef CONFIG_SDFAT_DBG_WARNON
#define sdfat_debug_warn_on(expr)        WARN_ON(expr)
#else
#define sdfat_debug_warn_on(expr)
#endif

#else /* CONFIG_SDFAT_DEBUG */

#define sdfat_debug_check_clusters(inode)
#define sdfat_debug_bug_on(expr)
#define sdfat_debug_warn_on(expr)

#endif /* CONFIG_SDFAT_DEBUG */

#ifdef CONFIG_SDFAT_TRACE_ELAPSED_TIME
u32 sdfat_time_current_usec(struct timeval *tv);
extern struct timeval __t1;
extern struct timeval __t2;

#define TIME_GET(tv)	sdfat_time_current_usec(tv)
#define TIME_START(s)	sdfat_time_current_usec(s)
#define TIME_END(e)	sdfat_time_current_usec(e)
#define TIME_ELAPSED(s, e) ((u32)(((e)->tv_sec - (s)->tv_sec) * 1000000 + \
			((e)->tv_usec - (s)->tv_usec)))
#define PRINT_TIME(n)	pr_info("[SDFAT] Elapsed time %d = %d (usec)\n", n, (__t2 - __t1))
#else /* CONFIG_SDFAT_TRACE_ELAPSED_TIME */
#define TIME_GET(tv)    (0)
#define TIME_START(s)
#define TIME_END(e)
#define TIME_ELAPSED(s, e)      (0)
#define PRINT_TIME(n)
#endif /* CONFIG_SDFAT_TRACE_ELAPSED_TIME */

#define	SDFAT_MSG_LV_NONE	(0x00000000)
#define SDFAT_MSG_LV_ERR	(0x00000001)
#define SDFAT_MSG_LV_INFO	(0x00000002)
#define SDFAT_MSG_LV_DBG	(0x00000003)
#define SDFAT_MSG_LV_MORE	(0x00000004)
#define SDFAT_MSG_LV_TRACE	(0x00000005)
#define SDFAT_MSG_LV_ALL	(0x00000006)

#define SDFAT_MSG_LEVEL		SDFAT_MSG_LV_INFO

#define SDFAT_TAG_NAME	"SDFAT"
#define __S(x) #x
#define _S(x) __S(x)

extern void __sdfat_dmsg(int level, const char *fmt, ...) __printf(2, 3) __cold;

#define SDFAT_EMSG_T(level, ...)	\
	__sdfat_dmsg(level, KERN_ERR "[" SDFAT_TAG_NAME "] [" _S(__FILE__) "(" _S(__LINE__) ")] " __VA_ARGS__)
#define SDFAT_DMSG_T(level, ...)	\
	__sdfat_dmsg(level, KERN_INFO "[" SDFAT_TAG_NAME "] " __VA_ARGS__)

#define SDFAT_EMSG(...) SDFAT_EMSG_T(SDFAT_MSG_LV_ERR, __VA_ARGS__)
#define SDFAT_IMSG(...) SDFAT_DMSG_T(SDFAT_MSG_LV_INFO, __VA_ARGS__)
#define SDFAT_DMSG(...) SDFAT_DMSG_T(SDFAT_MSG_LV_DBG, __VA_ARGS__)
#define SDFAT_MMSG(...) SDFAT_DMSG_T(SDFAT_MSG_LV_MORE, __VA_ARGS__)
#define SDFAT_TMSG(...) SDFAT_DMSG_T(SDFAT_MSG_LV_TRACE, __VA_ARGS__)

#define EMSG(...)
#define IMSG(...)
#define DMSG(...)
#define MMSG(...)
#define TMSG(...)

#define EMSG_VAR(exp)
#define IMSG_VAR(exp)
#define DMSG_VAR(exp)
#define MMSG_VAR(exp)
#define TMSG_VAR(exp)

#ifdef CONFIG_SDFAT_DBG_MSG


#if (SDFAT_MSG_LEVEL >= SDFAT_MSG_LV_ERR)
#undef EMSG
#undef EMSG_VAR
#define EMSG(...)	SDFAT_EMSG(__VA_ARGS__)
#define EMSG_VAR(exp)	exp
#endif

#if (SDFAT_MSG_LEVEL >= SDFAT_MSG_LV_INFO)
#undef IMSG
#undef IMSG_VAR
#define IMSG(...)	SDFAT_IMSG(__VA_ARGS__)
#define IMSG_VAR(exp)	exp
#endif

#if (SDFAT_MSG_LEVEL >= SDFAT_MSG_LV_DBG)
#undef DMSG
#undef DMSG_VAR
#define DMSG(...)	SDFAT_DMSG(__VA_ARGS__)
#define DMSG_VAR(exp)	exp
#endif

#if (SDFAT_MSG_LEVEL >= SDFAT_MSG_LV_MORE)
#undef MMSG
#undef MMSG_VAR
#define MMSG(...)	SDFAT_MMSG(__VA_ARGS__)
#define MMSG_VAR(exp)	exp
#endif

/* should replace with trace function */
#if (SDFAT_MSG_LEVEL >= SDFAT_MSG_LV_TRACE)
#undef TMSG
#undef TMSG_VAR
#define TMSG(...)	SDFAT_TMSG(__VA_ARGS__)
#define TMSG_VAR(exp)	exp
#endif

#endif /* CONFIG_SDFAT_DBG_MSG */


#define ASSERT(expr)	{					\
	if (!(expr)) {						\
		pr_err("Assertion failed! %s\n", #expr);	\
		BUG_ON(1);					\
	}							\
}

#endif /* !_SDFAT_H */

