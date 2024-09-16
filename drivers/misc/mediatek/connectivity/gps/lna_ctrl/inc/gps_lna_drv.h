/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _GPS_LNA_DRV_H
#define _GPS_LNA_DRV_H
enum gps_lna_link_id_enum {
	GPS_DATA_LINK_ID0	= 0,
	GPS_DATA_LINK_ID1	= 1,
	GPS_DATA_LINK_NUM	= 2,
};

int gps_lna_linux_plat_drv_register(void);
int gps_lna_linux_plat_drv_unregister(void);
void gps_lna_pin_ctrl(enum gps_lna_link_id_enum link_id, bool dsp_is_on, bool force_en);
void gps_lna_update_status_for_md_blanking(bool gps_is_on);

#endif /* _GPS_DL_LINUX_PLAT_DRV_H */

