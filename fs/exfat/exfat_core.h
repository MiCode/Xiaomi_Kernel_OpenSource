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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/************************************************************************/
/*                                                                      */
/*  PROJECT : exFAT & FAT12/16/32 File System                           */
/*  FILE    : exfat_core.h                                              */
/*  PURPOSE : Header File for exFAT File Manager                        */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*  NOTES                                                               */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*  REVISION HISTORY (Ver 0.9)                                          */
/*                                                                      */
/*  - 2010.11.15 [Joosun Hahn] : first writing                          */
/*                                                                      */
/************************************************************************/

#ifndef _EXFAT_H
#define _EXFAT_H

#include "exfat_config.h"
#include "exfat_data.h"
#include "exfat_oal.h"

#include "exfat_blkdev.h"
#include "exfat_cache.h"
#include "exfat_nls.h"
#include "exfat_api.h"
#include "exfat_cache.h"

#ifdef CONFIG_EXFAT_KERNEL_DEBUG
  /* For Debugging Purpose */
	/* IOCTL code 'f' used by
	 *   - file systems typically #0~0x1F
	 *   - embedded terminal devices #128~
	 *   - exts for debugging purpose #99
	 * number 100 and 101 is availble now but has possible conflicts
	 */
#define EXFAT_IOC_GET_DEBUGFLAGS       _IOR('f', 100, long)
#define EXFAT_IOC_SET_DEBUGFLAGS       _IOW('f', 101, long)

#define EXFAT_DEBUGFLAGS_INVALID_UMOUNT        0x01
#define EXFAT_DEBUGFLAGS_ERROR_RW              0x02
#endif /* CONFIG_EXFAT_KERNEL_DEBUG */

	/*----------------------------------------------------------------------*/
	/*  Constant & Macro Definitions                                        */
	/*----------------------------------------------------------------------*/

#define DENTRY_SIZE             32          /* dir entry size */
#define DENTRY_SIZE_BITS        5

/* PBR entries */
#define PBR_SIGNATURE           0xAA55
#define EXT_SIGNATURE           0xAA550000
#define VOL_LABEL               "NO NAME    " /* size should be 11 */
#define OEM_NAME                "MSWIN4.1"  /* size should be 8 */
#define STR_FAT12               "FAT12   "  /* size should be 8 */
#define STR_FAT16               "FAT16   "  /* size should be 8 */
#define STR_FAT32               "FAT32   "  /* size should be 8 */
#define STR_EXFAT               "EXFAT   "  /* size should be 8 */
#define VOL_CLEAN               0x0000
#define VOL_DIRTY               0x0002

/* max number of clusters */
#define FAT12_THRESHOLD         4087        /* 2^12 - 1 + 2 (clu 0 & 1) */
#define FAT16_THRESHOLD         65527       /* 2^16 - 1 + 2 */
#define FAT32_THRESHOLD         268435457   /* 2^28 - 1 + 2 */
#define EXFAT_THRESHOLD         268435457   /* 2^28 - 1 + 2 */

/* file types */
#define TYPE_UNUSED             0x0000
#define TYPE_DELETED            0x0001
#define TYPE_INVALID            0x0002
#define TYPE_CRITICAL_PRI       0x0100
#define TYPE_BITMAP             0x0101
#define TYPE_UPCASE             0x0102
#define TYPE_VOLUME             0x0103
#define TYPE_DIR                0x0104
#define TYPE_FILE               0x011F
#define TYPE_SYMLINK            0x015F
#define TYPE_CRITICAL_SEC       0x0200
#define TYPE_STREAM             0x0201
#define TYPE_EXTEND             0x0202
#define TYPE_ACL                0x0203
#define TYPE_BENIGN_PRI         0x0400
#define TYPE_GUID               0x0401
#define TYPE_PADDING            0x0402
#define TYPE_ACLTAB             0x0403
#define TYPE_BENIGN_SEC         0x0800
#define TYPE_ALL                0x0FFF

/* time modes */
#define TM_CREATE               0
#define TM_MODIFY               1
#define TM_ACCESS               2

/* checksum types */
#define CS_DIR_ENTRY            0
#define CS_PBR_SECTOR           1
#define CS_DEFAULT              2

#define CLUSTER_16(x)           ((u16)(x))
#define CLUSTER_32(x)           ((u32)(x))

#define FALSE			0
#define TRUE			1

#define MIN(a, b)		(((a) < (b)) ? (a) : (b))
#define MAX(a, b)		(((a) > (b)) ? (a) : (b))

#define START_SECTOR(x) \
	((((sector_t)((x) - 2)) << p_fs->sectors_per_clu_bits) + p_fs->data_start_sector)

#define IS_LAST_SECTOR_IN_CLUSTER(sec) \
		((((sec) - p_fs->data_start_sector + 1) & ((1 <<  p_fs->sectors_per_clu_bits) - 1)) == 0)

#define GET_CLUSTER_FROM_SECTOR(sec)			\
		((u32)((((sec) - p_fs->data_start_sector) >> p_fs->sectors_per_clu_bits) + 2))

#define GET16(p_src) \
	(((u16)(p_src)[0]) | (((u16)(p_src)[1]) << 8))
#define GET32(p_src) \
	(((u32)(p_src)[0]) | (((u32)(p_src)[1]) << 8) | \
	(((u32)(p_src)[2]) << 16) | (((u32)(p_src)[3]) << 24))
#define GET64(p_src) \
	(((u64)(p_src)[0]) | (((u64)(p_src)[1]) << 8) | \
	(((u64)(p_src)[2]) << 16) | (((u64)(p_src)[3]) << 24) | \
	(((u64)(p_src)[4]) << 32) | (((u64)(p_src)[5]) << 40) | \
	(((u64)(p_src)[6]) << 48) | (((u64)(p_src)[7]) << 56))


#define SET16(p_dst, src)                                  \
	do {                                              \
		(p_dst)[0] = (u8)(src);                     \
		(p_dst)[1] = (u8)(((u16)(src)) >> 8);       \
	} while (0)
#define SET32(p_dst, src)                                  \
	do {                                              \
		(p_dst)[0] = (u8)(src);                     \
		(p_dst)[1] = (u8)(((u32)(src)) >> 8);       \
		(p_dst)[2] = (u8)(((u32)(src)) >> 16);      \
		(p_dst)[3] = (u8)(((u32)(src)) >> 24);      \
	} while (0)
#define SET64(p_dst, src)                                  \
	do {                                              \
		(p_dst)[0] = (u8)(src);                   \
		(p_dst)[1] = (u8)(((u64)(src)) >> 8);     \
		(p_dst)[2] = (u8)(((u64)(src)) >> 16);    \
		(p_dst)[3] = (u8)(((u64)(src)) >> 24);    \
		(p_dst)[4] = (u8)(((u64)(src)) >> 32);    \
		(p_dst)[5] = (u8)(((u64)(src)) >> 40);    \
		(p_dst)[6] = (u8)(((u64)(src)) >> 48);    \
		(p_dst)[7] = (u8)(((u64)(src)) >> 56);    \
	} while (0)

#ifdef __LITTLE_ENDIAN
#define GET16_A(p_src)		(*((u16 *)(p_src)))
#define GET32_A(p_src)		(*((u32 *)(p_src)))
#define GET64_A(p_src)		(*((u64 *)(p_src)))
#define SET16_A(p_dst, src)	(*((u16 *)(p_dst)) = (u16)(src))
#define SET32_A(p_dst, src)	(*((u32 *)(p_dst)) = (u32)(src))
#define SET64_A(p_dst, src)	(*((u64 *)(p_dst)) = (u64)(src))
#else /* BIG_ENDIAN */
#define GET16_A(p_src)		GET16(p_src)
#define GET32_A(p_src)		GET32(p_src)
#define GET64_A(p_src)		GET64(p_src)
#define SET16_A(p_dst, src)	SET16(p_dst, src)
#define SET32_A(p_dst, src)	SET32(p_dst, src)
#define SET64_A(p_dst, src)	SET64(p_dst, src)
#endif

/* Upcase tabel mecro */
#define HIGH_INDEX_BIT (8)
#define HIGH_INDEX_MASK (0xFF00)
#define LOW_INDEX_BIT (16-HIGH_INDEX_BIT)
#define UTBL_ROW_COUNT (1<<LOW_INDEX_BIT)
#define UTBL_COL_COUNT (1<<HIGH_INDEX_BIT)

#if CONFIG_EXFAT_DEBUG_MSG
#define DPRINTK(...)			\
	do {								\
		printk("[EXFAT] " __VA_ARGS__);	\
	} while (0)
#else
#define DPRINTK(...)
#endif

static inline u16 get_col_index(u16 i)
{
	return i >> LOW_INDEX_BIT;
}
static inline u16 get_row_index(u16 i)
{
	return i & ~HIGH_INDEX_MASK;
}
/*----------------------------------------------------------------------*/
/*  Type Definitions                                                    */
/*----------------------------------------------------------------------*/

/* MS_DOS FAT partition boot record (512 bytes) */
typedef struct {
	u8       jmp_boot[3];
	u8       oem_name[8];
	u8       bpb[109];
	u8       boot_code[390];
	u8       signature[2];
} PBR_SECTOR_T;

/* MS-DOS FAT12/16 BIOS parameter block (51 bytes) */
typedef struct {
	u8       sector_size[2];
	u8       sectors_per_clu;
	u8       num_reserved[2];
	u8       num_fats;
	u8       num_root_entries[2];
	u8       num_sectors[2];
	u8       media_type;
	u8       num_fat_sectors[2];
	u8       sectors_in_track[2];
	u8       num_heads[2];
	u8       num_hid_sectors[4];
	u8       num_huge_sectors[4];

	u8       phy_drv_no;
	u8       reserved;
	u8       ext_signature;
	u8       vol_serial[4];
	u8       vol_label[11];
	u8       vol_type[8];
} BPB16_T;

/* MS-DOS FAT32 BIOS parameter block (79 bytes) */
typedef struct {
	u8       sector_size[2];
	u8       sectors_per_clu;
	u8       num_reserved[2];
	u8       num_fats;
	u8       num_root_entries[2];
	u8       num_sectors[2];
	u8       media_type;
	u8       num_fat_sectors[2];
	u8       sectors_in_track[2];
	u8       num_heads[2];
	u8       num_hid_sectors[4];
	u8       num_huge_sectors[4];
	u8       num_fat32_sectors[4];
	u8       ext_flags[2];
	u8       fs_version[2];
	u8       root_cluster[4];
	u8       fsinfo_sector[2];
	u8       backup_sector[2];
	u8       reserved[12];

	u8       phy_drv_no;
	u8       ext_reserved;
	u8       ext_signature;
	u8       vol_serial[4];
	u8       vol_label[11];
	u8       vol_type[8];
} BPB32_T;

/* MS-DOS EXFAT BIOS parameter block (109 bytes) */
typedef struct {
	u8       reserved1[53];
	u8       vol_offset[8];
	u8       vol_length[8];
	u8       fat_offset[4];
	u8       fat_length[4];
	u8       clu_offset[4];
	u8       clu_count[4];
	u8       root_cluster[4];
	u8       vol_serial[4];
	u8       fs_version[2];
	u8       vol_flags[2];
	u8       sector_size_bits;
	u8       sectors_per_clu_bits;
	u8       num_fats;
	u8       phy_drv_no;
	u8       perc_in_use;
	u8       reserved2[7];
} BPBEX_T;

/* MS-DOS FAT file system information sector (512 bytes) */
typedef struct {
	u8       signature1[4];
	u8       reserved1[480];
	u8       signature2[4];
	u8       free_cluster[4];
	u8       next_cluster[4];
	u8       reserved2[14];
	u8       signature3[2];
} FSI_SECTOR_T;

/* MS-DOS FAT directory entry (32 bytes) */
typedef struct {
	u8       dummy[32];
} DENTRY_T;

typedef struct {
	u8       name[DOS_NAME_LENGTH];
	u8       attr;
	u8       lcase;
	u8       create_time_ms;
	u8       create_time[2];
	u8       create_date[2];
	u8       access_date[2];
	u8       start_clu_hi[2];
	u8       modify_time[2];
	u8       modify_date[2];
	u8       start_clu_lo[2];
	u8       size[4];
} DOS_DENTRY_T;

/* MS-DOS FAT extended directory entry (32 bytes) */
typedef struct {
	u8       order;
	u8       unicode_0_4[10];
	u8       attr;
	u8       sysid;
	u8       checksum;
	u8       unicode_5_10[12];
	u8       start_clu[2];
	u8       unicode_11_12[4];
} EXT_DENTRY_T;

/* MS-DOS EXFAT file directory entry (32 bytes) */
typedef struct {
	u8       type;
	u8       num_ext;
	u8       checksum[2];
	u8       attr[2];
	u8       reserved1[2];
	u8       create_time[2];
	u8       create_date[2];
	u8       modify_time[2];
	u8       modify_date[2];
	u8       access_time[2];
	u8       access_date[2];
	u8       create_time_ms;
	u8       modify_time_ms;
	u8       access_time_ms;
	u8       reserved2[9];
} FILE_DENTRY_T;

/* MS-DOS EXFAT stream extension directory entry (32 bytes) */
typedef struct {
	u8       type;
	u8       flags;
	u8       reserved1;
	u8       name_len;
	u8       name_hash[2];
	u8       reserved2[2];
	u8       valid_size[8];
	u8       reserved3[4];
	u8       start_clu[4];
	u8       size[8];
} STRM_DENTRY_T;

/* MS-DOS EXFAT file name directory entry (32 bytes) */
typedef struct {
	u8       type;
	u8       flags;
	u8       unicode_0_14[30];
} NAME_DENTRY_T;

/* MS-DOS EXFAT allocation bitmap directory entry (32 bytes) */
typedef struct {
	u8       type;
	u8       flags;
	u8       reserved[18];
	u8       start_clu[4];
	u8       size[8];
} BMAP_DENTRY_T;

/* MS-DOS EXFAT up-case table directory entry (32 bytes) */
typedef struct {
	u8       type;
	u8       reserved1[3];
	u8       checksum[4];
	u8       reserved2[12];
	u8       start_clu[4];
	u8       size[8];
} CASE_DENTRY_T;

/* MS-DOS EXFAT volume label directory entry (32 bytes) */
typedef struct {
	u8       type;
	u8       label_len;
	u8       unicode_0_10[22];
	u8       reserved[8];
} VOLM_DENTRY_T;

/* unused entry hint information */
typedef struct {
	u32      dir;
	s32       entry;
	CHAIN_T     clu;
} UENTRY_T;

typedef struct {
	s32       (*alloc_cluster)(struct super_block *sb, s32 num_alloc, CHAIN_T *p_chain);
	void        (*free_cluster)(struct super_block *sb, CHAIN_T *p_chain, s32 do_relse);
	s32       (*count_used_clusters)(struct super_block *sb);

	s32      (*init_dir_entry)(struct super_block *sb, CHAIN_T *p_dir, s32 entry, u32 type,
								 u32 start_clu, u64 size);
	s32      (*init_ext_entry)(struct super_block *sb, CHAIN_T *p_dir, s32 entry, s32 num_entries,
								 UNI_NAME_T *p_uniname, DOS_NAME_T *p_dosname);
	s32       (*find_dir_entry)(struct super_block *sb, CHAIN_T *p_dir, UNI_NAME_T *p_uniname, s32 num_entries, DOS_NAME_T *p_dosname, u32 type);
	void        (*delete_dir_entry)(struct super_block *sb, CHAIN_T *p_dir, s32 entry, s32 offset, s32 num_entries);
	void        (*get_uni_name_from_ext_entry)(struct super_block *sb, CHAIN_T *p_dir, s32 entry, u16 *uniname);
	s32       (*count_ext_entries)(struct super_block *sb, CHAIN_T *p_dir, s32 entry, DENTRY_T *p_entry);
	s32       (*calc_num_entries)(UNI_NAME_T *p_uniname);

	u32      (*get_entry_type)(DENTRY_T *p_entry);
	void        (*set_entry_type)(DENTRY_T *p_entry, u32 type);
	u32      (*get_entry_attr)(DENTRY_T *p_entry);
	void        (*set_entry_attr)(DENTRY_T *p_entry, u32 attr);
	u8       (*get_entry_flag)(DENTRY_T *p_entry);
	void        (*set_entry_flag)(DENTRY_T *p_entry, u8 flag);
	u32      (*get_entry_clu0)(DENTRY_T *p_entry);
	void        (*set_entry_clu0)(DENTRY_T *p_entry, u32 clu0);
	u64      (*get_entry_size)(DENTRY_T *p_entry);
	void        (*set_entry_size)(DENTRY_T *p_entry, u64 size);
	void        (*get_entry_time)(DENTRY_T *p_entry, TIMESTAMP_T *tp, u8 mode);
	void        (*set_entry_time)(DENTRY_T *p_entry, TIMESTAMP_T *tp, u8 mode);
} FS_FUNC_T;

typedef struct __FS_INFO_T {
	u32      drv;                    /* drive ID */
	u32      vol_type;               /* volume FAT type */
	u32      vol_id;                 /* volume serial number */

	u64      num_sectors;            /* num of sectors in volume */
	u32      num_clusters;           /* num of clusters in volume */
	u32      cluster_size;           /* cluster size in bytes */
	u32      cluster_size_bits;
	u32      sectors_per_clu;        /* cluster size in sectors */
	u32      sectors_per_clu_bits;

	u32      PBR_sector;             /* PBR sector */
	u32      FAT1_start_sector;      /* FAT1 start sector */
	u32      FAT2_start_sector;      /* FAT2 start sector */
	u32      root_start_sector;      /* root dir start sector */
	u32      data_start_sector;      /* data area start sector */
	u32      num_FAT_sectors;        /* num of FAT sectors */

	u32      root_dir;               /* root dir cluster */
	u32      dentries_in_root;       /* num of dentries in root dir */
	u32      dentries_per_clu;       /* num of dentries per cluster */

	u32      vol_flag;               /* volume dirty flag */
	struct buffer_head *pbr_bh;         /* PBR sector */

	u32      map_clu;                /* allocation bitmap start cluster */
	u32      map_sectors;            /* num of allocation bitmap sectors */
	struct buffer_head **vol_amap;      /* allocation bitmap */

	u16      **vol_utbl;               /* upcase table */

	u32      clu_srch_ptr;           /* cluster search pointer */
	u32      used_clusters;          /* number of used clusters */
	UENTRY_T    hint_uentry;         /* unused entry hint information */

	u32      dev_ejected;            /* block device operation error flag */

	FS_FUNC_T	*fs_func;
	struct semaphore v_sem;

	/* FAT cache */
	BUF_CACHE_T FAT_cache_array[FAT_CACHE_SIZE];
	BUF_CACHE_T FAT_cache_lru_list;
	BUF_CACHE_T FAT_cache_hash_list[FAT_CACHE_HASH_SIZE];

	/* buf cache */
	BUF_CACHE_T buf_cache_array[BUF_CACHE_SIZE];
	BUF_CACHE_T buf_cache_lru_list;
	BUF_CACHE_T buf_cache_hash_list[BUF_CACHE_HASH_SIZE];
} FS_INFO_T;

#define ES_2_ENTRIES		2
#define ES_3_ENTRIES		3
#define ES_ALL_ENTRIES	0

typedef struct {
	sector_t	sector;	/* sector number that contains file_entry */
	s32	offset;		/* byte offset in the sector */
	s32	alloc_flag;	/* flag in stream entry. 01 for cluster chain, 03 for contig. clusteres. */
	u32 num_entries;

	/* __buf should be the last member */
	void *__buf;
} ENTRY_SET_CACHE_T;

/*----------------------------------------------------------------------*/
/*  External Function Declarations                                      */
/*----------------------------------------------------------------------*/

/* file system initialization & shutdown functions */
s32 ffsInit(void);
s32 ffsShutdown(void);

/* volume management functions */
s32 ffsMountVol(struct super_block *sb);
s32 ffsUmountVol(struct super_block *sb);
s32 ffsCheckVol(struct super_block *sb);
s32 ffsGetVolInfo(struct super_block *sb, VOL_INFO_T *info);
s32 ffsSyncVol(struct super_block *sb, s32 do_sync);

/* file management functions */
s32 ffsLookupFile(struct inode *inode, char *path, FILE_ID_T *fid);
s32 ffsCreateFile(struct inode *inode, char *path, u8 mode, FILE_ID_T *fid);
s32 ffsReadFile(struct inode *inode, FILE_ID_T *fid, void *buffer, u64 count, u64 *rcount);
s32 ffsWriteFile(struct inode *inode, FILE_ID_T *fid, void *buffer, u64 count, u64 *wcount);
s32 ffsTruncateFile(struct inode *inode, u64 old_size, u64 new_size);
s32 ffsMoveFile(struct inode *old_parent_inode, FILE_ID_T *fid, struct inode *new_parent_inode, struct dentry *new_dentry);
s32 ffsRemoveFile(struct inode *inode, FILE_ID_T *fid);
s32 ffsSetAttr(struct inode *inode, u32 attr);
s32 ffsGetStat(struct inode *inode, DIR_ENTRY_T *info);
s32 ffsSetStat(struct inode *inode, DIR_ENTRY_T *info);
s32 ffsMapCluster(struct inode *inode, s32 clu_offset, u32 *clu);

/* directory management functions */
s32 ffsCreateDir(struct inode *inode, char *path, FILE_ID_T *fid);
s32 ffsReadDir(struct inode *inode, DIR_ENTRY_T *dir_ent);
s32 ffsRemoveDir(struct inode *inode, FILE_ID_T *fid);

/*----------------------------------------------------------------------*/
/*  External Function Declarations (NOT TO UPPER LAYER)                 */
/*----------------------------------------------------------------------*/

/* fs management functions */
s32  fs_init(void);
s32  fs_shutdown(void);
void   fs_set_vol_flags(struct super_block *sb, u32 new_flag);
void   fs_sync(struct super_block *sb, s32 do_sync);
void   fs_error(struct super_block *sb);

/* cluster management functions */
s32   clear_cluster(struct super_block *sb, u32 clu);
s32  fat_alloc_cluster(struct super_block *sb, s32 num_alloc, CHAIN_T *p_chain);
s32  exfat_alloc_cluster(struct super_block *sb, s32 num_alloc, CHAIN_T *p_chain);
void   fat_free_cluster(struct super_block *sb, CHAIN_T *p_chain, s32 do_relse);
void   exfat_free_cluster(struct super_block *sb, CHAIN_T *p_chain, s32 do_relse);
u32 find_last_cluster(struct super_block *sb, CHAIN_T *p_chain);
s32  count_num_clusters(struct super_block *sb, CHAIN_T *dir);
s32  fat_count_used_clusters(struct super_block *sb);
s32  exfat_count_used_clusters(struct super_block *sb);
void   exfat_chain_cont_cluster(struct super_block *sb, u32 chain, s32 len);

/* allocation bitmap management functions */
s32  load_alloc_bitmap(struct super_block *sb);
void   free_alloc_bitmap(struct super_block *sb);
s32   set_alloc_bitmap(struct super_block *sb, u32 clu);
s32   clr_alloc_bitmap(struct super_block *sb, u32 clu);
u32 test_alloc_bitmap(struct super_block *sb, u32 clu);
void   sync_alloc_bitmap(struct super_block *sb);

/* upcase table management functions */
s32  load_upcase_table(struct super_block *sb);
void   free_upcase_table(struct super_block *sb);

/* dir entry management functions */
u32 fat_get_entry_type(DENTRY_T *p_entry);
u32 exfat_get_entry_type(DENTRY_T *p_entry);
void   fat_set_entry_type(DENTRY_T *p_entry, u32 type);
void   exfat_set_entry_type(DENTRY_T *p_entry, u32 type);
u32 fat_get_entry_attr(DENTRY_T *p_entry);
u32 exfat_get_entry_attr(DENTRY_T *p_entry);
void   fat_set_entry_attr(DENTRY_T *p_entry, u32 attr);
void   exfat_set_entry_attr(DENTRY_T *p_entry, u32 attr);
u8  fat_get_entry_flag(DENTRY_T *p_entry);
u8  exfat_get_entry_flag(DENTRY_T *p_entry);
void   fat_set_entry_flag(DENTRY_T *p_entry, u8 flag);
void   exfat_set_entry_flag(DENTRY_T *p_entry, u8 flag);
u32 fat_get_entry_clu0(DENTRY_T *p_entry);
u32 exfat_get_entry_clu0(DENTRY_T *p_entry);
void   fat_set_entry_clu0(DENTRY_T *p_entry, u32 start_clu);
void   exfat_set_entry_clu0(DENTRY_T *p_entry, u32 start_clu);
u64 fat_get_entry_size(DENTRY_T *p_entry);
u64 exfat_get_entry_size(DENTRY_T *p_entry);
void   fat_set_entry_size(DENTRY_T *p_entry, u64 size);
void   exfat_set_entry_size(DENTRY_T *p_entry, u64 size);
void   fat_get_entry_time(DENTRY_T *p_entry, TIMESTAMP_T *tp, u8 mode);
void   exfat_get_entry_time(DENTRY_T *p_entry, TIMESTAMP_T *tp, u8 mode);
void   fat_set_entry_time(DENTRY_T *p_entry, TIMESTAMP_T *tp, u8 mode);
void   exfat_set_entry_time(DENTRY_T *p_entry, TIMESTAMP_T *tp, u8 mode);
s32   fat_init_dir_entry(struct super_block *sb, CHAIN_T *p_dir, s32 entry, u32 type, u32 start_clu, u64 size);
s32   exfat_init_dir_entry(struct super_block *sb, CHAIN_T *p_dir, s32 entry, u32 type, u32 start_clu, u64 size);
s32   fat_init_ext_dir_entry(struct super_block *sb, CHAIN_T *p_dir, s32 entry, s32 num_entries, UNI_NAME_T *p_uniname, DOS_NAME_T *p_dosname);
s32   exfat_init_ext_dir_entry(struct super_block *sb, CHAIN_T *p_dir, s32 entry, s32 num_entries, UNI_NAME_T *p_uniname, DOS_NAME_T *p_dosname);
void   init_dos_entry(DOS_DENTRY_T *ep, u32 type, u32 start_clu);
void   init_ext_entry(EXT_DENTRY_T *ep, s32 order, u8 chksum, u16 *uniname);
void   init_file_entry(FILE_DENTRY_T *ep, u32 type);
void   init_strm_entry(STRM_DENTRY_T *ep, u8 flags, u32 start_clu, u64 size);
void   init_name_entry(NAME_DENTRY_T *ep, u16 *uniname);
void   fat_delete_dir_entry(struct super_block *sb, CHAIN_T *p_dir, s32 entry, s32 order, s32 num_entries);
void   exfat_delete_dir_entry(struct super_block *sb, CHAIN_T *p_dir, s32 entry, s32 order, s32 num_entries);

s32   find_location(struct super_block *sb, CHAIN_T *p_dir, s32 entry, sector_t *sector, s32 *offset);
DENTRY_T *get_entry_with_sector(struct super_block *sb, sector_t sector, s32 offset);
DENTRY_T *get_entry_in_dir(struct super_block *sb, CHAIN_T *p_dir, s32 entry, sector_t *sector);
ENTRY_SET_CACHE_T *get_entry_set_in_dir(struct super_block *sb, CHAIN_T *p_dir, s32 entry, u32 type, DENTRY_T **file_ep);
void release_entry_set(ENTRY_SET_CACHE_T *es);
s32 write_whole_entry_set(struct super_block *sb, ENTRY_SET_CACHE_T *es);
s32 write_partial_entries_in_entry_set(struct super_block *sb, ENTRY_SET_CACHE_T *es, DENTRY_T *ep, u32 count);
s32  search_deleted_or_unused_entry(struct super_block *sb, CHAIN_T *p_dir, s32 num_entries);
s32  find_empty_entry(struct inode *inode, CHAIN_T *p_dir, s32 num_entries);
s32  fat_find_dir_entry(struct super_block *sb, CHAIN_T *p_dir, UNI_NAME_T *p_uniname, s32 num_entries, DOS_NAME_T *p_dosname, u32 type);
s32  exfat_find_dir_entry(struct super_block *sb, CHAIN_T *p_dir, UNI_NAME_T *p_uniname, s32 num_entries, DOS_NAME_T *p_dosname, u32 type);
s32  fat_count_ext_entries(struct super_block *sb, CHAIN_T *p_dir, s32 entry, DENTRY_T *p_entry);
s32  exfat_count_ext_entries(struct super_block *sb, CHAIN_T *p_dir, s32 entry, DENTRY_T *p_entry);
s32  count_dos_name_entries(struct super_block *sb, CHAIN_T *p_dir, u32 type);
void   update_dir_checksum(struct super_block *sb, CHAIN_T *p_dir, s32 entry);
void update_dir_checksum_with_entry_set(struct super_block *sb, ENTRY_SET_CACHE_T *es);
bool   is_dir_empty(struct super_block *sb, CHAIN_T *p_dir);

/* name conversion functions */
s32  get_num_entries_and_dos_name(struct super_block *sb, CHAIN_T *p_dir, UNI_NAME_T *p_uniname, s32 *entries, DOS_NAME_T *p_dosname);
void   get_uni_name_from_dos_entry(struct super_block *sb, DOS_DENTRY_T *ep, UNI_NAME_T *p_uniname, u8 mode);
void   fat_get_uni_name_from_ext_entry(struct super_block *sb, CHAIN_T *p_dir, s32 entry, u16 *uniname);
void   exfat_get_uni_name_from_ext_entry(struct super_block *sb, CHAIN_T *p_dir, s32 entry, u16 *uniname);
s32  extract_uni_name_from_ext_entry(EXT_DENTRY_T *ep, u16 *uniname, s32 order);
s32  extract_uni_name_from_name_entry(NAME_DENTRY_T *ep, u16 *uniname, s32 order);
s32  fat_generate_dos_name(struct super_block *sb, CHAIN_T *p_dir, DOS_NAME_T *p_dosname);
void   fat_attach_count_to_dos_name(u8 *dosname, s32 count);
s32  fat_calc_num_entries(UNI_NAME_T *p_uniname);
s32  exfat_calc_num_entries(UNI_NAME_T *p_uniname);
u8  calc_checksum_1byte(void *data, s32 len, u8 chksum);
u16 calc_checksum_2byte(void *data, s32 len, u16 chksum, s32 type);
u32 calc_checksum_4byte(void *data, s32 len, u32 chksum, s32 type);

/* name resolution functions */
s32  resolve_path(struct inode *inode, char *path, CHAIN_T *p_dir, UNI_NAME_T *p_uniname);
s32  resolve_name(u8 *name, u8 **arg);

/* file operation functions */
s32  fat16_mount(struct super_block *sb, PBR_SECTOR_T *p_pbr);
s32  fat32_mount(struct super_block *sb, PBR_SECTOR_T *p_pbr);
s32  exfat_mount(struct super_block *sb, PBR_SECTOR_T *p_pbr);
s32  create_dir(struct inode *inode, CHAIN_T *p_dir, UNI_NAME_T *p_uniname, FILE_ID_T *fid);
s32  create_file(struct inode *inode, CHAIN_T *p_dir, UNI_NAME_T *p_uniname, u8 mode, FILE_ID_T *fid);
void   remove_file(struct inode *inode, CHAIN_T *p_dir, s32 entry);
s32  rename_file(struct inode *inode, CHAIN_T *p_dir, s32 old_entry, UNI_NAME_T *p_uniname, FILE_ID_T *fid);
s32  move_file(struct inode *inode, CHAIN_T *p_olddir, s32 oldentry, CHAIN_T *p_newdir, UNI_NAME_T *p_uniname, FILE_ID_T *fid);

/* sector read/write functions */
s32   sector_read(struct super_block *sb, sector_t sec, struct buffer_head **bh, s32 read);
s32   sector_write(struct super_block *sb, sector_t sec, struct buffer_head *bh, s32 sync);
s32   multi_sector_read(struct super_block *sb, sector_t sec, struct buffer_head **bh, s32 num_secs, s32 read);
s32   multi_sector_write(struct super_block *sb, sector_t sec, struct buffer_head *bh, s32 num_secs, s32 sync);

#endif /* _EXFAT_H */
