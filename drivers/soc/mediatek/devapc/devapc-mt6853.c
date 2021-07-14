// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/bug.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include "devapc-mt6853.h"

static struct mtk_device_num mtk6853_devices_num[] = {
	{SLAVE_TYPE_INFRA, VIO_SLAVE_NUM_INFRA},
	{SLAVE_TYPE_PERI, VIO_SLAVE_NUM_PERI},
	{SLAVE_TYPE_PERI2, VIO_SLAVE_NUM_PERI2},
	{SLAVE_TYPE_PERI_PAR, VIO_SLAVE_NUM_PERI_PAR},
};

static struct PERIAXI_ID_INFO peri_mi_id_to_master[] = {
	{"THERM2",       { 0, 0, 0 } },
	{"SPM",          { 0, 1, 0 } },
	{"CCU",          { 0, 0, 1 } },
	{"THERM",        { 0, 1, 1 } },
	{"SPM",          { 1, 1, 0 } },
};

static struct INFRAAXI_ID_INFO infra_mi_id_to_master[] = {
	{"CONNSYS_WFDMA",     { 0, 0, 0, 0,	0, 1, 2, 2,	2, 2, 2, 2,	2, 0 } },
	{"CONNSYS_ICAP",      { 0, 0, 0, 0,	0, 0, 2, 2,	2, 2, 2, 2,	2, 0 } },
	{"CONNSYS_WF_MCU",    { 0, 0, 0, 0,	1, 1, 2, 2,	2, 2, 2, 2,	2, 0 } },
	{"CONNSYS_BT_MCU",    { 0, 0, 0, 0,	1, 0, 0, 1,	2, 2, 2, 2,	2, 0 } },
	{"CONNSYS_GPS",       { 0, 0, 0, 0,	1, 0, 0, 0,	2, 2, 2, 2,	2, 0 } },
	{"Tinysys",           { 0, 1, 0, 0,	2, 2, 2, 2,	2, 2, 0, 0,	0, 0 } },
	{"CQ_DMA",            { 0, 0, 1, 0,	0, 0, 0, 2,	2, 2, 0, 0,	0, 0 } },
	{"DebugTop",          { 0, 0, 1, 0,	1, 0, 0, 2,	0, 0, 0, 0,	0, 0 } },
	{"SSUSB",             { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 0, 0,	2, 2 } },
	{"NOR",               { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 1, 0,	2, 2 } },
	{"PWM",               { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 0, 1,	0, 0 } },
	{"MSDC1",             { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 0, 1,	1, 0 } },
	{"SPI6",              { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 0, 1,	0, 1 } },
	{"SPI0",              { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 0, 1,	1, 1 } },
	{"APU",               { 0, 0, 1, 0,	0, 1, 0, 1,	0, 0, 2, 2,	0, 0 } },
	{"MSDC0",             { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 2, 2,	0, 0 } },
	{"SPI2",              { 0, 0, 1, 0,	0, 1, 0, 1,	1, 0, 0, 0,	0, 0 } },
	{"SPI3",              { 0, 0, 1, 0,	0, 1, 0, 1,	1, 0, 1, 0,	0, 0 } },
	{"SPI4",              { 0, 0, 1, 0,	0, 1, 0, 1,	1, 0, 0, 1,	0, 0 } },
	{"SPI5",              { 0, 0, 1, 0,	0, 1, 0, 1,	1, 0, 1, 1,	0, 0 } },
	{"SPI7",              { 0, 0, 1, 0,	0, 1, 0, 0,	0, 1, 0, 0,	0, 0 } },
	{"Audio",             { 0, 0, 1, 0,	0, 1, 0, 0,	0, 1, 0, 1,	0, 0 } },
	{"SPI1",              { 0, 0, 1, 0,	0, 1, 0, 0,	0, 1, 1, 1,	0, 0 } },
	{"AP_DMA_EXT",        { 0, 0, 1, 0,	0, 1, 0, 1,	0, 1, 2, 2,	2, 0 } },
	{"THERM2",            { 0, 0, 1, 0,	0, 1, 0, 0,	1, 1, 0, 0,	0, 0 } },
	{"SPM",               { 0, 0, 1, 0,	0, 1, 0, 0,	1, 1, 1, 0,	0, 0 } },
	{"CCU",               { 0, 0, 1, 0,	0, 1, 0, 0,	1, 1, 0, 1,	0, 0 } },
	{"THERM",             { 0, 0, 1, 0,	0, 1, 0, 0,	1, 1, 1, 1,	0, 0 } },
	{"DX_CC",             { 0, 0, 1, 0,	1, 1, 0, 2,	2, 2, 2, 0,	0, 0 } },
	{"GCE",               { 0, 0, 1, 0,	0, 0, 1, 2,	2, 0, 0, 0,	0, 0 } },
	{"DPMAIF",            { 0, 1, 1, 0,	2, 2, 2, 2,	0, 0, 0, 0,	0, 0 } },
	{"SSPM",              { 0, 0, 0, 1,	2, 2, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"UFS",               { 0, 1, 0, 1,	2, 2, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"CPUEB",             { 0, 0, 1, 1,	2, 2, 2, 2,	2, 2, 0, 0,	0, 0 } },
	{"APMCU_Write",       { 1, 2, 2, 2,	2, 0, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_Write",       { 1, 2, 2, 2,	2, 0, 0, 1,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_Write",       { 1, 2, 2, 2,	2, 2, 2, 2,	2, 1, 0, 0,	0, 0 } },
	{"APMCU_Read",        { 1, 2, 2, 2,	2, 0, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_Read",        { 1, 2, 2, 2,	2, 0, 0, 1,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_Read",        { 1, 2, 0, 0,	0, 0, 0, 0,	1, 0, 0, 0,	0, 0 } },
	{"APMCU_Read",        { 1, 2, 1, 0,	0, 0, 0, 0,	1, 0, 0, 0,	0, 0 } },
	{"APMCU_Read",        { 1, 2, 2, 2,	2, 2, 2, 2,	2, 1, 0, 0,	0, 0 } },
	{"APMCU_Read",        { 1, 2, 2, 2,	2, 2, 2, 2,	2, 2, 1, 0,	0, 0 } },
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

static const char *mt6853_bus_id_to_master(uint32_t bus_id, uint32_t vio_addr,
		int slave_type, int shift_sta_bit, int domain)
{
	const char *err_master = "UNKNOWN_MASTER";
	uint8_t h_1byte;

	pr_debug(PFX "[DEVAPC] %s:0x%x, %s:0x%x, %s:0x%x, %s:%d\n",
		"bus_id", bus_id, "vio_addr", vio_addr,
		"slave_type", slave_type,
		"shift_sta_bit", shift_sta_bit);

	h_1byte = (vio_addr >> 24) & 0xFF;

	if ((vio_addr >= TINYSYS_START_ADDR && vio_addr <= TINYSYS_END_ADDR) ||
	    (vio_addr >= MD_START_ADDR && vio_addr <= MD_END_ADDR)) {
		pr_info(PFX "[DEVAPC] bus_id might be wrong\n");

		if (domain == 0x1)
			return "SSPM";
		else if (domain == 0x2)
			return "CONNSYS";

	} else if (vio_addr >= CONN_START_ADDR && vio_addr <= CONN_END_ADDR) {
		pr_info(PFX "[DEVAPC] bus_id might be wrong\n");

		if (domain == 0x1)
			return "MD";
		else if (domain == 0x2)
			return "ADSP";
	}


	if (slave_type == SLAVE_TYPE_INFRA) {
		if (vio_addr <= 0x1FFFFF || shift_sta_bit == 7) {
			pr_info(PFX "vio_addr is from on-chip SRAMROM\n");
			if ((bus_id & 0x1) == 0)
				return "EMI_L2C_M";

			return infra_mi_trans(bus_id >> 1);

		} else if (shift_sta_bit >= 0 && shift_sta_bit <= 3) {
			return peri_mi_trans(bus_id);

		} else if (shift_sta_bit >= 4 && shift_sta_bit <= 6) {
			return infra_mi_trans(bus_id);

		} else if (shift_sta_bit == 8) {
			pr_info(PFX "vio_addr is from MMSYS_MALI\n");
			if ((bus_id & 0x1) == 1)
				return "GCE_M";

			return infra_mi_trans(bus_id >> 1);
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

	/* Filter by violation address */
	if ((vio_addr & 0xFF000000) == CONN_START_ADDR)
		return "CONNSYS";

	/* Filter by violation index */
	if (slave_type == SLAVE_TYPE_INFRA &&
			vio_index < VIO_SLAVE_NUM_INFRA) {
		for (i = 0; i < VIO_SLAVE_NUM_INFRA; i++) {
			if (vio_index == mt6853_devices_infra[i].vio_index)
				return mt6853_devices_infra[i].device;
		}

	} else if (slave_type == SLAVE_TYPE_PERI &&
			vio_index < VIO_SLAVE_NUM_PERI)
		for (i = 0; i < VIO_SLAVE_NUM_PERI; i++) {
			if (vio_index == mt6853_devices_peri[i].vio_index)
				return mt6853_devices_peri[i].device;
		}

	else if (slave_type == SLAVE_TYPE_PERI2 &&
			vio_index < VIO_SLAVE_NUM_PERI2)
		for (i = 0; i < VIO_SLAVE_NUM_PERI2; i++) {
			if (vio_index == mt6853_devices_peri2[i].vio_index)
				return mt6853_devices_peri2[i].device;
		}

	else if (slave_type == SLAVE_TYPE_PERI_PAR &&
			vio_index < VIO_SLAVE_NUM_PERI_PAR)
		for (i = 0; i < VIO_SLAVE_NUM_PERI_PAR; i++) {
			if (vio_index == mt6853_devices_peri_par[i].vio_index)
				return mt6853_devices_peri_par[i].device;
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

static uint32_t mt6853_shift_group_get(int slave_type, uint32_t vio_idx)
{
	if (slave_type == SLAVE_TYPE_INFRA) {
		if (vio_idx >= 0 && vio_idx <= 3)
			return 0;
		else if (vio_idx >= 4 && vio_idx <= 5)
			return 1;
		else if ((vio_idx >= 6 && vio_idx <= 31) ||
			 (vio_idx >= 344 && vio_idx <= 370) ||
			 vio_idx == 390)
			return 2;
		else if ((vio_idx >= 32 && vio_idx <= 45) ||
			 (vio_idx >= 371 && vio_idx <= 385) ||
			 vio_idx == 391)
			return 3;
		else if ((vio_idx >= 46 && vio_idx <= 54) || vio_idx == 386)
			return 4;
		else if ((vio_idx >= 55 && vio_idx <= 60) || vio_idx == 387)
			return 5;
		else if ((vio_idx >= 61 && vio_idx <= 65) || vio_idx == 388)
			return 6;
		else if ((vio_idx >= 66 && vio_idx <= 67) || vio_idx == 389)
			return 7;
		else if (vio_idx >= 68 && vio_idx <= 343)
			return 8;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI) {
		if ((vio_idx >= 0 && vio_idx <= 2) ||
		    (vio_idx >= 132 && vio_idx <= 135) ||
		    vio_idx == 186)
			return 0;
		else if ((vio_idx >= 3 && vio_idx <= 25) || vio_idx == 136)
			return 1;
		else if ((vio_idx >= 26 && vio_idx <= 36) || vio_idx == 137)
			return 2;
		else if ((vio_idx >= 37 && vio_idx <= 38) || vio_idx == 138)
			return 3;
		else if ((vio_idx >= 39 && vio_idx <= 42) || vio_idx == 139)
			return 4;
		else if (vio_idx == 43 || vio_idx == 140)
			return 5;
		else if ((vio_idx >= 44 && vio_idx <= 86) || vio_idx == 141)
			return 6;
		else if ((vio_idx >= 87 && vio_idx <= 89) || vio_idx == 142)
			return 7;
		else if (vio_idx == 90 || vio_idx == 143 ||
			 vio_idx == 144 || vio_idx == 187)
			return 8;
		if (vio_idx >= 91 && vio_idx <= 92)
			return 9;
		else if ((vio_idx >= 93 && vio_idx <= 104) ||
			 (vio_idx >= 145 && vio_idx <= 156) ||
			 vio_idx == 188)
			return 10;
		else if ((vio_idx >= 105 && vio_idx <= 119) ||
			 (vio_idx >= 157 && vio_idx <= 172) ||
			 vio_idx == 189)
			return 11;
		else if ((vio_idx >= 120 && vio_idx <= 131) ||
			 (vio_idx >= 174 && vio_idx <= 185) ||
			 vio_idx == 153 || vio_idx == 190)
			return 12;

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
		else if ((vio_idx >= 75 && vio_idx <= 94) ||
			 (vio_idx >= 179 && vio_idx <= 199) ||
			 vio_idx == 218)
			return 8;
		else if ((vio_idx >= 95 && vio_idx <= 105) ||
			 (vio_idx >= 200 && vio_idx <= 211) ||
			 vio_idx == 219)
			return 9;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI_PAR) {
		if ((vio_idx >= 0 && vio_idx <= 2) ||
		    (vio_idx >= 29 && vio_idx <= 30) ||
		    vio_idx == 60)
			return 0;
		else if ((vio_idx >= 3 && vio_idx <= 26) ||
			 (vio_idx >= 31 && vio_idx <= 55) ||
			 vio_idx == 61)
			return 1;
		else if ((vio_idx >= 27 && vio_idx <= 29) ||
			 (vio_idx >= 56 && vio_idx <= 59) ||
			 vio_idx == 62)
			return 2;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	}

	pr_err(PFX "%s:%d Wrong slave_type:0x%x\n",
			__func__, __LINE__, slave_type);

	return 31;
}

void devapc_catch_illegal_range(phys_addr_t phys_addr, size_t size)
{
	/*
	 * Catch BROM addr mapped
	 */
	if (phys_addr >= 0x0 && phys_addr < SRAM_START_ADDR) {
		pr_err(PFX "%s: %s %s:(%pa), %s:(0x%lx)\n",
				"catch BROM address mapped!",
				__func__, "phys_addr", &phys_addr,
				"size", size);
		BUG_ON(1);
	}
}

static struct mtk_devapc_dbg_status mt6853_devapc_dbg_stat = {
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

static struct mtk_devapc_vio_info mt6853_devapc_vio_info = {
	.vio_mask_sta_num = mtk_vio_mask_sta_num,
	.sramrom_vio_idx = SRAMROM_VIO_INDEX,
	.mdp_vio_idx = MDP_VIO_INDEX,
	.disp2_vio_idx = MDP_VIO_INDEX,
	.mmsys_vio_idx = MMSYS_VIO_INDEX,
	.sramrom_slv_type = SRAMROM_SLAVE_TYPE,
	.mm2nd_slv_type = MM2ND_SLAVE_TYPE,
};

static const struct mtk_infra_vio_dbg_desc mt6853_vio_dbgs = {
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

static const struct mtk_sramrom_sec_vio_desc mt6853_sramrom_sec_vios = {
	.vio_id_mask = SRAMROM_SEC_VIO_ID_MASK,
	.vio_id_shift = SRAMROM_SEC_VIO_ID_SHIFT,
	.vio_domain_mask = SRAMROM_SEC_VIO_DOMAIN_MASK,
	.vio_domain_shift = SRAMROM_SEC_VIO_DOMAIN_SHIFT,
	.vio_rw_mask = SRAMROM_SEC_VIO_RW_MASK,
	.vio_rw_shift = SRAMROM_SEC_VIO_RW_SHIFT,
};

static const uint32_t mt6853_devapc_pds[] = {
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

static struct mtk_devapc_soc mt6853_data = {
	.dbg_stat = &mt6853_devapc_dbg_stat,
	.slave_type_arr = slave_type_to_str,
	.slave_type_num = SLAVE_TYPE_NUM,
	.device_info[SLAVE_TYPE_INFRA] = mt6853_devices_infra,
	.device_info[SLAVE_TYPE_PERI] = mt6853_devices_peri,
	.device_info[SLAVE_TYPE_PERI2] = mt6853_devices_peri2,
	.device_info[SLAVE_TYPE_PERI_PAR] = mt6853_devices_peri_par,
	.ndevices = mtk6853_devices_num,
	.vio_info = &mt6853_devapc_vio_info,
	.vio_dbgs = &mt6853_vio_dbgs,
	.sramrom_sec_vios = &mt6853_sramrom_sec_vios,
	.devapc_pds = mt6853_devapc_pds,
	.subsys_get = &index_to_subsys,
	.master_get = &mt6853_bus_id_to_master,
	.mm2nd_vio_handler = &mm2nd_vio_handler,
	.shift_group_get = mt6853_shift_group_get,
};

static const struct of_device_id mt6853_devapc_dt_match[] = {
	{ .compatible = "mediatek,mt6853-devapc" },
	{},
};

static int mt6853_devapc_probe(struct platform_device *pdev)
{
	return mtk_devapc_probe(pdev, &mt6853_data);
}

static int mt6853_devapc_remove(struct platform_device *dev)
{
	return mtk_devapc_remove(dev);
}

static struct platform_driver mt6853_devapc_driver = {
	.probe = mt6853_devapc_probe,
	.remove = mt6853_devapc_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = mt6853_devapc_dt_match,
	},
};

module_platform_driver(mt6853_devapc_driver);

MODULE_DESCRIPTION("Mediatek MT6853 Device APC Driver");
MODULE_AUTHOR("Neal Liu <neal.liu@mediatek.com>");
MODULE_LICENSE("GPL");
