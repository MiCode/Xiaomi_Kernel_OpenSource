/************************************************************************
* Copyright (c) 2012-2020, Focaltech Systems (R), All Rights Reserved.
*
* File Name: Focaltech_test_ft8006sp.c
*
* Author: Focaltech Driver Team
*
* Created: 2019-07-02
*
* Abstract:
*
************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "../focaltech_test.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/

/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/

/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/

/*******************************************************************************
* Static variables
*******************************************************************************/

/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/

/*******************************************************************************
* Static function prototypes
*******************************************************************************/
static int ft8057_short_test(struct fts_test *tdata, bool *test_result)
{
    int ret = 0;
    int i = 0;
    bool tmp_result = false;
    int byte_num = 0;
    int ch_num = 0;
    int min = 0;
    int max = 0;
    int tmp_adc = 0;
    int *adcdata = NULL;
    struct incell_threshold *thr = &tdata->ic.incell.thr;

    FTS_TEST_FUNC_ENTER();
    FTS_TEST_SAVE_INFO("\n============ Test Item: Short Circuit Test\n");
    memset(tdata->buffer, 0, tdata->buffer_length);
    adcdata = tdata->buffer;

    ret = enter_factory_mode();
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
        goto test_err;
    }

    byte_num = tdata->node.node_num * 2;
    ch_num = tdata->node.rx_num;
    ret = short_get_adcdata_incell(TEST_RETVAL_AA, ch_num, byte_num, adcdata);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("get adc data fail\n");
        goto test_err;
    }

    /* calculate resistor */
    for (i = 0; i < tdata->node.node_num; i++) {
        tmp_adc = adcdata[i];
        if (tmp_adc <= 0) {
            adcdata[i] = -1;
            continue;
        }

        adcdata[i] = (124 * tmp_adc + 59059) / (8573 - 2 * tmp_adc);
    }

    /* save */
    show_data(adcdata, false);

    /* compare */
    min = thr->basic.short_res_min;
    max = TEST_SHORT_RES_MAX;
    tmp_result = compare_data(adcdata, min, max, min, max, false);

test_err:
    ret = fts_test_write_reg(FACTORY_REG_SHORT_TEST_STATE, 0x03);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("write idle to short test state fail\n");
    }

    if (tmp_result) {
        *test_result = true;
        FTS_TEST_SAVE_INFO("------ Short Circuit Test PASS\n");
    } else {
        *test_result = false;
        FTS_TEST_SAVE_INFO("------ Short Circuit Test NG\n");
    }

    /* save test data */
    fts_test_save_data("Short Circuit Test", CODE_SHORT_TEST,
                       adcdata, 0, false, false, *test_result);

    FTS_TEST_FUNC_EXIT();
    return ret;
}

static int ft8057_open_test(struct fts_test *tdata, bool *test_result)
{
    int ret = 0;
    int i = 0;
    bool tmp_result = false;
    u8 state = 0;
    int byte_num = 0;
    int min = 0;
    int max = 0;
    int *opendata = NULL;
    struct incell_threshold *thr = &tdata->ic.incell.thr;

    FTS_TEST_FUNC_ENTER();
    FTS_TEST_SAVE_INFO("\n============ Test Item: Open Test\n");
    memset(tdata->buffer, 0, tdata->buffer_length);
    opendata = tdata->buffer;

    ret = enter_factory_mode();
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
        goto test_err;
    }

    ret = fts_test_write_reg(FACTORY_REG_OPEN_START, 0x01);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("start open test fail\n");
        goto test_err;
    }

    /* check test status */
    for (i = 0; i < FACTORY_TEST_RETRY; i++) {
        sys_delay(FACTORY_TEST_RETRY_DELAY);
        ret = fts_test_read_reg(FACTORY_REG_OPEN_STATE, &state);
        if ((ret >= 0) && (TEST_RETVAL_AA == state)) {
            break;
        } else {
            FTS_TEST_DBG("reg%x=%x,retry:%d\n",
                         FACTORY_REG_OPEN_STATE, state, i);
        }
    }
    if (i >= FACTORY_TEST_RETRY) {
        FTS_TEST_SAVE_ERR("open test timeout\n");
        goto test_err;
    }

    /* get open data */
    byte_num = tdata->node.node_num * 2;
    ret = read_mass_data(FACTORY_REG_OPEN_ADDR, byte_num, opendata);
    if (ret) {
        FTS_TEST_SAVE_ERR("get open data fail\n");
    }

    /* save */
    show_data(opendata, false);

    /* compare */
    min = thr->basic.open_diff_min;
    max = 1000000;
    tmp_result = compare_data(opendata, min, max, 0, 0, false);

test_err:
    ret = fts_test_write_reg(FACTORY_REG_OPEN_STATE, 0x03);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("write idle to open test state fail\n");
    }

    if (tmp_result) {
        *test_result = true;
        FTS_TEST_SAVE_INFO("------ Open Test PASS\n");
    } else {
        *test_result = false;
        FTS_TEST_SAVE_INFO("------ Open Test NG\n");
    }

    /*save test data*/
    fts_test_save_data("Open Test", CODE_OPEN_TEST,
                       opendata, 0, false, false, *test_result);
    FTS_TEST_FUNC_EXIT();
    return ret;
}


static int ft8057_cb_test(struct fts_test *tdata, bool *test_result)
{
    int ret = 0;
    bool tmp_result = false;
    int byte_num = 0;
    int *cbdata = NULL;
    struct incell_threshold *thr = &tdata->ic.incell.thr;

    FTS_TEST_FUNC_ENTER();
    FTS_TEST_SAVE_INFO("\n============ Test Item: CB Test\n");
    memset(tdata->buffer, 0, tdata->buffer_length);
    cbdata = tdata->buffer;

    if (!thr->cb_min || !thr->cb_max) {
        FTS_TEST_SAVE_ERR("cb_min/max is null\n");
        ret = -EINVAL;
        goto test_err;
    }

    ret = enter_factory_mode();
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
        goto test_err;
    }

    /* cb test enable */
    ret = fts_test_write_reg(FACTORY_REG_CB_TEST_EN, 0x01);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("cb test enable fail\n");
        goto test_err;
    }

    /* auto clb */
    ret = chip_clb();
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("auto clb fail\n");
        goto test_err;
    }

    byte_num = tdata->node.node_num;
    ret = get_cb_incell(0, byte_num, cbdata);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("get cb fail\n");
        goto test_err;
    }

    /* save */
    show_data(cbdata, false);

    /* compare */
    tmp_result = compare_array(cbdata, thr->cb_min, thr->cb_max, false);

test_err:
    /* cb test disable */
    ret = fts_test_write_reg(FACTORY_REG_CB_TEST_EN, 0x00);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("cb test disable fail\n");
    }

    if (tmp_result) {
        *test_result = true;
        FTS_TEST_SAVE_INFO("------ CB Test PASS\n");
    } else {
        *test_result = false;
        FTS_TEST_SAVE_INFO("------ CB Test NG\n");
    }

    /*save test data*/
    fts_test_save_data("CB Test", CODE_CB_TEST,
                       cbdata, 0, false, false, *test_result);
    FTS_TEST_FUNC_EXIT();
    return ret;
}

static int ft8057_rawdata_test(struct fts_test *tdata, bool *test_result)
{
    int ret = 0;
    bool tmp_result = false;
    int *rawdata = NULL;
    struct incell_threshold *thr = &tdata->ic.incell.thr;

    FTS_TEST_FUNC_ENTER();
    FTS_TEST_SAVE_INFO("\n============ Test Item: RawData Test\n");
    memset(tdata->buffer, 0, tdata->buffer_length);
    rawdata = tdata->buffer;

    if (!thr->rawdata_min || !thr->rawdata_max) {
        FTS_TEST_SAVE_ERR("rawdata_min/max is null\n");
        ret = -EINVAL;
        goto test_err;
    }

    ret = enter_factory_mode();
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
        goto test_err;
    }

    /* rawdata test enable */
    ret = fts_test_write_reg(FACTORY_REG_RAWDATA_TEST_EN, 0x01);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("rawdata test enable fail\n");
        goto test_err;
    }

    /* read rawdata */
    ret = get_rawdata(rawdata);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("get RawData fail,ret=%d\n", ret);
        goto test_err;
    }

    /* save */
    show_data(rawdata, false);

    /* compare */
    tmp_result = compare_array(rawdata,
                               thr->rawdata_min,
                               thr->rawdata_max,
                               false);
    ret = 0;

test_err:
    /* rawdata test disble */
    ret = fts_test_write_reg(FACTORY_REG_RAWDATA_TEST_EN, 0x00);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("rawdata test disable fail\n");
    }

    if (tmp_result) {
        *test_result = true;
        FTS_TEST_SAVE_INFO("------ RawData Test PASS\n");
    } else {
        *test_result = false;
        FTS_TEST_SAVE_INFO("------ RawData Test NG\n");
    }

    /*save test data*/
    fts_test_save_data("RawData Test", CODE_RAWDATA_TEST,
                       rawdata, 0, false, false, *test_result);
    FTS_TEST_FUNC_EXIT();
    return ret;
}

static int ft8057_fast_calibration(void)
{
    int ret = 0;
    u8 val = 0;
    int i = 0;
    u8 fts_fast_cal_state = 0xBD;
    u8 fts_fast_cal_start = 0xBC;

    ret = enter_work_mode();
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("enter work mode fail,ret=%d\n", ret);
        return ret;
    }

    ret = fts_test_read_reg(fts_fast_cal_state, &val);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("read regBD fail\n");
        return ret;
    }

    if (val == 0xAA) {
        FTS_TEST_INFO("already have been fast calibration");
        return 0;
    }

    ret = fts_test_write_reg(fts_fast_cal_start, 0x01);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("write regBC fail\n");
        return ret;
    }

    sys_delay(1000);

    for (i = 0; i < 100; i++) {
        ret = fts_test_read_reg(fts_fast_cal_state, &val);
        if (ret < 0) {
            FTS_TEST_SAVE_ERR("read regBD fail\n");
            return ret;
        }

        if (val == 0xAA) {
            FTS_TEST_INFO("finish fast calibration");
            return 0;
        }
        sys_delay(50);
    }

    if (i >= 100) {
        FTS_TEST_SAVE_ERR("waite for fast calibration time out!!\n");
        return -EINVAL;
    }
    return 0;

}

static int ft8057_lcdnoise_test(struct fts_test *tdata, bool *test_result)
{
    int ret = 0;
    int i = 0;
    bool tmp_result = false;
    u8 old_mode = 0;
    u8 status = 0;
    int byte_num = 0;
    int frame_num = 0;
    int max = 0;
    int *lcdnoise = NULL;
    struct incell_threshold *thr = &tdata->ic.incell.thr;

    FTS_TEST_FUNC_ENTER();
    FTS_TEST_SAVE_INFO("\n============ Test Item: LCD Noise Test\n");
    memset(tdata->buffer, 0, tdata->buffer_length);
    lcdnoise = tdata->buffer;

    ret = ft8057_fast_calibration();
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("fast calibration fail,ret=%d\n", ret);
        goto test_err;
    }

    ret = enter_factory_mode();
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
        goto test_err;
    }

    ret = fts_test_read_reg(FACTORY_REG_DATA_SELECT, &old_mode);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("read reg06 fail\n");
        goto test_err;
    }

    ret =  fts_test_write_reg(FACTORY_REG_DATA_SELECT, 0x01);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("write 1 to reg06 fail\n");
        goto test_err;
    }

    ret =  fts_test_write_reg(FACTORY_REG_LINE_ADDR, 0xAD);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("write reg01 fail\n");
        goto test_err;
    }

    frame_num = thr->basic.lcdnoise_frame;
    ret = fts_test_write_reg(FACTORY_REG_LCD_NOISE_FRAME, frame_num / 4);
    if (ret < 0) {
        FTS_TEST_SAVE_INFO("write frame num fail\n");
        goto test_err;
    }

    /* start test */
    ret = fts_test_write_reg(FACTORY_REG_LCD_NOISE_START, 0x01);
    if (ret < 0) {
        FTS_TEST_SAVE_INFO("start lcdnoise test fail\n");
        goto test_err;
    }

    /* check test status */
    sys_delay(frame_num * FACTORY_TEST_DELAY / 2);
    for (i = 0; i < FACTORY_TEST_RETRY; i++) {
        status = 0xFF;
        ret = fts_test_read_reg(FACTORY_REG_LCD_NOISE_TEST_STATE, &status);
        if ((ret >= 0) && (TEST_RETVAL_AA == status)) {
            break;
        } else {
            FTS_TEST_DBG("reg%x=%x,retry:%d\n",
                         FACTORY_REG_LCD_NOISE_TEST_STATE, status, i);
        }
        sys_delay(FACTORY_TEST_RETRY_DELAY);
    }
    if (i >= FACTORY_TEST_RETRY) {
        FTS_TEST_SAVE_ERR("lcdnoise test timeout\n");
        goto test_err;
    }
    /* read lcdnoise */
    byte_num = tdata->node.node_num * 2;
    ret = read_mass_data(FACTORY_REG_RAWDATA_ADDR, byte_num, lcdnoise);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("read rawdata fail\n");
        goto test_err;
    }

    /* save */
    show_data(lcdnoise, false);

    /* compare */
    max = thr->basic.lcdnoise_coefficient * tdata->va_touch_thr * 32 / 100;
    FTS_TEST_DBG("va_touch_thr:%d, lcdnoise_coefficient:%d, max:%d",
                 tdata->va_touch_thr, thr->basic.lcdnoise_coefficient,  max);
    tmp_result = compare_data(lcdnoise, 0, max, 0, 0, false);

test_err:
    ret = fts_test_write_reg(FACTORY_REG_LCD_NOISE_START, 0x00);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("write 0 to reg11 fail\n");
    }

    ret = fts_test_write_reg(FACTORY_REG_DATA_SELECT, old_mode);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("restore reg06 fail\n");
    }

    ret = fts_test_write_reg(FACTORY_REG_LCD_NOISE_TEST_STATE, 0x03);
    if (ret < 0) {
        FTS_TEST_SAVE_ERR("write idle to lcdnoise test state fail\n");
    }

    if (tmp_result) {
        *test_result = true;
        FTS_TEST_SAVE_INFO("------ LCD Noise Test PASS\n");
    } else {
        *test_result = false;
        FTS_TEST_SAVE_INFO("------ LCD Noise Test NG\n");
    }

    /*save test data*/
    fts_test_save_data("LCD Noise Test", CODE_LCD_NOISE_TEST,
                       lcdnoise, 0, false, false, *test_result);
    FTS_TEST_FUNC_EXIT();
    return ret;
}

static int start_test_ft8057(void)
{
    int ret = 0;
    struct fts_test *tdata = fts_ftest;
    struct incell_testitem *test_item = &tdata->ic.incell.u.item;
    bool temp_result = false;
    bool test_result = true;

    FTS_TEST_FUNC_ENTER();
    FTS_TEST_INFO("test item:0x%x", fts_ftest->ic.incell.u.tmp);

    if (!tdata || !tdata->buffer) {
        FTS_TEST_ERROR("tdata/buffer is null");
        return -EINVAL;
    }

    /* short test */
    if (true == test_item->short_test) {
        ret = ft8057_short_test(tdata, &temp_result);
        if ((ret < 0) || (false == temp_result)) {
            test_result = false;
        }
    }

    /* open test */
    if (true == test_item->open_test) {
        ret = ft8057_open_test(tdata, &temp_result);
        if ((ret < 0) || (false == temp_result)) {
            test_result = false;
        }
    }

    /* cb test */
    if (true == test_item->cb_test) {
        ret = ft8057_cb_test(tdata, &temp_result);
        if ((ret < 0) || (false == temp_result)) {
            test_result = false;
        }
    }

    /* rawdata test */
    if (true == test_item->rawdata_test) {
        ret = ft8057_rawdata_test(tdata, &temp_result);
        if ((ret < 0) || (false == temp_result)) {
            test_result = false;
        }
    }

    /* lcd noise test */
    if (true == test_item->lcdnoise_test) {
        ret = ft8057_lcdnoise_test(tdata, &temp_result);
        if ((ret < 0) || (false == temp_result)) {
            test_result = false;
        }
    }

    return test_result;
}

struct test_funcs test_func_ft8057 = {
    .ctype = {0x28, 0x2B},
    .hwtype = IC_HW_INCELL,
    .key_num_total = 0,
    .start_test = start_test_ft8057,
};
