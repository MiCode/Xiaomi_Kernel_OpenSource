/*
 * Copyright (C) 2016 MediaTek Inc.
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

/*! \file
 * \brief  Declaration of library functions
 *
 * Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG "[WMT-CONSYS-HW]"

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "osal_typedef.h"
#include "mtk_wcn_consys_hw.h"
#include "wmt_step.h"
#include "wmt_ic.h"
#include <linux/of_reserved_mem.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <linux/syscore_ops.h>
#include <connectivity_build_in_adapter.h>
#include "wmt_lib.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
#include <linux/regulator/consumer.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#define ALLOCATE_CONNSYS_EMI_FROM_KO 1
#endif

#include <linux/thermal.h>

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static INT32 mtk_wmt_probe(struct platform_device *pdev);
static INT32 mtk_wmt_remove(struct platform_device *pdev);
static INT32 mtk_wmt_suspend(VOID);
static void mtk_wmt_resume(VOID);

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
UINT8 __iomem *pEmibaseaddr;

P_WMT_CONSYS_IC_OPS wmt_consys_ic_ops;

struct platform_device *g_pdev;

#ifdef ALLOCATE_CONNSYS_EMI_FROM_KO
phys_addr_t gConEmiPhyBase;
EXPORT_SYMBOL(gConEmiPhyBase);
unsigned long long gConEmiSize;
EXPORT_SYMBOL(gConEmiSize);
#endif

UINT32 gps_lna_pin_num = 0xffffffff;

INT32 chip_reset_status = -1;
static INT32 wifi_ant_swap_gpio_pin_num;

static UINT32 g_adie_chipid;
static OSAL_SLEEPABLE_LOCK g_adie_chipid_lock;

static atomic64_t g_sleep_counter_enable = ATOMIC64_INIT(1);
static OSAL_UNSLEEPABLE_LOCK g_sleep_counter_spinlock;

static atomic_t g_probe_called = ATOMIC_INIT(0);

#ifdef CONFIG_OF
const struct of_device_id apwmt_of_ids[] = {
	{.compatible = "mediatek,mt3967-consys",},
	{.compatible = "mediatek,mt6570-consys",},
	{.compatible = "mediatek,mt6580-consys",},
	{.compatible = "mediatek,mt6735-consys",},
	{.compatible = "mediatek,mt6739-consys",},
	{.compatible = "mediatek,mt6755-consys",},
	{.compatible = "mediatek,mt6757-consys",},
	{.compatible = "mediatek,mt6758-consys",},
	{.compatible = "mediatek,mt6759-consys",},
	{.compatible = "mediatek,mt6763-consys",},
	{.compatible = "mediatek,mt6797-consys",},
	{.compatible = "mediatek,mt8127-consys",},
	{.compatible = "mediatek,mt8163-consys",},
	{.compatible = "mediatek,mt8167-consys",},
	{.compatible = "mediatek,mt6775-consys",},
	{.compatible = "mediatek,mt6771-consys",},
	{.compatible = "mediatek,mt6765-consys",},
	{.compatible = "mediatek,mt6761-consys",},
	{.compatible = "mediatek,mt6779-consys",},
	{.compatible = "mediatek,mt6768-consys",},
	{.compatible = "mediatek,mt6785-consys",},
	{.compatible = "mediatek,mt6833-consys",},
	{.compatible = "mediatek,mt6853-consys",},
	{.compatible = "mediatek,mt6873-consys",},
	{.compatible = "mediatek,mt8168-consys",},
	{}
};
struct CONSYS_BASE_ADDRESS conn_reg;
#endif

static struct platform_driver mtk_wmt_dev_drv = {
	.probe = mtk_wmt_probe,
	.remove = mtk_wmt_remove,
	.driver = {
		   .name = "mtk_wmt",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = apwmt_of_ids,
#endif
		   },
};

static struct syscore_ops wmt_dbg_syscore_ops = {
	.suspend = mtk_wmt_suspend,
	.resume = mtk_wmt_resume,
};

/* GPIO part */
struct pinctrl *consys_pinctrl;

struct work_struct plt_resume_worker;
static void plat_resume_handler(struct work_struct *work);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
struct regmap *g_regmap;
#endif

static int wmt_thermal_get_temp_cb(void *data, int *temp);
static const struct thermal_zone_of_device_ops tz_wmt_thermal_ops = {
	.get_temp = wmt_thermal_get_temp_cb,
};

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
INT32 __weak mtk_wcn_consys_jtag_set_for_mcu(VOID)
{
	WMT_PLAT_PR_WARN("Does not support on combo\n");
	return 0;
}

#if CONSYS_ENALBE_SET_JTAG
UINT32 __weak mtk_wcn_consys_jtag_flag_ctrl(UINT32 en)
{
	WMT_PLAT_PR_WARN("Does not support on combo\n");
	return 0;
}
#endif

#ifdef CONSYS_WMT_REG_SUSPEND_CB_ENABLE
UINT32 __weak mtk_wcn_consys_hw_osc_en_ctrl(UINT32 en)
{
	WMT_PLAT_PR_WARN("Does not support on combo\n");
	return 0;
}
#endif

P_WMT_CONSYS_IC_OPS __weak mtk_wcn_get_consys_ic_ops(VOID)
{
	WMT_PLAT_PR_WARN("Does not support on combo\n");
	return NULL;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
static VOID mtk_wcn_get_regmap(struct platform_device *pdev)
{
	struct device_node *pmic_node;
	struct platform_device *pmic_pdev;
	struct mt6397_chip *chip;

	pmic_node = of_parse_phandle(pdev->dev.of_node, "pmic", 0);
	if (!pmic_node) {
		WMT_PLAT_PR_INFO("get pmic_node fail\n");
		return;
	}

	pmic_pdev = of_find_device_by_node(pmic_node);
	if (!pmic_pdev) {
		WMT_PLAT_PR_INFO("get pmic_pdev fail\n");
		return;
	}

	chip = dev_get_drvdata(&(pmic_pdev->dev));
	if (!chip) {
		WMT_PLAT_PR_INFO("get chip fail\n");
		return;
	}

	g_regmap = chip->regmap;
	if (IS_ERR_VALUE(g_regmap)) {
		g_regmap = NULL;
		WMT_PLAT_PR_INFO("get regmap fail\n");
	}
}
#endif

static INT32 wmt_allocate_connsys_emi(struct platform_device *pdev)
{
#ifdef ALLOCATE_CONNSYS_EMI_FROM_KO
	struct device_node *np;
	struct reserved_mem *rmem;

	np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!np) {
		WMT_PLAT_PR_INFO("no memory-region, np is NULL\n");
		return -1;
	}

	rmem = of_reserved_mem_lookup(np);
	of_node_put(np);

	if (!rmem) {
		WMT_PLAT_PR_INFO("no memory-region\n");
		return -1;
	}

	gConEmiPhyBase = rmem->base;
	gConEmiSize = rmem->size;
#endif
	return 0;
}

static int wmt_thermal_get_temp_cb(void *data, int *temp)
{
	if (temp) {
		*temp = wmt_lib_tm_temp_query() * 1000;
		WMT_PLAT_PR_INFO("thermal = %d\n", *temp);
	}
	return 0;
}

static INT32 wmt_thermal_register(struct platform_device *pdev)
{
	struct thermal_zone_device *tz;
	int ret;

	/* register thermal zone */
	tz = devm_thermal_zone_of_sensor_register(
		&pdev->dev, 0, NULL, &tz_wmt_thermal_ops);

	if (IS_ERR(tz)) {
		ret = PTR_ERR(tz);
		WMT_PLAT_PR_INFO("Failed to register thermal zone device %d\n", ret);
		return -1;
	}
	WMT_PLAT_PR_INFO("Register thermal zone device.\n");
	return 0;
}

static INT32 mtk_wmt_probe(struct platform_device *pdev)
{
	INT32 iRet = -1;
	INT32 pin_ret = 0;
	UINT32 pinmux = 0;
	struct device_node *pinctl_node = NULL, *pins_node = NULL;
	UINT8 __iomem *pConnsysEmiStart = NULL;

	if (pdev)
		g_pdev = pdev;
	else {
		WMT_PLAT_PR_ERR("pdev is NULL\n");
		return -1;
	}

	wmt_allocate_connsys_emi(pdev);
	wmt_thermal_register(pdev);

	if (wmt_consys_ic_ops->consys_ic_need_store_pdev) {
		if (wmt_consys_ic_ops->consys_ic_need_store_pdev() == MTK_WCN_BOOL_TRUE) {
			if (wmt_consys_ic_ops->consys_ic_store_pdev)
				wmt_consys_ic_ops->consys_ic_store_pdev(pdev);
			pm_runtime_enable(&pdev->dev);
		}
	}

	if (wmt_consys_ic_ops->consys_ic_read_reg_from_dts)
		iRet = wmt_consys_ic_ops->consys_ic_read_reg_from_dts(pdev);
	else
		iRet = -1;

	if (iRet)
		return iRet;

	if (wmt_consys_ic_ops->consys_ic_clk_get_from_dts)
		iRet = wmt_consys_ic_ops->consys_ic_clk_get_from_dts(pdev);
	else
		iRet = -1;

	if (iRet)
		return iRet;

	if (gConEmiPhyBase) {
		pConnsysEmiStart = ioremap_nocache(gConEmiPhyBase, gConEmiSize);
		WMT_PLAT_PR_INFO("Clearing Connsys EMI (virtual(0x%p) physical(0x%pa)) %llu bytes\n",
				   pConnsysEmiStart, &gConEmiPhyBase, gConEmiSize);
		memset_io(pConnsysEmiStart, 0, gConEmiSize);
		iounmap(pConnsysEmiStart);
		pConnsysEmiStart = NULL;

		if (wmt_consys_ic_ops->consys_ic_emi_mpu_set_region_protection)
			wmt_consys_ic_ops->consys_ic_emi_mpu_set_region_protection();
		if (wmt_consys_ic_ops->consys_ic_emi_set_remapping_reg)
			wmt_consys_ic_ops->consys_ic_emi_set_remapping_reg();
		if (wmt_consys_ic_ops->consys_ic_emi_coredump_remapping)
			wmt_consys_ic_ops->consys_ic_emi_coredump_remapping(&pEmibaseaddr, 1);
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
		if (wmt_consys_ic_ops->consys_ic_dedicated_log_path_init)
			wmt_consys_ic_ops->consys_ic_dedicated_log_path_init(pdev);
#endif
	} else {
		WMT_PLAT_PR_ERR("consys emi memory address gConEmiPhyBase invalid\n");
	}

#ifdef CONFIG_MTK_HIBERNATION
	WMT_PLAT_PR_INFO("register connsys restore cb for complying with IPOH function\n");
	register_swsusp_restore_noirq_func(ID_M_CONNSYS, mtk_wcn_consys_hw_restore, NULL);
#endif

	if (wmt_consys_ic_ops->ic_bt_wifi_share_v33_spin_lock_init)
		wmt_consys_ic_ops->ic_bt_wifi_share_v33_spin_lock_init();

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
	mtk_wcn_get_regmap(pdev);
#endif
	if (wmt_consys_ic_ops->consys_ic_pmic_get_from_dts)
		wmt_consys_ic_ops->consys_ic_pmic_get_from_dts(pdev);

	consys_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(consys_pinctrl)) {
		WMT_PLAT_PR_ERR("cannot find consys pinctrl.\n");
		consys_pinctrl = NULL;
	}

	/* find gps lna gpio number */
	if (consys_pinctrl) {
		pinctl_node = of_parse_phandle(pdev->dev.of_node, "pinctrl-1", 0);
		if (pinctl_node) {
			pins_node = of_get_child_by_name(pinctl_node, "pins_cmd_dat");
			if (pins_node) {
				pin_ret = of_property_read_u32(pins_node, "pinmux", &pinmux);
				if (pin_ret)
					pin_ret = of_property_read_u32(pins_node, "pins", &pinmux);
				gps_lna_pin_num = (pinmux >> 8) & 0xff;
				WMT_PLAT_PR_INFO("GPS LNA gpio pin number:%d, pinmux:0x%08x.\n",
						   gps_lna_pin_num, pinmux);
			}
		}
	}

	wifi_ant_swap_gpio_pin_num = of_get_named_gpio(pdev->dev.of_node, "wifi_ant_swap_gpio", 0);
	WMT_PLAT_PR_INFO("ant swap pin number:%d\n", wifi_ant_swap_gpio_pin_num);

	if (wmt_consys_ic_ops->consys_ic_store_reset_control)
		wmt_consys_ic_ops->consys_ic_store_reset_control(pdev);

	if (wmt_consys_ic_ops->consys_ic_register_devapc_cb)
		wmt_consys_ic_ops->consys_ic_register_devapc_cb();

	osal_unsleepable_lock_init(&g_sleep_counter_spinlock);
	osal_sleepable_lock_init(&g_adie_chipid_lock);

	INIT_WORK(&plt_resume_worker, plat_resume_handler);

	atomic_set(&g_probe_called, 1);

	return 0;
}

static INT32 mtk_wmt_remove(struct platform_device *pdev)
{
	if (wmt_consys_ic_ops->consys_ic_need_store_pdev) {
		if (wmt_consys_ic_ops->consys_ic_need_store_pdev() == MTK_WCN_BOOL_TRUE)
			pm_runtime_disable(&pdev->dev);
	}

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	if (wmt_consys_ic_ops->consys_ic_dedicated_log_path_deinit)
		wmt_consys_ic_ops->consys_ic_dedicated_log_path_deinit();
#endif
	if (wmt_consys_ic_ops->consys_ic_emi_coredump_remapping)
		wmt_consys_ic_ops->consys_ic_emi_coredump_remapping(&pEmibaseaddr, 0);

	if (g_pdev)
		g_pdev = NULL;

	osal_unsleepable_lock_deinit(&g_sleep_counter_spinlock);
	osal_sleepable_lock_deinit(&g_adie_chipid_lock);

	atomic_set(&g_probe_called, 0);
	return 0;
}

static INT32 mtk_wmt_suspend(VOID)
{
	WMT_PLAT_PR_INFO(" mtk_wmt_suspend !!");
	WMT_STEP_DO_ACTIONS_FUNC(STEP_TRIGGER_POINT_WHEN_AP_SUSPEND);

	mtk_wcn_consys_sleep_info_clear();
	return 0;
}


static void plat_resume_handler(struct work_struct *work)
{
	CONSYS_STATE_DMP_INFO dmp_info;
	UINT8 dmp_info_buf[DBG_LOG_STR_SIZE];
	int len = 0, i, dmp_cnt = 5;
	P_DEV_WMT pDev = &gDevWmt;

	if (wmt_lib_dmp_consys_state(&dmp_info, dmp_cnt, 0) == MTK_WCN_BOOL_TRUE) {
		len += snprintf(dmp_info_buf + len,
						DBG_LOG_STR_SIZE - len, "0x%08x", dmp_info.cpu_pcr[0]);

		for (i = 1; i < dmp_cnt; i++)
			len += snprintf(dmp_info_buf + len,
					DBG_LOG_STR_SIZE - len, ";0x%08x", dmp_info.cpu_pcr[i]);

		len += snprintf(dmp_info_buf + len,
				DBG_LOG_STR_SIZE - len, ";0x%08x", dmp_info.state.lp[1]);

		len += snprintf(dmp_info_buf + len,
				DBG_LOG_STR_SIZE - len, ";0x%08x", dmp_info.state.gating[1]);

		len += snprintf(dmp_info_buf + len,
				DBG_LOG_STR_SIZE - len, ";%llu", dmp_info.state.sleep_counter[0]);
		for (i = 1; i < WMT_SLEEP_COUNT_MAX; i++) {
			len += snprintf(dmp_info_buf + len,
					DBG_LOG_STR_SIZE - len, ",%llu", dmp_info.state.sleep_counter[i]);
		}

		len += snprintf(dmp_info_buf + len,
				DBG_LOG_STR_SIZE - len, ";%llu", dmp_info.state.sleep_timer[0]);
		for (i = 1; i < WMT_SLEEP_COUNT_MAX; i++) {
			len += snprintf(dmp_info_buf + len,
					DBG_LOG_STR_SIZE - len, ",%llu", dmp_info.state.sleep_timer[i]);
		}

		WMT_PLAT_PR_INFO("%s\n", dmp_info_buf);
	} else {
		if ((wmt_lib_get_drv_status(WMTDRV_TYPE_WMT) != DRV_STS_FUNC_ON)
				|| (osal_test_bit(WMT_STAT_PWR, &pDev->state)) == 0) {
			WMT_PLAT_PR_INFO("TOP:0,0;MCU:0,0;BT:0,0;WIFI:0,0;GPS:0,0\n");
		}
	}
}

static void mtk_wmt_resume(VOID)
{
	WMT_PLAT_PR_INFO(" mtk_wmt_resume !!");
	schedule_work(&plt_resume_worker);
}

INT32 mtk_wcn_consys_sleep_info_read_all_ctrl(P_CONSYS_STATE state)
{
	INT32 ret = 0;
	INT32 i = 0;
	UINT64 sleep_counter = 0, sleep_timer = 0;
	UINT8 strbuf[DBG_LOG_STR_SIZE] = {""};
	UINT32 len = 0;

	WMT_PLAT_PR_DBG("sleep count info read all ctrl start\n");

	if (wmt_consys_ic_ops->consys_ic_sleep_info_enable_ctrl &&
			wmt_consys_ic_ops->consys_ic_sleep_info_read_ctrl &&
			wmt_consys_ic_ops->consys_ic_sleep_info_clear) {
		osal_lock_unsleepable_lock(&g_sleep_counter_spinlock);
		if (atomic64_read(&g_sleep_counter_enable) == 1) {
			for (i = 0; i < WMT_SLEEP_COUNT_MAX; i++) {
				sleep_counter = 0;
				sleep_timer = 0;
				wmt_consys_ic_ops->consys_ic_sleep_info_read_ctrl(i, &sleep_counter, &sleep_timer);
				if (state) {
					state->sleep_counter[i] = sleep_counter;
					state->sleep_timer[i] = sleep_timer;
				}
				len += osal_sprintf(strbuf + len, "%s%s%s%s%s:%llu,%llu;",
					((i == WMT_SLEEP_COUNT_TOP) ? "TOP" : ""),
					((i == WMT_SLEEP_COUNT_MCU) ? "MCU" : ""),
					((i == WMT_SLEEP_COUNT_BT) ? "BT" : ""),
					((i == WMT_SLEEP_COUNT_WF) ? "WIFI" : ""),
					((i == WMT_SLEEP_COUNT_GPS) ? "GPS" : ""),
					sleep_counter, sleep_timer);
			}
			len += osal_sprintf(strbuf + len - 1, "");
			WMT_PLAT_PR_INFO("%s\n", strbuf);
		} else {
			ret = -21;
		}
		osal_unlock_unsleepable_lock(&g_sleep_counter_spinlock);
	}

	return ret;
}

INT32 mtk_wcn_consys_sleep_info_clear(VOID)
{
	INT32 ret = 0;
	P_DEV_WMT pDev = &gDevWmt;

	WMT_PLAT_PR_DBG("sleep count info clear start\n");

	if (wmt_consys_ic_ops->consys_ic_sleep_info_enable_ctrl &&
			wmt_consys_ic_ops->consys_ic_sleep_info_read_ctrl &&
			wmt_consys_ic_ops->consys_ic_sleep_info_clear) {

		osal_lock_unsleepable_lock(&g_sleep_counter_spinlock);

		if ((wmt_lib_get_drv_status(WMTDRV_TYPE_WMT) == DRV_STS_FUNC_ON)
				&& (osal_test_bit(WMT_STAT_PWR, &pDev->state)))
			ret = wmt_consys_ic_ops->consys_ic_sleep_info_clear();
		osal_unlock_unsleepable_lock(&g_sleep_counter_spinlock);
	}

	return ret;
}

INT32 mtk_wcn_consys_sleep_info_restore(VOID)
{
	P_DEV_WMT pDev = &gDevWmt;

	WMT_PLAT_PR_DBG("sleep count info restore start\n");

	if (NULL != wmt_consys_ic_ops &&
			wmt_consys_ic_ops->consys_ic_sleep_info_enable_ctrl &&
			wmt_consys_ic_ops->consys_ic_sleep_info_read_ctrl &&
			wmt_consys_ic_ops->consys_ic_sleep_info_clear) {
		if ((wmt_lib_get_drv_status(WMTDRV_TYPE_WMT) == DRV_STS_FUNC_ON)
				&& (osal_test_bit(WMT_STAT_PWR, &pDev->state))) {
			osal_lock_unsleepable_lock(&g_sleep_counter_spinlock);
			if (atomic64_read(&g_sleep_counter_enable) == 1)
				wmt_consys_ic_ops->consys_ic_sleep_info_enable_ctrl(1);
			osal_unlock_unsleepable_lock(&g_sleep_counter_spinlock);
		}
	}

	return 0;
}

INT32 mtk_wcn_consys_hw_reg_ctrl(UINT32 on, UINT32 co_clock_type)
{
	INT32 iRet = 0;

	WMT_PLAT_PR_INFO("CONSYS-HW-REG-CTRL(0x%08x),start\n", on);

	if (on) {
		WMT_PLAT_PR_DBG("++\n");
		if (wmt_consys_ic_ops->consys_ic_reset_emi_coredump)
			wmt_consys_ic_ops->consys_ic_reset_emi_coredump(pEmibaseaddr);

		if (wmt_consys_ic_ops->consys_ic_hw_vcn18_ctrl)
			wmt_consys_ic_ops->consys_ic_hw_vcn18_ctrl(ENABLE);

		if (wmt_consys_ic_ops->consys_ic_set_if_pinmux)
			wmt_consys_ic_ops->consys_ic_set_if_pinmux(ENABLE);

		udelay(150);

		if (co_clock_type) {
			WMT_PLAT_PR_INFO("co clock type(%d),turn on clk buf\n", co_clock_type);
			if (wmt_consys_ic_ops->consys_ic_clock_buffer_ctrl)
				wmt_consys_ic_ops->consys_ic_clock_buffer_ctrl(ENABLE);
		}

		if (co_clock_type) {
			/*if co-clock mode: */
			/*1.set VCN28 to SW control mode (with PMIC_WRAP API) */
			if (wmt_consys_ic_ops->consys_ic_vcn28_hw_mode_ctrl)
				wmt_consys_ic_ops->consys_ic_vcn28_hw_mode_ctrl(DISABLE);
		} else {
			/*if NOT co-clock: */
			/*1.set VCN28 to HW control mode (with PMIC_WRAP API) */
			/*2.turn on VCN28 LDO (with PMIC_WRAP API)" */
			if (wmt_consys_ic_ops->consys_ic_vcn28_hw_mode_ctrl)
				wmt_consys_ic_ops->consys_ic_vcn28_hw_mode_ctrl(ENABLE);
			if (wmt_consys_ic_ops->consys_ic_hw_vcn28_ctrl)
				wmt_consys_ic_ops->consys_ic_hw_vcn28_ctrl(ENABLE);
		}

		/* turn on VCN28 LDO for reading efuse usage */
		mtk_wcn_consys_hw_efuse_paldo_ctrl(ENABLE, co_clock_type);

		if (wmt_consys_ic_ops->consys_ic_hw_reset_bit_set)
			wmt_consys_ic_ops->consys_ic_hw_reset_bit_set(ENABLE);
		if (wmt_consys_ic_ops->consys_ic_hw_spm_clk_gating_enable)
			wmt_consys_ic_ops->consys_ic_hw_spm_clk_gating_enable();
		if (wmt_consys_ic_ops->consys_ic_hw_power_ctrl)
			wmt_consys_ic_ops->consys_ic_hw_power_ctrl(ENABLE);

		udelay(10);

		if (wmt_consys_ic_ops->consys_ic_ahb_clock_ctrl)
			wmt_consys_ic_ops->consys_ic_ahb_clock_ctrl(ENABLE);

		WMT_STEP_DO_ACTIONS_FUNC(STEP_TRIGGER_POINT_POWER_ON_BEFORE_GET_CONNSYS_ID);

		if (wmt_consys_ic_ops->polling_consys_ic_chipid &&
			wmt_consys_ic_ops->polling_consys_ic_chipid() < 0)
			return -1;

		if (wmt_consys_ic_ops->consys_ic_hw_vcn_ctrl_after_idle)
			wmt_consys_ic_ops->consys_ic_hw_vcn_ctrl_after_idle();
		if (wmt_consys_ic_ops->consys_ic_set_access_emi_hw_mode)
			wmt_consys_ic_ops->consys_ic_set_access_emi_hw_mode();
		if (wmt_consys_ic_ops->update_consys_rom_desel_value)
			wmt_consys_ic_ops->update_consys_rom_desel_value();
		if (wmt_consys_ic_ops->consys_ic_acr_reg_setting)
			wmt_consys_ic_ops->consys_ic_acr_reg_setting();
		if (wmt_consys_ic_ops->consys_ic_afe_reg_setting)
			wmt_consys_ic_ops->consys_ic_afe_reg_setting();
		if (wmt_consys_ic_ops->consys_ic_emi_entry_address)
			wmt_consys_ic_ops->consys_ic_emi_entry_address();
		if (wmt_consys_ic_ops->consys_ic_set_xo_osc_ctrl)
			wmt_consys_ic_ops->consys_ic_set_xo_osc_ctrl();
		if (wmt_consys_ic_ops->consys_ic_identify_adie)
			wmt_consys_ic_ops->consys_ic_identify_adie();
		if (wmt_consys_ic_ops->consys_ic_wifi_ctrl_setting)
			wmt_consys_ic_ops->consys_ic_wifi_ctrl_setting();
		if (wmt_consys_ic_ops->consys_ic_bus_timeout_config)
			wmt_consys_ic_ops->consys_ic_bus_timeout_config();
		if (wmt_consys_ic_ops->consys_ic_hw_reset_bit_set)
			wmt_consys_ic_ops->consys_ic_hw_reset_bit_set(DISABLE);

		msleep(20);

	} else {
		if (wmt_consys_ic_ops->consys_ic_ahb_clock_ctrl)
			wmt_consys_ic_ops->consys_ic_ahb_clock_ctrl(DISABLE);
		if (wmt_consys_ic_ops->consys_ic_hw_power_ctrl)
			wmt_consys_ic_ops->consys_ic_hw_power_ctrl(DISABLE);
		if (co_clock_type) {
			if (wmt_consys_ic_ops->consys_ic_clock_buffer_ctrl)
				wmt_consys_ic_ops->consys_ic_clock_buffer_ctrl(DISABLE);
		}

		if (co_clock_type == 0) {
			if (wmt_consys_ic_ops->consys_ic_vcn28_hw_mode_ctrl)
				wmt_consys_ic_ops->consys_ic_vcn28_hw_mode_ctrl(DISABLE);
			/*turn off VCN28 LDO (with PMIC_WRAP API)" */
			if (wmt_consys_ic_ops->consys_ic_hw_vcn28_ctrl)
				wmt_consys_ic_ops->consys_ic_hw_vcn28_ctrl(DISABLE);
		}

		if (wmt_consys_ic_ops->consys_ic_set_if_pinmux)
			wmt_consys_ic_ops->consys_ic_set_if_pinmux(DISABLE);

		if (wmt_consys_ic_ops->consys_ic_hw_vcn18_ctrl)
			wmt_consys_ic_ops->consys_ic_hw_vcn18_ctrl(DISABLE);
	}
	WMT_PLAT_PR_INFO("CONSYS-HW-REG-CTRL(0x%08x),finish\n", on);
	return iRet;
}
/*tag4 wujun api big difference end*/

INT32 mtk_wcn_consys_hw_bt_paldo_ctrl(UINT32 enable)
{
	if (wmt_consys_ic_ops->consys_ic_hw_bt_vcn33_ctrl)
		wmt_consys_ic_ops->consys_ic_hw_bt_vcn33_ctrl(enable);
	return 0;
}

INT32 mtk_wcn_consys_hw_wifi_paldo_ctrl(UINT32 enable)
{
	if (wmt_consys_ic_ops->consys_ic_hw_wifi_vcn33_ctrl)
		wmt_consys_ic_ops->consys_ic_hw_wifi_vcn33_ctrl(enable);
	return 0;
}
EXPORT_SYMBOL(mtk_wcn_consys_hw_wifi_paldo_ctrl);

INT32 mtk_wcn_consys_hw_efuse_paldo_ctrl(UINT32 enable, UINT32 co_clock_type)
{
	if (co_clock_type) {
		if (wmt_consys_ic_ops->consys_ic_hw_vcn28_ctrl)
			wmt_consys_ic_ops->consys_ic_hw_vcn28_ctrl(enable);
		if (enable)
			WMT_PLAT_PR_INFO("turn on vcn28 for efuse usage in co-clock mode\n");
		else
			WMT_PLAT_PR_INFO("turn off vcn28 for efuse usage in co-clock mode\n");
	}
	return 0;
}
EXPORT_SYMBOL(mtk_wcn_consys_hw_efuse_paldo_ctrl);

INT32 mtk_wcn_consys_hw_vcn28_ctrl(UINT32 enable)
{
	if (wmt_consys_ic_ops->consys_ic_hw_vcn28_ctrl)
		wmt_consys_ic_ops->consys_ic_hw_vcn28_ctrl(enable);
	if (enable)
		WMT_PLAT_PR_INFO("turn on vcn28 for fm/gps usage in co-clock mode\n");
	else
		WMT_PLAT_PR_INFO("turn off vcn28 for fm/gps usage in co-clock mode\n");
	return 0;
}

UINT32 mtk_wcn_consys_soc_chipid(VOID)
{
	if (wmt_consys_ic_ops == NULL)
		wmt_consys_ic_ops = mtk_wcn_get_consys_ic_ops();

	if (wmt_consys_ic_ops && wmt_consys_ic_ops->consys_ic_soc_chipid_get)
		return wmt_consys_ic_ops->consys_ic_soc_chipid_get();
	else
		return 0;
}

UINT32 mtk_wcn_consys_get_adie_chipid(VOID)
{
	return g_adie_chipid;
}

INT32 mtk_wcn_consys_detect_adie_chipid(UINT32 co_clock_type)
{
	INT32 chipid = 0;

	if (osal_lock_sleepable_lock(&g_adie_chipid_lock))
		return 0;

	/* detect a-die only once */
	if (g_adie_chipid) {
		osal_unlock_sleepable_lock(&g_adie_chipid_lock);
		return g_adie_chipid;
	}

	if (wmt_consys_ic_ops == NULL)
		wmt_consys_ic_ops = mtk_wcn_get_consys_ic_ops();

	if (wmt_consys_ic_ops && wmt_consys_ic_ops->consys_ic_adie_chipid_detect) {
		WMT_PLAT_PR_INFO("CONSYS A-DIE DETECT start\n");
		chipid = wmt_consys_ic_ops->consys_ic_adie_chipid_detect();
		if (chipid > 0) {
			g_adie_chipid = chipid;
			WMT_PLAT_PR_INFO("Set a-die chipid = %x\n", chipid);
		} else
			WMT_PLAT_PR_INFO("Detect a-die chipid = %x failed!\n", chipid);
		WMT_PLAT_PR_INFO("CONSYS A-DIE DETECT finish\n");
	}

	osal_unlock_sleepable_lock(&g_adie_chipid_lock);

	return chipid;
}

struct pinctrl *mtk_wcn_consys_get_pinctrl()
{
	return consys_pinctrl;
}

INT32 mtk_wcn_consys_hw_gpio_ctrl(UINT32 on)
{
	INT32 iRet = 0;

	WMT_PLAT_PR_DBG("CONSYS-HW-GPIO-CTRL(0x%08x), start\n", on);

	if (on) {

		if (wmt_consys_ic_ops->consys_ic_need_gps) {
			if (wmt_consys_ic_ops->consys_ic_need_gps() == MTK_WCN_BOOL_TRUE) {
				/*if external modem used,GPS_SYNC still needed to control */
				iRet += wmt_plat_gpio_ctrl(PIN_GPS_SYNC, PIN_STA_INIT);
				iRet += wmt_plat_gpio_ctrl(PIN_GPS_LNA, PIN_STA_INIT);

				iRet += wmt_plat_gpio_ctrl(PIN_I2S_GRP, PIN_STA_INIT);
			}
		} else {
			iRet += wmt_plat_gpio_ctrl(PIN_GPS_SYNC, PIN_STA_INIT);
			iRet += wmt_plat_gpio_ctrl(PIN_GPS_LNA, PIN_STA_INIT);

			iRet += wmt_plat_gpio_ctrl(PIN_I2S_GRP, PIN_STA_INIT);
		}
		/* TODO: [FixMe][GeorgeKuo] double check if BGF_INT is implemented ok */
		/* iRet += wmt_plat_gpio_ctrl(PIN_BGF_EINT, PIN_STA_MUX); */
		iRet += wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_INIT);
		iRet += wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_EINT_DIS);
		WMT_PLAT_PR_DBG("CONSYS-HW, BGF IRQ registered and disabled\n");

	} else {

		/* set bgf eint/all eint to deinit state, namely input low state */
		iRet += wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_EINT_DIS);
		iRet += wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_DEINIT);
		WMT_PLAT_PR_DBG("CONSYS-HW, BGF IRQ unregistered and disabled\n");
		/* iRet += wmt_plat_gpio_ctrl(PIN_BGF_EINT, PIN_STA_DEINIT); */
		if (wmt_consys_ic_ops->consys_ic_need_gps) {
			if (wmt_consys_ic_ops->consys_ic_need_gps() == MTK_WCN_BOOL_TRUE) {
				/*if external modem used,GPS_SYNC still needed to control */
				iRet += wmt_plat_gpio_ctrl(PIN_GPS_SYNC, PIN_STA_DEINIT);
				iRet += wmt_plat_gpio_ctrl(PIN_I2S_GRP, PIN_STA_DEINIT);
				/* deinit gps_lna */
				iRet += wmt_plat_gpio_ctrl(PIN_GPS_LNA, PIN_STA_DEINIT);
			}
		} else {
			iRet += wmt_plat_gpio_ctrl(PIN_GPS_SYNC, PIN_STA_DEINIT);
			iRet += wmt_plat_gpio_ctrl(PIN_I2S_GRP, PIN_STA_DEINIT);
			iRet += wmt_plat_gpio_ctrl(PIN_GPS_LNA, PIN_STA_DEINIT);
		}
	}
	WMT_PLAT_PR_DBG("CONSYS-HW-GPIO-CTRL(0x%08x), finish\n", on);
	return iRet;

}

INT32 mtk_wcn_consys_hw_pwr_on(UINT32 co_clock_type)
{
	INT32 iRet = 0;

	WMT_PLAT_PR_INFO("CONSYS-HW-PWR-ON, start\n");
	WMT_STEP_DO_ACTIONS_FUNC(STEP_TRIGGER_POINT_POWER_ON_START);
	if (!gConEmiPhyBase) {
		WMT_PLAT_PR_ERR("EMI base address is invalid, CONNSYS can not be powered on!");
		return -1;
	}
	iRet += mtk_wcn_consys_hw_reg_ctrl(1, co_clock_type);
	iRet += mtk_wcn_consys_hw_gpio_ctrl(1);
	mtk_wcn_consys_jtag_set_for_mcu();

	WMT_PLAT_PR_INFO("CONSYS-HW-PWR-ON, finish(%d)\n", iRet);
	return iRet;
}

INT32 mtk_wcn_consys_hw_pwr_off(UINT32 co_clock_type)
{
	INT32 iRet = 0;

	WMT_PLAT_PR_INFO("CONSYS-HW-PWR-OFF, start\n");
	WMT_STEP_DO_ACTIONS_FUNC(STEP_TRIGGER_POINT_BEFORE_POWER_OFF);

	iRet += mtk_wcn_consys_hw_reg_ctrl(0, co_clock_type);
	iRet += mtk_wcn_consys_hw_gpio_ctrl(0);

	WMT_PLAT_PR_INFO("CONSYS-HW-PWR-OFF, finish(%d)\n", iRet);
	return iRet;
}

INT32 mtk_wcn_consys_hw_rst(UINT32 co_clock_type)
{
	INT32 iRet = 0;

	WMT_PLAT_PR_INFO("CONSYS-HW, hw_rst start, eirq should be disabled before this step\n");

	mtk_consys_set_chip_reset_status(1);

	if (wmt_consys_ic_ops->consys_ic_set_dl_rom_patch_flag)
		wmt_consys_ic_ops->consys_ic_set_dl_rom_patch_flag(1);

	/* Dump infra register for debug purpose */
	if (wmt_consys_ic_ops->consys_ic_infra_reg_dump)
		wmt_consys_ic_ops->consys_ic_infra_reg_dump();

	/* write 0x5000_0154.Bit[1] = 1 (pdma_axi_rready_force_high) to prevent pdma block slpprot */
	if (wmt_consys_ic_ops->consys_ic_set_pdma_axi_rready_force_high)
		wmt_consys_ic_ops->consys_ic_set_pdma_axi_rready_force_high(1);

	/*1. do whole hw power off flow */
	iRet += mtk_wcn_consys_hw_reg_ctrl(0, co_clock_type);

	/* Write Wi-Fi calibration data back to EMI */
	mtk_wcn_soc_restore_wifi_cal_result();

	/*2. do whole hw power on flow */
	iRet += mtk_wcn_consys_hw_reg_ctrl(1, co_clock_type);

	/* Make sure pdma_axi_rready_force_high set to 0 after reset */
	if (wmt_consys_ic_ops->consys_ic_set_pdma_axi_rready_force_high)
		wmt_consys_ic_ops->consys_ic_set_pdma_axi_rready_force_high(0);

	mtk_consys_set_chip_reset_status(0);

	WMT_PLAT_PR_INFO("CONSYS-HW, hw_rst finish, eirq should be enabled after this step\n");
	return iRet;
}

INT32 mtk_wcn_consys_hw_state_show(VOID)
{
	return 0;
}

INT32 mtk_wcn_consys_hw_restore(struct device *device)
{
	if (gConEmiPhyBase) {
		if (wmt_consys_ic_ops->consys_ic_emi_mpu_set_region_protection)
			wmt_consys_ic_ops->consys_ic_emi_mpu_set_region_protection();
		if (wmt_consys_ic_ops->consys_ic_emi_set_remapping_reg)
			wmt_consys_ic_ops->consys_ic_emi_set_remapping_reg();
		if (wmt_consys_ic_ops->consys_ic_emi_coredump_remapping)
			wmt_consys_ic_ops->consys_ic_emi_coredump_remapping(&pEmibaseaddr, 1);
	} else {
		WMT_PLAT_PR_ERR("consys emi memory address gConEmiPhyBase invalid\n");
	}

	return 0;
}

INT32 mtk_wcn_consys_hw_init(VOID)
{
	INT32 iRet = -1, retry = 0;

	if (wmt_consys_ic_ops == NULL)
		wmt_consys_ic_ops = mtk_wcn_get_consys_ic_ops();

	iRet = platform_driver_register(&mtk_wmt_dev_drv);
	if (iRet)
		WMT_PLAT_PR_ERR("WMT platform driver registered failed(%d)\n", iRet);
	else {
		while (atomic_read(&g_probe_called) == 0 && retry < 100) {
			osal_sleep_ms(50);
			retry++;
			WMT_PLAT_PR_INFO("g_probe_called = 0, retry = %d\n", retry);
		}
		register_syscore_ops(&wmt_dbg_syscore_ops);
	}

	return iRet;

}

INT32 mtk_wcn_consys_hw_deinit(VOID)
{

	if (pEmibaseaddr) {
		if (wmt_consys_ic_ops->consys_ic_emi_coredump_remapping)
			wmt_consys_ic_ops->consys_ic_emi_coredump_remapping(&pEmibaseaddr, 0);
	}
#ifdef CONFIG_MTK_HIBERNATION
	unregister_swsusp_restore_noirq_func(ID_M_CONNSYS);
#endif

	platform_driver_unregister(&mtk_wmt_dev_drv);
	unregister_syscore_ops(&wmt_dbg_syscore_ops);

	if (wmt_consys_ic_ops)
		wmt_consys_ic_ops = NULL;

	return 0;
}

PUINT8 mtk_wcn_consys_emi_virt_addr_get(UINT32 ctrl_state_offset)
{
	UINT8 *p_virtual_addr = NULL;

	if (!pEmibaseaddr) {
		WMT_PLAT_PR_ERR("EMI base address is NULL\n");
		return NULL;
	}
	WMT_PLAT_PR_DBG("ctrl_state_offset(%08x)\n", ctrl_state_offset);
	p_virtual_addr = pEmibaseaddr + ctrl_state_offset;

	return p_virtual_addr;
}

INT32 mtk_wcn_consys_set_dbg_mode(UINT32 flag)
{
	INT32 ret = -1;
	PUINT8 vir_addr = NULL;

	vir_addr = mtk_wcn_consys_emi_virt_addr_get(EXP_APMEM_CTRL_CHIP_FW_DBGLOG_MODE);
	if (!vir_addr) {
		WMT_PLAT_PR_ERR("get vir address fail\n");
		return -2;
	}
	if (flag) {
		ret = 0;
		CONSYS_REG_WRITE(vir_addr, 0x1);
	} else {
		CONSYS_REG_WRITE(vir_addr, 0x0);
	}
	WMT_PLAT_PR_INFO("fw dbg mode register value(0x%08x)\n", CONSYS_REG_READ(vir_addr));
	return ret;
}

INT32 mtk_wcn_consys_set_dynamic_dump(PUINT32 str_buf)
{
	PUINT8 vir_addr = NULL;

	vir_addr = mtk_wcn_consys_emi_virt_addr_get(EXP_APMEM_CTRL_CHIP_DYNAMIC_DUMP);
	if (!vir_addr) {
		WMT_PLAT_PR_ERR("get vir address fail\n");
		return -2;
	}
	memcpy(vir_addr, str_buf, DYNAMIC_DUMP_GROUP_NUM*8);
	WMT_PLAT_PR_INFO("dynamic dump register value(0x%08x)\n", CONSYS_REG_READ(vir_addr));
	return 0;
}

INT32 mtk_wcn_consys_co_clock_type(VOID)
{
	if (wmt_consys_ic_ops == NULL)
		wmt_consys_ic_ops = mtk_wcn_get_consys_ic_ops();

	if (wmt_consys_ic_ops && wmt_consys_ic_ops->consys_ic_co_clock_type)
		return wmt_consys_ic_ops->consys_ic_co_clock_type();
	else
		return -1;
}

P_CONSYS_EMI_ADDR_INFO mtk_wcn_consys_soc_get_emi_phy_add(VOID)
{
	if (wmt_consys_ic_ops->consys_ic_soc_get_emi_phy_add)
		return wmt_consys_ic_ops->consys_ic_soc_get_emi_phy_add();
	else
		return NULL;
}

UINT32 mtk_wcn_consys_read_cpupcr(VOID)
{
	if (wmt_consys_ic_ops->consys_ic_read_cpupcr)
		return wmt_consys_ic_ops->consys_ic_read_cpupcr();
	else
		return 0;
}

VOID mtk_wcn_force_trigger_assert_debug_pin(VOID)
{
	if (wmt_consys_ic_ops->ic_force_trigger_assert_debug_pin)
		wmt_consys_ic_ops->ic_force_trigger_assert_debug_pin();
}

INT32 mtk_wcn_consys_read_irq_info_from_dts(PINT32 irq_num, PUINT32 irq_flag)
{
	if (wmt_consys_ic_ops->consys_ic_read_irq_info_from_dts)
		return wmt_consys_ic_ops->consys_ic_read_irq_info_from_dts(g_pdev, irq_num, irq_flag);
	else
		return 0;
}

VOID mtk_wcn_consys_hang_debug(VOID)
{
	if (wmt_consys_ic_ops && wmt_consys_ic_ops->consys_hang_debug)
		wmt_consys_ic_ops->consys_hang_debug();
}

UINT32 mtk_consys_get_gps_lna_pin_num(VOID)
{
	return gps_lna_pin_num;
}

INT32 mtk_wcn_consys_reg_ctrl(UINT32 is_write, enum CONSYS_BASE_ADDRESS_INDEX index, UINT32 offset,
		PUINT32 value)
{
	INT32 iRet = -1;
	PUINT32 reg_info = NULL;
	UINT32 reg_info_size = index * 4 + 4;
	struct device_node *node;
	PVOID remap_addr = NULL;

	reg_info = osal_malloc(reg_info_size * sizeof(UINT32));
	if (!reg_info) {
		WMT_PLAT_PR_ERR("reg_info osal_malloc fail\n");
		goto fail;
	}

	node = g_pdev->dev.of_node;
	if (node) {
		if (of_property_read_u32_array(node, "reg", reg_info, reg_info_size)) {
			WMT_PLAT_PR_ERR("get reg from DTS fail!!\n");
			goto fail;
		}
	} else {
		WMT_PLAT_PR_ERR("[%s] can't find CONSYS compatible node\n", __func__);
		goto fail;
	}

	if (reg_info[index*4 + 3] < offset) {
		WMT_PLAT_PR_ERR("Access overflow of address(0x%x), offset(0x%x)!\n",
				reg_info[index*4 + 1], reg_info[index*4 + 3]);
		goto fail;
	}

	remap_addr = ioremap(reg_info[index*4 + 1] + offset, 0x4);
	if (remap_addr == NULL) {
		WMT_PLAT_PR_ERR("ioremap fail!\n");
		goto fail;
	}

	if (is_write)
		CONSYS_REG_WRITE(remap_addr, *value);
	else
		*value = CONSYS_REG_READ(remap_addr);

	if (remap_addr)
		iounmap(remap_addr);

	iRet = 0;

fail:
	if (reg_info)
		osal_free(reg_info);
	return iRet;
}

/* Who call this?
 *	- wmt_step
 *	- stp_dbg.c stp_dbg_poll_cpupcr
 *	- wmt_core
 */
INT32 mtk_consys_check_reg_readable(VOID)
{
	return mtk_consys_check_reg_readable_by_addr(0);
}

INT32 mtk_consys_check_reg_readable_by_addr(SIZE_T addr)
{
	INT32 is_host_csr = 0;

	if (wmt_consys_ic_ops->consys_ic_is_host_csr)
		is_host_csr = wmt_consys_ic_ops->consys_ic_is_host_csr(addr);

	if (wmt_consys_ic_ops->consys_ic_check_reg_readable && is_host_csr == 0)
		return wmt_consys_ic_ops->consys_ic_check_reg_readable();
	else
		return 1;
}

VOID mtk_wcn_consys_clock_fail_dump(VOID)
{
	if (wmt_consys_ic_ops->consys_ic_clock_fail_dump)
		wmt_consys_ic_ops->consys_ic_clock_fail_dump();
	WMT_STEP_DO_ACTIONS_FUNC(STEP_TRIGGER_POINT_WHEN_CLOCK_FAIL);
}

INT32 mtk_consys_is_connsys_reg(UINT32 addr)
{
	if (wmt_consys_ic_ops->consys_ic_is_connsys_reg)
		return wmt_consys_ic_ops->consys_ic_is_connsys_reg(addr);
	else
		return 0;
}

VOID mtk_consys_set_mcif_mpu_protection(MTK_WCN_BOOL enable)
{
	if (wmt_consys_ic_ops->consys_ic_set_mcif_emi_mpu_protection)
		wmt_consys_ic_ops->consys_ic_set_mcif_emi_mpu_protection(enable);
}

INT32 mtk_consys_is_calibration_backup_restore_support(VOID)
{
	if (wmt_consys_ic_ops->consys_ic_calibration_backup_restore)
		return wmt_consys_ic_ops->consys_ic_calibration_backup_restore();
	else
		return 0;

}

VOID mtk_consys_set_chip_reset_status(INT32 status)
{
	chip_reset_status = status;
}

INT32 mtk_consys_chip_reset_status(VOID)
{
	return chip_reset_status;
}

INT32 mtk_consys_is_ant_swap_enable_by_hwid(VOID)
{
	if (wmt_consys_ic_ops->consys_ic_is_ant_swap_enable_by_hwid)
		return wmt_consys_ic_ops->consys_ic_is_ant_swap_enable_by_hwid(wifi_ant_swap_gpio_pin_num);
	else
		return 0;
}

INT32 mtk_consys_dump_osc_state(P_CONSYS_STATE state)
{
	if (wmt_consys_ic_ops->consys_ic_dump_osc_state)
		return wmt_consys_ic_ops->consys_ic_dump_osc_state(state);

	return MTK_WCN_BOOL_FALSE;
}

VOID mtk_wcn_consys_ic_get_ant_sel_cr_addr(PUINT32 default_invert_cr, PUINT32 default_invert_bit)
{
	if (wmt_consys_ic_ops->consys_ic_get_ant_sel_cr_addr)
		wmt_consys_ic_ops->consys_ic_get_ant_sel_cr_addr(default_invert_cr, default_invert_bit);
}

INT32 mtk_wcn_consys_dump_gating_state(P_CONSYS_STATE state)
{
	if (wmt_consys_ic_ops->consys_ic_dump_gating_state)
		return wmt_consys_ic_ops->consys_ic_dump_gating_state(state);

	return MTK_WCN_BOOL_FALSE;
}

UINT64 mtk_wcn_consys_get_options(VOID)
{
	if (wmt_consys_ic_ops->consys_ic_get_options)
		return wmt_consys_ic_ops->consys_ic_get_options();

	WMT_PLAT_PR_INFO("Please implement consys_ic_get_options!");
	wmt_lib_trigger_assert(WMTDRV_TYPE_WMT, 45);
	return 0;
}
