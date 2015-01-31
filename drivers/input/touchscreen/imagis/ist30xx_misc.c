/*
 *  Copyright (C) 2015 XiaoMi, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/stat.h>

#include "ist30xx.h"
#include "ist30xx_update.h"
#include "ist30xx_misc.h"

#define TSP_CH_SCREEN       (1)
#define TSP_CH_KEY          (2)

#define TOUCH_NODE_PARSING_DEBUG    (0)

int ist30xx_tkey_update_info(struct ist30xx_data *data)
{
	int ret = 0;
	u32 tkey_info1, tkey_info2, tkey_info3;
	TKEY_INFO *tkey = &data->tkey_info;

	ret = ist30xx_read_cmd(data->client, CMD_GET_KEY_INFO1, &tkey_info1);
	if (unlikely(ret)) return ret;
	ret = ist30xx_read_cmd(data->client, CMD_GET_KEY_INFO2, &tkey_info2);
	if (unlikely(ret)) return ret;
	ret = ist30xx_read_cmd(data->client, CMD_GET_KEY_INFO3, &tkey_info3);
	if (unlikely(ret)) return ret;

	tkey->enable = ((tkey_info1 & (0xFF << 24)) ? true : false);
	tkey->key_num = (tkey_info1 >> 16) & 0xFF;
	tkey->ch_num[0] = (tkey_info2 >> 24) & 0xFF;
	tkey->ch_num[1] = (tkey_info2 >> 16) & 0xFF;
	tkey->ch_num[2] = (tkey_info2 >> 8) & 0xFF;
	tkey->ch_num[3] = tkey_info2 & 0xFF;
	tkey->ch_num[4] = (tkey_info3 >> 24) & 0xFF;

	return ret;
}


#define TSP_INFO_SWAP_XY    (1 << 0)
#define TSP_INFO_FLIP_X     (1 << 1)
#define TSP_INFO_FLIP_Y     (1 << 2)
int ist30xx_tsp_update_info(struct ist30xx_data *data)
{
	int ret = 0;
	u32 tsp_ch_num, tsp_swap, tsp_dir;
	TSP_INFO *tsp = &data->tsp_info;

	ret = ist30xx_read_cmd(data->client, CMD_GET_TSP_SWAP_INFO, &tsp_swap);
	if (unlikely(ret)) return ret;

	ret = ist30xx_read_cmd(data->client, CMD_GET_TSP_DIRECTION, &tsp_dir);
	if (unlikely(ret)) return ret;

	ret = ist30xx_read_cmd(data->client, CMD_GET_TSP_CHNUM1, &tsp_ch_num);
	if (unlikely(ret || !tsp_ch_num)) return ret;

	tsp->finger_num = IST30XX_MAX_MT_FINGERS;

	tsp->ch_num.rx = tsp_ch_num >> 16;
	tsp->ch_num.tx = tsp_ch_num & 0xFFFF;

	tsp->dir.swap_xy = (tsp_swap & TSP_INFO_SWAP_XY ? true : false);
	tsp->dir.flip_x = (tsp_swap & TSP_INFO_FLIP_X ? true : false);
	tsp->dir.flip_y = (tsp_swap & TSP_INFO_FLIP_Y ? true : false);

	tsp->node.len = tsp->ch_num.tx * tsp->ch_num.rx;
	tsp->height = (tsp->dir.swap_xy ? tsp->ch_num.rx : tsp->ch_num.tx);
	tsp->width = (tsp->dir.swap_xy ? tsp->ch_num.tx : tsp->ch_num.rx);

	return ret;
}


int ist30xx_check_valid_ch(struct ist30xx_data *data, int ch_tx, int ch_rx)
{
	TKEY_INFO *tkey = &data->tkey_info;
	TSP_INFO *tsp = &data->tsp_info;

	if (unlikely((ch_tx > tsp->ch_num.tx) || (ch_rx > tsp->ch_num.rx)))
		return 0;

	if (tkey->enable) {
		if (tkey->axis_rx) {
			tsp_verb("tx: %d, rx: %d\n", ch_tx, ch_rx);
			if (ch_rx == tsp->ch_num.rx - 1) {
				tsp_verb("ch_tx: %d\n", ch_tx);
				if ((ch_tx == tkey->ch_num[0]) || (ch_tx == tkey->ch_num[1]) ||
				    (ch_tx == tkey->ch_num[2]) || (ch_tx == tkey->ch_num[3]) ||
				    (ch_tx == tkey->ch_num[4]))
					return TSP_CH_KEY;
				else
					return 0;
			}
		} else {
			if (ch_tx == tsp->ch_num.tx - 1) {
				if ((ch_rx == tkey->ch_num[0]) || (ch_rx == tkey->ch_num[1]) ||
				    (ch_rx == tkey->ch_num[2]) || (ch_rx == tkey->ch_num[3]) ||
				    (ch_rx == tkey->ch_num[4]))
					return TSP_CH_KEY;
				else
					return 0;
			}
		}
	}

	return TSP_CH_SCREEN;
}


int ist30xx_parse_touch_node(struct ist30xx_data *data, u8 flag, struct TSP_NODE_BUF *node)
{
#if TOUCH_NODE_PARSING_DEBUG
	int j;
	TSP_INFO *tsp = &data->tsp_info;
#endif
	int i;
	u16 *raw = (u16 *)&node->raw;
	u16 *base = (u16 *)&node->base;
	u16 *filter = (u16 *)&node->filter;
	u32 *rawbuf = data->frame_rawbuf;
	u32 *fltbuf = data->frame_fltbuf;

	for (i = 0; i < node->len; i++) {
		if (flag & (NODE_FLAG_RAW | NODE_FLAG_BASE)) {
			*raw++ = *rawbuf & 0xFFF;
			*base++ = (*rawbuf >> 16) & 0xFFF;

			data->frame_rawbuf++;
		}
		if (flag & NODE_FLAG_FILTER)
			*filter++ = (*fltbuf++) & 0xFFF;
	}

#if TOUCH_NODE_PARSING_DEBUG
	tsp_info("RAW - %d * %d\n", tsp->ch_num.tx, tsp->ch_num.rx);
	for (i = 0; i < tsp->ch_num.tx; i++) {
		printk("\n[ Imagis ] ");
		for (j = 0; j < tsp->ch_num.rx; j++)
			printk("%4d ", node->raw[i][j]);
	}

	tsp_info("BASE - %d * %d\n", tsp->ch_num.tx, tsp->ch_num.rx);
	for (i = 0; i < tsp->ch_num.tx; i++) {
		printk("\n[ Imagis ] ");
		for (j = 0; j < tsp->ch_num.rx; j++)
			printk("%4d ", node->base[i][j]);
	}

	tsp_info("FILTER - %d * %d\n", tsp->ch_num.tx, tsp->ch_num.rx);
	for (i = 0; i < tsp->ch_num.tx; i++) {
		printk("\n[ Imagis ] ");
		for (j = 0; j < tsp->ch_num.rx; j++)
			printk("%4d ", node->filter[i][j]);
	}
#endif

	return 0;
}

int print_touch_node(struct ist30xx_data *data, u8 flag, struct TSP_NODE_BUF *node, char *buf, bool ch_tsp)
{
	int i, j;
	int count = 0;
	int val = 0;
	const int msg_len = 128;
	char msg[msg_len];
	TSP_INFO *tsp = &data->tsp_info;

	if (tsp->dir.swap_xy) {
	} else {
		for (i = 0; i < tsp->ch_num.tx; i++) {
			for (j = 0; j < tsp->ch_num.rx; j++) {
				if (ch_tsp && (ist30xx_check_valid_ch(data, i, j) != TSP_CH_SCREEN))
					continue;

				if (flag == NODE_FLAG_RAW)
					val = (int)node->raw[i][j];
				else if (flag == NODE_FLAG_BASE)
					val = (int)node->base[i][j];
				else if (flag == NODE_FLAG_FILTER)
					val = (int)node->filter[i][j];
				else if (flag == NODE_FLAG_DIFF)
					val = (int)(node->raw[i][j] - node->base[i][j]);
				else
					return 0;

				if (val < 0) val = 0;

				count += snprintf(msg, msg_len, "%4d ", val);
				strncat(buf, msg, msg_len);
			}

			count += snprintf(msg, msg_len, "\n");
			strncat(buf, msg, msg_len);
		}
	}

	return count;
}

int parse_tsp_node(struct ist30xx_data *data, u8 flag, struct TSP_NODE_BUF *node, s16 *buf16)
{
	int i, j;
	s16 val = 0;
	TSP_INFO *tsp = &data->tsp_info;

	if (unlikely((flag != NODE_FLAG_RAW) && (flag != NODE_FLAG_BASE) &&
		     (flag != NODE_FLAG_FILTER) && (flag != NODE_FLAG_DIFF)))
		return -EPERM;

	if (tsp->dir.swap_xy) {
	} else {
		for (i = 0; i < tsp->ch_num.tx; i++) {
			for (j = 0; j < tsp->ch_num.rx; j++) {
				if (ist30xx_check_valid_ch(data, i, j) != TSP_CH_SCREEN)
					continue;

				switch ((int)flag) {
				case NODE_FLAG_RAW:
					val = (s16)node->raw[i][j];
					break;
				case NODE_FLAG_BASE:
					val = (s16)node->base[i][j];
					break;
				case NODE_FLAG_FILTER:
					val = (s16)node->filter[i][j];
					break;
				case NODE_FLAG_DIFF:
					val = (s16)(node->raw[i][j] - node->base[i][j]);
					break;
				}

				if (val < 0) val = 0;

				*buf16++ = val;
			}
		}
	}

	return 0;
}


int ist30xx_read_touch_node(struct ist30xx_data *data, u8 flag, struct TSP_NODE_BUF *node)
{
	int ret = 0;
	u32 addr = IST30XXB_RAW_ADDR;

	if (data->chip_id == IST3038_CHIP_ID)
		addr += 0xD4;

	ist30xx_disable_irq(data);

	if (flag & NODE_FLAG_NO_CCP) {
		ret = ist30xx_cmd_run_device(data->client, true);
		if (unlikely(ret)) goto read_tsp_node_end;

		ret = ist30xx_write_cmd(data->client, CMD_USE_CORRECT_CP, 0);
		if (unlikely(ret)) goto read_tsp_node_end;
	}

	ret = ist30xx_cmd_reg(data->client, CMD_ENTER_REG_ACCESS);
	if (unlikely(ret)) goto read_tsp_node_end;

	ret = ist30xx_write_cmd(data->client, IST30XX_RX_CNT_ADDR, node->len);
	if (unlikely(ret)) goto read_tsp_node_end;

	if (flag & (NODE_FLAG_RAW | NODE_FLAG_BASE)) {
		tsp_debug("Reg addr: %x, size: %d\n", addr, node->len);
		ret = ist30xx_read_buf(data->client, addr,
				       data->frame_rawbuf, node->len);
		if (unlikely(ret)) goto read_tsp_node_end;
	}
	if (flag & NODE_FLAG_FILTER) {
		tsp_debug("Reg addr: %x, size: %d\n", IST30XXB_FILTER_ADDR, node->len);
		ret = ist30xx_read_buf(data->client, IST30XXB_FILTER_ADDR,
				       data->frame_fltbuf, node->len);
		if (unlikely(ret)) goto read_tsp_node_end;
	}

	ret = ist30xx_cmd_reg(data->client, CMD_EXIT_REG_ACCESS);
	if (unlikely(ret)) goto read_tsp_node_end;

	ret = ist30xx_cmd_start_scan(data->client);
	if (unlikely(ret)) goto read_tsp_node_end;

read_tsp_node_end:
	ist30xx_enable_irq(data);

	return ret;
}


/* sysfs: /sys/class/touch/node/refresh */
ssize_t ist30xx_frame_refresh(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	int ret = 0;
	struct ist30xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;
	u8 flag = NODE_FLAG_RAW | NODE_FLAG_BASE | NODE_FLAG_FILTER;

	ret = ist30xx_read_touch_node(data, flag, &tsp->node);
	if (unlikely(ret))
		ret = sprintf(buf, "cmd 1frame raw update fail\n");

	ret = ist30xx_parse_touch_node(data, flag, &tsp->node);

	return ret;
}


/* sysfs: /sys/class/touch/node/read_nocp */
ssize_t ist30xx_frame_nocp(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	int ret = 0;
	struct ist30xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;
	u8 flag = NODE_FLAG_RAW | NODE_FLAG_BASE | NODE_FLAG_FILTER |
		  NODE_FLAG_NO_CCP;

	ret = ist30xx_read_touch_node(data, flag, &tsp->node);
	if (unlikely(ret))
		ret = sprintf(buf, "cmd 1frame raw update fail\n");

	ret = ist30xx_parse_touch_node(data, flag, &tsp->node);

	return ret;
}


/* sysfs: /sys/class/touch/node/base */
ssize_t ist30xx_base_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int count = 0;
	struct ist30xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	buf[0] = '\0';
	count = sprintf(buf, "dump ist30xxb baseline(%d)\n", tsp->node.len);

	count += print_touch_node(data, NODE_FLAG_BASE, &tsp->node, buf, false);

	return count;
}


/* sysfs: /sys/class/touch/node/raw */
ssize_t ist30xx_raw_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	int count = 0;
	struct ist30xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	buf[0] = '\0';
	count = sprintf(buf, "dump ist30xxb raw(%d)\n", tsp->node.len);

	count += print_touch_node(data, NODE_FLAG_RAW, &tsp->node, buf, false);

	return count;
}


/* sysfs: /sys/class/touch/node/diff */
ssize_t ist30xx_diff_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int count = 0;
	struct ist30xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	buf[0] = '\0';
	count = sprintf(buf, "dump ist30xxb difference (%d)\n", tsp->node.len);

	count += print_touch_node(data, NODE_FLAG_DIFF, &tsp->node, buf, false);

	return count;
}


/* sysfs: /sys/class/touch/node/filter */
ssize_t ist30xx_filter_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	int count = 0;
	struct ist30xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	buf[0] = '\0';
	count = sprintf(buf, "dump ist30xxb filter (%d)\n", tsp->node.len);

	count += print_touch_node(data, NODE_FLAG_FILTER, &tsp->node, buf, false);

	return count;
}


extern int calib_ms_delay;
/* sysfs: /sys/class/touch/sys/clb */
ssize_t ist30xx_calib_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t size)
{
	int ret = -1;
	int ms_delay;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &ms_delay);

	if (ms_delay > 10 && ms_delay < 1000) // 1sec ~ 100sec
		calib_ms_delay = ms_delay;

	tsp_info("Calibration wait time %dsec\n", calib_ms_delay / 10);

	ist30xx_disable_irq(data);
	ret = ist30xx_cmd_run_device(data->client, true);
	if (unlikely(ret)) {
		ist30xx_enable_irq(data);
		return size;
	}
	ist30xx_calibrate(data, 1);

	ist30xx_start(data);

	return size;
}

ssize_t ist30xx_calib_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	int ret;
	int count = 0;
	u32 value;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	mutex_lock(&data->ist30xx_mutex);
	ist30xx_disable_irq(data);
	ret = ist30xx_cmd_run_device(data->client, true);
	if (unlikely(ret))
		goto calib_show_end;

	ret = ist30xx_read_cmd(data->client, CMD_GET_CALIB_RESULT, &value);
	if (unlikely(ret)) {
		count = sprintf(buf, "Error Read Calibration Result\n");
		goto calib_show_end;
	}

	count = sprintf(buf,
			"Calibration Status : %d, Max raw gap : %d - (raw: %08x)\n",
			CALIB_TO_STATUS(value), CALIB_TO_GAP(value), value);

calib_show_end:
	ist30xx_start(data);
	ist30xx_enable_irq(data);
	mutex_unlock(&data->ist30xx_mutex);

	return count;
}


/* sysfs: /sys/class/touch/sys/power */
ssize_t ist30xx_power_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t size)
{
	u32 power_en;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &power_en);

	tsp_info("Power enable: %d\n", power_en);
	if (unlikely(power_en > 1)) {
		tsp_err("Unknown argument value, %d\n", power_en);
		return size;
	}

	if (power_en) {
		mutex_lock(&data->ist30xx_mutex);
		ist30xx_internal_resume(data);
		ist30xx_enable_irq(data);
		mutex_unlock(&data->ist30xx_mutex);

		ist30xx_start(data);
	} else {
		mutex_lock(&data->ist30xx_mutex);
		ist30xx_disable_irq(data);
		ist30xx_internal_suspend(data);
		mutex_unlock(&data->ist30xx_mutex);
	}

	return size;
}

extern int ist30xx_max_error_cnt;
/* sysfs: /sys/class/touch/sys/errcnt */
ssize_t ist30xx_errcnt_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	int err_cnt;

	sscanf(buf, "%d", &err_cnt);

	if (unlikely(err_cnt < 0)) return size;

	tsp_info("Request reset error count: %d\n", err_cnt);

	ist30xx_max_error_cnt = err_cnt;

	return size;
}

#if IST30XX_EVENT_MODE
extern int ist30xx_max_scan_retry;
/* sysfs: /sys/class/touch/sys/scancnt */
ssize_t ist30xx_scancnt_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	int retry;

	sscanf(buf, "%d", &retry);

	if (unlikely(retry < 0)) return size;

	tsp_info("Timer scan count retry: %d\n", retry);

	ist30xx_max_scan_retry = retry;

	return size;
}

extern int timer_period_ms;
/* sysfs: /sys/class/touch/sys/timerms */
ssize_t ist30xx_timerms_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	int ms;

	sscanf(buf, "%d", &ms);

	if (unlikely((ms < 0) || (ms > 10000))) return size;

	tsp_info("Timer period ms: %dms\n", ms);

	timer_period_ms = ms;

	return size;
}
#endif

/* sysfs: /sys/class/touch/sys/noise */
ssize_t ist30xx_noise_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t size)
{
	int noise_ms;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &noise_ms);

	if (unlikely((noise_ms < 1) || (noise_ms > 100))) return size;

	tsp_info("noise detect time : %dmsec\n", noise_ms);

	data->noise_ms = noise_ms;

	return size;
}

ssize_t ist30xx_noise_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "noise detect time : %dmsec\n", data->noise_ms);
}

/* sysfs: /sys/class/touch/sys/printk */
ssize_t ist30xx_printk_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	int level;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &level);

	if (unlikely((level < DEV_ERR) || (level > DEV_VERB))) return size;

	tsp_info("prink log level: %d\n", level);

	data->dbg_level = level;

	return size;
}

ssize_t ist30xx_printk_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "prink log level: %d\n", data->dbg_level);
}

/* sysfs: /sys/class/touch/sys/max_fingers */
ssize_t ist30xx_fingers_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	int count;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &count);

	tsp_info("max-fingers : %d\n", count);

	data->max_fingers = count;

	return size;
}

ssize_t ist30xx_fingers_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "max-fingers : %d\n", data->max_fingers);
}

/* sysfs: /sys/class/touch/sys/max_keys */
ssize_t ist30xx_keys_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	int count;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &count);

	tsp_info("max-keys : %d\n", count);

	data->max_keys = count;

	return size;
}

ssize_t ist30xx_keys_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "max-keys : %d\n", data->max_keys);
}

extern void ist30xx_scheduled_reset(struct ist30xx_data *data);
/* sysfs: /sys/class/touch/sys/touch_rate */
ssize_t ist30xx_touch_rate_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t size)
{
	int rate;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &rate);                       // us

	if (unlikely(rate > 0xFFFF)) return size;       // over 65.5ms

	tsp_info("touch reporting rate: %d\n", rate);

	data->report_rate = rate;

	ist30xx_scheduled_reset(data);

	return size;
}

/* sysfs: /sys/class/touch/sys/idle_rate */
ssize_t ist30xx_idle_scan_rate_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	int rate;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &rate);                       // us

	if (unlikely(rate > 0xFFFF)) return size;       // over 65.5ms

	tsp_info("touch idle scan rate: %d\n", rate);

	data->idle_rate = rate;

	ist30xx_scheduled_reset(data);

	return size;
}

extern void ist30xx_set_ta_mode(struct ist30xx_data *data, bool charging);
/* sysfs: /sys/class/touch/sys/mode_ta */
ssize_t ist30xx_ta_mode_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	int mode;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &mode);

	if (unlikely((mode != 0) && (mode != 1))) return size;  // enable/disable

	ist30xx_set_ta_mode(data, mode);

	return size;
}

extern void ist30xx_set_call_mode(struct ist30xx_data *data, int mode);
/* sysfs: /sys/class/touch/sys/mode_call */
ssize_t ist30xx_call_mode_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	int mode;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &mode);

	if (unlikely((mode != 0) && (mode != 1))) return size;  // enable/disable

	ist30xx_set_call_mode(data, mode);

	return size;
}

extern void ist30xx_set_cover_mode(struct ist30xx_data *data, int mode);
/* sysfs: /sys/class/touch/sys/mode_cover */
ssize_t ist30xx_cover_mode_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t size)
{
	int mode;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &mode);

	if (unlikely((mode != 0) && (mode != 1))) return size;  // enable/disable

	ist30xx_set_cover_mode(data, mode);

	return size;
}


#define TUNES_CMD_WRITE         (1)
#define TUNES_CMD_READ          (2)
#define TUNES_CMD_REG_ENTER     (3)
#define TUNES_CMD_REG_EXIT      (4)
#define TUNES_CMD_UPDATE_PARAM  (5)
#define TUNES_CMD_UPDATE_FW     (6)

#define DIRECT_ADDR(n)          (IST30XXB_DA_ADDR(n))
#define DIRECT_CMD_WRITE        ('w')
#define DIRECT_CMD_READ         ('r')

#pragma pack(1)
typedef struct {
	u8	cmd;
	u32	addr;
	u16	len;
} TUNES_INFO;
#pragma pack()
#pragma pack(1)
typedef struct {
	char	cmd;
	u32	addr;
	u32	val;
} DIRECT_INFO;
#pragma pack()

static TUNES_INFO ist30xx_tunes;
static DIRECT_INFO ist30xx_direct;

/* sysfs: /sys/class/touch/sys/direct */
ssize_t ist30xxb_direct_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	int ret = -EPERM;
	DIRECT_INFO *direct = (DIRECT_INFO *)&ist30xx_direct;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%c %x %x", &direct->cmd, &direct->addr, &direct->val);

	tsp_debug("Direct cmd: %c, addr: %x, val: %x\n",
		  direct->cmd, direct->addr, direct->val);

	if (unlikely((direct->cmd != DIRECT_CMD_WRITE) &&
		     (direct->cmd != DIRECT_CMD_READ))) {
		tsp_warn("Direct cmd is not correct!\n");
		return size;
	}

	if (ist30xx_intr_wait(data, 30) < 0) return size;

	data->status.event_mode = false;
	if (direct->cmd == DIRECT_CMD_WRITE) {
		ret = ist30xx_write_cmd(data->client, DIRECT_ADDR(direct->addr),
					direct->val);
		ret = ist30xx_read_cmd(data->client, DIRECT_ADDR(direct->addr),
				       &direct->val);
		tsp_debug("Direct write addr: %x, val: %x\n",
			  direct->addr, direct->val);
	}
	data->status.event_mode = true;

	return size;
}

#define DIRECT_BUF_COUNT        (4)
ssize_t ist30xxb_direct_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	int i, ret, count = 0;
	int len;
	u32 addr;
	u32 buf32[DIRECT_BUF_COUNT];
	int max_len = DIRECT_BUF_COUNT;
	const int msg_len = 256;
	char msg[msg_len];
	struct ist30xx_data *data = dev_get_drvdata(dev);

	DIRECT_INFO *direct = (DIRECT_INFO *)&ist30xx_direct;

	if (direct->cmd != DIRECT_CMD_READ)
		return sprintf(buf, "ex) echo r addr len > direct\n");

	len = direct->val;
	addr = DIRECT_ADDR(direct->addr);

	if (ist30xx_intr_wait(data, 30) < 0) return 0;
	data->status.event_mode = false;
	while (len > 0) {
		if (len < max_len) max_len = len;

		memset(buf32, 0, sizeof(buf32));
		ret = ist30xxb_burst_read(data->client, addr, buf32, max_len);
		if (unlikely(ret)) {
			count = sprintf(buf, "I2C Burst read fail, addr: %x\n", addr);
			break;
		}

		for (i = 0; i < max_len; i++) {
			count += snprintf(msg, msg_len, "0x%08x ", buf32[i]);
			strncat(buf, msg, msg_len);
		}
		count += snprintf(msg, msg_len, "\n");
		strncat(buf, msg, msg_len);

		addr += max_len * IST30XX_DATA_LEN;
		len -= max_len;
	}
	data->status.event_mode = true;

	tsp_debug("%s", buf);

	return count;
}

/* sysfs: /sys/class/touch/tunes/regcmd */
ssize_t tunes_regcmd_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	int ret = -1;
	u32 *buf32;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	memcpy(&ist30xx_tunes, buf, sizeof(ist30xx_tunes));
	buf += sizeof(ist30xx_tunes);
	buf32 = (u32 *)buf;

	data->tunes_cmd_done = false;

	switch (ist30xx_tunes.cmd) {
	case TUNES_CMD_WRITE:
		break;
	case TUNES_CMD_READ:
		break;
	case TUNES_CMD_REG_ENTER:
		ist30xx_disable_irq(data);
		ret = ist30xx_cmd_run_device(data->client, true);
		if (unlikely(ret)) goto regcmd_fail;

		/* enter reg access mode */
		ret = ist30xx_cmd_reg(data->client, CMD_ENTER_REG_ACCESS);
		if (unlikely(ret)) goto regcmd_fail;

		data->reg_mode = true;

		break;
	case TUNES_CMD_REG_EXIT:
		/* exit reg access mode */
		ret = ist30xx_cmd_reg(data->client, CMD_EXIT_REG_ACCESS);
		if (unlikely(ret)) goto regcmd_fail;

		ret = ist30xx_cmd_start_scan(data->client);
		if (unlikely(ret)) goto regcmd_fail;

		data->reg_mode = false;

		ist30xx_enable_irq(data);
		break;
	default:
		ist30xx_enable_irq(data);
		return size;
	}
	data->tunes_cmd_done = true;

	return size;

regcmd_fail:
	tsp_err("Tunes regcmd i2c_fail, ret=%d\n", ret);
	return size;
}

ssize_t tunes_regcmd_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int size;

	size = sprintf(buf, "cmd: 0x%02x, addr: 0x%08x, len: 0x%04x\n",
		       ist30xx_tunes.cmd, ist30xx_tunes.addr, ist30xx_tunes.len);

	return size;
}

#define MAX_WRITE_LEN   (1)
/* sysfs: /sys/class/touch/tunes/reg */
ssize_t tunes_reg_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t size)
{
	int ret;
	u32 *buf32 = (u32 *)buf;
	int waddr, wcnt = 0, len = 0;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	if (unlikely(ist30xx_tunes.cmd != TUNES_CMD_WRITE)) {
		tsp_err("error, IST30XX_REG_CMD is not correct!\n");
		return size;
	}

	if (unlikely(!data->reg_mode)) {
		tsp_err("error, IST30XX_REG_CMD is not ready!\n");
		return size;
	}

	if (unlikely(!data->tunes_cmd_done)) {
		tsp_err("error, IST30XX_REG_CMD is not ready!\n");
		return size;
	}

	waddr = ist30xx_tunes.addr;
	if (ist30xx_tunes.len >= MAX_WRITE_LEN)
		len = MAX_WRITE_LEN;
	else
		len = ist30xx_tunes.len;

	while (wcnt < ist30xx_tunes.len) {
		ret = ist30xx_write_buf(data->client, waddr, buf32, len);
		if (unlikely(ret)) {
			tsp_err("Tunes regstore i2c_fail, ret=%d\n", ret);
			return size;
		}

		wcnt += len;

		if ((ist30xx_tunes.len - wcnt) < MAX_WRITE_LEN)
			len = ist30xx_tunes.len - wcnt;

		buf32 += MAX_WRITE_LEN;
		waddr += MAX_WRITE_LEN * IST30XX_DATA_LEN;
	}

	data->tunes_cmd_done = false;

	return size;
}

ssize_t tunes_reg_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	int ret;
	int size;
	u32 *buf32 = (u32 *)buf;
	struct ist30xx_data *data = dev_get_drvdata(dev);

#if I2C_MONOPOLY_MODE
	unsigned long flags;
#endif

	if (unlikely(ist30xx_tunes.cmd != TUNES_CMD_READ)) {
		tsp_err("error, IST30XX_REG_CMD is not correct!\n");
		return 0;
	}

	if (unlikely(!data->tunes_cmd_done)) {
		tsp_err("error, IST30XX_REG_CMD is not ready!\n");
		return 0;
	}

	size = ist30xx_tunes.len;
	ret = ist30xx_write_cmd(data->client, IST30XX_RX_CNT_ADDR, size);
	if (unlikely(ret)) {
		tsp_err("Tunes regshow i2c_fail, ret=%d\n", ret);
		return 0;
	}

#if I2C_MONOPOLY_MODE
	local_irq_save(flags);          // Activated only when the GPIO I2C is used
#endif
	ret = ist30xx_read_buf(data->client, ist30xx_tunes.addr, buf32, size);
#if I2C_MONOPOLY_MODE
	local_irq_restore(flags);       // Activated only when the GPIO I2C is used
#endif
	if (unlikely(ret)) {
		tsp_err("Tunes regshow i2c_fail, ret=%d\n", ret);
		return size;
	}

	size = ist30xx_tunes.len * IST30XX_DATA_LEN;

	data->tunes_cmd_done = false;

	return size;
}


/* sysfs: /sys/class/touch/tunes/firmware */
ssize_t tunes_fw_store(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t size)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	ist30xx_get_update_info(data, buf, size);

	mutex_lock(&data->ist30xx_mutex);
	ist30xx_fw_update(data->client, buf, size, true);
	mutex_unlock(&data->ist30xx_mutex);

	ist30xx_calibrate(data, 1);

	ist30xx_init_touch_driver(data);

	return size;
}


/* sysfs: /sys/class/touch/tunes/adb */
ssize_t tunes_adb_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t size)
{
	int ret;
	char *tmp, *ptr;
	char token[9];
	u32 cmd, addr, len, val;
	int write_len;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%x %x %x", &cmd, &addr, &len);

	switch (cmd) {
	case TUNES_CMD_WRITE:   /* write cmd */
		write_len = 0;
		ptr = (char *)(buf + 15);

		while (write_len < len) {
			memcpy(token, ptr, 8);
			token[8] = 0;
			val = simple_strtoul(token, &tmp, 16);
			ret = ist30xx_write_buf(data->client, addr, &val, 1);
			if (unlikely(ret)) {
				tsp_err("Tunes regstore i2c_fail, ret=%d\n", ret);
				return size;
			}

			ptr += 8;
			write_len++;
			addr += 4;
		}
		break;

	case TUNES_CMD_READ:   /* read cmd */
		ist30xx_tunes.cmd = cmd;
		ist30xx_tunes.addr = addr;
		ist30xx_tunes.len = len;
		break;

	case TUNES_CMD_REG_ENTER:   /* enter */
		ist30xx_disable_irq(data);
		ret = ist30xx_cmd_run_device(data->client, true);
		if (unlikely(ret < 0)) goto cmd_fail;

		ret = ist30xx_cmd_reg(data->client, CMD_ENTER_REG_ACCESS);
		if (unlikely(ret < 0)) goto cmd_fail;
		data->reg_mode = true;
		break;

	case TUNES_CMD_REG_EXIT:   /* exit */
		if (data->reg_mode == true) {
			ret = ist30xx_cmd_reg(data->client, CMD_EXIT_REG_ACCESS);
			if (unlikely(ret < 0)) goto cmd_fail;

			ret = ist30xx_cmd_start_scan(data->client);
			if (unlikely(ret < 0)) goto cmd_fail;
			data->reg_mode = false;
			ist30xx_enable_irq(data);
		}
		break;

	default:
		break;
	}

	return size;

cmd_fail:
	tsp_err("Tunes adb i2c_fail\n");
	return size;
}

ssize_t tunes_adb_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	int ret;
	int i, len, size = 0;
	char reg_val[10];
	struct ist30xx_data *data = dev_get_drvdata(dev);

#if I2C_MONOPOLY_MODE
	unsigned long flags;
#endif

	ret = ist30xx_write_cmd(data->client, IST30XX_RX_CNT_ADDR, ist30xx_tunes.len);
	if (unlikely(ret)) {
		tsp_err("Tunes regshow i2c_fail, ret=%d\n", ret);
		return size;
	}

#if I2C_MONOPOLY_MODE
	local_irq_save(flags);
#endif
	ret = ist30xx_read_buf(data->client, ist30xx_tunes.addr,
			       data->frame_buf, ist30xx_tunes.len);
#if I2C_MONOPOLY_MODE
	local_irq_restore(flags);
#endif
	if (unlikely(ret)) {
		tsp_err("Tunes regshow i2c_fail, ret=%d\n", ret);
		return size;
	}

	size = 0;
	buf[0] = 0;
	len = sprintf(reg_val, "%08x", ist30xx_tunes.addr);
	strcat(buf, reg_val);
	size += len;
	for (i = 0; i < ist30xx_tunes.len; i++) {
		len = sprintf(reg_val, "%08x", data->frame_buf[i]);
		strcat(buf, reg_val);
		size += len;
	}

	return size;
}

#if IST30XX_ALGORITHM_MODE
/* sysfs: /sys/class/touch/tunes/algorithm */
ssize_t ist30xx_algr_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%x %d", &data->algr_addr, &data->algr_size);
	tsp_info("Algorithm addr: 0x%x, count: %d\n",
		 data->algr_addr, data->algr_size);

	data->algr_addr |= IST30XXB_ACCESS_ADDR;

	return size;
}

ssize_t ist30xx_algr_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int ret;
	u32 algr_addr;
	int count = 0;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	ret = ist30xx_read_cmd(data->client, IST30XXB_MEM_ALGORITHM, &algr_addr);
	if (unlikely(ret)) {
		tsp_warn("Algorithm mem addr read fail!\n");
		return 0;
	}

	tsp_info("algr_addr(0x%x): 0x%x\n", IST30XXB_MEM_ALGORITHM, algr_addr);

	count = sprintf(buf, "Algorithm addr : 0x%x\n", algr_addr);

	return count;
}
#endif // IST30XX_ALGORITHM_MODE

/* sysfs: /sys/class/touch/tunes/intr_debug */
ssize_t intr_debug_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t size)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%x %d", &data->intr_debug_addr, &data->intr_debug_size);
	tsp_info("Interrupt debug addr: 0x%x, count: %d\n",
		 data->intr_debug_addr, data->intr_debug_size);

	data->intr_debug_addr |= IST30XXB_ACCESS_ADDR;

	return size;
}

ssize_t intr_debug_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int count = 0;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	tsp_info("intr_debug_addr(0x%x): %d\n",
		 data->intr_debug_addr, data->intr_debug_size);

	count = sprintf(buf, "intr_debug_addr(0x%x): %d\n",
			data->intr_debug_addr, data->intr_debug_size);

	return count;
}


/* sysfs : node */
static DEVICE_ATTR(refresh, S_IRUGO, ist30xx_frame_refresh, NULL);
static DEVICE_ATTR(nocp, S_IRUGO, ist30xx_frame_nocp, NULL);
static DEVICE_ATTR(filter, S_IRUGO, ist30xx_filter_show, NULL);
static DEVICE_ATTR(raw, S_IRUGO, ist30xx_raw_show, NULL);
static DEVICE_ATTR(base, S_IRUGO, ist30xx_base_show, NULL);
static DEVICE_ATTR(diff, S_IRUGO, ist30xx_diff_show, NULL);

/* sysfs : sys */
static DEVICE_ATTR(noise, (S_IRUGO | S_IWUSR | S_IWGRP), ist30xx_noise_show, ist30xx_noise_store);
static DEVICE_ATTR(printk, (S_IRUGO | S_IWUSR | S_IWGRP), ist30xx_printk_show, ist30xx_printk_store);
static DEVICE_ATTR(direct, (S_IRUGO | S_IWUSR | S_IWGRP), ist30xxb_direct_show, ist30xxb_direct_store);
static DEVICE_ATTR(clb, (S_IRUGO | S_IWUSR | S_IWGRP), ist30xx_calib_show, ist30xx_calib_store);
static DEVICE_ATTR(tsp_power, (S_IRUGO | S_IWUSR | S_IWGRP), NULL, ist30xx_power_store);
static DEVICE_ATTR(errcnt, (S_IRUGO | S_IWUSR | S_IWGRP), NULL, ist30xx_errcnt_store);
#if IST30XX_EVENT_MODE
static DEVICE_ATTR(scancnt, (S_IRUGO | S_IWUSR | S_IWGRP), NULL, ist30xx_scancnt_store);
static DEVICE_ATTR(timerms, (S_IRUGO | S_IWUSR | S_IWGRP), NULL, ist30xx_timerms_store);
#endif
static DEVICE_ATTR(report_rate, (S_IRUGO | S_IWUSR | S_IWGRP), NULL, ist30xx_touch_rate_store);
static DEVICE_ATTR(idle_rate, (S_IRUGO | S_IWUSR | S_IWGRP), NULL, ist30xx_idle_scan_rate_store);
static DEVICE_ATTR(mode_ta, (S_IRUGO | S_IWUSR | S_IWGRP), NULL, ist30xx_ta_mode_store);
static DEVICE_ATTR(mode_call, (S_IRUGO | S_IWUSR | S_IWGRP), NULL, ist30xx_call_mode_store);
static DEVICE_ATTR(mode_cover, (S_IRUGO | S_IWUSR | S_IWGRP), NULL, ist30xx_cover_mode_store);
static DEVICE_ATTR(max_fingers, (S_IRUGO | S_IWUSR | S_IWGRP), ist30xx_fingers_show, ist30xx_fingers_store);
static DEVICE_ATTR(max_keys, (S_IRUGO | S_IWUSR | S_IWGRP), ist30xx_keys_show, ist30xx_keys_store);

/* sysfs : tunes */
static DEVICE_ATTR(regcmd, (S_IRUGO | S_IWUSR | S_IWGRP), tunes_regcmd_show, tunes_regcmd_store);
static DEVICE_ATTR(reg, (S_IRUGO | S_IWUSR | S_IWGRP), tunes_reg_show, tunes_reg_store);
static DEVICE_ATTR(tunes_fw, (S_IRUGO | S_IWUSR | S_IWGRP), NULL, tunes_fw_store);
static DEVICE_ATTR(adb, (S_IRUGO | S_IWUSR | S_IWGRP), tunes_adb_show, tunes_adb_store);
#if IST30XX_ALGORITHM_MODE
static DEVICE_ATTR(algorithm, (S_IRUGO | S_IWUSR | S_IWGRP), ist30xx_algr_show, ist30xx_algr_store);
#endif
static DEVICE_ATTR(intr_debug, (S_IRUGO | S_IWUSR | S_IWGRP), intr_debug_show, intr_debug_store);

static struct attribute *node_attributes[] = {
	&dev_attr_refresh.attr,
	&dev_attr_nocp.attr,
	&dev_attr_filter.attr,
	&dev_attr_raw.attr,
	&dev_attr_base.attr,
	&dev_attr_diff.attr,
	NULL,
};

static struct attribute *sys_attributes[] = {
	&dev_attr_noise.attr,
	&dev_attr_printk.attr,
	&dev_attr_direct.attr,
	&dev_attr_clb.attr,
	&dev_attr_tsp_power.attr,
	&dev_attr_errcnt.attr,
#if IST30XX_EVENT_MODE
	&dev_attr_scancnt.attr,
	&dev_attr_timerms.attr,
#endif
	&dev_attr_report_rate.attr,
	&dev_attr_idle_rate.attr,
	&dev_attr_mode_ta.attr,
	&dev_attr_mode_call.attr,
	&dev_attr_mode_cover.attr,
	&dev_attr_max_fingers.attr,
	&dev_attr_max_keys.attr,
	NULL,
};

static struct attribute *tunes_attributes[] = {
	&dev_attr_regcmd.attr,
	&dev_attr_reg.attr,
	&dev_attr_tunes_fw.attr,
	&dev_attr_adb.attr,
#if IST30XX_ALGORITHM_MODE
	&dev_attr_algorithm.attr,
#endif
	&dev_attr_intr_debug.attr,
	NULL,
};

static struct attribute_group node_attr_group = {
	.attrs	= node_attributes,
};

static struct attribute_group sys_attr_group = {
	.attrs	= sys_attributes,
};

static struct attribute_group tunes_attr_group = {
	.attrs	= tunes_attributes,
};

int ist30xx_init_misc_sysfs(struct ist30xx_data *data)
{
	/* /sys/class/touch/sys */
	data->sys_dev = device_create(data->ist30xx_class, NULL, 0, data, "sys");

	/* /sys/class/touch/sys/... */
	if (sysfs_create_group(&data->sys_dev->kobj, &sys_attr_group))
		tsp_err("Failed to create sysfs group(%s)!\n", "sys");

	/* /sys/class/touch/tunes */
	data->tunes_dev = device_create(data->ist30xx_class, NULL, 0, data, "tunes");

	/* /sys/class/touch/tunes/... */
	if (unlikely(sysfs_create_group(&data->tunes_dev->kobj,
					&tunes_attr_group)))
		tsp_err("Failed to create sysfs group(%s)!\n", "tunes");

	/* /sys/class/touch/node */
	data->node_dev = device_create(data->ist30xx_class, NULL, 0, data, "node");

	/* /sys/class/touch/node/... */
	if (unlikely(sysfs_create_group(&data->node_dev->kobj,
					&node_attr_group)))
		tsp_err("Failed to create sysfs group(%s)!\n", "node");


	data->frame_buf = kmalloc(4096, GFP_KERNEL);
	data->frame_rawbuf = kmalloc(4096, GFP_KERNEL);
	data->frame_fltbuf = kmalloc(4096, GFP_KERNEL);
	if (!data->frame_buf || !data->frame_rawbuf || !data->frame_fltbuf)
		return -ENOMEM;

	return 0;
}
