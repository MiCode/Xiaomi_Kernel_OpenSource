/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef _MT6799_CCU_REG_H_
#define _MT6799_CCU_REG_H_

#include <sync_write.h>
#include "ccu_sw_ver.h"
#include "CCU_A_c_header.h"

/*For CCU_A_c_header_v2*/
/*
 *#define SW_VER_SPLIT 0
 *#if (SW_VER_SPLIT == 1)
 *#include "CCU_A_c_header_v2.h"
 *#define ccu_read_reg(base, regName)           \
 *	((g_ccu_sw_version == CHIP_SW_VER_01) ?     \
 *			((PCCU_A_REGS)base)->regName.Raw :  \
 *			((PCCU_A_REGS_V2)base)->regName.Raw \
 *	)
 *#define ccu_write_reg(base, regName, val)            \
 *	do { if (g_ccu_sw_version == CHIP_SW_VER_01) {     \
 *			((PCCU_A_REGS)base)->regName.Raw = val;    \
 *		} else {                                       \
 *			((PCCU_A_REGS_V2)base)->regName.Raw = val; \
 *		} } while (0)
 *#define ccu_read_reg_bit(base, regName, fieldNmae)       \
 *	((g_ccu_sw_version == CHIP_SW_VER_01) ?                \
 *			((PCCU_A_REGS)base)->regName.Bits.fieldNmae :  \
 *			((PCCU_A_REGS_V2)base)->regName.Bits.fieldNmae \
 *	)
 *#define ccu_write_reg_bit(base, regName, fieldNmae, val)        \
 *	do { if (g_ccu_sw_version == CHIP_SW_VER_01) {                \
 *			((PCCU_A_REGS)base)->regName.Bits.fieldNmae = val;    \
 *		} else {                                                  \
 *			((PCCU_A_REGS_V2)base)->regName.Bits.fieldNmae = val; \
 *		} } while (0)
 *#else
 */
/*#define ccu_read_reg(base, regName) \
 *	(((struct CCU_A_REGS *)base)->regName.Raw)
 *#define ccu_write_reg(base, regName, val) \
 *	(((struct CCU_A_REGS *)base)->regName.Raw = val)
 */
#define ccu_read_reg_bit(base, regName, fieldNmae) \
	(((struct CCU_A_REGS *)(uintptr_t)base)->regName.Bits.fieldNmae)
#define ccu_write_reg_bit(base, regName, fieldNmae, val) \
	(((struct CCU_A_REGS *)(uintptr_t)base)->regName.Bits.fieldNmae = val)
#define ccu_read_reg(base, regName) \
	readl(&(((struct CCU_A_REGS *)(uintptr_t)base)->regName))
#define ccu_write_reg(base, regName, val) \
	writel(val, &(((struct CCU_A_REGS *)(uintptr_t)base)->regName))

/*#endif*/


#define CCU_SET_BIT(reg, bit) \
	((*(unsigned int *)(reg)) |= (unsigned int)(1 << (bit)))
#define CCU_CLR_BIT(reg, bit) \
	((*(unsigned int *)(reg)) &= ~((unsigned int)(1 << (bit))))

extern uint64_t ccu_base;

#endif
