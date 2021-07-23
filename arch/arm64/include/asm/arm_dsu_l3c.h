// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Sagy Shih <sagy.shih@mediatek.com>
 */

#ifndef __ASM_ARM_DSU_L3C_H
#define __ASM_ARM_DSU_L3C_H

#include <linux/bitops.h>
#include <linux/build_bug.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <asm/barrier.h>
#include <asm/sysreg.h>


#define CLUSTERACPSID_EL1		sys_reg(3, 0, 15, 4, 1)
#define CLUSTERSTASHSID_EL1		sys_reg(3, 0, 15, 4, 2)
#define CLUSTERPARTCR_EL1		sys_reg(3, 0, 15, 4, 3)

static inline u32 __dsu_l3c_read_partcr(void)
{
	return read_sysreg_s(CLUSTERPARTCR_EL1);
}

static inline void __dsu_l3c_write_partcr(u32 val)
{
	write_sysreg_s(val, CLUSTERPARTCR_EL1);
	isb();
}

static inline u32 __dsu_l3c_read_acpsid(void)
{
	return read_sysreg_s(CLUSTERACPSID_EL1);
}

static inline void __dsu_l3c_write_acpsid(u32 val)
{
	write_sysreg_s(val, CLUSTERACPSID_EL1);
	isb();
}

static inline u32 __dsu_l3c_read_stashsid(void)
{
	return read_sysreg_s(CLUSTERSTASHSID_EL1);
}

static inline void __dsu_l3c_write_stashsid(u32 val)
{
	write_sysreg_s(val, CLUSTERSTASHSID_EL1);
	isb();
}

#endif /* __ASM_ARM_DSU_L3C_H */
