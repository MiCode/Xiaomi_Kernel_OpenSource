/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/string.h>

#include <mtk_dramc.h>
#include <mt-plat/upmu_common.h>
#include <ext_wd_drv.h>
#include <mt_emi_api.h>
#include <mt-plat/mtk_devinfo.h>

#include <helio-dvfsrc_v2.h>
#include <helio-dvfsrc-opp.h>
#include <mtk_dvfsrc_reg.h>
#include <mtk_spm_internal.h>
#include <spm/mtk_vcore_dvfs.h>
#include <mt6765/mtk_gpufreq.h>

#include <mmdvfs_pmqos.h>

static struct reg_config dvfsrc_init_configs[][128] = {
	/* SPMFW_LP4_2CH_3200 */
	{
		{ DVFSRC_EMI_REQUEST,		0x00240009 },
		{ DVFSRC_EMI_REQUEST3,		0x09000000 },
		{ DVFSRC_EMI_HRT,		0x003E362C },
		{ DVFSRC_EMI_QOS0,		0x00000033 },
		{ DVFSRC_EMI_QOS1,		0x0000004C },
		{ DVFSRC_EMI_MD2SPM0_T,		0x00000007 },
		{ DVFSRC_EMI_MD2SPM1_T,		0x00000038 },
		{ DVFSRC_EMI_MD2SPM2_T,		0x000080C0 },
		{ DVFSRC_VCORE_MD2SPM1_T,	0x000080C0 },
		{ DVFSRC_VCORE_REQUEST,		0x000C0000 },

		{ DVFSRC_VCORE_HRT,		0x00000036 },

		{ DVFSRC_MD_SW_CONTROL,		0x20000000 },

		{ DVFSRC_TIMEOUT_NEXTREQ,	0x00000014 },
		{ DVFSRC_INT_EN,		0x00000003 },

		{ DVFSRC_LEVEL_LABEL_0_1,	0x00010000 },
		{ DVFSRC_LEVEL_LABEL_2_3,	0x00020101 },
		{ DVFSRC_LEVEL_LABEL_4_5,	0x01020012 },
		{ DVFSRC_LEVEL_LABEL_6_7,	0x02120112 },
		{ DVFSRC_LEVEL_LABEL_8_9,	0x00230013 },
		{ DVFSRC_LEVEL_LABEL_10_11,	0x01230113 },
		{ DVFSRC_LEVEL_LABEL_12_13,	0x02230213 },
		{ DVFSRC_LEVEL_LABEL_14_15,	0x03230323 },

		{ DVFSRC_FORCE,			0x20000000 },
		{ DVFSRC_RSRV_1,		0x0000000C },

		{ DVFSRC_QOS_EN,		0x0000407F },

		{ DVFSRC_BASIC_CONTROL,		0x0000407B },

		{ DVFSRC_FORCE,			0x00000000 },
		{ DVFSRC_BASIC_CONTROL,		0x0000017B },

		{ -1, 0 },
	},
	/* SPMFW_LP4X_2CH_3200 */
	{
		{ DVFSRC_EMI_REQUEST,		0x00240009 },
		{ DVFSRC_EMI_REQUEST3,		0x09000000 },
		{ DVFSRC_EMI_HRT,		0x003E362C },
		{ DVFSRC_EMI_QOS0,		0x00000033 },
		{ DVFSRC_EMI_QOS1,		0x0000004C },
		{ DVFSRC_EMI_MD2SPM0,		0x0000003F },
		{ DVFSRC_EMI_MD2SPM1,		0x00000000 },
		{ DVFSRC_EMI_MD2SPM2,		0x000080C0 },
		{ DVFSRC_EMI_MD2SPM0_T,		0x00000007 },
		{ DVFSRC_EMI_MD2SPM1_T,		0x00000038 },
		{ DVFSRC_EMI_MD2SPM2_T,		0x000080C0 },

		{ DVFSRC_VCORE_HRT,		0x00000036 },

		{ DVFSRC_MD_SW_CONTROL,		0x20000000 },

		{ DVFSRC_TIMEOUT_NEXTREQ,	0x00000014 },
		{ DVFSRC_INT_EN,		0x00000003 },

		{ DVFSRC_LEVEL_LABEL_0_1,	0x00010000 },
		{ DVFSRC_LEVEL_LABEL_2_3,	0x00020101 },
		{ DVFSRC_LEVEL_LABEL_4_5,	0x01020012 },
		{ DVFSRC_LEVEL_LABEL_6_7,	0x02120112 },
		{ DVFSRC_LEVEL_LABEL_8_9,	0x00230013 },
		{ DVFSRC_LEVEL_LABEL_10_11,	0x01230113 },
		{ DVFSRC_LEVEL_LABEL_12_13,	0x02230213 },
		{ DVFSRC_LEVEL_LABEL_14_15,	0x03230323 },

		{ DVFSRC_FORCE,			0x40000000 },
		{ DVFSRC_RSRV_1,		0x0000000C },

		{ DVFSRC_QOS_EN,		0x0000407F },

		{ DVFSRC_BASIC_CONTROL,		0x0000407B },

		{ DVFSRC_FORCE,			0x00000000 },
		{ DVFSRC_BASIC_CONTROL,		0x0000017B },

		{ -1, 0 },
	},
	/* SPMFW_LP3_1CH_1866 */
	{
		{ DVFSRC_EMI_REQUEST,		0x00240009 },
		{ DVFSRC_EMI_REQUEST3,		0x09000000 },
		{ DVFSRC_EMI_HRT,		0x00000020 },
		{ DVFSRC_EMI_QOS0,		0x00000026 },
		{ DVFSRC_EMI_QOS1,		0x00000033 },
		{ DVFSRC_EMI_MD2SPM0,		0x0000003F },
		{ DVFSRC_EMI_MD2SPM1,		0x00000000 },
		{ DVFSRC_EMI_MD2SPM2,		0x000080C0 },
		{ DVFSRC_EMI_MD2SPM0_T,		0x00000007 },
		{ DVFSRC_EMI_MD2SPM1_T,		0x00000038 },
		{ DVFSRC_EMI_MD2SPM2_T,		0x000080C0 },

		{ DVFSRC_VCORE_HRT,		0x00000020 },

		{ DVFSRC_MD_SW_CONTROL,		0x20000000 },

		{ DVFSRC_TIMEOUT_NEXTREQ,	0x00000014 },
		{ DVFSRC_INT_EN,		0x00000003 },

		{ DVFSRC_LEVEL_LABEL_0_1,	0x00010000 },
		{ DVFSRC_LEVEL_LABEL_2_3,	0x00020101 },
		{ DVFSRC_LEVEL_LABEL_4_5,	0x01020012 },
		{ DVFSRC_LEVEL_LABEL_6_7,	0x02120112 },
		{ DVFSRC_LEVEL_LABEL_8_9,	0x00230013 },
		{ DVFSRC_LEVEL_LABEL_10_11,	0x01230113 },
		{ DVFSRC_LEVEL_LABEL_12_13,	0x02230213 },
		{ DVFSRC_LEVEL_LABEL_14_15,	0x03230323 },

		{ DVFSRC_FORCE,			0x20000000 },
		{ DVFSRC_RSRV_1,		0x0000000C },

		{ DVFSRC_QOS_EN,		0x0000407F },

		{ DVFSRC_BASIC_CONTROL,		0x0000407B },

		{ DVFSRC_FORCE,			0x00000000 },
		{ DVFSRC_BASIC_CONTROL,		0x0000017B },

		{ -1, 0 },
	},
	/* SPMFW_LP4_2CH_2400 */
	{
		{ DVFSRC_EMI_REQUEST,		0x00240009 },
		{ DVFSRC_EMI_REQUEST3,		0x09000000 },
		{ DVFSRC_EMI_HRT,		0x003E362C },
		{ DVFSRC_EMI_QOS0,		0x00000033 },
		{ DVFSRC_EMI_QOS1,		0x0000004C },
		{ DVFSRC_EMI_MD2SPM0_T,		0x00000007 },
		{ DVFSRC_EMI_MD2SPM1_T,		0x00000038 },
		{ DVFSRC_EMI_MD2SPM2_T,		0x000080C0 },
		{ DVFSRC_VCORE_MD2SPM1_T,	0x000080C0 },
		{ DVFSRC_VCORE_REQUEST,		0x000C0000 },

		{ DVFSRC_VCORE_HRT,		0x00000036 },

		{ DVFSRC_MD_SW_CONTROL,		0x20000000 },

		{ DVFSRC_TIMEOUT_NEXTREQ,	0x00000014 },
		{ DVFSRC_INT_EN,		0x00000003 },

		{ DVFSRC_LEVEL_LABEL_0_1,	0x00010000 },
		{ DVFSRC_LEVEL_LABEL_2_3,	0x00020101 },
		{ DVFSRC_LEVEL_LABEL_4_5,	0x01020012 },
		{ DVFSRC_LEVEL_LABEL_6_7,	0x02120112 },
		{ DVFSRC_LEVEL_LABEL_8_9,	0x00230013 },
		{ DVFSRC_LEVEL_LABEL_10_11,	0x01230113 },
		{ DVFSRC_LEVEL_LABEL_12_13,	0x02230213 },
		{ DVFSRC_LEVEL_LABEL_14_15,	0x03230323 },

		{ DVFSRC_FORCE,			0x20000000 },
		{ DVFSRC_RSRV_1,		0x0000000C },

		{ DVFSRC_QOS_EN,		0x0000407F },

		{ DVFSRC_BASIC_CONTROL,		0x0000407B },

		{ DVFSRC_FORCE,			0x00000000 },
		{ DVFSRC_BASIC_CONTROL,		0x0000017B },

		{ -1, 0 },
	},
	/* NULL */
	{
		{ -1, 0 },
	},
};

struct reg_config *dvfsrc_get_init_conf(void)
{
	int spmfw_idx = spm_get_spmfw_idx();

	if (spmfw_idx < 0)
		spmfw_idx = ARRAY_SIZE(dvfsrc_init_configs) - 1;

	return dvfsrc_init_configs[spmfw_idx];
}

void dvfsrc_update_md_scenario(bool blank)
{
	switch (spm_get_spmfw_idx()) {
	case SPMFW_LP4_2CH_3200:
	case SPMFW_LP4_2CH_2400:
		/* fall through*/
	case SPMFW_LP4X_2CH_3200:
		if (blank) {
			dvfsrc_write(DVFSRC_EMI_MD2SPM0_T, 0x0000003F);
			dvfsrc_write(DVFSRC_EMI_MD2SPM1_T, 0x00000000);
		} else {
			dvfsrc_write(DVFSRC_EMI_MD2SPM0_T, 0x00000007);
			dvfsrc_write(DVFSRC_EMI_MD2SPM1_T, 0x00000038);
		}
		break;
	case SPMFW_LP3_1CH_1866:
		if (blank) {
			dvfsrc_write(DVFSRC_EMI_MD2SPM0_T, 0x0000003F);
			dvfsrc_write(DVFSRC_EMI_MD2SPM1_T, 0x00000000);
		} else {
			dvfsrc_write(DVFSRC_EMI_MD2SPM0_T, 0x00000007);
			dvfsrc_write(DVFSRC_EMI_MD2SPM1_T, 0x00000038);
		}
		break;
	default:
		break;
	}
}

static int dvfsrc_fb_notifier_call(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
		dvfsrc_update_md_scenario(false);
		break;
	case FB_BLANK_POWERDOWN:
		dvfsrc_update_md_scenario(true);
		break;
	default:
		break;
	}

	return 0;
}

static struct notifier_block dvfsrc_fb_notifier = {
	.notifier_call = dvfsrc_fb_notifier_call,
};

static int is_efuse_bypass_flavor(void)
{
	int r = 0;
#if defined(CONFIG_ARM64)
	int len;

	len = sizeof(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES);

	if (strncmp(&CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES[len - 4],
				"_lp", 3) == 0)
		r = 1;

	pr_info("flavor check: %s, is_bypass: %d\n",
		CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES, r);
#endif
	return r;
}

static int can_dvfsrc_enable(void)
{
	int enable = 0;
	int ptpod0 = get_devinfo_with_index(50);

	pr_info("%s: PTPOD0: 0x%x\n", __func__, ptpod0);

	if (is_efuse_bypass_flavor()) {
		enable = 1;
		pr_info("VCORE DVFS enable for special flavor\n");
	} else if (ptpod0 == 0x0000FF00 || ptpod0 == 0x0) {
		enable = 0;
		pr_info("VCORE DVFS disable for efuse\n");
	} else {
		enable = 1;
		pr_info("VCORE DVFS enable default\n");
	}

	return enable;
}

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
	/* notify GPU DVFS for msdc autok start */
	mt_gpufreq_disable_by_ptpod();
}

void finish_autok_task(void)
{
	/* check if dvfs force is released */
	int force = pm_qos_request(PM_QOS_VCORE_DVFS_FORCE_OPP);

	/* notify MM DVFS for msdc autok finish */
	mmdvfs_prepare_action(MMDVFS_PREPARE_CALIBRATION_END);
	/* notify GPU DVFS for msdc autok finish */
	mt_gpufreq_enable_by_ptpod();

	if (force >= 0 && force < 16)
		pr_info("autok task not release force opp: %d\n", force);
}

void dvfsrc_autok_manager(void)
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
int helio_dvfsrc_platform_init(struct helio_dvfsrc *dvfsrc)
{
	mtk_rgu_cfg_dvfsrc(1);
	helio_dvfsrc_sram_reg_init();

	if (can_dvfsrc_enable())
		helio_dvfsrc_enable(1);
	else
		helio_dvfsrc_enable(0);

	dvfsrc->init_config = dvfsrc_get_init_conf();
	helio_dvfsrc_reg_config(dvfsrc->init_config);

	dvfsrc_autok_manager();

	return fb_register_client(&dvfsrc_fb_notifier);
}

void get_opp_info(char *p)
{
	int pmic_val = pmic_get_register_value(PMIC_VCORE_ADDR);
	int vcore_uv = vcore_pmic_to_uv(pmic_val);
	int ddr_khz = get_dram_data_rate() * 1000;

	p += sprintf(p, "%-24s: %-8u uv  (PMIC: 0x%x)\n",
			"Vcore", vcore_uv, vcore_uv_to_pmic(vcore_uv));
	p += sprintf(p, "%-24s: %-8u khz\n", "DDR", ddr_khz);

}

void get_dvfsrc_reg(char *p)
{
	p += sprintf(p, "%-24s: 0x%08x\n",
			"DVFSRC_BASIC_CONTROL",
			dvfsrc_read(DVFSRC_BASIC_CONTROL));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x\n",
			"DVFSRC_SW_REQ(2)",
			dvfsrc_read(DVFSRC_SW_REQ),
			dvfsrc_read(DVFSRC_SW_REQ2));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_EMI_QOS0(1)(2)",
			dvfsrc_read(DVFSRC_EMI_QOS0),
			dvfsrc_read(DVFSRC_EMI_QOS1),
			dvfsrc_read(DVFSRC_EMI_QOS2));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_EMI_REQUEST(2)(3)",
			dvfsrc_read(DVFSRC_EMI_REQUEST),
			dvfsrc_read(DVFSRC_EMI_REQUEST2),
			dvfsrc_read(DVFSRC_EMI_REQUEST3));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_EMI_MD2SPM0~2",
			dvfsrc_read(DVFSRC_EMI_MD2SPM0),
			dvfsrc_read(DVFSRC_EMI_MD2SPM1),
			dvfsrc_read(DVFSRC_EMI_MD2SPM2));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_EMI_MD2SPM0~2_T",
			dvfsrc_read(DVFSRC_EMI_MD2SPM0_T),
			dvfsrc_read(DVFSRC_EMI_MD2SPM1_T),
			dvfsrc_read(DVFSRC_EMI_MD2SPM2_T));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x\n",
			"DVFSRC_VCORE_REQUEST(2)",
			dvfsrc_read(DVFSRC_VCORE_REQUEST),
			dvfsrc_read(DVFSRC_VCORE_REQUEST2));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_VCORE_MD2SPM0~2",
			dvfsrc_read(DVFSRC_VCORE_MD2SPM0),
			dvfsrc_read(DVFSRC_VCORE_MD2SPM1),
			dvfsrc_read(DVFSRC_VCORE_MD2SPM2));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_VCORE_MD2SPM0~2_T",
			dvfsrc_read(DVFSRC_VCORE_MD2SPM0_T),
			dvfsrc_read(DVFSRC_VCORE_MD2SPM1_T),
			dvfsrc_read(DVFSRC_VCORE_MD2SPM2_T));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"DVFSRC_MD_REQUEST",
			dvfsrc_read(DVFSRC_MD_REQUEST));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"DVFSRC_INT",
			dvfsrc_read(DVFSRC_INT));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"DVFSRC_INT_EN",
			dvfsrc_read(DVFSRC_INT_EN));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"DVFSRC_LEVEL",
			dvfsrc_read(DVFSRC_LEVEL));
	p += sprintf(p, "%-24s: %d, %d, %d, %d, %d\n",
			"DVFSRC_SW_BW_0~4",
			dvfsrc_read(DVFSRC_SW_BW_0),
			dvfsrc_read(DVFSRC_SW_BW_1),
			dvfsrc_read(DVFSRC_SW_BW_2),
			dvfsrc_read(DVFSRC_SW_BW_3),
			dvfsrc_read(DVFSRC_SW_BW_4));
}

void get_dvfsrc_record(char *p)
{
	p += sprintf(p, "%-24s: 0x%08x\n",
			"DVFSRC_FORCE",
			dvfsrc_read(DVFSRC_FORCE));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"DVFSRC_SEC_SW_REQ",
			dvfsrc_read(DVFSRC_SEC_SW_REQ));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"DVFSRC_LAST",
			dvfsrc_read(DVFSRC_LAST));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"DVFSRC_MD_SCENARIO",
			dvfsrc_read(DVFSRC_MD_SCENARIO));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_RECORD_0_0~0_2",
			dvfsrc_read(DVFSRC_RECORD_0_0),
			dvfsrc_read(DVFSRC_RECORD_0_1),
			dvfsrc_read(DVFSRC_RECORD_0_2));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_RECORD_1_0~1_2",
			dvfsrc_read(DVFSRC_RECORD_1_0),
			dvfsrc_read(DVFSRC_RECORD_1_1),
			dvfsrc_read(DVFSRC_RECORD_1_2));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_RECORD_2_0~2_2",
			dvfsrc_read(DVFSRC_RECORD_2_0),
			dvfsrc_read(DVFSRC_RECORD_2_1),
			dvfsrc_read(DVFSRC_RECORD_2_2));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_RECORD_3_0~3_2",
			dvfsrc_read(DVFSRC_RECORD_3_0),
			dvfsrc_read(DVFSRC_RECORD_3_1),
			dvfsrc_read(DVFSRC_RECORD_3_2));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_RECORD_4_0~4_2",
			dvfsrc_read(DVFSRC_RECORD_4_0),
			dvfsrc_read(DVFSRC_RECORD_4_1),
			dvfsrc_read(DVFSRC_RECORD_4_2));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_RECORD_5_0~5_2",
			dvfsrc_read(DVFSRC_RECORD_5_0),
			dvfsrc_read(DVFSRC_RECORD_5_1),
			dvfsrc_read(DVFSRC_RECORD_5_2));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_RECORD_6_0~6_2",
			dvfsrc_read(DVFSRC_RECORD_6_0),
			dvfsrc_read(DVFSRC_RECORD_6_1),
			dvfsrc_read(DVFSRC_RECORD_6_2));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_RECORD_7_0~7_2",
			dvfsrc_read(DVFSRC_RECORD_7_0),
			dvfsrc_read(DVFSRC_RECORD_7_1),
			dvfsrc_read(DVFSRC_RECORD_7_2));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_RECORD_MD_0~3",
			dvfsrc_read(DVFSRC_RECORD_MD_0),
			dvfsrc_read(DVFSRC_RECORD_MD_1),
			dvfsrc_read(DVFSRC_RECORD_MD_2),
			dvfsrc_read(DVFSRC_RECORD_MD_3));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_RECORD_MD_4~7",
			dvfsrc_read(DVFSRC_RECORD_MD_4),
			dvfsrc_read(DVFSRC_RECORD_MD_5),
			dvfsrc_read(DVFSRC_RECORD_MD_6),
			dvfsrc_read(DVFSRC_RECORD_MD_7));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"DVFSRC_RECORD_COUNT",
			dvfsrc_read(DVFSRC_RECORD_COUNT));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"DVFSRC_RSRV_0",
			dvfsrc_read(DVFSRC_RSRV_0));
}

/* met profile table */
unsigned int met_vcorefs_info[INFO_MAX];
unsigned int met_vcorefs_src[SRC_MAX];

char *met_info_name[INFO_MAX] = {
	"OPP",
	"FREQ",
	"VCORE",
	"SPM_LEVEL",
};

char *met_src_name[SRC_MAX] = {
	"MD2SPM",
	"QOS_EMI_LEVEL",
	"QOS_VCORE_LEVEL",
	"CM_MGR_LEVEL",
	"TOTAL_EMI_LEVEL_1",
	"TOTAL_EMI_LEVEL_2",
	"TOTAL_EMI_RESULT",
	"QOS_BW_LEVEL1",
	"QOS_BW_LEVEL2",
	"QOS_BW_RESULT",
	"SCP_VCORE_LEVEL",
};

/* met profile function */
int vcorefs_get_num_opp(void)
{
	return VCORE_DVFS_OPP_NUM;
}
EXPORT_SYMBOL(vcorefs_get_num_opp);

int vcorefs_get_opp_info_num(void)
{
	return INFO_MAX;
}
EXPORT_SYMBOL(vcorefs_get_opp_info_num);

int vcorefs_get_src_req_num(void)
{
	return SRC_MAX;
}
EXPORT_SYMBOL(vcorefs_get_src_req_num);

char **vcorefs_get_opp_info_name(void)
{
	return met_info_name;
}
EXPORT_SYMBOL(vcorefs_get_opp_info_name);

char **vcorefs_get_src_req_name(void)
{
	return met_src_name;
}
EXPORT_SYMBOL(vcorefs_get_src_req_name);

unsigned int *vcorefs_get_opp_info(void)
{
	met_vcorefs_info[INFO_OPP_IDX] = get_cur_vcore_dvfs_opp();
	met_vcorefs_info[INFO_FREQ_IDX] = get_cur_ddr_khz();
	met_vcorefs_info[INFO_VCORE_IDX] = get_cur_vcore_uv();
	met_vcorefs_info[INFO_SPM_LEVEL_IDX] = spm_get_dvfs_level();

	return met_vcorefs_info;
}
EXPORT_SYMBOL(vcorefs_get_opp_info);

unsigned int *vcorefs_get_src_req(void)
{
	unsigned int qos_total_bw = dvfsrc_read(DVFSRC_SW_BW_0) +
			   dvfsrc_read(DVFSRC_SW_BW_1) +
			   dvfsrc_read(DVFSRC_SW_BW_2) +
			   dvfsrc_read(DVFSRC_SW_BW_3) +
			   dvfsrc_read(DVFSRC_SW_BW_4);
	unsigned int total_bw_status = get_emi_bwst(0);
	unsigned int total_bw_last = (get_emi_bwvl(0) & 0x7F) * 813;
	unsigned int qos0_thres = dvfsrc_read(DVFSRC_EMI_QOS0);
	unsigned int qos1_thres = dvfsrc_read(DVFSRC_EMI_QOS1);
	unsigned int sw_req = dvfsrc_read(DVFSRC_SW_REQ);

	met_vcorefs_src[SRC_MD2SPM_IDX] =
		spm_vcorefs_get_MD_status();

	met_vcorefs_src[SRC_QOS_EMI_LEVEL_IDX] =
		(sw_req >> EMI_SW_AP_SHIFT) & EMI_SW_AP_MASK;

	met_vcorefs_src[SRC_QOS_VCORE_LEVEL_IDX] =
		(sw_req >> VCORE_SW_AP_SHIFT) & VCORE_SW_AP_MASK;

	met_vcorefs_src[SRC_CM_MGR_LEVEL_IDX] =
		(dvfsrc_read(DVFSRC_SW_REQ2) >> EMI_SW_AP2_SHIFT) &
			EMI_SW_AP2_MASK;

	met_vcorefs_src[SRC_TOTAL_EMI_LEVEL_1_IDX] =
		total_bw_status & 0x1;
	met_vcorefs_src[SRC_TOTAL_EMI_LEVEL_2_IDX] =
		(total_bw_status >> 1) & 0x1;
	met_vcorefs_src[SRC_TOTAL_EMI_RESULT_IDX] =
		total_bw_last;

	met_vcorefs_src[SRC_QOS_BW_LEVEL1_IDX] =
		(qos_total_bw >= qos0_thres) ? 1 : 0;
	met_vcorefs_src[SRC_QOS_BW_LEVEL2_IDX] =
		(qos_total_bw >= qos1_thres) ? 1 : 0;
	met_vcorefs_src[SRC_QOS_BW_RESUT_IDX] =
		qos_total_bw * 100;

	met_vcorefs_src[SRC_SCP_VCORE_LEVEL_IDX] =
	(dvfsrc_read(DVFSRC_VCORE_REQUEST) >> VCORE_SCP_GEAR_SHIFT) &
	VCORE_SCP_GEAR_MASK;

	return met_vcorefs_src;
}
EXPORT_SYMBOL(vcorefs_get_src_req);

/* gps workarund function */
static int is_freq_hopping;

void dvfsrc_enable_dvfs_freq_hopping(int gps_on)
{
	static struct pm_qos_request gps_vcore_req;
	static struct pm_qos_request gps_ddr_req;

	if (!is_dvfsrc_enabled())
		return;

	if (spm_get_spmfw_idx() == SPMFW_LP3_1CH_1866)
		return;

	if (!pm_qos_request_active(&gps_vcore_req))
		pm_qos_add_request(&gps_vcore_req,
			PM_QOS_VCORE_OPP, PM_QOS_VCORE_OPP_DEFAULT_VALUE);

	if (!pm_qos_request_active(&gps_ddr_req))
		pm_qos_add_request(&gps_ddr_req,
			PM_QOS_DDR_OPP, PM_QOS_DDR_OPP_DEFAULT_VALUE);

	pm_qos_update_request(&gps_vcore_req, VCORE_OPP_0);
	pm_qos_update_request(&gps_ddr_req, DDR_OPP_0);

	pr_info("[before]gps_on: %d, vcore: %d ddr: %d dvfsrc_level: 0x%x\n",
		gps_on,
		vcore_pmic_to_uv(pmic_get_register_value(PMIC_VCORE_ADDR)),
		get_dram_data_rate(),
		dvfsrc_read(DVFSRC_LEVEL));

	spm_freq_hopping_cmd(!!gps_on);

	pm_qos_update_request(&gps_ddr_req, DDR_OPP_UNREQ);
	pm_qos_update_request(&gps_vcore_req, VCORE_OPP_UNREQ);

	is_freq_hopping = !!gps_on;
	pr_info("[after]gps_on: %d, vcore: %d ddr: %d dvfsrc_level: 0x%x\n",
		gps_on,
		vcore_pmic_to_uv(pmic_get_register_value(PMIC_VCORE_ADDR)),
		get_dram_data_rate(),
		dvfsrc_read(DVFSRC_LEVEL));
}
EXPORT_SYMBOL(dvfsrc_enable_dvfs_freq_hopping);

int dvfsrc_get_dvfs_freq_hopping_status(void)
{
	return is_freq_hopping;
}

