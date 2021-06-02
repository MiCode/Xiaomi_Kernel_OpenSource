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

#ifndef _SDFAT_FS_H
#define _SDFAT_FS_H

#include <linux/types.h>
#include <linux/magic.h>
#include <asm/byteorder.h>

/*----------------------------------------------------------------------*/
/*  Constant & Macro Definitions                                        */
/*----------------------------------------------------------------------*/
#ifndef MSDOS_SUPER_MAGIC
#define MSDOS_SUPER_MAGIC       0x4d44          /* MD */
#endif

#ifndef EXFAT_SUPER_MAGIC
#define EXFAT_SUPER_MAGIC       (0x2011BAB0UL)
#endif /* EXFAT_SUPER_MAGIC */

#define SDFAT_SUPER_MAGIC       (0x5EC5DFA4UL)
#define SDFAT_ROOT_INO          1

/* FAT types */
#define FAT12                   0x01    // FAT12
#define FAT16                   0x0E    // Win95 FAT16 (LBA)
#define FAT32                   0x0C    // Win95 FAT32 (LBA)
#define EXFAT                   0x07    // exFAT

/* directory file name */
#define DOS_CUR_DIR_NAME        ".          "
#define DOS_PAR_DIR_NAME        "..         "

#ifdef __LITTLE_ENDIAN
#define UNI_CUR_DIR_NAME        ".\0"
#define UNI_PAR_DIR_NAME        ".\0.\0"
#else
#define UNI_CUR_DIR_NAME        "\0."
#define UNI_PAR_DIR_NAME        "\0.\0."
#endif

/* file name lengths */
/* NOTE :
 * The maximum length of input or output is limited to 256 including NULL,
 * But we allocate 4 extra bytes for utf8 translation reside in last position,
 * because utf8 can uses memory upto 6 bytes per one character.
 * Therefore, MAX_CHARSET_SIZE supports upto 6 bytes for utf8
 */
#define MAX_UNINAME_BUF_SIZE       (((MAX_NAME_LENGTH+1)*2)+4)
#define MAX_DOSNAME_BUF_SIZE       ((DOS_NAME_LENGTH+2)+6)
#define MAX_VFSNAME_BUF_SIZE       ((MAX_NAME_LENGTH+1)*MAX_CHARSET_SIZE)
#define MAX_CHARSET_SIZE        6       // max size of multi-byte character
#define MAX_NAME_LENGTH         255     // max len of file name excluding NULL
#define DOS_NAME_LENGTH         11      // DOS file name length excluding NULL

#define SECTOR_SIZE_BITS	9	/* VFS sector size is 512 bytes */

#define DENTRY_SIZE		32	/* directory entry size */
#define DENTRY_SIZE_BITS	5

#define MAX_FAT_DENTRIES	65536   /* FAT allows 65536 directory entries */
#define MAX_EXFAT_DENTRIES	8388608 /* exFAT allows 8388608(256MB) directory entries */

/* PBR entries */
#define PBR_SIGNATURE	0xAA55
#define EXT_SIGNATURE	0xAA550000
#define VOL_LABEL	"NO NAME    " /* size should be 11 */
#define OEM_NAME	"MSWIN4.1"  /* size should be 8 */
#define STR_FAT12	"FAT12   "  /* size should be 8 */
#define STR_FAT16	"FAT16   "  /* size should be 8 */
#define STR_FAT32	"FAT32   "  /* size should be 8 */
#define STR_EXFAT	"EXFAT   "  /* size should be 8 */

#define VOL_CLEAN	0x0000
#define VOL_DIRTY	0x0002

#define FAT_VOL_DIRTY	0x01

/* max number of clusters */
#define FAT12_THRESHOLD         4087        // 2^12 - 1 + 2 (clu 0 & 1)
#define FAT16_THRESHOLD         65527       // 2^16 - 1 + 2
#define FAT32_THRESHOLD         268435457   // 2^28 - 1 + 2
#define EXFAT_THRESHOLD         268435457   // 2^28 - 1 + 2

/* dentry types */
#define MSDOS_DELETED		0xE5	/* deleted mark */
#define MSDOS_UNUSED		0x00	/* end of directory */

#define EXFAT_UNUSED		0x00	/* end of directory */
#define IS_EXFAT_DELETED(x)	((x) < 0x80) /* deleted file (0x01~0x7F) */
#define EXFAT_INVAL		0x80	/* invalid value */
#define EXFAT_BITMAP		0x81	/* allocation bitmap */
#define EXFAT_UPCASE		0x82	/* upcase table */
#define EXFAT_VOLUME		0x83	/* volume label */
#define EXFAT_FILE		0x85	/* file or dir */
#define EXFAT_STREAM		0xC0	/* stream entry */
#define EXFAT_NAME		0xC1	/* file name entry */
#define EXFAT_ACL		0xC2	/* stream entry */

/* specific flag */
#define MSDOS_LAST_LFN		0x40

/* file attributes */
#define ATTR_NORMAL             0x0000
#define ATTR_READONLY           0x0001
#define ATTR_HIDDEN             0x0002
#define ATTR_SYSTEM             0x0004
#define ATTR_VOLUME             0x0008
#define ATTR_SUBDIR             0x0010
#define ATTR_ARCHIVE            0x0020
#define ATTR_SYMLINK            0x0040
#define ATTR_EXTEND             (ATTR_READONLY | ATTR_HIDDEN | ATTR_SYSTEM | \
				 ATTR_VOLUME) /* 0x000F */

#define ATTR_EXTEND_MASK        (ATTR_EXTEND | ATTR_SUBDIR | ATTR_ARCHIVE)
#define ATTR_RWMASK             (ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME | \
				 ATTR_SUBDIR | ATTR_ARCHIVE | ATTR_SYMLINK)/* 0x007E */

/* file creation modes */
#define FM_REGULAR              0x00
#define FM_SYMLINK              0x40

/* time modes */
#define TM_CREATE               0
#define TM_MODIFY               1
#define TM_ACCESS               2

/* checksum types */
#define CS_DIR_ENTRY            0
#define CS_PBR_SECTOR           1
#define CS_DEFAULT              2

/*
 * ioctl command
 */
#define SDFAT_IOCTL_GET_VOLUME_ID	_IOR('r', 0x12, __u32)
#define SDFAT_IOCTL_DFR_INFO		_IOC(_IOC_NONE, 'E', 0x13, sizeof(u32))
#define SDFAT_IOCTL_DFR_TRAV		_IOC(_IOC_NONE, 'E', 0x14, sizeof(u32))
#define SDFAT_IOCTL_DFR_REQ		_IOC(_IOC_NONE, 'E', 0x15, sizeof(u32))
#define SDFAT_IOCTL_DFR_SPO_FLAG	_IOC(_IOC_NONE, 'E', 0x16, sizeof(u32))
#define SDFAT_IOCTL_PANIC               _IOC(_IOC_NONE, 'E', 0x17, sizeof(u32))

/*
 * ioctl command for debugging
 */

/*
 * IOCTL code 'f' used by
 *   - file systems typically #0~0x1F
 *   - embedded terminal devices #128~
 *   - exts for debugging purpose #99
 * number 100 and 101 is available now but has possible conflicts
 *
 * NOTE : This is available only If CONFIG_SDFAT_DVBG_IOCTL is enabled.
 *
 */
#define SDFAT_IOC_GET_DEBUGFLAGS       _IOR('f', 100, long)
#define SDFAT_IOC_SET_DEBUGFLAGS       _IOW('f', 101, long)

#define SDFAT_DEBUGFLAGS_INVALID_UMOUNT        0x01
#define SDFAT_DEBUGFLAGS_ERROR_RW              0x02

/*----------------------------------------------------------------------*/
/*  On-Disk Type Definitions                                            */
/*----------------------------------------------------------------------*/

/* FAT12/16 BIOS parameter block (64 bytes) */
typedef struct {
	__u8	jmp_boot[3];
	__u8	oem_name[8];

	__u8	sect_size[2];		/* unaligned */
	__u8	sect_per_clus;
	__le16	num_reserved;		/* . */
	__u8	num_fats;
	__u8	num_root_entries[2];	/* unaligned */
	__u8	num_sectors[2];		/* unaligned */
	__u8	media_type;
	__le16  num_fat_sectors;
	__le16  sectors_in_track;
	__le16  num_heads;
	__le32	num_hid_sectors;	/* . */
	__le32	num_huge_sectors;

	__u8	phy_drv_no;
	__u8	state;			/* used by WindowsNT for mount state */
	__u8	ext_signature;
	__u8	vol_serial[4];
	__u8	vol_label[11];
	__u8	vol_type[8];
	__le16	dummy;
} bpb16_t;

/* FAT32 BIOS parameter block (64 bytes) */
typedef struct {
	__u8	jmp_boot[3];
	__u8	oem_name[8];

	__u8	sect_size[2];		/* unaligned */
	__u8	sect_per_clus;
	__le16	num_reserved;
	__u8	num_fats;
	__u8	num_root_entries[2];	/* unaligned */
	__u8	num_sectors[2];		/* unaligned */
	__u8	media_type;
	__le16  num_fat_sectors;	/* zero */
	__le16  sectors_in_track;
	__le16  num_heads;
	__le32	num_hid_sectors;	/* . */
	__le32	num_huge_sectors;

	__le32	num_fat32_sectors;
	__le16	ext_flags;
	__u8	fs_version[2];
	__le32	root_cluster;		/* . */
	__le16	fsinfo_sector;
	__le16	backup_sector;
	__le16	reserved[6];		/* . */
} bpb32_t;

/* FAT32 EXTEND BIOS parameter block (32 bytes) */
typedef struct {
	__u8	phy_drv_no;
	__u8	state;			/* used by WindowsNT for mount state */
	__u8	ext_signature;
	__u8	vol_serial[4];
	__u8	vol_label[11];
	__u8	vol_type[8];
	__le16  dummy[3];
} bsx32_t;

/* EXFAT BIOS parameter block (64 bytes) */
typedef struct {
	__u8	jmp_boot[3];
	__u8	oem_name[8];
	__u8	res_zero[53];
} bpb64_t;

/* EXFAT EXTEND BIOS parameter block (56 bytes) */
typedef struct {
	__le64	vol_offset;
	__le64	vol_length;
	__le32	fat_offset;
	__le32	fat_length;
	__le32	clu_offset;
	__le32	clu_count;
	__le32	root_cluster;
	__le32	vol_serial;
	__u8	fs_version[2];
	__le16	vol_flags;
	__u8	sect_size_bits;
	__u8	sect_per_clus_bits;
	__u8	num_fats;
	__u8	phy_drv_no;
	__u8	perc_in_use;
	__u8	reserved2[7];
} bsx64_t;

/* FAT32 PBR (64 bytes) */
typedef struct {
	bpb16_t bpb;
} pbr16_t;

/* FAT32 PBR[BPB+BSX] (96 bytes) */
typedef struct {
	bpb32_t bpb;
	bsx32_t bsx;
} pbr32_t;

/* EXFAT PBR[BPB+BSX] (120 bytes) */
typedef struct {
	bpb64_t bpb;
	bsx64_t bsx;
} pbr64_t;

/* Common PBR[Partition Boot Record] (512 bytes) */
typedef struct {
	union {
		__u8	raw[64];
		bpb16_t f16;
		bpb32_t f32;
		bpb64_t f64;
	} bpb;
	union {
		__u8	raw[56];
		bsx32_t f32;
		bsx64_t f64;
	} bsx;
	__u8	boot_code[390];
	__le16	signature;
} pbr_t;

/* FAT32 filesystem information sector (512 bytes) */
typedef struct {
	__le32	signature1;              // aligned
	__u8	reserved1[480];
	__le32	signature2;              // aligned
	__le32	free_cluster;            // aligned
	__le32	next_cluster;            // aligned
	__u8    reserved2[14];
	__le16	signature3[2];
} fat32_fsi_t;

/* FAT directory entry (32 bytes) */
typedef struct {
	__u8       dummy[32];
} DENTRY_T;

typedef struct {
	__u8	name[DOS_NAME_LENGTH];	/* 11 chars */
	__u8	attr;
	__u8	lcase;
	__u8	create_time_ms;
	__le16	create_time;             // aligned
	__le16	create_date;             // aligned
	__le16	access_date;             // aligned
	__le16	start_clu_hi;            // aligned
	__le16	modify_time;             // aligned
	__le16	modify_date;             // aligned
	__le16	start_clu_lo;            // aligned
	__le32	size;                    // aligned
} DOS_DENTRY_T;

/* FAT extended directory entry (32 bytes) */
typedef struct {
	__u8	order;
	__u8	unicode_0_4[10];
	__u8	attr;
	__u8	sysid;
	__u8	checksum;
	__le16	unicode_5_10[6];	// aligned
	__le16	start_clu;		// aligned
	__le16	unicode_11_12[2];	// aligned
} EXT_DENTRY_T;

/* EXFAT file directory entry (32 bytes) */
typedef struct {
	__u8	type;
	__u8	num_ext;
	__le16	checksum;		// aligned
	__le16	attr;			// aligned
	__le16	reserved1;
	__le16	create_time;		// aligned
	__le16	create_date;		// aligned
	__le16	modify_time;		// aligned
	__le16	modify_date;		// aligned
	__le16	access_time;		// aligned
	__le16	access_date;		// aligned
	__u8	create_time_ms;
	__u8	modify_time_ms;
	__u8	access_time_ms;
	__u8	reserved2[9];
} FILE_DENTRY_T;

/* EXFAT stream extension directory entry (32 bytes) */
typedef struct {
	__u8	type;
	__u8	flags;
	__u8	reserved1;
	__u8	name_len;
	__le16	name_hash;		// aligned
	__le16	reserved2;
	__le64	valid_size;		// aligned
	__le32	reserved3;		// aligned
	__le32	start_clu;		// aligned
	__le64	size;			// aligned
} STRM_DENTRY_T;

/* EXFAT file name directory entry (32 bytes) */
typedef struct {
	__u8	type;
	__u8	flags;
	__le16	unicode_0_14[15];	// aligned
} NAME_DENTRY_T;

/* EXFAT allocation bitmap directory entry (32 bytes) */
typedef struct {
	__u8	type;
	__u8	flags;
	__u8	reserved[18];
	__le32  start_clu;		// aligned
	__le64	size;			// aligned
} BMAP_DENTRY_T;

/* EXFAT up-case table directory entry (32 bytes) */
typedef struct {
	__u8	type;
	__u8	reserved1[3];
	__le32	checksum;		// aligned
	__u8	reserved2[12];
	__le32	start_clu;		// aligned
	__le64	size;			// aligned
} CASE_DENTRY_T;

/* EXFAT volume label directory entry (32 bytes) */
typedef struct {
	__u8	type;
	__u8	label_len;
	__le16	unicode_0_10[11];	// aligned
	__u8	reserved[8];
} VOLM_DENTRY_T;

#endif /* _SDFAT_FS_H */
