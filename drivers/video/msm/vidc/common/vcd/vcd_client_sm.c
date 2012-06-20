/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#include <media/msm/vidc_type.h>
#include "vcd.h"

static const struct vcd_clnt_state_table *vcd_clnt_state_table[];

void vcd_clnt_handle_device_err_fatal(struct vcd_clnt_ctxt *cctxt,
								  u32 event)
{
	if (cctxt->clnt_state.state == VCD_CLIENT_STATE_NULL) {
		cctxt->callback(VCD_EVT_RESP_OPEN, VCD_ERR_HW_FATAL, NULL, 0,
			cctxt, cctxt->client_data);
		vcd_destroy_client_context(cctxt);
		return;
	}
	if (event == VCD_EVT_RESP_BASE)
		event = VCD_EVT_IND_HWERRFATAL;
	if (cctxt->clnt_state.state != VCD_CLIENT_STATE_INVALID) {
		cctxt->callback(event, VCD_ERR_HW_FATAL, NULL, 0,
			cctxt, cctxt->client_data);
		vcd_flush_buffers_in_err_fatal(cctxt);
		vcd_do_client_state_transition(cctxt,
			VCD_CLIENT_STATE_INVALID,
			CLIENT_STATE_EVENT_NUMBER(clnt_cb));
	}
}

static u32 vcd_close_in_open(struct vcd_clnt_ctxt *cctxt)
{
	u32 rc = VCD_S_SUCCESS;

	VCD_MSG_LOW("vcd_close_in_open:");
	if (cctxt->in_buf_pool.allocated ||
		 cctxt->out_buf_pool.allocated) {
		VCD_MSG_ERROR("\n Allocated buffers are not freed yet");
		return VCD_ERR_ILLEGAL_OP;
	}
	vcd_destroy_client_context(cctxt);
	return rc;
}

static u32  vcd_close_in_invalid(struct vcd_clnt_ctxt *cctxt)
{
	VCD_MSG_LOW("vcd_close_in_invalid:");
	if (cctxt->in_buf_pool.allocated ||
		cctxt->out_buf_pool.allocated){
		VCD_MSG_ERROR("Allocated buffers are not freed yet");
		return VCD_ERR_ILLEGAL_OP;
	}

	if (cctxt->status.mask & VCD_CLEANING_UP)
		cctxt->status.mask |= VCD_CLOSE_PENDING;
	else
		vcd_destroy_client_context(cctxt);
	return VCD_S_SUCCESS;
}

static u32 vcd_start_in_run_cmn(struct vcd_clnt_ctxt *cctxt)
{
	VCD_MSG_LOW("vcd_start_in_run_cmn:");
	cctxt->callback(VCD_EVT_RESP_START, VCD_S_SUCCESS, NULL, 0,
					  cctxt, cctxt->client_data);
	return VCD_S_SUCCESS;

}

static u32 vcd_encode_start_in_open(struct vcd_clnt_ctxt *cctxt)
{
	u32 rc = VCD_S_SUCCESS;
	struct vcd_property_hdr prop_hdr;
	struct vcd_property_vop_timing timing;

	VCD_MSG_LOW("vcd_encode_start_in_open:");

	if (cctxt->decoding) {
		VCD_MSG_ERROR("vcd_encode_init for decoder client");

		return VCD_ERR_ILLEGAL_OP;
	}

	if ((!cctxt->meta_mode && !cctxt->in_buf_pool.entries) ||
	    !cctxt->out_buf_pool.entries ||
	    (!cctxt->meta_mode &&
		 cctxt->in_buf_pool.validated != cctxt->in_buf_pool.count) ||
	    cctxt->out_buf_pool.validated !=
	    cctxt->out_buf_pool.count) {
		VCD_MSG_HIGH("%s: Buffer pool is not completely setup yet",
			__func__);
	}

	rc = vcd_sched_add_client(cctxt);
	VCD_FAILED_RETURN(rc, "Failed: vcd_sched_add_client");

	prop_hdr.prop_id = VCD_I_VOP_TIMING;
	prop_hdr.sz = sizeof(struct vcd_property_vop_timing);
	rc = ddl_get_property(cctxt->ddl_handle, &prop_hdr, &timing);

	VCD_FAILED_RETURN(rc, "Failed: Get VCD_I_VOP_TIMING");
	if (!timing.vop_time_resolution) {
		VCD_MSG_ERROR("Vop_time_resolution value is zero");
		return VCD_ERR_FAIL;
	}
	cctxt->time_resoln = timing.vop_time_resolution;

	rc = vcd_process_cmd_sess_start(cctxt);

	if (!VCD_FAILED(rc)) {
		vcd_do_client_state_transition(cctxt,
					       VCD_CLIENT_STATE_STARTING,
					       CLIENT_STATE_EVENT_NUMBER
					       (encode_start));
	}

	return rc;
}

static u32  vcd_encode_start_in_run(struct vcd_clnt_ctxt
	*cctxt)
{
	VCD_MSG_LOW("vcd_encode_start_in_run:");
	(void) vcd_start_in_run_cmn(cctxt);
	return VCD_S_SUCCESS;
}


static u32 vcd_encode_frame_cmn(struct vcd_clnt_ctxt *cctxt,
     struct vcd_frame_data *input_frame)
{
	VCD_MSG_LOW("vcd_encode_frame_cmn in %d:", cctxt->clnt_state.state);

	if (cctxt->decoding) {
		VCD_MSG_ERROR("vcd_encode_frame for decoder client");

		return VCD_ERR_ILLEGAL_OP;
	}

	return vcd_handle_input_frame(cctxt, input_frame);
}

static u32 vcd_decode_start_in_open
    (struct vcd_clnt_ctxt *cctxt,
     struct vcd_sequence_hdr *seq_hdr)
{
	u32 rc = VCD_S_SUCCESS;

	VCD_MSG_LOW("vcd_decode_start_in_open:");

	if (!cctxt->decoding) {
		VCD_MSG_ERROR("vcd_decode_init for encoder client");

		return VCD_ERR_ILLEGAL_OP;
	}

	if (seq_hdr) {
		VCD_MSG_HIGH("Seq hdr supplied. len = %d",
			     seq_hdr->sequence_header_len);

		rc = vcd_store_seq_hdr(cctxt, seq_hdr);

	} else {
		VCD_MSG_HIGH("Seq hdr not supplied");

		cctxt->seq_hdr.sequence_header_len = 0;
		cctxt->seq_hdr.sequence_header = NULL;
	}

	VCD_FAILED_RETURN(rc, "Err processing seq hdr");

	rc = vcd_process_cmd_sess_start(cctxt);

	if (!VCD_FAILED(rc)) {
		vcd_do_client_state_transition(cctxt,
					       VCD_CLIENT_STATE_STARTING,
					       CLIENT_STATE_EVENT_NUMBER
					       (decode_start));
	}

	return rc;
}

static u32 vcd_decode_start_in_run(struct vcd_clnt_ctxt *cctxt,
	struct vcd_sequence_hdr *seqhdr)
{
   VCD_MSG_LOW("vcd_decode_start_in_run:");
   (void) vcd_start_in_run_cmn(cctxt);
   return VCD_S_SUCCESS;
}

static u32 vcd_decode_frame_cmn
    (struct vcd_clnt_ctxt *cctxt,
     struct vcd_frame_data *input_frame)
{
	VCD_MSG_LOW("vcd_decode_frame_cmn in %d:", cctxt->clnt_state.state);

	if (!cctxt->decoding) {
		VCD_MSG_ERROR("Decode_frame api called for Encoder client");

		return VCD_ERR_ILLEGAL_OP;
	}

	return vcd_handle_input_frame(cctxt, input_frame);
}

static u32 vcd_pause_in_run(struct vcd_clnt_ctxt *cctxt)
{
	u32 rc = VCD_S_SUCCESS;

	VCD_MSG_LOW("vcd_pause_in_run:");

	if (cctxt->sched_clnt_hdl) {
		rc = vcd_sched_suspend_resume_clnt(cctxt, false);
		VCD_FAILED_RETURN(rc, "Failed: vcd_sched_suspend_resume_clnt");
	}

	if (cctxt->status.frame_submitted > 0) {
		vcd_do_client_state_transition(cctxt,
					       VCD_CLIENT_STATE_PAUSING,
					       CLIENT_STATE_EVENT_NUMBER
					       (pause));

	} else {
		VCD_MSG_HIGH("No client frames are currently being processed");

		vcd_do_client_state_transition(cctxt,
					       VCD_CLIENT_STATE_PAUSED,
					       CLIENT_STATE_EVENT_NUMBER
					       (pause));

		cctxt->callback(VCD_EVT_RESP_PAUSE,
				  VCD_S_SUCCESS,
				  NULL, 0, cctxt, cctxt->client_data);

		rc = vcd_power_event(cctxt->dev_ctxt, cctxt,
				     VCD_EVT_PWR_CLNT_PAUSE);

		if (VCD_FAILED(rc))
			VCD_MSG_ERROR("VCD_EVT_PWR_CLNT_PAUSE_END failed");

	}

	return VCD_S_SUCCESS;
}

static u32 vcd_resume_in_paused(struct vcd_clnt_ctxt *cctxt)
{
	struct vcd_dev_ctxt *dev_ctxt = cctxt->dev_ctxt;
	u32 rc = VCD_S_SUCCESS;

	VCD_MSG_LOW("vcd_resume_in_paused:");


	if (cctxt->sched_clnt_hdl) {
		rc = vcd_power_event(cctxt->dev_ctxt,
				     cctxt, VCD_EVT_PWR_CLNT_RESUME);

		if (VCD_FAILED(rc)) {
			VCD_MSG_ERROR("VCD_EVT_PWR_CLNT_RESUME failed");
		} else {
			rc = vcd_sched_suspend_resume_clnt(cctxt, true);
			if (VCD_FAILED(rc)) {
				VCD_MSG_ERROR
				    ("rc = 0x%x. Failed: "
				     "vcd_sched_suspend_resume_clnt",
				     rc);
			}

		}
		if (!VCD_FAILED(rc)) {
			vcd_do_client_state_transition(cctxt,
						       VCD_CLIENT_STATE_RUN,
						       CLIENT_STATE_EVENT_NUMBER
						       (resume));
			vcd_try_submit_frame(dev_ctxt);
		}
	} else {
		vcd_do_client_state_transition(cctxt,
					       VCD_CLIENT_STATE_RUN,
					       CLIENT_STATE_EVENT_NUMBER
					       (resume));
	}

	return rc;
}

static u32 vcd_flush_cmn(struct vcd_clnt_ctxt *cctxt, u32 mode)
{
	u32 rc = VCD_S_SUCCESS;

	VCD_MSG_LOW("vcd_flush_cmn in %d:", cctxt->clnt_state.state);

	rc = vcd_flush_buffers(cctxt, mode);

	VCD_FAILED_RETURN(rc, "Failed: vcd_flush_buffers");

	if (cctxt->status.frame_submitted > 0) {
		vcd_do_client_state_transition(cctxt,
					       VCD_CLIENT_STATE_FLUSHING,
					       CLIENT_STATE_EVENT_NUMBER
					       (flush));
	} else {
		VCD_MSG_HIGH("All buffers are flushed");
		cctxt->status.mask |= (mode & VCD_FLUSH_ALL);
		vcd_send_flush_done(cctxt, VCD_S_SUCCESS);
	}

	return rc;
}

static u32  vcd_flush_inopen(struct vcd_clnt_ctxt *cctxt,
	u32 mode)
{
   VCD_MSG_LOW("vcd_flush_inopen:");
   cctxt->status.mask |= (mode & VCD_FLUSH_ALL);
   vcd_send_flush_done(cctxt, VCD_S_SUCCESS);
   return VCD_S_SUCCESS;
}

static u32 vcd_flush_in_flushing
    (struct vcd_clnt_ctxt *cctxt, u32 mode)
{
	u32 rc = VCD_S_SUCCESS;

	VCD_MSG_LOW("vcd_flush_in_flushing:");

	rc = vcd_flush_buffers(cctxt, mode);

	return rc;
}

static u32 vcd_flush_in_eos(struct vcd_clnt_ctxt *cctxt,
	u32 mode)
{
	u32 rc = VCD_S_SUCCESS;
	VCD_MSG_LOW("vcd_flush_in_eos:");

	if (mode > VCD_FLUSH_ALL || !mode) {
		VCD_MSG_ERROR("Invalid flush mode %d", mode);

		return VCD_ERR_ILLEGAL_PARM;
	}

	VCD_MSG_MED("Flush mode requested %d", mode);
	if (!(cctxt->status.frame_submitted) &&
		(!cctxt->decoding)) {
		rc = vcd_flush_buffers(cctxt, mode);
		if (!VCD_FAILED(rc)) {
			VCD_MSG_HIGH("All buffers are flushed");
			cctxt->status.mask |= (mode & VCD_FLUSH_ALL);
			vcd_send_flush_done(cctxt, VCD_S_SUCCESS);
		}
	} else
		cctxt->status.mask |= (mode & VCD_FLUSH_ALL);

	return rc;
}

static u32 vcd_flush_in_invalid(struct vcd_clnt_ctxt *cctxt,
	u32 mode)
{
	u32 rc = VCD_S_SUCCESS;
	VCD_MSG_LOW("vcd_flush_in_invalid:");
	if (!(cctxt->status.mask & VCD_CLEANING_UP)) {
		rc = vcd_flush_buffers(cctxt, mode);
		if (!VCD_FAILED(rc)) {
			VCD_MSG_HIGH("All buffers are flushed");
			cctxt->status.mask |= (mode & VCD_FLUSH_ALL);
			vcd_send_flush_done(cctxt, VCD_S_SUCCESS);
		}
	} else
		cctxt->status.mask |= (mode & VCD_FLUSH_ALL);
	return rc;
}

static u32 vcd_stop_cmn(struct vcd_clnt_ctxt *cctxt)
{
	struct vcd_dev_ctxt *dev_ctxt = cctxt->dev_ctxt;
	u32 rc = VCD_S_SUCCESS;
	struct vcd_transc *transc;

	VCD_MSG_LOW("vcd_stop_cmn in %d:", cctxt->clnt_state.state);

	rc = vcd_flush_buffers(cctxt, VCD_FLUSH_ALL);

	VCD_FAILED_RETURN(rc, "Failed: vcd_flush_buffers");

	if (!cctxt->status.frame_submitted) {

		if (vcd_get_command_channel(dev_ctxt, &transc)) {
			rc = vcd_power_event(dev_ctxt, cctxt,
				VCD_EVT_PWR_CLNT_CMD_BEGIN);

			if (!VCD_FAILED(rc)) {
				transc->type = VCD_CMD_CODEC_STOP;
				transc->cctxt = cctxt;

				rc = vcd_submit_cmd_sess_end(transc);
			} else {
				VCD_MSG_ERROR("Failed:"
					" VCD_EVT_PWR_CLNT_CMD_BEGIN");
			}

			if (VCD_FAILED(rc)) {
				vcd_release_command_channel(dev_ctxt,
							    transc);
			}

		} else {
			vcd_client_cmd_flush_and_en_q(cctxt,
						      VCD_CMD_CODEC_STOP);
		}
	}

	if (VCD_FAILED(rc)) {
		(void)vcd_power_event(dev_ctxt, cctxt,
				      VCD_EVT_PWR_CLNT_CMD_FAIL);
	} else {
		vcd_do_client_state_transition(cctxt,
					       VCD_CLIENT_STATE_STOPPING,
					       CLIENT_STATE_EVENT_NUMBER
					       (stop));
	}

	return rc;
}


static u32  vcd_stop_inopen(struct vcd_clnt_ctxt *cctxt)
{
	VCD_MSG_LOW("vcd_stop_inopen:");

	cctxt->callback(VCD_EVT_RESP_STOP, VCD_S_SUCCESS,
					  NULL, 0, cctxt,
					  cctxt->client_data);

	return VCD_S_SUCCESS;
}

static u32 vcd_stop_in_run(struct vcd_clnt_ctxt *cctxt)
{
	u32 rc = VCD_S_SUCCESS;
	VCD_MSG_LOW("vcd_stop_in_run:");
	rc = vcd_stop_cmn(cctxt);
	if (!VCD_FAILED(rc) &&
		(cctxt->status.mask & VCD_FIRST_IP_RCVD)) {
		rc = vcd_power_event(cctxt->dev_ctxt,
				     cctxt, VCD_EVT_PWR_CLNT_LAST_FRAME);
	}
	return rc;
}

static u32 vcd_stop_in_eos(struct vcd_clnt_ctxt *cctxt)
{
	u32 rc = VCD_S_SUCCESS;
	VCD_MSG_LOW("vcd_stop_in_eos:");
	if (cctxt->status.mask & VCD_EOS_WAIT_OP_BUF) {
		rc = vcd_stop_cmn(cctxt);
		if (!VCD_FAILED(rc)) {
			rc = vcd_power_event(cctxt->dev_ctxt,
				cctxt, VCD_EVT_PWR_CLNT_LAST_FRAME);
			cctxt->status.mask &= ~VCD_EOS_WAIT_OP_BUF;
		}
	} else
		cctxt->status.mask |= VCD_STOP_PENDING;
	return rc;
}

static u32  vcd_stop_in_invalid(struct vcd_clnt_ctxt *cctxt)
{
	VCD_MSG_LOW("vcd_stop_in_invalid:");
	if (cctxt->status.mask & VCD_CLEANING_UP) {
		cctxt->status.mask |= VCD_STOP_PENDING;
	} else {
		(void) vcd_flush_buffers(cctxt, VCD_FLUSH_ALL);
		cctxt->callback(VCD_EVT_RESP_STOP, VCD_S_SUCCESS, NULL,
			0, cctxt,	cctxt->client_data);
	}
	return VCD_S_SUCCESS;
}

static u32 vcd_set_property_cmn
    (struct vcd_clnt_ctxt *cctxt,
     struct vcd_property_hdr *prop_hdr, void *prop_val)
{
	u32 rc;
	VCD_MSG_LOW("vcd_set_property_cmn in %d:", cctxt->clnt_state.state);
	VCD_MSG_LOW("property Id = %d", prop_hdr->prop_id);
	if (!prop_hdr->sz || !prop_hdr->prop_id) {
		VCD_MSG_MED("Bad parameters");
		return VCD_ERR_ILLEGAL_PARM;
	}

	rc = ddl_set_property(cctxt->ddl_handle, prop_hdr, prop_val);
	if (rc) {
		/* Some properties aren't known to ddl that we can handle */
		if (prop_hdr->prop_id != VCD_I_VOP_TIMING_CONSTANT_DELTA)
			VCD_FAILED_RETURN(rc, "Failed: ddl_set_property");
	}

	switch (prop_hdr->prop_id) {
	case VCD_I_META_BUFFER_MODE:
		{
			struct vcd_property_live *live =
			    (struct vcd_property_live *)prop_val;
			cctxt->meta_mode = live->live;
			break;
		}
	case VCD_I_LIVE:
		{
			struct vcd_property_live *live =
			    (struct vcd_property_live *)prop_val;
			cctxt->live = live->live;
			break;
		}
	case VCD_I_FRAME_RATE:
		{
			if (cctxt->sched_clnt_hdl) {
				rc = vcd_set_frame_rate(cctxt,
					(struct vcd_property_frame_rate *)
					prop_val);
			}
			break;
		}
	case VCD_I_FRAME_SIZE:
		{
			if (cctxt->sched_clnt_hdl) {
				rc = vcd_set_frame_size(cctxt,
					(struct vcd_property_frame_size *)
					prop_val);
			}
			break;
		}
	case VCD_I_SET_TURBO_CLK:
	{
		if (cctxt->sched_clnt_hdl)
			rc = vcd_set_perf_turbo_level(cctxt);
		break;
	}
	case VCD_I_INTRA_PERIOD:
		{
			struct vcd_property_i_period *iperiod =
				(struct vcd_property_i_period *)prop_val;
			cctxt->bframe = iperiod->b_frames;
			break;
		}
	case VCD_REQ_PERF_LEVEL:
		rc = vcd_req_perf_level(cctxt,
				(struct vcd_property_perf_level *)prop_val);
		break;
	case VCD_I_VOP_TIMING_CONSTANT_DELTA:
		{
			struct vcd_property_vop_timing_constant_delta *delta =
				prop_val;

			if (delta->constant_delta > 0) {
				cctxt->time_frame_delta = delta->constant_delta;
				rc = VCD_S_SUCCESS;
			} else {
				VCD_MSG_ERROR("Frame delta must be positive");
				rc = VCD_ERR_ILLEGAL_PARM;
			}
			break;
		}
	default:
		{
			break;
		}
	}
	return rc;
}

static u32 vcd_get_property_cmn
    (struct vcd_clnt_ctxt *cctxt,
     struct vcd_property_hdr *prop_hdr, void *prop_val)
{
	int rc;
	VCD_MSG_LOW("vcd_get_property_cmn in %d:", cctxt->clnt_state.state);
	VCD_MSG_LOW("property Id = %d", prop_hdr->prop_id);
	if (!prop_hdr->sz || !prop_hdr->prop_id) {
		VCD_MSG_MED("Bad parameters");

		return VCD_ERR_ILLEGAL_PARM;
	}
	rc = ddl_get_property(cctxt->ddl_handle, prop_hdr, prop_val);
	if (rc) {
		/* Some properties aren't known to ddl that we can handle */
		if (prop_hdr->prop_id != VCD_I_VOP_TIMING_CONSTANT_DELTA)
			VCD_FAILED_RETURN(rc, "Failed: ddl_set_property");
	}

	switch (prop_hdr->prop_id) {
	case VCD_I_VOP_TIMING_CONSTANT_DELTA:
	{
		struct vcd_property_vop_timing_constant_delta *delta =
			(struct vcd_property_vop_timing_constant_delta *)
			prop_val;
		delta->constant_delta = cctxt->time_frame_delta;
		rc = VCD_S_SUCCESS;
	}
	}
	return rc;
}

static u32 vcd_set_buffer_requirements_cmn
    (struct vcd_clnt_ctxt *cctxt,
     enum vcd_buffer_type buffer,
     struct vcd_buffer_requirement *buffer_req)
{
	struct vcd_property_hdr Prop_hdr;
	u32 rc = VCD_S_SUCCESS;
	struct vcd_buffer_pool *buf_pool;
	u32 first_frm_recvd = 0;

	VCD_MSG_LOW("vcd_set_buffer_requirements_cmn in %d:",
		    cctxt->clnt_state.state);

	if (!cctxt->decoding &&
	    cctxt->clnt_state.state != VCD_CLIENT_STATE_OPEN) {
		VCD_MSG_ERROR("Bad state (%d) for encoder",
					cctxt->clnt_state.state);

		return VCD_ERR_BAD_STATE;
	}

	VCD_MSG_MED("Buffer type = %d", buffer);

	if (buffer == VCD_BUFFER_INPUT) {
		Prop_hdr.prop_id = DDL_I_INPUT_BUF_REQ;
		buf_pool = &cctxt->in_buf_pool;
		first_frm_recvd = VCD_FIRST_IP_RCVD;
	} else if (buffer == VCD_BUFFER_OUTPUT) {
		Prop_hdr.prop_id = DDL_I_OUTPUT_BUF_REQ;
		buf_pool = &cctxt->out_buf_pool;
		first_frm_recvd = VCD_FIRST_OP_RCVD;
	} else {
		rc = VCD_ERR_ILLEGAL_PARM;
	}

	VCD_FAILED_RETURN(rc, "Invalid buffer type provided");

	if (buf_pool->validated > 0) {
		VCD_MSG_ERROR("Need to free allocated buffers");
		return VCD_ERR_ILLEGAL_OP;
	}

	first_frm_recvd &= cctxt->status.mask;
	if (first_frm_recvd) {
		VCD_MSG_ERROR("VCD SetBufReq called when data path is active");
		return VCD_ERR_BAD_STATE;
	}
	Prop_hdr.sz = sizeof(*buffer_req);
	rc = ddl_set_property(cctxt->ddl_handle, &Prop_hdr, buffer_req);
	VCD_FAILED_RETURN(rc, "Failed: ddl_set_property");
	if (buf_pool->entries) {
		VCD_MSG_MED("Resetting buffer requirements");
		vcd_free_buffer_pool_entries(buf_pool);
	}
	return rc;
}

static u32 vcd_get_buffer_requirements_cmn
    (struct vcd_clnt_ctxt *cctxt,
     enum vcd_buffer_type buffer,
     struct vcd_buffer_requirement *buffer_req)
{
	struct vcd_property_hdr Prop_hdr;
	u32 rc = VCD_S_SUCCESS;

	VCD_MSG_LOW("vcd_get_buffer_requirements_cmn in %d:",
		    cctxt->clnt_state.state);

	VCD_MSG_MED("Buffer type = %d", buffer);

	if (buffer == VCD_BUFFER_INPUT)
		Prop_hdr.prop_id = DDL_I_INPUT_BUF_REQ;
	else if (buffer == VCD_BUFFER_OUTPUT)
		Prop_hdr.prop_id = DDL_I_OUTPUT_BUF_REQ;
	else
		rc = VCD_ERR_ILLEGAL_PARM;

	VCD_FAILED_RETURN(rc, "Invalid buffer type provided");

	Prop_hdr.sz = sizeof(*buffer_req);

	return ddl_get_property(cctxt->ddl_handle, &Prop_hdr, buffer_req);

}

static u32 vcd_set_buffer_cmn
    (struct vcd_clnt_ctxt *cctxt,
     enum vcd_buffer_type buffer_type, u8 *buffer, u32 buf_size)
{
	u32 rc;
	struct vcd_buffer_pool *buf_pool;

	VCD_MSG_LOW("vcd_set_buffer_cmn in %d:", cctxt->clnt_state.state);

	rc = vcd_common_allocate_set_buffer(cctxt, buffer_type, buf_size,
					    &buf_pool);

	if (!VCD_FAILED(rc)) {
		rc = vcd_set_buffer_internal(cctxt, buf_pool, buffer,
					     buf_size);
	}

	return rc;
}

static u32 vcd_allocate_buffer_cmn
    (struct vcd_clnt_ctxt *cctxt,
     enum vcd_buffer_type buffer,
     u32 buf_size, u8 **vir_buf_addr, u8 **phy_buf_addr)
{
	u32 rc;
	struct vcd_buffer_pool *buf_pool;

	VCD_MSG_LOW("vcd_allocate_buffer_cmn in %d:",
		    cctxt->clnt_state.state);

	rc = vcd_common_allocate_set_buffer(cctxt, buffer, buf_size,
					    &buf_pool);

	if (!VCD_FAILED(rc)) {
		rc = vcd_allocate_buffer_internal(cctxt,
						  buf_pool,
						  buf_size,
						  vir_buf_addr,
						  phy_buf_addr);
	}

	return rc;
}

static u32 vcd_free_buffer_cmn
    (struct vcd_clnt_ctxt *cctxt,
     enum vcd_buffer_type buffer_type, u8 *buffer)
{

	VCD_MSG_LOW("vcd_free_buffer_cmn in %d:", cctxt->clnt_state.state);

	return vcd_free_one_buffer_internal(cctxt, buffer_type, buffer);
}

static u32 vcd_fill_output_buffer_cmn
    (struct vcd_clnt_ctxt *cctxt,
     struct vcd_frame_data *buffer)
{
	u32 rc = VCD_S_SUCCESS;
	struct vcd_buffer_entry *buf_entry;
	u32 result = true;
	u32 handled = true;
	if (!cctxt || !buffer) {
		VCD_MSG_ERROR("%s(): Inavlid params cctxt %p buffer %p",
					__func__, cctxt, buffer);
		return VCD_ERR_BAD_POINTER;
	}
	VCD_MSG_LOW("vcd_fill_output_buffer_cmn in %d:",
		    cctxt->clnt_state.state);
	if (cctxt->status.mask & VCD_IN_RECONFIG) {
		buffer->time_stamp = 0;
		buffer->data_len = 0;
		VCD_MSG_LOW("In reconfig: Return output buffer");
		cctxt->callback(VCD_EVT_RESP_OUTPUT_DONE,
			VCD_S_SUCCESS,
			buffer,
			sizeof(struct vcd_frame_data),
			cctxt, cctxt->client_data);
		return rc;
	}
	buf_entry = vcd_check_fill_output_buffer(cctxt, buffer);
	if (!buf_entry)
		return VCD_ERR_BAD_POINTER;

	if (!(cctxt->status.mask & VCD_FIRST_OP_RCVD)) {
		rc = vcd_handle_first_fill_output_buffer(cctxt, buffer,
			&handled);
		VCD_FAILED_RETURN(rc,
			"Failed: vcd_handle_first_fill_output_buffer");
		if (handled)
			return rc ;
	}

	result =
	    vcd_buffer_pool_entry_en_q(&cctxt->out_buf_pool, buf_entry);

	if (!result && !cctxt->decoding) {
		VCD_MSG_ERROR("Failed: vcd_buffer_pool_entry_en_q");

		return VCD_ERR_FAIL;
	}

	buf_entry->frame = *buffer;
	rc = vcd_return_op_buffer_to_hw(cctxt, buf_entry);
	if (!VCD_FAILED(rc) && cctxt->sched_clnt_hdl) {
		cctxt->sched_clnt_hdl->tkns++;
		vcd_try_submit_frame(cctxt->dev_ctxt);
	}
	return rc;
}

static u32 vcd_fill_output_buffer_in_eos
    (struct vcd_clnt_ctxt *cctxt,
     struct vcd_frame_data *buffer)
{
	u32 rc = VCD_S_SUCCESS;
	struct vcd_buffer_entry *buf_entry;

	VCD_MSG_LOW("vcd_fill_output_buffer_in_eos:");

	buf_entry = vcd_check_fill_output_buffer(cctxt, buffer);
	if (!buf_entry)
		return VCD_ERR_BAD_POINTER;

	if (cctxt->status.mask & VCD_EOS_WAIT_OP_BUF) {
		VCD_MSG_HIGH("Got an output buffer we were waiting for");

		buf_entry->frame = *buffer;

		buf_entry->frame.data_len = 0;
		buf_entry->frame.flags |= VCD_FRAME_FLAG_EOS;
		buf_entry->frame.ip_frm_tag =
		    cctxt->status.eos_trig_ip_frm.ip_frm_tag;
		buf_entry->frame.time_stamp =
		    cctxt->status.eos_trig_ip_frm.time_stamp;

		cctxt->callback(VCD_EVT_RESP_OUTPUT_DONE,
				  VCD_S_SUCCESS,
				  &buf_entry->frame,
				  sizeof(struct vcd_frame_data),
				  cctxt, cctxt->client_data);

		cctxt->status.mask &= ~VCD_EOS_WAIT_OP_BUF;

		vcd_do_client_state_transition(cctxt,
					       VCD_CLIENT_STATE_RUN,
					       CLIENT_STATE_EVENT_NUMBER
					       (fill_output_buffer));

	} else {
		rc = vcd_fill_output_buffer_cmn(cctxt, buffer);
	}

	return rc;
}

static void vcd_clnt_cb_in_starting
    (struct vcd_clnt_ctxt *cctxt,
     u32 event, u32 status, void *payload, size_t sz,
	 u32 *ddl_handle, void *const client_data)
{
	struct vcd_dev_ctxt *dev_ctxt = cctxt->dev_ctxt;
	struct vcd_transc *transc =
		(struct vcd_transc *)client_data;
	VCD_MSG_LOW("vcd_clnt_cb_in_starting:");
	if (cctxt->ddl_handle != ddl_handle) {
		VCD_MSG_ERROR("vcd_clnt_cb_in_initing: Wrong DDL handle %p",
			ddl_handle);
		return;
	}

	switch (event) {
	case VCD_EVT_RESP_START:
		{
			vcd_handle_start_done(cctxt,
				(struct vcd_transc *)client_data,
				status);
			break;
		}
	case VCD_EVT_RESP_STOP:
		{
			vcd_handle_stop_done_in_starting(cctxt,
				(struct vcd_transc *)client_data,
				status);
			break;
		}
	case VCD_EVT_IND_HWERRFATAL:
		{
			cctxt->status.cmd_submitted--;
			vcd_mark_command_channel(cctxt->dev_ctxt, transc);
			vcd_handle_err_fatal(cctxt, VCD_EVT_RESP_START,
				status);
			break;
		}
	default:
		{
			VCD_MSG_ERROR("Unexpected callback event=%d status=%d "
				"from DDL",	event, status);
			dev_ctxt->command_continue = false;
			break;
		}
	}
}

static void vcd_clnt_cb_in_run
    (struct vcd_clnt_ctxt *cctxt,
     u32 event,
     u32 status,
     void *payload, size_t sz, u32 *ddl_handle, void *const client_data)
{
	struct vcd_dev_ctxt *dev_ctxt = cctxt->dev_ctxt;
	u32 rc = VCD_S_SUCCESS;

	if (cctxt->ddl_handle != ddl_handle) {
		VCD_MSG_ERROR("ddl_handle mismatch");

		return;
	}

	switch (event) {
	case VCD_EVT_RESP_INPUT_DONE:
		{
			rc = vcd_handle_input_done(cctxt, payload, event,
						   status);

			break;
		}

	case VCD_EVT_RESP_OUTPUT_DONE:
		{

			rc = vcd_handle_frame_done(cctxt, payload, event,
						   status);

			break;
		}
	case VCD_EVT_RESP_OUTPUT_REQ:
		{
			rc = vcd_handle_output_required(cctxt, payload,
				status);
			break;
		}

	case VCD_EVT_IND_OUTPUT_RECONFIG:
		{
			rc = vcd_handle_ind_output_reconfig(cctxt, payload,
				status);
      break;
		}
	case VCD_EVT_RESP_TRANSACTION_PENDING:
		{
			 vcd_handle_trans_pending(cctxt);
			 break;
		}

	case VCD_EVT_IND_HWERRFATAL:
		{
			 vcd_handle_ind_hw_err_fatal(cctxt,
				VCD_EVT_IND_HWERRFATAL, status);
			 break;
		}
	case VCD_EVT_IND_INFO_OUTPUT_RECONFIG:
		{
			vcd_handle_ind_info_output_reconfig(cctxt, status);
			break;
		}
	default:
		{
			VCD_MSG_ERROR
			    ("Unexpected callback event=%d status=%d from DDL",
			     event, status);
			dev_ctxt->command_continue = false;

			break;
		}
	}

	if (!VCD_FAILED(rc) &&
	    (event == VCD_EVT_RESP_INPUT_DONE ||
	     event == VCD_EVT_RESP_OUTPUT_DONE ||
	     event == VCD_EVT_RESP_OUTPUT_REQ)) {

		if (((struct ddl_frame_data_tag *)
					payload)->frm_trans_end)
			vcd_mark_frame_channel(cctxt->dev_ctxt);
	}
}

static void vcd_clnt_cb_in_eos
    (struct vcd_clnt_ctxt *cctxt,
     u32 event,
     u32 status,
     void *payload, size_t sz, u32 *ddl_handle, void *const client_data) {
	struct vcd_dev_ctxt *dev_ctxt = cctxt->dev_ctxt;
	struct vcd_transc *transc = NULL;
	u32 frm_trans_end = false, rc = VCD_S_SUCCESS;

	if (cctxt->ddl_handle != ddl_handle) {
		VCD_MSG_ERROR("ddl_handle mismatch");

		return;
	}

	switch (event) {
	case VCD_EVT_RESP_INPUT_DONE:
		{
			rc = vcd_handle_input_done_in_eos(cctxt, payload,
						     status);

			break;
		}

	case VCD_EVT_RESP_OUTPUT_DONE:
		{
			rc = vcd_handle_frame_done_in_eos(cctxt, payload,
						     status);

			break;
		}
	case VCD_EVT_RESP_OUTPUT_REQ:
		{
			rc = vcd_handle_output_required(cctxt, payload,
					status);
			break;
		}
	case VCD_EVT_RESP_EOS_DONE:
		{
			transc = (struct vcd_transc *)client_data;
			vcd_handle_eos_done(cctxt, transc, status);
			vcd_mark_frame_channel(cctxt->dev_ctxt);
			break;
		}
	case VCD_EVT_IND_OUTPUT_RECONFIG:
		{
			rc = vcd_handle_ind_output_reconfig(cctxt,
				payload, status);
			if (!VCD_FAILED(rc)) {
				frm_trans_end = true;
				payload = NULL;
				vcd_do_client_state_transition(cctxt,
					VCD_CLIENT_STATE_RUN,
					CLIENT_STATE_EVENT_NUMBER
					(clnt_cb));
				VCD_MSG_LOW
					("RECONFIGinEOS:Suspending Client");
				rc = vcd_sched_suspend_resume_clnt(cctxt,
						false);
				if (VCD_FAILED(rc)) {
					VCD_MSG_ERROR
					("Failed: suspend_resume_clnt. rc=0x%x",
						rc);
				}
			}
			break;
		}
	case VCD_EVT_IND_HWERRFATAL:
		{
			vcd_handle_ind_hw_err_fatal(cctxt,
				VCD_EVT_IND_HWERRFATAL,	status);
			break;
		}
	case VCD_EVT_IND_INFO_OUTPUT_RECONFIG:
		{
			vcd_handle_ind_info_output_reconfig(cctxt, status);
			break;
		}
	default:
		{
			VCD_MSG_ERROR
			    ("Unexpected callback event=%d status=%d from DDL",
			     event, status);

			dev_ctxt->command_continue = false;

			break;
		}

	}
	if (!VCD_FAILED(rc) &&
		(event == VCD_EVT_RESP_INPUT_DONE ||
		event == VCD_EVT_RESP_OUTPUT_DONE ||
		event == VCD_EVT_RESP_OUTPUT_REQ ||
		event == VCD_EVT_IND_OUTPUT_RECONFIG)) {
		if (payload && ((struct ddl_frame_data_tag *)
			payload)->frm_trans_end) {
			vcd_mark_frame_channel(cctxt->dev_ctxt);
			frm_trans_end = true;
		}
		if (frm_trans_end && !cctxt->status.frame_submitted)
			vcd_handle_eos_trans_end(cctxt);
	}
}

static void vcd_clnt_cb_in_flushing
    (struct vcd_clnt_ctxt *cctxt,
     u32 event,
     u32 status,
     void *payload, size_t sz, u32 *ddl_handle, void *const client_data) {
	struct vcd_dev_ctxt *dev_ctxt = cctxt->dev_ctxt;
	u32 rc = VCD_S_SUCCESS;
	u32 frm_trans_end = false;

	VCD_MSG_LOW("vcd_clnt_cb_in_flushing:");

	if (cctxt->ddl_handle != ddl_handle) {
		VCD_MSG_ERROR("ddl_handle mismatch");

		return;
	}

	switch (event) {
	case VCD_EVT_RESP_INPUT_DONE:
		{
			rc = vcd_handle_input_done(cctxt,
						   payload,
						   VCD_EVT_RESP_INPUT_FLUSHED,
						   status);

			break;
		}

	case VCD_EVT_RESP_OUTPUT_DONE:
		{

			rc = vcd_handle_frame_done(cctxt,
						   payload,
						   VCD_EVT_RESP_OUTPUT_FLUSHED,
						   status);

			break;
		}
	case VCD_EVT_RESP_OUTPUT_REQ:
		{
			rc = vcd_handle_output_required_in_flushing(cctxt,
				payload);
			break;
		}
	case VCD_EVT_IND_OUTPUT_RECONFIG:
		{
			rc = vcd_handle_ind_output_reconfig(cctxt,
				payload, status);
			if (!VCD_FAILED(rc)) {
				frm_trans_end = true;
				payload = NULL;
			}
			break;
		}
	case VCD_EVT_IND_HWERRFATAL:
		{
			vcd_handle_ind_hw_err_fatal(cctxt,
				VCD_EVT_IND_HWERRFATAL,	status);
			break;
		}
	default:
		{
			VCD_MSG_ERROR
			    ("Unexpected callback event=%d status=%d from DDL",
			     event, status);

			dev_ctxt->command_continue = false;

			break;
		}
	}
	if (!VCD_FAILED(rc) && ((event == VCD_EVT_RESP_INPUT_DONE ||
		event == VCD_EVT_RESP_OUTPUT_DONE ||
		event == VCD_EVT_RESP_OUTPUT_REQ ||
		event == VCD_EVT_IND_OUTPUT_RECONFIG))) {
		if (payload &&
			((struct ddl_frame_data_tag *)\
			payload)->frm_trans_end) {

			vcd_mark_frame_channel(cctxt->dev_ctxt);
			frm_trans_end = true;
		}
		if (frm_trans_end && !cctxt->status.frame_submitted) {
			VCD_MSG_HIGH
			    ("All pending frames recvd from DDL");
			if (cctxt->status.mask & VCD_FLUSH_INPUT)
				vcd_flush_bframe_buffers(cctxt,
							VCD_FLUSH_INPUT);
			if (cctxt->status.mask & VCD_FLUSH_OUTPUT)
				vcd_flush_output_buffers(cctxt);
			vcd_send_flush_done(cctxt, VCD_S_SUCCESS);
			vcd_release_interim_frame_channels(dev_ctxt);
			VCD_MSG_HIGH("Flush complete");
			vcd_do_client_state_transition(cctxt,
				VCD_CLIENT_STATE_RUN,
				CLIENT_STATE_EVENT_NUMBER
				(clnt_cb));
		}
	}
}

static void vcd_clnt_cb_in_stopping
    (struct vcd_clnt_ctxt *cctxt,
     u32 event,
     u32 status,
     void *payload, size_t sz, u32 *ddl_handle, void *const client_data) {
	struct vcd_dev_ctxt *dev_ctxt = cctxt->dev_ctxt;
	u32 rc = VCD_S_SUCCESS;
	u32 frm_trans_end = false;

	VCD_MSG_LOW("vcd_clnt_cb_in_stopping:");

	if (cctxt->ddl_handle != ddl_handle) {
		VCD_MSG_ERROR("ddl_handle mismatch");

		return;
	}

	switch (event) {

	case VCD_EVT_RESP_INPUT_DONE:
		{
			rc = vcd_handle_input_done(cctxt,
						   payload,
						   VCD_EVT_RESP_INPUT_FLUSHED,
						   status);

			break;
		}

	case VCD_EVT_RESP_OUTPUT_DONE:
		{

			rc = vcd_handle_frame_done(cctxt,
						   payload,
						   VCD_EVT_RESP_OUTPUT_FLUSHED,
						   status);

			break;
		}
	case VCD_EVT_RESP_OUTPUT_REQ:
		{
			rc = vcd_handle_output_required_in_flushing(cctxt,
				payload);
			break;
		}
	case VCD_EVT_RESP_STOP:
		{
			vcd_handle_stop_done(cctxt,
					     (struct vcd_transc *)
					     client_data, status);

			break;
		}
	case VCD_EVT_IND_OUTPUT_RECONFIG:
		{
			(void) vcd_handle_ind_output_reconfig(cctxt,
				payload, status);

			frm_trans_end = true;
			payload = NULL;

			break;
		}
	case VCD_EVT_IND_HWERRFATAL:
		{
			vcd_handle_ind_hw_err_fatal(cctxt, VCD_EVT_RESP_STOP,
				status);
			break;
		}

	default:
		{
			VCD_MSG_ERROR
			    ("Unexpected callback event=%d status=%d from DDL",
			     event, status);

			dev_ctxt->command_continue = false;

			break;
		}
	}

	if (!VCD_FAILED(rc) && ((event == VCD_EVT_RESP_INPUT_DONE ||
		event == VCD_EVT_RESP_OUTPUT_DONE) ||
		event == VCD_EVT_RESP_OUTPUT_REQ ||
		event == VCD_EVT_IND_OUTPUT_RECONFIG)) {

		if (payload &&
			((struct ddl_frame_data_tag *)\
			payload)->frm_trans_end) {

			vcd_mark_frame_channel(cctxt->dev_ctxt);
			frm_trans_end = true;
		}
		if (frm_trans_end && !cctxt->status.frame_submitted) {
				VCD_MSG_HIGH
					("All pending frames recvd from DDL");
				vcd_flush_bframe_buffers(cctxt,
							VCD_FLUSH_INPUT);
				vcd_flush_output_buffers(cctxt);
				cctxt->status.mask &= ~VCD_FLUSH_ALL;
				vcd_release_all_clnt_frm_transc(cctxt);
				VCD_MSG_HIGH
				("All buffers flushed. Enqueuing stop cmd");
				vcd_client_cmd_flush_and_en_q(cctxt,
						VCD_CMD_CODEC_STOP);
		}
	}
}

static void vcd_clnt_cb_in_pausing
    (struct vcd_clnt_ctxt *cctxt,
     u32 event,
     u32 status,
     void *payload, size_t sz, u32 *ddl_handle, void *const client_data)
{
	struct vcd_dev_ctxt *dev_ctxt = cctxt->dev_ctxt;
	u32 rc = VCD_S_SUCCESS;
	u32 frm_trans_end = false;

	VCD_MSG_LOW("vcd_clnt_cb_in_pausing:");

	if (cctxt->ddl_handle != ddl_handle) {
		VCD_MSG_ERROR("ddl_handle mismatch");

		return;
	}

	switch (event) {
	case VCD_EVT_RESP_INPUT_DONE:
		{
			rc = vcd_handle_input_done(cctxt, payload, event,
						   status);

			break;
		}

	case VCD_EVT_RESP_OUTPUT_DONE:
		{
			rc = vcd_handle_frame_done(cctxt, payload, event,
						   status);
			break;
		}
	case VCD_EVT_RESP_OUTPUT_REQ:
		{
			rc = vcd_handle_output_required(cctxt, payload,
				status);
			break;
		}
	case VCD_EVT_IND_OUTPUT_RECONFIG:
		{
			rc = vcd_handle_ind_output_reconfig(cctxt,
				payload, status);
			if (!VCD_FAILED(rc)) {
				frm_trans_end = true;
				payload = NULL;
			}
			break;
		}
	case VCD_EVT_IND_HWERRFATAL:
		{
			vcd_handle_ind_hw_err_fatal(cctxt,
				VCD_EVT_RESP_PAUSE,	status);
			rc = VCD_ERR_FAIL;
			break;
		}
	default:
		{
			VCD_MSG_ERROR
			    ("Unexpected callback event=%d status=%d from DDL",
			     event, status);

			dev_ctxt->command_continue = false;

			break;
		}

	}

	if (!VCD_FAILED(rc)) {

		if (payload &&
			((struct ddl_frame_data_tag *)\
			payload)->frm_trans_end) {

			vcd_mark_frame_channel(cctxt->dev_ctxt);
			frm_trans_end = true;
		}
		if (frm_trans_end && !cctxt->status.frame_submitted) {
			VCD_MSG_HIGH
			    ("All pending frames recvd from DDL");

			cctxt->callback(VCD_EVT_RESP_PAUSE,
					  VCD_S_SUCCESS,
					  NULL,
					  0,
					  cctxt,
					  cctxt->client_data);

			vcd_do_client_state_transition(cctxt,
					VCD_CLIENT_STATE_PAUSED,
					CLIENT_STATE_EVENT_NUMBER
						       (clnt_cb));

			rc = vcd_power_event(cctxt->dev_ctxt,
					     cctxt,
					     VCD_EVT_PWR_CLNT_PAUSE);

			if (VCD_FAILED(rc)) {
				VCD_MSG_ERROR
				    ("VCD_EVT_PWR_CLNT_PAUSE_END"
				     "failed");
			}
		}
	}
}

static void  vcd_clnt_cb_in_invalid(
   struct vcd_clnt_ctxt *cctxt, u32 event, u32 status,
   void *payload, size_t sz, u32 *ddl_handle,
   void *const client_data
)
{
	struct vcd_dev_ctxt *dev_ctxt = cctxt->dev_ctxt;
	VCD_MSG_LOW("vcd_clnt_cb_in_invalid:");
	if (cctxt->ddl_handle != ddl_handle) {
		VCD_MSG_ERROR("ddl_handle mismatch");
		return;
	}
	switch (event) {
	case VCD_EVT_RESP_STOP:
		{
			vcd_handle_stop_done_in_invalid(cctxt,
				(struct vcd_transc *)client_data,
				status);
			break;
		}
	case VCD_EVT_RESP_INPUT_DONE:
	case VCD_EVT_RESP_OUTPUT_REQ:
		{
			if (cctxt->status.frame_submitted)
				cctxt->status.frame_submitted--;
			if (payload && ((struct ddl_frame_data_tag *)
							payload)->frm_trans_end)
				vcd_mark_frame_channel(cctxt->dev_ctxt);
			break;
		}
	case VCD_EVT_RESP_OUTPUT_DONE:
		{
			if (payload && ((struct ddl_frame_data_tag *)
							payload)->frm_trans_end)
				vcd_mark_frame_channel(cctxt->dev_ctxt);
			break;
		}
	case VCD_EVT_RESP_TRANSACTION_PENDING:
		{
			if (cctxt->status.frame_submitted)
				cctxt->status.frame_submitted--;
			vcd_mark_frame_channel(cctxt->dev_ctxt);
			break;
		}
	case VCD_EVT_IND_HWERRFATAL:
		{
			if (status == VCD_ERR_HW_FATAL)
				vcd_handle_stop_done_in_invalid(cctxt,
					(struct vcd_transc *)client_data,
					status);

			break;
		}
	case VCD_EVT_RESP_EOS_DONE:
		{
			vcd_mark_frame_channel(cctxt->dev_ctxt);
			break;
		}
	case VCD_EVT_IND_OUTPUT_RECONFIG:
		{
			if (cctxt->status.frame_submitted > 0)
				cctxt->status.frame_submitted--;
			else
				cctxt->status.frame_delayed--;
			vcd_mark_frame_channel(cctxt->dev_ctxt);
			break;
		}
	default:
		{
			VCD_MSG_ERROR("Unexpected callback event=%d status=%d"
				"from DDL",	event, status);
			dev_ctxt->command_continue = false;
			break;
		}
	}
}

static void vcd_clnt_enter_open
    (struct vcd_clnt_ctxt *cctxt, s32 state_event) {
	VCD_MSG_MED("Entering CLIENT_STATE_OPEN on api %d", state_event);
}

static void vcd_clnt_enter_starting
    (struct vcd_clnt_ctxt *cctxt, s32 state_event) {
	VCD_MSG_MED("Entering CLIENT_STATE_STARTING on api %d",
		    state_event);
	cctxt->status.last_evt = VCD_EVT_RESP_START;
}

static void vcd_clnt_enter_run
    (struct vcd_clnt_ctxt *cctxt, s32 state_event) {
	VCD_MSG_MED("Entering CLIENT_STATE_RUN on api %d", state_event);
}

static void vcd_clnt_enter_flushing
    (struct vcd_clnt_ctxt *cctxt, s32 state_event) {
	VCD_MSG_MED("Entering CLIENT_STATE_FLUSHING on api %d",
		    state_event);
}

static void vcd_clnt_enter_stopping
    (struct vcd_clnt_ctxt *cctxt, s32 state_event) {
	VCD_MSG_MED("Entering CLIENT_STATE_STOPPING on api %d",
		    state_event);
	cctxt->status.last_evt = VCD_EVT_RESP_STOP;
}

static void vcd_clnt_enter_eos(struct vcd_clnt_ctxt *cctxt,
	s32 state_event)
{
   u32     rc;
   VCD_MSG_MED("Entering CLIENT_STATE_EOS on api %d", state_event);
	rc = vcd_sched_suspend_resume_clnt(cctxt, false);
	if (VCD_FAILED(rc))
		VCD_MSG_ERROR("Failed: vcd_sched_suspend_resume_clnt."
					  "rc=0x%x", rc);
}

static void vcd_clnt_enter_pausing
    (struct vcd_clnt_ctxt *cctxt, s32 state_event) {
	VCD_MSG_MED("Entering CLIENT_STATE_PAUSING on api %d",
		    state_event);
	cctxt->status.last_evt = VCD_EVT_RESP_PAUSE;
}

static void vcd_clnt_enter_paused
    (struct vcd_clnt_ctxt *cctxt, s32 state_event)
{
	VCD_MSG_MED("Entering CLIENT_STATE_PAUSED on api %d",
		state_event);
}

static void  vcd_clnt_enter_invalid(struct vcd_clnt_ctxt *cctxt,
	s32 state_event)
{
	VCD_MSG_MED("Entering CLIENT_STATE_INVALID on api %d",
		state_event);
	cctxt->ddl_hdl_valid = false;
	cctxt->status.mask &= ~(VCD_FIRST_IP_RCVD | VCD_FIRST_OP_RCVD);
	if (cctxt->sched_clnt_hdl)
		vcd_sched_suspend_resume_clnt(cctxt, false);
}

static void vcd_clnt_exit_open
    (struct vcd_clnt_ctxt *cctxt, s32 state_event)
{
	VCD_MSG_MED("Exiting CLIENT_STATE_OPEN on api %d", state_event);
}

static void vcd_clnt_exit_starting
    (struct vcd_clnt_ctxt *cctxt, s32 state_event) {
	VCD_MSG_MED("Exiting CLIENT_STATE_STARTING on api %d",
		    state_event);
	cctxt->status.last_evt = VCD_EVT_RESP_BASE;
}

static void vcd_clnt_exit_run
    (struct vcd_clnt_ctxt *cctxt, s32 state_event) {
	VCD_MSG_MED("Exiting CLIENT_STATE_RUN on api %d", state_event);
}

static void vcd_clnt_exit_flushing
    (struct vcd_clnt_ctxt *cctxt, s32 state_event) {
	VCD_MSG_MED("Exiting CLIENT_STATE_FLUSHING on api %d",
		    state_event);
}

static void vcd_clnt_exit_stopping
    (struct vcd_clnt_ctxt *cctxt, s32 state_event) {
	VCD_MSG_MED("Exiting CLIENT_STATE_STOPPING on api %d",
		    state_event);
	cctxt->status.last_evt = VCD_EVT_RESP_BASE;
}

static void vcd_clnt_exit_eos
    (struct vcd_clnt_ctxt *cctxt, s32 state_event)
{
	u32 rc;
	VCD_MSG_MED("Exiting CLIENT_STATE_EOS on api %d", state_event);
	rc = vcd_sched_suspend_resume_clnt(cctxt, true);
	if (VCD_FAILED(rc))
		VCD_MSG_ERROR("Failed: vcd_sched_suspend_resume_clnt. rc=0x%x",
			rc);
}

static void vcd_clnt_exit_pausing
    (struct vcd_clnt_ctxt *cctxt, s32 state_event) {
	VCD_MSG_MED("Exiting CLIENT_STATE_PAUSING on api %d",
		    state_event);
	cctxt->status.last_evt = VCD_EVT_RESP_BASE;
}

static void vcd_clnt_exit_paused
    (struct vcd_clnt_ctxt *cctxt, s32 state_event) {
	VCD_MSG_MED("Exiting CLIENT_STATE_PAUSED on api %d",
		    state_event);
}

static void  vcd_clnt_exit_invalid(struct vcd_clnt_ctxt *cctxt,
	s32 state_event)
{
	VCD_MSG_MED("Exiting CLIENT_STATE_INVALID on api %d",
		state_event);
}

void vcd_do_client_state_transition(struct vcd_clnt_ctxt *cctxt,
     enum vcd_clnt_state_enum to_state, u32 ev_code)
{
	struct vcd_clnt_state_ctxt *state_ctxt;

	if (!cctxt || to_state >= VCD_CLIENT_STATE_MAX) {
		VCD_MSG_ERROR("Bad parameters. cctxt=%p, to_state=%d",
			      cctxt, to_state);
	}

	state_ctxt = &cctxt->clnt_state;

	if (state_ctxt->state == to_state) {
		VCD_MSG_HIGH("Client already in requested to_state=%d",
			     to_state);

		return;
	}

	VCD_MSG_MED("vcd_do_client_state_transition: C%d -> C%d, for api %d",
		    (int)state_ctxt->state, (int)to_state, ev_code);

	if (state_ctxt->state_table->exit)
		state_ctxt->state_table->exit(cctxt, ev_code);


	state_ctxt->state = to_state;
	state_ctxt->state_table = vcd_clnt_state_table[to_state];

	if (state_ctxt->state_table->entry)
		state_ctxt->state_table->entry(cctxt, ev_code);
}

const struct vcd_clnt_state_table *vcd_get_client_state_table
    (enum vcd_clnt_state_enum state) {
	return vcd_clnt_state_table[state];
}

static const struct vcd_clnt_state_table vcd_clnt_table_open = {
	{
	 vcd_close_in_open,
	 vcd_encode_start_in_open,
	 NULL,
	 vcd_decode_start_in_open,
	 NULL,
	 NULL,
	 NULL,
	 vcd_flush_inopen,
	 vcd_stop_inopen,
	 vcd_set_property_cmn,
	 vcd_get_property_cmn,
	 vcd_set_buffer_requirements_cmn,
	 vcd_get_buffer_requirements_cmn,
	 vcd_set_buffer_cmn,
	 vcd_allocate_buffer_cmn,
	 vcd_free_buffer_cmn,
	 vcd_fill_output_buffer_cmn,
	 NULL,
	 },
	vcd_clnt_enter_open,
	vcd_clnt_exit_open
};

static const struct vcd_clnt_state_table vcd_clnt_table_starting = {
	{
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 vcd_get_property_cmn,
	 NULL,
	 vcd_get_buffer_requirements_cmn,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 vcd_clnt_cb_in_starting,
	 },
	vcd_clnt_enter_starting,
	vcd_clnt_exit_starting
};

static const struct vcd_clnt_state_table vcd_clnt_table_run = {
	{
	 NULL,
	 vcd_encode_start_in_run,
	 vcd_encode_frame_cmn,
	 vcd_decode_start_in_run,
	 vcd_decode_frame_cmn,
	 vcd_pause_in_run,
	 NULL,
	 vcd_flush_cmn,
	 vcd_stop_in_run,
	 vcd_set_property_cmn,
	 vcd_get_property_cmn,
	 vcd_set_buffer_requirements_cmn,
	 vcd_get_buffer_requirements_cmn,
	 vcd_set_buffer_cmn,
	 vcd_allocate_buffer_cmn,
	 vcd_free_buffer_cmn,
	 vcd_fill_output_buffer_cmn,
	 vcd_clnt_cb_in_run,
	 },
	vcd_clnt_enter_run,
	vcd_clnt_exit_run
};

static const struct vcd_clnt_state_table vcd_clnt_table_flushing = {
	{
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 vcd_flush_in_flushing,
	 NULL,
	 vcd_set_property_cmn,
	 vcd_get_property_cmn,
	 NULL,
	 vcd_get_buffer_requirements_cmn,
	 NULL,
	 NULL,
	 NULL,
	 vcd_fill_output_buffer_cmn,
	 vcd_clnt_cb_in_flushing,
	 },
	vcd_clnt_enter_flushing,
	vcd_clnt_exit_flushing
};

static const struct vcd_clnt_state_table vcd_clnt_table_stopping = {
	{
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 vcd_get_property_cmn,
	 NULL,
	 vcd_get_buffer_requirements_cmn,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 vcd_clnt_cb_in_stopping,
	 },
	vcd_clnt_enter_stopping,
	vcd_clnt_exit_stopping
};

static const struct vcd_clnt_state_table vcd_clnt_table_eos = {
	{
	 NULL,
	 NULL,
	 vcd_encode_frame_cmn,
	 NULL,
	 vcd_decode_frame_cmn,
	 NULL,
	 NULL,
	 vcd_flush_in_eos,
	 vcd_stop_in_eos,
	 NULL,
	 vcd_get_property_cmn,
	 NULL,
	 vcd_get_buffer_requirements_cmn,
	 NULL,
	 NULL,
	 NULL,
	 vcd_fill_output_buffer_in_eos,
	 vcd_clnt_cb_in_eos,
	 },
	vcd_clnt_enter_eos,
	vcd_clnt_exit_eos
};

static const struct vcd_clnt_state_table vcd_clnt_table_pausing = {
	{
	 NULL,
	 NULL,
	 vcd_encode_frame_cmn,
	 NULL,
	 vcd_decode_frame_cmn,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 vcd_set_property_cmn,
	 vcd_get_property_cmn,
	 NULL,
	 vcd_get_buffer_requirements_cmn,
	 NULL,
	 NULL,
	 NULL,
	 vcd_fill_output_buffer_cmn,
	 vcd_clnt_cb_in_pausing,
	 },
	vcd_clnt_enter_pausing,
	vcd_clnt_exit_pausing
};

static const struct vcd_clnt_state_table vcd_clnt_table_paused = {
	{
	 NULL,
	 NULL,
	 vcd_encode_frame_cmn,
	 NULL,
	 vcd_decode_frame_cmn,
	 NULL,
	 vcd_resume_in_paused,
	 vcd_flush_cmn,
	 vcd_stop_cmn,
	 vcd_set_property_cmn,
	 vcd_get_property_cmn,
	 vcd_set_buffer_requirements_cmn,
	 vcd_get_buffer_requirements_cmn,
	 vcd_set_buffer_cmn,
	 vcd_allocate_buffer_cmn,
	 NULL,
	 vcd_fill_output_buffer_cmn,
	 NULL,
	 },
	vcd_clnt_enter_paused,
	vcd_clnt_exit_paused
};
static const struct vcd_clnt_state_table vcd_clnt_table_invalid = {
   {
      vcd_close_in_invalid,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      vcd_flush_in_invalid,
      vcd_stop_in_invalid,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      vcd_free_buffer_cmn,
      NULL,
      vcd_clnt_cb_in_invalid,
   },
   vcd_clnt_enter_invalid,
   vcd_clnt_exit_invalid
};

static const struct vcd_clnt_state_table *vcd_clnt_state_table[] = {
	NULL,
	&vcd_clnt_table_open,
	&vcd_clnt_table_starting,
	&vcd_clnt_table_run,
	&vcd_clnt_table_flushing,
	&vcd_clnt_table_pausing,
	&vcd_clnt_table_paused,
	&vcd_clnt_table_stopping,
	&vcd_clnt_table_eos,
   &vcd_clnt_table_invalid
};
