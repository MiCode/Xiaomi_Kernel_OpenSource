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

/************************************************************************/
/*                                                                      */
/*  PROJECT : exFAT & FAT12/16/32 File System                           */
/*  FILE    : core_fat.c                                                */
/*  PURPOSE : FAT-fs core code for sdFAT                                */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*  NOTES                                                               */
/*                                                                      */
/*                                                                      */
/************************************************************************/

#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/log2.h>

#include "sdfat.h"
#include "core.h"
#include <asm/byteorder.h>
#include <asm/unaligned.h>

/*----------------------------------------------------------------------*/
/*  Constant & Macro Definitions                                        */
/*----------------------------------------------------------------------*/
#define	MAX_LFN_ORDER	(20)

/*
 * MAX_EST_AU_SECT should be changed according to 32/64bits.
 * On 32bit, 4KB page supports 512 clusters per AU.
 * But, on 64bit, 4KB page can handle a half of total list_head of 32bit's.
 * Bcause the size of list_head structure on 64bit increases twofold over 32bit.
 */
#if (BITS_PER_LONG == 64)
//#define MAX_EST_AU_SECT	(16384) /* upto 8MB */
#define MAX_EST_AU_SECT	(32768) /* upto 16MB, used more page for list_head */
#else
#define MAX_EST_AU_SECT	(32768) /* upto 16MB */
#endif

/*======================================================================*/
/*  Local Function Declarations                                         */
/*======================================================================*/
static s32 __extract_uni_name_from_ext_entry(EXT_DENTRY_T *, u16 *, s32);

/*----------------------------------------------------------------------*/
/*  Global Variable Definitions                                         */
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*  Local Variable Definitions                                          */
/*----------------------------------------------------------------------*/

/*======================================================================*/
/*  Local Function Definitions                                          */
/*======================================================================*/
static u32 __calc_default_au_size(struct super_block *sb)
{
	struct block_device *bdev = sb->s_bdev;
	struct gendisk *disk;
	struct request_queue *queue;
	struct queue_limits *limit;
	unsigned int est_au_sect = MAX_EST_AU_SECT;
	unsigned int est_au_size = 0;
	unsigned int queue_au_size = 0;
	sector_t total_sect = 0;

	/* we assumed that sector size is 512 bytes */

	disk = bdev->bd_disk;
	if (!disk)
		goto out;

	queue = disk->queue;
	if (!queue)
		goto out;

	limit = &queue->limits;
	queue_au_size = limit->discard_granularity;

	/* estimate function(x) =
	 * (total_sect / 2) * 512 / 1024
	 * => (total_sect >> 1) >> 1)
	 * => (total_sect >> 2)
	 * => estimated bytes size
	 *
	 * ex1) <=  8GB -> 4MB
	 * ex2)    16GB -> 8MB
	 * ex3) >= 32GB -> 16MB
	 */
	total_sect = disk->part0.nr_sects;
	est_au_size = total_sect >> 2;

	/* au_size assumed that bytes per sector is 512 */
	est_au_sect = est_au_size >> 9;

	MMSG("DBG1: total_sect(%llu) est_au_size(%u) est_au_sect(%u)\n",
			(u64)total_sect, est_au_size, est_au_sect);

	if (est_au_sect <= 8192) {
		/* 4MB */
		est_au_sect = 8192;
	} else if (est_au_sect <= 16384) {
		/* 8MB */
		est_au_sect = 16384;
	} else {
		/* 8MB or 16MB */
		est_au_sect = MAX_EST_AU_SECT;
	}

	MMSG("DBG2: total_sect(%llu) est_au_size(%u) est_au_sect(%u)\n",
			(u64)total_sect, est_au_size, est_au_sect);

	if (est_au_size < queue_au_size &&
			queue_au_size <= (MAX_EST_AU_SECT << 9)) {
		DMSG("use queue_au_size(%u) instead of est_au_size(%u)\n",
				queue_au_size, est_au_size);
		est_au_sect = queue_au_size >> 9;
	}

out:
	if (sb->s_blocksize != 512) {
		ASSERT(sb->s_blocksize_bits > 9);
		sdfat_log_msg(sb, KERN_INFO,
			"adjustment est_au_size by logical block size(%lu)",
			sb->s_blocksize);
		est_au_sect >>= (sb->s_blocksize_bits - 9);
	}

	sdfat_log_msg(sb, KERN_INFO, "set default AU sectors   : %u "
		"(queue_au_size : %u KB, disk_size : %llu MB)",
		est_au_sect, queue_au_size >> 10, (u64)(total_sect >> 11));
	return est_au_sect;
}


/*
 *  Cluster Management Functions
 */
static s32 fat_free_cluster(struct super_block *sb, CHAIN_T *p_chain, s32 do_relse)
{
	s32 ret = -EIO;
	s32 num_clusters = 0;
	u32 clu, prev;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	s32 i;
	u64 sector;

	/* invalid cluster number */
	if (IS_CLUS_FREE(p_chain->dir) || IS_CLUS_EOF(p_chain->dir))
		return 0;

	/* no cluster to truncate */
	if (!p_chain->size) {
		DMSG("%s: cluster(%u) truncation is not required.",
			__func__, p_chain->dir);
		return 0;
	}

	/* check cluster validation */
	if ((p_chain->dir < 2) && (p_chain->dir >= fsi->num_clusters)) {
		EMSG("%s: invalid start cluster (%u)\n", __func__, p_chain->dir);
		sdfat_debug_bug_on(1);
		return -EIO;
	}


	set_sb_dirty(sb);
	clu = p_chain->dir;

	do {
		if (do_relse) {
			sector = CLUS_TO_SECT(fsi, clu);
			for (i = 0; i < fsi->sect_per_clus; i++) {
				if (dcache_release(sb, sector+i) == -EIO)
					goto out;
			}
		}

		prev = clu;
		if (get_next_clus_safe(sb, &clu)) {
			/* print more helpful log */
			if (IS_CLUS_BAD(clu)) {
				sdfat_log_msg(sb, KERN_ERR, "%s : "
					"deleting bad cluster (clu[%u]->BAD)",
					__func__, prev);
			} else if (IS_CLUS_FREE(clu)) {
				sdfat_log_msg(sb, KERN_ERR, "%s : "
					"deleting free cluster (clu[%u]->FREE)",
					__func__, prev);
			}
			goto out;
		}

		/* Free FAT chain */
		if (fat_ent_set(sb, prev, CLUS_FREE))
			goto out;

		/* Update AMAP if needed */
		if (fsi->amap) {
			if (amap_release_cluster(sb, prev))
				return -EIO;
		}

		num_clusters++;

	} while (!IS_CLUS_EOF(clu));

	/* success */
	ret = 0;
out:
	fsi->used_clusters -= num_clusters;
	return ret;
} /* end of fat_free_cluster */

static s32 fat_alloc_cluster(struct super_block *sb, u32 num_alloc, CHAIN_T *p_chain, s32 dest)
{
	s32 ret = -ENOSPC;
	u32 i, num_clusters = 0, total_cnt;
	u32 new_clu, last_clu = CLUS_EOF, read_clu;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	total_cnt = fsi->num_clusters - CLUS_BASE;

	if (unlikely(total_cnt < fsi->used_clusters)) {
		sdfat_fs_error_ratelimit(sb,
				"%s : invalid used clusters(t:%u,u:%u)\n",
				__func__, total_cnt, fsi->used_clusters);
		return -EIO;
	}

	if (num_alloc > total_cnt - fsi->used_clusters)
		return -ENOSPC;

	new_clu = p_chain->dir;
	if (IS_CLUS_EOF(new_clu))
		new_clu = fsi->clu_srch_ptr;
	else if (new_clu >= fsi->num_clusters)
		new_clu = CLUS_BASE;

	set_sb_dirty(sb);

	p_chain->dir = CLUS_EOF;

	for (i = CLUS_BASE; i < fsi->num_clusters; i++) {
		if (fat_ent_get(sb, new_clu, &read_clu)) {
			ret = -EIO;
			goto error;
		}

		if (IS_CLUS_FREE(read_clu)) {
			if (fat_ent_set(sb, new_clu, CLUS_EOF)) {
				ret = -EIO;
				goto error;
			}
			num_clusters++;

			if (IS_CLUS_EOF(p_chain->dir)) {
				p_chain->dir = new_clu;
			} else {
				if (fat_ent_set(sb, last_clu, new_clu)) {
					ret = -EIO;
					goto error;
				}
			}

			last_clu = new_clu;

			if ((--num_alloc) == 0) {
				fsi->clu_srch_ptr = new_clu;
				fsi->used_clusters += num_clusters;

				return 0;
			}
		}
		if ((++new_clu) >= fsi->num_clusters)
			new_clu = CLUS_BASE;
	}
error:
	if (num_clusters)
		fat_free_cluster(sb, p_chain, 0);
	return ret;
} /* end of fat_alloc_cluster */

static s32 fat_count_used_clusters(struct super_block *sb, u32 *ret_count)
{
	s32 i;
	u32 clu, count = 0;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	for (i = CLUS_BASE; i < fsi->num_clusters; i++) {
		if (fat_ent_get(sb, i, &clu))
			return -EIO;

		if (!IS_CLUS_FREE(clu))
			count++;
	}

	*ret_count = count;
	return 0;
} /* end of fat_count_used_clusters */


/*
 *  Directory Entry Management Functions
 */
static u32 fat_get_entry_type(DENTRY_T *p_entry)
{
	DOS_DENTRY_T *ep = (DOS_DENTRY_T *)p_entry;

	/* first byte of 32bytes dummy */
	if (*(ep->name) == MSDOS_UNUSED)
		return TYPE_UNUSED;

	/* 0xE5 of Kanji Japanese is replaced to 0x05 */
	else if (*(ep->name) == MSDOS_DELETED)
		return TYPE_DELETED;

	/* 11th byte of 32bytes dummy */
	else if ((ep->attr & ATTR_EXTEND_MASK) == ATTR_EXTEND)
		return TYPE_EXTEND;

	else if (!(ep->attr & (ATTR_SUBDIR | ATTR_VOLUME)))
		return TYPE_FILE;

	else if ((ep->attr & (ATTR_SUBDIR | ATTR_VOLUME)) == ATTR_SUBDIR)
		return TYPE_DIR;

	else if ((ep->attr & (ATTR_SUBDIR | ATTR_VOLUME)) == ATTR_VOLUME)
		return TYPE_VOLUME;

	return TYPE_INVALID;
} /* end of fat_get_entry_type */

static void fat_set_entry_type(DENTRY_T *p_entry, u32 type)
{
	DOS_DENTRY_T *ep = (DOS_DENTRY_T *)p_entry;

	if (type == TYPE_UNUSED)
		*(ep->name) = MSDOS_UNUSED; /* 0x0 */

	else if (type == TYPE_DELETED)
		*(ep->name) = MSDOS_DELETED; /* 0xE5 */

	else if (type == TYPE_EXTEND)
		ep->attr = ATTR_EXTEND;

	else if (type == TYPE_DIR)
		ep->attr = ATTR_SUBDIR;

	else if (type == TYPE_FILE)
		ep->attr = ATTR_ARCHIVE;

	else if (type == TYPE_SYMLINK)
		ep->attr = ATTR_ARCHIVE | ATTR_SYMLINK;
} /* end of fat_set_entry_type */

static u32 fat_get_entry_attr(DENTRY_T *p_entry)
{
	DOS_DENTRY_T *ep = (DOS_DENTRY_T *)p_entry;

	return (u32)ep->attr;
} /* end of fat_get_entry_attr */

static void fat_set_entry_attr(DENTRY_T *p_entry, u32 attr)
{
	DOS_DENTRY_T *ep = (DOS_DENTRY_T *)p_entry;

	ep->attr = (u8)attr;
} /* end of fat_set_entry_attr */

static u8 fat_get_entry_flag(DENTRY_T *p_entry)
{
	return 0x01;
} /* end of fat_get_entry_flag */

static void fat_set_entry_flag(DENTRY_T *p_entry, u8 flags)
{
} /* end of fat_set_entry_flag */

static u32 fat_get_entry_clu0(DENTRY_T *p_entry)
{
	DOS_DENTRY_T *ep = (DOS_DENTRY_T *)p_entry;
	/* FIXME : is ok? */
	return(((u32)(le16_to_cpu(ep->start_clu_hi)) << 16) | le16_to_cpu(ep->start_clu_lo));
} /* end of fat_get_entry_clu0 */

static void fat_set_entry_clu0(DENTRY_T *p_entry, u32 start_clu)
{
	DOS_DENTRY_T *ep = (DOS_DENTRY_T *)p_entry;

	ep->start_clu_lo = cpu_to_le16(CLUSTER_16(start_clu));
	ep->start_clu_hi = cpu_to_le16(CLUSTER_16(start_clu >> 16));
} /* end of fat_set_entry_clu0 */

static u64 fat_get_entry_size(DENTRY_T *p_entry)
{
	DOS_DENTRY_T *ep = (DOS_DENTRY_T *)p_entry;

	return (u64)le32_to_cpu(ep->size);
} /* end of fat_get_entry_size */

static void fat_set_entry_size(DENTRY_T *p_entry, u64 size)
{
	DOS_DENTRY_T *ep = (DOS_DENTRY_T *)p_entry;

	ep->size = cpu_to_le32((u32)size);
} /* end of fat_set_entry_size */

static void fat_get_entry_time(DENTRY_T *p_entry, TIMESTAMP_T *tp, u8 mode)
{
	u16 t = 0x00, d = 0x21;
	DOS_DENTRY_T *ep = (DOS_DENTRY_T *) p_entry;

	switch (mode) {
	case TM_CREATE:
		t = le16_to_cpu(ep->create_time);
		d = le16_to_cpu(ep->create_date);
		break;
	case TM_MODIFY:
		t = le16_to_cpu(ep->modify_time);
		d = le16_to_cpu(ep->modify_date);
		break;
	}

	tp->sec  = (t & 0x001F) << 1;
	tp->min  = (t >> 5) & 0x003F;
	tp->hour = (t >> 11);
	tp->day  = (d & 0x001F);
	tp->mon  = (d >> 5) & 0x000F;
	tp->year = (d >> 9);
} /* end of fat_get_entry_time */

static void fat_set_entry_time(DENTRY_T *p_entry, TIMESTAMP_T *tp, u8 mode)
{
	u16 t, d;
	DOS_DENTRY_T *ep = (DOS_DENTRY_T *) p_entry;

	t = (tp->hour << 11) | (tp->min << 5) | (tp->sec >> 1);
	d = (tp->year <<  9) | (tp->mon << 5) |  tp->day;

	switch (mode) {
	case TM_CREATE:
		ep->create_time = cpu_to_le16(t);
		ep->create_date = cpu_to_le16(d);
		break;
	case TM_MODIFY:
		ep->modify_time = cpu_to_le16(t);
		ep->modify_date = cpu_to_le16(d);
		break;
	}
} /* end of fat_set_entry_time */

static void __init_dos_entry(struct super_block *sb, DOS_DENTRY_T *ep, u32 type, u32 start_clu)
{
	TIMESTAMP_T tm, *tp;

	fat_set_entry_type((DENTRY_T *) ep, type);
	ep->start_clu_lo = cpu_to_le16(CLUSTER_16(start_clu));
	ep->start_clu_hi = cpu_to_le16(CLUSTER_16(start_clu >> 16));
	ep->size = 0;

	tp = tm_now(SDFAT_SB(sb), &tm);
	fat_set_entry_time((DENTRY_T *) ep, tp, TM_CREATE);
	fat_set_entry_time((DENTRY_T *) ep, tp, TM_MODIFY);
	ep->access_date = 0;
	ep->create_time_ms = 0;
} /* end of __init_dos_entry */

static void __init_ext_entry(EXT_DENTRY_T *ep, s32 order, u8 chksum, u16 *uniname)
{
	s32 i;
	u8 end = false;

	fat_set_entry_type((DENTRY_T *) ep, TYPE_EXTEND);
	ep->order = (u8) order;
	ep->sysid = 0;
	ep->checksum = chksum;
	ep->start_clu = 0;

	/* unaligned name */
	for (i = 0; i < 5; i++) {
		if (!end) {
			put_unaligned_le16(*uniname, &(ep->unicode_0_4[i<<1]));
			if (*uniname == 0x0)
				end = true;
			else
				uniname++;
		} else {
			put_unaligned_le16(0xFFFF, &(ep->unicode_0_4[i<<1]));
		}
	}

	/* aligned name */
	for (i = 0; i < 6; i++) {
		if (!end) {
			ep->unicode_5_10[i] = cpu_to_le16(*uniname);
			if (*uniname == 0x0)
				end = true;
			else
				uniname++;
		} else {
			ep->unicode_5_10[i] = cpu_to_le16(0xFFFF);
		}
	}

	/* aligned name */
	for (i = 0; i < 2; i++) {
		if (!end) {
			ep->unicode_11_12[i] = cpu_to_le16(*uniname);
			if (*uniname == 0x0)
				end = true;
			else
				uniname++;
		} else {
			ep->unicode_11_12[i] = cpu_to_le16(0xFFFF);
		}
	}
} /* end of __init_ext_entry */

static s32 fat_init_dir_entry(struct super_block *sb, CHAIN_T *p_dir, s32 entry, u32 type,
						 u32 start_clu, u64 size)
{
	u64 sector;
	DOS_DENTRY_T *dos_ep;

	dos_ep = (DOS_DENTRY_T *) get_dentry_in_dir(sb, p_dir, entry, &sector);
	if (!dos_ep)
		return -EIO;

	__init_dos_entry(sb, dos_ep, type, start_clu);
	dcache_modify(sb, sector);

	return 0;
} /* end of fat_init_dir_entry */

static s32 fat_init_ext_entry(struct super_block *sb, CHAIN_T *p_dir, s32 entry, s32 num_entries,
						 UNI_NAME_T *p_uniname, DOS_NAME_T *p_dosname)
{
	s32 i;
	u64 sector;
	u8 chksum;
	u16 *uniname = p_uniname->name;
	DOS_DENTRY_T *dos_ep;
	EXT_DENTRY_T *ext_ep;

	dos_ep = (DOS_DENTRY_T *) get_dentry_in_dir(sb, p_dir, entry, &sector);
	if (!dos_ep)
		return -EIO;

	dos_ep->lcase = p_dosname->name_case;
	memcpy(dos_ep->name, p_dosname->name, DOS_NAME_LENGTH);
	if (dcache_modify(sb, sector))
		return -EIO;

	if ((--num_entries) > 0) {
		chksum = calc_chksum_1byte((void *) dos_ep->name, DOS_NAME_LENGTH, 0);

		for (i = 1; i < num_entries; i++) {
			ext_ep = (EXT_DENTRY_T *) get_dentry_in_dir(sb, p_dir, entry-i, &sector);
			if (!ext_ep)
				return -EIO;

			__init_ext_entry(ext_ep, i, chksum, uniname);
			if (dcache_modify(sb, sector))
				return -EIO;
			uniname += 13;
		}

		ext_ep = (EXT_DENTRY_T *) get_dentry_in_dir(sb, p_dir, entry-i, &sector);
		if (!ext_ep)
			return -EIO;

		__init_ext_entry(ext_ep, i+MSDOS_LAST_LFN, chksum, uniname);
		if (dcache_modify(sb, sector))
			return -EIO;
	}

	return 0;
} /* end of fat_init_ext_entry */

static s32 fat_delete_dir_entry(struct super_block *sb, CHAIN_T *p_dir, s32 entry, s32 order, s32 num_entries)
{
	s32 i;
	u64 sector;
	DENTRY_T *ep;

	for (i = num_entries-1; i >= order; i--) {
		ep = get_dentry_in_dir(sb, p_dir, entry-i, &sector);
		if (!ep)
			return -EIO;

		fat_set_entry_type(ep, TYPE_DELETED);
		if (dcache_modify(sb, sector))
			return -EIO;
	}

	return 0;
}

/* return values of fat_find_dir_entry()
 * >= 0 : return dir entiry position with the name in dir
 * -EEXIST : (root dir, ".") it is the root dir itself
 * -ENOENT : entry with the name does not exist
 * -EIO    : I/O error
 */
static inline s32 __get_dentries_per_clu(FS_INFO_T *fsi, s32 clu)
{
	if (IS_CLUS_FREE(clu)) /* FAT16 root_dir */
		return fsi->dentries_in_root;

	return fsi->dentries_per_clu;
}

static s32 fat_find_dir_entry(struct super_block *sb, FILE_ID_T *fid,
		CHAIN_T *p_dir, UNI_NAME_T *p_uniname, s32 num_entries, DOS_NAME_T *p_dosname, u32 type)
{
	s32 i, rewind = 0, dentry = 0, end_eidx = 0;
	s32 chksum = 0, lfn_ord = 0, lfn_len = 0;
	s32 dentries_per_clu, num_empty = 0;
	u32 entry_type;
	u16 entry_uniname[14], *uniname = NULL;
	CHAIN_T clu;
	DENTRY_T *ep;
	HINT_T *hint_stat = &fid->hint_stat;
	HINT_FEMP_T candi_empty;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	/*
	 * REMARK:
	 * DOT and DOTDOT are handled by VFS layer
	 */

	dentries_per_clu = __get_dentries_per_clu(fsi, p_dir->dir);
	clu.dir = p_dir->dir;
	clu.flags = p_dir->flags;

	if (hint_stat->eidx) {
		clu.dir = hint_stat->clu;
		dentry = hint_stat->eidx;
		end_eidx = dentry;
	}

	candi_empty.eidx = -1;

	MMSG("lookup dir= %s\n", p_dosname->name);
rewind:
	while (!IS_CLUS_EOF(clu.dir)) {
		i = dentry % dentries_per_clu;
		for (; i < dentries_per_clu; i++, dentry++) {
			if (rewind && (dentry == end_eidx))
				goto not_found;

			ep = get_dentry_in_dir(sb, &clu, i, NULL);
			if (!ep)
				return -EIO;

			entry_type = fat_get_entry_type(ep);

			/*
			 * Most directory entries have long name,
			 * So, we check extend directory entry first.
			 */
			if (entry_type == TYPE_EXTEND) {
				EXT_DENTRY_T *ext_ep = (EXT_DENTRY_T *)ep;
				u32 cur_ord = (u32)ext_ep->order;
				u32 cur_chksum = (s32)ext_ep->checksum;
				s32 len = 13;
				u16 unichar;

				num_empty = 0;
				candi_empty.eidx = -1;

				/* check whether new lfn or not */
				if (cur_ord & MSDOS_LAST_LFN) {
					cur_ord &= ~(MSDOS_LAST_LFN);
					chksum = cur_chksum;
					len = (13 * (cur_ord-1));
					uniname = (p_uniname->name + len);
					lfn_ord = cur_ord + 1;
					lfn_len = 0;

					/* check minimum name length */
					if (cur_ord &&
						(len > p_uniname->name_len)) {
						/* MISMATCHED NAME LENGTH */
						lfn_len = -1;
					}
					len = 0;
				}

				/* invalid lfn order */
				if (!cur_ord || (cur_ord > MAX_LFN_ORDER) ||
					((cur_ord + 1) != lfn_ord))
					goto reset_dentry_set;

				/* check checksum of directory entry set */
				if (cur_chksum != chksum)
					goto reset_dentry_set;

				/* update order for next dentry */
				lfn_ord = cur_ord;

				/* check whether mismatched lfn or not */
				if (lfn_len == -1) {
					/* MISMATCHED LFN DENTRY SET */
					continue;
				}

				if (!uniname) {
					sdfat_fs_error(sb,
						"%s : abnormal dentry "
						"(start_clu[%u], "
						"idx[%u])", __func__,
						p_dir->dir, dentry);
					sdfat_debug_bug_on(1);
					return -EIO;
				}

				/* update position of name buffer */
				uniname -= len;

				/* get utf16 characters saved on this entry */
				len = __extract_uni_name_from_ext_entry(ext_ep, entry_uniname, lfn_ord);

				/* replace last char to null */
				unichar = *(uniname+len);
				*(uniname+len) = (u16)0x0;

				/* uniname ext_dentry unit compare repeatdly */
				if (nls_cmp_uniname(sb, uniname, entry_uniname)) {
					/* DO HANDLE WRONG NAME */
					lfn_len = -1;
				} else {
					/* add matched chars length */
					lfn_len += len;
				}

				/* restore previous character */
				*(uniname+len) = unichar;

				/* jump to check next dentry */
				continue;

			} else if ((entry_type == TYPE_FILE) || (entry_type == TYPE_DIR)) {
				DOS_DENTRY_T *dos_ep = (DOS_DENTRY_T *)ep;
				u32 cur_chksum = (s32)calc_chksum_1byte(
							(void *) dos_ep->name,
							DOS_NAME_LENGTH, 0);

				num_empty = 0;
				candi_empty.eidx = -1;

				MMSG("checking dir= %c%c%c%c%c%c%c%c%c%c%c\n",
					dos_ep->name[0], dos_ep->name[1],
					dos_ep->name[2], dos_ep->name[3],
					dos_ep->name[4], dos_ep->name[5],
					dos_ep->name[6], dos_ep->name[7],
					dos_ep->name[8], dos_ep->name[9],
					dos_ep->name[10]);

				/*
				 * if there is no valid long filename,
				 * we should check short filename.
				 */
				if (!lfn_len || (cur_chksum != chksum)) {
					/* check shortname */
					if ((p_dosname->name[0] != '\0') &&
						!nls_cmp_sfn(sb,
							p_dosname->name,
							dos_ep->name)) {
						goto found;
					}
				/* check name length */
				} else if ((lfn_len > 0) &&
						((s32)p_uniname->name_len ==
						 lfn_len)) {
					goto found;
				}

				/* DO HANDLE MISMATCHED SFN, FALL THROUGH */
			} else if ((entry_type == TYPE_UNUSED) || (entry_type == TYPE_DELETED)) {
				num_empty++;
				if (candi_empty.eidx == -1) {
					if (num_empty == 1) {
						candi_empty.cur.dir = clu.dir;
						candi_empty.cur.size = clu.size;
						candi_empty.cur.flags = clu.flags;
					}

					if (num_empty >= num_entries) {
						candi_empty.eidx = dentry - (num_empty - 1);
						ASSERT(0 <= candi_empty.eidx);
						candi_empty.count = num_empty;

						if ((fid->hint_femp.eidx == -1) ||
								(candi_empty.eidx <= fid->hint_femp.eidx)) {
							memcpy(&fid->hint_femp,
									&candi_empty,
									sizeof(HINT_FEMP_T));
						}
					}
				}

				if (entry_type == TYPE_UNUSED)
					goto not_found;
				/* FALL THROUGH */
			}
reset_dentry_set:
			/* TYPE_DELETED, TYPE_VOLUME OR MISMATCHED SFN */
			lfn_ord = 0;
			lfn_len = 0;
			chksum = 0;
		}

		if (IS_CLUS_FREE(p_dir->dir))
			break; /* FAT16 root_dir */

		if (get_next_clus_safe(sb, &clu.dir))
			return -EIO;
	}

not_found:
	/* we started at not 0 index,so we should try to find target
	 * from 0 index to the index we started at.
	 */
	if (!rewind && end_eidx) {
		rewind = 1;
		dentry = 0;
		clu.dir = p_dir->dir;
		/* reset dentry set */
		lfn_ord = 0;
		lfn_len = 0;
		chksum = 0;
		/* reset empty hint_*/
		num_empty = 0;
		candi_empty.eidx = -1;
		goto rewind;
	}

	/* initialized hint_stat */
	hint_stat->clu = p_dir->dir;
	hint_stat->eidx = 0;
	return -ENOENT;

found:
	/* next dentry we'll find is out of this cluster */
	if (!((dentry + 1) % dentries_per_clu)) {
		int ret = 0;
		/* FAT16 root_dir */
		if (IS_CLUS_FREE(p_dir->dir))
			clu.dir = CLUS_EOF;
		else
			ret = get_next_clus_safe(sb, &clu.dir);

		if (ret || IS_CLUS_EOF(clu.dir)) {
			/* just initialized hint_stat */
			hint_stat->clu = p_dir->dir;
			hint_stat->eidx = 0;
			return dentry;
		}
	}

	hint_stat->clu = clu.dir;
	hint_stat->eidx = dentry + 1;
	return dentry;
} /* end of fat_find_dir_entry */

/* returns -EIO on error */
static s32 fat_count_ext_entries(struct super_block *sb, CHAIN_T *p_dir, s32 entry, DENTRY_T *p_entry)
{
	s32 count = 0;
	u8 chksum;
	DOS_DENTRY_T *dos_ep = (DOS_DENTRY_T *) p_entry;
	EXT_DENTRY_T *ext_ep;

	chksum = calc_chksum_1byte((void *) dos_ep->name, DOS_NAME_LENGTH, 0);

	for (entry--; entry >= 0; entry--) {
		ext_ep = (EXT_DENTRY_T *)get_dentry_in_dir(sb, p_dir, entry, NULL);
		if (!ext_ep)
			return -EIO;

		if ((fat_get_entry_type((DENTRY_T *)ext_ep) == TYPE_EXTEND) &&
			(ext_ep->checksum == chksum)) {
			count++;
			if (ext_ep->order > MSDOS_LAST_LFN)
				return count;
		} else {
			return count;
		}
	}

	return count;
}


/*
 *  Name Conversion Functions
 */
static s32 __extract_uni_name_from_ext_entry(EXT_DENTRY_T *ep, u16 *uniname, s32 order)
{
	s32 i, len = 0;

	for (i = 0; i < 5; i++) {
		*uniname = get_unaligned_le16(&(ep->unicode_0_4[i<<1]));
		if (*uniname == 0x0)
			return len;
		uniname++;
		len++;
	}

	if (order < 20) {
		for (i = 0; i < 6; i++) {
			/* FIXME : unaligned? */
			*uniname = le16_to_cpu(ep->unicode_5_10[i]);
			if (*uniname == 0x0)
				return len;
			uniname++;
			len++;
		}
	} else {
		for (i = 0; i < 4; i++) {
			/* FIXME : unaligned? */
			*uniname = le16_to_cpu(ep->unicode_5_10[i]);
			if (*uniname == 0x0)
				return len;
			uniname++;
			len++;
		}
		*uniname = 0x0; /* uniname[MAX_NAME_LENGTH] */
		return len;
	}

	for (i = 0; i < 2; i++) {
		/* FIXME : unaligned? */
		*uniname = le16_to_cpu(ep->unicode_11_12[i]);
		if (*uniname == 0x0)
			return len;
		uniname++;
		len++;
	}

	*uniname = 0x0;
	return len;

} /* end of __extract_uni_name_from_ext_entry */

static void fat_get_uniname_from_ext_entry(struct super_block *sb, CHAIN_T *p_dir, s32 entry, u16 *uniname)
{
	u32 i;
	u16 *name = uniname;
	u32 chksum;

	DOS_DENTRY_T *dos_ep =
		(DOS_DENTRY_T *)get_dentry_in_dir(sb, p_dir, entry, NULL);

	if (unlikely(!dos_ep))
		goto invalid_lfn;

	chksum = (u32)calc_chksum_1byte(
				(void *) dos_ep->name,
				DOS_NAME_LENGTH, 0);

	for (entry--, i = 1; entry >= 0; entry--, i++) {
		EXT_DENTRY_T *ep;

		ep = (EXT_DENTRY_T *)get_dentry_in_dir(sb, p_dir, entry, NULL);
		if (!ep)
			goto invalid_lfn;

		if (fat_get_entry_type((DENTRY_T *) ep) != TYPE_EXTEND)
			goto invalid_lfn;

		if (chksum != (u32)ep->checksum)
			goto invalid_lfn;

		if (i != (u32)(ep->order & ~(MSDOS_LAST_LFN)))
			goto invalid_lfn;

		__extract_uni_name_from_ext_entry(ep, name, (s32)i);
		if (ep->order & MSDOS_LAST_LFN)
			return;

		name += 13;
	}
invalid_lfn:
	*uniname = (u16)0x0;
} /* end of fat_get_uniname_from_ext_entry */

/* Find if the shortname exists
 * and check if there are free entries
 */
static s32 __fat_find_shortname_entry(struct super_block *sb, CHAIN_T *p_dir,
		u8 *p_dosname, s32 *offset, __attribute__((unused))int n_entry_needed)
{
	u32 type;
	s32 i, dentry = 0;
	s32 dentries_per_clu;
	DENTRY_T *ep = NULL;
	DOS_DENTRY_T *dos_ep = NULL;
	CHAIN_T clu = *p_dir;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	if (offset)
		*offset = -1;

	if (IS_CLUS_FREE(clu.dir)) /* FAT16 root_dir */
		dentries_per_clu = fsi->dentries_in_root;
	else
		dentries_per_clu = fsi->dentries_per_clu;

	while (!IS_CLUS_EOF(clu.dir)) {
		for (i = 0; i < dentries_per_clu; i++, dentry++) {
			ep = get_dentry_in_dir(sb, &clu, i, NULL);
			if (!ep)
				return -EIO;

			type = fat_get_entry_type(ep);

			if ((type == TYPE_FILE) || (type == TYPE_DIR))  {
				dos_ep = (DOS_DENTRY_T *)ep;
				if (!nls_cmp_sfn(sb, p_dosname, dos_ep->name)) {
					if (offset)
						*offset = dentry;
					return 0;
				}
			}
		}

		/* fat12/16 root dir */
		if (IS_CLUS_FREE(clu.dir))
			break;

		if (get_next_clus_safe(sb, &clu.dir))
			return -EIO;
	}
	return -ENOENT;
}

#ifdef CONFIG_SDFAT_FAT32_SHORTNAME_SEQ
static void __fat_attach_count_to_dos_name(u8 *dosname, s32 count)
{
	s32 i, j, length;
	s8 str_count[6];

	snprintf(str_count, sizeof(str_count), "~%d", count);
	length = strlen(str_count);

	i = j = 0;
	while (j <= (8 - length)) {
		i = j;
		if (dosname[j] == ' ')
			break;
		if (dosname[j] & 0x80)
			j += 2;
		else
			j++;
	}

	for (j = 0; j < length; i++, j++)
		dosname[i] = (u8) str_count[j];

	if (i == 7)
		dosname[7] = ' ';

} /* end of __fat_attach_count_to_dos_name */
#endif

s32 fat_generate_dos_name_new(struct super_block *sb, CHAIN_T *p_dir, DOS_NAME_T *p_dosname, s32 n_entry_needed)
{
	s32 i;
	s32 baselen, err;
	u8 work[DOS_NAME_LENGTH], buf[5];
	u8 tail;

	baselen = 8;
	memset(work, ' ', DOS_NAME_LENGTH);
	memcpy(work, p_dosname->name, DOS_NAME_LENGTH);

	while (baselen && (work[--baselen] == ' ')) {
		/* DO NOTHING, JUST FOR CHECK_PATCH */
	}

	if (baselen > 6)
		baselen = 6;

	BUG_ON(baselen < 0);

#ifdef CONFIG_SDFAT_FAT32_SHORTNAME_SEQ
	/* example) namei_exfat.c -> NAMEI_~1 - NAMEI_~9 */
	work[baselen] = '~';
	for (i = 1; i < 10; i++) {
		// '0' + i = 1 ~ 9 ASCII
		work[baselen + 1] = '0' + i;
		err = __fat_find_shortname_entry(sb, p_dir, work, NULL, n_entry_needed);
		if (err == -ENOENT) {
			/* void return */
			__fat_attach_count_to_dos_name(p_dosname->name, i);
			return 0;
		}

		/* any other error */
		if (err)
			return err;
	}
#endif

	i = jiffies;
	tail = (jiffies >> 16) & 0x7;

	if (baselen > 2)
		baselen = 2;

	BUG_ON(baselen < 0);

	work[baselen + 4] = '~';
	// 1 ~ 8 ASCII
	work[baselen + 5] = '1' + tail;
	while (1) {
		snprintf(buf, sizeof(buf), "%04X", i & 0xffff);
		memcpy(&work[baselen], buf, 4);
		err = __fat_find_shortname_entry(sb, p_dir, work, NULL, n_entry_needed);
		if (err == -ENOENT) {
			memcpy(p_dosname->name, work, DOS_NAME_LENGTH);
			break;
		}

		/* any other error */
		if (err)
			return err;

		i -= 11;
	}
	return 0;
} /* end of generate_dos_name_new */

static s32 fat_calc_num_entries(UNI_NAME_T *p_uniname)
{
	s32 len;

	len = p_uniname->name_len;
	if (len == 0)
		return 0;

	/* 1 dos name entry + extended entries */
	return((len-1) / 13 + 2);

} /* end of calc_num_enties */

static s32 fat_check_max_dentries(FILE_ID_T *fid)
{
	if ((fid->size >> DENTRY_SIZE_BITS) >= MAX_FAT_DENTRIES) {
		/* FAT spec allows a dir to grow upto 65536 dentries */
		return -ENOSPC;
	}
	return 0;
} /* end of check_max_dentries */


/*
 *  File Operation Functions
 */
static FS_FUNC_T fat_fs_func = {
	.alloc_cluster = fat_alloc_cluster,
	.free_cluster = fat_free_cluster,
	.count_used_clusters = fat_count_used_clusters,

	.init_dir_entry = fat_init_dir_entry,
	.init_ext_entry = fat_init_ext_entry,
	.find_dir_entry = fat_find_dir_entry,
	.delete_dir_entry = fat_delete_dir_entry,
	.get_uniname_from_ext_entry = fat_get_uniname_from_ext_entry,
	.count_ext_entries = fat_count_ext_entries,
	.calc_num_entries = fat_calc_num_entries,
	.check_max_dentries = fat_check_max_dentries,

	.get_entry_type = fat_get_entry_type,
	.set_entry_type = fat_set_entry_type,
	.get_entry_attr = fat_get_entry_attr,
	.set_entry_attr = fat_set_entry_attr,
	.get_entry_flag = fat_get_entry_flag,
	.set_entry_flag = fat_set_entry_flag,
	.get_entry_clu0 = fat_get_entry_clu0,
	.set_entry_clu0 = fat_set_entry_clu0,
	.get_entry_size = fat_get_entry_size,
	.set_entry_size = fat_set_entry_size,
	.get_entry_time = fat_get_entry_time,
	.set_entry_time = fat_set_entry_time,
};

static FS_FUNC_T amap_fat_fs_func = {
	.alloc_cluster = amap_fat_alloc_cluster,
	.free_cluster = fat_free_cluster,
	.count_used_clusters = fat_count_used_clusters,

	.init_dir_entry = fat_init_dir_entry,
	.init_ext_entry = fat_init_ext_entry,
	.find_dir_entry = fat_find_dir_entry,
	.delete_dir_entry = fat_delete_dir_entry,
	.get_uniname_from_ext_entry = fat_get_uniname_from_ext_entry,
	.count_ext_entries = fat_count_ext_entries,
	.calc_num_entries = fat_calc_num_entries,
	.check_max_dentries = fat_check_max_dentries,

	.get_entry_type = fat_get_entry_type,
	.set_entry_type = fat_set_entry_type,
	.get_entry_attr = fat_get_entry_attr,
	.set_entry_attr = fat_set_entry_attr,
	.get_entry_flag = fat_get_entry_flag,
	.set_entry_flag = fat_set_entry_flag,
	.get_entry_clu0 = fat_get_entry_clu0,
	.set_entry_clu0 = fat_set_entry_clu0,
	.get_entry_size = fat_get_entry_size,
	.set_entry_size = fat_set_entry_size,
	.get_entry_time = fat_get_entry_time,
	.set_entry_time = fat_set_entry_time,

	.get_au_stat = amap_get_au_stat,
};

s32 mount_fat16(struct super_block *sb, pbr_t *p_pbr)
{
	s32 num_root_sectors;
	bpb16_t *p_bpb = &(p_pbr->bpb.f16);
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	if (!p_bpb->num_fats) {
		sdfat_msg(sb, KERN_ERR, "bogus number of FAT structure");
		return -EINVAL;
	}

	num_root_sectors = get_unaligned_le16(p_bpb->num_root_entries) << DENTRY_SIZE_BITS;
	num_root_sectors = ((num_root_sectors-1) >> sb->s_blocksize_bits) + 1;

	fsi->sect_per_clus = p_bpb->sect_per_clus;
	fsi->sect_per_clus_bits = ilog2(p_bpb->sect_per_clus);
	fsi->cluster_size_bits = fsi->sect_per_clus_bits + sb->s_blocksize_bits;
	fsi->cluster_size = 1 << fsi->cluster_size_bits;

	fsi->num_FAT_sectors = le16_to_cpu(p_bpb->num_fat_sectors);

	fsi->FAT1_start_sector = le16_to_cpu(p_bpb->num_reserved);
	if (p_bpb->num_fats == 1)
		fsi->FAT2_start_sector = fsi->FAT1_start_sector;
	else
		fsi->FAT2_start_sector = fsi->FAT1_start_sector + fsi->num_FAT_sectors;

	fsi->root_start_sector = fsi->FAT2_start_sector + fsi->num_FAT_sectors;
	fsi->data_start_sector = fsi->root_start_sector + num_root_sectors;

	fsi->num_sectors = get_unaligned_le16(p_bpb->num_sectors);
	if (!fsi->num_sectors)
		fsi->num_sectors = le32_to_cpu(p_bpb->num_huge_sectors);

	if (!fsi->num_sectors) {
		sdfat_msg(sb, KERN_ERR, "bogus number of total sector count");
		return -EINVAL;
	}

	fsi->num_clusters = (u32)((fsi->num_sectors - fsi->data_start_sector) >> fsi->sect_per_clus_bits) + CLUS_BASE;
	/* because the cluster index starts with 2 */

	fsi->vol_type = FAT16;
	if (fsi->num_clusters < FAT12_THRESHOLD)
		fsi->vol_type = FAT12;

	fsi->vol_id = get_unaligned_le32(p_bpb->vol_serial);

	fsi->root_dir = 0;
	fsi->dentries_in_root = get_unaligned_le16(p_bpb->num_root_entries);
	if (!fsi->dentries_in_root) {
		sdfat_msg(sb, KERN_ERR, "bogus number of max dentry count "
					"of the root directory");
		return -EINVAL;
	}

	fsi->dentries_per_clu = 1 << (fsi->cluster_size_bits - DENTRY_SIZE_BITS);

	fsi->vol_flag = VOL_CLEAN;
	fsi->clu_srch_ptr = 2;
	fsi->used_clusters = (u32) ~0;

	fsi->fs_func = &fat_fs_func;
	fat_ent_ops_init(sb);

	if (p_bpb->state & FAT_VOL_DIRTY) {
		fsi->vol_flag |= VOL_DIRTY;
		sdfat_log_msg(sb, KERN_WARNING, "Volume was not properly "
			"unmounted. Some data may be corrupt. "
			"Please run fsck.");
	}

	return 0;
} /* end of mount_fat16 */

static sector_t __calc_hidden_sect(struct super_block *sb)
{
	struct block_device *bdev = sb->s_bdev;
	sector_t hidden = 0;

	if (!bdev)
		goto out;

	hidden = bdev->bd_part->start_sect;
	/* a disk device, not a partition */
	if (!hidden) {
		if (bdev != bdev->bd_contains)
			sdfat_log_msg(sb, KERN_WARNING,
				"hidden(0), but disk has a partition table");
		goto out;
	}

	if (sb->s_blocksize_bits != 9) {
		ASSERT(sb->s_blocksize_bits > 9);
		hidden >>= (sb->s_blocksize_bits - 9);
	}

out:
	sdfat_log_msg(sb, KERN_INFO, "start_sect of part(%d)    : %lld",
		bdev ? bdev->bd_part->partno : -1, (s64)hidden);
	return hidden;

}

s32 mount_fat32(struct super_block *sb, pbr_t *p_pbr)
{
	pbr32_t *p_bpb = (pbr32_t *)p_pbr;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	if (!p_bpb->bpb.num_fats) {
		sdfat_msg(sb, KERN_ERR, "bogus number of FAT structure");
		return -EINVAL;
	}

	fsi->sect_per_clus = p_bpb->bpb.sect_per_clus;
	fsi->sect_per_clus_bits = ilog2(p_bpb->bpb.sect_per_clus);
	fsi->cluster_size_bits = fsi->sect_per_clus_bits + sb->s_blocksize_bits;
	fsi->cluster_size = 1 << fsi->cluster_size_bits;

	fsi->num_FAT_sectors = le32_to_cpu(p_bpb->bpb.num_fat32_sectors);

	fsi->FAT1_start_sector = le16_to_cpu(p_bpb->bpb.num_reserved);
	if (p_bpb->bpb.num_fats == 1)
		fsi->FAT2_start_sector = fsi->FAT1_start_sector;
	else
		fsi->FAT2_start_sector = fsi->FAT1_start_sector + fsi->num_FAT_sectors;

	fsi->root_start_sector = fsi->FAT2_start_sector + fsi->num_FAT_sectors;
	fsi->data_start_sector = fsi->root_start_sector;

	/* SPEC violation for compatibility */
	fsi->num_sectors = get_unaligned_le16(p_bpb->bpb.num_sectors);
	if (!fsi->num_sectors)
		fsi->num_sectors = le32_to_cpu(p_bpb->bpb.num_huge_sectors);

	/* 2nd check */
	if (!fsi->num_sectors) {
		sdfat_msg(sb, KERN_ERR, "bogus number of total sector count");
		return -EINVAL;
	}

	fsi->num_clusters = (u32)((fsi->num_sectors - fsi->data_start_sector) >> fsi->sect_per_clus_bits) + CLUS_BASE;
	/* because the cluster index starts with 2 */

	fsi->vol_type = FAT32;
	fsi->vol_id = get_unaligned_le32(p_bpb->bsx.vol_serial);

	fsi->root_dir = le32_to_cpu(p_bpb->bpb.root_cluster);
	fsi->dentries_in_root = 0;
	fsi->dentries_per_clu = 1 << (fsi->cluster_size_bits - DENTRY_SIZE_BITS);

	fsi->vol_flag = VOL_CLEAN;
	fsi->clu_srch_ptr = 2;
	fsi->used_clusters = (u32) ~0;

	fsi->fs_func = &fat_fs_func;

	/* Delayed / smart allocation related init */
	fsi->reserved_clusters = 0;

	/* Should be initialized before calling amap_create() */
	fat_ent_ops_init(sb);

	/* AU Map Creation */
	if (SDFAT_SB(sb)->options.improved_allocation & SDFAT_ALLOC_SMART) {
		u32 hidden_sectors = le32_to_cpu(p_bpb->bpb.num_hid_sectors);
		u32 calc_hid_sect = 0;
		int ret;


		/* calculate hidden sector size */
		calc_hid_sect = __calc_hidden_sect(sb);
		if (calc_hid_sect != hidden_sectors) {
			sdfat_log_msg(sb, KERN_WARNING, "abnormal hidden "
				"sector   : bpb(%u) != ondisk(%u)",
				hidden_sectors, calc_hid_sect);
			if (SDFAT_SB(sb)->options.adj_hidsect) {
				sdfat_log_msg(sb, KERN_INFO,
					"adjustment hidden sector : "
					"bpb(%u) -> ondisk(%u)",
					hidden_sectors, calc_hid_sect);
				hidden_sectors = calc_hid_sect;
			}
		}

		SDFAT_SB(sb)->options.amap_opt.misaligned_sect = hidden_sectors;

		/* calculate AU size if it's not set */
		if (!SDFAT_SB(sb)->options.amap_opt.sect_per_au) {
			SDFAT_SB(sb)->options.amap_opt.sect_per_au =
				__calc_default_au_size(sb);
		}

		ret = amap_create(sb,
				SDFAT_SB(sb)->options.amap_opt.pack_ratio,
				SDFAT_SB(sb)->options.amap_opt.sect_per_au,
				SDFAT_SB(sb)->options.amap_opt.misaligned_sect);
		if (ret) {
			sdfat_log_msg(sb, KERN_WARNING, "failed to create AMAP."
				" disabling smart allocation. (err:%d)", ret);
			SDFAT_SB(sb)->options.improved_allocation &= ~(SDFAT_ALLOC_SMART);
		} else {
			fsi->fs_func = &amap_fat_fs_func;
		}
	}

	/* Check dependency of mount options */
	if (SDFAT_SB(sb)->options.improved_allocation !=
				(SDFAT_ALLOC_DELAY | SDFAT_ALLOC_SMART)) {
		sdfat_log_msg(sb, KERN_INFO, "disabling defragmentation because"
					" smart, delay options are disabled");
		SDFAT_SB(sb)->options.defrag = 0;
	}

	if (p_bpb->bsx.state & FAT_VOL_DIRTY) {
		fsi->vol_flag |= VOL_DIRTY;
		sdfat_log_msg(sb, KERN_WARNING, "Volume was not properly "
			"unmounted. Some data may be corrupt. "
			"Please run fsck.");
	}

	return 0;
} /* end of mount_fat32 */

/* end of core_fat.c */
