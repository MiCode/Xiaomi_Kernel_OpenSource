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
#include "devapc-mtk-multi-ao.h"

static struct mtk_device_info mt6873_devices_infra[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
};

static struct mtk_device_info mt6873_devices_peri[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
};

static struct mtk_device_info mt6873_devices_peri2[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
};

static struct mtk_device_info mt6873_devices_peri_par[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
};

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

static const char *infra_mi_trans(int bus_id)
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

static const char *peri_mi_trans(int bus_id)
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

static const char *mt6873_bus_id_to_master(int bus_id, uint32_t vio_addr)
{
	uint16_t h_2byte;
	uint8_t h_1byte;

	pr_debug(PFX "[DEVAPC] bus_id:0x%x, vio_addr:0x%x\n",
		bus_id, vio_addr);

	if (bus_id == 0x0 && vio_addr == 0x0)
		return NULL;

	/* bus only reference bit 0~29 */
	vio_addr = vio_addr & 0x3FFFFFFF;

	h_1byte = (vio_addr >> 24) & 0xFF;
	h_2byte = (vio_addr >> 16) & 0xFFFF;

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
	if (slave_type == SLAVE_TYPE_INFRA &&
			vio_index < VIO_SLAVE_NUM_INFRA) {

		/* check violation address */
		if (vio_addr >= MFG_PA_START && vio_addr <= MFG_PA_END)
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

		else if (vio_index >= CAM_SENINF_START &&
				vio_index <= CAM_SENINF_END)
			return "CAMSYS_SENINF";

		switch (vio_index) {
		case MFG_START ... MFG_END:
			return "MFGSYS";
		case MM_DISP_START ... MM_DISP_END:
			return "MMSYS_DISP";
		case IMG_START ... IMG_END:
			return "IMGSYS";
		case VDEC_START ... VDEC_END:
			return "VDECSYS";
		case VENC_START ... VENC_END:
			return "VENCSYS";
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
			return mt6873_devices_infra[vio_index].device;
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
			return mt6873_devices_peri[vio_index].device;
		}

	} else if (slave_type == SLAVE_TYPE_PERI2 &&
			vio_index < VIO_SLAVE_NUM_PERI2) {

		/* check violation address */
		if ((vio_addr >= GCE_PA_START && vio_addr <= GCE_PA_END) ||
				(vio_addr >= GCE_M2_PA_START &&
				 vio_addr <= GCE_M2_PA_END))
			return "GCE";

		return mt6873_devices_peri2[vio_index].device;
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
	.mmsys_vio_idx = MMSYS_VIO_INDEX,
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
