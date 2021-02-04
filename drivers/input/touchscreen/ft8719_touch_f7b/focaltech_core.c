/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2018, FocalTech Systems, Ltd., all rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*****************************************************************************
*
* File Name: focaltech_core.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract: entrance for focaltech ts driver
*
* Version: V1.0
*
*****************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "focaltech_core.h"
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#define FTS_SUSPEND_LEVEL 1     /* Early-suspend level */
#endif
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
#include "../xiaomi/xiaomi_touch.h"
#endif

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_DRIVER_NAME                     "fts_ts"
#define INTERVAL_READ_REG                   100  /* unit:ms */
#define TIMEOUT_READ_REG                    1000 /* unit:ms */
#if FTS_POWER_SOURCE_CUST_EN
#define FTS_VTG_MIN_UV                      2600000
#define FTS_VTG_MAX_UV                      3300000
#define FTS_I2C_VTG_MIN_UV                  1800000
#define FTS_I2C_VTG_MAX_UV                  1800000
#endif

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct fts_ts_data *fts_data;

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static void fts_release_all_finger(void);
static int fts_ts_suspend(struct device *dev);
static int fts_ts_resume(struct device *dev);

/* add begin by zhangchaofan@longcheer.com for resume delay, 2019-01-08*/
static void tp_fb_notifier_resume_work(struct work_struct *work);
static void fts_interr_work(struct work_struct *work);
/* add end by zhangchaofan@longcheer.com for resume delay, 2019-01-08 */
/* modify begin by zhangchaofan@longcheer.com for factory tp work, 2018-12-06 */
#ifdef CONFIG_KERNEL_CUSTOM_FACTORY
static int lct_tp_work_node_callback(bool flag);
#endif
/* modify end by zhangchaofan@longcheer.com for factory tp work, 2018-12-06 */

/* modify begin by zhangchaofan@longcheer.com for angle inhibit, 2019-01-14 */
static int lct_tp_get_screen_angle_callback(void);
static int lct_tp_set_screen_angle_callback(unsigned int angle);
/* modify end by zhangchaofan@longcheer.com for angle inhibit, 2019-01-14 */

/* modify begin by zhangchaofan@longcheer.com for tp_gesture, 2018-12-25 */
#if FTS_GESTURE_EN
static int lct_tp_gesture_node_callback(bool flag);
static int fts_gesture_switch(struct input_dev *dev, unsigned int type, unsigned int code, int value);
extern bool enable_gesture_mode; // for gesture
#define WAKEUP_OFF 4
#define WAKEUP_ON 5
bool fts_delay_gesture = false;
bool fts_suspend_stats = false;
static struct wakeup_source gestrue_wakelock;
#endif
/* modify end by zhangchaofan@longcheer.com for tp_gesture, 2018-12-25 */

/*****************************************************************************
*  Name: fts_wait_tp_to_valid
*  Brief: Read chip id until TP FW become valid(Timeout: TIMEOUT_READ_REG),
*         need call when reset/power on/resume...
*  Input:
*  Output:
*  Return: return 0 if tp valid, otherwise return error code
*****************************************************************************/
int fts_wait_tp_to_valid(struct i2c_client *client)
{
    int ret = 0;
    int cnt = 0;
    u8 reg_value = 0;
    u8 chip_id = fts_data->ic_info.ids.chip_idh;

    do {
        ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID, &reg_value);
        if ((ret < 0) || (reg_value != chip_id)) {
            FTS_DEBUG("TP Not Ready, ReadData = 0x%x", reg_value);
        } else if (reg_value == chip_id) {
            FTS_INFO("TP Ready, Device ID = 0x%x", reg_value);
            return 0;
        }
        cnt++;
        msleep(INTERVAL_READ_REG);
    } while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

    return -EIO;
}

/************************************************************************
* Name: fts_get_chip_types
* Brief: verity chip id and get chip type data
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
static int fts_get_chip_types(
    struct fts_ts_data *ts_data,
    u8 id_h, u8 id_l, bool fw_valid)
{
    int i = 0;
    struct ft_chip_t ctype[] = FTS_CHIP_TYPE_MAPPING;
    u32 ctype_entries = sizeof(ctype) / sizeof(struct ft_chip_t);

    if ((0x0 == id_h) || (0x0 == id_l)) {
        FTS_ERROR("id_h/id_l is 0");
        return -EINVAL;
    }

    FTS_DEBUG("verify id:0x%02x%02x", id_h, id_l);
    for (i = 0; i < ctype_entries; i++) {
        if (VALID == fw_valid) {
            if ((id_h == ctype[i].chip_idh) && (id_l == ctype[i].chip_idl))
                break;
        } else {
            if (((id_h == ctype[i].rom_idh) && (id_l == ctype[i].rom_idl))
                || ((id_h == ctype[i].pb_idh) && (id_l == ctype[i].pb_idl))
                || ((id_h == ctype[i].bl_idh) && (id_l == ctype[i].bl_idl)))
                break;
        }
    }

    if (i >= ctype_entries) {
        return -ENODATA;
    }

    ts_data->ic_info.ids = ctype[i];
    return 0;
}

/*****************************************************************************
*  Name: fts_get_ic_information
*  Brief:
*  Input:
*  Output:
*  Return: return 0 if success, otherwise return error code
*****************************************************************************/
static int fts_get_ic_information(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int cnt = 0;
    u8 chip_id[2] = { 0 };
    u8 id_cmd[4] = { 0 };
    u32 id_cmd_len = 0;
    struct i2c_client *client = ts_data->client;

    ts_data->ic_info.is_incell = FTS_CHIP_IDC;
    ts_data->ic_info.hid_supported = FTS_HID_SUPPORTTED;
    do {
        ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID, &chip_id[0]);
        ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID2, &chip_id[1]);
        if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
            FTS_DEBUG("i2c read invalid, read:0x%02x%02x", chip_id[0], chip_id[1]);
        } else {
            ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], VALID);
            if (!ret)
                break;
            else
                FTS_DEBUG("TP not ready, read:0x%02x%02x", chip_id[0], chip_id[1]);
        }

        cnt++;
        msleep(INTERVAL_READ_REG);
    } while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

    if ((cnt * INTERVAL_READ_REG) >= TIMEOUT_READ_REG) {
        FTS_INFO("fw is invalid, need read boot id");
        if (ts_data->ic_info.hid_supported) {
            fts_i2c_hid2std(client);
        }

        id_cmd[0] = FTS_CMD_START1;
        id_cmd[1] = FTS_CMD_START2;
        ret = fts_i2c_write(client, id_cmd, 2);
        if (ret < 0) {
            FTS_ERROR("start cmd write fail");
            return ret;
        }

        msleep(FTS_CMD_START_DELAY);
        id_cmd[0] = FTS_CMD_READ_ID;
        id_cmd[1] = id_cmd[2] = id_cmd[3] = 0x00;
        if (ts_data->ic_info.is_incell)
            id_cmd_len = FTS_CMD_READ_ID_LEN_INCELL;
        else
            id_cmd_len = FTS_CMD_READ_ID_LEN;
        ret = fts_i2c_read(client, id_cmd, id_cmd_len, chip_id, 2);
        if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
            FTS_ERROR("read boot id fail");
            return -EIO;
        }
        ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], INVALID);
        if (ret < 0) {
            FTS_ERROR("can't get ic informaton");
            return ret;
        }
    }

    FTS_INFO("get ic information, chip id = 0x%02x%02x",
             ts_data->ic_info.ids.chip_idh, ts_data->ic_info.ids.chip_idl);

    return 0;
}

/*****************************************************************************
*  Name: fts_tp_state_recovery
*  Brief: Need execute this function when reset
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_tp_state_recovery(struct i2c_client *client)
{
    FTS_FUNC_ENTER();
    /* wait tp stable */
    fts_wait_tp_to_valid(client);
    /* recover TP charger state 0x8B */
    /* recover TP glove state 0xC0 */
    /* recover TP cover state 0xC1 */
    fts_ex_mode_recovery(client);
    /* recover TP gesture state 0xD0 */
#if FTS_GESTURE_EN
    fts_gesture_recovery(client);
#endif
    FTS_FUNC_EXIT();
}

/*****************************************************************************
*  Name: fts_reset_proc
*  Brief: Execute reset operation
*  Input: hdelayms - delay time unit:ms
*  Output:
*  Return:
*****************************************************************************/
int fts_reset_proc(int hdelayms)
{
    FTS_FUNC_ENTER();
    gpio_direction_output(fts_data->pdata->reset_gpio, 0);
    msleep(20);
    gpio_direction_output(fts_data->pdata->reset_gpio, 1);
    if (hdelayms) {
        msleep(hdelayms);
    }

    FTS_FUNC_EXIT();
    return 0;
}

/*****************************************************************************
*  Name: fts_irq_disable
*  Brief: disable irq
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_irq_disable(void)
{
    unsigned long irqflags;

    FTS_FUNC_ENTER();
    spin_lock_irqsave(&fts_data->irq_lock, irqflags);

    if (!fts_data->irq_disabled) {
        disable_irq_nosync(fts_data->irq);
        fts_data->irq_disabled = true;
    }

    spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
    FTS_FUNC_EXIT();
}

/*****************************************************************************
*  Name: fts_irq_enable
*  Brief: enable irq
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_irq_enable(void)
{
    unsigned long irqflags = 0;

    FTS_FUNC_ENTER();
    spin_lock_irqsave(&fts_data->irq_lock, irqflags);

    if (fts_data->irq_disabled) {
        enable_irq(fts_data->irq);
        fts_data->irq_disabled = false;
    }

    spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
    FTS_FUNC_EXIT();
}

#if FTS_POWER_SOURCE_CUST_EN
/*****************************************************************************
* Power Control
*****************************************************************************/
static int fts_power_source_init(struct fts_ts_data *data)
{
    int ret = 0;

    FTS_FUNC_ENTER();

    data->vdd = regulator_get(&data->client->dev, "vdd");
    if (IS_ERR(data->vdd)) {
        ret = PTR_ERR(data->vdd);
        FTS_ERROR("get vdd regulator failed,ret=%d", ret);
        return ret;
    }

    if (regulator_count_voltages(data->vdd) > 0) {
        ret = regulator_set_voltage(data->vdd, FTS_VTG_MIN_UV, FTS_VTG_MAX_UV);
        if (ret) {
            FTS_ERROR("vdd regulator set_vtg failed ret=%d", ret);
            goto err_set_vtg_vdd;
        }
    }

    data->vcc_i2c = regulator_get(&data->client->dev, "vcc_i2c");
    if (IS_ERR(data->vcc_i2c)) {
        ret = PTR_ERR(data->vcc_i2c);
        FTS_ERROR("ret vcc_i2c regulator failed,ret=%d", ret);
        goto err_get_vcc;
    }

    if (regulator_count_voltages(data->vcc_i2c) > 0) {
        ret = regulator_set_voltage(data->vcc_i2c, FTS_I2C_VTG_MIN_UV, FTS_I2C_VTG_MAX_UV);
        if (ret) {
            FTS_ERROR("vcc_i2c regulator set_vtg failed ret=%d", ret);
            goto err_set_vtg_vcc;
        }
    }

    FTS_FUNC_EXIT();
    return 0;

err_set_vtg_vcc:
    regulator_put(data->vcc_i2c);
err_get_vcc:
    if (regulator_count_voltages(data->vdd) > 0)
        regulator_set_voltage(data->vdd, 0, FTS_VTG_MAX_UV);
err_set_vtg_vdd:
    regulator_put(data->vdd);

    FTS_FUNC_EXIT();
    return ret;
}

static int fts_power_source_release(struct fts_ts_data *data)
{
    if (regulator_count_voltages(data->vdd) > 0)
        regulator_set_voltage(data->vdd, 0, FTS_VTG_MAX_UV);
    regulator_put(data->vdd);

    if (regulator_count_voltages(data->vcc_i2c) > 0)
        regulator_set_voltage(data->vcc_i2c, 0, FTS_I2C_VTG_MAX_UV);
    regulator_put(data->vcc_i2c);

    return 0;
}

static int fts_power_source_ctrl(struct fts_ts_data *data, int enable)
{
    int ret = 0;

    FTS_FUNC_ENTER();
    if (enable) {
        if (data->power_disabled) {
            FTS_DEBUG("regulator enable !");
            ret = regulator_enable(data->vdd);
            if (ret) {
                FTS_ERROR("enable vdd regulator failed,ret=%d", ret);
            }

            ret = regulator_enable(data->vcc_i2c);
            if (ret) {
                FTS_ERROR("enable vcc_i2c regulator failed,ret=%d", ret);
            }
            data->power_disabled = false;
        }
    } else {
        if (!data->power_disabled) {
            FTS_DEBUG("regulator disable !");
            ret = regulator_disable(data->vdd);
            if (ret) {
                FTS_ERROR("disable vdd regulator failed,ret=%d", ret);
            }
            ret = regulator_disable(data->vcc_i2c);
            if (ret) {
                FTS_ERROR("disable vcc_i2c regulator failed,ret=%d", ret);
            }
            data->power_disabled = true;
        }
    }

    FTS_FUNC_EXIT();
    return ret;
}

#if FTS_PINCTRL_EN
/*****************************************************************************
*  Name: fts_pinctrl_init
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_pinctrl_init(struct fts_ts_data *ts)
{
    int ret = 0;
    struct i2c_client *client = ts->client;

    ts->pinctrl = devm_pinctrl_get(&client->dev);
    if (IS_ERR_OR_NULL(ts->pinctrl)) {
        FTS_ERROR("Failed to get pinctrl, please check dts");
        ret = PTR_ERR(ts->pinctrl);
        goto err_pinctrl_get;
    }

    ts->pins_active = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_active");
    if (IS_ERR_OR_NULL(ts->pins_active)) {
        FTS_ERROR("Pin state[active] not found");
        ret = PTR_ERR(ts->pins_active);
        goto err_pinctrl_lookup;
    }

    ts->pins_suspend = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_suspend");
    if (IS_ERR_OR_NULL(ts->pins_suspend)) {
        FTS_ERROR("Pin state[suspend] not found");
        ret = PTR_ERR(ts->pins_suspend);
        goto err_pinctrl_lookup;
    }

    ts->pins_release = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_release");
    if (IS_ERR_OR_NULL(ts->pins_release)) {
        FTS_ERROR("Pin state[release] not found");
        ret = PTR_ERR(ts->pins_release);
    }

    return 0;
err_pinctrl_lookup:
    if (ts->pinctrl) {
        devm_pinctrl_put(ts->pinctrl);
    }
err_pinctrl_get:
    ts->pinctrl = NULL;
    ts->pins_release = NULL;
    ts->pins_suspend = NULL;
    ts->pins_active = NULL;
    return ret;
}

static int fts_pinctrl_select_normal(struct fts_ts_data *ts)
{
    int ret = 0;

    if (ts->pinctrl && ts->pins_active) {
        ret = pinctrl_select_state(ts->pinctrl, ts->pins_active);
        if (ret < 0) {
            FTS_ERROR("Set normal pin state error:%d", ret);
        }
    }

    return ret;
}

static int fts_pinctrl_select_suspend(struct fts_ts_data *ts)
{
    int ret = 0;

    if (ts->pinctrl && ts->pins_suspend) {
        ret = pinctrl_select_state(ts->pinctrl, ts->pins_suspend);
        if (ret < 0) {
            FTS_ERROR("Set suspend pin state error:%d", ret);
        }
    }

    return ret;
}

static int fts_pinctrl_select_release(struct fts_ts_data *ts)
{
    int ret = 0;

    if (ts->pinctrl) {
        if (IS_ERR_OR_NULL(ts->pins_release)) {
            devm_pinctrl_put(ts->pinctrl);
            ts->pinctrl = NULL;
        } else {
            ret = pinctrl_select_state(ts->pinctrl, ts->pins_release);
            if (ret < 0)
                FTS_ERROR("Set gesture pin state error:%d", ret);
        }
    }

    return ret;
}
#endif /* FTS_PINCTRL_EN */

#endif /* FTS_POWER_SOURCE_CUST_EN */

/*****************************************************************************
*  Reprot related
*****************************************************************************/
#if (FTS_DEBUG_EN && (FTS_DEBUG_LEVEL == 2))
char g_sz_debug[1024] = {0};
static void fts_show_touch_buffer(u8 *buf, int point_num)
{
    int len = point_num * FTS_ONE_TCH_LEN;
    int count = 0;
    int i;

    memset(g_sz_debug, 0, 1024);
    if (len > (fts_data->pnt_buf_size - 3)) {
        len = fts_data->pnt_buf_size - 3;
    } else if (len == 0) {
        len += FTS_ONE_TCH_LEN;
    }
    count += snprintf(g_sz_debug, PAGE_SIZE, "%02X,%02X,%02X", buf[0], buf[1], buf[2]);
    for (i = 0; i < len; i++) {
        count += snprintf(g_sz_debug + count, PAGE_SIZE, ",%02X", buf[i + 3]);
    }
    FTS_DEBUG("buffer: %s", g_sz_debug);
}
#endif

/*****************************************************************************
 *  Name: fts_release_all_finger
 *  Brief: report all points' up events, release touch
 *  Input:
 *  Output:
 *  Return:
 *****************************************************************************/
static void fts_release_all_finger(void)
{
    struct input_dev *input_dev = fts_data->input_dev;
#if FTS_MT_PROTOCOL_B_EN
    u32 finger_count = 0;
#endif

    FTS_FUNC_ENTER();
    mutex_lock(&fts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
    for (finger_count = 0; finger_count < fts_data->pdata->max_touch_number; finger_count++) {
        input_mt_slot(input_dev, finger_count);
        input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
    }
#else
    input_mt_sync(input_dev);
#endif
    input_report_key(input_dev, BTN_TOUCH, 0);
    input_sync(input_dev);

    mutex_unlock(&fts_data->report_mutex);
    FTS_FUNC_EXIT();
}

/************************************************************************
 * Name: fts_input_report_key
 * Brief: report key event
 * Input: events info
 * Output:
 * Return: return 0 if success
 ***********************************************************************/
static int fts_input_report_key(struct fts_ts_data *data, int index)
{
    u32 ik;
    int id = data->events[index].id;
    int x = data->events[index].x;
    int y = data->events[index].y;
    int flag = data->events[index].flag;
    u32 key_num = data->pdata->key_number;

    if (!KEY_EN(data)) {
        return -EINVAL;
    }
    for (ik = 0; ik < key_num; ik++) {
        if (TOUCH_IN_KEY(x, data->pdata->key_x_coords[ik])) {
            if (EVENT_DOWN(flag)) {
                data->key_down = true;
                input_report_key(data->input_dev, data->pdata->keys[ik], 1);
                FTS_DEBUG("Key%d(%d, %d) DOWN!", ik, x, y);
            } else {
                data->key_down = false;
                input_report_key(data->input_dev, data->pdata->keys[ik], 0);
                FTS_DEBUG("Key%d(%d, %d) Up!", ik, x, y);
            }
            return 0;
        }
    }

    FTS_ERROR("invalid touch for key, [%d](%d, %d)", id, x, y);
    return -EINVAL;
}

#if FTS_MT_PROTOCOL_B_EN
static int fts_input_report_b(struct fts_ts_data *data)
{
    int i = 0;
    int uppoint = 0;
    int touchs = 0;
    bool va_reported = false;
    u32 max_touch_num = data->pdata->max_touch_number;
    u32 key_y_coor = data->pdata->key_y_coord;
    struct ts_event *events = data->events;

    for (i = 0; i < data->touch_point; i++) {
        if (KEY_EN(data) && TOUCH_IS_KEY(events[i].y, key_y_coor)) {
            fts_input_report_key(data, i);
            continue;
        }

        if (events[i].id >= max_touch_num)
            break;

        va_reported = true;
        input_mt_slot(data->input_dev, events[i].id);

        if (EVENT_DOWN(events[i].flag)) {
            input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);

#if FTS_REPORT_PRESSURE_EN
            if (events[i].p <= 0) {
                events[i].p = 0x3f;
            }
            input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
            if (events[i].area <= 0) {
                events[i].area = 0x09;
            }
            input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);
            input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
            input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);

            touchs |= BIT(events[i].id);
            data->touchs |= BIT(events[i].id);

            FTS_DEBUG("[B]P%d(%d, %d)[p:%d,tm:%d] DOWN!", events[i].id, events[i].x,
                      events[i].y, events[i].p, events[i].area);
        } else {
            uppoint++;
            input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
            data->touchs &= ~BIT(events[i].id);
            FTS_DEBUG("[B]P%d UP!", events[i].id);
        }
    }

    if (unlikely(data->touchs ^ touchs)) {
        for (i = 0; i < max_touch_num; i++)  {
            if (BIT(i) & (data->touchs ^ touchs)) {
                FTS_DEBUG("[B]P%d UP!", i);
                va_reported = true;
                input_mt_slot(data->input_dev, i);
                input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
            }
        }
    }
    data->touchs = touchs;

    if (va_reported) {
        /* touchs==0, there's no point but key */
        if (EVENT_NO_DOWN(data) || (!touchs)) {
            FTS_DEBUG("[B]Points All Up!");
            input_report_key(data->input_dev, BTN_TOUCH, 0);
        } else {
            input_report_key(data->input_dev, BTN_TOUCH, 1);
        }
    }

    input_sync(data->input_dev);
    return 0;
}

#else
static int fts_input_report_a(struct fts_ts_data *data)
{
    int i = 0;
    int touchs = 0;
    bool va_reported = false;
    u32 key_y_coor = data->pdata->key_y_coord;
    struct ts_event *events = data->events;

    for (i = 0; i < data->touch_point; i++) {
        if (KEY_EN(data) && TOUCH_IS_KEY(events[i].y, key_y_coor)) {
            fts_input_report_key(data, i);
            continue;
        }

        va_reported = true;
        if (EVENT_DOWN(events[i].flag)) {
            input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, events[i].id);
#if FTS_REPORT_PRESSURE_EN
            if (events[i].p <= 0) {
                events[i].p = 0x3f;
            }
            input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
            if (events[i].area <= 0) {
                events[i].area = 0x09;
            }
            input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);

            input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
            input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);

            input_mt_sync(data->input_dev);

            FTS_DEBUG("[A]P%d(%d, %d)[p:%d,tm:%d] DOWN!", events[i].id, events[i].x,
                      events[i].y, events[i].p, events[i].area);
            touchs++;
        }
    }

    /* last point down, current no point but key */
    if (data->touchs && !touchs) {
        va_reported = true;
    }
    data->touchs = touchs;

    if (va_reported) {
        if (EVENT_NO_DOWN(data)) {
            FTS_DEBUG("[A]Points All Up!");
            input_report_key(data->input_dev, BTN_TOUCH, 0);
            input_mt_sync(data->input_dev);
        } else {
            input_report_key(data->input_dev, BTN_TOUCH, 1);
        }
    }

    input_sync(data->input_dev);
    return 0;
}
#endif

/*****************************************************************************
*  Name: fts_read_touchdata
*  Brief:
*  Input:
*  Output:
*  Return: return 0 if succuss
*****************************************************************************/
static int fts_read_touchdata(struct fts_ts_data *data)
{
    int ret = 0;
    int i = 0;
    u8 pointid;
    int base;
    struct ts_event *events = data->events;
    int max_touch_num = data->pdata->max_touch_number;
    u8 *buf = data->point_buf;
    struct i2c_client *client = data->client;

#if FTS_GESTURE_EN
    if (0 == fts_gesture_readdata(data)) {
        FTS_INFO("succuss to get gesture data in irq handler");
        return 1;
    }
#endif

#if FTS_POINT_REPORT_CHECK_EN
    fts_prc_queue_work(data);
#endif

    data->point_num = 0;
    data->touch_point = 0;

    memset(buf, 0xFF, data->pnt_buf_size);
    buf[0] = 0x00;

    ret = fts_i2c_read(data->client, buf, 1, buf, data->pnt_buf_size);
    if (ret < 0) {
        FTS_ERROR("read touchdata failed, ret:%d", ret);
        return ret;
    }
    data->point_num = buf[FTS_TOUCH_POINT_NUM] & 0x0F;

    if (data->ic_info.is_incell) {
        if ((data->point_num == 0x0F) && (buf[1] == 0xFF) && (buf[2] == 0xFF)
            && (buf[3] == 0xFF) && (buf[4] == 0xFF) && (buf[5] == 0xFF) && (buf[6] == 0xFF)) {
            FTS_INFO("touch buff is 0xff, need recovery state");
            fts_tp_state_recovery(client);
            return -EIO;
        }
    }

    if (data->point_num > max_touch_num) {
        FTS_INFO("invalid point_num(%d)", data->point_num);
        return -EIO;
    }

#if (FTS_DEBUG_EN && (FTS_DEBUG_LEVEL == 2))
    fts_show_touch_buffer(buf, data->point_num);
#endif

    for (i = 0; i < max_touch_num; i++) {
        base = FTS_ONE_TCH_LEN * i;

        pointid = (buf[FTS_TOUCH_ID_POS + base]) >> 4;
        if (pointid >= FTS_MAX_ID)
            break;
        else if (pointid >= max_touch_num) {
            FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
            return -EINVAL;
        }

        data->touch_point++;

        events[i].x = ((buf[FTS_TOUCH_X_H_POS + base] & 0x0F) << 8) +
                      (buf[FTS_TOUCH_X_L_POS + base] & 0xFF);
        events[i].y = ((buf[FTS_TOUCH_Y_H_POS + base] & 0x0F) << 8) +
                      (buf[FTS_TOUCH_Y_L_POS + base] & 0xFF);
        events[i].flag = buf[FTS_TOUCH_EVENT_POS + base] >> 6;
        events[i].id = buf[FTS_TOUCH_ID_POS + base] >> 4;
        events[i].area = buf[FTS_TOUCH_AREA_POS + base] >> 4;
        events[i].p =  buf[FTS_TOUCH_PRE_POS + base];

        if (EVENT_DOWN(events[i].flag) && (data->point_num == 0)) {
            FTS_INFO("abnormal touch data from fw");
            return -EIO;
        }
    }
    if (data->touch_point == 0) {
        FTS_INFO("no touch point information");
        return -EIO;
    }

    return 0;
}

/*****************************************************************************
*  Name: fts_report_event
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void fts_report_event(struct fts_ts_data *data)
{
#if FTS_MT_PROTOCOL_B_EN
    fts_input_report_b(data);
#else
    fts_input_report_a(data);
#endif
}

/*****************************************************************************
*  Name: fts_ts_interrupt
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static irqreturn_t fts_ts_interrupt(int irq, void *data)
{
	__pm_wakeup_event(&gestrue_wakelock, 5000);

	queue_work(fts_data->ts_workqueue, &fts_data->interr_work);

    return IRQ_HANDLED;
}

static void fts_interr_work(struct work_struct *work)
{
	int ret = 0;

#if FTS_ESDCHECK_EN
    fts_esdcheck_set_intr(1);
#endif

    ret = fts_read_touchdata(fts_data);
    if (ret == 0) {
        mutex_lock(&fts_data->report_mutex);
        fts_report_event(fts_data);
        mutex_unlock(&fts_data->report_mutex);
    }

#if FTS_ESDCHECK_EN
    fts_esdcheck_set_intr(0);
#endif

}

/*****************************************************************************
*  Name: fts_irq_registration
*  Brief:
*  Input:
*  Output:
*  Return: return 0 if succuss, otherwise return error code
*****************************************************************************/
static int fts_irq_registration(struct fts_ts_data *ts_data)
{
    int ret = 0;
    struct fts_ts_platform_data *pdata = ts_data->pdata;

    ts_data->irq = gpio_to_irq(pdata->irq_gpio);
    FTS_INFO("irq in ts_data:%d irq in client:%d", ts_data->irq, ts_data->client->irq);
    if (ts_data->irq != ts_data->client->irq)
        FTS_ERROR("IRQs are inconsistent, please check <interrupts> & <focaltech,irq-gpio> in DTS");

    if (0 == pdata->irq_gpio_flags)
        pdata->irq_gpio_flags = IRQF_TRIGGER_FALLING;
    FTS_INFO("irq flag:%x", pdata->irq_gpio_flags);
    ret = request_threaded_irq(ts_data->irq, NULL, fts_ts_interrupt,
                               pdata->irq_gpio_flags | IRQF_ONESHOT | IRQF_NO_SUSPEND,
                               ts_data->client->name, ts_data);

    return ret;
}

/*****************************************************************************
*  Name: fts_input_init
*  Brief: input device init
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_input_init(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int key_num = 0;
    struct fts_ts_platform_data *pdata = ts_data->pdata;
    struct input_dev *input_dev;
    int point_num;

    FTS_FUNC_ENTER();

    input_dev = input_allocate_device();
    if (!input_dev) {
        FTS_ERROR("Failed to allocate memory for input device");
        return -ENOMEM;
    }

    /* Init and register Input device */
    input_dev->name = FTS_DRIVER_NAME;
    input_dev->id.bustype = BUS_I2C;
    input_dev->dev.parent = &ts_data->client->dev;

    input_set_drvdata(input_dev, ts_data);

    __set_bit(EV_SYN, input_dev->evbit);
    __set_bit(EV_ABS, input_dev->evbit);
    __set_bit(EV_KEY, input_dev->evbit);
    __set_bit(BTN_TOUCH, input_dev->keybit);
    __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

    if (pdata->have_key) {
        FTS_INFO("set key capabilities");
        for (key_num = 0; key_num < pdata->key_number; key_num++)
            input_set_capability(input_dev, EV_KEY, pdata->keys[key_num]);
    }

#if FTS_MT_PROTOCOL_B_EN
    input_mt_init_slots(input_dev, pdata->max_touch_number, INPUT_MT_DIRECT);
#else
    input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 0x0f, 0, 0);
#endif
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min, pdata->x_max, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min, pdata->y_max, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
#if FTS_REPORT_PRESSURE_EN
    input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
#endif
    point_num = pdata->max_touch_number;
    ts_data->pnt_buf_size = point_num * FTS_ONE_TCH_LEN + 3;
    ts_data->point_buf = (u8 *)kzalloc(ts_data->pnt_buf_size, GFP_KERNEL);
    if (!ts_data->point_buf) {
        FTS_ERROR("failed to alloc memory for point buf!");
        ret = -ENOMEM;
        goto err_point_buf;
    }

    ts_data->events = (struct ts_event *)kzalloc(point_num * sizeof(struct ts_event), GFP_KERNEL);
    if (!ts_data->events) {

        FTS_ERROR("failed to alloc memory for point events!");
        ret = -ENOMEM;
        goto err_event_buf;
    }


    ret = input_register_device(input_dev);
    if (ret) {
        FTS_ERROR("Input device registration failed");
        goto err_input_reg;
    }

    ts_data->input_dev = input_dev;

    FTS_FUNC_EXIT();
    return 0;

err_input_reg:
    kfree_safe(ts_data->events);

err_event_buf:
    kfree_safe(ts_data->point_buf);

err_point_buf:
    input_set_drvdata(input_dev, NULL);
    input_free_device(input_dev);
    input_dev = NULL;

    FTS_FUNC_EXIT();
    return ret;
}


/*****************************************************************************
*  Name: fts_gpio_configure
*  Brief: Configure IRQ&RESET GPIO
*  Input:
*  Output:
*  Return: return 0 if succuss
*****************************************************************************/
static int fts_gpio_configure(struct fts_ts_data *data)
{
    int ret = 0;

    FTS_FUNC_ENTER();
    /* request irq gpio */
    if (gpio_is_valid(data->pdata->irq_gpio)) {
        ret = gpio_request(data->pdata->irq_gpio, "fts_irq_gpio");
        if (ret) {
            FTS_ERROR("[GPIO]irq gpio request failed");
            goto err_irq_gpio_req;
        }

        ret = gpio_direction_input(data->pdata->irq_gpio);
        if (ret) {
            FTS_ERROR("[GPIO]set_direction for irq gpio failed");
            goto err_irq_gpio_dir;
        }
    }

    /* request reset gpio */
    if (gpio_is_valid(data->pdata->reset_gpio)) {
        ret = gpio_request(data->pdata->reset_gpio, "fts_reset_gpio");
        if (ret) {
            FTS_ERROR("[GPIO]reset gpio request failed");
            goto err_irq_gpio_dir;
        }

        ret = gpio_direction_output(data->pdata->reset_gpio, 1);
        if (ret) {
            FTS_ERROR("[GPIO]set_direction for reset gpio failed");
            goto err_reset_gpio_dir;
        }
    }

    FTS_FUNC_EXIT();
    return 0;

err_reset_gpio_dir:
    if (gpio_is_valid(data->pdata->reset_gpio))
        gpio_free(data->pdata->reset_gpio);
err_irq_gpio_dir:
    if (gpio_is_valid(data->pdata->irq_gpio))
        gpio_free(data->pdata->irq_gpio);
err_irq_gpio_req:
    FTS_FUNC_EXIT();
    return ret;
}

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
static struct xiaomi_touch_interface xiaomi_touch_interfaces;
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
static void fts_init_touchmode_data(void)
{
	int i;
	int ret;
	u8 reg_value;

	/* Touch Game Mode Switch */
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] = 0;

	/* Acitve Mode */
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_DEF_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_CUR_VALUE] = 0;

	/* sensivity */
	ret = fts_i2c_read_reg(fts_data->client, FTS_REG_SENSIVITY, &reg_value);
	if (ret < 0) {
		FTS_ERROR("read sensivity reg error");
	}
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MAX_VALUE] = 50;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MIN_VALUE] = 8;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] = reg_value;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_CUR_VALUE] = reg_value;

	/*  Tolerance */
	ret = fts_i2c_read_reg(fts_data->client, FTS_REG_THDIFF, &reg_value);
	if (ret < 0) {
		FTS_ERROR("read reg thdiff error");
	}
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MAX_VALUE] = 255;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MIN_VALUE] = 60;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE] = 112;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE] = reg_value;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_CUR_VALUE] = reg_value;
	/* edge filter orientation*/
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] = 0;

	/* edge filter area*/
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] = 2;


	for (i = 0; i < Touch_Mode_NUM; i++) {
		FTS_INFO("mode:%d, set cur:%d, get cur:%d, def:%d min:%d max:%d",
			i,
			xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MIN_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MAX_VALUE]);
	}

	return;
}

static void fts_update_touchmode_data(int mode)
{
	u8 temp_value;
	u8 reg_value = 0;
	int ret;

	mutex_lock(&fts_data->gamemode_mutex);
	temp_value = (u8)xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];
	switch (mode) {
	case Touch_Game_Mode:
		/*enable touch game mode,set tp into active mode, set high report rate*/
		if (temp_value == 1) {
			ret = fts_i2c_write_reg(fts_data->client, FTS_REG_MONITOR_MODE, 0);
			if (ret < 0)
				FTS_ERROR("disable monitor mode error, ret=%d", ret);
			fts_data->gamemode_enabled = true;
		} else {
			/*restore touch parameters */
			ret = fts_i2c_write_reg(fts_data->client, FTS_REG_MONITOR_MODE, 1);
			if (ret < 0)
				FTS_ERROR("restore monitor mode error, ret=%d", ret);
			ret = fts_i2c_write_reg(fts_data->client, FTS_REG_SENSIVITY, (u8)xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE]);
			if (ret < 0)
				FTS_ERROR("restore sensitivity error, ret=%d", ret);
			ret = fts_i2c_write_reg(fts_data->client, FTS_REG_THDIFF, (u8)xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE]);
			if (ret < 0)
				FTS_ERROR("restore touch smooth error, ret=%d", ret);
			ret = fts_i2c_write_reg(fts_data->client, FTS_REG_EDGE_FILTER_ORIENTATION, (u8)xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE]);
			if (ret < 0)
				FTS_ERROR("restore orientation error, ret=%d", ret);
			ret = fts_i2c_write_reg(fts_data->client, FTS_REG_EDGE_FILTER_LEVEL, (u8)xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE]);
			if (ret < 0)
				FTS_ERROR("restore orientation error, ret=%d", ret);
			fts_data->gamemode_enabled = false;

			FTS_ERROR("Reset Monitor: 1,Sensivity: %d,Thdiff: %d,Edge-Orientation: %d,Edge-filter-level: %d",
						xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE],
							xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE],
								xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE],
									xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE]);
		}
		break;
	case Touch_Active_MODE:
		break;
	case Touch_UP_THRESHOLD:
			if (fts_data->gamemode_enabled) {
				if (temp_value >= 8 && temp_value < 20)
					reg_value = 0x0E;
				else if (temp_value >= 20 && temp_value < 35)
					reg_value = 0x0F;
				else if (temp_value >= 35 && temp_value <= 50)
					reg_value = 0x10;
				else
					reg_value = 0x0E;

				ret = fts_i2c_write_reg(fts_data->client, FTS_REG_SENSIVITY, reg_value);
				if (ret < 0)
					FTS_ERROR("write sensitivity error, ret=%d\n", ret);
			}
		break;
	case Touch_Tolerance:
			if (fts_data->gamemode_enabled) {
				reg_value = temp_value;
				ret = fts_i2c_write_reg(fts_data->client, FTS_REG_THDIFF, reg_value);
				if (ret < 0)
					FTS_ERROR("write touch smooth error, ret=%d\n", ret);
			}
		break;
	case Touch_Panel_Orientation:
			if (temp_value == 0 || temp_value == 2) {
				reg_value = 0;
				ret = fts_i2c_write_reg(fts_data->client, FTS_REG_EDGE_FILTER_ORIENTATION, reg_value);
				if (ret < 0)
					FTS_ERROR("write orientation error, ret=%d\n", ret);
			}
			if (temp_value == 1) {
				reg_value = 1;
				ret = fts_i2c_write_reg(fts_data->client, FTS_REG_EDGE_FILTER_ORIENTATION, reg_value);
				if (ret < 0)
					FTS_ERROR("write orientation error, ret=%d\n", ret);
			}
			if (temp_value == 3) {
				reg_value = 2;
				ret = fts_i2c_write_reg(fts_data->client, FTS_REG_EDGE_FILTER_ORIENTATION, reg_value);
				if (ret < 0)
					FTS_ERROR("write orientation error, ret=%d\n", ret);
			}
		break;
	case Touch_Edge_Filter:
			if (fts_data->gamemode_enabled) {
				/*gamemode apk will set 0 as edge filter off,1 as level min, 2 as level middle,3 as level max
				 * register in touch ic:0 as default, 1 as edge filter off, 2 as level min, 3 as level middle,4 as level max
				 **/
				reg_value  = temp_value + 1;
				ret = fts_i2c_write_reg(fts_data->client, FTS_REG_EDGE_FILTER_LEVEL, reg_value);
				if (ret < 0)
					FTS_ERROR("write edge filter level error, ret=%d\n", ret);
			}
		break;
	case Touch_Report_Rate:
		break;
	default:
		break;
	}
	mutex_unlock(&fts_data->gamemode_mutex);
}

static int fts_set_cur_value(int mode, int value)
{
	if (fts_data->suspended) {
		FTS_INFO("%s, set mode:%d, value:%d failed while touch suspend!", __func__, mode, value);
		return 0;
	} else {

		if (mode < Touch_Mode_NUM && mode >= 0) {

			xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] = value;

			if (xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] >
				xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE]) {

				xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
					xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE];

			} else if (xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] <
				xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE]) {

			xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
					xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE];
			}
		} else {
			FTS_ERROR("%s, don't support\n",  __func__);
		}
		FTS_INFO("%s, mode:%d, value:%d", __func__, mode, value);
		fts_update_touchmode_data(mode);

		return 0;
	}
}

static int fts_get_mode_value(int mode, int value_type)
{
	int value = -1;

	if (mode < Touch_Mode_NUM && mode >= 0) {
		value = xiaomi_touch_interfaces.touch_mode[mode][value_type];
		FTS_INFO("%s, mode:%d, value:%d", __func__, mode, value);
        }else
		FTS_ERROR("%s, don't support\n", __func__);

	return value;
}

static int fts_get_mode_all(int mode, int *value)
{
	if (mode < Touch_Mode_NUM && mode >= 0) {
		value[0] = xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE];
		value[1] = xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		value[2] = xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE];
		value[3] = xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE];
	} else {
		FTS_ERROR("%s, don't support\n",  __func__);
	}
	FTS_INFO("%s, mode:%d, value:%d:%d:%d:%d\n", __func__, mode, value[0],
					value[1], value[2], value[3]);

	return 0;
}

static int fts_reset_mode(int mode)
{
	int i = 0;

	if (fts_data->suspended) {
		FTS_INFO("%s, reset failed while touch suspend!", __func__);
	} else {
		if (mode < Touch_Mode_NUM && mode > 0) {
			xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		} else if (mode == 0) {
			for (i = 0; i < Touch_Mode_NUM; i++) {
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE];
			}
		} else {
			FTS_ERROR("%s, don't support\n",  __func__);
		}

		FTS_ERROR("%s, mode:%d\n",  __func__, mode);
		fts_update_touchmode_data(mode);
	}

	return 0;
}
#endif
#endif

/*****************************************************************************
*  Name: fts_get_dt_coords
*  Brief:
*  Input:
*  Output:
*  Return: return 0 if succuss, otherwise return error code
*****************************************************************************/
static int fts_get_dt_coords(struct device *dev, char *name,
                             struct fts_ts_platform_data *pdata)
{
    int ret = 0;
    u32 coords[FTS_COORDS_ARR_SIZE] = { 0 };
    struct property *prop;
    struct device_node *np = dev->of_node;
    int coords_size;

    prop = of_find_property(np, name, NULL);
    if (!prop)
        return -EINVAL;
    if (!prop->value)
        return -ENODATA;

    coords_size = prop->length / sizeof(u32);
    if (coords_size != FTS_COORDS_ARR_SIZE) {
        FTS_ERROR("invalid:%s, size:%d", name, coords_size);
        return -EINVAL;
    }

    ret = of_property_read_u32_array(np, name, coords, coords_size);
    if (ret && (ret != -EINVAL)) {
        FTS_ERROR("Unable to read %s", name);
        return -ENODATA;
    }

    if (!strcmp(name, "focaltech,display-coords")) {
        pdata->x_min = coords[0];
        pdata->y_min = coords[1];
        pdata->x_max = coords[2];
        pdata->y_max = coords[3];
    } else {
        FTS_ERROR("unsupported property %s", name);
        return -EINVAL;
    }

    FTS_INFO("display x(%d %d) y(%d %d)", pdata->x_min, pdata->x_max,
             pdata->y_min, pdata->y_max);
    return 0;
}

/*****************************************************************************
*  Name: fts_parse_dt
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_parse_dt(struct device *dev, struct fts_ts_platform_data *pdata)
{
    int ret = 0;
    struct device_node *np = dev->of_node;
    u32 temp_val;

    FTS_FUNC_ENTER();

    ret = fts_get_dt_coords(dev, "focaltech,display-coords", pdata);
    if (ret < 0)
        FTS_ERROR("Unable to get display-coords");

    /* key */
    pdata->have_key = of_property_read_bool(np, "focaltech,have-key");
    if (pdata->have_key) {
        ret = of_property_read_u32(np, "focaltech,key-number", &pdata->key_number);
        if (ret)
            FTS_ERROR("Key number undefined!");

        ret = of_property_read_u32_array(np, "focaltech,keys",
                                         pdata->keys, pdata->key_number);
        if (ret)
            FTS_ERROR("Keys undefined!");
        else if (pdata->key_number > FTS_MAX_KEYS)
            pdata->key_number = FTS_MAX_KEYS;

        ret = of_property_read_u32(np, "focaltech,key-y-coord", &pdata->key_y_coord);
        if (ret)
            FTS_ERROR("Key Y Coord undefined!");

        ret = of_property_read_u32_array(np, "focaltech,key-x-coords",
                                         pdata->key_x_coords, pdata->key_number);
        if (ret)
            FTS_ERROR("Key X Coords undefined!");

        FTS_INFO("VK(%d): (%d, %d, %d), [%d, %d, %d][%d]",
                 pdata->key_number, pdata->keys[0], pdata->keys[1], pdata->keys[2],
                 pdata->key_x_coords[0], pdata->key_x_coords[1], pdata->key_x_coords[2],
                 pdata->key_y_coord);
    }

    /* reset, irq gpio info */
    pdata->reset_gpio = of_get_named_gpio_flags(np, "focaltech,reset-gpio", 0, &pdata->reset_gpio_flags);
    if (pdata->reset_gpio < 0)
        FTS_ERROR("Unable to get reset_gpio");

    pdata->irq_gpio = of_get_named_gpio_flags(np, "focaltech,irq-gpio", 0, &pdata->irq_gpio_flags);
    if (pdata->irq_gpio < 0)
        FTS_ERROR("Unable to get irq_gpio");

    ret = of_property_read_u32(np, "focaltech,max-touch-number", &temp_val);
    if (0 == ret) {
        if (temp_val < 2)
            pdata->max_touch_number = 2;
        else if (temp_val > FTS_MAX_POINTS_SUPPORT)
            pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
        else
            pdata->max_touch_number = temp_val;
    } else {
        FTS_ERROR("Unable to get max-touch-number");
        pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
    }

    FTS_INFO("max touch number:%d, irq gpio:%d, reset gpio:%d",
             pdata->max_touch_number, pdata->irq_gpio, pdata->reset_gpio);

    FTS_FUNC_EXIT();
    return 0;
}



#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
static ssize_t fts_gamemode_test_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	u8 monitor_mode, filter_ori, thdiff, sensitivity, report_rate, filter_level;

	fts_i2c_read_reg(fts_data->client, FTS_REG_MONITOR_MODE, &monitor_mode);
	fts_i2c_read_reg(fts_data->client, FTS_REG_REPORT_RATE, &report_rate);
	fts_i2c_read_reg(fts_data->client, FTS_REG_SENSIVITY, &sensitivity);
	fts_i2c_read_reg(fts_data->client, FTS_REG_THDIFF, &thdiff);
	fts_i2c_read_reg(fts_data->client, FTS_REG_EDGE_FILTER_ORIENTATION, &filter_ori);
	fts_i2c_read_reg(fts_data->client, FTS_REG_EDGE_FILTER_LEVEL, &filter_level);
	return snprintf(buf, PAGE_SIZE, "monitor_mode:0x%x\nreport_rate:0x%x\nsensitivity:0x%x\nfollowing performance:0x%x\nedge_filter_orieatation:0x%x\nedge_filter_level:0x%x\n",
			monitor_mode, report_rate, sensitivity, thdiff, filter_ori, filter_level);
}

static ssize_t fts_gamemode_test_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int mode, value;

	FTS_INFO("%s,buf:%s,count:%zu\n", __func__, buf, count);
	sscanf(buf, "%d %d", &mode, &value);
	fts_set_cur_value(mode, value);
	return count;
}
static DEVICE_ATTR(gamemode_test, 0644, fts_gamemode_test_show, fts_gamemode_test_store);
static struct attribute *fts_attrs[] = {
    &dev_attr_gamemode_test.attr,
    NULL
};
static const struct attribute_group fts_attr_group = {
    .attrs = fts_attrs
};
#endif


/* modify begin by zhangchaofan@longcheer.com for factory tp work, 2018-12-06 */
#ifdef CONFIG_KERNEL_CUSTOM_FACTORY
static int lct_tp_work_node_callback(bool flag)
{
	int ret = 0;

    if (enable_gesture_mode) {
		FTS_ERROR("ERROR: focal gesture=%d!\n", enable_gesture_mode);
		return -1;
	}

	if (fts_suspend_stats) return 0;

	if (flag) {// enable(resume) tp
		fts_suspend_stats = true;
		ret = fts_ts_resume(&fts_data->client->dev);
		if (ret < 0)
			FTS_ERROR("fts_ts_resume faild! ret=%d\n", ret);
	} else {// disbale(suspend) tp
		ret = fts_ts_suspend(&fts_data->client->dev);
		if (ret < 0)
			FTS_ERROR("fts_ts_suspend faild! ret=%d\n", ret);
	}
    fts_suspend_stats = false;
	return ret;
}
#endif
/* modify end by zhangchaofan@longcheer.com for factory tp work, 2018-12-06 */

/* modify begin by zhangchaofan@longcheer.com for angle inhibit, 2019-01-14 */
static int lct_tp_get_screen_angle_callback(void)
{
	return -EPERM;
}
static int lct_tp_set_screen_angle_callback(unsigned int angle)
{
	u8 val;
	int ret = -EIO;
	struct i2c_client *client = fts_data->client;

	FTS_FUNC_ENTER();

	if ( fts_data->suspended || (fts_data->touch_state == 1) ) {
		FTS_ERROR("tp is suspended or flashing, can not to set\n");
		return ret;
	}

	//mutex_lock(&fts_data->report_mutex);

	if (angle == 90) {
		val = 1;
	} else if (angle == 270) {
		val = 2;
	} else {
		val = 0;
	}

	ret = fts_i2c_write_reg(client,FTS_SET_ANGLE,val);
	if(ret < 0)
		FTS_ERROR("[FTS] i2c write FTS_SET_ANGLE, err\n");

	//mutex_unlock(&fts_data->report_mutex);
	FTS_FUNC_EXIT();
	return ret;
}
/* modify end by zhangchaofan@longcheer.com for angle inhibit, 2019-01-14 */

/* modify begin by zhangchaofan@longcheer.com for tp_gesture, 2018-12-25 */
#if FTS_GESTURE_EN
static int fts_gesture_switch(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
	if (type == EV_SYN && code == SYN_CONFIG)
	{
		if (fts_suspend_stats)
		{
			if ((value != WAKEUP_OFF) || enable_gesture_mode)
			{
				fts_delay_gesture = true;
			}
		}
		FTS_INFO("choose the gesture mode yes or not\n");
		if(value == WAKEUP_OFF){
			FTS_INFO("disable gesture mode\n");
			enable_gesture_mode = false;
		}else if(value == WAKEUP_ON){
			FTS_INFO("enable gesture mode\n");
			enable_gesture_mode  = true;
		}
	}
	return 0;
}

static int lct_tp_gesture_node_callback(bool flag)
{
	int retval = 0;

	if (fts_suspend_stats) {
		if (flag || enable_gesture_mode) {
			fts_delay_gesture = true;
		}
	}

	if(flag) {
		enable_gesture_mode = true;
		FTS_INFO("enable gesture mode\n");
	} else {
		enable_gesture_mode = false;
		FTS_INFO("disable gesture mode\n");
	}
	return retval;
}
#endif
/* modify end by zhangchaofan@longcheer.com for tp_gesture, 2018-12-25 */

#if defined(CONFIG_FB)

/* add begin by zhangchaofan@longcheer.com for resume dalay, 2019-01-08*/
static void tp_fb_notifier_resume_work(struct work_struct *work)
{
	int ret = 0;

	mutex_lock(&fts_data->pm_mutex);
	ret = fts_ts_resume(&fts_data->client->dev);
	if (ret < 0)
		FTS_ERROR("fts_ts_resume faild! ret=%d", ret);
	mutex_unlock(&fts_data->pm_mutex);
	return;
}
/* add end by zhangchaofan@longcheer.com for resume dalay, 2019-01-08 */

/*****************************************************************************
*  Name: fb_notifier_callback
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
/* modify begin by zhangchaofan@longcheer.com for kernel 4.9, 2018-12-25 */
static int fb_notifier_callback(struct notifier_block *self,
                                unsigned long event, void *data)
{
    struct msm_drm_notifier *evdata = data;
    int *blank;
    struct fts_ts_data *fts_data =
        container_of(self, struct fts_ts_data, fb_notif);

    if (evdata && evdata->data && event == MSM_DRM_EARLY_EVENT_BLANK &&
        fts_data && fts_data->client) {
        blank = evdata->data;
        if (*blank == MSM_DRM_BLANK_POWERDOWN) {
/* add begin by zhangchaofan@longcheer.com for resume dalay, 2019-02-16*/
		flush_work(&fts_data->fb_notify_work);
		mutex_lock(&fts_data->pm_mutex);
		fts_ts_suspend(&fts_data->client->dev);
		mutex_unlock(&fts_data->pm_mutex);
/* add end by zhangchaofan@longcheer.com for resume dalay, 2019-02-16 */
        }
    } else if (evdata && evdata->data && event == MSM_DRM_EVENT_BLANK &&
        fts_data && fts_data->client) {
        blank = evdata->data;
        if (*blank == MSM_DRM_BLANK_UNBLANK) {
/* add begin by zhangchaofan@longcheer.com for resume dalay, 2018-12-05*/
		//fts_ts_resume(&fts_data->client->dev);
		queue_work(fts_data->ts_workqueue, &fts_data->fb_notify_work);
/* add end by zhangchaofan@longcheer.com for resume dalay, 2018-12-05 */
        }

    }

    return 0;
}
/* modify end by zhangchaofan@longcheer.com for kernel 4.9, 2018-12-25 */
#elif defined(CONFIG_HAS_EARLYSUSPEND)
/*****************************************************************************
*  Name: fts_ts_early_suspend
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void fts_ts_early_suspend(struct early_suspend *handler)
{
    struct fts_ts_data *data = container_of(handler,
                                            struct fts_ts_data,
                                            early_suspend);

    fts_ts_suspend(&data->client->dev);
}

/*****************************************************************************
*  Name: fts_ts_late_resume
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void fts_ts_late_resume(struct early_suspend *handler)
{
    struct fts_ts_data *data = container_of(handler,
                                            struct fts_ts_data,
                                            early_suspend);

    fts_ts_resume(&data->client->dev);
}
#endif

/*****************************************************************************
*  Name: fts_ts_probe
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;
    struct fts_ts_platform_data *pdata;
    struct fts_ts_data *ts_data;

    FTS_FUNC_ENTER();
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        FTS_ERROR("I2C not supported");
        return -ENODEV;
    }

    if (client->dev.of_node) {
        pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
        if (!pdata) {
            FTS_ERROR("Failed to allocate memory for platform data");
            return -ENOMEM;
        }
        ret = fts_parse_dt(&client->dev, pdata);
        if (ret)
            FTS_ERROR("[DTS]DT parsing failed");
    } else {
        pdata = client->dev.platform_data;
    }

    if (!pdata) {
        FTS_ERROR("no ts platform data found");
        return -EINVAL;
    }

    ts_data = devm_kzalloc(&client->dev, sizeof(*ts_data), GFP_KERNEL);
    if (!ts_data) {
        FTS_ERROR("Failed to allocate memory for fts_data");
        return -ENOMEM;
    }

    fts_data = ts_data;
    ts_data->client = client;
    ts_data->pdata = pdata;
    i2c_set_clientdata(client, ts_data);

	fts_data->rbuf = (u8 *)kmalloc(256, GFP_KERNEL);
	if (fts_data->rbuf == NULL) {
		FTS_ERROR("failed to allocated memory for fts_data->rbuf\n");
		return -ENOMEM;
	}

	fts_data->wbuf = (u8 *)kmalloc(256, GFP_KERNEL);
	if (fts_data->wbuf == NULL) {
		FTS_ERROR("failed to allocated memory for fts_data->wbuf\n");
		return -ENOMEM;
	}

    ts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");
    if (NULL == ts_data->ts_workqueue) {
        FTS_ERROR("failed to create fts workqueue");
    }

    spin_lock_init(&ts_data->irq_lock);
    mutex_init(&ts_data->report_mutex);
    mutex_init(&ts_data->pm_mutex);

    ret = fts_input_init(ts_data);
    if (ret) {
        FTS_ERROR("fts input initialize fail");
        goto err_input_init;
    }

    /* add begin by zhangchaofan@longcheer.com for resume delay, 2019-01-08*/
    INIT_WORK(&ts_data->fb_notify_work, tp_fb_notifier_resume_work);
    INIT_WORK(&ts_data->interr_work, fts_interr_work);
    /* add end by zhangchaofan@longcheer.com for resume delay 2019-01-08 */

#if FTS_POWER_SOURCE_CUST_EN
    ret = fts_power_source_init(ts_data);
    if (ret) {
        FTS_ERROR("fail to get vdd/vcc_i2c regulator");
        goto err_power_init;
    }

    ts_data->power_disabled = true;
    ret = fts_power_source_ctrl(ts_data, ENABLE);
    if (ret) {
        FTS_ERROR("fail to enable vdd/vcc_i2c regulator");
        goto err_power_ctrl;
    }

#if FTS_PINCTRL_EN
    ret = fts_pinctrl_init(ts_data);
    if (0 == ret) {
        fts_pinctrl_select_normal(ts_data);
    }
#endif
#endif

    ret = fts_gpio_configure(ts_data);
    if (ret) {
        FTS_ERROR("[GPIO]Failed to configure the gpios");
        goto err_gpio_config;
    }

#if (!FTS_CHIP_IDC)
    fts_reset_proc(200);
#endif

    ret = fts_get_ic_information(ts_data);
    if (ret) {
        FTS_ERROR("not focal IC, unregister driver");
        goto err_irq_req;
    }

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
	ret = sysfs_create_group(&client->dev.kobj, &fts_attr_group);
	if (ret) {
		FTS_ERROR("fail to export sysfs entires\n");
		goto err_sysfs_create_group;
	}
#endif

#if FTS_APK_NODE_EN
    ret = fts_create_apk_debug_channel(ts_data);
    if (ret) {
        FTS_ERROR("create apk debug node fail");
    }
#endif

#if FTS_SYSFS_NODE_EN
    ret = fts_create_sysfs(client);
    if (ret) {
        FTS_ERROR("create sysfs node fail");
    }
#endif

#if FTS_POINT_REPORT_CHECK_EN
    ret = fts_point_report_check_init(ts_data);
    if (ret) {
        FTS_ERROR("init point report check fail");
    }
#endif

    ret = fts_ex_mode_init(client);
    if (ret) {
        FTS_ERROR("init glove/cover/charger fail");
    }

#if FTS_GESTURE_EN
    ret = fts_gesture_init(ts_data);
    if (ret) {
        FTS_ERROR("init gesture fail");
    }
/* modify begin by zhangchaofan@longcheer.com for tp_gesture, 2018-12-25 */
    ts_data->input_dev->event = fts_gesture_switch;
    ret = init_lct_tp_gesture(lct_tp_gesture_node_callback);
    if (ret < 0) {
        FTS_ERROR("Failed to add /proc/tp_gesture node!\n");
    }
/* modify end by zhangchaofan@longcheer.com for tp_gesture, 2018-12-25 */
#endif

#if FTS_TEST_EN
    ret = fts_test_init(client);
    if (ret) {
        FTS_ERROR("init production test fail");
    }
#endif

#if FTS_ESDCHECK_EN
    ret = fts_esdcheck_init(ts_data);
    if (ret) {
        FTS_ERROR("init esd check fail");
    }
#endif

    ret = fts_irq_registration(ts_data);
    if (ret) {
        FTS_ERROR("request irq failed");
        goto err_irq_req;
    }
	wakeup_source_init(&gestrue_wakelock, "gestrue_wakelock");

    //---set device node---
    /* add touchpad information by zhangchaofan start, 2018-11-27*/
    lct_fts_tp_info_node_init();
    /* add touchpad information by zhangchaofan end, 2018-11-27*/

    /* modify begin by zhangchaofan@longcheer.com for factory tp work, 2018-12-06 */
#ifdef CONFIG_KERNEL_CUSTOM_FACTORY
            ret = init_lct_tp_work(lct_tp_work_node_callback);
            if (ret < 0) {
                FTS_ERROR("Failed to add /proc/tp_work node!\n");
            }
#endif
    /* modify end by zhangchaofan@longcheer.com for factory tp work, 2018-12-06 */

    /* modify begin by zhangchaofan@longcheer.com for angle inhibit, 2019-01-14 */
	ret = init_lct_tp_grip_area(lct_tp_set_screen_angle_callback, lct_tp_get_screen_angle_callback);
	if (ret < 0) {
		FTS_ERROR("Failed to add /proc/tp_grip_area node!\n");
	}
    /* modify end by zhangchaofan@longcheer.com for angle inhibit, 2019-01-14 */

#if FTS_AUTO_UPGRADE_EN
    ret = fts_fwupg_init(ts_data);
    if (ret) {
        FTS_ERROR("init fw upgrade fail");
    }
#endif

#if defined(CONFIG_FB)
    ts_data->fb_notif.notifier_call = fb_notifier_callback;
    ret = msm_drm_register_client(&ts_data->fb_notif);  /* modify by zhangchaofan@longcheer.com for kernel 4.9, 2018-12-25 */
    if (ret) {
        FTS_ERROR("[FB]Unable to register fb_notifier: %d", ret);
    }
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    ts_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + FTS_SUSPEND_LEVEL;
    ts_data->early_suspend.suspend = fts_ts_early_suspend;
    ts_data->early_suspend.resume = fts_ts_late_resume;
    register_early_suspend(&ts_data->early_suspend);
#endif
	if (ts_data->fts_tp_class == NULL) {
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
		ts_data->fts_tp_class = get_xiaomi_touch_class();
#else
		ts_data->fts_tp_class = class_create(THIS_MODULE, "touch");
#endif
		if (ts_data->fts_tp_class) {
			ts_data->fts_touch_dev = device_create(ts_data->fts_tp_class, NULL, 0x38, ts_data, "tp_dev");
			if (IS_ERR(ts_data->fts_touch_dev)) {
				FTS_ERROR("Failed to create device !\n");
				goto err_class_create;
			}
			dev_set_drvdata(ts_data->fts_touch_dev, ts_data);
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
			memset(&xiaomi_touch_interfaces, 0x00, sizeof(struct xiaomi_touch_interface));
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
			xiaomi_touch_interfaces.getModeValue = fts_get_mode_value;
			xiaomi_touch_interfaces.setModeValue = fts_set_cur_value;
			xiaomi_touch_interfaces.resetMode = fts_reset_mode;
			xiaomi_touch_interfaces.getModeAll = fts_get_mode_all;
			fts_init_touchmode_data();
			mutex_init(&ts_data->gamemode_mutex);
#endif
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_PALMSENSOR
			xiaomi_touch_interfaces.palm_sensor_write = fts_palm_sensor_write;
#endif
			xiaomitouch_register_modedata(&xiaomi_touch_interfaces);
#endif
		}
	}

	FTS_FUNC_EXIT();
	return 0;
err_class_create:
	class_destroy(ts_data->fts_tp_class);
	ts_data->fts_tp_class = NULL;
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
err_sysfs_create_group:
	sysfs_remove_group(&client->dev.kobj, &fts_attr_group);
#endif
err_irq_req:
    if (gpio_is_valid(pdata->reset_gpio))
        gpio_free(pdata->reset_gpio);
    if (gpio_is_valid(pdata->irq_gpio))
        gpio_free(pdata->irq_gpio);
err_gpio_config:
#if FTS_POWER_SOURCE_CUST_EN
#if FTS_PINCTRL_EN
    fts_pinctrl_select_release(ts_data);
#endif
    fts_power_source_ctrl(ts_data, DISABLE);
err_power_ctrl:
    fts_power_source_release(ts_data);
err_power_init:
#endif
    kfree_safe(ts_data->point_buf);
	kfree(fts_data->rbuf);
	kfree(fts_data->wbuf);
    kfree_safe(ts_data->events);
	mutex_destroy(&ts_data->pm_mutex);
    input_unregister_device(ts_data->input_dev);
err_input_init:
    if (ts_data->ts_workqueue)
        destroy_workqueue(ts_data->ts_workqueue);
    devm_kfree(&client->dev, ts_data);

    FTS_FUNC_EXIT();
    return ret;
}

/*****************************************************************************
*  Name: fts_ts_remove
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_ts_remove(struct i2c_client *client)
{
    struct fts_ts_data *ts_data = i2c_get_clientdata(client);

    FTS_FUNC_ENTER();

#if FTS_POINT_REPORT_CHECK_EN
    fts_point_report_check_exit(ts_data);
#endif

#if FTS_APK_NODE_EN
    fts_release_apk_debug_channel(ts_data);
#endif

#if FTS_SYSFS_NODE_EN
    fts_remove_sysfs(client);
#endif

    fts_ex_mode_exit(client);

#if FTS_AUTO_UPGRADE_EN
    fts_fwupg_exit(ts_data);
#endif

#if FTS_TEST_EN
    fts_test_exit(client);
#endif

#if FTS_ESDCHECK_EN
    fts_esdcheck_exit(ts_data);
#endif

#if FTS_GESTURE_EN
    fts_gesture_exit(client);
#endif

#if defined(CONFIG_FB)
    if (msm_drm_unregister_client(&ts_data->fb_notif))  /* modify by zhangchaofan@longcheer.com for kernel 4.9, 2018-12-25 */
        FTS_ERROR("Error occurred while unregistering fb_notifier.");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    unregister_early_suspend(&ts_data->early_suspend);
#endif

    free_irq(ts_data->irq, ts_data);
    input_unregister_device(ts_data->input_dev);

    if (gpio_is_valid(ts_data->pdata->reset_gpio))
        gpio_free(ts_data->pdata->reset_gpio);

    if (gpio_is_valid(ts_data->pdata->irq_gpio))
        gpio_free(ts_data->pdata->irq_gpio);

    if (ts_data->ts_workqueue)
        destroy_workqueue(ts_data->ts_workqueue);

#if FTS_POWER_SOURCE_CUST_EN
#if FTS_PINCTRL_EN
    fts_pinctrl_select_release(ts_data);
#endif
    fts_power_source_ctrl(ts_data, DISABLE);
    fts_power_source_release(ts_data);
#endif

	wakeup_source_trash(&gestrue_wakelock);
    kfree_safe(ts_data->point_buf);
	kfree(fts_data->rbuf);
	kfree(fts_data->wbuf);
	mutex_destroy(&fts_data->pm_mutex);
    kfree_safe(ts_data->events);

    devm_kfree(&client->dev, ts_data);

    FTS_FUNC_EXIT();
    return 0;
}

/*****************************************************************************
*  Name: fts_ts_suspend
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_ts_suspend(struct device *dev)
{
    int ret = 0;
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    FTS_FUNC_ENTER();
    if (ts_data->suspended) {
        FTS_INFO("Already in suspend state");
        return 0;
    }

    if (ts_data->fw_loading) {
        FTS_INFO("fw upgrade in process, can't suspend");
        return 0;
    }

#if FTS_ESDCHECK_EN
    fts_esdcheck_suspend();
#endif

#if FTS_GESTURE_EN
	if (enable_gesture_mode) {
		if (fts_gesture_suspend(ts_data->client) == 0) {
			ts_data->suspended = true;
			fts_suspend_stats = true;   /* modify by zhangchaofan@longcheer.com for tp_gesture, 2018-12-25 */
			fts_release_all_finger();
			return 0;
		}
	}
#endif

    fts_irq_disable();

#if FTS_POWER_SOURCE_CUST_EN
    ret = fts_power_source_ctrl(ts_data, DISABLE);
    if (ret < 0) {
        FTS_ERROR("power off fail, ret=%d", ret);
    }
#if FTS_PINCTRL_EN
    fts_pinctrl_select_suspend(ts_data);
#endif
#else
    /* TP enter sleep mode */
    ret = fts_i2c_write_reg(ts_data->client, FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP_VALUE);
    if (ret < 0)
        FTS_ERROR("set TP to sleep mode fail, ret=%d", ret);
#endif
    gpio_direction_output(fts_data->pdata->reset_gpio, 0);
    ts_data->suspended = true;
    fts_suspend_stats = true;   /* modify by zhangchaofan@longcheer.com for tp_gesture, 2018-12-25 */
	fts_release_all_finger();
    FTS_FUNC_EXIT();
    return 0;
}

/*****************************************************************************
*  Name: fts_ts_resume
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_ts_resume(struct device *dev)
{
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    FTS_FUNC_ENTER();
    if (!ts_data->suspended) {
        FTS_DEBUG("Already in awake state");
        return 0;
    }

    fts_release_all_finger();

#if FTS_POWER_SOURCE_CUST_EN
    fts_power_source_ctrl(ts_data, ENABLE);
#if FTS_PINCTRL_EN
    fts_pinctrl_select_normal(ts_data);
#endif
#endif

    if (!ts_data->ic_info.is_incell) {
        fts_reset_proc(200);
    }
    gpio_direction_output(fts_data->pdata->reset_gpio, 1);
    fts_tp_state_recovery(ts_data->client);

#if FTS_ESDCHECK_EN
    fts_esdcheck_resume();
#endif

#if FTS_GESTURE_EN
	if(enable_gesture_mode) {
		if (fts_gesture_resume(ts_data->client) == 0) {
			ts_data->suspended = false;
			fts_suspend_stats = false;	/* modify by zhangchaofan@longcheer.com for tp_gesture, 2018-12-25 */
			return 0;
		}
	}

	if (fts_delay_gesture && !enable_gesture_mode) {
		if (fts_gesture_resume(ts_data->client) == 0) {
			ts_data->suspended = false;
			fts_suspend_stats = false;
			fts_delay_gesture = false;
			return 0;
		}
	}

#endif

	if (fts_delay_gesture) {
		enable_gesture_mode = !enable_gesture_mode;
	}

	if (!enable_gesture_mode){
		fts_irq_enable();
	}

	if (fts_delay_gesture ) {
		enable_gesture_mode = !enable_gesture_mode;
	}
    ts_data->suspended = false;

/* modify begin by zhangchaofan@longcheer.com for tp_gesture, 2018-12-25 */
    fts_suspend_stats = false;
	fts_delay_gesture = false;
/* modify end by zhangchaofan@longcheer.com for tp_gesture, 2018-12-25 */
    FTS_FUNC_EXIT();
    return 0;
}

/*****************************************************************************
* I2C Driver
*****************************************************************************/
static const struct i2c_device_id fts_ts_id[] = {
    {FTS_DRIVER_NAME, 0},
    {},
};
MODULE_DEVICE_TABLE(i2c, fts_ts_id);

static struct of_device_id fts_match_table[] = {
    { .compatible = "focaltech,fts", },
    { },
};

static struct i2c_driver fts_ts_driver = {
    .probe = fts_ts_probe,
    .remove = fts_ts_remove,
    .driver = {
        .name = FTS_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = fts_match_table,
    },
    .id_table = fts_ts_id,
};

/*****************************************************************************
*  Name: fts_ts_init
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int __init fts_ts_init(void)
{
    int ret = 0;

    FTS_FUNC_ENTER();
    /* add verify LCD by zhangchaofan start, 2018-11-20*/
	if (IS_ERR_OR_NULL(g_lcd_id)){
		FTS_ERROR("g_lcd_id is ERROR!\n");
		goto err_lcd;
	} else {
        if (strstr(g_lcd_id,"ft8719 video mode dsi tianma panel") != NULL) {
			FTS_INFO("LCM is right! [Vendor]tianma [IC]ft8719\n");
		} else if (strstr(g_lcd_id,"nt36672a video mode dsi shenchao panel") != NULL) {
			FTS_ERROR("LCM is right! [Vendor]shenchao [IC] nt36672a\n");
            goto err_lcd;
		} else {
			FTS_ERROR("Unknown LCM!\n");
			goto err_lcd;
		}
	}
    /* add verify LCD by zhangchaofan end, 2018-11-20*/
    ret = i2c_add_driver(&fts_ts_driver);
    if ( ret != 0 ) {
        FTS_ERROR("Focaltech touch screen driver init failed!");
    }
    FTS_FUNC_EXIT();
/* add verify LCD by zhangchaofan start, 2018-11-20*/

err_lcd:
    ret = -ENODEV;

/* add verify LCD by zhangchaofan end, 2018-11-20*/
    return ret;
}

/*****************************************************************************
*  Name: fts_ts_exit
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void __exit fts_ts_exit(void)
{
    i2c_del_driver(&fts_ts_driver);
}

module_init(fts_ts_init);
module_exit(fts_ts_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver");
MODULE_LICENSE("GPL v2");
