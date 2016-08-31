/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LINUX_USB_NV_USB_BULK_H
#define  __LINUX_USB_NV_USB_BULK_H


#define NVUSB_BULK_WRITE	0X2000
#define NVUSB_BULK_READ	0X2001

/* command block wrapper */
struct nvusb_cb_wrap {
	__le32	Signature;
	__u32	Tag;
	__le32	DataTransferLength;
	__u8	Flags;
	__u8	Length;
	__u8	CDB[16];
};

#define US_BULK_CB_WRAP_LEN	31
#define US_BULK_CB_SIGN		0x43425355
#define US_BULK_FLAG_IN		(1 << 7)
#define US_BULK_FLAG_OUT	0

/* command status wrapper */
struct nvusb_cs_wrap {
	__le32	Signature;
	__u32	Tag;
	__le32	Residue;
	__u8	Status;
};

#define US_BULK_CS_WRAP_LEN	13
#define US_BULK_CS_SIGN		0x53425355
#define US_BULK_STAT_OK		0
#define US_BULK_STAT_FAIL	1
#define US_BULK_STAT_BAD_DATA	2

struct bulk_data {
	struct nv_usb *dev;
	__u8 data_direction;
	__u8 sub_cmd_length;
	__u8 *sub_cmd;
	__u32 length;
	__u8 *buf;
	__u8 write_char;
	__u32 data_transfer_time;
	__u32  g_data_transfer_time;
};

struct user_bulk_data {
	__u8 sub_cmd_length;
	__u8 __user *sub_cmd;
	__u32 length;
	__u8 __user *buf;
	__u8 write_char;
	__u32  data_transfer_time;
	__u32  g_data_transfer_time;

};


/* Structure to hold all of our device specific stuff */
struct nv_usb {
	/* the usb device for this device */
	struct usb_device	*udev;
	/* the interface for this device */
	struct usb_interface	*interface;
	/* in case we need to retract our submissions */
	struct usb_anchor	submitted;
	/* the size of the receive buffer */
	size_t			bulk_in_size;
	/* the address of the bulk in endpoint */
	__u8			bulk_in_endpointAddr;
	/* the address of the bulk out endpoint */
	__u8			bulk_out_endpointAddr;
	__u32			tag;
	struct kref		kref;
	struct mutex		mutex;
};
#define to_nv_usb_dev(d) container_of(d, struct nv_usb, kref)

#endif



