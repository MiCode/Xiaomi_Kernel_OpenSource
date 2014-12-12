#ifndef __MSMB_PPROC_H
#define __MSMB_PPROC_H

#ifdef MSM_CAMERA_BIONIC
#include <sys/types.h>
#endif
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <linux/videodev2.h>
#include <linux/types.h>
#include <media/msmb_generic_buf_mgr.h>

/* Should be same as VIDEO_MAX_PLANES in videodev2.h */
#define MAX_PLANES VIDEO_MAX_PLANES

#define MAX_NUM_CPP_STRIPS 8
#define MSM_CPP_MAX_NUM_PLANES 3
#define MSM_CPP_MIN_FRAME_LENGTH 13
#define MSM_CPP_MAX_FRAME_LENGTH 2048
#define MSM_CPP_MAX_FW_NAME_LEN 32
#define MAX_FREQ_TBL 10

enum msm_cpp_frame_type {
	MSM_CPP_OFFLINE_FRAME,
	MSM_CPP_REALTIME_FRAME,
};

enum msm_vpe_frame_type {
	MSM_VPE_OFFLINE_FRAME,
	MSM_VPE_REALTIME_FRAME,
};

struct msm_cpp_frame_strip_info {
	uint32_t scale_v_en;
	uint32_t scale_h_en;

	uint32_t upscale_v_en;
	uint32_t upscale_h_en;

	int32_t src_start_x;
	uint32_t src_end_x;
	int32_t src_start_y;
	uint32_t src_end_y;

	uint32_t temporal_pad_bottom;
	uint32_t temporal_pad_top;
	uint32_t temporal_pad_right;
	uint32_t temporal_pad_left;

	int32_t temporal_src_start_x;
	uint32_t temporal_src_end_x;
	int32_t temporal_src_start_y;
	uint32_t temporal_src_end_y;

	/* Padding is required for upscaler because it does not
	 * pad internally like other blocks, also needed for rotation
	 * rotation expects all the blocks in the stripe to be the same size
	 * Padding is done such that all the extra padded pixels
	 * are on the right and bottom
	 */
	uint32_t pad_bottom;
	uint32_t pad_top;
	uint32_t pad_right;
	uint32_t pad_left;

	uint32_t v_init_phase;
	uint32_t h_init_phase;
	uint32_t h_phase_step;
	uint32_t v_phase_step;

	uint32_t spatial_denoise_crop_width_first_pixel;
	uint32_t spatial_denoise_crop_width_last_pixel;
	uint32_t spatial_denoise_crop_height_first_line;
	uint32_t spatial_denoise_crop_height_last_line;

	uint32_t sharpen_crop_height_first_line;
	uint32_t sharpen_crop_height_last_line;
	uint32_t sharpen_crop_width_first_pixel;
	uint32_t sharpen_crop_width_last_pixel;

	uint32_t temporal_denoise_crop_width_first_pixel;
	uint32_t temporal_denoise_crop_width_last_pixel;
	uint32_t temporal_denoise_crop_height_first_line;
	uint32_t temporal_denoise_crop_height_last_line;

	uint32_t prescaler_spatial_denoise_crop_width_first_pixel;
	uint32_t prescaler_spatial_denoise_crop_width_last_pixel;
	uint32_t prescaler_spatial_denoise_crop_height_first_line;
	uint32_t prescaler_spatial_denoise_crop_height_last_line;

	uint32_t state_crop_width_first_pixel;
	uint32_t state_crop_width_last_pixel;
	uint32_t state_crop_height_first_line;
	uint32_t state_crop_height_last_line;

	int32_t dst_start_x;
	uint32_t dst_end_x;
	int32_t dst_start_y;
	uint32_t dst_end_y;

	int32_t temporal_dst_start_x;
	uint32_t temporal_dst_end_x;
	int32_t temporal_dst_start_y;
	uint32_t temporal_dst_end_y;

	uint32_t input_bytes_per_pixel;
	uint32_t output_bytes_per_pixel;
	uint32_t temporal_bytes_per_pixel;

	uint32_t source_address[2];
	uint32_t destination_address[2];
	/* source_address[1] is used for CbCR planar
	 * to CbCr interleaved conversion
	 */
	uint32_t temporal_source_address[2];
	/* destination_address[1] is used for CbCr interleved
	 * to CbCr planar conversion
	 */
	uint32_t temporal_destination_address[2];
	uint32_t src_stride;
	uint32_t dst_stride;
	uint32_t rotate_270;
	uint32_t horizontal_flip;
	uint32_t vertical_flip;
	uint32_t scale_output_width;
	uint32_t scale_output_height;
	uint32_t spatial_denoise_crop_en;
	uint32_t sharpen_crop_en;
	uint32_t temporal_denoise_crop_en;
	uint32_t prescaler_spatial_denoise_crop_en;
	uint32_t state_crop_en;
};

struct msm_cpp_buffer_info_t {
	int32_t fd;
	uint32_t index;
	uint32_t offset;
	uint8_t native_buff;
	uint8_t processed_divert;
	uint32_t identity;
};

struct msm_cpp_stream_buff_info_t {
	uint32_t identity;
	uint32_t num_buffs;
	struct msm_cpp_buffer_info_t *buffer_info;
};

struct msm_cpp_frame_info_t {
	int32_t frame_id;
	struct timeval timestamp;
	uint32_t inst_id;
	uint32_t identity;
	uint32_t client_id;
	enum msm_cpp_frame_type frame_type;
	uint32_t num_strips;
	struct msm_cpp_frame_strip_info __user *strip_info;
	uint32_t msg_len;
	uint32_t *cpp_cmd_msg;
	int src_fd;
	int dst_fd;
	struct timeval in_time, out_time;
	void __user *cookie;
	int32_t *status;
	int32_t duplicate_output;
	uint32_t duplicate_identity;
	uint8_t we_disable;
	struct msm_cpp_buffer_info_t input_buffer_info;
	struct msm_cpp_buffer_info_t output_buffer_info[2];
	struct msm_cpp_buffer_info_t tnr_scratch_buffer_info[2];
	uint32_t reserved;
};

struct msm_cpp_pop_stream_info_t {
	int32_t frame_id;
	uint32_t identity;
};

struct cpp_hw_info {
	uint32_t cpp_hw_version;
	uint32_t cpp_hw_caps;
	unsigned long freq_tbl[MAX_FREQ_TBL];
	uint32_t freq_tbl_count;
};

struct msm_vpe_frame_strip_info {
	uint32_t src_w;
	uint32_t src_h;
	uint32_t dst_w;
	uint32_t dst_h;
	uint32_t src_x;
	uint32_t src_y;
	uint32_t phase_step_x;
	uint32_t phase_step_y;
	uint32_t phase_init_x;
	uint32_t phase_init_y;
};

struct msm_vpe_buffer_info_t {
	int32_t fd;
	uint32_t index;
	uint32_t offset;
	uint8_t native_buff;
	uint8_t processed_divert;
};

struct msm_vpe_stream_buff_info_t {
	uint32_t identity;
	uint32_t num_buffs;
	struct msm_vpe_buffer_info_t *buffer_info;
};

struct msm_vpe_frame_info_t {
	int32_t frame_id;
	struct timeval timestamp;
	uint32_t inst_id;
	uint32_t identity;
	uint32_t client_id;
	enum msm_vpe_frame_type frame_type;
	struct msm_vpe_frame_strip_info strip_info;
	unsigned long src_fd;
	unsigned long dst_fd;
	struct ion_handle *src_ion_handle;
	struct ion_handle *dest_ion_handle;
	unsigned long src_phyaddr;
	unsigned long dest_phyaddr;
	unsigned long src_chroma_plane_offset;
	unsigned long dest_chroma_plane_offset;
	struct timeval in_time, out_time;
	void *cookie;

	struct msm_vpe_buffer_info_t input_buffer_info;
	struct msm_vpe_buffer_info_t output_buffer_info;
};

struct msm_pproc_queue_buf_info {
	struct msm_buf_mngr_info buff_mgr_info;
	uint8_t is_buf_dirty;
};

struct msm_cpp_clock_settings_t {
	unsigned long clock_rate;
	uint64_t avg;
	uint64_t inst;
};

#define VIDIOC_MSM_CPP_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_GET_EVENTPAYLOAD \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 1, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_GET_INST_INFO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 2, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_LOAD_FIRMWARE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_GET_HW_INFO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_FLUSH_QUEUE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 5, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_ENQUEUE_STREAM_BUFF_INFO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 6, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_DEQUEUE_STREAM_BUFF_INFO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 7, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_VPE_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 8, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_VPE_TRANSACTION_SETUP \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 9, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_VPE_GET_EVENTPAYLOAD \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 10, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_VPE_GET_INST_INFO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 11, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_VPE_ENQUEUE_STREAM_BUFF_INFO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 12, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_VPE_DEQUEUE_STREAM_BUFF_INFO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 13, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_QUEUE_BUF \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 14, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_APPEND_STREAM_BUFF_INFO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 15, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_SET_CLOCK \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 16, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_POP_STREAM_BUFFER \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 17, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_IOMMU_ATTACH \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 18, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_IOMMU_DETACH \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 19, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_DELETE_STREAM_BUFF\
	_IOWR('V', BASE_VIDIOC_PRIVATE + 20, struct msm_camera_v4l2_ioctl_t)


#define V4L2_EVENT_CPP_FRAME_DONE  (V4L2_EVENT_PRIVATE_START + 0)
#define V4L2_EVENT_VPE_FRAME_DONE  (V4L2_EVENT_PRIVATE_START + 1)

struct msm_camera_v4l2_ioctl_t {
	uint32_t id;
	size_t len;
	int32_t trans_code;
	void __user *ioctl_ptr;
};

#ifdef CONFIG_COMPAT
struct msm_cpp_frame_info32_t {
	int32_t frame_id;
	struct compat_timeval timestamp;
	uint32_t inst_id;
	uint32_t identity;
	uint32_t client_id;
	enum msm_cpp_frame_type frame_type;
	uint32_t num_strips;
	compat_caddr_t strip_info;
	uint32_t msg_len;
	compat_uint_t cpp_cmd_msg;
	int src_fd;
	int dst_fd;
	struct compat_timeval in_time, out_time;
	compat_caddr_t cookie;
	compat_int_t status;
	int32_t duplicate_output;
	uint32_t duplicate_identity;
	uint8_t we_disable;
	struct msm_cpp_buffer_info_t input_buffer_info;
	struct msm_cpp_buffer_info_t output_buffer_info[2];
	struct msm_cpp_buffer_info_t tnr_scratch_buffer_info[2];
	uint32_t reserved;
};

struct msm_cpp_clock_settings32_t {
	compat_long_t clock_rate;
	uint64_t avg;
	uint64_t inst;
};

struct msm_cpp_stream_buff_info32_t {
	uint32_t identity;
	uint32_t num_buffs;
	compat_caddr_t buffer_info;
};

struct msm_pproc_queue_buf_info32_t {
	struct msm_buf_mngr_info32_t buff_mgr_info;
	uint8_t is_buf_dirty;
};

struct cpp_hw_info_32_t {
	uint32_t cpp_hw_version;
	uint32_t cpp_hw_caps;
	compat_long_t freq_tbl[MAX_FREQ_TBL];
	uint32_t freq_tbl_count;
};


#define VIDIOC_MSM_CPP_CFG32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_GET_EVENTPAYLOAD32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 1, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_GET_INST_INFO32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 2, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_LOAD_FIRMWARE32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_GET_HW_INFO32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_FLUSH_QUEUE32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 5, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_ENQUEUE_STREAM_BUFF_INFO32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 6, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_DEQUEUE_STREAM_BUFF_INFO32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 7, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_VPE_CFG32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 8, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_VPE_TRANSACTION_SETUP32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 9, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_VPE_GET_EVENTPAYLOAD32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 10, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_VPE_GET_INST_INFO32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 11, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_VPE_ENQUEUE_STREAM_BUFF_INFO32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 12, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_VPE_DEQUEUE_STREAM_BUFF_INFO32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 13, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_QUEUE_BUF32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 14, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_APPEND_STREAM_BUFF_INFO32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 15, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_SET_CLOCK32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 16, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_POP_STREAM_BUFFER32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 17, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_IOMMU_ATTACH32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 18, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_IOMMU_DETACH32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 19, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_DELETE_STREAM_BUFF32\
	_IOWR('V', BASE_VIDIOC_PRIVATE + 20, struct msm_camera_v4l2_ioctl32_t)

struct msm_camera_v4l2_ioctl32_t {
	uint32_t id;
	uint32_t len;
	int32_t trans_code;
	compat_caddr_t ioctl_ptr;
};
#endif

#endif /* __MSMB_PPROC_H */
