/*
 * Copyright (C) 2018 MediaTek Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef UFS_MTK_IOCTL_H__
#define UFS_MTK_IOCTL_H__

#include <linux/types.h>

/*
 *  IOCTL opcode for ufs ffu has the following opcode after
 *  Following the last SCSI IOCTL opcode.
 */
#define UFS_IOCTL_QUERY         0x5388  /* Query descriptors, attr/flags */
#define UFS_IOCTL_FFU           0x5389  /* Do firmware upgrade */
#define UFS_IOCTL_GET_FW_VER    0x5390  /* Query production revision level */
#define UFS_IOCTL_RPMB          0x5391  /* For RPMB access */
#define HPB_QUERY_OPCODE        0x5500

#define UFS_IOCTL_FFU_MAX_FW_SIZE_BYTES             (512L * 1024)
#define UFS_IOCTL_FFU_MAX_FW_VER_BYTES              (4)
#define UFS_IOCTL_FFU_MAX_FW_VER_STRING_DESCR_BYTES	(10)

struct ufs_ioctl_query_data {
	/*
	 * User should select one of the opcode defined in "enum query_opcode".
	 * Please check include/uapi/scsi/ufs/ufs.h for the definition of it.
	 * Note that only UPIU_QUERY_OPCODE_READ_DESC,
	 * UPIU_QUERY_OPCODE_READ_ATTR & UPIU_QUERY_OPCODE_READ_FLAG are
	 * supported as of now. All other query_opcode would be considered
	 * invalid.
	 * As of now only read query operations are supported.
	 */
	__u32 opcode;
	/*
	 * User should select one of the idn from "enum flag_idn" or "enum
	 * attr_idn" or "enum desc_idn" based on whether opcode above is
	 * attribute, flag or descriptor.
	 * Please check include/uapi/scsi/ufs/ufs.h for the definition of it.
	 */
	__u8 idn;
	__u8 idx;
	/*
	 * User should specify the size of the buffer (buffer[0] below) where
	 * it wants to read the query data (attribute/flag/descriptor).
	 * As we might end up reading less data then what is specified in
	 * buf_size. So we are updating buf_size to what exactly we have read.
	 */
	__u16 buf_byte;
	/*
	 * placeholder for the start of the data buffer where kernel will copy
	 * the query data (attribute/flag/descriptor) read from the UFS device
	 * Note:
	 * For Read Attribute you will have to allocate 4 bytes
	 * For Read Flag you will have to allocate 1 byte
	 */
	__u8 *buf_ptr;
};

struct ufs_ioctl_query_data_hpb {
	/*
	 * User should select one of the opcode defined in "enum query_opcode".
	 * Please check include/uapi/scsi/ufs/ufs.h for the definition of it.
	 * Note that only UPIU_QUERY_OPCODE_READ_DESC,
	 * UPIU_QUERY_OPCODE_READ_ATTR & UPIU_QUERY_OPCODE_READ_FLAG are
	 * supported as of now. All other query_opcode would be considered
	 * invalid.
	 * As of now only read query operations are supported.
	 */
	__u32 opcode;
	/*
	 * User should select one of the idn from "enum flag_idn" or "enum
	 * attr_idn" or "enum desc_idn" based on whether opcode above is
	 * attribute, flag or descriptor.
	 * Please check include/uapi/scsi/ufs/ufs.h for the definition of it.
	 */
	__u8 idn;
	/*
	 * User should specify the size of the buffer (buffer[0] below) where
	 * it wants to read the query data (attribute/flag/descriptor).
	 * As we might end up reading less data then what is specified in
	 * buf_size. So we are updating buf_size to what exactly we have read.
	 */
	__u16 buf_size;
	/*
	 * placeholder for the start of the data buffer where kernel will copy
	 * the query data (attribute/flag/descriptor) read from the UFS device
	 * Note:
	 * For Read Attribute you will have to allocate 4 bytes
	 * For Read Flag you will have to allocate 1 byte
	 */
	__u8 buffer[0];
};

/**
 * struct ufs_ioctl_ffu - used to transfer data to and from user via ioctl
 * @buf_size: number of allocated bytes/data size on return
 * @buf_ptr: data location
 */
struct ufs_ioctl_ffu_data {
	/*
	 * User should specify the size of the buffer (buf_ptr below) where
	 * it wants to transfer firmware image.
	 *
	 * Note: use __u32 here because FFU data may exceed 64 KB
	 * (limit of __u16).
	 */
	__u32 buf_byte;

	/*
	 * placeholder for the start address of the data buffer where kernel
	 * will copy
	 * the data.
	 */
	__u8 *buf_ptr;
};

/**
 * struct ufs_ioctl_ffu - used to transfer data to and from user via ioctl
 * @buf_size: number of allocated bytes/data size on return
 * @buf_ptr: data location
 */
struct ufs_ioctl_query_fw_ver_data {
	/*
	 * User should specify the size of the buffer (buf_ptr below) where
	 * it wants to read firmware version data.
	 */
	__u16 buf_byte;

	/*
	 * placeholder for the start address of the data buffer where kernel
	 * will copy
	 * the data.
	 */
	__u8 *buf_ptr;
};

#endif /* UFS_MTK_IOCTL_H__ */

