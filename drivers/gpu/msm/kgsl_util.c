// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */


#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/ktime.h>
#include <linux/of_address.h>
#include <linux/qcom_scm.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/soc/qcom/mdt_loader.h>
#include <linux/string.h>

#include "kgsl_util.h"

bool kgsl_regulator_disable_wait(struct regulator *reg, u32 timeout)
{
	ktime_t tout = ktime_add_us(ktime_get(), timeout * 1000);

	if (IS_ERR_OR_NULL(reg))
		return true;

	regulator_disable(reg);

	for (;;) {
		if (!regulator_is_enabled(reg))
			return true;

		if (ktime_compare(ktime_get(), tout) > 0)
			return (!regulator_is_enabled(reg));

		usleep_range((100 >> 2) + 1, 100);
	}
}

struct clk *kgsl_of_clk_by_name(struct clk_bulk_data *clks, int count,
		const char *id)
{
	int i;

	for (i = 0; clks && i < count; i++)
		if (!strcmp(clks[i].id, id))
			return clks[i].clk;

	return NULL;
}

int kgsl_regulator_set_voltage(struct device *dev,
		struct regulator *reg, u32 voltage)
{
	int ret;

	if (IS_ERR_OR_NULL(reg))
		return 0;

	ret = regulator_set_voltage(reg, voltage, INT_MAX);
	if (ret)
		dev_err(dev, "Regulator set voltage:%d failed:%d\n", voltage, ret);

	return ret;
}

int kgsl_clk_set_rate(struct clk_bulk_data *clks, int num_clks,
		const char *id, unsigned long rate)
{
	struct clk *clk;

	clk = kgsl_of_clk_by_name(clks, num_clks, id);
	if (!clk)
		return -ENODEV;

	return clk_set_rate(clk, rate);
}

/*
 * The PASID has stayed consistent across all targets thus far so we are
 * cautiously optimistic that we can hard code it
 */
#define GPU_PASID 13

int kgsl_zap_shader_load(struct device *dev, const char *name)
{
	struct device_node *np, *mem_np;
	const struct firmware *fw;
	void *mem_region = NULL;
	phys_addr_t mem_phys;
	struct resource res;
	const char *fwname;
	ssize_t mem_size;
	int ret;

	np = of_get_child_by_name(dev->of_node, "zap-shader");
	if (!np) {
		dev_err(dev, "zap-shader node not found. Please update the device tree\n");
		return -ENODEV;
	}

	mem_np = of_parse_phandle(np, "memory-region", 0);
	of_node_put(np);
	if (!mem_np) {
		dev_err(dev, "Couldn't parse the mem-region from the zap-shader node\n");
		return -EINVAL;
	}

	ret = of_address_to_resource(mem_np, 0, &res);
	of_node_put(mem_np);
	if (ret)
		return ret;

	/*
	 * To avoid confusion we will keep the "legacy" naming scheme
	 * without the .mdt postfix (i.e. "a660_zap") outside of this function
	 * so we have to fix it up here
	 */
	fwname = kasprintf(GFP_KERNEL, "%s.mdt", name);
	if (!fwname)
		return -ENOMEM;

	ret = request_firmware(&fw, fwname, dev);
	if (ret) {
		dev_err(dev, "Couldn't load the firmware %s\n", fwname);
		kfree(fwname);
		return ret;
	}

	mem_size = qcom_mdt_get_size(fw);
	if (mem_size < 0) {
		ret = mem_size;
		goto out;
	}

	if (mem_size > resource_size(&res)) {
		ret = -E2BIG;
		goto out;
	}

	mem_phys = res.start;

	mem_region = memremap(mem_phys, mem_size, MEMREMAP_WC);
	if (!mem_region) {
		ret = -ENOMEM;
		goto out;
	}

	ret = qcom_mdt_load(dev, fw, fwname, GPU_PASID, mem_region,
		mem_phys, mem_size, NULL);
	if (ret) {
		dev_err(dev, "Error %d while loading the MDT\n", ret);
		goto out;
	}

	ret = qcom_scm_pas_auth_and_reset(GPU_PASID);

out:
	if (mem_region)
		memunmap(mem_region);

	release_firmware(fw);
	kfree(fwname);
	return ret;
}

int kgsl_hwlock(struct cpu_gpu_lock *lock)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);

	/* Indicate that the CPU wants the lock */
	lock->cpu_req = 1;

	/* post the request */
	wmb();

	/* Wait for our turn */
	lock->turn = 0;

	/* Finish all memory transactions before moving on */
	mb();

	/*
	 * Spin here while GPU ucode holds the lock, lock->gpu_req will
	 * be set to 0 after GPU ucode releases the lock. Maximum wait time
	 * is 1 second and this should be enough for GPU to release the lock.
	 */
	while (lock->gpu_req && lock->turn == 0) {
		cpu_relax();
		/* Get the latest updates from GPU */
		rmb();

		if (time_after(jiffies, timeout))
			break;
	}

	if (lock->gpu_req && lock->turn == 0)
		return -EBUSY;

	return 0;
}

void kgsl_hwunlock(struct cpu_gpu_lock *lock)
{
	/* Make sure all writes are done before releasing the lock */
	wmb();
	lock->cpu_req = 0;
}
