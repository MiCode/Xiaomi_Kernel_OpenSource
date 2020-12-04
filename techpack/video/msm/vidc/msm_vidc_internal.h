/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
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
#include "vidc_hfi_helper.h"

#define MSM_VIDC_DRV_NAME "msm_vidc_driver"

/* kernel/msm-4.19 */
#define MSM_VIDC_VERSION     ((0 << 16) + (4 << 8) + 19)

#define MAX_DEBUGFS_NAME 50
#define DEFAULT_TIMEOUT 3
#define DEFAULT_HEIGHT 240
#define DEFAULT_WIDTH 320
#define MIN_SUPPORTED_WIDTH 32
#define MIN_SUPPORTED_HEIGHT 32
#define DEFAULT_FPS 30
#define MINIMUM_FPS 1
#define MAXIMUM_FPS 960
#define SINGLE_INPUT_BUFFER 1
#define SINGLE_OUTPUT_BUFFER 1
#define MAX_NUM_INPUT_BUFFERS VIDEO_MAX_FRAME // same as VB2_MAX_FRAME
#define MAX_NUM_OUTPUT_BUFFERS VIDEO_MAX_FRAME // same as VB2_MAX_FRAME

#define MAX_SUPPORTED_INSTANCES 16

/* Maintains the number of FTB's between each FBD over a window */
#define DCVS_FTB_WINDOW 16
/* Superframe can have maximum of 32 frames */
#define VIDC_SUPERFRAME_MAX 32
#define COLOR_RANGE_UNSPECIFIED (-1)

#define V4L2_EVENT_VIDC_BASE  10
#define INPUT_MPLANE V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
#define OUTPUT_MPLANE V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE

#define RATE_CONTROL_OFF (V4L2_MPEG_VIDEO_BITRATE_MODE_CQ + 1)
#define RATE_CONTROL_LOSSLESS (V4L2_MPEG_VIDEO_BITRATE_MODE_CQ + 2)
#define SYS_MSG_START HAL_SYS_INIT_DONE
#define SYS_MSG_END HAL_SYS_ERROR
#define SESSION_MSG_START HAL_SESSION_EVENT_CHANGE
#define SESSION_MSG_END HAL_SESSION_ERROR
#define SYS_MSG_INDEX(__msg) (__msg - SYS_MSG_START)
#define SESSION_MSG_INDEX(__msg) (__msg - SESSION_MSG_START)

#define MAX_NAME_LENGTH 64

#define DB_DISABLE_SLICE_BOUNDARY \
	V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED_AT_SLICE_BOUNDARY

#define NUM_MBS_PER_SEC(__height, __width, __fps) \
	(NUM_MBS_PER_FRAME(__height, __width) * __fps)

#define NUM_MBS_PER_FRAME(__height, __width) \
	((ALIGN(__height, 16) / 16) * (ALIGN(__width, 16) / 16))

#define call_core_op(c, op, ...)			\
	(((c) && (c)->core_ops && (c)->core_ops->op) ? \
	((c)->core_ops->op(__VA_ARGS__)) : 0)

/*
 * Convert Q16 number into Integer and Fractional part upto 2 places.
 * Ex : 105752 / 65536 = 1.61; 1.61 in Q16 = 105752;
 * Integer part =  105752 / 65536 = 1;
 * Reminder = 105752 * 0xFFFF = 40216; Last 16 bits.
 * Fractional part = 40216 * 100 / 65536 = 61;
 * Now convert to FP(1, 61, 100).
 */
#define Q16_INT(q) ((q) >> 16)
#define Q16_FRAC(q) ((((q) & 0xFFFF) * 100) >> 16)

struct msm_vidc_inst;

enum vidc_ports {
	INPUT_PORT,
	OUTPUT_PORT,
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

struct recon_buf {
	struct list_head list;
	u32 buffer_index;
	u32 CR;
	u32 CF;
};

struct eos_buf {
	struct list_head list;
	struct msm_smem smem;
	u32 is_queued;
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
	u32 input_tag;
	u32 input_tag2;
};

struct msm_vidc_window_data {
	struct list_head list;
	u32 frame_size;
	u32 etb_count;
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

struct msm_vidc_codec_capability {
	enum hal_capability capability_type;
	enum hal_domain domains;
	enum hal_video_codec codecs;
	u32 min;
	u32 max;
	u32 step_size;
	u32 default_value;
};

struct msm_vidc_codec {
	enum hal_domain domain;
	enum hal_video_codec codec;
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

enum vpu_version {
	VPU_VERSION_AR50 = 1,
	VPU_VERSION_IRIS1,
	VPU_VERSION_IRIS2,
	VPU_VERSION_IRIS2_1,
	VPU_VERSION_AR50_LITE,
};

struct msm_vidc_ubwc_config_data {
	struct {
		u32 max_channel_override : 1;
		u32 mal_length_override : 1;
		u32 hb_override : 1;
		u32 bank_swzl_level_override : 1;
		u32 bank_spreading_override : 1;
		u32 reserved : 27;
	} override_bit_info;

	u32 max_channels;
	u32 mal_length;
	u32 highest_bank_bit;
	u32 bank_swzl_level;
	u32 bank_spreading;
};

struct msm_vidc_platform_data {
	struct msm_vidc_common_data *common_data;
	unsigned int common_data_length;
	struct msm_vidc_codec_data *codec_data;
	unsigned int codec_data_length;
	struct msm_vidc_codec *codecs;
	uint32_t codecs_count;
	struct msm_vidc_codec_capability *codec_caps;
	uint32_t codec_caps_count;
	struct msm_vidc_csc_coeff csc_data;
	struct msm_vidc_efuse_data *efuse_data;
	unsigned int efuse_data_length;
	unsigned int sku_version;
	uint32_t vpu_ver;
	uint32_t num_vpp_pipes;
	struct msm_vidc_ubwc_config_data *ubwc_config;
};

struct msm_vidc_format_desc {
	char name[MAX_NAME_LENGTH];
	u8 description[32];
	u32 fourcc;
};

struct msm_vidc_format {
	char name[MAX_NAME_LENGTH];
	u8 description[32];
	u32 count_min;
	u32 count_min_host;
	u32 count_actual;
	struct v4l2_format v4l2_fmt;
};

struct msm_vidc_format_constraint {
	u32 fourcc;
	u32 num_planes;
	u32 y_max_stride;
	u32 y_buffer_alignment;
	u32 uv_max_stride;
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

struct session_prop {
	u32 fps;
	u32 bitrate;
	bool bframe_changed;
	u32 extradata_ctrls;
};

struct buf_queue {
	struct vb2_queue vb2_bufq;
	struct mutex lock;
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
	int min_threshold;
	int nom_threshold;
	int max_threshold;
	bool dcvs_mode;
	u32 dcvs_window;
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
	u32 work_mode;
	bool low_latency_mode;
	bool is_legacy_cbr;
	u32 work_route;
	u32 dcvs_flags;
	u32 frame_rate;
};

struct vidc_bus_vote_data {
	u32 sid;
	enum hal_domain domain;
	enum hal_video_codec codec;
	enum hal_uncompressed_format color_formats[2];
	int num_formats; /* 1 = DPB-OPB unified; 2 = split */
	int input_height, input_width, bitrate;
	int output_height, output_width;
	int rotation;
	int compression_ratio;
	int complexity_factor;
	int input_cr;
	unsigned int lcu_size;
	unsigned int fps;
	enum msm_vidc_power_mode power_mode;
	u32 work_mode;
	bool use_sys_cache;
	bool b_frames_enabled;
	unsigned long calc_bw_ddr;
	unsigned long calc_bw_llcc;
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
};

struct msm_vidc_core_ops {
	unsigned long (*calc_freq)(struct msm_vidc_inst *inst, u32 filled_len);
	int (*decide_work_route)(struct msm_vidc_inst *inst);
	int (*decide_work_mode)(struct msm_vidc_inst *inst);
	int (*decide_core_and_power_mode)(struct msm_vidc_inst *inst);
	int (*calc_bw)(struct vidc_bus_vote_data *vidc_data);
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
	struct msm_vidc_capability *capabilities;
	struct delayed_work fw_unload_work;
	struct work_struct ssr_work;
	struct workqueue_struct *vidc_core_workq;
	enum hal_ssr_trigger_type ssr_type;
	bool smmu_fault_handled;
	bool trigger_ssr;
	unsigned long min_freq;
	unsigned long curr_freq;
	struct msm_vidc_core_ops *core_ops;
};

struct msm_vidc_inst;
struct msm_vidc_inst_smem_ops {
	int (*smem_map_dma_buf)(struct msm_vidc_inst *inst,
		struct msm_smem *smem);
	int (*smem_unmap_dma_buf)(struct msm_vidc_inst *inst,
		struct msm_smem *smem);
};

struct msm_vidc_inst {
	struct list_head list;
	struct mutex sync_lock, lock, flush_lock;
	struct msm_vidc_core *core;
	enum session_type session_type;
	void *session;
	u32 sid;
	struct session_prop prop;
	enum instance_state state;
	struct msm_vidc_format fmts[MAX_PORT_NUM];
	struct buf_queue bufq[MAX_PORT_NUM];
	struct msm_vidc_list input_crs;
	struct msm_vidc_list scratchbufs;
	struct msm_vidc_list persistbufs;
	struct msm_vidc_list pending_getpropq;
	struct msm_vidc_list outputbufs;
	struct msm_vidc_list refbufs;
	struct msm_vidc_list eosbufs;
	struct msm_vidc_list registeredbufs;
	struct msm_vidc_list cvpbufs;
	struct msm_vidc_list etb_data;
	struct msm_vidc_list fbd_data;
	struct msm_vidc_list window_data;
	struct msm_vidc_list client_data;
	struct buffer_requirements buff_req;
	struct vidc_frame_data superframe_data[VIDC_SUPERFRAME_MAX];
	struct v4l2_ctrl_handler ctrl_handler;
	struct completion completions[SESSION_MSG_END - SESSION_MSG_START + 1];
	struct v4l2_fh event_handler;
	struct msm_smem *extradata_handle;
	bool in_reconfig;
	struct dentry *debugfs_root;
	void *priv;
	struct msm_vidc_debug debug;
	struct buf_count count;
	struct clock_data clk_data;
	struct vidc_bus_vote_data bus_data;
	enum msm_vidc_modes flags;
	struct msm_vidc_capability capability;
	u32 buffer_size_limit;
	enum buffer_mode_type buffer_mode_set[MAX_PORT_NUM];
	enum multi_stream stream_output_mode;
	struct v4l2_ctrl **ctrls;
	u32 num_ctrls;
	int bit_depth;
	struct kref kref;
	bool in_flush;
	bool out_flush;
	u32 pic_struct;
	u32 colour_space;
	u32 profile;
	u32 level;
	u32 entropy_mode;
	u32 rc_type;
	u32 hybrid_hp;
	u32 layer_bitrate;
	u32 client_set_ctrls;
	bool static_rotation_flip_enabled;
	struct internal_buf *dpb_extra_binfo;
	struct msm_vidc_codec_data *codec_data;
	struct hal_hdr10_pq_sei hdr10_sei_params;
	struct batch_mode batch;
	struct delayed_work batch_work;
	struct msm_vidc_inst_smem_ops *smem_ops;
	int (*buffer_size_calculators)(struct msm_vidc_inst *inst);
	bool all_intra;
	bool is_perf_eligible_session;
	u32 max_filled_len;
	int full_range;
};

extern struct msm_vidc_drv *vidc_driver;

struct msm_vidc_ctrl {
	u32 id;
	char name[MAX_NAME_LENGTH];
	enum v4l2_ctrl_type type;
	s64 minimum;
	s64 maximum;
	s64 default_value;
	u32 step;
	u32 menu_skip_mask;
	u32 flags;
	const char * const *qmenu;
};

void handle_cmd_response(enum hal_command_response cmd, void *data);
int msm_vidc_trigger_ssr(struct msm_vidc_core *core,
	enum hal_ssr_trigger_type type);
int msm_vidc_noc_error_info(struct msm_vidc_core *core);
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
};

struct msm_vidc_cvp_buffer {
	struct list_head list;
	struct msm_smem smem;
	struct msm_cvp_buffer buf;
};

void msm_comm_handle_thermal_event(void);
int msm_smem_alloc(size_t size, u32 align, u32 flags,
	enum hal_buffer buffer_type, int map_kernel,
	void  *res, u32 session_type, struct msm_smem *smem, u32 sid);
int msm_smem_free(struct msm_smem *smem, u32 sid);

struct context_bank_info *msm_smem_get_context_bank(u32 session_type,
	bool is_secure, struct msm_vidc_platform_resources *res,
	enum hal_buffer buffer_type, u32 sid);
int msm_smem_map_dma_buf(struct msm_vidc_inst *inst, struct msm_smem *smem);
int msm_smem_unmap_dma_buf(struct msm_vidc_inst *inst, struct msm_smem *smem);
struct dma_buf *msm_smem_get_dma_buf(int fd, u32 sid);
void msm_smem_put_dma_buf(void *dma_buf, u32 sid);
int msm_smem_cache_operations(struct dma_buf *dbuf,
	enum smem_cache_ops cache_op, unsigned long offset,
	unsigned long size, u32 sid);
void msm_vidc_fw_unload_handler(struct work_struct *work);
void msm_vidc_ssr_handler(struct work_struct *work);
/*
 * XXX: normally should be in msm_vidc.h, but that's meant for public APIs,
 * whereas this is private
 */
int msm_vidc_destroy(struct msm_vidc_inst *inst);
void *vidc_get_drv_data(struct device *dev);
#endif
