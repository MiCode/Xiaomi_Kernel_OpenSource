// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <stdarg.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/processor.h>

#include <mt-plat/mboot_params.h>
#include <mrdump.h>
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

	if (mrdump_cblock) {
		crash_record = &mrdump_cblock->crash_record;
		local_irq_disable();
		cpu = get_HW_cpuid();
		if (cpu < 0 || cpu >= nr_cpu_ids)
			return;
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

		if (cpu >= 0 && cpu < nr_cpu_ids) {
			/* null regs, no register dump */
			if (regs)
				elf_core_copy_kernel_regs(reg, regs);
			mrdump_save_control_register(creg);
		}

		va_start(ap, msg);
		vsnprintf(crash_record->msg, sizeof(crash_record->msg), msg,
				ap);
		va_end(ap);

		crash_record->fault_cpu = cpu;

		/* FIXME: Check reboot_mode is valid */
		crash_record->reboot_mode = reboot_mode;
	}
}

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

int __init mrdump_full_init(void)
{
	mrdump_cblock->enabled = MRDUMP_ENABLE_COOKIE;
	/* TODO: remove flush APIs after full ramdump support  HW_Reboot*/
	aee__flush_dcache_area(mrdump_cblock,
			sizeof(struct mrdump_control_block));
	pr_info("%s: MT-RAMDUMP enabled done\n", __func__);
#if IS_ENABLED(CONFIG_SYSFS)
	if (sysfs_create_group(kernel_kobj, &attr_group)) {
		pr_notice("MT-RAMDUMP: sysfs create sysfs failed\n");
		return -ENOMEM;
	}
#endif
	return 0;
}

static int param_set_mrdump_lbaooo(const char *val,
		const struct kernel_param *kp)
{
	int retval = 0;

	if (mrdump_cblock) {
		retval = param_set_ulong(val, kp);

		if (!retval) {
			mrdump_cblock->output_fs_lbaooo = mrdump_output_lbaooo;
/* TODO: remove flush APIs after full ramdump support  HW_Reboot*/
			aee__flush_dcache_area(mrdump_cblock,
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
__MODULE_PARM_TYPE(lbaooo, "ulong");

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek MRDUMP module");
MODULE_AUTHOR("MediaTek Inc.");
