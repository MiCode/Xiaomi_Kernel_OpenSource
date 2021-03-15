/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __VIDC_HFI_API_H__
#define __VIDC_HFI_API_H__

#include <linux/log2.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/hash.h>
#include "msm_vidc.h"
#include "msm_vidc_resources.h"

#define CONTAINS(__a, __sz, __t) (\
	(__t >= __a) && \
	(__t < __a + __sz) \
)

#define OVERLAPS(__t, __tsz, __a, __asz) (\
	(__t <= __a) && \
	(__t + __tsz >= __a + __asz) \
)

#define HAL_BUFFERFLAG_EOS              0x00000001
#define HAL_BUFFERFLAG_STARTTIME        0x00000002
#define HAL_BUFFERFLAG_DATACORRUPT      0x00000008
#define HAL_BUFFERFLAG_ENDOFFRAME       0x00000010
#define HAL_BUFFERFLAG_SYNCFRAME        0x00000020
#define HAL_BUFFERFLAG_EXTRADATA        0x00000040
#define HAL_BUFFERFLAG_CODECCONFIG      0x00000080
#define HAL_BUFFERFLAG_READONLY         0x00000200
#define HAL_BUFFERFLAG_ENDOFSUBFRAME    0x00000400
#define HAL_BUFFERFLAG_MBAFF            0x08000000
#define HAL_BUFFERFLAG_YUV_601_709_CSC_CLAMP   0x10000000
#define HAL_BUFFERFLAG_DROP_FRAME       0x20000000
#define HAL_BUFFERFLAG_TS_DISCONTINUITY	0x40000000
#define HAL_BUFFERFLAG_TS_ERROR		0x80000000
#define HAL_BUFFERFLAG_CVPMETADATA_SKIP    0x00000800


#define HAL_DEBUG_MSG_LOW				0x00000001
#define HAL_DEBUG_MSG_MEDIUM			0x00000002
#define HAL_DEBUG_MSG_HIGH				0x00000004
#define HAL_DEBUG_MSG_ERROR				0x00000008
#define HAL_DEBUG_MSG_FATAL				0x00000010
#define MAX_PROFILE_COUNT	16

#define HAL_MAX_MATRIX_COEFFS 9
#define HAL_MAX_BIAS_COEFFS 3
#define HAL_MAX_LIMIT_COEFFS 6
#define VENUS_VERSION_LENGTH 128

#define IDR_PERIOD 1

/* 16 video sessions */
#define VIDC_MAX_SESSIONS               16

enum vidc_status {
	VIDC_ERR_NONE = 0x0,
	VIDC_ERR_FAIL = 0x80000000,
	VIDC_ERR_ALLOC_FAIL,
	VIDC_ERR_ILLEGAL_OP,
	VIDC_ERR_BAD_PARAM,
	VIDC_ERR_BAD_HANDLE,
	VIDC_ERR_NOT_SUPPORTED,
	VIDC_ERR_BAD_STATE,
	VIDC_ERR_MAX_CLIENTS,
	VIDC_ERR_IFRAME_EXPECTED,
	VIDC_ERR_HW_FATAL,
	VIDC_ERR_BITSTREAM_ERR,
	VIDC_ERR_INDEX_NOMORE,
	VIDC_ERR_SEQHDR_PARSE_FAIL,
	VIDC_ERR_INSUFFICIENT_BUFFER,
	VIDC_ERR_BAD_POWER_STATE,
	VIDC_ERR_NO_VALID_SESSION,
	VIDC_ERR_TIMEOUT,
	VIDC_ERR_CMDQFULL,
	VIDC_ERR_START_CODE_NOT_FOUND,
	VIDC_ERR_NOC_ERROR,
	VIDC_ERR_CLIENT_PRESENT = 0x90000001,
	VIDC_ERR_CLIENT_FATAL,
	VIDC_ERR_CMD_QUEUE_FULL,
	VIDC_ERR_UNUSED = 0x10000000
};

enum hal_domain {
	HAL_VIDEO_DOMAIN_VPE        = BIT(0),
	HAL_VIDEO_DOMAIN_ENCODER    = BIT(1),
	HAL_VIDEO_DOMAIN_DECODER    = BIT(2),
	HAL_VIDEO_DOMAIN_CVP        = BIT(3),
	HAL_UNUSED_DOMAIN = 0x10000000,
};

enum multi_stream {
	HAL_VIDEO_DECODER_NONE = 0x00000000,
	HAL_VIDEO_DECODER_PRIMARY = 0x00000001,
	HAL_VIDEO_DECODER_SECONDARY = 0x00000002,
	HAL_VIDEO_DECODER_BOTH_OUTPUTS = 0x00000004,
	HAL_VIDEO_UNUSED_OUTPUTS = 0x10000000,
};

enum hal_core_capabilities {
	HAL_VIDEO_ENCODER_ROTATION_CAPABILITY = 0x00000001,
	HAL_VIDEO_ENCODER_SCALING_CAPABILITY = 0x00000002,
	HAL_VIDEO_ENCODER_DEINTERLACE_CAPABILITY = 0x00000004,
	HAL_VIDEO_DECODER_MULTI_STREAM_CAPABILITY = 0x00000008,
	HAL_VIDEO_UNUSED_CAPABILITY      = 0x10000000,
};

enum hal_default_properties {
	HAL_VIDEO_DYNAMIC_BUF_MODE = 0x00000001,
	HAL_VIDEO_CONTINUE_DATA_TRANSFER = 0x00000002,
};

enum hal_video_codec {
	HAL_VIDEO_CODEC_UNKNOWN  = 0x00000000,
	HAL_VIDEO_CODEC_MVC      = 0x00000001,
	HAL_VIDEO_CODEC_H264     = 0x00000002,
	HAL_VIDEO_CODEC_H263     = 0x00000004,
	HAL_VIDEO_CODEC_MPEG1    = 0x00000008,
	HAL_VIDEO_CODEC_MPEG2    = 0x00000010,
	HAL_VIDEO_CODEC_MPEG4    = 0x00000020,
	HAL_VIDEO_CODEC_DIVX_311 = 0x00000040,
	HAL_VIDEO_CODEC_DIVX     = 0x00000080,
	HAL_VIDEO_CODEC_VC1      = 0x00000100,
	HAL_VIDEO_CODEC_SPARK    = 0x00000200,
	HAL_VIDEO_CODEC_VP6      = 0x00000400,
	HAL_VIDEO_CODEC_VP7      = 0x00000800,
	HAL_VIDEO_CODEC_VP8      = 0x00001000,
	HAL_VIDEO_CODEC_HEVC     = 0x00002000,
	HAL_VIDEO_CODEC_VP9      = 0x00004000,
	HAL_VIDEO_CODEC_TME      = 0x00008000,
	HAL_VIDEO_CODEC_CVP      = 0x00010000,
	HAL_VIDEO_CODEC_HEVC_HYBRID     = 0x80000000,
	HAL_UNUSED_CODEC = 0x10000000,
};

enum hal_uncompressed_format {
	HAL_COLOR_FORMAT_MONOCHROME     = 0x00000001,
	HAL_COLOR_FORMAT_NV12           = 0x00000002,
	HAL_COLOR_FORMAT_NV21           = 0x00000004,
	HAL_COLOR_FORMAT_NV12_4x4TILE   = 0x00000008,
	HAL_COLOR_FORMAT_NV21_4x4TILE   = 0x00000010,
	HAL_COLOR_FORMAT_YUYV           = 0x00000020,
	HAL_COLOR_FORMAT_YVYU           = 0x00000040,
	HAL_COLOR_FORMAT_UYVY           = 0x00000080,
	HAL_COLOR_FORMAT_VYUY           = 0x00000100,
	HAL_COLOR_FORMAT_RGB565         = 0x00000200,
	HAL_COLOR_FORMAT_BGR565         = 0x00000400,
	HAL_COLOR_FORMAT_RGB888         = 0x00000800,
	HAL_COLOR_FORMAT_BGR888         = 0x00001000,
	HAL_COLOR_FORMAT_NV12_UBWC      = 0x00002000,
	HAL_COLOR_FORMAT_NV12_TP10_UBWC = 0x00004000,
	HAL_COLOR_FORMAT_RGBA8888       = 0x00008000,
	HAL_COLOR_FORMAT_RGBA8888_UBWC  = 0x00010000,
	HAL_COLOR_FORMAT_P010           = 0x00020000,
	HAL_COLOR_FORMAT_NV12_512       = 0x00040000,
	HAL_UNUSED_COLOR                = 0x10000000,
};

enum hal_ssr_trigger_type {
	SSR_ERR_FATAL = 1,
	SSR_SW_DIV_BY_ZERO,
	SSR_HW_WDOG_IRQ,
};

struct hal_profile_level {
	u32 profile;
	u32 level;
};

struct hal_profile_level_supported {
	u32 profile_count;
	struct hal_profile_level profile_level[MAX_PROFILE_COUNT];
};

enum hal_intra_refresh_mode {
	HAL_INTRA_REFRESH_NONE = 0x1,
	HAL_INTRA_REFRESH_CYCLIC = 0x2,
	HAL_INTRA_REFRESH_RANDOM = 0x5,
	HAL_UNUSED_INTRA = 0x10000000,
};

struct hal_intra_refresh {
	enum hal_intra_refresh_mode mode;
	u32 ir_mbs;
};

struct hal_buffer_requirements {
	enum hal_buffer buffer_type;
	u32 buffer_size;
	u32 buffer_region_size;
	u32 buffer_count_min;
	u32 buffer_count_min_host;
	u32 buffer_count_actual;
	u32 contiguous;
	u32 buffer_alignment;
};

enum hal_priority {/* Priority increases with number */
	HAL_PRIORITY_LOW = 10,
	HAL_PRIOIRTY_MEDIUM = 20,
	HAL_PRIORITY_HIGH = 30,
	HAL_UNUSED_PRIORITY = 0x10000000,
};

struct hal_batch_info {
	u32 input_batch_count;
	u32 output_batch_count;
};

struct hal_uncompressed_format_supported {
	enum hal_buffer buffer_type;
	u32 format_entries;
	u32 rg_format_info[1];
};

enum hal_interlace_format {
	HAL_INTERLACE_FRAME_PROGRESSIVE                 = 0x01,
	HAL_INTERLACE_INTERLEAVE_FRAME_TOPFIELDFIRST    = 0x02,
	HAL_INTERLACE_INTERLEAVE_FRAME_BOTTOMFIELDFIRST = 0x04,
	HAL_INTERLACE_FRAME_TOPFIELDFIRST               = 0x08,
	HAL_INTERLACE_FRAME_BOTTOMFIELDFIRST            = 0x10,
	HAL_UNUSED_INTERLACE = 0x10000000,
};

struct hal_interlace_format_supported {
	enum hal_buffer buffer_type;
	enum hal_interlace_format format;
};

enum hal_chroma_site {
	HAL_CHROMA_SITE_0,
	HAL_CHROMA_SITE_1,
	HAL_UNUSED_CHROMA = 0x10000000,
};

enum hal_capability {
	CAP_FRAME_WIDTH = 0x1,
	CAP_FRAME_HEIGHT,
	CAP_MBS_PER_FRAME,
	CAP_MBS_PER_SECOND,
	CAP_FRAMERATE,
	CAP_OPERATINGRATE,
	CAP_SCALE_X,
	CAP_SCALE_Y,
	CAP_BITRATE,
	CAP_CABAC_BITRATE,
	CAP_BFRAME,
	CAP_PEAKBITRATE,
	CAP_HIER_P_NUM_ENH_LAYERS,
	CAP_LTR_COUNT,
	CAP_SECURE_OUTPUT2_THRESHOLD,
	CAP_HIER_B_NUM_ENH_LAYERS,
	CAP_LCU_SIZE,
	CAP_HIER_P_HYBRID_NUM_ENH_LAYERS,
	CAP_MBS_PER_SECOND_POWER_SAVE,
	CAP_EXTRADATA,
	CAP_PROFILE,
	CAP_LEVEL,
	CAP_I_FRAME_QP,
	CAP_P_FRAME_QP,
	CAP_B_FRAME_QP,
	CAP_RATE_CONTROL_MODES,
	CAP_BLUR_WIDTH,
	CAP_BLUR_HEIGHT,
	CAP_SLICE_BYTE,
	CAP_SLICE_MB,
	CAP_SECURE,
	CAP_MAX_NUM_B_FRAMES,
	CAP_MAX_VIDEOCORES,
	CAP_MAX_WORKMODES,
	CAP_UBWC_CR_STATS,
	CAP_SECURE_FRAME_WIDTH,
	CAP_SECURE_FRAME_HEIGHT,
	CAP_SECURE_MBS_PER_FRAME,
	CAP_SECURE_BITRATE,
	CAP_BATCH_MAX_MB_PER_FRAME,
	CAP_BATCH_MAX_FPS,
	CAP_LOSSLESS_FRAME_WIDTH,
	CAP_LOSSLESS_FRAME_HEIGHT,
	CAP_LOSSLESS_MBS_PER_FRAME,
	CAP_ALLINTRA_MAX_FPS,
	CAP_HEVC_IMAGE_FRAME_WIDTH,
	CAP_HEVC_IMAGE_FRAME_HEIGHT,
	CAP_HEIC_IMAGE_FRAME_WIDTH,
	CAP_HEIC_IMAGE_FRAME_HEIGHT,
	CAP_H264_LEVEL,
	CAP_HEVC_LEVEL,
	CAP_MAX,
};

struct hal_capability_supported {
	enum hal_capability capability_type;
	u32 min;
	u32 max;
	u32 step_size;
	u32 default_value;
};

struct hal_nal_stream_format_supported {
	u32 nal_stream_format_supported;
};

struct hal_nal_stream_format_select {
	u32 nal_stream_format_select;
};

struct hal_multi_view_format {
	u32 views;
	u32 rg_view_order[1];
};

enum hal_buffer_layout_type {
	HAL_BUFFER_LAYOUT_TOP_BOTTOM,
	HAL_BUFFER_LAYOUT_SEQ,
	HAL_UNUSED_BUFFER_LAYOUT = 0x10000000,
};

struct hal_codec_supported {
	u32 decoder_codec_supported;
	u32 encoder_codec_supported;
};

enum hal_core_id {
	VIDC_CORE_ID_DEFAULT = 0,
	VIDC_CORE_ID_1 = 1, /* 0b01 */
	VIDC_CORE_ID_2 = 2, /* 0b10 */
	VIDC_CORE_ID_3 = 3, /* 0b11 */
	VIDC_CORE_ID_UNUSED = 0x10000000,
};

enum vidc_resource_id {
	VIDC_RESOURCE_NONE,
	VIDC_RESOURCE_SYSCACHE,
	VIDC_UNUSED_RESOURCE = 0x10000000,
};

struct vidc_resource_hdr {
	enum vidc_resource_id resource_id;
	void *resource_handle;
};

struct vidc_register_buffer {
	enum hal_buffer type;
	u32 index;
	u32 size;
	u32 device_addr;
	u32 response_required;
	u32 client_data;
};

struct vidc_unregister_buffer {
	enum hal_buffer type;
	u32 index;
	u32 size;
	u32 device_addr;
	u32 response_required;
	u32 client_data;
};

struct vidc_buffer_addr_info {
	enum hal_buffer buffer_type;
	u32 buffer_size;
	u32 num_buffers;
	u32 align_device_addr;
	u32 extradata_addr;
	u32 extradata_size;
	u32 response_required;
};

/* Needs to be exactly the same as hfi_buffer_info */
struct hal_buffer_info {
	u32 buffer_addr;
	u32 extra_data_addr;
};

struct vidc_frame_plane_config {
	u32 left;
	u32 top;
	u32 width;
	u32 height;
	u32 stride;
	u32 scan_lines;
};

struct vidc_uncompressed_frame_config {
	struct vidc_frame_plane_config luma_plane;
	struct vidc_frame_plane_config chroma_plane;
};

struct vidc_frame_data {
	enum hal_buffer buffer_type;
	u32 device_addr;
	u32 extradata_addr;
	int64_t timestamp;
	u32 flags;
	u32 offset;
	u32 alloc_len;
	u32 filled_len;
	u32 input_tag;
	u32 extradata_size;
};

struct hal_fw_info {
	char version[VENUS_VERSION_LENGTH];
	phys_addr_t base_addr;
	int register_base;
	int register_size;
	int irq;
};

enum hal_flush {
	HAL_FLUSH_INPUT = BIT(0),
	HAL_FLUSH_OUTPUT = BIT(1),
	HAL_FLUSH_ALL = HAL_FLUSH_INPUT | HAL_FLUSH_OUTPUT,
};

enum hal_event_type {
	HAL_EVENT_SEQ_CHANGED_SUFFICIENT_RESOURCES,
	HAL_EVENT_SEQ_CHANGED_INSUFFICIENT_RESOURCES,
	HAL_EVENT_RELEASE_BUFFER_REFERENCE,
	HAL_UNUSED_SEQCHG = 0x10000000,
};

enum buffer_mode_type {
	HAL_BUFFER_MODE_DYNAMIC = 0x100,
	HAL_BUFFER_MODE_STATIC = 0x001,
};

struct hal_buffer_alloc_mode {
	enum hal_buffer buffer_type;
	enum buffer_mode_type buffer_mode;
};

enum ltr_mode {
	HAL_LTR_MODE_DISABLE,
	HAL_LTR_MODE_MANUAL,
};

struct buffer_requirements {
	struct hal_buffer_requirements buffer[HAL_BUFFER_MAX];
};

struct hal_conceal_color {
	u32 conceal_color_8bit;
	u32 conceal_color_10bit;
};

union hal_get_property {
	struct hal_batch_info batch_info;
	struct hal_uncompressed_format_supported uncompressed_format_supported;
	struct hal_interlace_format_supported interlace_format_supported;
	struct hal_nal_stream_format_supported nal_stream_format_supported;
	struct hal_nal_stream_format_select nal_stream_format_select;
	struct hal_multi_view_format multi_view_format;
	struct hal_buffer_info buffer_info;
	struct hal_buffer_alloc_mode buffer_alloc_mode;
	struct buffer_requirements buf_req;
	struct hal_conceal_color conceal_color;
};

/* HAL Response */
#define IS_HAL_SYS_CMD(cmd) ((cmd) >= HAL_SYS_INIT_DONE && \
		(cmd) <= HAL_SYS_ERROR)
#define IS_HAL_SESSION_CMD(cmd) ((cmd) >= HAL_SESSION_EVENT_CHANGE && \
		(cmd) <= HAL_SESSION_ERROR)
enum hal_command_response {
	/* SYSTEM COMMANDS_DONE*/
	HAL_SYS_INIT_DONE,
	HAL_SYS_SET_RESOURCE_DONE,
	HAL_SYS_RELEASE_RESOURCE_DONE,
	HAL_SYS_PC_PREP_DONE,
	HAL_SYS_IDLE,
	HAL_SYS_DEBUG,
	HAL_SYS_WATCHDOG_TIMEOUT,
	HAL_SYS_ERROR,
	/* SESSION COMMANDS_DONE */
	HAL_SESSION_EVENT_CHANGE,
	HAL_SESSION_LOAD_RESOURCE_DONE,
	HAL_SESSION_INIT_DONE,
	HAL_SESSION_END_DONE,
	HAL_SESSION_ABORT_DONE,
	HAL_SESSION_START_DONE,
	HAL_SESSION_STOP_DONE,
	HAL_SESSION_ETB_DONE,
	HAL_SESSION_FTB_DONE,
	HAL_SESSION_FLUSH_DONE,
	HAL_SESSION_SUSPEND_DONE,
	HAL_SESSION_RESUME_DONE,
	HAL_SESSION_SET_PROP_DONE,
	HAL_SESSION_GET_PROP_DONE,
	HAL_SESSION_RELEASE_BUFFER_DONE,
	HAL_SESSION_REGISTER_BUFFER_DONE,
	HAL_SESSION_UNREGISTER_BUFFER_DONE,
	HAL_SESSION_RELEASE_RESOURCE_DONE,
	HAL_SESSION_PROPERTY_INFO,
	HAL_SESSION_ERROR,
	HAL_RESPONSE_UNUSED = 0x10000000,
};

struct ubwc_cr_stats_info_type {
	u32 cr_stats_info0;
	u32 cr_stats_info1;
	u32 cr_stats_info2;
	u32 cr_stats_info3;
	u32 cr_stats_info4;
	u32 cr_stats_info5;
	u32 cr_stats_info6;
};

struct recon_stats_type {
	u32 buffer_index;
	u32 complexity_number;
	struct ubwc_cr_stats_info_type ubwc_stats_info;
};

struct vidc_hal_ebd {
	u32 timestamp_hi;
	u32 timestamp_lo;
	u32 flags;
	enum vidc_status status;
	u32 input_tag;
	u32 stats;
	u32 offset;
	u32 alloc_len;
	u32 filled_len;
	u32 picture_type;
	struct recon_stats_type recon_stats;
	u32 packet_buffer;
	u32 extra_data_buffer;
};

struct vidc_hal_fbd {
	u32 stream_id;
	u32 view_id;
	u32 timestamp_hi;
	u32 timestamp_lo;
	u32 flags1;
	u32 stats;
	u32 alloc_len1;
	u32 filled_len1;
	u32 offset1;
	u32 frame_width;
	u32 frame_height;
	u32 start_x_coord;
	u32 start_y_coord;
	u32 input_tag;
	u32 input_tag2;
	u32 picture_type;
	u32 packet_buffer1;
	u32 extra_data_buffer;
	u32 flags2;
	u32 alloc_len2;
	u32 filled_len2;
	u32 offset2;
	u32 packet_buffer2;
	u32 flags3;
	u32 alloc_len3;
	u32 filled_len3;
	u32 offset3;
	u32 packet_buffer3;
	enum hal_buffer buffer_type;
};

struct msm_vidc_capability {
	enum hal_domain domain;
	enum hal_video_codec codec;
	struct hal_capability_supported cap[CAP_MAX];
};

struct vidc_hal_sys_init_done {
	u32 dec_codec_supported;
	u32 enc_codec_supported;
	u32 max_sessions_supported;
};

struct msm_vidc_cb_cmd_done {
	u32 device_id;
	void *inst_id;
	enum vidc_status status;
	u32 size;
	union {
		struct vidc_resource_hdr resource_hdr;
		struct vidc_buffer_addr_info buffer_addr_info;
		struct vidc_frame_plane_config frame_plane_config;
		struct vidc_uncompressed_frame_config uncompressed_frame_config;
		struct vidc_frame_data frame_data;
		struct vidc_hal_ebd ebd;
		struct vidc_hal_fbd fbd;
		struct vidc_hal_sys_init_done sys_init_done;
		struct hal_buffer_info buffer_info;
		struct vidc_register_buffer regbuf;
		struct vidc_unregister_buffer unregbuf;
		union hal_get_property property;
		enum hal_flush flush_type;
	} data;
};

struct msm_vidc_cb_event {
	u32 device_id;
	void *inst_id;
	enum vidc_status status;
	u32 height;
	u32 width;
	int bit_depth;
	u32 hal_event_type;
	u32 packet_buffer;
	u32 extra_data_buffer;
	u32 pic_struct;
	u32 colour_space;
	u32 profile;
	u32 level;
	u32 entropy_mode;
	u32 max_dpb_count;
	u32 max_ref_frames;
	u32 max_dec_buffering;
	u32 max_reorder_frames;
	u32 fw_min_cnt;
};

struct msm_vidc_cb_data_done {
	u32 device_id;
	void *inst_id;
	enum vidc_status status;
	u32 size;
	union {
		struct vidc_hal_ebd input_done;
		struct vidc_hal_fbd output_done;
	};
};

struct msm_vidc_cb_info {
	enum hal_command_response response_type;
	union {
		struct msm_vidc_cb_cmd_done cmd;
		struct msm_vidc_cb_event event;
		struct msm_vidc_cb_data_done data;
	} response;
};

enum msm_vidc_hfi_type {
	VIDC_HFI_VENUS,
};

enum msm_vidc_thermal_level {
	VIDC_THERMAL_NORMAL = 0,
	VIDC_THERMAL_LOW,
	VIDC_THERMAL_HIGH,
	VIDC_THERMAL_CRITICAL
};

enum msm_vidc_power_mode {
	VIDC_POWER_NORMAL = 0,
	VIDC_POWER_LOW,
	VIDC_POWER_TURBO
};

struct hal_cmd_sys_get_property_packet {
	u32 size;
	u32 packet_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hal_hdr10_pq_sei {
	struct msm_vidc_mastering_display_colour_sei_payload disp_color_sei;
	struct msm_vidc_content_light_level_sei_payload cll_sei;
};

struct hal_vbv_hdr_buf_size {
	u32 vbv_hdr_buf_size;
};

#define call_hfi_op(q, op, ...)			\
	(((q) && (q)->op) ? ((q)->op(__VA_ARGS__)) : 0)

struct hfi_device {
	void *hfi_device_data;

	/*Add function pointers for all the hfi functions below*/
	int (*core_init)(void *device);
	int (*core_release)(void *device);
	int (*core_trigger_ssr)(void *device, enum hal_ssr_trigger_type);
	int (*session_init)(void *device, void *inst_id,
		enum hal_domain session_type, enum hal_video_codec codec_type,
		void **new_session, u32 sid);
	int (*session_end)(void *session);
	int (*session_abort)(void *session);
	int (*session_set_buffers)(void *sess,
				struct vidc_buffer_addr_info *buffer_info);
	int (*session_release_buffers)(void *sess,
				struct vidc_buffer_addr_info *buffer_info);
	int (*session_register_buffer)(void *sess,
				struct vidc_register_buffer *buffer);
	int (*session_unregister_buffer)(void *sess,
				struct vidc_unregister_buffer *buffer);
	int (*session_load_res)(void *sess);
	int (*session_release_res)(void *sess);
	int (*session_start)(void *sess);
	int (*session_continue)(void *sess);
	int (*session_stop)(void *sess);
	int (*session_etb)(void *sess, struct vidc_frame_data *input_frame);
	int (*session_ftb)(void *sess, struct vidc_frame_data *output_frame);
	int (*session_process_batch)(void *sess,
		int num_etbs, struct vidc_frame_data etbs[],
		int num_ftbs, struct vidc_frame_data ftbs[]);
	int (*session_get_buf_req)(void *sess);
	int (*session_flush)(void *sess, enum hal_flush flush_mode);
	int (*session_set_property)(void *sess, u32 ptype,
			void *pdata, u32 size);
	int (*session_pause)(void *sess);
	int (*session_resume)(void *sess);
	int (*scale_clocks)(void *dev, u32 freq, u32 sid);
	int (*vote_bus)(void *dev, unsigned long bw_ddr,
			unsigned long bw_llcc, u32 sid);
	int (*get_fw_info)(void *dev, struct hal_fw_info *fw_info);
	int (*session_clean)(void *sess);
	int (*get_core_capabilities)(void *dev);
	int (*suspend)(void *dev);
	int (*flush_debug_queue)(void *dev);
	int (*noc_error_info)(void *dev);
	enum hal_default_properties (*get_default_properties)(void *dev);
};

typedef void (*hfi_cmd_response_callback) (enum hal_command_response cmd,
			void *data);
typedef void (*msm_vidc_callback) (u32 response, void *callback);

struct hfi_device *vidc_hfi_initialize(enum msm_vidc_hfi_type hfi_type,
		u32 device_id, struct msm_vidc_platform_resources *res,
		hfi_cmd_response_callback callback);
void vidc_hfi_deinitialize(enum msm_vidc_hfi_type hfi_type,
			struct hfi_device *hdev);
u32 vidc_get_hfi_domain(enum hal_domain hal_domain, u32 sid);
u32 vidc_get_hfi_codec(enum hal_video_codec hal_codec, u32 sid);
#endif /*__VIDC_HFI_API_H__ */
