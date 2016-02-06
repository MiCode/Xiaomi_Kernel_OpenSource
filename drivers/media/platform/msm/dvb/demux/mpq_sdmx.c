/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include "qseecom_kernel.h"
#include "mpq_sdmx.h"

static struct qseecom_handle *sdmx_qseecom_handles[SDMX_MAX_SESSIONS];
static struct mutex sdmx_lock[SDMX_MAX_SESSIONS];

#define QSEECOM_SBUFF_SIZE	SZ_128K

enum sdmx_cmd_id {
	SDMX_OPEN_SESSION_CMD,
	SDMX_CLOSE_SESSION_CMD,
	SDMX_SET_SESSION_CFG_CMD,
	SDMX_ADD_FILTER_CMD,
	SDMX_REMOVE_FILTER_CMD,
	SDMX_SET_KL_IDX_CMD,
	SDMX_ADD_RAW_PID_CMD,
	SDMX_REMOVE_RAW_PID_CMD,
	SDMX_PROCESS_CMD,
	SDMX_GET_DBG_COUNTERS_CMD,
	SDMX_RESET_DBG_COUNTERS_CMD,
	SDMX_GET_VERSION_CMD,
	SDMX_INVALIDATE_KL_CMD,
	SDMX_SET_LOG_LEVEL_CMD
};

#pragma pack(push, sdmx, 1)

struct sdmx_proc_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
	u8 flags;
	struct sdmx_buff_descr in_buf_descr;
	u32 inp_fill_cnt;
	u32 in_rd_offset;
	u32 num_filters;
	struct sdmx_filter_status filters_status[];
};

struct sdmx_proc_rsp {
	enum sdmx_status ret;
	u32 inp_fill_cnt;
	u32 in_rd_offset;
	u32 err_indicators;
	u32 status_indicators;
};

struct sdmx_open_ses_req {
	enum sdmx_cmd_id cmd_id;
};

struct sdmx_open_ses_rsp {
	enum sdmx_status ret;
	u32 session_handle;
};

struct sdmx_close_ses_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
};

struct sdmx_close_ses_rsp {
	enum sdmx_status ret;
};

struct sdmx_ses_cfg_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
	enum sdmx_proc_mode process_mode;
	enum sdmx_inp_mode input_mode;
	enum sdmx_pkt_format packet_len;
	u8 odd_scramble_bits;
	u8 even_scramble_bits;
};

struct sdmx_ses_cfg_rsp {
	enum sdmx_status ret;
};

struct sdmx_set_kl_ind_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
	u32 pid;
	u32 kl_index;
};

struct sdmx_set_kl_ind_rsp {
	enum sdmx_status ret;
};

struct sdmx_add_filt_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
	u32 pid;
	enum sdmx_filter filter_type;
	struct sdmx_buff_descr meta_data_buf;
	enum sdmx_buf_mode buffer_mode;
	enum sdmx_raw_out_format ts_out_format;
	u32 flags;
	u32 num_data_bufs;
	struct sdmx_data_buff_descr data_bufs[];
};

struct sdmx_add_filt_rsp {
	enum sdmx_status ret;
	u32 filter_handle;
};

struct sdmx_rem_filt_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
	u32 filter_handle;
};

struct sdmx_rem_filt_rsp {
	enum sdmx_status ret;
};

struct sdmx_add_raw_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
	u32 filter_handle;
	u32 pid;
};

struct sdmx_add_raw_rsp {
	enum sdmx_status ret;
};

struct sdmx_rem_raw_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
	u32 filter_handle;
	u32 pid;
};

struct sdmx_rem_raw_rsp {
	enum sdmx_status ret;
};

struct sdmx_get_counters_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
	u32 num_filters;
};

struct sdmx_get_counters_rsp {
	enum sdmx_status ret;
	struct sdmx_session_dbg_counters session_counters;
	u32 num_filters;
	struct sdmx_filter_dbg_counters filter_counters[];
};

struct sdmx_rst_counters_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
};

struct sdmx_rst_counters_rsp {
	enum sdmx_status ret;
};

struct sdmx_get_version_req {
	enum sdmx_cmd_id cmd_id;
};

struct sdmx_get_version_rsp {
	enum sdmx_status ret;
	int32_t version;
};

struct sdmx_set_log_level_req {
	enum sdmx_cmd_id cmd_id;
	enum sdmx_log_level level;
	u32 session_handle;
};

struct sdmx_set_log_level_rsp {
	enum sdmx_status ret;
};

#pragma pack(pop, sdmx)

static int get_cmd_rsp_buffers(int handle_index,
	void **cmd,
	int *cmd_len,
	void **rsp,
	int *rsp_len)
{
	if (*cmd_len & QSEECOM_ALIGN_MASK)
		*cmd_len = QSEECOM_ALIGN(*cmd_len);

	if (*rsp_len & QSEECOM_ALIGN_MASK)
		*rsp_len = QSEECOM_ALIGN(*rsp_len);

	if ((*rsp_len + *cmd_len) > QSEECOM_SBUFF_SIZE) {
		pr_err("%s: shared buffer too small to hold cmd=%d and rsp=%d\n",
			__func__, *cmd_len, *rsp_len);
		return SDMX_STATUS_OUT_OF_MEM;
	}

	*cmd = sdmx_qseecom_handles[handle_index]->sbuf;
	*rsp = sdmx_qseecom_handles[handle_index]->sbuf + *cmd_len;
	return SDMX_SUCCESS;
}

/*
 * Returns version of secure-demux app.
 *
 * @session_handle: Returned instance handle. Must not be NULL.
 * Return error code
 */
int sdmx_get_version(int session_handle, int32_t *version)
{
	int res, cmd_len, rsp_len;
	struct sdmx_get_version_req *cmd;
	struct sdmx_get_version_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS) ||
		(version == NULL))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_get_version_req);
	rsp_len = sizeof(struct sdmx_get_version_rsp);

	/* Lock shared memory */
	mutex_lock(&sdmx_lock[session_handle]);

	/* Get command and response buffers */
	ret = get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);
	if (ret)
		goto out;

	/* Populate command struct */
	cmd->cmd_id = SDMX_GET_VERSION_CMD;

	/* Issue QSEECom command */
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	ret = rsp->ret;
	*version = rsp->version;
out:
	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;

}
EXPORT_SYMBOL(sdmx_get_version);

/*
 * Initializes a new secure demux instance and returns a handle of the instance.
 *
 * @session_handle: handle of a secure demux instance to get its version.
 * Return the version if successful or an error code.
 */
int sdmx_open_session(int *session_handle)
{
	int res, cmd_len, rsp_len;
	enum sdmx_status ret, version_ret;
	struct sdmx_open_ses_req *cmd;
	struct sdmx_open_ses_rsp *rsp;
	struct qseecom_handle *qseecom_handle = NULL;
	int32_t version;

	/* Input validation */
	if (session_handle == NULL)
		return SDMX_STATUS_GENERAL_FAILURE;

	/* Start the TZ app */
	res = qseecom_start_app(&qseecom_handle, "securemm",
		QSEECOM_SBUFF_SIZE);

	if (res < 0)
		return SDMX_STATUS_GENERAL_FAILURE;

	cmd_len = sizeof(struct sdmx_open_ses_req);
	rsp_len = sizeof(struct sdmx_open_ses_rsp);

	/* Get command and response buffers */
	cmd = (struct sdmx_open_ses_req *)qseecom_handle->sbuf;

	if (cmd_len & QSEECOM_ALIGN_MASK)
		cmd_len = QSEECOM_ALIGN(cmd_len);

	rsp = (struct sdmx_open_ses_rsp *)qseecom_handle->sbuf + cmd_len;

	if (rsp_len & QSEECOM_ALIGN_MASK)
		rsp_len = QSEECOM_ALIGN(rsp_len);

	/* Will be later overridden by SDMX response */
	*session_handle = SDMX_INVALID_SESSION_HANDLE;

	/* Populate command struct */
	cmd->cmd_id = SDMX_OPEN_SESSION_CMD;

	/* Issue QSEECom command */
	res = qseecom_send_command(qseecom_handle, (void *)cmd, cmd_len,
		(void *)rsp, rsp_len);

	if (res < 0) {
		qseecom_shutdown_app(&qseecom_handle);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	/* Parse response struct */
	*session_handle = rsp->session_handle;

	/* Initialize handle and mutex */
	sdmx_qseecom_handles[*session_handle] = qseecom_handle;
	mutex_init(&sdmx_lock[*session_handle]);
	ret = rsp->ret;

	/* Get and print the app version */
	version_ret = sdmx_get_version(*session_handle, &version);
	if (SDMX_SUCCESS == version_ret)
		pr_info("TZ SDMX version is %x.%x\n", version >> 8,
			version & 0xFF);
	else
		pr_err("Error reading TZ SDMX version\n");

	return ret;
}
EXPORT_SYMBOL(sdmx_open_session);

/*
 * Closes a secure demux instance.
 *
 * @session_handle: handle of a secure demux instance to close.
 * Return error code
 */
int sdmx_close_session(int session_handle)
{
	int res, cmd_len, rsp_len;
	struct sdmx_close_ses_req *cmd;
	struct sdmx_close_ses_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_close_ses_req);
	rsp_len = sizeof(struct sdmx_close_ses_rsp);

	/* Lock shared memory */
	mutex_lock(&sdmx_lock[session_handle]);

	/* Get command and response buffers */
	ret = get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);
	if (ret)
		goto out;

	/* Populate command struct */
	cmd->cmd_id = SDMX_CLOSE_SESSION_CMD;
	cmd->session_handle = session_handle;

	/* Issue QSEECom command */
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	ret = rsp->ret;

	/* Shutdown the TZ app (or at least free the current handle) */
	res = qseecom_shutdown_app(&sdmx_qseecom_handles[session_handle]);
	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	sdmx_qseecom_handles[session_handle] = NULL;
out:
	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_close_session);

/*
 * Configures an open secure demux instance.
 *
 * @session_handle: secure demux instance
 * @proc_mode: Defines secure demux's behavior in case of output
 *             buffer overflow.
 * @inp_mode: Defines the input encryption settings.
 * @pkt_format: TS packet length in input buffer.
 * @odd_scramble_bits: Value of the scramble bits indicating the ODD key.
 * @even_scramble_bits: Value of the scramble bits indicating the EVEN key.
 * Return error code
 */
int sdmx_set_session_cfg(int session_handle,
	enum sdmx_proc_mode proc_mode,
	enum sdmx_inp_mode inp_mode,
	enum sdmx_pkt_format pkt_format,
	u8 odd_scramble_bits,
	u8 even_scramble_bits)
{
	int res, cmd_len, rsp_len;
	struct sdmx_ses_cfg_req *cmd;
	struct sdmx_ses_cfg_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_ses_cfg_req);
	rsp_len = sizeof(struct sdmx_ses_cfg_rsp);

	/* Lock shared memory */
	mutex_lock(&sdmx_lock[session_handle]);

	/* Get command and response buffers */
	ret = get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);
	if (ret)
		goto out;

	/* Populate command struct */
	cmd->cmd_id = SDMX_SET_SESSION_CFG_CMD;
	cmd->session_handle = session_handle;
	cmd->process_mode = proc_mode;
	cmd->input_mode = inp_mode;
	cmd->packet_len = pkt_format;
	cmd->odd_scramble_bits = odd_scramble_bits;
	cmd->even_scramble_bits = even_scramble_bits;

	/* Issue QSEECom command */
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	ret = rsp->ret;
out:
	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_set_session_cfg);

/*
 * Creates a new secure demux filter and returns a filter handle
 *
 * @session_handle: secure demux instance
 * @pid: pid to filter
 * @filter_type: type of filtering
 * @meta_data_buf: meta data buffer descriptor
 * @data_buf_mode: data buffer mode (ring/linear)
 * @num_data_bufs: number of data buffers (use 1 for a ring buffer)
 * @data_bufs: data buffers descriptors array
 * @filter_handle: returned filter handle
 * @ts_out_format: output format for raw filters
 * @flags: optional flags for filter
 *	   (currently only clear section CRC verification is supported)
 *
 * Return error code
 */
int sdmx_add_filter(int session_handle,
	u16 pid,
	enum sdmx_filter filterype,
	struct sdmx_buff_descr *meta_data_buf,
	enum sdmx_buf_mode d_buf_mode,
	u32 num_data_bufs,
	struct sdmx_data_buff_descr *data_bufs,
	int *filter_handle,
	enum sdmx_raw_out_format ts_out_format,
	u32 flags)
{
	int res, cmd_len, rsp_len;
	struct sdmx_add_filt_req *cmd;
	struct sdmx_add_filt_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS) ||
		(filter_handle == NULL))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_add_filt_req)
		+ num_data_bufs * sizeof(struct sdmx_data_buff_descr);
	rsp_len = sizeof(struct sdmx_add_filt_rsp);

	/* Will be later overridden by SDMX response */
	*filter_handle = SDMX_INVALID_FILTER_HANDLE;

	/* Lock shared memory */
	mutex_lock(&sdmx_lock[session_handle]);

	/* Get command and response buffers */
	ret = get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);
	if (ret)
		goto out;

	/* Populate command struct */
	cmd->cmd_id = SDMX_ADD_FILTER_CMD;
	cmd->session_handle = session_handle;
	cmd->pid = (u32)pid;
	cmd->filter_type = filterype;
	cmd->ts_out_format = ts_out_format;
	cmd->flags = flags;
	if (meta_data_buf != NULL)
		memcpy(&(cmd->meta_data_buf), meta_data_buf,
				sizeof(struct sdmx_buff_descr));
	else
		memset(&(cmd->meta_data_buf), 0, sizeof(cmd->meta_data_buf));

	cmd->buffer_mode = d_buf_mode;
	cmd->num_data_bufs = num_data_bufs;
	memcpy(cmd->data_bufs, data_bufs,
			num_data_bufs * sizeof(struct sdmx_data_buff_descr));

	/* Issue QSEECom command */
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	/* Parse response struct */
	*filter_handle = rsp->filter_handle;
	ret = rsp->ret;
out:
	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_add_filter);

/*
 * Removes a secure demux filter
 *
 * @session_handle: secure demux instance
 * @filter_handle: filter handle to remove
 *
 * Return error code
 */
int sdmx_remove_filter(int session_handle, int filter_handle)
{
	int res, cmd_len, rsp_len;
	struct sdmx_rem_filt_req *cmd;
	struct sdmx_rem_filt_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_rem_filt_req);
	rsp_len = sizeof(struct sdmx_rem_filt_rsp);

	/* Lock shared memory */
	mutex_lock(&sdmx_lock[session_handle]);

	/* Get command and response buffers */
	ret = get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);
	if (ret)
		goto out;

	/* Populate command struct */
	cmd->cmd_id = SDMX_REMOVE_FILTER_CMD;
	cmd->session_handle = session_handle;
	cmd->filter_handle = filter_handle;

	/* Issue QSEECom command */
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	ret = rsp->ret;
out:
	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_remove_filter);

/*
 * Associates a key ladder index for the specified pid
 *
 * @session_handle: secure demux instance
 * @pid: pid
 * @key_ladder_index: key ladder index to associate to the pid
 *
 * Return error code
 *
 * Note: if pid already has some key ladder index associated, it will be
 * overridden.
 */
int sdmx_set_kl_ind(int session_handle, u16 pid, u32 key_ladder_index)
{
	int res, cmd_len, rsp_len;
	struct sdmx_set_kl_ind_req *cmd;
	struct sdmx_set_kl_ind_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_set_kl_ind_req);
	rsp_len = sizeof(struct sdmx_set_kl_ind_rsp);

	/* Lock shared memory */
	mutex_lock(&sdmx_lock[session_handle]);

	/* Get command and response buffers */
	ret = get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);
	if (ret)
		goto out;

	/* Populate command struct */
	cmd->cmd_id = SDMX_SET_KL_IDX_CMD;
	cmd->session_handle = session_handle;
	cmd->pid = (u32)pid;
	cmd->kl_index = key_ladder_index;

	/* Issue QSEECom command */
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	ret = rsp->ret;
out:
	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_set_kl_ind);

/*
 * Adds the specified pid to an existing raw (recording) filter
 *
 * @session_handle: secure demux instance
 * @filter_handle: raw filter handle
 * @pid: pid
 *
 * Return error code
 */
int sdmx_add_raw_pid(int session_handle, int filter_handle, u16 pid)
{
	int res, cmd_len, rsp_len;
	struct sdmx_add_raw_req *cmd;
	struct sdmx_add_raw_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_add_raw_req);
	rsp_len = sizeof(struct sdmx_add_raw_rsp);

	/* Lock shared memory */
	mutex_lock(&sdmx_lock[session_handle]);

	/* Get command and response buffers */
	ret = get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);
	if (ret)
		goto out;

	/* Populate command struct */
	cmd->cmd_id = SDMX_ADD_RAW_PID_CMD;
	cmd->session_handle = session_handle;
	cmd->filter_handle = filter_handle;
	cmd->pid = (u32)pid;

	/* Issue QSEECom command */
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	ret = rsp->ret;
out:
	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_add_raw_pid);

/*
 * Removes the specified pid from a raw (recording) filter
 *
 * @session_handle: secure demux instance
 * @filter_handle: raw filter handle
 * @pid: pid
 *
 * Return error code
 */
int sdmx_remove_raw_pid(int session_handle, int filter_handle, u16 pid)
{
	int res, cmd_len, rsp_len;
	struct sdmx_rem_raw_req *cmd;
	struct sdmx_rem_raw_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_rem_raw_req);
	rsp_len = sizeof(struct sdmx_rem_raw_rsp);

	/* Lock shared memory */
	mutex_lock(&sdmx_lock[session_handle]);

	/* Get command and response buffers */
	ret = get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);
	if (ret)
		goto out;

	/* Populate command struct */
	cmd->cmd_id = SDMX_REMOVE_RAW_PID_CMD;
	cmd->session_handle = session_handle;
	cmd->filter_handle = filter_handle;
	cmd->pid = (u32)pid;

	/* Issue QSEECom command */
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	ret = rsp->ret;
out:
	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_remove_raw_pid);

/*
 * Call secure demux to perform processing on the specified input buffer
 *
 * @session_handle: secure demux instance
 * @flags: input flags. Currently only EOS marking is supported.
 * @input_buf_desc: input buffer descriptor
 * @input_fill_count: number of bytes available in input buffer
 * @input_read_offset: offset inside input buffer where data starts
 * @error_indicators: returned general error indicators
 * @status_indicators: returned general status indicators
 * @num_filters: number of filters in filter status array
 * @filter_status: filter status descriptor array
 *
 * Return error code
 */
int sdmx_process(int session_handle, u8 flags,
	struct sdmx_buff_descr *input_buf_desc,
	u32 *input_fill_count,
	u32 *input_read_offset,
	u32 *error_indicators,
	u32 *status_indicators,
	u32 num_filters,
	struct sdmx_filter_status *filter_status)
{
	int res, cmd_len, rsp_len;
	struct sdmx_proc_req *cmd;
	struct sdmx_proc_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS) ||
		(input_buf_desc == NULL) ||
		(input_fill_count == NULL) || (input_read_offset == NULL) ||
		(error_indicators == NULL) || (status_indicators == NULL) ||
		(filter_status == NULL))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_proc_req)
		+ num_filters * sizeof(struct sdmx_filter_status);
	rsp_len = sizeof(struct sdmx_proc_rsp);

	/* Lock shared memory */
	mutex_lock(&sdmx_lock[session_handle]);

	/* Get command and response buffers */
	ret = get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);
	if (ret)
		goto out;

	/* Populate command struct */
	cmd->cmd_id = SDMX_PROCESS_CMD;
	cmd->session_handle = session_handle;
	cmd->flags = flags;
	cmd->in_buf_descr.base_addr = input_buf_desc->base_addr;
	cmd->in_buf_descr.size = input_buf_desc->size;
	cmd->inp_fill_cnt = *input_fill_count;
	cmd->in_rd_offset = *input_read_offset;
	cmd->num_filters = num_filters;
	memcpy(cmd->filters_status, filter_status,
		num_filters * sizeof(struct sdmx_filter_status));

	/* Issue QSEECom command */
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	/* Parse response struct */
	*input_fill_count = rsp->inp_fill_cnt;
	*input_read_offset = rsp->in_rd_offset;
	*error_indicators = rsp->err_indicators;
	*status_indicators = rsp->status_indicators;
	memcpy(filter_status, cmd->filters_status,
		num_filters * sizeof(struct sdmx_filter_status));
	ret = rsp->ret;
out:
	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_process);

/*
 * Returns session-level & filter-level debug counters
 *
 * @session_handle: secure demux instance
 * @session_counters: returned session-level debug counters
 * @num_filters: returned number of filters reported in filter_counters
 * @filter_counters: returned filter-level debug counters array
 *
 * Return error code
 */
int sdmx_get_dbg_counters(int session_handle,
	struct sdmx_session_dbg_counters *session_counters,
	u32 *num_filters,
	struct sdmx_filter_dbg_counters *filter_counters)
{
	int res, cmd_len, rsp_len;
	struct sdmx_get_counters_req *cmd;
	struct sdmx_get_counters_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS) ||
		(session_counters == NULL) || (num_filters == NULL) ||
		(filter_counters == NULL))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_get_counters_req);
	rsp_len = sizeof(struct sdmx_get_counters_rsp)
		+ *num_filters * sizeof(struct sdmx_filter_dbg_counters);

	/* Lock shared memory */
	mutex_lock(&sdmx_lock[session_handle]);

	/* Get command and response buffers */
	ret = get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);
	if (ret)
		goto out;

	/* Populate command struct */
	cmd->cmd_id = SDMX_GET_DBG_COUNTERS_CMD;
	cmd->session_handle = session_handle;
	cmd->num_filters = *num_filters;

	/* Issue QSEECom command */
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	/* Parse response struct */
	*session_counters = rsp->session_counters;
	*num_filters = rsp->num_filters;
	memcpy(filter_counters, rsp->filter_counters,
		*num_filters * sizeof(struct sdmx_filter_dbg_counters));
	ret = rsp->ret;
out:
	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_get_dbg_counters);

/*
 * Reset debug counters
 *
 * @session_handle: secure demux instance
 *
 * Return error code
 */
int sdmx_reset_dbg_counters(int session_handle)
{
	int res, cmd_len, rsp_len;
	struct sdmx_rst_counters_req *cmd;
	struct sdmx_rst_counters_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_rst_counters_req);
	rsp_len = sizeof(struct sdmx_rst_counters_rsp);

	/* Lock shared memory */
	mutex_lock(&sdmx_lock[session_handle]);

	/* Get command and response buffers */
	ret = get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);
	if (ret)
		goto out;

	/* Populate command struct */
	cmd->cmd_id = SDMX_RESET_DBG_COUNTERS_CMD;
	cmd->session_handle = session_handle;

	/* Issue QSEECom command */
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	ret = rsp->ret;
out:
	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_reset_dbg_counters);

/*
 * Set debug log verbosity level
 *
 * @session_handle: secure demux instance
 * @level: requested log level
 *
 * Return error code
 */
int sdmx_set_log_level(int session_handle, enum sdmx_log_level level)
{
	int res, cmd_len, rsp_len;
	struct sdmx_set_log_level_req *cmd;
	struct sdmx_set_log_level_rsp *rsp;
	enum sdmx_status ret;

	cmd_len = sizeof(struct sdmx_set_log_level_req);
	rsp_len = sizeof(struct sdmx_set_log_level_rsp);

	/* Lock shared memory */
	mutex_lock(&sdmx_lock[session_handle]);

	/* Get command and response buffers */
	ret = get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);
	if (ret)
		goto out;

	/* Populate command struct */
	cmd->cmd_id = SDMX_SET_LOG_LEVEL_CMD;
	cmd->session_handle = session_handle;
	cmd->level = level;

	/* Issue QSEECom command */
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);
	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}
	ret = rsp->ret;
out:
	/* Unlock */
	mutex_unlock(&sdmx_lock[session_handle]);
	return ret;
}

