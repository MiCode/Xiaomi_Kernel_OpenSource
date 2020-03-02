/*
 * Copyright (C) 2016 MediaTek Inc.
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
/*
 * @file    mtk_udi.h
 * @brief   Driver for UDI interface define
 *
 */
#ifndef __MTK_UDI_INTERNAL_H__
#define __MTK_UDI_INTERNAL_H__

#include <mtk_udi.h>
#include <mt-plat/mtk_secure_api.h>

#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
#define mt_secure_call_udi	mt_secure_call
#else
/* This is workaround for idvfs use */
static noinline int mt_secure_call_udi
(u64 function_id, u64 arg0, u64 arg1, u64 arg2, u64 arg3)
{
	register u64 reg0 __asm__("x0") = function_id;
	register u64 reg1 __asm__("x1") = arg0;
	register u64 reg2 __asm__("x2") = arg1;
	register u64 reg3 __asm__("x3") = arg2;
	register u64 reg4 __asm__("x4") = arg3;
	int ret = 0;

	asm volatile ("smc    #0\n" : "+r" (reg0) :
		"r"(reg1), "r"(reg2), "r"(reg3), "r"(reg4));

	ret = (int)reg0;
	return ret;
}
#endif


/* define for UDI service */
#ifdef CONFIG_ARM64
#define MTK_SIP_KERNEL_UDI_JTAG_CLOCK		0xC20003A0
#define MTK_SIP_KERNEL_UDI_BIT_CTRL			0xC20003A1
#define MTK_SIP_KERNEL_UDI_WRITE			0xC20003A2
#define MTK_SIP_KERNEL_UDI_READ				0xC20003A3
#else
#define MTK_SIP_KERNEL_UDI_JTAG_CLOCK		0x820003A0
#define MTK_SIP_KERNEL_UDI_BIT_CTRL			0x820003A1
#define MTK_SIP_KERNEL_UDI_WRITE			0x820003A2
#define MTK_SIP_KERNEL_UDI_READ				0x820003A3
#endif

/*
 * bit operation
 */
#define BIT_UDI(bit)		(1U << (bit))

#define MSB_UDI(range)	(1 ? range)
#define LSB_UDI(range)	(0 ? range)
/**
 * Genearte a mask wher MSB to LSB are all 0b1
 * @r: Range in the form of MSB:LSB
 */
#define BITMASK_UDI(r) \
	(((unsigned int) -1 >> (31 - MSB_UDI(r))) & ~((1U << LSB_UDI(r)) - 1))

#define GET_BITS_VAL_UDI(_bits_, _val_) \
	(((_val_) & (BITMASK_UDI(_bits_))) >> ((0) ? _bits_))

/**
 * Set value at MSB:LSB. For example, BITS(7:3, 0x5A)
 * will return a value where bit 3 to bit 7 is 0x5A
 * @r:	Range in the form of MSB:LSB
 */
/* BITS(MSB:LSB, value) => Set value at MSB:LSB  */
#define BITS_UDI(r, val)	((val << LSB_UDI(r)) & BITMASK_UDI(r))

/* define for UDI register service */

#define udi_reg_read(addr)	\
	mt_secure_call_udi(MTK_SIP_KERNEL_UDI_READ, addr, 0, 0, 0)
#define udi_reg_write(addr, val)	\
	mt_secure_call_udi(MTK_SIP_KERNEL_UDI_WRITE, addr, val, 0, 0)
#define udi_reg_field(addr, range, val)	\
	udi_reg_write(addr, (udi_reg_read(addr) & ~(BITMASK_UDI(range)))\
					| BITS_UDI(range, val))

#define udi_jtag_clock(sw_tck, i_trst, i_tms, i_tdi, count)	\
	mt_secure_call_udi(MTK_SIP_KERNEL_UDI_JTAG_CLOCK,	\
			(((1 << (sw_tck & 0x03)) << 3) |	\
			((i_trst & 0x01) << 2) |	\
			((i_tms & 0x01) << 1) |	\
			(i_tdi & 0x01)),	\
			count, (sw_tck & 0x04), 0)

#define udi_bit_ctrl(sw_tck, i_tdi, i_tms, i_trst)	\
	mt_secure_call_udi(MTK_SIP_KERNEL_UDI_BIT_CTRL,	\
			((sw_tck & 0x0f) << 3) |	\
			((i_trst & 0x01) << 2) |	\
			((i_tms & 0x01) << 1) |	\
			(i_tdi & 0x01), (sw_tck & 0x04), 0, 0)


/* receive string for temp  */
/* #define UDI_FIFOSIZE 16384  */
#define UDI_FIFOSIZE 256

#endif /* __MTK_UDI_INTERNAL_H__ */
