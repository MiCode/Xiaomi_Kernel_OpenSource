// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Sagy Shih <sagy.shih@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <memory/mediatek/emi.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <mt-plat/aee.h>
LIST_HEAD(mpucb_list);
static DEFINE_MUTEX(mpucb_mutex);

static struct emimpu_callbacks {
	struct list_head list;
	unsigned long owner;
	irqreturn_t (*debug_dump)(unsigned int emi_id, struct reg_info_t *dump, unsigned int len);
	bool handled;
};

static struct platform_device *emimpu_pdev;

static int emimpu_probe(struct platform_device *pdev);
static irqreturn_t (*pre_handling_cb)(
	unsigned int emi_id, struct reg_info_t *dump, unsigned int leng);
static void (*post_clear_cb)(unsigned int emi_id);
static void (*md_handling_cb)(
	unsigned int emi_id, struct reg_info_t *dump, unsigned int leng);

static unsigned int emimpu_read_protection(
	unsigned int reg_type, unsigned int region, unsigned int dgroup)
{
	struct arm_smccc_res smc_res;

	arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_READ,
		reg_type, region, dgroup, 0, 0, 0, &smc_res);
	return (unsigned int)smc_res.a0;
}
#ifdef MTK_EMIMPU_DBG_ENABLE
static ssize_t emimpu_ctrl_show(struct device_driver *driver, char *buf)
{
	struct emimpu_dev_t *emimpu_dev_ptr;
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

	if (!emimpu_pdev)
		return strlen(buf);

	emimpu_dev_ptr =
		(struct emimpu_dev_t *)platform_get_drvdata(emimpu_pdev);

	for (ret = 0, region = emimpu_dev_ptr->show_region;
		region < emimpu_dev_ptr->region_cnt; region++) {
		start = (unsigned long long)emimpu_read_protection(
			MTK_EMIMPU_READ_SA, region, 0);
		start = (start << (emimpu_dev_ptr->addr_align)) +
			emimpu_dev_ptr->dram_start;

		end = (unsigned long long)emimpu_read_protection(
			MTK_EMIMPU_READ_EA, region, 0);
		end = (end << (emimpu_dev_ptr->addr_align)) +
			emimpu_dev_ptr->dram_start;

		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"R%u-> 0x%llx to 0x%llx\n",
			region, start, end + 0xFFFF);
		if (ret >= PAGE_SIZE)
			return strlen(buf);

		for (i = 0; i < (emimpu_dev_ptr->domain_cnt / 8); i++) {
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
	struct emimpu_dev_t *emimpu_dev_ptr;
	char *command;
	char *backup_command;
	char *ptr;
	char *token[MTK_EMI_MAX_TOKEN];
	static struct emimpu_region_t *rg_info;
	unsigned long long start, end;
	unsigned long region;
	unsigned long dgroup;
	unsigned long apc;
	int i, j, ret;

	if (!emimpu_pdev)
		return count;

	emimpu_dev_ptr =
		(struct emimpu_dev_t *)platform_get_drvdata(emimpu_pdev);

	if (!(emimpu_dev_ptr->ctrl_intf))
		return count;

	if (!rg_info) {
		rg_info = kmalloc(sizeof(struct emimpu_region_t), GFP_KERNEL);
		if (!rg_info)
			return count;
		rg_info->apc = (unsigned int *)kmalloc_array(
			emimpu_dev_ptr->domain_cnt, sizeof(unsigned int),
			GFP_KERNEL);
		if (!(rg_info->apc)) {
			kfree(rg_info);
			rg_info = NULL;
			return count;
		}
		rg_info->lock = false;
	}

	if ((strlen(buf) + 1) > MTK_EMI_MAX_CMD_LEN) {
		pr_info("%s: store command overflow\n", __func__);
		return count;
	}

	pr_info("%s: store: %s\n", __func__, buf);

	command = kmalloc((size_t) MTK_EMI_MAX_CMD_LEN, GFP_KERNEL);
	if (!command)
		return count;
	backup_command = command;
	if (!command)
		return count;
	strncpy(command, buf, (size_t) MTK_EMI_MAX_CMD_LEN);

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

		if (region < emimpu_dev_ptr->region_cnt) {
			emimpu_dev_ptr->show_region = (unsigned int) region;
			pr_info("%s: show_region to %u\n",
				__func__, emimpu_dev_ptr->show_region);
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

		if (dgroup < (emimpu_dev_ptr->domain_cnt / 8)) {
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

		ret = kstrtoull(token[1], 16, &start);
		if (ret != 0)
			pr_info("%s: fail to parse start\n", __func__);
		ret = kstrtoull(token[2], 16, &end);
		if (ret != 0)
			pr_info("%s: fail to parse end\n", __func__);
		ret = kstrtoul(token[3], 10, &region);
		if (ret != 0)
			pr_info("%s: fail to parse region\n", __func__);

		if (region < emimpu_dev_ptr->region_cnt) {
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

		if (region < emimpu_dev_ptr->region_cnt) {
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
#endif
static void set_regs(
	struct reg_info_t *reg_list, unsigned int reg_cnt,
	void __iomem *emi_cen_base)
{
	unsigned int i, j;

	for (i = 0; i < reg_cnt; i++)
		for (j = 0; j < reg_list[i].leng; j++)
			writel(reg_list[i].value, emi_cen_base +
				reg_list[i].offset + 4 * j);
}

static void clear_violation(
	struct emimpu_dev_t *emimpu_dev_ptr, unsigned int emi_id)
{
	void __iomem *emi_cen_base;

	emi_cen_base = emimpu_dev_ptr->emi_cen_base[emi_id];

	set_regs(emimpu_dev_ptr->clear_reg,
		emimpu_dev_ptr->clear_reg_cnt, emi_cen_base);

	if (post_clear_cb)
		post_clear_cb(emi_id);
}

static irqreturn_t emimpu_violation_irq(int irq, void *dev_id)
{
	struct emimpu_dev_t *emimpu_dev_ptr =
		(struct emimpu_dev_t *)platform_get_drvdata(emimpu_pdev);
	struct reg_info_t *dump_reg = emimpu_dev_ptr->dump_reg;
	void __iomem *emi_cen_base;
	unsigned int emi_id;
	unsigned int i;
	bool violation, mpu_bypass;
	char aee_msg[MTK_EMI_MAX_CMD_LEN];
	ssize_t aee_msg_cnt;
	struct emimpu_callbacks *mpucb;

	aee_msg_cnt = snprintf(aee_msg, MTK_EMI_MAX_CMD_LEN, "violation\n");
	for (emi_id = 0; emi_id < emimpu_dev_ptr->emi_cen_cnt; emi_id++) {
		violation = false;
		emi_cen_base = emimpu_dev_ptr->emi_cen_base[emi_id];

		for (i = 0; i < emimpu_dev_ptr->dump_cnt; i++) {
			dump_reg[i].value = readl(
				emi_cen_base + dump_reg[i].offset);
			pr_info("%s: emi%d, offset(0x%x), value(0x%x)\n",
				__func__, emi_id,
				dump_reg[i].offset, dump_reg[i].value);

			if (aee_msg_cnt < MTK_EMI_MAX_CMD_LEN) {
				aee_msg_cnt += snprintf(aee_msg + aee_msg_cnt,
					MTK_EMI_MAX_CMD_LEN - aee_msg_cnt,
					"%s(%d),%s(%x),%s(%x);\n",
					"emi", emi_id,
					"off", dump_reg[i].offset,
					"val", dump_reg[i].value);
			}

			if (dump_reg[i].value)
				violation = true;
		}

		if (!violation)
			continue;

		if (pre_handling_cb)
			if (pre_handling_cb(emi_id,
				dump_reg, emimpu_dev_ptr->dump_cnt)
				== IRQ_HANDLED) {
				clear_violation(emimpu_dev_ptr, emi_id);
				mtk_clear_md_violation();
				continue;
			}

		mpu_bypass = false;
		list_for_each_entry_reverse(mpucb, &mpucb_list, list) {
			if (mpucb->debug_dump)
				if (mpucb->debug_dump(emi_id,
					dump_reg, emimpu_dev_ptr->dump_cnt)
					== IRQ_HANDLED) {
					mpucb->handled = true;
					mpu_bypass = true;
					clear_violation(emimpu_dev_ptr, emi_id);
					mtk_clear_md_violation();
					break;
				}
		}

		if (mpu_bypass)
			continue;

		if (md_handling_cb) {
			md_handling_cb(emi_id,
				dump_reg, emimpu_dev_ptr->dump_cnt);
		}

		pr_info("%s: violation at emi%d\n", __func__, emi_id);
		aee_kernel_exception("EMIMPU", aee_msg);
	}

	for (emi_id = 0; emi_id < emimpu_dev_ptr->emi_cen_cnt; emi_id++)
		clear_violation(emimpu_dev_ptr, emi_id);

	return IRQ_HANDLED;
}

static int emimpu_remove(struct platform_device *dev)
{
	return 0;
}

static const struct of_device_id emimpu_of_ids[] = {
	{.compatible = "mediatek,common-emimpu",},
	{}
};

static struct platform_driver emimpu_drv = {
	.probe = emimpu_probe,
	.remove = emimpu_remove,
	.driver = {
		.name = "emimpu_drv",
		.owner = THIS_MODULE,
		.of_match_table = emimpu_of_ids,
	},
};

static int emimpu_probe(struct platform_device *pdev)
{
	struct device_node *emimpu_node = pdev->dev.of_node;
	struct device_node *emicen_node =
		of_parse_phandle(emimpu_node, "mediatek,emi-reg", 0);
	struct emimpu_dev_t *emimpu_dev_ptr;
	struct emimpu_region_t *rg_info;
	struct arm_smccc_res smc_res;
	struct resource *res;
	unsigned int *dump_list;
	unsigned int emimpu_irq;
	unsigned int i;
	unsigned int *ap_apc;
	unsigned int ap_region;
	unsigned int slverr;
	int size;
	int ret;

	pr_info("%s: module probe.\n", __func__);
	emimpu_pdev = pdev;
	emimpu_dev_ptr = devm_kmalloc(&pdev->dev,
		sizeof(struct emimpu_dev_t), GFP_KERNEL);
	if (!emimpu_dev_ptr)
		return -ENOMEM;
	emimpu_dev_ptr->show_region = 0;

	ret = of_property_read_u32(emimpu_node,
		"region_cnt", &(emimpu_dev_ptr->region_cnt));
	if (ret) {
		pr_info("%s: get region_cnt fail\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(emimpu_node,
		"domain_cnt", &(emimpu_dev_ptr->domain_cnt));
	if (ret) {
		pr_info("%s: get domain_cnt fail\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(emimpu_node,
		"addr_align", &(emimpu_dev_ptr->addr_align));
	if (ret) {
		pr_info("%s: get addr_align fail\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u64(emimpu_node,
		"dram_start", &(emimpu_dev_ptr->dram_start));
	if (ret) {
		pr_info("%s: get dram_start fail\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u64(emimpu_node,
		"dram_start", &(emimpu_dev_ptr->dram_start));
	if (ret) {
		pr_info("%s: get dram_start fail\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u64(emimpu_node,
		"dram_end", &(emimpu_dev_ptr->dram_end));
	if (ret) {
		pr_info("%s: get dram_end fail\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(emimpu_node,
		"ctrl_intf", &(emimpu_dev_ptr->ctrl_intf));
	if (ret) {
		pr_info("%s: get ctrl_intf fail\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: %s(%d),%s(%d),%s(%d),%s(%llx),%s(%llx),%s(%d)\n",
		__func__,
		"region_cnt", emimpu_dev_ptr->region_cnt,
		"domain_cnt", emimpu_dev_ptr->domain_cnt,
		"addr_align", emimpu_dev_ptr->addr_align,
		"dram_start", emimpu_dev_ptr->dram_start,
		"dram_end", emimpu_dev_ptr->dram_end,
		"ctrl_intf", emimpu_dev_ptr->ctrl_intf);

	/* get dump regs */
	size = of_property_count_elems_of_size(emimpu_node,
		"dump", sizeof(char));
	if (size <= 0) {
		pr_info("%s: get dump size fail\n", __func__);
		return -EINVAL;
	}
	dump_list = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!dump_list)
		return -ENOMEM;
	size >>= 2;
	emimpu_dev_ptr->dump_cnt = size;
	ret = of_property_read_u32_array(emimpu_node, "dump",
		dump_list, size);
	if (ret) {
		pr_info("%s: get dump fail\n", __func__);
		return -EINVAL;
	}
	size *= sizeof(struct reg_info_t);
	emimpu_dev_ptr->dump_reg = devm_kmalloc(&pdev->dev,
		size, GFP_KERNEL);
	if (!(emimpu_dev_ptr->dump_reg))
		return -ENOMEM;
	for (i = 0; i < emimpu_dev_ptr->dump_cnt; i++) {
		emimpu_dev_ptr->dump_reg[i].offset = dump_list[i];
		emimpu_dev_ptr->dump_reg[i].value = 0;
		emimpu_dev_ptr->dump_reg[i].leng = 0;
	}
	devm_kfree(&pdev->dev, dump_list);

	/* get clear regs */
	size = of_property_count_elems_of_size(emimpu_node,
		"clear", sizeof(char));
	if (size <= 0) {
		pr_info("%s: get clear fail\n", __func__);
		return -EINVAL;
	}
	emimpu_dev_ptr->clear_reg = devm_kmalloc(&pdev->dev,
		size, GFP_KERNEL);
	if (!(emimpu_dev_ptr->clear_reg))
		return -ENOMEM;
	emimpu_dev_ptr->clear_reg_cnt = size / sizeof(struct reg_info_t);
	size >>= 2;
	ret = of_property_read_u32_array(emimpu_node, "clear",
		(unsigned int *)(emimpu_dev_ptr->clear_reg), size);
	if (ret) {
		pr_info("%s: get clear fail\n", __func__);
		return -EINVAL;
	}

	/* get clear_md regs */
	size = of_property_count_elems_of_size(emimpu_node,
		"clear_md", sizeof(char));
	if (size <= 0) {
		pr_info("%s: get clear_md size fail\n", __func__);
		return -EINVAL;
	}
	emimpu_dev_ptr->clear_md_reg = devm_kmalloc(&pdev->dev,
		size, GFP_KERNEL);
	if (!(emimpu_dev_ptr->clear_md_reg))
		return -ENOMEM;
	emimpu_dev_ptr->clear_md_reg_cnt = size / sizeof(struct reg_info_t);
	size >>= 2;
	ret = of_property_read_u32_array(emimpu_node, "clear_md",
		(unsigned int *)(emimpu_dev_ptr->clear_md_reg), size);
	if (ret) {
		pr_info("%s: get clear_md fail\n", __func__);
		return -EINVAL;
	}

	/* get EMI base addr */
	emimpu_dev_ptr->emi_cen_cnt = of_property_count_elems_of_size(
			emicen_node, "reg", sizeof(unsigned int) * 4);
	if (emimpu_dev_ptr->emi_cen_cnt <= 0) {
		pr_info("%s: get emi_cen_cnt fail\n", __func__);
		return -EINVAL;
	}
	emimpu_dev_ptr->emi_cen_base = devm_kmalloc_array(&pdev->dev,
		emimpu_dev_ptr->emi_cen_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(emimpu_dev_ptr->emi_cen_base))
		return -ENOMEM;
	emimpu_dev_ptr->emi_mpu_base = devm_kmalloc_array(&pdev->dev,
		emimpu_dev_ptr->emi_cen_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(emimpu_dev_ptr->emi_mpu_base))
		return -ENOMEM;
	for (i = 0; i < emimpu_dev_ptr->emi_cen_cnt; i++) {
		emimpu_dev_ptr->emi_cen_base[i] = of_iomap(emicen_node, i);
		if (IS_ERR(emimpu_dev_ptr->emi_cen_base[i])) {
			pr_info("%s: unable to map EMI%d CEN base\n",
				__func__, i);
			return -EINVAL;
		}

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		emimpu_dev_ptr->emi_mpu_base[i] =
			devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(emimpu_dev_ptr->emi_mpu_base[i])) {
			pr_info("%s: unable to map EMI%d MPU base\n",
				__func__, i);
			return -EINVAL;
		}
	}

	platform_set_drvdata(pdev, emimpu_dev_ptr);

	emimpu_irq = irq_of_parse_and_map(emimpu_node, 0);
	ret = request_irq(emimpu_irq, (irq_handler_t)emimpu_violation_irq,
		IRQF_TRIGGER_NONE, "emimpu", &emimpu_drv);
	if (ret) {
		pr_info("%s: fail to request irq\n", __func__);
		return -EINVAL;
	}

	/* enable AP region */
	ret = of_property_read_u32(emimpu_node, "ap_region", &ap_region);
	if (ret) {
		emimpu_dev_ptr->ap_rg_info = NULL;
		pr_info("%s: no ap_region\n", __func__);
	} else {
		size = sizeof(unsigned int) * emimpu_dev_ptr->domain_cnt;
		ap_apc = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
		if (!ap_apc)
			return -ENOMEM;
		ret = of_property_read_u32_array(emimpu_node, "ap_apc",
			(unsigned int *)ap_apc, emimpu_dev_ptr->domain_cnt);
		if (ret) {
			pr_info("%s: fail to get ap_apc\n", __func__);
			return -EINVAL;
		}

		emimpu_dev_ptr->ap_rg_info =
			kmalloc(sizeof(struct emimpu_region_t), GFP_KERNEL);
		if (!(emimpu_dev_ptr->ap_rg_info))
			return -ENOMEM;
		rg_info = emimpu_dev_ptr->ap_rg_info;
		mtk_emimpu_init_region(rg_info, ap_region);
		mtk_emimpu_set_addr(rg_info,
			emimpu_dev_ptr->dram_start, emimpu_dev_ptr->dram_end);
		for (i = 0; i < emimpu_dev_ptr->domain_cnt; i++)
			if (ap_apc[i] != MTK_EMIMPU_FORBIDDEN)
				mtk_emimpu_set_apc(rg_info, i, ap_apc[i]);
		mtk_emimpu_lock_region(rg_info, MTK_EMIMPU_LOCK);
		devm_kfree(&pdev->dev, ap_apc);
	}

	ret = of_property_read_u32(emimpu_node, "slverr", &slverr);
	if (!ret && slverr)
		for (i = 0; i < emimpu_dev_ptr->domain_cnt; i++)
			arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_SLVERR,
				i, 0, 0, 0, 0, 0, &smc_res);
#ifdef MTK_EMIMPU_DBG_ENABLE
	ret = driver_create_file(&emimpu_drv.driver,
		&driver_attr_emimpu_ctrl);
	if (ret)
		pr_info("%s: fail to create emimpu_ctrl\n", __func__);
#endif
	return ret;
}

static int __init emimpu_ap_region_init(void)
{
	struct emimpu_dev_t *emimpu_dev_ptr;

	if (!emimpu_pdev)
		return 0;

	pr_info("%s: enable AP region\n", __func__);

	emimpu_dev_ptr =
		(struct emimpu_dev_t *)platform_get_drvdata(emimpu_pdev);
	if (!(emimpu_dev_ptr->ap_rg_info))
		return 0;

	mtk_emimpu_set_protection(emimpu_dev_ptr->ap_rg_info);
	mtk_emimpu_free_region(emimpu_dev_ptr->ap_rg_info);

	kfree(emimpu_dev_ptr->ap_rg_info);
	emimpu_dev_ptr->ap_rg_info = NULL;

	return 0;
}

static int __init emimpu_drv_init(void)
{
	int ret;

	ret = platform_driver_register(&emimpu_drv);
	if (ret) {
		pr_info("%s: init fail, ret 0x%x\n", __func__, ret);
		return ret;
	}

	return ret;
}

static void __exit emimpu_drv_exit(void)
{
	platform_driver_unregister(&emimpu_drv);
}

late_initcall_sync(emimpu_ap_region_init);
module_init(emimpu_drv_init);
module_exit(emimpu_drv_exit);

/*
 * mtk_emimpu_init_region - init rg_info's apc data with default forbidden
 * @rg_info:	the target region for init
 * @rg_num:	the region id for the rg_info
 *
 * Returns 0 for success and 1 for abort
 */
int mtk_emimpu_init_region(
	struct emimpu_region_t *rg_info, unsigned int rg_num)
{
	struct emimpu_dev_t *emimpu_dev_ptr;
	unsigned int size;
	unsigned int i;

	if (!emimpu_pdev)
		return -1;

	emimpu_dev_ptr =
		(struct emimpu_dev_t *)platform_get_drvdata(emimpu_pdev);

	if (rg_num >= emimpu_dev_ptr->region_cnt) {
		pr_info("%s: fail, out-of-range region\n", __func__);
		return -1;
	}

	rg_info->start = 0;
	rg_info->end = 0;
	rg_info->rg_num = rg_num;
	rg_info->lock = false;

	size = sizeof(unsigned int) * emimpu_dev_ptr->domain_cnt;
	rg_info->apc = kmalloc(size, GFP_KERNEL);
	if (!(rg_info->apc))
		return -1;
	for (i = 0; i < emimpu_dev_ptr->domain_cnt; i++)
		rg_info->apc[i] = MTK_EMIMPU_FORBIDDEN;

	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_init_region);

/*
 * mtk_emi_mpu_free_region - free the apc data in rg_info
 * @rg_info:	the target region for free
 *
 * Returns 0 for success
 */
int mtk_emimpu_free_region(struct emimpu_region_t *rg_info)
{
	kfree(rg_info->apc);
	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_free_region);

/*
 * mtk_emimpu_set_addr - set the address space
 * @rg_info:	the target region for address setting
 * @start:	the start address
 * @end:	the end address
 *
 * Returns 0 for success
 */
int mtk_emimpu_set_addr(struct emimpu_region_t *rg_info,
	unsigned long long start, unsigned long long end)
{
	rg_info->start = start;
	rg_info->end = end;
	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_set_addr);

/*
 * mtk_emimpu_set_apc - set access permission for target domain
 * @rg_info:	the target region for apc setting
 * @d_num:	the target domain id
 * @apc:	the access permission setting
 *
 * Returns 0 for success
 */
int mtk_emimpu_set_apc(struct emimpu_region_t *rg_info,
	unsigned int d_num, unsigned int apc)
{
	struct emimpu_dev_t *emimpu_dev_ptr;

	if (!emimpu_pdev)
		return -1;

	emimpu_dev_ptr =
		(struct emimpu_dev_t *)platform_get_drvdata(emimpu_pdev);

	if (d_num >= emimpu_dev_ptr->domain_cnt) {
		pr_info("%s: fail, out-of-range domain\n", __func__);
		return -1;
	}

	rg_info->apc[d_num] = apc & 0x7;
	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_set_apc);

/*
 * mtk_emimpu_lock_region - set lock for target region
 * @rg_info:	the target region for lock
 * @lock:	enable/disable lock
 *
 * Returns 0 for success
 */
int mtk_emimpu_lock_region(struct emimpu_region_t *rg_info, bool lock)
{
	rg_info->lock = lock;
	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_lock_region);

/*
 * mtk_emimpu_set_protection - set emimpu protect into device
 * @rg_info:	the target region information
 *
 * Return 0 for success, -1 for fail
 */
int mtk_emimpu_set_protection(struct emimpu_region_t *rg_info)
{
	struct emimpu_dev_t *emimpu_dev_ptr;
	unsigned int start, end;
	unsigned int group_apc;
	unsigned int d_group;
	struct arm_smccc_res smc_res;
	int i, j;

	if (!emimpu_pdev)
		return -1;

	emimpu_dev_ptr =
		(struct emimpu_dev_t *)platform_get_drvdata(emimpu_pdev);
	d_group = emimpu_dev_ptr->domain_cnt / 8;

	if (!(rg_info->apc)) {
		pr_info("%s: fail, protect without init\n", __func__);
		return -1;
	}

	start = (unsigned int)
		(rg_info->start >> (emimpu_dev_ptr->addr_align)) |
		(rg_info->rg_num << 24);

	for (i = d_group - 1; i >= 0; i--) {
		end = (unsigned int)
			(rg_info->end >> (emimpu_dev_ptr->addr_align)) |
			(i << 24);

		for (group_apc = 0, j = 0; j < 8; j++)
			group_apc |=
				((rg_info->apc[i * 8 + j]) & 0x7) << (3 * j);
		if ((i == 0) && rg_info->lock)
			group_apc |= 0x80000000;

		arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_SET,
			start, end, group_apc, 0, 0, 0, &smc_res);
	}

	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_set_protection);

/*
 * mtk_emimpu_clear_protection - clear emimpu protection
 * @rg_info:	the target region information
 *
 * Return 0 for success, -1 for fail
 */
int mtk_emimpu_clear_protection(struct emimpu_region_t *rg_info)
{
	struct emimpu_dev_t *emimpu_dev_ptr;
	struct arm_smccc_res smc_res;

	if (!emimpu_pdev)
		return -1;

	emimpu_dev_ptr =
		(struct emimpu_dev_t *)platform_get_drvdata(emimpu_pdev);

	if (rg_info->rg_num > emimpu_dev_ptr->region_cnt) {
		pr_info("%s: region %u overflow\n", __func__, rg_info->rg_num);
		return -1;
	}

	arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_CLEAR,
		rg_info->rg_num, 0, 0, 0, 0, 0, &smc_res);

	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_clear_protection);

/*
 * mtk_emimpu_prehandle_register - register callback for irq prehandler
 * @bypass_func:	function point for prehandler
 *
 * Return 0 for success, -EINVAL for fail
 */
int mtk_emimpu_prehandle_register(
	irqreturn_t (*bypass_func)
	(unsigned int emi_id, struct reg_info_t *dump, unsigned int leng))
{
	if (!bypass_func) {
		pr_info("%s: bypass_func is NULL\n", __func__);
		return -EINVAL;
	}

	pre_handling_cb = bypass_func;
	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_prehandle_register);

/*
 * mtk_emimpu_postclear_register - register callback for clear posthandler
 * @clear_func:	function point for posthandler
 *
 * Return 0 for success, -EINVAL for fail
 */
int mtk_emimpu_postclear_register(void (*clear_func)(unsigned int emi_id))
{
	if (!clear_func) {
		pr_info("%s: clear_func is NULL\n", __func__);
		return -EINVAL;
	}

	post_clear_cb = clear_func;
	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_postclear_register);

/*
 * mtk_emimpu_md_handling_register - register callback for md handling
 * @md_handling_func:	function point for md handling
 *
 * Return 0 for success, -EINVAL for fail
 */
int mtk_emimpu_md_handling_register(
	void (*md_handling_func)
	(unsigned int emi_id, struct reg_info_t *dump, unsigned int leng))
{
	if (!md_handling_func) {
		pr_info("%s: md_handling_func is NULL\n", __func__);
		return -EINVAL;
	}

	md_handling_cb = md_handling_func;
	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_md_handling_register);

/*
 * mtk_emimpu_register_callback - register callback for debug handling
 * @mpucb:   function point for debug handling
 *
 * Return 0 for success, -EINVAL or -ENOMEN for fail
 *
 * This function can only be called in a non-atomic context since it may sleep
 */
int mtk_emimpu_register_callback(
	irqreturn_t (*debug_dump)
	(unsigned int emi_id, struct reg_info_t *dump, unsigned int len))
{
	struct emimpu_callbacks *mpucb;

	if (!debug_dump) {
		pr_info("%s: %p is NULL", __func__, __builtin_return_address(0));
		return -EINVAL;
	}

	mpucb = kmalloc(sizeof(struct emimpu_callbacks), GFP_KERNEL);
	if (!mpucb)
		return -ENOMEM;

	mpucb->owner = __builtin_return_address(0);
	mpucb->debug_dump = debug_dump;
	mpucb->handled = false;

	INIT_LIST_HEAD(&mpucb->list);

	mutex_lock(&mpucb_mutex);
	list_add(&mpucb->list, &mpucb_list);
	mutex_unlock(&mpucb_mutex);

	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_register_callback);
/*
 * mtk_clear_md_violation - clear irq for md violation
 *
 * No return
 */
void mtk_clear_md_violation(void)
{
	struct emimpu_dev_t *emimpu_dev_ptr;
	void __iomem *emi_cen_base;
	unsigned int emi_id;

	if (!emimpu_pdev)
		return;

	emimpu_dev_ptr =
		(struct emimpu_dev_t *)platform_get_drvdata(emimpu_pdev);

	for (emi_id = 0; emi_id < emimpu_dev_ptr->emi_cen_cnt; emi_id++) {
		emi_cen_base = emimpu_dev_ptr->emi_cen_base[emi_id];

		set_regs(emimpu_dev_ptr->clear_md_reg,
			emimpu_dev_ptr->clear_md_reg_cnt, emi_cen_base);
	}
}
EXPORT_SYMBOL(mtk_clear_md_violation);

MODULE_DESCRIPTION("MediaTek EMIMPU Driver v0.1");

