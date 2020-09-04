/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
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
#include <linux/interconnect.h>
#include <linux/kref.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/dma-mapping.h>
#include "msm_cvp_core.h"
#include <media/msm_media_info.h>
#include <media/msm_cvp_private.h>
#include "cvp_hfi_api.h"
#include "cvp_hfi_helper.h"
#include <synx_api.h>

#define MAX_SUPPORTED_INSTANCES 16
#define MAX_NAME_LENGTH 64
#define MAX_DEBUGFS_NAME 50
#define MAX_DSP_INIT_ATTEMPTS 16
#define FENCE_WAIT_SIGNAL_TIMEOUT 100
#define FENCE_WAIT_SIGNAL_RETRY_TIMES 20
#define FENCE_BIT (1ULL << 63)

#define FENCE_DME_ICA_ENABLED_IDX 0
#define FENCE_DME_DS_IDX 1
#define FENCE_DME_OUTPUT_IDX 7

#define SYS_MSG_START HAL_SYS_INIT_DONE
#define SYS_MSG_END HAL_SYS_ERROR
#define SESSION_MSG_START HAL_SESSION_EVENT_CHANGE
#define SESSION_MSG_END HAL_SESSION_ERROR
#define SYS_MSG_INDEX(__msg) (__msg - SYS_MSG_START)
#define SESSION_MSG_INDEX(__msg) (__msg - SESSION_MSG_START)

#define ARP_BUF_SIZE 0x100000

/* Adjust this value to admit non-realtime session */
#define CVP_RT_PRIO_THRESHOLD 0

struct msm_cvp_inst;

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
	MSM_CVP_CLOSE,
	MSM_CVP_CLOSE_DONE,
	MSM_CVP_CORE_UNINIT,
	MSM_CVP_CORE_INVALID
};

enum dsp_state {
	DSP_INVALID,
	DSP_UNINIT,
	DSP_PROBED,
	DSP_READY,
	DSP_SUSPEND,
	DSP_INACTIVE,
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

struct msm_cvp_platform_data {
	struct msm_cvp_common_data *common_data;
	unsigned int common_data_length;
	unsigned int sku_version;
	uint32_t vpu_ver;
	struct msm_cvp_ubwc_config_data *ubwc_config;
};

struct msm_cvp_drv {
	struct mutex lock;
	struct list_head cores;
	int num_cores;
	struct dentry *debugfs_root;
	int thermal_level;
	u32 sku_version;
	struct kmem_cache *msg_cache;
	struct kmem_cache *frame_cache;
	struct kmem_cache *buf_cache;
	struct kmem_cache *smem_cache;
	char fw_version[CVP_VERSION_LENGTH];
};

enum profiling_points {
	SYS_INIT = 0,
	SESSION_INIT,
	LOAD_RESOURCES,
	FRAME_PROCESSING,
	FW_IDLE,
	MAX_PROFILING_POINTS,
};

struct cvp_clock_data {
	int buffer_counter;
	int load;
	int load_low;
	int load_norm;
	int load_high;
	int min_threshold;
	int max_threshold;
	unsigned long bitrate;
	unsigned long min_freq;
	unsigned long curr_freq;
	u32 ddr_bw;
	u32 sys_cache_bw;
	u32 operating_rate;
	u32 core_id;
	bool low_latency_mode;
	bool turbo_mode;
};

struct cvp_profile_data {
	int start;
	int stop;
	int cumulative;
	char name[64];
	int sampling;
	int average;
};

struct msm_cvp_debug {
	struct cvp_profile_data pdata[MAX_PROFILING_POINTS];
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

#define MAX_NUM_MSGS_PER_SESSION	128
#define CVP_MAX_WAIT_TIME	2000

struct cvp_session_msg {
	struct list_head node;
	struct cvp_hfi_msg_session_hdr_ext pkt;
};

struct cvp_session_queue {
	spinlock_t lock;
	enum queue_state state;
	unsigned int msg_count;
	struct list_head msgs;
	wait_queue_head_t wq;
};

#define CVP_CYCLE_STAT_SIZE		8
struct cvp_cycle_stat {
	u32 busy[CVP_CYCLE_STAT_SIZE];
	u32 total;
	u32 idx;
	u32 size;
};

struct cvp_cycle_info {
	u32 sum_fps[HFI_MAX_HW_THREADS];
	u32 hi_ctrl_lim[HFI_MAX_HW_THREADS];
	u32 lo_ctrl_lim[HFI_MAX_HW_THREADS];
	struct cvp_cycle_stat cycle[HFI_MAX_HW_THREADS];
	unsigned long conf_freq;
};

struct cvp_session_prop {
	u32 type;
	u32 kernel_mask;
	u32 priority;
	u32 is_secure;
	u32 dsp_mask;
	u32 fthread_nr;
	u32 fdu_cycles;
	u32 od_cycles;
	u32 mpu_cycles;
	u32 ica_cycles;
	u32 fw_cycles;
	u32 fdu_op_cycles;
	u32 od_op_cycles;
	u32 mpu_op_cycles;
	u32 ica_op_cycles;
	u32 fw_op_cycles;
	u32 ddr_bw;
	u32 ddr_op_bw;
	u32 ddr_cache;
	u32 ddr_op_cache;
	u32 fps[HFI_MAX_HW_THREADS];
};

enum cvp_event_t {
	CVP_NO_EVENT,
	CVP_SSR_EVENT = 1,
	CVP_SYS_ERROR_EVENT,
	CVP_MAX_CLIENTS_EVENT,
	CVP_HW_UNSUPPORTED_EVENT,
	CVP_INVALID_EVENT,
};

struct cvp_session_event {
	spinlock_t lock;
	enum cvp_event_t event;
	wait_queue_head_t wq;
};

struct msm_cvp_core {
	struct list_head list;
	struct mutex lock;
	struct mutex clk_lock;
	int id;
	dev_t dev_num;
	struct cdev cdev;
	struct class *class;
	struct device *dev;
	struct cvp_hfi_device *device;
	struct msm_cvp_platform_data *platform_data;
	struct list_head instances;
	struct dentry *debugfs_root;
	enum cvp_core_state state;
	struct completion completions[SYS_MSG_END - SYS_MSG_START + 1];
	enum msm_cvp_hfi_type hfi_type;
	struct msm_cvp_platform_resources resources;
	struct msm_cvp_capability *capabilities;
	struct delayed_work fw_unload_work;
	struct work_struct ssr_work;
	enum hal_ssr_trigger_type ssr_type;
	bool smmu_fault_handled;
	u32 last_fault_addr;
	bool trigger_ssr;
	unsigned long curr_freq;
	struct cvp_cycle_info dyn_clk;
	atomic64_t kernel_trans_id;
};

struct msm_cvp_inst {
	struct list_head list;
	struct mutex sync_lock, lock;
	struct msm_cvp_core *core;
	enum session_type session_type;
	struct cvp_session_queue session_queue;
	struct cvp_session_queue session_queue_fence;
	struct cvp_session_event event_handler;
	void *session;
	enum instance_state state;
	struct msm_cvp_list freqs;
	struct msm_cvp_list persistbufs;
	struct cvp_dmamap_cache dma_cache;
	struct msm_cvp_list cvpdspbufs;
	struct msm_cvp_list frames;
	struct completion completions[SESSION_MSG_END - SESSION_MSG_START + 1];
	struct dentry *debugfs_root;
	struct msm_cvp_debug debug;
	struct cvp_clock_data clk_data;
	enum msm_cvp_modes flags;
	struct msm_cvp_capability capability;
	struct kref kref;
	struct cvp_session_prop prop;
	u32 cur_cmd_type;
	struct synx_session synx_session_id;
	struct cvp_fence_queue fence_cmd_queue;
};

extern struct msm_cvp_drv *cvp_driver;

void cvp_handle_cmd_response(enum hal_command_response cmd, void *data);
int msm_cvp_trigger_ssr(struct msm_cvp_core *core,
	enum hal_ssr_trigger_type type);
int msm_cvp_noc_error_info(struct msm_cvp_core *core);
void msm_cvp_comm_handle_thermal_event(void);

void msm_cvp_fw_unload_handler(struct work_struct *work);
void msm_cvp_ssr_handler(struct work_struct *work);
/*
 * XXX: normally should be in msm_cvp_core.h, but that's meant for public APIs,
 * whereas this is private
 */
int msm_cvp_destroy(struct msm_cvp_inst *inst);
void *cvp_get_drv_data(struct device *dev);
#endif
