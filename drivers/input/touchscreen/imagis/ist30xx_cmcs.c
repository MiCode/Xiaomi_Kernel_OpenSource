/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *  Copyright (C) 2015 XiaoMi, Inc.
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
#include "ist30xx_cmcs.h"

#define CMCS_PARSING_DEBUG      (0)

#define CMCS_READY          (0)
#define CMCS_NOT_READY      (-1)

#define TSP_CH_UNUSED       (0)
#define TSP_CH_SCREEN       (1)
#define TSP_CH_KEY          (2)
#define TSP_CH_UNKNOWN      (-1)

int ist30xx_parse_cmcs_bin(struct ist30xx_data *data, const u8 *buf, const u32 size)
{
	int ret = -EPERM;
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	memcpy(data->cmcs->magic1, buf, sizeof(data->cmcs->magic1));
	memcpy(data->cmcs->magic2, &buf[size - sizeof(data->cmcs->magic2)],
	       sizeof(data->cmcs->magic2));
	memcpy(cmcs, &buf[sizeof(data->cmcs->magic1)], sizeof(data->cmcs->cmcs));

	if (!strncmp(data->cmcs->magic1, IST30XX_CMCS_MAGIC, sizeof(data->cmcs->magic1))
	    && !strncmp(data->cmcs->magic2, IST30XX_CMCS_MAGIC, sizeof(data->cmcs->magic2))) {
		int idx;

		idx = sizeof(data->cmcs->magic1) + sizeof(data->cmcs->cmcs);
		data->cmcs->buf_cmcs = (u8 *)&buf[idx];

		idx += cmcs->cmd.cmcs_size;
		data->cmcs->buf_sensor = (u32 *)&buf[idx];

		idx += (cmcs->sensor1_size + cmcs->sensor2_size + cmcs->sensor3_size);
		data->cmcs->buf_node = (u16 *)&buf[idx];

		ret = 0;
	}

	tsp_verb("Magic1: %s, Magic2: %s\n", data->cmcs->magic1, data->cmcs->magic2);
	tsp_verb(" mode: 0x%x, base(screen: %d, key: %d)\n",
		 cmcs->cmd.mode, cmcs->cmd.base_screen, cmcs->cmd.base_key);
	tsp_verb(" start_cp (cm: %d, cs: %d), vcmp (cm: %d, cs: %d)\n",
		 cmcs->cmd.start_cp_cm, cmcs->cmd.start_cp_cs,
		 cmcs->cmd.vcmp_cm, cmcs->cmd.vcmp_cs);
	tsp_verb(" timeout: %d\n", cmcs->timeout);
	tsp_verb(" baseline scrn: 0x%08x, key: 0x%08x\n",
		 cmcs->addr.base_screen, cmcs->addr.base_key);
	tsp_verb(" start_cp: 0x%08x, vcmp: 0x%08x\n",
		 cmcs->addr.start_cp, cmcs->addr.vcmp);
	tsp_verb(" sensor 1: 0x%08x, 2: 0x%08x, 3: 0x%08x\n",
		 cmcs->addr.sensor1, cmcs->addr.sensor2, cmcs->addr.sensor3);
	tsp_verb(" tx: %d, rx:%d, key rx: %d, num(%d, %d, %d, %d, %d)\n",
		 cmcs->ch.tx_num, cmcs->ch.rx_num, cmcs->ch.key_rx, cmcs->ch.key1,
		 cmcs->ch.key2, cmcs->ch.key3, cmcs->ch.key4, cmcs->ch.key5);
	tsp_verb(" cr: screen(%4d, %4d), key(%4d, %4d)\n",
		 cmcs->spec_cr.screen_min, cmcs->spec_cr.screen_max,
		 cmcs->spec_cr.key_min, cmcs->spec_cr.key_max);
	tsp_verb(" cm: screen(%4d, %4d), key(%4d, %4d)\n",
		 cmcs->spec_cm.screen_min, cmcs->spec_cm.screen_max,
		 cmcs->spec_cm.key_min, cmcs->spec_cm.key_max);
	tsp_verb(" cs0: screen(%4d, %4d), key(%4d, %4d)\n",
		 cmcs->spec_cs0.screen_min, cmcs->spec_cs0.screen_max,
		 cmcs->spec_cs0.key_min, cmcs->spec_cs0.key_max);
	tsp_verb(" cs1: screen(%4d, %4d), key(%4d, %4d)\n",
		 cmcs->spec_cs1.screen_min, cmcs->spec_cs1.screen_max,
		 cmcs->spec_cs1.key_min, cmcs->spec_cs1.key_max);
	tsp_verb(" slope - x(%d, %d), y(%d, %d)\n",
		 cmcs->slope.x_min, cmcs->slope.x_max,
		 cmcs->slope.y_min, cmcs->slope.y_max);
	tsp_verb(" size - cmcs(%d), sensor(%d, %d, %d)\n",
		 cmcs->cmd.cmcs_size, cmcs->sensor1_size,
		 cmcs->sensor2_size, cmcs->sensor3_size);
	tsp_verb(" checksum - cmcs: 0x%08x, sensor: 0x%08x\n",
		 cmcs->cmcs_chksum, cmcs->sensor_chksum);
	tsp_verb(" cmcs: %x, %x, %x, %x\n", data->cmcs->buf_cmcs[0],
		 data->cmcs->buf_cmcs[1], data->cmcs->buf_cmcs[2],
		 data->cmcs->buf_cmcs[3]);
	tsp_verb(" sensor: %x, %x, %x, %x\n",
		 data->cmcs->buf_sensor[0], data->cmcs->buf_sensor[1],
		 data->cmcs->buf_sensor[2], data->cmcs->buf_sensor[3]);
	tsp_verb(" node: %x, %x, %x, %x\n",
		 data->cmcs->buf_node[0], data->cmcs->buf_node[1],
		 data->cmcs->buf_node[2], data->cmcs->buf_node[3]);

	return ret;
}

int ist30xx_get_cmcs_info(struct ist30xx_data *data, const u8 *buf, const u32 size)
{
	int ret;

	data->cmcs_ready = CMCS_NOT_READY;

	ret = ist30xx_parse_cmcs_bin(data, buf, size);
	if (unlikely(ret != TAGS_PARSE_OK))
		tsp_warn("Cannot find tags of CMCS, make a bin by 'cmcs2bin.exe'\n");

	return ret;
}

int ist30xx_set_cmcs_sensor(struct i2c_client *client, CMCS_INFO *cmcs, u32 *buf32)
{
	int i, ret;
	int len;
	u32 waddr;
	u32 *tmp32;

	tsp_verb("%08x %08x %08x %08x\n", buf32[0], buf32[1], buf32[2], buf32[3]);

	waddr = cmcs->addr.sensor1;
	len = cmcs->sensor1_size / IST30XX_DATA_LEN;

	for (i = 0; i < len; i++) {
		ret = ist30xx_write_cmd(client, waddr, *buf32++);
		if (ret) return ret;

		waddr += IST30XX_DATA_LEN;
	}

	tmp32 = buf32;
	tsp_verb("%08x %08x %08x %08x\n", tmp32[0], tmp32[1], tmp32[2], tmp32[3]);

	waddr = cmcs->addr.sensor2;
	len = (cmcs->sensor2_size - 0x10) / IST30XX_DATA_LEN;

	for (i = 0; i < len; i++) {
		ret = ist30xx_write_cmd(client, waddr, *buf32++);
		if (ret) return ret;

		waddr += IST30XX_DATA_LEN;
	}
	buf32 += (0x10 / IST30XX_DATA_LEN);

	tmp32 = buf32;
	tsp_verb("%08x %08x %08x %08x\n", tmp32[0], tmp32[1], tmp32[2], tmp32[3]);

	waddr = cmcs->addr.sensor3;
	len = cmcs->sensor3_size / IST30XX_DATA_LEN;

	for (i = 0; i < len; i++) {
		ret = ist30xx_write_cmd(client, waddr, *buf32++);
		if (ret) return ret;

		waddr += IST30XX_DATA_LEN;
	}

	return 0;
}

int ist30xx_set_cmcs_cmd(struct i2c_client *client, CMCS_INFO *cmcs)
{
	int ret;
	u32 val;

	val = (u32)(cmcs->cmd.base_screen | (cmcs->cmd.mode << 16));
	ret = ist30xx_write_cmd(client, cmcs->addr.base_screen, val);
	if (ret) return ret;
	tsp_verb("Baseline screen(0x%08x): 0x%08x\n", cmcs->addr.base_screen, val);

	val = (u32)cmcs->cmd.base_key;
	ret = ist30xx_write_cmd(client, cmcs->addr.base_key, val);
	if (ret) return ret;
	tsp_verb("Baseline key(0x%08x): 0x%08x\n", cmcs->addr.base_key, val);

	val = cmcs->cmd.start_cp_cm | (cmcs->cmd.start_cp_cs << 16);
	ret = ist30xx_write_cmd(client, cmcs->addr.start_cp, val);
	if (ret) return ret;
	tsp_verb("StartCP(0x%08x): 0x%08x\n", cmcs->addr.start_cp, val);

	val = cmcs->cmd.vcmp_cm | (cmcs->cmd.vcmp_cs << 16);
	ret = ist30xx_write_cmd(client, cmcs->addr.vcmp, val);
	if (ret) return ret;
	tsp_verb("VCMP(0x%08x): 0x%08x\n", cmcs->addr.vcmp, val);

	return 0;
}

int ist30xx_parse_cmcs_buf(CMCS_INFO *cmcs, s16 *buf)
{
	int i, j;

	tsp_info(" %d * %d\n", cmcs->ch.tx_num, cmcs->ch.rx_num);
	for (i = 0; i < cmcs->ch.tx_num; i++) {
		tsp_info(" ");
		for (j = 0; j < cmcs->ch.rx_num; j++)
			printk("%5d ", buf[i * cmcs->ch.rx_num + j]);
		printk("\n");
	}

	return 0;
}

int ist30xx_load_cmcs_binary(struct ist30xx_data *data)
{
	int i, ret;
	struct ist30xx_config_info *info = data->pdata->config_array;
	const struct firmware *req_cmcs_bin = NULL;

	for (i = 0; i < data->pdata->config_array_size; i++) {
		if (data->tsp_type == info[i].tsp_type)
			break;
	}

	/* If no corresponding tsp firmware is found, just use the default one */
	if (i >= data->pdata->config_array_size) {
		tsp_warn("No corresponding TSP firmware found, use the default one(%x)\n", data->tsp_type);
		i = 0;
	} else {
		tsp_info("TSP vendor: %s(%x)\n", info[i].tsp_name, data->tsp_type);
	}

	ret = request_firmware(&req_cmcs_bin, info[i].cmcs_name, &data->client->dev);
	if (unlikely(ret)) {
		tsp_err("Cannot load CMCS binary file: %s\n", info[i].cmcs_name);
		return ret;
	}

	data->cmcs_bin = kmalloc(req_cmcs_bin->size, GFP_KERNEL);
	if (unlikely(!data->cmcs_bin)) {
		tsp_err("Cannot allocate memory for CMCS binary.\n");
		release_firmware(req_cmcs_bin);
		return -ENOMEM;
	}

	memcpy(data->cmcs_bin, req_cmcs_bin->data, req_cmcs_bin->size);
	data->cmcs_bin_size = (u32)req_cmcs_bin->size;
	tsp_info("CMCS binary [%s] loaded successfully.\n", info[i].cmcs_name);
	release_firmware(req_cmcs_bin);

	return 0;
}

int ist30xx_apply_cmcs_slope(struct ist30xx_data *data, CMCS_INFO *cmcs, CMCS_BUF *cmcs_buf)
{
	int i, j;
	int idx1, idx2;
	int ch_num = cmcs->ch.tx_num * cmcs->ch.rx_num;
	int width = cmcs->ch.rx_num;
	int height = cmcs->ch.tx_num;
	s16 *pcm = (s16 *)cmcs_buf->cm;
	s16 *pspec = (s16 *)cmcs_buf->spec;
	s16 *pslope0 = (s16 *)cmcs_buf->slope0;
	s16 *pslope1 = (s16 *)cmcs_buf->slope1;

	if (cmcs->ch.key_rx)
		width -= 1;
	else
		height -= 1;

	memset(cmcs_buf->slope0, 0, sizeof(cmcs_buf->slope0));
	memset(cmcs_buf->slope1, 0, sizeof(cmcs_buf->slope1));

	memcpy(cmcs_buf->spec, data->cmcs->buf_node, (ch_num * sizeof(u16)));

	idx1 = 0;
#if CMCS_PARSING_DEBUG
	tsp_info("# Node specific\n");
	for (i = 0; i < cmcs->ch.tx_num; i++) {
		tsp_info(" ");
		for (j = 0; j < cmcs->ch.rx_num; j++)
			printk("%5d ", cmcs_buf->spec[idx1++]);
		printk("\n");
	}
#endif

	tsp_verb("# Apply slope0_x\n");
	for (i = 0; i < height; i++) {
		for (j = 0; j < width - 1; j++) {
			idx1 = (i * cmcs->ch.rx_num) + j;
			idx2 = idx1 + 1;

			pslope0[idx1] = (pcm[idx2] - pcm[idx1]);
			pslope0[idx1] += (pspec[idx1] - pspec[idx2]);
		}
	}

	tsp_verb("# Apply slope1_y\n");
	for (i = 0; i < height - 1; i++) {
		for (j = 0; j < width; j++) {
			idx1 = (i * cmcs->ch.rx_num) + j;
			idx2 = idx1 + cmcs->ch.rx_num;

			pslope1[idx1] = (pcm[idx2] - pcm[idx1]);
			pslope1[idx1] += (pspec[idx1] - pspec[idx2]);
		}
	}

#if CMCS_PARSING_DEBUG
	tsp_info("# slope0_x\n");
	for (i = 0; i < height; i++) {
		tsp_info(" ");
		for (j = 0; j < width; j++) {
			idx1 = (i * cmcs->ch.rx_num) + j;
			printk("%5d ", pslope0[idx1]);
		}
		printk("\n");
	}

	tsp_info("# slope1_y\n");
	for (i = 0; i < height; i++) {
		tsp_info(" ");
		for (j = 0; j < width; j++) {
			idx1 = (i * cmcs->ch.rx_num) + j;
			printk("%5d ", pslope1[idx1]);
		}
		printk("\n");
	}
#endif

	return 0;
}


int ist30xx_get_cmcs_buf(struct i2c_client *client, CMCS_INFO *cmcs, s16 *buf)
{
	int ret;
	u16 len = (IST30XX_CMCS_BUF_SIZE * sizeof(buf[0])) / IST30XX_DATA_LEN;

	ret = ist30xx_read_buf(client, CMD_DEFAULT, (u32 *)buf, len);
	if (ret) return ret;

#if CMCS_PARSING_DEBUG
	ret = ist30xx_parse_cmcs_buf(cmcs, buf);
#endif

	return ret;
}


extern int ist30xx_calib_wait(struct ist30xx_data *data);
#define cmcs_next_step(ret)   { if (unlikely(ret)) goto end; msleep(10); }
int ist30xx_cmcs_test(struct ist30xx_data *data, const u8 *buf, int size)
{
	int ret;
	int len;
	u32 chksum = 0;
	u32 *buf32;
	struct i2c_client *client = (struct i2c_client *)data->client;
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	tsp_info("*** CM/CS test ***\n");
	tsp_info(" mode: 0x%x, baseline(screen: %d, key: %d)\n",
		 cmcs->cmd.mode, cmcs->cmd.base_screen, cmcs->cmd.base_key);
	tsp_info(" start_cp (cm: %d, cs: %d), vcmp (cm: %d, cs: %d)\n",
		 cmcs->cmd.start_cp_cm, cmcs->cmd.start_cp_cs,
		 cmcs->cmd.vcmp_cm, cmcs->cmd.vcmp_cs);

	ist30xx_disable_irq(data);

	ret = ist30xx_cmd_run_device(client, true);
	cmcs_next_step(ret);

	ret = ist30xx_cmd_reg(client, CMD_ENTER_REG_ACCESS);
	cmcs_next_step(ret);

	/* Set sensor register */
	buf32 = data->cmcs->buf_sensor;
	ret = ist30xx_set_cmcs_sensor(client, cmcs, buf32);
	cmcs_next_step(ret);

	/* Set command */
	ret = ist30xx_set_cmcs_cmd(client, cmcs);
	cmcs_next_step(ret);

	ret = ist30xx_cmd_reg(client, CMD_EXIT_REG_ACCESS);
	cmcs_next_step(ret);

	/* Load cmcs test code */
	ret = ist30xx_write_cmd(client, CMD_EXEC_MEM_CODE, 0);
	cmcs_next_step(ret);

	buf32 = (u32 *)data->cmcs->buf_cmcs;
	len = cmcs->cmd.cmcs_size / IST30XX_DATA_LEN;
	tsp_verb("%08x %08x %08x %08x\n", buf32[0], buf32[1], buf32[2], buf32[3]);
	ret = ist30xx_write_buf(client, len, buf32, len);
	cmcs_next_step(ret);

	/* Check checksum */
	ret = ist30xx_read_cmd(client, CMD_DEFAULT, &chksum);
	cmcs_next_step(ret);
	if (chksum != IST30XX_CMCS_LOAD_END)
		goto end;
	tsp_info("CM/CS code ready!!\n");

	/* Check checksum */
	ret = ist30xx_read_cmd(client, CMD_DEFAULT, &chksum);
	cmcs_next_step(ret);
	tsp_info("CM/CS code chksum: %08x, %08x\n", chksum, cmcs->cmcs_chksum);

	ist30xx_enable_irq(data);
	/* Wait CMCS test result */
	if (ist30xx_calib_wait(data) == 1)
		tsp_info("CM/CS test OK.\n");
	else
		tsp_info("CM/CS test fail.\n");
	ist30xx_disable_irq(data);

	/* Read CM/CS data*/
	if (ENABLE_CM_MODE(cmcs->cmd.mode)) {
		/* Read CM data */
		memset(data->cmcs_buf->cm, 0, sizeof(data->cmcs_buf->cm));

		ret = ist30xx_get_cmcs_buf(client, cmcs, data->cmcs_buf->cm);
		cmcs_next_step(ret);

		ret = ist30xx_apply_cmcs_slope(data, cmcs, data->cmcs_buf);
	}

	if (ENABLE_CS_MODE(cmcs->cmd.mode)) {
		/* Read CS0 data */
		memset(data->cmcs_buf->cs0, 0, sizeof(data->cmcs_buf->cs0));
		memset(data->cmcs_buf->cs1, 0, sizeof(data->cmcs_buf->cs1));

		ret = ist30xx_get_cmcs_buf(client, cmcs, data->cmcs_buf->cs0);
		cmcs_next_step(ret);

		/* Read CS1 data */
		ret = ist30xx_get_cmcs_buf(client, cmcs, data->cmcs_buf->cs1);
		cmcs_next_step(ret);
	}

	ret = ist30xx_cmd_run_device(client, true);
	cmcs_next_step(ret);

	ist30xx_start(data);

	data->cmcs_ready = CMCS_READY;

end:
	if (unlikely(ret)) {
		tsp_warn("CM/CS test Fail!, ret=%d\n", ret);
	} else if (unlikely(chksum != cmcs->cmcs_chksum)) {
		tsp_warn("Error CheckSum: %x(%x)\n", chksum, cmcs->cmcs_chksum);
		ret = -ENOEXEC;
	}

	ist30xx_enable_irq(data);

	return ret;
}


int check_tsp_type(struct ist30xx_data *data, int tx, int rx)
{
	struct CMCS_CH_INFO *ch = (struct CMCS_CH_INFO *)&data->cmcs->cmcs.ch;

	int last_rx_ch = (int)ch->rx_num - 1;
	int last_tx_ch = (int)ch->tx_num - 1;

	if ((rx > last_rx_ch) || (rx < 0) || (tx > last_tx_ch) || (tx < 0)) {
		tsp_warn("TSP channel is not correct!! (%d * %d)\n", tx, rx);
		return TSP_CH_UNKNOWN;
	}

	if (ch->key_rx) {  // Key on RX channel
		if (rx == last_rx_ch) {
			if ((tx == ch->key1) || (tx == ch->key2) || (tx == ch->key3) ||
			    (tx == ch->key4) || (tx == ch->key5))
				return TSP_CH_KEY;
			else
				return TSP_CH_UNUSED;
		}
	}                       // Key on TX channel
	else {
		if (tx == last_tx_ch) {
			if ((rx == ch->key1) || (rx == ch->key2) || (rx == ch->key3) ||
			    (rx == ch->key4) || (rx == ch->key5))
				return TSP_CH_KEY;
			else
				return TSP_CH_UNUSED;
		}
	}

	return TSP_CH_SCREEN;
}

int print_cmcs(struct ist30xx_data *data, s16 *buf16, char *buf)
{
	int i, j;
	int idx;
	int count = 0;
	char msg[128];

	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	int tx_num = cmcs->ch.tx_num;
	int rx_num = cmcs->ch.rx_num;

	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			idx = (i * cmcs->ch.rx_num) + j;
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
	int key_index[5] = { 0, };
	int key_cnt = 0;
	char msg[128];

	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	int tx_num = cmcs->ch.tx_num;
	int rx_num = cmcs->ch.rx_num;

	if ((mode == CMCS_FLAG_CM_SLOPE0) || (mode == CMCS_FLAG_CM_SLOPE1)) {
		if (cmcs->ch.key_rx)
			rx_num--;
		else
			tx_num--;
	}

	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			type = check_tsp_type(data, i, j);
			if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
				continue;   // Ignore

			if ((mode == CMCS_FLAG_CM_SLOPE0) && (j == (rx_num - 1)))
				continue;
			else if ((mode == CMCS_FLAG_CM_SLOPE1) && (i == (tx_num - 1)))
				continue;

			idx = (i * cmcs->ch.rx_num) + j;

			if (type == TSP_CH_KEY) {
				key_index[key_cnt++] = idx;
				continue;
			}

			count += sprintf(msg, "%5d ", buf16[idx]);
			strcat(buf, msg);
		}
	}

	tsp_info("key cnt: %d\n", key_cnt);
	if ((mode != CMCS_FLAG_CM_SLOPE0) && (mode != CMCS_FLAG_CM_SLOPE1)) {
		tsp_info("key cnt: %d\n", key_cnt);
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
ssize_t ist30xx_cmcs_info_show(struct device *dev, struct device_attribute *attr,
			       char *buf)
{
	int count;
	char msg[128];
	struct ist30xx_data *data = dev_get_drvdata(dev);

	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if (cmcs == NULL)
		return sprintf(buf, "Unknown cmcs bin\n");

	/* Mode */
	count = sprintf(msg, "%d ", cmcs->cmd.mode);
	strcat(buf, msg);

	/* Channel */
	count += sprintf(msg, "%d %d %d %d %d %d %d %d ",
			 cmcs->ch.tx_num, cmcs->ch.rx_num, cmcs->ch.key_rx, cmcs->ch.key1,
			 cmcs->ch.key2, cmcs->ch.key3, cmcs->ch.key4, cmcs->ch.key5);
	strcat(buf, msg);

	/* Slope */
	count += sprintf(msg, "%d %d %d %d ",
			 cmcs->slope.x_min, cmcs->slope.x_max,
			 cmcs->slope.y_min, cmcs->slope.y_max);
	strcat(buf, msg);

	/* CM */
	count += sprintf(msg, "%d %d %d %d ",
			 cmcs->spec_cm.screen_min, cmcs->spec_cm.screen_max,
			 cmcs->spec_cm.key_min, cmcs->spec_cm.key_max);
	strcat(buf, msg);

	/* CS0 */
	count += sprintf(msg, "%d %d %d %d ",
			 cmcs->spec_cs0.screen_min, cmcs->spec_cs0.screen_max,
			 cmcs->spec_cs0.key_min, cmcs->spec_cs0.key_max);
	strcat(buf, msg);

	/* CS1 */
	count += sprintf(msg, "%d %d %d %d ",
			 cmcs->spec_cs1.screen_min, cmcs->spec_cs1.screen_max,
			 cmcs->spec_cs1.key_min, cmcs->spec_cs1.key_max);
	strcat(buf, msg);

	/* CR */
	count += sprintf(msg, "%d %d %d %d ",
			 cmcs->spec_cr.screen_min, cmcs->spec_cr.screen_max,
			 cmcs->spec_cr.key_min, cmcs->spec_cr.key_max);
	strcat(buf, msg);

	tsp_verb("%s\n", buf);

	return count;
}

/* sysfs: /sys/class/touch/cmcs/cmcs_binary */
ssize_t ist30xx_cmcs_binary_show(struct device *dev, struct device_attribute *attr,
				 char *buf)
{
	int ret;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	ret = ist30xx_load_cmcs_binary(data);
	if (unlikely(ret)) {
		return sprintf(buf, "Binary loaded failed(%d).\n", data->cmcs_bin_size);
	}

	ist30xx_get_cmcs_info(data, data->cmcs_bin, data->cmcs_bin_size);

	mutex_lock(&data->ist30xx_mutex);
	ret = ist30xx_cmcs_test(data, data->cmcs_bin, data->cmcs_bin_size);
	mutex_unlock(&data->ist30xx_mutex);

	if (likely(data->cmcs_bin != NULL)) {
		kfree(data->cmcs_bin);
		data->cmcs_bin = NULL;
		data->cmcs_bin_size = 0;
	}

	return sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n"));
}

/* sysfs: /sys/class/touch/cmcs/cmcs_custom */
ssize_t ist30xx_cmcs_custom_show(struct device *dev, struct device_attribute *attr,
				 char *buf)
{
	int ret;
	int bin_size = 0;
	u8 *bin = NULL;
	const struct firmware *req_bin = NULL;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	ret = request_firmware(&req_bin, IST30XXB_CMCS_NAME, &data->client->dev);
	if (ret)
		return sprintf(buf, "File not found, %s\n", IST30XXB_CMCS_NAME);

	bin = (u8 *)req_bin->data;
	bin_size = (u32)req_bin->size;

	ist30xx_get_cmcs_info(data, bin, bin_size);

	mutex_lock(&data->ist30xx_mutex);
	ret = ist30xx_cmcs_test(data, bin, bin_size);
	mutex_unlock(&data->ist30xx_mutex);

	release_firmware(req_bin);

	tsp_info("size: %d\n", sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n")));

	return sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n"));
}

/* sysfs: /sys/class/touch/cmcs/cm */
ssize_t ist30xx_cm_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	tsp_verb("CM (%d * %d)\n", cmcs->ch.tx_num, cmcs->ch.rx_num);

	return print_cmcs(data, data->cmcs_buf->cm, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_spec */
ssize_t ist30xx_cm_spec_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	tsp_verb("CM Spec (%d * %d)\n", cmcs->ch.tx_num, cmcs->ch.rx_num);

	return print_cmcs(data, data->cmcs_buf->spec, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_slope0 */
ssize_t ist30xx_cm_slope0_show(struct device *dev, struct device_attribute *attr,
			       char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	tsp_verb("CM Slope0_X (%d * %d)\n", cmcs->ch.tx_num, cmcs->ch.rx_num);

	return print_cmcs(data, data->cmcs_buf->slope0, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_slope1 */
ssize_t ist30xx_cm_slope1_show(struct device *dev, struct device_attribute *attr,
			       char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	tsp_verb("CM Slope1_Y (%d * %d)\n",
		 data->cmcs->cmcs.ch.tx_num, data->cmcs->cmcs.ch.rx_num);

	return print_cmcs(data, data->cmcs_buf->slope1, buf);
}

/* sysfs: /sys/class/touch/cmcs/cs0 */
ssize_t ist30xx_cs0_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CS))
		return 0;

	tsp_verb("CS0 (%d * %d)\n", cmcs->ch.tx_num, cmcs->ch.rx_num);

	return print_cmcs(data, data->cmcs_buf->cs0, buf);
}

/* sysfs: /sys/class/touch/cmcs/cs1 */
ssize_t ist30xx_cs1_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CS))
		return 0;

	tsp_verb("CS1 (%d * %d)\n", cmcs->ch.tx_num, cmcs->ch.rx_num);

	return print_cmcs(data, data->cmcs_buf->cs1, buf);
}


int print_cm_slope_result(struct ist30xx_data *data, u8 flag, s16 *buf16, char *buf)
{
	int i, j;
	int type, idx;
	int count = 0, err_cnt = 0;
	int min_spec, max_spec;

	char msg[128];

	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;
	struct CMCS_SLOPE_INFO *spec = (struct CMCS_SLOPE_INFO *)&cmcs->slope;

	if (flag == CMCS_FLAG_CM_SLOPE0) {
		min_spec = spec->x_min;
		max_spec = spec->x_max;
	} else if (flag == CMCS_FLAG_CM_SLOPE1) {
		min_spec = spec->y_min;
		max_spec = spec->y_max;
	} else {
		count = sprintf(msg, "Unknown flag: %d\n", flag);
		strcat(buf, msg);
		return count;
	}

	min_spec *= -1;

	tsp_debug("CS Slope Spec: %d ~ %d\n", min_spec, max_spec);

	for (i = 0; i < cmcs->ch.tx_num; i++) {
		for (j = 0; j < cmcs->ch.rx_num; j++) {
			idx = (i * cmcs->ch.rx_num) + j;

			type = check_tsp_type(data, i, j);
			if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
				continue;   // Ignore

			if ((buf16[idx] > min_spec) && (buf16[idx] < max_spec))
				continue;   // OK

			count += sprintf(msg, "%2d,%2d:%4d\n", i, j, buf16[idx]);
			strcat(buf, msg);

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

int print_cs_result(struct ist30xx_data *data, s16 *buf16, char *buf, int cs_num)
{
	int i, j;
	int type, idx;
	int count = 0, err_cnt = 0;
	int min_spec, max_spec;

	char msg[128];

	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;
	struct CMCS_SPEC_INFO *spec;
	if (cs_num == 0)
		spec = (struct CMCS_SPEC_INFO *)&cmcs->spec_cs0;
	else
		spec = (struct CMCS_SPEC_INFO *)&cmcs->spec_cs1;

	tsp_debug("CS %d Spec: screen(%d~%d), key(%d~%d)\n", cs_num,
		spec->screen_min, spec->screen_max, spec->key_min, spec->key_max);

	for (i = 0; i < cmcs->ch.tx_num; i++) {
		for (j = 0; j < cmcs->ch.rx_num; j++) {
			idx = (i * cmcs->ch.rx_num) + j;

			type = check_tsp_type(data, i, j);
			if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
				continue;   // Ignore

			if (type == TSP_CH_SCREEN) {
				min_spec = spec->screen_min;
				max_spec = spec->screen_max;
			} else {    // TSP_CH_KEY
				min_spec = spec->key_min;
				max_spec = spec->key_max;
			}

			if ((buf16[idx] > min_spec) && (buf16[idx] < max_spec))
				continue;   // OK

			count += sprintf(msg, "%2d,%2d:%4d\n", i, j, buf16[idx]);
			strcat(buf, msg);

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

int print_cm_result(struct ist30xx_data *data, char *buf)
{
	int i, j;
	int type, idx, err_cnt = 0;
	int min_spec, max_spec;
	int count = 0;
	short cm;
	char msg[128];
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	for (i = 0; i < cmcs->ch.tx_num; i++) {
		for (j = 0; j < cmcs->ch.rx_num; j++) {
			idx = (i * cmcs->ch.rx_num) + j;

			type = check_tsp_type(data, i, j);
			//tsp_info("CH: %d (%d, %d) - %d\n", idx, i, j, type);
			if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
				continue;

			min_spec = max_spec = data->cmcs_buf->spec[idx];
			if (type == TSP_CH_SCREEN) {
				min_spec -= (min_spec * cmcs->spec_cm.screen_min / 100);
				max_spec += (min_spec * cmcs->spec_cm.screen_max / 100);
			} else {    // TSP_CH_KEY
				min_spec -= (min_spec * cmcs->spec_cm.key_min / 100);
				max_spec += (min_spec * cmcs->spec_cm.key_max / 100);
			}

			cm = data->cmcs_buf->cm[idx];
			if ((cm > min_spec) && (cm < max_spec))
				continue; // OK

			count += sprintf(msg, "%2d,%2d:%4d (%4d~%4d)\n",
					 i, j, cm, min_spec, max_spec);
			strcat(buf, msg);

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

/* sysfs: /sys/class/touch/cmcs/cm_result */
ssize_t ist30xx_cm_result_show(struct device *dev, struct device_attribute *attr,
			       char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	return print_cm_result(data, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_slope0_result */
ssize_t ist30xx_cm_slope0_result_show(struct device *dev, struct device_attribute *attr,
				      char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	return print_cm_slope_result(data, CMCS_FLAG_CM_SLOPE0,
				     data->cmcs_buf->slope0, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_slope1_result */
ssize_t ist30xx_cm_slope1_result_show(struct device *dev, struct device_attribute *attr,
				      char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	return print_cm_slope_result(data, CMCS_FLAG_CM_SLOPE1,
				     data->cmcs_buf->slope1, buf);
}

/* sysfs: /sys/class/touch/cmcs/cs0_result */
ssize_t ist30xx_cs0_result_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CS))
		return 0;

	return print_cs_result(data, data->cmcs_buf->cs0, buf, 0);
}

/* sysfs: /sys/class/touch/cmcs/cs1_result */
ssize_t ist30xx_cs1_result_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CS))
		return 0;

	return print_cs_result(data, data->cmcs_buf->cs1, buf, 1);
}

/* sysfs: /sys/class/touch/cmcs/line_cm */
ssize_t ist30xx_line_cm_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	return print_line_cmcs(data, CMCS_FLAG_CM, data->cmcs_buf->cm, buf);
}

/* sysfs: /sys/class/touch/cmcs/line_cm_slope0 */
ssize_t ist30xx_line_cm_slope0_show(struct device *dev, struct device_attribute *attr,
				    char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	return print_line_cmcs(data, CMCS_FLAG_CM_SLOPE0, data->cmcs_buf->slope0, buf);
}

/* sysfs: /sys/class/touch/cmcs/line_cm_slope1 */
ssize_t ist30xx_line_cm_slope1_show(struct device *dev, struct device_attribute *attr,
				    char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	return print_line_cmcs(data, CMCS_FLAG_CM_SLOPE1, data->cmcs_buf->slope1, buf);
}

/* sysfs: /sys/class/touch/cmcs/line_cs0 */
ssize_t ist30xx_line_cs0_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CS))
		return 0;

	return print_line_cmcs(data, CMCS_FLAG_CS0, data->cmcs_buf->cs0, buf);
}

/* sysfs: /sys/class/touch/cmcs/line_cs1 */
ssize_t ist30xx_line_cs1_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	if (data->cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CS))
		return 0;

	return print_line_cmcs(data, CMCS_FLAG_CS1, data->cmcs_buf->cs1, buf);
}

ssize_t ist30xx_cmcs_test_all_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	int ret;
	char* msg = NULL;

	struct ist30xx_data *data = dev_get_drvdata(dev);
	CMCS_INFO *cmcs = (CMCS_INFO *)&data->cmcs->cmcs;

	msg = kzalloc(sizeof(char) * 4096, GFP_KERNEL);
	if (!msg) {
		tsp_err("Memory allocation failed\n");
		return 0;
	}

	/* CMCS Binary */
	ret = ist30xx_load_cmcs_binary(data);
	if (unlikely(ret)) {
		kfree(msg);
		return sprintf(buf, "Binary loaded failed(%d).\n", data->cmcs_bin_size);
	}

	ist30xx_get_cmcs_info(data, data->cmcs_bin, data->cmcs_bin_size);

	mutex_lock(&data->ist30xx_mutex);
	ret = ist30xx_cmcs_test(data, data->cmcs_bin, data->cmcs_bin_size);
	mutex_unlock(&data->ist30xx_mutex);

	if (likely(data->cmcs_bin != NULL)) {
		kfree(data->cmcs_bin);
		data->cmcs_bin = NULL;
		data->cmcs_bin_size = 0;
	}

	if (ret == 0) {
		tsp_debug("CMCS Binary test passed!\n");
	} else {
		kfree(msg);
		return sprintf(buf, "CMCS Binary test failed!\n");
	}

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM) && !(cmcs->cmd.mode & FLAG_ENABLE_CS)) {
		kfree(msg);
		return sprintf(buf, "CMCS not enabled!\n");
	}

	/* CM Result */
	memset(msg, 0, sizeof(msg));
	ret = print_cm_result(data, msg);
	if (strncmp(msg, "OK\n", strlen("OK\n")) == 0) {
		tsp_debug("CM result test passed!\n");
	} else {
		goto out;
	}

	/* CM Slope 0 Result */
	memset(msg, 0, sizeof(msg));
	ret = print_cm_slope_result(data, CMCS_FLAG_CM_SLOPE0,
				data->cmcs_buf->slope0, msg);
	if (strncmp(msg, "OK\n", strlen("OK\n")) == 0) {
		tsp_debug("CM Slope 0 test passed!\n");
	} else {
		goto out;
	}

	/* CM Slope 1 Result */
	memset(msg, 0, sizeof(msg));
	ret = print_cm_slope_result(data, CMCS_FLAG_CM_SLOPE1,
				data->cmcs_buf->slope1, msg);
	if (strncmp(msg, "OK\n", strlen("OK\n")) == 0) {
		tsp_debug("CM Slope 1 test passed!\n");
	} else {
		goto out;
	}

	/* CS 0 Result */
	memset(msg, 0, sizeof(msg));
	ret = print_cs_result(data, data->cmcs_buf->cs0, msg, 0);
	if (strncmp(msg, "OK\n", strlen("OK\n")) == 0) {
		tsp_debug("CS 0 test passed!\n");
	} else {
		goto out;
	}

	/* CS 1 Result */
	memset(msg, 0, sizeof(msg));
	ret = print_cs_result(data, data->cmcs_buf->cs1, msg, 1);
	if (strncmp(msg, "OK\n", strlen("OK\n")) == 0) {
		tsp_debug("CS 1 test passed!\n");
	} else {
		goto out;
	}

out:
	ret = sprintf(buf, "%s", msg);
	kfree(msg);
	return ret;
}


/* sysfs : cmcs */
static DEVICE_ATTR(info, S_IRUGO, ist30xx_cmcs_info_show, NULL);
static DEVICE_ATTR(cmcs_binary, S_IRUGO, ist30xx_cmcs_binary_show, NULL);
static DEVICE_ATTR(cmcs_custom, S_IRUGO, ist30xx_cmcs_custom_show, NULL);
static DEVICE_ATTR(cm, S_IRUGO, ist30xx_cm_show, NULL);
static DEVICE_ATTR(cm_spec, S_IRUGO, ist30xx_cm_spec_show, NULL);
static DEVICE_ATTR(cm_slope0, S_IRUGO, ist30xx_cm_slope0_show, NULL);
static DEVICE_ATTR(cm_slope1, S_IRUGO, ist30xx_cm_slope1_show, NULL);
static DEVICE_ATTR(cs0, S_IRUGO, ist30xx_cs0_show, NULL);
static DEVICE_ATTR(cs1, S_IRUGO, ist30xx_cs1_show, NULL);
static DEVICE_ATTR(cm_result, S_IRUGO, ist30xx_cm_result_show, NULL);
static DEVICE_ATTR(cm_slope0_result, S_IRUGO, ist30xx_cm_slope0_result_show, NULL);
static DEVICE_ATTR(cm_slope1_result, S_IRUGO, ist30xx_cm_slope1_result_show, NULL);
static DEVICE_ATTR(cs0_result, S_IRUGO, ist30xx_cs0_result_show, NULL);
static DEVICE_ATTR(cs1_result, S_IRUGO, ist30xx_cs1_result_show, NULL);
static DEVICE_ATTR(line_cm, S_IRUGO, ist30xx_line_cm_show, NULL);
static DEVICE_ATTR(line_cm_slope0, S_IRUGO, ist30xx_line_cm_slope0_show, NULL);
static DEVICE_ATTR(line_cm_slope1, S_IRUGO, ist30xx_line_cm_slope1_show, NULL);
static DEVICE_ATTR(line_cs0, S_IRUGO, ist30xx_line_cs0_show, NULL);
static DEVICE_ATTR(line_cs1, S_IRUGO, ist30xx_line_cs1_show, NULL);
static DEVICE_ATTR(cmcs_test_all, S_IRUGO, ist30xx_cmcs_test_all_show, NULL);

static struct attribute *cmcs_attributes[] = {
	&dev_attr_info.attr,
	&dev_attr_cmcs_binary.attr,
	&dev_attr_cmcs_custom.attr,
	&dev_attr_cm.attr,
	&dev_attr_cm_spec.attr,
	&dev_attr_cm_slope0.attr,
	&dev_attr_cm_slope1.attr,
	&dev_attr_cs0.attr,
	&dev_attr_cs1.attr,
	&dev_attr_cm_result.attr,
	&dev_attr_cm_slope0_result.attr,
	&dev_attr_cm_slope1_result.attr,
	&dev_attr_cs0_result.attr,
	&dev_attr_cs1_result.attr,
	&dev_attr_line_cm.attr,
	&dev_attr_line_cm_slope0.attr,
	&dev_attr_line_cm_slope1.attr,
	&dev_attr_line_cs0.attr,
	&dev_attr_line_cs1.attr,
	&dev_attr_cmcs_test_all.attr,
	NULL,
};

static struct attribute_group cmcs_attr_group = {
	.attrs	= cmcs_attributes,
};

int ist30xx_init_cmcs_sysfs(struct ist30xx_data *data)
{
	/* /sys/class/touch/cmcs */
	data->cmcs_dev = device_create(data->ist30xx_class, NULL, 0, data, "cmcs");

	/* /sys/class/touch/cmcs/... */
	if (unlikely(sysfs_create_group(&data->cmcs_dev->kobj,
					&cmcs_attr_group)))
		tsp_err("Failed to create sysfs group(%s)!\n", "cmcs");

	data->cmcs_bin = NULL;
	data->cmcs_bin_size = 0;

	data->cmcs = (CMCS_BIN_INFO *)&data->ist30xx_cmcs_bin;
	data->cmcs_buf = (CMCS_BUF *)&data->ist30xx_cmcs_buf;

	data->cmcs_ready = CMCS_NOT_READY;

	return 0;
}
