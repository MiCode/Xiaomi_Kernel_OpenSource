/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_CVP_INTERNAL_H_
#define _MSM_CVP_INTERNAL_H_

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
#include "msm_cvp_core.h"
#include <media/msm_media_info.h>
#include "cvp_hfi_api.h"

#define MSM_CVP_DRV_NAME "msm_cvp_driver"
#define MSM_CVP_VERSION KERNEL_VERSION(0, 0, 1)
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

#define V4L2_EVENT_CVP_BASE  10

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

#define ARP_BUF_SIZE 0x100000

struct msm_cvp_inst;

enum cvp_ports {
	OUTPUT_PORT,
	CAPTURE_PORT,
	MAX_PORT_NUM
};

enum cvp_core_state {
	CVP_CORE_UNINIT = 0,
	CVP_CORE_INIT,
	CVP_CORE_INIT_DONE,
};

/*
 * Do not change the enum values unless
 * you know what you are doing
 */
enum instance_state {
	MSM_CVP_CORE_UNINIT_DONE = 0x0001,
	MSM_CVP_CORE_INIT,
	MSM_CVP_CORE_INIT_DONE,
	MSM_CVP_OPEN,
	MSM_CVP_OPEN_DONE,
	MSM_CVP_LOAD_RESOURCES,
	MSM_CVP_LOAD_RESOURCES_DONE,
	MSM_CVP_START,
	MSM_CVP_START_DONE,
	MSM_CVP_STOP,
	MSM_CVP_STOP_DONE,
	MSM_CVP_RELEASE_RESOURCES,
	MSM_CVP_RELEASE_RESOURCES_DONE,
	MSM_CVP_CLOSE,
	MSM_CVP_CLOSE_DONE,
	MSM_CVP_CORE_UNINIT,
	MSM_CVP_CORE_INVALID
};

struct buf_info {
	struct list_head list;
	struct vb2_buffer *buf;
};

struct msm_cvp_list {
	struct list_head list;
	struct mutex lock;
};

static inline void INIT_MSM_CVP_LIST(struct msm_cvp_list *mlist)
{
	mutex_init(&mlist->lock);
	INIT_LIST_HEAD(&mlist->list);
}

static inline void DEINIT_MSM_CVP_LIST(struct msm_cvp_list *mlist)
{
	mutex_destroy(&mlist->lock);
}

enum buffer_owner {
	DRIVER,
	FIRMWARE,
	CLIENT,
	MAX_OWNER
};

struct cvp_freq_data {
	struct list_head list;
	u32 device_addr;
	unsigned long freq;
	bool turbo;
};

struct internal_buf {
	struct list_head list;
	enum hal_buffer buffer_type;
	struct msm_smem smem;
	enum buffer_owner buffer_ownership;
	bool mark_remove;
};

struct msm_cvp_common_data {
	char key[128];
	int value;
};

enum sku_version {
	SKU_VERSION_0 = 0,
	SKU_VERSION_1,
	SKU_VERSION_2,
};

enum vpu_version {
	VPU_VERSION_4 = 1,
	VPU_VERSION_5,
};

struct msm_cvp_ubwc_config_data {
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

#define IS_VPU_4(ver) \
	(ver == VPU_VERSION_4)

#define IS_VPU_5(ver) \
	(ver == VPU_VERSION_5)

struct msm_cvp_platform_data {
	struct msm_cvp_common_data *common_data;
	unsigned int common_data_length;
	unsigned int sku_version;
	phys_addr_t gcc_register_base;
	uint32_t gcc_register_size;
	uint32_t vpu_ver;
	struct msm_cvp_ubwc_config_data *ubwc_config;
};

struct msm_cvp_format {
	char name[MAX_NAME_LENGTH];
	u8 description[32];
	u32 fourcc;
	int type;
	u32 (*get_frame_size)(int plane, u32 height, u32 width);
	bool defer_outputs;
	u32 input_min_count;
	u32 output_min_count;
};

struct msm_cvp_drv {
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
	MSM_CVP_DCVS_INCR = BIT(0),
	MSM_CVP_DCVS_DECR = BIT(1),
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
	struct msm_cvp_codec_data *entry;
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

struct msm_cvp_debug {
	struct profile_data pdata[MAX_PROFILING_POINTS];
	int profile;
	int samples;
};

enum msm_cvp_modes {
	CVP_SECURE = BIT(0),
	CVP_TURBO = BIT(1),
	CVP_THUMBNAIL = BIT(2),
	CVP_LOW_POWER = BIT(3),
	CVP_REALTIME = BIT(4),
};

struct msm_cvp_core_ops {
	unsigned long (*calc_freq)(struct msm_cvp_inst *inst, u32 filled_len);
	int (*decide_work_route)(struct msm_cvp_inst *inst);
	int (*decide_work_mode)(struct msm_cvp_inst *inst);
};

#define MAX_NUM_MSGS_PER_SESSION	128
#define CVP_MAX_WAIT_TIME	2000

struct session_msg {
	struct list_head node;
	struct hfi_msg_session_hdr pkt;
};

struct cvp_session_queue {
	spinlock_t lock;
	unsigned int msg_count;
	struct list_head msgs;
	wait_queue_head_t wq;
	struct kmem_cache *msg_cache;
};

struct msm_cvp_core {
	struct list_head list;
	struct mutex lock;
	int id;
	struct hfi_device *device;
	struct msm_cvp_platform_data *platform_data;
	struct msm_video_device vdev[MSM_CVP_MAX_DEVICES];
	struct v4l2_device v4l2_dev;
	struct list_head instances;
	struct dentry *debugfs_root;
	enum cvp_core_state state;
	struct completion completions[SYS_MSG_END - SYS_MSG_START + 1];
	enum msm_cvp_hfi_type hfi_type;
	struct msm_cvp_platform_resources resources;
	u32 enc_codec_supported;
	u32 dec_codec_supported;
	u32 codec_count;
	struct msm_cvp_capability *capabilities;
	struct delayed_work fw_unload_work;
	struct work_struct ssr_work;
	enum hal_ssr_trigger_type ssr_type;
	bool smmu_fault_handled;
	bool trigger_ssr;
	unsigned long min_freq;
	unsigned long curr_freq;
	struct msm_cvp_core_ops *core_ops;
};

struct msm_cvp_inst {
	struct list_head list;
	struct mutex sync_lock, lock, flush_lock;
	struct msm_cvp_core *core;
	enum session_type session_type;
	struct cvp_session_queue session_queue;
	void *session;
	struct session_prop prop;
	enum instance_state state;
	struct msm_cvp_format fmts[MAX_PORT_NUM];
	struct buf_queue bufq[MAX_PORT_NUM];
	struct msm_cvp_list freqs;
	struct msm_cvp_list persistbufs;
	struct msm_cvp_list registeredbufs;
	struct msm_cvp_list cvpbufs;
	struct buffer_requirements buff_req;
	struct v4l2_ctrl_handler ctrl_handler;
	struct completion completions[SESSION_MSG_END - SESSION_MSG_START + 1];
	struct v4l2_fh event_handler;
	struct msm_smem *extradata_handle;
	struct dentry *debugfs_root;
	void *priv;
	struct msm_cvp_debug debug;
	struct buf_count count;
	struct clock_data clk_data;
	enum msm_cvp_modes flags;
	struct msm_cvp_capability capability;
	u32 buffer_size_limit;
	enum buffer_mode_type buffer_mode_set[MAX_PORT_NUM];
	enum multi_stream stream_output_mode;
	struct v4l2_ctrl **ctrls;
	struct kref kref;
	struct msm_cvp_codec_data *codec_data;
	struct batch_mode batch;
};

extern struct msm_cvp_drv *cvp_driver;

void cvp_handle_cmd_response(enum hal_command_response cmd, void *data);
int msm_cvp_trigger_ssr(struct msm_cvp_core *core,
	enum hal_ssr_trigger_type type);
int msm_cvp_noc_error_info(struct msm_cvp_core *core);
void msm_cvp_queue_v4l2_event(struct msm_cvp_inst *inst, int event_type);

enum msm_cvp_flags {
	MSM_CVP_FLAG_DEFERRED            = BIT(0),
	MSM_CVP_FLAG_RBR_PENDING         = BIT(1),
	MSM_CVP_FLAG_QUEUED              = BIT(2),
};

struct msm_video_buffer {
	struct list_head list;
	struct kref kref;
	struct msm_smem smem[VIDEO_MAX_PLANES];
	struct vb2_v4l2_buffer vvb;
	enum msm_cvp_flags flags;
};

struct msm_cvp_internal_buffer {
	struct list_head list;
	struct msm_smem smem;
	struct cvp_kmd_buffer buf;
};

void msm_cvp_comm_handle_thermal_event(void);
int msm_cvp_smem_alloc(size_t size, u32 align, u32 flags,
	enum hal_buffer buffer_type, int map_kernel,
	void  *res, u32 session_type, struct msm_smem *smem);
int msm_cvp_smem_free(struct msm_smem *smem);

struct context_bank_info *msm_cvp_smem_get_context_bank(u32 session_type,
	bool is_secure, struct msm_cvp_platform_resources *res,
	enum hal_buffer buffer_type);
int msm_cvp_smem_map_dma_buf(struct msm_cvp_inst *inst, struct msm_smem *smem);
int msm_cvp_smem_unmap_dma_buf(struct msm_cvp_inst *inst,
	struct msm_smem *smem);
struct dma_buf *msm_cvp_smem_get_dma_buf(int fd);
void msm_cvp_smem_put_dma_buf(void *dma_buf);
int msm_cvp_smem_cache_operations(struct dma_buf *dbuf,
	enum smem_cache_ops cache_op, unsigned long offset, unsigned long size);
void msm_cvp_fw_unload_handler(struct work_struct *work);
void msm_cvp_ssr_handler(struct work_struct *work);
/*
 * XXX: normally should be in msm_cvp_core.h, but that's meant for public APIs,
 * whereas this is private
 */
int msm_cvp_destroy(struct msm_cvp_inst *inst);
void *cvp_get_drv_data(struct device *dev);
#endif
