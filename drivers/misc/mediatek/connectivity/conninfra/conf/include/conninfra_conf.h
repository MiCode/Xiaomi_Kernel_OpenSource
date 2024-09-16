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
*    \brief  Declaration of library functions
*
*    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/



#ifndef _CONNINFRA_CONF_H_
#define _CONNINFRA_CONF_H_

#include "osal.h"

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define CUST_CFG_INFRA "WMT.cfg"
#define CUST_CFG_INFRA_SOC "WMT_SOC.cfg"

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/



/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/



/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                        P U B L I C   D A T A
********************************************************************************
*/
struct conf_byte_ary {
	unsigned int size;
	char *data;
};

struct conninfra_conf {

	char conf_name[NAME_MAX + 1];
	//const osal_firmware *conf_inst;
	unsigned char cfg_exist;

	unsigned char coex_wmt_ant_mode;
	unsigned char coex_wmt_ant_mode_ex;
	unsigned char coex_wmt_ext_component;
	unsigned char coex_wmt_wifi_time_ctl;
	unsigned char coex_wmt_ext_pta_dev_on;
	/*combo chip and LTE coex filter mode setting */
	unsigned char coex_wmt_filter_mode;

	unsigned char coex_bt_rssi_upper_limit;
	unsigned char coex_bt_rssi_mid_limit;
	unsigned char coex_bt_rssi_lower_limit;
	unsigned char coex_bt_pwr_high;
	unsigned char coex_bt_pwr_mid;
	unsigned char coex_bt_pwr_low;

	unsigned char coex_wifi_rssi_upper_limit;
	unsigned char coex_wifi_rssi_mid_limit;
	unsigned char coex_wifi_rssi_lower_limit;
	unsigned char coex_wifi_pwr_high;
	unsigned char coex_wifi_pwr_mid;
	unsigned char coex_wifi_pwr_low;

	unsigned char coex_ext_pta_hi_tx_tag;
	unsigned char coex_ext_pta_hi_rx_tag;
	unsigned char coex_ext_pta_lo_tx_tag;
	unsigned char coex_ext_pta_lo_rx_tag;
	unsigned short coex_ext_pta_sample_t1;
	unsigned short coex_ext_pta_sample_t2;
	unsigned char coex_ext_pta_wifi_bt_con_trx;

	unsigned int coex_misc_ext_pta_on;
	unsigned int coex_misc_ext_feature_set;
	/*GPS LNA setting */
	unsigned char wmt_gps_lna_pin;
	unsigned char wmt_gps_lna_enable;
	/*Power on sequence */
	unsigned char pwr_on_rtc_slot;
	unsigned char pwr_on_ldo_slot;
	unsigned char pwr_on_rst_slot;
	unsigned char pwr_on_off_slot;
	unsigned char pwr_on_on_slot;
	unsigned char co_clock_flag;

	/*deep sleep feature flag*/
	unsigned char disable_deep_sleep_cfg;

	/* Combo chip side SDIO driving setting */
	unsigned int sdio_driving_cfg;

	/* Combo chip WiFi path setting */
	unsigned short coex_wmt_wifi_path;
	/* Combo chip WiFi eLAN gain setting */
	unsigned char  coex_wmt_ext_elna_gain_p1_support;
	unsigned int coex_wmt_ext_elna_gain_p1_D0;
	unsigned int coex_wmt_ext_elna_gain_p1_D1;
	unsigned int coex_wmt_ext_elna_gain_p1_D2;
	unsigned int coex_wmt_ext_elna_gain_p1_D3;

	struct conf_byte_ary *coex_wmt_epa_elna;

	unsigned char bt_tssi_from_wifi;
	unsigned short bt_tssi_target;

	unsigned char coex_config_bt_ctrl;
	unsigned char coex_config_bt_ctrl_mode;
	unsigned char coex_config_bt_ctrl_rw;

	unsigned char coex_config_addjust_opp_time_ratio;
	unsigned char coex_config_addjust_opp_time_ratio_bt_slot;
	unsigned char coex_config_addjust_opp_time_ratio_wifi_slot;

	unsigned char coex_config_addjust_ble_scan_time_ratio;
	unsigned char coex_config_addjust_ble_scan_time_ratio_bt_slot;
	unsigned char coex_config_addjust_ble_scan_time_ratio_wifi_slot;

	/* POS. If set, means using ext TCXO */
	unsigned char tcxo_gpio;

	unsigned char pre_cal_mode;
};



/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/





/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
const struct conninfra_conf *conninfra_conf_get_cfg(void);
int conninfra_conf_set_cfg_file(const char *name);

int conninfra_conf_init(void);
int conninfra_conf_deinit(void);

#endif				/* _CONNINFRA_CONF_H_ */
