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

struct emi_mpu_test {
	unsigned long long dram_start;
	unsigned long long dram_end;
	unsigned int region_cnt;
	unsigned int domain_cnt;
	unsigned int addr_align;
	unsigned int ctrl_intf;
	unsigned int show_region;
	struct device_driver *driver;
};

static struct emi_mpu_test *global_mpu_test;

static unsigned int emimpu_read_protection(
	unsigned int reg_type, unsigned int region, unsigned int dgroup)
{
	struct arm_smccc_res smc_res;

	arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_READ,
		reg_type, region, dgroup, 0, 0, 0, &smc_res);

	return (unsigned int)smc_res.a0;
}

static ssize_t emimpu_ctrl_show(struct device_driver *driver, char *buf)
{
	struct emi_mpu_test *mpu_test;
	ssize_t ret;
	unsigned int i;
	unsigned int region;
	unsigned int apc;
	unsigned long long start, end;
	static const char *permission[8] = {
		"No",
		"S_RW",
		"S_RW_NS_R",
		"S_RW_NS_W",
		"S_R_NS_R",
		"FORBIDDEN",
		"S_R_NS_RW",
		"NONE"
	};

	mpu_test = global_mpu_test;
	if (!mpu_test)
		return -ENXIO;

	for (ret = 0, region = mpu_test->show_region;
		region < mpu_test->region_cnt; region++) {
		start = (unsigned long long)emimpu_read_protection(
			MTK_EMIMPU_READ_SA, region, 0);
		start = (start << (mpu_test->addr_align)) +
			mpu_test->dram_start;

		end = (unsigned long long)emimpu_read_protection(
			MTK_EMIMPU_READ_EA, region, 0);
		end = (end << (mpu_test->addr_align)) +
			mpu_test->dram_start;

		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"R%u-> 0x%llx to 0x%llx\n",
			region, start, end + 0xFFFF);
		if (ret >= PAGE_SIZE)
			return strlen(buf);

		for (i = 0; i < (mpu_test->domain_cnt / 8); i++) {
			apc = emimpu_read_protection(
				MTK_EMIMPU_READ_APC, region, i);
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"%s, %s, %s, %s\n%s, %s, %s, %s\n\n",
				permission[(apc >> 0) & 0x7],
				permission[(apc >> 3) & 0x7],
				permission[(apc >> 6) & 0x7],
				permission[(apc >> 9) & 0x7],
				permission[(apc >> 12) & 0x7],
				permission[(apc >> 15) & 0x7],
				permission[(apc >> 18) & 0x7],
				permission[(apc >> 21) & 0x7]);
			if (ret >= PAGE_SIZE)
				return strlen(buf);
		}
	}

	return strlen(buf);
}

static ssize_t emimpu_ctrl_store
	(struct device_driver *driver, const char *buf, size_t count)
{
	struct emi_mpu_test *mpu_test;
	char *command, *backup_command;
	char *ptr, *token[MTK_EMI_MAX_TOKEN];
	static struct emimpu_region_t *rg_info;
	unsigned long long start, end;
	unsigned long region;
	unsigned long dgroup;
	unsigned long apc;
	int i, j, ret;

	mpu_test = global_mpu_test;
	if (!mpu_test)
		return -EFAULT;

	if (!(mpu_test->ctrl_intf))
		return count;

	if (!rg_info) {
		rg_info = kmalloc(sizeof(struct emimpu_region_t), GFP_KERNEL);
		if (!rg_info)
			return -ENOMEM;
		rg_info->apc = kmalloc_array(
			mpu_test->domain_cnt, sizeof(unsigned int),
			GFP_KERNEL);
		if (!(rg_info->apc)) {
			kfree(rg_info);
			rg_info = NULL;
			return -ENOMEM;
		}
		rg_info->lock = false;
	}

	if ((strlen(buf) + 1) > MTK_EMI_MAX_CMD_LEN) {
		pr_info("%s: store command overflow\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: store: %s\n", __func__, buf);

	command = kmalloc((size_t)MTK_EMI_MAX_CMD_LEN, GFP_KERNEL);
	if (!command)
		return -ENOMEM;
	backup_command = command;
	strncpy(command, buf, (size_t)MTK_EMI_MAX_CMD_LEN);

	for (i = 0; i < MTK_EMI_MAX_TOKEN; i++) {
		ptr = strsep(&command, " ");
		if (!ptr)
			break;
		token[i] = ptr;
	}

	if (!strncmp(buf, "SHOW", strlen("SHOW"))) {
		if (i < 2)
			goto emimpu_ctrl_store_end;

		pr_info("%s: %s %s\n", __func__, token[0], token[1]);

		ret = kstrtoul(token[1], 10, &region);
		if (ret != 0)
			pr_info("%s: fail to parse region\n", __func__);

		if (region < mpu_test->region_cnt) {
			mpu_test->show_region = (unsigned int) region;
			pr_info("%s: show_region to %u\n",
				__func__, mpu_test->show_region);
		}
	} else if (!strncmp(buf, "SET", strlen("SET"))) {
		if (i < 3)
			goto emimpu_ctrl_store_end;

		pr_info("%s: %s %s %s\n",
			__func__, token[0], token[1], token[2]);

		ret = kstrtoul(token[1], 10, &dgroup);
		if (ret != 0)
			pr_info("%s: fail to parse dgroup\n", __func__);
		ret = kstrtoul(token[2], 16, &apc);
		if (ret != 0)
			pr_info("[MPU] fail to parse apc\n");

		if (dgroup < (mpu_test->domain_cnt / 8)) {
			pr_info("%s: apc[%lu]: 0x%lx\n",
				__func__, dgroup, apc);

			for (j = 0; j < 8; j++)
				rg_info->apc[dgroup * 8 + j] =
					((unsigned int)apc >> (3 * j)) & 0x7;
		}
		if (dgroup == 0)
			rg_info->lock = apc & 0x80000000;
	} else if (!strncmp(buf, "ON", strlen("ON"))) {
		if (i < 4)
			goto emimpu_ctrl_store_end;

		pr_info("%s: %s %s %s %s\n",
			__func__, token[0], token[1], token[2], token[3]);

		ret = 0;
		if (kstrtoull(token[1], 16, &start) != 0) {
			ret = -1;
			pr_info("%s: fail to parse start\n", __func__);
		}
		if (kstrtoull(token[2], 16, &end) != 0) {
			ret = -1;
			pr_info("%s: fail to parse end\n", __func__);
		}
		if (kstrtoul(token[3], 10, &region) != 0) {
			ret = -1;
			pr_info("%s: fail to parse region\n", __func__);
		}
		if ((ret == 0) && (region < mpu_test->region_cnt)) {
			rg_info->start = start;
			rg_info->end = end;
			rg_info->rg_num = (unsigned int)region;
			mtk_emimpu_set_protection(rg_info);
		}
	} else if (!strncmp(buf, "OFF", strlen("OFF"))) {
		if (i < 2)
			goto emimpu_ctrl_store_end;

		pr_info("%s: %s %s\n", __func__, token[0], token[1]);

		ret = kstrtoul(token[1], 10, &region);
		if (ret != 0)
			pr_info("%s: fail to parse region\n", __func__);

		if (region < mpu_test->region_cnt) {
			rg_info->rg_num = (unsigned int)region;
			mtk_emimpu_clear_protection(rg_info);
		}
	} else
		pr_info("%s: unknown store command\n", __func__);

emimpu_ctrl_store_end:
	kfree(backup_command);

	return count;
}

static DRIVER_ATTR_RW(emimpu_ctrl);

static __init int emimputest_init(void)
{
	struct device_node *node;
	struct platform_device *pdev;
	struct emi_mpu_test *mpu_test;
	int ret;

	pr_info("emimputest was loaded\n");

	node = of_find_compatible_node(NULL, NULL, "mediatek,common-emimpu");
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

	mpu_test = kzalloc(sizeof(struct emi_mpu_test), GFP_KERNEL);
	if (!mpu_test)
		return -ENOMEM;

	ret = of_property_read_u32(node,
		"region_cnt", &(mpu_test->region_cnt));
	if (ret) {
		pr_info("emimputest: no region_cnt\n");
		ret = -ENXIO;
		goto free_emi_mpu_test;
	}

	ret = of_property_read_u32(node,
		"domain_cnt", &(mpu_test->domain_cnt));
	if (ret) {
		pr_info("emimputest: no domain_cnt\n");
		ret = -ENXIO;
		goto free_emi_mpu_test;
	}

	ret = of_property_read_u32(node,
		"addr_align", &(mpu_test->addr_align));
	if (ret) {
		pr_info("emimputest: no addr_align\n");
		ret = -ENXIO;
		goto free_emi_mpu_test;
	}

	ret = of_property_read_u64(node,
		"dram_start", &(mpu_test->dram_start));
	if (ret) {
		pr_info("emimputest: no dram_start\n");
		ret = -ENXIO;
		goto free_emi_mpu_test;
	}

	ret = of_property_read_u64(node,
		"dram_end", &(mpu_test->dram_end));
	if (ret) {
		pr_info("emimputest: no dram_end\n");
		ret = -ENXIO;
		goto free_emi_mpu_test;
	}

	ret = of_property_read_u32(node,
		"ctrl_intf", &(mpu_test->ctrl_intf));
	if (ret) {
		pr_info("emimputest: no ctrl_intf\n");
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

	global_mpu_test = mpu_test;

	return 0;

free_emi_mpu_test:
	kfree(mpu_test);

	return ret;
}

static __exit void emimputest_exit(void)
{
	struct emi_mpu_test *mpu_test = global_mpu_test;

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
