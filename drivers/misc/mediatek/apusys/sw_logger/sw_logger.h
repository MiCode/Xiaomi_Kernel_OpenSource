/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SW_LOGGER_H__
#define __SW_LOGGER_H__

#include "apu.h"
#include "apu_config.h"

#define LOGGER_PREFIX "[apusys_logger]"

#define LOG_LINE_MAX_LENS 128

enum {
	DEBUG_LOG_WARN,
	DEBUG_LOG_INFO,
	DEBUG_LOG_DEBUG,
};

#define LOGGER_ERR(x, args...) \
	dev_info(sw_logger_dev, LOGGER_PREFIX "[error] %s " x, __func__, ##args)
#define LOGGER_WARN(x, args...) \
	dev_info(sw_logger_dev, LOGGER_PREFIX "[warn] %s " x, __func__, ##args)
#define LOGGER_INFO(x, args...) \
	{ \
		if (g_sw_logger_log_lv >= DEBUG_LOG_DEBUG) \
			dev_info(sw_logger_dev, LOGGER_PREFIX "[debug] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}

/* print to console via seq file */
#define DBG_LOG_CON(s, x, args...) \
	{\
		if (s) \
			seq_printf(s, x, ##args); \
		else \
			LOGGER_INFO(x, ##args); \
	}

#define APUSYS_LOGGER_DIR "apusys_logger"
#define APUSYS_LOGGER_AEE_DIR "apusys_debug"
#define APU_LOG_SIZE (1024*1024)
#define APU_LOG_BUF_SIZE (1024*1024)

#define IPI_DEBUG_LEVEL 4

/* #define SW_LOGGER_DEBUG */

int sw_logger_config_init(struct mtk_apu *apu);
int sw_logger_ipi_init(struct mtk_apu *apu);
void sw_logger_ipi_remove(struct mtk_apu *apu);

#endif /* __SW_LOGGER_H__ */
