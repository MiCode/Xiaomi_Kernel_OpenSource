/* drivers/input/touchscreen/goodix_tool.c
 *
 * 2010 - 2012 Goodix Technology.
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
 * Version:1.2
 *        V1.0:2012/05/01,create file.
 *        V1.2:2012/10/17,reset_guitar etc.
 *        V1.4: 2013/06/08, new proc name
 */

#include "tpd.h"
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>	/*proc */
#include <generated/utsrelease.h>

#include "include/tpd_custom_gt9xx.h"
#pragma pack(1)
typedef struct {
	u8 wr;			/*write read flag£¬0:R  1:W  2:PID 3:*/
	u8 flag;		/*0:no need flag/int 1: need flag  2:need int*/
	u8 flag_addr[2];	/*flag address */
	u8 flag_val;		/*flag val*/
	u8 flag_relation;	/*flag_val:flag 0:not equal 1:equal 2:> 3:<*/
	u16 circle;		/*polling cycle*/
	u8 times;		/*plling times*/
	u8 retry;		/*I2C retry times*/
	u16 delay;		/*delay before read or after write*/
	u16 data_len;		/*data length*/
	u8 addr_len;		/*address length*/
	u8 addr[2];		/*address*/
	u8 res[3];		/*reserved*/
	u8 *data;		/*data pointer*/
} st_cmd_head;
#pragma pack()
st_cmd_head cmd_head;

#define DATA_LENGTH_UINT    512
#define CMD_HEAD_LENGTH     (sizeof(st_cmd_head) - sizeof(u8 *))
static char procname[20] = { 0 };
static struct i2c_client *gt_client;
static s32 goodix_tool_write(struct file *filp, const char __user *buff, unsigned long len, void *data);
static s32 goodix_tool_read(char *page, char **start, off_t off, int count, int *eof, void *data);
static s32 (*tool_i2c_read)(u8*, u16);
static s32 (*tool_i2c_write)(u8*, u16);


s32 DATA_LENGTH = 0;
s8 IC_TYPE[16] = "GT9XX";

#ifdef CONFIG_HOTKNOT_BLOCK_RW
DECLARE_WAIT_QUEUE_HEAD(bp_waiter);
u8 got_hotknot_state = 0;
u8 got_hotknot_extra_state = 0;
u8 wait_hotknot_state = 0;
u8 force_wake_flag = 0;
#endif

#define HOTKNOTNAME  "hotknot"

static ssize_t hotknot_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
	return goodix_tool_read(buffer, NULL, 0, count, NULL, ppos);
}

static ssize_t hotknot_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	return goodix_tool_write(file, buffer, count, ppos);
}

static const struct file_operations hotknot_fops = {
/*  .owner = THIS_MODULE,*/
	.read = hotknot_read,
	.write = hotknot_write,
};

static struct miscdevice hotknot_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = HOTKNOTNAME,
	.fops = &hotknot_fops,
};

static ssize_t goodix_tool_upper_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
	return goodix_tool_read(buffer, NULL, 0, count, NULL, ppos);
}

static ssize_t goodix_tool_upper_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	return goodix_tool_write(file, buffer, count, ppos);
}

static const struct file_operations gt_tool_fops = {
	.write = goodix_tool_upper_write,
	.read = goodix_tool_upper_read,
};

static void tool_set_proc_name(char *procname)
{
	int v0 = 0, v1 = 0, v2 = 0;
	int ret;

	ret = sscanf(UTS_RELEASE, "%d.%d.%d", &v0, &v1, &v2);
	sprintf(procname, "gmnode%02d%02d%02d", v0, v1, v2);
}

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
	u8 pre[2] = { 0x0f, 0xff };
	u8 end[2] = { 0x80, 0x00 };

	tool_i2c_write_no_extra(pre, 2);
	ret = tool_i2c_read_no_extra(buf, len);
	tool_i2c_write_no_extra(end, 2);

	return ret;
}

static s32 tool_i2c_write_with_extra(u8 *buf, u16 len)
{
	s32 ret = -1;
	u8 pre[2] = { 0x0f, 0xff };
	u8 end[2] = { 0x80, 0x00 };

	tool_i2c_write_no_extra(pre, 2);
	ret = tool_i2c_write_no_extra(buf, len);
	tool_i2c_write_no_extra(end, 2);

	return ret;
}

static void register_i2c_func(void)
{
	if (strncmp(IC_TYPE, "GT8110", 6) && strncmp(IC_TYPE, "GT8105", 6)
	    && strncmp(IC_TYPE, "GT801", 5) && strncmp(IC_TYPE, "GT800", 5)
	    && strncmp(IC_TYPE, "GT801PLUS", 9) && strncmp(IC_TYPE, "GT811", 5)
	    && strncmp(IC_TYPE, "GTxxx", 5) && strncmp(IC_TYPE, "GT9XX", 5)) {
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

		if (NULL != cmd_head.data)
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
	if (proc_create(procname, 0660, NULL, &gt_tool_fops) == NULL) {
		GTP_ERROR("create_proc_entry %s failed", procname);
		return -1;
	}

#if 1/*setting by hotknot feature*/
	if (misc_register(&hotknot_misc_device)) {
		GTP_ERROR("mtk_tpd: hotknot_device register failed\n");
		return FAIL;
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
		GTP_DEBUG("equal:src:0x%02x   dst:0x%02x   ret:%d.", src, dst, (s32) ret);
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

	memcpy(buf, cmd_head.flag_addr, cmd_head.addr_len);

	for (i = 0; i < cmd_head.times; i++) {
		if (tool_i2c_read(buf, 1) <= 0) {
			GTP_ERROR("Read flag data failed!");
			return FAIL;
		}

		if (true == relation(buf[GTP_ADDR_LENGTH], cmd_head.flag_val, cmd_head.flag_relation)) {
			GTP_DEBUG("value at flag addr:0x%02x.", buf[GTP_ADDR_LENGTH]);
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

/*******************************************************
Function:
    Goodix tool write function.
Input:
  standard proc write function param.
Output:
    Return write length.
********************************************************/
static s32 goodix_tool_write(struct file *filp, const char __user *buff, unsigned long len, void *data)
{
	u64 ret = 0;

	GTP_DEBUG_FUNC();
	GTP_DEBUG_ARRAY((u8 *) buff, len);

	if (tpd_halt == 1 || is_resetting == 1) {
		/*GTP_ERROR("[Write]tpd_halt =1 fail!");*/
		return FAIL;
	}

	ret = copy_from_user(&cmd_head, buff, CMD_HEAD_LENGTH);

	if (ret)
		GTP_ERROR("copy_from_user failed.");

	GTP_DEBUG("wr  :0x%02x.", cmd_head.wr);
	/*
	   GTP_DEBUG("flag:0x%02x.", cmd_head.flag);
	   GTP_DEBUG("flag addr:0x%02x%02x.", cmd_head.flag_addr[0], cmd_head.flag_addr[1]);
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
	 */

	if (1 == cmd_head.wr) {
		ret = copy_from_user(&cmd_head.data[GTP_ADDR_LENGTH], &buff[CMD_HEAD_LENGTH], cmd_head.data_len);

		if (ret)
			GTP_ERROR("copy_from_user failed.");

		memcpy(&cmd_head.data[GTP_ADDR_LENGTH - cmd_head.addr_len], cmd_head.addr, cmd_head.addr_len);

		GTP_DEBUG_ARRAY(cmd_head.data, cmd_head.data_len + cmd_head.addr_len);
		GTP_DEBUG_ARRAY((u8 *) &buff[CMD_HEAD_LENGTH], cmd_head.data_len);

		if (1 == cmd_head.flag) {
			if (FAIL == comfirm()) {
				GTP_ERROR("[WRITE]Comfirm fail!");
				return FAIL;
			}
		} else if (2 == cmd_head.flag) {
			/*Need interrupt!*/
		}

		if (tool_i2c_write(&cmd_head.data[GTP_ADDR_LENGTH - cmd_head.addr_len],
				   cmd_head.data_len + cmd_head.addr_len) <= 0) {
			GTP_ERROR("[WRITE]Write data failed!");
			return FAIL;
		}

		GTP_DEBUG_ARRAY(&cmd_head.data[GTP_ADDR_LENGTH - cmd_head.addr_len],
				cmd_head.data_len + cmd_head.addr_len);

		if (cmd_head.delay)
			msleep(cmd_head.delay);

		return cmd_head.data_len + CMD_HEAD_LENGTH;
	} else if (3 == cmd_head.wr) {/*Write ic type*/
		memcpy(IC_TYPE, cmd_head.data, cmd_head.data_len);
		register_i2c_func();

		return cmd_head.data_len + CMD_HEAD_LENGTH;
	} else if (5 == cmd_head.wr) {
		/*memcpy(IC_TYPE, cmd_head.data, cmd_head.data_len);*/
		return cmd_head.data_len + CMD_HEAD_LENGTH;
	} else if (7 == cmd_head.wr) {	/*disable irq!*/
		disable_irq(touch_irq);
#if GTP_ESD_PROTECT
		gtp_esd_switch(i2c_client_point, SWITCH_OFF);
#endif
		return CMD_HEAD_LENGTH;
	} else if (9 == cmd_head.wr) {/*enable irq!*/
		enable_irq(touch_irq);
#if GTP_ESD_PROTECT
		gtp_esd_switch(i2c_client_point, SWITCH_ON);
#endif
		return CMD_HEAD_LENGTH;
	} else if (17 == cmd_head.wr) {
		ret = copy_from_user(&cmd_head.data[GTP_ADDR_LENGTH], &buff[CMD_HEAD_LENGTH], cmd_head.data_len);

		if (ret)
			GTP_DEBUG("copy_from_user failed.");

		if (cmd_head.data[GTP_ADDR_LENGTH]) {
			GTP_DEBUG("gtp enter rawdiff.");
			gtp_rawdiff_mode = true;
		} else {
			gtp_rawdiff_mode = false;
			GTP_DEBUG("gtp leave rawdiff.");
		}

		return CMD_HEAD_LENGTH;
	}
#ifdef UPDATE_FUNCTIONS
	else if (11 == cmd_head.wr)	{/*Enter update mode!*/
		if (FAIL == gup_enter_update_mode(gt_client))
			return FAIL;
	} else if (13 == cmd_head.wr) {	/*Leave update mode!*/
		gup_leave_update_mode();
	} else if (15 == cmd_head.wr) {	/*Update firmware!*/
		show_len = 0;
		total_len = 0;
		memset(cmd_head.data, 0, cmd_head.data_len + 1);
		ret = copy_from_user(cmd_head.data, &buff[CMD_HEAD_LENGTH], cmd_head.data_len);
		if (ret)
			GTP_ERROR("copy_from_user failed.");
		GTP_DEBUG("update firmware, filename: %s", cmd_head.data);
		if (FAIL == gup_update_proc((void *)cmd_head.data))
			return FAIL;
	}
#endif
	else if (19 == cmd_head.wr)	 {/*load subsystem*/
		ret = copy_from_user(&cmd_head.data[0], &buff[CMD_HEAD_LENGTH], cmd_head.data_len);
		if (0 == cmd_head.data[0]) {
			if (FAIL == gup_load_hotknot_system())
				return FAIL;
		} else if (1 == cmd_head.data[0]) {
			if (FAIL == gup_load_fx_system())
				return FAIL;
		} else if (2 == cmd_head.data[0]) {
			if (FAIL == gup_recovery_main_system())
				return FAIL;
		} else if (3 == cmd_head.data[0]) {
			if (FAIL == gup_load_main_system(NULL))
				return FAIL;
		}
	}
#ifdef CONFIG_HOTKNOT_BLOCK_RW
	else if (21 == cmd_head.wr) {
		u16 wait_hotknot_timeout = 0;
		u8 rqst_hotknot_state;

		ret = copy_from_user(&cmd_head.data[GTP_ADDR_LENGTH], &buff[CMD_HEAD_LENGTH], cmd_head.data_len);

		if (ret)
			GTP_ERROR("copy_from_user failed.");

		rqst_hotknot_state = cmd_head.data[GTP_ADDR_LENGTH];
		wait_hotknot_state |= rqst_hotknot_state;
		wait_hotknot_timeout = (cmd_head.data[GTP_ADDR_LENGTH + 1] << 8) + cmd_head.data[GTP_ADDR_LENGTH + 2];
		GTP_DEBUG("Goodix tool received wait polling state:0x%x,timeout:%d, all wait state:0x%x",
			  rqst_hotknot_state, wait_hotknot_timeout, wait_hotknot_state);
		got_hotknot_state &= (~rqst_hotknot_state);
		switch (rqst_hotknot_state) {
			set_current_state(TASK_INTERRUPTIBLE);
		case HN_DEVICE_PAIRED:
			hotknot_paired_flag = 0;
			wait_event_interruptible(bp_waiter, force_wake_flag ||
						 rqst_hotknot_state == (got_hotknot_state & rqst_hotknot_state));
			wait_hotknot_state &= (~rqst_hotknot_state);
			if (rqst_hotknot_state != (got_hotknot_state & rqst_hotknot_state)) {
				GTP_ERROR("Wait 0x%x block polling waiter failed.", rqst_hotknot_state);
				force_wake_flag = 0;
				return FAIL;
			}
			break;
		case HN_MASTER_SEND:
		case HN_SLAVE_RECEIVED:
			wait_event_interruptible_timeout(bp_waiter, force_wake_flag ||
							 rqst_hotknot_state == (got_hotknot_state & rqst_hotknot_state),
							 wait_hotknot_timeout);
			wait_hotknot_state &= (~rqst_hotknot_state);
			if (rqst_hotknot_state == (got_hotknot_state & rqst_hotknot_state)) {
				return got_hotknot_extra_state;
			GTP_ERROR("Wait 0x%x block polling waiter timeout.", rqst_hotknot_state);
			force_wake_flag = 0;
			return FAIL;
		case HN_MASTER_DEPARTED:
		case HN_SLAVE_DEPARTED:
			wait_event_interruptible_timeout(bp_waiter, force_wake_flag ||
							 rqst_hotknot_state == (got_hotknot_state & rqst_hotknot_state),
							 wait_hotknot_timeout);
			wait_hotknot_state &= (~rqst_hotknot_state);
			if (rqst_hotknot_state != (got_hotknot_state & rqst_hotknot_state)) {
				GTP_ERROR("Wait 0x%x block polling waitor timeout.", rqst_hotknot_state);
				force_wake_flag = 0;
				return FAIL;
			}
			break;
		default:
			GTP_ERROR("Invalid rqst_hotknot_state in goodix_tool.");
			break;
		}
		force_wake_flag = 0;
	} else if (23 == cmd_head.wr) {
		GTP_DEBUG("Manual wakeup all block polling waiter!");
		got_hotknot_state = 0;
		wait_hotknot_state = 0;
		force_wake_flag = 1;
		hotknot_paired_flag = 0;
		wake_up_interruptible(&bp_waiter);
	}
#endif
	return CMD_HEAD_LENGTH;
}

/*******************************************************
Function:
    Goodix tool read function.
Input:
  standard proc read function param.
Output:
    Return read length.
********************************************************/
static s32 goodix_tool_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	u64 ret = 0;

	GTP_DEBUG_FUNC();

	if (tpd_halt == 1 || is_resetting == 1) {
		/*GTP_ERROR("[READ]tpd_halt =1 fail!");*/
		return FAIL;
	}
	if (cmd_head.wr % 2) {
		GTP_ERROR("[READ] invaild operator fail!");
		return FAIL;
	} else if (!cmd_head.wr) {
		u16 len = 0;
		s16 data_len = 0;
		u16 loc = 0;

		if (1 == cmd_head.flag) {
			if (FAIL == comfirm()) {
				GTP_ERROR("[READ]Comfirm fail!");
				return FAIL;
			}
		} else if (2 == cmd_head.flag) {
			/*Need interrupt!*/
		}

		memcpy(cmd_head.data, cmd_head.addr, cmd_head.addr_len);

		GTP_DEBUG("[CMD HEAD DATA] ADDR:0x%02x%02x.", cmd_head.data[0], cmd_head.data[1]);
		GTP_DEBUG("[CMD HEAD ADDR] ADDR:0x%02x%02x.", cmd_head.addr[0], cmd_head.addr[1]);

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

			ret = copy_to_user(&page[loc], &cmd_head.data[GTP_ADDR_LENGTH], len);
			if (ret) {
				GTP_ERROR("copy_from_user failed.");
				return FAIL;
			}
			loc += len;

			GTP_DEBUG_ARRAY(&cmd_head.data[GTP_ADDR_LENGTH], len);
			GTP_DEBUG_ARRAY(page, len);
		}
	} else if (2 == cmd_head.wr) {
		GTP_DEBUG("Return ic type:%s len:%d.", page, (s32) cmd_head.data_len);
		return cmd_head.data_len;
	} else if (4 == cmd_head.wr) {
		page[0] = show_len >> 8;
		page[1] = show_len & 0xff;
		page[2] = total_len >> 8;
		page[3] = total_len & 0xff;

		return cmd_head.data_len;
	} else if (6 == cmd_head.wr) {
		/*read error*/
	} else if (8 == cmd_head.wr) {	/*Read driver version*/
		s32 tmp_len;

		tmp_len = strlen(GTP_DRIVER_VERSION);
		ret = copy_to_user(page, GTP_DRIVER_VERSION, tmp_len);
		if (ret) {
			GTP_ERROR("copy_from_user failed.");
			return FAIL;
		}
		page[tmp_len] = 0;
	}

	return cmd_head.data_len;
}
