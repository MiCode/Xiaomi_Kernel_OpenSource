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

static uint32_t mem_size;
static int ddr_health_set(const char *val, struct kernel_param *kp);
module_param_call(mem_size, ddr_health_set, param_get_uint,
		  &mem_size, 0644);

struct ddr_health {
	uint64_t    addr;
	uint32_t    size;
	uint32_t    reserved;
};

static struct mutex lock;
static struct ddr_health *ddr_health;
static struct msm_rpm_kvp rpm_kvp;

static int ddr_health_set(const char *val, struct kernel_param *kp)
{
	int	 ret;
	void	 *virt;
	uint64_t old_addr = 0;
	uint32_t old_size = 0;

	mutex_lock(&lock);
	ret = param_set_uint(val, kp);
	if (ret) {
		pr_err("ddr-health: error setting value %d\n", ret);
		mutex_unlock(&lock);
		return ret;
	}

	if (rpm_kvp.data) {
		ddr_health = (struct ddr_health *)rpm_kvp.data;
		old_addr = ddr_health->addr;
		old_size = ddr_health->size;
	}

	rpm_kvp.key = RPM_MISC_REQ_DDR_HEALTH;

	if (mem_size) {
		virt = kzalloc(mem_size, GFP_KERNEL);
		if (!virt) {
			pr_err("ddr-health: failed to alloc mem request %x\n",
			       mem_size);
			mutex_unlock(&lock);
			return -ENOMEM;
		}

		ddr_health->addr = (uint64_t)virt_to_phys(virt);
		ddr_health->size = mem_size;

		rpm_kvp.length = sizeof(struct ddr_health);
		rpm_kvp.data = (void *)ddr_health;

		ret = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET,
					   RPM_MISC_REQ_TYPE, 0, &rpm_kvp, 1);
		if (ret) {
			pr_err("ddr-health: send buf to RPM failed %d, %x\n",
			       ret, mem_size);
			kfree(virt);
			goto err;
		}
	} else {
		ddr_health->addr = 0;
		ddr_health->size = 0;

		rpm_kvp.length = sizeof(struct ddr_health);
		rpm_kvp.data = (void *)ddr_health;

		ret = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET,
					   RPM_MISC_REQ_TYPE, 0, &rpm_kvp, 1);
		if (ret) {
			pr_err("ddr-health: send nobuf to RPM failed %d, %x\n",
			       ret, mem_size);
			goto err;
		}
	}

	if (old_addr)
		kfree(phys_to_virt((phys_addr_t)old_addr));

	mutex_unlock(&lock);
	return 0;
err:
	ddr_health->addr = old_addr;
	ddr_health->size = old_size;
	mutex_unlock(&lock);
	return ret;
}

static int __init ddr_health_init(void)
{
	mutex_init(&lock);

	ddr_health = kzalloc(sizeof(*ddr_health), GFP_KERNEL);
	if (!ddr_health) {
		pr_err("ddr-health: failed to alloc mem\n");
		return -ENOMEM;
	}

	return 0;
}
module_init(ddr_health_init);

static void __exit ddr_health_exit(void)
{
	if (rpm_kvp.data) {
		ddr_health = (struct ddr_health *)rpm_kvp.data;

		if (ddr_health->addr)
			kfree(phys_to_virt((phys_addr_t)ddr_health->addr));
	}

	kfree(ddr_health);
	mutex_destroy(&lock);
}
module_exit(ddr_health_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DDR health monitor driver");
