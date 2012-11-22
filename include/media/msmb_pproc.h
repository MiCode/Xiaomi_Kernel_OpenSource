#ifndef __MSMB_PPROC_H
#define __MSMB_PPROC_H

#ifdef MSM_CAMERA_BIONIC
#include <sys/types.h>
#endif
#include <linux/videodev2.h>
#include <linux/types.h>

/* Should be same as VIDEO_MAX_PLANES in videodev2.h */
#define MAX_PLANES VIDEO_MAX_PLANES

#define MAX_NUM_CPP_STRIPS 8

enum msm_cpp_frame_type {
	MSM_CPP_OFFLINE_FRAME,
	MSM_CPP_REALTIME_FRAME,
};

struct msm_cpp_frame_strip_info {
	int scale_v_en;
	int scale_h_en;

	int upscale_v_en;
	int upscale_h_en;

	int src_start_x;
	int src_end_x;
	int src_start_y;
	int src_end_y;

	/* Padding is required for upscaler because it does not
	 * pad internally like other blocks, also needed for rotation
	 * rotation expects all the blocks in the stripe to be the same size
	 * Padding is done such that all the extra padded pixels
	 * are on the right and bottom
	 */
	int pad_bottom;
	int pad_top;
	int pad_right;
	int pad_left;

	int v_init_phase;
	int h_init_phase;
	int h_phase_step;
	int v_phase_step;

	int prescale_crop_width_first_pixel;
	int prescale_crop_width_last_pixel;
	int prescale_crop_height_first_line;
	int prescale_crop_height_last_line;

	int postscale_crop_height_first_line;
	int postscale_crop_height_last_line;
	int postscale_crop_width_first_pixel;
	int postscale_crop_width_last_pixel;

	int dst_start_x;
	int dst_end_x;
	int dst_start_y;
	int dst_end_y;

	int bytes_per_pixel;
	unsigned int source_address;
	unsigned int destination_address;
	unsigned int src_stride;
	unsigned int dst_stride;
	int rotate_270;
	int horizontal_flip;
	int vertical_flip;
	int scale_output_width;
	int scale_output_height;
	int prescale_crop_en;
	int postscale_crop_en;
};

struct msm_cpp_frame_info_t {
	int32_t frame_id;
	uint32_t inst_id;
	uint32_t client_id;
	enum msm_cpp_frame_type frame_type;
	uint32_t num_strips;
	struct msm_cpp_frame_strip_info *strip_info;
	uint32_t msg_len;
	uint32_t *cpp_cmd_msg;
	int src_fd;
	int dst_fd;
};

struct msm_ver_num_info {
	uint32_t main;
	uint32_t minor;
	uint32_t rev;
};

#define VIDIOC_MSM_CPP_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_GET_EVENTPAYLOAD \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 1, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_GET_INST_INFO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 2, struct msm_camera_v4l2_ioctl_t)

#define V4L2_EVENT_CPP_FRAME_DONE  (V4L2_EVENT_PRIVATE_START + 0)

struct msm_camera_v4l2_ioctl_t {
	uint32_t id;
	uint32_t len;
	uint32_t trans_code;
	void __user *ioctl_ptr;
};

#endif /* __MSMB_PPROC_H */
