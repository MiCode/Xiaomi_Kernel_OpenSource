/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "service_test.h"

u_char template_frame[32] = { 0x88, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0x00, 0xAA, 0xBB, 0x12, 0x34, 0x56,
	0x00, 0x11, 0x22, 0xAA, 0xBB, 0xCC, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*****************************************************************************
 *	Internal functions
 *****************************************************************************/
static s_int32 mt_serv_init_op(struct test_operation *ops)
{
	ops->op_set_tr_mac = mt_op_set_tr_mac;
	ops->op_set_tx_stream = mt_op_set_tx_stream;
	ops->op_set_tx_path = mt_op_set_tx_path;
	ops->op_set_rx_path = mt_op_set_rx_path;
	ops->op_set_rx_filter = mt_op_set_rx_filter;
	ops->op_set_clean_persta_txq = mt_op_set_clean_persta_txq;
	ops->op_set_cfg_on_off = mt_op_set_cfg_on_off;
	ops->op_log_on_off = mt_op_log_on_off;
	ops->op_dbdc_tx_tone = mt_op_dbdc_tx_tone;
	ops->op_dbdc_tx_tone_pwr = mt_op_dbdc_tx_tone_pwr;
	ops->op_dbdc_continuous_tx = mt_op_dbdc_continuous_tx;
	ops->op_get_tx_info = mt_op_get_tx_info;
	ops->op_set_antenna_port = mt_op_set_antenna_port;
	ops->op_set_slot_time = mt_op_set_slot_time;
	ops->op_set_power_drop_level = mt_op_set_power_drop_level;
	ops->op_get_antswap_capability = mt_op_get_antswap_capability;
	ops->op_set_antswap = mt_op_set_antswap;
	ops->op_set_rx_filter_pkt_len = mt_op_set_rx_filter_pkt_len;
	ops->op_set_freq_offset = mt_op_set_freq_offset;
	ops->op_set_phy_counter = mt_op_set_phy_counter;
	ops->op_set_rxv_index = mt_op_set_rxv_index;
	ops->op_set_fagc_path = mt_op_set_fagc_path;
	ops->op_set_fw_mode = mt_op_set_fw_mode;
	ops->op_set_rf_test_mode = mt_op_set_rf_test_mode;
	ops->op_set_test_mode_start = mt_op_set_test_mode_start;
	ops->op_set_test_mode_abort = mt_op_set_test_mode_abort;
	ops->op_start_tx = mt_op_start_tx;
	ops->op_stop_tx = mt_op_stop_tx;
	ops->op_start_rx = mt_op_start_rx;
	ops->op_stop_rx = mt_op_stop_rx;
	ops->op_set_channel = mt_op_set_channel;
	ops->op_set_tx_content = mt_op_set_tx_content;
	ops->op_set_preamble = mt_op_set_preamble;
	ops->op_set_rate = mt_op_set_rate;
	ops->op_set_system_bw = mt_op_set_system_bw;
	ops->op_set_per_pkt_bw = mt_op_set_per_pkt_bw;
	ops->op_reset_txrx_counter = mt_op_reset_txrx_counter;
	ops->op_set_rx_vector_idx = mt_op_set_rx_vector_idx;
	ops->op_set_fagc_rssi_path = mt_op_set_fagc_rssi_path;
	ops->op_get_rx_stat_leg = mt_op_get_rx_stat_leg;
	ops->op_get_rx_statistics_all = mt_op_get_rx_statistics_all;
	ops->op_get_capability = mt_op_get_capability;
	ops->op_calibration_test_mode = mt_op_calibration_test_mode;
	ops->op_set_icap_start = mt_op_set_icap_start;
	ops->op_get_icap_status = mt_op_get_icap_status;
	ops->op_get_icap_max_data_len = mt_op_get_icap_max_data_len;
	ops->op_get_icap_data = mt_op_get_icap_data;
	ops->op_do_cal_item = mt_op_do_cal_item;
	ops->op_set_band_mode = mt_op_set_band_mode;
	ops->op_get_chipid = mt_op_get_chipid;
	ops->op_mps_start = mt_op_mps_start;
	ops->op_mps_set_nss = mt_op_mps_set_nss;
	ops->op_mps_set_per_packet_bw = mt_op_mps_set_per_packet_bw;
	ops->op_mps_set_packet_count = mt_op_mps_set_packet_count;
	ops->op_mps_set_payload_length = mt_op_mps_set_payload_length;
	ops->op_mps_set_power_gain = mt_op_mps_set_power_gain;
	ops->op_mps_set_seq_data = mt_op_mps_set_seq_data;
	ops->op_get_tx_pwr = mt_op_get_tx_pwr;
	ops->op_set_tx_pwr = mt_op_set_tx_pwr;
	ops->op_get_freq_offset = mt_op_get_freq_offset;
	ops->op_get_cfg_on_off = mt_op_get_cfg_on_off;
	ops->op_get_tx_tone_pwr = mt_op_get_tx_tone_pwr;
	ops->op_get_recal_cnt = mt_op_get_recal_cnt;
	ops->op_get_recal_content = mt_op_get_recal_content;
	ops->op_get_rxv_cnt = mt_op_get_rxv_cnt;
	ops->op_get_rxv_content = mt_op_get_rxv_content;
	ops->op_get_thermal_val = mt_op_get_thermal_val;
	ops->op_set_cal_bypass = mt_op_set_cal_bypass;
	ops->op_set_dpd = mt_op_set_dpd;
	ops->op_set_tssi = mt_op_set_tssi;
	ops->op_set_rdd_test = mt_op_set_rdd_test;
	ops->op_get_wf_path_comb = mt_op_get_wf_path_comb;
	ops->op_set_off_ch_scan = mt_op_set_off_ch_scan;
	ops->op_get_rdd_cnt = mt_op_get_rdd_cnt;
	ops->op_get_rdd_content = mt_op_get_rdd_content;
	ops->op_set_muru_manual = mt_op_set_muru_manual;
	ops->op_set_tam_arb = mt_op_set_tam_arb;
	ops->op_hetb_ctrl = mt_op_hetb_ctrl;
	ops->op_set_ru_aid = mt_op_set_ru_aid;
	ops->op_set_mutb_spe = mt_op_set_mutb_spe;
	ops->op_get_rx_stat_band = mt_op_get_rx_stat_band;
	ops->op_get_rx_stat_path = mt_op_get_rx_stat_path;
	ops->op_get_rx_stat_user = mt_op_get_rx_stat_user;
	ops->op_get_rx_stat_comm = mt_op_get_rx_stat_comm;
	/* For test mac usage */
	ops->op_backup_and_set_cr = mt_op_backup_and_set_cr;
	ops->op_restore_cr = mt_op_restore_cr;
	ops->op_set_ampdu_ba_limit = mt_op_set_ampdu_ba_limit;
	ops->op_set_sta_pause_cr = mt_op_set_sta_pause_cr;
	ops->op_set_ifs_cr = mt_op_set_ifs_cr;
	ops->op_write_mac_bbp_reg = mt_op_write_mac_bbp_reg;
	ops->op_read_bulk_mac_bbp_reg = mt_op_read_bulk_mac_bbp_reg;
	ops->op_read_bulk_rf_reg = mt_op_read_bulk_rf_reg;
	ops->op_write_bulk_rf_reg = mt_op_write_bulk_rf_reg;
	ops->op_read_bulk_eeprom = mt_op_read_bulk_eeprom;

	return SERV_STATUS_SUCCESS;
}

static s_int32 mt_serv_init_config(
	struct test_configuration *configs, u_char band_idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;

	if (band_idx == TEST_DBDC_BAND0) {
		/* Operated mode init */
		configs->op_mode = OP_MODE_STOP;

		/* Test packet init */
		ret = sys_ad_alloc_mem((u_char **) &configs->test_pkt,
					TEST_PKT_LEN);
		configs->pkt_skb = NULL;

		/* OS related  init */
		SERV_OS_INIT_COMPLETION(&configs->tx_wait);
		configs->tx_status = 0;

		/* Hardware resource init */
		configs->wdev_idx = 0;
		configs->wmm_idx = 0;
		configs->ac_idx = SERV_QID_AC_BE;

		/* Tx frame init */
		sys_ad_move_mem(&configs->template_frame, &template_frame, 32);
		configs->addr1[0][0] = 0x00;
		configs->addr1[0][1] = 0x11;
		configs->addr1[0][2] = 0x22;
		configs->addr1[0][3] = 0xAA;
		configs->addr1[0][4] = 0xBB;
		configs->addr1[0][5] = 0xCC;
		sys_ad_move_mem(configs->addr2[0], configs->addr1[0],
				SERV_MAC_ADDR_LEN);
		sys_ad_move_mem(configs->addr3[0], &configs->addr1[0],
				SERV_MAC_ADDR_LEN);
		configs->payload[0] = 0xAA;
		configs->seq = 0;
		configs->hdr_len = SERV_LENGTH_802_11;
		configs->pl_len = 1;
		configs->tx_len = 1058;
		configs->fixed_payload = 1;
		configs->max_pkt_ext = 2;
		configs->retry = 1;

		/* Tx strategy/type init */
		configs->txs_enable = FALSE;
		configs->tx_strategy = TEST_TX_STRA_THREAD;
		configs->rate_ctrl_type = TEST_TX_TYPE_TXD;

		/* Tx timing init */
		configs->duty_cycle = 0;
		configs->tx_time_param.pkt_tx_time_en = FALSE;
		configs->tx_time_param.pkt_tx_time = 0;
		configs->ipg_param.ipg = 0;
		configs->ipg_param.sig_ext = TEST_SIG_EXTENSION;
		configs->ipg_param.slot_time = TEST_DEFAULT_SLOT_TIME;
		configs->ipg_param.sifs_time = TEST_DEFAULT_SIFS_TIME;
		configs->ipg_param.ac_num = SERV_QID_AC_BE;
		configs->ipg_param.aifsn = TEST_MIN_AIFSN;
		configs->ipg_param.cw = TEST_MIN_CW;
		configs->ipg_param.txop = 0;

		/* Rx init */
		sys_ad_zero_mem(&configs->own_mac, SERV_MAC_ADDR_LEN);

		/* Test tx statistic and txs init */
		configs->txs_enable = FALSE;
		sys_ad_zero_mem(&configs->tx_stat,
				sizeof(struct test_tx_statistic));

		/* Phy */
		configs->tx_ant = 1;
		configs->rx_ant = 1;

		/* TODO: factor out here for phy */
		configs->channel = 1;
		configs->ch_band = 0;
		configs->ctrl_ch = 1;
#if 0
		if (BOARD_IS_5G_ONLY(pAd))
			configs->channel = 36;
		else
			configs->channel = 1;
#endif
		configs->tx_mode = TEST_MODE_OFDM;
		configs->bw = TEST_BW_20;
		configs->mcs = 7;
		configs->sgi = 0;

		/* tx power */
		configs->tx_pwr_sku_en = FALSE;
		configs->tx_pwr_percentage_en = FALSE;
		configs->tx_pwr_backoff_en = FALSE;
		configs->tx_pwr_percentage_level = 100;
	}
/* #ifdef DBDC_MODE  */
#if 0
	else if (band_idx == TEST_DBDC_BAND1) {
		/* Operated mode init */
		configs->op_mode = OP_MODE_STOP;

		/* Test packet init */
		ret = sys_ad_alloc_mem((u_char **) &configs->test_pkt,
					   TEST_PKT_LEN);
		configs->pkt_skb = NULL;

		/* OS related  init */
		SERV_OS_INIT_COMPLETION(&configs->tx_wait);
		configs->tx_status = 0;

		/* Hardware resource init */
		configs->wdev_idx = 1;
		configs->wmm_idx = 1;
		configs->ac_idx = SERV_QID_AC_BE;

		/* Tx frame init */
		sys_ad_move_mem(&configs->template_frame, &template_frame, 32);
		configs->addr1[0][0] = 0x00;
		configs->addr1[1][0] = 0x11;
		configs->addr1[2][0] = 0x22;
		configs->addr1[3][0] = 0xAA;
		configs->addr1[4][0] = 0xBB;
		configs->addr1[5][0] = 0xCC;
		sys_ad_move_mem(&configs->addr2, &configs->addr1,
				SERV_MAC_ADDR_LEN);
		sys_ad_move_mem(&configs->addr3, &configs->addr1,
				SERV_MAC_ADDR_LEN);
		configs->payload[0] = 0xAA;
		configs->seq = 0;
		configs->hdr_len = SERV_LENGTH_802_11;
		configs->pl_len = 1;
		configs->tx_len = 1024;
		configs->fixed_payload = 1;
		configs->max_pkt_ext = 2;
		configs->retry = 1;

		/* Tx strategy/type init */
		configs->tx_strategy = TEST_TX_STRA_THREAD;
		configs->rate_ctrl_type = TEST_TX_TYPE_TXD;

		/* Tx timing init */
		configs->duty_cycle = 0;
		configs->tx_time_param.pkt_tx_time_en = FALSE;
		configs->tx_time_param.pkt_tx_time = 0;
		configs->ipg_param.ipg = 0;
		configs->ipg_param.sig_ext = TEST_SIG_EXTENSION;
		configs->ipg_param.slot_time = TEST_DEFAULT_SLOT_TIME;
		configs->ipg_param.sifs_time = TEST_DEFAULT_SIFS_TIME;
		configs->ipg_param.ac_num = SERV_QID_AC_BE;
		configs->ipg_param.aifsn = TEST_MIN_AIFSN;
		configs->ipg_param.cw = TEST_MIN_CW;
		configs->ipg_param.txop = 0;

		/* Rx init */
		sys_ad_zero_mem(&configs->own_mac, SERV_MAC_ADDR_LEN);

		/* Test tx statistic and txs init */
		configs->txs_enable = FALSE;
		sys_ad_zero_mem(&configs->tx_stat,
				sizeof(struct test_tx_statistic));

		/* Phy */
		configs->tx_ant = 1;
		configs->rx_ant = 1;

		/* TODO: factor out here for phy */
		configs->channel = 36;
		configs->ch_band = 1;
		configs->ctrl_ch = 36;
#if 0
		if (pAd->CommonCfg.eDBDC_mode == ENUM_DBDC_5G5G) {
			configs->channel = 100;
			configs->ctrl_ch = 100;
		} else {
			configs->channel = 36;
			configs->ctrl_ch = 36;
		}
#endif
		configs->tx_mode = TEST_MODE_OFDM;
		configs->bw = TEST_BW_20;
		configs->mcs = 7;
		configs->sgi = 0;

		/* TODO: factor out here for Tx power */
	}
#endif /* DBDC_MODE */
	else
		return SERV_STATUS_SERV_TEST_INVALID_BANDIDX;

	return ret;
}

static s_int32 mt_serv_release_config(
	struct test_configuration *configs, u_char band_idx)
{
	s_int32 ret = SERV_STATUS_SUCCESS;

	if (band_idx == TEST_DBDC_BAND0) {
		if (configs->test_pkt) {
			sys_ad_free_mem(configs->test_pkt);
			configs->test_pkt = NULL;
		}
	}
/* #ifdef DBDC_MODE  */
#if 1
	else if (band_idx == TEST_DBDC_BAND1) {
		if (configs->test_pkt) {
			sys_ad_free_mem(configs->test_pkt);
			configs->test_pkt = NULL;
		}
	}
#endif /* DBDC_MODE */
	else
		return SERV_STATUS_SERV_TEST_INVALID_BANDIDX;

	return ret;
}

/*****************************************************************************
 *	Extern functions
 *****************************************************************************/
/* For test mode init of service.git */
s_int32 mt_serv_init_test(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char band_idx;

	if (!serv_test->engine_offload) {
		for (band_idx = TEST_DBDC_BAND0;
			band_idx < TEST_DBDC_BAND_NUM; band_idx++) {
			ret = mt_serv_init_config(
				&serv_test->test_config[band_idx], band_idx);
			if (ret != SERV_STATUS_SUCCESS)
				return SERV_STATUS_SERV_TEST_FAIL;
		}

		/* Control band0 as default setting */
		serv_test->ctrl_band_idx = TEST_DBDC_BAND0;

		/* Init test mode backup CR data struct */
		sys_ad_zero_mem(serv_test->test_bkcr,
			sizeof(struct test_bk_cr) * TEST_MAX_BKCR_NUM);

		/* Init test mode rx statistic data struct */
		sys_ad_zero_mem(serv_test->test_rx_statistic,
			sizeof(struct test_rx_stat) * TEST_DBDC_BAND_NUM);

		/* Init test mode rx statistic data struct */
		sys_ad_zero_mem(&serv_test->test_bstat,
			sizeof(struct test_band_state));
	} else {
		serv_test->test_winfo->dbdc_mode = TEST_DBDC_DISABLE;
		serv_test->test_winfo->hw_tx_enable = TEST_HWTX_DISABLE;
	}
	ret = mt_serv_init_op(serv_test->test_op);

	/* Init test mode control register data struct */
	sys_ad_zero_mem(&serv_test->test_reg,
			sizeof(struct test_register));

	/* Init test mode eeprom data struct */
	sys_ad_zero_mem(&serv_test->test_eprm,
			sizeof(struct test_eeprom));

	/* Init test mode eeprom data struct */
	serv_test->ctrl_band_idx = 0;

	/* TODO: factor out here */
	/* Common Part */

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

/* For test mode exit of service.git */
s_int32 mt_serv_exit_test(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char band_idx;

	if (!serv_test->engine_offload) {
		for (band_idx = TEST_DBDC_BAND0;
			band_idx < TEST_DBDC_BAND_NUM; band_idx++) {
			ret = mt_serv_release_config(
				&serv_test->test_config[band_idx], band_idx);
			if (ret != SERV_STATUS_SUCCESS)
				return SERV_STATUS_SERV_TEST_FAIL;
		}
	}

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_start(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;

	if (!serv_test->engine_offload) {
		ret = mt_engine_start(serv_test->test_winfo,
					&serv_test->test_backup,
					serv_test->test_config,
					serv_test->test_op,
					serv_test->test_bkcr,
					&serv_test->test_rx_statistic[0],
					serv_test->en_log);
	} else {
		ret = serv_test->test_op->op_set_test_mode_start(
			serv_test->test_winfo);
	}

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_stop(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;

	if (!serv_test->engine_offload) {
		ret = mt_engine_stop(serv_test->test_winfo,
					&serv_test->test_backup,
					serv_test->test_config,
					serv_test->test_op,
					serv_test->test_bkcr);
	} else {
		ret = serv_test->test_op->op_set_test_mode_abort(
			serv_test->test_winfo);
	}

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_channel(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *configs;
	struct test_wlan_info *winfos = serv_test->test_winfo;
	struct test_operation *ops = serv_test->test_op;
	struct serv_chip_cap *cap = &winfos->chip_cap;
	s_int32 ant_loop;
	u_char ant_mask = 0;
	u_int32 tx_stream_num = 0, max_stream_num = 0;
	s_int8 ch_offset = 0;
#if 0
	u_char tmp = 0;
#endif
	u_char pri_sel = 0, channel = 0, channel_2nd = 0;
	const s_int8 bw40_sel[] = { -2, 2};
	const s_int8 bw80_sel[] = { -6, -2, 2, 6};
	const s_int8 bw160_sel[] = { -14, -10, -6, -2, 2, 6, 10, 14};

	configs = &serv_test->test_config[ctrl_band_idx];

	/* update max stream num cap */
	max_stream_num = cap->mcs_nss.max_nss;
	if (IS_TEST_DBDC(winfos))
		max_stream_num /= 2;

	for (ant_loop = 0; ant_loop < max_stream_num; ant_loop++) {
		if (configs->tx_ant & (0x1 << ant_loop))
			ant_mask |= (0x1 << ant_loop);
	}

	/* update tx anteena config */
	configs->tx_ant = ant_mask;

	/*
	 * To get TX max stream number from TX antenna bit mask
	 * tx_sel=2 -> tx_stream_num=2
	 * tx_sel=4 -> tx_stream_num=3
	 * tx_sel=8 -> tx_stream_num=4
	 */

	/*
	 * tx stream for arbitrary tx ant bitmap
	 * (ex: tx_sel=5 -> tx_stream_num=3, not 2)
	 */
	for (ant_loop = max_stream_num; ant_loop > 0; ant_loop--) {
		if (ant_mask & BIT(ant_loop - 1)) {
			tx_stream_num = ant_loop;
			break;
		}
	}

	/* tx stream parameter sanity protection */
	tx_stream_num = tx_stream_num ? tx_stream_num : 1;
	tx_stream_num = (tx_stream_num <= max_stream_num)
		? tx_stream_num : max_stream_num;

	/* update tx stream num config */
	configs->tx_strm_num = tx_stream_num;

	SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_TRACE,
		("%s: tx_ant:0x%x, tx stream:%u\n",
		__func__, configs->tx_ant, configs->tx_strm_num));

	for (ant_loop = 0; ant_loop < max_stream_num; ant_loop++) {
		if (configs->rx_ant & (0x1 << ant_loop))
			ant_mask |= (0x1 << ant_loop);
	}

	/* fw need parameter rx stream path */
	configs->rx_ant = ant_mask;
	configs->rx_strm_pth = ant_mask;

	SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_TRACE,
		("%s: rx_ant:0x%x, rx path:0x%x\n", __func__,
			configs->rx_ant, configs->rx_strm_pth));

	/* read test config info */
	pri_sel = configs->pri_sel;
	channel = configs->channel;
	channel_2nd = configs->channel_2nd;

	switch (configs->bw) {
	case TEST_BW_20:
		configs->ctrl_ch = channel;
		if (configs->per_pkt_bw > configs->bw) {
			switch (configs->per_pkt_bw) {
			case TEST_BW_80:
				if (pri_sel >= 4)
					goto error;

				ch_offset = bw80_sel[pri_sel];
				break;
			case TEST_BW_160C:
			case TEST_BW_160NC:
				if (pri_sel >= 8)
					goto error;

				ch_offset = bw160_sel[pri_sel];
				break;
			default: /* BW_40 */
				if (pri_sel > 1)
					goto error;

				ch_offset = bw40_sel[pri_sel];
			}
		}

		break;
	case TEST_BW_40:
		if (pri_sel >= 2)
			goto error;

		configs->ctrl_ch = channel + bw40_sel[pri_sel];
		ch_offset = bw40_sel[pri_sel];

		break;

	case TEST_BW_160NC:
		if (pri_sel >= 8)
			goto error;

		if (!channel_2nd)
			goto error2;

#if 0
		/* swap control channel to be in order */
		if (channel_2nd < channel) {
			tmp = channel;
			channel = channel_2nd;
			channel_2nd = tmp;
		}
#endif
		/* TODO: bw80+80 primary select definition */
		if (pri_sel < 4) {
			configs->ctrl_ch = channel + bw80_sel[pri_sel];
			ch_offset = bw80_sel[pri_sel];
		} else {
			configs->ctrl_ch = channel + bw80_sel[pri_sel - 4];
			ch_offset = bw80_sel[pri_sel - 4];
		}

		break;

	case TEST_BW_80:
		if (pri_sel >= 4)
			goto error;

		configs->ctrl_ch = channel + bw80_sel[pri_sel];
		ch_offset = bw80_sel[pri_sel];

		break;

	case TEST_BW_160C:
		if (pri_sel >= 8)
			goto error;

		configs->ctrl_ch = channel + bw160_sel[pri_sel];
		ch_offset = bw160_sel[pri_sel];

		break;

	default:
		goto error3;
	}

	/* sanity check for channel parameter */
	if (((channel + ch_offset) <= 0) ||
		((channel - ch_offset) <= 0))
		goto error;

	/* update test config info */
	configs->pri_sel = pri_sel;
	configs->ch_offset = ch_offset;
	configs->channel = channel;
	configs->channel_2nd = channel_2nd;

	/* set channel */
	ret = ops->op_set_channel(winfos, ctrl_band_idx, configs);
	if (ret)
		goto error;

	return ret;

error:
	SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
		("%s: set channel fail, ", __func__));
	SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
		("control channel: %d|%d\n", configs->ctrl_ch,
		channel - ch_offset));
	return SERV_STATUS_OSAL_NET_FAIL_SET_CHANNEL;

error2:
	SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
		("%s: set channel fail, ", __func__));
	SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
		("second control channel is 0 for bw 80+80\n"));
	return SERV_STATUS_OSAL_NET_FAIL_SET_CHANNEL;

error3:
	SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
		("%s: set channel fail, ", __func__));
	SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
		("bw=%d is invalid\n", configs->bw));
	return SERV_STATUS_OSAL_NET_FAIL_SET_CHANNEL;
}

s_int32 mt_serv_set_tx_content(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *configs;

	configs = &serv_test->test_config[ctrl_band_idx];

	ret = serv_test->test_op->op_set_tx_content(
		serv_test->test_winfo,
		ctrl_band_idx,
		configs);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_tx_path(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *configs;

	configs = &serv_test->test_config[ctrl_band_idx];

	ret = serv_test->test_op->op_set_tx_path(
		serv_test->test_winfo,
		ctrl_band_idx,
		configs);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_rx_path(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *configs;

	configs = &serv_test->test_config[ctrl_band_idx];

	ret = serv_test->test_op->op_set_rx_path(
		serv_test->test_winfo,
		ctrl_band_idx,
		configs);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_submit_tx(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *configs;
	struct test_operation *ops = serv_test->test_op;
	struct test_wlan_info *winfos = serv_test->test_winfo;

	configs = &serv_test->test_config[ctrl_band_idx];

	if (!serv_test->engine_offload) {
		ret = mt_engine_subscribe_tx(ops, winfos, configs);
		if (ret)
			goto err_out;
	} else {
		/* TBD */
	}

err_out:
	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%04x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_revert_tx(struct service_test *serv_test)
{
	u_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *configs;
	struct test_operation *ops = serv_test->test_op;
	struct test_wlan_info *winfos = serv_test->test_winfo;

	configs = &serv_test->test_config[ctrl_band_idx];

	if (!serv_test->engine_offload) {
		ret = mt_engine_unsubscribe_tx(ops, winfos, configs);
		if (ret)
			goto err_out;
	} else {
		/* TBD */
	}

err_out:
	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_start_tx(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *configs;
	struct test_operation *ops = serv_test->test_op;
	struct test_wlan_info *winfos = serv_test->test_winfo;
	struct test_txpwr_param *pwr_param = NULL;

	configs = &serv_test->test_config[ctrl_band_idx];
	pwr_param = &configs->pwr_param;

	if (!pwr_param) {
		ret = SERV_STATUS_SERV_TEST_INVALID_NULL_POINTER;
		goto err_out;
	}

	if (configs->tx_mode < TEST_MODE_HE_SU)	{
		if (configs->mcs == 32
			&& configs->per_pkt_bw != TEST_BW_40
			&& configs->bw != TEST_BW_40) {
			ret = SERV_STATUS_SERV_TEST_INVALID_PARAM;
			SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: bandwidth must to be 40 at MCS 32\n", __func__));
			goto err_out;
		}
	}

	if (!serv_test->engine_offload) {
		ret = ops->op_set_tx_pwr(
			winfos, configs, ctrl_band_idx, pwr_param);
		if (ret)
			return ret;

		ret = mt_engine_calc_ipg_param_by_ipg(
				&serv_test->test_config[ctrl_band_idx]);
		if (ret)
			goto err_out;

		ret = mt_engine_start_tx(serv_test->test_winfo,
					&serv_test->test_config[ctrl_band_idx],
					serv_test->test_op, ctrl_band_idx);
	} else {
		ret = serv_test->test_op->op_start_tx(
			serv_test->test_winfo,
			ctrl_band_idx,
			&serv_test->test_config[ctrl_band_idx]);
	}

err_out:
	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_stop_tx(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;

	if (!serv_test->engine_offload) {
		ret = mt_engine_stop_tx(serv_test->test_winfo,
					&serv_test->test_config[ctrl_band_idx],
					serv_test->test_op, ctrl_band_idx);
	} else {
		ret = serv_test->test_op->op_stop_tx(
			serv_test->test_winfo,
			ctrl_band_idx);
	}

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_start_rx(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_wlan_info *winfos = serv_test->test_winfo;
	struct serv_chip_cap *cap = &winfos->chip_cap;
	struct test_configuration *configs;
	s_int32 ant_loop;
	u_char ant_mask = 0;
	u_int32 max_stream_num = 0;

	configs = &serv_test->test_config[ctrl_band_idx];

	if (!serv_test->engine_offload) {
		ret = mt_engine_set_auto_resp(serv_test->test_winfo,
				&serv_test->test_config[ctrl_band_idx],
				ctrl_band_idx, 1);
		if (ret)
			return ret;

		/* update max stream num cap */
		max_stream_num = cap->mcs_nss.max_nss;

		for (ant_loop = 0; ant_loop < max_stream_num; ant_loop++) {
			if (configs->rx_ant & (0x1 << ant_loop))
				ant_mask |= (0x1 << ant_loop);
		}

		/* fw need parameter rx stream path */
		configs->rx_ant = ant_mask;
		configs->rx_strm_pth = ant_mask;

		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_TRACE,
			("%s: rx_ant:0x%x, rx path:0x%x\n", __func__,
			configs->rx_ant, configs->rx_strm_pth));

		ret = mt_engine_start_rx(serv_test->test_winfo,
					&serv_test->test_config[ctrl_band_idx],
					serv_test->test_op, ctrl_band_idx);
	} else {
		ret = serv_test->test_op->op_start_rx(
			serv_test->test_winfo,
			ctrl_band_idx,
			configs);
	}

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_stop_rx(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;

	if (!serv_test->engine_offload) {
		ret = mt_engine_stop_rx(serv_test->test_winfo,
					&serv_test->test_config[ctrl_band_idx],
					serv_test->test_op, ctrl_band_idx);
	} else {
		ret = serv_test->test_op->op_stop_rx(
			serv_test->test_winfo,
			ctrl_band_idx);
	}

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_freq_offset(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *configs;
	struct test_operation *ops = serv_test->test_op;
	u_int32 rf_freq_offset;

	configs = &serv_test->test_config[ctrl_band_idx];

	rf_freq_offset = configs->rf_freq_offset;

	ret = ops->op_set_freq_offset(
			serv_test->test_winfo,
			rf_freq_offset,
			ctrl_band_idx);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_tx_power_operation(
	struct service_test *serv_test, u_int32 item)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *configs;
	struct test_wlan_info *winfos = serv_test->test_winfo;
	struct test_operation *ops = serv_test->test_op;
	struct test_txpwr_param *pwr_param = NULL;

	configs = &serv_test->test_config[ctrl_band_idx];
	if (!configs)
		return SERV_STATUS_SERV_TEST_INVALID_NULL_POINTER;

	pwr_param = &configs->pwr_param;
	if (!pwr_param)
		return SERV_STATUS_SERV_TEST_INVALID_NULL_POINTER;

	if (pwr_param->ant_idx >= TEST_ANT_NUM)
		goto error;

	switch (item) {
	case SERV_TEST_TXPWR_SET_PWR:
		configs->tx_pwr[pwr_param->ant_idx] = pwr_param->power;
		ret = ops->op_set_tx_pwr(
			winfos, configs, ctrl_band_idx, pwr_param);
		break;

	case SERV_TEST_TXPWR_GET_PWR:
		ret = ops->op_get_tx_pwr(
				winfos, configs, ctrl_band_idx,
				configs->channel, (u_char)pwr_param->ant_idx,
				&(pwr_param->power));
		break;

	case SERV_TEST_TXPWR_SET_PWR_INIT:
		/* TODO: */
		break;

	case SERV_TEST_TXPWR_SET_PWR_MAN:
		/* TODO: */
		break;

	default:
		return SERV_STATUS_SERV_TEST_INVALID_PARAM;
	}

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;

error:
	SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_TRACE,
		("%s: invalid parameter for ant_idx(0x%x).\n",
		__func__, pwr_param->ant_idx));
	return SERV_STATUS_SERV_TEST_INVALID_PARAM;
}

s_int32 mt_serv_get_freq_offset(
	struct service_test *serv_test, u_int32 *freq_offset)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_operation *ops;

	ops = serv_test->test_op;
	ret = ops->op_get_freq_offset(
			serv_test->test_winfo,
			ctrl_band_idx,
			freq_offset);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_get_cfg_on_off(
	struct service_test *serv_test,
	u_int32 type,
	u_int32 *result)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_operation *ops;

	ops = serv_test->test_op;
	ret = ops->op_get_cfg_on_off(
			serv_test->test_winfo,
			ctrl_band_idx,
			type,
			result);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_get_tx_tone_pwr(
	struct service_test *serv_test,
	u_int32 ant_idx,
	u_int32 *power)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_operation *ops;

	ops = serv_test->test_op;
	ret = ops->op_get_tx_tone_pwr(
			serv_test->test_winfo,
			ctrl_band_idx,
			ant_idx,
			power);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_get_thermal_val(
	struct service_test *serv_test,
	u_char band_idx,
	u_int32 *value)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *configs;
	struct test_operation *ops;

	configs = &serv_test->test_config[ctrl_band_idx];

	ops = serv_test->test_op;
	ret = ops->op_get_thermal_val(
			serv_test->test_winfo,
			configs,
			ctrl_band_idx,
			value);

	/* update config */
	configs->thermal_val = *value;

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_cal_bypass(
	struct service_test *serv_test,
	u_int32 cal_item)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_operation *ops;

	ops = serv_test->test_op;
	ret = ops->op_set_cal_bypass(
			serv_test->test_winfo,
			ctrl_band_idx,
			cal_item);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_dpd(
	struct service_test *serv_test,
	u_int32 on_off,
	u_int32 wf_sel)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *ops;

	ops = serv_test->test_op;
	ret = ops->op_set_dpd(
			serv_test->test_winfo,
			on_off,
			wf_sel);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_tssi(
	struct service_test *serv_test,
	u_int32 on_off,
	u_int32 wf_sel)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *ops;

	ops = serv_test->test_op;
	ret = ops->op_set_tssi(
			serv_test->test_winfo,
			on_off,
			wf_sel);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_rdd_on_off(
	struct service_test *serv_test,
	u_int32 rdd_num,
	u_int32 rdd_sel,
	u_int32 enable)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_operation *ops;
	struct test_wlan_info *winfos;

	ops = serv_test->test_op;
	winfos = serv_test->test_winfo;

	SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_OFF,
		("%s(): ctrl_band_idx %d, enable %d\n",
		__func__, ctrl_band_idx, enable));

	if (ops->op_set_tr_mac)
		ret = ops->op_set_tr_mac(winfos, SERV_TEST_MAC_RX,
		enable, ctrl_band_idx);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: op_set_tr_mac, err=0x%08x\n", __func__, ret));

	if (ops->op_set_rdd_test)
		ops->op_set_rdd_test(winfos, rdd_num, rdd_sel, enable);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: op_set_rdd_test, err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_off_ch_scan(
	struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *configs;
	struct test_wlan_info *winfos = serv_test->test_winfo;
	struct test_operation *ops = serv_test->test_op;
	struct test_off_ch_param *param = NULL;

	configs = &serv_test->test_config[ctrl_band_idx];
	if (!configs)
		return SERV_STATUS_SERV_TEST_INVALID_NULL_POINTER;

	param = &configs->off_ch_param;
	if (!param)
		return SERV_STATUS_SERV_TEST_INVALID_NULL_POINTER;

	ret = ops->op_set_off_ch_scan(
		winfos, configs, ctrl_band_idx, param);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}


s_int32 mt_serv_set_icap_start(
	struct service_test *serv_test,
	struct hqa_rbist_cap_start *icap_info)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *op = serv_test->test_op;
	struct test_wlan_info *winfos = serv_test->test_winfo;

	if (op->op_set_icap_start)
		ret = op->op_set_icap_start(winfos, (u_int8 *)icap_info);
	else
		ret = SERV_STATUS_SERV_TEST_NOT_SUPPORTED;

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err = 0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_get_icap_status(
	struct service_test *serv_test,
	s_int32 *icap_stat)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *op = serv_test->test_op;
	struct test_wlan_info *winfos = serv_test->test_winfo;

	if (op->op_get_icap_status)
		ret = op->op_get_icap_status(winfos, icap_stat);
	else
		ret = SERV_STATUS_SERV_TEST_NOT_SUPPORTED;

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err = 0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_get_icap_max_data_len(
	struct service_test *serv_test,
	u_long *max_data_len)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *op = serv_test->test_op;
	struct test_wlan_info *winfos = serv_test->test_winfo;

	if (op->op_get_icap_max_data_len)
		ret = op->op_get_icap_max_data_len(winfos, max_data_len);
	else
		ret = SERV_STATUS_SERV_TEST_NOT_SUPPORTED;

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err = 0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_get_icap_data(
	struct service_test *serv_test,
	s_int32 *icap_cnt,
	s_int32 *icap_data,
	u_int32 wf_num,
	u_int32 iq_type)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *op = serv_test->test_op;
	struct test_wlan_info *winfos = serv_test->test_winfo;

	if (op->op_get_icap_data)
		ret = op->op_get_icap_data(winfos, icap_cnt
					, icap_data, wf_num, iq_type);
	else
		ret = SERV_STATUS_SERV_TEST_NOT_SUPPORTED;

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err = 0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_get_recal_cnt(
	struct service_test *serv_test,
	u_int32 *recal_cnt,
	u_int32 *recal_dw_num)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *ops;

	ops = serv_test->test_op;

	ret = ops->op_get_recal_cnt(
			serv_test->test_winfo,
			recal_cnt,
			recal_dw_num);

	return ret;
}

s_int32 mt_serv_get_recal_content(
	struct service_test *serv_test,
	u_int32 *content)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *ops;

	ops = serv_test->test_op;

	ret = ops->op_get_recal_content(
			serv_test->test_winfo,
			content);

	return ret;
}

s_int32 mt_serv_get_rxv_cnt(
	struct service_test *serv_test,
	u_int32 *rxv_cnt,
	u_int32 *rxv_dw_num)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_wlan_info *winfos;
	struct test_operation *ops;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;

	u_int32 byte_cnt = 0;

	if (!serv_test->engine_offload) {
		winfos = serv_test->test_winfo;

		/* Note: only support single rxv count report */
		*rxv_cnt = 1;

		/* query rxv byte count */
		ret = net_ad_get_rxv_cnt(winfos, ctrl_band_idx, &byte_cnt);
	} else {
		ops = serv_test->test_op;
		ret = ops->op_get_rxv_cnt(
			serv_test->test_winfo,
			rxv_cnt,
			rxv_dw_num);
	}

	return ret;
}

s_int32 mt_serv_get_rxv_content(
	struct service_test *serv_test,
	u_int32 dw_cnt,
	u_int32 *content)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_wlan_info *winfos;
	struct test_operation *ops;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;

	if (!serv_test->engine_offload) {
		/* query rxv content */
		winfos = serv_test->test_winfo;
		ret = net_ad_get_rxv_content(winfos, ctrl_band_idx, content);
	} else {
		ops = serv_test->test_op;
		ret = ops->op_get_rxv_content(
			serv_test->test_winfo,
			dw_cnt,
			content);
	}

	return ret;
}

s_int32 mt_serv_get_rdd_cnt(
	struct service_test *serv_test,
	u_int32 *rdd_cnt,
	u_int32 *rdd_dw_num)
{
	struct test_operation *ops;
	s_int32 ret = SERV_STATUS_SUCCESS;

	ops = serv_test->test_op;
	ret = ops->op_get_rdd_cnt(
		serv_test->test_winfo,
		rdd_cnt,
		rdd_dw_num);

	return ret;
}

s_int32 mt_serv_get_rdd_content(
	struct service_test *serv_test,
	u_int32 *content,
	u_int32 *total_cnt)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *ops;

	ops = serv_test->test_op;
	ret = ops->op_get_rdd_content(
		serv_test->test_winfo,
		content, total_cnt);

	return ret;
}

s_int32 mt_serv_reset_txrx_counter(
	struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_wlan_info *winfos;
	struct test_configuration *config_band0;
	struct test_configuration *config_band1;
	struct test_rx_stat *test_rx_st;
	u_int8 ant_idx = 0, band_idx = 0;

	winfos = serv_test->test_winfo;
	config_band0 = &serv_test->test_config[TEST_DBDC_BAND0];
	config_band1 = &serv_test->test_config[TEST_DBDC_BAND1];

	for (band_idx = TEST_DBDC_BAND0;
		band_idx < TEST_DBDC_BAND_NUM; band_idx++) {
		test_rx_st = serv_test->test_rx_statistic + band_idx;
		sys_ad_zero_mem(test_rx_st,
				sizeof(struct test_rx_stat));

		for (ant_idx = 0; ant_idx < TEST_ANT_NUM; ant_idx++) {
			test_rx_st->rx_st_path[ant_idx].rssi = 0xFF;
			test_rx_st->rx_st_path[ant_idx].rcpi = 0xFF;
			test_rx_st->rx_st_path[ant_idx].fagc_ib_rssi = 0xFF;
			test_rx_st->rx_st_path[ant_idx].fagc_wb_rssi = 0xFF;
		}
	}

	config_band0->tx_stat.tx_done_cnt = 0;
	if (IS_TEST_DBDC(winfos))
		config_band1->tx_stat.tx_done_cnt = 0;

	ret = serv_test->test_op->op_reset_txrx_counter(
			serv_test->test_winfo);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_rx_vector_idx(
	struct service_test *serv_test, u_int32 group1, u_int32 group2)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *ops;

	ops = serv_test->test_op;
	ret = ops->op_set_rx_vector_idx(
		serv_test->test_winfo,
		serv_test->ctrl_band_idx,
		group1,
		group2);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_fagc_rssi_path(
	struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *ops;

	ops = serv_test->test_op;
	ret = ops->op_set_fagc_rssi_path(
		serv_test->test_winfo,
		serv_test->ctrl_band_idx,
		serv_test->test_config[TEST_DBDC_BAND0].fagc_path);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_get_rx_stat_leg(
	struct service_test *serv_test,
	struct test_rx_stat_leg *rx_stat)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *ops;

	ops = serv_test->test_op;
	ret = ops->op_get_rx_stat_leg(
		serv_test->test_winfo, rx_stat);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_get_rx_stat(
	struct service_test *serv_test,
	u_int8 band_idx,
	u_int8 blk_idx,
	u_int8 test_rx_stat_cat,
	struct test_rx_stat_u *st)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *ops;
	struct test_wlan_info *winfos;
	struct test_rx_stat *stat;
	boolean dbdc_mode = FALSE;

	ops = serv_test->test_op;
	winfos = serv_test->test_winfo;
	stat = &serv_test->test_rx_statistic[band_idx];

	/* check dbdc mode condition */
	dbdc_mode = IS_TEST_DBDC(serv_test->test_winfo);

	/* sanity check for band index param */
	if ((!dbdc_mode) && (band_idx != TEST_DBDC_BAND0))
		goto error1;

	switch (test_rx_stat_cat) {
	case TEST_RX_STAT_BAND:
		ret = ops->op_get_rx_stat_band(
		serv_test->test_winfo,
		band_idx,
		blk_idx,
		stat->rx_st_band + blk_idx);

		sys_ad_move_mem(st, stat->rx_st_band + blk_idx,
				sizeof(struct test_rx_stat_band_info));
		break;
	case TEST_RX_STAT_PATH:
		ret = ops->op_get_rx_stat_path(
		serv_test->test_winfo,
		band_idx,
		blk_idx,
		stat->rx_st_path + blk_idx);

		sys_ad_move_mem(st, stat->rx_st_path + blk_idx,
				sizeof(struct test_rx_stat_path_info));
		break;
	case TEST_RX_STAT_USER:
		ret = ops->op_get_rx_stat_user(
		serv_test->test_winfo,
		band_idx,
		blk_idx,
		stat->rx_st_user + blk_idx);

		sys_ad_move_mem(st, stat->rx_st_user + blk_idx,
				sizeof(struct test_rx_stat_user_info));
		break;
	case TEST_RX_STAT_COMM:
		ret = ops->op_get_rx_stat_comm(
		serv_test->test_winfo,
		band_idx,
		blk_idx,
		&stat->rx_st_comm);

		sys_ad_move_mem(st, &stat->rx_st_comm,
				sizeof(struct test_rx_stat_comm_info));
		break;
	default:
		break;
	}

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;

error1:
	SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
		("%s: invalid band index for non-dbdc mode.\n",
		__func__));
	return ret;
}

s_int32 mt_serv_get_capability(
	struct service_test *serv_test,
	struct test_capability *capability)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *op = serv_test->test_op;
	struct test_wlan_info *winfo = serv_test->test_winfo;

	ret = op->op_get_capability(winfo, capability);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_calibration_test_mode(
	struct service_test *serv_test, u_char mode)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *op = serv_test->test_op;
	struct test_wlan_info *winfo = serv_test->test_winfo;

	ret = op->op_calibration_test_mode(winfo, mode);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_do_cal_item(
	struct service_test *serv_test, u_int32 item)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;

	ret = serv_test->test_op->op_do_cal_item(
		serv_test->test_winfo,
		item, ctrl_band_idx);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_band_mode(
	struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *ops;

	ops = serv_test->test_op;

	if (!serv_test->engine_offload) {
		ret = net_ad_set_band_mode(
			serv_test->test_winfo,
			&serv_test->test_bstat);

		if (ret)
			SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
				("%s: err=0x%08x\n", __func__, ret));
	} else {
		ret = ops->op_set_band_mode(
			serv_test->test_winfo,
			&serv_test->test_bstat);
	}

	return ret;
}

s_int32 mt_serv_get_band_mode(
	struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	u_int32 band_type;
	struct test_operation *ops;

	ops = serv_test->test_op;

	if (!serv_test->engine_offload) {
		/*
		 * DLL will query two times per band0/band1 if DBDC chip set.
		 * 0: no this band
		 * 1: 2.4G
		 * 2: 5G
		 * 3. 2.4G+5G
		 */
		if (IS_TEST_DBDC(serv_test->test_winfo))
			band_type = (ctrl_band_idx == TEST_DBDC_BAND0)
					? TEST_BAND_TYPE_G : TEST_BAND_TYPE_A;
		else {
			/* Always report 2.4+5G*/
			band_type = TEST_BAND_TYPE_ALL;

			/*
			 * If IS_TEST_DBDC=0,
			 * band_idx should not be 1 so return band_mode=0
			 */
			if (ctrl_band_idx == TEST_DBDC_BAND1)
				band_type = TEST_BAND_TYPE_UNUSE;
		}

		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: band_type=%u\n", __func__, band_type));
	} else {
		ret = ops->op_set_band_mode(
			serv_test->test_winfo,
			&serv_test->test_bstat);

		if (ctrl_band_idx == TEST_DBDC_BAND0)
			band_type = TEST_BAND_TYPE_ALL;
		else {
			if (serv_test->test_bstat.band_mode ==
				TEST_BAND_MODE_DUAL)
				band_type = TEST_BAND_TYPE_ALL;
			else
				band_type = TEST_BAND_TYPE_UNUSE;
		}
	}

	BSTATE_SET_PARAM(serv_test, band_type, band_type);

	return ret;
}

s_int32 mt_serv_log_on_off(
	struct service_test *serv_test, u_int32 log_type,
	u_int32 log_ctrl, u_int32 log_size)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *ops;

	ops = serv_test->test_op;
	ret = ops->op_log_on_off(
		serv_test->test_winfo,
		serv_test->ctrl_band_idx,
		log_type,
		log_ctrl,
		log_size);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}


s_int32 mt_serv_set_cfg_on_off(
	struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_configuration *configs;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_operation *ops;

	configs = &serv_test->test_config[ctrl_band_idx];

	ops = serv_test->test_op;
	ret = ops->op_set_cfg_on_off(
			serv_test->test_winfo,
			(u_int8)configs->log_type,
			(u_int8)configs->log_enable,
			ctrl_band_idx);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_rx_filter_pkt_len(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_configuration *configs;
	struct test_operation *ops;

	configs = &serv_test->test_config[serv_test->ctrl_band_idx];
	ops = serv_test->test_op;
	ret = ops->op_set_rx_filter_pkt_len(
		serv_test->test_winfo,
		configs->rx_filter_en,
		serv_test->ctrl_band_idx,
		configs->rx_filter_pkt_len);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_low_power(
	struct service_test *serv_test, u_int32 control)
{
	s_int32 ret = SERV_STATUS_SUCCESS;

	ret = net_ad_set_low_power(serv_test->test_winfo, control);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_get_antswap_capability(
	struct service_test *serv_test, u_int32 *antswap_support)
{
	s_int32 ret = SERV_STATUS_SUCCESS;

	struct test_operation *ops;

	ops = serv_test->test_op;

	ret = ops->op_get_antswap_capability(
			serv_test->test_winfo,
			antswap_support);

	return ret;
}

s_int32 mt_serv_set_antswap(
	struct service_test *serv_test, u_int32 ant)
{
	s_int32 ret = SERV_STATUS_SUCCESS;

	struct test_operation *ops;

	ops = serv_test->test_op;

	ret = ops->op_set_antswap(
			serv_test->test_winfo,
			ant);

	return ret;
}

s_int32 mt_serv_reg_eprm_operation(
	struct service_test *serv_test, u_int32 item)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_operation *ops;

	ops = serv_test->test_op;

	switch (item) {
	case SERV_TEST_REG_MAC_READ:
		if (!serv_test->engine_offload) {
		ret = net_ad_read_mac_bbp_reg(serv_test->test_winfo,
						&serv_test->test_reg);
		} else {
			ops = serv_test->test_op;
			ret = ops->op_read_bulk_mac_bbp_reg(
				serv_test->test_winfo,
				&serv_test->test_reg);
		}
		break;

	case SERV_TEST_REG_MAC_WRITE:
		if (!serv_test->engine_offload) {

		ret = net_ad_write_mac_bbp_reg(serv_test->test_winfo,
						&serv_test->test_reg);
		} else {
			ops = serv_test->test_op;
			ret = ops->op_write_mac_bbp_reg(serv_test->test_winfo,
				&serv_test->test_reg);
		}
		break;

	case SERV_TEST_REG_MAC_READ_BULK:
		if (!serv_test->engine_offload) {
			ret = net_ad_read_bulk_mac_bbp_reg(
				serv_test->test_winfo,
				&serv_test->test_config[TEST_DBDC_BAND0],
				&serv_test->test_reg);
		} else {
			ops = serv_test->test_op;
			ret = ops->op_read_bulk_mac_bbp_reg(
				serv_test->test_winfo,
				&serv_test->test_reg);
		}
		break;

	case SERV_TEST_REG_RF_READ_BULK:
		if (!serv_test->engine_offload) {
		ret = net_ad_read_bulk_rf_reg(serv_test->test_winfo,
			&serv_test->test_reg);
		} else {
			ops = serv_test->test_op;
			ret = ops->op_read_bulk_rf_reg(serv_test->test_winfo,
				&serv_test->test_reg);
		}
		break;

	case SERV_TEST_REG_RF_WRITE_BULK:
		if (!serv_test->engine_offload) {
		ret = net_ad_write_bulk_rf_reg(serv_test->test_winfo,
			&serv_test->test_reg);
		} else {
			ops = serv_test->test_op;
			ret = ops->op_write_bulk_rf_reg(serv_test->test_winfo,
				&serv_test->test_reg);
		}
		break;

	case SERV_TEST_REG_CA53_READ:
		net_ad_read_ca53_reg(&serv_test->test_reg);
		break;

	case SERV_TEST_REG_CA53_WRITE:
		net_ad_write_ca53_reg(&serv_test->test_reg);
		break;

	case SERV_TEST_EEPROM_READ:
		ret = net_ad_read_write_eeprom(serv_test->test_winfo,
			&serv_test->test_eprm,
			TRUE);
		break;

	case SERV_TEST_EEPROM_WRITE:
		ret = net_ad_read_write_eeprom(serv_test->test_winfo,
			&serv_test->test_eprm,
			FALSE);
		break;

	case SERV_TEST_EEPROM_READ_BULK:
		if (!serv_test->engine_offload) {
		ret = net_ad_read_write_bulk_eeprom(serv_test->test_winfo,
			&serv_test->test_eprm,
			TRUE);
		} else {
			ops = serv_test->test_op;
			ret = ops->op_read_bulk_eeprom(serv_test->test_winfo,
				&serv_test->test_eprm);
		}
		break;

	case SERV_TEST_EEPROM_WRITE_BULK:
		ret = net_ad_read_write_bulk_eeprom(serv_test->test_winfo,
			&serv_test->test_eprm,
			FALSE);
		break;

	case SERV_TEST_EEPROM_GET_FREE_EFUSE_BLOCK:
		ret = net_ad_get_free_efuse_block(serv_test->test_winfo,
			&serv_test->test_eprm);
		break;

	default:
		return SERV_STATUS_SERV_TEST_INVALID_PARAM;
	}

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_mps_operation(
	struct service_test *serv_test, u_int32 item)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;

	if (!serv_test->engine_offload) {
		switch (item) {
		case SERV_TEST_MPS_START_TX:
			ret = net_ad_mps_tx_operation(serv_test->test_winfo,
				&serv_test->test_config[ctrl_band_idx],
				TRUE);

			if (ret)
				return ret;

			ret = mt_engine_start_tx(serv_test->test_winfo,
					&serv_test->test_config[ctrl_band_idx],
					serv_test->test_op, ctrl_band_idx);
			break;

		case SERV_TEST_MPS_STOP_TX:
			ret = net_ad_mps_tx_operation(serv_test->test_winfo,
				&serv_test->test_config[ctrl_band_idx],
				FALSE);
			break;

		default:
			return SERV_STATUS_SERV_TEST_INVALID_PARAM;
		}
	} else {
		switch (item) {
		case SERV_TEST_MPS_START_TX:
			ret = serv_test->test_op->op_mps_start(
				serv_test->test_winfo,
				ctrl_band_idx);
			break;

		case SERV_TEST_MPS_STOP_TX:
			ret = serv_test->test_op->op_stop_tx(
				serv_test->test_winfo,
				ctrl_band_idx);
			break;

		default:
			return SERV_STATUS_SERV_TEST_INVALID_PARAM;
		}

	}

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_get_chipid(
	struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;

	ret = serv_test->test_op->op_get_chipid(
			serv_test->test_winfo);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_mps_set_nss(
	struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *test_config;
	struct test_mps_cb *mps_cb;
	u_int32 len;

	test_config = &serv_test->test_config[ctrl_band_idx];
	mps_cb = &test_config->mps_cb;
	len = mps_cb->mps_cnt;

	ret = serv_test->test_op->op_mps_set_nss(
			serv_test->test_winfo,
			mps_cb->mps_cnt,
			mps_cb->mps_setting);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_mps_set_per_packet_bw(
	struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *test_config;
	struct test_mps_cb *mps_cb;
	u_int32 len;

	test_config = &serv_test->test_config[ctrl_band_idx];
	mps_cb = &test_config->mps_cb;
	len = mps_cb->mps_cnt;

	ret = serv_test->test_op->op_mps_set_per_packet_bw(
			serv_test->test_winfo,
			mps_cb->mps_cnt,
			mps_cb->mps_setting);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_mps_set_packet_count(
	struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *test_config;
	struct test_mps_cb *mps_cb;
	u_int32 len;

	test_config = &serv_test->test_config[ctrl_band_idx];
	mps_cb = &test_config->mps_cb;
	len = mps_cb->mps_cnt;

	ret = serv_test->test_op->op_mps_set_packet_count(
			serv_test->test_winfo,
			mps_cb->mps_cnt,
			mps_cb->mps_setting);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}


s_int32 mt_serv_mps_set_payload_length(
	struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *test_config;
	struct test_mps_cb *mps_cb;
	u_int32 len;

	test_config = &serv_test->test_config[ctrl_band_idx];
	mps_cb = &test_config->mps_cb;
	len = mps_cb->mps_cnt;

	ret = serv_test->test_op->op_mps_set_payload_length(
			serv_test->test_winfo,
			mps_cb->mps_cnt,
			mps_cb->mps_setting);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}


s_int32 mt_serv_mps_set_power_gain(
	struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *test_config;
	struct test_mps_cb *mps_cb;
	u_int32 len;

	test_config = &serv_test->test_config[ctrl_band_idx];
	mps_cb = &test_config->mps_cb;
	len = mps_cb->mps_cnt;

	ret = serv_test->test_op->op_mps_set_power_gain(
			serv_test->test_winfo,
			mps_cb->mps_cnt,
			mps_cb->mps_setting);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_mps_set_seq_data(
	struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *test_config;
	struct test_mps_cb *mps_cb;
	u_int32 len;

	test_config = &serv_test->test_config[ctrl_band_idx];
	mps_cb = &test_config->mps_cb;
	len = mps_cb->mps_cnt;

	if (!serv_test->engine_offload) {

	} else {
		ret = serv_test->test_op->op_mps_set_seq_data(
			serv_test->test_winfo,
			mps_cb->mps_cnt,
			mps_cb->mps_setting);
	}

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_tmr(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;

	ret = net_ad_set_tmr(serv_test->test_winfo, &serv_test->test_tmr);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_preamble(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	u_char tx_mode = serv_test->test_config[ctrl_band_idx].tx_mode;

	if (!serv_test->engine_offload) {

	} else {
		ret = serv_test->test_op->op_set_preamble(
			serv_test->test_winfo,
			tx_mode);
	}

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_rate(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	u_char mcs = serv_test->test_config[ctrl_band_idx].mcs;

	if (!serv_test->engine_offload) {

	} else {
		ret = serv_test->test_op->op_set_rate(
			serv_test->test_winfo,
			mcs);
	}

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_system_bw(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	u_char sys_bw = serv_test->test_config[ctrl_band_idx].bw;

	if (!serv_test->engine_offload) {

	} else {
		ret = serv_test->test_op->op_set_system_bw(
			serv_test->test_winfo,
			sys_bw);
	}

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_set_per_pkt_bw(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	u_char per_pkt_bw = serv_test->test_config[ctrl_band_idx].per_pkt_bw;

	if (!serv_test->engine_offload) {

	} else {
		ret = serv_test->test_op->op_set_per_pkt_bw(
			serv_test->test_winfo,
			per_pkt_bw);
	}

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_dbdc_tx_tone(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *configs;

	configs = &serv_test->test_config[ctrl_band_idx];

	ret = serv_test->test_op->op_dbdc_tx_tone(
			serv_test->test_winfo,
			ctrl_band_idx,
			configs);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_dbdc_tx_tone_pwr(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *configs;

	configs = &serv_test->test_config[ctrl_band_idx];

	ret = serv_test->test_op->op_dbdc_tx_tone_pwr(
			serv_test->test_winfo,
			ctrl_band_idx,
			configs);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_dbdc_continuous_tx(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	u_char ctrl_band_idx = serv_test->ctrl_band_idx;
	struct test_configuration *configs;

	configs = &serv_test->test_config[ctrl_band_idx];

	ret = serv_test->test_op->op_dbdc_continuous_tx(
			serv_test->test_winfo,
			ctrl_band_idx,
			configs);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_get_tx_info(struct service_test *serv_test)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_configuration *test_configs_band0;
	struct test_configuration *test_configs_band1;

	test_configs_band0 = &serv_test->test_config[TEST_DBDC_BAND0];
	test_configs_band1 = &serv_test->test_config[TEST_DBDC_BAND1];

	ret = serv_test->test_op->op_get_tx_info(
		serv_test->test_winfo,
		test_configs_band0,
		test_configs_band1);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_get_wf_path_comb(
	struct service_test *serv_test,
	u_int8 band_idx,
	boolean dbdc_mode_en,
	u_int8 *path,
	u_int8 *path_len
)
{
	s_int32 ret = SERV_STATUS_SUCCESS;
	struct test_configuration *test_configs;

	test_configs = &serv_test->test_config[TEST_DBDC_BAND0];

	ret = serv_test->test_op->op_get_wf_path_comb(
			serv_test->test_winfo,
			band_idx,
			dbdc_mode_en,
			path,
			path_len);

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}

s_int32 mt_serv_main(struct service_test *serv_test, u_int32 test_item)
{
	s_int32 ret = SERV_STATUS_SUCCESS;

	switch (test_item) {
	case SERV_TEST_ITEM_INIT:
		ret = mt_serv_init_test(serv_test);
		break;

	case SERV_TEST_ITEM_EXIT:
		ret = mt_serv_exit_test(serv_test);
		break;

	case SERV_TEST_ITEM_START:
		ret = mt_serv_start(serv_test);
		break;

	case SERV_TEST_ITEM_STOP:
		ret = mt_serv_stop(serv_test);
		break;

	case SERV_TEST_ITEM_START_TX:
		ret = mt_serv_start_tx(serv_test);
		break;

	case SERV_TEST_ITEM_STOP_TX:
		ret = mt_serv_stop_tx(serv_test);
		break;

	case SERV_TEST_ITEM_START_RX:
		ret = mt_serv_start_rx(serv_test);
		break;

	case SERV_TEST_ITEM_STOP_RX:
		ret = mt_serv_stop_rx(serv_test);
		break;

	default:
		return SERV_STATUS_SERV_TEST_NOT_SUPPORTED;
	}

	if (ret)
		SERV_LOG(SERV_DBG_CAT_TEST, SERV_DBG_LVL_ERROR,
			("%s: err=0x%08x\n", __func__, ret));

	return ret;
}
