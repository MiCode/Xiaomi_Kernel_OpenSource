/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <mach/scm.h>
#include <linux/mm.h>

#include <asm/sections.h>

#define TZ_PROTECT_MEMORY 0x1

/* filesystem parameters */
#define MPU_MAGIC_LOCK 0x11
#define MPU_MAGIC_UNLOCK 0x10

static ulong mpu_start;
static ulong mpu_size;
static u32 mpu_enable;

module_param(mpu_start, ulong, 0644);
module_param(mpu_size, ulong, 0644);

static void mem_prot_region(u64 start, u64 size, bool lock);
static int set_enabled(const char *val, const struct kernel_param *kp)
{
	int ret = 0;
	ret = param_set_int(val, kp);

	if (mpu_enable == MPU_MAGIC_LOCK)
		mem_prot_region(mpu_start, mpu_size, true);
	else if (mpu_enable == MPU_MAGIC_UNLOCK)
		mem_prot_region(mpu_start, mpu_size, false);
	return ret;
}

static struct kernel_param_ops mpu_ops = {
	.set = set_enabled,
	.get = param_get_int,
};
module_param_cb(mpu_enable, &mpu_ops, &mpu_enable, 0644);

static void mem_prot_region(u64 start, u64 size, bool lock)
{
	int ret;
	struct req_cmd {
		u32 address;
		u32 size;
		u32 permission;
		u32 lock;
		u32 arg;
	} request;

	request.address = PAGE_ALIGN(start);
	request.size =  PAGE_ALIGN(size);
	/*
	 * Permissions:  Write:         Read
	 * 0x1           TZ             Anyone
	 * 0x2           TZ             Tz, APPS
	 * 0x3           TZ             Tz
	 */
	request.permission  = 0x1;
	request.lock = lock;
	request.arg = 0;

	ret = scm_call(SCM_SVC_MP, TZ_PROTECT_MEMORY,
			&request, sizeof(request), &ret, sizeof(ret));

	if (ret != 0)
		pr_err("Failed to %s region %llx - %llx\n",
			lock ? "protect" : "unlock", start, start + size);
	else
		pr_debug("SUCCESS to %s region %llx - %llx\n",
			lock ? "protect" : "unlock", start, start + size);
}

#ifdef CONFIG_KERNEL_TEXT_MPU_PROT
static int __init mem_prot_init(void)
{
	phys_addr_t phys = virt_to_phys(_stext);
	mem_prot_region((u64)phys, (u64)(_etext - _stext), true);
	return 0;
}
late_initcall(mem_prot_init);
#else
static int __init mem_prot_init(void)
{
	if (mpu_start && mpu_size)
		mem_prot_region(mpu_start, mpu_size, true);
	return 0;
}
late_initcall(mem_prot_init);
#endif
