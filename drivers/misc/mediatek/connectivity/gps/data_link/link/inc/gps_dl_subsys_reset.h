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
#ifndef _GPS_DL_SUBSYS_RESET_H
#define _GPS_DL_SUBSYS_RESET_H

#include "gps_dl_base.h"

enum gps_each_link_reset_level {
	GPS_DL_RESET_LEVEL_NONE,
	GPS_DL_RESET_LEVEL_GPS_SINGLE_LINK,
	GPS_DL_RESET_LEVEL_GPS_SUBSYS,
	GPS_DL_RESET_LEVEL_CONNSYS,
	GPS_DL_RESET_LEVEL_NUM
};

enum GDL_RET_STATUS gps_dl_reset_level_set_and_trigger(
	enum gps_each_link_reset_level level, bool wait_reset_done);

bool gps_dl_reset_level_is_none(enum gps_dl_link_id_enum link_id);
bool gps_dl_reset_level_is_single(void);
bool gps_dl_reset_level_gt_single(void);

void gps_dl_trigger_gps_print_hw_status(void);
void gps_dl_trigger_gps_print_data_status(void);
int gps_dl_trigger_gps_subsys_reset(bool wait_reset_done);
int gps_dl_trigger_connsys_reset(void);
void gps_dl_handle_connsys_reset_done(void);

void gps_dl_register_conninfra_reset_cb(void);
void gps_dl_unregister_conninfra_reset_cb(void);

bool gps_dl_conninfra_is_readable(void);
void gps_dl_conninfra_not_readable_show_warning(unsigned int host_addr);
bool gps_dl_conninfra_is_okay_or_handle_it(int *p_hung_value, bool dump_on_hung_value_zero);

void gps_dl_test_mask_mcub_irq_on_open_set(enum gps_dl_link_id_enum link_id, bool mask);
bool gps_dl_test_mask_mcub_irq_on_open_get(enum gps_dl_link_id_enum link_id);

void gps_dl_test_mask_hasdata_irq_set(enum gps_dl_link_id_enum link_id, bool mask);
bool gps_dl_test_mask_hasdata_irq_get(enum gps_dl_link_id_enum link_id);

#endif /* _GPS_DL_SUBSYS_RESET_H */

