/*
 * Copyright (C) 2019 MediaTek Inc.
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
#ifndef _GPS_DL_NAME_LIST_H
#define _GPS_DL_NAME_LIST_H

#include "gps_dl_config.h"

#include "gps_each_link.h"
#include "gps_dl_hal_api.h"
#include "gps_dsp_fsm.h"
#include "gps_dl_base.h"

const char *gps_dl_dsp_state_name(enum gps_dsp_state_t state);
const char *gps_dl_dsp_event_name(enum gps_dsp_event_t event);

const char *gps_dl_link_state_name(enum gps_each_link_state_enum state);
const char *gps_dl_link_event_name(enum gps_dl_link_event_id event);
const char *gps_dl_hal_event_name(enum gps_dl_hal_event_id event);

const char *gps_dl_waitable_type_name(enum gps_each_link_waitable_type type);

#endif /* _GPS_DL_NAME_LIST_H */

