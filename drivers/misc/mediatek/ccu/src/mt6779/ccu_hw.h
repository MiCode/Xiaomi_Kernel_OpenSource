/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef _MT6798_CCU_HW_H_
#define _MT6798_CCU_HW_H_

#include "ccu_reg.h"
#include "ccu_drv.h"
extern void cam_mtcmos_check(void);

/*spare register define*/
#define CCU_STA_REG_SW_INIT_DONE        CCU_INFO30
#define CCU_STA_REG_3A_INIT_DONE        CCU_INFO01
#define CCU_STA_REG_SP_ISR_TASK          CCU_INFO24
#define CCU_STA_REG_I2C_TRANSAC_LEN        CCU_INFO25
#define CCU_STA_REG_I2C_DO_DMA_EN         CCU_INFO26
#define CCU_STA_REG_QOS_BW_I            CCU_INFO27
#define CCU_STA_REG_QOS_BW_O            CCU_INFO28
#define CCU_STA_REG_QOS_BW_G            CCU_INFO29

/*
 * KuanFu Yeh@20160715
 * Spare Register         Data Type        Field
 * 0        int32        APMCU mailbox addr.
 * 1        int32        CCU mailbox addr.
 * 2        int32        DRAM log buffer addr.1
 * 3        int32        DRAM log buffer addr.2
 */
#define CCU_DATA_REG_MAILBOX_APMCU        CCU_INFO00
#define CCU_DATA_REG_MAILBOX_CCU        CCU_INFO01
#define CCU_DATA_REG_LOG_BUF0                CCU_INFO02
#define CCU_DATA_REG_LOG_BUF1                CCU_INFO03

#endif
