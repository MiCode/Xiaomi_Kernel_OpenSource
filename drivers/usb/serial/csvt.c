/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/tty_flip.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/usb/cdc.h>
#include <linux/usb/serial.h>
#include <asm/unaligned.h>


/* output control lines*/
#define CSVT_CTRL_DTR		0x01
#define CSVT_CTRL_RTS		0x02

/* input control lines*/
#define CSVT_CTRL_CTS		0x01
#define CSVT_CTRL_DSR		0x02
#define CSVT_CTRL_RI		0x04
#define CSVT_CTRL_CD		0x08

static int debug;
module_param(debug, int, S_IRUGO | S_IWUSR);

struct csvt_ctrl_dev {
	struct mutex		dev_lock;

	/* input control lines (DSR, CTS, CD, RI) */
	unsigned int		cbits_tolocal;

	/* output control lines (DTR, RTS) */
	unsigned int		cbits_tomdm;
};

static const struct usb_device_id id_table[] = {
	{ USB_DEVICE_AND_INTERFACE_INFO(0x05c6 , 0x904c, 0xff, 0xfe, 0xff)},
	{ USB_DEVICE_AND_INTERFACE_INFO(0x05c6 , 0x9075, 0xff, 0xfe, 0xff)},
	{}, /* terminating entry */
};
MODULE_DEVICE_TABLE(usb, id_table);

static struct usb_driver csvt_driver = {
	.name			= "qc_csvt",
	.probe			= usb_serial_probe,
	.disconnect		= usb_serial_disconnect,
	.id_table		= id_table,
	.suspend		= usb_serial_suspend,
	.resume			= usb_serial_resume,
	.supports_autosuspend	= true,
};

#define CSVT_IFC_NUM	4

static int csvt_probe(struct usb_serial *serial, const struct usb_device_id *id)
{
	struct usb_host_interface	*intf =
		serial->interface->cur_altsetting;

	pr_debug("%s:\n", __func__);

	if (intf->desc.bInterfaceNumber != CSVT_IFC_NUM)
		return -ENODEV;

	usb_enable_autosuspend(serial->dev);

	return 0;
}

static int csvt_ctrl_write_cmd(struct csvt_ctrl_dev	*dev,
	struct usb_serial_port *port)
{
	struct usb_device	*udev = port->serial->dev;
	struct usb_interface	*iface = port->serial->interface;
	unsigned int		iface_num;
	int			retval = 0;

	retval = usb_autopm_get_interface(iface);
	if (retval < 0) {
		dev_err(&port->dev, "%s: Unable to resume interface: %d\n",
			__func__, retval);
		return retval;
	}

	dev_dbg(&port->dev, "%s: cbits to mdm 0x%x\n", __func__,
		dev->cbits_tomdm);

	iface_num = iface->cur_altsetting->desc.bInterfaceNumber;

	retval = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
		USB_CDC_REQ_SET_CONTROL_LINE_STATE,
		(USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE),
		dev->cbits_tomdm,
		iface_num,
		NULL, 0, USB_CTRL_SET_TIMEOUT);
	usb_autopm_put_interface(iface);

	return retval;
}

static void csvt_ctrl_dtr_rts(struct usb_serial_port *port, int on)
{
	struct csvt_ctrl_dev	*dev = usb_get_serial_port_data(port);

	if (!dev)
		return;

	dev_dbg(&port->dev, "%s", __func__);

	mutex_lock(&dev->dev_lock);
	if (on) {
		dev->cbits_tomdm |= CSVT_CTRL_DTR;
		dev->cbits_tomdm |= CSVT_CTRL_RTS;
	} else {
		dev->cbits_tomdm &= ~CSVT_CTRL_DTR;
		dev->cbits_tomdm &= ~CSVT_CTRL_RTS;
	}
	mutex_unlock(&dev->dev_lock);

	csvt_ctrl_write_cmd(dev, port);
}

static int get_serial_info(struct usb_serial_port *port,
			   struct serial_struct __user *retinfo)
{
	struct serial_struct	tmp;

	if (!retinfo)
		return -EFAULT;

	memset(&tmp, 0, sizeof(tmp));
	tmp.line            = port->serial->minor;
	tmp.port            = port->number;
	tmp.baud_base       = tty_get_baud_rate(port->port.tty);
	tmp.close_delay	    = port->port.close_delay / 10;
	tmp.closing_wait    =
		port->port.closing_wait == ASYNC_CLOSING_WAIT_NONE ?
				 ASYNC_CLOSING_WAIT_NONE :
				 port->port.closing_wait / 10;

	if (copy_to_user(retinfo, &tmp, sizeof(*retinfo)))
		return -EFAULT;
	return 0;
}

static int set_serial_info(struct usb_serial_port *port,
			   struct serial_struct __user *newinfo)
{
	struct serial_struct	new_serial;
	unsigned int		closing_wait;
	unsigned int		close_delay;
	int			retval = 0;

	if (copy_from_user(&new_serial, newinfo, sizeof(new_serial)))
		return -EFAULT;

	close_delay = new_serial.close_delay * 10;
	closing_wait = new_serial.closing_wait == ASYNC_CLOSING_WAIT_NONE ?
			ASYNC_CLOSING_WAIT_NONE : new_serial.closing_wait * 10;

	mutex_lock(&port->port.mutex);

	if (!capable(CAP_SYS_ADMIN)) {
		if ((close_delay != port->port.close_delay) ||
		    (closing_wait != port->port.closing_wait))
			retval = -EPERM;
		else
			retval = -EOPNOTSUPP;
	} else {
		port->port.close_delay  = close_delay;
		port->port.closing_wait = closing_wait;
	}

	mutex_unlock(&port->port.mutex);
	return retval;
}

static int csvt_ctrl_ioctl(struct tty_struct *tty, unsigned int cmd,
	unsigned long arg)
{
	struct usb_serial_port	*port = tty->driver_data;

	dev_dbg(&port->dev, "%s cmd 0x%04x", __func__, cmd);

	switch (cmd) {
	case TIOCGSERIAL:
		return get_serial_info(port,
				       (struct serial_struct __user *) arg);
	case TIOCSSERIAL:
		return set_serial_info(port,
				       (struct serial_struct __user *) arg);
	default:
		break;
	}

	dev_err(&port->dev, "%s arg not supported", __func__);

	return -ENOIOCTLCMD;
}

static int csvt_ctrl_tiocmget(struct tty_struct *tty)
{
	struct usb_serial_port	*port = tty->driver_data;
	struct csvt_ctrl_dev	*dev = usb_get_serial_port_data(port);
	unsigned int		control_state = 0;

	if (!dev)
		return -ENODEV;

	mutex_lock(&dev->dev_lock);
	control_state = (dev->cbits_tomdm & CSVT_CTRL_DTR ? TIOCM_DTR : 0) |
		(dev->cbits_tomdm & CSVT_CTRL_RTS ? TIOCM_RTS : 0) |
		(dev->cbits_tolocal & CSVT_CTRL_DSR ? TIOCM_DSR : 0) |
		(dev->cbits_tolocal & CSVT_CTRL_RI ? TIOCM_RI : 0) |
		(dev->cbits_tolocal & CSVT_CTRL_CD ? TIOCM_CD : 0) |
		(dev->cbits_tolocal & CSVT_CTRL_CTS ? TIOCM_CTS : 0);
	mutex_unlock(&dev->dev_lock);

	dev_dbg(&port->dev, "%s -- %x", __func__, control_state);

	return control_state;
}

static int csvt_ctrl_tiocmset(struct tty_struct *tty,
			       unsigned int set, unsigned int clear)
{
	struct usb_serial_port	*port = tty->driver_data;
	struct csvt_ctrl_dev	*dev = usb_get_serial_port_data(port);

	if (!dev)
		return -ENODEV;

	dev_dbg(&port->dev, "%s\n", __func__);

	mutex_lock(&dev->dev_lock);
	if (set & CSVT_CTRL_DTR)
		dev->cbits_tomdm |= TIOCM_DTR;
	if (set & CSVT_CTRL_RTS)
		dev->cbits_tomdm |= TIOCM_RTS;

	if (clear & CSVT_CTRL_DTR)
		dev->cbits_tomdm &= ~TIOCM_DTR;
	if (clear & CSVT_CTRL_RTS)
		dev->cbits_tomdm &= ~TIOCM_RTS;
	mutex_unlock(&dev->dev_lock);

	return csvt_ctrl_write_cmd(dev, port);
}

static void csvt_ctrl_set_termios(struct tty_struct *tty,
			  struct usb_serial_port *port,
			  struct ktermios *old_termios)
{
	struct csvt_ctrl_dev	*dev = usb_get_serial_port_data(port);

	if (!dev)
		return;

	dev_dbg(&port->dev, "%s", __func__);

	/* Doesn't support option setting */
	tty_termios_copy_hw(tty->termios, old_termios);

	csvt_ctrl_write_cmd(dev, port);
}

static void csvt_ctrl_int_cb(struct urb *urb)
{
	int				status;
	struct usb_cdc_notification	*ctrl;
	struct usb_serial_port		*port = urb->context;
	struct csvt_ctrl_dev		*dev;
	unsigned int			ctrl_bits;
	unsigned char			*data;

	switch (urb->status) {
	case 0:
		/*success*/
		break;
	case -ESHUTDOWN:
	case -ENOENT:
	case -ECONNRESET:
	case -EPROTO:
		 /* unplug */
		 return;
	case -EPIPE:
		dev_err(&port->dev, "%s: stall on int endpoint\n", __func__);
		/* TBD : halt to be cleared in work */
	case -EOVERFLOW:
	default:
		pr_debug_ratelimited("%s: non zero urb status = %d\n",
					__func__, urb->status);
		goto resubmit_int_urb;
	}

	dev = usb_get_serial_port_data(port);
	if (!dev)
		return;

	ctrl = urb->transfer_buffer;
	data = (unsigned char *)(ctrl + 1);

	usb_serial_debug_data(debug, &port->dev, __func__,
					urb->actual_length, data);

	switch (ctrl->bNotificationType) {
	case USB_CDC_NOTIFY_NETWORK_CONNECTION:
		dev_dbg(&port->dev, "%s network\n", ctrl->wValue ?
					"connected to" : "disconnected from");
		break;
	case USB_CDC_NOTIFY_SERIAL_STATE:
		ctrl_bits = get_unaligned_le16(data);
		dev_dbg(&port->dev, "serial state: %d\n", ctrl_bits);
		dev->cbits_tolocal = ctrl_bits;
		break;
	default:
		dev_err(&port->dev, "%s: unknown notification %d received:"
			"index %d len %d data0 %d data1 %d",
			__func__, ctrl->bNotificationType, ctrl->wIndex,
			ctrl->wLength, data[0], data[1]);
	}

resubmit_int_urb:
	status = usb_submit_urb(urb, GFP_ATOMIC);
	if (status)
		dev_err(&port->dev, "%s: Error re-submitting Int URB %d\n",
		__func__, status);

}

static int csvt_ctrl_open(struct tty_struct *tty,
					struct usb_serial_port *port)
{
	int	retval;

	dev_dbg(&port->dev, "%s port %d", __func__, port->number);

	retval = usb_submit_urb(port->interrupt_in_urb, GFP_KERNEL);
	if (retval) {
		dev_err(&port->dev, "usb_submit_urb(read int) failed\n");
		return retval;
	}

	retval = usb_serial_generic_open(tty, port);
	if (retval)
		usb_kill_urb(port->interrupt_in_urb);

	return retval;
}

static void csvt_ctrl_close(struct usb_serial_port *port)
{
	dev_dbg(&port->dev, "%s port %d", __func__, port->number);

	usb_serial_generic_close(port);
	usb_kill_urb(port->interrupt_in_urb);
}

static int csvt_ctrl_attach(struct usb_serial *serial)
{
	struct csvt_ctrl_dev	*dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	mutex_init(&dev->dev_lock);
	usb_set_serial_port_data(serial->port[0], dev);

	return 0;
}

static void csvt_ctrl_release(struct usb_serial *serial)
{
	struct usb_serial_port	*port = serial->port[0];
	struct csvt_ctrl_dev	*dev = usb_get_serial_port_data(port);

	dev_dbg(&port->dev, "%s", __func__);

	kfree(dev);
	usb_set_serial_port_data(port, NULL);
}

static struct usb_serial_driver csvt_device = {
	.driver			= {
		.owner	= THIS_MODULE,
		.name	= "qc_csvt",
	},
	.description		= "qc_csvt",
	.id_table		= id_table,
	.num_ports		= 1,
	.open			= csvt_ctrl_open,
	.close			= csvt_ctrl_close,
	.probe			= csvt_probe,
	.dtr_rts		= csvt_ctrl_dtr_rts,
	.tiocmget		= csvt_ctrl_tiocmget,
	.tiocmset		= csvt_ctrl_tiocmset,
	.ioctl			= csvt_ctrl_ioctl,
	.set_termios		= csvt_ctrl_set_termios,
	.read_int_callback	= csvt_ctrl_int_cb,
	.attach			= csvt_ctrl_attach,
	.release		= csvt_ctrl_release,
};

static struct usb_serial_driver * const serial_drivers[] = {
	&csvt_device,
	NULL,
};

static int __init csvt_init(void)
{
	int	retval;

	retval = usb_serial_register_drivers(&csvt_driver, serial_drivers);
	if (retval) {
		err("%s: usb serial register failed\n", __func__);
		return retval;
	}

	return 0;
}

static void __exit csvt_exit(void)
{
	usb_serial_deregister_drivers(&csvt_driver, serial_drivers);
}

module_init(csvt_init);
module_exit(csvt_exit);

MODULE_LICENSE("GPL v2");
