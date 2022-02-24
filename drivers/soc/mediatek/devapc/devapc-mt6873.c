// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/bug.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <asm-generic/io.h>

#include "devapc-mt6873.h"

static struct mtk_device_num mtk6873_devices_num[] = {
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
	{"SPM_DRAMC",    { 1, 1, 0 } },
};

static struct INFRAAXI_ID_INFO infra_mi_id_to_master[] = {
	{"CONNSYS_WFDMA",     { 0, 0, 0, 0,	0, 0, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"CONNSYS_ICAP",      { 0, 0, 0, 0,	1, 0, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"CONNSYS_MCU_SYS",   { 0, 0, 0, 0,	0, 1, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"CONNSYS_GPS",       { 0, 0, 0, 0,	1, 1, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"Tinysys",           { 0, 1, 0, 0,	2, 2, 2, 2,	2, 2, 0, 0,	0, 0 } },
	{"CQ_DMA",            { 0, 0, 1, 0,	0, 0, 0, 2,	2, 2, 0, 0,	0, 0 } },
	{"DebugTop",          { 0, 0, 1, 0,	1, 0, 0, 2,	0, 0, 0, 0,	0, 0 } },
	{"SSUSB",             { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 0, 0,	2, 0 } },
	{"SSUSB2",            { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 1, 0,	2, 0 } },
	{"NOR",               { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 0, 1,	2, 0 } },
	{"PWM",               { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 1, 1,	0, 0 } },
	{"SPI6",              { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 1, 1,	0, 0 } },
	{"SPI0",              { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 1, 1,	1, 0 } },
	{"APU",               { 0, 0, 1, 0,	0, 1, 0, 1,	0, 0, 2, 2,	0, 0 } },
	{"SPI2",              { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 0, 0,	0, 0 } },
	{"SPI3",              { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 1, 0,	0, 0 } },
	{"SPI4",              { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 0, 1,	0, 0 } },
	{"SPI5",              { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 1, 1,	0, 0 } },
	{"SPI7",              { 0, 0, 1, 0,	0, 1, 0, 1,	1, 0, 0, 0,	0, 0 } },
	{"Audio",             { 0, 0, 1, 0,	0, 1, 0, 1,	1, 0, 0, 1,	0, 0 } },
	{"SPI1",              { 0, 0, 1, 0,	0, 1, 0, 1,	1, 0, 1, 1,	0, 0 } },
	{"AP_DMA_EXT",        { 0, 0, 1, 0,	0, 1, 0, 0,	0, 1, 2, 2,	2, 0 } },
	{"THERM2",            { 0, 0, 1, 0,	0, 1, 0, 1,	0, 1, 0, 0,	0, 0 } },
	{"SPM",               { 0, 0, 1, 0,	0, 1, 0, 1,	0, 1, 1, 0,	0, 0 } },
	{"CCU",               { 0, 0, 1, 0,	0, 1, 0, 1,	0, 1, 0, 1,	0, 0 } },
	{"THERM",             { 0, 0, 1, 0,	0, 1, 0, 1,	0, 1, 1, 1,	0, 0 } },
	{"DX_CC",             { 0, 0, 1, 0,	1, 1, 0, 2,	2, 2, 2, 0,	0, 0 } },
	{"GCE",               { 0, 0, 1, 0,	0, 0, 1, 2,	2, 0, 0, 0,	0, 0 } },
	{"PCIE",              { 0, 0, 1, 0,	0, 1, 1, 2,	2, 2, 2, 2,	0, 0 } },
	{"DPMAIF",            { 0, 1, 1, 0,	2, 2, 2, 2,	0, 0, 0, 0,	0, 0 } },
	{"SSPM",              { 0, 0, 0, 1,	2, 2, 2, 0,	0, 0, 0, 0,	0, 0 } },
	{"UFS",               { 0, 1, 0, 1,	0, 2, 2, 0,	0, 0, 0, 0,	0, 0 } },
	{"MSDC0",             { 0, 1, 0, 1,	1, 0, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"MSDC1",             { 0, 1, 0, 1,	1, 1, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"MSDC2",             { 0, 1, 0, 1,	1, 1, 1, 0,	0, 0, 0, 0,	0, 0 } },
	{"CPUEB",             { 0, 0, 1, 1,	2, 2, 2, 2,	2, 2, 0, 0,	0, 0 } },
	{"APMCU_write",       { 1, 2, 2, 2,	2, 0, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_write",       { 1, 2, 2, 2,	2, 0, 0, 1,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_write",       { 1, 2, 2, 2,	2, 2, 2, 2,	2, 1, 0, 0,	0, 0 } },
	{"APMCU_read",        { 1, 2, 2, 2,	2, 0, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_read",        { 1, 2, 2, 2,	2, 0, 0, 1,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_read",        { 1, 2, 0, 0,	0, 0, 0, 0,	1, 0, 0, 0,	0, 0 } },
	{"APMCU_read",        { 1, 2, 1, 0,	0, 0, 0, 0,	1, 0, 0, 0,	0, 0 } },
	{"APMCU_read",        { 1, 2, 2, 2,	2, 2, 2, 2,	2, 1, 0, 0,	0, 0 } },
	{"APMCU_read",        { 1, 2, 2, 2,	2, 2, 2, 2,	2, 2, 1, 0,	0, 0 } },
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

static const char *mt6873_bus_id_to_master(uint32_t bus_id, uint32_t vio_addr,
		int slave_type, int shift_sta_bit, int domain)
{
	uint8_t h_1byte;

	pr_debug(PFX "[DEVAPC] %s:0x%x, %s:0x%x, %s:0x%x, %s:%d\n",
		"bus_id", bus_id, "vio_addr", vio_addr,
		"slave_type", slave_type,
		"shift_sta_bit", shift_sta_bit);

	if (bus_id == 0x0 && vio_addr == 0x0)
		return NULL;

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
		if (vio_addr <= 0x1FFFFF) {
			pr_info(PFX "vio_addr is from on-chip SRAMROM\n");
			if ((bus_id & 0x1) == 0)
				return "EMI_L2C_M";

			return infra_mi_trans(bus_id >> 1);

		} else if (shift_sta_bit == 3) {
			if ((bus_id & 0x1) == 0)
				return "EMI_L2C_M";

			return infra_mi_trans(bus_id >> 1);

		} else if (shift_sta_bit == 4) {
			if ((bus_id & 0x1) == 1)
				return "GCE_M";

			return infra_mi_trans(bus_id >> 1);
		}

		return infra_mi_trans(bus_id);

	} else if (slave_type == SLAVE_TYPE_PERI) {
		if ((h_1byte >= 0x14 && h_1byte < 0x18) ||
				(h_1byte >= 0x1A && h_1byte < 0x1C) ||
				(h_1byte >= 0x1F && h_1byte < 0x20)) {
			pr_info(PFX "vio addr is from MM 2nd\n");
			if ((bus_id & 0x1) == 1)
				return "GCE_M";

			return infra_mi_trans(bus_id >> 1);
		}

		if (shift_sta_bit == 3 || shift_sta_bit == 4 ||
				shift_sta_bit == 8) {
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

	return "UNKNOWN_MASTER";
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
			if (vio_index == mt6873_devices_infra[i].vio_index)
				return mt6873_devices_infra[i].device;
		}

	} else if (slave_type == SLAVE_TYPE_PERI &&
			vio_index < VIO_SLAVE_NUM_PERI)
		for (i = 0; i < VIO_SLAVE_NUM_PERI; i++) {
			if (vio_index == mt6873_devices_peri[i].vio_index)
				return mt6873_devices_peri[i].device;
		}

	else if (slave_type == SLAVE_TYPE_PERI2 &&
			vio_index < VIO_SLAVE_NUM_PERI2)
		for (i = 0; i < VIO_SLAVE_NUM_PERI2; i++) {
			if (vio_index == mt6873_devices_peri2[i].vio_index)
				return mt6873_devices_peri2[i].device;
		}

	else if (slave_type == SLAVE_TYPE_PERI_PAR &&
			vio_index < VIO_SLAVE_NUM_PERI_PAR)
		for (i = 0; i < VIO_SLAVE_NUM_PERI_PAR; i++) {
			if (vio_index == mt6873_devices_peri_par[i].vio_index)
				return mt6873_devices_peri_par[i].device;
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

		strncpy(mm_str, "INFRACFG_MDP_SEC_VIO",
				sizeof("INFRACFG_MDP_SEC_VIO"));

	} else if (mmsys_vio) {
		vio_sta_num = INFRACFG_MM_VIO_STA_NUM;
		vio0_offset = INFRACFG_MM_SEC_VIO0_OFFSET;

		strncpy(mm_str, "INFRACFG_MM_SEC_VIO",
				sizeof("INFRACFG_MM_SEC_VIO"));

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

static uint32_t mt6873_shift_group_get(int slave_type, uint32_t vio_idx)
{
	if (slave_type == SLAVE_TYPE_INFRA) {
		if ((vio_idx >= 0 && vio_idx <= 8) || vio_idx == 355)
			return 0;
		else if ((vio_idx >= 9 && vio_idx <= 14) || vio_idx == 356)
			return 1;
		else if ((vio_idx >= 15 && vio_idx <= 19) || vio_idx == 357)
			return 2;
		else if ((vio_idx >= 20 && vio_idx <= 21) || vio_idx == 358)
			return 3;
		else if (vio_idx >= 22 && vio_idx <= 347)
			return 4;
		else if ((vio_idx >= 348 && vio_idx <= 354) ||
				(vio_idx >= 359 && vio_idx <= 365) ||
				vio_idx == 366)
			return 5;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI) {
		if (vio_idx >= 0 && vio_idx <= 4)
			return 0;
		else if (vio_idx >= 5 && vio_idx <= 6)
			return 1;
		else if ((vio_idx >= 7 && vio_idx <= 38) || vio_idx == 187 ||
				(vio_idx >= 188 && vio_idx <= 219) ||
				vio_idx == 286)
			return 2;
		else if ((vio_idx >= 39 && vio_idx <= 61) || vio_idx == 220)
			return 3;
		else if ((vio_idx >= 62 && vio_idx <= 72) || vio_idx == 221)
			return 4;
		else if ((vio_idx >= 73 && vio_idx <= 74) || vio_idx == 222)
			return 5;
		else if (vio_idx == 75 || vio_idx == 223)
			return 6;
		else if ((vio_idx >= 76 && vio_idx <= 118) || vio_idx == 224)
			return 7;
		else if ((vio_idx >= 119 && vio_idx <= 121) || vio_idx == 225)
			return 8;
		if (vio_idx >= 122 && vio_idx <= 125)
			return 9;
		else if (vio_idx == 126 || (vio_idx >= 226 && vio_idx <= 227) ||
				vio_idx == 287)
			return 10;
		if (vio_idx >= 127 && vio_idx <= 128)
			return 11;
		if (vio_idx >= 129 && vio_idx <= 130)
			return 12;
		else if ((vio_idx >= 131 && vio_idx <= 141) ||
				(vio_idx >= 228 && vio_idx <= 238) ||
				vio_idx == 288)
			return 13;
		else if ((vio_idx >= 142 && vio_idx <= 143) ||
				(vio_idx >= 239 && vio_idx <= 240) ||
				vio_idx == 289)
			return 14;
		else if ((vio_idx >= 144 && vio_idx <= 173) || vio_idx == 241 ||
				(vio_idx >= 242 && vio_idx <= 271) ||
				vio_idx == 290)
			return 15;
		else if ((vio_idx >= 174 && vio_idx <= 186) || vio_idx == 272 ||
				(vio_idx >= 273 && vio_idx <= 285) ||
				vio_idx == 291)
			return 16;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI2) {
		if ((vio_idx >= 0 && vio_idx <= 12) || vio_idx == 117 ||
				(vio_idx >= 118 && vio_idx <= 130) ||
				vio_idx == 234)
			return 0;
		else if (vio_idx >= 13 && vio_idx <= 16)
			return 1;
		else if (vio_idx >= 17 && vio_idx <= 20)
			return 2;
		else if ((vio_idx >= 21 && vio_idx <= 36) || vio_idx == 131 ||
				(vio_idx >= 132 && vio_idx <= 147) ||
				vio_idx == 235)
			return 3;
		else if ((vio_idx >= 37 && vio_idx <= 44) || vio_idx == 148 ||
				(vio_idx >= 149 && vio_idx <= 156) ||
				vio_idx == 236)
			return 4;
		else if ((vio_idx >= 45 && vio_idx <= 60) || vio_idx == 157 ||
				(vio_idx >= 158 && vio_idx <= 173) ||
				vio_idx == 237)
			return 5;
		else if ((vio_idx >= 61 && vio_idx <= 76) || vio_idx == 174 ||
				(vio_idx >= 175 && vio_idx <= 190) ||
				vio_idx == 238)
			return 6;
		else if ((vio_idx >= 77 && vio_idx <= 84) || vio_idx == 191 ||
				(vio_idx >= 192 && vio_idx <= 199) ||
				vio_idx == 239)
			return 7;
		else if ((vio_idx >= 85 && vio_idx <= 105) || vio_idx == 200 ||
				(vio_idx >= 201 && vio_idx <= 221) ||
				vio_idx == 240)
			return 8;
		else if ((vio_idx >= 106 && vio_idx <= 116) || vio_idx == 222 ||
				(vio_idx >= 223 && vio_idx <= 233) ||
				vio_idx == 241)
			return 9;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI_PAR) {
		if ((vio_idx >= 0 && vio_idx <= 23) || vio_idx == 27 ||
				(vio_idx >= 28 && vio_idx <= 51) ||
				vio_idx == 56)
			return 0;
		else if ((vio_idx >= 24 && vio_idx <= 26) || vio_idx == 52 ||
				(vio_idx >= 53 && vio_idx <= 55) ||
				vio_idx == 57)
			return 1;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else {
		pr_err(PFX "%s:%d Wrong slave_type:0x%x\n",
				__func__, __LINE__, slave_type);
	}

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
EXPORT_SYMBOL(devapc_catch_illegal_range);

static struct mtk_devapc_dbg_status mt6873_devapc_dbg_stat = {
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

static struct mtk_devapc_vio_info mt6873_devapc_vio_info = {
	.vio_mask_sta_num = mtk_vio_mask_sta_num,
	.sramrom_vio_idx = SRAMROM_VIO_INDEX,
	.mdp_vio_idx = MDP_VIO_INDEX,
	.disp2_vio_idx = MDP_VIO_INDEX,
	.mmsys_vio_idx = MMSYS_VIO_INDEX,
	.sramrom_slv_type = SRAMROM_SLAVE_TYPE,
	.mm2nd_slv_type = MM2ND_SLAVE_TYPE,
};

static const struct mtk_infra_vio_dbg_desc mt6873_vio_dbgs = {
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

static const struct mtk_sramrom_sec_vio_desc mt6873_sramrom_sec_vios = {
	.vio_id_mask = SRAMROM_SEC_VIO_ID_MASK,
	.vio_id_shift = SRAMROM_SEC_VIO_ID_SHIFT,
	.vio_domain_mask = SRAMROM_SEC_VIO_DOMAIN_MASK,
	.vio_domain_shift = SRAMROM_SEC_VIO_DOMAIN_SHIFT,
	.vio_rw_mask = SRAMROM_SEC_VIO_RW_MASK,
	.vio_rw_shift = SRAMROM_SEC_VIO_RW_SHIFT,
};

static const uint32_t mt6873_devapc_pds[] = {
	PD_VIO_MASK_OFFSET,
	PD_VIO_STA_OFFSET,
	PD_VIO_DBG0_OFFSET,
	PD_VIO_DBG1_OFFSET,
	PD_VIO_DBG2_OFFSET,
	PD_APC_CON_OFFSET,
	PD_SHIFT_STA_OFFSET,
	PD_SHIFT_SEL_OFFSET,
	PD_SHIFT_CON_OFFSET,
};

static struct mtk_devapc_soc mt6873_data = {
	.dbg_stat = &mt6873_devapc_dbg_stat,
	.slave_type_arr = slave_type_to_str,
	.slave_type_num = SLAVE_TYPE_NUM,
	.device_info[SLAVE_TYPE_INFRA] = mt6873_devices_infra,
	.device_info[SLAVE_TYPE_PERI] = mt6873_devices_peri,
	.device_info[SLAVE_TYPE_PERI2] = mt6873_devices_peri2,
	.device_info[SLAVE_TYPE_PERI_PAR] = mt6873_devices_peri_par,
	.ndevices = mtk6873_devices_num,
	.vio_info = &mt6873_devapc_vio_info,
	.vio_dbgs = &mt6873_vio_dbgs,
	.sramrom_sec_vios = &mt6873_sramrom_sec_vios,
	.devapc_pds = mt6873_devapc_pds,
	.subsys_get = &index_to_subsys,
	.master_get = &mt6873_bus_id_to_master,
	.mm2nd_vio_handler = &mm2nd_vio_handler,
	.shift_group_get = mt6873_shift_group_get,
};

static const struct of_device_id mt6873_devapc_dt_match[] = {
	{ .compatible = "mediatek,mt6873-devapc" },
	{},
};

static int mt6873_devapc_probe(struct platform_device *pdev)
{
	return mtk_devapc_probe(pdev, &mt6873_data);
}

static int mt6873_devapc_remove(struct platform_device *dev)
{
	return mtk_devapc_remove(dev);
}

static struct platform_driver mt6873_devapc_driver = {
	.probe = mt6873_devapc_probe,
	.remove = mt6873_devapc_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = mt6873_devapc_dt_match,
	},
};

module_platform_driver(mt6873_devapc_driver);

MODULE_DESCRIPTION("Mediatek MT6873 Device APC Driver");
MODULE_AUTHOR("Neal Liu <neal.liu@mediatek.com>");
MODULE_LICENSE("GPL");
