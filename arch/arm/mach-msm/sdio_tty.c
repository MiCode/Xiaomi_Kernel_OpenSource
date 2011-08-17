/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <mach/sdio_al.h>

#define INPUT_SPEED			4800
#define OUTPUT_SPEED			4800
#define SDIO_TTY_MODULE_NAME		"sdio_tty"
#define SDIO_TTY_MAX_PACKET_SIZE	4096
#define MAX_SDIO_TTY_DRV		1

enum sdio_tty_state {
	TTY_INITIAL = 0,
	TTY_REGISTERED = 1,
	TTY_OPENED = 2,
	TTY_CLOSED = 3,
};

struct sdio_tty {
	struct sdio_channel *ch;
	char *sdio_ch_name;
	struct workqueue_struct *workq;
	struct work_struct work_read;
	wait_queue_head_t   waitq;
	struct tty_driver *tty_drv;
	struct tty_struct *tty_str;
	int debug_msg_on;
	char *read_buf;
	enum sdio_tty_state sdio_tty_state;
	int is_sdio_open;
};

static struct sdio_tty *sdio_tty;

#define DEBUG_MSG(sdio_tty, x...) if (sdio_tty->debug_msg_on) pr_info(x)

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
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL sdio_tty",
		       __func__);
		return ;
	}

	if (sdio_tty_drv->sdio_tty_state != TTY_OPENED) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: sdio_tty_state = %d",
		       __func__, sdio_tty_drv->sdio_tty_state);
		return;
	}

	if (!sdio_tty_drv->read_buf) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL read_buf", __func__);
		return;
	}

	/* Read the data from teh SDIO channel as long as there is available
	   data */
	while (1) {
		if (test_bit(TTY_THROTTLED, &sdio_tty_drv->tty_str->flags)) {
			DEBUG_MSG(sdio_tty_drv,
				  SDIO_TTY_MODULE_NAME ": %s: TTY_THROTTLED bit"
						       " is set, exit",
				  __func__);
			return;
		}

		total_push = 0;
		read_avail = sdio_read_avail(sdio_tty_drv->ch);

		DEBUG_MSG(sdio_tty_drv, SDIO_TTY_MODULE_NAME
					     ": %s: read_avail is %d", __func__,
					     read_avail);

		if (read_avail == 0) {
			DEBUG_MSG(sdio_tty_drv,
				  SDIO_TTY_MODULE_NAME ": %s: read_avail is 0",
				  __func__);
			return;
		}

		if (read_avail > SDIO_TTY_MAX_PACKET_SIZE) {
			pr_err(SDIO_TTY_MODULE_NAME ": %s: read_avail(%d) is "
				"bigger than SDIO_TTY_MAX_PACKET_SIZE(%d)",
			       __func__, read_avail, SDIO_TTY_MAX_PACKET_SIZE);
			return;
		}

		ret = sdio_read(sdio_tty_drv->ch,
				sdio_tty_drv->read_buf,
				read_avail);
		if (ret < 0) {
			pr_err(SDIO_TTY_MODULE_NAME ": %s: sdio_read error(%d)",
			       __func__, ret);
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
						    "(%d) != read_avail(%d)\n",
			       __func__, total_push, read_avail);
		}

		tty_flip_buffer_push(sdio_tty_drv->tty_str);

		DEBUG_MSG(sdio_tty_drv,
			  SDIO_TTY_MODULE_NAME ": %s: End of read %d bytes",
				__func__, read_avail);
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
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL tty",
		       __func__);
		return -ENODEV;
	}
	sdio_tty_drv = tty->driver_data;
	if (!sdio_tty_drv) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL sdio_tty_drv",
		       __func__);
		return -ENODEV;
	}

	write_avail = sdio_write_avail(sdio_tty_drv->ch);
	DEBUG_MSG(sdio_tty_drv,
		  SDIO_TTY_MODULE_NAME ": %s: write_avail=%d",
		 __func__, write_avail);

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
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL tty",
		       __func__);
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

	DEBUG_MSG(sdio_tty_drv,
		  SDIO_TTY_MODULE_NAME ": %s: WRITING CALLBACK CALLED WITH "
				       "%d bytes\n",
		 __func__, count);
	write_avail = sdio_write_avail(sdio_tty_drv->ch);
	if (write_avail == 0) {
		DEBUG_MSG(sdio_tty_drv,
			  SDIO_TTY_MODULE_NAME ": %s: write_avail is 0\n",
			 __func__);
		return 0;
	}
	if (write_avail > SDIO_TTY_MAX_PACKET_SIZE) {
		DEBUG_MSG(sdio_tty_drv,
			  SDIO_TTY_MODULE_NAME ": %s: write_avail(%d) is "
			  "bigger than max packet size,(%d), setting to "
			  "max_packet_size\n",
			  __func__, write_avail, SDIO_TTY_MAX_PACKET_SIZE);
		write_avail = SDIO_TTY_MAX_PACKET_SIZE;
	}
	if (write_avail < count) {
		DEBUG_MSG(sdio_tty_drv,
			  SDIO_TTY_MODULE_NAME ": %s: write_avail(%d) is "
					       "smaller than "
				    "required(%d), writing only %d bytes\n",
			 __func__, write_avail, count, write_avail);
		len = write_avail;
	}
	ret = sdio_write(sdio_tty_drv->ch, buf, len);
	if (ret) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: sdio_write failed, ret=%d\n",
			 __func__, ret);
		return 0;
	}

	DEBUG_MSG(sdio_tty_drv,
		  SDIO_TTY_MODULE_NAME ": %s: End of function, len=%d bytes\n",
		 __func__, len);

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

	DEBUG_MSG(sdio_tty_drv,
		  SDIO_TTY_MODULE_NAME ": %s: event %d received\n", __func__,
		  event);

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
	struct sdio_tty *sdio_tty_drv = NULL;

	if (!tty) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL tty",
		       __func__);
		return -ENODEV;
	}
	sdio_tty_drv = sdio_tty;
	if (!sdio_tty_drv) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL sdio_tty_drv",
		       __func__);
		return -ENODEV;
	}

	tty->driver_data = sdio_tty_drv;

	sdio_tty_drv->tty_str = tty;
	sdio_tty_drv->tty_str->low_latency = 1;
	sdio_tty_drv->tty_str->icanon = 0;
	set_bit(TTY_NO_WRITE_SPLIT, &sdio_tty_drv->tty_str->flags);

	sdio_tty_drv->read_buf = kzalloc(SDIO_TTY_MAX_PACKET_SIZE, GFP_KERNEL);
	if (sdio_tty_drv->read_buf == NULL) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: failed to allocate read_buf",
		       __func__);
		return -ENOMEM;
	}

	sdio_tty_drv->workq = create_singlethread_workqueue("sdio_tty_read");
	if (!sdio_tty_drv->workq) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: failed to create workq",
		       __func__);
		return -ENOMEM;
	}

	if (sdio_tty_drv->sdio_tty_state == TTY_OPENED) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: tty is already open",
		       __func__);
		return -EBUSY;
	}

	if (!sdio_tty_drv->is_sdio_open) {
		ret = sdio_open(sdio_tty_drv->sdio_ch_name, &sdio_tty_drv->ch,
				sdio_tty_drv, sdio_tty_notify);
		if (ret < 0) {
			pr_err(SDIO_TTY_MODULE_NAME ": %s: sdio_open err=%d\n",
			       __func__, ret);
			destroy_workqueue(sdio_tty_drv->workq);
			return ret;
		}

		pr_info(SDIO_TTY_MODULE_NAME ": %s: SDIO_TTY channel opened\n",
			__func__);

		sdio_tty_drv->is_sdio_open = 1;
	} else {
		/* If SDIO channel is already open try to read the data
		 * from the modem
		 */
		queue_work(sdio_tty_drv->workq, &sdio_tty_drv->work_read);

	}

	sdio_tty_drv->sdio_tty_state = TTY_OPENED;

	pr_info(SDIO_TTY_MODULE_NAME ": %s: TTY device opened\n",
		__func__);

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
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL tty",
		       __func__);
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
					    "TTY device that was not opened\n",
		       __func__);
		return;
	}

	flush_workqueue(sdio_tty_drv->workq);
	destroy_workqueue(sdio_tty_drv->workq);

	kfree(sdio_tty_drv->read_buf);
	sdio_tty_drv->read_buf = NULL;

	sdio_tty_drv->sdio_tty_state = TTY_CLOSED;

	pr_info(SDIO_TTY_MODULE_NAME ": %s: SDIO_TTY channel closed\n",
		__func__);
}

static void sdio_tty_unthrottle(struct tty_struct *tty)
{
	struct sdio_tty *sdio_tty_drv = NULL;

	if (!tty) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL tty",
		       __func__);
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

void *sdio_tty_init_tty(char *tty_name, char *sdio_ch_name)
{
	int ret = 0;
	struct device *tty_dev;
	struct sdio_tty *sdio_tty_drv;

	pr_info(SDIO_TTY_MODULE_NAME ": %s\n", __func__);

	sdio_tty_drv = kzalloc(sizeof(struct sdio_tty), GFP_KERNEL);
	if (sdio_tty_drv == NULL) {
		pr_err(SDIO_TTY_MODULE_NAME "%s: failed to allocate sdio_tty",
		       __func__);
		return NULL;
	}

	sdio_tty = sdio_tty_drv;
	sdio_tty_drv->sdio_ch_name = sdio_ch_name;

	INIT_WORK(&sdio_tty_drv->work_read, sdio_tty_read);

	sdio_tty_drv->tty_drv = alloc_tty_driver(MAX_SDIO_TTY_DRV);

	if (!sdio_tty_drv->tty_drv) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s - tty_drv is NULL",
				   __func__);
		kfree(sdio_tty_drv);
		return NULL;
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
					    "failed\n", __func__);

		sdio_tty_drv->tty_drv = NULL;
		kfree(sdio_tty_drv);
		return NULL;
	}

	tty_dev = tty_register_device(sdio_tty_drv->tty_drv, 0, NULL);
	if (IS_ERR(tty_dev)) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: tty_register_device() "
			"failed\n", __func__);
		tty_unregister_driver(sdio_tty_drv->tty_drv);
		put_tty_driver(sdio_tty_drv->tty_drv);
		kfree(sdio_tty_drv);
		return NULL;
	}

	sdio_tty_drv->sdio_tty_state = TTY_REGISTERED;
	return sdio_tty_drv;
}
EXPORT_SYMBOL(sdio_tty_init_tty);

int sdio_tty_uninit_tty(void *sdio_tty_handle)
{
	int ret = 0;
	struct sdio_tty *sdio_tty_drv = sdio_tty_handle;

	if (!sdio_tty_drv) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL sdio_tty_drv",
		       __func__);
		return -ENODEV;
	}

	if (sdio_tty_drv->sdio_tty_state != TTY_INITIAL) {
		tty_unregister_device(sdio_tty_drv->tty_drv, 0);

		ret = tty_unregister_driver(sdio_tty_drv->tty_drv);
		if (ret) {
			pr_err(SDIO_TTY_MODULE_NAME ": %s: "
			    "tty_unregister_driver() failed\n", __func__);
		}
		put_tty_driver(sdio_tty_drv->tty_drv);
		sdio_tty_drv->sdio_tty_state = TTY_INITIAL;
		sdio_tty_drv->tty_drv = NULL;
	}

	pr_info(SDIO_TTY_MODULE_NAME ": %s: Freeing sdio_tty structure",
		__func__);
	kfree(sdio_tty_drv);

	return 0;
}
EXPORT_SYMBOL(sdio_tty_uninit_tty);


void sdio_tty_enable_debug_msg(void *sdio_tty_handle, int enable)
{
	struct sdio_tty *sdio_tty_drv = sdio_tty_handle;

	if (!sdio_tty_drv) {
		pr_err(SDIO_TTY_MODULE_NAME ": %s: NULL sdio_tty_drv",
		       __func__);
		return;
	}
	pr_info(SDIO_TTY_MODULE_NAME ": %s: setting debug_msg_on to %d",
		       __func__, enable);
	sdio_tty_drv->debug_msg_on = enable;
}
EXPORT_SYMBOL(sdio_tty_enable_debug_msg);


static int __init sdio_tty_init(void)
{
	return 0;
};

/*
 *  Module Exit.
 *
 *  Unregister SDIO driver.
 *
 */
static void __exit sdio_tty_exit(void)
{
}

module_init(sdio_tty_init);
module_exit(sdio_tty_exit);

MODULE_DESCRIPTION("SDIO TTY");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Maya Erez <merez@codeaurora.org>");
