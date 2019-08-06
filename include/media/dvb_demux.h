/*
 * dvb_demux.h: DVB kernel demux API
 *
 * Copyright (C) 2000-2001 Marcus Metzler & Ralph Metzler
 *                         for convergence integrated media GmbH
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

#ifndef _DVB_DEMUX_H_
#define _DVB_DEMUX_H_

#include <linux/time.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>

#include <media/demux.h>

/**
 * enum dvb_dmx_filter_type - type of demux feed.
 *
 * @DMX_TYPE_TS:	feed is in TS mode.
 * @DMX_TYPE_SEC:	feed is in Section mode.
 */
enum dvb_dmx_filter_type {
	DMX_TYPE_TS,
	DMX_TYPE_SEC,
};

/**
 * enum dvb_dmx_state - state machine for a demux filter.
 *
 * @DMX_STATE_FREE:		indicates that the filter is freed.
 * @DMX_STATE_ALLOCATED:	indicates that the filter was allocated
 *				to be used.
 * @DMX_STATE_READY:		indicates that the filter is ready
 *				to be used.
 * @DMX_STATE_GO:		indicates that the filter is running.
 */
enum dvb_dmx_state {
	DMX_STATE_FREE,
	DMX_STATE_ALLOCATED,
	DMX_STATE_READY,
	DMX_STATE_GO,
};

#define DVB_DEMUX_MASK_MAX 18

#define MAX_PID 0x1fff

#define TIMESTAMP_LEN	4

#define SPEED_PKTS_INTERVAL 50000

/**
 * struct dvb_demux_filter - Describes a DVB demux section filter.
 *
 * @filter:		Section filter as defined by &struct dmx_section_filter.
 * @maskandmode:	logical ``and`` bit mask.
 * @maskandnotmode:	logical ``and not`` bit mask.
 * @doneq:		flag that indicates when a filter is ready.
 * @next:		pointer to the next section filter.
 * @feed:		&struct dvb_demux_feed pointer.
 * @index:		index of the used demux filter.
 * @state:		state of the filter as described by &enum dvb_dmx_state.
 * @type:		type of the filter as described
 *			by &enum dvb_dmx_filter_type.
 */

struct dvb_demux_filter {
	struct dmx_section_filter filter;
	u8 maskandmode[DMX_MAX_FILTER_SIZE];
	u8 maskandnotmode[DMX_MAX_FILTER_SIZE];
	bool doneq;

	struct dvb_demux_filter *next;
	struct dvb_demux_feed *feed;
	int index;
	enum dvb_dmx_state state;
	enum dvb_dmx_filter_type type;

	/* private: used only by av7110 */
	u16 hw_handle;
};

struct dmx_index_entry {
	struct dmx_index_event_info event;
	struct list_head next;
};

#define DMX_IDX_EVENT_QUEUE_SIZE	DMX_EVENT_QUEUE_SIZE

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

		/* Number of indexing-enabled feeds */
		u8 indexing_feeds_num;

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


/**
 * struct dvb_demux_feed - describes a DVB field
 *
 * @feed:	a union describing a digital TV feed.
 *		Depending on the feed type, it can be either
 *		@feed.ts or @feed.sec.
 * @feed.ts:	a &struct dmx_ts_feed pointer.
 *		For TS feed only.
 * @feed.sec:	a &struct dmx_section_feed pointer.
 *		For section feed only.
 * @cb:		a union describing digital TV callbacks.
 *		Depending on the feed type, it can be either
 *		@cb.ts or @cb.sec.
 * @cb.ts:	a dmx_ts_cb() calback function pointer.
 *		For TS feed only.
 * @cb.sec:	a dmx_section_cb() callback function pointer.
 *		For section feed only.
 * @demux:	pointer to &struct dvb_demux.
 * @priv:	private data that can optionally be used by a DVB driver.
 * @type:	type of the filter, as defined by &enum dvb_dmx_filter_type.
 * @state:	state of the filter as defined by &enum dvb_dmx_state.
 * @pid:	PID to be filtered.
 * @timeout:	feed timeout.
 * @filter:	pointer to &struct dvb_demux_filter.
 * @buffer_flags: Buffer flags used to report discontinuity users via DVB
 *		  memory mapped API, as defined by &enum dmx_buffer_flags.
 * @ts_type:	type of TS, as defined by &enum ts_filter_type.
 * @pes_type:	type of PES, as defined by &enum dmx_ts_pes.
 * @cc:		MPEG-TS packet continuity counter
 * @pusi_seen:	if true, indicates that a discontinuity was detected.
 *		it is used to prevent feeding of garbage from previous section.
 * @peslen:	length of the PES (Packet Elementary Stream).
 * @list_head:	head for the list of digital TV demux feeds.
 * @index:	a unique index for each feed. Can be used as hardware
 *		pid filter index.
 */
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
	enum dvb_dmx_filter_type type;
	enum dvb_dmx_state state;
	u16 pid;
	u8 *buffer;
	int buffer_size;
	enum dmx_tsp_format_t tsp_out_format;
	struct dmx_secure_mode secure_mode;
	struct dmx_cipher_operations cipher_ops;

	ktime_t timeout;
	struct dvb_demux_filter *filter;

	u32 buffer_flags;

	enum ts_filter_type ts_type;
	enum dmx_ts_pes pes_type;

	int cc;
	int first_cc;
	bool pusi_seen;

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
const struct dvb_dmx_video_patterns *patterns[DVB_DMX_MAX_SEARCH_PATTERN_NUM];
	struct dvb_dmx_video_prefix_size_masks prefix_size;
	u16 peslen;
	u32 pes_tei_counter;
	u32 pes_cont_err_counter;
	u32 pes_ts_packets_num;

	struct list_head list_head;
	/*
	 * a unique index for each feed
	 * (can be used as hardware pid filter index)
	 */
	unsigned int index;

	enum dmx_video_codec video_codec;
	struct dmx_indexing_params idx_params;
};

/**
 * struct dvb_demux - represents a digital TV demux
 * @dmx:		embedded &struct dmx_demux with demux capabilities
 *			and callbacks.
 * @priv:		private data that can optionally be used by
 *			a DVB driver.
 * @filternum:		maximum amount of DVB filters.
 * @feednum:		maximum amount of DVB feeds.
 * @start_feed:		callback routine to be called in order to start
 *			a DVB feed.
 * @stop_feed:		callback routine to be called in order to stop
 *			a DVB feed.
 * @write_to_decoder:	callback routine to be called if the feed is TS and
 *			it is routed to an A/V decoder, when a new TS packet
 *			is received.
 *			Used only on av7110-av.c.
 * @check_crc32:	callback routine to check CRC. If not initialized,
 *			dvb_demux will use an internal one.
 * @memcopy:		callback routine to memcopy received data.
 *			If not initialized, dvb_demux will default to memcpy().
 * @users:		counter for the number of demux opened file descriptors.
 *			Currently, it is limited to 10 users.
 * @filter:		pointer to &struct dvb_demux_filter.
 * @feed:		pointer to &struct dvb_demux_feed.
 * @frontend_list:	&struct list_head with frontends used by the demux.
 * @pesfilter:		array of &struct dvb_demux_feed with the PES types
 *			that will be filtered.
 * @pids:		list of filtered program IDs.
 * @feed_list:		&struct list_head with feeds.
 * @tsbuf:		temporary buffer used internally to store TS packets.
 * @tsbufp:		temporary buffer index used internally.
 * @mutex:		pointer to &struct mutex used to protect feed set
 *			logic.
 * @lock:		pointer to &spinlock_t, used to protect buffer handling.
 * @cnt_storage:	buffer used for TS/TEI continuity check.
 * @speed_last_time:	&ktime_t used for TS speed check.
 * @speed_pkts_cnt:	packets count used for TS speed check.
 */
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
	int (*set_indexing)(struct dvb_demux_feed *feed);
	int (*flush_decoder_buffer)(struct dvb_demux_feed *feed, size_t length);

	int users;
#define MAX_DVB_DEMUX_USERS 10
	struct dvb_demux_filter *filter;
	struct dvb_demux_feed *feed;

	struct list_head frontend_list;

	struct dvb_demux_feed *pesfilter[DMX_PES_OTHER];
	u16 pids[DMX_PES_OTHER];

#define DMX_MAX_PID 0x2000
	struct list_head feed_list;
	u8 tsbuf[204];
	int tsbufp;

	struct mutex mutex;
	spinlock_t lock;

	uint8_t *cnt_storage; /* for TS continuity check */

	ktime_t speed_last_time; /* for TS speed check */
	uint32_t speed_pkts_cnt; /* for TS speed check */

	/* private: used only on av7110 */
	int playing;
	int recording;
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

/**
 * dvb_dmx_init - initialize a digital TV demux struct.
 *
 * @demux: &struct dvb_demux to be initialized.
 *
 * Before being able to register a digital TV demux struct, drivers
 * should call this routine. On its typical usage, some fields should
 * be initialized at the driver before calling it.
 *
 * A typical usecase is::
 *
 *	dvb->demux.dmx.capabilities =
 *		DMX_TS_FILTERING | DMX_SECTION_FILTERING |
 *		DMX_MEMORY_BASED_FILTERING;
 *	dvb->demux.priv       = dvb;
 *	dvb->demux.filternum  = 256;
 *	dvb->demux.feednum    = 256;
 *	dvb->demux.start_feed = driver_start_feed;
 *	dvb->demux.stop_feed  = driver_stop_feed;
 *	ret = dvb_dmx_init(&dvb->demux);
 *	if (ret < 0)
 *		return ret;
 */
int dvb_dmx_init(struct dvb_demux *demux);

/**
 * dvb_dmx_release - releases a digital TV demux internal buffers.
 *
 * @demux: &struct dvb_demux to be released.
 *
 * The DVB core internally allocates data at @demux. This routine
 * releases those data. Please notice that the struct itelf is not
 * released, as it can be embedded on other structs.
 */
void dvb_dmx_release(struct dvb_demux *demux);

int dvb_dmx_swfilter_section_packet(struct dvb_demux_feed *feed, const u8 *buf,
	int should_lock);
/**
 * dvb_dmx_swfilter_packets - use dvb software filter for a buffer with
 *	multiple MPEG-TS packets with 188 bytes each.
 *
 * @demux: pointer to &struct dvb_demux
 * @buf: buffer with data to be filtered
 * @count: number of MPEG-TS packets with size of 188.
 *
 * The routine will discard a DVB packet that don't start with 0x47.
 *
 * Use this routine if the DVB demux fills MPEG-TS buffers that are
 * already aligned.
 *
 * NOTE: The @buf size should have size equal to ``count * 188``.
 */
void dvb_dmx_swfilter_packets(struct dvb_demux *demux, const u8 *buf,
			      size_t count);
/**
 * dvb_dmx_swfilter -  use dvb software filter for a buffer with
 *	multiple MPEG-TS packets with 188 bytes each.
 *
 * @demux: pointer to &struct dvb_demux
 * @buf: buffer with data to be filtered
 * @count: number of MPEG-TS packets with size of 188.
 *
 * If a DVB packet doesn't start with 0x47, it will seek for the first
 * byte that starts with 0x47.
 *
 * Use this routine if the DVB demux fill buffers that may not start with
 * a packet start mark (0x47).
 *
 * NOTE: The @buf size should have size equal to ``count * 188``.
 */
void dvb_dmx_swfilter(struct dvb_demux *demux, const u8 *buf, size_t count);
/**
 * dvb_dmx_swfilter_204 -  use dvb software filter for a buffer with
 *	multiple MPEG-TS packets with 204 bytes each.
 *
 * @demux: pointer to &struct dvb_demux
 * @buf: buffer with data to be filtered
 * @count: number of MPEG-TS packets with size of 204.
 *
 * If a DVB packet doesn't start with 0x47, it will seek for the first
 * byte that starts with 0x47.
 *
 * Use this routine if the DVB demux fill buffers that may not start with
 * a packet start mark (0x47).
 *
 * NOTE: The @buf size should have size equal to ``count * 204``.
 */
void dvb_dmx_swfilter_204(struct dvb_demux *demux, const u8 *buf,
			  size_t count);
/**
 * dvb_dmx_swfilter_raw -  make the raw data available to userspace without
 *	filtering
 *
 * @demux: pointer to &struct dvb_demux
 * @buf: buffer with data
 * @count: number of packets to be passed. The actual size of each packet
 *	depends on the &dvb_demux->feed->cb.ts logic.
 *
 * Use it if the driver needs to deliver the raw payload to userspace without
 * passing through the kernel demux. That is meant to support some
 * delivery systems that aren't based on MPEG-TS.
 *
 * This function relies on &dvb_demux->feed->cb.ts to actually handle the
 * buffer.
 */
void dvb_dmx_swfilter_raw(struct dvb_demux *demux, const u8 *buf,
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
		struct dmx_index_event_info *idx_event, int should_lock);
void dvb_dmx_process_idx_pattern(struct dvb_demux_feed *feed,
		struct dvb_dmx_video_patterns_results *patterns, int pattern,
		u64 curr_stc, u64 prev_stc,
		u64 curr_match_tsp, u64 prev_match_tsp,
		u64 curr_pusi_tsp, u64 prev_pusi_tsp);
void dvb_dmx_notify_idx_events(struct dvb_demux_feed *feed, int should_lock);
int dvb_dmx_notify_section_event(struct dvb_demux_feed *feed,
	struct dmx_data_ready *event, int should_lock);
void dvbdmx_ts_reset_pes_state(struct dvb_demux_feed *feed);

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

	if ((feed->pes_type == DMX_PES_VIDEO0) ||
		(feed->pes_type == DMX_PES_VIDEO1) ||
		(feed->pes_type == DMX_PES_VIDEO2) ||
		(feed->pes_type == DMX_PES_VIDEO3))
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

	if ((feed->pes_type == DMX_PES_PCR0) ||
		(feed->pes_type == DMX_PES_PCR1) ||
		(feed->pes_type == DMX_PES_PCR2) ||
		(feed->pes_type == DMX_PES_PCR3))
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

static inline u16 ts_pid(const u8 *buf)
{
	return ((buf[1] & 0x1f) << 8) + buf[2];
}


#endif /* _DVB_DEMUX_H_ */
