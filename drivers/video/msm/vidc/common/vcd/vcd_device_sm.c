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

static const struct vcd_dev_state_table *vcd_dev_state_table[];
static const struct vcd_dev_state_table vcd_dev_table_null;

struct vcd_drv_ctxt *vcd_get_drv_context(void)
{
	static struct vcd_drv_ctxt drv_context = {
		{&vcd_dev_table_null, VCD_DEVICE_STATE_NULL},
		{0},
	};

	return &drv_context;

}

void vcd_do_device_state_transition(struct vcd_drv_ctxt *drv_ctxt,
	 enum vcd_dev_state_enum to_state, u32 ev_code)
{
	struct vcd_dev_state_ctxt *state_ctxt;

	if (!drv_ctxt || to_state >= VCD_DEVICE_STATE_MAX) {
		VCD_MSG_ERROR("Bad parameters. drv_ctxt=%p, to_state=%d",
				  drv_ctxt, to_state);
	}

	state_ctxt = &drv_ctxt->dev_state;

	if (state_ctxt->state == to_state) {
		VCD_MSG_HIGH("Device already in requested to_state=%d",
				 to_state);

		return;
	}

	VCD_MSG_MED("vcd_do_device_state_transition: D%d -> D%d, for api %d",
			(int)state_ctxt->state, (int)to_state, ev_code);

	if (state_ctxt->state_table->exit)
		state_ctxt->state_table->exit(drv_ctxt, ev_code);


	state_ctxt->state = to_state;
	state_ctxt->state_table = vcd_dev_state_table[to_state];

	if (state_ctxt->state_table->entry)
		state_ctxt->state_table->entry(drv_ctxt, ev_code);
}

void vcd_hw_timeout_handler(void *user_data)
{
	struct vcd_drv_ctxt *drv_ctxt;

	VCD_MSG_HIGH("vcd_hw_timeout_handler:");
	user_data = NULL;
	drv_ctxt = vcd_get_drv_context();
	mutex_lock(&drv_ctxt->dev_mutex);
	if (drv_ctxt->dev_state.state_table->ev_hdlr.timeout)
		drv_ctxt->dev_state.state_table->ev_hdlr.
			timeout(drv_ctxt, user_data);
	else
		VCD_MSG_ERROR("hw_timeout unsupported in device state %d",
			drv_ctxt->dev_state.state);
	mutex_unlock(&drv_ctxt->dev_mutex);
}

void vcd_ddl_callback(u32 event, u32 status, void *payload,
	size_t sz, u32 *ddl_handle, void *const client_data)
{
	struct vcd_drv_ctxt *drv_ctxt;
	struct vcd_dev_ctxt *dev_ctxt;
	struct vcd_dev_state_ctxt *dev_state;
	struct vcd_clnt_ctxt *cctxt;
	struct vcd_transc *transc;

	VCD_MSG_LOW("vcd_ddl_callback:");

	VCD_MSG_LOW("event=0x%x status=0x%x", event, status);

	drv_ctxt = vcd_get_drv_context();
	dev_ctxt = &drv_ctxt->dev_ctxt;
	dev_state = &drv_ctxt->dev_state;

	dev_ctxt->command_continue = true;
	vcd_device_timer_stop(dev_ctxt);

	switch (dev_state->state) {
	case VCD_DEVICE_STATE_NULL:
		{
			VCD_MSG_HIGH("Callback unexpected in NULL state");
			break;
		}

	case VCD_DEVICE_STATE_NOT_INIT:
		{
			VCD_MSG_HIGH("Callback unexpected in NOT_INIT state");
			break;
		}

	case VCD_DEVICE_STATE_INITING:
		{
			if (dev_state->state_table->ev_hdlr.dev_cb) {
				dev_state->state_table->ev_hdlr.
					dev_cb(drv_ctxt, event, status,
						  payload, sz, ddl_handle,
						  client_data);
			} else {
				VCD_MSG_HIGH("No device handler in %d state",
						 dev_state->state);
			}
			break;
		}

	case VCD_DEVICE_STATE_READY:
		{
			transc = (struct vcd_transc *)client_data;

			if (!transc || !transc->in_use || !transc->cctxt) {
				VCD_MSG_ERROR("Invalid clientdata "
					"received from DDL, transc = 0x%x\n",
					(u32)transc);
				if (transc) {
					VCD_MSG_ERROR("transc->in_use = %u, "
						"transc->cctxt = 0x%x\n",
						transc->in_use,
						(u32)transc->cctxt);
				}
			} else {
				cctxt = transc->cctxt;

				if (cctxt->clnt_state.state_table->ev_hdlr.
					clnt_cb) {
					cctxt->clnt_state.state_table->
						ev_hdlr.clnt_cb(cctxt,
						event, status, payload,
						sz,	ddl_handle,
						client_data);
				} else {
					VCD_MSG_HIGH
					("No client handler in"
					" (dsm:READY, csm:%d) state",
					(int)cctxt->clnt_state.state);

					if (VCD_FAILED(status)) {
						VCD_MSG_FATAL("DDL callback"
						" returned failure 0x%x",
						status);
					}
				}
			}
			break;
		}

	default:
		{
			VCD_MSG_ERROR("Unknown state");
			break;
		}

	}

}

u32 vcd_init_device_context(struct vcd_drv_ctxt *drv_ctxt,
		u32 ev_code)
{
	struct vcd_dev_ctxt *dev_ctxt = &drv_ctxt->dev_ctxt;
	u32 rc;
	struct ddl_init_config ddl_init;

	VCD_MSG_LOW("vcd_init_device_context:");

	dev_ctxt->pending_cmd = VCD_CMD_NONE;

	rc = vcd_power_event(dev_ctxt, NULL, VCD_EVT_PWR_DEV_INIT_BEGIN);
	VCD_FAILED_RETURN(rc, "VCD_EVT_PWR_DEV_INIT_BEGIN failed");

	VCD_MSG_HIGH("Device powered ON and clocked");
	rc = vcd_sched_create(&dev_ctxt->sched_clnt_list);
	if (VCD_FAILED(rc)) {
		VCD_MSG_ERROR("rc = 0x%x. Failed: vcd_sched_create", rc);

		(void)vcd_power_event(dev_ctxt, NULL,
					  VCD_EVT_PWR_DEV_INIT_FAIL);

		return rc;
	}

	VCD_MSG_HIGH("Created scheduler instance.");

	ddl_init.core_virtual_base_addr = dev_ctxt->device_base_addr;
	ddl_init.interrupt_clr = dev_ctxt->config.interrupt_clr;
	ddl_init.ddl_callback = vcd_ddl_callback;

	rc = ddl_device_init(&ddl_init, NULL);

	if (VCD_FAILED(rc)) {
		VCD_MSG_ERROR("rc = 0x%x. Failed: ddl_device_init", rc);
		vcd_sched_destroy(&dev_ctxt->sched_clnt_list);
		(void)vcd_power_event(dev_ctxt, NULL,
					  VCD_EVT_PWR_DEV_INIT_FAIL);
	} else {
		vcd_device_timer_start(dev_ctxt);
		vcd_do_device_state_transition(drv_ctxt,
						   VCD_DEVICE_STATE_INITING,
						   ev_code);
	}
	dev_ctxt->turbo_mode_set = 0;

	return rc;
}

void vcd_handle_device_init_failed(struct vcd_drv_ctxt *drv_ctxt,
		u32 status)
{
	struct vcd_clnt_ctxt *client;
	struct vcd_clnt_ctxt *tmp_client;

	VCD_MSG_ERROR("Device init failed. status = %d", status);

	client = drv_ctxt->dev_ctxt.cctxt_list_head;
	while (client) {
		client->callback(VCD_EVT_RESP_OPEN,
				   status, NULL, 0, 0, client->client_data);

		tmp_client = client;
		client = client->next;

		vcd_destroy_client_context(tmp_client);
	}
	if (ddl_device_release(NULL))
		VCD_MSG_ERROR("Failed: ddl_device_release");

	vcd_sched_destroy(&drv_ctxt->dev_ctxt.sched_clnt_list);
	if (vcd_power_event(&drv_ctxt->dev_ctxt,
		NULL, VCD_EVT_PWR_DEV_INIT_FAIL))
		VCD_MSG_ERROR("VCD_EVT_PWR_DEV_INIT_FAIL failed");

	vcd_do_device_state_transition(drv_ctxt,
		VCD_DEVICE_STATE_NOT_INIT,
		DEVICE_STATE_EVENT_NUMBER(dev_cb));
}

u32 vcd_deinit_device_context(struct vcd_drv_ctxt *drv_ctxt,
		u32 ev_code)
{
	struct vcd_dev_ctxt *dev_ctxt = &drv_ctxt->dev_ctxt;
	u32 rc = VCD_S_SUCCESS;

	VCD_MSG_LOW("vcd_deinit_device_context:");

	rc = vcd_power_event(&drv_ctxt->dev_ctxt, NULL,
				 VCD_EVT_PWR_DEV_TERM_BEGIN);

	VCD_FAILED_RETURN(rc, "VCD_EVT_PWR_DEV_TERM_BEGIN failed");

	rc = ddl_device_release(NULL);

	if (VCD_FAILED(rc)) {
		VCD_MSG_ERROR("rc = 0x%x. Failed: ddl_device_release", rc);

		(void)vcd_power_event(dev_ctxt, NULL,
					  VCD_EVT_PWR_DEV_TERM_FAIL);
	} else {
		vcd_sched_destroy(&dev_ctxt->sched_clnt_list);
		(void) vcd_power_event(dev_ctxt, NULL,
			VCD_EVT_PWR_DEV_TERM_END);

		vcd_do_device_state_transition(drv_ctxt,
			VCD_DEVICE_STATE_NOT_INIT, ev_code);
	}
	return rc;
}

void vcd_term_driver_context(struct vcd_drv_ctxt *drv_ctxt)
{
	struct vcd_dev_ctxt *dev_ctxt = &drv_ctxt->dev_ctxt;

	VCD_MSG_HIGH("All driver instances terminated");

	if (dev_ctxt->config.deregister_isr)
		dev_ctxt->config.deregister_isr();

	if (dev_ctxt->config.un_map_dev_base_addr)
		dev_ctxt->config.un_map_dev_base_addr();

	if (dev_ctxt->config.timer_release)
		dev_ctxt->config.timer_release(
			dev_ctxt->hw_timer_handle);

	kfree(dev_ctxt->trans_tbl);

	memset(dev_ctxt, 0, sizeof(struct vcd_dev_ctxt));

	vcd_do_device_state_transition(drv_ctxt,
					   VCD_DEVICE_STATE_NULL,
					   DEVICE_STATE_EVENT_NUMBER(term));

}

u32 vcd_reset_device_context(struct vcd_drv_ctxt *drv_ctxt,
	u32 ev_code)
{
	struct vcd_dev_ctxt *dev_ctxt = &drv_ctxt->dev_ctxt;
	u32 rc = VCD_S_SUCCESS;

	VCD_MSG_LOW("vcd_reset_device_context:");
	vcd_reset_device_channels(dev_ctxt);
	rc = vcd_power_event(&drv_ctxt->dev_ctxt, NULL,
						 VCD_EVT_PWR_DEV_TERM_BEGIN);
	VCD_FAILED_RETURN(rc, "VCD_EVT_PWR_DEV_TERM_BEGIN failed");
	if (ddl_reset_hw(0))
		VCD_MSG_HIGH("HW Reset done");
	else
		VCD_MSG_FATAL("HW Reset failed");

	(void)vcd_power_event(dev_ctxt, NULL, VCD_EVT_PWR_DEV_TERM_END);

	return VCD_S_SUCCESS;
}

void vcd_handle_device_err_fatal(struct vcd_dev_ctxt *dev_ctxt,
	struct vcd_clnt_ctxt *trig_clnt)
{
	struct vcd_clnt_ctxt *cctxt = dev_ctxt->cctxt_list_head;
	struct vcd_clnt_ctxt *tmp_clnt = NULL;
	VCD_MSG_LOW("vcd_handle_device_err_fatal:");
	while (cctxt) {
		tmp_clnt = cctxt;
		cctxt = cctxt->next;
		if (tmp_clnt != trig_clnt)
			vcd_clnt_handle_device_err_fatal(tmp_clnt,
				tmp_clnt->status.last_evt);
	}
	dev_ctxt->pending_cmd = VCD_CMD_DEVICE_RESET;
	if (!dev_ctxt->cctxt_list_head)
		vcd_do_device_state_transition(vcd_get_drv_context(),
			VCD_DEVICE_STATE_NOT_INIT,
			DEVICE_STATE_EVENT_NUMBER(timeout));
	else
		vcd_do_device_state_transition(vcd_get_drv_context(),
			VCD_DEVICE_STATE_INVALID,
			DEVICE_STATE_EVENT_NUMBER(dev_cb));
}

void vcd_handle_for_last_clnt_close(
	struct vcd_dev_ctxt *dev_ctxt, u32 send_deinit)
{
	if (!dev_ctxt->cctxt_list_head) {
		VCD_MSG_HIGH("All clients are closed");
		if (send_deinit)
			(void) vcd_deinit_device_context(
				vcd_get_drv_context(),
				DEVICE_STATE_EVENT_NUMBER(close));
		else
			dev_ctxt->pending_cmd =
			VCD_CMD_DEVICE_TERM;
	}
}
void vcd_continue(void)
{
	struct vcd_drv_ctxt *drv_ctxt;
	struct vcd_dev_ctxt *dev_ctxt;
	u32 command_continue;
	struct vcd_transc *transc;
	u32 rc;
	VCD_MSG_LOW("vcd_continue:");

	drv_ctxt = vcd_get_drv_context();
	dev_ctxt = &drv_ctxt->dev_ctxt;

	dev_ctxt->command_continue = false;

	if (dev_ctxt->pending_cmd == VCD_CMD_DEVICE_INIT) {
		VCD_MSG_HIGH("VCD_CMD_DEVICE_INIT is pending");

		dev_ctxt->pending_cmd = VCD_CMD_NONE;

		(void)vcd_init_device_context(drv_ctxt,
			DEVICE_STATE_EVENT_NUMBER(open));
	} else if (dev_ctxt->pending_cmd == VCD_CMD_DEVICE_TERM) {
		VCD_MSG_HIGH("VCD_CMD_DEVICE_TERM is pending");

		dev_ctxt->pending_cmd = VCD_CMD_NONE;

		(void)vcd_deinit_device_context(drv_ctxt,
			DEVICE_STATE_EVENT_NUMBER(close));
	} else if (dev_ctxt->pending_cmd == VCD_CMD_DEVICE_RESET) {
		VCD_MSG_HIGH("VCD_CMD_DEVICE_RESET is pending");
		dev_ctxt->pending_cmd = VCD_CMD_NONE;
		(void)vcd_reset_device_context(drv_ctxt,
			DEVICE_STATE_EVENT_NUMBER(dev_cb));
	} else {
		if (dev_ctxt->set_perf_lvl_pending) {
			rc = vcd_power_event(dev_ctxt, NULL,
						 VCD_EVT_PWR_DEV_SET_PERFLVL);

			if (VCD_FAILED(rc)) {
				VCD_MSG_ERROR
					("VCD_EVT_PWR_CLNT_SET_PERFLVL failed");
				VCD_MSG_HIGH
					("Not running at desired perf level."
					 "curr=%d, reqd=%d",
					 dev_ctxt->curr_perf_lvl,
					 dev_ctxt->reqd_perf_lvl);
			} else {
				dev_ctxt->set_perf_lvl_pending = false;
			}
		}

		do {
			command_continue = false;

			if (vcd_get_command_channel_in_loop
				(dev_ctxt, &transc)) {
				if (vcd_submit_command_in_continue(dev_ctxt,
					transc))
					command_continue = true;
				else {
					VCD_MSG_MED
						("No more commands to submit");

					vcd_release_command_channel(dev_ctxt,
						transc);

					vcd_release_interim_command_channels
						(dev_ctxt);
				}
			}
		} while (command_continue);

		do {
			command_continue = false;

			if (vcd_get_frame_channel_in_loop
				(dev_ctxt, &transc)) {
				if (vcd_try_submit_frame_in_continue(dev_ctxt,
					transc)) {
					command_continue = true;
				} else {
					VCD_MSG_MED("No more frames to submit");

					vcd_release_frame_channel(dev_ctxt,
								  transc);

					vcd_release_interim_frame_channels
						(dev_ctxt);
				}
			}

		} while (command_continue);

		if (!vcd_core_is_busy(dev_ctxt)) {
			rc = vcd_power_event(dev_ctxt, NULL,
				VCD_EVT_PWR_CLNT_CMD_END);

			if (VCD_FAILED(rc))
				VCD_MSG_ERROR("Failed:"
					"VCD_EVT_PWR_CLNT_CMD_END");
		}
	}
}

static void vcd_pause_all_sessions(struct vcd_dev_ctxt *dev_ctxt)
{
	struct vcd_clnt_ctxt *cctxt = dev_ctxt->cctxt_list_head;
	u32 rc;

	while (cctxt) {
		if (cctxt->clnt_state.state_table->ev_hdlr.pause) {
			rc = cctxt->clnt_state.state_table->ev_hdlr.
				pause(cctxt);

			if (VCD_FAILED(rc))
				VCD_MSG_ERROR("Client pause failed");

		}

		cctxt = cctxt->next;
	}
}

static void vcd_resume_all_sessions(struct vcd_dev_ctxt *dev_ctxt)
{
	struct vcd_clnt_ctxt *cctxt = dev_ctxt->cctxt_list_head;
	u32 rc;

	while (cctxt) {
		if (cctxt->clnt_state.state_table->ev_hdlr.resume) {
			rc = cctxt->clnt_state.state_table->ev_hdlr.
				resume(cctxt);

			if (VCD_FAILED(rc))
				VCD_MSG_ERROR("Client resume failed");

		}

		cctxt = cctxt->next;
	}
}

static u32 vcd_init_cmn
	(struct vcd_drv_ctxt *drv_ctxt,
	 struct vcd_init_config *config, s32 *driver_handle)
{
	struct vcd_dev_ctxt *dev_ctxt = &drv_ctxt->dev_ctxt;
	s32 driver_id;

	if (dev_ctxt->config.interrupt_clr !=
		config->interrupt_clr
		|| dev_ctxt->config.register_isr !=
		config->register_isr
		|| dev_ctxt->config.deregister_isr !=
		config->deregister_isr
		|| dev_ctxt->config.map_dev_base_addr !=
		config->map_dev_base_addr
		|| dev_ctxt->config.un_map_dev_base_addr !=
		config->un_map_dev_base_addr) {
		VCD_MSG_HIGH("Device config mismatch. "
			"VCD will be using config from 1st vcd_init");
	}

	*driver_handle = 0;

	driver_id = 0;
	while (driver_id < VCD_DRIVER_INSTANCE_MAX &&
		   dev_ctxt->driver_ids[driver_id]) {
		++driver_id;
	}

	if (driver_id == VCD_DRIVER_INSTANCE_MAX) {
		VCD_MSG_ERROR("Max driver instances reached");

		return VCD_ERR_FAIL;
	}

	++dev_ctxt->refs;
	dev_ctxt->driver_ids[driver_id] = true;
	*driver_handle = driver_id + 1;

	VCD_MSG_HIGH("Driver_id = %d. No of driver instances = %d",
			 driver_id, dev_ctxt->refs);

	return VCD_S_SUCCESS;

}

static u32 vcd_init_in_null
	(struct vcd_drv_ctxt *drv_ctxt,
	 struct vcd_init_config *config, s32 *driver_handle) {
	u32 rc = VCD_S_SUCCESS;
	struct vcd_dev_ctxt *dev_ctxt = &drv_ctxt->dev_ctxt;
	u32 done_create_timer = false;
	VCD_MSG_LOW("vcd_init_in_dev_null:");


	dev_ctxt->config = *config;

	dev_ctxt->device_base_addr =
		(u8 *)config->map_dev_base_addr(
			dev_ctxt->config.device_name);

	if (!dev_ctxt->device_base_addr) {
		VCD_MSG_ERROR("NULL Device_base_addr");

		return VCD_ERR_FAIL;
	}

	if (config->register_isr) {
		config->register_isr(dev_ctxt->config.
			device_name);
	}

	if (config->timer_create) {
		if (config->timer_create(vcd_hw_timeout_handler,
			NULL, &dev_ctxt->hw_timer_handle))
			done_create_timer = true;
		else {
			VCD_MSG_ERROR("timercreate failed");
			return VCD_ERR_FAIL;
		}
	}


	rc = vcd_init_cmn(drv_ctxt, config, driver_handle);

	if (!VCD_FAILED(rc)) {
		vcd_do_device_state_transition(drv_ctxt,
						   VCD_DEVICE_STATE_NOT_INIT,
						   DEVICE_STATE_EVENT_NUMBER
						   (init));
	} else {
		if (dev_ctxt->config.un_map_dev_base_addr)
			dev_ctxt->config.un_map_dev_base_addr();

		if (dev_ctxt->config.deregister_isr)
			dev_ctxt->config.deregister_isr();

		if (done_create_timer && dev_ctxt->config.timer_release)
			dev_ctxt->config.timer_release(dev_ctxt->
				hw_timer_handle);

	}

	return rc;

}

static u32 vcd_init_in_not_init
	(struct vcd_drv_ctxt *drv_ctxt,
	 struct vcd_init_config *config, s32 *driver_handle)
{

	VCD_MSG_LOW("vcd_init_in_dev_not_init:");

	return vcd_init_cmn(drv_ctxt, config, driver_handle);

}

static u32 vcd_init_in_initing
	(struct vcd_drv_ctxt *drv_ctxt,
	 struct vcd_init_config *config, s32 *driver_handle) {

	VCD_MSG_LOW("vcd_init_in_dev_initing:");

	return vcd_init_cmn(drv_ctxt, config, driver_handle);

}

static u32 vcd_init_in_ready
	(struct vcd_drv_ctxt *drv_ctxt,
	 struct vcd_init_config *config, s32 *driver_handle)
{
	VCD_MSG_LOW("vcd_init_in_dev_ready:");

	return vcd_init_cmn(drv_ctxt, config, driver_handle);
}

static u32 vcd_term_cmn
	(struct vcd_drv_ctxt *drv_ctxt, s32 driver_handle)
{
	struct vcd_dev_ctxt *dev_ctxt = &drv_ctxt->dev_ctxt;

	if (!vcd_validate_driver_handle(dev_ctxt, driver_handle)) {
		VCD_MSG_ERROR("Invalid driver handle = %d", driver_handle);

		return VCD_ERR_BAD_HANDLE;
	}

	if (vcd_check_for_client_context(dev_ctxt,
				driver_handle - 1)) {
		VCD_MSG_ERROR("Driver has active client");

		return VCD_ERR_BAD_STATE;
	}

	--dev_ctxt->refs;
	dev_ctxt->driver_ids[driver_handle - 1] = false;

	VCD_MSG_HIGH("Driver_id %d terminated. No of driver instances = %d",
			 driver_handle - 1, dev_ctxt->refs);

	return VCD_S_SUCCESS;
}

static u32 vcd_term_in_not_init
	(struct vcd_drv_ctxt *drv_ctxt, s32 driver_handle)
{
	struct vcd_dev_ctxt *dev_ctxt = &drv_ctxt->dev_ctxt;
	u32 rc;

	VCD_MSG_LOW("vcd_term_in_dev_not_init:");

	rc = vcd_term_cmn(drv_ctxt, driver_handle);

	if (!VCD_FAILED(rc) && !dev_ctxt->refs)
		vcd_term_driver_context(drv_ctxt);

	return rc;
}

static u32 vcd_term_in_initing
	(struct vcd_drv_ctxt *drv_ctxt, s32 driver_handle)
{
	VCD_MSG_LOW("vcd_term_in_dev_initing:");

	return vcd_term_cmn(drv_ctxt, driver_handle);
}

static u32 vcd_term_in_ready
	(struct vcd_drv_ctxt *drv_ctxt, s32 driver_handle)
{
	VCD_MSG_LOW("vcd_term_in_dev_ready:");

	return vcd_term_cmn(drv_ctxt, driver_handle);
}

static u32  vcd_term_in_invalid(struct vcd_drv_ctxt *drv_ctxt,
							 s32  driver_handle)
{
	u32 rc;
	VCD_MSG_LOW("vcd_term_in_invalid:");
	rc = vcd_term_cmn(drv_ctxt, driver_handle);
	if (!VCD_FAILED(rc) && !drv_ctxt->dev_ctxt.refs)
		vcd_term_driver_context(drv_ctxt);

	return rc;
}

static u32 vcd_open_cmn
	(struct vcd_drv_ctxt *drv_ctxt,
	 s32 driver_handle,
	 u32 decoding,
	 void (*callback) (u32 event, u32 status, void *info, size_t sz,
			   void *handle, void *const client_data),
	 void *client_data, struct vcd_clnt_ctxt ** clnt_cctxt)
{
	struct vcd_dev_ctxt *dev_ctxt = &drv_ctxt->dev_ctxt;
	struct vcd_clnt_ctxt *cctxt;
	struct vcd_clnt_ctxt *client;

	if (!vcd_validate_driver_handle(dev_ctxt, driver_handle)) {
		VCD_MSG_ERROR("Invalid driver handle = %d", driver_handle);

		return VCD_ERR_BAD_HANDLE;
	}

	cctxt =	(struct vcd_clnt_ctxt *)
		kmalloc(sizeof(struct vcd_clnt_ctxt), GFP_KERNEL);
	if (!cctxt) {
		VCD_MSG_ERROR("No memory for client ctxt");

		return VCD_ERR_ALLOC_FAIL;
	}

	memset(cctxt, 0, sizeof(struct vcd_clnt_ctxt));
	cctxt->dev_ctxt = dev_ctxt;
	cctxt->driver_id = driver_handle - 1;
	cctxt->decoding = decoding;
	cctxt->callback = callback;
	cctxt->client_data = client_data;
	cctxt->status.last_evt = VCD_EVT_RESP_OPEN;
	INIT_LIST_HEAD(&cctxt->in_buf_pool.queue);
	INIT_LIST_HEAD(&cctxt->out_buf_pool.queue);
	client = dev_ctxt->cctxt_list_head;
	dev_ctxt->cctxt_list_head = cctxt;
	cctxt->next = client;
	dev_ctxt->turbo_mode_set = 0;

	*clnt_cctxt = cctxt;

	return VCD_S_SUCCESS;

}

static u32 vcd_open_in_not_init
	(struct vcd_drv_ctxt *drv_ctxt,
	 s32 driver_handle,
	 u32 decoding,
	 void (*callback) (u32 event, u32 status, void *info, size_t sz,
			   void *handle, void *const client_data),
	 void *client_data)
{
	struct vcd_clnt_ctxt *cctxt;
	u32 rc;

	VCD_MSG_LOW("vcd_open_in_dev_not_init:");

	rc = vcd_open_cmn(drv_ctxt, driver_handle, decoding, callback,
			  client_data, &cctxt);

	VCD_FAILED_RETURN(rc, "Failed: vcd_open_cmn");

	rc = vcd_init_device_context(drv_ctxt,
					 DEVICE_STATE_EVENT_NUMBER(open));

	if (VCD_FAILED(rc))
		vcd_destroy_client_context(cctxt);

	return rc;
}

static u32 vcd_open_in_initing(struct vcd_drv_ctxt *drv_ctxt,
	 s32 driver_handle, u32 decoding,
	 void (*callback) (u32 event, u32 status, void *info, size_t sz,
			   void *handle, void *const client_data),
	 void *client_data)
{
	struct vcd_clnt_ctxt *cctxt;

	VCD_MSG_LOW("vcd_open_in_dev_initing:");

	return vcd_open_cmn(drv_ctxt, driver_handle, decoding, callback,
				 client_data, &cctxt);
}

static u32 vcd_open_in_ready
	(struct vcd_drv_ctxt *drv_ctxt,
	 s32 driver_handle,
	 u32 decoding,
	 void (*callback) (u32 event, u32 status, void *info, size_t sz,
			   void *handle, void *const client_data),
	 void *client_data)
{
	struct vcd_clnt_ctxt *cctxt;
	struct vcd_handle_container container;
	u32 rc;

	VCD_MSG_LOW("vcd_open_in_dev_ready:");

	rc = vcd_open_cmn(drv_ctxt, driver_handle, decoding, callback,
			  client_data, &cctxt);

	VCD_FAILED_RETURN(rc, "Failed: vcd_open_cmn");

	rc = vcd_init_client_context(cctxt);

	if (!VCD_FAILED(rc)) {
		container.handle = (void *)cctxt;

		callback(VCD_EVT_RESP_OPEN,
			 VCD_S_SUCCESS,
			 &container,
			 sizeof(container), container.handle, client_data);
	} else {
		VCD_MSG_ERROR("rc = 0x%x. Failed: vcd_init_client_context", rc);

		vcd_destroy_client_context(cctxt);
	}

	return rc;
}

static u32 vcd_close_in_ready
	(struct vcd_drv_ctxt *drv_ctxt,
	 struct vcd_clnt_ctxt *cctxt) {
	u32 rc;

	VCD_MSG_LOW("vcd_close_in_dev_ready:");

	if (cctxt->clnt_state.state_table->ev_hdlr.close) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
			close(cctxt);
	} else {
		VCD_MSG_ERROR("Unsupported API in client state %d",
				  cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	if (!VCD_FAILED(rc))
		vcd_handle_for_last_clnt_close(&drv_ctxt->dev_ctxt, true);

	return rc;
}

static u32  vcd_close_in_dev_invalid(struct vcd_drv_ctxt *drv_ctxt,
	struct vcd_clnt_ctxt *cctxt)
{
	u32 rc;
	VCD_MSG_LOW("vcd_close_in_dev_invalid:");
	if (cctxt->clnt_state.state_table->ev_hdlr.close) {
		rc = cctxt->clnt_state.state_table->
			ev_hdlr.close(cctxt);
	} else {
		VCD_MSG_ERROR("Unsupported API in client state %d",
					  cctxt->clnt_state.state);
		rc = VCD_ERR_BAD_STATE;
	}
	if (!VCD_FAILED(rc) && !drv_ctxt->dev_ctxt.
		cctxt_list_head) {
		VCD_MSG_HIGH("All INVALID clients are closed");
		vcd_do_device_state_transition(drv_ctxt,
			VCD_DEVICE_STATE_NOT_INIT,
			DEVICE_STATE_EVENT_NUMBER(close));
	}
	return rc;
}

static u32 vcd_resume_in_ready
	(struct vcd_drv_ctxt *drv_ctxt,
	 struct vcd_clnt_ctxt *cctxt) {
	u32 rc = VCD_S_SUCCESS;

	VCD_MSG_LOW("vcd_resume_in_ready:");

	if (cctxt->clnt_state.state_table->ev_hdlr.resume) {
		rc = cctxt->clnt_state.state_table->ev_hdlr.
			resume(cctxt);
	} else {
		VCD_MSG_ERROR("Unsupported API in client state %d",
				  cctxt->clnt_state.state);

		rc = VCD_ERR_BAD_STATE;
	}

	return rc;
}

static u32 vcd_set_dev_pwr_in_ready
	(struct vcd_drv_ctxt *drv_ctxt,
	 enum vcd_power_state pwr_state)
{
	u32 rc = VCD_S_SUCCESS;
	struct vcd_dev_ctxt *dev_ctxt = &drv_ctxt->dev_ctxt;

	VCD_MSG_LOW("vcd_set_dev_pwr_in_ready:");

	switch (pwr_state) {
	case VCD_PWR_STATE_SLEEP:
		{
			if (dev_ctxt->pwr_state == VCD_PWR_STATE_ON)
				vcd_pause_all_sessions(dev_ctxt);
			dev_ctxt->pwr_state = VCD_PWR_STATE_SLEEP;
			break;
		}

	case VCD_PWR_STATE_ON:
		{
			if (dev_ctxt->pwr_state == VCD_PWR_STATE_SLEEP)
				vcd_resume_all_sessions(dev_ctxt);
			dev_ctxt->pwr_state = VCD_PWR_STATE_ON;
			break;
		}

	default:
		{
			VCD_MSG_ERROR("Invalid power state requested %d",
					  pwr_state);
			break;
		}

	}

	return rc;
}

static void vcd_dev_cb_in_initing
	(struct vcd_drv_ctxt *drv_ctxt,
	 u32 event,
	 u32 status,
	 void *payload, size_t sz, u32 *ddl_handle, void *const client_data)
{
	struct vcd_dev_ctxt *dev_ctxt;
	struct vcd_clnt_ctxt *client;
	struct vcd_clnt_ctxt *tmp_client;
	struct vcd_handle_container container;
	u32 rc = VCD_S_SUCCESS;
	u32 client_inited = false;
	u32 fail_all_open = false;
	struct ddl_context *ddl_context;

	ddl_context = ddl_get_context();

	VCD_MSG_LOW("vcd_dev_cb_in_initing:");

	if (event != VCD_EVT_RESP_DEVICE_INIT) {
		VCD_MSG_ERROR("vcd_dev_cb_in_initing: Unexpected event %d",
				  (int)event);
		return;
	}

	dev_ctxt = &drv_ctxt->dev_ctxt;

	dev_ctxt->command_continue = false;

	if (VCD_FAILED(status)) {
		vcd_handle_device_init_failed(drv_ctxt, status);

		return;
	}

	vcd_do_device_state_transition(drv_ctxt,
					   VCD_DEVICE_STATE_READY,
					   DEVICE_STATE_EVENT_NUMBER(open));

	if (!dev_ctxt->cctxt_list_head) {
		VCD_MSG_HIGH("All clients are closed");

		dev_ctxt->pending_cmd = VCD_CMD_DEVICE_TERM;

		return;
	}

	if (!dev_ctxt->ddl_cmd_ch_depth
		|| !dev_ctxt->trans_tbl)
		rc = vcd_setup_with_ddl_capabilities(dev_ctxt);


	if (VCD_FAILED(rc)) {
		VCD_MSG_ERROR
			("rc = 0x%x: Failed vcd_setup_with_ddl_capabilities",
			 rc);

		fail_all_open = true;
	}

	client = dev_ctxt->cctxt_list_head;
	while (client) {
		if (!fail_all_open)
			rc = vcd_init_client_context(client);


		if (!VCD_FAILED(rc)) {
			container.handle = (void *)client;
			client->callback(VCD_EVT_RESP_OPEN,
					   VCD_S_SUCCESS,
					   &container,
					   sizeof(container),
					   container.handle,
					   client->client_data);

			client = client->next;

			client_inited = true;
		} else {
			VCD_MSG_ERROR
				("rc = 0x%x, Failed: vcd_init_client_context",
				 rc);

			client->callback(VCD_EVT_RESP_OPEN,
					   rc,
					   NULL, 0, 0, client->client_data);

			tmp_client = client;
			client = client->next;
			if (tmp_client == dev_ctxt->cctxt_list_head)
				fail_all_open = true;

			vcd_destroy_client_context(tmp_client);
		}
	}

	if (!client_inited || fail_all_open) {
		VCD_MSG_ERROR("All client open requests failed");

		DDL_IDLE(ddl_context);

		vcd_handle_device_init_failed(drv_ctxt,
			DEVICE_STATE_EVENT_NUMBER(close));
		dev_ctxt->pending_cmd = VCD_CMD_DEVICE_TERM;
	} else {
		if (vcd_power_event(dev_ctxt, NULL,
					 VCD_EVT_PWR_DEV_INIT_END)) {
			VCD_MSG_ERROR("VCD_EVT_PWR_DEV_INIT_END failed");
		}
	}
}

static void  vcd_hw_timeout_cmn(struct vcd_drv_ctxt *drv_ctxt,
							  void *user_data)
{
	struct vcd_dev_ctxt *dev_ctxt = &drv_ctxt->dev_ctxt;
	VCD_MSG_LOW("vcd_hw_timeout_cmn:");
	vcd_device_timer_stop(dev_ctxt);

	vcd_handle_device_err_fatal(dev_ctxt, NULL);

	/* Reset HW. */
	(void) vcd_reset_device_context(drv_ctxt,
		DEVICE_STATE_EVENT_NUMBER(timeout));
}

static void vcd_dev_enter_null
	(struct vcd_drv_ctxt *drv_ctxt, s32 state_event) {
	VCD_MSG_MED("Entering DEVICE_STATE_NULL on api %d", state_event);

}

static void vcd_dev_enter_not_init
	(struct vcd_drv_ctxt *drv_ctxt, s32 state_event) {
	VCD_MSG_MED("Entering DEVICE_STATE_NOT_INIT on api %d",
			state_event);

}

static void vcd_dev_enter_initing
	(struct vcd_drv_ctxt *drv_ctxt, s32 state_event) {
	VCD_MSG_MED("Entering DEVICE_STATE_INITING on api %d",
			state_event);

}

static void vcd_dev_enter_ready
	(struct vcd_drv_ctxt *drv_ctxt, s32 state_event) {
	VCD_MSG_MED("Entering DEVICE_STATE_READY on api %d",
			state_event);
}

static void vcd_dev_enter_invalid(struct vcd_drv_ctxt *drv_ctxt,
							   s32 state_event)
{
   VCD_MSG_MED("Entering DEVICE_STATE_INVALID on api %d", state_event);
}

static void vcd_dev_exit_null
	(struct vcd_drv_ctxt *drv_ctxt, s32 state_event) {
	VCD_MSG_MED("Exiting DEVICE_STATE_NULL on api %d", state_event);
}

static void vcd_dev_exit_not_init
	(struct vcd_drv_ctxt *drv_ctxt, s32 state_event) {
	VCD_MSG_MED("Exiting DEVICE_STATE_NOT_INIT on api %d",
			state_event);

}

static void vcd_dev_exit_initing
	(struct vcd_drv_ctxt *drv_ctxt, s32 state_event) {
	VCD_MSG_MED("Exiting DEVICE_STATE_INITING on api %d",
			state_event);
}

static void vcd_dev_exit_ready
	(struct vcd_drv_ctxt *drv_ctxt, s32 state_event) {
	VCD_MSG_MED("Exiting DEVICE_STATE_READY on api %d", state_event);
}

static void vcd_dev_exit_invalid(struct vcd_drv_ctxt *drv_ctxt,
							  s32 state_event)
{
   VCD_MSG_MED("Exiting DEVICE_STATE_INVALID on api %d", state_event);
}

static const struct vcd_dev_state_table vcd_dev_table_null = {
	{
	 vcd_init_in_null,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 },
	vcd_dev_enter_null,
	vcd_dev_exit_null
};

static const struct vcd_dev_state_table vcd_dev_table_not_init = {
	{
	 vcd_init_in_not_init,
	 vcd_term_in_not_init,
	 vcd_open_in_not_init,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 NULL,
	 },
	vcd_dev_enter_not_init,
	vcd_dev_exit_not_init
};

static const struct vcd_dev_state_table vcd_dev_table_initing = {
	{
	 vcd_init_in_initing,
	 vcd_term_in_initing,
	 vcd_open_in_initing,
	 NULL,
	 NULL,
	 NULL,
	 vcd_dev_cb_in_initing,
	 vcd_hw_timeout_cmn,
	 },
	vcd_dev_enter_initing,
	vcd_dev_exit_initing
};

static const struct vcd_dev_state_table vcd_dev_table_ready = {
	{
	 vcd_init_in_ready,
	 vcd_term_in_ready,
	 vcd_open_in_ready,
	 vcd_close_in_ready,
	 vcd_resume_in_ready,
	 vcd_set_dev_pwr_in_ready,
	 NULL,
	 vcd_hw_timeout_cmn,
	 },
	vcd_dev_enter_ready,
	vcd_dev_exit_ready
};

static const struct vcd_dev_state_table vcd_dev_table_in_invalid = {
	{
		NULL,
		vcd_term_in_invalid,
		NULL,
		vcd_close_in_dev_invalid,
		NULL,
		NULL,
		NULL,
		NULL,
	},
	vcd_dev_enter_invalid,
	vcd_dev_exit_invalid
};

static const struct vcd_dev_state_table *vcd_dev_state_table[] = {
	&vcd_dev_table_null,
	&vcd_dev_table_not_init,
	&vcd_dev_table_initing,
	&vcd_dev_table_ready,
	&vcd_dev_table_in_invalid
};
