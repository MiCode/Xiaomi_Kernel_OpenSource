/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/fs.h>
#include <linux/firmware.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>

#include "fm_patch.h"
#include "fm_typedef.h"
#include "fm_dbg.h"
#include "fm_err.h"

static struct fm_file_read_data g_file_read_data;

int file_read_thread(void *arg)
{
	struct fm_file_read_data *data = (struct fm_file_read_data *)arg;
	const struct firmware *fw = NULL;

	if (!data)
		return 0;

	data->ret = request_firmware(&fw, data->filename, NULL);
	if (data->ret) {
		WCN_DBG(FM_ERR | CHIP, "Failed to load firmware \"%s\"\n",
			data->filename);
		release_firmware(fw);
		data->ret = -FM_EPATCH;

		complete(&data->comp);
		return 0;
	}
	WCN_DBG(FM_NTC | CHIP, "load firmware \"%s\" ok\n", data->filename);

	if (data->len >= fw->size) {
		memcpy(data->dst, fw->data, fw->size);
		WCN_DBG(FM_NTC | CHIP, "Copy file data(%p) size(%zu)\n",
			fw->data, fw->size);
		data->ret = fw->size;
	} else {
		WCN_DBG(FM_NTC | CHIP,
			"Copy file data failed fw->size(%zu) > bufsize(%d)\n",
			fw->size, data->len);
		data->ret = -FM_EPATCH;
	}
	release_firmware(fw);

	complete(&data->comp);

	return 0;
}

/*
 * fm_file_read - read FM DSP patch/coeff/hwcoeff/rom binary file
 * @filename - source file name
 * @dst - target buffer
 * @len - desired read length
 * @position - the read position
 * If success, return read length in bytes, else error code
 */
signed int fm_file_read(const signed char *filename, unsigned char *dst, signed int len, signed int position)
{
	struct fm_file_read_data *data = &g_file_read_data;
	struct task_struct *k;

	init_completion(&data->comp);

	data->filename = filename;
	data->dst = dst;
	data->len = len;
	data->position = position;
	data->ret = 0;

	k = kthread_run(file_read_thread, (void *)data, "file_read_thread");
	if (IS_ERR(k)) {
		WCN_DBG(FM_NTC | CHIP, "%s error ret:%d\n", __func__, PTR_ERR(k));
		data->ret = -FM_EPATCH;
	} else
		wait_for_completion(&data->comp);
	return data->ret;
}

signed int fm_file_write(const signed char *filename, unsigned char *dst, signed int len, signed int *ppos)
{
	signed int ret = 0;
	return ret;
}
