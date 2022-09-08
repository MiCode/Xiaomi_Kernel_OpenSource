// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/bug.h>
#include <linux/cpufeature.h>
#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <asm/memory.h>
#include <asm/pgtable-hwdef.h>
#include <asm/sections.h>

#include <mt-plat/mrdump.h>
#include "mrdump_private.h"

struct mrdump_control_block *mrdump_cblock;

static unsigned long mrdump_output_lbaooo;

#if IS_ENABLED(CONFIG_SYSFS)

static ssize_t mrdump_version_show(struct kobject *kobj,
		struct kobj_attribute *kattr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s", MRDUMP_GO_DUMP);
}

static struct kobj_attribute mrdump_version_attribute =
	__ATTR(mrdump_version, 0400, mrdump_version_show, NULL);

static struct attribute *attrs[] = {
	&mrdump_version_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

#endif

#if IS_ENABLED(CONFIG_ARM64)
static inline u64 read_tcr_el1_t1sz(void)
{
	return (read_sysreg(tcr_el1) & TCR_T1SZ_MASK) >> TCR_T1SZ_OFFSET;
}

static inline u64 read_kernel_pac_mask(void)
{
	return system_supports_address_auth() ? ptrauth_kernel_pac_mask() : 0;
}
#endif

#if IS_ENABLED(CONFIG_KALLSYMS)
#if !IS_ENABLED(CONFIG_KALLSYMS_BASE_RELATIVE)
static void mrdump_cblock_kallsyms_init(struct mrdump_ksyms_param *kparam)
{
	struct mrdump_ksyms_param tmp_kp;
	unsigned long start_addr;

	memset(&tmp_kp, 0, sizeof(struct mrdump_ksyms_param));
	start_addr = (unsigned long)kallsyms_addresses;
	tmp_kp.tag[0] = 'K';
	tmp_kp.tag[1] = 'S';
	tmp_kp.tag[2] = 'Y';
	tmp_kp.tag[3] = 'M';

	switch (sizeof(unsigned long)) {
	case 4:
		tmp_kp.flag = KSYM_32;
		break;
	case 8:
		tmp_kp.flag = KSYM_64;
		break;
	default:
		BUILD_BUG();
	}
	tmp_kp.start_addr = __pa_symbol(start_addr);
	tmp_kp.size = (unsigned long)&kallsyms_token_index - start_addr + 512;
	tmp_kp.crc = crc32(0, (unsigned char *)start_addr, tmp_kp.size);
	tmp_kp.addresses_off = (unsigned long)&kallsyms_addresses - start_addr;
	tmp_kp.num_syms_off = (unsigned long)&kallsyms_num_syms - start_addr;
	tmp_kp.names_off = (unsigned long)&kallsyms_names - start_addr;
	tmp_kp.markers_off = (unsigned long)&kallsyms_markers - start_addr;
	tmp_kp.token_table_off =
		(unsigned long)&kallsyms_token_table - start_addr;
	tmp_kp.token_index_off =
		(unsigned long)&kallsyms_token_index - start_addr;
	memcpy_toio(kparam, &tmp_kp, sizeof(struct mrdump_ksyms_param));
}
#else
static void mrdump_cblock_kallsyms_init(struct mrdump_ksyms_param *kparam)
{
	struct mrdump_ksyms_param tmp_kp;
	unsigned long start_addr;

	memset(&tmp_kp, 0, sizeof(struct mrdump_ksyms_param));
	start_addr = aee_get_kallsyms_addresses();
	tmp_kp.tag[0] = 'K';
	tmp_kp.tag[1] = 'S';
	tmp_kp.tag[2] = 'Y';
	tmp_kp.tag[3] = 'M';
	tmp_kp.flag |= 1 << MKP_BIT_SHIFT_RELATIVE;
#if IS_ENABLED(CONFIG_KALLSYMS_ABSOLUTE_PERCPU)
	tmp_kp.flag |= 1 << MKP_BIT_SHIFT_ABS_PERCPU;
#endif
	switch (sizeof(unsigned long)) {
	case 4:
		break;
	case 8:
		tmp_kp.flag |= 1 << MKP_BIT_SHIFT_ARCH64;
		break;
	default:
		BUILD_BUG();
	}

	tmp_kp.start_addr = __pa_symbol(start_addr);
	tmp_kp.size = (u32)(aee_get_kti_addresses() - start_addr + 512);
	tmp_kp.crc = crc32(0, (unsigned char *)start_addr, tmp_kp.size);
	tmp_kp.addresses_off = 0;
	tmp_kp.num_syms_off = (u32)aee_get_kns_off();
	tmp_kp.names_off = (u32)aee_get_kn_off();
	tmp_kp.markers_off = (u32)aee_get_km_off();
	tmp_kp.token_table_off = (u32)aee_get_ktt_off();
	tmp_kp.token_index_off = (u32)aee_get_kti_off();

	memcpy_toio(kparam, &tmp_kp, sizeof(struct mrdump_ksyms_param));
}
#endif
#endif

void mrdump_cblock_late_init(void)
{
	struct mrdump_machdesc *machdesc_p;

	if (!mrdump_cblock) {
		pr_notice("%s: mrdump_cb not mapped\n", __func__);
		return;
	}

	machdesc_p = &mrdump_cblock->machdesc;
#if IS_ENABLED(CONFIG_KALLSYMS)
	mrdump_cblock_kallsyms_init(&machdesc_p->kallsyms);
#endif
	machdesc_p->kimage_stext = (uint64_t)aee_get_text();
	machdesc_p->kimage_etext = (uint64_t)aee_get_etext();
	machdesc_p->kimage_stext_real = (uint64_t)aee_get_stext();
	mrdump_cblock->machdesc_crc = crc32(0, machdesc_p,
			sizeof(struct mrdump_machdesc));
	pr_notice("%s: done.\n", __func__);
}

__init void mrdump_cblock_init(const struct mrdump_params *mparams)
{
	struct mrdump_machdesc *machdesc_p;

	if (strcmp(mparams->lk_version, MRDUMP_GO_DUMP) != 0) {
		pr_notice("%s: ramdump disabled, lk version %s not matched.\n",
			  __func__, mparams->lk_version);
		return;
	}

	if (mparams->cb_addr == 0) {
		pr_notice("%s: mrdump control address cannot be 0\n",
			  __func__);
		return;
	}
	if (mparams->cb_size < sizeof(struct mrdump_control_block)) {
		pr_notice("%s: not enough space for mrdump control block\n",
			  __func__);
		return;
	}

	mrdump_cblock = ioremap(mparams->cb_addr, mparams->cb_size);
	if (!mrdump_cblock) {
		pr_notice("%s: mrdump_cb not mapped\n", __func__);
		return;
	}
	memset_io(mrdump_cblock, 0,
		sizeof(struct mrdump_control_block) +
		nr_cpu_ids * sizeof(mrdump_cblock->crash_record.cpu_reg[0]));
	memcpy_toio(mrdump_cblock->sig, MRDUMP_GO_DUMP,
			sizeof(mrdump_cblock->sig));

	machdesc_p = &mrdump_cblock->machdesc;
	machdesc_p->nr_cpus = nr_cpu_ids;
	machdesc_p->page_offset = (uint64_t)PAGE_OFFSET;
#if IS_ENABLED(CONFIG_ARM64)
	machdesc_p->tcr_el1_t1sz = (uint64_t)read_tcr_el1_t1sz();
	machdesc_p->kernel_pac_mask = (uint64_t)read_kernel_pac_mask();
#endif
#if defined(KIMAGE_VADDR)
	machdesc_p->kimage_vaddr = KIMAGE_VADDR;
#endif
#if defined(CONFIG_ARM64)
	machdesc_p->kimage_voffset = (unsigned long)kimage_voffset;
#endif

	machdesc_p->vmalloc_start = (uint64_t)VMALLOC_START;
	machdesc_p->vmalloc_end = (uint64_t)VMALLOC_END;

	machdesc_p->modules_start = (uint64_t)MODULES_VADDR;
	machdesc_p->modules_end = (uint64_t)MODULES_END;

	machdesc_p->phys_offset = (uint64_t)(phys_addr_t)PHYS_OFFSET;
	machdesc_p->master_page_table = mrdump_get_mpt();

#if defined(CONFIG_SPARSEMEM_VMEMMAP)
	machdesc_p->memmap = (uintptr_t)vmemmap;
#endif

	machdesc_p->pageflags = (1UL << PG_uptodate) + (1UL << PG_dirty) +
				(1UL << PG_lru) + (1UL << PG_writeback);

	machdesc_p->struct_page_size = (uint32_t)sizeof(struct page);

#ifdef MODULE
	mrdump_cblock->machdesc_crc = crc32(0, machdesc_p,
			sizeof(struct mrdump_machdesc));
#else
	mrdump_cblock_late_init();
#endif

#if IS_ENABLED(CONFIG_SYSFS)
	if (sysfs_create_group(kernel_kobj, &attr_group)) {
		pr_notice("MT-RAMDUMP: sysfs create sysfs failed\n");
		return;
	}
#endif

	pr_notice("%s: done.\n", __func__);
}


static int param_set_mrdump_lbaooo(const char *val,
				   const struct kernel_param *kp)
{
	int retval = 0;

	if (mrdump_cblock) {
		retval = param_set_ulong(val, kp);
		if (!retval)
			mrdump_cblock->output_fs_lbaooo = mrdump_output_lbaooo;
	}
	return retval;
}

/* sys/modules/mrdump/parameter/lbaooo */
struct kernel_param_ops param_ops_mrdump_lbaooo = {
	.set = param_set_mrdump_lbaooo,
	.get = param_get_ulong,
};

param_check_ulong(lbaooo, &mrdump_output_lbaooo);
module_param_cb(lbaooo, &param_ops_mrdump_lbaooo, &mrdump_output_lbaooo,
		0644);
__MODULE_PARM_TYPE(lbaooo, "ulong");
