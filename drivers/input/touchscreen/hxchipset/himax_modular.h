/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for modularize functions
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef __HIMAX_IC_USAGE_H__
#define __HIMAX_IC_USAGE_H__

#if defined(CONFIG_TOUCHSCREEN_HIMAX_IC_HX852xJ)
extern bool _hx852xJ_init(void);
#endif
#if defined(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102)
extern bool _hx83102_init(void);
#endif
#if defined(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83108)
extern bool _hx83108_init(void);
#endif
#if defined(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83112)
extern bool _hx83112_init(void);
#endif
#if defined(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83121)
extern bool _hx83121_init(void);
#endif

#if !defined(__HIMAX_HX852xJ_MOD__)
extern struct fw_operation *pfw_op;
extern struct ic_operation *pic_op;
extern struct flash_operation *pflash_op;
extern struct driver_operation *pdriver_op;
#endif

#if defined(HX_ZERO_FLASH) && defined(CONFIG_TOUCHSCREEN_HIMAX_INCELL)
extern struct zf_operation *pzf_op;
extern int G_POWERONOF;
#endif

extern unsigned char IC_CHECKSUM;

#if defined(HX_EXCP_RECOVERY)
extern u8 HX_EXCP_RESET_ACTIVATE;
#endif

#if defined(HX_ZERO_FLASH) && defined(HX_CODE_OVERLAY)
#if defined(CONFIG_TOUCHSCREEN_HIMAX_INCELL)
extern uint8_t *ovl_idx;
#endif
#endif

extern unsigned long FW_VER_MAJ_FLASH_ADDR;
extern unsigned long FW_VER_MIN_FLASH_ADDR;
extern unsigned long CFG_VER_MAJ_FLASH_ADDR;
extern unsigned long CFG_VER_MIN_FLASH_ADDR;
extern unsigned long CID_VER_MAJ_FLASH_ADDR;
extern unsigned long CID_VER_MIN_FLASH_ADDR;
extern uint32_t CFG_TABLE_FLASH_ADDR;
extern uint32_t CFG_TABLE_FLASH_ADDR_T;

#if defined(HX_TP_PROC_2T2R)
	// static bool Is_2T2R;
#endif

#if defined(HX_USB_DETECT_GLOBAL)
	extern void (himax_cable_detect_func)(bool force_renew);
#endif

#if defined(HX_RST_PIN_FUNC)
	extern void (himax_rst_gpio_set)(int pinnum, uint8_t value);
#endif

extern struct himax_ts_data *private_ts;
extern struct himax_core_fp g_core_fp;
extern struct himax_ic_data *ic_data;

#if !defined(__HIMAX_HX852xJ_MOD__)
extern void himax_mcu_in_cmd_init(void);
extern int himax_mcu_in_cmd_struct_init(void);
#else
extern struct on_driver_operation *on_pdriver_op;
extern struct on_flash_operation *on_pflash_op;

extern void himax_mcu_on_cmd_init(void);
extern int himax_mcu_on_cmd_struct_init(void);
#endif

extern void himax_parse_assign_cmd(uint32_t addr, uint8_t *cmd, int len);

extern int himax_bus_read(uint8_t cmd, uint8_t *buf, uint32_t len);
extern int himax_bus_write(uint8_t cmd, uint8_t *addr, uint8_t *data,
	uint32_t len);

extern void himax_int_enable(int enable);

#endif
