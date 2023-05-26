/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/platform_device.h>
#ifdef CONFIG_MTK_DRAMC
#include <mtk_dramc.h>
#endif
#ifdef CONFIG_MTK_EMI
#include <mt_emi_api.h>
#endif

#include <mt-plat/upmu_common.h>
#include "helio-dvfsrc-ip-v2.h"
#include <helio-dvfsrc-opp.h>
#include <helio-dvfsrc-mt6785.h>
#include <mt-plat/mtk_devinfo.h>
#include <linux/regulator/consumer.h>
#include "mmdvfs_pmqos.h"
#ifdef CONFIG_MTK_WATCHDOG
#include <ext_wd_drv.h>
#endif

#define DVFSRC_1600_FLOOR
#define AUTOK_ENABLE

static struct reg_config dvfsrc_init_configs[][128] = {
	{
		{ DVFSRC_TIMEOUT_NEXTREQ,  0x0000002B },
		{ DVFSRC_INT_EN,           0x00000002 },
		{ DVFSRC_QOS_EN,           0x0000407C },
		{ DVFSRC_VCORE_REQUEST4,   0x21110000 },
#ifdef DVFSRC_1600_FLOOR
		{ DVFSRC_DDR_REQUEST,	   0x00004322 },
#else
		{ DVFSRC_DDR_REQUEST,	   0x00004321 },
#endif
		{ DVFSRC_DDR_REQUEST3,     0x00000055 },
#ifdef DVFSRC_1600_FLOOR
		{ DVFSRC_DDR_REQUEST5,	   0x00322000 },
#else
		{ DVFSRC_DDR_REQUEST5,	   0x00321000 },
#endif
		{ DVFSRC_DDR_REQUEST7,     0x54000000 },
		{ DVFSRC_DDR_REQUEST8,     0x55000000 },
		{ DVFSRC_DDR_QOS0,         0x00000019 },
		{ DVFSRC_DDR_QOS1,         0x00000026 },
		{ DVFSRC_DDR_QOS2,         0x00000033 },
		{ DVFSRC_DDR_QOS3,         0x0000004C },
		{ DVFSRC_DDR_QOS4,         0x00000066 },
		{ DVFSRC_DDR_QOS5,         0x00000077 },
		{ DVFSRC_DDR_QOS6,         0x00000077 },
		{ DVFSRC_95MD_SCEN_BW0_T,  0x22222220 },
		{ DVFSRC_95MD_SCEN_BW1_T,  0x22222222 },
		{ DVFSRC_95MD_SCEN_BW2_T,  0x22222222 },
		{ DVFSRC_95MD_SCEN_BW3_T,  0x52222222 },
		{ DVFSRC_95MD_SCEN_BW4,    0x00000005 },
		{ DVFSRC_HRT_REQ_UNIT,      0x0000011E },
		{ DVSFRC_HRT_REQ_MD_URG,    0x0000D3D3 },
		{ DVFSRC_HRT_REQ_MD_BW_0,   0x00200802 },
		{ DVFSRC_HRT_REQ_MD_BW_1,   0x00200800 },
		{ DVFSRC_HRT_REQ_MD_BW_2,   0x00200002 },
		{ DVFSRC_HRT_REQ_MD_BW_3,   0x00200802 },
		{ DVFSRC_HRT_REQ_MD_BW_4,   0x00400802 },
		{ DVFSRC_HRT_REQ_MD_BW_5,   0x00601404 },
		{ DVFSRC_HRT_REQ_MD_BW_6,   0x00902008 },
		{ DVFSRC_HRT_REQ_MD_BW_7,   0x00E0380E },
		{ DVFSRC_HRT_REQ_MD_BW_8,   0x00000000 },
		{ DVFSRC_HRT_REQ_MD_BW_9,   0x00000000 },
		{ DVFSRC_HRT_REQ_MD_BW_10,  0x00034C00 },
		{ DVFSRC_HRT1_REQ_MD_BW_0,  0x0360D836 },
		{ DVFSRC_HRT1_REQ_MD_BW_1,  0x0360D800 },
		{ DVFSRC_HRT1_REQ_MD_BW_2,  0x03600036 },
		{ DVFSRC_HRT1_REQ_MD_BW_3,  0x0360D836 },
		{ DVFSRC_HRT1_REQ_MD_BW_4,  0x0360D836 },
		{ DVFSRC_HRT1_REQ_MD_BW_5,  0x0360D836 },
		{ DVFSRC_HRT1_REQ_MD_BW_6,  0x0360D836 },
		{ DVFSRC_HRT1_REQ_MD_BW_7,  0x0360D836 },
		{ DVFSRC_HRT1_REQ_MD_BW_8,  0x00000000 },
		{ DVFSRC_HRT1_REQ_MD_BW_9,  0x00000000 },
		{ DVFSRC_HRT1_REQ_MD_BW_10, 0x00034C00 },
		{ DVFSRC_HRT_HIGH,          0x070804B0 },
		{ DVFSRC_HRT_HIGH_1,        0x11830B80 },
		{ DVFSRC_HRT_HIGH_2,        0x18A618A6 },
		{ DVFSRC_HRT_HIGH_3,        0x000018A6 },
		{ DVFSRC_HRT_LOW,           0x070704AF },
		{ DVFSRC_HRT_LOW_1,         0x11820B7F },
		{ DVFSRC_HRT_LOW_2,         0x18A518A5 },
		{ DVFSRC_HRT_LOW_3,         0x000018A5 },
#ifdef DVFSRC_1600_FLOOR
		{ DVFSRC_HRT_REQUEST,	    0x05554322 },
#else
		{ DVFSRC_HRT_REQUEST,	    0x05554321 },
#endif
		{ DVFSRC_EMI_MON_DEBOUNCE_TIME,   0x4C4C0AB0 },
		{ DVFSRC_DDR_ADD_REQUEST,   0x05543210 },
		{ DVFSRC_EMI_ADD_REQUEST,   0x03333210 },
		{ DVFSRC_LEVEL_LABEL_0_1,   0x40225032 },
		{ DVFSRC_LEVEL_LABEL_2_3,   0x20223012 },
		{ DVFSRC_LEVEL_LABEL_4_5,   0x40211012 },
		{ DVFSRC_LEVEL_LABEL_6_7,   0x20213011 },
		{ DVFSRC_LEVEL_LABEL_8_9,   0x30101011 },
		{ DVFSRC_LEVEL_LABEL_10_11, 0x10102000 },
		{ DVFSRC_LEVEL_LABEL_12_13, 0x00000000 },
		{ DVFSRC_LEVEL_LABEL_14_15, 0x00000000 },
		{ DVFSRC_CURRENT_FORCE,     0x00000001 },
		{ DVFSRC_BASIC_CONTROL,     0x7599404B },
		{ DVFSRC_BASIC_CONTROL,     0x7599014B },
		{ DVFSRC_CURRENT_FORCE,     0x00000000 },
		{ -1, 0 },
	},
	/* NULL */
	{
		{ -1, 0 },
	},
};

#define dvfsrc_rmw(offset, val, mask, shift) \
	dvfsrc_write(offset, (dvfsrc_read(offset) & ~(mask << shift)) \
			| (val << shift))

u32 dvfsrc_get_ddr_qos(void)
{
	unsigned int qos_total_bw = dvfsrc_read(DVFSRC_SW_BW_0) +
			   dvfsrc_read(DVFSRC_SW_BW_1) +
			   dvfsrc_read(DVFSRC_SW_BW_2) +
			   dvfsrc_read(DVFSRC_SW_BW_3) +
			   dvfsrc_read(DVFSRC_SW_BW_4);

	if (qos_total_bw < 0x19)
		return 0;
#ifdef DVFSRC_1600_FLOOR
	else if (qos_total_bw < 0x26)
		return 2;
#else
	else if (qos_total_bw < 0x26)
		return 1;
#endif
	else if (qos_total_bw < 0x33)
		return 2;
	else if (qos_total_bw < 0x4c)
		return 3;
	else if (qos_total_bw < 0x66)
		return 4;
	else
		return 5;
}


static int dvfsrc_get_emi_mon_gear(void)
{
	unsigned int total_bw_status;
	int i;

	total_bw_status = vcorefs_get_total_emi_status() & 0x1F;
	for (i = 4; i >= 0 ; i--) {
		if ((total_bw_status >> i) > 0) {
#ifdef DVFSRC_1600_FLOOR
			if (i == 0)
				return i + 2;
			else
				return i + 1;
#else
			return i + 1;
#endif
		}
	}

	return 0;
}

static u32 dvfsrc_calc_hrt_opp(int data)
{
	if (data < 0x04B0)
		return DDR_OPP_5;
#ifdef DVFSRC_1600_FLOOR
	else if (data < 0x0708)
		return DDR_OPP_3;
#else
	else if (data < 0x0708)
		return DDR_OPP_4;
#endif
	else if (data < 0x0B80)
		return DDR_OPP_3;
	else if (data < 0x1183)
		return DDR_OPP_2;
	else if (data < 0x18A6)
		return DDR_OPP_1;
	else
		return DDR_OPP_0;
}

int dvfsrc_latch_register(int enable)
{
#ifdef CONFIG_MTK_WATCHDOG
	return mtk_rgu_cfg_dvfsrc(enable);
#else
	return 0;
#endif
}

void dvfsrc_set_isp_hrt_bw(int data)
{
	data = (data + 29) / 30;

	if (data > 0x3FF)
		data = 0x3FF;

	dvfsrc_write(DVFSRC_ISP_HRT, data);
}

u32 dvfsrc_calc_isp_hrt_opp(int data)
{
	return dvfsrc_calc_hrt_opp(((data + 29) /  30) * 30);
}

struct regulator *dvfsrc_vcore_requlator(struct device *dev)
{
	return regulator_get(dev, "vcore");
}


#ifdef AUTOK_ENABLE
__weak int emmc_autok(void)
{
	pr_info("NOT SUPPORT EMMC AUTOK\n");
	return 0;
}

__weak int sd_autok(void)
{
	pr_info("NOT SUPPORT SD AUTOK\n");
	return 0;
}

__weak int sdio_autok(void)
{
	pr_info("NOT SUPPORT SDIO AUTOK\n");
	return 0;
}


void begin_autok_task(void)
{
	/* notify MM DVFS for msdc autok start */
	mmdvfs_prepare_action(MMDVFS_PREPARE_CALIBRATION_START);
}


void finish_autok_task(void)
{
	/* check if dvfs force is released */
	int force = mtk_pm_qos_request(MTK_PM_QOS_VCORE_DVFS_FORCE_OPP);

	/* notify MM DVFS for msdc autok finish */
	mmdvfs_prepare_action(MMDVFS_PREPARE_CALIBRATION_END);

	if (force >= 0 && force < 13)
		pr_info("autok task not release force opp: %d\n", force);
}

static void dvfsrc_autok_manager(void)
{
	int r = 0;

	begin_autok_task();

	r = emmc_autok();
	pr_info("EMMC autok done: %s\n", (r == 0) ? "Yes" : "No");

	r = sd_autok();
	pr_info("SD autok done: %s\n", (r == 0) ? "Yes" : "No");

	r = sdio_autok();
	pr_info("SDIO autok done: %s\n", (r == 0) ? "Yes" : "No");

	finish_autok_task();
}
#endif

void helio_dvfsrc_platform_pre_init(struct helio_dvfsrc *dvfsrc)
{

}


void helio_dvfsrc_platform_init(struct helio_dvfsrc *dvfsrc)
{
	int spmfw_idx = 0;
	struct reg_config *config;
	int idx = 0;

	config = dvfsrc_init_configs[spmfw_idx];

	while (config[idx].offset != -1) {
		dvfsrc_write(config[idx].offset, config[idx].val);
		idx++;
	}
#ifdef AUTOK_ENABLE
	dvfsrc_autok_manager();
#endif
}

int vcore_pmic_to_uv(int pmic_val)
{
	return __vcore_pmic_to_uv(pmic_val);
}
int vcore_uv_to_pmic(int vcore_uv)
{
	return __vcore_uv_to_pmic(vcore_uv);
}

void get_opp_info(char *p)
{
#if defined(CONFIG_FPGA_EARLY_PORTING) || !defined(CONFIG_MTK_PMIC_COMMON)
	int pmic_val = 0;
	int vsram_val = 0;
#else
	int pmic_val = pmic_get_register_value(PMIC_VCORE_ADDR);
	int vsram_val = pmic_get_register_value(PMIC_VSRAM_OTHERS_ADDR);
#endif
#ifdef CONFIG_MTK_DRAMC
	int ddr_khz = get_dram_data_rate() * 1000;
#else
	int ddr_khz = 0;
#endif
	int vcore_uv = vcore_pmic_to_uv(pmic_val);
	int vsram_uv = vsram_pmic_to_uv(vsram_val);

	p += sprintf(p, "%-10s: %-8u uv  (PMIC: 0x%x)\n",
			"Vcore", vcore_uv, vcore_uv_to_pmic(vcore_uv));
	p += sprintf(p, "%-10s: %-8u uv  (PMIC: 0x%x)\n",
			"Vsram", vsram_uv, vsram_uv_to_pmic(vsram_uv));
	p += sprintf(p, "%-10s: %-8u khz\n", "DDR", ddr_khz);
}


/* met profile table */
static unsigned int met_vcorefs_src[SRC_MAX];

static char *met_src_name[SRC_MAX] = {
	"MD2SPM",
	"SRC_DDR_OPP",
	"DDR__SW_REQ1_SPM",
	"DDR__SW_REQ2_CM",
	"DDR__SW_REQ3_PMQOS",
	"DDR__SW_REQ4_MD",
	"DDR__SW_REQ8_MCUSYS",
	"DDR__QOS_BW",
	"DDR__EMI_TOTAL",
	"DDR__HRT_BW",
	"DDR__HIFI",
	"DDR__HIFI_LATENCY",
	"DDR__MD_LATENCY",
	"DDR__MD_DDR",
	"SRC_VCORE_OPP",
	"VCORE__SW_REQ3_PMQOS",
	"VCORE__SCP",
	"VCORE__HIFI",
	"SCP_REQ",
	"PMQOS_TATOL",
	"PMQOS_BW0",
	"PMQOS_BW1",
	"PMQOS_BW2",
	"PMQOS_BW3",
	"PMQOS_BW4",
	"TOTAL_EMI_BW",
	"HRT_MD_BW",
	"HRT_DISP_BW",
	"HRT_ISP_BW",
	"MD_SCENARIO",
	"HIFI_SCENARIO_IDX",
	"MD_EMI_LATENCY",
};

/* met profile function */
int vcorefs_get_src_req_num(void)
{
	return SRC_MAX;
}
EXPORT_SYMBOL(vcorefs_get_src_req_num);

char **vcorefs_get_src_req_name(void)
{
	return met_src_name;
}
EXPORT_SYMBOL(vcorefs_get_src_req_name);

static void vcorefs_get_src_ddr_req(void)
{
	unsigned int sw_req;

	met_vcorefs_src[DDR_OPP_IDX] =
		get_cur_ddr_opp();

	sw_req = dvfsrc_read(DVFSRC_SW_REQ1);
	met_vcorefs_src[DDR_SW_REQ1_SPM_IDX] =
		(sw_req >> DDR_SW_AP_SHIFT) & DDR_SW_AP_MASK;

	sw_req = dvfsrc_read(DVFSRC_SW_REQ2);
	met_vcorefs_src[DDR_SW_REQ2_CM_IDX] =
		(sw_req >> DDR_SW_AP_SHIFT) & DDR_SW_AP_MASK;

	sw_req = dvfsrc_read(DVFSRC_SW_REQ3);
	met_vcorefs_src[DDR_SW_REQ3_PMQOS_IDX] =
		(sw_req >> DDR_SW_AP_SHIFT) & DDR_SW_AP_MASK;

	sw_req = dvfsrc_read(DVFSRC_SW_REQ4);
	met_vcorefs_src[DDR_SW_REQ4_MD_IDX] =
		(sw_req >> DDR_SW_AP_SHIFT) & DDR_SW_AP_MASK;

	sw_req = dvfsrc_read(DVFSRC_SW_REQ8);
	met_vcorefs_src[DDR_SW_REQ8_MCUSYS_IDX] =
		(sw_req >> DDR_SW_AP_SHIFT) & DDR_SW_AP_MASK;

	met_vcorefs_src[DDR_QOS_BW_IDX] =
		dvfsrc_get_ddr_qos();

	met_vcorefs_src[DDR_EMI_TOTAL_IDX] =
		dvfsrc_get_emi_mon_gear();

	met_vcorefs_src[DDR_HRT_BW_IDX] =
		vcorefs_get_hrt_bw_ddr();

	met_vcorefs_src[DDR_HIFI_IDX] =
		vcorefs_get_hifi_ddr_status();

	met_vcorefs_src[DDR_HIFI_LATENCY_IDX] =
		vcorefs_get_hifi_rising_ddr();

	met_vcorefs_src[DDR_MD_LATENCY_IDX] =
		vcorefs_get_md_rising_ddr();

	met_vcorefs_src[DDR_MD_DDR_IDX] =
		vcorefs_get_md_scenario_ddr();
}

static void vcorefs_get_src_vcore_req(void)
{
	u32 sw_req;
	u32 scp_en;

	scp_en = vcorefs_get_scp_req_status();

	met_vcorefs_src[VCORE_OPP_IDX] =
		get_cur_vcore_opp();

	sw_req = dvfsrc_read(DVFSRC_SW_REQ3);
	met_vcorefs_src[VCORE_SW_REQ3_PMQOS_IDX] =
		(sw_req >> VCORE_SW_AP_SHIFT) & VCORE_SW_AP_MASK;

	if (scp_en) {
		sw_req = dvfsrc_read(DVFSRC_VCORE_REQUEST);
		met_vcorefs_src[VCORE_SCP_IDX] =
			(sw_req >> VCORE_SCP_GEAR_SHIFT) & VCORE_SCP_GEAR_MASK;
	} else
		met_vcorefs_src[VCORE_SCP_IDX] = 0;

	met_vcorefs_src[VCORE_HIFI_IDX] =
		vcorefs_get_hifi_vcore_status();
}

static void vcorefs_get_src_misc_info(void)
{
#ifdef CONFIG_MTK_EMI
	unsigned int total_bw_last = (get_emi_bwvl(0) & 0x7F) * 813;
#endif
	u32 qos_bw0, qos_bw1, qos_bw2, qos_bw3, qos_bw4;

	qos_bw0 = dvfsrc_read(DVFSRC_SW_BW_0);
	qos_bw1 = dvfsrc_read(DVFSRC_SW_BW_1);
	qos_bw2 = dvfsrc_read(DVFSRC_SW_BW_2);
	qos_bw3 = dvfsrc_read(DVFSRC_SW_BW_3);
	qos_bw4 = dvfsrc_read(DVFSRC_SW_BW_4);

	met_vcorefs_src[SRC_MD2SPM_IDX] =
		vcorefs_get_md_scenario();

	met_vcorefs_src[SRC_SCP_REQ_IDX] =
		vcorefs_get_scp_req_status();

	met_vcorefs_src[SRC_PMQOS_TATOL_IDX] =
		qos_bw0 + qos_bw1 + qos_bw2 + qos_bw3 + qos_bw4;

	met_vcorefs_src[SRC_PMQOS_BW0_IDX] =
		qos_bw0;

	met_vcorefs_src[SRC_PMQOS_BW1_IDX] =
		qos_bw1;

	met_vcorefs_src[SRC_PMQOS_BW2_IDX] =
		qos_bw2;

	met_vcorefs_src[SRC_PMQOS_BW3_IDX] =
		qos_bw3;

	met_vcorefs_src[SRC_PMQOS_BW4_IDX] =
		qos_bw4;

#ifdef CONFIG_MTK_EMI
	met_vcorefs_src[SRC_TOTAL_EMI_BW_IDX] =
		total_bw_last;
#endif

	met_vcorefs_src[SRC_HRT_MD_BW_IDX] =
		dvfsrc_get_md_bw();

	met_vcorefs_src[SRC_HRT_ISP_BW_IDX] =
		dvfsrc_read(DVFSRC_ISP_HRT);

	met_vcorefs_src[SRC_MD_SCENARIO_IDX] =
		vcorefs_get_md_scenario();

	met_vcorefs_src[SRC_HIFI_SCENARIO_IDX] =
		vcorefs_get_hifi_scenario();

	met_vcorefs_src[SRC_MD_EMI_LATENCY_IDX] =
		vcorefs_get_md_emi_latency_status();

}


unsigned int *vcorefs_get_src_req(void)
{
	vcorefs_get_src_ddr_req();
	vcorefs_get_src_vcore_req();
	vcorefs_get_src_misc_info();

	vcorefs_trace_qos();

	return met_vcorefs_src;
}
EXPORT_SYMBOL(vcorefs_get_src_req);


int get_cur_ddr_ratio(void)
{
	int idx;

	if (!is_dvfsrc_enabled())
		return 0;

	idx = get_cur_vcore_dvfs_opp();

	if (idx >= VCORE_DVFS_OPP_NUM)
		return 0;

	if ((get_ddr_opp(idx) < DDR_OPP_3) || (idx == 10))
		return 8;
	else
		return 4;
}
EXPORT_SYMBOL(get_cur_ddr_ratio);

