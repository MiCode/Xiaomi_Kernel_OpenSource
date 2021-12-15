/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __MTK_PLATFORM_DEBUG_H__
#define __MTK_PLATFORM_DEBUG_H__

#ifdef CONFIG_MTK_PLAT_SRAM_FLAG
/* plat_sram_flag */
extern int set_sram_flag_lastpc_valid(void);
extern int set_sram_flag_dfd_valid(void);
extern int set_sram_flag_etb_user(unsigned int etb_id, unsigned int user_id);
extern int set_sram_flag_timestamp(void);

#define ETB_USER_BIG_CORE       0x0
#define ETB_USER_CM4            0x1
#define ETB_USER_AUDIO_CM4      0x2
#define ETB_USER_BUS_TRACER     0x3
#define ETB_USER_MCSIB_TRACER   0x4
#endif

#ifdef CONFIG_MTK_DFD_INTERNAL_DUMP
#define DFD_BASIC_DUMP          0
#define DFD_EXTENDED_DUMP       1

extern int dfd_setup(int version);
#endif

#endif /* __MTK_PLATFORM_DEBUG_H__ */
