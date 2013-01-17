#ifndef VCAP_FMT_H
#define VCAP_FMT_H
#include <linux/videodev2.h>

#define V4L2_BUF_TYPE_INTERLACED_IN_DECODER (V4L2_BUF_TYPE_PRIVATE)

#define VCAP_GENERIC_NOTIFY_EVENT 0
#define VCAP_VC_PIX_ERR_EVENT 1
#define VCAP_VC_LINE_ERR_EVENT 2
#define VCAP_VC_VSYNC_ERR_EVENT 3
#define VCAP_VC_NPL_OFLOW_ERR_EVENT 4
#define VCAP_VC_LBUF_OFLOW_ERR_EVENT 5
#define VCAP_VC_BUF_OVERWRITE_EVENT 6
#define VCAP_VC_VSYNC_SEQ_ERR 7
#define VCAP_VP_REG_R_ERR_EVENT 8
#define VCAP_VP_REG_W_ERR_EVENT 9
#define VCAP_VP_IN_HEIGHT_ERR_EVENT 10
#define VCAP_VP_IN_WIDTH_ERR_EVENT 11
#define VCAP_VC_UNEXPECT_BUF_DONE 12
#define VCAP_MAX_NOTIFY_EVENT 13

enum hal_vcap_mode {
	HAL_VCAP_MODE_PRO = 0,
	HAL_VCAP_MODE_INT,
};

enum hal_vcap_polar {
	HAL_VCAP_POLAR_POS = 0,
	HAL_VCAP_POLAR_NEG,
};

enum hal_vcap_color {
	HAL_VCAP_YUV = 0,
	HAL_VCAP_RGB,
};

enum nr_threshold_mode {
	NR_THRESHOLD_STATIC = 0,
	NR_THRESHOLD_DYNAMIC,
};

enum nr_mode {
	NR_DISABLE = 0,
	NR_AUTO,
	NR_MANUAL,
};

enum nr_decay_ratio {
	NR_Decay_Ratio_26 = 0,
	NR_Decay_Ratio_25,
	NR_Decay_Ratio_24,
	NR_Decay_Ratio_23,
	NR_Decay_Ratio_22,
	NR_Decay_Ratio_21,
	NR_Decay_Ratio_20,
	NR_Decay_Ratio_19,
};

struct nr_config {
	uint8_t max_blend_ratio;
	uint8_t scale_diff_ratio;
	uint8_t diff_limit_ratio;
	uint8_t scale_motion_ratio;
	uint8_t blend_limit_ratio;
};

struct nr_param {
	enum nr_threshold_mode threshold;
	enum nr_mode mode;
	enum nr_decay_ratio decay_ratio;
	uint8_t window;
	struct nr_config luma;
	struct nr_config chroma;
};

#define VCAPIOC_NR_S_PARAMS _IOWR('V', (BASE_VIDIOC_PRIVATE+0), struct nr_param)

#define VCAPIOC_NR_G_PARAMS _IOWR('V', (BASE_VIDIOC_PRIVATE+1), struct nr_param)
#define VCAPIOC_S_NUM_VC_BUF _IOWR('V', (BASE_VIDIOC_PRIVATE+2), int)

struct v4l2_format_vc_ext {
	enum hal_vcap_mode     mode;
	enum hal_vcap_polar    h_polar;
	enum hal_vcap_polar    v_polar;
	enum hal_vcap_polar    d_polar;
	enum hal_vcap_color    color_space;

	uint32_t clk_freq;
	uint32_t frame_rate;
	uint32_t vtotal;
	uint32_t htotal;
	uint32_t hactive_start;
	uint32_t hactive_end;
	uint32_t vactive_start;
	uint32_t vactive_end;
	uint32_t vsync_start;
	uint32_t vsync_end;
	uint32_t hsync_start;
	uint32_t hsync_end;
	uint32_t f2_vactive_start;
	uint32_t f2_vactive_end;
	uint32_t f2_vsync_h_start;
	uint32_t f2_vsync_h_end;
	uint32_t f2_vsync_v_start;
	uint32_t f2_vsync_v_end;
	uint32_t sizeimage;
	uint32_t bytesperline;
};

enum vcap_type {
	VC_TYPE,
	VP_IN_TYPE,
	VP_OUT_TYPE,
};

enum vcap_stride {
	VC_STRIDE_16,
	VC_STRIDE_32,
};

struct vcap_priv_fmt {
	enum vcap_type type;
	enum vcap_stride stride;
	union {
		struct v4l2_format_vc_ext timing;
		struct v4l2_pix_format pix;
		/* Once VP is created there will be another type in here */
	} u;
};
#endif
