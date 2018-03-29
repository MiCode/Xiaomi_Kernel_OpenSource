/*
 * Copyright (C) 2015 MediaTek Inc.
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

/**
* @file    mt_clk_buf_ctl.c
* @brief   Driver for RF clock buffer control
*
*/
#ifndef __MT_CLK_BUF_CTL_H__
#define __MT_CLK_BUF_CTL_H__

#include <linux/kernel.h>
#include <linux/mutex.h>
#if defined(CONFIG_MTK_LEGACY)
#include <cust_clk_buf.h>
#endif

#if 1 /*TODO: need add Pad name@DCT tool */
#ifndef GPIO_RFIC0_BSI_CS
#define GPIO_RFIC0_BSI_CS         (GPIO159 | 0x80000000)   /* RFIC0_BSI_CS = GPIO159 */
#endif
#ifndef GPIO_RFIC0_BSI_CK
#define GPIO_RFIC0_BSI_CK         (GPIO160 | 0x80000000)   /* RFIC0_BSI_CK = GPIO160 */
#endif
#ifndef GPIO_RFIC0_BSI_D0
#define GPIO_RFIC0_BSI_D0         (GPIO69  | 0x80000000)    /* RFIC0_BSI_D0 = GPIO69 */
#endif
#ifndef GPIO_RFIC0_BSI_D1
#define GPIO_RFIC0_BSI_D1         (GPIO68  | 0x80000000)    /* RFIC0_BSI_D1 = GPIO68 */
#endif
#ifndef GPIO_RFIC0_BSI_D2
#define GPIO_RFIC0_BSI_D2         (GPIO67  | 0x80000000)    /* RFIC0_BSI_D2 = GPIO67 */
#endif
#endif

enum clk_buf_id {
	CLK_BUF_BB_MD		= 0,
	CLK_BUF_CONN		= 1,
	CLK_BUF_NFC			= 2,
	CLK_BUF_AUDIO		= 3,
	CLK_BUF_INVALID		= 4,
};

enum pmic_clk_buf_id {
	PMIC_CLK_BUF_BB_MD		= 0,
	PMIC_CLK_BUF_CONN		= 1,
	PMIC_CLK_BUF_NFC		= 2,
	PMIC_CLK_BUF_RF			= 3,
	PMIC_CLK_BUF_INVALID	= 4
};

#if !defined(CONFIG_MTK_LEGACY)
typedef enum {
	CLOCK_BUFFER_DISABLE	= 0,
	CLOCK_BUFFER_SW_CONTROL = 1,
	CLOCK_BUFFER_HW_CONTROL = 2,
} CLK_BUF_STATUS;

typedef enum {
	CLK_BUF_DRIVING_CURR_0_4MA,
	CLK_BUF_DRIVING_CURR_0_9MA,
	CLK_BUF_DRIVING_CURR_1_4MA,
	CLK_BUF_DRIVING_CURR_1_9MA
} MTK_CLK_BUF_DRIVING_CURR;
#endif

typedef enum {
	CLK_BUF_SW_DISABLE = 0,
	CLK_BUF_SW_ENABLE  = 1,
} CLK_BUF_SWCTRL_STATUS_T;

#define CLKBUF_NUM      4

#define STA_CLK_ON      1
#define STA_CLK_OFF     0

bool clk_buf_ctrl(enum clk_buf_id id, bool onoff);
void clk_buf_get_swctrl_status(CLK_BUF_SWCTRL_STATUS_T *status);
void clk_buf_get_rf_drv_curr(void *rf_drv_curr);
void clk_buf_set_by_flightmode(bool is_flightmode_on);
void clk_buf_save_afc_val(unsigned int afcdac);
void clk_buf_write_afcdac(void);
void clk_buf_control_bblpm(bool on);
int clk_buf_init(void);
bool is_clk_buf_under_flightmode(void);
bool is_clk_buf_from_pmic(void);

extern struct mutex clk_buf_ctrl_lock;
#endif

