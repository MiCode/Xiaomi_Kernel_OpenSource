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

#ifndef __CCCI_CFG_H__
#define __CCCI_CFG_H__

#include "ccci_ch.h"

/*******************************************************************/
/**     ccci version define and explanation                                                   **/
/*******************************************************************/
/* v1.4 20120618: add dual ccci design for MT6589 and enhance ccci driver architecture */
#define CCCI_VERSION        "v1.4 20120618"

/*******************************************************************/
/**     ccci configure macro define                                                              **/
/*******************************************************************/
#define CCCI_MAX_CHANNEL    (100)

#define CCCI_WAKEUP_LOCK_NAME_LEN    (16)

#define MDLOGGER_FILE_PATH "/data/mdl/mdl_config"

#define IMG_INF_LEN            (256)

#define EE_BUF_LEN            (256)

#define EE_TIMER_BASE        (HZ)

#define CCCI_NODE_TYPE_NUM    (10)
#define NET_PORT_NUM        (4)

/*  Total must less than 255 */
#define STD_CHR_DEV_NUM        CCCI_MAX_CH_NUM
#define IPC_DEV_NUM            (20)
#define FS_DEV_NUM            (10)
#define VIR_CHR_DEV_NUM        (10)
#define TTY_DEV_NUM            (10)
#define RPC_DEV_NUM            (10)

#define CCCI_MAX_VCHR_NUM    (10)
#define CCCI_VIR_CHR_KFIFO_SIZE        (16)

/*******************************************************************/
/**     Feature options                                                                        **/
/*******************************************************************/
/* #define USING_PRINTK_LOG */

#endif				/* __CCCI_CFG_H__ */
