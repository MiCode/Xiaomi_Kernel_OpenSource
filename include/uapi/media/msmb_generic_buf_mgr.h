#ifndef __UAPI_MEDIA_MSMB_GENERIC_BUF_MGR_H__
#define __UAPI_MEDIA_MSMB_GENERIC_BUF_MGR_H__

#include <media/msmb_camera.h>

enum msm_camera_buf_mngr_cmd {
	MSM_CAMERA_BUF_MNGR_CONT_MAP,
	MSM_CAMERA_BUF_MNGR_CONT_UNMAP,
	MSM_CAMERA_BUF_MNGR_CONT_MAX,
};

enum msm_camera_buf_mngr_buf_type {
	MSM_CAMERA_BUF_MNGR_BUF_PLANAR,
	MSM_CAMERA_BUF_MNGR_BUF_USER,
	MSM_CAMERA_BUF_MNGR_BUF_INVALID,
};

struct msm_buf_mngr_info {
	uint32_t session_id;
	uint32_t stream_id;
	uint32_t frame_id;
	struct timeval timestamp;
	uint32_t index;
	uint32_t reserved;
	enum msm_camera_buf_mngr_buf_type type;
	struct msm_camera_user_buf_cont_t user_buf;
};

struct msm_buf_mngr_main_cont_info {
	uint32_t session_id;
	uint32_t stream_id;
	enum msm_camera_buf_mngr_cmd cmd;
	uint32_t cnt;
	int32_t cont_fd;
};

#define MSM_CAMERA_BUF_MNGR_IOCTL_ID_BASE 0
#define MSM_CAMERA_BUF_MNGR_IOCTL_ID_GET_BUF_BY_IDX 1

#define VIDIOC_MSM_BUF_MNGR_GET_BUF \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 33, struct msm_buf_mngr_info)

#define VIDIOC_MSM_BUF_MNGR_PUT_BUF \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 34, struct msm_buf_mngr_info)

#define VIDIOC_MSM_BUF_MNGR_BUF_DONE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 35, struct msm_buf_mngr_info)

#define VIDIOC_MSM_BUF_MNGR_CONT_CMD \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 36, struct msm_buf_mngr_main_cont_info)

#define VIDIOC_MSM_BUF_MNGR_INIT \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 37, struct msm_buf_mngr_info)

#define VIDIOC_MSM_BUF_MNGR_DEINIT \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 38, struct msm_buf_mngr_info)

#define VIDIOC_MSM_BUF_MNGR_FLUSH \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 39, struct msm_buf_mngr_info)

#define VIDIOC_MSM_BUF_MNGR_IOCTL_CMD \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 40, \
	struct msm_camera_private_ioctl_arg)

#define VIDIOC_MSM_BUF_MNGR_BUF_ERROR \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 41, struct msm_buf_mngr_info)
#endif

