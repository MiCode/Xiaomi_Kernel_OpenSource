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

#ifndef __WCN_INTEGRATE_PLATFORM_H__
#define __WCN_INTEGRATE_PLATFORM_H__
#include <linux/regmap.h>

#define FALSE								(0)
#define TRUE								(1)

/* The name should be set the same as DTS */
#define WCN_MARLIN_DEV_NAME "wcn_btwf"
#define WCN_GNSS_DEV_NAME "wcn_gnss"

/*
 * ASIC: enable or disable vddwifipa and vddcon,
 * the interval time should more than 1ms.
 */
#define VDDWIFIPA_VDDCON_MIN_INTERVAL_TIME	(10000)	/* us */
#define VDDWIFIPA_VDDCON_MAX_INTERVAL_TIME	(30000)	/* us */

enum wcn_marlin_sub_sys {
	WCN_MARLIN_BLUETOOTH = 0,
	WCN_MARLIN_FM,
	WCN_MARLIN_WIFI,
	WCN_MARLIN_MDBG = 6,
	WCN_MARLIN_ALL = 7,
};

enum wcn_gnss_sub_sys {
	/*
	 * The value is different with wcn_marlin_sub_sys
	 * Or the start interface can't distinguish
	 * Marlin or GNSS
	 */
	WCN_GNSS = 16,
	WCN_GNSS_BD,
	WCN_GNSS_GAL,
	WCN_GNSS_ALL,
};

enum wcn_gnss_type {
	WCN_GNSS_TYPE_INVALID,
	WCN_GNSS_TYPE_GL,
	WCN_GNSS_TYPE_BD,
};

enum wcn_aon_chip_id {
	WCN_AON_CHIP_ID_INVALID,
	WCN_SHARKLE_CHIP_AA_OR_AB,
	WCN_SHARKLE_CHIP_AC,
	WCN_SHARKLE_CHIP_AD,
	WCN_PIKE2_CHIP,
	WCN_PIKE2_CHIP_AA,
	WCN_PIKE2_CHIP_AB,
	WCN_SHARKL6_CHIP,
	WCN_SHARKL3_CHIP,
	WCN_SHARKL3_CHIP_22NM,
};

#define FIRMWARE_FILEPATHNAME_LENGTH_MAX 256
#define WCN_MARLIN_MASK 0xcf /* Base on wcn_marlin_sub_sys */
#define WCN_MARLIN_BTWIFI_MASK 0x05
#define WCN_GNSS_MASK BIT(WCN_GNSS)
#define WCN_GNSS_BD_MASK BIT(WCN_GNSS_BD)
#define WCN_GNSS_GAL_MASK BIT(WCN_GNSS_GAL)
#define WCN_GNSS_ALL_MASK (WCN_GNSS_MASK | WCN_GNSS_BD_MASK | WCN_GNSS_GAL_MASK)

#define WCN_POWERUP_WAIT_MS	30000 /*time out in waiting wifi to come up*/

#define WCN_AON_CHIP_ID0 0x00E0
#define WCN_AON_CHIP_ID1 0x00E4
#define WCN_AON_PLATFORM_ID0 0x00E8
#define WCN_AON_PLATFORM_ID1 0x00EC
#define WCN_AON_CHIP_ID 0x00FC
#define WCN_AON_VERSION_ID 0x00F8
#define WCN_AON_MANUFACTURE_ID 0x00F4

#define PIKE2_CHIP_ID0 0x32000000	/* 2 */
#define PIKE2_CHIP_ID1 0x50696B65	/* Pike */
#define SHARKLE_CHIP_ID0 0x6B4C4500	/* kle */
#define SHARKLE_CHIP_ID1 0x53686172	/* Shar */
#define SHARKL3_CHIP_ID0 0x6B4C3300	/* kl3 */
#define SHARKL3_CHIP_ID1 0x53686172	/* Shar */

#define QOGIRL6_CHIP_ID0 0x724C3600	/* rL6 */
#define QOGIRL6_CHIP_ID1 0x516F6769	/* Qogi */

#define AON_CHIP_ID_AA 0x96360000
#define AON_CHIP_ID_AC 0x96360002

enum {
	WCN_PLATFORM_TYPE_SHARKLE,
	WCN_PLATFORM_TYPE_PIKE2,
	WCN_PLATFORM_TYPE_SHARKL3,
	WCN_PLATFORM_TYPE_QOGIRL6,
	WCN_PLATFORM_TYPE,
};

struct platform_chip_id {
	u32 aon_chip_id0;
	u32 aon_chip_id1;
	u32 aon_platform_id0;
	u32 aon_platform_id1;
	u32 aon_chip_id;
};

enum {
	WCN_POWER_STATUS_OFF = 0,
	WCN_POWER_STATUS_ON,
};

typedef void (*gnss_dump_callback) (void);
extern gnss_dump_callback gnss_dump_handle;

void mdbg_dump_gnss_register(
			gnss_dump_callback callback_func, void *para);
void mdbg_dump_gnss_unregister(void);

int start_integrate_wcn(u32 subsys);
int stop_integrate_wcn(u32 subsys);
int start_marlin(u32 subsys);
int stop_marlin(u32 subsys);
int wcn_get_gnss_power_status(void);
int wcn_get_btwf_power_status(void);
bool wcn_get_download_status(void);
int wcn_get_module_status(void);
int wcn_get_module_status_changed(void);
int marlin_reset_register_notify(void *callback_func, void *para);
int marlin_reset_unregister_notify(void);
phys_addr_t wcn_get_btwf_base_addr(void);
phys_addr_t wcn_get_btwf_sleep_addr(void);
phys_addr_t wcn_get_btwf_init_status_addr(void);
void mdbg_assert_interface(char *str);
void wcn_regmap_raw_write_bit(struct regmap *cur_regmap,
			      u32 reg,
			      unsigned int val);
void wcn_regmap_read(struct regmap *cur_regmap,
		     u32 reg,
		     unsigned int *val);
struct regmap *wcn_get_btwf_regmap(u32 regmap_type);
struct regmap *wcn_get_gnss_regmap(u32 regmap_type);
phys_addr_t wcn_get_gnss_base_addr(void);
u32 wcn_get_cp2_comm_rx_count(void);
u32 wcn_platform_chip_type(void);
int wcn_write_data_to_phy_addr(phys_addr_t phy_addr,
			       void *src_data, u32 size);
int wcn_read_data_from_phy_addr(phys_addr_t phy_addr,
				void *tar_data, u32 size);
void *wcn_mem_ram_vmap_nocache(phys_addr_t start, size_t size,
			       unsigned int *count);
void wcn_mem_ram_unmap(const void *mem, unsigned int count);
enum wcn_aon_chip_id wcn_get_aon_chip_id(void);
void wcn_device_poweroff(void);
char *gnss_firmware_path_get(void);

#endif
