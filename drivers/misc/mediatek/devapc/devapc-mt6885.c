// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/bug.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <asm-generic/io.h>

#include "devapc-mt6885.h"

static struct mtk_device_num mtk6885_devices_num[] = {
	{SLAVE_TYPE_INFRA, VIO_SLAVE_NUM_INFRA},
	{SLAVE_TYPE_PERI, VIO_SLAVE_NUM_PERI},
	{SLAVE_TYPE_PERI2, VIO_SLAVE_NUM_PERI2},
};

static struct PERIAXI_ID_INFO peri_mi_id_to_master[] = {
	{"THERM2",       { 0, 0, 0 } },
	{"SPM",          { 0, 1, 0 } },
	{"CCU",          { 0, 0, 1 } },
	{"THERM",        { 0, 1, 1 } },
	{"SPM_DRAMC",    { 1, 1, 0 } },
};

static struct INFRAAXI_ID_INFO infra_mi_id_to_master[] = {
	{"CONNSYS_WFDMA",     { 0, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2 } },
	{"CONNSYS_ICAP",      { 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2 } },
	{"CONNSYS_WF_MCU",    { 0, 0, 0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 2 } },
	{"CONNSYS_BT_MCU",    { 0, 0, 0, 0, 1, 0, 0, 1, 2, 2, 2, 2, 2 } },
	{"CONNSYS_GPS",       { 0, 0, 0, 0, 1, 0, 0, 0, 2, 2, 2, 2, 2 } },
	{"Tinysys",           { 0, 1, 0, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0 } },
	{"GCE_M2",            { 0, 0, 1, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0 } },
	{"CQ_DMA",            { 0, 0, 1, 0, 1, 0, 0, 2, 2, 2, 0, 0, 0 } },
	{"DebugTop",          { 0, 0, 1, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0 } },
	{"SSUSB",             { 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 2, 2 } },
	{"PWM",               { 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0 } },
	{"MSDC1",             { 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0 } },
	{"SPI6",              { 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1 } },
	{"SPI0",              { 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1 } },
	{"APU",               { 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 2, 2, 0 } },
	{"MSDC0",             { 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 2, 2, 0 } },
	{"SPI2",              { 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0 } },
	{"SPI3",              { 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0 } },
	{"SPI4",              { 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0 } },
	{"SPI5",              { 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0 } },
	{"SPI7",              { 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0 } },
	{"Audio",             { 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0 } },
	{"SPI1",              { 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0 } },
	{"AP_DMA_EXT",        { 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 2, 2, 2 } },
	{"THERM2",            { 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0 } },
	{"SPM",               { 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0 } },
	{"CCU",               { 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0 } },
	{"THERM",             { 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0 } },
	{"DX_CC",             { 0, 0, 1, 0, 0, 0, 1, 2, 2, 2, 2, 0, 0 } },
	{"GCE_M",             { 0, 0, 1, 0, 1, 0, 1, 2, 2, 0, 0, 0, 0 } },
	{"DPMAIF",            { 0, 1, 1, 0, 2, 2, 2, 2, 0, 0, 0, 0, 0 } },
	{"MCUPM",             { 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"SSPM",              { 0, 1, 0, 1, 2, 2, 2, 0, 0, 0, 0, 0, 0 } },
	{"UFS",               { 0, 0, 1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"APMCU_write",       { 1, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"APMCU_write",       { 1, 2, 2, 2, 2, 0, 0, 1, 0, 0, 0, 0, 0 } },
	{"APMCU_write",       { 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0 } },
	{"APMCU_read",        { 1, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"APMCU_read",        { 1, 2, 2, 2, 2, 0, 0, 1, 0, 0, 0, 0, 0 } },
	{"APMCU_read",        { 1, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 } },
	{"APMCU_read",        { 1, 2, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 } },
	{"APMCU_read",        { 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0 } },
	{"APMCU_read",        { 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0 } },
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

static const char *mt6885_bus_id_to_master(uint32_t bus_id, uint32_t vio_addr,
		int slave_type, int shift_sta_bit, int domain)
{
	uint16_t h_2byte;
	uint8_t h_1byte;

	UNUSED(slave_type);
	UNUSED(shift_sta_bit);

	pr_debug(PFX "[DEVAPC] bus_id:0x%x, vio_addr:0x%x\n",
		bus_id, vio_addr);

	if (bus_id == 0x0 && vio_addr == 0x0)
		return NULL;

	/* bus only reference bit 0~29 */
	vio_addr = vio_addr & 0x3FFFFFFF;

	h_1byte = (vio_addr >> 24) & 0xFF;
	h_2byte = (vio_addr >> 16) & 0xFFFF;

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

	if (h_2byte < 0x0C51 || (h_2byte >= 0x0C80 && h_2byte < 0x0CC0)) {
		pr_debug(PFX "vio_addr is from L3C or on-chip SRAMROM\n");
		if ((bus_id & 0x1) == 1)
			return "EMI_L2C_M";

		return infra_mi_trans(bus_id >> 1);

	} else if (h_1byte == 0x15 || h_1byte == 0x1A || h_1byte == 0x1B ||
			h_1byte == 0x1F) {
		pr_debug(PFX "vio_addr is from MDP System\n");
		if ((bus_id & 0x1) == 0)
			return "GCE_M2";

		return infra_mi_trans(bus_id >> 1);

	} else if (h_2byte >= 0x1410 && h_2byte < 0x1420) {
		pr_debug(PFX "vio_addr is from DISP-2 MM System\n");
		if ((bus_id & 0x1) == 1)
			return "GCE_M";
		else if ((bus_id >> 3) == 0)
			pr_info(PFX "Master might be %s\n", "GCE_M2");

		return infra_mi_trans(bus_id >> 1);

	} else if (h_1byte == 0x14 || h_1byte == 0x16 || h_1byte == 0x17) {
		pr_debug(PFX "vio_addr is from DISP MM System\n");
		if ((bus_id & 0x1) == 1)
			return "GCE_M";

		return infra_mi_trans(bus_id >> 1);

	} else if (h_1byte == 0x13 || h_1byte == 0x19) {
		pr_debug(PFX "vio_addr is from APU or GPU\n");
		return infra_mi_trans(bus_id);

	} else if (h_2byte >= 0x1050 && h_2byte < 0x1090) {
		pr_debug(PFX "vio_addr is from Tinysys\n");
		if ((bus_id & 0x1) == 0)
			return "MD_AP_M";

		return peri_mi_trans(bus_id >> 1);

	} else if (h_2byte >= 0x1040 && h_2byte < 0x1050) {
		pr_debug(PFX "vio_addr is from PWR_MD32\n");
		if ((bus_id & 0x1) == 0)
			return "MD_AP_M";

		return peri_mi_trans(bus_id >> 1);

	} else if (h_2byte >= 0x1120 && h_2byte < 0x1121) {
		pr_debug(PFX "vio_addr is from SSUSB\n");
		if ((bus_id & 0x1) == 0)
			return "MD_AP_M";

		return peri_mi_trans(bus_id >> 1);

	} else {
		return peri_mi_trans(bus_id);
	}
}

/* violation index corresponds to subsys */
const char *index_to_subsys(int slave_type, uint32_t vio_index,
		uint32_t vio_addr)
{
	int i;

	if (slave_type == SLAVE_TYPE_INFRA &&
			vio_index < VIO_SLAVE_NUM_INFRA) {

		/* check violation address */
		if (vio_addr >= MFG_START_ADDR && vio_addr <= MFG_END_ADDR)
			return "MFGSYS";

		/* check violation index */
		if (vio_index == SMI_LARB0_VIO_INDEX ||
				vio_index == SMI_LARB1_VIO_INDEX ||
				vio_index == SMI_LARB2_VIO_INDEX ||
				vio_index == SMI_LARB3_VIO_INDEX ||
				vio_index == SMI_LARB4_VIO_INDEX ||
				vio_index == SMI_LARB5_VIO_INDEX ||
				vio_index == SMI_LARB6_VIO_INDEX ||
				vio_index == SMI_LARB7_VIO_INDEX ||
				vio_index == SMI_LARB8_VIO_INDEX ||
				vio_index == SMI_LARB9_VIO_INDEX ||
				vio_index == SMI_LARB10_VIO_INDEX ||
				vio_index == SMI_LARB11_VIO_INDEX ||
				vio_index == SMI_LARB12_VIO_INDEX ||
				vio_index == SMI_LARB13_VIO_INDEX ||
				vio_index == SMI_LARB14_VIO_INDEX ||
				vio_index == SMI_LARB15_VIO_INDEX ||
				vio_index == SMI_LARB16_VIO_INDEX ||
				vio_index == SMI_LARB17_VIO_INDEX ||
				vio_index == SMI_LARB18_VIO_INDEX ||
				vio_index == SMI_LARB19_VIO_INDEX ||
				vio_index == SMI_LARB20_VIO_INDEX)
			return "SMI";

		if (vio_index == IOMMU0_VIO_INDEX ||
				vio_index == IOMMU1_VIO_INDEX ||
				vio_index == IOMMU0_SEC_VIO_INDEX ||
				vio_index == IOMMU1_SEC_VIO_INDEX)
			return "IOMMU";

		else if (vio_index >= CAM_SENINF_START &&
				vio_index <= CAM_SENINF_END)
			return "CAMSYS_SENINF";

		switch (vio_index) {
		case MFG_START ... MFG_END:
			return "MFGSYS";
		case MM_DISP_START ... MM_DISP_END:
		case MM_DISP2_START ... MM_DISP2_END:
			return "MMSYS_DISP";
		case MM_SSRAM_VIO_INDEX:
		case MM_MDP_START ... MM_MDP_END:
			return "MMSYS_MDP";
		case IMG_START ... IMG_END:
			return "IMGSYS";
		case VDEC_START ... VDEC_END:
			return "VDECSYS";
		case VENC_START ... VENC_END:
			return "VENCSYS";
		case APU_SSRAM_VIO_INDEX:
		case APU_START ... APU_END:
			return "APUSYS";
		case CAM_CCU_START ... CAM_CCU_END:
			return "CAMSYS_CCU";
		case CAM_START ... CAM_END:
			return "CAMSYS";
		case IPE_START ... IPE_END:
			return "IPESYS";
		case MDP_START ... MDP_END:
			return "MMSYS_MDP";
		default:
			break;
		}

		for (i = 0; i < VIO_SLAVE_NUM_INFRA; i++) {
			if (vio_index == mt6885_devices_infra[i].vio_index)
				return mt6885_devices_infra[i].device;
		}

	} else if (slave_type == SLAVE_TYPE_PERI &&
			vio_index < VIO_SLAVE_NUM_PERI) {

		/* check violation index */
		switch (vio_index) {
		case TINY_START ... TINY_END:
			return "TINYSYS";
		case MD_START ... MD_END:
			return "MDSYS";
		case CONN_VIO_INDEX:
			return "CONNSYS";
		default:
			break;
		}

		for (i = 0; i < VIO_SLAVE_NUM_PERI; i++) {
			if (vio_index == mt6885_devices_peri[i].vio_index)
				return mt6885_devices_peri[i].device;
		}

	} else if (slave_type == SLAVE_TYPE_PERI2 &&
			vio_index < VIO_SLAVE_NUM_PERI2) {

		/* check violation address */
		if ((vio_addr >= GCE_START_ADDR && vio_addr <= GCE_END_ADDR) ||
				(vio_addr >= GCE_M2_START_ADDR &&
				 vio_addr <= GCE_M2_END_ADDR))
			return "GCE";

		for (i = 0; i < VIO_SLAVE_NUM_PERI2; i++) {
			if (vio_index == mt6885_devices_peri2[i].vio_index)
				return mt6885_devices_peri2[i].device;
		}
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

	} else if (disp2_vio) {
		vio_sta_num = INFRACFG_DISP2_VIO_STA_NUM;
		vio0_offset = INFRACFG_DISP2_SEC_VIO0_OFFSET;

		strncpy(mm_str, "INFRACFG_DISP2_SEC_VIO", sizeof(mm_str));

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

static uint32_t mt6885_shift_group_get(int slave_type, uint32_t vio_idx)
{
	if (slave_type == SLAVE_TYPE_INFRA) {
		if ((vio_idx >= 0 && vio_idx <= 8) || vio_idx == 412)
			return 0;
		else if ((vio_idx >= 9 && vio_idx <= 14) || vio_idx == 413)
			return 1;
		else if ((vio_idx >= 15 && vio_idx <= 16) || vio_idx == 414)
			return 2;
		else if ((vio_idx >= 17 && vio_idx <= 18) || vio_idx == 415)
			return 3;
		else if (vio_idx >= 19 && vio_idx <= 70)
			return 4;
		else if (vio_idx >= 71 && vio_idx <= 369)
			return 5;
		else if (vio_idx >= 370 && vio_idx <= 409)
			return 6;
		else if (vio_idx == 410 || vio_idx == 416)
			return 7;
		else if (vio_idx == 411 || vio_idx == 417)
			return 8;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI) {
		if (vio_idx >= 0 && vio_idx <= 4)
			return 0;
		else if (vio_idx >= 5 && vio_idx <= 6)
			return 1;
		else if ((vio_idx >= 7 && vio_idx <= 38) || vio_idx == 216 ||
				(vio_idx >= 217 && vio_idx <= 248) ||
				vio_idx == 346)
			return 2;
		else if ((vio_idx >= 39 && vio_idx <= 61) || vio_idx == 249)
			return 3;
		else if ((vio_idx >= 62 && vio_idx <= 72) || vio_idx == 250)
			return 4;
		else if ((vio_idx >= 73 && vio_idx <= 74) || vio_idx == 251)
			return 5;
		else if ((vio_idx >= 75 && vio_idx <= 78) || vio_idx == 252)
			return 6;
		else if ((vio_idx >= 79 && vio_idx <= 121) || vio_idx == 253)
			return 7;
		else if ((vio_idx >= 122 && vio_idx <= 124) || vio_idx == 254)
			return 8;
		else if (vio_idx == 125 || vio_idx == 255 ||
				vio_idx == 256 || vio_idx == 347)
			return 9;
		else if (vio_idx == 126 || vio_idx == 257)
			return 10;
		else if (vio_idx == 127 || vio_idx == 128)
			return 11;
		else if (vio_idx == 129 || vio_idx == 130)
			return 12;
		else if ((vio_idx >= 131 && vio_idx <= 142) ||
				(vio_idx >= 258 && vio_idx <= 269) ||
				vio_idx == 348)
			return 13;
		else if ((vio_idx >= 143 && vio_idx <= 172) || vio_idx == 270 ||
				(vio_idx >= 271 && vio_idx <= 300) ||
				vio_idx == 349)
			return 14;
		else if ((vio_idx >= 173 && vio_idx <= 202) || vio_idx == 301 ||
				(vio_idx >= 302 && vio_idx <= 331) ||
				vio_idx == 350)
			return 15;
		else if ((vio_idx >= 203 && vio_idx <= 215) ||
				(vio_idx >= 332 && vio_idx <= 345) ||
				vio_idx == 351)
			return 16;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI2) {
		if ((vio_idx >= 0 && vio_idx <= 8) ||
				(vio_idx >= 130 && vio_idx <= 139) ||
				vio_idx == 252)
			return 0;
		else if (vio_idx >= 9 && vio_idx <= 12)
			return 1;
		else if (vio_idx >= 13 && vio_idx <= 16)
			return 2;
		else if (vio_idx >= 17 && vio_idx <= 20)
			return 3;
		else if (vio_idx >= 21 && vio_idx <= 24)
			return 4;
		else if ((vio_idx >= 25 && vio_idx <= 40) ||
				(vio_idx >= 140 && vio_idx <= 156) ||
				vio_idx == 253)
			return 5;
		else if ((vio_idx >= 41 && vio_idx <= 48) ||
				(vio_idx >= 157 && vio_idx <= 165) ||
				vio_idx == 254)
			return 6;
		else if ((vio_idx >= 49 && vio_idx <= 64) ||
				(vio_idx >= 166 && vio_idx <= 182) ||
				vio_idx == 255)
			return 7;
		else if ((vio_idx >= 65 && vio_idx <= 80) ||
				(vio_idx >= 183 && vio_idx <= 199) ||
				vio_idx == 256)
			return 8;
		else if ((vio_idx >= 81 && vio_idx <= 88) ||
				(vio_idx >= 200 && vio_idx <= 208) ||
				vio_idx == 257)
			return 9;
		else if ((vio_idx >= 89 && vio_idx <= 111) ||
				(vio_idx >= 209 && vio_idx <= 232) ||
				vio_idx == 258)
			return 10;
		else if ((vio_idx >= 112 && vio_idx <= 40) ||
				(vio_idx >= 233 && vio_idx <= 251) ||
				vio_idx == 259)
			return 11;

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
	phys_addr_t test_pa = 0x17a54c50;

	/*
	 * Catch BROM addr mapped
	 */
	if (phys_addr >= 0x0 && phys_addr < SRAM_START_ADDR) {
		pr_err(PFX "%s %s:(%pa), %s:(0x%lx)\n",
				"catch BROM address mapped!",
				"phys_addr", &phys_addr,
				"size", size);
		BUG_ON(1);
	}

	if ((phys_addr <= test_pa) && (phys_addr + size > test_pa)) {
		pr_err(PFX "%s %s:(%pa), %s:(0x%lx), %s:(%pa)\n",
				"catch VENCSYS address mapped!",
				"phys_addr", &phys_addr,
				"size", size, "test_pa", &test_pa);
		BUG_ON(1);
	}
}
EXPORT_SYMBOL(devapc_catch_illegal_range);

static struct mtk_devapc_dbg_status mt6885_devapc_dbg_stat = {
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
	"WRONG_SLAVE_TYPE",
};

static int mtk_vio_mask_sta_num[] = {
	VIO_MASK_STA_NUM_INFRA,
	VIO_MASK_STA_NUM_PERI,
	VIO_MASK_STA_NUM_PERI2,
};

static struct mtk_devapc_vio_info mt6885_devapc_vio_info = {
	.vio_mask_sta_num = mtk_vio_mask_sta_num,
	.sramrom_vio_idx = SRAMROM_VIO_INDEX,
	.mdp_vio_idx = MDP_VIO_INDEX,
	.disp2_vio_idx = DISP2_VIO_INDEX,
	.mmsys_vio_idx = MMSYS_VIO_INDEX,
	.sramrom_slv_type = SRAMROM_SLAVE_TYPE,
	.mm2nd_slv_type = MM2ND_SLAVE_TYPE,
};

static const struct mtk_infra_vio_dbg_desc mt6885_vio_dbgs = {
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

static const struct mtk_sramrom_sec_vio_desc mt6885_sramrom_sec_vios = {
	.vio_id_mask = SRAMROM_SEC_VIO_ID_MASK,
	.vio_id_shift = SRAMROM_SEC_VIO_ID_SHIFT,
	.vio_domain_mask = SRAMROM_SEC_VIO_DOMAIN_MASK,
	.vio_domain_shift = SRAMROM_SEC_VIO_DOMAIN_SHIFT,
	.vio_rw_mask = SRAMROM_SEC_VIO_RW_MASK,
	.vio_rw_shift = SRAMROM_SEC_VIO_RW_SHIFT,
};

static const uint32_t mt6885_devapc_pds[] = {
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

static struct mtk_devapc_soc mt6885_data = {
	.dbg_stat = &mt6885_devapc_dbg_stat,
	.slave_type_arr = slave_type_to_str,
	.slave_type_num = SLAVE_TYPE_NUM,
	.device_info[SLAVE_TYPE_INFRA] = mt6885_devices_infra,
	.device_info[SLAVE_TYPE_PERI] = mt6885_devices_peri,
	.device_info[SLAVE_TYPE_PERI2] = mt6885_devices_peri2,
	.ndevices = mtk6885_devices_num,
	.vio_info = &mt6885_devapc_vio_info,
	.vio_dbgs = &mt6885_vio_dbgs,
	.sramrom_sec_vios = &mt6885_sramrom_sec_vios,
	.devapc_pds = mt6885_devapc_pds,
	.subsys_get = &index_to_subsys,
	.master_get = &mt6885_bus_id_to_master,
	.mm2nd_vio_handler = &mm2nd_vio_handler,
	.shift_group_get = mt6885_shift_group_get,
};

static const struct of_device_id mt6885_devapc_dt_match[] = {
	{ .compatible = "mediatek,mt6885-devapc" },
	{},
};

static int mt6885_devapc_probe(struct platform_device *pdev)
{
	return mtk_devapc_probe(pdev, &mt6885_data);
}

static int mt6885_devapc_remove(struct platform_device *dev)
{
	return mtk_devapc_remove(dev);
}

static struct platform_driver mt6885_devapc_driver = {
	.probe = mt6885_devapc_probe,
	.remove = mt6885_devapc_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = mt6885_devapc_dt_match,
	},
};

module_platform_driver(mt6885_devapc_driver);

MODULE_DESCRIPTION("Mediatek MT6885 Device APC Driver");
MODULE_AUTHOR("Neal Liu <neal.liu@mediatek.com>");
MODULE_LICENSE("GPL");
