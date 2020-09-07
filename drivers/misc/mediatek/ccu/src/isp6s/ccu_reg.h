/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _CCU_REG_H_
#define _CCU_REG_H_

#include "CCU_A_c_header.h"
#include <linux/types.h>

#define ccu_read_reg_bit(base, regName, fieldNmae) \
(((struct CCU_A_REGS *)(uintptr_t)base)->regName.Bits.fieldNmae)
#define ccu_write_reg_bit(base, regName, fieldNmae, val) \
(((struct CCU_A_REGS *)(uintptr_t)base)->regName.Bits.fieldNmae = val)
#define ccu_read_reg(base, regName) \
readl(&(((struct CCU_A_REGS *)(uintptr_t)base)->regName))
#define ccu_write_reg(base, regName, val) \
writel(val, &(((struct CCU_A_REGS *)(uintptr_t)base)->regName))

#define CCU_SET_BIT(reg, bit) \
((*(unsigned int *)(reg)) |= (unsigned int)(1 << (bit)))
#define CCU_CLR_BIT(reg, bit) \
((*(unsigned int *)(reg)) &= ~((unsigned int)(1 << (bit))))

extern uint64_t ccu_base;

#endif
