/*
 *
 * FocalTech ftxxxx TouchScreen driver.
 *
 * Copyright (c) 2012-2020, Focaltech Ltd. All rights reserved.
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
* File Name: focaltech_ex_mode.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-31
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "focaltech_core.h"

/* N17 code for HQ-291711 by liunianliang at 2023/06/03 start */
#include "mtk_charger.h"
#include "mtk_battery.h"
#include "tcpci_typec.h"
#include <linux/power_supply.h>
#include <linux/switch.h>
/* N17 code for HQ-291711 by liunianliang at 2023/06/03 end */

/*****************************************************************************
* 2.Private constant and macro definitions using #define
*****************************************************************************/

/*****************************************************************************
* 3.Private enumerations, structures and unions using typedef
*****************************************************************************/
enum _ex_mode {
    MODE_GLOVE = 0,
    MODE_COVER,
    MODE_CHARGER,
    /* N17 code for HQ-291087 by liunianliang at 2023/5/29 start */
    MODE_EARPHONE,
    MODE_EDGE,
    /* N17 code for HQ-291087 by liunianliang at 2023/5/29 end */
};

/*****************************************************************************
* 4.Static variables
*****************************************************************************/

/*****************************************************************************
* 5.Global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* 6.Static function prototypes
*******************************************************************************/
/* N17 code for HQ-291087 by liunianliang at 2023/5/29 start */
static int fts_ex_mode_switch(enum _ex_mode mode, u8 value)
{
    int ret = 0;
    u8 m_val = 0;

    if (value)
        m_val = value;//0x01;
    else
        m_val = 0x00;

    switch (mode) {
    case MODE_GLOVE:
        ret = fts_write_reg(FTS_REG_GLOVE_MODE_EN, m_val);
        if (ret < 0) {
            FTS_ERROR("MODE_GLOVE switch to %d fail", m_val);
        }
        break;
    case MODE_COVER:
        ret = fts_write_reg(FTS_REG_COVER_MODE_EN, m_val);
        if (ret < 0) {
            FTS_ERROR("MODE_COVER switch to %d fail", m_val);
        }
        break;
    case MODE_CHARGER:
        ret = fts_write_reg(FTS_REG_CHARGER_MODE_EN, m_val);
        if (ret < 0) {
            FTS_ERROR("MODE_CHARGER switch to %d fail", m_val);
        }
        break;
    case MODE_EARPHONE:
        ret = fts_write_reg(FTS_REG_EARPHONE_MODE_EN, m_val);
        if (ret < 0) {
            FTS_ERROR("MODE_EARPHONE switch to %d fail", m_val);
        }
        break;
    case MODE_EDGE:
        ret = fts_write_reg(FTS_REG_EDGE_MODE_EN, m_val);
        if (ret < 0) {
            FTS_ERROR("MODE_EDGE switch to %d fail", m_val);
        }
        break;
    default:
        FTS_ERROR("mode(%d) unsupport", mode);
        ret = -EINVAL;
        break;
    }

    return ret;
}
/* N17 code for HQ-291087 by liunianliang at 2023/5/29 end */

static ssize_t fts_glove_mode_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;
    u8 val = 0;
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);
    struct input_dev *input_dev = ts_data->input_dev;

    mutex_lock(&input_dev->mutex);
    fts_read_reg(FTS_REG_GLOVE_MODE_EN, &val);
    count = snprintf(buf + count, PAGE_SIZE, "Glove Mode:%s\n",
                     ts_data->glove_mode ? "On" : "Off");
    count += snprintf(buf + count, PAGE_SIZE, "Glove Reg(0xC0):%d\n", val);
    mutex_unlock(&input_dev->mutex);

    return count;
}

static ssize_t fts_glove_mode_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    int ret = 0;
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    if (FTS_SYSFS_ECHO_ON(buf)) {
        if (!ts_data->glove_mode) {
            FTS_DEBUG("enter glove mode");
            ret = fts_ex_mode_switch(MODE_GLOVE, ENABLE);
            if (ret >= 0) {
                ts_data->glove_mode = ENABLE;
            }
        }
    } else if (FTS_SYSFS_ECHO_OFF(buf)) {
        if (ts_data->glove_mode) {
            FTS_DEBUG("exit glove mode");
            ret = fts_ex_mode_switch(MODE_GLOVE, DISABLE);
            if (ret >= 0) {
                ts_data->glove_mode = DISABLE;
            }
        }
    }

    FTS_DEBUG("glove mode:%d", ts_data->glove_mode);
    return count;
}


static ssize_t fts_cover_mode_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;
    u8 val = 0;
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);
    struct input_dev *input_dev = ts_data->input_dev;

    mutex_lock(&input_dev->mutex);
    fts_read_reg(FTS_REG_COVER_MODE_EN, &val);
    count = snprintf(buf + count, PAGE_SIZE, "Cover Mode:%s\n",
                     ts_data->cover_mode ? "On" : "Off");
    count += snprintf(buf + count, PAGE_SIZE, "Cover Reg(0xC1):%d\n", val);
    mutex_unlock(&input_dev->mutex);

    return count;
}

static ssize_t fts_cover_mode_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    int ret = 0;
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    if (FTS_SYSFS_ECHO_ON(buf)) {
        if (!ts_data->cover_mode) {
            FTS_DEBUG("enter cover mode");
            ret = fts_ex_mode_switch(MODE_COVER, ENABLE);
            if (ret >= 0) {
                ts_data->cover_mode = ENABLE;
            }
        }
    } else if (FTS_SYSFS_ECHO_OFF(buf)) {
        if (ts_data->cover_mode) {
            FTS_DEBUG("exit cover mode");
            ret = fts_ex_mode_switch(MODE_COVER, DISABLE);
            if (ret >= 0) {
                ts_data->cover_mode = DISABLE;
            }
        }
    }

    FTS_DEBUG("cover mode:%d", ts_data->cover_mode);
    return count;
}

static ssize_t fts_charger_mode_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;
    u8 val = 0;
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);
    struct input_dev *input_dev = ts_data->input_dev;

    mutex_lock(&input_dev->mutex);
    fts_read_reg(FTS_REG_CHARGER_MODE_EN, &val);
    count = snprintf(buf + count, PAGE_SIZE, "Charger Mode:%s\n",
                     ts_data->charger_mode ? "On" : "Off");
    count += snprintf(buf + count, PAGE_SIZE, "Charger Reg(0x8B):%d\n", val);
    mutex_unlock(&input_dev->mutex);

    return count;
}

static ssize_t fts_charger_mode_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    int ret = 0;
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    if (FTS_SYSFS_ECHO_ON(buf)) {
        if (!ts_data->charger_mode) {
            FTS_DEBUG("enter charger mode");
            ret = fts_ex_mode_switch(MODE_CHARGER, ENABLE);
            if (ret >= 0) {
                ts_data->charger_mode = ENABLE;
            }
        }
    } else if (FTS_SYSFS_ECHO_OFF(buf)) {
        if (ts_data->charger_mode) {
            FTS_DEBUG("exit charger mode");
            ret = fts_ex_mode_switch(MODE_CHARGER, DISABLE);
            if (ret >= 0) {
                ts_data->charger_mode = DISABLE;
            }
        }
    }

    FTS_DEBUG("charger mode:%d", ts_data->charger_mode);
    return count;
}

/* N17 code for HQ-291087 by liunianliang at 2023/5/29 start */
static ssize_t fts_earphone_mode_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;
    u8 val = 0;
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);
    struct input_dev *input_dev = ts_data->input_dev;

    mutex_lock(&input_dev->mutex);
    fts_read_reg(FTS_REG_EARPHONE_MODE_EN, &val);
    count = snprintf(buf + count, PAGE_SIZE, "Earphone Mode:%s\n",
                     ts_data->earphone_mode ? "On" : "Off");
    count += snprintf(buf + count, PAGE_SIZE, "Earphone Reg(0xC3):%d\n", val);
    mutex_unlock(&input_dev->mutex);

    return count;
}

static ssize_t fts_earphone_mode_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    int ret = 0;
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    if (FTS_SYSFS_ECHO_ON(buf)) {
        if (!ts_data->earphone_mode) {
            FTS_DEBUG("enter earphone mode");
            ret = fts_ex_mode_switch(MODE_EARPHONE, ENABLE);
            if (ret >= 0) {
                ts_data->earphone_mode = ENABLE;
            }
        }
    } else if (FTS_SYSFS_ECHO_OFF(buf)) {
        if (ts_data->earphone_mode) {
            FTS_DEBUG("exit earphone mode");
            ret = fts_ex_mode_switch(MODE_EARPHONE, DISABLE);
            if (ret >= 0) {
                ts_data->earphone_mode = DISABLE;
            }
        }
    }

    FTS_DEBUG("earphone mode:%d", ts_data->earphone_mode);
    return count;
}

static ssize_t fts_edge_mode_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;
    u8 val = 0;
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);
    struct input_dev *input_dev = ts_data->input_dev;

    mutex_lock(&input_dev->mutex);
    fts_read_reg(FTS_REG_EDGE_MODE_EN, &val);
    count = snprintf(buf + count, PAGE_SIZE, "Edge Mode:%s\n",
                     ts_data->edge_mode ? "On" : "Off");
    count += snprintf(buf + count, PAGE_SIZE, "Edge Reg(0x8C):%d\n", val);
    mutex_unlock(&input_dev->mutex);

    return count;
}

static ssize_t fts_edge_mode_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    int ret = 0;
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

/* N17 code for HQ-299540 by jiangyue at 2023/6/9 start */
    if (FTS_SYSFS_ECHO_ON(buf)) {
        if (ts_data->edge_mode == 0 || ts_data->edge_mode == 1 || ts_data->edge_mode == 2){
/* N17 code for HQ-299540 by jiangyue at 2023/6/9 end */
            FTS_DEBUG("enter edge mode");
            if (buf[0] == '1') /*USB PORTS RIGHT*/ {
	            ret = fts_ex_mode_switch(MODE_EDGE, 1);
	            if (ret >= 0) {
	                ts_data->edge_mode = 1;
	            }
            }
            else if (buf[0] == '2') /*USB PORTS LEFT*/ {
	            ret = fts_ex_mode_switch(MODE_EDGE, 2);
	            if (ret >= 0) {
	                ts_data->edge_mode = 2;
	            }
            }
        }
    } else if (FTS_SYSFS_ECHO_OFF(buf)) {
        if (ts_data->edge_mode) {
            FTS_DEBUG("exit edge mode");
            ret = fts_ex_mode_switch(MODE_EDGE, DISABLE);
            if (ret >= 0) {
                ts_data->edge_mode = DISABLE;
            }
        }
    }

    FTS_DEBUG("edge mode:%d", ts_data->edge_mode);
    return count;
}
/* N17 code for HQ-291087 by liunianliang at 2023/5/29 end */

/* read and write charger mode
 * read example: cat fts_glove_mode        ---read  glove mode
 * write example:echo 1 > fts_glove_mode   ---write glove mode to 01
 */
static DEVICE_ATTR(fts_glove_mode, S_IRUGO | S_IWUSR,
                   fts_glove_mode_show, fts_glove_mode_store);

static DEVICE_ATTR(fts_cover_mode, S_IRUGO | S_IWUSR,
                   fts_cover_mode_show, fts_cover_mode_store);

static DEVICE_ATTR(fts_charger_mode, S_IRUGO | S_IWUSR,
                   fts_charger_mode_show, fts_charger_mode_store);

/* N17 code for HQ-291087 by liunianliang at 2023/5/29 start */
static DEVICE_ATTR(fts_earphone_mode, S_IRUGO | S_IWUSR,
                   fts_earphone_mode_show, fts_earphone_mode_store);

static DEVICE_ATTR(fts_edge_mode, S_IRUGO | S_IWUSR,
                   fts_edge_mode_show, fts_edge_mode_store);
/* N17 code for HQ-291087 by liunianliang at 2023/5/29 end */

static struct attribute *fts_touch_mode_attrs[] = {
    &dev_attr_fts_glove_mode.attr,
    &dev_attr_fts_cover_mode.attr,
    &dev_attr_fts_charger_mode.attr,
    /* N17 code for HQ-291087 by liunianliang at 2023/5/29 start */
    &dev_attr_fts_earphone_mode.attr,
    &dev_attr_fts_edge_mode.attr,
    /* N17 code for HQ-291087 by liunianliang at 2023/5/29 end */
    NULL,
};

static struct attribute_group fts_touch_mode_group = {
    .attrs = fts_touch_mode_attrs,
};

int fts_ex_mode_recovery(struct fts_ts_data *ts_data)
{
    if (ts_data->glove_mode) {
        fts_ex_mode_switch(MODE_GLOVE, ENABLE);
    }

    if (ts_data->cover_mode) {
        fts_ex_mode_switch(MODE_COVER, ENABLE);
    }

    if (ts_data->charger_mode) {
        fts_ex_mode_switch(MODE_CHARGER, ENABLE);
    }

    /* N17 code for HQ-291087 by liunianliang at 2023/5/29 start */
    if (ts_data->earphone_mode) {
        fts_ex_mode_switch(MODE_EARPHONE, ENABLE);
    }

    if (ts_data->edge_mode) {
        fts_ex_mode_switch(MODE_EDGE, ts_data->edge_mode);
    }
    /* N17 code for HQ-291087 by liunianliang at 2023/5/29 end */

    return 0;
}

/* N17 code for HQ-291711 by liunianliang at 2023/06/03 start */
static int charger_mode_noti(struct notifier_block *nb,
			       unsigned long event, void *v)
{
    struct fts_ts_data *ts_data = container_of(nb,
                                      struct fts_ts_data, tcpc_nb);
    struct tcp_notify *noti = v;
    int ret = 0;

    switch (event) {
    case TCP_NOTIFY_TYPEC_STATE:
        if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
                    (noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
                    noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
                    noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)) {
            FTS_DEBUG("USB Plug in");
            if (!ts_data->charger_mode) {
                FTS_DEBUG("enter charger mode");
                ret = fts_ex_mode_switch(MODE_CHARGER, ENABLE);
                if (ret >= 0) {
                    ts_data->charger_mode = ENABLE;
                }
            } else {
                FTS_DEBUG("alreadly in charger mode");
            }
        } else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
                    noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
                    noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC ||
                    noti->typec_state.old_state == TYPEC_ATTACHED_AUDIO)
                        && noti->typec_state.new_state == TYPEC_UNATTACHED) {
            FTS_DEBUG("USB Plug out");
            if (ts_data->charger_mode) {
                FTS_DEBUG("exit charger mode");
                ret = fts_ex_mode_switch(MODE_CHARGER, DISABLE);
                if (ret >= 0) {
                    ts_data->charger_mode = DISABLE;
                }
            } else {
                FTS_DEBUG("alreadly exit charger mode");
            }
        } else if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
                    noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
            FTS_DEBUG("Type C headset Plug in");
            if (!ts_data->earphone_mode) {
                FTS_DEBUG("enter earphone mode");
                ret = fts_ex_mode_switch(MODE_EARPHONE, ENABLE);
                if (ret >= 0) {
                    ts_data->earphone_mode = ENABLE;
                }
            } else {
                FTS_DEBUG("alreadly in earphone mode");
            }
        } else if (noti->typec_state.old_state == TYPEC_ATTACHED_SRC &&
                    noti->typec_state.new_state == TYPEC_UNATTACHED) {
            FTS_DEBUG("Type C headset Plug out");
            if (ts_data->earphone_mode) {
                FTS_DEBUG("exit earphone mode");
                ret = fts_ex_mode_switch(MODE_EARPHONE, DISABLE);
                if (ret >= 0) {
                    ts_data->earphone_mode = DISABLE;
                }
            } else {
                FTS_DEBUG("alreadly exit earphone mode");
            }
        }
        break;
    default:
        break;
    }
    return NOTIFY_OK;
}

int earphone_mode_noti(struct notifier_block *nb,
			       unsigned long event, void *v)
{
    struct fts_ts_data *ts_data = container_of(nb,
                                      struct fts_ts_data, earphone_nb);
    struct switch_dev *noti = v;
    int ret = 0;

    FTS_DEBUG("event: %d, state: %d", event, noti->state);

    switch(event){
    case EARPHONE_MODE_STATE:
        if (noti->state) {
            if (!ts_data->earphone_mode) {
                FTS_DEBUG("enter earphone mode");
                ret = fts_ex_mode_switch(MODE_EARPHONE, ENABLE);
                if (ret >= 0) {
                    ts_data->earphone_mode = ENABLE;
                }
            }
        } else {
            if (ts_data->earphone_mode) {
                FTS_DEBUG("exit earphone mode");
                ret = fts_ex_mode_switch(MODE_EARPHONE, DISABLE);
                if (ret >= 0) {
                    ts_data->earphone_mode = DISABLE;
                }
            }
        }
        break;
    default:
        break;
    }

    return NOTIFY_OK;
}
/* N17 code for HQ-291711 by liunianliang at 2023/06/03 end */

int fts_ex_mode_init(struct fts_ts_data *ts_data)
{
    int ret = 0;

    ts_data->glove_mode = DISABLE;
    ts_data->cover_mode = DISABLE;
    ts_data->charger_mode = DISABLE;
    /* N17 code for HQ-291087 by liunianliang at 2023/5/29 start */
    ts_data->earphone_mode = DISABLE;
    ts_data->edge_mode = DISABLE;
    /* N17 code for HQ-291087 by liunianliang at 2023/5/29 end */

    ret = sysfs_create_group(&ts_data->dev->kobj, &fts_touch_mode_group);
    if (ret < 0) {
        FTS_ERROR("create sysfs(ex_mode) fail");
        sysfs_remove_group(&ts_data->dev->kobj, &fts_touch_mode_group);
        return ret;
    } else {
        FTS_DEBUG("create sysfs(ex_mode) successfully");
    }

    /* N17 code for HQ-291711 by liunianliang at 2023/06/03 start */
    ts_data->usb_psy = power_supply_get_by_name("usb");
    if (!ts_data->usb_psy) {
        FTS_ERROR("get usb psy failed !!!");
    } else {
        ts_data->tcpc = tcpc_dev_get_by_name("type_c_port0");
        if (!ts_data->tcpc) {
            FTS_ERROR("get typec device fail");
            return -PTR_ERR(ts_data->tcpc);
        }

        ts_data->tcpc_nb.notifier_call = charger_mode_noti;
        ret = register_tcp_dev_notifier(ts_data->tcpc, &ts_data->tcpc_nb,
                TCP_NOTIFY_TYPE_USB | TCP_NOTIFY_TYPE_MISC |
                TCP_NOTIFY_TYPE_VBUS);
        if (ret < 0) {
            FTS_ERROR("register tcpc notifier fail");
            return -EINVAL;
        }
    }

    ts_data->earphone_nb.notifier_call = earphone_mode_noti;
    ret = register_earphone_mode_notifier(&ts_data->earphone_nb);
    if (ret < 0) {
        FTS_ERROR("register earphone mode notifier fail");
        return -EINVAL;
    }
    /* N17 code for HQ-291711 by liunianliang at 2023/06/03 end */

    return 0;
}

int fts_ex_mode_exit(struct fts_ts_data *ts_data)
{
    sysfs_remove_group(&ts_data->dev->kobj, &fts_touch_mode_group);
    return 0;
}
