/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/mutex.h>
#include <soc/qcom/rpm-smd.h>

#define RPM_MISC_REQ_TYPE	0x6373696d
#define RPM_MISC_REQ_DDR_HEALTH 0x31726464

static unsigned long mem_size;
static int ddr_health_set(const char *val, struct kernel_param *kp);
module_param_call(mem_size, ddr_health_set, param_get_ulong,
		  &mem_size, 0644);

static struct mutex lock;
static struct msm_rpm_kvp rpm_kvp;

static int ddr_health_set(const char *val, struct kernel_param *kp)
{
	int ret;
	void *virt;
	phys_addr_t addr = 0;

	mutex_lock(&lock);
	ret = param_set_ulong(val, kp);
	if (ret) {
		pr_err("ddr-health: error setting value %d\n", ret);
		mutex_unlock(&lock);
		return ret;
	}

	if (rpm_kvp.data)
		addr = (phys_addr_t)rpm_kvp.data;

	rpm_kvp.key = RPM_MISC_REQ_DDR_HEALTH;

	if (mem_size) {
		virt = kzalloc(mem_size, GFP_KERNEL);
		if (!virt) {
			pr_err("ddr-health: failed to alloc mem %lx\n",
			       mem_size);
			mutex_unlock(&lock);
			return -ENOMEM;
		}

		rpm_kvp.length = (uint32_t)mem_size;
		rpm_kvp.data = (void *)virt_to_phys(virt);

		ret = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET,
					   RPM_MISC_REQ_TYPE, 0, &rpm_kvp, 1);
		if (ret) {
			kfree(phys_to_virt((phys_addr_t)rpm_kvp.data));
			rpm_kvp.data = 0;
			pr_err("ddr-health: send buf to RPM failed %d, %lx\n",
			       ret, mem_size);
			mutex_unlock(&lock);
			return ret;
		}
	} else {
		rpm_kvp.length = 0;
		rpm_kvp.data = 0;

		ret = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET,
					   RPM_MISC_REQ_TYPE, 0, &rpm_kvp, 1);
		if (ret) {
			pr_err("ddr-health: send nobuf to RPM failed %d,%lx\n",
			       ret, mem_size);
			mutex_unlock(&lock);
			return ret;
		}
	}

	if (addr)
		kfree(phys_to_virt(addr));

	mutex_unlock(&lock);
	return 0;
}

static int __init ddr_health_init(void)
{
	mutex_init(&lock);
	return 0;
}
module_init(ddr_health_init);

static void __exit ddr_health_exit(void)
{
	if (rpm_kvp.data)
		kfree(phys_to_virt((phys_addr_t)rpm_kvp.data));

	mutex_destroy(&lock);
}
module_exit(ddr_health_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DDR health monitor driver");
