/*
 * HECI client driver for HID (ISS)
 *
 * Copyright (c) 2014-2015, Intel Corporation.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/uuid.h>
#include "heci_dev.h"
#include "client.h"
#include "heci-hid.h"

/* Rx ring buffer pool size */
#define RX_RING_SIZE	32
#define TX_RING_SIZE	16

/* Global vars, may eventually end up in a structure */
struct heci_cl	*hid_heci_cl = NULL;			/* ISS HECI client */

/* Set when ISS HECI client is successfully probed */
int	hid_heci_client_found;
int	may_send;		/* Global flag that determines if sender thread
				can safely send something or it should
				wait more */
int	enum_devices_done;	/* Enum devices response complete flag */
int	hid_descr_done;		/* Get HID descriptor complete flag */
int	report_descr_done;	/* Get report descriptor complete flag */
int	get_report_done;	/* Get Feature/Input report complete flag */

struct device_info	*hid_devices;
unsigned	cur_hid_dev;
unsigned	hid_dev_count;
unsigned	max_hid_devices = /*1*/ MAX_HID_DEVICES;
unsigned	num_hid_devices;
unsigned char	*hid_descr[MAX_HID_DEVICES];
int	hid_descr_size[MAX_HID_DEVICES];
unsigned char	*report_descr[MAX_HID_DEVICES];
int	report_descr_size[MAX_HID_DEVICES];
struct hid_device	*hid_sensor_hubs[MAX_HID_DEVICES];

static wait_queue_head_t	init_wait;
wait_queue_head_t	heci_hid_wait;

/*flush notification*/
void (*flush_cb)(void);

/* HECI client driver structures and API for bus interface */
void	process_recv(void *recv_buf, size_t data_len)
{
	struct hostif_msg	*recv_msg;
	unsigned char	*payload;
	/*size_t	size;*/
	struct device_info	*dev_info;
	int	i, j;
	size_t	payload_len, total_len, cur_pos;
	int	report_type;

	struct report_list *reports_list;
	char *reports;
	size_t report_len;

	ISH_DBG_PRINT(KERN_ALERT "[hid-ish]: %s():+++ len=%u\n", __func__,
		(unsigned)data_len);

	if (data_len < sizeof(struct hostif_msg_hdr)) {
		printk(KERN_ERR "[hid-ish]: error, received %u which is less than data header %u\n",
			(unsigned)data_len,
			(unsigned)sizeof(struct hostif_msg_hdr));
		return;
	}

	payload = recv_buf + sizeof(struct hostif_msg_hdr);
	total_len = data_len;
	cur_pos = 0;

	may_send = 0;

	do {
		recv_msg = (struct hostif_msg *)(recv_buf + cur_pos);
		payload_len = recv_msg->hdr.size;

		switch (recv_msg->hdr.command & CMD_MASK) {
		default:
			break;

		case HOSTIF_DM_ENUM_DEVICES:
			ISH_DBG_PRINT(KERN_ALERT
				"[hid-ish]: %s(): received HOSTIF_DM_ENUM_DEVICES\n",
				__func__);
			g_ish_print_log(
				"%s() received HOSTIF_DM_ENUM_DEVICES\n"
				, __func__);
			hid_dev_count = (unsigned)*payload;
			ISH_DBG_PRINT(KERN_ALERT
				"[hid-ish]: %s(): hid_dev_count=%d\n",
				__func__, hid_dev_count);
			hid_devices = kmalloc(hid_dev_count *
				sizeof(struct device_info), GFP_KERNEL);
			if (hid_devices)
				memset(hid_devices, 0, hid_dev_count *
					sizeof(struct device_info));

			for (i = 0; i < hid_dev_count; ++i) {
				if (1 + sizeof(struct device_info) * i >=
						payload_len)
					printk(KERN_ERR "[hid-ish]: [HOSTIF_DM_ENUM_DEVICES]: content size %u is bigger than payload_len %u\n",
						1 + (unsigned)(sizeof(struct device_info) * i),
						(unsigned)payload_len);

				if (1 + sizeof(struct device_info) * i >=
						data_len)
					break;

				dev_info = (struct device_info *)(payload + 1 +
					sizeof(struct device_info) * i);
				ISH_DBG_PRINT(KERN_ALERT
					"[hid-ish]: %s(): [%d] -- dev_id=%08X dev_class=%02X pid=%04X vid=%04X\n",
					__func__, i, dev_info->dev_id,
					dev_info->dev_class, dev_info->pid,
					dev_info->vid);
				if (hid_devices)
					memcpy(hid_devices + i, dev_info,
						sizeof(struct device_info));
			}

			enum_devices_done = 1;
			if (waitqueue_active(&init_wait))
				wake_up(&init_wait);

			break;

		case HOSTIF_GET_HID_DESCRIPTOR:
			ISH_DBG_PRINT(KERN_ALERT
				"[hid-ish]: %s(): received HOSTIF_GET_HID_DESCRIPTOR\n",
				__func__);
			g_ish_print_log(
				"%s() received HOSTIF_GET_HID_DESCRIPTOR\n"
				, __func__);
			ISH_DBG_PRINT(KERN_ALERT
				"[hid-ish]: %s(): dump HID descriptor\n",
				__func__);
			for (i = 0; i < payload_len; ++i)
				ISH_DBG_PRINT(KERN_ALERT "%02X ", payload[i]);
			ISH_DBG_PRINT(KERN_ALERT "\n");
			hid_descr[cur_hid_dev] = kmalloc(payload_len,
				GFP_KERNEL);
			if (hid_descr[cur_hid_dev])
				memcpy(hid_descr[cur_hid_dev], payload,
					payload_len);
			hid_descr_size[cur_hid_dev] = payload_len;

			hid_descr_done = 1;
			if (waitqueue_active(&init_wait))
				wake_up(&init_wait);

			break;

		case HOSTIF_GET_REPORT_DESCRIPTOR:
			ISH_DBG_PRINT(KERN_ALERT
				"[hid-ish]: %s(): received HOSTIF_GET_REPORT_DESCRIPTOR\n",
				__func__);
			g_ish_print_log(
				"%s() received HOSTIF_GET_REPORT_DESCRIPTOR\n"
				, __func__);
			ISH_DBG_PRINT(KERN_ALERT
				"[hid-ish]: %s(): Length of report descriptor is %u\n",
				__func__, (unsigned)payload_len);
			report_descr[cur_hid_dev] = kmalloc(payload_len,
				GFP_KERNEL);
			if (report_descr[cur_hid_dev])
				memcpy(report_descr[cur_hid_dev], payload,
					payload_len);
			report_descr_size[cur_hid_dev] = payload_len;

			report_descr_done = 1;
			if (waitqueue_active(&init_wait))
				wake_up(&init_wait);

			break;

		case HOSTIF_GET_FEATURE_REPORT:
			report_type = HID_FEATURE_REPORT;
			ISH_DBG_PRINT(KERN_ALERT
				"[hid-ish]: %s(): received HOSTIF_GET_FEATURE_REPORT\n",
				__func__);
			g_ish_print_log(
				"%s() received HOSTIF_GET_FEATURE_REPORT\n",
				__func__);
			ISH_DBG_PRINT(KERN_ALERT
				"[hid-ish]: %s(): dump Get Feature Result\n",
				__func__);
			flush_cb(); /*each "GET_FEATURE_REPORT" ends a batch*/
			goto	do_get_report;

		case HOSTIF_GET_INPUT_REPORT:
			report_type = HID_INPUT_REPORT;
			ISH_DBG_PRINT(KERN_ALERT
				"[hid-ish]: %s(): received HOSTIF_GET_INPUT_REPORT\n",
				__func__);
			g_ish_print_log(
				"%s() received HOSTIF_GET_INPUT_REPORT\n"
				, __func__);
			ISH_DBG_PRINT(KERN_ALERT
				"[hid-ish]: %s(): dump Get Input Result\n",
				__func__);
do_get_report:
			for (i = 0; i < payload_len; ++i)
				ISH_DBG_PRINT(KERN_ALERT "%02X ", payload[i]);
			ISH_DBG_PRINT(KERN_ALERT "\n");


			/* Get index of device that matches this id */
			for (i = 0; i < num_hid_devices; ++i)
				if (recv_msg->hdr.device_id ==
						hid_devices[i].dev_id)
					if (hid_sensor_hubs[i] != NULL) {
						hid_input_report(
							hid_sensor_hubs[i],
							report_type, payload,
							payload_len, 0);
						break;
					}
			ISH_DBG_PRINT(KERN_ALERT
				"%s(): received input report, upstreaming\n",
				__func__);
			get_report_done = 1;
			if (waitqueue_active(&heci_hid_wait))
				wake_up(&heci_hid_wait);
			break;

		case HOSTIF_SET_FEATURE_REPORT:
			ISH_DBG_PRINT(KERN_ALERT
				"[hid-ish]: %s(): HOSTIF_SET_FEATURE_REPORT returned status=%02X\n",
				__func__, recv_msg->hdr.status);
			g_ish_print_log(
				"%s() HOSTIF_SET_FEATURE_REPORT returned status=%02X\n"
				, __func__, recv_msg->hdr.status);
			ISH_DBG_PRINT(KERN_ALERT
				"%s(): received feature report, upstreaming\n",
				__func__);
			get_report_done = 1;
			if (waitqueue_active(&heci_hid_wait))
				wake_up(&heci_hid_wait);
			break;

		case HOSTIF_PUBLISH_INPUT_REPORT:
			report_type = HID_INPUT_REPORT;
			g_ish_print_log(
				"%s() received ASYNC DATA REPORT\n"
				, __func__, (unsigned)payload_len);
			do {
				ISH_DBG_PRINT(KERN_ALERT
					"[hid-ish]: %s(): received ASYNC DATA REPORT [payload_len=%u]. Dump data:\n",
					__func__, (unsigned)payload_len);
				for (i = 0; i < payload_len; ++i)
					ISH_DBG_PRINT(KERN_ALERT "%02X\n",
						payload[i]);
			} while (0);

			for (i = 0; i < num_hid_devices; ++i)
				if (recv_msg->hdr.device_id ==
						hid_devices[i].dev_id)
					if (hid_sensor_hubs[i] != NULL)
						hid_input_report(
							hid_sensor_hubs[i],
							report_type, payload,
							payload_len, 0);
			break;

		case HOSTIF_PUBLISH_INPUT_REPORT_LIST:
			ISH_DBG_PRINT(KERN_ALERT
				"[hid-ish]: %s(): received HOSTIF_PUBLISH_INPUT_REPORT_LIST\n",
				__func__);

			report_type = HID_INPUT_REPORT;
			reports_list = (struct report_list *)payload;
			reports = (char *)reports_list->reports;

			for (j = 0; j < reports_list->num_of_reports; j++) {
				recv_msg = (struct hostif_msg *)(reports +
					sizeof(uint16_t));
				report_len = *(uint16_t *)reports;
				payload = reports + sizeof(uint16_t) +
					sizeof(struct hostif_msg_hdr);
				payload_len = report_len -
					sizeof(struct hostif_msg_hdr);

				ISH_DBG_PRINT(KERN_ALERT
					"[hid-ish]: %s(): report #%d, report_len: %d, payload_len: %d, device_id: %d, payload Data\n",
					__func__, j, (int)report_len,
					(int)payload_len,
					(int)recv_msg->hdr.device_id);
				for (i = 0; i < payload_len; ++i)
					ISH_DBG_PRINT(KERN_ALERT "%02X ",
						payload[i]);
				ISH_DBG_PRINT(KERN_ALERT "\n");

				for (i = 0; i < num_hid_devices; ++i)
					if (recv_msg->hdr.device_id ==
							hid_devices[i].dev_id &&
							hid_sensor_hubs[i] !=
							NULL) {
						hid_input_report(
							hid_sensor_hubs[i],
							report_type,
							payload, payload_len,
							0);
					}

				reports += sizeof(uint16_t) + report_len;
			}
			break;

		}

		cur_pos += payload_len + sizeof(struct hostif_msg);
		payload += payload_len + sizeof(struct hostif_msg);

	} while (cur_pos < total_len);
	may_send = 1;
}


void ish_cl_event_cb(struct heci_cl_device *device, u32 events, void *context)
{
	size_t r_length;
	struct heci_cl_rb *rb_in_proc;
	unsigned long	flags;

	ISH_DBG_PRINT(KERN_ALERT "%s() +++\n", __func__);

	if (!hid_heci_cl)
		return;

	spin_lock_irqsave(&hid_heci_cl->in_process_spinlock, flags);
	while (!list_empty(&hid_heci_cl->in_process_list.list)) {
		rb_in_proc = list_entry(hid_heci_cl->in_process_list.list.next,
			struct heci_cl_rb, list);
		list_del_init(&rb_in_proc->list);
		spin_unlock_irqrestore(&hid_heci_cl->in_process_spinlock,
			flags);

		if (!rb_in_proc->buffer.data) {
			ISH_DBG_PRINT(KERN_ALERT
				"%s(): !rb_in_proc-->buffer.data, something's wrong\n",
				__func__);
			return;
		}
		r_length = rb_in_proc->buf_idx;
		ISH_DBG_PRINT(KERN_ALERT
			"%s(): OK received buffer of %u length\n", __func__,
			(unsigned)r_length);

		/* decide what to do with received data */
		process_recv(rb_in_proc->buffer.data, r_length);

		heci_io_rb_recycle(rb_in_proc);
		spin_lock_irqsave(&hid_heci_cl->in_process_spinlock, flags);
	}
	spin_unlock_irqrestore(&hid_heci_cl->in_process_spinlock, flags);
}

void hid_heci_set_feature(struct hid_device *hid, char *buf, unsigned len,
	int report_id)
{
	int	rv;
	struct hostif_msg *msg = (struct hostif_msg *)buf;
	int	i;

	ISH_DBG_PRINT(KERN_ALERT
		"[hid-ish]: %s(): writing SET FEATURE REPORT\n", __func__);
	memset(msg, 0, sizeof(struct hostif_msg));
	msg->hdr.command = HOSTIF_SET_FEATURE_REPORT;
	for (i = 0; i < num_hid_devices; ++i)
		if (hid == hid_sensor_hubs[i]) {
			msg->hdr.device_id = hid_devices[i].dev_id;
			break;
		}
	if (i == num_hid_devices)
		return;

	rv = heci_cl_send(hid_heci_cl, buf, len);
	ISH_DBG_PRINT(KERN_ALERT
		"[hid-ish]: %s(): heci_cl_send() returned %d\n", __func__, rv);
}


void hid_heci_get_report(struct hid_device *hid, int report_id, int report_type)
{
	int	rv;
	static unsigned char	buf[10];
	unsigned	len;
	struct hostif_msg_to_sensor *msg = (struct hostif_msg_to_sensor *)buf;
	int	i;

	len = sizeof(struct hostif_msg_to_sensor);

	ISH_DBG_PRINT(KERN_ALERT
		"[hid-ish]: %s(): writing GET REPORT of type: %d\n", __func__,
		report_type);
	memset(msg, 0, sizeof(struct hostif_msg_to_sensor));
	msg->hdr.command = (report_type == HID_FEATURE_REPORT) ?
		HOSTIF_GET_FEATURE_REPORT : HOSTIF_GET_INPUT_REPORT;
	for (i = 0; i < num_hid_devices; ++i)
		if (hid == hid_sensor_hubs[i]) {
			msg->hdr.device_id = hid_devices[i].dev_id;
			/*
			 * FIXME - temporary when single collection exists,
			 * then has to be part of hid_device custom fields
			 */
			break;
		}
	if (i == num_hid_devices)
		return;

	msg->report_id = report_id;
	rv = heci_cl_send(hid_heci_cl, buf, len);
	ISH_DBG_PRINT(KERN_ALERT
		"[hid-ish]: %s(): heci_cl_send() returned %d\n", __func__, rv);
}


int	hid_heci_cl_probe(struct heci_cl_device *cl_device,
	const struct heci_cl_device_id *id)
{
	int	rv;

	ISH_DBG_PRINT(KERN_ALERT "%s(): +++\n", __func__);
	if (!cl_device)
		return	-ENODEV;

	ISH_DBG_PRINT(KERN_ALERT
		"%s(): dev != NULL && dev->cl != NULL /* OK */\n",
		__func__);
	if (uuid_le_cmp(ish_heci_guid,
			cl_device->fw_client->props.protocol_name) != 0) {
		ISH_DBG_PRINT(KERN_ALERT "%s(): device doesn't match\n",
			__func__);
		return	-ENODEV;
	}

	ISH_DBG_PRINT(KERN_ALERT "%s(): device matches!\n", __func__);
	hid_heci_cl = heci_cl_allocate(cl_device->heci_dev);
	if (!hid_heci_cl)
		return	-ENOMEM;

	rv = heci_cl_link(hid_heci_cl, HECI_HOST_CLIENT_ID_ANY);
	if (rv)
		return	-ENOMEM;

	hid_heci_client_found = 1;
	if (waitqueue_active(&init_wait))
		wake_up(&init_wait);

	ISH_DBG_PRINT(KERN_ALERT "%s(): ---\n", __func__);
	return	0;

	/*
	 * Linux generic drivers framework doesn't like probe() functions
	 * to start kernel threads
	 */
}


int     hid_heci_cl_remove(struct heci_cl_device *dev)
{
	ISH_DBG_PRINT(KERN_ALERT "%s(): +++\n", __func__);
	heci_hid_remove();
	hid_heci_client_found = 0;
	hid_heci_cl = NULL;
	ISH_DBG_PRINT(KERN_ALERT "%s(): ---\n", __func__);
	return  0;
}


struct heci_cl_driver	hid_heci_cl_driver = {
	.name = "ish",
	.probe = hid_heci_cl_probe,
	.remove = hid_heci_cl_remove,
};


/****************************************************************/

struct work_struct my_work;


void workqueue_init_function(struct work_struct *work)
{
	int	rv;
	static unsigned char	buf[4096];
	unsigned	len;
	struct hostif_msg	*msg = (struct hostif_msg *)buf;
	int	i;
	struct heci_device	*dev;
	int	retry_count;

	ISH_DBG_PRINT(KERN_ALERT
		"[ish client driver] %s() in workqueue func, continue initialization process\n",
		__func__);

	if (!hid_heci_client_found)
		wait_event_timeout(init_wait, hid_heci_client_found, 30 * HZ);

	ISH_DBG_PRINT(KERN_ALERT
		"[ish client driver] %s() completed waiting for hid_heci_client_found[=%d]\n",
		__func__, hid_heci_client_found);

	if (!hid_heci_client_found) {
		printk(KERN_ERR "[hid-ish]: timed out waiting for hid_heci_client_found\n");
		rv = -ENODEV;
		goto	ret;
	}

	dev = hid_heci_cl->dev;

	/* Connect to FW client */
	hid_heci_cl->rx_ring_size = RX_RING_SIZE;
	hid_heci_cl->tx_ring_size = TX_RING_SIZE;

	i = heci_me_cl_by_uuid(dev, &ish_heci_guid);
	hid_heci_cl->me_client_id = dev->me_clients[i].client_id;
	hid_heci_cl->state = HECI_CL_CONNECTING;

	rv = heci_cl_connect(hid_heci_cl);
	if (rv)
		goto	ret;

	/* Register read callback */
	heci_register_event_cb(hid_heci_cl->device, ish_cl_event_cb, NULL);

#if 0
	/*
	 * Wait until we can send without risking flow-control break scenario
	 * (sending OUR FC ahead of message, so that FW will respond)
	 * We probably need here only a small delay in order to let our FC
	 * to be sent over to FW
	 */
	schedule_timeout(WAIT_FOR_SEND_SLICE);
#endif

	/* Send HOSTIF_DM_ENUM_DEVICES */
	memset(msg, 0, sizeof(struct hostif_msg));
	msg->hdr.command = HOSTIF_DM_ENUM_DEVICES;
	len = sizeof(struct hostif_msg);
	ISH_DBG_PRINT(KERN_ALERT
		"[ish client driver] %s() writing HOSTIF_DM_ENUM_DEVICES len = %u\n",
		__func__, len);
	rv = heci_cl_send(hid_heci_cl, buf, len);
	ISH_DBG_PRINT(KERN_ALERT
		"[ish client driver] %s() heci_cl_send() returned %d\n",
		__func__, rv);
	if (rv)
		goto	ret;

	rv = 0;

	retry_count = 0;
	printk(KERN_ALERT "[hid-ish]: going to send HOSTIF_DM_ENUM_DEVICES\n");
	while (!enum_devices_done && retry_count < 10) {
		wait_event_timeout(init_wait, enum_devices_done, 3 * HZ);
		++retry_count;
		printk(KERN_ALERT "[hid-ish]: enum_devices_done = %d, retry_count = %d\n",
			enum_devices_done, retry_count);
		if (!enum_devices_done) {
			/* Send HOSTIF_DM_ENUM_DEVICES */
			memset(msg, 0, sizeof(struct hostif_msg));
			msg->hdr.command = HOSTIF_DM_ENUM_DEVICES;
			len = sizeof(struct hostif_msg);
			rv = heci_cl_send(hid_heci_cl, buf, len);
		}
	}
	printk(KERN_ALERT "[hid-ish]: enum_devices_done = %d, retry_count = %d\n",
		enum_devices_done, retry_count);

	if (!enum_devices_done) {
		printk(KERN_ERR "[ish client driver]: timed out waiting for enum_devices_done\n");
		rv = -ETIMEDOUT;
		goto	ret;
	}
	if (!hid_devices) {
		printk(KERN_ERR "[ish client driver]: failed to allocate sensors devices structures\n");
		rv = -ENOMEM;
		goto	ret;
	}

	/* Send GET_HID_DESCRIPTOR for each device */

	/*
	 * Temporary work-around for multi-descriptor traffic:
	 * read only the first one
	 * Will be removed when multi-TLC are supported
	 */

	num_hid_devices = hid_dev_count;
	printk(KERN_ALERT "[hid-ish]: enum_devices_done OK, num_hid_devices=%d\n",
		num_hid_devices);


	for (i = 0; i < num_hid_devices /*hid_dev_count*/; ++i) {
		cur_hid_dev = i;

		/* Get HID descriptor */
		hid_descr_done = 0;
		ISH_DBG_PRINT(KERN_ALERT
			"[hid-ish]: %s(): [%d] writing HOSTIF_GET_HID_DESCRIPTOR\n",
			__func__, i);
		memset(msg, 0, sizeof(struct hostif_msg));
		msg->hdr.command = HOSTIF_GET_HID_DESCRIPTOR;
		msg->hdr.device_id = hid_devices[i].dev_id;
		len = sizeof(struct hostif_msg);
		rv = heci_cl_send(hid_heci_cl, buf, len);
		ISH_DBG_PRINT(KERN_ALERT
			"[hid-ish]: %s(): heci_cl_send() [HOSTIF_GET_HID_DESCRIPTOR] returned %d\n",
			__func__, rv);
		rv = 0;
#ifdef HOST_VIRTUALBOX
		timed_wait_for(WAIT_FOR_SEND_SLICE, hid_descr_done);
#else
		if (!hid_descr_done)
			wait_event_timeout(init_wait, hid_descr_done, 30 * HZ);
#endif
		if (!hid_descr_done) {
			printk(KERN_ERR "[hid-ish]: timed out waiting for hid_descr_done\n");
			continue;
		}

		if (!hid_descr[i]) {
			printk(KERN_ERR "[hid-ish]: failed to allocate HID descriptor buffer\n");
			continue;
		}

		/* Get report descriptor */
		report_descr_done = 0;
		ISH_DBG_PRINT(KERN_ALERT
			"[hid-ish]: %s(): [%d] writing HOSTIF_GET_REPORT_DESCRIPTOR\n",
			__func__, i);
		memset(msg, 0, sizeof(struct hostif_msg));
		msg->hdr.command = HOSTIF_GET_REPORT_DESCRIPTOR;
		msg->hdr.device_id = hid_devices[i].dev_id;
		len = sizeof(struct hostif_msg);
		rv = heci_cl_send(hid_heci_cl, buf, len);

		ISH_DBG_PRINT(KERN_ALERT
			"[hid-ish]: %s(): heci_cl_send() [HOSTIF_GET_REPORT_DESCRIPTOR] returned %d\n",
			__func__, rv);
		rv = 0;
#ifdef HOST_VIRTUALBOX
		timed_wait_for(WAIT_FOR_SEND_SLICE, report_descr_done);
#else
		if (!report_descr_done)
			wait_event_timeout(init_wait, report_descr_done,
				30 * HZ);
#endif
		if (!report_descr_done) {
			printk(KERN_ERR "[hid-ish]: timed out waiting for report_descr_done\n");
			continue;
		}

		if (!report_descr[i]) {
			printk(KERN_ERR "[hid-ish]: failed to allocate report descriptor buffer\n");
			continue;
		}

		rv = heci_hid_probe(i);
		if (rv) {
			printk(KERN_ERR "[hid-ish]: HECI-HID probe for device #%u failed: %d\n",
				i, rv);
			continue;
		}
	} /* for() */

	ISH_DBG_PRINT(KERN_ALERT
		"[hid-ish] %s() in workqueue func, finished initialization process\n",
		__func__);

ret:

	ISH_DBG_PRINT(KERN_ALERT
		"[hid-ish] %s() :in ret label --- returning %d\n", __func__,
		rv);
}
/****************************************************************/

static int __init ish_init(void)
{
	int	rv;
	struct workqueue_struct *workqueue_for_init;

	ISH_INFO_PRINT(KERN_ERR "[hid-ish]: %s():+++ [Build" BUILD_ID "]\n",
		__func__);
	init_waitqueue_head(&init_wait);
	init_waitqueue_head(&heci_hid_wait);

	/* Register HECI client device driver - ISS */
	rv = heci_cl_driver_register(&hid_heci_cl_driver);

	/*
	 * 7/7/2014: in order to not stick Android boot, from here & below
	 * needs to run in work queue and here we should return rv
	 */
	/****************************************************************/
	workqueue_for_init = create_workqueue("workqueue_for_init");
	if (!workqueue_for_init)
		return -ENOMEM;
	INIT_WORK(&my_work, workqueue_init_function);
	queue_work(workqueue_for_init, &my_work);

	ISH_DBG_PRINT(KERN_ALERT
		"[ish client driver] %s() enqueue init_work function\n",
		__func__);

	return rv;
	/****************************************************************/

}


static void __exit ish_exit(void)
{
	ISH_DBG_PRINT(KERN_ALERT "[hid-ish]: %s():+++\n", __func__);
	heci_cl_driver_unregister(&hid_heci_cl_driver);
	ISH_DBG_PRINT(KERN_ALERT
		"[hid-ish]: %s(): unregistered from HECI bus\n", __func__);
	ISH_DBG_PRINT(KERN_ALERT "[hid-ish]: %s():---\n", __func__);
}

module_init(ish_init);
module_exit(ish_exit);

MODULE_DESCRIPTION("ISS HECI client driver");
MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("GPL");

