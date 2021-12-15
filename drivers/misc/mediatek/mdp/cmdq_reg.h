/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __CMDQ_REG_H__
#define __CMDQ_REG_H__

#include <mt-plat/sync_write.h>
#include <linux/io.h>

#include "mdp_cmdq_helper_ext.h"
#include "mdp_cmdq_device.h"

#define GCE_BASE_PA			cmdq_dev_get_module_base_PA_GCE()
#define GCE_BASE_VA			cmdq_dev_get_module_base_VA_GCE()

#define CMDQ_CORE_WARM_RESET	(GCE_BASE_VA + 0x000)
#define CMDQ_CURR_IRQ_STATUS	(GCE_BASE_VA + 0x010)
#define CMDQ_SECURE_IRQ_STATUS	(GCE_BASE_VA + 0x014)
#define CMDQ_CURR_LOADED_THR	(GCE_BASE_VA + 0x018)
#define CMDQ_THR_SLOT_CYCLES	(GCE_BASE_VA + 0x030)
#define CMDQ_THR_EXEC_CYCLES	(GCE_BASE_VA + 0x034)
#define CMDQ_THR_TIMEOUT_TIMER	(GCE_BASE_VA + 0x038)
#define CMDQ_BUS_CONTROL_TYPE	(GCE_BASE_VA + 0x040)
#define CMDQ_H_SPEED_BUSY	(GCE_BASE_VA + 0x048)
#define CMDQ_CURR_INST_ABORT	(GCE_BASE_VA + 0x020)
#define CMDQ_SECURITY_ABORT	(GCE_BASE_VA + 0x050)
#define CMDQ_SECURITY_CTL	(GCE_BASE_VA + 0x054)

#define CMDQ_SECURITY_STA(id)	(GCE_BASE_VA + (0x030 * id) + 0x024)
#define CMDQ_SECURITY_SET(id)	(GCE_BASE_VA + (0x030 * id) + 0x028)
#define CMDQ_SECURITY_CLR(id)	(GCE_BASE_VA + (0x030 * id) + 0x02C)

#define CMDQ_SYNC_TOKEN_ID	(GCE_BASE_VA + 0x060)
#define CMDQ_SYNC_TOKEN_VAL	(GCE_BASE_VA + 0x064)
#define CMDQ_SYNC_TOKEN_UPD	(GCE_BASE_VA + 0x068)

#define CMDQ_PREFETCH_GSIZE	(GCE_BASE_VA + 0x0C0)
#define CMDQ_TPR_MASK		(GCE_BASE_VA + 0x0D0)
#define CMDQ_TPR_GPR_TIMER	(GCE_BASE_VA + 0x0DC)
#define CMDQ_CTL_INT0		(GCE_BASE_VA + 0x0F0)
#define CMDQ_CTL_INT1		(GCE_BASE_VA + 0x0F4)
#define CMDQ_CACHE_0_EN_LO	(GCE_BASE_VA + 0x10D0)
#define CMDQ_CACHE_0_EN_HI	(GCE_BASE_VA + 0x10D4)
#define CMDQ_CACHE_1_EN_LO	(GCE_BASE_VA + 0x10D8)
#define CMDQ_CACHE_1_EN_HI	(GCE_BASE_VA + 0x10DC)
#define CMDQ_TOKEN_0_EN_LO	(GCE_BASE_VA + 0x10E0)
#define CMDQ_TOKEN_0_EN_HI	(GCE_BASE_VA + 0x10E4)
#define CMDQ_TOKEN_1_EN_LO	(GCE_BASE_VA + 0x10E8)
#define CMDQ_TOKEN_1_EN_HI	(GCE_BASE_VA + 0x10EC)

#define GCE_DBG_CTL		(GCE_BASE_VA + 0x3000)
#define GCE_DBG0		(GCE_BASE_VA + 0x3004)
#define GCE_DBG1		(GCE_BASE_VA + 0x3008)
#define GCE_DBG2		(GCE_BASE_VA + 0x300C)
#define GCE_DBG3		(GCE_BASE_VA + 0x3010)

#define CMDQ_GPR_R32(id)		(GCE_BASE_VA + (0x004 * id) + 0x80)
#define CMDQ_GPR_R32_PA(id)		(GCE_BASE_PA + (0x004 * id) + 0x80)

#define CMDQ_THR_WARM_RESET(id)		(GCE_BASE_VA + (0x080 * id) + 0x100)
#define CMDQ_THR_ENABLE_TASK(id)	(GCE_BASE_VA + (0x080 * id) + 0x104)
#define CMDQ_THR_SUSPEND_TASK(id)	(GCE_BASE_VA + (0x080 * id) + 0x108)
#define CMDQ_THR_CURR_STATUS(id)	(GCE_BASE_VA + (0x080 * id) + 0x10C)
#define CMDQ_THR_IRQ_STATUS(id)		(GCE_BASE_VA + (0x080 * id) + 0x110)
#define CMDQ_THR_IRQ_ENABLE(id)		(GCE_BASE_VA + (0x080 * id) + 0x114)
#define CMDQ_THR_SECURITY(id)		(GCE_BASE_VA + (0x080 * id) + 0x118)
#define CMDQ_THR_CURR_ADDR(id)		(GCE_BASE_VA + (0x080 * id) + 0x120)
#define CMDQ_THR_END_ADDR(id)		(GCE_BASE_VA + (0x080 * id) + 0x124)
#define CMDQ_THR_EXEC_CNT(id)		(GCE_BASE_VA + (0x080 * id) + 0x128)
#define CMDQ_THR_WAIT_TOKEN(id)		(GCE_BASE_VA + (0x080 * id) + 0x130)
#define CMDQ_THR_CFG(id)		(GCE_BASE_VA + (0x080 * id) + 0x140)
#define CMDQ_THR_PREFETCH(id)		(GCE_BASE_VA + (0x080 * id) + 0x144)
#define CMDQ_THR_INST_CYCLES(id)	(GCE_BASE_VA + (0x080 * id) + 0x150)
#define CMDQ_THR_INST_THRESX(id)	(GCE_BASE_VA + (0x080 * id) + 0x154)
#define CMDQ_THR_SPR0(id)		(GCE_BASE_VA + (0x080 * id) + 0x160)
#define CMDQ_THR_SPR1(id)		(GCE_BASE_VA + (0x080 * id) + 0x164)
#define CMDQ_THR_SPR2(id)		(GCE_BASE_VA + (0x080 * id) + 0x168)
#define CMDQ_THR_SPR3(id)		(GCE_BASE_VA + (0x080 * id) + 0x16c)

#define CMDQ_THR_SECURITY_PA(id)	(GCE_BASE_PA + (0x080 * id) + 0x118)
#define CMDQ_THR_CURR_ADDR_PA(id)	(GCE_BASE_PA + (0x080 * id) + 0x120)
#define CMDQ_THR_END_ADDR_PA(id)	(GCE_BASE_PA + (0x080 * id) + 0x124)
#define CMDQ_THR_EXEC_CNT_PA(id)	(GCE_BASE_PA + (0x080 * id) + 0x128)
#define CMDQ_THR_SPR0_PA(id)		(GCE_BASE_PA + (0x080 * id) + 0x160)
#define CMDQ_THR_SPR1_PA(id)		(GCE_BASE_PA + (0x080 * id) + 0x164)
#define CMDQ_THR_SPR2_PA(id)		(GCE_BASE_PA + (0x080 * id) + 0x168)
#define CMDQ_THR_SPR3_PA(id)		(GCE_BASE_PA + (0x080 * id) + 0x16c)


#define CMDQ_SECURITY_CTL_PA		(GCE_BASE_PA + 0x054)
#define CMDQ_SYNC_TOKEN_ID_PA		(GCE_BASE_PA + 0x060)
#define CMDQ_SYNC_TOKEN_VAL_PA		(GCE_BASE_PA + 0x064)
#define CMDQ_PREFETCH_GSIZE_PA		(GCE_BASE_PA + 0x0C0)
#define CMDQ_TPR_MASK_PA		(GCE_BASE_PA + 0x0D0)

#define CMDQ_GCE_END_ADDR_PA		(GCE_BASE_PA + 0xC00)
#define CMDQ_THR_FIX_END_ADDR(id)	(CMDQ_GCE_END_ADDR_PA | (id << 4))

#define CMDQ_CACHE_0_EN_LO_PA		(GCE_BASE_PA + 0x10D0)
#define CMDQ_CACHE_0_EN_HI_PA		(GCE_BASE_PA + 0x10D4)
#define CMDQ_CACHE_1_EN_LO_PA		(GCE_BASE_PA + 0x10D8)
#define CMDQ_CACHE_1_EN_HI_PA		(GCE_BASE_PA + 0x10DC)
#define CMDQ_TOKEN_0_EN_LO_PA		(GCE_BASE_PA + 0x10E0)
#define CMDQ_TOKEN_0_EN_HI_PA		(GCE_BASE_PA + 0x10E4)
#define CMDQ_TOKEN_1_EN_LO_PA		(GCE_BASE_PA + 0x10E8)
#define CMDQ_TOKEN_1_EN_HI_PA		(GCE_BASE_PA + 0x10EC)

#define CMDQ_IS_END_ADDR(addr)		(addr == 0)
#define CMDQ_IS_END_INSTR(p_cmd_end)	\
	(p_cmd_end[0] == 0 && p_cmd_end[-1] == 0)
#define CMDQ_IS_SRAM_ADDR(addr)		(((addr) & 0xFFFF) == (addr))

#define CMDQ_REG_GET32(addr)		(readl((void *)addr) & 0xFFFFFFFF)
#define CMDQ_REG_GET16(addr)		(readl((void *)addr) & 0x0000FFFF)

#define CMDQ_REG_GET64_GPR_PX(id)	cmdq_core_get_gpr64(id)
#define CMDQ_REG_SET64_GPR_PX(id, value)	cmdq_core_set_gpr64(id, value)

#define CMDQ_GET_GPR_PX2RX_LOW(id)	((id & 0xf) * 2)
#define CMDQ_GET_GPR_PX2RX_HIGH(id)	((id & 0xf) * 2 + 1)

#define CMDQ_REG_SET32(addr, val)	mt_reg_sync_writel(val, (addr))


#endif				/* __CMDQ_REG_H__ */
