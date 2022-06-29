/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef HBT_H_
#define HBT_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define HBT_IOCTL_BASE 't'

#define HBT_ABI_MAJOR 1
#define HBT_ABI_MINOR 1

struct hbt_abi_version {
	__u16 major;
	__u16 minor;
};
#define HBT_GET_VERSION                                                    \
	_IOR(HBT_IOCTL_BASE, 0xa0, struct hbt_abi_version)

struct hbt_mm {
	__u64 start_code;
	__u64 end_code;
	__u64 start_data;
	__u64 end_data;
	__u64 start_brk;
	__u64 brk;
	__u64 start_stack;
	__u64 arg_start;
	__u64 arg_end;
	__u64 env_start;
	__u64 env_end;
	__u64 auxv;
	__u64 auxv_size;
};
#define HBT_SET_MM _IOW(HBT_IOCTL_BASE, 0xa1, struct hbt_mm)

#define HBT_SET_MMAP_BASE _IO(HBT_IOCTL_BASE, 0xa2)

struct hbt_compat_ioctl {
	__u32 fd;
	__u32 cmd;
	__u32 arg;
};
#define HBT_COMPAT_IOCTL                                                   \
	_IOW(HBT_IOCTL_BASE, 0xa3, struct hbt_compat_ioctl)

struct hbt_compat_robust_list {
	__u32 head;
	__u32 len;
};
#define HBT_COMPAT_SET_ROBUST_LIST                                         \
	_IOW(HBT_IOCTL_BASE, 0xa4, struct hbt_compat_robust_list)

#define HBT_COMPAT_GET_ROBUST_LIST                                         \
	_IOR(HBT_IOCTL_BASE, 0xa5, struct hbt_compat_robust_list)

struct hbt_compat_getdents64 {
	__u32 fd;
	__u64 dirp;
	__u32 count;
};
#define HBT_COMPAT_GETDENTS64                                              \
	_IOR(HBT_IOCTL_BASE, 0xa6, struct hbt_compat_getdents64)

struct hbt_compat_lseek {
	__u32 fd;
	__u64 offset;
	__u32 whence;
};
#define HBT_COMPAT_LSEEK                                              \
	_IOR(HBT_IOCTL_BASE, 0xa7, struct hbt_compat_lseek)

#endif /* HBT_H_ */
