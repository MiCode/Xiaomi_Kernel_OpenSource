/* Copyright (c) 2010,2012, The Linux Foundation. All rights reserved.
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

#ifndef MPQ_DVB_VIDEO_INTERNAL_H
#define MPQ_DVB_VIDEO_INTERNAL_H

#include <linux/msm_vidc_dec.h>
#include <media/msm/vidc_init.h>
#include <linux/dvb/video.h>

/*
 * MPQ Specific Includes.
 */
#include "mpq_dvb_debug.h"
#include "mpq_adapter.h"
#include "mpq_stream_buffer.h"

#define DVB_MPQ_NUM_VIDEO_DEVICES CONFIG_DVB_MPQ_NUM_VIDEO_DEVICES

/*
 * Input Buffer Requirements for Video Decoder.
 */
#define DVB_VID_NUM_IN_BUFFERS (2)
#define DVB_VID_IN_BUFFER_SIZE (2*1024*1024)
#define DVB_VID_IN_BUFFER_ALGN (8*1024)

struct vid_dec_msg {
	struct list_head list;
	struct vdec_msginfo vdec_msg_info;
};

enum mpq_bcast_msgcode {
	MPQ_BCAST_MSG_START,
	MPQ_BCAST_MSG_IBD,
	MPQ_BCAST_MSG_FLUSH,
	MPQ_BCAST_MSG_TERM
};

struct mpq_bcast_msg_info {
	enum mpq_bcast_msgcode code;
	unsigned int data;
};

struct mpq_bcast_msg {
	struct list_head list;
	struct mpq_bcast_msg_info info;
};

struct mpq_dmx_src_data {
	struct mpq_streambuffer *stream_buffer;
	struct video_data_buffer in_buffer[DVB_VID_NUM_IN_BUFFERS];
	struct list_head msg_queue;
	wait_queue_head_t msg_wait;
	struct mutex msg_queue_lock;
	struct task_struct *data_task;
};

struct mpq_dvb_video_inst {
	struct dvb_device  *video_dev;
	video_stream_source_t source;
	struct mpq_dmx_src_data *dmx_src_data;
	struct video_client_ctx *client_ctx;
};

struct mpq_dvb_video_dev {

	resource_size_t phys_base;
	void __iomem *virt_base;
	unsigned int irq;
	struct clk *hclk;
	struct clk *hclk_div2;
	struct clk *pclk;
	unsigned long hclk_rate;
	struct mutex lock;
	s32 device_handle;
	struct dvb_adapter *mpq_adapter;
	struct mpq_dvb_video_inst dev_inst[DVB_MPQ_NUM_VIDEO_DEVICES];
	struct video_client_ctx vdec_clients[DVB_MPQ_NUM_VIDEO_DEVICES];
	u32 num_clients;
	void(*timer_handler)(void *);
};

#endif /* MPQ_DVB_VIDEO_INTERNAL_H */
