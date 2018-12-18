#ifndef __UAPI_MSM_JPEG_DMA__
#define __UAPI_MSM_JPEG_DMA__

#include <linux/videodev2.h>

/* msm jpeg dma control ID's */
#define V4L2_CID_JPEG_DMA_SPEED (V4L2_CID_PRIVATE_BASE)
#define V4L2_CID_JPEG_DMA_MAX_DOWN_SCALE (V4L2_CID_PRIVATE_BASE + 1)

/* msm_jpeg_dma_buf */
struct msm_jpeg_dma_buff {
	int32_t fd;
	uint32_t offset;
};

#endif /* __UAPI_MSM_JPEG_DMA__ */
