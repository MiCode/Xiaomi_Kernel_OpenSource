/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_MEMINFO_H__
#define __MTK_MEMINFO_H__
#include <linux/cma.h>
#include <linux/of_reserved_mem.h>

extern void *vmap_reserved_mem(phys_addr_t start, phys_addr_t size,
		pgprot_t prot);
#ifdef CONFIG_MTK_MEMORY_LOWPOWER
extern phys_addr_t memory_lowpower_base(void);
extern phys_addr_t memory_lowpower_size(void);
extern struct single_cma_registration memory_lowpower_registration;
#endif /* end CONFIG_MTK_MEMORY_LOWPOWER */

#ifdef CONFIG_ZONE_MOVABLE_CMA
extern phys_addr_t zmc_max_zone_dma_phys;
#define ZMC_ALLOC_ALL 0x01 /* allocate all memory reserved from dts */

/* Priority of ZONE_MOVABLE_CMA users */
enum zmc_prio {
	ZMC_SSMR,
	ZMC_CHECK_MEM_STAT,
	ZMC_MLP = ZMC_CHECK_MEM_STAT,
	NR_ZMC_OWNER,
};

struct single_cma_registration {
	phys_addr_t size;
	phys_addr_t align;
	unsigned long flag;
	const char *name;
	int (*preinit)(struct reserved_mem *rmem);
	void (*init)(struct cma *cma);
	enum zmc_prio prio;
};

extern bool is_zmc_inited(void);
extern void zmc_get_range(phys_addr_t *base, phys_addr_t *size);
extern phys_addr_t zmc_base(void);
extern struct page *zmc_cma_alloc(struct cma *cma, int count,
		unsigned int align, struct single_cma_registration *p);
extern bool zmc_cma_release(struct cma *cma, struct page *pages, int count);

extern int zmc_register_client(struct notifier_block *nb);
extern int zmc_unregister_client(struct notifier_block *nb);
extern int zmc_notifier_call_chain(unsigned long val, void *v);

#define ZMC_EVENT_ALLOC_MOVABLE 0x01
#endif

#ifdef CONFIG_MTK_SSMR
extern bool memory_ssmr_inited(void);
extern struct single_cma_registration memory_ssmr_registration;
#endif /* end CONFIG_MTK_SSMR */

#endif /* end __MTK_MEMINFO_H__ */
