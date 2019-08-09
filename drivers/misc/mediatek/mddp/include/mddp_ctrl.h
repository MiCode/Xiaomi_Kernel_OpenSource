/*
 * mddp_ctrl.h - Public API/structure provided by ctrl.
 *
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MDDP_CTRL_H
#define __MDDP_CTRL_H

#include "mddp_export.h"

//------------------------------------------------------------------------------
// Define marco.
// -----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// External functions.
//------------------------------------------------------------------------------
extern int32_t rndis_mddpu_change_state(enum mddp_state_e state,
			void *usb_buf, uint32_t *len);
extern void rndis_mddpu_usb_event(uint32_t msg_id, void *data, uint16_t len);
//------------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------
struct mddp_app_t *mddp_get_app_inst(enum mddp_app_type_e type);
enum mddp_state_e mddp_get_state(struct mddp_app_t *app);
uint8_t mddp_get_md_version(void);
void mddp_set_md_version(uint8_t version);

#endif				/* __MDDP_CTRL_H */
