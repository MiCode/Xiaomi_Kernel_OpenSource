/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_VFE_H__
#define __MSM_VFE_H__

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_isp.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>

#include "msm_buf_mgr.h"

#define MAX_IOMMU_CTX 2
#define MAX_NUM_WM 7
#define MAX_NUM_RDI 3
#define MAX_NUM_RDI_MASTER 3
#define MAX_NUM_COMPOSITE_MASK 4
#define MAX_NUM_STATS_COMP_MASK 2
#define MAX_INIT_FRAME_DROP 31
#define ISP_Q2 (1 << 2)

#define VFE_PING_FLAG 0xFFFFFFFF
#define VFE_PONG_FLAG 0x0

struct vfe_device;
struct msm_vfe_axi_stream;
struct msm_vfe_stats_stream;

struct vfe_subscribe_info {
	struct v4l2_fh *vfh;
	uint32_t active;
};

enum msm_isp_camif_update_state {
	NO_UPDATE,
	ENABLE_CAMIF,
	DISABLE_CAMIF,
	DISABLE_CAMIF_IMMEDIATELY
};

struct msm_isp_timestamp {
	/*Monotonic clock for v4l2 buffer*/
	struct timeval buf_time;
	/*Wall clock for userspace event*/
	struct timeval event_time;
};

struct msm_vfe_irq_ops {
	void (*read_irq_status) (struct vfe_device *vfe_dev,
		uint32_t *irq_status0, uint32_t *irq_status1);
	void (*process_reg_update) (struct vfe_device *vfe_dev,
		uint32_t irq_status0, uint32_t irq_status1,
		struct msm_isp_timestamp *ts);
	void (*process_reset_irq) (struct vfe_device *vfe_dev,
		uint32_t irq_status0, uint32_t irq_status1);
	void (*process_halt_irq) (struct vfe_device *vfe_dev,
		uint32_t irq_status0, uint32_t irq_status1);
	void (*process_camif_irq) (struct vfe_device *vfe_dev,
		uint32_t irq_status0, uint32_t irq_status1,
		struct msm_isp_timestamp *ts);
	void (*process_axi_irq) (struct vfe_device *vfe_dev,
		uint32_t irq_status0, uint32_t irq_status1,
		struct msm_isp_timestamp *ts);
	void (*process_stats_irq) (struct vfe_device *vfe_dev,
		uint32_t irq_status0, uint32_t irq_status1,
		struct msm_isp_timestamp *ts);
};

struct msm_vfe_axi_ops {
	void (*reload_wm) (struct vfe_device *vfe_dev,
		uint32_t reload_mask);
	void (*enable_wm) (struct vfe_device *vfe_dev,
		uint8_t wm_idx, uint8_t enable);
	void (*cfg_io_format) (struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream_request_cmd *stream_req_cmd);
	void (*cfg_framedrop) (struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info);
	void (*clear_framedrop) (struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info);
	void (*cfg_comp_mask) (struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info);
	void (*clear_comp_mask) (struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info);
	void (*cfg_wm_irq_mask) (struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info);
	void (*clear_wm_irq_mask) (struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info);

	void (*cfg_wm_reg) (struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd,
		uint8_t plane_idx);
	void (*clear_wm_reg) (struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx);

	void (*cfg_wm_xbar_reg) (struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd,
		uint8_t plane_idx);
	void (*clear_wm_xbar_reg) (struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx);

	void (*cfg_ub) (struct vfe_device *vfe_dev);

	void (*update_ping_pong_addr) (struct vfe_device *vfe_dev,
		uint8_t wm_idx, uint32_t pingpong_status, unsigned long paddr);

	uint32_t (*get_wm_mask) (uint32_t irq_status0, uint32_t irq_status1);
	uint32_t (*get_comp_mask) (uint32_t irq_status0, uint32_t irq_status1);
	uint32_t (*get_pingpong_status) (struct vfe_device *vfe_dev);
	long (*halt) (struct vfe_device *vfe_dev);
};

struct msm_vfe_core_ops {
	void (*reg_update) (struct vfe_device *vfe_dev);
	long (*reset_hw) (struct vfe_device *vfe_dev);
	int (*init_hw) (struct vfe_device *vfe_dev);
	void (*init_hw_reg) (struct vfe_device *vfe_dev);
	void (*release_hw) (struct vfe_device *vfe_dev);
	void (*cfg_camif) (struct vfe_device *vfe_dev,
		struct msm_vfe_pix_cfg *pix_cfg);
	void (*update_camif_state) (struct vfe_device *vfe_dev,
		enum msm_isp_camif_update_state update_state);
	void (*cfg_rdi_reg) (struct vfe_device *vfe_dev,
		struct msm_vfe_rdi_cfg *rdi_cfg,
		enum msm_vfe_input_src input_src);
	int (*get_platform_data) (struct vfe_device *vfe_dev);
	void (*get_error_mask) (uint32_t *error_mask0, uint32_t *error_mask1);
	void (*process_error_status) (struct vfe_device *vfe_dev);
};
struct msm_vfe_stats_ops {
	int (*get_stats_idx) (enum msm_isp_stats_type stats_type);
	void (*cfg_framedrop) (struct vfe_device *vfe_dev,
		struct msm_vfe_stats_stream *stream_info);
	void (*clear_framedrop) (struct vfe_device *vfe_dev,
		struct msm_vfe_stats_stream *stream_info);
	void (*cfg_comp_mask) (struct vfe_device *vfe_dev);
	void (*cfg_wm_irq_mask) (struct vfe_device *vfe_dev,
		struct msm_vfe_stats_stream *stream_info);
	void (*clear_wm_irq_mask) (struct vfe_device *vfe_dev,
		struct msm_vfe_stats_stream *stream_info);

	void (*cfg_wm_reg) (struct vfe_device *vfe_dev,
		struct msm_vfe_stats_stream *stream_info);
	void (*clear_wm_reg) (struct vfe_device *vfe_dev,
		struct msm_vfe_stats_stream *stream_info);

	void (*cfg_ub) (struct vfe_device *vfe_dev);

	void (*enable_module) (struct vfe_device *vfe_dev,
		uint32_t stats_mask, uint8_t enable);

	void (*update_ping_pong_addr) (struct vfe_device *vfe_dev,
		struct msm_vfe_stats_stream *stream_info,
		uint32_t pingpong_status, unsigned long paddr);

	uint32_t (*get_frame_id) (struct vfe_device *vfe_dev);
	uint32_t (*get_wm_mask) (uint32_t irq_status0, uint32_t irq_status1);
	uint32_t (*get_comp_mask) (uint32_t irq_status0, uint32_t irq_status1);
	uint32_t (*get_pingpong_status) (struct vfe_device *vfe_dev);
};

struct msm_vfe_ops {
	struct msm_vfe_irq_ops irq_ops;
	struct msm_vfe_axi_ops axi_ops;
	struct msm_vfe_core_ops core_ops;
	struct msm_vfe_stats_ops stats_ops;
};

struct msm_vfe_hardware_info {
	int num_iommu_ctx;
	int vfe_clk_idx;
	struct msm_vfe_ops vfe_ops;
	struct msm_vfe_axi_hardware_info *axi_hw_info;
	struct msm_vfe_stats_hardware_info *stats_hw_info;
	struct v4l2_subdev_internal_ops *subdev_internal_ops;
	struct v4l2_subdev_ops *subdev_ops;
	uint32_t dmi_reg_offset;
};

struct msm_vfe_axi_hardware_info {
	uint8_t num_wm;
	uint8_t num_rdi;
	uint8_t num_rdi_master;
	uint8_t num_comp_mask;
	uint32_t min_wm_ub;
};

enum msm_vfe_axi_state {
	AVALIABLE,
	INACTIVE,
	ACTIVE,
	PAUSE,
	START_PENDING,
	STOP_PENDING,
	STARTING,
	STOPPING,
	PAUSE_PENDING,
};

#define VFE_NO_DROP            0xFFFFFFFF
#define VFE_DROP_EVERY_2FRAME  0x55555555
#define VFE_DROP_EVERY_4FRAME  0x11111111
#define VFE_DROP_EVERY_8FRAME  0x01010101
#define VFE_DROP_EVERY_16FRAME 0x00010001
#define VFE_DROP_EVERY_32FRAME 0x00000001

enum msm_vfe_axi_stream_type {
	CONTINUOUS_STREAM,
	BURST_STREAM,
};

struct msm_vfe_axi_stream {
	uint32_t frame_id;
	enum msm_vfe_axi_state state;
	enum msm_vfe_axi_stream_src stream_src;
	uint8_t num_planes;
	uint8_t wm[MAX_PLANES_PER_STREAM];
	uint32_t plane_offset[MAX_PLANES_PER_STREAM];
	uint8_t comp_mask_index;
	struct msm_isp_buffer *buf[2];
	uint32_t session_id;
	uint32_t stream_id;
	uint32_t bufq_handle;
	uint32_t stream_handle;
	uint8_t buf_divert;
	enum msm_vfe_axi_stream_type stream_type;

	uint32_t frame_based;
	uint32_t framedrop_period;
	uint32_t framedrop_pattern;
	uint32_t num_burst_capture;/*number of frame to capture*/
	uint32_t init_frame_drop;
	uint32_t burst_frame_count;/*number of sof before burst stop*/
	uint8_t framedrop_update;

	/*Bandwidth calculation info*/
	uint32_t max_width;
	/*Based on format plane size in Q2. e.g NV12 = 1.5*/
	uint32_t format_factor;
	uint32_t bandwidth;

	/*Run time update variables*/
	uint32_t runtime_init_frame_drop;
	uint32_t runtime_burst_frame_count;/*number of sof before burst stop*/
	uint32_t runtime_num_burst_capture;
	uint8_t runtime_framedrop_update;
};

struct msm_vfe_axi_composite_info {
	uint32_t stream_handle;
	uint32_t stream_composite_mask;
};

struct msm_vfe_src_info {
	unsigned long frame_id;
	uint8_t active;
	uint8_t pix_stream_count;
	uint8_t raw_stream_count;
	enum msm_vfe_inputmux input_mux;
	uint32_t width;
	long pixel_clock;
};

enum msm_wm_ub_cfg_type {
	MSM_WM_UB_CFG_DEFAULT,
	MSM_WM_UB_EQUAL_SLICING,
	MSM_WM_UB_CFG_MAX_NUM
};

struct msm_vfe_axi_shared_data {
	struct msm_vfe_axi_hardware_info *hw_info;
	struct msm_vfe_axi_stream stream_info[MAX_NUM_STREAM];
	uint32_t free_wm[MAX_NUM_WM];
	uint32_t wm_image_size[MAX_NUM_WM];
	enum msm_wm_ub_cfg_type wm_ub_cfg_policy;
	uint8_t num_used_wm;
	uint8_t num_active_stream;
	struct msm_vfe_axi_composite_info
		composite_info[MAX_NUM_COMPOSITE_MASK];
	uint8_t num_used_composite_mask;
	uint32_t stream_update;
	struct msm_vfe_src_info src_info[VFE_SRC_MAX];
	uint16_t stream_handle_cnt;
	unsigned long event_mask;
};

struct msm_vfe_stats_hardware_info {
	uint32_t stats_capability_mask;
	uint32_t stats_ping_pong_offset;
	uint8_t num_stats_type;
	uint8_t num_stats_comp_mask;
};

enum msm_vfe_stats_state {
	STATS_AVALIABLE,
	STATS_INACTIVE,
	STATS_ACTIVE,
	STATS_START_PENDING,
	STATS_STOP_PENDING,
	STATS_STOPPING,
};

struct msm_vfe_stats_stream {
	uint32_t session_id;
	uint32_t stream_id;
	uint32_t stream_handle;
	enum msm_isp_stats_type stats_type;
	enum msm_vfe_stats_state state;
	uint32_t framedrop_pattern;
	uint32_t irq_subsample_pattern;

	uint32_t buffer_offset;
	struct msm_isp_buffer *buf[2];
	uint32_t bufq_handle;
};

struct msm_vfe_stats_shared_data {
	struct msm_vfe_stats_stream stream_info[MSM_ISP_STATS_MAX];
	enum msm_vfe_stats_pipeline_policy stats_pipeline_policy;
	uint32_t comp_framedrop_pattern;
	uint32_t comp_irq_subsample_pattern;
	uint8_t num_active_stream;
	uint16_t stream_handle_cnt;
};

struct msm_vfe_tasklet_queue_cmd {
	struct list_head list;
	uint32_t vfeInterruptStatus0;
	uint32_t vfeInterruptStatus1;
	struct msm_isp_timestamp ts;
	uint8_t cmd_used;
};

#define MSM_VFE_TASKLETQ_SIZE 200

struct msm_vfe_error_info {
	uint32_t error_mask0;
	uint32_t error_mask1;
	uint32_t violation_status;
	uint32_t camif_status;
	uint32_t stream_framedrop_count[MAX_NUM_STREAM];
	uint32_t stats_framedrop_count[MSM_ISP_STATS_MAX];
	uint32_t info_dump_frame_count;
	uint32_t error_count;
};

struct vfe_device {
	struct platform_device *pdev;
	struct msm_sd_subdev subdev;
	struct resource *vfe_irq;
	struct resource *vfe_mem;
	struct resource *vfe_vbif_mem;
	struct resource *vfe_io;
	struct resource *vfe_vbif_io;
	void __iomem *vfe_base;
	void __iomem *vfe_vbif_base;

	struct device *iommu_ctx[MAX_IOMMU_CTX];

	struct regulator *fs_vfe;
	struct clk *vfe_clk[7];

	uint32_t bus_perf_client;

	struct completion reset_complete;
	struct completion halt_complete;
	struct completion stream_config_complete;
	struct mutex realtime_mutex;
	struct mutex core_mutex;

	atomic_t irq_cnt;
	uint8_t taskletq_idx;
	spinlock_t  tasklet_lock;
	spinlock_t  shared_data_lock;
	struct list_head tasklet_q;
	struct tasklet_struct vfe_tasklet;
	struct msm_vfe_tasklet_queue_cmd
		tasklet_queue_cmd[MSM_VFE_TASKLETQ_SIZE];

	uint32_t vfe_hw_version;
	struct msm_vfe_hardware_info *hw_info;
	struct msm_vfe_axi_shared_data axi_data;
	struct msm_vfe_stats_shared_data stats_data;
	struct msm_vfe_error_info error_info;
	struct msm_isp_buf_mgr *buf_mgr;
	int dump_reg;
	uint32_t vfe_open_cnt;
};

#endif
