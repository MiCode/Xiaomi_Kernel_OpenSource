/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _AOLTEST_MESSAGE_BUILDER_H_
#define _AOLTEST_MESSAGE_BUILDER_H_

#include "aoltest_ring_buffer.h"

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define MAX_BUF_LEN    (3 * 1024)

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
enum aoltest_msg_id {
	AOLTEST_MSG_ID_DEFAULT = 0,
	AOLTEST_MSG_ID_WIFI = 1,
	AOLTEST_MSG_ID_BT = 2,
	AOLTEST_MSG_ID_GPS = 3,
	AOLTEST_MSG_ID_MAX
};

enum aoltest_msg_format {
    AOLTEST_MSG_FORMAT_DEFAULT = 0,
    AOLTEST_MSG_FORMAT_AUCSSID = 1,
    AOLTEST_MSG_FORMAT_MAX
};

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
void aoltest_push_message(struct aoltest_core_rb *rb, unsigned int type, unsigned int *buf);
int aoltest_pop_message(struct aoltest_core_rb *rb, char* buf);

#endif // _AOLTEST_MESSAGE_BUILDER_H_
