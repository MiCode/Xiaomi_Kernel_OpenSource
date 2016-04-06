/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_FD_DEV_H__
#define __MSM_FD_DEV_H__

#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ctrls.h>
#include <linux/msm-bus.h>
#include <media/msm_fd.h>
#include <linux/dma-buf.h>
#include <linux/msm_ion.h>
#include "cam_soc_api.h"
#include "cam_hw_ops.h"
/* Maximum number of result buffers */
#define MSM_FD_MAX_RESULT_BUFS 5
/* Max number of clocks defined in device tree */
#define MSM_FD_MAX_CLK_NUM 15
/* Max number of clock rates defined in device tree */
#define MSM_FD_MAX_CLK_RATES 5
/* Max number of faces which can be detected in one hw processing */
#define MSM_FD_MAX_FACES_DETECTED 32
/* Max number of regulators defined in device tree */
#define MSM_FD_MAX_REGULATOR_NUM 3

/*
 * struct msm_fd_size - Structure contain FD size related values.
 * @width: Image width.
 * @height: Image height.
 * @reg_val: Register value for this size.
 * @work_size: Working buffer size in bytes for this size.
 */
struct msm_fd_size {
	int width;
	int height;
	u32 reg_val;
	int work_size;
};

/*
 * struct msm_fd_setings - Structure contain FD settings values.
 * @min_size_index: Minimum face size array index.
 * @angle_index: Face detection angle array index.
 * @direction_index: Face detection direction array index.
 * @threshold: Face detection threshold value.
 * @speed: Face detection speed value (it should match with clock rate index).
 */
struct msm_fd_setings {
	unsigned int min_size_index;
	unsigned int angle_index;
	unsigned int direction_index;
	unsigned int threshold;
	unsigned int speed;
};

/*
 * struct msm_fd_format - Structure contain FD format settings.
 * @size: Pointer to fd size struct used for this format.
 * @crop: V4l2 crop structure.
 * @bytesperline: Bytes per line of input image buffer.
 * @sizeimage: Size of input image buffer.
 * @pixelformat: Pixel format of input image buffer.
 */
struct msm_fd_format {
	struct msm_fd_size *size;
	struct v4l2_rect crop;
	int bytesperline;
	int sizeimage;
	u32 pixelformat;
};

/*
 * struct msm_fd_mem_pool - Structure contain FD memory pool information.
 * @fd_device: Pointer to fd device.
 * @client: Pointer to ion client.
 * @domain_num: Domain number associated with FD hw.
 */
struct msm_fd_mem_pool {
	struct msm_fd_device *fd_device;
};

/*
 * struct msm_fd_buf_handle - Structure contain FD buffer handle information.
 * @fd: ion FD from which this buffer is imported.
 * @pool: Pointer to FD memory pool struct.
 * @handle: Pointer to ion handle.
 * @size: Size of the buffer.
 * @addr: Adders of FD mmu mapped buffer. This address should be set to FD hw.
 */
struct msm_fd_buf_handle {
	int fd;
	struct msm_fd_mem_pool *pool;
	size_t size;
	ion_phys_addr_t addr;
};

/*
 * struct msm_fd_buffer - Vb2 buffer wrapper structure.
 * @vb: Videobuf 2 buffer structure.
 * @active: Flag indicating if buffer currently used by FD hw.
 * @completion: Completion need to wait on, if buffer is used by FD hw.
 * @format: Format information of this buffer.
 * @settings: Settings value of this buffer.
 * @work_addr: Working buffer address need to be used when for this buffer.
 * @list: Buffer is part of FD device processing queue
 */
struct msm_fd_buffer {
	struct vb2_buffer vb;
	atomic_t active;
	struct completion completion;
	struct msm_fd_format format;
	struct msm_fd_setings settings;
	ion_phys_addr_t work_addr;
	struct list_head list;
};

/*
 * struct msm_fd_stats - Structure contains FD result statistic information.
 * @frame_id: Frame id for which statistic corresponds to.
 * @face_cnt: Number of faces detected and included in face data.
 * @face_data: Structure containing detected face data information.
 */
struct msm_fd_stats {
	atomic_t frame_id;
	u32 face_cnt;
	struct msm_fd_face_data face_data[MSM_FD_MAX_FACES_DETECTED];
};

/*
 * struct fd_ctx - Structure contains per open file handle context.
 * @fd_device: Pointer to fd device.
 * @fh: V4l2 file handle.
 * @vb2_q: Videobuf 2 queue.
 * @sequence: Sequence number for this statistic.
 * @format: Current format.
 * @settings: Current settings.
 * @mem_pool: FD hw memory pool.
 * @stats: Pointer to statistic buffers.
 * @work_buf: Working memory buffer handle.
 */
struct fd_ctx {
	struct msm_fd_device *fd_device;
	struct v4l2_fh fh;
	struct vb2_queue vb2_q;
	unsigned int sequence;
	atomic_t subscribed_for_event;
	struct msm_fd_format format;
	struct msm_fd_setings settings;
	struct msm_fd_mem_pool mem_pool;
	struct msm_fd_stats *stats;
	struct msm_fd_buf_handle work_buf;
};

/*
 * enum msm_fd_device_state - FD device state.
 * @MSM_FD_DEVICE_IDLE: Device is idle, we can start with processing.
 * @MSM_FD_DEVICE_RUNNING: Device is running, next processing will be
 * scheduled from fd irq.
 */
enum msm_fd_device_state {
	MSM_FD_DEVICE_IDLE,
	MSM_FD_DEVICE_RUNNING,
};

/*
 * enum msm_fd_mem_resources - FD device iomem resources.
 * @MSM_FD_IOMEM_CORE: Index of fd core registers.
 * @MSM_FD_IOMEM_MISC: Index of fd misc registers.
 * @MSM_FD_IOMEM_VBIF: Index of fd vbif registers.
 * @MSM_FD_IOMEM_LAST: Not valid.
 */
enum msm_fd_mem_resources {
	MSM_FD_IOMEM_CORE,
	MSM_FD_IOMEM_MISC,
	MSM_FD_IOMEM_VBIF,
	MSM_FD_IOMEM_LAST
};

/*
 * struct msm_fd_device - FD device structure.
 * @hw_revision: Face detection hw revision.
 * @lock: Lock used for reference count.
 * @slock: Spinlock used to protect FD device struct.
 * @irq_num: Face detection irq number.
 * @ref_count: Device reference count.
 * @res_mem: Array of memory resources used by FD device.
 * @iomem_base: Array of register mappings used by FD device.
 * @vdd: Pointer to vdd regulator.
 * @clk_num: Number of clocks attached to the device.
 * @clk: Array of clock resources used by fd device.
 * @clk_rates: Array of clock rates set.
 * @bus_vectors: Pointer to bus vectors array.
 * @bus_paths: Pointer to bus paths array.
 * @bus_scale_data: Memory access bus scale data.
 * @bus_client: Memory access bus client.
 * @iommu_attached_cnt: Iommu attached devices reference count.
 * @iommu_hdl: reference for iommu context.
 * @dev: Pointer to device struct.
 * @v4l2_dev: V4l2 device.
 * @video: Video device.
 * @state: FD device state.
 * @buf_queue: FD device processing queue.
 * @work_queue: Pointer to FD device IRQ bottom half workqueue.
 * @work: IRQ bottom half work struct.
 * @hw_halt_completion: Completes when face detection hw halt completes.
 */
struct msm_fd_device {
	u32 hw_revision;

	struct mutex lock;
	spinlock_t slock;
	int ref_count;

	int irq_num;
	void __iomem *iomem_base[MSM_FD_IOMEM_LAST];
	struct msm_cam_clk_info *clk_info;
	struct msm_cam_regulator *vdd_info;
	int num_reg;
	struct resource *irq;

	size_t clk_num;
	size_t clk_rates_num;
	struct clk **clk;
	uint32_t **clk_rates;
	uint32_t bus_client;

	unsigned int iommu_attached_cnt;

	int iommu_hdl;
	struct device *dev;
	struct platform_device *pdev;
	struct v4l2_device v4l2_dev;
	struct video_device video;

	enum msm_fd_device_state state;
	struct list_head buf_queue;
	struct workqueue_struct *work_queue;
	struct work_struct work;
	struct completion hw_halt_completion;
};

#endif /* __MSM_FD_DEV_H__ */
