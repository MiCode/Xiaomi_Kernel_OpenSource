/* Copyright (c) 2010-2015, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/file.h>
#include <linux/percpu.h>
#include <linux/string.h>
#include <linux/smp.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <asm/cacheflush.h>
#include <asm/smp_plat.h>
#include <asm/insn.h>
#include <soc/qcom/kryo-l2-accessors.h>

#define TYPE_MAX_CHARACTERS 20

/*
 * CP parameters
 */
struct cp_params {
	unsigned long il2index;
	unsigned long op0;
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
char type[TYPE_MAX_CHARACTERS] = "S";

static DEFINE_PER_CPU(struct cp_params, cp_param)
	 = { 0, 3, 0, 0, 0, 0, 0, 'r' };

static void get_asm_value(void *ret);
void cpaccess_dummy_inst(void);

/*
 * do_read_il2 - Read indirect L2 registers
 * @ret:Pointerto return value
 */
static void do_read_il2(void *ret)
{
	*(unsigned long *)ret =
		get_l2_indirect_reg(per_cpu(cp_param.il2index, cpu));
}

/*
 * do_write_il2 - Write indirect L2 registers
 * @ret:Pointerto return value
 */
static void do_write_il2(void *ret)
{
	set_l2_indirect_reg(per_cpu(cp_param.il2index, cpu),
		per_cpu(cp_param.write_value, cpu));
	*(unsigned long *)ret =
	get_l2_indirect_reg(per_cpu(cp_param.il2index, cpu));
}

/*
 * do_il2_rw - Call Read/Write indirect L2 register functions
 * @ret:Pointerto return value in case of CP register
 */
static int do_il2_rw(char *str_tmp)
{
	unsigned long write_value, il2index;
	char rw;
	int rc;

	il2index = 0;
	rc = sscanf(str_tmp, "%lx:%c:%lx:%d", &il2index, &rw, &write_value,
		&cpu);
	if (rc < 4) {
		pr_err("cpaccess: Invalid L2 syntax\n");
		il2_output = 0;
		return 0;
	}
	per_cpu(cp_param.il2index, cpu) = il2index;
	per_cpu(cp_param.rw, cpu) = rw;
	per_cpu(cp_param.write_value, cpu) = write_value;

	if (per_cpu(cp_param.rw, cpu) == 'r') {
		if (smp_call_function_single(cpu, do_read_il2,
			&il2_output, 1))
			pr_err("Error cpaccess smp call single\n");
	} else if (per_cpu(cp_param.rw, cpu) == 'w') {
		if (smp_call_function_single(cpu, do_write_il2,
			&il2_output, 1))
			pr_err("Error cpaccess smp call single\n");
	} else {
		pr_err("cpaccess: Wrong Entry for 'r' or 'w'.\n");
		return -EINVAL;
	}
	return 0;
}

/*
 * get_asm_value - Dummy function
 * @write_val:	Write value incase of a CP register write operation.
 *
 * This function is just a placeholder. The first msr instruction
 * will be replaced to perform MRS/MSR instruction and a return.
 * See do_cpregister_rw function. Value passed to function is
 * accessed from r0 register.
 */
static noinline u64 cpaccess_dummy(u64 write_val)
{
	u64 ret = 0xDEADBEEFDEADBEEF;
	asm volatile (".globl cpaccess_dummy_inst\n\t"
			"cpaccess_dummy_inst:\n\t"
			"msr midr_el1, %0\n\t"
			"mov %0, x0\n\t" : "=r" (ret) : "r" (write_val));
	return ret;
} __aligned(32)

/*
 * do_cpregister_rw - Read/Write CP registers
 * @write: 1 for Write and 0 for Read operation
 *
 * Returns value read from CP register
 */
static u64 do_cpregister_rw(int write)
{
	u64 ret;
	u32 opcode;
	u32 *p_opcode;

	/*
	 * Mask the crn, crm, op1, op2 and cp values so they do not
	 * interfere with other fields of the op code.
	 */
	per_cpu(cp_param.crn, cpu) &= 0xF;
	per_cpu(cp_param.crm, cpu) &= 0xF;
	per_cpu(cp_param.op1, cpu) &= 0x7;
	per_cpu(cp_param.op2, cpu) &= 0x7;
	per_cpu(cp_param.op0, cpu) &= 0x3;

	/*
	 * Base MRS opcode for MIDR is 0xD5200000,
	 * MSR is 0xD5000000
	 */
	opcode = (write == 1 ? 0xD5000000 : 0xD5200000);
	opcode |= (per_cpu(cp_param.op0, cpu)<<19) |
	(per_cpu(cp_param.op1, cpu)<<16) |
	(per_cpu(cp_param.crn, cpu)<<12) |
	(per_cpu(cp_param.crm, cpu)<<8) |
	(per_cpu(cp_param.op2, cpu)<<5);

	/*
	 * Grab address of the Dummy function, write the MRS/MSR
	 * instruction, ensuring cache coherency.
	 */
	p_opcode = (u32 *)&cpaccess_dummy_inst;
	aarch64_insn_write(p_opcode, opcode);

	/*
	 * Use smp_call_function_single to do CPU core specific
	 * get_asm_value function call.
	 */
	  if (smp_call_function_single(cpu, get_asm_value, &ret, 1) != 0)
		pr_err(KERN_ERR "Error cpaccess smp call single\n");

	return ret;
}

static int get_register_params(char *str_tmp)
{
	unsigned long op1, op2, crn, crm, op0, write_value;
	char rw;
	int cnt = 0;
	char *p;

	p = strsep(&str_tmp, ":");
	if (p == NULL)
		return -EINVAL;

	strlcpy(type, p, TYPE_MAX_CHARACTERS);
	if (strncasecmp(type, "S", TYPE_MAX_CHARACTERS) == 0) {

		sscanf(str_tmp, "%lu:%lu:%lu:%lu:%lu:%c:%lx:%d",
			&op0, &op1, &crn, &crm, &op2, &rw, &write_value, &cpu);
		per_cpu(cp_param.op0, cpu) = op0;
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
 * get_asm_value - Read/Write CP registers
 * @ret:	Pointer	to return value in case of CP register
 * read op.
 *
 */
static void get_asm_value(void *ret)
{
	*(u64 *)ret =
	 cpaccess_dummy(per_cpu(cp_param.write_value, cpu));
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
	u64 ret;

	if (strncasecmp(type, "S", TYPE_MAX_CHARACTERS) == 0)
		ret = snprintf(buf, TYPE_MAX_CHARACTERS, "%llx\n",
					do_cpregister_rw(0));
	else if (strncasecmp(type, "IL2", TYPE_MAX_CHARACTERS) == 0)
		ret = snprintf(buf, TYPE_MAX_CHARACTERS, "%lx\n",
					    il2_output);
	else
		ret = -EINVAL;

	if (cp_sem.count <= 0)
		up(&cp_sem);

	return ret;
}

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
	int error;

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

	return 0;

exit1:
	device_unregister(&cpaccess_dev);
exit0:
	return error;
}

static void __exit exit_cpaccess_sysfs(void)
{
	sysfs_remove_group(&cpaccess_dev.kobj, &attr_group);
	device_unregister(&cpaccess_dev);
}

module_init(init_cpaccess_sysfs);
module_exit(exit_cpaccess_sysfs);
MODULE_LICENSE("GPL v2");
