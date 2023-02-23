// SPDX-License-Identifier: GPL-2.0
/************************************************************************
 * Copyright (c) 2012-2021, Focaltech Systems (R), All Rights Reserved.
 *
 * File Name: Focaltech_test_ft5662.c
 *
 * Author: Focaltech Driver Team
 *
 * Created: 2021-08-16
 *
 * Abstract:
 *
 ************************************************************************/

/*****************************************************************************
 * included header files
 *****************************************************************************/
#include "../focaltech_test.h"

/*****************************************************************************
 * private constant and macro definitions using #define
 *****************************************************************************/

/*****************************************************************************
 * Static function
 *****************************************************************************/

static int short_test_ch_to_all(
	struct fts_test *tdata, int *res, u8 *ab_ch, bool *result)
{
	int ret = 0;
	int i = 0;
	int min_cc = tdata->ic.mc_sc.thr.basic.short_cc;
	int ch_num = tdata->sc_node.tx_num + tdata->sc_node.rx_num;
	int byte_num = ch_num * 2;
	u8 ab_ch_num = 0;

	FTS_TEST_DBG("short test:channel to all other\n");
	/*get resistance data*/
	ret = short_get_adc_data_mc(TEST_RETVAL_AA, byte_num, &res[0],
			FACTROY_REG_SHORT2_CA);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get weak short data fail,ret:%d\n", ret);
		return ret;
	}

	*result = true;
	for (i = 0; i < ch_num; i++) {
		if (res[i] < min_cc) {
			ab_ch_num++;
			ab_ch[ab_ch_num] = i;
			*result = false;
		}
	}

	if (ab_ch_num) {
		print_buffer(res, ch_num, ch_num);
		ab_ch[0] = ab_ch_num;
		pr_info("[FTS_TS]ab_ch:");
		for (i = 0; i < ab_ch_num + 1; i++)
			pr_info("%2d ", ab_ch[i]);
		pr_info("\n");
	}

	return 0;
}

static int short_test_ch_to_gnd(
	struct fts_test *tdata, int *res, u8 *ab_ch, bool *result)
{
	int ret = 0;
	int i = 0;
	int min_cg = tdata->ic.mc_sc.thr.basic.short_cg;
	int tx_num = tdata->sc_node.tx_num;
	int byte_num = 0;
	u8 ab_ch_num = 0;
	bool is_cg_short = false;

	FTS_TEST_DBG("short test:channel to gnd\n");
	ab_ch_num = ab_ch[0];
	ret = fts_test_write(FACTROY_REG_SHORT2_AB_CH, ab_ch, ab_ch_num + 1);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write abnormal channel fail\n");
		return ret;
	}

	/*get resistance data*/
	byte_num = ab_ch_num * 2;
	ret = short_get_adc_data_mc(TEST_RETVAL_AA, byte_num, &res[0],
			FACTROY_REG_SHORT2_CG);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get weak short data fail,ret:%d\n", ret);
		return ret;
	}

	*result = true;
	for (i = 0; i < ab_ch_num; i++) {
		if (res[i] < min_cg) {
			*result = false;
			if (!is_cg_short) {
				FTS_TEST_SAVE_INFO("\nGND Short:\n");
				is_cg_short = true;
			}

			if (ab_ch[i + 1] <= tx_num)
				FTS_TEST_SAVE_INFO("Tx%d with GND:", ab_ch[i + 1]);
			else
				FTS_TEST_SAVE_INFO("Rx%d with GND:", (ab_ch[i + 1] - tx_num));
			FTS_TEST_SAVE_INFO("%d(K)\n", res[i]);
		}
	}

	return 0;
}

static int short_test_ch_to_ch(
	struct fts_test *tdata, int *res, u8 *ab_ch, bool *result)
{
	int ret = 0;
	int i = 0;
	int j = 0;
	int adc_cnt = 0;
	int min_cc = tdata->ic.mc_sc.thr.basic.short_cc;
	int tx_num = tdata->sc_node.tx_num;
	int ch_num = tdata->sc_node.tx_num + tdata->sc_node.rx_num;
	int byte_num = 0;
	int tmp_num = 0;
	u8 ab_ch_num = 0;
	bool is_cc_short = false;

	FTS_TEST_DBG("short test:channel to channel\n");
	ab_ch_num = ab_ch[0];
	if (ab_ch_num < 2) {
		FTS_TEST_DBG("abnormal channel number<2, not run ch_ch test");
		return ret;
	}

	ret = fts_test_write(FACTROY_REG_SHORT2_AB_CH, ab_ch, ab_ch_num + 1);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write abnormal channel fail\n");
		return ret;
	}

	/* get resistance data */
	/* channel to channel: num * (num - 1) / 2, max. node_num */
	tmp_num = ab_ch_num * (ab_ch_num - 1) / 2;
	tmp_num = (tmp_num > ch_num) ? ch_num : tmp_num;
	byte_num = tmp_num * 2;
	ret = short_get_adc_data_mc(TEST_RETVAL_AA, byte_num, &res[0],
			FACTROY_REG_SHORT2_CC);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get weak short data fail,ret:%d\n", ret);
		return ret;
	}

	*result = true;
	for (i = 0; i < ab_ch_num; i++) {
		for (j = i + 1; j < ab_ch_num; j++) {
			if (adc_cnt >= tmp_num)
				break;

			if (res[adc_cnt] < min_cc) {
				*result = false;
				if (!is_cc_short) {
					FTS_TEST_SAVE_INFO("\nMutual Short:\n");
					is_cc_short = true;
				}

				if (ab_ch[i + 1] <= tx_num)
					FTS_TEST_SAVE_INFO("Tx%d with", (ab_ch[i + 1]));
				else
					FTS_TEST_SAVE_INFO("Rx%d with", (ab_ch[i + 1] - tx_num));

				if (ab_ch[j + 1] <= tx_num)
					FTS_TEST_SAVE_INFO(" Tx%d", (ab_ch[j + 1]));
				else
					FTS_TEST_SAVE_INFO(" Rx%d", (ab_ch[j + 1] - tx_num));
				FTS_TEST_SAVE_INFO(":%d(K)\n", res[adc_cnt]);
			}
			adc_cnt++;
		}
	}

	return 0;
}

static int get_cb_ft5662(int *cb_buf, int byte_num, bool is_cf)
{
	int ret = 0;
	int i = 0;
	u8 cb[SC_NUM_MAX] = { 0 };

	if (byte_num > SC_NUM_MAX) {
		FTS_TEST_SAVE_ERR("CB byte(%d)>max(%d)", byte_num, SC_NUM_MAX);
		return -EINVAL;
	}

	ret = fts_test_write_reg(FACTORY_REG_MC_SC_CB_H_ADDR_OFF, 0);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write cb_h addr offset fail\n");
		return ret;
	}


	ret = fts_test_write_reg(FACTORY_REG_MC_SC_CB_ADDR_OFF, 0);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write cb addr offset fail\n");
		return ret;
	}

	ret = fts_test_read(FACTORY_REG_MC_SC_CB_ADDR, cb, byte_num);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read cb fail\n");
		return ret;
	}

	for (i = 0; i < byte_num; i++) {
		if (is_cf)
			cb_buf[i] = (cb[i] & 0x80) ? -((cb[i] & 0x3F)) : (cb[i] & 0x3F);
		else
			cb_buf[i] = (cb[i] & 0x80) ? -((cb[i] & 0x7F)) : (cb[i] & 0x7F);
	}

	return 0;
}

static int scap_cb_cc(struct fts_test *tdata, int *scap_cb, bool *result)
{
	int ret = 0;
	int i = 0;
	u8 wc_sel = 0;
	u8 hc_sel = 0;
	u8 hov_high = 0;
	int byte_num = tdata->sc_node.node_num;
	bool tmp_result = false;
	bool tmp2_result = false;
	bool tmp3_result = false;
	bool fw_wp_check = false;
	bool tx_check = false;
	bool rx_check = false;
	int *scb_tmp = NULL;
	int *scb_cf = NULL;
	int scb_cnt = 0;
	struct mc_sc_threshold *thr = &tdata->ic.mc_sc.thr;

	FTS_TEST_FUNC_ENTER();
	if (!thr->scap_cb_on_min || !thr->scap_cb_on_max
		|| !thr->scap_cb_off_min || !thr->scap_cb_off_max
		|| !thr->scap_cb_hi_min || !thr->scap_cb_hi_max
		|| !thr->scap_cb_on_cf_min || !thr->scap_cb_on_cf_max
		|| !thr->scap_cb_off_cf_min || !thr->scap_cb_off_cf_max
		|| !thr->scap_cb_hi_cf_min || !thr->scap_cb_hi_cf_max) {
		FTS_TEST_SAVE_ERR("scap_cb_on/off/hi/cf_min/max is null\n");
		ret = -EINVAL;
		return ret;
	}

	/* get waterproof channel select */
	ret = fts_test_read_reg(FACTORY_REG_WC_SEL, &wc_sel);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read water_channel_sel fail,ret=%d\n", ret);
		return ret;
	}

	ret = fts_test_read_reg(FACTORY_REG_HC_SEL, &hc_sel);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read high_channel_sel fail,ret=%d\n", ret);
		return ret;
	}

	/* water proof on check */
	tmp_result = true;
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_ON);
	if (thr->basic.scap_cb_wp_on_check && fw_wp_check) {
		tdata->csv_item_cnt += 2;
		tdata->csv_item_scb |= 0x01;
		scb_tmp = scap_cb + scb_cnt;
		scb_cf = scb_tmp + tdata->sc_node.node_num;
		/* 1:waterproof 0:non-waterproof */
		ret = fts_test_write_reg(FACTORY_REG_MC_SC_MODE, WATER_PROOF_ON);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("set mc_sc mode fail\n");
			return ret;
		}

		ret = wait_state_update(TEST_RETVAL_AA);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("wait state update fail\n");
			return ret;
		}

		ret = get_cb_ft5662(scb_tmp, byte_num, false);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read sc_cb fail,ret=%d\n", ret);
			return ret;
		}

		ret = fts_test_write_reg(FACTROY_REG_CB_BUF_SEL, 1);
		if (ret) {
			FTS_TEST_SAVE_ERR("restore sc mode fail,ret=%d\n", ret);
			return ret;
		}

		ret = get_cb_ft5662(scb_cf, byte_num, true);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("get sc_cb fail,ret=%d\n", ret);
			return ret;
		}

		ret = fts_test_write_reg(FACTROY_REG_CB_BUF_SEL, 0);
		if (ret) {
			FTS_TEST_SAVE_ERR("restore sc mode fail,ret=%d\n", ret);
			return ret;
		}

		/* show Scap CB */
		FTS_TEST_SAVE_INFO("scap_cb in waterproof on mode:\n");
		show_data_mc_sc(scb_tmp);
		FTS_TEST_SAVE_INFO("scap_cb_cf:\n");
		show_data_mc_sc(scb_cf);

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_RX);
		for (i = 0; i < tdata->sc_node.node_num; i++) {
			if (tdata->node_valid_sc[i] == 0)
				continue;

			if ((i == 0) || (i == tdata->sc_node.rx_num))
				continue;

			if ((rx_check && (i < tdata->sc_node.rx_num)) ||
				(tx_check && (i >= tdata->sc_node.rx_num))) {
				if ((scb_tmp[i] < thr->scap_cb_on_min[i]) ||
					(scb_tmp[i] > thr->scap_cb_on_max[i])) {
					FTS_TEST_SAVE_ERR("test fail,CH%d=%5d,range=(%5d,%5d)\n",
									  i + 1, scb_tmp[i],
									  thr->scap_cb_on_min[i],
									  thr->scap_cb_on_max[i]);
					tmp_result = false;
				}

				if ((scb_cf[i] < thr->scap_cb_on_cf_min[i]) ||
					(scb_cf[i] > thr->scap_cb_on_cf_max[i])) {
					FTS_TEST_SAVE_ERR("test2 fail,CH%d=%5d,range=(%5d,%5d)\n",
									  i + 1, scb_cf[i],
									  thr->scap_cb_on_cf_min[i],
									  thr->scap_cb_on_cf_max[i]);
					tmp_result = false;
				}
			}
		}

		scb_cnt += tdata->sc_node.node_num * 2;
	}

	/* water proof off check */
	tmp2_result = true;
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_OFF);
	if (thr->basic.scap_cb_wp_off_check && fw_wp_check) {
		tdata->csv_item_cnt += 2;
		tdata->csv_item_scb |= 0x02;
		scb_tmp = scap_cb + scb_cnt;
		scb_cf = scb_tmp + tdata->sc_node.node_num;
		/* 1:waterproof 0:non-waterproof */
		ret = fts_test_write_reg(FACTORY_REG_MC_SC_MODE, WATER_PROOF_OFF);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("set mc_sc mode fail\n");
			return ret;
		}

		ret = wait_state_update(TEST_RETVAL_AA);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("wait state update fail\n");
			return ret;
		}

		ret = get_cb_ft5662(scb_tmp, byte_num, false);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read sc_cb fail,ret=%d\n", ret);
			return ret;
		}

		ret = fts_test_write_reg(FACTROY_REG_CB_BUF_SEL, 1);
		if (ret) {
			FTS_TEST_SAVE_ERR("restore sc mode fail,ret=%d\n", ret);
			return ret;
		}

		ret = get_cb_ft5662(scb_cf, byte_num, true);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("get sc_cb fail,ret=%d\n", ret);
			return ret;
		}

		ret = fts_test_write_reg(FACTROY_REG_CB_BUF_SEL, 0);
		if (ret) {
			FTS_TEST_SAVE_ERR("restore sc mode fail,ret=%d\n", ret);
			return ret;
		}

		/* show Scap CB */
		FTS_TEST_SAVE_INFO("scap_cb in waterproof off mode:\n");
		show_data_mc_sc(scb_tmp);
		FTS_TEST_SAVE_INFO("scap_cb_cf:\n");
		show_data_mc_sc(scb_cf);

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_RX);
		for (i = 0; i < tdata->sc_node.node_num; i++) {
			if (tdata->node_valid_sc[i] == 0)
				continue;

			if ((i == 0) || (i == tdata->sc_node.rx_num))
				continue;

			if ((rx_check && (i < tdata->sc_node.rx_num)) ||
				(tx_check && (i >= tdata->sc_node.rx_num))) {
				if ((scb_tmp[i] < thr->scap_cb_off_min[i]) ||
					(scb_tmp[i] > thr->scap_cb_off_max[i])) {
					FTS_TEST_SAVE_ERR("test fail,CH%d=%5d,range=(%5d,%5d)\n",
									  i + 1, scb_tmp[i],
									  thr->scap_cb_off_min[i],
									  thr->scap_cb_off_max[i]);
					tmp2_result = false;
				}

				if ((scb_cf[i] < thr->scap_cb_off_cf_min[i]) ||
					(scb_cf[i] > thr->scap_cb_off_cf_max[i])) {
					FTS_TEST_SAVE_ERR("test2 fail,CH%d=%5d,range=(%5d,%5d)\n",
									  i + 1, scb_cf[i],
									  thr->scap_cb_off_cf_min[i],
									  thr->scap_cb_off_cf_max[i]);
					tmp2_result = false;
				}
			}
		}

		scb_cnt += tdata->sc_node.node_num * 2;
	}

	/* high mode */
	tmp3_result = true;
	hov_high = (hc_sel & 0x03);
	if (thr->basic.scap_cb_hi_check && hov_high) {
		tdata->csv_item_cnt += 2;
		tdata->csv_item_scb |= 0x04;
		scb_tmp = scap_cb + scb_cnt;
		scb_cf = scb_tmp + tdata->sc_node.node_num;
		/* 1:waterproof 0:non-waterproof */
		ret = fts_test_write_reg(FACTORY_REG_MC_SC_MODE, HIGH_SENSITIVITY);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("set mc_sc mode fail\n");
			return ret;
		}

		ret = wait_state_update(TEST_RETVAL_AA);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("wait state update fail\n");
			return ret;
		}

		ret = get_cb_ft5662(scb_tmp, byte_num, false);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read sc_cb fail,ret=%d\n", ret);
			return ret;
		}

		ret = fts_test_write_reg(FACTROY_REG_CB_BUF_SEL, 1);
		if (ret) {
			FTS_TEST_SAVE_ERR("restore sc mode fail,ret=%d\n", ret);
			return ret;
		}

		ret = get_cb_ft5662(scb_cf, byte_num, true);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("get sc_cb fail,ret=%d\n", ret);
			return ret;
		}

		ret = fts_test_write_reg(FACTROY_REG_CB_BUF_SEL, 0);
		if (ret) {
			FTS_TEST_SAVE_ERR("restore sc mode fail,ret=%d\n", ret);
			return ret;
		}

		/* show Scap CB */
		FTS_TEST_SAVE_INFO("scap_cb in high mode:\n");
		show_data_mc_sc(scb_tmp);
		FTS_TEST_SAVE_INFO("scap_cb_cf:\n");
		show_data_mc_sc(scb_cf);

		/* compare */
		tx_check = ((hov_high == 1) || (hov_high == 3));
		rx_check = ((hov_high == 2) || (hov_high == 3));
		for (i = 0; i < tdata->sc_node.node_num; i++) {
			if (tdata->node_valid_sc[i] == 0)
				continue;

			if ((i == 0) || (i == tdata->sc_node.rx_num))
				continue;

			if ((rx_check && (i < tdata->sc_node.rx_num)) ||
				(tx_check && (i >= tdata->sc_node.rx_num))) {
				if ((scb_tmp[i] < thr->scap_cb_hi_min[i]) ||
					(scb_tmp[i] > thr->scap_cb_hi_max[i])) {
					FTS_TEST_SAVE_ERR("test fail,CH%d=%5d,range=(%5d,%5d)\n",
									  i + 1, scb_tmp[i],
									  thr->scap_cb_hi_min[i],
									  thr->scap_cb_hi_max[i]);
					tmp3_result = false;
				}

				if ((scb_cf[i] < thr->scap_cb_hi_cf_min[i]) ||
					(scb_cf[i] > thr->scap_cb_hi_cf_max[i])) {
					FTS_TEST_SAVE_ERR("test2 fail,CH%d=%5d,range=(%5d,%5d)\n",
									  i + 1, scb_cf[i],
									  thr->scap_cb_hi_cf_min[i],
									  thr->scap_cb_hi_cf_max[i]);
					tmp3_result = false;
				}
			}
		}

		scb_cnt += tdata->sc_node.node_num * 2;
	}

	*result = tmp_result && tmp2_result && tmp3_result;
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int scap_cb_ccbypass(struct fts_test *tdata, int *scap_cb, bool *result)
{
	int ret = 0;
	int i = 0;
	u8 wc_sel = 0;
	u8 hc_sel = 0;
	u8 hov_high = 0;
	u8 scap_gcb_tx = 0;
	u8 scap_gcb_rx = 0;
	int byte_num = tdata->sc_node.node_num;
	bool tmp_result = false;
	bool tmp2_result = false;
	bool tmp3_result = false;
	bool fw_wp_check = false;
	bool tx_check = false;
	bool rx_check = false;
	int *scb_tmp = NULL;
	int scb_cnt = 0;
	struct mc_sc_threshold *thr = &tdata->ic.mc_sc.thr;

	FTS_TEST_FUNC_ENTER();
	if (!thr->scap_cb_on_min || !thr->scap_cb_on_max
		|| !thr->scap_cb_off_min || !thr->scap_cb_off_max
		|| !thr->scap_cb_hi_min || !thr->scap_cb_hi_max) {
		FTS_TEST_SAVE_ERR("scap_cb_on/off/hi_min/max is null\n");
		ret = -EINVAL;
		return ret;
	}

	/* get waterproof channel select */
	ret = fts_test_read_reg(FACTORY_REG_WC_SEL, &wc_sel);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read water_channel_sel fail,ret=%d\n", ret);
		return ret;
	}

	ret = fts_test_read_reg(FACTORY_REG_HC_SEL, &hc_sel);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read high_channel_sel fail,ret=%d\n", ret);
		return ret;
	}

	/* water proof on check */
	tmp_result = true;
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_ON);
	if (thr->basic.scap_cb_wp_on_check && fw_wp_check) {
		tdata->csv_item_cnt += 2;
		tdata->csv_item_scb |= 0x01;
		scb_tmp = scap_cb + scb_cnt;
		/* 1:waterproof 0:non-waterproof */
		ret = fts_test_write_reg(FACTORY_REG_MC_SC_MODE, WATER_PROOF_ON);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("set mc_sc mode fail\n");
			return ret;
		}

		ret = wait_state_update(TEST_RETVAL_AA);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("wait state update fail\n");
			return ret;
		}

		ret = get_cb_ft5662(scb_tmp, byte_num, false);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read sc_cb fail,ret=%d\n", ret);
			return ret;
		}

		ret = fts_test_read_reg(FACTROY_REG_SCAP_GCB_RX, &scap_gcb_rx);
		if (ret) {
			FTS_TEST_SAVE_ERR("read GCB_RX fail,ret=%d\n", ret);
			return ret;
		}

		ret = fts_test_read_reg(FACTROY_REG_SCAP_GCB_TX, &scap_gcb_tx);
		if (ret) {
			FTS_TEST_SAVE_ERR("read GCB_TX fail,ret=%d\n", ret);
			return ret;
		}
		scb_tmp[tdata->sc_node.node_num] = scap_gcb_rx;
		scb_tmp[tdata->sc_node.node_num + tdata->sc_node.rx_num] = scap_gcb_tx;

		/* show Scap CB */
		FTS_TEST_SAVE_INFO("scap_cb in waterproof on mode:\n");
		show_data_mc_sc(scb_tmp);
		FTS_TEST_SAVE_INFO("GCB RX:%d,TX:%d\n", scap_gcb_rx, scap_gcb_tx);

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_RX);
		for (i = 0; i < tdata->sc_node.node_num; i++) {
			if (tdata->node_valid_sc[i] == 0)
				continue;

			if ((rx_check && (i < tdata->sc_node.rx_num)) ||
				(tx_check && (i >= tdata->sc_node.rx_num))) {
				if ((scb_tmp[i] < thr->scap_cb_on_min[i]) ||
					(scb_tmp[i] > thr->scap_cb_on_max[i])) {
					FTS_TEST_SAVE_ERR("test fail,CH%d=%5d,range=(%5d,%5d)\n",
									  i + 1, scb_tmp[i],
									  thr->scap_cb_on_min[i],
									  thr->scap_cb_on_max[i]);
					tmp_result = false;
				}
			}
		}

		if (rx_check && ((scap_gcb_rx < thr->basic.scap_cb_on_gcb_min) ||
						 (scap_gcb_rx > thr->basic.scap_cb_on_gcb_max))) {
			FTS_TEST_SAVE_ERR("test fail,gcb_rx:%5d,range=(%5d,%5d)\n",
							  scap_gcb_rx,
							  thr->basic.scap_cb_on_gcb_min,
							  thr->basic.scap_cb_on_gcb_max);
			tmp_result = false;
		}

		if (tx_check && ((scap_gcb_tx < thr->basic.scap_cb_on_gcb_min) ||
						 (scap_gcb_tx > thr->basic.scap_cb_on_gcb_max))) {
			FTS_TEST_SAVE_ERR("test fail,gcb_tx:%5d,range=(%5d,%5d)\n",
							  scap_gcb_tx,
							  thr->basic.scap_cb_on_gcb_min,
							  thr->basic.scap_cb_on_gcb_max);
			tmp_result = false;
		}

		scb_cnt += tdata->sc_node.node_num * 2;
	}

	/* water proof off check */
	tmp2_result = true;
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_OFF);
	if (thr->basic.scap_cb_wp_off_check && fw_wp_check) {
		tdata->csv_item_cnt += 2;
		tdata->csv_item_scb |= 0x02;
		scb_tmp = scap_cb + scb_cnt;
		/* 1:waterproof 0:non-waterproof */
		ret = fts_test_write_reg(FACTORY_REG_MC_SC_MODE, WATER_PROOF_OFF);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("set mc_sc mode fail\n");
			return ret;
		}

		ret = wait_state_update(TEST_RETVAL_AA);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("wait state update fail\n");
			return ret;
		}

		ret = get_cb_ft5662(scb_tmp, byte_num, false);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read sc_cb fail,ret=%d\n", ret);
			return ret;
		}

		ret = fts_test_read_reg(FACTROY_REG_SCAP_GCB_RX, &scap_gcb_rx);
		if (ret) {
			FTS_TEST_SAVE_ERR("read GCB_RX fail,ret=%d\n", ret);
			return ret;
		}

		ret = fts_test_read_reg(FACTROY_REG_SCAP_GCB_TX, &scap_gcb_tx);
		if (ret) {
			FTS_TEST_SAVE_ERR("read GCB_TX fail,ret=%d\n", ret);
			return ret;
		}
		scb_tmp[tdata->sc_node.node_num] = scap_gcb_rx;
		scb_tmp[tdata->sc_node.node_num + tdata->sc_node.rx_num] = scap_gcb_tx;

		/* show Scap CB */
		FTS_TEST_SAVE_INFO("scap_cb in waterproof off mode:\n");
		show_data_mc_sc(scb_tmp);
		FTS_TEST_SAVE_INFO("GCB RX:%d,TX:%d\n", scap_gcb_rx, scap_gcb_tx);

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_RX);
		for (i = 0; i < tdata->sc_node.node_num; i++) {
			if (tdata->node_valid_sc[i] == 0)
				continue;

			if ((rx_check && (i < tdata->sc_node.rx_num)) ||
				(tx_check && (i >= tdata->sc_node.rx_num))) {
				if ((scb_tmp[i] < thr->scap_cb_off_min[i]) ||
					(scb_tmp[i] > thr->scap_cb_off_max[i])) {
					FTS_TEST_SAVE_ERR("test fail,CH%d=%5d,range=(%5d,%5d)\n",
									  i + 1, scb_tmp[i],
									  thr->scap_cb_off_min[i],
									  thr->scap_cb_off_max[i]);
					tmp2_result = false;
				}
			}
		}

		if (rx_check && ((scap_gcb_rx < thr->basic.scap_cb_off_gcb_min) ||
						 (scap_gcb_rx > thr->basic.scap_cb_off_gcb_max))) {
			FTS_TEST_SAVE_ERR("test fail,gcb_rx:%5d,range=(%5d,%5d)\n",
							  scap_gcb_rx,
							  thr->basic.scap_cb_off_gcb_min,
							  thr->basic.scap_cb_off_gcb_max);
			tmp2_result = false;
		}

		if (tx_check && ((scap_gcb_tx < thr->basic.scap_cb_off_gcb_min) ||
						 (scap_gcb_tx > thr->basic.scap_cb_off_gcb_max))) {
			FTS_TEST_SAVE_ERR("test fail,gcb_tx:%5d,range=(%5d,%5d)\n",
							  scap_gcb_tx,
							  thr->basic.scap_cb_off_gcb_min,
							  thr->basic.scap_cb_off_gcb_max);
			tmp2_result = false;
		}

		scb_cnt += tdata->sc_node.node_num * 2;
	}

	/*high mode*/
	tmp3_result = true;
	hov_high = (hc_sel & 0x03);
	if (thr->basic.scap_cb_hi_check && hov_high) {
		tdata->csv_item_cnt += 2;
		tdata->csv_item_scb |= 0x04;
		scb_tmp = scap_cb + scb_cnt;
		/* 1:waterproof 0:non-waterproof */
		ret = fts_test_write_reg(FACTORY_REG_MC_SC_MODE, HIGH_SENSITIVITY);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("set mc_sc mode fail\n");
			return ret;
		}

		ret = wait_state_update(TEST_RETVAL_AA);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("wait state update fail\n");
			return ret;
		}

		ret = get_cb_ft5662(scb_tmp, byte_num, false);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read sc_cb fail,ret=%d\n", ret);
			return ret;
		}

		ret = fts_test_read_reg(FACTROY_REG_SCAP_GCB_RX, &scap_gcb_rx);
		if (ret) {
			FTS_TEST_SAVE_ERR("read GCB_RX fail,ret=%d\n", ret);
			return ret;
		}

		ret = fts_test_read_reg(FACTROY_REG_SCAP_GCB_TX, &scap_gcb_tx);
		if (ret) {
			FTS_TEST_SAVE_ERR("read GCB_TX fail,ret=%d\n", ret);
			return ret;
		}
		scb_tmp[tdata->sc_node.node_num] = scap_gcb_rx;
		scb_tmp[tdata->sc_node.node_num + tdata->sc_node.rx_num] = scap_gcb_tx;

		/* show Scap CB */
		FTS_TEST_SAVE_INFO("scap_cb in high mode:\n");
		show_data_mc_sc(scb_tmp);
		FTS_TEST_SAVE_INFO("GCB RX:%d,TX:%d\n", scap_gcb_rx, scap_gcb_tx);

		/* compare */
		tx_check = ((hov_high == 1) || (hov_high == 3));
		rx_check = ((hov_high == 2) || (hov_high == 3));
		for (i = 0; i < tdata->sc_node.node_num; i++) {
			if (tdata->node_valid_sc[i] == 0)
				continue;

			if ((rx_check && (i < tdata->sc_node.rx_num)) ||
				(tx_check && (i >= tdata->sc_node.rx_num))) {
				if ((scb_tmp[i] < thr->scap_cb_hi_min[i]) ||
					(scb_tmp[i] > thr->scap_cb_hi_max[i])) {
					FTS_TEST_SAVE_ERR("test fail,CH%d=%5d,range=(%5d,%5d)\n",
									  i + 1, scb_tmp[i],
									  thr->scap_cb_hi_min[i],
									  thr->scap_cb_hi_max[i]);
					tmp3_result = false;
				}
			}
		}

		if (rx_check && ((scap_gcb_rx < thr->basic.scap_cb_hi_gcb_min) ||
						 (scap_gcb_rx > thr->basic.scap_cb_hi_gcb_max))) {
			FTS_TEST_SAVE_ERR("test fail,gcb_rx:%5d,range=(%5d,%5d)\n",
							  scap_gcb_rx,
							  thr->basic.scap_cb_hi_gcb_min,
							  thr->basic.scap_cb_hi_gcb_max);
			tmp3_result = false;
		}

		if (tx_check && ((scap_gcb_tx < thr->basic.scap_cb_hi_gcb_min) ||
						 (scap_gcb_tx > thr->basic.scap_cb_hi_gcb_max))) {
			FTS_TEST_SAVE_ERR("test fail,gcb_tx:%5d,range=(%5d,%5d)\n",
							  scap_gcb_tx,
							  thr->basic.scap_cb_hi_gcb_min,
							  thr->basic.scap_cb_hi_gcb_max);
			tmp3_result = false;
		}

		scb_cnt += tdata->sc_node.node_num * 2;
	}

	*result = tmp_result && tmp2_result && tmp3_result;
	FTS_TEST_FUNC_EXIT();
	return ret;
}

/*
 * start_scan - start to scan a frame
 */
static int start_scan_ft5662(int frame_num)
{
	int ret = 0;
	u8 addr = 0;
	u8 val = 0;
	u8 finish_val = 0;
	int times = 0;
	struct fts_test *tdata = fts_ftest;

	if ((tdata == NULL) || (tdata->func == NULL)) {
		FTS_TEST_SAVE_ERR("test/func is null\n");
		return -EINVAL;
	}

	addr = DEVIDE_MODE_ADDR;
	val = 0xC0;
	finish_val = 0x40;

	/* write register to start scan */
	ret = fts_test_write_reg(addr, val);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write start scan mode fail\n");
		return ret;
	}

	sys_delay(frame_num * FACTORY_TEST_DELAY / 2);
	/* Wait for the scan to complete */
	while (times++ < 100) {
		sys_delay(FACTORY_TEST_DELAY);

		ret = fts_test_read_reg(addr, &val);
		if ((ret >= 0) && (val == finish_val))
			break;
		FTS_TEST_DBG("reg%x=%x,retry:%d", addr, val, times);
	}

	if (times >= 100) {
		FTS_TEST_SAVE_ERR("scan timeout\n");
		return -EIO;
	}

	return 0;
}

static int get_noise_ft5662(struct fts_test *tdata, int *noise_data, u8 fre)
{
	int ret = 0;
	int byte_num = 0;

	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, fre);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set frequecy fail,ret=%d\n", ret);
		return ret;
	}

	/* wait fw state update */
	ret = wait_state_update(TEST_RETVAL_AA);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("wait state update fail\n");
		return ret;
	}

	ret = start_scan_ft5662(tdata->ic.mc_sc.thr.basic.noise_framenum);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("start_scan fail,ret=%d\n", ret);
		return ret;
	}

	ret = fts_test_write_reg(0x01, 0xAA);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write 0x01 fail,ret=%d\n", ret);
		return ret;
	}

	byte_num = tdata->node.node_num * 2;
	ret = read_mass_data(0xCE, byte_num, noise_data);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read noise fail\n");
		return ret;
	}

	return 0;
}

static void show_null_noise(struct fts_test *tdata, int *null_noise)
{
	int i = 0;
	int j = 0;
	int cnt = 0;
	char tmpbuf[512] = { 0 };

	/*show noise*/
	FTS_TEST_INFO("null noise:%d", null_noise[0]);
	FTS_TEST_SAVE_INFO("null noise:%d\n", null_noise[0]);
	for (i = 0; i < tdata->fre_num; i++) {
		for (j = 0; j < tdata->node.rx_num; j++)
			cnt += snprintf(tmpbuf + cnt, 512 - cnt, "%5d,",
					null_noise[i * tdata->node.rx_num + j + 1]);
		cnt = 0;
		FTS_TEST_INFO("%s", tmpbuf);
		FTS_TEST_SAVE_INFO("%s\n", tmpbuf);
	}
}

static int get_null_noise(struct fts_test *tdata)
{
	int ret = 0;
	int *null_noise;
	int null_byte_num = 0;

	null_byte_num = tdata->node.rx_num * tdata->fre_num + 1;
	FTS_TEST_INFO("null noise num:%d", null_byte_num);
	null_noise = kcalloc(null_byte_num, sizeof(int), GFP_KERNEL);
	if (!null_noise) {
		FTS_TEST_ERROR("null_noise malloc fail");
		return -ENOMEM;
	}

	ret = fts_test_write_reg(0x01, 0xB0);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write 0x01 fail,ret=%d\n", ret);
		goto err_null_noise;
	}

	ret = read_mass_data(0xCE, null_byte_num * 2, null_noise);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read null noise fail\n");
		goto err_null_noise;
	}

	tdata->null_noise_max = null_noise[0];
	show_null_noise(tdata, null_noise);
	ret = 0;

err_null_noise:
	kfree(null_noise);
	null_noise = NULL;
	return ret;
}

static int ft5662_rawdata_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	int i = 0;
	int *rawdata = NULL;
	u8 fre = 0;
	u8 data_type = 0;
	bool result = false;
	struct mc_sc_threshold *thr = &tdata->ic.mc_sc.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: rawdata test\n");
	rawdata = tdata->item1_data;
	tdata->csv_item_cnt++;

	if (!rawdata || !thr->rawdata_h_min || !thr->rawdata_h_max) {
		FTS_TEST_SAVE_ERR("rawdata_h_min/max is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode,ret=%d\n", ret);
		goto test_err;
	}

	/* rawdata test in mapping mode */
	ret = mapping_switch(MAPPING);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("switch mapping fail,ret=%d\n", ret);
		goto test_err;
	}

	/* save origin value */
	ret = fts_test_read_reg(FACTORY_REG_FRE_LIST, &fre);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0x0A fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_DATA_TYPE, &data_type);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0x5B fail,ret=%d\n", ret);
		goto test_err;
	}

	/* set frequecy high */
	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, 0x81);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set frequecy fail,ret=%d\n", ret);
		goto restore_reg;
	}

	/* wait fw state update */
	ret = wait_state_update(TEST_RETVAL_AA);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("wait state update fail\n");
		goto restore_reg;
	}

	ret = fts_test_write_reg(FACTORY_REG_DATA_TYPE, 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set raw type fail,ret=%d\n", ret);
		goto restore_reg;
	}

	/*********************GET RAWDATA*********************/
	for (i = 0; i < 3; i++) {
		/* lost 3 frames, in order to obtain stable data */
		ret = get_rawdata(rawdata);
	}
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get rawdata fail,ret=%d\n", ret);
		goto restore_reg;
	}

	/* show test data */
	show_data(rawdata, false);

	/* compare */
	result = compare_array(rawdata, thr->rawdata_h_min,
			thr->rawdata_h_max, false);

restore_reg:
	/* set the origin value */
	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, fre);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("restore reg0A fail,ret=%d\n", ret);

	/* wait fw state update */
	ret = wait_state_update(TEST_RETVAL_AA);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("wait state update fail\n");

	ret = fts_test_write_reg(FACTORY_REG_DATA_TYPE, data_type);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("set raw type fail,ret=%d\n", ret);

test_err:
	if (result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ rawdata test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("------ rawdata test NG\n");
	}

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft5662_uniformity_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	int index = 0;
	int row = 0;
	int col = 1;
	int i = 0;
	int deviation = 0;
	int max = 0;
	int min = 0;
	int uniform = 0;
	int *rawdata = NULL;
	int *rawdata_linearity = NULL;
	int *rl_tmp = NULL;
	int rl_cnt = 0;
	int offset = 0;
	int offset2 = 0;
	int tx_num = 0;
	int rx_num = 0;
	u8 fre = 0;
	u8 data_type = 0;
	struct mc_sc_threshold *thr = &fts_ftest->ic.mc_sc.thr;
	bool result = false;
	bool result2 = false;
	bool result3 = false;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: rawdata unfiormity test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	rawdata = tdata->buffer;
	tx_num = tdata->node.tx_num;
	rx_num = tdata->node.rx_num;

	rawdata_linearity = tdata->item2_data;
	if (!rawdata_linearity) {
		FTS_TEST_SAVE_ERR("rawdata_linearity buffer fail");
		ret = -ENOMEM;
		goto test_err;
	}

	if (!thr->tx_linearity_max || !thr->rx_linearity_max
		|| !tdata->node_valid) {
		FTS_TEST_SAVE_ERR("tx/rx_lmax/node_valid is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode,ret=%d\n", ret);
		goto test_err;
	}

	/* rawdata unfiormity test in mapping mode */
	ret = mapping_switch(MAPPING);
	if (ret) {
		FTS_TEST_SAVE_ERR("failed to switch_to_mapping,ret=%d", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_FRE_LIST, &fre);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0x0A fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_DATA_TYPE, &data_type);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0x5B fail,ret=%d\n", ret);
		goto test_err;
	}

	/* set frequecy high */
	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, 0x81);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set frequecy fail,ret=%d\n", ret);
		goto restore_reg;
	}

	/* wait fw state update */
	ret = wait_state_update(TEST_RETVAL_AA);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("wait state update fail\n");
		goto restore_reg;
	}

	ret = fts_test_write_reg(FACTORY_REG_DATA_TYPE, 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set raw type fail,ret=%d\n", ret);
		goto restore_reg;
	}

	/* change register value before,need to lose 3 frame data */
	for (index = 0; index < 3; ++index)
		ret = get_rawdata(rawdata);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get rawdata fail,ret=%d\n", ret);
		goto restore_reg;
	}
	print_buffer(rawdata, tdata->node.node_num, tdata->node.rx_num);

	result = true;
	if (thr->basic.uniformity_check_tx) {
		FTS_TEST_SAVE_INFO("Check Tx Linearity\n");
		tdata->csv_item_cnt++;
		rl_tmp = rawdata_linearity + rl_cnt;
		for (row = 0; row < tx_num; row++) {
			for (col = 1; col <  rx_num; col++) {
				offset = row * rx_num + col;
				offset2 = row * rx_num + col - 1;
				deviation = abs(rawdata[offset] - rawdata[offset2]);
				max = max(rawdata[offset], rawdata[offset2]);
				max = max ? max : 1;
				rl_tmp[offset] = 100 * deviation / max;
			}
		}
		/*show data in result.txt*/
		FTS_TEST_SAVE_INFO(" Tx Linearity:\n");
		show_data(rl_tmp, false);
		FTS_TEST_SAVE_INFO("\n");

		/* compare */
		result = compare_array(rl_tmp, thr->tx_linearity_min,
				thr->tx_linearity_max, false);

		rl_cnt += tdata->node.node_num;
	}

	result2 = true;
	if (thr->basic.uniformity_check_rx) {
		FTS_TEST_SAVE_INFO("Check Rx Linearity\n");
		tdata->csv_item_cnt++;
		rl_tmp = rawdata_linearity + rl_cnt;
		for (row = 1; row < tx_num; row++) {
			for (col = 0; col < rx_num; col++) {
				offset = row * rx_num + col;
				offset2 = (row - 1) * rx_num + col;
				deviation = abs(rawdata[offset] - rawdata[offset2]);
				max = max(rawdata[offset], rawdata[offset2]);
				max = max ? max : 1;
				rl_tmp[offset] = 100 * deviation / max;
			}
		}

		FTS_TEST_SAVE_INFO("Rx Linearity:\n");
		show_data(rl_tmp, false);
		FTS_TEST_SAVE_INFO("\n");

		/* compare */
		result2 = compare_array(rl_tmp, thr->rx_linearity_min,
				thr->rx_linearity_max, false);
		rl_cnt += tdata->node.node_num;
	}

	result3 = true;
	if (thr->basic.uniformity_check_min_max) {
		FTS_TEST_SAVE_INFO("Check Min/Max\n");
		min = 100000;
		max = -100000;
		for (i = 0; i < tdata->node.node_num; i++) {
			if (tdata->node_valid[i] == 0)
				continue;
			min = min(min, rawdata[i]);
			max = max(max, rawdata[i]);
		}
		max = !max ? 1 : max;
		uniform = 100 * abs(min) / abs(max);

		FTS_TEST_SAVE_INFO("min:%d, max:%d, get value of min/max:%d\n",
						   min, max, uniform);
		if (uniform < thr->basic.uniformity_min_max_hole) {
			result3 = false;
			FTS_TEST_SAVE_ERR("min_max out of range, set value: %d\n",
							  thr->basic.uniformity_min_max_hole);
		}
	}

restore_reg:
	/* set the origin value */
	ret = fts_test_write_reg(FACTORY_REG_DATA_TYPE, data_type);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("set raw type fail,ret=%d\n", ret);

	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, fre);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("restore 0x0A fail,ret=%d\n", ret);

	/* wait fw state update */
	ret = wait_state_update(TEST_RETVAL_AA);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("wait state update fail\n");
test_err:
	if (result && result2 && result3) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("uniformity test is OK\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_ERR("uniformity test is NG\n");
	}

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft5662_scap_cb_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	u8 sc_mode = 0;
	u8 scap_cfg = 0;
	bool tmp_result = false;
	bool cc_en = false;
	int *scap_cb = NULL;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Scap CB Test\n");
	scap_cb = tdata->item3_data;

	if (!scap_cb) {
		FTS_TEST_SAVE_ERR("scap_cb fails");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	/* SCAP CB is in no-mapping mode */
	ret = mapping_switch(NO_MAPPING);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("switch no-mapping fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_MC_SC_MODE, &sc_mode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read sc_mode fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTROY_REG_SCAP_CFG, &scap_cfg);
	if (ret) {
		FTS_TEST_SAVE_ERR("read reg58 fail,ret=%d\n", ret);
		goto test_err;
	}

	cc_en = !(scap_cfg & 0x80);
	FTS_TEST_INFO("cc_en:%d", cc_en);
	if (cc_en)
		ret = scap_cb_cc(tdata, scap_cb, &tmp_result);
	else
		ret = scap_cb_ccbypass(tdata, scap_cb, &tmp_result);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("scap_cb fail,ret:%d", ret);

	ret = fts_test_write_reg(FACTORY_REG_MC_SC_MODE, sc_mode);/* set the origin value */
	if (ret) {
		FTS_TEST_SAVE_ERR("restore sc mode fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = wait_state_update(TEST_RETVAL_AA);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("wait state update fail\n");

test_err:
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n------ scap cb test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_ERR("\n------ scap cb test NG\n");
	}

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft5662_scap_rawdata_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	int i = 0;
	bool tmp_result = false;
	bool tmp2_result = false;
	bool tmp3_result = false;
	bool fw_wp_check = false;
	bool tx_check = false;
	bool rx_check = false;
	bool cc_en = false;
	int *scap_rawdata = NULL;
	int *srawdata_tmp = NULL;
	int srawdata_cnt = 0;
	u8 wc_sel = 0;
	u8 hc_sel = 0;
	u8 hov_high = 0;
	u8 data_type = 0;
	u8 scap_cfg = 0;
	struct mc_sc_threshold *thr = &tdata->ic.mc_sc.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Scap Rawdata Test\n");
	scap_rawdata = tdata->item4_data;

	if (!scap_rawdata) {
		FTS_TEST_SAVE_ERR("scap rawdata fails");
		ret = -EINVAL;
		goto test_err;
	}

	if (!thr->scap_rawdata_on_min || !thr->scap_rawdata_on_max
		|| !thr->scap_rawdata_off_min || !thr->scap_rawdata_off_max
		|| !thr->scap_rawdata_hi_min || !thr->scap_rawdata_hi_max) {
		FTS_TEST_SAVE_ERR("scap_rawdata_on/off/hi_min/max is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	/* SCAP RAWDATA is in no-mapping mode */
	ret = mapping_switch(NO_MAPPING);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("switch no-mapping fail,ret=%d\n", ret);
		goto test_err;
	}

	/* get waterproof channel select */
	ret = fts_test_read_reg(FACTORY_REG_WC_SEL, &wc_sel);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read water_channel_sel fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_HC_SEL, &hc_sel);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read high_channel_sel fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_DATA_TYPE, &data_type);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0x5B fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTROY_REG_SCAP_CFG, &scap_cfg);
	if (ret) {
		FTS_TEST_SAVE_ERR("read reg58 fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_write_reg(FACTORY_REG_DATA_TYPE, 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set raw type fail,ret=%d\n", ret);
		goto restore_reg;
	}

	/* scan rawdata */
	ret = start_scan();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("scan scap rawdata fail\n");
		goto restore_reg;
	}

	ret = start_scan();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("scan scap rawdata(2) fail\n");
		goto restore_reg;
	}

	cc_en = !(scap_cfg & 0x80);
	FTS_TEST_INFO("cc_en:%d", cc_en);

	/* water proof on check */
	tmp_result = true;
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_ON);
	if (thr->basic.scap_rawdata_wp_on_check && fw_wp_check) {
		tdata->csv_item_cnt++;
		tdata->csv_item_sraw |= 0x01;
		srawdata_tmp = scap_rawdata + srawdata_cnt;
		ret = get_rawdata_mc_sc(WATER_PROOF_ON, srawdata_tmp);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("get scap(WP_ON) rawdata fail\n");
			goto restore_reg;
		}

		FTS_TEST_SAVE_INFO("scap_rawdata in waterproof on mode:\n");
		show_data_mc_sc(srawdata_tmp);

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_RX);
		for (i = 0; i < tdata->sc_node.node_num; i++) {
			if (tdata->node_valid_sc[i] == 0)
				continue;

			if (cc_en && ((i == 0) || (i == tdata->sc_node.rx_num)))
				continue;

			if ((rx_check && (i < tdata->sc_node.rx_num)) ||
				(tx_check && (i >= tdata->sc_node.rx_num))) {
				if ((srawdata_tmp[i] < thr->scap_rawdata_on_min[i]) ||
					(srawdata_tmp[i] > thr->scap_rawdata_on_max[i])) {
					FTS_TEST_SAVE_ERR("test fail,CH%d=%5d,range=(%5d,%5d)\n",
							i + 1, srawdata_tmp[i],
							thr->scap_rawdata_on_min[i],
							thr->scap_rawdata_on_max[i]);
					tmp_result = false;
				}
			}
		}

		srawdata_cnt += tdata->sc_node.node_num;
	}

	/* water proof off check */
	tmp2_result = true;
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_OFF);
	if (thr->basic.scap_rawdata_wp_off_check && fw_wp_check) {
		tdata->csv_item_cnt++;
		tdata->csv_item_sraw |= 0x02;
		srawdata_tmp = scap_rawdata + srawdata_cnt;
		ret = get_rawdata_mc_sc(WATER_PROOF_OFF, srawdata_tmp);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("get scap(WP_OFF) rawdata fail\n");
			goto restore_reg;
		}

		FTS_TEST_SAVE_INFO("scap_rawdata in waterproof off mode:\n");
		show_data_mc_sc(srawdata_tmp);

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_RX);
		for (i = 0; i < tdata->sc_node.node_num; i++) {
			if (tdata->node_valid_sc[i] == 0)
				continue;

			if (cc_en && ((i == 0) || (i == tdata->sc_node.rx_num)))
				continue;

			if ((rx_check && (i < tdata->sc_node.rx_num)) ||
				(tx_check && (i >= tdata->sc_node.rx_num))) {
				if ((srawdata_tmp[i] < thr->scap_rawdata_off_min[i]) ||
					(srawdata_tmp[i] > thr->scap_rawdata_off_max[i])) {
					FTS_TEST_SAVE_ERR("test fail,CH%d=%5d,range=(%5d,%5d)\n",
							i + 1, srawdata_tmp[i],
							thr->scap_rawdata_off_min[i],
							thr->scap_rawdata_off_max[i]);
					tmp2_result = false;
				}
			}
		}
		srawdata_cnt += tdata->sc_node.node_num;
	}

	/*high mode*/
	tmp3_result = true;
	hov_high = (hc_sel & 0x03);
	if (thr->basic.scap_rawdata_hi_check && hov_high) {
		tdata->csv_item_cnt++;
		tdata->csv_item_sraw |= 0x04;
		srawdata_tmp = scap_rawdata + srawdata_cnt;
		ret = get_rawdata_mc_sc(HIGH_SENSITIVITY, srawdata_tmp);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("get scap(HS) rawdata fail\n");
			goto restore_reg;
		}

		FTS_TEST_SAVE_INFO("scap_rawdata in hs mode:\n");
		show_data_mc_sc(srawdata_tmp);

		/* compare */
		tx_check = ((hov_high == 1) || (hov_high == 3));
		rx_check = ((hov_high == 2) || (hov_high == 3));
		for (i = 0; i < tdata->sc_node.node_num; i++) {
			if (tdata->node_valid_sc[i] == 0)
				continue;

			if (cc_en && ((i == 0) || (i == tdata->sc_node.rx_num)))
				continue;

			if ((rx_check && (i < tdata->sc_node.rx_num)) ||
				(tx_check && (i >= tdata->sc_node.rx_num))) {
				if ((srawdata_tmp[i] < thr->scap_rawdata_hi_min[i]) ||
					(srawdata_tmp[i] > thr->scap_rawdata_hi_max[i])) {
					FTS_TEST_SAVE_ERR("test fail,CH%d=%5d,range=(%5d,%5d)\n",
							i + 1, srawdata_tmp[i],
							thr->scap_rawdata_hi_min[i],
							thr->scap_rawdata_hi_max[i]);
					tmp3_result = false;
				}
			}
		}

		srawdata_cnt += tdata->sc_node.node_num;
	}

restore_reg:
	ret = fts_test_write_reg(FACTORY_REG_DATA_TYPE, data_type);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("set raw type fail,ret=%d\n", ret);

test_err:
	if (tmp_result && tmp2_result && tmp3_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n------ scap rawdata test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("\n------ scap rawdata test NG\n");
	}

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft5662_short_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	int ch_num = 0;
	int short_res[SC_NUM_MAX + 1] = { 0 };
	u8 ab_ch[SC_NUM_MAX + 1] = { 0 };
	bool ca_result = false;
	bool cg_result = false;
	bool cc_result = false;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Short Test\n");
	ch_num = tdata->sc_node.tx_num + tdata->sc_node.rx_num;

	if (ch_num >= SC_NUM_MAX) {
		FTS_TEST_SAVE_ERR("sc_node ch_num(%d)>max(%d)", ch_num, SC_NUM_MAX);
		goto test_err;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	/* short is in no-mapping mode */
	ret = mapping_switch(NO_MAPPING);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("switch no-mapping fail,ret=%d\n", ret);
		goto test_err;
	}

	/* get short resistance and exceptional channel */
	ret = short_test_ch_to_all(tdata, short_res, ab_ch, &ca_result);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("short test of channel to all fails\n");
		goto test_err;
	}

	if (!ca_result) {
		/*weak short fail, get short values*/
		ret = short_test_ch_to_gnd(tdata, short_res, ab_ch, &cg_result);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("short test of channel to gnd fails\n");
			goto test_err;
		}

		ret = short_test_ch_to_ch(tdata, short_res, ab_ch, &cc_result);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("short test of channel to channel fails\n");
			goto test_err;
		}
	}

test_err:
	if (ca_result) {
		FTS_TEST_SAVE_INFO("------ short test PASS\n");
		*test_result = true;
	} else {
		FTS_TEST_SAVE_ERR("------ short test NG\n");
		*test_result = false;
	}

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft5662_panel_differ_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	int i = 0;
	u8 fre = 0;
	u8 g_cb = 0;
	u8 normalize = 0;
	u8 data_type = 0;
	int *panel_differ = NULL;
	struct mc_sc_threshold *thr = &tdata->ic.mc_sc.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Panel Differ Test\n");
	panel_differ = tdata->item5_data;
	tdata->csv_item_cnt++;

	if (!panel_differ || !thr->panel_differ_min || !thr->panel_differ_max) {
		FTS_TEST_SAVE_ERR("panel_differ_h_min/max is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode,ret=%d\n", ret);
		goto test_err;
	}

	/* panel differ test in mapping mode */
	ret = mapping_switch(MAPPING);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("switch mapping fail,ret=%d\n", ret);
		goto test_err;
	}

	/* save origin value */
	ret = fts_test_read_reg(FACTORY_REG_FRE_LIST, &fre);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0x0A fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_DATA_TYPE, &data_type);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0x5B fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_GCB, &g_cb);
	if (ret) {
		FTS_TEST_SAVE_ERR("read regBD fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_NORMALIZE, &normalize);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read normalize fail,ret=%d\n", ret);
		goto test_err;
	}

	/* set frequecy high */
	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, 0x81);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set frequecy fail,ret=%d\n", ret);
		goto restore_reg;
	}

	/* wait fw state update */
	ret = wait_state_update(TEST_RETVAL_AA);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("wait state update fail\n");
		goto restore_reg;
	}

	ret = fts_test_write_reg(FACTORY_REG_DATA_TYPE, 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set raw type fail,ret=%d\n", ret);
		goto restore_reg;
	}

	ret = fts_test_write_reg(FACTORY_REG_GCB, 0x00);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set fir fail,ret=%d\n", ret);
		goto restore_reg;
	}

	ret = wait_state_update(TEST_RETVAL_AA);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("wait state update fail\n");
		goto restore_reg;
	}

	/* set to overall normalize */
	ret = fts_test_write_reg(FACTORY_REG_NORMALIZE, 0x00);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write normalize fail,ret=%d\n", ret);
		goto restore_reg;
	}

	/* get rawdata */
	for (i = 0; i < 3; i++) {
		ret = get_rawdata(panel_differ);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("get rawdata fail\n");
			goto restore_reg;
		}
	}

	for (i = 0; i < tdata->node.node_num; i++)
		panel_differ[i] = panel_differ[i] / 10;

	/* show test data */
	show_data(panel_differ, false);

	/* compare */
	tmp_result = compare_array(panel_differ, thr->panel_differ_min,
			thr->panel_differ_max, false);

restore_reg:
	/* set the origin value */
	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, fre);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("restore 0x0A fail,ret=%d\n", ret);

	/* wait fw state update */
	ret = wait_state_update(TEST_RETVAL_AA);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("wait state update fail\n");

	ret = fts_test_write_reg(FACTORY_REG_DATA_TYPE, data_type);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("set raw type fail,ret=%d\n", ret);

	ret = fts_test_write_reg(FACTORY_REG_GCB, g_cb);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("restore 0xBD fail,ret=%d\n", ret);

	ret = wait_state_update(TEST_RETVAL_AA);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("wait state update fail\n");

	ret = fts_test_write_reg(FACTORY_REG_NORMALIZE, normalize);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("restore normalize fail,ret=%d\n", ret);

test_err:
	/* result */
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ panel differ test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_ERR("------ panel differ test NG\n");
	}

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft5662_noise_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	int i = 0;
	u8 fre = 0;
	u8 reg1a_val = 0;
	u8 reg1b_val = 0;
	u8 touch_value = 0;
	u8 data_sel = 0;
	u8 frame_h_byte = 0;
	u8 frame_l_byte = 0;
	int nose_test_max = 0;
	int *noise = NULL;
	struct mc_sc_threshold *thr = &tdata->ic.mc_sc.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Noise Test\n");
	noise = tdata->item6_data;
	tdata->csv_item_cnt++;

	if (!noise) {
		FTS_TEST_SAVE_ERR("noise is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode,ret=%d\n", ret);
		goto test_err;
	}

	/* panel differ test in mapping mode */
	ret = mapping_switch(MAPPING);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("switch mapping fail,ret=%d\n", ret);
		goto test_err;
	}

	/* save origin value */
	ret = fts_test_read_reg(0x0D, &touch_value);
	if (ret) {
		FTS_TEST_SAVE_ERR("read touch_value fail,ret=%d\n", ret);
		goto test_err;
	}
	nose_test_max = touch_value * 4 * thr->basic.noise_max / 100;
	FTS_TEST_INFO("noise max::%d,%d", nose_test_max, touch_value);

	ret = fts_test_read_reg(FACTORY_REG_FRE_LIST, &fre);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0x0A fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_DATA_SELECT, &data_sel);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0x06 fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(0x1A, &reg1a_val);
	if (ret) {
		FTS_TEST_SAVE_ERR("read reg1a fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(0x1B, &reg1b_val);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read reg1b fail,ret=%d\n", ret);
		goto test_err;
	}
	FTS_TEST_INFO("fre:%d,data_sel:%d,reg1a:%d,reg1b:%d", fre, data_sel, reg1a_val, reg1b_val);


	ret = fts_test_write_reg(FACTORY_REG_DATA_SELECT, 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set data_sel fail,ret=%d\n", ret);
		goto restore_reg;
	}

	ret = wait_state_update(TEST_RETVAL_AA);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("wait state update fail\n");
		goto restore_reg;
	}

	ret = fts_test_write_reg(0x1A, 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set reg1A fail,ret=%d\n", ret);
		goto restore_reg;
	}

	frame_h_byte = BYTE_OFF_8(thr->basic.noise_framenum);
	frame_l_byte = BYTE_OFF_0(thr->basic.noise_framenum);
	FTS_TEST_INFO("noise frame num:%d,%d", frame_h_byte, frame_l_byte);
	ret = fts_test_write_reg(0x1c, frame_h_byte);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write 0x1c fail,ret=%d\n", ret);
		goto restore_reg;
	}

	ret = fts_test_write_reg(0x1d, frame_l_byte);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write 0x1d fail,ret=%d\n", ret);
		goto restore_reg;
	}

	FTS_TEST_INFO("noise_mode = %d", thr->basic.noise_mode);
	ret = fts_test_write_reg(0x1B, thr->basic.noise_mode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set reg1B fail,ret=%d\n", ret);
		goto restore_reg;
	}
	sys_delay(20);

	if (thr->basic.noise_polling) {
		FTS_TEST_INFO("fre_num:%d", tdata->fre_num);
		for (i = tdata->fre_num - 1; i > 0; i--) {
			ret = get_noise_ft5662(tdata, &noise[tdata->node.node_num * i], i);
			if (ret < 0) {
				FTS_TEST_SAVE_ERR("get noise fails,ret=%d\n", ret);
				goto restore_reg;
			}
		}
	}

	ret = get_noise_ft5662(tdata, &noise[0], 0x00);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get noise fails,ret=%d\n", ret);
		goto restore_reg;
	}

	/* show test data */
	show_data(noise, false);

	/* compare */
	tmp_result = compare_data(noise, 0, nose_test_max, 0, 0, false);

	get_null_noise(tdata);

restore_reg:
	/* set the origin value */
	ret = fts_test_write_reg(0x1A, reg1a_val);
	if (ret)
		FTS_TEST_SAVE_ERR("restore reg1a fail,ret=%d\n", ret);

	ret = fts_test_write_reg(0x1B, reg1b_val);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("restore reg1b fail,ret=%d\n", ret);

	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, fre);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("restore 0x0A fail,ret=%d\n", ret);

	ret = wait_state_update(TEST_RETVAL_AA);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("wait state update fail\n");

	ret = fts_test_write_reg(FACTORY_REG_DATA_SELECT, data_sel);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("restore data_sel fail,ret=%d\n", ret);

	ret = wait_state_update(TEST_RETVAL_AA);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("wait state update fail\n");

test_err:
	/* result */
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ noise test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_ERR("------ noise test NG\n");
	}

	FTS_TEST_FUNC_EXIT();
	return ret;
}


static int malloc_item_data(struct fts_test *tdata)
{
	tdata->item1_data = fts_malloc(tdata->node.node_num * sizeof(int));
	if (!tdata->item1_data) {
		FTS_TEST_SAVE_ERR("memory malloc fails");
		return -ENOMEM;
	}

	tdata->item2_data = fts_malloc(tdata->node.node_num * sizeof(int) * 2);
	if (!tdata->item2_data) {
		FTS_TEST_SAVE_ERR("memory malloc fails");
		return -ENOMEM;
	}

	tdata->item3_data = fts_malloc(tdata->sc_node.node_num * sizeof(int) * 6);
	if (!tdata->item3_data) {
		FTS_TEST_SAVE_ERR("memory malloc fails");
		return -ENOMEM;
	}

	tdata->item4_data = fts_malloc(tdata->sc_node.node_num * sizeof(int) * 3);
	if (!tdata->item4_data) {
		FTS_TEST_SAVE_ERR("memory malloc fails");
		return -ENOMEM;
	}

	tdata->item5_data = fts_malloc(tdata->node.node_num * sizeof(int));
	if (!tdata->item5_data) {
		FTS_TEST_SAVE_ERR("memory malloc fails");
		return -ENOMEM;
	}

	tdata->item6_data = fts_malloc(tdata->node.node_num * sizeof(int) * tdata->fre_num);
	if (!tdata->item6_data) {
		FTS_TEST_SAVE_ERR("memory malloc fails");
		return -ENOMEM;
	}

	return 0;
}

static void free_item_data(struct fts_test *tdata)
{
	fts_free(tdata->item1_data);
	fts_free(tdata->item2_data);
	fts_free(tdata->item3_data);
	fts_free(tdata->item4_data);
	fts_free(tdata->item5_data);
	fts_free(tdata->item6_data);
}

static int start_test_ft5662(void)
{
	int ret = 0;
	struct fts_test *tdata = fts_ftest;
	struct mc_sc_testitem *test_item = &tdata->ic.mc_sc.u.item;
	bool temp_result = false;
	bool test_result = true;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_INFO("test item:0x%x", tdata->ic.mc_sc.u.tmp);

	ret = fts_test_read_reg(0x14, &tdata->fre_num);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read fre_num fails");
		return ret;
	}
	FTS_TEST_INFO("fre_num:%d", tdata->fre_num);

	ret = malloc_item_data(tdata);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("memory malloc fails");
		return -ENOMEM;
	}

	/* rawdata test */
	if (true == test_item->rawdata_test) {
		ret = ft5662_rawdata_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result))
			test_result = false;
	}

	if (true == test_item->rawdata_uniformity_test) {
		ret = ft5662_uniformity_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result))
			test_result = false;
	}

	/* scap_cb test */
	if (true == test_item->scap_cb_test) {
		ret = ft5662_scap_cb_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result))
			test_result = false;
	}

	/* scap_rawdata test */
	if (true == test_item->scap_rawdata_test) {
		ret = ft5662_scap_rawdata_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result))
			test_result = false;
	}

	/* short test */
	if (true == test_item->short_test) {
		ret = ft5662_short_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result))
			test_result = false;
	}
	/* panel differ test */
	if (true == test_item->panel_differ_test) {
		ret = ft5662_panel_differ_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result))
			test_result = false;
	}

	/* noise test */
	if (true == test_item->noise_test) {
		ret = ft5662_noise_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result))
			test_result = false;
	}

	/* restore mapping state */
	fts_test_write_reg(FACTORY_REG_NOMAPPING, tdata->mapping);

	FTS_TEST_FUNC_EXIT();
	return test_result;
}

static int fts_open_test(void)
{
	int ret = 0;
	struct fts_test *tdata = fts_ftest;
	struct mc_sc_testitem *test_item = &tdata->ic.mc_sc.u.item;
	bool temp_result = false;
	bool test_result = true;

	FTS_TEST_FUNC_ENTER();
	ret = fts_test_read_reg(0x14, &tdata->fre_num);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read fre_num fails");
		return ret;
	}
	FTS_TEST_INFO("fre_num:%d", tdata->fre_num);

	ret = malloc_item_data(tdata);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("memory malloc fails");
		return -ENOMEM;
	}
	/* panel differ test */
	if (true == test_item->panel_differ_test) {
		ret = ft5662_panel_differ_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result))
			test_result = false;
	}

	if (test_result == true)
		ret = SELFTEST_PASS;
	else
		ret = SELFTEST_FAIL;
	fts_test_write_reg(FACTORY_REG_NOMAPPING, tdata->mapping);
	FTS_TEST_FUNC_EXIT();
	return ret;
}


static int fts_short_test(void)
{
	int ret = 0;
	struct fts_test *tdata = fts_ftest;
	struct mc_sc_testitem *test_item = &tdata->ic.mc_sc.u.item;
	bool temp_result = false;
	bool test_result = true;

	FTS_TEST_FUNC_ENTER();
	ret = fts_test_read_reg(0x14, &tdata->fre_num);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read fre_num fails");
		return ret;
	}
	FTS_TEST_INFO("fre_num:%d", tdata->fre_num);

	ret = malloc_item_data(tdata);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("memory malloc fails");
		return -ENOMEM;
	}

	/* short test */
	if (true == test_item->short_test) {
		ret = ft5662_short_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result))
			test_result = false;
	}

	if (test_result == true)
		ret = SELFTEST_PASS;
	else
		ret = SELFTEST_FAIL;
	fts_test_write_reg(FACTORY_REG_NOMAPPING, tdata->mapping);

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int fts_spi_test(void)
{
	int ret = 0;
	struct fts_test *tdata = fts_ftest;
	struct mc_sc_testitem *test_item = &tdata->ic.mc_sc.u.item;
	u8 chip_id[2] = { 0 };
	int cnt = 0;

	FTS_TEST_FUNC_ENTER();
	ret = fts_test_read_reg(0x14, &tdata->fre_num);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read fre_num fails");
		return ret;
	}
	FTS_TEST_INFO("fre_num:%d", tdata->fre_num);

	ret = malloc_item_data(tdata);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("memory malloc fails");
		return -ENOMEM;
	}
	do {
		ret = fts_read_reg(FTS_REG_CHIP_ID, &chip_id[0]);
		ret = fts_read_reg(FTS_REG_CHIP_ID2, &chip_id[1]);
		if ((ret < 0) || (chip_id[0] == 0x0) || (chip_id[1] == 0x0)) {
			cnt++;
			ret = SELFTEST_FAIL;
			msleep(100);
		} else {
			ret = SELFTEST_PASS;
			FTS_TEST_INFO("spi test success, Device id: 0x%02x%02x",
							chip_id[0], chip_id[1]);
			break;
		}
	} while (cnt < 10);

	fts_test_write_reg(FACTORY_REG_NOMAPPING, tdata->mapping);

	FTS_TEST_FUNC_EXIT();

	return ret;
}

static int param_init_ft5662(void)
{
	struct mc_sc_threshold *thr = &fts_ftest->ic.mc_sc.thr;

	get_value_basic("SCapCbTest_ON_Global_Cb_Min", &thr->basic.scap_cb_on_gcb_min);
	get_value_basic("SCapCbTest_ON_Global_Cb_Max", &thr->basic.scap_cb_on_gcb_max);
	get_value_basic("SCapCbTest_OFF_Global_Cb_Min", &thr->basic.scap_cb_off_gcb_min);
	get_value_basic("SCapCbTest_OFF_Global_Cb_Max", &thr->basic.scap_cb_off_gcb_max);
	get_value_basic("SCapCbTest_High_Global_Cb_Min", &thr->basic.scap_cb_hi_gcb_min);
	get_value_basic("SCapCbTest_High_Global_Cb_Max", &thr->basic.scap_cb_hi_gcb_max);
	FTS_TEST_INFO("GCB,On(%d,%d),Off(%d,%d),High(%d,%d)",
			thr->basic.scap_cb_on_gcb_min, thr->basic.scap_cb_on_gcb_max,
			thr->basic.scap_cb_off_gcb_min, thr->basic.scap_cb_off_gcb_max,
			thr->basic.scap_cb_hi_gcb_min, thr->basic.scap_cb_hi_gcb_max);

	get_value_basic("SCapCbTest_ON_Cf_Cb_Min", &thr->basic.scap_cb_on_cf_min);
	get_value_basic("SCapCbTest_ON_Cf_Cb_Max", &thr->basic.scap_cb_on_cf_max);
	get_value_basic("SCapCbTest_OFF_Cf_Cb_Min", &thr->basic.scap_cb_off_cf_min);
	get_value_basic("SCapCbTest_OFF_Cf_Cb_Max", &thr->basic.scap_cb_off_cf_max);
	get_value_basic("SCapCbTest_High_Cf_Cb_Min", &thr->basic.scap_cb_hi_cf_min);
	get_value_basic("SCapCbTest_High_Cf_Cb_Max", &thr->basic.scap_cb_hi_cf_max);

	get_value_basic("NoiseTest_Max", &thr->basic.noise_max);
	get_value_basic("NoiseTest_Frames", &thr->basic.noise_framenum);
	get_value_basic("NoiseTest_FwNoiseMode", &thr->basic.noise_mode);
	get_value_basic("Polling_Frequency", &thr->basic.noise_polling);
	FTS_TEST_INFO("noise_max:%d,frame_num:%d,noise_mode:%d,polling:%d", thr->basic.noise_max,
			thr->basic.noise_framenum, thr->basic.noise_mode, thr->basic.noise_polling);

	return 0;
}


static void save_data_ft5662(char *buf, int *data_length)
{
	struct fts_test *tdata = fts_ftest;
	struct mc_sc_testitem *test_item = &tdata->ic.mc_sc.u.item;
	struct mc_sc_threshold *thr = &fts_ftest->ic.mc_sc.thr;
	u32 cnt = 0;
	u32 tmp_cnt = 0;
	int *tmp_data = NULL;
	int line_num = 11;
	int i = 0;
	int j = 0;
	u8 tx = tdata->node.tx_num;
	u8 rx = tdata->node.rx_num;
	u8 sc_rx = (tdata->sc_node.tx_num > tdata->sc_node.rx_num) ? tdata->sc_node.tx_num : tdata->sc_node.rx_num;
	u8 tmp_rx = 0;
	u8 scap_cfg = 0xFF;
	bool cc_en = false;

	fts_test_read_reg(FACTROY_REG_SCAP_CFG, &scap_cfg);
	cc_en = !(scap_cfg & 0x80);
	FTS_TEST_INFO("cc_en:%d", cc_en);

	/* line 1 */
	cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "ECC, 85, 170, IC Name, %s, IC Code, %x\n", tdata->ini.ic_name, 0);

	/*line 2*/
	cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "TestItem Num, %d, ", tdata->csv_item_cnt);
	if (true == test_item->rawdata_test) {
		cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%s, %d, %d, %d, %d, %d, ", "Rawdata Test", CODE_M_RAWDATA_TEST, tx, rx, line_num, 2);
		line_num += tx;
	}

	if (true == test_item->rawdata_uniformity_test) {
		if (thr->basic.uniformity_check_tx) {
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%s, %d, %d, %d, %d, %d, ",
					"Rawdata Uniformity Test", CODE_M_RAWDATA_UNIFORMITY_TEST, tx, rx, line_num, 1);
			line_num += tx;
		}
		if (thr->basic.uniformity_check_rx) {
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%s, %d, %d, %d, %d, %d, ",
					"Rawdata Uniformity Test", CODE_M_RAWDATA_UNIFORMITY_TEST, tx, rx, line_num, 2);
			line_num += tx;
		}
	}

	if (true == test_item->scap_cb_test) {
		tmp_rx = (cc_en) ? sc_rx : 1;
		if (tdata->csv_item_scb & 0x01) {
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%s, %d, %d, %d, %d, %d, ", "SCAP CB Test", CODE_M_SCAP_CB_TEST, 2, sc_rx, line_num, 1);
			line_num += 2;
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%s, %d, %d, %d, %d, %d, ", "SCAP CB Test", CODE_M_SCAP_CB_TEST, 2, tmp_rx, line_num, 2);
			line_num += 2;
		}

		if (tdata->csv_item_scb & 0x02) {
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%s, %d, %d, %d, %d, %d, ", "SCAP CB Test", CODE_M_SCAP_CB_TEST, 2, sc_rx, line_num, 3);
			line_num += 2;
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%s, %d, %d, %d, %d, %d, ", "SCAP CB Test", CODE_M_SCAP_CB_TEST, 2, tmp_rx, line_num, 4);
			line_num += 2;
		}

		if (tdata->csv_item_scb & 0x04) {
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%s, %d, %d, %d, %d, %d, ", "SCAP CB Test", CODE_M_SCAP_CB_TEST, 2, sc_rx, line_num, 5);
			line_num += 2;
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%s, %d, %d, %d, %d, %d, ", "SCAP CB Test", CODE_M_SCAP_CB_TEST, 2, tmp_rx, line_num, 6);
			line_num += 2;
		}
	}

	if (true == test_item->scap_rawdata_test) {
		if (tdata->csv_item_sraw & 0x01) {
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%s, %d, %d, %d, %d, %d, ", "SCAP Rawdata Test", CODE_M_SCAP_RAWDATA_TEST, 2, sc_rx, line_num, 1);
			line_num += 2;
		}

		if (tdata->csv_item_sraw & 0x02) {
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%s, %d, %d, %d, %d, %d, ", "SCAP Rawdata Test", CODE_M_SCAP_RAWDATA_TEST, 2, sc_rx, line_num, 2);
			line_num += 2;
		}

		if (tdata->csv_item_sraw & 0x04) {
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%s, %d, %d, %d, %d, %d, ", "SCAP Rawdata Test", CODE_M_SCAP_RAWDATA_TEST, 2, sc_rx, line_num, 3);
			line_num += 2;
		}
	}

	if (true == test_item->panel_differ_test) {
		cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%s, %d, %d, %d, %d, %d, ", "Panel Differ Test", CODE_M_PANELDIFFER_TEST, tx, rx, line_num, 1);
		line_num += tx;
	}

	if (true == test_item->noise_test) {
		cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%s, %d, %d, %d, %d, %d, ", "Noise Test", CODE_M_NOISE_TEST, tx, rx, line_num, 1);
		line_num += tx;

		for (i = 1; thr->basic.noise_polling && (i < tdata->fre_num); i++) {
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%s, %d, %d, %d, %d, %d, ", "Noise Test", CODE_M_NOISE_TEST, tx, rx, line_num, 1 + i);
			line_num += tx;
		}

		cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%s, %d, %d, %d, %d, %d, ", "Null Noise", 41, 1, 1, line_num, 1);
		line_num += 1;
	}

	cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n\n\n\n\n\n\n\n\n");

	/*data*/
	if (true == test_item->rawdata_test) {
		for (i = 0; i < tdata->node.node_num; i++) {
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tdata->item1_data[i]);
			if (((i + 1) % tdata->node.rx_num) == 0)
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
		}
	}

	if (true == test_item->rawdata_uniformity_test) {
		tmp_cnt = 0;
		if (thr->basic.uniformity_check_tx) {
			tmp_data = tdata->item2_data + tmp_cnt;
			for (i = 0; i < tdata->node.node_num; i++) {
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
				if (((i + 1) % tdata->node.rx_num) == 0)
					cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			}
			tmp_cnt += tdata->node.node_num;
		}

		if (thr->basic.uniformity_check_rx) {
			tmp_data = tdata->item2_data + tmp_cnt;
			for (i = 0; i < tdata->node.node_num; i++) {
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
				if (((i + 1) % tdata->node.rx_num) == 0)
					cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			}
			tmp_cnt += tdata->node.node_num;
		}
	}

	if (true == test_item->scap_cb_test) {
		tmp_cnt = 0;
		if (tdata->csv_item_scb & 0x01) {
			tmp_data = tdata->item3_data + tmp_cnt;
			for (i = 0; i < tdata->sc_node.rx_num; i++)
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			for (i = tdata->sc_node.rx_num; i < tdata->sc_node.node_num; i++)
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			tmp_cnt += tdata->sc_node.node_num;

			tmp_data = tdata->item3_data + tmp_cnt;
			if (cc_en) {
				for (i = 0; i < tdata->sc_node.rx_num; i++)
					cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
				for (i = tdata->sc_node.rx_num; i < tdata->sc_node.node_num; i++)
					cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			} else {
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,\n", tmp_data[0]);
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,\n", tmp_data[tdata->sc_node.rx_num]);
			}
			tmp_cnt += tdata->sc_node.node_num;
		}

		if (tdata->csv_item_scb & 0x02) {
			tmp_data = tdata->item3_data + tmp_cnt;
			for (i = 0; i < tdata->sc_node.rx_num; i++)
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			for (i = tdata->sc_node.rx_num; i < tdata->sc_node.node_num; i++)
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			tmp_cnt += tdata->sc_node.node_num;

			tmp_data = tdata->item3_data + tmp_cnt;
			if (cc_en) {
				for (i = 0; i < tdata->sc_node.rx_num; i++)
					cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
				for (i = tdata->sc_node.rx_num; i < tdata->sc_node.node_num; i++)
					cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			} else {
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,\n", tmp_data[0]);
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,\n", tmp_data[tdata->sc_node.rx_num]);
			}
			tmp_cnt += tdata->sc_node.node_num;
		}

		if (tdata->csv_item_scb & 0x04) {
			tmp_data = tdata->item3_data + tmp_cnt;
			for (i = 0; i < tdata->sc_node.rx_num; i++)
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			for (i = tdata->sc_node.rx_num; i < tdata->sc_node.node_num; i++)
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			tmp_cnt += tdata->sc_node.node_num;

			tmp_data = tdata->item3_data + tmp_cnt;
			if (cc_en) {
				for (i = 0; i < tdata->sc_node.rx_num; i++)
					cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
				for (i = tdata->sc_node.rx_num; i < tdata->sc_node.node_num; i++)
					cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			} else {
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,\n", tmp_data[0]);
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,\n", tmp_data[tdata->sc_node.rx_num]);
			}
			tmp_cnt += tdata->sc_node.node_num;
		}
	}

	if (true == test_item->scap_rawdata_test) {
		tmp_cnt = 0;
		if (tdata->csv_item_sraw & 0x01) {
			tmp_data = tdata->item4_data + tmp_cnt;
			for (i = 0; i < tdata->sc_node.rx_num; i++)
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			for (i = tdata->sc_node.rx_num; i < tdata->sc_node.node_num; i++)
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			tmp_cnt += tdata->sc_node.node_num;
		}

		if (tdata->csv_item_sraw & 0x02) {
			tmp_data = tdata->item4_data + tmp_cnt;
			for (i = 0; i < tdata->sc_node.rx_num; i++)
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			for (i = tdata->sc_node.rx_num; i < tdata->sc_node.node_num; i++)
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			tmp_cnt += tdata->sc_node.node_num;
		}

		if (tdata->csv_item_sraw & 0x04) {
			tmp_data = tdata->item4_data + tmp_cnt;
			for (i = 0; i < tdata->sc_node.rx_num; i++)
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			for (i = tdata->sc_node.rx_num; i < tdata->sc_node.node_num; i++)
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			tmp_cnt += tdata->sc_node.node_num;
		}
	}

	if (true == test_item->panel_differ_test) {
		for (i = 0; i < tdata->node.node_num; i++) {
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tdata->item5_data[i]);
			if (((i + 1) % tdata->node.rx_num) == 0)
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
		}
	}

	if (true == test_item->noise_test) {
		for (i = 0; i < tdata->node.node_num; i++) {
			cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tdata->item6_data[i]);
			if (((i + 1) % tdata->node.rx_num) == 0)
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
		}

		for (j = 1; thr->basic.noise_polling && (j < tdata->fre_num); j++) {
			tmp_data = &tdata->item6_data[tdata->node.node_num * j];
			for (i = 0; i < tdata->node.node_num; i++) {
				cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,", tmp_data[i]);
				if (((i + 1) % tdata->node.rx_num) == 0)
					cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "\n");
			}
		}
		cnt += snprintf(buf + cnt, CSV_BUFFER_LEN - cnt, "%d,\n", tdata->null_noise_max);
	}

	*data_length = cnt;
	free_item_data(tdata);
}

static int ft3658_get_rawdata(bool is_raw, int *data_buffer, int byte_num)
{
	int ret = 0;
	int i = 0;
	u8 data_type = 0;
	u8 data_sel = 0;
	u8 fre = 0;
	u8 *data = NULL;

	data = (u8 *)fts_malloc(byte_num * sizeof(u8));
	if (data == NULL) {
		FTS_TEST_SAVE_ERR("raw/diff data buffer malloc fail\n");
		return -ENOMEM;
	}

	/* select rawdata */
	ret = fts_test_write_reg(FACTORY_REG_DATA_SELECT, is_raw ? 0x00 : 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set fir fail,ret=%d\n", ret);
		goto restore_reg;
	}

	ret = start_scan();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("scan fail\n");
		goto restore_reg;
	}

	ret = fts_test_write_reg(FACTORY_REG_LINE_ADDR, 0xAA);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("write line addr fail\n");

	ret = fts_test_read(FACTORY_REG_RAWDATA_ADDR_MC_SC, data, byte_num);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read mass data fail\n");
		goto restore_reg;
	}

	for (i = 0; i < byte_num; i = i + 2)
		data_buffer[i >> 1] = (int)(short)((data[i] << 8) + data[i + 1]);
	ret = 0;
restore_reg:
	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, fre);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("restore 0x0A fail,ret=%d\n", ret);

	ret = fts_test_write_reg(FACTORY_REG_DATA_TYPE, data_type);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("restore 0x5B fail,ret=%d\n", ret);

	ret = fts_test_write_reg(FACTORY_REG_DATA_SELECT, data_sel);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("restore 0x06 fail,ret=%d\n", ret);
	return ret;
}

static int ft3658_data_dump(int *rawdata, int *differ_data)
{
	int ret = 0;
	struct fts_test *tdata = fts_ftest;

	FTS_TEST_FUNC_ENTER();
	ret = ft3658_get_rawdata(true, rawdata, tdata->node.node_num * 2);
	if (ret) {
		FTS_TEST_ERROR("get rawdata error!");
		goto out;
	}
	ret = ft3658_get_rawdata(false, differ_data, tdata->node.node_num * 2);
	if (ret) {
		FTS_TEST_ERROR("get differ_data error!");
		goto out;
	}
out:
	FTS_TEST_FUNC_EXIT();
	return ret;
}

struct test_funcs test_func_ft5662 = {
	.ctype = {0x8A},
	.hwtype = IC_HW_MC_SC,
	.key_num_total = 0,
	.mc_sc_short_v2 = true,
	.cb_high_support = true,
	.param_update_support = true,
	.param_init = param_init_ft5662,
	.start_test = start_test_ft5662,
	.open_test = fts_open_test,
	.short_test = fts_short_test,
	.spi_test = fts_spi_test,
	.data_dump = ft3658_data_dump,
	.save_data_private = save_data_ft5662,
};
