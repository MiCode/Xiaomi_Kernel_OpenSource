// SPDX-License-Identifier: GPL-2.0-only
/*
 * wcn_dump_integrate.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __WCN_DUMP_INTEGRATE_H__
#define __WCN_DUMP_INTEGRATE_H__

int mdbg_snap_shoot_iram(void *buf);
void mdbg_dump_mem_integ(enum wcn_source_type type);
int dump_arm_reg_integ(void);
u32 mdbg_check_wifi_ip_status(void);
u32 mdbg_check_bt_poweron(void);
u32 mdbg_check_gnss_poweron(void);
u32 mdbg_check_wcn_sys_exit_sleep(void);
u32 mdbg_check_btwf_sys_exit_sleep(void);

#endif
