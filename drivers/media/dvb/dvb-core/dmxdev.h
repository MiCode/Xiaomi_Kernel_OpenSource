/*
 * dmxdev.h
 *
 * Copyright (C) 2000 Ralph Metzler & Marcus Metzler
 *                    for convergence integrated media GmbH
 *
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef _DMXDEV_H_
#define _DMXDEV_H_

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/dvb/dmx.h>

#include "dvbdev.h"
#include "demux.h"
#include "dvb_ringbuffer.h"

enum dmxdev_type {
	DMXDEV_TYPE_NONE,
	DMXDEV_TYPE_SEC,
	DMXDEV_TYPE_PES,
};

enum dmxdev_state {
	DMXDEV_STATE_FREE,
	DMXDEV_STATE_ALLOCATED,
	DMXDEV_STATE_SET,
	DMXDEV_STATE_GO,
	DMXDEV_STATE_DONE,
	DMXDEV_STATE_TIMEDOUT
};

struct dmxdev_feed {
	u16 pid;
	struct dmx_secure_mode sec_mode;
	struct dmx_indexing_params idx_params;
	struct dmx_ts_feed *ts;
	struct list_head next;
};

struct dmxdev_sec_feed {
	struct dmx_secure_mode sec_mode;
	struct dmx_section_feed *feed;
};

#define DMX_EVENT_QUEUE_SIZE	500 /* number of events */
struct dmxdev_events_queue {
	/*
	 * indices used to manage events queue.
	 * read_index advanced when relevent data is read
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
	void *priv_buff_handle;
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

	/* Decoder buffer(s) related */
	struct dmx_decoder_buffers decoder_buffers;
};

struct dmxdev {
	struct dvb_device *dvbdev;
	struct dvb_device *dvr_dvbdev;

	struct dmxdev_filter *filter;
	struct dmx_demux *demux;

	int filternum;
	int capabilities;
#define DMXDEV_CAP_DUPLEX	0x01
#define DMXDEV_CAP_PULL_MODE	0x02
#define DMXDEV_CAP_INDEXING	0x04
#define DMXDEV_CAP_EXTERNAL_BUFFS_ONLY	0x08
#define DMXDEV_CAP_TS_INSERTION	0x10


	enum dmx_playback_mode_t playback_mode;
	dmx_source_t source;

	unsigned int exit:1;
	unsigned int dvr_in_exit:1;
	unsigned int dvr_processing_input:1;

	struct dmx_frontend *dvr_orig_fe;

	struct dvb_ringbuffer dvr_buffer;
	void *dvr_priv_buff_handle;
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


int dvb_dmxdev_init(struct dmxdev *dmxdev, struct dvb_adapter *);
void dvb_dmxdev_release(struct dmxdev *dmxdev);

#endif /* _DMXDEV_H_ */
