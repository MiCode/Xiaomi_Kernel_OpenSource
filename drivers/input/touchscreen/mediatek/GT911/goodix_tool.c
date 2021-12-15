/* SPDX-License-Identifier: GPL-2.0 */	
/*	
 * Copyright (c) 2019 MediaTek Inc.	
*/

#include "tpd.h"
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "include/tpd_gt9xx_common.h"

#include <linux/device.h>
#include <linux/proc_fs.h> /*proc*/

#pragma pack(1)
struct st_cmd_head {
	u8  wr;         /* write read flag - 0:R  1:W  2:PID 3: */
	u8 flag;	  /* 0:no need flag/int 1: need flag  2:need int */
	u8 flag_addr[2];  /* flag address */
	u8 flag_val;      /* flag val */
	u8 flag_relation; /* flag_val:flag 0:not equal 1:equal 2:> 3:< */
	u16 circle;       /* polling cycle */
	u8 times;	 /* plling times */
	u8 retry;	 /* I2C retry times */
	u16 delay;	/* delay before read or after write */
	u16 data_len;     /* data length */
	u8 addr_len;      /* address length */
	u8 addr[2];       /* address */
	u8 res[3];	/* reserved */
	u8 *data;	 /* data pointer */
};
#pragma pack()
struct st_cmd_head cmd_head;

#define DATA_LENGTH_UINT 512
#define CMD_HEAD_LENGTH (sizeof(struct st_cmd_head) - sizeof(u8 *))
#define GOODIX_ENTRY_NAME "goodix_tool"
static char procname[20] = {0};
static struct i2c_client *gt_client;

#if 0
static struct proc_dir_entry *goodix_proc_entry;
#endif

static s32 goodix_tool_write(struct file *filp, const char __user *buff,
			     unsigned long len, void *data);
static s32 goodix_tool_read(char *page, char **start, off_t off, int count,
			    int *eof, void *data);
static s32 (*tool_i2c_read)(u8 *, u16);
static s32 (*tool_i2c_write)(u8 *, u16);

s32 DATA_LENGTH;
s8 IC_TYPE[16] = "GT9XX";

static void tool_set_proc_name(char *procname)
{
	sprintf(procname, "gmnode");
}

static ssize_t goodix_tool_upper_read(struct file *file, char __user *buffer,
				      size_t count, loff_t *ppos)
{
	return goodix_tool_read(buffer, NULL, 0, count, NULL, ppos);
}

static ssize_t goodix_tool_upper_write(struct file *file,
				       const char __user *buffer, size_t count,
				       loff_t *ppos)
{
	return goodix_tool_write(file, buffer, count, ppos);
}

static const struct file_operations gt_tool_fops = {
	.write = goodix_tool_upper_write, .read = goodix_tool_upper_read};

static s32 tool_i2c_read_no_extra(u8 *buf, u16 len)
{
	s32 ret = -1;

	ret = gtp_i2c_read(gt_client, buf, len + GTP_ADDR_LENGTH);
	return ret;
}

static s32 tool_i2c_write_no_extra(u8 *buf, u16 len)
{
	s32 ret = -1;

	ret = gtp_i2c_write(gt_client, buf, len);
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
	/* if (!strncmp(IC_TYPE, "GT818", 5) || !strncmp(IC_TYPE, "GT816", 5) */
	/* || !strncmp(IC_TYPE, "GT811", 5) || !strncmp(IC_TYPE, "GT818F", 6) */
	/* || !strncmp(IC_TYPE, "GT827", 5) || !strncmp(IC_TYPE,"GT828", 5) */
	/* || !strncmp(IC_TYPE, "GT813", 5)) */
	if (strncmp(IC_TYPE, "GT8110", 6) && strncmp(IC_TYPE, "GT8105", 6) &&
	    strncmp(IC_TYPE, "GT801", 5) && strncmp(IC_TYPE, "GT800", 5) &&
	    strncmp(IC_TYPE, "GT801PLUS", 9) && strncmp(IC_TYPE, "GT811", 5) &&
	    strncmp(IC_TYPE, "GTxxx", 5) && strncmp(IC_TYPE, "GT9XX", 5)) {
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
	s32 i;

	gt_client = i2c_client_point;

	memset(&cmd_head, 0, sizeof(cmd_head));
	cmd_head.data = NULL;

	i = 5;

	while ((!cmd_head.data) && i) {
		cmd_head.data = kzalloc(i * DATA_LENGTH_UINT, GFP_KERNEL);

		if (cmd_head.data != NULL)
			break;

		i--;
	}

	if (i) {
		DATA_LENGTH = i * DATA_LENGTH_UINT + GTP_ADDR_LENGTH;
		GTP_INFO("Applied memory size:%d.", DATA_LENGTH);
	} else {
		GTP_ERROR("Apply for memory failed.");
		return FAIL;
	}

	cmd_head.addr_len = 2;
	cmd_head.retry = 5;

	register_i2c_func();

	tool_set_proc_name(procname);
#if 0 /* fix 3.10 */
	goodix_proc_entry = create_proc_entry(gtp_tool_entry, 0664, NULL);

	if (goodix_proc_entry == NULL) {
		GTP_ERROR("Couldn't create proc entry!");
		return FAIL;
	}
	GTP_INFO("Create proc entry success!");
	goodix_proc_entry->write_proc = goodix_tool_write;
	goodix_proc_entry->read_proc = goodix_tool_read;

#else
	if (proc_create(procname, 0660, NULL, &gt_tool_fops) == NULL) {
		GTP_ERROR("create_proc_entry %s failed", procname);
		return -1;
	}
#endif
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
		GTP_DEBUG("equal:src:0x%02x   dst:0x%02x   ret:%d.", src, dst,
			  (s32)ret);
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

/*
 *******************************************************
 *Function:
 *	Comfirm function.
 *Input:
 *  None.
 *Output:
 *	Return write length.
 *********************************************************
 */
static u8 comfirm(void)
{
	s32 i = 0;
	u8 buf[32];

	/* memcpy(&buf[GTP_ADDR_LENGTH - cmd_head.addr_len], */
	/* &cmd_head.flag_addr, cmd_head.addr_len); */
	/* memcpy(buf, &cmd_head.flag_addr, cmd_head.addr_len);*/
	/*Modified by  Scott, 2012-02-17 */
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

/*
 * ******************************************************
 * Function:
 *	Goodix tool write function.
 * Input:
 *  standard proc write function param.
 * Output:
 *	Return write length.
 * *******************************************************
 */
static s32 goodix_tool_write(struct file *filp, const char __user *buff,
			     unsigned long len, void *data)
{
	return 0;
}

/*
 *******************************************************
 *Function:
 *	Goodix tool read function.
 *Input:
 *  standard proc read function param.
 *Output:
 *	Return read length.
 ********************************************************
 */
static s32 goodix_tool_read(char *page, char **start, off_t off, int count,
			    int *eof, void *data)
{
	GTP_DEBUG_FUNC();

	if (cmd_head.wr % 2) {
		return FAIL;
	} else if (!cmd_head.wr) {
		u16 len = 0;
		s16 data_len = 0;
		u16 loc = 0;

		if (cmd_head.flag == 1) {
			if (comfirm() == FAIL) {
				GTP_ERROR("[READ]Comfirm fail!");
				return FAIL;
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

			data_len -= DATA_LENGTH;

			if (tool_i2c_read(cmd_head.data, len) <= 0) {
				GTP_ERROR("[READ]Read data failed!");
				return FAIL;
			}

			memcpy(&page[loc], &cmd_head.data[GTP_ADDR_LENGTH],
			       len);
			loc += len;

			GTP_DEBUG_ARRAY(&cmd_head.data[GTP_ADDR_LENGTH], len);
			GTP_DEBUG_ARRAY(page, len);
		}
	} else if (cmd_head.wr == 2) {
		/* memcpy(page, "gt8", cmd_head.data_len); */
		/* memcpy(page, "GT818", 5); */
		/* page[5] = 0; */

		GTP_DEBUG("Return ic type:%s len:%d.", page,
			  (s32)cmd_head.data_len);
		return cmd_head.data_len;
		/* return sizeof(IC_TYPE_NAME); */
	} else if (cmd_head.wr == 4) {
		page[0] = show_len >> 8;
		page[1] = show_len & 0xff;
		page[2] = total_len >> 8;
		page[3] = total_len & 0xff;

		return cmd_head.data_len;
	} else if (cmd_head.wr == 6) {
		/* Read error code! */
	} else if (cmd_head.wr == 8) { /* Read driver version */
		/* memcpy(page, GTP_DRIVER_VERSION, strlen(GTP_DRIVER_VERSION));
		 */
		s32 tmp_len;

		tmp_len = strlen(GTP_DRIVER_VERSION);
		memcpy(page, GTP_DRIVER_VERSION, tmp_len);
		page[tmp_len] = 0;
	}

	return cmd_head.data_len;
}
