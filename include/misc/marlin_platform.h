/*
 * Copyright (C) 2016 Spreadtrum Communications Inc.
 *
 * Filename : marlin.h
 * Abstract : This file is a implementation for driver of marlin2
 *
 * Authors	: yufeng.yang
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __MARLIN_H__
#define __MARLIN_H__

#include <linux/types.h>
#include <misc/wcn_bus.h>

#define FALSE								(0)
#define TRUE								(1)

/* sync with wcn_get_chip_type() and wcn_chip_name */
enum wcn_chip_id_type {
	WCN_CHIP_ID_INVALID,
	WCN_CHIP_ID_AA,
	WCN_CHIP_ID_AB,
	WCN_CHIP_ID_AC,
	WCN_CHIP_ID_AD,
	/* WCN_CHIP_ID_MAX is the last one */
	WCN_CHIP_ID_MAX,
};

enum wcn_clock_type {
	WCN_CLOCK_TYPE_UNKNOWN,
	WCN_CLOCK_TYPE_TCXO,
	WCN_CLOCK_TYPE_TSX,
};

enum wcn_clock_mode {
	WCN_CLOCK_MODE_UNKNOWN,
	WCN_CLOCK_MODE_XO,
	WCN_CLOCK_MODE_BUFFER,
};

enum wcn_clock_type wcn_get_xtal_26m_clk_type(void);
enum wcn_clock_mode wcn_get_xtal_26m_clk_mode(void);
const char *wcn_get_chip_name(void);
enum wcn_chip_id_type wcn_get_chip_type(void);
void marlin_power_off(enum wcn_sub_sys subsys);
int marlin_get_power(void);
int marlin_set_wakeup(enum wcn_sub_sys subsys);
int marlin_set_sleep(enum wcn_sub_sys subsys, bool enable);
int marlin_reset_reg(void);
int start_marlin(enum wcn_sub_sys subsys);
int stop_marlin(enum wcn_sub_sys subsys);
int open_power_ctl(void);
bool marlin_get_download_status(void);
void marlin_set_download_status(int f);
void marlin_chip_en(bool enable, bool reset);
int marlin_get_module_status(void);
int marlin_get_module_status_changed(void);
int wcn_get_module_status_changed(void);
void wcn_set_module_status_changed(bool status);
int marlin_reset_register_notify(void *callback_func, void *para);
int marlin_reset_unregister_notify(void);
int is_first_power_on(enum wcn_sub_sys subsys);
void wcn_reset_pcie(void);
int cali_ini_need_download(enum wcn_sub_sys subsys);
const char *strno(enum wcn_sub_sys subsys);
void wcn_chip_power_on(void);
void wcn_chip_power_off(void);
void mdbg_assert_interface(char *str);
char *gnss_firmware_path_get(void);

#endif
