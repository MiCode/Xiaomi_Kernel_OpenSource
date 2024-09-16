/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _CONNSYSLOG_H_
#define _CONNSYSLOG_H_

#include <linux/types.h>
#include <linux/compiler.h>

#include "connsys_debug_utility.h"

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
/* Close debug log */
//#define DEBUG_LOG_ON 1

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
enum FW_LOG_MODE {
	PRINT_TO_KERNEL_LOG = 0,
	LOG_TO_FILE = 1,
};

typedef void (*CONNLOG_EVENT_CB) (void);

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/* utility  */
void connsys_log_dump_buf(const char *title, const char *buf, ssize_t sz);
void connsys_log_get_utc_time(
	unsigned int *second, unsigned int *usecond);

/* Global config */
int connsys_dedicated_log_path_apsoc_init(phys_addr_t emiaddr);
int connsys_dedicated_log_path_apsoc_deinit(void);
void connsys_dedicated_log_set_log_mode(int mode);
int connsys_dedicated_log_get_log_mode(void);
int connsys_dedicated_log_path_alarm_enable(unsigned int sec);
int connsys_dedicated_log_path_alarm_disable(void);
int connsys_dedicated_log_path_blank_state_changed(int blank_state);

/* For subsys */
int connsys_log_init(int conn_type);
int connsys_log_deinit(int conn_type);
unsigned int connsys_log_get_buf_size(int conn_type);
int connsys_log_register_event_cb(int conn_type, CONNLOG_EVENT_CB func);
ssize_t connsys_log_read_to_user(int conn_type, char __user *buf, size_t count);
ssize_t connsys_log_read(int conn_type, char *buf, size_t count);
int connsys_log_irq_handler(int conn_type);

#endif /*_CONNSYSLOG_H_*/
