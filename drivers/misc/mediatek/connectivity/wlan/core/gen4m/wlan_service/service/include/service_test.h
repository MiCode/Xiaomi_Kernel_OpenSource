/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __SERVICE_TEST_H__
#define __SERVICE_TEST_H__

#include "test_engine.h"
#include "operation.h"

/*****************************************************************************
 *	Macro
 *****************************************************************************/
#define SERV_GET_PARAM(_struct, _member)				\
	(_struct->_member)
#define SERV_GET_PADDR(_struct, _member)				\
	(&_struct->_member)
#define SERV_SET_PARAM(_struct, _member, _val)				\
	(_struct->_member = _val)
#define WINFO_GET_PARAM(_struct, _member)				\
	(_struct->test_winfo->_member)
#define WINFO_GET_PADDR(_struct, _member)				\
	(&_struct->test_winfo->_member)
#define WINFO_SET_PARAM(_struct, _member, _val)				\
	(_struct->test_winfo->_member = _val)
#define BSTATE_GET_PARAM(_struct, _member)				\
	(_struct->test_bstat._member)
#define BSTATE_SET_PARAM(_struct, _member, _val)			\
	(_struct->test_bstat._member = _val)
#define CONFIG_GET_PARAM(_struct, _member, _bandidx)			\
	(_struct->test_config[_bandidx]._member)
#define CONFIG_GET_PADDR(_struct, _member, _bandidx)			\
	(&_struct->test_config[_bandidx]._member)
#define CONFIG_SET_PARAM(_struct, _member, _val, _bandidx)		\
	(_struct->test_config[_bandidx]._member = _val)
#define CONFIG_SET_PADDR(_struct, _member, _val, _size, _bandidx) ({	\
	struct test_configuration *configs;				\
	configs = &_struct->test_config[_bandidx];			\
	(sys_ad_move_mem(configs->_member, _val, _size));		\
	})
#define EEPROM_GET_PARAM(_struct, _member)				\
	(_struct->test_eprm._member)
#define EEPROM_SET_PARAM(_struct, _member, _val)			\
	(_struct->test_eprm._member = _val)


/*****************************************************************************
 *	Enum value definition
 *****************************************************************************/
/* Service test item id */
enum {
	SERV_TEST_ITEM_INIT = 0,
	SERV_TEST_ITEM_EXIT,
	SERV_TEST_ITEM_START,
	SERV_TEST_ITEM_STOP,
	SERV_TEST_ITEM_START_TX,
	SERV_TEST_ITEM_STOP_TX,
	SERV_TEST_ITEM_START_RX,
	SERV_TEST_ITEM_STOP_RX
};

/* Service test register/eeprom related operation */
enum {
	SERV_TEST_REG_MAC_READ = 0,
	SERV_TEST_REG_MAC_WRITE,
	SERV_TEST_REG_MAC_READ_BULK,
	SERV_TEST_REG_RF_READ_BULK,
	SERV_TEST_REG_RF_WRITE_BULK,
	SERV_TEST_REG_CA53_READ,
	SERV_TEST_REG_CA53_WRITE,

	SERV_TEST_EEPROM_READ = 10,
	SERV_TEST_EEPROM_WRITE,
	SERV_TEST_EEPROM_READ_BULK,
	SERV_TEST_EEPROM_WRITE_BULK,
	SERV_TEST_EEPROM_GET_FREE_EFUSE_BLOCK
};

/* Service test mps related operation */
enum {
	SERV_TEST_MPS_START_TX = 0,
	SERV_TEST_MPS_STOP_TX
};

/* Service test tx power related operation */
enum {
	SERV_TEST_TXPWR_SET_PWR = 0,
	SERV_TEST_TXPWR_GET_PWR,
	SERV_TEST_TXPWR_SET_PWR_INIT,
	SERV_TEST_TXPWR_SET_PWR_MAN,
};

/*****************************************************************************
 *	Data struct definition
 *****************************************************************************/
/* Service data struct for test mode usage */
struct service_test {
	/*========== Jedi only ==========*/
	/* Wlan related information which test needs */
	struct test_wlan_info *test_winfo;

	/* Test backup CR */
	struct test_bk_cr test_bkcr[TEST_MAX_BKCR_NUM];

	/* Test Rx statistic */
	struct test_rx_stat test_rx_statistic[TEST_DBDC_BAND_NUM];

	/* The band related state which communicate between UI/driver */
	struct test_band_state test_bstat;

	/* The band_idx which user wants to control currently */
	u_char ctrl_band_idx;

	struct test_backup_params test_backup;

	/*========== Common part ==========*/
	/* Test configuration */
	struct test_configuration test_config[TEST_DBDC_BAND_NUM];

	/* Test operation */
	struct test_operation *test_op;

	/* Test control register read/write */
	struct test_register test_reg;

	/* Test eeprom read/write */
	struct test_eeprom test_eprm;

	/* Test tmr related configuration */
	struct test_tmr_info test_tmr;

	/* Jedi: false, Gen4m: true */
	boolean engine_offload;

	/* TODO: factor out here for log dump */
	u_int32 en_log;
	/* struct _ATE_LOG_DUMP_CB log_dump[ATE_LOG_TYPE_NUM]; */
};

/*****************************************************************************
 *	Function declaration
 *****************************************************************************/
s_int32 mt_serv_init_test(struct service_test *serv_test);
s_int32 mt_serv_exit_test(struct service_test *serv_test);
s_int32 mt_serv_start(struct service_test *serv_test);
s_int32 mt_serv_stop(struct service_test *serv_test);
s_int32 mt_serv_set_channel(struct service_test *serv_test);
s_int32 mt_serv_set_tx_content(struct service_test *serv_test);
s_int32 mt_serv_set_tx_path(struct service_test *serv_test);
s_int32 mt_serv_set_rx_path(struct service_test *serv_test);
s_int32 mt_serv_submit_tx(struct service_test *serv_test);
s_int32 mt_serv_revert_tx(struct service_test *serv_test);
s_int32 mt_serv_start_tx(struct service_test *serv_test);
s_int32 mt_serv_stop_tx(struct service_test *serv_test);
s_int32 mt_serv_start_rx(struct service_test *serv_test);
s_int32 mt_serv_stop_rx(struct service_test *serv_test);
s_int32 mt_serv_set_freq_offset(struct service_test *serv_test);
s_int32 mt_serv_tx_power_operation(
	struct service_test *serv_test, u_int32 item);
s_int32 mt_serv_get_freq_offset(
	struct service_test *serv_test, u_int32 *freq_offset);
s_int32 mt_serv_get_cfg_on_off(
	struct service_test *serv_test,
	u_int32 type, u_int32 *result);
s_int32 mt_serv_get_tx_tone_pwr(
	struct service_test *serv_test,
	u_int32 ant_idx, u_int32 *power);
s_int32 mt_serv_get_thermal_val(
	struct service_test *serv_test,
	u_char band_idx,
	u_int32 *value);
s_int32 mt_serv_set_cal_bypass(
	struct service_test *serv_test,
	u_int32 cal_item);
s_int32 mt_serv_set_dpd(
	struct service_test *serv_test,
	u_int32 on_off,
	u_int32 wf_sel);
s_int32 mt_serv_set_tssi(
	struct service_test *serv_test,
	u_int32 on_off,
	u_int32 wf_sel);
s_int32 mt_serv_set_rdd_on_off(
	struct service_test *serv_test,
	u_int32 rdd_num,
	u_int32 rdd_sel,
	u_int32 enable);
s_int32 mt_serv_set_off_ch_scan(
	struct service_test *serv_test);
s_int32 mt_serv_set_icap_start(
	struct service_test *serv_test,
	struct hqa_rbist_cap_start *icap_info);
s_int32 mt_serv_get_icap_status(
	struct service_test *serv_test,
	s_int32 *icap_stat);
s_int32 mt_serv_get_icap_max_data_len(
	struct service_test *serv_test,
	u_long *max_data_len);
s_int32 mt_serv_get_icap_data(
	struct service_test *serv_test,
	s_int32 *icap_cnt,
	s_int32 *icap_data,
	u_int32 wf_num,
	u_int32 iq_type);
s_int32 mt_serv_get_recal_cnt(
	struct service_test *serv_test,
	u_int32 *recal_cnt,
	u_int32 *recal_dw_num);
s_int32 mt_serv_get_recal_content(
	struct service_test *serv_test,
	u_int32 *content);
s_int32 mt_serv_get_rxv_cnt(
	struct service_test *serv_test,
	u_int32 *rxv_cnt,
	u_int32 *rxv_dw_num);
s_int32 mt_serv_get_rxv_content(
	struct service_test *serv_test,
	u_int32 dw_cnt,
	u_int32 *content);
s_int32 mt_serv_get_rdd_cnt(
	struct service_test *serv_test,
	u_int32 *rdd_cnt,
	u_int32 *rdd_dw_num);
s_int32 mt_serv_get_rdd_content(
	struct service_test *serv_test,
	u_int32 *content,
	u_int32 *total_cnt);
s_int32 mt_serv_reset_txrx_counter(struct service_test *serv_test);
s_int32 mt_serv_set_rx_vector_idx(
	struct service_test *serv_test, u_int32 group1, u_int32 group2);
s_int32 mt_serv_set_fagc_rssi_path(
	struct service_test *serv_test);
s_int32 mt_serv_get_rx_stat_leg(
	struct service_test *serv_test,
	struct test_rx_stat_leg *rx_stat);
s_int32 mt_serv_get_capability(
	struct service_test *serv_test,
	struct test_capability *capability);
s_int32 mt_serv_calibration_test_mode(
	struct service_test *serv_test, u_char mode);
s_int32 mt_serv_do_cal_item(
	struct service_test *serv_test, u_int32 item);
s_int32 mt_serv_set_band_mode(struct service_test *serv_test);
s_int32 mt_serv_get_band_mode(struct service_test *serv_test);
s_int32 mt_serv_log_on_off(
	struct service_test *serv_test, u_int32 log_type,
	u_int32 log_ctrl, u_int32 log_size);
s_int32 mt_serv_set_cfg_on_off(struct service_test *serv_test);
s_int32 mt_serv_set_rx_filter_pkt_len(struct service_test *serv_test);
s_int32 mt_serv_get_wf_path_comb(struct service_test *serv_test,
	u_int8 band_idx, boolean dbdc_mode_en, u_int8 *path, u_int8 *path_len);
s_int32 mt_serv_set_low_power(
	struct service_test *serv_test, u_int32 control);
s_int32 mt_serv_get_antswap_capability(
	struct service_test *serv_test, u_int32 *antswap_support);
s_int32 mt_serv_set_antswap(
	struct service_test *serv_test, u_int32 ant);
s_int32 mt_serv_reg_eprm_operation(
	struct service_test *serv_test, u_int32 item);
s_int32 mt_serv_mps_operation(
	struct service_test *serv_test, u_int32 item);
s_int32 mt_serv_get_chipid(struct service_test *serv_test);
s_int32 mt_serv_mps_set_nss(struct service_test *serv_test);
s_int32 mt_serv_mps_set_per_packet_bw(struct service_test *serv_test);
s_int32 mt_serv_mps_set_packet_count(struct service_test *serv_test);
s_int32 mt_serv_mps_set_payload_length(struct service_test *serv_test);
s_int32 mt_serv_mps_set_power_gain(struct service_test *serv_test);
s_int32 mt_serv_mps_set_seq_data(struct service_test *serv_test);
s_int32 mt_serv_set_tmr(struct service_test *serv_test);
s_int32 mt_serv_set_preamble(struct service_test *serv_test);
s_int32 mt_serv_set_rate(struct service_test *serv_test);
s_int32 mt_serv_set_system_bw(struct service_test *serv_test);
s_int32 mt_serv_set_per_pkt_bw(struct service_test *serv_test);
s_int32 mt_serv_dbdc_tx_tone(struct service_test *serv_test);
s_int32 mt_serv_dbdc_tx_tone_pwr(struct service_test *serv_test);
s_int32 mt_serv_dbdc_continuous_tx(struct service_test *serv_test);
s_int32 mt_serv_get_tx_info(struct service_test *serv_test);
s_int32 mt_serv_main(struct service_test *serv_test, u_int32 test_item);
s_int32 mt_serv_get_rx_stat(struct service_test *serv_test, u_int8 band_idx,
	u_int8 blk_idx, u_int8 test_rx_stat_cat, struct test_rx_stat_u *st);

#endif /* __SERVICE_TEST_H__ */
