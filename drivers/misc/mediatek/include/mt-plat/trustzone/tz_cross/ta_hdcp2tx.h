/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __TRUSTZONE_TA_HDCP_TX__
#define __TRUSTZONE_TA_HDCP_TX__

#define TZ_TA_HDCP2_TX_UUID   "65e64a92-d60e-4d2d-bc38-a0a7ab721112"

/* Data Structure for HDCP2 tx TA
 * You should define data structure used both in REE/TEE here
 */
enum E_HDCP2_TX_KEY_SRC {
	CODED_FACSMILE_KEY = 0,
	CODED_LICENSE_KEY,
	DRM_KEY
};

/* Command for HDCP2_TX TA */
#define TZCMD_HDCP2_TX_SET_ENCKEY               0
#define TZCMD_HDCP2_TX_CHECK_RXID               1
#define TZCMD_HDCP2_TX_GET_ENC_KM               2
#define TZCMD_HDCP2_TX_KD_KEY_DEV               3
#define TZCMD_HDCP2_TX_COMPUTE_H                4
#define TZCMD_HDCP2_TX_COMPUTE_L                5
#define TZCMD_HDCP2_TX_GET_ENC_KS               6
#define TZCMD_HDCP2_TX_SET_PAIR_INFO            7
#define TZCMD_HDCP2_TX_INIT_AES                 8
#define TZCMD_HDCP2_TX_GET_ENC_DATA             9
#define TZCMD_HDCP2_TX_GET_PAIR_INFO           10
#define TZCMD_HDCP2_TX_QUERY_KEY_HAVE_SET      11
#define TZCMD_HDCP2_TX_VERIFY_SIGNATURE        12
#define TZCMD_HDCP2_TX_COMPUTE_V               13
#define TZCMD_HDCP2_TX_COMPUTE_2_2_H           14
#define TZCMD_HDCP2_TX_COMPUTE_2_12_V          15
#define TZCMD_HDCP2_TX_COMPUTE_M               16

#endif /* __TRUSTZONE_TA_HDCP2_TX__ */
