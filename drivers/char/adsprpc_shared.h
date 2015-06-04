/*
 * Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef ADSPRPC_SHARED_H
#define ADSPRPC_SHARED_H

#include <linux/types.h>

#define FASTRPC_IOCTL_INVOKE  _IOWR('R', 1, struct fastrpc_ioctl_invoke)
#define FASTRPC_IOCTL_MMAP    _IOWR('R', 2, struct fastrpc_ioctl_mmap)
#define FASTRPC_IOCTL_MUNMAP  _IOWR('R', 3, struct fastrpc_ioctl_munmap)
#define FASTRPC_IOCTL_INVOKE_FD  _IOWR('R', 4, struct fastrpc_ioctl_invoke_fd)
#define FASTRPC_IOCTL_SETMODE    _IOWR('R', 5, uint32_t)
#define FASTRPC_IOCTL_INIT       _IOWR('R', 6, struct fastrpc_ioctl_init)
#define FASTRPC_IOCTL_MUNMAP_REMOTE_HEAP  \
			_IOWR('R', 7, struct fastrpc_ioctl_munmap_remote_heap)
#define FASTRPC_SMD_GUID "fastrpcsmd-apps-dsp"
#define DEVICE_NAME      "adsprpc-smd"

/* Driver should operate in parallel with the co-processor */
#define FASTRPC_MODE_PARALLEL    0

/* Driver should operate in serial mode with the co-processor */
#define FASTRPC_MODE_SERIAL      1

/* INIT a new process or attach to guestos */
#define FASTRPC_INIT_ATTACH      0
#define FASTRPC_INIT_CREATE      1

/* Retrives number of input buffers from the scalars parameter */
#define REMOTE_SCALARS_INBUFS(sc)        (((sc) >> 16) & 0x0ff)

/* Retrives number of output buffers from the scalars parameter */
#define REMOTE_SCALARS_OUTBUFS(sc)       (((sc) >> 8) & 0x0ff)

/* Retrives number of input handles from the scalars parameter */
#define REMOTE_SCALARS_INHANDLES(sc)     (((sc) >> 4) & 0x0f)

/* Retrives number of output handles from the scalars parameter */
#define REMOTE_SCALARS_OUTHANDLES(sc)    ((sc) & 0x0f)

#define REMOTE_SCALARS_LENGTH(sc)	(REMOTE_SCALARS_INBUFS(sc) +\
					REMOTE_SCALARS_OUTBUFS(sc) +\
					REMOTE_SCALARS_INHANDLES(sc) +\
					REMOTE_SCALARS_OUTHANDLES(sc))

#define REMOTE_SCALARS_MAKEX(attr, method, in, out, oin, oout) \
		((((uint32_t)   (attr) & 0x7) << 29) | \
		(((uint32_t) (method) & 0x1f) << 24) | \
		(((uint32_t)     (in) & 0xff) << 16) | \
		(((uint32_t)    (out) & 0xff) <<  8) | \
		(((uint32_t)    (oin) & 0x0f) <<  4) | \
		((uint32_t)   (oout) & 0x0f))

#define REMOTE_SCALARS_MAKE(method, in, out) \
		REMOTE_SCALARS_MAKEX(0, method, in, out, 0, 0)


#ifndef VERIFY_PRINT_ERROR
#define VERIFY_EPRINTF(format, args) (void)0
#endif

#ifndef VERIFY_PRINT_INFO
#define VERIFY_IPRINTF(args) (void)0
#endif

#ifndef VERIFY
#define __STR__(x) #x ":"
#define __TOSTR__(x) __STR__(x)
#define __FILE_LINE__ __FILE__ ":" __TOSTR__(__LINE__)

#define VERIFY(err, val) \
do {\
	VERIFY_IPRINTF(__FILE_LINE__"info: calling: " #val "\n");\
	if (0 == (val)) {\
		(err) = (err) == 0 ? -1 : (err);\
		VERIFY_EPRINTF(__FILE_LINE__"error: %d: " #val "\n", (err));\
	} else {\
		VERIFY_IPRINTF(__FILE_LINE__"info: passed: " #val "\n");\
	} \
} while (0)
#endif

#define remote_arg64_t    union remote_arg64

struct remote_buf64 {
	uint64_t pv;
	int64_t len;
};

union remote_arg64 {
	struct remote_buf64	buf;
	uint32_t h;
};

#define remote_arg_t    union remote_arg

struct remote_buf {
	void *pv;
	size_t len;
};

union remote_arg {
	struct remote_buf     buf;
	uint32_t h;
};

struct fastrpc_ioctl_invoke {
	uint32_t handle;	/* remote handle */
	uint32_t sc;		/* scalars describing the data */
	remote_arg_t *pra;	/* remote arguments list */
};

struct fastrpc_ioctl_invoke_fd {
	struct fastrpc_ioctl_invoke inv;
	int *fds;		/* fd list */
};

struct fastrpc_ioctl_init {
	uint32_t flags;		/* one of FASTRPC_INIT_* macros */
	uintptr_t __user file;	/* pointer to elf file */
	int32_t filelen;	/* elf file length */
	int32_t filefd;		/* ION fd for the file */
	uintptr_t __user mem;	/* mem for the PD */
	int32_t memlen;		/* mem length */
	int32_t memfd;		/* ION fd for the mem */
};

struct fastrpc_ioctl_munmap {
	uintptr_t vaddrout;	/* address to unmap */
	ssize_t size;		/* size */
};

struct fastrpc_ioctl_munmap_remote_heap {
	uintptr_t vaddrout;	/* address to unmap */
	ssize_t size;		/* size */
	uint8_t akey;       /* authentication key */
};

struct fastrpc_ioctl_mmap {
	int fd;				/* ion fd */
	uint32_t flags;			/* flags for dsp to map with */
	uintptr_t __user *vaddrin;	/* optional virtual address */
	ssize_t size;			/* size */
	uintptr_t vaddrout;		/* dsps virtual address */
};

struct smq_null_invoke {
	uint64_t ctx;			/* invoke caller context */
	uint32_t handle;	    /* handle to invoke */
	uint32_t sc;		    /* scalars structure describing the data */
};

struct smq_phy_page {
	uint64_t addr;		/* physical address */
	uint64_t size;		/* size of contiguous region */
};

struct smq_invoke_buf {
	int num;		/* number of contiguous regions */
	int pgidx;		/* index to start of contiguous region */
};

struct smq_invoke {
	struct smq_null_invoke header;
	struct smq_phy_page page;   /* remote arg and list of pages address */
};

struct smq_msg {
	uint32_t pid;           /* process group id */
	uint32_t tid;           /* thread id */
	struct smq_invoke invoke;
};

struct smq_invoke_rsp {
	uint64_t ctx;			/* invoke caller context */
	int retval;	             /* invoke return value */
};

static inline struct smq_invoke_buf *smq_invoke_buf_start(remote_arg64_t *pra,
							uint32_t sc)
{
	int len = REMOTE_SCALARS_LENGTH(sc);
	return (struct smq_invoke_buf *)(&pra[len]);
}

static inline struct smq_phy_page *smq_phy_page_start(uint32_t sc,
						struct smq_invoke_buf *buf)
{
	int nTotal = REMOTE_SCALARS_INBUFS(sc) + REMOTE_SCALARS_OUTBUFS(sc);
	return (struct smq_phy_page *)(&buf[nTotal]);
}

#endif
