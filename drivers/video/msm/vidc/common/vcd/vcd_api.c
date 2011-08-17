/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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

#include "vidc_type.h"
#include "vcd.h"

u32 vcd_init(struct vcd_init_config *config, s32 *driver_handle)
{
	u32 rc = VCD_S_SUCCESS;
	struct vcd_drv_ctxt *drv_ctxt;

	VCD_MSG_MED("vcd_init:");

	if (!config ||
	    !driver_handle || !config->map_dev_base_addr) {
		VCD_MSG_ERROR("Bad parameters");

		return VCD_ERR_ILLEGAL_PARM;
	}

	drv_ctxt = vcd_get_drv_context();
	mutex_init(&drv_ctxt->dev_mutex);
	mutex_lock(&drv_ctxt->dev_mutex);

	if (drv_ctxt->dev_state.state_table->ev_hdlr.init) {
		rc = drv_ctxt->dev_state.state_table->ev_hdlr.
		    init(drv_ctxt, config, driver_handle);
	} else {
		VCD_MSG_ERROR("Unsupported API in device state %d",
			      drv_ctxt->dev_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_init);

u32 vcd_term(s32 driver_handle)
{
	u32 rc = VCD_S_SUCCESS;
	struct vcd_drv_ctxt *drv_ctxt;

	VCD_MSG_MED("vcd_term:");

	drv_ctxt = vcd_get_drv_context();
	mutex_lock(&drv_ctxt->dev_mutex);

	if (drv_ctxt->dev_state.state_table->ev_hdlr.term) {
		rc = drv_ctxt->dev_state.state_table->ev_hdlr.
		    term(drv_ctxt, driver_handle);
	} else {
		VCD_MSG_ERROR("Unsupported API in device state %d",
			      drv_ctxt->dev_state.state);

		rc = VCD_ERR_BAD_STATE;
	}
	mutex_unlock(&drv_ctxt->dev_mutex);
	return rc;

}
EXPORT_SYMBOL(vcd_term);

u32 vcd_open(s32 driver_handle, u32 decoding,
	void (*callback) (u32 event, u32 status, void *info, size_t sz,
		       void *handle, void *const client_data),
	void *client_data)
{
	u32 rc = VCD_S_SUCCESS;
	struct vcd_drv_ctxt *drv_ctxt;

	VCD_MSG_MED("vcd_open:");

	if (!callback) {
		VCD_MSG_ERROR("Bad parameters");

		return VCD_ERR_ILLEGAL_PARM;
	}

	drv_ctxt = vcd_get_drv_context();
	mutex_lock(&drv_ctxt->dev_mutex);

	if (drv_ctxt->dev_state.state_table->ev_hdlr.open) {
		rc = drv_ctxt->dev_state.state_table->ev_hdlr.
		    open(drv_ctxt, driver_handle, decoding, callback,
			    client_data);
	} else {
		VCD_MSG_ERROR("Unsupported API in device state %d",
			      drv_ctxt->dev_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_open);

u32 vcd_close(void *handle)
{
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	struct vcd_drv_ctxt *drv_ctxt;
	u32 rc;

	VCD_MSG_MED("vcd_close:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	drv_ctxt = vcd_get_drv_context();
	mutex_lock(&drv_ctxt->dev_mutex);
	if (drv_ctxt->dev_state.state_table->ev_hdlr.close) {
		rc = drv_ctxt->dev_state.state_table->ev_hdlr.
		    close(drv_ctxt, cctxt);
	} else {
		VCD_MSG_ERROR("Unsupported API in device state %d",
			      drv_ctxt->dev_state.state);

		rc = VCD_ERR_BAD_STATE;
	}
	mutex_unlock(&drv_ctxt->dev_mutex);
	return rc;

}
EXPORT_SYMBOL(vcd_close);

u32 vcd_encode_start(void *handle)
{
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	struct vcd_drv_ctxt *drv_ctxt;
	u32 rc;

	VCD_MSG_MED("vcd_encode_start:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);

	if (cctxt->clnt_state.state_table->ev_hdlr.encode_start &&
	    drv_ctxt->dev_ctxt.pwr_state != VCD_PWR_STATE_SLEEP) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
		    encode_start(cctxt);
	} else {
		VCD_MSG_ERROR
		    ("Unsupported API in dev power state %d OR client state %d",
		     drv_ctxt->dev_ctxt.pwr_state,
		     cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_encode_start);

u32 vcd_encode_frame(void *handle, struct vcd_frame_data *input_frame)
{
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	struct vcd_drv_ctxt *drv_ctxt;
	u32 rc;

	VCD_MSG_MED("vcd_encode_frame:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	if (!input_frame) {
		VCD_MSG_ERROR("Bad parameters");

		return VCD_ERR_BAD_POINTER;
	}

	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);

	if (cctxt->clnt_state.state_table->ev_hdlr.encode_frame) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
		    encode_frame(cctxt, input_frame);
	} else {
		VCD_MSG_ERROR("Unsupported API in client state %d",
			      cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_encode_frame);

u32 vcd_decode_start(void *handle, struct vcd_sequence_hdr *seq_hdr)
{
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	struct vcd_drv_ctxt *drv_ctxt;
	u32 rc;

	VCD_MSG_MED("vcd_decode_start:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);

	if (cctxt->clnt_state.state_table->ev_hdlr.decode_start &&
	    drv_ctxt->dev_ctxt.pwr_state != VCD_PWR_STATE_SLEEP) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
		    decode_start(cctxt, seq_hdr);
	} else {
		VCD_MSG_ERROR
		    ("Unsupported API in dev power state %d OR client state %d",
		     drv_ctxt->dev_ctxt.pwr_state,
		     cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_decode_start);

u32 vcd_decode_frame(void *handle, struct vcd_frame_data *input_frame)
{
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	struct vcd_drv_ctxt *drv_ctxt;
	u32 rc;

	VCD_MSG_MED("vcd_decode_frame:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	if (!input_frame) {
		VCD_MSG_ERROR("Bad parameters");

		return VCD_ERR_BAD_POINTER;
	}

	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);

	if (cctxt->clnt_state.state_table->ev_hdlr.decode_frame) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
		    decode_frame(cctxt, input_frame);
	} else {
		VCD_MSG_ERROR("Unsupported API in client state %d",
			      cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_decode_frame);

u32 vcd_pause(void *handle)
{
	struct vcd_drv_ctxt *drv_ctxt;
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	u32 rc;

	VCD_MSG_MED("vcd_pause:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);

	if (cctxt->clnt_state.state_table->ev_hdlr.pause) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
		    pause(cctxt);
	} else {
		VCD_MSG_ERROR("Unsupported API in client state %d",
			      cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_pause);

u32 vcd_resume(void *handle)
{
	struct vcd_drv_ctxt *drv_ctxt;
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	u32 rc;

	VCD_MSG_MED("vcd_resume:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);

	if (drv_ctxt->dev_state.state_table->ev_hdlr.resume &&
	    drv_ctxt->dev_ctxt.pwr_state != VCD_PWR_STATE_SLEEP) {
		rc = drv_ctxt->dev_state.state_table->ev_hdlr.
		    resume(drv_ctxt, cctxt);
	} else {
		VCD_MSG_ERROR
		    ("Unsupported API in dev power state %d OR client state %d",
		     drv_ctxt->dev_ctxt.pwr_state,
		     cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_resume);

u32 vcd_flush(void *handle, u32 mode)
{
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	struct vcd_drv_ctxt *drv_ctxt;
	u32 rc;

	VCD_MSG_MED("vcd_flush:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);

	if (cctxt->clnt_state.state_table->ev_hdlr.flush) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
		    flush(cctxt, mode);
	} else {
		VCD_MSG_ERROR("Unsupported API in client state %d",
			      cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_flush);

u32 vcd_stop(void *handle)
{
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	struct vcd_drv_ctxt *drv_ctxt;
	u32 rc;

	VCD_MSG_MED("vcd_stop:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);

	if (cctxt->clnt_state.state_table->ev_hdlr.stop &&
	    drv_ctxt->dev_ctxt.pwr_state != VCD_PWR_STATE_SLEEP) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
		    stop(cctxt);
	} else {
		VCD_MSG_ERROR
		    ("Unsupported API in dev power state %d OR client state %d",
		     drv_ctxt->dev_ctxt.pwr_state,
		     cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_stop);

u32 vcd_set_property(void *handle,
     struct vcd_property_hdr *prop_hdr, void *prop_val)
{
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	struct vcd_drv_ctxt *drv_ctxt;
	u32 rc;

	VCD_MSG_MED("vcd_set_property:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	if (!prop_hdr || !prop_val) {
		VCD_MSG_ERROR("Bad parameters");

		return VCD_ERR_BAD_POINTER;
	}

	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);

	if (cctxt->clnt_state.state_table->ev_hdlr.set_property) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
		    set_property(cctxt, prop_hdr, prop_val);
	} else {
		VCD_MSG_ERROR("Unsupported API in client state %d",
			      cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_set_property);

u32 vcd_get_property(void *handle,
     struct vcd_property_hdr *prop_hdr, void *prop_val)
{
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	struct vcd_drv_ctxt *drv_ctxt;
	u32 rc;

	VCD_MSG_MED("vcd_get_property:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	if (!prop_hdr || !prop_val) {
		VCD_MSG_ERROR("Bad parameters");

		return VCD_ERR_BAD_POINTER;
	}

	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);

	if (cctxt->clnt_state.state_table->ev_hdlr.get_property) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
		    get_property(cctxt, prop_hdr, prop_val);
	} else {
		VCD_MSG_ERROR("Unsupported API in client state %d",
			      cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_get_property);

u32 vcd_set_buffer_requirements(void *handle,
     enum vcd_buffer_type buffer,
     struct vcd_buffer_requirement *buffer_req)
{
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	struct vcd_drv_ctxt *drv_ctxt;
	u32 rc;

	VCD_MSG_MED("vcd_set_buffer_requirements:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	if (!buffer_req) {
		VCD_MSG_ERROR("Bad parameters");

		return VCD_ERR_BAD_POINTER;
	}

	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);

	if (cctxt->clnt_state.state_table->ev_hdlr.
	    set_buffer_requirements) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
		    set_buffer_requirements(cctxt, buffer, buffer_req);
	} else {
		VCD_MSG_ERROR("Unsupported API in client state %d",
			      cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_set_buffer_requirements);

u32 vcd_get_buffer_requirements(void *handle,
     enum vcd_buffer_type buffer,
     struct vcd_buffer_requirement *buffer_req)
{
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	struct vcd_drv_ctxt *drv_ctxt;
	u32 rc;

	VCD_MSG_MED("vcd_get_buffer_requirements:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	if (!buffer_req) {
		VCD_MSG_ERROR("Bad parameters");

		return VCD_ERR_BAD_POINTER;
	}

	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);

	if (cctxt->clnt_state.state_table->ev_hdlr.
	    get_buffer_requirements) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
		    get_buffer_requirements(cctxt, buffer, buffer_req);
	} else {
		VCD_MSG_ERROR("Unsupported API in client state %d",
			      cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_get_buffer_requirements);

u32 vcd_set_buffer(void *handle,
     enum vcd_buffer_type buffer_type, u8 *buffer, u32 buf_size)
{
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	struct vcd_drv_ctxt *drv_ctxt;
	u32 rc;

	VCD_MSG_MED("vcd_set_buffer:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	if (!buffer || !buf_size) {
		VCD_MSG_ERROR("Bad parameters");

		return VCD_ERR_BAD_POINTER;
	}

	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);

	if (cctxt->clnt_state.state_table->ev_hdlr.set_buffer) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
		    set_buffer(cctxt, buffer_type, buffer, buf_size);
	} else {
		VCD_MSG_ERROR("Unsupported API in client state %d",
			      cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_set_buffer);

u32 vcd_allocate_buffer(void *handle,
     enum vcd_buffer_type buffer,
     u32 buf_size, u8 **vir_buf_addr, u8 **phy_buf_addr)
{
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	struct vcd_drv_ctxt *drv_ctxt;
	u32 rc;

	VCD_MSG_MED("vcd_allocate_buffer:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	if (!vir_buf_addr || !phy_buf_addr
	    || !buf_size) {
		VCD_MSG_ERROR("Bad parameters");

		return VCD_ERR_BAD_POINTER;
	}

	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);

	if (cctxt->clnt_state.state_table->ev_hdlr.allocate_buffer) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
		    allocate_buffer(cctxt, buffer, buf_size,
				       vir_buf_addr, phy_buf_addr);
	} else {
		VCD_MSG_ERROR("Unsupported API in client state %d",
			      cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_allocate_buffer);

u32 vcd_free_buffer(void *handle, enum vcd_buffer_type buffer_type, u8 *buffer)
{
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	struct vcd_drv_ctxt *drv_ctxt;
	u32 rc;

	VCD_MSG_MED("vcd_free_buffer:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);

	if (cctxt->clnt_state.state_table->ev_hdlr.free_buffer) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
		    free_buffer(cctxt, buffer_type, buffer);
	} else {
		VCD_MSG_ERROR("Unsupported API in client state %d",
			      cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_free_buffer);

u32 vcd_fill_output_buffer(void *handle, struct vcd_frame_data *buffer)
{
	struct vcd_clnt_ctxt *cctxt =
	    (struct vcd_clnt_ctxt *)handle;
	struct vcd_drv_ctxt *drv_ctxt;
	u32 rc;

	VCD_MSG_MED("vcd_fill_output_buffer:");

	if (!cctxt || cctxt->signature != VCD_SIGNATURE) {
		VCD_MSG_ERROR("Bad client handle");

		return VCD_ERR_BAD_HANDLE;
	}

	if (!buffer) {
		VCD_MSG_ERROR("Bad parameters");

		return VCD_ERR_BAD_POINTER;
	}

	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);

	if (cctxt->clnt_state.state_table->ev_hdlr.fill_output_buffer) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
		    fill_output_buffer(cctxt, buffer);
	} else {
		VCD_MSG_ERROR("Unsupported API in client state %d",
			      cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_fill_output_buffer);

u32 vcd_set_device_power(s32 driver_handle,
		enum vcd_power_state pwr_state)
{
	u32 rc = VCD_S_SUCCESS;
	struct vcd_drv_ctxt *drv_ctxt;

	VCD_MSG_MED("vcd_set_device_power:");

	drv_ctxt = vcd_get_drv_context();
	mutex_lock(&drv_ctxt->dev_mutex);

	if (drv_ctxt->dev_state.state_table->ev_hdlr.set_dev_pwr) {
		rc = drv_ctxt->dev_state.state_table->ev_hdlr.
		    set_dev_pwr(drv_ctxt, pwr_state);
	} else {
		VCD_MSG_ERROR("Unsupported API in device state %d",
			      drv_ctxt->dev_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	mutex_unlock(&drv_ctxt->dev_mutex);

	return rc;

}
EXPORT_SYMBOL(vcd_set_device_power);

void vcd_read_and_clear_interrupt(void)
{
   VCD_MSG_LOW("vcd_read_and_clear_interrupt:");
   ddl_read_and_clear_interrupt();
}


void vcd_response_handler(void)
{
	struct vcd_drv_ctxt *drv_ctxt;

	VCD_MSG_LOW("vcd_response_handler:");
  drv_ctxt = vcd_get_drv_context();

  mutex_lock(&drv_ctxt->dev_mutex);

	if (!ddl_process_core_response()) {
		VCD_MSG_HIGH
		    ("ddl_process_core_response indicated no further"
		     "processing");
    mutex_unlock(&drv_ctxt->dev_mutex);
		return;
	}

	if (drv_ctxt->dev_ctxt.command_continue)
		vcd_continue();
	mutex_unlock(&drv_ctxt->dev_mutex);
}
EXPORT_SYMBOL(vcd_response_handler);

u8 vcd_get_num_of_clients(void)
{
	struct vcd_drv_ctxt *drv_ctxt;
	struct vcd_clnt_ctxt *cctxt;
	u8 count = 0;

	VCD_MSG_LOW("vcd_get_num_of_clients:");
	drv_ctxt = vcd_get_drv_context();

	mutex_lock(&drv_ctxt->dev_mutex);
	cctxt = drv_ctxt->dev_ctxt.cctxt_list_head;
	while (cctxt) {
		count++;
		cctxt = cctxt->next;
	}
	mutex_unlock(&drv_ctxt->dev_mutex);
	return count;
}
EXPORT_SYMBOL(vcd_get_num_of_clients);




