/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/wait.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <linux/proc_fs.h> /*proc */
#include <linux/ratelimit.h>
#include <linux/uaccess.h>
#ifdef CONFIG_GTP_REQUEST_FW_UPDATE
#include <linux/firmware.h>
#endif
#include "include/gt1x_tpd_common.h"
#include <asm/ioctl.h>

#ifdef CONFIG_GTP_REQUEST_FW_UPDATE
#define GT1151_FW_SIZE 5000
#define GT1151_PATCH_JUMP_FW "gt1151_patch_jump_"
#define HOTKNOT_AUTH_FW "gt1151_hotknot_auth_"
unsigned char gt1x_patch_jump_fw[GT1151_FW_SIZE];
unsigned char hotknot_auth_fw[GT1151_FW_SIZE];
#endif
#ifdef CONFIG_GTP_GESTURE_WAKEUP

#define GESTURE_NODE "goodix_gesture"
#define GESTURE_MAX_POINT_COUNT 64

#pragma pack(1)
struct {
	u8 ic_msg[6]; /*from the first byte */
	u8 gestures[4];
	u8 data[3 + GESTURE_MAX_POINT_COUNT * 4 +
		80]; /*80 bytes for extra data */
} st_gesture_data;
#pragma pack()

#define SETBIT(longlong, bit) (longlong[bit / 8] |= (1 << bit % 8))
#define CLEARBIT(longlong, bit) (longlong[bit / 8] &= (~(1 << bit % 8)))
#define QUERYBIT(longlong, bit) (!!(longlong[bit / 8] & (1 << bit % 8)))

int gesture_enabled;
enum DOZE_T gesture_doze_status = DOZE_DISABLED;

static u8 gestures_flag[32];
static st_gesture_data gesture_data;
static struct mutex gesture_data_mutex;

static ssize_t gt1x_gesture_data_read(struct file *file, char __user *page,
				      size_t size, loff_t *ppos)
{
	s32 ret = -1;

	GTP_DEBUG("visit %s. ppos:%d",  __func__, (int)*ppos);
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

static ssize_t gt1x_gesture_data_write(struct file *filp,
				       const char __user *buff, size_t len,
				       loff_t *off)
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

	for (i = 0; i < size; i++)
		sum |= longlong[i];

	return !sum;
}

int gesture_enter_doze(void)
{
	int retry = 0;

	GTP_DEBUG_FUNC();
	GTP_DEBUG("entering doze mode...");
	while (retry++ < 5) {
		if (!gt1x_send_cmd(0x08, 0)) {
			gesture_doze_status = DOZE_ENABLED;
			GTP_DEBUG("GTP has been working in doze mode!");
			return 0;
		}
		msleep(20);
	}
	GTP_ERROR("GTP send doze cmd failed.");
	return -1;
}

s32 gesture_event_handler(struct input_dev *dev)
{
	u8 doze_buf[4] = {0};
	s32 ret = -1;
	int len, extra_len;

	if (gesture_doze_status == DOZE_ENABLED) {
		ret = gt1x_i2c_read(GTP_REG_WAKEUP_GESTURE, doze_buf, 4);
		GTP_DEBUG("0x%x = 0x%02X,0x%02X,0x%02X,0x%02X",
			  GTP_REG_WAKEUP_GESTURE, doze_buf[0], doze_buf[1],
			  doze_buf[2], doze_buf[3]);
		if (ret == 0 && doze_buf[0] != 0) {
			if (!QUERYBIT(gestures_flag, doze_buf[0])) {
				GTP_INFO(
					"Sorry, this gesture has been disabled.");
				doze_buf[0] = 0x00;
				gt1x_i2c_write(GTP_REG_WAKEUP_GESTURE, doze_buf,
					       1);
				return 0;
			}

			mutex_lock(&gesture_data_mutex);
			len = doze_buf[1];
			if (len > GESTURE_MAX_POINT_COUNT) {
				GTP_ERROR(
					"Gesture contain too many points!(%d)",
					len);
				len = GESTURE_MAX_POINT_COUNT;
			}
			if (len > 0) {
				ret = gt1x_i2c_read(
					GTP_REG_WAKEUP_GESTURE_DETAIL,
					&gesture_data.data[4], len * 4);
				if (ret < 0) {
					GTP_DEBUG("Read gesture data failed.");
					mutex_unlock(&gesture_data_mutex);
					return 0;
				}
			}
			extra_len = doze_buf[3];
			if (extra_len > 80) {
				GTP_ERROR(
					"Gesture contain too many extra data!(%d)",
					extra_len);
				extra_len = 80;
			}
			if (extra_len > 0) {
				ret = gt1x_i2c_read(
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

			gesture_data.data[0] = doze_buf[0]; /*gesture type*/
			gesture_data.data[1] = len; /*gesture points number*/
			gesture_data.data[2] = doze_buf[2];
			gesture_data.data[3] = extra_len;
			mutex_unlock(&gesture_data_mutex);

			GTP_DEBUG("Gesture: 0x%02X, points: %d", doze_buf[0],
				  doze_buf[1]);

			doze_buf[0] = 0;
			gt1x_i2c_write(GTP_REG_WAKEUP_GESTURE, doze_buf, 1);

			input_report_key(dev, KEY_GESTURE, 1);
			input_sync(dev);
			input_report_key(dev, KEY_GESTURE, 0);
			input_sync(dev);
			return 1;
		}
		return 0;
	}
	return -1;
}

void gesture_clear_wakeup_data(void)
{
	mutex_lock(&gesture_data_mutex);
	memset(gesture_data.data, 0, 4);
	mutex_unlock(&gesture_data_mutex);
}
#endif /*CONFIG_GTP_GESTURE_WAKEUP*/

/*HotKnot module*/
#ifdef CONFIG_GTP_HOTKNOT

#define HOTKNOT_NODE "hotknot"

u8 hotknot_enabled;
u8 hotknot_transfer_mode;

static int hotknot_open(struct inode *node, struct file *flip)
{
	GTP_DEBUG("Hotknot is enable.");
	hotknot_enabled = 1;
	return 0;
}

static int hotknot_release(struct inode *node, struct file *filp)
{
	GTP_DEBUG("Hotknot is disable.");
	hotknot_enabled = 0;
	return 0;
}

static s32 hotknot_enter_transfer_mode(void)
{
	int ret = 0;
	u8 buffer[5] = {0};

	hotknot_transfer_mode = 1;
#ifdef CONFIG_GTP_ESD_PROTECT
	gt1x_esd_switch(SWITCH_OFF);
#endif

	gt1x_irq_disable();
	gt1x_send_cmd(GTP_CMD_HN_TRANSFER, 0);
	msleep(100);
	gt1x_irq_enable();

	ret = gt1x_i2c_read(0x8140, buffer, sizeof(buffer));
	if (ret) {
		hotknot_transfer_mode = 0;
		return ret;
	}

	buffer[4] = 0;
	GTP_DEBUG("enter transfer mode: %s ", buffer);
	if (strcmp(buffer, "GHot")) {
		hotknot_transfer_mode = 0;
		return ERROR_HN_VER;
	}

	return 0;
}

static s32 hotknot_load_hotknot_subsystem(void)
{
	return hotknot_enter_transfer_mode();
}

static s32 hotknot_load_authentication_subsystem(void)
{
	s32 ret = 0;
	u8 buffer[5] = {0};
#ifdef CONFIG_GTP_REQUEST_FW_UPDATE
	const struct firmware *fw_entry;
	char buf_jump[64], buf_auth[64];
#endif
	ret = gt1x_hold_ss51_dsp_no_reset();
	if (ret < 0) {
		GTP_ERROR("Hold ss51 fail!");
		return ERROR;
	}

	if (gt1x_chip_type == CHIP_TYPE_GT1X) {
		GTP_INFO("hotknot load jump code.");

#ifdef CONFIG_GTP_REQUEST_FW_UPDATE
		/* Load gt1x_patch_jump_fw */
		sprintf(buf_jump, "%s%s.img", GT1151_PATCH_JUMP_FW,
			CONFIG_GT1151_FIRMWARE);
		GTP_INFO("Request firmware version: %s\n", buf_jump);
		ret = request_firmware(&fw_entry, buf_jump,
				       &gt1x_i2c_client->dev);
		if (ret) {
			GTP_ERROR("load %s fail\n", buf_jump);
			return ret;
		}
		GTP_DEBUG("firmware size : %zu\n", fw_entry->size);
		memcpy(gt1x_patch_jump_fw, fw_entry->data, fw_entry->size);
		release_firmware(fw_entry);
#endif
		ret = gt1x_load_patch(gt1x_patch_jump_fw, 4096, 0, 1024 * 8);
		if (ret < 0) {
			GTP_ERROR("Load jump code fail!");
			return ret;
		}
		GTP_INFO("hotknot load auth code.");
#ifdef CONFIG_GTP_REQUEST_FW_UPDATE
		sprintf(buf_auth, "%s%s.img", HOTKNOT_AUTH_FW,
			CONFIG_GT1151_FIRMWARE);
		GTP_INFO("Request firmware version: %s\n", buf_auth);
		ret = request_firmware(&fw_entry, buf_auth,
				       &gt1x_i2c_client->dev);
		if (ret) {
			GTP_ERROR("load %s fail\n", buf_auth);
			return ret;
		}
		GTP_DEBUG("firmware size : %zu\n", fw_entry->size);
		memcpy(hotknot_auth_fw, fw_entry->data, fw_entry->size);
		release_firmware(fw_entry);
#endif
		ret = gt1x_load_patch(hotknot_auth_fw, 4096, 4096, 1024 * 8);
		if (ret < 0) {
			GTP_ERROR("Load auth system fail!");
			return ret;
		}
	} else {
		GTP_INFO("hotknot load auth code.");
#ifdef CONFIG_GTP_REQUEST_FW_UPDATE
		ret = request_firmware(&fw_entry, buf_auth,
				       &gt1x_i2c_client->dev);
		if (ret) {
			GTP_ERROR("load %s fail\n", buf_auth);
			return ret;
		}
		GTP_DEBUG("firmware size : %zu\n", fw_entry->size);
		memcpy(hotknot_auth_fw, fw_entry->data, fw_entry->size);
		release_firmware(fw_entry);
#endif
		ret = gt1x_load_patch(hotknot_auth_fw, 4096, 0, 1024 * 6);
		if (ret < 0) {
			GTP_ERROR("load auth system fail!");
			return ret;
		}
	}

	ret = gt1x_startup_patch();
	if (ret < 0) {
		GTP_ERROR("Startup auth system fail!");
		return ret;
	}
	ret = gt1x_i2c_read(GTP_REG_VERSION, buffer, 4);
	if (ret < 0) {
		GTP_ERROR("i2c read error!");
		return ERROR_IIC;
	}
	buffer[4] = 0;
	GTP_INFO("Current System version: %s", buffer);
	return 0;
}

static s32 hotknot_recovery_main_system(void)
{
	gt1x_irq_disable();
	gt1x_reset_guitar();
	gt1x_irq_enable();
#ifdef CONFIG_GTP_ESD_PROTECT
	gt1x_esd_switch(SWITCH_ON);
#endif
	hotknot_transfer_mode = 0;
	return 0;
}

#ifdef CONFIG_HOTKNOT_BLOCK_RW
DECLARE_WAIT_QUEUE_HEAD(bp_waiter);
static u8 got_hotknot_state;
static u8 got_hotknot_extra_state;
static u8 wait_hotknot_state;
static u8 force_wake_flag;
static u8 block_enable;
s32 hotknot_paired_flag;

static s32 hotknot_block_rw(u8 rqst_hotknot_state, s32 wait_hotknot_timeout)
{
	s32 ret = 0;

	wait_hotknot_state |= rqst_hotknot_state;
	GTP_DEBUG(
		"Goodix tool received wait polling state:0x%x,timeout:%d, all wait state:0x%x",
		rqst_hotknot_state, wait_hotknot_timeout, wait_hotknot_state);
	got_hotknot_state &= (~rqst_hotknot_state);

	set_current_state(TASK_INTERRUPTIBLE);
	if (wait_hotknot_timeout <= 0) {
		wait_event_interruptible(bp_waiter,
					 force_wake_flag ||
						 rqst_hotknot_state ==
							 (got_hotknot_state &
							  rqst_hotknot_state));
	} else {
		wait_event_interruptible_timeout(
			bp_waiter,
			force_wake_flag ||
				rqst_hotknot_state == (got_hotknot_state &
						       rqst_hotknot_state),
			wait_hotknot_timeout);
	}

	wait_hotknot_state &= (~rqst_hotknot_state);

	if (rqst_hotknot_state != (got_hotknot_state & rqst_hotknot_state)) {
		GTP_ERROR("Wait 0x%x block polling waiter failed.",
			  rqst_hotknot_state);
		ret = -1;
	}

	force_wake_flag = 0;
	return ret;
}

static void hotknot_wakeup_block(void)
{
	GTP_DEBUG("Manual wakeup all block polling waiter!");
	got_hotknot_state = 0;
	wait_hotknot_state = 0;
	force_wake_flag = 1;
	hotknot_paired_flag = 0;
	wake_up_interruptible(&bp_waiter);
}

s32 hotknot_event_handler(u8 *data)
{
	u8 hn_pxy_state = 0;
	u8 hn_pxy_state_bak = 0;
	static u8 hn_paired_cnt;
	u8 hn_state_buf[10] = {0};
	u8 finger = data[0];
	u8 id = 0;

	if (block_enable && !hotknot_paired_flag && (finger & 0x0F)) {
		id = data[1];
		hn_pxy_state = data[2] & 0x80;
		hn_pxy_state_bak = data[3] & 0x80;
		if ((id == 32) && (hn_pxy_state == 0x80) &&
		    (hn_pxy_state_bak == 0x80)) {
#ifdef HN_DBLCFM_PAIRED
			if (hn_paired_cnt++ < 2)
				return 0;
#endif
			GTP_DEBUG("HotKnot paired!");
			if (wait_hotknot_state & HN_DEVICE_PAIRED) {
				GTP_DEBUG(
					"INT wakeup HN_DEVICE_PAIRED block polling waiter");
				got_hotknot_state |= HN_DEVICE_PAIRED;
				wake_up_interruptible(&bp_waiter);
			}
			block_enable = 0;
			hotknot_paired_flag = 1;
			return 0;
		}
		got_hotknot_state &= (~HN_DEVICE_PAIRED);
		hn_paired_cnt = 0;
	}

	if (hotknot_paired_flag) {
		s32 ret = -1;

		ret = gt1x_i2c_read(GTP_REG_HN_STATE, hn_state_buf, 6);
		if (ret < 0) {
			GTP_ERROR("I2C transfer error. errno:%d\n ", ret);
			return 0;
		}

		got_hotknot_state = 0;

		GTP_DEBUG("wait_hotknot_state:%x", wait_hotknot_state);
		GTP_DEBUG("[0x8800~0x8803]=0x%x,0x%x,0x%x,0x%x",
			  hn_state_buf[0], hn_state_buf[1], hn_state_buf[2],
			  hn_state_buf[3]);

		if (wait_hotknot_state & HN_MASTER_SEND) {
			if ((hn_state_buf[0] == 0x03) ||
			    (hn_state_buf[0] == 0x04) ||
			    (hn_state_buf[0] == 0x07)) {
				GTP_DEBUG(
					"Wakeup HN_MASTER_SEND block polling waiter");
				got_hotknot_state |= HN_MASTER_SEND;
				got_hotknot_extra_state = hn_state_buf[0];
				wake_up_interruptible(&bp_waiter);
			}
		} else if (wait_hotknot_state & HN_SLAVE_RECEIVED) {
			if ((hn_state_buf[1] == 0x03) ||
			    (hn_state_buf[1] == 0x04) ||
			    (hn_state_buf[1] == 0x07)) {
				GTP_DEBUG(
					"Wakeup HN_SLAVE_RECEIVED block polling waiter:0x%x",
					hn_state_buf[1]);
				got_hotknot_state |= HN_SLAVE_RECEIVED;
				got_hotknot_extra_state = hn_state_buf[1];
				wake_up_interruptible(&bp_waiter);
			}
		} else if (wait_hotknot_state & HN_MASTER_DEPARTED) {
			if (hn_state_buf[0] == 0x07) {
				GTP_DEBUG(
					"Wakeup HN_MASTER_DEPARTED block polling waiter");
				got_hotknot_state |= HN_MASTER_DEPARTED;
				wake_up_interruptible(&bp_waiter);
			}
		} else if (wait_hotknot_state & HN_SLAVE_DEPARTED) {
			if (hn_state_buf[1] == 0x07) {
				GTP_DEBUG(
					"Wakeup HN_SLAVE_DEPARTED block polling waiter");
				got_hotknot_state |= HN_SLAVE_DEPARTED;
				wake_up_interruptible(&bp_waiter);
			}
		}
		return 0;
	}

	return -1;
}
#endif /*CONFIG_HOTKNOT_BLOCK_RW*/
#endif /*CONFIG_GTP_HOTKNOT*/

#define GOODIX_MAGIC_NUMBER 'G'
#define NEGLECT_SIZE_MASK (~(_IOC_SIZEMASK << _IOC_SIZESHIFT))

#define GESTURE_ENABLE_TOTALLY _IO(GOODIX_MAGIC_NUMBER, 1) /* 1*/
#define GESTURE_DISABLE_TOTALLY _IO(GOODIX_MAGIC_NUMBER, 2)
#define GESTURE_ENABLE_PARTLY _IO(GOODIX_MAGIC_NUMBER, 3)
#define GESTURE_DISABLE_PARTLY _IO(GOODIX_MAGIC_NUMBER, 4)
/*#define SET_ENABLED_GESTURE         */
/* (_IOW(GOODIX_MAGIC_NUMBER, 5, u8) & NEGLECT_SIZE_MASK)*/
#define GESTURE_DATA_OBTAIN                                                    \
	(_IOR(GOODIX_MAGIC_NUMBER, 6, u8) & NEGLECT_SIZE_MASK)
#define GESTURE_DATA_ERASE _IO(GOODIX_MAGIC_NUMBER, 7)

/*#define HOTKNOT_LOAD_SUBSYSTEM (_IOW(GOODIX_MAGIC_NUMBER, 6, u8) & */
/* NEGLECT_SIZE_MASK)*/
#define HOTKNOT_LOAD_HOTKNOT _IO(GOODIX_MAGIC_NUMBER, 20)
#define HOTKNOT_LOAD_AUTHENTICATION _IO(GOODIX_MAGIC_NUMBER, 21)
#define HOTKNOT_RECOVERY_MAIN _IO(GOODIX_MAGIC_NUMBER, 22)
/*#define HOTKNOT_BLOCK_RW      (_IOW(GOODIX_MAGIC_NUMBER, 6, u8) & */
/* NEGLECT_SIZE_MASK)*/
#define HOTKNOT_DEVICES_PAIRED _IO(GOODIX_MAGIC_NUMBER, 23)
#define HOTKNOT_MASTER_SEND _IO(GOODIX_MAGIC_NUMBER, 24)
#define HOTKNOT_SLAVE_RECEIVE _IO(GOODIX_MAGIC_NUMBER, 25)
/*#define HOTKNOT_DEVICES_COMMUNICATION*/
#define HOTKNOT_MASTER_DEPARTED _IO(GOODIX_MAGIC_NUMBER, 26)
#define HOTKNOT_SLAVE_DEPARTED _IO(GOODIX_MAGIC_NUMBER, 27)
#define HOTKNOT_WAKEUP_BLOCK _IO(GOODIX_MAGIC_NUMBER, 29)

#define IO_IIC_READ (_IOR(GOODIX_MAGIC_NUMBER, 100, u8) & NEGLECT_SIZE_MASK)
#define IO_IIC_WRITE (_IOW(GOODIX_MAGIC_NUMBER, 101, u8) & NEGLECT_SIZE_MASK)
#define IO_RESET_GUITAR _IO(GOODIX_MAGIC_NUMBER, 102)
#define IO_DISABLE_IRQ _IO(GOODIX_MAGIC_NUMBER, 103)
#define IO_ENABLE_IRQ _IO(GOODIX_MAGIC_NUMBER, 104)
#define IO_GET_VERSION (_IOR(GOODIX_MAGIC_NUMBER, 110, u8) & NEGLECT_SIZE_MASK)
#define IO_PRINT (_IOW(GOODIX_MAGIC_NUMBER, 111, u8) & NEGLECT_SIZE_MASK)
#define IO_VERSION "V1.0-20140709"
#ifdef CONFIG_COMPAT
#define COMPAT_GESTURE_ENABLE_TOTALLY _IO(GOODIX_MAGIC_NUMBER, 1) /*1*/
#define COMPAT_GESTURE_DISABLE_TOTALLY _IO(GOODIX_MAGIC_NUMBER, 2)
#define COMPAT_GESTURE_ENABLE_PARTLY _IO(GOODIX_MAGIC_NUMBER, 3)
#define COMPAT_GESTURE_DISABLE_PARTLY _IO(GOODIX_MAGIC_NUMBER, 4)
/*#define SET_ENABLED_GESTURE  (_IOW(GOODIX_MAGIC_NUMBER, 5, u8) & */
/* NEGLECT_SIZE_MASK)*/
#define COMPAT_GESTURE_DATA_OBTAIN                                             \
	(_IOR(GOODIX_MAGIC_NUMBER, 6, u8) & NEGLECT_SIZE_MASK)
#define COMPAT_GESTURE_DATA_ERASE _IO(GOODIX_MAGIC_NUMBER, 7)

/*#define HOTKNOT_LOAD_SUBSYSTEM  (_IOW(GOODIX_MAGIC_NUMBER, 6, u8) & */
/* NEGLECT_SIZE_MASK)*/
#define COMPAT_HOTKNOT_LOAD_HOTKNOT _IO(GOODIX_MAGIC_NUMBER, 20)
#define COMPAT_HOTKNOT_LOAD_AUTHENTICATION _IO(GOODIX_MAGIC_NUMBER, 21)
#define COMPAT_HOTKNOT_RECOVERY_MAIN _IO(GOODIX_MAGIC_NUMBER, 22)
/*#define HOTKNOT_BLOCK_RW   (_IOW(GOODIX_MAGIC_NUMBER, 6, u8) & */
/* NEGLECT_SIZE_MASK)*/
#define COMPAT_HOTKNOT_DEVICES_PAIRED _IO(GOODIX_MAGIC_NUMBER, 23)
#define COMPAT_HOTKNOT_MASTER_SEND _IO(GOODIX_MAGIC_NUMBER, 24)
#define COMPAT_HOTKNOT_SLAVE_RECEIVE _IO(GOODIX_MAGIC_NUMBER, 25)
/*#define HOTKNOT_DEVICES_COMMUNICATION*/
#define COMPAT_HOTKNOT_MASTER_DEPARTED _IO(GOODIX_MAGIC_NUMBER, 26)
#define COMPAT_HOTKNOT_SLAVE_DEPARTED _IO(GOODIX_MAGIC_NUMBER, 27)
#define COMPAT_HOTKNOT_WAKEUP_BLOCK _IO(GOODIX_MAGIC_NUMBER, 29)

#define COMPAT_IO_IIC_READ                                                     \
	(_IOR(GOODIX_MAGIC_NUMBER, 100, u8) & NEGLECT_SIZE_MASK)
#define COMPAT_IO_IIC_WRITE                                                    \
	(_IOW(GOODIX_MAGIC_NUMBER, 101, u8) & NEGLECT_SIZE_MASK)
#define COMPAT_IO_RESET_GUITAR _IO(GOODIX_MAGIC_NUMBER, 102)
#define COMPAT_IO_DISABLE_IRQ _IO(GOODIX_MAGIC_NUMBER, 103)
#define COMPAT_IO_ENABLE_IRQ _IO(GOODIX_MAGIC_NUMBER, 104)
#define COMPAT_IO_GET_VERSION                                                  \
	(_IOR(GOODIX_MAGIC_NUMBER, 110, u8) & NEGLECT_SIZE_MASK)
#define COMPAT_IO_PRINT (_IOW(GOODIX_MAGIC_NUMBER, 111, u8) & NEGLECT_SIZE_MASK)
#endif
#define CMD_HEAD_LENGTH 20
static s32 io_iic_read(u8 *data, int buf_size, void __user *arg)
{
	s32 err = ERROR;
	s32 data_length = 0;
	u16 addr = 0;

	err = copy_from_user(data, arg, CMD_HEAD_LENGTH);
	if (err) {
		GTP_DEBUG("Can't access the memory.");
		return ERROR_MEM;
	}

	addr = data[0] << 8 | data[1];
	data_length = data[2] << 8 | data[3];

	if (data_length + CMD_HEAD_LENGTH > buf_size) {
		GTP_ERROR("incorrect data length, data = %d, buffer = %d\n",
			  data_length, buf_size);
		return ERROR_MEM;
	}

	err = gt1x_i2c_read(addr, &data[CMD_HEAD_LENGTH], data_length);
	if (!err) {
		err = copy_to_user(&((u8 __user *)arg)[CMD_HEAD_LENGTH],
				   &data[CMD_HEAD_LENGTH], data_length);
		if (err) {
			GTP_ERROR(
				"ERROR when copy to user.[addr: %04x], [read length:%d]",
				addr, data_length);
			return ERROR_MEM;
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
	s32 err = ERROR;
	s32 data_length = 0;
	u16 addr = 0;

	if (data == NULL) {
		GTP_ERROR("data is null\n");
		return -1;
	}
	addr = data[0] << 8 | data[1];
	data_length = data[2] << 8 | data[3];

	err = gt1x_i2c_write(addr, &data[CMD_HEAD_LENGTH], data_length);
	if (!err)
		err = CMD_HEAD_LENGTH + data_length;

	GTP_DEBUG("IIC_WRITE.addr:0x%4x, length:%d, ret:%d", addr, data_length,
		  err);
	GTP_DEBUG_ARRAY((&data[CMD_HEAD_LENGTH]), data_length);
	return err;
}

/*@return, 0:operate successfully
 *         > 0: the length of memory size ioctl has accessed,
 *         error otherwise.
 */
static long gt1x_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 value = 0;
	s32 ret = 0; /*the initial value must be 0*/
	u8 *data = NULL;
	int cnt = 30;
	s32 data_length = 0;
	static struct ratelimit_state ratelimit = {
		.lock = __RAW_SPIN_LOCK_UNLOCKED(ratelimit.lock),
		.interval = HZ / 2,
		.burst = 1,
		.begin = 1,
	};

	/* Blocking when firmwaer updating */
	while (cnt-- && update_info.status)
		ssleep(1);

	GTP_DEBUG("IOCTL CMD:%x", cmd);
	/*GTP_DEBUG("command:%d, length:%d, rw:%s", _IOC_NR(cmd),
	 *_IOC_SIZE(cmd),
	 *	(_IOC_DIR(cmd) & _IOC_READ) ? "read" : (_IOC_DIR(cmd) &
	 *_IOC_WRITE) ? "write" : "-");
	 */

	if (_IOC_DIR(cmd)) {
		s32 err = -1;

		data_length = _IOC_SIZE(cmd);
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
		if (gt1x_is_tpd_halt() == 1) {
			if (__ratelimit(&ratelimit))
				GTP_ERROR("touch is suspended.");
			break;
		}
		if (data != NULL)
			ret = io_iic_read(data, data_length,
					  (void __user *)arg);
		else
			GTP_ERROR("Touch read data is NULL.");
		break;

	case IO_IIC_WRITE:
		if (gt1x_is_tpd_halt() == 1) {
			if (__ratelimit(&ratelimit))
				GTP_ERROR("touch is suspended.");
			break;
		}
		if (data != NULL)
			ret = io_iic_write(data);
		else
			GTP_ERROR("Touch write data is NULL.");
		break;

	case IO_RESET_GUITAR:
		gt1x_reset_guitar();
		break;

	case IO_DISABLE_IRQ:
		gt1x_irq_disable();
#ifdef CONFIG_GTP_ESD_PROTECT
		gt1x_esd_switch(SWITCH_OFF);
#endif
		break;

	case IO_ENABLE_IRQ:
		gt1x_irq_enable();
#ifdef CONFIG_GTP_ESD_PROTECT
		gt1x_esd_switch(SWITCH_ON);
#endif
		break;

	/*print a string to syc log messages between application and kernel.*/
	case IO_PRINT:
		if (data)
			GTP_INFO("%s", (char *)data);
		break;

#ifdef CONFIG_GTP_GESTURE_WAKEUP
	case GESTURE_ENABLE_TOTALLY:
		GTP_DEBUG("ENABLE_GESTURE_TOTALLY");
		gesture_enabled =
			(is_all_dead(gestures_flag, sizeof(gestures_flag)) ? 0
									   : 1);
		break;

	case GESTURE_DISABLE_TOTALLY:
		GTP_DEBUG("DISABLE_GESTURE_TOTALLY");
		gesture_enabled = 0;
		break;

	case GESTURE_ENABLE_PARTLY:
		SETBIT(gestures_flag, (u8)value);
		gesture_enabled = 1;
		GTP_DEBUG(
			"ENABLE_GESTURE_PARTLY, gesture = 0x%02X, gesture_enabled = %d",
			value, gesture_enabled);
		break;

	case GESTURE_DISABLE_PARTLY:
		ret = QUERYBIT(gestures_flag, (u8)value);
		if (!ret)
			break;
		CLEARBIT(gestures_flag, (u8)value);
		if (is_all_dead(gestures_flag, sizeof(gestures_flag)))
			gesture_enabled = 0;
		GTP_DEBUG(
			"DISABLE_GESTURE_PARTLY, gesture = 0x%02X, gesture_enabled = %d",
			value, gesture_enabled);
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
		if (ret) {
			GTP_ERROR("ERROR when copy gesture data to user.");
			ret = ERROR_MEM;
		} else {
			ret = 4 + gesture_data.data[1] * 4 +
			      gesture_data.data[3];
		}
		break;

	case GESTURE_DATA_ERASE:
		GTP_DEBUG("ERASE_GESTURE_DATA");
		gesture_clear_wakeup_data();
		break;
#endif /*CONFIG_GTP_GESTURE_WAKEUP*/

#ifdef CONFIG_GTP_HOTKNOT
	case HOTKNOT_LOAD_HOTKNOT:
		ret = hotknot_load_hotknot_subsystem();
		break;

	case HOTKNOT_LOAD_AUTHENTICATION:
		if (gt1x_is_tpd_halt() == 1) {
			GTP_ERROR("touch is suspended.");
			break;
		}
#ifdef CONFIG_GTP_ESD_PROTECT
		gt1x_esd_switch(SWITCH_ON);
#endif
		ret = hotknot_load_authentication_subsystem();
		break;

	case HOTKNOT_RECOVERY_MAIN:
		if (gt1x_is_tpd_halt() == 1) {
			GTP_ERROR("touch is suspended.");
			break;
		}
		ret = hotknot_recovery_main_system();
		break;
#ifdef CONFIG_HOTKNOT_BLOCK_RW
	case HOTKNOT_DEVICES_PAIRED:
		hotknot_paired_flag = 0;
		force_wake_flag = 0;
		block_enable = 1;
		ret = hotknot_block_rw(HN_DEVICE_PAIRED, (s32)value);
		break;

	case HOTKNOT_MASTER_SEND:
		ret = hotknot_block_rw(HN_MASTER_SEND, (s32)value);
		if (!ret)
			ret = got_hotknot_extra_state;
		break;

	case HOTKNOT_SLAVE_RECEIVE:
		ret = hotknot_block_rw(HN_SLAVE_RECEIVED, (s32)value);
		if (!ret)
			ret = got_hotknot_extra_state;
		break;

	case HOTKNOT_MASTER_DEPARTED:
		ret = hotknot_block_rw(HN_MASTER_DEPARTED, (s32)value);
		break;

	case HOTKNOT_SLAVE_DEPARTED:
		ret = hotknot_block_rw(HN_SLAVE_DEPARTED, (s32)value);
		break;

	case HOTKNOT_WAKEUP_BLOCK:
		hotknot_wakeup_block();
		break;
#endif /*CONFIG_HOTKNOT_BLOCK_RW*/
#endif /*CONFIG_GTP_HOTKNOT*/

	default:
		GTP_INFO("Unknown cmd.");
		ret = -1;
		break;
	}

	if (data != NULL)
		kfree(data);
	return ret;
}
#ifdef CONFIG_GTP_HOTKNOT
#ifdef CONFIG_COMPAT
static long gt1x_compat_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	long ret;
	void __user *arg32 = NULL;

	GTP_DEBUG("%s cmd = %x, arg: 0x%lx\n", __func__, cmd, arg);
	arg32 = compat_ptr(arg);
	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	/*GTP_DEBUG("gt1x_compat_ioctl arg: 0x%lx, arg32: 0x%p\n",arg, arg32);*/

	switch (cmd & NEGLECT_SIZE_MASK) {
	case COMPAT_IO_GET_VERSION:
		/*GTP_DEBUG("gt1x_compat_ioctl COMPAT_IO_GET_VERSION\n");*/
		if (arg32 == NULL) {
			GTP_ERROR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_IO_IIC_READ:
		/*GTP_DEBUG("gt1x_compat_ioctl COMPAT_IO_IIC_READ\n");*/
		if (arg32 == NULL) {
			GTP_ERROR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_IO_IIC_WRITE:
		/*GTP_DEBUG("gt1x_compat_ioctl COMPAT_IO_IIC_WRITE\n");*/
		if (arg32 == NULL) {
			GTP_ERROR("invalid argument.");
			return -EINVAL;
		}
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_IO_RESET_GUITAR:
		/*GTP_DEBUG("gt1x_compat_ioctl COMPAT_IO_RESET_GUITAR\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_IO_DISABLE_IRQ:
		/*GTP_DEBUG("gt1x_compat_ioctl COMPAT_IO_DISABLE_IRQ\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_IO_ENABLE_IRQ:
		/*GTP_DEBUG("gt1x_compat_ioctl COMPAT_IO_ENABLE_IRQ\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_IO_PRINT:
		/*GTP_DEBUG("gt1x_compat_ioctl COMPAT_IO_PRINT\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_GESTURE_ENABLE_TOTALLY:
		/*GTP_DEBUG("gt1x_compat_ioctl */
		/* COMPAT_GESTURE_ENABLE_TOTALLY\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_GESTURE_DISABLE_TOTALLY:
		/*GTP_DEBUG("gt1x_compat_ioctl */
		/* COMPAT_GESTURE_DISABLE_TOTALLY\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_GESTURE_ENABLE_PARTLY:
		/*GTP_DEBUG("gt1x_compat_ioctl */
		/* COMPAT_GESTURE_ENABLE_PARTLY\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_GESTURE_DISABLE_PARTLY:
		/*GTP_DEBUG("gt1x_compat_ioctl */
		/* COMPAT_GESTURE_DISABLE_PARTLY\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_GESTURE_DATA_OBTAIN:
		if (arg32 == NULL) {
			GTP_ERROR("invalid argument.");
			return -EINVAL;
		}
		/*GTP_DEBUG("gt1x_compat_ioctl COMPAT_GESTURE_DATA_OBTAIN\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_GESTURE_DATA_ERASE:
		/*GTP_DEBUG("gt1x_compat_ioctl COMPAT_GESTURE_DATA_ERASE\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_HOTKNOT_LOAD_HOTKNOT:
		/*GTP_DEBUG("gt1x_compat_ioctl */
		/* COMPAT_HOTKNOT_LOAD_HOTKNOT\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_HOTKNOT_LOAD_AUTHENTICATION:
		/*GTP_DEBUG("gt1x_compat_ioctl */
		/* COMPAT_HOTKNOT_LOAD_AUTHENTICATION\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_HOTKNOT_RECOVERY_MAIN:
		/*GTP_DEBUG("gt1x_compat_ioctl */
		/* COMPAT_HOTKNOT_RECOVERY_MAIN\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_HOTKNOT_DEVICES_PAIRED:
		/*GTP_DEBUG("gt1x_compat_ioctl */
		/* COMPAT_HOTKNOT_DEVICES_PAIRED\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_HOTKNOT_MASTER_SEND:
		/*GTP_DEBUG("gt1x_compat_ioctl COMPAT_HOTKNOT_MASTER_SEND\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_HOTKNOT_SLAVE_RECEIVE:
		/*GTP_DEBUG("gt1x_compat_ioctl */
		/* COMPAT_HOTKNOT_SLAVE_RECEIVE\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_HOTKNOT_MASTER_DEPARTED:
		/*GTP_DEBUG("gt1x_compat_ioctl */
		/* COMPAT_HOTKNOT_MASTER_DEPARTED\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_HOTKNOT_SLAVE_DEPARTED:
		/*GTP_DEBUG("gt1x_compat_ioctl */
		/* COMPAT_HOTKNOT_SLAVE_DEPARTED\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	case COMPAT_HOTKNOT_WAKEUP_BLOCK:
		/*GTP_DEBUG("gt1x_compat_ioctl */
		/* COMPAT_HOTKNOT_WAKEUP_BLOCK\n");*/
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)arg32);
		break;
	default:
		GTP_INFO("Unknown cmd.");
		ret = -1;
		break;
	}
	return ret;
}
#endif
#endif
static const struct file_operations gt1x_fops = {
	.owner = THIS_MODULE,
#ifdef CONFIG_GTP_GESTURE_WAKEUP
	.read = gt1x_gesture_data_read,
	.write = gt1x_gesture_data_write,
#endif
	.unlocked_ioctl = gt1x_ioctl,
};

#ifdef CONFIG_GTP_HOTKNOT
static const struct file_operations hotknot_fops = {
	.unlocked_ioctl = gt1x_ioctl,
	.open = hotknot_open,
	.release = hotknot_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gt1x_compat_ioctl,
#endif
};

static struct miscdevice hotknot_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = HOTKNOT_NODE,
	.fops = &hotknot_fops,
};
#endif

s32 gt1x_init_node(void)
{
#ifdef CONFIG_GTP_GESTURE_WAKEUP
	struct proc_dir_entry *proc_entry = NULL;

	mutex_init(&gesture_data_mutex);
	memset(gestures_flag, 0, sizeof(gestures_flag));
	memset((u8 *)&gesture_data, 0, sizeof(st_gesture_data));

	proc_entry = proc_create(GESTURE_NODE, 0644, NULL, &gt1x_fops);
	if (proc_entry == NULL) {
		GTP_ERROR("Couldn't create proc entry[GESTURE_NODE]!");
		return -1;
	}
	GTP_INFO("Create proc entry[GESTURE_NODE] success!");
#endif

#ifdef CONFIG_GTP_HOTKNOT
	if (misc_register(&hotknot_misc_device)) {
		GTP_ERROR("Couldn't create [HOTKNOT_NODE] device!");
		return -1;
	}
	GTP_INFO("Create [HOTKNOT_NODE] device success!");
#endif
	return 0;
}

void gt1x_deinit_node(void)
{
#ifdef CONFIG_GTP_GESTURE_WAKEUP
	remove_proc_entry(GESTURE_NODE, NULL);
#endif

#ifdef CONFIG_GTP_HOTKNOT
	misc_deregister(&hotknot_misc_device);
#endif
}
