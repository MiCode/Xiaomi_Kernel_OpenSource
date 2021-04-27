/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __DP_HDCP_CMD_H__
#define __DP_HDCP_CMD_H__

#define CMD_DEVICE_ADDED        1
#define CMD_DEVICE_REMOVE       2
#define CMD_WRITE_VAL           3
#define CMD_DEVICE_CLEAN        4
#define CMD_ENABLE_ENCRYPT      5

//V1.3
#define CMD_CALCULATE_LM        11
#define CMD_COMPARE_R0          12
#define CMD_COMPARE_V1          13
#define CMD_GET_AKSV            14

//V2.2
#define CMD_AKE_CERTIFICATE     20
#define CMD_ENC_KM              21
#define CMD_AKE_H_PRIME         22
#define CMD_AKE_PARING          23
#define CMD_LC_L_PRIME          24
#define CMD_COMPARE_L           25
#define CMD_SKE_CAL_EKS         26

#define CMD_COMPARE_V2          27
#define CMD_COMPARE_M           28

//need remove in furture
#define CMD_LOAD_KEY            50

#endif //__DP_HDCP_CMD_H__
