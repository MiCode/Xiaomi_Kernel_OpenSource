// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef _MT6798_CCU_HW_H_
#define _MT6798_CCU_HW_H_

#include "ccu_reg.h"
#include "ccu_drv.h"
extern void cam_mtcmos_check(void);

#define SPREG_00_MB_CCU2AP            CCU_INFO00
#define SPREG_01_MB_AP2CCU            CCU_INFO01
#define SPREG_02_LOG_DRAM_ADDR1       CCU_INFO02
#define SPREG_03_LOG_DRAM_ADDR2       CCU_INFO03
#define SPREG_04_LOG_LEVEL            CCU_INFO04
#define SPREG_05_LOG_TAGLEVEL         CCU_INFO05
#define SPREG_06_CPUREF_BUF_ADDR      CCU_INFO06
#define SPREG_07_LOG_SRAM_ADDR        CCU_INFO07
#define SPREG_08_CCU_INIT_CHECK       CCU_INFO08
#define SPREG_09_FORCE_PWR_DOWN       CCU_INFO09
#define SPREG_10_STRUCT_SIZE_CHECK    CCU_INFO10
#define SPREG_11_CCU_VER_NO           CCU_INFO11
#define SPREG_12_CCU_BW_I_REG         CCU_INFO12
#define SPREG_13_CCU_BW_O_REG         CCU_INFO13
#define SPREG_14_CCU_BW_G_REG         CCU_INFO14
#define SPREG_15_DMA_RST_CHK          CCU_INFO15
#define SPREG_16_DMA_TRG_CHK          CCU_INFO16
#define SPREG_17_DBG_MAIN_INIT        CCU_INFO17
#define SPREG_18_DBG_SRAM_LOG_CUR_POS CCU_INFO18
#define SPREG_19_DBG_SRAM_LOG_SLOT    CCU_INFO19
#define SPREG_20_DBG_ASSET_ERRNO      CCU_INFO20
#define SPREG_21_CTRL_BUF_ADDR        CCU_INFO21
#define SPREG_22                 CCU_INFO22
#define SPREG_23                 CCU_INFO23
#define SPREG_24                 CCU_INFO24
#define SPREG_25                 CCU_INFO25
#define SPREG_26                 CCU_INFO26
#define SPREG_27                 CCU_INFO27
#define SPREG_28                 CCU_INFO28
#define SPREG_29                 CCU_INFO29
#define SPREG_30                 CCU_INFO30
#define SPREG_31                 CCU_INFO31


#endif
