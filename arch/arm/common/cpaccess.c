/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/sysrq.h>
#include <linux/time.h>
#include <linux/proc_fs.h>
#include <linux/kernel_stat.h>
#include <linux/uaccess.h>
#include <linux/sysdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/file.h>
#include <linux/percpu.h>
#include <linux/string.h>
#include <linux/smp.h>
#include <asm/cacheflush.h>
#include <asm/smp_plat.h>
#include <asm/mmu_writeable.h>

#ifdef CONFIG_ARCH_MSM_KRAIT
#include <mach/msm-krait-l2-accessors.h>
#endif

#define TYPE_MAX_CHARACTERS 10

/*
 * CP parameters
 */
struct cp_params {
	unsigned long il2index;
	unsigned long cp;
	unsigned long op1;
	unsigned long op2;
	unsigned long crn;
	unsigned long crm;
	unsigned long write_value;
	char rw;
};

static struct semaphore cp_sem;
static unsigned long il2_output;
static int cpu;
char type[TYPE_MAX_CHARACTERS] = "C";

static DEFINE_PER_CPU(struct cp_params, cp_param)
	 = { 0, 15, 0, 0, 0, 0, 0, 'r' };

static struct sysdev_class cpaccess_sysclass = {
	.name = "cpaccess",
};

void cpaccess_dummy_inst(void);

#ifdef CONFIG_ARCH_MSM_KRAIT
/*
 * do_read_il2 - Read indirect L2 registers
 * @ret:	Pointer	to return value
 *
 */
static void do_read_il2(void *ret)
{
	*(unsigned long *)ret =
		get_l2_indirect_reg(per_cpu(cp_param.il2index, cpu));
}

/*
 * do_write_il2 - Write indirect L2 registers
 * @ret:	Pointer	to return value
 *
 */
static void do_write_il2(void *ret)
{
	*(unsigned long *)ret =
		set_get_l2_indirect_reg(per_cpu(cp_param.il2index, cpu),
				per_cpu(cp_param.write_value, cpu));
}

/*
 * do_il2_rw - Call Read/Write indirect L2 register functions
 * @ret:	Pointer	to return value in case of CP register
 *
 */
static int do_il2_rw(char *str_tmp)
{
	unsigned long write_value, il2index;
	char rw;
	int ret = 0;

	il2index = 0;
	sscanf(str_tmp, "%lx:%c:%lx:%d", &il2index, &rw, &write_value,
								&cpu);
	per_cpu(cp_param.il2index, cpu) = il2index;
	per_cpu(cp_param.rw, cpu) = rw;
	per_cpu(cp_param.write_value, cpu) = write_value;

	if (per_cpu(cp_param.rw, cpu) == 'r') {
		if (is_smp()) {
			if (smp_call_function_single(cpu, do_read_il2,
							&il2_output, 1))
				pr_err("Error cpaccess smp call single\n");
		} else
			do_read_il2(&il2_output);
	} else if (per_cpu(cp_param.rw, cpu) == 'w') {
		if (is_smp()) {
			if (smp_call_function_single(cpu, do_write_il2,
							&il2_output, 1))
				pr_err("Error cpaccess smp call single\n");
		} else
			do_write_il2(&il2_output);
	} else {
			pr_err("cpaccess: Wrong Entry for 'r' or 'w'.\n");
			return -EINVAL;
	}
	return ret;
}
#else
static void do_il2_rw(char *str_tmp)
{
	il2_output = 0;
}
#endif

/*
 * get_asm_value - Dummy fuction
 * @write_val:	Write value incase of a CP register write operation.
 *
 * This function is just a placeholder. The first 2 instructions
 * will be inserted to perform MRC/MCR instruction and a return.
 * See do_cpregister_rw function. Value passed to function is
 * accessed from r0 register.
 */
static noinline unsigned long cpaccess_dummy(unsigned long write_val)
{
	unsigned long ret = 0xBEEF;

	asm volatile (".globl cpaccess_dummy_inst\n"
			"cpaccess_dummy_inst:\n\t"
			"mrc p15, 0, %0, c0, c0, 0\n\t" : "=r" (ret) :
				"r" (write_val));
	return ret;
} __attribute__((aligned(32)))

/*
 * get_asm_value - Read/Write CP registers
 * @ret:	Pointer	to return value in case of CP register
 * read op.
 *
 */
static void get_asm_value(void *ret)
{
	*(unsigned long *)ret =
	 cpaccess_dummy(per_cpu(cp_param.write_value, cpu));
}

/*
 * dp_cpregister_rw - Read/Write CP registers
 * @write:		1 for Write and 0 for Read operation
 *
 * Returns value read from CP register
 */
static unsigned long do_cpregister_rw(int write)
{
	unsigned long opcode, ret, *p_opcode;

	/*
	 * Mask the crn, crm, op1, op2 and cp values so they do not
	 * interfer with other fields of the op code.
	 */
	per_cpu(cp_param.cp, cpu)  &= 0xF;
	per_cpu(cp_param.crn, cpu) &= 0xF;
	per_cpu(cp_param.crm, cpu) &= 0xF;
	per_cpu(cp_param.op1, cpu) &= 0x7;
	per_cpu(cp_param.op2, cpu) &= 0x7;

	/*
	 * Base MRC opcode for MIDR is EE100010,
	 * MCR is 0xEE000010
	 */
	opcode = (write == 1 ? 0xEE000010 : 0xEE100010);
	opcode |= (per_cpu(cp_param.crn, cpu)<<16) |
	(per_cpu(cp_param.crm, cpu)<<0) |
	(per_cpu(cp_param.op1, cpu)<<21) |
	(per_cpu(cp_param.op2, cpu)<<5) |
	(per_cpu(cp_param.cp, cpu) << 8);

	/*
	 * Grab address of the Dummy function, write the MRC/MCR
	 * instruction, ensuring cache coherency.
	 */
	p_opcode = (unsigned long *)&cpaccess_dummy_inst;
	mem_text_write_kernel_word(p_opcode, opcode);

#ifdef CONFIG_SMP
	/*
	 * Use smp_call_function_single to do CPU core specific
	 * get_asm_value function call.
	 */
	if (smp_call_function_single(cpu, get_asm_value, &ret, 1))
		printk(KERN_ERR "Error cpaccess smp call single\n");
#else
		get_asm_value(&ret);
#endif

	return ret;
}

static int get_register_params(char *str_tmp)
{
	unsigned long op1, op2, crn, crm, cp = 15, write_value, il2index;
	char rw;
	int cnt = 0;

	il2index = 0;
	strncpy(type, strsep(&str_tmp, ":"), TYPE_MAX_CHARACTERS);

	if (strncasecmp(type, "C", TYPE_MAX_CHARACTERS) == 0) {

		sscanf(str_tmp, "%lu:%lu:%lu:%lu:%lu:%c:%lx:%d",
			&cp, &op1, &crn, &crm, &op2, &rw, &write_value, &cpu);
		per_cpu(cp_param.cp, cpu) = cp;
		per_cpu(cp_param.op1, cpu) = op1;
		per_cpu(cp_param.crn, cpu) = crn;
		per_cpu(cp_param.crm, cpu) = crm;
		per_cpu(cp_param.op2, cpu) = op2;
		per_cpu(cp_param.rw, cpu) = rw;
		per_cpu(cp_param.write_value, cpu) = write_value;

		if ((per_cpu(cp_param.rw, cpu) != 'w') &&
				(per_cpu(cp_param.rw, cpu) != 'r')) {
			pr_err("cpaccess: Wrong entry for 'r' or 'w'.\n");
			return -EINVAL;
		}

		if (per_cpu(cp_param.rw, cpu) == 'w')
			do_cpregister_rw(1);
	} else if (strncasecmp(type, "IL2", TYPE_MAX_CHARACTERS) == 0)
		do_il2_rw(str_tmp);
	else {
		pr_err("cpaccess: Not a valid type. Entered: %s\n", type);
		return -EINVAL;
	}

	return cnt;
}

/*
 * cp_register_write_sysfs - sysfs interface for writing to
 * CP register
 */
static ssize_t cp_register_write_sysfs(
	struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t cnt)
{
	char *str_tmp = (char *)buf;

	if (down_timeout(&cp_sem, 6000))
		return -ERESTARTSYS;

	get_register_params(str_tmp);

	return cnt;
}

/*
 * wrapper for deprecated sysdev write interface
 */
static ssize_t sysdev_cp_register_write_sysfs(struct sys_device *dev,
	struct sysdev_attribute *attr, const char *buf, size_t cnt)
{
	return cp_register_write_sysfs(NULL, NULL, buf, cnt);
}

/*
 * cp_register_read_sysfs - sysfs interface for reading CP registers
 *
 * Code to read in the CPxx crn, crm, op1, op2 variables, or into
 * the base MRC opcode, store to executable memory, clean/invalidate
 * caches and then execute the new instruction and provide the
 * result to the caller.
 */
static ssize_t cp_register_read_sysfs(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int ret;

	if (strncasecmp(type, "C", TYPE_MAX_CHARACTERS) == 0)
		ret = snprintf(buf, TYPE_MAX_CHARACTERS, "%lx\n",
					do_cpregister_rw(0));
	else if (strncasecmp(type, "IL2", TYPE_MAX_CHARACTERS) == 0)
		ret = snprintf(buf, TYPE_MAX_CHARACTERS, "%lx\n", il2_output);
	else
		ret = -EINVAL;

	if (cp_sem.count <= 0)
		up(&cp_sem);

	return ret;
}

/*
 * wrapper for deprecated sysdev read interface
 */
static ssize_t sysdev_cp_register_read_sysfs(struct sys_device *dev,
	struct sysdev_attribute *attr, char *buf)
{
	return cp_register_read_sysfs(NULL, NULL, buf);
}

/*
 * Setup sysfs files
 */
SYSDEV_ATTR(cp_rw, 0644, sysdev_cp_register_read_sysfs,
	    sysdev_cp_register_write_sysfs);

static struct sys_device device_cpaccess = {
	.id     = 0,
	.cls    = &cpaccess_sysclass,
};

static struct device cpaccess_dev = {
	.init_name = "cpaccess",
};

static struct kobj_attribute cp_rw_attribute =
	__ATTR(cp_rw, 0644, cp_register_read_sysfs, cp_register_write_sysfs);

static struct attribute *attrs[] = {
	&cp_rw_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.name = "cpaccess0",
	.attrs = attrs,
};

/*
 * init_cpaccess_sysfs - initialize sys devices
 */
static int __init init_cpaccess_sysfs(void)
{
	/*
	 * sysdev interface is deprecated and will be removed
	 * after migration to new sysfs entry
	 */

	int error = sysdev_class_register(&cpaccess_sysclass);

	if (!error)
		error = sysdev_register(&device_cpaccess);
	else
		pr_err("Error initializing cpaccess interface\n");

	if (!error)
		error = sysdev_create_file(&device_cpaccess,
		 &attr_cp_rw);
	else {
		pr_err("Error initializing cpaccess interface\n");
		goto exit0;
	}

	error = device_register(&cpaccess_dev);
	if (error) {
		pr_err("Error registering cpaccess device\n");
		goto exit0;
	}
	error = sysfs_create_group(&cpaccess_dev.kobj, &attr_group);
	if (error) {
		pr_err("Error creating cpaccess sysfs group\n");
		goto exit1;
	}

	sema_init(&cp_sem, 1);

	/*
	 * Make the target instruction writeable when built as a module
	 */
	set_memory_rw((unsigned long)&cpaccess_dummy_inst & PAGE_MASK, 1);

	return 0;

exit1:
	device_unregister(&cpaccess_dev);
exit0:
	sysdev_unregister(&device_cpaccess);
	sysdev_class_unregister(&cpaccess_sysclass);
	return error;
}

static void __exit exit_cpaccess_sysfs(void)
{
	sysdev_remove_file(&device_cpaccess, &attr_cp_rw);
	sysdev_unregister(&device_cpaccess);
	sysdev_class_unregister(&cpaccess_sysclass);

	sysfs_remove_group(&cpaccess_dev.kobj, &attr_group);
	device_unregister(&cpaccess_dev);
}

module_init(init_cpaccess_sysfs);
module_exit(exit_cpaccess_sysfs);
MODULE_LICENSE("GPL v2");
