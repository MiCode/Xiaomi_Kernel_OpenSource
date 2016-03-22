#ifndef __UAPI_MSMB_PPROC_H
#define __UAPI_MSMB_PPROC_H

#include <linux/videodev2.h>
#include <linux/types.h>
#include <media/msmb_generic_buf_mgr.h>

/* Should be same as VIDEO_MAX_PLANES in videodev2.h */
#define MAX_PLANES VIDEO_MAX_PLANES
/* PARTIAL_FRAME_STRIPE_COUNT must be even */
#define PARTIAL_FRAME_STRIPE_COUNT 4

#define MAX_NUM_CPP_STRIPS 8
#define MSM_CPP_MAX_NUM_PLANES 3
#define MSM_CPP_MIN_FRAME_LENGTH 13
#define MSM_CPP_MAX_FRAME_LENGTH 4096
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

enum msm_cpp_batch_mode_t {
	BATCH_MODE_NONE,
	BATCH_MODE_VIDEO,
	BATCH_MODE_PREVIEW
};

struct msm_cpp_batch_info_t {
	enum msm_cpp_batch_mode_t  batch_mode;
	uint32_t batch_size;
	uint32_t intra_plane_offset[MAX_PLANES];
	uint32_t pick_preview_idx;
	uint32_t cont_idx;
};

struct msm_cpp_frame_info_t {
	int32_t frame_id;
	struct timeval timestamp;
	uint32_t inst_id;
	uint32_t identity;
	uint32_t client_id;
	enum msm_cpp_frame_type frame_type;
	uint32_t num_strips;
	uint32_t msg_len;
	uint32_t *cpp_cmd_msg;
	int src_fd;
	int dst_fd;
	struct timeval in_time, out_time;
	void __user *cookie;
	int32_t *status;
	int32_t duplicate_output;
	uint32_t duplicate_identity;
	uint32_t feature_mask;
	uint8_t we_disable;
	struct msm_cpp_buffer_info_t input_buffer_info;
	struct msm_cpp_buffer_info_t output_buffer_info[8];
	struct msm_cpp_buffer_info_t duplicate_buffer_info;
	struct msm_cpp_buffer_info_t tnr_scratch_buffer_info[2];
	uint32_t reserved;
	uint8_t partial_frame_indicator;
	/* the followings are used only for partial_frame type
	 * and is only used for offline frame processing and
	 * only if payload big enough and need to be split into partial_frame
	 * if first_payload, kernel acquires output buffer
	 * first payload must have the last stripe
	 * buffer addresses from 0 to last_stripe_index are updated.
	 * kernel updates payload with msg_len and stripe_info
	 * kernel sends top level, plane level, then only stripes
	 * starting with first_stripe_index and
	 * ends with last_stripe_index
	 * kernel then sends trailing flag at frame done,
	 * if last payload, kernel queues the output buffer to HAL
	 */
	uint8_t first_payload;
	uint8_t last_payload;
	uint32_t first_stripe_index;
	uint32_t last_stripe_index;
	uint32_t stripe_info_offset;
	uint32_t stripe_info;
	struct msm_cpp_batch_info_t  batch_info;
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

#endif

