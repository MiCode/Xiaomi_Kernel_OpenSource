/*
 *  MHL3 HID driver for multitouch panels
 *
 *  Copyright (c) 2013-2014 Lee Mulcahy <william.mulcahy@siliconimage.com>
 *  Copyright (c) 2013-2014 Silicon Image, Inc
 *
 *  This code is based on hid-multitouch.c:
 *
 *  Copyright (c) 2010-2011 Stephane Chatty <chatty@enac.fr>
 *  Copyright (c) 2010-2011 Benjamin Tissoires <benjamin.tissoires@gmail.com>
 *  Copyright (c) 2010-2011 Ecole Nationale de l'Aviation Civile, France
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/hidraw.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input/mt.h>

#include "hid-ids.h"

#include "si_emsc_hid.h"

#ifndef PCI_VENDOR_ID_SILICONIMAGE
#define PCI_VENDOR_ID_SILICONIMAGE		0x1095
#define USB_VENDOR_ID_SILICONIMAGE		0x1A4A
#define MHL_PRODUCT_ID_SILICONIMAGE_9394	0x9394
#define MHL_PRODUCT_ID_SILICONIMAGE_9679	0x9679
#define MHL_PRODUCT_ID_SILICONIMAGE_TEST	0x0000
#endif



#ifndef USB_VENDOR_ID_ATMEL
#define USB_VENDOR_ID_ATMEL		0x03EB
#endif
#ifndef USB_DEVICE_ID_ATMEL_MXT_384E
#define USB_DEVICE_ID_ATMEL_MXT_384E	0x2128
#endif
#ifndef USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH
#define USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH 1
#endif

#ifndef USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH1
#define USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH1 1
#endif
#ifndef USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH2
#define USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH2 2
#endif
#ifndef USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH3
#define USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH3 3
#endif
#ifndef USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH4
#define USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH4 4
#endif

#ifndef USB_VENDOR_ID_ATMEL
#define USB_VENDOR_ID_ATMEL		0x03EB
#endif
#ifndef USB_DEVICE_ID_ATMEL_MXT_384E
#define USB_DEVICE_ID_ATMEL_MXT_384E	0x2128
#endif

/* quirks to control the device */
#define MT_QUIRK_NOT_SEEN_MEANS_UP	(1 << 0)
#define MT_QUIRK_SLOT_IS_CONTACTID	(1 << 1)
#define MT_QUIRK_CYPRESS		(1 << 2)
#define MT_QUIRK_SLOT_IS_CONTACTNUMBER	(1 << 3)
#define MT_QUIRK_ALWAYS_VALID		(1 << 4)
#define MT_QUIRK_VALID_IS_INRANGE	(1 << 5)
#define MT_QUIRK_VALID_IS_CONFIDENCE	(1 << 6)
#define MT_QUIRK_EGALAX_XYZ_FIXUP	(1 << 7)
#define MT_QUIRK_SLOT_IS_CONTACTID_MINUS_ONE	(1 << 8)
#if (LINUX_KERNEL_VER >= 311)
#define MT_QUIRK_NO_AREA		(1 << 9)
#define MT_QUIRK_IGNORE_DUPLICATES	(1 << 10)
#define MT_QUIRK_HOVERING		(1 << 11)
#define MT_QUIRK_CONTACT_CNT_ACCURATE	(1 << 12)
#endif
/* SIMG GCS - 130603 - bz33633 */
#define MT_QUIRK_SIMG_FIRST		13
#define MT_QUIRK_SWAP_LEFTRIGHT		(1 << (MT_QUIRK_SIMG_FIRST+0))
#define MT_QUIRK_SWAP_UPDOWN		(1 << (MT_QUIRK_SIMG_FIRST+1))
#define MT_QUIRK_SWAP_XY		(1 << (MT_QUIRK_SIMG_FIRST+2))
#define MT_QUIRK_SWAP_WH		(1 << (MT_QUIRK_SIMG_FIRST+3))
struct mt_slot {
#if (LINUX_KERNEL_VER >= 311)
	__s32 x, y, cx, cy, p, w, h;
#else
	__s32 x, y, p, w, h;
#endif

	__s32 contactid;	/* the device ContactID assigned to this slot */
	bool touch_state;	/* is the touch valid? */
#if (LINUX_KERNEL_VER >= 311)
	bool inrange_state;	/* is the finger in proximity of the sensor? */
#else
	bool seen_in_this_frame;/* has this slot been updated */
#endif
};

struct mt_class {
	__s32 name;	/* MT_CLS */
	__s32 quirks;
	__s32 sn_move;	/* Signal/noise ratio for move events */
	__s32 sn_width;	/* Signal/noise ratio for width events */
	__s32 sn_height;	/* Signal/noise ratio for height events */
	__s32 sn_pressure;	/* Signal/noise ratio for pressure events */
	__u8 maxcontacts;
#if (LINUX_KERNEL_VER >= 311)
	bool is_indirect;	/* true for touchpads */
#endif
};

#if (LINUX_KERNEL_VER >= 311)
struct mt_device {
	struct mt_slot curdata;	/* placeholder of incoming data */
	struct mt_class mtclass;	/* our mt device class */
	struct mt_fields *fields;	/* temporary placeholder for storing the
					   multitouch fields */
	int cc_index;	/* contact count field index in the report */
	int cc_value_index;	/* contact count value index in the field */
	unsigned last_slot_field;	/* the last field of a slot */
	unsigned mt_report_id;	/* the report ID of the multitouch device */
	unsigned pen_report_id;	/* the report ID of the pen device */
	__s16 inputmode;	/* InputMode HID feature, -1 if non-existent */
	__s16 inputmode_index;	/* InputMode HID feature index in the report */
	__s16 maxcontact_report_id;	/* Maximum Contact Number HID feature,
				   -1 if non-existent */
	__u8 num_received;	/* how many contacts we received */
	__u8 num_expected;	/* expected last contact index */
	__u8 maxcontacts;
	__u8 touches_by_report;	/* how many touches are present in one report:
				* 1 means we should use a serial protocol
				* > 1 means hybrid (multitouch) protocol */
	bool serial_maybe;	/* need to check for serial protocol */
	bool curvalid;		/* is the current contact valid? */
	unsigned mt_flags;	/* flags to pass to input-mt */
};

struct mt_fields {
	unsigned usages[HID_MAX_FIELDS];
	unsigned int length;
};

static void mt_post_parse_default_settings(struct mt_device *td);
static void mt_post_parse(struct mt_device *td);


#else
struct mt_device {
	struct mt_slot curdata;	/* placeholder of incoming data */
	struct mt_class *mtclass;	/* our mt device class */
	unsigned last_field_index;	/* last field index of the report */
	unsigned last_slot_field;	/* the last field of a slot */
	int last_mt_collection;	/* last known mt-related collection */
	__s8 inputmode;		/* InputMode HID feature, -1 if non-existent */
	__u8 num_received;	/* how many contacts we received */
	__u8 num_expected;	/* expected last contact index */
	__u8 maxcontacts;
	bool curvalid;		/* is the current contact valid? */
	struct mt_slot *slots;
};
#endif

/* classes of device behavior */
#define MT_CLS_DEFAULT				0x0001

#define MT_CLS_SERIAL				0x0002
#define MT_CLS_CONFIDENCE			0x0003

#if (LINUX_KERNEL_VER >= 311)
#define MT_CLS_CONFIDENCE_CONTACT_ID		0x0004
#define MT_CLS_CONFIDENCE_MINUS_ONE		0x0005
#define MT_CLS_DUAL_INRANGE_CONTACTID		0x0006
#define MT_CLS_DUAL_INRANGE_CONTACTNUMBER	0x0007

#define MT_CLS_DUAL_NSMU_CONTACTID		0x0008
#define MT_CLS_INRANGE_CONTACTNUMBER		0x0009
#define MT_CLS_NSMU				0x000a
#define MT_CLS_DUAL_CONTACT_NUMBER		0x0010
#define MT_CLS_DUAL_CONTACT_ID			0x0011

#else
#define MT_CLS_CONFIDENCE_MINUS_ONE		0x0004
#define MT_CLS_DUAL_INRANGE_CONTACTID		0x0005
#define MT_CLS_DUAL_INRANGE_CONTACTNUMBER	0x0006
#define MT_CLS_DUAL_NSMU_CONTACTID		0x0007
#endif

/* vendor specific classes */
#define MT_CLS_3M				0x0101
#define MT_CLS_CYPRESS				0x0102
#define MT_CLS_EGALAX				0x0103
#if (LINUX_KERNEL_VER >= 311)
#define MT_CLS_EGALAX_SERIAL			0x0104
#define MT_CLS_TOPSEED				0x0105
#define MT_CLS_PANASONIC			0x0106
#define MT_CLS_FLATFROG				0x0107
#define MT_CLS_GENERALTOUCH_TWOFINGERS		0x0108
#define MT_CLS_GENERALTOUCH_PWT_TENFINGERS	0x0109
#endif
/* SIMG 140714 - BZ33633 */
#define MT_CLS_OCULAR				0x010A

#define MT_DEFAULT_MAXCONTACT	10

#define MT_DEFAULT_MAXCONTACT	10
#define MT_MAX_MAXCONTACT	250
#if (LINUX_KERNEL_VER >= 311)
#define MT_USB_DEVICE(v, p)	HID_DEVICE(BUS_USB, HID_GROUP_MULTITOUCH, v, p)
#define MT_BT_DEVICE(v, p)	HID_DEVICE(BUS_BLUETOOTH, \
					HID_GROUP_MULTITOUCH, v, p)
#else
#define MT_USB_DEVICE(v, p)	HID_USB_DEVICE(v, p)
#endif

/*
 * These device-dependent functions determine what slot corresponds
 * to a valid contact that was just read.
 */

static int cypress_compute_slot(struct mt_device *td)
{
	if (td->curdata.contactid != 0 || td->num_received == 0)
		return td->curdata.contactid;
	else
		return -1;
}

#if (LINUX_KERNEL_VER < 309)
static int find_slot_from_contactid(struct mt_device *td)
{
	int i;
	for (i = 0; i < td->maxcontacts; ++i) {
		if (td->slots[i].contactid == td->curdata.contactid &&
			td->slots[i].touch_state)
			return i;
	}
	for (i = 0; i < td->maxcontacts; ++i) {
		if (!td->slots[i].seen_in_this_frame &&
			!td->slots[i].touch_state)
			return i;
	}
	/* should not occurs. If this happens that means
	 * that the device sent more touches that it says
	 * in the report descriptor. It is ignored then. */
	return -1;
}
#endif


#if (LINUX_KERNEL_VER >= 311)
struct mt_class mhl3_mt_classes[] = {
	{ .name = MT_CLS_DEFAULT,
		.quirks = MT_QUIRK_ALWAYS_VALID |
			MT_QUIRK_CONTACT_CNT_ACCURATE },
	{ .name = MT_CLS_NSMU,
		.quirks = MT_QUIRK_NOT_SEEN_MEANS_UP },
	{ .name = MT_CLS_SERIAL,
		.quirks = MT_QUIRK_ALWAYS_VALID},
	{ .name = MT_CLS_CONFIDENCE,
		.quirks = MT_QUIRK_VALID_IS_CONFIDENCE },
	{ .name = MT_CLS_CONFIDENCE_CONTACT_ID,
		.quirks = MT_QUIRK_VALID_IS_CONFIDENCE |
			MT_QUIRK_SLOT_IS_CONTACTID },
	{ .name = MT_CLS_CONFIDENCE_MINUS_ONE,
		.quirks = MT_QUIRK_VALID_IS_CONFIDENCE |
			MT_QUIRK_SLOT_IS_CONTACTID_MINUS_ONE },
	{ .name = MT_CLS_DUAL_INRANGE_CONTACTID,
		.quirks = MT_QUIRK_VALID_IS_INRANGE |
			MT_QUIRK_SLOT_IS_CONTACTID,
		.maxcontacts = 2 },
	{ .name = MT_CLS_DUAL_INRANGE_CONTACTNUMBER,
		.quirks = MT_QUIRK_VALID_IS_INRANGE |
			MT_QUIRK_SLOT_IS_CONTACTNUMBER,
		.maxcontacts = 2 },
	{ .name = MT_CLS_DUAL_NSMU_CONTACTID,
		.quirks = MT_QUIRK_NOT_SEEN_MEANS_UP |
			MT_QUIRK_SLOT_IS_CONTACTID,
		.maxcontacts = 2 },
	{ .name = MT_CLS_INRANGE_CONTACTNUMBER,
		.quirks = MT_QUIRK_VALID_IS_INRANGE |
			MT_QUIRK_SLOT_IS_CONTACTNUMBER },
	{ .name = MT_CLS_DUAL_CONTACT_NUMBER,
		.quirks = MT_QUIRK_ALWAYS_VALID |
			MT_QUIRK_CONTACT_CNT_ACCURATE |
			MT_QUIRK_SLOT_IS_CONTACTNUMBER,
		.maxcontacts = 2 },
	{ .name = MT_CLS_DUAL_CONTACT_ID,
		.quirks = MT_QUIRK_ALWAYS_VALID |
			MT_QUIRK_CONTACT_CNT_ACCURATE |
			MT_QUIRK_SLOT_IS_CONTACTID,
		.maxcontacts = 2 },

	/*
	 * vendor specific classes
	 */
	{ .name = MT_CLS_3M,
		.quirks = MT_QUIRK_VALID_IS_CONFIDENCE |
			MT_QUIRK_SLOT_IS_CONTACTID,
		.sn_move = 2048,
		.sn_width = 128,
		.sn_height = 128,
		.maxcontacts = 60,
	},
	{ .name = MT_CLS_CYPRESS,
		.quirks = MT_QUIRK_NOT_SEEN_MEANS_UP |
			MT_QUIRK_CYPRESS,
		.maxcontacts = 10 },
	{ .name = MT_CLS_EGALAX,
		.quirks =  MT_QUIRK_SLOT_IS_CONTACTID |
			MT_QUIRK_VALID_IS_INRANGE,
		.sn_move = 4096,
		.sn_pressure = 32,
	},
	{ .name = MT_CLS_OCULAR,
		.quirks =  MT_QUIRK_ALWAYS_VALID |
			MT_QUIRK_SWAP_UPDOWN |
			MT_QUIRK_SWAP_LEFTRIGHT,
		.maxcontacts = 8,
	},
	{ .name = MT_CLS_EGALAX_SERIAL,
		.quirks =  MT_QUIRK_SLOT_IS_CONTACTID |
			MT_QUIRK_ALWAYS_VALID,
		.sn_move = 4096,
		.sn_pressure = 32,
	},
	{ .name = MT_CLS_TOPSEED,
		.quirks = MT_QUIRK_ALWAYS_VALID,
		.is_indirect = true,
		.maxcontacts = 2,
	},
	{ .name = MT_CLS_PANASONIC,
		.quirks = MT_QUIRK_NOT_SEEN_MEANS_UP,
		.maxcontacts = 4 },
	{ .name	= MT_CLS_GENERALTOUCH_TWOFINGERS,
		.quirks	= MT_QUIRK_NOT_SEEN_MEANS_UP |
			MT_QUIRK_VALID_IS_INRANGE |
			MT_QUIRK_SLOT_IS_CONTACTNUMBER,
		.maxcontacts = 2
	},
	{ .name	= MT_CLS_GENERALTOUCH_PWT_TENFINGERS,
		.quirks	= MT_QUIRK_NOT_SEEN_MEANS_UP |
			MT_QUIRK_SLOT_IS_CONTACTNUMBER
	},

	{ .name = MT_CLS_FLATFROG,
		.quirks = MT_QUIRK_NOT_SEEN_MEANS_UP |
			MT_QUIRK_NO_AREA,
		.sn_move = 2048,
		.maxcontacts = 40,
	},
	{ }
};
#else
struct mt_class mhl3_mt_classes[] = {
	{ .name = MT_CLS_DEFAULT,
		.quirks = MT_QUIRK_NOT_SEEN_MEANS_UP },
	{ .name = MT_CLS_SERIAL,
		.quirks = MT_QUIRK_ALWAYS_VALID},
	{ .name = MT_CLS_CONFIDENCE,
		.quirks = MT_QUIRK_VALID_IS_CONFIDENCE },
	{ .name = MT_CLS_CONFIDENCE_MINUS_ONE,
		.quirks = MT_QUIRK_VALID_IS_CONFIDENCE |
			MT_QUIRK_SLOT_IS_CONTACTID_MINUS_ONE },
	{ .name = MT_CLS_DUAL_INRANGE_CONTACTID,
		.quirks = MT_QUIRK_VALID_IS_INRANGE |
			MT_QUIRK_SLOT_IS_CONTACTID,
		.maxcontacts = 2 },
	{ .name = MT_CLS_DUAL_INRANGE_CONTACTNUMBER,
		.quirks = MT_QUIRK_VALID_IS_INRANGE |
			MT_QUIRK_SLOT_IS_CONTACTNUMBER,
		.maxcontacts = 2 },
	{ .name = MT_CLS_DUAL_NSMU_CONTACTID,
		.quirks = MT_QUIRK_NOT_SEEN_MEANS_UP |
			MT_QUIRK_SLOT_IS_CONTACTID,
		.maxcontacts = 2 },

	/*
	 * vendor specific classes
	 */
	{ .name = MT_CLS_3M,
		.quirks = MT_QUIRK_VALID_IS_CONFIDENCE |
			MT_QUIRK_SLOT_IS_CONTACTID,
		.sn_move = 2048,
		.sn_width = 128,
		.sn_height = 128 },
	{ .name = MT_CLS_CYPRESS,
		.quirks = MT_QUIRK_NOT_SEEN_MEANS_UP |
			MT_QUIRK_CYPRESS,
		.maxcontacts = 10 },
	{ .name = MT_CLS_EGALAX,
		.quirks =  MT_QUIRK_SLOT_IS_CONTACTID |
			MT_QUIRK_VALID_IS_INRANGE |
			MT_QUIRK_EGALAX_XYZ_FIXUP,
		.maxcontacts = 2,
		.sn_move = 4096,
		.sn_pressure = 32,
	},
	{ .name = MT_CLS_OCULAR,
		.quirks =  MT_QUIRK_ALWAYS_VALID |
			MT_QUIRK_SWAP_UPDOWN |
			MT_QUIRK_SWAP_LEFTRIGHT,
		.maxcontacts = 8,
	},
	{ }
};
#endif

#if (LINUX_KERNEL_VER >= 311)
static void mt_free_input_name(struct hid_input *hi)
{
	struct hid_device *hdev = hi->report->device;
	const char *name = hi->input->name;

	if (name != hdev->name) {
		hi->input->name = hdev->name;
		kfree(name);
	}
}
#endif

void mt_feature_mapping(struct hid_device *hdev,
		struct hid_field *field, struct hid_usage *usage)
{
	struct mt_device *td = hid_get_drvdata(hdev);

	switch (usage->hid) {
	case HID_DG_INPUTMODE:
#if (LINUX_KERNEL_VER >= 311)
		/* Ignore if value index is out of bounds. */
		if (usage->usage_index >= field->report_count) {
			dev_err(&hdev->dev, "HID_DG_INPUTMODE out of range\n");
			break;
		}

		td->inputmode = field->report->id;
		td->inputmode_index = usage->usage_index;
#else
		td->inputmode = field->report->id;
#endif
		break;
	case HID_DG_CONTACTMAX:
#if (LINUX_KERNEL_VER >= 311)
		td->maxcontact_report_id = field->report->id;
		td->maxcontacts = field->value[0];
		if (!td->maxcontacts &&
		    field->logical_maximum <= MT_MAX_MAXCONTACT)
			td->maxcontacts = field->logical_maximum;
		/* check if the maxcontacts is given by the class */
		if (td->mtclass.maxcontacts)
			td->maxcontacts = td->mtclass.maxcontacts;
#else
		td->maxcontacts = field->value[0];
		/* check if the maxcontacts is given by the class */
		if (td->mtclass->maxcontacts)
			td->maxcontacts = td->mtclass->maxcontacts;
#endif

		break;
#if (LINUX_KERNEL_VER >= 311)
	case 0xff0000c5:
		if (field->report_count == 256 && field->report_size == 8) {
			/* Win 8 devices need special quirks */
			__s32 *quirks = &td->mtclass.quirks;
			*quirks |= MT_QUIRK_ALWAYS_VALID;
			*quirks |= MT_QUIRK_IGNORE_DUPLICATES;
			*quirks |= MT_QUIRK_HOVERING;
			*quirks |= MT_QUIRK_CONTACT_CNT_ACCURATE;
			*quirks &= ~MT_QUIRK_NOT_SEEN_MEANS_UP;
			*quirks &= ~MT_QUIRK_VALID_IS_INRANGE;
			*quirks &= ~MT_QUIRK_VALID_IS_CONFIDENCE;
		}
#endif
	}
}

static void set_abs(struct input_dev *input, unsigned int code,
		struct hid_field *field, int snratio)
{
	int fmin = field->logical_minimum;
	int fmax = field->logical_maximum;
	int fuzz = snratio ? (fmax - fmin) / snratio : 0;
	input_set_abs_params(input, code, fmin, fmax, fuzz, 0);
#if (LINUX_KERNEL_VER >= 311)
	input_abs_set_res(input, code, hidinput_calc_abs_res(field, code));
#endif
}

#if (LINUX_KERNEL_VER >= 311)
static void mt_store_field(struct hid_usage *usage, struct mt_device *td,
		struct hid_input *hi)
{
	struct mt_fields *f = td->fields;

	if (f->length >= HID_MAX_FIELDS)
		return;

	f->usages[f->length++] = usage->hid;
}
#endif

/*
 * In 3.11, this function is called mt_touch_input_mapping
 */
int mhl3_mt_input_mapping(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
#if (LINUX_KERNEL_VER >= 311)
{
	struct mt_device *td = hid_get_drvdata(hdev);
	struct mt_class *cls = &td->mtclass;
	int code;
	struct hid_usage *prev_usage = NULL;

	if (field->application == HID_DG_TOUCHSCREEN)
		td->mt_flags |= INPUT_MT_DIRECT;

	/*
	 * Model touchscreens providing buttons as touchpads.
	 */
	if (field->application == HID_DG_TOUCHPAD ||
		(usage->hid & HID_USAGE_PAGE) == HID_UP_BUTTON)
		td->mt_flags |= INPUT_MT_POINTER;

	if (usage->usage_index)
		prev_usage = &field->usage[usage->usage_index - 1];

	switch (usage->hid & HID_USAGE_PAGE) {

	case HID_UP_GENDESK:
		switch (usage->hid) {
		case HID_GD_X:
			if (prev_usage && (prev_usage->hid == usage->hid)) {
				hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_TOOL_X);
				set_abs(hi->input, ABS_MT_TOOL_X, field,
					cls->sn_move);
			} else {
				hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_POSITION_X);
				set_abs(hi->input, ABS_MT_POSITION_X, field,
					cls->sn_move);
			}

			mt_store_field(usage, td, hi);
			return 1;
		case HID_GD_Y:
			if (prev_usage && (prev_usage->hid == usage->hid)) {
				hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_TOOL_Y);
				set_abs(hi->input, ABS_MT_TOOL_Y, field,
					cls->sn_move);
			} else {
				hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_POSITION_Y);
				set_abs(hi->input, ABS_MT_POSITION_Y, field,
					cls->sn_move);
			}

			mt_store_field(usage, td, hi);
			return 1;
		}
		return 0;

	case HID_UP_DIGITIZER:
		switch (usage->hid) {
		case HID_DG_INRANGE:
			if (cls->quirks & MT_QUIRK_HOVERING) {
				hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_DISTANCE);
				input_set_abs_params(hi->input,
					ABS_MT_DISTANCE, 0, 1, 0, 0);
			}
			mt_store_field(usage, td, hi);
			return 1;
		case HID_DG_CONFIDENCE:
			mt_store_field(usage, td, hi);
			return 1;
		case HID_DG_TIPSWITCH:
			hid_map_usage(hi, usage, bit, max, EV_KEY, BTN_TOUCH);
			input_set_capability(hi->input, EV_KEY, BTN_TOUCH);
			mt_store_field(usage, td, hi);
			return 1;
		case HID_DG_CONTACTID:
			mt_store_field(usage, td, hi);
			td->touches_by_report++;
			td->mt_report_id = field->report->id;
			return 1;
		case HID_DG_WIDTH:
			hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_TOUCH_MAJOR);
			if (!(cls->quirks & MT_QUIRK_NO_AREA))
				set_abs(hi->input, ABS_MT_TOUCH_MAJOR, field,
					cls->sn_width);
			mt_store_field(usage, td, hi);
			return 1;
		case HID_DG_HEIGHT:
			hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_TOUCH_MINOR);
			if (!(cls->quirks & MT_QUIRK_NO_AREA)) {
				set_abs(hi->input, ABS_MT_TOUCH_MINOR, field,
					cls->sn_height);
				input_set_abs_params(hi->input,
					ABS_MT_ORIENTATION, 0, 1, 0, 0);
			}
			mt_store_field(usage, td, hi);
			return 1;
		case HID_DG_TIPPRESSURE:
			hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_PRESSURE);
			set_abs(hi->input, ABS_MT_PRESSURE, field,
				cls->sn_pressure);
			mt_store_field(usage, td, hi);
			return 1;
		case HID_DG_CONTACTCOUNT:
			/* Ignore if indexes are out of bounds. */
			if (field->index >= field->report->maxfield ||
				usage->usage_index >= field->report_count)
				return 1;
			td->cc_index = field->index;
			td->cc_value_index = usage->usage_index;
			return 1;
		case HID_DG_CONTACTMAX:
			/* we don't set td->last_slot_field as contactcount and
			 * contact max are global to the report */
			return -1;
		case HID_DG_TOUCH:
			/* Legacy devices use TIPSWITCH and not TOUCH.
			 * Let's just ignore this field. */
			return -1;
		}
		/* let hid-input decide for the others */
		return 0;

	case HID_UP_BUTTON:
		code = BTN_MOUSE + ((usage->hid - 1) & HID_USAGE);
		hid_map_usage(hi, usage, bit, max, EV_KEY, code);
		input_set_capability(hi->input, EV_KEY, code);
		return 1;

	case 0xff000000:
		/* we do not want to map these: no input-oriented meaning */
		return -1;
	}

	return 0;
}
#else
{
	struct mt_device *td = hid_get_drvdata(hdev);
	struct mt_class *cls = td->mtclass;

#ifdef GCS_QUIRKS_FOR_SIMG
	__s32 quirks = cls->quirks;
#endif

#if (LINUX_KERNEL_VER >= 311)
	/* Only map fields from TouchScreen or TouchPad collections.
	* We need to ignore fields that belong to other collections
	* such as Mouse that might have the same GenericDesktop usages. */
	if (field->application != HID_DG_TOUCHSCREEN &&
		field->application != HID_DG_TOUCHPAD)
		return -1;
#else
	/* Only map fields from TouchScreen or TouchPad collections.
	 * We need to ignore fields that belong to other collections
	 * such as Mouse that might have the same GenericDesktop usages. */
	if (field->application == HID_DG_TOUCHSCREEN)
		set_bit(INPUT_PROP_DIRECT, hi->input->propbit);
	else if (field->application == HID_DG_TOUCHPAD)
		set_bit(INPUT_PROP_POINTER, hi->input->propbit);
	else
		return 0;
#endif

	switch (usage->hid & HID_USAGE_PAGE) {

	case HID_UP_GENDESK:
		switch (usage->hid) {
		case HID_GD_X:
#ifdef GCS_QUIRKS_FOR_SIMG
			if (quirks & MT_QUIRK_EGALAX_XYZ_FIXUP)
				field->logical_maximum = 32760;
#endif
			hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_POSITION_X);
			set_abs(hi->input, ABS_MT_POSITION_X, field,
				cls->sn_move);
			/* touchscreen emulation */
			set_abs(hi->input, ABS_X, field, cls->sn_move);
			if (td->last_mt_collection == usage->collection_index) {
				td->last_slot_field = usage->hid;
				td->last_field_index = field->index;
			}
			return 1;
		case HID_GD_Y:
#ifdef GCS_QUIRKS_FOR_SIMG
			if (quirks & MT_QUIRK_EGALAX_XYZ_FIXUP)
				field->logical_maximum = 32760;
#endif
			hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_POSITION_Y);
			set_abs(hi->input, ABS_MT_POSITION_Y, field,
				cls->sn_move);
			/* touchscreen emulation */
			set_abs(hi->input, ABS_Y, field, cls->sn_move);
			if (td->last_mt_collection == usage->collection_index) {
				td->last_slot_field = usage->hid;
				td->last_field_index = field->index;
			}
			return 1;
		}
		return 0;

	case HID_UP_DIGITIZER:
		switch (usage->hid) {
		case HID_DG_INRANGE:
			if (td->last_mt_collection == usage->collection_index) {
				td->last_slot_field = usage->hid;
				td->last_field_index = field->index;
			}
			return 1;
		case HID_DG_CONFIDENCE:
			if (td->last_mt_collection == usage->collection_index) {
				td->last_slot_field = usage->hid;
				td->last_field_index = field->index;
			}
			return 1;
		case HID_DG_TIPSWITCH:
			hid_map_usage(hi, usage, bit, max, EV_KEY, BTN_TOUCH);
			input_set_capability(hi->input, EV_KEY, BTN_TOUCH);
			if (td->last_mt_collection == usage->collection_index) {
				td->last_slot_field = usage->hid;
				td->last_field_index = field->index;
			}
			return 1;
		case HID_DG_CONTACTID:
			if (!td->maxcontacts)
				td->maxcontacts = MT_DEFAULT_MAXCONTACT;
#if (LINUX_KERNEL_VER >= 311)
			input_mt_init_slots(hi->input, td->maxcontacts, 0);
#else
			input_mt_init_slots(hi->input, td->maxcontacts);
#endif
			td->last_slot_field = usage->hid;
			td->last_field_index = field->index;
			td->last_mt_collection = usage->collection_index;
			return 1;
		case HID_DG_WIDTH:
			hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_TOUCH_MAJOR);
			set_abs(hi->input, ABS_MT_TOUCH_MAJOR, field,
				cls->sn_width);
			if (td->last_mt_collection == usage->collection_index) {
				td->last_slot_field = usage->hid;
				td->last_field_index = field->index;
			}
			return 1;
		case HID_DG_HEIGHT:
			hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_TOUCH_MINOR);
			set_abs(hi->input, ABS_MT_TOUCH_MINOR, field,
				cls->sn_height);
			input_set_abs_params(hi->input,
					ABS_MT_ORIENTATION, 0, 1, 0, 0);
			if (td->last_mt_collection == usage->collection_index) {
				td->last_slot_field = usage->hid;
				td->last_field_index = field->index;
			}
			return 1;
		case HID_DG_TIPPRESSURE:
			if (quirks & MT_QUIRK_EGALAX_XYZ_FIXUP)
				field->logical_minimum = 0;
			hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_PRESSURE);
			set_abs(hi->input, ABS_MT_PRESSURE, field,
				cls->sn_pressure);
			/* touchscreen emulation */
			set_abs(hi->input, ABS_PRESSURE, field,
				cls->sn_pressure);
			if (td->last_mt_collection == usage->collection_index) {
				td->last_slot_field = usage->hid;
				td->last_field_index = field->index;
			}
			return 1;
		case HID_DG_CONTACTCOUNT:
			if (td->last_mt_collection == usage->collection_index)
				td->last_field_index = field->index;
			return 1;
		case HID_DG_CONTACTMAX:
			/* we don't set td->last_slot_field as contactcount and
			 * contact max are global to the report */
			if (td->last_mt_collection == usage->collection_index)
				td->last_field_index = field->index;
			return -1;
		}
		/* let hid-input decide for the others */
		return 0;

	case 0xff000000:
		/* we do not want to map these: no input-oriented meaning */
		return -1;
	}

	return 0;
}
#endif

int mhl3_mt_input_mapped(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
#if (LINUX_KERNEL_VER >= 311)
	if (field->physical == HID_DG_STYLUS)
		return 0;
#endif

	if (usage->type == EV_KEY || usage->type == EV_ABS)
		set_bit(usage->type, hi->input->evbit);

	return -1;
}

#if (LINUX_KERNEL_VER >= 311)
static int mt_compute_slot(struct mt_device *td, struct input_dev *input)
#else
static int mt_compute_slot(struct mt_device *td)
#endif
{
#if (LINUX_KERNEL_VER >= 311)
	__s32 quirks = td->mtclass.quirks;
#else
	__s32 quirks = td->mtclass->quirks;
#endif

	if (quirks & MT_QUIRK_SLOT_IS_CONTACTID)
		return td->curdata.contactid;

	if (quirks & MT_QUIRK_CYPRESS)
		return cypress_compute_slot(td);

	if (quirks & MT_QUIRK_SLOT_IS_CONTACTNUMBER)
		return td->num_received;

	if (quirks & MT_QUIRK_SLOT_IS_CONTACTID_MINUS_ONE)
		return td->curdata.contactid - 1;

#if (LINUX_KERNEL_VER >= 307)
	return input_mt_get_slot_by_key(input, td->curdata.contactid);
#else
	return find_slot_from_contactid(td);
#endif
}

/*
 * this function is called when a whole contact has been processed,
 * so that it can assign it to a slot and store the data there
 */
#if (LINUX_KERNEL_VER >= 311)
static void mt_complete_slot(struct mt_device *td, struct input_dev *input)
{
	if ((td->mtclass.quirks & MT_QUIRK_CONTACT_CNT_ACCURATE) &&
		td->num_received >= td->num_expected)
		return;

	if (td->curvalid || (td->mtclass.quirks & MT_QUIRK_ALWAYS_VALID)) {
		int slotnum = mt_compute_slot(td, input);
		struct mt_slot *s = &td->curdata;
		struct input_mt *mt = input->mt;

		if (slotnum < 0 || slotnum >= td->maxcontacts)
			return;

		if ((td->mtclass.quirks & MT_QUIRK_IGNORE_DUPLICATES) && mt) {
			struct input_mt_slot *slot = &mt->slots[slotnum];
			if (input_mt_is_active(slot) &&
				input_mt_is_used(mt, slot))
				return;
		}

		input_mt_slot(input, slotnum);
		input_mt_report_slot_state(input, MT_TOOL_FINGER,
			s->touch_state || s->inrange_state);
		if (s->touch_state || s->inrange_state) {

			/* SIMG GCS - 130603 - bz33633 */
			/* this finger is on the screen */
			int wide, major, minor;
			if (td->mtclass.quirks & MT_QUIRK_SWAP_WH) {
				wide = (s->h > s->w);
				/*
				 * Divided by two to match visual scale
				 * of touch
				 */
				major = min(s->w, s->h) >> 1;
				minor = max(s->w, s->h) >> 1;
			} else {
				wide = (s->w > s->h);
				/*
				 * Divided by two to match visual scale
				 * of touch
				 */
				major = max(s->w, s->h) >> 1;
				minor = min(s->w, s->h) >> 1;
			}

			if (td->mtclass.quirks & MT_QUIRK_SWAP_XY) {
				input_event(input, EV_ABS, ABS_MT_POSITION_X,
					s->y);
				input_event(input, EV_ABS, ABS_MT_POSITION_Y,
					s->x);
				input_event(input, EV_ABS, ABS_MT_TOOL_X,
					s->cy);
				input_event(input, EV_ABS, ABS_MT_TOOL_Y,
					s->cx);
			} else {
				input_event(input, EV_ABS, ABS_MT_POSITION_X,
					s->x);
				input_event(input, EV_ABS, ABS_MT_POSITION_Y,
					s->y);
				input_event(input, EV_ABS, ABS_MT_TOOL_X,
					s->cx);
				input_event(input, EV_ABS, ABS_MT_TOOL_Y,
					s->cy);

			}
			input_event(input, EV_ABS, ABS_MT_DISTANCE,
				!s->touch_state);
			input_event(input, EV_ABS, ABS_MT_ORIENTATION, wide);
			input_event(input, EV_ABS, ABS_MT_PRESSURE, s->p);
			input_event(input, EV_ABS, ABS_MT_TOUCH_MAJOR, major);
			input_event(input, EV_ABS, ABS_MT_TOUCH_MINOR, minor);
		}
	}

	td->num_received++;
}
/*
 * this function is called when a whole packet has been received and processed,
 * so that it can decide what to send to the input layer.
 */
static void mt_sync_frame(struct mt_device *td, struct input_dev *input)
{
	input_mt_sync_frame(input);
	input_sync(input);
	td->num_received = 0;
}
#else

static void mt_complete_slot(struct mt_device *td)
{
	td->curdata.seen_in_this_frame = true;
	if (td->curvalid) {
		int slotnum = mt_compute_slot(td);

		if (slotnum >= 0 && slotnum < td->maxcontacts)
			td->slots[slotnum] = td->curdata;
	}
	td->num_received++;
}

/*
 * this function is called when a whole packet has been received and processed,
 * so that it can decide what to send to the input layer.
 */
static void mt_emit_event(struct mt_device *td, struct input_dev *input)
{
	int i;

	for (i = 0; i < td->maxcontacts; ++i) {
		struct mt_slot *s = &(td->slots[i]);
		if ((td->mtclass->quirks & MT_QUIRK_NOT_SEEN_MEANS_UP) &&
			!s->seen_in_this_frame) {
			s->touch_state = false;
		}

		input_mt_slot(input, i);
		input_mt_report_slot_state(input, MT_TOOL_FINGER,
			s->touch_state);
		if (s->touch_state) {
			/* SIMG GCS - 130603 - bz33633 */
			/* this finger is on the screen */
			int wide, major, minor;
			if (td->mtclass->quirks & MT_QUIRK_SWAP_WH) {
				wide = (s->h > s->w);
				/*
				 * Divided by two to match visual scale of
				 * touch
				 */
				major = min(s->w, s->h) >> 1;
				minor = max(s->w, s->h) >> 1;
			} else {
				wide = (s->w > s->h);
				/*
				 * Divided by two to match visual scale of
				 * touch
				 */
				major = max(s->w, s->h) >> 1;
				minor = min(s->w, s->h) >> 1;
			}

			if (td->mtclass->quirks & MT_QUIRK_SWAP_XY) {
				input_event(input, EV_ABS, ABS_MT_POSITION_X,
					s->y);
				input_event(input, EV_ABS, ABS_MT_POSITION_Y,
					s->x);
			} else {
				input_event(input, EV_ABS, ABS_MT_POSITION_X,
					s->x);
				input_event(input, EV_ABS, ABS_MT_POSITION_Y,
					s->y);
			}
			input_event(input, EV_ABS, ABS_MT_ORIENTATION, wide);
			input_event(input, EV_ABS, ABS_MT_PRESSURE, s->p);
			input_event(input, EV_ABS, ABS_MT_TOUCH_MAJOR, major);
			input_event(input, EV_ABS, ABS_MT_TOUCH_MINOR, minor);
		}
		s->seen_in_this_frame = false;

	}

	input_mt_report_pointer_emulation(input, true);
	input_sync(input);
	td->num_received = 0;
}
#endif

int mhl3_mt_event(struct hid_device *hid, struct hid_field *field,
				struct hid_usage *usage, __s32 value)
{
	struct mt_device *td = hid_get_drvdata(hid);
#if (LINUX_KERNEL_VER >= 311)
	__s32 quirks = td->mtclass.quirks;
#else
	__s32 quirks = td->mtclass->quirks;
#endif

#if (LINUX_KERNEL_VER >= 311)
	struct input_dev *input = field->hidinput->input;

	if (field->report->id != td->mt_report_id)
		return 1;

	if (hid->claimed & HID_CLAIMED_INPUT) {
#else
	if (hid->claimed & HID_CLAIMED_INPUT && td->slots) {
#endif
		switch (usage->hid) {
		case HID_DG_INRANGE:
			if (quirks & MT_QUIRK_ALWAYS_VALID)
				td->curvalid = true;
			else if (quirks & MT_QUIRK_VALID_IS_INRANGE)
#if (LINUX_KERNEL_VER >= 311)
				td->curdata.inrange_state = value;
#else
				td->curvalid = value;
#endif
			break;
		case HID_DG_TIPSWITCH:
			if (quirks & MT_QUIRK_NOT_SEEN_MEANS_UP)
				td->curvalid = value;
			td->curdata.touch_state = value;
			break;
		case HID_DG_CONFIDENCE:
			if (quirks & MT_QUIRK_VALID_IS_CONFIDENCE)
				td->curvalid = value;
			break;
		case HID_DG_CONTACTID:
			td->curdata.contactid = value;
			break;
		case HID_DG_TIPPRESSURE:
			td->curdata.p = value;
			break;
		case HID_GD_X:
			/* SIMG 140714 - BZ33633 */
			if (quirks & MT_QUIRK_SWAP_LEFTRIGHT)
				value = field->logical_maximum - value;
#if (LINUX_KERNEL_VER >= 308)
			if (usage->code == ABS_MT_TOOL_X)
				td->curdata.cx = value;
			else
#endif
				td->curdata.x = value;
			break;
		case HID_GD_Y:
			/* SIMG 140714 - BZ33633 */
			if (quirks & MT_QUIRK_SWAP_UPDOWN)
				value = field->logical_maximum - value;
#if (LINUX_KERNEL_VER >= 308)
			if (usage->code == ABS_MT_TOOL_Y)
				td->curdata.cy = value;
			else
#endif
				td->curdata.y = value;
			break;
		case HID_DG_WIDTH:
			td->curdata.w = value;
			break;
		case HID_DG_HEIGHT:
			td->curdata.h = value;
			break;
		case HID_DG_CONTACTCOUNT:
#if (LINUX_KERNEL_VER < 311)
			/*
			 * Includes multi-packet support where subsequent
			 * packets are sent with zero contactcount.
			 */
			if (value)
				td->num_expected = value;
#endif
			break;

#if (LINUX_KERNEL_VER >= 311)
		case HID_DG_TOUCH:
			/* do nothing */
			break;
		default:
			if (usage->type)
				input_event(input, usage->type, usage->code,
						value);
			return 1;
		} /* end of switch */

		if (usage->usage_index + 1 == field->report_count) {
			/* we only take into account the last report. */
			if (usage->hid == td->last_slot_field)
				mt_complete_slot(td, field->hidinput->input);
		}
	}

#else
		default:
			/* fallback to the generic hidinput handling */
			return 0;
		}

		if (usage->hid == td->last_slot_field)
			mt_complete_slot(td);

		if (field->index == td->last_field_index
			&& td->num_received >= td->num_expected)
			mt_emit_event(td, field->hidinput->input);
	}

	/* we have handled the hidinput part, now remains hiddev */
	if (hid->claimed & HID_CLAIMED_HIDDEV && hid->hiddev_hid_event)
		hid->hiddev_hid_event(hid, field, usage, value);
#endif
	return 1;
}

#if (LINUX_KERNEL_VER >= 311)
static void mt_touch_report(struct hid_device *hid, struct hid_report *report)
{
	struct mt_device *td = hid_get_drvdata(hid);
	struct hid_field *field;
	unsigned count;
	int r, n;

	/*
	 * Includes multi-packet support where subsequent
	 * packets are sent with zero contactcount.
	 */
	if (td->cc_index >= 0) {
		struct hid_field *field = report->field[td->cc_index];
		int value = field->value[td->cc_value_index];
		if (value)
			td->num_expected = value;
	}

	for (r = 0; r < report->maxfield; r++) {
		field = report->field[r];
		count = field->report_count;

		if (!(HID_MAIN_ITEM_VARIABLE & field->flags))
			continue;

		for (n = 0; n < count; n++)
			mhl3_mt_event(hid, field, &field->usage[n],
				field->value[n]);
	}

	if (td->num_received >= td->num_expected)
		mt_sync_frame(td, report->field[0]->hidinput->input);
}

static void mt_touch_input_configured(struct hid_device *hdev,
					struct hid_input *hi)
{
	struct mt_device *td = hid_get_drvdata(hdev);
	struct mt_class *cls = &td->mtclass;
	struct input_dev *input = hi->input;

	if (!td->maxcontacts)
		td->maxcontacts = MT_DEFAULT_MAXCONTACT;

	mt_post_parse(td);
	if (td->serial_maybe)
		mt_post_parse_default_settings(td);

	if (cls->is_indirect)
		td->mt_flags |= INPUT_MT_POINTER;

	if (cls->quirks & MT_QUIRK_NOT_SEEN_MEANS_UP)
		td->mt_flags |= INPUT_MT_DROP_UNUSED;

	input_mt_init_slots(input, td->maxcontacts, td->mt_flags);

	td->mt_flags = 0;
}

void mt_report(struct hid_device *hid, struct hid_report *report)
{
	struct mt_device *td = hid_get_drvdata(hid);

	MHL3_HID_DBG_INFO(
		"%s mt_report welcome %x %x\n", __func__,
		(int)(hid->claimed & HID_CLAIMED_INPUT),
		(int)(report->id == td->mt_report_id));

	if (!(hid->claimed & HID_CLAIMED_INPUT))
		return;

	if (report->id == td->mt_report_id)
		mt_touch_report(hid, report);
}
#endif

static void mt_set_input_mode(struct hid_device *hdev)
{
	struct mt_device *td = hid_get_drvdata(hdev);
	struct hid_report *r;
	struct hid_report_enum *re;

	if (td->inputmode < 0)
		return;

	re = &(hdev->report_enum[HID_FEATURE_REPORT]);
	r = re->report_id_hash[td->inputmode];
#if (LINUX_KERNEL_VER >= 311)
	if (r) {
		r->field[0]->value[td->inputmode_index] = 0x02;
/*		hid_hw_request(hdev, r, HID_REQ_SET_REPORT); */
	}
#else
	if (r) {
		r->field[0]->value[0] = 0x02;
/* TODO: mhl3_hid_submit_report(hdev, r, USB_DIR_OUT); */
	}
#endif
}

#if (LINUX_KERNEL_VER >= 311)
static void mt_set_maxcontacts(struct hid_device *hdev)
{
	struct mt_device *td = hid_get_drvdata(hdev);
	struct hid_report *r;
	struct hid_report_enum *re;
	int fieldmax, max;

	if (td->maxcontact_report_id < 0)
		return;

	if (!td->mtclass.maxcontacts)
		return;

	re = &hdev->report_enum[HID_FEATURE_REPORT];
	r = re->report_id_hash[td->maxcontact_report_id];
	if (r) {
		max = td->mtclass.maxcontacts;
		fieldmax = r->field[0]->logical_maximum;
		max = min(fieldmax, max);
		if (r->field[0]->value[0] != max) {
			r->field[0]->value[0] = max;
			hid_hw_request(hdev, r, HID_REQ_SET_REPORT);
		}
	}
}
static void mt_post_parse_default_settings(struct mt_device *td)
{
	__s32 quirks = td->mtclass.quirks;

	/* unknown serial device needs special quirks */
	if (td->touches_by_report == 1) {
		quirks |= MT_QUIRK_ALWAYS_VALID;
		quirks &= ~MT_QUIRK_NOT_SEEN_MEANS_UP;
		quirks &= ~MT_QUIRK_VALID_IS_INRANGE;
		quirks &= ~MT_QUIRK_VALID_IS_CONFIDENCE;
		quirks &= ~MT_QUIRK_CONTACT_CNT_ACCURATE;
	}

	td->mtclass.quirks = quirks;
}

static void mt_post_parse(struct mt_device *td)
{
	struct mt_fields *f = td->fields;
	struct mt_class *cls = &td->mtclass;

	if (td->touches_by_report > 0) {
		int field_count_per_touch = f->length / td->touches_by_report;
		td->last_slot_field = f->usages[field_count_per_touch - 1];
	}

	if (td->cc_index < 0)
		cls->quirks &= ~MT_QUIRK_CONTACT_CNT_ACCURATE;
}

void mt_input_configured(struct hid_device *hdev, struct hid_input *hi)
{
	struct mt_device *td = hid_get_drvdata(hdev);
	char *name = kstrdup(hdev->name, GFP_KERNEL);

	MHL3_HID_DBG_INFO(
		"%s mt_input_configured welcome\n", __func__);

	if (name)
		hi->input->name = name;

	if (hi->report->id == td->mt_report_id)
		mt_touch_input_configured(hdev, hi);
}
#endif

static const struct hid_device_id mhl3_mt_devices[] = {
	{ .driver_data = MT_CLS_SERIAL,
		HID_USB_DEVICE(USB_VENDOR_ID_SILICONIMAGE,
			MHL_PRODUCT_ID_SILICONIMAGE_9394) },
	{ .driver_data = MT_CLS_SERIAL,
		HID_USB_DEVICE(USB_VENDOR_ID_SILICONIMAGE,
			MHL_PRODUCT_ID_SILICONIMAGE_9679) },
	{ .driver_data = MT_CLS_SERIAL,
		HID_USB_DEVICE(USB_VENDOR_ID_SILICONIMAGE,
			MHL_PRODUCT_ID_SILICONIMAGE_TEST) },
	{ .driver_data = MT_CLS_SERIAL,
		HID_USB_DEVICE(PCI_VENDOR_ID_SILICONIMAGE,
			MHL_PRODUCT_ID_SILICONIMAGE_9394) },
	{ .driver_data = MT_CLS_SERIAL,
		HID_USB_DEVICE(PCI_VENDOR_ID_SILICONIMAGE,
			MHL_PRODUCT_ID_SILICONIMAGE_9679) },
	{ .driver_data = MT_CLS_SERIAL,
		HID_USB_DEVICE(PCI_VENDOR_ID_SILICONIMAGE,
			MHL_PRODUCT_ID_SILICONIMAGE_TEST) },
	{ .driver_data = MT_CLS_DEFAULT, MT_USB_DEVICE(0x0457, 0x10A8) },
	{ .driver_data = MT_CLS_DEFAULT, MT_USB_DEVICE(0x2383, 0x0065) },
	{ .driver_data = MT_CLS_DEFAULT, MT_USB_DEVICE(0x1013, 0x1030) },
	{ .driver_data = MT_CLS_DEFAULT, MT_USB_DEVICE(0x04d8, 0xf723) },
	{ .driver_data = MT_CLS_DEFAULT, MT_USB_DEVICE(0x1b96, 0x0007) },

	/* 3M panels */
	{ .driver_data = MT_CLS_3M,
		HID_USB_DEVICE(USB_VENDOR_ID_3M,
			USB_DEVICE_ID_3M1968) },
	{ .driver_data = MT_CLS_3M,
		HID_USB_DEVICE(USB_VENDOR_ID_3M,
			USB_DEVICE_ID_3M2256) },

	/* ActionStar panels */
	{ .driver_data = MT_CLS_DEFAULT,
		HID_USB_DEVICE(USB_VENDOR_ID_ACTIONSTAR,
			USB_DEVICE_ID_ACTIONSTAR_1011) },

	/* Added - Atmel panels */
	{ .driver_data = MT_CLS_OCULAR,
		HID_USB_DEVICE(USB_VENDOR_ID_ATMEL,
		USB_DEVICE_ID_ATMEL_MXT_384E) },
	{ .driver_data = MT_CLS_SERIAL,
		HID_USB_DEVICE(USB_VENDOR_ID_ATMEL, 0x214E) },

	/* Cando panels */
	{ .driver_data = MT_CLS_DUAL_INRANGE_CONTACTNUMBER,
		HID_USB_DEVICE(USB_VENDOR_ID_CANDO,
			USB_DEVICE_ID_CANDO_MULTI_TOUCH) },
	{ .driver_data = MT_CLS_DUAL_INRANGE_CONTACTNUMBER,
		HID_USB_DEVICE(USB_VENDOR_ID_CANDO,
			USB_DEVICE_ID_CANDO_MULTI_TOUCH_10_1) },
	{ .driver_data = MT_CLS_DUAL_INRANGE_CONTACTNUMBER,
		HID_USB_DEVICE(USB_VENDOR_ID_CANDO,
			USB_DEVICE_ID_CANDO_MULTI_TOUCH_11_6) },
	{ .driver_data = MT_CLS_DUAL_INRANGE_CONTACTNUMBER,
		HID_USB_DEVICE(USB_VENDOR_ID_CANDO,
			USB_DEVICE_ID_CANDO_MULTI_TOUCH_15_6) },

	/* Chunghwa Telecom touch panels */
	{ .driver_data = MT_CLS_DEFAULT,
		HID_USB_DEVICE(USB_VENDOR_ID_CHUNGHWAT,
			USB_DEVICE_ID_CHUNGHWAT_MULTITOUCH) },

	/* CVTouch panels */
	{ .driver_data = MT_CLS_DEFAULT,
		HID_USB_DEVICE(USB_VENDOR_ID_CVTOUCH,
			USB_DEVICE_ID_CVTOUCH_SCREEN) },

	/* Cypress panel */
	{ .driver_data = MT_CLS_CYPRESS,
		HID_USB_DEVICE(USB_VENDOR_ID_CYPRESS,
			USB_DEVICE_ID_CYPRESS_TRUETOUCH) },

	/* Elo TouchSystems IntelliTouch Plus panel */
	{ .driver_data = MT_CLS_DUAL_NSMU_CONTACTID,
		HID_USB_DEVICE(USB_VENDOR_ID_ELO,
			USB_DEVICE_ID_ELO_TS2515) },

	/* GeneralTouch panel */
	{ .driver_data = MT_CLS_DUAL_INRANGE_CONTACTNUMBER,
		HID_USB_DEVICE(USB_VENDOR_ID_GENERAL_TOUCH,
			USB_DEVICE_ID_GENERAL_TOUCH_WIN7_TWOFINGERS) },

	/* GoodTouch panels */
	{ .driver_data = MT_CLS_DEFAULT,
		HID_USB_DEVICE(USB_VENDOR_ID_GOODTOUCH,
			USB_DEVICE_ID_GOODTOUCH_000f) },

	/* Ideacom panel */
	{ .driver_data = MT_CLS_SERIAL,
		HID_USB_DEVICE(USB_VENDOR_ID_IDEACOM,
			USB_DEVICE_ID_IDEACOM_IDC6650) },

	/* Ilitek dual touch panel */
	{  .driver_data = MT_CLS_DEFAULT,
		HID_USB_DEVICE(USB_VENDOR_ID_ILITEK,
			USB_DEVICE_ID_ILITEK_MULTITOUCH) },

	/* IRTOUCH panels */
	{ .driver_data = MT_CLS_DUAL_INRANGE_CONTACTID,
		HID_USB_DEVICE(USB_VENDOR_ID_IRTOUCHSYSTEMS,
			USB_DEVICE_ID_IRTOUCH_INFRARED_USB) },

	/* LG Display panels */
	{ .driver_data = MT_CLS_DEFAULT,
		HID_USB_DEVICE(USB_VENDOR_ID_LG,
			USB_DEVICE_ID_LG_MULTITOUCH) },

	/* Lumio panels */
	{ .driver_data = MT_CLS_CONFIDENCE_MINUS_ONE,
		HID_USB_DEVICE(USB_VENDOR_ID_LUMIO,
			USB_DEVICE_ID_CRYSTALTOUCH) },
	{ .driver_data = MT_CLS_CONFIDENCE_MINUS_ONE,
		HID_USB_DEVICE(USB_VENDOR_ID_LUMIO,
			USB_DEVICE_ID_CRYSTALTOUCH_DUAL) },

	/* MosArt panels */
	{ .driver_data = MT_CLS_CONFIDENCE_MINUS_ONE,
		HID_USB_DEVICE(USB_VENDOR_ID_ASUS,
			USB_DEVICE_ID_ASUS_T91MT)},
	{ .driver_data = MT_CLS_CONFIDENCE_MINUS_ONE,
		HID_USB_DEVICE(USB_VENDOR_ID_ASUS,
			USB_DEVICE_ID_ASUSTEK_MULTITOUCH_YFO) },
	{ .driver_data = MT_CLS_CONFIDENCE_MINUS_ONE,
		HID_USB_DEVICE(USB_VENDOR_ID_TURBOX,
			USB_DEVICE_ID_TURBOX_TOUCHSCREEN_MOSART) },

	/* PenMount panels */
	{ .driver_data = MT_CLS_CONFIDENCE,
		HID_USB_DEVICE(USB_VENDOR_ID_PENMOUNT,
			USB_DEVICE_ID_PENMOUNT_PCI) },

	/* PixCir-based panels */
	{ .driver_data = MT_CLS_DUAL_INRANGE_CONTACTID,
		HID_USB_DEVICE(USB_VENDOR_ID_HANVON,
			USB_DEVICE_ID_HANVON_MULTITOUCH) },
	{ .driver_data = MT_CLS_DUAL_INRANGE_CONTACTID,
		HID_USB_DEVICE(USB_VENDOR_ID_CANDO,
			USB_DEVICE_ID_CANDO_PIXCIR_MULTI_TOUCH) },

	/* Stantum panels */
	{ .driver_data = MT_CLS_CONFIDENCE,
		HID_USB_DEVICE(USB_VENDOR_ID_STANTUM,
			USB_DEVICE_ID_MTP)},
	{ .driver_data = MT_CLS_CONFIDENCE,
		HID_USB_DEVICE(USB_VENDOR_ID_STANTUM_STM,
			USB_DEVICE_ID_MTP_STM)},
	{ .driver_data = MT_CLS_CONFIDENCE,
		HID_USB_DEVICE(USB_VENDOR_ID_STANTUM_SITRONIX,
			USB_DEVICE_ID_MTP_SITRONIX)},

	/* Touch International panels */
	{ .driver_data = MT_CLS_DEFAULT,
		HID_USB_DEVICE(USB_VENDOR_ID_TOUCH_INTL,
			USB_DEVICE_ID_TOUCH_INTL_MULTI_TOUCH) },

	/* Unitec panels */
	{ .driver_data = MT_CLS_DEFAULT,
		HID_USB_DEVICE(USB_VENDOR_ID_UNITEC,
			USB_DEVICE_ID_UNITEC_USB_TOUCH_0709) },
	{ .driver_data = MT_CLS_DEFAULT,
		HID_USB_DEVICE(USB_VENDOR_ID_UNITEC,
			USB_DEVICE_ID_UNITEC_USB_TOUCH_0A19) },
	/* XAT */
	{ .driver_data = MT_CLS_DEFAULT,
		HID_USB_DEVICE(USB_VENDOR_ID_XAT,
			USB_DEVICE_ID_XAT_CSR) },

#if (LINUX_KERNEL_VER >= 311)
	{ .driver_data = MT_CLS_3M,
		MT_USB_DEVICE(USB_VENDOR_ID_3M,
		USB_DEVICE_ID_3M3266) },
	{ .driver_data = MT_CLS_SERIAL,
		MT_USB_DEVICE(USB_VENDOR_ID_ATMEL,
			USB_DEVICE_ID_ATMEL_MULTITOUCH) },
	{ .driver_data = MT_CLS_SERIAL,
		MT_USB_DEVICE(USB_VENDOR_ID_ATMEL,
			USB_DEVICE_ID_ATMEL_MXT_DIGITIZER) },

	/* Baanto multitouch devices */
	{ .driver_data = MT_CLS_NSMU,
		MT_USB_DEVICE(USB_VENDOR_ID_BAANTO,
			USB_DEVICE_ID_BAANTO_MT_190W2) },

	/* Data Modul easyMaxTouch */
	{ .driver_data = MT_CLS_DEFAULT,
		MT_USB_DEVICE(USB_VENDOR_ID_DATA_MODUL,
			USB_VENDOR_ID_DATA_MODUL_EASYMAXTOUCH) },

	/* eGalax devices (resistive) */
	{ .driver_data = MT_CLS_EGALAX,
		MT_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_480D) },
	{ .driver_data = MT_CLS_EGALAX,
		MT_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_480E) },

	/* eGalax devices (capacitive) */
	{ .driver_data = MT_CLS_EGALAX_SERIAL,
		MT_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_7207) },
	{ .driver_data = MT_CLS_EGALAX,
		MT_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_720C) },
	{ .driver_data = MT_CLS_EGALAX_SERIAL,
		MT_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_7224) },
	{ .driver_data = MT_CLS_EGALAX_SERIAL,
		MT_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_722A) },
	{ .driver_data = MT_CLS_EGALAX_SERIAL,
		MT_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_725E) },
	{ .driver_data = MT_CLS_EGALAX_SERIAL,
		MT_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_7262) },
	{ .driver_data = MT_CLS_EGALAX,
		MT_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_726B) },
	{ .driver_data = MT_CLS_EGALAX,
		MT_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_72A1) },
	{ .driver_data = MT_CLS_EGALAX_SERIAL,
		MT_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_72AA) },
	{ .driver_data = MT_CLS_EGALAX,
		HID_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_72C4) },
	{ .driver_data = MT_CLS_EGALAX,
		HID_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_72D0) },
	{ .driver_data = MT_CLS_EGALAX,
		MT_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_72FA) },
	{ .driver_data = MT_CLS_EGALAX,
		MT_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_7302) },
	{ .driver_data = MT_CLS_EGALAX_SERIAL,
		MT_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_7349) },
	{ .driver_data = MT_CLS_EGALAX_SERIAL,
		MT_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_73F7) },
	{ .driver_data = MT_CLS_EGALAX_SERIAL,
		MT_USB_DEVICE(USB_VENDOR_ID_DWAV,
			USB_DEVICE_ID_DWAV_EGALAX_MULTITOUCH_A001) },

	/* Flatfrog Panels */
	{ .driver_data = MT_CLS_FLATFROG,
		MT_USB_DEVICE(USB_VENDOR_ID_FLATFROG,
			USB_DEVICE_ID_MULTITOUCH_3200) },

	{ .driver_data = MT_CLS_GENERALTOUCH_PWT_TENFINGERS,
		MT_USB_DEVICE(USB_VENDOR_ID_GENERAL_TOUCH,
			USB_DEVICE_ID_GENERAL_TOUCH_WIN8_PWT_TENFINGERS) },

	/* Gametel game controller */
	{ .driver_data = MT_CLS_NSMU,
		MT_BT_DEVICE(USB_VENDOR_ID_FRUCTEL,
			USB_DEVICE_ID_GAMETEL_MT_MODE) },

	/* Hanvon panels */
	{ .driver_data = MT_CLS_DUAL_INRANGE_CONTACTID,
		MT_USB_DEVICE(USB_VENDOR_ID_HANVON_ALT,
			USB_DEVICE_ID_HANVON_ALT_MULTITOUCH) },

	{ .driver_data = MT_CLS_SERIAL,
		MT_USB_DEVICE(USB_VENDOR_ID_IDEACOM,
			USB_DEVICE_ID_IDEACOM_IDC6651) },

	/* Nexio panels */
	{ .driver_data = MT_CLS_DEFAULT,
		MT_USB_DEVICE(USB_VENDOR_ID_NEXIO,
			USB_DEVICE_ID_NEXIO_MULTITOUCH_420)},

	/* Panasonic panels */
	{ .driver_data = MT_CLS_PANASONIC,
		MT_USB_DEVICE(USB_VENDOR_ID_PANASONIC,
			USB_DEVICE_ID_PANABOARD_UBT780) },
	{ .driver_data = MT_CLS_PANASONIC,
		MT_USB_DEVICE(USB_VENDOR_ID_PANASONIC,
			USB_DEVICE_ID_PANABOARD_UBT880) },

	/* Novatek Panel */
	{ .driver_data = MT_CLS_NSMU,
		MT_USB_DEVICE(USB_VENDOR_ID_NOVATEK,
			USB_DEVICE_ID_NOVATEK_PCT) },

	/* PixArt optical touch screen */
	{ .driver_data = MT_CLS_INRANGE_CONTACTNUMBER,
		MT_USB_DEVICE(USB_VENDOR_ID_PIXART,
			USB_DEVICE_ID_PIXART_OPTICAL_TOUCH_SCREEN) },
	{ .driver_data = MT_CLS_INRANGE_CONTACTNUMBER,
		MT_USB_DEVICE(USB_VENDOR_ID_PIXART,
			USB_DEVICE_ID_PIXART_OPTICAL_TOUCH_SCREEN1) },
	{ .driver_data = MT_CLS_INRANGE_CONTACTNUMBER,
		MT_USB_DEVICE(USB_VENDOR_ID_PIXART,
			USB_DEVICE_ID_PIXART_OPTICAL_TOUCH_SCREEN2) },

	/* Quanta-based panels */
	{ .driver_data = MT_CLS_CONFIDENCE_CONTACT_ID,
		MT_USB_DEVICE(USB_VENDOR_ID_QUANTA,
			USB_DEVICE_ID_QUANTA_OPTICAL_TOUCH) },
	{ .driver_data = MT_CLS_CONFIDENCE_CONTACT_ID,
		MT_USB_DEVICE(USB_VENDOR_ID_QUANTA,
			USB_DEVICE_ID_QUANTA_OPTICAL_TOUCH_3001) },
	{ .driver_data = MT_CLS_CONFIDENCE_CONTACT_ID,
		MT_USB_DEVICE(USB_VENDOR_ID_QUANTA,
			USB_DEVICE_ID_QUANTA_OPTICAL_TOUCH_3008) },

	/* TopSeed panels */
	{ .driver_data = MT_CLS_TOPSEED,
		MT_USB_DEVICE(USB_VENDOR_ID_TOPSEED2,
			USB_DEVICE_ID_TOPSEED2_PERIPAD_701) },

	/* Xiroku */
	{ .driver_data = MT_CLS_NSMU,
		MT_USB_DEVICE(USB_VENDOR_ID_XIROKU,
			USB_DEVICE_ID_XIROKU_SPX) },
	{ .driver_data = MT_CLS_NSMU,
		MT_USB_DEVICE(USB_VENDOR_ID_XIROKU,
			USB_DEVICE_ID_XIROKU_MPX) },
	{ .driver_data = MT_CLS_NSMU,
		MT_USB_DEVICE(USB_VENDOR_ID_XIROKU,
			USB_DEVICE_ID_XIROKU_CSR) },
	{ .driver_data = MT_CLS_NSMU,
		MT_USB_DEVICE(USB_VENDOR_ID_XIROKU,
			USB_DEVICE_ID_XIROKU_SPX1) },
	{ .driver_data = MT_CLS_NSMU,
		MT_USB_DEVICE(USB_VENDOR_ID_XIROKU,
			USB_DEVICE_ID_XIROKU_MPX1) },
	{ .driver_data = MT_CLS_NSMU,
		MT_USB_DEVICE(USB_VENDOR_ID_XIROKU,
			USB_DEVICE_ID_XIROKU_CSR1) },
	{ .driver_data = MT_CLS_NSMU,
		MT_USB_DEVICE(USB_VENDOR_ID_XIROKU,
			USB_DEVICE_ID_XIROKU_SPX2) },
	{ .driver_data = MT_CLS_NSMU,
		MT_USB_DEVICE(USB_VENDOR_ID_XIROKU,
			USB_DEVICE_ID_XIROKU_MPX2) },
	{ .driver_data = MT_CLS_NSMU,
		MT_USB_DEVICE(USB_VENDOR_ID_XIROKU,
			USB_DEVICE_ID_XIROKU_CSR2) },

	/* Zytronic panels */
	{ .driver_data = MT_CLS_SERIAL,
		MT_USB_DEVICE(USB_VENDOR_ID_ZYTRONIC,
			USB_DEVICE_ID_ZYTRONIC_ZXY100) },

	/* Generic MT device */
	{ HID_DEVICE(HID_BUS_ANY,
		HID_GROUP_MULTITOUCH, HID_ANY_ID, HID_ANY_ID) },

	{ .driver_data = MT_CLS_DEFAULT,
		MT_USB_DEVICE(USB_VENDOR_ID_DATA_MODUL,
		USB_VENDOR_ID_DATA_MODUL_EASYMAXTOUCH) },

	{ HID_DEVICE(BUS_VIRTUAL,
		HID_GROUP_MULTITOUCH, HID_ANY_ID, HID_ANY_ID) },
#endif

	{ }
};
MODULE_DEVICE_TABLE(hid, mhl3_mt_devices);

static const struct hid_usage_id mt_grabbed_usages[] = {
	{ HID_ANY_ID, HID_ANY_ID, HID_ANY_ID },
	{ HID_ANY_ID - 1, HID_ANY_ID - 1, HID_ANY_ID - 1}
};

/* If incoming HID device is a multitouch device, this function
 * will process it instead of the normal HID connect.
 *
 * Do not call in an interrupt context.
 */
int mhl3_mt_add(struct mhl3_hid_data *mhid, struct hid_device_id *id)
{
	int ret, i;
	struct hid_device *hdev = mhid->hid;
	struct mt_device *td;
	struct mt_class *mtclass = mhl3_mt_classes; /* MT_CLS_DEFAULT */
	const struct hid_device_id *this_id = NULL;

#if (LINUX_KERNEL_VER >= 311)
	struct hid_input *hi;
#endif

	for (i = 0; mhl3_mt_devices[i].driver_data; i++) {
		if ((id->vendor == mhl3_mt_devices[i].vendor) &&
				(id->product == mhl3_mt_devices[i].product)) {
			this_id = &mhl3_mt_devices[i];
			break;
		}
	}
	if (this_id == NULL) {
		MHL3_HID_DBG_INFO(
			"VID/PID %04X/%04X not found\n", (int)id->vendor,
			(int)id->product);
		return -ENODEV;
	}

	for (i = 0; mhl3_mt_classes[i].name; i++) {
		if (this_id->driver_data == mhl3_mt_classes[i].name) {
			mtclass = &(mhl3_mt_classes[i]);
			break;
		}
	}

	/* This allows the driver to correctly support devices
	 * that emit events over several HID messages.
	 */
	hdev->quirks |= HID_QUIRK_NO_INPUT_SYNC;

#if (LINUX_KERNEL_VER >= 311)
	/*
	 * This allows the driver to handle different input sensors
	 * that emits events through different reports on the same HID
	 * device.
	 */
	hdev->quirks |= HID_QUIRK_MULTI_INPUT;
	hdev->quirks |= HID_QUIRK_NO_EMPTY_INPUT;
#endif

	td = kzalloc(sizeof(struct mt_device), GFP_KERNEL);
	if (!td) {
		dev_err(&hdev->dev, "cannot allocate multitouch data\n");
		return -ENOMEM;
	}
#if (LINUX_KERNEL_VER >= 311)
	td->mtclass = *mtclass;
	td->maxcontact_report_id = -1;
	td->cc_index = -1;
	td->mt_report_id = -1;
	td->pen_report_id = -1;
#else
	td->mtclass = mtclass;
	td->last_mt_collection = -1;
#endif
	td->inputmode = -1;

	hid_set_drvdata(hdev, td);

#if (LINUX_KERNEL_VER >= 311)
	td->fields = kzalloc(sizeof(struct mt_fields), GFP_KERNEL);
	if (!td->fields) {
		dev_err(&hdev->dev, "cannot allocate multitouch fields data\n");
		ret = -ENOMEM;
		goto fail;
	}

	if ((id->vendor == HID_ANY_ID) && (id->product == HID_ANY_ID))
		td->serial_maybe = true;
#endif

	ret = mhl3_hid_report_desc_parse(mhid);
	if (ret != 0)
		goto fail;

/*	ret = hid_connect(hdev, HID_CONNECT_DEFAULT); */
	ret = hidinput_connect(hdev, 0);
	if (!ret) {
		MHL3_HID_DBG_ERR(
			"%s hidinput_connect succeeded\n", __func__);
		hdev->claimed |= HID_CLAIMED_INPUT;
		mhid->flags |= MHL3_HID_CONNECTED;
	} else {
		MHL3_HID_DBG_ERR(
			"%s hidinput_connect FAILED with value %x\n", __func__,
			(int)ret);

#if (LINUX_KERNEL_VER >= 311)
		goto hid_fail;
#endif
	}

/* TODO: HIDRAW not supported
	if (!hidraw_connect(hdev)) {
		MHL3_HID_DBG_ERR(
			"%s hidraw_connect succeeded\n", __func__);
		hdev->claimed |= HID_CLAIMED_HIDRAW;
		mhid->flags |= MHL3_HID_CONNECTED;
	} else {
		MHL3_HID_DBG_ERR(
			"%s hidraw_connect FAILED\n", __func__);
	}
*/

/*	if (ret)
		goto fail;
*/
#if (LINUX_KERNEL_VER >= 311)
	mt_set_maxcontacts(hdev);
	mt_set_input_mode(hdev);

	kfree(td->fields);
	td->fields = NULL;
	return 0;

hid_fail:
	list_for_each_entry(hi, &hdev->inputs, list)
		mt_free_input_name(hi);
fail:
	kfree(td->fields);
#else
	td->slots = kzalloc(
		td->maxcontacts * sizeof(struct mt_slot), GFP_KERNEL);
	if (!td->slots) {
		dev_err(&hdev->dev, "cannot allocate multitouch slots\n");
		hid_hw_stop(hdev);
		ret = -ENOMEM;
		goto fail;
	}
	mt_set_input_mode(hdev);
	return 0;
fail:
#endif
	kfree(td);
	return ret;
}

#if 0
#ifdef CONFIG_PM
static int mt_reset_resume(struct hid_device *hdev)
{
	mt_set_input_mode(hdev);
	return 0;
}
#endif

static void mt_remove(struct hid_device *hdev)
{
	struct mt_device *td = hid_get_drvdata(hdev);

#if (LINUX_KERNEL_VER >= 311)
	struct hid_input *hi;

	list_for_each_entry(hi, &hdev->inputs, list)
		mt_free_input_name(hi);
	hid_hw_stop(hdev);
#else
	hid_hw_stop(hdev);
	kfree(td->slots);
#endif
	kfree(td);
	hid_set_drvdata(hdev, NULL);
}


static struct hid_driver mt_driver = {
	.name = "hid-multitouch",
	.id_table = mhl3_mt_devices,
/*	.probe = mt_probe, */
	.remove = mt_remove,
	.input_mapping = mhl3_mt_input_mapping,
	.input_mapped = mhl3_mt_input_mapped,
#if (LINUX_KERNEL_VER >= 311)
	.input_configured = mt_input_configured,
#endif
	.feature_mapping = mt_feature_mapping,
	.usage_table = mt_grabbed_usages,
/*	.event = mt_event, */
#if (LINUX_KERNEL_VER >= 311)
	.report = mt_report,
#endif
#ifdef CONFIG_PM
	.reset_resume = mt_reset_resume,
#if (LINUX_KERNEL_VER >= 311)
	.resume = mt_resume,
#endif
#endif
};

static int __init mt_init(void)
{
	return hid_register_driver(&mt_driver);
}

static void __exit mt_exit(void)
{
	hid_unregister_driver(&mt_driver);
}

#endif
