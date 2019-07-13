/*
 * dmxdev.h
 *
 * Copyright (C) 2000 Ralph Metzler & Marcus Metzler
 *                    for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _DMXDEV_H_
#define _DMXDEV_H_

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/dvb/dmx.h>

#include <media/dvbdev.h>
#include <media/demux.h>
#include <media/dvb_ringbuffer.h>
#include <media/dvb_vb2.h>

/**
 * enum dmxdev_type - type of demux filter type.
 *
 * @DMXDEV_TYPE_NONE:	no filter set.
 * @DMXDEV_TYPE_SEC:	section filter.
 * @DMXDEV_TYPE_PES:	Program Elementary Stream (PES) filter.
 */
enum dmxdev_type {
	DMXDEV_TYPE_NONE,
	DMXDEV_TYPE_SEC,
	DMXDEV_TYPE_PES,
};

/**
 * enum dmxdev_state - state machine for the dmxdev.
 *
 * @DMXDEV_STATE_FREE:		indicates that the filter is freed.
 * @DMXDEV_STATE_ALLOCATED:	indicates that the filter was allocated
 *				to be used.
 * @DMXDEV_STATE_SET:		indicates that the filter parameters are set.
 * @DMXDEV_STATE_GO:		indicates that the filter is running.
 * @DMXDEV_STATE_DONE:		indicates that a packet was already filtered
 *				and the filter is now disabled.
 *				Set only if %DMX_ONESHOT. See
 *				&dmx_sct_filter_params.
 * @DMXDEV_STATE_TIMEDOUT:	Indicates a timeout condition.
 */
enum dmxdev_state {
	DMXDEV_STATE_FREE,
	DMXDEV_STATE_ALLOCATED,
	DMXDEV_STATE_SET,
	DMXDEV_STATE_GO,
	DMXDEV_STATE_DONE,
	DMXDEV_STATE_TIMEDOUT
};

/**
 * struct dmxdev_feed - digital TV dmxdev feed
 *
 * @pid:	Program ID to be filtered
 * @ts:		pointer to &struct dmx_ts_feed
 * @next:	&struct list_head pointing to the next feed.
 */

struct dmxdev_feed {
	u16 pid;
	struct dmx_indexing_params idx_params;
	struct dmx_cipher_operations cipher_ops;
	struct dmx_ts_feed *ts;
	struct list_head next;
};

struct dmxdev_sec_feed {
	struct dmx_section_feed *feed;
	struct dmx_cipher_operations cipher_ops;
};

struct dmxdev_events_queue {
	/*
	 * indices used to manage events queue.
	 * read_index advanced when relevant data is read
	 * from the buffer.
	 * notified_index is the index from which next events
	 * are returned.
	 * read_index <= notified_index <= write_index
	 *
	 * If user reads the data without getting the respective
	 * event first, the read/notified indices are updated
	 * automatically to reflect the actual data that exist
	 * in the buffer.
	 */
	u32 read_index;
	u32 write_index;
	u32 notified_index;

	/* Bytes read by user without having respective event in the queue */
	u32 bytes_read_no_event;

	/* internal tracking of PES and recording events */
	u32 current_event_data_size;
	u32 current_event_start_offset;

	/* current setting of the events masking */
	struct dmx_events_mask event_mask;

	/*
	 * indicates if an event used for data-reading from demux
	 * filter is enabled or not. These are events on which
	 * user may wait for before calling read() on the demux filter.
	 */
	int data_read_event_masked;

	/*
	 * holds the current number of pending events in the
	 * events queue that are considered as a wake-up source
	 */
	u32 wakeup_events_counter;

	struct dmx_filter_event queue[DMX_EVENT_QUEUE_SIZE];
};

#define DMX_MIN_INSERTION_REPETITION_TIME	25 /* in msec */
struct ts_insertion_buffer {
	/* work scheduled for insertion of this buffer */
	struct delayed_work dwork;

	struct list_head next;

	/* buffer holding TS packets for insertion */
	char *buffer;

	/* buffer size */
	size_t size;

	/* buffer ID from user */
	u32 identifier;

	/* repetition time for the buffer insertion */
	u32 repetition_time;

	/* the recording filter to which this buffer belongs */
	struct dmxdev_filter *dmxdevfilter;

	/* indication whether insertion should be aborted */
	int abort;
};

/**
 * struct dmxdev_filter - digital TV dmxdev filter
 *
 * @filter:	a union describing a dmxdev filter.
 *		Currently used only for section filters.
 * @filter.sec: a &struct dmx_section_filter pointer.
 *		For section filter only.
 * @feed:	a union describing a dmxdev feed.
 *		Depending on the filter type, it can be either
 *		@feed.ts or @feed.sec.
 * @feed.ts:	a &struct list_head list.
 *		For TS and PES feeds.
 * @feed.sec:	a &struct dmx_section_feed pointer.
 *		For section feed only.
 * @params:	a union describing dmxdev filter parameters.
 *		Depending on the filter type, it can be either
 *		@params.sec or @params.pes.
 * @params.sec:	a &struct dmx_sct_filter_params embedded struct.
 *		For section filter only.
 * @params.pes:	a &struct dmx_pes_filter_params embedded struct.
 *		For PES filter only.
 * @type:	type of the dmxdev filter, as defined by &enum dmxdev_type.
 * @state:	state of the dmxdev filter, as defined by &enum dmxdev_state.
 * @dev:	pointer to &struct dmxdev.
 * @buffer:	an embedded &struct dvb_ringbuffer buffer.
 * @vb2_ctx:	control struct for VB2 handler
 * @mutex:	protects the access to &struct dmxdev_filter.
 * @timer:	&struct timer_list embedded timer, used to check for
 *		feed timeouts.
 *		Only for section filter.
 * @todo:	index for the @secheader.
 *		Only for section filter.
 * @secheader:	buffer cache to parse the section header.
 *		Only for section filter.
 */
struct dmxdev_filter {
	union {
		struct dmx_section_filter *sec;
	} filter;

	union {
		/* list of TS and PES feeds (struct dmxdev_feed) */
		struct list_head ts;
		struct dmxdev_sec_feed sec;
	} feed;

	union {
		struct dmx_sct_filter_params sec;
		struct dmx_pes_filter_params pes;
	} params;

	struct dmxdev_events_queue events;

	enum dmxdev_type type;
	enum dmxdev_state state;
	struct dmxdev *dev;
	struct dvb_ringbuffer buffer;
	struct dvb_vb2_ctx vb2_ctx;
	struct ion_dma_buff_info buff_dma_info;
	enum dmx_buffer_mode buffer_mode;

	struct mutex mutex;

	/* for recording output */
	enum dmx_tsp_format_t dmx_tsp_format;
	u32 rec_chunk_size;

	/* list of buffers used for insertion (struct ts_insertion_buffer) */
	struct list_head insertion_buffers;

	/* End-of-stream indication has been received */
	int eos_state;

	/* only for sections */
	struct timer_list timer;
	int todo;
	u8 secheader[3];

	struct dmx_secure_mode sec_mode;

	/* Decoder buffer(s) related */
	struct dmx_decoder_buffers decoder_buffers;
};

/**
 * struct dmxdev - Describes a digital TV demux device.
 *
 * @dvbdev:		pointer to &struct dvb_device associated with
 *			the demux device node.
 * @dvr_dvbdev:		pointer to &struct dvb_device associated with
 *			the dvr device node.
 * @filter:		pointer to &struct dmxdev_filter.
 * @demux:		pointer to &struct dmx_demux.
 * @filternum:		number of filters.
 * @capabilities:	demux capabilities as defined by &enum dmx_demux_caps.
 * @may_do_mmap:	flag used to indicate if the device may do mmap.
 * @exit:		flag to indicate that the demux is being released.
 * @dvr_orig_fe:	pointer to &struct dmx_frontend.
 * @dvr_buffer:		embedded &struct dvb_ringbuffer for DVB output.
 * @dvr_vb2_ctx:	control struct for VB2 handler
 * @mutex:		protects the usage of this structure.
 * @lock:		protects access to &dmxdev->filter->data.
 */
struct dmxdev {
	struct dvb_device *dvbdev;
	struct dvb_device *dvr_dvbdev;

	struct dmxdev_filter *filter;
	struct dmx_demux *demux;

	int filternum;
	int capabilities;

	enum dmx_playback_mode_t playback_mode;
	enum dmx_source_t source;

	unsigned int may_do_mmap:1;
	unsigned int exit:1;
	unsigned int dvr_in_exit:1;
	unsigned int dvr_processing_input:1;

#define DMXDEV_CAP_DUPLEX 1
	struct dmx_frontend *dvr_orig_fe;

	struct dvb_ringbuffer dvr_buffer;
	struct ion_dma_buff_info dvr_buff_dma_info;
	enum dmx_buffer_mode dvr_buffer_mode;
	struct dmxdev_events_queue dvr_output_events;
	struct dmxdev_filter *dvr_feed;
	int dvr_feeds_count;

	struct dvb_ringbuffer dvr_input_buffer;
	enum dmx_buffer_mode dvr_input_buffer_mode;
	struct task_struct *dvr_input_thread;
	/* DVR commands (data feed / OOB command) queue */
	struct dvb_ringbuffer dvr_cmd_buffer;

#define DVR_BUFFER_SIZE (10*188*1024)

	struct dvb_vb2_ctx dvr_vb2_ctx;

	struct mutex mutex;
	spinlock_t lock;
	spinlock_t dvr_in_lock;
};

enum dvr_cmd {
	DVR_DATA_FEED_CMD,
	DVR_OOB_CMD
};

struct dvr_command {
	enum dvr_cmd type;
	union {
		struct dmx_oob_command oobcmd;
		size_t data_feed_count;
	} cmd;
};

#define DVR_CMDS_BUFFER_SIZE (sizeof(struct dvr_command)*500)

/**
 * dvb_dmxdev_init - initializes a digital TV demux and registers both demux
 *	and DVR devices.
 *
 * @dmxdev: pointer to &struct dmxdev.
 * @adap: pointer to &struct dvb_adapter.
 */
int dvb_dmxdev_init(struct dmxdev *dmxdev, struct dvb_adapter *adap);

/**
 * dvb_dmxdev_release - releases a digital TV demux and unregisters it.
 *
 * @dmxdev: pointer to &struct dmxdev.
 */
void dvb_dmxdev_release(struct dmxdev *dmxdev);

#endif /* _DMXDEV_H_ */
