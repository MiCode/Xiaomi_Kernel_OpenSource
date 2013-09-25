/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_TSPP2_H_
#define _MSM_TSPP2_H_

#include <linux/ion.h>
#include <mach/sps.h>

#define TSPP2_NUM_DEVICES			1
#define TSPP2_NUM_TSIF_INPUTS			2
#define TSPP2_NUM_PIPES				31
#define TSPP2_NUM_MEM_INPUTS			8
#define TSPP2_NUM_INDEXING_TABLES		4
#define TSPP2_NUM_INDEXING_PATTERNS		26
#define TSPP2_MAX_OPS_PER_FILTER		16

#define TSPP2_INVALID_HANDLE			0
#define TSPP2_UNIQUE_PID_MASK			0x1FFF

/**
 * struct msm_tspp2_platform_data - TSPP2 platform data
 *
 * @tspp2_ahb_clk:		TSPP2 device AHB clock name.
 * @tspp2_core_clk:		TSPP2 device core clock name.
 * @tsif_ref_clk:		TSIF device reference clock name.
 * @hlos_group:			IOMMU HLOS group name.
 * @cpz_group:			IOMMU CPZ group name.
 * @hlos_partition:		IOMMU HLOS partition number.
 * @cpz_partition:		IOMMU CPZ partition number.
 */
struct msm_tspp2_platform_data {
	const char *tspp2_ahb_clk;
	const char *tspp2_core_clk;
	const char *tsif_ref_clk;
	const char *hlos_group;
	const char *cpz_group;
	int hlos_partition;
	int cpz_partition;
};

/**
 * struct tspp2_config - Global configuration
 *
 * @min_pcr_interval:		Minimum time (in msec) between PCR notifications
 *				on the same PID. Setting to 0 means every PCR
 *				is notified. Default value is 50 msec.
 * @pcr_on_discontinuity:	Flag to indicate whether to notify on PCR when
 *				the discontinuity flag is set in the TS packet,
 *				regardless of min_pcr_interval.
 * @stc_byte_offset:		Offset (in bytes) between the 4 byte timestamp
 *				and the 7 byte STC counter.
 *				Valid values are 0 - 3. A value of 0 means the
 *				4 LSBytes of the STC are used in the timestamp,
 *				while a value of 3 means the 4 MSBytes are used.
 */
struct tspp2_config {
	u8 min_pcr_interval;
	int pcr_on_discontinuity;
	u8 stc_byte_offset;
};

/**
 * enum tspp2_src_input - Source input type: TSIF or memory
 *
 * @TSPP2_INPUT_TSIF0: Input from TSIF 0.
 * @TSPP2_INPUT_TSIF1: Input from TSIF 1.
 * @TSPP2_INPUT_MEMORY: Input from memory.
 */
enum tspp2_src_input {
	TSPP2_INPUT_TSIF0 = 0,
	TSPP2_INPUT_TSIF1 = 1,
	TSPP2_INPUT_MEMORY = 2
};

/**
 * enum tspp2_tsif_mode - TSIF mode of operation
 *
 * @TSPP2_TSIF_MODE_LOOPBACK:	Loopback mode, used for debug only.
 * @TSPP2_TSIF_MODE_1:		Mode 1: TSIF works with 3 interface signals.
 * @TSPP2_TSIF_MODE_2:		Mode 2: TSIF works with 4 interface signals.
 */
enum tspp2_tsif_mode {
	TSPP2_TSIF_MODE_LOOPBACK,
	TSPP2_TSIF_MODE_1,
	TSPP2_TSIF_MODE_2,
};

/**
 * enum tspp2_packet_format - Packet size (in bytes) and timestamp format
 *
 * @TSPP2_PACKET_FORMAT_188_RAW:	Packet size is 188 Bytes, no timestamp.
 * @TSPP2_PACKET_FORMAT_192_HEAD:	Packet size is 192 Bytes,
 *					4 Byte timestamp before the data.
 * @TSPP2_PACKET_FORMAT_192_TAIL:	Packet size is 192 Bytes,
 *					4 Byte timestamp after the data.
 */
enum tspp2_packet_format {
	TSPP2_PACKET_FORMAT_188_RAW,
	TSPP2_PACKET_FORMAT_192_HEAD,
	TSPP2_PACKET_FORMAT_192_TAIL
};

/**
 * struct tspp2_tsif_src_params - TSIF source configuration parameters
 *
 * @tsif_mode:		TSIF mode of operation.
 * @clock_inverse:	Invert incoming clock signal.
 * @data_inverse:	Invert incoming data signal.
 * @sync_inverse:	Invert incoming sync signal.
 * @enable_inverse:	Invert incoming enable signal.
 */
struct tspp2_tsif_src_params {
	enum tspp2_tsif_mode tsif_mode;
	int clock_inverse;
	int data_inverse;
	int sync_inverse;
	int enable_inverse;
};

/**
 * struct tspp2_src_cfg - Source configuration
 *
 * @input:		Source input (TSIF0/1 or memory).
 * @params:	source configuration parameters.
 */
struct tspp2_src_cfg {
	enum tspp2_src_input input;
	union {
		struct tspp2_tsif_src_params tsif_params;
	} params;
};

/**
 * enum tspp2_src_parsing_option - Parsing options
 *
 * @TSPP2_SRC_PARSING_OPT_CHECK_CONTINUITY:	Detect discontinuities in
 *						TS packets.
 * @TSPP2_SRC_PARSING_OPT_IGNORE_DISCONTINUITY:	Ignore discontinuity indicator.
 *						If set, discontinuities are
 *						detected according to the
 *						continuity counter only,
 *						which may result in false
 *						positives.
 * @TSPP2_SRC_PARSING_OPT_ASSUME_DUPLICATE_PACKETS:	Assume TS packets
 *						may be duplicated (i.e., have
 *						the same continuity counter).
 *						If cleared, may result in more
 *						discontinuity statuses. If TSPP2
 *						is configured using PID filter
 *						masks it is recommended to
 *						disable this field. Multi PID
 *						filters may detect false
 *						duplicates which will results in
 *						discarded packets.
 * @TSPP2_SRC_PARSING_OPT_DISCARD_INVALID_AF_PACKETS:	Discard TS packets with
 *						invalid Adaptation Field
 *						control.
 * @TSPP2_SRC_PARSING_OPT_VERIFY_PES_START:	Verify PES start code. If
 *						enabled and the PES doesn’t
 *						start with the start code, the
 *						whole PES is not assembled.
 */
enum tspp2_src_parsing_option {
	TSPP2_SRC_PARSING_OPT_CHECK_CONTINUITY,
	TSPP2_SRC_PARSING_OPT_IGNORE_DISCONTINUITY,
	TSPP2_SRC_PARSING_OPT_ASSUME_DUPLICATE_PACKETS,
	TSPP2_SRC_PARSING_OPT_DISCARD_INVALID_AF_PACKETS,
	TSPP2_SRC_PARSING_OPT_VERIFY_PES_START
};

/**
 * enum tspp2_src_scrambling_ctrl - Scrambling bits control
 *
 * @TSPP2_SRC_SCRAMBLING_CTRL_PASSTHROUGH:	Packet is clear, pass-through
 *						without decryption.
 * @TSPP2_SRC_SCRAMBLING_CTRL_DISCARD:		Discard packet.
 * @TSPP2_SRC_SCRAMBLING_CTRL_EVEN:		Packet is scrambled with
 *						even Key.
 * @TSPP2_SRC_SCRAMBLING_CTRL_ODD:		Packet is scrambled with
 *						odd key.
 */
enum tspp2_src_scrambling_ctrl {
	TSPP2_SRC_SCRAMBLING_CTRL_PASSTHROUGH,
	TSPP2_SRC_SCRAMBLING_CTRL_DISCARD,
	TSPP2_SRC_SCRAMBLING_CTRL_EVEN,
	TSPP2_SRC_SCRAMBLING_CTRL_ODD
};

/**
 * enum tspp2_src_scrambling_monitoring - Scrambling bits monitoring
 *
 * @TSPP2_SRC_SCRAMBLING_MONITOR_NONE:		No scrambling bits monitoring.
 * @TSPP2_SRC_SCRAMBLING_MONITOR_PES_ONLY:	Monitor only PES heaer
 *						scrambling bit control field.
 *						If no PES header was found,
 *						scrambling bits will be
 *						considered as ‘00’.
 * @TSPP2_SRC_SCRAMBLING_MONITOR_TS_ONLY:	Monitor only TS packet header
 *						scrambling bit control field.
 * @TSPP2_SRC_SCRAMBLING_MONITOR_PES_AND_TS:	Monitor both TS packet and PES
 *						header. Monitor result is the
 *						logical OR of the two.
 */
enum tspp2_src_scrambling_monitoring {
	TSPP2_SRC_SCRAMBLING_MONITOR_NONE,
	TSPP2_SRC_SCRAMBLING_MONITOR_PES_ONLY,
	TSPP2_SRC_SCRAMBLING_MONITOR_TS_ONLY,
	TSPP2_SRC_SCRAMBLING_MONITOR_PES_AND_TS
};

/**
 * struct tspp2_src_scrambling_config - Source scrambling bits configuration
 *
 *					Each TS/PES packet has two bits that
 *					control the scrambling, called
 *					transport_scrambling_control. This
 *					configuration sets the user-defined
 *					meaning of these bits.
 *
 * @scrambling_0_ctrl:			Scrambling bits control for value '00'.
 * @scrambling_1_ctrl:			Scrambling bits control for value '01'.
 * @scrambling_2_ctrl:			Scrambling bits control for value '10'.
 * @scrambling_3_ctrl:			Scrambling bits control for value '11'.
 * @scrambling_bits_monitoring:		Scrambling bits monitoring configuration
 *					for this source.
 */
struct tspp2_src_scrambling_config {
	enum tspp2_src_scrambling_ctrl scrambling_0_ctrl;
	enum tspp2_src_scrambling_ctrl scrambling_1_ctrl;
	enum tspp2_src_scrambling_ctrl scrambling_2_ctrl;
	enum tspp2_src_scrambling_ctrl scrambling_3_ctrl;
	enum tspp2_src_scrambling_monitoring scrambling_bits_monitoring;
};

/**
 * enum tspp2_src_pipe_mode - pipe mode
 *
 * @TSPP2_SRC_PIPE_INPUT: An input (consumer) pipe.
 * @TSPP2_SRC_PIPE_OUTPUT: An output (producer) pipe.
 */
enum tspp2_src_pipe_mode {
	TSPP2_SRC_PIPE_INPUT,
	TSPP2_SRC_PIPE_OUTPUT
};

/**
 * struct tspp2_pipe_pull_mode_params - Pipe pull mode parameters
 *
 * @is_stalling:	Whether this pipe is stalling when working in pull mode.
 *			Relevant for the source the pipe is being attached to.
 *			Relevant only for output pipes.
 * @threshold:		The threshold used for flow control (in bytes). The
 *			same threshold must be used for all sources.
 */
struct tspp2_pipe_pull_mode_params {
	int is_stalling;
	u16 threshold;
};

/**
 * struct tspp2_pipe_sps_params - Pipe SPS configuration parameters
 *
 * @descriptor_size:	Size of each pipe descriptor, in bytes.
 * @descriptor_flags:	Descriptor flags (SPS_IOVEC_FLAG_XXX).
 * @setting:		Pipe settings.
 * @wakeup_events:	Pipe wakeup events.
 * @callback:		A callback function invoked on pipe events.
 * @user_info:		User information reported with each event.
 */
struct tspp2_pipe_sps_params {
	u32 descriptor_size;
	u32 descriptor_flags;
	enum sps_option setting;
	enum sps_option wakeup_events;
	void (*callback)(struct sps_event_notify *notify);
	void *user_info;
};

/**
 * struct tspp2_pipe_config_params - Pipe configuration parameters
 *
 * @ion_client:		The ION client used to allocate the buffer.
 * @buffer_handle:	The ION handle representing the buffer.
 * @buffer_size:	The memory buffer size.
 * @is_secure:		Is this a securely allocated and locked buffer or not.
 * @pipe_mode:		Pipe mode (input / output).
 * @sps_cfg:		Pipe SPS configuration.
 */
struct tspp2_pipe_config_params {
	struct ion_client *ion_client;
	struct ion_handle *buffer_handle;
	u32 buffer_size;
	int is_secure;
	enum tspp2_src_pipe_mode pipe_mode;
	struct tspp2_pipe_sps_params sps_cfg;
};

/**
 * enum tspp2_operation_type - Operation types
 *
 * @TSPP2_OP_PES_ANALYSIS:	PES analysis operation parses the PES header
 *				and extracts necessary variables from it. Use
 *				this operation before any other PES transmit
 *				operation, otherwise the output buffer will be
 *				empty.
 *				PID filters that includes any PES operation, and
 *				specifically PES analysis, must have a mask that
 *				will match a single PID.
 * @TSPP2_OP_RAW_TRANSMIT:	RAW transmit operation, used to send whole TS
 *				packets to the output pipe. A timestamp can also
 *				be appended.
 *				Use this operation for PIDs that carry section
 *				information, and for recording.
 * @TSPP2_OP_PES_TRANSMIT:	PES transmit operation, used to assemble PES
 *				packets. There are two modes for this operation:
 *				1. Full PES transmit: the full PES, both header
 *				and payload are written to the output pipe.
 *				2. Separated PES header and payload: the PES
 *				header is written to one pipe and the payload is
 *				written to a different pipe.
 * @TSPP2_OP_PCR_EXTRACTION:	PCR extraction operation, used to extract TS
 *				packets with PCR. No processing is performed on
 *				the PCR. This operation simply outputs the
 *				packet and the timestamp. It is recommended that
 *				PCR extraction operation will be set before
 *				other operations (PES analysis, cipher) so that
 *				failures in those operation will not affect it.
 * @TSPP2_OP_CIPHER:		Cipher operation, used to encrypt / decrypt TS
 *				packets.
 * @TSPP2_OP_INDEXING:		Indexing operation, used for searching patterns
 *				in the PES payload or for indexing using the
 *				random access indicator.
 *				In order to perform pattern search, a PES
 *				Analysis operation must precede the indexing
 *				operation. For random access indicator indexing
 *				this is not mandatory.
 *				Indexing is performed on video streams.
 *				Use only a single indexing operation per filter.
 * @TSPP2_OP_COPY_PACKET:	Copy packet operation, used to copy the packet
 *				from one sketch buffer to the other.
 */
enum tspp2_operation_type {
	TSPP2_OP_PES_ANALYSIS,
	TSPP2_OP_RAW_TRANSMIT,
	TSPP2_OP_PES_TRANSMIT,
	TSPP2_OP_PCR_EXTRACTION,
	TSPP2_OP_CIPHER,
	TSPP2_OP_INDEXING,
	TSPP2_OP_COPY_PACKET
};

/**
 * enum tspp2_operation_buffer - Operation sketch buffer
 *
 * @TSPP2_OP_BUFFER_A: Sketch buffer A (initial)
 * @TSPP2_OP_BUFFER_B: Sketch buffer B
 */
enum tspp2_operation_buffer {
	TSPP2_OP_BUFFER_A,
	TSPP2_OP_BUFFER_B
};

/**
 * enum tspp2_operation_timestamp_mode - RAW transmit operation timestamp mode
 *
 * @TSPP2_OP_TIMESTAMP_NONE:	Don't add timestamp. Output is 188 byte packets.
 * @TSPP2_OP_TIMESTAMP_ZERO:	Add 4-byte timestamp, value is all zeros.
 * @TSPP2_OP_TIMESTAMP_STC:	Add 4-byte timestamp, value according to STC
 *				generated by TSIF.
 */
enum tspp2_operation_timestamp_mode {
	TSPP2_OP_TIMESTAMP_NONE,
	TSPP2_OP_TIMESTAMP_ZERO,
	TSPP2_OP_TIMESTAMP_STC
};

/**
 * enum tspp2_operation_cipher_mode - Cipher operation mode
 *
 * @TSPP2_OP_CIPHER_DECRYPT:	Decrypt packet.
 * @TSPP2_OP_CIPHER_ENCRYPT:	Encrypt packet.
 */
enum tspp2_operation_cipher_mode {
	TSPP2_OP_CIPHER_DECRYPT,
	TSPP2_OP_CIPHER_ENCRYPT
};

/**
 * enum tspp2_operation_cipher_scrambling_mode - Cipher operation scrambling mode
 *
 * @TSPP2_OP_CIPHER_AS_IS:		Use the original scrambling bits to
 *					decide which key to use (even, odd or
 *					pass-through). If enabled, the operation
 *					will not modify the scrambling bits in
 *					the TS packet header.
 * @TSPP2_OP_CIPHER_SET_SCRAMBLING_0:	Set TS packet scrambling bits to '00'.
 * @TSPP2_OP_CIPHER_SET_SCRAMBLING_1:	Set TS packet scrambling bits to '01'.
 * @TSPP2_OP_CIPHER_SET_SCRAMBLING_2:	Set TS packet scrambling bits to '10'.
 * @TSPP2_OP_CIPHER_SET_SCRAMBLING_3:	Set TS packet scrambling bits to '11'.
 */
enum tspp2_operation_cipher_scrambling_mode {
	TSPP2_OP_CIPHER_AS_IS,
	TSPP2_OP_CIPHER_SET_SCRAMBLING_0,
	TSPP2_OP_CIPHER_SET_SCRAMBLING_1,
	TSPP2_OP_CIPHER_SET_SCRAMBLING_2,
	TSPP2_OP_CIPHER_SET_SCRAMBLING_3
};

/**
 * enum tspp2_op_pes_transmit_mode - PES transmit operation mode
 *
 * TSPP2_OP_PES_TRANSMIT_SEPARATED:	Separated PES mode.
 * TSPP2_OP_PES_TRANSMIT_FULL:		Full PES mode.
 */
enum tspp2_op_pes_transmit_mode {
	TSPP2_OP_PES_TRANSMIT_SEPARATED,
	TSPP2_OP_PES_TRANSMIT_FULL
};

/**
 * struct tspp2_op_pes_analysis_params - PES analysis operation parameters
 *
 * @input:				Input buffer for this operation.
 * @skip_ts_errs:	If set, TS packet with an error
 *					indication will not be processed by this
 *					operation. The implication would be that
 *					if a PES started with an erred packet,
 *					the entire PES will be lost. This
 *					parameter affects all PES operations
 *					later in the sequence: PES transmit and
 *					indexing. For indexing, the implications
 *					might be that a pattern will not be
 *					found and the frame will not be indexed.
 */
struct tspp2_op_pes_analysis_params {
	enum tspp2_operation_buffer input;
	int skip_ts_errs;
};

/**
 * struct tspp2_op_raw_transmit_params - Raw transmit operation parameters
 *
 * @input:				Input buffer for this operation.
 * @timestamp_mode:			Determines timestamp mode.
 * @timestamp_position:			Determines timestamp position, if added.
 * @support_indexing:			If set, then the indexing information
 *					generated by a following indexing
 *					operation refers to the data in the
 *					output pipe used by this RAW operation.
 *					When a filter has multiple RAW
 *					operations, only one of them should set
 *					the support_indexing option.
 * @skip_ts_errs:			If set, TS packet with an error
 *					indication will not be processed by this
 *					operation. The implication depends on
 *					the content which the PID carries.
 * @output_pipe_handle:			Handle of the output pipe.
 */
struct tspp2_op_raw_transmit_params {
	enum tspp2_operation_buffer input;
	enum tspp2_operation_timestamp_mode timestamp_mode;
	enum tspp2_packet_format timestamp_position;
	int support_indexing;
	int skip_ts_errs;
	u32 output_pipe_handle;
};

/**
 * struct tspp2_op_pes_transmit_params - PES transmit operation parameters
 *
 * @input:				Input buffer for this operation.
 * @mode:				Seperated / Full PES mode.
 * @enable_sw_indexing:			Enable the PES addressing which is
 *					appended to the PES payload.
 * @attach_stc_flags:			Attach STC and flags to the PES.
 *					Relevant only when mode is full PES.
 * @disable_tx_on_pes_discontinuity:	Disable transmission of PES payload
 *					after a discontinuity. When set, TSPP2
 *					waits until a new PUSI is received and
 *					all the packets until then are
 *					discarded. The current PES will be
 *					closed when the	new PUSI arrives.
 * @output_pipe_handle:			Handle of the output pipe in full PES
 *					mode, or the payload output pipe in
 *					separated PES mode.
 * @header_output_pipe_handle:		Handle of the PES header output pipe in
 *					separated PES mode.
 */
struct tspp2_op_pes_transmit_params {
	enum tspp2_operation_buffer input;
	enum tspp2_op_pes_transmit_mode mode;
	int enable_sw_indexing;
	int attach_stc_flags;
	int disable_tx_on_pes_discontinuity;
	u32 output_pipe_handle;
	u32 header_output_pipe_handle;
};

/**
 * struct tspp2_op_pcr_extraction_params - PCR extraction operation parameters
 *
 * @input:				Input buffer for this operation.
 * @skip_ts_errs:			If set, TS packet with an error
 *					indication will not be processed by this
 *					operation. The implication would be that
 *					a PCR packet may be lost.
 * @extract_pcr:			Extract TS packets containing PCR.
 * @extract_opcr:			Extract TS packets containing OPCR.
 * @extract_splicing_point:		Extract TS packets containing a splicing
 *					point indication.
 * @extract_transport_private_data:	Extract TS packets containig private
 *					data.
 * @extract_af_extension:		Extract TS packets with an adaptation
 *					field extension.
 * @extract_all_af:			Extract all TS packets with an adaptaion
 *					field.
 * @output_pipe_handle:			Handle of the output pipe.
 */
struct tspp2_op_pcr_extraction_params {
	enum tspp2_operation_buffer input;
	int skip_ts_errs;
	int extract_pcr;
	int extract_opcr;
	int extract_splicing_point;
	int extract_transport_private_data;
	int extract_af_extension;
	int extract_all_af;
	u32 output_pipe_handle;
};

/**
 * struct tspp2_op_cipher_params - Cipher operation parameters
 *
 * @input:				Input buffer for this operation.
 * @mode:				Decrypt / encrypt.
 * @decrypt_pes_header:			Decrypt PES header TS packets (use if
 *					PES header is encrypted).
 * @skip_ts_errs:			If set, TS packet with an error
 *					indication will not be processed by this
 *					operation.
 * @key_ladder_index:			Key ladder index.
 * @scrambling_mode:			Scrambling bits manipulation mode.
 * @output:				Output buffer for this operation.
 */
struct tspp2_op_cipher_params {
	enum tspp2_operation_buffer input;
	enum tspp2_operation_cipher_mode mode;
	int decrypt_pes_header;
	int skip_ts_errs;
	u32 key_ladder_index;
	enum tspp2_operation_cipher_scrambling_mode scrambling_mode;
	enum tspp2_operation_buffer output;
};

/**
 * struct tspp2_op_indexing_params - Indexing operation parameters
 *
 * @input:				Input buffer for this operation.
 * @random_access_indicator_indexing:	If set, do indexing according to the
 *					random access indicator in the TS packet
 *					header. TSPP2 will not look for the
 *					patterns defined in the specified table.
 * @indexing_table_id:			Indexing table ID.
 * @skip_ts_errs:			If set, TS packet with an error
 *					indication will not be processed by this
 *					operation.
 * @output_pipe_handle:			Handle of the output pipe.
 */
struct tspp2_op_indexing_params {
	enum tspp2_operation_buffer input;
	int random_access_indicator_indexing;
	u8 indexing_table_id;
	int skip_ts_errs;
	u32 output_pipe_handle;
};

/**
 * struct tspp2_op_copy_packet_params - Copy packet operation parameters
 *
 * @input:				Input buffer for this operation.
 */
struct tspp2_op_copy_packet_params {
	enum tspp2_operation_buffer input;
};

/**
 * struct tspp2_operation - Operation
 *
 * @type:	Operation type.
 * @params:	Operation-specific parameters.
 */
struct tspp2_operation {
	enum tspp2_operation_type type;
	union {
		struct tspp2_op_pes_analysis_params pes_analysis;
		struct tspp2_op_raw_transmit_params raw_transmit;
		struct tspp2_op_pes_transmit_params pes_transmit;
		struct tspp2_op_pcr_extraction_params pcr_extraction;
		struct tspp2_op_cipher_params cipher;
		struct tspp2_op_indexing_params indexing;
		struct tspp2_op_copy_packet_params copy_packet;
	} params;
};

/* Global configuration API */
int tspp2_config_set(u32 dev_id, const struct tspp2_config *cfg);

int tspp2_config_get(u32 dev_id, struct tspp2_config *cfg);

/* Indexing tables API functions */
int tspp2_indexing_prefix_set(u32 dev_id,
				u8 table_id,
				u32 value,
				u32 mask);

int tspp2_indexing_patterns_add(u32 dev_id,
				u8 table_id,
				const u32 *values,
				const u32 *masks,
				u8 patterns_num);

int tspp2_indexing_patterns_clear(u32 dev_id,
				u8 table_id);

/* Pipe API functions */
int tspp2_pipe_open(u32 dev_id,
			const struct tspp2_pipe_config_params *cfg,
			ion_phys_addr_t *iova,
			u32 *pipe_handle);

int tspp2_pipe_close(u32 pipe_handle);

/* Source API functions */
int tspp2_src_open(u32 dev_id,
			struct tspp2_src_cfg *cfg,
			u32 *src_handle);

int tspp2_src_close(u32 src_handle);

int tspp2_src_parsing_option_set(u32 src_handle,
			enum tspp2_src_parsing_option option,
			int value);

int tspp2_src_parsing_option_get(u32 src_handle,
			enum tspp2_src_parsing_option option,
			int *value);

int tspp2_src_sync_byte_config_set(u32 src_handle,
			int check_sync_byte,
			u8 sync_byte_value);

int tspp2_src_sync_byte_config_get(u32 src_handle,
			int *check_sync_byte,
			u8 *sync_byte_value);

int tspp2_src_scrambling_config_set(u32 src_handle,
			const struct tspp2_src_scrambling_config *cfg);

int tspp2_src_scrambling_config_get(u32 src_handle,
			struct tspp2_src_scrambling_config *cfg);

int tspp2_src_packet_format_set(u32 src_handle,
			enum tspp2_packet_format format);

int tspp2_src_pipe_attach(u32 src_handle,
			u32 pipe_handle,
			const struct tspp2_pipe_pull_mode_params *cfg);

int tspp2_src_pipe_detach(u32 src_handle, u32 pipe_handle);

int tspp2_src_enable(u32 src_handle);

int tspp2_src_disable(u32 src_handle);

int tspp2_src_filters_clear(u32 src_handle);

/* Filters and Operations API functions */
int tspp2_filter_open(u32 src_handle, u16 pid, u16 mask, u32 *filter_handle);

int tspp2_filter_close(u32 filter_handle);

int tspp2_filter_enable(u32 filter_handle);

int tspp2_filter_disable(u32 filter_handle);

int tspp2_filter_operations_set(u32 filter_handle,
			const struct tspp2_operation *ops,
			u8 operations_num);

int tspp2_filter_operations_clear(u32 filter_handle);

int tspp2_filter_current_scrambling_bits_get(u32 filter_handle,
			u8 *scrambling_bits_value);

/* Data-path API functions */
int tspp2_pipe_descriptor_get(u32 pipe_handle, struct sps_iovec *desc);

int tspp2_pipe_descriptor_put(u32 pipe_handle, u32 addr, u32 size, u32 flags);

int tspp2_pipe_last_address_used_get(u32 pipe_handle, u32 *address);

int tspp2_data_write(u32 src_handle, u32 offset, u32 size);

int tspp2_tsif_data_write(u32 src_handle, u32 *data);

/* Event notification API functions */

/* Global events */
#define TSPP2_GLOBAL_EVENT_INVALID_AF_CTRL	0x00000001
#define TSPP2_GLOBAL_EVENT_INVALID_AF_LENGTH	0x00000002
#define TSPP2_GLOBAL_EVENT_PES_NO_SYNC		0x00000004
#define TSPP2_GLOBAL_EVENT_TX_FAIL		0x00000008
/* Source events */
#define TSPP2_SRC_EVENT_TSIF_LOST_SYNC		0x00000001
#define TSPP2_SRC_EVENT_TSIF_TIMEOUT		0x00000002
#define TSPP2_SRC_EVENT_TSIF_OVERFLOW		0x00000004
#define TSPP2_SRC_EVENT_TSIF_PKT_READ_ERROR	0x00000008
#define TSPP2_SRC_EVENT_TSIF_PKT_WRITE_ERROR	0x00000010
#define TSPP2_SRC_EVENT_MEMORY_READ_ERROR	0x00000020
#define TSPP2_SRC_EVENT_FLOW_CTRL_STALL		0x00000040
/* Filter events */
#define TSPP2_FILTER_EVENT_SCRAMBLING_HIGH	0x00000001
#define TSPP2_FILTER_EVENT_SCRAMBLING_LOW	0x00000002


int tspp2_global_event_notification_register(u32 dev_id,
			u32 global_event_bitmask,
			void (*callback)(void *cookie, u32 event_bitmask),
			void *cookie);

int tspp2_src_event_notification_register(u32 src_handle,
			u32 src_event_bitmask,
			void (*callback)(void *cookie, u32 event_bitmask),
			void *cookie);

int tspp2_filter_event_notification_register(u32 filter_handle,
			u32 filter_event_bitmask,
			void (*callback)(void *cookie, u32 event_bitmask),
			void *cookie);

#endif /* _MSM_TSPP2_H_ */

