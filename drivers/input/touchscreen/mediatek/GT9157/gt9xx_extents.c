/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include "include/tpd_gt9xx_common.h"
#include <asm/ioctl.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#ifdef CONFIG_GTP_GESTURE_WAKEUP

#define GESTURE_NODE "goodix_gesture"
#define GTP_REG_WAKEUP_GESTURE 0x814B
#define GTP_REG_WAKEUP_GESTURE_DETAIL 0x9420

#define SETBIT(longlong, bit) (longlong[bit / 8] |= (1 << bit % 8))
#define CLEARBIT(longlong, bit) (longlong[bit / 8] &= (~(1 << bit % 8)))
#define QUERYBIT(longlong, bit) (!!(longlong[bit / 8] & (1 << bit % 8)))

static u8 gestures_flag[32];
struct gesture_data gesture_data;
static struct mutex gesture_data_mutex;

static inline s32 ges_i2c_write_bytes(u16 addr, u8 *buf, s32 len)
{
	return i2c_write_bytes(i2c_client_point, addr, buf, len);
}

static inline s32 ges_i2c_read_bytes(u16 addr, u8 *buf, s32 len)
{
	return i2c_read_bytes(i2c_client_point, addr, buf, len);
}

static ssize_t gtp_gesture_data_read(struct file *file, char __user *page,
				     size_t size, loff_t *ppos)
{
	s32 ret = -1;

	GTP_DEBUG("visit %s. ppos:%d", __func__, (int)*ppos);
	if (*ppos)
		return 0;

	if (size == 4) {
		ret = copy_to_user(((u8 __user *)page), "GT1X", 4);
		return 4;
	}
	ret = simple_read_from_buffer(page, size, ppos, &gesture_data,
				      sizeof(gesture_data));

	GTP_DEBUG("Got the gesture data.");
	return ret;
}

static ssize_t gtp_gesture_data_write(struct file *filp,
				      const char __user *buff, size_t len,
				      loff_t *off)
{
	s32 ret = 0;

	ret = copy_from_user(&gesture_data.enabled, buff, 1);
	if (ret) {
		GTP_ERROR("copy_from_user failed.");
		return -EPERM;
	}

	GTP_DEBUG("gesture enabled:%x, ret:%d", gesture_data.enabled, ret);

	return len;
}

s8 gtp_enter_doze(void)
{
	int ret = -1;
	s8 retry = 0;
	u8 i2c_control_buf[1] = {8};

	GTP_DEBUG("Entering doze mode.");
	while (retry++ < 5) {
		ret = ges_i2c_write_bytes(0x8046, i2c_control_buf, 1);
		if (ret < 0) {
			GTP_DEBUG("failed to set doze flag into 0x8046, %d",
				  retry);
			continue;
		}

		ret = ges_i2c_write_bytes(0x8040, i2c_control_buf, 1);
		if (!ret) {
			gesture_data.doze_status = DOZE_ENABLED;
			GTP_INFO("Gesture mode enabled.");
			return ret;
		}
		msleep(20);
	}
	GTP_ERROR("GTP send doze cmd failed.");
	return ret;
}

s32 gesture_event_handler(struct input_dev *dev)
{
	u8 doze_buf[4] = {0};
	unsigned int key_code;
	s32 ret = 0;
	int len, extra_len;

	if (gesture_data.doze_status == DOZE_ENABLED) {
		ret = ges_i2c_read_bytes(GTP_REG_WAKEUP_GESTURE, doze_buf, 4);
		GTP_DEBUG("0x%x = 0x%02X,0x%02X,0x%02X,0x%02X",
			  GTP_REG_WAKEUP_GESTURE, doze_buf[0], doze_buf[1],
			  doze_buf[2], doze_buf[3]);
	/*GTP_DEBUG("0x%x = 0x%02X,0x%02X", GTP_REG_WAKEUP_GESTURE, */
		/* doze_buf[0], doze_buf[1]); */
		if (ret == 0 && doze_buf[0] != 0) {
			if (!QUERYBIT(gestures_flag, doze_buf[0])) {
				GTP_INFO(
					"Sorry, this gesture has been disabled.");
				doze_buf[0] = 0x00;
				ges_i2c_write_bytes(GTP_REG_WAKEUP_GESTURE,
						    doze_buf, 1);
				gtp_enter_doze();
				return 0;
			}

			mutex_lock(&gesture_data_mutex);
			len = doze_buf[1] & 0x7F;
			if (len > GESTURE_MAX_POINT_COUNT) {
				GTP_ERROR(
					"Gesture contain too many points!(%d)",
					len);
				len = GESTURE_MAX_POINT_COUNT;
			}
			if (len > 0) {
				ret = ges_i2c_read_bytes(
					GTP_REG_WAKEUP_GESTURE_DETAIL,
					&gesture_data.data[4], len * 4);
				if (ret < 0) {
					GTP_DEBUG("Read gesture data failed.");
					mutex_unlock(&gesture_data_mutex);
					return 0;
				}
			}

			extra_len = doze_buf[1] & 0x80 ? doze_buf[3] : 0;
			if (extra_len > 80) {
				GTP_ERROR(
					"Gesture contain too many extra data!(%d)",
					extra_len);
				extra_len = 80;
			}
			if (extra_len > 0) {
				ret = ges_i2c_read_bytes(
					GTP_REG_WAKEUP_GESTURE + 4,
					&gesture_data.data[4 + len * 4],
					extra_len);
				if (ret < 0) {
					GTP_DEBUG(
						"Read extra gesture data failed.");
					mutex_unlock(&gesture_data_mutex);
					return 0;
				}
			}

			doze_buf[2] &= ~0x30;
			doze_buf[2] |= extra_len > 0 ? 0x20 : 0x10;

			gesture_data.data[0] = doze_buf[0]; /* gesture type */
			gesture_data.data[1] = len; /* gesture points number */
			gesture_data.data[2] = doze_buf[2];
			gesture_data.data[3] = extra_len;
			mutex_unlock(&gesture_data_mutex);

			key_code = doze_buf[0] < 16 ? KEY_F3 : KEY_F2;
			GTP_DEBUG("Gesture: 0x%02X, points: %d", doze_buf[0],
				  doze_buf[1]);

			doze_buf[0] = 0;
			ges_i2c_write_bytes(GTP_REG_WAKEUP_GESTURE, doze_buf,
					    1);

			input_report_key(dev, key_code, 1);
			input_sync(dev);
			input_report_key(dev, key_code, 0);
			input_sync(dev);
			return 2; /* doze enabled and get valid gesture data */
		}
		return 1; /* doze enabled, but no invalid gesutre data */
	}
	return 0; /* doze not enabled */
}

void gesture_clear_wakeup_data(void)
{
	mutex_lock(&gesture_data_mutex);
	memset(gesture_data.data, 0, 4);
	mutex_unlock(&gesture_data_mutex);
}

#define GOODIX_MAGIC_NUMBER 'G'
#define NEGLECT_SIZE_MASK (~(_IOC_SIZEMASK << _IOC_SIZESHIFT))

#define GESTURE_ENABLE_TOTALLY _IO(GOODIX_MAGIC_NUMBER, 1)
#define GESTURE_DISABLE_TOTALLY _IO(GOODIX_MAGIC_NUMBER, 2)
#define GESTURE_ENABLE_PARTLY _IO(GOODIX_MAGIC_NUMBER, 3)
#define GESTURE_DISABLE_PARTLY _IO(GOODIX_MAGIC_NUMBER, 4)
#define GESTURE_DATA_OBTAIN                                                    \
	(_IOR(GOODIX_MAGIC_NUMBER, 6, u8) & NEGLECT_SIZE_MASK)
#define GESTURE_DATA_ERASE _IO(GOODIX_MAGIC_NUMBER, 7)

#define IO_IIC_READ (_IOR(GOODIX_MAGIC_NUMBER, 100, u8) & NEGLECT_SIZE_MASK)
#define IO_IIC_WRITE (_IOW(GOODIX_MAGIC_NUMBER, 101, u8) & NEGLECT_SIZE_MASK)
#define IO_RESET_GUITAR _IO(GOODIX_MAGIC_NUMBER, 102)
#define IO_DISABLE_IRQ _IO(GOODIX_MAGIC_NUMBER, 103)
#define IO_ENABLE_IRQ _IO(GOODIX_MAGIC_NUMBER, 104)
#define IO_GET_VERSION (_IOR(GOODIX_MAGIC_NUMBER, 110, u8) & NEGLECT_SIZE_MASK)
#define IO_PRINT (_IOW(GOODIX_MAGIC_NUMBER, 111, u8) & NEGLECT_SIZE_MASK)
#define IO_VERSION "V1.0-20141015"

#define CMD_HEAD_LENGTH 20

static s32 io_iic_read(u8 *data, void __user *arg)
{
	s32 err = -1;
	s32 data_length = 0;
	u16 addr = 0;

	err = copy_from_user(data, arg, CMD_HEAD_LENGTH);
	if (err) {
		GTP_DEBUG("Can't access the memory.");
		return err;
	}

	addr = data[0] << 8 | data[1];
	data_length = data[2] << 8 | data[3];

	err = ges_i2c_read_bytes(addr, &data[CMD_HEAD_LENGTH], data_length);
	if (!err) {
		err = copy_to_user(&((u8 __user *)arg)[CMD_HEAD_LENGTH],
				   &data[CMD_HEAD_LENGTH], data_length);
		if (err) {
			GTP_ERROR(
				"ERROR when copy to user.[addr: %04x], [read length:%d]",
				addr, data_length);
			return err;
		}
		err = CMD_HEAD_LENGTH + data_length;
	}
	GTP_DEBUG("IIC_READ.addr:0x%4x, length:%d, ret:%d", addr, data_length,
		  err);
	GTP_DEBUG_ARRAY((&data[CMD_HEAD_LENGTH]), data_length);

	return err;
}

static s32 io_iic_write(u8 *data)
{
	s32 err = -1;
	s32 data_length = 0;
	u16 addr = 0;

	addr = data[0] << 8 | data[1];
	data_length = data[2] << 8 | data[3];

	err = ges_i2c_write_bytes(addr, &data[CMD_HEAD_LENGTH], data_length);
	if (!err)
		err = CMD_HEAD_LENGTH + data_length;

	GTP_DEBUG("IIC_WRITE.addr:0x%4x, length:%d, ret:%d", addr, data_length,
		  err);
	GTP_DEBUG_ARRAY((&data[CMD_HEAD_LENGTH]), data_length);
	return err;
}

static long gtp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 value = 0;
	s32 ret = 0;
	u8 *data = NULL;

	if (_IOC_DIR(cmd)) {
		s32 err = -1;
		s32 data_length = _IOC_SIZE(cmd);

		data = kzalloc(data_length, GFP_KERNEL);
		memset(data, 0, data_length);

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			err = copy_from_user(data, (void __user *)arg,
					     data_length);
			if (err) {
				GTP_DEBUG("Can't access the memory.");
				kfree(data);
				return -1;
			}
		}
	} else {
		value = (u32)arg;
	}

	switch (cmd & NEGLECT_SIZE_MASK) {
	case IO_GET_VERSION:
		if ((u8 __user *)arg) {
			ret = copy_to_user(((u8 __user *)arg), IO_VERSION,
					   sizeof(IO_VERSION));
			if (!ret)
				ret = sizeof(IO_VERSION);

			GTP_INFO("%s", IO_VERSION);
		}
		break;
	case IO_IIC_READ:
		ret = io_iic_read(data, (void __user *)arg);
		break;

	case IO_IIC_WRITE:
		ret = io_iic_write(data);
		break;

	case IO_RESET_GUITAR:
		gtp_reset_guitar(i2c_client_point, 10);
		break;

	case IO_DISABLE_IRQ: {
		gtp_irq_disable();
#ifdef CONFIG_GTP_ESD_PROTECT
		gtp_esd_switch(i2c_client_point, SWITCH_OFF);
#endif
		break;
	}
	case IO_ENABLE_IRQ: {
		gtp_irq_enable();
	}
#ifdef CONFIG_GTP_ESD_PROTECT
		gtp_esd_switch(i2c_client_point, SWITCH_ON);
#endif
		break;

	case IO_PRINT:
		if (data)
			GTP_INFO("%s", (char *)data);
		break;

	case GESTURE_ENABLE_TOTALLY:
		GTP_DEBUG("ENABLE_GESTURE_TOTALLY");
		gesture_data.enabled = 1;
		break;

	case GESTURE_DISABLE_TOTALLY:
		GTP_DEBUG("DISABLE_GESTURE_TOTALLY");
		gesture_data.enabled = 0;
		break;

	case GESTURE_ENABLE_PARTLY:
		SETBIT(gestures_flag, (u8)value);
		gesture_data.enabled = 1;
		GTP_DEBUG(
			"ENABLE_GESTURE_PARTLY, gesture = 0x%02X, gesture_data.enabled = %d",
			value, gesture_data.enabled);
		break;

	case GESTURE_DISABLE_PARTLY:
		CLEARBIT(gestures_flag, (u8)value);
		GTP_DEBUG(
			"DISABLE_GESTURE_PARTLY, gesture = 0x%02X, gesture_data.enabled = %d",
			value, gesture_data.enabled);
		break;

	case GESTURE_DATA_OBTAIN:
		GTP_DEBUG("OBTAIN_GESTURE_DATA");

		mutex_lock(&gesture_data_mutex);
		if (gesture_data.data[1] > GESTURE_MAX_POINT_COUNT)
			gesture_data.data[1] = GESTURE_MAX_POINT_COUNT;

		if (gesture_data.data[3] > 80)
			gesture_data.data[3] = 80;

		ret = copy_to_user(((u8 __user *)arg), &gesture_data.data,
				   4 + gesture_data.data[1] * 4 +
					   gesture_data.data[3]);
		mutex_unlock(&gesture_data_mutex);
		if (ret)
			GTP_ERROR("ERROR when copy gesture data to user.");
		else
			ret = 4 + gesture_data.data[1] * 4 +
			      gesture_data.data[3];

		break;

	case GESTURE_DATA_ERASE:
		GTP_DEBUG("ERASE_GESTURE_DATA");
		gesture_clear_wakeup_data();
		break;

	default:
		GTP_INFO("Unknown cmd.");
		ret = -1;
		break;
	}

	if (data != NULL)
		kfree(data);

	return ret;
}

#ifdef CONFIG_COMPAT
static long gtp_compat_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOMEM;

	return file->f_op->unlocked_ioctl(file, cmd, (unsigned long)arg32);
}
#endif

static int gtp_gesture_open(struct inode *node, struct file *flip)
{
	GTP_DEBUG("gesture node is opened.");
	return 0;
}

static int gtp_gesture_release(struct inode *node, struct file *filp)
{
	GTP_DEBUG("gesture node is closed.");
	return 0;
}

static const struct file_operations gtp_fops = {
	.owner = THIS_MODULE,
	.open = gtp_gesture_open,
	.release = gtp_gesture_release,
	.read = gtp_gesture_data_read,
	.write = gtp_gesture_data_write,
	.unlocked_ioctl = gtp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gtp_compat_ioctl,
#endif
};

s32 gtp_extents_init(void)
{
	struct proc_dir_entry *proc_entry = NULL;

	mutex_init(&gesture_data_mutex);
	memset(gestures_flag, 0, sizeof(gestures_flag));
	memset((u8 *)&gesture_data, 0, sizeof(struct gesture_data));

	proc_entry = proc_create(GESTURE_NODE, 0444, NULL, &gtp_fops);
	if (proc_entry == NULL) {
		GTP_ERROR("Couldn't create proc entry[GESTURE_NODE]!");
		return -1;
	}

	return 0;
}

void gtp_extents_exit(void)
{
	remove_proc_entry(GESTURE_NODE, NULL);
}
#endif /* GTP_GESTURE_WAKEUP */
