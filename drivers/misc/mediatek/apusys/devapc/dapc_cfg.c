/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/io.h>

#include "dapc.h"
#include "dapc_cfg.h"

#define __dbg(dbg, cfg, a) \
	((dbg & cfg->vio_dbg_##a) >> cfg->vio_dbg_##a##_shift)

/*
 * Platform devapc slaves
 *    {sys_idx, ctrl_idx, vio_idx, name, vio_irq_en}
 * Note: "vio_idx" must be aligned with array index,
 *    add paddings to make them continuous.
 */

static struct dapc_slave dapc_slv_mt6885[] = {
	{0, 0, 0, "apusys_ao-0", true},
	{0, 1, 1, "apusys_ao-1", true},
	{0, 2, 2, "apusys_ao-2", true},
	{0, 3, 3, "apusys_ao-3", true},
	{0, 4, 4, "apusys_ao-4", true},
	{0, 5, 5, "apusys_ao-5", true},
	{0, 6, 6, "md32_apb_s-0", true},
	{0, 7, 7, "md32_apb_s-1", true},
	{0, 8, 8, "md32_apb_s-2", true},
	{0, 0, 9, "NULL_PADDING", false},  /* padding */
	{0, 9, 10, "md32_debug_apb", true},
	{0, 10, 11, "apu_conn_config", true},
	{0, 11, 12, "apu_sctrl_reviser", true},
	{0, 12, 13, "apu_sema_stimer", true},
	{0, 13, 14, "apu_emi_config", true},
	{0, 14, 15, "apu_adl", true},
	{0, 15, 16, "apu_edma_lite0", true},
	{0, 16, 17, "apu_edma_lite1", true},
	{0, 17, 18, "apu_edma0", true},
	{0, 18, 19, "apu_edma1", true},
	{0, 19, 20, "apu_dapc_ao", true},
	{0, 20, 21, "apu_dapc", true},
	{0, 21, 22, "infra_bcrm", true},
	{0, 22, 23, "apb_dbg_ctl", true},
	{0, 23, 24, "noc_dapc", true},
	{0, 24, 25, "apu_noc_bcrm", true},
	{0, 25, 26, "apu_noc_config", true},
	{0, 26, 27, "vpu_core0_config-0", true},
	{0, 27, 28, "vpu_core0_config-1", true},
	{0, 28, 29, "vpu_core1_config-0", true},
	{0, 29, 30, "vpu_core1_config-1", true},
	{0, 30, 31, "vpu_core2_config-0", true},
	{0, 31, 32, "vpu_core2_config-1", true},
	{0, 32, 33, "mdla0_apb-0", true},
	{0, 33, 34, "mdla0_apb-1", true},
	{0, 34, 35, "mdla0_apb-2", true},
	{0, 35, 36, "mdla0_apb-3", true},
	{0, 36, 37, "mdla1_apb-0", true},
	{0, 37, 38, "mdla1_apb-1", true},
	{0, 38, 39, "mdla1_apb-2", true},
	{0, 39, 40, "mdla1_apb-3", true},
	{0, 40, 41, "apu_iommu0_r0", true},
	{0, 41, 42, "apu_iommu0_r1", true},
	{0, 42, 43, "apu_iommu0_r2", true},
	{0, 43, 44, "apu_iommu0_r3", true},
	{0, 44, 45, "apu_iommu0_r4", true},
	{0, 45, 46, "apu_iommu1_r0", true},
	{0, 46, 47, "apu_iommu1_r1", true},
	{0, 47, 48, "apu_iommu1_r2", true},
	{0, 48, 49, "apu_iommu1_r3", true},
	{0, 49, 50, "apu_iommu1_r4", true},
	{0, 50, 51, "apu_rsi0_config", true},
	{0, 51, 52, "apu_rsi1_config", true},
	{0, 52, 53, "apu_rsi2_config", true},
	{0, 53, 54, "apu_ssc0_config", true},
	{0, 54, 55, "apu_ssc1_config", true},
	{0, 55, 56, "apu_ssc2_config", true},
	{0, 56, 57, "vp6_core0_debug_apb", true},
	{0, 57, 58, "vp6_core1_debug_apb", true},
	{0, 58, 59, "vp6_core2_debug_apb", true},
	DAPC_SLAVE_END
};

static struct dapc_slave dapc_slv_mt6873[] = {
	{0, 0, 0, "apusys_ao-0", true},
	{0, 1, 1, "apusys_ao-1", true},
	{0, 2, 2, "apusys_ao-2", true},
	{0, 3, 3, "apusys_ao-3", true},
	{0, 4, 4, "apusys_ao-4", true},
	{0, 5, 5, "apusys_ao-5", true},
	{0, 6, 6, "md32_apb_s-0", true},
	{0, 7, 7, "md32_apb_s-1", true},
	{0, 8, 8, "md32_apb_s-2", true},
	{0, 0, 9, "NULL_PADDING", false}, /* padding */
	{0, 9, 10, "md32_debug_apb", true},
	{0, 10, 11, "apu_conn_config", true},
	{0, 11, 12, "apu_sctrl_reviser", true},
	{0, 12, 13, "apu_sema_stimer", true},
	{0, 13, 14, "apu_emi_config", true},
	{0, 14, 15, "apu_edma0", true},
	{0, 15, 16, "apu_dapc_ao", true},
	{0, 16, 17, "apu_dapc", true},
	{0, 17, 18, "infra_bcrm", true},
	{0, 18, 19, "apb_dbg_ctl", true},
	{0, 19, 20, "noc_dapc", true},
	{0, 20, 21, "apu_noc_bcrm", true},
	{0, 21, 22, "apu_noc_config_0", true},
	{0, 22, 23, "apu_noc_config_1", true},
	{0, 23, 24, "apu_noc_config_2", true},
	{0, 24, 25, "apu_noc_config_3", true},
	{0, 25, 26, "apu_noc_config_4", true},
	{0, 26, 27, "vpu_core0_config-0", true},
	{0, 27, 28, "vpu_core0_config-1", true},
	{0, 28, 29, "vpu_core1_config-0", true},
	{0, 29, 30, "vpu_core1_config-1", true},
	{0, 30, 31, "mdla0_apb-0", true},
	{0, 31, 32, "mdla0_apb-1", true},
	{0, 32, 33, "mdla0_apb-2", true},
	{0, 33, 34, "mdla0_apb-3", true},
	{0, 34, 35, "apu_iommu0_r0", true},
	{0, 35, 36, "apu_iommu0_r1", true},
	{0, 36, 37, "apu_iommu0_r2", true},
	{0, 37, 38, "apu_iommu0_r3", true},
	{0, 38, 39, "apu_iommu0_r4", true},
	{0, 39, 40, "apu_rsi2_config", true},
	{0, 40, 41, "apu_ssc2_config", true},
	{0, 41, 42, "vp6_core0_debug_apb", true},
	{0, 42, 43, "vp6_core1_debug_apb", true},
	DAPC_SLAVE_END
};

static struct dapc_slave dapc_slv_mt6853[] = {
	{0, 0, 0, "apusys_ao-0", true},
	{0, 1, 1, "apusys_ao-1", true},
	{0, 2, 2, "apusys_ao-2", true},
	{0, 3, 3, "apusys_ao-3", true},
	{0, 4, 4, "apusys_ao-4", true},
	{0, 5, 5, "apusys_ao-5", true},
	{0, 6, 6, "md32_apb_s-0", true},
	{0, 7, 7, "md32_apb_s-1", true},
	{0, 8, 8, "md32_apb_s-2", true},
	{0, 0, 9, "NULL_PADDING", false}, /* padding */
	{0, 9, 10, "md32_debug_apb", true},
	{0, 10, 11, "apu_conn_config", true},
	{0, 11, 12, "apu_sctrl_reviser", true},
	{0, 12, 13, "apu_sema_stimer", true},
	{0, 13, 14, "apu_emi_config", true},
	{0, 14, 15, "apu_dapc_ao", true},
	{0, 15, 16, "apu_dapc", true},
	{0, 16, 17, "infra_bcrm", true},
	{0, 17, 18, "apb_dbg_ctl", true},
	{0, 18, 19, "noc_dapc", true},
	{0, 19, 20, "apu_noc_bcrm", true},
	{0, 20, 21, "apu_noc_config_0", true},
	{0, 21, 22, "vpu_core0_config-0", true},
	{0, 22, 23, "vpu_core0_config-1", true},
	{0, 23, 24, "vpu_core1_config-0", true},
	{0, 24, 25, "vpu_core1_config-1", true},
	{0, 25, 26, "apu_iommu0_r0", true},
	{0, 26, 27, "apu_iommu0_r1", true},
	{0, 27, 28, "apu_iommu0_r2", true},
	{0, 28, 29, "apu_iommu0_r3", true},
	{0, 29, 30, "apu_iommu0_r4", true},
	{0, 30, 31, "vp6_core0_debug_apb", true},
	{0, 31, 32, "vp6_core1_debug_apb", true},
	DAPC_SLAVE_END
};

static struct dapc_slave dapc_slv_mt6877[] = {
	{0, 0, 0, "apusys_ao-0", true},
	{0, 1, 1, "apusys_ao-1", true},
	{0, 2, 2, "apusys_ao-2", true},
	{0, 3, 3, "apusys_ao-3", true},
	{0, 4, 4, "apusys_ao-4", true},
	{0, 5, 5, "apusys_ao-5", true},
	{0, 6, 6, "apusys_ao-6", true},
	{0, 7, 7, "apusys_ao-8", true},
	{0, 8, 8, "apusys_ao-9", true},
	{0, 9, 9, "md32_apb_s-0", true},
	{0, 10, 10, "md32_apb_s-1", true},
	{0, 0, 11, "padding", false},
	{0, 0, 12, "padding", false},
	{0, 0, 13, "padding", false},
	{0, 0, 14, "padding", false},
	{0, 0, 15, "padding", false},
	{0, 0, 16, "padding", false},
	{0, 0, 17, "padding", false},
	{0, 11, 18, "noc_axi", true},
	{0, 12, 19, "apu_con2_config", true},
	{0, 13, 20, "apu_con1_config", true},
	{0, 14, 21, "apu_sctrl_reviser", true},
	{0, 15, 22, "apu_sema_stimer", true},
	{0, 16, 23, "apu_emi_config", true},
	{0, 17, 24, "apu_edma0", true},
	{0, 0, 25, "padding", false},
	{0, 19, 26, "apu_dapc", true},
	{0, 20, 27, "infra_bcrm", true},
	{0, 21, 28, "infra_ao_bcrm", true},
	{0, 22, 29, "noc_dapc", true},
	{0, 23, 30, "apu_noc_bcrm", true},
	{0, 24, 31, "apu_noc_config_0", true},
	{0, 25, 32, "vpu_core0_config-0", true},
	{0, 26, 33, "vpu_core0_config-1", true},
	{0, 27, 34, "vpu_core1_config-0", true},
	{0, 28, 35, "vpu_core1_config-1", true},
	{0, 29, 36, "mdla0_apb-0", true},
	{0, 30, 37, "mdla0_apb-1", true},
	{0, 31, 38, "mdla0_apb-2", true},
	{0, 32, 39, "mdla0_apb-3", true},
	{0, 33, 40, "apu_iommu0_r0", true},
	{0, 34, 41, "apu_iommu0_r1", true},
	{0, 35, 42, "apu_iommu0_r2", true},
	{0, 36, 43, "apu_iommu0_r3", true},
	{0, 37, 44, "apu_iommu0_r4", true},
	{0, 38, 45, "apu_n0_ssc_config", true},
	{0, 39, 46, "apu_ao_dbgapb-0", true},
	{0, 40, 47, "apu_ao_dbgapb-1", true},
	{0, 41, 48, "apu_ao_dbgapb-2", true},
	{0, 42, 49, "apu_ao_dbgapb-3", true},
	{0, 43, 50, "apu_ao_dbgapb-4", true},
	{0, 44, 51, "apu_ao_dbgapb-5", true},
	{0, 45, 52, "vpu_core0_debug_apb", true},
	{0, 46, 53, "vpu_core1_debug_apb", true},
	{0, 47, 54, "apb_infra_dbg_ctl", true},
	DAPC_SLAVE_END
};

static uint32_t vio_mask_v1(uint32_t idx)
{
	return (0x4 * idx);
}

static uint32_t vio_sta_v1(uint32_t idx)
{
	return 0x400 + (0x4 * idx);
}

/* read excetpion info to "excp" */
static int excp_info_v1(struct dapc_driver *d, struct dapc_exception *ex)
{
	struct dapc_config *cfg;
	uint32_t dbg0;

	if (!ex || !d)
		return -EINVAL;

	cfg = d->cfg;
	dbg0 = dapc_reg_r(d, cfg->vio_dbg0);
	ex->trans_id = dapc_reg_r(d, cfg->vio_dbg1);
	ex->addr = dapc_reg_r(d, cfg->vio_dbg2);

	ex->domain_id = __dbg(dbg0, cfg, dmnid);
	ex->write_vio = __dbg(dbg0, cfg, w_vio);
	ex->read_vio = __dbg(dbg0, cfg, r_vio);
	ex->addr_high = __dbg(dbg0, cfg, addr);

	return 0;
}

struct dapc_config dapc_cfg_mt6885 = {
	.irq_enable = 1,
	.slv = dapc_slv_mt6885,
	.slv_cnt = 60,

	.vio_mask = vio_mask_v1,
	.vio_sta = vio_sta_v1,
	.excp_info = excp_info_v1,

	.apc_con = 0xf00,
	.apc_con_vio = 0x80000100, /* APC_VIO | VIO_AUTO_CLR_EN */
	.vio_shift_sta = 0xf10,
	.vio_shift_sel = 0xf14,
	.vio_shift_con = 0xf20,
	.vio_shift_con_mask = 0x3, /* SHFT_EN | SHFT_DONE */
	.vio_dbg0 = 0x900,
	.vio_dbg1 = 0x904,
	.vio_dbg2 = 0x908,

	.slv_per_dapc = 32,
	.vio_shift_max_bit = 11,
	.vio_dbg_dmnid = 0x3f,
	.vio_dbg_dmnid_shift = 0,
	.vio_dbg_w_vio = 0x40,
	.vio_dbg_w_vio_shift = 6,
	.vio_dbg_r_vio = 0x80,
	.vio_dbg_r_vio_shift = 7,
	.vio_dbg_addr = 0xf00,
	.vio_dbg_addr_shift = 8,

	.ut_base = 0x19021000, /* reviser sysctrl */
};

struct dapc_config dapc_cfg_mt6873 = {
	.irq_enable = 1,
	.slv = dapc_slv_mt6873,
	.slv_cnt = 44,

	.vio_mask = vio_mask_v1,
	.vio_sta = vio_sta_v1,
	.excp_info = excp_info_v1,

	.apc_con = 0xf00,
	.apc_con_vio = 0x80000100, /* APC_VIO | VIO_AUTO_CLR_EN */
	.vio_shift_sta = 0xf10,
	.vio_shift_sel = 0xf14,
	.vio_shift_con = 0xf20,
	.vio_shift_con_mask = 0x3, /* SHFT_EN | SHFT_DONE */
	.vio_dbg0 = 0x900,
	.vio_dbg1 = 0x904,
	.vio_dbg2 = 0x908,

	.slv_per_dapc = 32,
	.vio_shift_max_bit = 9,
	.vio_dbg_dmnid = 0x3f,
	.vio_dbg_dmnid_shift = 0,
	.vio_dbg_w_vio = 0x40,
	.vio_dbg_w_vio_shift = 6,
	.vio_dbg_r_vio = 0x80,
	.vio_dbg_r_vio_shift = 7,
	.vio_dbg_addr = 0xf00,
	.vio_dbg_addr_shift = 8,

	.ut_base = 0x19021000, /* reviser sysctrl */
};

struct dapc_config dapc_cfg_mt6853 = {
	.irq_enable = 1,
	.slv = dapc_slv_mt6853,
	.slv_cnt = 33,

	.vio_mask = vio_mask_v1,
	.vio_sta = vio_sta_v1,
	.excp_info = excp_info_v1,

	.apc_con = 0xf00,
	.apc_con_vio = 0x80000100,  /* APC_VIO | VIO_AUTO_CLR_EN */
	.vio_shift_sta = 0xf20,
	.vio_shift_sel = 0xf30,
	.vio_shift_con = 0xf10,
	.vio_shift_con_mask = 0x3, /* SHFT_EN | SHFT_DONE */
	.vio_dbg0 = 0x900,
	.vio_dbg1 = 0x904,
	.vio_dbg2 = 0x908,

	.slv_per_dapc = 32,
	.vio_shift_max_bit = 9,
	.vio_dbg_dmnid = 0x3f,
	.vio_dbg_dmnid_shift = 0,
	.vio_dbg_w_vio = 0x40,
	.vio_dbg_w_vio_shift = 6,
	.vio_dbg_r_vio = 0x80,
	.vio_dbg_r_vio_shift = 7,
	.vio_dbg_addr = 0xf00,
	.vio_dbg_addr_shift = 8,

	.ut_base = 0x19021000, /* reviser sysctrl */
};

struct dapc_config dapc_cfg_mt6877 = {
	.irq_enable = 1,
	.slv = dapc_slv_mt6877,
	.slv_cnt = 47,

	.vio_mask = vio_mask_v1,
	.vio_sta = vio_sta_v1,
	.excp_info = excp_info_v1,

	.apc_con = 0xf00,
	.apc_con_vio = 0x80000100,  /* APC_VIO | VIO_AUTO_CLR_EN */
	.vio_shift_sta = 0xf20,
	.vio_shift_sel = 0xf30,
	.vio_shift_con = 0xf10,
	.vio_shift_con_mask = 0x3, /* SHFT_EN | SHFT_DONE */
	.vio_dbg0 = 0x900,
	.vio_dbg1 = 0x904,
	.vio_dbg2 = 0x908,

	.slv_per_dapc = 32,
	.vio_shift_max_bit = 9,
	.vio_dbg_dmnid = 0x3f,
	.vio_dbg_dmnid_shift = 0,
	.vio_dbg_w_vio = 0x40,
	.vio_dbg_w_vio_shift = 6,
	.vio_dbg_r_vio = 0x80,
	.vio_dbg_r_vio_shift = 7,
	.vio_dbg_addr = 0xf00,
	.vio_dbg_addr_shift = 8,

	.ut_base = 0x19021000, /* reviser sysctrl */
};

