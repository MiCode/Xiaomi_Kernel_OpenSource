/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <mach/sdio_al.h>

#define INPUT_SPEED			4800
#define OUTPUT_SPEED			4800
#define SDIO_TTY_MODULE_NAME		"sdio_tty"
#define SDIO_TTY_MAX_PACKET_SIZE	4096
#define MAX_SDIO_TTY_DRV		1
#define MAX_SDIO_TTY_DEVS		2
#define MAX_SDIO_TTY_DEV_NAME_SIZE	25

/* Configurations per channel device */
/* CSVT */
#define SDIO_TTY_CSVT_DEV		"sdio_tty_csvt_0"
#define SDIO_TTY_CSVT_TEST_DEV		"sdio_tty_csvt_test_0"
#define SDIO_TTY_CH_CSVT		"SDIO_CSVT"

enum sdio_tty_state {
	TTY_INITIAL = 0,
	TTY_REGISTERED = 1,
	TTY_OPENED = 2,
	TTY_CLOSED = 3,
};

enum sdio_tty_devices {
	SDIO_CSVT,
	SDIO_CSVT_TEST_APP,
};

static const struct platform_device_id sdio_tty_id_table[] = {
	{ "SDIO_CSVT",		SDIO_CSVT },
	{ "SDIO_CSVT_TEST_APP",	SDIO_CSVT_TEST_APP },
	{ },
};
MODULE_DEVICE_TABLE(platform, sdio_tty_id_table);

struct sdio_tty {
	struct sdio_channel *ch;
	char *sdio_ch_name;
	char tty_dev_name[MAX_SDIO_TTY_DEV_NAME_SIZE];
	int device_id;
	struct workqueue_struct *workq;
	struct work_struct work_read;
	wait_queue_head_t   waitq;
	struct tty_driver *tty_drv;
	struct tty_struct *tty_str;
	int debug_msg_on;
	char *read_buf;
	enum sdio_tty_state sdio_tty_state;
	int is_sdio_open;
	int tty_open_count;
	int total_rx;
	int total_tx;
};

static struct sdio_tty *sdio_tty[MAX_SDIO_TTY_DEVS];

#ifdef CONFIG_DEBUG_FS
struct dentry *sdio_tty_debug_root;
struct dentry *sdio_tty_debug_info;
#endif

#define DEBUG_MSG(sdio_tty_drv, x...) if (sdio_tty_drv->debug_msg_on) pr_info(x)

/*
 * Enable sdio_tty debug messages
 * By default the sdio_tty debug messages are turned off
 */
static int csvt_debug_msg_on;
module_param(csvt_debug_msg_on, int, 0);

static void sdio_tty_read(struct work_struct *work)
{
	int ret = 0;
	int read_avail = 0;
	int left = 0;
	int total_push = 0;
	int num_push = 0;
	struct sdio_tty *sdio_tty_drv = NULL;

	sdio_tty_drv = container_of(work, struct sdio_tty, work_read);

	if (!sdio_tty_drv) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL sdio_tty", __func__);
		return ;
	}

	if (sdio_tty_drv->sdio_tty_state != TTY_OPENED) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: sdio_tty_state = %d",
			__func__, sdio_tty_drv->sdio_tty_state);
		return;
	}

	if (!sdio_tty_drv->read_buf) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL read_buf for dev %s",
			__func__, sdio_tty_drv->tty_dev_name);
		return;
	}

	/* Read the data from the SDIO channel as long as there is available
	   data */
	while (1) {
		if (test_bit(TTY_THROTTLED, &sdio_tty_drv->tty_str->flags)) {
			DEBUG_MSG(sdio_tty_drv, SDIO_TTY_MODULE_NAME
					": %s: TTY_THROTTLED bit is set for "
					"dev %s, exit", __func__,
					sdio_tty_drv->tty_dev_name);
			return;
		}

		total_push = 0;
		read_avail = sdio_read_avail(sdio_tty_drv->ch);

		DEBUG_MSG(sdio_tty_drv, SDIO_TTY_MODULE_NAME
				": %s: read_avail is %d for dev %s", __func__,
				read_avail, sdio_tty_drv->tty_dev_name);

		if (read_avail == 0) {
			DEBUG_MSG(sdio_tty_drv, SDIO_TTY_MODULE_NAME
					": %s: read_avail is 0 for dev %s",
					__func__, sdio_tty_drv->tty_dev_name);
			return;
		}

		if (read_avail > SDIO_TTY_MAX_PACKET_SIZE) {
			pr_err(SDIO_TTY_MODULE_NAME ": %s: read_avail(%d) is "
				"bigger than SDIO_TTY_MAX_PACKET_SIZE(%d) "
				"for dev %s", __func__, read_avail,
				SDIO_TTY_MAX_PACKET_SIZE,
				sdio_tty_drv->tty_dev_name);
			return;
		}

		ret = sdio_read(sdio_tty_drv->ch,
				sdio_tty_drv->read_buf,
				read_avail);
		if (ret < 0) {
			pr_err(SDIO_TTY_MODULE_NAME ": %s: sdio_read error(%d) "
				"for dev %s", __func__, ret,
				sdio_tty_drv->tty_dev_name);
			return;
		}

		left = read_avail;
		do {
			num_push = tty_insert_flip_string(
				sdio_tty_drv->tty_str,
				sdio_tty_drv->read_buf+total_push,
				left);
			total_push += num_push;
			left -= num_push;
			tty_flip_buffer_push(sdio_tty_drv->tty_str);
		} while (left != 0);

		if (total_push != read_avail) {
			pr_err(SDIO_TTY_MODULE_NAME ": %s: failed, total_push"
				"(%d) != read_avail(%d) for dev %s\n",
				__func__, total_push, read_avail,
				sdio_tty_drv->tty_dev_name);
		}

		tty_flip_buffer_push(sdio_tty_drv->tty_str);
		sdio_tty_drv->total_rx += read_avail;

		DEBUG_MSG(sdio_tty_drv, SDIO_TTY_MODULE_NAME ": %s: Rx: %d, "
				"Total Rx = %d bytes for dev %s", __func__,
				read_avail, sdio_tty_drv->total_rx,
				sdio_tty_drv->tty_dev_name);
	}
}

/**
  * sdio_tty_write_room
  *
  * This is the write_room function of the tty driver.
  *
  * @tty: pointer to tty struct.
  * @return free bytes for write.
  *
  */
static int sdio_tty_write_room(struct tty_struct *tty)
{
	int write_avail = 0;
	struct sdio_tty *sdio_tty_drv = NULL;

	if (!tty) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL tty", __func__);
		return -ENODEV;
	}
	sdio_tty_drv = tty->driver_data;
	if (!sdio_tty_drv) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL sdio_tty_drv",
			__func__);
		return -ENODEV;
	}

	if (sdio_tty_drv->sdio_tty_state != TTY_OPENED) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: sdio_tty_state = %d",
			__func__, sdio_tty_drv->sdio_tty_state);
		return -EPERM;
	}

	write_avail = sdio_write_avail(sdio_tty_drv->ch);
	DEBUG_MSG(sdio_tty_drv, SDIO_TTY_MODULE_NAME ": %s: write_avail=%d "
			"for dev %s", __func__, write_avail,
			sdio_tty_drv->tty_dev_name);

	return write_avail;
}

/**
  * sdio_tty_write_callback
  * this is the write callback of the tty driver.
  *
  * @tty: pointer to tty struct.
  * @buf: buffer to write from.
  * @count: number of bytes to write.
  * @return bytes written or negative value on error.
  *
  * if destination buffer has not enough room for the incoming
  * data, writes the possible amount of bytes .
  */
static int sdio_tty_write_callback(struct tty_struct *tty,
				   const unsigned char *buf, int count)
{
	int write_avail = 0;
	int len = count;
	int ret = 0;
	struct sdio_tty *sdio_tty_drv = NULL;

	if (!tty) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL tty", __func__);
		return -ENODEV;
	}
	sdio_tty_drv = tty->driver_data;
	if (!sdio_tty_drv) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL sdio_tty_drv",
			__func__);
		return -ENODEV;
	}

	if (sdio_tty_drv->sdio_tty_state != TTY_OPENED) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: sdio_tty_state = %d",
			__func__, sdio_tty_drv->sdio_tty_state);
		return -EPERM;
	}

	DEBUG_MSG(sdio_tty_drv, SDIO_TTY_MODULE_NAME ": %s: Write Callback "
			"called with %d bytes for dev %s\n", __func__, count,
			sdio_tty_drv->tty_dev_name);
	write_avail = sdio_write_avail(sdio_tty_drv->ch);
	if (write_avail == 0) {
		DEBUG_MSG(sdio_tty_drv, SDIO_TTY_MODULE_NAME ": %s: "
				"write_avail is 0 for dev %s\n",
				__func__, sdio_tty_drv->tty_dev_name);
		return 0;
	}
	if (write_avail > SDIO_TTY_MAX_PACKET_SIZE) {
		DEBUG_MSG(sdio_tty_drv, SDIO_TTY_MODULE_NAME ": %s: "
				"write_avail(%d) is bigger than max packet "
				"size(%d) for dev %s, setting to "
				"max_packet_size\n", __func__, write_avail,
				SDIO_TTY_MAX_PACKET_SIZE,
				sdio_tty_drv->tty_dev_name);
		write_avail = SDIO_TTY_MAX_PACKET_SIZE;
	}
	if (write_avail < count) {
		DEBUG_MSG(sdio_tty_drv, SDIO_TTY_MODULE_NAME ": %s: "
				"write_avail(%d) is smaller than required(%d) "
				"for dev %s, writing only %d bytes\n",
				__func__, write_avail, count,
				sdio_tty_drv->tty_dev_name, write_avail);
		len = write_avail;
	}
	ret = sdio_write(sdio_tty_drv->ch, buf, len);
	if (ret) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: sdio_write failed for "
			"dev %s, ret=%d\n", __func__,
			sdio_tty_drv->tty_dev_name, ret);
		return 0;
	}

	sdio_tty_drv->total_tx += len;

	DEBUG_MSG(sdio_tty_drv, SDIO_TTY_MODULE_NAME ": %s: Tx: %d, "
			"Total Tx = %d for dev %s", __func__, len,
			sdio_tty_drv->total_tx, sdio_tty_drv->tty_dev_name);
	return len;
}

static void sdio_tty_notify(void *priv, unsigned event)
{
	struct sdio_tty *sdio_tty_drv = priv;

	if (!sdio_tty_drv) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL sdio_tty_drv",
			__func__);
	}

	if (sdio_tty_drv->sdio_tty_state != TTY_OPENED) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: sdio_tty_state = %d",
			__func__, sdio_tty_drv->sdio_tty_state);
		return;
	}

	DEBUG_MSG(sdio_tty_drv, SDIO_TTY_MODULE_NAME ": %s: event %d "
			"received for dev %s\n", __func__, event,
			sdio_tty_drv->tty_dev_name);

	if (event == SDIO_EVENT_DATA_READ_AVAIL)
		queue_work(sdio_tty_drv->workq, &sdio_tty_drv->work_read);
}

/**
  * sdio_tty_open
  * This is the open callback of the tty driver. it opens
  * the sdio channel, and creates the workqueue.
  *
  * @tty: a pointer to the tty struct.
  * @file: file descriptor.
  * @return 0 on success or negative value on error.
  */
static int sdio_tty_open(struct tty_struct *tty, struct file *file)
{
	int ret = 0;
	int i = 0;
	struct sdio_tty *sdio_tty_drv = NULL;

	if (!tty) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL tty", __func__);
		return -ENODEV;
	}

	for (i = 0; i < MAX_SDIO_TTY_DEVS; i++) {
		if (sdio_tty[i] == NULL)
			continue;
		if (!strncmp(sdio_tty[i]->tty_dev_name, tty->name,
				MAX_SDIO_TTY_DEV_NAME_SIZE)) {
			sdio_tty_drv = sdio_tty[i];
			break;
		}
	}

	if (!sdio_tty_drv) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL sdio_tty_drv",
		       __func__);
		return -ENODEV;
	}

	sdio_tty_drv->tty_open_count++;
	if (sdio_tty_drv->sdio_tty_state == TTY_OPENED) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: tty dev(%s) is already open",
			__func__, sdio_tty_drv->tty_dev_name);
		return -EBUSY;
	}

	tty->driver_data = sdio_tty_drv;

	sdio_tty_drv->tty_str = tty;
	sdio_tty_drv->tty_str->low_latency = 1;
	sdio_tty_drv->tty_str->icanon = 0;
	set_bit(TTY_NO_WRITE_SPLIT, &sdio_tty_drv->tty_str->flags);

	sdio_tty_drv->read_buf = kzalloc(SDIO_TTY_MAX_PACKET_SIZE, GFP_KERNEL);
	if (sdio_tty_drv->read_buf == NULL) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: failed to allocate read_buf "
			"for dev %s", __func__, sdio_tty_drv->tty_dev_name);
		return -ENOMEM;
	}

	sdio_tty_drv->workq = create_singlethread_workqueue("sdio_tty_read");
	if (!sdio_tty_drv->workq) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: failed to create workq "
			"for dev %s", __func__, sdio_tty_drv->tty_dev_name);
		return -ENOMEM;
	}

	if (!sdio_tty_drv->is_sdio_open) {
		ret = sdio_open(sdio_tty_drv->sdio_ch_name, &sdio_tty_drv->ch,
				sdio_tty_drv, sdio_tty_notify);
		if (ret < 0) {
			pr_err(SDIO_TTY_MODULE_NAME ": %s: sdio_open err=%d "
				"for dev %s\n", __func__, ret,
				sdio_tty_drv->tty_dev_name);
			destroy_workqueue(sdio_tty_drv->workq);
			return ret;
		}

		pr_info(SDIO_TTY_MODULE_NAME ": %s: SDIO_TTY channel(%s) "
			"opened\n", __func__, sdio_tty_drv->sdio_ch_name);

		sdio_tty_drv->is_sdio_open = 1;
	} else {
		/* If SDIO channel is already open try to read the data
		 * from the modem
		 */
		queue_work(sdio_tty_drv->workq, &sdio_tty_drv->work_read);

	}

	sdio_tty_drv->sdio_tty_state = TTY_OPENED;

	pr_info(SDIO_TTY_MODULE_NAME ": %s: TTY device(%s) opened\n",
		__func__, sdio_tty_drv->tty_dev_name);

	return ret;
}

/**
  * sdio_tty_close
  * This is the close callback of the tty driver. it requests
  * the main thread to exit, and waits for notification of it.
  * it also de-allocates the buffers, and unregisters the tty
  * driver and device.
  *
  * @tty: a pointer to the tty struct.
  * @file: file descriptor.
  * @return None.
  */
static void sdio_tty_close(struct tty_struct *tty, struct file *file)
{
	struct sdio_tty *sdio_tty_drv = NULL;

	if (!tty) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL tty", __func__);
		return;
	}
	sdio_tty_drv = tty->driver_data;
	if (!sdio_tty_drv) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL sdio_tty_drv",
		       __func__);
		return;
	}
	if (sdio_tty_drv->sdio_tty_state != TTY_OPENED) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: trying to close a "
			"TTY device that was not opened\n", __func__);
		return;
	}
	if (--sdio_tty_drv->tty_open_count != 0)
		return;

	flush_workqueue(sdio_tty_drv->workq);
	destroy_workqueue(sdio_tty_drv->workq);

	kfree(sdio_tty_drv->read_buf);
	sdio_tty_drv->read_buf = NULL;

	sdio_tty_drv->sdio_tty_state = TTY_CLOSED;

	pr_info(SDIO_TTY_MODULE_NAME ": %s: SDIO_TTY device(%s) closed\n",
		__func__, sdio_tty_drv->tty_dev_name);
}

static void sdio_tty_unthrottle(struct tty_struct *tty)
{
	struct sdio_tty *sdio_tty_drv = NULL;

	if (!tty) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL tty", __func__);
		return;
	}
	sdio_tty_drv = tty->driver_data;
	if (!sdio_tty_drv) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL sdio_tty_drv",
		       __func__);
		return;
	}

	if (sdio_tty_drv->sdio_tty_state != TTY_OPENED) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: sdio_tty_state = %d",
		       __func__, sdio_tty_drv->sdio_tty_state);
		return;
	}

	queue_work(sdio_tty_drv->workq, &sdio_tty_drv->work_read);
	return;
}

static const struct tty_operations sdio_tty_ops = {
	.open = sdio_tty_open,
	.close = sdio_tty_close,
	.write = sdio_tty_write_callback,
	.write_room = sdio_tty_write_room,
	.unthrottle = sdio_tty_unthrottle,
};

int sdio_tty_init_tty(char *tty_name, char *sdio_ch_name,
			enum sdio_tty_devices device_id, int debug_msg_on)
{
	int ret = 0;
	int i = 0;
	struct device *tty_dev = NULL;
	struct sdio_tty *sdio_tty_drv = NULL;

	sdio_tty_drv = kzalloc(sizeof(struct sdio_tty), GFP_KERNEL);
	if (sdio_tty_drv == NULL) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: failed to allocate sdio_tty "
			"for dev %s", __func__, tty_name);
		return -ENOMEM;
	}

	for (i = 0; i < MAX_SDIO_TTY_DEVS; i++) {
		if (sdio_tty[i] == NULL) {
			sdio_tty[i] = sdio_tty_drv;
			break;
		}
	}

	if (i == MAX_SDIO_TTY_DEVS) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: tty dev(%s) creation failed,"
			" max limit(%d) reached.", __func__, tty_name,
			MAX_SDIO_TTY_DEVS);
		kfree(sdio_tty_drv);
		return -ENODEV;
	}

	snprintf(sdio_tty_drv->tty_dev_name, MAX_SDIO_TTY_DEV_NAME_SIZE,
			"%s%d", tty_name, 0);
	sdio_tty_drv->sdio_ch_name = sdio_ch_name;
	sdio_tty_drv->device_id = device_id;
	pr_info(SDIO_TTY_MODULE_NAME ": %s: dev=%s, id=%d, channel=%s\n",
		__func__, sdio_tty_drv->tty_dev_name, sdio_tty_drv->device_id,
		sdio_tty_drv->sdio_ch_name);

	INIT_WORK(&sdio_tty_drv->work_read, sdio_tty_read);

	sdio_tty_drv->tty_drv = alloc_tty_driver(MAX_SDIO_TTY_DRV);

	if (!sdio_tty_drv->tty_drv) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s - tty_drv is NULL for dev %s",
			__func__, sdio_tty_drv->tty_dev_name);
		kfree(sdio_tty_drv);
		return -ENODEV;
	}

	sdio_tty_drv->tty_drv->name = tty_name;
	sdio_tty_drv->tty_drv->owner = THIS_MODULE;
	sdio_tty_drv->tty_drv->driver_name = "SDIO_tty";
	/* uses dynamically assigned dev_t values */
	sdio_tty_drv->tty_drv->type = TTY_DRIVER_TYPE_SERIAL;
	sdio_tty_drv->tty_drv->subtype = SERIAL_TYPE_NORMAL;
	sdio_tty_drv->tty_drv->flags = TTY_DRIVER_REAL_RAW
		| TTY_DRIVER_DYNAMIC_DEV
		| TTY_DRIVER_RESET_TERMIOS;

	/* initializing the tty driver */
	sdio_tty_drv->tty_drv->init_termios = tty_std_termios;
	sdio_tty_drv->tty_drv->init_termios.c_cflag =
		B4800 | CS8 | CREAD | HUPCL | CLOCAL;
	sdio_tty_drv->tty_drv->init_termios.c_ispeed = INPUT_SPEED;
	sdio_tty_drv->tty_drv->init_termios.c_ospeed = OUTPUT_SPEED;

	tty_set_operations(sdio_tty_drv->tty_drv, &sdio_tty_ops);

	ret = tty_register_driver(sdio_tty_drv->tty_drv);
	if (ret) {
		put_tty_driver(sdio_tty_drv->tty_drv);
		pr_err(SDIO_TTY_MODULE_NAME ": %s: tty_register_driver() "
			"failed for dev %s\n", __func__,
			sdio_tty_drv->tty_dev_name);

		sdio_tty_drv->tty_drv = NULL;
		kfree(sdio_tty_drv);
		return -ENODEV;
	}

	tty_dev = tty_register_device(sdio_tty_drv->tty_drv, 0, NULL);
	if (IS_ERR(tty_dev)) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: tty_register_device() "
			"failed for dev %s\n", __func__,
			sdio_tty_drv->tty_dev_name);
		tty_unregister_driver(sdio_tty_drv->tty_drv);
		put_tty_driver(sdio_tty_drv->tty_drv);
		kfree(sdio_tty_drv);
		return -ENODEV;
	}

	sdio_tty_drv->sdio_tty_state = TTY_REGISTERED;
	if (debug_msg_on) {
		pr_info(SDIO_TTY_MODULE_NAME ": %s: turn on debug msg for %s",
			__func__, sdio_tty_drv->tty_dev_name);
		sdio_tty_drv->debug_msg_on = debug_msg_on;
	}
	return 0;
}

int sdio_tty_uninit_tty(void *sdio_tty_handle)
{
	int ret = 0;
	int i = 0;
	struct sdio_tty *sdio_tty_drv = sdio_tty_handle;

	if (!sdio_tty_drv) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL sdio_tty_drv",
		       __func__);
		return -ENODEV;
	}
	if (sdio_tty_drv->sdio_tty_state == TTY_OPENED) {
		flush_workqueue(sdio_tty_drv->workq);
		destroy_workqueue(sdio_tty_drv->workq);

		kfree(sdio_tty_drv->read_buf);
		sdio_tty_drv->read_buf = NULL;
	}

	if (sdio_tty_drv->sdio_tty_state != TTY_INITIAL) {
		tty_unregister_device(sdio_tty_drv->tty_drv, 0);

		ret = tty_unregister_driver(sdio_tty_drv->tty_drv);
		if (ret) {
			pr_err(SDIO_TTY_MODULE_NAME ": %s: "
				"tty_unregister_driver() failed for dev %s\n",
				__func__, sdio_tty_drv->tty_dev_name);
		}
		put_tty_driver(sdio_tty_drv->tty_drv);
		sdio_tty_drv->sdio_tty_state = TTY_INITIAL;
		sdio_tty_drv->tty_drv = NULL;
	}

	for (i = 0; i < MAX_SDIO_TTY_DEVS; i++) {
		if (sdio_tty[i] == NULL)
			continue;
		if (sdio_tty[i]->device_id == sdio_tty_drv->device_id) {
			sdio_tty[i] = NULL;
			break;
		}
	}

	DEBUG_MSG(sdio_tty_drv, SDIO_TTY_MODULE_NAME ": %s: Freeing sdio_tty "
			"structure, dev=%s", __func__,
			sdio_tty_drv->tty_dev_name);
	kfree(sdio_tty_drv);

	return 0;
}

static int sdio_tty_probe(struct platform_device *pdev)
{
	const struct platform_device_id *id = platform_get_device_id(pdev);
	enum sdio_tty_devices device_id = id->driver_data;
	char *device_name = NULL;
	char *channel_name = NULL;
	int debug_msg_on = 0;
	int ret = 0;

	pr_debug(SDIO_TTY_MODULE_NAME ": %s for %s", __func__, pdev->name);

	switch (device_id) {
	case SDIO_CSVT:
		device_name = SDIO_TTY_CSVT_DEV;
		channel_name = SDIO_TTY_CH_CSVT;
		debug_msg_on = csvt_debug_msg_on;
		break;
	case SDIO_CSVT_TEST_APP:
		device_name = SDIO_TTY_CSVT_TEST_DEV;
		channel_name = SDIO_TTY_CH_CSVT;
		debug_msg_on = csvt_debug_msg_on;
		break;
	default:
		pr_err(SDIO_TTY_MODULE_NAME ": %s Invalid device:%s, id:%d",
			__func__, pdev->name, device_id);
		ret = -ENODEV;
		break;
	}

	if (device_name) {
		ret = sdio_tty_init_tty(device_name, channel_name,
					device_id, debug_msg_on);
		if (ret) {
			pr_err(SDIO_TTY_MODULE_NAME ": %s: sdio_tty_init_tty "
				"failed for dev:%s", __func__, device_name);
		}
	}
	return ret;
}

static int sdio_tty_remove(struct platform_device *pdev)
{
	const struct platform_device_id *id = platform_get_device_id(pdev);
	enum sdio_tty_devices device_id = id->driver_data;
	struct sdio_tty *sdio_tty_drv = NULL;
	int i = 0;
	int ret = 0;

	pr_debug(SDIO_TTY_MODULE_NAME ": %s for %s", __func__, pdev->name);

	for (i = 0; i < MAX_SDIO_TTY_DEVS; i++) {
		if (sdio_tty[i] == NULL)
			continue;
		if (sdio_tty[i]->device_id == device_id) {
			sdio_tty_drv = sdio_tty[i];
			break;
		}
	}

	if (!sdio_tty_drv) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL sdio_tty_drv",
		       __func__);
		return -ENODEV;
	}

	ret = sdio_tty_uninit_tty(sdio_tty_drv);
	if (ret) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: sdio_tty_uninit_tty "
			"failed for %s", __func__, pdev->name);
	}
	return ret;
}

static struct platform_driver sdio_tty_pdrv = {
	.probe		= sdio_tty_probe,
	.remove		= sdio_tty_remove,
	.id_table	= sdio_tty_id_table,
	.driver		= {
		.name	= "SDIO_TTY",
		.owner	= THIS_MODULE,
	},
};

#ifdef CONFIG_DEBUG_FS
void sdio_tty_print_info(void)
{
	int i = 0;

	for (i = 0; i < MAX_SDIO_TTY_DEVS; i++) {
		if (sdio_tty[i] == NULL)
			continue;
		pr_info(SDIO_TTY_MODULE_NAME ": %s: Total Rx=%d, Tx = %d "
			"for dev %s", __func__, sdio_tty[i]->total_rx,
			sdio_tty[i]->total_tx, sdio_tty[i]->tty_dev_name);
	}
}

static int tty_debug_info_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t tty_debug_info_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	sdio_tty_print_info();
	return count;
}

const struct file_operations tty_debug_info_ops = {
	.open = tty_debug_info_open,
	.write = tty_debug_info_write,
};
#endif

/*
 *  Module Init.
 *
 *  Register SDIO TTY driver.
 *
 */
static int __init sdio_tty_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&sdio_tty_pdrv);
	if (ret) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: platform_driver_register "
					    "failed", __func__);
	}
#ifdef CONFIG_DEBUG_FS
	else {
		sdio_tty_debug_root = debugfs_create_dir("sdio_tty", NULL);
		if (sdio_tty_debug_root) {
			sdio_tty_debug_info = debugfs_create_file(
							"sdio_tty_debug",
							S_IRUGO | S_IWUGO,
							sdio_tty_debug_root,
							NULL,
							&tty_debug_info_ops);
		}
	}
#endif
	return ret;
};

/*
 *  Module Exit.
 *
 *  Unregister SDIO TTY driver.
 *
 */
static void __exit sdio_tty_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove(sdio_tty_debug_info);
	debugfs_remove(sdio_tty_debug_root);
#endif
	platform_driver_unregister(&sdio_tty_pdrv);
}

module_init(sdio_tty_init);
module_exit(sdio_tty_exit);

MODULE_DESCRIPTION("SDIO TTY");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Maya Erez <merez@codeaurora.org>");
