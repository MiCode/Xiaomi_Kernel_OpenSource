/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* Architecture-specific VCM functions */

#include <linux/kernel.h>
#include <linux/vcm_mm.h>

#include <asm/pgtable-hwdef.h>
#include <asm/tlbflush.h>

#define MRC(reg, processor, op1, crn, crm, op2)				\
__asm__ __volatile__ (							\
"   mrc   "   #processor "," #op1 ", %0,"  #crn "," #crm "," #op2 " \n" \
: "=r" (reg))

#define RCP15_PRRR(reg)		MRC(reg, p15, 0, c10, c2, 0)
#define RCP15_NMRR(reg) 	MRC(reg, p15, 0, c10, c2, 1)


/* Local type attributes (not the same as VCM) */
#define ARM_MT_NORMAL		2
#define ARM_MT_STRONGLYORDERED	0
#define ARM_MT_DEVICE		1

#define ARM_CP_NONCACHED	0
#define ARM_CP_WB_WA		1
#define ARM_CP_WB_NWA		3
#define ARM_CP_WT_NWA		2

#define smmu_err(a, ...)						\
	pr_err("ERROR %s %i " a, __func__, __LINE__, ##__VA_ARGS__)

#define FL_OFFSET(va)	(((va) & 0xFFF00000) >> 20)
#define SL_OFFSET(va)	(((va) & 0xFF000) >> 12)

int vcm_driver_tex_class[4];

static int find_tex_class(int icp, int ocp, int mt, int nos)
{
	int i = 0;
	unsigned int prrr = 0;
	unsigned int nmrr = 0;
	int c_icp, c_ocp, c_mt, c_nos;

	RCP15_PRRR(prrr);
	RCP15_NMRR(nmrr);

	/* There are only 8 classes on this architecture */
	/* If they add more classes, registers will VASTLY change */
	for (i = 0; i < 8; i++)	{
		c_nos = prrr & (1 << (i + 24)) ? 1 : 0;
		c_mt = (prrr & (3 << (i * 2))) >> (i * 2);
		c_icp = (nmrr & (3 << (i * 2))) >> (i * 2);
		c_ocp = (nmrr & (3 << (i * 2 + 16))) >> (i * 2 + 16);

		if (icp == c_icp && ocp == c_ocp && c_mt == mt && c_nos == nos)
			return i;
	}
	smmu_err("Could not find TEX class for ICP=%d, OCP=%d, MT=%d, NOS=%d\n",
		 icp, ocp, mt, nos);

	/* In reality, we may want to remove this panic. Some classes just */
	/* will not be available, and will fail in smmu_set_attr */
	panic("SMMU: Could not determine TEX attribute mapping.\n");
	return -1;
}


int vcm_setup_tex_classes(void)
{
	unsigned int cpu_prrr;
	unsigned int cpu_nmrr;

	if (!(get_cr() & CR_TRE))	/* No TRE? */
		panic("TEX remap not enabled, but the SMMU driver needs it!\n");

	RCP15_PRRR(cpu_prrr);
	RCP15_NMRR(cpu_nmrr);

	vcm_driver_tex_class[VCM_DEV_ATTR_NONCACHED] =
		find_tex_class(ARM_CP_NONCACHED, ARM_CP_NONCACHED,
			       ARM_MT_NORMAL, 1);

	vcm_driver_tex_class[VCM_DEV_ATTR_CACHED_WB_WA] =
		find_tex_class(ARM_CP_WB_WA, ARM_CP_WB_WA,
			       ARM_MT_NORMAL, 1);

	vcm_driver_tex_class[VCM_DEV_ATTR_CACHED_WB_NWA] =
		find_tex_class(ARM_CP_WB_NWA, ARM_CP_WB_NWA,
			       ARM_MT_NORMAL, 1);

	vcm_driver_tex_class[VCM_DEV_ATTR_CACHED_WT] =
		find_tex_class(ARM_CP_WT_NWA, ARM_CP_WT_NWA,
			       ARM_MT_NORMAL, 1);
#ifdef DEBUG_TEX
	printk(KERN_INFO "VCM driver debug: Using TEX classes: %d %d %d %d\n",
	       vcm_driver_tex_class[VCM_DEV_ATTR_NONCACHED],
	       vcm_driver_tex_class[VCM_DEV_ATTR_CACHED_WB_WA],
	       vcm_driver_tex_class[VCM_DEV_ATTR_CACHED_WB_NWA],
	       vcm_driver_tex_class[VCM_DEV_ATTR_CACHED_WT]);
#endif
	return 0;
}


int set_arm7_pte_attr(unsigned long pt_base, unsigned long va,
					unsigned long len, unsigned int attr)
{
	unsigned long *fl_table = NULL;
	unsigned long *fl_pte = NULL;
	unsigned long fl_offset = 0;
	unsigned long *sl_table = NULL;
	unsigned long *sl_pte = NULL;
	unsigned long sl_offset = 0;
	int i;
	int sh = 0;
	int class = 0;

	/* Alignment */
	if (va & (len-1)) {
		smmu_err("misaligned va: %p\n", (void *) va);
		goto fail;
	}
	if (attr > 7) {
		smmu_err("bad attribute: %d\n", attr);
		goto fail;
	}

	sh = (attr & VCM_DEV_ATTR_SH) ? 1 : 0;
	class = vcm_driver_tex_class[attr & 0x03];

	if (class > 7 || class < 0) {	/* Bad class */
		smmu_err("bad tex class: %d\n", class);
		goto fail;
	}

	if (len != SZ_16M && len != SZ_1M &&
	    len != SZ_64K && len != SZ_4K) {
		smmu_err("bad size: %lu\n", len);
		goto fail;
	}

	fl_table = (unsigned long *) pt_base;

	if (!fl_table) {
		smmu_err("null page table\n");
		goto fail;
	}

	fl_offset = FL_OFFSET(va);	/* Upper 12 bits */
	fl_pte = fl_table + fl_offset;	/* int pointers, 4 bytes */

	if (*fl_pte == 0) {	/* Nothing there! */
		smmu_err("first level pte is 0\n");
		goto fail;
	}

	/* Supersection attributes */
	if (len == SZ_16M) {
		for (i = 0; i < 16; i++) {
			/* Clear the old bits */
			*(fl_pte+i) &= ~(PMD_SECT_S | PMD_SECT_CACHEABLE |
					 PMD_SECT_BUFFERABLE | PMD_SECT_TEX(1));

			/* Assign new class and S bit */
			*(fl_pte+i) |= sh ? PMD_SECT_S : 0;
			*(fl_pte+i) |= class & 0x01 ? PMD_SECT_BUFFERABLE : 0;
			*(fl_pte+i) |= class & 0x02 ? PMD_SECT_CACHEABLE : 0;
			*(fl_pte+i) |= class & 0x04 ? PMD_SECT_TEX(1) : 0;
		}
	} else	if (len == SZ_1M) {

		/* Clear the old bits */
		*(fl_pte) &= ~(PMD_SECT_S | PMD_SECT_CACHEABLE |
			       PMD_SECT_BUFFERABLE | PMD_SECT_TEX(1));

		/* Assign new class and S bit */
		*(fl_pte) |= sh ? PMD_SECT_S : 0;
		*(fl_pte) |= class & 0x01 ? PMD_SECT_BUFFERABLE : 0;
		*(fl_pte) |= class & 0x02 ? PMD_SECT_CACHEABLE : 0;
		*(fl_pte) |= class & 0x04 ? PMD_SECT_TEX(1) : 0;
	}

	sl_table = (unsigned long *) __va(((*fl_pte) & 0xFFFFFC00));
	sl_offset = SL_OFFSET(va);
	sl_pte = sl_table + sl_offset;

	if (len == SZ_64K) {
		for (i = 0; i < 16; i++) {
			/* Clear the old bits */
			*(sl_pte+i) &= ~(PTE_EXT_SHARED | PTE_CACHEABLE |
					 PTE_BUFFERABLE | PTE_EXT_TEX(1));

			/* Assign new class and S bit */
			*(sl_pte+i) |= sh ? PTE_EXT_SHARED : 0;
			*(sl_pte+i) |= class & 0x01 ? PTE_BUFFERABLE : 0;
			*(sl_pte+i) |= class & 0x02 ? PTE_CACHEABLE : 0;
			*(sl_pte+i) |= class & 0x04 ? PTE_EXT_TEX(1) : 0;
		}
	} else 	if (len == SZ_4K) {
		/* Clear the old bits */
		*(sl_pte) &= ~(PTE_EXT_SHARED | PTE_CACHEABLE |
			       PTE_BUFFERABLE | PTE_EXT_TEX(1));

		/* Assign new class and S bit */
		*(sl_pte) |= sh ? PTE_EXT_SHARED : 0;
		*(sl_pte) |= class & 0x01 ? PTE_BUFFERABLE : 0;
		*(sl_pte) |= class & 0x02 ? PTE_CACHEABLE : 0;
		*(sl_pte) |= class & 0x04 ? PTE_EXT_TEX(1) : 0;
	}


	mb();
	return 0;
fail:
	return 1;
}


int cpu_set_attr(unsigned long va, unsigned long len, unsigned int attr)
{
	int ret;
	pgd_t *pgd = init_mm.pgd;

	if (!pgd) {
		smmu_err("null pgd\n");
		goto fail;
	}

	ret = set_arm7_pte_attr((unsigned long)pgd, va, len, attr);

	if (ret != 0) {
		smmu_err("could not set attribute: \
					pgd=%p, va=%p, len=%lu, attr=%d\n",
			 (void *) pgd, (void *) va, len, attr);
		goto fail;
	}
	dmb();
	flush_tlb_all();
	return 0;
fail:
	return -1;
}
