// SPDX-License-Identifier: GPL-2.0-only
/*
 * rf.h - Unisoc platform header
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

#ifndef __MARLIN2_RF_H__
#define __MARLIN2_RF_H__

#define DCOC_CALI_CODE_LEN 18
#define TXPOWER_LEN 14
#define TXPOWER_GAIN_MAP_COUNTER 14
#define TXPOWER_GAIN_MAPPING_LEN 32
#define TXPOWER_SUBCARRIES_COUNTER 14
#define TXPOWER_SUBCARRIES_LEN 15
#define RF_CTUNE_LEN 14
#define TPC_CFG_LEN 50
#define CHANNEL_DPD_CALI_LEN 182
#define CHANNEL_DPD_CALI_NUM 3

#define CALI_CMD_NAME_LEN 64
#define CALI_CMD_PARA_LEN 200
struct nvm_cali_cmd {
	int8_t itm[CALI_CMD_NAME_LEN];
	int32_t par[CALI_CMD_PARA_LEN];
	int32_t num;
};

struct nvm_name_table {
	int8_t *itm;
	uint32_t mem_offset;
	int32_t type;
};

struct tx_power_control_t {
	int8_t data_rate_power;
	int8_t channel_num;
	int8_t channel_range[6];
	int8_t b_tx_power_dr0[3];
	int8_t b_tx_power_dr1[3];
	int8_t g_tx_power_dr0[3];
	int8_t g_tx_power_dr1[3];
	int8_t g_tx_power_dr2[3];
	int8_t g_tx_power_dr3[3];
	int8_t n_tx_power_dr0[3];
	int8_t n_tx_power_dr1[3];
	int8_t n_tx_power_dr2[3];
	int8_t n_tx_power_dr3[3];
	int8_t power_reserved[10];
};

struct init_register_t {
	int8_t phy0_init_num;
	uint16_t init_phy0_regs[16];
	int8_t phy1_init_num;
	uint16_t init_phy1_regs[6];
	int8_t rf_init_num;
	uint32_t init_rf_regs[16];
	int8_t reserved_w16_num;
	uint16_t reserved_w16_regs[10];
	int8_t reserved_w32_num;
	uint16_t reserved_w32_regs[10];
};

struct enhance_config_t {
	int8_t tpc_enable;
	int8_t power_save_key;
	int8_t enhance_reserved[4];
};

struct coex_config_t {
	int8_t CoexExcutionMode;
	int8_t CoexWifiScanCntPerChannel;
	int8_t CoexWifiScanDurationOneTime;
	int8_t CoexScoPeriodsToBlockDuringDhcp;
	int8_t CoexA2dpDhcpProtectLevel;
	int8_t CoexScoperiodsToBlockDuringEap;
	int8_t CoexA2dpEapProtectLevel;
	int8_t CoexScoPeriodsToBlockDuringWifiJoin;
	int8_t CoexA2dpWifiJoinProtectLevel;
	uint16_t CoexEnterPMStateTime;
	uint16_t CoexAclA2dpBtWorkTime;
	uint16_t CoexAclA2dpWifiWorkTime;
	uint16_t CoexAclNoA2dpBtWorkTime;
	uint16_t CoexAclNoA2dpWifiWorkTime;
	uint16_t CoexAclMixBtWorkTime;
	uint16_t CoexAclMixWifiWorkTime;
	uint16_t CoexPageInqBtWorkTime;
	uint16_t CoexPageInqWifiWorkTime;
	uint16_t CoexScoSchema;
	uint16_t CoexDynamicScoSchemaEnable;
	uint16_t CoexScoPeriodsBtTakeAll;
	uint16_t CoexLteTxAdvancedTime;
	uint16_t CoexLteOneSubFrameLen;
	uint16_t CoexLteTxTimerLen;
	uint16_t CoexLteTxTimerFrameHeadLen;
	uint16_t CoexLteStrategyFlag;
	uint16_t CoexWifiDegradePowerValue;
	uint16_t CoexBtDegradePowerValue;
	uint16_t CoexWifi2300TxSpur2Lte[7];
	uint16_t CoexWifi2310TxSpur2Lte[7];
	uint16_t CoexWifi2320TxSpur2Lte[7];
	uint16_t CoexWifi2330TxSpur2Lte[7];
	uint16_t CoexWifi2340TxSpur2Lte[7];
	uint16_t CoexWifi2350TxSpur2Lte[7];
	uint16_t CoexWifi2360TxSpur2Lte[7];
	uint16_t CoexWifi2370TxSpur2Lte[7];
	uint16_t CoexWifi2380TxSpur2Lte[7];
	uint16_t CoexWifi2390TxSpur2Lte[7];
	uint16_t CoexWifi2400TxSpur2Lte[7];
	uint16_t CoexLteTxSpur2Wifi2300[7];
	uint16_t CoexLteTxSpur2Wifi2310[7];
	uint16_t CoexLteTxSpur2Wifi2320[7];
	uint16_t CoexLteTxSpur2Wifi2330[7];
	uint16_t CoexLteTxSpur2Wifi2340[7];
	uint16_t CoexLteTxSpur2Wifi2350[7];
	uint16_t CoexLteTxSpur2Wifi2360[7];
	uint16_t CoexLteTxSpur2Wifi2370[7];
	uint16_t CoexLteTxSpur2Wifi2380[7];
	uint16_t CoexLteTxSpur2Wifi2390[7];
	uint16_t CoexLteTxSpur2Wifi2400[7];
	uint16_t CoexReserved[16];
};

struct public_config_t {
	int8_t public_reserved[10];
};

struct cali_config_t {
	int8_t is_calibrated;
	int8_t rc_cali_en;
	int8_t dcoc_cali_en;
	int8_t txiq_cali_en;
	int8_t rxiq_cali_en;
	int8_t txpower_cali_en;
	int8_t dpd_cali_en;
	int8_t config_reserved[4];
};

struct rctune_cali_t {
	int8_t rctune_value;
	int8_t rctune_reserved[2];
};

struct dcoc_cali_t {
	uint16_t dcoc_cali_code[DCOC_CALI_CODE_LEN];
	uint32_t dcoc_reserved[4];
};

struct txiq_cali_t {
	uint32_t rf_txiq_c11;
	uint32_t rf_txiq_c12;
	uint32_t rf_txiq_c22;
	uint32_t rf_txiq_dc;
	uint32_t txiq_reserved[4];
};

struct rxiq_cali_t {
	uint32_t rf_rxiq_coef21_22;
	uint32_t rf_rxiq_coef11_12;
	uint32_t rxiq_reserved[2];
};

struct txpower_cali_t {
	int32_t txpower_psat_temperature;
	int8_t txpower_psat_gainindex;
	uint16_t txpower_psat_power;
	int8_t txpower_psat_backoff;
	uint8_t txpower_psat_upper_limit;
	uint8_t txpower_psat_lower_limit;
	int8_t txpower_freq_delta_gainindex[TXPOWER_LEN];
	int8_t txpower_psat_11b_backoff;
	int8_t txpower_psat_11g_backoff;
	int8_t txpower_psat_11n_backoff;
	int8_t txpower_sar_11b_backoff;
	int8_t txpower_sar_11g_backoff;
	int8_t txpower_sar_11n_backoff;
	int8_t txpower_countrycode_11b_backoff;
	int8_t txpower_countrycode_11g_backoff;
	int8_t txpower_countrycode_11n_backoff;
	int8_t g_txpower_npi_set;
	int8_t txpower_gain_mapping_flag;
	int8_t txpower_gain_mapping_table[TXPOWER_GAIN_MAP_COUNTER]
					 [TXPOWER_GAIN_MAPPING_LEN];
	int8_t txpower_subcarries_compensation_flag;

	int8_t txpower_subcarries_channel[TXPOWER_SUBCARRIES_COUNTER]
					 [TXPOWER_SUBCARRIES_LEN];

	int8_t txpower_psat_trace_value[4];
	uint32_t txpower_reserved[4];
	int8_t c_pad[3];
};

struct rf_para_t {
	uint8_t rf_ctune[RF_CTUNE_LEN];
	uint32_t rf_reserved[4];
};

struct tpc_cfg_t {
	uint32_t tpc_cfg[TPC_CFG_LEN];
	uint32_t tpc_reserved[4];
};

struct dpd_cali_t {
	int8_t dpd_cali_channel_num;
	int8_t dpd_cali_channel[3];
	int8_t dpd_mod_switch_flag;
	int8_t dpd_npi_cali_flag;
	uint32_t channel_dpd_cali_table[CHANNEL_DPD_CALI_NUM]
				       [CHANNEL_DPD_CALI_LEN];
	uint32_t dpd_reserved[4];
};

struct wifi_config_t {
	int8_t config_version;
	struct tx_power_control_t tx_power_control;
	struct init_register_t init_register;
	struct enhance_config_t enhance_config;
	struct coex_config_t coex_config;
	struct public_config_t public_config;
};

struct wifi_cali_t {
	int8_t cali_version;
	struct cali_config_t cali_config;
	struct rctune_cali_t rctune_cali;
	struct dcoc_cali_t dcoc_cali;
	struct txiq_cali_t txiq_cali;
	struct rxiq_cali_t rxiq_cali;
	struct txpower_cali_t txpower_cali;
	struct dpd_cali_t dpd_cali;
	struct rf_para_t rf_para;
	struct tpc_cfg_t tpc_cfg;
};

struct wifi_calibration {
	struct wifi_config_t config_data;
	struct wifi_cali_t cali_data;
};

int get_connectivity_config_param(struct wifi_config_t *p);
int get_connectivity_cali_param(struct wifi_cali_t *p);
void dump_cali_file(struct wifi_cali_t *p);

#endif
