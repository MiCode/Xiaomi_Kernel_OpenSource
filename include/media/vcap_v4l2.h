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

#ifndef VCAP_V4L2_H
#define VCAP_V4L2_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-common.h>
#include <media/vcap_fmt.h>
#include <mach/board.h>

struct vcap_client_data;

enum rdy_buf {
	VC_NO_BUF = 0,
	VC_BUF1 = 1 << 1,
	VC_BUF2 = 1 << 2,
	VC_BUF1N2 = 0x11 << 1,
};

struct vcap_buf_info {
	unsigned long vaddr;
	unsigned long size;
};

struct vcap_action {
	struct list_head		active;

	/* thread for generating video stream*/
	struct task_struct		*kthread;
	wait_queue_head_t		wq;

	/* Buffer index */
	enum rdy_buf            buf_ind;

	/* Buffers inside vc */
	struct vcap_buffer      *buf1;
	struct vcap_buffer      *buf2;

	/* Counters to control fps rate */
	int						frame;
	int						ini_jiffies;
};

struct vcap_dev {
	struct v4l2_device		v4l2_dev;

	struct video_device		*vfd;
	struct ion_client       *ion_client;

	struct resource         *vcapirq;

	struct resource			*vcapmem;
	struct resource			*vcapio;
	void __iomem			*vcapbase;

	struct vcap_platform_data	*vcap_pdata;

	struct regulator		*fs_vcap;
	struct clk				*vcap_clk;
	struct clk				*vcap_p_clk;
	struct clk				*vcap_npl_clk;
	/*struct platform_device	*pdev;*/

	uint32_t				bus_client_handle;

	struct vcap_client_data *vc_client;
	struct vcap_client_data *vp_client;

	atomic_t			    vc_enabled;
	atomic_t				vc_resource;
	atomic_t				vp_resource;
};

struct vp_format_data {
	unsigned int		width, height;
	unsigned int		pixelformat;
	enum v4l2_field		field;

};

struct vcap_buffer {
	/* common v4l buffer stuff -- must be first */
	struct vb2_buffer	vb;
	struct list_head	list;
	unsigned long		paddr;
	struct ion_handle   *ion_handle;
};

struct vcap_client_data {
	struct vcap_dev *dev;

	struct vb2_queue		vc_vidq;
	/*struct vb2_queue		vb__vidq;*/
	/*struct vb2_queue		vb_cap_vidq;*/

	struct v4l2_format_vc_ext vc_format;

	enum v4l2_buf_type		vp_buf_type_field;
	struct vp_format_data	vp_format;

	struct vcap_action		vid_vc_action;
	struct workqueue_struct *vcap_work_q;
	struct ion_handle			*vc_ion_handle;

	uint32_t				hold_vc;
	uint32_t				hold_vp;

	spinlock_t				cap_slock;
};

#endif
#endif
