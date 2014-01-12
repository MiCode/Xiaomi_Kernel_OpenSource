/*
 * demux.h
 *
 * Copyright (c) 2002 Convergence GmbH
 *
 * based on code:
 * Copyright (c) 2000 Nokia Research Center
 *                    Tampere, FINLAND
 *
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#ifndef __DEMUX_H
#define __DEMUX_H

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/time.h>
#include <linux/dvb/dmx.h>

/*--------------------------------------------------------------------------*/
/* Common definitions */
/*--------------------------------------------------------------------------*/

#define DMX_EVENT_QUEUE_SIZE	500 /* number of events */

/*
 * DMX_MAX_FILTER_SIZE: Maximum length (in bytes) of a section/PES filter.
 */

#ifndef DMX_MAX_FILTER_SIZE
#define DMX_MAX_FILTER_SIZE 18
#endif

/*
 * DMX_MAX_SECFEED_SIZE: Maximum length (in bytes) of a private section feed filter.
 */

#ifndef DMX_MAX_SECTION_SIZE
#define DMX_MAX_SECTION_SIZE 4096
#endif
#ifndef DMX_MAX_SECFEED_SIZE
#define DMX_MAX_SECFEED_SIZE (DMX_MAX_SECTION_SIZE + 188)
#endif


/*
 * enum dmx_success: Success codes for the Demux Callback API.
 */
enum dmx_success {
	DMX_OK = 0, /* Received Ok */
	DMX_OK_PES_END, /* Received OK, data reached end of PES packet */
	DMX_OK_PCR, /* Received OK, data with new PCR/STC pair */
	DMX_OK_EOS, /* Received OK, reached End-of-Stream (EOS) */
	DMX_OK_MARKER, /* Received OK, reached a data Marker */
	DMX_LENGTH_ERROR, /* Incorrect length */
	DMX_OVERRUN_ERROR, /* Receiver ring buffer overrun */
	DMX_CRC_ERROR, /* Incorrect CRC */
	DMX_FRAME_ERROR, /* Frame alignment error */
	DMX_FIFO_ERROR, /* Receiver FIFO overrun */
	DMX_MISSED_ERROR, /* Receiver missed packet */
	DMX_OK_DECODER_BUF, /* Received OK, new ES data in decoder buffer */
	DMX_OK_IDX, /* Received OK, new index event */
	DMX_OK_SCRAMBLING_STATUS, /* Received OK, new scrambling status */
};


/*
 * struct dmx_data_ready: Parameters for event notification callback.
 * Event notification notifies demux device that data is written
 * and available in the device's output buffer or provides
 * notification on errors and other events. In the latter case
 * data_length is zero.
 */
struct dmx_data_ready {
	enum dmx_success status;

	/*
	 * data_length may be 0 in case of DMX_OK_PES_END or DMX_OK_EOS
	 * and in non-DMX_OK_XXX events. In DMX_OK_PES_END,
	 * data_length is for data coming after the end of PES.
	 */
	int data_length;

	union {
		struct {
			int start_gap;
			int actual_length;
			int disc_indicator_set;
			int pes_length_mismatch;
			u64 stc;
			u32 tei_counter;
			u32 cont_err_counter;
			u32 ts_packets_num;
		} pes_end;

		struct {
			u64 pcr;
			u64 stc;
			int disc_indicator_set;
		} pcr;

		struct {
			int handle;
			int cookie;
			u32 offset;
			u32 len;
			int pts_exists;
			u64 pts;
			int dts_exists;
			u64 dts;
			u32 tei_counter;
			u32 cont_err_counter;
			u32 ts_packets_num;
			u32 ts_dropped_bytes;
			u64 stc;
		} buf;

		struct {
			u64 id;
		} marker;

		struct dmx_index_event_info idx_event;
		struct dmx_scrambling_status_event_info scrambling_bits;
	};
};

/*
 * struct data_buffer: Parameters of buffer allocated by
 * demux device for input/output. Can be used to directly map the
 * demux-device buffer to HW output if HW supports it.
 */
struct data_buffer {
	/* dvb_ringbuffer managed by demux-device */
	const struct dvb_ringbuffer *ringbuff;


	/*
	 * Private handle returned by kernel demux when
	 * map_buffer is called in case external buffer
	 * is used. NULL if buffer is allocated internally.
	 */
	void *priv_handle;
};

/*--------------------------------------------------------------------------*/
/* TS packet reception */
/*--------------------------------------------------------------------------*/

/* TS filter type for set() */

#define TS_PACKET       1   /* send TS packets (188 bytes) to callback (default) */
#define	TS_PAYLOAD_ONLY 2   /* in case TS_PACKET is set, only send the TS
			       payload (<=184 bytes per packet) to callback */
#define TS_DECODER      4   /* send stream to built-in decoder (if present) */
#define TS_DEMUX        8   /* in case TS_PACKET is set, send the TS to
			       the demux device, not to the dvr device */

struct dmx_ts_feed;
typedef int (*dmx_ts_data_ready_cb)(
		struct dmx_ts_feed *source,
		struct dmx_data_ready *dmx_data_ready);

struct dmx_ts_feed {
	int is_filtering; /* Set to non-zero when filtering in progress */
	struct dmx_demux *parent; /* Back-pointer */
	struct data_buffer buffer;
	void *priv; /* Pointer to private data of the API client */
	struct dmx_decoder_buffers *decoder_buffers;
	int (*set) (struct dmx_ts_feed *feed,
		    u16 pid,
		    int type,
		    enum dmx_ts_pes pes_type,
		    size_t circular_buffer_size,
		    struct timespec timeout);
	int (*start_filtering) (struct dmx_ts_feed* feed);
	int (*stop_filtering) (struct dmx_ts_feed* feed);
	int (*set_video_codec) (struct dmx_ts_feed *feed,
				enum dmx_video_codec video_codec);
	int (*set_idx_params) (struct dmx_ts_feed *feed,
				struct dmx_indexing_params *idx_params);
	int (*get_decoder_buff_status)(
			struct dmx_ts_feed *feed,
			struct dmx_buffer_status *dmx_buffer_status);
	int (*reuse_decoder_buffer)(
			struct dmx_ts_feed *feed,
			int cookie);
	int (*data_ready_cb)(struct dmx_ts_feed *feed,
			dmx_ts_data_ready_cb callback);
	int (*notify_data_read)(struct dmx_ts_feed *feed,
			u32 bytes_num);
	int (*set_tsp_out_format)(struct dmx_ts_feed *feed,
				enum dmx_tsp_format_t tsp_format);
	int (*set_secure_mode)(struct dmx_ts_feed *feed,
				struct dmx_secure_mode *sec_mode);
	int (*set_cipher_ops)(struct dmx_ts_feed *feed,
				struct dmx_cipher_operations *cipher_ops);
	int (*oob_command) (struct dmx_ts_feed *feed,
			struct dmx_oob_command *cmd);
	int (*ts_insertion_init)(struct dmx_ts_feed *feed);
	int (*ts_insertion_terminate)(struct dmx_ts_feed *feed);
	int (*ts_insertion_insert_buffer)(struct dmx_ts_feed *feed,
			char *data, size_t size);
	int (*get_scrambling_bits)(struct dmx_ts_feed *feed, u8 *value);
	int (*flush_buffer)(struct dmx_ts_feed *feed, size_t length);
};

/*--------------------------------------------------------------------------*/
/* Section reception */
/*--------------------------------------------------------------------------*/

struct dmx_section_filter {
	u8 filter_value [DMX_MAX_FILTER_SIZE];
	u8 filter_mask [DMX_MAX_FILTER_SIZE];
	u8 filter_mode [DMX_MAX_FILTER_SIZE];
	struct dmx_section_feed* parent; /* Back-pointer */
	struct data_buffer buffer;
	void* priv; /* Pointer to private data of the API client */
};

struct dmx_section_feed;
typedef int (*dmx_section_data_ready_cb)(
		struct dmx_section_filter *source,
		struct dmx_data_ready *dmx_data_ready);

struct dmx_section_feed {
	int is_filtering; /* Set to non-zero when filtering in progress */
	struct dmx_demux* parent; /* Back-pointer */
	void* priv; /* Pointer to private data of the API client */

	int check_crc;
	u32 crc_val;

	u8 *secbuf;
	u8 secbuf_base[DMX_MAX_SECFEED_SIZE];
	u16 secbufp, seclen, tsfeedp;

	int (*set) (struct dmx_section_feed* feed,
		    u16 pid,
		    size_t circular_buffer_size,
		    int check_crc);
	int (*allocate_filter) (struct dmx_section_feed* feed,
				struct dmx_section_filter** filter);
	int (*release_filter) (struct dmx_section_feed* feed,
			       struct dmx_section_filter* filter);
	int (*start_filtering) (struct dmx_section_feed* feed);
	int (*stop_filtering) (struct dmx_section_feed* feed);
	int (*data_ready_cb)(struct dmx_section_feed *feed,
			dmx_section_data_ready_cb callback);
	int (*notify_data_read)(struct dmx_section_filter *filter,
			u32 bytes_num);
	int (*set_secure_mode)(struct dmx_section_feed *feed,
				struct dmx_secure_mode *sec_mode);
	int (*set_cipher_ops)(struct dmx_section_feed *feed,
				struct dmx_cipher_operations *cipher_ops);
	int (*oob_command) (struct dmx_section_feed *feed,
				struct dmx_oob_command *cmd);
	int (*get_scrambling_bits)(struct dmx_section_feed *feed, u8 *value);
	int (*flush_buffer)(struct dmx_section_feed *feed, size_t length);
};

/*--------------------------------------------------------------------------*/
/* Callback functions */
/*--------------------------------------------------------------------------*/

typedef int (*dmx_ts_cb) ( const u8 * buffer1,
			   size_t buffer1_length,
			   const u8 * buffer2,
			   size_t buffer2_length,
			   struct dmx_ts_feed* source,
			   enum dmx_success success);

typedef int (*dmx_section_cb) (	const u8 * buffer1,
				size_t buffer1_len,
				const u8 * buffer2,
				size_t buffer2_len,
				struct dmx_section_filter * source,
				enum dmx_success success);

typedef int (*dmx_ts_fullness) (
				struct dmx_ts_feed *source,
				int required_space,
				int wait);

typedef int (*dmx_section_fullness) (
				struct dmx_section_filter *source,
				int required_space,
				int wait);

/*--------------------------------------------------------------------------*/
/* DVB Front-End */
/*--------------------------------------------------------------------------*/

enum dmx_frontend_source {
	DMX_MEMORY_FE,
	DMX_FRONTEND_0,
	DMX_FRONTEND_1,
	DMX_FRONTEND_2,
	DMX_FRONTEND_3,
	DMX_STREAM_0,    /* external stream input, e.g. LVDS */
	DMX_STREAM_1,
	DMX_STREAM_2,
	DMX_STREAM_3
};

struct dmx_frontend {
	struct list_head connectivity_list; /* List of front-ends that can
					       be connected to a particular
					       demux */
	enum dmx_frontend_source source;
};

/*--------------------------------------------------------------------------*/
/* MPEG-2 TS Demux */
/*--------------------------------------------------------------------------*/

/*
 * Flags OR'ed in the capabilities field of struct dmx_demux.
 */

#define DMX_TS_FILTERING                        1
#define DMX_PES_FILTERING                       2
#define DMX_SECTION_FILTERING                   4
#define DMX_MEMORY_BASED_FILTERING              8    /* write() available */
#define DMX_CRC_CHECKING                        16
#define DMX_TS_DESCRAMBLING                     32

/*
 * Demux resource type identifier.
*/

/*
 * DMX_FE_ENTRY(): Casts elements in the list of registered
 * front-ends from the generic type struct list_head
 * to the type * struct dmx_frontend
 *.
*/

#define DMX_FE_ENTRY(list) list_entry(list, struct dmx_frontend, connectivity_list)

struct dmx_demux {
	u32 capabilities;            /* Bitfield of capability flags */
	struct dmx_frontend* frontend;    /* Front-end connected to the demux */
	void* priv;                  /* Pointer to private data of the API client */
	struct data_buffer dvr_input; /* DVR input buffer */
	int dvr_input_protected;
	struct dentry *debugfs_demux_dir; /* debugfs dir */

	int (*open) (struct dmx_demux* demux);
	int (*close) (struct dmx_demux* demux);
	int (*write) (struct dmx_demux *demux, const char *buf, size_t count);
	int (*allocate_ts_feed) (struct dmx_demux* demux,
				 struct dmx_ts_feed** feed,
				 dmx_ts_cb callback);
	int (*release_ts_feed) (struct dmx_demux* demux,
				struct dmx_ts_feed* feed);
	int (*allocate_section_feed) (struct dmx_demux* demux,
				      struct dmx_section_feed** feed,
				      dmx_section_cb callback);
	int (*release_section_feed) (struct dmx_demux* demux,
				     struct dmx_section_feed* feed);
	int (*add_frontend) (struct dmx_demux* demux,
			     struct dmx_frontend* frontend);
	int (*remove_frontend) (struct dmx_demux* demux,
				struct dmx_frontend* frontend);
	struct list_head* (*get_frontends) (struct dmx_demux* demux);
	int (*connect_frontend) (struct dmx_demux* demux,
				 struct dmx_frontend* frontend);
	int (*disconnect_frontend) (struct dmx_demux* demux);

	int (*get_pes_pids) (struct dmx_demux* demux, u16 *pids);

	int (*get_caps) (struct dmx_demux* demux, struct dmx_caps *caps);

	int (*set_source) (struct dmx_demux *demux, const dmx_source_t *src);

	int (*set_tsp_format) (struct dmx_demux *demux,
				enum dmx_tsp_format_t tsp_format);

	int (*set_playback_mode) (struct dmx_demux *demux,
				 enum dmx_playback_mode_t mode,
				 dmx_ts_fullness ts_fullness_callback,
				 dmx_section_fullness sec_fullness_callback);

	int (*write_cancel) (struct dmx_demux *demux);

	int (*get_stc) (struct dmx_demux* demux, unsigned int num,
			u64 *stc, unsigned int *base);

	int (*map_buffer) (struct dmx_demux *demux,
			struct dmx_buffer *dmx_buffer,
			void **priv_handle, void **mem);

	int (*unmap_buffer) (struct dmx_demux *demux,
			void *priv_handle);

	int (*get_tsp_size) (struct dmx_demux *demux);
};

#endif /* #ifndef __DEMUX_H */
