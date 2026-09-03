/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * Filename : wcn_integrate_platform.h
 * Abstract : This file is a implementation for driver of integrated marlin:
 *                The marlin chip and GNSS chip were integrated with AP chipset.
 *
 * Authors	: yaoguang.chen
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __WCN_INTEGRATE_H__
#define __WCN_INTEGRATE_H__
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
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <misc/wcn_integrate_platform.h>
#include "linux/sipc.h"

#include "wcn_integrate_dev.h"

#define REGMAP_UPDATE_BITS_ENABLE 0	/* It can't work well. */

#define MDBG_CACHE_FLAG_VALUE	(0xcdcddcdc)

#define PD_WCN_AUTO_SHUTDOWN_EN BIT(24)
#define PD_WCN_FORCE_SHUTDOWN BIT(25)

#define WCN_REG_SET_OFFSET 0x1000
#define WCN_REG_CLEAR_OFFSET 0x2000

/* power domain reg */
#define PD_WCN_CFG0_OFFSET 0x100
#define PD_WCN_CFG0_OFFSET_PIKE2 0x50
#define PD_WCN_CFG0_OFFSET_QOGIRL6 0x3a8

#define WCN_BTWF_CPU_RESET 1
#define WCN_BTWF_CPU_RESET_RELEASE 2

struct wcn_chip_type {
	u32 chipid;
	enum wcn_aon_chip_id chiptype;
};

struct marlin_special_share_mem {
	u32 init_status;
	u32 loopcheck_cnt;
};

struct gnss_special_share_mem {
	u32 calibration_flag;
	u32 efuse[GNSS_EFUSE_BLOCK_COUNT];
};

struct wifi_special_share_mem {
	struct wifi_calibration calibration_data;
	u32 efuse[WIFI_EFUSE_BLOCK_COUNT];
	u32 calibration_flag;
};

/* gnss cpu view(ddr 6m start) */
#define WCN_GNSS_SPECIAL_SHARME_MEM_ADDR	(0x001ffc00)
struct wcn_gnss_special_share_mem {
	phys_addr_t sync_base_addr;
	phys_addr_t init_status_phy_addr;
	phys_addr_t gnss_ddr_offset;
	phys_addr_t gnss_efuse_value;
	phys_addr_t cali_status;
	phys_addr_t gnss_test;
};

struct wcn_dfs_sync_info {
	union {
		struct {
			/* btwf */
			u32 btwf_record_gnss_current_clk:4;
			u32 btwf_pwr_state:1;
			u32 btwf_dfs_init:1;
			u32 btwf_dfs_active:1;
			u32 btwf_spinlock:1;
			u32 reserved_btwf:24;
		};
		u32	btwf_dfs_info;
	};
	union {
		struct {
			/* gnss */
			u32 gnss_clk_req_ack:5;
			u32 gnss_pwr_state:1;
			u32 gnss_dfs_active:1;
			u32 gnss_spinlock:1;
			u32 reserved_gnss:24;
		};
		u32	gnss_dfs_info;
	};
	u32 debugdfs0;
	u32 debugdfs1;
};

#define WCN_GNSS_DDR_OFFSET (0x600000)
#define WCN_SYS_DFS_SYNC_ADDR_OFFSET (0x007ffb00)
#define WCN_SYS_RFI_SYNC_ADDR_OFFSET (0x007ffb10)

#define QOGIRL6_WCN_SPECIAL_SHARME_MEM_ADDR	(0x007fdc00)
struct qogirl6_wcn_special_share_mem {
	/* 0x007fdc00 */
	struct marlin_special_share_mem marlin;
	/* 0x007fdc08 */
	u32 gnss_flag_addr;
	/* 0x007fdc0c */
	u32 include_gnss;
	/* 0x007fdc10 */
	u32 cp2_sleep_status;
	/* 0x007fdc14 */
	u32 sleep_flag_addr;
	/* 0x007fdc18 */
	u32 efuse_temper_magic;
	/* 0x007fdc1c */
	u32 efuse_temper_val;
	/* 0x007fdc20 */
	/* use for wifi */
	u32 efuse[WIFI_EFUSE_BLOCK_COUNT];
	struct wifi_special_share_mem wifi;
};

#define WCN_SPECIAL_SHARME_MEM_ADDR	(0x0017c000)
struct wcn_special_share_mem {
	/* 0x17c000 */
	struct wifi_special_share_mem wifi;
	/* 0x17cf54 */
	struct marlin_special_share_mem marlin;
	/* 0x17cf5c */
	u32 include_gnss;
	/* 0x17cf60 */
	u32 gnss_flag_addr;
	/* 0x17cf64 */
	u32 cp2_sleep_status;
	/* 0x17cf68 */
	u32 sleep_flag_addr;
	/* 0x17cf6c */
	u32 efuse_temper_magic;
	/* 0x17cf70 */
	u32 efuse_temper_val;
	/* 0x17cf74 */
	struct gnss_special_share_mem gnss;
	u32 efuse[WIFI_EFUSE_BLOCK_COUNT];
};

extern struct platform_chip_id g_platform_chip_id;
extern char integ_functionmask[8];
extern struct qogirl6_wcn_special_share_mem *qogirl6_s_wssm_phy_offset_p;
extern struct wcn_special_share_mem *s_wssm_phy_offset_p;
extern struct wcn_gnss_special_share_mem s_wcngnss_sync_addr;
extern int ge2_bin_type;

void wcn_dfs_poweroff_state_clear(struct wcn_device *wcn_dev);
void wcn_dfs_poweroff_shutdown_clear(struct wcn_device *wcn_dev);
void wcn_dfs_poweron_status_clear(struct wcn_device *wcn_dev);
void wcn_dfs_status_clear(void);
void wcn_rfi_status_clear(void);
u32 wcn_platform_chip_id(void);
u32 wcn_platform_chip_type(void);
u32 wcn_get_cp2_comm_rx_count(void);
phys_addr_t wcn_get_btwf_base_addr(void);
phys_addr_t wcn_get_btwf_sleep_addr(void);
phys_addr_t wcn_get_btwf_init_status_addr(void);
int wcn_get_btwf_power_status(void);
void wcn_regmap_read(struct regmap *cur_regmap,
		     u32 reg,
		     unsigned int *val);
void wcn_regmap_raw_write_bit(struct regmap *cur_regmap,
			      u32 reg,
			      unsigned int val);
struct regmap *wcn_get_btwf_regmap(u32 regmap_type);
struct regmap *wcn_get_gnss_regmap(u32 regmap_type);
phys_addr_t wcn_get_apcp_sync_addr(struct wcn_device *wcn_dev);
phys_addr_t wcn_get_gnss_base_addr(void);
bool wcn_get_download_status(void);
void wcn_set_download_status(bool status);
u32 gnss_get_boot_status(void);
void gnss_set_boot_status(u32 status);
int marlin_get_module_status(void);
int wcn_get_module_status_changed(void);
void wcn_set_module_status_changed(bool status);
int marlin_reset_register_notify(void *callback_func, void *para);
int marlin_reset_unregister_notify(void);
void wcn_set_module_state(bool status);
void wcn_set_loopcheck_state(bool status);
void wcn_set_apcp_sync_addr(struct wcn_device *wcn_dev);
int wcn_send_force_sleep_cmd(struct wcn_device *wcn_dev);
u32 wcn_get_sleep_status(struct wcn_device *wcn_dev, int force_sleep);
u32 wcn_subsys_shutdown_status(struct wcn_device *wcn_dev);
u32 wcn_shutdown_status(struct wcn_device *wcn_dev);
u32 wcn_deep_sleep_status(struct wcn_device *wcn_dev);
int btwf_force_deepsleep(void);
int gnss_force_deepsleep(void);
u32 wcn_subsys_active_num(void);
bool wcn_subsys_active_is_gnss_only(void);
void wcn_set_auto_shutdown(struct wcn_device *wcn_dev);
void wcn_power_domain_set(struct wcn_device *wcn_dev, u32 set_type);
void wcn_xtl_auto_sel(bool enable);
int wcn_power_enable_sys_domain(bool enable);
int wcn_power_enable_merlion_domain(bool enable);
void wcn_sys_soft_reset(void);
void wcn_sys_ctrl_26m(bool enable);
void wcn_clock_ctrl(bool enable);
void wcn_sys_soft_release(void);
void wcn_sys_deep_sleep_en(void);
void wcn_power_set_vddcon(u32 value);
void wcn_power_set_dcxo1v8(u32 value);
int wcn_power_enable_vddcon(bool enable);
int wcn_power_enable_dcxo1v8(bool enable);
void wcn_power_set_vddwifipa(u32 value);
int wcn_marlin_power_enable_vddwifipa(bool enable);
bool wcn_power_status_check(struct wcn_device *wcn_dev);
u32 wcn_parse_platform_chip_id(struct wcn_device *wcn_dev);
void mdbg_hold_cpu(void);
void mdbg_cpu_reset(void);
enum wcn_aon_chip_id wcn_get_aon_chip_id(void);
const char *wcn_get_chip_name(void);
void wcn_merlion_power_control(bool enable);

enum wcn_clock_mode integ_wcn_get_xtal_26m_clk_mode(void);
void integ_wcn_chip_power_off(void);
int integ_wcn_get_module_status_changed(void);
void integ_wcn_set_module_status_changed(bool status);
int integ_marlin_get_module_status(void);
int start_integ_marlin(u32 subsys);
int stop_integ_marlin(u32 subsys);
int wcn_check_2to1_bin(struct wcn_device *wcn_dev, const struct firmware *firmware, loff_t *off);
#endif
