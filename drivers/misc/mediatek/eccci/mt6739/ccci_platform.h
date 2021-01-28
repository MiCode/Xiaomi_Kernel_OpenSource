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

#ifndef __CCCI_PLATFORM_H__
#define __CCCI_PLATFORM_H__

#include <mt-plat/sync_write.h>
#include "ccci_config.h"
#include "modem_sys.h"
#include "hif/ccci_hif_cldma.h"

#define INVALID_ADDR (0xF0000000)	/* the last EMI bank, properly not used */
#define KERN_EMI_BASE (0x40000000)	/* Bank4 */

/* - AP side, using mcu config base */
/* -- AP Bank4 */
#define AP_BANK4_MAP0 (0)	/* ((volatile unsigned int*)(MCUSYS_CFGREG_BASE+0x200)) */
#define AP_BANK4_MAP1 (0)	/* ((volatile unsigned int*)(MCUSYS_CFGREG_BASE+0x204)) */

/* - MD side, using infra config base */
#define DBG_FLAG_DEBUG		(1<<0)
#define DBG_FLAG_JTAG		(1<<1)
#define MD_DBG_JTAG_BIT		(1<<0)

#define ccci_write32(b, a, v)           mt_reg_sync_writel(v, (b)+(a))
#define ccci_write16(b, a, v)           mt_reg_sync_writew(v, (b)+(a))
#define ccci_write8(b, a, v)            mt_reg_sync_writeb(v, (b)+(a))
#define ccci_read32(b, a)               ioread32((void __iomem *)((b)+(a)))
#define ccci_read16(b, a)               ioread16((void __iomem *)((b)+(a)))
#define ccci_read8(b, a)                ioread8((void __iomem *)((b)+(a)))

#ifdef SET_EMI_STEP_BY_STAGE
void ccci_set_mem_access_protection_1st_stage(struct ccci_modem *md);
void ccci_set_mem_access_protection_second_stage(int md_id);
#endif

unsigned int ccci_get_md_debug_mode(struct ccci_modem *md);
void ccci_get_platform_version(char *ver);

int ccci_plat_common_init(void);
int ccci_platform_init(struct ccci_modem *md);
void ccci_reset_ccif_hw(unsigned char md_id, int ccif_id, void __iomem *baseA, void __iomem *baseB);
void ccci_set_clk_cg(struct ccci_modem *md, unsigned int is_on);
#ifdef ENABLE_DRAM_API
extern phys_addr_t get_max_DRAM_size(void);
#endif
int Is_MD_EMI_voilation(void);
#define MD_IN_DEBUG(md) (0)/* ((ccci_get_md_debug_mode(md)&(DBG_FLAG_JTAG|DBG_FLAG_DEBUG)) != 0) */
#endif	/* __CCCI_PLATFORM_H__ */
