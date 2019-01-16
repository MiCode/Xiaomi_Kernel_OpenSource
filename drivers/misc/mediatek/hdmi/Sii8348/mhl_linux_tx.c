/*

SiI8348 Linux Driver

Copyright (C) 2013 Silicon Image, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation version 2.
This program is distributed AS-IS WITHOUT ANY WARRANTY of any
kind, whether express or implied; INCLUDING without the implied warranty
of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.  See 
the GNU General Public License for more details at http://www.gnu.org/licenses/gpl-2.0.html.             

*/
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/stringify.h>
#include <asm/uaccess.h>

#include "sii_hal.h"
#include "si_fw_macros.h"
#include "si_mhl_defs.h"
#include "si_infoframe.h"
#include "si_edid.h"
#include "si_mhl2_edid_3d_api.h"
#include "si_mhl_tx_hw_drv_api.h"
#ifdef MEDIA_DATA_TUNNEL_SUPPORT
#include "si_mdt_inputdev.h"
#endif
#ifdef RCP_INPUTDEV_SUPPORT
#include "mhl_rcp_inputdev.h"
#endif
#include "mhl_linux_tx.h"
#include "mhl_supp.h"
#include "platform.h"

#include <linux/kthread.h>
#include <mach/irqs.h>
#include "mach/eint.h"
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <cust_eint.h>
#include "hdmi_drv.h"
#include "smartbook.h"

#define MHL_DRIVER_MINOR_MAX 1
static wait_queue_head_t mhl_irq_wq;
static struct task_struct *mhl_irq_task = NULL;
static atomic_t mhl_irq_event = ATOMIC_INIT(0);


/************************** MHL TX User Layer To HAL****************************************/
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
extern int smartbook_kthread(void *data);
extern wait_queue_head_t smartbook_wq;
static struct task_struct *smartbook_task = NULL;
#endif

void Notify_AP_MHL_TX_Event(unsigned int event, unsigned int event_param, void *param);
/************************** ****************************************************/


struct mhl_dev_context *si_dev_context;


static char *white_space = "' ', '\t'";
static dev_t dev_num;

static struct class *mhl_class;

static void mhl_tx_destroy_timer_support(struct  mhl_dev_context *dev_context);

/* Define SysFs attribute names */
#define SYS_ATTR_NAME_CONN				connection_state
#define SYS_ATTR_NAME_RCP				rcp_keycode
#define SYS_ATTR_NAME_RCPK				rcp_ack
#define SYS_ATTR_NAME_RAP				rap
#define SYS_ATTR_NAME_RAP_STATUS			rap_status
#define SYS_ATTR_NAME_DEVCAP				devcap
#define SYS_ATTR_NAME_UCP				ucp_keycode
#define SYS_ATTR_NAME_UCPK				ucp_ack
#define SYS_ATTR_NAME_SPAD				spad
#define SYS_ATTR_NAME_DEBUG				debug
#define SYS_ATTR_NAME_TRACE_LEVEL			trace_level


/*
 * show_connection_state() - Handle read request to the connection_state
 * 							 attribute file.
 */
ssize_t show_connection_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);

	if (dev_context->mhl_flags & MHL_STATE_FLAG_CONNECTED) {
		return scnprintf(buf, PAGE_SIZE, "connected");
	} else {
		return scnprintf(buf, PAGE_SIZE, "not connected");
	}
}

#ifndef RCP_INPUTDEV_SUPPORT
/*
 * show_rcp() - Handle read request to the rcp_keycode attribute file.
 */
ssize_t show_rcp(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = 0;

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->mhl_flags &
		(MHL_STATE_FLAG_RCP_SENT | MHL_STATE_FLAG_RCP_RECEIVED)) {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x %s",
				dev_context->rcp_key_code,
				dev_context->mhl_flags & MHL_STATE_FLAG_RCP_SENT? "sent" : "received");
	}

	up(&dev_context->isr_lock);

	return status;
}


/*
 * send_rcp() - Handle write request to the rcp_keycode attribute file.
 */
ssize_t send_rcp(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	unsigned long key_code;
	int status = -EINVAL;

	MHL_TX_DBG_INFO(dev_context, "send_rcp received string: ""%s""\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}

	if (!(dev_context->mhl_flags & MHL_STATE_FLAG_CONNECTED))
		goto err_exit;

	if (strict_strtoul(buf, 0, &key_code)) {
		MHL_TX_DBG_ERR(dev_context, "Unable to convert key code string\n");
		goto err_exit;
	}

	if (key_code >= 0xFE) {
		MHL_TX_DBG_ERR(dev_context, "key code (0x%lx) is too large to be valid\n", key_code);
		goto err_exit;
	}

	dev_context->mhl_flags &= ~(MHL_STATE_FLAG_RCP_RECEIVED |
			MHL_STATE_FLAG_RCP_ACK |
			MHL_STATE_FLAG_RCP_NAK);
	dev_context->mhl_flags |= MHL_STATE_FLAG_RCP_SENT;
	dev_context->rcp_send_status = 0;
	dev_context->rcp_key_code = (u8)key_code;
	if (!si_mhl_tx_rcp_send(dev_context, (u8)key_code))
		goto err_exit;

	status = count;

err_exit:
	up(&dev_context->isr_lock);

	return status;
}


/*
 * send_rcp_ack() - Handle write request to the rcp_ack attribute file.
 *
 * This file is used to send either an ACK or NAK for a received
 * Remote Control Protocol (RCP) key code.
 *
 * The format of the string in buf must be:
 * 	"keycode=<keyvalue> errorcode=<errorvalue>
 * 	where:	<keyvalue>		is replaced with value of the RCP to be ACK'd or NAK'd
 * 			<errorvalue>	0 if the RCP key code is to be ACK'd
 * 							non-zero error code if the RCP key code is to be NAK'd
 */
ssize_t send_rcp_ack(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	unsigned long	key_code = 0x100;	/* initialize with invalid values */
	unsigned long	err_code = 0x100;
	char		*pStr;
	int		status = -EINVAL;

	MHL_TX_DBG_INFO(dev_context, "received string: %s\n", buf);

	/* Parse the input string and extract the RCP key code and error code */
	pStr = strstr(buf, "keycode=");
	if (pStr != NULL) {
		key_code = simple_strtoul(pStr + 8, NULL, 0);
		if (key_code > 0xFF) {
			MHL_TX_DBG_ERR(dev_context, "Unable to convert keycode string\n");
			goto err_exit_2;
		}
	} else {
		MHL_TX_DBG_ERR(dev_context, "Invalid string format, can't find ""keycode"" value\n");
		goto err_exit_2;
	}

	pStr = strstr(buf, "errorcode=");
	if (pStr != NULL) {
		if(strict_strtoul(pStr + 10, 0, &err_code)) {
			MHL_TX_DBG_ERR(dev_context, "Unable to convert errorcode string\n");
			goto err_exit_2;
		}
	} else {
		MHL_TX_DBG_ERR(dev_context, "Invalid string format, can't find ""errorcode"" value\n");
		goto err_exit_2;
	}

	if ((key_code > 0xFF) || (err_code > 0xFF)) {
		MHL_TX_DBG_ERR(dev_context, "Invalid key code or error code "\
				"specified, key code: 0x%02lx  error code: 0x%02lx\n",
				key_code, err_code);
		goto err_exit_2;
	}

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit_1;
	}

	if (dev_context->mhl_flags & MHL_STATE_FLAG_CONNECTED) {

		if((key_code != dev_context->rcp_key_code)
				|| !(dev_context->mhl_flags & MHL_STATE_FLAG_RCP_RECEIVED)) {

			MHL_TX_DBG_ERR(dev_context, "Attempting to ACK a key code "\
					"that was not received! try:0x%02x(%d)\n"
					,dev_context->rcp_key_code
					,dev_context->rcp_key_code);
			goto err_exit_1;
		}

		if (err_code == 0) {
			if (!si_mhl_tx_rcpk_send(dev_context, (u8)key_code)) {
				status = -ENOMEM;
				goto err_exit_1;
			}
		} else {
			if (!si_mhl_tx_rcpe_send(dev_context, (u8)err_code))
				goto err_exit_1;
		}

		status = count;
	}

err_exit_1:
	up(&dev_context->isr_lock);

err_exit_2:
	return status;
}


/*
 * show_rcp_ack() - Handle read request to the rcp_ack attribute file.
 *
 * Reads from this file return a string detailing the last RCP
 * ACK or NAK received by the driver.
 *
 * The format of the string returned in buf is:
 * 	"keycode=<keyvalue> errorcode=<errorvalue>
 * 	where:	<keyvalue>		is replaced with value of the RCP key code for which
 * 							an ACK or NAK has been received.
 * 			<errorvalue>	0 if the last RCP key code was ACK'd or
 * 							non-zero error code if the RCP key code was NAK'd
 */
ssize_t show_rcp_ack(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	MHL_TX_DBG_INFO(dev_context, "called\n");

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->mhl_flags & (MHL_STATE_FLAG_RCP_ACK | MHL_STATE_FLAG_RCP_NAK)) {

		status = scnprintf(buf, PAGE_SIZE, "keycode=0x%02x errorcode=0x%02x",
				dev_context->rcp_key_code, dev_context->rcp_err_code);
	}

	up(&dev_context->isr_lock);

	return status;
}
#endif /* #ifndef RCP_INPUTDEV_SUPPORT */

/*
 * show_ucp() - Handle read request to the ucp_keycode attribute file.
 */
ssize_t show_ucp(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	int status = 0;

	MHL_TX_DBG_INFO(dev_context, "called keycode:0x%02x\n",dev_context->ucp_key_code);
	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->mhl_flags &
			(MHL_STATE_FLAG_UCP_SENT | MHL_STATE_FLAG_UCP_RECEIVED)) {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x %s",
				dev_context->ucp_key_code,
				dev_context->mhl_flags & MHL_STATE_FLAG_UCP_SENT? "sent" : "received");
	}

	up(&dev_context->isr_lock);

	return status;
}


/*
 * send_ucp() - Handle write request to the ucp_keycode attribute file.
 */
ssize_t send_ucp(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	unsigned long key_code;
	int status = -EINVAL;

	MHL_TX_DBG_INFO(dev_context, "received string: ""%s""\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}

	if (!(dev_context->mhl_flags & MHL_STATE_FLAG_CONNECTED))
		goto err_exit;

	if (strict_strtoul(buf, 0, &key_code)) {
		MHL_TX_DBG_ERR(dev_context, "Unable to convert key code string\n");
		goto err_exit;
	}

	if (key_code > 0xFF) {
		MHL_TX_DBG_ERR(dev_context, "ucp key code (0x%lx) is too large to be valid\n", key_code);
		goto err_exit;
	}

	dev_context->mhl_flags &= ~(MHL_STATE_FLAG_UCP_RECEIVED |
			MHL_STATE_FLAG_UCP_ACK |
			MHL_STATE_FLAG_UCP_NAK);
	dev_context->mhl_flags |= MHL_STATE_FLAG_UCP_SENT;
	dev_context->ucp_key_code = (u8)key_code;
	if (!si_mhl_tx_ucp_send(dev_context, (u8)key_code))
		goto err_exit;

	status = count;

err_exit:
	up(&dev_context->isr_lock);

	return status;
}


/*
 * send_ucp_ack() - Handle write request to the ucp_ack attribute file.
 *
 * This file is used to send either an ACK or NAK for a received
 * UTF-8 Control Protocol (UCP) key code.
 *
 * The format of the string in buf must be:
 * 	"keycode=<keyvalue> errorcode=<errorvalue>
 * 	where:	<keyvalue>		is replaced with value of the UCP to be ACK'd or NAK'd
 * 			<errorvalue>	0 if the UCP key code is to be ACK'd
 * 							non-zero error code if the UCP key code is to be NAK'd
 */
ssize_t send_ucp_ack(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	unsigned long	key_code = 0x100;	/* initialize with invalid values */
	unsigned long	err_code = 0x100;
	char		*pStr;
	int		status = -EINVAL;

	MHL_TX_DBG_INFO(dev_context, "received string: %s\n", buf);

	/* Parse the input string and extract the UCP key code and error code */
	pStr = strstr(buf, "keycode=");
	if (pStr != NULL) {
		key_code = simple_strtoul(pStr + 8, NULL, 0);
		if (key_code > 0xFF) {
			MHL_TX_DBG_ERR(dev_context, "Unable to convert keycode string\n");
			goto err_exit_2;
		}
	} else {
		MHL_TX_DBG_ERR(dev_context, "Invalid string format, can't find ""keycode"" value\n");
		goto err_exit_2;
	}

	pStr = strstr(buf, "errorcode=");
	if (pStr != NULL) {
		if(strict_strtoul(pStr + 10, 0, &err_code)) {
			MHL_TX_DBG_ERR(dev_context, "Unable to convert errorcode string\n");
			goto err_exit_2;
		}
	} else {
		MHL_TX_DBG_ERR(dev_context, "Invalid string format, can't find ""errorcode"" value\n");
		goto err_exit_2;
	}

	if ((key_code > 0xFF) || (err_code > 0xFF)) {
		MHL_TX_DBG_ERR(dev_context, "Invalid key code or error code "\
				"specified, key code: 0x%02lx  error code: 0x%02lx\n",
				key_code, err_code);
		goto err_exit_2;
	}

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit_1;
	}

	if (dev_context->mhl_flags & MHL_STATE_FLAG_CONNECTED) {

		if((key_code != dev_context->ucp_key_code)
			|| !(dev_context->mhl_flags & MHL_STATE_FLAG_UCP_RECEIVED)) {

			MHL_TX_DBG_ERR(dev_context, "Attempting to ACK a key code that was not received!\n");
			goto err_exit_1;
		}

		if (err_code == 0) {
			if (!si_mhl_tx_ucpk_send(dev_context, (u8)key_code)) {
				status = -ENOMEM;
				goto err_exit_1;
			}
		} else {
			if (!si_mhl_tx_ucpe_send(dev_context, (u8)err_code)) {
				status = -ENOMEM;
				goto err_exit_1;
			}
		}

		status = count;
	}

err_exit_1:
	up(&dev_context->isr_lock);

err_exit_2:
	return status;
}


/*
 * show_ucp_ack() - Handle read request to the ucp_ack attribute file.
 *
 * Reads from this file return a string detailing the last UCP
 * ACK or NAK received by the driver.
 *
 * The format of the string returned in buf is:
 * 	"keycode=<keyvalue> errorcode=<errorvalue>
 * 	where:	<keyvalue>		is replaced with value of the UCP key code for which
 * 							an ACK or NAK has been received.
 * 			<errorvalue>	0 if the last UCP key code was ACK'd or
 * 							non-zero error code if the UCP key code was NAK'd
 */
ssize_t show_ucp_ack(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	MHL_TX_DBG_INFO(dev_context, "called\n");

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->mhl_flags & (MHL_STATE_FLAG_UCP_ACK | MHL_STATE_FLAG_UCP_NAK)) {

		status = scnprintf(buf, PAGE_SIZE, "keycode=0x%02x errorcode=0x%02x",
				dev_context->ucp_key_code, dev_context->ucp_err_code);
	}

	up(&dev_context->isr_lock);

	return status;
}

/*
 * show_rap() - Handle read request to the rap attribute file.
 *
 * Reads from this file return a string value indicating the last
 * Request Action Protocol (RAP) request received.
 *
 * The return value is the number characters written to buf, or EAGAIN
 * if the driver is busy and cannot service the read request immediately.
 * If EAGAIN is returned the caller should wait a little and retry the
 * read.
 */
ssize_t show_rap(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	MHL_TX_DBG_INFO(dev_context, "called last sub-command:0x%02x\n",dev_context->rap_sub_command);

	if (down_interruptible(&dev_context->isr_lock)){
		MHL_TX_DBG_ERR(dev_context,"-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR(dev_context,"-ENODEV\n");
		status = -ENODEV;
		goto err_exit;
	}
	*buf = '\0';
	if (MHL_RAP_POLL == dev_context->rap_sub_command)
		status = scnprintf(buf, PAGE_SIZE, "poll");
	else if (MHL_RAP_CONTENT_ON ==  dev_context->rap_sub_command)
		status = scnprintf(buf, PAGE_SIZE, "content_on");
	else if (MHL_RAP_CONTENT_OFF ==  dev_context->rap_sub_command)
		status = scnprintf(buf, PAGE_SIZE, "content_off");
	MHL_TX_DBG_INFO(dev_context,"buf:%c%s%c\n",'"',buf,'"');

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * send_rap() - Handle write request to the rap attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t send_rap(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO(dev_context, "received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}

	if (strnicmp("poll", buf, count - 1) == 0) {
		if (!si_mhl_tx_rap_send(dev_context, MHL_RAP_POLL))
			status = -EPERM;

	} else if (strnicmp("content_on", buf, count - 1) == 0) {
		if (!si_mhl_tx_rap_send(dev_context, MHL_RAP_CONTENT_ON))
			status = -EPERM;

	} else if (strnicmp("content_off", buf, count - 1) == 0) {
		if (!si_mhl_tx_rap_send(dev_context, MHL_RAP_CONTENT_OFF))
			status = -EPERM;

	} else {
		MHL_TX_DBG_ERR(dev_context, "Invalid parameter %s received\n", buf);
		status = -EINVAL;
	}

err_exit:
	up(&dev_context->isr_lock);

	return status;
}
//( begin rap_status interface
/*
 * show_rap_status() - Handle read request to the rap_status attribute file.
 *
 * Reads from this file return the last setting from the customer application
 */
ssize_t show_rap_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;


	MHL_TX_DBG_INFO(dev_context, "called last sub-command:0x%02x\n",dev_context->rap_sub_command);

	if (down_interruptible(&dev_context->isr_lock)){
		MHL_TX_DBG_ERR(dev_context,"-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR(dev_context,"-ENODEV\n");
		status = -ENODEV;
		goto err_exit;
	}
	*buf = '\0';
	if (dev_context->mhl_flags & MHL_STATE_APPLICATION_RAP_BUSY){
		status = scnprintf(buf, PAGE_SIZE, "busy");
	}else{
		status = scnprintf(buf, PAGE_SIZE, "ready");
	}
	MHL_TX_DBG_INFO(dev_context,"buf:%c%s%c\n",'"',buf,'"');

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * set_rap_status() - Handle write request to the rap attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t set_rap_status(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO(dev_context, "received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}

	if (strnicmp("busy", buf, count - 1) == 0) {
		dev_context->mhl_flags |= MHL_STATE_APPLICATION_RAP_BUSY;
	} else if (strnicmp("ready", buf, count - 1) == 0) {
		dev_context->mhl_flags &= ~MHL_STATE_APPLICATION_RAP_BUSY;
	} else {
		MHL_TX_DBG_ERR(dev_context, "Invalid parameter %s received\n", buf);
		status = -EINVAL;
	}

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

//) end rap_status interface

/*
 * select_dev_cap() - Handle write request to the devcap attribute file.
 *
 * Writes to the devcap file are done to set the offset of a particular
 * Device Capabilities register to be returned by a subsequent read
 * from this file.
 *
 * All we need to do is validate the specified offset and if valid
 * save it for later use.
 */
ssize_t select_dev_cap(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	unsigned long		offset;
	int			status = -EINVAL;


	MHL_TX_DBG_INFO(dev_context, "received string: ""%s""\n", buf);

	if (strict_strtoul(buf, 0, &offset)) {
		MHL_TX_DBG_ERR(dev_context, "Unable to convert register offset string\n");
		goto err_exit;
	}

	if (offset > 0x0F) {
		MHL_TX_DBG_INFO(dev_context,
				"dev cap offset (0x%lx) is too large to be valid\n", offset);
		goto err_exit;
	}

	dev_context->dev_cap_offset = (u8)offset;
	status = count;

err_exit:
	return status;
}


/*
 * show_dev_cap() - Handle read request to the devcap attribute file.
 *
 * Reads from this file return the hexadecimal string value of the last
 * Device Capability register offset written to this file.
 *
 * The return value is the number characters written to buf, or EAGAIN
 * if the driver is busy and cannot service the read request immediately.
 * If EAGAIN is returned the caller should wait a little and retry the
 * read.
 *
 * The format of the string returned in buf is:
 * 	"offset:<offset>=<regvalue>
 * 	where:	<offset>	is the last Device Capability register offset
 * 						written to this file
 * 			<regvalue>	the currentl value of the Device Capability register
 * 						specified in offset
 */
ssize_t show_dev_cap(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	uint8_t			regValue;
	int			status = -EINVAL;


	MHL_TX_DBG_INFO(dev_context, "called\n");

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}

	if (dev_context->mhl_flags & MHL_STATE_FLAG_CONNECTED) {

		status = si_mhl_tx_get_peer_dev_cap_entry(dev_context,
				dev_context->dev_cap_offset,
				&regValue);
		if (status != 0) {
			/*
			 * Driver is busy and cannot provide the requested DEVCAP
			 * register value right now so inform caller they need to
			 * try again later.
			 */
			status = -EAGAIN;
			goto err_exit;
		}
		status = scnprintf(buf, PAGE_SIZE, "offset:0x%02x=0x%02x",
				dev_context->dev_cap_offset, regValue);
	}

err_exit:
	up(&dev_context->isr_lock);

	return status;
}


/*
 * send_scratch_pad() - Handle write request to the spad attribute file.
 *
 * This file is used to either initiate a write to the scratch pad registers
 * of an attached device, or to set the offset and byte count for a subsequent
 * read from the local scratch pad registers.
 *
 * The format of the string in buf must be:
 * 	offset=<offset_value> length=<Length_value> \
 * 	data=data_byte_0 ... data_byte_length-1
 * 	where:	<offset_value>	specifies the starting register offset to begin
 * 							read/writing within the scratch pad register space
 * 			<length_value>	number of scratch pad registers to be written/read
 * 			data_byte		space separated list of <length_value> data bytes
 * 							to be written.  If no data bytes are present then
 * 							the write to this file will only be used to set
 * 							the offset and length for a subsequent read from
 * 							this file.
 */
ssize_t send_scratch_pad(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	unsigned long	offset = 0x100;		/* initialize with invalid values */
	unsigned long	length = 0x100;
	unsigned long	value;
	u8		data[MAX_SCRATCH_PAD_TRANSFER_SIZE];
	u8		idx;
	char		*str;
	char		*endptr;
	enum scratch_pad_status	scratch_pad_status;
	int		status = -EINVAL;

	MHL_TX_DBG_ERR(dev_context, "received string: ""%s""\n", buf);

	/*
	 * Parse the input string and extract the scratch pad register selection
	 * parameters
	 */
	str = strstr(buf, "offset=");
	if (str != NULL) {
		offset = simple_strtoul(str + 7, NULL, 0);
		if (offset > SCRATCH_PAD_SIZE) {
			MHL_TX_DBG_ERR(dev_context, "Invalid offset value entered\n");
			goto err_exit_2;
		}
	} else {
		MHL_TX_DBG_ERR(dev_context, "Invalid string format, can't find ""offset"" value\n");
		goto err_exit_2;
	}

	str = strstr(buf, "length=");
	if (str != NULL) {
		length = simple_strtoul(str + 7, NULL, 0);
		if (length > MAX_SCRATCH_PAD_TRANSFER_SIZE) {
			MHL_TX_DBG_ERR(dev_context, "Transfer length too large\n");
			goto err_exit_2;
		}
	} else {
		MHL_TX_DBG_ERR(dev_context, "Invalid string format, can't find ""length"" value\n");
		goto err_exit_2;
	}

	str = strstr(buf, "data=");
	if (str != NULL) {

		str += 5;
		endptr = str;
		for(idx = 0; idx < length; idx++) {

			endptr += strspn(endptr, white_space);
			str = endptr;
			if (*str == 0) {
				MHL_TX_DBG_ERR(dev_context, "Too few data values provided\n");
				goto err_exit_2;
			}

			value = simple_strtoul(str, &endptr, 0);
			if (value > 0xFF) {
				MHL_TX_DBG_ERR(dev_context, "Invalid scratch pad data detected\n");
				goto err_exit_2;
			}

			data[idx] = value;
		}

	} else {
		idx = 0;
	}

	if ((offset + length) > SCRATCH_PAD_SIZE) {
		MHL_TX_DBG_ERR(dev_context, "Invalid offset/length combination entered");
		goto err_exit_2;
	}

	dev_context->spad_offset = offset;
	dev_context->spad_xfer_length = length;

	if (idx == 0) {
		MHL_TX_DBG_INFO(dev_context, "No data specified, storing offset "\
				"and length for subsequent scratch pad read\n");

		goto err_exit_2;
	}

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit_1;
	}

	/*
	 * Make sure there is an MHL connection and that the requested
	 * data transfer parameters don't exceed the address space of
	 * the scratch pad.  NOTE: The address space reserved for the
	 * Scratch Pad registers is 64 bytes but sources and sink devices
	 * are only required to implement the 1st 16 bytes.
	 */
	if (!(dev_context->mhl_flags & MHL_STATE_FLAG_CONNECTED) ||
		(length < ADOPTER_ID_SIZE) ||
		(offset > (SCRATCH_PAD_SIZE - ADOPTER_ID_SIZE)) ||
		(offset + length > SCRATCH_PAD_SIZE)) {
		status = -EINVAL;
		goto err_exit_1;
	}

	dev_context->mhl_flags |= MHL_STATE_FLAG_SPAD_SENT;
	dev_context->spad_send_status = 0;

	scratch_pad_status = si_mhl_tx_request_write_burst(dev_context, offset, length, data);

	switch (scratch_pad_status) {
		case SCRATCHPAD_SUCCESS:
			/* On success return the number of bytes written to this file */
			status = count;
			break;

		case SCRATCHPAD_BUSY:
			status = -EAGAIN;
			break;

		default:
			status = -EFAULT;
			break;
	}

err_exit_1:
	up(&dev_context->isr_lock);

err_exit_2:
	return status;
}


/*
 * show_scratch_pad() - Handle read request to the spad attribute file.
 *
 * Reads from this file return one or more scratch pad register values
 * in hexadecimal string format.  The registers returned are specified
 * by the offset and length values previously written to this file.
 *
 * The return value is the number characters written to buf, or EAGAIN
 * if the driver is busy and cannot service the read request immediately.
 * If EAGAIN is returned the caller should wait a little and retry the
 * read.
 *
 * The format of the string returned in buf is:
 * 	"offset:<offset> length:<lenvalue> data:<datavalues>
 * 	where:	<offset>	is the last scratch pad register offset
 * 						written to this file
 * 			<lenvalue>	is the last scratch pad register transfer length
 * 						written to this file
 * 			<datavalue>	space separated list of <lenvalue> scratch pad
 * 						register values in OxXX format
 */
ssize_t show_scratch_pad(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	u8			data[MAX_SCRATCH_PAD_TRANSFER_SIZE];
	u8			idx;
	enum scratch_pad_status	scratch_pad_status;
	int			status = -EINVAL;


	MHL_TX_DBG_INFO(dev_context, "called\n");

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}

	if (dev_context->mhl_flags & MHL_STATE_FLAG_CONNECTED) {

		scratch_pad_status  = si_get_scratch_pad_vector(dev_context,
				dev_context->spad_offset,
				dev_context->spad_xfer_length,
				data);

		switch (scratch_pad_status) {
			case SCRATCHPAD_SUCCESS:
				status = scnprintf(buf, PAGE_SIZE, "offset:0x%02x " \
						"length:0x%02x data:",
						dev_context->spad_offset,
						dev_context->spad_xfer_length);

				for (idx = 0; idx < dev_context->spad_xfer_length; idx++) {
					status += scnprintf(&buf[status], PAGE_SIZE, "0x%02x ", data[idx]);
				}
				break;

			case SCRATCHPAD_BUSY:
				status = -EAGAIN;
				break;

			default:
				status = -EFAULT;
				break;
		}
	}

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * send_debug() - Handle write request to the debug attribute file.
 *
 * This file is used to either perform a write to registers of the transmitter
 * or to set the address, offset and byte count for a subsequent from the
 * register(s) of the transmitter.
 *
 * The format of the string in buf must be:
 * 	address=<pageaddr> offset=<offset_value> length=<Length_value> \
 * 	data=data_byte_0 ... data_byte_length-1
 * 	where: <pageaddr>		specifies the I2C register page of the register(s)
 * 							to be written/read
 * 			<offset_value>	specifies the starting register offset within the
 * 							register page to begin writing/reading
 * 			<length_value>	number registers to be written/read
 * 			data_byte		space separated list of <length_value> data bytes
 * 							to be written.  If no data bytes are present then
 * 							the write to this file will only be used to set
 * 							the  page address, offset and length for a
 * 							subsequent read from this file.
 */
ssize_t send_debug(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	unsigned long	address = 0x100;		/* initialize with invalid values */
	unsigned long	offset = 0x100;
	unsigned long	length = 0x100;
	unsigned long	value;
	u8		data[MAX_DEBUG_TRANSFER_SIZE];
	u8		idx;
	char		*str;
	char		*endptr;
	int		status = -EINVAL;

	MHL_TX_DBG_INFO(dev_context, "received string: ""%s""\n", buf);

	/*
	 * Parse the input string and extract the scratch pad register selection
	 * parameters
	 */
	str = strstr(buf, "address=");
	if (str != NULL) {
		address = simple_strtoul(str + 8, NULL, 0);
		if (address > 0xFF) {
			MHL_TX_DBG_ERR(dev_context, "Invalid page address: 0x%02lx specified\n", address);
			goto err_exit_2;
		}
	} else {
		MHL_TX_DBG_ERR(dev_context, "Invalid string format, can't find ""address"" parameter\n");
		goto err_exit_2;
	}

	str = strstr(buf, "offset=");
	if (str != NULL) {
		offset = simple_strtoul(str + 7, NULL, 0);
		if (offset > 0xFF) {
			MHL_TX_DBG_ERR(dev_context, "Invalid page offset: 0x%02lx specified\n", offset);
			goto err_exit_2;
		}
	} else {
		MHL_TX_DBG_ERR(dev_context, "Invalid string format, can't find ""offset"" value\n");
		goto err_exit_2;
	}

	str = strstr(buf, "length=");
	if (str != NULL) {
		length = simple_strtoul(str + 7, NULL, 0);
		if (length > MAX_DEBUG_TRANSFER_SIZE) {
			MHL_TX_DBG_ERR(dev_context, "Transfer size 0x%02lx is too large\n", length);
			goto err_exit_2;
		}
	} else {
		MHL_TX_DBG_ERR(dev_context, "Invalid string format, can't find ""length"" value\n");
		goto err_exit_2;
	}

	str = strstr(buf, "data=");
	if (str != NULL) {

		str += 5;
		endptr = str;
		for(idx = 0; idx < length; idx++) {
			endptr += strspn(endptr, white_space);
			str = endptr;
			if (*str == 0) {
				MHL_TX_DBG_ERR(dev_context, "Too few data values provided\n");
				goto err_exit_2;
			}

			value = simple_strtoul(str, &endptr, 0);

			if (value > 0xFF) {
				MHL_TX_DBG_ERR(dev_context, "Invalid register data value detected\n");
				goto err_exit_2;
			}

			data[idx] = value;
		}
		

	} else {
		idx = 0;
	}

	if ((offset + length) > 0x100) {
		MHL_TX_DBG_ERR(dev_context
			, "Invalid offset/length combination entered 0x%02x/0x%02x"
			, offset, length);
		goto err_exit_2;
	}

	dev_context->debug_i2c_address = address;
	dev_context->debug_i2c_offset = offset;
	dev_context->debug_i2c_xfer_length = length;

	if (idx == 0) {
		MHL_TX_DBG_INFO(dev_context, "No data specified, storing address "\
				"offset and length for subsequent debug read\n");
		goto err_exit_2;
	}

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit_1;
	}

	status =  dev_context->drv_info->mhl_device_dbg_i2c_reg_xfer(
			&dev_context->drv_context,
			address, offset, length,
			DEBUG_I2C_WRITE, data);
	if (status == 0)
		status = count;

err_exit_1:
	up(&dev_context->isr_lock);

err_exit_2:
	return status;
}

/*
 * show_debug()	- Handle read request to the debug attribute file.
 *
 * Reads from this file return one or more transmitter register values in
 * hexadecimal string format.  The registers returned are specified by the
 * address, offset and length values previously written to this file.
 *
 * The return value is the number characters written to buf, or an error
 * code if the I2C read fails.
 *
 * The format of the string returned in buf is:
 * 	"address:<pageaddr> offset:<offset> length:<lenvalue> data:<datavalues>
 * 	where:	<pageaddr>	is the last I2C register page address written
 * 						to this file
 * 			<offset>	is the last register offset written to this file
 * 			<lenvalue>	is the last register transfer length written
 * 						to this file
 * 			<datavalue>	space separated list of <lenvalue> register
 * 						values in OxXX format
 */
ssize_t show_debug(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	u8			data[MAX_DEBUG_TRANSFER_SIZE];
	u8			idx;
	int			status = -EINVAL;

	MHL_TX_DBG_INFO(dev_context, "called\n");

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto no_dev;
	}

	status =  dev_context->drv_info->mhl_device_dbg_i2c_reg_xfer(
			&dev_context->drv_context,
			dev_context->debug_i2c_address,
			dev_context->debug_i2c_offset,
			dev_context->debug_i2c_xfer_length,
			DEBUG_I2C_READ, data);
no_dev:
	up(&dev_context->isr_lock);

	if (status == 0) {

		status = scnprintf(buf, PAGE_SIZE, "address:0x%02x offset:0x%02x " \
				"length:0x%02x data:",
				dev_context->debug_i2c_address,
				dev_context->debug_i2c_offset,
				dev_context->debug_i2c_xfer_length);

		for (idx = 0; idx < dev_context->debug_i2c_xfer_length; idx++) {
			status += scnprintf(&buf[status], PAGE_SIZE, "0x%02x ", data[idx]);
		}
	}

	return status;
}


//( begin trace_level API
/*
 * show_trace_level() - Handle read request to the trace_level attribute file.
 *
 * The return value is the number characters written to buf, or EAGAIN
 * if the driver is busy and cannot service the read request immediately.
 * If EAGAIN is returned the caller should wait a little and retry the
 * read.
 */
ssize_t get_trace_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	int	status = -EINVAL;
	extern int debug_msgs;


	if (down_interruptible(&dev_context->isr_lock)){
		MHL_TX_DBG_ERR(dev_context,"-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR(dev_context,"-ENODEV\n");
		status = -ENODEV;
		goto err_exit;
	}
	
	status = scnprintf(buf, PAGE_SIZE, "level=%d",debug_msgs );
	MHL_TX_DBG_INFO(dev_context,"buf:%c%s%c\n",'"',buf,'"');

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * set_trace_level() - Handle write request to the trace_level attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t set_trace_level(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct mhl_dev_context	*dev_context = dev_get_drvdata(dev);
	int status;
	char *str;
	const char *key="level=";
	extern int debug_msgs;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO(dev_context, "received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}
	str = strstr(buf, key);
	if (str != NULL) {
		debug_msgs = simple_strtol(str + strlen(key), NULL, 0);
	} else {
		MHL_TX_DBG_ERR(dev_context, "Invalid string format, can't find \"%s\" parameter\n",key);
	}

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

//) end trace_level API

#define MAX_EVENT_STRING_LEN 128
/*
 * Handler for event notifications from the MhlTx layer.
 *
 */
 #define MTK_MHL_NOTIFY_SYS

void mhl_event_notify(struct mhl_dev_context *dev_context, u32 event, u32 event_param, void *data)
{
	char	event_string[MAX_EVENT_STRING_LEN];
	char	*envp[] = {event_string, NULL};
	char	*buf;
	u32	length;
	u32	count;
	int	idx;

	MHL_TX_DBG_INFO(dev_context, "called, event: 0x%08x "\
			 "event_param: 0x%08x\n", event, event_param);

	/*
	 * Save the info on the most recent event.  This is done to support the
	 * SII_MHL_GET_MHL_TX_EVENT IOCTL.  If at some point in the future the
	 * driver's IOCTL interface is abandoned in favor of using sysfs attributes
	 * this can be removed.
	 */
	dev_context->pending_event = event;
	dev_context->pending_event_data = event_param;

	switch(event) {
		
	case MHL_TX_EVENT_SMB_DATA:
	case MHL_TX_EVENT_HPD_CLEAR:
	case MHL_TX_EVENT_HPD_GOT:
	case MHL_TX_EVENT_DEV_CAP_UPDATE:
	case MHL_TX_EVENT_EDID_UPDATE:
	case MHL_TX_EVENT_EDID_DONE:
		Notify_AP_MHL_TX_Event(event, event_param, data);
		break;
			
	case MHL_TX_EVENT_CONNECTION:
		dev_context->mhl_flags |= MHL_STATE_FLAG_CONNECTED;

#ifdef MEDIA_DATA_TUNNEL_SUPPORT
//		mdt_init(dev_context);
#endif
#ifdef RCP_INPUTDEV_SUPPORT
		//init_rcp_input_dev(dev_context);
#endif

#ifndef MTK_MHL_NOTIFY_SYS
		sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
				__stringify(SYS_ATTR_NAME_CONN));

		strncpy(event_string, "MHLEVENT=connected", MAX_EVENT_STRING_LEN);
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE, envp);
#else

#endif
		Notify_AP_MHL_TX_Event(event, event_param, data);
		break;

	case MHL_TX_EVENT_DISCONNECTION:
		dev_context->mhl_flags = 0;
		dev_context->rcp_key_code = 0;
		dev_context->rcp_err_code = 0;
		dev_context->rcp_send_status = 0;
		dev_context->ucp_key_code = 0;
		dev_context->ucp_err_code = 0;
		dev_context->spad_send_status = 0;
		dev_context->misc_flags.flags.have_complete_devcap = false;//SET the para to default;

#ifdef MEDIA_DATA_TUNNEL_SUPPORT
//		mdt_destroy(dev_context);
#endif
#ifdef RCP_INPUTDEV_SUPPORT
		//destroy_rcp_input_dev(dev_context);
#endif

#ifndef MTK_MHL_NOTIFY_SYS
		sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
					 __stringify(SYS_ATTR_NAME_CONN));

		strncpy(event_string, "MHLEVENT=disconnected", MAX_EVENT_STRING_LEN);
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE, envp);
#else

#endif

		Notify_AP_MHL_TX_Event(event, event_param, data);
		break;

	case MHL_TX_EVENT_RCP_RECEIVED:

#ifdef RCP_INPUTDEV_SUPPORT
		if (0 == generate_rcp_input_event(dev_context, (uint8_t)event_param))
			si_mhl_tx_rcpk_send(dev_context, (uint8_t)event_param);
		else
		{
			si_mhl_tx_rcpe_send(dev_context, MHL_RCPE_STATUS_INEEFECTIVE_KEY_CODE);
			si_mhl_tx_rcpk_send(dev_context, (uint8_t)event_param);
		}
#else
		dev_context->mhl_flags &= ~MHL_STATE_FLAG_RCP_SENT;
		dev_context->mhl_flags |= MHL_STATE_FLAG_RCP_RECEIVED;
		dev_context->rcp_key_code = event_param;

#ifndef MTK_MHL_NOTIFY_SYS
		sysfs_notify(&dev_context->mhl_dev->kobj, NULL, __stringify(SYS_ATTR_NAME_RCP));

		snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=received_RCP key code=0x%02x", event_param);
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE, envp);
#else

#endif
		break;

	case MHL_TX_EVENT_RCPK_RECEIVED:
		if ((dev_context->mhl_flags & MHL_STATE_FLAG_RCP_SENT)
			&& (dev_context->rcp_key_code == event_param)) {

			dev_context->rcp_err_code = 0;
			dev_context->mhl_flags |= MHL_STATE_FLAG_RCP_ACK;

			MHL_TX_DBG_INFO(dev_context, "Generating RCPK received event, keycode: 0x%02x\n", event_param);

#ifndef MTK_MHL_NOTIFY_SYS

			sysfs_notify(&dev_context->mhl_dev->kobj, NULL, __stringify(SYS_ATTR_NAME_RCPK));

			snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=received_RCPK key code=0x%02x", event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE, envp);
#else

#endif
		} else {
			MHL_TX_DBG_ERR(dev_context, "Ignoring unexpected RCPK received event, keycode: 0x%02x\n", event_param);
		}
		break;

	case MHL_TX_EVENT_RCPE_RECEIVED:
		if (dev_context->mhl_flags & MHL_STATE_FLAG_RCP_SENT) {

			dev_context->rcp_err_code = event_param;
			dev_context->mhl_flags |= MHL_STATE_FLAG_RCP_NAK;

			MHL_TX_DBG_INFO(dev_context, "Generating RCPE received event, error code: 0x%02x\n", event_param);
#ifndef MTK_MHL_NOTIFY_SYS
			sysfs_notify(&dev_context->mhl_dev->kobj, NULL, __stringify(SYS_ATTR_NAME_RCPK));

			snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=received_RCPE error code=0x%02x", event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE, envp);
#else

#endif
		} else {
			MHL_TX_DBG_ERR(dev_context, "Ignoring unexpected RCPE received event, error code: 0x%02x\n", event_param);
		}
		break;
#endif /* #ifdef RCP_INPUTDEV_SUPPORT */

	case MHL_TX_EVENT_UCP_RECEIVED:
		dev_context->mhl_flags &= ~MHL_STATE_FLAG_UCP_SENT;
		dev_context->mhl_flags |= MHL_STATE_FLAG_UCP_RECEIVED;
		dev_context->ucp_key_code = event_param;
#ifndef MTK_MHL_NOTIFY_SYS		
		sysfs_notify(&dev_context->mhl_dev->kobj, NULL, __stringify(SYS_ATTR_NAME_UCP));

		snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=received_UCP key code=0x%02x", event_param);
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE, envp);
#else

#endif
		break;

	case MHL_TX_EVENT_UCPK_RECEIVED:
		if ((dev_context->mhl_flags & MHL_STATE_FLAG_UCP_SENT)
			&& (dev_context->ucp_key_code == event_param)) {

			dev_context->mhl_flags |= MHL_STATE_FLAG_UCP_ACK;

			MHL_TX_DBG_INFO(dev_context, "Generating UCPK received event, keycode: 0x%02x\n", event_param);
#ifndef MTK_MHL_NOTIFY_SYS
			sysfs_notify(&dev_context->mhl_dev->kobj, NULL, __stringify(SYS_ATTR_NAME_UCPK));

			snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=received_UCPK key code=0x%02x", event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE, envp);
#else

#endif
		} else {
			MHL_TX_DBG_ERR(dev_context, "Ignoring unexpected UCPK received event, keycode: 0x%02x\n", event_param);
		}
		break;

	case MHL_TX_EVENT_UCPE_RECEIVED:
		if (dev_context->mhl_flags & MHL_STATE_FLAG_UCP_SENT) {

			dev_context->ucp_err_code = event_param;
			dev_context->mhl_flags |= MHL_STATE_FLAG_UCP_NAK;

			MHL_TX_DBG_INFO(dev_context, "Generating UCPE received event, error code: 0x%02x\n", event_param);
#ifndef MTK_MHL_NOTIFY_SYS
			sysfs_notify(&dev_context->mhl_dev->kobj, NULL, __stringify(SYS_ATTR_NAME_UCPK));

			snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=received_UCPE error code=0x%02x", event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE, envp);
#else

#endif
		} else {
			MHL_TX_DBG_ERR(dev_context, "Ignoring unexpected UCPE received event, error code: 0x%02x\n", event_param);
		}
		break;

	case MHL_TX_EVENT_SPAD_RECEIVED:
		length = event_param;
		buf = data;

#ifndef MTK_MHL_NOTIFY_SYS
		sysfs_notify(&dev_context->mhl_dev->kobj, NULL, __stringify(SYS_ATTR_NAME_SPAD));

		idx = snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=SPAD_CHG length=0x%02x data=", length);

		count = 0;
		while (idx < MAX_EVENT_STRING_LEN) {
			if (count >= length)
				break;

			idx += snprintf(&event_string[idx], MAX_EVENT_STRING_LEN - idx, "0x%02x ", buf[count]);
			count++;
		}

		if (idx < MAX_EVENT_STRING_LEN) {
			kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE, envp);
		} else {
			MHL_TX_DBG_ERR(dev_context, "Buffer too small to contain scratch pad data!\n");
		}
#else

#endif		
		break;

	case MHL_TX_EVENT_POW_BIT_CHG:
		MHL_TX_DBG_INFO(dev_context, "Generating VBUS power bit change event, POW bit is %s\n", event_param? "ON" : "OFF");
#ifndef MTK_MHL_NOTIFY_SYS		
		snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=MHL VBUS power %s", event_param? "ON" : "OFF");
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE, envp);
#else

#endif
		break;

	case MHL_TX_EVENT_RAP_RECEIVED:
		MHL_TX_DBG_INFO(dev_context, "Generating RAP received event, action code: 0x%02x\n", event_param);
#ifndef MTK_MHL_NOTIFY_SYS

		sysfs_notify(&dev_context->mhl_dev->kobj, NULL, __stringify(SYS_ATTR_NAME_RAP));

		snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=received_RAP action code=0x%02x", event_param);
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE, envp);
#else

#endif
		break;

	default:
		MHL_TX_DBG_ERR(dev_context, "called with unrecognized event code!\n");
	}
}

/*
 *  File operations supported by the MHL driver
 */
static const struct file_operations mhl_fops = {
	.owner = THIS_MODULE
};

/*
 * Sysfs attribute files supported by this driver.
 */
struct device_attribute driver_attribs[] = {
	__ATTR(SYS_ATTR_NAME_CONN		,0444,show_connection_state, NULL),
#ifndef RCP_INPUTDEV_SUPPORT
	__ATTR(SYS_ATTR_NAME_RCP		,0444,show_rcp			, send_rcp),
	__ATTR(SYS_ATTR_NAME_RCPK		,0444,show_rcp_ack		, send_rcp_ack),
#endif
	__ATTR(SYS_ATTR_NAME_RAP		,0444,show_rap			, send_rap),
	__ATTR(SYS_ATTR_NAME_RAP_STATUS		,0444,show_rap_status		, set_rap_status),
	__ATTR(SYS_ATTR_NAME_DEVCAP		,0444,show_dev_cap		, select_dev_cap),
	__ATTR(SYS_ATTR_NAME_UCP		,0444,show_ucp			, send_ucp),
	__ATTR(SYS_ATTR_NAME_UCPK		,0444,show_ucp_ack		, send_ucp_ack),
	__ATTR(SYS_ATTR_NAME_SPAD		,0444,show_scratch_pad		, send_scratch_pad),
	__ATTR(SYS_ATTR_NAME_DEBUG		,0444,show_debug		, send_debug),
	__ATTR(SYS_ATTR_NAME_TRACE_LEVEL	,0444,get_trace_level		, set_trace_level),
	__ATTR_NULL
};

static void mhl8338_irq_handler(void)
{
 	atomic_set(&mhl_irq_event, 1);
    wake_up_interruptible(&mhl_irq_wq); 
	//mt65xx_eint_unmask(CUST_EINT_HDMI_HPD_NUM);   
}

static void mhl_irq_handler(int irq, void *data);

static int irq_cnt = 0;
static int mhl_irq_kthread(void *data)
{
    int i=0;
	struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
	sched_setscheduler(current, SCHED_RR, &param);
    
    for( ;; ) {
        set_current_state(TASK_INTERRUPTIBLE);
        wait_event_interruptible(mhl_irq_wq, atomic_read(&mhl_irq_event));
        set_current_state(TASK_RUNNING);
        irq_cnt++;
        //hdmi_update_impl();

		atomic_set(&mhl_irq_event, 0);
		//for( i=0;i<30; i++)
            mhl_irq_handler(0, si_dev_context);
        
        if (kthread_should_stop())
            break;
#ifdef CUST_EINT_MHL_NUM
		mt_eint_unmask(CUST_EINT_MHL_NUM);
#endif
    }

    return 0;
}

/*
 * Interrupt handler for MHL transmitter interrupts.
 *
 * @irq:	The number of the asserted IRQ line that caused
 *  		this handler to be called.
 * @data:	Data pointer passed when the interrupt was enabled,
 *  		which in this case is a pointer to an mhl_dev_context struct.
 *
 * Always returns IRQ_HANDLED.
 */
static void mhl_irq_handler(int irq, void *data)
{
	struct mhl_dev_context	*dev_context = (struct mhl_dev_context *)data;


	if (!down_interruptible(&dev_context->isr_lock)) {
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN)
			goto irq_done;
		if (dev_context->dev_flags & DEV_FLAG_COMM_MODE)
			goto irq_done;

		memset(&dev_context->intr_info, 0, sizeof(*(&dev_context->intr_info)));

		dev_context->intr_info.edid_parser_context = dev_context->edid_parser_context;

		dev_context->drv_info->mhl_device_isr((struct drv_hw_context *)
				(&dev_context->drv_context),
				&dev_context->intr_info);

		/* Now post process events detected by the interrupt handler */
		if(dev_context->intr_info.flags & DRV_INTR_FLAG_DISCONNECT) {
			dev_context->misc_flags.flags.rap_content_on = false;
			dev_context->misc_flags.flags.mhl_rsen = false;
			dev_context->mhl_connection_event = true;
			dev_context->mhl_connected = MHL_TX_EVENT_DISCONNECTION;
			si_mhl_tx_process_events(dev_context);
		} else {
			if (dev_context->intr_info.flags & DRV_INTR_FLAG_CONNECT) {
				dev_context->misc_flags.flags.rap_content_on = true;
				dev_context->rap_sub_command = MHL_RAP_CONTENT_ON;
				dev_context->misc_flags.flags.mhl_rsen = true;
				dev_context->mhl_connection_event = true;
				dev_context->mhl_connected = MHL_TX_EVENT_CONNECTION;
				si_mhl_tx_process_events(dev_context);
			}

			if (dev_context->intr_info.flags & DRV_INTR_FLAG_CBUS_ABORT)
				process_cbus_abort(dev_context);

			if (dev_context->intr_info.flags & DRV_INTR_FLAG_WRITE_BURST)
				si_mhl_tx_process_write_burst_data(dev_context);

			if (dev_context->intr_info.flags & DRV_INTR_FLAG_SET_INT)
				si_mhl_tx_got_mhl_intr(dev_context,
						dev_context->intr_info.int_msg[0],
						dev_context->intr_info.int_msg[1]);

			if (dev_context->intr_info.flags & DRV_INTR_FLAG_MSC_DONE)
				si_mhl_tx_msc_command_done(dev_context,
						dev_context->intr_info.msc_done_data);

			if (dev_context->intr_info.flags & DRV_INTR_FLAG_HPD_CHANGE)
				si_mhl_tx_notify_downstream_hpd_change(dev_context,
						dev_context->intr_info.hpd_status);

			if (dev_context->intr_info.flags & DRV_INTR_FLAG_WRITE_STAT)
				si_mhl_tx_got_mhl_status(dev_context,
						dev_context->intr_info.write_stat[0],
						dev_context->intr_info.write_stat[1]);

			if (dev_context->intr_info.flags & DRV_INTR_FLAG_MSC_RECVD) {
				dev_context->msc_msg_arrived		= true;
				dev_context->msc_msg_sub_command	= dev_context->intr_info.msc_msg[0];
				dev_context->msc_msg_data		= dev_context->intr_info.msc_msg[1];
				si_mhl_tx_process_events(dev_context);
			}
		}

		/*
		 *  Check to see if we can send any messages that may have
		 * been queued up as the result of interrupt processing.
		 */
		si_mhl_tx_drive_states(dev_context);

//		if(sii_mhl_connected != dev_context->mhl_connected)
//    		pr_err("MHL connected status %d -> %d\n",sii_mhl_connected, dev_context->mhl_connected);
irq_done:
		up(&dev_context->isr_lock);
	}

}

/* APIs provided by the MHL layer to the lower level driver */

int mhl_tx_init(struct mhl_drv_info const *drv_info, struct i2c_client *client)
{
	///struct mhl_dev_context *dev_context;
	int ret,dummy;


	if (drv_info == NULL || client == NULL) {
		pr_err("Null parameter passed to %s\n",__FUNCTION__);
		return -EINVAL;
	}

	init_waitqueue_head(&mhl_irq_wq);	
	mhl_irq_task = kthread_create(mhl_irq_kthread, NULL, "mhl_irq_kthread"); 
	wake_up_process(mhl_irq_task);

    #ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
    init_waitqueue_head(&smartbook_wq);	
    smartbook_task = kthread_create(smartbook_kthread, NULL, "smartbook_kthread"); 	
    wake_up_process(smartbook_task);
    #endif

	si_dev_context = kzalloc(sizeof(*si_dev_context) + drv_info->drv_context_size, GFP_KERNEL);
	if (!si_dev_context) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	si_dev_context->signature = MHL_DEV_CONTEXT_SIGNATURE;
	si_dev_context->drv_info = drv_info;
	si_dev_context->client = client;

	sema_init(&si_dev_context->isr_lock, 1);
	INIT_LIST_HEAD(&si_dev_context->timer_list);
	si_dev_context->timer_work_queue = create_workqueue(MHL_DRIVER_NAME);
	if (si_dev_context->timer_work_queue == NULL) {
		ret = -ENOMEM;
		goto free_mem;
	}
    
	if (mhl_class == NULL) {
		mhl_class = class_create(THIS_MODULE, "mhl");
		if(IS_ERR(mhl_class)) {
			ret = PTR_ERR(mhl_class);
			pr_info("class_create failed %d\n", ret);
			goto err_exit;
		}

		mhl_class->dev_attrs = driver_attribs;

		ret = alloc_chrdev_region(&dev_num,
				0, MHL_DRIVER_MINOR_MAX,
				MHL_DRIVER_NAME);

		if (ret) {
			pr_info("register_chrdev %s failed, error code: %d\n", MHL_DRIVER_NAME, ret);
			goto free_class;
		}

		cdev_init(&si_dev_context->mhl_cdev, &mhl_fops);
		si_dev_context->mhl_cdev.owner = THIS_MODULE;
		ret = cdev_add(&si_dev_context->mhl_cdev, MINOR(dev_num), MHL_DRIVER_MINOR_MAX);
		if (ret) {
			pr_info("cdev_add %s failed %d\n", MHL_DRIVER_NAME, ret);
			goto free_chrdev;
		}
	}

	si_dev_context->mhl_dev = device_create(mhl_class, &si_dev_context->client->dev,
			dev_num, si_dev_context,
			"%s", MHL_DEVICE_NAME);
	if (IS_ERR(si_dev_context->mhl_dev)) {
		ret = PTR_ERR(si_dev_context->mhl_dev);
		pr_info("device_create failed %s %d\n", MHL_DEVICE_NAME, ret);
		goto free_cdev;
	}

#ifdef CUST_EINT_MHL_NUM
        mt_eint_registration(CUST_EINT_MHL_NUM, CUST_EINT_MHL_TYPE, &mhl8338_irq_handler, 0);
        mt_eint_mask(CUST_EINT_MHL_NUM);
#else
        printk("%s,%d Error: CUST_EINT_MHL_NUM is not defined\n", __func__, __LINE__);
#endif
	/* Initialize the MHL transmitter hardware. */
	ret = down_interruptible(&si_dev_context->isr_lock);
	if (ret) {
		dev_err(&client->dev, "failed to acquire ISR semaphore, status: %d\n", ret);
		goto free_irq_handler;
	}

	i2c_set_clientdata(client, si_dev_context);

	/* initialize the PCA 950x GPIO expander, if present */
	//ret = gpio_expander_init(dev_context);
	//if (ret < 0) {
	//	dev_err(&client->dev,"failed to initialize GPIO expander, status: %d\n",ret);
	//	goto free_irq_handler;
	//}

	/* Initialize EDID parser module */
	si_dev_context->edid_parser_context = si_edid_create_context(si_dev_context,&si_dev_context->drv_context);

	ret = si_mhl_tx_initialize(si_dev_context, true);
	up(&si_dev_context->isr_lock);

#ifdef RCP_INPUTDEV_SUPPORT
    init_rcp_input_dev(si_dev_context);
#endif

	MHL_TX_DBG_INFO(si_dev_context, "MHL transmitter successfully initialized\n");

	return ret;

free_irq_handler:
	i2c_set_clientdata(client, NULL);
	dummy = down_interruptible(&si_dev_context->isr_lock);
	if(si_dev_context->edid_parser_context)
		si_edid_destroy_context(si_dev_context->edid_parser_context);

	free_irq(si_dev_context->client->irq, si_dev_context);

free_device:
	device_destroy(mhl_class, dev_num);

free_cdev:
	cdev_del(&si_dev_context->mhl_cdev);

free_chrdev:
	unregister_chrdev_region(dev_num, MHL_DRIVER_MINOR_MAX);
	dev_num = 0;

free_class:
	class_destroy(mhl_class);

err_exit:
	destroy_workqueue(si_dev_context->timer_work_queue);

free_mem:
	kfree(si_dev_context);

	return ret;
}

int mhl_tx_remove(struct i2c_client *client)
{
	struct mhl_dev_context *dev_context;
	int ret = 0;

	dev_context = i2c_get_clientdata(client);

	if (dev_context != NULL){
		MHL_TX_DBG_INFO(dev_context, "%x\n",dev_context);
		ret = down_interruptible(&dev_context->isr_lock);

		dev_context->dev_flags |= DEV_FLAG_SHUTDOWN;

		ret = si_mhl_tx_shutdown(dev_context);

		mhl_tx_destroy_timer_support(dev_context);

		up(&dev_context->isr_lock);

		free_irq(dev_context->client->irq, dev_context);

		device_destroy(mhl_class, dev_num);

		cdev_del(&dev_context->mhl_cdev);

		unregister_chrdev_region(dev_num, MHL_DRIVER_MINOR_MAX);
		dev_num = 0;

		class_destroy(mhl_class);
		mhl_class = NULL;

#ifdef MEDIA_DATA_TUNNEL_SUPPORT
//		mdt_destroy(dev_context);
#endif
#ifdef RCP_INPUTDEV_SUPPORT
		destroy_rcp_input_dev(dev_context);
#endif

		si_edid_destroy_context(dev_context->edid_parser_context);

		//mhl_tx_delete_timer(dev_context, dev_context->cbus_abort_timer);  //TB added to clean up timer object

		kfree(dev_context);

		MHL_TX_DBG_INFO(dev_context, "%x\n",dev_context);
	}
	return ret;
}

static void mhl_tx_destroy_timer_support(struct  mhl_dev_context *dev_context)
{
	struct timer_obj	*mhl_timer;

	/*
	 * Make sure all outstanding timer objects are canceled and the
	 * memory allocated for them is freed.
	 */
	while(!list_empty(&dev_context->timer_list)) {
		mhl_timer = list_first_entry(&dev_context->timer_list, struct timer_obj, list_link);
		hrtimer_cancel(&mhl_timer->hr_timer);
		list_del(&mhl_timer->list_link);
		kfree(mhl_timer);
	}

	destroy_workqueue(dev_context->timer_work_queue);
	dev_context->timer_work_queue = NULL;
}

static void mhl_tx_timer_work_handler(struct work_struct *work)
{
	struct timer_obj	*mhl_timer;

	mhl_timer = container_of(work, struct timer_obj, work_item);

	mhl_timer->flags |= TIMER_OBJ_FLAG_WORK_IP;
	if (!down_interruptible(&mhl_timer->dev_context->isr_lock)) {

		mhl_timer->timer_callback_handler(mhl_timer->callback_param);

		up(&mhl_timer->dev_context->isr_lock);
	}
	mhl_timer->flags &= ~TIMER_OBJ_FLAG_WORK_IP;

	if(mhl_timer->flags & TIMER_OBJ_FLAG_DEL_REQ) {
		/*
		 * Deletion of this timer was requested during the execution of
		 * the callback handler so go ahead and delete it now.
		 */
		kfree(mhl_timer);
	}
}

static enum hrtimer_restart mhl_tx_timer_handler(struct hrtimer *timer)
{
	struct timer_obj	*mhl_timer;

	mhl_timer = container_of(timer, struct timer_obj, hr_timer);

	queue_work(mhl_timer->dev_context->timer_work_queue, &mhl_timer->work_item);

	return HRTIMER_NORESTART;
}

static int is_timer_handle_valid(struct mhl_dev_context *dev_context, void *timer_handle)
{
	struct timer_obj	*timer;

	list_for_each_entry(timer, &dev_context->timer_list, list_link) {
		if (timer == timer_handle)
			break;
	}

	if(timer != timer_handle) {
		MHL_TX_DBG_ERR(dev_context, "Invalid timer handle %p received\n", timer_handle);
		return -EINVAL;
	}
	return 0;
}

int mhl_tx_create_timer(void *context,
		void (*callback_handler)(void *callback_param),
		void *callback_param,
		void **timer_handle)
{
	struct mhl_dev_context	*dev_context;
	struct timer_obj		*new_timer;

	dev_context = get_mhl_device_context(context);

	if (callback_handler == NULL)
		return -EINVAL;

	if (dev_context->timer_work_queue == NULL)
		return -ENOMEM;

	new_timer = kmalloc(sizeof(*new_timer), GFP_KERNEL);
	if (new_timer == NULL)
		return -ENOMEM;

	new_timer->timer_callback_handler = callback_handler;
	new_timer->callback_param = callback_param;
	new_timer->flags = 0;

	new_timer->dev_context = dev_context;
	INIT_WORK(&new_timer->work_item, mhl_tx_timer_work_handler);

	list_add(&new_timer->list_link, &dev_context->timer_list);

	hrtimer_init(&new_timer->hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	new_timer->hr_timer.function = mhl_tx_timer_handler;
	*timer_handle = new_timer;
	return 0;
}

int mhl_tx_delete_timer(void *context, void *timer_handle)
{
	struct mhl_dev_context	*dev_context;
	struct timer_obj	*timer;
	int			status;

	dev_context = get_mhl_device_context(context);

	status = is_timer_handle_valid(dev_context, timer_handle);
	if (status == 0) {
		timer = timer_handle;

		list_del(&timer->list_link);

		hrtimer_cancel(&timer->hr_timer);

		if(timer->flags & TIMER_OBJ_FLAG_WORK_IP) {
			/*
			 * Request to delete timer object came from within the timer's
			 * callback handler.  If we were to proceed with the timer deletion
			 * we would deadlock at cancel_work_sync().  So instead just flag
			 * that the user wants the timer deleted.  Later when the timer
			 * callback completes the timer's work handler will complete the
			 * process of deleting this timer.
			 */
			timer->flags |= TIMER_OBJ_FLAG_DEL_REQ;
		} else {
			cancel_work_sync(&timer->work_item);

			kfree(timer);
		}
	}

	return status;
}

int mhl_tx_start_timer(void *context, void *timer_handle, uint32_t time_msec)
{
	struct mhl_dev_context	*dev_context;
	struct timer_obj	*timer;
	ktime_t			timer_period;
	int			status;

	dev_context = get_mhl_device_context(context);

	status = is_timer_handle_valid(dev_context, timer_handle);
	if (status == 0) {
		timer = timer_handle;

		timer_period = ktime_set(0, MSEC_TO_NSEC(time_msec));
		hrtimer_start(&timer->hr_timer, timer_period, HRTIMER_MODE_REL);
	}

	return status;
}

int mhl_tx_stop_timer(void *context, void *timer_handle)
{
	struct mhl_dev_context	*dev_context;
	struct timer_obj	*timer;
	int			status;

	dev_context = get_mhl_device_context(context);

	status = is_timer_handle_valid(dev_context, timer_handle);
	if (status == 0) {
		timer = timer_handle;

		hrtimer_cancel(&timer->hr_timer);
	}
	return status;
}
