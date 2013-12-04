/* Copyright (c) 2010, 2012-2013, The Linux Foundation. All rights reserved.
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

#define MPQ_DBG_INFO  "mpq_video:%d "
#define MPQ_VID_DEC_NAME "mpq_vidc_dec"

#define TRICKMODE_SUPPORT

extern int mpq_debug;
enum {
	MPQ_ERR = 0x001,
	MPQ_WRN = 0x002,
	MPQ_INF = 0x004,
	MPQ_DBG = 0x008
};

#define dprintk(dbg_mask, fmt, arg...)    \
	do { \
		if (mpq_debug & dbg_mask) { \
			printk(MPQ_DBG_INFO fmt, dbg_mask, ## arg); \
		} \
	} while (0)


#define DBG(x...)	dprintk(MPQ_DBG, ## x)
#define INF(x...)	dprintk(MPQ_INF, ## x)
#define WRN(x...)	dprintk(MPQ_WRN, ## x)
#define ERR(x...)	dprintk(MPQ_ERR, ## x)


#define DVB_MPQ_NUM_VIDEO_DEVICES	4
#define SAFE_GAP			16
#define MAX_NUM_BUFS			32

#define MPQ_DVB_INPUT_BUF_COUNT_BIT		0x00000001
#define MPQ_DVB_INPUT_BUF_REQ_BIT		0x00000002
#define MPQ_DVB_INPUT_BUF_SETUP_BIT		0x00000004
#define MPQ_DVB_OUTPUT_BUF_COUNT_BIT	0x00000008
#define MPQ_DVB_OUTPUT_BUF_REQ_BIT		0x00000010
#define MPQ_DVB_OUTPUT_BUF_SETUP_BIT	0x00000020
#define MPQ_DVB_INPUT_STREAMON_BIT		0x00000040
#define MPQ_DVB_OUTPUT_STREAMON_BIT		0x00000080
#define MPQ_DVB_EVENT_FLUSH_DONE_BIT		0x00000100
#define MPQ_DVB_INPUT_FLUSH_IN_PROGRESS_BIT	0x00000200
#define MPQ_DVB_OUTPUT_FLUSH_IN_PROGRESS_BIT	0x00000400

#define DEFAULT_INPUT_BUF_SIZE	        (1024*1024)
#define DEFAULT_INPUT_BUF_NUM		16

#define MPQ_VID_DEC_NAME "mpq_vidc_dec"
#define EXTRADATA_HANDLING
#define EXTRADATA_IDX(__num_planes) (__num_planes - 1)

enum {
	OUTPUT_PORT = 0,
	CAPTURE_PORT,
	MAX_PORTS
};

enum {
	INPUT_MODE_LINEAR,
	INPUT_MODE_RING
};

enum {
	MPQ_INPUT_BUFFER_FREE,
	MPQ_INPUT_BUFFER_IN_USE
};

enum {
	MPQ_MSG_OUTPUT_BUFFER_DONE,
	MPQ_MSG_VIDC_EVENT
};

enum {
	MPQ_STATE_INIT,
	MPQ_STATE_READY,
	MPQ_STATE_RUNNING,
	MPQ_STATE_IDLE,
	MPQ_STATE_STOPPED
};

struct mpq_inq_msg {
	struct list_head	list;
	u32			buf_index;
};

struct mpq_outq_msg {
	struct list_head	list;
	u32			type;
	struct video_event	vidc_event;
};

struct mpq_msg_q_msg {
	struct list_head	list;
	u32			msg_type;
};

struct mpq_pkt_msg {
	struct list_head	list;
	int	ion_fd;
	u32		offset;
	u32		len;
	u64		pts;
};


struct mpq_extradata {
	int index;
	u32	uaddr;
	u32 length;
	u32 bytesused;
	int	ion_fd;
	int fd_offset;
};

struct buffer_info {
	int			index;
	int			state;
	enum v4l2_buf_type	buf_type;
	u32			size;
	u32			offset;
	u32			bytesused;
	u32			vaddr;
	int			fd;
	u32			dev_addr;
	u32			kernel_vaddr;
	u32			buf_offset;
	u64			pts;
	struct msm_smem     *handle;

	struct mpq_extradata extradata;

};

struct mpq_ring_buffer {
	struct buffer_info	buf;
	size_t			len;
	size_t			write_idx;
	size_t			read_idx;
	size_t			release_idx;
	u32				flush_buffer;
	wait_queue_head_t	write_wait;
	wait_queue_head_t	read_wait;
	struct semaphore	sem;
};

struct v4l2_instance {
	struct msm_vidc_instance	*vidc_inst;
	void	                    *mem_client;
	struct mutex                lock;
	struct list_head		msg_queue;
	struct semaphore		msg_sem;
	wait_queue_head_t		msg_wait;
	struct list_head		inq;
	struct semaphore		inq_sem;
	wait_queue_head_t		inq_wait;
	struct list_head		outq;
	struct semaphore		outq_sem;
	wait_queue_head_t		outq_wait;
	int						video_codec;
	struct v4l2_requestbuffers	bufreq[MAX_PORTS];
	struct v4l2_format		fmt[MAX_PORTS];
	struct buffer_info		buf_info[MAX_PORTS][MAX_NUM_BUFS];
	u32				input_mode;
	struct mpq_ring_buffer	    *ringbuf;

	u32				num_input_buffers;
	u32				input_buf_count;
	u32				num_output_buffers;
	u32				output_buf_count;

	u32				flag;
	u32				state;
	u32		vidc_etb;
	u32		vidc_ebd;
	u32		vidc_ftb;
	u32		vidc_fbd;
	struct mutex flush_lock;

	struct msm_smem     *extradata_handle;
	u32					extradata_types;
	u32					extradata_size;
	struct extradata_buffer extradata;


	int		playback_mode;

};

struct mpq_dmx_source {
	struct mpq_streambuffer *stream_buffer;
	wait_queue_head_t		dmx_wait;
	int						device_id;
	struct video_data_buffer dmx_video_buf;
	struct list_head		pkt_queue;
	struct semaphore		pkt_sem;
	wait_queue_head_t		pkt_wait;
};

struct mpq_dvb_video_instance {
	struct dvb_device		*video_dev;
	video_stream_source_t		source;
	struct mpq_dmx_source		*dmx_src_data;
	struct v4l2_instance		*v4l2_inst;
	struct task_struct		*input_task;
	struct task_struct		*event_task;
	struct task_struct		*demux_task;
};

struct mpq_dvb_video_device {
	s32				device_handle;
	u32				num_clients;
	struct mutex			lock;
	struct dvb_adapter		*mpq_adapter;
	struct mpq_dvb_video_instance	dev_inst[DVB_MPQ_NUM_VIDEO_DEVICES];
	struct ion_client		*ion_clnt;
};

#endif /* MPQ_DVB_VIDEO_INTERNAL_H */
