/* drivers/input/touchscreen/goodix_tool.c
 *
 * 2010 - 2012 Goodix Technology.
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version:1.6
 *        V1.0:2012/05/01,create file.
 *        V1.2:2012/06/08,modify some warning.
 *        V1.4:2012/08/28,modified to support GT9XX
 *        V1.6:new proc name
 */

#include "gt9xx.h"
#include <linux/mutex.h>

#define DATA_LENGTH_UINT    512
#define CMD_HEAD_LENGTH     (sizeof(st_cmd_head) - sizeof(u8 *))
static char procname[20] = {0};

#define UPDATE_FUNCTIONS

#pragma pack(1)
struct {
	u8  wr;		/* write read flag£¬0:R  1:W  2:PID 3: */
	u8  flag;	/* 0:no need flag/int 1: need flag  2:need int */
	u8 flag_addr[2];/* flag address */
	u8  flag_val;	/* flag val */
	u8  flag_relation; /* flag_val:flag 0:not equal 1:equal 2:> 3:< */
	u16 circle;	/* polling cycle */
	u8  times;	/* plling times */
	u8  retry;	/* I2C retry times */
	u16 delay;	/* delay befor read or after write */
	u16 data_len;	/* data length */
	u8  addr_len;	/* address length */
	u8  addr[2];	/* address */
	u8  res[3];	/* reserved */
	u8  *data;	/* data pointer */
} st_cmd_head;
#pragma pack()
st_cmd_head cmd_head;

static struct i2c_client *gt_client;

static struct proc_dir_entry *goodix_proc_entry;

static struct mutex lock;

static s32 goodix_tool_write(struct file *filp, const char __user *buff,
						unsigned long len, void *data);
static s32 goodix_tool_read(char *page, char **start, off_t off, int count,
							int *eof, void *data);
static s32 (*tool_i2c_read)(u8 *, u16);
static s32 (*tool_i2c_write)(u8 *, u16);

s32 DATA_LENGTH;
s8 IC_TYPE[16] = {0};

static void tool_set_proc_name(char *procname)
{
	char *months[12] = {"Jan", "Feb", "Mar", "Apr", "May",
	"Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	char date[20] = {0};
	char month[4] = {0};
	int i = 0, n_month = 1, n_day = 0, n_year = 0;
	snprintf(date, 20, "%s", __DATE__);

	/* GTP_DEBUG("compile date: %s", date); */

	sscanf(date, "%s %d %d", month, &n_day, &n_year);

	for (i = 0; i < 12; ++i) {
		if (!memcmp(months[i], month, 3)) {
			n_month = i+1;
			break;
		}
	}

	snprintf(procname, 20, "gmnode%04d%02d%02d", n_year, n_month, n_day);
	/* GTP_DEBUG("procname = %s", procname); */
}

static s32 tool_i2c_read_no_extra(u8 *buf, u16 len)
{
	s32 ret = -1;
	u8 i = 0;
	struct i2c_msg msgs[2] = {
		{
			.flags = !I2C_M_RD,
			.addr  = gt_client->addr,
			.len   = cmd_head.addr_len,
			.buf   = &buf[0],
		},
		{
			.flags = I2C_M_RD,
			.addr  = gt_client->addr,
			.len   = len,
			.buf   = &buf[GTP_ADDR_LENGTH],
		},
	};

	for (i = 0; i < cmd_head.retry; i++) {
		ret = i2c_transfer(gt_client->adapter, msgs, 2);
		if (ret > 0)
			break;
	}

	if (i == cmd_head.retry) {
		dev_err(&client->dev, "I2C read retry limit over.\n");
		ret = -EIO;
	}

	return ret;
}

static s32 tool_i2c_write_no_extra(u8 *buf, u16 len)
{
	s32 ret = -1;
	u8 i = 0;
	struct i2c_msg msg = {
		.flags = !I2C_M_RD,
		.addr  = gt_client->addr,
		.len   = len,
		.buf   = buf,
	};

	for (i = 0; i < cmd_head.retry; i++) {
		ret = i2c_transfer(gt_client->adapter, &msg, 1);
		if (ret > 0)
			break;
	}

	if (i == cmd_head.retry) {
		dev_err(&client->dev, "I2C write retry limit over.\n");
		ret = -EIO;
	}

	return ret;
}

static s32 tool_i2c_read_with_extra(u8 *buf, u16 len)
{
	s32 ret = -1;
	u8 pre[2] = {0x0f, 0xff};
	u8 end[2] = {0x80, 0x00};

	tool_i2c_write_no_extra(pre, 2);
	ret = tool_i2c_read_no_extra(buf, len);
	tool_i2c_write_no_extra(end, 2);

	return ret;
}

static s32 tool_i2c_write_with_extra(u8 *buf, u16 len)
{
	s32 ret = -1;
	u8 pre[2] = {0x0f, 0xff};
	u8 end[2] = {0x80, 0x00};

	tool_i2c_write_no_extra(pre, 2);
	ret = tool_i2c_write_no_extra(buf, len);
	tool_i2c_write_no_extra(end, 2);

	return ret;
}

static void register_i2c_func(void)
{
	/* if (!strncmp(IC_TYPE, "GT818", 5) || !strncmp(IC_TYPE, "GT816", 5)
	|| !strncmp(IC_TYPE, "GT811", 5) || !strncmp(IC_TYPE, "GT818F", 6)
	|| !strncmp(IC_TYPE, "GT827", 5) || !strncmp(IC_TYPE,"GT828", 5)
	|| !strncmp(IC_TYPE, "GT813", 5)) */

	if (strncmp(IC_TYPE, "GT8110", 6) && strncmp(IC_TYPE, "GT8105", 6)
	&& strncmp(IC_TYPE, "GT801", 5) && strncmp(IC_TYPE, "GT800", 5)
	&& strncmp(IC_TYPE, "GT801PLUS", 9) && strncmp(IC_TYPE, "GT811", 5)
	&& strncmp(IC_TYPE, "GTxxx", 5)) {
		tool_i2c_read = tool_i2c_read_with_extra;
		tool_i2c_write = tool_i2c_write_with_extra;
		GTP_DEBUG("I2C function: with pre and end cmd!");
	} else {
		tool_i2c_read = tool_i2c_read_no_extra;
		tool_i2c_write = tool_i2c_write_no_extra;
		GTP_INFO("I2C function: without pre and end cmd!");
	}
}

static void unregister_i2c_func(void)
{
	tool_i2c_read = NULL;
	tool_i2c_write = NULL;
	GTP_INFO("I2C function: unregister i2c transfer function!");
}

s32 init_wr_node(struct i2c_client *client)
{
	u8 i;

	gt_client = client;
	memset(&cmd_head, 0, sizeof(cmd_head));
	cmd_head.data = NULL;

	i = 5;
	while ((!cmd_head.data) && i) {
		cmd_head.data = kzalloc(i * DATA_LENGTH_UINT, GFP_KERNEL);
		if (NULL != cmd_head.data)
			break;
		i--;
	}
	if (i) {
		DATA_LENGTH = i * DATA_LENGTH_UINT;
		dev_dbg(&client->dev, "Applied memory size:%d.", DATA_LENGTH);
	} else {
		GTP_ERROR("Apply for memory failed.");
		return FAIL;
	}

	cmd_head.addr_len = 2;
	cmd_head.retry = 5;

	register_i2c_func();

	mutex_init(&lock);
	tool_set_proc_name(procname);
	goodix_proc_entry = create_proc_entry(procname, 0660, NULL);
	if (goodix_proc_entry == NULL) {
		GTP_ERROR("Couldn't create proc entry!");
		return FAIL;
	} else {
		GTP_INFO("Create proc entry success!");
		goodix_proc_entry->write_proc = goodix_tool_write;
		dix_proc_entry->read_proc = goodix_tool_read;
	}

	return SUCCESS;
}

void uninit_wr_node(void)
{
	kfree(cmd_head.data);
	cmd_head.data = NULL;
	unregister_i2c_func();
	remove_proc_entry(procname, NULL);
}

static u8 relation(u8 src, u8 dst, u8 rlt)
{
	u8 ret = 0;

	switch (rlt) {

	case 0:
		ret = (src != dst) ? true : false;
		break;

	case 1:
		ret = (src == dst) ? true : false;
		GTP_DEBUG("equal:src:0x%02x   dst:0x%02x  ret:%d.",
					src, dst, (s32)ret);
		break;

	case 2:
		ret = (src > dst) ? true : false;
		break;

	case 3:
		ret = (src < dst) ? true : false;
		break;

	case 4:
		ret = (src & dst) ? true : false;
		break;

	case 5:
		ret = (!(src | dst)) ? true : false;
		break;

	default:
		ret = false;
		break;
	}

	return ret;
}

/*******************************************************
Function:
    Comfirm function.
Input:
  None.
Output:
    Return write length.
********************************************************/
static u8 comfirm(void)
{
	s32 i = 0;
	u8 buf[32];

/*    memcpy(&buf[GTP_ADDR_LENGTH - cmd_head.addr_len],
			&cmd_head.flag_addr, cmd_head.addr_len);
    memcpy(buf, &cmd_head.flag_addr, cmd_head.addr_len);
		//Modified by Scott, 2012-02-17 */
	memcpy(buf, cmd_head.flag_addr, cmd_head.addr_len);

	for (i = 0; i < cmd_head.times; i++) {
		if (tool_i2c_read(buf, 1) <= 0) {
			GTP_ERROR("Read flag data failed!");
			return FAIL;
		}
		if (true == relation(buf[GTP_ADDR_LENGTH], cmd_head.flag_val,
						cmd_head.flag_relation)) {
			GTP_DEBUG("value at flag addr:0x%02x.",
						buf[GTP_ADDR_LENGTH]);
			GTP_DEBUG("flag value:0x%02x.", cmd_head.flag_val);
			break;
		}

		msleep(cmd_head.circle);
	}

	if (i >= cmd_head.times) {
		GTP_ERROR("Didn't get the flag to continue!");
		return FAIL;
	}

	return SUCCESS;
}

/********************************************************
Function:
    Goodix tool write function.
nput:
  standard proc write function param.
Output:
    Return write length.
********************************************************/
static s32 goodix_tool_write(struct file *filp, const char __user *buff,
						unsigned long len, void *data)
{
	s32 ret = 0;
	GTP_DEBUG_FUNC();
	GTP_DEBUG_ARRAY((u8 *)buff, len);

	mutex_lock(&lock);
	ret = copy_from_user(&cmd_head, buff, CMD_HEAD_LENGTH);
	if (ret) {
		GTP_ERROR("copy_from_user failed.");
		ret = -EACCES;
		goto exit;
	}

	GTP_DEBUG("wr  :0x%02x.", cmd_head.wr);
	GTP_DEBUG("flag:0x%02x.", cmd_head.flag);
	GTP_DEBUG("flag addr:0x%02x%02x.", cmd_head.flag_addr[0],
						cmd_head.flag_addr[1]);
	GTP_DEBUG("flag val:0x%02x.", cmd_head.flag_val);
	GTP_DEBUG("flag rel:0x%02x.", cmd_head.flag_relation);
	GTP_DEBUG("circle  :%d.", (s32)cmd_head.circle);
	GTP_DEBUG("times   :%d.", (s32)cmd_head.times);
	GTP_DEBUG("retry   :%d.", (s32)cmd_head.retry);
	GTP_DEBUG("delay   :%d.", (s32)cmd_head.delay);
	GTP_DEBUG("data len:%d.", (s32)cmd_head.data_len);
	GTP_DEBUG("addr len:%d.", (s32)cmd_head.addr_len);
	GTP_DEBUG("addr:0x%02x%02x.", cmd_head.addr[0], cmd_head.addr[1]);
	GTP_DEBUG("len:%d.", (s32)len);
	GTP_DEBUG("buf[20]:0x%02x.", buff[CMD_HEAD_LENGTH]);

	if (cmd_head.data_len > (DATA_LENGTH - GTP_ADDR_LENGTH)) {
		pr_err("data len %d > data buff %d, rejected!\n",
			cmd_head.data_len, (DATA_LENGTH - GTP_ADDR_LENGTH));
		ret = -EINVAL;
		goto exit;
	}
	if (cmd_head.addr_len > GTP_ADDR_LENGTH) {
		pr_err(" addr len %d > data buff %d, rejected!\n",
			cmd_head.addr_len, GTP_ADDR_LENGTH);
		ret = -EINVAL;
		goto exit;
	}

	if (cmd_head.wr == 1) {
		/*  copy_from_user(&cmd_head.data[cmd_head.addr_len],
				&buff[CMD_HEAD_LENGTH], cmd_head.data_len); */
		ret = copy_from_user(&cmd_head.data[GTP_ADDR_LENGTH],
				&buff[CMD_HEAD_LENGTH], cmd_head.data_len);
		if (ret)
			GTP_ERROR("copy_from_user failed.");

		memcpy(&cmd_head.data[GTP_ADDR_LENGTH - cmd_head.addr_len],
					cmd_head.addr, cmd_head.addr_len);

		GTP_DEBUG_ARRAY(cmd_head.data,
				cmd_head.data_len + cmd_head.addr_len);
		GTP_DEBUG_ARRAY((u8 *)&buff[CMD_HEAD_LENGTH],
							cmd_head.data_len);

		if (cmd_head.flag == 1) {
			if (FAIL == comfirm()) {
				GTP_ERROR("[WRITE]Comfirm fail!");
				ret = -EINVAL;
				goto exit;
			}
		} else if (cmd_head.flag == 2) {
			/* Need interrupt! */
		}
		if (tool_i2c_write(
		&cmd_head.data[GTP_ADDR_LENGTH - cmd_head.addr_len],
		cmd_head.data_len + cmd_head.addr_len) <= 0) {
			GTP_ERROR("[WRITE]Write data failed!");
			ret = -EIO;
			goto exit;
		}

		GTP_DEBUG_ARRAY(
			&cmd_head.data[GTP_ADDR_LENGTH - cmd_head.addr_len],
			cmd_head.data_len + cmd_head.addr_len);
		if (cmd_head.delay)
			msleep(cmd_head.delay);

		ret = cmd_head.data_len + CMD_HEAD_LENGTH;
		goto exit;
	} else if (cmd_head.wr == 3) {  /* Write ic type */

		ret = copy_from_user(&cmd_head.data[0], &buff[CMD_HEAD_LENGTH],
				cmd_head.data_len);
		if (ret)
			GTP_ERROR("copy_from_user failed.");

		if (cmd_head.data_len > sizeof(IC_TYPE)) {
			pr_err("<<-GTP->> data len %d > data buff %d, rejected!\n",
			cmd_head.data_len, sizeof(IC_TYPE));
			ret = -EINVAL;
			goto exit;
		}
		memcpy(IC_TYPE, cmd_head.data, cmd_head.data_len);

		register_i2c_func();

		ret = cmd_head.data_len + CMD_HEAD_LENGTH;
		goto exit;
	} else if (cmd_head.wr == 5) {

		/* memcpy(IC_TYPE, cmd_head.data, cmd_head.data_len); */

		ret = cmd_head.data_len + CMD_HEAD_LENGTH;
		goto exit;
	} else if (cmd_head.wr == 7) { /* disable irq! */
		gtp_irq_disable(i2c_get_clientdata(gt_client));

		#if GTP_ESD_PROTECT
		gtp_esd_switch(gt_client, SWITCH_OFF);
		#endif
		ret = CMD_HEAD_LENGTH;
		goto exit;
	} else if (cmd_head.wr == 9) { /* enable irq! */
		gtp_irq_enable(i2c_get_clientdata(gt_client));

		#if GTP_ESD_PROTECT
		gtp_esd_switch(gt_client, SWITCH_ON);
		#endif
		ret = CMD_HEAD_LENGTH;
		goto exit;
	} else if (cmd_head.wr == 17) {
		struct goodix_ts_data *ts = i2c_get_clientdata(gt_client);
		ret = copy_from_user(&cmd_head.data[GTP_ADDR_LENGTH],
				&buff[CMD_HEAD_LENGTH], cmd_head.data_len);
		if (ret)
			GTP_DEBUG("copy_from_user failed.");
		if (cmd_head.data[GTP_ADDR_LENGTH]) {
			GTP_DEBUG("gtp enter rawdiff.");
			ts->gtp_rawdiff_mode = true;
		} else {
			ts->gtp_rawdiff_mode = false;
			GTP_DEBUG("gtp leave rawdiff.");
		}
		ret = CMD_HEAD_LENGTH;
		goto exit;
	}
#ifdef UPDATE_FUNCTIONS
	else if (cmd_head.wr == 11) { /* Enter update mode! */
		if (FAIL == gup_enter_update_mode(gt_client)) {
			ret = -EBUSY;
			goto exit;
		}
	} else if (cmd_head.wr == 13) { /* Leave update mode! */
		gup_leave_update_mode();
	} else if (cmd_head.wr == 15) { /* Update firmware! */
		show_len = 0;
		total_len = 0;
		if (cmd_head.data_len + 1 > DATA_LENGTH) {
			pr_err("<<-GTP->> data len %d > data buff %d, rejected!\n",
			cmd_head.data_len + 1, DATA_LENGTH);
			ret = -EINVAL;
			goto exit;
		}
		memset(cmd_head.data, 0, cmd_head.data_len + 1);
		memcpy(cmd_head.data, &buff[CMD_HEAD_LENGTH],
					cmd_head.data_len);

		if (FAIL == gup_update_proc((void *)cmd_head.data)) {
			ret = -EBUSY;
			goto exit;
		}
	}
#endif
	ret = CMD_HEAD_LENGTH;

exit:
	mutex_unlock(&lock);
	return ret;
}

/*******************************************************
Function:
    Goodix tool read function.
Input:
  standard proc read function param.
Output:
    Return read length.
********************************************************/
static s32 goodix_tool_read(char *page, char **start, off_t off, int count,
							int *eof, void *data)
{
	s32 ret;
	GTP_DEBUG_FUNC();

	mutex_lock(&lock);
	if (cmd_head.wr % 2) {
		pr_err("<< [READ]command head wrong\n");
		ret = -EINVAL;
		goto exit;
	} else if (!cmd_head.wr) {
		u16 len = 0;
		s16 data_len = 0;
		u16 loc = 0;

		if (cmd_head.flag == 1) {
			if (FAIL == comfirm()) {
				GTP_ERROR("[READ]Comfirm fail!");
				ret = -EINVAL;
				goto exit;
			}
		} else if (cmd_head.flag == 2) {
			/* Need interrupt! */
		}

		memcpy(cmd_head.data, cmd_head.addr, cmd_head.addr_len);

		GTP_DEBUG("[CMD HEAD DATA] ADDR:0x%02x%02x.", cmd_head.data[0],
							cmd_head.data[1]);
		GTP_DEBUG("[CMD HEAD ADDR] ADDR:0x%02x%02x.", cmd_head.addr[0],
							cmd_head.addr[1]);

		if (cmd_head.delay)
			msleep(cmd_head.delay);

		data_len = cmd_head.data_len;
		while (data_len > 0) {
			if (data_len > DATA_LENGTH)
				len = DATA_LENGTH;
			else
				len = data_len;

			data_len -= len;

			if (tool_i2c_read(cmd_head.data, len) <= 0) {
				GTP_ERROR("[READ]Read data failed!");
				ret = -EINVAL;
				goto exit;
			}
			memcpy(&page[loc], &cmd_head.data[GTP_ADDR_LENGTH],
									len);
			loc += len;

			GTP_DEBUG_ARRAY(&cmd_head.data[GTP_ADDR_LENGTH], len);
			GTP_DEBUG_ARRAY(page, len);
		}
	} else if (cmd_head.wr == 2) {
		/* memcpy(page, "gt8", cmd_head.data_len);
		memcpy(page, "GT818", 5);
		page[5] = 0; */

		GTP_DEBUG("Return ic type:%s len:%d.", page,
						(s32)cmd_head.data_len);
		ret = cmd_head.data_len;
		goto exit;
		/* return sizeof(IC_TYPE_NAME); */
	} else if (cmd_head.wr == 4) {
		page[0] = show_len >> 8;
		page[1] = show_len & 0xff;
		page[2] = total_len >> 8;
		page[3] = total_len & 0xff;
	} else if (6 == cmd_head.wr) {
		/* Read error code! */
	} else if (8 == cmd_head.wr) { /*Read driver version */
		/* memcpy(page, GTP_DRIVER_VERSION,
				strlen(GTP_DRIVER_VERSION)); */
		s32 tmp_len;
		tmp_len = strlen(GTP_DRIVER_VERSION);
		memcpy(page, GTP_DRIVER_VERSION, tmp_len);
		page[tmp_len] = 0;
	}
	ret = cmd_head.data_len;

exit:
	mutex_unlock(&lock);
	return ret;
}
