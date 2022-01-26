/* drivers/input/touchscreen/mediatek/gt9xx_mtk/goodix_tool.c
 *
 * Copyright  (C)  2010 - 2016 Goodix., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the GNU
 * General Public License for more details.
 *
 * Version: V2.6.0.3
 */

#include "include/tpd_gt9xx_common.h"
#include "tpd.h"
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h> /*proc */
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#pragma pack(1)
struct st_cmd_head {
	u8 wr;		  /* write read flag£¬0:R1:W2:PID 3: */
	u8 flag;	  /* 0:no need flag/int 1: need flag2:need int */
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
static struct st_cmd_head cmd_head, cmd_head2;

#define DATA_LENGTH_UINT 512
#define CMD_HEAD_LENGTH (sizeof(struct st_cmd_head) - sizeof(u8 *))
static char procname[20] = {0};

static struct i2c_client *gt_client;

static struct proc_dir_entry *goodix_proc_entry;

static ssize_t goodix_tool_write(struct file *, const char __user *, size_t,
				 loff_t *);
static ssize_t goodix_tool_read(struct file *, char __user *, size_t, loff_t *);
static s32 (*tool_i2c_read)(u8 *, u16);
static s32 (*tool_i2c_write)(u8 *, u16);

static const struct file_operations tool_ops = {
	.owner = THIS_MODULE,
	.read = goodix_tool_read,
	.write = goodix_tool_write,
};

static int hotknot_open(struct inode *node, struct file *flip);
static int hotknot_release(struct inode *node, struct file *flip);
static ssize_t hotknot_write(struct file *, const char __user *, size_t,
			     loff_t *);
static ssize_t hotknot_read(struct file *, char __user *, size_t, loff_t *);

static s32 DATA_LENGTH;
static s8 IC_TYPE[16] = "GT9XX";

#ifdef CONFIG_HOTKNOT_BLOCK_RW
DECLARE_WAIT_QUEUE_HEAD(bp_waiter);
u8 got_hotknot_state;
u8 got_hotknot_extra_state;
u8 wait_hotknot_state;
u8 force_wake_flag;
#endif

#define HOTKNOTNAME "hotknot"
u8 gtp_hotknot_enabled;

static const struct file_operations hotknot_fops = {
	/* .owner = THIS_MODULE, */
	.open = hotknot_open,
	.release = hotknot_release,
	.read = hotknot_read,
	.write = hotknot_write,
};

static struct miscdevice hotknot_misc_device = {
	.minor = MISC_DYNAMIC_MINOR, .name = HOTKNOTNAME, .fops = &hotknot_fops,
};

static void tool_set_proc_name(char *procname)
{
	sprintf(procname, "gmnode");
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

	memset(&cmd_head2, 0, sizeof(cmd_head2));
	cmd_head2.data = NULL;

	i = 5;

	while ((!cmd_head2.data) && i) {
		cmd_head2.data = kzalloc(i * DATA_LENGTH_UINT, GFP_KERNEL);

		if (cmd_head2.data != NULL)
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

	cmd_head2.addr_len = 2;
	cmd_head2.retry = 5;

	register_i2c_func();

	tool_set_proc_name(procname);
	goodix_proc_entry = proc_create(procname, 0444, NULL, &tool_ops);

	if (misc_register(&hotknot_misc_device)) {
		GTP_ERROR("mtk_tpd: hotknot_device register failed\n");
		return FAIL;
	}

	if (goodix_proc_entry == NULL) {
		GTP_ERROR("Couldn't create proc entry!");
		return FAIL;
	}
	GTP_INFO("Create proc entry success!");

	return SUCCESS;
}

void uninit_wr_node(void)
{
	kfree(cmd_head.data);
	cmd_head.data = NULL;
	kfree(cmd_head2.data);
	cmd_head2.data = NULL;
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
		GTP_DEBUG("equal:src:0x%02x dst:0x%02x ret:%d.", src, dst,
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

static u8 comfirm(void)
{
	s32 i = 0;
	u8 buf[32];

	/* memcpy(&buf[GTP_ADDR_LENGTH - cmd_head.addr_len], */
	/* &cmd_head.flag_addr, cmd_head.addr_len); */
	/* memcpy(buf, &cmd_head.flag_addr, cmd_head.addr_len); Modified by */
	/* Scott, 2012-02-17 */
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

static ssize_t goodix_tool_write(struct file *filp, const char __user *buff,
				 size_t len, loff_t *off)
{
	s32 ret = 0;

	GTP_DEBUG_FUNC();
	GTP_DEBUG_ARRAY((u8 *)buff, len);

	if (gtp_resetting == 1) {
		/* GTP_ERROR("[Write]tpd_halt =1 fail!"); */
		return FAIL;
	}

	ret = copy_from_user(&cmd_head, buff, CMD_HEAD_LENGTH);

	if (ret)
		GTP_ERROR("copy_from_user failed.");

	GTP_DEBUG("wr:0x%02x.", cmd_head.wr);

	if (cmd_head.wr == 1) {
		if ((cmd_head.data == NULL)
		    || (cmd_head.data_len >= (DATA_LENGTH - GTP_ADDR_LENGTH))
		    || (cmd_head.data_len >= (len - CMD_HEAD_LENGTH))) {
			GTP_ERROR("copy_from_user data out of range.");
			return -EINVAL;
		}

		/* copy_from_user(&cmd_head.data[cmd_head.addr_len], */
		/* &buff[CMD_HEAD_LENGTH], cmd_head.data_len); */
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
			if (comfirm() == FAIL) {
				GTP_ERROR("[WRITE]Comfirm fail!");
				return FAIL;
			}
		} else if (cmd_head.flag == 2) {
			/* Need interrupt! */
		}

		if (tool_i2c_write(
			    &cmd_head.data[GTP_ADDR_LENGTH - cmd_head.addr_len],
			    cmd_head.data_len + cmd_head.addr_len) <= 0) {
			GTP_ERROR("[WRITE]Write data failed!");
			return FAIL;
		}

		GTP_DEBUG_ARRAY(
			&cmd_head.data[GTP_ADDR_LENGTH - cmd_head.addr_len],
			cmd_head.data_len + cmd_head.addr_len);

		if (cmd_head.delay)
			msleep(cmd_head.delay);

		return cmd_head.data_len + CMD_HEAD_LENGTH;
	} else if (cmd_head.wr == 3) { /* Write ic type */
		if (cmd_head.data_len > 16) {
			GTP_ERROR("copy_from_user data out of range.");
			return -EINVAL;
		}
		memcpy(IC_TYPE, cmd_head.data, cmd_head.data_len);
		register_i2c_func();

		return cmd_head.data_len + CMD_HEAD_LENGTH;
	} else if (cmd_head.wr == 5) {
		/* memcpy(IC_TYPE, cmd_head.data, cmd_head.data_len); */

		return cmd_head.data_len + CMD_HEAD_LENGTH;
	} else if (cmd_head.wr == 7) { /* disable irq! */
		gtp_irq_disable();
#ifdef CONFIG_GTP_ESD_PROTECT
		gtp_esd_switch(i2c_client_point, SWITCH_OFF);
#endif
#ifdef CONFIG_GTP_CHARGER_DETECT
		gtp_charger_switch(0);
#endif
		return CMD_HEAD_LENGTH;
	} else if (cmd_head.wr == 9) { /* enable irq! */
		gtp_irq_enable();
#ifdef CONFIG_GTP_ESD_PROTECT
		gtp_esd_switch(i2c_client_point, SWITCH_ON);
#endif
#ifdef CONFIG_GTP_CHARGER_DETECT
		gtp_charger_switch(1);
#endif
		return CMD_HEAD_LENGTH;
	} else if (cmd_head.wr == 17) {
		if ((cmd_head.data == NULL)
		    || (cmd_head.data_len >= (DATA_LENGTH - GTP_ADDR_LENGTH))
		    || (cmd_head.data_len >= (len - CMD_HEAD_LENGTH))) {
			GTP_ERROR("copy_from_user data out of range.");
			return -EINVAL;
		}

		ret = copy_from_user(&cmd_head.data[GTP_ADDR_LENGTH],
				     &buff[CMD_HEAD_LENGTH], cmd_head.data_len);
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
	else if (cmd_head.wr == 11) { /* Enter update mode! */
		if (gup_enter_update_mode(gt_client) == FAIL)
			return FAIL;
	} else if (cmd_head.wr == 13) { /* Leave update mode! */
		gup_leave_update_mode();
	} else if (cmd_head.wr == 15) { /* Update firmware! */
		show_len = 0;
		total_len = 0;
		if ((cmd_head.data == NULL)
			|| (cmd_head.data_len >= DATA_LENGTH)
			|| (cmd_head.data_len >= (len - CMD_HEAD_LENGTH))) {
			GTP_ERROR("copy_from_user data out of range.");
			return -EINVAL;
		}

		memset(cmd_head.data, 0, cmd_head.data_len + 1);
		memcpy(cmd_head.data, &buff[CMD_HEAD_LENGTH],
		       cmd_head.data_len);
		GTP_DEBUG("update firmware, filename: %s", cmd_head.data);
		if (gup_update_proc((void *)cmd_head.data) == FAIL)
			return FAIL;

	}
#endif
#ifdef CONFIG_GTP_HOTKNOT
	else if (cmd_head.wr == 19) { /* load subsystem */
		if ((cmd_head.data == NULL)
			|| (cmd_head.data_len >= DATA_LENGTH)
			|| (cmd_head.data_len >= (len - CMD_HEAD_LENGTH))) {
			GTP_ERROR("copy_from_user data out of range.");
			return -EINVAL;
		}

		ret = copy_from_user(&cmd_head.data[0], &buff[CMD_HEAD_LENGTH],
				     cmd_head.data_len);
		if (cmd_head.data[0] == 0) {
			if (gup_load_hotknot_fw() == FAIL)
				return FAIL;

		} else if (cmd_head.data[0] == 1) {
			if (gup_load_authorization_fw() == FAIL)
				return FAIL;

		} else if (cmd_head.data[0] == 2) {
			if (gup_recovery_touch() == FAIL)
				return FAIL;

		} else if (cmd_head.data[0] == 3) {
			if (gup_load_touch_fw(NULL) == FAIL)
				return FAIL;
		}
	}
#endif
#ifdef CONFIG_HOTKNOT_BLOCK_RW
	else if (cmd_head.wr == 21) {
		u16 wait_hotknot_timeout = 0;

		u8 rqst_hotknot_state;
		if ((cmd_head.data == NULL)
		    || (cmd_head.data_len >= (DATA_LENGTH - GTP_ADDR_LENGTH))
		    || (cmd_head.data_len >= (len - CMD_HEAD_LENGTH))) {
			GTP_ERROR("copy_from_user data out of range.");
			return -EINVAL;
		}

		ret = copy_from_user(&cmd_head.data[GTP_ADDR_LENGTH],
				     &buff[CMD_HEAD_LENGTH], cmd_head.data_len);
		if (ret)
			GTP_ERROR("copy_from_user failed.");

		rqst_hotknot_state = cmd_head.data[GTP_ADDR_LENGTH];
		wait_hotknot_state |= rqst_hotknot_state;
		wait_hotknot_timeout =
			(cmd_head.data[GTP_ADDR_LENGTH + 1] << 8) +
			cmd_head.data[GTP_ADDR_LENGTH + 2];
		GTP_DEBUG(
			"Goodix tool received wait polling state:0x%x,timeout:%d, all wait state:0x%x",
			rqst_hotknot_state, wait_hotknot_timeout,
			wait_hotknot_state);
		got_hotknot_state &= (~rqst_hotknot_state);
		/* got_hotknot_extra_state = 0; */
		switch (rqst_hotknot_state) {
			set_current_state(TASK_INTERRUPTIBLE);
		case HN_DEVICE_PAIRED:
			hotknot_paired_flag = 0;
			wait_event_interruptible(
				bp_waiter,
				force_wake_flag ||
					rqst_hotknot_state ==
						(got_hotknot_state &
						 rqst_hotknot_state));
			wait_hotknot_state &= (~rqst_hotknot_state);
			if (rqst_hotknot_state !=
			    (got_hotknot_state & rqst_hotknot_state)) {
				GTP_ERROR(
					"Wait 0x%x block polling waiter failed.",
					rqst_hotknot_state);
				force_wake_flag = 0;
				return FAIL;
			}
			break;
		case HN_MASTER_SEND:
		case HN_SLAVE_RECEIVED:
			wait_event_interruptible_timeout(
				bp_waiter, force_wake_flag ||
						   rqst_hotknot_state ==
							   (got_hotknot_state &
							    rqst_hotknot_state),
				wait_hotknot_timeout);
			wait_hotknot_state &= (~rqst_hotknot_state);
			if (rqst_hotknot_state ==
			    (got_hotknot_state & rqst_hotknot_state))
				return got_hotknot_extra_state;

			GTP_ERROR("Wait 0x%x block polling waiter timeout.",
				  rqst_hotknot_state);
			force_wake_flag = 0;
			return FAIL;
		case HN_MASTER_DEPARTED:
		case HN_SLAVE_DEPARTED:
			wait_event_interruptible_timeout(
				bp_waiter, force_wake_flag ||
						   rqst_hotknot_state ==
							   (got_hotknot_state &
							    rqst_hotknot_state),
				wait_hotknot_timeout);
			wait_hotknot_state &= (~rqst_hotknot_state);
			if (rqst_hotknot_state !=
			    (got_hotknot_state & rqst_hotknot_state)) {
				GTP_ERROR(
					"Wait 0x%x block polling waitor timeout.",
					rqst_hotknot_state);
				force_wake_flag = 0;
				return FAIL;
			}
			break;
		default:
			GTP_ERROR("Invalid rqst_hotknot_state in goodix_tool.");
			break;
		}
		force_wake_flag = 0;
	} else if (cmd_head.wr == 23) {
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

static ssize_t goodix_tool_read(struct file *flie, char __user *page,
				size_t size, loff_t *ppos)
{
	s32 ret;

	GTP_DEBUG_FUNC();

	if (gtp_resetting == 1)
		return FAIL;

	if (*ppos) {
		*ppos = 0;
		return 0;
	}

	if (cmd_head.wr % 2) {
		GTP_ERROR("[READ] invaild operator fail!");
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

		if ((cmd_head.data == NULL)
		    || (cmd_head.addr_len >= (DATA_LENGTH))) {
			GTP_ERROR("copy_from_user data out of range.");
			return -EINVAL;
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
				return FAIL;
			}

			/* memcpy(&page[loc], &cmd_head.data[GTP_ADDR_LENGTH],
			 */
			/* len); */
			ret = simple_read_from_buffer(
				&page[loc], size, ppos,
				&cmd_head.data[GTP_ADDR_LENGTH], len);
			if (ret < 0)
				return ret;

			loc += len;

			GTP_DEBUG_ARRAY(&cmd_head.data[GTP_ADDR_LENGTH], len);
			GTP_DEBUG_ARRAY(page, len);
		}
		return cmd_head.data_len;
	} else if (cmd_head.wr == 2) {
		ret = simple_read_from_buffer(page, size, ppos, IC_TYPE,
					      sizeof(IC_TYPE));
		return ret;
	} else if (cmd_head.wr == 4) {
		u8 progress_buf[4];

		progress_buf[0] = show_len >> 8;
		progress_buf[1] = show_len & 0xff;
		progress_buf[2] = total_len >> 8;
		progress_buf[3] = total_len & 0xff;

		ret = simple_read_from_buffer(page, size, ppos, progress_buf,
					      4);
		return ret;
	} else if (cmd_head.wr == 6) {
		/* Read error code! */
	} else if (cmd_head.wr == 8) { /* Read driver version */
		ret = simple_read_from_buffer(page, size, ppos,
					      GTP_DRIVER_VERSION,
					      strlen(GTP_DRIVER_VERSION));
		return ret;
	}
	return -EPERM;
}

static int hotknot_open(struct inode *node, struct file *flip)
{
	GTP_DEBUG("Hotknot is enable.");
	gtp_hotknot_enabled = 1;
	return 0;
}

static int hotknot_release(struct inode *node, struct file *filp)
{
	GTP_DEBUG("Hotknot is disable.");
	gtp_hotknot_enabled = 0;
	return 0;
}

static ssize_t hotknot_write(struct file *filp, const char __user *buff,
			     size_t len, loff_t *ppos)
{
	s32 ret = 0;
	int cnt = 30;

	GTP_DEBUG_FUNC();
	GTP_DEBUG_ARRAY((u8 *)buff, len);

	while (cnt-- && gtp_loading_fw)
		ssleep(1);

	if (gtp_resetting == 1) {
		GTP_DEBUG("[Write]tpd_halt =1 fail!");
		return FAIL;
	}

	ret = copy_from_user(&cmd_head2, buff, CMD_HEAD_LENGTH);
	if (ret)
		GTP_ERROR("copy_from_user failed.");

	GTP_DEBUG("wr:0x%02x.", cmd_head2.wr);

	if (cmd_head2.wr == 1) {
		if ((cmd_head2.data == NULL)
		    || (cmd_head2.data_len >= (DATA_LENGTH - GTP_ADDR_LENGTH))
		    || (cmd_head2.data_len >= (len - CMD_HEAD_LENGTH))) {
			GTP_ERROR("copy_from_user data out of range.");
			return -EINVAL;
		}
		/* copy_from_user(&cmd_head2.data[cmd_head2.addr_len], */
		/* &buff[CMD_HEAD_LENGTH], cmd_head2.data_len); */
		ret = copy_from_user(&cmd_head2.data[GTP_ADDR_LENGTH],
				     &buff[CMD_HEAD_LENGTH],
				     cmd_head2.data_len);

		if (ret)
			GTP_ERROR("copy_from_user failed.");

		memcpy(&cmd_head2.data[GTP_ADDR_LENGTH - cmd_head2.addr_len],
		       cmd_head2.addr, cmd_head2.addr_len);

		GTP_DEBUG_ARRAY(cmd_head2.data,
				cmd_head2.data_len + cmd_head2.addr_len);
		GTP_DEBUG_ARRAY((u8 *)&buff[CMD_HEAD_LENGTH],
				cmd_head2.data_len);

		if (cmd_head2.flag == 1) {
			if (comfirm() == FAIL) {
				GTP_ERROR("[WRITE]Comfirm fail!");
				return FAIL;
			}
		} else if (cmd_head2.flag == 2) {
			/* Need interrupt! */
		}

		if (tool_i2c_write(&cmd_head2.data[GTP_ADDR_LENGTH -
						   cmd_head2.addr_len],
				   cmd_head2.data_len + cmd_head2.addr_len) <=
		    0) {
			GTP_ERROR("[WRITE]Write data failed!");
			return FAIL;
		}

		GTP_DEBUG_ARRAY(
			&cmd_head2.data[GTP_ADDR_LENGTH - cmd_head2.addr_len],
			cmd_head2.data_len + cmd_head2.addr_len);

		if (cmd_head2.delay)
			msleep(cmd_head2.delay);

		return cmd_head2.data_len + CMD_HEAD_LENGTH;
	} else if (cmd_head2.wr == 3) { /* Write ic type */
		if (cmd_head2.data_len > 16) {
			GTP_ERROR("copy_from_user data out of range.");
			return -EINVAL;
		}
		memcpy(IC_TYPE, cmd_head2.data, cmd_head2.data_len);
		register_i2c_func();

		return cmd_head2.data_len + CMD_HEAD_LENGTH;
	} else if (cmd_head2.wr == 5) {
		/* memcpy(IC_TYPE, cmd_head2.data, cmd_head2.data_len); */

		return cmd_head2.data_len + CMD_HEAD_LENGTH;
	} else if (cmd_head2.wr == 7) { /* disable irq! */
		gtp_irq_disable();
#ifdef CONFIG_GTP_ESD_PROTECT
		gtp_esd_switch(i2c_client_point, SWITCH_OFF);
#endif
#ifdef CONFIG_GTP_CHARGER_DETECT
		gtp_charger_switch(0);
#endif
		return CMD_HEAD_LENGTH;
	} else if (cmd_head2.wr == 9) { /* enable irq! */
		gtp_irq_enable();
#ifdef CONFIG_GTP_ESD_PROTECT
		gtp_esd_switch(i2c_client_point, SWITCH_ON);
#endif
#ifdef CONFIG_GTP_CHARGER_DETECT
		gtp_charger_switch(1);
#endif
		return CMD_HEAD_LENGTH;
	} else if (cmd_head2.wr == 17) {
		if ((cmd_head2.data == NULL)
		    || (cmd_head2.data_len >= (DATA_LENGTH - GTP_ADDR_LENGTH))
		    || (cmd_head2.data_len >= (len - CMD_HEAD_LENGTH))) {
			GTP_ERROR("copy_from_user data out of range.");
			return -EINVAL;
		}
		ret = copy_from_user(&cmd_head2.data[GTP_ADDR_LENGTH],
				     &buff[CMD_HEAD_LENGTH],
				     cmd_head2.data_len);
		if (ret)
			GTP_DEBUG("copy_from_user failed.");

		if (cmd_head2.data[GTP_ADDR_LENGTH]) {
			GTP_DEBUG("gtp enter rawdiff.");
			gtp_rawdiff_mode = true;
		} else {
			gtp_rawdiff_mode = false;
			GTP_DEBUG("gtp leave rawdiff.");
		}

		return CMD_HEAD_LENGTH;
	}
#ifdef UPDATE_FUNCTIONS
	else if (cmd_head2.wr == 11) { /* Enter update mode! */
		if (gup_enter_update_mode(gt_client) == FAIL)
			return FAIL;
	} else if (cmd_head2.wr == 13) { /* Leave update mode! */
		gup_leave_update_mode();
	} else if (cmd_head2.wr == 15) { /* Update firmware! */
		show_len = 0;
		total_len = 0;
		if ((cmd_head2.data == NULL)
			|| (cmd_head2.data_len >= DATA_LENGTH)
			|| (cmd_head2.data_len >= (len - CMD_HEAD_LENGTH))) {
			GTP_ERROR("copy_from_user data out of range.");
			return -EINVAL;
		}
		memset(cmd_head2.data, 0, cmd_head2.data_len + 1);
		memcpy(cmd_head2.data, &buff[CMD_HEAD_LENGTH],
		       cmd_head2.data_len);
		GTP_DEBUG("update firmware, filename: %s", cmd_head2.data);
		if (gup_update_proc((void *)cmd_head2.data) == FAIL)
			return FAIL;

	}
#endif
#ifdef CONFIG_GTP_HOTKNOT
	else if (cmd_head2.wr == 19) { /* load subsystem */
		if ((cmd_head2.data == NULL)
			|| (cmd_head2.data_len >= DATA_LENGTH)
			|| (cmd_head2.data_len >= (len - CMD_HEAD_LENGTH))) {
			GTP_ERROR("copy_from_user data out of range.");
			return -EINVAL;
		}
		ret = copy_from_user(&cmd_head2.data[0], &buff[CMD_HEAD_LENGTH],
				     cmd_head2.data_len);
		if (cmd_head2.data[0] == 0) {
			if (gup_load_hotknot_fw() == FAIL)
				return FAIL;

		} else if (cmd_head2.data[0] == 1) {
			if (gup_load_authorization_fw() == FAIL)
				return FAIL;

		} else if (cmd_head2.data[0] == 2) {
			if (gup_recovery_touch() == FAIL)
				return FAIL;

		} else if (cmd_head2.data[0] == 3) {
			if (gup_load_touch_fw(NULL) == FAIL)
				return FAIL;
		}
	}
#endif
#ifdef CONFIG_HOTKNOT_BLOCK_RW
	else if (cmd_head2.wr == 21) {
		u16 wait_hotknot_timeout = 0;
		u8 rqst_hotknot_state;

		if ((cmd_head2.data == NULL)
		    || (cmd_head2.data_len >= (DATA_LENGTH - GTP_ADDR_LENGTH))
		    || (cmd_head2.data_len >= (len - CMD_HEAD_LENGTH))) {
			GTP_ERROR("copy_from_user data out of range.");
			return -EINVAL;
		}

		ret = copy_from_user(&cmd_head2.data[GTP_ADDR_LENGTH],
				     &buff[CMD_HEAD_LENGTH],
				     cmd_head2.data_len);

		if (ret)
			GTP_ERROR("copy_from_user failed.");

		rqst_hotknot_state = cmd_head2.data[GTP_ADDR_LENGTH];
		wait_hotknot_state |= rqst_hotknot_state;
		wait_hotknot_timeout =
			(cmd_head2.data[GTP_ADDR_LENGTH + 1] << 8) +
			cmd_head2.data[GTP_ADDR_LENGTH + 2];
		GTP_DEBUG(
			"Goodix tool received wait polling state:0x%x,timeout:%d, all wait state:0x%x",
			rqst_hotknot_state, wait_hotknot_timeout,
			wait_hotknot_state);
		got_hotknot_state &= (~rqst_hotknot_state);
		/* got_hotknot_extra_state = 0; */
		switch (rqst_hotknot_state) {
			set_current_state(TASK_INTERRUPTIBLE);
		case HN_DEVICE_PAIRED:
			hotknot_paired_flag = 0;
			wait_event_interruptible(
				bp_waiter,
				force_wake_flag ||
					rqst_hotknot_state ==
						(got_hotknot_state &
						 rqst_hotknot_state));
			wait_hotknot_state &= (~rqst_hotknot_state);
			if (rqst_hotknot_state !=
			    (got_hotknot_state & rqst_hotknot_state)) {
				GTP_ERROR(
					"Wait 0x%x block polling waiter failed.",
					rqst_hotknot_state);
				force_wake_flag = 0;
				return FAIL;
			}
			break;
		case HN_MASTER_SEND:
		case HN_SLAVE_RECEIVED:
			wait_event_interruptible_timeout(
				bp_waiter, force_wake_flag ||
						   rqst_hotknot_state ==
							   (got_hotknot_state &
							    rqst_hotknot_state),
				wait_hotknot_timeout);
			wait_hotknot_state &= (~rqst_hotknot_state);
			if (rqst_hotknot_state ==
			    (got_hotknot_state & rqst_hotknot_state))
				return got_hotknot_extra_state;

			GTP_ERROR("Wait 0x%x block polling waiter timeout.",
				  rqst_hotknot_state);
			force_wake_flag = 0;
			return FAIL;
		case HN_MASTER_DEPARTED:
		case HN_SLAVE_DEPARTED:
			wait_event_interruptible_timeout(
				bp_waiter, force_wake_flag ||
						   rqst_hotknot_state ==
							   (got_hotknot_state &
							    rqst_hotknot_state),
				wait_hotknot_timeout);
			wait_hotknot_state &= (~rqst_hotknot_state);
			if (rqst_hotknot_state !=
			    (got_hotknot_state & rqst_hotknot_state)) {
				GTP_ERROR(
					"Wait 0x%x block polling waitor timeout.",
					rqst_hotknot_state);
				force_wake_flag = 0;
				return FAIL;
			}
			break;
		default:
			GTP_ERROR("Invalid rqst_hotknot_state in goodix_tool.");
			break;
		}
		force_wake_flag = 0;
	} else if (cmd_head2.wr == 23) {
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

static ssize_t _hotknot_read(struct file *file, char __user *page, size_t size,
			     loff_t *ppos)
{
	int ret;

	GTP_DEBUG_FUNC();

	if (gtp_resetting == 1) {
		GTP_DEBUG("[READ]tpd_halt =1 fail!");
		return FAIL;
	}
	if (*ppos) {
		*ppos = 0;
		return 0;
	}
	if (cmd_head2.wr % 2) {
		GTP_ERROR("[READ] invaild operator fail!");
		return FAIL;
	} else if (!cmd_head2.wr) {
		u16 len = 0;
		s16 data_len = 0;
		u16 loc = 0;

		if (cmd_head2.flag == 1) {
			if (comfirm() == FAIL) {
				GTP_ERROR("[READ]Comfirm fail!");
				return FAIL;
			}
		} else if (cmd_head2.flag == 2) {
			/* Need interrupt! */
		}

		if ((cmd_head2.data == NULL)
		    || (cmd_head2.addr_len >= (DATA_LENGTH))) {
			GTP_ERROR("copy_from_user data out of range.");
			return -EINVAL;
		}
		memcpy(cmd_head2.data, cmd_head2.addr, cmd_head2.addr_len);

		GTP_DEBUG("[CMD HEAD DATA] ADDR:0x%02x%02x.", cmd_head2.data[0],
			  cmd_head2.data[1]);
		GTP_DEBUG("[CMD HEAD ADDR] ADDR:0x%02x%02x.", cmd_head2.addr[0],
			  cmd_head2.addr[1]);

		if (cmd_head2.delay)
			msleep(cmd_head2.delay);

		data_len = cmd_head2.data_len;

		while (data_len > 0) {
			if (data_len > DATA_LENGTH)
				len = DATA_LENGTH;
			else
				len = data_len;

			data_len -= DATA_LENGTH;

			if (tool_i2c_read(cmd_head2.data, len) <= 0) {
				GTP_ERROR("[READ]Read data failed!");
				return FAIL;
			}

			ret = copy_to_user(&page[loc],
					   &cmd_head2.data[GTP_ADDR_LENGTH],
					   len);
			loc += len;

			GTP_DEBUG_ARRAY(&cmd_head2.data[GTP_ADDR_LENGTH], len);
			GTP_DEBUG_ARRAY(page, len);
		}
	} else if (cmd_head2.wr == 2) {
		GTP_DEBUG("Return ic type:%s len:%d.", page,
			  (s32)cmd_head2.data_len);
		*ppos += cmd_head2.data_len;
		return cmd_head2.data_len;
	} else if (cmd_head2.wr == 4) {
		page[0] = show_len >> 8;
		page[1] = show_len & 0xff;
		page[2] = total_len >> 8;
		page[3] = total_len & 0xff;
		*ppos += cmd_head2.data_len;
		return cmd_head2.data_len;
	} else if (cmd_head2.wr == 6) {
		/* Read error code! */
	} else if (cmd_head2.wr == 8) { /* Read driver version */
		s32 tmp_len = strlen(GTP_DRIVER_VERSION);

		ret = copy_to_user(page, GTP_DRIVER_VERSION, tmp_len);
		page[tmp_len] = 0;
	}
	*ppos += cmd_head2.data_len;
	return cmd_head2.data_len;
}

static ssize_t hotknot_read(struct file *file, char __user *page, size_t size,
			    loff_t *ppos)
{
	*ppos = 0;
	return _hotknot_read(file, page, size, ppos);
}
