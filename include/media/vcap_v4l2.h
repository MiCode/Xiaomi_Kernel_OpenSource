/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <mach/iommu_domains.h>

#define to_client_data(val)     container_of(val, struct vcap_client_data, vfh)

#define writel_iowmb(val, addr)		\
	do {							\
		__iowmb();					\
		writel_relaxed(val, addr);	\
	} while (0)

#define VCAP_USEC (1000000)

#define VCAP_STRIDE_ALIGN_16 0x10
#define VCAP_STRIDE_ALIGN_32 0x20
#define VCAP_STRIDE_CALC(x, align) (((x / align) + \
			(!(!(x % align)))) * align)

#define VCAP_BASE (dev->vcapbase)
#define VCAP_OFFSET(off) (VCAP_BASE + off)

struct reg_range {
	u32 min_val;
	u32 max_val;
};

#define VCAP_REG_RANGE_1_MIN	0x0
#define VCAP_REG_RANGE_1_MAX	0x48
#define VCAP_REG_RANGE_2_MIN	0x100
#define VCAP_REG_RANGE_2_MAX	0x104
#define VCAP_REG_RANGE_3_MIN	0x400
#define VCAP_REG_RANGE_3_MAX	0x7F0
#define VCAP_REG_RANGE_4_MIN	0x800
#define VCAP_REG_RANGE_4_MAX	0x8A0
#define VCAP_REG_RANGE_5_MIN	0xC00
#define VCAP_REG_RANGE_5_MAX	0xDF0

#define VCAP_SW_RESET_REQ (VCAP_BASE + 0x024)
#define VCAP_SW_RESET_STATUS (VCAP_BASE + 0x028)

#define VCAP_VP_MIN_BUF 4
#define VCAP_VC_MAX_BUF 6
#define VCAP_VC_MIN_BUF 2
struct vcap_client_data;

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

struct vc_action {
	struct list_head		active;

	/* thread for generating video stream*/
	wait_queue_head_t		wq;

	/* Buffer index */
	uint8_t					tot_buf;
	uint8_t					buf_num;

	bool					field1;
	bool					field_dropped;

	struct timeval			vc_ts;
	uint32_t				last_ts;

	/* Buffers inside vc */
	struct vcap_buffer      *buf[6];
};

struct nr_buffer {
	struct ion_handle			*nr_handle;
	unsigned long				paddr;
	enum nr_buf_pos				nr_pos;
};

struct vp_action {
	struct list_head		in_active;
	struct list_head		out_active;

	/* Buffer index */
	enum vp_state			vp_state;

	/* Buffers inside vc */
	struct vcap_buffer      *bufTm1;
	struct vcap_buffer      *bufT0;
	struct vcap_buffer      *bufT1;
	struct vcap_buffer      *bufT2;
	struct vcap_buffer      *bufNRT2;

	struct vcap_buffer      *bufOut;

	struct ion_handle		*motionHandle;
	void					*bufMotion;
	struct nr_buffer		bufNR;
};

struct vp_work_t {
	struct work_struct work;
	struct vcap_client_data *cd;
};

struct vcap_debugfs_params {
	atomic_t vc_drop_count;
	uint32_t vc_timestamp;
	uint32_t vp_timestamp;
	uint32_t vp_ewma;/* Exponential moving average */
	uint32_t clk_rate;
	uint32_t bw_request;
	uint32_t reg_addr;
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

	int						domain_num;
	struct device			*vc_iommu_ctx;
	struct device			*vp_iommu_ctx;
	struct iommu_domain		*iommu_vcap_domain;

	struct vcap_client_data *vc_client;
	struct vcap_client_data *vp_client;

	atomic_t			    vc_enabled;
	atomic_t			    vp_enabled;

	struct mutex			dev_mutex;
	atomic_t			    open_clients;
	bool					vc_resource;
	bool					vp_resource;
	bool					vp_dummy_event;
	bool					vp_dummy_complete;
	bool					vp_shutdown;
	wait_queue_head_t		vp_dummy_waitq;

	uint8_t					vc_tot_buf;

	struct workqueue_struct	*vcap_wq;
	struct vp_work_t		vp_work;
	struct vp_work_t		vc_to_vp_work;
	struct vp_work_t		vp_to_vc_work;

	struct nr_param			nr_param;
	bool					nr_update;
	struct vcap_debugfs_params	dbg_p;
};

struct vp_format_data {
	unsigned int		width, height;
	unsigned int		pixfmt;
};

struct vcap_buffer {
	/* common v4l buffer stuff -- must be first */
	struct vb2_buffer	vb;
	struct list_head	list;
	dma_addr_t		paddr;
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
	enum vcap_stride		stride;

	enum v4l2_buf_type		vp_buf_type_field;
	struct vp_format_data	vp_in_fmt;
	struct vp_format_data	vp_out_fmt;

	struct vc_action		vc_action;
	struct vp_action		vp_action;
	struct workqueue_struct *vcap_work_q;
	struct ion_handle			*vc_ion_handle;

	uint32_t				hold_vc;
	uint32_t				hold_vp;

	/* Mutex ensures only one thread is dq buffer or turning streamoff */
	struct mutex			mutex;
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
int vcvp_qbuf(struct vb2_queue *q, struct v4l2_buffer *b);
int vcvp_dqbuf(struct vb2_queue *q, struct v4l2_buffer *b);
#endif
