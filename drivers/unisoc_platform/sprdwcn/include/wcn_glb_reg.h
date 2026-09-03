/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 *
 */
#ifndef _WCN_GLB_REG_H_
#define _WCN_GLB_REG_H_

#include "sprd_wcn.h"
#include "sc2342_glb.h"
#include "umw2631_integrate_glb.h"
#include "sc2355_glb.h"
#include "umw2652_glb.h"
#include "umw2653_glb.h"

#define PACKET_SIZE		(32*1024)
#define WIFI_REG 0x60300004

#define CALI_REG 0x70040000
#define CALI_OFSET_REG 0x70040010

#define GNSS_CHIPID_REG 0x603003fc

static inline unsigned int get_gnss_cp_start_addr(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		return INTEG_GNSS_CP_START_ADDR;
	else if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_GNSS_CP_START_ADDR;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_GNSS_CP_START_ADDR;
	else
		return M3E_GNSS_CP_START_ADDR;

}

static inline unsigned int get_cp_start_addr(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		return INTEG_CP_START_ADDR;
	else
		return CP_START_ADDR;

}

static inline unsigned int get_gnss_firmware_max_size(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		return INTEG_GNSS_FIRMWARE_MAX_SIZE;
	else if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_GNSS_FIRMWARE_MAX_SIZE;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_GNSS_FIRMWARE_MAX_SIZE;
	else
		return M3E_GNSS_FIRMWARE_MAX_SIZE;

}

static inline unsigned int get_firmware_max_size(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		return INTEG_FIRMWARE_MAX_SIZE;
	else if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_FIRMWARE_MAX_SIZE;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_FIRMWARE_MAX_SIZE;
	else
		return M3E_FIRMWARE_MAX_SIZE;

}

static inline unsigned int get_cp_reset_reg(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		return INTEG_CP_RESET_REG;
	else if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_CP_RESET_REG;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_CP_RESET_REG;
	else
		return M3E_CP_RESET_REG;

}

static inline unsigned int get_gnss_cp_reset_reg(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		return INTEG_GNSS_CP_RESET_REG;
	else if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_GNSS_CP_RESET_REG;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_GNSS_CP_RESET_REG;
	else
		return M3E_GNSS_CP_RESET_REG;

}

static inline unsigned int get_chipid_reg(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		return INTEG_CHIPID_REG;
	else if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_CHIPID_REG;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_CHIPID_REG;
	else
		return M3E_CHIPID_REG;

}

static inline unsigned int get_sync_addr(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_SYNC_ADDR;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_SYNC_ADDR;
	else
		return M3E_SYNC_ADDR;

}

static inline unsigned int get_arm_dap_reg1(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_ARM_DAP_REG1;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_ARM_DAP_REG1;
	else
		return M3E_ARM_DAP_REG1;

}

static inline unsigned int get_cp_slp_ctl_reg(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_REG_CP_SLP_CTL;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_REG_CP_SLP_CTL;
	else
		return M3E_REG_CP_SLP_CTL;

}

static inline unsigned int get_btwf_slp_sts_reg(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_REG_BTWF_SLP_STS;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_REG_BTWF_SLP_STS;
	else
		return M3E_REG_BTWF_SLP_STS;

}

static inline unsigned int get_btwf_in_deepslp_xtl_on(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_BTWF_IN_DEEPSLEEP_XLT_ON;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_BTWF_IN_DEEPSLEEP_XLT_ON;
	else
		return M3E_BTWF_IN_DEEPSLEEP_XLT_ON;

}

static inline unsigned int get_wcn_bound_xo_mode(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_WCN_BOUND_XO_MODE;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_WCN_BOUND_XO_MODE;
	else
		return M3_WCN_BOUND_XO_MODE;

}

static inline unsigned int get_arm_dap_reg2(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_ARM_DAP_REG2;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_ARM_DAP_REG2;
	else
		return M3E_ARM_DAP_REG2;

}

static inline unsigned int get_arm_dap_reg3(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_ARM_DAP_REG3;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_ARM_DAP_REG3;
	else
		return M3E_ARM_DAP_REG3;

}

static inline unsigned int get_btwf_status_reg(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_BTWF_STATUS_REG;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_BTWF_STATUS_REG;
	else
		return M3E_BTWF_STATUS_REG;

}

static inline unsigned int get_wifi_aon_mac_size(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_WIFI_AON_MAC_SIZE;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_WIFI_AON_MAC_SIZE;
	else
		return M3E_WIFI_AON_MAC_SIZE;

}

static inline unsigned int get_wifi_ram_size(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_WIFI_RAM_SIZE;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_WIFI_RAM_SIZE;
	else
		return M3E_WIFI_RAM_SIZE;

}

static inline unsigned int get_bt_acc_size(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_BT_ACC_SIZE;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_BT_ACC_SIZE;
	else
		return M3E_BT_ACC_SIZE;

}

static inline unsigned int get_bt_modem_size(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3)
		return M3_BT_MODEM_SIZE;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return M3L_BT_MODEM_SIZE;
	else
		return M3E_BT_MODEM_SIZE;

}


#endif
