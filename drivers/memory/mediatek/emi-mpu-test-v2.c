// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <soc/mediatek/emi.h>

struct emi_mpu_test_v2 {
	unsigned long long dram_start;
	unsigned long long dram_end;
	unsigned int sr_cnt;
	unsigned int sr_num;
	unsigned int aid_cnt;
	unsigned int aid_num_per_set;
	unsigned int max_aid_set;
	struct aid_table *aid_table;
	struct device_driver *driver;
};

struct aid_table {
	unsigned int start;
	unsigned long long aid_val;
};

static struct emi_mpu_test_v2 *global_mpu_test;

static unsigned long long emimpu_read_protection(
	unsigned int reg_type, unsigned int sr_num, unsigned int aid_shift)
{
	struct arm_smccc_res smc_res;

	arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_READ,
		reg_type, sr_num, aid_shift, 0, 0, 0, &smc_res);

	return smc_res.a0;
}

static ssize_t emimpu_ctrl_show(struct device_driver *driver, char *buf)
{
	struct emi_mpu_test_v2 *mpu_test;
	struct aid_table *table;
	ssize_t ret = 0;
	unsigned int sr_num;
	unsigned long long start, end;
	unsigned int enable_val;
	unsigned int i, j, k;
	unsigned int val;
	unsigned int aid_num;

	static const char *aid_permission[4] = {
		"No",
		"WO",
		"RO",
		"RW",
	};
	mpu_test = global_mpu_test;
	if (!mpu_test) {
		pr_info("%s: no valid mpu info\n", __func__);
		return -ENXIO;
	}

	sr_num = mpu_test->sr_num;
	if (sr_num > mpu_test->sr_cnt) {
		pr_info("%s: sr_num is not valid\n", __func__);
		return -EINVAL;
	}
	if (sr_num == 0) {
		k = 0;
		enable_val = emimpu_read_protection(MTK_EMIMPU_READ_ENABLE, sr_num, 0);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"NSR -> %s\n",
				enable_val ? "enable" : "disable");
		if (ret >= PAGE_SIZE)
			return strlen(buf);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"aid  perm   aid  perm   aid  perm   aid  perm   aid  perm   aid  perm\n");
		if (ret >= PAGE_SIZE)
			return strlen(buf);
		table = kcalloc(mpu_test->max_aid_set, sizeof(struct aid_table), GFP_KERNEL);
		if (!table)
			return -ENOMEM;
		mpu_test->aid_table = table;
		for (i = 0; i < mpu_test->max_aid_set; i++) {
			mpu_test->aid_table[i].start = i * mpu_test->aid_num_per_set;
			mpu_test->aid_table[i].aid_val = emimpu_read_protection(
								MTK_EMIMPU_READ_AID, sr_num, i);
			if (!(mpu_test->aid_table[i].aid_val))
				continue;
			for (j = 0; j < mpu_test->aid_num_per_set; j++) {
				val = (mpu_test->aid_table[i].aid_val >> (j * 2)) & 0x3;
				if (!val)
					continue;
				aid_num = mpu_test->aid_table[i].start + j;
				pr_info("%s: aid_num=%d, aid_val = 0x%llx, aid = %d\n",
					__func__, aid_num, mpu_test->aid_table[i].aid_val, val);
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
						"%03d  %s     ",
						aid_num, aid_permission[val]);
				if (ret >= PAGE_SIZE)
					return strlen(buf);
				k++;
				if (k == 6) {
					ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
					if (ret >= PAGE_SIZE)
						return strlen(buf);
					k = 0;
				}
			}
		}
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
		return strlen(buf);
	}
	start = emimpu_read_protection(MTK_EMIMPU_READ_SA, sr_num, 0);
	end = emimpu_read_protection(MTK_EMIMPU_READ_EA, sr_num, 0);
	enable_val = emimpu_read_protection(MTK_EMIMPU_READ_ENABLE, sr_num, 0);

	pr_info("%s: emimpu_ctrl sr_num: %d, 0x%x, 0x%x\n",
		__func__, sr_num, start, end);
	ret = snprintf(buf + ret, PAGE_SIZE - ret,
			"SR:%u -> 0x%llx to 0x%llx; %s\n",
			sr_num, start, end,
			enable_val ? "enable" : "disable");
	if (ret >= PAGE_SIZE)
		return strlen(buf);

	table = kcalloc(mpu_test->max_aid_set, sizeof(struct aid_table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	mpu_test->aid_table = table;

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"aid  perm\n");
	if (ret >= PAGE_SIZE)
		return strlen(buf);

	for (i = 0; i < mpu_test->max_aid_set; i++) {
		mpu_test->aid_table[i].start = i * mpu_test->aid_num_per_set;
		mpu_test->aid_table[i].aid_val = emimpu_read_protection(
							MTK_EMIMPU_READ_AID, sr_num, i);
		if (mpu_test->aid_table[i].aid_val) {
			for (j = 0; j < mpu_test->aid_num_per_set; j++) {
				val = (mpu_test->aid_table[i].aid_val >> (j * 2)) & 0x3;
				if (val) {
					aid_num = mpu_test->aid_table[i].start + j;
					pr_info("%s: aid_num=%d, aid_val = 0x%llx, aid = %d\n",
					__func__, aid_num, mpu_test->aid_table[i].aid_val, val);
					ret += snprintf(buf + ret, PAGE_SIZE - ret,
							"%03d  %s\n",
							aid_num, aid_permission[val]);
					if (ret >= PAGE_SIZE)
						return strlen(buf);
				}
			}
		}
	}

	/* For full aid info debug */
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"-----------------\n");
	if (ret >= PAGE_SIZE)
		return strlen(buf);
	for (i = 0; i < mpu_test->max_aid_set; i++) {
		if (mpu_test->aid_table[i].aid_val) {
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"aid_%d 0x%llx\n",
					mpu_test->aid_table[i].start,
					mpu_test->aid_table[i].aid_val);
			if (ret >= PAGE_SIZE)
				return strlen(buf);
		}
	}

	return strlen(buf);
}

static ssize_t emimpu_ctrl_store
	(struct device_driver *driver, const char *buf, size_t count)
{
	struct emi_mpu_test_v2 *mpu_test;
	unsigned int sr_num;
	int ret;

	mpu_test = global_mpu_test;
	if (!mpu_test)
		return -EFAULT;

	ret = sscanf(buf, "%u\n", &sr_num);

	if (ret == 1 && sr_num <= mpu_test->sr_cnt) {
		mpu_test->sr_num = sr_num;
		pr_info("%s: show_sr_num : %u\n", __func__, mpu_test->sr_num);
	} else {
		mpu_test->sr_num = 9999;
		pr_info("%s: please keyin correct sr_num\n", __func__);
		return -EINVAL;
	}

	return count;
}

static DRIVER_ATTR_RW(emimpu_ctrl);

static __init int emimputest_init(void)
{
	struct device_node *node;
	struct platform_device *pdev;
	struct emi_mpu_test_v2 *mpu_test;
	int ret;

	pr_info("emimputest was loaded\n");

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6983-emimpu");
	if (!node) {
		pr_info("emimputest: cannot find emimpu node\n");
		return -ENXIO;
	}

	pdev = of_find_device_by_node(node);
	if (!pdev) {
		pr_info("emimputest: cannot find emimpu pdev\n");
		return -ENXIO;
	}
	if (!pdev->dev.driver) {
		pr_info("emimputest: cannot find emimpu driver\n");
		return -ENXIO;
	}

	mpu_test = kzalloc(sizeof(struct emi_mpu_test_v2), GFP_KERNEL);
	if (!mpu_test)
		return -ENOMEM;

	ret = of_property_read_u32(node,
		"sr_cnt", &(mpu_test->sr_cnt));
	if (ret) {
		pr_info("emimputest: no sr_cnt\n");
		ret = -ENXIO;
		goto free_emi_mpu_test;
	}

	ret = of_property_read_u32(node,
		"aid_cnt", &(mpu_test->aid_cnt));
	if (ret) {
		pr_info("emimputest: no aid_cnt\n");
		ret = -ENXIO;
		goto free_emi_mpu_test;
	}

	ret = of_property_read_u32(node,
		"aid_num_per_set", &(mpu_test->aid_num_per_set));
	if (ret) {
		pr_info("emimputest: no aid_num_per_set\n");
		ret = -ENXIO;
		goto free_emi_mpu_test;
	}

	ret = driver_create_file(pdev->dev.driver,
				 &driver_attr_emimpu_ctrl);
	if (ret) {
		pr_info("emimputest: failed to create file\n");
		goto free_emi_mpu_test;
	}
	mpu_test->driver = pdev->dev.driver;
	mpu_test->max_aid_set = mpu_test->aid_cnt / mpu_test->aid_num_per_set;
	mpu_test->sr_num = -1;
	global_mpu_test = mpu_test;

	return 0;

free_emi_mpu_test:
	kfree(mpu_test);

	return ret;
}

static __exit void emimputest_exit(void)
{
	struct emi_mpu_test_v2 *mpu_test = global_mpu_test;

	pr_info("emimputest was unloaded\n");

	driver_remove_file(mpu_test->driver,
			&driver_attr_emimpu_ctrl);

	global_mpu_test = NULL;

	kfree(mpu_test);
}

module_init(emimputest_init);
module_exit(emimputest_exit);

MODULE_DESCRIPTION("MediaTek EMI MPU Driver");
MODULE_LICENSE("GPL v2");
