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
 */

#include <linux/init.h>
#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/videodev2.h>
#include <linux/platform_device.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/msm_mdp.h>
#include <linux/sched.h>
#include <linux/capability.h>

#include <media/v4l2-ioctl.h>
#include <media/videobuf-dma-sg.h>
#include <media/v4l2-dev.h>
#include <media/msm_v4l2_overlay.h>

#include <mach/board.h>
#include <mach/msm_fb.h>

#include "msm_v4l2_video.h"

#define MSM_VIDEO -1

static struct msm_v4l2_overlay_device	*saved_vout0;

static struct mutex msmfb_lock;
static char *v4l2_ram_phys;
static unsigned int v4l2_ram_size;

static int msm_v4l2_overlay_mapformat(uint32_t pixelformat);

static int msm_v4l2_overlay_startstreaming(struct msm_v4l2_overlay_device *vout)
{
	vout->req.src.width = vout->pix.width;
	vout->req.src.height = vout->pix.height;

	vout->req.src_rect.x = vout->crop_rect.left;
	vout->req.src_rect.y = vout->crop_rect.top;
	vout->req.src_rect.w = vout->crop_rect.width;
	vout->req.src_rect.h = vout->crop_rect.height;

	vout->req.src.format =
		msm_v4l2_overlay_mapformat(vout->pix.pixelformat);

	vout->req.dst_rect.x = vout->win.w.left;
	vout->req.dst_rect.y = vout->win.w.top;
	vout->req.dst_rect.w = vout->win.w.width;
	vout->req.dst_rect.h = vout->win.w.height;

	vout->req.alpha = MDP_ALPHA_NOP;
	vout->req.transp_mask = MDP_TRANSP_NOP;

	pr_debug("msm_v4l2_overlay:startstreaming:enabling fb\n");
	mutex_lock(&msmfb_lock);
	msm_fb_v4l2_enable(&vout->req, true, &vout->par);
	mutex_unlock(&msmfb_lock);

	vout->streaming = 1;

	return 0;
}

static int msm_v4l2_overlay_stopstreaming(struct msm_v4l2_overlay_device *vout)
{
	if (!vout->streaming)
		return 0;

	pr_debug("msm_v4l2_overlay:startstreaming:disabling fb\n");
	mutex_lock(&msmfb_lock);
	msm_fb_v4l2_enable(&vout->req, false, &vout->par);
	mutex_unlock(&msmfb_lock);

	vout->streaming = 0;

	return 0;
}

static int msm_v4l2_overlay_mapformat(uint32_t pixelformat)
{
	int mdp_format;

	switch (pixelformat) {
	case V4L2_PIX_FMT_RGB565:
		mdp_format = MDP_RGB_565;
		break;
	case V4L2_PIX_FMT_RGB32:
		mdp_format = MDP_ARGB_8888;
		break;
	case V4L2_PIX_FMT_RGB24:
		mdp_format = MDP_RGB_888;
		break;
	case V4L2_PIX_FMT_NV12:
		mdp_format = MDP_Y_CRCB_H2V2;
		break;
	case V4L2_PIX_FMT_NV21:
		mdp_format = MDP_Y_CBCR_H2V2;
		break;
	case V4L2_PIX_FMT_YUV420:
		mdp_format = MDP_Y_CR_CB_H2V2;
		break;
	default:
		pr_err("%s:Unrecognized format %u\n", __func__, pixelformat);
		mdp_format = MDP_Y_CBCR_H2V2;
		break;
	}

	return mdp_format;
}

static int
msm_v4l2_overlay_fb_update(struct msm_v4l2_overlay_device *vout,
	struct v4l2_buffer *buffer)
{
	int ret;
	unsigned long src_addr, src_size;
	struct msm_v4l2_overlay_userptr_buffer up_buffer;

	if (!buffer ||
		(buffer->memory == V4L2_MEMORY_MMAP &&
		 buffer->index >= vout->numbufs))
		return -EINVAL;

	mutex_lock(&msmfb_lock);
	switch (buffer->memory) {
	case V4L2_MEMORY_MMAP:
		src_addr = (unsigned long)v4l2_ram_phys
		+ vout->bufs[buffer->index].offset;
		src_size = buffer->bytesused;
		ret = msm_fb_v4l2_update(vout->par, src_addr, src_size,
		0, 0, 0, 0);
		break;
	case V4L2_MEMORY_USERPTR:
		if (copy_from_user(&up_buffer,
		(void __user *)buffer->m.userptr,
		sizeof(struct msm_v4l2_overlay_userptr_buffer))) {
			mutex_unlock(&msmfb_lock);
			return -EINVAL;
		}
		ret = msm_fb_v4l2_update(vout->par,
		(unsigned long)up_buffer.base[0], up_buffer.length[0],
		(unsigned long)up_buffer.base[1], up_buffer.length[1],
		(unsigned long)up_buffer.base[2], up_buffer.length[2]);
		break;
	default:
		mutex_unlock(&msmfb_lock);
		return -EINVAL;
	}
	mutex_unlock(&msmfb_lock);

	if (buffer->memory == V4L2_MEMORY_MMAP) {
		vout->bufs[buffer->index].queued = 1;
		buffer->flags |= V4L2_BUF_FLAG_MAPPED;
	}
	buffer->flags |= V4L2_BUF_FLAG_QUEUED;

	return ret;
}

static int
msm_v4l2_overlay_vidioc_dqbuf(struct file *file,
	struct msm_v4l2_overlay_fh *fh, void *arg)
{
	struct msm_v4l2_overlay_device *vout = fh->vout;
	struct v4l2_buffer *buffer = arg;
	int i;

	if (!vout->streaming) {
		pr_err("%s: Video Stream not enabled\n", __func__);
		return -EINVAL;
	}

	if (!buffer || buffer->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	if (buffer->memory == V4L2_MEMORY_MMAP) {
		for (i = 0; i < vout->numbufs; i++) {
			if (vout->bufs[i].queued == 1)  {
				vout->bufs[i].queued = 0;
				/* Call into fb to remove this buffer? */
				break;
			}
		}

		/*
		 * This should actually block, unless O_NONBLOCK was
		 *  specified in open, but fine for now, especially
		 *  since this is not a capturing device
		 */
		if (i == vout->numbufs)
			return -EAGAIN;
	}

	buffer->flags &= ~V4L2_BUF_FLAG_QUEUED;

	return 0;
}


static int
msm_v4l2_overlay_vidioc_qbuf(struct file *file, struct msm_v4l2_overlay_fh* fh,
	void *arg, bool bUserPtr)
{
	struct msm_v4l2_overlay_device *vout = fh->vout;
	struct v4l2_buffer *buffer = arg;
	int ret;

	if (!bUserPtr && buffer->memory != V4L2_MEMORY_MMAP)
		return -EINVAL;

	if (!vout->streaming) {
		pr_err("%s: Video Stream not enabled\n", __func__);
		return -EINVAL;
	}

	if (!buffer || buffer->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	/* maybe allow only one qbuf at a time? */
	ret =  msm_v4l2_overlay_fb_update(vout, buffer);

	return 0;
}

static int
msm_v4l2_overlay_vidioc_querycap(struct file *file, void *arg)
{
	struct v4l2_capability *buffer = arg;

	memset(buffer, 0, sizeof(struct v4l2_capability));
	strlcpy(buffer->driver, "msm_v4l2_video_overlay",
		ARRAY_SIZE(buffer->driver));
	strlcpy(buffer->card, "MSM MDP",
		ARRAY_SIZE(buffer->card));
	buffer->capabilities = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT
		| V4L2_CAP_VIDEO_OVERLAY;
	return 0;
}

static int
msm_v4l2_overlay_vidioc_fbuf(struct file *file,
	struct msm_v4l2_overlay_device *vout, void *arg, bool get)
{
	struct v4l2_framebuffer *fb = arg;

	if (fb == NULL)
		return -EINVAL;

	if (get) {
		mutex_lock(&vout->update_lock);
		memcpy(&fb->fmt, &vout->pix, sizeof(struct v4l2_pix_format));
		mutex_unlock(&vout->update_lock);
	}
	/* The S_FBUF request does not store anything right now */
	return 0;
}

static long msm_v4l2_overlay_calculate_bufsize(struct v4l2_pix_format *pix)
{
	int bpp;
	long bufsize;
	switch (pix->pixelformat) {
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_NV12:
		bpp = 12;
		break;

	case V4L2_PIX_FMT_RGB565:
		bpp = 16;
		break;

	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_YUV444:
		bpp = 24;
		break;

	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_BGR32:
		bpp = 32;
		break;
	default:
		pr_err("%s: Unrecognized format %u\n", __func__,
		pix->pixelformat);
		bpp = 0;
	}

	bufsize = (pix->width * pix->height * bpp)/8;

	return bufsize;
}

static long
msm_v4l2_overlay_vidioc_reqbufs(struct file *file,
	struct msm_v4l2_overlay_device *vout, void *arg)

{
	struct v4l2_requestbuffers *rqb = arg;
	long bufsize;
	int i;

	if (rqb == NULL || rqb->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	if (rqb->memory == V4L2_MEMORY_MMAP) {
		if (rqb->count == 0) {
			/* Deallocate allocated buffers */
			mutex_lock(&vout->update_lock);
			vout->numbufs = 0;
			kfree(vout->bufs);
			/*
			 * There should be a way to look at bufs[i]->mapped,
			 * and prevent userspace from mmaping and directly
			 * calling this ioctl without unmapping. Maybe kernel
			 * handles for us, but needs to be checked out
			 */
			mutex_unlock(&vout->update_lock);
		} else {
			/*
			 * Keep it simple for now - need to deallocate
			 * before reallocate
			 */
			if (vout->bufs)
				return -EINVAL;

			mutex_lock(&vout->update_lock);
			bufsize =
				msm_v4l2_overlay_calculate_bufsize(&vout->pix);
			mutex_unlock(&vout->update_lock);

			if (bufsize == 0
				|| (bufsize * rqb->count) > v4l2_ram_size) {
				pr_err("%s: Unsupported format or buffer size too large\n",
				__func__);
				pr_err("%s: bufsize %lu ram_size %u count %u\n",
				__func__, bufsize, v4l2_ram_size, rqb->count);
				return -EINVAL;
			}

			/*
			 * We don't support multiple open of one vout,
			 * but there are probably still some MT problems here,
			 * (what if same fh is shared between two userspace
			 * threads and they both call REQBUFS etc)
			 */

			mutex_lock(&vout->update_lock);
			vout->numbufs = rqb->count;
			vout->bufs =
				kmalloc(rqb->count *
					sizeof(struct msm_v4l2_overlay_buffer),
					GFP_KERNEL);

			for (i = 0; i < rqb->count; i++) {
				struct msm_v4l2_overlay_buffer *b =
				(struct msm_v4l2_overlay_buffer *)vout->bufs
				+ i;
				b->mapped = 0;
				b->queued = 0;
				b->offset = PAGE_ALIGN(bufsize*i);
				b->bufsize = bufsize;
			}

			mutex_unlock(&vout->update_lock);

		}
	}

	return 0;
}

static long
msm_v4l2_overlay_vidioc_querybuf(struct file *file,
				 struct msm_v4l2_overlay_device *vout,
				 void *arg)
{
	struct v4l2_buffer *buf = arg;
	struct msm_v4l2_overlay_buffer *mbuf;

	if (buf == NULL || buf->type != V4L2_BUF_TYPE_VIDEO_OUTPUT
			|| buf->memory == V4L2_MEMORY_USERPTR
			|| buf->index >= vout->numbufs)
		return -EINVAL;

	mutex_lock(&vout->update_lock);

	mbuf = (struct msm_v4l2_overlay_buffer *)vout->bufs + buf->index;
	buf->flags = 0;
	if (mbuf->mapped)
		buf->flags |= V4L2_BUF_FLAG_MAPPED;
	if (mbuf->queued)
		buf->flags |= V4L2_BUF_FLAG_QUEUED;

	buf->memory = V4L2_MEMORY_MMAP;
	buf->length = mbuf->bufsize;
	buf->m.offset = mbuf->offset;

	mutex_unlock(&vout->update_lock);

	return 0;
}

static long
msm_v4l2_overlay_do_ioctl(struct file *file,
		       unsigned int cmd, void *arg)
{
	struct msm_v4l2_overlay_fh *fh = file->private_data;
	struct msm_v4l2_overlay_device *vout = fh->vout;
	int ret;

	switch (cmd) {
	case VIDIOC_QUERYCAP:
		return msm_v4l2_overlay_vidioc_querycap(file, arg);

	case VIDIOC_G_FBUF:
		return msm_v4l2_overlay_vidioc_fbuf(file, vout, arg, true);

	case VIDIOC_S_FBUF:
		return msm_v4l2_overlay_vidioc_fbuf(file, vout, arg, false);

	case VIDIOC_REQBUFS:
		return msm_v4l2_overlay_vidioc_reqbufs(file, vout, arg);

	case VIDIOC_QUERYBUF:
		return msm_v4l2_overlay_vidioc_querybuf(file, vout, arg);

	case VIDIOC_QBUF:
		mutex_lock(&vout->update_lock);
		ret = msm_v4l2_overlay_vidioc_qbuf(file, fh, arg, false);
		mutex_unlock(&vout->update_lock);

		return ret;

	case VIDIOC_MSM_USERPTR_QBUF:
		if (!capable(CAP_SYS_RAWIO))
			return -EPERM;

		mutex_lock(&vout->update_lock);
		ret = msm_v4l2_overlay_vidioc_qbuf(file, fh, arg, true);
		mutex_unlock(&vout->update_lock);

		return ret;

	case VIDIOC_DQBUF:
		mutex_lock(&vout->update_lock);
		ret = msm_v4l2_overlay_vidioc_dqbuf(file, fh, arg);
		mutex_unlock(&vout->update_lock);
		break;

	case VIDIOC_S_FMT: {
		struct v4l2_format *f = arg;

		switch (f->type) {
		case V4L2_BUF_TYPE_VIDEO_OVERLAY:
			mutex_lock(&vout->update_lock);
			memcpy(&vout->win, &f->fmt.win,
				sizeof(struct v4l2_window));
			mutex_unlock(&vout->update_lock);
			break;

		case V4L2_BUF_TYPE_VIDEO_OUTPUT:
			mutex_lock(&vout->update_lock);
			memcpy(&vout->pix, &f->fmt.pix,
				sizeof(struct v4l2_pix_format));
			mutex_unlock(&vout->update_lock);
			break;

		default:
			return -EINVAL;
		}
		break;
	}
	case VIDIOC_G_FMT: {
		struct v4l2_format *f = arg;

		switch (f->type) {
		case V4L2_BUF_TYPE_VIDEO_OUTPUT: {
			struct v4l2_pix_format *pix = &f->fmt.pix;
			memset(pix, 0, sizeof(*pix));
			*pix = vout->pix;
			break;
		}

		case V4L2_BUF_TYPE_VIDEO_OVERLAY: {
			struct v4l2_window *win = &f->fmt.win;
			memset(win, 0, sizeof(*win));
			win->w = vout->win.w;
			break;
		}
		default:
			return -EINVAL;
		}
		break;
	}

	case VIDIOC_CROPCAP: {
		struct v4l2_cropcap *cr = arg;
		if (cr->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
			return -EINVAL;

		cr->bounds.left =  0;
		cr->bounds.top = 0;
		cr->bounds.width = vout->crop_rect.width;
		cr->bounds.height = vout->crop_rect.height;

		cr->defrect.left =  0;
		cr->defrect.top = 0;
		cr->defrect.width = vout->crop_rect.width;
		cr->defrect.height = vout->crop_rect.height;

		cr->pixelaspect.numerator = 1;
		cr->pixelaspect.denominator = 1;
		break;
	}

	case VIDIOC_S_CROP: {
		struct v4l2_crop *crop = arg;

		switch (crop->type) {

		case V4L2_BUF_TYPE_VIDEO_OUTPUT:

			mutex_lock(&vout->update_lock);
			memcpy(&vout->crop_rect, &crop->c,
				sizeof(struct v4l2_rect));
			mutex_unlock(&vout->update_lock);

			break;

		default:

			return -EINVAL;
		}
		break;
	}
	case VIDIOC_G_CROP: {
		struct v4l2_crop *crop = arg;

		switch (crop->type) {

		case V4L2_BUF_TYPE_VIDEO_OUTPUT:
			memcpy(&crop->c, &vout->crop_rect,
				sizeof(struct v4l2_rect));
			break;

		default:
			return -EINVAL;
		}
		break;
	}

	case VIDIOC_S_CTRL: {
		struct v4l2_control *ctrl = arg;
		int32_t rotflag;

		switch (ctrl->id) {

		case V4L2_CID_ROTATE:
			switch (ctrl->value) {
			case 0:
				rotflag = MDP_ROT_NOP;
				break;
			case 90:
				rotflag = MDP_ROT_90;
				break;
			case 180:
				rotflag = MDP_ROT_180;
				break;
			case 270:
				rotflag = MDP_ROT_270;
				break;
			default:
				pr_err("%s: V4L2_CID_ROTATE invalid rotation value %d.\n",
						__func__, ctrl->value);
				return -ERANGE;
			}

			mutex_lock(&vout->update_lock);
			/* Clear the rotation flags */
			vout->req.flags &= ~MDP_ROT_NOP;
			vout->req.flags &= ~MDP_ROT_90;
			vout->req.flags &= ~MDP_ROT_180;
			vout->req.flags &= ~MDP_ROT_270;
			/* Set the new rotation flag */
			vout->req.flags |= rotflag;
			mutex_unlock(&vout->update_lock);

			break;

		case V4L2_CID_HFLIP:
			mutex_lock(&vout->update_lock);
			/* Clear the flip flag */
			vout->req.flags &= ~MDP_FLIP_LR;
			if (true == ctrl->value)
				vout->req.flags |= MDP_FLIP_LR;
			mutex_unlock(&vout->update_lock);

			break;

		case V4L2_CID_VFLIP:
			mutex_lock(&vout->update_lock);
			/* Clear the flip flag */
			vout->req.flags &= ~MDP_FLIP_UD;
			if (true == ctrl->value)
				vout->req.flags |= MDP_FLIP_UD;
			mutex_unlock(&vout->update_lock);

			break;

		default:
			pr_err("%s: VIDIOC_S_CTRL invalid control ID %d.\n",
			__func__, ctrl->id);
			return -EINVAL;
		}
		break;
	}
	case VIDIOC_G_CTRL: {
		struct v4l2_control *ctrl = arg;
		__s32 rotation;

		switch (ctrl->id) {

		case V4L2_CID_ROTATE:
			if (MDP_ROT_NOP == (vout->req.flags & MDP_ROT_NOP))
				rotation = 0;
			if (MDP_ROT_90 == (vout->req.flags & MDP_ROT_90))
				rotation = 90;
			if (MDP_ROT_180 == (vout->req.flags & MDP_ROT_180))
				rotation = 180;
			if (MDP_ROT_270 == (vout->req.flags & MDP_ROT_270))
				rotation = 270;

			ctrl->value = rotation;
			break;

		case V4L2_CID_HFLIP:
			if (MDP_FLIP_LR == (vout->req.flags & MDP_FLIP_LR))
				ctrl->value = true;
			break;

		case V4L2_CID_VFLIP:
			if (MDP_FLIP_UD == (vout->req.flags & MDP_FLIP_UD))
				ctrl->value = true;
			break;

		default:
			pr_err("%s: VIDIOC_G_CTRL invalid control ID %d.\n",
			__func__, ctrl->id);
			return -EINVAL;
		}
		break;
	}

	case VIDIOC_STREAMON: {

		if (vout->streaming) {
			pr_err("%s: VIDIOC_STREAMON: already streaming.\n",
			__func__);
			return -EBUSY;
		}

		mutex_lock(&vout->update_lock);
		msm_v4l2_overlay_startstreaming(vout);
		mutex_unlock(&vout->update_lock);

		break;
	}

	case VIDIOC_STREAMOFF: {

		if (!vout->streaming) {
			pr_err("%s: VIDIOC_STREAMOFF: not currently streaming.\n",
			__func__);
			return -EINVAL;
		}

		mutex_lock(&vout->update_lock);
		msm_v4l2_overlay_stopstreaming(vout);
		mutex_unlock(&vout->update_lock);

		break;
	}

	default:
		return -ENOIOCTLCMD;

	} /* switch */

	return 0;
}

static long
msm_v4l2_overlay_ioctl(struct file *file, unsigned int cmd,
		    unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_v4l2_overlay_do_ioctl);
}

static int
msm_v4l2_overlay_mmap(struct file *filp, struct vm_area_struct * vma)
{
	unsigned long start = (unsigned long)v4l2_ram_phys;

	/*
	 * vm_pgoff is the offset (>>PAGE_SHIFT) that we provided
	 * during REQBUFS. off therefore should equal the offset we
	 * provided in REQBUFS, since last (PAGE_SHIFT) bits of off
	 * should be 0
	 */
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	u32 len = PAGE_ALIGN((start & ~PAGE_MASK) + v4l2_ram_size);

	/*
	 * This is probably unnecessary now - the last PAGE_SHIFT
	 * bits of start should be 0 now, since we are page aligning
	 * v4l2_ram_phys
	 */
	start &= PAGE_MASK;

	pr_debug("v4l2 map req for phys(%p,%p) offset %u to virt (%p,%p)\n",
	(void *)(start+off), (void *)(start+off+(vma->vm_end - vma->vm_start)),
	(unsigned int)off, (void *)vma->vm_start, (void *)vma->vm_end);

	if ((vma->vm_end - vma->vm_start + off) > len) {
		pr_err("v4l2 map request, memory requested too big\n");
		return -EINVAL;
	}

	start += off;
	vma->vm_pgoff = start >> PAGE_SHIFT;
	/* This is an IO map - tell maydump to skip this VMA */
	vma->vm_flags |= VM_IO | VM_RESERVED;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	/* Remap the frame buffer I/O range */
	if (io_remap_pfn_range(vma, vma->vm_start, start >> PAGE_SHIFT,
				vma->vm_end - vma->vm_start,
				vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static int
msm_v4l2_overlay_release(struct file *file)
{
	struct msm_v4l2_overlay_fh *fh = file->private_data;
	struct msm_v4l2_overlay_device *vout = fh->vout;

	if (vout->streaming)
		msm_v4l2_overlay_stopstreaming(vout);

	vout->ref_count--;

	kfree(vout->bufs);
	vout->numbufs = 0;
	kfree(fh);

	return 0;
}

static int
msm_v4l2_overlay_open(struct file *file)
{
	struct msm_v4l2_overlay_device	*vout = NULL;
	struct v4l2_pix_format	*pix = NULL;
	struct msm_v4l2_overlay_fh *fh;

	vout = saved_vout0;
	vout->id = 0;

	if (vout->ref_count) {
		pr_err("%s: multiple open currently is not"
		"supported!\n", __func__);
		return -EBUSY;
	}

	vout->ref_count++;

	/* allocate per-filehandle data */
	fh = kmalloc(sizeof(struct msm_v4l2_overlay_fh), GFP_KERNEL);
	if (NULL == fh)
		return -ENOMEM;

	fh->vout = vout;
	fh->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	file->private_data = fh;

	vout->streaming		= 0;
	vout->crop_rect.left	= vout->crop_rect.top = 0;
	vout->crop_rect.width	= vout->screen_width;
	vout->crop_rect.height	= vout->screen_height;

	pix				= &vout->pix;
	pix->width			= vout->screen_width;
	pix->height		= vout->screen_height;
	pix->pixelformat	= V4L2_PIX_FMT_RGB32;
	pix->field			= V4L2_FIELD_NONE;
	pix->bytesperline	= pix->width * 4;
	pix->sizeimage		= pix->bytesperline * pix->height;
	pix->priv			= 0;
	pix->colorspace		= V4L2_COLORSPACE_SRGB;

	vout->win.w.left	= 0;
	vout->win.w.top		= 0;
	vout->win.w.width	= vout->screen_width;
	vout->win.w.height	= vout->screen_height;

	vout->fb.capability = V4L2_FBUF_CAP_EXTERNOVERLAY
		| V4L2_FBUF_CAP_LOCAL_ALPHA;
	vout->fb.flags = V4L2_FBUF_FLAG_LOCAL_ALPHA;
	vout->fb.base = 0;
	memcpy(&vout->fb.fmt, pix, sizeof(struct v4l2_format));

	vout->bufs = 0;
	vout->numbufs = 0;

	mutex_init(&vout->update_lock);

	return 0;
}


static int __devinit
msm_v4l2_overlay_probe(struct platform_device *pdev)
{
	char *v4l2_ram_phys_unaligned;
	if ((pdev->id == 0) && (pdev->num_resources > 0)) {
		v4l2_ram_size =
			pdev->resource[0].end - pdev->resource[0].start + 1;
		v4l2_ram_phys_unaligned = (char *)pdev->resource[0].start;
		v4l2_ram_phys =
		(char *)PAGE_ALIGN((unsigned int)v4l2_ram_phys_unaligned);
		/*
		 * We are (fwd) page aligning the start of v4l2 memory.
		 * Therefore we have that much less physical memory available
		 */
		v4l2_ram_size -= (unsigned int)v4l2_ram_phys
			- (unsigned int)v4l2_ram_phys_unaligned;


	}
	return 0;
}

static int __devexit
msm_v4l2_overlay_remove(struct platform_device *pdev)
{
	return 0;
}

static void msm_v4l2_overlay_videodev_release(struct video_device *vfd)
{
	return;
}

static const struct v4l2_file_operations msm_v4l2_overlay_fops = {
	.owner		= THIS_MODULE,
	.open		= msm_v4l2_overlay_open,
	.release	= msm_v4l2_overlay_release,
	.mmap		= msm_v4l2_overlay_mmap,
	.ioctl		= msm_v4l2_overlay_ioctl,
};

static struct video_device msm_v4l2_overlay_vid_device0 = {
	.name		= "msm_v4l2_overlay",
	.fops       = &msm_v4l2_overlay_fops,
	.minor		= -1,
	.release	= msm_v4l2_overlay_videodev_release,
};

static struct platform_driver msm_v4l2_overlay_platform_driver = {
	.probe   = msm_v4l2_overlay_probe,
	.remove  = msm_v4l2_overlay_remove,
	.driver  = {
			 .name = "msm_v4l2_overlay_pd",
		   },
};

static int __init msm_v4l2_overlay_init(void)
{
	int ret;


	saved_vout0 = kzalloc(sizeof(struct msm_v4l2_overlay_device),
		GFP_KERNEL);

	if (!saved_vout0)
		return -ENOMEM;

	ret = platform_driver_register(&msm_v4l2_overlay_platform_driver);
	if (ret < 0)
		goto end;

	/*
	 * Register the device with videodev.
	 * Videodev will make IOCTL calls on application requests
	 */
	ret = video_register_device(&msm_v4l2_overlay_vid_device0,
		VFL_TYPE_GRABBER, MSM_VIDEO);

	if (ret < 0) {
		pr_err("%s: V4L2 video overlay device registration failure(%d)\n",
				  __func__, ret);
		goto end_unregister;
	}

	mutex_init(&msmfb_lock);

	return 0;

end_unregister:
	platform_driver_unregister(&msm_v4l2_overlay_platform_driver);

end:
	kfree(saved_vout0);
	return ret;
}

static void __exit msm_v4l2_overlay_exit(void)
{
	video_unregister_device(&msm_v4l2_overlay_vid_device0);
	platform_driver_unregister(&msm_v4l2_overlay_platform_driver);
	mutex_destroy(&msmfb_lock);
	kfree(saved_vout0);
}

module_init(msm_v4l2_overlay_init);
module_exit(msm_v4l2_overlay_exit);

MODULE_DESCRIPTION("MSM V4L2 Video Overlay Driver");
MODULE_LICENSE("GPL v2");
