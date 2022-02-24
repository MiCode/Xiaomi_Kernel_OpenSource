// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#include <linux/bug.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include "devapc-mt6789.h"

static struct mtk_device_num mtk6789_devices_num[] = {
	{SLAVE_TYPE_INFRA, VIO_SLAVE_NUM_INFRA},
	{SLAVE_TYPE_PERI, VIO_SLAVE_NUM_PERI},
	{SLAVE_TYPE_PERI2, VIO_SLAVE_NUM_PERI2},
	{SLAVE_TYPE_PERI_PAR, VIO_SLAVE_NUM_PERI_PAR},
};

static struct PERIAXI_ID_INFO peri_mi_id_to_master[] = {
	{"THERM2",       { 0, 0, 0 } },
	{"SPM",          { 0, 1, 0 } },
	{"THERM",        { 0, 1, 1 } },
	{"SPM",          { 1, 1, 0 } },
};

static struct INFRAAXI_ID_INFO infra_mi_id_to_master[] = {
	{"CONNSYS",           { 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0 } },
	{"ADSPSYS",           { 0, 1, 0, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"CQDMA",             { 0, 0, 1, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0 } },
	{"debug_top",         { 0, 0, 1, 0, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0 } },
	{"SSUSB",             { 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 2, 2, 0 } },
	{"SPI0",              { 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0 } },
	{"MSDC1",             { 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0 } },
	{"SPI1",              { 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0 } },
	{"MSDC0",             { 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 2, 2, 0, 0 } },
	{"SPI2",              { 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0 } },
	{"SPI3",              { 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0 } },
	{"SPI4",              { 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0 } },
	{"SPI5",              { 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0 } },
	{"PWM",               { 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0 } },
	{"AUDIO",             { 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0 } },
	{"APDMA_EXT",         { 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 2, 2, 2, 0 } },
	{"THERM2",            { 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0 } },
	{"SPM",               { 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0 } },
	{"THERM",             { 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0 } },
	{"HWCCF",             { 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 2, 2, 0, 0 } },
	{"DXCC",              { 0, 0, 1, 0, 1, 1, 0, 2, 2, 2, 2, 0, 0, 0 } },
	{"GCE",               { 0, 0, 1, 0, 0, 0, 1, 2, 2, 0, 0, 0, 0, 0 } },
	{"MCUPM",             { 0, 0, 1, 0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 0 } },
	{"DPMAIF",            { 0, 1, 1, 0, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0 } },
	{"SSPM",              { 0, 0, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"UFS",               { 0, 1, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"MCUPM",             { 0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"APMCU_Write",       { 1, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"APMCU_Write",       { 1, 2, 2, 2, 2, 0, 0, 1, 0, 0, 0, 0, 0, 0 } },
	{"APMCU_Write",       { 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0 } },
	{"APMCU_Read",        { 1, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"APMCU_Read",        { 1, 2, 2, 2, 2, 0, 0, 1, 0, 0, 0, 0, 0, 0 } },
	{"APMCU_Read",        { 1, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0 } },
	{"APMCU_Read",        { 1, 2, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0 } },
	{"APMCU_Read",        { 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0 } },
	{"APMCU_Read",        { 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0 } },
};

static const char *infra_mi_trans(uint32_t bus_id)
{
	int master_count = ARRAY_SIZE(infra_mi_id_to_master);
	const char *master = "UNKNOWN_MASTER_FROM_INFRA";
	int i, j;

	for (i = 0; i < master_count; i++) {
		for (j = 0; j < INFRAAXI_MI_BIT_LENGTH; j++) {
			if (infra_mi_id_to_master[i].bit[j] == 2)
				continue;
			if (((bus_id >> j) & 0x1) ==
					infra_mi_id_to_master[i].bit[j])
				continue;
			break;
		}
		if (j == INFRAAXI_MI_BIT_LENGTH) {
			pr_debug(PFX "%s %s %s\n",
				"catch it from INFRAAXI_MI",
				"Master is:",
				infra_mi_id_to_master[i].master);
			master = infra_mi_id_to_master[i].master;
		}
	}

	return master;
}

static const char *peri_mi_trans(uint32_t bus_id)
{
	int master_count = ARRAY_SIZE(peri_mi_id_to_master);
	const char *master = "UNKNOWN_MASTER_FROM_PERI";
	int i, j;

	if ((bus_id & 0x3) == 0x0)
		return infra_mi_trans(bus_id >> 2);
	else if ((bus_id & 0x3) == 0x2)
		return "MD_AP_M";
	else if ((bus_id & 0x3) == 0x3)
		return "AP_DMA_M";

	bus_id = bus_id >> 2;

	for (i = 0 ; i < master_count; i++) {
		for (j = 0 ; j < PERIAXI_MI_BIT_LENGTH ; j++) {
			if (peri_mi_id_to_master[i].bit[j] == 2)
				continue;
			if (((bus_id >> j) & 0x1) ==
					peri_mi_id_to_master[i].bit[j])
				continue;
			break;
		}
		if (j == PERIAXI_MI_BIT_LENGTH) {
			pr_debug(PFX "%s %s %s\n",
				"catch it from PERIAXI_MI.",
				"Master is:",
				peri_mi_id_to_master[i].master);
			master = peri_mi_id_to_master[i].master;
		}
	}

	return master;
}

static bool is_addr_in_mdp_mali(uint32_t addr)
{
	if ((addr >= MDP_REGION1_START_ADDR && addr <= MDP_REGION1_END_ADDR) ||
		(addr >= MDP_REGION2_START_ADDR && addr <= MDP_REGION2_END_ADDR) ||
		(addr >= MDP_REGION3_START_ADDR && addr <= MDP_REGION3_END_ADDR)) {
		pr_info(PFX "vio_addr might from MDP_MALI\n");
		return true;
	}

	return false;
}

static bool is_addr_in_mmsys_mali(uint32_t addr)
{
	if ((addr >= MMSYS_REGION1_START_ADDR && addr <= MMSYS_REGION1_END_ADDR) ||
		(addr >= MMSYS_REGION2_START_ADDR && addr <= MMSYS_REGION2_END_ADDR)) {
		pr_info(PFX "vio_addr might from MMSYS_MALI\n");
		return true;
	}

	return false;
}

static const char *mt6789_bus_id_to_master(uint32_t bus_id, uint32_t vio_addr,
		int slave_type, int shift_sta_bit, int domain)
{
	const char *err_master = "UNKNOWN_MASTER";

	pr_debug(PFX "%s:0x%x, %s:0x%x, %s:0x%x, %s:%d\n",
		"bus_id", bus_id, "vio_addr", vio_addr,
		"slave_type", slave_type,
		"shift_sta_bit", shift_sta_bit);

	if ((vio_addr >= TINYSYS_START_ADDR && vio_addr <= TINYSYS_END_ADDR) ||
	    (vio_addr >= MD_START_ADDR && vio_addr <= MD_END_ADDR)) {
		pr_info(PFX "bus_id might be wrong\n");

		if (domain == 0x1)
			return "SSPM";
		else if (domain == 0x2)
			return "CONNSYS";

	} else if (vio_addr >= CONN_START_ADDR && vio_addr <= CONN_END_ADDR) {
		pr_info(PFX "bus_id might be wrong\n");

		if (domain == 0x1)
			return "MD";
		else if (domain == 0x2)
			return "ADSP";
	}

	if (slave_type == SLAVE_TYPE_INFRA) {
		if (vio_addr <= 0x1FFFFF || shift_sta_bit == 6) {
			pr_info(PFX "vio_addr might from SRAMROM\n");
			if ((bus_id & 0x1) == 0x0)
				return "EMI_L2C_M";

			return infra_mi_trans(bus_id >> 1);

		} else if (is_addr_in_mdp_mali(vio_addr) ||
				is_addr_in_mmsys_mali(vio_addr) ||
				shift_sta_bit == 7) {
			if ((bus_id & 0x1) == 0x1)
				return "GCE_M";

			return infra_mi_trans(bus_id >> 1);

		} else if (shift_sta_bit >= 0 && shift_sta_bit <= 3) {
			return peri_mi_trans(bus_id);

		} else if (shift_sta_bit >= 4 && shift_sta_bit <= 5) {
			return infra_mi_trans(bus_id);
		}

		return err_master;

	} else if (slave_type == SLAVE_TYPE_PERI) {
		if (shift_sta_bit == 1 || shift_sta_bit == 2 ||
				shift_sta_bit == 7) {
			if ((bus_id & 0x1) == 0)
				return "MD_AP_M";

			return peri_mi_trans(bus_id >> 1);
		}
		return peri_mi_trans(bus_id);

	} else if (slave_type == SLAVE_TYPE_PERI2) {
		return peri_mi_trans(bus_id);

	} else if (slave_type == SLAVE_TYPE_PERI_PAR) {
		return peri_mi_trans(bus_id);

	}

	return err_master;
}

/* violation index corresponds to subsys */
const char *index_to_subsys(int slave_type, uint32_t vio_index,
		uint32_t vio_addr)
{
	int i;

	pr_debug(PFX "%s %s %d, %s %d, %s 0x%x\n",
			__func__,
			"slave_type", slave_type,
			"vio_index", vio_index,
			"vio_addr", vio_addr);

	/* Filter by violation index */
	if (slave_type == SLAVE_TYPE_INFRA) {
		for (i = 0; i < VIO_SLAVE_NUM_INFRA; i++) {
			if (vio_index == mt6789_devices_infra[i].vio_index)
				return mt6789_devices_infra[i].device;
		}

	} else if (slave_type == SLAVE_TYPE_PERI)
		for (i = 0; i < VIO_SLAVE_NUM_PERI; i++) {
			if (vio_index == mt6789_devices_peri[i].vio_index)
				return mt6789_devices_peri[i].device;
		}

	else if (slave_type == SLAVE_TYPE_PERI2)
		for (i = 0; i < VIO_SLAVE_NUM_PERI2; i++) {
			if (vio_index == mt6789_devices_peri2[i].vio_index)
				return mt6789_devices_peri2[i].device;
		}

	else if (slave_type == SLAVE_TYPE_PERI_PAR)
		for (i = 0; i < VIO_SLAVE_NUM_PERI_PAR; i++) {
			if (vio_index == mt6789_devices_peri_par[i].vio_index)
				return mt6789_devices_peri_par[i].device;
		}

	return "OUT_OF_BOUND";
}

static void mm2nd_vio_handler(void __iomem *infracfg,
			      struct mtk_devapc_vio_info *vio_info,
			      bool mdp_vio, bool disp2_vio, bool mmsys_vio)
{
	uint32_t vio_sta, vio_dbg, rw;
	uint32_t vio_sta_num;
	uint32_t vio0_offset;
	char mm_str[64] = {0};
	void __iomem *reg;
	int i;

	if (!infracfg) {
		pr_err(PFX "%s, param check failed, infracfg ptr is NULL\n",
				__func__);
		return;
	}

	if (mdp_vio) {
		vio_sta_num = INFRACFG_MDP_VIO_STA_NUM;
		vio0_offset = INFRACFG_MDP_SEC_VIO0_OFFSET;

		strncpy(mm_str, "INFRACFG_MDP_SEC_VIO", sizeof(mm_str));

	} else if (mmsys_vio) {
		vio_sta_num = INFRACFG_MM_VIO_STA_NUM;
		vio0_offset = INFRACFG_MM_SEC_VIO0_OFFSET;

		strncpy(mm_str, "INFRACFG_MM_SEC_VIO", sizeof(mm_str));

	} else {
		pr_err(PFX "%s: param check failed, %s:%s, %s:%s, %s:%s\n",
				__func__,
				"mdp_vio", mdp_vio ? "true" : "false",
				"disp2_vio", disp2_vio ? "true" : "false",
				"mmsys_vio", mmsys_vio ? "true" : "false");
		return;
	}

	/* Get mm2nd violation status */
	for (i = 0; i < vio_sta_num; i++) {
		reg = infracfg + vio0_offset + i * 4;
		vio_sta = readl(reg);
		if (vio_sta)
			pr_info(PFX "MM 2nd violation: %s%d:0x%x\n",
					mm_str, i, vio_sta);
	}

	/* Get mm2nd violation address */
	reg = infracfg + vio0_offset + i * 4;
	vio_info->vio_addr = readl(reg);

	/* Get mm2nd violation information */
	reg = infracfg + vio0_offset + (i + 1) * 4;
	vio_dbg = readl(reg);

	vio_info->domain_id = (vio_dbg & INFRACFG_MM2ND_VIO_DOMAIN_MASK) >>
		INFRACFG_MM2ND_VIO_DOMAIN_SHIFT;

	vio_info->master_id = (vio_dbg & INFRACFG_MM2ND_VIO_ID_MASK) >>
		INFRACFG_MM2ND_VIO_ID_SHIFT;

	rw = (vio_dbg & INFRACFG_MM2ND_VIO_RW_MASK) >>
		INFRACFG_MM2ND_VIO_RW_SHIFT;
	vio_info->read = (rw == 0);
	vio_info->write = (rw == 1);
}

static uint32_t mt6789_shift_group_get(int slave_type, uint32_t vio_idx)
{
	if (slave_type == SLAVE_TYPE_INFRA) {
		if (vio_idx >= 0 && vio_idx <= 3)
			return 0;
		else if (vio_idx >= 4 && vio_idx <= 5)
			return 1;
		else if ((vio_idx >= 6 && vio_idx <= 31) ||
			 (vio_idx >= 347 && vio_idx <= 373) ||
			 vio_idx == 390)
			return 2;
		else if ((vio_idx >= 32 && vio_idx <= 43) ||
			 (vio_idx >= 374 && vio_idx <= 386) ||
			 vio_idx == 391)
			return 3;
		else if ((vio_idx >= 46 && vio_idx <= 61) || vio_idx == 387)
			return 4;
		else if ((vio_idx >= 62 && vio_idx <= 66) || vio_idx == 388)
			return 5;
		else if ((vio_idx >= 67 && vio_idx <= 68) || vio_idx == 389)
			return 6;
		else if (vio_idx >= 69 && vio_idx <= 346)
			return 7;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI) {
		if ((vio_idx >= 0 && vio_idx <= 2) ||
		    (vio_idx >= 117 && vio_idx <= 120) ||
		    vio_idx == 156)
			return 0;
		else if ((vio_idx >= 3 && vio_idx <= 25) || vio_idx == 121)
			return 1;
		else if ((vio_idx >= 26 && vio_idx <= 40) || vio_idx == 122)
			return 2;
		else if ((vio_idx >= 41 && vio_idx <= 79) || vio_idx == 123)
			return 3;
		else if ((vio_idx >= 80 && vio_idx <= 81) || vio_idx == 124)
			return 4;
		else if ((vio_idx >= 82 && vio_idx <= 85) || vio_idx == 125)
			return 5;
		else if (vio_idx == 86 || vio_idx == 126)
			return 6;
		else if ((vio_idx >= 87 && vio_idx <= 89) || vio_idx == 127)
			return 7;
		else if (vio_idx == 90 ||
			 (vio_idx >= 128 && vio_idx <= 129) ||
			 vio_idx == 157)
			return 8;
		else if (vio_idx >= 91 && vio_idx <= 92)
			return 9;
		else if ((vio_idx >= 93 && vio_idx <= 107) ||
			 (vio_idx >= 130 && vio_idx <= 145) ||
			 vio_idx == 158)
			return 10;
		else if ((vio_idx >= 108 && vio_idx <= 116) ||
			 (vio_idx >= 146 && vio_idx <= 155) ||
			 vio_idx == 159)
			return 11;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI2) {
		if ((vio_idx >= 0 && vio_idx <= 2) ||
		    (vio_idx >= 106 && vio_idx <= 109) ||
		    vio_idx == 212)
			return 0;
		else if (vio_idx >= 3 && vio_idx <= 6)
			return 1;
		else if (vio_idx >= 7 && vio_idx <= 10)
			return 2;
		else if ((vio_idx >= 11 && vio_idx <= 26) ||
			 (vio_idx >= 110 && vio_idx <= 126) ||
			 vio_idx == 213)
			return 3;
		else if ((vio_idx >= 27 && vio_idx <= 34) ||
			 (vio_idx >= 127 && vio_idx <= 135) ||
			 vio_idx == 214)
			return 4;
		else if ((vio_idx >= 35 && vio_idx <= 50) ||
			 (vio_idx >= 136 && vio_idx <= 152) ||
			 vio_idx == 215)
			return 5;
		else if ((vio_idx >= 51 && vio_idx <= 66) ||
			 (vio_idx >= 153 && vio_idx <= 169) ||
			 vio_idx == 216)
			return 6;
		else if ((vio_idx >= 67 && vio_idx <= 74) ||
			 (vio_idx >= 170 && vio_idx <= 178) ||
			 vio_idx == 217)
			return 7;
		else if ((vio_idx >= 75 && vio_idx <= 93) ||
			 (vio_idx >= 179 && vio_idx <= 198) ||
			 vio_idx == 218)
			return 8;
		else if ((vio_idx >= 94 && vio_idx <= 105) ||
			 (vio_idx >= 199 && vio_idx <= 211) ||
			 vio_idx == 219)
			return 9;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI_PAR) {
		if ((vio_idx >= 0 && vio_idx <= 1) ||
		    (vio_idx >= 25 && vio_idx <= 26) ||
		    vio_idx == 52)
			return 0;
		else if ((vio_idx >= 2 && vio_idx <= 21) ||
			 (vio_idx >= 27 && vio_idx <= 47) ||
			 vio_idx == 53)
			return 1;
		else if ((vio_idx >= 22 && vio_idx <= 24) ||
			 (vio_idx >= 48 && vio_idx <= 51) ||
			 vio_idx == 54)
			return 2;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	}

	pr_err(PFX "%s:%d Wrong slave_type:0x%x\n",
			__func__, __LINE__, slave_type);

	return 31;
}

static struct mtk_devapc_dbg_status mt6789_devapc_dbg_stat = {
	.enable_ut = PLAT_DBG_UT_DEFAULT,
	.enable_KE = PLAT_DBG_KE_DEFAULT,
	.enable_AEE = PLAT_DBG_AEE_DEFAULT,
	.enable_WARN = PLAT_DBG_WARN_DEFAULT,
	.enable_dapc = PLAT_DBG_DAPC_DEFAULT,
};

static const char * const slave_type_to_str[] = {
	"SLAVE_TYPE_INFRA",
	"SLAVE_TYPE_PERI",
	"SLAVE_TYPE_PERI2",
	"SLAVE_TYPE_PERI_PAR",
	"WRONG_SLAVE_TYPE",
};

static int mtk_vio_mask_sta_num[] = {
	VIO_MASK_STA_NUM_INFRA,
	VIO_MASK_STA_NUM_PERI,
	VIO_MASK_STA_NUM_PERI2,
	VIO_MASK_STA_NUM_PERI_PAR,
};

static struct mtk_devapc_vio_info mt6789_devapc_vio_info = {
	.vio_mask_sta_num = mtk_vio_mask_sta_num,
	.sramrom_vio_idx = SRAMROM_VIO_INDEX,
	.mdp_vio_idx = MDP_VIO_INDEX,
	.disp2_vio_idx = MDP_VIO_INDEX,
	.mmsys_vio_idx = MMSYS_VIO_INDEX,
	.sramrom_slv_type = SRAMROM_SLAVE_TYPE,
	.mm2nd_slv_type = MM2ND_SLAVE_TYPE,
};

static const struct mtk_infra_vio_dbg_desc mt6789_vio_dbgs = {
	.vio_dbg_mstid = INFRA_VIO_DBG_MSTID,
	.vio_dbg_mstid_start_bit = INFRA_VIO_DBG_MSTID_START_BIT,
	.vio_dbg_dmnid = INFRA_VIO_DBG_DMNID,
	.vio_dbg_dmnid_start_bit = INFRA_VIO_DBG_DMNID_START_BIT,
	.vio_dbg_w_vio = INFRA_VIO_DBG_W_VIO,
	.vio_dbg_w_vio_start_bit = INFRA_VIO_DBG_W_VIO_START_BIT,
	.vio_dbg_r_vio = INFRA_VIO_DBG_R_VIO,
	.vio_dbg_r_vio_start_bit = INFRA_VIO_DBG_R_VIO_START_BIT,
	.vio_addr_high = INFRA_VIO_ADDR_HIGH,
	.vio_addr_high_start_bit = INFRA_VIO_ADDR_HIGH_START_BIT,
};

static const struct mtk_sramrom_sec_vio_desc mt6789_sramrom_sec_vios = {
	.vio_id_mask = SRAMROM_SEC_VIO_ID_MASK,
	.vio_id_shift = SRAMROM_SEC_VIO_ID_SHIFT,
	.vio_domain_mask = SRAMROM_SEC_VIO_DOMAIN_MASK,
	.vio_domain_shift = SRAMROM_SEC_VIO_DOMAIN_SHIFT,
	.vio_rw_mask = SRAMROM_SEC_VIO_RW_MASK,
	.vio_rw_shift = SRAMROM_SEC_VIO_RW_SHIFT,
};

static const uint32_t mt6789_devapc_pds[] = {
	PD_VIO_MASK_OFFSET,
	PD_VIO_STA_OFFSET,
	PD_VIO_DBG0_OFFSET,
	PD_VIO_DBG1_OFFSET,
	PD_VIO_DBG2_OFFSET,
	PD_APC_CON_OFFSET,
	PD_SHIFT_STA_OFFSET,
	PD_SHIFT_SEL_OFFSET,
	PD_SHIFT_CON_OFFSET,
	PD_VIO_DBG3_OFFSET,
};

static struct mtk_devapc_soc mt6789_data = {
	.dbg_stat = &mt6789_devapc_dbg_stat,
	.slave_type_arr = slave_type_to_str,
	.slave_type_num = SLAVE_TYPE_NUM,
	.device_info[SLAVE_TYPE_INFRA] = mt6789_devices_infra,
	.device_info[SLAVE_TYPE_PERI] = mt6789_devices_peri,
	.device_info[SLAVE_TYPE_PERI2] = mt6789_devices_peri2,
	.device_info[SLAVE_TYPE_PERI_PAR] = mt6789_devices_peri_par,
	.ndevices = mtk6789_devices_num,
	.vio_info = &mt6789_devapc_vio_info,
	.vio_dbgs = &mt6789_vio_dbgs,
	.sramrom_sec_vios = &mt6789_sramrom_sec_vios,
	.devapc_pds = mt6789_devapc_pds,
	.subsys_get = &index_to_subsys,
	.master_get = &mt6789_bus_id_to_master,
	.mm2nd_vio_handler = &mm2nd_vio_handler,
	.shift_group_get = mt6789_shift_group_get,
};

static const struct of_device_id mt6789_devapc_dt_match[] = {
	{ .compatible = "mediatek,mt6789-devapc" },
	{},
};

static int mt6789_devapc_probe(struct platform_device *pdev)
{
	return mtk_devapc_probe(pdev, &mt6789_data);
}

static int mt6789_devapc_remove(struct platform_device *dev)
{
	return mtk_devapc_remove(dev);
}

static struct platform_driver mt6789_devapc_driver = {
	.probe = mt6789_devapc_probe,
	.remove = mt6789_devapc_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = mt6789_devapc_dt_match,
	},
};

module_platform_driver(mt6789_devapc_driver);

MODULE_DESCRIPTION("Mediatek MT6789 Device APC Driver");
MODULE_AUTHOR("Jackson Chang <jackson-kt.chang@mediatek.com>");
MODULE_LICENSE("GPL");
