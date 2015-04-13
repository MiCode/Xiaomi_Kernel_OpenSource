#ifndef _H_MSM_VPU_H_
#define _H_MSM_VPU_H_

#include <linux/videodev2.h>

/*
 * V 4 L 2   E X T E N S I O N S   B Y   V P U
 */

/*
 * v4l2_buffer:
 *
 * VPU uses standard V4L2 buffer flags, and defines some custom
 * flags (used in v4l2_buffer.flags field):
 *	V4L2_QCOM_BUF_FLAG_EOS: buffer flag indicating end of stream
 *	V4L2_BUF_FLAG_CDS_ENABLE: buffer flag to enable chroma down-sampling
 */
#define V4L2_BUF_FLAG_CDS_ENABLE	0x10000000

/*
 * VPU uses multi-plane v4l2_buffer in the following manner:
 * each plane can be a separate ION buffer, or all planes are from the
 * same ION buffer (under this case all planes have the same fd, but different
 * offset).
 *
 * For struct v4l2_plane
 *   fd: ION fd representing the ION buffer this plane is from
 *   reserved[0]: offset of this plane from the start of the ION buffer in
 *		bytes. Needed when all planes are from the same ION buffer.
 */
#define V4L2_PLANE_MEM_OFFSET		0

/*
 * struct v4l2_format:
 * always use v4l2_pix_format_mplane, even when there is only one plane
 *
 * v4l2_pix_format_mplane:
 *
 * VPU uses v4l2_pix_format_mplane for pixel format configuration
 * The following members of this structure is either extended or changed:
 *    pixelformat: extended, a few more private formats added
 *    colorspace:  possible values are enum vpu_colorspace
 *    field: when it is V4L2_FIELD_ALTERNATE, flags from vpu format extension
 *           specifies which field first.
 *    reserved[]:  VPU format extension. struct v4l2_format_vpu_extension
 */
enum vpu_colorspace {
	VPU_CS_MIN = 0,
	/* RGB with full range*/
	VPU_CS_RGB_FULL = 1,
	/* RGB with limited range*/
	VPU_CS_RGB_LIMITED = 2,
	/* REC 601 with full range */
	VPU_CS_REC601_FULL = 3,
	/* REC 601 with limited range */
	VPU_CS_REC601_LIMITED = 4,
	/* REC 709 with full range */
	VPU_CS_REC709_FULL = 5,
	/* REC 709 with limited range */
	VPU_CS_REC709_LIMITED = 6,
	/* SMPTE 240 with full range */
	VPU_CS_SMPTE240_FULL = 7,
	/* SMPTE 240 with limited range */
	VPU_CS_SMPTE240_LIMITED = 8,
	VPU_CS_MAX = 9,
};


#define VPU_FMT_EXT_FLAG_BT	1	/* bottom field first */
#define VPU_FMT_EXT_FLAG_TB	2	/* top field first */
#define VPU_FMT_EXT_FLAG_3D	4	/* 3D format */
struct v4l2_format_vpu_extension {
	__u8		flag;
	__u8		gap_in_lines;
};

/*
 * Supported pixel formats:
 *
 * VPU supported pixel format fourcc codes (use in s_fmt pixelformat field).
 *	Can be enumerated using VIDIOC_ENUM_FMT
 *
 * Standard V4L2 formats, defined in videodev2.h :
 *
 * V4L2_PIX_FMT_RGB24		24 bit RGB-8-8-8
 * V4L2_PIX_FMT_RGB32		32 bit XRGB-8-8-8-8
 * V4L2_PIX_FMT_BGR24		24 bit BGR-8-8-8
 * V4L2_PIX_FMT_BGR32		32 bit BGRX-8-8-8-8
 *
 * V4L2_PIX_FMT_NV12		12 bit YUV 4:2:0  semi-planar NV12
 * V4L2_PIX_FMT_NV21		12 bit YUV 4:2:0  semi-planar NV21
 * V4L2_PIX_FMT_YUYV		16 bit YUYV 4:2:2 interleaved
 * V4L2_PIX_FMT_YVYU		16 bit YVYU 4:2:2 interleaved
 * V4L2_PIX_FMT_UYVY		16 bit UYVY 4:2:2 interleaved
 * V4L2_PIX_FMT_VYUY		16 bit VYUY 4:2:2 interleaved
 *
 *
 * Private VPU formats, defined here :
 *
 * V4L2_PIX_FMT_XRGB2		32 bit XRGB-2-10-10-10
 * V4L2_PIX_FMT_XBGR2		32 bit XBGR-2-10-10-10
 *
 * V4L2_PIX_FMT_YUYV10		24 bit YUYV 4:2:2  10 bit per component loose
 * V4L2_PIX_FMT_YUV8		24 bit YUV 4:4:4   8 bit per component
 * V4L2_PIX_FMT_YUV10		32 bit YUV 4:4:4   10 bit per component loose
 * V4L2_PIX_FMT_YUYV10BWC	10 bit YUYV 4:2:2  compressed, for output only
 */
#define V4L2_PIX_FMT_XRGB2		v4l2_fourcc('X', 'R', 'G', '2')
#define V4L2_PIX_FMT_XBGR2		v4l2_fourcc('X', 'B', 'G', '2')
#define V4L2_PIX_FMT_YUYV10		v4l2_fourcc('Y', 'U', 'Y', 'L')
#define V4L2_PIX_FMT_YUV8		v4l2_fourcc('Y', 'U', 'V', '8')
#define V4L2_PIX_FMT_YUV10		v4l2_fourcc('Y', 'U', 'V', 'L')
#define V4L2_PIX_FMT_YUYV10BWC		v4l2_fourcc('Y', 'B', 'W', 'C')

/*
 * VIDIOC_S_INPUT/VIDIOC_S_OUTPUT
 *
 * The single integer passed by these commands specifies port type in the
 * lower 16 bits, and pipe bit mask in the higher 16 bits.
 */
/* input / output types */
#define VPU_INPUT_TYPE_HOST			0
#define VPU_INPUT_TYPE_VCAP			1
#define VPU_OUTPUT_TYPE_HOST			0
#define VPU_OUTPUT_TYPE_DISPLAY			1

/* input / output pipe bit fields */
#define VPU_PIPE_VCAP0			(1 << 16)
#define VPU_PIPE_VCAP1			(1 << 17)
#define VPU_PIPE_DISPLAY0		(1 << 18)
#define VPU_PIPE_DISPLAY1		(1 << 19)
#define VPU_PIPE_DISPLAY2		(1 << 20)
#define VPU_PIPE_DISPLAY3		(1 << 21)

/*
 * V P U   E V E N T S :   I D s   A N D   D A T A   P A Y L O A D S
 */

/*
 * Event ID: set in type field of struct v4l2_event
 * payload: returned in u.data array of struct v4l2_event
 *
 *
 * VPU_EVENT_FLUSH_DONE: Done flushing buffers after VPU_FLUSH_BUFS ioctl
 * payload data: enum v4l2_buf_type (buffer type of flushed port)
 *
 * VPU_EVENT_ACTIVE_REGION_CHANGED: New Active Region Detected
 * payload data: struct v4l2_rect (new active region rectangle)
 *
 * VPU_EVENT_SESSION_TIMESTAMP: New Session timestamp
 * payload data: vpu_frame_timestamp_info
 *
 * VPU_EVENT_SESSION_CREATED: New session has been created
 * payload data: int (number of the attached session)
 *
 * VPU_EVENT_SESSION_FREED: Session is detached and free
 * payload data: int (number of the detached session)
 *
 * VPU_EVENT_SESSION_CLIENT_EXITED: Indicates that clients of current
 *	session have exited.
 * payload data: int (number of all remaining clients for this session)
 *
 * VPU_EVENT_HW_ERROR: a hardware error occurred in VPU
 * payload data: NULL
 *
 * VPU_EVENT_INVALID_CONFIG: invalid VPU session configuration
 * payload data: NULL
 *
 * VPU_EVENT_FAILED_SESSION_STREAMING: Failed to stream session
 * payload data: NULL
 */
#define VPU_PRIVATE_EVENT_BASE (V4L2_EVENT_PRIVATE_START + 6 * 1000)
enum VPU_PRIVATE_EVENT {
	VPU_EVENT_START = VPU_PRIVATE_EVENT_BASE,

	VPU_EVENT_FLUSH_DONE = VPU_EVENT_START + 1,
	VPU_EVENT_ACTIVE_REGION_CHANGED = VPU_EVENT_START + 2,
	VPU_EVENT_SESSION_TIMESTAMP = VPU_EVENT_START + 3,
	VPU_EVENT_SESSION_CREATED = VPU_EVENT_START + 4,
	VPU_EVENT_SESSION_FREED = VPU_EVENT_START + 5,
	VPU_EVENT_SESSION_CLIENT_EXITED = VPU_EVENT_START + 6,

	VPU_EVENT_HW_ERROR = VPU_EVENT_START + 11,
	VPU_EVENT_INVALID_CONFIG = VPU_EVENT_START + 12,
	VPU_EVENT_FAILED_SESSION_STREAMING = VPU_EVENT_START + 13,

	VPU_EVENT_END
};


/*
 * V P U   CO N T R O L S :   S T R U C T S   A N D   I D s
 *
 * Controls are video processing parameters
 */

/*
 * Standard VPU Controls
 */
struct vpu_ctrl_standard {
	__u32 enable;		/* boolean: 0=disable, else=enable */
	__s32 value;
};

struct vpu_ctrl_auto_manual {
	__u32 enable;		/* boolean: 0=disable, else=enable */
	__u32 auto_mode;	/* boolean: 0=manual, else=automatic */
	__s32 value;
};

struct vpu_ctrl_range_mapping {
	__u32 enable;		/* boolean: 0=disable, else=enable */
	__u32 y_range;		/* the range mapping set for Y [0, 7] */
	__u32 uv_range;		/* the range mapping set for UV [0, 7] */
};

#define VPU_ACTIVE_REGION_N_EXCLUSIONS 1
struct vpu_ctrl_active_region_param {
	__u32               enable; /* boolean: 0=disable, else=enable */
	/* number of exclusion regions */
	__u32               num_exclusions;
	/* roi where active region detection is applied */
	struct v4l2_rect    detection_region;
	/* roi(s) excluded from active region detection*/
	struct v4l2_rect    excluded_regions[VPU_ACTIVE_REGION_N_EXCLUSIONS];
};

struct vpu_ctrl_deinterlacing_mode {
	__u32 field_polarity;
	__u32 mvp_mode;
};

struct vpu_ctrl_hqv {
	__u32 enable;
	/* strength control of all sharpening features [0, 100] */
	__u32 sharpen_strength;
	/* strength control of Auto NR feature [0, 100] */
	__u32 auto_nr_strength;
};

struct vpu_info_frame_timestamp {
	/* presentation timestamp of the frame */
	__u32 pts_low;
	__u32 pts_high;
	/* qtimer snapshot */
	__u32 qtime_low;
	__u32 qtime_high;
};

struct vpu_control {
	__u32 control_id;
	union control_data {
		__s32 value;
		struct vpu_ctrl_standard standard;
		struct vpu_ctrl_auto_manual auto_manual;
		struct vpu_ctrl_range_mapping range_mapping;
		struct vpu_ctrl_active_region_param active_region_param;
		struct v4l2_rect active_region_result;
		struct vpu_ctrl_deinterlacing_mode deinterlacing_mode;
		struct vpu_ctrl_hqv hqv;
		struct vpu_info_frame_timestamp timestamp;
		__u8 reserved[124];
	} data;
};

/*
 * IDs for standard controls (use in control_id field of struct vpu_control)
 *
 * VPU_CTRL_NOISE_REDUCTION: noise reduction level, data: auto_manual,
 * value: [0, 100] (step in increments of 25).
 *
 * VPU_CTRL_IMAGE_ENHANCEMENT: image enhancement level, data: auto_manual,
 * value: [-100, 100] (step in increments of 1).
 *
 * VPU_CTRL_ANAMORPHIC_SCALING: anamorphic scaling config, data: standard,
 * value: [0, 100] (step in increments of 1).
 *
 * VPU_CTRL_DIRECTIONAL_INTERPOLATION: directional interpolation config
 * data: standard, value: [0, 100] (step in increments of 1).
 *
 * VPU_CTRL_BACKGROUND_COLOR: , data: value,
 * value: red[0:7] green[8:15] blue[16:23] alpha[24:31]
 *
 * VPU_CTRL_RANGE_MAPPING: Y/UV range mapping, data: range_mapping,
 * y_range: [0, 7], uv_range: [0, 7] (step in increments of 1).
 *
 * VPU_CTRL_DEINTERLACING_MODE: deinterlacing mode, data: deinterlacing_mode,
 * field_polarity: [0, 2], mvp_mode: [0, 2] (step in increments of 1).
 *
 * VPU_CTRL_ACTIVE_REGION_PARAM: active region detection parameters (set only)
 * data: active_region_param,
 *
 * VPU_CTRL_ACTIVE_REGION_RESULT: detected active region roi (get only)
 * data: active_region_result
 *
 * VPU_CTRL_PRIORITY: Session priority, data: value,
 * value: high 100, normal 50
 *
 * VPU_CTRL_CONTENT_PROTECTION: input content protection status, data: value,
 * value: secure 1, non-secure 0
 *
 * VPU_CTRL_DISPLAY_REFRESH_RATE: display refresh rate (set only)
 * data: value (set to __u32 16.16 format)
 *
 * VPU_CTRL_HQV: hqv block config, data: hqv,
 * sharpen_strength: [0, 100] (step in increments of 25),
 * auto_nr_strength: [0, 100] (step in increments of 1).
 *
 * VPU_CTRL_HQV_SHARPEN: , data: value,
 * sharpen_strength: [0, 100] (step in increments of 1).
 *
 * VPU_CTRL_HQV_AUTONR: , data: value,
 * auto_nr_strength: [0, 100] (step in increments of 1).
 *
 * VPU_CTRL_ACE: , data: value
 *
 * VPU_CTRL_ACE_BRIGHTNESS: , data: value,
 * value: [-100, 100] (step in increments of 1).
 *
 * VPU_CTRL_ACE_CONTRAST: , data: value,
 * value: [-100, 100] (step in increments of 1).
 *
 * VPU_CTRL_2D3D: , data: value,
 * value: 1 enabled, 0 disabled
 *
 * VPU_CTRL_2D3D_DEPTH: , data: value,
 * value: [0, 100] (step in increments of 1).
 *
 * VPU_CTRL_TIMESTAMP_INFO_MODE: timestamp reporting mode,
 *  data: value specifying how frequent a timestamp reporting info, value
 *  is in frames
 *
 * VPU_INFO_TIMESTAMP: timestamp information (get only)
 *  data: struct vpu_frame_timestamp_info
 *
 * VPU_CTRL_FRC: enable/disable FRC, data: value,
 * value: 1 enable, 0 disable
 *
 * VPU_CTRL_FRC_MOTION_SMOOTHNESS: , data: value,
 * value: [0, 100] (step in increments of 1).
 *
 * VPU_CTRL_FRC_MOTION_CLEAR: , data: value,
 * value: [0, 100] (step in increments of 1).
 *
 * VPU_CTRL_LATENCY: session latency, data: value in us
 *
 * VPU_CTRL_LATENCY_MODE: data: value (ultra low, low, etc.)
 *
 * VPU_INFO_STATISTICS: frames dropped, etc (get only),
 *  data: reserved
 */
#define VPU_CTRL_ID_MIN						0

#define VPU_CTRL_NOISE_REDUCTION				1
#define VPU_CTRL_IMAGE_ENHANCEMENT				2
#define VPU_CTRL_ANAMORPHIC_SCALING				3
#define VPU_CTRL_DIRECTIONAL_INTERPOLATION			4
#define VPU_CTRL_BACKGROUND_COLOR				5
#define VPU_CTRL_RANGE_MAPPING					6
#define VPU_CTRL_DEINTERLACING_MODE				7
#define VPU_CTRL_ACTIVE_REGION_PARAM				8
#define VPU_CTRL_ACTIVE_REGION_RESULT				9
#define VPU_CTRL_PRIORITY					10
#define VPU_CTRL_CONTENT_PROTECTION				11
#define VPU_CTRL_DISPLAY_REFRESH_RATE				12

#define VPU_CTRL_HQV						20
#define VPU_CTRL_HQV_SHARPEN					21
#define VPU_CTRL_HQV_AUTONR					22
#define VPU_CTRL_ACE						23
#define VPU_CTRL_ACE_BRIGHTNESS					24
#define VPU_CTRL_ACE_CONTRAST					25
#define VPU_CTRL_2D3D						26
#define VPU_CTRL_2D3D_DEPTH					27
#define VPU_CTRL_FRC						28
#define VPU_CTRL_FRC_MOTION_SMOOTHNESS				29
#define VPU_CTRL_FRC_MOTION_CLEAR				30

#define VPU_INFO_TIMESTAMP					35
#define VPU_CTRL_TIMESTAMP_INFO_MODE				36
#define VPU_INFO_STATISTICS					37
#define VPU_CTRL_LATENCY					38
#define VPU_CTRL_LATENCY_MODE					39

#define VPU_CTRL_ID_MAX						40


/*
 * Extended VPU Controls (large data payloads)
 */
#define VPU_MAX_EXT_DATA_SIZE	720
struct vpu_control_extended {
	/*
	 * extended control type
	 * 0: system
	 * 1: session
	 */
	__u32 type;

	/*
	 * size and ptr of the data to send
	 * maximum VPU_MAX_EXT_DATA_SIZE bytes
	 */
	__u32 data_len;
	void __user *data_ptr;

	/*
	 * size and ptr of the buffer to recv data
	 * maximum VPU_MAX_EXT_DATA_SIZE bytes
	 */
	__u32 buf_size;
	void __user *buf_ptr;
};

/*
 * Port specific controls
 */
struct vpu_control_port {
	__u32 control_id;
	__u32 port;	/* 0: INPUT, 1: OUTPUT */
	union control_port_data {
		__u32 framerate;
	} data;
};

/*
 * IDs for port controls (use in control_id field of struct vpu_control_port)
 *
 * VPU_CTRL_FPS: set frame rate, data: __u32, 16.16 format
 */
#define	VPU_CTRL_FPS				1000


/*
 * V P U   D E V I C E   P R I V A T E   I O C T L   C O D E S
 */

/* VPU Session ioctls (deprecated) */
#define VPU_ATTACH_TO_SESSION	_IOW('V', (BASE_VIDIOC_PRIVATE + 1), int)

/* VPU Session ioctls */
#define VPU_QUERY_SESSIONS	_IOR('V', (BASE_VIDIOC_PRIVATE + 0), int)
#define VPU_CREATE_SESSION	_IOR('V', (BASE_VIDIOC_PRIVATE + 2), int)
#define VPU_JOIN_SESSION	_IOW('V', (BASE_VIDIOC_PRIVATE + 3), int)

/* Enable second VPU output port and use with current client */
#define VPU_CREATE_OUTPUT2	_IO('V', (BASE_VIDIOC_PRIVATE + 5))

/* Explicit commit of session configuration */
#define VPU_COMMIT_CONFIGURATION    _IO('V', (BASE_VIDIOC_PRIVATE + 10))

/* Flush all buffers of given type (port) */
#define VPU_FLUSH_BUFS		_IOW('V', (BASE_VIDIOC_PRIVATE + 15), \
		enum v4l2_buf_type)

/* VPU controls get/set ioctls (for most controls with small data) */
#define VPU_G_CONTROL		_IOWR('V', (BASE_VIDIOC_PRIVATE + 20), \
						struct vpu_control)
#define VPU_S_CONTROL		_IOW('V', (BASE_VIDIOC_PRIVATE + 21), \
						struct vpu_control)

/* extended control set/get ioctls (large data payloads) */
#define VPU_G_CONTROL_EXTENDED	_IOWR('V', (BASE_VIDIOC_PRIVATE + 22), \
		struct vpu_control_extended)
#define VPU_S_CONTROL_EXTENDED	_IOW('V', (BASE_VIDIOC_PRIVATE + 23), \
		struct vpu_control_extended)

/* VPU port (input/output) specific controls get/set ioctls */
#define VPU_G_CONTROL_PORT	_IOWR('V', (BASE_VIDIOC_PRIVATE + 24), \
						struct vpu_control_port)
#define VPU_S_CONTROL_PORT	_IOW('V', (BASE_VIDIOC_PRIVATE + 25), \
						struct vpu_control_port)

#endif /* _H_MSM_VPU_H_ */

