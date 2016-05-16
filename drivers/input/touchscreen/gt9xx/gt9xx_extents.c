/* drivers/input/touchscreen/gtp_extents.c
 *
 * 2010 - 2014 Goodix Technology.
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
 * Version: 1.0
 * Revision Record:
 *	  V1.0:  first release. 2014/10/15.
 *
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/input.h>

#include <asm/uaccess.h>
#include <linux/proc_fs.h>	/*proc */

#include <asm/ioctl.h>
#include "gt9xx.h"

#if GTP_GESTURE_WAKEUP

#define GESTURE_NODE "goodix_gesture"
#define GESTURE_MAX_POINT_COUNT	64

#define GTP_REG_WAKEUP_GESTURE	 		0x814c
#define GTP_REG_WAKEUP_GESTURE_DETAIL	0xC0EA


#pragma pack(1)
typedef struct {
	u8 ic_msg[6];		/*from the first byte */
	u8 gestures[4];
	u8 data[3 + GESTURE_MAX_POINT_COUNT * 4 + 80];	/*80 bytes for extra data */
} st_gesture_data;
#pragma pack()

#define SETBIT(longlong, bit)   (longlong[bit/8] |=  (1 << bit%8))
#define CLEARBIT(longlong, bit) (longlong[bit/8] &= (~(1 << bit%8)))
#define QUERYBIT(longlong, bit) (!!(longlong[bit/8] & (1 << bit%8)))

#define GTP_GESTURE_TPYE_STR  "RLUDKcemosvwz"
#define GTP_GLOVE_SUPPORT_ONOFF  'Y'
#define GTP_GESTURE_SUPPORT_ONOFF   'Y'
#define GTP_PROC_DRIVER_VERSION		  "GTP_V1.0_20140327"


int gesture_enabled = 0;
int glove_enabled = 0;
DOZE_T gesture_doze_status = DOZE_DISABLED;

static u8 gestures_flag[32];
static st_gesture_data gesture_data;
static struct mutex gesture_data_mutex;

extern struct i2c_client *i2c_connect_client;


char gtp_gesture_coordinate[60] = {0X8140 >> 8, 0X8140 & 0xFF};
char gtp_gesture_value = 0;
char gtp_gesture_onoff = '0';
char gtp_glove_onoff = '0';
const char gtp_gesture_type[] = GTP_GESTURE_TPYE_STR;
static const char gtp_glove_support_flag = GTP_GLOVE_SUPPORT_ONOFF;
static const char gtp_gesture_support_flag = GTP_GESTURE_SUPPORT_ONOFF;
static const char gtp_version[] = GTP_PROC_DRIVER_VERSION;
static  char gtp_glove_support_flag_changed;
static  char gtp_gesture_support_flag_changed;

static u8 glove_cfg[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH] = {
GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff, \
0x45, 0x38, 0x04, 0x80, 0x07, 0x0A, 0x34, 0x00, 0x01, 0x88, \
0x32, 0x06, 0x46, 0x3C, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, \
0x01, 0x00, 0x08, 0x19, 0x1B, 0x1E, 0x14, 0x0E, 0x0E, 0x0F, \
0x0B, 0x00, 0xBB, 0x32, 0x00, 0x00, 0x00, 0x04, 0x46, 0x19, \
0x05, 0x0A, 0x05, 0x03, 0x1E, 0x00, 0x40, 0x00, 0x00, 0x01, \
0x00, 0x0A, 0x19, 0x54, 0xC5, 0x02, 0x07, 0x00, 0x00, 0x04, \
0x80, 0x0B, 0x00, 0x6C, 0x0D, 0x00, 0x5F, 0x0F, 0x00, 0x4D, \
0x13, 0x00, 0x45, 0x16, 0x00, 0x45, 0x00, 0x00, 0x00, 0x00, \
0x85, 0x60, 0x35, 0xFF, 0xFF, 0x19, 0x00, 0x52, 0x01, 0x03, \
0x00, 0x00, 0x00, 0x00, 0xFF, 0x7F, 0x02, 0x00, 0x00, 0xD4, \
0x30, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x28, 0x05, \
0x23, 0x00, 0x1C, 0x1A, 0x18, 0x16, 0x14, 0x12, 0x10, 0x0E, \
0x0C, 0x0A, 0x08, 0x06, 0x04, 0x02, 0x00, 0xFF, 0xFF, 0xFF, \
0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
0x00, 0x00, 0x24, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, \
0x10, 0x27, 0x20, 0x4E, 0x00, 0x0F, 0x14, 0x03, 0x07, 0x00, \
0x00, 0x28, 0x00, 0x0B, 0x0C, 0x00, 0x00, 0x00, 0x03, 0x00, \
0x06, 0x0A, 0x00, 0x01, 0x00, 0x00, 0x01, 0x24, 0x60, 0x00, \
0x00, 0x6B, 0x80, 0x00, 0x01, 0x00, 0xAF, 0x50, 0x3C, 0x28, \
0xB8, 0x0B, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x01\
};
extern s8 gtp_enter_doze(struct goodix_ts_data *ts);
extern void gtp_esd_switch(struct i2c_client *client, s32 on);
extern void gtp_reset_guitar(struct i2c_client *client, s32 ms);
extern void gtp_irq_disable(struct goodix_ts_data *ts);
extern void gtp_irq_enable(struct goodix_ts_data *ts);
extern int get_gesture_onoff_status(void);
extern s32 gtp_send_cfg(struct i2c_client *client);
extern s32 gtp_i2c_write(struct i2c_client *client, u8 *buf, s32 len);


s32 gtp_send_glove_cfg(struct i2c_client *client);

void gesture_clear_wakeup_data(void);

int get_gesture_onoff_status(void)
{
	return gesture_enabled;
}
EXPORT_SYMBOL(get_gesture_onoff_status);

static s32 i2c_write_bytes(u16 addr, u8 *buf, s32 len)
{
	s32 ret = 0;
	s32 retry = 0;
	struct i2c_msg msg;
	u8 *i2c_buf = NULL;

	i2c_buf = (u8 *)kmalloc(GTP_ADDR_LENGTH + len, GFP_KERNEL);
	if (i2c_buf == NULL) {
		return -ENOMEM;
	}

	i2c_buf[0] = (u8)(addr >> 8);
	i2c_buf[1] = (u8)(addr & 0xFF);
	memcpy(i2c_buf + 2, buf, len);

	msg.flags = !I2C_M_RD;
	msg.addr = i2c_connect_client->addr;
	msg.len  = GTP_ADDR_LENGTH + len;
	msg.buf  = i2c_buf;

	while (retry++ < 5) {
		ret = i2c_transfer(i2c_connect_client->adapter, &msg, 1);
		if (ret == 1) {
				break;
		}
		GTP_ERROR("I2C Write: 0x%04X, %d bytes failed, errcode: %d! [%d].", addr, len, ret, retry);
	}
	if (i2c_buf != NULL) {
		kfree(i2c_buf);
	}

	if (retry >= 5) {
		GTP_ERROR("I2c transfer retry timeout.");
		return -EPERM;
	} else {
		return 1;
	}
}

static s32 i2c_read_bytes(u16 addr, u8 *buf, s32 len)
{
	s32 ret = 0;
	s32 retry = 0;
	u8 i2c_buf[GTP_ADDR_LENGTH];
	struct i2c_msg msg[2] = {
		{
			.flags = !I2C_M_RD,
			.addr  = i2c_connect_client->addr,
			.buf   = i2c_buf,
			.len   = GTP_ADDR_LENGTH
		},
		{
			.flags = I2C_M_RD,
			.addr  = i2c_connect_client->addr,
			.buf   = buf,
			.len   = len,
		}
	};

	i2c_buf[0] = (u8)(addr >> 8);
	i2c_buf[1] = (u8)(addr & 0xFF);

	while (retry++ < 5) {
		ret = i2c_transfer(i2c_connect_client->adapter, msg, 2);
		if (ret == 2) {
			break;
		}
	}

	if (retry >= 5) {
		GTP_ERROR("I2c retry timeout, I2C read 0x%04X %d bytes failed!", addr, len);
		return -EPERM;
	} else {
		return 0;
	}
}

static ssize_t gtp_gesture_data_read(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	s32 ret = -1;
	GTP_DEBUG("visit gtp_gesture_data_read. ppos:%d", (int)*ppos);
	if (*ppos) {
		return 0;
	}

	ret = simple_read_from_buffer(page, size, ppos, &gesture_data, sizeof(gesture_data));

	GTP_DEBUG("Got the gesture data.");
	return ret;
}

static ssize_t gtp_gesture_data_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
	s32 ret = 0;

	GTP_DEBUG_FUNC();

	ret = copy_from_user(&gesture_enabled, buff, 1);
	if (ret) {
		GTP_ERROR("copy_from_user failed.");
		return -EPERM;
	}

	GTP_DEBUG("gesture enabled:%x, ret:%d", gesture_enabled, ret);

	return len;
}

static u8 is_all_dead(u8 *longlong, s32 size)
{
	int i = 0;
	u8 sum = 0;

	for (i = 0; i < size; i++) {
		sum |= longlong[i];
	}

	return !sum;
}

s8 gtp_enter_doze(struct goodix_ts_data *ts)
{
	s8 ret = -1;
	s8 retry = 0;
	u8 i2c_control_buf[1] = {8};

	GTP_DEBUG_FUNC();

	GTP_DEBUG("Entering doze mode.\n");

	while (retry++ < 5) {
		ret = i2c_write_bytes(0x8046, i2c_control_buf, 1);
		if (ret < 0) {
			GTP_DEBUG("failed to set doze flag into 0x8046, %d", retry);
			continue;
		}

		ret = i2c_write_bytes(0x8040, i2c_control_buf, 1);
		if (ret > 0) {
			gesture_doze_status = DOZE_ENABLED;
			GTP_INFO("Gesture mode enabled.");
			return ret;
		}
		msleep(10);
	}
	GTP_ERROR("GTP send doze cmd failed.");
	return ret;
}


s32 gesture_event_handler(struct goodix_ts_data * ts)
{
	u8 doze_buf[4] = {0};
	unsigned int key_code;
	s32 ret = -1;
	int len, extra_len;

	if (gesture_enabled == 1) {
		if (DOZE_ENABLED == gesture_doze_status) {
			ret = i2c_read_bytes(GTP_REG_WAKEUP_GESTURE, doze_buf, 4);
			GTP_DEBUG("0x%x = 0x%02X, 0x%02X, 0x%02X, 0x%02X", GTP_REG_WAKEUP_GESTURE, doze_buf[0], doze_buf[1], doze_buf[2], doze_buf[3]);

			if (ret == 0 && doze_buf[0] != 0) {
				if (!QUERYBIT(gestures_flag, doze_buf[0])) {
					GTP_INFO("Sorry, this gesture has been disabled.");
					doze_buf[0] = 0x00;
					i2c_write_bytes(GTP_REG_WAKEUP_GESTURE, doze_buf, 1);
					gtp_enter_doze(ts);
					return 0;
				}

				mutex_lock(&gesture_data_mutex);
				len = doze_buf[1];
				if (len > GESTURE_MAX_POINT_COUNT) {
					GTP_ERROR("Gesture contain too many points!(%d)", len);
					len = GESTURE_MAX_POINT_COUNT;
				}
				if (len > 0) {
					ret = i2c_read_bytes(GTP_REG_WAKEUP_GESTURE_DETAIL, &gesture_data.data[4], len * 4);
					if (ret < 0) {
						GTP_DEBUG("Read gesture data failed.");
						mutex_unlock(&gesture_data_mutex);
						return 0;
					}
				}

				extra_len = doze_buf[3];
				if (extra_len > 80) {
					GTP_ERROR("Gesture contain too many extra data!(%d)", extra_len);
					extra_len = 80;
				}
				if (extra_len > 0) {
					ret = i2c_read_bytes(GTP_REG_WAKEUP_GESTURE + 4, &gesture_data.data[4 + len * 4], extra_len);
					if (ret < 0) {
						GTP_DEBUG("Read extra gesture data failed.");
						mutex_unlock(&gesture_data_mutex);
						return 0;
					}
				}

				gesture_data.data[0] = doze_buf[0];
				gesture_data.data[1] = len;
				gesture_data.data[2] = doze_buf[2];
				gesture_data.data[3] = extra_len;
				mutex_unlock(&gesture_data_mutex);

				key_code = doze_buf[0] < 16 ?  KEY_F3 : KEY_F2;
				GTP_DEBUG("Gesture: 0x%02X, points: %d\n", doze_buf[0], doze_buf[1]);

				doze_buf[0] = 0;
				i2c_write_bytes(GTP_REG_WAKEUP_GESTURE, doze_buf, 1);


				switch (gesture_data.data[0]) {
				case 0xCC:
					gtp_gesture_value = 'K';
					break;
				case 0xAA:
					gtp_gesture_value = 'R';
					break;
				case 0xBB:
					gtp_gesture_value = 'L';
					break;
				case 0xAB:
					gtp_gesture_value = 'D';
					break;
				case 0xBA:
					gtp_gesture_value = 'U';
					break;
				case 0x63:
				case 0x65:
				case 0x6D:
				case 0x6F:
				case 0x73:
				case 0x77:
				case 0x7A:
					gtp_gesture_value = gesture_data.data[0];
					break;
				default:
					break;
				}


				input_report_key(ts->input_dev, KEY_GESTURE, 1);
				input_sync(ts->input_dev);
				msleep(100);
				input_report_key(ts->input_dev, KEY_GESTURE, 0);
				input_sync(ts->input_dev);

				GTP_DEBUG("in really gesture, %x", gtp_gesture_value);
				doze_buf[0] = 0x00;
				i2c_write_bytes(GTP_REG_WAKEUP_GESTURE, doze_buf, 1);
				gtp_enter_doze(ts);

				return 1;
			} else {
				GTP_DEBUG("not really gesture");
				doze_buf[0] = 0x00;
				i2c_write_bytes(GTP_REG_WAKEUP_GESTURE, doze_buf, 1);
				gtp_enter_doze(ts);
			}
			return 0;
		}
	}
	return -EPERM;
}

void gesture_clear_wakeup_data(void)
{
	mutex_lock(&gesture_data_mutex);
	memset(gesture_data.data, 0, 4);
	mutex_unlock(&gesture_data_mutex);
}


#define GOODIX_MAGIC_NUMBER		'G'
#define NEGLECT_SIZE_MASK		   (~(_IOC_SIZEMASK << _IOC_SIZESHIFT))

#define GESTURE_ENABLE_TOTALLY	  _IO(GOODIX_MAGIC_NUMBER, 1)
#define GESTURE_DISABLE_TOTALLY	 _IO(GOODIX_MAGIC_NUMBER, 2)
#define GESTURE_ENABLE_PARTLY	   _IO(GOODIX_MAGIC_NUMBER, 3)
#define GESTURE_DISABLE_PARTLY	  _IO(GOODIX_MAGIC_NUMBER, 4)

#define GESTURE_DATA_OBTAIN		 (_IOR(GOODIX_MAGIC_NUMBER, 6, u8) & NEGLECT_SIZE_MASK)
#define GESTURE_DATA_ERASE		  _IO(GOODIX_MAGIC_NUMBER, 7)

#define IO_IIC_READ				  (_IOR(GOODIX_MAGIC_NUMBER, 100, u8) & NEGLECT_SIZE_MASK)
#define IO_IIC_WRITE				 (_IOW(GOODIX_MAGIC_NUMBER, 101, u8) & NEGLECT_SIZE_MASK)
#define IO_RESET_GUITAR			  _IO(GOODIX_MAGIC_NUMBER, 102)
#define IO_DISABLE_IRQ			   _IO(GOODIX_MAGIC_NUMBER, 103)
#define IO_ENABLE_IRQ				_IO(GOODIX_MAGIC_NUMBER, 104)
#define IO_GET_VERISON			   (_IOR(GOODIX_MAGIC_NUMBER, 110, u8) & NEGLECT_SIZE_MASK)
#define IO_PRINT					 (_IOW(GOODIX_MAGIC_NUMBER, 111, u8) & NEGLECT_SIZE_MASK)
#define IO_VERSION				   "V1.0-20141015"

#define CMD_HEAD_LENGTH			 20

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

	err = i2c_read_bytes(addr, &data[CMD_HEAD_LENGTH], data_length);
	if (!err) {
		err = copy_to_user(&((u8 __user *) arg)[CMD_HEAD_LENGTH], &data[CMD_HEAD_LENGTH], data_length);
		if (err) {
			GTP_ERROR("ERROR when copy to user.[addr: %04x], [read length:%d]", addr, data_length);
			return err;
		}
		err = CMD_HEAD_LENGTH + data_length;
	}
	GTP_DEBUG("IIC_READ.addr:0x%4x, length:%d, ret:%d", addr, data_length, err);
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

	err = i2c_write_bytes(addr, &data[CMD_HEAD_LENGTH], data_length);
	if (!err) {
		err = CMD_HEAD_LENGTH + data_length;
	}

	GTP_DEBUG("IIC_WRITE.addr:0x%4x, length:%d, ret:%d", addr, data_length, err);
	GTP_DEBUG_ARRAY((&data[CMD_HEAD_LENGTH]), data_length);
	return err;
}




static long gtp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 value = 0;
	s32 ret = 0;
	u8 *data = NULL;

	GTP_DEBUG("IOCTL CMD:%x", cmd);
	GTP_DEBUG("command:%d, length:%d, rw:%s", _IOC_NR(cmd), _IOC_SIZE(cmd), (_IOC_DIR(cmd) & _IOC_READ) ? "read" : (_IOC_DIR(cmd) & _IOC_WRITE) ? "write" : "-");

	if (_IOC_DIR(cmd)) {
		s32 err = -1;
		s32 data_length = _IOC_SIZE(cmd);
		data = (u8 *) kzalloc(data_length, GFP_KERNEL);
		memset(data, 0, data_length);

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			err = copy_from_user(data, (void __user *)arg, data_length);
			if (err) {
				GTP_DEBUG("Can't access the memory.");
				kfree(data);
				return -EPERM;
			}
		}
	} else {
		value = (u32) arg;
	}

	switch (cmd & NEGLECT_SIZE_MASK) {
	case IO_GET_VERISON:
		if ((u8 __user *) arg) {
			ret = copy_to_user(((u8 __user *) arg), IO_VERSION, sizeof(IO_VERSION));
			if (!ret) {
				ret = sizeof(IO_VERSION);
			}
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
		gtp_reset_guitar(i2c_connect_client, 10);
		break;

	case IO_DISABLE_IRQ: {
		struct goodix_ts_data *ts = i2c_get_clientdata(i2c_connect_client);
		gtp_irq_disable(ts);
#if GTP_ESD_PROTECT
		gtp_esd_switch(i2c_connect_client, SWITCH_OFF);
#endif
		break;
		}
	case IO_ENABLE_IRQ: {
			struct goodix_ts_data *ts = i2c_get_clientdata(i2c_connect_client);
			gtp_irq_enable(ts);
		}
#if GTP_ESD_PROTECT
		gtp_esd_switch(i2c_connect_client , SWITCH_ON);
#endif
		break;


	case IO_PRINT:
		if (data)
			GTP_INFO("%s", (char *)data);
		break;

#if GTP_GESTURE_WAKEUP
	case GESTURE_ENABLE_TOTALLY:
		GTP_DEBUG("ENABLE_GESTURE_TOTALLY");
		gesture_enabled = (is_all_dead(gestures_flag, sizeof(gestures_flag)) ? 0 : 1);
		break;

	case GESTURE_DISABLE_TOTALLY:
		GTP_DEBUG("DISABLE_GESTURE_TOTALLY");
		gesture_enabled = 0;
		break;

	case GESTURE_ENABLE_PARTLY:
		SETBIT(gestures_flag, (u8) value);
		gesture_enabled = 1;
		GTP_DEBUG("ENABLE_GESTURE_PARTLY, gesture = 0x%02X, gesture_enabled = %d", value, gesture_enabled);
		break;

	case GESTURE_DISABLE_PARTLY:
		ret = QUERYBIT(gestures_flag, (u8) value);
		if (!ret) {
			break;
		}
		CLEARBIT(gestures_flag, (u8) value);
		if (is_all_dead(gestures_flag, sizeof(gestures_flag))) {
			gesture_enabled = 0;
		}
		GTP_DEBUG("DISABLE_GESTURE_PARTLY, gesture = 0x%02X, gesture_enabled = %d", value, gesture_enabled);
		break;

	case GESTURE_DATA_OBTAIN:
		GTP_DEBUG("OBTAIN_GESTURE_DATA");

		mutex_lock(&gesture_data_mutex);
		if (gesture_data.data[1] > GESTURE_MAX_POINT_COUNT) {
			gesture_data.data[1] = GESTURE_MAX_POINT_COUNT;
		}
		if (gesture_data.data[3] > 80) {
			gesture_data.data[3] = 80;
		}
		ret = copy_to_user(((u8 __user *) arg), &gesture_data.data, 4 + gesture_data.data[1] * 4 + gesture_data.data[3]);
		mutex_unlock(&gesture_data_mutex);
		if (ret) {
			GTP_ERROR("ERROR when copy gesture data to user.");
		} else {
			ret = 4 + gesture_data.data[1] * 4 + gesture_data.data[3];
		}
		break;

	case GESTURE_DATA_ERASE:
		GTP_DEBUG("ERASE_GESTURE_DATA");
		gesture_clear_wakeup_data();
		break;
#endif

	default:
		GTP_INFO("Unknown cmd.");
		ret = -1;
		break;
	}

	if (data != NULL) {
		kfree(data);
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long gtp_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl) {
		return -ENOMEM;
	}

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
#if GTP_GESTURE_WAKEUP
	.read = gtp_gesture_data_read,
	.write = gtp_gesture_data_write,
#endif
	.unlocked_ioctl = gtp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gtp_compat_ioctl,
#endif
};


s32 gtp_send_glove_cfg(struct i2c_client *client)
{
	s32 ret = 2;
	s32 retry = 0;

	GTP_INFO("send glove config.");
	for (retry = 0; retry < 5; retry++) {
		ret = gtp_i2c_write(client, glove_cfg , GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);
		if (ret > 0)
			break;
	}

	return ret;
}


static ssize_t proc_gesture_data_read(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	int num = 0;
	if (*ppos)
		return 0;
	GTP_INFO("proc_gesture_data_read=%d\n", gtp_gesture_value);
	num =  sprintf(page, "%c\n", gtp_gesture_value);
	*ppos += num;
	return num;
}

static ssize_t proc_gesture_data_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
	int num = 0;
	if (*off)
		return 0;
	num =  sscanf(buff, "%c", &gtp_gesture_value);
	*off += num;
	return len;
}

static ssize_t proc_gesture_type_read(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	int num;
	if (*ppos) {
		printk("%s\n", __func__);
		return 0;
	}
		num = sprintf(page, "%s\n", gtp_gesture_type);
	*ppos += num;
	return num;

}

static ssize_t proc_gesture_type_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{

	return -EPERM;
}

static ssize_t proc_gesture_onoff_read(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	int num;
	GTP_DEBUG("in proc_gesture_onoff_read ");
	if (*ppos)
		return 0;
	if (0 == gtp_gesture_support_flag_changed)
		num = sprintf(page, "%c\n", gtp_gesture_support_flag);
	else
		num = sprintf(page, "%c\n", gtp_gesture_onoff);
	GTP_DEBUG("gtp_gesture_onoff:%c", gtp_gesture_onoff);
	*ppos += num;
	return num;

}

static ssize_t proc_gesture_onoff_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
	int num = 0;
	GTP_DEBUG("in proc_gesture_onoff_write ");
	GTP_DEBUG("len:%zu, off:%lld", len, *off);
	if (*off)
	return 0;

	num = sscanf(buff, "%c", &gtp_gesture_onoff);
	if (gtp_gesture_onoff == '1') {
		gesture_enabled = 1;
	} else {
		gesture_enabled = 0;
	}
	gtp_gesture_support_flag_changed = 1;
	*off += num;
	GTP_DEBUG("gtp_gesture_onoff:%c", gtp_gesture_onoff);
	return len;
}

static ssize_t proc_glove_onoff_read(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	int num;
	if (*ppos)
		return 0;
	if (0 == gtp_glove_support_flag_changed)
		num = sprintf(page, "%c\n", gtp_glove_support_flag);
	else
		num = sprintf(page, "%c\n", gtp_glove_onoff);
	*ppos += num;
	return num;

}

static ssize_t proc_glove_onoff_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
	int num = 0;
	if (*off)
	return 0;
	num = sscanf(buff, "%c", &gtp_glove_onoff);

	if (gtp_glove_onoff == '1') {
		gtp_glove_support_flag_changed = 1;
		gtp_send_glove_cfg(i2c_connect_client);
		glove_enabled = 1;
		msleep(200);
	} else {
		gtp_send_cfg(i2c_connect_client);
		glove_enabled = 0;
		msleep(200);
	}

	*off += num;
	return len;
}

static ssize_t proc_version_read(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	int num;
	if (*ppos)
		return 0;
	num = copy_to_user(page, gtp_version, strlen(gtp_version));
	*ppos += num;
	return num;
}

static ssize_t proc_version_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
	return len;
}


static ssize_t proc_gesture_coordinate_read(struct file *file, char __user *page, size_t size, loff_t *ppos)
{

	int num = 0;
	if (*ppos)
		return 0;
	num = copy_to_user(page, gtp_gesture_coordinate, 60);
	printk("GTP gtp_ges 0x%02x\n", gtp_gesture_coordinate[13]);
	memset(gtp_gesture_coordinate, 0, 60);
	*ppos += num;
	return num;
}

static ssize_t proc_gesture_coordinate_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
	return len;
}

static const struct file_operations gt_gesture_var_proc_fops = {
	.write = proc_gesture_data_write,
	.read = proc_gesture_data_read,
};

static const struct file_operations gt_gesture_type_proc_fops = {
	.write = proc_gesture_type_write,
	.read = proc_gesture_type_read,
};

static const struct file_operations gt_gesture_onoff_proc_fops = {
	.write = proc_gesture_onoff_write,
	.read = proc_gesture_onoff_read,
};

static const struct file_operations gt_glove_onoff_proc_fops = {
	.write = proc_glove_onoff_write,
	.read = proc_glove_onoff_read,
};

static const struct file_operations gt_version_proc_fops = {
	.write = proc_version_write,
	.read = proc_version_read,
};

static const struct file_operations gt_coordinate_proc_fops = {
	.write = proc_gesture_coordinate_write,
	.read = proc_gesture_coordinate_read,
};


void Ctp_Gesture_Fucntion_Proc_File(void)
{
	struct proc_dir_entry *ctp_device_proc = NULL;
	struct proc_dir_entry *ctp_gesture_var_proc = NULL;
	struct proc_dir_entry *ctp_gesture_type_proc = NULL;
	struct proc_dir_entry *ctp_gesture_onoff_proc = NULL;
	struct proc_dir_entry *ctp_glove_onoff_proc = NULL;
	struct proc_dir_entry *ctp_version_proc = NULL;
	struct proc_dir_entry *ctp_coordinate_proc = NULL;
#define CTP_GESTURE_FUNCTION_AUTHORITY_PROC 0777

	ctp_device_proc = proc_mkdir("touchscreen_feature", NULL);

	ctp_gesture_var_proc = proc_create("gesture_data", 0444, ctp_device_proc, &gt_gesture_var_proc_fops);
	if (ctp_gesture_var_proc == NULL)
		GTP_DEBUG("ctp_gesture_var_proc create failed\n");

	ctp_gesture_type_proc = proc_create("gesture_type", 0444, ctp_device_proc, &gt_gesture_type_proc_fops);
	if (ctp_gesture_type_proc == NULL)
		GTP_DEBUG("ctp_gesture_type_proc create failed\n");

	ctp_gesture_onoff_proc = proc_create("gesture_onoff", 0666, ctp_device_proc, &gt_gesture_onoff_proc_fops);
	if (ctp_gesture_onoff_proc == NULL)
		GTP_DEBUG("ctp_gesture_onoff_proc create failed\n");

	ctp_glove_onoff_proc = proc_create("glove_onoff", 0666, ctp_device_proc, &gt_glove_onoff_proc_fops);
	if (ctp_glove_onoff_proc == NULL)
		GTP_DEBUG("ctp_gesture_onoff_proc create failed\n");

	ctp_version_proc = proc_create("version", 0444, ctp_device_proc, &gt_version_proc_fops);
	if (ctp_version_proc == NULL)
		GTP_DEBUG("create_proc_entry version failed\n");

	ctp_coordinate_proc = proc_create("gesture_coordinate", 0777, ctp_device_proc, &gt_coordinate_proc_fops);
	if (ctp_coordinate_proc == NULL)
		GTP_DEBUG("create_proc_entry version failed\n");

}

s32 gtp_init_node(void)
{
 #if GTP_GESTURE_WAKEUP

	mutex_init(&gesture_data_mutex);
	memset(gestures_flag, 0xff, sizeof(gestures_flag));
	memset((u8 *) &gesture_data, 0, sizeof(st_gesture_data));

	Ctp_Gesture_Fucntion_Proc_File();
#endif

	return 0;
}

void gtp_deinit_node(void)
{
#if GTP_GESTURE_WAKEUP

#endif

}

#endif

