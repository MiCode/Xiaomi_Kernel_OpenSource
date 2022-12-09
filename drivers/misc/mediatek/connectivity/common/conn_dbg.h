/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef CONN_DBG_H
#define CONN_DBG_H

enum conn_dbg_log_type {
	CONN_DBG_LOG_TYPE_HW_ERR = 0,
	CONN_DBG_LOG_TYPE_NUM
};

/* Use this function if you want to add log everytime */
int conn_dbg_add_log(enum conn_dbg_log_type type, const char *buf);

/* Use this function if you want to add log only once. */
#define conn_dbg_add_log_once(_type, _buf) \
do { \
	static int _print;\
\
	if (_print == 0)\
		conn_dbg_add_log(_type, _buf);\
	else {\
		pr_info("%s type %d, log: [%s]. have been added %d times.",\
			__func__, _type, _buf, _print);\
	} \
	_print++;\
\
} while (0) \

#endif /* CONN_DBG_H */
