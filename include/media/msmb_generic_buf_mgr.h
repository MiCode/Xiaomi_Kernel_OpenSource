#ifndef __MEDIA_MSMB_BUF_MNGR_H__
#define __MEDIA_MSMB_BUF_MNGR_H__

struct msm_buf_mngr_info {
	uint32_t session_id;
	uint32_t stream_id;
	uint32_t frame_id;
	struct timeval timestamp;
	uint32_t index;
};

struct v4l2_subdev *msm_buf_mngr_get_subdev(void);

#define VIDIOC_MSM_BUF_MNGR_GET_BUF \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 33, struct msm_buf_mngr_info)

#define VIDIOC_MSM_BUF_MNGR_PUT_BUF \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 34, struct msm_buf_mngr_info)

#define VIDIOC_MSM_BUF_MNGR_BUF_DONE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 35, struct msm_buf_mngr_info)

#define VIDIOC_MSM_BUF_MNGR_INIT \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 36, struct msm_buf_mngr_info)

#define VIDIOC_MSM_BUF_MNGR_DEINIT \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 37, struct msm_buf_mngr_info)

#endif
