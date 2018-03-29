/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/ioctl.h>

#ifndef __ALMK_DRV_H__
#define __ALMK_DRV_H__


typedef struct {
	unsigned int pid;
	unsigned int *maxSafeSize;
	/* unsigned int *result; */

} ALMK_DRV_DATA;

	extern int get_min_free_pages(pid_t pid);
	extern int get_min_free_pages(pid_t pid);
	extern int query_lmk_minfree(int index);

#define ALMK_IOCTL_MAGIC        'x'

/* #define JPEG_DEC_IOCTL_INIT     _IO  (ALMK_IOCTL_MAGIC, 1) */
/* #define JPEG_DEC_IOCTL_CONFIG   _IOW (ALMK_IOCTL_MAGIC, 2, JPEG_DEC_DRV_IN) */
/* #define JPEG_DEC_IOCTL_START    _IO  (ALMK_IOCTL_MAGIC, 3) */
/* #define JPEG_DEC_IOCTL_WAIT     _IOWR(ALMK_IOCTL_MAGIC, 6, JPEG_DEC_DRV_OUT) */
/* #define JPEG_DEC_IOCTL_DEINIT   _IO  (ALMK_IOCTL_MAGIC, 8) */

#define ALMK_IOCTL_CMD_INIT     _IO(ALMK_IOCTL_MAGIC, 11)
#define ALMK_IOCTL_CMD_GET_MAX_SIZE     _IOWR(ALMK_IOCTL_MAGIC, 12, ALMK_DRV_DATA)
#define ALMK_IOCTL_CMD_DEINIT   _IO(ALMK_IOCTL_MAGIC, 13)

#endif
