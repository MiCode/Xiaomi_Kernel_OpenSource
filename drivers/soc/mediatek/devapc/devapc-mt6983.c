// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/bug.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include "devapc-mt6983.h"

static const struct mtk_device_num mtk6983_devices_num[] = {
	{SLAVE_TYPE_INFRA, VIO_SLAVE_NUM_INFRA, IRQ_TYPE_INFRA},
	{SLAVE_TYPE_INFRA1, VIO_SLAVE_NUM_INFRA1, IRQ_TYPE_INFRA},
	{SLAVE_TYPE_PERI_PAR, VIO_SLAVE_NUM_PERI_PAR, IRQ_TYPE_INFRA},
	{SLAVE_TYPE_VLP, VIO_SLAVE_NUM_VLP, IRQ_TYPE_VLP},
#if ENABLE_DEVAPC_ADSP
	{SLAVE_TYPE_ADSP, VIO_SLAVE_NUM_ADSP, IRQ_TYPE_ADSP},
#endif
#if ENABLE_DEVAPC_MMINFRA
	{SLAVE_TYPE_MMINFRA, VIO_SLAVE_NUM_MMINFRA, IRQ_TYPE_MMINFRA},
#endif
#if ENABLE_DEVAPC_MMUP
	{SLAVE_TYPE_MMUP, VIO_SLAVE_NUM_MMUP, IRQ_TYPE_MMUP},
#endif
};

static const struct INFRAAXI_ID_INFO infra_mi_id_to_master[] = {
	{"ADSPSYS_M1",             { 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0 } },
	{"DSP_M1",                 { 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"DMA_M1",                 { 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"DSP_M2",                 { 0, 0, 0, 0, 0, 1, 0, 0, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"DMA_M2",                 { 0, 0, 0, 0, 0, 1, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"AFE_M",                  { 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"CONN_M",                 { 0, 0, 1, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0 } },
	{"CQ_DMA",                 { 0, 0, 0, 1, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0 } },
	{"DebugTop",               { 0, 0, 0, 1, 0, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"GPU_EB",                 { 0, 0, 0, 1, 0, 0, 1, 0, 2, 2, 2, 2, 2, 2, 2, 0 } },
	{"CPUM_M",                 { 0, 0, 0, 1, 0, 1, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"DXCC_M",                 { 0, 0, 0, 1, 0, 0, 0, 1, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"VLPSYS_M",               { 0, 0, 1, 1, 0, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0 } },
	{"SCP_M",                  { 0, 0, 1, 1, 0, 0, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0 } },
	{"SSPM_M",                 { 0, 0, 1, 1, 0, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"SPM_M",                  { 0, 0, 1, 1, 0, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"DPMAIF_M",               { 0, 0, 0, 0, 1, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"HWCCF_M",                { 0, 0, 1, 0, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"THERM_M",                { 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"THERM2_M",               { 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"CCU_M",                  { 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"PERI2INFRA1_M",          { 0, 0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0 } },
	{"UFS_M",                  { 0, 0, 0, 1, 1, 0, 2, 2, 0, 0, 0, 2, 2, 0, 0, 0 } },
	{"APDMA_INT_M",            { 0, 0, 0, 1, 1, 1, 0, 0, 2, 2, 2, 2, 2, 0, 0, 0 } },
	{"NOR_M",                  { 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 2, 2, 0, 0, 0 } },
	{"SSUSB_DUAL_M",           { 0, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 2, 2, 0, 0, 0 } },
	{"MSDC1_M/SPI7_M/MSDC2_M", { 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 2, 2, 0, 0, 0 } },
	{"IPU_M",                  { 0, 0, 1, 1, 1, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"INFRA_BUS_HRE_M",        { 0, 0, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"MCU_AP_M",               { 1, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0 } },
	{"MM2SLB1_M",              { 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 } },
	{"GCE_D_M",                { 0, 1, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } },
	{"GCE_M_M",                { 0, 1, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } },
	{"MMUP2INFRA_M",           { 0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 1 } },
	{"MD_AP_M",                { 1, 1, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
};

static const struct ADSPAXI_ID_INFO ADSP_mi13_id_to_master[] = {
	{"DSP_M1",            { 0, 0, 0, 2, 2, 2, 2, 0 } },
	{"DMA_M1",            { 0, 0, 1, 2, 0, 0, 0, 0 } },
	{"DSP_M2",            { 1, 0, 0, 2, 2, 2, 2, 0 } },
	{"DMA_M2",            { 1, 0, 1, 2, 0, 0, 0, 0 } },
	{"AFE_M",             { 0, 1, 2, 0, 0, 0, 0, 0 } },
};

static const struct ADSPAXI_ID_INFO ADSP_mi15_id_to_master[] = {
	{"DSP_M1",            { 1, 0, 0, 2, 2, 2, 2, 0 } },
	{"DMA_M1",            { 1, 0, 1, 2, 0, 0, 0, 0 } },
	{"DSP_M2",            { 1, 1, 0, 2, 2, 2, 2, 0 } },
	{"DMA_M2",            { 1, 1, 1, 2, 0, 0, 0, 0 } },
	{"AFE_M",             { 0, 0, 2, 0, 0, 0, 0, 0 } },
};

static const struct MMINFRAAXI_ID_INFO mminfra_mi_id_to_master[] = {
	{"INFRA2MM",    { 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 } },
	{"MMINFRA_HRE", { 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"GCED",        { 0, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"GCEM",        { 1, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"MMUP",        { 0, 0, 1, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
};

static const char * const adsp_domain[] = {
	"AP",
	"SSPM",
	"CONN",
	"SCP",
	"MCUPM",
	"CCU",
	"others",
	"others",
};

static const char * const mminfra_domain[] = {
	"AP",
	"SSPM",
	"CCU",
	"SCP",
	"GCE",
	"GZ",
	"MMuP",
	"others",
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
			pr_info(PFX "%s %s %s\n",
				"catch it from INFRAAXI_MI",
				"Master is:",
				infra_mi_id_to_master[i].master);
			master = infra_mi_id_to_master[i].master;
		}
	}

	return master;
}


static const char *adsp_mi_trans(uint32_t bus_id, int mi)
{
	int master_count = ARRAY_SIZE(infra_mi_id_to_master);
	const char *master = "UNKNOWN_MASTER_FROM_ADSP";
	int i, j;

	const struct ADSPAXI_ID_INFO *ADSP_mi_id_to_master;

	if (mi == ADSP_MI13)
		ADSP_mi_id_to_master = ADSP_mi13_id_to_master;
	else
		ADSP_mi_id_to_master = ADSP_mi15_id_to_master;

	for (i = 0; i < master_count; i++) {
		for (j = 0; j < ADSPAXI_MI_BIT_LENGTH; j++) {
			if (ADSP_mi_id_to_master[i].bit[j] == 2)
				continue;
			if (((bus_id >> j) & 0x1) ==
				ADSP_mi_id_to_master[i].bit[j])
				continue;
			break;
		}
		if (j == ADSPAXI_MI_BIT_LENGTH) {
			pr_debug(PFX "%s %s %s\n",
				"catch it from ADSPAXI_MI",
				"Master is:",
				infra_mi_id_to_master[i].master);
			master = infra_mi_id_to_master[i].master;
		}
	}
	return master;
}

static const char *mminfra_mi_trans(uint32_t bus_id)
{
	int master_count = ARRAY_SIZE(mminfra_mi_id_to_master);
	const char *master = "UNKNOWN_MASTER_FROM_MMINFRA";
	int i, j;

	for (i = 0; i < master_count; i++) {
		for (j = 0; j < MMINFRAAXI_MI_BIT_LENGTH; j++) {
			if (mminfra_mi_id_to_master[i].bit[j] == 2)
				continue;
			if (((bus_id >> j) & 0x1) ==
					mminfra_mi_id_to_master[i].bit[j])
				continue;
			break;
		}
		if (j == MMINFRAAXI_MI_BIT_LENGTH) {
			pr_debug(PFX "%s %s %s\n",
				"catch it from MMINFRAAXI_MI",
				"Master is:",
				mminfra_mi_id_to_master[i].master);
			master = mminfra_mi_id_to_master[i].master;
		}
	}

	return master;
}

static const char *mt6983_bus_id_to_master(uint32_t bus_id, uint32_t vio_addr,
		int slave_type, int shift_sta_bit, int domain)
{
	pr_debug(PFX "%s:0x%x, %s:0x%x, %s:0x%x, %s:%d\n",
		"bus_id", bus_id, "vio_addr", vio_addr,
		"slave_type", slave_type,
		"shift_sta_bit", shift_sta_bit);

	if (vio_addr <= SRAM_END_ADDR) {
		pr_info(PFX "vio_addr is from on-chip SRAMROM\n");
		if ((bus_id & 0x3) == 0x0)
			return "NTH_EMI_GMC_M";
		else if ((bus_id & 0x3) == 0x2)
			return "STH_EMI_GMC_M";
		else
			return infra_mi_trans(bus_id >> 1);
	} else if ((vio_addr >= L3CACHE_0_START && vio_addr <= L3CACHE_0_END) ||
		(vio_addr >= L3CACHE_1_START && vio_addr <= L3CACHE_1_END) ||
		(vio_addr >= L3CACHE_2_START && vio_addr <= L3CACHE_2_END) ||
		(vio_addr >= L3CACHE_3_START && vio_addr <= L3CACHE_3_END)) {
		pr_info(PFX "vio_addr is from L3Cache share SRAM\n");
		if ((bus_id & 0x3) == 0x0)
			return "NTH_EMI_GMC_M";
		else if ((bus_id & 0x3) == 0x2)
			return "STH_EMI_GMC_M";
		else
			return infra_mi_trans(bus_id >> 1);
	} else if (slave_type == SLAVE_TYPE_VLP) {
		/* mi3 */
		if ((vio_addr >= VLP_SCP_START_ADDR) && (vio_addr <= VLP_SCP_END_ADDR)) {
			if ((bus_id & 0x3) == 0x0)
				return "SSPM_M";
			else if ((bus_id & 0x3) == 0x1)
				return "SPM_M";
			else if ((bus_id & 0x3) == 0x2)
				return infra_mi_trans(bus_id >> 2);
			else
				return "UNKNOWN_MASTER_TO_SCP";
		/* mi1 */
		} else if ((vio_addr >= VLP_INFRA_START && vio_addr <= VLP_INFRA_END) ||
			(vio_addr >= VLP_INFRA_1_START && vio_addr <= VLP_INFRA_1_END)) {
			if ((bus_id & 0x3) == 0x0)
				return "SCP_M";
			else if ((bus_id & 0x3) == 0x1)
				return "SSPM_M";
			else if ((bus_id & 0x3) == 0x2)
				return "SPM_M";
			else
				return "UNKNOWN_MASTER_TO_INFRA";
		/* mi2 */
		} else {
			if ((bus_id & 0x3) == 0x0)
				return "SCP_M";
			else if ((bus_id & 0x3) == 0x1)
				return "SSPM_M";
			else if ((bus_id & 0x3) == 0x2)
				return "SPM_M";
			else
				return infra_mi_trans(bus_id >> 2);
		}
#if ENABLE_DEVAPC_ADSP
	} else if (slave_type == SLAVE_TYPE_ADSP) {
		/* infra slave */
		if ((vio_addr >= ADSP_INFRA_START && vio_addr <= ADSP_INFRA_END) ||
			(vio_addr >= ADSP_INFRA_1_START && vio_addr <= ADSP_INFRA_1_END)) {
			return adsp_mi_trans(bus_id, ADSP_MI13);
		/* adsp misc slave */
		} else if (vio_addr >= ADSP_OTHER_START && vio_addr <= ADSP_OTHER_END) {
			if ((bus_id & 0x1) == 0x1)
				return "HRE";
			else if ((bus_id & 0x7) == 0x4)
				return adsp_domain[domain];
			else
				return adsp_mi_trans(bus_id >> 1, ADSP_MI15);
		/* adsp audio_pwr, dsp_pwr slave */
		} else {
			if ((bus_id & 0x3) == 0x2)
				return adsp_domain[domain];
			else
				return adsp_mi_trans(bus_id, ADSP_MI15);
		}
#endif
	} else if (slave_type == SLAVE_TYPE_MMINFRA) {
		/* MMUP slave */
		if ((vio_addr >= MMUP_START_ADDR) && (vio_addr <= MMUP_END_ADDR)) {
			if (domain < ARRAY_SIZE(mminfra_domain))
				return mminfra_domain[domain];
			return NULL;

		/* CODEC slave*/
		} else if ((vio_addr >= CODEC_START_ADDR) && (vio_addr <= CODEC_END_ADDR)) {
			if ((bus_id & 0x1) == 0x0)
				return "MMUP";
			else
				return infra_mi_trans(bus_id >> 4);

		/* DISP / MDP / DMDP slave*/
		} else if (((vio_addr >= DISP_START_ADDR) && (vio_addr <= DISP_END_ADDR)) ||
			((vio_addr >= MDP_START_ADDR) && (vio_addr <= MDP_END_ADDR))) {
			if ((bus_id & 0x1) == 0x0)
				return "GCED";
			else
				return infra_mi_trans(bus_id >> 4);

		/* DISP2 / DPTX slave*/
		} else if (((vio_addr >= DISP2_START_ADDR) && (vio_addr <= DISP2_END_ADDR)) ||
			((vio_addr >= DPTX_START_ADDR) && (vio_addr <= DPTX_END_ADDR))) {
			if ((bus_id & 0x3) == 0x0)
				return "GCED";
			else if ((bus_id & 0x3) == 0x1)
				return "GCEM";
			else
				return infra_mi_trans(bus_id >> 5);

		/* other mminfra slave*/
		} else {
			if ((bus_id & 0x7) == 0x0)
				return infra_mi_trans(bus_id >> 3);
			else
				return mminfra_mi_trans(bus_id);
		}
#if ENABLE_DEVAPC_MMUP
	} else if (slave_type == SLAVE_TYPE_MMUP) {
		return mminfra_domain[domain];
#endif
	} else {
		return infra_mi_trans(bus_id);
	}
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
			if (vio_index == mt6983_devices_infra[i].vio_index)
				return mt6983_devices_infra[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_INFRA1) {
		for (i = 0; i < VIO_SLAVE_NUM_INFRA1; i++) {
			if (vio_index == mt6983_devices_infra1[i].vio_index)
				return mt6983_devices_infra1[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_PERI_PAR) {
		for (i = 0; i < VIO_SLAVE_NUM_PERI_PAR; i++) {
			if (vio_index == mt6983_devices_peri_par[i].vio_index)
				return mt6983_devices_peri_par[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_VLP) {
		for (i = 0; i < VIO_SLAVE_NUM_VLP; i++) {
			if (vio_index == mt6983_devices_vlp[i].vio_index)
				return mt6983_devices_vlp[i].device;
		}
#if ENABLE_DEVAPC_ADSP
	} else if (slave_type == SLAVE_TYPE_ADSP) {
		for (i = 0; i < VIO_SLAVE_NUM_ADSP; i++) {
			if (vio_index == mt6983_devices_adsp[i].vio_index)
				return mt6983_devices_adsp[i].device;
		}
#endif
#if ENABLE_DEVAPC_MMINFRA
	} else if (slave_type == SLAVE_TYPE_MMINFRA) {
		for (i = 0; i < VIO_SLAVE_NUM_MMINFRA; i++) {
			if (vio_index == mt6983_devices_mminfra[i].vio_index)
				return mt6983_devices_mminfra[i].device;
		}
#endif
#if ENABLE_DEVAPC_MMUP
	} else if (slave_type == SLAVE_TYPE_MMUP) {
		for (i = 0; i < VIO_SLAVE_NUM_MMUP; i++) {
			if (vio_index == mt6983_devices_mmup[i].vio_index)
				return mt6983_devices_mmup[i].device;
		}
#endif
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

static uint32_t mt6983_shift_group_get(int slave_type, uint32_t vio_idx)
{
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

static struct mtk_devapc_dbg_status mt6983_devapc_dbg_stat = {
	.enable_ut = PLAT_DBG_UT_DEFAULT,
	.enable_KE = PLAT_DBG_KE_DEFAULT,
	.enable_AEE = PLAT_DBG_AEE_DEFAULT,
	.enable_WARN = PLAT_DBG_WARN_DEFAULT,
	.enable_dapc = PLAT_DBG_DAPC_DEFAULT,
};

static const char * const slave_type_to_str[] = {
	"SLAVE_TYPE_INFRA",
	"SLAVE_TYPE_INFRA1",
	"SLAVE_TYPE_PERI_PAR",
	"SLAVE_TYPE_VLP",
#if ENABLE_DEVAPC_ADSP
	"SLAVE_TYPE_ADSP",
#endif
#if ENABLE_DEVAPC_MMINFRA
	"SLAVE_TYPE_MMINFRA",
#endif
#if ENABLE_DEVAPC_MMUP
	"SLAVE_TYPE_MMUP",
#endif
	"WRONG_SLAVE_TYPE",
};

static int mtk_vio_mask_sta_num[] = {
	VIO_MASK_STA_NUM_INFRA,
	VIO_MASK_STA_NUM_INFRA1,
	VIO_MASK_STA_NUM_PERI_PAR,
	VIO_MASK_STA_NUM_VLP,
#if ENABLE_DEVAPC_ADSP
	VIO_MASK_STA_NUM_ADSP,
#endif
#if ENABLE_DEVAPC_MMINFRA
	VIO_MASK_STA_NUM_MMINFRA,
#endif
#if ENABLE_DEVAPC_MMUP
	VIO_MASK_STA_NUM_MMUP,
#endif
};

static struct mtk_devapc_vio_info mt6983_devapc_vio_info = {
	.vio_mask_sta_num = mtk_vio_mask_sta_num,
	.sramrom_vio_idx = SRAMROM_VIO_INDEX,
	.mdp_vio_idx = MDP_VIO_INDEX,
	.disp2_vio_idx = DISP2_VIO_INDEX,
	.mmsys_vio_idx = MMSYS_VIO_INDEX,
	.sramrom_slv_type = SRAMROM_SLAVE_TYPE,
	.mm2nd_slv_type = MM2ND_SLAVE_TYPE,
};

static const struct mtk_infra_vio_dbg_desc mt6983_vio_dbgs = {
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

static const struct mtk_sramrom_sec_vio_desc mt6983_sramrom_sec_vios = {
	.vio_id_mask = SRAMROM_SEC_VIO_ID_MASK,
	.vio_id_shift = SRAMROM_SEC_VIO_ID_SHIFT,
	.vio_domain_mask = SRAMROM_SEC_VIO_DOMAIN_MASK,
	.vio_domain_shift = SRAMROM_SEC_VIO_DOMAIN_SHIFT,
	.vio_rw_mask = SRAMROM_SEC_VIO_RW_MASK,
	.vio_rw_shift = SRAMROM_SEC_VIO_RW_SHIFT,
};

static const uint32_t mt6983_devapc_pds[] = {
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

static struct mtk_devapc_soc mt6983_data = {
	.dbg_stat = &mt6983_devapc_dbg_stat,
	.slave_type_arr = slave_type_to_str,
	.slave_type_num = SLAVE_TYPE_NUM,
	.device_info[SLAVE_TYPE_INFRA] = mt6983_devices_infra,
	.device_info[SLAVE_TYPE_INFRA1] = mt6983_devices_infra1,
	.device_info[SLAVE_TYPE_PERI_PAR] = mt6983_devices_peri_par,
	.device_info[SLAVE_TYPE_VLP] = mt6983_devices_vlp,
#if ENABLE_DEVAPC_ADSP
	.device_info[SLAVE_TYPE_ADSP] = mt6983_devices_adsp,
#endif
#if ENABLE_DEVAPC_MMINFRA
	.device_info[SLAVE_TYPE_MMINFRA] = mt6983_devices_mminfra,
#endif
#if ENABLE_DEVAPC_MMUP
	.device_info[SLAVE_TYPE_MMUP] = mt6983_devices_mmup,
#endif
	.ndevices = mtk6983_devices_num,
	.vio_info = &mt6983_devapc_vio_info,
	.vio_dbgs = &mt6983_vio_dbgs,
	.sramrom_sec_vios = &mt6983_sramrom_sec_vios,
	.devapc_pds = mt6983_devapc_pds,
	.irq_type_num = IRQ_TYPE_NUM,
	.subsys_get = &index_to_subsys,
	.master_get = &mt6983_bus_id_to_master,
	.mm2nd_vio_handler = &mm2nd_vio_handler,
	.shift_group_get = mt6983_shift_group_get,
};

static const struct of_device_id mt6983_devapc_dt_match[] = {
	{ .compatible = "mediatek,mt6983-devapc" },
	{},
};

static const struct dev_pm_ops devapc_dev_pm_ops = {
	.suspend_noirq	= devapc_suspend_noirq,
	.resume_noirq = devapc_resume_noirq,
};

static int mt6983_devapc_probe(struct platform_device *pdev)
{
	return mtk_devapc_probe(pdev, &mt6983_data);
}

static int mt6983_devapc_remove(struct platform_device *dev)
{
	return mtk_devapc_remove(dev);
}

static struct platform_driver mt6983_devapc_driver = {
	.probe = mt6983_devapc_probe,
	.remove = mt6983_devapc_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = mt6983_devapc_dt_match,
		.pm = &devapc_dev_pm_ops,
	},
};

module_platform_driver(mt6983_devapc_driver);

MODULE_DESCRIPTION("Mediatek MT6983 Device APC Driver");
MODULE_AUTHOR("Jackson Chang <jackson-kt.chang@mediatek.com>");
MODULE_LICENSE("GPL");
