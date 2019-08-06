/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CMDQ_UTIL_H__
#define __CMDQ_UTIL_H__

#include <aee.h>

enum {
	CMDQ_LOG_FEAT_SECURE,
	CMDQ_LOG_FEAT_PERF,
	CMDQ_LOG_FEAT_NUM,
};

#define cmdq_util_log(feat, fmt, args...) \
	do { \
		cmdq_util_save_first_error("[cmdq] "fmt"\n", ##args); \
		if (cmdq_util_get_bit_feature() & (1 << feat)) \
			cmdq_msg(fmt, ##args); \
	} while (0)

#define cmdq_aee(fmt, args...) \
	pr_notice("[cmdq][aee] "fmt"\n", ##args)

#define cmdq_util_msg(fmt, args...) \
	do { \
		cmdq_msg(fmt, ##args); \
		cmdq_util_save_first_error("[cmdq] "fmt"\n", ##args); \
	} while (0)

#define cmdq_util_err(fmt, args...) \
	do { \
		cmdq_err(fmt, ##args); \
		cmdq_util_save_first_error("[cmdq][err] "fmt"\n", ##args); \
	} while (0)

#define DB_OPT_CMDQ	(DB_OPT_DEFAULT | DB_OPT_PROC_CMDQ_INFO | \
	DB_OPT_MMPROFILE_BUFFER | DB_OPT_FTRACE | DB_OPT_DUMP_DISPLAY)

#define cmdq_util_aee(key, fmt, args...) \
	do { \
		char tag[LINK_MAX]; \
		snprintf(tag, LINK_MAX, "CRDISPATCH_KEY:%s", key); \
		cmdq_aee("[cmdq][aee] "fmt, ##args); \
		cmdq_util_save_first_error("[cmdq][aee] "fmt, ##args); \
		cmdq_util_disable(); \
		aee_kernel_warning_api(__FILE__, __LINE__, \
			DB_OPT_CMDQ, tag, fmt, ##args); \
	} while (0)

u32 cmdq_util_get_bit_feature(void);

void cmdq_util_error_enable(void); // TODO : need be called
void cmdq_util_error_disable(void);
s32 cmdq_util_error_save(const char *str, ...);

const char *cmdq_event_module_dispatch(phys_addr_t gce_pa, const u16 event);

#endif
