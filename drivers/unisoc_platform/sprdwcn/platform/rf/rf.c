// SPDX-License-Identifier: GPL-2.0-only
/*
 * rf.c - Unisoc platform driver
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <misc/marlin_platform.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include "rf.h"
#include "wcn_integrate.h"

#define VENDOR_WIFI_CONFIG_FILE "connectivity_configure.ini"
#define VENDOR_WIFI_CALI_FILE "connectivity_calibration.ini"
#define VENDOR_WIFI_CONFIG_22NM_FILE "connectivity_configure.22nm.ini"
#define VENDOR_WIFI_CALI_22NM_FILE "connectivity_calibration.22nm.ini"
#define VENDOR_WIFI_CONFIG_AD_FILE "connectivity_configure.ad.ini"
#define VENDOR_WIFI_CALI_AD_FILE "connectivity_calibration.ad.ini"
#define VENDOR_WIFI_CONFIG_AB_FILE "connectivity_configure.ab.ini"
#define VENDOR_WIFI_CALI_AB_FILE "connectivity_calibration.ab.ini"
#define WIFI_CALI_DUMP_FILE "/mnt/vendor/wcn/connectivity_calibration_bak.ini"

#define CONF_TYPE 1
#define CALI_TYPE 2

#define MIN_DOUBLE_DIGIT 10
#define CF_TAB(NAME, MEM_OFFSET, TYPE) \
	{ NAME, (size_t)(&(((struct wifi_config_t *)(0))->MEM_OFFSET)), TYPE}
#define CL_TAB(NAME, MEM_OFFSET, TYPE) \
	{ NAME, (size_t)(&(((struct wifi_cali_t *)(0))->MEM_OFFSET)), TYPE}
#define OFS_MARK_STRING \
	"#-----------------------------------------------------------------\r\n"
#define GAIN_MP_ST \
	((int8_t *)&p->txpower_cali.txpower_gain_mapping_table1[0])
#define GAIN_MP_ED \
	((int8_t *)&p->txpower_cali.txpower_gain_mapping_table1[31])
#define TXPW_CARCH_ST \
	((int8_t *)&p->txpower_cali.txpower_subcarries_channel[0])
#define TXPW_CARCH_ED \
	((int8_t *)&p->txpower_cali.txpower_subcarries_channel[14])

static struct nvm_name_table g_config_table[] = {
	/*
	 * [SETCTION 0]Marlin config Version info
	 */
	CF_TAB("conf_version", config_version, 1),

	/*
	 * [SETCTION 1]wifi TX Power tx power control: tx_power_control_t
	 */
	CF_TAB("data_rate_power", tx_power_control.data_rate_power, 1),
	CF_TAB("channel_num", tx_power_control.channel_num, 1),
	CF_TAB("channel_range", tx_power_control.channel_range, 1),
	CF_TAB("b_tx_power_dr0", tx_power_control.b_tx_power_dr0, 1),
	CF_TAB("b_tx_power_dr1", tx_power_control.b_tx_power_dr1, 1),
	CF_TAB("g_tx_power_dr0", tx_power_control.g_tx_power_dr0, 1),
	CF_TAB("g_tx_power_dr1", tx_power_control.g_tx_power_dr1, 1),
	CF_TAB("g_tx_power_dr2", tx_power_control.g_tx_power_dr2, 1),
	CF_TAB("g_tx_power_dr3", tx_power_control.g_tx_power_dr3, 1),
	CF_TAB("n_tx_power_dr0", tx_power_control.n_tx_power_dr0, 1),
	CF_TAB("n_tx_power_dr1", tx_power_control.n_tx_power_dr1, 1),
	CF_TAB("n_tx_power_dr2", tx_power_control.n_tx_power_dr2, 1),
	CF_TAB("n_tx_power_dr3", tx_power_control.n_tx_power_dr3, 1),
	CF_TAB("power_reserved", tx_power_control.power_reserved, 1),

	/*
	 * [SETCTION 2]wifi PHY/RF reg init: init_register_t
	 */
	CF_TAB("phy0_init_num", init_register.phy0_init_num, 1),
	CF_TAB("init_phy0_regs", init_register.init_phy0_regs, 2),
	CF_TAB("phy1_init_num", init_register.phy1_init_num, 1),
	CF_TAB("init_phy1_regs", init_register.init_phy1_regs, 2),
	CF_TAB("rf_init_num", init_register.rf_init_num, 1),
	CF_TAB("init_rf_regs", init_register.init_rf_regs, 4),
	CF_TAB("reserved_w16_num", init_register.reserved_w16_num, 1),
	CF_TAB("reserved_w16_regs", init_register.reserved_w16_regs, 2),
	CF_TAB("reserved_w32_num", init_register.reserved_w32_num, 1),
	CF_TAB("reserved_w32_regs", init_register.reserved_w32_regs, 2),

	/*
	 * [SETCTION 3]wifi enhance config: enhance_config_t
	 */
	CF_TAB("tpc_enable", enhance_config.tpc_enable, 1),
	CF_TAB("power_save_key", enhance_config.power_save_key, 1),
	CF_TAB("enhance_reserved", enhance_config.enhance_reserved, 1),

	/*
	 * [SETCTION 4]Wifi/BT/lte coex config: coex_config_t
	 */
	CF_TAB("CoexExcutionMode",
		coex_config.CoexExcutionMode, 1),
	CF_TAB("CoexWifiScanCntPerChannel",
		coex_config.CoexWifiScanCntPerChannel, 1),
	CF_TAB("CoexWifiScanDurationOneTime",
		coex_config.CoexWifiScanDurationOneTime, 1),
	CF_TAB("CoexScoPeriodsToBlockDuringDhcp",
		coex_config.CoexScoPeriodsToBlockDuringDhcp, 1),
	CF_TAB("CoexA2dpDhcpProtectLevel",
		coex_config.CoexA2dpDhcpProtectLevel, 1),
	CF_TAB("CoexScoperiodsToBlockDuringEap",
		coex_config.CoexScoperiodsToBlockDuringEap, 1),
	CF_TAB("CoexA2dpEapProtectLevel",
		coex_config.CoexA2dpEapProtectLevel, 1),
	CF_TAB("CoexScoPeriodsToBlockDuringWifiJoin",
		coex_config.CoexScoPeriodsToBlockDuringWifiJoin, 1),
	CF_TAB("CoexA2dpWifiJoinProtectLevel",
		coex_config.CoexA2dpWifiJoinProtectLevel, 1),
	CF_TAB("CoexEnterPMStateTime",
		coex_config.CoexEnterPMStateTime, 2),
	CF_TAB("CoexAclA2dpBtWorkTime",
		coex_config.CoexAclA2dpBtWorkTime, 2),
	CF_TAB("CoexAclA2dpWifiWorkTime",
		coex_config.CoexAclA2dpWifiWorkTime, 2),
	CF_TAB("CoexAclNoA2dpBtWorkTime",
		coex_config.CoexAclNoA2dpBtWorkTime, 2),
	CF_TAB("CoexAclNoA2dpWifiWorkTime",
		coex_config.CoexAclNoA2dpWifiWorkTime, 2),
	CF_TAB("CoexAclMixBtWorkTime",
		coex_config.CoexAclMixBtWorkTime, 2),
	CF_TAB("CoexAclMixWifiWorkTime",
		coex_config.CoexAclMixWifiWorkTime, 2),
	CF_TAB("CoexPageInqBtWorkTime",
		coex_config.CoexPageInqBtWorkTime, 2),
	CF_TAB("CoexPageInqWifiWorkTime",
		coex_config.CoexPageInqWifiWorkTime, 2),
	CF_TAB("CoexScoSchema",
		coex_config.CoexScoSchema, 2),
	CF_TAB("CoexDynamicScoSchemaEnable",
		coex_config.CoexDynamicScoSchemaEnable, 2),
	CF_TAB("CoexScoPeriodsBtTakeAll",
		coex_config.CoexScoPeriodsBtTakeAll, 2),
	CF_TAB("CoexLteTxAdvancedTime",
		coex_config.CoexLteTxAdvancedTime, 2),
	CF_TAB("CoexLteOneSubFrameLen",
		coex_config.CoexLteOneSubFrameLen, 2),
	CF_TAB("CoexLteTxTimerLen",
		coex_config.CoexLteTxTimerLen, 2),
	CF_TAB("CoexLteTxTimerFrameHeadLen",
		coex_config.CoexLteTxTimerFrameHeadLen, 2),
	CF_TAB("CoexLteStrategyFlag",
		coex_config.CoexLteStrategyFlag, 2),
	CF_TAB("CoexWifiDegradePowerValue",
		coex_config.CoexWifiDegradePowerValue, 2),
	CF_TAB("CoexBtDegradePowerValue",
		coex_config.CoexBtDegradePowerValue, 2),
	CF_TAB("CoexWifi2300TxSpur2Lte",
		coex_config.CoexWifi2300TxSpur2Lte[0], 2),
	CF_TAB("CoexWifi2310TxSpur2Lte",
		coex_config.CoexWifi2310TxSpur2Lte[0], 2),
	CF_TAB("CoexWifi2320TxSpur2Lte",
		coex_config.CoexWifi2320TxSpur2Lte[0], 2),
	CF_TAB("CoexWifi2330TxSpur2Lte",
		coex_config.CoexWifi2330TxSpur2Lte[0], 2),
	CF_TAB("CoexWifi2340TxSpur2Lte",
		coex_config.CoexWifi2340TxSpur2Lte[0], 2),
	CF_TAB("CoexWifi2350TxSpur2Lte",
		coex_config.CoexWifi2350TxSpur2Lte[0], 2),
	CF_TAB("CoexWifi2360TxSpur2Lte",
		coex_config.CoexWifi2360TxSpur2Lte[0], 2),
	CF_TAB("CoexWifi2370TxSpur2Lte",
		coex_config.CoexWifi2370TxSpur2Lte[0], 2),
	CF_TAB("CoexWifi2380TxSpur2Lte",
		coex_config.CoexWifi2380TxSpur2Lte[0], 2),
	CF_TAB("CoexWifi2390TxSpur2Lte",
		coex_config.CoexWifi2390TxSpur2Lte[0], 2),
	CF_TAB("CoexWifi2400TxSpur2Lte",
		coex_config.CoexWifi2400TxSpur2Lte[0], 2),
	CF_TAB("CoexLteTxSpur2Wifi2300",
		coex_config.CoexLteTxSpur2Wifi2300[0], 2),
	CF_TAB("CoexLteTxSpur2Wifi2310",
		coex_config.CoexLteTxSpur2Wifi2310[0], 2),
	CF_TAB("CoexLteTxSpur2Wifi2320",
		coex_config.CoexLteTxSpur2Wifi2320[0], 2),
	CF_TAB("CoexLteTxSpur2Wifi2330",
		coex_config.CoexLteTxSpur2Wifi2330[0], 2),
	CF_TAB("CoexLteTxSpur2Wifi2340",
		coex_config.CoexLteTxSpur2Wifi2340[0], 2),
	CF_TAB("CoexLteTxSpur2Wifi2350",
		coex_config.CoexLteTxSpur2Wifi2350[0], 2),
	CF_TAB("CoexLteTxSpur2Wifi2360",
		coex_config.CoexLteTxSpur2Wifi2360[0], 2),
	CF_TAB("CoexLteTxSpur2Wifi2370",
		coex_config.CoexLteTxSpur2Wifi2370[0], 2),
	CF_TAB("CoexLteTxSpur2Wifi2380",
		coex_config.CoexLteTxSpur2Wifi2380[0], 2),
	CF_TAB("CoexLteTxSpur2Wifi2390",
		coex_config.CoexLteTxSpur2Wifi2390[0], 2),
	CF_TAB("CoexLteTxSpur2Wifi2400",
		coex_config.CoexLteTxSpur2Wifi2400[0], 2),
	CF_TAB("CoexReserved", coex_config.CoexReserved, 2),

	/*
	 * [SETCTION 5]Wifi&BT public config
	 */
	CF_TAB("public_reserved", public_config.public_reserved, 1),
	{0, 0, 0}
};

static struct nvm_name_table g_cali_table[] = {
	/*
	 * [SETCTION 0]Marlin cali Version info
	 */
	CL_TAB("cali_version", cali_version, 1),

	/*
	 * [SETCTION 1]Calibration Config: cali_config_t
	 */
	CL_TAB("is_calibrated", cali_config.is_calibrated, 1),
	CL_TAB("rc_cali_en", cali_config.rc_cali_en, 1),
	CL_TAB("dcoc_cali_en", cali_config.dcoc_cali_en, 1),
	CL_TAB("txiq_cali_en", cali_config.txiq_cali_en, 1),
	CL_TAB("rxiq_cali_en", cali_config.rxiq_cali_en, 1),
	CL_TAB("txpower_cali_en", cali_config.txpower_cali_en, 1),
	CL_TAB("dpd_cali_en", cali_config.dpd_cali_en, 1),
	CL_TAB("config_reserved", cali_config.config_reserved[0], 1),

	/*
	 * [SETCTION 2]rc calibration data: rctune_cali_t
	 */
	CL_TAB("rctune_value", rctune_cali.rctune_value, 1),
	CL_TAB("rc_cali_reserved", rctune_cali.rctune_reserved[0], 1),

	/*
	 * [SETCTION 3]doco calibration data: dcoc_cali_t
	 */
	CL_TAB("dcoc_cali_code", dcoc_cali.dcoc_cali_code[0], 2),
	CL_TAB("dcoc_reserved", dcoc_cali.dcoc_reserved[0], 4),

	/*
	 * [SETCTION 4]txiq calibration data: txiq_cali_t
	 */
	CL_TAB("rf_txiq_c11", txiq_cali.rf_txiq_c11, 4),
	CL_TAB("rf_txiq_c12", txiq_cali.rf_txiq_c12, 4),
	CL_TAB("rf_txiq_c22", txiq_cali.rf_txiq_c22, 4),
	CL_TAB("rf_txiq_dc", txiq_cali.rf_txiq_dc, 4),
	CL_TAB("txiq_reserved", txiq_cali.txiq_reserved[0], 4),

	/*
	 * [SETCTION 5]rxiq calibration data: rxiq_cali_t
	 */
	CL_TAB("rf_rxiq_coef21_22", rxiq_cali.rf_rxiq_coef21_22, 4),
	CL_TAB("rf_rxiq_coef11_12", rxiq_cali.rf_rxiq_coef11_12, 4),
	CL_TAB("rxiq_reserved", rxiq_cali.rxiq_reserved[0], 4),

	/*
	 * [SETCTION 6]txpower calibration data: txpower_cali_t
	 */
	CL_TAB("txpower_psat_temperature",
		txpower_cali.txpower_psat_temperature, 4),
	CL_TAB("txpower_psat_gainindex",
		txpower_cali.txpower_psat_gainindex, 1),

	CL_TAB("txpower_psat_power",
		txpower_cali.txpower_psat_power, 2),
	CL_TAB("txpower_psat_backoff",
		txpower_cali.txpower_psat_backoff, 1),
	CL_TAB("txpower_psat_upper_limit",
		txpower_cali.txpower_psat_upper_limit, 1),
	CL_TAB("txpower_psat_lower_limit",
		txpower_cali.txpower_psat_lower_limit, 1),

	CL_TAB("txpower_freq_delta_gainindex",
		txpower_cali.txpower_freq_delta_gainindex[0], 1),

	CL_TAB("txpower_psat_11b_backoff",
		txpower_cali.txpower_psat_11b_backoff, 1),
	CL_TAB("txpower_psat_11g_backoff",
		txpower_cali.txpower_psat_11g_backoff, 1),
	CL_TAB("txpower_psat_11n_backoff",
		txpower_cali.txpower_psat_11n_backoff, 1),
	CL_TAB("txpower_sar_11b_backoff",
		txpower_cali.txpower_sar_11b_backoff, 1),
	CL_TAB("txpower_sar_11g_backoff",
		txpower_cali.txpower_sar_11g_backoff, 1),
	CL_TAB("txpower_sar_11n_backoff",
		txpower_cali.txpower_sar_11n_backoff, 1),
	CL_TAB("txpower_countrycode_11b_backoff",
		txpower_cali.txpower_countrycode_11b_backoff, 1),
	CL_TAB("txpower_countrycode_11g_backoff",
		txpower_cali.txpower_countrycode_11g_backoff, 1),
	CL_TAB("txpower_countrycode_11n_backoff",
		txpower_cali.txpower_countrycode_11n_backoff, 1),
	CL_TAB("g_txpower_npi_set",
		txpower_cali.g_txpower_npi_set, 1),
	CL_TAB("txpower_gain_mapping_flag",
		txpower_cali.txpower_gain_mapping_flag, 1),

	CL_TAB("txpower_gain_mapping_table1",
		txpower_cali.txpower_gain_mapping_table[0], 1),
	CL_TAB("txpower_gain_mapping_table2",
		txpower_cali.txpower_gain_mapping_table[1], 1),
	CL_TAB("txpower_gain_mapping_table3",
		txpower_cali.txpower_gain_mapping_table[2], 1),
	CL_TAB("txpower_gain_mapping_table4",
		txpower_cali.txpower_gain_mapping_table[3], 1),
	CL_TAB("txpower_gain_mapping_table5",
		txpower_cali.txpower_gain_mapping_table[4], 1),
	CL_TAB("txpower_gain_mapping_table6",
		txpower_cali.txpower_gain_mapping_table[5], 1),
	CL_TAB("txpower_gain_mapping_table7",
		txpower_cali.txpower_gain_mapping_table[6], 1),
	CL_TAB("txpower_gain_mapping_table8",
		txpower_cali.txpower_gain_mapping_table[7], 1),
	CL_TAB("txpower_gain_mapping_table9",
		txpower_cali.txpower_gain_mapping_table[8], 1),
	CL_TAB("txpower_gain_mapping_table10",
		txpower_cali.txpower_gain_mapping_table[9], 1),
	CL_TAB("txpower_gain_mapping_table11",
		txpower_cali.txpower_gain_mapping_table[10], 1),
	CL_TAB("txpower_gain_mapping_table12",
		txpower_cali.txpower_gain_mapping_table[11], 1),
	CL_TAB("txpower_gain_mapping_table13",
		txpower_cali.txpower_gain_mapping_table[12], 1),
	CL_TAB("txpower_gain_mapping_table14",
		txpower_cali.txpower_gain_mapping_table[13], 1),

	CL_TAB("txpower_subcarries_compensation_flag",
		txpower_cali.txpower_subcarries_compensation_flag, 1),
	CL_TAB("txpower_subcarries_channel1",
		txpower_cali.txpower_subcarries_channel[0], 1),
	CL_TAB("txpower_subcarries_channel2",
		txpower_cali.txpower_subcarries_channel[1], 1),
	CL_TAB("txpower_subcarries_channel3",
		txpower_cali.txpower_subcarries_channel[2], 1),
	CL_TAB("txpower_subcarries_channel4",
		txpower_cali.txpower_subcarries_channel[3], 1),
	CL_TAB("txpower_subcarries_channel5",
		txpower_cali.txpower_subcarries_channel[4], 1),
	CL_TAB("txpower_subcarries_channel6",
		txpower_cali.txpower_subcarries_channel[5], 1),
	CL_TAB("txpower_subcarries_channel7",
		txpower_cali.txpower_subcarries_channel[6], 1),
	CL_TAB("txpower_subcarries_channel8",
		txpower_cali.txpower_subcarries_channel[7], 1),
	CL_TAB("txpower_subcarries_channel9",
		txpower_cali.txpower_subcarries_channel[8], 1),
	CL_TAB("txpower_subcarries_channel10",
		txpower_cali.txpower_subcarries_channel[9], 1),
	CL_TAB("txpower_subcarries_channel11",
		txpower_cali.txpower_subcarries_channel[10], 1),
	CL_TAB("txpower_subcarries_channel12",
		txpower_cali.txpower_subcarries_channel[11], 1),
	CL_TAB("txpower_subcarries_channel13",
		txpower_cali.txpower_subcarries_channel[12], 1),
	CL_TAB("txpower_subcarries_channel14",
		txpower_cali.txpower_subcarries_channel[13], 1),

	CL_TAB("txpower_psat_trace_value",
		txpower_cali.txpower_psat_trace_value[0], 1),
	CL_TAB("txpower_reserved",
		txpower_cali.txpower_reserved[0], 4),
	CL_TAB("c_pad",
		txpower_cali.c_pad[0], 1),

	/*
	 * [SETCTION 7]DPD calibration data: dpd_cali_t
	 */
	CL_TAB("dpd_cali_channel_num",
		dpd_cali.dpd_cali_channel_num, 1),
	CL_TAB("dpd_cali_channel",
		dpd_cali.dpd_cali_channel[0], 1),
	CL_TAB("dpd_mod_switch_flag",
		dpd_cali.dpd_mod_switch_flag, 1),
	CL_TAB("dpd_npi_cali_flag",
		dpd_cali.dpd_npi_cali_flag, 1),
	CL_TAB("channel1_dpd_cali_table",
		dpd_cali.channel_dpd_cali_table[0], 4),
	CL_TAB("channel2_dpd_cali_table",
		dpd_cali.channel_dpd_cali_table[1], 4),
	CL_TAB("channel3_dpd_cali_table",
		dpd_cali.channel_dpd_cali_table[2], 4),
	CL_TAB("dpd_reserved",
		dpd_cali.dpd_reserved[0], 4),

	/*
	 * [SETCTION 8]RF parameters data: rf_para_t
	 */
	CL_TAB("rf_ctune", rf_para.rf_ctune[0], 1),
	CL_TAB("rf_reserved", rf_para.rf_reserved[0], 4),

	/*
	 * [SETCTION 9]RF parameters data: tpc_cfg_t
	 */
	CL_TAB("tpc_cfg", tpc_cfg.tpc_cfg[0], 4),
	CL_TAB("tpc_reserved", tpc_cfg.tpc_reserved[0], 4),
	{0, 0, 0}
};

static int find_type(char key)
{
	if ((key >= 'a' && key <= 'w') ||
		(key >= 'y' && key <= 'z') ||
		(key >= 'A' && key <= 'W') ||
		(key >= 'Y' && key <= 'Z') ||
		('_' == key))
		return 1;
	if ((key >= '0' && key <= '9') ||
		('-' == key))
		return 2;
	if (('x' == key) ||
		('X' == key) ||
		('.' == key))
		return 3;
	if ((key == '\0') ||
		('\r' == key) ||
		('\n' == key) ||
		('#' == key))
		return 4;
	return 0;
}

static int wifi_nvm_set_cmd(struct nvm_name_table *pTable,
	struct nvm_cali_cmd *cmd, void *p_data)
{
	int i;
	unsigned char *p;

	if ((pTable->type != 1) &&
		(pTable->type !=  2) &&
		(pTable->type != 4))
		return -1;

	p = (unsigned char *)(p_data) + pTable->mem_offset;

	pr_info("[g_table]%s, offset:%u, num:%u, value: %x %x %x",
		pTable->itm, pTable->mem_offset, cmd->num,
		cmd->par[0], cmd->par[1], cmd->par[2]);
	pr_debug(", %x %x %x %x %x %x\n",
		cmd->par[6], cmd->par[7], cmd->par[8],
		cmd->par[9], cmd->par[10], cmd->par[11]);

	for (i = 0; i < cmd->num; i++) {
		if (pTable->type == 1)
			*((unsigned char *)p + i) =
			(unsigned char)(cmd->par[i]);
		else if (pTable->type == 2)
			*((unsigned short *)p + i) =
			(unsigned short)(cmd->par[i]);
		else if (pTable->type == 4)
			*((unsigned int *)p + i) =
			(unsigned int)(cmd->par[i]);
		else
			pr_info("%s, type err\n", __func__);
	}
	return 0;
}

static void get_cmd_par(const char *str, struct nvm_cali_cmd *cmd)
{
	int i, j, bufType, cType, flag;
	char tmp[64];
	char c;
	s64 val;

	bufType = -1;
	cType = 0;
	flag = 0;
	memset(cmd, 0, sizeof(struct nvm_cali_cmd));
	for (i = 0, j = 0;; i++) {
		c = str[i];
		cType = find_type(c);
		if ((cType == 1) || (cType == 2) ||
			(cType == 3)) {
			tmp[j] = c;
			j++;
			if (bufType == -1) {
				if (cType == 2)
					bufType = 2;
				else
					bufType = 1;
			} else if (bufType == 2) {
				if (cType == 1)
					bufType = 1;
			}
			continue;
		}
		if (-1 != bufType) {
			tmp[j] = '\0';

			if ((bufType == 1) && (flag == 0)) {
				strcpy(cmd->itm, tmp);
				flag = 1;
			} else {
				if (kstrtos64(tmp, 0, &val))
					pr_err("kstrtos64 %s: error is %d\n",
					       tmp, kstrtos64(tmp, 0, &val));
				cmd->par[cmd->num] = val & 0xFFFFFFFFFFFFFFFF;
				cmd->num++;
			}
			bufType = -1;
			j = 0;
		}
		if (cType == 0)
			continue;
		if (cType == 4)
			return;
	}
}

static struct nvm_name_table *cf_table_match(struct nvm_cali_cmd *cmd)
{
	int i;
	struct nvm_name_table *pTable = NULL;
	int len = sizeof(g_config_table) / sizeof(struct nvm_name_table);

	if (cmd == NULL)
		return NULL;
	for (i = 0; i < len; i++) {
		if (g_config_table[i].itm == NULL)
			continue;
		if (strcmp(g_config_table[i].itm, cmd->itm) != 0)
			continue;
		pTable = &g_config_table[i];
		break;
	}
	return pTable;
}

static struct nvm_name_table *cali_table_match(struct nvm_cali_cmd *cmd)
{
	int i;
	struct nvm_name_table *pTable = NULL;
	int len = sizeof(g_cali_table) / sizeof(struct nvm_name_table);

	if (cmd == NULL)
		return NULL;
	for (i = 0; i < len; i++) {
		if (g_cali_table[i].itm == NULL)
			continue;
		if (strcmp(g_cali_table[i].itm, cmd->itm) != 0)
			continue;
		pTable = &g_cali_table[i];
		break;
	}
	return pTable;
}

static int wifi_nvm_buf_operate(const char *pBuf, int file_len,
				const int type, void *p_data)
{
	int i, p;
	struct nvm_cali_cmd cmd;
	struct nvm_name_table *pTable = NULL;

	if ((pBuf == NULL) || (file_len == 0))
		return -1;
	for (i = 0, p = 0; i < file_len; i++) {
		if (('\n' == *(pBuf + i)) ||
			('\r' == *(pBuf + i)) ||
			('\0' == *(pBuf + i))) {
			if (5 <= (i - p)) {
				get_cmd_par((pBuf + p), &cmd);
				if (type == 1) {
					pTable = cf_table_match(&cmd);
				} else if (type == 2) {	/*calibration */
					pTable = cali_table_match(&cmd);
				} else {
					pr_info("%s unknown type\n", __func__);
					return -1;
				}

				if (pTable != NULL)
					wifi_nvm_set_cmd(pTable, &cmd, p_data);
			}
			p = i + 1;
		}
	}
	return 0;
}

static int wifi_nvm_parse(const char *path, const int type, void *p_data)
{
	const struct firmware *fw;
	int ret = 0;

	pr_info("%s()...\n", __func__);

	ret = request_firmware(&fw, path, NULL);
	if (ret) {
		pr_err("open file %s error\n", path);
		return -1;
	}

	if (!fw || !fw->data || fw->size <= 0) {
		pr_err("%s invalid firmware file\n", __func__);
		release_firmware(fw);
		return -EINVAL;
	}


	pr_info("%s read %s data_len:0x%lx\n", __func__, path, fw->size);
	wifi_nvm_buf_operate(fw->data, fw->size, type, p_data);
	release_firmware(fw);
	pr_info("%s(), ok!\n", __func__);
	return 0;
}


int get_connectivity_config_param(struct wifi_config_t *p)
{
	char *path = VENDOR_WIFI_CONFIG_FILE;

	if (wcn_get_aon_chip_id() == WCN_SHARKLE_CHIP_AD)
		path = VENDOR_WIFI_CONFIG_AD_FILE;
	else if (wcn_get_aon_chip_id() == WCN_SHARKL3_CHIP_22NM)
		path = VENDOR_WIFI_CONFIG_22NM_FILE;
	else if (wcn_get_aon_chip_id() == WCN_PIKE2_CHIP_AB)
		path = VENDOR_WIFI_CONFIG_AB_FILE;
	pr_info("%s path : %s\n", __func__, path);
	return wifi_nvm_parse(path, CONF_TYPE, (void *)p);
}

int get_connectivity_cali_param(struct wifi_cali_t *p)
{
	char *path = VENDOR_WIFI_CALI_FILE;

	if (wcn_get_aon_chip_id() == WCN_SHARKLE_CHIP_AD)
		path = VENDOR_WIFI_CALI_AD_FILE;
	else if (wcn_get_aon_chip_id() == WCN_SHARKL3_CHIP_22NM)
		path = VENDOR_WIFI_CALI_22NM_FILE;
	else if (wcn_get_aon_chip_id() == WCN_PIKE2_CHIP_AB)
		path = VENDOR_WIFI_CALI_AB_FILE;
	pr_info("%s path : %s\n", __func__, path);
	return wifi_nvm_parse(path, CALI_TYPE, (void *)p);
}

static int write_file(struct file *fp, char *buf, size_t len)
{
	loff_t offset = 0;

	offset = vfs_llseek(fp, 0, SEEK_END);
	return 0;
}

#define DUMP(fp, fmt, args...) \
	do { \
		static char buf[1024]; \
		size_t len = snprintf(buf, sizeof(buf), fmt, ## args); \
		write_file(fp, buf, len); \
	} while (0)

static void cali_save_file(char *path, struct wifi_cali_t *p)
{
	struct file *fp;
	int i, j;

	//set_fs(KERNEL_DS);

	fp = NULL;
	if (IS_ERR_OR_NULL(fp)) {
		pr_err("%s(), open error!\n", __func__);
		return;
	}
	DUMP(fp, "[SETCTION 0]\r\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "# Marlin2 cali Version info\r\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "cali_version = %d\r\n\r\n", p->cali_version);


	DUMP(fp, "[SETCTION 1]\r\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "# Calibration Config\r\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "is_calibrated   = %d\r\n",
		p->cali_config.is_calibrated/*p->cali_config->is_calibrated*/);
	DUMP(fp, "rc_cali_en      = %d\r\n", p->cali_config.rc_cali_en);
	DUMP(fp, "dcoc_cali_en    = %d\r\n",
		p->cali_config.dcoc_cali_en);
	DUMP(fp, "txiq_cali_en    = %d\r\n", p->cali_config.txiq_cali_en);
	DUMP(fp, "rxiq_cali_en    = %d\r\n", p->cali_config.rxiq_cali_en);
	DUMP(fp, "txpower_cali_en = %d\r\n",
		p->cali_config.txpower_cali_en);
	DUMP(fp, "dpd_cali_en     = %d\r\n",
		p->cali_config.dpd_cali_en);
	DUMP(fp, "config_reserved = %d, %d, %d, %d\r\n\r\n",
		p->cali_config.config_reserved[0],
		p->cali_config.config_reserved[1],
		p->cali_config.config_reserved[2],
		p->cali_config.config_reserved[3]);


	DUMP(fp, "[SETCTION 2]\r\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "# rc calibration data\r\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "rctune_value    = 0x%x\n",
		p->rctune_cali.rctune_value);
	DUMP(fp, "rc_cali_reserved= 0x%x, 0x%x\r\n\r\n",
		p->rctune_cali.rctune_reserved[0],
		p->rctune_cali.rctune_reserved[1]);

	DUMP(fp, "[SETCTION 3]\r\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "# doco calibration data\r\n");
	DUMP(fp, OFS_MARK_STRING);

	DUMP(fp, "dcoc_cali_code    =");
	for (i = 0; i < DCOC_CALI_CODE_LEN - 1; i++)
		DUMP(fp, " 0x%x,", p->dcoc_cali.dcoc_cali_code[i]);
	DUMP(fp, " 0x%x\r\n",
		p->dcoc_cali.dcoc_cali_code[DCOC_CALI_CODE_LEN - 1]);

	DUMP(fp, "dcoc_reserved   = 0x%x, 0x%x, 0x%x, 0x%x\r\n\r\n",
		p->dcoc_cali.dcoc_reserved[0],
		p->dcoc_cali.dcoc_reserved[1],
		p->dcoc_cali.dcoc_reserved[2],
		p->dcoc_cali.dcoc_reserved[3]);


	DUMP(fp, "[SETCTION 4]\r\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "# txiq calibration data\r\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "rf_txiq_c11     = 0x%x\r\n", p->txiq_cali.rf_txiq_c11);
	DUMP(fp, "rf_txiq_c12     = 0x%x\r\n", p->txiq_cali.rf_txiq_c12);
	DUMP(fp, "rf_txiq_c22     = 0x%x\r\n", p->txiq_cali.rf_txiq_c22);
	DUMP(fp, "rf_txiq_dc      = 0x%x\r\n", p->txiq_cali.rf_txiq_dc);
	DUMP(fp, "txiq_reserved   = 0x%x, 0x%x, 0x%x, 0x%x\r\n",
		p->txiq_cali.txiq_reserved[0],
		p->txiq_cali.txiq_reserved[1],
		p->txiq_cali.txiq_reserved[2],
		p->txiq_cali.txiq_reserved[3]);
	DUMP(fp, "\r\n");


	DUMP(fp, "[SETCTION 5]\r\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "# rxiq calibration data\r\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "rf_rxiq_coef21_22   = 0x%x\r\n",
		p->rxiq_cali.rf_rxiq_coef21_22);
	DUMP(fp, "rf_rxiq_coef11_12   = 0x%x\r\n",
		p->rxiq_cali.rf_rxiq_coef11_12);
	DUMP(fp, "rxiq_reserved       = 0x%x, 0x%x\r\n",
		p->rxiq_cali.rxiq_reserved[0],
		p->rxiq_cali.rxiq_reserved[1]);
	DUMP(fp, "\r\n");

	DUMP(fp, "[SETCTION 6]\r\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "# txpower calibration data\r\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "txpower_psat_temperature   = %d\r\n",
		p->txpower_cali.txpower_psat_temperature);
	DUMP(fp, "txpower_psat_gainindex   = %d\r\n",
		p->txpower_cali.txpower_psat_gainindex);
	DUMP(fp, "txpower_psat_power   = %d\r\n",
		p->txpower_cali.txpower_psat_power);
	DUMP(fp, "txpower_psat_backoff   = %d\r\n",
		p->txpower_cali.txpower_psat_backoff);
	DUMP(fp, "txpower_psat_upper_limit   = %d\r\n",
		p->txpower_cali.txpower_psat_upper_limit);
	DUMP(fp, "txpower_psat_lower_limit   = %d\r\n",
		p->txpower_cali.txpower_psat_lower_limit);

	DUMP(fp, "txpower_freq_delta_gainindex =");
	for (i = 0; i < TXPOWER_LEN - 1; i++)
		DUMP(fp, " %d,",
		p->txpower_cali.txpower_freq_delta_gainindex[i]);
	DUMP(fp, " %d\r\n",
		p->txpower_cali.txpower_freq_delta_gainindex[TXPOWER_LEN - 1]);

	DUMP(fp, "txpower_psat_11b_backoff = %d\r\n",
		p->txpower_cali.txpower_psat_11b_backoff);
	DUMP(fp, "txpower_psat_11g_backoff = %d\r\n",
		p->txpower_cali.txpower_psat_11g_backoff);
	DUMP(fp, "txpower_psat_11n_backoff = %d\r\n",
		p->txpower_cali.txpower_psat_11n_backoff);
	DUMP(fp, "txpower_sar_11b_backoff = %d\r\n",
		p->txpower_cali.txpower_sar_11b_backoff);
	DUMP(fp, "txpower_sar_11g_backoff = %d\r\n",
		p->txpower_cali.txpower_sar_11g_backoff);
	DUMP(fp, "txpower_sar_11n_backoff = %d\r\n",
		p->txpower_cali.txpower_sar_11n_backoff);
	DUMP(fp, "txpower_countrycode_11b_backoff = %d\r\n",
		p->txpower_cali.txpower_countrycode_11b_backoff);
	DUMP(fp, "txpower_countrycode_11g_backoff = %d\r\n",
		p->txpower_cali.txpower_countrycode_11g_backoff);
	DUMP(fp, "txpower_countrycode_11n_backoff = %d\r\n",
		p->txpower_cali.txpower_countrycode_11n_backoff);
	DUMP(fp, "g_txpower_npi_set = %d\r\n",
		p->txpower_cali.g_txpower_npi_set);
	DUMP(fp, "txpower_gain_mapping_flag    = %d\r\n",
		p->txpower_cali.txpower_gain_mapping_flag);

	for (j = 0; j < TXPOWER_GAIN_MAP_COUNTER; j++) {
		if (j < MIN_DOUBLE_DIGIT)
			DUMP(fp, "txpower_gain_mapping_table%d  =", j + 1);
		else
			DUMP(fp, "txpower_gain_mapping_table%d =", j + 1);

		for (i = 0; i < TXPOWER_GAIN_MAPPING_LEN - 1; i++)
			DUMP(fp, " %d,",
			     p->txpower_cali.txpower_gain_mapping_table[j][i]);
		DUMP(fp, " %d\r\n",
		     p->txpower_cali.txpower_gain_mapping_table[j]
		     [TXPOWER_GAIN_MAPPING_LEN - 1]);
	}

	DUMP(fp, "txpower_subcarries_compensation_flag = %d\r\n",
		p->txpower_cali.txpower_subcarries_compensation_flag);

	for (j = 0; j < TXPOWER_SUBCARRIES_COUNTER; j++) {
		if (j < MIN_DOUBLE_DIGIT)
			DUMP(fp, "txpower_subcarries_channel%d  =", j + 1);
		else
			DUMP(fp, "txpower_subcarries_channel%d =", j + 1);

		for (i = 0; i < TXPOWER_SUBCARRIES_LEN - 1; i++)
			DUMP(fp, " %d,",
			     p->txpower_cali.txpower_subcarries_channel[j][i]);
		DUMP(fp, " %d\r\n",
		     p->txpower_cali.txpower_subcarries_channel[j]
		     [TXPOWER_SUBCARRIES_LEN - 1]);
	}

	DUMP(fp, "txpower_psat_trace_value = %d, %d, %d, %d\r\n",
		p->txpower_cali.txpower_psat_trace_value[0],
		p->txpower_cali.txpower_psat_trace_value[1],
		p->txpower_cali.txpower_psat_trace_value[2],
		p->txpower_cali.txpower_psat_trace_value[3]);

	DUMP(fp, "txpower_reserved = %d, %d, %d, %d\r\n",
		p->txpower_cali.txpower_reserved[0],
		p->txpower_cali.txpower_reserved[1],
		p->txpower_cali.txpower_reserved[2],
		p->txpower_cali.txpower_reserved[3]);

	DUMP(fp, "c_pad = %d, %d, %d\r\n", p->txpower_cali.c_pad[0],
			p->txpower_cali.c_pad[1],
			p->txpower_cali.c_pad[2]);


	DUMP(fp, "[SETCTION 7]\r\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "# DPD calibration data\r\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "dpd_cali_channel_num   = %d\r\n",
		p->dpd_cali.dpd_cali_channel_num);
	DUMP(fp, "dpd_cali_channel   = %d, %d, %d\r\n",
		p->dpd_cali.dpd_cali_channel[0],
		p->dpd_cali.dpd_cali_channel[1],
		p->dpd_cali.dpd_cali_channel[2]);
	DUMP(fp, "dpd_mod_switch_flag = %d\r\n",
		p->dpd_cali.dpd_mod_switch_flag);
	DUMP(fp, "dpd_npi_cali_flag = %d\r\n",
		p->dpd_cali.dpd_npi_cali_flag);

	for (j = 0; j < CHANNEL_DPD_CALI_NUM; j++) {
		DUMP(fp, "channel%d_dpd_cali_table =", j + 1);
		for (i = 0; i < CHANNEL_DPD_CALI_LEN - 1; i++)
			DUMP(fp, " 0x%x,",
			     p->dpd_cali.channel_dpd_cali_table[j][i]);
		DUMP(fp, " 0x%x\r\n", p->dpd_cali.channel_dpd_cali_table[j]
		     [CHANNEL_DPD_CALI_LEN - 1]);
	}

	DUMP(fp, "dpd_reserved   = 0x%x, 0x%x, 0x%x, 0x%x\r\n",
		p->dpd_cali.dpd_reserved[0],
		p->dpd_cali.dpd_reserved[1],
		p->dpd_cali.dpd_reserved[2],
		p->dpd_cali.dpd_reserved[3]);


	DUMP(fp, "[SETCTION 8]\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "# RF parameters data\n");
	DUMP(fp, OFS_MARK_STRING);

	DUMP(fp, "rf_ctune =");
	for (i = 0; i < RF_CTUNE_LEN - 1; i++)
		DUMP(fp, " %d,", p->rf_para.rf_ctune[i]);
	DUMP(fp, " %d\r\n", p->rf_para.rf_ctune[RF_CTUNE_LEN - 1]);

	DUMP(fp, "rf_reserved = %d, %d, %d, %d\r\n\r\n",
		p->rf_para.rf_reserved[0],
		p->rf_para.rf_reserved[1],
		p->rf_para.rf_reserved[2],
		p->rf_para.rf_reserved[3]);


	DUMP(fp, "[SETCTION 9]\n");
	DUMP(fp, OFS_MARK_STRING);
	DUMP(fp, "# TPC Configuration data\n");
	DUMP(fp, OFS_MARK_STRING);

	DUMP(fp, "tpc_cfg =");
	for (i = 0; i < TPC_CFG_LEN - 1; i++)
		DUMP(fp, " 0x%x,", p->tpc_cfg.tpc_cfg[i]);
	DUMP(fp, " 0x%x\r\n", p->tpc_cfg.tpc_cfg[TPC_CFG_LEN - 1]);

	DUMP(fp, "tpc_reserved = 0x%x, 0x%x, 0x%x, 0x%x\r\n",
		p->tpc_cfg.tpc_reserved[0],
		p->tpc_cfg.tpc_reserved[1],
		p->tpc_cfg.tpc_reserved[2],
		p->tpc_cfg.tpc_reserved[3]);

    //set_fs(USER_DS);
}


void dump_cali_file(struct wifi_cali_t *p)
{
	pr_err("%s() write cali bak file: %s\n", __func__, WIFI_CALI_DUMP_FILE);
	cali_save_file(WIFI_CALI_DUMP_FILE, p);
}
