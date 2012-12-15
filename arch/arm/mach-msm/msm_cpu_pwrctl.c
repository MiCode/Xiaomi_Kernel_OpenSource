/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/errno.h>

static const phys_addr_t primary_cpu_pwrctl_phys = 0x2088004;

static int __init msm_cpu_pwrctl_init(void)
{
	void *pwrctl_ptr;
	unsigned int value;
	int rc = 0;

	pwrctl_ptr = ioremap_nocache(primary_cpu_pwrctl_phys, SZ_4K);
	if (unlikely(!pwrctl_ptr)) {
		pr_err("Failed to remap APCS_CPU_PWR_CTL register for CPU0\n");
		rc = -EINVAL;
		goto done;
	}

	value = readl_relaxed(pwrctl_ptr);
	value |= 0x100;
	writel_relaxed(value, pwrctl_ptr);
	mb();
	iounmap(pwrctl_ptr);

done:
	return rc;
}

early_initcall(msm_cpu_pwrctl_init);
