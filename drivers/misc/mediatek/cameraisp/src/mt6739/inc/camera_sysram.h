/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef CAMERA_SYSRAM_H
#define CAMERA_SYSRAM_H
/* ----------------------------------------------------------------------------- */
#define SYSRAM_DEV_NAME     "camera-sysram"
#define SYSRAM_MAGIC_NO     'p'
/* ----------------------------------------------------------------------------- */
enum SYSRAM_USER_ENUM {
	SYSRAM_USER_VIDO,
	SYSRAM_USER_GDMA,
	SYSRAM_USER_SW_FD,
	SYSRAM_USER_AMOUNT,
	SYSRAM_USER_NONE
};
/*  */
struct SYSRAM_ALLOC_STRUCT {
	unsigned long Alignment;
	unsigned long Size;
	enum SYSRAM_USER_ENUM User;
	unsigned long Addr;	/* In/Out : address */
	unsigned long TimeoutMS;	/* In : millisecond */
};
/*  */
enum SYSRAM_CMD_ENUM {
	SYSRAM_CMD_ALLOC,
	SYSRAM_CMD_FREE,
	SYSRAM_CMD_DUMP
};
/* ----------------------------------------------------------------------------- */
#define SYSRAM_ALLOC    _IOWR(SYSRAM_MAGIC_NO,    SYSRAM_CMD_ALLOC,   struct SYSRAM_ALLOC_STRUCT)
#define SYSRAM_FREE     _IOWR(SYSRAM_MAGIC_NO,    SYSRAM_CMD_FREE,    enum SYSRAM_USER_ENUM)
#define SYSRAM_DUMP     _IO(SYSRAM_MAGIC_NO,    SYSRAM_CMD_DUMP)
/* ----------------------------------------------------------------------------- */
#endif
