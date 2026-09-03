// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Host Controller driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 Unisoc, Inc.
 *
 */

#ifndef __UFS_SPRD_IOCTL_H__
#define __UFS_SPRD_IOCTL_H__

/*
 *  IOCTL opcode for ufs ffu has the following opcode after
 *  Following the last SCSI IOCTL opcode.
 */
#define UFS_IOCTL_FFU                               0x5379
#define UFS_IOCTL_GET_DEVICE_INFO                   0x5392

#define UFS_IOCTL_FFU_MAX_FW_VER_BYTES              (4)
#define UFS_IOCTL_FFU_MAX_MODEL_BYTES               (16)
#define UFS_IOCTL_FFU_MAX_VENDOR_BYTES              (8)
#define UFS_IOCTL_FFU_MAX_FW_SIZE                   (0x200000) /* 2MB */

/* FFU */
#define WB_MODE_MASK (0x1F)
#define DOWNLOAD_MODE (0xE)
enum ufs_ffu_status {
	UFS_FFU_STATUS_NO_INFORMATION   = 0x0,
	UFS_FFU_STATUS_SUCCESSFUL_UPDATE = 0x1,
	UFS_FFU_STATUS_CORRUPTION_ERR   = 0x2,
	UFS_FFU_STATUS_INTERNAL_ERROR   = 0x3,
	UFS_FFU_STATUS_VERSION_MISMATCH = 0x4,
	UFS_FFU_STATUS_GENERAL_ERROR    = 0xFF,
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
	__u32 chunk_byte;

	/*
	 * placeholder for the start address of the data buffer where kernel
	 * will copy
	 * the data.
	 */
	__u64 buf_ptr;
};

/**
 * struct ufs_ioctl_ffu - used to transfer data to and from user via ioctl
 * @buf_size: number of allocated bytes/data size on return
 * @buf_ptr: data location
 */
struct ufs_ioctl_query_device_info {
	/*
	 * placeholder for the start address of the data buffer where kernel
	 * will copy
	 * the data.
	 */
	__u8 vendor[UFS_IOCTL_FFU_MAX_VENDOR_BYTES];
	__u8 model[UFS_IOCTL_FFU_MAX_MODEL_BYTES];
	__u8 fw_rev[UFS_IOCTL_FFU_MAX_FW_VER_BYTES];
	__u16 manid;
	__u32 max_hw_sectors_size;
};

int ufshcd_sprd_ioctl(struct scsi_device *dev,
		      unsigned int cmd, void __user *buffer);
void prepare_command_send_in_ffu_state(struct ufs_hba *hba,
				       struct ufshcd_lrb *lrbp, int *err);
#endif
