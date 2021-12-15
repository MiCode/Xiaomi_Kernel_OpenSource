/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

/*
 * This file is generated.
 * From 20190924_MT6885_MDReg_remap.xlsx
 * With ap_md_reg_dump_code_gentool.py v0.1
 * Date 2019-10-04 09:48:58.842149
 */

#ifndef __AP_MD_REG_DUMP_H__
#define __AP_MD_REG_DUMP_H__

/*  dump_md_reg ioremap,for internal dump */
struct dump_reg_ioremap {
	void __iomem **dump_reg;
	unsigned long long addr;
	unsigned long size;
};

enum MD_REG_ID {
	MD_REG_SET_DBGSYS_TIME_OUT_ADDR = 0,
	MD_REG_PC_MONITOR_ADDR,
	MD_REG_BUSMON_ADDR_0,
	MD_REG_BUSMON_ADDR_1,
	MD_REG_USIP_ADDR_0,
	MD_REG_USIP_ADDR_1,
	MD_REG_USIP_ADDR_2,
	MD_REG_USIP_ADDR_3,
	MD_REG_USIP_ADDR_4,
	MD_REG_USIP_ADDR_5,
};
extern void md_io_remap_internal_dump_register(struct ccci_modem *md);
void internal_md_dump_debug_register(unsigned int md_index);

#endif
