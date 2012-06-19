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

#define TOP_FIELD_FIX
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

#define to_client_data(val)     container_of(val, struct vcap_client_data, vfh)

#define writel_iowmb(val, addr)		\
	do {							\
		__iowmb();					\
		writel_relaxed(val, addr);	\
	} while (0)

struct vcap_client_data;

enum rdy_buf {
	VC_NO_BUF = 0,
	VC_BUF1 = 1 << 1,
	VC_BUF2 = 1 << 2,
	VC_BUF1N2 = 0x11 << 1,
};

enum vp_state {
	VP_UNKNOWN = 0,
	VP_FRAME1,
	VP_FRAME2,
	VP_FRAME3,
	VP_NORMAL,
};

enum nr_buf_pos {
	BUF_NOT_IN_USE = 0,
	NRT2_BUF,
	T1_BUF,
	T0_BUF,
	TM1_BUF,
};

struct vcap_buf_info {
	unsigned long vaddr;
	unsigned long size;
};

enum vcap_op_mode {
	UNKNOWN_VCAP_OP = 0,
	VC_VCAP_OP,
	VP_VCAP_OP,
	VC_AND_VP_VCAP_OP,
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

struct nr_buffer {
	void						*vaddr;
	unsigned long				paddr;
	enum nr_buf_pos				nr_pos;
};

struct vp_action {
	struct list_head		in_active;
	struct list_head		out_active;

	/* Buffer index */
	enum vp_state			vp_state;
#ifdef TOP_FIELD_FIX
	bool					top_field;
#endif

	/* Buffers inside vc */
	struct vcap_buffer      *bufTm1;
	struct vcap_buffer      *bufT0;
	struct vcap_buffer      *bufT1;
	struct vcap_buffer      *bufT2;
	struct vcap_buffer      *bufNRT2;

	struct vcap_buffer      *bufOut;

	void					*bufMotion;
	struct nr_buffer		bufNR;
	bool					nr_enabled;
};

struct vp_work_t {
	struct work_struct work;
	struct vcap_client_data *cd;
	uint32_t irq;
};

struct vcap_dev {
	struct v4l2_device		v4l2_dev;

	struct video_device		*vfd;
	struct ion_client       *ion_client;

	struct resource         *vcirq;
	struct resource         *vpirq;

	struct resource			*vcapmem;
	struct resource			*vcapio;
	void __iomem			*vcapbase;

	struct vcap_platform_data	*vcap_pdata;

	struct regulator		*fs_vcap;
	struct clk				*vcap_clk;
	struct clk				*vcap_p_clk;
	struct clk				*vcap_npl_clk;
	struct device			*ddev;
	/*struct platform_device	*pdev;*/

	uint32_t				bus_client_handle;

	struct vcap_client_data *vc_client;
	struct vcap_client_data *vp_client;

	atomic_t			    vc_enabled;
	atomic_t			    vp_enabled;

	spinlock_t				dev_slock;
	atomic_t			    open_clients;
	bool					vc_resource;
	bool					vp_resource;

	struct workqueue_struct	*vcap_wq;
	struct vp_work_t		vp_work;
	struct vp_work_t		vc_to_vp_work;
	struct vp_work_t		vp_to_vc_work;
};

struct vp_format_data {
	unsigned int		width, height;
	unsigned int		pixfmt;
};

struct vcap_buffer {
	/* common v4l buffer stuff -- must be first */
	struct vb2_buffer	vb;
	struct list_head	list;
	unsigned long		paddr;
	struct ion_handle   *ion_handle;
};

struct vcap_client_data {
	bool set_cap, set_decode, set_vp_o;
	struct vcap_dev *dev;

	struct vb2_queue		vc_vidq;
	struct vb2_queue		vp_in_vidq;
	struct vb2_queue		vp_out_vidq;

	enum vcap_op_mode		op_mode;

	struct v4l2_format_vc_ext vc_format;

	enum v4l2_buf_type		vp_buf_type_field;
	struct vp_format_data	vp_in_fmt;
	struct vp_format_data	vp_out_fmt;

	struct vcap_action		vid_vc_action;
	struct vp_action		vid_vp_action;
	struct workqueue_struct *vcap_work_q;
	struct ion_handle			*vc_ion_handle;

	uint32_t				hold_vc;
	uint32_t				hold_vp;

	spinlock_t				cap_slock;
	bool					streaming;

	struct v4l2_fh			vfh;
};

struct vcap_hacked_vals {
	uint32_t	value;
	uint32_t	offset;
};

extern struct vcap_hacked_vals hacked_buf[];

#endif
int free_ion_handle(struct vcap_dev *dev, struct vb2_queue *q,
					 struct v4l2_buffer *b);

int get_phys_addr(struct vcap_dev *dev, struct vb2_queue *q,
				  struct v4l2_buffer *b);
#endif
