/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef __MTK_MALI_CONFIG_H__
#define __MTK_MALI_CONFIG_H__

extern unsigned int _mtk_mali_ged_log;
#ifdef MTK_MALI_USE_GED_LOG

/* MTK logs */
#include <ged_log.h>
#define _MTK_MALI_PRINT(...) \
	do { ged_log_buf_print2(_mtk_mali_ged_log, GED_LOG_ATTR_TIME, __VA_ARGS__); } while (0)
#define MTK_err(...) \
	_MTK_MALI_PRINT(__VA_ARGS__)
#define dev_MTK_err(dev, ...) \
	do { _MTK_MALI_PRINT(__VA_ARGS__); dev_err(dev, __VA_ARGS__); } while (0)
#define dev_MTK_info(dev, ...) \
	do { _MTK_MALI_PRINT(__VA_ARGS__); dev_info(dev, __VA_ARGS__); } while (0)
#define pr_MTK_err( ...) \
	do { _MTK_MALI_PRINT(__VA_ARGS__); pr_err(__VA_ARGS__); } while (0)
#define pr_MTK_info( ...) \
	do { _MTK_MALI_PRINT(__VA_ARGS__); pr_info(__VA_ARGS__); } while (0)

#else

#define MTK_err(...)
#define dev_MTK_err dev_err
#define pr_MTK_err pr_err

#endif

#endif
