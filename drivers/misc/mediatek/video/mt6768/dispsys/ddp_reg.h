/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _DDP_REG_H_
#define _DDP_REG_H_
#include "mt-plat/sync_write.h"
#include "display_recorder.h"
#include "cmdq_record.h"
#include "cmdq_core.h"
#include "ddp_hal.h"

#define ENABLE_CLK_MGR

#define UINT32 unsigned int

/* ////////////////////////////// macro //////////////////////////// */
#ifndef READ_REGISTER_UINT32
	#define READ_REGISTER_UINT32(reg) (*(UINT32 * const)(reg))
#endif
#ifndef WRITE_REGISTER_UINT32
	#define WRITE_REGISTER_UINT32(reg, val) \
		((*(UINT32 * const)(reg)) = (val))
#endif
#ifndef READ_REGISTER_UINT16
	#define READ_REGISTER_UINT16(reg) ((*(UINT16 * const)(reg)))
#endif
#ifndef WRITE_REGISTER_UINT16
	#define WRITE_REGISTER_UINT16(reg, val) \
		((*(UINT16 * const)(reg)) = (val))
#endif
#ifndef READ_REGISTER_UINT8
	#define READ_REGISTER_UINT8(reg) ((*(UINT8 * const)(reg)))
#endif
#ifndef WRITE_REGISTER_UINT8
	#define WRITE_REGISTER_UINT8(reg, val) \
		((*(UINT8 * const)(reg)) = (val))
#endif

#define INREG8(x) READ_REGISTER_UINT8((UINT8 *)((void *)(x)))
#define OUTREG8(x, y) \
	WRITE_REGISTER_UINT8((UINT8 *)((void *)(x)), (UINT8)(y))
#define SETREG8(x, y) OUTREG8(x, INREG8(x)|(y))
#define CLRREG8(x, y) OUTREG8(x, INREG8(x)&~(y))
#define MASKREG8(x, y, z) OUTREG8(x, (INREG8(x)&~(y))|(z))

#define INREG16(x) READ_REGISTER_UINT16((UINT16 *)((void *)(x)))
#define OUTREG16(x, y) \
	WRITE_REGISTER_UINT16((UINT16 *)((void *)(x)), (UINT16)(y))
#define SETREG16(x, y) OUTREG16(x, INREG16(x)|(y))
#define CLRREG16(x, y) OUTREG16(x, INREG16(x)&~(y))
#define MASKREG16(x, y, z) OUTREG16(x, (INREG16(x)&~(y))|(z))

#define INREG32(x) READ_REGISTER_UINT32((UINT32 *)((void *)(x)))
#define OUTREG32(x, y) \
	WRITE_REGISTER_UINT32((UINT32 *)((void *)(x)), (UINT32)(y))
#define SETREG32(x, y) OUTREG32(x, INREG32(x)|(y))
#define CLRREG32(x, y) OUTREG32(x, INREG32(x)&~(y))
#define MASKREG32(x, y, z) OUTREG32(x, (INREG32(x)&~(y))|(z))

#ifndef ASSERT
	#define ASSERT(expr)	WARN_ON(!(expr))
#endif

#define AS_INT32(x)     (*(INT32 *)((void *)x))
#define AS_INT16(x)     (*(INT16 *)((void *)x))
#define AS_INT8(x)      (*(INT8  *)((void *)x))

#define AS_UINT32(x)    (*(UINT32 *)((void *)x))
#define AS_UINT16(x)    (*(UINT16 *)((void *)x))
#define AS_UINT8(x)     (*(UINT8  *)((void *)x))

#ifndef FALSE
  #define FALSE (0)
#endif

#ifndef TRUE
  #define TRUE  (1)
#endif



extern cmdqBackupSlotHandle dispsys_slot;

#define DISP_RDMA_INDEX_OFFSET \
	(ddp_get_module_va(DISP_MODULE_RDMA1) - \
	ddp_get_module_va(DISP_MODULE_RDMA0))
#define DISP_WDMA_INDEX_OFFSET  (0)
#define DISP_OVL_INDEX_OFFSET   (0)
#define DISP_MIPI_INDEX_OFFSET  (0)
#define DISPSYS_CONFIG_BASE ddp_get_module_va(DISP_MODULE_CONFIG)
#define DISPSYS_OVL0_BASE ddp_get_module_va(DISP_MODULE_OVL0)
#define DISPSYS_OVL0_2L_BASE ddp_get_module_va(DISP_MODULE_OVL0_2L)
#define DISPSYS_OVL1_2L_BASE ddp_get_module_va(DISP_MODULE_OVL1_2L)
#define DISPSYS_RDMA0_BASE ddp_get_module_va(DISP_MODULE_RDMA0)
#define DISPSYS_RDMA1_BASE ddp_get_module_va(DISP_MODULE_RDMA1)
#define DISPSYS_WDMA0_BASE ddp_get_module_va(DISP_MODULE_WDMA0)
#define DISPSYS_COLOR0_BASE ddp_get_module_va(DISP_MODULE_COLOR0)
#define DISPSYS_CCORR0_BASE ddp_get_module_va(DISP_MODULE_CCORR0)
#define DISPSYS_AAL0_BASE ddp_get_module_va(DISP_MODULE_AAL0)
#define DISPSYS_GAMMA0_BASE ddp_get_module_va(DISP_MODULE_GAMMA0)
#define DISPSYS_DITHER0_BASE ddp_get_module_va(DISP_MODULE_DITHER0)
#define DISPSYS_DSI0_BASE ddp_get_module_va(DISP_MODULE_DSI0)
#define DISPSYS_DSI1_BASE ddp_get_module_va(DISP_MODULE_DSI1)
#define DISPSYS_RSZ0_BASE ddp_get_module_va(DISP_MODULE_RSZ0)
#define DISPSYS_DPI_BASE ddp_get_module_va(DISP_MODULE_DPI)
#define DISPSYS_PWM0_BASE ddp_get_module_va(DISP_MODULE_PWM0)
#define DISPSYS_MUTEX_BASE ddp_get_module_va(DISP_MODULE_MUTEX)
#define DISPSYS_SMI_LARB0_BASE ddp_get_module_va(DISP_MODULE_SMI_LARB0)
#define DISPSYS_SMI_LARB1_BASE ddp_get_module_va(DISP_MODULE_SMI_LARB1)
#define DISPSYS_SMI_COMMON_BASE ddp_get_module_va(DISP_MODULE_SMI_COMMON)
#define DISPSYS_MIPITX0_BASE ddp_get_module_va(DISP_MODULE_MIPI0)
#define DISPSYS_MIPITX1_BASE ddp_get_module_va(DISP_MODULE_MIPI1)
#define DISPSYS_SLOT_BASE		    dispsys_slot

#ifdef INREG32
#undef INREG32
#define INREG32(x)          (__raw_readl((unsigned long *)(x)))
#endif

/* ------------------------------------------------------------------------- */
/* Register Field Access */
/* ------------------------------------------------------------------------- */

#define REG_FLD(width, shift) \
	((unsigned int)((((width) & 0xFF) << 16) | ((shift) & 0xFF)))

#define REG_FLD_MSB_LSB(msb, lsb) REG_FLD((msb) - (lsb) + 1, (lsb))

#define REG_FLD_WIDTH(field) \
	((unsigned int)(((field) >> 16) & 0xFF))

#define REG_FLD_SHIFT(field) \
	((unsigned int)((field) & 0xFF))

#define REG_FLD_MASK(field) \
	((unsigned int)((1ULL << REG_FLD_WIDTH(field)) - 1) << \
	REG_FLD_SHIFT(field))

#define REG_FLD_VAL(field, val) \
	(((val) << REG_FLD_SHIFT(field)) & REG_FLD_MASK(field))

#define REG_FLD_VAL_GET(field, regval) \
	(((regval) & REG_FLD_MASK(field)) >> REG_FLD_SHIFT(field))


#define DISP_REG_GET(reg32) __raw_readl((unsigned long *)(reg32))
#define DISP_REG_GET_FIELD(field, reg32) \
	REG_FLD_VAL_GET(field, __raw_readl((unsigned long *)(reg32)))

/* polling register until masked bit is 1 */
#define DDP_REG_POLLING(reg32, mask) \
	do { \
		while (!((DISP_REG_GET(reg32))&mask))\
			; \
	} while (0)

/* Polling register until masked bit is 0 */
#define DDP_REG_POLLING_NEG(reg32, mask) \
	do { \
		while ((DISP_REG_GET(reg32))&mask)\
			; \
	} while (0)

#define DISP_CPU_REG_SET(reg32, val) \
	mt_reg_sync_writel(val, (unsigned long *)(reg32))

/* after apply device tree va/pa is not mapped by a fixed offset */
static inline unsigned long disp_addr_convert(unsigned long va)
{
	unsigned int i = 0;

	for (i = 0; i < DISP_MODULE_NUM; i++) {
		if (ddp_get_module_va(i) == (va & (~0xfffl)))
			return ddp_get_module_pa(i) + (va & 0xfffl);
	}
	pr_info("DDP/can not find reg addr for va=0x%lx!\n", va);
	ASSERT(0);
	return 0;
}


#define DISP_REG_MASK(handle, reg32, val, mask)	\
	do { \
		if (handle == NULL) { \
			mt_reg_sync_writel((unsigned int)(INREG32(reg32)& \
			~(mask))|(val), (reg32));\
		} else { \
			cmdqRecWrite(handle, \
			disp_addr_convert((unsigned long)(reg32)), \
			val, mask); \
		}	\
	} while (0)

#define DISP_REG_SET(handle, reg32, val) \
	do { \
		if (handle == NULL) { \
			mt_reg_sync_writel(val, (unsigned long *)(reg32));\
		} else { \
			cmdqRecWrite(handle, \
			disp_addr_convert((unsigned long)(reg32)), \
			val, ~0); \
		}  \
	} while (0)


#define DISP_REG_SET_FIELD(handle, field, reg32, val)  \
	do {  \
		if (handle == NULL) { \
			unsigned int regval; \
			regval = __raw_readl((unsigned long *)(reg32)); \
			regval  = (regval & ~REG_FLD_MASK(field)) | \
				(REG_FLD_VAL((field), (val))); \
			mt_reg_sync_writel(regval, (reg32));  \
		} else { \
			cmdqRecWrite(handle, disp_addr_convert(reg32), \
				(val)<<REG_FLD_SHIFT(field), \
				REG_FLD_MASK(field));\
		} \
	} while (0)

#define DISP_REG_CMDQ_POLLING(handle, reg32, val, mask) \
	do { \
		if (handle == NULL) { \
			while ((DISP_REG_GET(reg32) & (mask)) != \
				((val) & (mask)))\
				; \
		} else { \
			cmdqRecPoll(handle, \
				disp_addr_convert((unsigned long)(reg32)), \
				val, mask); \
		}  \
	} while (0)

#define DISP_REG_BACKUP(handle, hSlot, idx, reg32) \
	do { \
		if (handle != NULL) { \
			if (hSlot) \
				cmdqRecBackupRegisterToSlot(handle, hSlot, idx,\
				disp_addr_convert((unsigned long)(reg32)));\
		}  \
	} while (0)

#define DISP_SLOT_SET(handle, hSlot, idx, val) \
	do { \
		if (handle != NULL) { \
			if (hSlot) \
				cmdqRecBackupUpdateSlot(handle, hSlot,	\
					idx, val); \
		}  \
	} while (0)

/* Helper macros for local command queue */
#define DISP_CMDQ_BEGIN(__cmdq, scenario) \
	do { \
		cmdqRecCreate(scenario, &__cmdq);\
		cmdqRecReset(__cmdq);\
		ddp_insert_config_allow_rec(__cmdq);\
	} while (0)

#define DISP_CMDQ_REG_SET(__cmdq, reg32, val, mask) \
	DISP_REG_MASK(__cmdq, reg32, val, mask)

#define DISP_CMDQ_CONFIG_STREAM_DIRTY(__cmdq) \
	ddp_insert_config_dirty_rec(__cmdq)

#define DISP_CMDQ_END(__cmdq)		\
	do {				\
		cmdqRecFlush(__cmdq);	\
		cmdqRecDestroy(__cmdq); \
	} while (0)

/********************************/
#include "ddp_reg_mmsys.h"
#include "ddp_reg_mutex.h"
#include "ddp_reg_ovl.h"
#include "ddp_reg_pq.h"
#include "ddp_reg_dma.h"
#include "ddp_reg_dsi.h"
#include "ddp_reg_mipi.h"
#include "ddp_reg_rsz.h"

#endif /* _DDP_REG_H_ */
