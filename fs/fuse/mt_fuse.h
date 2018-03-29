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

#ifndef _FS_MT_FUSE_H
#define _FS_MT_FUSE_H

#include <linux/fuse.h>
#include <linux/fs.h>

void fuse_request_send_background_ex(struct fuse_conn *fc,
	struct fuse_req *req, __u32 size);
void fuse_request_send_ex(struct fuse_conn *fc, struct fuse_req *req,
	__u32 size);

#if defined(CONFIG_FUSE_IO_LOG)

#include <linux/sched.h>
#include <linux/kthread.h>

/* max number of access traces in one log */
#define FUSE_IOLOG_MAX     12
/* kernel log print buffer size */
#define FUSE_IOLOG_BUFLEN  512
/* latency time between logs */
#define FUSE_IOLOG_LATENCY 1
/* max number of logs in the ring buffer */
#define FUSE_IOLOG_RINGBUF_MAX 120

void fuse_time_diff(struct timespec *start,
	struct timespec *end,
	struct timespec *diff);

void fuse_iolog_add(__u32 io_bytes, int type,
	struct timespec *start,
	struct timespec *end);

__u32 fuse_iolog_timeus_diff(struct timespec *start,
	struct timespec *end);

void fuse_iolog_exit(void);
void fuse_iolog_init(void);

/* access trace statistic */
struct fuse_rw_info {
	__u32  count;
	__u32  bytes;
	__u32  us;
};

/* access trace */
struct fuse_proc_info {
	pid_t pid;
	__u32 valid;
	int misc_type;
	struct fuse_rw_info read;
	struct fuse_rw_info write;
	struct fuse_rw_info misc;
};

/* data container for timing macros */
struct fuse_iolog_t {
	struct timespec _tstart, _tend;
	__u32 size;
	unsigned int opcode;
};

/* log entry of ring buffer */
struct fuse_proc_log {
	uint64_t time;
	struct fuse_proc_info info[FUSE_IOLOG_MAX];
};

#define FUSE_IOLOG_INIT(iobytes, type)   struct fuse_iolog_t _iolog = { .size = iobytes, .opcode = type }
#define FUSE_IOLOG_START()  get_monotonic_boottime(&_iolog._tstart)
#define FUSE_IOLOG_END()    get_monotonic_boottime(&_iolog._tend)
#define FUSE_IOLOG_US()     fuse_iolog_timeus_diff(&_iolog._tstart, &_iolog._tend)
#define FUSE_IOLOG_PRINT()  fuse_iolog_add(_iolog.size, _iolog.opcode, &_iolog._tstart, &_iolog._tend)

#else

#define FUSE_IOLOG_INIT(...)
#define FUSE_IOLOG_START(...)
#define FUSE_IOLOG_END(...)
#define FUSE_IOLOG_PRINT(...)
#define fuse_iolog_init(...)
#define fuse_iolog_exit(...)

#endif

#endif

