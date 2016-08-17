/*
 * include/linux/nvhost_ioctl.h
 *
 * Tegra graphics host driver
 *
 * Copyright (c) 2009-2013, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __LINUX_NVHOST_IOCTL_H
#define __LINUX_NVHOST_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#if !defined(__KERNEL__)
#define __user
#endif

#define NVHOST_INVALID_SYNCPOINT 0xFFFFFFFF
#define NVHOST_NO_TIMEOUT (-1)
#define NVHOST_NO_CONTEXT 0x0
#define NVHOST_IOCTL_MAGIC 'H'
#define NVHOST_PRIORITY_LOW 50
#define NVHOST_PRIORITY_MEDIUM 100
#define NVHOST_PRIORITY_HIGH 150

#define NVHOST_TIMEOUT_FLAG_DISABLE_DUMP	0

/* version 0 header (used with write() submit interface) */
struct nvhost_submit_hdr {
	__u32 syncpt_id;
	__u32 syncpt_incrs;
	__u32 num_cmdbufs;
	__u32 num_relocs;
};

#define NVHOST_SUBMIT_VERSION_V0		0x0
#define NVHOST_SUBMIT_VERSION_V1		0x1
#define NVHOST_SUBMIT_VERSION_V2		0x2
#define NVHOST_SUBMIT_VERSION_MAX_SUPPORTED	NVHOST_SUBMIT_VERSION_V2

/* version 1 header (used with ioctl() submit interface) */
struct nvhost_submit_hdr_ext {
	__u32 syncpt_id;	/* version 0 fields */
	__u32 syncpt_incrs;
	__u32 num_cmdbufs;
	__u32 num_relocs;
	__u32 submit_version;	/* version 1 fields */
	__u32 num_waitchks;
	__u32 waitchk_mask;
	__u32 pad[5];		/* future expansion */
};

struct nvhost_cmdbuf {
	__u32 mem;
	__u32 offset;
	__u32 words;
};

struct nvhost_reloc {
	__u32 cmdbuf_mem;
	__u32 cmdbuf_offset;
	__u32 target;
	__u32 target_offset;
};

struct nvhost_reloc_shift {
	__u32 shift;
};

struct nvhost_waitchk {
	__u32 mem;
	__u32 offset;
	__u32 syncpt_id;
	__u32 thresh;
};

struct nvhost_syncpt_incr {
	__u32 syncpt_id;
	__u32 syncpt_incrs;
};

struct nvhost_get_param_args {
	__u32 value;
};

struct nvhost_set_nvmap_fd_args {
	__u32 fd;
};

struct nvhost_read_3d_reg_args {
	__u32 offset;
	__u32 value;
};

enum nvhost_clk_attr {
	NVHOST_CLOCK = 0,
	NVHOST_BW,
};

/*
 * moduleid[15:0]  => module id
 * moduleid[24:31] => nvhost_clk_attr
 */
#define NVHOST_MODULE_ID_BIT_POS	0
#define NVHOST_MODULE_ID_BIT_WIDTH	16
#define NVHOST_CLOCK_ATTR_BIT_POS	24
#define NVHOST_CLOCK_ATTR_BIT_WIDTH	8
struct nvhost_clk_rate_args {
	__u32 rate;
	__u32 moduleid;
};

struct nvhost_set_timeout_args {
	__u32 timeout;
};

struct nvhost_set_timeout_ex_args {
	__u32 timeout;
	__u32 flags;
};

struct nvhost_set_priority_args {
	__u32 priority;
};

struct nvhost_ctrl_module_regrdwr_args {
	__u32 id;
	__u32 num_offsets;
	__u32 block_size;
	__u32 *offsets;
	__u32 *values;
	__u32 write;
};

struct nvhost_submit_args {
	__u32 submit_version;
	__u32 num_syncpt_incrs;
	__u32 num_cmdbufs;
	__u32 num_relocs;
	__u32 num_waitchks;
	__u32 timeout;
	struct nvhost_syncpt_incr *syncpt_incrs;
	struct nvhost_cmdbuf *cmdbufs;
	struct nvhost_reloc *relocs;
	struct nvhost_reloc_shift *reloc_shifts;
	struct nvhost_waitchk *waitchks;

	__u32 pad[5];		/* future expansion */
	__u32 fence;		/* Return value */
};

#define NVHOST_IOCTL_CHANNEL_FLUSH		\
	_IOR(NVHOST_IOCTL_MAGIC, 1, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_GET_SYNCPOINTS	\
	_IOR(NVHOST_IOCTL_MAGIC, 2, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_GET_WAITBASES	\
	_IOR(NVHOST_IOCTL_MAGIC, 3, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_GET_MODMUTEXES	\
	_IOR(NVHOST_IOCTL_MAGIC, 4, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_SET_NVMAP_FD	\
	_IOW(NVHOST_IOCTL_MAGIC, 5, struct nvhost_set_nvmap_fd_args)
#define NVHOST_IOCTL_CHANNEL_NULL_KICKOFF	\
	_IOR(NVHOST_IOCTL_MAGIC, 6, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_SUBMIT_EXT		\
	_IOW(NVHOST_IOCTL_MAGIC, 7, struct nvhost_submit_hdr_ext)
#define NVHOST_IOCTL_CHANNEL_READ_3D_REG \
	_IOWR(NVHOST_IOCTL_MAGIC, 8, struct nvhost_read_3d_reg_args)
#define NVHOST_IOCTL_CHANNEL_GET_CLK_RATE		\
	_IOR(NVHOST_IOCTL_MAGIC, 9, struct nvhost_clk_rate_args)
#define NVHOST_IOCTL_CHANNEL_SET_CLK_RATE		\
	_IOW(NVHOST_IOCTL_MAGIC, 10, struct nvhost_clk_rate_args)
#define NVHOST_IOCTL_CHANNEL_SET_TIMEOUT	\
	_IOW(NVHOST_IOCTL_MAGIC, 11, struct nvhost_set_timeout_args)
#define NVHOST_IOCTL_CHANNEL_GET_TIMEDOUT	\
	_IOR(NVHOST_IOCTL_MAGIC, 12, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_SET_PRIORITY	\
	_IOW(NVHOST_IOCTL_MAGIC, 13, struct nvhost_set_priority_args)
#define NVHOST_IOCTL_CHANNEL_MODULE_REGRDWR	\
	_IOWR(NVHOST_IOCTL_MAGIC, 14, struct nvhost_ctrl_module_regrdwr_args)
#define NVHOST_IOCTL_CHANNEL_SUBMIT		\
	_IOWR(NVHOST_IOCTL_MAGIC, 15, struct nvhost_submit_args)
#define NVHOST_IOCTL_CHANNEL_SET_TIMEOUT_EX	\
	_IOWR(NVHOST_IOCTL_MAGIC, 18, struct nvhost_set_timeout_ex_args)
#define NVHOST_IOCTL_CHANNEL_LAST		\
	_IOC_NR(NVHOST_IOCTL_CHANNEL_SET_TIMEOUT_EX)
#define NVHOST_IOCTL_CHANNEL_MAX_ARG_SIZE sizeof(struct nvhost_submit_args)

struct nvhost_ctrl_syncpt_read_args {
	__u32 id;
	__u32 value;
};

struct nvhost_ctrl_syncpt_incr_args {
	__u32 id;
};

struct nvhost_ctrl_syncpt_wait_args {
	__u32 id;
	__u32 thresh;
	__s32 timeout;
};

struct nvhost_ctrl_syncpt_waitex_args {
	__u32 id;
	__u32 thresh;
	__s32 timeout;
	__u32 value;
};

struct nvhost_ctrl_module_mutex_args {
	__u32 id;
	__u32 lock;
};

enum nvhost_module_id {
	NVHOST_MODULE_NONE = -1,
	NVHOST_MODULE_DISPLAY_A = 0,
	NVHOST_MODULE_DISPLAY_B,
	NVHOST_MODULE_VI,
	NVHOST_MODULE_ISP,
	NVHOST_MODULE_MPE,
	NVHOST_MODULE_MSENC,
	NVHOST_MODULE_TSEC,
};

#define NVHOST_IOCTL_CTRL_SYNCPT_READ		\
	_IOWR(NVHOST_IOCTL_MAGIC, 1, struct nvhost_ctrl_syncpt_read_args)
#define NVHOST_IOCTL_CTRL_SYNCPT_INCR		\
	_IOW(NVHOST_IOCTL_MAGIC, 2, struct nvhost_ctrl_syncpt_incr_args)
#define NVHOST_IOCTL_CTRL_SYNCPT_WAIT		\
	_IOW(NVHOST_IOCTL_MAGIC, 3, struct nvhost_ctrl_syncpt_wait_args)

#define NVHOST_IOCTL_CTRL_MODULE_MUTEX		\
	_IOWR(NVHOST_IOCTL_MAGIC, 4, struct nvhost_ctrl_module_mutex_args)
#define NVHOST_IOCTL_CTRL_MODULE_REGRDWR	\
	_IOWR(NVHOST_IOCTL_MAGIC, 5, struct nvhost_ctrl_module_regrdwr_args)

#define NVHOST_IOCTL_CTRL_SYNCPT_WAITEX		\
	_IOWR(NVHOST_IOCTL_MAGIC, 6, struct nvhost_ctrl_syncpt_waitex_args)

#define NVHOST_IOCTL_CTRL_GET_VERSION	\
	_IOR(NVHOST_IOCTL_MAGIC, 7, struct nvhost_get_param_args)

#define NVHOST_IOCTL_CTRL_SYNCPT_READ_MAX	\
	_IOWR(NVHOST_IOCTL_MAGIC, 8, struct nvhost_ctrl_syncpt_read_args)

#define NVHOST_IOCTL_CTRL_LAST			\
	_IOC_NR(NVHOST_IOCTL_CTRL_SYNCPT_READ_MAX)
#define NVHOST_IOCTL_CTRL_MAX_ARG_SIZE	\
	sizeof(struct nvhost_ctrl_module_regrdwr_args)

#endif
