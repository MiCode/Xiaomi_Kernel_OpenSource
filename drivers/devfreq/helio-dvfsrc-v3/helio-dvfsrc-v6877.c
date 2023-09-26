/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include <linux/platform_device.h>
#ifdef CONFIG_MEDIATEK_DRAMC
#include <dramc.h>
#endif
#ifdef CONFIG_MTK_EMI
#include <mt_emi_api.h>
#endif

#include <mt-plat/upmu_common.h>
#include "helio-dvfsrc-ip-v2.h"
#include <helio-dvfsrc-opp.h>
#include <helio-dvfsrc-v6877.h>
#include <mt-plat/mtk_devinfo.h>
#include <linux/regulator/consumer.h>
#include "mmdvfs_pmqos.h"
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sched/clock.h>
#include <dbgtop.h>

#define dvfsrc_rmw(offset, val, mask, shift) \
	dvfsrc_write(offset, (dvfsrc_read(offset) & ~(mask << shift)) \
			| (val << shift))

#define V_OPP_TYPE_SHIFT 20

#ifdef	CONFIG_MTK_DVFSRC_MT6877_PRETEST
static struct reg_config dvfsrc_init_configs[][128] = {
	{
		{ DVFSRC_HRT_REQ_UNIT,       0x0000001E },
		{ DVFSRC_DEBOUNCE_TIME,      0x00001965 },
		{ DVFSRC_TIMEOUT_NEXTREQ,    0x00000015 },

		{ DVFSRC_DDR_QOS0,	     0x00000019 },
		{ DVFSRC_DDR_QOS1,	     0x00000026 },
		{ DVFSRC_DDR_QOS2,	     0x00000033 },
		{ DVFSRC_DDR_QOS3,	     0x0000003B },
		{ DVFSRC_DDR_QOS4,	     0x00000055 },
		{ DVFSRC_DDR_QOS5,	     0x00000077 },
		{ DVFSRC_DDR_QOS6,	     0x00000088 },

		{ DVFSRC_LEVEL_LABEL_0_1,	0x60647074 },
		{ DVFSRC_LEVEL_LABEL_2_3,	0x50546063 },
		{ DVFSRC_LEVEL_LABEL_4_5,	0x50525053 },
		{ DVFSRC_LEVEL_LABEL_6_7,	0x40434044 },
		{ DVFSRC_LEVEL_LABEL_8_9,	0x40414042 },
		{ DVFSRC_LEVEL_LABEL_10_11,	0x30333034 },
		{ DVFSRC_LEVEL_LABEL_12_13,	0x30313032 },
		{ DVFSRC_LEVEL_LABEL_14_15,	0x20243030 },
		{ DVFSRC_LEVEL_LABEL_16_17,	0x20222023 },
		{ DVFSRC_LEVEL_LABEL_18_19,	0x20202021 },
		{ DVFSRC_LEVEL_LABEL_20_21,	0x10131014 },
		{ DVFSRC_LEVEL_LABEL_22_23,	0x10111012 },
		{ DVFSRC_LEVEL_LABEL_24_25,	0x00041010 },
		{ DVFSRC_LEVEL_LABEL_26_27,	0x00020003 },
		{ DVFSRC_LEVEL_LABEL_28_29,	0x00000001 },

		{ DVFSRC_MD_LATENCY_IMPROVE, 0x00000040 },
		{ DVFSRC_HRT_BW_BASE,        0x00000004 },
		{ DVSFRC_HRT_REQ_MD_URG,     0x000D20D2 },
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
		{ DVFSRC_HRT_REQ_MD_BW_10,   0x00034800 },
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
		{ DVFSRC_HRT1_REQ_MD_BW_10,  0x00034800 },
		{ DVFSRC_95MD_SCEN_BW0_T,    0x40444440 },
		{ DVFSRC_95MD_SCEN_BW1_T,    0x22244444 },
		{ DVFSRC_95MD_SCEN_BW2_T,    0x00400444 },
		{ DVFSRC_95MD_SCEN_BW3_T,    0x60000000 },
		{ DVFSRC_95MD_SCEN_BW0,      0x20222220 },
		{ DVFSRC_95MD_SCEN_BW1,      0x00022222},
		{ DVFSRC_95MD_SCEN_BW2,      0x00200222 },
		{ DVFSRC_95MD_SCEN_BW3,      0x60000000 },
		{ DVFSRC_95MD_SCEN_BW4,      0x00000006 },
		{ DVFSRC_RSRV_5,             0x00000001 },
		{ DVFSRC_DDR_REQUEST,        0x00004321 },
		{ DVFSRC_DDR_REQUEST3,       0x00000765 },
		{ DVFSRC_DDR_ADD_REQUEST,    0x76543210 },
		{ DVFSRC_HRT_REQUEST,        0x77654321 },
		{ DVFSRC_DDR_REQUEST5,	     0x54321000 },
		{ DVFSRC_DDR_REQUEST7,       0x76000000 },
		{ DVFSRC_EMI_MON_DEBOUNCE_TIME,   0x4C2D0000 },
		{ DVFSRC_DDR_REQUEST6,       0x76543210 },
		{ DVFSRC_VCORE_USER_REQ,     0x00010A29 },
		{ DVFSRC_HRT_HIGH_3,	     0x20DC20DC },
		{ DVFSRC_HRT_HIGH_2,	     0x1B3D1328 },
		{ DVFSRC_HRT_HIGH_1,	     0x0D690B80 },
		{ DVFSRC_HRT_HIGH,	     0x070804B0 },
		{ DVFSRC_HRT_LOW_3,	     0x20DB20DB },
		{ DVFSRC_HRT_LOW_2,	     0x1B3C132A },
		{ DVFSRC_HRT_LOW_1,	     0x0D680B7F },
		{ DVFSRC_HRT_LOW,	     0x070704AF },
		{ DVFSRC_BASIC_CONTROL_3,    0x00000006 },
		{ DVFSRC_INT_EN,             0x00000002 },
		{ DVFSRC_QOS_EN,             0x0000407C },
#ifdef	CONFIG_MTK_DVFSRC_MT6877_PRETEST
		{ DVFSRC_DDR_REQUEST3,	     0x00000065 },
		{ DVFSRC_DDR_QOS4,	     0x0000004C },
		{ DVFSRC_DDR_QOS5,	     0x00000066 },
		{ DVFSRC_HRT_HIGH_2,	     0x18A61183 },
		{ DVFSRC_HRT_LOW_2,	     0x18A51182 },
#endif
		{ DVFSRC_CURRENT_FORCE,      0x00000001 },
		{ DVFSRC_BASIC_CONTROL,      0x67B8444B },
		{ DVFSRC_BASIC_CONTROL,      0x67B8054B },
		{ DVFSRC_CURRENT_FORCE,      0x00000000 },
		{ -1, 0 },
	},
	/* NULL */
	{
		{ -1, 0 },
	},
};
#else
static struct reg_config dvfsrc_init_configs[][128] = {
	{
		{ DVFSRC_HRT_REQ_UNIT,       0x0000001E },
		{ DVFSRC_DEBOUNCE_TIME,      0x00001965 },
		{ DVFSRC_TIMEOUT_NEXTREQ,    0x0000003C },

		{ DVFSRC_DDR_QOS0,	     0x00000019 },
		{ DVFSRC_DDR_QOS1,	     0x00000026 },
		{ DVFSRC_DDR_QOS2,	     0x00000033 },
		{ DVFSRC_DDR_QOS3,	     0x0000003B },
		{ DVFSRC_DDR_QOS4,	     0x00000055 },
		{ DVFSRC_DDR_QOS5,	     0x00000077 },
		{ DVFSRC_DDR_QOS6,	     0x00000088 },

		{ DVFSRC_LEVEL_LABEL_0_1,	0x60647074 },
		{ DVFSRC_LEVEL_LABEL_2_3,	0x50546063 },
		{ DVFSRC_LEVEL_LABEL_4_5,	0x50525053 },
		{ DVFSRC_LEVEL_LABEL_6_7,	0x40434044 },
		{ DVFSRC_LEVEL_LABEL_8_9,	0x40414042 },
		{ DVFSRC_LEVEL_LABEL_10_11,	0x30333034 },
		{ DVFSRC_LEVEL_LABEL_12_13,	0x30313032 },
		{ DVFSRC_LEVEL_LABEL_14_15,	0x20243030 },
		{ DVFSRC_LEVEL_LABEL_16_17,	0x20222023 },
		{ DVFSRC_LEVEL_LABEL_18_19,	0x20202021 },
		{ DVFSRC_LEVEL_LABEL_20_21,	0x10131014 },
		{ DVFSRC_LEVEL_LABEL_22_23,	0x10111012 },
		{ DVFSRC_LEVEL_LABEL_24_25,	0x00041010 },
		{ DVFSRC_LEVEL_LABEL_26_27,	0x00020003 },
		{ DVFSRC_LEVEL_LABEL_28_29,	0x00000001 },

		{ DVFSRC_MD_LATENCY_IMPROVE, 0x00000040 },
		{ DVFSRC_HRT_BW_BASE,        0x00000004 },
		{ DVSFRC_HRT_REQ_MD_URG,     0x001A51A5 },
		{ DVFSRC_HRT_REQ_MD_BW_0,    0x00300C03 },
		{ DVFSRC_HRT_REQ_MD_BW_1,    0x00300C03 },
		{ DVFSRC_HRT_REQ_MD_BW_2,    0x00300C00 },
		{ DVFSRC_HRT_REQ_MD_BW_3,    0x00601003 },
		{ DVFSRC_HRT_REQ_MD_BW_4,    0x00902007 },
		{ DVFSRC_HRT_REQ_MD_BW_5,    0x0160440F },
		{ DVFSRC_HRT_REQ_MD_BW_6,    0x0000001E },
		{ DVFSRC_HRT_REQ_MD_BW_7,    0x0000003A },
		{ DVFSRC_HRT_REQ_MD_BW_8,    0x00000000 },
		{ DVFSRC_HRT_REQ_MD_BW_9,    0x00000000 },
		{ DVFSRC_HRT_REQ_MD_BW_10,   0x00069400 },
		{ DVFSRC_HRT1_REQ_MD_BW_0,   0x08020080 },
		{ DVFSRC_HRT1_REQ_MD_BW_1,   0x08020080 },
		{ DVFSRC_HRT1_REQ_MD_BW_2,   0x08020000 },
		{ DVFSRC_HRT1_REQ_MD_BW_3,   0x08020080 },
		{ DVFSRC_HRT1_REQ_MD_BW_4,   0x08020080 },
		{ DVFSRC_HRT1_REQ_MD_BW_5,   0x08020080 },
		{ DVFSRC_HRT1_REQ_MD_BW_6,   0x00000080 },
		{ DVFSRC_HRT1_REQ_MD_BW_7,   0x00000098 },
		{ DVFSRC_HRT1_REQ_MD_BW_8,   0x00000000 },
		{ DVFSRC_HRT1_REQ_MD_BW_9,   0x00000000 },
		{ DVFSRC_HRT1_REQ_MD_BW_10,  0x00069400 },
		{ DVFSRC_95MD_SCEN_BW0_T,    0x20222220 },
		{ DVFSRC_95MD_SCEN_BW1_T,    0x22222222 },
		{ DVFSRC_95MD_SCEN_BW2_T,    0x04300444 },
		{ DVFSRC_95MD_SCEN_BW3_T,    0x60000000 },
		{ DVFSRC_95MD_SCEN_BW0,      0x00000000 },
		{ DVFSRC_95MD_SCEN_BW1,      0x00000000 },
		{ DVFSRC_95MD_SCEN_BW2,      0x02200222 },
		{ DVFSRC_95MD_SCEN_BW3,      0x60000000 },
		{ DVFSRC_95MD_SCEN_BW4,      0x00000006 },
		{ DVFSRC_RSRV_5,             0x00000001 },
		{ DVFSRC_DDR_REQUEST,        0x00004321 },
		{ DVFSRC_DDR_REQUEST3,       0x00000765 },
		{ DVFSRC_DDR_ADD_REQUEST,    0x76543210 },
		{ DVFSRC_HRT_REQUEST,        0x77654321 },
		{ DVFSRC_DDR_REQUEST5,	     0x54321000 },
		{ DVFSRC_DDR_REQUEST7,       0x76000000 },
		{ DVFSRC_EMI_MON_DEBOUNCE_TIME,   0x4C2D0000 },
		{ DVFSRC_DDR_REQUEST6,       0x76543210 },
		{ DVFSRC_VCORE_USER_REQ,     0x00010A29 },
		{ DVFSRC_HRT_HIGH_3,	     0x31533153 },
		{ DVFSRC_HRT_HIGH_2,	     0x28D41900 },
		{ DVFSRC_HRT_HIGH_1,	     0x117E0F00 },
		{ DVFSRC_HRT_HIGH,	     0x0B400780 },
		{ DVFSRC_HRT_LOW_3,	     0x31523152 },
		{ DVFSRC_HRT_LOW_2,	     0x28D318FF },
		{ DVFSRC_HRT_LOW_1,	     0x117D0EFF },
		{ DVFSRC_HRT_LOW,	     0x0B3F077F },
		{ DVFSRC_BASIC_CONTROL_3,    0x00000006 },
		{ DVFSRC_INT_EN,             0x00000002 },
		{ DVFSRC_QOS_EN,             0x0000407C },
		{ DVFSRC_CURRENT_FORCE,      0x00000001 },
		{ DVFSRC_BASIC_CONTROL,      0x67B8444B },
		{ DVFSRC_BASIC_CONTROL,      0x67B8054B },
		{ DVFSRC_CURRENT_FORCE,      0x00000000 },
		{ -1, 0 },
	},
	/* NULL */
	{
		{ -1, 0 },
	},
};
#endif

static ssize_t dvfsrc_vcore_settle_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* DE's comment: settle time was hard code in fw (15,30) */
	return sprintf(buf, "rising 15 uS, falling 30 uS for mt6877\n");
}
static DEVICE_ATTR(dvfsrc_vcore_settle_time, 0444,
		dvfsrc_vcore_settle_time_show, NULL);

static struct attribute *mt6877_helio_dvfsrc_attrs[] = {
	&dev_attr_dvfsrc_vcore_settle_time.attr,
	NULL,
};

static struct attribute_group mt6877_helio_dvfsrc_attr_group = {
	.name = "helio-dvfsrc",
	.attrs = mt6877_helio_dvfsrc_attrs,
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

#ifdef	CONFIG_MTK_DVFSRC_MT6877_PRETEST
	if (qos_total_bw < 0x19)
		return 0;
	else if (qos_total_bw < 0x26)
		return 1;
	else if (qos_total_bw < 0x33)
		return 2;
	else if (qos_total_bw < 0x3B)
		return 3;
	else if (qos_total_bw < 0x4C)
		return 4;
	else if (qos_total_bw < 0x66)
		return 5;
	else if (qos_total_bw < 0x88)
		return 6;
	else
		return 7;
#else
	if (qos_total_bw < 0x19)
		return 0;
	else if (qos_total_bw < 0x26)
		return 1;
	else if (qos_total_bw < 0x33)
		return 2;
	else if (qos_total_bw < 0x3B)
		return 3;
	else if (qos_total_bw < 0x55)
		return 4;
	else if (qos_total_bw < 0x77)
		return 5;
	else if (qos_total_bw < 0x88)
		return 6;
	else
		return 7;
#endif
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
#ifdef	CONFIG_MTK_DVFSRC_MT6877_PRETEST
	if (data < 0x04B0)
		return DDR_OPP_7;
	else if (data < 0x0708)
		return DDR_OPP_6;
	else if (data < 0x0B80)
		return DDR_OPP_5;
	else if (data < 0x0D69)
		return DDR_OPP_4;
	else if (data < 0x1183)
		return DDR_OPP_3;
	else if (data < 0x18A6)
		return DDR_OPP_2;
	else if (data < 0x20DC)
		return DDR_OPP_1;
	else
		return DDR_OPP_0;

#else
	int dvfsrc_rsrv;

	if ((dvfsrc_rsrv >> V_OPP_TYPE_SHIFT) & 0x3) {
		if (data < 0x780)
			return DDR_OPP_7;
		else if (data < 0xB40)
			return DDR_OPP_6;
		else if (data < 0xF00)
			return DDR_OPP_5;
		else if (data < 0x117E)
			return DDR_OPP_4;
		else if (data < 0x1900)
			return DDR_OPP_3;
		else if (data < 0x24BF)
			return DDR_OPP_2;
		else if (data < 0x2CA8)
			return DDR_OPP_1;
		else
			return DDR_OPP_0;
	} else {
		if (data < 0x780)
			return DDR_OPP_7;
		else if (data < 0xB40)
			return DDR_OPP_6;
		else if (data < 0xF00)
			return DDR_OPP_5;
		else if (data < 0x117E)
			return DDR_OPP_4;
		else if (data < 0x1900)
			return DDR_OPP_3;
		else if (data < 0x28D4)
			return DDR_OPP_2;
		else if (data < 0x3153)
			return DDR_OPP_1;
		else
			return DDR_OPP_0;
	}
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

u32 dvfsrc_get_pcie_vcore_status(void)
{
	u32 val, pcie_en;

	val = dvfsrc_read(DVFSRC_DEBUG_STA_2);

	pcie_en = (val >> DEBUG_STA2_PCIE_SHIFT) & DEBUG_STA2_PCIE_MASK;

	if (pcie_en)
		return 1;
	else
		return 0;
}

struct regulator *dvfsrc_vcore_requlator(struct device *dev)
{
#ifdef	CONFIG_MTK_DVFSRC_MT6877_PRETEST
	return regulator_get(dev, "vcore");
#else
	return regulator_get(dev, "vgpu11");
#endif
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
	int force = pm_qos_request(PM_QOS_VCORE_DVFS_FORCE_OPP);

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
	int dvfsrc_rsrv;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	dvfsrc->spm_regs = devm_ioremap(&pdev->dev,
				res->start, resource_size(res));
	if (IS_ERR(dvfsrc->spm_regs))
		pr_info("not get spm register\n");

	dvfsrc_rsrv = readl(dvfsrc->regs + DVFSRC_RSRV_4);
#if 0
	if (((dvfsrc_rsrv >> V_OPP_TYPE_SHIFT) & 0x3) && (dvfsrc->dvfsrc_flag == 0)) {
		dvfsrc->dvfsrc_flag = 0x3;
		writel(0x7000, dvfsrc->regs + DVFSRC_SW_REQ6);
	}
#endif
}

__weak void pm_qos_trace_dbg_dump(int pm_qos_class)
{
}

void dvfsrc_suspend_cb(struct helio_dvfsrc *dvfsrc)
{
	int sw_req;

	sw_req = dvfsrc_read(DVFSRC_SW_REQ3);
	pr_info("[DVFSRC] V:%d, F_OPP:%d, RG:%08x, %08x, %08x, %08x\n",
		get_cur_vcore_uv(),
		pm_qos_request(PM_QOS_VCORE_DVFS_FORCE_OPP),
		dvfsrc_read(DVFSRC_CURRENT_LEVEL),
		dvfsrc_read(DVFSRC_SW_REQ2),
		sw_req,
		dvfsrc_read(DVFSRC_DEBUG_STA_0));

	if (sw_req & (DDR_SW_AP_MASK << DDR_SW_AP_SHIFT))
		pm_qos_trace_dbg_dump(PM_QOS_DDR_OPP);

	if (sw_req & (VCORE_SW_AP_MASK << VCORE_SW_AP_SHIFT))
		pm_qos_trace_dbg_dump(PM_QOS_VCORE_OPP);
}

void dvfsrc_resume_cb(struct helio_dvfsrc *dvfsrc)
{

}

void helio_dvfsrc_platform_init(struct helio_dvfsrc *dvfsrc)
{
	int spmfw_idx = 0;
	struct reg_config *config;
	int idx = 0;
	int dvfsrc_rsrv;

	sysfs_merge_group(&dvfsrc->dev->kobj, &mt6877_helio_dvfsrc_attr_group);
	dvfsrc_rsrv = readl(dvfsrc->regs + DVFSRC_RSRV_4);
	if ((dvfsrc_rsrv >> V_OPP_TYPE_SHIFT) & 0x3)
		writel(0x7000, dvfsrc->regs + DVFSRC_SW_REQ6);

	config = dvfsrc_init_configs[spmfw_idx];
	while (config[idx].offset != -1) {
		dvfsrc_write(config[idx].offset, config[idx].val);
		idx++;
	}

	if ((dvfsrc_rsrv >> V_OPP_TYPE_SHIFT) & 0x3) {
		writel(0x2CA82CA8, dvfsrc->regs + DVFSRC_HRT_HIGH_3);
		writel(0x24BF1900, dvfsrc->regs + DVFSRC_HRT_HIGH_2);
		writel(0x2CA72CA7, dvfsrc->regs + DVFSRC_HRT_LOW_3);
		writel(0x24BE18FF, dvfsrc->regs + DVFSRC_HRT_LOW_2);
		writel(0x0000, dvfsrc->regs + DVFSRC_SW_REQ6);
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
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"SPM_DVFS_CMD0~4",
			spm_reg_read(SPM_DVFS_CMD0),
			spm_reg_read(SPM_DVFS_CMD1),
			spm_reg_read(SPM_DVFS_CMD2),
			spm_reg_read(SPM_DVFS_CMD3),
			spm_reg_read(SPM_DVFS_CMD4));
}

void get_opp_info(char *p)
{
#if defined(CONFIG_FPGA_EARLY_PORTING) || !defined(CONFIG_MTK_PMIC_COMMON) \
					|| !defined(CONFIG_MTK_PMIC_NEW_ARCH)
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
	p += sprintf(p, "%6s: 0x%08x\n", "INFO1", get_devinfo_with_index(210));
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
	"VCORE__PCIE",
	"VCORE__DPAMIF",
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

	if (vopp != 3 && md_srclk == 1)
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

	sw_req = dvfsrc_read(DVFSRC_SW_REQ7);
	met_vcorefs_src[VCORE_DPAMIF_IDX] =
		(sw_req >> VCORE_SW_AP_SHIFT) & VCORE_SW_AP_MASK;

	if (scp_en) {
		sw_req = dvfsrc_read(DVFSRC_VCORE_REQUEST);
		met_vcorefs_src[VCORE_SCP_IDX] =
			(sw_req >> VCORE_SCP_GEAR_SHIFT) & VCORE_SCP_GEAR_MASK;
	} else
		met_vcorefs_src[VCORE_SCP_IDX] = 0;

	met_vcorefs_src[VCORE_HIFI_IDX] =
		vcorefs_get_hifi_vcore_status();

	met_vcorefs_src[VCORE_PCIE_IDX] =
		dvfsrc_get_pcie_vcore_status();
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
		vcorefs_get_md_scenario()  & 0x1FFFF;

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
#endif
	return 0;
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

