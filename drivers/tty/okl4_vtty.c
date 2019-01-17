/*
 * drivers/char/okl4_vtty.c
 *
 * Copyright (c) 2012-2014 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 * Copyright (c) 2014-2017 Cog Systems Pty Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * OKL4 Microvisor Virtual TTY driver.
 *
 * Clients using this driver must have vclient names of the form
 * "vtty%d", where %d is the tty number, which must be
 * unique and less than MAX_VTTYS.
 */

/* #define DEBUG 1 */
/* #define VERBOSE_DEBUG 1 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <clocksource/arm_arch_timer.h>
#include <asm-generic/okl4_virq.h>

#include <microvisor/microvisor.h>
#if 0
#include <asm/okl4-microvisor/okl4tags.h>
#include <asm/okl4-microvisor/microvisor_bus.h>
#include <asm/okl4-microvisor/virq.h>
#endif

#define DRIVER_NAME "okl4-vtty"
#define DEVICE_NAME "vtty"
#define DEVICE_PREFIX "ttyV"

/* FIXME: Jira ticket SDK-138 - philipd. */
#define MAX_VTTYS 8
#define MAX_MSG_SIZE 32

struct vtty_port {
	bool exists;
	int vtty_id;

	bool read_throttled, write_full, irq_registered;
	struct work_struct read_work;
	spinlock_t write_lock;

	/*
	 * Buffer length is max_msg_size plus one u32, which encodes the
	 * message length.
	 */
	char *read_buf;
	int read_buf_pos, read_buf_len;
	char *write_buf;
	int write_buffered;
	size_t max_msg_size;

	okl4_kcap_t pipe_tx_kcap;
	okl4_kcap_t pipe_rx_kcap;
	int tx_irq;
	int rx_irq;

#ifdef CONFIG_OKL4_VTTY_CONSOLE
	struct console console;
#endif

	struct device *dev;
	struct tty_port port;
};

static struct workqueue_struct *read_workqueue;

static struct vtty_port ports[MAX_VTTYS];

static void
vtty_read_irq(struct vtty_port *port)
{
	queue_work(read_workqueue, &port->read_work);
}

static int
do_pipe_write(struct vtty_port *port, int count)
{
	okl4_error_t ret;
	int send;

	if (port->write_full)
		return 0;

	BUG_ON(count > port->max_msg_size);

	*(u32 *)port->write_buf = count;
	send = roundup(count + sizeof(u32), sizeof(u32));

	ret = _okl4_sys_pipe_send(port->pipe_tx_kcap, send,
			(void *)port->write_buf);

	if (ret == OKL4_ERROR_PIPE_NOT_READY) {
		okl4_pipe_control_t x = 0;

		okl4_pipe_control_setdoop(&x, true);
		okl4_pipe_control_setoperation(&x,
			OKL4_PIPE_CONTROL_OP_SET_TX_READY);
		_okl4_sys_pipe_control(port->pipe_tx_kcap, x);

		ret = _okl4_sys_pipe_send(port->pipe_tx_kcap, send,
				(void *)port->write_buf);
	}

	if (ret == OKL4_ERROR_PIPE_FULL ||
			ret == OKL4_ERROR_PIPE_NOT_READY) {
		port->write_full = true;
		return 0;
	}

	if (ret != OKL4_OK)
		return -EIO;

	return count;
}

static void
vtty_write_irq(struct vtty_port *port)
{
	struct tty_struct *tty = tty_port_tty_get(&port->port);

	spin_lock(&port->write_lock);

	port->write_full = false;

	if (port->write_buffered &&
			do_pipe_write(port, port->write_buffered) > 0)
		port->write_buffered = 0;

	if (tty)
		tty_wakeup(tty);

	spin_unlock(&port->write_lock);

	tty_kref_put(tty);
}

static irqreturn_t
vtty_tx_irq(int irq, void *dev)
{
	struct vtty_port *port = dev;
	okl4_pipe_state_t payload = okl4_get_virq_payload(irq);

	if (okl4_pipe_state_gettxavailable(&payload))
		vtty_write_irq(port);

	return IRQ_HANDLED;
}

static irqreturn_t
vtty_rx_irq(int irq, void *dev)
{
	struct vtty_port *port = dev;
	okl4_pipe_state_t payload = okl4_get_virq_payload(irq);

	if (okl4_pipe_state_getrxavailable(&payload))
		vtty_read_irq(port);

	return IRQ_HANDLED;
}

static int
vtty_install(struct tty_driver *driver, struct tty_struct *tty)
{
	int port_num = tty->index;
	struct vtty_port *port;
	int status;

	if (port_num < 0 || port_num >= MAX_VTTYS)
		return -ENXIO;

	port = &ports[port_num];
	if (!port->exists)
		return -ENODEV;

	tty->driver_data = port;

	port->write_full = false;
	port->read_throttled = false;
	port->write_buffered = 0;

	/*
	 * low_latency forces all tty read handling to be done by the
	 * read task.
	 */
	port->port.low_latency = 1;

	if (!port->irq_registered) {
		status = devm_request_irq(port->dev, port->tx_irq,
				vtty_tx_irq, 0, dev_name(port->dev), port);
		if (status)
			return status;

		status = devm_request_irq(port->dev, port->rx_irq,
				vtty_rx_irq, 0, dev_name(port->dev), port);
		if (status) {
			devm_free_irq(port->dev, port->tx_irq, port);
			return status;
		}

		port->irq_registered = true;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
	tty_port_install(&port->port, driver, tty);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	tty->port = &port->port;
	tty_standard_install(driver, tty);
#else
	tty->port = &port->port;
	if (tty_init_termios(tty) != 0)
		return -ENOMEM;

	tty_driver_kref_get(driver);
	tty->count++;
	driver->ttys[tty->index] = tty;
#endif

	return 0;
}

static int
vtty_open(struct tty_struct *tty, struct file *file)
{
	struct vtty_port *port = tty->driver_data;
	okl4_pipe_control_t x = 0;

	okl4_pipe_control_setdoop(&x, true);
	okl4_pipe_control_setoperation(&x,
		OKL4_PIPE_CONTROL_OP_SET_TX_READY);
	_okl4_sys_pipe_control(port->pipe_tx_kcap, x);
	okl4_pipe_control_setoperation(&x,
		OKL4_PIPE_CONTROL_OP_SET_RX_READY);
	_okl4_sys_pipe_control(port->pipe_rx_kcap, x);

	return tty_port_open(&port->port, tty, file);
}

static void
vtty_close(struct tty_struct *tty, struct file *file)
{
	struct vtty_port *port = tty->driver_data;
	if (port)
		tty_port_close(&port->port, tty, file);
}

static int
vtty_activate(struct tty_port *port, struct tty_struct *tty)
{
	struct vtty_port *vtty_port = tty->driver_data;

	/* Run the read task immediately to drain the channel */
	queue_work(read_workqueue, &vtty_port->read_work);

	return 0;
}

static void
vtty_shutdown(struct tty_port *port)
{
	struct vtty_port *vtty_port =
			container_of(port, struct vtty_port, port);

	cancel_work_sync(&vtty_port->read_work);
}

static int
do_vtty_write(struct tty_struct *tty, const unsigned char *buf, int count)
{
	struct vtty_port *port = tty->driver_data;
	int retval = 0;
	unsigned long flags;

	spin_lock_irqsave(&port->write_lock, flags);

	/* If we have a whole message, try to send it */
	if (port->write_buffered == 0 && count >= port->max_msg_size) {
		if (count > port->max_msg_size)
			count = port->max_msg_size;

		memcpy(&port->write_buf[sizeof(u32)], buf, count);
		retval = do_pipe_write(port, count);
		count -= retval;
	}

	/* If nothing was sent yet, buffer the data */
	if (!retval) {
		/* Determine how much data will fit in the buffer */
		if (count > port->max_msg_size - port->write_buffered)
			count = port->max_msg_size - port->write_buffered;

		/* Copy into the buffer if possible */
		if (count) {
			memcpy(&port->write_buf[sizeof(u32) +
					port->write_buffered], buf, count);
			port->write_buffered += count;
			retval = count;
		}

		/* Flush the buffer if it is full */
		if (port->write_buffered == port->max_msg_size) {
			if (do_pipe_write(port, port->write_buffered) > 0)
				port->write_buffered = 0;
		}
	}

	spin_unlock_irqrestore(&port->write_lock, flags);

	return retval;
}

static void
vtty_flush_chars(struct tty_struct *tty)
{
	struct vtty_port *port = tty->driver_data;
	unsigned long flags;

	spin_lock_irqsave(&port->write_lock, flags);

	if (port->write_buffered && do_pipe_write(port,
			port->write_buffered) > 0) {
		port->write_buffered = 0;
		tty_wakeup(tty);
	}

	spin_unlock_irqrestore(&port->write_lock, flags);
}

static int
vtty_put_char(struct tty_struct *tty, unsigned char ch)
{
	return do_vtty_write(tty, &ch, 1);
}

static int
vtty_write(struct tty_struct *tty, const unsigned char *buf, int count)
{
	int retval;

	retval = do_vtty_write(tty, buf, count);
	vtty_flush_chars(tty);

	return retval;
}

static int
vtty_write_room(struct tty_struct *tty)
{
	struct vtty_port *port = tty->driver_data;

	/*
	 * If the channel is full, we have to buffer writes locally. While
	 * vtty_write() can handle that, we may as well tell the ldisc to wait
	 * for the channel to drain, so we return 0 here.
	 */
	return port->write_full ? 0 : port->max_msg_size - port->write_buffered;
}

static int
vtty_chars_in_buffer(struct tty_struct *tty)
{
	struct vtty_port *port = tty->driver_data;

	return port->max_msg_size - vtty_write_room(tty);
}

static void
vtty_throttle(struct tty_struct *tty)
{
	struct vtty_port *port = tty->driver_data;

	port->read_throttled = true;
}

static void
vtty_unthrottle(struct tty_struct *tty)
{
	struct vtty_port *port = tty->driver_data;

	port->read_throttled = false;
	queue_work(read_workqueue, &port->read_work);
}

static const struct tty_port_operations vtty_port_ops = {
	.activate = vtty_activate,
	.shutdown = vtty_shutdown,
};

static int vtty_proc_show(struct seq_file *m, void *v)
{
	int i;

    seq_puts(m, "okl4vttyinfo:1.0 driver:1.0\n");
	for (i = 0; i < ARRAY_SIZE(ports); i++) {
		struct vtty_port *port = &ports[i];

		if (!port->exists)
			continue;
		seq_printf(m, "%d: tx_kcap: %d tx_irq: %d rx_kcap: %d rx_irq: %d\n",
				i, port->pipe_tx_kcap, port->tx_irq, port->pipe_rx_kcap, port->rx_irq);
	}

	return 0;
}

static const struct tty_operations vtty_ops = {
	.install = vtty_install,
	.open = vtty_open,
	.close = vtty_close,
	.write = vtty_write,
	.put_char = vtty_put_char,
	.flush_chars = vtty_flush_chars,
	.write_room = vtty_write_room,
	.chars_in_buffer = vtty_chars_in_buffer,
	.throttle = vtty_throttle,
	.unthrottle = vtty_unthrottle,
	.proc_show = vtty_proc_show,
};

static void
vtty_read_task(struct work_struct *work)
{
	struct vtty_port *port = container_of(work, struct vtty_port,
			read_work);
	struct tty_struct *tty = tty_port_tty_get(&port->port);
	bool pushed = false;

	if (!tty)
		return;

	while (true) {
		struct _okl4_sys_pipe_recv_return ret_recv;
		int space, len;

		/* Stop reading if we are throttled. */
		if (port->read_throttled)
			break;

		/* Find out how much space we have in the tty buffer. */
		space = tty_buffer_request_room(&port->port,
				port->max_msg_size);

		if (space == 0) {
			BUG_ON(pushed);
			tty_flip_buffer_push(&port->port);
			pushed = true;
			continue;
		} else {
			pushed = false;
		}

		if (port->read_buf_pos == port->read_buf_len) {
			/*
			 * We have run out of chars in our message buffer.
			 * Check whether there are any more messages in the
			 * queue.
			 */

			ret_recv = _okl4_sys_pipe_recv(port->pipe_rx_kcap,
					port->max_msg_size + sizeof(u32),
					(void *)port->read_buf);
			if (ret_recv.error == OKL4_ERROR_PIPE_NOT_READY) {
				okl4_pipe_control_t x = 0;

				okl4_pipe_control_setdoop(&x, true);
				okl4_pipe_control_setoperation(&x,
					OKL4_PIPE_CONTROL_OP_SET_RX_READY);
				_okl4_sys_pipe_control(port->pipe_rx_kcap, x);

				ret_recv = _okl4_sys_pipe_recv(port->pipe_rx_kcap,
						port->max_msg_size + sizeof(u32),
						(void *)port->read_buf);
			}
			if (ret_recv.error == OKL4_ERROR_PIPE_EMPTY ||
					ret_recv.error == OKL4_ERROR_PIPE_NOT_READY) {
				port->read_buf_pos = 0;
				port->read_buf_len = 0;
				break;
			}

			if (ret_recv.error != OKL4_OK) {
				dev_err(port->dev,
					"pipe receive returned error %d in vtty driver !\n",
					(int)ret_recv.error);
				port->read_buf_pos = 0;
				port->read_buf_len = 0;
				break;
			}

			port->read_buf_pos = sizeof(uint32_t);
			port->read_buf_len = sizeof(uint32_t) +
					*(uint32_t *)port->read_buf;
		}

		/* Send chars to tty layer. */
		len = port->read_buf_len - port->read_buf_pos;
		if (len > space)
			len = space;

		tty_insert_flip_string(&port->port, port->read_buf +
				port->read_buf_pos, len);
		port->read_buf_pos += len;
	}

	tty_flip_buffer_push(&port->port);

	tty_kref_put(tty);
}

static struct tty_driver *vtty_driver;

#ifdef CONFIG_OKL4_VTTY_CONSOLE
static int vconsole_setup(struct console *co, char *options);
static void vconsole_write(struct console *co, const char *p, unsigned count);
static struct tty_driver *vconsole_device(struct console *co, int *index);
#endif

static int
vtty_probe(struct platform_device *pdev)
{
	struct vtty_port *vtty_port;
	struct device *tty_dev;
	u32 reg[2];
	int vtty_id, irq, err;

	vtty_id = of_alias_get_id(pdev->dev.of_node, "vserial");
	if (vtty_id < 0)
		vtty_id = of_alias_get_id(pdev->dev.of_node, "serial");

	if (vtty_id < 0 || vtty_id >= MAX_VTTYS) {
		err = -ENXIO;
		goto fail_vtty_id;
	}

	vtty_port = &ports[vtty_id];
	if (vtty_port->exists) {
		dev_err(&pdev->dev, "vtty port already exists\n");
		err = -ENODEV;
		goto fail_vtty_id;
	}

	if (of_property_read_u32_array(pdev->dev.of_node, "reg", reg, 2)) {
		dev_err(&pdev->dev, "need 2 reg resources\n");
		err = -ENODEV;
		goto fail_vtty_id;
	}

	dev_set_drvdata(&pdev->dev, vtty_port);

	/* Set up and register the tty port */
	vtty_port->dev = &pdev->dev;
	vtty_port->vtty_id = vtty_id;
	tty_port_init(&vtty_port->port);
	vtty_port->port.ops = &vtty_port_ops;

	vtty_port->pipe_tx_kcap = reg[0];
	vtty_port->pipe_rx_kcap = reg[1];
	vtty_port->max_msg_size = MAX_MSG_SIZE;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no tx irq resource?\n");
		err = -ENODEV;
		goto fail_of;
	}
	vtty_port->tx_irq = irq;

	irq = platform_get_irq(pdev, 1);
	if (irq < 0) {
		dev_err(&pdev->dev, "no rx irq resource?\n");
		err = -ENODEV;
		goto fail_of;
	}
	vtty_port->rx_irq = irq;

	vtty_port->exists = true;

	spin_lock_init(&vtty_port->write_lock);
	INIT_WORK(&vtty_port->read_work, vtty_read_task);

	vtty_port->read_buf = kmalloc(vtty_port->max_msg_size + sizeof(u32),
		GFP_KERNEL);
	if (!vtty_port->read_buf) {
		dev_err(&pdev->dev, "%s: bad kmalloc\n", __func__);
		err = -ENOMEM;
		goto fail_malloc_read;
	}
	vtty_port->read_buf_pos = 0;
	vtty_port->read_buf_len = 0;

	vtty_port->write_buf = kmalloc(vtty_port->max_msg_size + sizeof(u32),
		GFP_KERNEL);
	if (!vtty_port->write_buf) {
		dev_err(&pdev->dev, "%s: bad kmalloc\n", __func__);
		err = -ENOMEM;
		goto fail_malloc_write;
	}

	tty_dev = tty_register_device(vtty_driver, vtty_id, &pdev->dev);
	if (IS_ERR(tty_dev)) {
		dev_err(&pdev->dev, "%s: can't register "DEVICE_NAME"%d: %ld\n",
			__func__, vtty_id, PTR_ERR(tty_dev));
		err = PTR_ERR(tty_dev);
		goto fail_tty_register;
	}

#ifdef CONFIG_OKL4_VTTY_CONSOLE
	/* Set up and register the port's console device */
	strlcpy(vtty_port->console.name, DEVICE_PREFIX,
		sizeof(vtty_port->console.name));
	vtty_port->console.write = vconsole_write;
	vtty_port->console.flags = CON_PRINTBUFFER;
	vtty_port->console.device = vconsole_device;
	vtty_port->console.setup = vconsole_setup;
	vtty_port->console.index = vtty_id;

	register_console(&vtty_port->console);
#endif

	return 0;

fail_tty_register:
	kfree(vtty_port->write_buf);
fail_malloc_write:
	kfree(vtty_port->read_buf);
	vtty_port->exists = false;
fail_of:
fail_vtty_id:
fail_malloc_read:
	dev_set_drvdata(&pdev->dev, NULL);
	return err;
}

static int
vtty_remove(struct platform_device *pdev)
{
	struct vtty_port *vtty_port = dev_get_drvdata(&pdev->dev);

	if (!vtty_port->exists)
		return -ENOENT;

#ifdef CONFIG_OKL4_VTTY_CONSOLE
	unregister_console(&vtty_port->console);
#endif
	tty_unregister_device(vtty_driver, vtty_port->vtty_id);
	vtty_port->exists = false;
	kfree(vtty_port->write_buf);
	kfree(vtty_port->read_buf);

	dev_set_drvdata(&pdev->dev, NULL);
	devm_kfree(&pdev->dev, vtty_port);

	return 0;
}

static const struct of_device_id vtty_match[] = {
	{
		.compatible = "okl,pipe-tty",
	},
	{},
};
MODULE_DEVICE_TABLE(of, vtty_match);

static struct platform_driver driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = vtty_match,
	},
	.probe		= vtty_probe,
	.remove		= vtty_remove,
};


static int __init vtty_init(void)
{
	int err;

	/* Allocate workqueue */
	read_workqueue = create_workqueue("okl4vtty");
	if (read_workqueue == NULL) {
		err = -ENOMEM;
		goto fail_create_workqueue;
	}

	/* Set up the tty driver. */
	vtty_driver = alloc_tty_driver(MAX_VTTYS);
	if (vtty_driver == NULL) {
		err = -ENOMEM;
		goto fail_alloc_tty_driver;
	}

	vtty_driver->owner = THIS_MODULE;
	vtty_driver->driver_name = DRIVER_NAME;
	vtty_driver->name = DEVICE_PREFIX;
	vtty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	vtty_driver->subtype = SERIAL_TYPE_NORMAL;
	vtty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	vtty_driver->init_termios = tty_std_termios;

	/* These flags don't really matter; just use sensible defaults. */
	vtty_driver->init_termios.c_cflag =
			B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	vtty_driver->init_termios.c_ispeed = 9600;
	vtty_driver->init_termios.c_ospeed = 9600;

	tty_set_operations(vtty_driver, &vtty_ops);

	err = tty_register_driver(vtty_driver);
	if (err)
		goto fail_tty_driver_register;

	err = platform_driver_register(&driver);
	if (err)
		goto fail_mv_driver_register;

	return 0;

fail_mv_driver_register:
	tty_unregister_driver(vtty_driver);
fail_tty_driver_register:
	put_tty_driver(vtty_driver);
	vtty_driver = NULL;
fail_alloc_tty_driver:
	destroy_workqueue(read_workqueue);
	read_workqueue = NULL;
fail_create_workqueue:
	return err;
}

static void __exit vtty_exit(void)
{
	platform_driver_unregister(&driver);

	tty_unregister_driver(vtty_driver);
	put_tty_driver(vtty_driver);
	vtty_driver = NULL;
	destroy_workqueue(read_workqueue);
	read_workqueue = NULL;
}

module_init(vtty_init);
module_exit(vtty_exit);

#ifdef CONFIG_OKL4_VTTY_CONSOLE

static u32 cycle_limit = 0;

static int
vconsole_setup(struct console *co, char *options)
{
	struct vtty_port *port;

	if (co->index < 0 || co->index >= MAX_VTTYS)
		co->index = 0;

	port = &ports[co->index];
	if (!port->exists)
		return -ENODEV;

	cycle_limit = arch_timer_get_rate() * 20 / MSEC_PER_SEC;
	if (cycle_limit == 0) {
		cycle_limit = -1;
	}
	return 0;
}

#ifdef CONFIG_OKL4_INTERLEAVED_PRIORITIES
extern int vcpu_prio_normal;
#endif

static void
vconsole_write(struct console *co, const char *p, unsigned count)
{
	struct vtty_port *port = &ports[co->index];
	size_t bytes_remaining = count;
	char buf[MAX_MSG_SIZE + sizeof(u32)];
	cycles_t last_sent_start = get_cycles();
	static int pipe_full = 0;

	memset(buf, 0, sizeof(buf));

	while (bytes_remaining > 0) {
		unsigned to_send = min(port->max_msg_size, bytes_remaining);
		unsigned send = roundup(to_send + sizeof(u32), sizeof(u32));
		okl4_error_t ret;

		*(u32 *)buf = to_send;
		memcpy(&buf[sizeof(u32)], p, to_send);

		ret = _okl4_sys_pipe_send(port->pipe_tx_kcap, send,
				(void *)buf);

		if (ret == OKL4_ERROR_PIPE_NOT_READY) {
			okl4_pipe_control_t x = 0;

			okl4_pipe_control_setdoop(&x, true);
			okl4_pipe_control_setoperation(&x,
					OKL4_PIPE_CONTROL_OP_SET_TX_READY);
			_okl4_sys_pipe_control(port->pipe_tx_kcap, x);
			continue;
		}

		if (ret == OKL4_ERROR_PIPE_FULL) {
			cycles_t last_sent_cycles = get_cycles() -
					last_sent_start;
			if (last_sent_cycles > cycle_limit || pipe_full) {
				pipe_full = 1;
				return;
			}
#ifdef CONFIG_OKL4_INTERLEAVED_PRIORITIES
			_okl4_sys_priority_waive(vcpu_prio_normal);
#else
			_okl4_sys_priority_waive(0);
#endif
			continue;
		}

		if (ret != OKL4_OK) {
			/*
			 * We cannot call printk here since that will end up
			 * calling back here and make things worse. We just
			 * have to return and hope that the problem corrects
			 * itself.
			 */
			return;
		}

		p += to_send;
		bytes_remaining -= to_send;
		last_sent_start = get_cycles();
		pipe_full = 0;
	}
}

struct tty_driver *
vconsole_device(struct console *co, int *index)
{
	*index = co->index;
	return vtty_driver;
}

#endif /* CONFIG_OKL4_VTTY_CONSOLE */

MODULE_DESCRIPTION("OKL4 virtual TTY driver");
MODULE_AUTHOR("Philip Derrin <philipd@ok-labs.com>");
