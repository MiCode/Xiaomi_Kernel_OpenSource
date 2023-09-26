/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <stdarg.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/kallsyms.h>
#include <linux/processor.h>

#include <asm/cacheflush.h>

#include <mtk_wd_api.h>
#include <mt-plat/aee.h>
#include <mt-plat/mtk_ram_console.h>
#if defined(CONFIG_FIQ_GLUE)
#include <mt-plat/fiq_smp_call.h>
#endif
#include <mrdump.h>
#ifdef CONFIG_MTK_WATCHDOG
#include <ext_wd_drv.h>
#endif
#ifdef CONFIG_MTK_DFD_INTERNAL_DUMP
#include <mtk_platform_debug.h>
#endif
#include "mrdump_private.h"

static unsigned long mrdump_output_lbaooo;

void __mrdump_create_oops_dump(enum AEE_REBOOT_MODE reboot_mode,
		struct pt_regs *regs, const char *msg, ...)
{
	va_list ap;
	struct mrdump_crash_record *crash_record;
	void *creg;
	int cpu;
	elf_gregset_t *reg;

	if (!mrdump_cblock)
		return;

	crash_record = &mrdump_cblock->crash_record;

	local_irq_disable();
	cpu = get_HW_cpuid();

	if (cpu >= 0 && cpu < nr_cpu_ids) {
		switch (sizeof(unsigned long)) {
		case 4:
			reg = (elf_gregset_t *)&crash_record->cpu_reg[cpu].arm32_reg.arm32_regs;
			creg = (void *)&crash_record->cpu_reg[cpu].arm32_reg.arm32_creg;
			break;
		case 8:
			reg = (elf_gregset_t *)&crash_record->cpu_reg[cpu].arm64_reg.arm64_regs;
			creg = (void *)&crash_record->cpu_reg[cpu].arm64_reg.arm64_creg;
			break;
		default:
			BUILD_BUG();
		}

		/* null regs, no register dump */
		if (regs)
			elf_core_copy_kernel_regs(reg, regs);

		mrdump_save_control_register(creg);
	}

	va_start(ap, msg);
	vsnprintf(crash_record->msg, sizeof(crash_record->msg), msg, ap);
	va_end(ap);

	crash_record->fault_cpu = cpu;

	/* FIXME: Check reboot_mode is valid */
	crash_record->reboot_mode = reboot_mode;
}

#if CONFIG_SYSFS

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

int __init mrdump_full_init(void)
{
#ifdef CONFIG_MTK_DFD_INTERNAL_DUMP
	int res;
#endif
	mrdump_cblock->enabled = MRDUMP_ENABLE_COOKIE;
	__flush_dcache_area(mrdump_cblock,
		sizeof(struct mrdump_control_block));

#ifdef CONFIG_MTK_DFD_INTERNAL_DUMP
	/* DFD cache dump */
	res = dfd_setup(DFD_EXTENDED_DUMP);
	if (res == -1)
		pr_notice("%s: DFD_EXTENDED_DUMP disabled\n", __func__);
	else
		pr_notice("%s: DFD_EXTENDED_DUMP enabled\n", __func__);
#endif

#if CONFIG_SYSFS
	if (sysfs_create_group(kernel_kobj, &attr_group)) {
		pr_notice("MT-RAMDUMP: sysfs create sysfs failed\n");
		return -ENOMEM;
	}
#endif
	pr_info("%s: MT-RAMDUMP enabled done\n", __func__);
	return 0;
}

static int param_set_mrdump_lbaooo(const char *val,
		const struct kernel_param *kp)
{
	int retval = 0;

	if (mrdump_cblock) {
		retval = param_set_ulong(val, kp);

		if (retval == 0) {
			mrdump_cblock->output_fs_lbaooo = mrdump_output_lbaooo;
			__flush_dcache_area(mrdump_cblock,
					sizeof(struct mrdump_control_block));
		}
	}

	return retval;
}

/* sys/modules/mrdump/parameter/lbaooo */
struct kernel_param_ops param_ops_mrdump_lbaooo = {
	.set = param_set_mrdump_lbaooo,
	.get = param_get_ulong,
};

param_check_ulong(lbaooo, &mrdump_output_lbaooo);
/* 0644: S_IRUGO | S_IWUSR */
module_param_cb(lbaooo, &param_ops_mrdump_lbaooo, &mrdump_output_lbaooo,
		0644);
__MODULE_PARM_TYPE(lbaooo, unsigned long);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek MRDUMP module");
MODULE_AUTHOR("MediaTek Inc.");

