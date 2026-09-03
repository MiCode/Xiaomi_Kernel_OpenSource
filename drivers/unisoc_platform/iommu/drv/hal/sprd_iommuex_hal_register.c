// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include "sprd_iommuex_hal_register.h"

bool mmu_ex_check_en(ulong ctrl_base_addr, u32 iommu_id)
{
	ulong reg_addr = ctrl_base_addr;

	if (iommu_id == IOMMU_EX_ISP)
		reg_addr += 0x20;

	return (reg_read_dword(reg_addr) & 0x1) ? true : false;
}

void mmu_ex_enable(ulong ctrl_base_addr, u32 iommu_id, u32 mmu_enable)
{
	ulong reg_addr = ctrl_base_addr;

	if (iommu_id == IOMMU_EX_ISP)
		reg_addr += 0x20;

	putbit(reg_addr, mmu_enable, 0);
}

/*
* sharkl3 dpu register is shadowed to internal ram, so we have to set
* vaorbypass, clkgate and enable in a single function.
*/
void mmu_ex_vaorbypass_clkgate_enable_combined(ulong ctrl_base_addr,
	u32 iommu_id)
{
	ulong reg_addr = ctrl_base_addr;
	u32  reg_value = 0;

	if (iommu_id == IOMMU_EX_ISP) {
		/*isp vaor_bypass register is different*/
		reg_addr += 0x10;
		reg_write_dword(reg_addr, 0);
		reg_addr += 0x4;
		reg_write_dword(reg_addr, 0);
		reg_addr += 0xC;
		reg_value = reg_read_dword(reg_addr);
		reg_write_dword(reg_addr, reg_value | 0x1);
	} else if (iommu_id == IOMMU_EX_DCAM || iommu_id == IOMMU_EX_NEWISP ||
		   iommu_id == IOMMU_EX_VSP || iommu_id == IOMMU_EX_JPG ||
		   iommu_id == IOMMU_EX_CPP) {
		reg_value = reg_read_dword(reg_addr);
		reg_value &= ~(1 << 4);
		reg_write_dword(reg_addr, reg_value | 0x3);
	} else {
		reg_value = reg_read_dword(reg_addr);
		reg_write_dword(reg_addr, reg_value | 0x13);
	}

}


void mmu_ex_clock_gate_enable(ulong ctrl_base_addr, u32 cg_enable)
{
	ulong reg_addr = ctrl_base_addr;

	putbit(reg_addr, cg_enable, 1);
}

void mmu_ex_vaout_bypass_enable(ulong ctrl_base_addr, u32 iommu_id,
		u32 iommu_type, bool vaor_bp_en)
{
	ulong reg_addr = ctrl_base_addr;
	u32 reg_value;
	if (iommu_id == IOMMU_EX_ISP) {
		/*just isp need write and read*/
		reg_value = vaor_bp_en ? 0xffffffff : 0x0;
		reg_addr += 0x10;
		reg_write_dword(reg_addr, reg_value);
		reg_addr += 0x4;
		reg_write_dword(reg_addr, reg_value);
	} else {
		if (vaor_bp_en)
			putbit(reg_addr, 1, 4);
		else
			putbit(reg_addr, 0, 4);
	}
}

/*just isp_iommu need,be similar to update*/
void mmuex_tlb_enable(ulong ctrl_base_addr, u32 r_enable, u32 w_enable)
{
	ulong reg_addr = ctrl_base_addr + 0x18;

	if (w_enable)
		reg_write_dword(reg_addr, 0xffffffff);
	else
		reg_write_dword(reg_addr, 0);

	reg_addr += 0x4;
	if (r_enable)
		reg_write_dword(reg_addr, 0xffffffff);
	else
		reg_write_dword(reg_addr, 0);
}

/*just isp_iommu need,be similar to update*/
void mmu_ex_tlb_update(ulong ctrl_base_addr, enum sprd_iommu_ch_type ch_type,
		       u32 ch_id)
{
	ulong reg_addr = ctrl_base_addr;
	u32  org_value = 0;
	u32  new_value = 0;

	if (ch_type == EX_CH_WRITE)
		reg_addr = ctrl_base_addr + 0x18;
	else if (ch_type == EX_CH_READ)
		reg_addr = ctrl_base_addr + 0x1C;

	org_value = reg_read_dword(reg_addr);

	/*invalid channel*/
	new_value = org_value & (~ch_id);
	reg_write_dword(reg_addr, new_value);

	/*enable channel*/
	new_value = org_value | ch_id;
	reg_write_dword(reg_addr, new_value);
}

void mmu_ex_update(ulong ctrl_base_addr, u32 iommu_id, u32 iommu_type)
{
	ulong reg_addr = ctrl_base_addr + UPDATE_OFFSET;
	reg_write_dword(reg_addr, 0xffffffff);
}

void mmu_ex_first_vpn(ulong ctrl_base_addr, u32 iommu_id, u32 vp_addr)
{
	ulong reg_addr = ctrl_base_addr;

	if (iommu_id == IOMMU_EX_ISP)
		reg_addr += 0x24;
	else
		reg_addr += FIRST_VPN_OFFSET;

	reg_write_dword(reg_addr, (vp_addr >> MMU_MAPING_PAGESIZE_SHIFFT));
}

void mmu_ex_vpn_range(ulong ctrl_base_addr, u32 iommu_id, u32 vp_range)
{
	ulong reg_addr = ctrl_base_addr + VPN_RANGE_OFFSET;

	reg_write_dword(reg_addr, vp_range);
}

void mmu_ex_first_ppn(ulong ctrl_base_addr, u32 iommu_id, ulong pp_addr)
{
	ulong reg_addr = ctrl_base_addr;

	if (iommu_id == IOMMU_EX_ISP) {
		reg_addr += 0x28;
		/*lower bits of pagetable base addr,must be 4k align*/
		reg_write_dword(reg_addr, pp_addr & 0xfffff000);
		/*higher bits of pagetable base addr*/
		reg_addr += 0x4;
		reg_write_dword(reg_addr, ((u64)pp_addr >> 32) & 0xff);
	} else {
		reg_addr += FIRST_PPN_OFFSET;
		reg_write_dword(reg_addr,
			(pp_addr >> MMU_MAPING_PAGESIZE_SHIFFT));
	}
}

/*just isp_iommu need,max is 256K*/
void mmuex_pagetable_size(ulong ctrl_base_addr, ulong pt_size)
{
	ulong reg_addr = ctrl_base_addr + 0x30;

	reg_write_dword(reg_addr, pt_size & 0x3ffff);
}

void mmu_ex_default_ppn(ulong ctrl_base_addr, u32 iommu_id, ulong pp_addr)
{
	ulong reg_addr = ctrl_base_addr;

	if (iommu_id == IOMMU_EX_ISP)
		reg_addr += 0x34;
	else
		reg_addr += DEFAULT_PPN_OFFSET;

	reg_write_dword(reg_addr, (pp_addr >> MMU_MAPING_PAGESIZE_SHIFFT));
}

void mmu_ex_pt_update_arqos(ulong ctrl_base_addr, u32 arqos)
{
	ulong reg_addr = ctrl_base_addr + PT_UPDATE_QOS_OFFSET;

	reg_write_dword(reg_addr, (arqos & 0xf));
}

/*1M align*/
void mmu_ex_mini_ppn1(ulong ctrl_base_addr, u32 iommu_id, ulong ppn1)
{
	ulong reg_addr = 0;

	if (iommu_id == IOMMU_EX_JPG)
		reg_addr = ctrl_base_addr + JPG_PPN1_OFFSET;
	else
		reg_addr = ctrl_base_addr + MINI_PPN1_OFFSET;

	reg_write_dword(reg_addr, (ppn1 >> 20));
}

void mmu_ex_ppn1_range(ulong ctrl_base_addr, u32 iommu_id, ulong ppn1_range)
{
	ulong reg_addr = 0;

	if (iommu_id == IOMMU_EX_JPG)
		reg_addr = ctrl_base_addr + JPG_PPN1_RANGE_OFFSET;
	else
		reg_addr = ctrl_base_addr + PPN1_RANGE_OFFSET;

	reg_write_dword(reg_addr, (ppn1_range >> 20));
}

void mmu_ex_mini_ppn2(ulong ctrl_base_addr, u32 iommu_id, ulong ppn2)
{
	ulong reg_addr = 0;

	if (iommu_id == IOMMU_EX_JPG)
		reg_addr = ctrl_base_addr + JPG_PPN2_OFFSET;
	else
		reg_addr = ctrl_base_addr + MINI_PPN2_OFFSET;

	reg_write_dword(reg_addr, (ppn2 >> 20));
}

void mmu_ex_ppn2_range(ulong ctrl_base_addr, u32 iommu_id, ulong ppn2_range)
{
	ulong reg_addr = 0;

	if (iommu_id == IOMMU_EX_JPG)
		reg_addr = ctrl_base_addr + JPG_PPN2_RANGE_OFFSET;
	else
		reg_addr = ctrl_base_addr + PPN2_RANGE_OFFSET;

	reg_write_dword(reg_addr, (ppn2_range >> 20));
}

void mmu_ex_reg_authority(ulong ctrl_base_addr, u32 iommu_id, ulong reg_ctrl)
{
	ulong reg_addr = ctrl_base_addr;

	if (iommu_id == IOMMU_EX_ISP)
		reg_addr += 0x3FC;
	else
		reg_addr += REG_AUTHORITY_OFFSET;

	putbit(reg_addr, reg_ctrl, 0);
}

void mmu_ex_write_pate_totable(ulong pgt_base_addr,
	u32 entry_index, u32 ppn_addr)
{
	ulong pgt_addr = pgt_base_addr + entry_index * 4;

	reg_write_dword(pgt_addr, ppn_addr);
}

u32 mmu_ex_read_page_entry(ulong page_table_addr, u32 entry_index)
{
	ulong reg_addr = page_table_addr + entry_index * 4;
	u32 phy_addr = 0;

	phy_addr = reg_read_dword(reg_addr);
	return phy_addr;
}

void mmu_ex_frc_copy(ulong ctrl_base_addr, u32 iommu_id, u32 iommu_type)
{
	ulong reg_addr;

	if (iommu_id == IOMMU_EX_DCAM) {
		if (iommu_type == SPRD_IOMMUEX_SHARKLE)
			reg_addr = ctrl_base_addr + 0x2010;
		else if (iommu_type == SPRD_IOMMUEX_PIKE2)
			reg_addr = ctrl_base_addr + 0x4;
		else if (iommu_type == SPRD_IOMMUEX_SHARKL3
			|| iommu_type == SPRD_IOMMUEX_SHARKL5)
			reg_addr = ctrl_base_addr + 0x3010;
		else
			return;

		putbit(reg_addr, 1, 0);
	}
}
