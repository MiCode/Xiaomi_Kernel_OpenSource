/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __EXTD_DRV_LOG_H__
#define __EXTD_DRV_LOG_H__

#include <linux/printk.h>
#include "disp_drv_log.h"

/* HDMI log functions*/
#define HDMI_LOG(fmt, arg...)	DISPDBG("[EXTD-HDMI]:"fmt, ##arg)
#define HDMI_FUNC()		DISPDBG("[EXTD-HDMI]:%s\n", __func__)
#define HDMI_ERR(fmt, arg...)	DISPDBG("[EXTD-HDMI]:"fmt, ##arg)

/* Eink log functions */
#define EPD_LOG(fmt, arg...)	DISPDBG("[EXTD-EPD]:"fmt, ##arg)
#define EPD_FUNC()		DISPDBG("[EXTD-EPD]:%s\n", __func__)
#define EPD_ERR(fmt, arg...)	DISPDBG("[EXTD-EPD]:"fmt, ##arg)

/* LCM log functions*/
#define LCM_LOG(fmt, arg...)	DISPDBG("[EXTD-LCM]:"fmt, ##arg)
#define LCM_FUNC()		DISPDBG("[EXTD-LCM]:%s\n", __func__)
#define LCM_ERR(fmt, arg...)	DISPDBG("[EXTD-LCM]:"fmt, ##arg)

/* external display - multi-control log functions */
#define MULTI_COTRL_LOG(fmt, arg...)	DISPDBG("[EXTD-MULTI]:"fmt, ##arg)
#define MULTI_COTRL_ERR(fmt, arg...)	DISPDBG("[EXTD-MULTI]:"fmt, ##arg)
#define MULTI_COTRL_FUNC()		DISPDBG("[EXTD-MULTI]:%s\n", __func__)

/* external display log functions*/
#define EXT_DISP_LOG(fmt, arg...)	DISPDBG("[EXTD]:"fmt, ##arg)
#define EXT_DISP_ERR(fmt, arg...)	DISPDBG("[EXTD]:"fmt, ##arg)
#define EXT_DISP_FUNC()			DISPDBG("[EXTD]:%s\n", __func__)

/* external display mgr log functions*/
#define EXT_MGR_LOG(fmt, arg...)	DISPDBG("[EXTD-MGR]:"fmt, ##arg)
#define EXT_MGR_ERR(fmt, arg...)	DISPDBG("[EXTD-MGR]:"fmt, ##arg)
#define EXT_MGR_FUNC()			DISPDBG("[EXTD-MGR]:%s\n", __func__)

/* external display - factory log functions*/
#define EXTD_FACTORY_LOG(fmt, arg...)	DISPDBG("[EXTD-FACT]:"fmt, ##arg)
#define EXTD_FACTORY_ERR(fmt, arg...)	DISPDBG("[EXTD-FACT]:"fmt, ##arg)
#define EXTD_FACTORY_FUNC()		DISPDBG("[EXTD-FACT]:%s\n", __func__)

#endif /* __EXTD_DRV_LOG_H__ */
