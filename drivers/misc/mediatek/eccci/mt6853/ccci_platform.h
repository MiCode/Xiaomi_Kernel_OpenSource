/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __CCCI_PLATFORM_H__
#define __CCCI_PLATFORM_H__

#include <mt-plat/sync_write.h>
#include "ccci_config.h"
#include "../hif/ccci_hif_internal.h"
#include "modem_sys.h"
#include "../hif/ccci_hif_ccif.h"

struct  ccci_plat_val {
	void __iomem *infra_ao_base;
	unsigned int md_gen;
	unsigned long offset_epof_md1;
	void __iomem *md_plat_info;
};

static struct ccci_plat_val md_cd_plat_val_ptr;

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

int ccci_plat_common_init(void);
int ccci_platform_init(struct ccci_modem *md);
void ccci_reset_ccif_hw(unsigned char md_id,
			int ccif_id, void __iomem *baseA, void __iomem *baseB, struct md_ccif_ctrl *md_ctrl);
extern void ccci_md_devapc_register_cb(void);
#ifdef ENABLE_DRAM_API
extern phys_addr_t get_max_DRAM_size(void);
#endif
int Is_MD_EMI_voilation(void);
#define MD_IN_DEBUG(md) (0)
#endif	/* __CCCI_PLATFORM_H__ */
