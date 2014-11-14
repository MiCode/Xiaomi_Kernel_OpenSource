/*
 * SiI8620 Linux Driver
 *
 * Copyright (C) 2013-2014 Silicon Image, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/stringify.h>
#include <linux/uaccess.h>

#include "si_fw_macros.h"
#include "si_infoframe.h"
#include "si_edid.h"
#include "si_mhl_defs.h"
#include "si_mhl2_edid_3d_api.h"
#include "si_mhl_tx_hw_drv_api.h"
#ifdef MEDIA_DATA_TUNNEL_SUPPORT
#include "si_mdt_inputdev.h"
#endif
#include "mhl_rcp_inputdev.h"
#if (INCLUDE_RBP == 1)
#include "mhl_rbp_inputdev.h"
#endif
#include "mhl_linux_tx.h"
#include "mhl_supp.h"
#include "platform.h"
#include "si_mhl_callback_api.h"
#include "si_8620_drv.h"

#define MHL_DRIVER_MINOR_MAX 1

/* Convert a value specified in milliseconds to nanoseconds */
#define MSEC_TO_NSEC(x)	(x * 1000000UL)

static char *white_space = "' ', '\t'";
static dev_t dev_num;

static struct class *mhl_class;

static void mhl_tx_destroy_timer_support(struct mhl_dev_context *dev_context);

/* Define SysFs attribute names */
#define SYS_ATTR_NAME_CONN			connection_state
#define SYS_ATTR_NAME_DSHPD			ds_hpd
#define SYS_ATTR_NAME_HDCP2			hdcp2_status

#define SYS_ATTR_NAME_SPAD			spad
#define SYS_ATTR_NAME_I2C_REGISTERS		i2c_registers
#define SYS_ATTR_NAME_DEBUG_LEVEL		debug_level
#define SYS_ATTR_NAME_REG_DEBUG_LEVEL		debug_reg_dump
#define SYS_ATTR_NAME_AKSV			aksv
#define SYS_ATTR_NAME_EDID			edid
#define SYS_ATTR_NAME_HEV_3D_DATA		hev_3d_data
#define SYS_ATTR_NAME_GPIO_INDEX		gpio_index
#define SYS_ATTR_NAME_GPIO_VALUE		gpio_value
#define SYS_ATTR_NAME_PP_16BPP			pp_16bpp

#ifdef DEBUG
#define SYS_ATTR_NAME_TX_POWER			tx_power
#define SYS_ATTR_NAME_STARK_CTL			set_stark_ctl
#endif

/* define SysFs object names */
#define		SYS_ATTR_NAME_IN		in
#define		SYS_ATTR_NAME_IN_STATUS		in_status
#define		SYS_ATTR_NAME_OUT		out
#define		SYS_ATTR_NAME_OUT_STATUS	out_status
#define		SYS_ATTR_NAME_INPUT_DEV		input_dev


#define SYS_OBJECT_NAME_BIST			bist
#define		SYS_ATTR_NAME_BIST_ECBUS_DUR	ecbus_duration
#define		SYS_ATTR_NAME_BIST_ECBUS_PAT	ecbus_pattern
#define		SYS_ATTR_NAME_BIST_ECBUS_FPAT	ecbus_fixed_pattern
#define		SYS_ATTR_NAME_BIST_AV_LINK_DR	av_link_data_rate
#define		SYS_ATTR_NAME_BIST_AV_LINK_PAT	av_link_pattern
#define		SYS_ATTR_NAME_BIST_AV_LINK_VM	av_link_video_mode
#define		SYS_ATTR_NAME_BIST_AV_LINK_DUR	av_link_duration
#define		SYS_ATTR_NAME_BIST_AV_LINK_FPAT	av_link_fixed_pattern
#define		SYS_ATTR_NAME_BIST_AV_LINK_RDZR	av_link_randomizer
#define		SYS_ATTR_NAME_BIST_AV_LINK_IMPM	impedance_mode
#define		SYS_ATTR_NAME_BIST_SETUP	setup
#define		SYS_ATTR_NAME_BIST_TRIGGER	trigger
#define		SYS_ATTR_NAME_BIST_STOP		stop
#define		SYS_ATTR_NAME_BIST_STATUS_REQ	status_req
#define		SYS_ATTR_NAME_T_BIST_MODE_DOWN	t_bist_mode_down


#define SYS_OBJECT_NAME_REG_ACCESS		reg_access
#define		SYS_ATTR_NAME_REG_ACCESS_PAGE	page
#define		SYS_ATTR_NAME_REG_ACCESS_OFFSET	offset
#define		SYS_ATTR_NAME_REG_ACCESS_LENGTH	length
#define		SYS_ATTR_NAME_REG_ACCESS_DATA	data

#define SYS_OBJECT_NAME_RAP			rap
#define		SYS_ATTR_NAME_RAP_IN		SYS_ATTR_NAME_IN
#define		SYS_ATTR_NAME_RAP_IN_STATUS	SYS_ATTR_NAME_IN_STATUS
#define		SYS_ATTR_NAME_RAP_OUT		SYS_ATTR_NAME_OUT
#define		SYS_ATTR_NAME_RAP_OUT_STATUS	SYS_ATTR_NAME_OUT_STATUS
#define		SYS_ATTR_NAME_RAP_INPUT_DEV	SYS_ATTR_NAME_INPUT_DEV

#define SYS_OBJECT_NAME_RCP			rcp
#define		SYS_ATTR_NAME_RCP_IN		SYS_ATTR_NAME_IN
#define		SYS_ATTR_NAME_RCP_IN_STATUS	SYS_ATTR_NAME_IN_STATUS
#define		SYS_ATTR_NAME_RCP_OUT		SYS_ATTR_NAME_OUT
#define		SYS_ATTR_NAME_RCP_OUT_STATUS	SYS_ATTR_NAME_OUT_STATUS
#define		SYS_ATTR_NAME_RCP_INPUT_DEV	SYS_ATTR_NAME_INPUT_DEV

#if (INCLUDE_RBP == 1)
#define SYS_OBJECT_NAME_RBP			rbp
#define		SYS_ATTR_NAME_RBP_IN		SYS_ATTR_NAME_IN
#define		SYS_ATTR_NAME_RBP_IN_STATUS	SYS_ATTR_NAME_IN_STATUS
#define		SYS_ATTR_NAME_RBP_OUT		SYS_ATTR_NAME_OUT
#define		SYS_ATTR_NAME_RBP_OUT_STATUS	SYS_ATTR_NAME_OUT_STATUS
#define		SYS_ATTR_NAME_RBP_INPUT_DEV	SYS_ATTR_NAME_INPUT_DEV
#endif

#define SYS_OBJECT_NAME_UCP			ucp
#define		SYS_ATTR_NAME_UCP_IN		SYS_ATTR_NAME_IN
#define		SYS_ATTR_NAME_UCP_IN_STATUS	SYS_ATTR_NAME_IN_STATUS
#define		SYS_ATTR_NAME_UCP_OUT		SYS_ATTR_NAME_OUT
#define		SYS_ATTR_NAME_UCP_OUT_STATUS	SYS_ATTR_NAME_OUT_STATUS
#define		SYS_ATTR_NAME_UCP_INPUT_DEV	SYS_ATTR_NAME_INPUT_DEV

#define SYS_OBJECT_NAME_DEVCAP				devcap
#define		SYS_ATTR_NAME_DEVCAP_LOCAL_OFFSET	local_offset
#define		SYS_ATTR_NAME_DEVCAP_LOCAL		local
#define		SYS_ATTR_NAME_DEVCAP_REMOTE_OFFSET	remote_offset
#define		SYS_ATTR_NAME_DEVCAP_REMOTE		remote

#define SYS_OBJECT_NAME_HDCP				hdcp
#define		SYS_ATTR_NAME_HDCP_CONTENT_TYPE		hdcp_content_type

#define SYS_OBJECT_NAME_VC				vc
#define		SYS_ATTR_NAME_VC_ASSIGN			vc_assign

/*
 * show_connection_state() - Handle read request to the connection_state
 *							attribute file.
 */
ssize_t show_connection_state(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);

	if (dev_context->mhl_flags & MHL_STATE_FLAG_CONNECTED)
		return scnprintf(buf, PAGE_SIZE, "connected");
	else
		return scnprintf(buf, PAGE_SIZE, "not connected");
}

/*
 * set_connection_state() - Handle write request to the connection_state
 *   attribute file.
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t set_connection_state(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long new_connection_state = 0x100;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (buf != NULL) {
		/* BUGZILLA 27012 no prefix here, only on module parameter. */
		status = kstrtoul(buf, 0, &new_connection_state);
		if ((status != 0) || (new_connection_state > 0xFF)) {
			MHL_TX_DBG_ERR("Invalid connection_state: 0x%02lX\n",
				new_connection_state);
			goto err_exit;
		}
	} else {
		MHL_TX_DBG_ERR("Missing connection_state parameter\n");
		status = -EINVAL;
		goto err_exit;
	}

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}
	si_mhl_tx_drv_switch_cbus_mode((struct drv_hw_context *)
	    &dev_context->drv_context, CM_NO_CONNECTION);
	status = count;

	MHL_TX_DBG_ERR("%sdisconnecting%s\n",
		ANSI_ESC_YELLOW_TEXT,
		ANSI_ESC_RESET_TEXT);

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * show_ds_hpd_state() - Handle read request to the ds_hpd attribute file.
 */
ssize_t show_ds_hpd_state(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	int hpd_status = 0;
	int status;

	status = si_8620_get_hpd_status(&hpd_status);
	if (status)
		return status;
	else
		return scnprintf(buf, PAGE_SIZE, "%d", hpd_status);
}

/*
 * show_ds_hdcp2_status() - Handle read request to the ds_hpd attribute file.
 */
ssize_t show_hdcp2_status(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	uint32_t hdcp2_status = 0;
	int status;

	status = si_8620_get_hdcp2_status(&hdcp2_status);
	if (status)
		return status;
	else
		return scnprintf(buf, PAGE_SIZE, "%x", hdcp2_status);
}

/*
 * Wrapper for kstrtoul() that nul-terminates the input string at
 * the first non-digit character instead of returning an error.
 *
 * This function is destructive to the input string.
 *
 */
static int si_strtoul(char **str, int base, unsigned long *val)
{
	int tok_length, status, nul_offset;
	char *tstr = *str;

	nul_offset = 1;
	status = -EINVAL;
	if ((base == 0) && (tstr[0] == '0') && (tolower(tstr[1]) == 'x')) {
		tstr += 2;
		base = 16;
	}

	tok_length = strspn(tstr, "0123456789ABCDEFabcdef");
	if (tok_length) {
		if ((tstr[tok_length] == '\n') || (tstr[tok_length] == 0))
			nul_offset = 0;

		tstr[tok_length] = 0;
		status = kstrtoul(tstr, base, val);
		if (status == 0) {
			tstr = (tstr + tok_length) + nul_offset;
			*str = tstr;
		}
	}
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
 *	offset=<offset_value> length=<Length_value> \
 *	data=data_byte_0 ... data_byte_length-1
 * where: <offset_value> specifies the starting register offset to begin
 *			 read/writing within the scratch pad register space
 *	 <length_value> number of scratch pad registers to be written/read
 *	 data_byte	space separated list of <length_value> data bytes to be
 *			written.  If no data bytes are present then the write
 *			to this file will only be used to set the offset and
 *			length for a subsequent read from this file.
 */
ssize_t send_scratch_pad(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	unsigned long offset = 0x100;	/* initialize with invalid values */
	unsigned long length = 0x100;
	unsigned long value;
	u8 data[MAX_SCRATCH_PAD_TRANSFER_SIZE];
	unsigned int idx;
	char *str;
	int status = -EINVAL;
	struct bist_setup_burst *setup_burst;
	uint16_t burst_id;
	char *pinput = kmalloc(count, GFP_KERNEL);

	MHL_TX_DBG_INFO("received string(%d):%s\n", count, buf)

	if (pinput == 0) {

		MHL_TX_DBG_ERR("error exit\n");
		return -EINVAL;
	}
	memcpy(pinput, buf, count);

	/*
	 * Parse the input string and extract the scratch pad register selection
	 * parameters
	 */
	str = strnstr(pinput, "offset=", count);
	if (str != NULL) {
		str += 7;
		status = si_strtoul(&str, 0, &offset);
		if ((status != 0) || (offset > SCRATCH_PAD_SIZE)) {
			MHL_TX_DBG_ERR("Invalid offset value entered\n");
			goto err_exit_2;
		}
	} else {
		MHL_TX_DBG_ERR(
			"Invalid string format, can't find offset value\n");
		goto err_exit_2;
	}

	str = strstr(str, "length=");
	if (str != NULL) {
		str += 7;
		status = si_strtoul(&str, 0, &length);
		if ((status != 0) || (length > MAX_SCRATCH_PAD_TRANSFER_SIZE)) {
			MHL_TX_DBG_ERR("Transfer length too large\n");
			goto err_exit_2;
		}
	} else {
		MHL_TX_DBG_ERR(
			"Invalid string format, can't find length value\n");
		goto err_exit_2;
	}

	str = strstr(str, "data=");
	if (str != NULL) {
		str += 5;
		for (idx = 0; idx < length; idx++) {
			str += strspn(str, white_space);
			if (*str == 0) {
				MHL_TX_DBG_ERR(
					"Too few data values provided\n");
				goto err_exit_2;
			}

			status = si_strtoul(&str, 0, &value);
			if ((status != 0) || (value > 0xFF)) {
				MHL_TX_DBG_ERR(
					"Invalid scratch pad data detected\n");
				goto err_exit_2;
			}
			data[idx] = value;
		}
	} else {
		idx = 0;
	}

	if ((offset + length) > SCRATCH_PAD_SIZE) {
		MHL_TX_DBG_ERR("Invalid offset/length combination entered");
		goto err_exit_2;
	}

	dev_context->spad_offset = offset;
	dev_context->spad_xfer_length = length;
	if (idx == 0) {
		MHL_TX_DBG_INFO("No data specified, storing offset "
				"and length for subsequent scratch pad read\n");

		goto err_exit_2;
	}

	if (down_interruptible(&dev_context->isr_lock)) {
		status = -ERESTARTSYS;
		goto err_exit_2;
	}

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

	setup_burst = (struct bist_setup_burst *)data;
	burst_id = setup_burst->burst_id_h << 8;
	burst_id |= setup_burst->burst_id_l;
	{
		enum scratch_pad_status scratch_pad_status;

		scratch_pad_status = si_mhl_tx_request_write_burst(dev_context,
			offset, length, data);
		switch (scratch_pad_status) {
		case SCRATCHPAD_SUCCESS:
			/* Return the number of bytes written to this file */
			status = count;
			break;

		case SCRATCHPAD_BUSY:
			status = -EAGAIN;
			break;

		default:
			status = -EFAULT;
			break;
		}
	}

err_exit_1:
	up(&dev_context->isr_lock);

err_exit_2:
	kfree(pinput);
	MHL_TX_DBG_ERR("status: %d\n", status);
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
 *	"offset:<offset> length:<lenvalue> data:<datavalues>
 *	where:	<offset> is the last scratch pad register offset
 *			 written to this file
 *		<lenvalue> is the last scratch pad register transfer length
 *			   written to this file
 *		<datavalue> space separated list of <lenvalue> scratch pad
 *			    register values in OxXX format
 */
ssize_t show_scratch_pad(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	u8 data[MAX_SCRATCH_PAD_TRANSFER_SIZE];
	u8 idx;
	enum scratch_pad_status scratch_pad_status;
	int status = -EINVAL;

	MHL_TX_DBG_INFO("called\n");

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}

	if (dev_context->mhl_flags & MHL_STATE_FLAG_CONNECTED) {

		scratch_pad_status = si_get_scratch_pad_vector(dev_context,
			dev_context->spad_offset,
			dev_context->spad_xfer_length, data);

		switch (scratch_pad_status) {
		case SCRATCHPAD_SUCCESS:
			status = scnprintf(buf, PAGE_SIZE, "offset:0x%02x "
					   "length:0x%02x data:",
					   dev_context->spad_offset,
					   dev_context->spad_xfer_length);

			for (idx = 0; idx < dev_context->spad_xfer_length;
			     idx++) {
				status +=
				    scnprintf(&buf[status], PAGE_SIZE,
					      "0x%02x ", data[idx]);
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
 * set_reg_access_page() - Handle write request to set the
 *		reg access page value.
 *
 * The format of the string in buf must be:
 *	<pageaddr>
 * Where: <pageaddr> specifies the reg page of the register(s)
 *			to be written/read
 */
ssize_t set_reg_access_page(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	unsigned long address = 0x100;
	int status = -EINVAL;
	char my_buf[20];
	unsigned int i;

	MHL_TX_COMM_INFO("received string: %c%s%c\n", '"', buf, '"');
	if (count >= sizeof(my_buf)) {
		MHL_TX_DBG_ERR("string too long %c%s%c\n", '"', buf, '"');
		return status;
	}
	for (i = 0; i < count; ++i) {
		if ('\n' == buf[i]) {
			my_buf[i] = '\0';
			break;
		}
		if ('\t' == buf[i]) {
			my_buf[i] = '\0';
			break;
		}
		if (' ' == buf[i]) {
			my_buf[i] = '\0';
			break;
		}
		my_buf[i] = buf[i];
	}

	status = kstrtoul(my_buf, 0, &address);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, my_buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, my_buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, my_buf, ANSI_ESC_RESET_TEXT);
	} else if (address > 0xFF) {
		MHL_TX_DBG_ERR("address:0x%x buf:%s%s%s\n", address,
			ANSI_ESC_RED_TEXT, my_buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			dev_context->debug_i2c_address = address;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

/*
 * show_reg_access_page()	- Show the current page number to be used when
 *	reg_access/data is accessed.
 *
 */
ssize_t show_reg_access_page(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	MHL_TX_COMM_INFO("called\n");

	status = scnprintf(buf, PAGE_SIZE, "0x%02x",
		dev_context->debug_i2c_address);

	return status;
}

/*
 * set_reg_access_offset() - Handle write request to set the
 *		reg access page value.
 *
 * The format of the string in buf must be:
 *	<pageaddr>
 * Where: <pageaddr> specifies the reg page of the register(s)
 *			to be written/read
 */
ssize_t set_reg_access_offset(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	unsigned long offset = 0x100;
	int status = -EINVAL;

	MHL_TX_COMM_INFO("received string: " "%s" "\n", buf);

	status = kstrtoul(buf, 0, &offset);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%c%s%c\n",
				status, '"', buf, '"');
	} else if (offset > 0xFF) {
		MHL_TX_DBG_ERR("offset:0x%x buf:%c%s%c\n",
				offset, '"', buf, '"');
	} else {

		if (down_interruptible(&dev_context->isr_lock))
			return -ERESTARTSYS;
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			status = -ENODEV;
		} else {
			dev_context->debug_i2c_offset = offset;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

/*
 * show_reg_access_offset()	- Show the current page number to be used when
 *	reg_access/data is accessed.
 *
 */
ssize_t show_reg_access_offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	MHL_TX_COMM_INFO("called\n");

	status = scnprintf(buf, PAGE_SIZE, "0x%02x",
		dev_context->debug_i2c_offset);

	return status;
}

/*
 * set_reg_access_length() - Handle write request to set the
 *		reg access page value.
 *
 * The format of the string in buf must be:
 *	<pageaddr>
 * Where: <pageaddr> specifies the reg page of the register(s)
 *			to be written/read
 */
ssize_t set_reg_access_length(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	unsigned long length = 0x100;
	int status = -EINVAL;

	MHL_TX_COMM_INFO("received string: " "%s" "\n", buf);

	status = kstrtoul(buf, 0, &length);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s'%s'%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%c%s%c\n",
				status, '"', buf, '"');
	} else if (length > 0xFF) {
		MHL_TX_DBG_ERR("length:0x%x buf:%c%s%c\n",
				length, '"', buf, '"');
	} else {

		if (down_interruptible(&dev_context->isr_lock))
			return -ERESTARTSYS;
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			status = -ENODEV;
		} else {
			dev_context->debug_i2c_xfer_length = length;
			status = count;
		}
		up(&dev_context->isr_lock);
	}


	return status;
}

/*
 * show_reg_access_length()	- Show the current page number to be used when
 *	reg_access/data is accessed.
 *
 */
ssize_t show_reg_access_length(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	MHL_TX_COMM_INFO("called\n");

	status = scnprintf(buf, PAGE_SIZE, "0x%02x",
		dev_context->debug_i2c_xfer_length);

	return status;
}

/*
 * set_reg_access_data() - Handle write request to the
 *	reg_access_data attribute file.
 *
 * This file is used to either perform a write to registers of the transmitter
 * or to set the address, offset and byte count for a subsequent from the
 * register(s) of the transmitter.
 *
 * The format of the string in buf must be:
 *	data_byte_0 ... data_byte_length-1
 * Where: data_byte is a space separated list of <length_value> data bytes
 *			to be written.  If no data bytes are present then
 *			the write to this file will only be used to set
 *			the  page address, offset and length for a
 *			subsequent read from this file.
 */
ssize_t set_reg_access_data(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	unsigned long value;
	u8 data[MAX_DEBUG_TRANSFER_SIZE];
	int i;
	char *str;
	int status = -EINVAL;
	char *pinput = kmalloc(count, GFP_KERNEL);

	MHL_TX_COMM_INFO("received string: %c%s%c\n", '"', buf, '"');

	if (pinput == 0)
		return -EINVAL;
	memcpy(pinput, buf, count);

	/*
	 * Parse the input string and extract the scratch pad register
	 * selection parameters
	 */
	str = pinput;
	for (i = 0; (i < MAX_DEBUG_TRANSFER_SIZE) && ('\0' != *str); i++) {

		status = si_strtoul(&str, 0, &value);
		if (-ERANGE == status) {
			MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_reg_access_data;
		} else if (-EINVAL == status) {
			MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_reg_access_data;
		} else if (status != 0) {
			MHL_TX_DBG_ERR("status:%d %s%s%s\n", status,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_reg_access_data;
		} else if (value > 0xFF) {
			MHL_TX_DBG_ERR("value:0x%x str:%s%s%s\n", value,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_reg_access_data;
		} else {
			data[i] = value;
		}
	}

	if (i == 0) {
		MHL_TX_COMM_INFO("No data specified\n");
		goto exit_reg_access_data;
	}

	if (down_interruptible(&dev_context->isr_lock)) {
		status = -ERESTARTSYS;
		goto exit_reg_access_data;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {

		status = dev_context->drv_info->mhl_device_dbg_i2c_reg_xfer(
			&dev_context->drv_context,
			 dev_context->debug_i2c_address,
			 dev_context->debug_i2c_offset,
			 i, DEBUG_I2C_WRITE, data);
		if (status == 0)
			status = count;
	}

	up(&dev_context->isr_lock);

exit_reg_access_data:
	kfree(pinput);
	return status;
}

/*
 * show_reg_access_data()	- Handle read request to the
				reg_access_data attribute file.
 *
 * Reads from this file return one or more transmitter register values in
 * hexadecimal string format.  The registers returned are specified by the
 * address, offset and length values previously written to this file.
 *
 * The return value is the number characters written to buf, or an error
 * code if the I2C read fails.
 *
 * The format of the string returned in buf is:
 *	"address:<pageaddr> offset:<offset> length:<lenvalue> data:<datavalues>
 *	where:	<pageaddr>  is the last I2C register page address written
 *				to this file
 *		<offset>    is the last register offset written to this file
 *		<lenvalue>  is the last register transfer length written
 *				to this file
 *		<datavalue> space separated list of <lenvalue> register
 *				values in OxXX format
 */
ssize_t show_reg_access_data(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	u8 data[MAX_DEBUG_TRANSFER_SIZE] = {0};
	u8 idx;
	int status = -EINVAL;

	MHL_TX_COMM_INFO("called\n");

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto no_dev;
	}

	status = dev_context->drv_info->mhl_device_dbg_i2c_reg_xfer(
		&dev_context->drv_context,
		dev_context->debug_i2c_address,
		dev_context->debug_i2c_offset,
		dev_context->debug_i2c_xfer_length,
		DEBUG_I2C_READ,
		data);
no_dev:
	up(&dev_context->isr_lock);

	if (status == 0) {
		status = scnprintf(buf, PAGE_SIZE,
			"0x%02x'0x%02x:",
			dev_context->debug_i2c_address,
			dev_context->debug_i2c_offset
			);

		for (idx = 0; idx < dev_context->debug_i2c_xfer_length;
			idx++) {
			status += scnprintf(&buf[status], PAGE_SIZE, " 0x%02x",
				data[idx]);
		}
	}

	return status;
}

static struct device_attribute reg_access_page_attr =
	__ATTR(SYS_ATTR_NAME_REG_ACCESS_PAGE, 0666,
		show_reg_access_page, set_reg_access_page);

static struct device_attribute reg_access_offset_attr =
	__ATTR(SYS_ATTR_NAME_REG_ACCESS_OFFSET, 0666,
		show_reg_access_offset, set_reg_access_offset);

static struct device_attribute reg_access_length_attr =
	__ATTR(SYS_ATTR_NAME_REG_ACCESS_LENGTH, 0666,
		show_reg_access_length, set_reg_access_length);

static struct device_attribute reg_access_data_attr =
	__ATTR(SYS_ATTR_NAME_REG_ACCESS_DATA, 0666,
		show_reg_access_data, set_reg_access_data);

static struct attribute *reg_access_attrs[] = {
	&reg_access_page_attr.attr,
	&reg_access_offset_attr.attr,
	&reg_access_length_attr.attr,
	&reg_access_data_attr.attr,
	NULL
};

static struct attribute_group reg_access_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_REG_ACCESS),
	.attrs = reg_access_attrs
};
/*
 * show_debug_level() - Handle read request to the debug_level attribute file.
 *
 * The return value is the number characters written to buf, or EAGAIN
 * if the driver is busy and cannot service the read request immediately.
 * If EAGAIN is returned the caller should wait a little and retry the
 * read.
 */
ssize_t get_debug_level(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
		goto err_exit;
	}

	status = scnprintf(buf, PAGE_SIZE, "level=%d %s",
			   debug_level,
			   debug_reg_dump ? "(includes reg dump)" : "");
	MHL_TX_DBG_INFO("buf:%c%s%c\n", '"', buf, '"');

err_exit:
	up(&dev_context->isr_lock);
/* this should be a separate sysfs!!!
	si_dump_important_regs((struct drv_hw_context *)&dev_context->
			       drv_context);
*/

	return status;
}

/*
 * set_debug_level() - Handle write request to the debug_level attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t set_debug_level(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	long new_debug_level = 0x100;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (buf != NULL) {
		/* BUGZILLA 27012 no prefix here, only on module parameter. */
		status = kstrtol(buf, 0, &new_debug_level);
		if (status != 0) {
			MHL_TX_DBG_ERR("Invalid debug_level: 0x%02lX\n",
				new_debug_level);
			goto err_exit;
		}
	} else {
		MHL_TX_DBG_ERR("Missing debug_level parameter\n");
		status = -EINVAL;
		goto err_exit;
	}

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}
	debug_level = new_debug_level;
	status = count;

	MHL_TX_GENERIC_DBG_PRINT(debug_level,
		"new debug_level=%d\n", debug_level);

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * show_debug_reg_dump()
 *
 * Handle read request to the debug_reg_dump attribute file.
 *
 * The return value is the number characters written to buf, or EAGAIN
 * if the driver is busy and cannot service the read request immediately.
 * If EAGAIN is returned the caller should wait a little and retry the
 * read.
 */
ssize_t get_debug_reg_dump(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
		goto err_exit;
	}

	status = scnprintf(buf, PAGE_SIZE, "%s",
			   debug_reg_dump ? "(includes reg dump)" : "");
	MHL_TX_DBG_INFO("buf:%c%s%c\n", '"', buf, '"');

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * set_debug_reg_dump()
 *
 * Handle write request to the debug_reg_dump attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t set_debug_reg_dump(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long new_debug_reg_dump = 0x100;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &new_debug_reg_dump);
	if (status != 0)
		goto err_exit2;

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}
	debug_reg_dump = (new_debug_reg_dump > 0) ? true : false;
	MHL_TX_DBG_ERR("debug dump %s\n", debug_reg_dump ? "ON" : "OFF");

	status = count;
err_exit:
	up(&dev_context->isr_lock);
err_exit2:
	return status;
}

/*
 * show_gpio_index() - Handle read request to the gpio_index attribute file.
 *
 * The return value is the number characters written to buf, or EAGAIN
 * if the driver is busy and cannot service the read request immediately.
 * If EAGAIN is returned the caller should wait a little and retry the
 * read.
 */
ssize_t get_gpio_index(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
		goto err_exit;
	}

	status = scnprintf(buf, PAGE_SIZE, "%d", gpio_index);
	MHL_TX_DBG_INFO("buf:%c%s%c\n", '"', buf, '"');

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * set_gpio_index() - Handle write request to the gpio_index attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t set_gpio_index(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long new_gpio_index;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}
	if (kstrtoul(buf, 0, &new_gpio_index) != 0)
		goto err_exit;

	gpio_index = new_gpio_index;
	MHL_TX_DBG_INFO("gpio: %d\n", gpio_index);

	status = count;
err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * show_gpio_level() - Handle read request to the gpio_level attribute file.
 *
 * The return value is the number characters written to buf, or EAGAIN
 * if the driver is busy and cannot service the read request immediately.
 * If EAGAIN is returned the caller should wait a little and retry the
 * read.
 */
ssize_t get_gpio_level(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;
	int gpio_level;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
		goto err_exit;
	}
	gpio_level = gpio_get_value(gpio_index);
	status = scnprintf(buf, PAGE_SIZE, "%d", gpio_level);
	MHL_TX_DBG_INFO("buf:%c%s%c\n", '"', buf, '"');

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * set_gpio_level() - Handle write request to the gpio_level attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t set_gpio_level(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long gpio_level;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto err_exit;
	}
	if (kstrtoul(buf, 0, &gpio_level) != 0)
		goto err_exit;

	MHL_TX_DBG_INFO("gpio: %d<-%d\n", gpio_index, gpio_level);
	gpio_set_value(gpio_index, gpio_level);
	status = count;

err_exit:
	up(&dev_context->isr_lock);

	return status;
}

/*
 * show_pp_16bpp() - Handle read request to the gpio_level attribute file.
 *
 * The return value is the number characters written to buf, or EAGAIN
 * if the driver is busy and cannot service the read request immediately.
 * If EAGAIN is returned the caller should wait a little and retry the
 * read.
 */
ssize_t show_pp_16bpp(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;
	int pp_16bpp;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		pp_16bpp = si_mhl_tx_drv_get_pp_16bpp_override(dev_context);
		status = scnprintf(buf, PAGE_SIZE, "%d", pp_16bpp);
		MHL_TX_DBG_INFO("buf:%c%s%c\n", '"', buf, '"');
	}

	up(&dev_context->isr_lock);

	return status;
}

/*
 * set_pp_16bpp() - Handle write request to the gpio_level attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t set_pp_16bpp(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;
	unsigned long pp_16bpp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &pp_16bpp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n"
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			si_mhl_tx_drv_set_pp_16bpp_override(dev_context,
				pp_16bpp);
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

/*
 * show_aksv()	- Handle read request to the aksv attribute file.
 *
 * Reads from this file return the 5 bytes of the transmitter's
 * Key Selection Vector (KSV). The registers are returned as a string
 * of space separated list of hexadecimal values formated as 0xXX.
 *
 * The return value is the number characters written to buf.
 */
ssize_t show_aksv(struct device *dev, struct device_attribute *attr,
		  char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	u8 data[5] = {0};
	u8 idx;
	int status = -EINVAL;

	MHL_TX_DBG_INFO("called\n");

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto no_dev;
	}

	status = dev_context->drv_info->mhl_device_get_aksv(
		(struct drv_hw_context *)&dev_context->drv_context, data);
no_dev:
	up(&dev_context->isr_lock);

	if (status == 0) {

		for (idx = 0; idx < 5; idx++) {
			status += scnprintf(&buf[status], PAGE_SIZE, "0x%02x ",
					    data[idx]);
		}
	}

	return status;
}

/*
 * show_edid()	- Handle read request to the aksv attribute file.
 *
 * Reads from this file return the edid, as processed by this driver and
 *	presented upon the upstream DDC interface.
 *
 * The return value is the number characters written to buf.
 */
ssize_t show_edid(struct device *dev, struct device_attribute *attr,
		  char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	u8 edid_buffer[256] = {0};
	int status = -EINVAL;

	MHL_TX_DBG_INFO("called\n");

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		status = si_mhl_tx_drv_sample_edid_buffer(
			(struct drv_hw_context *)&dev_context->drv_context,
			edid_buffer);
	}

	up(&dev_context->isr_lock);

	if (status == 0) {
		int idx, i;
		for (idx = 0, i = 0; i < 16; i++) {
			u8 j;
			for (j = 0; j < 16; ++j, ++idx) {
				status += scnprintf(&buf[status],
					PAGE_SIZE, "0x%02x ",
					edid_buffer[idx]);
			}
			status += scnprintf(&buf[status], PAGE_SIZE, "\n");
		}
	}

	return status;
}

/*
 * show_hev_3d()
 *
 * Reads from this file return the HEV_VIC, HEV_DTD,3D_VIC, and
 *	3D_DTD WRITE_BURST information presented in a format showing the
 *	respective associations.
 *
 * The return value is the number characters written to buf.
 */
ssize_t show_hev_3d(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	MHL_TX_DBG_INFO("called\n");

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		struct edid_3d_data_t *p_edid_data =
			dev_context->edid_parser_context;
		int i;
		status = 0;
		status += scnprintf(&buf[status], PAGE_SIZE, "HEV_DTD list:\n");
		for (i = 0; i < (int)p_edid_data->hev_dtd_info.num_items; ++i) {

			status += scnprintf(&buf[status], PAGE_SIZE,
				"%s %s %s 0x%02x 0x%04x 0x%04x 0x%04x 0x%04x "
				"0x%04x 0x%02x -- 0x%04x 0x%02x 0x%02x 0x%02x "
				"0x%02x 0x%02x\n",
				p_edid_data->hev_dtd_list[i]._3d_info.vdi_l.
					top_bottom ? "TB" : "--",
				p_edid_data->hev_dtd_list[i]._3d_info.vdi_l.
					left_right ? "LR" : "--",
				p_edid_data->hev_dtd_list[i]._3d_info.vdi_l.
					frame_sequential ? "FS" : "--",
				p_edid_data->hev_dtd_list[i].sequence_index,
				ENDIAN_CONVERT_16(p_edid_data->hev_dtd_list[i].
					a.pixel_clock_in_MHz),
				ENDIAN_CONVERT_16(p_edid_data->hev_dtd_list[i].
					a.h_active_in_pixels),
				ENDIAN_CONVERT_16(p_edid_data->hev_dtd_list[i].
					a.h_blank_in_pixels),
				ENDIAN_CONVERT_16(p_edid_data->hev_dtd_list[i].
					a.h_front_porch_in_pixels),
				ENDIAN_CONVERT_16(p_edid_data->hev_dtd_list[i].
					a.h_sync_width_in_pixels),
				p_edid_data->hev_dtd_list[i].a.h_flags,
				ENDIAN_CONVERT_16(p_edid_data->hev_dtd_list[i].
					b.v_total_in_lines),
				p_edid_data->hev_dtd_list[i].b.
					v_blank_in_lines,
				p_edid_data->hev_dtd_list[i].b.
					v_front_porch_in_lines,
				p_edid_data->hev_dtd_list[i].b.
					v_sync_width_in_lines,
				p_edid_data->hev_dtd_list[i].b.
					v_refresh_rate_in_fields_per_second,
				p_edid_data->hev_dtd_list[i].b.v_flags);
		}

		status += scnprintf(&buf[status], PAGE_SIZE, "HEV_VIC list:\n");
		for (i = 0; i < (int)p_edid_data->hev_vic_info.num_items; ++i) {
			status += scnprintf(&buf[status], PAGE_SIZE,
			"%s %s %s 0x%02x 0x%02x\n",
			p_edid_data->hev_vic_list[i]._3d_info.vdi_l.
				top_bottom ? "TB" : "--",
			p_edid_data->hev_vic_list[i]._3d_info.vdi_l.
				left_right ? "LR" : "--",
			p_edid_data->hev_vic_list[i]._3d_info.vdi_l.
				frame_sequential ? "FS" : "--",
			p_edid_data->hev_vic_list[i].mhl3_hev_vic_descriptor.
				vic_cea861f,
			p_edid_data->hev_vic_list[i].mhl3_hev_vic_descriptor.
				reserved);
		}

		status += scnprintf(&buf[status], PAGE_SIZE, "3D_DTD list:\n");
		for (i = 0; i < (int)p_edid_data->_3d_dtd_info.num_items; ++i) {
			status += scnprintf(&buf[status], PAGE_SIZE,
				/* pixel clk */
				"%s %s %s " "0x%02x 0x%02x "
				/* horizontal active and blanking */
				"0x%02x 0x%02x {0x%1x 0x%1x} "
				/* vertical active and blanking */
				"0x%02x 0x%02x {0x%1x 0x%1x} "
				/* sync pulse width and offset */
				"0x%02x 0x%02x {0x%1x 0x%1x} "
				"{0x%1x 0x%1x 0x%1x 0x%1x} "
				/* Image sizes */
				"0x%02x 0x%02x {0x%1x 0x%1x} "
				/* borders and flags */
				"0x%1x 0x%1x {0x%1x 0x%1x 0x%1x 0x%1x %s}\n",
				p_edid_data->_3d_dtd_list[i]._3d_info.vdi_l.
					top_bottom ? "TB" : "--",
				p_edid_data->_3d_dtd_list[i]._3d_info.vdi_l.
					left_right ? "LR" : "--",
				p_edid_data->_3d_dtd_list[i]._3d_info.vdi_l.
					frame_sequential ? "FS" : "--",
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					pixel_clock_low,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					pixel_clock_high,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					horz_active_7_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					horz_blanking_7_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					horz_active_blanking_high.
					horz_blanking_11_8,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					horz_active_blanking_high.
					horz_active_11_8,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					vert_active_7_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					vert_blanking_7_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					vert_active_blanking_high.
					vert_blanking_11_8,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					vert_active_blanking_high.
					vert_active_11_8,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					horz_sync_offset_7_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					horz_sync_pulse_width7_0,
				p_edid_data->_3d_dtd_list[i].
					dtd_cea_861.vert_sync_offset_width.
					vert_sync_pulse_width_3_0,
				p_edid_data->_3d_dtd_list[i].
					dtd_cea_861.vert_sync_offset_width.
					vert_sync_offset_3_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					hs_vs_offset_pulse_width.
					vert_sync_pulse_width_5_4,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					hs_vs_offset_pulse_width.
					vert_sync_offset_5_4,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					hs_vs_offset_pulse_width.
					horz_sync_pulse_width_9_8,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					hs_vs_offset_pulse_width.
					horz_sync_offset_9_8,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					horz_image_size_in_mm_7_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					vert_image_size_in_mm_7_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					image_size_high.
					vert_image_size_in_mm_11_8,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					image_size_high.
					horz_image_size_in_mm_11_8,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					horz_border_in_lines,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.
					vert_border_in_pixels,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.flags.
					stereo_bit_0,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.flags.
					sync_signal_options,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.flags.
					sync_signal_type,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.flags.
					stereo_bits_2_1,
				p_edid_data->_3d_dtd_list[i].dtd_cea_861.flags.
					interlaced ? "interlaced" :
					"progressive");
		}

		status += scnprintf(&buf[status], PAGE_SIZE, "3d_VIC list:\n");
		for (i = 0; i < (int)p_edid_data->_3d_vic_info.num_items; ++i) {
			status +=
			    scnprintf(&buf[status], PAGE_SIZE,
				      "%s %s %s %s 0x%02x\n",
				      p_edid_data->_3d_vic_list[i]._3d_info.
				      vdi_l.top_bottom ? "TB" : "--",
				      p_edid_data->_3d_vic_list[i]._3d_info.
				      vdi_l.left_right ? "LR" : "--",
				      p_edid_data->_3d_vic_list[i]._3d_info.
				      vdi_l.frame_sequential ? "FS" : "--",
				      p_edid_data->_3d_vic_list[i].svd.
				      native ? "N" : " ",
				      p_edid_data->_3d_vic_list[i].svd.VIC);
		}
	}

	up(&dev_context->isr_lock);
	return status;
}

#ifdef DEBUG
/*
 * set_tx_power() - Handle write request to the tx_power attribute file.
 *
 * Write the string "on" or "off" to this file to power on or off the
 * MHL transmitter.
 *
 * The return value is the number of characters in buf if successful or an
 * error code if not successful.
 */
ssize_t set_tx_power(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = 0;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else if (strnicmp("on", buf, count - 1) == 0) {
		status = si_8620_power_control(true);
	} else if (strnicmp("off", buf, count - 1) == 0) {
		status = si_8620_power_control(false);
	} else {
		MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
		status = -EINVAL;
	}

	if (status != 0)
		return status;
	else
		return count;
}

/*
 * set_stark_ctl() - Handle write request to the tx_power attribute file.
 *
 * Write the string "on" or "off" to this file to power on or off the
 * MHL transmitter.
 *
 * The return value is the number of characters in buf if successful or an
 * error code if not successful.
 */
ssize_t set_stark_ctl(struct device *dev, struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = 0;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		if (strnicmp("on", buf, count - 1) == 0) {
			/*
			 * Set MHL/USB switch to USB
			 * NOTE: Switch control is implemented differently on
			 * each version of the starter kit.
			 */
			set_pin(X02_USB_SW_CTRL, 1);
		} else if (strnicmp("off", buf, count - 1) == 0) {
			/*
			 * Set MHL/USB switch to USB
			 * NOTE: Switch control is implemented differently on
			 * each version of the starter kit.
			 */
			set_pin(X02_USB_SW_CTRL, 0);
		} else {
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
		}
	}

	up(&dev_context->isr_lock);

	if (status != 0)
		return status;
	else
		return count;
}
#endif

ssize_t show_bist_ecbus_duration(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n",
				dev_context->sysfs_bist_setup.e_cbus_duration);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_bist_ecbus_duration(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long temp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &temp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (temp > 0xFF) {
		MHL_TX_DBG_ERR("temp:0x%x buf:%s%s%s\n", temp,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			dev_context->sysfs_bist_setup.e_cbus_duration = temp;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

ssize_t show_bist_ecbus_pattern(struct device *dev,
		 struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%04x\n",
				dev_context->sysfs_bist_setup.e_cbus_pattern);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_bist_ecbus_pattern(struct device *dev,
		struct device_attribute *attr,  const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long temp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &temp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (temp > 0xFF) {
		MHL_TX_DBG_ERR("temp:0x%x buf:%s%s%s\n", temp,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			dev_context->sysfs_bist_setup.e_cbus_pattern = temp;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;

}

ssize_t show_bist_ecbus_fixed_pattern(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%04x\n",
				dev_context->sysfs_bist_setup.e_cbus_fixed_pat);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_bist_ecbus_fixed_pattern(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long temp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &temp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (temp > 0xFFFF) {
		MHL_TX_DBG_ERR("temp:0x%x buf:%s%s%s\n", temp,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			dev_context->sysfs_bist_setup.e_cbus_fixed_pat = temp;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

ssize_t show_bist_av_link_data_rate(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE,
				"avlink_data_rate:   0x%02x\n",
				dev_context->sysfs_bist_setup.avlink_data_rate);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_bist_av_link_data_rate(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long temp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &temp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (temp > 0xFF) {
		MHL_TX_DBG_ERR("temp:0x%x buf:%s%s%s\n", temp,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			dev_context->sysfs_bist_setup.avlink_data_rate = temp;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

ssize_t show_bist_av_link_pattern(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n",
				dev_context->sysfs_bist_setup.avlink_pattern);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_bist_av_link_pattern(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long temp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &temp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (temp > 0xFF) {
		MHL_TX_DBG_ERR("temp:0x%x buf:%s%s%s\n", temp,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			dev_context->sysfs_bist_setup.avlink_pattern = temp;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

ssize_t show_bist_av_link_video_mode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			dev_context->sysfs_bist_setup.avlink_video_mode);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_bist_av_link_video_mode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long temp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &temp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (temp > 0xFF) {
		MHL_TX_DBG_ERR("temp:0x%x buf:%s%s%s\n", temp,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			dev_context->sysfs_bist_setup.avlink_video_mode = temp;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

ssize_t show_bist_av_link_duration(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n",
				dev_context->sysfs_bist_setup.avlink_duration);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_bist_av_link_duration(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long temp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &temp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (temp > 0xFF) {
		MHL_TX_DBG_ERR("temp:0x%x buf:%s%s%s\n", temp,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			dev_context->sysfs_bist_setup.avlink_duration = temp;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

ssize_t show_bist_av_link_fixed_pattern(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%04x\n",
				dev_context->sysfs_bist_setup.avlink_fixed_pat);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_bist_av_link_fixed_pattern(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long temp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &temp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (temp > 0xFFFF) {
		MHL_TX_DBG_ERR("temp:0x%x buf:%s%s%s\n", temp,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			dev_context->sysfs_bist_setup.avlink_fixed_pat = temp;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

ssize_t show_bist_av_link_randomizer(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			dev_context->sysfs_bist_setup.avlink_randomizer);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_bist_av_link_randomizer(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long temp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &temp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (temp > 0xFF) {
		MHL_TX_DBG_ERR("temp:0x%x buf:%s%s%s\n", temp,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			dev_context->sysfs_bist_setup.avlink_randomizer = temp;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

ssize_t show_bist_impedance_mode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n",
				dev_context->sysfs_bist_setup.impedance_mode);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_bist_impedance_mode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long temp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &temp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (temp > 0xFF) {
		MHL_TX_DBG_ERR("temp:0x%x buf:%s%s%s\n", temp,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			dev_context->sysfs_bist_setup.impedance_mode = temp;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

ssize_t set_bist_setup(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long temp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &temp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (temp > 0xFF) {
		MHL_TX_DBG_ERR("temp:0x%x buf:%s%s%s\n", temp,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			MHL_TX_DBG_ERR("\n");
			si_mhl_tx_execute_bist(dev_context,
				&dev_context->sysfs_bist_setup);
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

ssize_t set_bist_trigger(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long temp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &temp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (temp > 0xFF) {
		MHL_TX_DBG_ERR("temp:0x%x buf:%s%s%s\n", temp,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			dev_context->sysfs_bist_setup.bist_trigger_parm =
				(u8)temp;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

ssize_t set_bist_stop(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long temp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &temp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (temp > 0xFF) {
		MHL_TX_DBG_ERR("temp:0x%x buf:%s%s%s\n", temp,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			status = si_mhl_tx_bist_stop(dev_context);
			MHL_TX_DBG_ERR("stop status: %d\n", status);
			if (status == BIST_STATUS_NO_ERROR)
				status = count;
			else
				status = -EINVAL;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

ssize_t show_bist_status(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "unimplemented");
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_bist_status_req(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long temp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &temp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (temp > 0xFF) {
		MHL_TX_DBG_ERR("temp:0x%x buf:%s%s%s\n", temp,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			dev_context->sysfs_bist_setup.bist_stat_parm =
				(u8) temp;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

ssize_t show_t_bist_mode_down(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "%d",
				dev_context->sysfs_bist_setup.t_bist_mode_down);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_t_bist_mode_down(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long temp;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	status = kstrtoul(buf, 0, &temp);
	if (-ERANGE == status) {
		MHL_TX_DBG_ERR("ERANGE %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		MHL_TX_DBG_ERR("EINVAL %s%s%s\n",
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		MHL_TX_DBG_ERR("status:%d buf:%s%s%s\n", status,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (temp > 0xFF) {
		MHL_TX_DBG_ERR("temp:0x%x buf:%s%s%s\n", temp,
			ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else {
		if (down_interruptible(&dev_context->isr_lock)) {
			MHL_TX_DBG_ERR("%scould not get mutex%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return -ERESTARTSYS;
		}
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			MHL_TX_DBG_ERR("%sDEV_FLAG_SHUTDOWN%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			status = -ENODEV;
		} else {
			dev_context->sysfs_bist_setup.t_bist_mode_down = temp;
			status = count;
		}
		up(&dev_context->isr_lock);
	}

	return status;
}

static struct device_attribute bist_ecbus_duration_attr =
	__ATTR(SYS_ATTR_NAME_BIST_ECBUS_DUR,
		0666, show_bist_ecbus_duration, set_bist_ecbus_duration);
static struct device_attribute bist_ecbus_pattern_attr =
	__ATTR(SYS_ATTR_NAME_BIST_ECBUS_PAT,
		0666, show_bist_ecbus_pattern, set_bist_ecbus_pattern);
static struct device_attribute bist_ecbus_fixed_pattern_attr =
	__ATTR(SYS_ATTR_NAME_BIST_ECBUS_FPAT, 0666,
		show_bist_ecbus_fixed_pattern, set_bist_ecbus_fixed_pattern);
static struct device_attribute bist_av_link_data_rate_attr =
	__ATTR(SYS_ATTR_NAME_BIST_AV_LINK_DR, 0666,
		show_bist_av_link_data_rate, set_bist_av_link_data_rate);
static struct device_attribute bist_av_link_pattern_attr =
	__ATTR(SYS_ATTR_NAME_BIST_AV_LINK_PAT, 0666,
		show_bist_av_link_pattern, set_bist_av_link_pattern);
static struct device_attribute bist_av_link_video_mode_attr =
	__ATTR(SYS_ATTR_NAME_BIST_AV_LINK_VM, 0666,
		show_bist_av_link_video_mode, set_bist_av_link_video_mode);
static struct device_attribute bist_av_link_duration_attr =
	__ATTR(SYS_ATTR_NAME_BIST_AV_LINK_DUR, 0666,
		show_bist_av_link_duration, set_bist_av_link_duration);
static struct device_attribute bist_av_link_fixed_pattern_attr =
	__ATTR(SYS_ATTR_NAME_BIST_AV_LINK_FPAT, 0666,
		show_bist_av_link_fixed_pattern,
		set_bist_av_link_fixed_pattern);
static struct device_attribute bist_av_link_randomizer_attr =
	__ATTR(SYS_ATTR_NAME_BIST_AV_LINK_RDZR, 0666,
		show_bist_av_link_randomizer, set_bist_av_link_randomizer);
static struct device_attribute bist_impedance_mode_attr =
	__ATTR(SYS_ATTR_NAME_BIST_AV_LINK_IMPM, 0666,
		show_bist_impedance_mode, set_bist_impedance_mode);
static struct device_attribute bist_setup_attr =
	__ATTR(SYS_ATTR_NAME_BIST_SETUP, 0222,
		NULL, set_bist_setup);
static struct device_attribute bist_trigger_attr =
	__ATTR(SYS_ATTR_NAME_BIST_TRIGGER, 0222,
		NULL, set_bist_trigger);
static struct device_attribute bist_stop_attr =
	__ATTR(SYS_ATTR_NAME_BIST_STOP, 0222,
		NULL, set_bist_stop);
static struct device_attribute bist_status_attr =
	__ATTR(SYS_ATTR_NAME_BIST_STATUS_REQ, 0666,
		show_bist_status, set_bist_status_req);

static struct device_attribute t_bist_mode_down_attr =
	__ATTR(SYS_ATTR_NAME_T_BIST_MODE_DOWN, 0666,
	show_t_bist_mode_down, set_t_bist_mode_down);

static struct attribute *bist_attrs[] = {
	&bist_ecbus_duration_attr.attr,
	&bist_ecbus_pattern_attr.attr,
	&bist_ecbus_fixed_pattern_attr.attr,
	&bist_av_link_data_rate_attr.attr,
	&bist_av_link_pattern_attr.attr,
	&bist_av_link_video_mode_attr.attr,
	&bist_av_link_duration_attr.attr,
	&bist_av_link_fixed_pattern_attr.attr,
	&bist_av_link_randomizer_attr.attr,
	&bist_impedance_mode_attr.attr,
	&bist_setup_attr.attr,
	&bist_trigger_attr.attr,
	&bist_stop_attr.attr,
	&bist_status_attr.attr,
	&t_bist_mode_down_attr.attr,
	NULL
};

static struct attribute_group bist_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_BIST),
	.attrs = bist_attrs
};


#if (INCLUDE_SII6031 == 1)
static int mhl_tx_discover_mhl_device(void *mhl_ctx, int id,
	void (*mhl_notify_cb)(void *, int), void *motg)
{
	struct	mhl_dev_context *dev_context;
	int	remaining_time;

	dev_context = (struct mhl_dev_context *)mhl_ctx;
	dev_context->mhl_discovery_in_progress = true;
	init_completion(&dev_context->sem_mhl_discovery_complete);

	remaining_time = wait_for_completion_interruptible_timeout(
		&dev_context->sem_mhl_discovery_complete, 2 * HZ);
	dev_context->mhl_discovery_in_progress = false;
	if (dev_context->mhl_detected) {
		pr_debug("mhl driver detected mhl connection\n");
		return 1;
	} else
		return 0;
}

void mhl_tx_notify_otg(struct mhl_dev_context *dev_context, bool mhl_detected)
{
	dev_context->mhl_detected = mhl_detected;
	if (dev_context->mhl_discovery_in_progress)
		complete(&dev_context->sem_mhl_discovery_complete);
	otg_mhl_notify(dev_context, mhl_detected);
}

static void mhl_tx_register_mhl_discovery(struct mhl_dev_context *dev_context)
{
	if (otg_register_mhl_discovery((void *)dev_context,
		mhl_tx_discover_mhl_device))
		pr_debug("%s: USB callback registration failed\n", __func__);
}
#endif
#define MAX_EVENT_STRING_LEN 128
/*
 * Handler for event notifications from the MhlTx layer.
 *
 */
void mhl_event_notify(struct mhl_dev_context *dev_context, u32 event,
	u32 event_param, void *data)
{
	char event_string[MAX_EVENT_STRING_LEN];
	char *envp[] = { event_string, NULL };
	char *buf;
	u32 length;
	u32 count;
	int idx;

	MHL_TX_DBG_INFO("called, event: 0x%08x event_param: 0x%08x\n",
		event, event_param);

	switch (event) {

	case MHL_TX_EVENT_CONNECTION:
		dev_context->mhl_flags |= MHL_STATE_FLAG_CONNECTED;
		init_rcp_input_dev(dev_context);
		sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
			__stringify(SYS_ATTR_NAME_CONN));

		strncpy(event_string, "MHLEVENT=connected",
			MAX_EVENT_STRING_LEN);
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE,
			envp);
#if (INCLUDE_SII6031 == 1)
		mhl_tx_notify_otg(dev_context, true);
#endif
		break;

	case MHL_TX_EVENT_DISCONNECTION:
		dev_context->mhl_flags = 0;
#if (INCLUDE_RBP == 1)
		dev_context->rbp_in_button_code = 0;
		dev_context->rbp_out_button_code = 0;
		dev_context->rbp_err_code = 0;
		dev_context->rbp_send_status = 0;
#endif
		dev_context->rcp_in_key_code = 0;
		dev_context->rcp_out_key_code = 0;
		dev_context->rcp_err_code = 0;
		dev_context->rcp_send_status = 0;
		dev_context->ucp_in_key_code = 0;
		dev_context->ucp_out_key_code = 0;
		dev_context->ucp_err_code = 0;
		dev_context->spad_send_status = 0;

#if (INCLUDE_HID == 1)
		mhl3_hid_remove_all(dev_context);
#endif
#ifdef MEDIA_DATA_TUNNEL_SUPPORT
		mdt_destroy(dev_context);
#endif
#if (INCLUDE_SII6031 == 1)
		mhl_tx_notify_otg(dev_context, false);
#endif
		destroy_rcp_input_dev(dev_context);
		sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
			__stringify(SYS_ATTR_NAME_CONN));

		strncpy(event_string, "MHLEVENT=disconnected",
			MAX_EVENT_STRING_LEN);
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE,
			envp);
		break;

	case MHL_TX_EVENT_RCP_RECEIVED:
		if (input_dev_rcp) {
			int result;

			result = generate_rcp_input_event(dev_context,
				(uint8_t) event_param);
			if (0 == result)
				si_mhl_tx_rcpk_send(dev_context,
					(uint8_t) event_param);
			else
				si_mhl_tx_rcpe_send(dev_context,
					MHL_RCPE_STATUS_INEFFECTIVE_KEY_CODE);
		} else {
			dev_context->mhl_flags &= ~MHL_STATE_FLAG_RCP_SENT;
			dev_context->mhl_flags |= MHL_STATE_FLAG_RCP_RECEIVED;
			dev_context->rcp_in_key_code = event_param;

			sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
				__stringify(SYS_ATTR_NAME_RCP));

			snprintf(event_string, MAX_EVENT_STRING_LEN,
				"MHLEVENT=received_RCP key code=0x%02x",
				event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj,
				KOBJ_CHANGE, envp);
		}
		break;

	case MHL_TX_EVENT_RCPK_RECEIVED:
		if ((dev_context->mhl_flags & MHL_STATE_FLAG_RCP_SENT)
		    && (dev_context->rcp_out_key_code == event_param)) {

			if (!(dev_context->mhl_flags & MHL_STATE_FLAG_RCP_NAK))
					dev_context->rcp_err_code = 0;
			dev_context->mhl_flags |= MHL_STATE_FLAG_RCP_ACK;
			dev_context->mhl_flags &= ~MHL_STATE_FLAG_RCP_SENT;
			MHL_TX_DBG_INFO(
				"Generating RCPK rcvd event, keycode: 0x%02x\n",
				event_param);

			sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
				__stringify(SYS_ATTR_NAME_RCPK));

			snprintf(event_string, MAX_EVENT_STRING_LEN,
				"MHLEVENT=received_RCPK key code=0x%02x",
				event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj,
				KOBJ_CHANGE, envp);
		} else {
			MHL_TX_DBG_ERR(
				"Unexpected RCPK rcvd event, keycode: 0x%02x\n",
				event_param);
		}
		break;

	case MHL_TX_EVENT_RCPE_RECEIVED:
		if (input_dev_rcp) {
			/* do nothing */
		} else {
			if (dev_context->mhl_flags & MHL_STATE_FLAG_RCP_SENT) {

				dev_context->rcp_err_code = event_param;
				dev_context->mhl_flags |=
					MHL_STATE_FLAG_RCP_NAK;

				MHL_TX_DBG_INFO(
					"Generating RCPE received event, "
					"error code: 0x%02x\n", event_param);

				sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
					__stringify(SYS_ATTR_NAME_RCPK));

				snprintf(event_string, MAX_EVENT_STRING_LEN,
					"MHLEVENT=rcvd_RCPE error code=0x%02x",
					event_param);
				kobject_uevent_env(&dev_context->mhl_dev->kobj,
					KOBJ_CHANGE, envp);
			} else {
				MHL_TX_DBG_ERR(
					"Ignoring unexpected RCPE received "
					"event, error code: 0x%02x\n",
					event_param);
			}
		}
		break;

	case MHL_TX_EVENT_UCP_RECEIVED:
		if (input_dev_ucp) {
			if (event_param <= 0xFF)
				si_mhl_tx_ucpk_send(dev_context,
					(uint8_t) event_param);
			else
				si_mhl_tx_ucpe_send(dev_context,
					MHL_UCPE_STATUS_INEFFECTIVE_KEY_CODE);
		} else {
			dev_context->mhl_flags &= ~MHL_STATE_FLAG_UCP_SENT;
			dev_context->mhl_flags |= MHL_STATE_FLAG_UCP_RECEIVED;
			dev_context->ucp_in_key_code = event_param;
			sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
				__stringify(SYS_ATTR_NAME_UCP));

			snprintf(event_string, MAX_EVENT_STRING_LEN,
				"MHLEVENT=received_UCP key code=0x%02x",
				event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj,
				KOBJ_CHANGE, envp);
		}
		break;

	case MHL_TX_EVENT_UCPK_RECEIVED:
		if ((dev_context->mhl_flags & MHL_STATE_FLAG_UCP_SENT) &&
			(dev_context->ucp_out_key_code == event_param)) {

			dev_context->mhl_flags |= MHL_STATE_FLAG_UCP_ACK;

			MHL_TX_DBG_INFO("Generating UCPK received event, "
				"keycode: 0x%02x\n", event_param);

			sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
				__stringify(SYS_ATTR_NAME_UCPK));

			snprintf(event_string, MAX_EVENT_STRING_LEN,
				 "MHLEVENT=received_UCPK key code=0x%02x",
				 event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj,
				KOBJ_CHANGE, envp);
		} else {
			MHL_TX_DBG_ERR("Ignoring unexpected UCPK received "
				"event, keycode: 0x%02x\n", event_param);
		}
		break;

	case MHL_TX_EVENT_UCPE_RECEIVED:
		if (dev_context->mhl_flags & MHL_STATE_FLAG_UCP_SENT) {

			dev_context->ucp_err_code = event_param;
			dev_context->mhl_flags |= MHL_STATE_FLAG_UCP_NAK;

			MHL_TX_DBG_INFO("Generating UCPE received event, "
				"error code: 0x%02x\n", event_param);

			sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
				__stringify(SYS_ATTR_NAME_UCPK));

			snprintf(event_string, MAX_EVENT_STRING_LEN,
				"MHLEVENT=received_UCPE error code=0x%02x",
				event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj,
				KOBJ_CHANGE, envp);
		} else {
			MHL_TX_DBG_ERR("Ignoring unexpected UCPE received "
				"event, error code: 0x%02x\n",
				event_param);
		}
		break;
#if (INCLUDE_RBP == 1)
	case MHL_TX_EVENT_RBP_RECEIVED:
		if (input_dev_rbp) {
			if (((event_param >= 0x01) && (event_param <= 0x07)) ||
			    ((event_param >= 0x20) && (event_param <= 0x21)) ||
			    ((event_param >= 0x30) && (event_param <= 0x35)))
				si_mhl_tx_rbpk_send(dev_context,
					(uint8_t) event_param);
			else
				si_mhl_tx_rbpe_send(dev_context,
					MHL_RBPE_STATUS_INEFFECTIVE_BUTTON_CODE
					);
		} else {
			dev_context->mhl_flags |= MHL_STATE_FLAG_RBP_RECEIVED;
			dev_context->rbp_in_button_code = event_param;

			sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
				__stringify(SYS_ATTR_NAME_RBP));

			snprintf(event_string, MAX_EVENT_STRING_LEN,
				"MHLEVENT=received_RBP button code=0x%02x",
				event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj,
				KOBJ_CHANGE, envp);
		}
		break;

	case MHL_TX_EVENT_RBPK_RECEIVED:
		MHL_TX_DBG_INFO("RBPK received\n");
		if ((dev_context->mhl_flags & MHL_STATE_FLAG_RBP_SENT)
		    && (dev_context->rbp_out_button_code == event_param)) {

			dev_context->mhl_flags |= MHL_STATE_FLAG_RBP_ACK;

			MHL_TX_DBG_INFO(
				"Generating RBPK rcvd event, button code: 0x%02x\n",
				event_param);

			sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
				__stringify(SYS_ATTR_NAME_RBPK));

			snprintf(event_string, MAX_EVENT_STRING_LEN,
				"MHLEVENT=received_RBPK button code=0x%02x",
				event_param);
			kobject_uevent_env(&dev_context->mhl_dev->kobj,
				KOBJ_CHANGE, envp);
		} else {
			MHL_TX_DBG_ERR(
				"Unexpected RBPK rcvd event, button code: 0x%02x\n",
				event_param);
		}
		break;

	case MHL_TX_EVENT_RBPE_RECEIVED:
		MHL_TX_DBG_INFO("RBPE received\n");
		if (input_dev_rbp) {
			/* do nothing */
		} else {
			if (dev_context->mhl_flags & MHL_STATE_FLAG_RBP_SENT) {

				dev_context->rbp_err_code = event_param;
				dev_context->mhl_flags |=
					MHL_STATE_FLAG_RBP_NAK;

				MHL_TX_DBG_INFO(
					"Generating RBPE received event, "
					"error code: 0x%02x\n", event_param);

				sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
					__stringify(SYS_ATTR_NAME_RBPK));

				snprintf(event_string, MAX_EVENT_STRING_LEN,
					"MHLEVENT=rcvd_RBPE error code=0x%02x",
					event_param);
				kobject_uevent_env(&dev_context->mhl_dev->kobj,
					KOBJ_CHANGE, envp);
			} else {
				MHL_TX_DBG_ERR(
					"Ignoring unexpected RBPE received "
					"event, error code: 0x%02x\n",
					event_param);
			}
		}
		break;
#endif
	case MHL_TX_EVENT_SPAD_RECEIVED:
		length = event_param;
		buf = data;

		sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
			__stringify(SYS_ATTR_NAME_SPAD));

		idx = snprintf(event_string, MAX_EVENT_STRING_LEN,
			"MHLEVENT=SPAD_CHG length=0x%02x data=", length);

		count = 0;
		while (buf && idx < MAX_EVENT_STRING_LEN) {
			if (count >= length)
				break;

			idx += snprintf(&event_string[idx],
				MAX_EVENT_STRING_LEN - idx, "0x%02x ",
				buf[count]);
			count++;
		}

		if (idx < MAX_EVENT_STRING_LEN) {
			kobject_uevent_env(&dev_context->mhl_dev->kobj,
				KOBJ_CHANGE, envp);
		} else {
			MHL_TX_DBG_ERR(
				"Buffer too small for scratch pad data!\n");
		}
		break;

	case MHL_TX_EVENT_POW_BIT_CHG:
		MHL_TX_DBG_INFO("Generating VBUS power bit change "
				"event, POW bit is %s\n",
				event_param ? "ON" : "OFF");
		snprintf(event_string, MAX_EVENT_STRING_LEN,
			 "MHLEVENT=MHL VBUS power %s",
			 event_param ? "ON" : "OFF");
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE,
				   envp);
		break;

	case MHL_TX_EVENT_RAP_RECEIVED:
		MHL_TX_DBG_INFO("Generating RAP received event, "
				"action code: 0x%02x\n", event_param);

		sysfs_notify(&dev_context->mhl_dev->kobj, NULL,
			     __stringify(SYS_ATTR_NAME_RAP_IN));

		snprintf(event_string, MAX_EVENT_STRING_LEN,
			 "MHLEVENT=received_RAP action code=0x%02x",
			 event_param);
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE,
				   envp);
		break;

	case MHL_TX_EVENT_BIST_READY_RECEIVED:
		MHL_TX_DBG_INFO("Received BIST_READY 0x%02X\n", event_param);
		snprintf(event_string, MAX_EVENT_STRING_LEN,
			 "MHLEVENT=received_BIST_READY code=0x%02x",
			 event_param);
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE,
				   envp);
		break;

	case MHL_TX_EVENT_BIST_TEST_DONE:
		MHL_TX_DBG_INFO("Triggered BIST test completed.\n");
		snprintf(event_string, MAX_EVENT_STRING_LEN,
			 "MHLEVENT=BIST_complete");
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE,
				   envp);
		break;

	case MHL_TX_EVENT_BIST_STATUS_RECEIVED:
		{
			struct bist_stat_info *stat =
			    (struct bist_stat_info *)data;
			MHL_TX_DBG_INFO("Received BIST_RETURN_STAT\n",
					event_param);
			if (stat != NULL) {
				MHL_TX_DBG_INFO
				    ("eCBUS Rx: 0x%04X,"
					" eCBUS Tx: 0x04X,"
					" AV_LINK: 0x%04X\n",
					stat->e_cbus_local_stat,
					stat->e_cbus_remote_stat,
					stat->avlink_stat);
				snprintf(event_string, MAX_EVENT_STRING_LEN,
					"MHLEVENT=received_BIST_STATUS "\
					"e_cbus_rx=0x%04x"
					"e_cbus_tx=0x%04x"
					" av_link=0x%04x",
					stat->e_cbus_local_stat,
					stat->e_cbus_remote_stat,
					stat->avlink_stat);
				kobject_uevent_env(&dev_context->
					mhl_dev->kobj, KOBJ_CHANGE, envp);
			} else {
				MHL_TX_DBG_INFO("\n");
			}
		}
		break;
	case MHL_TX_EVENT_T_RAP_MAX_EXPIRED:
		MHL_TX_DBG_INFO("T_RAP_MAX expired\n");
		snprintf(event_string, MAX_EVENT_STRING_LEN,
			 "MHLEVENT=T_RAP_MAX_EXPIRED");
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE,
				   envp);

		break;
	case MHL_EVENT_AUD_DELAY_RCVD:
		MHL_TX_DBG_INFO("Audio Delay burst received\n");
		snprintf(event_string, MAX_EVENT_STRING_LEN,
			 "MHLEVENT=AUD_DELAY_RCVD");
		kobject_uevent_env(&dev_context->mhl_dev->kobj, KOBJ_CHANGE,
				   envp);

		break;
	default:
		MHL_TX_DBG_ERR("called with unrecognized event code!\n");
		break;
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
	__ATTR(SYS_ATTR_NAME_CONN, 0666, show_connection_state,
		set_connection_state),
	__ATTR(SYS_ATTR_NAME_DSHPD, 0444, show_ds_hpd_state, NULL),
	__ATTR(SYS_ATTR_NAME_HDCP2, 0444, show_hdcp2_status, NULL),
	__ATTR(SYS_ATTR_NAME_SPAD, 0666, show_scratch_pad, send_scratch_pad),
	__ATTR(SYS_ATTR_NAME_DEBUG_LEVEL, 0666, get_debug_level,
	       set_debug_level),
	__ATTR(SYS_ATTR_NAME_REG_DEBUG_LEVEL, 0666, get_debug_reg_dump,
	       set_debug_reg_dump),
	__ATTR(SYS_ATTR_NAME_GPIO_INDEX, 0666, get_gpio_index, set_gpio_index),
	__ATTR(SYS_ATTR_NAME_GPIO_VALUE, 0666, get_gpio_level, set_gpio_level),
	__ATTR(SYS_ATTR_NAME_PP_16BPP  , 0666, show_pp_16bpp, set_pp_16bpp),
	__ATTR(SYS_ATTR_NAME_AKSV, 0444, show_aksv, NULL),
	__ATTR(SYS_ATTR_NAME_EDID, 0444, show_edid, NULL),
	__ATTR(SYS_ATTR_NAME_HEV_3D_DATA, 0444, show_hev_3d, NULL),
#ifdef DEBUG
	__ATTR(SYS_ATTR_NAME_TX_POWER, 0222, NULL, set_tx_power),
	__ATTR(SYS_ATTR_NAME_STARK_CTL, 0222, NULL, set_stark_ctl),
#endif

	__ATTR_NULL
};

ssize_t show_rap_in(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("could not acquire mutex!!!\n");
		return -ERESTARTSYS;
	}
	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			dev_context->rap_in_sub_command);
	}
	up(&dev_context->isr_lock);

	return status;
}

/*
 * set_rap_in_status() - Handle write request to the rap attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t set_rap_in_status(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		unsigned long param;
		if (kstrtoul(buf, 0, &param) != 0)
			param = INT_MAX;
		switch (param) {
		case 0x00:
			dev_context->mhl_flags &=
			    ~MHL_STATE_APPLICATION_RAP_BUSY;
			break;
		case 0x03:
			dev_context->mhl_flags |=
			    MHL_STATE_APPLICATION_RAP_BUSY;
			break;
		default:
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
			break;
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_rap_out(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status =
		    scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			      dev_context->rap_out_sub_command);
	}

	up(&dev_context->isr_lock);

	return status;
}

/*
 * send_rap_out() - Handle write request to the rap attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t send_rap_out(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		if (kstrtoul(buf, 0, &param) != 0)
			param = INT_MAX;
		switch (param) {
		case MHL_RAP_POLL:
		case MHL_RAP_CONTENT_ON:
		case MHL_RAP_CONTENT_OFF:
		case MHL_RAP_CBUS_MODE_DOWN:
		case MHL_RAP_CBUS_MODE_UP:
			if (!si_mhl_tx_rap_send(dev_context, param)) {
				MHL_TX_DBG_ERR("-EPERM\n");
				status = -EPERM;
			} else {
				dev_context->rap_out_sub_command = (u8) param;
			}
			break;
		default:
			dev_context->rap_out_status = MHL_RAPK_UNRECOGNIZED;
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
			break;
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_rap_out_status(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		/* todo: check for un-ack'ed RAP sub command
		 * and block until ack received.
		 */
		status =
		    scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			      dev_context->rap_out_status);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_rap_input_dev(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n", input_dev_rap);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_rap_input_dev(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		if (kstrtoul(buf, 0, &param) != 0)
			param = INT_MAX;
		switch (param) {
		case 0:
		case 1:
			input_dev_rap = (bool)param;
			break;
		default:
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
			break;
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

static struct device_attribute rap_in_attr =
	__ATTR(SYS_ATTR_NAME_RAP_IN, 0444, show_rap_in, NULL);
static struct device_attribute rap_in_status_attr =
	__ATTR(SYS_ATTR_NAME_RAP_IN_STATUS, 0222, NULL, set_rap_in_status);
static struct device_attribute rap_out_attr =
	__ATTR(SYS_ATTR_NAME_RAP_OUT, 0666, show_rap_out, send_rap_out);
static struct device_attribute rap_out_status_attr =
	__ATTR(SYS_ATTR_NAME_RAP_OUT_STATUS, 0444, show_rap_out_status, NULL);
static struct device_attribute rap_input_dev =
	__ATTR(SYS_ATTR_NAME_RAP_INPUT_DEV, 0666, show_rap_input_dev,
	set_rap_input_dev);

static struct attribute *rap_attrs[] = {
	&rap_in_attr.attr,
	&rap_in_status_attr.attr,
	&rap_out_attr.attr,
	&rap_out_status_attr.attr,
	&rap_input_dev.attr,
	NULL
};

static struct attribute_group rap_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_RAP),
	.attrs = rap_attrs
};

ssize_t show_rcp_in(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("could not acquire mutex!!!\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else if (input_dev_rcp) {
		MHL_TX_DBG_INFO("\n");
		status = scnprintf(buf, PAGE_SIZE, "rcp_input_dev\n");
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n",
				   dev_context->rcp_in_key_code);
	}

	up(&dev_context->isr_lock);

	return status;
}

/*
 * send_rcp_in_status() - Handle write request to the rcp attribute file.
 *
 * Writes to this file cause a RCP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t send_rcp_in_status(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		unsigned long err_code;
		if (kstrtoul(buf, 0, &err_code) != 0)
			status = -EINVAL;
		else {
			status = count;
			if (err_code == 0) {
				if (!si_mhl_tx_rcpk_send(
				    dev_context,
				    dev_context->rcp_in_key_code)) {
					status = -ENOMEM;
				}
			} else if (!si_mhl_tx_rcpe_send(dev_context,
				(u8)err_code)) {
				status = -EINVAL;
			}
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_rcp_out(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02X\n",
				   dev_context->rcp_out_key_code);
	}

	up(&dev_context->isr_lock);

	return status;
}

/*
 * send_rcp_out() - Handle write request to the rcp attribute file.
 *
 * Writes to this file cause a RCP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t send_rcp_out(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		if (kstrtoul(buf, 0, &param) != 0)
			status = -EINVAL;
		else {
			dev_context->mhl_flags &=
					~(MHL_STATE_FLAG_RCP_RECEIVED |
					MHL_STATE_FLAG_RCP_ACK |
					MHL_STATE_FLAG_RCP_NAK);
			dev_context->mhl_flags |= MHL_STATE_FLAG_RCP_SENT;
			dev_context->rcp_send_status = 0;
			dev_context->rcp_err_code = 0; /*reset error code*/
			if (!si_mhl_tx_rcp_send(dev_context, (u8) param)) {
				MHL_TX_DBG_ERR("-EPERM\n");
				status = -EPERM;
			} else {
				dev_context->rcp_out_key_code = param;
			}
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_rcp_out_status(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		/* todo: check for un-ack'ed RCP sub command
		 * and block until ack received.
		 */
		if (dev_context->
		    mhl_flags & (MHL_STATE_FLAG_RCP_ACK |
				 MHL_STATE_FLAG_RCP_NAK)) {
			status =
			    scnprintf(buf, PAGE_SIZE, "0x%02X\n",
				      dev_context->rcp_err_code);
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_rcp_input_dev(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n", input_dev_rcp);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_rcp_input_dev(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		if (kstrtoul(buf, 0, &param) != 0)
			param = INT_MAX;
		switch (param) {
		case 0:
		case 1:
			input_dev_rcp = (bool)param;
			break;
		default:
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
			break;
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

static struct device_attribute rcp_in_attr =
	__ATTR(SYS_ATTR_NAME_RCP_IN, 0444, show_rcp_in, NULL);
static struct device_attribute rcp_in_status_attr =
	__ATTR(SYS_ATTR_NAME_RCP_IN_STATUS, 0222, NULL, send_rcp_in_status);
static struct device_attribute rcp_out_attr =
	__ATTR(SYS_ATTR_NAME_RCP_OUT, 0666, show_rcp_out, send_rcp_out);
static struct device_attribute rcp_out_status_attr =
	__ATTR(SYS_ATTR_NAME_RCP_OUT_STATUS, 0444, show_rcp_out_status, NULL);
static struct device_attribute rcp_input_dev =
	__ATTR(SYS_ATTR_NAME_RCP_INPUT_DEV, 0666, show_rcp_input_dev,
	set_rcp_input_dev);

static struct attribute *rcp_attrs[] = {
	&rcp_in_attr.attr,
	&rcp_in_status_attr.attr,
	&rcp_out_attr.attr,
	&rcp_out_status_attr.attr,
	&rcp_input_dev.attr,
	NULL
};

static struct attribute_group rcp_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_RCP),
	.attrs = rcp_attrs
};
#if (INCLUDE_RBP == 1)
ssize_t show_rbp_in(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("could not acquire mutex!!!\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status =
		    scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			      dev_context->rbp_in_button_code);
	}
	up(&dev_context->isr_lock);

	return status;
}

/*
 * send_rbp_in_status() - Handle write request to the rbp attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t send_rbp_in_status(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		unsigned long err_code;
		status = kstrtoul(buf, 0, &err_code);
		if (status == 0) {
			status = count;
			if (err_code == 0) {
				if (!si_mhl_tx_rbpk_send(
					dev_context,
					dev_context->rbp_in_button_code)) {
					status = -ENOMEM;
				}
			} else if (!si_mhl_tx_rbpe_send(
					dev_context, (u8)err_code)) {
				status = -EINVAL;
			}
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

/*
 * send_rbp_out() - Handle write request to the rbp attribute file.
 *
 * Writes to this file cause a RBP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t send_rbp_out(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		if (kstrtoul(buf, 0, &param) != 0)
			status = -EINVAL;
		else {
			dev_context->mhl_flags &=
					~(MHL_STATE_FLAG_RBP_RECEIVED |
					MHL_STATE_FLAG_RBP_ACK |
					MHL_STATE_FLAG_RBP_NAK);
			dev_context->mhl_flags |= MHL_STATE_FLAG_RBP_SENT;
			dev_context->rbp_send_status = 0;
			if (!si_mhl_tx_rbp_send(dev_context, (u8) param)) {
				MHL_TX_DBG_ERR("-EPERM\n");
				status = -EPERM;
			} else {
				dev_context->rbp_out_button_code = param;
			}
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_rbp_out(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02X\n",
				   dev_context->rbp_out_button_code);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_rbp_out_status(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		/* todo: check for un-ack'ed RBP sub command
		 * and block until ack received.
		 */
		if (dev_context->
		    mhl_flags & (MHL_STATE_FLAG_RBP_ACK |
				 MHL_STATE_FLAG_RBP_NAK)) {
			status =
			    scnprintf(buf, PAGE_SIZE, "0x%02X\n",
				      dev_context->rbp_err_code);
		}
	}

	up(&dev_context->isr_lock);

	return status;
}


ssize_t show_rbp_input_dev(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n", input_dev_rbp);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_rbp_input_dev(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		if (kstrtoul(buf, 0, &param) != 0)
			param = INT_MAX;
		switch (param) {
		case 0:
		case 1:
			input_dev_rbp = (bool)param;
			break;
		default:
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
			break;
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

static struct device_attribute rbp_in_attr =
	__ATTR(SYS_ATTR_NAME_RBP_IN, 0444, show_rbp_in, NULL);
static struct device_attribute rbp_in_status_attr =
	__ATTR(SYS_ATTR_NAME_RBP_IN_STATUS, 0222, NULL, send_rbp_in_status);
static struct device_attribute rbp_out_attr =
	__ATTR(SYS_ATTR_NAME_RBP_OUT, 0666, show_rbp_out, send_rbp_out);
static struct device_attribute rbp_out_status_attr =
	__ATTR(SYS_ATTR_NAME_RBP_OUT_STATUS, 0444, show_rbp_out_status, NULL);
static struct device_attribute rbp_input_dev =
	__ATTR(SYS_ATTR_NAME_RBP_INPUT_DEV, 0666, show_rbp_input_dev,
	set_rbp_input_dev);

static struct attribute *rbp_attrs[] = {
	&rbp_in_attr.attr,
	&rbp_in_status_attr.attr,
	&rbp_out_attr.attr,
	&rbp_out_status_attr.attr,
	&rbp_input_dev.attr,
	NULL
};

static struct attribute_group rbp_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_RBP),
	.attrs = rbp_attrs
};
#endif
ssize_t show_ucp_in(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("could not acquire mutex!!!\n");
		return -ERESTARTSYS;
	}
	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status =
		    scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			      dev_context->ucp_in_key_code);
	}
	up(&dev_context->isr_lock);

	return status;
}

/*
 * send_ucp_in_status() - Handle write request to the ucp attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t send_ucp_in_status(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else if (input_dev_ucp) {
		MHL_TX_DBG_ERR("channel busy\n");
	} else {
		unsigned long err_code;
		status = kstrtoul(buf, 0, &err_code);
		if (status == 0) {
			status = count;
			if (err_code == 0) {
				if (!si_mhl_tx_ucpk_send(
					dev_context,
					dev_context->ucp_in_key_code)) {
					status = -ENOMEM;
				}
			} else if (!si_mhl_tx_ucpe_send(
					dev_context, (u8) err_code)) {
				status = -EINVAL;
			}
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_ucp_out(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status =
		    scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			      dev_context->ucp_out_key_code);
	}

	up(&dev_context->isr_lock);

	return status;
}

/*
 * send_ucp_out() - Handle write request to the ucp attribute file.
 *
 * Writes to this file cause a RAP message with the specified action code
 * to be sent to the downstream device.
 */
ssize_t send_ucp_out(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		status = kstrtoul(buf, 0, &param);
		if (status == 0) {
			dev_context->mhl_flags &=
					~(MHL_STATE_FLAG_UCP_RECEIVED |
					MHL_STATE_FLAG_UCP_ACK |
					MHL_STATE_FLAG_UCP_NAK);
			dev_context->mhl_flags |= MHL_STATE_FLAG_UCP_SENT;
			if (!si_mhl_tx_ucp_send(dev_context, (u8)param)) {
				MHL_TX_DBG_ERR("-EPERM\n");
				status = -EPERM;
			} else {
				dev_context->ucp_out_key_code = (u8)param;
				status = count;
			}
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_ucp_out_status(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		/* todo: check for un-ack'ed RAP sub command
		 * and block until ack received.
		 */
		if (dev_context->
		    mhl_flags & (MHL_STATE_FLAG_UCP_ACK |
				 MHL_STATE_FLAG_UCP_NAK)) {
			status =
			    scnprintf(buf, PAGE_SIZE, "0x%02x\n",
				      dev_context->ucp_err_code);
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_ucp_input_dev(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n", input_dev_ucp);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_ucp_input_dev(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	/* Assume success */
	status = count;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		if (kstrtoul(buf, 0, &param) != 0)
			param = INT_MAX;
		switch (param) {
		case 0:
		case 1:
			input_dev_ucp = (bool)param;
			break;
		default:
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
			break;
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

static struct device_attribute ucp_in_attr =
	__ATTR(SYS_ATTR_NAME_UCP_IN, 0444, show_ucp_in, NULL);
static struct device_attribute ucp_in_status_attr =
	__ATTR(SYS_ATTR_NAME_UCP_IN_STATUS, 0222, NULL, send_ucp_in_status);
static struct device_attribute ucp_out_attr =
	__ATTR(SYS_ATTR_NAME_UCP_OUT, 0666, show_ucp_out, send_ucp_out);
static struct device_attribute ucp_out_status_attr =
	__ATTR(SYS_ATTR_NAME_UCP_OUT_STATUS, 0444, show_ucp_out_status, NULL);

static struct device_attribute ucp_input_dev =
	__ATTR(SYS_ATTR_NAME_UCP_INPUT_DEV, 0666, show_ucp_input_dev,
	set_ucp_input_dev);

static struct attribute *ucp_attrs[] = {
	&ucp_in_attr.attr,
	&ucp_in_status_attr.attr,
	&ucp_out_attr.attr,
	&ucp_out_status_attr.attr,
	&ucp_input_dev.attr,
	NULL
};

static struct attribute_group ucp_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_UCP),
	.attrs = ucp_attrs
};

ssize_t show_devcap_local_offset(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status =
		    scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			      dev_context->dev_cap_local_offset);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_devcap_local_offset(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		status = kstrtoul(buf, 0, &param);
		if ((status != 0) || (param >= 16)) {
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
		} else {
			dev_context->dev_cap_local_offset = param;
			status = count;
		}
	}

	up(&dev_context->isr_lock);
	return status;
}

ssize_t show_devcap_local(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			dev_cap_values[dev_context->dev_cap_local_offset]);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_devcap_local(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		status = kstrtoul(buf, 0, &param);
		if ((status != 0) || (param > 0xFF)) {
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
		} else {
			dev_cap_values[dev_context->dev_cap_local_offset] =
			    param;
			status = count;
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_devcap_remote_offset(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status =
		    scnprintf(buf, PAGE_SIZE, "0x%02x\n",
			      dev_context->dev_cap_remote_offset);
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t set_devcap_remote_offset(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		status = kstrtoul(buf, 0, &param);
		if ((status == 0) && (param < 16)) {
			dev_context->dev_cap_remote_offset = (u8)param;
			status = count;
		} else {
			MHL_TX_DBG_ERR("Invalid parameter %s received\n", buf);
			status = -EINVAL;
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_devcap_remote(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		uint8_t regValue;
		status = si_mhl_tx_get_peer_dev_cap_entry(dev_context,
							  dev_context->
							  dev_cap_remote_offset,
							  &regValue);
		if (status != 0) {
			/*
			 * Driver is busy and cannot provide the requested
			 * DEVCAP register value right now so inform caller
			 * they need to try again later.
			 */
			status = -EAGAIN;
		} else {
			status = scnprintf(buf, PAGE_SIZE, "0x%02x", regValue);
		}
	}

	up(&dev_context->isr_lock);

	return status;
}

static struct device_attribute attr_devcap_local_offset =
	__ATTR(SYS_ATTR_NAME_DEVCAP_LOCAL_OFFSET, 0666,
	show_devcap_local_offset, set_devcap_local_offset);
static struct device_attribute attr_devcap_local =
	__ATTR(SYS_ATTR_NAME_DEVCAP_LOCAL, 0666, show_devcap_local,
	set_devcap_local);
static struct device_attribute attr_devcap_remote_offset =
	__ATTR(SYS_ATTR_NAME_DEVCAP_REMOTE_OFFSET, 0666,
	show_devcap_remote_offset, set_devcap_remote_offset);
static struct device_attribute attr_devcap_remote =
	__ATTR(SYS_ATTR_NAME_DEVCAP_REMOTE, 0444, show_devcap_remote, NULL);

static struct attribute *devcap_attrs[] = {
	&attr_devcap_local_offset.attr,
	&attr_devcap_local.attr,
	&attr_devcap_remote_offset.attr,
	&attr_devcap_remote.attr,
	NULL
};

static struct attribute_group devcap_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_DEVCAP),
	.attrs = devcap_attrs
};

ssize_t set_hdcp_force_content_type(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	unsigned long new_hdcp_content_type;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = kstrtoul(buf, 0, &new_hdcp_content_type);
		if (status == 0) {
			status = count;
			hdcp_content_type = new_hdcp_content_type;
		}
	}

	up(&dev_context->isr_lock);
	return status;
}

ssize_t show_hdcp_force_content_type(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "%d", hdcp_content_type);
	}

	up(&dev_context->isr_lock);

	return status;
}

static struct device_attribute attr_hdcp_force_content_type =
	__ATTR(SYS_ATTR_NAME_HDCP_CONTENT_TYPE, 0666,
	show_hdcp_force_content_type, set_hdcp_force_content_type);

static struct attribute *hdcp_attrs[] = {
	&attr_hdcp_force_content_type.attr,
	NULL
};

static struct attribute_group hdcp_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_HDCP),
	.attrs = hdcp_attrs
};

ssize_t set_vc_assign(struct device *dev, struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status;
	struct tdm_alloc_burst tdm_burst;
	uint8_t slots;

	MHL_TX_DBG_INFO("received string: %s\n", buf);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		unsigned long param;
		enum cbus_mode_e cbus_mode;
		status = kstrtoul(buf, 0, &param);
		if (status != 0)
			goto input_err;
		status = count;

		cbus_mode = si_mhl_tx_drv_get_cbus_mode(dev_context);
		switch (cbus_mode) {
		case CM_eCBUS_S:
			if (param > 24)
				goto input_err;
			break;
		case CM_eCBUS_D:
			if (param > 200)
				goto input_err;
			break;
		default:
input_err:
			MHL_TX_DBG_ERR(
				"Invalid number of eMSC slots specified\n");
			status = -EINVAL;
			goto done;
		}

		slots = (u8)param;

		memset(&tdm_burst, 0, sizeof(tdm_burst));
		tdm_burst.header.burst_id.high = burst_id_VC_ASSIGN >> 8;
		tdm_burst.header.burst_id.low = (uint8_t) burst_id_VC_ASSIGN;
		tdm_burst.header.sequence_index = 1;
		tdm_burst.header.total_entries = 2;
		tdm_burst.num_entries_this_burst = 2;
		tdm_burst.vc_info[0].vc_num = TDM_VC_E_MSC;
		tdm_burst.vc_info[0].feature_id = FEATURE_ID_E_MSC;
		tdm_burst.vc_info[0].req_resp.channel_size = slots;
		tdm_burst.vc_info[1].vc_num = TDM_VC_T_CBUS;
		tdm_burst.vc_info[1].feature_id = FEATURE_ID_USB;
		tdm_burst.vc_info[1].req_resp.channel_size = (u8) (24 - slots);
		tdm_burst.vc_info[2].vc_num = 0;
		tdm_burst.vc_info[2].feature_id = 0;
		tdm_burst.vc_info[2].req_resp.channel_size = 0;
		tdm_burst.header.checksum = 0;
		tdm_burst.header.checksum =
		    calculate_generic_checksum((uint8_t *) (&tdm_burst), 0,
					       sizeof(tdm_burst));

		dev_context->prev_virt_chan_slot_counts[TDM_VC_E_MSC] =
		    dev_context->virt_chan_slot_counts[TDM_VC_E_MSC];
		dev_context->prev_virt_chan_slot_counts[TDM_VC_T_CBUS] =
		    dev_context->virt_chan_slot_counts[TDM_VC_T_CBUS];

		dev_context->virt_chan_slot_counts[TDM_VC_E_MSC] = slots;
		dev_context->virt_chan_slot_counts[TDM_VC_T_CBUS] =
		    (u8) (24 - slots);

		si_mhl_tx_request_write_burst(dev_context, 0, sizeof(tdm_burst),
					      (uint8_t *) &tdm_burst);
		status = count;
	}
done:
	up(&dev_context->isr_lock);

	return status;
}

ssize_t show_vc_assign(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct mhl_dev_context *dev_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("-ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("-ENODEV\n");
		status = -ENODEV;
	} else {
		status = scnprintf(buf, PAGE_SIZE, "%d", 0);
	}

	up(&dev_context->isr_lock);

	return status;
}

static struct device_attribute attr_vc_assign =
	__ATTR(SYS_ATTR_NAME_VC_ASSIGN, 0666, show_vc_assign, set_vc_assign);

static struct attribute *vc_attrs[] = {
	&attr_vc_assign.attr,
	NULL
};

static struct attribute_group vc_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_VC),
	.attrs = vc_attrs
};

/*
process_si_adopter_id
*/

void process_si_adopter_id(struct mhl_dev_context *dev_context,
			struct si_adopter_id_data *p_burst)
{
	uint8_t opcode = p_burst->hdr.op_code;
	uint8_t checksum;
	size_t total_size;
	total_size = SII_OFFSETOF(struct si_adopter_id_data, hdr.checksum)
			+  p_burst->hdr.remaining_length;
	checksum = calculate_generic_checksum(p_burst, 0, total_size);
	MHL_TX_DBG_INFO("total_size: 0x%02X"
		" id: %02X%02X"
		" rem_len: 0x%02X"
		" cksum: 0x%02X"
		" opcode: 0x%02X\n",
		total_size,
		p_burst->hdr.burst_id.high,
		p_burst->hdr.burst_id.low,
		p_burst->hdr.remaining_length,
		p_burst->hdr.checksum,
		p_burst->hdr.op_code)
	if (checksum) {
		MHL_TX_DBG_ERR("%schecksum error 0x%02X %s\n",
			ANSI_ESC_RED_TEXT,
			checksum,
			ANSI_ESC_RESET_TEXT)
	} else
	switch (opcode) {
	case EDID_BLOCK: {
		struct edid_3d_data_t *edid_context;
		edid_context = dev_context->edid_parser_context;
		MHL_TX_DBG_INFO("EDID_BLOCK "
			"blk_num: %02X\n",
			p_burst->opcode_data.edid_blk.block_num)
			process_emsc_edid_sub_payload(edid_context, p_burst);
		}
		break;
	case EDID_STOP:
		MHL_TX_DBG_ERR("EDID_STOP: %d\n", total_size)
		break;
	}

}

/*
 * Called from the Titan interrupt handler to parse data
 * received from the eSMC BLOCK hardware. Process all waiting input
 * buffers. If multiple messages are sent in the same BLOCK Command buffer,
 * they are sorted out here also.
 */
static void si_mhl_tx_emsc_received(struct mhl_dev_context *context)
{
	struct drv_hw_context *hw_context =
		(struct drv_hw_context *)(&context->drv_context);
	int length, index, pass;
	uint16_t burst_id;
	uint8_t *prbuf, *pmsg;

	while (si_mhl_tx_drv_peek_block_input_buffer(context,
		&prbuf, &length) == 0) {
		index = 0;
		pass = 0;
/*		dump_array(DBG_MSG_LEVEL_INFO, "si_mhl_tx_emsc_received",
			prbuf, length); */
		do {
			struct MHL_burst_id_t *p_burst_id;
			pmsg = &prbuf[index];
			p_burst_id = (struct MHL_burst_id_t *)pmsg;
			burst_id = (((uint16_t)pmsg[0]) << 8) | pmsg[1];
			/* Move past the BURST ID */
			index += sizeof(*p_burst_id);

			switch (burst_id) {
			case burst_id_HID_PAYLOAD:
#if (INCLUDE_HID == 1)
				build_received_hid_message(context,
					&pmsg[3], pmsg[2]);
#endif
				index += (1 + pmsg[2]);
				break;

			case burst_id_BLK_RCV_BUFFER_INFO:
				hw_context->block_protocol.peer_blk_rx_buf_max
					= pmsg[3];
				hw_context->block_protocol.peer_blk_rx_buf_max
					<<= 8;
				hw_context->block_protocol.peer_blk_rx_buf_max
					|= pmsg[2];

				hw_context->block_protocol.
					peer_blk_rx_buf_avail =
					hw_context->block_protocol.
					peer_blk_rx_buf_max -
					(EMSC_RCV_BUFFER_DEFAULT -
						hw_context->block_protocol.
						peer_blk_rx_buf_avail);
				MHL_TX_DBG_INFO(
					"Total PEER Buffer Size: %d\n",
					hw_context->block_protocol.
					peer_blk_rx_buf_max);

				index += 2;
				break;

			case burst_id_BITS_PER_PIXEL_FMT:
				index += (4 + (2 * pmsg[5]));
				/*
				 * Inform the rest of the code of the
				 * acknowledgment
				 */
				break;
				/*
				 * Any BURST_ID reported by EMSC_SUPPORT can be
				 * sent using eMSC BLOCK
				 */
			case burst_id_HEV_DTDA:{
				struct MHL3_hev_dtd_a_data_t *p_dtda =
					(struct MHL3_hev_dtd_a_data_t *)
					p_burst_id;
				index += sizeof(*p_dtda) - sizeof(*p_burst_id);
				}
				break;
			case burst_id_HEV_DTDB:{
				struct MHL3_hev_dtd_b_data_t *p_dtdb =
				    (struct MHL3_hev_dtd_b_data_t *) p_burst_id;
				index +=
				    sizeof(*p_dtdb) -
				    sizeof(*p_burst_id);
				}
				/* for now, as a robustness excercise, adjust
				 * index according to the this burst field
				 */
				break;
			case burst_id_HEV_VIC:{
				struct MHL3_hev_vic_data_t *p_burst;
				p_burst = (struct MHL3_hev_vic_data_t *)
					p_burst_id;
				index += sizeof(*p_burst) -
					sizeof(*p_burst_id) +
					sizeof(p_burst->video_descriptors[0]) *
					p_burst->num_entries_this_burst;
				}
				break;
			case burst_id_3D_VIC:{
				struct MHL3_hev_vic_data_t *p_burst =
					(struct MHL3_hev_vic_data_t *)
					p_burst_id;
				index += sizeof(*p_burst) -
					sizeof(*p_burst_id) -
					sizeof(p_burst->video_descriptors) +
					sizeof(p_burst->video_descriptors[0]) *
					p_burst->num_entries_this_burst;
				}
				break;
			case burst_id_3D_DTD:{
				struct MHL2_video_format_data_t *p_burst =
					(struct MHL2_video_format_data_t *)
					p_burst_id;
				index += sizeof(*p_burst) -
					sizeof(*p_burst_id) -
					sizeof(p_burst->video_descriptors) +
					sizeof(p_burst->video_descriptors[0]) *
					p_burst->num_entries_this_burst;
				}
				break;
			case burst_id_VC_ASSIGN:{
				struct SI_PACK_THIS_STRUCT tdm_alloc_burst
					*p_burst;
				p_burst = (struct SI_PACK_THIS_STRUCT
					tdm_alloc_burst*)p_burst_id;
				index += sizeof(*p_burst) -
					sizeof(*p_burst_id) -
					sizeof(p_burst->reserved) -
					sizeof(p_burst->vc_info) +
					sizeof(p_burst->vc_info[0]) *
					p_burst->num_entries_this_burst;
				}
				break;
			case burst_id_VC_CONFIRM:{
				struct SI_PACK_THIS_STRUCT tdm_alloc_burst
					*p_burst;
				p_burst = (struct SI_PACK_THIS_STRUCT
					tdm_alloc_burst*)p_burst_id;
				index += sizeof(*p_burst) -
					sizeof(*p_burst_id) -
					sizeof(p_burst->reserved) -
					sizeof(p_burst->vc_info) +
					sizeof(p_burst->vc_info[0]) *
					p_burst->num_entries_this_burst;
				}
				break;
			case burst_id_AUD_DELAY:{
				struct MHL3_audio_delay_burst_t *p_burst =
					(struct MHL3_audio_delay_burst_t *)
					p_burst_id;
				index += sizeof(*p_burst) -
					sizeof(*p_burst_id) -
					sizeof(p_burst->reserved);
				}
				break;
			case burst_id_ADT_BURSTID:{
				struct MHL3_adt_data_t *p_burst =
					(struct MHL3_adt_data_t *)p_burst_id;
				index += sizeof(*p_burst) - sizeof(*p_burst_id);
				}
				break;
			case burst_id_BIST_SETUP:{
				struct SI_PACK_THIS_STRUCT
					bist_setup_burst * p_bist_setup;
				p_bist_setup = (struct SI_PACK_THIS_STRUCT
					bist_setup_burst*)p_burst_id;
				index += sizeof(*p_bist_setup) -
					sizeof(*p_burst_id);
				}
				break;
			case burst_id_BIST_RETURN_STAT:{
				struct SI_PACK_THIS_STRUCT
					bist_return_stat_burst
					*p_bist_return;
				p_bist_return = (struct SI_PACK_THIS_STRUCT
					bist_return_stat_burst*)p_burst_id;
				index += sizeof(*p_bist_return) -
					sizeof(*p_burst_id);
				}
				break;
			case burst_id_EMSC_SUPPORT:{
				/*
				 * For robustness, adjust index
				 * according to the num-entries this
				 * burst field
				 */
				struct MHL3_emsc_support_data_t *p_burst =
					(struct MHL3_emsc_support_data_t *)
					p_burst_id;
				int i;
				index += sizeof(*p_burst) -
					sizeof(*p_burst_id) -
					sizeof(p_burst->payload) +
					sizeof(p_burst->payload.burst_ids[0]) *
					p_burst->num_entries_this_burst;
				for (i = 0;
					i < p_burst->num_entries_this_burst;
					++i) {
					enum BurstId_e burst_id =
					BURST_ID(p_burst->payload.burst_ids[i]);
					MHL_TX_DBG_INFO("BURST_ID: %04X\n",
						burst_id)
					switch (burst_id) {
					case SILICON_IMAGE_ADOPTER_ID:
						context->sii_adopter_id = true;
						break;
					case burst_id_HID_PAYLOAD:
						break;
					default:
						MHL_TX_DBG_ERR("%sunexpected"
							" burst/adopter id:%04X%s\n",
							ANSI_ESC_RED_TEXT,
							burst_id,
							ANSI_ESC_RESET_TEXT)
					}
				}
				}
				break;
			case SILICON_IMAGE_ADOPTER_ID:
				{
				struct si_adopter_id_data *p_burst =
					(struct si_adopter_id_data *)
						p_burst_id;

				index +=
				sizeof(p_burst->hdr.remaining_length)
					+ p_burst->hdr.remaining_length;
				process_si_adopter_id(context, p_burst);

				}
				break;
			default:
				if ((MHL_TEST_ADOPTER_ID == burst_id) ||
					(burst_id >= adopter_id_RANGE_START)) {
					struct EMSC_BLK_ADOPT_ID_PAYLD_HDR*
					p_adopter_id = (struct
						EMSC_BLK_ADOPT_ID_PAYLD_HDR*)
						pmsg;
					MHL_TX_DBG_ERR(
						"Bad ADOPTER_ID: %04X\n",
						burst_id);
					index += p_adopter_id->remaining_length;
				} else {
					MHL_TX_DBG_ERR(
						"Bad BURST ID: %04X\n",
						burst_id);
					index = length;
				}
				break;
			}

			length -= index;
		} while (length > 0);
		si_mhl_tx_drv_free_block_input_buffer(context);
	}
}

static void check_drv_intr_flags(struct mhl_dev_context *dev_context)
{
	enum cbus_mode_e cbus_mode;
	cbus_mode = si_mhl_tx_drv_get_cbus_mode(dev_context);
	if (dev_context->intr_info.flags & DRV_INTR_CBUS_ABORT)
		process_cbus_abort(dev_context);

	if (dev_context->intr_info.flags & DRV_INTR_WRITE_BURST)
		si_mhl_tx_process_write_burst_data(dev_context);

	/* sometimes SET_INT and WRITE_STAT come at the
	 * same time. Always process WRITE_STAT first.
	 */
	if (dev_context->intr_info.flags & DRV_INTR_WRITE_STAT)
		si_mhl_tx_got_mhl_status(dev_context,
			&dev_context->intr_info.dev_status);

	if (dev_context->intr_info.flags & DRV_INTR_SET_INT)
		si_mhl_tx_got_mhl_intr(dev_context,
			dev_context->intr_info.int_msg[0],
			dev_context->intr_info.int_msg[1]);

	if (dev_context->intr_info.flags & DRV_INTR_MSC_DONE)
		si_mhl_tx_msc_command_done(dev_context,
			dev_context->intr_info.msc_done_data);

	if (dev_context->intr_info.flags & DRV_INTR_EMSC_INCOMING)
		si_mhl_tx_emsc_received(dev_context);

	if (dev_context->intr_info.flags & DRV_INTR_HPD_CHANGE)
		si_mhl_tx_notify_downstream_hpd_change
		    (dev_context, dev_context->intr_info.hpd_status);

	if (dev_context->intr_info.flags & DRV_INTR_MSC_RECVD) {
		dev_context->msc_msg_arrived = true;
		dev_context->msc_msg_sub_command =
		    dev_context->intr_info.msc_msg[0];
		dev_context->msc_msg_data =
		    dev_context->intr_info.msc_msg[1];
		si_mhl_tx_process_events(dev_context);
	}

	if (DRV_INTR_COC_CAL & dev_context->intr_info.flags) {
		bool do_test_now = false;
		switch (cbus_mode) {
		case CM_TRANSITIONAL_TO_eCBUS_S_CAL_BIST:
			if (0 == dev_context->bist_setup.avlink_duration)  {
				MHL_TX_DBG_ERR("infinite av_link duration\n")
				do_test_now = false;
			} else if (BIST_TRIGGER_ECBUS_AV_LINK_MASK &
				dev_context->bist_trigger_info) {
				MHL_TX_DBG_ERR("%sAV_LINK BIST start%s\n",
					ANSI_ESC_GREEN_TEXT,
					ANSI_ESC_RESET_TEXT)
				do_test_now = true;
			}

			if (BIST_TRIGGER_ECBUS_TX_RX_MASK &
				dev_context->bist_trigger_info) {
				MHL_TX_DBG_ERR("%seCBUS BIST start%s\n",
					ANSI_ESC_GREEN_TEXT,
					ANSI_ESC_RESET_TEXT)
				do_test_now = true;
			}
			if (!do_test_now)
				;
			else if (dev_context->misc_flags.flags.bist_role_TE)
				start_bist_initiator_test(dev_context);
			else
				initiate_bist_test(dev_context);
			break;
		default:
			;
		}
	}
	if (DRV_INTR_TDM_SYNC & dev_context->intr_info.flags) {
		switch (cbus_mode) {
		case CM_eCBUS_S_BIST:
		case CM_eCBUS_D_BIST:
			break;
		case CM_eCBUS_S_AV_BIST:
		case CM_eCBUS_D_AV_BIST:
			if (dev_context->bist_setup.avlink_duration)  {
				MHL_TX_DBG_ERR("finite av_link duration\n")
			} else if (BIST_TRIGGER_ECBUS_AV_LINK_MASK &
					dev_context->bist_trigger_info) {
				if (dev_context->
					misc_flags.flags.bist_role_TE) {
					MHL_TX_DBG_ERR("\n")
					start_bist_initiator_test(dev_context);
				} else {
					MHL_TX_DBG_ERR("\n")
					initiate_bist_test(dev_context);
				}
			} else {
				MHL_TX_DBG_ERR("infinite av_link duration\n")
			}
			break;
		case CM_eCBUS_S:
		case CM_eCBUS_D:
			/*
			   MHL3 spec requires the process of reading
			   the remainder of XDEVCAP and all of DEVCAP
			   to be deferred until after the first
			   transition to eCBUS mode.
			   Check DCAP_RDY, initiate XDEVCAP read here.
			 */
			if (MHL_STATUS_DCAP_RDY & dev_context->status_0)
				si_mhl_tx_ecbus_started(dev_context);
			if (dev_context->
				misc_flags.flags.have_complete_devcap) {
				if (dev_context->misc_flags.flags.mhl_hpd &&
				   !dev_context->edid_valid) {
					si_mhl_tx_initiate_edid_sequence(
					    dev_context->edid_parser_context);
				}
			}
			break;
		default:
			MHL_TX_DBG_ERR("%sunexpected CBUS mode%s:%s\n",
				ANSI_ESC_RED_TEXT,
				si_mhl_tx_drv_get_cbus_mode_str(cbus_mode),
				ANSI_ESC_RESET_TEXT)

		}
	}
}
/*
 * Interrupt handler for MHL transmitter interrupts.
 *
 * @irq:	The number of the asserted IRQ line that caused
 *		this handler to be called.
 * @data:	Data pointer passed when the interrupt was enabled,
 *		which in this case is a pointer to an mhl_dev_context struct.
 *
 * Always returns IRQ_HANDLED.
 */
static irqreturn_t mhl_irq_handler(int irq, void *data)
{
	struct mhl_dev_context *dev_context = (struct mhl_dev_context *)data;

	MHL_TX_DBG_INFO("called\n");

	if (!down_interruptible(&dev_context->isr_lock)) {
		int loop_limit = 5;
		if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN)
			goto irq_done;
		if (dev_context->dev_flags & DEV_FLAG_COMM_MODE)
			goto irq_done;

		do {
			memset(&dev_context->intr_info, 0,
				sizeof(*(&dev_context->intr_info)));

			dev_context->intr_info.edid_parser_context =
				dev_context->edid_parser_context;

			dev_context->drv_info->
				mhl_device_isr((struct drv_hw_context *)
				(&dev_context->drv_context),
				&dev_context->intr_info);

			/* post process events raised by interrupt handler */
			if (dev_context->intr_info.flags &
				DRV_INTR_DISCONNECT) {
				dev_context->misc_flags.flags.rap_content_on =
					false;
				dev_context->misc_flags.flags.mhl_rsen = false;
				dev_context->mhl_connection_event = true;
				dev_context->mhl_connected =
					MHL_TX_EVENT_DISCONNECTION;
				si_mhl_tx_process_events(dev_context);
			} else {
				if (dev_context->intr_info.flags &
					DRV_INTR_CONNECT) {
					dev_context->misc_flags.flags.
						rap_content_on = true;
					dev_context->rap_in_sub_command =
						MHL_RAP_CONTENT_ON;
					dev_context->misc_flags.flags.mhl_rsen =
						true;
					dev_context->mhl_connection_event =
						true;
					dev_context->mhl_connected =
						MHL_TX_EVENT_CONNECTION;
					si_mhl_tx_process_events(dev_context);
				}


				check_drv_intr_flags(dev_context);
			}
			/*
			 * Send any messages that may have been queued up
			 * as the result of interrupt processing.
			 */
			si_mhl_tx_drive_states(dev_context);
			if (si_mhl_tx_drv_get_cbus_mode(dev_context) >=
			    CM_eCBUS_S) {
				si_mhl_tx_push_block_transactions(dev_context);
			}
		} while ((--loop_limit > 0) && is_interrupt_asserted());
irq_done:
		up(&dev_context->isr_lock);
	}

	return IRQ_HANDLED;
}

/* APIs provided by the MHL layer to the lower level driver */

int mhl_handle_power_change_request(struct device *parent_dev, bool power_up)
{
	struct mhl_dev_context *dev_context;
	int status;

	dev_context = dev_get_drvdata(parent_dev);

	MHL_TX_DBG_INFO("\n");

	/* Power down the MHL transmitter hardware. */
	status = down_interruptible(&dev_context->isr_lock);
	if (status) {
		MHL_TX_DBG_ERR("failed to acquire ISR semaphore,"
			       "status: %d\n", status);
		goto done;
	}

	if (power_up)
		status = si_mhl_tx_initialize(dev_context);
	else
		status = si_mhl_tx_shutdown(dev_context);

	up(&dev_context->isr_lock);
done:
	return status;

}

int mhl_tx_init(struct mhl_drv_info const *drv_info, struct device *parent_dev)
{
	struct mhl_dev_context *dev_context;
	int status = 0;
	int ret;

	if (drv_info == NULL || parent_dev == NULL) {
		pr_err("Null parameter passed to %s\n", __func__);
		return -EINVAL;
	}

	if (drv_info->mhl_device_isr == NULL || drv_info->irq == 0) {
		dev_err(parent_dev, "No IRQ specified!\n");
		return -EINVAL;
	}

	dev_context = kzalloc(sizeof(*dev_context) + drv_info->drv_context_size,
			      GFP_KERNEL);
	if (!dev_context) {
		dev_err(parent_dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	dev_context->signature = MHL_DEV_CONTEXT_SIGNATURE;
	dev_context->drv_info = drv_info;

	sema_init(&dev_context->isr_lock, 1);
	INIT_LIST_HEAD(&dev_context->timer_list);
	dev_context->timer_work_queue = create_workqueue(MHL_DRIVER_NAME);
	if (dev_context->timer_work_queue == NULL) {
		ret = -ENOMEM;
		goto free_mem;
	}

#if (INCLUDE_HID == 1)
	/*
	 * Create a single-threaded work queue so that no two queued
	 * items can run at the same time.
	 */
	dev_context->hid_work_queue = create_singlethread_workqueue(
		"MHL3_HID_WORK");
	if (dev_context->hid_work_queue == NULL) {
		ret = -ENOMEM;
		goto timer_cleanup;
	}
#endif

	if (mhl_class == NULL) {
		mhl_class = class_create(THIS_MODULE, "mhl");
		if (IS_ERR(mhl_class)) {
			ret = PTR_ERR(mhl_class);
			pr_info("class_create failed %d\n", ret);
			goto hid_cleanup;
		}

#if (LINUX_KERNEL_VER < 315)
		mhl_class->dev_attrs = driver_attribs;
#endif

		ret = alloc_chrdev_region(&dev_num,
			0, MHL_DRIVER_MINOR_MAX, MHL_DRIVER_NAME);

		if (ret) {
			pr_info("register_chrdev %s failed, error code: %d\n",
				MHL_DRIVER_NAME, ret);
			goto free_class;
		}

		cdev_init(&dev_context->mhl_cdev, &mhl_fops);
		dev_context->mhl_cdev.owner = THIS_MODULE;
		ret =
		    cdev_add(&dev_context->mhl_cdev, MINOR(dev_num),
			     MHL_DRIVER_MINOR_MAX);
		if (ret) {
			pr_info("cdev_add %s failed %d\n", MHL_DRIVER_NAME,
				ret);
			goto free_chrdev;
		}
	}

	dev_context->mhl_dev = device_create(mhl_class, parent_dev,
		dev_num, dev_context, "%s", MHL_DEVICE_NAME);
	if (IS_ERR(dev_context->mhl_dev)) {
		ret = PTR_ERR(dev_context->mhl_dev);
		pr_info("device_create failed %s %d\n", MHL_DEVICE_NAME, ret);
		goto free_cdev;
	}
	status = sysfs_create_group(&dev_context->mhl_dev->kobj,
		&reg_access_attribute_group);

	if (status)
		MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n", status);
	status = sysfs_create_group(&dev_context->mhl_dev->kobj,
				       &bist_attribute_group);
	if (status) {
		MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n",
			       status);
	}
	status = sysfs_create_group(&dev_context->mhl_dev->kobj,
		&rap_attribute_group);
	if (status)
		MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n", status);
#if (INCLUDE_RBP == 1)
	status = sysfs_create_group(&dev_context->mhl_dev->kobj,
		&rbp_attribute_group);
	if (status)
		MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n", status);
#endif
	status = sysfs_create_group(&dev_context->mhl_dev->kobj,
		&rcp_attribute_group);
	if (status)
		MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n", status);
	status = sysfs_create_group(&dev_context->mhl_dev->kobj,
		&ucp_attribute_group);
	if (status)
		MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n", status);
	status = sysfs_create_group(&dev_context->mhl_dev->kobj,
		&devcap_attribute_group);
	if (status)
		MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n", status);
	status = sysfs_create_group(&dev_context->mhl_dev->kobj,
		&hdcp_attribute_group);
	if (status)
		MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n", status);
	status = sysfs_create_group(&dev_context->mhl_dev->kobj,
		&vc_attribute_group);
	if (status)
		MHL_TX_DBG_ERR("sysfs_create_group failed:%d\n", status);

	ret = request_threaded_irq(drv_info->irq, NULL,
		mhl_irq_handler, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
		MHL_DEVICE_NAME, dev_context);
	if (ret < 0) {
		dev_err(parent_dev,
			"request_threaded_irq failed, status: %d\n", ret);
		goto free_device;
	}

	/* Initialize the MHL transmitter hardware. */
	ret = down_interruptible(&dev_context->isr_lock);
	if (ret) {
		dev_err(parent_dev,
			"Failed to acquire ISR semaphore, status: %d\n", ret);
		goto free_irq_handler;
	}

	/* Initialize EDID parser module */
	dev_context->edid_parser_context = si_edid_create_context(dev_context,
		(struct drv_hw_context *)&dev_context->drv_context);
	rcp_input_dev_one_time_init(dev_context);

	ret = si_mhl_tx_reserve_resources(dev_context);
	if (ret)
		dev_err(parent_dev,
			"failed to reserve resources\n");
	else
		ret = si_mhl_tx_initialize(dev_context);

	up(&dev_context->isr_lock);

	if (ret)
		goto free_edid_context;

#if (INCLUDE_SII6031 == 1)
	mhl_tx_register_mhl_discovery(dev_context);
#endif
	MHL_TX_DBG_INFO("MHL transmitter successfully initialized\n");
	dev_set_drvdata(parent_dev, dev_context);

	return ret;

free_edid_context:
	MHL_TX_DBG_INFO("%sMHL transmitter initialization failed%s\n",
		ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
	status = down_interruptible(&dev_context->isr_lock);
	if (status) {
		dev_err(parent_dev,
			"Failed to acquire ISR semaphore, "
			"could not destroy EDID context. Status: %d\n",
			status);
		goto free_irq_handler;
	}
	if (dev_context->edid_parser_context)
		si_edid_destroy_context(dev_context->edid_parser_context);
	up(&dev_context->isr_lock);

free_irq_handler:
	if (dev_context->client)
		free_irq(dev_context->client->irq, dev_context);

free_device:
	device_destroy(mhl_class, dev_num);

free_cdev:
	cdev_del(&dev_context->mhl_cdev);

free_chrdev:
	unregister_chrdev_region(dev_num, MHL_DRIVER_MINOR_MAX);
	dev_num = 0;

free_class:
	class_destroy(mhl_class);

hid_cleanup:
#if (INCLUDE_HID == 1)
	destroy_workqueue(dev_context->hid_work_queue);
timer_cleanup:
#endif
	destroy_workqueue(dev_context->timer_work_queue);

free_mem:
	kfree(dev_context);

	return ret;
}

int mhl_tx_remove(struct device *parent_dev)
{
	struct mhl_dev_context *dev_context;
	int ret = 0;

	dev_context = dev_get_drvdata(parent_dev);

	if (dev_context != NULL) {
		MHL_TX_DBG_INFO("%x\n", dev_context);

		dev_context->dev_flags |= DEV_FLAG_SHUTDOWN;
		ret = down_interruptible(&dev_context->isr_lock);
		free_irq(dev_context->drv_info->irq, dev_context);
		up(&dev_context->isr_lock);

#if (INCLUDE_HID == 1)
		mhl3_hid_remove_all(dev_context);
		destroy_workqueue(dev_context->hid_work_queue);
		dev_context->hid_work_queue = NULL;
#endif
		ret = si_mhl_tx_shutdown(dev_context);

		mhl_tx_destroy_timer_support(dev_context);

		sysfs_remove_group(&dev_context->mhl_dev->kobj,
			&reg_access_attribute_group);
		sysfs_remove_group(&dev_context->mhl_dev->kobj,
			&rap_attribute_group);
#if (INCLUDE_RBP == 1)
		sysfs_remove_group(&dev_context->mhl_dev->kobj,
			&rbp_attribute_group);
#endif
		sysfs_remove_group(&dev_context->mhl_dev->kobj,
			&rcp_attribute_group);
		sysfs_remove_group(&dev_context->mhl_dev->kobj,
			&ucp_attribute_group);
		sysfs_remove_group(&dev_context->mhl_dev->kobj,
			&devcap_attribute_group);
		sysfs_remove_group(&dev_context->mhl_dev->kobj,
			&hdcp_attribute_group);
		sysfs_remove_group(&dev_context->mhl_dev->kobj,
			&vc_attribute_group);
		sysfs_remove_group(&dev_context->mhl_dev->kobj,
			&bist_attribute_group);

		device_destroy(mhl_class, dev_num);
		cdev_del(&dev_context->mhl_cdev);
		unregister_chrdev_region(dev_num, MHL_DRIVER_MINOR_MAX);
		dev_num = 0;
		class_destroy(mhl_class);
		mhl_class = NULL;

#ifdef MEDIA_DATA_TUNNEL_SUPPORT
		mdt_destroy(dev_context);
#endif
		destroy_rcp_input_dev(dev_context);
		si_edid_destroy_context(dev_context->edid_parser_context);
#if (INCLUDE_SII6031 == 1)
		otg_unregister_mhl_discovery();
#endif
		kfree(dev_context);
	}
	return ret;
}
void mhl_tx_stop_all_timers(struct mhl_dev_context *dev_context)
{
	struct timer_obj *timer = NULL;

	list_for_each_entry(timer, &dev_context->timer_list, list_link) {
		mhl_tx_stop_timer(dev_context, timer);
	}
}
static void mhl_tx_destroy_timer_support(struct mhl_dev_context *dev_context)
{
	struct timer_obj *mhl_timer;

	/*
	 * Make sure all outstanding timer objects are canceled and the
	 * memory allocated for them is freed.
	 */
	while (!list_empty(&dev_context->timer_list)) {
		mhl_timer = list_first_entry(&dev_context->timer_list,
					     struct timer_obj, list_link);
		hrtimer_cancel(&mhl_timer->hr_timer);
		list_del(&mhl_timer->list_link);
		kfree(mhl_timer);
	}

	destroy_workqueue(dev_context->timer_work_queue);
	dev_context->timer_work_queue = NULL;
}

static void mhl_tx_timer_work_handler(struct work_struct *work)
{
	struct timer_obj *mhl_timer;

	mhl_timer = container_of(work, struct timer_obj, work_item);

	mhl_timer->flags |= TIMER_OBJ_FLAG_WORK_IP;
	if (!down_interruptible(&mhl_timer->dev_context->isr_lock)) {

		mhl_timer->timer_callback_handler(mhl_timer->callback_param);

		up(&mhl_timer->dev_context->isr_lock);
	}
	mhl_timer->flags &= ~TIMER_OBJ_FLAG_WORK_IP;

	if (mhl_timer->flags & TIMER_OBJ_FLAG_DEL_REQ) {
		/*
		 * Deletion of this timer was requested during the execution of
		 * the callback handler so go ahead and delete it now.
		 */
		kfree(mhl_timer);
	}
}

static enum hrtimer_restart mhl_tx_timer_handler(struct hrtimer *timer)
{
	struct timer_obj *mhl_timer;

	mhl_timer = container_of(timer, struct timer_obj, hr_timer);

	queue_work(mhl_timer->dev_context->timer_work_queue,
		   &mhl_timer->work_item);

	return HRTIMER_NORESTART;
}

static int is_timer_handle_valid(struct mhl_dev_context *dev_context,
				 void *timer_handle)
{
	struct timer_obj *timer = timer_handle; /* Set to avoid lint warning. */

	list_for_each_entry(timer, &dev_context->timer_list, list_link) {
		if (timer == timer_handle)
			break;
	}

	if (timer != timer_handle) {
		MHL_TX_DBG_WARN("Invalid timer handle %p received\n",
				timer_handle);
		return -EINVAL;
	}
	return 0;
}

int mhl_tx_create_timer(void *context,
			void (*callback_handler) (void *callback_param),
			void *callback_param, void **timer_handle)
{
	struct mhl_dev_context *dev_context;
	struct timer_obj *new_timer;

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

int mhl_tx_delete_timer(void *context, void **timer_handle)
{
	struct mhl_dev_context *dev_context;
	struct timer_obj *timer;
	int status;

	dev_context = get_mhl_device_context(context);

	status = is_timer_handle_valid(dev_context, *timer_handle);
	if (status == 0) {
		timer = *timer_handle;

		list_del(&timer->list_link);

		hrtimer_cancel(&timer->hr_timer);

		if (timer->flags & TIMER_OBJ_FLAG_WORK_IP) {
			/*
			 * Request to delete timer object came from within the
			 * timer's callback handler. If we were to proceed with
			 * the timer deletion we would deadlock at
			 * cancel_work_sync(). Instead, just flag that the user
			 * wants the timer deleted. Later when the timer
			 * callback completes the timer's work handler will
			 * complete the process of deleting this timer.
			 */
			timer->flags |= TIMER_OBJ_FLAG_DEL_REQ;
		} else {
			cancel_work_sync(&timer->work_item);
			*timer_handle = NULL;
			kfree(timer);
		}
	}

	return status;
}

int mhl_tx_start_timer(void *context, void *timer_handle, uint32_t time_msec)
{
	struct mhl_dev_context *dev_context;
	struct timer_obj *timer;
	ktime_t timer_period;
	int status;

	dev_context = get_mhl_device_context(context);

	status = is_timer_handle_valid(dev_context, timer_handle);
	if (status == 0) {
		long secs = 0;
		timer = timer_handle;

		secs = time_msec / 1000;
		time_msec %= 1000;
		timer_period = ktime_set(secs, MSEC_TO_NSEC(time_msec));
		hrtimer_start(&timer->hr_timer, timer_period, HRTIMER_MODE_REL);
	}

	return status;
}

int mhl_tx_stop_timer(void *context, void *timer_handle)
{
	struct mhl_dev_context *dev_context;
	struct timer_obj *timer;
	int status;

	dev_context = get_mhl_device_context(context);

	status = is_timer_handle_valid(dev_context, timer_handle);
	if (status == 0) {
		timer = timer_handle;
		hrtimer_cancel(&timer->hr_timer);
	}
	return status;
}
