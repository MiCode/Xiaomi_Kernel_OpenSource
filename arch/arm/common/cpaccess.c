/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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

/*
 * CP parameters
 */
struct cp_params {
	unsigned long cp;
	unsigned long op1;
	unsigned long op2;
	unsigned long crn;
	unsigned long crm;
	unsigned long write_value;
	char rw;
};

static struct semaphore cp_sem;
static int cpu;

static DEFINE_PER_CPU(struct cp_params, cp_param)
	 = { 15, 0, 0, 0, 0, 0, 'r' };

static struct sysdev_class cpaccess_sysclass = {
	.name = "cpaccess",
};

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
	asm("mrc p15, 0, r0, c0, c0, 0\n\t");
	asm("bx	lr\n\t");
	return 0xBEEF;
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
	 * Grab address of the Dummy function, insert MRC/MCR
	 * instruction and a return instruction ("bx lr"). Do
	 * a D cache clean and I cache invalidate after inserting
	 * new code.
	 */
	p_opcode = (unsigned long *)&cpaccess_dummy;
	*p_opcode++ = opcode;
	*p_opcode-- = 0xE12FFF1E;
	__cpuc_coherent_kern_range((unsigned long)p_opcode,
	 ((unsigned long)p_opcode + (sizeof(long) * 2)));

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

/*
 * cp_register_write_sysfs - sysfs interface for writing to
 * CP register
 * @dev:	sys device
 * @attr:	device attribute
 * @buf:	write value
 * @cnt:	not used
 *
 */
static ssize_t cp_register_write_sysfs(struct sys_device *dev,
 struct sysdev_attribute *attr, const char *buf, size_t cnt)
{
	unsigned long op1, op2, crn, crm, cp = 15, write_value, ret;
	char rw;
	if (down_timeout(&cp_sem, 6000))
		return -ERESTARTSYS;

	sscanf(buf, "%lu:%lu:%lu:%lu:%lu:%c:%lx:%d", &cp, &op1, &crn,
	 &crm, &op2, &rw, &write_value, &cpu);
	per_cpu(cp_param.cp, cpu) = cp;
	per_cpu(cp_param.op1, cpu) = op1;
	per_cpu(cp_param.crn, cpu) = crn;
	per_cpu(cp_param.crm, cpu) = crm;
	per_cpu(cp_param.op2, cpu) = op2;
	per_cpu(cp_param.rw, cpu) = rw;
	per_cpu(cp_param.write_value, cpu) = write_value;

	if (per_cpu(cp_param.rw, cpu) == 'w') {
		do_cpregister_rw(1);
		ret = cnt;
	}

	if ((per_cpu(cp_param.rw, cpu) != 'w') &&
	(per_cpu(cp_param.rw, cpu) != 'r')) {
		ret = -1;
		printk(KERN_INFO "Wrong Entry for 'r' or 'w'. \
			Use cp:op1:crn:crm:op2:r/w:write_value.\n");
	}

	return cnt;
}

/*
 * cp_register_read_sysfs - sysfs interface for reading CP registers
 * @dev:        sys device
 * @attr:       device attribute
 * @buf:        write value
 *
 * Code to read in the CPxx crn, crm, op1, op2 variables, or into
 * the base MRC opcode, store to executable memory, clean/invalidate
 * caches and then execute the new instruction and provide the
 * result to the caller.
 */
static ssize_t cp_register_read_sysfs(struct sys_device *dev,
 struct sysdev_attribute *attr, char *buf)
{
	int ret;
	ret = sprintf(buf, "%lx\n", do_cpregister_rw(0));

	if (cp_sem.count <= 0)
		up(&cp_sem);

	return ret;
}

/*
 * Setup sysfs files
 */
SYSDEV_ATTR(cp_rw, 0644, cp_register_read_sysfs,
 cp_register_write_sysfs);

static struct sys_device device_cpaccess = {
	.id     = 0,
	.cls    = &cpaccess_sysclass,
};

/*
 * init_cpaccess_sysfs - initialize sys devices
 */
static int __init init_cpaccess_sysfs(void)
{
	int error = sysdev_class_register(&cpaccess_sysclass);

	if (!error)
		error = sysdev_register(&device_cpaccess);
	else
		printk(KERN_ERR "Error initializing cpaccess \
		interface\n");

	if (!error)
		error = sysdev_create_file(&device_cpaccess,
		 &attr_cp_rw);
	else {
		printk(KERN_ERR "Error initializing cpaccess \
		interface\n");
		sysdev_unregister(&device_cpaccess);
		sysdev_class_unregister(&cpaccess_sysclass);
	}

	sema_init(&cp_sem, 1);

	return error;
}

static void __exit exit_cpaccess_sysfs(void)
{
	sysdev_remove_file(&device_cpaccess, &attr_cp_rw);
	sysdev_unregister(&device_cpaccess);
	sysdev_class_unregister(&cpaccess_sysclass);
}

module_init(init_cpaccess_sysfs);
module_exit(exit_cpaccess_sysfs);
MODULE_LICENSE("GPL v2");
