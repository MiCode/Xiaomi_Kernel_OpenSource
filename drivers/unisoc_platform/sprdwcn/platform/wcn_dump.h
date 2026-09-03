/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _WCN_DUMP_H
#define _WCN_DUMP_H

int mdbg_dump_mem(void);
int dump_arm_reg(void);
void sprdwcn_bus_armreg_write(unsigned int reg_index, unsigned int value);
int gnss_dump_data(void *start_addr, int len);
void gnss_dump_str(char *str, int str_len);

#endif
