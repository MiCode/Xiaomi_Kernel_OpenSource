/*
 *  Copyright (C) 2010, Imagis Technology Co. Ltd. All Rights Reserved.
 *  Copyright (C) 2016 XiaoMi, Inc.
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
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/err.h>

#include "ist30xxc.h"
#include "ist30xxc_cmcs.h"

#if IST30XX_CMCS_TEST
#if INTERNAL_CMCS_BIN
#include "ist30xxc_cmcs_bin.h"
#endif

#ifdef WT_CTP_OPEN_SHORT_TEST
#define ITO_TEST_AUTHORITY   0777
#define IST_PROC_MSG_ITO_TESE			"touchscreen"
#define IST_PROC_ITO_OPENTEST_DEBUG	  "ctp_openshort_test"
#define CTP_LOCKDOWN_INFOR_NAME		  "lockdown_info"


extern  struct ist30xx_data *global_ts_data;
extern lockinfoH Lockdown_Info_High;
extern lockinfoL Lockdown_Info_LOW;
#endif

#define TSP_CH_UNUSED			   (0)
#define TSP_CH_SCREEN			   (1)
#define TSP_CH_GTX				  (2)
#define TSP_CH_KEY				  (3)
#define TSP_CH_UNKNOWN			  (-1)

#define CMCS_FLAG_CM				(1 << 0)
#define CMCS_FLAG_CM_SPEC		   (1 << 1)
#define CMCS_FLAG_CM_SLOPE0		 (1 << 2)
#define CMCS_FLAG_CM_SLOPE1		 (1 << 3)
#define CMCS_FLAG_CMJIT			 (1 << 4)
#define CMCS_FLAG_CS				(1 << 5)
#define CMCS_FLAG_INT			   (1 << 6)

#define CMCS_ENABLE_CM			  (1 << 0)
#define CMCS_ENABLE_CS			  (1 << 8)
#define CMCS_ENABLE_INT			 (1 << 16)
#define CMCS_ENABLE_CMJIT		   (1 << 24)

#define CMCS_READY				  (0)
#define CMCS_NOT_READY			  (-1)

#define CMCS_TIMEOUT				(10000)

#define CMCS_CMJIT				  ("CMJIT")
#define CMCS_CM					 ("CM")
#define CMCS_CS					 ("CS")

#define CMCS_PARSING_DEBUG		  (0)
#define CMCS_TAGS_PARSE_OK		  (0)

int cmcs_ready = CMCS_READY;
u8 *ts_cmcs_bin = NULL;
u32 ts_cmcs_bin_size = 0;
CMCS_BIN_INFO ist30xx_cmcs_bin;
CMCS_BIN_INFO *ts_cmcs = (CMCS_BIN_INFO *)&ist30xx_cmcs_bin;
CMCS_BUF ist30xx_cmcs_buf;
CMCS_BUF *ts_cmcs_buf = (CMCS_BUF *)&ist30xx_cmcs_buf;

int ist30xx_parse_cmcs_bin(const u8 *buf, const u32 size)
{
	int ret = -EPERM;
	int i;
	int node_spec_cnt;

	memcpy(ts_cmcs->magic1, buf, sizeof(ts_cmcs->magic1));
	memcpy(ts_cmcs->magic2, &buf[size - sizeof(ts_cmcs->magic2)],
		   sizeof(ts_cmcs->magic2));

	if (!strncmp(ts_cmcs->magic1, IST30XX_CMCS_MAGIC, sizeof(ts_cmcs->magic1))
		&& !strncmp(ts_cmcs->magic2, IST30XX_CMCS_MAGIC,
			sizeof(ts_cmcs->magic2))) {
		int idx;

		idx = sizeof(ts_cmcs->magic1);

		memcpy(&ts_cmcs->items.cnt, &buf[idx], sizeof(ts_cmcs->items.cnt));
		idx += sizeof(ts_cmcs->items.cnt);
		ts_cmcs->items.item = kmalloc(
			sizeof(struct CMCS_ITEM_INFO) * ts_cmcs->items.cnt, GFP_KERNEL);
		for (i = 0; i < ts_cmcs->items.cnt; i++) {
			memcpy(&ts_cmcs->items.item[i], &buf[idx],
				   sizeof(struct CMCS_ITEM_INFO));
			idx += sizeof(struct CMCS_ITEM_INFO);
		}

		memcpy(&ts_cmcs->cmds.cnt, &buf[idx], sizeof(ts_cmcs->cmds.cnt));
		idx += sizeof(ts_cmcs->cmds.cnt);
		ts_cmcs->cmds.cmd = kmalloc(
			sizeof(struct CMCS_CMD_INFO) * ts_cmcs->cmds.cnt, GFP_KERNEL);
		for (i = 0; i < ts_cmcs->cmds.cnt; i++) {
			memcpy(&ts_cmcs->cmds.cmd[i], &buf[idx],
				   sizeof(struct CMCS_CMD_INFO));
			idx += sizeof(struct CMCS_CMD_INFO);
		}

		memcpy(&ts_cmcs->spec_slope, &buf[idx], sizeof(ts_cmcs->spec_slope));
		idx += sizeof(ts_cmcs->spec_slope);

		memcpy(&ts_cmcs->spec_cr, &buf[idx], sizeof(ts_cmcs->spec_cr));
		idx += sizeof(ts_cmcs->spec_cr);

		memcpy(&ts_cmcs->param, &buf[idx], sizeof(ts_cmcs->param));
		idx += sizeof(ts_cmcs->param);

		ts_cmcs->spec_item = kmalloc(
			sizeof(struct CMCS_SPEC_TOTAL) * ts_cmcs->items.cnt,
			GFP_KERNEL);
		for (i = 0; i < ts_cmcs->items.cnt; i++) {
			if (!strcmp(ts_cmcs->items.item[i].spec_type, "N")) {
				memcpy(&node_spec_cnt, &buf[idx], sizeof(node_spec_cnt));
				ts_cmcs->spec_item[i].spec_node.node_cnt = node_spec_cnt;
				idx += sizeof(node_spec_cnt);
				ts_cmcs->spec_item[i].spec_node.buf_min = (u16 *)&buf[idx];
				idx += node_spec_cnt * sizeof(s16);
				ts_cmcs->spec_item[i].spec_node.buf_max = (u16 *)&buf[idx];
				idx += node_spec_cnt * sizeof(s16);
			} else if (!strcmp(ts_cmcs->items.item[i].spec_type, "T")) {
				memcpy(&ts_cmcs->spec_item[i].spec_total, &buf[idx],
					   sizeof(struct CMCS_SPEC_TOTAL));
				idx += sizeof(struct CMCS_SPEC_TOTAL);
			}
		}

		ts_cmcs->buf_cmcs = (u8 *)&buf[idx];
		idx += ts_cmcs->param.cmcs_size;

		ts_cmcs->buf_cm_sensor = (u32 *)&buf[idx];
		idx += ts_cmcs->param.cm_sensor1_size + ts_cmcs->param.cm_sensor2_size
			   + ts_cmcs->param.cm_sensor3_size;

		ts_cmcs->buf_cs_sensor = (u32 *)&buf[idx];
		idx += ts_cmcs->param.cs_sensor1_size + ts_cmcs->param.cs_sensor2_size
			   + ts_cmcs->param.cs_sensor3_size;

		ts_cmcs->version = *((u32 *)&buf[idx]);

		ret = 0;
	}

	tsp_verb("Magic1: %s, Magic2: %s\n", ts_cmcs->magic1, ts_cmcs->magic2);
	tsp_verb("CmCs ver: %08X\n", ts_cmcs->version);
	tsp_verb(" item(%d)\n", ts_cmcs->items.cnt);
	for (i = 0; i < ts_cmcs->items.cnt; i++) {
		tsp_verb(" (%d): %s, 0x%08x, %d, %s, %s\n",
			 i, ts_cmcs->items.item[i].name, ts_cmcs->items.item[i].addr,
			 ts_cmcs->items.item[i].size, ts_cmcs->items.item[i].data_type,
			 ts_cmcs->items.item[i].spec_type);
	}
	tsp_verb(" cmd(%d)\n", ts_cmcs->cmds.cnt);
	for (i = 0; i < ts_cmcs->cmds.cnt; i++)
		tsp_verb(" (%d): 0x%08x, 0x%08x\n",
			 i, ts_cmcs->cmds.cmd[i].addr, ts_cmcs->cmds.cmd[i].value);
	tsp_verb(" param\n");
	tsp_verb("  fw: 0x%08x, %d\n", ts_cmcs->param.cmcs_size_addr,
		 ts_cmcs->param.cmcs_size);
	tsp_verb("  enable: 0x%08x\n", ts_cmcs->param.enable_addr);
	tsp_verb("  checksum: 0x%08x\n", ts_cmcs->param.checksum_addr);
	tsp_verb("  endnotify: 0x%08x\n", ts_cmcs->param.end_notify_addr);
	tsp_verb("  cm sensor1: 0x%08x, %d\n", ts_cmcs->param.sensor1_addr,
		 ts_cmcs->param.cm_sensor1_size);
	tsp_verb("  cm sensor2: 0x%08x, %d\n", ts_cmcs->param.sensor2_addr,
		 ts_cmcs->param.cm_sensor2_size);
	tsp_verb("  cm sensor3: 0x%08x, %d\n", ts_cmcs->param.sensor3_addr,
		 ts_cmcs->param.cm_sensor3_size);
	tsp_verb("  cs sensor1: 0x%08x, %d\n", ts_cmcs->param.sensor1_addr,
		 ts_cmcs->param.cs_sensor1_size);
	tsp_verb("  cs sensor2: 0x%08x, %d\n", ts_cmcs->param.sensor2_addr,
		 ts_cmcs->param.cs_sensor2_size);
	tsp_verb("  cs sensor3: 0x%08x, %d\n", ts_cmcs->param.sensor3_addr,
		 ts_cmcs->param.cs_sensor3_size);
	tsp_verb("  chksum: 0x%08x\n, 0x%08x\n, 0x%08x\n",
		 ts_cmcs->param.cmcs_chksum, ts_cmcs->param.cm_sensor_chksum,
		 ts_cmcs->param.cs_sensor_chksum);
	tsp_verb("  cs result addr(tx, rx): 0x%08x, 0x%08x\n",
		 ts_cmcs->param.cs_tx_result_addr, ts_cmcs->param.cs_rx_result_addr);
	tsp_verb(" slope(%s)\n", ts_cmcs->spec_slope.name);
	tsp_verb("  x(%d, %d), y(%d, %d), gtx_x(%d, %d), gtx_y(%d, %d), key(%d, %d)\n",
		 ts_cmcs->spec_slope.x_min, ts_cmcs->spec_slope.x_max,
		 ts_cmcs->spec_slope.y_min, ts_cmcs->spec_slope.y_max,
		 ts_cmcs->spec_slope.gtx_x_min, ts_cmcs->spec_slope.gtx_x_max,
		 ts_cmcs->spec_slope.gtx_y_min, ts_cmcs->spec_slope.gtx_y_max,
		 ts_cmcs->spec_slope.key_min, ts_cmcs->spec_slope.key_max);
	tsp_verb(" cr: screen(%4d, %4d), gtx(%4d, %4d), key(%4d, %4d)\n",
		 ts_cmcs->spec_cr.screen_min, ts_cmcs->spec_cr.screen_max,
		 ts_cmcs->spec_cr.gtx_min, ts_cmcs->spec_cr.gtx_max,
		 ts_cmcs->spec_cr.key_min, ts_cmcs->spec_cr.key_max);
	for (i = 0; i < ts_cmcs->items.cnt; i++) {
		if (!strcmp(ts_cmcs->items.item[i].spec_type, "N")) {
			tsp_verb(" %s\n", ts_cmcs->items.item[i].name);
			tsp_verb(" min: %x, %x, %x, %x\n",
				 ts_cmcs->spec_item[i].spec_node.buf_min[0],
				 ts_cmcs->spec_item[i].spec_node.buf_min[1],
				 ts_cmcs->spec_item[i].spec_node.buf_min[2],
				 ts_cmcs->spec_item[i].spec_node.buf_min[3]);
			tsp_verb(" max: %x, %x, %x, %x\n",
				 ts_cmcs->spec_item[i].spec_node.buf_max[0],
				 ts_cmcs->spec_item[i].spec_node.buf_max[1],
				 ts_cmcs->spec_item[i].spec_node.buf_max[2],
				 ts_cmcs->spec_item[i].spec_node.buf_max[3]);
		} else if (!strcmp(ts_cmcs->items.item[i].spec_type, "T")) {
			tsp_verb(" %s: screen(%4d, %4d), gtx(%4d, %4d), key(%4d, %4d)\n",
				 ts_cmcs->items.item[i].name,
				 ts_cmcs->spec_item[i].spec_total.screen_min,
				 ts_cmcs->spec_item[i].spec_total.screen_max,
				 ts_cmcs->spec_item[i].spec_total.gtx_min,
				 ts_cmcs->spec_item[i].spec_total.gtx_max,
				 ts_cmcs->spec_item[i].spec_total.key_min,
				 ts_cmcs->spec_item[i].spec_total.key_max);
		}
	}
	tsp_verb(" cmcs: %x, %x, %x, %x\n", ts_cmcs->buf_cmcs[0],
		 ts_cmcs->buf_cmcs[1], ts_cmcs->buf_cmcs[2], ts_cmcs->buf_cmcs[3]);
	tsp_verb(" cm sensor: %x, %x, %x, %x\n",
		 ts_cmcs->buf_cm_sensor[0], ts_cmcs->buf_cm_sensor[1],
		 ts_cmcs->buf_cm_sensor[2], ts_cmcs->buf_cm_sensor[3]);
	tsp_verb(" cs sensor: %x, %x, %x, %x\n",
		 ts_cmcs->buf_cs_sensor[0], ts_cmcs->buf_cs_sensor[1],
		 ts_cmcs->buf_cs_sensor[2], ts_cmcs->buf_cs_sensor[3]);

	return ret;
}

int ist30xx_get_cmcs_info(const u8 *buf, const u32 size)
{
	int ret;

	cmcs_ready = CMCS_NOT_READY;

	ret = ist30xx_parse_cmcs_bin(buf, size);
	if (unlikely(ret != CMCS_TAGS_PARSE_OK))
		tsp_warn("Cannot find tags of CMCS, make a bin by 'cmcs2bin.exe'\n");

	return ret;
}

int ist30xx_set_cmcs_sensor(struct i2c_client *client, CMCS_PARAM param,
				u32 *buf32, int mode)
{
	int ret;
	int len = 0;
	u32 waddr;

	if ((mode == CMCS_FLAG_CM) || (mode == CMCS_FLAG_CMJIT)) {
		waddr = IST30XX_DA_ADDR(param.sensor1_addr);
		len = (param.cm_sensor1_size / IST30XX_DATA_LEN) - 2;
		buf32 += 2;
		tsp_verb("%08x %08x %08x %08x\n",
			 buf32[0], buf32[1], buf32[2], buf32[3]);
		tsp_verb("%08x(%d)\n", waddr, len);

		if (len > 0) {
			ret = ist30xx_burst_write(client, waddr, buf32, len);
			if (ret)
				return ret;

			tsp_info("cm sensor reg1 loaded!\n");
		}

		buf32 += len;
		waddr = IST30XX_DA_ADDR(param.sensor2_addr);
		len = param.cm_sensor2_size / IST30XX_DATA_LEN;
		tsp_verb("%08x %08x %08x %08x\n",
			 buf32[0], buf32[1], buf32[2], buf32[3]);
		tsp_verb("%08x(%d)\n", waddr, len);

		if (len > 0) {
			ret = ist30xx_burst_write(client, waddr, buf32, len);
			if (ret)
				return ret;

			tsp_info("cm sensor reg2 loaded!\n");
		}

		buf32 += len;
		waddr = IST30XX_DA_ADDR(param.sensor3_addr);
		len = param.cm_sensor3_size / IST30XX_DATA_LEN;
		tsp_verb("%08x %08x %08x %08x\n",
			 buf32[0], buf32[1], buf32[2], buf32[3]);
		tsp_verb("%08x(%d)\n", waddr, len);

		if (len > 0) {
			ret = ist30xx_burst_write(client, waddr, buf32, len);
			if (ret)
				return ret;

			tsp_info("cm sensor reg3 loaded!\n");
		}
	} else if (mode == CMCS_FLAG_CS) {
		waddr = IST30XX_DA_ADDR(param.sensor1_addr);
		len = (param.cs_sensor1_size / IST30XX_DATA_LEN) - 2;
		buf32 += 2;
		tsp_verb("%08x %08x %08x %08x\n",
			 buf32[0], buf32[1], buf32[2], buf32[3]);
		tsp_verb("%08x(%d)\n", waddr, len);

		if (len > 0) {
			ret = ist30xx_burst_write(client, waddr, buf32, len);
			if (ret)
				return ret;

			tsp_info("cs sensor reg1 loaded!\n");
		}

		buf32 += len;
		waddr = IST30XX_DA_ADDR(param.sensor2_addr);
		len = param.cs_sensor2_size / IST30XX_DATA_LEN;
		tsp_verb("%08x %08x %08x %08x\n",
			 buf32[0], buf32[1], buf32[2], buf32[3]);
		tsp_verb("%08x(%d)\n", waddr, len);

		if (len > 0) {
			ret = ist30xx_burst_write(client, waddr, buf32, len);
			if (ret)
				return ret;

			tsp_info("cs sensor reg2 loaded!\n");
		}

		buf32 += len;
		waddr = IST30XX_DA_ADDR(param.sensor3_addr);
		len = param.cs_sensor3_size / IST30XX_DATA_LEN;
		tsp_verb("%08x %08x %08x %08x\n",
			 buf32[0], buf32[1], buf32[2], buf32[3]);
		tsp_verb("%08x(%d)\n", waddr, len);

		if (len > 0) {
			ret = ist30xx_burst_write(client, waddr, buf32, len);
			if (ret)
				return ret;

			tsp_info("cs sensor reg3 loaded!\n");
		}
	}

	return 0;
}

int ist30xx_set_cmcs_cmd(struct ist30xx_data *data, CMCS_CMD cmds)
{
	int ret;
	int i;
	u32 val;
	u32 waddr;

	for (i = 0; i < cmds.cnt; i++) {
		waddr = IST30XX_DA_ADDR(cmds.cmd[i].addr);
		val = cmds.cmd[i].value;
		ret = ist30xx_write_cmd(data, waddr, val);
		if (ret)
			return ret;
		tsp_verb("cmd%d(0x%08x): 0x%08x\n", i, waddr, val);
	}

	return 0;
}

int ist30xx_parse_cmcs_buf(struct ist30xx_data *data, s16 *buf)
{
	int i, j;
	TSP_INFO *tsp = &data->tsp_info;

	tsp_info(" %d * %d\n", tsp->ch_num.tx, tsp->ch_num.rx);
	for (i = 0; i < tsp->ch_num.tx; i++) {
		tsp_info(" ");
		for (j = 0; j < tsp->ch_num.rx; j++)
			printk("%5d ", buf[i * tsp->ch_num.rx + j]);
		printk("\n");
	}

	return 0;
}

int ist30xx_apply_cmcs_slope(struct ist30xx_data *data, CMCS_BUF *cmcs_buf)
{
	int i, j, k;
	int idx1, idx2, gtx_idx, key_idx;
	int type;
	int success = false;
	TSP_INFO *tsp = &data->tsp_info;
	int width = tsp->screen.rx;
	int height = tsp->screen.tx;
	s16 *presult;
#if IST30XX_SDC_SLOPE
	s16 slope_x = 0;
	s16 slope_y = 0;
#else
	s16 *pspec_min = (s16 *)cmcs_buf->spec_min;
	s16 *pspec_max = (s16 *)cmcs_buf->spec_max;
#endif
	s16 *pslope0 = (s16 *)cmcs_buf->slope0;
	s16 *pslope1 = (s16 *)cmcs_buf->slope1;

	memset(cmcs_buf->slope0, 0, sizeof(cmcs_buf->slope0));
	memset(cmcs_buf->slope1, 0, sizeof(cmcs_buf->slope1));

	if (!strcmp(ts_cmcs->spec_slope.name, CMCS_CS))
		presult = (s16 *)&cmcs_buf->cs;
	else
		presult = (s16 *)&cmcs_buf->cm;

	for (i = 0; i < ts_cmcs->items.cnt; i++) {
		if (!strcmp(ts_cmcs->spec_slope.name, ts_cmcs->items.item[i].name)) {
			idx1 = idx2 = gtx_idx = key_idx = 0;
			gtx_idx = tsp->screen.tx * tsp->screen.rx;
			key_idx = gtx_idx + (tsp->gtx.num * tsp->screen.rx);
			for (j = 0; j < tsp->ch_num.tx; j++) {
				for (k = 0; k < tsp->ch_num.rx; k++) {
					type = check_tsp_type(data, j, k);
					idx1 = (j * tsp->ch_num.rx) + k;
					if (type == TSP_CH_UNKNOWN) {
						continue;
					} else if (type == TSP_CH_UNUSED) {
						cmcs_buf->spec_min[idx1] = 0;
						cmcs_buf->spec_max[idx1] = 0;
					} else if (type == TSP_CH_GTX) {
						cmcs_buf->spec_min[idx1] =
							ts_cmcs->spec_item[i].spec_node.buf_min[gtx_idx];
						cmcs_buf->spec_max[idx1] =
							ts_cmcs->spec_item[i].spec_node.buf_max[gtx_idx];
						gtx_idx++;
					} else if (type == TSP_CH_KEY) {
						cmcs_buf->spec_min[idx1] =
							ts_cmcs->spec_item[i].spec_node.buf_min[key_idx];
						cmcs_buf->spec_max[idx1] =
							ts_cmcs->spec_item[i].spec_node.buf_max[key_idx];
						key_idx++;
					} else {
						cmcs_buf->spec_min[idx1] =
							ts_cmcs->spec_item[i].spec_node.buf_min[idx2];
						cmcs_buf->spec_max[idx1] =
							ts_cmcs->spec_item[i].spec_node.buf_max[idx2];
						idx2++;
					}
				}
			}
			success = true;
		}
	}

	if (success == false)
		return -EPERM;

#if CMCS_PARSING_DEBUG
	idx1 = 0;
	tsp_info("# Node min specific\n");
	for (i = 0; i < tsp->ch_num.tx; i++) {
		tsp_info(" ");
		for (j = 0; j < tsp->ch_num.rx; j++)
			printk("%5d ", cmcs_buf->spec_min[idx1++]);
		printk("\n");
	}

	idx1 = 0;
	tsp_info("# Node max specific\n");
	for (i = 0; i < tsp->ch_num.tx; i++) {
		tsp_info(" ");
		for (j = 0; j < tsp->ch_num.rx; j++)
			printk("%5d ", cmcs_buf->spec_max[idx1++]);
		printk("\n");
	}
#endif

#if IST30XX_SDC_SLOPE
	tsp_verb("# Apply slope\n");
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			idx1 = (i * tsp->ch_num.rx) + j;

			idx2 = idx1 + 1;
			if (j == (width - 1)) {
				slope_x = 0;
			} else {
				slope_x = 100 - ((presult[idx1] * 100) / presult[idx2]);
				if (slope_x < 0)
					slope_x *= -1;
			}

			idx2 = idx1 + tsp->ch_num.rx;
			if (i == (height - 1)) {
				slope_y = 0;
			} else {
				slope_y = 100 - ((presult[idx1] * 100) / presult[idx2]);
				if (slope_y < 0)
					slope_y *= -1;
			}

			if (slope_x > slope_y) {
				pslope0[idx1] = (s16)slope_x;
				pslope1[idx1] = (s16)slope_x;
			} else {
				pslope0[idx1] = (s16)slope_y;
				pslope1[idx1] = (s16)slope_y;
			}
		}
	}

	tsp_verb("# Apply slope_gtx\n");
	for (i = 0; i < tsp->gtx.num; i++) {
		if (tsp->gtx.ch_num[i] > height) {
			for (j = 0; j < width; j++) {
				idx1 = (tsp->gtx.ch_num[i] * tsp->ch_num.rx) + j;

				idx2 = idx1 + 1;
				if (j == (width - 1)) {
					slope_x = 0;
				} else {
					slope_x = 100 - ((presult[idx1] * 100) / presult[idx2]);
					if (slope_x < 0)
						slope_x *= -1;
				}

				idx2 = idx1 + tsp->ch_num.rx;
				if (i == (tsp->gtx.num - 1)) {
					slope_y = 0;
				} else {
					slope_y = 100 - ((presult[idx1] * 100) / presult[idx2]);
					if (slope_y < 0)
						slope_y *= -1;
				}

				if (slope_x > slope_y) {
					pslope0[idx1] = (s16)slope_x;
					pslope1[idx1] = (s16)slope_x;
				} else {
					pslope0[idx1] = (s16)slope_y;
					pslope1[idx1] = (s16)slope_y;
				}
			}
		}
	}
#else
	tsp_verb("# Apply slope0\n");
	for (i = 0; i < height; i++) {
		for (j = 0; j < width - 1; j++) {
			idx1 = (i * tsp->ch_num.rx) + j;
			idx2 = idx1 + 1;

			pslope0[idx1] = (presult[idx2] - presult[idx1]);
			pslope0[idx1] +=
				(((pspec_min[idx1] + pspec_max[idx1]) / 2) -
				 ((pspec_min[idx2] + pspec_max[idx2]) / 2));
		}
	}

	tsp_verb("# Apply slope1\n");
	for (i = 0; i < height - 1; i++) {
		for (j = 0; j < width; j++) {
			idx1 = (i * tsp->ch_num.rx) + j;
			idx2 = idx1 + tsp->ch_num.rx;

			pslope1[idx1] = (presult[idx2] - presult[idx1]);
			pslope1[idx1] +=
				(((pspec_min[idx1] + pspec_max[idx1]) / 2) -
				 ((pspec_min[idx2] + pspec_max[idx2]) / 2));
		}
	}

	tsp_verb("# Apply slope0 gtx\n");
	for (i = 0; i < tsp->gtx.num; i++) {
		if (tsp->gtx.ch_num[i] > height) {
			for (j = 0; j < width - 1; j++) {
				idx1 = (tsp->gtx.ch_num[i] * tsp->ch_num.rx) + j;
				idx2 = idx1 + 1;

				pslope0[idx1] = (presult[idx2] - presult[idx1]);
				pslope0[idx1] +=
					(((pspec_min[idx1] + pspec_max[idx1]) / 2) -
					 ((pspec_min[idx2] + pspec_max[idx2]) / 2));
			}
		}
	}

	tsp_verb("# Apply slope1 gtx\n");
	for (i = 0; i < (tsp->gtx.num - 1); i++) {
		if (tsp->gtx.ch_num[i] > height) {
			for (j = 0; j < width; j++) {
				idx1 = (tsp->gtx.ch_num[i] * tsp->ch_num.rx) + j;
				idx2 = idx1 + tsp->ch_num.rx;

				pslope1[idx1] = (presult[idx2] - presult[idx1]);
				pslope1[idx1] +=
					(((pspec_min[idx1] + pspec_max[idx1]) / 2) -
					 ((pspec_min[idx2] + pspec_max[idx2]) / 2));
			}
		}
	}
#endif

#if CMCS_PARSING_DEBUG
	tsp_info("# slope0_x\n");
	for (i = 0; i < height; i++) {
		tsp_info(" ");
		for (j = 0; j < width; j++) {
			idx1 = (i * tsp->ch_num.rx) + j;
			printk("%5d ", pslope0[idx1]);
		}
		printk("\n");
	}

	tsp_info("# slope1_y\n");
	for (i = 0; i < height; i++) {
		tsp_info(" ");
		for (j = 0; j < width; j++) {
			idx1 = (i * tsp->ch_num.rx) + j;
			printk("%5d ", pslope1[idx1]);
		}
		printk("\n");
	}

	tsp_info("# slope0_gtx_x\n");
	for (i = 0; i < tsp->gtx.num; i++) {
		tsp_info(" ");
		for (j = 0; j < width; j++) {
			idx1 = (tsp->gtx.ch_num[i] * tsp->ch_num.rx) + j;
			printk("%5d ", pslope0[idx1]);
		}
		printk("\n");
	}

	tsp_info("# slope1_gtx_y\n");
	for (i = 0; i < tsp->gtx.num; i++) {
		tsp_info(" ");
		for (j = 0; j < width; j++) {
			idx1 = (tsp->gtx.ch_num[i] * tsp->ch_num.rx) + j;
			printk("%5d ", pslope1[idx1]);
		}
		printk("\n");
	}

#endif

	return 0;
}

int ist30xx_get_cmcs_buf(struct ist30xx_data *data, const char *mode,
			 CMCS_ITEM items, s16 *buf)
{
	int ret = 0;
	int i;
	bool success = false;
	u32 waddr;
	u16 len;

	for (i = 0; i < items.cnt; i++) {
		if (!strcmp(items.item[i].name, mode)) {
			waddr = IST30XX_DA_ADDR(items.item[i].addr);
			len = items.item[i].size / IST30XX_DATA_LEN;
			ret = ist30xx_burst_read(data->client,
						 waddr, (u32 *)buf, len, true);
			if (unlikely(ret))
				return ret;
			tsp_verb("%s, 0x%08x, %d\n", __func__, waddr, len);
			success = true;
		}
	}

	if (success == false) {
		tsp_info("item(%s) dosen't exist!\n", mode);
		return ret;
	}

#if CMCS_PARSING_DEBUG
	ret = ist30xx_parse_cmcs_buf(data, buf);
#endif

	return ret;
}

int ist30xx_cmcs_wait(struct ist30xx_data *data, int mode)
{
	u32 waddr;
	u32 val;
	int ret;
	int cnt = CMCS_TIMEOUT / 100;

	data->status.cmcs = 0;

	waddr = IST30XX_DA_ADDR(ts_cmcs->param.enable_addr);
	if (mode == CMCS_FLAG_CM)
		val = CMCS_ENABLE_CM;
	else if (mode == CMCS_FLAG_CS)
		val = CMCS_ENABLE_CS;
	else if (mode == CMCS_FLAG_CMJIT)
		val = CMCS_ENABLE_CMJIT;
	else if (mode == CMCS_FLAG_INT)
		val = CMCS_ENABLE_INT;
	else
		return -EPERM;

	ret = ist30xx_write_cmd(data, waddr, val);
	if (ret)
		return -EPERM;

	while (cnt-- > 0) {
		msleep(100);

		if (data->status.cmcs) {
			if (mode == CMCS_FLAG_CM)
				goto cm_end;
			else if (mode == CMCS_FLAG_CS)
				goto cs_end;
			else if (mode == CMCS_FLAG_CMJIT)
				goto cmjit_end;
			else if (mode == CMCS_FLAG_INT)
				goto int_end;
			else
				return -EPERM;
		}
	}
	tsp_warn("cmcs time out\n");

	return -EPERM;

cm_end:
	if ((data->status.cmcs & CMCS_MSG_MASK) == CM_MSG_VALID)
		if (!(data->status.cmcs & 0x1))
			return 0;

	tsp_warn("CM test fail\n");

	return -EPERM;

cs_end:
	if ((data->status.cmcs & CMCS_MSG_MASK) == CS_MSG_VALID)
		if (!(data->status.cmcs & 0x1))
			return 0;

	tsp_warn("CS test fail\n");

	return -EPERM;

cmjit_end:
	if ((data->status.cmcs & CMCS_MSG_MASK) == CMJIT_MSG_VALID)
		if (!(data->status.cmcs & 0x1))
			return 0;

	tsp_warn("CMJIT test fail\n");

	return -EPERM;

int_end:
	if ((data->status.cmcs & CMCS_MSG_MASK) == INT_MSG_VALID)
		if (!(data->status.cmcs & 0x1))
			return 0;

	tsp_warn("INT test fail\n");

	return -EPERM;
}

#define cmcs_next_step(ret)   {if (unlikely(ret)) goto end; msleep(20); }
int ist30xx_cmcs_test(struct ist30xx_data *data, const u8 *buf, int size)
{
	int i;
	int ret;
	int len;
	u32 waddr;
	u32 val;
	u32 chksum = 0;
	u32 *buf32;

	tsp_info("*** CM/CS test ***\n");

	ist30xx_disable_irq(data);
	ist30xx_reset(data, false);

	/* 1. set CmCsFW write mode */
	ret = ist30xx_write_cmd(data, IST30XX_HIB_CMD,
			(eHCOM_RUN_RAMCODE << 16) | 0);
	cmcs_next_step(ret);

	/* 2. write CmCsFW */
	buf32 = (u32 *)ts_cmcs->buf_cmcs;
	len = ts_cmcs->param.cmcs_size / IST30XX_DATA_LEN;
	tsp_verb("%08x %08x %08x %08x\n", buf32[0], buf32[1], buf32[2], buf32[3]);
	waddr = IST30XX_DA_ADDR(data->tags.ram_base);
	tsp_verb("%08x(%d)\n", waddr, len);

	ret = ist30xx_burst_write(data->client, waddr, buf32, len);
	cmcs_next_step(ret);

	/* 3. write CmCsFW size */
	waddr = IST30XX_DA_ADDR(ts_cmcs->param.cmcs_size_addr);
	val = ts_cmcs->param.cmcs_size;
	ret = ist30xx_write_cmd(data, waddr, val);
	cmcs_next_step(ret);
	tsp_verb("size(0x%08x): 0x%08x\n", waddr, val);

	tsp_info("cmcs code loaded!\n");

	/* 4. write cmds */
	ret = ist30xx_set_cmcs_cmd(data, ts_cmcs->cmds);
	cmcs_next_step(ret);
	tsp_info("cmcs command loaded!\n");

	/* 5. start CmCsFW */
	ret = ist30xx_write_cmd(data, IST30XX_HIB_CMD,
			(eHCOM_RUN_RAMCODE << 16) | 1);
	cmcs_next_step(ret);

	/* 6. read & compare checksum */
	waddr = IST30XX_DA_ADDR(ts_cmcs->param.checksum_addr);
	ret = ist30xx_read_reg(data->client, waddr, &chksum);
	cmcs_next_step(ret);
	if (chksum != ts_cmcs->param.cmcs_chksum)
		goto end;

	/* 7. find "CM" in items & test */
	for (i = 0; i < ts_cmcs->items.cnt; i++) {
		if (!strcmp(ts_cmcs->items.item[i].name, CMCS_CM)) {
			/* 7-1. write CM SensorReg */
			buf32 = ts_cmcs->buf_cm_sensor;
			ret = ist30xx_set_cmcs_sensor(data->client,
					  ts_cmcs->param, buf32, CMCS_FLAG_CM);
			cmcs_next_step(ret);
			tsp_info("CM test ready!!\n");

			/* 7-2. write Start CM enable & wait CM complete interrupt */
			ist30xx_enable_irq(data);
			if (ist30xx_cmcs_wait(data, CMCS_FLAG_CM) == 0) {
				tsp_info("CM test success\n");
			} else {
				tsp_info("CM test fail!\n");
				ret = -ENOEXEC;
				goto end;
			}
			ist30xx_disable_irq(data);

			/* 7-3. read CM data */
			memset(ts_cmcs_buf->cm, 0, sizeof(ts_cmcs_buf->cm));
			ret = ist30xx_get_cmcs_buf(data, CMCS_CM, ts_cmcs->items,
					ts_cmcs_buf->cm);
			cmcs_next_step(ret);
			tsp_info("CM data read\n");
		}
	}

	/* 8. find "CMJIT" in items & test */
	for (i = 0; i < ts_cmcs->items.cnt; i++) {
		if (!strcmp(ts_cmcs->items.item[i].name, CMCS_CMJIT)) {
			/* 8-1. write CM SensorReg */
			buf32 = ts_cmcs->buf_cm_sensor;
			ret = ist30xx_set_cmcs_sensor(data->client,
					ts_cmcs->param, buf32, CMCS_FLAG_CMJIT);
			cmcs_next_step(ret);
			tsp_info("CMJIT test ready!!\n");

			/* 8-2. write Start CMJIT enable & wait CMJIT complete interrupt */
			ist30xx_enable_irq(data);
			if (ist30xx_cmcs_wait(data, CMCS_FLAG_CMJIT) == 0) {
				tsp_info("CMJIT test success\n");
			} else {
				tsp_info("CMJIT test Fail!\n");
				ret = -ENOEXEC;
				goto end;
			}
			ist30xx_disable_irq(data);

			/* 8-3. read CMJIT data */
			memset(ts_cmcs_buf->cm_jit, 0, sizeof(ts_cmcs_buf->cm_jit));
			ret = ist30xx_get_cmcs_buf(data, CMCS_CMJIT, ts_cmcs->items,
					ts_cmcs_buf->cm_jit);
			cmcs_next_step(ret);
			tsp_info("CMJIT data read\n");
		}
	}

	/* 9. find "CS" in items & test */
	for (i = 0; i < ts_cmcs->items.cnt; i++) {
		if (!strcmp(ts_cmcs->items.item[i].name, CMCS_CS)) {
			/* 9-1. write CS SensorReg */
			buf32 = ts_cmcs->buf_cs_sensor;
			ret = ist30xx_set_cmcs_sensor(data->client,
							  ts_cmcs->param, buf32, CMCS_FLAG_CS);
			cmcs_next_step(ret);
			tsp_info("CS test ready!!\n");

			/* 9-2. write Start CS enable & wait CS complete interrupt */
			ist30xx_enable_irq(data);
			if (ist30xx_cmcs_wait(data, CMCS_FLAG_CS) == 0) {
				tsp_info("CS test success\n");
			} else {
				tsp_info("CS test fail!\n");
				ret = -ENOEXEC;
				goto end;
			}
			ist30xx_disable_irq(data);

			/* 9-3. read CS data */
			memset(ts_cmcs_buf->cs, 0, sizeof(ts_cmcs_buf->cs));
			ret = ist30xx_get_cmcs_buf(data, CMCS_CS, ts_cmcs->items,
					ts_cmcs_buf->cs);
			cmcs_next_step(ret);
			tsp_info("CS data read\n");

			/* 9-4 read CS T/RX result */
			ret = ist30xx_read_reg(data->client,
					IST30XX_DA_ADDR(ts_cmcs->param.cs_tx_result_addr),
					&ts_cmcs_buf->cs_tx_result);

			ret = ist30xx_read_reg(data->client,
					IST30XX_DA_ADDR(ts_cmcs->param.cs_rx_result_addr),
					&ts_cmcs_buf->cs_rx_result);
		}
	}

	/* 10. calculate slope value */
	ret = ist30xx_apply_cmcs_slope(data, ts_cmcs_buf);
			cmcs_next_step(ret);

	cmcs_ready = CMCS_READY;
end:
	if (unlikely(ret)) {
		tsp_warn("CmCs test Fail!, ret=%d\n", ret);
	} else if (unlikely(chksum != ts_cmcs->param.cmcs_chksum)) {
		tsp_warn("Error CheckSum: %x(%x)\n",
			 chksum, ts_cmcs->param.cmcs_chksum);
		ret = -ENOEXEC;
	}

	ist30xx_reset(data, false);
	ist30xx_start(data);
	ist30xx_enable_irq(data);

	return ret;
}


int check_tsp_type(struct ist30xx_data *data, int tx, int rx)
{
	int i;
	TSP_INFO *tsp = &data->tsp_info;
	TKEY_INFO *tkey = &data->tkey_info;

	if ((tx >= tsp->ch_num.tx) || (tx < 0) ||
		(rx >= tsp->ch_num.rx) || (rx < 0)) {
		tsp_warn("TSP channel is not correct!! (%d * %d)\n", tx, rx);
		return TSP_CH_UNKNOWN;
	}

	if ((tx >= tsp->screen.tx) || (rx >= tsp->screen.rx)) {
		for (i = 0; i < tsp->gtx.num; i++)
			if ((tx == tsp->gtx.ch_num[i]) && (rx < tsp->screen.rx))
				return TSP_CH_GTX;

		for (i = 0; i < tkey->key_num; i++) {
			if ((tx == tkey->ch_num[i].tx) &&
				(rx == tkey->ch_num[i].rx))
				return TSP_CH_KEY;
		}
	} else {
		return TSP_CH_SCREEN;
	}

	return TSP_CH_UNUSED;
}

int print_cmcs(struct ist30xx_data *data, s16 *buf16, char *buf)
{
	int i, j;
	int idx;
	int type;
	int count = 0;
	char msg[128];

	TSP_INFO *tsp = &data->tsp_info;

	int tx_num = tsp->ch_num.tx;
	int rx_num = tsp->ch_num.rx;

	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			type = check_tsp_type(data, i, j);
			idx = (i * rx_num) + j;
			if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
				count += sprintf(msg, "%5d ", 0);
			else
				count += sprintf(msg, "%5d ", buf16[idx]);
			strcat(buf, msg);
		}

		count += sprintf(msg, "\n");
		strcat(buf, msg);
	}

	return count;
}

int print_line_cmcs(struct ist30xx_data *data, int mode, s16 *buf16, char *buf)
{
	int i, j;
	int idx;
	int type;
	int count = 0;
	int key_index[5] = {0,};
	int key_cnt = 0;
	char msg[128];

	TSP_INFO *tsp = &data->tsp_info;

	int tx_num = tsp->ch_num.tx;
	int rx_num = tsp->ch_num.rx;

	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			type = check_tsp_type(data, i, j);
			if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
				continue;

			if ((mode == CMCS_FLAG_CM_SLOPE0) &&
				(j == (tsp->screen.rx - 1)))
				continue;
			else if ((mode == CMCS_FLAG_CM_SLOPE1) &&
				 (i == (tsp->screen.tx - 1)))
				continue;

			if ((mode == CMCS_FLAG_CM_SLOPE0) && (type == TSP_CH_GTX) &&
				(j == (tsp->screen.rx - 1)))
				continue;
			else if ((mode == CMCS_FLAG_CM_SLOPE1) &&
				 (type == TSP_CH_GTX) &&
				 (i == tsp->gtx.ch_num[tsp->gtx.num - 1]))
				continue;

			idx = (i * rx_num) + j;

			if (type == TSP_CH_KEY) {
				key_index[key_cnt++] = idx;
				continue;
			}

			count += sprintf(msg, "%5d ", buf16[idx]);
			strcat(buf, msg);
		}
	}

	if ((mode != CMCS_FLAG_CM_SLOPE0) && (mode != CMCS_FLAG_CM_SLOPE1)) {
		tsp_debug("key cnt: %d\n", key_cnt);
		for (i = 0; i < key_cnt; i++) {
			count += sprintf(msg, "%5d ", buf16[key_index[i]]);
			strcat(buf, msg);
		}
	}

	count += sprintf(msg, "\n");
	strcat(buf, msg);

	return count;
}

/* sysfs: /sys/class/touch/cmcs/info */
ssize_t ist30xx_cmcs_info_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int i;
	int count;
	char msg[128];
	struct ist30xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;
	TKEY_INFO *tkey = &data->tkey_info;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	/* Channel */
	count = sprintf(msg, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d ",
			tsp->ch_num.tx, tsp->ch_num.rx, tsp->screen.tx, tsp->screen.rx,
			tsp->gtx.num, tsp->gtx.ch_num[0], tsp->gtx.ch_num[1],
			tsp->gtx.ch_num[2], tsp->gtx.ch_num[3], tkey->key_num,
			tkey->ch_num[0].tx, tkey->ch_num[0].rx, tkey->ch_num[1].tx,
			tkey->ch_num[1].rx, tkey->ch_num[2].tx, tkey->ch_num[2].rx);
	strcat(buf, msg);

	/* Slope */
	count += sprintf(msg, "%d %d %d %d %d %d %d %d %d %d ",
			 ts_cmcs->spec_slope.x_min, ts_cmcs->spec_slope.x_max,
			 ts_cmcs->spec_slope.y_min, ts_cmcs->spec_slope.y_max,
			 ts_cmcs->spec_slope.gtx_x_min, ts_cmcs->spec_slope.gtx_x_max,
			 ts_cmcs->spec_slope.gtx_y_min, ts_cmcs->spec_slope.gtx_y_max,
			 ts_cmcs->spec_slope.key_min, ts_cmcs->spec_slope.key_max);
	strcat(buf, msg);

	/* CS */
	for (i = 0; i < ts_cmcs->items.cnt; i++) {
		if (!strcmp(ts_cmcs->items.item[i].name, CMCS_CS)) {
			if (!strcmp(ts_cmcs->items.item[i].spec_type, "T")) {
				count += sprintf(msg, "%d %d %d %d %d %d ",
						 ts_cmcs->spec_item[i].spec_total.screen_min,
						 ts_cmcs->spec_item[i].spec_total.screen_max,
						 ts_cmcs->spec_item[i].spec_total.gtx_min,
						 ts_cmcs->spec_item[i].spec_total.gtx_max,
						 ts_cmcs->spec_item[i].spec_total.key_min,
						 ts_cmcs->spec_item[i].spec_total.key_max);
				strcat(buf, msg);
			}
		}
	}

	/* CR */
	count += sprintf(msg, "%d %d %d %d %d %d ",
			 ts_cmcs->spec_cr.screen_min, ts_cmcs->spec_cr.screen_max,
			 ts_cmcs->spec_cr.gtx_min, ts_cmcs->spec_cr.gtx_max,
			 ts_cmcs->spec_cr.key_min, ts_cmcs->spec_cr.key_max);
	strcat(buf, msg);

	/* CMJIT */
	for (i = 0; i < ts_cmcs->items.cnt; i++) {
		if (!strcmp(ts_cmcs->items.item[i].name, CMCS_CMJIT)) {
			if (!strcmp(ts_cmcs->items.item[i].spec_type, "T")) {
				count += sprintf(msg, "%d %d %d %d %d %d ",
						 ts_cmcs->spec_item[i].spec_total.screen_min,
						 ts_cmcs->spec_item[i].spec_total.screen_max,
						 ts_cmcs->spec_item[i].spec_total.gtx_min,
						 ts_cmcs->spec_item[i].spec_total.gtx_max,
						 ts_cmcs->spec_item[i].spec_total.key_min,
						 ts_cmcs->spec_item[i].spec_total.key_max);
				strcat(buf, msg);
			}
		}
	}

	tsp_verb("%s\n", buf);

	return count;
}

/* sysfs: /sys/class/touch/cmcs/cmcs_binary */
ssize_t ist30xx_cmcs_binary_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ret;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	if ((ts_cmcs_bin == NULL) || (ts_cmcs_bin_size == 0))
		return sprintf(buf, "Binary is not correct(%d)\n", ts_cmcs_bin_size);

	ret = ist30xx_get_cmcs_info(ts_cmcs_bin, ts_cmcs_bin_size);
	if (ret)
		goto binary_end;

	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_cmcs_test(data, ts_cmcs_bin, ts_cmcs_bin_size);
	mutex_unlock(&ist30xx_mutex);

binary_end:
	return sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n"));
}

/* sysfs: /sys/class/touch/cmcs/cmcs_custom */
ssize_t ist30xx_cmcs_custom_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ret;
	int bin_size = 0;
	u8 *bin = NULL;
	const struct firmware *req_bin = NULL;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	ret = request_firmware(&req_bin, IST30XX_CMCS_NAME, &data->client->dev);
	if (ret)
		return sprintf(buf, "File not found, %s\n", IST30XX_CMCS_NAME);

	bin = (u8 *)req_bin->data;
	bin_size = (u32)req_bin->size;

	ret = ist30xx_get_cmcs_info(bin, bin_size);
	if (ret)
		goto custom_end;

	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_cmcs_test(data, bin, bin_size);
	mutex_unlock(&ist30xx_mutex);

custom_end:
	release_firmware(req_bin);

	return sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n"));
}

#define MAX_FILE_PATH   255
/* sysfs: /sys/class/touch/cmcs/cmcs_sdcard */
ssize_t ist30xx_cmcs_sdcard_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int bin_size = 0;
	u8 *bin = NULL;
	const u8 *buff = 0;
	mm_segment_t old_fs = {0};
	struct file *fp = NULL;
	long fsize = 0, nread = 0;
	char fw_path[MAX_FILE_PATH];
	struct ist30xx_data *data = dev_get_drvdata(dev);

	old_fs = get_fs();
	set_fs(get_ds());

	snprintf(fw_path, MAX_FILE_PATH, "/sdcard/%s",
		 IST30XX_CMCS_NAME);
	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {

		ret = -ENOENT;
		goto err_file_open;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;

	buff = kzalloc((size_t)fsize, GFP_KERNEL);
	if (!buff) {
		tsp_info("fail to alloc buffer\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	nread = vfs_read(fp, (char __user *)buff, fsize, &fp->f_pos);
	if (nread != fsize) {
		tsp_info("mismatch fw size\n");

		goto err_fw_size;
	}

	bin = (u8 *)buff;
	bin_size = (u32)fsize;

	filp_close(fp, current->files);
	set_fs(old_fs);
	tsp_info("firmware is loaded!!\n");

	ret = ist30xx_get_cmcs_info(bin, bin_size);
	if (ret)
		goto sdcard_end;

	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_cmcs_test(data, bin, bin_size);
	mutex_unlock(&ist30xx_mutex);

err_fw_size:
	if (buff)
		kfree(buff);
err_alloc:
	if (fp)
		filp_close(fp, NULL);
err_file_open:
	set_fs(old_fs);

sdcard_end:
	tsp_info("size: %d\n", sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n")));

	return sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n"));
}

/* sysfs: /sys/class/touch/cmcs/cm_jit */
ssize_t ist30xx_cm_jit_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	tsp_verb("CMJIT (%d * %d)\n", tsp->ch_num.tx, tsp->ch_num.rx);

	return print_cmcs(data, ts_cmcs_buf->cm_jit, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm */
ssize_t ist30xx_cm_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	tsp_verb("CM (%d * %d)\n", tsp->ch_num.tx, tsp->ch_num.rx);

	return print_cmcs(data, ts_cmcs_buf->cm, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_spec_min */
ssize_t ist30xx_cm_spec_min_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	tsp_verb("CM Spec Min (%d * %d)\n", tsp->ch_num.tx, tsp->ch_num.rx);

	return print_cmcs(data, ts_cmcs_buf->spec_min, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_spec_max */
ssize_t ist30xx_cm_spec_max_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	tsp_verb("CM Spec Max (%d * %d)\n", tsp->ch_num.tx, tsp->ch_num.rx);

	return print_cmcs(data, ts_cmcs_buf->spec_max, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_slope0 */
ssize_t ist30xx_cm_slope0_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	tsp_verb("CM Slope0_X (%d * %d)\n", tsp->ch_num.tx, tsp->ch_num.rx);

	return print_cmcs(data, ts_cmcs_buf->slope0, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_slope1 */
ssize_t ist30xx_cm_slope1_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	tsp_verb("CM Slope1_Y (%d * %d)\n", tsp->ch_num.tx, tsp->ch_num.rx);

	return print_cmcs(data, ts_cmcs_buf->slope1, buf);
}

/* sysfs: /sys/class/touch/cmcs/cs */
ssize_t ist30xx_cs_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	tsp_verb("CS (%d * %d)\n", tsp->ch_num.tx, tsp->ch_num.rx);

	return print_cmcs(data, ts_cmcs_buf->cs, buf);
}

/* sysfs: /sys/class/touch/cmcs/cs_tx */
ssize_t ist30xx_cs_tx_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int i;
	int count = 0;
	struct ist30xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;
	char msg[8];

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	for (i = 0; i < tsp->ch_num.tx; i++) {
		count += sprintf(msg, "%5d ", (ts_cmcs_buf->cs_tx_result >> i) & 1);
		strcat(buf, msg);
	}

	count += sprintf(msg, "\n");
	strcat(buf, msg);

	return count;
}

/* sysfs: /sys/class/touch/cmcs/cs_rx */
ssize_t ist30xx_cs_rx_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int i;
	int count = 0;
	struct ist30xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;
	char msg[8];

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	for (i = 0; i < tsp->ch_num.rx; i++) {
		count += sprintf(msg, "%5d ", (ts_cmcs_buf->cs_rx_result >> i) & 1);
		strcat(buf, msg);
	}

	count += sprintf(msg, "\n");
	strcat(buf, msg);

	return count;
}

int print_cm_slope_result(struct ist30xx_data *data, u8 flag, s16 *buf16,
			  char *buf, bool fail_list)
{
	int i, j;
	int type, idx;
	int count = 0, err_cnt = 0;
	int min_spec, max_spec, gtx_min_spec, gtx_max_spec;

	char msg[128];

	TSP_INFO *tsp = &data->tsp_info;
	struct CMCS_SPEC_SLOPE *spec =
		(struct CMCS_SPEC_SLOPE *)&ts_cmcs->spec_slope;

	if (flag == CMCS_FLAG_CM_SLOPE0) {
		min_spec = spec->x_min;
		max_spec = spec->x_max;
		gtx_min_spec = spec->gtx_x_min;
		gtx_max_spec = spec->gtx_x_max;
	} else if (flag == CMCS_FLAG_CM_SLOPE1) {
		min_spec = spec->y_min;
		max_spec = spec->y_max;
		gtx_min_spec = spec->gtx_y_min;
		gtx_max_spec = spec->gtx_y_max;
	} else {
		count = sprintf(msg, "Unknown flag: %d\n", flag);
		strcat(buf, msg);
		return count;
	}

	if (fail_list) {
		count = sprintf(msg, "Spec: screen(%d~%d), GTx(%d~%d)\n",
				min_spec, max_spec, gtx_min_spec, gtx_max_spec);
		strcat(buf, msg);
	}

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = (i * tsp->ch_num.rx) + j;

			type = check_tsp_type(data, i, j);
			if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED) ||
				(type == TSP_CH_KEY))
				continue;

			if (type == TSP_CH_SCREEN) {
				if ((buf16[idx] >= min_spec) && (buf16[idx] <= max_spec))
					continue;
			} else if (type == TSP_CH_GTX) {
				if ((buf16[idx] >= gtx_min_spec) &&
					(buf16[idx] <= gtx_max_spec))
					continue;
			}

			if (fail_list) {
				count += sprintf(msg, "%2d, %2d:%4d\n", i, j, buf16[idx]);
				strcat(buf, msg);
			}

			err_cnt++;
		}
	}

	/* Check error count */
	if (err_cnt == 0)
		count += sprintf(msg, "OK\n");
	else
		count += sprintf(msg, "Fail, node count: %d\n", err_cnt);
	strcat(buf, msg);

	return count;
}

int print_cm_key_slope_result(struct ist30xx_data *data, s16 *buf16, char *buf,
				  bool print)
{
	int i, j;
	int type, idx;
	int count = 0;
	int min_spec, max_spec;
	int key_num = 0;
	s16 key_cm[5] = {0,};
	s16 slope_result;

	char msg[128];

	TSP_INFO *tsp = &data->tsp_info;
	struct CMCS_SPEC_SLOPE *spec =
		(struct CMCS_SPEC_SLOPE *)&ts_cmcs->spec_slope;

	min_spec = spec->key_min;
	max_spec = spec->key_max;

	if (print) {
		count = sprintf(msg, "Spec: %d ~ %d\n", min_spec, max_spec);
		strcat(buf, msg);
	}

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = (i * tsp->ch_num.rx) + j;

			type = check_tsp_type(data, i, j);
			if (type == TSP_CH_KEY)
				key_cm[key_num++] = buf16[idx];
		}
	}

	/* Check key slope */
	slope_result = key_cm[0] - key_cm[1];

	if (print) {
		if ((slope_result >= min_spec) && (slope_result <= max_spec))
			count += sprintf(msg, "OK (%d)\n", slope_result);
		else
			count += sprintf(msg, "Fail (%d)\n", slope_result);
	} else {
		count += sprintf(msg, "%d\n",
				 (slope_result >= 0 ? slope_result : -slope_result));
	}

	strcat(buf, msg);

	return count;
}

int print_cm_result(struct ist30xx_data *data, char *buf, bool fail_list)
{
	int i, j;
	int type, idx, err_cnt = 0;
	int min_spec, max_spec;
	int count = 0;
	short cm;
	char msg[128];
	TSP_INFO *tsp = &data->tsp_info;

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = (i * tsp->ch_num.rx) + j;

			type = check_tsp_type(data, i, j);
			if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
				continue;

			min_spec = ts_cmcs_buf->spec_min[idx];
			max_spec = ts_cmcs_buf->spec_max[idx];

			cm = ts_cmcs_buf->cm[idx];
			if ((cm >= min_spec) && (cm <= max_spec))
				continue;

			if (fail_list) {
				count += sprintf(msg, "%2d, %2d:%4d (%4d~%4d)\n",
						 i, j, cm, min_spec, max_spec);
				strcat(buf, msg);
			}

			err_cnt++;
		}
	}

	/* Check error count */
	if (err_cnt == 0)
		count += sprintf(msg, "OK\n");
	else
		count += sprintf(msg, "Fail, node count: %d\n", err_cnt);
	strcat(buf, msg);

	return count;
}

int print_total_result(struct ist30xx_data *data, s16 *buf16, char *buf,
			   const char *mode, bool fail_list)
{
	int i, j;
	bool success = false;
	int type, idx;
	int count = 0, err_cnt = 0;
	int min_spec, max_spec;
	char msg[128];

	TSP_INFO *tsp = &data->tsp_info;
	struct CMCS_SPEC_TOTAL *spec;

	for (i = 0; i < ts_cmcs->items.cnt; i++) {
		if (!strcmp(ts_cmcs->items.item[i].name, mode)) {
			if (!strcmp(ts_cmcs->items.item[i].spec_type, "T")) {
				spec =
					(struct CMCS_SPEC_TOTAL *)&ts_cmcs->spec_item[i].spec_total;
				success = true;
				break;
			}
		}
	}

	if (success == false)
		return 0;

	if (fail_list) {
		count = sprintf(msg, "Spec: screen(%d~%d), gtx(%d~%d), key(%d~%d)\n",
				spec->screen_min, spec->screen_max, spec->gtx_min,
				spec->gtx_max, spec->key_min, spec->key_max);
		strcat(buf, msg);
	}

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = (i * tsp->ch_num.rx) + j;

			type = check_tsp_type(data, i, j);
			if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
				continue;

			if (type == TSP_CH_SCREEN) {
				min_spec = spec->screen_min;
				max_spec = spec->screen_max;
			} else if (type == TSP_CH_GTX) {
				min_spec = spec->gtx_min;
				max_spec = spec->gtx_max;
			} else {
				min_spec = spec->key_min;
				max_spec = spec->key_max;
			}

			if ((buf16[idx] >= min_spec) && (buf16[idx] <= max_spec))
				continue;

			if (fail_list) {
				count += sprintf(msg, "%2d, %2d:%4d\n", i, j, buf16[idx]);
				strcat(buf, msg);
			}

			err_cnt++;
		}
	}

	/* Check error count */
	if (err_cnt == 0)
		count += sprintf(msg, "OK\n");
	else
		count += sprintf(msg, "Fail, node count: %d\n", err_cnt);
	strcat(buf, msg);

	return count;
}

/* sysfs: /sys/class/touch/cmcs/cm_jit_result */
ssize_t ist30xx_cm_jit_result_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_total_result(data, ts_cmcs_buf->cm_jit, buf, CMCS_CMJIT, true);
}

/* sysfs: /sys/class/touch/cmcs/cm_result */
ssize_t ist30xx_cm_result_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_cm_result(data, buf, true);
}

/* sysfs: /sys/class/touch/cmcs/cm_slope0_result */
ssize_t ist30xx_cm_slope0_result_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_cm_slope_result(data, CMCS_FLAG_CM_SLOPE0,
					 ts_cmcs_buf->slope0, buf, true);
}

/* sysfs: /sys/class/touch/cmcs/cm_slope1_result */
ssize_t ist30xx_cm_slope1_result_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_cm_slope_result(data, CMCS_FLAG_CM_SLOPE1,
					 ts_cmcs_buf->slope1, buf, true);
}
/* sysfs: /sys/class/touch/cmcs/cm_key_slope_result */
ssize_t ist30xx_cm_key_slope_result_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_cm_key_slope_result(data, ts_cmcs_buf->cm, buf, true);
}

/* sysfs: /sys/class/touch/cmcs/cs_result */
ssize_t ist30xx_cs_result_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_total_result(data, ts_cmcs_buf->cs, buf, CMCS_CS, true);
}

/* sysfs: /sys/class/touch/cmcs/line_cm_jit */
ssize_t ist30xx_line_cm_jit_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_line_cmcs(data, CMCS_FLAG_CMJIT, ts_cmcs_buf->cm_jit, buf);
}

/* sysfs: /sys/class/touch/cmcs/line_cm */
ssize_t ist30xx_line_cm_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_line_cmcs(data, CMCS_FLAG_CM, ts_cmcs_buf->cm, buf);
}

/* sysfs: /sys/class/touch/cmcs/line_cm_slope0 */
ssize_t ist30xx_line_cm_slope0_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_line_cmcs(data, CMCS_FLAG_CM_SLOPE0, ts_cmcs_buf->slope0, buf);
}

/* sysfs: /sys/class/touch/cmcs/line_cm_slope1 */
ssize_t ist30xx_line_cm_slope1_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_line_cmcs(data, CMCS_FLAG_CM_SLOPE1, ts_cmcs_buf->slope1, buf);
}

/* sysfs: /sys/class/touch/cmcs/line_cs */
ssize_t ist30xx_line_cs_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_line_cmcs(data, CMCS_FLAG_CS, ts_cmcs_buf->cs, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_key_slope_value */
ssize_t ist30xx_cm_key_slope_value_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int ret;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	if ((ts_cmcs_bin == NULL) || (ts_cmcs_bin_size == 0))
		goto err_key_slope;

	ret = ist30xx_get_cmcs_info(ts_cmcs_bin, ts_cmcs_bin_size);
	if (ret)
		goto err_key_slope;

	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_cmcs_test(data, ts_cmcs_bin, ts_cmcs_bin_size);
	mutex_unlock(&ist30xx_mutex);

	if (ret)
		goto err_key_slope;

	if (cmcs_ready == CMCS_NOT_READY)
		goto err_key_slope;

	return print_cm_key_slope_result(data, ts_cmcs_buf->cm, buf, false);

err_key_slope:
	return sprintf(buf, "%s", "FFFF");
}

int cmcs_test_result(struct ist30xx_data *data)
{
	char *msg = NULL;
	int ret;

	msg = kzalloc(sizeof(char) * 4096, GFP_KERNEL);
	if (!msg) {
		tsp_err("Memory allocation failed\n");
		return 0;
	}

	/* CMCS binary */
	if ((ts_cmcs_bin == NULL) || (ts_cmcs_bin_size == 0)) {
		tsp_err("Binary is not correct(%d)\n", ts_cmcs_bin_size);
		goto out;
	}

	ret = ist30xx_get_cmcs_info(ts_cmcs_bin, ts_cmcs_bin_size);
	if (ret) {
		kfree(msg);
		tsp_err("Getting CMCS Binary info failed!\n");
		goto out;
	}

	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_cmcs_test(data, ts_cmcs_bin, ts_cmcs_bin_size);
	mutex_unlock(&ist30xx_mutex);

	if (ret == 0) {
		tsp_debug("CMCS Binary test passed!\n");
	} else {
		kfree(msg);
		tsp_err("CMCS Binary test failed!\n");
		goto out;
	}

	/* CM result */
	memset(msg, 0, 4096);
	print_cm_result(data, msg, false);
	if (strncmp(msg, "OK\n", strlen("OK\n")) == 0) {
		tsp_debug("CM result test passed!\n");
	} else {
		tsp_err("CM result test failed!\n");
		goto out;
	}

	/* CM slope 0 result */
	memset(msg, 0, 4096);
	print_cm_slope_result(data, CMCS_FLAG_CM_SLOPE0, ts_cmcs_buf->slope0, msg,
			false);
	if (strncmp(msg, "OK\n", strlen("OK\n")) == 0) {
		tsp_debug("CM slope 0 test passed!\n");
	} else {
		tsp_err("CM slope 0 test failed!\n");
		goto out;
	}

	/* CM slope 0 result */
	memset(msg, 0, 4096);
	print_cm_slope_result(data, CMCS_FLAG_CM_SLOPE1, ts_cmcs_buf->slope1, msg,
			false);
	if (strncmp(msg, "OK\n", strlen("OK\n")) == 0) {
		tsp_debug("CM slope 1 test passed!\n");
	} else {
		tsp_err("CM slope 1 test failed!\n");
		goto out;
	}

	/* CS Result */
	memset(msg, 0, 4096);
	print_total_result(data, ts_cmcs_buf->cs, msg, CMCS_CS, false);
	if (strncmp(msg, "OK\n", strlen("OK\n")) == 0) {
		tsp_debug("CS test passed!\n");
	} else {
		tsp_debug("CS test failed!\n");
		goto out;
	}

	kfree(msg);
	return 1;

out:
	kfree(msg);
	return 0;
}

/* sysfs: /sys/class/touch/cmcs/cmcs_test_all */
ssize_t ist30xx_cmcs_test_all_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result = 0;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	result = cmcs_test_result(data);

	return sprintf(buf, "%d", result);
}

/* sysfs : cmcs */
static DEVICE_ATTR(info, S_IRUGO, ist30xx_cmcs_info_show, NULL);
static DEVICE_ATTR(cmcs_binary, S_IRUGO, ist30xx_cmcs_binary_show, NULL);
static DEVICE_ATTR(cmcs_custom, S_IRUGO, ist30xx_cmcs_custom_show, NULL);
static DEVICE_ATTR(cmcs_sdcard, S_IRUGO, ist30xx_cmcs_sdcard_show, NULL);
static DEVICE_ATTR(cm_jit, S_IRUGO, ist30xx_cm_jit_show, NULL);
static DEVICE_ATTR(cm, S_IRUGO, ist30xx_cm_show, NULL);
static DEVICE_ATTR(cm_spec_min, S_IRUGO, ist30xx_cm_spec_min_show, NULL);
static DEVICE_ATTR(cm_spec_max, S_IRUGO, ist30xx_cm_spec_max_show, NULL);
static DEVICE_ATTR(cm_slope0, S_IRUGO, ist30xx_cm_slope0_show, NULL);
static DEVICE_ATTR(cm_slope1, S_IRUGO, ist30xx_cm_slope1_show, NULL);
static DEVICE_ATTR(cs, S_IRUGO, ist30xx_cs_show, NULL);
static DEVICE_ATTR(cs_tx, S_IRUGO, ist30xx_cs_tx_show, NULL);
static DEVICE_ATTR(cs_rx, S_IRUGO, ist30xx_cs_rx_show, NULL);
static DEVICE_ATTR(cm_jit_result, S_IRUGO, ist30xx_cm_jit_result_show, NULL);
static DEVICE_ATTR(cm_result, S_IRUGO, ist30xx_cm_result_show, NULL);
static DEVICE_ATTR(cm_slope0_result, S_IRUGO, ist30xx_cm_slope0_result_show,
		NULL);
static DEVICE_ATTR(cm_slope1_result, S_IRUGO, ist30xx_cm_slope1_result_show,
		NULL);
static DEVICE_ATTR(cm_key_slope_result, S_IRUGO,
		ist30xx_cm_key_slope_result_show, NULL);
static DEVICE_ATTR(cs_result, S_IRUGO, ist30xx_cs_result_show, NULL);
static DEVICE_ATTR(line_cm_jit, S_IRUGO, ist30xx_line_cm_jit_show, NULL);
static DEVICE_ATTR(line_cm, S_IRUGO, ist30xx_line_cm_show, NULL);
static DEVICE_ATTR(line_cm_slope0, S_IRUGO, ist30xx_line_cm_slope0_show, NULL);
static DEVICE_ATTR(line_cm_slope1, S_IRUGO, ist30xx_line_cm_slope1_show, NULL);
static DEVICE_ATTR(line_cs, S_IRUGO, ist30xx_line_cs_show, NULL);
static DEVICE_ATTR(cm_key_slope_value, S_IRUGO, ist30xx_cm_key_slope_value_show,
		NULL);
static DEVICE_ATTR(cmcs_test_all, S_IRUGO, ist30xx_cmcs_test_all_show, NULL);

static struct attribute *cmcs_attributes[] = {
	&dev_attr_info.attr,
	&dev_attr_cmcs_binary.attr,
	&dev_attr_cmcs_custom.attr,
	&dev_attr_cmcs_sdcard.attr,
	&dev_attr_cm_jit.attr,
	&dev_attr_cm.attr,
	&dev_attr_cm_spec_min.attr,
	&dev_attr_cm_spec_max.attr,
	&dev_attr_cm_slope0.attr,
	&dev_attr_cm_slope1.attr,
	&dev_attr_cs.attr,
	&dev_attr_cs_tx.attr,
	&dev_attr_cs_rx.attr,
	&dev_attr_cm_jit_result.attr,
	&dev_attr_cm_result.attr,
	&dev_attr_cm_slope0_result.attr,
	&dev_attr_cm_slope1_result.attr,
	&dev_attr_cm_key_slope_result.attr,
	&dev_attr_cs_result.attr,
	&dev_attr_line_cm_jit.attr,
	&dev_attr_line_cm.attr,
	&dev_attr_line_cm_slope0.attr,
	&dev_attr_line_cm_slope1.attr,
	&dev_attr_line_cs.attr,
	&dev_attr_cm_key_slope_value.attr,
	&dev_attr_cmcs_test_all.attr,
	NULL,
};

static struct attribute_group cmcs_attr_group = {
	.attrs	= cmcs_attributes,
};

extern struct class *ist30xx_class;
struct device *ist30xx_cmcs_dev;

int ist30xx_init_cmcs_sysfs(struct ist30xx_data *data)
{
	int ret = 0;

	/* /sys/class/touch/cmcs */
	ist30xx_cmcs_dev = device_create(ist30xx_class, NULL, 0, data, "cmcs");

	/* /sys/class/touch/cmcs/... */
	if (unlikely(sysfs_create_group(&ist30xx_cmcs_dev->kobj,
					&cmcs_attr_group)))
		tsp_err("Failed to create sysfs group(%s)!\n", "cmcs");

#if INTERNAL_CMCS_BIN
	ts_cmcs_bin = (u8 *)ist30xx_cmcs;
	ts_cmcs_bin_size = sizeof(ist30xx_cmcs);

	ret = ist30xx_get_cmcs_info(ts_cmcs_bin, ts_cmcs_bin_size);
#endif

	return ret;
}

#if  WT_CTP_OPEN_SHORT_TEST

static ssize_t  opentest_proc_write(struct file *filp, const char __user *userbuf, size_t count, loff_t *ppos);
static ssize_t  opentest_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static  const struct file_operations  proc_opentest_file_fops = {
	.write = opentest_proc_write,
	.read = opentest_proc_read,
};

static ssize_t  opentest_proc_write(struct file *filp, const char __user *userbuf, size_t count, loff_t *ppos)
{
	return -EPERM;
}

static ssize_t  opentest_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	char *ptr = buf;
	int result = 0;
	int ret = 0;

	if (*ppos) {
		tsp_err("tp test again return\n");
		return 0;
	}
	*ppos += count;

	ts_cmcs_bin = (u8 *)ist30xx_cmcs;
	ts_cmcs_bin_size = sizeof(ist30xx_cmcs);
	ret = ist30xx_get_cmcs_info(ts_cmcs_bin, ts_cmcs_bin_size);

	result = cmcs_test_result(global_ts_data);

	return sprintf(ptr, "result=%d\n", result);

}

 static ssize_t ctp_lockdown_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
 static ssize_t ctp_lockdown_proc_write(struct file *filp, const char __user *userbuf, size_t count, loff_t *ppos);
 static const struct file_operations ctp_lockdown_procs_fops = {
	.write = ctp_lockdown_proc_write,
	.read = ctp_lockdown_proc_read,
	.owner = THIS_MODULE,
};

 static ssize_t ctp_lockdown_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	char *ptr = buf;

	if (*ppos)
		return 0;
	*ppos += count;

	return sprintf(ptr, "Lockdown info: %02X %02X %02X %02X %02X %02X %02X %02X\n",
				  Lockdown_Info_High.lockinfo[3], Lockdown_Info_High.lockinfo[2],
				  Lockdown_Info_High.lockinfo[1], Lockdown_Info_High.lockinfo[0],
				  Lockdown_Info_LOW.lockinfo[3], Lockdown_Info_LOW.lockinfo[2],
				  Lockdown_Info_LOW.lockinfo[1], Lockdown_Info_LOW.lockinfo[0]);

}

 static ssize_t ctp_lockdown_proc_write(struct file *filp, const char __user *userbuf, size_t count, loff_t *ppos)
{
	return -EPERM;
}


void create_tp_proc(void)
{
	struct proc_dir_entry  *msg_tp_test = NULL;
	struct proc_dir_entry  *opentest = NULL;
	struct proc_dir_entry *ctp_lockdown_proc = NULL;


	msg_tp_test = proc_mkdir(IST_PROC_MSG_ITO_TESE, NULL);
	if (NULL == msg_tp_test) {
		tsp_err("creating  /proc/ctp/  failed\n");
	}

	opentest = proc_create(IST_PROC_ITO_OPENTEST_DEBUG, ITO_TEST_AUTHORITY, msg_tp_test, &proc_opentest_file_fops);
	if (NULL == opentest) {
		tsp_err("creating  /proc/ctp/openteset  failed\n");
	}

	ctp_lockdown_proc = proc_create(CTP_LOCKDOWN_INFOR_NAME, 0666, msg_tp_test, &ctp_lockdown_procs_fops);
	if (ctp_lockdown_proc == NULL) {
		tsp_err("ft5x06: create ctp_lockdown_proc fail\n");
	}
}
#endif

#endif
