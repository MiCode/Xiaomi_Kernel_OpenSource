#ifndef LINUX_MSM_V4L2_OVERLAY
#define LINUX_MSM_V4L2_OVERLAY

#include <linux/videodev2.h>

#define VIDIOC_MSM_USERPTR_QBUF	\
_IOWR('V', BASE_VIDIOC_PRIVATE, struct v4l2_buffer)

#endif
