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
#include <misc/wcn_integrate_platform.h>
#include "wcn_glb.h"
#include "wcn_glb_reg.h"
#include "wcn_misc.h"
#include "wcn_procfs.h"
#include "../include/wcn_dbg.h"

#define WCN_CLK_EN 0xc0
#define WCN_LDO_EN 0x1000
#define WCN_FASTCHARGE_EN 0x100

#define MARLIN_USE_FORCE_SHUTDOWN	(0xabcd250)
#define MARLIN_FORCE_SHUTDOWN_OK	(0x6B6B6B6B)
#define BTWF_SW_DEEP_SLEEP_MAGIC	(0x504C5344) /* SW deep sleep:DSLP */

static int wcn_chiptype;
static int wcn_open_module;
static int wcn_module_state_change;
/* format: marlin2-built-in_id0_id1 */
static char wcn_chip_name[40];
char integ_functionmask[8];
struct platform_chip_id g_platform_chip_id;
static u32 g_platform_chip_type;
static const struct wcn_chip_type wcn_chip_type[] = {
	/* WCN_SHARKL3_CHIP and WCN_SHARKL3_CHIP_22NM is the same */
	{0x98550000, WCN_SHARKL3_CHIP},
	{0x96360000, WCN_SHARKLE_CHIP_AA_OR_AB},
	{0x96360002, WCN_SHARKLE_CHIP_AC},
	{0x96360003, WCN_SHARKLE_CHIP_AD},
	/* WCN_PIKE2_CHIP_AA and WCN_PIKE2_CHIP_AB is the same */
	{0x96330000, WCN_PIKE2_CHIP},
	/* WCN_SHARKL6_CHIP is error */
	{0x00000000, WCN_SHARKL6_CHIP},
	{0x00000001, WCN_SHARKL6_CHIP},
};

struct qogirl6_wcn_special_share_mem *qogirl6_s_wssm_phy_offset_p =
	(struct qogirl6_wcn_special_share_mem *)QOGIRL6_WCN_SPECIAL_SHARME_MEM_ADDR;

struct wcn_special_share_mem *s_wssm_phy_offset_p =
	(struct wcn_special_share_mem *)WCN_SPECIAL_SHARME_MEM_ADDR;

struct wcn_gnss_special_share_mem s_wcngnss_sync_addr = {
	WCN_GNSS_SPECIAL_SHARME_MEM_ADDR,
	0x34, /* init_status_phy_addr */
	WCN_GNSS_DDR_OFFSET,
	0x08, /* gnss_efuse_value */
};

static void wcn_dfs_status_show(struct wcn_dfs_sync_info *dfs_info)
{
	WCN_INFO("btwf_record_gnss_current_clk %d\n",
		 dfs_info->btwf_record_gnss_current_clk);
	WCN_INFO("btwf_pwr_state %d\n", dfs_info->btwf_pwr_state);
	WCN_INFO("btwf_dfs_init %d\n", dfs_info->btwf_dfs_init);
	WCN_INFO("btwf_dfs_active %d\n", dfs_info->btwf_dfs_active);
	WCN_INFO("btwf_spinlock %d\n", dfs_info->btwf_spinlock);

	WCN_INFO("gnss_clk_req_ack %d\n",
		 dfs_info->gnss_clk_req_ack);
	WCN_INFO("gnss_pwr_state %d\n", dfs_info->gnss_pwr_state);
	WCN_INFO("gnss_dfs_active %d\n", dfs_info->gnss_dfs_active);
	WCN_INFO("gnss_spinlock %d\n", dfs_info->gnss_spinlock);
	WCN_INFO("wcn_dfs_info: 0x%x-0x%x-0x%x-0x%x\n",
		dfs_info->btwf_dfs_info, dfs_info->gnss_dfs_info,
		dfs_info->debugdfs0, dfs_info->debugdfs1);
}

void wcn_dfs_poweroff_state_clear(struct wcn_device *wcn_dev)
{
	bool is_marlin;
	phys_addr_t phy_addr;
	struct wcn_dfs_sync_info dfs_info, dfs_info1;
	u32 temp_btwf = 0, temp_gnss = 0;

	is_marlin = wcn_dev_is_marlin(wcn_dev);
	if (is_marlin) {
		phy_addr = wcn_dev->base_addr + WCN_SYS_DFS_SYNC_ADDR_OFFSET;
		wcn_read_data_from_phy_addr(phy_addr, &dfs_info,
			sizeof(struct wcn_dfs_sync_info));
		temp_btwf = dfs_info.btwf_dfs_info;
		temp_gnss = dfs_info.gnss_dfs_info;
		dfs_info.btwf_pwr_state = 0;
		wcn_write_data_to_phy_addr(phy_addr, &dfs_info.btwf_dfs_info,
								   sizeof(u32));
		wcn_read_data_from_phy_addr(phy_addr, &dfs_info1,
			sizeof(struct wcn_dfs_sync_info));
		WCN_INFO("poweroff_state before btwf clear : 0x%x-0x%x\n",
			temp_btwf, temp_gnss);
		wcn_dfs_status_show(&dfs_info);
		WCN_INFO("poweroff_state after btwf clear:\n");
		wcn_dfs_status_show(&dfs_info1);
	} else {
		phy_addr = wcn_dev->base_addr - WCN_GNSS_DDR_OFFSET
					+ WCN_SYS_DFS_SYNC_ADDR_OFFSET;
		wcn_read_data_from_phy_addr(phy_addr, &dfs_info,
			sizeof(struct wcn_dfs_sync_info));
		temp_btwf = dfs_info.btwf_dfs_info;
		temp_gnss = dfs_info.gnss_dfs_info;
		dfs_info.gnss_pwr_state = 0;
		wcn_write_data_to_phy_addr(phy_addr + sizeof(u32),
			&dfs_info.gnss_dfs_info, sizeof(u32));
		wcn_read_data_from_phy_addr(phy_addr, &dfs_info1,
			sizeof(struct wcn_dfs_sync_info));
		WCN_INFO("poweroff_state before gnss clear: 0x%x-0x%x\n",
			temp_btwf, temp_gnss);
		wcn_dfs_status_show(&dfs_info);
		WCN_INFO("poweroff_state after gnss clear:\n");
		wcn_dfs_status_show(&dfs_info1);
	}
}
void wcn_dfs_poweroff_shutdown_clear(struct wcn_device *wcn_dev)
{
	bool is_marlin;
	phys_addr_t phy_addr;
	struct wcn_dfs_sync_info dfs_info, dfs_info1;
	u32 temp_btwf = 0, temp_gnss = 0;

	is_marlin = wcn_dev_is_marlin(wcn_dev);
	if (is_marlin) {
		phy_addr = wcn_dev->base_addr + WCN_SYS_DFS_SYNC_ADDR_OFFSET;
		wcn_read_data_from_phy_addr(phy_addr, &dfs_info,
			sizeof(struct wcn_dfs_sync_info));
		temp_btwf = dfs_info.btwf_dfs_info;
		temp_gnss = dfs_info.gnss_dfs_info;

		dfs_info.btwf_record_gnss_current_clk = 0;
		dfs_info.btwf_dfs_init = 0;
		dfs_info.btwf_dfs_active = 0;
		dfs_info.btwf_spinlock = 0;
		wcn_write_data_to_phy_addr(phy_addr, &dfs_info.btwf_dfs_info,
								   sizeof(u32));
		wcn_read_data_from_phy_addr(phy_addr, &dfs_info1,
			sizeof(struct wcn_dfs_sync_info));
		WCN_INFO("poweroff_shutdown before btwf clear: 0x%x-0x%x\n",
			temp_btwf, temp_gnss);
		wcn_dfs_status_show(&dfs_info);
		WCN_INFO("poweroff_shutdown after btwf clear:\n");
		wcn_dfs_status_show(&dfs_info1);
		/* reset spinlock */
	} else {
		phy_addr = wcn_dev->base_addr - WCN_GNSS_DDR_OFFSET
					+ WCN_SYS_DFS_SYNC_ADDR_OFFSET;
		wcn_read_data_from_phy_addr(phy_addr, &dfs_info,
			sizeof(struct wcn_dfs_sync_info));
		temp_btwf = dfs_info.btwf_dfs_info;
		temp_gnss = dfs_info.gnss_dfs_info;

		dfs_info.gnss_dfs_active = 0;
		dfs_info.gnss_spinlock = 0;
		dfs_info.gnss_clk_req_ack = 0;
		wcn_write_data_to_phy_addr(phy_addr + sizeof(u32),
			&dfs_info.gnss_dfs_info, sizeof(u32));
		wcn_read_data_from_phy_addr(phy_addr, &dfs_info,
			sizeof(struct wcn_dfs_sync_info));
		WCN_INFO("poweroff_shutdown before gnss clear: 0x%x-0x%x\n",
			temp_btwf, temp_gnss);
		WCN_INFO("poweroff_shutdown after gnss clear:\n");
		wcn_dfs_status_show(&dfs_info);
		/* reset spinlock */
	}
}

void wcn_dfs_poweron_status_clear(struct wcn_device *wcn_dev)
{
	bool is_marlin;
	phys_addr_t phy_addr;
	struct wcn_dfs_sync_info dfs_info, dfs_info1;
	u32 temp_btwf = 0, temp_gnss = 0;

	is_marlin = wcn_dev_is_marlin(wcn_dev);
	if (is_marlin) {
		phy_addr = wcn_dev->base_addr + WCN_SYS_DFS_SYNC_ADDR_OFFSET;
		wcn_read_data_from_phy_addr(phy_addr, &dfs_info,
			sizeof(struct wcn_dfs_sync_info));
		temp_btwf = dfs_info.btwf_dfs_info;
		temp_gnss = dfs_info.gnss_dfs_info;

		dfs_info.btwf_record_gnss_current_clk = 0;
		dfs_info.btwf_pwr_state = 0;
		dfs_info.btwf_dfs_init = 0;
		dfs_info.btwf_dfs_active = 0;
		dfs_info.btwf_spinlock = 0;
		wcn_write_data_to_phy_addr(phy_addr, &dfs_info.btwf_dfs_info,
								   sizeof(u32));
		wcn_read_data_from_phy_addr(phy_addr, &dfs_info1,
			sizeof(struct wcn_dfs_sync_info));
		WCN_INFO("poweron before btwf clear: 0x%x-0x%x\n",
			temp_btwf, temp_gnss);
		wcn_dfs_status_show(&dfs_info);
		WCN_INFO("poweron after btwf clear:\n");
		wcn_dfs_status_show(&dfs_info1);
	} else {
		phy_addr = wcn_dev->base_addr - WCN_GNSS_DDR_OFFSET
					+ WCN_SYS_DFS_SYNC_ADDR_OFFSET;
		wcn_read_data_from_phy_addr(phy_addr, &dfs_info,
			sizeof(struct wcn_dfs_sync_info));
		temp_btwf = dfs_info.btwf_dfs_info;
		temp_gnss = dfs_info.gnss_dfs_info;

		dfs_info.gnss_pwr_state = 0;
		dfs_info.gnss_dfs_active = 0;
		dfs_info.gnss_spinlock = 0;
		dfs_info.gnss_clk_req_ack = 0;
		wcn_write_data_to_phy_addr(phy_addr + sizeof(u32),
			&dfs_info.gnss_dfs_info, sizeof(u32));
		wcn_read_data_from_phy_addr(phy_addr, &dfs_info1,
			sizeof(struct wcn_dfs_sync_info));
		WCN_INFO("poweron before gnss clear before: 0x%x-0x%x\n",
			temp_btwf, temp_gnss);
		wcn_dfs_status_show(&dfs_info);
		WCN_INFO("poweron after gnss clear:\n");
		wcn_dfs_status_show(&dfs_info1);
	}
}

void wcn_dfs_status_clear(void)
{
	struct wcn_device *wcn_dev = s_wcn_device.btwf_device;
	phys_addr_t phy_addr;
	struct wcn_dfs_sync_info dfs_info;

	dfs_info.btwf_dfs_info = 0;
	dfs_info.gnss_dfs_info = 0;

	phy_addr = wcn_dev->base_addr + WCN_SYS_DFS_SYNC_ADDR_OFFSET;
	wcn_write_data_to_phy_addr(phy_addr, &dfs_info,
		sizeof(struct wcn_dfs_sync_info));
	WCN_INFO("first boot clear dfs status :\n");
	wcn_dfs_status_show(&dfs_info);
}

/*
 * BTWF,GNSS SYS will initialize RFI and set the status
 * After BTWF,GNSS both close, AP SYS clear them.
 */
void wcn_rfi_status_clear(void)
{
	struct wcn_device *wcn_dev = s_wcn_device.btwf_device;
	phys_addr_t phy_addr;
	u32 status;
	u32 clear_value = 0;

	phy_addr = wcn_dev->base_addr + WCN_SYS_RFI_SYNC_ADDR_OFFSET;
	wcn_read_data_from_phy_addr(phy_addr, &status, sizeof(u32));
	wcn_write_data_to_phy_addr(phy_addr, &clear_value, sizeof(u32));
	WCN_INFO("[-]%s:status=%d\n", __func__, status);
}

enum wcn_aon_chip_id wcn_get_aon_chip_id(void)
{
	u32 aon_chip_id;
	u32 version_id, manufacture_id;
	u32 i;
	struct regmap *regmap;

	if (unlikely(!s_wcn_device.btwf_device))
		return WCN_AON_CHIP_ID_INVALID;

	regmap = wcn_get_btwf_regmap(REGMAP_AON_APB);
	wcn_regmap_read(s_wcn_device.btwf_device->rmap[REGMAP_AON_APB],
			WCN_AON_CHIP_ID, &aon_chip_id);
	WCN_INFO("aon_chip_id=0x%08x\n", aon_chip_id);
	for (i = 0; i < ARRAY_SIZE(wcn_chip_type); i++) {
		if (wcn_chip_type[i].chipid == aon_chip_id) {
			if (wcn_chip_type[i].chiptype == WCN_SHARKL3_CHIP) {
				wcn_chiptype = 1;
				wcn_regmap_read(regmap, WCN_AON_MANUFACTURE_ID,
						&manufacture_id);
				WCN_INFO("manufacture_id=0x%08x\n", manufacture_id);
				/* manufacture_id:
				 * 0x800 for 28NM
				 * 0xA00 for 22NM
				 */
				return (manufacture_id == 0xA00) ?
					WCN_SHARKL3_CHIP_22NM : WCN_SHARKL3_CHIP;
			}

			if (wcn_chip_type[i].chiptype == WCN_SHARKLE_CHIP_AA_OR_AB)
				return wcn_chip_type[i].chiptype;

			if (wcn_chip_type[i].chiptype == WCN_SHARKLE_CHIP_AC)
				return wcn_chip_type[i].chiptype;

			if (wcn_chip_type[i].chiptype == WCN_SHARKLE_CHIP_AD)
				return wcn_chip_type[i].chiptype;

			if (wcn_chip_type[i].chiptype == WCN_SHARKLE_CHIP_AD)
				return wcn_chip_type[i].chiptype;

			if (wcn_chip_type[i].chiptype == WCN_PIKE2_CHIP) {
				wcn_regmap_read(regmap, WCN_AON_VERSION_ID,
						&version_id);
				WCN_INFO("aon_version_id=0x%08x\n", version_id);
				/* version_id:
				 * 0 for WCN_PIKE2_CHIP_AA
				 * others for WCN_PIKE2_CHIP_AB
				 */
				return (version_id == 0) ? WCN_PIKE2_CHIP_AA : WCN_PIKE2_CHIP_AB;
			}

			if (wcn_chip_type[i].chiptype == WCN_SHARKL6_CHIP) {
				wcn_regmap_read(regmap, WCN_AON_VERSION_ID,
						&version_id);
				WCN_INFO("aon_version_id=0x%08x\n", version_id);
				return wcn_chip_type[i].chiptype;
			}
		}
	}

	WCN_ERR("wcn get chipid invalid\n");
	return WCN_AON_CHIP_ID_INVALID;
}
EXPORT_SYMBOL_GPL(wcn_get_aon_chip_id);

#define WCN_WFBT_LOAD_FIRMWARE_OFFSET 0x180000
#define GNSS_LOAD_FIRMWARE_OFFSET 0xA2800
int wcn_check_2to1_bin(struct wcn_device *wcn_dev, const struct firmware *firmware, loff_t *off)
{
	int ret;

	if (wcn_dev_is_gnss(wcn_dev) == 1) {
		WCN_INFO("gnss bin--------\r\n");
		if (ge2_bin_type == 18) {
			WCN_INFO("gnss for L3 use GAl gnss bin\n");
			*off = GNSS_LOAD_FIRMWARE_OFFSET;
			ret = 1;
		} else {
			*off = 0;
			WCN_INFO("gnss for L3 use GE2 gnss bin\n");
			ret = 0;
		}
	} else {
		WCN_INFO("btwf bin--------\r\n");
		if (wcn_get_aon_chip_id() == WCN_SHARKL3_CHIP_22NM) {
			WCN_INFO("it is sharkl3 22nm\n");
			*off = WCN_WFBT_LOAD_FIRMWARE_OFFSET;
		} else {
			*off = 0;
			WCN_INFO("it is not sharkl3 22nm\n");
		}

		if (wcn_chiptype == 1) {
			WCN_INFO("wcn for L3 use 2to1 btwf bin\n");
			ret = 2;
		} else {
			WCN_INFO("not L3,wcn not use 2to1 btwf bin\n");
			ret = 0;
		}
	}
	return ret;
}

u32 wcn_platform_chip_id(void)
{
	return g_platform_chip_id.aon_chip_id;
}

u32 wcn_platform_chip_type(void)
{
	return g_platform_chip_type;
}
EXPORT_SYMBOL_GPL(wcn_platform_chip_type);

u32 wcn_get_cp2_comm_rx_count(void)
{
	u32 rx_count = 0;
	phys_addr_t phy_addr;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		phy_addr = s_wcn_device.btwf_device->base_addr +
			(phys_addr_t)&qogirl6_s_wssm_phy_offset_p->marlin.loopcheck_cnt;
	} else {
		phy_addr = s_wcn_device.btwf_device->base_addr +
			(phys_addr_t)&s_wssm_phy_offset_p->marlin.loopcheck_cnt;
	}
	wcn_read_data_from_phy_addr(phy_addr,
				    &rx_count, sizeof(u32));
	//WCN_INFO("cp2 comm rx count :%d\n", rx_count);

	return rx_count;
}

phys_addr_t wcn_get_btwf_base_addr(void)
{
	return s_wcn_device.btwf_device->base_addr;
}

int wcn_get_btwf_power_status(void)
{
	WCN_INFO("btwf_device power_state:%d\n",
		 s_wcn_device.btwf_device->power_state);
	return s_wcn_device.btwf_device->power_state;
}

int wcn_get_gnss_power_status(void)
{
	WCN_INFO("gnss_device power_state:%d\n",
		 s_wcn_device.gnss_device->power_state);
	return s_wcn_device.gnss_device->power_state;
}

int integ_marlin_get_power(void)
{
	if (s_wcn_device.gnss_device &&
	    s_wcn_device.gnss_device->power_state)
		return 1;

	if (s_wcn_device.btwf_device &&
	    s_wcn_device.btwf_device->power_state)
		return 1;

	return 0;
}

/* for qogirl6 */
phys_addr_t wcn_get_apcp_sync_addr(struct wcn_device *wcn_dev)
{
	WCN_INFO("apcp_sync_addr:%llu\n", wcn_dev->apcp_sync_addr);

	return wcn_dev->apcp_sync_addr;
}

void wcn_set_apcp_sync_addr(struct wcn_device *wcn_dev)
{
	if (strcmp(wcn_dev->name, WCN_MARLIN_DEV_NAME) == 0) {
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
			qogirl6_s_wssm_phy_offset_p =
			(struct qogirl6_wcn_special_share_mem *)wcn_dev->apcp_sync_addr;
		} else {
			s_wssm_phy_offset_p =
			(struct wcn_special_share_mem *)wcn_dev->apcp_sync_addr;
		}
	} else
		s_wcngnss_sync_addr.sync_base_addr = wcn_dev->apcp_sync_addr;
	WCN_INFO("wcn_dev->apcp_sync_addr:%llu\n", wcn_dev->apcp_sync_addr);
}

phys_addr_t wcn_get_btwf_init_status_addr(void)
{
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		return s_wcn_device.btwf_device->base_addr +
			   (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->marlin.init_status;
	} else {
		return s_wcn_device.btwf_device->base_addr +
			   (phys_addr_t)&s_wssm_phy_offset_p->marlin.init_status;
	}
}

phys_addr_t wcn_get_btwf_sleep_addr(void)
{
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		return s_wcn_device.btwf_device->base_addr +
			   (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->cp2_sleep_status;
	} else {
		return s_wcn_device.btwf_device->base_addr +
			   (phys_addr_t)&s_wssm_phy_offset_p->cp2_sleep_status;
	}
}

struct regmap *wcn_get_btwf_regmap(u32 regmap_type)
{
	return s_wcn_device.btwf_device->rmap[regmap_type];
}

struct regmap *wcn_get_gnss_regmap(u32 regmap_type)
{
	return s_wcn_device.gnss_device->rmap[regmap_type];
}
EXPORT_SYMBOL_GPL(wcn_get_gnss_regmap);

phys_addr_t wcn_get_gnss_base_addr(void)
{
	return s_wcn_device.gnss_device->base_addr;
}
EXPORT_SYMBOL_GPL(wcn_get_gnss_base_addr);

bool wcn_get_download_status(void)
{
	return s_wcn_device.btwf_device->download_status;
}

void wcn_set_download_status(bool status)
{
	s_wcn_device.btwf_device->download_status = status;
}

enum wcn_clock_type integ_wcn_get_xtal_26m_clk_type(void)
{
	return s_wcn_device.clk_xtal_26m.type;
}

enum wcn_clock_mode integ_wcn_get_xtal_26m_clk_mode(void)
{
	return s_wcn_device.clk_xtal_26m.mode;
}

u32 gnss_get_boot_status(void)
{
	return s_wcn_device.gnss_device->boot_cp_status;
}

void gnss_set_boot_status(u32 status)
{
	s_wcn_device.gnss_device->boot_cp_status = status;
}

int integ_wcn_get_module_status_changed(void)
{
	return wcn_module_state_change;
}

void integ_wcn_set_module_status_changed(bool status)
{
	wcn_module_state_change = status;
}

int integ_marlin_get_module_status(void)
{
	return wcn_open_module;
}
EXPORT_SYMBOL_GPL(integ_marlin_get_module_status);

void wcn_set_module_state(bool status)
{
	if (s_wcn_device.btwf_device->wcn_open_status & WCN_MARLIN_MASK)
		wcn_open_module = 1;
	else
		wcn_open_module = 0;
	wcn_set_download_status(status);
	WCN_INFO("cp2 power status:%d\n", status);
}

void wcn_set_loopcheck_state(bool status)
{
	wcn_set_module_status_changed(true);
	if (status) {
		loopcheck_ready_set();
		start_loopcheck();
	} else if (!(s_wcn_device.btwf_device->wcn_open_status &
		WCN_MARLIN_MASK)) {
		stop_loopcheck();
	}
	wakeup_loopcheck_int();
}

#if REGMAP_UPDATE_BITS_ENABLE
static void wcn_regmap_update_bit(struct wcn_device *ctrl,
				  u32 index,
				  u32 mask,
				  u32 val)
{
	u32 type;
	u32 reg;
	int ret;

	type = ctrl->ctrl_type[index];
	reg = ctrl->ctrl_reg[index];

	ret = regmap_update_bits(ctrl->rmap[type],
				 reg,
				 mask,
				 val);
	if (ret)
		WCN_ERR("regmap_update_bits ret=%d\n", ret);
}

static void wcn_regmap_write_bit(struct wcn_device *ctrl,
				 u32 index,
				 u32 mask,
				 u32 val)
{
	u32 type;
	u32 reg;
	int ret;

	type = ctrl->ctrl_type[index];
	reg = ctrl->ctrl_reg[index];

	ret = regmap_write_bits(ctrl->rmap[type],
				reg,
				mask,
				val);
	if (ret)
		WCN_ERR("regmap_write_bits ret=%d\n", ret);
}
#endif

void wcn_regmap_raw_write_bit(struct regmap *cur_regmap,
			      u32 reg, unsigned int val)
{
	int ret;

	ret = regmap_write(cur_regmap, reg, (u32)val);
	if (ret)
		WCN_ERR("regmap_raw_write ret=%d\n", ret);
}
EXPORT_SYMBOL_GPL(wcn_regmap_raw_write_bit);

/* addr_offset:some REGs has twice group, one read and another write */
void wcn_regmap_read(struct regmap *cur_regmap,
		     u32 reg,
		     unsigned int *val)
{
	(void)regmap_read(cur_regmap, reg, val);
}
EXPORT_SYMBOL_GPL(wcn_regmap_read);

/* return val: 1 for send the cmd to CP2 */
int wcn_send_force_sleep_cmd(struct wcn_device *wcn_dev)
{
	u32 val = 0;
	phys_addr_t phy_addr;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		phy_addr = wcn_dev->base_addr +
			   (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->sleep_flag_addr;
	} else {
		phy_addr = wcn_dev->base_addr +
			   (phys_addr_t)&s_wssm_phy_offset_p->sleep_flag_addr;
	}
	wcn_read_data_from_phy_addr(phy_addr, &val, sizeof(val));
	if  (val == MARLIN_USE_FORCE_SHUTDOWN) {
		mdbg_send("at+sleep_switch=2\r",
			  strlen("at+sleep_switch=2\r"), MDBG_SUBTYPE_AT);
		WCN_INFO("send sleep_switch=2\n");
		return 1;
	}

	return 0;
}

/*
 * WCN SYS include BTWF and GNSS sys, ret: 0 is sleep, else is not
 * force_sleep: 0 for old way, others for send CP2 shutdown cmd way.
 */
u32 wcn_get_sleep_status(struct wcn_device *wcn_dev, int force_sleep)
{
	u32 sleep_status = 0;
	u32 wcn_sleep_status_mask = 0xf000;
	u32 val = 0;
	phys_addr_t phy_addr;

	/* QogirL6 project
	 *
	 * GNSS SYS doesn't use SW sleep
	 * but return sleep status to do no wait
	 */
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		if (wcn_dev_is_marlin(wcn_dev)) {
			phy_addr = wcn_dev->base_addr +
				   (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->sleep_flag_addr;
			wcn_read_data_from_phy_addr(phy_addr, &val, sizeof(val));
			WCN_INFO("cp2 sw sleep status:0x%x\n", val);
			/* SW entry deep sleep */
			if (val != BTWF_SW_DEEP_SLEEP_MAGIC)
				return 1;
			else
				return 0;
		} else
			return 0;
	}

	if (wcn_dev_is_marlin(wcn_dev) && force_sleep) {
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
			phy_addr = wcn_dev->base_addr +
				   (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->cp2_sleep_status;
		} else {
			phy_addr = wcn_dev->base_addr +
				   (phys_addr_t)&s_wssm_phy_offset_p->cp2_sleep_status;
		}
		wcn_read_data_from_phy_addr(phy_addr, &val, sizeof(val));
		WCN_INFO("Sleep status flag:0x%x\n", val);
		if (val == MARLIN_FORCE_SHUTDOWN_OK) {
			usleep_range(10000, 12000);
			return 0;
		}
		return 1;
	}

	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
			0xd4, &sleep_status);

	return (sleep_status & wcn_sleep_status_mask);
}

/*
 * WCN SYS include BTWF and GNSS sys, ret: 1 is shutdown, else is not
 * please help confirm that the SHUTDOWN REGs can be read.
 * WARNING:This is for QogirL6 chip only now.
 */
u32 wcn_subsys_shutdown_status(struct wcn_device *wcn_dev)
{
	u32 shutdown_status = 0;
	u32 shutdown_mask = 0;
	u32 btwf_shutdown_mask = (0x1<<8);
	u32 gnss_shutdown_mask = (0x1<<14);

	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x03b0, &shutdown_status);
	if (wcn_dev_is_marlin(wcn_dev))
		shutdown_mask = btwf_shutdown_mask;
	else
		shutdown_mask = gnss_shutdown_mask;

	WCN_INFO("subsys shutdown:0x4080c3b0=0x%x\n", shutdown_status);

	return (shutdown_status & shutdown_mask);
}
/*
 * NOTES:This is for QogirL6 chip.
 * becare:after forcedeep, wcn sys maybe enter deepsleep,
 * so if want to  access WCN REGs after this step, please
 * confirm WCN SYS doesn't enter deep sleep.
 */
int btwf_force_deepsleep(void)
{
	u32 reg_val = 0;
	struct wcn_device *wcn_dev = s_wcn_device.btwf_device;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev && wcn_dev->rmap[REGMAP_WCN_AON_APB]) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
						0x0098, &reg_val);
		WCN_INFO("%s:reg0x4080c098=0x%x\n", __func__, reg_val);
		reg_val |= (0x1<<3);
		wcn_regmap_raw_write_bit(
				wcn_dev->rmap[REGMAP_WCN_AON_APB],
				0x0098, reg_val);
		return 0;
	}

	WCN_ERR("%s:wcn_dev=0x%p\n", __func__, wcn_dev);
	return -EINVAL;
}
/* NOTES:This is for QogirL6 chip.
 * This function shuold be called after WCN wake up
 */
int gnss_force_deepsleep(void)
{
	u32 reg_val = 0;
	struct wcn_device *wcn_dev = s_wcn_device.gnss_device;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev && wcn_dev->rmap[REGMAP_WCN_AON_APB]) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
						0x00c8, &reg_val);
		WCN_INFO("%s:reg0x4080c0c8=0x%x\n", __func__, reg_val);
		reg_val |= (0x1<<3);
		wcn_regmap_raw_write_bit(
				wcn_dev->rmap[REGMAP_WCN_AON_APB],
				0x00c8, reg_val);
		msleep(1);
		wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
						0x00c8, &reg_val);
		WCN_INFO("%s:reg0x4080c0c8=0x%x\n", __func__, reg_val);
		return 0;
	}

	WCN_ERR("%s:wcn_dev=0x%p\n", __func__, wcn_dev);
	return -EINVAL;
}

u32 wcn_subsys_active_num(void)
{
	u32 count = 0;

	if (s_wcn_device.btwf_device &&
	    s_wcn_device.btwf_device->wcn_open_status & WCN_MARLIN_MASK)
		count++;

	if (s_wcn_device.gnss_device &&
	    s_wcn_device.gnss_device->wcn_open_status & WCN_GNSS_ALL_MASK)
		count++;

	WCN_INFO("%s, %d", __func__, count);
	return count;
}

bool wcn_subsys_active_is_gnss_only(void)
{
	if (s_wcn_device.btwf_device &&
	    s_wcn_device.btwf_device->wcn_open_status & WCN_MARLIN_MASK)
		return false;

	if (s_wcn_device.gnss_device &&
	    s_wcn_device.gnss_device->wcn_open_status & WCN_GNSS_ALL_MASK)
		return true;

	return false;
}

/*
 * WCN SYS shutdown, ret: 1 is shutdown, else is not
 * If WCN is at shutdown status, it can't access WCN REGs.
 * WARNING:This is for QogirL6 chip until now.
 */
u32 wcn_shutdown_status(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);

	/* WCN shutdown */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				 0x0538, &reg_val);
	WCN_INFO("wcn shutdown reg0x0538 =0x%x!\n", reg_val);
	if (reg_val == (0x7<<24)) {
		WCN_INFO("wcn is shutdown!\n");
		return true;
	}

	WCN_INFO("wcn isn't shutdown!\n");
	return false;
}

void wcn_set_auto_shutdown(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				 0x03a8, &reg_val);
	WCN_INFO("%s:before set, val=0x%x!\n", __func__, reg_val);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_PMU_APB],
			0x13a8, 0x1<<24);
	msleep(1);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				 0x03a8, &reg_val);
	WCN_INFO("%s:after set, val=0x%x!\n", __func__, reg_val);
}

/*
 * WCN SYS sleep, ret: 1 is deep sleep, else is not
 * deepsleep status can't access WCN REGs
 * WARNING:This is for QogirL6 chip until now.
 */
u32 wcn_deep_sleep_status(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	/* WCN sleep,wakeup */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				 0x0860, &reg_val);
	WCN_INFO("wcn sleep status =0x%x!\n", reg_val);
	if ((reg_val & 0xF0000000) == 0) {
		WCN_INFO("wcn is deep sleep!\n");
		return 1;
	} else if ((reg_val & 0xF0000000) == (0x6<<28)) {
		WCN_INFO("wcn is wakeup!\n");
		return 0;
	}

	return 1;
}

void wcn_power_domain_set(struct wcn_device *wcn_dev, u32 set_type)
{
	u32 offset0 = 0, offset1 = 0;
	u32 bitmap0 = 0, bitmap1 = 0;

	bitmap0 = PD_WCN_AUTO_SHUTDOWN_EN;
	bitmap1 = PD_WCN_FORCE_SHUTDOWN;
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_PIKE2) {
		if (set_type == 1) {
			offset0 = WCN_REG_CLEAR_OFFSET +
					PD_WCN_CFG0_OFFSET_PIKE2;
			offset1 = WCN_REG_SET_OFFSET +
					PD_WCN_CFG0_OFFSET_PIKE2;
		} else {
			offset0 = WCN_REG_CLEAR_OFFSET +
					PD_WCN_CFG0_OFFSET_PIKE2;
			offset1 = WCN_REG_CLEAR_OFFSET +
					PD_WCN_CFG0_OFFSET_PIKE2;
		}
	} else if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		if (set_type == 1) { /* Close:set force shutdown */
			offset0 = WCN_REG_CLEAR_OFFSET +
					PD_WCN_CFG0_OFFSET_QOGIRL6;
			offset1 = WCN_REG_SET_OFFSET +
					PD_WCN_CFG0_OFFSET_QOGIRL6;
		} else { /* Open: Auto, Force shutdown clear */
			offset0 = WCN_REG_CLEAR_OFFSET +
					PD_WCN_CFG0_OFFSET_QOGIRL6;
			offset1 = WCN_REG_CLEAR_OFFSET +
					PD_WCN_CFG0_OFFSET_QOGIRL6;
		}
	} else {
		if (set_type == 1) {
			offset0 = WCN_REG_CLEAR_OFFSET +
					PD_WCN_CFG0_OFFSET;
			offset1 = WCN_REG_SET_OFFSET +
					PD_WCN_CFG0_OFFSET;
		} else {
			offset0 = WCN_REG_CLEAR_OFFSET +
					PD_WCN_CFG0_OFFSET;
			offset1 = WCN_REG_CLEAR_OFFSET +
					PD_WCN_CFG0_OFFSET;
		}
	}
	WCN_DBG("offset0:0x%x bitmap0:0x%x\n",
		offset0, bitmap0);
	WCN_DBG("offset1:0x%x bitmap1:0x%x\n",
		offset1, bitmap1);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_PMU_APB],
				 offset0, bitmap0);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_PMU_APB],
				 offset1, bitmap1);
}

void wcn_xtl_auto_sel(bool enable)
{
	struct regmap *regmap;
	u32 value;

	regmap = wcn_get_btwf_regmap(REGMAP_PMU_APB);
	wcn_regmap_read(regmap, 0x338, &value);

	if (enable) {
		value |= 1 << 4;
		wcn_regmap_raw_write_bit(regmap, 0x338, value);
	} else {
		value &= ~(1 << 4);
		wcn_regmap_raw_write_bit(regmap, 0X338, value);
	}
}

int wcn_power_enable_merlion_domain(bool enable)
{
	u32 btwf_open = false;
	u32 gnss_open = false;
	static u32 merlion_domain;

	if (!s_wcn_device.btwf_device) {
		WCN_ERR("dev is NULL\n");
		return -ENODEV;
	}

	if (s_wcn_device.btwf_device->wcn_open_status & WCN_MARLIN_MASK)
		btwf_open = true;
	if (s_wcn_device.gnss_device &&
	    s_wcn_device.gnss_device->wcn_open_status & WCN_GNSS_ALL_MASK)
		gnss_open = true;

	if (enable && !merlion_domain) {
		wcn_merlion_power_control(true);
		merlion_domain = true;
		WCN_INFO("clear WCN SYS TOP PD\n");
	} else if ((!btwf_open) && (!gnss_open) && merlion_domain) {
		wcn_merlion_power_control(false);
		merlion_domain = false;
		WCN_INFO("set WCN SYS TOP PD\n");
	}
	WCN_INFO("enable = %d, btwf_open=%d, gnss_open=%d\n",
		 enable, btwf_open, gnss_open);

	return 0;
}

int wcn_power_enable_sys_domain(bool enable)
{
	int ret = 0;
	u32 btwf_open = false;
	u32 gnss_open = false;
	static u32 sys_domain;

	if (!s_wcn_device.btwf_device) {
		WCN_ERR("dev is NULL\n");
		return -ENODEV;
	}

	if (s_wcn_device.btwf_device &&
	    s_wcn_device.btwf_device->wcn_open_status & WCN_MARLIN_MASK)
		btwf_open = true;
	if (s_wcn_device.gnss_device &&
	    s_wcn_device.gnss_device->wcn_open_status & WCN_GNSS_ALL_MASK)
		gnss_open = true;

	if (enable && !sys_domain) {
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_PIKE2)
			wcn_xtl_auto_sel(false);
		wcn_power_domain_set(s_wcn_device.btwf_device, 0);
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_PIKE2)
			wcn_xtl_auto_sel(true);
		sys_domain = true;
		WCN_INFO("clear WCN SYS TOP PD\n");
	} else if ((!btwf_open) && (!gnss_open) && sys_domain) {
		if (wcn_platform_chip_type() ==
				WCN_PLATFORM_TYPE_PIKE2)
			wcn_xtl_auto_sel(false);

		wcn_power_domain_set(s_wcn_device.btwf_device, 1);

		sys_domain = false;
		WCN_INFO("set WCN SYS TOP PD\n");
	}
	WCN_INFO("enable = %d, ret = %d, btwf_open=%d, gnss_open=%d\n",
		 enable, ret, btwf_open, gnss_open);

	return ret;
}

#define WCN_CP_SOFT_RST_MIN_TIME (5000)	/* us */
#define WCN_CP_SOFT_RST_MAX_TIME (6000)	/* us */

/*
 * wcn_sys_soft_reset was used by BTWF and GNSS together
 * both BTWF and GNSS not work, we should set it.
 */
void wcn_sys_soft_reset(void)
{
	u32 offset;
	u32 bitmap;
	u32 btwf_open = false;
	u32 gnss_open = false;
	u32 valid_chip = true;
	struct wcn_device *wcn_dev;

	wcn_dev = s_wcn_device.btwf_device ?
		  s_wcn_device.btwf_device : s_wcn_device.gnss_device;
	if (!wcn_dev)
		return;

	if (s_wcn_device.btwf_device &&
	    s_wcn_device.btwf_device->wcn_open_status)
		btwf_open = true;
	if (s_wcn_device.gnss_device &&
	    s_wcn_device.gnss_device->wcn_open_status)
		gnss_open = true;

	if (!btwf_open && !gnss_open) {
		if (wcn_platform_chip_type() ==
		    WCN_PLATFORM_TYPE_PIKE2) {
			bitmap = 1 << 7;
			offset  = 0X10b0;
		} else if (wcn_platform_chip_type() ==
			   WCN_PLATFORM_TYPE_SHARKLE) {
			bitmap = 1 << 9;
			offset  = 0X10b0;
		} else if (wcn_platform_chip_type() ==
			   WCN_PLATFORM_TYPE_SHARKL3) {
			bitmap = 1 << 16;
			offset  = 0X10b0;
		} else if (wcn_platform_chip_type() ==
			   WCN_PLATFORM_TYPE_QOGIRL6) {
			bitmap = 1 << 20;
			offset  = 0X1B98;
		} else {
			valid_chip = false;
			WCN_ERR("chip type err\n");
		}

		if (valid_chip)
			wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_PMU_APB],
						 offset, bitmap);
		WCN_INFO("%s finish\n", __func__);
		usleep_range(WCN_CP_SOFT_RST_MIN_TIME,
			     WCN_CP_SOFT_RST_MAX_TIME);
	}
}

void wcn_sys_ctrl_26m(bool enable)
{
	struct regmap *regmap;
	u32 value;

	regmap = wcn_get_btwf_regmap(REGMAP_ANLG_PHY_G6);
	wcn_regmap_read(regmap, 0x28, &value);

	if (enable)
		value &= ~(1 << 2);
	else
		value |= 1 << 2;

	wcn_regmap_raw_write_bit(regmap, 0X28, value);
}

void wcn_clock_ctrl(bool enable)
{
	struct regmap *regmap;
	u32 value;

	regmap = wcn_get_btwf_regmap(REGMAP_ANLG_PHY_G5);
	if (IS_ERR(regmap)) {
		WCN_ERR("failed to get REGMAP_ANLG_PHY_G5\n");
		return;
	}
	if (enable) {
		value = WCN_LDO_EN;
		wcn_regmap_raw_write_bit(regmap, 0x1044, value);
		value = WCN_FASTCHARGE_EN;
		wcn_regmap_raw_write_bit(regmap, 0x1044, value);
		usleep_range(10, 20);
		wcn_regmap_raw_write_bit(regmap, 0x2044, value);
		value = WCN_CLK_EN;
		wcn_regmap_raw_write_bit(regmap, 0x1044, value);
	} else {
		value = WCN_CLK_EN;
		wcn_regmap_raw_write_bit(regmap, 0x2044, value);
		value = WCN_LDO_EN;
		wcn_regmap_raw_write_bit(regmap, 0x2044, value);
	}
}

/*
 * wcn_sys_soft_release was used by BTWF and GNSS together
 * both BTWF and GNSS not work, we should set it.
 */
void wcn_sys_soft_release(void)
{
	u32 offset;
	u32 bitmap;
	u32 btwf_open = false;
	u32 gnss_open = false;
	struct wcn_device *wcn_dev;

	wcn_dev = s_wcn_device.btwf_device ?
		  s_wcn_device.btwf_device : s_wcn_device.gnss_device;
	if (!wcn_dev)
		return;

	if (s_wcn_device.btwf_device &&
	    s_wcn_device.btwf_device->wcn_open_status)
		btwf_open = true;
	if (s_wcn_device.gnss_device &&
	    s_wcn_device.gnss_device->wcn_open_status)
		gnss_open = true;

	if (!btwf_open && !gnss_open) {
		if (wcn_platform_chip_type() ==
		    WCN_PLATFORM_TYPE_PIKE2) {
			bitmap = 1 << 7;
			offset  = 0X20b0;
		} else if (wcn_platform_chip_type() ==
			   WCN_PLATFORM_TYPE_SHARKLE) {
			bitmap = 1 << 9;
			offset  = 0X20b0;
		} else if (wcn_platform_chip_type() ==
			   WCN_PLATFORM_TYPE_SHARKL3) {
			bitmap = 1 << 16;
			offset  = 0X20b0;
		} else if (wcn_platform_chip_type() ==
			   WCN_PLATFORM_TYPE_QOGIRL6) {
			bitmap = 1 << 20;
			offset  = 0X2B98;
		} else {
			WCN_ERR("chip type err\n");
			return;
		}
		wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_PMU_APB],
					 offset, bitmap);
		WCN_DBG("%s finish!\n", __func__);
		usleep_range(WCN_CP_SOFT_RST_MIN_TIME,
			     WCN_CP_SOFT_RST_MAX_TIME);
	}
}

/*
 * wcn_sys_deep_sleep_en was used by BTWF and GNSS together
 * both BTWF and GNSS not work, we should set it.
 */
void wcn_sys_deep_sleep_en(void)
{
	struct regmap *rmap = NULL;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		if (s_wcn_device.btwf_device) {
			rmap = s_wcn_device.btwf_device->rmap[REGMAP_PMU_APB];
		} else if (s_wcn_device.gnss_device) {
			rmap = s_wcn_device.gnss_device->rmap[REGMAP_PMU_APB];
		} else {
			WCN_ERR("no devices\n");
			return;
		}
		wcn_regmap_raw_write_bit(rmap, 0x178C, 1 << 0);
	} else if (wcn_platform_chip_type() != WCN_PLATFORM_TYPE_PIKE2) {
		if (s_wcn_device.btwf_device) {
			rmap = s_wcn_device.btwf_device->rmap[REGMAP_PMU_APB];
		} else if (s_wcn_device.gnss_device) {
			rmap = s_wcn_device.gnss_device->rmap[REGMAP_PMU_APB];
		} else {
			WCN_ERR("no devices\n");
			return;
		}
		wcn_regmap_raw_write_bit(rmap, 0x1244, 1 << 0);
	}
		WCN_INFO("%s finish!\n", __func__);
}

void wcn_power_set_vddcon(u32 value)
{
	int ret = 0;
	if (s_wcn_device.vddwcn)
		ret = regulator_set_voltage(s_wcn_device.vddwcn,
					    value, value);
	WCN_INFO("%s ret = %d\n", __func__, ret);
}

void wcn_power_set_dcxo1v8(u32 value)
{
	int ret = 0;
	if (s_wcn_device.dcxo1v8)
		ret = regulator_set_voltage(s_wcn_device.dcxo1v8,
					    value, value);
	WCN_INFO("%s ret = %d\n", __func__, ret);
}

/*
 * NOTES:regulator function has compute-counter
 * We needn't judge GNSS and BTWF coxist case now.
 * But we should reserve the open status to debug.
 */
int wcn_power_enable_vddcon(bool enable)
{
	int ret = 0;
	u32 btwf_open = false;
	u32 gnss_open = false;

	if (s_wcn_device.btwf_device &&
	    s_wcn_device.btwf_device->wcn_open_status & WCN_MARLIN_MASK)
		btwf_open = true;
	if (s_wcn_device.gnss_device &&
	    s_wcn_device.gnss_device->wcn_open_status & WCN_GNSS_ALL_MASK)
		gnss_open = true;

	mutex_lock(&s_wcn_device.vddwcn_lock);
	if (s_wcn_device.vddwcn) {
		if (enable) {
			ret = regulator_enable(s_wcn_device.vddwcn);
			s_wcn_device.vddwcn_en_count++;
			if (wcn_platform_chip_type() ==
				WCN_PLATFORM_TYPE_SHARKLE)
				wcn_sys_ctrl_26m(true);
			if (wcn_platform_chip_type() ==
			    WCN_PLATFORM_TYPE_SHARKL3)
				wcn_clock_ctrl(true);
		} else if (regulator_is_enabled(s_wcn_device.vddwcn)) {
			ret = regulator_disable(s_wcn_device.vddwcn);
			s_wcn_device.vddwcn_en_count--;
			if ((wcn_platform_chip_type() ==
				WCN_PLATFORM_TYPE_SHARKLE) &&
				s_wcn_device.vddwcn_en_count == 0) {
				wcn_sys_ctrl_26m(false);
			}
			if ((wcn_platform_chip_type() ==
			    WCN_PLATFORM_TYPE_SHARKL3) &&
			    s_wcn_device.vddwcn_en_count == 0) {
				wcn_clock_ctrl(false);
			}
		}

		WCN_INFO("enable=%d,en_count=%d,ret=%d,btwf=%d,gnss=%d\n",
			 enable, s_wcn_device.vddwcn_en_count,
			 ret, btwf_open, gnss_open);
		if (s_wcn_device.vddwcn_en_count > 2 ||
		    s_wcn_device.vddwcn_en_count < 0)
			WCN_ERR("vddwcn_en_count=%d",
				s_wcn_device.vddwcn_en_count);
	}
	mutex_unlock(&s_wcn_device.vddwcn_lock);

	return ret;
}

int wcn_power_enable_dcxo1v8(bool enable)
{
	int ret = 0;
	u32 btwf_open = false;
	u32 gnss_open = false;

	if (s_wcn_device.btwf_device &&
	    s_wcn_device.btwf_device->wcn_open_status & WCN_MARLIN_MASK)
		btwf_open = true;
	if (s_wcn_device.gnss_device &&
	    s_wcn_device.gnss_device->wcn_open_status & WCN_GNSS_ALL_MASK)
		gnss_open = true;

	mutex_lock(&s_wcn_device.dcxo1v8_lock);
	if (s_wcn_device.dcxo1v8) {
		if (enable) {
			ret = regulator_enable(s_wcn_device.dcxo1v8);
			s_wcn_device.dcxo1v8_en_count++;
		} else if (regulator_is_enabled(s_wcn_device.dcxo1v8)) {
			ret = regulator_disable(s_wcn_device.dcxo1v8);
			s_wcn_device.dcxo1v8_en_count--;
			}
		}

	WCN_INFO("enable=%d,en_count=%d,ret=%d,btwf=%d,gnss=%d\n",
			 enable, s_wcn_device.dcxo1v8_en_count,
			 ret, btwf_open, gnss_open);
	if (s_wcn_device.dcxo1v8_en_count > 2 ||
		s_wcn_device.dcxo1v8_en_count < 0)
		WCN_ERR("vddwcn_en_count=%d", s_wcn_device.dcxo1v8_en_count);

	mutex_unlock(&s_wcn_device.dcxo1v8_lock);

	return ret;
}

/* The VDDCON default value is 1.6V, we should set it to 1.2v */
void wcn_power_set_vddwifipa(u32 value)
{
	struct wcn_device *btwf_device = s_wcn_device.btwf_device;
	int ret = 0;

	if (btwf_device->vddwifipa)
		ret = regulator_set_voltage(btwf_device->vddwifipa,
					    value, value);
	WCN_INFO("%s ret = %d\n", __func__, ret);
	WCN_INFO("vddwifipa value %d\n", value);
}

/* NOTES: wifipa: only used by WIFI module */
int wcn_marlin_power_enable_vddwifipa(bool enable)
{
	int ret = 0;
	struct wcn_device *btwf_device = s_wcn_device.btwf_device;

	mutex_lock(&btwf_device->vddwifipa_lock);
	if (btwf_device->vddwifipa) {
		if (enable)
			ret = regulator_enable(btwf_device->vddwifipa);
		else if (regulator_is_enabled(btwf_device->vddwifipa))
			ret = regulator_disable(btwf_device->vddwifipa);

		WCN_INFO("enable = %d, ret = %d\n", enable, ret);
	}
	mutex_unlock(&btwf_device->vddwifipa_lock);

	return ret;
}

/* QogirL6:Before read/write WCN SUB SYS, should confirm
 * the SUB SYS is at wake up status, or maybe operate fail.
 */
bool wcn_power_status_check(struct wcn_device *wcn_dev)
{
	bool wcn_pd_state = false;
	u32 wcn_pd_top_state = 0;
	u32 wcn_pd_subsys_state = 0;
	u32 reg_value = 0;

	/* PWR_STATUS_DBG_18 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
					0x538, &reg_value);
	WCN_INFO("PWR_STATUS_DBG_18:0x%x\n", reg_value);
	/* bit 24-28:default 7-power off, 0-power on */
	wcn_pd_top_state = reg_value & 0x1f000000;
	if (wcn_pd_top_state != 0)
		return false;

	/* TOP PMU: wcn deep */
	if (wcn_deep_sleep_status(wcn_dev)) {
		WCN_INFO("%s is deep\n", __func__);
		return false;
	}

	reg_value = 0;
	/* WCN PMU:sub sys power on */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
					0x3b0, &reg_value);
	WCN_INFO("PD_SLP_STATUS:REG0x4080c3b0=0x%x\n", reg_value);
	/* btwf sys poweron finish */
	if (wcn_dev == s_wcn_device.btwf_device)
		wcn_pd_subsys_state = reg_value & BIT(0);
	/* gnss sys poweron finish */
	else
		wcn_pd_subsys_state = reg_value & BIT(6);
	if (wcn_pd_subsys_state == 0)
		return false;

	/* BTWF SYS: wake up finish */
	if (wcn_dev_is_marlin(wcn_dev)) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
						0x3b4, &reg_value);
		WCN_INFO("BTWF SYS Wake up,Reg0x4080c3b4=0x%x\n", reg_value);
		/* btwf sys wake up finish */
		if ((reg_value & (0xF<<12)) == (0x6 << 12))
			wcn_pd_state = true;
		else
			wcn_pd_state = false;
	} else { /* GNSS SYS: wake up finish */
		wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
						0x3c0, &reg_value);
		WCN_INFO("GNSS SYS Wake up,Reg0x4080c3c0=0x%x\n", reg_value);
		/* btwf sys wake up finish */
		if ((reg_value & (0xF<<9)) == (0x6 << 9))
			wcn_pd_state = true;
		else
			wcn_pd_state = false;
	}

	return wcn_pd_state;
}

u32 wcn_parse_platform_chip_id(struct wcn_device *wcn_dev)
{
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
			WCN_AON_CHIP_ID0,
			&g_platform_chip_id.aon_chip_id0);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
			WCN_AON_CHIP_ID1,
			&g_platform_chip_id.aon_chip_id1);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
			WCN_AON_PLATFORM_ID0,
			&g_platform_chip_id.aon_platform_id0);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
			WCN_AON_PLATFORM_ID1,
			&g_platform_chip_id.aon_platform_id1);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
			WCN_AON_CHIP_ID,
			&g_platform_chip_id.aon_chip_id);

	if (g_platform_chip_id.aon_chip_id0 == PIKE2_CHIP_ID0 &&
	    g_platform_chip_id.aon_chip_id1 == PIKE2_CHIP_ID1)
		g_platform_chip_type = WCN_PLATFORM_TYPE_PIKE2;
	else if (g_platform_chip_id.aon_chip_id0 == SHARKLE_CHIP_ID0 &&
		 g_platform_chip_id.aon_chip_id1 == SHARKLE_CHIP_ID1)
		g_platform_chip_type = WCN_PLATFORM_TYPE_SHARKLE;
	else if (g_platform_chip_id.aon_chip_id0 == SHARKL3_CHIP_ID0 &&
		 g_platform_chip_id.aon_chip_id1 == SHARKL3_CHIP_ID1)
		g_platform_chip_type = WCN_PLATFORM_TYPE_SHARKL3;
	else if (g_platform_chip_id.aon_chip_id0 == QOGIRL6_CHIP_ID0 &&
		 g_platform_chip_id.aon_chip_id1 == QOGIRL6_CHIP_ID1)
		g_platform_chip_type = WCN_PLATFORM_TYPE_QOGIRL6;
	else
		WCN_ERR("aon_chip_id0:[%d],id1[%d]\n",
			g_platform_chip_id.aon_chip_id0,
			g_platform_chip_id.aon_chip_id1);

	WCN_DBG("platform chip type: [%d]\n",
		 g_platform_chip_type);

	return 0;
}

const char *integ_wcn_get_chip_name(void)
{
	if (wcn_platform_chip_type() != WCN_PLATFORM_TYPE_QOGIRL6) {
		snprintf(wcn_chip_name, sizeof(wcn_chip_name),
			 "marlin2-built-in_0x%x_0x%x",
			 g_platform_chip_id.aon_chip_id0,
			 g_platform_chip_id.aon_chip_id1);
	} else {
		snprintf(wcn_chip_name, sizeof(wcn_chip_name),
			 "SharkL6_0x%x_0x%x", QOGIRL6_CHIP_ID0, QOGIRL6_CHIP_ID1);
	}

	return wcn_chip_name;
}

/* soft reset btwf without cache */
static void wcn_soft_reset_release_btwf_cpu(u32 type)
{
	struct regmap *regmap;
	u32 value;
	u32 platform_type = wcn_platform_chip_type();

	if (platform_type == WCN_PLATFORM_TYPE_QOGIRL6) {
		regmap = wcn_get_btwf_regmap(REGMAP_WCN_AON_AHB);
		if (type == WCN_BTWF_CPU_RESET) {
			/* reset btwf cm4 */
			wcn_regmap_read(regmap, 0X0c, &value);
			/* BIT2:btwf_cmstart_sys_rst set */
			value |= 1 << 2;
			wcn_regmap_raw_write_bit(regmap, 0X0c, value);
		} else if (type == WCN_BTWF_CPU_RESET_RELEASE) {
			/* release btwf cm4 */
			wcn_regmap_read(regmap, 0X0c, &value);
			/* BIT2:btwf_cmstart_sys_rst clear */
			value &= ~(1 << 2);
			wcn_regmap_raw_write_bit(regmap, 0X0c, value);
			msleep(200);
		}
		return;
	}

	if (platform_type == WCN_PLATFORM_TYPE_SHARKL3)
		regmap = wcn_get_btwf_regmap(REGMAP_WCN_REG);
	else
		regmap = wcn_get_btwf_regmap(REGMAP_ANLG_WRAP_WCN);
	if (type == WCN_BTWF_CPU_RESET) {
		/* reset btwf cm4 */
		wcn_regmap_read(regmap, 0X20, &value);
		value |= 1 << 3;
		wcn_regmap_raw_write_bit(regmap, 0X20, value);

		wcn_regmap_read(regmap, 0X24, &value);
		value |= 1 << 2;
		wcn_regmap_raw_write_bit(regmap, 0X24, value);
	} else if (type == WCN_BTWF_CPU_RESET_RELEASE) {
		value = 0;
		/* release btwf cm4 */
		wcn_regmap_raw_write_bit(regmap, 0X20, value);
		wcn_regmap_raw_write_bit(regmap, 0X24, value);
		msleep(200);
	}
}

void mdbg_hold_cpu(void)
{
	u32 value;
	phys_addr_t init_addr;

	wcn_soft_reset_release_btwf_cpu(WCN_BTWF_CPU_RESET);

	/* set cache flag */
	value = MDBG_CACHE_FLAG_VALUE;
	init_addr = wcn_get_btwf_init_status_addr();
	wcn_write_data_to_phy_addr(init_addr, (void *)&value, 4);

	//wait for set flag done
	usleep_range(2000, 2500);
	wcn_soft_reset_release_btwf_cpu(WCN_BTWF_CPU_RESET_RELEASE);
}

void mdbg_cpu_reset(void)
{
	wcn_soft_reset_release_btwf_cpu(WCN_BTWF_CPU_RESET);
}

static void wcn_merlion_power_on(void)
{
	struct gpio_desc *merlion_chip_en_gpio = s_wcn_device.merlion_chip_en;
	struct gpio_desc *merlion_reset_gpio = s_wcn_device.merlion_reset;

	if (merlion_chip_en_gpio && merlion_reset_gpio) {
		WCN_INFO("merlion chip en pull up\n");
		gpiod_set_value(merlion_chip_en_gpio, 1);
		udelay(500);
		gpiod_set_value(merlion_reset_gpio, 0);
		udelay(500);
		gpiod_set_value(merlion_reset_gpio, 1);

		WCN_INFO("merlion  chip en reset\n");
	}
}

static void wcn_merlion_power_off(void)
{
	struct gpio_desc *merlion_chip_en_gpio = s_wcn_device.merlion_chip_en;
	struct gpio_desc *merlion_reset_gpio = s_wcn_device.merlion_reset;

	if (merlion_chip_en_gpio && merlion_reset_gpio) {
		WCN_INFO("merlion chip en pull down\n");
		gpiod_set_value(merlion_reset_gpio, 0);
		gpiod_set_value(merlion_chip_en_gpio, 0);
	}
}

void wcn_merlion_power_control(bool enable)
{
	if (enable)
		wcn_merlion_power_on();
	else
		wcn_merlion_power_off();
}
