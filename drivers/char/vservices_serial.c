/*
 * drivers/char/vservice_serial.c
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * serial vservice client driver
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/bitmap.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/console.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/mutex.h>

#include <vservices/transport.h>
#include <vservices/types.h>
#include <vservices/buffer.h>
#include <vservices/protocol/serial/common.h>
#include <vservices/protocol/serial/types.h>
#include <vservices/protocol/serial/server.h>
#include <vservices/service.h>
#include <vservices/wait.h>

#include "vs_serial_common.h"

struct vtty_in_packet {
	struct vs_pbuf	pbuf;
	size_t		offset;
};

static int max_ttys = CONFIG_VSERVICES_VTTY_COUNT;
static unsigned long *alloced_ttys;
module_param(max_ttys, int, S_IRUGO);

static struct tty_driver *vtty_driver;

static DEFINE_MUTEX(tty_bitmap_lock);

static struct vtty_port *dev_to_port(struct device *dev)
{
	struct vs_service_device *service = to_vs_service_device(dev);

#if defined(CONFIG_VSERVICES_SERIAL_SERVER) || \
    defined(CONFIG_VSERIVCES_SERIAL_SERVER_MODULE)
	if (service->is_server) {
		struct vs_server_serial_state *server = dev_get_drvdata(dev);
		return container_of(server, struct vtty_port, u.vs_server);
	}
#endif
#if defined(CONFIG_VSERVICES_SERIAL_CLIENT) || \
    defined(CONFIG_VSERIVCES_SERIAL_CLIENT_MODULE)
	if (!service->is_server) {
		struct vs_client_serial_state *client = dev_get_drvdata(dev);
		return container_of(client, struct vtty_port, u.vs_client);
	}
#endif
	/* should never get here */
	WARN_ON(1);
	return NULL;
}

static struct vtty_port *port_from_tty(struct tty_struct *tty)
{
	return dev_to_port(tty->dev->parent);
}

static int vtty_install(struct tty_driver *driver, struct tty_struct *tty)
{
	struct vtty_port *port;

	if (tty->index < 0 || !test_bit(tty->index, alloced_ttys))
		return -ENXIO;

	port = port_from_tty(tty);

	if (!port)
		return -ENXIO;

	tty->driver_data = port;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	if (tty->port)
		tty->port->low_latency = 0;
#else
	tty->low_latency = 0;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
	tty_port_install(&port->port, driver, tty);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
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

static int vtty_open(struct tty_struct *tty, struct file *file)
{
	struct vtty_port *port = tty->driver_data;
	return tty_port_open(&port->port, tty, file);
}

static void vtty_close(struct tty_struct *tty, struct file *file)
{
	struct vtty_port *port = tty->driver_data;
	if (port)
		tty_port_close(&port->port, tty, file);
}

static void vtty_shutdown(struct tty_port *port)
{
	struct vtty_port *vtty_port =
			container_of(port, struct vtty_port, port);

	if (vtty_port->doing_release)
		kfree(port);
}

static int vtty_write_room(struct tty_struct *tty)
{
	struct vtty_port *port = tty->driver_data;

	return vs_service_send_mbufs_available(port->service) *
			port->max_transfer_size;
}

static struct vs_mbuf *vserial_alloc_send_buffer(struct vtty_port *port,
		const unsigned char *buf, size_t size, struct vs_pbuf *pbuf,
		gfp_t gfp_flags)
{
	struct vs_mbuf *mbuf;
	ssize_t ret;

	mbuf = port->ops.alloc_msg_buf(port, pbuf, gfp_flags);
	if (IS_ERR(mbuf)) {
		ret = PTR_ERR(mbuf);
		goto fail;
	}

	ret = vs_pbuf_resize(pbuf, size);
	if (ret < (ssize_t)size)
		goto fail_free_buf;

	ret = vs_pbuf_copyin(pbuf, 0, buf, size);
	if (ret < (ssize_t)size)
		goto fail_free_buf;

	return mbuf;

fail_free_buf:
	port->ops.free_msg_buf(port, mbuf, pbuf);
fail:
	return ERR_PTR(ret);
}

static int vtty_write(struct tty_struct *tty, const unsigned char *buf,
		int count)
{
	struct vtty_port *port;
	size_t sent_bytes = 0, size;
	struct vs_mbuf *mbuf;
	struct vs_pbuf pbuf;
	int err;

	if (WARN_ON(!tty || !buf))
		return -EINVAL;

	port = tty->driver_data;
	if (!port->ops.is_running(port)) {
		dev_dbg(&port->service->dev, "tty is not running!");
		return 0;
	}

	/*
	 * We need to break our message up into chunks of
	 * port->max_transfer_size.
	 */
	dev_dbg(&port->service->dev, "Writing %d bytes\n", count);
	while (sent_bytes < count) {
		size = min_t(size_t, count - sent_bytes,
				port->max_transfer_size);

		/*
		 * Passing &port->u.vs_client here works for both the client
		 * and the server since vs_client and vs_server are in the
		 * same union, and therefore have the same address.
		 */
		mbuf = vs_service_waiting_alloc(&port->u.vs_client,
				vserial_alloc_send_buffer(port,
				buf + sent_bytes, size, &pbuf, GFP_KERNEL));
		if (IS_ERR(mbuf)) {
			dev_err(&port->service->dev,
					"Failed to alloc mbuf of %zu bytes: %ld - resetting service\n",
					size, PTR_ERR(mbuf));
			vs_service_reset(port->service, port->service);
			return -EIO;
		}

		vs_service_state_lock(port->service);
		err = port->ops.send_msg_buf(port, mbuf, &pbuf);
		vs_service_state_unlock(port->service);
		if (err) {
			port->ops.free_msg_buf(port, mbuf, &pbuf);
			dev_err(&port->service->dev,
					"send failed: %d - resetting service",
					err);
			vs_service_reset(port->service, port->service);
			return -EIO;
		}

		dev_dbg(&port->service->dev, "Sent %zu bytes (%zu/%d)\n",
				size, sent_bytes + size, count);
		sent_bytes += size;
	}

	dev_dbg(&port->service->dev, "Write complete - sent %zu/%d bytes\n",
			sent_bytes, count);
	return sent_bytes;
}

static int vtty_put_char(struct tty_struct *tty, unsigned char ch)
{
	return vtty_write(tty, &ch, 1);
}

static size_t vs_serial_send_pbuf_to_tty(struct vtty_port *port,
		struct vs_pbuf *pbuf, size_t offset)
{
	struct tty_struct *tty = tty_port_tty_get(&port->port);
	size_t space, size;

	lockdep_assert_held(&port->in_lock);

	size = vs_pbuf_size(pbuf) - offset;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	space = tty_buffer_request_room(tty->port, size);
#else
	space = tty_buffer_request_room(tty, size);
#endif
	if (space) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
		tty_insert_flip_string(tty->port, pbuf->data + offset, space);
		tty_flip_buffer_push(tty->port);
#else
		tty_insert_flip_string(tty, pbuf->data + offset, space);
		tty_flip_buffer_push(tty);
#endif
	}

	tty_kref_put(tty);

	/* Return the number of bytes written */
	return space;
}

static void vtty_throttle(struct tty_struct *tty)
{
	struct vtty_port *port = tty->driver_data;

	dev_dbg(&port->service->dev, "throttle\n");

	spin_lock_bh(&port->in_lock);
	port->tty_canrecv = false;
	spin_unlock_bh(&port->in_lock);
}

static void vtty_unthrottle(struct tty_struct *tty)
{
	struct vtty_port *port = tty->driver_data;
	struct vtty_in_packet *packet;
	struct vs_mbuf *mbuf;
	size_t sent;

	dev_dbg(&port->service->dev, "unthrottle\n");

	spin_lock_bh(&port->in_lock);

	while (!list_empty(&port->pending_in_packets)) {
		mbuf = list_first_entry(&port->pending_in_packets,
				struct vs_mbuf, queue);
		packet = mbuf->priv;

		sent = vs_serial_send_pbuf_to_tty(port, &packet->pbuf,
				packet->offset);
		packet->offset += sent;
		if (packet->offset < vs_pbuf_size(&packet->pbuf)) {
			/*
			 * Only wrote part of the buffer. This means that we
			 * still have pending data that cannot be written to
			 * the tty at this time. The tty layer will rethrottle
			 * and this function will be called again when the tty
			 * layer is next able to handle data and we can write
			 * the remainder of the buffer.
			 */
			dev_dbg(&port->service->dev,
					"unthrottle: Only wrote %zu (%zu/%zu) bytes\n",
					sent, packet->offset,
					vs_pbuf_size(&packet->pbuf));
			break;
		}

		dev_dbg(&port->service->dev,
				"unthrottle: wrote %zu (%zu/%zu) bytes\n",
				sent, packet->offset,
				vs_pbuf_size(&packet->pbuf));

		/* Wrote the whole buffer - free it */
		list_del(&mbuf->queue);
		port->ops.free_msg_buf(port, mbuf, &packet->pbuf);
		kfree(packet);
	}

	port->tty_canrecv = true;
	spin_unlock_bh(&port->in_lock);
}

static struct tty_port_operations vtty_port_ops = {
	.shutdown	= vtty_shutdown,
};

static struct tty_operations vtty_ops = {
	.install	= vtty_install,
	.open		= vtty_open,
	.close		= vtty_close,
	.write		= vtty_write,
	.write_room	= vtty_write_room,
	.put_char	= vtty_put_char,
	.throttle	= vtty_throttle,
	.unthrottle	= vtty_unthrottle
};

static int vs_serial_queue_incoming_packet(struct vtty_port *port,
		struct vs_mbuf *mbuf, struct vs_pbuf *pbuf, size_t offset)
{
	struct vtty_in_packet *packet;

	lockdep_assert_held(&port->in_lock);

	packet = kzalloc(sizeof(*packet), GFP_ATOMIC);
	if (!packet) {
		/*
		 * Uh oh, we are seriously out of memory. The incoming data
		 * will be lost.
		 */
		return -ENOMEM;
	}

	dev_dbg(&port->service->dev, "Queuing packet %zu bytes, offset %zu\n",
			vs_pbuf_size(pbuf), offset);
	mbuf->priv = packet;
	memcpy(&packet->pbuf, pbuf, sizeof(*pbuf));
	packet->offset = offset;

	list_add_tail(&mbuf->queue, &port->pending_in_packets);
	return 0;
}

int vs_serial_handle_message(struct vtty_port *port, struct vs_mbuf *mbuf,
		struct vs_pbuf *pbuf)
{
	struct tty_struct *tty = tty_port_tty_get(&port->port);
	bool queue_packet = false;
	size_t sent = 0;
	int err;

	if (!tty) {
		dev_dbg(&port->service->dev,
				"tty not open. Dropping %zu chars\n",
				pbuf->size);
		port->ops.free_msg_buf(port, mbuf, pbuf);
		return 0;
	}

	dev_dbg(&port->service->dev, "Incoming message - len = %zu\n",
			pbuf->size);

	spin_lock(&port->in_lock);
	if (!port->tty_canrecv || !list_empty(&port->pending_in_packets)) {
		/*
		 * We cannot send to the tty right now, either because we are
		 * being throttled or because we still have pending data
		 * to write out to the tty. Queue the buffer up so we can
		 * write it later.
		 */
		dev_dbg(&port->service->dev,
				"Cannot send (canrecv = %d, queued = %d) - queuing message\n",
				port->tty_canrecv,
				!list_empty(&port->pending_in_packets));
		queue_packet = true;

	} else {
		sent = vs_serial_send_pbuf_to_tty(port, pbuf, 0);
		if (sent < vs_pbuf_size(pbuf)) {
			/*
			 * Only wrote part of the buffer to the tty. Queue
			 * the buffer to write the rest.
			 */
			dev_dbg(&port->service->dev,
					"Sent %zu/%zu bytes to tty - queueing rest\n",
					sent, vs_pbuf_size(pbuf));
			queue_packet = true;
		}
	}

	if (queue_packet) {
		/*
		 * Queue the incoming data up. If we are not already throttled,
		 * the tty layer will do so now since it has no room in its
		 * buffers.
		 */
		err = vs_serial_queue_incoming_packet(port, mbuf, pbuf, sent);
		if (err) {
			dev_err(&port->service->dev,
					"Failed to queue packet - dropping chars\n");
			port->ops.free_msg_buf(port, mbuf, pbuf);
		}

	} else {
		port->ops.free_msg_buf(port, mbuf, pbuf);
	}

	spin_unlock(&port->in_lock);
	tty_kref_put(tty);

	return 0;
}
EXPORT_SYMBOL_GPL(vs_serial_handle_message);

#ifdef CONFIG_OKL4_VTTY_CONSOLE
static int vconsole_setup(struct console *co, char *options)
{
	if (co->index < 0 || co->index >= max_ttys)
		co->index = 0;

	pr_info("OKL4 virtual console init\n");

	return 0;
}

static void vconsole_write(struct console *co, const char *p, unsigned count)
{
}

static struct tty_driver *vconsole_device(struct console *co, int *index)
{
	*index = co->index;

	return vtty_driver;
}
#endif /* CONFIG_OKL4_VTTY_CONSOLE */

static void vs_serial_free_buffers(struct vtty_port *port)
{
	struct vtty_in_packet *packet;
	struct vs_mbuf *mbuf;

	/* Free the list of incoming buffers */
	spin_lock_bh(&port->in_lock);
	while (!list_empty(&port->pending_in_packets)) {
		mbuf = list_first_entry(&port->pending_in_packets,
				struct vs_mbuf, queue);
		packet = mbuf->priv;

		list_del(&mbuf->queue);
		port->ops.free_msg_buf(port, mbuf, &packet->pbuf);
		kfree(packet);
	}
	spin_unlock_bh(&port->in_lock);
}

/** vservices callbacks **/
struct vtty_port *vs_serial_alloc_port(struct vs_service_device *service,
		struct vtty_port_ops *port_ops)
{
	struct vtty_port *port;
	int port_num;

	mutex_lock(&tty_bitmap_lock);
	port_num = find_first_zero_bit(alloced_ttys, max_ttys);

	if (port_num >= max_ttys) {
		mutex_unlock(&tty_bitmap_lock);
		return NULL;
	}

	port = kzalloc(sizeof(struct vtty_port), GFP_KERNEL);
	if (!port) {
		mutex_unlock(&tty_bitmap_lock);
		return NULL;
	}

	port->service = service;
	port->ops = *port_ops;
	port->tty_canrecv = true;
	port->port_num = port_num;
	INIT_LIST_HEAD(&port->pending_in_packets);
	spin_lock_init(&port->in_lock);
#ifdef CONFIG_OKL4_VTTY_CONSOLE
	/* Set up and register the port's console device */
	strlcpy(port->console.name, "vconvs", sizeof(port->console.name));
	port->console.write = vconsole_write;
	port->console.flags = CON_PRINTBUFFER;
	port->console.device = vconsole_device;
	port->console.setup = vconsole_setup;
	port->console.index = port_num;

	register_console(&port->console);
#endif
	port->vtty_driver = vtty_driver;

	tty_port_init(&port->port);
	port->port.ops = &vtty_port_ops;

	tty_register_device(vtty_driver, port_num, &service->dev);
	bitmap_set(alloced_ttys, port_num, 1);
	mutex_unlock(&tty_bitmap_lock);

	return port;
}
EXPORT_SYMBOL(vs_serial_alloc_port);

void vs_serial_release(struct vtty_port *port)
{
	dev_dbg(&port->service->dev, "Release\n");

#ifdef CONFIG_OKL4_VTTY_CONSOLE
	unregister_console(&port->console);
#endif

	mutex_lock(&tty_bitmap_lock);
	bitmap_clear(alloced_ttys, port->port_num, 1);
	mutex_unlock(&tty_bitmap_lock);

	if (port->port.tty) {
		tty_vhangup(port->port.tty);
		tty_kref_put(port->port.tty);
	}

	vs_serial_free_buffers(port);
	port->doing_release = true;
	tty_unregister_device(vtty_driver, port->port_num);
}
EXPORT_SYMBOL_GPL(vs_serial_release);

void vs_serial_reset(struct vtty_port *port)
{
	/* Free list of in and out mbufs. */
	vs_serial_free_buffers(port);
}
EXPORT_SYMBOL_GPL(vs_serial_reset);

static int __init vs_serial_init(void)
{
	int err;

	if (max_ttys == 0)
		return -EINVAL;

	alloced_ttys = kzalloc(sizeof(unsigned long) * BITS_TO_LONGS(max_ttys),
			GFP_KERNEL);
	if (!alloced_ttys) {
		err = -ENOMEM;
		goto fail_alloc_ttys;
	}

	/* Set up the tty driver. */
	vtty_driver = alloc_tty_driver(max_ttys);
	if (!vtty_driver) {
		err = -ENOMEM;
		goto fail_alloc_tty_driver;
	}

	vtty_driver->owner = THIS_MODULE;
	vtty_driver->driver_name = "okl4-vservices-serial";
	vtty_driver->name = "ttyVS";
	vtty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	vtty_driver->subtype = SERIAL_TYPE_NORMAL;
	vtty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	vtty_driver->init_termios = tty_std_termios;
	vtty_driver->num = max_ttys;

	/* These flags don't really matter; just use sensible defaults. */
	vtty_driver->init_termios.c_cflag =
			B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	vtty_driver->init_termios.c_ispeed = 9600;
	vtty_driver->init_termios.c_ospeed = 9600;

	tty_set_operations(vtty_driver, &vtty_ops);

	err = tty_register_driver(vtty_driver);
	if (err)
		goto fail_tty_driver_register;

	return 0;

fail_tty_driver_register:
	put_tty_driver(vtty_driver);
fail_alloc_tty_driver:
	kfree(alloced_ttys);
fail_alloc_ttys:
	return err;
}

static void __exit vs_serial_exit(void)
{
	tty_unregister_driver(vtty_driver);
	put_tty_driver(vtty_driver);
}

module_init(vs_serial_init);
module_exit(vs_serial_exit);

MODULE_DESCRIPTION("OKL4 Virtual Services Serial Core Driver");
MODULE_AUTHOR("Open Kernel Labs, Inc");
