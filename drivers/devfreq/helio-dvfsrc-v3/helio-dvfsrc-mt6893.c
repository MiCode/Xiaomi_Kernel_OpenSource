/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/platform_device.h>
#ifdef CONFIG_MEDIATEK_DRAMC
#include <dramc.h>
#endif
/*
#ifdef CONFIG_MTK_EMI
#include <mt_emi_api.h>
#endif
*/
#include <mt-plat/upmu_common.h>
#include "helio-dvfsrc-ip-v2.h"
#include <helio-dvfsrc-opp.h>
#include <helio-dvfsrc-mt6893.h>
#include <mt-plat/mtk_devinfo.h>
#include <linux/regulator/consumer.h>
#include "mmdvfs_pmqos.h"
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sched/clock.h>
#ifdef	CONFIG_MTK_DBGTOP
#include <dbgtop.h>
#endif

//#define AUTOK_ENABLE

static struct reg_config dvfsrc_init_configs[][128] = {
	{
		{ DVFSRC_HRT_REQ_UNIT,       0x0000001E },
		{ DVFSRC_DEBOUNCE_TIME,      0x00001965 },
		{ DVFSRC_TIMEOUT_NEXTREQ,    0x0000001E },
		{ DVFSRC_LEVEL_MASK,         0x001DC000 },
		{ DVFSRC_LEVEL_MASK,	     0x00CCCC80 },

		{ DVFSRC_DDR_QOS0,           0x00000033 },
		{ DVFSRC_DDR_QOS1,           0x0000004C },
		{ DVFSRC_DDR_QOS2,           0x00000062 },
		{ DVFSRC_DDR_QOS3,           0x00000077 },
		{ DVFSRC_DDR_QOS4,           0x00000099 },
		{ DVFSRC_DDR_QOS5,           0x000000CC },
		{ DVFSRC_DDR_QOS6,           0x000000EE },

		{ DVFSRC_LEVEL_LABEL_0_1,    0x60547064 },
		{ DVFSRC_LEVEL_LABEL_2_3,    0x50436053 },
		{ DVFSRC_LEVEL_LABEL_4_5,    0x40335042 },
		{ DVFSRC_LEVEL_LABEL_6_7,    0x40314032 },
		{ DVFSRC_LEVEL_LABEL_8_9,    0x30223023 },
		{ DVFSRC_LEVEL_LABEL_10_11,  0x30203021 },
		{ DVFSRC_LEVEL_LABEL_12_13,  0x20122013 },
		{ DVFSRC_LEVEL_LABEL_14_15,  0x20102011 },
		{ DVFSRC_LEVEL_LABEL_16_17,  0x10021003 },
		{ DVFSRC_LEVEL_LABEL_18_19,  0x10001001 },
		{ DVFSRC_LEVEL_LABEL_20_21,  0x00120013 },
		{ DVFSRC_LEVEL_LABEL_22_23,  0x00100011 },

		{ DVFSRC_MD_LATENCY_IMPROVE, 0x00000040 },
		{ DVSFRC_HRT_REQ_MD_URG,     0x001AA1AA },
		{ DVFSRC_HRT_REQ_MD_BW_0,    0x00200802 },
		{ DVFSRC_HRT_REQ_MD_BW_1,    0x00200802 },
		{ DVFSRC_HRT_REQ_MD_BW_2,    0x00200800 },
		{ DVFSRC_HRT_REQ_MD_BW_3,    0x00400802 },
		{ DVFSRC_HRT_REQ_MD_BW_4,    0x00601404 },
		{ DVFSRC_HRT_REQ_MD_BW_5,    0x00D02C09 },
		{ DVFSRC_HRT_REQ_MD_BW_6,    0x00000012 },
		{ DVFSRC_HRT_REQ_MD_BW_7,    0x00000024 },
		{ DVFSRC_HRT_REQ_MD_BW_8,    0x00000000 },
		{ DVFSRC_HRT_REQ_MD_BW_9,    0x00000000 },
		{ DVFSRC_HRT_REQ_MD_BW_10,   0x0006A800 },
		{ DVFSRC_HRT1_REQ_MD_BW_0,   0x04B12C4B },
		{ DVFSRC_HRT1_REQ_MD_BW_1,   0x04B12C4B },
		{ DVFSRC_HRT1_REQ_MD_BW_2,   0x04B12C00 },
		{ DVFSRC_HRT1_REQ_MD_BW_3,   0x04B12C4B },
		{ DVFSRC_HRT1_REQ_MD_BW_4,   0x04B12C4B },
		{ DVFSRC_HRT1_REQ_MD_BW_5,   0x04B12C4B },
		{ DVFSRC_HRT1_REQ_MD_BW_6,   0x0000004B },
		{ DVFSRC_HRT1_REQ_MD_BW_7,   0x0000005C },
		{ DVFSRC_HRT1_REQ_MD_BW_8,   0x00000000 },
		{ DVFSRC_HRT1_REQ_MD_BW_9,   0x00000000 },
		{ DVFSRC_HRT1_REQ_MD_BW_10,  0x0006A800 },

		{ DVFSRC_95MD_SCEN_BW0_T,    0x40444440 },
		{ DVFSRC_95MD_SCEN_BW1_T,    0x43344444 },
		{ DVFSRC_95MD_SCEN_BW2_T,    0x04300444 },
		{ DVFSRC_95MD_SCEN_BW3_T,    0x60000000 },
		{ DVFSRC_95MD_SCEN_BW0,      0x20222220 },
		{ DVFSRC_95MD_SCEN_BW1,      0x22222222 },
		{ DVFSRC_95MD_SCEN_BW2,      0x02200222 },
		{ DVFSRC_95MD_SCEN_BW3,      0x60000000 },
		{ DVFSRC_95MD_SCEN_BW4,      0x00000006 },

		{ DVFSRC_DDR_REQUEST,        0x00004321 },
		{ DVFSRC_DDR_REQUEST3,       0x00000765 },
		{ DVFSRC_DDR_ADD_REQUEST,    0x76543210 },
		{ DVFSRC_EMI_ADD_REQUEST,    0x66543210 },
		{ DVFSRC_HRT_REQUEST,        0x77654321 },
		{ DVFSRC_DDR_REQUEST5,	     0x54321000 },
		{ DVFSRC_DDR_REQUEST7,       0x76000000 },
		{ DVFSRC_EMI_MON_DEBOUNCE_TIME,   0x4C2D0000 },
		{ DVFSRC_VCORE_REQUEST4,     0x21110000 },

		{ DVFSRC_HRT_HIGH_3,         0x3A543A54 },
		{ DVFSRC_HRT_HIGH_2,         0x314C2306 },
		{ DVFSRC_HRT_HIGH_1,         0x1AD21700 },
		{ DVFSRC_HRT_HIGH,           0x0E100960 },
		{ DVFSRC_HRT_LOW_3,          0x3A533A53 },
		{ DVFSRC_HRT_LOW_2,          0x314B2305 },
		{ DVFSRC_HRT_LOW_1,          0x1AD116FF },
		{ DVFSRC_HRT_LOW,            0x0E0F095F },

/*		{ DVFSRC_BASIC_CONTROL_3,    0x0000000E },*/
		{ DVFSRC_INT_EN,             0x00000002 },
		{ DVFSRC_QOS_EN,             0x0000407C },

		{ DVFSRC_CURRENT_FORCE,      0x00000004 },
		{ DVFSRC_BASIC_CONTROL,      0x66F8404B },
		{ DVFSRC_BASIC_CONTROL,      0x66F8014B },
		{ DVFSRC_CURRENT_FORCE,      0x00000000 },
		{ -1, 0 },
	},
	/* NULL */
	{
		{ -1, 0 },
	},
};

static ssize_t dvfsrc_level_mask_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", helio_dvfsrc_level_mask_get());
}
static ssize_t dvfsrc_level_mask_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int level = 0, en = 0;

	if (sscanf(buf, "%d %d", &level, &en) != 2)
		return -EINVAL;

	helio_dvfsrc_level_mask_set(en, level);

	return count;
}

static DEVICE_ATTR(dvfsrc_level_mask, 0644,
		dvfsrc_level_mask_show, dvfsrc_level_mask_store);


static ssize_t dvfsrc_vcore_settle_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* DE's comment: settle time was hard code in fw (15,30) */
	return sprintf(buf, "rising 15 uS, falling 30 uS for mt6893\n");
}

static DEVICE_ATTR(dvfsrc_vcore_settle_time, 0444,
		dvfsrc_vcore_settle_time_show, NULL);

static struct attribute *mt6893_helio_dvfsrc_attrs[] = {
	&dev_attr_dvfsrc_level_mask.attr,
	&dev_attr_dvfsrc_vcore_settle_time.attr,
	NULL,
};

static struct attribute_group mt6893_helio_dvfsrc_attr_group = {
	.name = "helio-dvfsrc",
	.attrs = mt6893_helio_dvfsrc_attrs,
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

		if (qos_total_bw < 0x33)
			return 0;
		else if (qos_total_bw < 0x4C)
			return 1;
		else if (qos_total_bw < 0x62)
			return 2;
		else if (qos_total_bw < 0x77)
			return 3;
		else if (qos_total_bw < 0x99)
			return 4;
		else if (qos_total_bw < 0xCC)
			return 5;
		else if (qos_total_bw < 0xEE)
			return 6;
		else
			return 7;

	return 0;
}


static int dvfsrc_get_emi_mon_gear(void)
{
	unsigned int total_bw_status;
	int i;

	total_bw_status = vcorefs_get_total_emi_status() & 0x7F;
	for (i = 6; i >= 0 ; i--) {
		if ((total_bw_status >> i) > 0)
			return i + 1;
	}

	return 0;
}

static u32 dvfsrc_calc_hrt_opp(int data)
{
	if (data < 0x0960)
		return DDR_OPP_7;
	else if (data < 0x0E10)
		return DDR_OPP_6;
	else if (data < 0x1700)
		return DDR_OPP_5;
	else if (data < 0x1AD2)
		return DDR_OPP_4;
	else if (data < 0x2306)
		return DDR_OPP_3;
	else if (data < 0x314C)
		return DDR_OPP_2;
	else if (data < 0x3A54)
		return DDR_OPP_1;
	else
		return DDR_OPP_0;
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
	return regulator_get(dev, "vgpu11");
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

	if (force >= 0 && force < VCORE_DVFS_OPP_NUM)
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
	struct platform_device *pdev = to_platform_device(dvfsrc->dev);
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	dvfsrc->spm_regs = devm_ioremap(&pdev->dev,
				res->start, resource_size(res));
	if (IS_ERR(dvfsrc->spm_regs))
		pr_info("not get spm register\n");

}

__weak void mtk_pm_qos_trace_dbg_dump(int mtk_pm_qos_class)
{
}

void dvfsrc_suspend_cb(struct helio_dvfsrc *dvfsrc)
{
	int sw_req;

	sw_req = dvfsrc_read(DVFSRC_SW_REQ3);
	pr_info("[DVFSRC] V:%d, F_OPP:%d, RG:%08x, %08x, %08x, %08x\n",
		get_cur_vcore_uv(),
		mtk_pm_qos_request(MTK_PM_QOS_VCORE_DVFS_FORCE_OPP),
		dvfsrc_read(DVFSRC_CURRENT_LEVEL),
		dvfsrc_read(DVFSRC_SW_REQ2),
		sw_req,
		dvfsrc_read(DVFSRC_DEBUG_STA_0));

	if (sw_req & (DDR_SW_AP_MASK << DDR_SW_AP_SHIFT))
		mtk_pm_qos_trace_dbg_dump(MTK_PM_QOS_DDR_OPP);

	if (sw_req & (VCORE_SW_AP_MASK << VCORE_SW_AP_SHIFT))
		mtk_pm_qos_trace_dbg_dump(MTK_PM_QOS_VCORE_OPP);
}

void dvfsrc_resume_cb(struct helio_dvfsrc *dvfsrc)
{

}

#define DVFSRC_MD_UP_SHIFT 28
void helio_dvfsrc_platform_init(struct helio_dvfsrc *dvfsrc)
{
	int spmfw_idx = 0;
	struct reg_config *config;
	int idx = 0;
	int dvfsrc_rsrv;


	sysfs_merge_group(&dvfsrc->dev->kobj, &mt6893_helio_dvfsrc_attr_group);

	dvfsrc_rsrv = readl(dvfsrc->regs + DVFSRC_RSRV_4);

	if (((dvfsrc_rsrv >> DVFSRC_MD_UP_SHIFT) & 0x1))
		dvfsrc_write(DVFSRC_BASIC_CONTROL_3, 0xE);
	else
		dvfsrc_write(DVFSRC_BASIC_CONTROL_3, 0x6);

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

void get_spm_reg(char *p)
{
	p += sprintf(p, "%-24s: 0x%08x\n",
			"POWERON_CONFIG_EN",
			spm_reg_read(POWERON_CONFIG_EN));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_SW_FLAG_0",
			spm_reg_read(SPM_SW_FLAG_0));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_PC_STA",
			spm_reg_read(SPM_PC_STA));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_DVFS_LEVEL",
			spm_reg_read(SPM_DVFS_LEVEL));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_DVS_DFS_LEVEL",
			spm_reg_read(SPM_DVS_DFS_LEVEL));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_DVFS_STA",
			spm_reg_read(SPM_DVFS_STA));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_DVFS_MISC",
			spm_reg_read(SPM_DVFS_MISC));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x 0x%08x\n",
			"SPM_DVFS_CMD16~20",
			spm_reg_read(SPM_DVFS_CMD16),
			spm_reg_read(SPM_DVFS_CMD17),
			spm_reg_read(SPM_DVFS_CMD18),
			spm_reg_read(SPM_DVFS_CMD19),
			spm_reg_read(SPM_DVFS_CMD20));
	p += sprintf(p, "%-16s: %x , 0x%08x\n",
			"MD_MASK",
			dvfsrc_read(DVFSRC_BASIC_CONTROL_3),
			dvfsrc_read(DVFSRC_LEVEL_MASK));
}

void get_opp_info(char *p)
{
#if defined(CONFIG_FPGA_EARLY_PORTING) || !defined(CONFIG_MTK_PMIC_COMMON)
	int pmic_val = 0;
#else
	int pmic_val = pmic_get_register_value(PMIC_VCORE_ADDR);
#endif
#ifdef CONFIG_MEDIATEK_DRAMC
	int ddr_khz = mtk_dramc_get_data_rate() * 1000;
#else
	int ddr_khz = 0;
#endif
	int vcore_uv = vcore_pmic_to_uv(pmic_val);

	p += sprintf(p, "%-10s: %-8u uv  (PMIC: 0x%x)\n",
			"Vcore", vcore_uv, vcore_uv_to_pmic(vcore_uv));
	p += sprintf(p, "%-10s: %-8u khz\n", "DDR", ddr_khz);
	p += sprintf(p, "%6s: 0x%08x\n", "INFO1", get_devinfo_with_index(134));
	p += sprintf(p, "%6s: 0x%08x\n", "INFO2", get_devinfo_with_index(140));
	p += sprintf(p, "%6s: 0x%08x\n", "INFO3", get_devinfo_with_index(69));
	p += sprintf(p, "%6s: 0x%08x\n", "V_MODE", dvfsrc_read(DVFSRC_RSRV_4));
}


/* met profile table */
static unsigned int met_vcorefs_src[SRC_MAX];

static char *met_src_name[SRC_MAX] = {
	"MD2SPM",
	"SRC_DDR_OPP",
	"DDR__SW_REQ1_SPM",
	"DDR__SW_REQ2_CM",
	"DDR__SW_REQ3_PMQOS",
	"DDR__QOS_BW",
	"DDR__EMI_TOTAL",
	"DDR__HRT_BW",
	"DDR__HIFI",
	"DDR__HIFI_LATENCY",
	"DDR__MD_LATENCY",
	"DDR__MD_DDR",
	"DDR__MD_LEVEL_MASK",
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

static u32 vcorefs_get_md_level_mask_ddr(void)
{
	int md_srclk, vopp;

	if (dvfsrc_read(DVFSRC_BASIC_CONTROL_3) & 0x8) {
		md_srclk = dvfsrc_read(DVFSRC_DEBUG_STA_0);
		vopp = get_cur_vcore_opp();
		md_srclk = (md_srclk >> MD_SRC_CLK_DEBUG_SHIFT)
			& MD_SRC_CLK_DEBUG_MASK;

		if (vopp != 4 && md_srclk == 1)
			return 2;
	}

	return 0;
}

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
		vcorefs_get_md_imp_ddr();

	met_vcorefs_src[DDR_MD_DDR_IDX] =
		vcorefs_get_md_scenario_ddr();

	met_vcorefs_src[DDR_MD_SRCLK_IDX] =
		vcorefs_get_md_level_mask_ddr();
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
	u32 qos_bw0, qos_bw1, qos_bw2, qos_bw3, qos_bw4;

	qos_bw0 = dvfsrc_read(DVFSRC_SW_BW_0);
	qos_bw1 = dvfsrc_read(DVFSRC_SW_BW_1);
	qos_bw2 = dvfsrc_read(DVFSRC_SW_BW_2);
	qos_bw3 = dvfsrc_read(DVFSRC_SW_BW_3);
	qos_bw4 = dvfsrc_read(DVFSRC_SW_BW_4);

	met_vcorefs_src[SRC_MD2SPM_IDX] =
		vcorefs_get_md_scenario() & 0x1FFFF;

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

int dvfsrc_latch_register(int enable)
{
#ifdef	CONFIG_MTK_DBGTOP
	return mtk_dbgtop_cfg_dvfsrc(1);
#else
	return 0;
#endif
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

	if (get_ddr_opp(idx) < DDR_OPP_7)
		return 8;
	else
		return 4;
}
EXPORT_SYMBOL(get_cur_ddr_ratio);

