/*
 * dvb_demux.h: DVB kernel demux API
 *
 * Copyright (C) 2000-2001 Marcus Metzler & Ralph Metzler
 *                         for convergence integrated media GmbH
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

#ifndef _DVB_DEMUX_H_
#define _DVB_DEMUX_H_

#include <linux/time.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>

#include "demux.h"

#define DMX_TYPE_TS  0
#define DMX_TYPE_SEC 1
#define DMX_TYPE_PES 2

#define DMX_STATE_FREE      0
#define DMX_STATE_ALLOCATED 1
#define DMX_STATE_SET       2
#define DMX_STATE_READY     3
#define DMX_STATE_GO        4

#define DVB_DEMUX_MASK_MAX 18

#define MAX_PID 0x1fff

#define TIMESTAMP_LEN	4

#define SPEED_PKTS_INTERVAL 50000

struct dvb_demux_filter {
	struct dmx_section_filter filter;
	u8 maskandmode[DMX_MAX_FILTER_SIZE];
	u8 maskandnotmode[DMX_MAX_FILTER_SIZE];
	int doneq;

	struct dvb_demux_filter *next;
	struct dvb_demux_feed *feed;
	int index;
	int state;
	int type;

	u16 hw_handle;
	struct timer_list timer;
};

#define DMX_FEED_ENTRY(pos) list_entry(pos, struct dvb_demux_feed, list_head)


struct dmx_index_entry {
	struct dmx_index_event_info event;
	struct list_head next;
};

#define DMX_IDX_EVENT_QUEUE_SIZE	100
struct dvb_demux_rec_info {
	/* Reference counter for number of feeds using this information */
	int ref_count;

	/* Counter for number of TS packets output to recording buffer */
	u64 ts_output_count;

	/* Indexing information */
	struct {
		/*
		 * Minimum TS packet number encountered in recording filter
		 * among all feeds that search for video patterns
		 */
		u64 min_pattern_tsp_num;

		/* Number of feeds with video pattern search request */
		u8 pattern_search_feeds_num;

		/* Index entries pool */
		struct dmx_index_entry events[DMX_IDX_EVENT_QUEUE_SIZE];

		/* List of free entries that can be used for new index events */
		struct list_head free_list;

		/* List holding ready index entries not notified to user yet */
		struct list_head ready_list;
	} idx_info;
};

#define DVB_DMX_MAX_PATTERN_LEN			6
struct dvb_dmx_video_patterns {
	/* the byte pattern to look for */
	u8 pattern[DVB_DMX_MAX_PATTERN_LEN];

	/* the byte mask to use (same length as pattern) */
	u8 mask[DVB_DMX_MAX_PATTERN_LEN];

	/* the length of the pattern, in bytes */
	size_t size;

	/* the type of the pattern. One of DMX_IDX_* definitions */
	u64 type;
};

#define DVB_DMX_MAX_FOUND_PATTERNS					20
#define DVB_DMX_MAX_SEARCH_PATTERN_NUM				20
struct dvb_dmx_video_prefix_size_masks {
	/*
	 * a bit mask (per pattern) of possible prefix sizes to use
	 * when searching for a pattern that started in the previous TS packet.
	 * Updated by dvb_dmx_video_pattern_search for use in the next lookup.
	 */
	u32 size_mask[DVB_DMX_MAX_FOUND_PATTERNS];
};

struct dvb_dmx_video_patterns_results {
	struct {
		/*
		 * The offset in the buffer where the pattern was found.
		 * If a pattern is found using a prefix (i.e. started on the
		 * previous buffer), offset is zero.
		 */
		u32 offset;

		/*
		 * The type of the pattern found.
		 * One of DMX_IDX_* definitions.
		 */
		u64 type;

		/* The prefix size that was used to find this pattern */
		u32 used_prefix_size;
	} info[DVB_DMX_MAX_FOUND_PATTERNS];
};

struct dvb_demux_feed {
	union {
		struct dmx_ts_feed ts;
		struct dmx_section_feed sec;
	} feed;

	union {
		dmx_ts_cb ts;
		dmx_section_cb sec;
	} cb;

	union {
		dmx_ts_data_ready_cb ts;
		dmx_section_data_ready_cb sec;
	} data_ready_cb;

	struct dvb_demux *demux;
	void *priv;
	int type;
	int state;
	u16 pid;
	u8 *buffer;
	int buffer_size;
	enum dmx_tsp_format_t tsp_out_format;
	struct dmx_secure_mode secure_mode;
	struct dmx_cipher_operations cipher_ops;

	struct timespec timeout;
	struct dvb_demux_filter *filter;

	int ts_type;
	enum dmx_ts_pes pes_type;

	int cc;
	int first_cc;
	int pusi_seen;		/* prevents feeding of garbage from previous section */

	u8 scrambling_bits;

	struct dvb_demux_rec_info *rec_info;
	u64 prev_tsp_num;
	u64 prev_stc;
	u64 curr_pusi_tsp_num;
	u64 prev_pusi_tsp_num;
	int prev_frame_valid;
	u64 prev_frame_type;
	int first_frame_in_seq;
	int first_frame_in_seq_notified;
	u64 last_pattern_tsp_num;
	int pattern_num;
	const struct dvb_dmx_video_patterns
		*patterns[DVB_DMX_MAX_SEARCH_PATTERN_NUM];
	struct dvb_dmx_video_prefix_size_masks prefix_size;

	u32 peslen;
	u32 pes_tei_counter;
	u32 pes_cont_err_counter;
	u32 pes_ts_packets_num;

	struct list_head list_head;
	unsigned int index;	/* a unique index for each feed (can be used as hardware pid filter index) */

	enum dmx_video_codec video_codec;
	struct dmx_indexing_params idx_params;
};

struct dvb_demux {
	struct dmx_demux dmx;
	void *priv;
	int filternum;
	int feednum;
	int (*start_feed)(struct dvb_demux_feed *feed);
	int (*stop_feed)(struct dvb_demux_feed *feed);
	int (*write_to_decoder)(struct dvb_demux_feed *feed,
				 const u8 *buf, size_t len);
	int (*decoder_fullness_init)(struct dvb_demux_feed *feed);
	int (*decoder_fullness_wait)(struct dvb_demux_feed *feed,
				 size_t required_space);
	int (*decoder_fullness_abort)(struct dvb_demux_feed *feed);
	int (*decoder_buffer_status)(struct dvb_demux_feed *feed,
				struct dmx_buffer_status *dmx_buffer_status);
	int (*reuse_decoder_buffer)(struct dvb_demux_feed *feed,
				int cookie);
	int (*set_cipher_op)(struct dvb_demux_feed *feed,
				struct dmx_cipher_operations *cipher_ops);
	u32 (*check_crc32)(struct dvb_demux_feed *feed,
			    const u8 *buf, size_t len);
	void (*memcopy)(struct dvb_demux_feed *feed, u8 *dst,
			 const u8 *src, size_t len);
	int (*oob_command)(struct dvb_demux_feed *feed,
		struct dmx_oob_command *cmd);
	void (*convert_ts)(struct dvb_demux_feed *feed,
			 const u8 timestamp[TIMESTAMP_LEN],
			 u64 *timestampIn27Mhz);

	int users;
#define MAX_DVB_DEMUX_USERS 10
	struct dvb_demux_filter *filter;
	struct dvb_demux_feed *feed;

	struct list_head frontend_list;

	struct dvb_demux_feed *pesfilter[DMX_TS_PES_OTHER];
	u16 pids[DMX_TS_PES_OTHER];
	int playing;
	int recording;

#define DMX_MAX_PID 0x2000
	struct list_head feed_list;
	u8 tsbuf[204];
	int tsbufp;

	struct mutex mutex;
	spinlock_t lock;

	uint8_t *cnt_storage; /* for TS continuity check */

	struct timespec speed_last_time; /* for TS speed check */
	uint32_t speed_pkts_cnt; /* for TS speed check */

	enum dmx_tsp_format_t tsp_format;
	size_t ts_packet_size;

	enum dmx_playback_mode_t playback_mode;
	int sw_filter_abort;

	struct {
		dmx_ts_fullness ts;
		dmx_section_fullness sec;
	} buffer_ctrl;

	struct dvb_demux_rec_info *rec_info_pool;

	/*
	 * the following is used for debugfs exposing info
	 * about dvb demux performance.
	 */
#define MAX_DVB_DEMUX_NAME_LEN 10
	char alias[MAX_DVB_DEMUX_NAME_LEN];

	u32 total_process_time;
	u32 total_crc_time;
};

int dvb_dmx_init(struct dvb_demux *dvbdemux);
void dvb_dmx_release(struct dvb_demux *dvbdemux);
void dvb_dmx_swfilter_section_packets(struct dvb_demux *demux, const u8 *buf,
			      size_t count);
void dvb_dmx_swfilter_packets(struct dvb_demux *dvbdmx, const u8 *buf,
			      size_t count);
void dvb_dmx_swfilter(struct dvb_demux *demux, const u8 *buf, size_t count);
void dvb_dmx_swfilter_204(struct dvb_demux *demux, const u8 *buf,
			  size_t count);
void dvb_dmx_swfilter_format(
			struct dvb_demux *demux, const u8 *buf,
			size_t count,
			enum dmx_tsp_format_t tsp_format);
void dvb_dmx_swfilter_packet(struct dvb_demux *demux, const u8 *buf,
				const u8 timestamp[TIMESTAMP_LEN]);
const struct dvb_dmx_video_patterns *dvb_dmx_get_pattern(u64 dmx_idx_pattern);
int dvb_dmx_video_pattern_search(
		const struct dvb_dmx_video_patterns
			*patterns[DVB_DMX_MAX_SEARCH_PATTERN_NUM],
		int patterns_num,
		const u8 *buf, size_t buf_size,
		struct dvb_dmx_video_prefix_size_masks *prefix_size_masks,
		struct dvb_dmx_video_patterns_results *results);
int dvb_demux_push_idx_event(struct dvb_demux_feed *feed,
		struct dmx_index_event_info *idx_event);
void dvb_dmx_process_idx_pattern(struct dvb_demux_feed *feed,
		struct dvb_dmx_video_patterns_results *patterns, int pattern,
		u64 curr_stc, u64 prev_stc,
		u64 curr_match_tsp, u64 prev_match_tsp,
		u64 curr_pusi_tsp, u64 prev_pusi_tsp);
void dvb_dmx_notify_idx_events(struct dvb_demux_feed *feed);
int dvb_dmx_notify_section_event(struct dvb_demux_feed *feed,
	struct dmx_data_ready *event, int should_lock);

/**
 * dvb_dmx_is_video_feed - Returns whether the PES feed
 * is video one.
 *
 * @feed: The feed to be checked.
 *
 * Return     1 if feed is video feed, 0 otherwise.
 */
static inline int dvb_dmx_is_video_feed(struct dvb_demux_feed *feed)
{
	if (feed->type != DMX_TYPE_TS)
		return 0;

	if (feed->ts_type & (~TS_DECODER))
		return 0;

	if ((feed->pes_type == DMX_TS_PES_VIDEO0) ||
		(feed->pes_type == DMX_TS_PES_VIDEO1) ||
		(feed->pes_type == DMX_TS_PES_VIDEO2) ||
		(feed->pes_type == DMX_TS_PES_VIDEO3))
		return 1;

	return 0;
}

/**
 * dvb_dmx_is_pcr_feed - Returns whether the PES feed
 * is PCR one.
 *
 * @feed: The feed to be checked.
 *
 * Return     1 if feed is PCR feed, 0 otherwise.
 */
static inline int dvb_dmx_is_pcr_feed(struct dvb_demux_feed *feed)
{
	if (feed->type != DMX_TYPE_TS)
		return 0;

	if (feed->ts_type & (~TS_DECODER))
		return 0;

	if ((feed->pes_type == DMX_TS_PES_PCR0) ||
		(feed->pes_type == DMX_TS_PES_PCR1) ||
		(feed->pes_type == DMX_TS_PES_PCR2) ||
		(feed->pes_type == DMX_TS_PES_PCR3))
		return 1;

	return 0;
}

/**
 * dvb_dmx_is_sec_feed - Returns whether this is a section feed
 *
 * @feed: The feed to be checked.
 *
 * Return 1 if feed is a section feed, 0 otherwise.
 */
static inline int dvb_dmx_is_sec_feed(struct dvb_demux_feed *feed)
{
	return (feed->type == DMX_TYPE_SEC);
}

/**
 * dvb_dmx_is_rec_feed - Returns whether this is a recording feed
 *
 * @feed: The feed to be checked.
 *
 * Return 1 if feed is recording feed, 0 otherwise.
 */
static inline int dvb_dmx_is_rec_feed(struct dvb_demux_feed *feed)
{
	if (feed->type != DMX_TYPE_TS)
		return 0;

	if (feed->ts_type & (TS_DECODER | TS_PAYLOAD_ONLY))
		return 0;

	return 1;
}


#endif /* _DVB_DEMUX_H_ */
