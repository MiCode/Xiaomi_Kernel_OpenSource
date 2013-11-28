/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 * Copyright (C) 1994 Martin Schaller
 *
 * 2001 - Documented with DocBook
 * - Brad Douglas <brad@neruo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/compat.h>
#include <linux/fb.h>

#include <linux/uaccess.h>

#include "mdss_fb.h"

struct mdp_buf_sync32 {
	u32		flags;
	u32		acq_fen_fd_cnt;
	u32		session_id;
	compat_caddr_t	acq_fen_fd;
	compat_caddr_t	rel_fen_fd;
};

struct fb_cmap32 {
	u32		start;
	u32		len;
	compat_caddr_t	red;
	compat_caddr_t	green;
	compat_caddr_t	blue;
	compat_caddr_t	transp;
};

#define MSMFB_SET_LUT32 _IOW(MSMFB_IOCTL_MAGIC, 131, struct fb_cmap32)
#define MSMFB_HISTOGRAM32 _IOWR(MSMFB_IOCTL_MAGIC, 132,\
					struct mdp_histogram_data32)
#define MSMFB_GET_CCS_MATRIX32  _IOWR(MSMFB_IOCTL_MAGIC, 133, struct mdp_ccs32)
#define MSMFB_SET_CCS_MATRIX32  _IOW(MSMFB_IOCTL_MAGIC, 134, struct mdp_ccs32)
#define MSMFB_OVERLAY_SET32       _IOWR(MSMFB_IOCTL_MAGIC, 135,\
					struct mdp_overlay32)

#define MSMFB_OVERLAY_GET32      _IOR(MSMFB_IOCTL_MAGIC, 140,\
					struct mdp_overlay32)
#define MSMFB_OVERLAY_BLT32       _IOWR(MSMFB_IOCTL_MAGIC, 142,\
					struct msmfb_overlay_blt32)
#define MSMFB_HISTOGRAM_START32	_IOR(MSMFB_IOCTL_MAGIC, 144,\
					struct mdp_histogram_start_req32)

#define MSMFB_OVERLAY_3D32       _IOWR(MSMFB_IOCTL_MAGIC, 147,\
					struct msmfb_overlay_3d32)

#define MSMFB_MIXER_INFO32       _IOWR(MSMFB_IOCTL_MAGIC, 148,\
						struct msmfb_mixer_info_req32)
#define MSMFB_MDP_PP32 _IOWR(MSMFB_IOCTL_MAGIC, 156, struct msmfb_mdp_pp32)
#define MSMFB_BUFFER_SYNC32  _IOW(MSMFB_IOCTL_MAGIC, 162, struct mdp_buf_sync32)


static unsigned int __do_compat_ioctl_nr(unsigned int cmd32)
{
	unsigned int cmd;

	switch (cmd32) {
	case MSMFB_SET_LUT32:
		cmd = MSMFB_SET_LUT;
		break;
	case MSMFB_HISTOGRAM32:
		cmd = MSMFB_HISTOGRAM;
		break;
	case MSMFB_GET_CCS_MATRIX32:
		cmd = MSMFB_GET_CCS_MATRIX;
		break;
	case MSMFB_SET_CCS_MATRIX32:
		cmd = MSMFB_SET_CCS_MATRIX;
		break;
	case MSMFB_OVERLAY_SET32:
		cmd = MSMFB_OVERLAY_SET;
		break;
	case MSMFB_OVERLAY_PLAY32:
		cmd = MSMFB_OVERLAY_PLAY;
		break;
	case MSMFB_OVERLAY_GET32:
		cmd = MSMFB_OVERLAY_GET;
		break;
	case MSMFB_OVERLAY_BLT32:
		cmd = MSMFB_OVERLAY_BLT;
		break;
	case MSMFB_OVERLAY_3D32:
		cmd = MSMFB_OVERLAY_3D;
		break;
	case MSMFB_MIXER_INFO32:
		cmd = MSMFB_MIXER_INFO;
		break;
	case MSMFB_OVERLAY_PLAY_WAIT32:
		cmd = MSMFB_OVERLAY_PLAY_WAIT;
		break;
	case MSMFB_WRITEBACK_QUEUE_BUFFER32:
		cmd = MSMFB_WRITEBACK_QUEUE_BUFFER;
		break;
	case MSMFB_WRITEBACK_DEQUEUE_BUFFER32:
		cmd = MSMFB_WRITEBACK_DEQUEUE_BUFFER;
		break;
	case MSMFB_MDP_PP32:
		cmd = MSMFB_MDP_PP;
		break;
	case MSMFB_BUFFER_SYNC32:
		cmd = MSMFB_BUFFER_SYNC;
		break;
	case MSMFB_METADATA_SET32:
		cmd = MSMFB_METADATA_SET;
		break;
	case MSMFB_METADATA_GET32:
		cmd = MSMFB_METADATA_GET;
		break;
	default:
		cmd = cmd32;
		break;
	}

	return cmd;
}

static int mdss_fb_compat_buf_sync(struct fb_info *info, unsigned int cmd,
			 unsigned long arg)
{
	struct mdp_buf_sync32 __user *buf_sync32;
	struct mdp_buf_sync __user *buf_sync;
	u32 data;
	int ret;

	buf_sync = compat_alloc_user_space(sizeof(*buf_sync));
	buf_sync32 = compat_ptr(arg);

	if (copy_in_user(&buf_sync->flags, &buf_sync32->flags,
			 3 * sizeof(u32)))
		return -EFAULT;

	if (get_user(data, &buf_sync32->acq_fen_fd) ||
	    put_user(compat_ptr(data), &buf_sync->acq_fen_fd) ||
	    get_user(data, &buf_sync32->rel_fen_fd) ||
	    put_user(compat_ptr(data), &buf_sync->rel_fen_fd))
		return -EFAULT;

	ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) buf_sync);
	if (ret) {
		pr_err("%s: failed %d\n", __func__, ret);
		return ret;
	}

	if (copy_in_user(buf_sync32->rel_fen_fd, buf_sync->rel_fen_fd,
			   sizeof(int)))
		return -EFAULT;
	if (copy_in_user(buf_sync32->retire_fen_fd, buf_sync->retire_fen_fd,
			   sizeof(int)))
		return -EFAULT;

	return ret;
}

static int mdss_fb_compat_set_lut(struct fb_info *info, unsigned long arg)
{
	struct fb_cmap_user __user *cmap;
	struct fb_cmap32 __user *cmap32;
	__u32 data;
	int ret;

	cmap = compat_alloc_user_space(sizeof(*cmap));
	cmap32 = compat_ptr(arg);

	if (copy_in_user(&cmap->start, &cmap32->start, 2 * sizeof(__u32)))
		return -EFAULT;

	if (get_user(data, &cmap32->red) ||
	    put_user(compat_ptr(data), &cmap->red) ||
	    get_user(data, &cmap32->green) ||
	    put_user(compat_ptr(data), &cmap->green) ||
	    get_user(data, &cmap32->blue) ||
	    put_user(compat_ptr(data), &cmap->blue) ||
	    get_user(data, &cmap32->transp) ||
	    put_user(compat_ptr(data), &cmap->transp))
		return -EFAULT;

	ret = mdss_fb_do_ioctl(info, MSMFB_SET_LUT, (unsigned long) cmap);
	if (!ret)
		pr_debug("%s: compat ioctl successful\n", __func__);

	return ret;
}

/*
 * mdss_fb_compat_ioctl() - MDSS Framebuffer compat ioctl function
 * @info:	pointer to framebuffer info
 * @cmd:	ioctl command
 * @arg:	argument to ioctl
 *
 * This function adds the compat translation layer for framebuffer
 * ioctls to allow 32-bit userspace call ioctls on the mdss
 * framebuffer device driven in 64-bit kernel.
 */
int mdss_fb_compat_ioctl(struct fb_info *info, unsigned int cmd,
			 unsigned long arg)
{
	int ret;

	if (!info || !info->par)
		return -EINVAL;

	cmd = __do_compat_ioctl_nr(cmd);
	switch (cmd) {
	case MSMFB_CURSOR:
		pr_debug("%s: MSMFB_CURSOR not supported\n", __func__);
		ret = -ENOSYS;
		break;
	case MSMFB_SET_LUT:
		ret = mdss_fb_compat_set_lut(info, arg);
		break;
	case MSMFB_BUFFER_SYNC:
		ret = mdss_fb_compat_buf_sync(info, cmd, arg);
		break;
	case MSMFB_NOTIFY_UPDATE:
	case MSMFB_DISPLAY_COMMIT:
	default:
		ret = mdss_fb_do_ioctl(info, cmd, arg);
		break;
	}

	if (ret == -ENOSYS)
		pr_err("%s: unsupported ioctl\n", __func__);
	else if (ret)
		pr_err("%s: ioctl err cmd=%u ret=%d\n", __func__, cmd, ret);

	return ret;
}
EXPORT_SYMBOL(mdss_fb_compat_ioctl);
