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
/*  FILE    : fatent.c                                                  */
/*  PURPOSE : sdFAT FAT entry manager                                   */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*  NOTES                                                               */
/*                                                                      */
/*                                                                      */
/************************************************************************/

#include <asm/unaligned.h>

#include "sdfat.h"
#include "core.h"

/*----------------------------------------------------------------------*/
/*  Global Variable Definitions                                         */
/*----------------------------------------------------------------------*/
/* All buffer structures are protected w/ fsi->v_sem */

/*----------------------------------------------------------------------*/
/*  Static functions                                                    */
/*----------------------------------------------------------------------*/

/*======================================================================*/
/*  FAT Read/Write Functions                                            */
/*======================================================================*/
/* in : sb, loc
 * out: content
 * returns 0 on success, -1 on error
 */
static s32 exfat_ent_get(struct super_block *sb, u32 loc, u32 *content)
{
	u32 off, _content;
	u64 sec;
	u8 *fat_sector;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	/* fsi->vol_type == EXFAT */
	sec = fsi->FAT1_start_sector + (loc >> (sb->s_blocksize_bits-2));
	off = (loc << 2) & (u32)(sb->s_blocksize - 1);

	fat_sector = fcache_getblk(sb, sec);
	if (!fat_sector)
		return -EIO;

	_content = le32_to_cpu(*(__le32 *)(&fat_sector[off]));

	/* remap reserved clusters to simplify code */
	if (_content >= CLUSTER_32(0xFFFFFFF8))
		_content = CLUS_EOF;

	*content = CLUSTER_32(_content);
	return 0;
}

static s32 exfat_ent_set(struct super_block *sb, u32 loc, u32 content)
{
	u32 off;
	u64 sec;
	u8 *fat_sector;
	__le32 *fat_entry;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	sec = fsi->FAT1_start_sector + (loc >> (sb->s_blocksize_bits-2));
	off = (loc << 2) & (u32)(sb->s_blocksize - 1);

	fat_sector = fcache_getblk(sb, sec);
	if (!fat_sector)
		return -EIO;

	fat_entry = (__le32 *)&(fat_sector[off]);
	*fat_entry = cpu_to_le32(content);

	return fcache_modify(sb, sec);
}

#define FATENT_FAT32_VALID_MASK		(0x0FFFFFFFU)
#define FATENT_FAT32_IGNORE_MASK	(0xF0000000U)
static s32 fat32_ent_get(struct super_block *sb, u32 loc, u32 *content)
{
	u32 off, _content;
	u64 sec;
	u8 *fat_sector;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	sec = fsi->FAT1_start_sector + (loc >> (sb->s_blocksize_bits-2));
	off = (loc << 2) & (u32)(sb->s_blocksize - 1);

	fat_sector = fcache_getblk(sb, sec);
	if (!fat_sector)
		return -EIO;

	_content = le32_to_cpu(*(__le32 *)(&fat_sector[off]));
	_content &= FATENT_FAT32_VALID_MASK;

	/* remap reserved clusters to simplify code */
	if (_content == CLUSTER_32(0x0FFFFFF7U))
		_content = CLUS_BAD;
	else if (_content >= CLUSTER_32(0x0FFFFFF8U))
		_content = CLUS_EOF;

	*content = CLUSTER_32(_content);
	return 0;
}

static s32 fat32_ent_set(struct super_block *sb, u32 loc, u32 content)
{
	u32 off;
	u64 sec;
	u8 *fat_sector;
	__le32 *fat_entry;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	content &= FATENT_FAT32_VALID_MASK;

	sec = fsi->FAT1_start_sector + (loc >> (sb->s_blocksize_bits-2));
	off = (loc << 2) & (u32)(sb->s_blocksize - 1);

	fat_sector = fcache_getblk(sb, sec);
	if (!fat_sector)
		return -EIO;

	fat_entry = (__le32 *)&(fat_sector[off]);
	content |= (le32_to_cpu(*fat_entry) & FATENT_FAT32_IGNORE_MASK);
	*fat_entry = cpu_to_le32(content);

	return fcache_modify(sb, sec);
}

#define FATENT_FAT16_VALID_MASK		(0x0000FFFFU)
static s32 fat16_ent_get(struct super_block *sb, u32 loc, u32 *content)
{
	u32 off, _content;
	u64 sec;
	u8 *fat_sector;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	sec = fsi->FAT1_start_sector + (loc >> (sb->s_blocksize_bits-1));
	off = (loc << 1) & (u32)(sb->s_blocksize - 1);

	fat_sector = fcache_getblk(sb, sec);
	if (!fat_sector)
		return -EIO;

	_content = (u32)le16_to_cpu(*(__le16 *)(&fat_sector[off]));
	_content &= FATENT_FAT16_VALID_MASK;

	/* remap reserved clusters to simplify code */
	if (_content == CLUSTER_16(0xFFF7U))
		_content = CLUS_BAD;
	else if (_content >= CLUSTER_16(0xFFF8U))
		_content = CLUS_EOF;

	*content = CLUSTER_32(_content);
	return 0;
}

static s32 fat16_ent_set(struct super_block *sb, u32 loc, u32 content)
{
	u32 off;
	u64 sec;
	u8 *fat_sector;
	__le16 *fat_entry;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	content &= FATENT_FAT16_VALID_MASK;

	sec = fsi->FAT1_start_sector + (loc >> (sb->s_blocksize_bits-1));
	off = (loc << 1) & (u32)(sb->s_blocksize - 1);

	fat_sector = fcache_getblk(sb, sec);
	if (!fat_sector)
		return -EIO;

	fat_entry = (__le16 *)&(fat_sector[off]);
	*fat_entry = cpu_to_le16(content);

	return fcache_modify(sb, sec);
}

#define FATENT_FAT12_VALID_MASK		(0x00000FFFU)
static s32 fat12_ent_get(struct super_block *sb, u32 loc, u32 *content)
{
	u32 off, _content;
	u64 sec;
	u8 *fat_sector;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	sec = fsi->FAT1_start_sector + ((loc + (loc >> 1)) >> sb->s_blocksize_bits);
	off = (loc + (loc >> 1)) & (u32)(sb->s_blocksize - 1);

	fat_sector = fcache_getblk(sb, sec);
	if (!fat_sector)
		return -EIO;

	if (off == (u32)(sb->s_blocksize - 1)) {
		_content  = (u32) fat_sector[off];

		fat_sector = fcache_getblk(sb, ++sec);
		if (!fat_sector)
			return -EIO;

		_content |= (u32) fat_sector[0] << 8;
	} else {
		_content = get_unaligned_le16(&fat_sector[off]);
	}

	if (loc & 1)
		_content >>= 4;

	_content &= FATENT_FAT12_VALID_MASK;

	/* remap reserved clusters to simplify code */
	if (_content == CLUSTER_16(0x0FF7U))
		_content = CLUS_BAD;
	else if (_content >= CLUSTER_16(0x0FF8U))
		_content = CLUS_EOF;

	*content = CLUSTER_32(_content);
	return 0;
}

static s32 fat12_ent_set(struct super_block *sb, u32 loc, u32 content)
{
	u32 off;
	u64 sec;
	u8 *fat_sector, *fat_entry;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	content &= FATENT_FAT12_VALID_MASK;

	sec = fsi->FAT1_start_sector + ((loc + (loc >> 1)) >> sb->s_blocksize_bits);
	off = (loc + (loc >> 1)) & (u32)(sb->s_blocksize - 1);

	fat_sector = fcache_getblk(sb, sec);
	if (!fat_sector)
		return -EIO;

	if (loc & 1) { /* odd */

		content <<= 4;

		if (off == (u32)(sb->s_blocksize-1)) {
			fat_sector[off] = (u8)(content | (fat_sector[off] & 0x0F));
			if (fcache_modify(sb, sec))
				return -EIO;

			fat_sector = fcache_getblk(sb, ++sec);
			if (!fat_sector)
				return -EIO;

			fat_sector[0] = (u8)(content >> 8);
		} else {
			fat_entry = &(fat_sector[off]);
			content |= 0x000F & get_unaligned_le16(fat_entry);
			put_unaligned_le16(content, fat_entry);
		}
	} else { /* even */
		fat_sector[off] = (u8)(content);

		if (off == (u32)(sb->s_blocksize-1)) {
			fat_sector[off] = (u8)(content);
			if (fcache_modify(sb, sec))
				return -EIO;

			fat_sector = fcache_getblk(sb, ++sec);
			if (!fat_sector)
				return -EIO;

			fat_sector[0] = (u8)((fat_sector[0] & 0xF0) | (content >> 8));
		} else {
			fat_entry = &(fat_sector[off]);
			content |= 0xF000 & get_unaligned_le16(fat_entry);
			put_unaligned_le16(content, fat_entry);
		}
	}
	return fcache_modify(sb, sec);
}


static FATENT_OPS_T fat12_ent_ops = {
	fat12_ent_get,
	fat12_ent_set
};

static FATENT_OPS_T fat16_ent_ops = {
	fat16_ent_get,
	fat16_ent_set
};

static FATENT_OPS_T fat32_ent_ops = {
	fat32_ent_get,
	fat32_ent_set
};

static FATENT_OPS_T exfat_ent_ops = {
	exfat_ent_get,
	exfat_ent_set
};

s32 fat_ent_ops_init(struct super_block *sb)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	switch (fsi->vol_type) {
	case EXFAT:
		fsi->fatent_ops = &exfat_ent_ops;
		break;
	case FAT32:
		fsi->fatent_ops = &fat32_ent_ops;
		break;
	case FAT16:
		fsi->fatent_ops = &fat16_ent_ops;
		break;
	case FAT12:
		fsi->fatent_ops = &fat12_ent_ops;
		break;
	default:
		fsi->fatent_ops = NULL;
		EMSG("Unknown volume type : %d", (int)fsi->vol_type);
		return -ENOTSUPP;
	}

	return 0;
}

static inline bool is_reserved_clus(u32 clus)
{
	if (IS_CLUS_FREE(clus))
		return true;
	if (IS_CLUS_EOF(clus))
		return true;
	if (IS_CLUS_BAD(clus))
		return true;
	return false;
}

static inline bool is_valid_clus(FS_INFO_T *fsi, u32 clus)
{
	if (clus < CLUS_BASE || fsi->num_clusters <= clus)
		return false;
	return true;
}

s32 fat_ent_get(struct super_block *sb, u32 loc, u32 *content)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	s32 err;

	if (!is_valid_clus(fsi, loc)) {
		sdfat_fs_error(sb, "invalid access to FAT (entry 0x%08x)", loc);
		return -EIO;
	}

	err = fsi->fatent_ops->ent_get(sb, loc, content);
	if (err) {
		sdfat_fs_error(sb, "failed to access to FAT "
				"(entry 0x%08x, err:%d)", loc, err);
		return err;
	}

	if (!is_reserved_clus(*content) && !is_valid_clus(fsi, *content)) {
		sdfat_fs_error(sb, "invalid access to FAT (entry 0x%08x) "
			"bogus content (0x%08x)", loc, *content);
		return -EIO;
	}

	return 0;
}

s32 fat_ent_set(struct super_block *sb, u32 loc, u32 content)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	return fsi->fatent_ops->ent_set(sb, loc, content);
}

s32 fat_ent_get_safe(struct super_block *sb, u32 loc, u32 *content)
{
	s32 err = fat_ent_get(sb, loc, content);

	if (err)
		return err;

	if (IS_CLUS_FREE(*content)) {
		sdfat_fs_error(sb, "invalid access to FAT free cluster "
				"(entry 0x%08x)", loc);
		return -EIO;
	}

	if (IS_CLUS_BAD(*content)) {
		sdfat_fs_error(sb, "invalid access to FAT bad cluster "
				"(entry 0x%08x)", loc);
		return -EIO;
	}

	return 0;
}

/* end of fatent.c */
