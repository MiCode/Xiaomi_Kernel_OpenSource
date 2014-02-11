/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MPQ_DMX_TSPPV2_PLUGIN_H
#define _MPQ_DMX_TSPPV2_PLUGIN_H

#include <linux/types.h>
#include <linux/ion.h>
#include <linux/msm-sps.h>
#include <linux/timer.h>
#include <mach/msm_tspp2.h>
#include "mpq_dmx_plugin_common.h"

#define TSPP2_DMX_MAX_CIPHER_OPS		5

/*
 * Allocate source per possible TSIF input, and per demux instance as any
 * instance may use it's DVR as BAM source.
 */
#define TSPP2_DMX_SOURCE_COUNT			(TSPP2_NUM_TSIF_INPUTS + \
						CONFIG_DVB_MPQ_NUM_DMX_DEVICES)

#define TSPP2_DMX_SOURCE_NAME_LENGTH		10

/* Max number of PID filters */
#define TSPP2_DMX_MAX_PID_FILTER_NUM		128

#define TSPP2_DMX_MAX_FEED_OPS			4

#define TSPP2_DMX_PIPE_WORK_POOL_SIZE		500

/* Polling timer interval in milliseconds  */
#define TSPP2_DMX_POLL_TIMER_INTERVAL_MSEC	10

#define VPES_HEADER_DATA_SIZE			204

/* Polling interval of scrambling bit status in milliseconds */
#define TSPP2_DMX_SB_MONITOR_INTERVAL		50

/* Number of identical scrambling bit samples considered stable to report */
#define TSPP2_DMX_SB_MONITOR_THRESHOLD		3

/* Sizes of BAM descriptor */
#define TSPP2_DMX_SPS_SECTION_DESC_SIZE		188	/* size of TS packet */
#define TSPP2_DMX_SPS_PCR_DESC_SIZE		195	/* size of PCR packet */
#define TSPP2_DMX_SPS_INDEXING_DESC_SIZE	28	/* index entry size */
#define TSPP2_DMX_MIN_INDEXING_DESC_SIZE	24	/* partial index desc */
#define TSPP2_DMX_SPS_VPES_HEADER_DESC_SIZE	(1 + 2*VPES_HEADER_DATA_SIZE)
#define TSPP2_DMX_SPS_VPES_PAYLOAD_DESC_SIZE	2048	/* Video PES payload */
#define TSPP2_DMX_SPS_NON_VID_PES_DESC_SIZE	256	/* Non-Video PES */

#define TSPP2_DMX_SPS_188_RECORDING_DESC_SIZE	1128
#define TSPP2_DMX_SPS_192_RECORDING_DESC_SIZE	1152

#define TSPP2_DMX_SPS_188_INPUT_BUFF_DESC_SIZE	\
	((SPS_IOVEC_MAX_SIZE / 188) * 188)
#define TSPP2_DMX_SPS_192_INPUT_BUFF_DESC_SIZE	\
	((SPS_IOVEC_MAX_SIZE / 192) * 192)
#define TSPP2_DMX_SPS_TS_INSERTION_DESC_SIZE	9024	/* LCM (188,192) */

/* Max allowed buffer sizes */
#define TSPP2_DMX_SPS_MAX_NUM_OF_DESCRIPTORS	(8*1024 - 2)
#define TSPP2_DMX_SECTION_MAX_BUFF_SIZE		\
	(TSPP2_DMX_SPS_MAX_NUM_OF_DESCRIPTORS *	\
	TSPP2_DMX_SPS_SECTION_DESC_SIZE)
#define TSPP2_DMX_SECTION_BUFFER_THRESHOLD	\
	(((TSPP2_DMX_SECTION_MAX_BUFF_SIZE) * 9) / 10) /* buffer 90% full */

#define TSPP2_DMX_PCR_MAX_BUFF_SIZE		\
	(TSPP2_DMX_SPS_MAX_NUM_OF_DESCRIPTORS *	\
	TSPP2_DMX_SPS_PCR_DESC_SIZE)

#define TSPP2_DMX_VPES_HEADER_MAX_BUFF_SIZE	\
	(TSPP2_DMX_SPS_MAX_NUM_OF_DESCRIPTORS *	\
	TSPP2_DMX_SPS_VPES_HEADER_DESC_SIZE)

#define TSPP2_DMX_VPES_PAYLOAD_MAX_BUFF_SIZE	\
	(TSPP2_DMX_SPS_MAX_NUM_OF_DESCRIPTORS *	\
	TSPP2_DMX_SPS_VPES_PAYLOAD_DESC_SIZE)

#define TSPP2_DMX_SPS_NON_VID_PES_MAX_BUFF_SIZE	\
	(TSPP2_DMX_SPS_MAX_NUM_OF_DESCRIPTORS *	\
	TSPP2_DMX_SPS_NON_VID_PES_DESC_SIZE)

#define TSPP2_DMX_SPS_188_RECORD_MAX_BUFF_SIZE	\
	(TSPP2_DMX_SPS_MAX_NUM_OF_DESCRIPTORS *	\
	TSPP2_DMX_SPS_188_RECORDING_DESC_SIZE)

#define TSPP2_DMX_SPS_192_RECORD_MAX_BUFF_SIZE	\
	(TSPP2_DMX_SPS_MAX_NUM_OF_DESCRIPTORS *	\
	TSPP2_DMX_SPS_192_RECORDING_DESC_SIZE)

#define TSPP2_DMX_SPS_INDEXING_MAX_BUFF_SIZE	\
	(TSPP2_DMX_SPS_MAX_NUM_OF_DESCRIPTORS *	\
	TSPP2_DMX_SPS_INDEXING_DESC_SIZE)

#define TSPP2_DMX_SPS_188_MAX_INPUT_BUFF_SIZE	\
	(TSPP2_DMX_SPS_MAX_NUM_OF_DESCRIPTORS *	\
	TSPP2_DMX_SPS_188_INPUT_BUFF_DESC_SIZE)

#define TSPP2_DMX_SPS_192_MAX_INPUT_BUFF_SIZE	\
	(TSPP2_DMX_SPS_MAX_NUM_OF_DESCRIPTORS *	\
	TSPP2_DMX_SPS_192_INPUT_BUFF_DESC_SIZE)

#define TSPP2_DMX_TS_INSERTION_BUFF_SIZE	\
	(7 * TSPP2_DMX_SPS_TS_INSERTION_DESC_SIZE) /* ~64kb */
/*
 * Size of buffer allocated for section pipe.
 * The size is set to ~1 second of 6Mbps stream
 */
#define TSPP2_DMX_SECTION_PIPE_BUFF_SIZE \
	(TSPP2_DMX_SPS_SECTION_DESC_SIZE * 1024 * 4)

/* Size of buffer allocated for PCR pipe */
#define TSPP2_DMX_PCR_PIPE_BUFF_SIZE \
	(TSPP2_DMX_SPS_PCR_DESC_SIZE * 512)

/* Buffer allocated for TS packets holding PES headers */
#define TSPP2_DMX_VPES_HEADER_PIPE_BUFF_SIZE \
	(TSPP2_DMX_SPS_VPES_HEADER_DESC_SIZE * VIDEO_NUM_OF_PES_PACKETS)

/* Indices in VPES header descriptor for extra-info from TSPP */
#define VPES_HEADER_STC_OFFSET		188
#define VPES_HEADER_SA_OFFSET		195
#define VPES_HEADER_EA_OFFSET		199
#define VPES_HEADER_STATUS_OFFSET	203

#define PES_STC_FIELD_LENGTH		8
#define PES_ASM_STATUS_FIELD_LENGTH	1
#define PES_ASM_STATUS_HOLE_IN_PES	0x01
#define PES_ASM_STATUS_DCI		0x02
#define PES_ASM_STATUS_SIZE_MISMATCH	0x04
#define PES_ASM_STATUS_TX_FAILED	0x08

/*
 * Indexing header pipe should contain no more headers than is possible
 * to report.
 */
#define TSPP2_DMX_INDEX_PIPE_BUFFER_SIZE	\
	(TSPP2_DMX_SPS_INDEXING_DESC_SIZE * DMX_EVENT_QUEUE_SIZE)

#define INDEX_TABLE_PREFIX_LENGTH	3
#define INDEX_TABLE_PREFIX_VALUE	0x00000001
#define INDEX_TABLE_PREFIX_MASK		0x00FFFFFF

#define INDEX_DESC_TABLE_ID_MASK	0x60
#define INDEX_DESC_PATTERN_ID_MASK	0x1F


/**
 * enum mpq_dmx_tspp2_source - TSPP2 source enumeration
 *
 * @TSIF0_SOURCE:	First Demod
 * @TSIF1_SOURCE:	Second Demod
 * @BAMx_SOURCE:	Memory input (x = BAM pipes 0-7)
 *			BAMx are for DVRx devices (x = 0-3)
 *			BAMx are for buffer insertion source (x = 4-7)
 */
enum mpq_dmx_tspp2_source {
	TSIF0_SOURCE = 0,
	TSIF1_SOURCE = 1,
	BAM0_SOURCE = 2,
	BAM1_SOURCE = 3,
	BAM2_SOURCE = 4,
	BAM3_SOURCE = 5,
	BAM4_SOURCE = 6,
	BAM5_SOURCE = 7,
	BAM6_SOURCE = 8,
	BAM7_SOURCE = 9
};

/**
 * enum mpq_dmx_tspp2_source_type - TSPP2 source type enumeration
 *
 * @DEMUXING_SOURCE:		Source with dedicated input from either TSIF or
 *				memory that performs full demuxing on its input.
 * @TS_INSERTION_SOURCE:	Source performing TS packet copy used just
 *				for TS insertion and shared between all demux
 *				instances.
 */
enum mpq_dmx_tspp2_source_type {
	DEMUXING_SOURCE = 0,
	TS_INSERTION_SOURCE = 1,
};

/**
 * enum mpq_dmx_tspp2_pipe_type - pipe type
 *
 * @PCR_PIPE:			Output pipe for PCR
 * @CLEAR_SECTION_PIPE:		Output pipe for all clear section TS packets
 * @SCRAMBLED_SECTION_PIPE:	Output pipe for all scrambled section TS packets
 * @PES_PIPE:			Output pipe for full-PES
 * @VPES_HEADER_PIPE:		Output pipe for separated PES headers
 * @VPES_PAYLOAD_PIPE:		Output pipe for separated PES payload
 * @REC_PIPE:			Output pipe for TS packet recording
 * @INDEXING_PIPE:		Output pipe for indexing
 * @INPUT_PIPE:			Input pipe (TSPP2 consumer pipe)
 */
enum mpq_dmx_tspp2_pipe_type {
	PCR_PIPE = 0,
	CLEAR_SECTION_PIPE = 1,
	SCRAMBLED_SECTION_PIPE = 2,
	PES_PIPE = 3,
	VPES_HEADER_PIPE = 4,
	VPES_PAYLOAD_PIPE = 5,
	REC_PIPE = 6,
	INDEXING_PIPE = 7,
	INPUT_PIPE = 8
};

/**
 * enum mpq_dmx_tspp2_pipe_event - pipe event
 *
 * @PIPE_DATA_EVENT:		Callback notification from TSPP2 due to new data
 * @PIPE_OVERFLOW_EVENT:	Callback notification from TSPP2 due to out
 *				of descriptors.
 * @PIPE_EOS_EVENT:		Callback notification from internal logic reason
 *				is handling of end-of-stream (EOS).
 */
enum mpq_dmx_tspp2_pipe_event {
	PIPE_DATA_EVENT,
	PIPE_OVERFLOW_EVENT,
	PIPE_EOS_EVENT
};

/**
 * struct pipe_work - Work scheduled each time we receive data from a pipe
 *
 * @pipe_info:		Associated pipe
 * @event:		Source event type for this work item
 * @session_id:		pipe_info.session_id cached value at time of pipe
 *			work creation.
 * @event_count:	Number of events included in this work
 * @next:		List node field for pipe_work_queue lists
 */
struct pipe_work {
	struct pipe_info *pipe_info;
	enum mpq_dmx_tspp2_pipe_event event;
	u32 session_id;
	u32 event_count;
	struct list_head next;
};

/**
 * struct pipe_work_queue -
 *
 * @work_pool:	pipe_work objects pool
 * @work_list:	Queue of pipe_work element source thread should process
 * @free_list:	List of free pipe_work objects
 * @lock:	Lock to protect modifications to lists
 */
struct pipe_work_queue {
	struct pipe_work work_pool[TSPP2_DMX_PIPE_WORK_POOL_SIZE];
	struct list_head work_list;
	struct list_head free_list;
	spinlock_t lock;
};

/**
 * struct mpq_dmx_tspp2_filter_op - TSPPv2 filter operation
 *
 * @op:		TSPP2 driver operation (along with parameters)
 * @next:	Filter operations list
 * @ref_count:	Number of feeds using this operation
 */
struct mpq_dmx_tspp2_filter_op {
	struct tspp2_operation op;
	struct list_head next;
	u8 ref_count;
};

/**
 * struct mpq_dmx_tspp2_pipe_buffer - pipe buffer information
 *
 * @size:		Size of the buffer
 * @internal_mem:	Indicates whether memory was allocated internally
 * @kernel_map:		Indicates whether memory was mapped to kernel
 * @handle:		ION buffer handle
 * @mem:		Kernel mapped address of the buffer.
 *			This is available only in case of non-secured buffer.
 * @iova:		The mapped address in TSPP MMU
 */
struct mpq_dmx_tspp2_pipe_buffer {
	u32 size;
	int internal_mem;
	int kernel_map;
	struct ion_handle *handle;
	void *mem;
	u32 iova;
};

/**
 * struct pipe_info - describes information related to TSPP pipe
 *
 * @ref_count:		Number of feeds attached to the pipe
 * @lock:		Spinlock to specifically protect the pipe ref_count
 *			with regard to the timer context.
 * @type:		the data type this pipe is handling
 * @handle:		Pipe handle with the TSPP2 driver
 * @pipe_cfg:		SPS configuration with TSPP2 driver
 * @source_info:	The source this pipe is associated with
 * @session_id:		Monotonically increasing sequence number for "stamping"
 *			the pipe work object to a certain session of a pipe.
 *			When pipe is terminated and then initialized again,
 *			the session id is incremented.
 * @parent:		Back-pointer to the feed using this pipe.
 *			In case the pipe is used by multiple feeds
 *			(such as sections or recording), this points only to
 *			the first feed.
 * @buffer:		Pipe's buffer information
 * @tspp_last_addr:	Last reported output pipe address TSPP2 wrote to
 * @tspp_write_offset:	Last offset in the output buffer TSPP2 wrote data to
 *			as reported by the TSPPv2.
 * @tspp_read_offset:	Offset in the output buffer next data is read from.
 *			This address is advanced whenever user releases data
 *			not required to be read any longer.
 *			tspp_read_offset <= tapp_write_offset
 * @bam_read_offset:	Offset in the output buffer data is read from based on
 *			BAM. When data is polled using TSPP last write address,
 *			the BAM did not necessarily reach the address TSPP
 *			reached in the output buffer (due to partial
 *			descriptors or internal BAM queue), this offset holds
 *			the offset from which data can be read from the BAM.
 *			bam_read_offset <= tspp_read_offset
 * @pipe_handler:	Pipe handler function of notifications from a BAM pipe
 * @mutex:		Mutex for protecting access to pipe info
 * @eos_pending:	Flag specifying whether the pipe handler has an
 *			end of stream notification that should be handled.
 * @work_queue:		pipe_work queue of work pending for this pipe
 * @overflow:		overflow condition for output pipes
 * @hw_notif_count:	Total number of HW notifications
 * @hw_notif_rate_hz:	Rate of HW notifications in unit of Hz
 * @hw_notif_last_time:	Time at which previous HW notification was received
 */
struct pipe_info {
	u32 ref_count;
	spinlock_t lock;
	enum mpq_dmx_tspp2_pipe_type type;
	u32 handle;
	struct tspp2_pipe_config_params pipe_cfg;
	struct source_info *source_info;
	u32 session_id;
	struct mpq_tspp2_feed *parent;
	struct mpq_dmx_tspp2_pipe_buffer buffer;
	u32 tspp_last_addr;
	u32 tspp_write_offset;
	u32 tspp_read_offset;
	u32 bam_read_offset;
	int (*pipe_handler)(struct pipe_info *pipe_info,
		enum mpq_dmx_tspp2_pipe_event event);
	struct mutex mutex;
	int eos_pending;
	struct pipe_work_queue work_queue;
	int overflow;

	/* debug-fs */
	u32 hw_notif_count;
	u32 hw_notif_rate_hz;
	u32 hw_missed_notif;
	u32 handler_count;
	struct timespec hw_notif_last_time;
};

/**
 * struct mpq_dmx_tspp2_filter - TSPPv2 filter info
 *
 * @handle:		TSPP2 driver filter handle
 * @source_info:	Source associated with filter
 * @pid:		Filtered PID (mask set for single PID filtering)
 * @operations_list:	Filter operations list
 * @num_ops:		Number of filter operations
 * @num_cipher_ops:	Number of cipher operations in filter
 * @indexing_enabled:	Whether the indexing operation exists for this filter
 * @pes_analysis_op:	Singular PES analysis operation (can be required
 *			by >1 operations). Valid only when num_pes_ops is not 0.
 * @index_op:		Singular indexing operation. Valid only when
 *			indexing_enabled is set.
 * @dwork:		Delayed work object for filter's scrambling bit monitor
 * @scm_prev_val:	Scrambling bit value of previous sample
 * @scm_count:		Number of consecutive identical scrambling bit samples
 * @scm_started:	Flag stating whether monitor is running
 * @cipher_ops:		Cipher operation objects
 */
struct mpq_dmx_tspp2_filter {
	u32 handle;
	struct source_info *source_info;
	u16 pid;
	struct list_head operations_list;
	u8 num_ops;
	u8 num_cipher_ops;
	u8 indexing_enabled;
	struct mpq_dmx_tspp2_filter_op pes_analysis_op;
	struct mpq_dmx_tspp2_filter_op index_op;
	struct delayed_work dwork;
	u8 scm_prev_val;
	u8 scm_count;
	bool scm_started;
	struct mpq_dmx_tspp2_filter_op cipher_ops[TSPP2_DMX_MAX_CIPHER_OPS];
};

/**
 * struct mpq_tspp2_feed - extended info saved for each demux feed
 *
 * @main_pipe:		Main pipe used to hold feed's data
 * @secondary_pipe:	Secondary pipe, used only for feed with PES separated
 *			into two buffers or in case of recording feeds with
 *			indexing.
 * @mpq_feed:		Back-pointer to parent mpq feed
 * @filter:		TSPP2 filter associated for this feed
 * @ops:		Operations owned exclusively by this feed in the filter.
 *			Singleton operations PES analysis & indexing not
 *			included.
 * @index_table:	For feed with pattern indexing holds the index table id
 *			used for the pattern searching.
 * @op_count:		Number of valid entries in 'ops', that is, number of
 *			operations owned exclusively by this feed in the filter.
 *			Singleton operations PES analysis & indexing not
 *			included.
 * @last_pusi_addr:	Address of the last PUSI TSP in the output pipe.
 *			Initialized to 0, meaning no such packet yet.
 * @last_pattern_addr:	Address of the last matched TSP in the output pipe.
 *			Initialized to 0, meaning no such packet yet.
 * @codec:		Codec type used for indexing
 */
struct mpq_tspp2_feed {
	struct pipe_info *main_pipe;
	struct pipe_info *secondary_pipe;
	struct mpq_feed *mpq_feed;
	struct mpq_dmx_tspp2_filter *filter;
	struct mpq_dmx_tspp2_filter_op ops[TSPP2_DMX_MAX_FEED_OPS];
	u8 index_table;
	u8 op_count;
	u32 last_pusi_addr;
	u32 last_pattern_addr;
	enum dmx_video_codec codec;
};

/**
 * struct mpq_tspp2_demux - TSPPv2 demux info per instance
 *
 * @mpq_demux:		Back-pointer to parent demux instance
 * @source_info:	Source in use by this mpq_tspp2_demux instance
 * @feeds:		mpq_tspp2_feed object pool
 */
struct mpq_tspp2_demux {
	struct mpq_demux *mpq_demux;
	struct source_info *source_info;
	struct mpq_tspp2_feed feeds[MPQ_MAX_DMX_FILES];
};

/**
 * struct mpq_tspp2_index_entry - index entry pattern data
 *
 * @value:	Pattern data bits
 * @mask:	Pattern mask bits
 * @type:	DMX_IDX_* type for converting TSPPv2 index entry to pattern type
 */
struct mpq_tspp2_index_entry {
	u32 value;
	u32 mask;
	u64 type;
};

/**
 * struct mpq_tspp2_index_table - index table
 *
 * @patterns:		Array of patterns for this index table (Prefix)
 * @num_patterns:	Number of valid pattern in the array
 */
struct mpq_tspp2_index_table {
	struct mpq_tspp2_index_entry patterns[TSPP2_NUM_INDEXING_PATTERNS];
	u8 num_patterns;
};

/**
 * struct mpq_tspp2_index_desc - Indexing descriptor
 *
 * @sequence:		8 bytes of stream data that matched the pattern
 * @pusi_tsp_addr:	TSPPv2 address of the first byte of the TS packet with
 *			the PUSI flag set.
 * @matched_tsp_addr:	TSPPv2 address of the last byte of the TS packet in
 *			which the pattern was found. If a pattern spreads
 *			across 2 TS packet, this is of the 2nd TS packet.
 * @pattern_id:		Identifies the pattern that was matched - 3 MSB are the
 *			table id (0-4), and the 5 LSB are the pattern entry
 *			in that table (0-25).
 *			Pattern id is 0x80 if no matched were found in the PES,
 *			and 0xC0 if RAI was set somewhere in the PES.
 * @stc:		STC of the matched TS packet.
 * @last_tsp_addr:	TSPPv2 address of the last byte of the last TS packet
 *			in the PES, or the last TS packet before another
 *			pattern was matched.
 */
struct mpq_tspp2_index_desc {
	u8 sequence[8];
	u32 pusi_tsp_addr;
	u32 matched_tsp_addr;
	u8 pattern_id;
	u8 stc[7];
	u32 last_tsp_addr;
} __packed;

/**
 * struct buffer_insertion_source - TS buffer insertion special source details
 *
 * @filter:	Filter used by the source for the TS buffers output.
 *		This filter is set up with pid=0 & mask=0 so all TS packets
 *		are copied, and a single RAW_TX operation.
 * @raw_op:	Filter RAW_TX operation
 */
struct buffer_insertion_source {
	struct mpq_dmx_tspp2_filter *filter;
	struct mpq_dmx_tspp2_filter_op raw_op;
};

/**
 * struct demuxing_source - demuxing source related resources
 *
 * @thread:			Source processing thread
 * @wait_queue:			Processing thread wait queue
 * @mpq_demux:			Pointer to the demux connected to this source
 * @clear_section_pipe:		Pipe opened to hold clear TS packets of sections
 * @scrambled_section_pipe:	Pipe opened to hold scrambled TS packets of
 *				sections.
 */
struct demuxing_source {
	struct task_struct *thread;
	wait_queue_head_t wait_queue;
	struct mpq_demux *mpq_demux;
	struct pipe_info *clear_section_pipe;
	struct pipe_info *scrambled_section_pipe;
};

/**
 * struct source_info - describes information common to demuxing and TS buffer
 * insertion sources.
 *
 * @type:	Source type
 * @name:	Alias string
 * @ref_count:	Number of pipes attached to this source
 * @handle:	Source handle with the TSPP2 driver
 * @completion:	Used to wait for consumer pipes acknowledge
 * @input_pipe:	Input pipe attached to source, relevant only for memory input
 * @tsp_format: Source TS packet format
 * @enabled:	Indicates whether this source has been enabled or not
 */
struct source_info {
	enum mpq_dmx_tspp2_source_type type;
	char name[TSPP2_DMX_SOURCE_NAME_LENGTH];
	u32 ref_count;
	u32 handle;
	struct completion completion;
	struct pipe_info *input_pipe;
	enum tspp2_packet_format tsp_format;
	int enabled;
	union {
		struct buffer_insertion_source insert_src;
		struct demuxing_source demux_src;
	};
};

#endif /* _MPQ_DMX_TSPPV2_PLUGIN_H */
