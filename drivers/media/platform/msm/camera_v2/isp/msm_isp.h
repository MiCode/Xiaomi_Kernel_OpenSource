/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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
#include <linux/avtimer_kernel.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_isp.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>

#include "msm_buf_mgr.h"

#define VFE40_8974V1_VERSION 0x10000018
#define VFE40_8974V2_VERSION 0x1001001A
#define VFE40_8974V3_VERSION 0x1001001B
#define VFE40_8x26_VERSION 0x20000013
#define VFE40_8x26V2_VERSION 0x20010014
#define VFE40_8916_VERSION 0x10030000
#define VFE40_8939_VERSION 0x10040000
#define VFE40_8952_VERSION 0x10060000
#define VFE40_8976_VERSION 0x10050000
#define VFE32_8909_VERSION 0x30600

#define MAX_IOMMU_CTX 2
#define MAX_NUM_WM 7
#define MAX_NUM_RDI 3
#define MAX_NUM_RDI_MASTER 3
#define MAX_NUM_COMPOSITE_MASK 4
#define MAX_NUM_STATS_COMP_MASK 2
#define MAX_INIT_FRAME_DROP 31
#define MAX_REG_UPDATE_THRESHOLD 10
#define ISP_Q2 (1 << 2)

#define VFE_PING_FLAG 0xFFFFFFFF
#define VFE_PONG_FLAG 0x0

#define VFE_MAX_CFG_TIMEOUT 3000
#define VFE_CLK_INFO_MAX 16
#define STATS_COMP_BIT_MASK 0x1FF

#define MSM_ISP_MIN_AB 100000000
#define MSM_ISP_MIN_IB 100000000

#define MAX_VFE 2

struct vfe_device;
struct msm_vfe_axi_stream;
struct msm_vfe_stats_stream;

#define VFE_SD_HW_MAX VFE_SD_COMMON

/* This struct is used to save/track SOF info for some INTF.
 * e.g. used in Master-Slave mode */
struct msm_vfe_sof_info {
	uint32_t timestamp_ms;
	uint32_t mono_timestamp_ms;
	uint32_t frame_id;
};

/* Each INTF in Master-Slave mode uses this struct. */
struct msm_vfe_dual_hw_ms_info {
	/* type is Master/Slave */
	enum msm_vfe_dual_hw_ms_type dual_hw_ms_type;
	/* sof_info is resource from common_data. If NULL, then this INTF
	 * sof does not need to be saved */
	struct msm_vfe_sof_info *sof_info;
	/* slave_id is index in common_data sof_info array for slaves */
	uint8_t slave_id;
};

struct vfe_subscribe_info {
	struct v4l2_fh *vfh;
	uint32_t active;
};

enum msm_isp_pack_fmt {
	QCOM,
	MIPI,
	DPCM6,
	DPCM8,
	PLAIN8,
	PLAIN16,
	DPCM10,
	MAX_ISP_PACK_FMT,
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
	/*Monotonic clock for VT */
	struct timeval vt_time;
	/*Wall clock for userspace event*/
	struct timeval event_time;
};

struct msm_vfe_irq_ops {
	void (*read_irq_status) (struct vfe_device *vfe_dev,
		uint32_t *irq_status0, uint32_t *irq_status1);
	void (*process_reg_update) (struct vfe_device *vfe_dev,
		uint32_t irq_status0, uint32_t irq_status1,
		struct msm_isp_timestamp *ts);
	void (*process_epoch_irq)(struct vfe_device *vfe_dev,
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
	void (*reload_wm)(struct vfe_device *vfe_dev, void __iomem *vfe_base,
		uint32_t reload_mask);
	void (*enable_wm) (struct vfe_device *vfe_dev,
		uint8_t wm_idx, uint8_t enable);
	int32_t (*cfg_io_format) (struct vfe_device *vfe_dev,
		enum msm_vfe_axi_stream_src stream_src,
		uint32_t io_format);
	void (*cfg_framedrop)(void __iomem *vfe_base,
		struct msm_vfe_axi_stream *stream_info,
		uint32_t framedrop_pattern, uint32_t framedrop_period);
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
		struct msm_vfe_axi_stream *stream_info,
		uint8_t plane_idx);
	void (*clear_wm_reg) (struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx);

	void (*cfg_wm_xbar_reg) (struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info,
		uint8_t plane_idx);
	void (*clear_wm_xbar_reg) (struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx);

	void (*cfg_ub) (struct vfe_device *vfe_dev);

	void (*read_wm_ping_pong_addr)(struct vfe_device *vfe_dev);

	void (*update_ping_pong_addr)(void __iomem *vfe_base,
		uint8_t wm_idx, uint32_t pingpong_bit, dma_addr_t paddr);

	uint32_t (*get_wm_mask) (uint32_t irq_status0, uint32_t irq_status1);
	uint32_t (*get_comp_mask) (uint32_t irq_status0, uint32_t irq_status1);
	uint32_t (*get_pingpong_status) (struct vfe_device *vfe_dev);
	int (*halt) (struct vfe_device *vfe_dev, uint32_t blocking);
	int (*restart) (struct vfe_device *vfe_dev, uint32_t blocking,
		uint32_t enable_camif);
	void (*update_cgc_override) (struct vfe_device *vfe_dev,
		uint8_t wm_idx, uint8_t cgc_override);
};

struct msm_vfe_core_ops {
	void (*reg_update)(struct vfe_device *vfe_dev,
		enum msm_vfe_input_src frame_src);
	long (*reset_hw) (struct vfe_device *vfe_dev, uint32_t first_start,
		uint32_t blocking_call);
	int (*init_hw) (struct vfe_device *vfe_dev);
	void (*init_hw_reg) (struct vfe_device *vfe_dev);
	void (*clear_status_reg) (struct vfe_device *vfe_dev);
	void (*release_hw) (struct vfe_device *vfe_dev);
	void (*cfg_input_mux) (struct vfe_device *vfe_dev,
		struct msm_vfe_pix_cfg *pix_cfg);
	int (*start_fetch_eng)(struct vfe_device *vfe_dev,
		void *arg);
	void (*update_camif_state) (struct vfe_device *vfe_dev,
		enum msm_isp_camif_update_state update_state);
	void (*cfg_rdi_reg) (struct vfe_device *vfe_dev,
		struct msm_vfe_rdi_cfg *rdi_cfg,
		enum msm_vfe_input_src input_src);
	int (*get_platform_data) (struct vfe_device *vfe_dev);
	void (*get_error_mask) (uint32_t *error_mask0, uint32_t *error_mask1);
	void (*process_error_status) (struct vfe_device *vfe_dev);
	void (*get_overflow_mask) (uint32_t *overflow_mask);
	void (*get_irq_mask) (struct vfe_device *vfe_dev,
		uint32_t *irq0_mask, uint32_t *irq1_mask);
	void (*restore_irq_mask) (struct vfe_device *vfe_dev);
	void (*get_halt_restart_mask) (uint32_t *irq0_mask,
		uint32_t *irq1_mask);
	void (*get_rdi_wm_mask)(struct vfe_device *vfe_dev,
		uint32_t *rdi_wm_mask);
	bool (*is_module_cfg_lock_needed)(uint32_t reg_offset);
};
struct msm_vfe_stats_ops {
	int (*get_stats_idx) (enum msm_isp_stats_type stats_type);
	int (*check_streams) (struct msm_vfe_stats_stream *stream_info);
	void (*cfg_framedrop) (struct vfe_device *vfe_dev,
		struct msm_vfe_stats_stream *stream_info,
		uint32_t framedrop_pattern, uint32_t framedrop_period);
	void (*clear_framedrop) (struct vfe_device *vfe_dev,
		struct msm_vfe_stats_stream *stream_info);
	void (*cfg_comp_mask)(struct vfe_device *vfe_dev,
		uint32_t stats_mask, uint8_t comp_index,
		uint8_t enable);
	void (*cfg_wm_irq_mask)(struct vfe_device *vfe_dev,
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

	void (*update_ping_pong_addr)(void __iomem *vfe_base,
		struct msm_vfe_stats_stream *stream_info,
		uint32_t pingpong_status, dma_addr_t paddr);

	uint32_t (*get_frame_id) (struct vfe_device *vfe_dev);
	uint32_t (*get_wm_mask) (uint32_t irq_status0, uint32_t irq_status1);
	uint32_t (*get_comp_mask) (uint32_t irq_status0, uint32_t irq_status1);
	uint32_t (*get_pingpong_status) (struct vfe_device *vfe_dev);

	void (*update_cgc_override) (struct vfe_device *vfe_dev,
		uint32_t stats_mask, uint8_t enable);
};

struct msm_vfe_ops {
	struct msm_vfe_irq_ops irq_ops;
	struct msm_vfe_axi_ops axi_ops;
	struct msm_vfe_core_ops core_ops;
	struct msm_vfe_stats_ops stats_ops;
};

struct msm_vfe_hardware_info {
	int num_iommu_ctx;
	/* secure iommu ctx nums */
	int num_iommu_secure_ctx;
	int vfe_clk_idx;
	struct msm_vfe_ops vfe_ops;
	struct msm_vfe_axi_hardware_info *axi_hw_info;
	struct msm_vfe_stats_hardware_info *stats_hw_info;
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
	AVAILABLE,
	INACTIVE,
	ACTIVE,
	PAUSED,
	START_PENDING,
	STOP_PENDING,
	PAUSE_PENDING,
	RESUME_PENDING,
	STARTING,
	STOPPING,
	PAUSING,
	RESUMING,
	UPDATING,
	STARTED,
	STOPPED,
};

enum msm_vfe_axi_cfg_update_state {
	NO_AXI_CFG_UPDATE,
	APPLYING_UPDATE_RESUME,
	UPDATE_REQUESTED,
};

#define VFE_NO_DROP	       0xFFFFFFFF
#define VFE_DROP_EVERY_2FRAME  0x55555555
#define VFE_DROP_EVERY_4FRAME  0x11111111
#define VFE_DROP_EVERY_8FRAME  0x01010101
#define VFE_DROP_EVERY_16FRAME 0x00010001
#define VFE_DROP_EVERY_32FRAME 0x00000001

enum msm_vfe_axi_stream_type {
	CONTINUOUS_STREAM,
	BURST_STREAM,
};

struct msm_vfe_frame_request_queue {
	struct list_head list;
	enum msm_vfe_buff_queue_id buff_queue_id;
	uint8_t cmd_used;
};

#define MSM_VFE_REQUESTQ_SIZE 8

struct msm_vfe_axi_stream {
	uint32_t frame_id;
	enum msm_vfe_axi_state state;
	enum msm_vfe_axi_stream_src stream_src;
	uint8_t num_planes;
	uint8_t wm[MAX_PLANES_PER_STREAM];
	uint32_t output_format;/*Planar/RAW/Misc*/
	struct msm_vfe_axi_plane_cfg plane_cfg[MAX_PLANES_PER_STREAM];
	uint8_t comp_mask_index;
	struct msm_isp_buffer *buf[2];
	uint32_t session_id;
	uint32_t stream_id;
	uint32_t bufq_handle[VFE_BUF_QUEUE_MAX];
	uint8_t controllable_output;
	uint8_t undelivered_request_cnt;
	uint8_t request_q_idx;
	uint32_t request_q_cnt;
	struct list_head request_q;
	struct msm_vfe_frame_request_queue
			request_queue_cmd[MSM_VFE_REQUESTQ_SIZE];
	uint32_t stream_handle;
	uint8_t buf_divert;
	enum msm_vfe_axi_stream_type stream_type;
	uint32_t frame_based;
	enum msm_vfe_frame_skip_pattern frame_skip_pattern;
	uint32_t framedrop_period;
	uint32_t framedrop_pattern;
	uint32_t prev_framedrop_period;
	uint32_t prev_framedrop_pattern;
	uint32_t framedrop_altern_cnt;
	uint32_t num_burst_capture;/*number of frame to capture*/
	uint32_t init_frame_drop;
	uint32_t burst_frame_count;/*number of sof before burst stop*/
	uint8_t framedrop_update;
	spinlock_t lock;

	/*Bandwidth calculation info*/
	uint32_t max_width;
	/*Based on format plane size in Q2. e.g NV12 = 1.5*/
	uint32_t format_factor;
	uint32_t bandwidth;

	/*Run time update variables*/
	uint32_t runtime_init_frame_drop;
	uint32_t runtime_burst_frame_count;/*number of sof before burst stop*/
	uint32_t runtime_num_burst_capture;
	uint8_t  runtime_framedrop_update;
	uint8_t  runtime_framedrop_update_burst;
	uint32_t runtime_output_format;
	enum msm_stream_memory_input_t  memory_input;
	struct msm_isp_sw_framskip sw_skip;
	uint8_t sw_ping_pong_bit;
};

struct msm_vfe_axi_composite_info {
	uint32_t stream_handle;
	uint32_t stream_composite_mask;
};

enum msm_vfe_camif_state {
	CAMIF_STOPPED,
	CAMIF_ENABLE,
	CAMIF_DISABLE,
	CAMIF_STOPPING,
};

struct msm_vfe_src_info {
	uint32_t frame_id;
	uint32_t reg_update_frame_id;
	uint8_t active;
	uint8_t pix_stream_count;
	uint8_t raw_stream_count;
	enum msm_vfe_inputmux input_mux;
	uint32_t width;
	long pixel_clock;
	uint32_t input_format;/*V4L2 pix format with bayer pattern*/
	uint32_t last_updt_frm_id;
	uint32_t sof_counter_step;
	uint64_t stats_ab;
	uint64_t stats_ib;
	enum msm_vfe_dual_hw_type dual_hw_type;
	struct msm_vfe_dual_hw_ms_info dual_hw_ms_info;
};

struct msm_vfe_fetch_engine_info {
	uint32_t session_id;
	uint32_t stream_id;
	uint32_t bufq_handle;
	uint32_t buf_idx;
	uint8_t is_busy;
	uint8_t offline_mode;
	uint32_t fd;
};

enum msm_wm_ub_cfg_type {
	MSM_WM_UB_CFG_DEFAULT,
	MSM_WM_UB_EQUAL_SLICING,
	MSM_WM_UB_CFG_MAX_NUM
};

struct msm_vfe_axi_shared_data {
	struct msm_vfe_axi_hardware_info *hw_info;
	struct msm_vfe_axi_stream stream_info[VFE_AXI_SRC_MAX];
	uint32_t free_wm[MAX_NUM_WM];
	uint32_t wm_image_size[MAX_NUM_WM];
	enum msm_wm_ub_cfg_type wm_ub_cfg_policy;
	uint8_t num_used_wm;
	uint8_t num_active_stream;
	uint8_t num_rdi_stream;
	uint8_t num_pix_stream;
	uint32_t rdi_wm_mask;
	struct msm_vfe_axi_composite_info
		composite_info[MAX_NUM_COMPOSITE_MASK];
	uint8_t num_used_composite_mask;
	uint32_t stream_update[VFE_SRC_MAX];
	atomic_t axi_cfg_update[VFE_SRC_MAX];
	enum msm_isp_camif_update_state pipeline_update;
	struct msm_vfe_src_info src_info[VFE_SRC_MAX];
	uint16_t stream_handle_cnt;
	uint32_t event_mask;
	uint8_t enable_frameid_recovery;
	enum msm_vfe_camif_state camif_state;
};

struct msm_vfe_stats_hardware_info {
	uint32_t stats_capability_mask;
	uint8_t *stats_ping_pong_offset;
	uint8_t num_stats_type;
	uint8_t num_stats_comp_mask;
};

enum msm_vfe_stats_state {
	STATS_AVALIABLE,
	STATS_INACTIVE,
	STATS_ACTIVE,
	STATS_START_PENDING,
	STATS_STOP_PENDING,
	STATS_STARTING,
	STATS_STOPPING,
};

struct msm_vfe_stats_stream {
	uint32_t session_id;
	uint32_t stream_id;
	uint32_t stream_handle;
	uint32_t composite_flag;
	enum msm_isp_stats_type stats_type;
	enum msm_vfe_stats_state state;
	uint32_t framedrop_pattern;
	uint32_t framedrop_period;
	uint32_t irq_subsample_pattern;
	uint32_t init_stats_frame_drop;
	struct msm_isp_sw_framskip sw_skip;

	uint32_t buffer_offset;
	struct msm_isp_buffer *buf[2];
	uint32_t bufq_handle;
};

struct msm_vfe_stats_shared_data {
	struct msm_vfe_stats_stream stream_info[MSM_ISP_STATS_MAX];
	uint8_t num_active_stream;
	atomic_t stats_comp_mask[MAX_NUM_STATS_COMP_MASK];
	uint16_t stream_handle_cnt;
	atomic_t stats_update;
};

struct msm_vfe_tasklet_queue_cmd {
	struct list_head list;
	uint32_t vfeInterruptStatus0;
	uint32_t vfeInterruptStatus1;
	struct msm_isp_timestamp ts;
	uint8_t cmd_used;
	uint8_t iommu_page_fault;
};

#define MSM_VFE_TASKLETQ_SIZE 200

enum msm_vfe_overflow_state {
	NO_OVERFLOW,
	OVERFLOW_DETECTED,
	HALT_REQUESTED,
	RESTART_REQUESTED,
	HALT_ENFORCED,
};

struct msm_vfe_error_info {
	atomic_t overflow_state;
	uint32_t overflow_recover_irq_mask0;
	uint32_t overflow_recover_irq_mask1;
	uint32_t error_mask0;
	uint32_t error_mask1;
	uint32_t violation_status;
	uint32_t camif_status;
	uint8_t stream_framedrop_count[BUF_MGR_NUM_BUF_Q];
	uint8_t stats_framedrop_count[MSM_ISP_STATS_MAX];
	uint32_t info_dump_frame_count;
	uint32_t error_count;
	uint32_t framedrop_flag;
};

struct msm_isp_statistics {
	int64_t imagemaster0_overflow;
	int64_t imagemaster1_overflow;
	int64_t imagemaster2_overflow;
	int64_t imagemaster3_overflow;
	int64_t imagemaster4_overflow;
	int64_t imagemaster5_overflow;
	int64_t imagemaster6_overflow;
	int64_t be_overflow;
	int64_t bg_overflow;
	int64_t bf_overflow;
	int64_t awb_overflow;
	int64_t rs_overflow;
	int64_t cs_overflow;
	int64_t ihist_overflow;
	int64_t skinbhist_overflow;
	int64_t bfscale_overflow;

	int64_t isp_vfe0_active;
	int64_t isp_vfe0_ab;
	int64_t isp_vfe0_ib;

	int64_t isp_vfe1_active;
	int64_t isp_vfe1_ab;
	int64_t isp_vfe1_ib;

	int64_t isp_cpp_active;
	int64_t isp_cpp_ab;
	int64_t isp_cpp_ib;

	int64_t last_overflow_ab;
	int64_t last_overflow_ib;

	int64_t vfe_clk_rate;
	int64_t cpp_clk_rate;
};

enum msm_isp_hw_client {
	ISP_VFE0,
	ISP_VFE1,
	ISP_CPP,
	MAX_ISP_CLIENT,
};

struct msm_isp_bandwidth_info {
	uint32_t active;
	uint64_t ab;
	uint64_t ib;
};

struct msm_isp_bw_req_info {
	uint32_t client;
	unsigned long long timestamp;
	uint64_t total_ab;
	uint64_t total_ib;
	struct msm_isp_bandwidth_info client_info[MAX_ISP_CLIENT];
};

#define MSM_ISP_MAX_WM 7
struct msm_isp_ub_info {
	enum msm_wm_ub_cfg_type policy;
	uint8_t num_wm;
	uint32_t wm_ub;
	uint32_t data[MSM_ISP_MAX_WM];
	uint64_t addr[MSM_ISP_MAX_WM];
};

struct msm_vfe_hw_init_parms {
	const char *entries;
	const char *regs;
	const char *settings;
};

struct master_slave_resource_info {
	enum msm_vfe_dual_hw_type dual_hw_type;
	struct msm_vfe_sof_info master_sof_info;
	uint8_t master_active;
	uint32_t sof_delta_threshold; /* Updated by Master */
	uint32_t num_slave;
	uint32_t reserved_slave_mask;
	uint32_t slave_active_mask;
	struct msm_vfe_sof_info slave_sof_info[MS_NUM_SLAVE_MAX];
};

struct msm_vfe_common_dev_data {
	spinlock_t common_dev_data_lock;
	spinlock_t common_dev_axi_lock;
	struct dual_vfe_resource *dual_vfe_res;
	struct master_slave_resource_info ms_resource;
};

struct msm_vfe_common_subdev {
	/* parent reference */
	struct vfe_parent_device *parent;

	/* Media Subdevice */
	struct msm_sd_subdev *subdev;

	/* Buf Mgr */
	struct msm_isp_buf_mgr *buf_mgr;

	/* Common Data */
	struct msm_vfe_common_dev_data *common_data;
};

struct dual_vfe_resource {
	struct vfe_device *vfe_dev[MAX_VFE];
	void __iomem *vfe_base[MAX_VFE];
	uint32_t reg_update_mask[MAX_VFE];
	struct msm_vfe_stats_shared_data *stats_data[MAX_VFE];
	struct msm_vfe_axi_shared_data *axi_data[MAX_VFE];
	uint32_t wm_reload_mask[MAX_VFE];
};

struct vfe_device {
	/* Driver private data */
	struct platform_device *pdev;
	struct msm_vfe_common_dev_data *common_data;
	struct msm_sd_subdev subdev;
	struct msm_isp_buf_mgr *buf_mgr;

	/* Resource info */
	struct resource *vfe_irq;
	struct resource *vfe_mem;
	struct resource *vfe_vbif_mem;
	struct resource *vfe_io;
	struct resource *vfe_vbif_io;
	void __iomem *vfe_base;
	void __iomem *vfe_vbif_base;
	struct device *iommu_ctx[MAX_IOMMU_CTX];
	struct regulator *fs_vfe;
	struct clk **vfe_clk;
	uint32_t num_clk;

	/* Sync variables*/
	struct completion reset_complete;
	struct completion halt_complete;
	struct completion stream_config_complete;
	struct completion stats_config_complete;
	struct mutex realtime_mutex;
	struct mutex core_mutex;
	struct mutex buf_mgr_mutex;
	spinlock_t shared_data_lock;
	spinlock_t reg_update_lock;
	spinlock_t tasklet_lock;

	/* Tasklet info */
	atomic_t irq_cnt;
	uint8_t taskletq_idx;
	struct list_head tasklet_q;
	struct tasklet_struct vfe_tasklet;
	struct msm_vfe_tasklet_queue_cmd
		tasklet_queue_cmd[MSM_VFE_TASKLETQ_SIZE];

	/* Data structures */
	struct msm_vfe_hardware_info *hw_info;
	struct msm_vfe_axi_shared_data axi_data;
	struct msm_vfe_stats_shared_data stats_data;
	struct msm_vfe_error_info error_info;
	struct msm_vfe_fetch_engine_info fetch_engine_info;
	enum msm_vfe_hvx_streaming_cmd hvx_cmd;

	/* State variables */
	uint32_t vfe_hw_version;
	int vfe_clk_idx;
	uint32_t vfe_open_cnt;
	uint8_t vt_enable;
	uint8_t ignore_error;
	uint32_t vfe_ub_policy;
	uint8_t reset_pending;
	uint8_t reg_update_requested;
	uint8_t reg_updated;
	uint32_t is_split;

	/* Debug variables */
	int dump_reg;
	struct msm_isp_statistics *stats;
	uint64_t msm_isp_last_overflow_ab;
	uint64_t msm_isp_last_overflow_ib;
	uint64_t msm_isp_vfe_clk_rate;
	struct msm_isp_ub_info *ub_info;
	uint32_t isp_sof_debug;
	uint32_t isp_raw0_debug;
	uint32_t isp_raw1_debug;
	uint32_t isp_raw2_debug;
};

struct vfe_parent_device {
	struct platform_device *pdev;
	uint32_t num_sd;
	uint32_t num_hw_sd;
	struct platform_device *child_list[VFE_SD_HW_MAX];
	struct msm_vfe_common_subdev *common_sd;
};

int vfe_hw_probe(struct platform_device *pdev);

#endif
