/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#ifndef __H_VPU_PROPERTY_H__
#define __H_VPU_PROPERTY_H__

/* all properties needed for VPU */
#define VPU_PROP_SYS_BASE                           0x00000000
#define VPU_PROP_SYS_VERSION                        0x00000001

#define VPU_PROP_SYS_STATUS                         0x00000003
#define VPU_PROP_SYS_LOG_CONFIG                     0x00000004
#define VPU_PROP_SYS_LOG_CTRL                       0x00000005
#define VPU_PROP_SYS_SYS_STATS                      0x00000006
#define VPU_PROP_SYS_TRACE_CTRL                     0x00000007
#define VPU_PROP_SYS_GENERIC                        0x00000ffe

#define VPU_PROP_SESSION_BASE                       0x00001000
#define VPU_PROP_SESSION_PRIORITY                   0x00001001
#define VPU_PROP_SESSION_STATUS                     0x00001002
#define VPU_PROP_SESSION_BUFFER_REQUIREMENTS        0x00001003
#define VPU_PROP_SESSION_LATENCY_REQUIREMENTS       0x00001004
#define VPU_PROP_SESSION_STATS                      0x00001005

#define VPU_PROP_SESSION_COMMIT                     0x00001010
#define VPU_PROP_SESSION_INPUT                      0x00001011
#define VPU_PROP_SESSION_OUTPUT                     0x00001012
#define VPU_PROP_SESSION_STD_SETUP                  0x00001013
#define VPU_PROP_SESSION_ROI                        0x00001014
#define VPU_PROP_SESSION_DISPLAY_FPS                0x00001015
#define VPU_PROP_SESSION_INPUT_FPS                  0x00001016
#define VPU_PROP_SESSION_OUTPUT_FPS                 0x00001017
#define VPU_PROP_SESSION_BACKGROUND_COLOR           0x00001018
#define VPU_PROP_SESSION_RANGE_MAPPING              0x00001019
#define VPU_PROP_SESSION_COLOR_SPACE                0x0000101a
#define VPU_PROP_SESSION_SOURCE_CONFIG              0x0000101b
#define VPU_PROP_SESSION_SINK_CONFIG                0x0000101c

#define VPU_PROP_SESSION_DEINTERLACING              0x00001030
#define VPU_PROP_SESSION_NOISE_REDUCTION            0x00001031
#define VPU_PROP_SESSION_ANAMORPHIC_SCALING         0x00001032
#define VPU_PROP_SESSION_BAR_DETECT                 0x00001033 /* deprecated */
#define VPU_PROP_SESSION_NOISE_REDUCTION_CONFIG     0x00001034
#define VPU_PROP_SESSION_ACTIVE_REGION_DETECT       0x00001035

#define VPU_PROP_SESSION_DETAIL_ENHANCEMENT         0x00001040 /* deprecated */
#define VPU_PROP_SESSION_DI                         0x00001041
#define VPU_PROP_SESSION_STI                        0x00001042 /* deprecated */
#define VPU_PROP_SESSION_IMAGE_ENHANCEMENT          0x00001043

#define VPU_PROP_SESSION_AUTO_HQV                   0x00001050
#define VPU_PROP_SESSION_AUTO_HQV_SHARPEN_STRENGTH  0x00001051
#define VPU_PROP_SESSION_AUTO_HQV_AUTONR_STRENGTH   0x00001052

#define VPU_PROP_SESSION_ACE                        0x00001100
#define VPU_PROP_SESSION_ACE_BRIGHTNESS             0x00001101
#define VPU_PROP_SESSION_ACE_CONTRAST               0x00001102

#define VPU_PROP_SESSION_2D3D                       0x00001110
#define VPU_PROP_SESSION_2D3D_DEPTH                 0x00001111
#define VPU_PROP_SESSION_TIMESTAMP                  0x00001118
#define VPU_PROP_SESSION_TIMESTAMP_AUTO_MODE        0x00001119

#define VPU_PROP_SESSION_FRC                        0x00001900
#define VPU_PROP_SESSION_FRC_MOTION_SMOOTHNESS      0x00001901
#define VPU_PROP_SESSION_FRC_MOTION_CLEAR           0x00001902
#define VPU_PROP_SESSION_FRC_EXTENDED               0x00001903

#define VPU_PROP_SESSION_GENERIC                    0x00001ffe


/* General purpose property data structure
 *  Used for properties with limited msg data requirements.
 *  Value and flag are limited to 32bits each.
 *  Value and flag meaning is property dependent
 */
struct vpu_data_value {
	u32	value;
	u32	flags;
};

struct vpu_s_data_value {
	s32	value;
	u32	flags;
};


struct vpu_data_pkt {
	/* total size in bytes (size of nSize + payload size) */
	u32	size;
	/* variable length payload starting at payload[0] */
	u32	payload[1];
};

#define PROP_FALSE			0
#define PROP_TRUE			1

#define PROP_MODE_DISABLED		0
#define PROP_MODE_MANUAL		1
#define PROP_MODE_AUTO			2

/* data types defined for VPU_PROP_SYS_VERSION */
struct vpu_sys_version {
	/* incremental change within the minor version */
	u32 step;
	/* major version of a core: substantial change affecting the core */
	u32 major;
	/* minor version of a core: small change within the major version */
	u32 minor;
};

/* associated structure for VPU_PROP_SYS_VERSION */
struct vpu_prop_sys_version {
	/* system VPU hardware revision information */
	struct vpu_sys_version	hw_version;
	/* system firmware revision information */
	struct vpu_sys_version	fw_version;
	/* reserved */
	u32			reserved;
};

/* values defined for VPU_PROP_SYS_STATUS */
/* system is in an uninitialized state */
#define VPU_SYS_STATE_UNKNOWN		0
/* system is idle, i.e. no session is currently opened for any activity */
#define VPU_SYS_STATE_IDLE		1
/* system is active, i.e. at least one session is opened */
#define VPU_SYS_STATE_ACTIVE		2

/* associated structure for VPU_PROP_SYS_STATUS */
struct vpu_prop_sys_status {
	/* state of the system: unknown, idle or active*/
	u32 state;
	/* total number of sessions supported by the system */
	u32 sessions_total;
	/* number of opened sessions in the system */
	u32 sessions_open;

	/* Bit mask representing session ID of opened sessions.
	 * ex. sessionId 0 = bit 0
	 */
	u32 session_mask;
};

/* values defined for VPU_PROP_SYS_LOG_CONFIG */
/* logging only applied to FW component */
#define	LOG_COMPONENT_FW	0
/* logging only applied to FRC component  */
#define	LOG_COMPONENT_FRC	1

/* associated structure for VPU_PROP_SYS_LOG_CONFIG */
struct vpu_prop_sys_log_config {
	/* component that logging config is applied to: Firmware or FRC
	 * (refer to LOGGING_COMP_ defines)
	 */
	u32	component;
	/* virtual addr for FRC */
	u32	log_q_addr;
};

/* values defined for VPU_PROP_SYS_LOG_CTRL */
/* logging level.
 * Range is from least verbose (no logs) to most verbose (all logs)
 */
#define VPU_LOGGING_NONE	0
#define VPU_LOGGING_FATAL	1
#define VPU_LOGGING_ERROR	2
#define VPU_LOGGING_WARNING	3
#define VPU_LOGGING_MSGHIGH	4
#define VPU_LOGGING_MSGMED	5
#define VPU_LOGGING_MSGLOW	6
#define VPU_LOGGING_ALL		7

/* associated structure for VPU_PROP_SYS_LOG_CTRL */
struct vpu_prop_sys_log_ctrl {
	/* component that logging config is applied to: Firmware or FRC
	 * (refer to LOG_COMPONENT_ defines)
	 */
	u32	component;
	/* logging level for messages(refer to LOGGING_LEVEL_ defines) */
	u32     log_level;
};

/* values defined for VPU_PROP_SYS_GENERIC */
#define VPU_PROP_SYS_WATCHDOG_TEST		0x20000100
#define VPU_PROP_SYS_TABLE_LOAD			0x20000111
#define VPU_PROP_SYS_TUNE			0x20000112
#define VPU_PROP_SYS_STATS_EXTENDED		0x20000113

/* associated structure for VPU_PROP_SYS_GENERIC
 *  struct vpu_data_pkt:
 *       size:       uint32 (total size of struct in bytes)
 *       payload[0]: uint32 (refer to VPU_PROP_SYS_ defines)
 *       payload[1-n]: (optional additional data)
 */

/* values defined for VPU_PROP_SESSION_PRIORITY */
#define VPU_SESSION_PRIORITY_HIGH                   100
#define VPU_SESSION_PRIORITY_NORMAL                  50

/* associated structure for VPU_PROP_SESSION_PRIORITY
 * struct vpu_data_value:
 *       value: uint32 (refer to value limits)
 *       flags: uint32 (reserved)
 */

/* values defined for VPU_PROP_SESSION_STATUS */
/* session is not opened yet */
#define SESSION_STATE_NONE	0
/* session is already opened, session SET/GET commands can be issued */
#define SESSION_STATE_OPENED	1
/* session is closed successfully, no more commands */
#define SESSION_STATE_CLOSED	2
/* session is started and actively running */
#define SESSION_STATE_STARTED	3
/* session is stopped with any video processing activities */
#define SESSION_STATE_STOPPED	4
/* session is paused completely, no other command can be issued than resume */
#define SESSION_STATE_PAUSED	5

/* associated structure for VPU_PROP_SESSION_STATUS
 * struct vpu_data_value:
 *       value = uint32 (refer to defines)
 *       flags = uint32 (reserved)
 */

/* associated structure for VPU_PROP_SESSION_BUFFER_REQUIREMENTS */
struct vpu_prop_session_buffer_requirements {
	/* vpu_ipc_buffer: buffer type */
	u32	buffer_type;
	/* size of buffers that are required */
	u32	buffer_size;
	/* num of buffers that are required */
	u32	buffer_num;
	/* required buffers need to be contiguous, 0 no, 1 yes */
	u32	contiguous;
	/* buffer start address alignment */
	u32	alignment;
	/* Minimum alignment in bytes for each plane */
	u32	plane0_stride_align;
	u32	plane1_stride_align;
	u32	plane2_stride_align;
	/* Extra padding in pixels for frame width */
	u32	extra_pad_width;
};

/* associated structure for VPU_PROP_SESSION_LATENCY_REQUIREMENTS
 * struct vpu_data_value:
 *       value: uint32. latency in microseconds
 *       flags: uint32 (reserved)
 */

/* values defined for VPU_PROP_SESSION_COMMIT */
#define VPU_COMMIT_APPLY_AT_ONCE	0
#define VPU_COMMIT_APPLY_IN_ORDER	1

/* associated structure for VPU_PROP_SESSION_COMMIT
 * struct vpu_data_value:
 *       value: uint32 (refer to defines)
 *       flags = uint32 (reserved)
 */

/* data structures and enum defined for VPU_PROP_SESSION_INPUT
 * and VPU_PROP_SESSION_OUTPUT */
struct frame_resolution {
	/* frame width of the input signal in pixels */
	u32 width;
	/* frame height of the input signal in pixels */
	u32 height;
};

struct vpu_frame_info {
	/* frame width and height of the input signal in pixels */
	struct frame_resolution resolution;
	/* enum vpu_pixel_format */
	u32	pixel_format;
	/* memory stride of planes of the supplied output buffers */
	u32	plane0_stride;
	u32	plane1_stride;
	u32	plane2_stride;
};

/* the input source */
#define INPUT_SOURCE_HOST	0
#define INPUT_SOURCE_VCAP	1

/* the output destination */
#define OUTPUT_DEST_NULL	0
#define OUTPUT_DEST_HOST	1
#define OUTPUT_DEST_MDSS	2

enum vpu_video_format {
	/* outcoming video stream is in 2D format */
	VIDEO_FORMAT_2D = 0,
	/* 2 half width frames packed horizontally */
	VIDEO_FORMAT_3D_SIDE_BY_SIDE = 1,
	/* 2 half heights images packed together vertically */
	VIDEO_FORMAT_3D_TOP_AND_BOTTOM = 2,

	VIDEO_FORMAT_NUM,
	VIDEO_FORMAT_MAX = 0x7fffffff
};

enum vpu_line_scan {
	/* scan mode for each line is set to be progressive */
	LINESCANPROGRESSIVE = 0,
	/* scan mode for each line is set to be interlacing */
	LINESCANINTERLACED = 1,

	LINESCANNUM,
	LINESCANMAX = 0x7fffffff
};

enum vpu_pixel_format {
	/* Interleaved RGB - BitsPerComponent is ARGB: 0 8 8 8 */
	PIXEL_FORMAT_RGB888 = 0x00,
	/* Interleaved RGB - BitsPerComponent is XRGB: 8 8 8 8 */
	PIXEL_FORMAT_XRGB8888 = 0x01,
	/* Interleaved RGB - BitsPerComponent is XRGB2: 2 10 10 10 */
	PIXEL_FORMAT_XRGB2 = 0x02,
	/* Interleaved BGR - BitsPerComponent is BGR: 8 8 8 */
	PIXEL_FORMAT_BGR888 = 0x03,
	/* Interleaved BGR - BitsPerComponent is BGRX: 8 8 8 8 */
	PIXEL_FORMAT_BGRX8888 = 0x04,
	/* Interleaved BGR - BitsPerComponent is XBGR2: 2 10 10 10 */
	PIXEL_FORMAT_XBGR2 = 0x05,
	/* YUV420 - YUV Semi-Planar (AYUV: 0 8 8 8) */
	PIXEL_FORMAT_NV12 = 0x06,
	/* YUV420 - YVU Semi-Planar (AYUV: 0 8 8 8) */
	PIXEL_FORMAT_NV21 = 0x07,
	/* YUV422 - YUYV Interleaved (AYUV: 0 8 8 8) */
	PIXEL_FORMAT_YUYV = 0x08,
	/* YUV422 - YVYU Interleaved (AYUV: 0 8 8 8) */
	PIXEL_FORMAT_YVYU = 0x09,
	/* YUV422 - VYUY Interleaved (AYUV: 0 8 8 8) */
	PIXEL_FORMAT_VYUY = 0x0a,
	/* YUV422 - UYVY Interleaved (AYUV: 0 8 8 8) */
	PIXEL_FORMAT_UYVY = 0x0b,
	/* YUV422 - 10 Bit Packed Loose (AYUV: 0 10 10 10) */
	PIXEL_FORMAT_YUYV10_LOOSE = 0x0c,
	/* YUV444 - YUV 8 Bit Interleaved Packed Dense (AYUV:0 8 8 8) */
	PIXEL_FORMAT_YUV_8BIT_INTERLEAVED_DENSE = 0x0d, /* not supported */
	/* YUV444 - YUV 10 Interleaved Bit Packed Loose (AYUV:0 10 10 10) */
	PIXEL_FORMAT_YUV_10BIT_INTERLEAVED_LOOSE = 0x0e, /* not supported */
	/* YUV422 - VPU BWC 10 bit Compressed */
	PIXEL_FORMAT_COMPRESSED_YUYV422 = 0x0f,

	PIXEL_FORMAT_NUM,
	PIXEL_FORMAT_MAX = 0x7fffffff
};

struct rect {
	u32	left;	/* x of the upper-left corner of the rectangle */
	u32	top;	/* y of the upper-left corner of the rectangle */
	u32	right;	/* x of the lower-right corner of the rectangle */
	u32	bottom; /* y of the lower-right corner of the rectangle */
};

/* input/output channel flags */
/* bit [0]: reserved. */
/* bit [1]: 1=content is secure, 0=content is non-secure */
#define VPU_CHANNEL_FLAG_SECURE_CONTENT             0x00000002
/* bit [2]: 1=MDSS Info PKT present (vpu_ipc_mdss_info_pkt), 0=not present */
#define VPU_CHANNEL_FLAG_MDSS_INFO_PKT              0x00000004
/* bits [31:3] reserved. */

/* If the MDSS_INFO_PKT flag is set, then this struct is included at the end of
 * vpu_prop_session_output struct
 */
struct vpu_ipc_mdss_info_pkt {
	u32	size; /* header + payload size. Variable length payload */
	u32	payload[1]; /* Variable length payload starting at payload[0] */
};

/* associated structure for VPU_PROP_SESSION_INPUT */
struct vpu_prop_session_input {
	/* frame info */
	struct vpu_frame_info frame_info;
	/* enum vpu_video_format, different supported formats for 2D and 3D */
	u32	video_format;
	/* the input signal is coming from VCAP hardware block or HOST
	 * (refer to INPUT_SOURCE_ defines)
	 */
	u32	input_source;
	/* enum vpu_line_scan: scan mode per line: progressive/interlaced */
	u32	scan_mode;
	/* frame rate of the input signal 16.16 fixed point */
	u32	frame_rate;
	/* region of interest on the input buffers */
	struct rect region_interest;
	/* see VPU_CHANNEL_FLAG_ definition */
	u32	flags;
};

/* associated structure for VPU_PROP_SESSION_OUTPUT */
struct vpu_prop_session_output {
	/* frame info */
	struct vpu_frame_info frame_info;
	/* enum vpu_video_format, different supported formats for 2D and 3D */
	u32	video_format;
	/* output frame sent to HOST or MDSS (refer to OUTPUT_DEST_ defines) */
	u32	output_dest;
	/* value represents the frame rate of the output signal */
	u32	frame_rate;
	/* destination region of interest for scaling on the output buffers */
	struct rect dest_rect;
	/* target region of interest on the output buffers */
	struct rect target_rect;
	/* see VPU_CHANNEL_FLAG_ definition */
	u32	flags;
};

/* associated structure for VPU_PROP_SESSION_STD_SETUP */
struct vpu_prop_session_std_setup {
	struct vpu_prop_session_input session_input;
	struct vpu_prop_session_output session_output;
};

/* values defined for VPU_PROP_SESSION_ROI */
#define RECT_TYPE_SRC	0
#define RECT_TYPE_DST	1
#define RECT_TYPE_TGT	2

/* associated structure for VPU_PROP_SESSION_ROI */
struct vpu_prop_session_roi {
	/* ROI rectangle type   */
	u32 rect_type;
	/* Rectangle definition */
	struct rect rect;
};

/* associated structure for VPU_PROP_SESSION_INPUT_FPS
 * associated structure for VPU_PROP_SESSION_OUTPUT_FPS
 * associated structure for VPU_PROP_SESSION_DISPLAY_FPS
 *
 * struct vpu_data_value:
 *       value = uint32 - frame rate in Q16 format. Q16 is unsigned 16.16 format
 *               (Float2Q = (float_num << 16) or (float_num * 2 ^ 16)
 *               (Q2FLOAT = (q_num >> 16) or (q_num / 2 ^ 16)
 *       flags = uint32 (reserved)
 */

/* data types defined for VPU_PROP_SESSION_BACKGROUND_COLOR */
struct vpu_color_rgb {
	/* red component of the specified color*/
	u8  red;
	/* green component of the specified color */
	u8  green;
	/* blue component of the specified color */
	u8  blue;
	/* alpha component of the specified color */
	u8  alpha;
};

/* associated structure for VPU_PROP_SESSION_BACKGROUND_COLOR
 * struct vpu_data_value:
 *       value = (uint32) vpu_color_rgb (refer to struct definition)
 *       flags = uint32 (reserved)
 */

/* values defined for VPU_PROP_SESSION_RANGE_MAPPING */
#define VPU_RANGE_MAPPING_MIN		0
#define VPU_RANGE_MAPPING_MAX		7
#define VPU_RANGE_MAPPING_STEP		1

/* associated structure for VPU_PROP_SESSION_RANGE_MAPPING */
struct vpu_prop_session_range_mapping {
	u32	enabled;
	/* the range mapping set for Y */
	u32	y_map_range;
	/* the range mapping set for UV */
	u32	uv_map_range;
};

enum vpu_color_space_config {
	CONFIG_RGB_RANGE = 0,
	CONFIG_NOMINAL_RANGE,
	CONFIG_YCBCR_MATRIX,
};

enum vpu_rgb_range {
	/* RGB values will range in the range from 0 to 255 */
	RGB_RANGE_FULL = 0,
	/* RGB values will range in the range from 16 to 235 */
	RGB_RANGE_LIMITED = 1,
};

enum vpu_ycbcr_matrix {
	YCbCr_MATRIX_BT709 = 0,
	YCbCr_MATRIX_BT601 = 1,
	YCbCr_MATRIX_BT240 = 2,
};

enum vpu_nominal_range {
	/* YUV values will range in the range from 0 to 255 */
	NOMINAL_RANGE_FULL = 0,
	/* YUV values will range in the range from 16 to 235 */
	NOMINAL_RANGE_LIMITED = 1,
};

/* associated structure for VPU_PROP_SESSION_COLOR_SPACE */
struct vpu_prop_session_color_space {
	/* selection of RGB range, YUV range, YCbCr matrix
	 * refer to enum vpu_color_space_config
	 */
	u32	cs_config;

	 /* value associated with cs_config.
	  * refer to associated value ranges:
	  * CONFIG_RGB_RANGE:      enum vpu_rgb_range
	  * CONFIG_NOMINAL_RANGE:  enum vpu_nominal_range
	  * CONFIG_YCBCR_MATRIX:   enum vpu_ycbcr_matrix
	  */
	u32	value;
};

/* values defined for VPU_PROP_SESSION_SOURCE_CONFIG */
#define VPU_SOURCE_VCAP_CH_0	1
#define VPU_SOURCE_VCAP_CH_1	2

/* values defined for VPU_PROP_SESSION_SINK_CONFIG */
#define VPU_DEST_MDSS_CH_0	1
#define VPU_DEST_MDSS_CH_1	2
#define VPU_DEST_MDSS_CH_2	4
#define VPU_DEST_MDSS_CH_3	8

/* associated structure for VPU_PROP_SESSION_SOURCE_CONFIG
 * struct vpu_data_value:
 *       value: uint32 (refer to defines). Bitmask specifying 1 or more channels
 *       flags: uint32 (reserved)
 */

/* values defined for VPU_PROP_SESSION_DEINTERLACING */

/* field polarity will be auto selected during deinterlacing */
#define	FIELD_POLARITY_AUTO	0
/* odd field to be used for scanning during deinterlacing */
#define	FIELD_POLARITY_EVEN	1
/* odd field to be used for scanning during deinterlacing */
#define	FIELD_POLARITY_ODD	2

/* auto: interpolation with film cadence detection is used */
#define MVP_MODE_AUTO		0
/* film: interpolation with film cadence detection and film pulldown is used */
#define MVP_MODE_FILM		1
/* video: interpolation without film cadence detection is used */
#define MVP_MODE_VIDEO		2

/* associated structure for VPU_PROP_SESSION_DEINTERLACING */
struct vpu_prop_session_deinterlacing {
	/* specifies which field (odd/even) to start with for deinterlacing.
	 * use auto if unknown or don't care.
	 * (refer to FIELD_POLARITY_ defines)
	 */
	u32	field_polarity;

	/* specifies the deinterlacing bias mode
	 * (refer to MVP_MODE_ defines)
	 */
	u32	mvp_mode;
};

/* values defined for VPU_PROP_SESSION_NOISE_REDUCTION */
/* Note: limits are different between auto and manual mode  */
/* PROP_MODE_MANUAL */
#define VPU_NOISE_REDUCTION_LEVEL_MIN		0
#define VPU_NOISE_REDUCTION_LEVEL_MAX		100
#define VPU_NOISE_REDUCTION_STEP		10
#define VPU_NOISE_REDUCTION_LEVEL_LOW		20 /* helper */
#define VPU_NOISE_REDUCTION_LEVEL_MED		50 /* helper */
#define VPU_NOISE_REDUCTION_LEVEL_HI		80 /* helper */
/* PROP_MODE_AUTO */
#define VPU_AUTO_NR_LEVEL_MIN			0
#define VPU_AUTO_NR_LEVEL_MAX			100
#define VPU_AUTO_NR_LEVEL_STEP			25
#define VPU_AUTO_NR_LEVEL_LOW			0  /* helper */
#define VPU_HFI_AUTO_NR_LEVEL_MILD		25 /* helper */
#define VPU_AUTO_NR_LEVEL_MED			50 /* helper */
#define VPU_AUTO_NR_LEVEL_HI			75 /* helper */
#define VPU_HFI_AUTO_NR_LEVEL_XHI		100 /* helper */

/* associated structure for VPU_PROP_SESSION_NOISE_REDUCTION
 * struct vpu_data_value:
 *       value: uint32 (refer to value limits)
 *       flags: uint32. PROP_MODE_ value (disabled/manual/auto)
 */

/* values defined for VPU_PROP_SESSION_ANAMORPHIC_SCALING */
#define VPU_ANAMORPHIC_SCALE_VALUE_MIN         0
#define VPU_ANAMORPHIC_SCALE_VALUE_MAX         100
#define VPU_ANAMORPHIC_SCALE_VALUE_STEP        1
#define VPU_ANAMORPHIC_SCALE_VALUE_DEF         67 /* helper */

/* associated structure for VPU_PROP_SESSION_ANAMORPHIC_SCALING
 * struct vpu_data_value:
 *       value: uint32 (refer to defines)
 *       flags: uint32. enabled flag. TRUE=enabled, FALSE=disabled
 *
 * Note: Anamorphic scaling is activated only when it is up scaling and
 *       Horizontal Scale ratio > Vertical Scale ratio, where:
 *       Horizontal Scale ratio = output  width / input width
 *       Vertical Scale ratio   = output height / input height
 */

/* associated structure for VPU_PROP_SESSION_BAR_DETECT
 * struct vpu_data_value:
 *       value: uint32 (reserved)
 *       flags: uint32. enabled flag. TRUE=enabled, FALSE=disabled
 */

/* associated structure for VPU_PROP_SESSION_ACTIVE_REGION_DETECT
 * struct vpu_prop_session_active_region_detect:
 *       enabled: TRUE=enabled, FALSE=disabled
 *       regon_of_interest: detection region
 *       nNumExclusionRegions: Number of exclusion region rects that follow
 *                             this struct
 */
struct vpu_prop_session_active_region_detect {
	u32		enabled;
	struct rect	region_of_interest;
	/* Number of exclusion region rects that follow this structure */
	u32		num_exclusion_regions;
};
/*
 * Overall property struct will appear as:
 * vpu_prop_session_active_region_detect  active_region_detect;
 * rect_t exclusion_region[num_exclusion_regions]  // 0 - n rectangles.
 */

/* associated structure for VPU_PROP_SESSION_NOISE_REDUCTION_CONFIG */
struct vpu_prop_session_noise_reduction_config {
	/* buffer address for temporal input buffer */
	u32	in_buf_addr;

	/* buffer address for temporal output buffer */
	u32	out_buf_addr;

	/* true:   NR buffers released (in/out addresses ignored).
	 * false:  NR buffer addresses set
	 */
	u32	release_flag;
};

/* values defined for VPU_PROP_SESSION_DETAIL_ENHANCEMENT */
#define VPU_DETAIL_ENHANCEMENT_VALUE_MIN	0
#define VPU_DETAIL_ENHANCEMENT_VALUE_MAX	100

/* associated structure for VPU_PROP_SESSION_DETAIL_ENHANCEMENT
 * struct vpu_data_value:
 *       value: uint32 (refer to defines)
 *       flags: uint32. enabled flag. TRUE=enabled, FALSE=disabled
 */

/* values defined for VPU_PROP_SESSION_DI */
#define VPU_DI_VALUE_MIN                0
#define VPU_DI_VALUE_MAX                100

/* associated structure for VPU_PROP_SESSION_DI
 * struct vpu_data_value:
 *       value: uint32 (refer to defines)
 *       flags: uint32. enabled flag. TRUE=enabled, FALSE=disabled
 */

/* values defined for VPU_PROP_SESSION_STI */
#define VPU_STI_VALUE_MIN               0
#define VPU_STI_VALUE_MAX               100

/* associated structure for VPU_PROP_SESSION_STI
 * struct vpu_data_value:
 *       value: uint32 (refer to defines)
 *       flags: uint32. enabled flag. TRUE=enabled, FALSE=disabled
 */

/* values defined for VPU_PROP_SESSION_IMAGE_ENHANCEMENT */
/* Note: limits are different between auto and manual mode  */
/* PROP_MODE_MANUAL */
#define VPU_IMAGE_ENHANCEMENT_LEVEL_MIN		(-100)
#define VPU_IMAGE_ENHANCEMENT_LEVEL_MAX		100
#define VPU_IMAGE_ENHANCEMENT_STEP		1
/* PROP_MODE_AUTO */
#define VPU_AUTO_IE_LEVEL_MIN			0
#define VPU_AUTO_IE_LEVEL_MAX			100
#define VPU_AUTO_IE_LEVEL_STEP			1

/* associated structure for VPU_PROP_SESSION_IMAGE_ENHANCEMENT
 * struct vpu_s_data_value:
 *       value: int32 (refer to defines)
 *       flags: uint32. PROP_MODE_ value (disabled/manual/auto)
 */

/* values defined for VPU_PROP_SESSION_AUTO_HQV_AUTONR_STRENGTH */
#define VPU_AUTONR_STRENGTH_MIN         0
#define VPU_AUTONR_STRENGTH_MAX         100

/* associated structure for VPU_PROP_SESSION_AUTO_HQV_AUTONR_STRENGTH
 * struct vpu_data_value:
 *       value = uint32 (refer to defines)
 *       flags = uint32 (reserved)
 */

/* values defined for VPU_PROP_SESSION_AUTO_HQV_SHARPEN_STRENGTH */
#define VPU_SHARPEN_STRENGTH_MIN        0
#define VPU_SHARPEN_STRENGTH_MAX        100

/* associated structure for VPU_PROP_SESSION_AUTO_HQV_SHARPEN_STRENGTH
 * struct vpu_data_value:
 *       value = uint32 (refer to defines)
 *       flags = uint32 (reserved)
 */

/* associated structure for VPU_PROP_SESSION_AUTO_HQV */
struct vpu_prop_session_auto_hqv {
	/* set enabled=TRUE to turn Auto HQV block on, enabled=FALSE to turn
	 * off Auto HQV
	 */
	u32 enabled;
	/* strength control of all sharpening features such as STI, DE,
	 * range [0..100]
	 */
	u32 sharpen_strength;
	/* strength control of Auto NR feature, range [0..100] */
	u32 auto_nr_strength;
};

/* associated structure for VPU_PROP_SESSION_ACE
 * struct vpu_data_value:
 *       value: uint32 (reserved)
 *       flags: uint32. enable flag. TRUE=enabled, FALSE=disabled
 */

/* values defined for VPU_PROP_SESSION_ACE_BRIGHTNESS */
#define VPU_ACE_BRIGHTNESS_MIN		-100
#define VPU_ACE_BRIGHTNESS_MAX		100
#define VPU_ACE_BRIGHTNESS_STEP		1
#define VPU_ACE_BRIGHTNESS_DEFAULT	0

/* associated structure for VPU_PROP_SESSION_ACE_BRIGHTNESS
 * struct vpu_s_data_value:
 *       value: int32 (refer to defined limits)
 *       flags: uint32. (reserved)
 */

/* values defined for VPU_PROP_SESSION_ACE_CONTRAST */
#define VPU_ACE_CONTRAST_MIN		-100
#define VPU_ACE_CONTRAST_MAX		100
#define VPU_ACE_CONTRAST_STEP		1
#define VPU_ACE_CONTRAST_DEFAULT	0

/* associated structure for VPU_PROP_SESSION_ACE_CONTRAST
 * struct vpu_s_data_value:
 *       value: int32 (refer to defined limits)
 *       flags: uint32. (reserved)
 */

/* associated structure for VPU_PROP_SESSION_2D3D
 * struct vpu_data_value:
 *       value: uint32 (reserved)
 *       flags: uint32. enable flag. TRUE=enabled, FALSE=disabled
 */

/* values defined for VPU_PROP_SESSION_2D3D_DEPTH */
#define VPU_2D3D_DEPTH_MIN		0
#define VPU_2D3D_DEPTH_MAX		100
#define VPU_2D3D_DEPTH_STEP		1

/* associated structure for VPU_PROP_SESSION_2D3D_DEPTH
 * struct vpu_data_value:
 *       value: uint32 (refer to defined limits)
 *       flags: uint32. (reserved)
 */

/* associated structure for VPU_PROP_SESSION_TIMESTAMP
 *	property to poll for the timestamp information of the last buffer sent
 *	to MDSS
 */
struct vpu_timestamp_info {
	u32 high;
	u32 low;
};

struct vpu_prop_session_timestamp {
	struct vpu_timestamp_info presentation;
	struct vpu_timestamp_info qtimer;
	u32 reserved;
};

/* associated structure for VPU_PROP_SESSION_TIMESTAMP_AUTO_MODE
 *	In auto mode, the timestamp is returned at the output frame frequency
 *	specified in value.
 *
 * struct vpu_data_value:
 *       value: uint32. frame frequency. 0=auto mode disabled
 *       flags: uint32. (reserved)
 */

/* associated structure for VPU_PROP_SESSION_FRC
 * struct vpu_data_value:
 *       value: uint32 (reserved)
 *       flags: uint32. enable flag. TRUE=enabled, FALSE=disabled
 */

/* values defined for VPU_PROP_SESSION_FRC_MOTION_SMOOTHNESS */
#define VPU_FRC_MOTION_SMOOTHNESS_MIN		0
#define VPU_FRC_MOTION_SMOOTHNESS_MAX		100
#define VPU_FRC_MOTION_SMOOTHNESS_STEP		1
#define VPU_FRC_MOTION_SMOOTHNESS_DEFAULT	0

/* associated structure for VPU_PROP_SESSION_FRC_MOTION_SMOOTHNESS
 * struct vpu_data_value:
 *       value: uint32 (refer to defined limits)
 *       flags: uint32. (reserved)
 */

/* values defined for VPU_PROP_SESSION_FRC_MOTION_CLEAR */
#define VPU_FRC_MOTION_CLEAR_MIN		0
#define VPU_FRC_MOTION_CLEAR_MAX		100
#define VPU_FRC_MOTION_CLEAR_STEP		1
#define VPU_FRC_MOTION_CLEAR_DEFAULT		0

/* associated structure for VPU_PROP_SESSION_FRC_MOTION_CLEAR
 * struct vpu_data_value:
 *       value: uint32 (refer to defined limits)
 *       flags: uint32. (reserved)
 */

/* associated structure for VPU_PROP_SESSION_FRC_EXTENDED
 *  struct vpu_data_pkt:
 *       size:       reserved
 *       payload[0]: reserved
 */

/* associated structure for VPU_PROP_SESSION_GENERIC
 *  struct vpu_data_pkt:
 *       size:       uint32 (total size of struct in bytes)
 *       payload[0]: uint32 (refer to VPU_PROP_SESSION_ defines)
 *       payload[1-n]: (optional additional data)
 */

#endif /* __H_VPU_PROP_H__ */

