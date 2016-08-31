/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __LINUX_NVHOST_DBG_GPU_IOCTL_H
#define __LINUX_NVHOST_DBG_GPU_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#if !defined(__KERNEL__)
#define __user
#endif

#define NVHOST_DBG_GPU_IOCTL_MAGIC 'D'

/*
 * /dev/nvhost-dbg-* devices
 *
 * Opening a '/dev/nvhost-dbg-<module_name>' device node creates a new debugger
 * session.  nvhost channels (for the same module) can then be bound to such a
 * session.
 *
 * Once a nvhost channel has been bound to a debugger session it cannot be
 * bound to another.
 *
 * As long as there is an open device file to the session, or any bound
 * nvhost channels it will be valid.  Once all references to the session
 * are removed the session is deleted.
 *
 */

/*
 * Binding/attaching a debugger session to an nvhost gpu channel
 *
 * The 'channel_fd' given here is the fd used to allocate the
 * gpu channel context.  To detach/unbind the debugger session
 * use a channel_fd of -1.
 *
 */
struct nvhost_dbg_gpu_bind_channel_args {
	__u32 channel_fd; /* in */
	__u32 _pad0[1];
};

#define NVHOST_DBG_GPU_IOCTL_BIND_CHANNEL				\
	_IOWR(NVHOST_DBG_GPU_IOCTL_MAGIC, 1, struct nvhost_dbg_gpu_bind_channel_args)

/*
 * Register operations
 */
/* valid op values */
#define NVHOST_DBG_GPU_REG_OP_READ_32                             (0x00000000)
#define NVHOST_DBG_GPU_REG_OP_WRITE_32                            (0x00000001)
#define NVHOST_DBG_GPU_REG_OP_READ_64                             (0x00000002)
#define NVHOST_DBG_GPU_REG_OP_WRITE_64                            (0x00000003)
/* note: 8b ops are unsupported */
#define NVHOST_DBG_GPU_REG_OP_READ_08                             (0x00000004)
#define NVHOST_DBG_GPU_REG_OP_WRITE_08                            (0x00000005)

/* valid type values */
#define NVHOST_DBG_GPU_REG_OP_TYPE_GLOBAL                         (0x00000000)
#define NVHOST_DBG_GPU_REG_OP_TYPE_GR_CTX                         (0x00000001)
#define NVHOST_DBG_GPU_REG_OP_TYPE_GR_CTX_TPC                     (0x00000002)
#define NVHOST_DBG_GPU_REG_OP_TYPE_GR_CTX_SM                      (0x00000004)
#define NVHOST_DBG_GPU_REG_OP_TYPE_GR_CTX_CROP                    (0x00000008)
#define NVHOST_DBG_GPU_REG_OP_TYPE_GR_CTX_ZROP                    (0x00000010)
/*#define NVHOST_DBG_GPU_REG_OP_TYPE_FB                           (0x00000020)*/
#define NVHOST_DBG_GPU_REG_OP_TYPE_GR_CTX_QUAD                    (0x00000040)

/* valid status values */
#define NVHOST_DBG_GPU_REG_OP_STATUS_SUCCESS                      (0x00000000)
#define NVHOST_DBG_GPU_REG_OP_STATUS_INVALID_OP                   (0x00000001)
#define NVHOST_DBG_GPU_REG_OP_STATUS_INVALID_TYPE                 (0x00000002)
#define NVHOST_DBG_GPU_REG_OP_STATUS_INVALID_OFFSET               (0x00000004)
#define NVHOST_DBG_GPU_REG_OP_STATUS_UNSUPPORTED_OP               (0x00000008)
#define NVHOST_DBG_GPU_REG_OP_STATUS_INVALID_MASK                 (0x00000010)

struct nvhost_dbg_gpu_reg_op {
	__u8    op;
	__u8    type;
	__u8    status;
	__u8    quad;
	__u32   group_mask;
	__u32   sub_group_mask;
	__u32   offset;
	__u32   value_lo;
	__u32   value_hi;
	__u32   and_n_mask_lo;
	__u32   and_n_mask_hi;
};

struct nvhost_dbg_gpu_exec_reg_ops_args {
	__u64 ops; /* pointer to nvhost_reg_op operations */
	__u32 num_ops;
	__u32 _pad0[1];
};

#define NVHOST_DBG_GPU_IOCTL_REG_OPS					\
	_IOWR(NVHOST_DBG_GPU_IOCTL_MAGIC, 2, struct nvhost_dbg_gpu_exec_reg_ops_args)

/* Enable/disable/clear event notifications */
struct nvhost_dbg_gpu_events_ctrl_args {
	__u32 cmd; /* in */
	__u32 _pad0[1];
};

/* valid event ctrl values */
#define NVHOST_DBG_GPU_EVENTS_CTRL_CMD_DISABLE                    (0x00000000)
#define NVHOST_DBG_GPU_EVENTS_CTRL_CMD_ENABLE                     (0x00000001)
#define NVHOST_DBG_GPU_EVENTS_CTRL_CMD_CLEAR                      (0x00000002)

#define NVHOST_DBG_GPU_IOCTL_EVENTS_CTRL				\
	_IOWR(NVHOST_DBG_GPU_IOCTL_MAGIC, 3, struct nvhost_dbg_gpu_events_ctrl_args)


/* Powergate/Unpowergate control */

#define NVHOST_DBG_GPU_POWERGATE_MODE_ENABLE                                 1
#define NVHOST_DBG_GPU_POWERGATE_MODE_DISABLE                                2

struct nvhost_dbg_gpu_powergate_args {
	__u32 mode;
} __packed;

#define NVHOST_DBG_GPU_IOCTL_POWERGATE					\
	_IOWR(NVHOST_DBG_GPU_IOCTL_MAGIC, 4, struct nvhost_dbg_gpu_powergate_args)


/* SMPC Context Switch Mode */
#define NVHOST_DBG_GPU_SMPC_CTXSW_MODE_NO_CTXSW               (0x00000000)
#define NVHOST_DBG_GPU_SMPC_CTXSW_MODE_CTXSW                  (0x00000001)

struct nvhost_dbg_gpu_smpc_ctxsw_mode_args {
	__u32 mode;
} __packed;

#define NVHOST_DBG_GPU_IOCTL_SMPC_CTXSW_MODE \
	_IOWR(NVHOST_DBG_GPU_IOCTL_MAGIC, 5, struct nvhost_dbg_gpu_smpc_ctxsw_mode_args)


#define NVHOST_DBG_GPU_IOCTL_LAST		\
	_IOC_NR(NVHOST_DBG_GPU_IOCTL_SMPC_CTXSW_MODE)
#define NVHOST_DBG_GPU_IOCTL_MAX_ARG_SIZE		\
	sizeof(struct nvhost_dbg_gpu_exec_reg_ops_args)

#endif
