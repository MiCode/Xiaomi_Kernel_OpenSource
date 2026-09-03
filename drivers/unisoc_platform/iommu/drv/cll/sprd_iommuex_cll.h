/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_IOMMU_CLL_H_
#define _SPRD_IOMMU_CLL_H_

#include "../inc/sprd_defs.h"
#include "../com/sprd_com.h"
#include "../api/sprd_iommu_api.h"
#include "../hal/sprd_iommuex_hal_register.h"

struct sprd_iommuex_interrupt {
	u8 pa_out_range_r_en;
	u8 pa_out_range_w_en;
	u8 va_out_range_r_en;
	u8 va_out_range_w_en;
	u8 invalid_r_en;
	u8 invalid_w_en;
	u8 unsecure_r_en;
	u8 unsecure_w_en;
};

struct sprd_iommuex_priv {
	ulong master_reg_addr;/*master reg base address*/
	ulong mmu_reg_addr;/*mmu register offset from master base addr*/
	u32 pgt_size;
	u32 phys_offset;

	u8 va_out_bypass_en;/*va out of range bypass,1 default*/
	ulong vpn_base_addr;
	u32 vpn_range;
	ulong ppn_base_addr;/*pagetable base addr in ddr*/
	ulong default_addr;
	ulong mini_ppn1;
	ulong ppn1_range;
	ulong mini_ppn2;
	ulong ppn2_range;
	/*iommu reserved memory of pf page table*/
	unsigned long pagt_base_ddr;
	unsigned int pagt_ddr_size;
	unsigned long pagt_base_phy_ddr;

	u8 ram_clk_div;/*Clock divisor*/

	u8 map_cnt;
	enum sprd_iommu_type iommu_type;
	enum IOMMU_ID iommu_id;
	int chip;
	struct sprd_iommuex_interrupt st_interrupt;
};

u32 sprd_iommuex_cll_init(struct sprd_iommu_init_param *p_init_param,
			sprd_iommu_hdl  p_iommu_hdl);
u32 sprd_iommuex_cll_uninit(sprd_iommu_hdl  p_iommu_hdl);
u32 sprd_iommuex_cll_map(sprd_iommu_hdl  p_iommu_hdl,
				struct sprd_iommu_map_param *p_map_param);
u32 sprd_iommuex_cll_unmap(sprd_iommu_hdl p_iommu_hdl,
			struct sprd_iommu_unmap_param *p_unmap_param);
u32 sprd_iommuex_cll_unmap_orphaned(sprd_iommu_hdl p_iommu_hdl,
			struct sprd_iommu_unmap_param *p_unmap_param);
u32 sprd_iommuex_cll_enable(sprd_iommu_hdl p_iommu_hdl);
u32 sprd_iommuex_cll_disable(sprd_iommu_hdl  p_iommu_hdl);
u32 sprd_iommuex_cll_suspend(sprd_iommu_hdl p_iommu_hdl);
u32 sprd_iommuex_cll_resume(sprd_iommu_hdl  p_iommu_hdl);
u32 sprd_iommuex_cll_release(sprd_iommu_hdl  p_iommu_hdl);
u32 sprd_iommuex_cll_reset(sprd_iommu_hdl  p_iommu_hdl, u32 channel_num);
u32 sprd_iommuex_cll_set_bypass(sprd_iommu_hdl  p_iommu_hdl, bool vaor_bp_en);
u32 sprd_iommuex_cll_virt_to_phy(sprd_iommu_hdl p_iommu_hdl,
			u64 virt_addr, u64 *dest_addr);
u32 sprd_iommuex_reg_authority(sprd_iommu_hdl  p_iommu_hdl, u8 authority);
void sprd_iommuex_flush_pgt(ulong ppn_base, u32 start_entry, u32 end_entry);

#endif  /* _SPRD_IOMMU_CLL_H_ */
