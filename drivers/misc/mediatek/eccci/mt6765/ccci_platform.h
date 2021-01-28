/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */


#ifndef __CCCI_PLATFORM_H__
#define __CCCI_PLATFORM_H__

#include <mt-plat/sync_write.h>
#include "ccci_config.h"
#include "ccci_common_config.h"
#include "modem_sys.h"
#include "hif/ccci_hif_cldma.h"

extern unsigned int devapc_check_flag;

struct  ccci_plat_val {
	void __iomem *infra_ao_base;
	unsigned int md_gen;
	unsigned long offset_epof_md1;
	void __iomem *md_plat_info;
};

static struct ccci_plat_val md_cd_plat_val_ptr;

/* the last EMI bank, properly not used */
#define INVALID_ADDR (0xF0000000)
#define KERN_EMI_BASE (0x40000000)	/* Bank4 */

/* - AP side, using mcu config base */
/* -- AP Bank4 */
/* ((volatile unsigned int*)(MCUSYS_CFGREG_BASE+0x200)) */
#define AP_BANK4_MAP0 (0)
/* ((volatile unsigned int*)(MCUSYS_CFGREG_BASE+0x204)) */
#define AP_BANK4_MAP1 (0)

/* - MD side, using infra config base */
#define DBG_FLAG_DEBUG		(1<<0)
#define DBG_FLAG_JTAG		(1<<1)
#define MD_DBG_JTAG_BIT		(1<<0)

#ifndef CCCI_KMODULE_ENABLE
#define ccci_write32(b, a, v)  \
do { \
	writel(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)


#define ccci_write16(b, a, v)  \
do { \
	writew(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)


#define ccci_write8(b, a, v)  \
do { \
	writeb(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)


#define ccci_read32(b, a)               ioread32((void __iomem *)((b)+(a)))
#define ccci_read16(b, a)               ioread16((void __iomem *)((b)+(a)))
#define ccci_read8(b, a)                ioread8((void __iomem *)((b)+(a)))

#endif

#ifdef SET_EMI_STEP_BY_STAGE
void ccci_set_mem_access_protection_1st_stage(struct ccci_modem *md);
void ccci_set_mem_access_protection_second_stage(int md_id);
#endif

unsigned int ccci_get_md_debug_mode(struct ccci_modem *md);
void ccci_get_platform_version(char *ver);

int ccci_plat_common_init(void);
//int ccci_platform_init(struct ccci_modem *md);
void ccci_platform_common_init(struct ccci_modem *md);

void ccci_platform_init_6765(struct ccci_modem *md);

//void ccci_reset_ccif_hw(unsigned char md_id,
//			int ccif_id, void __iomem *baseA, void __iomem *baseB);
//void ccci_set_clk_cg(struct ccci_modem *md, unsigned int is_on);
#ifdef ENABLE_DRAM_API
extern phys_addr_t get_max_DRAM_size(void);
#endif
int Is_MD_EMI_voilation(void);
/* ((ccci_get_md_debug_mode(md)&(DBG_FLAG_JTAG|DBG_FLAG_DEBUG)) != 0) */
#define MD_IN_DEBUG(md) (0)
#endif	/* __CCCI_PLATFORM_H__ */
