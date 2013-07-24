/* Copyright (c) 2002,2008-2011,2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __KGSL_LOG_H
#define __KGSL_LOG_H

extern unsigned int kgsl_cff_dump_enable;

#define KGSL_LOG_INFO(dev, lvl, fmt, args...) \
	do { \
		if ((lvl) >= 6)  \
			dev_info(dev, "|%s| " fmt, \
					__func__, ##args);\
	} while (0)

#define KGSL_LOG_WARN(dev, lvl, fmt, args...) \
	do { \
		if ((lvl) >= 4)  \
			dev_warn(dev, "|%s| " fmt, \
					__func__, ##args);\
	} while (0)

#define KGSL_LOG_ERR(dev, lvl, fmt, args...) \
	do { \
		if ((lvl) >= 3)  \
			dev_err(dev, "|%s| " fmt, \
					__func__, ##args);\
	} while (0)

#define KGSL_LOG_CRIT(dev, lvl, fmt, args...) \
	do { \
		if ((lvl) >= 2) \
			dev_crit(dev, "|%s| " fmt, \
					__func__, ##args);\
	} while (0)

#define KGSL_LOG_POSTMORTEM_WRITE(_dev, fmt, args...) \
	do { dev_crit(_dev->dev, fmt, ##args); } while (0)

#define KGSL_LOG_DUMP(_dev, fmt, args...)	dev_err(_dev->dev, fmt, ##args)

#define KGSL_DEV_ERR_ONCE(_dev, fmt, args...) \
({ \
	static bool kgsl_dev_err_once; \
							\
	if (!kgsl_dev_err_once) { \
		kgsl_dev_err_once = true; \
		dev_crit(_dev->dev, "|%s| " fmt, __func__, ##args); \
	} \
})

#define KGSL_DRV_INFO(_dev, fmt, args...) \
KGSL_LOG_INFO(_dev->dev, _dev->drv_log, fmt, ##args)
#define KGSL_DRV_WARN(_dev, fmt, args...) \
KGSL_LOG_WARN(_dev->dev, _dev->drv_log, fmt, ##args)
#define KGSL_DRV_ERR(_dev, fmt, args...)  \
KGSL_LOG_ERR(_dev->dev, _dev->drv_log, fmt, ##args)
#define KGSL_DRV_CRIT(_dev, fmt, args...) \
KGSL_LOG_CRIT(_dev->dev, _dev->drv_log, fmt, ##args)

#define KGSL_CMD_INFO(_dev, fmt, args...) \
KGSL_LOG_INFO(_dev->dev, _dev->cmd_log, fmt, ##args)
#define KGSL_CMD_WARN(_dev, fmt, args...) \
KGSL_LOG_WARN(_dev->dev, _dev->cmd_log, fmt, ##args)
#define KGSL_CMD_ERR(_dev, fmt, args...) \
KGSL_LOG_ERR(_dev->dev, _dev->cmd_log, fmt, ##args)
#define KGSL_CMD_CRIT(_dev, fmt, args...) \
KGSL_LOG_CRIT(_dev->dev, _dev->cmd_log, fmt, ##args)

#define KGSL_CTXT_INFO(_dev, fmt, args...) \
KGSL_LOG_INFO(_dev->dev, _dev->ctxt_log, fmt, ##args)
#define KGSL_CTXT_WARN(_dev, fmt, args...) \
KGSL_LOG_WARN(_dev->dev, _dev->ctxt_log, fmt, ##args)
#define KGSL_CTXT_ERR(_dev, fmt, args...)  \
KGSL_LOG_ERR(_dev->dev, _dev->ctxt_log, fmt, ##args)
#define KGSL_CTXT_CRIT(_dev, fmt, args...) \
KGSL_LOG_CRIT(_dev->dev, _dev->ctxt_log, fmt, ##args)

#define KGSL_MEM_INFO(_dev, fmt, args...) \
KGSL_LOG_INFO(_dev->dev, _dev->mem_log, fmt, ##args)
#define KGSL_MEM_WARN(_dev, fmt, args...) \
KGSL_LOG_WARN(_dev->dev, _dev->mem_log, fmt, ##args)
#define KGSL_MEM_ERR(_dev, fmt, args...)  \
KGSL_LOG_ERR(_dev->dev, _dev->mem_log, fmt, ##args)
#define KGSL_MEM_CRIT(_dev, fmt, args...) \
KGSL_LOG_CRIT(_dev->dev, _dev->mem_log, fmt, ##args)

#define KGSL_PWR_INFO(_dev, fmt, args...) \
KGSL_LOG_INFO(_dev->dev, _dev->pwr_log, fmt, ##args)
#define KGSL_PWR_WARN(_dev, fmt, args...) \
KGSL_LOG_WARN(_dev->dev, _dev->pwr_log, fmt, ##args)
#define KGSL_PWR_ERR(_dev, fmt, args...) \
KGSL_LOG_ERR(_dev->dev, _dev->pwr_log, fmt, ##args)
#define KGSL_PWR_CRIT(_dev, fmt, args...) \
KGSL_LOG_CRIT(_dev->dev, _dev->pwr_log, fmt, ##args)

/* Core error messages - these are for core KGSL functions that have
   no device associated with them (such as memory) */

#define KGSL_CORE_ERR(fmt, args...) \
pr_err("kgsl: %s: " fmt, __func__, ##args)

#endif /* __KGSL_LOG_H */
