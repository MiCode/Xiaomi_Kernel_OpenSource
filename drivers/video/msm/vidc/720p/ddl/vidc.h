/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef VIDC_H
#define VIDC_H
#include <linux/io.h>
#include <linux/delay.h>
#include <asm/system.h>

#define VIDC_720P_IN(reg)                       VIDC_##reg##_IN
#define VIDC_720P_INM(reg,  mask)                VIDC_##reg##_INM(mask)
#define VIDC_720P_OUT(reg,  val)                 VIDC_##reg##_OUT(val)
#define VIDC_720P_OUTI(reg,  index,  val)         VIDC_##reg##_OUTI(index, val)
#define VIDC_720P_OUTM(reg,  mask,  val)          VIDC_##reg##_OUTM(mask,  val)
#define VIDC_720P_SHFT(reg,  field)              VIDC_##reg##_##field##_SHFT
#define VIDC_720P_FMSK(reg,  field)              VIDC_##reg##_##field##_BMSK

#define VIDC_720P_INF(io, field) (VIDC_720P_INM(io, VIDC_720P_FMSK(io, field)) \
		>> VIDC_720P_SHFT(io,  field))
#define VIDC_720P_OUTF(io, field, val) \
		VIDC_720P_OUTM(io, VIDC_720P_FMSK(io, field), \
		val << VIDC_720P_SHFT(io,  field))

#define __inpdw(port)	ioread32(port)
#define __outpdw(port,  val) iowrite32(val, port)

#define in_dword_masked(addr,  mask) (__inpdw(addr) & (mask))

#define out_dword(addr,  val)        __outpdw(addr, val)

#define out_dword_masked(io,  mask,  val,  shadow)  \
do { \
	shadow = (shadow & (u32)(~(mask))) | ((u32)((val) & (mask))); \
	(void) out_dword(io,  shadow); \
} while (0)

#define out_dword_masked_ns(io,  mask,  val,  current_reg_content) \
	(void) out_dword(io,  ((current_reg_content & (u32)(~(mask))) | \
				((u32)((val) & (mask)))))

extern u8 *vidc_base_addr;

#define VIDC720P_BASE  vidc_base_addr
#define VIDC_720P_WRAPPER_REG_BASE               (VIDC720P_BASE + \
		0x00000000)
#define VIDC_720P_WRAPPER_REG_BASE_PHYS          VIDC_720P_BASE_PHYS

#define VIDC_REG_614413_ADDR                     \
	(VIDC_720P_WRAPPER_REG_BASE      + 00000000)
#define VIDC_REG_614413_PHYS                     \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 00000000)
#define VIDC_REG_614413_RMSK                            0x1
#define VIDC_REG_614413_SHFT                              0
#define VIDC_REG_614413_IN                       \
	in_dword_masked(VIDC_REG_614413_ADDR,        \
		VIDC_REG_614413_RMSK)
#define VIDC_REG_614413_INM(m)                   \
	in_dword_masked(VIDC_REG_614413_ADDR,  m)
#define VIDC_REG_614413_OUT(v)                   \
	out_dword(VIDC_REG_614413_ADDR, v)
#define VIDC_REG_614413_OUTM(m, v)                \
do { \
	out_dword_masked_ns(VIDC_REG_614413_ADDR, m, v, \
			VIDC_REG_614413_IN); \
} while (0)
#define VIDC_REG_614413_DMA_START_BMSK                  0x1
#define VIDC_REG_614413_DMA_START_SHFT                    0

#define VIDC_REG_591577_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x0000000c)
#define VIDC_REG_591577_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x0000000c)
#define VIDC_REG_591577_RMSK                 0xffffffff
#define VIDC_REG_591577_SHFT                          0
#define VIDC_REG_591577_IN                   \
	in_dword_masked(VIDC_REG_591577_ADDR,  \
			VIDC_REG_591577_RMSK)
#define VIDC_REG_591577_INM(m)               \
	in_dword_masked(VIDC_REG_591577_ADDR,  m)
#define VIDC_REG_591577_OUT(v)               \
	out_dword(VIDC_REG_591577_ADDR, v)
#define VIDC_REG_591577_OUTM(m, v)            \
do { \
	out_dword_masked_ns(VIDC_REG_591577_ADDR, m, v, \
			VIDC_REG_591577_IN); \
} while (0)
#define VIDC_REG_591577_BOOTCODE_SIZE_BMSK   0xffffffff
#define VIDC_REG_591577_BOOTCODE_SIZE_SHFT            0

#define VIDC_REG_203921_ADDR                   \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000014)
#define VIDC_REG_203921_PHYS                   \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000014)
#define VIDC_REG_203921_RMSK                   0xffffffff
#define VIDC_REG_203921_SHFT                            0
#define VIDC_REG_203921_IN                     \
	in_dword_masked(VIDC_REG_203921_ADDR,  \
			VIDC_REG_203921_RMSK)
#define VIDC_REG_203921_INM(m)                 \
	in_dword_masked(VIDC_REG_203921_ADDR,  m)
#define VIDC_REG_203921_OUT(v)                 \
	out_dword(VIDC_REG_203921_ADDR, v)
#define VIDC_REG_203921_OUTM(m, v)              \
do { \
	out_dword_masked_ns(VIDC_REG_203921_ADDR, m, v, \
			VIDC_REG_203921_IN); \
} while (0)
#define VIDC_REG_203921_DMA_EXTADDR_BMSK       0xffffffff
#define VIDC_REG_203921_DMA_EXTADDR_SHFT                0

#define VIDC_REG_275113_ADDR_ADDR            \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000018)
#define VIDC_REG_275113_ADDR_PHYS            \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000018)
#define VIDC_REG_275113_ADDR_RMSK            0xffffffff
#define VIDC_REG_275113_ADDR_SHFT                     0
#define VIDC_REG_275113_ADDR_IN              \
	in_dword_masked(VIDC_REG_275113_ADDR_ADDR,  \
			VIDC_REG_275113_ADDR_RMSK)
#define VIDC_REG_275113_ADDR_INM(m)          \
	in_dword_masked(VIDC_REG_275113_ADDR_ADDR,  m)
#define VIDC_REG_275113_ADDR_OUT(v)          \
	out_dword(VIDC_REG_275113_ADDR_ADDR, v)
#define VIDC_REG_275113_ADDR_OUTM(m, v)       \
do { \
	out_dword_masked_ns(VIDC_REG_275113_ADDR_ADDR, m, v, \
			VIDC_REG_275113_ADDR_IN); \
} while (0)
#define VIDC_REG_742076_ADDR_BMSK 0xffffffff
#define VIDC_REG_742076_ADDR_SHFT          0

#define VIDC_REG_988007_ADDR_ADDR              \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x0000001c)
#define VIDC_REG_988007_ADDR_PHYS              \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x0000001c)
#define VIDC_REG_988007_ADDR_RMSK              0xffffffff
#define VIDC_REG_988007_ADDR_SHFT                       0
#define VIDC_REG_988007_ADDR_IN                \
	in_dword_masked(VIDC_REG_988007_ADDR_ADDR,  \
			VIDC_REG_988007_ADDR_RMSK)
#define VIDC_REG_988007_ADDR_INM(m)            \
	in_dword_masked(VIDC_REG_988007_ADDR_ADDR,  m)
#define VIDC_REG_988007_ADDR_OUT(v)            \
	out_dword(VIDC_REG_988007_ADDR_ADDR, v)
#define VIDC_REG_988007_ADDR_OUTM(m, v)         \
do { \
	out_dword_masked_ns(VIDC_REG_988007_ADDR_ADDR, m, v, \
			VIDC_REG_988007_ADDR_IN); \
} while (0)
#define VIDC_REG_988007_ADDR_EXT_BUF_END_ADDR_BMSK 0xffffffff
#define VIDC_REG_988007_ADDR_EXT_BUF_END_ADDR_SHFT          0

#define VIDC_REG_531515_ADDR_ADDR                  \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000020)
#define VIDC_REG_531515_ADDR_PHYS                  \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000020)
#define VIDC_REG_531515_ADDR_RMSK                  0xffffffff
#define VIDC_REG_531515_ADDR_SHFT                           0
#define VIDC_REG_531515_ADDR_IN                    \
	in_dword_masked(VIDC_REG_531515_ADDR_ADDR,  \
			VIDC_REG_531515_ADDR_RMSK)
#define VIDC_REG_531515_ADDR_INM(m)                \
	in_dword_masked(VIDC_REG_531515_ADDR_ADDR,  m)
#define VIDC_REG_531515_ADDR_OUT(v)                \
	out_dword(VIDC_REG_531515_ADDR_ADDR, v)
#define VIDC_REG_531515_ADDR_OUTM(m, v)             \
do { \
	out_dword_masked_ns(VIDC_REG_531515_ADDR_ADDR, m, v, \
			VIDC_REG_531515_ADDR_IN); \
} while (0)
#define VIDC_REG_531515_ADDR_DMA_INT_ADDR_BMSK     0xffffffff
#define VIDC_REG_531515_ADDR_DMA_INT_ADDR_SHFT              0

#define VIDC_REG_87912_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000024)
#define VIDC_REG_87912_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000024)
#define VIDC_REG_87912_RMSK                 0xffffffff
#define VIDC_REG_87912_SHFT                          0
#define VIDC_REG_87912_IN                   \
	in_dword_masked(VIDC_REG_87912_ADDR,  \
			VIDC_REG_87912_RMSK)
#define VIDC_REG_87912_INM(m)               \
	in_dword_masked(VIDC_REG_87912_ADDR,  m)
#define VIDC_REG_87912_OUT(v)               \
	out_dword(VIDC_REG_87912_ADDR, v)
#define VIDC_REG_87912_OUTM(m, v)            \
do { \
	out_dword_masked_ns(VIDC_REG_87912_ADDR, m, v, \
			VIDC_REG_87912_IN); \
} while (0)
#define VIDC_REG_87912_HOST_PTR_ADDR_BMSK   0xffffffff
#define VIDC_REG_87912_HOST_PTR_ADDR_SHFT            0

#define VIDC_REG_896825_ADDR                      \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000028)
#define VIDC_REG_896825_PHYS                      \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000028)
#define VIDC_REG_896825_RMSK                             0x1
#define VIDC_REG_896825_SHFT                               0
#define VIDC_REG_896825_IN                        \
	in_dword_masked(VIDC_REG_896825_ADDR,         \
	VIDC_REG_896825_RMSK)
#define VIDC_REG_896825_INM(m)                    \
	in_dword_masked(VIDC_REG_896825_ADDR,  m)
#define VIDC_REG_896825_OUT(v)                    \
	out_dword(VIDC_REG_896825_ADDR, v)
#define VIDC_REG_896825_OUTM(m, v)                 \
do { \
	out_dword_masked_ns(VIDC_REG_896825_ADDR, m, v, \
			VIDC_REG_896825_IN); \
} while (0)
#define VIDC_REG_896825_LAST_DEC_BMSK                    0x1
#define VIDC_REG_896825_LAST_DEC_SHFT                      0

#define VIDC_REG_174526_ADDR                        \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x0000002c)
#define VIDC_REG_174526_PHYS                        \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x0000002c)
#define VIDC_REG_174526_RMSK                               0x1
#define VIDC_REG_174526_SHFT                                 0
#define VIDC_REG_174526_IN                          \
	in_dword_masked(VIDC_REG_174526_ADDR,  VIDC_REG_174526_RMSK)
#define VIDC_REG_174526_INM(m)                      \
	in_dword_masked(VIDC_REG_174526_ADDR,  m)
#define VIDC_REG_174526_DONE_M_BMSK                        0x1
#define VIDC_REG_174526_DONE_M_SHFT                          0

#define VIDC_REG_736316_ADDR                   \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000044)
#define VIDC_REG_736316_PHYS                   \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000044)
#define VIDC_REG_736316_RMSK                          0x1
#define VIDC_REG_736316_SHFT                            0
#define VIDC_REG_736316_IN                     \
	in_dword_masked(VIDC_REG_736316_ADDR,  \
			VIDC_REG_736316_RMSK)
#define VIDC_REG_736316_INM(m)                 \
	in_dword_masked(VIDC_REG_736316_ADDR,  m)
#define VIDC_REG_736316_OUT(v)                 \
	out_dword(VIDC_REG_736316_ADDR, v)
#define VIDC_REG_736316_OUTM(m, v)              \
do { \
	out_dword_masked_ns(VIDC_REG_736316_ADDR, m, v, \
			VIDC_REG_736316_IN); \
} while (0)
#define VIDC_REG_736316_BITS_ENDIAN_BMSK              0x1
#define VIDC_REG_736316_BITS_ENDIAN_SHFT                0

#define VIDC_REG_761892_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000054)
#define VIDC_REG_761892_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000054)
#define VIDC_REG_761892_RMSK                 0xffffffff
#define VIDC_REG_761892_SHFT                          0
#define VIDC_REG_761892_IN                   \
	in_dword_masked(VIDC_REG_761892_ADDR,  \
			VIDC_REG_761892_RMSK)
#define VIDC_REG_761892_INM(m)               \
	in_dword_masked(VIDC_REG_761892_ADDR,  m)
#define VIDC_REG_761892_OUT(v)               \
	out_dword(VIDC_REG_761892_ADDR, v)
#define VIDC_REG_761892_OUTM(m, v)            \
do { \
	out_dword_masked_ns(VIDC_REG_761892_ADDR, m, v, \
			VIDC_REG_761892_IN); \
} while (0)
#define VIDC_REG_761892_DEC_UNIT_SIZE_BMSK   0xffffffff
#define VIDC_REG_761892_DEC_UNIT_SIZE_SHFT            0

#define VIDC_REG_782249_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000058)
#define VIDC_REG_782249_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000058)
#define VIDC_REG_782249_RMSK                 0xffffffff
#define VIDC_REG_782249_SHFT                          0
#define VIDC_REG_782249_IN                   \
	in_dword_masked(VIDC_REG_782249_ADDR,  \
			VIDC_REG_782249_RMSK)
#define VIDC_REG_782249_INM(m)               \
	in_dword_masked(VIDC_REG_782249_ADDR,  m)
#define VIDC_REG_782249_ENC_UNIT_SIZE_BMSK   0xffffffff
#define VIDC_REG_782249_ENC_UNIT_SIZE_SHFT            0

#define VIDC_REG_66693_ADDR                \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x0000005c)
#define VIDC_REG_66693_PHYS                \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x0000005c)
#define VIDC_REG_66693_RMSK                       0xf
#define VIDC_REG_66693_SHFT                         0
#define VIDC_REG_66693_IN                  \
	in_dword_masked(VIDC_REG_66693_ADDR,  \
			VIDC_REG_66693_RMSK)
#define VIDC_REG_66693_INM(m)              \
	in_dword_masked(VIDC_REG_66693_ADDR,  m)
#define VIDC_REG_66693_OUT(v)              \
	out_dword(VIDC_REG_66693_ADDR, v)
#define VIDC_REG_66693_OUTM(m, v)           \
do { \
	out_dword_masked_ns(VIDC_REG_66693_ADDR, m, v, \
			VIDC_REG_66693_IN); \
} while (0)
#define VIDC_REG_66693_START_BYTE_NUM_BMSK        0xf
#define VIDC_REG_66693_START_BYTE_NUM_SHFT          0

#define VIDC_REG_114286_ADDR               \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000060)
#define VIDC_REG_114286_PHYS               \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000060)
#define VIDC_REG_114286_RMSK               0xffffffff
#define VIDC_REG_114286_SHFT                        0
#define VIDC_REG_114286_IN                 \
	in_dword_masked(VIDC_REG_114286_ADDR,  \
			VIDC_REG_114286_RMSK)
#define VIDC_REG_114286_INM(m)             \
	in_dword_masked(VIDC_REG_114286_ADDR,  m)
#define VIDC_REG_114286_ENC_HEADER_SIZE_BMSK 0xffffffff
#define VIDC_REG_114286_ENC_HEADER_SIZE_SHFT          0

#define VIDC_REG_713080_ADDR                   \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000100)
#define VIDC_REG_713080_PHYS                   \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000100)
#define VIDC_REG_713080_RMSK                         0x1f
#define VIDC_REG_713080_SHFT                            0
#define VIDC_REG_713080_IN                     \
	in_dword_masked(VIDC_REG_713080_ADDR,  \
			VIDC_REG_713080_RMSK)
#define VIDC_REG_713080_INM(m)                 \
	in_dword_masked(VIDC_REG_713080_ADDR,  m)
#define VIDC_REG_713080_OUT(v)                 \
	out_dword(VIDC_REG_713080_ADDR, v)
#define VIDC_REG_713080_OUTM(m, v)              \
do { \
	out_dword_masked_ns(VIDC_REG_713080_ADDR, m, v, \
			VIDC_REG_713080_IN); \
} while (0)
#define VIDC_REG_713080_ENC_ON_BMSK                  0x10
#define VIDC_REG_713080_ENC_ON_SHFT                   0x4
#define VIDC_REG_713080_STANDARD_SEL_BMSK             0xf
#define VIDC_REG_713080_STANDARD_SEL_SHFT               0

#define VIDC_REG_97293_ADDR                         \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000104)
#define VIDC_REG_97293_PHYS                         \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000104)
#define VIDC_REG_97293_RMSK                               0x1f
#define VIDC_REG_97293_SHFT                                  0
#define VIDC_REG_97293_IN                           \
	in_dword_masked(VIDC_REG_97293_ADDR,  VIDC_REG_97293_RMSK)
#define VIDC_REG_97293_INM(m)                       \
	in_dword_masked(VIDC_REG_97293_ADDR,  m)
#define VIDC_REG_97293_OUT(v)                       \
	out_dword(VIDC_REG_97293_ADDR, v)
#define VIDC_REG_97293_OUTM(m, v)                    \
do { \
	out_dword_masked_ns(VIDC_REG_97293_ADDR, m, v, \
			VIDC_REG_97293_IN); \
} while (0)
#define VIDC_REG_97293_CH_ID_BMSK                         0x1f
#define VIDC_REG_97293_CH_ID_SHFT                            0

#define VIDC_REG_224135_ADDR                     \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000108)
#define VIDC_REG_224135_PHYS                     \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000108)
#define VIDC_REG_224135_RMSK                            0x1
#define VIDC_REG_224135_SHFT                              0
#define VIDC_REG_224135_IN                       \
	in_dword_masked(VIDC_REG_224135_ADDR,        \
	VIDC_REG_224135_RMSK)
#define VIDC_REG_224135_INM(m)                   \
	in_dword_masked(VIDC_REG_224135_ADDR,  m)
#define VIDC_REG_224135_OUT(v)                   \
	out_dword(VIDC_REG_224135_ADDR, v)
#define VIDC_REG_224135_OUTM(m, v)                \
do { \
	out_dword_masked_ns(VIDC_REG_224135_ADDR, m, v, \
			VIDC_REG_224135_IN); \
} while (0)
#define VIDC_REG_224135_CPU_RESET_BMSK                  0x1
#define VIDC_REG_224135_CPU_RESET_SHFT                    0

#define VIDC_REG_832522_ADDR                        \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x0000010c)
#define VIDC_REG_832522_PHYS                        \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x0000010c)
#define VIDC_REG_832522_RMSK                               0x1
#define VIDC_REG_832522_SHFT                                 0
#define VIDC_REG_832522_IN                          \
	in_dword_masked(VIDC_REG_832522_ADDR,  VIDC_REG_832522_RMSK)
#define VIDC_REG_832522_INM(m)                      \
	in_dword_masked(VIDC_REG_832522_ADDR,  m)
#define VIDC_REG_832522_OUT(v)                      \
	out_dword(VIDC_REG_832522_ADDR, v)
#define VIDC_REG_832522_OUTM(m, v)                   \
do { \
	out_dword_masked_ns(VIDC_REG_832522_ADDR, m, v, \
			VIDC_REG_832522_IN); \
} while (0)
#define VIDC_REG_832522_FW_END_BMSK                        0x1
#define VIDC_REG_832522_FW_END_SHFT                          0

#define VIDC_REG_361582_ADDR                    \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000110)
#define VIDC_REG_361582_PHYS                    \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000110)
#define VIDC_REG_361582_RMSK                           0x1
#define VIDC_REG_361582_SHFT                             0
#define VIDC_REG_361582_IN                      \
	in_dword_masked(VIDC_REG_361582_ADDR,  \
			VIDC_REG_361582_RMSK)
#define VIDC_REG_361582_INM(m)                  \
	in_dword_masked(VIDC_REG_361582_ADDR,  m)
#define VIDC_REG_361582_OUT(v)                  \
	out_dword(VIDC_REG_361582_ADDR, v)
#define VIDC_REG_361582_OUTM(m, v)               \
do { \
	out_dword_masked_ns(VIDC_REG_361582_ADDR, m, v, \
			VIDC_REG_361582_IN); \
} while (0)
#define VIDC_REG_361582_BUS_MASTER_BMSK                0x1
#define VIDC_REG_361582_BUS_MASTER_SHFT                  0

#define VIDC_REG_314435_ADDR                   \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000114)
#define VIDC_REG_314435_PHYS                   \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000114)
#define VIDC_REG_314435_RMSK                          0x1
#define VIDC_REG_314435_SHFT                            0
#define VIDC_REG_314435_IN                     \
	in_dword_masked(VIDC_REG_314435_ADDR,  \
			VIDC_REG_314435_RMSK)
#define VIDC_REG_314435_INM(m)                 \
	in_dword_masked(VIDC_REG_314435_ADDR,  m)
#define VIDC_REG_314435_OUT(v)                 \
	out_dword(VIDC_REG_314435_ADDR, v)
#define VIDC_REG_314435_OUTM(m, v)              \
do { \
	out_dword_masked_ns(VIDC_REG_314435_ADDR, m, v, \
			VIDC_REG_314435_IN); \
} while (0)
#define VIDC_REG_314435_FRAME_START_BMSK              0x1
#define VIDC_REG_314435_FRAME_START_SHFT                0

#define VIDC_REG_999267_ADDR                    \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000118)
#define VIDC_REG_999267_PHYS                    \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000118)
#define VIDC_REG_999267_RMSK                        0xffff
#define VIDC_REG_999267_SHFT                             0
#define VIDC_REG_999267_IN                      \
	in_dword_masked(VIDC_REG_999267_ADDR,  \
			VIDC_REG_999267_RMSK)
#define VIDC_REG_999267_INM(m)                  \
	in_dword_masked(VIDC_REG_999267_ADDR,  m)
#define VIDC_REG_999267_OUT(v)                  \
	out_dword(VIDC_REG_999267_ADDR, v)
#define VIDC_REG_999267_OUTM(m, v)               \
do { \
	out_dword_masked_ns(VIDC_REG_999267_ADDR, m, v, \
			VIDC_REG_999267_IN); \
} while (0)
#define VIDC_REG_999267_IMG_SIZE_X_BMSK             0xffff
#define VIDC_REG_999267_IMG_SIZE_X_SHFT                  0

#define VIDC_REG_345712_ADDR                    \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x0000011c)
#define VIDC_REG_345712_PHYS                    \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x0000011c)
#define VIDC_REG_345712_RMSK                        0xffff
#define VIDC_REG_345712_SHFT                             0
#define VIDC_REG_345712_IN                      \
	in_dword_masked(VIDC_REG_345712_ADDR,  \
			VIDC_REG_345712_RMSK)
#define VIDC_REG_345712_INM(m)                  \
	in_dword_masked(VIDC_REG_345712_ADDR,  m)
#define VIDC_REG_345712_OUT(v)                  \
	out_dword(VIDC_REG_345712_ADDR, v)
#define VIDC_REG_345712_OUTM(m, v)               \
do { \
	out_dword_masked_ns(VIDC_REG_345712_ADDR, m, v, \
			VIDC_REG_345712_IN); \
} while (0)
#define VIDC_REG_345712_IMG_SIZE_Y_BMSK             0xffff
#define VIDC_REG_345712_IMG_SIZE_Y_SHFT                  0

#define VIDC_REG_443811_ADDR                       \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000124)
#define VIDC_REG_443811_PHYS                       \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000124)
#define VIDC_REG_443811_RMSK                              0x1
#define VIDC_REG_443811_SHFT                                0
#define VIDC_REG_443811_IN                         \
	in_dword_masked(VIDC_REG_443811_ADDR,  VIDC_REG_443811_RMSK)
#define VIDC_REG_443811_INM(m)                     \
	in_dword_masked(VIDC_REG_443811_ADDR,  m)
#define VIDC_REG_443811_OUT(v)                     \
	out_dword(VIDC_REG_443811_ADDR, v)
#define VIDC_REG_443811_OUTM(m, v)                  \
do { \
	out_dword_masked_ns(VIDC_REG_443811_ADDR, m, v, \
			VIDC_REG_443811_IN); \
} while (0)
#define VIDC_REG_443811_POST_ON_BMSK                      0x1
#define VIDC_REG_443811_POST_ON_SHFT                        0

#define VIDC_REG_538267_ADDR                    \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000128)
#define VIDC_REG_538267_PHYS                    \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000128)
#define VIDC_REG_538267_RMSK                    0xffffffff
#define VIDC_REG_538267_SHFT                             0
#define VIDC_REG_538267_IN                      \
	in_dword_masked(VIDC_REG_538267_ADDR,  \
			VIDC_REG_538267_RMSK)
#define VIDC_REG_538267_INM(m)                  \
	in_dword_masked(VIDC_REG_538267_ADDR,  m)
#define VIDC_REG_538267_OUT(v)                  \
	out_dword(VIDC_REG_538267_ADDR, v)
#define VIDC_REG_538267_OUTM(m, v)               \
do { \
	out_dword_masked_ns(VIDC_REG_538267_ADDR, m, v, \
			VIDC_REG_538267_IN); \
} while (0)
#define VIDC_REG_538267_QUOTIENT_VAL_BMSK       0xffff0000
#define VIDC_REG_538267_QUOTIENT_VAL_SHFT             0x10
#define VIDC_REG_538267_REMAINDER_VAL_BMSK          0xffff
#define VIDC_REG_538267_REMAINDER_VAL_SHFT               0

#define VIDC_REG_661565_ADDR                \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x0000012c)
#define VIDC_REG_661565_PHYS                \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x0000012c)
#define VIDC_REG_661565_RMSK                       0x1
#define VIDC_REG_661565_SHFT                         0
#define VIDC_REG_661565_IN                  \
	in_dword_masked(VIDC_REG_661565_ADDR,  \
			VIDC_REG_661565_RMSK)
#define VIDC_REG_661565_INM(m)              \
	in_dword_masked(VIDC_REG_661565_ADDR,  m)
#define VIDC_REG_661565_OUT(v)              \
	out_dword(VIDC_REG_661565_ADDR, v)
#define VIDC_REG_661565_OUTM(m, v)           \
do { \
	out_dword_masked_ns(VIDC_REG_661565_ADDR, m, v, \
			VIDC_REG_661565_IN); \
} while (0)
#define VIDC_REG_661565_SEQUENCE_START_BMSK        0x1
#define VIDC_REG_661565_SEQUENCE_START_SHFT          0

#define VIDC_REG_141269_ADDR                      \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000130)
#define VIDC_REG_141269_PHYS                      \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000130)
#define VIDC_REG_141269_RMSK                             0x1
#define VIDC_REG_141269_SHFT                               0
#define VIDC_REG_141269_IN                        \
	in_dword_masked(VIDC_REG_141269_ADDR,         \
	VIDC_REG_141269_RMSK)
#define VIDC_REG_141269_INM(m)                    \
	in_dword_masked(VIDC_REG_141269_ADDR,  m)
#define VIDC_REG_141269_OUT(v)                    \
	out_dword(VIDC_REG_141269_ADDR, v)
#define VIDC_REG_141269_OUTM(m, v)                 \
do { \
	out_dword_masked_ns(VIDC_REG_141269_ADDR, m, v, \
			VIDC_REG_141269_IN); \
} while (0)
#define VIDC_REG_141269_SW_RESET_BMSK                    0x1
#define VIDC_REG_141269_SW_RESET_SHFT                      0

#define VIDC_REG_193553_ADDR                      \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000134)
#define VIDC_REG_193553_PHYS                      \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000134)
#define VIDC_REG_193553_RMSK                             0x1
#define VIDC_REG_193553_SHFT                               0
#define VIDC_REG_193553_IN                        \
	in_dword_masked(VIDC_REG_193553_ADDR,         \
	VIDC_REG_193553_RMSK)
#define VIDC_REG_193553_INM(m)                    \
	in_dword_masked(VIDC_REG_193553_ADDR,  m)
#define VIDC_REG_193553_OUT(v)                    \
	out_dword(VIDC_REG_193553_ADDR, v)
#define VIDC_REG_193553_OUTM(m, v)                 \
do { \
	out_dword_masked_ns(VIDC_REG_193553_ADDR, m, v, \
			VIDC_REG_193553_IN); \
} while (0)
#define VIDC_REG_193553_FW_START_BMSK                    0x1
#define VIDC_REG_193553_FW_START_SHFT                      0

#define VIDC_REG_215724_ADDR                    \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000138)
#define VIDC_REG_215724_PHYS                    \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000138)
#define VIDC_REG_215724_RMSK                           0x1
#define VIDC_REG_215724_SHFT                             0
#define VIDC_REG_215724_IN                      \
	in_dword_masked(VIDC_REG_215724_ADDR,  \
			VIDC_REG_215724_RMSK)
#define VIDC_REG_215724_INM(m)                  \
	in_dword_masked(VIDC_REG_215724_ADDR,  m)
#define VIDC_REG_215724_OUT(v)                  \
	out_dword(VIDC_REG_215724_ADDR, v)
#define VIDC_REG_215724_OUTM(m, v)               \
do { \
	out_dword_masked_ns(VIDC_REG_215724_ADDR, m, v, \
			VIDC_REG_215724_IN); \
} while (0)
#define VIDC_REG_215724_ARM_ENDIAN_BMSK                0x1
#define VIDC_REG_215724_ARM_ENDIAN_SHFT                  0

#define VIDC_REG_846346_ADDR                      \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x0000013c)
#define VIDC_REG_846346_PHYS                      \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x0000013c)
#define VIDC_REG_846346_RMSK                             0x1
#define VIDC_REG_846346_SHFT                               0
#define VIDC_REG_846346_IN                        \
	in_dword_masked(VIDC_REG_846346_ADDR,         \
	VIDC_REG_846346_RMSK)
#define VIDC_REG_846346_INM(m)                    \
	in_dword_masked(VIDC_REG_846346_ADDR,  m)
#define VIDC_REG_846346_OUT(v)                    \
	out_dword(VIDC_REG_846346_ADDR, v)
#define VIDC_REG_846346_OUTM(m, v)                 \
do { \
	out_dword_masked_ns(VIDC_REG_846346_ADDR, m, v, \
			VIDC_REG_846346_IN); \
} while (0)
#define VIDC_REG_846346_ERR_CTRL_BMSK                    0x1
#define VIDC_REG_846346_ERR_CTRL_SHFT                      0

#define VIDC_REG_765787_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000200)
#define VIDC_REG_765787_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000200)
#define VIDC_REG_765787_RMSK                 0xffffffff
#define VIDC_REG_765787_SHFT                          0
#define VIDC_REG_765787_IN                   \
	in_dword_masked(VIDC_REG_765787_ADDR,  \
			VIDC_REG_765787_RMSK)
#define VIDC_REG_765787_INM(m)               \
	in_dword_masked(VIDC_REG_765787_ADDR,  m)
#define VIDC_REG_765787_OUT(v)               \
	out_dword(VIDC_REG_765787_ADDR, v)
#define VIDC_REG_765787_OUTM(m, v)            \
do { \
	out_dword_masked_ns(VIDC_REG_765787_ADDR, m, v, \
			VIDC_REG_765787_IN); \
} while (0)
#define VIDC_REG_765787_FW_STT_ADDR_0_BMSK   0xffffffff
#define VIDC_REG_765787_FW_STT_ADDR_0_SHFT            0

#define VIDC_REG_225040_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000204)
#define VIDC_REG_225040_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000204)
#define VIDC_REG_225040_RMSK                 0xffffffff
#define VIDC_REG_225040_SHFT                          0
#define VIDC_REG_225040_IN                   \
	in_dword_masked(VIDC_REG_225040_ADDR,  \
			VIDC_REG_225040_RMSK)
#define VIDC_REG_225040_INM(m)               \
	in_dword_masked(VIDC_REG_225040_ADDR,  m)
#define VIDC_REG_225040_OUT(v)               \
	out_dword(VIDC_REG_225040_ADDR, v)
#define VIDC_REG_225040_OUTM(m, v)            \
do { \
	out_dword_masked_ns(VIDC_REG_225040_ADDR, m, v, \
			VIDC_REG_225040_IN); \
} while (0)
#define VIDC_REG_225040_FW_STT_ADDR_1_BMSK   0xffffffff
#define VIDC_REG_225040_FW_STT_ADDR_1_SHFT            0

#define VIDC_REG_942456_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000208)
#define VIDC_REG_942456_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000208)
#define VIDC_REG_942456_RMSK                 0xffffffff
#define VIDC_REG_942456_SHFT                          0
#define VIDC_REG_942456_IN                   \
	in_dword_masked(VIDC_REG_942456_ADDR,  \
			VIDC_REG_942456_RMSK)
#define VIDC_REG_942456_INM(m)               \
	in_dword_masked(VIDC_REG_942456_ADDR,  m)
#define VIDC_REG_942456_OUT(v)               \
	out_dword(VIDC_REG_942456_ADDR, v)
#define VIDC_REG_942456_OUTM(m, v)            \
do { \
	out_dword_masked_ns(VIDC_REG_942456_ADDR, m, v, \
			VIDC_REG_942456_IN); \
} while (0)
#define VIDC_REG_942456_FW_STT_ADDR_2_BMSK   0xffffffff
#define VIDC_REG_942456_FW_STT_ADDR_2_SHFT            0

#define VIDC_REG_942170_ADDR_3_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x0000020c)
#define VIDC_REG_942170_ADDR_3_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x0000020c)
#define VIDC_REG_942170_ADDR_3_RMSK                 0xffffffff
#define VIDC_REG_942170_ADDR_3_SHFT                          0
#define VIDC_REG_942170_ADDR_3_IN                   \
	in_dword_masked(VIDC_REG_942170_ADDR_3_ADDR,  \
			VIDC_REG_942170_ADDR_3_RMSK)
#define VIDC_REG_942170_ADDR_3_INM(m)               \
	in_dword_masked(VIDC_REG_942170_ADDR_3_ADDR,  m)
#define VIDC_REG_942170_ADDR_3_OUT(v)               \
	out_dword(VIDC_REG_942170_ADDR_3_ADDR, v)
#define VIDC_REG_942170_ADDR_3_OUTM(m, v)            \
do { \
	out_dword_masked_ns(VIDC_REG_942170_ADDR_3_ADDR, m, v, \
			VIDC_REG_942170_ADDR_3_IN); \
} while (0)
#define VIDC_REG_942170_ADDR_3_FW_STT_ADDR_3_BMSK   0xffffffff
#define VIDC_REG_942170_ADDR_3_FW_STT_ADDR_3_SHFT            0

#define VIDC_REG_880188_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000210)
#define VIDC_REG_880188_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000210)
#define VIDC_REG_880188_RMSK                 0xffffffff
#define VIDC_REG_880188_SHFT                          0
#define VIDC_REG_880188_IN                   \
	in_dword_masked(VIDC_REG_880188_ADDR,  \
			VIDC_REG_880188_RMSK)
#define VIDC_REG_880188_INM(m)               \
	in_dword_masked(VIDC_REG_880188_ADDR,  m)
#define VIDC_REG_880188_OUT(v)               \
	out_dword(VIDC_REG_880188_ADDR, v)
#define VIDC_REG_880188_OUTM(m, v)            \
do { \
	out_dword_masked_ns(VIDC_REG_880188_ADDR, m, v, \
			VIDC_REG_880188_IN); \
} while (0)
#define VIDC_REG_880188_FW_STT_ADDR_4_BMSK   0xffffffff
#define VIDC_REG_880188_FW_STT_ADDR_4_SHFT            0

#define VIDC_REG_40293_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000214)
#define VIDC_REG_40293_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000214)
#define VIDC_REG_40293_RMSK                 0xffffffff
#define VIDC_REG_40293_SHFT                          0
#define VIDC_REG_40293_IN                   \
	in_dword_masked(VIDC_REG_40293_ADDR,  \
			VIDC_REG_40293_RMSK)
#define VIDC_REG_40293_INM(m)               \
	in_dword_masked(VIDC_REG_40293_ADDR,  m)
#define VIDC_REG_40293_OUT(v)               \
	out_dword(VIDC_REG_40293_ADDR, v)
#define VIDC_REG_40293_OUTM(m, v)            \
do { \
	out_dword_masked_ns(VIDC_REG_40293_ADDR, m, v, \
			VIDC_REG_40293_IN); \
} while (0)
#define VIDC_REG_40293_FW_STT_ADDR_5_BMSK   0xffffffff
#define VIDC_REG_40293_FW_STT_ADDR_5_SHFT            0

#define VIDC_REG_942170_ADDR_6_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000218)
#define VIDC_REG_942170_ADDR_6_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000218)
#define VIDC_REG_942170_ADDR_6_RMSK                 0xffffffff
#define VIDC_REG_942170_ADDR_6_SHFT                          0
#define VIDC_REG_942170_ADDR_6_IN                   \
	in_dword_masked(VIDC_REG_942170_ADDR_6_ADDR,  \
			VIDC_REG_942170_ADDR_6_RMSK)
#define VIDC_REG_942170_ADDR_6_INM(m)               \
	in_dword_masked(VIDC_REG_942170_ADDR_6_ADDR,  m)
#define VIDC_REG_942170_ADDR_6_OUT(v)               \
	out_dword(VIDC_REG_942170_ADDR_6_ADDR, v)
#define VIDC_REG_942170_ADDR_6_OUTM(m, v)            \
do { \
	out_dword_masked_ns(VIDC_REG_942170_ADDR_6_ADDR, m, v, \
			VIDC_REG_942170_ADDR_6_IN); \
} while (0)
#define VIDC_REG_942170_ADDR_6_FW_STT_ADDR_6_BMSK   0xffffffff
#define VIDC_REG_942170_ADDR_6_FW_STT_ADDR_6_SHFT            0

#define VIDC_REG_958768_ADDR                  \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000230)
#define VIDC_REG_958768_PHYS                  \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000230)
#define VIDC_REG_958768_RMSK                  0xffffffff
#define VIDC_REG_958768_SHFT                           0
#define VIDC_REG_958768_IN                    \
	in_dword_masked(VIDC_REG_958768_ADDR,  \
			VIDC_REG_958768_RMSK)
#define VIDC_REG_958768_INM(m)                \
	in_dword_masked(VIDC_REG_958768_ADDR,  m)
#define VIDC_REG_958768_OUT(v)                \
	out_dword(VIDC_REG_958768_ADDR, v)
#define VIDC_REG_958768_OUTM(m, v)             \
do { \
	out_dword_masked_ns(VIDC_REG_958768_ADDR, m, v, \
			VIDC_REG_958768_IN); \
} while (0)
#define VIDC_REG_699384_ADDR_BMSK     0xffffffff
#define VIDC_REG_699384_ADDR_SHFT              0

#define VIDC_REG_979942_ADDR                   \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000234)
#define VIDC_REG_979942_PHYS                   \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000234)
#define VIDC_REG_979942_RMSK                   0xffffffff
#define VIDC_REG_979942_SHFT                            0
#define VIDC_REG_979942_IN                     \
	in_dword_masked(VIDC_REG_979942_ADDR,  \
			VIDC_REG_979942_RMSK)
#define VIDC_REG_979942_INM(m)                 \
	in_dword_masked(VIDC_REG_979942_ADDR,  m)
#define VIDC_REG_979942_OUT(v)                 \
	out_dword(VIDC_REG_979942_ADDR, v)
#define VIDC_REG_979942_OUTM(m, v)              \
do { \
	out_dword_masked_ns(VIDC_REG_979942_ADDR, m, v, \
			VIDC_REG_979942_IN); \
} while (0)
#define VIDC_REG_979942_DB_STT_ADDR_BMSK       0xffffffff
#define VIDC_REG_979942_DB_STT_ADDR_SHFT                0

#define VIDC_REG_839021_ADDR                       \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000300)
#define VIDC_REG_839021_PHYS                       \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000300)
#define VIDC_REG_839021_RMSK                           0xff1f
#define VIDC_REG_839021_SHFT                                0
#define VIDC_REG_839021_IN                         \
	in_dword_masked(VIDC_REG_839021_ADDR,  VIDC_REG_839021_RMSK)
#define VIDC_REG_839021_INM(m)                     \
	in_dword_masked(VIDC_REG_839021_ADDR,  m)
#define VIDC_REG_839021_OUT(v)                     \
	out_dword(VIDC_REG_839021_ADDR, v)
#define VIDC_REG_839021_OUTM(m, v)                  \
do { \
	out_dword_masked_ns(VIDC_REG_839021_ADDR, m, v, \
			VIDC_REG_839021_IN); \
} while (0)
#define VIDC_REG_839021_LEVEL_BMSK                     0xff00
#define VIDC_REG_839021_LEVEL_SHFT                        0x8
#define VIDC_REG_839021_PROFILE_BMSK                     0x1f
#define VIDC_REG_839021_PROFILE_SHFT                        0

#define VIDC_REG_950374_ADDR                      \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000308)
#define VIDC_REG_950374_PHYS                      \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000308)
#define VIDC_REG_950374_RMSK                          0xffff
#define VIDC_REG_950374_SHFT                               0
#define VIDC_REG_950374_IN                        \
	in_dword_masked(VIDC_REG_950374_ADDR,         \
	VIDC_REG_950374_RMSK)
#define VIDC_REG_950374_INM(m)                    \
	in_dword_masked(VIDC_REG_950374_ADDR,  m)
#define VIDC_REG_950374_OUT(v)                    \
	out_dword(VIDC_REG_950374_ADDR, v)
#define VIDC_REG_950374_OUTM(m, v)                 \
do { \
	out_dword_masked_ns(VIDC_REG_950374_ADDR, m, v, \
			VIDC_REG_950374_IN); \
} while (0)
#define VIDC_REG_950374_I_PERIOD_BMSK                 0xffff
#define VIDC_REG_950374_I_PERIOD_SHFT                      0

#define VIDC_REG_504878_ADDR               \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000310)
#define VIDC_REG_504878_PHYS               \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000310)
#define VIDC_REG_504878_RMSK                      0xd
#define VIDC_REG_504878_SHFT                        0
#define VIDC_REG_504878_IN                 \
	in_dword_masked(VIDC_REG_504878_ADDR,  \
			VIDC_REG_504878_RMSK)
#define VIDC_REG_504878_INM(m)             \
	in_dword_masked(VIDC_REG_504878_ADDR,  m)
#define VIDC_REG_504878_OUT(v)             \
	out_dword(VIDC_REG_504878_ADDR, v)
#define VIDC_REG_504878_OUTM(m, v)          \
do { \
	out_dword_masked_ns(VIDC_REG_504878_ADDR, m, v, \
			VIDC_REG_504878_IN); \
} while (0)
#define VIDC_REG_504878_FIXED_NUMBER_BMSK         0xc
#define VIDC_REG_504878_FIXED_NUMBER_SHFT         0x2
#define VIDC_REG_504878_ENTROPY_SEL_BMSK          0x1
#define VIDC_REG_504878_ENTROPY_SEL_SHFT            0

#define VIDC_REG_458130_ADDR            \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000314)
#define VIDC_REG_458130_PHYS            \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000314)
#define VIDC_REG_458130_RMSK                 0xfff
#define VIDC_REG_458130_SHFT                     0
#define VIDC_REG_458130_IN              \
	in_dword_masked(VIDC_REG_458130_ADDR,  \
			VIDC_REG_458130_RMSK)
#define VIDC_REG_458130_INM(m)          \
	in_dword_masked(VIDC_REG_458130_ADDR,  m)
#define VIDC_REG_458130_OUT(v)          \
	out_dword(VIDC_REG_458130_ADDR, v)
#define VIDC_REG_458130_OUTM(m, v)       \
do { \
	out_dword_masked_ns(VIDC_REG_458130_ADDR, m, v, \
			VIDC_REG_458130_IN); \
} while (0)
#define VIDC_REG_458130_SLICE_ALPHA_C0_OFFSET_DIV2_BMSK      \
	0xf80
#define VIDC_REG_458130_SLICE_ALPHA_C0_OFFSET_DIV2_SHFT      \
	0x7
#define VIDC_REG_458130_SLICE_BETA_OFFSET_DIV2_BMSK       0x7c
#define VIDC_REG_458130_SLICE_BETA_OFFSET_DIV2_SHFT        0x2
#define \
	\
VIDC_REG_458130_DISABLE_DEBLOCKING_FILTER_IDC_BMSK        0x3
#define \
	\
VIDC_REG_458130_DISABLE_DEBLOCKING_FILTER_IDC_SHFT          0

#define VIDC_REG_314290_ADDR                   \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000318)
#define VIDC_REG_314290_PHYS                   \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000318)
#define VIDC_REG_314290_RMSK                          0x1
#define VIDC_REG_314290_SHFT                            0
#define VIDC_REG_314290_IN                     \
	in_dword_masked(VIDC_REG_314290_ADDR,  \
			VIDC_REG_314290_RMSK)
#define VIDC_REG_314290_INM(m)                 \
	in_dword_masked(VIDC_REG_314290_ADDR,  m)
#define VIDC_REG_314290_OUT(v)                 \
	out_dword(VIDC_REG_314290_ADDR, v)
#define VIDC_REG_314290_OUTM(m, v)              \
do { \
	out_dword_masked_ns(VIDC_REG_314290_ADDR, m, v, \
			VIDC_REG_314290_IN); \
} while (0)
#define VIDC_REG_314290_SHORT_HD_ON_BMSK              0x1
#define VIDC_REG_314290_SHORT_HD_ON_SHFT                0

#define VIDC_REG_588301_ADDR                    \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x0000031c)
#define VIDC_REG_588301_PHYS                    \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x0000031c)
#define VIDC_REG_588301_RMSK                           0x1
#define VIDC_REG_588301_SHFT                             0
#define VIDC_REG_588301_IN                      \
	in_dword_masked(VIDC_REG_588301_ADDR,  \
			VIDC_REG_588301_RMSK)
#define VIDC_REG_588301_INM(m)                  \
	in_dword_masked(VIDC_REG_588301_ADDR,  m)
#define VIDC_REG_588301_OUT(v)                  \
	out_dword(VIDC_REG_588301_ADDR, v)
#define VIDC_REG_588301_OUTM(m, v)               \
do { \
	out_dword_masked_ns(VIDC_REG_588301_ADDR, m, v, \
			VIDC_REG_588301_IN); \
} while (0)
#define VIDC_REG_588301_MSLICE_ENA_BMSK                0x1
#define VIDC_REG_588301_MSLICE_ENA_SHFT                  0

#define VIDC_REG_1517_ADDR                    \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000320)
#define VIDC_REG_1517_PHYS                    \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000320)
#define VIDC_REG_1517_RMSK                           0x3
#define VIDC_REG_1517_SHFT                             0
#define VIDC_REG_1517_IN                      \
	in_dword_masked(VIDC_REG_1517_ADDR,  \
			VIDC_REG_1517_RMSK)
#define VIDC_REG_1517_INM(m)                  \
	in_dword_masked(VIDC_REG_1517_ADDR,  m)
#define VIDC_REG_1517_OUT(v)                  \
	out_dword(VIDC_REG_1517_ADDR, v)
#define VIDC_REG_1517_OUTM(m, v)               \
do { \
	out_dword_masked_ns(VIDC_REG_1517_ADDR, m, v, \
			VIDC_REG_1517_IN); \
} while (0)
#define VIDC_REG_1517_MSLICE_SEL_BMSK                0x3
#define VIDC_REG_1517_MSLICE_SEL_SHFT                  0

#define VIDC_REG_105335_ADDR                     \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000324)
#define VIDC_REG_105335_PHYS                     \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000324)
#define VIDC_REG_105335_RMSK                     0xffffffff
#define VIDC_REG_105335_SHFT                              0
#define VIDC_REG_105335_IN                       \
	in_dword_masked(VIDC_REG_105335_ADDR,        \
	VIDC_REG_105335_RMSK)
#define VIDC_REG_105335_INM(m)                   \
	in_dword_masked(VIDC_REG_105335_ADDR,  m)
#define VIDC_REG_105335_OUT(v)                   \
	out_dword(VIDC_REG_105335_ADDR, v)
#define VIDC_REG_105335_OUTM(m, v)                \
do { \
	out_dword_masked_ns(VIDC_REG_105335_ADDR, m, v, \
			VIDC_REG_105335_IN); \
} while (0)
#define VIDC_REG_105335_MSLICE_MB_BMSK           0xffffffff
#define VIDC_REG_105335_MSLICE_MB_SHFT                    0

#define VIDC_REG_561679_ADDR                   \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000328)
#define VIDC_REG_561679_PHYS                   \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000328)
#define VIDC_REG_561679_RMSK                   0xffffffff
#define VIDC_REG_561679_SHFT                            0
#define VIDC_REG_561679_IN                     \
	in_dword_masked(VIDC_REG_561679_ADDR,  \
			VIDC_REG_561679_RMSK)
#define VIDC_REG_561679_INM(m)                 \
	in_dword_masked(VIDC_REG_561679_ADDR,  m)
#define VIDC_REG_561679_OUT(v)                 \
	out_dword(VIDC_REG_561679_ADDR, v)
#define VIDC_REG_561679_OUTM(m, v)              \
do { \
	out_dword_masked_ns(VIDC_REG_561679_ADDR, m, v, \
			VIDC_REG_561679_IN); \
} while (0)
#define VIDC_REG_561679_MSLICE_BYTE_BMSK       0xffffffff
#define VIDC_REG_561679_MSLICE_BYTE_SHFT                0

#define VIDC_REG_151345_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000400)
#define VIDC_REG_151345_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000400)
#define VIDC_REG_151345_RMSK                 0xffffffff
#define VIDC_REG_151345_SHFT                          0
#define VIDC_REG_151345_IN                   \
	in_dword_masked(VIDC_REG_151345_ADDR,  \
			VIDC_REG_151345_RMSK)
#define VIDC_REG_151345_INM(m)               \
	in_dword_masked(VIDC_REG_151345_ADDR,  m)
#define VIDC_REG_151345_DISPLAY_Y_ADR_BMSK   0xffffffff
#define VIDC_REG_151345_DISPLAY_Y_ADR_SHFT            0

#define VIDC_REG_293983_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000404)
#define VIDC_REG_293983_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000404)
#define VIDC_REG_293983_RMSK                 0xffffffff
#define VIDC_REG_293983_SHFT                          0
#define VIDC_REG_293983_IN                   \
	in_dword_masked(VIDC_REG_293983_ADDR,  \
			VIDC_REG_293983_RMSK)
#define VIDC_REG_293983_INM(m)               \
	in_dword_masked(VIDC_REG_293983_ADDR,  m)
#define VIDC_REG_293983_DISPLAY_C_ADR_BMSK   0xffffffff
#define VIDC_REG_293983_DISPLAY_C_ADR_SHFT            0

#define VIDC_REG_612715_ADDR                \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000408)
#define VIDC_REG_612715_PHYS                \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000408)
#define VIDC_REG_612715_RMSK                      0x3f
#define VIDC_REG_612715_SHFT                         0
#define VIDC_REG_612715_IN                  \
	in_dword_masked(VIDC_REG_612715_ADDR,  \
			VIDC_REG_612715_RMSK)
#define VIDC_REG_612715_INM(m)              \
	in_dword_masked(VIDC_REG_612715_ADDR,  m)
#define VIDC_REG_612715_DISPLAY_STATUS_BMSK       0x3f
#define VIDC_REG_612715_DISPLAY_STATUS_SHFT          0

#define VIDC_REG_209364_ADDR                   \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x0000040c)
#define VIDC_REG_209364_PHYS                   \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x0000040c)
#define VIDC_REG_209364_RMSK                          0x1
#define VIDC_REG_209364_SHFT                            0
#define VIDC_REG_209364_IN                     \
	in_dword_masked(VIDC_REG_209364_ADDR,  \
			VIDC_REG_209364_RMSK)
#define VIDC_REG_209364_INM(m)                 \
	in_dword_masked(VIDC_REG_209364_ADDR,  m)
#define VIDC_REG_209364_HEADER_DONE_BMSK              0x1
#define VIDC_REG_209364_HEADER_DONE_SHFT                0

#define VIDC_REG_757835_ADDR                     \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000410)
#define VIDC_REG_757835_PHYS                     \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000410)
#define VIDC_REG_757835_RMSK                     0xffffffff
#define VIDC_REG_757835_SHFT                              0
#define VIDC_REG_757835_IN                       \
	in_dword_masked(VIDC_REG_757835_ADDR,        \
	VIDC_REG_757835_RMSK)
#define VIDC_REG_757835_INM(m)                   \
	in_dword_masked(VIDC_REG_757835_ADDR,  m)
#define VIDC_REG_757835_FRAME_NUM_BMSK           0xffffffff
#define VIDC_REG_757835_FRAME_NUM_SHFT                    0

#define VIDC_REG_352831_ADDR              \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000414)
#define VIDC_REG_352831_PHYS              \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000414)
#define VIDC_REG_352831_RMSK              0xffffffff
#define VIDC_REG_352831_SHFT                       0
#define VIDC_REG_352831_IN                \
	in_dword_masked(VIDC_REG_352831_ADDR,  \
			VIDC_REG_352831_RMSK)
#define VIDC_REG_352831_INM(m)            \
	in_dword_masked(VIDC_REG_352831_ADDR,  m)
#define VIDC_REG_352831_DBG_INFO_OUTPUT0_BMSK 0xffffffff
#define VIDC_REG_352831_DBG_INFO_OUTPUT0_SHFT          0

#define VIDC_REG_668634_ADDR              \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000418)
#define VIDC_REG_668634_PHYS              \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000418)
#define VIDC_REG_668634_RMSK              0xffffffff
#define VIDC_REG_668634_SHFT                       0
#define VIDC_REG_668634_IN                \
	in_dword_masked(VIDC_REG_668634_ADDR,  \
			VIDC_REG_668634_RMSK)
#define VIDC_REG_668634_INM(m)            \
	in_dword_masked(VIDC_REG_668634_ADDR,  m)
#define VIDC_REG_668634_DBG_INFO_OUTPUT1_BMSK 0xffffffff
#define VIDC_REG_668634_DBG_INFO_OUTPUT1_SHFT          0

#define VIDC_REG_609676_ADDR                       \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000500)
#define VIDC_REG_609676_PHYS                       \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000500)
#define VIDC_REG_609676_RMSK                              0x1
#define VIDC_REG_609676_SHFT                                0
#define VIDC_REG_609676_IN                         \
	in_dword_masked(VIDC_REG_609676_ADDR,  VIDC_REG_609676_RMSK)
#define VIDC_REG_609676_INM(m)                     \
	in_dword_masked(VIDC_REG_609676_ADDR,  m)
#define VIDC_REG_609676_OUT(v)                     \
	out_dword(VIDC_REG_609676_ADDR, v)
#define VIDC_REG_609676_OUTM(m, v)                  \
do { \
	out_dword_masked_ns(VIDC_REG_609676_ADDR, m, v, \
			VIDC_REG_609676_IN); \
} while (0)
#define VIDC_REG_609676_INT_OFF_BMSK                      0x1
#define VIDC_REG_609676_INT_OFF_SHFT                        0

#define VIDC_REG_491082_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000504)
#define VIDC_REG_491082_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000504)
#define VIDC_REG_491082_RMSK                        0x1
#define VIDC_REG_491082_SHFT                          0
#define VIDC_REG_491082_IN                   \
	in_dword_masked(VIDC_REG_491082_ADDR,  \
			VIDC_REG_491082_RMSK)
#define VIDC_REG_491082_INM(m)               \
	in_dword_masked(VIDC_REG_491082_ADDR,  m)
#define VIDC_REG_491082_OUT(v)               \
	out_dword(VIDC_REG_491082_ADDR, v)
#define VIDC_REG_491082_OUTM(m, v)            \
do { \
	out_dword_masked_ns(VIDC_REG_491082_ADDR, m, v, \
			VIDC_REG_491082_IN); \
} while (0)
#define VIDC_REG_491082_INT_PULSE_SEL_BMSK          0x1
#define VIDC_REG_491082_INT_PULSE_SEL_SHFT            0

#define VIDC_REG_614776_ADDR                \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000508)
#define VIDC_REG_614776_PHYS                \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000508)
#define VIDC_REG_614776_RMSK                       0x1
#define VIDC_REG_614776_SHFT                         0
#define VIDC_REG_614776_IN                  \
	in_dword_masked(VIDC_REG_614776_ADDR,  \
			VIDC_REG_614776_RMSK)
#define VIDC_REG_614776_INM(m)              \
	in_dword_masked(VIDC_REG_614776_ADDR,  m)
#define VIDC_REG_614776_OUT(v)              \
	out_dword(VIDC_REG_614776_ADDR, v)
#define VIDC_REG_614776_OUTM(m, v)           \
do { \
	out_dword_masked_ns(VIDC_REG_614776_ADDR, m, v, \
			VIDC_REG_614776_IN); \
} while (0)
#define VIDC_REG_614776_INT_DONE_CLEAR_BMSK        0x1
#define VIDC_REG_614776_INT_DONE_CLEAR_SHFT          0

#define VIDC_REG_982553_ADDR                \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x0000050c)
#define VIDC_REG_982553_PHYS                \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x0000050c)
#define VIDC_REG_982553_RMSK                       0x1
#define VIDC_REG_982553_SHFT                         0
#define VIDC_REG_982553_IN                  \
	in_dword_masked(VIDC_REG_982553_ADDR,  \
			VIDC_REG_982553_RMSK)
#define VIDC_REG_982553_INM(m)              \
	in_dword_masked(VIDC_REG_982553_ADDR,  m)
#define VIDC_REG_982553_OPERATION_DONE_BMSK        0x1
#define VIDC_REG_982553_OPERATION_DONE_SHFT          0

#define VIDC_REG_259967_ADDR                       \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000510)
#define VIDC_REG_259967_PHYS                       \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000510)
#define VIDC_REG_259967_RMSK                              0x1
#define VIDC_REG_259967_SHFT                                0
#define VIDC_REG_259967_IN                         \
	in_dword_masked(VIDC_REG_259967_ADDR,  VIDC_REG_259967_RMSK)
#define VIDC_REG_259967_INM(m)                     \
	in_dword_masked(VIDC_REG_259967_ADDR,  m)
#define VIDC_REG_259967_FW_DONE_BMSK                      0x1
#define VIDC_REG_259967_FW_DONE_SHFT                        0

#define VIDC_REG_512143_ADDR                    \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000514)
#define VIDC_REG_512143_PHYS                    \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000514)
#define VIDC_REG_512143_RMSK                         0x1f8
#define VIDC_REG_512143_SHFT                             0
#define VIDC_REG_512143_IN                      \
	in_dword_masked(VIDC_REG_512143_ADDR,  \
			VIDC_REG_512143_RMSK)
#define VIDC_REG_512143_INM(m)                  \
	in_dword_masked(VIDC_REG_512143_ADDR,  m)
#define VIDC_REG_512143_FRAME_DONE_STAT_BMSK         0x100
#define VIDC_REG_512143_FRAME_DONE_STAT_SHFT           0x8
#define VIDC_REG_512143_DMA_DONE_STAT_BMSK            0x80
#define VIDC_REG_512143_DMA_DONE_STAT_SHFT             0x7
#define VIDC_REG_512143_HEADER_DONE_STAT_BMSK         0x40
#define VIDC_REG_512143_HEADER_DONE_STAT_SHFT          0x6
#define VIDC_REG_512143_FW_DONE_STAT_BMSK             0x20
#define VIDC_REG_512143_FW_DONE_STAT_SHFT              0x5
#define VIDC_REG_512143_OPERATION_FAILED_BMSK         0x10
#define VIDC_REG_512143_OPERATION_FAILED_SHFT          0x4
#define VIDC_REG_512143_STREAM_HDR_CHANGED_BMSK        0x8
#define VIDC_REG_512143_STREAM_HDR_CHANGED_SHFT        0x3

#define VIDC_REG_418173_ADDR                \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000518)
#define VIDC_REG_418173_PHYS                \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000518)
#define VIDC_REG_418173_RMSK                     0x1fa
#define VIDC_REG_418173_SHFT                         0
#define VIDC_REG_418173_IN                  \
	in_dword_masked(VIDC_REG_418173_ADDR,  \
			VIDC_REG_418173_RMSK)
#define VIDC_REG_418173_INM(m)              \
	in_dword_masked(VIDC_REG_418173_ADDR,  m)
#define VIDC_REG_418173_OUT(v)              \
	out_dword(VIDC_REG_418173_ADDR, v)
#define VIDC_REG_418173_OUTM(m, v)           \
do { \
	out_dword_masked_ns(VIDC_REG_418173_ADDR, m, v, \
			VIDC_REG_418173_IN); \
} while (0)
#define VIDC_REG_418173_FRAME_DONE_ENABLE_BMSK      0x100
#define VIDC_REG_418173_FRAME_DONE_ENABLE_SHFT        0x8
#define VIDC_REG_418173_DMA_DONE_ENABLE_BMSK       0x80
#define VIDC_REG_418173_DMA_DONE_ENABLE_SHFT        0x7
#define VIDC_REG_418173_HEADER_DONE_ENABLE_BMSK       0x40
#define VIDC_REG_418173_HEADER_DONE_ENABLE_SHFT        0x6
#define VIDC_REG_418173_FW_DONE_ENABLE_BMSK       0x20
#define VIDC_REG_418173_FW_DONE_ENABLE_SHFT        0x5
#define VIDC_REG_418173_OPERATION_FAILED_ENABLE_BMSK       0x10
#define VIDC_REG_418173_OPERATION_FAILED_ENABLE_SHFT        0x4
#define VIDC_REG_418173_STREAM_HDR_CHANGED_ENABLE_BMSK        0x8
#define VIDC_REG_418173_STREAM_HDR_CHANGED_ENABLE_SHFT        0x3
#define VIDC_REG_418173_BUFFER_FULL_ENABLE_BMSK        0x2
#define VIDC_REG_418173_BUFFER_FULL_ENABLE_SHFT        0x1

#define VIDC_REG_841539_ADDR                \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000600)
#define VIDC_REG_841539_PHYS                \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000600)
#define VIDC_REG_841539_RMSK                       0x3
#define VIDC_REG_841539_SHFT                         0
#define VIDC_REG_841539_IN                  \
	in_dword_masked(VIDC_REG_841539_ADDR,  \
			VIDC_REG_841539_RMSK)
#define VIDC_REG_841539_INM(m)              \
	in_dword_masked(VIDC_REG_841539_ADDR,  m)
#define VIDC_REG_841539_OUT(v)              \
	out_dword(VIDC_REG_841539_ADDR, v)
#define VIDC_REG_841539_OUTM(m, v)           \
do { \
	out_dword_masked_ns(VIDC_REG_841539_ADDR, m, v, \
			VIDC_REG_841539_IN); \
} while (0)
#define VIDC_REG_841539_TILE_MODE_BMSK             0x3
#define VIDC_REG_841539_TILE_MODE_SHFT               0

#define VIDC_REG_99105_ADDR                \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000800)
#define VIDC_REG_99105_PHYS                \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000800)
#define VIDC_REG_99105_RMSK                0xffffffff
#define VIDC_REG_99105_SHFT                         0
#define VIDC_REG_99105_IN                  \
	in_dword_masked(VIDC_REG_99105_ADDR,  \
			VIDC_REG_99105_RMSK)
#define VIDC_REG_99105_INM(m)              \
	in_dword_masked(VIDC_REG_99105_ADDR,  m)
#define VIDC_REG_99105_OUT(v)              \
	out_dword(VIDC_REG_99105_ADDR, v)
#define VIDC_REG_99105_OUTM(m, v)           \
do { \
	out_dword_masked_ns(VIDC_REG_99105_ADDR, m, v, \
			VIDC_REG_99105_IN); \
} while (0)
#define VIDC_REG_99105_ENC_CUR_Y_ADDR_BMSK 0xffffffff
#define VIDC_REG_99105_ENC_CUR_Y_ADDR_SHFT          0

#define VIDC_REG_777113_ADDR_ADDR                \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000804)
#define VIDC_REG_777113_ADDR_PHYS                \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000804)
#define VIDC_REG_777113_ADDR_RMSK                0xffffffff
#define VIDC_REG_777113_ADDR_SHFT                         0
#define VIDC_REG_777113_ADDR_IN                  \
	in_dword_masked(VIDC_REG_777113_ADDR_ADDR,  \
			VIDC_REG_777113_ADDR_RMSK)
#define VIDC_REG_777113_ADDR_INM(m)              \
	in_dword_masked(VIDC_REG_777113_ADDR_ADDR,  m)
#define VIDC_REG_777113_ADDR_OUT(v)              \
	out_dword(VIDC_REG_777113_ADDR_ADDR, v)
#define VIDC_REG_777113_ADDR_OUTM(m, v)           \
do { \
	out_dword_masked_ns(VIDC_REG_777113_ADDR_ADDR, m, v, \
			VIDC_REG_777113_ADDR_IN); \
} while (0)
#define VIDC_REG_777113_ADDR_ENC_CUR_C_ADDR_BMSK 0xffffffff
#define VIDC_REG_777113_ADDR_ENC_CUR_C_ADDR_SHFT          0

#define VIDC_REG_341928_ADDR_ADDR                  \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x0000080c)
#define VIDC_REG_341928_ADDR_PHYS                  \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x0000080c)
#define VIDC_REG_341928_ADDR_RMSK                  0xffffffff
#define VIDC_REG_341928_ADDR_SHFT                           0
#define VIDC_REG_341928_ADDR_IN                    \
	in_dword_masked(VIDC_REG_341928_ADDR_ADDR,  \
			VIDC_REG_341928_ADDR_RMSK)
#define VIDC_REG_341928_ADDR_INM(m)                \
	in_dword_masked(VIDC_REG_341928_ADDR_ADDR,  m)
#define VIDC_REG_341928_ADDR_OUT(v)                \
	out_dword(VIDC_REG_341928_ADDR_ADDR, v)
#define VIDC_REG_341928_ADDR_OUTM(m, v)             \
do { \
	out_dword_masked_ns(VIDC_REG_341928_ADDR_ADDR, m, v, \
			VIDC_REG_341928_ADDR_IN); \
} while (0)
#define VIDC_REG_341928_ADDR_ENC_DPB_ADR_BMSK      0xffffffff
#define VIDC_REG_341928_ADDR_ENC_DPB_ADR_SHFT               0

#define VIDC_REG_857491_ADDR                    \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000810)
#define VIDC_REG_857491_PHYS                    \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000810)
#define VIDC_REG_857491_RMSK                         0xfff
#define VIDC_REG_857491_SHFT                             0
#define VIDC_REG_857491_IN                      \
	in_dword_masked(VIDC_REG_857491_ADDR,  \
			VIDC_REG_857491_RMSK)
#define VIDC_REG_857491_INM(m)                  \
	in_dword_masked(VIDC_REG_857491_ADDR,  m)
#define VIDC_REG_857491_OUT(v)                  \
	out_dword(VIDC_REG_857491_ADDR, v)
#define VIDC_REG_857491_OUTM(m, v)               \
do { \
	out_dword_masked_ns(VIDC_REG_857491_ADDR, m, v, \
			VIDC_REG_857491_IN); \
} while (0)
#define VIDC_REG_857491_CIR_MB_NUM_BMSK              0xfff
#define VIDC_REG_857491_CIR_MB_NUM_SHFT                  0

#define VIDC_REG_518133_ADDR                  \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000900)
#define VIDC_REG_518133_PHYS                  \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000900)
#define VIDC_REG_518133_RMSK                  0xffffffff
#define VIDC_REG_518133_SHFT                           0
#define VIDC_REG_518133_IN                    \
	in_dword_masked(VIDC_REG_518133_ADDR,  \
			VIDC_REG_518133_RMSK)
#define VIDC_REG_518133_INM(m)                \
	in_dword_masked(VIDC_REG_518133_ADDR,  m)
#define VIDC_REG_518133_OUT(v)                \
	out_dword(VIDC_REG_518133_ADDR, v)
#define VIDC_REG_518133_OUTM(m, v)             \
do { \
	out_dword_masked_ns(VIDC_REG_518133_ADDR, m, v, \
			VIDC_REG_518133_IN); \
} while (0)
#define VIDC_REG_518133_DEC_DPB_ADDR_BMSK     0xffffffff
#define VIDC_REG_518133_DEC_DPB_ADDR_SHFT              0

#define VIDC_REG_456376_ADDR_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000904)
#define VIDC_REG_456376_ADDR_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000904)
#define VIDC_REG_456376_ADDR_RMSK                 0xffffffff
#define VIDC_REG_456376_ADDR_SHFT                          0
#define VIDC_REG_456376_ADDR_IN                   \
	in_dword_masked(VIDC_REG_456376_ADDR_ADDR,  \
			VIDC_REG_456376_ADDR_RMSK)
#define VIDC_REG_456376_ADDR_INM(m)               \
	in_dword_masked(VIDC_REG_456376_ADDR_ADDR,  m)
#define VIDC_REG_456376_ADDR_OUT(v)               \
	out_dword(VIDC_REG_456376_ADDR_ADDR, v)
#define VIDC_REG_456376_ADDR_OUTM(m, v)            \
do { \
	out_dword_masked_ns(VIDC_REG_456376_ADDR_ADDR, m, v, \
			VIDC_REG_456376_ADDR_IN); \
} while (0)
#define VIDC_REG_456376_ADDR_DPB_COMV_ADDR_BMSK   0xffffffff
#define VIDC_REG_456376_ADDR_DPB_COMV_ADDR_SHFT            0

#define VIDC_REG_267567_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000908)
#define VIDC_REG_267567_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000908)
#define VIDC_REG_267567_RMSK                 0xffffffff
#define VIDC_REG_267567_SHFT                          0
#define VIDC_REG_267567_IN                   \
	in_dword_masked(VIDC_REG_267567_ADDR,  \
			VIDC_REG_267567_RMSK)
#define VIDC_REG_267567_INM(m)               \
	in_dword_masked(VIDC_REG_267567_ADDR,  m)
#define VIDC_REG_267567_OUT(v)               \
	out_dword(VIDC_REG_267567_ADDR, v)
#define VIDC_REG_267567_OUTM(m, v)            \
do { \
	out_dword_masked_ns(VIDC_REG_267567_ADDR, m, v, \
			VIDC_REG_267567_IN); \
} while (0)
#define VIDC_REG_798486_ADDR_BMSK   0xffffffff
#define VIDC_REG_798486_ADDR_SHFT            0

#define VIDC_REG_105770_ADDR                      \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x0000090c)
#define VIDC_REG_105770_PHYS                      \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x0000090c)
#define VIDC_REG_105770_RMSK                            0xff
#define VIDC_REG_105770_SHFT                               0
#define VIDC_REG_105770_IN                        \
	in_dword_masked(VIDC_REG_105770_ADDR,         \
	VIDC_REG_105770_RMSK)
#define VIDC_REG_105770_INM(m)                    \
	in_dword_masked(VIDC_REG_105770_ADDR,  m)
#define VIDC_REG_105770_DPB_SIZE_BMSK                   0xff
#define VIDC_REG_105770_DPB_SIZE_SHFT                      0

#define VIDC_REG_58211_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000a00)
#define VIDC_REG_58211_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000a00)
#define VIDC_REG_58211_RMSK                      0x33f
#define VIDC_REG_58211_SHFT                          0
#define VIDC_REG_58211_IN                   \
	in_dword_masked(VIDC_REG_58211_ADDR,  \
			VIDC_REG_58211_RMSK)
#define VIDC_REG_58211_INM(m)               \
	in_dword_masked(VIDC_REG_58211_ADDR,  m)
#define VIDC_REG_58211_OUT(v)               \
	out_dword(VIDC_REG_58211_ADDR, v)
#define VIDC_REG_58211_OUTM(m, v)            \
do { \
	out_dword_masked_ns(VIDC_REG_58211_ADDR, m, v, \
			VIDC_REG_58211_IN); \
} while (0)
#define VIDC_REG_58211_FR_RC_EN_BMSK             0x200
#define VIDC_REG_58211_FR_RC_EN_SHFT               0x9
#define VIDC_REG_58211_MB_RC_EN_BMSK             0x100
#define VIDC_REG_58211_MB_RC_EN_SHFT               0x8
#define VIDC_REG_58211_FRAME_QP_BMSK              0x3f
#define VIDC_REG_58211_FRAME_QP_SHFT                 0

#define VIDC_REG_548359_ADDR                    \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000a04)
#define VIDC_REG_548359_PHYS                    \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000a04)
#define VIDC_REG_548359_RMSK                          0x3f
#define VIDC_REG_548359_SHFT                             0
#define VIDC_REG_548359_IN                      \
	in_dword_masked(VIDC_REG_548359_ADDR,  \
			VIDC_REG_548359_RMSK)
#define VIDC_REG_548359_INM(m)                  \
	in_dword_masked(VIDC_REG_548359_ADDR,  m)
#define VIDC_REG_548359_OUT(v)                  \
	out_dword(VIDC_REG_548359_ADDR, v)
#define VIDC_REG_548359_OUTM(m, v)               \
do { \
	out_dword_masked_ns(VIDC_REG_548359_ADDR, m, v, \
			VIDC_REG_548359_IN); \
} while (0)
#define VIDC_REG_548359_P_FRAME_QP_BMSK               0x3f
#define VIDC_REG_548359_P_FRAME_QP_SHFT                  0

#define VIDC_REG_174150_ADDR                   \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000a08)
#define VIDC_REG_174150_PHYS                   \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000a08)
#define VIDC_REG_174150_RMSK                   0xffffffff
#define VIDC_REG_174150_SHFT                            0
#define VIDC_REG_174150_IN                     \
	in_dword_masked(VIDC_REG_174150_ADDR,  \
			VIDC_REG_174150_RMSK)
#define VIDC_REG_174150_INM(m)                 \
	in_dword_masked(VIDC_REG_174150_ADDR,  m)
#define VIDC_REG_174150_OUT(v)                 \
	out_dword(VIDC_REG_174150_ADDR, v)
#define VIDC_REG_174150_OUTM(m, v)              \
do { \
	out_dword_masked_ns(VIDC_REG_174150_ADDR, m, v, \
			VIDC_REG_174150_IN); \
} while (0)
#define VIDC_REG_174150_BIT_RATE_BMSK          0xffffffff
#define VIDC_REG_174150_BIT_RATE_SHFT                   0

#define VIDC_REG_734318_ADDR                     \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000a0c)
#define VIDC_REG_734318_PHYS                     \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000a0c)
#define VIDC_REG_734318_RMSK                         0x3f3f
#define VIDC_REG_734318_SHFT                              0
#define VIDC_REG_734318_IN                       \
	in_dword_masked(VIDC_REG_734318_ADDR,        \
	VIDC_REG_734318_RMSK)
#define VIDC_REG_734318_INM(m)                   \
	in_dword_masked(VIDC_REG_734318_ADDR,  m)
#define VIDC_REG_734318_OUT(v)                   \
	out_dword(VIDC_REG_734318_ADDR, v)
#define VIDC_REG_734318_OUTM(m, v)                \
do { \
	out_dword_masked_ns(VIDC_REG_734318_ADDR, m, v, \
			VIDC_REG_734318_IN); \
} while (0)
#define VIDC_REG_734318_MAX_QP_BMSK                  0x3f00
#define VIDC_REG_734318_MAX_QP_SHFT                     0x8
#define VIDC_REG_734318_MIN_QP_BMSK                    0x3f
#define VIDC_REG_734318_MIN_QP_SHFT                       0

#define VIDC_REG_677784_ADDR                      \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000a10)
#define VIDC_REG_677784_PHYS                      \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000a10)
#define VIDC_REG_677784_RMSK                          0xffff
#define VIDC_REG_677784_SHFT                               0
#define VIDC_REG_677784_IN                        \
	in_dword_masked(VIDC_REG_677784_ADDR,         \
	VIDC_REG_677784_RMSK)
#define VIDC_REG_677784_INM(m)                    \
	in_dword_masked(VIDC_REG_677784_ADDR,  m)
#define VIDC_REG_677784_OUT(v)                    \
	out_dword(VIDC_REG_677784_ADDR, v)
#define VIDC_REG_677784_OUTM(m, v)                 \
do { \
	out_dword_masked_ns(VIDC_REG_677784_ADDR, m, v, \
			VIDC_REG_677784_IN); \
} while (0)
#define VIDC_REG_677784_REACT_PARA_BMSK               0xffff
#define VIDC_REG_677784_REACT_PARA_SHFT                    0

#define VIDC_REG_995041_ADDR                    \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000a14)
#define VIDC_REG_995041_PHYS                    \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000a14)
#define VIDC_REG_995041_RMSK                           0xf
#define VIDC_REG_995041_SHFT                             0
#define VIDC_REG_995041_IN                      \
	in_dword_masked(VIDC_REG_995041_ADDR,  \
			VIDC_REG_995041_RMSK)
#define VIDC_REG_995041_INM(m)                  \
	in_dword_masked(VIDC_REG_995041_ADDR,  m)
#define VIDC_REG_995041_OUT(v)                  \
	out_dword(VIDC_REG_995041_ADDR, v)
#define VIDC_REG_995041_OUTM(m, v)               \
do { \
	out_dword_masked_ns(VIDC_REG_995041_ADDR, m, v, \
			VIDC_REG_995041_IN); \
} while (0)
#define VIDC_REG_995041_DARK_DISABLE_BMSK              0x8
#define VIDC_REG_995041_DARK_DISABLE_SHFT              0x3
#define VIDC_REG_995041_SMOOTH_DISABLE_BMSK            0x4
#define VIDC_REG_995041_SMOOTH_DISABLE_SHFT            0x2
#define VIDC_REG_995041_STATIC_DISABLE_BMSK            0x2
#define VIDC_REG_995041_STATIC_DISABLE_SHFT            0x1
#define VIDC_REG_995041_ACT_DISABLE_BMSK               0x1
#define VIDC_REG_995041_ACT_DISABLE_SHFT                 0

#define VIDC_REG_273649_ADDR                       \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000a18)
#define VIDC_REG_273649_PHYS                       \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000a18)
#define VIDC_REG_273649_RMSK                             0x3f
#define VIDC_REG_273649_SHFT                                0
#define VIDC_REG_273649_IN                         \
	in_dword_masked(VIDC_REG_273649_ADDR,  VIDC_REG_273649_RMSK)
#define VIDC_REG_273649_INM(m)                     \
	in_dword_masked(VIDC_REG_273649_ADDR,  m)
#define VIDC_REG_273649_QP_OUT_BMSK                      0x3f
#define VIDC_REG_273649_QP_OUT_SHFT                         0

#define VIDC_REG_548823_ADDR                   \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000b00)
#define VIDC_REG_548823_PHYS                   \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000b00)
#define VIDC_REG_548823_RMSK                   0xffffffff
#define VIDC_REG_548823_SHFT                            0
#define VIDC_REG_548823_IN                     \
	in_dword_masked(VIDC_REG_548823_ADDR,  \
			VIDC_REG_548823_RMSK)
#define VIDC_REG_548823_INM(m)                 \
	in_dword_masked(VIDC_REG_548823_ADDR,  m)
#define VIDC_REG_548823_720P_VERSION_BMSK       0xffffffff
#define VIDC_REG_548823_720P_VERSION_SHFT                0

#define VIDC_REG_881638_ADDR                     \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000c00)
#define VIDC_REG_881638_PHYS                     \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000c00)
#define VIDC_REG_881638_RMSK                     0xffffffff
#define VIDC_REG_881638_SHFT                              0
#define VIDC_REG_881638_IN                       \
	in_dword_masked(VIDC_REG_881638_ADDR,        \
	VIDC_REG_881638_RMSK)
#define VIDC_REG_881638_INM(m)                   \
	in_dword_masked(VIDC_REG_881638_ADDR,  m)
#define VIDC_REG_881638_CROP_RIGHT_OFFSET_BMSK   0xffff0000
#define VIDC_REG_881638_CROP_RIGHT_OFFSET_SHFT         0x10
#define VIDC_REG_881638_CROP_LEFT_OFFSET_BMSK        0xffff
#define VIDC_REG_881638_CROP_LEFT_OFFSET_SHFT             0

#define VIDC_REG_161486_ADDR                     \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000c04)
#define VIDC_REG_161486_PHYS                     \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000c04)
#define VIDC_REG_161486_RMSK                     0xffffffff
#define VIDC_REG_161486_SHFT                              0
#define VIDC_REG_161486_IN                       \
	in_dword_masked(VIDC_REG_161486_ADDR,        \
	VIDC_REG_161486_RMSK)
#define VIDC_REG_161486_INM(m)                   \
	in_dword_masked(VIDC_REG_161486_ADDR,  m)
#define VIDC_REG_161486_CROP_BOTTOM_OFFSET_BMSK  0xffff0000
#define VIDC_REG_161486_CROP_BOTTOM_OFFSET_SHFT        0x10
#define VIDC_REG_161486_CROP_TOP_OFFSET_BMSK         0xffff
#define VIDC_REG_161486_CROP_TOP_OFFSET_SHFT              0

#define VIDC_REG_580603_ADDR              \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000c08)
#define VIDC_REG_580603_PHYS              \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000c08)
#define VIDC_REG_580603_RMSK              0xffffffff
#define VIDC_REG_580603_SHFT                       0
#define VIDC_REG_580603_IN                \
	in_dword_masked(VIDC_REG_580603_ADDR,  \
			VIDC_REG_580603_RMSK)
#define VIDC_REG_580603_INM(m)            \
	in_dword_masked(VIDC_REG_580603_ADDR,  m)
#define VIDC_REG_580603_720P_DEC_FRM_SIZE_BMSK 0xffffffff
#define VIDC_REG_580603_720P_DEC_FRM_SIZE_SHFT          0


#define VIDC_REG_606447_ADDR \
		(VIDC_720P_WRAPPER_REG_BASE + 0x00000c0c)
#define VIDC_REG_606447_PHYS \
		(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000c0c)
#define VIDC_REG_606447_RMSK  0xff1f
#define VIDC_REG_606447_SHFT  0
#define VIDC_REG_606447_IN                         \
		in_dword_masked(VIDC_REG_606447_ADDR, \
		VIDC_REG_606447_RMSK)
#define VIDC_REG_606447_INM(m)                     \
		in_dword_masked(VIDC_REG_606447_ADDR, m)
#define VIDC_REG_606447_OUT(v)                     \
		out_dword(VIDC_REG_606447_ADDR, v)
#define VIDC_REG_606447_OUTM(m, v)                  \
		out_dword_masked_ns(VIDC_REG_606447_ADDR, \
		m, v, VIDC_REG_606447_IN); \

#define VIDC_REG_606447_DIS_PIC_LEVEL_BMSK 0xff00
#define VIDC_REG_606447_DIS_PIC_LEVEL_SHFT 0x8
#define VIDC_REG_606447_DISP_PIC_PROFILE_BMSK 0x1f
#define VIDC_REG_606447_DISP_PIC_PROFILE_SHFT 0

#define VIDC_REG_854281_ADDR \
		(VIDC_720P_WRAPPER_REG_BASE      + 0x00000c10)
#define VIDC_REG_854281_PHYS \
		(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000c10)
#define VIDC_REG_854281_RMSK 0xffffffff
#define VIDC_REG_854281_SHFT 0
#define VIDC_REG_854281_IN \
		in_dword_masked(VIDC_REG_854281_ADDR, \
		VIDC_REG_854281_RMSK)
#define VIDC_REG_854281_INM(m) \
		in_dword_masked(VIDC_REG_854281_ADDR, m)
#define VIDC_REG_854281_MIN_DPB_SIZE_BMSK 0xffffffff
#define VIDC_REG_854281_MIN_DPB_SIZE_SHFT 0


#define VIDC_REG_381535_ADDR \
		(VIDC_720P_WRAPPER_REG_BASE + 0x00000c14)
#define VIDC_REG_381535_PHYS \
		(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000c14)
#define VIDC_REG_381535_RMSK 0xffffffff
#define VIDC_REG_381535_SHFT 0
#define VIDC_REG_381535_IN \
		in_dword_masked(VIDC_REG_381535_ADDR, \
		VIDC_REG_381535_RMSK)
#define VIDC_REG_381535_INM(m) \
		in_dword_masked(VIDC_REG_381535_ADDR, m)
#define VIDC_REG_381535_720P_FW_STATUS_BMSK 0xffffffff
#define VIDC_REG_381535_720P_FW_STATUS_SHFT 0


#define VIDC_REG_347105_ADDR \
		(VIDC_720P_WRAPPER_REG_BASE + 0x00000c18)
#define VIDC_REG_347105_PHYS \
		(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000c18)
#define VIDC_REG_347105_RMSK 0xffffffff
#define VIDC_REG_347105_SHFT 0
#define VIDC_REG_347105_IN \
		in_dword_masked(VIDC_REG_347105_ADDR, \
		VIDC_REG_347105_RMSK)
#define VIDC_REG_347105_INM(m) \
		in_dword_masked(VIDC_REG_347105_ADDR, m)
#define VIDC_REG_347105_FREE_LUMA_DPB_BMSK 0xffffffff
#define VIDC_REG_347105_FREE_LUMA_DPB_SHFT 0


#define VIDC_REG_62325_ADDR              \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000d00)
#define VIDC_REG_62325_PHYS              \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000d00)
#define VIDC_REG_62325_RMSK                     0xf
#define VIDC_REG_62325_SHFT                       0
#define VIDC_REG_62325_IN                \
		in_dword_masked(VIDC_REG_62325_ADDR,  \
		VIDC_REG_62325_RMSK)
#define VIDC_REG_62325_INM(m)            \
	in_dword_masked(VIDC_REG_62325_ADDR,  m)
#define VIDC_REG_62325_OUT(v)            \
	out_dword(VIDC_REG_62325_ADDR, v)
#define VIDC_REG_62325_OUTM(m, v)         \
do { \
	out_dword_masked_ns(VIDC_REG_62325_ADDR, m, v, \
			VIDC_REG_62325_IN); \
} while (0)
#define VIDC_REG_62325_COMMAND_TYPE_BMSK        0xf
#define VIDC_REG_62325_COMMAND_TYPE_SHFT          0

#define VIDC_REG_101184_ADDR  \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000d04)
#define VIDC_REG_101184_PHYS  \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000d04)
#define VIDC_REG_101184_RMSK                0xffffffff
#define VIDC_REG_101184_SHFT                0
#define VIDC_REG_101184_OUT(v)                     \
	out_dword(VIDC_REG_101184_ADDR, v)

#define VIDC_REG_490443_ADDR  \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000d08)
#define VIDC_REG_490443_PHYS  \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000d08)
#define VIDC_REG_490443_RMSK                       \
	0xffffffff
#define \
	\
VIDC_REG_490443_SHFT                                0
#define VIDC_REG_490443_OUT(v)                     \
	out_dword(VIDC_REG_490443_ADDR, v)

#define VIDC_REG_625444_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000d14)
#define VIDC_REG_625444_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000d14)
#define VIDC_REG_625444_RMSK                 0xffffffff
#define VIDC_REG_625444_SHFT                          0
#define VIDC_REG_625444_IN                   \
	in_dword_masked(VIDC_REG_625444_ADDR,  \
			VIDC_REG_625444_RMSK)
#define VIDC_REG_625444_INM(m)               \
	in_dword_masked(VIDC_REG_625444_ADDR,  m)
#define VIDC_REG_625444_OUT(v)               \
	out_dword(VIDC_REG_625444_ADDR, v)
#define VIDC_REG_625444_OUTM(m, v)            \
do { \
	out_dword_masked_ns(VIDC_REG_625444_ADDR, m, v, \
			VIDC_REG_625444_IN); \
} while (0)
#define VIDC_REG_625444_FRAME_RATE_BMSK      0xffffffff
#define VIDC_REG_625444_FRAME_RATE_SHFT               0

#define VIDC_REG_639999_ADDR                    \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000d20)
#define VIDC_REG_639999_PHYS                    \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000d20)
#define VIDC_REG_639999_RMSK                    0xffff
#define VIDC_REG_639999_SHFT                         0
#define VIDC_REG_639999_OUT(v)                  \
	out_dword(VIDC_REG_639999_ADDR, v)

#define VIDC_REG_64895_ADDR                    \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000e00)
#define VIDC_REG_64895_PHYS                    \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000e00)
#define VIDC_REG_64895_RMSK                    0xffffffff
#define VIDC_REG_64895_SHFT                             0
#define VIDC_REG_64895_OUT(v)                  \
	out_dword(VIDC_REG_64895_ADDR, v)

#define VIDC_REG_965480_ADDR \
		(VIDC_720P_WRAPPER_REG_BASE + 0x00000e04)
#define VIDC_REG_965480_PHYS \
		(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000e04)
#define VIDC_REG_965480_RMSK 0x1
#define VIDC_REG_965480_SHFT 0
#define VIDC_REG_965480_OUT(v) \
		out_dword(VIDC_REG_965480_ADDR, v)

#define VIDC_REG_804959_ADDR              \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000e08)
#define VIDC_REG_804959_PHYS              \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000e08)
#define VIDC_REG_804959_RMSK                     0x7
#define VIDC_REG_804959_SHFT                       0
#define VIDC_REG_804959_OUT(v)            \
	out_dword(VIDC_REG_804959_ADDR, v)

#define VIDC_REG_257463_ADDR                   \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000e10)
#define VIDC_REG_257463_PHYS                   \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000e10)
#define VIDC_REG_257463_RMSK                   0xffffffff
#define VIDC_REG_257463_SHFT                            0
#define VIDC_REG_257463_IN                     \
	in_dword_masked(VIDC_REG_257463_ADDR,  \
			VIDC_REG_257463_RMSK)
#define VIDC_REG_257463_INM(m)                 \
	in_dword_masked(VIDC_REG_257463_ADDR,  m)
#define VIDC_REG_257463_MIN_NUM_DPB_BMSK       0xffffffff
#define VIDC_REG_257463_MIN_NUM_DPB_SHFT                0

#define VIDC_REG_883500_ADDR                       \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000e14)
#define VIDC_REG_883500_PHYS                       \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000e14)
#define VIDC_REG_883500_RMSK                       0xffffffff
#define VIDC_REG_883500_SHFT                                0
#define VIDC_REG_883500_OUT(v)                     \
	out_dword(VIDC_REG_883500_ADDR, v)

#define VIDC_REG_615716_ADDR(n)               \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000e18 + 4 * (n))
#define VIDC_REG_615716_PHYS(n)               \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000e18 + 4 * (n))
#define VIDC_REG_615716_RMSK                  0xffffffff
#define VIDC_REG_615716_SHFT                           0
#define VIDC_REG_615716_OUTI(n, v) \
	out_dword(VIDC_REG_615716_ADDR(n), v)

#define VIDC_REG_603032_ADDR                \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000e98)
#define VIDC_REG_603032_PHYS                \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000e98)
#define VIDC_REG_603032_RMSK                0xffffffff
#define VIDC_REG_603032_SHFT                         0
#define VIDC_REG_603032_OUT(v)              \
	out_dword(VIDC_REG_603032_ADDR, v)

#define VIDC_REG_300310_ADDR                  \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000e9c)
#define VIDC_REG_300310_PHYS                  \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000e9c)
#define VIDC_REG_300310_RMSK                  0xffffffff
#define VIDC_REG_300310_SHFT                           0
#define VIDC_REG_300310_IN                    \
	in_dword_masked(VIDC_REG_300310_ADDR,  \
			VIDC_REG_300310_RMSK)
#define VIDC_REG_300310_INM(m)                \
	in_dword_masked(VIDC_REG_300310_ADDR,  m)
#define VIDC_REG_300310_ERROR_STATUS_BMSK     0xffffffff
#define VIDC_REG_300310_ERROR_STATUS_SHFT              0

#define VIDC_REG_792026_ADDR        \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000ea0)
#define VIDC_REG_792026_PHYS        \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000ea0)
#define VIDC_REG_792026_RMSK        0xffffffff
#define VIDC_REG_792026_SHFT                 0
#define VIDC_REG_792026_OUT(v)      \
	out_dword(VIDC_REG_792026_ADDR, v)

#define VIDC_REG_844152_ADDR        \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000ea4)
#define VIDC_REG_844152_PHYS        \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000ea4)
#define VIDC_REG_844152_RMSK        0xffffffff
#define VIDC_REG_844152_SHFT                 0
#define VIDC_REG_844152_OUT(v)      \
	out_dword(VIDC_REG_844152_ADDR, v)

#define VIDC_REG_370409_ADDR            \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000ea8)
#define VIDC_REG_370409_PHYS                \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000ea8)
#define VIDC_REG_370409_RMSK                0xffffffff
#define VIDC_REG_370409_SHFT                         0
#define VIDC_REG_370409_IN                  \
	in_dword_masked(VIDC_REG_370409_ADDR,  \
			VIDC_REG_370409_RMSK)
#define VIDC_REG_370409_INM(m)              \
	in_dword_masked(VIDC_REG_370409_ADDR,  m)
#define VIDC_REG_370409_GET_FRAME_TAG_TOP_BMSK 0xffffffff
#define VIDC_REG_370409_GET_FRAME_TAG_TOP_SHFT          0

#define VIDC_REG_147682_ADDR               \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000eac)
#define VIDC_REG_147682_PHYS               \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000eac)
#define VIDC_REG_147682_RMSK                        0x1
#define VIDC_REG_147682_SHFT                        0
#define VIDC_REG_147682_OUT(v)             \
	out_dword(VIDC_REG_147682_ADDR, v)

#define VIDC_REG_407718_ADDR                    \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000eb0)
#define VIDC_REG_407718_PHYS                    \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000eb0)
#define VIDC_REG_407718_RMSK                    0xffffffff
#define VIDC_REG_407718_SHFT                             0
#define VIDC_REG_407718_OUT(v)                  \
	out_dword(VIDC_REG_407718_ADDR, v)

#define VIDC_REG_697961_ADDR \
		(VIDC_720P_WRAPPER_REG_BASE + 0x00000eb4)
#define VIDC_REG_697961_PHYS \
		(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000eb4)
#define VIDC_REG_697961_RMSK 0x3
#define VIDC_REG_697961_SHFT 0
#define VIDC_REG_697961_IN \
		in_dword_masked(VIDC_REG_697961_ADDR, \
		VIDC_REG_697961_RMSK)
#define VIDC_REG_697961_INM(m) \
		in_dword_masked(VIDC_REG_697961_ADDR, m)
#define VIDC_REG_697961_FRAME_TYPE_BMSK 0x3
#define VIDC_REG_697961_FRAME_TYPE_SHFT 0


#define VIDC_REG_613254_ADDR               \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000eb8)
#define VIDC_REG_613254_PHYS               \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000eb8)
#define VIDC_REG_613254_RMSK                      0x1
#define VIDC_REG_613254_SHFT                        0
#define VIDC_REG_613254_IN                 \
	in_dword_masked(VIDC_REG_613254_ADDR,  \
			VIDC_REG_613254_RMSK)
#define VIDC_REG_613254_INM(m)             \
	in_dword_masked(VIDC_REG_613254_ADDR,  m)
#define VIDC_REG_613254_METADATA_STATUS_BMSK        0x1
#define VIDC_REG_613254_METADATA_STATUS_SHFT          0
#define VIDC_REG_441270_ADDR                    \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000ebc)
#define VIDC_REG_441270_PHYS                    \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000ebc)
#define VIDC_REG_441270_RMSK                          0x1f
#define VIDC_REG_441270_SHFT                             0
#define VIDC_REG_441270_IN                      \
	in_dword_masked(VIDC_REG_441270_ADDR,  \
			VIDC_REG_441270_RMSK)
#define VIDC_REG_441270_INM(m)                  \
	in_dword_masked(VIDC_REG_441270_ADDR,  m)
#define VIDC_REG_441270_DATA_PARTITIONED_BMSK 0x8
#define VIDC_REG_441270_DATA_PARTITIONED_SHFT 0x3

#define VIDC_REG_441270_FRAME_TYPE_BMSK               0x17
#define VIDC_REG_441270_FRAME_TYPE_SHFT                  0

#define VIDC_REG_724381_ADDR        \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000ec0)
#define VIDC_REG_724381_PHYS        \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000ec0)
#define VIDC_REG_724381_RMSK               0x3
#define VIDC_REG_724381_SHFT                 0
#define VIDC_REG_724381_IN          \
	in_dword_masked(VIDC_REG_724381_ADDR,  \
			VIDC_REG_724381_RMSK)
#define VIDC_REG_724381_INM(m)      \
	in_dword_masked(VIDC_REG_724381_ADDR,  m)
#define VIDC_REG_724381_MORE_FIELD_NEEDED_BMSK       0x4
#define VIDC_REG_724381_MORE_FIELD_NEEDED_SHFT       0x2
#define VIDC_REG_724381_OPERATION_FAILED_BMSK        0x2
#define VIDC_REG_724381_OPERATION_FAILED_SHFT        0x1
#define VIDC_REG_724381_RESOLUTION_CHANGE_BMSK       0x1
#define VIDC_REG_724381_RESOLUTION_CHANGE_SHFT         0

#define VIDC_REG_854681_ADDR               \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000ec4)
#define VIDC_REG_854681_PHYS               \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000ec4)
#define VIDC_REG_854681_RMSK                     0x7f
#define VIDC_REG_854681_SHFT                        0
#define VIDC_REG_854681_OUT(v)             \
	out_dword(VIDC_REG_854681_ADDR, v)

#define VIDC_REG_128234_ADDR               \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000ec8)
#define VIDC_REG_128234_PHYS               \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000ec8)
#define VIDC_REG_128234_RMSK               0xffff000f
#define VIDC_REG_128234_SHFT                        0
#define VIDC_REG_128234_OUT(v)             \
	out_dword(VIDC_REG_128234_ADDR, v)

#define VIDC_REG_1137_ADDR        \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000ecc)
#define VIDC_REG_1137_PHYS        \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000ecc)
#define VIDC_REG_1137_RMSK        0xffffffff
#define VIDC_REG_1137_SHFT                 0
#define VIDC_REG_1137_IN          \
	in_dword_masked(VIDC_REG_1137_ADDR,  \
			VIDC_REG_1137_RMSK)
#define VIDC_REG_1137_INM(m)      \
	in_dword_masked(VIDC_REG_1137_ADDR,  m)
#define VIDC_REG_1137_METADATA_DISPLAY_INDEX_BMSK \
	0xffffffff
#define \
	\
VIDC_REG_1137_METADATA_DISPLAY_INDEX_SHFT          0

#define VIDC_REG_988552_ADDR       \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000ed0)
#define VIDC_REG_988552_PHYS       \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000ed0)
#define VIDC_REG_988552_RMSK       0xffffffff
#define VIDC_REG_988552_SHFT                0
#define VIDC_REG_988552_OUT(v)     \
	out_dword(VIDC_REG_988552_ADDR, v)

#define VIDC_REG_319934_ADDR  \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000ed4)
#define VIDC_REG_319934_PHYS  \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000ed4)
#define VIDC_REG_319934_RMSK                       0xffffffff
#define VIDC_REG_319934_SHFT                   0
#define VIDC_REG_319934_OUT(v)                     \
	out_dword(VIDC_REG_319934_ADDR, v)

#define VIDC_REG_679165_ADDR                   \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000ed8)
#define VIDC_REG_679165_PHYS                   \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000ed8)
#define VIDC_REG_679165_RMSK                   0xffffffff
#define VIDC_REG_679165_SHFT                            0
#define VIDC_REG_679165_IN                     \
	in_dword_masked(VIDC_REG_679165_ADDR,  \
			VIDC_REG_679165_RMSK)
#define VIDC_REG_679165_INM(m)                 \
	in_dword_masked(VIDC_REG_679165_ADDR,  m)
#define VIDC_REG_679165_PIC_TIME_TOP_BMSK       0xffffffff
#define VIDC_REG_679165_PIC_TIME_TOP_SHFT                0

#define VIDC_REG_374150_ADDR                     \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000edc)
#define VIDC_REG_374150_PHYS                     \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000edc)
#define VIDC_REG_374150_RMSK                     0xffffffff
#define VIDC_REG_374150_SHFT                              0
#define VIDC_REG_374150_IN                       \
	in_dword_masked(VIDC_REG_374150_ADDR,  \
			VIDC_REG_374150_RMSK)
#define VIDC_REG_374150_INM(m)                   \
	in_dword_masked(VIDC_REG_374150_ADDR,  m)
#define VIDC_REG_374150_PIC_TIME_BOTTOM_BMSK           0xffffffff
#define VIDC_REG_374150_PIC_TIME_BOTTOM_SHFT                    0

#define VIDC_REG_94750_ADDR                 \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000ee0)
#define VIDC_REG_94750_PHYS                 \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000ee0)
#define VIDC_REG_94750_RMSK                 0xffffffff
#define VIDC_REG_94750_SHFT                          0
#define VIDC_REG_94750_OUT(v)               \
	out_dword(VIDC_REG_94750_ADDR, v)

#define VIDC_REG_438677_ADDR          \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000ee4)
#define VIDC_REG_438677_PHYS                \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000ee4)
#define VIDC_REG_438677_RMSK                0xffffffff
#define VIDC_REG_438677_SHFT                         0
#define VIDC_REG_438677_IN                  \
	in_dword_masked(VIDC_REG_438677_ADDR,  \
			VIDC_REG_438677_RMSK)
#define VIDC_REG_438677_INM(m)              \
	in_dword_masked(VIDC_REG_438677_ADDR,  m)
#define VIDC_REG_438677_GET_FRAME_TAG_BOTTOM_BMSK 0xffffffff
#define VIDC_REG_438677_GET_FRAME_TAG_BOTTOM_SHFT          0

#define VIDC_REG_76706_ADDR               \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00000ee8)
#define VIDC_REG_76706_PHYS               \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000ee8)
#define VIDC_REG_76706_RMSK                      0x1
#define VIDC_REG_76706_SHFT                        0
#define VIDC_REG_76706_OUT(v)             \
	out_dword(VIDC_REG_76706_ADDR, v)

#define VIDC_REG_809984_ADDR                       \
	(VIDC_720P_WRAPPER_REG_BASE      + 0x00001000)
#define VIDC_REG_809984_PHYS                       \
	(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00001000)
#define VIDC_REG_809984_RMSK                       0xffff0007
#define VIDC_REG_809984_SHFT                                0
#define VIDC_REG_809984_IN                         \
	in_dword_masked(VIDC_REG_809984_ADDR,  VIDC_REG_809984_RMSK)
#define VIDC_REG_809984_INM(m)                     \
	in_dword_masked(VIDC_REG_809984_ADDR,  m)
#define VIDC_REG_809984_720PV_720P_WRAPPER_VERSION_BMSK 0xffff0000
#define VIDC_REG_809984_720PV_720P_WRAPPER_VERSION_SHFT       0x10
#define VIDC_REG_809984_TEST_MUX_SEL_BMSK                 0x7
#define VIDC_REG_809984_TEST_MUX_SEL_SHFT                   0


#define VIDC_REG_699747_ADDR \
       (VIDC_720P_WRAPPER_REG_BASE + 0x00000d0c)
#define VIDC_REG_699747_PHYS \
       (VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000d0c)
#define VIDC_REG_699747_RMSK 0xffffffff
#define VIDC_REG_699747_SHFT 0
#define VIDC_REG_699747_OUT(v)                  \
		out_dword(VIDC_REG_699747_ADDR, v)

#define VIDC_REG_166247_ADDR \
       (VIDC_720P_WRAPPER_REG_BASE + 0x00000d10)
#define VIDC_REG_166247_PHYS \
       (VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000d10)
#define VIDC_REG_166247_RMSK 0xffffffff
#define VIDC_REG_166247_SHFT 0
#define VIDC_REG_166247_OUT(v)               \
		out_dword(VIDC_REG_166247_ADDR, v)

#define VIDC_REG_486169_ADDR \
		(VIDC_720P_WRAPPER_REG_BASE + 0x00000d18)
#define VIDC_REG_486169_PHYS \
		(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000d18)
#define VIDC_REG_486169_RMSK 0xffffffff
#define VIDC_REG_486169_SHFT 0
#define VIDC_REG_486169_OUT(v) \
		out_dword(VIDC_REG_486169_ADDR, v)

#define VIDC_REG_926519_ADDR \
		(VIDC_720P_WRAPPER_REG_BASE + 0x00000d1c)
#define VIDC_REG_926519_PHYS \
		(VIDC_720P_WRAPPER_REG_BASE_PHYS + 0x00000d1c)
#define VIDC_REG_926519_RMSK 0xffffffff
#define VIDC_REG_926519_SHFT 0
#define VIDC_REG_926519_OUT(v) \
		out_dword(VIDC_REG_926519_ADDR, v)

/** List all the levels and their register valus */

#define VIDC_720P_PROFILE_MPEG4_SP      0
#define VIDC_720P_PROFILE_MPEG4_ASP     1
#define VIDC_720P_PROFILE_H264_BASELINE 0
#define VIDC_720P_PROFILE_H264_MAIN     1
#define VIDC_720P_PROFILE_H264_HIGH     2
#define VIDC_720P_PROFILE_H264_CPB      3
#define VIDC_720P_PROFILE_H263_BASELINE 0

#define VIDC_720P_PROFILE_VC1_SP        0
#define VIDC_720P_PROFILE_VC1_MAIN      1
#define VIDC_720P_PROFILE_VC1_ADV       2
#define VIDC_720P_PROFILE_MPEG2_MAIN    4
#define VIDC_720P_PROFILE_MPEG2_SP      5

#define VIDC_720P_MPEG4_LEVEL0  0
#define VIDC_720P_MPEG4_LEVEL0b 9
#define VIDC_720P_MPEG4_LEVEL1  1
#define VIDC_720P_MPEG4_LEVEL2  2
#define VIDC_720P_MPEG4_LEVEL3  3
#define VIDC_720P_MPEG4_LEVEL3b 7
#define VIDC_720P_MPEG4_LEVEL4a 4
#define VIDC_720P_MPEG4_LEVEL5  5
#define VIDC_720P_MPEG4_LEVEL6  6

#define VIDC_720P_H264_LEVEL1     10
#define VIDC_720P_H264_LEVEL1b    9
#define VIDC_720P_H264_LEVEL1p1   11
#define VIDC_720P_H264_LEVEL1p2   12
#define VIDC_720P_H264_LEVEL1p3   13
#define VIDC_720P_H264_LEVEL2     20
#define VIDC_720P_H264_LEVEL2p1   21
#define VIDC_720P_H264_LEVEL2p2   22
#define VIDC_720P_H264_LEVEL3     30
#define VIDC_720P_H264_LEVEL3p1   31
#define VIDC_720P_H264_LEVEL3p2   32

#define VIDC_720P_H263_LEVEL10    10
#define VIDC_720P_H263_LEVEL20    20
#define VIDC_720P_H263_LEVEL30    30
#define VIDC_720P_H263_LEVEL40    40
#define VIDC_720P_H263_LEVEL45    45
#define VIDC_720P_H263_LEVEL50    50
#define VIDC_720P_H263_LEVEL60    60
#define VIDC_720P_H263_LEVEL70    70

#define VIDC_720P_VC1_LEVEL_LOW    0
#define VIDC_720P_VC1_LEVEL_MED    2
#define VIDC_720P_VC1_LEVEL_HIGH   4
#define VIDC_720P_VC1_LEVEL0       0
#define VIDC_720P_VC1_LEVEL1       1
#define VIDC_720P_VC1_LEVEL2       2
#define VIDC_720P_VC1_LEVEL3       3
#define VIDC_720P_VC1_LEVEL4       4

#define VIDCL_720P_MPEG2_LEVEL_LOW 10
#define VIDCL_720P_MPEG2_LEVEL_MAIN 8
#define VIDCL_720P_MPEG2_LEVEL_HIGH14 6

#define VIDC_720P_CMD_CHSET               0x0
#define VIDC_720P_CMD_CHEND               0x2
#define VIDC_720P_CMD_INITCODEC           0x3
#define VIDC_720P_CMD_FRAMERUN            0x4
#define VIDC_720P_CMD_INITBUFFERS         0x5
#define VIDC_720P_CMD_FRAMERUN_REALLOCATE 0x6
#define VIDC_720P_CMD_MFC_ENGINE_RESET 0x7

enum vidc_720p_endian {
	VIDC_720P_BIG_ENDIAN = 0x0,
	VIDC_720P_LITTLE_ENDIAN = 0x1
};

enum vidc_720p_memory_access_method {
	VIDC_720P_TILE_LINEAR = 0,
	VIDC_720P_TILE_16x16 = 2,
	VIDC_720P_TILE_64x32 = 3
};

enum vidc_720p_interrupt_control_mode {
	VIDC_720P_INTERRUPT_MODE = 0,
	VIDC_720P_POLL_MODE = 1
};

enum vidc_720p_interrupt_level_selection {
	VIDC_720P_INTERRUPT_LEVEL_SEL = 0,
	VIDC_720P_INTERRUPT_PULSE_SEL = 1
};

#define VIDC_720P_INTR_BUFFER_FULL             0x002
#define VIDC_720P_INTR_FW_DONE                 0x020
#define VIDC_720P_INTR_HEADER_DONE             0x040
#define VIDC_720P_INTR_DMA_DONE                0x080
#define VIDC_720P_INTR_FRAME_DONE              0x100

enum vidc_720p_enc_dec_selection {
	VIDC_720P_DECODER = 0,
	VIDC_720P_ENCODER = 1
};

enum vidc_720p_codec {
	VIDC_720P_MPEG4 = 0,
	VIDC_720P_H264 = 1,
	VIDC_720P_DIVX = 2,
	VIDC_720P_XVID = 3,
	VIDC_720P_H263 = 4,
	VIDC_720P_MPEG2 = 5,
	VIDC_720P_VC1 = 6
};

enum vidc_720p_frame {
	VIDC_720P_NOTCODED = 0,
	VIDC_720P_IFRAME = 1,
	VIDC_720P_PFRAME = 2,
	VIDC_720P_BFRAME = 3,
	VIDC_720P_IDRFRAME = 4
};

enum vidc_720p_entropy_sel {
	VIDC_720P_ENTROPY_SEL_CAVLC = 0,
	VIDC_720P_ENTROPY_SEL_CABAC = 1
};

enum vidc_720p_cabac_model {
	VIDC_720P_CABAC_MODEL_NUMBER_0 = 0,
	VIDC_720P_CABAC_MODEL_NUMBER_1 = 1,
	VIDC_720P_CABAC_MODEL_NUMBER_2 = 2
};

enum vidc_720p_DBConfig {
	VIDC_720P_DB_ALL_BLOCKING_BOUNDARY = 0,
	VIDC_720P_DB_DISABLE = 1,
	VIDC_720P_DB_SKIP_SLICE_BOUNDARY = 2
};

enum vidc_720p_MSlice_selection {
	VIDC_720P_MSLICE_BY_MB_COUNT = 0,
	VIDC_720P_MSLICE_BY_BYTE_COUNT = 1,
	VIDC_720P_MSLICE_BY_GOB = 2,
	VIDC_720P_MSLICE_OFF = 3
};

enum vidc_720p_display_status {
	VIDC_720P_DECODE_ONLY = 0,
	VIDC_720P_DECODE_AND_DISPLAY = 1,
	VIDC_720P_DISPLAY_ONLY = 2,
	VIDC_720P_EMPTY_BUFFER = 3
};

#define VIDC_720P_ENC_IFRAME_REQ       0x1
#define VIDC_720P_ENC_IPERIOD_CHANGE   0x1
#define VIDC_720P_ENC_FRAMERATE_CHANGE 0x2
#define VIDC_720P_ENC_BITRATE_CHANGE   0x4

#define VIDC_720P_FLUSH_REQ     0x1
#define VIDC_720P_EXTRADATA     0x2

#define VIDC_720P_METADATA_ENABLE_QP           0x01
#define VIDC_720P_METADATA_ENABLE_CONCEALMB    0x02
#define VIDC_720P_METADATA_ENABLE_VC1          0x04
#define VIDC_720P_METADATA_ENABLE_SEI          0x08
#define VIDC_720P_METADATA_ENABLE_VUI          0x10
#define VIDC_720P_METADATA_ENABLE_ENCSLICE     0x20
#define VIDC_720P_METADATA_ENABLE_PASSTHROUGH  0x40

struct vidc_720p_dec_disp_info {
	enum vidc_720p_display_status disp_status;
	u32 resl_change;
	u32 reconfig_flush_done;
	u32 img_size_x;
	u32 img_size_y;
	u32 y_addr;
	u32 c_addr;
	u32 tag_top;
	u32 pic_time_top;
	u32 disp_is_interlace;
	u32 tag_bottom;
	u32 pic_time_bottom;
	u32 metadata_exists;
	u32 crop_exists;
	u32 crop_right_offset;
	u32 crop_left_offset;
	u32 crop_bottom_offset;
	u32 crop_top_offset;
	u32 input_frame;
	u32 input_bytes_consumed;
	u32 input_is_interlace;
	u32 input_frame_num;
};

struct vidc_720p_seq_hdr_info {
	u32 img_size_x;
	u32 img_size_y;
	u32 dec_frm_size;
	u32 min_num_dpb;
	u32 min_dpb_size;
	u32 profile;
	u32 level;
	u32 progressive;
	u32 data_partitioned;
	u32  crop_exists;
	u32  crop_right_offset;
	u32  crop_left_offset;
	u32  crop_bottom_offset;
	u32  crop_top_offset;
};

struct vidc_720p_enc_frame_info {
	u32 enc_size;
	u32 frame;
	u32 metadata_exists;
};

void vidc_720p_set_device_virtual_base(u8 *core_virtual_base_addr);

void vidc_720p_init(char **ppsz_version, u32 i_firmware_size,
	u32 *pi_firmware_address, enum vidc_720p_endian dma_endian,
	u32 interrupt_off,
	enum vidc_720p_interrupt_level_selection	interrupt_sel,
	u32 interrupt_mask);

u32 vidc_720p_do_sw_reset(void);

u32 vidc_720p_reset_is_success(void);

void vidc_720p_start_cpu(enum vidc_720p_endian dma_endian,
		u32 *icontext_bufferstart, u32 *debug_core_dump_addr,
		u32  debug_buffer_size);

u32 vidc_720p_cpu_start(void);

void vidc_720p_stop_fw(void);

void vidc_720p_get_interrupt_status(u32 *interrupt_status,
		u32 *cmd_err_status, u32 *disp_pic_err_status,
		u32 *op_failed);

void vidc_720p_interrupt_done_clear(void);

void vidc_720p_submit_command(u32 ch_id, u32 cmd_id);


void vidc_720p_set_channel(u32 i_ch_id,
	enum vidc_720p_enc_dec_selection enc_dec_sel,
	enum vidc_720p_codec codec, u32 *pi_fw, u32 i_firmware_size);

u32 vidc_720p_engine_reset(u32 ch_id,
   enum vidc_720p_endian dma_endian,
   enum vidc_720p_interrupt_level_selection interrupt_sel,
   u32 interrupt_mask
);

void vidc_720p_encode_set_profile(u32 i_profile, u32 i_level);

void vidc_720p_set_frame_size(u32 i_size_x, u32 i_size_y);

void vidc_720p_encode_set_fps(u32 i_rc_frame_rate);

void vidc_720p_encode_set_vop_time(u32 vop_time_resolution,
		u32 vop_time_increment);

void vidc_720p_encode_set_hec_period(u32 hec_period);

void vidc_720p_encode_set_short_header(u32 i_short_header);

void vidc_720p_encode_set_qp_params(u32 i_max_qp, u32 i_min_qp);

void vidc_720p_encode_set_rc_config(u32 enable_frame_level_rc,
		u32 enable_mb_level_rc_flag, u32 i_frame_qp, u32 pframe_qp);

void vidc_720p_encode_set_bit_rate(u32 i_target_bitrate);

void vidc_720p_encoder_set_param_change(u32 enc_param_change);

void vidc_720p_encode_set_control_param(u32 param_val);

void vidc_720p_encode_set_frame_level_rc_params(u32 i_reaction_coeff);

void vidc_720p_encode_set_mb_level_rc_params(u32 dark_region_as_flag,
	u32 smooth_region_as_flag, u32 static_region_as_flag,
	u32 activity_region_flag);

void vidc_720p_encode_set_entropy_control(enum vidc_720p_entropy_sel \
		entropy_sel,
		enum vidc_720p_cabac_model cabac_model_number);

void vidc_720p_encode_set_db_filter_control(enum vidc_720p_DBConfig
		db_config, u32 i_slice_alpha_offset, u32 i_slice_beta_offset);

void vidc_720p_encode_set_intra_refresh_mb_number(u32 i_cir_mb_number);

void vidc_720p_encode_set_multi_slice_info(
		enum vidc_720p_MSlice_selection m_slice_sel,
		u32 multi_slice_size);

void vidc_720p_encode_set_dpb_buffer(u32 *pi_enc_dpb_addr, u32 alloc_len);

void vidc_720p_set_deblock_line_buffer(u32 *pi_deblock_line_buffer_start,
		u32 alloc_len);

void vidc_720p_encode_set_i_period(u32 i_i_period);

void vidc_720p_encode_init_codec(u32 i_ch_id,
	enum vidc_720p_memory_access_method memory_access_model);

void vidc_720p_encode_unalign_bitstream(u32 upper_unalign_word,
	u32 lower_unalign_word);

void vidc_720p_encode_set_seq_header_buffer(u32 ext_buffer_start,
	u32 ext_buffer_end, u32 start_byte_num);

void vidc_720p_encode_frame(u32 ch_id, u32 ext_buffer_start,
	u32 ext_buffer_end, u32 start_byte_number,
	u32 y_addr, u32 c_addr);

void vidc_720p_encode_get_header(u32 *pi_enc_header_size);

void vidc_720p_enc_frame_info
	(struct vidc_720p_enc_frame_info *enc_frame_info);

void vidc_720p_decode_bitstream_header(u32 ch_id, u32 dec_unit_size,
	u32 start_byte_num, u32 ext_buffer_start, u32 ext_buffer_end,
	enum vidc_720p_memory_access_method memory_access_model,
	u32 decode_order);

void vidc_720p_decode_get_seq_hdr_info
    (struct vidc_720p_seq_hdr_info *seq_hdr_info);

void vidc_720p_decode_set_dpb_release_buffer_mask
    (u32 i_dpb_release_buffer_mask);

void vidc_720p_decode_set_dpb_buffers(u32 i_buf_index, u32 *pi_dpb_buffer);

void vidc_720p_decode_set_comv_buffer
    (u32 *pi_dpb_comv_buffer, u32 alloc_len);

void vidc_720p_decode_set_dpb_details
    (u32 num_dpb, u32 alloc_len, u32 *ref_buffer);

void vidc_720p_decode_set_mpeg4Post_filter(u32 enable_post_filter);

void vidc_720p_decode_set_error_control(u32 enable_error_control);

void vidc_720p_decode_set_mpeg4_data_partitionbuffer(u32 *vsp_buf_start);

void vidc_720p_decode_setH264VSPBuffer(u32 *pi_vsp_temp_buffer_start);

void vidc_720p_decode_frame(u32 ch_id, u32 ext_buffer_start,
		u32 ext_buffer_end, u32 dec_unit_size,
		u32 start_byte_num, u32 input_frame_tag);

void vidc_720p_issue_eos(u32 i_ch_id);
void vidc_720p_eos_info(u32 *disp_status, u32 *resl_change);

void vidc_720p_decode_display_info
    (struct vidc_720p_dec_disp_info *disp_info);

void vidc_720p_decode_skip_frm_details(u32 *free_luma_dpb);

void vidc_720p_metadata_enable(u32 flag, u32 *input_buffer);

void vidc_720p_decode_dynamic_req_reset(void);

void vidc_720p_decode_dynamic_req_set(u32 property);

void vidc_720p_decode_setpassthrough_start(u32 pass_startaddr);



#define DDL_720P_REG_BASE VIDC_720P_WRAPPER_REG_BASE
#define VIDC_BUSY_WAIT(n) udelay(n)

#undef VIDC_REGISTER_LOG_MSG
#undef VIDC_REGISTER_LOG_INTO_BUFFER

#ifdef VIDC_REGISTER_LOG_MSG
#define VIDC_MSG1(msg_format, a) printk(KERN_INFO msg_format, a)
#define VIDC_MSG2(msg_format, a, b) printk(KERN_INFO msg_format, a, b)
#define VIDC_MSG3(msg_format, a, b, c) printk(KERN_INFO msg_format, a, b, c)
#else
#define VIDC_MSG1(msg_format, a)
#define VIDC_MSG2(msg_format, a, b)
#define VIDC_MSG3(msg_format, a, b, c)
#endif

#ifdef VIDC_REGISTER_LOG_INTO_BUFFER

#define VIDC_REGLOG_BUFSIZE 200000
#define VIDC_REGLOG_MAX_PRINT_SIZE 100
extern char vidclog[VIDC_REGLOG_BUFSIZE];
extern unsigned int vidclog_index;

#define VIDC_LOG_BUFFER_INIT \
{if (vidclog_index) \
  memset(vidclog, 0, vidclog_index+1); \
  vidclog_index = 0; }

#define VIDC_REGLOG_CHECK_BUFINDEX(req_size) \
  vidclog_index = \
  (vidclog_index+(req_size) < VIDC_REGLOG_BUFSIZE) ? vidclog_index : 0;

#define VIDC_LOG_WRITE(reg, val) \
{unsigned int len; \
	VIDC_REGLOG_CHECK_BUFINDEX(VIDC_REGLOG_MAX_PRINT_SIZE); \
	len = snprintf(&vidclog[vidclog_index], VIDC_REGLOG_MAX_PRINT_SIZE, \
	"(0x%x:"#reg"=0x%x)" , VIDC_##reg##_ADDR - DDL_720P_REG_BASE, val);\
	vidclog_index += len; }

#define VIDC_LOG_WRITEI(reg, index, val) \
{unsigned int len; \
	VIDC_REGLOG_CHECK_BUFINDEX(VIDC_REGLOG_MAX_PRINT_SIZE); \
	len = snprintf(&vidclog[vidclog_index], VIDC_REGLOG_MAX_PRINT_SIZE, \
	"(0x%x:"#reg"=0x%x)" , VIDC_##reg##_ADDR(index)-DDL_720P_REG_BASE,  \
	val); vidclog_index += len; }

#define VIDC_LOG_WRITEF(reg, field, val) \
{unsigned int len; \
	VIDC_REGLOG_CHECK_BUFINDEX(VIDC_REGLOG_MAX_PRINT_SIZE); \
	len = snprintf(&vidclog[vidclog_index], VIDC_REGLOG_MAX_PRINT_SIZE, \
	"(0x%x:"#reg":0x%x:=0x%x)" , VIDC_##reg##_ADDR - DDL_720P_REG_BASE,  \
	VIDC_##reg##_##field##_BMSK,  val);\
	vidclog_index += len; }

#define VIDC_LOG_READ(reg, pval) \
{ unsigned int len; \
	VIDC_REGLOG_CHECK_BUFINDEX(VIDC_REGLOG_MAX_PRINT_SIZE); \
	len = snprintf(&vidclog[vidclog_index], VIDC_REGLOG_MAX_PRINT_SIZE, \
	"(0x%x:"#reg"==0x%x)" , VIDC_##reg##_ADDR - DDL_720P_REG_BASE,  \
	(u32)*pval); \
	vidclog_index += len; }

#define VIDC_STR_LOGBUFFER(str) \
{ unsigned int len; \
	VIDC_REGLOG_CHECK_BUFINDEX(VIDC_REGLOG_MAX_PRINT_SIZE); \
	len = snprintf(&vidclog[vidclog_index], VIDC_REGLOG_MAX_PRINT_SIZE, \
	"<%s>" , str); vidclog_index += len; }

#define VIDC_LONG_LOGBUFFER(str, arg1) \
{ unsigned int len; \
	VIDC_REGLOG_CHECK_BUFINDEX(VIDC_REGLOG_MAX_PRINT_SIZE); \
	len = snprintf(&vidclog[vidclog_index], VIDC_REGLOG_MAX_PRINT_SIZE, \
	"<%s=0x%x>" , str, arg1); vidclog_index += len; }

#define VIDC_DEBUG_REGISTER_LOG \
{ u32 val; unsigned int len; \
	val = VIDC_720P_IN(REG_881638); \
	VIDC_REGLOG_CHECK_BUFINDEX(VIDC_REGLOG_MAX_PRINT_SIZE); \
	len = snprintf(&vidclog[vidclog_index], 50,  "[dbg1=%x]" , val); \
	vidclog_index += len; \
	val = VIDC_720P_IN(REG_161486); \
	VIDC_REGLOG_CHECK_BUFINDEX(VIDC_REGLOG_MAX_PRINT_SIZE); \
	len = snprintf(&vidclog[vidclog_index], 50,  "[dbg2=%x]" , val); \
	vidclog_index += len; }

#else
#define VIDC_LOG_WRITE(reg, val)
#define VIDC_LOG_WRITEI(reg, index, val)
#define VIDC_LOG_WRITEF(reg, field, val)
#define VIDC_LOG_READ(reg, pval)
#define VIDC_LOG_BUFFER_INIT
#define VIDC_STR_LOGBUFFER(str)
#define VIDC_LONG_LOGBUFFER(str, arg1)
#define VIDC_DEBUG_REGISTER_LOG
#endif

void vidcputlog(char *str);
void vidcput_debug_reglog(void);

#define VIDC_LOGERR_STRING(str) \
do { \
	VIDC_STR_LOGBUFFER(str); \
	VIDC_MSG1("\n<%s>", str); \
} while (0)

#define VIDC_LOG_STRING(str) \
do { \
	VIDC_STR_LOGBUFFER(str); \
	VIDC_MSG1("\n<%s>", str); \
} while (0)

#define VIDC_LOG1(str, arg1) \
do { \
	VIDC_LONG_LOGBUFFER(str, arg1); \
	VIDC_MSG2("\n<%s=0x%08x>", str, arg1); \
} while (0)

#define VIDC_IO_OUT(reg,  val) \
do { \
	VIDC_LOG_WRITE(reg, (u32)val);  \
	VIDC_MSG2("\n(0x%08x:"#reg"=0x%08x)",  \
	(u32)(VIDC_##reg##_ADDR - DDL_720P_REG_BASE),  (u32)val); \
	mb(); \
	VIDC_720P_OUT(reg, val);  \
} while (0)

#define VIDC_IO_OUTI(reg,  index,  val) \
do { \
	VIDC_LOG_WRITEI(reg, index, (u32)val); \
	VIDC_MSG2("\n(0x%08x:"#reg"=0x%08x)",  \
	(u32)(VIDC_##reg##_ADDR(index)-DDL_720P_REG_BASE),  (u32)val); \
	mb(); \
	VIDC_720P_OUTI(reg, index, val);  \
} while (0)

#define VIDC_IO_OUTF(reg,  field,  val) \
do { \
	VIDC_LOG_WRITEF(reg, field, val); \
	VIDC_MSG3("\n(0x%08x:"#reg":0x%x:=0x%08x)",  \
	(u32)(VIDC_##reg##_ADDR - DDL_720P_REG_BASE),  \
	VIDC_##reg##_##field##_BMSK,  (u32)val); \
	mb(); \
	VIDC_720P_OUTF(reg, field, val);  \
} while (0)

#define VIDC_IO_IN(reg, pval) \
do { \
	mb(); \
	*pval = (u32) VIDC_720P_IN(reg); \
	VIDC_LOG_READ(reg, pval); \
	VIDC_MSG2("\n(0x%08x:"#reg"==0x%08x)",  \
	(u32)(VIDC_##reg##_ADDR - DDL_720P_REG_BASE), (u32) *pval);  \
} while (0)

#define VIDC_IO_INF(reg, mask, pval) \
do { \
	mb(); \
	*pval = VIDC_720P_INF(reg, mask); \
	VIDC_LOG_READ(reg, pval); \
	VIDC_MSG2("\n(0x%08x:"#reg"==0x%08x)",  \
	(u32)(VIDC_##reg##_ADDR - DDL_720P_REG_BASE),  *pval); \
} while (0)

#endif
