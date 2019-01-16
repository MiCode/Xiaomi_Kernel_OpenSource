#ifndef _CCCCI_PLATFORM_H_
#define _CCCCI_PLATFORM_H_

#include <mach/sync_write.h>
#include <mach/ccci_config.h>
#include "ccci_core.h"

#define INVALID_ADDR (0xF0000000) // the last EMI bank, properly not used
#define KERN_EMI_BASE (0x40000000) // Bank4

//- AP side, using mcu config base
//-- AP Bank4
#define AP_BANK4_MAP0 (0) // ((volatile unsigned int*)(MCUSYS_CFGREG_BASE+0x200))
#define AP_BANK4_MAP1 (0) // ((volatile unsigned int*)(MCUSYS_CFGREG_BASE+0x204))

//- MD side, using infra config base
//-- SWITCH
#define SIM_CTRL2 ((unsigned int*)(GPIO_BASE+0xE40)) 
#define SIM_CTRL3 ((unsigned int*)(GPIO_BASE+0xE50)) 
#define DBG_FLAG_DEBUG		(1<<0)
#define DBG_FLAG_JTAG		(1<<1)
#define MD_DEBUG_MODE 		(DBGAPB_BASE+0x1A010)
#define MD_DBG_JTAG_BIT		(1<<0)

#define ccci_write32(b, a, v)           mt_reg_sync_writel(v, (b)+(a))
#define ccci_write16(b, a, v)           mt_reg_sync_writew(v, (b)+(a))
#define ccci_write8(b, a, v)            mt_reg_sync_writeb(v, (b)+(a))
#define ccci_read32(b, a)               ioread32((void __iomem *)((b)+(a)))
#define ccci_read16(b, a)               ioread16((void __iomem *)((b)+(a)))
#define ccci_read8(b, a)                ioread8((void __iomem *)((b)+(a)))


void ccci_clear_md_region_protection(struct ccci_modem *md);
void ccci_set_mem_access_protection(struct ccci_modem *md);
void ccci_set_ap_region_protection(struct ccci_modem *md);
void ccci_set_mem_remap(struct ccci_modem *md, unsigned long smem_offset, phys_addr_t invalid);
unsigned int ccci_get_md_debug_mode(struct ccci_modem *md);
void ccci_get_platform_version(char * ver);
void ccci_set_dsp_region_protection(struct ccci_modem *md, int loaded);
void ccci_clear_dsp_region_protection(struct ccci_modem *md);
int ccci_plat_common_init(void);
int ccci_platform_init(struct ccci_modem *md);

#define MD_IN_DEBUG(md) ((ccci_get_md_debug_mode(md)&(DBG_FLAG_JTAG|DBG_FLAG_DEBUG))!=0)

#endif //_CCCCI_PLATFORM_H_
