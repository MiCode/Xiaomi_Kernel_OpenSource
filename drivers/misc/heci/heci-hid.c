/*
 * HECI-HID glue driver.
 *
 * Copyright (c) 2012-2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include "heci-hid.h"
#include "platform-config.h"
#include "client.h"

/*
 * TODO - figure out if this number is used for anything but assignment.
 * BUS_I2C is not
 */
#define	BUS_HECI	0x44
/* TODO: just to bootstrap, numbers will probably change */
#define	ISH_HID_VENDOR	0x8086
#define	ISH_HID_PRODUCT	0x22D8
#define	ISH_HID_VERSION	0x0200

static int heci_hid_parse(struct hid_device *hid)
{
	int	rv;

	ISH_DBG_PRINT(KERN_ALERT "[hid-heci]: %s():+++\n", __func__);

	rv = hid_parse_report(hid, report_descr[cur_hid_dev],
		report_descr_size[cur_hid_dev]);
	if (rv) {
		ISH_DBG_PRINT(KERN_ALERT
			"[heci-hid] %s(): parsing report descriptor failed\n",
			__func__);
		return	rv;
	}

	ISH_DBG_PRINT(KERN_ALERT
		"[heci-hid] %s(): parsing report descriptor succeeded\n",
		__func__);
	return 0;
}

static int heci_hid_start(struct hid_device *hid)
{
	return 0;
}

/* should we free smth? */
static void heci_hid_stop(struct hid_device *hid)
{
	return;
}

/* probably connect might be here (move from probe) */
static int heci_hid_open(struct hid_device *hid)
{
	return 0;
}


/* naturally if connect in open, disconnect here */
static void ish_heci_close(struct hid_device *hid)
{
	return;
}

static int heci_hid_power(struct hid_device *hid, int lvl)
{
	return 0;
}



static void heci_hid_request(struct hid_device *hid, struct hid_report *rep,
	int reqtype)
{
	/* the specific report length, just HID part of it */
	unsigned len = ((rep->size - 1) >> 3) + 1 + (rep->id > 0);
	char *buf;
	/* s32 checkValue = 0; */
	/* int i = 0; */
	unsigned header_size =  sizeof(struct hostif_msg);

	len += header_size;

	switch (reqtype) {
	case HID_REQ_GET_REPORT:
		hid_heci_get_report(hid, rep->id, rep->type);
		break;
	case HID_REQ_SET_REPORT:
		buf = kzalloc(len, GFP_KERNEL);
		if (!buf)
			return;
		hid_output_report(rep, buf + header_size);
	/* checkValue = rep->field[3]->value[0]; */
	/* ISH_DBG_PRINT(KERN_ALERT
		"[hid-ish]: %s(): after hid_output_report value is %d\n",
		__func__, checkValue);	 */
	/* for(;i < len; i++) */
	/*   ISH_DBG_PRINT("\n%d %d\n", i, (int) buf[i]); */
		hid_heci_set_feature(hid, buf, len, rep->id);
		kfree(buf);
		break;
	}

	return;
}


static int heci_hid_hidinput_input_event(struct input_dev *dev,
		unsigned int type, unsigned int code, int value)
{
	return 0;
}


static int heci_wait_for_response(struct hid_device *hid)
{
	ISH_DBG_PRINT(KERN_ALERT "%s() +++\n", __func__);
#ifdef HOST_VIRTUALBOX
	timed_wait_for_timeout(WAIT_FOR_SEND_SLICE, get_report_done, (10 * HZ));
#else
	if (!get_report_done)
		wait_event_timeout(heci_hid_wait, get_report_done, 3 * HZ);

#endif
	if (!get_report_done) {
		hid_err(hid, "timeout waiting for response from HECI device\n");
		return -1;
	}

	get_report_done = 0;
	return 0;
}


static struct hid_ll_driver heci_hid_ll_driver = {
	.parse = heci_hid_parse,
	.start = heci_hid_start,
	.stop = heci_hid_stop,
	.open = heci_hid_open,
	.close = ish_heci_close,
	.power = heci_hid_power,
	.request = heci_hid_request,
	.hidinput_input_event = heci_hid_hidinput_input_event,
	.wait = heci_wait_for_response
};


struct tmp_heci_data {
	int hdesc_length;
	struct task_struct	*read_task;
};

static int heci_hid_get_raw_report(struct hid_device *hid,
	unsigned char report_number, __u8 *buf, size_t count,
	unsigned char report_type)
{
	return	0;
}

static int heci_hid_output_raw_report(struct hid_device *hid, __u8 *buf,
	size_t count, unsigned char report_type)
{
	return	0;
}

/*
 * probably the best way make it driver probe so it will create device with
 * itself as ll_driver, as usb and i2c do
 */
int	heci_hid_probe(unsigned cur_hid_dev)
{
	int rv;
	struct hid_device	*hid;

	ISH_DBG_PRINT(KERN_ALERT "[hid-heci]: %s():+++\n", __func__);

	hid = hid_allocate_device();
	if (IS_ERR(hid)) {
		rv = PTR_ERR(hid);
		return	-ENOMEM;
	}

	hid_sensor_hubs[cur_hid_dev] = hid;

	hid->ll_driver = &heci_hid_ll_driver;
	hid->hid_get_raw_report = heci_hid_get_raw_report;
	hid->hid_output_raw_report = heci_hid_output_raw_report;
	hid->bus = BUS_HECI;
	hid->version = le16_to_cpu(ISH_HID_VERSION);
	hid->vendor = le16_to_cpu(ISH_HID_VENDOR);
	hid->product = le16_to_cpu(ISH_HID_PRODUCT);

	snprintf(hid->name, sizeof(hid->name), "%s %04hX:%04hX", "hid-heci",
		hid->vendor, hid->product);

	rv = hid_add_device(hid);
	if (rv) {
		if (rv != -ENODEV)
			printk(KERN_ERR "[hid-heci]: can't add HID device: %d\n",
				rv);
		kfree(hid);
		return	rv;
	}

#if 0
	/* Initialize all reports */
	list_for_each_entry(report,
		&hid->report_enum[HID_FEATURE_REPORT].report_list, list)
		hid_heci_get_report(hid, report->id, HID_FEATURE_REPORT);
#endif

	ISH_DBG_PRINT(KERN_ALERT "[hid-heci]: %s():---\n", __func__);
	return 0;
}

void	heci_hid_remove(void)
{
	int	i;

	ISH_DBG_PRINT(KERN_ALERT "[hid-heci]: %s():+++\n", __func__);
	for (i = 0; i < num_hid_devices; ++i)
		if (hid_sensor_hubs[i]) {
			hid_destroy_device(hid_sensor_hubs[i]);
			hid_sensor_hubs[i] = NULL;
		}
	ISH_DBG_PRINT(KERN_ALERT "[hid-heci]: %s():---\n", __func__);
}

void register_flush_cb(void (*flush_cb_func)(void))
{
	flush_cb = flush_cb_func;
}
EXPORT_SYMBOL_GPL(register_flush_cb);

