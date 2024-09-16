/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "fm_typedef.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_stdlib.h"
#include "fm_link.h"
#include "fm_utils.h"
#include "fm_reg_utils.h"
#include "plat.h"

struct fm_wcn_reg_ops fm_wcn_ops;
unsigned char *cmd_buf;
struct fm_lock *cmd_buf_lock;
struct fm_res_ctx *fm_res;

void fw_spi_read(unsigned char addr, unsigned short *data)
{
	struct fm_spi_interface *si = &fm_wcn_ops.si;
	int ret = 0;
	unsigned int rdata;

	ret = si->sys_spi_read(si, SYS_SPI_FM, addr, &rdata);
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "read error[%d].\n", ret);
	*data = (unsigned short)rdata;
}

void fw_spi_write(unsigned char addr, unsigned short data)
{
	struct fm_spi_interface *si = &fm_wcn_ops.si;
	int ret = 0;
	unsigned int wdata = (unsigned int)data;

	ret = si->sys_spi_write(si, SYS_SPI_FM, addr, wdata);
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "write error[%d].\n", ret);
}

void fw_bop_udelay(unsigned int usec)
{
	fm_delayus(usec);
}

void fw_bop_rd_until(unsigned char addr, unsigned short mask,
		     unsigned short value)
{
	unsigned short data, count = 0;

	do {
		fm_delayus(1000);
		fw_spi_read(addr, &data);
		count++;
	} while (((data & mask) != value) && (count < 3000));

	/* 3000ms should be big enough for polling bits */
	if (count == 3000)
		WCN_DBG(FM_WAR | CHIP, "Value is never changed.\n");
}

void fw_bop_modify(unsigned char addr, unsigned short mask_and,
		   unsigned short mask_or)
{
	unsigned short data;

	fw_spi_read(addr, &data);
	data &= mask_and;
	data |= mask_or;
	fw_spi_write(addr, data);
}

void fw_bop_spi_rd_until(unsigned char subsys, unsigned short addr,
			 unsigned int mask, unsigned int value)
{
	struct fm_spi_interface *si = &fm_wcn_ops.si;
	unsigned int data;
	unsigned short count = 0;

	do {
		fm_delayus(1000);
		si->sys_spi_read(si, subsys, addr, &data);
		count++;
	} while (((data & mask) != value) && (count < 3000));

	/* 3000ms should be big enough for polling bits */
	if (count == 3000)
		WCN_DBG(FM_WAR | CHIP, "Value is never changed.\n");
}

void fw_bop_spi_modify(unsigned char subsys, unsigned short addr,
		       unsigned int mask_and, unsigned int mask_or)
{
	struct fm_spi_interface *si = &fm_wcn_ops.si;
	unsigned int data;

	si->sys_spi_read(si, subsys, addr, &data);
	data &= mask_and;
	data |= mask_or;
	si->sys_spi_write(si, subsys, addr, data);
}
