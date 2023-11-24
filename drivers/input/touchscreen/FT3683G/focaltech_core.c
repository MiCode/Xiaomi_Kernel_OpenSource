/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, FocalTech Systems, Ltd., all rights reserved.
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
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#if IS_ENABLED(CONFIG_DRM)
#if IS_ENABLED(CONFIG_DRM_PANEL)
#include <drm/drm_panel.h>
#else
#include <linux/msm_drm_notify.h>
#endif //CONFIG_DRM_PANEL

#elif IS_ENABLED(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif //CONFIG_DRM
#include "focaltech_core.h"

#include "../../../gpu/drm/mediatek/mediatek_v2/mtk_disp_notify.h"

/* N17 code for HQ-290835 by liunianliang at 2023/6/12 start */
#include "../xiaomi/xiaomi_touch.h"
/* N17 code for HQ-290835 by liunianliang at 2023/6/12 end */

/* N17 code for HQ-301859 by liunianliang at 2023/06/30 start */
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
#include "../../../gpu/drm/mediatek/mediatek_v2/mi_disp/mi_disp_notifier.h"
#endif
/* N17 code for HQ-301859 by liunianliang at 2023/06/30 end */

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_DRIVER_PEN_NAME                 "fts_ts,pen"
#define INTERVAL_READ_REG                   200  /* unit:ms */
#define INTERVAL_READ_REG_RESUME            50  /* unit:ms */
#define TIMEOUT_READ_REG                    1000 /* unit:ms */
#if FTS_POWER_SOURCE_CUST_EN
#define FTS_VTG_MIN_UV                      3300000
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
static int fts_ts_suspend(struct device *dev);
static int fts_ts_resume(struct device *dev);
/* N17 code for HQ-322975 by xionglei6 at 2023/8/30 start */
bool fts_gestures_status = false;
/* N17 code for HQ-322975 by xionglei6 at 2023/8/30 end */

/* N17 code for HQ-290835 by liunianliang at 2023/6/12 start */
static struct xiaomi_touch_interface xiaomi_touch_interfaces;
static int fts_read_palm_data(void);
static int fts_palm_sensor_cmd(int value);
static void fts_palm_mode_recovery(struct fts_ts_data *ts_data);
/* N17 code for HQ-290835 by liunianliang at 2023/6/12 end */

/* N17 code for HQ-299546 by liunianliang at 2023/6/13 start */
static void fts_game_mode_recovery(struct fts_ts_data *ts_data);
/* N17 code for HQ-299546 by liunianliang at 2023/6/13 end */

int fts_check_cid(struct fts_ts_data *ts_data, u8 id_h)
{
    int i = 0;
    struct ft_chip_id_t *cid = &ts_data->ic_info.cid;
    u8 cid_h = 0x0;

    if (cid->type == 0)
        return -ENODATA;

    for (i = 0; i < FTS_MAX_CHIP_IDS; i++) {
        cid_h = ((cid->chip_ids[i] >> 8) & 0x00FF);
        if (cid_h && (id_h == cid_h)) {
            return 0;
        }
    }

    return -ENODATA;
}

/*****************************************************************************
*  Name: fts_wait_tp_to_valid
*  Brief: Read chip id until TP FW become valid(Timeout: TIMEOUT_READ_REG),
*         need call when reset/power on/resume...
*  Input:
*  Output:
*  Return: return 0 if tp valid, otherwise return error code
*****************************************************************************/
int fts_wait_tp_to_valid(void)
{
    int ret = 0;
    int cnt = 0;
    u8 idh = 0;
    struct fts_ts_data *ts_data = fts_data;
    u8 chip_idh = ts_data->ic_info.ids.chip_idh;

    do {
        ret = fts_read_reg(FTS_REG_CHIP_ID, &idh);
        if ((idh == chip_idh) || (fts_check_cid(ts_data, idh) == 0)) {
            FTS_INFO("TP Ready,Device ID:0x%02x", idh);
            return 0;
        } else
            FTS_DEBUG("TP Not Ready,ReadData:0x%02x,ret:%d", idh, ret);

        cnt++;
        msleep(INTERVAL_READ_REG_RESUME);
    } while ((cnt * INTERVAL_READ_REG_RESUME) < TIMEOUT_READ_REG);

    return -EIO;
}

/*****************************************************************************
*  Name: fts_tp_state_recovery
*  Brief: Need execute this function when reset
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_tp_state_recovery(struct fts_ts_data *ts_data)
{
    FTS_FUNC_ENTER();
    /* wait tp stable */
    fts_wait_tp_to_valid();
    /* recover TP charger state 0x8B */
    /* recover TP glove state 0xC0 */
    /* recover TP cover state 0xC1 */
    fts_ex_mode_recovery(ts_data);
    /* recover TP gesture state 0xD0 */
    fts_gesture_recovery(ts_data);
    /* N17 code for HQ-290835 by liunianliang at 2023/6/12 start */
    fts_palm_mode_recovery(ts_data);
    /* N17 code for HQ-290835 by liunianliang at 2023/6/12 end */

    /* N17 code for HQ-299546 by liunianliang at 2023/6/13 start */
    fts_game_mode_recovery(ts_data);
    /* N17 code for HQ-299546 by liunianliang at 2023/6/13 end */
    FTS_FUNC_EXIT();
}

int fts_reset_proc(int hdelayms)
{
    FTS_DEBUG("tp reset");

    /* N17 code for HQ-299560 by zhangzhijian5 at 2023/8/16 start */
    fts_write_reg(FTS_REG_IDE_PARA_STATUS, FTS_REG_IDE_PARA_STATUS_EN);
    msleep(20);
    /* N17 code for HQ-299560 by zhangzhijian5 at 2023/8/16 end */

    gpio_direction_output(fts_data->pdata->reset_gpio, 0);
    msleep(1);
    gpio_direction_output(fts_data->pdata->reset_gpio, 1);
    if (hdelayms) {
        msleep(hdelayms);
    }

    return 0;
}

/* N17 code for HQ-299560 by zhangzhijian5 at 2023/8/16 start */
int fts_reset_proc_extend(int hdelayms)
{
    FTS_DEBUG("Set reset gpio to hight");

    gpio_direction_output(fts_data->pdata->reset_gpio, 1);
    if (hdelayms) {
        msleep(hdelayms);
    }

    return 0;
}
/* N17 code for HQ-299560 by zhangzhijian5 at 2023/8/16 end */

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

void fts_hid2std(void)
{
    int ret = 0;
    u8 buf[3] = {0xEB, 0xAA, 0x09};

    if (fts_data->bus_type != BUS_TYPE_I2C)
        return;

    ret = fts_write(buf, 3);
    if (ret < 0) {
        FTS_ERROR("hid2std cmd write fail");
    } else {
        msleep(10);
        buf[0] = buf[1] = buf[2] = 0;
        ret = fts_read(NULL, 0, buf, 3);
        if (ret < 0) {
            FTS_ERROR("hid2std cmd read fail");
        } else if ((0xEB == buf[0]) && (0xAA == buf[1]) && (0x08 == buf[2])) {
            FTS_DEBUG("hidi2c change to stdi2c successful");
        } else {
            FTS_DEBUG("hidi2c change to stdi2c not support or fail");
        }
    }
}

static int fts_match_cid(struct fts_ts_data *ts_data,
                         u16 type, u8 id_h, u8 id_l, bool force)
{
#ifdef FTS_CHIP_ID_MAPPING
    u32 i = 0;
    u32 j = 0;
    struct ft_chip_id_t chip_id_list[] = FTS_CHIP_ID_MAPPING;
    u32 cid_entries = sizeof(chip_id_list) / sizeof(struct ft_chip_id_t);
    u16 id = (id_h << 8) + id_l;

    memset(&ts_data->ic_info.cid, 0, sizeof(struct ft_chip_id_t));
    for (i = 0; i < cid_entries; i++) {
        if (!force && (type == chip_id_list[i].type)) {
            break;
        } else if (force && (type == chip_id_list[i].type)) {
            FTS_INFO("match cid,type:0x%x", (int)chip_id_list[i].type);
            ts_data->ic_info.cid = chip_id_list[i];
            return 0;
        }
    }

    if (i >= cid_entries) {
        return -ENODATA;
    }

    for (j = 0; j < FTS_MAX_CHIP_IDS; j++) {
        if (id == chip_id_list[i].chip_ids[j]) {
            FTS_DEBUG("cid:%x==%x", id, chip_id_list[i].chip_ids[j]);
            FTS_INFO("match cid,type:0x%x", (int)chip_id_list[i].type);
            ts_data->ic_info.cid = chip_id_list[i];
            return 0;
        }
    }

    return -ENODATA;
#else
    return -EINVAL;
#endif
}


static int fts_get_chip_types(
    struct fts_ts_data *ts_data,
    u8 id_h, u8 id_l, bool fw_valid)
{
    u32 i = 0;
    struct ft_chip_t ctype[] = FTS_CHIP_TYPE_MAPPING;
    u32 ctype_entries = sizeof(ctype) / sizeof(struct ft_chip_t);

    if ((0x0 == id_h) || (0x0 == id_l)) {
        FTS_ERROR("id_h/id_l is 0");
        return -EINVAL;
    }

    FTS_INFO("verify id:0x%02x%02x", id_h, id_l);
    for (i = 0; i < ctype_entries; i++) {
        if (VALID == fw_valid) {
            if (((id_h == ctype[i].chip_idh) && (id_l == ctype[i].chip_idl))
                || (!fts_match_cid(ts_data, ctype[i].type, id_h, id_l, 0)))
                break;
        } else {
            if (((id_h == ctype[i].rom_idh) && (id_l == ctype[i].rom_idl))
                || ((id_h == ctype[i].pb_idh) && (id_l == ctype[i].pb_idl))
                || ((id_h == ctype[i].bl_idh) && (id_l == ctype[i].bl_idl))) {
                break;
            }
        }
    }

    if (i >= ctype_entries) {
        return -ENODATA;
    }

    fts_match_cid(ts_data, ctype[i].type, id_h, id_l, 1);
    ts_data->ic_info.ids = ctype[i];
    return 0;
}

static int fts_read_bootid(struct fts_ts_data *ts_data, u8 *id)
{
    int ret = 0;
    u8 chip_id[2] = { 0 };
    u8 id_cmd[4] = { 0 };
    u32 id_cmd_len = 0;

    id_cmd[0] = FTS_CMD_START1;
    id_cmd[1] = FTS_CMD_START2;
    ret = fts_write(id_cmd, 2);
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
    ret = fts_read(id_cmd, id_cmd_len, chip_id, 2);
    if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
        FTS_ERROR("read boot id fail,read:0x%02x%02x", chip_id[0], chip_id[1]);
        return -EIO;
    }

    id[0] = chip_id[0];
    id[1] = chip_id[1];
    return 0;
}

/*****************************************************************************
* Name: fts_get_ic_information
* Brief: read chip id to get ic information, after run the function, driver w-
*        ill know which IC is it.
*        If cant get the ic information, maybe not focaltech's touch IC, need
*        unregister the driver
* Input:
* Output:
* Return: return 0 if get correct ic information, otherwise return error code
*****************************************************************************/
static int fts_get_ic_information(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int cnt = 0;
    u8 chip_id[2] = { 0 };

    ts_data->ic_info.is_incell = FTS_CHIP_IDC;
    ts_data->ic_info.hid_supported = FTS_HID_SUPPORTTED;


    do {
        ret = fts_read_reg(FTS_REG_CHIP_ID, &chip_id[0]);
        ret = fts_read_reg(FTS_REG_CHIP_ID2, &chip_id[1]);
        if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
            FTS_DEBUG("chip id read invalid, read:0x%02x%02x",
                      chip_id[0], chip_id[1]);
        } else {
            ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], VALID);
            if (!ret)
                break;
            else
                FTS_DEBUG("TP not ready, read:0x%02x%02x",
                          chip_id[0], chip_id[1]);
        }

        cnt++;
        msleep(INTERVAL_READ_REG);
    } while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

    if ((cnt * INTERVAL_READ_REG) >= TIMEOUT_READ_REG) {
        for (cnt = 0; cnt < 3; cnt++) {
            FTS_INFO("fw is invalid, need read boot id");

            if (cnt >= 1) {
                fts_reset_proc(0);
                mdelay(FTS_CMD_START_DELAY);
            }
            if (ts_data->ic_info.hid_supported) {
                fts_hid2std();
            }

            ret = fts_read_bootid(ts_data, &chip_id[0]);
            if (ret <  0) {
                FTS_ERROR("read boot id fail");
                continue;
            }

            ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], INVALID);
            if (ret < 0) {
                FTS_ERROR("can't get ic informaton");
                continue;
            }
            break;

        }
    }

    FTS_INFO("get ic information, chip id = 0x%02x%02x(cid type=0x%x)",
             ts_data->ic_info.ids.chip_idh, ts_data->ic_info.ids.chip_idl,
             ts_data->ic_info.cid.type);

    return ret;
}

/*****************************************************************************
*  Reprot related
*****************************************************************************/
static void fts_show_touch_buffer(u8 *data, u32 datalen)
{
    u32 i = 0;
    u32 count = 0;
    char *tmpbuf = NULL;

    tmpbuf = kzalloc(1024, GFP_KERNEL);
    if (!tmpbuf) {
        FTS_ERROR("tmpbuf zalloc fail");
        return;
    }

    for (i = 0; i < datalen; i++) {
        count += snprintf(tmpbuf + count, 1024 - count, "%02X,", data[i]);
        if (count >= 1024)
            break;
    }
    FTS_DEBUG("touch_buf:%s", tmpbuf);

    if (tmpbuf) {
        kfree(tmpbuf);
        tmpbuf = NULL;
    }
}

void fts_release_all_finger(void)
{
    struct fts_ts_data *ts_data = fts_data;
    struct input_dev *input_dev = ts_data->input_dev;
#if FTS_MT_PROTOCOL_B_EN
    u32 finger_count = 0;
    u32 max_touches = ts_data->pdata->max_touch_number;
#endif

    mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
    for (finger_count = 0; finger_count < max_touches; finger_count++) {
        input_mt_slot(input_dev, finger_count);
        input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
        /* N17 code for HQ-299546 by liunianliang at 2023/6/13 start */
        last_touch_events_collect(finger_count, 0);
        /* N17 code for HQ-299546 by liunianliang at 2023/6/13 end */
    }
#else
    input_mt_sync(input_dev);
#endif
    input_report_key(input_dev, BTN_TOUCH, 0);
    input_sync(input_dev);

#if FTS_PEN_EN
    input_report_key(ts_data->pen_dev, BTN_TOOL_PEN, 0);
    input_report_key(ts_data->pen_dev, BTN_TOUCH, 0);
    input_sync(ts_data->pen_dev);
#endif

    ts_data->touch_points = 0;
    ts_data->key_state = 0;
    mutex_unlock(&ts_data->report_mutex);
}

/*****************************************************************************
* Name: fts_input_report_key
* Brief: process key events,need report key-event if key enable.
*        if point's coordinate is in (x_dim-50,y_dim-50) ~ (x_dim+50,y_dim+50),
*        need report it to key event.
*        x_dim: parse from dts, means key x_coordinate, dimension:+-50
*        y_dim: parse from dts, means key y_coordinate, dimension:+-50
* Input:
* Output:
* Return: return 0 if it's key event, otherwise return error code
*****************************************************************************/
static int fts_input_report_key(struct fts_ts_data *ts_data, struct ts_event *kevent)
{
    int i = 0;
    int x = kevent->x;
    int y = kevent->y;
    int *x_dim = &ts_data->pdata->key_x_coords[0];
    int *y_dim = &ts_data->pdata->key_y_coords[0];

    if (!ts_data->pdata->have_key) {
        return -EINVAL;
    }
    for (i = 0; i < ts_data->pdata->key_number; i++) {
        if ((x >= x_dim[i] - FTS_KEY_DIM) && (x <= x_dim[i] + FTS_KEY_DIM) &&
            (y >= y_dim[i] - FTS_KEY_DIM) && (y <= y_dim[i] + FTS_KEY_DIM)) {
            if (EVENT_DOWN(kevent->flag)
                && !(ts_data->key_state & (1 << i))) {
                input_report_key(ts_data->input_dev, ts_data->pdata->keys[i], 1);
                ts_data->key_state |= (1 << i);
                FTS_DEBUG("Key%d(%d,%d) DOWN!", i, x, y);
            } else if (EVENT_UP(kevent->flag)
                       && (ts_data->key_state & (1 << i))) {
                input_report_key(ts_data->input_dev, ts_data->pdata->keys[i], 0);
                ts_data->key_state &= ~(1 << i);
                FTS_DEBUG("Key%d(%d,%d) Up!", i, x, y);
            }
            return 0;
        }
    }
    return -EINVAL;
}

#if FTS_MT_PROTOCOL_B_EN
static int fts_input_report_b(struct fts_ts_data *ts_data, struct ts_event *events)
{
    int i = 0;
    int touch_down_point_cur = 0;
    int touch_point_pre = ts_data->touch_points;
    u32 max_touch_num = ts_data->pdata->max_touch_number;
    bool touch_event_coordinate = false;
    struct input_dev *input_dev = ts_data->input_dev;

    for (i = 0; i < ts_data->touch_event_num; i++) {
        if (fts_input_report_key(ts_data, &events[i]) == 0) {
            continue;
        }

        touch_event_coordinate = true;
        if (EVENT_DOWN(events[i].flag)) {
            input_mt_slot(input_dev, events[i].id);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
#if FTS_REPORT_PRESSURE_EN
            input_report_abs(input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);
            input_report_abs(input_dev, ABS_MT_TOUCH_MINOR, events[i].minor);

            input_report_abs(input_dev, ABS_MT_POSITION_X, events[i].x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, events[i].y);

            touch_down_point_cur |= (1 << events[i].id);
            touch_point_pre |= (1 << events[i].id);

            if ((ts_data->log_level >= 2) ||
                ((1 == ts_data->log_level) && (FTS_TOUCH_DOWN == events[i].flag))) {
                FTS_DEBUG("[B]P%d(%d, %d)[p:%d,tm:%d] DOWN!",
                          events[i].id, events[i].x, events[i].y,
                          events[i].p, events[i].area);
            }
            /* N17 code for HQ-299546 by liunianliang at 2023/6/13 start */
            last_touch_events_collect(events[i].id, 1);
            /* N17 code for HQ-299546 by liunianliang at 2023/6/13 end */
        } else {
            input_mt_slot(input_dev, events[i].id);
            input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
            touch_point_pre &= ~(1 << events[i].id);
            if (ts_data->log_level >= 1) FTS_DEBUG("[B]P%d UP!", events[i].id);
            /* N17 code for HQ-299546 by liunianliang at 2023/6/13 start */
            last_touch_events_collect(events[i].id, 0);
            /* N17 code for HQ-299546 by liunianliang at 2023/6/13 end */
        }
    }

    if (unlikely(touch_point_pre ^ touch_down_point_cur)) {
        for (i = 0; i < max_touch_num; i++)  {
            if ((1 << i) & (touch_point_pre ^ touch_down_point_cur)) {
                if (ts_data->log_level >= 1) FTS_DEBUG("[B]P%d UP!", i);
                input_mt_slot(input_dev, i);
                input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
                /* N17 code for HQ-299546 by liunianliang at 2023/6/13 start */
                last_touch_events_collect(i, 0);
                /* N17 code for HQ-299546 by liunianliang at 2023/6/13 end */
            }
        }
    }

    if (touch_down_point_cur)
        input_report_key(input_dev, BTN_TOUCH, 1);
    else if (touch_event_coordinate || ts_data->touch_points) {
        if (ts_data->touch_points && (ts_data->log_level >= 1))
            FTS_DEBUG("[B]Points All Up!");
        input_report_key(input_dev, BTN_TOUCH, 0);
    }

    ts_data->touch_points = touch_down_point_cur;
    input_sync(input_dev);
    return 0;
}
#else
static int fts_input_report_a(struct fts_ts_data *ts_data, struct ts_event *events)
{
    int i = 0;
    int touch_down_point_num_cur = 0;
    bool touch_event_coordinate = false;
    struct input_dev *input_dev = ts_data->input_dev;

    for (i = 0; i < ts_data->touch_event_num; i++) {
        if (fts_input_report_key(ts_data, &events[i]) == 0) {
            continue;
        }

        touch_event_coordinate = true;
        if (EVENT_DOWN(events[i].flag)) {
            input_report_abs(input_dev, ABS_MT_TRACKING_ID, events[i].id);
#if FTS_REPORT_PRESSURE_EN
            input_report_abs(input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);
            input_report_abs(input_dev, ABS_MT_TOUCH_MINOR, events[i].minor);

            input_report_abs(input_dev, ABS_MT_POSITION_X, events[i].x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, events[i].y);
            input_mt_sync(input_dev);

            touch_down_point_num_cur++;
            if ((ts_data->log_level >= 2) ||
                ((1 == ts_data->log_level) && (FTS_TOUCH_DOWN == events[i].flag))) {
                FTS_DEBUG("[A]P%d(%d, %d)[p:%d,tm:%d] DOWN!",
                          events[i].id, events[i].x, events[i].y,
                          events[i].p, events[i].area);
            }
        }
    }

    if (touch_down_point_num_cur)
        input_report_key(input_dev, BTN_TOUCH, 1);
    else if (touch_event_coordinate || ts_data->touch_points) {
        if (ts_data->touch_points && (ts_data->log_level >= 1))
            FTS_DEBUG("[A]Points All Up!");
        input_report_key(input_dev, BTN_TOUCH, 0);
        input_mt_sync(input_dev);
    }

    ts_data->touch_points = touch_down_point_num_cur;
    input_sync(input_dev);
    return 0;
}
#endif

#if FTS_PEN_EN
static int fts_input_pen_report(struct fts_ts_data *ts_data, u8 *pen_buf)
{
    struct input_dev *pen_dev = ts_data->pen_dev;
    struct pen_event *pevt = &ts_data->pevent;

    /*get information of stylus*/
    pevt->inrange = (pen_buf[2] & 0x20) ? 1 : 0;
    pevt->tip = (pen_buf[2] & 0x01) ? 1 : 0;
    pevt->flag = pen_buf[3] >> 6;
#if FTS_PEN_HIRES_EN
    pevt->id = 0;
    pevt->x = ((u32)((pen_buf[3] & 0x0F) << 12) + (pen_buf[4] << 4) + ((pen_buf[5] >> 4) & 0x0F));
    pevt->y = ((u32)((pen_buf[5] & 0x0F) << 12) + (pen_buf[6] << 4) + ((pen_buf[7] >> 4) & 0x0F));
    pevt->x = (pevt->x * FTS_PEN_HIRES_X ) / FTS_HI_RES_X_MAX;
    pevt->y = (pevt->y * FTS_PEN_HIRES_X ) / FTS_HI_RES_X_MAX;
#else
    pevt->id = pen_buf[5] >> 4;
    pevt->x = ((pen_buf[3] & 0x0F) << 8) + pen_buf[4];
    pevt->y = ((pen_buf[5] & 0x0F) << 8) + pen_buf[6];
#endif
    pevt->p = ((pen_buf[7] & 0x0F) << 8) + pen_buf[8];
    pevt->tilt_x = (short)((pen_buf[9] << 8) + pen_buf[10]);
    pevt->tilt_y = (short)((pen_buf[11] << 8) + pen_buf[12]);
    pevt->azimuth = ((pen_buf[13] << 8) + pen_buf[14]);
    pevt->tool_type = BTN_TOOL_PEN;

    input_report_key(pen_dev, BTN_STYLUS, !!(pen_buf[2] & 0x02));
    input_report_key(pen_dev, BTN_STYLUS2, !!(pen_buf[2] & 0x08));

    switch (ts_data->pen_etype) {
    case STYLUS_DEFAULT:
        if (pevt->tip && pevt->p) {
            if ((ts_data->log_level >= 2) || (!pevt->down))
                FTS_DEBUG("[PEN]x:%d,y:%d,p:%d,tip:%d,flag:%d,tilt:%d,%d DOWN",
                          pevt->x, pevt->y, pevt->p, pevt->tip, pevt->flag,
                          pevt->tilt_x, pevt->tilt_y);
            input_report_abs(pen_dev, ABS_X, pevt->x);
            input_report_abs(pen_dev, ABS_Y, pevt->y);
            input_report_abs(pen_dev, ABS_PRESSURE, pevt->p);
            input_report_abs(pen_dev, ABS_TILT_X, pevt->tilt_x);
            input_report_abs(pen_dev, ABS_TILT_Y, pevt->tilt_y);
            input_report_key(pen_dev, BTN_TOUCH, 1);
            input_report_key(pen_dev, BTN_TOOL_PEN, 1);
            pevt->down = 1;
        } else if (!pevt->tip && pevt->down) {
            FTS_DEBUG("[PEN]x:%d,y:%d,p:%d,tip:%d,flag:%d,tilt:%d,%d UP",
                      pevt->x, pevt->y, pevt->p, pevt->tip, pevt->flag,
                      pevt->tilt_x, pevt->tilt_y);
            input_report_abs(pen_dev, ABS_X, pevt->x);
            input_report_abs(pen_dev, ABS_Y, pevt->y);
            input_report_abs(pen_dev, ABS_PRESSURE, pevt->p);
            input_report_key(pen_dev, BTN_TOUCH, 0);
            input_report_key(pen_dev, BTN_TOOL_PEN, 0);
            pevt->down = 0;
        }
        input_sync(pen_dev);
        break;
    case STYLUS_HOVER:
        if (ts_data->log_level >= 1)
            FTS_DEBUG("[PEN][%02X]x:%d,y:%d,p:%d,tip:%d,flag:%d,tilt:%d,%d,%d",
                      pen_buf[2], pevt->x, pevt->y, pevt->p, pevt->tip,
                      pevt->flag, pevt->tilt_x, pevt->tilt_y, pevt->azimuth);
        input_report_abs(pen_dev, ABS_X, pevt->x);
        input_report_abs(pen_dev, ABS_Y, pevt->y);
        input_report_abs(pen_dev, ABS_Z, pevt->azimuth);
        input_report_abs(pen_dev, ABS_PRESSURE, pevt->p);
        input_report_abs(pen_dev, ABS_TILT_X, pevt->tilt_x);
        input_report_abs(pen_dev, ABS_TILT_Y, pevt->tilt_y);
        input_report_key(pen_dev, BTN_TOOL_PEN, EVENT_DOWN(pevt->flag));
        input_report_key(pen_dev, BTN_TOUCH, pevt->tip);
        input_sync(pen_dev);
        break;
    default:
        FTS_ERROR("Unknown stylus event");
        break;
    }

    return 0;
}
#endif

static int fts_read_touchdata_spi(struct fts_ts_data *ts_data, u8 *buf)
{
    int ret = 0;

    ts_data->touch_addr = 0x01;
    ret = fts_read(&ts_data->touch_addr, 1, buf, ts_data->touch_size);


    if (ret < 0) {
        FTS_ERROR("touch data(%x) abnormal,ret:%d", buf[1], ret);
        return ret;
    }

    /* N17 code for HQ-290835 by liunianliang at 2023/6/12 start */
    if (ts_data->palm_sensor_switch)
        fts_read_palm_data();
    /* N17 code for HQ-290835 by liunianliang at 2023/6/12 end */
    return 0;
}

static int fts_read_touchdata_i2c(struct fts_ts_data *ts_data, u8 *buf)
{
    int ret = 0;
    u32 touch_max_size = 0;
    u32 max_touch_num = ts_data->pdata->max_touch_number;
    u8 event = 0xFF;

    ts_data->touch_addr = 0x01;
    ret = fts_read(&ts_data->touch_addr, 1, buf, ts_data->touch_size);
    if (ret < 0) {
        FTS_ERROR("read touchdata fails,ret:%d", ret);
        return ret;
    }

    event = (buf[FTS_TOUCH_E_NUM] >> 4) & 0x0F;
    if (event == TOUCH_DEFAULT) {
        if (buf[ts_data->touch_size - 1] != 0xFF)
            touch_max_size = max_touch_num * FTS_ONE_TCH_LEN + 2;
    } else if (event == TOUCH_PROTOCOL_v2) {
        touch_max_size = (buf[FTS_TOUCH_E_NUM] & 0x0F) * FTS_ONE_TCH_LEN_V2 + 4;
    }
#if FTS_PEN_EN
    else if (event == TOUCH_PEN) {
        touch_max_size = FTS_SIZE_PEN;
        if (touch_max_size > ts_data->touch_size) {
            FTS_INFO("read next touch message of pen,size:%d-%d",
                     touch_max_size, ts_data->touch_size);
        }
    }
#endif
    else if (event == TOUCH_EXTRA_MSG) {
        touch_max_size = (buf[FTS_TOUCH_E_NUM] & 0x0F) * FTS_ONE_TCH_LEN + \
                         4 + ((buf[2] << 8) + buf[3]);
        if (touch_max_size > FTS_MAX_TOUCH_BUF)
            touch_max_size = FTS_MAX_TOUCH_BUF;
    }

    if (touch_max_size > ts_data->touch_size) {
        ts_data->ta_size = touch_max_size;
        ts_data->touch_addr += ts_data->touch_size;
        ret = fts_read(&ts_data->touch_addr, 1, buf + ts_data->touch_size, \
                       touch_max_size - ts_data->touch_size);
        if (ret < 0) {
            FTS_ERROR("read touchdata2 fails,ret:%d", ret);
            return ret;
        }
    }

    return 0;
}

static int fts_read_parse_touchdata(struct fts_ts_data *ts_data, u8 *touch_buf)
{
    int ret = 0;
    u8 gesture_en = 0xFF;

    memset(touch_buf, 0xFF, FTS_MAX_TOUCH_BUF);
    ts_data->ta_size = ts_data->touch_size;

    /*read touch data*/
    if (ts_data->bus_type == BUS_TYPE_SPI)
        ret = fts_read_touchdata_spi(ts_data, touch_buf);
    else if (ts_data->bus_type == BUS_TYPE_I2C)
        ret = fts_read_touchdata_i2c(ts_data, touch_buf);
    else FTS_ERROR("unknown bus type:%d", ts_data->bus_type);
    if (ret < 0) {
        FTS_ERROR("read touch data fails");
        return TOUCH_ERROR;
    }

    if (ts_data->log_level >= 3)
        fts_show_touch_buffer(touch_buf, ts_data->ta_size);

    if (ret)
        return TOUCH_IGNORE;

    /*gesture*/
    if (ts_data->suspended && ts_data->gesture_support) {
        ret = fts_read_reg(FTS_REG_GESTURE_EN, &gesture_en);
        if ((ret >= 0) && (gesture_en == ENABLE))
            return TOUCH_GESTURE;
        else
            FTS_DEBUG("gesture not enable in fw, don't process gesture");
    }

    if ((touch_buf[1] == 0xFF) && (touch_buf[2] == 0xFF)
        && (touch_buf[3] == 0xFF) && (touch_buf[4] == 0xFF)) {
        FTS_INFO("touch buff is 0xff, need recovery state");
        return TOUCH_FW_INIT;
    }

#if FTS_TOUCH_HIRES_EN
    if (TOUCH_DEFAULT == ((touch_buf[FTS_TOUCH_E_NUM] >> 4) & 0x0F)) {
        return TOUCH_DEFAULT_HI_RES;
    }
#endif

    return ((touch_buf[FTS_TOUCH_E_NUM] >> 4) & 0x0F);
}

static int fts_irq_read_report(struct fts_ts_data *ts_data)
{
    int i = 0;
    int max_touch_num = ts_data->pdata->max_touch_number;
    int touch_etype = 0;
    u8 event_num = 0;
    u8 finger_num = 0;
    u8 pointid = 0;
    u8 base = 0;
    u8 *touch_buf = ts_data->touch_buf;
    struct ts_event *events = ts_data->events;

    touch_etype = fts_read_parse_touchdata(ts_data, touch_buf);
    switch (touch_etype) {
    case TOUCH_DEFAULT:
        finger_num = touch_buf[FTS_TOUCH_E_NUM] & 0x0F;
        if (finger_num > max_touch_num) {
            FTS_ERROR("invalid point_num(%d)", finger_num);
            return -EIO;
        }

        for (i = 0; i < max_touch_num; i++) {
            base = FTS_ONE_TCH_LEN * i + 2;
            pointid = (touch_buf[FTS_TOUCH_OFF_ID_YH + base]) >> 4;
            if (pointid >= FTS_MAX_ID)
                break;
            else if (pointid >= max_touch_num) {
                FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
                return -EINVAL;
            }

            events[i].id = pointid;
            events[i].flag = touch_buf[FTS_TOUCH_OFF_E_XH + base] >> 6;
            events[i].x = ((touch_buf[FTS_TOUCH_OFF_E_XH + base] & 0x0F) << 8) \
                          + (touch_buf[FTS_TOUCH_OFF_XL + base] & 0xFF);
            events[i].y = ((touch_buf[FTS_TOUCH_OFF_ID_YH + base] & 0x0F) << 8) \
                          + (touch_buf[FTS_TOUCH_OFF_YL + base] & 0xFF);
            events[i].p =  touch_buf[FTS_TOUCH_OFF_PRE + base];
            events[i].area = touch_buf[FTS_TOUCH_OFF_AREA + base];
            if (events[i].p <= 0) events[i].p = 0x3F;
            if (events[i].area <= 0) events[i].area = 0x09;
            events[i].minor = events[i].area;

            event_num++;
            if (EVENT_DOWN(events[i].flag) && (finger_num == 0)) {
                FTS_INFO("abnormal touch data from fw");
                return -EIO;
            }
        }

        if (event_num == 0) {
            FTS_INFO("no touch point information(%02x)", touch_buf[2]);
            return -EIO;
        }
        ts_data->touch_event_num = event_num;

        mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
        fts_input_report_b(ts_data, events);
#else
        fts_input_report_a(ts_data, events);
#endif
        mutex_unlock(&ts_data->report_mutex);
        break;

#if FTS_PEN_EN
    case TOUCH_PEN:
        mutex_lock(&ts_data->report_mutex);
        fts_input_pen_report(ts_data, touch_buf);
        mutex_unlock(&ts_data->report_mutex);
        break;
#endif

#if FTS_TOUCH_HIRES_EN
    case TOUCH_DEFAULT_HI_RES:
        finger_num = touch_buf[FTS_TOUCH_E_NUM] & 0x0F;
        if (finger_num > max_touch_num) {
            FTS_ERROR("invalid point_num(%d)", finger_num);
            return -EIO;
        }

        for (i = 0; i < max_touch_num; i++) {
            base = FTS_ONE_TCH_LEN * i + 2;
            pointid = (touch_buf[FTS_TOUCH_OFF_ID_YH + base]) >> 4;
            if (pointid >= FTS_MAX_ID)
                break;
            else if (pointid >= max_touch_num) {
                FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
                return -EINVAL;
            }

            events[i].id = pointid;
            events[i].flag = touch_buf[FTS_TOUCH_OFF_E_XH + base] >> 6;
            events[i].x = ((touch_buf[FTS_TOUCH_OFF_E_XH + base] & 0x0F) << 12) \
                          + ((touch_buf[FTS_TOUCH_OFF_XL + base] & 0xFF) << 4) \
                          + ((touch_buf[FTS_TOUCH_OFF_PRE + base] >> 4) & 0x0F);
            events[i].y = ((touch_buf[FTS_TOUCH_OFF_ID_YH + base] & 0x0F) << 12) \
                          + ((touch_buf[FTS_TOUCH_OFF_YL + base] & 0xFF) << 4) \
                          + (touch_buf[FTS_TOUCH_OFF_PRE + base] & 0x0F);
            events[i].x = (events[i].x * FTS_TOUCH_HIRES_X ) / FTS_HI_RES_X_MAX;
            events[i].y = (events[i].y * FTS_TOUCH_HIRES_X ) / FTS_HI_RES_X_MAX;
            events[i].area = touch_buf[FTS_TOUCH_OFF_AREA + base];
            events[i].p = 0x3F;
            if (events[i].area <= 0) events[i].area = 0x09;
            events[i].minor = events[i].area;
#if FTS_REPORT_PRESSURE_EN
            FTS_ERROR("high solution project doesn't support FTS_REPORT_PRESSURE_EN");
#endif
            event_num++;
            if (EVENT_DOWN(events[i].flag) && (finger_num == 0)) {
                FTS_INFO("abnormal touch data from fw");
                return -EIO;
            }
        }

        if (event_num == 0) {
            FTS_INFO("no touch point information(%02x)", touch_buf[2]);
            return -EIO;
        }
        ts_data->touch_event_num = event_num;

        mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
        fts_input_report_b(ts_data, events);
#else
        fts_input_report_a(ts_data, events);
#endif
        mutex_unlock(&ts_data->report_mutex);
        break;
#endif

    case TOUCH_PROTOCOL_v2:
        event_num = touch_buf[FTS_TOUCH_E_NUM] & 0x0F;
        if (!event_num || (event_num > max_touch_num)) {
            FTS_ERROR("invalid touch event num(%d)", event_num);
            return -EIO;
        }

        ts_data->touch_event_num = event_num;

        for (i = 0; i < event_num; i++) {
            /* base = FTS_ONE_TCH_LEN_V2 * i + 4;
             pointid = (touch_buf[FTS_TOUCH_OFF_ID_YH + base]) >> 4;
             if (pointid >= FTS_MAX_ID)
                 break;
             else if (pointid >= max_touch_num) {
                 FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
                 return -EINVAL;
             }*/

            base = FTS_ONE_TCH_LEN_V2 * i + 4;
            pointid = (touch_buf[FTS_TOUCH_OFF_ID_YH + base]) >> 4;
            if (pointid >= max_touch_num) {
                FTS_ERROR("touch point ID(%d) beyond max_touch_number(%d)",
                          pointid, max_touch_num);
                return -EINVAL;
            }

            events[i].id = pointid;
            events[i].flag = touch_buf[FTS_TOUCH_OFF_E_XH + base] >> 6;

            events[i].x = ((touch_buf[FTS_TOUCH_OFF_E_XH + base] & 0x0F) << 12) \
                          + ((touch_buf[FTS_TOUCH_OFF_XL + base] & 0xFF) << 4) \
                          + ((touch_buf[FTS_TOUCH_OFF_PRE + base] >> 4) & 0x0F);

            events[i].y = ((touch_buf[FTS_TOUCH_OFF_ID_YH + base] & 0x0F) << 12) \
                          + ((touch_buf[FTS_TOUCH_OFF_YL + base] & 0xFF) << 4) \
                          + (touch_buf[FTS_TOUCH_OFF_PRE + base] & 0x0F);

            /* N17 code for HQ-305170 by liunianliang at 2023/07/08 start */
            events[i].x = events[i].x * 16 / FTS_HI_RES_X_MAX;
            events[i].y = events[i].y * 16 / FTS_HI_RES_X_MAX;
            /* N17 code for HQ-305170 by liunianliang at 2023/07/08 end */
            events[i].area = touch_buf[FTS_TOUCH_OFF_AREA + base];
            events[i].minor = touch_buf[FTS_TOUCH_OFF_MINOR + base];
            events[i].p = 0x3F;

            if (events[i].area <= 0) events[i].area = 0x09;
            if (events[i].minor <= 0) events[i].minor = 0x09;

        }

        mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
        fts_input_report_b(ts_data, events);
#else
        fts_input_report_a(ts_data, events);
#endif
        mutex_unlock(&ts_data->report_mutex);

        break;

    case TOUCH_EXTRA_MSG:
        if (!ts_data->touch_analysis_support) {
            FTS_ERROR("touch_analysis is disabled");
            return -EINVAL;
        }

        event_num = touch_buf[FTS_TOUCH_E_NUM] & 0x0F;
        if (!event_num || (event_num > max_touch_num)) {
            FTS_ERROR("invalid touch event num(%d)", event_num);
            return -EIO;
        }

        ts_data->touch_event_num = event_num;
        for (i = 0; i < event_num; i++) {
            base = FTS_ONE_TCH_LEN * i + 4;
            pointid = (touch_buf[FTS_TOUCH_OFF_ID_YH + base]) >> 4;
            if (pointid >= max_touch_num) {
                FTS_ERROR("touch point ID(%d) beyond max_touch_number(%d)",
                          pointid, max_touch_num);
                return -EINVAL;
            }

            events[i].id = pointid;
            events[i].flag = touch_buf[FTS_TOUCH_OFF_E_XH + base] >> 6;
            events[i].x = ((touch_buf[FTS_TOUCH_OFF_E_XH + base] & 0x0F) << 8) \
                          + (touch_buf[FTS_TOUCH_OFF_XL + base] & 0xFF);
            events[i].y = ((touch_buf[FTS_TOUCH_OFF_ID_YH + base] & 0x0F) << 8) \
                          + (touch_buf[FTS_TOUCH_OFF_YL + base] & 0xFF);
            events[i].p =  touch_buf[FTS_TOUCH_OFF_PRE + base];
            events[i].area = touch_buf[FTS_TOUCH_OFF_AREA + base];
            if (events[i].p <= 0) events[i].p = 0x3F;
            if (events[i].area <= 0) events[i].area = 0x09;
            events[i].minor = events[i].area;
        }

        mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
        fts_input_report_b(ts_data, events);
#else
        fts_input_report_a(ts_data, events);
#endif
        mutex_unlock(&ts_data->report_mutex);
        break;

    case TOUCH_GESTURE:
        if (0 == fts_gesture_readdata(ts_data, touch_buf)) {
            FTS_INFO("succuss to get gesture data in irq handler");
        }
        break;

    case TOUCH_FW_INIT:
        fts_release_all_finger();
        fts_tp_state_recovery(ts_data);
        break;

    case TOUCH_IGNORE:
    case TOUCH_ERROR:
        break;

    default:
        FTS_INFO("unknown touch event(%d)", touch_etype);
        break;
    }

    return 0;
}

static irqreturn_t fts_irq_handler(int irq, void *data)
{
    struct fts_ts_data *ts_data = fts_data;
#if IS_ENABLED(CONFIG_PM) && FTS_PATCH_COMERR_PM
    int ret = 0;

    if ((ts_data->suspended) && (ts_data->pm_suspend)) {
        ret = wait_for_completion_timeout(
                  &ts_data->pm_completion,
                  msecs_to_jiffies(FTS_TIMEOUT_COMERR_PM));
        if (!ret) {
            FTS_ERROR("Bus don't resume from pm(deep),timeout,skip irq");
            return IRQ_HANDLED;
        }
    }
#endif


    ts_data->intr_jiffies = jiffies;
    fts_prc_queue_work(ts_data);
    fts_irq_read_report(ts_data);
    if (ts_data->touch_analysis_support && ts_data->ta_flag) {
        ts_data->ta_flag = 0;
        if (ts_data->ta_buf && ts_data->ta_size)
            memcpy(ts_data->ta_buf, ts_data->touch_buf, ts_data->ta_size);
        wake_up_interruptible(&ts_data->ts_waitqueue);
    }

    return IRQ_HANDLED;
}

static int fts_irq_registration(struct fts_ts_data *ts_data)
{
    int ret = 0;
    struct fts_ts_platform_data *pdata = ts_data->pdata;

    ts_data->irq = gpio_to_irq(pdata->irq_gpio);
    pdata->irq_gpio_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
    FTS_INFO("irq:%d, flag:%x", ts_data->irq, pdata->irq_gpio_flags);
    ret = request_threaded_irq(ts_data->irq, NULL, fts_irq_handler,
                               pdata->irq_gpio_flags,
                               FTS_DRIVER_NAME, ts_data);

    return ret;
}

#if FTS_PEN_EN
static int fts_input_pen_init(struct fts_ts_data *ts_data)
{
    int ret = 0;
    struct input_dev *pen_dev;
    struct fts_ts_platform_data *pdata = ts_data->pdata;
    u32 pen_x_max = pdata->x_max;
    u32 pen_y_max = pdata->y_max;

    FTS_FUNC_ENTER();
    pen_dev = input_allocate_device();
    if (!pen_dev) {
        FTS_ERROR("Failed to allocate memory for input_pen device");
        return -ENOMEM;
    }

#if FTS_PEN_HIRES_EN
    pen_x_max = (pdata->x_max + 1) * FTS_PEN_HIRES_X - 1;
    pen_y_max = (pdata->y_max + 1) * FTS_PEN_HIRES_X - 1;
#endif
    pen_dev->dev.parent = ts_data->dev;
    pen_dev->name = FTS_DRIVER_PEN_NAME;
    pen_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
    __set_bit(ABS_X, pen_dev->absbit);
    __set_bit(ABS_Y, pen_dev->absbit);
    __set_bit(BTN_STYLUS, pen_dev->keybit);
    __set_bit(BTN_STYLUS2, pen_dev->keybit);
    __set_bit(BTN_TOUCH, pen_dev->keybit);
    __set_bit(BTN_TOOL_PEN, pen_dev->keybit);
    __set_bit(INPUT_PROP_DIRECT, pen_dev->propbit);
    input_set_abs_params(pen_dev, ABS_X, pdata->x_min, pen_x_max, 0, 0);
    input_set_abs_params(pen_dev, ABS_Y, pdata->y_min, pen_y_max, 0, 0);
    input_set_abs_params(pen_dev, ABS_PRESSURE, 0, 4096, 0, 0);
    input_set_abs_params(pen_dev, ABS_TILT_X, -9000, 9000, 0, 0);
    input_set_abs_params(pen_dev, ABS_TILT_Y, -9000, 9000, 0, 0);
    input_set_abs_params(pen_dev, ABS_Z, 0, 36000, 0, 0);

    ret = input_register_device(pen_dev);
    if (ret) {
        FTS_ERROR("Input device registration failed");
        input_free_device(pen_dev);
        pen_dev = NULL;
        return ret;
    }

    ts_data->pen_dev = pen_dev;
    ts_data->pen_etype = STYLUS_DEFAULT;
    FTS_FUNC_EXIT();
    return 0;
}
#endif

static int fts_input_init(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int key_num = 0;
    struct fts_ts_platform_data *pdata = ts_data->pdata;
    struct input_dev *input_dev;
    u32 touch_x_max = pdata->x_max;
    u32 touch_y_max = pdata->y_max;

    FTS_FUNC_ENTER();
    input_dev = input_allocate_device();
    if (!input_dev) {
        FTS_ERROR("Failed to allocate memory for input device");
        return -ENOMEM;
    }

    /* Init and register Input device */
    input_dev->name = FTS_DRIVER_NAME;
    if (ts_data->bus_type == BUS_TYPE_I2C)
        input_dev->id.bustype = BUS_I2C;
    else
        input_dev->id.bustype = BUS_SPI;
    input_dev->dev.parent = ts_data->dev;

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

    /* N17 code for HQ-290835 by liunianliang at 2023/6/12 start */
    input_set_capability(input_dev, EV_KEY, FTS_PALM_KEYCODE);
    /* N17 code for HQ-290835 by liunianliang at 2023/6/12 end */

    /* N17 code for HQ-290808 by liunianliang at 2023/6/19 start */
    input_set_capability(input_dev, EV_KEY, KEY_GOTO);
    /* N17 code for HQ-290808 by liunianliang at 2023/6/19 end */

#if FTS_TOUCH_HIRES_EN
    touch_x_max = (pdata->x_max + 1) * FTS_TOUCH_HIRES_X - 1;
    touch_y_max = (pdata->y_max + 1) * FTS_TOUCH_HIRES_X - 1;
#endif

#if FTS_MT_PROTOCOL_B_EN
    input_mt_init_slots(input_dev, pdata->max_touch_number, INPUT_MT_DIRECT);
#else
    input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 0x0F, 0, 0);
#endif
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min, touch_x_max, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min, touch_y_max, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
#if FTS_REPORT_PRESSURE_EN
    input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
#endif

    ret = input_register_device(input_dev);
    if (ret) {
        FTS_ERROR("Input device registration failed");
        input_set_drvdata(input_dev, NULL);
        input_free_device(input_dev);
        input_dev = NULL;
        return ret;
    }

#if FTS_PEN_EN
    ret = fts_input_pen_init(ts_data);
    if (ret) {
        FTS_ERROR("Input-pen device registration failed");
        input_set_drvdata(input_dev, NULL);
        input_free_device(input_dev);
        input_dev = NULL;
        return ret;
    }
#endif

    ts_data->input_dev = input_dev;
    FTS_FUNC_EXIT();
    return 0;
}

static int fts_buffer_init(struct fts_ts_data *ts_data)
{
    ts_data->touch_buf = (u8 *)kzalloc(FTS_MAX_TOUCH_BUF, GFP_KERNEL);
    if (!ts_data->touch_buf) {
        FTS_ERROR("failed to alloc memory for touch buf");
        return -ENOMEM;
    }

    if (ts_data->bus_type == BUS_TYPE_SPI)
        ts_data->touch_size = FTS_TOUCH_DATA_LEN_V2;
    else if (ts_data->bus_type == BUS_TYPE_I2C)
        ts_data->touch_size = FTS_SIZE_DEFAULT_V2;
    else FTS_ERROR("unknown bus type:%d", ts_data->bus_type);

    ts_data->touch_analysis_support = 0;
    ts_data->ta_flag = 0;
    ts_data->ta_size = 0;

    return 0;
}

#if FTS_POWER_SOURCE_CUST_EN
/*****************************************************************************
* Power Control
*****************************************************************************/
#if FTS_PINCTRL_EN
static int fts_pinctrl_init(struct fts_ts_data *ts)
{
    int ret = 0;

    ts->pinctrl = devm_pinctrl_get(ts->dev);
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

static int fts_power_source_ctrl(struct fts_ts_data *ts_data, int enable)
{
    int ret = 0;

    if (IS_ERR_OR_NULL(ts_data->vdd)) {
        FTS_ERROR("vdd is invalid");
        return -EINVAL;
    }

    FTS_FUNC_ENTER();
    if (enable) {
        if (ts_data->power_disabled) {
            FTS_DEBUG("regulator enable !");
            ret = regulator_enable(ts_data->vdd);
            if (ret) {
                FTS_ERROR("enable vdd regulator failed,ret=%d", ret);
            }

            //msleep(1);
            FTS_ERROR("ts_data->pdata->iovdd_gpio=%d", ts_data->pdata->iovdd_gpio);
            gpio_direction_output(ts_data->pdata->iovdd_gpio, 1);
            ret = regulator_enable(ts_data->iovdd);
            if (ret) {
                FTS_ERROR("enable iovdd regulator failed,ret=%d", ret);
            }

            msleep(1);
            gpio_direction_output(ts_data->pdata->reset_gpio, 1);

#if 0
            if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
                ret = regulator_enable(ts_data->vcc_i2c);
                if (ret) {
                    FTS_ERROR("enable vcc_i2c regulator failed,ret=%d", ret);
                }
            }
#endif
            ts_data->power_disabled = false;
        }
    } else {
        if (!ts_data->power_disabled) {
            FTS_DEBUG("regulator disable !");
            gpio_direction_output(ts_data->pdata->reset_gpio, 0);

            msleep(1);
            gpio_direction_output(ts_data->pdata->iovdd_gpio, 0);
            ret = regulator_disable(ts_data->iovdd);
            if (ret) {
                FTS_ERROR("disable iovdd regulator failed,ret=%d", ret);
            }

            msleep(1);
            ret = regulator_disable(ts_data->vdd);
            if (ret) {
                FTS_ERROR("disable vdd regulator failed,ret=%d", ret);
            }

#if 0
            if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
                ret = regulator_disable(ts_data->vcc_i2c);
                if (ret) {
                    FTS_ERROR("disable vcc_i2c regulator failed,ret=%d", ret);
                }
            }
#endif
            ts_data->power_disabled = true;
        }
    }

    FTS_FUNC_EXIT();
    return ret;
}

/*****************************************************************************
* Name: fts_power_source_init
* Brief: Init regulator power:vdd/vcc_io(if have), generally, no vcc_io
*        vdd---->vdd-supply in dts, kernel will auto add "-supply" to parse
*        Must be call after fts_gpio_configure() execute,because this function
*        will operate reset-gpio which request gpio in fts_gpio_configure()
* Input:
* Output:
* Return: return 0 if init power successfully, otherwise return error code
*****************************************************************************/
static int fts_power_source_init(struct fts_ts_data *ts_data)
{
    int ret = 0;

    FTS_FUNC_ENTER();
    ts_data->vdd = devm_regulator_get(ts_data->dev, ts_data->pdata->avdd_name);
    if (IS_ERR_OR_NULL(ts_data->vdd)) {
        ret = PTR_ERR(ts_data->vdd);
        FTS_ERROR("get vdd regulator failed,ret=%d", ret);
        return ret;
    }

    if (regulator_count_voltages(ts_data->vdd) > 0) {
        ret = regulator_set_voltage(ts_data->vdd, FTS_VTG_MIN_UV,
                                    FTS_VTG_MAX_UV);
        if (ret) {
            FTS_ERROR("vdd regulator set_vtg failed ret=%d", ret);
            regulator_put(ts_data->vdd);
            return ret;
        }
    }

    ts_data->iovdd = devm_regulator_get(ts_data->dev, ts_data->pdata->iovdd_name);
    if (IS_ERR_OR_NULL(ts_data->iovdd)) {
        ret = PTR_ERR(ts_data->iovdd);
        FTS_ERROR("get iovdd regulator failed,ret=%d", ret);
        ts_data->iovdd = NULL;
    }

#if 0
    ts_data->vcc_i2c = devm_regulator_get(ts_data->dev, "vcc_i2c");
    if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
        if (regulator_count_voltages(ts_data->vcc_i2c) > 0) {
            ret = regulator_set_voltage(ts_data->vcc_i2c,
                                        FTS_I2C_VTG_MIN_UV,
                                        FTS_I2C_VTG_MAX_UV);
            if (ret) {
                FTS_ERROR("vcc_i2c regulator set_vtg failed,ret=%d", ret);
                regulator_put(ts_data->vcc_i2c);
            }
        }
    }
#endif

#if FTS_PINCTRL_EN
    fts_pinctrl_init(ts_data);
    fts_pinctrl_select_normal(ts_data);
#endif

    ts_data->power_disabled = true;
    ret = fts_power_source_ctrl(ts_data, ENABLE);
    if (ret) {
        FTS_ERROR("fail to enable power(regulator)");
    }

    FTS_FUNC_EXIT();
    return ret;
}

static int fts_power_source_exit(struct fts_ts_data *ts_data)
{
#if FTS_PINCTRL_EN
    fts_pinctrl_select_release(ts_data);
#endif

    fts_power_source_ctrl(ts_data, DISABLE);

#if 0
    if (!IS_ERR_OR_NULL(ts_data->vdd)) {
        if (regulator_count_voltages(ts_data->vdd) > 0)
            regulator_set_voltage(ts_data->vdd, 0, FTS_VTG_MAX_UV);
        regulator_put(ts_data->vdd);
    }

    if (!IS_ERR_OR_NULL(ts_data->iovdd)) {
        if (regulator_count_voltages(ts_data->iovdd) > 0)
            regulator_set_voltage(ts_data->iovdd, 0, FTS_VTG_MAX_UV);
        regulator_put(ts_data->iovdd);
    }


    if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
        if (regulator_count_voltages(ts_data->vcc_i2c) > 0)
            regulator_set_voltage(ts_data->vcc_i2c, 0, FTS_I2C_VTG_MAX_UV);
        regulator_put(ts_data->vcc_i2c);
    }
#endif

    return 0;
}

/* N17 code for HQ-308632 by p-zhangzhijian5 at 2023/7/28 start */
#if 0
static int fts_power_source_suspend(struct fts_ts_data *ts_data)
{
    int ret = 0;

#if FTS_PINCTRL_EN
    fts_pinctrl_select_suspend(ts_data);
#endif

    ret = fts_power_source_ctrl(ts_data, DISABLE);
    if (ret < 0) {
        FTS_ERROR("power off fail, ret=%d", ret);
    }

    return ret;
}

static int fts_power_source_resume(struct fts_ts_data *ts_data)
{
    int ret = 0;

#if FTS_PINCTRL_EN
    fts_pinctrl_select_normal(ts_data);
#endif

    ret = fts_power_source_ctrl(ts_data, ENABLE);
    if (ret < 0) {
        FTS_ERROR("power on fail, ret=%d", ret);
    }

    return ret;
}
#endif
/* N17 code for HQ-308632 by p-zhangzhijian5 at 2023/7/28 end */

#endif /* FTS_POWER_SOURCE_CUST_EN */

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
            goto err_irq_gpio_req;
        }
#if 0
        ret = gpio_direction_output(data->pdata->reset_gpio, 1);
        if (ret) {
            FTS_ERROR("[GPIO]set_direction for reset gpio failed");
            goto err_reset_gpio_dir;
        }
#endif
    }

    /* request iovdd gpio */
    if (gpio_is_valid(data->pdata->iovdd_gpio)) {
        ret = gpio_request(data->pdata->iovdd_gpio, "fts_iovdd_gpio");
        if (ret) {
            FTS_ERROR("[GPIO]iovdd gpio request failed");
            goto err_irq_gpio_req;
        }
    }

    FTS_FUNC_EXIT();
    return 0;
#if 0
err_reset_gpio_dir:
    if (gpio_is_valid(data->pdata->reset_gpio))
        gpio_free(data->pdata->reset_gpio);
#endif
err_irq_gpio_dir:
    if (gpio_is_valid(data->pdata->irq_gpio))
        gpio_free(data->pdata->irq_gpio);
err_irq_gpio_req:
    FTS_FUNC_EXIT();
    return ret;
}

static int fts_bus_init(struct fts_ts_data *ts_data)
{
    FTS_FUNC_ENTER();
    ts_data->bus_tx_buf = kzalloc(FTS_MAX_BUS_BUF, GFP_KERNEL);
    if (NULL == ts_data->bus_tx_buf) {
        FTS_ERROR("failed to allocate memory for bus_tx_buf");
        return -ENOMEM;
    }

    ts_data->bus_rx_buf = kzalloc(FTS_MAX_BUS_BUF, GFP_KERNEL);
    if (NULL == ts_data->bus_rx_buf) {
        FTS_ERROR("failed to allocate memory for bus_rx_buf");
        return -ENOMEM;
    }

    FTS_FUNC_EXIT();
    return 0;
}


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
    if (ret < 0) {
        FTS_ERROR("Unable to read %s, please check dts", name);
        pdata->x_min = FTS_X_MIN_DISPLAY_DEFAULT;
        pdata->y_min = FTS_Y_MIN_DISPLAY_DEFAULT;
        pdata->x_max = FTS_X_MAX_DISPLAY_DEFAULT;
        pdata->y_max = FTS_Y_MAX_DISPLAY_DEFAULT;
        return -ENODATA;
    } else {
        pdata->x_min = coords[0];
        pdata->y_min = coords[1];
        pdata->x_max = coords[2];
        pdata->y_max = coords[3];
    }

    FTS_INFO("display x(%d %d) y(%d %d)", pdata->x_min, pdata->x_max,
             pdata->y_min, pdata->y_max);
    return 0;
}

static int fts_parse_dt(struct device *dev, struct fts_ts_platform_data *pdata)
{
    int ret = 0;
    struct device_node *np = dev->of_node;
    u32 temp_val = 0;
    const char *name_tmp;

    FTS_FUNC_ENTER();
    if (!np || !pdata) {
        FTS_ERROR("np/pdata is null");
        return -EINVAL;
    }

    ret = fts_get_dt_coords(dev, "focaltech,display-coords", pdata);
    if (ret < 0)
        FTS_ERROR("Unable to get display-coords");

    /* key */
    pdata->have_key = of_property_read_bool(np, "focaltech,have-key");
    if (pdata->have_key) {
        ret = of_property_read_u32(np, "focaltech,key-number", &pdata->key_number);
        if (ret < 0)
            FTS_ERROR("Key number undefined!");

        ret = of_property_read_u32_array(np, "focaltech,keys",
                                         pdata->keys, pdata->key_number);
        if (ret < 0)
            FTS_ERROR("Keys undefined!");
        else if (pdata->key_number > FTS_MAX_KEYS)
            pdata->key_number = FTS_MAX_KEYS;

        ret = of_property_read_u32_array(np, "focaltech,key-x-coords",
                                         pdata->key_x_coords,
                                         pdata->key_number);
        if (ret < 0)
            FTS_ERROR("Key Y Coords undefined!");

        ret = of_property_read_u32_array(np, "focaltech,key-y-coords",
                                         pdata->key_y_coords,
                                         pdata->key_number);
        if (ret < 0)
            FTS_ERROR("Key X Coords undefined!");

        FTS_INFO("VK Number:%d, key:(%d,%d,%d), "
                 "coords:(%d,%d),(%d,%d),(%d,%d)",
                 pdata->key_number,
                 pdata->keys[0], pdata->keys[1], pdata->keys[2],
                 pdata->key_x_coords[0], pdata->key_y_coords[0],
                 pdata->key_x_coords[1], pdata->key_y_coords[1],
                 pdata->key_x_coords[2], pdata->key_y_coords[2]);
    }

    /* reset, irq gpio info */
    pdata->reset_gpio = of_get_named_gpio_flags(np, "focaltech,reset-gpio",
                        0, &pdata->reset_gpio_flags);
    if (pdata->reset_gpio < 0)
        FTS_ERROR("Unable to get reset_gpio");

    pdata->irq_gpio = of_get_named_gpio_flags(np, "focaltech,irq-gpio",
                      0, &pdata->irq_gpio_flags);
    if (pdata->irq_gpio < 0)
        FTS_ERROR("Unable to get irq_gpio");

    pdata->iovdd_gpio = of_get_named_gpio_flags(np, "focaltech,iovdd-gpio",
                      0, &pdata->iovdd_gpio_flags);
    if (pdata->iovdd_gpio < 0)
        FTS_ERROR("Unable to get iovdd_gpio");

    memset(pdata->avdd_name, 0, sizeof(pdata->avdd_name));
    ret = of_property_read_string(np, "focaltech,avdd-name", &name_tmp);
    if (!ret) {
        FTS_INFO("avdd name form dt: %s", name_tmp);
        if (strlen(name_tmp) < sizeof(pdata->avdd_name))
            strncpy(pdata->avdd_name,
                name_tmp, sizeof(pdata->avdd_name));
        else
            FTS_ERROR("invalied avdd name length: %ld > %ld",
                strlen(name_tmp), sizeof(pdata->avdd_name));
    }

    memset(pdata->iovdd_name, 0, sizeof(pdata->iovdd_name));
    ret = of_property_read_string(np, "focaltech,iovdd-name", &name_tmp);
    if (!ret) {
        FTS_INFO("iovdd name form dt: %s", name_tmp);
        if (strlen(name_tmp) < sizeof(pdata->iovdd_name))
            strncpy(pdata->iovdd_name,
                name_tmp, sizeof(pdata->iovdd_name));
        else
            FTS_ERROR("invalied iovdd name length: %ld > %ld",
                strlen(name_tmp),
                sizeof(pdata->iovdd_name));
    }

    ret = of_property_read_u32(np, "focaltech,max-touch-number", &temp_val);
    if (ret < 0) {
        FTS_ERROR("Unable to get max-touch-number, please check dts");
        pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
    } else {
        if (temp_val < 2)
            pdata->max_touch_number = 2; /* max_touch_number must >= 2 */
        else if (temp_val > FTS_MAX_POINTS_SUPPORT)
            pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
        else
            pdata->max_touch_number = temp_val;
    }

    FTS_INFO("max touch number:%d, irq gpio:%d, reset gpio:%d, iovdd gpio:%d",
             pdata->max_touch_number, pdata->irq_gpio, pdata->reset_gpio, pdata->iovdd_gpio);

    /* N17 code for HQ-299546 by liunianliang at 2023/6/13 start */
    ret = of_property_read_u32_array(np, "focaltech,touch-def-array",
                        pdata->touch_def_array, 4);
    if (ret < 0) {
        FTS_ERROR("Unable to get touch default array, please check dts");
        return ret;
    }
    ret = of_property_read_u32_array(np, "focaltech,touch-range-array",
                        pdata->touch_range_array, 5);
    if (ret < 0) {
        FTS_ERROR("Unable to get touch range array, please check dts");
        return ret;
    }
    ret = of_property_read_u32_array(np, "focaltech,touch-expert-array",
                        pdata->touch_expert_array, 4 * EXPERT_ARRAY_SIZE);
    if (ret < 0) {
        FTS_ERROR("Unable to get touch expert array, please check dts");
        return ret;
    }
    /* N17 code for HQ-299546 by liunianliang at 2023/6/13 end */

    FTS_FUNC_EXIT();
    return 0;
}

static int fts_ts_suspend(struct device *dev)
{
    int ret = 0;
    struct fts_ts_data *ts_data = fts_data;

    FTS_FUNC_ENTER();
    if (ts_data->suspended) {
        FTS_INFO("Already in suspend state");
        return 0;
    }

    if (ts_data->fw_loading) {
        FTS_INFO("fw upgrade in process, can't suspend");
        return 0;
    }

    /* N17 code for HQ-290835 by liunianliang at 2023/6/12 start */
    if (ts_data->palm_sensor_switch) {
        FTS_INFO("palm sensor ON, switch to OFF");
        update_palm_sensor_value(0);
        fts_palm_sensor_cmd(0);
    }
    /* N17 code for HQ-290835 by liunianliang at 2023/6/12 end */

    fts_esdcheck_suspend(ts_data);

    /* N17 code for HQ-299546 by liunianliang at 2023/6/13 start */
    if (ts_data->gesture_cmd_delay) {
        ts_data->gesture_support = ts_data->gesture_status != 0 ? ENABLE : DISABLE;
        FTS_INFO("suspended gesture state:0x%02X, write cmd:0x%02X",
            ts_data->gesture_status, ts_data->gesture_cmd);
        ts_data->gesture_cmd_delay = false;
    }
    /* N17 code for HQ-299546 by liunianliang at 2023/6/13 end */

    /* N17 code for HQ-322975 by xionglei6 at 2023/8/30 start */
    if (ts_data->gesture_support) {
        fts_gesture_suspend(ts_data);
        fts_gestures_status = true;
    } else {
        fts_irq_disable();
        fts_gestures_status = false;
        FTS_INFO("make TP enter into sleep mode");
        ret = fts_write_reg(FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP);
        if (ret < 0)
            FTS_ERROR("set TP to sleep mode fail, ret=%d", ret);
    }
    /* N17 code for HQ-322975 by xionglei6 at 2023/8/30 end */

    fts_release_all_finger();
    ts_data->suspended = true;
    FTS_FUNC_EXIT();
    return 0;
}

static int fts_ts_resume(struct device *dev)
{
    /* N17 code for HQ-299560 by zhangzhijian5 at 2023/8/16 start */
    int i = 0;
    int retry_time = 20;
    u8 idh = 0;
    struct fts_ts_data *ts_data = fts_data;
    u8 chip_idh = ts_data->ic_info.ids.chip_idh;
    /* N17 code for HQ-299560 by zhangzhijian5 at 2023/8/16 end */

    FTS_FUNC_ENTER();
    if (!ts_data->suspended) {
        FTS_DEBUG("Already in awake state");
        return 0;
    }

    ts_data->suspended = false;
    fts_release_all_finger();

    /* N17 code for HQ-299560 by zhangzhijian5 at 2023/8/16 start */
    if (!ts_data->ic_info.is_incell)
        fts_reset_proc(40);

    for (i = 0; i < retry_time; i++) {
	fts_read_reg(FTS_REG_CHIP_ID, &idh);
	if ((idh == chip_idh) || (fts_check_cid(ts_data, idh) == 0))
	    break;
	msleep(10);
    }
    if (i >= retry_time)
	FTS_ERROR("Wait tp fw valid timeout, ReadDate: 0x%02x", idh);
    /* N17 code for HQ-299560 by zhangzhijian5 at 2023/8/16 end */

    fts_ex_mode_recovery(ts_data);

    fts_esdcheck_resume(ts_data);

    /* N17 code for HQ-290835 by liunianliang at 2023/6/12 start */
    if (ts_data->palm_sensor_switch) {
        fts_palm_sensor_cmd(1);
        FTS_INFO("palm sensor OFF, switch to ON");
    }
    /* N17 code for HQ-290835 by liunianliang at 2023/6/12 end */

    /* N17 code for HQ-322975 by xionglei6 at 2023/8/30 start */
    if (fts_gestures_status == true) {
        fts_gesture_resume(ts_data);
    } else {
        fts_irq_enable();
    }
    /* N17 code for HQ-322975 by xionglei6 at 2023/8/30 end */

    FTS_FUNC_EXIT();
    return 0;
}

static void fts_resume_work(struct work_struct *work)
{
    struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data, resume_work);
    fts_ts_resume(ts_data->dev);
}

/* N17 code for HQ-301859 by liunianliang at 2023/06/30 start */
static int fb_notifier_callback(struct notifier_block *nb,
                unsigned long val, void *data)
{
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
    struct fts_ts_data *ts_data = container_of(nb, struct fts_ts_data, fb_notif);
    struct mi_disp_notifier *evdata = data;
    unsigned int blank;

    FTS_FUNC_ENTER();

    if (!(val == MI_DISP_DPMS_EARLY_EVENT ||
          val == MI_DISP_DPMS_EVENT)) {
        FTS_ERROR("event(%lu) do not need process", val);
        return 0;
    }

    if (evdata && evdata->data && ts_data) {

        blank = *(int *)(evdata->data);
        FTS_ERROR("val:%lu,blank:%u", val, blank);

        if (val == MI_DISP_DPMS_EVENT
            && (blank == MI_DISP_DPMS_POWERDOWN
            || blank == MI_DISP_DPMS_LP1
            || blank == MI_DISP_DPMS_LP2)) {

            FTS_ERROR("FB_BLANK_POWERDOWN");

            cancel_work_sync(&fts_data->resume_work);
            fts_ts_suspend(ts_data->dev);
        } else if (val == MI_DISP_DPMS_EVENT && blank == MI_DISP_DPMS_ON) {

            FTS_ERROR("FB_BLANK_UNBLANK");

            flush_workqueue(fts_data->ts_workqueue);
            queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
        }
    }

    FTS_FUNC_EXIT();
#endif
    return 0;
}

static int fts_notifier_callback_init(struct fts_ts_data *ts_data)
{
    int ret = 0;
    FTS_FUNC_ENTER();

    ts_data->fb_notif.notifier_call = fb_notifier_callback;

#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
    mi_disp_register_client(&ts_data->fb_notif);
#endif

    FTS_FUNC_EXIT();
    return ret;
}
/* N17 code for HQ-301859 by liunianliang at 2023/06/30 end */

/* N17 code for HQ-290835 by liunianliang at 2023/6/12 start */
static int fts_read_palm_data(void)
{
    int ret = 0;
    u8 reg_value;

    if (fts_data == NULL)
        return -EINVAL;

    ret = fts_read_reg(FTS_PALM_DATA, &reg_value);
    if (ret < 0) {
        FTS_ERROR("read palm data error");
        return -EINVAL;
    }

    if (reg_value == 0x01) {
        update_palm_sensor_value(1);

        input_report_key(fts_data->input_dev, FTS_PALM_KEYCODE, 1);
        input_sync(fts_data->input_dev);
        input_report_key(fts_data->input_dev, FTS_PALM_KEYCODE, 0);
        input_sync(fts_data->input_dev);
    } else if (reg_value == 0x00) {
        update_palm_sensor_value(0);
    }

    if (reg_value == 0x01)
        FTS_INFO("update palm data:0x%02X", reg_value);

    return 0;
}

static int fts_palm_sensor_cmd(int value)
{
    int ret = 0;
    ret = fts_write_reg(FTS_PALM_EN, value ? FTS_PALM_ON : FTS_PALM_OFF);
    if (ret < 0)
        FTS_ERROR("Set palm sensor switch failed!");
    else
        FTS_INFO("Set palm sensor switch: %d", value);

    return ret;
}

static int fts_palm_sensor_write(int value)
{
    int ret = 0;

    if (fts_data == NULL)
        return -EINVAL;

    fts_data->palm_sensor_switch = value;
    if (fts_data->suspended)
        return 0;

    ret = fts_palm_sensor_cmd(value);
    if (ret < 0)
        FTS_ERROR("set palm sensor cmd failed: %d", value);

    return ret;
}

static void fts_palm_mode_recovery(struct fts_ts_data *ts_data)
{
    int ret = 0;
    ret = fts_palm_sensor_cmd(ts_data->palm_sensor_switch);

    if (ret < 0)
        FTS_ERROR("set palm sensor cmd failed: %d", ts_data->palm_sensor_switch);
}

/* N17 code for HQ-310258 by zhangzhijian5 at 2023/7/29 start */
static int fts_touch_hdle_mode_set(bool value)
{
    int ret = 0;

    ret = fts_write_reg(FTS_REG_HDLEMODE, value);
    if (ret >= 0)
        FTS_DEBUG("set hdle mode to %d successfully !", value);
    else
        FTS_ERROR("send hdle mode cmd failed : %d", value);

    return ret;
}
/* N17 code for HQ-310258 by zhangzhijian5 at 2023/7/29 end */

/* This is strange, but it's ok */
/* N17 code for HQ-299546 by liunianliang at 2023/6/13 start */
#include "focaltech_mi_custom.c"
/* N17 code for HQ-299546 by liunianliang at 2023/6/13 end */

static void fts_init_xiaomi_touchfeature(struct fts_ts_data *ts_data)
{
    mutex_init(&ts_data->cmd_update_mutex);
    memset(&xiaomi_touch_interfaces, 0x00, sizeof(struct xiaomi_touch_interface));
    xiaomi_touch_interfaces.palm_sensor_write = fts_palm_sensor_write;

    /* N17 code for HQ-299546 by liunianliang at 2023/6/13 start */
    xiaomi_touch_interfaces.getModeValue = fts_get_mode_value;
    xiaomi_touch_interfaces.setModeValue = fts_set_cur_value;
    xiaomi_touch_interfaces.resetMode = fts_reset_mode;
    xiaomi_touch_interfaces.getModeAll = fts_get_mode_all;
    fts_init_touchmode_data(ts_data);
    /* N17 code for HQ-299546 by liunianliang at 2023/6/13 end */

    /* N17 code for HQ-299728 by liunianliang at 2023/6/15 start */
    xiaomi_touch_interfaces.panel_vendor_read = fts_panel_vendor_read;
    xiaomi_touch_interfaces.panel_color_read = fts_panel_color_read;
    xiaomi_touch_interfaces.panel_display_read = fts_panel_display_read;
    xiaomi_touch_interfaces.touch_vendor_read = fts_touch_vendor_read;
    /* N17 code for HQ-299728 by liunianliang at 2023/6/15 end */

    /* N17 code for HQ-307700 by p-xionglei6 at 2023.07.24 start */
    xiaomi_touch_interfaces.touch_edge_mode_set = fts_touch_edge_mode_set;
    /* N17 code for HQ-307700 by p-xionglei6 at 2023.07.24 end */

    /* N17 code for HQ-310258 by zhangzhijian5 at 2023/7/29 start */
    xiaomi_touch_interfaces.touch_hdle_mode_set = fts_touch_hdle_mode_set;
    /* N17 code for HQ-310258 by zhangzhijian5 at 2023/7/29 end */

    xiaomitouch_register_modedata(0, &xiaomi_touch_interfaces);
}
/* N17 code for HQ-290835 by liunianliang at 2023/6/12 end */

/* N17 code for HQ-301859 by liunianliang at 2023/06/30 start */
static int fts_notifier_callback_exit(struct fts_ts_data *ts_data)
{
    FTS_FUNC_ENTER();

#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
    mi_disp_unregister_client(&ts_data->fb_notif);
#endif

    FTS_FUNC_EXIT();
    return 0;
}
/* N17 code for HQ-301859 by liunianliang at 2023/06/30 end */

int fts_ts_probe_entry(struct fts_ts_data *ts_data)
{
    int ret = 0;

    FTS_FUNC_ENTER();
    FTS_INFO("%s", FTS_DRIVER_VERSION);
    ts_data->pdata = kzalloc(sizeof(struct fts_ts_platform_data), GFP_KERNEL);
    if (!ts_data->pdata) {
        FTS_ERROR("allocate memory for platform_data fail");
        return -ENOMEM;
    }


    ret = fts_parse_dt(ts_data->dev, ts_data->pdata);
    if (ret) {
        FTS_ERROR("device-tree parse fail");
    }

    ts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");
    if (!ts_data->ts_workqueue) {
        FTS_ERROR("create fts workqueue fail");
    }

    fts_data = ts_data;
    spin_lock_init(&ts_data->irq_lock);
    mutex_init(&ts_data->report_mutex);
    mutex_init(&ts_data->bus_lock);
    init_waitqueue_head(&ts_data->ts_waitqueue);

    ret = fts_bus_init(ts_data);
    if (ret) {
        FTS_ERROR("bus initialize fail");
        goto err_bus_init;
    }

    ret = fts_input_init(ts_data);
    if (ret) {
        FTS_ERROR("input initialize fail");
        goto err_input_init;
    }

    ret = fts_buffer_init(ts_data);
    if (ret) {
        FTS_ERROR("buffer init fail");
        goto err_buffer_init;
    }

    ret = fts_gpio_configure(ts_data);
    if (ret) {
        FTS_ERROR("configure the gpios fail");
        goto err_gpio_config;
    }

#if FTS_POWER_SOURCE_CUST_EN
    ret = fts_power_source_init(ts_data);
    if (ret) {
        FTS_ERROR("fail to get power(regulator)");
        goto err_power_init;
    }
#endif

/* N17 code for HQ-299560 by zhangzhijian5 at 2023/8/16 start */
#if (!FTS_CHIP_IDC)
    fts_reset_proc_extend(100);
#endif
/* N17 code for HQ-299560 by zhangzhijian5 at 2023/8/16 end */

    ret = fts_get_ic_information(ts_data);
    if (ret) {
        FTS_ERROR("not focal IC, unregister driver");
        goto err_irq_req;
    }

    ret = fts_create_apk_debug_channel(ts_data);
    if (ret) {
        FTS_ERROR("create apk debug node fail");
    }

    ret = fts_create_sysfs(ts_data);
    if (ret) {
        FTS_ERROR("create sysfs node fail");
    }

    ret = fts_point_report_check_init(ts_data);
    if (ret) {
        FTS_ERROR("init point report check fail");
    }

    ret = fts_ex_mode_init(ts_data);
    if (ret) {
        FTS_ERROR("init glove/cover/charger fail");
    }

    ret = fts_gesture_init(ts_data);
    if (ret) {
        FTS_ERROR("init gesture fail");
    }

    ret = fts_test_init(ts_data);
    if (ret) {
        FTS_ERROR("init host test fail");
    }

    ret = fts_esdcheck_init(ts_data);
    if (ret) {
        FTS_ERROR("init esd check fail");
    }

    ret = fts_irq_registration(ts_data);
    if (ret) {
        FTS_ERROR("request irq failed");
        goto err_irq_req;
    }

    ret = fts_fwupg_init(ts_data);
    if (ret) {
        FTS_ERROR("init fw upgrade fail");
    }

    if (ts_data->ts_workqueue) {
        INIT_WORK(&ts_data->resume_work, fts_resume_work);
    }

#if IS_ENABLED(CONFIG_PM) && FTS_PATCH_COMERR_PM
    init_completion(&ts_data->pm_completion);
    ts_data->pm_suspend = false;
#endif

    ret = fts_notifier_callback_init(ts_data);
    if (ret) {
        FTS_ERROR("init notifier callback fail");
    }

    /* N17 code for HQ-291087 by liunianliang at 2023/5/29 start */
    ret = fts_create_procfs(ts_data);
    if (ret) {
        FTS_ERROR("create procfs node fail");
    }
    /* N17 code for HQ-291087 by liunianliang at 2023/5/29 end */

    /* N17 code for HQ-290835 by liunianliang at 2023/6/12 start */
    fts_init_xiaomi_touchfeature(ts_data);
    /* N17 code for HQ-290835 by liunianliang at 2023/6/12 end */

    FTS_FUNC_EXIT();
    return 0;

err_irq_req:
#if FTS_POWER_SOURCE_CUST_EN
err_power_init:
    fts_power_source_exit(ts_data);
#endif
    if (gpio_is_valid(ts_data->pdata->reset_gpio))
        gpio_free(ts_data->pdata->reset_gpio);
    if (gpio_is_valid(ts_data->pdata->irq_gpio))
        gpio_free(ts_data->pdata->irq_gpio);
    if (gpio_is_valid(ts_data->pdata->iovdd_gpio))
        gpio_free(ts_data->pdata->iovdd_gpio);
err_gpio_config:
    kfree_safe(ts_data->touch_buf);
err_buffer_init:
    input_unregister_device(ts_data->input_dev);
#if FTS_PEN_EN
    input_unregister_device(ts_data->pen_dev);
#endif
err_input_init:
err_bus_init:
    if (ts_data->ts_workqueue) destroy_workqueue(ts_data->ts_workqueue);
    kfree_safe(ts_data->bus_tx_buf);
    kfree_safe(ts_data->bus_rx_buf);
    kfree_safe(ts_data->pdata);

    FTS_FUNC_EXIT();
    return ret;
}

int fts_ts_remove_entry(struct fts_ts_data *ts_data)
{
    FTS_FUNC_ENTER();
    cancel_work_sync(&fts_data->resume_work);
    fts_point_report_check_exit(ts_data);
    fts_release_apk_debug_channel(ts_data);
    fts_remove_sysfs(ts_data);
    /* N17 code for HQ-291087 by liunianliang at 2023/5/29 start */
    fts_remove_procfs(ts_data);
    /* N17 code for HQ-291087 by liunianliang at 2023/5/29 end */
    fts_ex_mode_exit(ts_data);

    fts_fwupg_exit(ts_data);

    fts_test_exit(ts_data);

    fts_esdcheck_exit(ts_data);

    fts_gesture_exit(ts_data);

    free_irq(ts_data->irq, ts_data);

    input_unregister_device(ts_data->input_dev);
#if FTS_PEN_EN
    input_unregister_device(ts_data->pen_dev);
#endif

    if (ts_data->ts_workqueue) destroy_workqueue(ts_data->ts_workqueue);

    fts_notifier_callback_exit(ts_data);

    if (gpio_is_valid(ts_data->pdata->reset_gpio))
        gpio_free(ts_data->pdata->reset_gpio);

    if (gpio_is_valid(ts_data->pdata->irq_gpio))
        gpio_free(ts_data->pdata->irq_gpio);

    if (gpio_is_valid(ts_data->pdata->iovdd_gpio))
        gpio_free(ts_data->pdata->iovdd_gpio);


#if FTS_POWER_SOURCE_CUST_EN
    fts_power_source_exit(ts_data);
#endif

    kfree_safe(ts_data->touch_buf);
    kfree_safe(ts_data->bus_tx_buf);
    kfree_safe(ts_data->bus_rx_buf);
    kfree_safe(ts_data->pdata);

    FTS_FUNC_EXIT();
    return 0;
}
