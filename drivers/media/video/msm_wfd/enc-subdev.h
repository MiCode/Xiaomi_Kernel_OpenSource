/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#ifndef _WFD_ENC_SUBDEV_
#define _WFD_ENC_SUBDEV_

#include <linux/msm_ion.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf2-core.h>
#define VENC_MAGIC_IOCTL 'V'

struct mem_region {
	struct list_head list;
	u8 *kvaddr;
	u8 *paddr;
	u32 size;
	u32 offset;
	u32 fd;
	u32 cookie;
	struct ion_handle *ion_handle;
};
struct bufreq {
	u32 count;
	u32 height;
	u32 width;
	u32 size;
};

struct venc_buf_info {
	u64 timestamp;
	struct mem_region *mregion;
};

struct venc_msg_ops {
	void *cookie;
	void *cbdata;
	int secure;
	void (*op_buffer_done)(void *cookie, u32 status,
			struct vb2_buffer *buf);
	void (*ip_buffer_done)(void *cookie, u32 status,
			struct mem_region *mregion);
};

#define OPEN  _IOR('V', 1, void *)
#define CLOSE  _IO('V', 2)
#define ENCODE_START  _IO('V', 3)
#define ENCODE_FRAME  _IOW('V', 4, struct venc_buf_info *)
#define PAUSE  _IO('V', 5)
#define RESUME  _IO('V', 6)
#define FLUSH  _IO('V', 7)
#define ENCODE_STOP  _IO('V', 8)
#define SET_PROP  _IO('V', 9)
#define GET_PROP  _IO('V', 10)
#define SET_BUFFER_REQ  _IOWR('V', 11, struct v4l2_requestbuffers *)
#define GET_BUFFER_REQ  _IOWR('V', 12, struct v4l2_requestbuffers *)
#define ALLOCATE_BUFFER  _IO('V', 13)
#define FREE_BUFFER  _IO('V', 14)
#define FILL_OUTPUT_BUFFER  _IO('V', 15)
#define SET_FORMAT _IOW('V', 16, struct v4l2_format *)
#define SET_FRAMERATE _IOW('V', 17, struct v4l2_fract *)
#define SET_INPUT_BUFFER _IOWR('V', 18, struct mem_region *)
#define SET_OUTPUT_BUFFER _IOWR('V', 19, struct mem_region *)
#define ALLOC_RECON_BUFFERS _IO('V', 20)
#define FREE_OUTPUT_BUFFER _IOWR('V', 21, struct mem_region *)
#define FREE_INPUT_BUFFER _IOWR('V', 22, struct mem_region *)
#define FREE_RECON_BUFFERS _IO('V', 23)
#define ENCODE_FLUSH _IO('V', 24)

extern int venc_init(struct v4l2_subdev *sd, u32 val);
extern int venc_load_fw(struct v4l2_subdev *sd);
extern long venc_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg);


#endif /* _WFD_ENC_SUBDEV_ */
