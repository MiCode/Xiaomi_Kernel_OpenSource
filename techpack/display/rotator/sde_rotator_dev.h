/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __SDE_ROTATOR_DEV_H__
#define __SDE_ROTATOR_DEV_H__

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include <linux/iommu.h>
#include <linux/dma-buf.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/llcc-qcom.h>
#include <linux/kthread.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ctrls.h>
#include <media/msm_sde_rotator.h>

#include "sde_rotator_core.h"
#include "sde_rotator_sync.h"

/* Rotator device name */
#define SDE_ROTATOR_DRV_NAME		"sde_rotator"

/* Event logging constants */
#define SDE_ROTATOR_NUM_EVENTS		4096
#define SDE_ROTATOR_NUM_TIMESTAMPS	SDE_ROTATOR_TS_MAX

/* maximum number of outstanding requests per ctx session */
#define SDE_ROTATOR_REQUEST_MAX		2

#define MAX_ROT_OPEN_SESSION 16

struct sde_rotator_device;
struct sde_rotator_ctx;

/*
 * struct sde_rotator_buf_handle - Structure contain rotator buffer information.
 * @fd: ion file descriptor from which this buffer is imported.
 * @rot_dev: Pointer to rotator device.
 * @ctx: Pointer to rotator context.
 * @size: Size of the buffer.
 * @addr: Address of rotator mmu mapped buffer.
 * @secure: Non-secure/secure buffer.
 * @buffer: Pointer to dma buf associated with this fd.
 */
struct sde_rotator_buf_handle {
	int fd;
	struct sde_rotator_device *rot_dev;
	struct sde_rotator_ctx *ctx;
	unsigned long size;
	dma_addr_t addr;
	int secure;
	struct dma_buf *buffer;
};

/*
 * struct sde_rotator_vbinfo - Structure define video buffer info.
 * @fd: fence file descriptor.
 * @fence: fence associated with fd.
 * @fence_ts: completion timestamp associated with fd
 * @qbuf_ts: timestamp associated with buffer queue event
 * @dqbuf_ts: Pointer to timestamp associated with buffer dequeue event
 * @comp_ratio: compression ratio of this buffer
 */
struct sde_rotator_vbinfo {
	int fd;
	struct sde_rot_sync_fence *fence;
	u32 fence_ts;
	ktime_t qbuf_ts;
	ktime_t *dqbuf_ts;
	struct sde_mult_factor comp_ratio;
};

/*
 * struct sde_rotator_request - device layer rotation request
 * @list: list head for submit/retire list
 * @submit_work: submit work structure
 * @retire_work: retire work structure
 * @req: Pointer to core layer rotator manager request
 *	 Request can be freed by core layer during sde_rotator_stop_streaming.
 *	 Avoid dereference in dev layer if possible.
 * @ctx: Pointer to parent context
 * @committed: true if request committed to hardware
 * @sequence_id: sequence identifier of this request
 */
struct sde_rotator_request {
	struct list_head list;
	struct kthread_work submit_work;
	struct kthread_work retire_work;
	struct sde_rot_entry_container *req;
	struct sde_rotator_ctx *ctx;
	bool committed;
	u32 sequence_id;
};

/*
 * struct sde_rotator_ctx - Structure contains per open file handle context.
 * @kobj: kernel object of this context
 * @rot_dev: Pointer to rotator device.
 * @file: Pointer to device file handle
 * @fh: V4l2 file handle.
 * @ctrl_handler: control handler
 * @format_cap: Current capture format.
 * @format_out: Current output format.
 * @crop_cap: Current capture crop.
 * @crop_out: Current output crop.
 * @timeperframe: Time per frame in seconds.
 * @session_id: unique id for this context
 * @hflip: horizontal flip (1-flip)
 * @vflip: vertical flip (1-flip)
 * @rotate: rotation angle (0,90,180,270)
 * @secure: Non-secure (0) / Secure processing
 * @abort_pending: True if abort is requested for async handling.
 * @nbuf_cap: Number of requested buffer for capture queue
 * @nbuf_out: Number of requested buffer for output queue
 * @fence_cap: Fence info for each requested capture buffer
 * @fence_out: Fence info for each requested output buffer
 * @wait_queue: Wait queue for signaling end of job
 * @work_queue: work queue for submit and retire processing
 * @private: Pointer to session private information
 * @slice: Pointer to system cache slice descriptor
 * @commit_sequence_id: last committed sequence id
 * @retired_sequence_id: last retired sequence id
 * @list_lock: lock for pending/retired list
 * @pending_list: list of pending request
 * @retired_list: list of retired/free request
 * @requests: static allocation of free requests
 * @rotcfg: current core rotation configuration
 * @kthread_id: thread_id used for fence management
 */
struct sde_rotator_ctx {
	struct kobject kobj;
	struct sde_rotator_device *rot_dev;
	struct file *file;
	struct v4l2_fh fh;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_format format_cap;
	struct v4l2_format format_out;
	struct v4l2_rect crop_cap;
	struct v4l2_rect crop_out;
	struct v4l2_fract timeperframe;
	u32 session_id;
	s32 hflip;
	s32 vflip;
	s32 rotate;
	s32 secure;
	s32 secure_camera;
	int abort_pending;
	int nbuf_cap;
	int nbuf_out;
	struct sde_rotator_vbinfo *vbinfo_cap;
	struct sde_rotator_vbinfo *vbinfo_out;
	wait_queue_head_t wait_queue;
	struct sde_rot_queue_v1 work_queue;
	struct sde_rot_file_private *private;
	struct llcc_slice_desc *slice;
	u32 commit_sequence_id;
	u32 retired_sequence_id;
	spinlock_t list_lock;
	struct list_head pending_list;
	struct list_head retired_list;
	struct sde_rotator_request requests[SDE_ROTATOR_REQUEST_MAX];
	struct sde_rotation_config rotcfg;

	int kthread_id;
};

/*
 * struct sde_rotator_statistics - Storage for statistics
 * @count: Number of processed request
 * @fail_count: Number of failed request
 * @ts: Timestamps of most recent requests
 */
struct sde_rotator_statistics {
	u64 count;
	u64 fail_count;
	ktime_t ts[SDE_ROTATOR_NUM_EVENTS][SDE_ROTATOR_NUM_TIMESTAMPS];
};

/*
 * struct sde_rotator_device - FD device structure.
 * @lock: Lock protecting this device structure and serializing IOCTL.
 * @dev: Pointer to device struct.
 * @v4l2_dev: V4l2 device.
 * @vdev: Pointer to video device.
 * @m2m_dev: Memory to memory device.
 * @pdev: Pointer to platform device.
 * @drvdata: Pointer to driver data.
 * @early_submit: flag enable job submission in ready state.
 * @disable_syscache: true to disable system cache
 * @mgr: Pointer to core rotator manager.
 * @mdata: Pointer to common rotator data/resource.
 * @session_id: Next context session identifier
 * @fence_timeout: Timeout value in msec for fence wait
 * @streamoff_timeout: Timeout value in msec for stream off
 * @min_rot_clk: Override the minimum rotator clock from perf calculation
 * @min_bw: Override the minimum bandwidth from perf calculation
 * @min_overhead_us: Override the minimum overhead in us from perf calculation
 * @debugfs_root: Pointer to debugfs directory entry.
 * @stats: placeholder for rotator statistics
 * @open_timeout: maximum wait time for ctx open in msec
 * @open_wq: wait queue for ctx open
 * @excl_ctx: Pointer to exclusive ctx
 * @rot_kw: rotator thread work
 * @rot_thread: rotator threads
 * @kthread_free: check if thread is available or not
 */
struct sde_rotator_device {
	struct mutex lock;
	struct device *dev;
	struct v4l2_device v4l2_dev;
	struct video_device *vdev;
#ifndef CONFIG_MSM_SDE_ROTATOR_INIT_ONLY
	struct v4l2_m2m_dev *m2m_dev;
#endif
	struct platform_device *pdev;
	const void *drvdata;
	u32 early_submit;
	u32 disable_syscache;
	struct sde_rot_mgr *mgr;
	struct sde_rot_data_type *mdata;
	u32 session_id;
	u32 fence_timeout;
	u32 streamoff_timeout;
	u32 min_rot_clk;
	u32 min_bw;
	u32 min_overhead_us;
	struct sde_rotator_statistics stats;
	struct dentry *debugfs_root;
	struct dentry *perf_root;
	u32 open_timeout;
	wait_queue_head_t open_wq;
	struct sde_rotator_ctx *excl_ctx;

	struct kthread_worker rot_kw[MAX_ROT_OPEN_SESSION];
	struct task_struct *rot_thread[MAX_ROT_OPEN_SESSION];
	bool kthread_free[MAX_ROT_OPEN_SESSION];
};

static inline
struct sde_rot_mgr *sde_rot_mgr_from_pdevice(struct platform_device *pdev)
{
	return ((struct sde_rotator_device *) platform_get_drvdata(pdev))->mgr;
}

static inline
struct sde_rot_mgr *sde_rot_mgr_from_device(struct device *dev)
{
	return ((struct sde_rotator_device *) dev_get_drvdata(dev))->mgr;
}

void sde_rotator_pm_qos_add(struct sde_rot_data_type *rot_mdata);

#endif /* __SDE_ROTATOR_DEV_H__ */
