/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#ifndef __CCCI_PLATFORM_H__
#define __CCCI_PLATFORM_H__

#include <linux/io.h>
#include "ccci_config.h"
#include "ccci_common_config.h"
//#include "modem_sys.h"

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

extern struct regmap *syscon_regmap_lookup_by_phandle(
		struct device_node *np, const char *property);
extern int regmap_write(struct regmap *map,
		unsigned int reg, unsigned int val);
extern int regmap_read(struct regmap *map,
		unsigned int reg, unsigned int *val);

#ifdef FEATURE_LOW_BATTERY_SUPPORT
enum LOW_POEWR_NOTIFY_TYPE {
	LOW_BATTERY,
	BATTERY_PERCENT,
	OVER_CURRENT,
};
#endif

struct ccci_modem;

#ifdef SET_EMI_STEP_BY_STAGE
void ccci_set_mem_access_protection_1st_stage(struct ccci_modem *md);
void ccci_set_mem_access_protection_second_stage(int md_id);
#endif

//void ccci_get_platform_version(char *ver);
void ccci_platform_common_init(struct ccci_modem *md);
void ccci_platform_init_6873(struct ccci_modem *md);


int ccci_plat_common_init(void);
//void ccci_set_clk_cg(struct ccci_modem *md, unsigned int is_on);
#ifdef ENABLE_DRAM_API
extern phys_addr_t get_max_DRAM_size(void);
#endif
int Is_MD_EMI_voilation(void);

#define MD_IN_DEBUG(md) (0)
#endif	/* __CCCI_PLATFORM_H__ */
