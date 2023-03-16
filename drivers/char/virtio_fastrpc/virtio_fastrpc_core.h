/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __VIRTIO_FASTRPC_CORE_H__
#define __VIRTIO_FASTRPC_CORE_H__

#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include "../adsprpc_compat.h"
#include "../adsprpc_shared.h"
#include "virtio_fastrpc_base.h"

#define ADSP_MMAP_HEAP_ADDR		4
#define ADSP_MMAP_REMOTE_HEAP_ADDR	8
#define ADSP_MMAP_ADD_PAGES		0x1000

#define to_fastrpc_file(x) ((struct fastrpc_file *)&(x)->file)
#define to_vfastrpc_file(x) container_of(x, struct vfastrpc_file, file)

#define K_COPY_FROM_USER(err, kernel, dst, src, size) \
	do {\
		if (!(kernel))\
			VERIFY(err, 0 == copy_from_user((dst),\
			(void const __user *)(src),\
							(size)));\
		else\
			memmove((dst), (src), (size));\
	} while (0)

#define K_COPY_TO_USER(err, kernel, dst, src, size) \
	do {\
		if (!(kernel))\
			VERIFY(err, 0 == copy_to_user((void __user *)(dst),\
						(src), (size)));\
		else\
			memmove((dst), (src), (size));\
	} while (0)

#define K_COPY_TO_USER_WITHOUT_ERR(kernel, dst, src, size) \
	do {\
		if (!(kernel))\
			(void)copy_to_user((void __user *)(dst),\
			(src), (size));\
		else\
			memmove((dst), (src), (size));\
	} while (0)

struct virt_fastrpc_msg {
	struct completion work;
	struct vfastrpc_invoke_ctx *ctx;
	u16 msgid;
	void *txbuf;
	void *rxbuf;
};

struct vfastrpc_file {
	struct fastrpc_file file;
	struct vfastrpc_apps *apps;
	int domain;
	int procattrs;
	/*
	 * List to store virtio fastrpc cmds interrupted by signal while waiting
	 * for completion.
	 */
	struct hlist_head interrupted_cmds;
};

struct vfastrpc_invoke_ctx {
	struct virt_fastrpc_msg *msg;
	size_t size;
	struct vfastrpc_buf_desc *desc;
	struct hlist_node hn;
	struct list_head asyncn;
	struct vfastrpc_mmap **maps;
	remote_arg_t *lpra;
	int *fds;
	unsigned int outbufs_offset;
	unsigned int *attrs;
	struct vfastrpc_file *vfl;
	int pid;
	int tgid;
	uint32_t sc;
	uint32_t handle;
	uint32_t *crc;
	struct fastrpc_perf *perf;
	uint64_t *perf_kernel;
	uint64_t *perf_dsp;
	struct fastrpc_async_job asyncjob;
};

struct virt_msg_hdr {
	u32 pid;	/* GVM pid */
	u32 tid;	/* GVM tid */
	s32 cid;	/* channel id connected to DSP */
	u32 cmd;	/* command type */
	u32 len;	/* command length */
	u16 msgid;	/* unique message id */
	u32 result;	/* message return value */
} __packed;

struct vfastrpc_file *vfastrpc_file_alloc(void);
int vfastrpc_file_free(struct vfastrpc_file *vfl);
int vfastrpc_internal_get_info(struct vfastrpc_file *vfl,
					uint32_t *info);
int vfastrpc_get_info_from_kernel(
		struct fastrpc_ioctl_capability *cap,
		struct vfastrpc_file *vfl);
int vfastrpc_internal_control(struct vfastrpc_file *vfl,
					struct fastrpc_ioctl_control *cp);
int vfastrpc_internal_init_process(struct vfastrpc_file *vfl,
				struct fastrpc_ioctl_init_attrs *uproc);
int vfastrpc_internal_mmap(struct vfastrpc_file *vfl,
				 struct fastrpc_ioctl_mmap *ud);
int vfastrpc_internal_munmap(struct vfastrpc_file *vfl,
				   struct fastrpc_ioctl_munmap *ud);
int vfastrpc_internal_munmap_fd(struct vfastrpc_file *vfl,
				struct fastrpc_ioctl_munmap_fd *ud);
int vfastrpc_internal_invoke(struct vfastrpc_file *vfl,
		uint32_t mode, struct fastrpc_ioctl_invoke_async *inv);

int vfastrpc_internal_get_dsp_info(struct fastrpc_ioctl_capability *cap,
		void *param, struct vfastrpc_file *vfl);

int vfastrpc_internal_invoke2(struct vfastrpc_file *vfl,
				struct fastrpc_ioctl_invoke2 *inv2);

void vfastrpc_queue_completed_async_job(struct vfastrpc_invoke_ctx *ctx);

int vfastrpc_internal_mem_map(struct vfastrpc_file *vfl,
				struct fastrpc_ioctl_mem_map *ud);

int vfastrpc_internal_mem_unmap(struct vfastrpc_file *vfl,
				struct fastrpc_ioctl_mem_unmap *ud);
#endif /*__VIRTIO_FASTRPC_CORE_H__*/
