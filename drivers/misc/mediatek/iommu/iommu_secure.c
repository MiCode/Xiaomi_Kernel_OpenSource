// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#define pr_fmt(fmt)    "mtk_iommu: secure " fmt

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/export.h>
#if 0 //IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#endif
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/arm-smccc.h>
#include "mtk_iommu.h"
#include "iommu_secure.h"

/*
 * IOMMU TF-A SMC cmd format:
 * iommu_type[31:24] + iommu_id[23:16] + iommu_bank[15:8] + cmd_id[7:0]
 */
#define IOMMU_ATF_SET_CMD(iommu_type, iommu_id, iommu_bank, cmd) \
	((cmd) | (iommu_bank << 8) | (iommu_id << 16) | (iommu_type << 24))

#define TYPE_IGNORE				MM_IOMMU
#define ID_IGNORE				DISP_IOMMU
#define BANK_IGNORE				IOMMU_BK0

enum iommu_copy_type {
	PROTECT_TYPE,
	SECURE_TYPE,
	COPY_TYPE_NUM
};

enum iommu_atf_cmd {
	SECURE_BANK_INIT,
	SECURE_BANK_IRQ_EN,
	SECURE_BANK_BACKUP,
	SECURE_BANK_RESTORE,
	SECURITY_DBG_SWITCH,
	IOMMU_TF_DUMP,
	/* test cmd */
#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
	DUMP_PGTABLE_SEC,
	DUMP_BANK_BASE,
	DUMP_BANK_VAL,
#endif
	COPY_ENTRY_TO_SECURE,
	CLEAN_SECURE_ENTRY,
	DUMP_SECURE_ENTRY,
	CMD_NUM
};

static int mtk_iommu_hw_is_valid(uint32_t type, uint32_t id, uint32_t bank)
{
	if (bank >= IOMMU_BK_NUM) {
		pr_err("%s BANK id is invalid, %u\n", __func__, bank);
		return SMC_IOMMU_FAIL;
	}

	switch (type) {
		case MM_IOMMU:
			if (id >= MM_IOMMU_NUM) {
				pr_err("%s MM_IOMMU id is invalid, %u\n", __func__, id);
				return SMC_IOMMU_FAIL;
			}
			break;
		case APU_IOMMU:
			if (id >= APU_IOMMU_NUM) {
				pr_err("%s APU_IOMMU id is invalid, %u\n", __func__, id);
				return SMC_IOMMU_FAIL;
			}
			break;
		case PERI_IOMMU:
			if (id >= PERI_IOMMU_NUM) {
				pr_err("%s PERI_IOMMU id is invalid, %u\n", __func__, id);
				return SMC_IOMMU_FAIL;
			}
			break;
		default:
			pr_err("%s IOMMU TYPE is invalid, type:%u\n", __func__, type);
			return SMC_IOMMU_FAIL;

	}

	return SMC_IOMMU_SUCCESS;
}

static int mtk_iommu_dump_sec_bank(unsigned long cmd, unsigned long in2,
			unsigned long in3, unsigned long in4, unsigned long in5,
			unsigned long in6, unsigned long in7, u32 *out1,
			u32 *out2, u32 *out3)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_IOMMU_SECURE_CONTROL, cmd, in2, in3, in4, in5, in6, in7, &res);
	*out1 = (u32)res.a1;
	*out2 = (u32)res.a2;
	*out3 = (u32)res.a3;

	return res.a0;
}

/*
 * a0/in0 = MTK_IOMMU_SECURE_CONTROL(IOMMU SMC ID)
 * a1/in1 = cmd(type + id + bank + cmd)
 * a2/in2 ~ a7/in7: user defined
 */
static int mtk_iommu_atf_call(uint32_t type, uint32_t id, uint32_t bank,
			unsigned long cmd, unsigned long in2, unsigned long in3,
			unsigned long in4, unsigned long in5, unsigned long in6,
			unsigned long in7)
{
	int ret;
	struct arm_smccc_res res;

	ret = mtk_iommu_hw_is_valid(type, id, bank);
	if (ret) {
		pr_err("%s, IOMMU HW type is invalid, type:%u, id:%u\n",
		       __func__, type, id);
		return SMC_IOMMU_FAIL;
	}
	arm_smccc_smc(MTK_IOMMU_SECURE_CONTROL, cmd, in2, in3, in4, in5, in6, in7, &res);

	return res.a0;
}

int mtk_iommu_sec_bk_init_by_atf(uint32_t type, uint32_t id)
{
	int ret;
	unsigned long cmd = IOMMU_ATF_SET_CMD(type, id, IOMMU_BK4, SECURE_BANK_INIT);

	ret = mtk_iommu_atf_call(type, id, IOMMU_BK4, cmd, 0, 0, 0, 0, 0, 0);
	if (ret) {
		pr_err("%s, iommu call is fail, type:%u, id:%u, cmd:0x%lx\n",
			__func__, type, id, cmd);
		return SMC_IOMMU_FAIL;
	}

	return SMC_IOMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_iommu_sec_bk_init_by_atf);

int mtk_iommu_sec_bk_irq_en_by_atf(uint32_t type, uint32_t id,
				unsigned long en)
{
	int ret;
	unsigned long cmd = IOMMU_ATF_SET_CMD(type, id, IOMMU_BK4,
			SECURE_BANK_IRQ_EN);

	if (en != 0 && en != 1) {
		pr_info("%s fail, enable is invalid, en:%lu\n", __func__, en);
		return SMC_IOMMU_FAIL;
	}

	ret = mtk_iommu_atf_call(type, id, IOMMU_BK4, cmd, en, 0, 0, 0, 0, 0);
	if (ret) {
		pr_err("%s, iommu call is fail, type:%u, id:%u, cmd:0x%lx, en:%lu\n",
			__func__, type, id, cmd, en);
		return SMC_IOMMU_FAIL;
	}

	return SMC_IOMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_iommu_sec_bk_irq_en_by_atf);

int mtk_iommu_secure_bk_backup_by_atf(uint32_t type, uint32_t id)
{
	int ret;
	unsigned long cmd = IOMMU_ATF_SET_CMD(type, id, IOMMU_BK4, SECURE_BANK_BACKUP);

	ret = mtk_iommu_atf_call(type, id, IOMMU_BK4, cmd, 0, 0, 0, 0, 0, 0);
	if (ret && ret != SMC_IOMMU_NONSUPPORT) {
		pr_err("%s, iommu call is fail, type:%u, id:%u, cmd:0x%lx\n",
			__func__, type, id, cmd);
		return SMC_IOMMU_FAIL;
	}

	return SMC_IOMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_iommu_secure_bk_backup_by_atf);

int mtk_iommu_secure_bk_restore_by_atf(uint32_t type, uint32_t id)
{
	int ret;
	unsigned long cmd = IOMMU_ATF_SET_CMD(type, id, IOMMU_BK4, SECURE_BANK_RESTORE);

	ret = mtk_iommu_atf_call(type, id, IOMMU_BK4, cmd, 0, 0, 0, 0, 0, 0);
	if (ret && ret != SMC_IOMMU_NONSUPPORT) {
		pr_err("%s, iommu call is fail, type:%u, id:%u, cmd:0x%lx\n",
			__func__, type, id, cmd);
		return SMC_IOMMU_FAIL;
	}

	return SMC_IOMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_iommu_secure_bk_restore_by_atf);

int ao_secure_dbg_switch_by_atf(uint32_t type,
				uint32_t id, unsigned long en)
{
	int ret;
	unsigned long cmd = IOMMU_ATF_SET_CMD(type, id, BANK_IGNORE, SECURITY_DBG_SWITCH);

	if (en != 0 && en != 1) {
		pr_info("%s fail, enable is invalid, en:%lu\n", __func__, en);
		return SMC_IOMMU_FAIL;
	}

	ret = mtk_iommu_atf_call(type, id, BANK_IGNORE, cmd, en, 0, 0, 0, 0, 0);
	if (ret) {
		pr_err("%s, iommu call is fail, type:%u, id:%u, cmd:0x%lx, en:%lu\n",
			__func__, type, id, cmd, en);
		return SMC_IOMMU_FAIL;
	}

	return SMC_IOMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(ao_secure_dbg_switch_by_atf);

mtk_iommu_secure_bk_tf_dump(uint32_t type, uint32_t id, uint32_t bank,
		u32 *iova, u32 *pa, u32 *fault_id)
{
	int ret;
	unsigned long cmd = IOMMU_ATF_SET_CMD(type, id, bank, IOMMU_TF_DUMP);

	ret = mtk_iommu_hw_is_valid(type, id, bank);
	if (ret) {
		pr_err("%s, IOMMU HW type is invalid, type:%u, id:%u, bk:%u\n",
		       __func__, type, id, bank);
		return SMC_IOMMU_FAIL;
	}

	ret = mtk_iommu_dump_sec_bank(cmd, 0, 0, 0, 0, 0, 0, iova, pa, fault_id);
	if (ret) {
		pr_err("%s, iommu call is fail, type:%u, id:%u, bk:%u, cmd:0x%lx\n",
			__func__, type, id, bank, cmd);
		return SMC_IOMMU_FAIL;
	}

	return SMC_IOMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_iommu_secure_bk_tf_dump);

int mtk_iommu_copy_to_secure_entry(uint32_t type, uint32_t id, dma_addr_t iova, size_t size)
{
	int ret;
	unsigned long cmd = IOMMU_ATF_SET_CMD(type, id, BANK_IGNORE, COPY_ENTRY_TO_SECURE);

	ret = mtk_iommu_atf_call(type, id, BANK_IGNORE, cmd, iova, size, 0, 0, 0, 0);
	if (ret) {
		pr_err("%s fail, iova:%pa, sz:0x%zx, cmd:0x%lx\n",
		       __func__, &iova, size, cmd);
		return SMC_IOMMU_FAIL;
	}

	return SMC_IOMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_iommu_copy_to_secure_entry);

int mtk_iommu_clean_secure_entry(uint32_t type, uint32_t id, dma_addr_t iova, size_t size)
{
	int ret;
	unsigned long cmd = IOMMU_ATF_SET_CMD(type, id, BANK_IGNORE, CLEAN_SECURE_ENTRY);

	ret = mtk_iommu_atf_call(type, id, BANK_IGNORE, cmd, iova, size, 0, 0, 0, 0);
	if (ret) {
		pr_err("%s fail, iova:%pa, sz:0x%zx, cmd:0x%lx\n",
		       __func__, &iova, size, cmd);
		return SMC_IOMMU_FAIL;
	}

	return SMC_IOMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_iommu_clean_secure_entry);

int mtk_iommu_dump_secure_entry(uint32_t type, uint32_t id, dma_addr_t iova, size_t size)
{
	int ret;
	unsigned long cmd = IOMMU_ATF_SET_CMD(type, id, BANK_IGNORE, DUMP_SECURE_ENTRY);

	ret = mtk_iommu_atf_call(type, id, BANK_IGNORE, cmd, iova, size, 0, 0, 0, 0);
	if (ret) {
		pr_err("%s fail, iova:%pa, sz:0x%zx, cmd:0x%lx\n",
		       __func__, &iova, size, cmd);
		return SMC_IOMMU_FAIL;
	}

	return SMC_IOMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_iommu_dump_secure_entry);

#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
void mtk_iommu_dump_bank_base(void)
{
	int i, j, ret;
	unsigned long cmd;

	for (i = (int)DISP_IOMMU; i < (int)MM_IOMMU_NUM; i++) {
		for (j = IOMMU_BK0; j < IOMMU_BK_NUM; j++) {
			cmd = IOMMU_ATF_SET_CMD(MM_IOMMU,
						i, j, DUMP_BANK_BASE);

			ret = mtk_iommu_atf_call(MM_IOMMU, i, j, cmd, 0, 0, 0, 0, 0, 0);
			if (ret)
				pr_warn("%s fail, type:%s, id:%d, cmd:0x%lx\n",
					__func__, "mm_iommu", i, cmd);
		}
	}
	for (i = (int)APU_IOMMU0; i < (int)APU_IOMMU_NUM; i++) {
		for (j = IOMMU_BK0; j < IOMMU_BK_NUM; j++) {
			cmd = IOMMU_ATF_SET_CMD(APU_IOMMU,
						i, j, DUMP_BANK_BASE);

			ret = mtk_iommu_atf_call(APU_IOMMU, i, j, cmd, 0, 0, 0, 0, 0, 0);
			if (ret)
				pr_warn("%s fail, type:%s, id:%d, cmd:0x%lx\n",
					__func__, "apu_iommu", i, cmd);
		}

	}

	for (i = (int)PERI_IOMMU_M4; i < (int)PERI_IOMMU_NUM; i++) {
		for (j = IOMMU_BK0; j < IOMMU_BK_NUM; j++) {
			cmd = IOMMU_ATF_SET_CMD(PERI_IOMMU,
						i, j, DUMP_BANK_BASE);

			ret = mtk_iommu_atf_call(PERI_IOMMU, i, j, cmd, 0, 0, 0, 0, 0, 0);
			if (ret)
				pr_warn("%s fail, type:%s, id:%d, cmd:0x%lx\n",
					__func__, "peri_iommu", i, cmd);
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_iommu_dump_bank_base);

int mtk_iommu_dump_bk0_val(uint32_t type, uint32_t id)
{
	int ret;
	unsigned long cmd = IOMMU_ATF_SET_CMD(type, id, IOMMU_BK0, DUMP_BANK_VAL);

	ret = mtk_iommu_atf_call(type, id, IOMMU_BK0, cmd, 0, 0, 0, 0, 0, 0);
	if (ret) {
		pr_err("%s, iommu call is fail, type:%u, id:%u, cmd:0x%lx\n",
			__func__, type, id, cmd);
		return SMC_IOMMU_FAIL;
	}

	return SMC_IOMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_iommu_dump_bk0_val);

int mtk_iommu_sec_bk_pgtable_dump(uint32_t type, uint32_t id, uint32_t bank,
		u64 iova)
{
	int ret;
	unsigned long cmd = IOMMU_ATF_SET_CMD(type, id, bank, DUMP_PGTABLE_SEC);

	ret = mtk_iommu_hw_is_valid(type, id, bank);
	if (ret) {
		pr_err("%s, IOMMU HW type is invalid, type:%u, id:%u, bk:%u\n",
		       __func__, type, id, bank);
		return SMC_IOMMU_FAIL;
	}

	ret = mtk_iommu_atf_call(type, id, bank, cmd, iova, 0, 0, 0, 0, 0);
	if (ret) {
		pr_err("%s, iommu call is fail, type:%u, id:%u, bk:%u, cmd:0x%lx\n",
			__func__, type, id, bank, cmd);
		return SMC_IOMMU_FAIL;
	}

	return SMC_IOMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_iommu_sec_bk_pgtable_dump);
#endif

#if IS_ENABLED(CONFIG_MTK_ENABLE_GENIEZONE)
#define IOMMU_PSEUDO_DT_NAME	"mtk_iommu_pseudo"

int iommu_on_mtee = -1;
bool is_iommu_sec_on_mtee(void)
{
	struct device_node *iommu_pseudo_node;

	if (iommu_on_mtee != -1)
		return (iommu_on_mtee == 1);

	iommu_pseudo_node = of_find_node_by_name(NULL, IOMMU_PSEUDO_DT_NAME);
	if (!iommu_pseudo_node) {
		pr_info("%s, iommu_pseudo node not found\n", __func__);
		iommu_on_mtee = 0;
		return false;
	}
	of_node_put(iommu_pseudo_node);

	iommu_on_mtee = 1;

	return true;
}
EXPORT_SYMBOL_GPL(is_iommu_sec_on_mtee);
#else
bool is_iommu_sec_on_mtee(void)
{
	return false;
}
EXPORT_SYMBOL_GPL(is_iommu_sec_on_mtee);
#endif

static int mtk_iommu_sec_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_info("%s done, dev:%s\n", __func__, dev_name(dev));
	return 0;
}

static const struct of_device_id mtk_iommu_bank_of_ids[] = {
	{ .compatible = "mediatek,common-disp-iommu-bank1"},
	{ .compatible = "mediatek,common-disp-iommu-bank2"},
	{ .compatible = "mediatek,common-disp-iommu-bank3"},
	{ .compatible = "mediatek,common-disp-iommu-bank4"},
	{ .compatible = "mediatek,common-mdp-iommu-bank1"},
	{ .compatible = "mediatek,common-mdp-iommu-bank2"},
	{ .compatible = "mediatek,common-mdp-iommu-bank3"},
	{ .compatible = "mediatek,common-mdp-iommu-bank4"},
	{ .compatible = "mediatek,common-apu-iommu0-bank1"},
	{ .compatible = "mediatek,common-apu-iommu0-bank2"},
	{ .compatible = "mediatek,common-apu-iommu0-bank3"},
	{ .compatible = "mediatek,common-apu-iommu0-bank4"},
	{ .compatible = "mediatek,common-apu-iommu1-bank1"},
	{ .compatible = "mediatek,common-apu-iommu1-bank2"},
	{ .compatible = "mediatek,common-apu-iommu1-bank3"},
	{ .compatible = "mediatek,common-apu-iommu1-bank4"},
	{ .compatible = "mediatek,common-peri-iommu-m4-bank1"},
	{ .compatible = "mediatek,common-peri-iommu-m4-bank2"},
	{ .compatible = "mediatek,common-peri-iommu-m4-bank3"},
	{ .compatible = "mediatek,common-peri-iommu-m4-bank4"},
	{ .compatible = "mediatek,common-peri-iommu-m6-bank1"},
	{ .compatible = "mediatek,common-peri-iommu-m6-bank2"},
	{ .compatible = "mediatek,common-peri-iommu-m6-bank3"},
	{ .compatible = "mediatek,common-peri-iommu-m6-bank4"},
	{ .compatible = "mediatek,common-peri-iommu-m7-bank1"},
	{ .compatible = "mediatek,common-peri-iommu-m7-bank2"},
	{ .compatible = "mediatek,common-peri-iommu-m7-bank3"},
	{ .compatible = "mediatek,common-peri-iommu-m7-bank4"},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_iommu_bank_of_ids);

static struct platform_driver disp_iommu_bank1_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "disp-iommu-bank1",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver disp_iommu_bank2_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "disp-iommu-bank2",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver disp_iommu_bank3_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "disp-iommu-bank3",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver disp_iommu_bank4_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "disp-iommu-bank4",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver mdp_iommu_bank1_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "mdp-iommu-bank1",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver mdp_iommu_bank2_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "mdp-iommu-bank2",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver mdp_iommu_bank3_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "mdp-iommu-bank3",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver mdp_iommu_bank4_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "mdp-iommu-bank4",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver apu_iommu0_bank1_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "apu-iommu0-bank1",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver apu_iommu0_bank2_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "apu-iommu0-bank2",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver apu_iommu0_bank3_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "apu-iommu0-bank3",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver apu_iommu0_bank4_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "apu-iommu0-bank4",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver apu_iommu1_bank1_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "apu-iommu1-bank1",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver apu_iommu1_bank2_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "apu-iommu1-bank2",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver apu_iommu1_bank3_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "apu-iommu1-bank3",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver apu_iommu1_bank4_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "apu-iommu1-bank4",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m4_bank1_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m4-bank1",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m4_bank2_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m4-bank2",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m4_bank3_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m4-bank3",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m4_bank4_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m4-bank4",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m6_bank1_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m6-bank1",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m6_bank2_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m6-bank2",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m6_bank3_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m6-bank3",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m6_bank4_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m6-bank4",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m7_bank1_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m7-bank1",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m7_bank2_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m7-bank2",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m7_bank3_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m7-bank3",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver peri_iommu_m7_bank4_driver = {
	.probe	= mtk_iommu_sec_probe,
	.driver	= {
		.name = "peri-iommu-m7-bank4",
		.of_match_table = of_match_ptr(mtk_iommu_bank_of_ids),
	}
};

static struct platform_driver *const mtk_iommu_bk_drivers[] = {
	&disp_iommu_bank1_driver,
	&disp_iommu_bank2_driver,
	&disp_iommu_bank3_driver,
	&disp_iommu_bank4_driver,
	&mdp_iommu_bank1_driver,
	&mdp_iommu_bank2_driver,
	&mdp_iommu_bank3_driver,
	&mdp_iommu_bank4_driver,
	&apu_iommu0_bank1_driver,
	&apu_iommu0_bank2_driver,
	&apu_iommu0_bank3_driver,
	&apu_iommu0_bank4_driver,
	&apu_iommu1_bank1_driver,
	&apu_iommu1_bank2_driver,
	&apu_iommu1_bank3_driver,
	&apu_iommu1_bank4_driver,
	&peri_iommu_m4_bank1_driver,
	&peri_iommu_m4_bank2_driver,
	&peri_iommu_m4_bank3_driver,
	&peri_iommu_m4_bank4_driver,
	&peri_iommu_m6_bank1_driver,
	&peri_iommu_m6_bank2_driver,
	&peri_iommu_m6_bank3_driver,
	&peri_iommu_m6_bank4_driver,
	&peri_iommu_m7_bank1_driver,
	&peri_iommu_m7_bank2_driver,
	&peri_iommu_m7_bank3_driver,
	&peri_iommu_m7_bank4_driver,
};

static int __init mtk_iommu_sec_init(void)
{
	int ret;
	int i;

	pr_info("%s+\n", __func__);
	for (i = 0; i < ARRAY_SIZE(mtk_iommu_bk_drivers); i++) {
		ret = platform_driver_register(mtk_iommu_bk_drivers[i]);
		if (ret < 0) {
			pr_err("Failed to register %s driver: %d\n",
				  mtk_iommu_bk_drivers[i]->driver.name, ret);
			goto err;
		}
	}
	pr_info("%s-\n", __func__);

	return 0;

err:
	while (--i >= 0)
		platform_driver_unregister(mtk_iommu_bk_drivers[i]);

	return ret;
}

static void __exit mtk_iommu_sec_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(mtk_iommu_bk_drivers) - 1; i >= 0; i--)
		platform_driver_unregister(mtk_iommu_bk_drivers[i]);
}

module_init(mtk_iommu_sec_init);
module_exit(mtk_iommu_sec_exit);
MODULE_LICENSE("GPL v2");
