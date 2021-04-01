// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <stdarg.h>
#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <linux/kallsyms.h>
#include <linux/kdebug.h>
#include <linux/kernel.h>
#include <linux/kexec.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/processor.h>
#include <linux/reboot.h>
#include <linux/stacktrace.h>
#include <linux/vmalloc.h>
#include <asm/kexec.h>
#include <asm/pgtable.h>

#if IS_ENABLED(CONFIG_MTK_WATCHDOG)
#include <ext_wd_drv.h>
#endif
#include <mt-plat/aee.h>
#if IS_ENABLED(CONFIG_FIQ_GLUE)
#include <mt-plat/fiq_smp_call.h>
#endif
#include <mt-plat/mboot_params.h>
#include <mrdump.h>
#include "mrdump_private.h"

static int crashing_cpu;

#ifndef CONFIG_KEXEC_CORE
/* note_buf_t defined in linux/crash_core.h included by linux/kexec.h */
static note_buf_t __percpu *crash_notes;
#endif
static unsigned long mrdump_output_lbaooo;

#ifndef CONFIG_KEXEC_CORE
static u32 *mrdump_append_elf_note(u32 *buf, char *name, unsigned int type,
				void *data, size_t data_len)
{
	struct elf_note note;

	note.n_namesz = strlen(name) + 1;
	note.n_descsz = data_len;
	note.n_type = type;
	memcpy(buf, &note, sizeof(note));
	buf += (sizeof(note) + 3) / 4;
	memcpy(buf, name, note.n_namesz);
	buf += (note.n_namesz + 3) / 4;
	memcpy(buf, data, note.n_descsz);
	buf += (note.n_descsz + 3) / 4;

	return buf;
}

static void mrdump_final_note(u32 *buf)
{
	struct elf_note note;

	note.n_namesz = 0;
	note.n_descsz = 0;
	note.n_type = 0;
	memcpy(buf, &note, sizeof(note));
}

static void crash_save_cpu(struct pt_regs *regs, int cpu)
{
	struct elf_prstatus prstatus;
	u32 *buf;

	if ((cpu < 0) || (cpu >= nr_cpu_ids))
		return;
	if (!crash_notes)
		return;
	buf = (u32 *)per_cpu_ptr(crash_notes, cpu);
	if (!buf)
		return;
	memset(&prstatus, 0, sizeof(prstatus));
	prstatus.pr_pid = current->pid;
	elf_core_copy_kernel_regs((elf_gregset_t *)&prstatus.pr_reg, regs);
	buf = mrdump_append_elf_note(buf, CRASH_CORE_NOTE_NAME, NT_PRSTATUS,
			      &prstatus, sizeof(prstatus));
	mrdump_final_note(buf);
}
#endif

#if defined(CONFIG_FIQ_GLUE)

static void aee_kdump_cpu_stop(void *arg, void *regs, void *svc_sp)
{
	struct mrdump_crash_record *crash_record = &mrdump_cblock->crash_record;
	int cpu = 0;

	register int sp asm("sp");
	struct pt_regs *ptregs = (struct pt_regs *)regs;
	void *creg;
	elf_gregset_t *reg;

	asm volatile("mov %0, %1\n\t"
		     "mov fp, %2\n\t"
		     : "=r" (sp)
		     : "r" (svc_sp), "r" (ptregs->ARM_fp)
		);
	cpu = get_HW_cpuid();

	switch (sizeof(unsigned long)) {
	case 4:
		reg = (elf_gregset_t *)&crash_record->cpu_reg[cpu].arm32_reg.arm32_regs;
		creg = (void *)&crash_record->cpu_reg[cpu].arm32_reg.arm32_creg;
		break;
	case 8:
		reg = (elf_gregset_t *)&crash_record->cpu_regs[cpu].arm64_reg.arm64_regs;
		creg = (void *)&crash_record->cpu_reg[cpu].arm64_reg.arm64_creg;
		break;
	default:
		BUILD_BUG();
	}

	if (cpu >= 0) {
		crash_save_cpu((struct pt_regs *)regs, cpu);
		elf_core_copy_kernel_regs(reg, ptregs);
		mrdump_save_control_register(creg);
	}

	local_irq_disable();

/* TODO: remove flush APIs after full ramdump support  HW_Reboot*/
#if IS_ENABLED(CONFIG_MEDIATEK_CACHE_API)
	dis_D_inner_flush_all();
#else
	pr_info("dis_D_inner_flush_all invalid");
#endif

	while (1)
		cpu_relax();
}

static void __mrdump_reboot_stop_all(struct mrdump_crash_record *crash_record)
{
	int timeout;

	fiq_smp_call_function(aee_kdump_cpu_stop, NULL, 0);

	/* Wait up to two second for other CPUs to stop */
	timeout = 2 * USEC_PER_SEC;
	while (num_online_cpus() > 1 && timeout--)
		udelay(1);
}

#else

/* Generic IPI support */
static atomic_t waiting_for_crash_ipi;

static void mrdump_stop_noncore_cpu(void *unused)
{
	struct mrdump_crash_record *crash_record = &mrdump_cblock->crash_record;
	struct pt_regs regs;
	void *creg;
	int cpu = get_HW_cpuid();

	atomic_dec(&waiting_for_crash_ipi);
	if (cpu >= 0) {
		crash_setup_regs(&regs, NULL);
		crash_save_cpu((struct pt_regs *)&regs, cpu);
		switch (sizeof(unsigned long)) {
		case 4:
			elf_core_copy_kernel_regs(
				(elf_gregset_t *)&crash_record->cpu_reg[cpu].arm32_reg.arm32_regs,
				&regs);
			creg = (void *)&crash_record->cpu_reg[cpu].arm32_reg.arm32_creg;
			break;
		case 8:
			elf_core_copy_kernel_regs(
				(elf_gregset_t *)&crash_record->cpu_reg[cpu].arm64_reg.arm64_regs,
				&regs);
			creg = (void *)&crash_record->cpu_reg[cpu].arm64_reg.arm64_creg;
			break;
		default:
			BUILD_BUG();
		}
		mrdump_save_control_register(creg);
	}

	local_irq_disable();

/* TODO: remove flush APIs after full ramdump support  HW_Reboot*/
#if IS_ENABLED(CONFIG_MEDIATEK_CACHE_API)
	dis_D_inner_flush_all();
#else
	pr_info("dis_D_inner_flush_all invalid");
#endif

	while (1)
		cpu_relax();
}

static void __mrdump_reboot_stop_all(struct mrdump_crash_record *crash_record)
{
	unsigned long msecs;

	atomic_set(&waiting_for_crash_ipi, num_online_cpus() - 1);
	smp_call_function(mrdump_stop_noncore_cpu, NULL, false);

	msecs = 1000; /* Wait at most a second for the other cpus to stop */
	while ((atomic_read(&waiting_for_crash_ipi) > 0) && msecs) {
		mdelay(1);
		msecs--;
	}
	if (atomic_read(&waiting_for_crash_ipi) > 0) {
		pr_notice("Non-crashing %d CPUs did not react to IPI\n",
				atomic_read(&waiting_for_crash_ipi));
	}
}

#endif

void mrdump_save_ctrlreg(int cpu)
{
	struct mrdump_crash_record *crash_record;
	void *creg;

	if (mrdump_cblock && cpu >= 0) {
		crash_record = &mrdump_cblock->crash_record;
		switch (sizeof(unsigned long)) {
		case 4:
			creg = (void *)&crash_record->cpu_reg[cpu].arm32_reg.arm32_creg;
			break;
		case 8:
			creg = (void *)&crash_record->cpu_reg[cpu].arm64_reg.arm64_creg;
			break;
		default:
			BUILD_BUG();
		}
		mrdump_save_control_register(creg);
	}
}

void mrdump_save_per_cpu_reg(int cpu, struct pt_regs *regs)
{
	struct mrdump_crash_record *crash_record;
	elf_gregset_t *reg;

	if (mrdump_cblock)
		crash_record = &mrdump_cblock->crash_record;

	switch (sizeof(unsigned long)) {
	case 4:
		reg = (elf_gregset_t *)&crash_record->cpu_reg[cpu].arm32_reg.arm32_regs;
		break;
	case 8:
		reg = (elf_gregset_t *)&crash_record->cpu_reg[cpu].arm64_reg.arm64_regs;
		break;
	default:
		BUILD_BUG();
	}

	if (regs) {
		crash_save_cpu(regs, cpu);

		if (reg)
			elf_core_copy_kernel_regs(reg, regs);
	}
}

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

#if defined(CONFIG_SMP)
		if ((reboot_mode != AEE_REBOOT_MODE_WDT) &&
		    (reboot_mode != AEE_REBOOT_MODE_GZ_WDT))
			__mrdump_reboot_stop_all(crash_record);
#endif

		cpu = get_HW_cpuid();

		switch (sizeof(unsigned long)) {
		case 4:
			reg = (elf_gregset_t *)&crash_record->cpu_reg[cpu].arm32_reg.arm32_regs;
			creg = (void *)&crash_record->cpu_reg[cpu].arm32_reg.arm32_creg;
			break;
		case 8:
			reg = (elf_gregset_t *)&crash_record->cpu_reg[cpu].arm64_reg.arm64_regs;
			creg = (void *)&crash_record->cpu_reg[cpu].arm32_reg.arm32_creg;
			break;
		default:
			BUILD_BUG();
		}

		if (cpu >= 0 && cpu < AEE_MTK_CPU_NUMS) {
			crashing_cpu = cpu;
			/* null regs, no register dump */
			if (regs) {
				crash_save_cpu(regs, cpu);
				elf_core_copy_kernel_regs(reg, regs);
			}
			if (creg)
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
static ssize_t mrdump_version_show(struct module_attribute *attr,
		struct module_kobject *kobj, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s", MRDUMP_GO_DUMP);
}

static struct module_attribute mrdump_version_attribute =
	__ATTR(version, 0400, mrdump_version_show, NULL);

static struct attribute *attrs[] = {
	&mrdump_version_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static int __init mrdump_sysfs_init(void)
{
	struct kobject *kobj;
	struct kset *p_module_kset = aee_get_module_kset();

	if (!p_module_kset) {
		pr_notice("MT-RAMDUMP: Cannot find module_kset");
		return -EINVAL;
	}

	kobj = kset_find_obj(p_module_kset, KBUILD_MODNAME);
	if (kobj) {
		if (sysfs_create_group(kobj, &attr_group)) {
			pr_notice("MT-RAMDUMP: sysfs create sysfs failed\n");
			return -ENOMEM;
		}
	} else {
		pr_notice("MT-RAMDUMP: Cannot find module %s object\n",
				KBUILD_MODNAME);
		return -EINVAL;
	}

	pr_info("%s: done.\n", __func__);
	return 0;
}
#ifndef MODULE
module_init(mrdump_sysfs_init);
#endif
#endif

int __init mrdump_full_init(void)
{
	/* Allocate memory for saving cpu registers. */
	crash_notes = alloc_percpu(note_buf_t);
	if (!crash_notes) {
		pr_notice("MT-RAMDUMP: alloc mem fail for cpu registers\n");
		return -ENOMEM;
	}

	mrdump_cblock->enabled = MRDUMP_ENABLE_COOKIE;
	/* TODO: remove flush APIs after full ramdump support  HW_Reboot*/
	aee__flush_dcache_area(mrdump_cblock,
			sizeof(struct mrdump_control_block));
	pr_info("%s: MT-RAMDUMP enabled done\n", __func__);
#if defined(MODULE)
#if IS_ENABLED(CONFIG_SYSFS)
	mrdump_sysfs_init();
#endif
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
