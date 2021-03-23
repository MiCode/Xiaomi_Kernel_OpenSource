// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Force feedback support for AKSYS QRD USB adapter
 *   Copyright (c) 2020, Daniel@AKsys <daniel@aksys.co.kr>
 */

/* #define DEBUG */

#include <linux/input.h>
#include <linux/slab.h>
#include <linux/hid.h>
#include <linux/module.h>
#include "hid-ids.h"

#ifdef CONFIG_AKSYS_QRD_FF
int initialed = 0;

#define TOUCH_SCREEN 4 // (0x09, 0x04)
#define GAMEPAD 5 // (0x09, 0x05)

#define AKSYS_QRD_USB 0x1000
#define AKSYS_ANDROID_BT 0x0016

struct aksys_qrd_ff_device {
	struct hid_report *report;
};

static int aksys_qrd_play(struct input_dev *dev, void *data,
			 struct ff_effect *effect)
{
	u32 left, right;
	u8 buf[4];
	
	struct hid_device *hid = input_get_drvdata(dev);

	struct aksys_qrd_ff_device *ff = data;
	if(hid == NULL) {
		hid_err(hid, "Can not get hid device ......\n");
		return -1;
	}

	if(ff == NULL) {
		hid_err(hid, "Can not get hid ff data ......\n");
		return -1;
	}

	left = effect->u.rumble.strong_magnitude;
	right = effect->u.rumble.weak_magnitude;
	left = left / 256;
	right = right / 256;
	//hid_info(hid, "called with 0x%08x 0x%08x\n", ff->report->id, left, right);
	if(hid->product == AKSYS_QRD_USB) // #define AKSYS_QRD_USB 0x1000
	{
		ff->report->field[0]->value[0] = 0x64;
		ff->report->field[0]->value[1] = left;
		ff->report->field[0]->value[2] = 0x64;
		ff->report->field[0]->value[3] = right;

		hid_hw_request(hid, ff->report, HID_REQ_SET_REPORT);
	}
	
	else if(hid->product == AKSYS_ANDROID_BT) // #define AKSYS_ANDROID_BT 0x0016
	{
		buf[0] = 0x03; 			// Report ID
		buf[1] = 0x91; 			// Vibration index
		buf[2] = (u8)(left);	// Left Motor Force
		buf[3] = (u8)(right);	// Right Motor Force
		
		hid_hw_output_report(hid, buf, 4);
	}
	return 0;
}

static int aksys_qrd_ff_init(struct hid_device *hid)
{
	struct aksys_qrd_ff_device *ff;
	struct hid_report *report;
	struct hid_input *hidinput;
	struct list_head *report_list =
			&hid->report_enum[HID_OUTPUT_REPORT].report_list;
	struct list_head *report_ptr = report_list;
	struct input_dev *dev;
	int error;

	if (list_empty(report_list)) {
		hid_err(hid, "no output reports found\n");
		return -ENODEV;
	}

	list_for_each_entry(hidinput, &hid->inputs, list) {
		report_ptr = report_ptr->next;

		if (report_ptr == report_list) {
			hid_err(hid, "required output report is missing\n");
			return -ENODEV;
		}

		report = list_entry(report_ptr, struct hid_report, list);

		if (report->maxfield < 1) {
			hid_err(hid, "no fields in the report\n");
			return -ENODEV;
		}

		if (report->field[0]->report_count < 3) {
			hid_err(hid, "not enough values in the field\n");
			return -ENODEV;
		}

		ff = kzalloc(sizeof(struct aksys_qrd_ff_device), GFP_KERNEL);
		if (!ff)
			return -ENOMEM;

		dev = hidinput->input;

		input_set_capability(dev, EV_FF, FF_RUMBLE);

		if(!test_bit(FF_RUMBLE, dev->ffbit)) {
			hid_err(hid, "aksys qrd vibrator FF_RUMBLE not set!");
		}

		error = input_ff_create_memless(dev, ff, aksys_qrd_play);

		if (error) {
			kfree(ff);
			return error;
		}

		ff->report = report;
	}
	hid_info(hid, "Setup force feedback for aksys qrd USB gamepad\n");
	return 0;
}
#else
static inline int aksys_qrd_ff_init(struct hid_device *hid)
{
	hid_info(hid, "aksys_qrd_ff_init nothing");
	return 0;
}
#endif

static int aksys_qrd_input_configured(struct hid_device *hdev,
					struct hid_input *hidinput)
{
	int ret = 0;
	//hid_info(hdev, "Now config the hid device");
	ret = hid_hw_open(hdev);
	if (ret < 0) {
		hid_err(hdev, "hw open failed\n");
		goto err_close;
	}

	aksys_qrd_ff_init(hdev);

	hid_info(hdev, "aksys qrd hid vibrator configured");
	return 0;

err_close:
	hid_hw_close(hdev);
	return ret;
}

//static int aksys_qrd_raw_event(struct hid_device *hdev, struct hid_report *report, u8 *rd, int size)
//{
//}

static int aksys_qrd_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;
	//hid_info(hdev, "Descriptor == 0x%02X", (*(hdev->dev_rdesc)));
	if((*(hdev->dev_rdesc)) != GAMEPAD) {
		hid_info(hdev, "ignore non-gamepad devices");
		return 0;
	} else {
		hid_info(hdev, "aksys qrd vibrator probing");
	}

	hdev->quirks |= id->driver_data;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "aksys qrd parse failed\n");
		goto err;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT & ~HID_CONNECT_FF);
	if (ret) {
		hid_err(hdev, "hw start failed");
		goto err;
	}

	hid_info(hdev, "aksys qrd vibrator probing end.");
	return 0;
err:
	hid_err(hdev, "aksys_qrd_probe failed!\n");
	return ret;
}

static const struct hid_device_id aksys_qrd_devices[] = {
	{ HID_USB_DEVICE(USB_VENDER_ID_QUALCOMM, USB_PRODUCT_ID_AKSYS_HHG)},
	{ HID_USB_DEVICE(USB_VENDER_ID_TEMP_HHG_AKSY, USB_PRODUCT_ID_AKSYS_HHG)},
	{ HID_BLUETOOTH_DEVICE(0x0A12, 0x0016)},
	{ }
};
MODULE_DEVICE_TABLE(hid, aksys_qrd_devices);

static struct hid_driver aksys_qrd_driver = {
	.name = "aksys_qrd_gamepad",
	.id_table = aksys_qrd_devices,
	.input_configured = aksys_qrd_input_configured,
	//.raw_event        = aksys_qrd_raw_event,
	.probe = aksys_qrd_probe,
};

module_hid_driver(aksys_qrd_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel G");