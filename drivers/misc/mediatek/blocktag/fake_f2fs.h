/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

// to avoid to inlcude more unnecessary module in,
// copy the follow lines form fs/f2fs.h.
// Those defines is used in trace/event/f2fs.h
#ifndef _FAKE_F2FS_H
#define _FAKE_F2FS_H
typedef u32 block_t;
typedef u32 nid_t;

#define	CP_UMOUNT   0x00000001
#define	CP_FASTBOOT 0x00000002
#define	CP_SYNC     0x00000004
#define	CP_RECOVERY 0x00000008
#define	CP_DISCARD  0x00000010
#define CP_TRIMMED  0x00000020
#define CP_PAUSE    0x00000040
#define CP_RESIZE   0x00000080
#endif //_FAKE_F2FS_H
