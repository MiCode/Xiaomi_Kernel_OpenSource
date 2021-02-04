/*
 *
 
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2017, FocalTech Systems, Ltd., all rights reserved.
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
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
//#include <linux/wakelock.h>
#include "focaltech_core.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/

//add by cassy begin
//#define CONFIG_MTK_POWER
#define GTP_enable_power_PORT 2//add by cassy
struct pinctrl *pinctrl3;
struct pinctrl_state *focal_power_output1,*focal_power_output0;
static void tpd_power_on(int flag);

//add by cassy end

#define FTS_DRIVER_NAME                     "fts_ts"
#define INTERVAL_READ_REG                   100  //interval time per read reg unit:ms
#define TIMEOUT_READ_REG                    1000 //timeout of read reg unit:ms
#define FTS_I2C_SLAVE_ADDR                  0x38

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static int tpd_flag;
unsigned int tpd_rst_gpio_number = 0;
unsigned int tpd_int_gpio_number = 1;

#if (defined(CONFIG_TPD_HAVE_CALIBRATION) && !defined(CONFIG_TPD_CUSTOM_CALIBRATION))
static int tpd_def_calmat_local_normal[8]  = TPD_CALIBRATION_MATRIX_ROTATION_NORMAL;
static int tpd_def_calmat_local_factory[8] = TPD_CALIBRATION_MATRIX_ROTATION_FACTORY;
#endif

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct fts_ts_data *fts_data;

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static int tpd_remove(struct i2c_client *client);
static void tpd_resume(struct device *h);
static void tpd_suspend(struct device *h);
static void fts_release_all_finger(void);

/*****************************************************************************
* Focaltech ts i2c driver configuration
*****************************************************************************/
static const struct i2c_device_id fts_tpd_id[] = {{FTS_DRIVER_NAME, 0}, {} };
static const struct of_device_id fts_dt_match[] = {
    {.compatible = "mediatek,ft_cap_touch"},
    {},
};
MODULE_DEVICE_TABLE(of, fts_dt_match);

static struct i2c_driver tpd_i2c_driver = {
    .driver = {
        .name = FTS_DRIVER_NAME,
        .of_match_table = of_match_ptr(fts_dt_match),
    },
    .probe = tpd_probe,
    .remove = tpd_remove,
    .id_table = fts_tpd_id,
    .detect = tpd_i2c_detect,
};

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
    struct i2c_client *client = ts_data->client;

    do {
        ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID, &chip_id[0]);
        ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID2, &chip_id[1]);
        if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
            FTS_DEBUG("i2c read error, read:0x%02x%02x", chip_id[0], chip_id[1]);
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
#if FTS_HID_SUPPORTTED
        fts_i2c_hid2std(client);
#endif
        id_cmd[0] = FTS_CMD_START1;
        id_cmd[1] = FTS_CMD_START2;
        ret = fts_i2c_write(client, id_cmd, 2);
        if (ret < 0) {
            FTS_ERROR("start cmd write fail");
            return ret;
        }
        id_cmd[0] = FTS_CMD_READ_ID;
        id_cmd[1] = id_cmd[2] = id_cmd[3] = 0x00;
        ret = fts_i2c_read(client, id_cmd, 4, chip_id, 2);
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

    ts_data->ic_info.is_incell = FTS_CHIP_IDC;
    ts_data->ic_info.hid_supported = FTS_HID_SUPPORTTED;
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
    /* wait tp stable */
    fts_wait_tp_to_valid(client);
    /* recover TP charger state 0x8B */
    /* recover TP glove state 0xC0 */
    /* recover TP cover state 0xC1 */
    fts_ex_mode_recovery(client);
#if FTS_PSENSOR_EN
    fts_proximity_recovery(client);
#endif

    /* recover TP gesture state 0xD0 */
#if FTS_GESTURE_EN
    fts_gesture_recovery(client);
#endif
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
	
    tpd_gpio_output(tpd_rst_gpio_number, 0);
    msleep(20);
    tpd_gpio_output(tpd_rst_gpio_number, 1);
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
int fts_power_init(void)
{

    int ret;
    /*set TP volt*/
    tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
    ret = regulator_set_voltage(tpd->reg, 3000000, 3000000);
    if (ret != 0) {
        FTS_ERROR("[POWER]Failed to set voltage of regulator,ret=%d!", ret);
        return ret;
    }

    ret = regulator_enable(tpd->reg);
    if (ret != 0) {
        FTS_ERROR("[POWER]Fail to enable regulator when init,ret=%d!", ret);
        return ret;
    }

    return 0;
}

void fts_power_suspend(void)
{
    int ret;

    ret = regulator_disable(tpd->reg);
    if (ret != 0)
        FTS_ERROR("[POWER]Failed to disable regulator when suspend ret=%d!", ret);
}

int fts_power_resume(void)
{
    int ret = 0;

    ret = regulator_enable(tpd->reg);
    if (ret != 0)
        FTS_ERROR("[POWER]Failed to enable regulator when resume,ret=%d!", ret);

    return ret;
}
#endif

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
    count += sprintf(g_sz_debug, "%02X,%02X,%02X", buf[0], buf[1], buf[2]);
    for (i = 0; i < len; i++) {
        count += sprintf(g_sz_debug + count, ",%02X", buf[i + 3]);
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
    unsigned int finger_count = 0;
#endif

    FTS_FUNC_ENTER();
    mutex_lock(&fts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
    for (finger_count = 0; finger_count < tpd_dts_data.touch_max_num; finger_count++) {
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
    int i = 0;
    struct ts_event *events = data->events;
    u32 key_num = tpd_dts_data.tpd_key_num;

    if (!KEY_EN) {
        return -EINVAL;
    }
    for (i = 0; i < key_num; i++) {
        if (EVENT_DOWN(events[index].flag)) {
            data->key_down = true;
            tpd_button(events[index].x, events[index].y, 1);
            FTS_DEBUG("Key(%d, %d) DOWN", events[index].x, events[index].y);
        } else {
            data[index].key_down = false;
            tpd_button(events[index].x, events[index].y, 0);
            FTS_DEBUG("Key(%d, %d) UP", events[index].x, events[index].y);
        }
    }

    return 0;
}

#if FTS_MT_PROTOCOL_B_EN
static int fts_input_report_b(struct fts_ts_data *data)
{
    int i = 0;
    int uppoint = 0;
    int touchs = 0;
    bool va_reported = false;
    u32 max_touch_num = tpd_dts_data.touch_max_num;
    u32 key_y_coor = TPD_RES_Y;
    struct ts_event *events = data->events;

    for (i = 0; i < data->touch_point; i++) {
        if (KEY_EN && TOUCH_IS_KEY(events[i].y, key_y_coor)) {
			
            fts_input_report_key(data, i);
            continue;
        }

        if (events[i].id >= max_touch_num)
            break;

        va_reported = true;
        input_mt_slot(tpd->dev, events[i].id);

        if (EVENT_DOWN(events[i].flag)) {
            input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, true);

#if FTS_REPORT_PRESSURE_EN
            if (events[i].p <= 0) {
                events[i].p = 0x3f;
            }
            input_report_abs(tpd->dev, ABS_MT_PRESSURE, events[i].p);
#endif
            if (events[i].area <= 0) {
                events[i].area = 0x09;
            }
            input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, events[i].area);
            input_report_abs(tpd->dev, ABS_MT_POSITION_X, events[i].x);
            input_report_abs(tpd->dev, ABS_MT_POSITION_Y, events[i].y);

            touchs |= BIT(events[i].id);
            data->touchs |= BIT(events[i].id);

            FTS_DEBUG("[B]P%d(%d, %d)[p:%d,tm:%d] DOWN!", events[i].id, events[i].x,
                      events[i].y, events[i].p, events[i].area);
        } else {
            uppoint++;
            input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, false);
            data->touchs &= ~BIT(events[i].id);
            FTS_DEBUG("[B]P%d UP!", events[i].id);
        }
    }

    if (unlikely(data->touchs ^ touchs)) {
        for (i = 0; i < max_touch_num; i++)  {
            if (BIT(i) & (data->touchs ^ touchs)) {
                FTS_DEBUG("[B]P%d UP!", i);
                va_reported = true;
                input_mt_slot(tpd->dev, i);
                input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, false);
            }
        }
    }
    data->touchs = touchs;

    if (va_reported) {
        /* touchs==0, there's no point but key */
        if (EVENT_NO_DOWN(data) || (!touchs)) {
            FTS_DEBUG("[B]Points All Up!");
            input_report_key(tpd->dev, BTN_TOUCH, 0);
        } else {
            input_report_key(tpd->dev, BTN_TOUCH, 1);
        }
        input_sync(tpd->dev);
    }

    return 0;
}

#else
static int fts_input_report_a(struct fts_ts_data *data)
{
    int i = 0;
    int touchs = 0;
    bool va_reported = false;
    u32 key_y_coor = TPD_RES_Y;
    struct ts_event *events = data->events;

    for (i = 0; i < data->touch_point; i++) {
        if (KEY_EN && TOUCH_IS_KEY(events[i].y, key_y_coor)) {
            fts_input_report_key(data, i);
            continue;
        }

        va_reported = true;
        if (EVENT_DOWN(events[i].flag)) {
            input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, events[i].id);
#if FTS_REPORT_PRESSURE_EN
            if (events[i].p <= 0) {
                events[i].p = 0x3f;
            }
            input_report_abs(tpd->dev, ABS_MT_PRESSURE, events[i].p);
#endif
            if (events[i].area <= 0) {
                events[i].area = 0x09;
            }
            input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, events[i].area);

            input_report_abs(tpd->dev, ABS_MT_POSITION_X, events[i].x);
            input_report_abs(tpd->dev, ABS_MT_POSITION_Y, events[i].y);

            input_mt_sync(tpd->dev);

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
            input_report_key(tpd->dev, BTN_TOUCH, 0);
            input_mt_sync(tpd->dev);
        } else {
            input_report_key(tpd->dev, BTN_TOUCH, 1);
        }

        input_sync(tpd->dev);
    }
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
    int max_touch_num = tpd_dts_data.touch_max_num ;
    u8 *buf = data->point_buf;

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
*  Name: tpd_eint_interrupt_handler
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static irqreturn_t tpd_eint_interrupt_handler(int irq, void *dev_id)
{
    tpd_flag = 1;
    wake_up_interruptible(&waiter);
    return IRQ_HANDLED;
}

/*****************************************************************************
*  Name: tpd_irq_registration
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int tpd_irq_registration(struct fts_ts_data *ts_data)
{
    struct device_node *node = NULL;
    int ret = 0;

    node = of_find_matching_node(node, touch_of_match);
    if (NULL == node) {
        FTS_ERROR("Can not find touch eint device node!");
        return -ENODATA;
    }

    ts_data->irq = irq_of_parse_and_map(node, 0);
    ts_data->client->irq = ts_data->irq;
    FTS_INFO("IRQ request succussfully, irq:%d client:%d", ts_data->irq, ts_data->client->irq);
    ret = request_irq(ts_data->irq, tpd_eint_interrupt_handler,
                      IRQF_TRIGGER_FALLING, "TOUCH_PANEL-eint", NULL);

    return ret;
}

/*****************************************************************************
*  Name: touch_event_handler
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int touch_event_handler(void *unused)
{
    int ret;
    struct fts_ts_data *ts_data = fts_data;
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };

    sched_setscheduler(current, SCHED_RR, &param);
    do {
        set_current_state(TASK_INTERRUPTIBLE);
        wait_event_interruptible(waiter, tpd_flag != 0);

        tpd_flag = 0;

        set_current_state(TASK_RUNNING);

        FTS_DEBUG("touch_event_handler start");
#if FTS_PSENSOR_EN
        if (fts_proximity_readdata(ts_data->client) == 0)
            continue;
#endif

#if FTS_GESTURE_EN
        if (0 == fts_gesture_readdata(ts_data)) {
            FTS_INFO("succuss to get gesture data in irq handler");
            continue;
        }
#endif

#if FTS_POINT_REPORT_CHECK_EN
        fts_prc_queue_work(ts_data);
#endif

#if FTS_ESDCHECK_EN
        fts_esdcheck_set_intr(1);
#endif

        ret = fts_read_touchdata(ts_data);
#if FTS_MT_PROTOCOL_B_EN
        if (ret == 0) {
            mutex_lock(&ts_data->report_mutex);
            fts_input_report_b(ts_data);
            mutex_unlock(&ts_data->report_mutex);
        }
#else
        if (ret == 0) {
            mutex_lock(&ts_data->report_mutex);
            fts_input_report_a(ts_data);
            mutex_unlock(&ts_data->report_mutex);
        }
#endif

#if FTS_ESDCHECK_EN
        fts_esdcheck_set_intr(0);
#endif
    } while (!kthread_should_stop());

    return 0;
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
    int point_num = 0;
    int ret = 0;

    FTS_FUNC_ENTER();

#if FTS_MT_PROTOCOL_B_EN
    input_mt_init_slots(tpd->dev, tpd_dts_data.touch_max_num, INPUT_MT_DIRECT);
#endif

    point_num = tpd_dts_data.touch_max_num;
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

    FTS_FUNC_EXIT();
    return 0;

err_event_buf:
    kfree_safe(ts_data->point_buf);

err_point_buf:
    FTS_FUNC_EXIT();
    return ret;
}


/************************************************************************
* Name: tpd_i2c_detect
* Brief:
* Input:
* Output:
* Return:
***********************************************************************/
static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
    strcpy(info->type, TPD_DEVICE);

    return 0;
}

/************************************************************************
* Name: fts_probe
* Brief:
* Input:
* Output:
* Return:
***********************************************************************/
	//add by cassy begin
	int tpd_get_gpio_info_focal(struct i2c_client *pdev)
	{
		int ret;
	
		
		pinctrl3 = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(pinctrl3)) {
			ret = PTR_ERR(pinctrl3);
		
			return ret;
		}
			focal_power_output0 = pinctrl_lookup_state(pinctrl3, "state_focal_power_output0");
			if (IS_ERR(focal_power_output0)) {
				ret = PTR_ERR(focal_power_output0);
				return ret;
			}
			else
				{
				//printk("success\n");
				}
			focal_power_output1 = pinctrl_lookup_state(pinctrl3, "state_focal_power_output1");
			if (IS_ERR(focal_power_output1)) {
				ret = PTR_ERR(focal_power_output1);
				return ret;
			}
		
		return 0;
	}
	
	
	//add by cassy end

static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;
    struct fts_ts_data *ts_data;

    FTS_FUNC_ENTER();
    ts_data = devm_kzalloc(&client->dev, sizeof(*ts_data), GFP_KERNEL);
    if (!ts_data) {
        FTS_ERROR("Failed to allocate memory for fts_data");
        return -ENOMEM;
    }

    fts_data = ts_data;
    ts_data->client = client;
    ts_data->input_dev = tpd->dev;

    if (client->addr != FTS_I2C_SLAVE_ADDR) {
        FTS_INFO("[TPD]Change i2c addr 0x%02x to %x", client->addr, FTS_I2C_SLAVE_ADDR);
        client->addr = FTS_I2C_SLAVE_ADDR;
        FTS_INFO("[TPD]i2c addr=0x%x\n", client->addr);
    }
    ts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");
    if (NULL == ts_data->ts_workqueue) {
        FTS_ERROR("failed to create fts workqueue");
    }
	
	//add by cassy begin
		tpd_get_gpio_info_focal(client);
		//add by cassy end
		
//add by allen start
tpd_power_on(1);
//add by allen end
    spin_lock_init(&ts_data->irq_lock);
    mutex_init(&ts_data->report_mutex);
    /* Init I2C */
    fts_i2c_init();
	    ret = fts_input_init(ts_data);
    if (ret) {
        FTS_ERROR("fts input initialize fail");
        goto err_input_init;
    }

  fts_reset_proc(200);
	
    ret = fts_get_ic_information(ts_data);
    if (ret) {
        FTS_ERROR("not focal IC, unregister driver");
        goto err_input_init;
    }
	
#if FTS_APK_NODE_EN////1
    ret = fts_create_apk_debug_channel(ts_data);
    if (ret) {
        FTS_ERROR("create apk debug node fail");
    }
#endif

#if FTS_SYSFS_NODE_EN////1
    ret = fts_create_sysfs(client);
    if (ret) {
        FTS_ERROR("create sysfs node fail");
    }
#endif

#if FTS_POINT_REPORT_CHECK_EN///0
    ret = fts_point_report_check_init(ts_data);
    if (ret) {
        FTS_ERROR("init point report check fail");
    }
#endif
	
    ret = fts_ex_mode_init(client);
    if (ret) {
        FTS_ERROR("init glove/cover/charger fail");
    }

#if FTS_GESTURE_EN////0
    ret = fts_gesture_init(ts_data);
    if (ret) {
        FTS_ERROR("init gesture fail");
    }
#endif

#if FTS_PSENSOR_EN///0
    fts_proximity_init(client);
#endif

#if FTS_TEST_EN///0(cassy change from 1 to 0)
    ret = fts_test_init(client);
    if (ret) {
        FTS_ERROR("init production test fail");
    }
#endif

#if FTS_ESDCHECK_EN//0
    ret = fts_esdcheck_init(ts_data);
    if (ret) {
        FTS_ERROR("init esd check fail");
    }
#endif
	
    ts_data->thread_tpd = kthread_run(touch_event_handler, 0, TPD_DEVICE);
    if (IS_ERR(ts_data->thread_tpd)) {
        ret = PTR_ERR(ts_data->thread_tpd);
        FTS_ERROR("[TPD]Failed to create kernel thread_tpd,ret:%d", ret);
        ts_data->thread_tpd = NULL;
        goto err_input_init;
    }

    FTS_DEBUG("[TPD]Touch Panel Device Probe %s!", (ret < 0) ? "FAIL" : "PASS");
	

    /* Configure gpio to irq and request irq */
    tpd_gpio_as_int(tpd_int_gpio_number);
    ret = tpd_irq_registration(ts_data);
    if (ret) {
        FTS_ERROR("request irq failed");
        goto err_irq_req;
    }

#if FTS_AUTO_UPGRADE_EN////1
    ret = fts_fwupg_init(ts_data);
    if (ret) {
        FTS_ERROR("init fw upgrade fail");
    }
#endif

    tpd_load_status = 1;
    FTS_DEBUG("TPD_RES_Y:%d", (int)TPD_RES_Y);
    FTS_FUNC_EXIT();
    return 0;

err_irq_req:
    if (ts_data->thread_tpd) {
        kthread_stop(ts_data->thread_tpd);
        ts_data->thread_tpd = NULL;
    }
err_input_init:
    if (ts_data->ts_workqueue)
        destroy_workqueue(ts_data->ts_workqueue);
    kfree_safe(ts_data->point_buf);
    kfree_safe(ts_data->events);
    devm_kfree(&client->dev, ts_data);

    FTS_FUNC_EXIT();
    return ret;
}

/*****************************************************************************
*  Name: tpd_remove
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int tpd_remove(struct i2c_client *client)
{

    struct fts_ts_data *ts_data = i2c_get_clientdata(client);

    FTS_FUNC_ENTER();

#if FTS_TEST_EN
    fts_test_exit(client);
#endif

#if FTS_POINT_REPORT_CHECK_EN
    fts_point_report_check_exit();
#endif

#if FTS_SYSFS_NODE_EN
    fts_remove_sysfs(client);
#endif

    fts_ex_mode_exit(client);

#if FTS_PSENSOR_EN
    fts_proximity_exit(client);
#endif
#if FTS_APK_NODE_EN
    fts_release_apk_debug_channel(ts_data);
#endif

#if FTS_AUTO_UPGRADE_EN
    fts_fwupg_exit(ts_data);
#endif

#if FTS_ESDCHECK_EN
    fts_esdcheck_exit(ts_data);
#endif

#if FTS_GESTURE_EN
    fts_gesture_exit(client);
#endif

    fts_i2c_exit();

    kfree_safe(ts_data->point_buf);
    kfree_safe(ts_data->events);

    if (ts_data->ts_workqueue)
        destroy_workqueue(ts_data->ts_workqueue);

    FTS_FUNC_EXIT();

    return 0;
}

/*****************************************************************************
*  Name: tpd_local_init
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
//add by cassy begin
void tpd_gpio_output_focal(int pin, int level)
{
	//mutex_lock(&tpd_set_gpio_mutex);
	if (pin == 2) {
		if (level)
			pinctrl_select_state(pinctrl3, focal_power_output1);
		else
			pinctrl_select_state(pinctrl3, focal_power_output0);
	} 
	//mutex_unlock(&tpd_set_gpio_mutex);
}
#if FTS_POWER_SOURCE_CUST_EN
#else

static void tpd_power_on(int flag)
{
	if(flag)
		{
		
		tpd_gpio_output_focal(GTP_enable_power_PORT, 1);
		}
		else
		{
		
        tpd_gpio_output_focal(GTP_enable_power_PORT, 0);

		}
	}
#endif
//add by cassy end

static int tpd_local_init(void)
{
    FTS_FUNC_ENTER();
	//modify by cassy begin
#if FTS_POWER_SOURCE_CUST_EN///1//modify by cassy from 0 to 1

    if (fts_power_init() != 0)
        return -1;
#else


//tpd_power_on(1);
#endif
	//modify by cassy end
	

    if (i2c_add_driver(&tpd_i2c_driver) != 0) {
        FTS_ERROR("[TPD]: Unable to add fts i2c driver!!");
        FTS_FUNC_EXIT();
        return -1;
    }

    if (tpd_dts_data.use_tpd_button) {
		
        tpd_button_setting(tpd_dts_data.tpd_key_num, tpd_dts_data.tpd_key_local,
                           tpd_dts_data.tpd_key_dim_local);
    }

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
#endif

#if (defined(CONFIG_TPD_HAVE_CALIBRATION) && !defined(CONFIG_TPD_CUSTOM_CALIBRATION))
    memcpy(tpd_calmat, tpd_def_calmat_local_factory, 8 * 4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local_factory, 8 * 4);

    memcpy(tpd_calmat, tpd_def_calmat_local_normal, 8 * 4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local_normal, 8 * 4);
#endif

    tpd_type_cap = 1;

    FTS_FUNC_EXIT();
    return 0;
}

/*****************************************************************************
*  Name: tpd_suspend
*  Brief: When suspend, will call this function
*           1. Set gesture if EN
*           2. Disable ESD if EN
*           3. Process MTK sensor hub if configure, default n, if n, execute 4/5/6
*           4. disable irq
*           5. Set TP to sleep mode
*           6. Disable power(regulator) if EN
*           7. fts_release_all_finger
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void tpd_suspend(struct device *h)
{
    int ret = 0;
    struct fts_ts_data *ts_data = fts_data;

    FTS_FUNC_ENTER();

    if (ts_data->suspended) {
        FTS_INFO("Already in suspend state");
        return;
    }

    if (ts_data->fw_loading) {
        FTS_INFO("fw upgrade in process, can't suspend");
        return;
    }

#if FTS_PSENSOR_EN
    if (fts_proximity_suspend() == 0) {
        fts_release_all_finger();
        ts_data->suspended = true;
        return;
    }
#endif

#if FTS_ESDCHECK_EN
    fts_esdcheck_suspend();
#endif

#if FTS_GESTURE_EN
    ret = fts_gesture_suspend(ts_data->client);
    if (ret == 0) {
        /* Enter into gesture mode(suspend) */
        ts_data->suspended = true;
        return;
    }
#endif

    fts_irq_disable();
    /* TP enter sleep mode */
    ret = fts_i2c_write_reg(ts_data->client, FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP_VALUE);
    if (ret < 0) {
        FTS_ERROR("Set TP to sleep mode fail, ret=%d!", ret);
    }

#if FTS_POWER_SOURCE_CUST_EN
    fts_power_suspend();
#endif

    ts_data->suspended = true;
    FTS_FUNC_EXIT();
}


/*****************************************************************************
*  Name: tpd_resume
*  Brief: When suspend, will call this function
*           1. Clear gesture if EN
*           2. Enable power(regulator) if EN
*           3. Execute reset if no IDC to wake up
*           4. Confirm TP in valid app by read chip ip register:0xA3
*           5. fts_release_all_finger
*           6. Enable ESD if EN
*           7. tpd_usb_plugin if EN
*           8. Process MTK sensor hub if configure, default n, if n, execute 9
*           9. disable irq
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void tpd_resume(struct device *h)
{
    struct fts_ts_data *ts_data = fts_data;

    FTS_FUNC_ENTER();

    if (!ts_data->suspended) {
        FTS_DEBUG("Already in awake state");
        return;
    }

#if FTS_PSENSOR_EN
    if (fts_proximity_resume() == 0) {
        ts_data->suspended = false;
        return;
    }
#endif

    fts_release_all_finger();

#if FTS_POWER_SOURCE_CUST_EN
    fts_power_resume();
#endif

    if (!ts_data->ic_info.is_incell) {
        fts_reset_proc(200);
    }

    /* Before read/write TP register, need wait TP to valid */
    fts_tp_state_recovery(ts_data->client);

#if FTS_ESDCHECK_EN
    fts_esdcheck_resume();
#endif

#if FTS_GESTURE_EN
    if (fts_gesture_resume(ts_data->client) == 0) {
        ts_data->suspended = false;
        FTS_FUNC_EXIT();
        return;
    }
#endif

    fts_irq_enable();

    ts_data->suspended = false;
    FTS_FUNC_EXIT();
}

/*****************************************************************************
*  TPD Device Driver
*****************************************************************************/
static struct tpd_driver_t tpd_device_driver = {
    .tpd_device_name = FTS_DRIVER_NAME,
    .tpd_local_init = tpd_local_init,
    .suspend = tpd_suspend,
    .resume = tpd_resume,
};

/*****************************************************************************
*  Name: tpd_driver_init
*  Brief: 1. Get dts information
*         2. call tpd_driver_add to add tpd_device_driver
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int __init tpd_driver_init(void)
{
	
    FTS_FUNC_ENTER();
    FTS_INFO("Driver version: %s", FTS_DRIVER_VERSION);
    tpd_get_dts_info();
    if (tpd_dts_data.touch_max_num < 2)
        tpd_dts_data.touch_max_num = 2;
    else if (tpd_dts_data.touch_max_num > FTS_MAX_POINTS_SUPPORT)///10
        tpd_dts_data.touch_max_num = FTS_MAX_POINTS_SUPPORT;
    FTS_INFO("tpd max touch num:%d", tpd_dts_data.touch_max_num);
    if (tpd_driver_add(&tpd_device_driver) < 0) {
        FTS_ERROR("[TPD]: Add FTS Touch driver failed!!");
    }

    FTS_FUNC_EXIT();
    return 0;
}

/*****************************************************************************
*  Name: tpd_driver_exit
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void __exit tpd_driver_exit(void)
{
    FTS_FUNC_ENTER();
    tpd_driver_remove(&tpd_device_driver);
    FTS_FUNC_EXIT();
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver for Mediatek");
MODULE_LICENSE("GPL v2");
