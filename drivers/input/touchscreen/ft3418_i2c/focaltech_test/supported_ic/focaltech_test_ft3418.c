/************************************************************************
* Copyright (c) 2012-2020, Focaltech Systems (R), All Rights Reserved.
* Copyright (C) 2021 XiaoMi, Inc.
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
static int short_test_channel_to_all(
    struct fts_test *tdata,
    int *ca_short_resistance,
    int *adc_data,
    int offset,
    bool *result)
{
    int ret = 0;
    int i = 0;
    int err_ch_num = 0;
    int tx_num = 0;
    int rx_num = 0;
    int ch_num = 0;
    int code_1 = 1437;
    int code = 0;
    struct mc_sc_threshold *thr = &fts_ftest->ic.mc_sc.thr;
    int min_cc = thr->basic.short_cc;
    int numerator = 0;
    int denominator = 0;

    FTS_TEST_FUNC_ENTER();
    *result = false;
    tx_num = tdata->sc_node.tx_num;
    rx_num = tdata->sc_node.rx_num;
    ch_num = tx_num + rx_num;

    /*get adc data for channel to all*/
    ret = fts_test_write_reg(FACTROY_REG_SHORT_RES_LEVEL, 0x01);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("write 5A fails,ret:%d\n", ret);
        goto TEST_END;
    }

    ret = short_get_adc_data_mc(TEST_RETVAL_00, (ch_num + 1) * 2, \
                                adc_data, FACTROY_REG_SHORT_CA);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("get adc data of ca fails,ret:%d\n", ret);
        goto TEST_END;
    }

    for (i = 0; i < ch_num; i++) {
        code = adc_data[i];
        denominator = code_1 - code;
        if (denominator <= 0) {
            ca_short_resistance[i] = min_cc;
        } else {
            numerator = (code - offset + 395) * 111;
            ca_short_resistance[i] = fts_abs(numerator / denominator - 3);
        }

        if (ca_short_resistance[i] < min_cc) {
            err_ch_num++;
        }
    }

    if (err_ch_num > 0) {
        /* print all Adc value */
        FTS_TEST_SAVE_INFO("Offset:%d, Code1:%d\n", offset, code_1);
        print_buffer(adc_data, ch_num, ch_num);
        *result = false;
    } else {
        *result = true;
    }

TEST_END:

    FTS_TEST_FUNC_EXIT();
    return ret;
}

static int short_test_channel_to_gnd(
    struct fts_test *tdata,
    int *ca_short_resistance,
    int *adc_data,
    int offset)
{
    int ret = 0;
    int error_num = 0;
    int min_70k_num = 0;
    int ch_num = 0;
    int tx_num = 0;
    int rx_num = 0;
    int min_cg = 0;
    int min_cc = 0;
    int i = 0;
    int code_1 = 1437;
    int code = 0;
    int numerator = 0;
    int denominator = 0;
    int tmp_res = 0;
    bool is_short_gnd = false;
    int *cg_short_resistance = NULL;
    int *tmp_adc_data = NULL;
    u8 *w_buf = NULL;
    u8 *error_ch = NULL;
    u8 *min_70k_ch = NULL;
    struct mc_sc_threshold *thr = &fts_ftest->ic.mc_sc.thr;

    FTS_TEST_FUNC_ENTER();
    tx_num = tdata->sc_node.tx_num;
    rx_num = tdata->sc_node.rx_num;
    ch_num = tx_num + rx_num;
    min_cc = thr->basic.short_cc;
    min_cg = thr->basic.short_cg;

    /*store channels of abnormal*/
    error_ch = fts_malloc(ch_num * sizeof(u8));
    if (NULL == error_ch) {
        FTS_TEST_SAVE_ERR("error_ch buffer malloc fail\n");
        ret = -ENOMEM;
        goto TEST_END;
    }

    /*store channels of resistor which are less than 70k*/
    min_70k_ch = fts_malloc(ch_num * sizeof(u8));
    if (NULL == min_70k_ch) {
        FTS_TEST_SAVE_ERR("min_70k_ch buffer malloc fail\n");
        ret = -ENOMEM;
        goto TEST_END;
    }

    w_buf = fts_malloc((ch_num + 1) * sizeof(u8));
    if (NULL == w_buf) {
        FTS_TEST_SAVE_ERR("w_buf buffer malloc fail\n");
        ret = -ENOMEM;
        goto TEST_END;
    }

    tmp_adc_data = fts_malloc((ch_num + 1) * sizeof(int));
    if (NULL == tmp_adc_data) {
        FTS_TEST_SAVE_ERR("adc_data buffer malloc fail\n");
        ret = -ENOMEM;
        goto TEST_END;
    }

    /*store resistor after channel to gnd test*/
    cg_short_resistance = fts_malloc(ch_num * sizeof(int));
    if (NULL == cg_short_resistance) {
        FTS_TEST_SAVE_ERR("cg_short_resistance buffer malloc fail\n");
        ret = -ENOMEM;
        goto TEST_END;
    }

    /*get abnormal channels from the 1st test step*/
    for (i = 0; i < ch_num; i++) {
        cg_short_resistance[i] = ca_short_resistance[i];
        if (ca_short_resistance[i] < min_cc) {
            error_ch[error_num++] = (u8)(i + 1);
        }
    }

    if (error_num > 0) {
        /*write error channel numbers*/
        w_buf[0] = (u8)error_num;
        for (i = 0; i < error_num; i++) {
            w_buf[1 + i] = error_ch[i];
            FTS_TEST_DBG("cg:%d,", error_ch[i]);
        }

        ret = fts_test_write(FACTROY_REG_SHORT_AB_CH, w_buf, error_num + 1);
        if (ret < 0) {
            FTS_TEST_SAVE_ERR("failed to set error channels,ret:%d\n", ret);
            goto TEST_END;
        }

        /*get adc data of test of channel to gnd*/
        ret = fts_test_write_reg(FACTROY_REG_SHORT_RES_LEVEL, 0x01);
        if (ret < 0) {
            FTS_TEST_SAVE_ERR("write 5A fails,ret:%d\n", ret);
            goto TEST_END;
        }

        ret = short_get_adc_data_mc(TEST_RETVAL_00, error_num * 2, \
                                    tmp_adc_data, FACTROY_REG_SHORT_CG);
        if (ret < 0) {
            FTS_TEST_SAVE_ERR("get adc data of cg fails,ret:%d\n", ret);
            goto TEST_END;
        }

        for (i = 0; i < error_num; i++) {
            code = tmp_adc_data[i];
            denominator = code_1 - code;
            if (denominator <= 0) {
                tmp_res = min_cg;
            } else {
                numerator = (code - offset + 395) * 111;
                tmp_res = fts_abs(numerator / denominator - 3);
            }

            /*resistor for channel to gnd fails*/
            if (tmp_res < min_cg) {
                cg_short_resistance[error_ch[i] - 1] = tmp_res;
                adc_data[error_ch[i] - 1] = code;

                if (!is_short_gnd) {
                    is_short_gnd = true;
                    FTS_TEST_SAVE_INFO("\nGND Short:\n");
                }
                if (tmp_res > 70) {
                    if (error_ch[i] <= tx_num) {
                        FTS_TEST_SAVE_INFO("Tx%d with GND:", error_ch[i]);
                    } else {
                        FTS_TEST_SAVE_INFO( "Rx%d with GND:", (error_ch[i] - tx_num));
                    }

                    FTS_TEST_SAVE_INFO("%d(K), ADC:%d\n", tmp_res, code);
                } else {
                    min_70k_ch[min_70k_num++] = error_ch[i];
                }
            }
        }
    }

    if (min_70k_num > 0) {
        /*resistor of some channels are less than 70k*/

        memset(tmp_adc_data, 0, (ch_num + 1) * sizeof(int));
        w_buf[0] = (u8)min_70k_num;
        for (i = 0; i < min_70k_num; i++) {
            w_buf[1 + i] = min_70k_ch[i];
            FTS_TEST_DBG("cg:%d,", min_70k_ch[i]);
        }

        ret = fts_test_write(FACTROY_REG_SHORT_AB_CH, w_buf, min_70k_num + 1);
        if (ret < 0) {
            FTS_TEST_SAVE_ERR("failed to set error channels,ret:%d\n", ret);
            goto TEST_END;
        }

        ret = fts_test_write_reg(FACTROY_REG_SHORT_RES_LEVEL, 0x00);
        if (ret < 0) {
            FTS_TEST_SAVE_ERR("write 5A fails,ret:%d\n", ret);
            goto TEST_END;
        }

        ret = short_get_adc_data_mc(TEST_RETVAL_00, min_70k_num * 2, \
                                    tmp_adc_data, FACTROY_REG_SHORT_CG);
        if (ret < 0) {
            FTS_TEST_SAVE_ERR("get adc data of cg(70K) fails,ret:%d\n", ret);
            goto TEST_END;
        }

        for (i = 0; i < min_70k_num; i++) {
            code = tmp_adc_data[i];
            denominator = code_1 - code;
            if (denominator <= 0) {
                tmp_res = min_cg;
            } else {
                numerator = (code - offset + 395) * 4;
                tmp_res = fts_abs(numerator / denominator - 3);
            }

            if (tmp_res < min_cg) {
                cg_short_resistance[min_70k_ch[i] - 1] = tmp_res;
                adc_data[min_70k_ch[i] - 1] = tmp_adc_data[i];
                if (min_70k_ch[i] <= tx_num) {
                    FTS_TEST_SAVE_INFO("Tx%d with GND:", min_70k_ch[i]);
                } else {
                    FTS_TEST_SAVE_INFO("Rx%d with GND:", (min_70k_ch[i] - tx_num));
                }

                FTS_TEST_SAVE_INFO("%d(K), ADC:%d\n", tmp_res, code);
            }
        }
    }

    if (is_short_gnd) {
        FTS_TEST_DBG("channel to gnd test fail");
    }
    ret = 0;
TEST_END:

    fts_free(error_ch);
    fts_free (min_70k_ch);
    fts_free (tmp_adc_data);
    fts_free(cg_short_resistance);
    fts_free(w_buf);

    FTS_TEST_FUNC_EXIT();
    return ret;
}

static int short_test_channel_to_channel(
    struct fts_test *tdata,
    int *ca_short_resistance,
    int *adc_data,
    int offset)
{
    int ret = 0;
    int error_num = 0;
    int tmp_num = 0;
    int min_70k_num = 0;
    int ch_num = 0;
    int rx_num = 0;
    int tx_num = 0;
    int min_cc = 0;
    int i = 0;
    int j = 0;
    int k = 0;
    int code_1 = 1437;
    int code = 0;
    int numerator = 0;
    int denominator = 0;
    int tmp_res = 0;
    int adc_count = 0;
    bool is_short_mutual = false;
    int *cc_short_resistance = NULL;
    int *tmp_adc_data = NULL;
    u8 *w_buf = NULL;
    u8 *error_ch = NULL;
    u8 *min_70k_ch = NULL ;
    struct mc_sc_threshold *thr = &fts_ftest->ic.mc_sc.thr;

    FTS_TEST_FUNC_ENTER();
    tx_num = tdata->sc_node.tx_num;
    rx_num = tdata->sc_node.rx_num;
    ch_num = tx_num + rx_num;
    min_cc = thr->basic.short_cc;

    w_buf = fts_malloc((ch_num + 1) * sizeof(u8));
    if (NULL == w_buf) {
        FTS_TEST_SAVE_ERR("w_buf buffer malloc fail\n");
        ret = -ENOMEM;
        goto TEST_END;
    }

    error_ch = fts_malloc(ch_num * sizeof(u8));
    if (NULL == error_ch) {
        FTS_TEST_SAVE_ERR("error_ch buffer malloc fail\n");
        ret = -ENOMEM;
        goto TEST_END;
    }
    min_70k_ch = fts_malloc(ch_num * sizeof(u8));
    if (NULL == min_70k_ch) {
        FTS_TEST_SAVE_ERR("min_70k_ch buffer malloc fail\n");
        ret = -ENOMEM;
        goto TEST_END;
    }

    tmp_adc_data = fts_malloc((ch_num + 1) * sizeof(int));
    if (NULL == tmp_adc_data) {
        FTS_TEST_SAVE_ERR("adc_data buffer malloc fail\n");
        ret = -ENOMEM;
        goto TEST_END;
    }

    /*store resistor after channel to gnd test*/
    cc_short_resistance = fts_malloc(ch_num * sizeof(int));
    if (NULL == cc_short_resistance) {
        FTS_TEST_SAVE_ERR("cc_short_resistance buffer malloc fail\n");
        ret = -ENOMEM;
        goto TEST_END;
    }

    /*get abnormal channels from the 1st test step*/
    for (i = 0; i < ch_num; i++) {
        cc_short_resistance[i] = ca_short_resistance[i];
        if (ca_short_resistance[i] < min_cc) {
            error_ch[error_num++] = (u8)(i + 1);
        }
    }

    /*channel to channel, must have 2 or multiple of 2*/
    if (error_num >= 2) {
        w_buf[0] = (u8)error_num;
        for ( i = 0; i < error_num; i++) {
            w_buf[1 + i] = error_ch[i];
            FTS_TEST_DBG("cc:%d,", error_ch[i]);
        }

        ret = fts_test_write(FACTROY_REG_SHORT_AB_CH, w_buf, error_num + 1);
        if (ret < 0) {
            FTS_TEST_SAVE_ERR("failed to set error channels,ret:%d\n", ret);
            goto TEST_END;
        }

        /*get adc data of test of channel to channel*/
        ret = fts_test_write_reg(FACTROY_REG_SHORT_RES_LEVEL, 0x01);
        if (ret < 0) {
            FTS_TEST_SAVE_ERR("write 5A fails,ret:%d\n", ret);
            goto TEST_END;
        }

        /*channel to channel: num * (num - 1) / 2, max. node_num*/
        tmp_num = error_num * (error_num - 1) / 2;
        tmp_num = (tmp_num > ch_num) ? ch_num : tmp_num;
        ret = short_get_adc_data_mc(TEST_RETVAL_00, tmp_num * 2, \
                                    tmp_adc_data, FACTROY_REG_SHORT_CC);
        if (ret < 0) {
            FTS_TEST_SAVE_ERR("get adc data of cc fails,ret:%d\n", ret);
            goto TEST_END;
        }

        for (i = 0; i < error_num; i++) {
            for (j = i + 1; j < error_num; j++) {
                if (adc_count >= tmp_num)
                    break;
                code = tmp_adc_data[adc_count];
                denominator = code_1 - code;
                if (denominator <= 0) {
                    tmp_res = min_cc;
                } else {
                    numerator = (code - offset + 395) * 111;
                    tmp_res = fts_abs(numerator / denominator - 3);
                }

                if (tmp_res < min_cc) {
                    cc_short_resistance[error_ch[i] - 1] = tmp_res;
                    cc_short_resistance[error_ch[j] - 1] = tmp_res;
                    adc_data[error_ch[i] - 1] = code;
                    adc_data[error_ch[j] - 1] = code;

                    if (!is_short_mutual) {
                        FTS_TEST_SAVE_INFO("\nMutual Short:\n");
                        is_short_mutual = true;
                    }
                    if (tmp_res > 70) {
                        if (error_ch[i] <= tx_num) {
                            FTS_TEST_SAVE_INFO("Tx%d with", (error_ch[i]));
                        } else {
                            FTS_TEST_SAVE_INFO("Rx%d with", (error_ch[i] - tx_num));
                        }

                        if (error_ch[j] <= tx_num) {
                            FTS_TEST_SAVE_INFO(" Tx%d", (error_ch[j] ) );
                        } else {
                            FTS_TEST_SAVE_INFO(" Rx%d", (error_ch[j] - tx_num));
                        }
                        FTS_TEST_SAVE_INFO(":%d(K), ADC:%d\n", tmp_res, code);
                    } else {
                        int has_ch_i = 0;
                        int has_ch_j = 0;

                        for (k = 0; k < min_70k_num; k++) {
                            if (min_70k_ch[k] == error_ch[i])
                                has_ch_i = 1;
                            else if (min_70k_ch[k] == error_ch[j])
                                has_ch_j = 1;
                        }

                        if (!has_ch_i)
                            min_70k_ch[min_70k_num++] = error_ch[i];
                        if (!has_ch_j)
                            min_70k_ch[min_70k_num++] = error_ch[j];
                    }
                }

                adc_count++;
            }
        }
    }

    if (min_70k_num > 0) {
        /*resistor of some channels are less than 70k*/
        memset(tmp_adc_data, 0, (ch_num + 1) * sizeof(int));
        w_buf[0] = (u8)min_70k_num;
        for (i = 0; i < min_70k_num; i++) {
            w_buf[1 + i] = min_70k_ch[i];
            FTS_TEST_DBG("cc(70K):%d,", min_70k_ch[i]);
        }

        ret = fts_test_write(FACTROY_REG_SHORT_AB_CH, w_buf, min_70k_num + 1);
        if (ret < 0) {
            FTS_TEST_SAVE_ERR("failed to set error channels,ret:%d\n", ret);
            goto TEST_END;
        }

        ret = fts_test_write_reg(FACTROY_REG_SHORT_RES_LEVEL, 0x00);
        if (ret < 0) {
            FTS_TEST_SAVE_ERR("write 5A fails,ret:%d\n", ret);
            goto TEST_END;
        }

        tmp_num = min_70k_num * (min_70k_num - 1) / 2;
        tmp_num = (tmp_num > ch_num) ? ch_num : tmp_num;
        ret = short_get_adc_data_mc(TEST_RETVAL_00, tmp_num * 2, \
                                    tmp_adc_data, FACTROY_REG_SHORT_CC);
        if (ret < 0) {
            FTS_TEST_SAVE_ERR("get adc data of cc(70K) fails,ret:%d\n", ret);
            goto TEST_END;
        }

        adc_count = 0;
        for (i = 0; i < min_70k_num; i++) {
            for (j = i + 1; j < min_70k_num; j++) {
                if (adc_count >= tmp_num)
                    break;
                code = tmp_adc_data[adc_count];
                denominator = code_1 - code;
                if (denominator <= 0) {
                    tmp_res = min_cc;
                } else {
                    numerator = (code - offset + 395) * 4;
                    tmp_res = fts_abs(numerator / denominator - 3);
                }

                if (tmp_res < min_cc) {
                    cc_short_resistance[min_70k_ch[i] - 1] = tmp_res;
                    cc_short_resistance[min_70k_ch[j] - 1] = tmp_res;
                    adc_data[min_70k_ch[i] - 1] = code;
                    adc_data[min_70k_ch[j] - 1] = code;
                    if (min_70k_ch[i] <= tx_num) {
                        FTS_TEST_SAVE_INFO("Tx%d with", (min_70k_ch[i]));
                    } else {
                        FTS_TEST_SAVE_INFO("Rx%d with", (min_70k_ch[i] - tx_num));
                    }

                    if (min_70k_ch[j] <= tx_num) {
                        FTS_TEST_SAVE_INFO(" Tx%d", (min_70k_ch[j]));
                    } else {
                        FTS_TEST_SAVE_INFO(" Rx%d", (min_70k_ch[j] - tx_num));
                    }

                    FTS_TEST_SAVE_INFO(":%d(K), ADC:%d\n", tmp_res, code);
                }

                adc_count++;
            }
        }
    }

    if (is_short_mutual) {
        FTS_TEST_DBG("channel to channel test fail");
    }
    ret = 0;
TEST_END:

    fts_free(error_ch);
    fts_free(min_70k_ch);
    fts_free (tmp_adc_data);
    fts_free(cc_short_resistance);
    fts_free(w_buf);

    FTS_TEST_FUNC_EXIT();
    return ret;
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

    FTS_TEST_FUNC_ENTER();
    FTS_TEST_SAVE_INFO("\n============ Test Item: rawdata test\n");
    memset(tdata->buffer, 0, tdata->buffer_length);
    rawdata = tdata->buffer;

    if (!thr->rawdata_h_min || !thr->rawdata_h_max) {
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

    FTS_TEST_FUNC_EXIT();
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
    u8 normalize = 0;
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
    print_buffer(rawdata, tdata->node.node_num, tdata->node.rx_num);

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
        for (i = 0; i < tdata->node.node_num; i++) {
            if (0 == tdata->node_valid[i])
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
    if (result && result2 && result3) {
        *test_result = true;
        FTS_TEST_SAVE_INFO("uniformity test is OK\n");
    } else {
        *test_result = false;
        FTS_TEST_SAVE_ERR("uniformity test is NG\n");
    }

    fts_test_save_data("Rawdata Uniformity Test",
                       CODE_M_RAWDATA_UNIFORMITY_TEST, rawdata_linearity,
                       tdata->node.node_num * 2, false, false, *test_result);

    fts_free(rawdata_linearity);
    FTS_TEST_FUNC_EXIT();
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

    FTS_TEST_FUNC_ENTER();
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
        || !thr->scap_cb_off_min || !thr->scap_cb_off_max) {
        FTS_TEST_SAVE_ERR("scap_cb_on/off_min/max is null\n");
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
    if (thr->basic.scap_cb_wp_off_check && fw_wp_check) {
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

    FTS_TEST_FUNC_EXIT();
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

    FTS_TEST_FUNC_ENTER();
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
        || !thr->scap_rawdata_off_min || !thr->scap_rawdata_off_max) {
        FTS_TEST_SAVE_ERR("scap_rawdata_on/off_min/max is null\n");
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
    if (thr->basic.scap_rawdata_wp_off_check && fw_wp_check) {
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

    FTS_TEST_FUNC_EXIT();
    return ret;
}

static int ft5452_short_test(struct fts_test *tdata, bool *test_result)
{
    int ret = 0;
    int ch_num = 0;
    int *adc_data = NULL;
    int *ca_short_resistance = NULL;
    int offset = 0;
    u8 short_delay = 0;
    bool weak_short_result = false;

    FTS_TEST_FUNC_ENTER();
    FTS_TEST_SAVE_INFO("\n============ Test Item: Short Test\n");
    ch_num = tdata->sc_node.tx_num + tdata->sc_node.rx_num;

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

    adc_data = fts_malloc((ch_num + 3) * sizeof(int));
    if (NULL == adc_data) {
        FTS_TEST_SAVE_ERR("adc_data buffer malloc fail\n");
        ret = -ENOMEM;
        goto test_err;
    }

    ca_short_resistance = fts_malloc(ch_num * sizeof(int));
    if (NULL == ca_short_resistance) {
        FTS_TEST_SAVE_ERR("short_resistance buffer malloc fail\n");
        ret = -ENOMEM;
        goto test_err;
    }

    ret = fts_test_read_reg(FACTROY_REG_SHORT_RES_LEVEL, &short_delay);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("read 5A fails\n");
        goto test_err;
    }

    ret = short_get_adc_data_mc(TEST_RETVAL_00, 1 * 2, &offset, \
                                FACTROY_REG_SHORT_OFFSET);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("failed to get weak short data,ret=%d\n", ret);
        goto restore_reg;
    }

    offset -= 1024;

    /* get short resistance and exceptional channel */
    ret = short_test_channel_to_all(tdata, ca_short_resistance, adc_data, offset, \
                                    &weak_short_result);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("short test of channel to all fails\n");
        goto restore_reg;
    }

    if (!weak_short_result) {
        /*weak short fail, get short values*/
        ret = short_test_channel_to_gnd(tdata, ca_short_resistance, \
                                        adc_data, offset);
        if (ret < 0) {
            FTS_TEST_SAVE_ERR("short test of channel to gnd fails\n");
            goto restore_reg;
        }

        ret = short_test_channel_to_channel(tdata, ca_short_resistance, \
                                            adc_data, offset);
        if (ret < 0) {
            FTS_TEST_SAVE_ERR("short test of channel to channel fails\n");
            goto restore_reg;
        }

    }

restore_reg:
    ret = fts_test_write_reg(FACTROY_REG_SHORT_RES_LEVEL, short_delay);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("restore 5A fails");
    }

test_err:
    fts_free(adc_data);
    fts_free(ca_short_resistance);

    if (weak_short_result) {
        FTS_TEST_SAVE_INFO("------ short test PASS\n");
        *test_result = true;
    } else {
        FTS_TEST_SAVE_ERR("------ short test NG\n");
        *test_result = false;
    }

    FTS_TEST_FUNC_EXIT();
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

    FTS_TEST_FUNC_ENTER();
    FTS_TEST_SAVE_INFO("\n============ Test Item: Panel Differ Test\n");
    memset(tdata->buffer, 0, tdata->buffer_length);
    panel_differ = tdata->buffer;

    if (!thr->panel_differ_min || !thr->panel_differ_max) {
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

    FTS_TEST_FUNC_EXIT();
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

    FTS_TEST_FUNC_EXIT();
    return test_result;
}

struct test_funcs test_func_ft5452 = {
    .ctype = {0x81},
    .hwtype = IC_HW_MC_SC,
    .key_num_total = 0,
    .start_test = start_test_ft5452,
};
