/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _GPS_DL_HIST_REC_H
#define _GPS_DL_HIST_REC_H

#include "gps_dl_config.h"

enum gps_dl_hist_rec_rw_rec_point {
	DRW_ENTER,
	DRW_RETURN
};

void gps_each_link_rec_read(enum gps_dl_link_id_enum link_id, int pid, int len,
	enum gps_dl_hist_rec_rw_rec_point rec_point);
void gps_each_link_rec_write(enum gps_dl_link_id_enum link_id, int pid, int len,
	enum gps_dl_hist_rec_rw_rec_point rec_point);
void gps_each_link_rec_reset(enum gps_dl_link_id_enum link_id);
void gps_each_link_rec_force_dump(enum gps_dl_link_id_enum link_id);

#endif /* _GPS_DL_HIST_REC_H */

