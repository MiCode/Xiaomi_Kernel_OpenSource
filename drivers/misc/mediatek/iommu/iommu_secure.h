/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#ifndef IOMMU_SECURE_H
#define IOMMU_SECURE_H

#define SMC_IOMMU_SUCCESS			(0)
#define SMC_IOMMU_FAIL				(1)
#define SMC_IOMMU_NONSUPPORT			(-1)

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
int mtk_iommu_sec_bk_init_by_atf(uint32_t type, uint32_t id);
int mtk_iommu_sec_bk_irq_en_by_atf(uint32_t type, uint32_t id, unsigned long en);
int mtk_iommu_secure_bk_backup_by_atf(uint32_t type, uint32_t id);
int mtk_iommu_secure_bk_restore_by_atf(uint32_t type, uint32_t id);
int mtk_iommu_secure_bk_tf_dump(uint32_t type, uint32_t id, uint32_t bank,
		u32 *iova, u32 *pa, u32 *fault_id);
int ao_secure_dbg_switch_by_atf(uint32_t type, uint32_t id, unsigned long en);
int mtk_iommu_copy_to_secure_entry(uint32_t type, uint32_t id, dma_addr_t iova, size_t size);
int mtk_iommu_dump_secure_entry(uint32_t type, uint32_t id, dma_addr_t iova, size_t size);
int mtk_iommu_clean_secure_entry(uint32_t type, uint32_t id, dma_addr_t iova, size_t size);
/* test cmd */
#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
void mtk_iommu_dump_bank_base(void);
int mtk_iommu_dump_bk0_val(uint32_t type, uint32_t id);
int mtk_iommu_sec_bk_pgtable_dump(uint32_t type, uint32_t id, uint32_t bank,
		u64 iova);
#endif
bool is_iommu_sec_on_mtee(void);
#else
int mtk_iommu_sec_bk_init_by_atf(uint32_t type, uint32_t id)
{
	pr_warn("mtk_iommu: secure warning, %s is not support\n", __func__);

	return 0;
}

int mtk_iommu_sec_bk_irq_en_by_atf(uint32_t type, uint32_t id, unsigned long en)
{
	pr_warn("mtk_iommu: secure warning, %s is not support\n", __func__);

	return 0;
}

int mtk_iommu_secure_bk_backup_by_atf(uint32_t type, uint32_t id)
{
	pr_warn("mtk_iommu: secure warning, %s is not support\n", __func__);

	return 0;
}

int mtk_iommu_secure_bk_restore_by_atf(uint32_t type, uint32_t id)
{
	pr_warn("mtk_iommu: secure warning, %s is not support\n", __func__);

	return 0;
}

int mtk_iommu_secure_bk_tf_dump(uint32_t type, uint32_t id, uint32_t bank,
		u32 *iova, u32 *pa, u32 *fault_id)
{
	pr_warn("mtk_iommu: secure warning, %s is not support\n", __func__);

	return 0;
}

int ao_secure_dbg_switch_by_atf(uint32_t type, uint32_t id, unsigned long en)
{
	pr_warn("mtk_iommu: secure warning, %s is not support\n", __func__);

	return 0;
}

int mtk_iommu_copy_to_secure_entry(uint32_t type, uint32_t id, dma_addr_t iova, size_t size)
{
	pr_warn("mtk_iommu: secure warning, %s is not support\n", __func__);

	return 0;
}

int mtk_iommu_dump_secure_entry(uint32_t type, uint32_t id, dma_addr_t iova, size_t size)
{
	pr_warn("mtk_iommu: secure warning, %s is not support\n", __func__);

	return 0;
}

int mtk_iommu_clean_secure_entry(uint32_t type, uint32_t id, dma_addr_t iova, size_t size)
{
	pr_warn("mtk_iommu: secure warning, %s is not support\n", __func__);

	return 0;
}

/* test cmd */
#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
void mtk_iommu_dump_bank_base(void)
{
	pr_warn("mtk_iommu: secure warning, %s is not support\n", __func__);
}

int mtk_iommu_dump_bk0_val(uint32_t type, uint32_t id)
{
	pr_warn("mtk_iommu: secure warning, %s is not support\n", __func__);

	return 0;
}

int mtk_iommu_sec_bk_pgtable_dump(uint32_t type, uint32_t id, uint32_t bank,
		u64 iova)
{
	pr_warn("mtk_iommu: secure warning, %s is not support\n", __func__);

	return 0;
}
#endif

bool is_iommu_sec_on_mtee(void)
{
	pr_warn("mtk_iommu: secure warning, %s is not support\n", __func__);

	return false;
}
#endif

#endif
