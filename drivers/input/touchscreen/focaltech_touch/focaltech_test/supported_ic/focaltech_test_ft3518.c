/************************************************************************
* Copyright (C) 2012-2019, Focaltech Systems (R), All Rights Reserved.
* Copyright (C) 2020 XiaoMi, Inc.
*
* File Name: Focaltech_test_ft5452.c
*
* Author: Focaltech Driver Team
*
* Created: 2018-03-08
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

static bool short_test_get_channel_num(struct fts_test *tdata, int *fm_short_resistance, int *adc_data, int offset, bool *is_weak_short_mut)
{
	int ret = 0;
	int i;
	int count = 0;
	int tx_num;
	int rx_num;
	int total_num;
	int max_tx;
	int all_adc_data_num;
	int res_stalls = 111;
	int code_1 = 1437;
	int code_0 = 1437;
	struct mc_sc_threshold *thr = &fts_ftest->ic.mc_sc.thr;
	int min_cc = thr->basic.short_cc;
	bool tmp_result = true;

	tx_num = tdata->sc_node.tx_num;
	rx_num = tdata->sc_node.rx_num;
	total_num = tx_num + rx_num;
	max_tx = tx_num;
	all_adc_data_num = 1 + tx_num + rx_num;

	for ( i = 0; i < 1; i++) {
		ret = short_get_adc_data_mc(TEST_RETVAL_00, all_adc_data_num * 2, adc_data, FACTROY_REG_SHORT_CA);
		sys_delay(50);
		if (ret) {
			tmp_result = false;
			FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d\n", ret);
			goto TEST_END;
		}
	}

	/* print all Adc value */
	FTS_TEST_SAVE_INFO("Offset:%4d, Code0:%4d, Code1:%4d, \n", adc_data[0], code_0, code_1);
	show_data_mc_sc(&adc_data[1]);

	count = 0;
	for ( i = 0; i < total_num; i++) {
		if (code_1 - adc_data[i] <= 0) {
			fm_short_resistance[i] = min_cc;
			continue;
		}

		fm_short_resistance[i] = (adc_data[i] - offset + 395) * res_stalls / (code_1 - adc_data[i]) - 3;
		if (fm_short_resistance[i] < 0)
			fm_short_resistance[i] = abs(fm_short_resistance[i]);

		if (min_cc > fm_short_resistance[i]) {
			count++;
		}
	}

	if (count > 0) {
		*is_weak_short_mut = true;
	}

TEST_END:
	return tmp_result;
}

static bool short_test_channel_to_gnd(struct fts_test *tdata, int *fm_short_resistance, int *adc_data, int offset, bool *is_weak_short_gnd)
{
	int ret = 0;
	int error_num = 0;
	int min_70k_num = 0;
	int total_num;
	int tx_num;
	int rx_num;
	int min_cg = 0;
	int min_cc = 0;
	int i;
	int num;
	int code_1 = 1437;
	int code_0 = 1437;
	int res_stalls = 111;
	int fvalue = 0;
	int all_adc_data_num = 0;
	int res_stalls0 = 4;
	int *fg_short_resistance = NULL;
	int *tmp_adc_data = NULL;
	u8 *w_buf = NULL;
	u8 *error_ch = NULL;
	u8 *min_70k_ch = NULL ;
	bool is_used = false;
	bool tmp_result = true;
	struct mc_sc_threshold *thr = &fts_ftest->ic.mc_sc.thr;

	tx_num = tdata->sc_node.tx_num;
	rx_num = tdata->sc_node.rx_num;
	total_num = tx_num + rx_num;
	all_adc_data_num = 1 + tx_num + rx_num;

	min_cc = thr->basic.short_cc;
	min_cg = thr->basic.short_cg;

	error_ch = fts_malloc(total_num * sizeof(u8));
	if (NULL == error_ch) {
		FTS_TEST_SAVE_ERR("error_ch buffer malloc fail\n");
		goto TEST_END;
	}
	min_70k_ch = fts_malloc(total_num * sizeof(u8));
	if (NULL == min_70k_ch) {
		FTS_TEST_SAVE_ERR("min_70k_ch buffer malloc fail\n");
		goto TEST_END;
	}

	w_buf = fts_malloc((total_num + 3) * sizeof(u8));
	if (NULL == w_buf) {
		FTS_TEST_SAVE_ERR("w_buf buffer malloc fail\n");
		goto TEST_END;
	}

	tmp_adc_data = fts_malloc(total_num * total_num * sizeof(int));
	if (NULL == tmp_adc_data) {
		FTS_TEST_SAVE_ERR("adc_data buffer malloc fail\n");
		goto TEST_END;
	}
	memset(tmp_adc_data, 0, (total_num * total_num));

	fg_short_resistance =  fts_malloc(total_num * sizeof(int));
	if (NULL == fg_short_resistance) {
		FTS_TEST_SAVE_ERR("fg_short_resistance buffer malloc fail\n");
		goto TEST_END;
	}
	memset(fg_short_resistance, 0, total_num);

	error_num = 0;
	min_70k_num = 0;

	for ( i = 0; i < total_num; i++) {
		if (fm_short_resistance[i] < min_cc) {
			error_ch[error_num] = (u8)(i + 1);
			error_num++;
		}
	}

	if (error_num > 0) {
		w_buf[0] = (u8)error_num;
		for ( i = 0; i < error_num; i++) {
			w_buf[1 + i] = error_ch[i];
		}
		ret = fts_test_write(FACTROY_REG_SHORT_AB_CH, w_buf, error_num + 1);
		for ( i = 0; i < 1; i++) {
			ret = short_get_adc_data_mc(TEST_RETVAL_00, error_num * 2, tmp_adc_data, FACTROY_REG_SHORT_CG);
			sys_delay(50);
			if (ret) {
				FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d\n", ret);
				tmp_result = false;
				goto TEST_END;
			}
		}

		for ( i = 0; i < error_num; i++) {
			if (code_1 - tmp_adc_data[i] <= 0) {
				fg_short_resistance[i] = min_cg;
				continue;
			}
			fvalue = (tmp_adc_data[i] - offset + 395) * res_stalls / (code_1 - tmp_adc_data[i]) - 3;

			if (fvalue < 0) fvalue = abs(fg_short_resistance[i]);
			if (min_cg > fvalue) {
				fg_short_resistance[error_ch[i] - 1] = fvalue;
				adc_data[error_ch[i] - 1] = tmp_adc_data[i];
				*is_weak_short_gnd = true;
				if (fvalue > 70) {
					if (error_ch[i] <= tx_num) {
						FTS_TEST_SAVE_INFO("Tx%d with GND ", error_ch[i]);
					} else {
						FTS_TEST_SAVE_INFO( "Rx%d with GND ", (error_ch[i] - tx_num) );
					}

					FTS_TEST_SAVE_INFO(" Resistance: %2d, ADC: %d\n", fvalue, tmp_adc_data[i]);
					tmp_result = false;
				}

				if (fvalue < 70) {
					is_used = false;
					for ( num = 0; num < min_70k_num; num++) {
						if (error_ch[i] == min_70k_ch[num]) {
							is_used = true;
							break;
						}
					}
					if (!is_used) {
						min_70k_ch[min_70k_num] = error_ch[i];
						min_70k_num++;
					}
				}
			}
		}
	}

	if (min_70k_num > 0) {
		ret = fts_test_write_reg(FACTROY_REG_SHORT_DELAY, 0x00);
		if (ret) {
			goto TEST_END;
		}
		memset(tmp_adc_data, 0, (all_adc_data_num + 1));
		w_buf[0] = (u8)min_70k_num;
		for ( i = 0; i < min_70k_num; i++) {
			w_buf[1 + i] = min_70k_ch[i];
		}
		ret = fts_test_write(FACTROY_REG_SHORT_AB_CH, w_buf, min_70k_num + 1);
		for ( i = 0; i < 1; i++) {
			ret = short_get_adc_data_mc(TEST_RETVAL_00, min_70k_num * 2, tmp_adc_data, FACTROY_REG_SHORT_CG);
			sys_delay(50);
			if (ret) {
				FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d\n", ret);
				tmp_result = false;
				goto TEST_END;
			}
		}

		for ( i = 0; i < min_70k_num; i++) {
			if (code_0 - tmp_adc_data[i] <= 0) {
				fg_short_resistance[i] = min_cg;
				continue;
			}

			fvalue = (tmp_adc_data[i] - offset + 395) * res_stalls0 / (code_0 - tmp_adc_data[i]) - 3;

			if (fvalue < 0) fvalue = abs(fg_short_resistance[i]);
			if (min_cg > fvalue) {
				fg_short_resistance[min_70k_ch[i] - 1] = fvalue;
				adc_data[min_70k_ch[i] - 1] = tmp_adc_data[i];
				if (min_70k_ch[i] <= tx_num) {
					FTS_TEST_SAVE_INFO("Tx%d with GND", min_70k_ch[i]);
				} else {
					FTS_TEST_SAVE_INFO( "Rx%d with GND", (min_70k_ch[i] - tx_num));
				}

				FTS_TEST_SAVE_INFO(" Resistance: %d, ADC: %d\n", fvalue, tmp_adc_data[i]);
				tmp_result = false;
			}
		}
	}

TEST_END:

	fts_free(error_ch);
	fts_free (min_70k_ch);
	fts_free (tmp_adc_data);
	fts_free(fg_short_resistance);
	fts_free(w_buf);

	return tmp_result;
}

static bool short_test_channel_to_channel(struct fts_test *tdata, int *fm_short_resistance, int *adc_data, int offset)
{
	int ret = 0;
	int error_num = 0;
	int min_70k_num = 0;
	int total_num;
	int rx_num;
	int tx_num;
	int min_cc = 0;
	int i;
	int j;
	int num;
	int code_1 = 1437;
	int code_0 = 1437;
	int res_stalls = 111;
	int fvalue = 0;
	int adc_count = 0;
	int all_adc_data_num = 0;
	int res_stalls0 = 4;
	int *f_origin_resistance = NULL;
	int *tmp_adc_data = NULL;
	u8 *w_buf = NULL;
	u8 *error_ch = NULL;
	u8 *min_70k_ch = NULL ;
	bool is_used = false;
	bool tmp_result = true;
	struct mc_sc_threshold *thr = &fts_ftest->ic.mc_sc.thr;

	tx_num = tdata->sc_node.tx_num;
	rx_num = tdata->sc_node.rx_num;
	total_num = tx_num + rx_num;
	all_adc_data_num = 1 + tx_num + rx_num;

	min_cc = thr->basic.short_cc;

	f_origin_resistance =  fts_malloc(total_num * sizeof(int));
	if (NULL == f_origin_resistance) {
		FTS_TEST_SAVE_ERR("f_origin_resistance buffer malloc fail \n");
		goto TEST_END;
	}
	memset(f_origin_resistance, 0, total_num);

	tmp_adc_data = fts_malloc(total_num * total_num * sizeof(int));
	if (NULL == tmp_adc_data) {
		FTS_TEST_SAVE_ERR("adc_data buffer malloc fail \n");
		goto TEST_END;
	}
	memset(tmp_adc_data, 0, (total_num * total_num));


	for ( i = 0; i < total_num; i++) {
		f_origin_resistance[i] = fm_short_resistance[i];
	}

	ret = fts_test_write_reg(FACTROY_REG_SHORT_DELAY, 0x01);
	if (ret) {
		goto TEST_END;
	}

	w_buf = fts_malloc((total_num + 3) * sizeof(u8));
	if (NULL == w_buf) {
		FTS_TEST_SAVE_ERR("w_buf buffer malloc fail\n");
		return -ENOMEM;
	}

	error_ch = fts_malloc(total_num * sizeof(u8));
	if (NULL == error_ch) {
		FTS_TEST_SAVE_ERR("error_ch buffer malloc fail\n");
		goto TEST_END;
	}
	min_70k_ch =  fts_malloc(total_num * sizeof(u8));
	if (NULL == min_70k_ch) {
		FTS_TEST_SAVE_ERR("min_70k_ch buffer malloc fail\n");
		goto TEST_END;
	}
	error_num = 0;
	min_70k_num = 0;

	for (i = 0; i < total_num; i++) {
		if (f_origin_resistance[i] < min_cc) {
			error_ch[error_num] = (u8)(i + 1);
			error_num++;
		}
	}
	if (error_num > 1) {
		w_buf[0] = (u8)error_num;
		for ( i = 0; i < error_num; i++) {
			w_buf[1 + i] = error_ch[i];
		}
		memset(tmp_adc_data, 0, (all_adc_data_num + 1));

		ret = fts_test_write(FACTROY_REG_SHORT_AB_CH, w_buf, error_num + 1);
		for ( j = 0; j < 1; j++) {
			ret = short_get_adc_data_mc(TEST_RETVAL_00, error_num * (error_num - 1) * 2 / 2, tmp_adc_data, FACTROY_REG_SHORT_CC);
			sys_delay(50);
			if (ret) {
				FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d\n", ret);
				tmp_result = false;
				goto TEST_END;
			}
		}

		adc_count = 0;
		for (i = 0; i < error_num; i++) {
			for (j = i + 1; j < error_num; j++) {
				if (code_1 - tmp_adc_data[adc_count] <= 0) {
					fvalue = min_cc;
					continue;
				}
				fvalue = (tmp_adc_data[adc_count] - offset + 395) * res_stalls / (code_1 - tmp_adc_data[adc_count]) - 3;
				adc_count++;
				if (fvalue < 0) fvalue = abs(fvalue);
				if (min_cc > fvalue) {
					fm_short_resistance[error_ch[i] - 1] = fvalue;
					fm_short_resistance[error_ch[j] - 1] = fvalue;
					adc_data[error_ch[i] - 1] = tmp_adc_data[adc_count];
					adc_data[error_ch[j] - 1] = tmp_adc_data[adc_count];

					if (fvalue > 70) {
						if (error_ch[i] <= tx_num) {
							FTS_TEST_SAVE_INFO("Tx%d ", (error_ch[i]));
						} else {
							FTS_TEST_SAVE_INFO("Rx%d ", (error_ch[i] - tx_num));
						}

						if (error_ch[j] <= tx_num) {
							FTS_TEST_SAVE_INFO("Tx%d ", (error_ch[j] ) );
						} else {
							FTS_TEST_SAVE_INFO( "Rx%d ", (error_ch[j] - tx_num));
						}
						FTS_TEST_SAVE_INFO(": Resistance: %d , ADC: %d\n", fvalue, tmp_adc_data[adc_count]);
						tmp_result = false;
					} else {
						is_used = false;
						for (num = 0; num < min_70k_num; num++) {
							if (error_ch[i] == min_70k_ch[num]) {
								is_used = true;
								break;
							}
						}
						if (!is_used) {
							min_70k_ch[min_70k_num] = error_ch[i];
							min_70k_num++;
						}
						is_used = false;
						for (num = 0; num < min_70k_num; num++) {
							if (error_ch[j] == min_70k_ch[num]) {
								is_used = true;
								break;
							}
						}
						if (!is_used) {
							min_70k_ch[min_70k_num] = error_ch[j];
							min_70k_num++;
						}
					}
				}
			}
		}
	}

	if (min_70k_num > 0) {
		ret = fts_test_write_reg(FACTROY_REG_SHORT_DELAY, 0x00);
		if (ret) {
			goto TEST_END;
		}
		w_buf[0] = (u8)min_70k_num + 1;
		for (i = 0; i < min_70k_num; i++) {
			w_buf[1 + i] = min_70k_ch[i];
		}
		ret = fts_test_write(FACTROY_REG_SHORT_AB_CH, w_buf, min_70k_num + 1);
		memset(tmp_adc_data, 0, (all_adc_data_num + 1));
		for (i = 0; i < 1; i++) {
			ret = short_get_adc_data_mc(TEST_RETVAL_00, min_70k_num * (min_70k_num - 1) * 2 / 2, tmp_adc_data, FACTROY_REG_SHORT_CC);
			sys_delay(50);
			if (ret) {
				FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d\n", ret);
				tmp_result = false;
				goto TEST_END;
			}
		}
		adc_count = 0;
		for (i = 0; i < min_70k_num; i++) {
			for (j = i + 1; j < min_70k_num; j++) {
				if (0 >= code_0 - tmp_adc_data[adc_count]) {
					fvalue = min_cc;
					continue;
				}
				fvalue = (tmp_adc_data[adc_count] - offset + 395) * res_stalls0 / (code_0 - tmp_adc_data[adc_count]) - 3;
				adc_count++;
				if (fvalue < 0) fvalue = abs(fvalue);
				if (min_cc > fvalue) {
					fm_short_resistance[min_70k_ch[i] - 1] = fvalue;
					fm_short_resistance[min_70k_ch[j] - 1] = fvalue;
					adc_data[min_70k_ch[i] - 1] = tmp_adc_data[adc_count];
					adc_data[min_70k_ch[j] - 1] = tmp_adc_data[adc_count];
					if (min_70k_ch[i] <= tx_num) {
						FTS_TEST_SAVE_INFO("Tx%d ", (min_70k_ch[i]));
					} else {
						FTS_TEST_SAVE_INFO( "Rx%d ", (min_70k_ch[i] - tx_num));
					}

					if (min_70k_ch[j] <= tx_num) {
						FTS_TEST_SAVE_INFO("Tx%d ", (min_70k_ch[j]));
					} else {
						FTS_TEST_SAVE_INFO( "Rx%d ", (min_70k_ch[j] - tx_num));
					}

					FTS_TEST_SAVE_INFO(":Resistance: %d, ADC: %d\n", fvalue, tmp_adc_data[adc_count]);

					tmp_result = false;
				}
			}
		}
	}

TEST_END:

	fts_free(error_ch);
	fts_free(min_70k_ch);
	fts_free(w_buf);
	fts_free(f_origin_resistance);
	fts_free(tmp_adc_data);


	return tmp_result;
}
static int ft5452_rawdata_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	int i = 0;
	int *rawdata = NULL;
	u8 fre = 0;
	u8 fir = 0;
	u8 normalize = 0;
	bool result = false;
	struct mc_sc_threshold *thr = &tdata->ic.mc_sc.thr;

	FTS_TEST_SAVE_INFO("\n============ Test Item: rawdata test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	rawdata = tdata->buffer;

	if (!thr->rawdata_h_min || !thr->rawdata_h_max || !test_result) {
		FTS_TEST_SAVE_ERR("rawdata_h_min/max test_result is null\n");
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
	ret = fts_test_read_reg(FACTORY_REG_NORMALIZE, &normalize);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read normalize fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_FRE_LIST, &fre);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0x0A fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_FIR, &fir);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0xFB error,ret=%d\n", ret);
		goto test_err;
	}

	/* set to auto normalize */
	if (normalize != 0x01) {
		ret = fts_test_write_reg(FACTORY_REG_NORMALIZE, 0x01);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("write normalize fail,ret=%d\n", ret);
			goto restore_reg;
		}
	}

	/* set frequecy high */
	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, 0x81);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set frequecy fail,ret=%d\n", ret);
		goto restore_reg;
	}

	/* fir enable */
	ret = fts_test_write_reg(FACTORY_REG_FIR, 1);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set fir fail,ret=%d\n", ret);
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
	result = compare_array(rawdata,
						   thr->rawdata_h_min,
						   thr->rawdata_h_max,
						   false);

restore_reg:
	/* set the origin value */
	ret = fts_test_write_reg(FACTORY_REG_NORMALIZE, normalize);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore normalize fail,ret=%d\n", ret);
	}

	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, fre);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore 0x0A fail,ret=%d\n", ret);
	}

	ret = fts_test_write_reg(FACTORY_REG_FIR, fir);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore 0xFB fail,ret=%d\n", ret);
	}

test_err:
	if (result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ rawdata test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("------ rawdata test NG\n");
	}

	/* save test data */
	fts_test_save_data("Rawdata Test", CODE_M_RAWDATA_TEST,
					   rawdata, 0, false, false, *test_result);

	return ret;
}

static int ft5452_uniformity_test(struct fts_test *tdata, bool *test_result)
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
	u8 fir = 0;
	struct mc_sc_threshold *thr = &fts_ftest->ic.mc_sc.thr;
	bool result = false;
	bool result2 = false;
	bool result3 = false;

	FTS_TEST_SAVE_INFO("\n============ Test Item: rawdata unfiormity test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	rawdata = tdata->buffer;
	tx_num = tdata->node.tx_num;
	rx_num = tdata->node.rx_num;

	if (!thr->tx_linearity_max || !thr->rx_linearity_max
		|| !tdata->node_valid || !test_result) {
		FTS_TEST_SAVE_ERR("tx/rx_lmax/node_valid/test_result is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode,ret=%d\n", ret);
		goto test_err;
	}

	rawdata_linearity = fts_malloc(tdata->node.node_num * 2 * sizeof(int));
	if (!rawdata_linearity) {
		FTS_TEST_SAVE_ERR("rawdata_linearity buffer malloc fail");
		ret = -ENOMEM;
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

	ret = fts_test_read_reg(FACTORY_REG_FIR, &fir);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0xFB error,ret=%d\n", ret);
		goto test_err;
	}

	/* set frequecy high */
	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, 0x81);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set frequecy fail,ret=%d\n", ret);
		goto restore_reg;
	}
	sys_delay(10);

	/* fir enable */
	ret = fts_test_write_reg(FACTORY_REG_FIR, 1);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set fir fail,ret=%d\n", ret);
		goto restore_reg;
	}
	sys_delay(10);

	/* change register value before,need to lose 3 frame data */
	for (index = 0; index < 3; ++index) {
		ret = get_rawdata(rawdata);
	}
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get rawdata fail,ret=%d\n", ret);
		goto restore_reg;
	}

	result = true;
	if (thr->basic.uniformity_check_tx) {
		FTS_TEST_SAVE_INFO("Check Tx Linearity\n");
		rl_tmp = rawdata_linearity + rl_cnt;
		for (row = 0; row < tx_num; row++) {
			for (col = 1; col <  rx_num; col++) {
				offset = row * rx_num + col;
				offset2 = row * rx_num + col - 1;
				deviation = abs( rawdata[offset] - rawdata[offset2]);
				max = max(rawdata[offset], rawdata[offset2]);
				max = max ? max : 1;
				rl_tmp[offset] = 100 * deviation / max;
			}
		}
		/*show data in result.txt*/
		FTS_TEST_SAVE_INFO(" Tx Linearity:\n");
		show_data(rl_tmp, false);
		FTS_TEST_SAVE_INFO("\n" );

		/* compare */
		result = compare_array(rl_tmp,
							   thr->tx_linearity_min,
							   thr->tx_linearity_max,
							   false);

		rl_cnt += tdata->node.node_num;
	}

	result2 = true;
	if (thr->basic.uniformity_check_rx) {
		FTS_TEST_SAVE_INFO("Check Rx Linearity\n");
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
		result2 = compare_array(rl_tmp,
								thr->rx_linearity_min,
								thr->rx_linearity_max,
								false);
		rl_cnt += tdata->node.node_num;
	}

	result3 = true;
	if (thr->basic.uniformity_check_min_max) {
		FTS_TEST_SAVE_INFO("Check Min/Max\n") ;
		min = 100000;
		max = -100000;
		for (i = 0; i < tdata->node.rx_num; i++) {
			if (0 == tdata->node_valid[i])
				continue;
			min = min(min, rawdata[(row * rx_num) + col]);
			max = max(max, rawdata[(row * rx_num) + col]);
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
	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, fre);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore 0x0A fail,ret=%d\n", ret);
	}

	ret = fts_test_write_reg(FACTORY_REG_FIR, fir);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore 0xFB fail,ret=%d\n", ret);
	}

test_err:
	fts_free(rawdata_linearity);

	if (result && result2 && result3) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("uniformity test is OK\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_ERR("uniformity test is NG\n");
	}

	fts_test_save_data("Rawdata Uniformity Test",
					   CODE_M_RAWDATA_UNIFORMITY_TEST,
					   rawdata_linearity, 0, false, false, *test_result);

	return ret;
}

static int ft5452_scap_cb_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	bool tmp2_result = false;
	u8 wc_sel = 0;
	u8 sc_mode = 0;
	int byte_num = 0;
	bool fw_wp_check = false;
	bool tx_check = false;
	bool rx_check = false;
	int *scap_cb = NULL;
	int *scb_tmp = NULL;
	int scb_cnt = 0;
	struct mc_sc_threshold *thr = &tdata->ic.mc_sc.thr;

	FTS_TEST_SAVE_INFO("\n============ Test Item: Scap CB Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	scap_cb = tdata->buffer;
	byte_num = tdata->sc_node.node_num * 2;

	if ((tdata->sc_node.node_num * 2) > tdata->buffer_length) {
		FTS_TEST_SAVE_ERR("scap cb num(%d) > buffer length(%d)",
						  tdata->sc_node.node_num * 2,
						  tdata->buffer_length);
		ret = -EINVAL;
		goto test_err;
	}

	if (!thr->scap_cb_on_min || !thr->scap_cb_on_max
		|| !thr->scap_cb_off_min || !thr->scap_cb_off_max || !test_result) {
		FTS_TEST_SAVE_ERR("scap_cb_on/off_min/max test_result is null\n");
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

	/* get waterproof channel select */
	ret = fts_test_read_reg(FACTORY_REG_WC_SEL, &wc_sel);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read water_channel_sel fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_MC_SC_MODE, &sc_mode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read sc_mode fail,ret=%d\n", ret);
		goto test_err;
	}

	/* water proof on check */
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_ON);
	if (thr->basic.scap_cb_wp_on_check && fw_wp_check) {
		scb_tmp = scap_cb + scb_cnt;
		/* 1:waterproof 0:non-waterproof */
		ret = get_cb_mc_sc(WATER_PROOF_ON, byte_num, scb_tmp, DATA_TWO_BYTE);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read sc_cb fail,ret=%d\n", ret);
			goto restore_reg;
		}

		/* show Scap CB */
		FTS_TEST_SAVE_INFO("scap_cb in waterproof on mode:\n");
		show_data_mc_sc(scb_tmp);

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_RX);
		tmp_result = compare_mc_sc(tx_check, rx_check, scb_tmp,
								   thr->scap_cb_on_min,
								   thr->scap_cb_on_max);

		scb_cnt += tdata->sc_node.node_num;
	} else {
		tmp_result = true;
	}

	/* water proof off check */
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_OFF);
	if (thr->basic.scap_cb_wp_on_check && fw_wp_check) {
		scb_tmp = scap_cb + scb_cnt;
		/* 1:waterproof 0:non-waterproof */
		ret = get_cb_mc_sc(WATER_PROOF_OFF, byte_num, scb_tmp, DATA_TWO_BYTE);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read sc_cb fail,ret=%d\n", ret);
			goto restore_reg;
		}

		/* show Scap CB */
		FTS_TEST_SAVE_INFO("scap_cb in waterproof off mode:\n");
		show_data_mc_sc(scb_tmp);

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_RX);
		tmp2_result = compare_mc_sc(tx_check, rx_check, scb_tmp,
									thr->scap_cb_off_min,
									thr->scap_cb_off_max);

		scb_cnt += tdata->sc_node.node_num;
	} else {
		tmp2_result = true;
	}

restore_reg:
	ret = fts_test_write_reg(FACTORY_REG_MC_SC_MODE, sc_mode);/* set the origin value */
	if (ret) {
		FTS_TEST_SAVE_ERR("write sc mode fail,ret=%d\n", ret);
	}
test_err:
	if (tmp_result && tmp2_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n------ scap cb test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_ERR("\n------ scap cb test NG\n");
	}

	/* save test data */
	fts_test_save_data("SCAP CB Test", CODE_M_SCAP_CB_TEST,
					   scap_cb, scb_cnt, true, false, *test_result);

	return ret;
}

static int ft5452_scap_rawdata_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	bool tmp2_result = false;
	u8 wc_sel = 0;
	bool fw_wp_check = false;
	bool tx_check = false;
	bool rx_check = false;
	int *scap_rawdata = NULL;
	int *srawdata_tmp = NULL;
	int srawdata_cnt = 0;
	struct mc_sc_threshold *thr = &tdata->ic.mc_sc.thr;

	FTS_TEST_SAVE_INFO("\n============ Test Item: Scap Rawdata Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	scap_rawdata = tdata->buffer;

	if ((tdata->sc_node.node_num * 2) > tdata->buffer_length) {
		FTS_TEST_SAVE_ERR("scap rawdata num(%d) > buffer length(%d)",
						  tdata->sc_node.node_num * 2,
						  tdata->buffer_length);
		ret = -EINVAL;
		goto test_err;
	}

	if (!thr->scap_rawdata_on_min || !thr->scap_rawdata_on_max
		|| !thr->scap_rawdata_off_min || !thr->scap_rawdata_off_max
		|| !test_result) {
		FTS_TEST_SAVE_ERR("scap_rawdata_on/off_min/max test_result is null\n");
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

	/* scan rawdata */
	ret = start_scan();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("scan scap rawdata fail\n");
		goto test_err;
	}

	/* water proof on check */
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_ON);
	if (thr->basic.scap_rawdata_wp_on_check && fw_wp_check) {
		srawdata_tmp = scap_rawdata + srawdata_cnt;
		ret = get_rawdata_mc_sc(WATER_PROOF_ON, srawdata_tmp);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("get scap(WP_ON) rawdata fail\n");
			goto test_err;
		}

		FTS_TEST_SAVE_INFO("scap_rawdata in waterproof on mode:\n");
		show_data_mc_sc(srawdata_tmp);

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_RX);
		tmp_result = compare_mc_sc(tx_check, rx_check, srawdata_tmp,
								   thr->scap_rawdata_on_min,
								   thr->scap_rawdata_on_max);

		srawdata_cnt += tdata->sc_node.node_num;
	} else {
		tmp_result = true;
	}

	/* water proof off check */
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_OFF);
	if (thr->basic.scap_rawdata_wp_on_check && fw_wp_check) {
		srawdata_tmp = scap_rawdata + srawdata_cnt;
		ret = get_rawdata_mc_sc(WATER_PROOF_OFF, srawdata_tmp);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("get scap(WP_OFF) rawdata fail\n");
			goto test_err;
		}

		FTS_TEST_SAVE_INFO("scap_rawdata in waterproof off mode:\n");
		show_data_mc_sc(srawdata_tmp);

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_RX);
		tmp2_result = compare_mc_sc(tx_check, rx_check, srawdata_tmp,
									thr->scap_rawdata_off_min,
									thr->scap_rawdata_off_max);

		srawdata_cnt += tdata->sc_node.node_num;
	} else {
		tmp2_result = true;
	}

test_err:
	if (tmp_result && tmp2_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n------ scap rawdata test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("\n------ scap rawdata test NG\n");
	}

	/* save data */
	fts_test_save_data("SCAP Rawdata Test", CODE_M_SCAP_RAWDATA_TEST,
					   scap_rawdata, srawdata_cnt, true, false, *test_result);

	return ret;
}
static int ft5452_panel_differ_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	int i = 0;
	u8 fre = 0;
	u8 fir = 0;
	u8 normalize = 0;
	int *panel_differ = NULL;
	struct mc_sc_threshold *thr = &tdata->ic.mc_sc.thr;

	FTS_TEST_SAVE_INFO("\n============ Test Item: Panel Differ Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	panel_differ = tdata->buffer;

	if (!thr->panel_differ_min || !thr->panel_differ_max || !test_result) {
		FTS_TEST_SAVE_ERR("panel_differ_h_min/max test_result is null\n");
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
	ret = fts_test_read_reg(FACTORY_REG_NORMALIZE, &normalize);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read normalize fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_FRE_LIST, &fre);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0x0A fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_FIR, &fir);
	if (ret) {
		FTS_TEST_SAVE_ERR("read 0xFB fail,ret=%d\n", ret);
		goto test_err;
	}

	/* set to overall normalize */
	if (normalize != NORMALIZE_OVERALL) {
		ret = fts_test_write_reg(FACTORY_REG_NORMALIZE, NORMALIZE_OVERALL);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("write normalize fail,ret=%d\n", ret);
			goto restore_reg;
		}
	}

	/* set frequecy high */
	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, 0x81);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set frequecy fail,ret=%d\n", ret);
		goto restore_reg;
	}

	/* fir disable */
	ret = fts_test_write_reg(FACTORY_REG_FIR, 0);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set fir fail,ret=%d\n", ret);
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

	for (i = 0; i < tdata->node.node_num; i++) {
		panel_differ[i] = panel_differ[i] / 10;
	}

	/* show test data */
	show_data(panel_differ, false);

	/* compare */
	tmp_result = compare_array(panel_differ,
							   thr->panel_differ_min,
							   thr->panel_differ_max,
							   false);

restore_reg:
	/* set the origin value */
	ret = fts_test_write_reg(FACTORY_REG_NORMALIZE, normalize);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore normalize fail,ret=%d\n", ret);
	}

	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, fre);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore 0x0A fail,ret=%d\n", ret);
	}

	ret = fts_test_write_reg(FACTORY_REG_FIR, fir);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore 0xFB fail,ret=%d\n", ret);
	}
test_err:
	/* result */
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ panel differ test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_ERR("------ panel differ test NG\n");
	}

	/* save test data */
	fts_test_save_data("Panel Differ Test", CODE_M_PANELDIFFER_TEST,
					   panel_differ, 0, false, false, *test_result);

	return ret;
}

static int ft5452_short_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	int offset = 0;
	int total_num;
	int all_adc_data_num = 63;
	int *adc_data  = NULL;
	int *fm_short_resistance = NULL;
	int offset_value[4] = {0};
	u8 stall_value = 1;
	bool tmp_result = true;
	bool is_weak_short_gnd = false;
	bool is_weak_short_mut = false;

	total_num = tdata->sc_node.node_num;
	all_adc_data_num = 1 + total_num;

	FTS_TEST_SAVE_INFO("\n============ Test Item: Short Test\n");
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

	adc_data = fts_malloc((all_adc_data_num + 1) * sizeof(int));
	if (NULL == adc_data) {
		FTS_TEST_SAVE_ERR("adc_data buffer malloc fail\n");
		goto test_err;
	}
	memset(adc_data, 0, (all_adc_data_num + 1));

	fm_short_resistance = fts_malloc(total_num * sizeof(int));
	if (NULL == fm_short_resistance) {
		FTS_TEST_SAVE_ERR("fm_short_resistance buffer malloc fail\n");
		goto test_err;
	}
	memset(fm_short_resistance, 0, total_num);

	ret = fts_test_read_reg(FACTROY_REG_SHORT_DELAY, &stall_value);
	ret = fts_test_write_reg(FACTROY_REG_SHORT_DELAY, 0x01);
	if (ret) {
		tmp_result = false;
		goto test_err;
	}

	ret = short_get_adc_data_mc(TEST_RETVAL_00, 2, offset_value, FACTROY_REG_SHORT_OFFSET);
	if (ret) {
		tmp_result = false;
		FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d \n", ret);
		goto test_err;
	}

	offset = offset_value[0] - 1024;

	/* get short resistance and exceptional channel */
	tmp_result = short_test_get_channel_num(tdata, fm_short_resistance, adc_data, offset, &is_weak_short_mut);

	/* use the exceptional channel to conduct channel to ground short circuit test. */
	if (is_weak_short_mut) {
		tmp_result = short_test_channel_to_gnd(tdata, fm_short_resistance, adc_data, offset, &is_weak_short_gnd);
	}

	/* use the exceptional channel to conduct channel to channel short circuit test. */
	if (is_weak_short_mut) {
		tmp_result &= short_test_channel_to_channel(tdata, fm_short_resistance, adc_data, offset);
	}

test_err:

	fts_free(adc_data);
	fts_free(fm_short_resistance);

	ret = fts_test_write_reg(FACTROY_REG_SHORT_DELAY, stall_value);

	if (is_weak_short_gnd && is_weak_short_mut) {
		FTS_TEST_ERROR("gnd and mutual weak short! \n");
	} else if (is_weak_short_gnd) {
		FTS_TEST_ERROR("gnd weak short! \n");
	} else if (is_weak_short_mut) {
		FTS_TEST_ERROR("mutual weak short! \n");
	} else {
		FTS_TEST_ERROR("no short! \n");
	}

	if (tmp_result) {
		FTS_TEST_SAVE_INFO("------ short test PASS\n");
		* test_result = true;
	} else {
		FTS_TEST_SAVE_ERR("------ short test NG\n");
		* test_result = false;
	}
	return ret;
}

static int start_test_ft5452(void)
{
	int ret = 0;
	struct fts_test *tdata = fts_ftest;
	struct mc_sc_testitem *test_item = &tdata->ic.mc_sc.u.item;
	bool temp_result = false;
	bool test_result = true;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_INFO("test item:0x%x", fts_ftest->ic.mc_sc.u.tmp);

	/* rawdata test */
	if (true == test_item->rawdata_test) {
		ret = ft5452_rawdata_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
		}
	}

	if (true == test_item->rawdata_uniformity_test) {
		ret = ft5452_uniformity_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
		}
	}

	/* scap_cb test */
	if (true == test_item->scap_cb_test) {
		ret = ft5452_scap_cb_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
		}
	}

	/* scap_rawdata test */
	if (true == test_item->scap_rawdata_test) {
		ret = ft5452_scap_rawdata_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
		}
	}

	/* short test */
	if (true == test_item->short_test) {
		ret = ft5452_short_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
		}
	}
	/* panel differ test */
	if (true == test_item->panel_differ_test) {
		ret = ft5452_panel_differ_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
		}
	}

	/* restore mapping state */
	fts_test_write_reg(FACTORY_REG_NOMAPPING, tdata->mapping);
	return test_result;
}

struct test_funcs test_func_ft5452 = {
	.ctype = {0x81},
	.hwtype = IC_HW_MC_SC,
	.key_num_total = 0,
	.start_test = start_test_ft5452,
};


