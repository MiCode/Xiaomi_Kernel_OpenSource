/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/file.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/nvmem-consumer.h>
#include <linux/thermal.h>

#include "../platform/gnss/gnss.h"
#include "gnss_firmware_bin.h"
#include "marlin_firmware_bin.h"
#include <misc/wcn_bus.h>
#include "wcn_glb.h"
#include "wcn_glb_reg.h"
#include "wcn_log.h"
#include "wcn_misc.h"
#include "wcn_procfs.h"
#include "../include/wcn_dbg.h"
#include "wcn_txrx.h"
#include "wcn_gnss_dump.h"
#include "wcn_debug_bus.h"

#define SUFFIX "androidboot.slot_suffix="
#define SUFFIXS "sprdboot.slot_suffix="

struct wcn_device_manage s_wcn_device;

static u32 wcn_efuse_val[WCN_EFUSE_BLOCK_COUNT];
static u32 gnss_efuse_val[GNSS_EFUSE_BLOCK_COUNT];
static char *fstab_ab;
static char *slot_suffix;
module_param(slot_suffix, charp, 0444);
extern int is_wcn_shutdown;

/* efuse blk */
static const u32
s_wifi_efuse_id[WCN_PLATFORM_TYPE][WIFI_EFUSE_BLOCK_COUNT] = {
	{20, 24, 28},	/* sharkle */
	{8, 12, 16},	/* pike2 */
	{20, 24, 28},	/* sharkl3 */
	{81, 81, 81},	/* qogirl6 */
};

static const u32
s_gnss_efuse_id[WCN_PLATFORM_TYPE][GNSS_EFUSE_BLOCK_COUNT] = {
	{16, 24, 28},	/* sharkle */
	{32, 12, 16},	/* pike2 */
	{32, 24, 28},	/* sharkl3 */
	{81, 81, 81},	/* qogirl6 */
};

static void wcn_global_source_init(void)
{
	wcn_boot_init();
	WCN_DBG("%s finish!\n", __func__);
}

#if 0
#ifdef CONFIG_PM_SLEEP
static int wcn_resume(struct device *dev)
{
	WCN_INFO("%s enter\n", __func__);
#if SUSPEND_RESUME_ENABLE
	slp_mgr_resume();
#endif
	WCN_INFO("ok\n");

	return 0;
}

static int wcn_suspend(struct device *dev)
{
	WCN_INFO("%s enter\n", __func__);
#if SUSPEND_RESUME_ENABLE
	slp_mgr_suspend();
#endif
	WCN_INFO("ok\n");

	return 0;
}
#endif /* CONFIG_PM_SLEEP */
#endif

#if WCN_INTEGRATE_PLATFORM_DEBUG
static u32 s_wcn_debug_case;
static struct task_struct *s_thead_wcn_codes_debug;
static int wcn_codes_debug_thread(void *data)
{
	u32 i;
	static u32 is_first_time = 1;

	while (!kthread_should_stop()) {
		switch (s_wcn_debug_case) {
		case WCN_START_MARLIN_DEBUG:
			for (i = 0; i < 16; i++)
				start_integrate_wcn(i);

			s_wcn_debug_case = 0;
			break;

		case WCN_STOP_MARLIN_DEBUG:
			for (i = 0; i < 16; i++)
				stop_integrate_wcn(i);

			s_wcn_debug_case = 0;
			break;

		case WCN_START_MARLIN_DDR_FIRMWARE_DEBUG:
			for (i = 0; i < 16; i++)
				start_integrate_wcn(i);

			s_wcn_debug_case = 0;
			break;

		case WCN_START_GNSS_DEBUG:
			for (i = 16; i < 32; i++)
				start_integrate_wcn(i);

			s_wcn_debug_case = 0;
			break;

		case WCN_STOP_GNSS_DEBUG:
			for (i = 16; i < 32; i++)
				stop_integrate_wcn(i);

			s_wcn_debug_case = 0;
			break;

		case WCN_START_GNSS_DDR_FIRMWARE_DEBUG:
			for (i = 16; i < 32; i++)
				start_integrate_wcn(i);

			s_wcn_debug_case = 0;
			break;

		case WCN_PRINT_INFO:
			if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
				WCN_INFO(
					"cali[data=%p flag=%p]efuse=%p status=%p gnss=%p\n",
					&qogirl6_s_wssm_phy_offset_p->wifi.calibration_data,
					&qogirl6_s_wssm_phy_offset_p->wifi.calibration_flag,
					&qogirl6_s_wssm_phy_offset_p->wifi.efuse[0],
					&qogirl6_s_wssm_phy_offset_p->marlin.init_status,
					&qogirl6_s_wssm_phy_offset_p->include_gnss);
			} else {
				WCN_INFO(
					"cali[data=%p flag=%p]efuse=%p status=%p gnss=%p\n",
					&s_wssm_phy_offset_p->wifi.calibration_data,
					&s_wssm_phy_offset_p->wifi.calibration_flag,
					&s_wssm_phy_offset_p->wifi.efuse[0],
					&s_wssm_phy_offset_p->marlin.init_status,
					&s_wssm_phy_offset_p->include_gnss);

			}
			break;

		case WCN_BRINGUP_DEBUG:
			if (is_first_time) {
				msleep(100000);
				is_first_time = 0;
			}

			for (i = 0; i < 16; i++) {
				msleep(5000);
				start_integrate_wcn(i);
			}

			for (i = 0; i < 16; i++) {
				msleep(5000);
				stop_integrate_wcn(i);
			}

			break;

		default:
			msleep(5000);
			break;
		}
	}

	kthread_stop(s_thead_wcn_codes_debug);

	return 0;
}

static void wcn_codes_debug(void)
{
	/* Check reg read */
	s_thead_wcn_codes_debug = kthread_create(wcn_codes_debug_thread, NULL,
						 "wcn_codes_debug");
	wake_up_process(s_thead_wcn_codes_debug);
}
#endif

static void wcn_config_ctrlreg(struct wcn_device *wcn_dev, u32 start, u32 end)
{
	u32 reg_read, type, i, val, utemp_val;
	u32 debug_value = 0;
	u32 wcn_power_status = 0;
	u32 timeout = 0;

	for (i = start; i < end; i++) {
		val = 0;
		type = wcn_dev->ctrl_type[i];
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6 &&
			(type == REGMAP_WCN_BTWF_AHB ||
			type == REGMAP_WCN_GNSS_SYS_AHB)) {
			timeout = 0;
			WCN_INFO("wcn power btwf check!\n");
			do {
				if (wcn_power_status_check(wcn_dev)) {
					wcn_power_status = 1;
					break;
				}
				udelay(wcn_dev->ctrl_us_delay[i]);
				WCN_INFO("wait poweron timeout %d!\n",
						timeout);
			} while (timeout++ < 300);
			if (!wcn_power_status) {
				WCN_ERR("wcn power on fail!\n");
				return;
			}
			WCN_INFO("wcn poweron finish\n");
		}

		reg_read = wcn_dev->ctrl_reg[i] -
			   wcn_dev->ctrl_rw_offset[i];
		wcn_regmap_read(wcn_dev->rmap[type], reg_read, &val);
		WCN_INFO("rmap[%d]:ctrl_reg[%d]=0x%x,read=0x%x, set=0x%x\n",
			 type, i, reg_read, val,
			 wcn_dev->ctrl_value[i]);
		utemp_val = wcn_dev->ctrl_value[i];

		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_PIKE2) {
			if (wcn_dev->ctrl_rw_offset[i] == 0x00)
				utemp_val = val | wcn_dev->ctrl_value[i];
		}

		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
			/* release cpu reset  manually */
			if (s_wcn_device.boot_manually == true &&
					wcn_dev->ctrl_reg[i] == 0x0c) {
				WCN_INFO("manual boot!\n");
				/* set cp run to while(1) */
				/* 0x0 */
				debug_value = 0xE7FE;
				wcn_write_data_to_phy_addr(
					wcn_dev->base_addr + 0x0,
					&debug_value, sizeof(debug_value));
				/* 0x4 */
				debug_value = 0x1;
				wcn_write_data_to_phy_addr(
					wcn_dev->base_addr + 0x4,
					&debug_value, sizeof(debug_value));
				continue;
			}
			if (wcn_dev->ctrl_rw_offset[i] == 0x00) {
				/* need to clear bit */
				utemp_val = val & (~wcn_dev->ctrl_mask[i]);
				utemp_val |= wcn_dev->ctrl_value[i];
			}
		}
		/* raw: write bits */
		WCN_INFO("utemp_val 0x%x  val 0x%x\n", utemp_val, val);
		wcn_regmap_raw_write_bit(wcn_dev->rmap[type],
					wcn_dev->ctrl_reg[i], utemp_val);

		if (wcn_dev->ctrl_us_delay[i] >= 10)
			usleep_range(wcn_dev->ctrl_us_delay[i],
				     wcn_dev->ctrl_us_delay[i] + 40);
		else
			udelay(wcn_dev->ctrl_us_delay[i]);
		wcn_regmap_read(wcn_dev->rmap[type], reg_read, &val);
		WCN_INFO("rmap[%d]:ctrl_reg[%d] = 0x%x, val=0x%x\n",
			 type, i, reg_read, val);
	}
}

static void wcn_set_tcxo_init(struct wcn_device *wcn_dev)
{
	/*this function is used in sharkl6 with tcxo*/
	u32 reg_val = 0;

	/*XTL3_REL_CFG XTL3_WCN_SEL = 1*/
	reg_val = (0x1<<9);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_PMU_APB], 0x19d0, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB], 0x09d0, &reg_val);
	WCN_INFO("REG 0x640209d0:val=0x%x!\n", reg_val);

	/*XTLBUF0_REL_CFG XTLBUF0_WCN_SEL = 0*/
	reg_val = (0x1<<9);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_PMU_APB], 0x29d4, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB], 0x09d4, &reg_val);
	WCN_INFO("REG 0x640209d4:val=0x%x!\n", reg_val);

	/*XTLBUF1_REL_CFG XTLBUF1_WCN_SEL = 0*/
	reg_val = (0x1<<9);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_PMU_APB], 0x29d8, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB], 0x09d8, &reg_val);
	WCN_INFO("REG 0x640209d8:val=0x%x!\n", reg_val);

	/*XTLBUF2_REL_CFG XTLBUF2_WCN_SEL = 0*/
	reg_val = (0x1<<9);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_PMU_APB], 0x29dc, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB], 0x09dc, &reg_val);
	WCN_INFO("REG 0x640209dc:val=0x%x!\n", reg_val);

	/*XTLBUF3_REL_CFG XTLBUF3_WCN_SEL = 1*/
	reg_val = (0x1<<9);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_PMU_APB], 0x19e0, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB], 0x09e0, &reg_val);
	WCN_INFO("REG 0x640209e0:val=0x%x!\n", reg_val);

	/*XTL0_REL_CFG XTL0_WCN_SEL = 0*/
	reg_val = (0x1<<9);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_PMU_APB], 0x29c4, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB], 0x09c4, &reg_val);
	WCN_INFO("REG 0x640209c4:val=0x%x!\n", reg_val);

	/*XTL1_REL_CFG XTL1_WCN_SEL = 0*/
	reg_val = (0x1<<9);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_PMU_APB], 0x29c8, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB], 0x09c8, &reg_val);
	WCN_INFO("REG 0x640209c8:val=0x%x!\n", reg_val);

	/*XTL2_REL_CFG XTL2_WCN_SEL = 0*/
	reg_val = (0x1<<9);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_PMU_APB], 0x29cc, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB], 0x09cc, &reg_val);
	WCN_INFO("REG 0x640209cc:val=0x%x!\n", reg_val);

	/*	wcn_xtl_ctrl
	 *	wcn_ref_26m_sel = 2'b01
	 *	wcn_bb_ck26m_sel = 1'b1
	 */
	reg_val = 0x11;
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_PMU_APB], 0x1fe4, reg_val);
	reg_val = 0x4;
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_PMU_APB], 0x2fe4, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB], 0x0fe4, &reg_val);
	WCN_INFO("REG 0x64020fe4:val=0x%x!\n", reg_val);
}

void wcn_cpu_bootup(struct wcn_device *wcn_dev)
{
	u32 reg_nr;

	if (!wcn_dev)
		return;

	reg_nr = wcn_dev->reg_nr < REG_CTRL_CNT_MAX ?
		wcn_dev->reg_nr : REG_CTRL_CNT_MAX;
	wcn_config_ctrlreg(wcn_dev, wcn_dev->ctrl_probe_num, reg_nr);
}

#if 0
static const struct of_device_id wcn_match_table[] = {
	{ .compatible = "unisoc,integrate_marlin",},
	{ .compatible = "unisoc,integrate_gnss",},
	{ },
};
#endif

static int wcn_efuse_cal_read(struct device_node *np, const char *cell_id,
			      u32 *val)
{
	struct nvmem_cell *cell;
	void *buf;
	size_t len;

	cell = of_nvmem_cell_get(np, cell_id);
	if (IS_ERR(cell))
		return PTR_ERR(cell);
	buf = nvmem_cell_read(cell, &len);
	if (IS_ERR(buf)) {
		nvmem_cell_put(cell);
		return PTR_ERR(buf);
	}

	memcpy(val, buf, min(len, sizeof(u32)));
	kfree(buf);
	nvmem_cell_put(cell);
	return 0;
}

/* only wifi need it */
static void marlin_write_efuse_data(void)
{
	phys_addr_t phy_addr;
	u32 iloop = 0;
	u32 tmp_value[WIFI_EFUSE_BLOCK_COUNT];
	u32 chip_type;

	chip_type = wcn_platform_chip_type();
	memset(&tmp_value, 0, sizeof(tmp_value[0]) *
	       WIFI_EFUSE_BLOCK_COUNT);
	for (iloop = 0; iloop < WIFI_EFUSE_BLOCK_COUNT; iloop++) {
		tmp_value[iloop] = wcn_efuse_val[iloop];
		WCN_INFO("s_wifi_efuse_id[%d][%d]=%d, value=0x%x\n",
			 chip_type, iloop,
			 s_wifi_efuse_id[chip_type][iloop],
			 tmp_value[iloop]);
	}
	/* copy efuse data to target ddr address */
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		phy_addr = s_wcn_device.btwf_device->base_addr +
		   (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->efuse[0];
	} else {
		phy_addr = s_wcn_device.btwf_device->base_addr +
		   (phys_addr_t)&s_wssm_phy_offset_p->wifi.efuse[0];
	}
	wcn_write_data_to_phy_addr(phy_addr, &tmp_value,
				   sizeof(tmp_value[0]) *
				   WIFI_EFUSE_BLOCK_COUNT);

	WCN_INFO("%s finish.\n", __func__);
}

#define WCN_EFUSE_TEMPERATURE_MAGIC 0x432ff678
/* now just sharkle */
static void marlin_write_efuse_temperature(void)
{
	phys_addr_t phy_addr;
	u32 magic;

	magic = WCN_EFUSE_TEMPERATURE_MAGIC;
	if (wcn_efuse_val[3] == 0) {
		WCN_INFO("temperature efuse read err\n");
		magic += 1;
		goto out;
	}
	WCN_INFO("temperature efuse read 0x%x\n", wcn_efuse_val[3]);
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		phy_addr = s_wcn_device.btwf_device->base_addr +
		  (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->efuse_temper_val;
	} else {
		phy_addr = s_wcn_device.btwf_device->base_addr +
		  (phys_addr_t)&s_wssm_phy_offset_p->efuse_temper_val;
	}
	wcn_write_data_to_phy_addr(phy_addr, &wcn_efuse_val[3],
				   sizeof(wcn_efuse_val[3]));
out:
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		phy_addr = s_wcn_device.btwf_device->base_addr +
		   (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->efuse_temper_magic;
	} else {
		phy_addr = s_wcn_device.btwf_device->base_addr +
		   (phys_addr_t)&s_wssm_phy_offset_p->efuse_temper_magic;
	}
	wcn_write_data_to_phy_addr(phy_addr, &magic, sizeof(magic));
}

void wcn_marlin_write_efuse(void)
{
	marlin_write_efuse_data();
	marlin_write_efuse_temperature();
}

/* used for provide efuse data to gnss */
void gnss_write_efuse_data(void)
{
	struct wcn_device *wcn_dev = s_wcn_device.gnss_device;
	phys_addr_t phy_addr, phy_addr1;
	u32 efuse_enable_value = GNSS_EFUSE_ENABLE_VALUE;
	u32 iloop = 0;
	u32 chip_type;
	u32 tmp_value[GNSS_EFUSE_BLOCK_COUNT];

	chip_type = wcn_platform_chip_type();
	memset(&tmp_value, 0, sizeof(tmp_value[0]) * GNSS_EFUSE_BLOCK_COUNT);
	for (iloop = 0; iloop < GNSS_EFUSE_BLOCK_COUNT; iloop++) {
		tmp_value[iloop] = gnss_efuse_val[iloop];
		WCN_INFO("s_gnss_efuse_id[%d][%d]=%d, value=0x%x\n",
			 chip_type, iloop,
			 s_gnss_efuse_id[chip_type][iloop],
			 tmp_value[iloop]);
	}
	/* copy efuse data to target ddr address */
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		phy_addr = wcn_dev->base_addr +
				wcn_get_apcp_sync_addr(wcn_dev) +
				s_wcngnss_sync_addr.gnss_efuse_value;
	else
		phy_addr = wcn_dev->base_addr +
			   GNSS_EFUSE_DATA_OFFSET;
	wcn_write_data_to_phy_addr(phy_addr, &tmp_value,
				   sizeof(tmp_value[0]) *
				   GNSS_EFUSE_BLOCK_COUNT);
	/*write efuse function enable value*/
	phy_addr1 = wcn_dev->base_addr +
		    GNSS_EFUSE_ENABLE_ADDR;
	wcn_write_data_to_phy_addr(phy_addr1, &efuse_enable_value, 4);

	WCN_INFO("%s finish.\n", __func__);
}

static void wcn_parse_dt_regmap_judge(struct wcn_device *wcn_dev)
{
	int i = 0;

	wcn_dev->need_regmap[REGMAP_PMU_APB] = TRUE;
	wcn_dev->need_regmap[REGMAP_ANLG_WRAP_WCN] = TRUE;
	wcn_dev->need_sync_efuse = TRUE;
	wcn_dev->need_set_sync_addr = FALSE;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKLE) {
		wcn_dev->need_regmap[REGMAP_PUB_APB] = TRUE;
		wcn_dev->need_regmap[REGMAP_ANLG_PHY_G6] = TRUE;
	} else if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_PIKE2) {

	} else if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKL3) {
		wcn_dev->need_regmap[REGMAP_WCN_REG] = TRUE;
		wcn_dev->need_regmap[REGMAP_ANLG_PHY_G5] = TRUE;
	} else if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		wcn_dev->need_regmap[REGMAP_PUB_APB] = TRUE;
		if (strcmp(wcn_dev->name, WCN_MARLIN_DEV_NAME) == 0)
			wcn_dev->need_regmap[REGMAP_WCN_BTWF_AHB] = TRUE;
		else if (strcmp(wcn_dev->name, WCN_GNSS_DEV_NAME) == 0)
			wcn_dev->need_regmap[REGMAP_WCN_GNSS_SYS_AHB] = TRUE;

		wcn_dev->need_regmap[REGMAP_WCN_AON_AHB] = TRUE;
		wcn_dev->need_regmap[REGMAP_WCN_AON_APB] = TRUE;
		wcn_dev->need_regmap[REGMAP_ANLG_WRAP_WCN] = FALSE;
		wcn_dev->need_set_sync_addr = TRUE;
		wcn_dev->need_gpio = TRUE;
		wcn_dev->need_dcxo1v8 = TRUE;
	}
	for (i = 0; i < REGMAP_TYPE_NR; i++)
		WCN_INFO("need_regmap[%d] : %d\n", i, wcn_dev->need_regmap[i]);
}

static int wcn_parse_dt(struct platform_device *pdev,
			struct wcn_device *wcn_dev)
{
	struct device_node *np = pdev->dev.of_node;
	u32 cr_num;
	int index, ret;
	u32 i;
	struct resource res;
	struct wcn_device_manage *wcn_devm = &s_wcn_device;
	struct device_node *cmdline_node;
	u32 rc = 0;
	const char *cmd_line;

	WCN_INFO("%s start!\n", __func__);

	if (!wcn_dev) {
		WCN_ERR("wcn_dev NULL\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "sprd,gnss-cp-start-addr",
				   &GNSS_CP_START_ADDR);
	if (ret) {
		WCN_ERR("failed to read sprd,gnss-cp-start-addr\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "sprd,gnss-firmware-max-size",
				   &GNSS_FIRMWARE_MAX_SIZE);
	if (ret) {
		WCN_ERR("failed to read sprd,gnss-firmware-max-size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "sprd,gnss-dump-packet-size",
				   &GNSS_DUMP_PACKET_SIZE);
	if (ret) {
		WCN_ERR("failed to read sprd,gnss-dump-packet-size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "sprd,gnss-share-memory-size",
				   &GNSS_SHARE_MEMORY_SIZE);
	if (ret) {
		WCN_ERR("failed to read sprd,gnss-share-memory-size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "sprd,gnss-dump-iram-start-addr",
				   &GNSS_DUMP_IRAM_START_ADDR);
	if (ret) {
		WCN_ERR("failed to read sprd,gnss-dump-iram-start-addr\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "sprd,gnss-cp-iram-data-num",
				   &GNSS_CP_IRAM_DATA_NUM);
	if (ret) {
		WCN_ERR("failed to read sprd,gnss-cp-iram-data-num\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "sprd,gnss-dump-reg-number",
				   &GNSS_DUMP_REG_NUMBER);
	if (ret) {
		WCN_ERR("failed to read sprd,gnss-dump-reg-number\n");
		return -EINVAL;
	}

	/* get the wcn chip name */
	ret = of_property_read_string(np,
				      "sprd,name",
				      (const char **)&wcn_dev->name);
	if (ret)
		WCN_ERR("sprd,name, ret %d\n", ret);

	/* get apb reg handle */
	wcn_dev->rmap[REGMAP_AON_APB] =
				syscon_regmap_lookup_by_phandle(
				np, "sprd,syscon-ap-apb");
	if (IS_ERR(wcn_dev->rmap[REGMAP_AON_APB])) {
		WCN_ERR("failed to find sprd,syscon-ap-apb\n");
		return -EINVAL;
	}

	wcn_parse_platform_chip_id(wcn_dev);

	wcn_parse_dt_regmap_judge(wcn_dev);

	/* get pmu reg handle */
	if (wcn_dev->need_regmap[REGMAP_PMU_APB] == TRUE) {
		wcn_dev->rmap[REGMAP_PMU_APB] =
					syscon_regmap_lookup_by_phandle(
					np, "sprd,syscon-ap-pmu");
		if (IS_ERR(wcn_dev->rmap[REGMAP_PMU_APB])) {
			WCN_ERR("failed to find sprd,syscon-ap-pmu\n");
			return -EINVAL;
		}
	}

	/* get pub apb reg handle:SHARKLE has it, but PIKE2 hasn't  */
	if (wcn_dev->need_regmap[REGMAP_PUB_APB] == TRUE) {
		wcn_dev->rmap[REGMAP_PUB_APB] =
					syscon_regmap_lookup_by_phandle(
					np, "sprd,syscon-ap-pub-apb");
		if (IS_ERR(wcn_dev->rmap[REGMAP_PUB_APB])) {
			WCN_ERR("failed to find sprd,syscon-ap-pub-apb\n");
			return -EINVAL;
		}
		WCN_INFO("PUB APB REG MAP!\n");
	}

	/* get  anlg wrap wcn reg handle */
	if (wcn_dev->need_regmap[REGMAP_ANLG_WRAP_WCN] == TRUE) {
		wcn_dev->rmap[REGMAP_ANLG_WRAP_WCN] =
					syscon_regmap_lookup_by_phandle(
					np, "sprd,syscon-anlg-wrap-wcn");
		if (IS_ERR(wcn_dev->rmap[REGMAP_ANLG_WRAP_WCN])) {
			WCN_ERR("failed to find sprd,anlg-wrap-wcn\n");
			return -EINVAL;
		}
	}

	if (wcn_dev->need_regmap[REGMAP_ANLG_PHY_G6] == TRUE) {
		/* get  anlg wrap wcn reg handle */
		wcn_dev->rmap[REGMAP_ANLG_PHY_G6] =
					syscon_regmap_lookup_by_phandle(
					np, "sprd,syscon-anlg-phy-g6");
		if (IS_ERR(wcn_dev->rmap[REGMAP_ANLG_PHY_G6])) {
			WCN_ERR("failed to find sprd,anlg-phy-g6\n");
			return -EINVAL;
		}
	}

	/* SharkL3:The base Reg changed which used by AP read CP2 Regs */
	if (wcn_dev->need_regmap[REGMAP_WCN_REG] == TRUE) {
		/* get  anlg wrap wcn reg handle */
		wcn_dev->rmap[REGMAP_WCN_REG] =
		syscon_regmap_lookup_by_phandle(np, "sprd,syscon-wcn-reg");
		if (IS_ERR(wcn_dev->rmap[REGMAP_WCN_REG])) {
			WCN_ERR("failed to find sprd,wcn-reg\n");
			return -EINVAL;
		}

		WCN_INFO("success to find sprd,wcn-reg for SharkL3 %p\n",
			 wcn_dev->rmap[REGMAP_WCN_REG]);
	}

	if (wcn_dev->need_regmap[REGMAP_ANLG_PHY_G5] == TRUE) {
		/* get  anlg wrap wcn reg handle */
		wcn_dev->rmap[REGMAP_ANLG_PHY_G5] =
		syscon_regmap_lookup_by_phandle(np, "sprd,syscon-anlg-phy-g5");
		if (IS_ERR(wcn_dev->rmap[REGMAP_ANLG_PHY_G5]))
			WCN_ERR("failed to find sprd,anlg-phy-g5\n");
	}

	if (wcn_dev->need_regmap[REGMAP_WCN_BTWF_AHB] == TRUE) {
		/* get  wcn btwf ahb reg handle */
		wcn_dev->rmap[REGMAP_WCN_BTWF_AHB] =
		syscon_regmap_lookup_by_phandle(np, "sprd,syscon-wcn-btwf-ahb");
		if (IS_ERR(wcn_dev->rmap[REGMAP_WCN_BTWF_AHB]))
			WCN_ERR("failed to find sprd,wcn-btwf-ahb\n");
	}

	if (wcn_dev->need_regmap[REGMAP_WCN_GNSS_SYS_AHB] == TRUE) {
		/* get  wcn gnss sys anh reg handle */
		wcn_dev->rmap[REGMAP_WCN_GNSS_SYS_AHB] =
		syscon_regmap_lookup_by_phandle(np, "sprd,syscon-wcn-gnss-ahb");
		if (IS_ERR(wcn_dev->rmap[REGMAP_WCN_GNSS_SYS_AHB]))
			WCN_ERR("failed to find sprd,wcn-gnss-ahb\n");
	}

	if (wcn_dev->need_regmap[REGMAP_WCN_AON_AHB] == TRUE) {
		/* get  wcn aon ahb reg handle */
		wcn_dev->rmap[REGMAP_WCN_AON_AHB] =
		syscon_regmap_lookup_by_phandle(np, "sprd,syscon-wcn-aon-ahb");
		if (IS_ERR(wcn_dev->rmap[REGMAP_WCN_AON_AHB]))
			WCN_ERR("failed to find sprd,wcn-aon-ahb\n");
	}

	if (wcn_dev->need_regmap[REGMAP_WCN_AON_APB] == TRUE) {
		/* get	wcn aon ahb reg handle */
		wcn_dev->rmap[REGMAP_WCN_AON_APB] =
		syscon_regmap_lookup_by_phandle(np, "sprd,syscon-wcn-aon-apb");
		if (IS_ERR(wcn_dev->rmap[REGMAP_WCN_AON_APB]))
			WCN_ERR("failed to find sprd,wcn-aon-apb\n");
	}

	ret = of_property_read_u32(np, "sprd,ctrl-probe-num",
				   &wcn_dev->ctrl_probe_num);
	if (ret) {
		WCN_ERR("failed to find sprd,ctrl-probe-num\n");
		return -EINVAL;
	}

	/*
	 * get ctrl_reg offset, the ctrl-reg variable number, so need
	 * to start reading from the largest until success
	 */
	cr_num = of_property_count_elems_of_size(np, "sprd,ctrl-reg", 4);
	if (cr_num > REG_CTRL_CNT_MAX) {
		WCN_ERR("DTS config err. cr_num=%d\n", cr_num);
		return -EINVAL;
	}

	do {
		ret = of_property_read_u32_array(np, "sprd,ctrl-reg",
						 (u32 *)wcn_dev->ctrl_reg,
						 cr_num);
		if (ret)
			cr_num--;
		if (!cr_num)
			return -EINVAL;
	} while (ret);

	wcn_dev->reg_nr = cr_num;
	for (i = 0; i < cr_num; i++)
		WCN_DBG("ctrl_reg[%d] = 0x%x\n",
			i, wcn_dev->ctrl_reg[i]);

	/* get ctrl_mask */
	ret = of_property_read_u32_array(np, "sprd,ctrl-mask",
					 (u32 *)wcn_dev->ctrl_mask, cr_num);
	if (ret)
		return -EINVAL;
	for (i = 0; i < cr_num; i++)
		WCN_DBG("ctrl_mask[%d] = 0x%08x\n",
			i, wcn_dev->ctrl_mask[i]);

	/* get ctrl_value */
	ret = of_property_read_u32_array(np,
					 "sprd,ctrl-value",
					 (u32 *)wcn_dev->ctrl_value,
					 cr_num);
	if (ret)
		return -EINVAL;
	for (i = 0; i < cr_num; i++)
		WCN_INFO("ctrl_value[%d] = 0x%08x\n",
			 i, wcn_dev->ctrl_value[i]);

	/* get ctrl_rw_offset */
	ret = of_property_read_u32_array(np,
					 "sprd,ctrl-rw-offset",
					 (u32 *)wcn_dev->ctrl_rw_offset,
					 cr_num);
	if (ret)
		return -EINVAL;
	for (i = 0; i < cr_num; i++)
		WCN_DBG("ctrl_rw_offset[%d] = 0x%08x\n",
			i, wcn_dev->ctrl_rw_offset[i]);

	/* get ctrl_us_delay */
	ret = of_property_read_u32_array(np,
					 "sprd,ctrl-us-delay",
					 (u32 *)wcn_dev->ctrl_us_delay,
					 cr_num);
	if (ret)
		return -EINVAL;
	for (i = 0; i < cr_num; i++)
		WCN_DBG("ctrl_us_delay[%d] = 0x%08x\n",
			i, wcn_dev->ctrl_us_delay[i]);

	/* get ctrl_type */
	ret = of_property_read_u32_array(np, "sprd,ctrl-type",
					 (u32 *)wcn_dev->ctrl_type, cr_num);
	if (ret)
		return -EINVAL;

	for (i = 0; i < cr_num; i++)
		WCN_DBG("ctrl_type[%d] = 0x%08x\n",
			i, wcn_dev->ctrl_type[i]);

	/*
	 * Add a new group to control shut down WCN
	 * get ctrl_reg offset, the ctrl-reg variable number, so need
	 * to start reading from the largest until success
	 */
	cr_num = of_property_count_elems_of_size(np,
						 "sprd,ctrl-shutdown-reg", 4);
	if (cr_num > REG_CTRL_CNT_MAX) {
		WCN_ERR("DTS config err. cr_num=%d\n", cr_num);
		return -EINVAL;
	}

	do {
		ret = of_property_read_u32_array(np,
						 "sprd,ctrl-shutdown-reg",
						 (u32 *)
						 wcn_dev->ctrl_shutdown_reg,
						 cr_num);
		if (ret)
			cr_num--;
		if (!cr_num)
			return -EINVAL;
	} while (ret);

	wcn_dev->reg_shutdown_nr = cr_num;
	for (i = 0; i < cr_num; i++) {
		WCN_DBG("ctrl_shutdown_reg[%d] = 0x%x\n",
			i, wcn_dev->ctrl_shutdown_reg[i]);
	}

	/* get ctrl_shutdown_mask */
	ret = of_property_read_u32_array(np,
					 "sprd,ctrl-shutdown-mask",
					 (u32 *)
					 wcn_dev->ctrl_shutdown_mask,
					 cr_num);
	if (ret)
		return -EINVAL;
	for (i = 0; i < cr_num; i++) {
		WCN_DBG("ctrl_shutdown_mask[%d] = 0x%08x\n",
			i, wcn_dev->ctrl_shutdown_mask[i]);
	}

	/* get ctrl_shutdown_value */
	ret = of_property_read_u32_array(np, "sprd,ctrl-shutdown-value",
					 (u32 *)
					 wcn_dev->ctrl_shutdown_value,
					 cr_num);
	if (ret)
		return -EINVAL;
	for (i = 0; i < cr_num; i++) {
		WCN_DBG("ctrl_shutdown_value[%d] = 0x%08x\n",
			i, wcn_dev->ctrl_shutdown_value[i]);
	}

	/* get ctrl_shutdown_rw_offset */
	ret = of_property_read_u32_array(np,
					 "sprd,ctrl-shutdown-rw-offset",
					 (u32 *)
					 wcn_dev->ctrl_shutdown_rw_offset,
					 cr_num);
	if (ret)
		return -EINVAL;
	for (i = 0; i < cr_num; i++) {
		WCN_DBG("ctrl_shutdown_rw_offset[%d] = 0x%08x\n",
			i, wcn_dev->ctrl_shutdown_rw_offset[i]);
	}

	/* get ctrl_shutdown_us_delay */
	ret = of_property_read_u32_array(np,
					 "sprd,ctrl-shutdown-us-delay",
					 (u32 *)
					 wcn_dev->ctrl_shutdown_us_delay,
					 cr_num);
	if (ret)
		return -EINVAL;
	for (i = 0; i < cr_num; i++) {
		WCN_DBG("ctrl_shutdown_us_delay[%d] = 0x%08x\n",
			i, wcn_dev->ctrl_shutdown_us_delay[i]);
	}

	/* get ctrl_shutdown_type */
	ret = of_property_read_u32_array(np,
					 "sprd,ctrl-shutdown-type",
					 (u32 *)
					 wcn_dev->ctrl_shutdown_type,
					 cr_num);
	if (ret)
		return -EINVAL;

	for (i = 0; i < cr_num; i++)
		WCN_DBG("ctrl_shutdown_type[%d] = 0x%08x\n",
			i, wcn_dev->ctrl_shutdown_type[i]);
	/* get vddwcn */
	if (!wcn_devm->vddwcn) {
		wcn_devm->vddwcn = devm_regulator_get(&pdev->dev,
							 "vddwcn");
		if (IS_ERR(wcn_devm->vddwcn)) {
			WCN_ERR("Get regulator of vddwcn error!\n");
			return -EINVAL;
		}
		WCN_INFO("Get regulator of vddwcn\n");
	}

	/* get dcxo1v8 */
	if (wcn_dev->need_dcxo1v8 && !wcn_devm->dcxo1v8) {
		wcn_devm->dcxo1v8 =
			devm_regulator_get(&pdev->dev, "dcxo1v8");
		if (IS_ERR(wcn_devm->dcxo1v8)) {
			WCN_ERR("Get regulator of dcxo1v8 error!\n");
			return -EINVAL;
		}
		WCN_INFO("Get regulator of dcxo1v8\n");
	}

	/* get vddwifipa: only MARLIN has it */
	if (strcmp(wcn_dev->name, WCN_MARLIN_DEV_NAME) == 0) {
		wcn_dev->vddwifipa = devm_regulator_get(&pdev->dev,
							"vddwifipa");
		if (IS_ERR(wcn_dev->vddwifipa)) {
			WCN_ERR("Get regulator of vddwifipa error!\n");
			return -EINVAL;
		}
		WCN_INFO("Get regulator of vddwifipa\n");
	}
	if (wcn_dev->need_gpio) {
		if (!wcn_devm->merlion_chip_en) {
			wcn_devm->merlion_chip_en =
				gpiod_get(&pdev->dev,
					"merlion-chip-en", GPIOD_OUT_LOW);
			if (IS_ERR(wcn_devm->merlion_chip_en)) {
				WCN_ERR("get gpio merlion chip en error!\n");
				return -EINVAL;
			}
			WCN_INFO("get merlion chip gpio\n");
		}
		if (!wcn_devm->merlion_reset) {
			wcn_devm->merlion_reset =
				gpiod_get(&pdev->dev,
				"merlion-rst", GPIOD_OUT_LOW);
			if (IS_ERR(wcn_devm->merlion_reset)) {
				WCN_ERR("get gpio merlion rst error!\n");
				return -EINVAL;
			}
			WCN_INFO("get merlion rst gpio\n");
		}
		if (!wcn_devm->clk_26m_type_sel) {
			wcn_devm->clk_26m_type_sel =
				gpiod_get(&pdev->dev,
				"xtal-26m-type-sel", GPIOD_IN);
			if (IS_ERR(wcn_devm->clk_26m_type_sel)) {
				WCN_ERR("get xtal-26m-type-sel error!\n");
				wcn_devm->clk_xtal_26m.type =
					WCN_CLOCK_TYPE_TSX;
			} else {
				if (gpiod_get_value(wcn_devm->clk_26m_type_sel))
					wcn_devm->clk_xtal_26m.type =
						WCN_CLOCK_TYPE_TSX;
				else {
					wcn_devm->clk_xtal_26m.type =
						WCN_CLOCK_TYPE_TCXO;
					if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
						wcn_set_tcxo_init(wcn_dev);
						WCN_INFO("WCN init PUM_APB_RF with tcxo\n");
					}
				}
			}
			WCN_INFO("get xtal-26m-type-sel gpio\n");
		}
	}

	/* get cp base */
	index = 0;
	ret = of_address_to_resource(np, index, &res);
	if (ret)
		return -EINVAL;
	wcn_dev->base_addr = res.start;
	wcn_dev->maxsz = res.end - res.start + 1;
	WCN_INFO("cp base = %llu, size = 0x%x\n",
		 (u64)wcn_dev->base_addr, wcn_dev->maxsz);
	if (strcmp(wcn_dev->name, WCN_MARLIN_DEV_NAME) == 0) {
		wcn_dev->db_to_ddr_disable = of_property_read_bool(np,
				"sprd,debugbus-to-ddr-disable");
		if (wcn_dev->db_to_ddr_disable == true) {
			WCN_INFO("Debugbus data does not need to be saved to DDR\n");
			wcn_dev->dbus.base_addr = 0xffffffff; /* invalid addr */
			wcn_dev->dbus.maxsz = DEBUGBUS_TO_DDR_LEN;
		} else {
			index++;
			ret = of_address_to_resource(np, index, &res);
			if (ret) {
				WCN_INFO("Use temporary debugbus DDR\n");
				wcn_dev->dbus.base_addr = DEBUGBUS_TO_DDR_BASE;
				wcn_dev->dbus.maxsz = DEBUGBUS_TO_DDR_LEN;
			} else {
				wcn_dev->dbus.base_addr = res.start;
				wcn_dev->dbus.maxsz = res.end - res.start + 1;
			}
		}
		WCN_INFO("index = %d, dbus base = 0x%llx, size = 0x%x\n", index,
				(u64)wcn_dev->dbus.base_addr, wcn_dev->dbus.maxsz);

		index++;
		ret = of_address_to_resource(np, index, &res);
		if (ret) {
			WCN_INFO("Use temporary debugbus register\n");
			wcn_dev->dbus.phy_reg = DEBUGBUS_REG_BASE;
			wcn_dev->dbus.dbus_max_offset = DEBUGBUS_REG_LEN;
			wcn_dev->dbus.dbus_reg_base = ioremap(wcn_dev->dbus.phy_reg,
					wcn_dev->dbus.dbus_max_offset);
		} else {
			wcn_dev->dbus.phy_reg = res.start;
			wcn_dev->dbus.dbus_max_offset = res.end - res.start + 1;
			wcn_dev->dbus.dbus_reg_base = of_iomap(np, index);
		}

		WCN_INFO("index = %d, phy_reg=0x%llx,size=0x%x,map to dbus_reg_base=0x%p\n", index,
			(u64)wcn_dev->dbus.phy_reg, wcn_dev->dbus.dbus_max_offset,
			wcn_dev->dbus.dbus_reg_base);
	}
	ret = of_property_read_string(np, "sprd,file-name",
				      (const char **)&wcn_dev->file_path);
	if (!ret)
		WCN_DBG("firmware name:%s\n", wcn_dev->file_path);

	ret = of_property_read_string(np, "sprd,file-name-ext",
				      (const char **)&wcn_dev->file_path_ext);
	if (!ret)
		WCN_DBG("firmware name ext:%s\n", wcn_dev->file_path_ext);
	ret = of_property_read_string(np, "sprd,file-name-ufs",
					(const char **)&wcn_dev->file_path_ufs);
	if (!ret)
		WCN_DBG("firmware name:%s\n", wcn_dev->file_path_ufs);

	ret = of_property_read_string(np, "sprd,file-name-ext-ufs",
					(const char **)&wcn_dev->file_path_ext_ufs);
	if (!ret)
		WCN_DBG("firmware name ext:%s\n", wcn_dev->file_path_ext_ufs);
	ret = of_property_read_string(np, "sprd,firmware-path",
					(const char **)&wcn_dev->firmware_path_name);
	if (!ret) {
		WCN_DBG("firmware path:%s\n", wcn_dev->firmware_path_name);
		strcpy(gnss_firmware_path, wcn_dev->firmware_path_name);
	}
	cmdline_node = of_find_node_by_path("/chosen");
	if (cmdline_node)
		rc = of_property_read_string(cmdline_node, "bootargs", &cmd_line);

	if (slot_suffix) {
		if (strcmp(slot_suffix, "_a") == 0)
			wcn_dev->fstab = 'a';
		else if (strcmp(slot_suffix, "_b") == 0)
			wcn_dev->fstab = 'b';
	} else {
		if (!rc) {
			fstab_ab = strstr(cmd_line, SUFFIX);
			if (fstab_ab) {
				WCN_INFO("fstab: %s.\n", fstab_ab);
				if (strncmp(fstab_ab + strlen(SUFFIX), "_a", 2) == 0)
					wcn_dev->fstab = 'a';
				else if (strncmp(fstab_ab + strlen(SUFFIX), "_b", 2) == 0)
					wcn_dev->fstab = 'b';
			} else {
				fstab_ab = strstr(cmd_line, SUFFIXS);
				if (fstab_ab) {
					WCN_INFO("fstab_sprdboot: %s.\n", fstab_ab);
					if (strncmp(fstab_ab + strlen(SUFFIXS), "_a", 2) == 0)
						wcn_dev->fstab = 'a';
					else if (strncmp(fstab_ab + strlen(SUFFIXS), "_b", 2) == 0)
						wcn_dev->fstab = 'b';
				}
			}
		}
	}

	/* get cp source file length */
	ret = of_property_read_u32_index(np,
					 "sprd,file-length",
					 0, &wcn_dev->file_length);
	WCN_INFO("wcn_dev->file_length:%d\n", wcn_dev->file_length);
	if (ret)
		return -EINVAL;

	/* get wcn efuse values from dts */
	if (wcn_dev->need_sync_efuse &&
		strcmp(wcn_dev->name, WCN_MARLIN_DEV_NAME) == 0) {
		ret = wcn_efuse_cal_read(np, "wcn_efuse_blk0",
					 &wcn_efuse_val[0]);
		if (ret) {
			wcn_efuse_val[0] = 0x11111111;
			WCN_ERR("wcn_efuse_blk0 read error, ret %d\n", ret);
		}

		ret = wcn_efuse_cal_read(np, "wcn_efuse_blk1",
					 &wcn_efuse_val[1]);
		if (ret) {
			wcn_efuse_val[1] = 0x22222222;
			WCN_ERR("wcn_efuse_blk1 read error, ret %d\n", ret);
		}

		ret = wcn_efuse_cal_read(np, "wcn_efuse_blk2",
					 &wcn_efuse_val[2]);
		if (ret) {
			wcn_efuse_val[2] = 0x33333333;
			WCN_ERR("wcn_efuse_blk2 read error, ret %d\n", ret);
		}

		if ((wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKLE) ||
			(wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKL3)) {
			ret = wcn_efuse_cal_read(np, "wcn_efuse_blk3",
						 &wcn_efuse_val[3]);
			if (ret)
				WCN_ERR("wcn_efuse_blk3 read error, ret %d\n",
					ret);
		}
	}
	/* get gnss efuse values from dts */
	if (wcn_dev->need_sync_efuse &&
		strcmp(wcn_dev->name, WCN_GNSS_DEV_NAME) == 0) {

		ret = wcn_efuse_cal_read(np, "gnss_efuse_blk0",
					 &gnss_efuse_val[0]);
		if (ret)
			WCN_ERR("gnss_efuse_blk0 read error, ret %d\n", ret);

		ret = wcn_efuse_cal_read(np, "gnss_efuse_blk1",
					 &gnss_efuse_val[1]);
		if (ret)
			WCN_ERR("gnss_efuse_blk1 read error, ret %d\n", ret);

		ret = wcn_efuse_cal_read(np, "gnss_efuse_blk2",
					 &gnss_efuse_val[2]);
		if (ret)
			WCN_ERR("gnss_efuse_blk2 read error, ret %d\n", ret);
	}

	if (wcn_dev->need_set_sync_addr) {
		ret = of_property_read_u32_index(np,
					 "sprd,apcp-sync-addr",
					 0, (u32 *)&wcn_dev->apcp_sync_addr);
		if (ret)
			WCN_ERR("sprd,apcp-sync-addr, ret %d\n", ret);
		WCN_INFO("wcn_dev->apcp-sync-addr:0x%08llx\n",
				    wcn_dev->apcp_sync_addr);
		/* qogirl6 get apcp sync addr from  */
		wcn_set_apcp_sync_addr(wcn_dev);
	}
	wcn_dev->start = wcn_proc_native_start;
	wcn_dev->stop = wcn_proc_native_stop;

	return 0;
}

static int wcn_platform_open(struct inode *inode, struct file *filp)
{
	struct platform_proc_file_entry
	*entry = (struct platform_proc_file_entry *)PDE_DATA(inode);

	WCN_INFO("entry name:%s\n!", entry->name);

	filp->private_data = entry;

	return 0;
}

static ssize_t wcn_platform_read(struct file *filp,
				 char __user *buf,
				 size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t wcn_platform_write(struct file *filp,
				  const char __user *buf,
				  size_t count,
				  loff_t *ppos)
{
	struct platform_proc_file_entry
		*entry = (struct platform_proc_file_entry *)filp->private_data;
	struct wcn_device *wcn_dev = entry->wcn_dev;
	char *type = entry->name;
	unsigned int flag;
	char str[WCN_PROC_FILE_LENGTH_MAX + 1];
	u32 sub_sys = 0;

	flag = entry->flag;
	WCN_INFO("type = %s flag = 0x%x\n", type, flag);

	if ((flag & BE_WRONLY) == 0)
		return -EPERM;

	memset(&str[0], 0, WCN_PROC_FILE_LENGTH_MAX + 1);
	if (copy_from_user(&str[0], buf, WCN_PROC_FILE_LENGTH_MAX) == 0) {
		if (strncmp(str, "gnss", strlen("gnss")) == 0)
			sub_sys = WCN_GNSS;
		else
			sub_sys = str[0] - '0';
	} else {
		WCN_ERR("copy_from_user too length %s!\n", buf);
		return -EINVAL;
	}
	if (strncmp(str, "manual_boot_on",
			strlen("manual_boot_on")) == 0) {
		s_wcn_device.boot_manually = true;
		return count;
	}
	if (strncmp(str, "manual_boot_off",
			strlen("manual_boot_off")) == 0) {
		s_wcn_device.boot_manually = false;
		return count;
	}

	if ((flag & BE_CTRL_ON) != 0) {
		start_integrate_wcn(sub_sys);
		wcn_dev->status = CP_NORMAL_STATUS;
		WCN_INFO("start, str=%s!\n", str);

		return count;
	} else if ((flag & BE_CTRL_OFF) != 0) {
		stop_integrate_wcn(sub_sys);
		wcn_dev->status = CP_STOP_STATUS;
		WCN_INFO("stop, str=%s!\n", str);

		return count;
	}

	return 0;
}

static const struct proc_ops wcn_platform_fs_fops = {
	.proc_open		= wcn_platform_open,
	.proc_read		= wcn_platform_read,
	.proc_write		= wcn_platform_write,
};

static inline void wcn_platform_fs_init(struct wcn_device *wcn_dev)
{
	u8 i, ucnt;
	unsigned int flag;
	umode_t    mode = 0;

	wcn_dev->platform_fs.platform_proc_dir_entry =
		proc_mkdir(wcn_dev->name, NULL);

	memset(wcn_dev->platform_fs.entrys,
	       0, sizeof(wcn_dev->platform_fs.entrys));

	for (flag = 0, ucnt = 0, i = 0;
		i < MAX_PLATFORM_ENTRY_NUM;
		i++, flag = 0, mode = 0) {
		switch (i) {
		case 0:
			wcn_dev->platform_fs.entrys[i].name = "start";
			flag |= (BE_WRONLY | BE_CTRL_ON);
			ucnt++;
			break;

		case 1:
			wcn_dev->platform_fs.entrys[i].name = "stop";
			flag |= (BE_WRONLY | BE_CTRL_OFF);
			ucnt++;
			break;

		case 2:
			wcn_dev->platform_fs.entrys[i].name = "status";
			flag |= (BE_RDONLY | BE_RDWDTS);
			ucnt++;
			break;

		default:
			return;		/* we didn't use it until now */
		}

		wcn_dev->platform_fs.entrys[i].flag = flag;

		mode |= (0x0600);

		WCN_INFO("entry name is %s type 0x%x addr: 0x%p\n",
			 wcn_dev->platform_fs.entrys[i].name,
			 wcn_dev->platform_fs.entrys[i].flag,
			 &wcn_dev->platform_fs.entrys[i]);

		wcn_dev->platform_fs.entrys[i].platform_proc_dir_entry =
			proc_create_data(
				wcn_dev->platform_fs.entrys[i].name,
				mode,
				wcn_dev->platform_fs.platform_proc_dir_entry,
				&wcn_platform_fs_fops,
				&wcn_dev->platform_fs.entrys[i]);
		wcn_dev->platform_fs.entrys[i].wcn_dev = wcn_dev;
	}
}

static inline void wcn_platform_fs_exit(struct wcn_device *wcn_dev)
{
	u8 i = 0;

	for (i = 0; i < MAX_PLATFORM_ENTRY_NUM; i++) {
		if (!wcn_dev->platform_fs.entrys[i].name)
			break;

		if (wcn_dev->platform_fs.entrys[i].flag != 0) {
			remove_proc_entry(
				wcn_dev->platform_fs.entrys[i].name,
				wcn_dev->platform_fs.platform_proc_dir_entry
				);
		}
	}

	remove_proc_entry(wcn_dev->name, NULL);
}

static void wcn_subdts_init(void)
{
	int ret;

	if (s_wcn_device.btwf_device == NULL) {
		WCN_ERR("s_wcn_device.btwf_device is NULL!\n");
		return;
	}

	if (s_wcn_device.btwf_device->dev == NULL) {
		WCN_ERR("s_wcn_device.btwf_device->dev is NULL!\n");
		return;
	}

	WCN_INFO("%s start, s_wcn_device.btwf_device->name = %s\n", __func__, s_wcn_device.btwf_device->name);
	ret = devm_of_platform_populate(s_wcn_device.btwf_device->dev);
	if (ret)
		WCN_ERR("init BTWF subdts error\n");
}

/* wcn triggers power on and off by itself in probe */
static void wcn_probe_power_wq(struct work_struct *work)
{
	WCN_INFO("%s start itself\n", __func__);
	if (start_marlin(MARLIN_MDBG)) {
		WCN_ERR("%s power on failed\n", __func__);
		return;
	}

	/* BTWF SYS calibration time consumption is about 250 ms */
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		wcn_reset_mdbg_notifier_init();
		msleep(WCCN_BTWF_CALIBRATION_TIME);
	}

	if (stop_marlin(MARLIN_MDBG))
		WCN_ERR("%s power down failed\n", __func__);

	wcn_subdts_init();
}

int wcn_probe(struct platform_device *pdev)
{
	struct wcn_device *wcn_dev;
	static int first = 1;

	WCN_INFO("%s start!\n", __func__);

	wcn_dev = kzalloc(sizeof(*wcn_dev), GFP_KERNEL);
	if (!wcn_dev)
		return -ENOMEM;

	wcn_dev->dev = &pdev->dev;
	if (wcn_parse_dt(pdev, wcn_dev) < 0) {
		WCN_ERR("wcn_parse_dt Failed!\n");
		kfree(wcn_dev);
		return -EINVAL;
	}

	/* init the regs which can be init in the driver probe */
	wcn_config_ctrlreg(wcn_dev, 0, wcn_dev->ctrl_probe_num);

	mutex_init(&wcn_dev->power_lock);

	wcn_platform_fs_init(wcn_dev);

	platform_set_drvdata(pdev, (void *)wcn_dev);

	if (strcmp(wcn_dev->name, WCN_MARLIN_DEV_NAME) == 0)
		s_wcn_device.btwf_device = wcn_dev;
	else if (strcmp(wcn_dev->name, WCN_GNSS_DEV_NAME) == 0)
		s_wcn_device.gnss_device = wcn_dev;

	if (!s_wcn_device.vddcon_voltage_setted) {
		s_wcn_device.vddcon_voltage_setted = true;
		/* default vddcon is 1.6V, we should set it to 1.2v */
		/* marlin3-integ default vddcon is 1.2vset to 1.2v */
		wcn_power_set_vddcon(WCN_VDDCON_WORK_VOLTAGE);
		mutex_init(&s_wcn_device.vddwcn_lock);
	}
	if (wcn_dev->need_dcxo1v8 &&
		!s_wcn_device.dcxo1v8_voltage_setted) {
		s_wcn_device.dcxo1v8_voltage_setted = true;
		/* marlin3-integ default dcxo1v8 is 3v set to 1.8v */
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
			wcn_power_set_dcxo1v8(WCN_DCXO1V8_WORK_VOLTAGE);
			mutex_init(&s_wcn_device.dcxo1v8_lock);
		}
	}

	if (strcmp(wcn_dev->name, WCN_MARLIN_DEV_NAME) == 0) {
		mutex_init(&wcn_dev->vddwifipa_lock);
		if (wcn_platform_chip_id() == AON_CHIP_ID_AA)
			wcn_power_set_vddwifipa(WCN_VDDWIFIPA_WORK_VOLTAGE);
		wcn_global_source_init();
		/* register ops */
		wcn_bus_init();
		/* sipc preinit */
		sprdwcn_bus_preinit();

		proc_fs_init();
		log_dev_init();
		wcn_gnss_dump_init();

		init_wcn_sysfs();
		mdbg_atcmd_owner_init();
		if (wcn_dev->need_sync_efuse)
			wcn_marlin_write_efuse();
		loopcheck_init();
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
			wcn_dfs_status_clear();
	} else if (strcmp(wcn_dev->name, WCN_GNSS_DEV_NAME) == 0) {
		gnss_write_efuse_data();
	}

	INIT_DELAYED_WORK(&wcn_dev->power_wq, wcn_power_wq);
	INIT_DELAYED_WORK(&wcn_dev->probe_power_wq, wcn_probe_power_wq);
	INIT_WORK(&wcn_dev->firmware_init_wq, wcn_firmware_init_wq);
	if (first) {
		/* Transceiver can't get into LP, so force deep sleep */
		if ((wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKLE) ||
		    (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKL3)) {
			wcn_sys_soft_release();
			wcn_sys_deep_sleep_en();
		}
		first = 0;
	} else {
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
			schedule_delayed_work(&wcn_dev->probe_power_wq,
						msecs_to_jiffies(1000));
		else
			schedule_delayed_work(&wcn_dev->probe_power_wq,
						msecs_to_jiffies(1500));
	}

#if WCN_INTEGRATE_PLATFORM_DEBUG
	wcn_codes_debug();
#endif

	WCN_INFO("%s finish!\n", __func__);

	return 0;
}

int wcn_remove(struct platform_device *pdev)
{
	struct wcn_device *wcn_dev = platform_get_drvdata(pdev);

	if (!wcn_dev) {
		WCN_ERR("dev is NULL!\n");
		return -ENODEV;
	}

	WCN_INFO("%s dev name %s\n", __func__, wcn_dev->name);

	cancel_delayed_work_sync(&wcn_dev->power_wq);
	cancel_delayed_work_sync(&wcn_dev->probe_power_wq);
	cancel_work_sync(&wcn_dev->firmware_init_wq);
	if (wcn_dev_is_marlin(wcn_dev)) {
		loopcheck_deinit();
		mdbg_atcmd_owner_deinit();
		wcn_gnss_dump_exit();
		log_dev_exit();
		proc_fs_exit();
		wcn_bus_deinit();
		exit_wcn_sysfs();
	}

	wcn_platform_fs_exit(wcn_dev);
	kfree(wcn_dev);

	return 0;
}

void wcn_shutdown(struct platform_device *pdev)
{
	struct wcn_device *wcn_dev = platform_get_drvdata(pdev);

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		WCN_INFO("%s WCN A-DIE powerdown\n", __func__);
		wcn_sys_power_clock_unsupport(true);
		return;
	}

	if (wcn_dev && wcn_dev->wcn_open_status) {
		/* CPU hold on */
		wcn_proc_native_stop(wcn_dev);
		/* wifipa power off */
		if (strcmp(wcn_dev->name, WCN_MARLIN_DEV_NAME) == 0) {
			wcn_marlin_power_enable_vddwifipa(false);
			/* ASIC: disable vddcon, wifipa interval time > 1ms */
			usleep_range(VDDWIFIPA_VDDCON_MIN_INTERVAL_TIME,
				     VDDWIFIPA_VDDCON_MAX_INTERVAL_TIME);
		}
		/* vddcon power off */
		wcn_power_enable_vddcon(false);
		/* dcxo1v8 power off*/
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
			wcn_power_enable_dcxo1v8(false);
		wcn_sys_soft_reset();
		wcn_sys_soft_release();
		wcn_sys_deep_sleep_en();
		WCN_INFO("dev name %s\n", wcn_dev->name);
	}
}

#if 0
static SIMPLE_DEV_PM_OPS(wcn_pm_ops, wcn_suspend, wcn_resume);
static struct platform_driver wcn_driver = {
	.driver = {
		.name = "wcn_integrate_platform",
		.pm = &wcn_pm_ops,
		.of_match_table = wcn_match_table,
	},
	.probe = wcn_probe,
	.remove = wcn_remove,
	.shutdown = wcn_shutdown,
};

static int __init wcn_init(void)
{
	WCN_INFO("entry!\n");
	return platform_driver_register(&wcn_driver);
}
late_initcall(wcn_init);

static void __exit wcn_exit(void)
{
	platform_driver_unregister(&wcn_driver);
}
module_exit(wcn_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum  WCN Integrate Platform Driver");
MODULE_AUTHOR("YaoGuang Chen <yaoguang.chen@spreadtrum.com>");
#endif
