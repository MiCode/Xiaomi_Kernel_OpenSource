/* Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_VIDC_INTERNAL_H_
#define _MSM_VIDC_INTERNAL_H_

#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/kref.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>
#include "msm_vidc.h"
#include <media/msm_media_info.h>
#include "vidc_hfi_api.h"
#include <linux/timer.h>

#define MSM_VIDC_DRV_NAME "msm_vidc_driver"
#define MSM_VIDC_VERSION KERNEL_VERSION(0, 0, 1)
#define MAX_DEBUGFS_NAME 50
#define DEFAULT_TIMEOUT 3
#define DEFAULT_HEIGHT 1088
#define DEFAULT_WIDTH 1920
#define MIN_SUPPORTED_WIDTH 32
#define MIN_SUPPORTED_HEIGHT 32
#define DEFAULT_FPS 15
#define MIN_NUM_OUTPUT_BUFFERS 1
#define MIN_NUM_OUTPUT_BUFFERS_VP9 6
#define MIN_NUM_CAPTURE_BUFFERS 1
#define MAX_NUM_OUTPUT_BUFFERS VIDEO_MAX_FRAME // same as VB2_MAX_FRAME
#define MAX_NUM_CAPTURE_BUFFERS VIDEO_MAX_FRAME // same as VB2_MAX_FRAME

#define MAX_SUPPORTED_INSTANCES 16

/* Maintains the number of FTB's between each FBD over a window */
#define DCVS_FTB_WINDOW 16

#define V4L2_EVENT_VIDC_BASE  10

#define SYS_MSG_START HAL_SYS_INIT_DONE
#define SYS_MSG_END HAL_SYS_ERROR
#define SESSION_MSG_START HAL_SESSION_EVENT_CHANGE
#define SESSION_MSG_END HAL_SESSION_ERROR
#define SYS_MSG_INDEX(__msg) (__msg - SYS_MSG_START)
#define SESSION_MSG_INDEX(__msg) (__msg - SESSION_MSG_START)


#define MAX_NAME_LENGTH 64

#define EXTRADATA_IDX(__num_planes) ((__num_planes) ? (__num_planes) - 1 : 0)

#define NUM_MBS_PER_SEC(__height, __width, __fps) \
	(NUM_MBS_PER_FRAME(__height, __width) * __fps)

#define NUM_MBS_PER_FRAME(__height, __width) \
	((ALIGN(__height, 16) / 16) * (ALIGN(__width, 16) / 16))

#define call_core_op(c, op, args...)			\
	(((c) && (c)->core_ops && (c)->core_ops->op) ? \
	((c)->core_ops->op(args)) : 0)

struct msm_vidc_inst;

enum vidc_ports {
	OUTPUT_PORT,
	CAPTURE_PORT,
	MAX_PORT_NUM
};

enum vidc_core_state {
	VIDC_CORE_UNINIT = 0,
	VIDC_CORE_INIT,
	VIDC_CORE_INIT_DONE,
};

/*
 * Do not change the enum values unless
 * you know what you are doing
 */
enum instance_state {
	MSM_VIDC_CORE_UNINIT_DONE = 0x0001,
	MSM_VIDC_CORE_INIT,
	MSM_VIDC_CORE_INIT_DONE,
	MSM_VIDC_OPEN,
	MSM_VIDC_OPEN_DONE,
	MSM_VIDC_LOAD_RESOURCES,
	MSM_VIDC_LOAD_RESOURCES_DONE,
	MSM_VIDC_START,
	MSM_VIDC_START_DONE,
	MSM_VIDC_STOP,
	MSM_VIDC_STOP_DONE,
	MSM_VIDC_RELEASE_RESOURCES,
	MSM_VIDC_RELEASE_RESOURCES_DONE,
	MSM_VIDC_CLOSE,
	MSM_VIDC_CLOSE_DONE,
	MSM_VIDC_CORE_UNINIT,
	MSM_VIDC_CORE_INVALID
};

struct buf_info {
	struct list_head list;
	struct vb2_buffer *buf;
};

struct msm_vidc_list {
	struct list_head list;
	struct mutex lock;
};

static inline void INIT_MSM_VIDC_LIST(struct msm_vidc_list *mlist)
{
	mutex_init(&mlist->lock);
	INIT_LIST_HEAD(&mlist->list);
}

static inline void DEINIT_MSM_VIDC_LIST(struct msm_vidc_list *mlist)
{
	mutex_destroy(&mlist->lock);
}

enum buffer_owner {
	DRIVER,
	FIRMWARE,
	CLIENT,
	MAX_OWNER
};

struct vidc_freq_data {
	struct list_head list;
	u32 device_addr;
	unsigned long freq;
	bool turbo;
};

struct vidc_input_cr_data {
	struct list_head list;
	u32 index;
	u32 input_cr;
};

struct vidc_tag_data {
	struct list_head list;
	u32 index;
	u32 type;
	u32 input_tag;
	u32 output_tag;
};

struct recon_buf {
	struct list_head list;
	u32 buffer_index;
	u32 CR;
	u32 CF;
};

struct eos_buf {
	struct list_head list;
	struct msm_smem smem;
};

struct internal_buf {
	struct list_head list;
	enum hal_buffer buffer_type;
	struct msm_smem smem;
	enum buffer_owner buffer_ownership;
	bool mark_remove;
};

struct msm_vidc_csc_coeff {
	u32 *vpe_csc_custom_matrix_coeff;
	u32 *vpe_csc_custom_bias_coeff;
	u32 *vpe_csc_custom_limit_coeff;
};

struct msm_vidc_buf_data {
	struct list_head list;
	u32 index;
	u32 mark_data;
	u32 mark_target;
};

struct msm_vidc_common_data {
	char key[128];
	int value;
};

struct msm_vidc_codec_data {
	u32 fourcc;
	enum session_type session_type;
	int vpp_cycles;
	int vsp_cycles;
	int low_power_cycles;
};

enum efuse_purpose {
	SKU_VERSION = 0,
};

enum sku_version {
	SKU_VERSION_0 = 0,
	SKU_VERSION_1,
	SKU_VERSION_2,
};

struct msm_vidc_efuse_data {
	u32 start_address;
	u32 size;
	u32 mask;
	u32 shift;
	enum efuse_purpose purpose;
};

struct msm_vidc_capability_range {
	u32 min;
	u32 max;
};

struct msm_vidc_image_capability {
	struct msm_vidc_capability_range width;
	struct msm_vidc_capability_range height;
};

enum vpu_version {
	VPU_VERSION_4 = 1,
	VPU_VERSION_5,
};

#define IS_VPU_4(ver) \
	(ver == VPU_VERSION_4)

#define IS_VPU_5(ver) \
	(ver == VPU_VERSION_5)

struct msm_vidc_platform_data {
	struct msm_vidc_common_data *common_data;
	unsigned int common_data_length;
	struct msm_vidc_codec_data *codec_data;
	unsigned int codec_data_length;
	struct msm_vidc_csc_coeff csc_data;
	struct msm_vidc_efuse_data *efuse_data;
	unsigned int efuse_data_length;
	struct msm_vidc_ubwc_config *ubwc_config;
	unsigned int ubwc_config_length;
	struct msm_vidc_image_capability *heic_image_capability;
	struct msm_vidc_image_capability *hevc_image_capability;
	unsigned int sku_version;
	uint32_t vpu_ver;
};

struct msm_vidc_format {
	char name[MAX_NAME_LENGTH];
	u8 description[32];
	u32 fourcc;
	int type;
	u32 (*get_frame_size)(int plane, u32 height, u32 width);
	bool defer_outputs;
	u32 input_min_count;
	u32 output_min_count;
};

struct msm_vidc_format_constraint {
	u32 fourcc;
	u32 num_planes;
	u32 y_stride_multiples;
	u32 y_max_stride;
	u32 y_min_plane_buffer_height_multiple;
	u32 y_buffer_alignment;
	u32 uv_stride_multiples;
	u32 uv_max_stride;
	u32 uv_min_plane_buffer_height_multiple;
	u32 uv_buffer_alignment;
};

struct msm_vidc_drv {
	struct mutex lock;
	struct list_head cores;
	int num_cores;
	struct dentry *debugfs_root;
	int thermal_level;
	u32 sku_version;
};

struct msm_video_device {
	int type;
	struct video_device vdev;
};

struct session_crop {
	u32 left;
	u32 top;
	u32 width;
	u32 height;
};

struct session_prop {
	u32 width[MAX_PORT_NUM];
	u32 height[MAX_PORT_NUM];
	struct session_crop crop_info;
	u32 fps;
	u32 bitrate;
};

struct buf_queue {
	struct vb2_queue vb2_bufq;
	struct mutex lock;
	unsigned int plane_sizes[VB2_MAX_PLANES];
	int num_planes;
};

enum profiling_points {
	SYS_INIT = 0,
	SESSION_INIT,
	LOAD_RESOURCES,
	FRAME_PROCESSING,
	FW_IDLE,
	MAX_PROFILING_POINTS,
};

struct buf_count {
	int etb;
	int ftb;
	int fbd;
	int ebd;
};

struct batch_mode {
	bool enable;
	u32 size;
};

enum dcvs_flags {
	MSM_VIDC_DCVS_INCR = BIT(0),
	MSM_VIDC_DCVS_DECR = BIT(1),
};

struct clock_data {
	int buffer_counter;
	int load;
	int load_low;
	int load_norm;
	int load_high;
	int min_threshold;
	int max_threshold;
	enum hal_buffer buffer_type;
	bool dcvs_mode;
	unsigned long bitrate;
	unsigned long min_freq;
	unsigned long curr_freq;
	u32 vpss_cycles;
	u32 ise_cycles;
	u32 ddr_bw;
	u32 sys_cache_bw;
	u32 operating_rate;
	struct msm_vidc_codec_data *entry;
	u32 core_id;
	u32 dpb_fourcc;
	u32 opb_fourcc;
	enum hal_work_mode work_mode;
	bool low_latency_mode;
	bool turbo_mode;
	u32 work_route;
	u32 dcvs_flags;
};

struct profile_data {
	int start;
	int stop;
	int cumulative;
	char name[64];
	int sampling;
	int average;
};

struct msm_vidc_debug {
	struct profile_data pdata[MAX_PROFILING_POINTS];
	int profile;
	int samples;
};

enum msm_vidc_modes {
	VIDC_SECURE = BIT(0),
	VIDC_TURBO = BIT(1),
	VIDC_THUMBNAIL = BIT(2),
	VIDC_LOW_POWER = BIT(3),
	VIDC_REALTIME = BIT(4),
};

struct msm_vidc_core_ops {
	unsigned long (*calc_freq)(struct msm_vidc_inst *inst, u32 filled_len);
	int (*decide_work_route)(struct msm_vidc_inst *inst);
	int (*decide_work_mode)(struct msm_vidc_inst *inst);
};

struct msm_vidc_core {
	struct list_head list;
	struct mutex lock;
	int id;
	struct hfi_device *device;
	struct msm_vidc_platform_data *platform_data;
	struct msm_video_device vdev[MSM_VIDC_MAX_DEVICES];
	struct v4l2_device v4l2_dev;
	struct list_head instances;
	struct dentry *debugfs_root;
	enum vidc_core_state state;
	struct completion completions[SYS_MSG_END - SYS_MSG_START + 1];
	enum msm_vidc_hfi_type hfi_type;
	struct msm_vidc_platform_resources resources;
	u32 enc_codec_supported;
	u32 dec_codec_supported;
	u32 codec_count;
	struct msm_vidc_capability *capabilities;
	struct delayed_work fw_unload_work;
	struct work_struct ssr_work;
	enum hal_ssr_trigger_type ssr_type;
	bool smmu_fault_handled;
	bool trigger_ssr;
	unsigned long min_freq;
	unsigned long curr_freq;
	struct msm_vidc_core_ops *core_ops;
};

struct msm_vidc_inst {
	struct list_head list;
	struct mutex sync_lock, lock;
	struct msm_vidc_core *core;
	enum session_type session_type;
	void *session;
	struct session_prop prop;
	enum instance_state state;
	struct msm_vidc_format fmts[MAX_PORT_NUM];
	struct buf_queue bufq[MAX_PORT_NUM];
	struct msm_vidc_list freqs;
	struct msm_vidc_list input_crs;
	struct msm_vidc_list buffer_tags;
	struct msm_vidc_list scratchbufs;
	struct msm_vidc_list persistbufs;
	struct msm_vidc_list pending_getpropq;
	struct msm_vidc_list outputbufs;
	struct msm_vidc_list reconbufs;
	struct msm_vidc_list eosbufs;
	struct msm_vidc_list registeredbufs;
	struct msm_vidc_list cvpbufs;
	struct msm_vidc_list etb_data;
	struct msm_vidc_list fbd_data;
	struct buffer_requirements buff_req;
	struct v4l2_ctrl_handler ctrl_handler;
	struct completion completions[SESSION_MSG_END - SESSION_MSG_START + 1];
	struct v4l2_ctrl **cluster;
	struct v4l2_fh event_handler;
	struct msm_smem *extradata_handle;
	bool in_reconfig;
	u32 reconfig_width;
	u32 reconfig_height;
	struct dentry *debugfs_root;
	void *priv;
	struct msm_vidc_debug debug;
	struct buf_count count;
	struct clock_data clk_data;
	enum msm_vidc_modes flags;
	struct msm_vidc_capability capability;
	u32 buffer_size_limit;
	enum buffer_mode_type buffer_mode_set[MAX_PORT_NUM];
	enum multi_stream stream_output_mode;
	struct v4l2_ctrl **ctrls;
	int bit_depth;
	struct kref kref;
	bool in_flush;
	u32 pic_struct;
	u32 colour_space;
	u32 profile;
	u32 level;
	u32 entropy_mode;
	u32 grid_enable;
	u32 frame_quality;
	struct msm_vidc_codec_data *codec_data;
	struct hal_hdr10_pq_sei hdr10_sei_params;
	struct batch_mode batch;
	struct timer_list batch_timer;
	struct work_struct batch_work;
	bool decode_batching;
	u32 max_filled_length;
};

extern struct msm_vidc_drv *vidc_driver;

struct msm_vidc_ctrl_cluster {
	struct v4l2_ctrl **cluster;
	struct list_head list;
};

struct msm_vidc_ctrl {
	u32 id;
	char name[MAX_NAME_LENGTH];
	enum v4l2_ctrl_type type;
	s64 minimum;
	s64 maximum;
	s64 default_value;
	u32 step;
	u64 menu_skip_mask;
	u32 flags;
	const char * const *qmenu;
};

void handle_cmd_response(enum hal_command_response cmd, void *data);
int msm_vidc_trigger_ssr(struct msm_vidc_core *core,
	enum hal_ssr_trigger_type type);
int msm_vidc_freeze_core(struct msm_vidc_core *core);
int msm_vidc_noc_error_info(struct msm_vidc_core *core);
bool heic_encode_session_supported(struct msm_vidc_inst *inst);
int msm_vidc_check_session_supported(struct msm_vidc_inst *inst);
int msm_vidc_check_scaling_supported(struct msm_vidc_inst *inst);
void msm_vidc_queue_v4l2_event(struct msm_vidc_inst *inst, int event_type);

enum msm_vidc_flags {
	MSM_VIDC_FLAG_DEFERRED            = BIT(0),
	MSM_VIDC_FLAG_RBR_PENDING         = BIT(1),
	MSM_VIDC_FLAG_QUEUED              = BIT(2),
};

struct msm_vidc_buffer {
	struct list_head list;
	struct kref kref;
	struct msm_smem smem[VIDEO_MAX_PLANES];
	struct vb2_v4l2_buffer vvb;
	enum msm_vidc_flags flags;
	u32 output_tag;
};

struct msm_vidc_cvp_buffer {
	struct list_head list;
	struct msm_smem smem;
	struct msm_cvp_buffer buf;
};

void msm_comm_handle_thermal_event(void);
int msm_smem_alloc(size_t size, u32 align, u32 flags,
	enum hal_buffer buffer_type, int map_kernel,
	void  *res, u32 session_type, struct msm_smem *smem);
int msm_smem_free(struct msm_smem *smem);

struct context_bank_info *msm_smem_get_context_bank(u32 session_type,
	bool is_secure, struct msm_vidc_platform_resources *res,
	enum hal_buffer buffer_type);
int msm_smem_map_dma_buf(struct msm_vidc_inst *inst, struct msm_smem *smem);
int msm_smem_unmap_dma_buf(struct msm_vidc_inst *inst, struct msm_smem *smem);
struct dma_buf *msm_smem_get_dma_buf(int fd);
void msm_smem_put_dma_buf(void *dma_buf);
int msm_smem_cache_operations(struct dma_buf *dbuf,
	enum smem_cache_ops cache_op, unsigned long offset, unsigned long size);
void msm_vidc_fw_unload_handler(struct work_struct *work);
void msm_vidc_ssr_handler(struct work_struct *work);
/*
 * XXX: normally should be in msm_vidc.h, but that's meant for public APIs,
 * whereas this is private
 */
int msm_vidc_destroy(struct msm_vidc_inst *inst);
void *vidc_get_drv_data(struct device *dev);
#endif
