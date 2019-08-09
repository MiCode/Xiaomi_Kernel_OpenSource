/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _MPQ_SDMX_H
#define _MPQ_SDMX_H

#include <linux/types.h>

/* Constant declarations */
#define SDMX_MAX_SESSIONS  (4)
#define SDMX_LOOPBACK_PID  (0x2000)

#define SDMX_MAX_PHYSICAL_CHUNKS (256)

/* Filter-level error indicators */
#define SDMX_FILTER_SUCCESS                       (0)
#define SDMX_FILTER_ERR_MD_BUF_FULL               BIT(0)
#define SDMX_FILTER_ERR_D_BUF_FULL                BIT(1)
#define SDMX_FILTER_ERR_D_LIN_BUFS_FULL           BIT(2)
#define SDMX_FILTER_ERR_INVALID_SCRAMBLE_BITS     BIT(3)
#define SDMX_FILTER_ERR_KL_IND_NOT_SET            BIT(4)
#define SDMX_FILTER_ERR_CAS_DECRYPT_ERROR         BIT(5)
#define SDMX_FILTER_ERR_SEC_VERIF_CRC32_FAIL      BIT(6)
#define SDMX_FILTER_ERR_SEC_INTERNAL_MALLOC_FAIL  BIT(7)
#define SDMX_FILTER_ERR_SEC_LEN_INVALID           BIT(8)
#define SDMX_FILTER_ERR_SEC_PUSI_PTR_INVALID      BIT(9)
#define SDMX_FILTER_ERR_TS_SYNC_BYTE_INVALID      BIT(10)
#define SDMX_FILTER_ERR_TS_TRANSPORT_ERR          BIT(11)
#define SDMX_FILTER_ERR_CONT_CNT_INVALID          BIT(12)
#define SDMX_FILTER_ERR_CONT_CNT_DUPLICATE        BIT(13)
#define SDMX_FILTER_ERR_INVALID_PES_HDR           BIT(14)
#define SDMX_FILTER_ERR_INVALID_PES_LEN           BIT(15)
#define SDMX_FILTER_ERR_INVALID_PES_ENCRYPTION    BIT(16)
#define SDMX_FILTER_ERR_SECURITY_FAULT            BIT(17)
#define SDMX_FILTER_ERR_IN_NS_BUFFER              BIT(18)

/* Filter-level status indicators */
#define SDMX_FILTER_STATUS_EOS                    BIT(0)
#define SDMX_FILTER_STATUS_WR_PTR_CHANGED         BIT(1)

/* Filter-level flags */
#define SDMX_FILTER_FLAG_VERIFY_SECTION_CRC	BIT(0)

#define SDMX_INVALID_SESSION_HANDLE		(-1)
#define SDMX_INVALID_FILTER_HANDLE		(-1)

/* Input flags */
#define SDMX_INPUT_FLAG_EOS		BIT(0)
#define SDMX_INPUT_FLAG_DBG_ENABLE	BIT(1)


enum sdmx_buf_mode {
	SDMX_RING_BUF,
	SDMX_LINEAR_GROUP_BUF,
};

enum sdmx_proc_mode {
	SDMX_PUSH_MODE,
	SDMX_PULL_MODE,
};

enum sdmx_inp_mode {
	SDMX_PKT_ENC_MODE,
	SDMX_BULK_ENC_MODE,
	SDMX_CLEAR_MODE,
};

enum sdmx_pkt_format {
	SDMX_188_BYTE_PKT = 188,
	SDMX_192_BYTE_PKT = 192,
	SDMX_195_BYTE_PKT = 195,
};

enum sdmx_log_level {
	SDMX_LOG_NO_PRINT,
	SDMX_LOG_MSG_ERROR,
	SDMX_LOG_DEBUG,
	SDMX_LOG_VERBOSE
};

enum sdmx_status {
	SDMX_SUCCESS = 0,
	SDMX_STATUS_GENERAL_FAILURE = -1,
	SDMX_STATUS_MAX_OPEN_SESSIONS_REACHED = -2,
	SDMX_STATUS_INVALID_SESSION_HANDLE = -3,
	SDMX_STATUS_INVALID_INPUT_PARAMS = -4,
	SDMX_STATUS_UNSUPPORTED_MODE = -5,
	SDMX_STATUS_INVALID_PID = -6,
	SDMX_STATUS_OUT_OF_MEM = -7,
	SDMX_STATUS_FILTER_EXISTS = -8,
	SDMX_STATUS_INVALID_FILTER_HANDLE = -9,
	SDMX_STATUS_MAX_RAW_PIDS_REACHED = -10,
	SDMX_STATUS_SINGLE_PID_RAW_FILTER = -11,
	SDMX_STATUS_INP_BUF_INVALID_PARAMS = -12,
	SDMX_STATUS_INVALID_FILTER_CFG = -13,
	SDMX_STATUS_STALLED_IN_PULL_MODE = -14,
	SDMX_STATUS_SECURITY_FAULT = -15,
	SDMX_STATUS_NS_BUFFER_ERROR = -16,
};

enum sdmx_filter {
	SDMX_PES_FILTER,		/* Other PES */
	SDMX_SEPARATED_PES_FILTER,	/* Separated PES (for decoder) */
	SDMX_SECTION_FILTER,		/* Section */
	SDMX_PCR_FILTER,		/* PCR */
	SDMX_RAW_FILTER,		/* Recording */
};

enum sdmx_raw_out_format {
	SDMX_188_OUTPUT,
	SDMX_192_HEAD_OUTPUT,
	SDMX_192_TAIL_OUTPUT
};

struct sdmx_buff_descriptor {
	void *virt_base;   /* logical address of the actual data */
	phys_addr_t phys_base; /* physical address of the actual data */
	dma_addr_t dma_base; /* DMA address of the actual data */
	u32 size;          /* size of buffer in bytes */
	int id;            /* unique identifier */
	void *user;        /* user-defined data */
};

#pragma pack(push, sdmx, 1)

struct sdmx_session_dbg_counters {
	/* Total number of TS-packets input to SDMX. */
	u32 ts_pkt_in;

	/* Total number of TS-packets filtered out by SDMX. */
	u32 ts_pkt_out;
};

struct sdmx_filter_dbg_counters {
	int filter_handle;

	/* Number of TS-packets filtered. */
	u32 ts_pkt_count;

	/* Number of TS-packets with adaptation field only (no payload). */
	u32 ts_pkt_no_payload;

	/* Number of TS-packets with the discontinuity indicator set. */
	u32 ts_pkt_discont;

	/* Number of duplicate TS-packets detected. */
	u32 ts_pkt_dup;

	/* Number of packets not decrypted because the key wasn't ready. */
	u32 ts_pkt_key_not_ready;
};

struct sdmx_pes_counters {
	/* Number of TS packets with the TEI flag set */
	u32 transport_err_count;

	/* Number of TS packets with continuity counter errors */
	u32 continuity_err_count;

	/* Number of TS packets composing this PES frame */
	u32 pes_ts_count;

	/* Number of TS packets dropped due to full buffer */
	u32 drop_count;
};

struct sdmx_buff_descr {
	/* Physical address where buffer starts */
	u64 base_addr;

	/* Size of buffer */
	u32 size;
};

struct sdmx_data_buff_descr {
	/* Physical chunks of the buffer */
	struct sdmx_buff_descr buff_chunks[SDMX_MAX_PHYSICAL_CHUNKS];

	/* Length of buffer */
	u32 length;
};

/*
 * Data payload residing in the data buffers is described using this meta-data
 * header. The meta data header specifies where the payload is located in the
 * data buffer and how big it is.
 * The meta data header optionally carries additional relevant meta data
 * immediately following the meta-data header.
 */
struct sdmx_metadata_header {
	/*
	 * Payload start offset inside data buffer. In case data is managed
	 * as a linear buffer group, this specifies buffer index.
	 */
	u32 payload_start;

	/* Payload length */
	u32 payload_length;

	/* Number of meta data bytes immediately following this header */
	u32 metadata_length;
};


struct sdmx_filter_status {
	/* Secure demux filter handle */
	int filter_handle;

	/*
	 * Number of pending bytes in filter's output data buffer.
	 * For linear buffer mode, this is number of buffers pending.
	 */
	u32 data_fill_count;

	/*
	 * Offset in data buffer for next data payload to be written.
	 * For linear buffer mode, this is a buffer index.
	 */
	u32 data_write_offset;

	/* Number of pending bytes in filter's output meta data buffer */
	u32 metadata_fill_count;

	/* Offset in meta data buffer for next metadata header to be written */
	u32 metadata_write_offset;

	/* Errors (bitmap) reported by secure demux for this filter */
	u32 error_indicators;

	/* General status (bitmap) reported by secure demux for this filter */
	u32 status_indicators;
};
#pragma pack(pop, sdmx)

#ifdef CONFIG_QSEECOM

int sdmx_open_session(int *session_handle);

int sdmx_close_session(int session_handle);

int sdmx_get_version(int session_handle, int32_t *version);

int sdmx_set_session_cfg(int session_handle, enum sdmx_proc_mode proc_mode,
	enum sdmx_inp_mode inp_mode, enum sdmx_pkt_format pkt_format,
	u8 odd_scramble_bits, u8 even_scramble_bits);

int sdmx_add_filter(int session_handle, u16 pid, enum sdmx_filter filter_type,
	struct sdmx_buff_descr *meta_data_buf, enum sdmx_buf_mode data_buf_mode,
	u32 num_data_bufs, struct sdmx_data_buff_descr *data_bufs,
	int *filter_handle, enum sdmx_raw_out_format ts_out_format, u32 flags);

int sdmx_remove_filter(int session_handle, int filter_handle);

int sdmx_set_kl_ind(int session_handle, u16 pid, u32 key_ladder_index);

int sdmx_add_raw_pid(int session_handle, int filter_handle, u16 pid);

int sdmx_remove_raw_pid(int session_handle, int filter_handle, u16 pid);

int sdmx_process(int session_handle, u8 flags,
	struct sdmx_buff_descr *input_buf_desc,
	u32 *input_fill_count, u32 *input_read_offset,
	u32 *error_indicators,
	u32 *status_indicators,
	u32 num_filters,
	struct sdmx_filter_status *filter_status);

int sdmx_get_dbg_counters(int session_handle,
	struct sdmx_session_dbg_counters *session_counters,
	u32 *num_filters,
	struct sdmx_filter_dbg_counters *filter_counters);

int sdmx_reset_dbg_counters(int session_handle);

int sdmx_set_log_level(int session_handle, enum sdmx_log_level level);

#else

static inline int sdmx_open_session(int *session_handle)
{
	return SDMX_STATUS_GENERAL_FAILURE;
}

static inline int sdmx_close_session(int session_handle)
{
	return SDMX_STATUS_GENERAL_FAILURE;
}

static inline int sdmx_get_version(int session_handle, int32_t *version)
{
	return SDMX_STATUS_GENERAL_FAILURE;
}

static inline int sdmx_set_session_cfg(int session_handle,
	enum sdmx_proc_mode proc_mode,
	enum sdmx_inp_mode inp_mode, enum sdmx_pkt_format pkt_format,
	u8 odd_scramble_bits, u8 even_scramble_bits)
{
	return SDMX_STATUS_GENERAL_FAILURE;
}

static inline int sdmx_add_filter(int session_handle, u16 pid,
	enum sdmx_filter filter_type,
	struct sdmx_buff_descr *meta_data_buf, enum sdmx_buf_mode data_buf_mode,
	u32 num_data_bufs, struct sdmx_data_buff_descr *data_bufs,
	int *filter_handle, enum sdmx_raw_out_format ts_out_format, u32 flags)
{
	return SDMX_STATUS_GENERAL_FAILURE;
}

static inline int sdmx_remove_filter(int session_handle, int filter_handle)
{
	return SDMX_STATUS_GENERAL_FAILURE;
}

static inline int sdmx_set_kl_ind(int session_handle, u16 pid,
	u32 key_ladder_index)
{
	return SDMX_STATUS_GENERAL_FAILURE;
}

static inline int sdmx_add_raw_pid(int session_handle, int filter_handle,
	u16 pid)
{
	return SDMX_STATUS_GENERAL_FAILURE;
}

static inline int sdmx_remove_raw_pid(int session_handle, int filter_handle,
	u16 pid)
{
	return SDMX_STATUS_GENERAL_FAILURE;
}

static inline int sdmx_process(int session_handle, u8 flags,
	struct sdmx_buff_descr *input_buf_desc,
	u32 *input_fill_count, u32 *input_read_offset,
	u32 *error_indicators,
	u32 *status_indicators,
	u32 num_filters,
	struct sdmx_filter_status *filter_status)
{
	*status_indicators = 0;
	*error_indicators = 0;
	return SDMX_STATUS_GENERAL_FAILURE;
}

static inline int sdmx_get_dbg_counters(int session_handle,
	struct sdmx_session_dbg_counters *session_counters,
	u32 *num_filters,
	struct sdmx_filter_dbg_counters *filter_counters)
{
	return SDMX_STATUS_GENERAL_FAILURE;
}

static inline int sdmx_reset_dbg_counters(int session_handle)
{
	return SDMX_STATUS_GENERAL_FAILURE;
}

static inline int sdmx_set_log_level(int session_handle,
	enum sdmx_log_level level)
{
	return SDMX_STATUS_GENERAL_FAILURE;
}

#endif

#endif /* _MPQ_SDMX_H */
