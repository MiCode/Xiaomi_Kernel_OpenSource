/*
 * (C) COPYRIGHT 2015 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */

/*
 * Show VA -> PA mapping details (page table and flags)
 */

#include "mali_kbase_debug_gpu_mem_mapping.h"
#include "mali_kbase_jm_rb.h"

/* MTK */
#include <platform/mtk_platform_common.h>

#define ENTRY_TYPE_BITS		(3ULL)
#define	ENTRY_TYPE_IS_ATE	(1ULL)
#define	ENTRY_TYPE_IS_PTE	(3ULL)

#define ATE_ATTRIDX_SHIFT	(2)
#define ATE_ATTRIDX_BITS	(7ULL << ATE_ATTRIDX_SHIFT)
#define ATE_AP_SHIFT		(6)
#define ATE_AP_BITS		(3ULL << ATE_AP_SHIFT)
#define ATE_AP_NO_ACCESS	(0ULL)
#define ATE_AP_RO		(1ULL)
#define ATE_AP_WO		(2ULL)
#define ATE_AP_RW		(3ULL)
#define ATE_SH_SHIFT		(8)
#define ATE_SH_BITS		(3ULL << ATE_SH_SHIFT)
#define ATE_SH_NOT_SHARABLE	(0)
#define ATE_SH_IN_OUT_SHARE	(2)
#define ATE_SH_IN_SHARE_ONLY	(3)
#define ATE_AI_SHIFT		(10)
#define ATE_AI_BIT		(1ULL << ATE_AI_SHIFT)
#define ATE_OUT_ADDR_SHIFT	(12)
#define ATE_OUT_ADDR_BITS	(0xfffffffffULL << ATE_OUT_ADDR_SHIFT)
#define ATE_NX_SHIFT		(54)
#define ATE_NX_BIT		(1ULL << ATE_NX_SHIFT)

#define show(kctx, fmt, ...) \
	dev_MTK_err(kctx->kbdev->dev, fmt, ##__VA_ARGS__)


enum ate_flag {
	ATE_FLAG_ATTRIB_INDEX,
	ATE_FLAG_ACCESS_PERM,
	ATE_FLAG_SHAREABILITY,
	ATE_FLAG_ACCESS_INTERRUPT,
	ATE_FLAG_EXEC_NEVER,
	ATE_FLAG_NUM
};

static u64 ate_flags_masks[ATE_FLAG_NUM] = {
	[ATE_FLAG_ATTRIB_INDEX]		= ATE_ATTRIDX_BITS,
	[ATE_FLAG_ACCESS_PERM]		= ATE_AP_BITS,
	[ATE_FLAG_SHAREABILITY]		= ATE_SH_BITS,
	[ATE_FLAG_ACCESS_INTERRUPT]	= ATE_AI_BIT,
	[ATE_FLAG_EXEC_NEVER]		= ATE_NX_BIT
};

static const char *ate_flag_name(u64 mask)
{
	switch (mask) {
	case ATE_ATTRIDX_BITS:	return "Attribute Index";
	case ATE_AP_BITS:	return "Access Permissions";
	case ATE_SH_BITS:	return "Shareability";
	case ATE_AI_BIT:	return "Access Interrupt flag";
	case ATE_NX_BIT:	return "Execute never";
	}
	return NULL;
}

static const char *ate_flag_value(u64 mask, u64 entry)
{
	static const char * const digits[] = { "0", "1", "2", "3",
					       "4", "5", "6", "7" };

	switch (mask) {
	case ATE_ATTRIDX_BITS:
		return digits[((entry & mask) >> ATE_ATTRIDX_SHIFT)];
	case ATE_AP_BITS:
		switch ((entry & mask) >> ATE_AP_SHIFT) {
		case ATE_AP_NO_ACCESS:
			return "No access";
		case ATE_AP_RO:
			return "Read-only access";
		case ATE_AP_WO:
			return "Write-only access";
		case ATE_AP_RW:
			return "Read and write access";
		default:
			/* Unreachable */
			return NULL;
		}
	case ATE_SH_BITS:
		switch ((entry & mask) >> ATE_SH_SHIFT) {
		case ATE_SH_NOT_SHARABLE:
			return "Not shareable";
		case ATE_SH_IN_OUT_SHARE:
			return "Inner and outer shareable";
		case ATE_SH_IN_SHARE_ONLY:
			return "Inner shareable only";
		default:
			/* Unreachable */
			return NULL;
		}
	case ATE_AI_BIT:
	case ATE_NX_BIT:
		return digits[(entry & mask) ? 1 : 0];
	}
	return NULL;
}

static void pr_ate_flags(struct kbase_context *kctx, u64 entry)
{
	size_t i;

	show(kctx, "ATE flags: [flag name]: value\n");
	for (i = 0; i < ATE_FLAG_NUM; i++)
		show(kctx, "  [%s]: %s\n", ate_flag_name(ate_flags_masks[i]),
				ate_flag_value(ate_flags_masks[i], entry));
}


static phys_addr_t get_next_pgd(struct kbase_context *kctx,
		phys_addr_t pgd, u64 vpfn, int level)
{
	u64 *pgd_page;
	u64 pte;
	phys_addr_t target_pgd;
	struct page *p;
	size_t index = (vpfn >> (3 - level) * 9) & 0x1FF;
	struct kbase_mmu_mode const *m = kctx->kbdev->mmu_mode;
	char pte_str[32];

	lockdep_assert_held(&kctx->reg_lock);

	p = pfn_to_page(PFN_DOWN(pgd));
	pgd_page = kmap(p);
	if (!pgd_page) {
		dev_MTK_err(kctx->kbdev->dev, "%s: kmap failed!\n", __func__);
		return 0;
	}
	pte = pgd_page[index];
	kunmap(p);
	if (m->pte_is_valid(pte)) {
		target_pgd = m->pte_to_phy_addr(pte);
		snprintf(pte_str, sizeof(pte_str), " (phy=%pa)\n", &target_pgd);
	} else {
		target_pgd = 0;
		snprintf(pte_str, sizeof(pte_str), " - PTE is INVALID!\n");
	}
	if (kctx->kbdev->debug_gpu_page_tables)
		show(kctx, "  level=%d [pgd=%pa index=%-3d] -> pte=%016llx%s",
				level, &pgd, (int)index, pte, pte_str);
	return target_pgd;
}


static phys_addr_t get_bottom_pgd(struct kbase_context *kctx, u64 vpfn)
{
	phys_addr_t pgd = kctx->pgd;
	int l;

	for (l = MIDGARD_MMU_TOPLEVEL; l < 3; l++) {
		pgd = get_next_pgd(kctx, pgd, vpfn, l);
		if (!pgd) {
			dev_MTK_err(kctx->kbdev->dev, "%s: Table walk failed on level %d.\n",
					__func__, l);
			break;
		}
	}
	return pgd;
}

static phys_addr_t ate_to_phy_addr(u64 entry)
{
	if ((entry & ENTRY_TYPE_BITS) != ENTRY_TYPE_IS_ATE)
		return 0;

	return entry & MIDGARD_MMU_PA_MASK;
}

static u64 *get_ate_page(struct kbase_context *kctx, phys_addr_t pgd)
{
	struct page *p;
	u64 *ate_page;

	p = pfn_to_page(PFN_DOWN(pgd));
	ate_page = kmap(p);
	if (!ate_page) {
		dev_MTK_err(kctx->kbdev->dev, "%s: kmap failed!\n", __func__);
		return NULL;
	}
	return ate_page;
}

static void release_ate_page(phys_addr_t pgd)
{
	kunmap(pfn_to_page(PFN_DOWN(pgd)));
}

static phys_addr_t get_phy_addr(struct kbase_context *kctx, u64 va)
{
	u64 vpfn = va >> PAGE_SHIFT;
	size_t offset = va & MIDGARD_MMU_PAGE_MASK;
	size_t index = vpfn & 0x1FF;
	phys_addr_t bottom_pgd, phy_addr;
	u64 *ate_page;
	u64 ate;
	struct kbase_mmu_mode const *m = kctx->kbdev->mmu_mode;
	char ate_str[32];

	if (kctx->kbdev->debug_gpu_page_tables)
		show(kctx, "Page table state: level [PGD index] -> PTE (next PGD or PFN\n");

	kbase_gpu_vm_lock(kctx);

	bottom_pgd = get_bottom_pgd(kctx, vpfn);
	if (!bottom_pgd)
		goto err;

	ate_page = get_ate_page(kctx, bottom_pgd);
	if (!ate_page)
		goto err;

	ate = ate_page[index];
	release_ate_page(bottom_pgd);
	if (m->ate_is_valid(ate)) {
		phy_addr = ate_to_phy_addr(ate);
		snprintf(ate_str, sizeof(ate_str), " (phy=%pa)\n", &phy_addr);
	} else {
		phy_addr = 0;
		snprintf(ate_str, sizeof(ate_str), " - ATE is INVALID!\n");
	}
	if (kctx->kbdev->debug_gpu_page_tables)
		show(kctx, "  level=3 [pgd=%pa index=%-3d] -> ate=%016llx%s\n",
				&bottom_pgd, (int)index, ate, ate_str);
	if (!phy_addr)
		goto err;

	if (kctx->kbdev->debug_gpu_page_tables)
		pr_ate_flags(kctx, ate);
	kbase_gpu_vm_unlock(kctx);
	return phy_addr + offset;
err:
	if (bottom_pgd)
		dev_MTK_err(kctx->kbdev->dev, "%s: Table walk failed on level 3.\n",
				__func__);
	kbase_gpu_vm_unlock(kctx);
	return 0;
}


/**
 * kbase_debug_gpu_mem_mapping - walk GPU page table and print out the mapping.
 * @kctx: kbase context
 * @va: virtual address to examine
 *
 * Returns physical address mapped to va or 0 if no mapping.
 */
phys_addr_t kbase_debug_gpu_mem_mapping(struct kbase_context *kctx, u64 va)
{
	phys_addr_t phy_addr = get_phy_addr(kctx, va);

	if (!phy_addr && kctx->kbdev->debug_gpu_page_tables) {
		show(kctx, "VA %016llx NOT mapped on GPU, process:%s\n", va, kctx->process_name);
		return 0;
	}
	if (kctx->kbdev->debug_gpu_page_tables)
		show(kctx, "VA %016llx mapped -> PA %pa\n", va, &phy_addr);

	return phy_addr;

}

#define KBASE_MMU_PAGE_ENTRIES 512

#define PRINT_DETAIL 0

static bool kbasep_mmu_dump_level(struct kbase_context *kctx, phys_addr_t pgd, int level, u64 pa)
{
	phys_addr_t target_pgd;
	u64 *pgd_page;
	int i;
	struct kbase_mmu_mode const *mmu_mode;
	bool ret =false;
	u64 m_pgd, phy_u64;
	phys_addr_t phy_addr;

	KBASE_DEBUG_ASSERT(NULL != kctx);
	lockdep_assert_held(&kctx->reg_lock);
#if PRINT_DETAIL
	dev_MTK_info(kctx->kbdev->dev,"======== mmu level:%d\n", level);
#endif
	mmu_mode = kctx->kbdev->mmu_mode;

	pgd_page = kmap(pfn_to_page(PFN_DOWN(pgd)));
	if (!pgd_page) {
		dev_warn(kctx->kbdev->dev, "kbasep_mmu_dump_level: kmap failure\n");
		return ret;
	}

	m_pgd = pgd | level;
	phy_addr = mmu_mode->pte_to_phy_addr(m_pgd);
	phy_u64 = (u64)phy_addr;
	phy_u64 &= PAGE_MASK;
#if PRINT_DETAIL
	dev_MTK_info(kctx->kbdev->dev,"=====%pa, %llx, %llx \n", &phy_addr, phy_u64, m_pgd);
#endif
	if ((phy_u64&MIDGARD_MMU_PA_MASK) == (pa&MIDGARD_MMU_PA_MASK))
		goto success;
	/* A modified physical address that contains the page table level */
	if (level == 3) {
		u64 *ate_page,ate;

		ate_page = pgd_page;
		for (i=0; i < KBASE_MMU_PAGE_ENTRIES; i++) {

			ate = ate_page[i];
			/*if (mmu_mode->ate_is_valid(ate))*/ {
				phy_addr = ate_to_phy_addr(ate);
				phy_u64 = (u64) phy_addr;
				phy_u64 &= PAGE_MASK;
#if PRINT_DETAIL
				dev_MTK_info(kctx->kbdev->dev,"=%pa \n", &phy_addr);
#endif
				if ((phy_u64&MIDGARD_MMU_PA_MASK) == (pa&MIDGARD_MMU_PA_MASK))
					goto success;

			}
		}

	}
	else {
		u64 pte;

		/* Followed by the page table itself */
		for (i=0; i < KBASE_MMU_PAGE_ENTRIES; i++) {
			pte = pgd_page[i];
			/*if (mmu_mode->pte_is_valid(pte))*/ {
				phy_addr = mmu_mode->pte_to_phy_addr(pte);
#if PRINT_DETAIL
				dev_MTK_info(kctx->kbdev->dev,"=%pa \n", &phy_addr);
#endif
				phy_u64 = (u64) phy_addr;
				phy_u64 &= PAGE_MASK;
				if ((phy_u64&MIDGARD_MMU_PA_MASK) == (pa&MIDGARD_MMU_PA_MASK))
					goto success;

			}
		}
	}


	if (level < 3) {
	    for (i = 0; i < KBASE_MMU_PAGE_ENTRIES; i++) {
			if (mmu_mode->pte_is_valid(pgd_page[i])) {
				target_pgd = mmu_mode->pte_to_phy_addr(pgd_page[i]);

				ret = kbasep_mmu_dump_level(kctx, target_pgd, level + 1, pa);
				if (ret == true)
					goto success;
			}
	    }
	}

	kunmap(pfn_to_page(PFN_DOWN(pgd)));
	return ret;

success: ret = true;
	kunmap(pfn_to_page(PFN_DOWN(pgd)));

	return ret;
}
static bool kbasep_check_va_reg(struct kbase_context *kctx, u64 pa)
{
	bool ret = false;
	struct rb_node *p;
	int i;
	u64 phy_u64;

	for (p = rb_first(&kctx->reg_rbtree); p; p = rb_next(p)) {
		struct kbase_va_region *reg;

		reg = rb_entry(p, struct kbase_va_region, rblink);
		if (reg->gpu_alloc == NULL)
			continue;
		for (i = 0; i < reg->gpu_alloc->nents; i++) {
			phy_u64 = (u64)reg->gpu_alloc->pages[i];
			phy_u64 &= PAGE_MASK;
			if ((phy_u64&MIDGARD_MMU_PA_MASK) == (pa&MIDGARD_MMU_PA_MASK)) {
				dev_MTK_info(kctx->kbdev->dev,"    Get the PA:%016llx in VA region, kctx:%p, PID:%llx\n, Process:%s",
					phy_u64, kctx, (u64)(kctx->tgid), kctx->process_name);
				ret = true;
			}
		}
	}
	return ret;
}
/*
static int kbase_backend_nr_atoms_on_slot(struct kbase_device *kbdev, int js)
{
	int nr = 0;
	int i;

	lockdep_assert_held(&kbdev->js_data.runpool_irq.lock);

	for (i = 0; i < SLOT_RB_SIZE; i++) {
		if (kbase_gpu_inspect(kbdev, js, i))
			nr++;
	}

	return nr;
}
*/
void kbase_check_PA(u64 pa)
{
	mtk_trigger_emi_report(pa);
}
bool kbase_debug_gpu_mem_mapping_check_pa(u64 pa)
{
	bool ret = false;
	struct list_head *entry;
	const struct list_head *kbdev_list;
	struct kbase_context *kctx = NULL;
	int /*s,*/ i;
	struct kbasep_js_device_data *js_devdata;
	/*unsigned long flags;*/

	pr_MTK_info("Mali PA page check:%llx\n", pa);
	pa &= PAGE_MASK;
	kbdev_list = kbase_dev_list_get();
	if(kbdev_list == NULL)
		return false;
	list_for_each(entry, kbdev_list) {
		struct kbase_device *kbdev = NULL;
		struct kbasep_kctx_list_element *element;

		kbdev = list_entry(entry, struct kbase_device, entry);
		if(kbdev == NULL) {
			pr_MTK_info("    No Mali device\n");
			return false;
		}

		js_devdata = &kbdev->js_data;
/*
		spin_lock_irqsave(&js_devdata->runpool_irq.lock, flags);
		for (s = 0; s < kbdev->gpu_props.num_job_slots; s++) {
			struct kbase_jd_atom *atom = NULL;

			if (kbase_backend_nr_atoms_on_slot(kbdev, s) > 0) {
				atom = kbase_gpu_inspect(kbdev, s, 0);
				KBASE_DEBUG_ASSERT(atom != NULL);
			}
			else
				pr_MTK_info("No atoms in slot :%d\n", s);
			if (atom != NULL) {
				kbase_job_slot_hardstop(atom->kctx, s, atom);
				pr_MTK_info("hard-stop\n");
			}
		}
		spin_unlock_irqrestore(&js_devdata->runpool_irq.lock, flags);
*/

		list_for_each_entry(element, &kbdev->kctx_list, link) {
			/* list for each kctx opened on this device */
			kctx = element->kctx;
			kbase_gpu_vm_lock(kctx);
		}

		list_for_each_entry(element, &kbdev->kctx_list, link) {
			/* list for each kctx opened on this device */
			kctx = element->kctx;
			ret = kbasep_mmu_dump_level(kctx, kctx->pgd, MIDGARD_MMU_TOPLEVEL, pa);
			if (ret == true) {
				dev_MTK_info(kctx->kbdev->dev,"    Get the PA:%016llx, %s, PID:%llx\n",
						pa, kctx->process_name,  (u64)(kctx->tgid));
			}
			else {
				dev_MTK_info(kctx->kbdev->dev,"    Didn't get the PA:%016llx in :%s\n",
						pa, kctx->process_name);
			}
			ret = kbasep_check_va_reg(kctx, pa);
			if (ret == false) {
				dev_MTK_info(kctx->kbdev->dev,"    Didn't get the PA:%016llx in VA region\n", pa);
			}

			dev_MTK_info(kctx->kbdev->dev,"\n Map history: \n");
			for (i = 0; i < TRACE_MAP_COUNT; i++)
				dev_MTK_info(kctx->kbdev->dev," VA:%llx, PA:%llx\n",
						kctx->map_pa_trace[0][i], kctx->map_pa_trace[1][i]);
			dev_MTK_info(kctx->kbdev->dev,"\n Unap history: \n");
			for (i = 0; i < TRACE_MAP_COUNT; i++)
				dev_MTK_info(kctx->kbdev->dev," VA:%llx, PA:%llx\n",
						kctx->unmap_pa_trace[0][i], kctx->unmap_pa_trace[1][i]);

			kbase_gpu_vm_unlock(kctx);

			dev_MTK_info(kctx->kbdev->dev,"\n MMU REG history: \n");
			for (i = 0; i < TRACE_MMU_REG_COUNT; i++)
				dev_MTK_info(kctx->kbdev->dev," MMU offset:%x, VAL:%x\n",
					kbdev->mmu_reg_trace[0][i], kbdev->mmu_reg_trace[1][i]);
		}

		mtk_trigger_aee_report("check_pa");
	}
	kbase_dev_list_put(kbdev_list);

	if(kctx == NULL) {
		pr_MTK_info("    No Mali context \n");
		return false;
	}

	return ret;
}

KBASE_EXPORT_SYMBOL(kbase_debug_gpu_mem_mapping_check_pa);

KBASE_EXPORT_SYMBOL(kbase_debug_gpu_mem_mapping);

