#ifndef __MEDIA_MSMB_BUF_MNGR_H__
#define __MEDIA_MSMB_BUF_MNGR_H__

struct msm_buf_mngr_info {
	uint32_t session_id;
	uint32_t stream_id;
	uint32_t frame_id;
	uint32_t index;
};


#define VIDIOC_MSM_BUF_MNGR_GET_BUF \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 33, struct msm_buf_mngr_info)

#define VIDIOC_MSM_BUF_MNGR_PUT_BUF \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 34, struct msm_buf_mngr_info)

#define VIDIOC_MSM_BUF_MNGR_BUF_DONE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 35, struct msm_buf_mngr_info)

#endif
