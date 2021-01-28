/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef _H_DDP_LOG_
#define _H_DDP_LOG_

#ifdef CONFIG_MTK_AEE_FEATURE
#  include "mt-plat/aee.h"
#endif
#include "display_recorder.h"
#include "ddp_debug.h"
#include "disp_drv_log.h"

#ifndef LOG_TAG
#  define LOG_TAG
#endif

#define DDPSVPMSG(fmt, args...)	DISPMSG(fmt, ##args)

#define DISP_LOG_I(fmt, args...)					\
	do {								\
		dprec_logger_pr(DPREC_LOGGER_DEBUG, fmt, ##args);	\
		if (g_mobilelog)					\
			pr_info("[DDP/"LOG_TAG"]"fmt, ##args);		\
	} while (0)

#define DISP_LOG_V(fmt, args...)					\
	do {								\
		if (ddp_debug_dbg_log_level() >= 2)			\
			DISP_LOG_I(fmt, ##args);			\
	} while (0)

#define DISP_LOG_D(fmt, args...)					\
	do {								\
		if (ddp_debug_dbg_log_level())				\
			DISP_LOG_I(fmt, ##args);			\
	} while (0)

#define DISP_LOG_W(fmt, args...)					\
	do {								\
		dprec_logger_pr(DPREC_LOGGER_DEBUG, fmt, ##args);	\
		pr_debug("[DDP/"LOG_TAG"]warn:"fmt, ##args);		\
	} while (0)

#define DISP_LOG_E(fmt, args...)					\
	do {								\
		dprec_logger_pr(DPREC_LOGGER_ERROR, fmt, ##args);	\
		pr_debug("[DDP/"LOG_TAG"]error:"fmt, ##args);		\
	} while (0)

#define DDPIRQ(fmt, args...)						\
	do {								\
		if (ddp_debug_irq_log_level())				\
			DISP_LOG_I(fmt, ##args);			\
	} while (0)

#define DDPDBG(fmt, args...) DISP_LOG_D(fmt, ##args)
#define DDPMSG(fmt, args...) DISP_LOG_I(fmt, ##args)
#define DDP_PR_WARN(fmt, args...) DISP_LOG_W(fmt, ##args)
#define DDP_PR_ERR(fmt, args...) DISP_LOG_E(fmt, ##args)

#define DDPDUMP(fmt, ...)						\
	do {								\
		if (get_oneshot_dump() == ONESHOT_DUMP_UNDERGOING) {	\
			dprec_logger_pr(DPREC_LOGGER_ONESHOT_DUMP,	\
					fmt, ##__VA_ARGS__);		\
		} else {						\
			dprec_logger_pr(DPREC_LOGGER_DUMP,		\
					fmt, ##__VA_ARGS__);		\
			if (g_mobilelog)				\
				pr_debug("[DDP/"LOG_TAG"]"fmt,		\
					 ##__VA_ARGS__);		\
		}							\
	} while (0)

#ifndef ASSERT
#define ASSERT(expr)							\
	do {								\
		if (expr)						\
			break;						\
		pr_err("DDP ASSERT FAILED %s, %d\n", __FILE__, __LINE__); \
		WARN_ON(1);						\
	} while (0)
#endif

#ifdef CONFIG_MTK_AEE_FEATURE
#define DDPAEE(string, args...)						\
	do {								\
		char str[200];						\
		int ret;						\
		ret = snprintf(str, 199, "DDP:"string, ##args);		\
		if (ret < 0)						\
			str[0] = '\0';					\
		aee_kernel_warning_api(__FILE__, __LINE__,		\
			DB_OPT_DEFAULT | DB_OPT_MMPROFILE_BUFFER, str,	\
			string, ##args);				\
		pr_err("[DDP Error]"string, ##args);			\
	} while (0)
#else /* !CONFIG_MTK_AEE_FEATURE */
#define DDPAEE(string, args...)						\
	do {								\
		char str[200];						\
		snprintf(str, 199, "DDP:"string, ##args);		\
		pr_err("[DDP Error]"string, ##args);			\
	} while (0)
#endif /* CONFIG_MTK_AEE_FEATURE */

#endif /* _H_DDP_LOG_ */
