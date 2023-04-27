// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Sagy Shih <sagy.shih@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <soc/mediatek/emi.h>

//========= Structure ==================

#if (CONFIG_MTK_EMI_BWL_VERSION == 1)
#include<emibwl/1.0/emibwl.h>
#else
#include<emibwl/default/emibwl.h>
#endif

/**
 * structure to store emi info.
 */
struct emi_cen {
	unsigned int emi_cen_cnt;
	unsigned int emi_chn_cnt;
	unsigned int ch_cnt;
	unsigned int rk_cnt;
	unsigned long long *rk_size;
	void __iomem **emi_cen_base;
	void __iomem **emi_chn_base;
};
static struct emi_cen *global_emi_cen;

struct scn_name_t {
	char *name;
};

struct scn_reg_t {
	unsigned int offset;
	unsigned int value;
};

//========= Variables ==================

DEFINE_SEMAPHORE(bwl_sem);

// reg table and pointer
static struct scn_reg_t (*env_cen_reg)[BWL_CEN_MAX];
static struct scn_reg_t cen_reg[BWL_ENV_MAX][BWL_SCN_MAX][BWL_CEN_MAX] = {
#define SET_BWL_CEN_REG(ENV, SCN, OFFSET, VAL) \
	[ENV][SCN][BWL_CEN_##OFFSET].offset = OFFSET, \
	[ENV][SCN][BWL_CEN_##OFFSET].value = VAL,
#define SET_BWL_CHN_REG(ENV, SCN, OFFSET, VAL)
#if (CONFIG_MTK_EMI_BWL_VERSION == 1)
#include<emibwl/1.0/bwl_scenario.h>
#endif
#undef SET_BWL_CEN_REG
#undef SET_BWL_CHN_REG
};
static struct scn_reg_t (*env_chn_reg)[BWL_CHN_MAX];
static struct scn_reg_t chn_reg[BWL_ENV_MAX][BWL_SCN_MAX][BWL_CHN_MAX] = {
#define SET_BWL_CEN_REG(ENV, SCN, OFFSET, VAL)
#define SET_BWL_CHN_REG(ENV, SCN, OFFSET, VAL) \
	[ENV][SCN][BWL_CHN_##OFFSET].offset = OFFSET, \
	[ENV][SCN][BWL_CHN_##OFFSET].value = VAL,
#if (CONFIG_MTK_EMI_BWL_VERSION == 1)
#include<emibwl/1.0/bwl_scenario.h>
#endif
#undef SET_BWL_CEN_REG
#undef SET_BWL_CHN_REG
};

// BWL control table
static unsigned int ctrl_table[BWL_SCN_MAX];
static unsigned int cur_scn = 0xFFFFFFFF;
static struct scn_name_t scn_name[BWL_SCN_MAX] = {
#define SET_BWL_CEN_REG(ENV, SCN, OFFSET, VAL) \
	[SCN].name = #SCN,
#define SET_BWL_CHN_REG(ENV, SCN, OFFSET, VAL)
#if (CONFIG_MTK_EMI_BWL_VERSION == 1)
#include<emibwl/1.0/bwl_scenario.h>
#endif
#undef SET_BWL_CEN_REG
#undef SET_BWL_CHN_REG
};

//========= Function ==================

unsigned int decode_bwl_env(unsigned int ch_num)
{
	if (ch_num == 1)
		return BWL_ENV_LPDDR3_1CH;
	else
		return BWL_ENV_LPDDR4_2CH;
}

void set_emi_reg(unsigned int scn)
{
	int i, j;
	unsigned int value, offset;

	for (i = 0; i < BWL_CEN_MAX; i++) {
		value = env_cen_reg[scn][i].value;
		offset = env_cen_reg[scn][i].offset;

		writel(value, global_emi_cen->emi_cen_base[0] + offset);
	}
	for (i = 0; i < BWL_CHN_MAX; i++) {
		value = env_chn_reg[scn][i].value;
		offset = env_chn_reg[scn][i].offset;

		for (j = 0; j < global_emi_cen->emi_chn_cnt; j++)
			writel(value, global_emi_cen->emi_chn_base[j] + offset);
	}
}

// op: 0 for disable, 1 for enable
int bwl_ctrl(unsigned int scn, unsigned int op)
{
	int i;
	int highest;

	if (scn >= BWL_SCN_MAX)
		return -1;

	if (op > 1)
		return -1;

	if (in_interrupt())
		return -1;

	down(&bwl_sem);

	if (op == 1)
		ctrl_table[scn]++;
	else if (op == 0) {
		if (ctrl_table[scn] != 0)
			ctrl_table[scn]--;
	}

	// find the scenario with the highest priority
	highest = -1;
	for (i = 0; i < BWL_SCN_MAX; i++) {
		if (ctrl_table[i] != 0) {
			highest = i;
			break;
		}
	}
	if (highest == -1)
		highest = SCN_DEFAULT;

	// set new EMI bandwidth limiter value
	if (highest != cur_scn) {
		set_emi_reg(highest);
		cur_scn = highest;
	}

	up(&bwl_sem);

	return 0;
}

// control the feature on/off dynamically.
bool enable = true;

static ssize_t concurrency_scenario_store
	(struct device_driver *driver, const char *buf, size_t count)
{
	int i;
	char *name;

	if (!strncmp(buf, "ENABLE", strlen("ENABLE")))
		enable = true;
	if (!strncmp(buf, "DISABLE", strlen("DISABLE")))
		enable = false;

	if (!enable) {
		pr_info("%s: feature not enabled(%d)", __func__, enable);
		return count;
	}

	for (i = 0; i < BWL_SCN_MAX; i++) {
		name = scn_name[i].name;

		if (strncmp(buf, name, strlen(name)))
			continue;

		if (!strncmp(buf + strlen(name) + 1,
			"ON", strlen("ON"))) {

			bwl_ctrl(i, 1);
			/* pr_info("[BWL] %s ON\n", name); */
			break;
		} else if (!strncmp(buf + strlen(name) + 1,
			"OFF", strlen("OFF"))) {

			bwl_ctrl(i, 0);
			/* pr_info("[BWL] %s OFF\n", name); */
			break;
		}
	}

	return count;
}

static ssize_t concurrency_scenario_show
	(struct device_driver *driver, char *buf)
{
	ssize_t ret = 0;
	unsigned int offset;
	int i, j;

	if (cur_scn >= BWL_SCN_MAX)
		return sprintf(buf, "none\n");

	ret += snprintf(buf, 64, "current scenario: %s\n",
			scn_name[cur_scn].name);
	ret += snprintf(buf + ret, 32, "%s\n", scn_name[cur_scn].name);

	for (i = 0; i < BWL_CEN_MAX; i++) {
		if (ret >= PAGE_SIZE)
			return strlen(buf);

		offset = env_cen_reg[cur_scn][i].offset;
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "CEN 0x%x: 0x%x\n",
			offset, readl(global_emi_cen->emi_cen_base[0] + offset));
	}

	for (i = 0; i < BWL_CHN_MAX; i++) {
		offset = env_chn_reg[cur_scn][i].offset;

		for (j = 0; j < global_emi_cen->emi_chn_cnt; j++) {
			if (ret >= PAGE_SIZE)
				return strlen(buf);

			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"CH%d 0x%x:0x%x\n", j, offset,
				readl(global_emi_cen->emi_chn_base[j] + offset));
		}
	}

	for (i = 0; i < BWL_SCN_MAX; i++) {
		if (ret >= PAGE_SIZE)
			return strlen(buf);

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%s = 0x%x\n",
			scn_name[i].name, ctrl_table[i]);
	}

	return strlen(buf);
}

static DRIVER_ATTR_RW(concurrency_scenario);

void bwl_init(struct platform_driver *emictrl_drv)
{
	int ret;
	unsigned int env;

	env = decode_bwl_env(global_emi_cen->ch_cnt);
	env_cen_reg = cen_reg[env];
	env_chn_reg = chn_reg[env];

	bwl_ctrl(SCN_DEFAULT, 1);

	ret = driver_create_file(&emictrl_drv->driver,
		&driver_attr_concurrency_scenario);
	if (ret)
		pr_info("[BWL] fail to create concurrency_scenario\n");
}


//======== emi_ctrl ===========================================

static int emictrl_probe(struct platform_device *pdev);
static int emictrl_remove(struct platform_device *dev);

static const struct of_device_id emictrl_of_ids[] = {
	{.compatible = "mediatek,common-emictrl",},
	{}
};

static struct platform_driver emictrl_drv = {
	.probe = emictrl_probe,
	.remove = emictrl_remove,
	.driver = {
		.name = "emi_ctrl",
		.owner = THIS_MODULE,
		.of_match_table = emictrl_of_ids,
	},
};

static int emictrl_probe(struct platform_device *pdev)
{
	struct device_node *emictrl_node = pdev->dev.of_node;
	struct device_node *emicen_node =
			of_parse_phandle(emictrl_node, "mediatek,emi-reg", 0);
	struct device_node *emichn_node =
			of_parse_phandle(emicen_node, "mediatek,emi-reg", 0);
	int i, ret;
	struct emi_cen *emi_cen;

	pr_info("%s: module probe.\n", __func__);

	// read emi_cen from emicen_node & emichn_node
	emi_cen = devm_kmalloc(&pdev->dev,
		sizeof(struct emi_cen), GFP_KERNEL);
	if (!emi_cen)
		return -ENOMEM;

	ret = of_property_read_u32(emicen_node,
		"ch_cnt", &(emi_cen->ch_cnt));
	if (ret) {
		pr_info("%s: get ch_cnt fail\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(emicen_node,
		"rk_cnt", &(emi_cen->rk_cnt));
	if (ret) {
		pr_info("%s: get rk_cnt fail\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: %s(%d), %s(%d)\n", __func__,
		"ch_cnt", emi_cen->ch_cnt,
		"rk_cnt", emi_cen->rk_cnt);

	emi_cen->rk_size = devm_kmalloc_array(&pdev->dev,
		emi_cen->rk_cnt, sizeof(unsigned long long),
		GFP_KERNEL);
	if (!(emi_cen->rk_size))
		return -ENOMEM;
	ret = of_property_read_u64_array(emicen_node,
		"rk_size", emi_cen->rk_size, emi_cen->rk_cnt);

	for (i = 0; i < emi_cen->rk_cnt; i++)
		pr_info("%s: rk_size%d(0x%llx)\n", __func__,
			i, emi_cen->rk_size[i]);

	ret = of_property_count_elems_of_size(
		emicen_node, "reg", sizeof(unsigned int) * 4);
	if (ret <= 0) {
		pr_info("%s: get emi_cen_cnt fail\n", __func__);
		return -EINVAL;
	}
	emi_cen->emi_cen_cnt = (unsigned int)ret;
	emi_cen->emi_cen_base = devm_kmalloc_array(&pdev->dev,
		emi_cen->emi_cen_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(emi_cen->emi_cen_base))
		return -ENOMEM;
	for (i = 0; i < emi_cen->emi_cen_cnt; i++)
		emi_cen->emi_cen_base[i] = of_iomap(emicen_node, i);

	ret = of_property_count_elems_of_size(
		emichn_node, "reg", sizeof(unsigned int) * 4);
	if (ret <= 0) {
		pr_info("%s: get emi_chn_cnt fail\n", __func__);
		return -EINVAL;
	}
	emi_cen->emi_chn_cnt = (unsigned int)ret;

	emi_cen->emi_chn_base = devm_kmalloc_array(&pdev->dev,
		emi_cen->emi_chn_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(emi_cen->emi_chn_base))
		return -ENOMEM;
	for (i = 0; i < emi_cen->emi_chn_cnt; i++)
		emi_cen->emi_chn_base[i] = of_iomap(emichn_node, i);

	global_emi_cen = emi_cen;

	pr_info("%s: %s(%d), %s(%d)\n", __func__,
		"emi_cen_cnt", emi_cen->emi_cen_cnt,
		"emi_chn_cnt", emi_cen->emi_chn_cnt);
	for (i = 0; i < emi_cen->emi_cen_cnt; i++)
		pr_info("%s: emi_cen_base[%d] = 0x%x\n", __func__,
			i, emi_cen->emi_cen_base[i]);
	for (i = 0; i < emi_cen->emi_chn_cnt; i++)
		pr_info("%s: emi_chn_base[%d] = 0x%x\n", __func__,
			i, emi_cen->emi_chn_base[i]);

	bwl_init(&emictrl_drv);
	return 0;
}


static int emictrl_remove(struct platform_device *dev)
{
	return 0;
}

static int __init emictrl_drv_init(void)
{
	int ret;

	// chip without related of_device_id info in .dts, return here.
	#if (CONFIG_MTK_EMI_BWL_VERSION == 0)
	pr_info("%s: Exit for CONFIG_MTK_EMI_BWL_VERSION not defined.\n", __func__);
	return 0;
	#endif

	ret = platform_driver_register(&emictrl_drv);
	if (ret) {
		pr_info("%s: init fail, ret 0x%x\n", __func__, ret);
		return ret;
	}

	return ret;
}

static void __exit emictrl_drv_exit(void)
{
	#if (CONFIG_MTK_EMI_BWL_VERSION == 0)
	pr_info("%s: Exit for CONFIG_MTK_EMI_BWL_VERSION not defined.\n", __func__);
	return;
	#endif

	platform_driver_unregister(&emictrl_drv);
}

module_init(emictrl_drv_init);
module_exit(emictrl_drv_exit);

MODULE_DESCRIPTION("MediaTek EMICTRL Driver v0.1");
MODULE_LICENSE("GPL v2");
