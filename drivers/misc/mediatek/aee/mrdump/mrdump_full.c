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
#include <mt-plat/aee.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <linux/kallsyms.h>
#include <linux/miscdevice.h>
#include <mt-plat/mtk_ram_console.h>
#include <linux/reboot.h>
#include <linux/stacktrace.h>
#include <linux/vmalloc.h>
#include <linux/elfcore.h>
#include <linux/kexec.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <mach/wd_api.h>
#if defined(CONFIG_FIQ_GLUE)
#include <mt-plat/fiq_smp_call.h>
#endif
#include <smp.h>
#include <mrdump.h>
#include <linux/kdebug.h>
#include "mrdump_private.h"

#define KEXEC_NOTE_HEAD_BYTES ALIGN(sizeof(struct elf_note), 4)
#define KEXEC_CORE_NOTE_NAME "CORE"
#define KEXEC_CORE_NOTE_NAME_BYTES ALIGN(sizeof(KEXEC_CORE_NOTE_NAME), 4)
#define KEXEC_CORE_NOTE_DESC_BYTES ALIGN(sizeof(struct elf_prstatus), 4)
#define KEXEC_NOTE_BYTES ((KEXEC_NOTE_HEAD_BYTES * 2) +		\
			  KEXEC_CORE_NOTE_NAME_BYTES +		\
			  KEXEC_CORE_NOTE_DESC_BYTES)
typedef u32 note_buf_t[KEXEC_NOTE_BYTES / 4];

static int crashing_cpu;

static note_buf_t __percpu *crash_notes;

static bool mrdump_enable = 1;
static int mrdump_output_device;
static int mrdump_output_fstype;
static unsigned long mrdump_output_lbaooo;

static struct mrdump_control_block mrdump_cblock __attribute__((section (".mrdump")));

static const struct mrdump_platform *mrdump_plat;

static char mrdump_lk[12];
static int mrdump_rsv_conflict;

static u32 *append_elf_note(u32 *buf, char *name, unsigned type, void *data,
			    size_t data_len)
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

static void final_note(u32 *buf)
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

	buf = (u32 *)per_cpu_ptr(crash_notes, cpu);
	if (!buf)
		return;
	memset(&prstatus, 0, sizeof(prstatus));
	prstatus.pr_pid = current->pid;
	elf_core_copy_kernel_regs((elf_gregset_t *)&prstatus.pr_reg, regs);
	buf = append_elf_note(buf, KEXEC_CORE_NOTE_NAME, NT_PRSTATUS,
			      &prstatus, sizeof(prstatus));
	final_note(buf);
}

static void save_current_task(void)
{
	int i;
	struct stack_trace trace;
	unsigned long stack_entries[16];
	struct task_struct *tsk;
	struct mrdump_crash_record *crash_record = &mrdump_cblock.crash_record;

	tsk = current_thread_info()->task;

	/* Grab kernel task stack trace */
	trace.nr_entries = 0;
	trace.max_entries = sizeof(stack_entries) / sizeof(stack_entries[0]);
	trace.entries = stack_entries;
	trace.skip = 1;
	save_stack_trace_tsk(tsk, &trace);

	for (i = 0; i < trace.nr_entries; i++) {
		int off = strlen(crash_record->backtrace);
		int plen = sizeof(crash_record->backtrace) - off;
		if (plen > 16) {
			snprintf(crash_record->backtrace + off, plen, "[<%p>] %pS\n",
				 (void *)stack_entries[i], (void *)stack_entries[i]);
		}
	}
}

#if defined(CONFIG_FIQ_GLUE)

static void aee_kdump_cpu_stop(void *arg, void *regs, void *svc_sp)
{
	struct mrdump_crash_record *crash_record = &mrdump_cblock.crash_record;
	int cpu = 0;
	register int sp asm("sp");
	struct pt_regs *ptregs = (struct pt_regs *)regs;

	asm volatile("mov %0, %1\n\t"
		     "mov fp, %2\n\t"
		     : "=r" (sp)
		     : "r" (svc_sp), "r" (ptregs->ARM_fp)
		);
	cpu = get_HW_cpuid();

	elf_core_copy_kernel_regs((elf_gregset_t *)&crash_record->cpu_regs[cpu], ptregs);
	crash_save_cpu((struct pt_regs *)regs, cpu);
	local_fiq_disable();
	local_irq_disable();

	__disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2();
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
	struct mrdump_crash_record *crash_record = &mrdump_cblock.crash_record;
	struct pt_regs regs;
	int cpu = get_HW_cpuid();

	mrdump_save_current_backtrace(&regs);

	elf_core_copy_kernel_regs((elf_gregset_t *)&crash_record->cpu_regs[cpu], &regs);
	crash_save_cpu((struct pt_regs *)&regs, cpu);

	local_fiq_disable();
	local_irq_disable();

	__disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2();
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
	if (atomic_read(&waiting_for_crash_ipi) > 0)
		pr_warn("Non-crashing CPUs did not react to IPI\n");
}

#endif


static void __mrdump_reboot_va(AEE_REBOOT_MODE reboot_mode, struct pt_regs *regs, const char *msg, va_list ap)
{
	struct mrdump_crash_record *crash_record;
	int cpu;

	if (mrdump_enable != 1)
		pr_info("MT-RAMDUMP no enable");

	crash_record = &mrdump_cblock.crash_record;

	local_irq_disable();
	local_fiq_disable();

#if defined(CONFIG_SMP)
	__mrdump_reboot_stop_all(crash_record);
#endif

	cpu = get_HW_cpuid();
	crashing_cpu = cpu;
	crash_save_cpu(regs, cpu);

	elf_core_copy_kernel_regs((elf_gregset_t *)&crash_record->cpu_regs[cpu], regs);

	vsnprintf(crash_record->msg, sizeof(crash_record->msg), msg, ap);
	crash_record->fault_cpu = cpu;
	save_current_task();

	/* FIXME: Check reboot_mode is valid */
	crash_record->reboot_mode = reboot_mode;
	__disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2();

	if (reboot_mode == AEE_REBOOT_MODE_NESTED_EXCEPTION) {
		while (1)
			cpu_relax();
	}

	mrdump_plat->reboot();
}

void aee_kdump_reboot(AEE_REBOOT_MODE reboot_mode, const char *msg, ...)
{
	va_list ap;
	struct pt_regs regs;

	mrdump_save_current_backtrace(&regs);

	va_start(ap, msg);
	__mrdump_reboot_va(reboot_mode, &regs, msg, ap);
	/* No return anymore */
	va_end(ap);
}

void __mrdump_create_oops_dump(AEE_REBOOT_MODE reboot_mode, struct pt_regs *regs, const char *msg, ...)
{
	va_list ap;
	struct mrdump_crash_record *crash_record;
	int cpu;

	crash_record = &mrdump_cblock.crash_record;

	local_irq_disable();
	local_fiq_disable();

#if defined(CONFIG_SMP)
	__mrdump_reboot_stop_all(crash_record);
#endif

	cpu = get_HW_cpuid();
	crashing_cpu = cpu;
	crash_save_cpu(regs, cpu);

	elf_core_copy_kernel_regs((elf_gregset_t *)&crash_record->cpu_regs[cpu], regs);

	va_start(ap, msg);
	vsnprintf(crash_record->msg, sizeof(crash_record->msg), msg, ap);
	va_end(ap);

	crash_record->fault_cpu = cpu;
	save_current_task();

	/* FIXME: Check reboot_mode is valid */
	crash_record->reboot_mode = reboot_mode;
}

int __init mrdump_platform_init(const struct mrdump_platform *plat)
{
	struct mrdump_machdesc *machdesc_p;

	memset(&mrdump_cblock, 0, sizeof(struct mrdump_control_block));

	mrdump_plat = plat;
	if (mrdump_plat == NULL) {
		mrdump_enable = 0;
		pr_err("%s: MT-RAMDUMP platform no init\n", __func__);
		return -EINVAL;
	}

	if (strcmp(mrdump_lk, MRDUMP_GO_DUMP) != 0) {
		mrdump_enable = 0;
		pr_err("%s: MT-RAMDUMP init failed, lk version %s not matched.\n", __func__, mrdump_lk);
		return -EINVAL;
	}

	memcpy(&mrdump_cblock.sig, MRDUMP_GO_DUMP, 8);

	/* move default enable MT-RAMDUMP to late_init (this function) */
	if (mrdump_enable) {
		mrdump_plat->hw_enable(mrdump_enable);
		__inner_flush_dcache_all();
	}

	machdesc_p = &mrdump_cblock.machdesc;
	machdesc_p->output_device = MRDUMP_DEV_EMMC;
	machdesc_p->output_fstype = MRDUMP_FS_EXT4;
	machdesc_p->nr_cpus = mrdump_enable ? NR_CPUS : 0;
	machdesc_p->page_offset = (uint64_t)PAGE_OFFSET;
	machdesc_p->high_memory = (uintptr_t)high_memory;

	machdesc_p->vmalloc_start = (uint64_t)VMALLOC_START;
	machdesc_p->vmalloc_end = (uint64_t)VMALLOC_END;

	machdesc_p->modules_start = (uint64_t)MODULES_VADDR;
	machdesc_p->modules_end = (uint64_t)MODULES_END;

	machdesc_p->phys_offset = (uint64_t)PHYS_OFFSET;
	machdesc_p->master_page_table = (uintptr_t)&swapper_pg_dir;

	/* Allocate memory for saving cpu registers. */
	crash_notes = alloc_percpu(note_buf_t);
	if (!crash_notes) {
		pr_err("MT-RAMDUMP: Memory allocation for saving cpu register failed\n");
		return -ENOMEM;
	}

	pr_info("%s: init_done.\n", __func__);
	return 0;
}

#if CONFIG_SYSFS

static ssize_t dump_status_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *page)
{
	return 0;
}

static ssize_t mrdump_version_show(struct kobject *kobj, struct kobj_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", MRDUMP_GO_DUMP);
}

static ssize_t manual_dump_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", "Trigger manual dump with message, format \"manualdump:HelloWorld\"");
}

static ssize_t manual_dump_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	if (strncmp(buf, "manualdump:", 11) == 0)
		aee_kdump_reboot(AEE_REBOOT_MODE_MANUAL_KDUMP, buf + 11);
	return count;
}

static struct kobj_attribute dump_status_attribute =
	__ATTR(dump_status, 0400, dump_status_show, NULL);

static struct kobj_attribute mrdump_version_attribute =
	__ATTR(version, 0600, mrdump_version_show, NULL);

static struct kobj_attribute manual_dump_attribute =
	__ATTR(manualdump, 0600, manual_dump_show, manual_dump_store);

static struct attribute *attrs[] = {
	&dump_status_attribute.attr,
	&mrdump_version_attribute.attr,
	&manual_dump_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static int __init mrdump_sysfs_init(void)
{
	struct kobject *kobj;

	kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (kobj) {
		if (sysfs_create_group(kobj, &attr_group)) {
			pr_err("MT-RAMDUMP: sysfs create sysfs failed\n");
			return -ENOMEM;
		}
	} else {
		pr_err("MT-RAMDUMP: Cannot find module %s object\n", KBUILD_MODNAME);
		return -EINVAL;
	}

	pr_info("%s: init_done.\n", __func__);
	return 0;
}

module_init(mrdump_sysfs_init);

#endif

static int param_set_mrdump_device(const char *val, const struct kernel_param *kp)
{
	char strval[16], *strp;
	int eval;

	strlcpy(strval, val, sizeof(strval));
	strp = strstrip(strval);

	if (strcmp(strp, "null") == 0)
		eval = MRDUMP_DEV_NULL;
	else if (strcmp(strp, "sdcard") == 0)
		eval = MRDUMP_DEV_SDCARD;
	else if (strcmp(strp, "emmc") == 0)
		eval = MRDUMP_DEV_EMMC;
	else
		eval = MRDUMP_DEV_NULL;
	*(int *)kp->arg = eval;
	mrdump_cblock.machdesc.output_device = eval;
	__inner_flush_dcache_all();
	return 0;
}

static int param_get_mrdump_device(char *buffer, const struct kernel_param *kp)
{
	char *dev;
	switch (mrdump_cblock.machdesc.output_device) {
	case MRDUMP_DEV_NULL:
		dev = "null";
		break;
	case MRDUMP_DEV_SDCARD:
		dev = "sdcard";
		break;
	case MRDUMP_DEV_EMMC:
		dev = "emmc";
		break;
	default:
		dev = "none(unknown)";
		break;
	}

	strlcpy(buffer, dev, PAGE_SIZE);
	return strlen(dev);
}

static int param_set_mrdump_enable(const char *val, const struct kernel_param *kp)
{
	int res, retval = 0;
	struct wd_api *wd_api = NULL;

	res = get_wd_api(&wd_api);
	if (res < 0) {
		pr_alert("wd_ddr_reserved_mode, get wd api error %d\n", res);
		return res;
	}

	/* Always disable if version not matched...cannot enable manually. */
	if ((mrdump_plat != NULL) && (0 == memcmp(mrdump_cblock.sig, MRDUMP_GO_DUMP, 8)) && !mrdump_rsv_conflict) {
		retval = param_set_bool(val, kp);
		if (retval == 0) {
			mrdump_plat->hw_enable(mrdump_enable);
			mrdump_cblock.machdesc.nr_cpus = mrdump_enable ? NR_CPUS : 0;
			__inner_flush_dcache_all();
		}
	}
	return retval;
}

static int param_set_mrdump_fstype(const char *val, const struct kernel_param *kp)
{
	char strval[16], *strp;
	int eval;

	strlcpy(strval, val, sizeof(strval));
	strp = strstrip(strval);

	if (strcmp(strp, "null") == 0)
		eval = MRDUMP_FS_NULL;
	else if (strcmp(strp, "vfat") == 0)
		eval = MRDUMP_FS_VFAT;
	else if (strcmp(strp, "ext4") == 0)
		eval = MRDUMP_FS_EXT4;
	else
		eval = MRDUMP_FS_NULL;

	*(int *)kp->arg = eval;
	mrdump_cblock.machdesc.output_fstype = eval;
	__inner_flush_dcache_all();
	return 0;
}

static int param_get_mrdump_fstype(char *buffer, const struct kernel_param *kp)
{
	char *dev;
	switch (mrdump_cblock.machdesc.output_fstype) {
	case MRDUMP_FS_NULL:
		dev = "null";
		break;
	case MRDUMP_FS_VFAT:
		dev = "vfat";
		break;
	case MRDUMP_FS_EXT4:
		dev = "ext4";
		break;
	default:
		dev = "none(unknown)";
		break;
	}
	strlcpy(buffer, dev, PAGE_SIZE);
	return strlen(dev);
}

static int param_set_mrdump_lbaooo(const char *val, const struct kernel_param *kp)
{
	int retval = param_set_ulong(val, kp);
	if ((retval == 0) && (mrdump_cblock.machdesc.output_fstype == MRDUMP_FS_EXT4)) {
		mrdump_cblock.machdesc.output_lbaooo = mrdump_output_lbaooo;
		__inner_flush_dcache_all();
	}
	return retval;
}


module_param_string(lk, mrdump_lk, sizeof(mrdump_lk), S_IRUGO);

/* sys/modules/mrdump/parameter/lbaooo */
struct kernel_param_ops param_ops_mrdump_lbaooo = {
	.set = param_set_mrdump_lbaooo,
	.get = param_get_ulong,
};

param_check_ulong(lbaooo, &mrdump_output_lbaooo);
module_param_cb(lbaooo, &param_ops_mrdump_lbaooo, &mrdump_output_lbaooo, S_IRUGO | S_IWUSR);
__MODULE_PARM_TYPE(lbaooo, unsigned long);

/* sys/modules/mrdump/parameter/fstype */
struct kernel_param_ops param_ops_mrdump_fstype = {
	.set = param_set_mrdump_fstype,
	.get = param_get_mrdump_fstype,
};

param_check_int(fstype, &mrdump_output_fstype);
module_param_cb(fstype, &param_ops_mrdump_fstype, &mrdump_output_fstype, S_IRUGO | S_IWUSR);
__MODULE_PARM_TYPE(fstype, int);

/* sys/modules/mrdump/parameter/enable */
struct kernel_param_ops param_ops_mrdump_enable = {
	.set = param_set_mrdump_enable,
	.get = param_get_bool,
};
param_check_bool(enable, &mrdump_enable);
module_param_cb(enable, &param_ops_mrdump_enable, &mrdump_enable, S_IRUGO | S_IWUSR);
__MODULE_PARM_TYPE(enable, bool);

/* sys/modules/mrdump/parameter/device */
struct kernel_param_ops param_ops_mrdump_device = {
	.set = param_set_mrdump_device,
	.get = param_get_mrdump_device,
};

param_check_int(device, &mrdump_output_device);
module_param_cb(device, &param_ops_mrdump_device, &mrdump_output_device, S_IRUGO | S_IWUSR);
__MODULE_PARM_TYPE(device, int);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek MRDUMP module");
MODULE_AUTHOR("MediaTek Inc.");

