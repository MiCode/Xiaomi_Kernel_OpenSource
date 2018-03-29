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

#ifndef __RAWFS_H__
#define __RAWFS_H__

#include <asm/byteorder.h>
#include <linux/list.h>

#if defined(CONFIG_MT_ENG_BUILD)  /* log is only enabled in eng load */
#define RAWFS_DBG		 pr_debug
#endif

#define RAWFS_BLOCK_LIMIT 2  /* do not modify, design limit */
#define RAWFS_BLOCK_FILE
/* #define RAWFS_RAM_DISK */

#define RAWFS_VERSION  0x01

/* Debug Message Mask */
enum rawfs_debug_level_enum {
	RAWFS_DBG_SUPER  = 0x0001,
	RAWFS_DBG_DEVICE = 0x0002,
	RAWFS_DBG_INODE  = 0x0004,
	RAWFS_DBG_FILE   = 0x0008,
	RAWFS_DBG_DIR	= 0x0010,
	RAWFS_DBG_DENTRY = 0x0020,
	RAWFS_DBG_INIT   = 0x0040,
	RAWFS_DBG_GC	 = 0x0080,
	RAWFS_DBG_MOUNT  = 0x0100
};

extern int rawfs_debug_msg_mask;

#define RAWFS_DEBUG_MSG_DEFAULT  (RAWFS_DBG_SUPER | RAWFS_DBG_DEVICE | \
		RAWFS_DBG_INODE | RAWFS_DBG_FILE | \
		RAWFS_DBG_DIR | RAWFS_DBG_DENTRY | \
		RAWFS_DBG_INIT | RAWFS_DBG_GC | RAWFS_DBG_MOUNT)

#ifdef RAWFS_DBG
#define RAWFS_PRINT(category, str, ...) do { \
	if (category & rawfs_debug_msg_mask)	{ \
		RAWFS_DBG("rawfs: " str, ##__VA_ARGS__); \
	} } while (0)
#else
#define RAWFS_PRINT(...)
#endif

#define RAWFS_MAX_FILENAME_LEN 60

#define RAWFS_MNT_RAM		0x01
#define RAWFS_MNT_MTD		0x02
#define RAWFS_MNT_CASE		0x10

#define RAWFS_MNT_BLOCKFILE  0x40
#define RAWFS_MNT_FIRSTBOOT  0x80

/* RAW FS super block info */
#define RAWFS_HASH_BITS 2
#define RAWFS_HASH_SIZE (1UL << RAWFS_HASH_BITS)

/* Interface to MTD block device */
struct rawfs_dev {
	int (*erase_block)(struct super_block *sb, int block_no);
	int (*read_page_user)(struct super_block *sb, int block_no, int addr,
		const struct iovec *iov, unsigned long nr_segs, int size);
	int (*write_page)(struct super_block *sb, int block_no,
		int page_no, void *buffer);
	int (*read_page)(struct super_block *sb, int block_no,
		int page_no, void *buffer);
};

/* RAW FS super block info */
struct rawfs_sb_info {
	/* File System Context */
	struct list_head fs_context;
	struct super_block *super;
	struct proc_dir_entry *s_proc;
	/* Driver context */
	void *driver_context;
	struct rawfs_dev dev;
	/* Device Info */
	int total_blocks;
	int pages_per_block;
	int sectors_per_page;
	int block_size;
	int page_size;
	int page_data_size;
	/* Management */
	int data_block;
	int data_block_free_page_index;
	int data_block_gcmarker_page_index;
	int empty_block;
	__u32 sequence_number;
	__u32 erase_count_max;
	int flags;
	char *fake_block;
	struct mutex rawfs_lock;
	struct nls_table *local_nls;	/* Codepage used on disk */
	/* File List */
	struct list_head folder_list;
	struct list_head file_list;
	struct mutex file_list_lock;
	/* Inode Hash Table */
	spinlock_t inode_hash_lock;
	struct hlist_head inode_hashtable[RAWFS_HASH_SIZE];
};

#define RAWFS_NAND_BLOCKS(sb)		 ((sb)?RAWFS_BLOCK_LIMIT:0) /* We use only two block */
#define RAWFS_NAND_PAGES(sb)		  (sb->pages_per_block)
#define RAWFS_NAND_PAGE_SIZE(sb)	  (sb->page_size)
#define RAWFS_NAND_BLOCK_SIZE(sb)	 (sb->block_size)
#define RAWFS_NAND_PAGE_SECTORS(sb)	(sb->sectors_per_page)
#define RAWFS_NAND_PAGE_DATA_SIZE(sb) (sb->page_data_size)

/* RAW FS inode info */
struct rawfs_inode_info {
	spinlock_t cache_lru_lock;
	struct list_head cache_lru;
	int nr_caches;
	unsigned int cache_valid_id;

	/* NOTE: mmu_private is 64bits, so must hold ->i_mutex to access */
	loff_t mmu_private;	/* physically allocated size */

	int i_location_block;		/* File Location: block */
	int i_location_page;		 /* File Location: starting page */
	int i_location_page_count;
	int i_id;
	int i_parent_folder_id;	  /* Parent folder ID */

	char i_name[RAWFS_MAX_FILENAME_LEN+4];
	struct hlist_node i_rawfs_hash;	/* hash by i_name */
	struct rw_semaphore truncate_lock; /* protect bmap against truncate */
	struct inode vfs_inode;
};

static inline struct rawfs_sb_info *
RAWFS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct rawfs_inode_info *
RAWFS_I(struct inode *inode)
{
	return container_of(inode, struct rawfs_inode_info, vfs_inode);
}

static inline struct mtd_info *
RAWFS_MTD(struct super_block *sb)
{
	return RAWFS_SB(sb)->driver_context;
}

#define RAWFS_CACHE_VALID 0

#define RAWFS_ROOT_DIR_ID 1

/* RAWFS inode number */
#define RAWFS_ROOT_INO	1
#define RAWFS_BLOCK0_INO 2
#define RAWFS_BLOCK1_INO 3
#define RAWFS_MAX_RESERVED_INO 64

/* Page Signatures */
#define RAWFS_NAND_BLOCK_SIG_HEAD		0x44484B42 /* BKHD */
#define RAWFS_NAND_PAGE_SIG_HEAD		 0x44484750 /* PGHD */
#define RAWFS_NAND_PAGE_SIG_FOOT		 0x54464750 /* PGFT */
#define RAWFS_NAND_GC_MARKER_SIG_HEAD	0x44484347 /* GCHD */
#define RAWFS_NAND_PAGE_SIG_EMPTY		0xFFFFFFFF

enum rawfs_block_stat_enum {
	RAWFS_BLOCK_STAT_INVALID_HEAD  = 0,
	RAWFS_BLOCK_STAT_EMPTY		 = 1,
	RAWFS_BLOCK_STAT_INVALID_DATA  = 2,
	RAWFS_BLOCK_STAT_DATA		  = 3
};

enum rawfs_page_stat_enum {
	RAWFS_PAGE_STAT_EMPTY		 =  0,
	RAWFS_PAGE_STAT_DELETED		=  1,
	RAWFS_PAGE_STAT_VALID		 =  2,
	RAWFS_PAGE_STAT_BLOCK_HEAD	=  3,
	RAWFS_PAGE_STAT_GC_MARKER	 =  4,
	RAWFS_PAGE_STAT_UNCORRECTABLE =  5,
	RAWFS_PAGE_STAT_INVALID		=  6
};

struct rawfs_block_header {
	__u32 i_signature_head;
	__u32 i_rawfs_version;
	__u32 i_sequence_number;
	__u32 i_sequence_number_last;
	__u32 i_erase_count;
	__u32 i_crc;
} __packed;

struct rawfs_gc_marker_page {
	__u32 i_signature_head;
	__u32 i_src_block_index;
	__u32 i_src_block_sequence_number;
	__u32 i_src_block_erase_count;
	__u32 i_crc;
} __packed;

struct rawfs_file_info {   /* dentry */
	char			i_name[RAWFS_MAX_FILENAME_LEN+4];
	int			 i_chunk_index;
	int			 i_chunk_total;
	struct timespec	i_atime;
	struct timespec	i_mtime;
	struct timespec	i_ctime;
	umode_t		 i_mode;
	uid_t			i_uid;
	gid_t			i_gid;
	loff_t		  i_size;
	int			 i_parent_folder_id;
	int			 i_id; /* 0 for normal file */
};

struct rawfs_file_list_entry {
	struct list_head list;
	struct rawfs_file_info file_info;
	int i_location_block;		/* File Location: block */
	int i_location_page;		 /* File Location: starting page */
	int i_location_page_count;
};

struct rawfs_page {
	__u32	i_signature_head;
	__u32	i_crc;
	union {
		struct rawfs_file_info  i_file_info;
		__u8	padding[488];
	} i_info;
	__u8	i_data[1];
} __packed;


/* Inode operations */
int __init rawfs_init_inodecache(void);
void __exit rawfs_destroy_inodecache(void);
void rawfs_hash_init(struct super_block *sb);
struct inode *rawfs_alloc_inode(struct super_block *sb);
void rawfs_destroy_inode(struct inode *inode);
int rawfs_fill_inode(struct inode *inode,
	struct rawfs_file_info *file_info, int block_no, int page_no,
	umode_t mode, dev_t dev);
struct inode *rawfs_iget(struct super_block *sb, const char *name,
	int folder);

/* Mount-time analysis */
int rawfs_block_level_analysis(struct super_block *sb);
int rawfs_page_level_analysis(struct super_block *sb);
int rawfs_file_level_analysis(struct super_block *sb);
int rawfs_page_get(struct super_block *sb, int block_no, int page_no,
	struct rawfs_file_info *file_info, void *data);
int rawfs_block_is_valid(struct super_block *sb, int block_no,
	struct rawfs_block_header *block_head_out,
	struct rawfs_gc_marker_page *gc_page_out);

/* Device Operation */
int rawfs_dev_free_space(struct super_block *sb);
int rawfs_dev_garbage_collection(struct super_block *sb);
void rawfs_page_signature(struct super_block *sb, void *buf);

int rawfs_dev_mtd_erase_block(struct super_block *sb, int block_no);
int rawfs_dev_mtd_read_page_user(struct super_block *sb, int block_no,
	int block_offset, const struct iovec *iov, unsigned long nr_segs, int size);
int rawfs_dev_mtd_write_page(struct super_block *sb,
	int block_no, int page_no, void *buffer);
int rawfs_dev_mtd_read_page(struct super_block *sb,
	int block_no, int page_no, void *buffer);

int rawfs_dev_ram_erase_block(struct super_block *sb, int block_no);
int rawfs_dev_ram_read_page_user(struct super_block *sb, int block_no,
	int block_offset, const struct iovec *iov, unsigned long nr_segs, int size);
int rawfs_dev_ram_write_page(struct super_block *sb,
	int block_no, int page_no, void *buffer);
int rawfs_dev_ram_read_page(struct super_block *sb,
	int block_no, int page_no, void *buffer);

/* File Operations */
int rawfs_reg_file_delete(struct inode *dir, struct dentry *dentry);
int rawfs_reg_file_create(struct inode *dir, struct dentry *dentry,
	umode_t mode, struct nameidata *nd);
int rawfs_reg_file_copy(struct inode *src_dir, struct dentry *src_dentry,
	struct inode *dest_dir, struct dentry *dest_dentry);
int rawfs_reserve_space(struct super_block *sb, int chunks);
ssize_t
rawfs_reg_file_aio_read(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos);
ssize_t
rawfs_reg_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
	unsigned long nr_segs, loff_t pos);
ssize_t
rawfs_block_file_aio_read(struct kiocb *iocb, const struct iovec *iov,
	unsigned long nr_segs, loff_t pos);
ssize_t rawfs_block_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
	unsigned long nr_segs, loff_t pos);
int rawfs_file_sync(struct file *file, loff_t start, loff_t end,
	int datasync);
int rawfs_readdir(struct file *filp, struct dir_context *ctx);

/* dentry operations */
int rawfs_delete_dentry(const struct dentry *dentry);

/* File info */
void rawfs_fill_file_info(struct inode *inode,
	struct rawfs_file_info *file_info);
void rawfs_fill_fileinfo_by_dentry(struct dentry *dentry,
	struct rawfs_file_info *file_info);

/* File and Folder lists */
struct rawfs_file_list_entry *rawfs_file_list_get(struct super_block *sb,
	const char *name, int folder_id);
struct rawfs_file_list_entry *rawfs_file_list_get_by_id(
	struct super_block *sb, umode_t mode, int id);
void rawfs_file_list_init(struct super_block *sb);
int rawfs_file_list_add(struct super_block *sb,
	struct rawfs_file_info *fi, int block_no, int page_no);
void rawfs_file_list_remove(struct super_block *sb,
	struct rawfs_file_info *fi);
void rawfs_file_list_destroy(struct super_block *sb);
int rawfs_file_list_count(struct super_block *sb,
	unsigned int *entry_count, unsigned int *used_blocks,
	unsigned int *free_blocks);
__u32 rawfs_page_crc_data(struct super_block *sb, void *data_page);
__u32 rawfs_page_crc_gcmarker(struct super_block *sb, void *gcmarker_page);

/* Address Space Operations: Block file & Normal file */
int rawfs_readpage(struct file *filp, struct page *page);
int rawfs_write_begin(struct file *filp, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned flags,
			struct page **pagep, void **fsdata);
int rawfs_write_end(struct file *filp, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned copied,
			struct page *pg, void *fsdata);

/* Case in-sensitive dentry operations */
int rawfs_ci_hash(const struct dentry *dentry, struct qstr *q);
int rawfs_compare_dentry(const struct dentry *parent,
	const struct dentry *dentry, unsigned int len, const char *str,
	const struct qstr *name);
int rawfs_delete_dentry(const struct dentry *dentry);

/* Utility */
uint32_t rawfs_div(uint64_t n, uint32_t base);

#endif
