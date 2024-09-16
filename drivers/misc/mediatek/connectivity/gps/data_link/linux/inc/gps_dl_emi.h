/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _GPS_GPS_DL_EMI_H
#define _GPS_GPS_DL_EMI_H

#include "gps_dl_config.h"

#if GPS_DL_HAS_PLAT_DRV
#define GPS_ICAP_BUF_SIZE 0x50000 /* 320KB */

void gps_icap_probe(void);
#endif

#endif /* _GPS_GPS_DL_EMI_H */

