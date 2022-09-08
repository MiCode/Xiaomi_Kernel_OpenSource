// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <generated/utsrelease.h>
#include "gt1x_tpd_common.h"

static ssize_t gt1x_tool_read(struct file *filp, char __user *buffer,
					size_t count, loff_t *ppos);
static ssize_t gt1x_tool_write(struct file *filp, const char *buffer,
					size_t count, loff_t *ppos);
static s32 gt1x_tool_release(struct inode *inode, struct file *filp);
static s32 gt1x_tool_open(struct inode *inode, struct file *file);

#pragma pack(1)
struct st_cmd_head {
	u8 wr;			/*write read flag£¬0:R  1:W  2:PID 3:*/
	u8 flag;		/*0:no need flag/int 1: need flag  2:need int*/
	u8 flag_addr[2];	/*flag address*/
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
};
#pragma pack()
struct st_cmd_head cmd_head;
static DEFINE_MUTEX(rw_mutex);

s32 DATA_LENGTH;
s8 IC_TYPE[16] = "GT9XX";

#define UPDATE_FUNCTIONS
#define DATA_LENGTH_UINT    512
#define CMD_HEAD_LENGTH     (sizeof(struct st_cmd_head) - sizeof(u8 *))

static char procname[20] = { 0 };

static struct proc_dir_entry *gt1x_tool_proc_entry;
static const struct file_operations gt1x_tool_fops = {
	.read = gt1x_tool_read,
	.write = gt1x_tool_write,
	.open = gt1x_tool_open,
	.release = gt1x_tool_release,
	.owner = THIS_MODULE,
};
static void set_tool_node_name(char *procname)
{
	int v0 = 0, v1 = 0, v2 = 0;
	int ret;

	ret = sscanf(UTS_RELEASE, "%d.%d.%d", &v0, &v1, &v2);
	sprintf(procname, "gmnode%02d%02d%02d", v0, v1, v2);
}

int gt1x_init_tool_node(void)
{
	memset(&cmd_head, 0, sizeof(cmd_head));
	/*if the first operation is read, will return fail.*/
	cmd_head.wr = 1;
	cmd_head.data = kzalloc(DATA_LENGTH_UINT, GFP_KERNEL);
	if (cmd_head.data == NULL) {
		GTP_ERROR("Apply for memory failed.");
		return -1;
	}
	GTP_INFO("Applied memory size:%d.", DATA_LENGTH_UINT);
	DATA_LENGTH = DATA_LENGTH_UINT - GTP_ADDR_LENGTH;

	set_tool_node_name(procname);

	gt1x_tool_proc_entry =
		proc_create(procname, 0664, NULL, &gt1x_tool_fops);
	if (gt1x_tool_proc_entry == NULL) {
		GTP_ERROR("Couldn't create proc entry!");
		return -1;
	}
	GTP_INFO("Create proc entry success!");
	return 0;
}

void gt1x_deinit_tool_node(void)
{
	remove_proc_entry(procname, NULL);
	kfree(cmd_head.data);
	cmd_head.data = NULL;
}

static s32 tool_i2c_read(u8 *buf, u16 len)
{
	u16 addr = (buf[0] << 8) + buf[1];

	if (!gt1x_i2c_read(addr, &buf[2], len))
		return 1;
	return -1;
}

static s32 tool_i2c_write(u8 *buf, u16 len)
{
	u16 addr = (buf[0] << 8) + buf[1];

	if (!gt1x_i2c_write(addr, &buf[2], len - 2))
		return 1;
	return -1;
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
		GTP_DEBUG("equal:src:0x%02x   dst:0x%02x   ret:%d.",
				src, dst, (s32) ret);
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
 *Function:
 *   Comfirm function.
 *Input:
 *   None.
 *Output:
 *   Return write length.
 ********************************************************/
static u8 comfirm(void)
{
	s32 i = 0;
	u8 buf[32];

	memcpy(buf, cmd_head.flag_addr, cmd_head.addr_len);

	for (i = 0; i < cmd_head.times; i++) {
		if (tool_i2c_read(buf, 1) <= 0) {
			GTP_ERROR("Read flag data failed!");
			return -1;
		}

		if (true == relation(buf[GTP_ADDR_LENGTH],
					cmd_head.flag_val,
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
		return -1;
	}

	return 0;
}

/*******************************************************
 *Function:
 *   Goodix tool write function.
 *Input:
 * standard proc write function param.
 *Output:
 *   Return write length.
 ********************************************************/
static ssize_t gt1x_tool_write(struct file *filp, const char __user *buff,
					size_t len, loff_t *data)
{
	u64 ret = 0;
	u8 *pre_data_p;
	u8 *post_data_p;

	GTP_DEBUG_FUNC();

	mutex_lock(&rw_mutex);
	pre_data_p = cmd_head.data;
	ret = copy_from_user(&cmd_head, buff, CMD_HEAD_LENGTH);
	if (ret)
		GTP_ERROR("copy_from_user failed.");
	post_data_p = cmd_head.data;
	if (pre_data_p != post_data_p) {
		GTP_ERROR("pointer is overwritten! %p, %p, %p, %p, %dx\n",
			pre_data_p, post_data_p,
			&cmd_head, &cmd_head.data, (int)CMD_HEAD_LENGTH);
	}

	GTP_DEBUG("wr  :0x%02x.", cmd_head.wr);
	/*
	 * GTP_DEBUG("flag:0x%02x.", cmd_head.flag);
	 *  GTP_DEBUG("flag addr:0x%02x%02x.", cmd_head.flag_addr[0],
	 *                           cmd_head.flag_addr[1]);
	 *  GTP_DEBUG("flag val:0x%02x.", cmd_head.flag_val);
	 *  GTP_DEBUG("flag rel:0x%02x.", cmd_head.flag_relation);
	 *  GTP_DEBUG("circle  :%d.", (s32)cmd_head.circle);
	 *  GTP_DEBUG("times   :%d.", (s32)cmd_head.times);
	 *  GTP_DEBUG("retry   :%d.", (s32)cmd_head.retry);
	 *  GTP_DEBUG("delay   :%d.", (s32)cmd_head.delay);
	 *  GTP_DEBUG("data len:%d.", (s32)cmd_head.data_len);
	 *  GTP_DEBUG("addr len:%d.", (s32)cmd_head.addr_len);
	 *  GTP_DEBUG("addr:0x%02x%02x.", cmd_head.addr[0], cmd_head.addr[1]);
	 *  GTP_DEBUG("len:%d.", (s32)len);
	 *  GTP_DEBUG("buf[20]:0x%02x.", buff[CMD_HEAD_LENGTH]);
	 */
	GTP_DEBUG_ARRAY((u8 *) cmd_head.data, cmd_head.data_len);

	if (cmd_head.data_len > DATA_LENGTH)
		cmd_head.data_len = DATA_LENGTH;

	if (cmd_head.wr == 1) {
		u16 addr, data_len, pos;

		if (cmd_head.flag == 1) {
			if (comfirm()) {
				GTP_ERROR("[WRITE]Comfirm fail!");
				ret = -1;
				goto out;
			}
		} else if (cmd_head.flag == 2) {
			/*Need interrupt!*/
		}

		addr = (cmd_head.addr[0] << 8) + cmd_head.addr[1];
		data_len = cmd_head.data_len;
		pos = 0;
		while (data_len > 0) {
			len = data_len > DATA_LENGTH ? DATA_LENGTH : data_len;
			ret = copy_from_user(&cmd_head.data[GTP_ADDR_LENGTH],
						&buff[CMD_HEAD_LENGTH + pos],
						len);
			if (ret) {
				GTP_ERROR("[WRITE]copy_from_user failed.");
				ret = -1;
				goto out;
			}
			cmd_head.data[0] = ((addr >> 8) & 0xFF);
			cmd_head.data[1] = (addr & 0xFF);

			GTP_DEBUG_ARRAY(cmd_head.data, len + GTP_ADDR_LENGTH);

			if (tool_i2c_write(cmd_head.data,
						len + GTP_ADDR_LENGTH) <= 0) {
				GTP_ERROR("[WRITE]Write data failed!");
				ret = -1;
				goto out;
			}
			addr += len;
			pos += len;
			data_len -= len;
		}

		if (cmd_head.delay)
			msleep(cmd_head.delay);
		mutex_unlock(&rw_mutex);
		return cmd_head.data_len + CMD_HEAD_LENGTH;
	} else if (cmd_head.wr == 3) {	/*gt1x unused*/

		cmd_head.data_len =
			cmd_head.data_len > sizeof(IC_TYPE) ?
				sizeof(IC_TYPE) : cmd_head.data_len;
		memcpy(IC_TYPE, cmd_head.data, cmd_head.data_len);
		ret = cmd_head.data_len + CMD_HEAD_LENGTH;
		goto out;
	} else if (cmd_head.wr == 5) {

		/*memcpy(IC_TYPE, cmd_head.data, cmd_head.data_len);*/
		mutex_unlock(&rw_mutex);
		return cmd_head.data_len + CMD_HEAD_LENGTH;
	} else if (cmd_head.wr == 7) {	/*disable irq!*/
		gt1x_irq_disable();
#ifdef CONFIG_GTP_ESD_PROTECT
		gt1x_esd_switch(SWITCH_OFF);
#endif
		ret = CMD_HEAD_LENGTH;
		goto out;
	} else if (cmd_head.wr == 9) {	/*enable irq!*/
		gt1x_irq_enable();
#ifdef CONFIG_GTP_ESD_PROTECT
		gt1x_esd_switch(SWITCH_ON);
#endif
		ret = CMD_HEAD_LENGTH;
		goto out;
	} else if (cmd_head.wr == 17) {
		ret = copy_from_user(&cmd_head.data[GTP_ADDR_LENGTH],
					&buff[CMD_HEAD_LENGTH],
					cmd_head.data_len);
		if (ret) {
			GTP_DEBUG("copy_from_user failed.");
			ret = -1;
			goto out;
		}

		if (cmd_head.data[GTP_ADDR_LENGTH]) {
			GTP_DEBUG("gtp enter rawdiff.");
			gt1x_rawdiff_mode = true;
		} else {
			gt1x_rawdiff_mode = false;
			GTP_DEBUG("gtp leave rawdiff.");
		}
		ret = CMD_HEAD_LENGTH;
		goto out;
	} else if (cmd_head.wr == 11) {
		gt1x_enter_update_mode();
	} else if (cmd_head.wr == 13) {
		gt1x_leave_update_mode();
	} else if (cmd_head.wr == 15) {
		memset(cmd_head.data, 0, cmd_head.data_len + 1);
		ret = copy_from_user(cmd_head.data,
				&buff[CMD_HEAD_LENGTH], cmd_head.data_len);
		if (ret) {
			GTP_DEBUG("copy_from_user failed.");
			ret = -1;
			goto out;
		}
		GTP_DEBUG("update firmware, filename: %s", cmd_head.data);
		ret = gt1x_update_firmware((void *)cmd_head.data);
		if (ret) {
			ret = -1;
			goto out;
		}
	}
	ret = CMD_HEAD_LENGTH;
out:
	mutex_unlock(&rw_mutex);
	return ret;
}

static u8 devicecount;
static s32 gt1x_tool_open(struct inode *inode, struct file *file)
{
	GTP_DEBUG("gt1x_tool_proc open start.");
	if (devicecount > 0)
		return -ERESTARTSYS;
	GTP_DEBUG("gt1x_tool_proc open success!");
	devicecount++;
	return 0;
}

static s32 gt1x_tool_release(struct inode *inode, struct file *filp)
{
	GTP_DEBUG("gt1x_tool_proc release start.");
	devicecount--;
	GTP_DEBUG("gt1x_tool_proc release!");
	return 0;
}

/*******************************************************
 * Function:
 *   Goodix tool read function.
 *Input:
 * standard proc read function param.
 * Output:
 *   Return read length.
 ********************************************************/
static ssize_t gt1x_tool_read(struct file *filp, char __user *buffer,
					size_t count, loff_t *ppos)
{
	u64 ret;

	GTP_DEBUG_FUNC();
	if (*ppos) {
		GTP_DEBUG("[PARAM]size: %d, *ppos: %d", (int)count, (int)*ppos);
		*ppos = 0;
		return 0;
	}

	if (cmd_head.data_len > DATA_LENGTH)
		cmd_head.data_len = DATA_LENGTH;
	mutex_lock(&rw_mutex);
	if (cmd_head.wr % 2) {
		GTP_ERROR("[READ] invaild operator fail!");
		ret = -1;
		goto out;
	} else if (!cmd_head.wr) {
		u16 addr, data_len, len, loc;

		if (cmd_head.flag == 1) {
			if (comfirm()) {
				GTP_ERROR("[READ]Comfirm fail!");
				ret = -1;
				goto out;
			}
		} else if (cmd_head.flag == 2) {
			/*Need interrupt!*/
		}

		addr = (cmd_head.addr[0] << 8) + cmd_head.addr[1];
		data_len = cmd_head.data_len;
		loc = 0;

		GTP_DEBUG("[READ] ADDR:0x%04X.", addr);
		GTP_DEBUG("[READ] Length: %d", data_len);

		if (cmd_head.delay)
			msleep(cmd_head.delay);

		while (data_len > 0) {
			len = data_len > DATA_LENGTH ? DATA_LENGTH : data_len;
			cmd_head.data[0] = (addr >> 8) & 0xFF;
			cmd_head.data[1] = (addr & 0xFF);
			if (tool_i2c_read(cmd_head.data, len) <= 0) {
				GTP_ERROR("[READ]Read data failed!");
				ret = -1;
				goto out;
			}
			if (copy_to_user(&buffer[loc],
					&cmd_head.data[GTP_ADDR_LENGTH],
					len)) {
				GTP_ERROR("[READ]copy_to_user failed!");
				ret = -1;
				goto out;
			}
			data_len -= len;
			addr += len;
			loc += len;
			GTP_DEBUG_ARRAY(&cmd_head.data[GTP_ADDR_LENGTH], len);
		}
		*ppos += cmd_head.data_len;
		ret = cmd_head.data_len;
		goto out;
	} else if (cmd_head.wr == 2) {
		GTP_DEBUG("Return ic type:%s len:%d.",
				buffer, (s32) cmd_head.data_len);
		ret = -1;
		goto out;
	} else if (cmd_head.wr == 4) {
		u8 val[4];

		val[0] = update_info.progress >> 8;
		val[1] = update_info.progress & 0xff;
		val[2] = update_info.max_progress >> 8;
		val[3] = update_info.max_progress & 0xff;
		if (copy_to_user(buffer, val, sizeof(val))) {
			GTP_ERROR("[READ]copy_to_user failed!");
			ret = cmd_head.data_len;
		goto out;
		}
		*ppos += 4;
		ret = 4;
		goto out;
	} else if (cmd_head.wr == 6) {
		/*Read error code!*/
		ret = -1;
		goto out;
	} else if (cmd_head.wr == 8) {	/*Read driver version*/
		s32 tmp_len = strlen(GTP_DRIVER_VERSION) + 1;
		char *drv_ver = NULL;

		if (count < tmp_len) {
			ret = -1;
			goto out;
		}
		drv_ver = kzalloc(tmp_len, GFP_ATOMIC);

		if (drv_ver == NULL) {
			GTP_ERROR("Allocate %d buffer fail\n", tmp_len);
			ret = -1;
			goto out;
		}
		strncpy(drv_ver, GTP_DRIVER_VERSION,
			strlen(GTP_DRIVER_VERSION));
		drv_ver[strlen(GTP_DRIVER_VERSION)] = 0;
		if (copy_to_user(buffer, drv_ver, tmp_len)) {
			GTP_ERROR("[READ]copy_to_user failed");
			kfree(drv_ver);
			ret = -1;
			goto out;
		}
		*ppos += tmp_len;
		kfree(drv_ver);
		mutex_unlock(&rw_mutex);
		return tmp_len;
	}
	*ppos += cmd_head.data_len;
	ret = cmd_head.data_len;
out:
	mutex_unlock(&rw_mutex);
	return ret;
}

void gt1x_tool_exit(void)
{
	remove_proc_entry(procname, NULL);
}
EXPORT_SYMBOL_GPL(gt1x_tool_exit);
