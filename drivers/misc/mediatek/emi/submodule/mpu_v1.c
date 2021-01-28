// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Sagy Shih <sagy.shih@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/printk.h>
#include <linux/memblock.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_ccci_common.h>
#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif

#include <emi.h>
#include <emi_io.h>
#include <mt_emi.h>
#include "mpu_v1.h"
#include <mpu_platform.h>

_Static_assert(EMI_MPU_DOMAIN_NUM <= 2048, "EMI_MPU_DOMAIN_NUM is over 2048");
_Static_assert(EMI_MPU_REGION_NUM <= 256, "EMI_MPU_REGION_NUM is over 256");

#if EMI_MPU_TEST
char mpu_test_buf[0x20000] __aligned(PAGE_SIZE);
#endif

static void __iomem *CEN_EMI_BASE;

static void (*check_violation_cb)(void);
static const char *UNKNOWN_MASTER = "unknown";
static unsigned int show_region;

#ifdef MPU_BYPASS
static unsigned int init_flag;
#endif

static unsigned int match_id(
	unsigned int axi_id, unsigned int tbl_idx, unsigned int port_id)
{
	if ((axi_id & mst_tbl[tbl_idx].id_mask) == mst_tbl[tbl_idx].id_val) {
		if (port_id == mst_tbl[tbl_idx].port)
			return 1;
	}

	return 0;
}

static const char *id2name(unsigned int axi_id, unsigned int port_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mst_tbl); i++) {
		if (match_id(axi_id, i, port_id))
			return mst_tbl[i].name;
	}

	return (char *)UNKNOWN_MASTER;
}

static unsigned int emi_mpu_read_protection(
	unsigned int reg_type, unsigned int region, unsigned int dgroup)
{
	struct arm_smccc_res smc_res;

	arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_READ,
		reg_type, region, dgroup, 0, 0, 0, &smc_res);
	return (unsigned int)smc_res.a0;
}

static void clear_violation(void)
{
	unsigned int mpus, mput, i;

	/* clear violation status */
	for (i = 0; i < EMI_MPU_DOMAIN_NUM; i++) {
		/* clear region abort violation */
		mt_reg_sync_writel(0xFFFFFFFF, EMI_MPUD_ST(i));
		/* clear out-of-range violation */
		mt_reg_sync_writel(0x3, EMI_MPUD_ST2(i));
	}

	/* clear debug info */
	mt_reg_sync_writel(0x80000000, EMI_MPUS);

	mpus = readl(IOMEM(EMI_MPUS));
	mput = readl(IOMEM(EMI_MPUT));

	if (mpus) {
		pr_info("[MPU] fail to clear violation\n");
		pr_info("[MPU] EMI_MPUS: %x, EMI_MPUT: %x\n", mpus, mput);
	}
}

static void check_violation(void)
{
	unsigned int mpus, mput, mput_2nd;
	unsigned int master_id, domain_id;
	unsigned int port_id, axi_id;
	unsigned int region;
	unsigned int wr_vio, wr_oo_vio;
	unsigned long long vio_addr;
	const char *master_name;

	mpus = readl(IOMEM(EMI_MPUS));
	mput = readl(IOMEM(EMI_MPUT));
	mput_2nd = readl(IOMEM(EMI_MPUT_2ND));
	vio_addr = ((((unsigned long long)(mput_2nd & 0xF)) << 32) + mput +
		DRAM_OFFSET);

	/* decode EMI_MPUS */
	master_id = mpus & 0xFFFF;
	domain_id = (mpus >> 21) & 0xF;
	region = (mpus >> 16) & 0x1F;
	wr_vio = (mpus >> 29) & 0x3;
	wr_oo_vio = (mpus >> 27) & 0x3;
	port_id = master_id & 0x7;
	axi_id = (master_id >> 3) & 0x1FFF;
	master_name = id2name(axi_id, port_id);

	pr_info("[MPU] EMI MPU violation\n");
	pr_info("[MPU] MPUS: %x, MPUT: %x, MPUT_2ND: %x.\n",
		mpus, mput, mput_2nd);
	pr_info("[MPU] current process is \"%s \" (pid: %i)\n",
		current->comm, current->pid);
	pr_info("[MPU] corrupted address is 0x%llx, in region %d\n",
		vio_addr, region);
	pr_info("[MPU] master ID: 0x%x, AXI ID: 0x%x, port ID: 0x%x\n",
		master_id, axi_id, port_id);
	pr_info("[MPU] violation master is %s, from domain 0x%x\n",
		master_name, domain_id);

	if (wr_vio == 1)
		pr_info("[MPU] write violation\n");
	else if (wr_vio == 2)
		pr_info("[MPU] read violation\n");
	else
		pr_info("[MPU] strange write/read violation (%d)\n", wr_vio);
	if (wr_oo_vio == 1)
		pr_info("[MPU] write out-of-range violation\n");
	else if (wr_oo_vio == 2)
		pr_info("[MPU] read out-of-range violation\n");

#ifdef MPU_BYPASS
	if (bypass_violation(mpus, &init_flag)) {
		pr_info("[MPU] bypass flow\n");
		clear_violation();
		clear_md_violation();
		return;
	}
#endif

#ifdef CONFIG_MTK_AEE_FEATURE
	if (wr_vio != 0) {
		if (is_md_master(master_id, domain_id)) {
			char str[CCCI_STR_MAX_LEN] = "0";

			if (snprintf(str, CCCI_STR_MAX_LEN,
				"EMI_MPUS = 0x%x, ADDR = 0x%llx",
				mpus, vio_addr) < 0) {
				pr_info("[MPU] CCCI string fail\n");
			}

#if CCCI_API_READY
			exec_ccci_kern_func_by_md_id(0, ID_MD_MPU_ASSERT,
				str, strlen(str));
#endif

			pr_info("[MPU] violation trigger MD, ");
			pr_info("str=%s strlen(str)=%d\n",
				str, (int)strlen(str));
		}

		aee_kernel_exception("EMI MPU",
			"%s%s = 0x%x,%s = 0x%x,%s = 0x%x,%s = 0x%llx\n%s%s\n",
			"EMI MPU violation.\n",
			"EMI_MPUS", mpus,
			"EMI_MPUT", mput,
			"EMI_MPUT_2ND", mput_2nd,
			"vio_addr", vio_addr,
			"CRDISPATCH_KEY:EMI MPU Violation Issue/",
			master_name);
	}
#endif

	clear_violation();
}

static irqreturn_t violation_irq(int irq, void *dev_id)
{
	check_violation_cb();
	return IRQ_HANDLED;
}

int emi_mpu_set_protection(struct emi_region_info_t *region_info)
{
	unsigned int start, end;
	struct arm_smccc_res smc_res;
	int i;

	if (region_info->region >= EMI_MPU_REGION_NUM) {
		pr_info("[MPU] can not support region %u\n",
			region_info->region);
		return -1;
	}

	start = (unsigned int)(region_info->start >> EMI_MPU_ALIGN_BITS) |
		(region_info->region << 24);

	for (i = EMI_MPU_DGROUP_NUM - 1; i >= 0; i--) {
		end = (unsigned int)(region_info->end >> EMI_MPU_ALIGN_BITS) |
			(i << 24);
		arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_SET,
			start, end, region_info->apc[i], 0, 0, 0, &smc_res);
	}

	return 0;
}
EXPORT_SYMBOL(emi_mpu_set_protection);

int emi_mpu_set_single_permission(unsigned int region,
				  unsigned int domain,
				  unsigned int permission)
{
	struct arm_smccc_res smc_res;
	unsigned int old_apc, new_apc;
	unsigned long long start, end;
	int i;

	if (region >= EMI_MPU_REGION_NUM) {
		pr_debug("[EMI] wrong region %d when calling %s\n",
		       region, __func__);
		return -1;
	}

	if (domain >= EMI_MPU_DOMAIN_NUM) {
		pr_debug("[EMI] wrong domain %d when calling %s\n",
		       domain, __func__);
		return -1;
	}

	for (i = 0; i < EMI_MPU_DGROUP_NUM; i++) {
		unsigned int index = domain % 8;

		if ((domain / 8) == i) {
			old_apc = emi_mpu_read_protection(
				MTK_EMIMPU_READ_APC, region, i);
			old_apc &= ~(0x7 << (3 * index));
			new_apc = old_apc | (permission << (3 * index));

			start = (unsigned long long)emi_mpu_read_protection(
				MTK_EMIMPU_READ_SA, region, 0) & 0xffffff;

			end = (unsigned long long)emi_mpu_read_protection(
				MTK_EMIMPU_READ_EA, region, 0) & 0xffffff;

			start = (start << EMI_MPU_ALIGN_BITS) + DRAM_OFFSET;
			start = start >> EMI_MPU_ALIGN_BITS;

			end = (end << EMI_MPU_ALIGN_BITS) + DRAM_OFFSET;
			end = end >> EMI_MPU_ALIGN_BITS;

			arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_SET,
				(region << 24) | start, (i << 24) | end,
				new_apc, 0, 0, 0, &smc_res);
		} else {
			pr_debug("[EMI] don't need to set apc\n");
			continue;
		}
	}

	return 0;
}
EXPORT_SYMBOL(emi_mpu_set_single_permission);

int emi_mpu_clear_protection(struct emi_region_info_t *region_info)
{
	struct arm_smccc_res smc_res;

	if (region_info->region > EMI_MPU_REGION_NUM) {
		pr_info("[MPU] can not support region %u\n",
			region_info->region);
		return -1;
	}

	arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_CLEAR,
		region_info->region, 0, 0, 0, 0, 0, &smc_res);

	return 0;
}

static ssize_t mpu_config_show(struct device_driver *driver, char *buf)
{
	ssize_t ret = 0;
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

#if EMI_MPU_TEST
	i = (*((unsigned int *)(mpu_test_buf + 0x10000)));
	pr_info("[MPU] trigger violation with read 0x%x\n", i);
#endif

	for (region = show_region; region < EMI_MPU_REGION_NUM; region++) {
		start = (unsigned long long)emi_mpu_read_protection(
			MTK_EMIMPU_READ_SA, region, 0);
		start = (start << EMI_MPU_ALIGN_BITS) + DRAM_OFFSET;

		end = (unsigned long long)emi_mpu_read_protection(
			MTK_EMIMPU_READ_EA, region, 0);
		end = (end << EMI_MPU_ALIGN_BITS) + DRAM_OFFSET;

		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"R%u-> 0x%llx to 0x%llx\n",
			region, start, end + 0xFFFF);
		if (ret >= PAGE_SIZE)
			return strlen(buf);

		for (i = 0; i < EMI_MPU_DGROUP_NUM; i++) {
			apc = emi_mpu_read_protection(
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

static ssize_t mpu_config_store
	(struct device_driver *driver, const char *buf, size_t count)
{
	char *command;
	char *backup_command;
	char *ptr;
	char *token[EMI_MPU_MAX_TOKEN];
	static struct emi_region_info_t region_info;
	unsigned long long start, end;
	unsigned long region;
	unsigned long dgroup;
	unsigned long apc;
	int i, ret;

	if ((strlen(buf) + 1) > EMI_MPU_MAX_CMD_LEN) {
		pr_info("[MPU] store command overflow\n");
		return count;
	}

	pr_info("[MPU] store: %s\n", buf);

	command = kmalloc((size_t) EMI_MPU_MAX_CMD_LEN, GFP_KERNEL);
	backup_command = command;
	if (!command)
		return count;
	strncpy(command, buf, (size_t) EMI_MPU_MAX_CMD_LEN);

	for (i = 0; i < EMI_MPU_MAX_TOKEN; i++) {
		ptr = strsep(&command, " ");
		if (ptr == NULL)
			break;
		token[i] = ptr;
	}

	if (!strncmp(buf, "SHOW", strlen("SHOW"))) {
		if (i < 2)
			goto mpu_store_end;

		pr_info("[MPU] %s %s\n", token[0], token[1]);

		ret = kstrtoul(token[1], 10, &region);
		if (ret != 0) {
			pr_info("[MPU] fail to parse region\n");
			goto mpu_store_end;
		}

		if (region < EMI_MPU_REGION_NUM) {
			show_region = (unsigned int) region;
			pr_info("[MPU] set show_region to %u\n", show_region);
		}
	} else if (!strncmp(buf, "SET", strlen("SET"))) {
		if (i < 3)
			goto mpu_store_end;

		pr_info("[MPU] %s %s %s\n", token[0], token[1], token[2]);

		ret = kstrtoul(token[1], 10, &dgroup);
		if (ret != 0) {
			pr_info("[MPU] fail to parse dgroup\n");
			goto mpu_store_end;
		}

		ret = kstrtoul(token[2], 16, &apc);
		if (ret != 0) {
			pr_info("[MPU] fail to parse apc\n");
			goto mpu_store_end;
		}

		if (dgroup < EMI_MPU_DGROUP_NUM) {
			region_info.apc[dgroup] = (unsigned int) apc;
			pr_info("[MPU] apc[%lu]: 0x%x\n",
				dgroup, region_info.apc[dgroup]);
		}
	} else if (!strncmp(buf, "ON", strlen("ON"))) {
		if (i < 4)
			goto mpu_store_end;

		pr_info("[MPU] %s %s %s %s\n",
			token[0], token[1], token[2], token[3]);

		ret = kstrtoull(token[1], 16, &start);
		if (ret != 0) {
			pr_info("[MPU] fail to parse start\n");
			goto mpu_store_end;
		}

		ret = kstrtoull(token[2], 16, &end);
		if (ret != 0) {
			pr_info("[MPU] fail to parse end\n");
			goto mpu_store_end;
		}

		ret = kstrtoul(token[3], 10, &region);
		if (ret != 0) {
			pr_info("[MPU] fail to parse region\n");
			goto mpu_store_end;
		}

		if (region < EMI_MPU_REGION_NUM) {
			region_info.start = start;
			region_info.end = end;
			region_info.region = (unsigned int)region;
			emi_mpu_set_protection(&region_info);
		}
	} else if (!strncmp(buf, "OFF", strlen("OFF"))) {
		if (i < 2)
			goto mpu_store_end;

		pr_info("[MPU] %s %s\n", token[0], token[1]);

		ret = kstrtoul(token[1], 10, &region);
		if (ret != 0) {
			pr_info("[MPU] fail to parse region\n");
			goto mpu_store_end;
		}

		if (region < EMI_MPU_REGION_NUM) {
			region_info.region = (unsigned int)region;
			emi_mpu_clear_protection(&region_info);
		}
	} else
		pr_info("[MPU] unknown store command\n");

mpu_store_end:
	kfree(backup_command);

	return count;
}

static DRIVER_ATTR_RW(mpu_config);

#if ENABLE_AP_REGION
static void protect_ap_region(void)
{
	struct emi_region_info_t region_info;

	region_info.start = (unsigned long long)memblock_start_of_DRAM();
	region_info.end = (unsigned long long)memblock_end_of_DRAM() - 1;
	region_info.region = AP_REGION_ID;
	set_ap_region_permission(region_info.apc);

	emi_mpu_set_protection(&region_info);
}
#endif

#ifdef ENABLE_MPU_SLVERR
static void enable_slverr(void)
{
	struct arm_smccc_res smc_res;
	unsigned int domain;

	for (domain = 0; domain < EMI_MPU_DOMAIN_NUM; domain++) {
		arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_SLVERR,
			i, 0, 0, 0, 0, 0, &smc_res);
	}
}
#endif

void mpu_init(struct platform_driver *emi_ctrl, struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	unsigned int mpu_irq;
	int ret;

#if EMI_MPU_TEST
	unsigned int *ptr_test_buf;

	ptr_test_buf = (unsigned int *)__pa(mpu_test_buf);
	pr_info("[MPU] mpu_test_buf: %p\n", ptr_test_buf);
	*((unsigned int *)(mpu_test_buf + 0x10000)) = 0xDEADDEAD;
#endif

	pr_info("[MPU] initialize EMI MPU\n");

	CEN_EMI_BASE = mt_cen_emi_base_get();

#ifdef MPU_BYPASS
	bypass_init(&init_flag);
#endif

	if (!check_violation_cb)
		check_violation_cb = check_violation;
	if (readl(IOMEM(EMI_MPUS))) {
		pr_info("[MPU] detect violation in driver init\n");
		check_violation_cb();
	} else
		clear_violation();

	if (node) {
		mpu_irq = irq_of_parse_and_map(node, MPU_IRQ_INDEX);
		pr_info("[MPU] get MPU IRQ: %d\n", mpu_irq);

		ret = request_irq(mpu_irq, (irq_handler_t)violation_irq,
			IRQF_TRIGGER_NONE, "mpu", emi_ctrl);
		if (ret != 0) {
			pr_info("[MPU] fail to request IRQ (%d)\n", ret);
			return;
		}
	}

#if ENABLE_AP_REGION
	protect_ap_region();
#endif

#ifdef ENABLE_MPU_SLVERR
	enable_slverr();
#endif

#if !defined(USER_BUILD_KERNEL)
	ret = driver_create_file(&emi_ctrl->driver, &driver_attr_mpu_config);
	if (ret)
		pr_info("[MPU] fail to create mpu_config\n");
#endif
}

int emi_mpu_check_register(void (*cb_func)(void))
{
	if (!cb_func) {
		pr_info("%s%d: cb_func is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	check_violation_cb = cb_func;
	return 0;
}
EXPORT_SYMBOL(emi_mpu_check_register);

void clear_md_violation(void)
{
	mt_reg_sync_writel(0x80000000, EMI_MPUT_2ND);
}
EXPORT_SYMBOL(clear_md_violation);
