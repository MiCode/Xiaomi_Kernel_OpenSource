/*
 * HID over I2C protocol implementation
 *
 * Copyright (c) 2012 Benjamin Tissoires <benjamin.tissoires@gmail.com>
 * Copyright (c) 2012 Ecole Nationale de l'Aviation Civile, France
 * Copyright (c) 2012 Red Hat, Inc
 * Copyright (c) 2013, Movea SA, Jean-Baptiste Maneyrol <jbmaneyrol@movea.com>
 *
 * This code is partly based on "USB HID support for Linux":
 *
 *  Copyright (c) 1999 Andreas Gal
 *  Copyright (c) 2000-2005 Vojtech Pavlik <vojtech@suse.cz>
 *  Copyright (c) 2005 Michael Haboustak <mike-@cinci.rr.com> for Concept2, Inc
 *  Copyright (c) 2007-2008 Oliver Neukum
 *  Copyright (c) 2006-2010 Jiri Kosina
 *  Copyright (C) 2015 XiaoMi, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/hid.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>

#include <linux/i2c/i2c-hid.h>

/* flags */
#define I2C_HID_STARTED		(1 << 0)
#define I2C_HID_RESET_PENDING	(1 << 1)
#define I2C_HID_RESUME		(1 << 2)

#define I2C_HID_PWR_ON		0x00
#define I2C_HID_PWR_SLEEP	0x01

/* debug option */
static bool debug;
module_param(debug, bool, 0444);
MODULE_PARM_DESC(debug, "print a lot of debug information");

#define i2c_hid_dbg(ihid, fmt, arg...)					  \
do {									  \
	if (debug)							  \
		dev_printk(KERN_DEBUG, &(ihid)->client->dev, fmt, ##arg); \
} while (0)

struct i2c_hid_desc {
	__le16 wHIDDescLength;
	__le16 bcdVersion;
	__le16 wReportDescLength;
	__le16 wReportDescRegister;
	__le16 wInputRegister;
	__le16 wMaxInputLength;
	__le16 wOutputRegister;
	__le16 wMaxOutputLength;
	__le16 wCommandRegister;
	__le16 wDataRegister;
	__le16 wVendorID;
	__le16 wProductID;
	__le16 wVersionID;
	__le32 reserved;
} __packed;

struct i2c_hid_cmd {
	unsigned int registerIndex;
	__u8 opcode;
	unsigned int length;
	bool wait;
};

union command {
	u8 data[0];
	struct cmd {
		__le16 reg;
		__u8 reportTypeID;
		__u8 opcode;
	} __packed c;
};

#define I2C_HID_CMD(opcode_) \
	.opcode = opcode_, .length = 4, \
	.registerIndex = offsetof(struct i2c_hid_desc, wCommandRegister)

/* fetch HID descriptor */
static const struct i2c_hid_cmd hid_descr_cmd = { .length = 2 };
/* fetch report descriptors */
static const struct i2c_hid_cmd hid_report_descr_cmd = {
		.registerIndex = offsetof(struct i2c_hid_desc,
			wReportDescRegister),
		.opcode = 0x00,
		.length = 2 };
/* commands */
static const struct i2c_hid_cmd hid_reset_cmd =		{ I2C_HID_CMD(0x01),
							  .wait = true };
static const struct i2c_hid_cmd hid_get_report_cmd =	{ I2C_HID_CMD(0x02) };
static const struct i2c_hid_cmd hid_set_report_cmd =	{ I2C_HID_CMD(0x03) };
static const struct i2c_hid_cmd hid_set_power_cmd =	{ I2C_HID_CMD(0x08) };
static const struct i2c_hid_cmd hid_vendor_cmd =	{ I2C_HID_CMD(0x0e) };
static const struct i2c_hid_cmd hid_no_cmd =		{ .length = 0 };

/*
 * These definitions are not used here, but are defined by the spec.
 * Keeping them here for documentation purposes.
 *
 * static const struct i2c_hid_cmd hid_get_idle_cmd = { I2C_HID_CMD(0x04) };
 * static const struct i2c_hid_cmd hid_set_idle_cmd = { I2C_HID_CMD(0x05) };
 * static const struct i2c_hid_cmd hid_get_protocol_cmd = { I2C_HID_CMD(0x06) };
 * static const struct i2c_hid_cmd hid_set_protocol_cmd = { I2C_HID_CMD(0x07) };
 */

/* vendor specific commands */
#define I2C_HID_VENDOR_CMD_CHUNK_DESCRIPTOR		0x01
#define I2C_HID_VENDOR_CMD_FIRMWARE_UPGRADE		0x02

static DEFINE_MUTEX(i2c_hid_open_mut);

/* The main device structure */
struct i2c_hid {
	struct i2c_client	*client;	/* i2c client */
	struct hid_device	*hid;	/* pointer to corresponding HID dev */
	union {
		__u8 hdesc_buffer[sizeof(struct i2c_hid_desc)];
		struct i2c_hid_desc hdesc;	/* the HID Descriptor */
	};
	__le16			wHIDDescRegister; /* location of the i2c
						   * register of the HID
						   * descriptor. */
	unsigned int		bufsize;	/* i2c buffer size */
	char			*irqinbuf;	/* Asynchronous Input buffer */
	char			*inbuf;		/* Input buffer */
	char			*cmdbuf;	/* Command buffer */
	char			*argsbuf;	/* Command arguments buffer */

	unsigned long		flags;		/* device flags */

	wait_queue_head_t	wait;		/* For waiting the interrupt */
	struct wake_lock	wake_lock;

	struct i2c_hid_platform_data pdata;

};

inline static int __i2c_hid_transfer(struct i2c_client *client,
				 struct i2c_msg *msg, int msg_num)
{
	const struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret;

	if (gpio_is_valid(ihid->pdata.wake_pin)) {
		gpio_set_value(ihid->pdata.wake_pin, 1);
		usleep_range(ihid->pdata.wake_rising_delay_min,
				ihid->pdata.wake_rising_delay_max);
	}

	ret = i2c_transfer(client->adapter, msg, msg_num);

	if (gpio_is_valid(ihid->pdata.wake_pin)) {
		gpio_set_value(ihid->pdata.wake_pin, 0);
		usleep_range(ihid->pdata.wake_falling_delay_min,
				ihid->pdata.wake_falling_delay_max);
	}

	return ret;
}

static int __i2c_hid_command(struct i2c_client *client,
		const struct i2c_hid_cmd *command, u8 reportID,
		u8 reportType, u8 *args, int args_len,
		unsigned char *buf_recv, int data_len)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	union command *cmd = (union command *)ihid->cmdbuf;
	int ret;
	struct i2c_msg msg[2];
	int msg_num = 1;

	int length = command->length;
	bool wait = command->wait;
	unsigned int registerIndex = command->registerIndex;

	/* special case for hid_descr_cmd */
	if (command == &hid_descr_cmd) {
		cmd->c.reg = ihid->wHIDDescRegister;
	} else {
		cmd->data[0] = ihid->hdesc_buffer[registerIndex];
		cmd->data[1] = ihid->hdesc_buffer[registerIndex + 1];
	}

	if (length > 2) {
		cmd->c.opcode = command->opcode;
		cmd->c.reportTypeID = reportID | reportType << 4;
	}

	memcpy(cmd->data + length, args, args_len);
	length += args_len;

	i2c_hid_dbg(ihid, "%s: cmd=%*ph\n", __func__, length, cmd->data);

	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = length;
	msg[0].buf = cmd->data;
	if (data_len > 0) {
		msg[1].addr = client->addr;
		msg[1].flags = client->flags & I2C_M_TEN;
		msg[1].flags |= I2C_M_RD;
		msg[1].len = data_len;
		msg[1].buf = buf_recv;
		msg_num = 2;
	}

	if (wait)
		set_bit(I2C_HID_RESET_PENDING, &ihid->flags);

	ret = __i2c_hid_transfer(client, msg, msg_num);
	if (ret != msg_num)
		return ret < 0 ? ret : -EIO;

	ret = 0;

	if (wait) {
		i2c_hid_dbg(ihid, "%s: waiting...\n", __func__);
		if (!wait_event_timeout(ihid->wait,
				!test_bit(I2C_HID_RESET_PENDING, &ihid->flags),
				msecs_to_jiffies(5000)))
			ret = -ENODATA;
		i2c_hid_dbg(ihid, "%s: finished.\n", __func__);
	}

	return ret;
}

static int i2c_hid_command(struct i2c_client *client,
		const struct i2c_hid_cmd *command,
		unsigned char *buf_recv, int data_len)
{
	return __i2c_hid_command(client, command, 0, 0, NULL, 0,
				buf_recv, data_len);
}

static int i2c_hid_get_report(struct i2c_client *client, u8 reportType,
		u8 reportID, unsigned char *buf_recv, int data_len)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	u8 args[3];
	int ret;
	int args_len = 0;
	u16 readRegister = le16_to_cpu(ihid->hdesc.wDataRegister);

	i2c_hid_dbg(ihid, "%s\n", __func__);

	if (reportID >= 0x0F) {
		args[args_len++] = reportID;
		reportID = 0x0F;
	}

	args[args_len++] = readRegister & 0xFF;
	args[args_len++] = readRegister >> 8;

	ret = __i2c_hid_command(client, &hid_get_report_cmd, reportID,
		reportType, args, args_len, buf_recv, data_len);
	if (ret) {
		dev_err(&client->dev,
			"failed to retrieve report from device.\n");
		return ret;
	}

	return 0;
}

static int i2c_hid_set_report(struct i2c_client *client, u8 reportType,
		u8 reportID, unsigned char *buf, size_t data_len)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	u8 *args = ihid->argsbuf;
	const struct i2c_hid_cmd * hidcmd = &hid_set_report_cmd;
	int ret;
	u16 dataRegister = le16_to_cpu(ihid->hdesc.wDataRegister);
	u16 outputRegister = le16_to_cpu(ihid->hdesc.wOutputRegister);
	u16 maxOutputLength = le16_to_cpu(ihid->hdesc.wMaxOutputLength);

	/* hidraw already checked that data_len < HID_MAX_BUFFER_SIZE */
	u16 size =	2			/* size */ +
			(reportID ? 1 : 0)	/* reportID */ +
			data_len		/* buf */;
	int args_len =	(reportID >= 0x0F ? 1 : 0) /* optional third byte */ +
			2			/* dataRegister */ +
			size			/* args */;
	int index = 0;

	i2c_hid_dbg(ihid, "%s\n", __func__);

	if (reportID >= 0x0F) {
		args[index++] = reportID;
		reportID = 0x0F;
	}

	/*
	 * use the data register for feature reports or if the device does not
	 * support the output register
	 */
	if (reportType == 0x03 || maxOutputLength == 0) {
		args[index++] = dataRegister & 0xFF;
		args[index++] = dataRegister >> 8;
	} else {
		args[index++] = outputRegister & 0xFF;
		args[index++] = outputRegister >> 8;
		hidcmd = &hid_no_cmd;
	}

	args[index++] = size & 0xFF;
	args[index++] = size >> 8;

	if (reportID)
		args[index++] = reportID;

	memcpy(&args[index], buf, data_len);

	ret = __i2c_hid_command(client, hidcmd, reportID,
		reportType, args, args_len, NULL, 0);
	if (ret) {
		dev_err(&client->dev, "failed to set a report to device.\n");
		return ret;
	}

	return data_len;
}

static int i2c_hid_set_power(struct i2c_client *client, int power_state)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret;

	i2c_hid_dbg(ihid, "%s\n", __func__);

	ret = __i2c_hid_command(client, &hid_set_power_cmd, power_state,
		0, NULL, 0, NULL, 0);
	if (ret)
		dev_err(&client->dev, "failed to change power setting.\n");

	return ret;
}

static int i2c_hid_vendor_chunk_descriptor(struct i2c_client *client,
		int offset, unsigned char *buf_recv, int data_len)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	u8 args[4];
	int ret;

	i2c_hid_dbg(ihid, "%s\n", __func__);

	args[0] = offset & 0xFF;
	args[1] = offset >> 8;
	args[2] = data_len & 0xFF;
	args[3] = data_len >> 8;

	ret = __i2c_hid_command(client, &hid_vendor_cmd,
		I2C_HID_VENDOR_CMD_CHUNK_DESCRIPTOR, 0, args, sizeof(args),
		buf_recv, data_len);
	if (ret) {
		dev_err(&client->dev,
			"failed to retrieve chunk descriptor from device.\n");
		return ret;
	}

	return 0;
}

static int i2c_hid_hwreset(struct i2c_client *client)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret;

	i2c_hid_dbg(ihid, "%s\n", __func__);

	ret = i2c_hid_set_power(client, I2C_HID_PWR_ON);
	if (ret)
		return ret;

	i2c_hid_dbg(ihid, "resetting...\n");

	ret = i2c_hid_command(client, &hid_reset_cmd, NULL, 0);
	if (ret) {
		dev_err(&client->dev, "failed to reset device.\n");
		i2c_hid_set_power(client, I2C_HID_PWR_SLEEP);
		return ret;
	}
	dev_info(&client->dev, "device reset succeed.\n");

	return 0;
}

static void i2c_hid_get_input(struct i2c_hid *ihid)
{
	int ret, ret_size;
	int size = le16_to_cpu(ihid->hdesc.wMaxInputLength);

	ret = i2c_master_recv(ihid->client, ihid->irqinbuf, size);
	if (ret != size) {
		if (ret < 0)
			return;

		dev_err(&ihid->client->dev, "%s: got %d data instead of %d\n",
			__func__, ret, size);
		return;
	}

	ret_size = ihid->irqinbuf[0] | ihid->irqinbuf[1] << 8;

	if (!ret_size) {
		/* host or device initiated RESET completed */
		if (test_and_clear_bit(I2C_HID_RESET_PENDING, &ihid->flags))
			wake_up(&ihid->wait);
		return;
	}

	if (ret_size > size) {
		dev_err(&ihid->client->dev, "%s: incomplete report (%d/%d)\n",
			__func__, size, ret_size);
		return;
	}

	i2c_hid_dbg(ihid, "input: %*ph\n", ret_size, ihid->irqinbuf);

	if (test_bit(I2C_HID_STARTED, &ihid->flags))
		hid_input_report(ihid->hid, HID_INPUT_REPORT,
				 ihid->irqinbuf + 2, ret_size - 2, 1);

	return;
}

static irqreturn_t i2c_hid_irq(int irq, void *dev_id)
{
	struct i2c_hid *ihid = dev_id;
	i2c_hid_get_input(ihid);

	if (test_and_clear_bit(I2C_HID_RESUME, &ihid->flags))
		wake_lock_timeout(&ihid->wake_lock, HZ);

	return IRQ_HANDLED;
}

static int i2c_hid_get_report_length(struct hid_report *report)
{
	return ((report->size - 1) >> 3) + 1 +
		report->device->report_enum[report->type].numbered + 2;
}

static void i2c_hid_init_report(struct hid_report *report, u8 *buffer,
	size_t bufsize)
{
	struct hid_device *hid = report->device;
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	unsigned int size, ret_size;

	size = i2c_hid_get_report_length(report);
	if (i2c_hid_get_report(client,
			report->type == HID_FEATURE_REPORT ? 0x03 : 0x01,
			report->id, buffer, size))
		return;

	i2c_hid_dbg(ihid, "report (len=%d): %*ph\n", size, size, ihid->inbuf);

	ret_size = buffer[0] | (buffer[1] << 8);

	if (ret_size != size) {
		dev_err(&client->dev, "error in %s size:%d / ret_size:%d\n",
			__func__, size, ret_size);
		return;
	}

	/* hid->driver_lock is held as we are in probe function,
	 * we just need to setup the input fields, so using
	 * hid_report_raw_event is safe. */
	hid_report_raw_event(hid, report->type, buffer + 2, size - 2, 1);
}

/*
 * Initialize all reports
 */
static void i2c_hid_init_reports(struct hid_device *hid)
{
	struct hid_report *report;
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	u8 *inbuf = kzalloc(ihid->bufsize, GFP_KERNEL);

	if (!inbuf) {
		dev_err(&client->dev, "can not retrieve initial reports\n");
		return;
	}

	list_for_each_entry(report,
		&hid->report_enum[HID_INPUT_REPORT].report_list, list)
		i2c_hid_init_report(report, inbuf, ihid->bufsize);

	list_for_each_entry(report,
		&hid->report_enum[HID_FEATURE_REPORT].report_list, list)
		i2c_hid_init_report(report, inbuf, ihid->bufsize);

	kfree(inbuf);
}

/*
 * Traverse the supplied list of reports and find the longest
 */
static void i2c_hid_find_max_report(struct hid_device *hid, unsigned int type,
		unsigned int *max)
{
	struct hid_report *report;
	unsigned int size;

	/* We should not rely on wMaxInputLength, as some devices may set it to
	 * a wrong length. */
	list_for_each_entry(report, &hid->report_enum[type].report_list, list) {
		size = i2c_hid_get_report_length(report);
		if (*max < size)
			*max = size;
	}
}

static void i2c_hid_free_buffers(struct i2c_hid *ihid)
{
	kfree(ihid->irqinbuf);
	kfree(ihid->inbuf);
	kfree(ihid->argsbuf);
	kfree(ihid->cmdbuf);
	ihid->irqinbuf = NULL;
	ihid->inbuf = NULL;
	ihid->cmdbuf = NULL;
	ihid->argsbuf = NULL;
	ihid->bufsize = 0;
}

static int i2c_hid_alloc_buffers(struct i2c_hid *ihid, size_t report_size)
{
	/* the worst case is computed from the set_report command with a
	 * reportID > 15 and the maximum report length */
	int args_len = sizeof(__u8) + /* optional ReportID byte */
		       sizeof(__u16) + /* data register */
		       sizeof(__u16) + /* size of the report */
		       report_size; /* report */

	ihid->irqinbuf = kzalloc(report_size, GFP_KERNEL);
	ihid->inbuf = kzalloc(report_size, GFP_KERNEL);
	ihid->argsbuf = kzalloc(args_len, GFP_KERNEL);
	ihid->cmdbuf = kzalloc(sizeof(union command) + args_len, GFP_KERNEL);

	if (!ihid->irqinbuf || !ihid->inbuf ||
	    !ihid->argsbuf || !ihid->cmdbuf) {
		i2c_hid_free_buffers(ihid);
		return -ENOMEM;
	}

	ihid->bufsize = report_size;

	return 0;
}

static int i2c_hid_get_raw_report(struct hid_device *hid,
		unsigned char report_number, __u8 *buf, size_t count,
		unsigned char report_type)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	size_t ret_count, ask_count;
	int ret;

	if (report_type == HID_OUTPUT_REPORT)
		return -EINVAL;

	/* +2 bytes to include the size of the reply in the query buffer */
	ask_count = min(count + 2, (size_t)ihid->bufsize);

	ret = i2c_hid_get_report(client,
			report_type == HID_FEATURE_REPORT ? 0x03 : 0x01,
			report_number, ihid->inbuf, ask_count);

	if (ret < 0)
		return ret;

	ret_count = ihid->inbuf[0] | (ihid->inbuf[1] << 8);

	if (ret_count <= 2)
		return 0;

	ret_count = min(ret_count, ask_count);

	/* The query buffer contains the size, dropping it in the reply */
	count = min(count, ret_count - 2);
	memcpy(buf, ihid->inbuf + 2, count);

	return count;
}

static int i2c_hid_output_raw_report(struct hid_device *hid, __u8 *buf,
		size_t count, unsigned char report_type)
{
	struct i2c_client *client = hid->driver_data;
	int report_id = buf[0];
	int ret;

	if (report_type == HID_INPUT_REPORT)
		return -EINVAL;

	if (report_id) {
		buf++;
		count--;
	}

	ret = i2c_hid_set_report(client,
				report_type == HID_FEATURE_REPORT ? 0x03 : 0x02,
				report_id, buf, count);

	if (report_id && ret >= 0)
		ret++; /* add report_id to the number of transfered bytes */

	return ret;
}

static void i2c_hid_request(struct hid_device *hid, struct hid_report *rep,
		int reqtype)
{
	struct i2c_client *client = hid->driver_data;
	char *buf;
	int ret;
	int len = i2c_hid_get_report_length(rep) - 2;

	buf = kzalloc(len, GFP_KERNEL);
	if (!buf)
		return;

	switch (reqtype) {
	case HID_REQ_GET_REPORT:
		ret = i2c_hid_get_raw_report(hid, rep->id, buf, len, rep->type);
		if (ret < 0)
			dev_err(&client->dev, "%s: unable to get report: %d\n",
				__func__, ret);
		else
			hid_input_report(hid, rep->type, buf, ret, 0);
		break;
	case HID_REQ_SET_REPORT:
		hid_output_report(rep, buf);
		i2c_hid_output_raw_report(hid, buf, len, rep->type);
		break;
	}

	kfree(buf);
}

static int i2c_hid_parse(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	struct i2c_hid_desc *hdesc = &ihid->hdesc;
	unsigned int rsize;
	char *rdesc;
	int i, ret;
	int tries = 3;

	i2c_hid_dbg(ihid, "entering %s\n", __func__);

	rsize = le16_to_cpu(hdesc->wReportDescLength);
	if (!rsize || rsize > HID_MAX_DESCRIPTOR_SIZE) {
		dbg_hid("weird size of report descriptor (%u)\n", rsize);
		return -EINVAL;
	}

	do {
		ret = i2c_hid_hwreset(client);
		if (ret)
			msleep(1000);
	} while (tries-- > 0 && ret);

	if (ret)
		return ret;

	rdesc = kzalloc(rsize, GFP_KERNEL);

	if (!rdesc) {
		dbg_hid("couldn't allocate rdesc memory\n");
		return -ENOMEM;
	}

	i2c_hid_dbg(ihid, "asking HID report descriptor\n");

	if (ihid->pdata.enable_i2c_limit_workaroud) {
		for (i = 0; i < rsize / 0xFF; ++i) {
			ret = i2c_hid_vendor_chunk_descriptor(client, i * 0xFF,
				rdesc + i * 0xFF, 0xFF);
			if (ret) {
				hid_err(hid, "reading report descriptor failed [1]\n");
				kfree(rdesc);
				return -EIO;
			}
		}
		if (rsize % 0xFF) {
			ret = i2c_hid_vendor_chunk_descriptor(client, i * 0xFF,
				rdesc + i * 0xFF, rsize % 0xFF);
			if (ret) {
				hid_err(hid, "reading report descriptor failed [2]\n");
				kfree(rdesc);
				return -EIO;
			}
		}
	} else {
		ret = i2c_hid_command(client, &hid_report_descr_cmd, rdesc, rsize);
		if (ret) {
			hid_err(hid, "reading report descriptor failed\n");
			kfree(rdesc);
			return -EIO;
		}
	}

	i2c_hid_dbg(ihid, "Report Descriptor: %*ph\n", rsize, rdesc);

	ret = hid_parse_report(hid, rdesc, rsize);
	kfree(rdesc);
	if (ret) {
		dbg_hid("parsing report descriptor failed\n");
		return ret;
	}

	return 0;
}

static int i2c_hid_start(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret;
	unsigned int bufsize = HID_MIN_BUFFER_SIZE;

	i2c_hid_find_max_report(hid, HID_INPUT_REPORT, &bufsize);
	i2c_hid_find_max_report(hid, HID_OUTPUT_REPORT, &bufsize);
	i2c_hid_find_max_report(hid, HID_FEATURE_REPORT, &bufsize);

	if (bufsize > ihid->bufsize) {
		i2c_hid_free_buffers(ihid);

		ret = i2c_hid_alloc_buffers(ihid, bufsize);

		if (ret)
			return ret;
	}

	if (!(hid->quirks & HID_QUIRK_NO_INIT_REPORTS))
		i2c_hid_init_reports(hid);

	return 0;
}

static void i2c_hid_stop(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);

	hid->claimed = 0;

	i2c_hid_free_buffers(ihid);
}

static int i2c_hid_open(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&i2c_hid_open_mut);
	if (!hid->open++) {
		ret = i2c_hid_set_power(client, I2C_HID_PWR_ON);
		if (ret) {
			hid->open--;
			goto done;
		}
		set_bit(I2C_HID_STARTED, &ihid->flags);
	}
done:
	mutex_unlock(&i2c_hid_open_mut);
	return ret;
}

static void i2c_hid_close(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);

	/* protecting hid->open to make sure we don't restart
	 * data acquistion due to a resumption we no longer
	 * care about
	 */
	mutex_lock(&i2c_hid_open_mut);
	if (!--hid->open) {
		clear_bit(I2C_HID_STARTED, &ihid->flags);

		/* Save some power */
		i2c_hid_set_power(client, I2C_HID_PWR_SLEEP);
	}
	mutex_unlock(&i2c_hid_open_mut);
}

static int i2c_hid_power(struct hid_device *hid, int lvl)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret = 0;

	i2c_hid_dbg(ihid, "%s lvl:%d\n", __func__, lvl);

	switch (lvl) {
	case PM_HINT_FULLON:
		ret = i2c_hid_set_power(client, I2C_HID_PWR_ON);
		break;
	case PM_HINT_NORMAL:
		ret = i2c_hid_set_power(client, I2C_HID_PWR_SLEEP);
		break;
	}
	return ret;
}

static int i2c_hid_hidinput_input_event(struct input_dev *dev,
		unsigned int type, unsigned int code, int value)
{
	struct hid_device *hid = input_get_drvdata(dev);
	struct hid_field *field;
	int offset;

	if (type == EV_FF)
		return input_ff_event(dev, type, code, value);

	if (type != EV_LED)
		return -1;

	offset = hidinput_find_field(hid, type, code, &field);

	if (offset == -1) {
		hid_warn(dev, "event field not found\n");
		return -1;
	}

	return hid_set_field(field, offset, value);
}

static struct hid_ll_driver i2c_hid_ll_driver = {
	.parse = i2c_hid_parse,
	.start = i2c_hid_start,
	.stop = i2c_hid_stop,
	.open = i2c_hid_open,
	.close = i2c_hid_close,
	.power = i2c_hid_power,
	.request = i2c_hid_request,
	.hidinput_input_event = i2c_hid_hidinput_input_event,
};

static int i2c_hid_init_irq(struct i2c_client *client)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret;

	dev_dbg(&client->dev, "Requesting IRQ: %d\n", client->irq);

	ret = request_threaded_irq(client->irq, NULL, i2c_hid_irq,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			client->name, ihid);
	if (ret < 0) {
		dev_warn(&client->dev,
			"Could not register for %s interrupt, irq = %d,"
			" ret = %d\n",
			client->name, client->irq, ret);

		return ret;
	}

	return 0;
}

static int i2c_hid_fetch_hid_descriptor(struct i2c_hid *ihid)
{
	struct i2c_client *client = ihid->client;
	struct i2c_hid_desc *hdesc = &ihid->hdesc;
	unsigned int dsize;
	int ret;

	/* Fetch the length of HID description, retrieve the 4 first bytes:
	 * bytes 0-1 -> length
	 * bytes 2-3 -> bcdVersion (has to be 1.00) */
	ret = i2c_hid_command(client, &hid_descr_cmd, ihid->hdesc_buffer, 4);

	i2c_hid_dbg(ihid, "%s, ihid->hdesc_buffer: %4ph\n", __func__,
			ihid->hdesc_buffer);

	if (ret) {
		dev_err(&client->dev,
			"unable to fetch the size of HID descriptor (ret=%d)\n",
			ret);
		return -ENODEV;
	}

	dsize = le16_to_cpu(hdesc->wHIDDescLength);
	/*
	 * the size of the HID descriptor should at least contain
	 * its size and the bcdVersion (4 bytes), and should not be greater
	 * than sizeof(struct i2c_hid_desc) as we directly fill this struct
	 * through i2c_hid_command.
	 */
	if (dsize < 4 || dsize > sizeof(struct i2c_hid_desc)) {
		dev_err(&client->dev, "weird size of HID descriptor (%u)\n",
			dsize);
		return -ENODEV;
	}

	/* check bcdVersion == 1.0 */
	if (le16_to_cpu(hdesc->bcdVersion) != 0x0100) {
		dev_err(&client->dev,
			"unexpected HID descriptor bcdVersion (0x%04hx)\n",
			le16_to_cpu(hdesc->bcdVersion));
		return -ENODEV;
	}

	i2c_hid_dbg(ihid, "Fetching the HID descriptor\n");

	ret = i2c_hid_command(client, &hid_descr_cmd, ihid->hdesc_buffer,
				dsize);
	if (ret) {
		dev_err(&client->dev, "hid_descr_cmd Fail\n");
		return -ENODEV;
	}

	i2c_hid_dbg(ihid, "HID Descriptor: %*ph\n", dsize, ihid->hdesc_buffer);

	return 0;
}

#ifdef CONFIG_OF
static int i2c_hid_of_probe(struct i2c_client *client,
		struct i2c_hid_platform_data *pdata)
{
	struct device *dev = &client->dev;
	u32 val;
	int ret;

	ret = of_property_read_u32(dev->of_node, "hid-descr-addr", &val);
	if (ret) {
		dev_err(&client->dev, "HID register address not provided\n");
		return -ENODEV;
	}
	if (val >> 16) {
		dev_err(&client->dev, "Bad HID register address: 0x%08x\n",
			val);
		return -EINVAL;
	}
	pdata->hid_descriptor_address = val;

	pdata->enable_i2c_limit_workaroud = of_property_read_bool(dev->of_node,
						"enable_i2c_limit_workaroud");
	pdata->enable_wakeup_host = of_property_read_bool(dev->of_node,
						"enable_wakeup_host");

	ret = of_get_named_gpio(dev->of_node, "wake-gpio", 0);
	pdata->wake_pin = ret;

	if (!gpio_is_valid(pdata->wake_pin))
		return 0;

	dev_info(&client->dev, "Wake GPIO=%d\n", pdata->wake_pin);

	ret = of_property_read_u32(dev->of_node, "wake-rising-delay-min", &val);
	if (ret) {
		dev_err(&client->dev, "Wake rising delay min not provided\n");
		return -ENODEV;
	}
	pdata->wake_rising_delay_min = val;
	ret = of_property_read_u32(dev->of_node, "wake-rising-delay-max", &val);
	if (ret) {
		dev_err(&client->dev, "Wake rising delay max not provided\n");
		return -ENODEV;
	}
	pdata->wake_rising_delay_max = val;
	ret = of_property_read_u32(dev->of_node, "wake-falling-delay-min", &val);
	if (ret) {
		dev_err(&client->dev, "Wake falling delay min not provided\n");
		return -ENODEV;
	}
	pdata->wake_falling_delay_min = val;
	ret = of_property_read_u32(dev->of_node, "wake-falling-delay-max", &val);
	if (ret) {
		dev_err(&client->dev, "Wake falling delay max not provided\n");
		return -ENODEV;
	}
	pdata->wake_falling_delay_max = val;

	return 0;
}

static const struct of_device_id i2c_hid_of_match[] = {
	{ .compatible = "hid-over-i2c" },
	{},
};
MODULE_DEVICE_TABLE(of, i2c_hid_of_match);
#else
static inline int i2c_hid_of_probe(struct i2c_client *client,
		struct i2c_hid_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static int i2c_hid_probe(struct i2c_client *client,
			 const struct i2c_device_id *dev_id)
{
	int ret;
	unsigned int version;
	struct i2c_hid *ihid;
	struct hid_device *hid;
	__u16 hidRegister;
	struct i2c_hid_platform_data *platform_data = client->dev.platform_data;

	dbg_hid("HID probe called for i2c 0x%02x\n", client->addr);

	if (!client->irq) {
		dev_err(&client->dev,
			"HID over i2c has not been provided an Int IRQ\n");
		return -EINVAL;
	}

	ihid = kzalloc(sizeof(struct i2c_hid), GFP_KERNEL);
	if (!ihid)
		return -ENOMEM;

	wake_lock_init(&ihid->wake_lock, WAKE_LOCK_SUSPEND, "i2c-hid");

	if (client->dev.of_node) {
		ret = i2c_hid_of_probe(client, &ihid->pdata);
		if (ret)
			goto err;
	} else if (!platform_data) {
		dev_err(&client->dev, "HID register address not provided\n");
		ret = -ENODEV;
		goto err;
	} else {
		ihid->pdata = *platform_data;
	}

	if (gpio_is_valid(ihid->pdata.wake_pin)) {
		ret = gpio_request(ihid->pdata.wake_pin, "i2c-hid_wake");
		if (ret) {
			dev_err(&client->dev, "Cannot request Wake GPIO\n");
			goto err;
		}
		ret = gpio_direction_output(ihid->pdata.wake_pin, 0);
		if (ret) {
			dev_err(&client->dev, "Cannot set Wake GPIO\n");
			goto err_gpio;
		}
	}

	i2c_set_clientdata(client, ihid);

	ihid->client = client;

	hidRegister = ihid->pdata.hid_descriptor_address;
	ihid->wHIDDescRegister = cpu_to_le16(hidRegister);

	init_waitqueue_head(&ihid->wait);

	/* we need to allocate the command buffer without knowing the maximum
	 * size of the reports. Let's use HID_MIN_BUFFER_SIZE, then we do the
	 * real computation later. */
	ret = i2c_hid_alloc_buffers(ihid, HID_MIN_BUFFER_SIZE);
	if (ret < 0)
		goto err_gpio;

	ret = i2c_hid_fetch_hid_descriptor(ihid);
	if (ret < 0)
		goto err_gpio;

	ret = i2c_hid_init_irq(client);
	if (ret < 0)
		goto err_gpio;

	hid = hid_allocate_device();
	if (IS_ERR(hid)) {
		ret = PTR_ERR(hid);
		goto err_irq;
	}
	if (ihid->pdata.enable_wakeup_host) {
		ret = device_init_wakeup(&client->dev, true);
		if (ret) {
			device_init_wakeup(&client->dev, false);
			dev_err(&client->dev, "init wakeup failed: err=%d\n", ret);
		}
	}
	ihid->hid = hid;

	hid->driver_data = client;
	hid->ll_driver = &i2c_hid_ll_driver;
	hid->hid_get_raw_report = i2c_hid_get_raw_report;
	hid->hid_output_raw_report = i2c_hid_output_raw_report;
	hid->dev.parent = &client->dev;
	hid->bus = BUS_I2C;
	hid->version = le16_to_cpu(ihid->hdesc.bcdVersion);
	hid->vendor = le16_to_cpu(ihid->hdesc.wVendorID);
	hid->product = le16_to_cpu(ihid->hdesc.wProductID);
	version = le16_to_cpu(ihid->hdesc.wVersionID);

	snprintf(hid->name, sizeof(hid->name), "%s %04hX:%04hX",
		 client->name, hid->vendor, hid->product);
	if ((version & 0x000f) == 0)
		snprintf(hid->uniq, sizeof(hid->uniq), "%u.%u.%u",
				(version & 0xf000) >> 12, (version & 0x0f00) >> 8,
				(version & 0x00f0) >> 4);
	else
		snprintf(hid->uniq, sizeof(hid->uniq), "%u.%u.%u-rc%u",
				(version & 0xf000) >> 12, (version & 0x0f00) >> 8,
				(version & 0x00f0) >> 4, version & 0x000f);

	ret = hid_add_device(hid);
	if (ret) {
		if (ret != -ENODEV)
			hid_err(client, "can't add hid device: %d\n", ret);
		goto err_mem_free;
	}

	return 0;

err_mem_free:
	hid_destroy_device(hid);

err_irq:
	free_irq(client->irq, ihid);

err_gpio:
	if (gpio_is_valid(ihid->pdata.wake_pin))
		gpio_free(ihid->pdata.wake_pin);

err:
	i2c_hid_free_buffers(ihid);
	wake_lock_destroy(&ihid->wake_lock);
	kfree(ihid);
	return ret;
}

static int i2c_hid_remove(struct i2c_client *client)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	struct hid_device *hid;

	i2c_hid_set_power(client, I2C_HID_PWR_SLEEP);

	hid = ihid->hid;
	hid_destroy_device(hid);

	if (ihid->pdata.enable_wakeup_host)
		device_init_wakeup(&client->dev, false);

	free_irq(client->irq, ihid);

	if (gpio_is_valid(ihid->pdata.wake_pin))
		gpio_free(ihid->pdata.wake_pin);

	if (ihid->bufsize)
		i2c_hid_free_buffers(ihid);

	wake_lock_destroy(&ihid->wake_lock);
	kfree(ihid);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int i2c_hid_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	if (device_may_wakeup(&client->dev)) {
		enable_irq_wake(client->irq);
		disable_irq(client->irq);
	}
	/* Save some power */
	i2c_hid_set_power(client, I2C_HID_PWR_SLEEP);

	return 0;
}

static int i2c_hid_resume(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_hid *ihid = i2c_get_clientdata(client);

	dev_err(dev, "i2c_hid_resume\n");
	ret = i2c_hid_set_power(client, I2C_HID_PWR_ON);
	if (ret)
		return ret;

	if (device_may_wakeup(&client->dev)) {
		disable_irq_wake(client->irq);
		enable_irq(client->irq);
	}

	set_bit(I2C_HID_RESUME, &ihid->flags);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(i2c_hid_pm, i2c_hid_suspend, i2c_hid_resume);

static const struct i2c_device_id i2c_hid_id_table[] = {
	{ "hid", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, i2c_hid_id_table);


static struct i2c_driver i2c_hid_driver = {
	.driver = {
		.name	= "i2c_hid",
		.owner	= THIS_MODULE,
		.pm	= &i2c_hid_pm,
		.of_match_table = of_match_ptr(i2c_hid_of_match),
	},

	.probe		= i2c_hid_probe,
	.remove		= i2c_hid_remove,

	.id_table	= i2c_hid_id_table,
};

static const struct hid_device_id hid_i2chid_table[] = {
	{ HID_DEVICE(BUS_I2C, HID_GROUP_GENERIC, HID_ANY_ID, HID_ANY_ID) },
	{ }
};

static struct hid_driver hid_i2chid_driver = {
	.name = "generic-i2c",
	.id_table = hid_i2chid_table,
};

static int __init i2chid_init(void)
{
	int retval;

	retval = hid_register_driver(&hid_i2chid_driver);
	if (retval)
		goto hid_register_fail;

	retval = i2c_add_driver(&i2c_hid_driver);
	if (retval)
		goto i2c_register_fail;

	return 0;

i2c_register_fail:
	hid_unregister_driver(&hid_i2chid_driver);

hid_register_fail:
	return retval;
}

static void __exit i2chid_exit(void)
{
	i2c_del_driver(&i2c_hid_driver);
	hid_unregister_driver(&hid_i2chid_driver);
}

module_init(i2chid_init);
module_exit(i2chid_exit);

MODULE_DESCRIPTION("HID over I2C core driver");
MODULE_AUTHOR("Benjamin Tissoires <benjamin.tissoires@gmail.com>");
MODULE_LICENSE("GPL");
