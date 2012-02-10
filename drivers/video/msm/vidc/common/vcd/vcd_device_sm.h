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
#ifndef _VCD_DEVICE_SM_H_
#define _VCD_DEVICE_SM_H_

#include <media/msm/vcd_api.h>
#include "vcd_ddl_api.h"
#include "vcd_core.h"

struct vcd_dev_state_table;
struct vcd_dev_state_ctxt;
struct vcd_drv_ctxt;

enum vcd_dev_state_enum {
	VCD_DEVICE_STATE_NULL = 0,
	VCD_DEVICE_STATE_NOT_INIT,
	VCD_DEVICE_STATE_INITING,
	VCD_DEVICE_STATE_READY,
	VCD_DEVICE_STATE_INVALID,
	VCD_DEVICE_STATE_MAX,
	VCD_DEVICE_STATE_32BIT = 0x7FFFFFFF
};

struct vcd_dev_state_table {
	struct {
		u32(*init) (struct vcd_drv_ctxt *drv_ctxt,
				struct vcd_init_config *config,
				s32 *driver_handle);

		u32(*term) (struct vcd_drv_ctxt *drv_ctxt,
				s32 driver_handle);

		u32(*open) (struct vcd_drv_ctxt *drv_ctxt,
				s32 driver_handle, u32 decoding,
				void (*callback) (u32 event, u32 status,
					void *info, size_t sz, void *handle,
					void *const client_data),
				void *client_data);

		u32(*close) (struct vcd_drv_ctxt *drv_ctxt,
				struct vcd_clnt_ctxt *cctxt);

		u32(*resume) (struct vcd_drv_ctxt *drv_ctxt,
				struct vcd_clnt_ctxt *cctxt);

		u32(*set_dev_pwr) (struct vcd_drv_ctxt *drv_ctxt,
				enum vcd_power_state pwr_state);

		void (*dev_cb) (struct vcd_drv_ctxt *drv_ctxt,
				u32 event, u32 status, void *payload,
				size_t sz, u32 *ddl_handle,
				void *const client_data);

		void (*timeout) (struct vcd_drv_ctxt *drv_ctxt,
							void *user_data);
	} ev_hdlr;

	void (*entry) (struct vcd_drv_ctxt *drv_ctxt,
			s32 state_event);
	void (*exit) (struct vcd_drv_ctxt *drv_ctxt,
			s32 state_event);
};

#define   DEVICE_STATE_EVENT_NUMBER(ppf) \
	((u32 *) (&(((struct vcd_dev_state_table*)0)->ev_hdlr.ppf)) - \
	(u32 *) (&(((struct vcd_dev_state_table*)0)->ev_hdlr.init)) \
	+ 1)

struct vcd_dev_state_ctxt {
	const struct vcd_dev_state_table *state_table;

	enum vcd_dev_state_enum state;
};

struct vcd_drv_ctxt {
	struct vcd_dev_state_ctxt dev_state;
	struct vcd_dev_ctxt dev_ctxt;
	struct mutex dev_mutex;
};


extern struct vcd_drv_ctxt *vcd_get_drv_context(void);

void vcd_continue(void);

#endif
