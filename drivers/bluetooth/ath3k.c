/*
 * Copyright (c) 2008-2009 Atheros Communications Inc.
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/usb.h>
#include <net/bluetooth/bluetooth.h>

#include "ath3k.h"

#define VERSION "1.0"
#define ATH3K_FIRMWARE	"ath3k-1.fw"

#define ROME1_1_USB_RAMPATCH_FILE	"ar3k/rampatch_1.1.img"
#define ROME1_1_USB_NVM_FILE		"ar3k/nvm_tlv_usb_1.1.bin"

#define ROME2_1_USB_RAMPATCH_FILE	"ar3k/rampatch_tlv_usb_2.1.tlv"
#define ROME2_1_USB_NVM_FILE		"ar3k/nvm_tlv_usb_2.1.bin"

#define ROME3_0_USB_RAMPATCH_FILE	"ar3k/rampatch_tlv_usb_3.0.tlv"
#define ROME3_0_USB_NVM_FILE		"ar3k/nvm_tlv_usb_3.0.bin"

#define TF1_1_USB_RAMPATCH_FILE	"ar3k/rampatch_tlv_usb_tf_1.1.tlv"
#define TF1_1_USB_NVM_FILE		"ar3k/nvm_tlv_usb_tf_1.1.bin"

#define ROME2_1_USB_RAMPATCH_HEADER	sizeof(struct rome2_1_version)
#define ROME1_1_USB_RAMPATCH_HEADER	sizeof(struct rome1_1_version)

#define ROME1_1_USB_NVM_HEADER			0x04
#define ROME2_1_USB_NVM_HEADER			0x04

#define ROME1_1_USB_CHIP_VERSION		0x101
#define ROME2_1_USB_CHIP_VERSION		0x200
#define ROME3_0_USB_CHIP_VERSION		0x300
#define ROME3_2_USB_CHIP_VERSION		0x302

#define TF1_1_USB_PRODUCT_ID			0xe500
#define ATH3K_DNLOAD				0x01
#define ATH3K_GETSTATE				0x05
#define ATH3K_SET_NORMAL_MODE			0x07
#define ATH3K_GETVERSION			0x09
#define USB_REG_SWITCH_VID_PID			0x0a

#define ATH3K_MODE_MASK				0x3F
#define ATH3K_NORMAL_MODE			0x0E

#define ATH3K_PATCH_UPDATE			0xA0
#define ATH3K_SYSCFG_UPDATE			0x60
#define ATH3K_PATCH_SYSCFG_UPDATE		(ATH3K_PATCH_UPDATE | \
							ATH3K_SYSCFG_UPDATE)

#define ATH3K_XTAL_FREQ_26M			0x00
#define ATH3K_XTAL_FREQ_40M			0x01
#define ATH3K_XTAL_FREQ_19P2			0x02
#define ATH3K_NAME_LEN				0xFF

struct __packed rome1_1_version {
	u8	type;
	u8	length[3];
	u8	sign_ver;
	u8	sign_algo;
	u8	resv1[2];
	u16	product_id;
	u16	build_ver;
	u16	patch_ver;
	u8	resv2[2];
	u32	entry_addr;
};
struct __packed rome2_1_version {
	u8	type;
	u8	length[3];
	u32	total_len;
	u32	patch_len;
	u8	sign_ver;
	u8	sign_algo;
	u8	resv1[2];
	u16	product_id;
	u16	build_ver;
	u16	patch_ver;
	u8	resv2[2];
	u32	entry_addr;
};


static struct usb_device_id ath3k_table[] = {
	/* Atheros AR3011 */
	{ USB_DEVICE(0x0CF3, 0x3000) },

	/* Atheros AR3011 with sflash firmware*/
	{ USB_DEVICE(0x0CF3, 0x3002) },
	{ USB_DEVICE(0x0CF3, 0xE019) },
	{ USB_DEVICE(0x13d3, 0x3304) },
	{ USB_DEVICE(0x0930, 0x0215) },
	{ USB_DEVICE(0x0489, 0xE03D) },
	{ USB_DEVICE(0x0489, 0xE027) },

	/* Atheros AR9285 Malbec with sflash firmware */
	{ USB_DEVICE(0x03F0, 0x311D) },

	/* Atheros AR3012 with sflash firmware*/
	{ USB_DEVICE(0x0CF3, 0x0036) },
	{ USB_DEVICE(0x0CF3, 0x3004) },
	{ USB_DEVICE(0x0CF3, 0x3008) },
	{ USB_DEVICE(0x0CF3, 0x311D) },
	{ USB_DEVICE(0x0CF3, 0x311E) },
	{ USB_DEVICE(0x0CF3, 0x311F) },
	{ USB_DEVICE(0x0CF3, 0x817a) },
	{ USB_DEVICE(0x0CF3, 0xe500) },
	{ USB_DEVICE(0x13d3, 0x3375) },
	{ USB_DEVICE(0x04CA, 0x3004) },
	{ USB_DEVICE(0x04CA, 0x3005) },
	{ USB_DEVICE(0x04CA, 0x3006) },
	{ USB_DEVICE(0x04CA, 0x3007) },
	{ USB_DEVICE(0x04CA, 0x3008) },
	{ USB_DEVICE(0x13d3, 0x3362) },
	{ USB_DEVICE(0x0CF3, 0xE004) },
	{ USB_DEVICE(0x0CF3, 0xE005) },
	{ USB_DEVICE(0x0930, 0x0219) },
	{ USB_DEVICE(0x0489, 0xe057) },
	{ USB_DEVICE(0x13d3, 0x3393) },
	{ USB_DEVICE(0x0489, 0xe04e) },
	{ USB_DEVICE(0x0489, 0xe056) },
	{ USB_DEVICE(0x0489, 0xe04d) },
	{ USB_DEVICE(0x04c5, 0x1330) },
	{ USB_DEVICE(0x13d3, 0x3402) },
	{ USB_DEVICE(0x0cf3, 0x3121) },
	{ USB_DEVICE(0x0cf3, 0xe003) },

	/* Atheros AR5BBU12 with sflash firmware */
	{ USB_DEVICE(0x0489, 0xE02C) },

	/* Atheros AR5BBU22 with sflash firmware */
	{ USB_DEVICE(0x0489, 0xE03C) },
	{ USB_DEVICE(0x0489, 0xE036) },

	{ }	/* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, ath3k_table);

#define BTUSB_ATH3012		0x80
/* This table is to load patch and sysconfig files
 * for AR3012 */
static struct usb_device_id ath3k_blist_tbl[] = {

	/* Atheros AR3012 with sflash firmware*/
	{ USB_DEVICE(0x0CF3, 0x0036), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x3004), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x3008), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x311D), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x311E), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x311F), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0CF3, 0x817a), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x13d3, 0x3375), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x3004), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x3005), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x3006), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x3007), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x3008), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x13d3, 0x3362), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0xe004), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0xe005), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0930, 0x0219), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0489, 0xe057), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x13d3, 0x3393), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0489, 0xe04e), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0489, 0xe056), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0489, 0xe04d), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04c5, 0x1330), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x13d3, 0x3402), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x3121), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0xe003), .driver_info = BTUSB_ATH3012 },

	/* Atheros AR5BBU22 with sflash firmware */
	{ USB_DEVICE(0x0489, 0xE03C), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0489, 0xE036), .driver_info = BTUSB_ATH3012 },

	{ }	/* Terminating entry */
};

#define USB_REQ_DFU_DNLOAD	1
#define BULK_SIZE		4096
#define FW_HDR_SIZE		20
#define TIMEGAP_USEC_MIN	50
#define TIMEGAP_USEC_MAX	100

static int ath3k_load_firmware(struct usb_device *udev,
				const struct firmware *firmware)
{
	u8 *send_buf;
	int err, pipe, len, size, sent = 0;
	int count = firmware->size;

	BT_DBG("udev %p", udev);

	pipe = usb_sndctrlpipe(udev, 0);

	send_buf = kmalloc(BULK_SIZE, GFP_KERNEL);
	if (!send_buf) {
		BT_ERR("Can't allocate memory chunk for firmware");
		return -ENOMEM;
	}

	memcpy(send_buf, firmware->data, 20);
	if ((err = usb_control_msg(udev, pipe,
				USB_REQ_DFU_DNLOAD,
				USB_TYPE_VENDOR, 0, 0,
				send_buf, 20, USB_CTRL_SET_TIMEOUT)) < 0) {
		BT_ERR("Can't change to loading configuration err");
		goto error;
	}
	sent += 20;
	count -= 20;

	while (count) {
		/* workaround the compatibility issue with xHCI controller*/
		usleep_range(TIMEGAP_USEC_MIN, TIMEGAP_USEC_MAX);

		size = min_t(uint, count, BULK_SIZE);
		pipe = usb_sndbulkpipe(udev, 0x02);
		memcpy(send_buf, firmware->data + sent, size);

		err = usb_bulk_msg(udev, pipe, send_buf, size,
					&len, 3000);

		if (err || (len != size)) {
			BT_ERR("Error in firmware loading err = %d,"
				"len = %d, size = %d", err, len, size);
			goto error;
		}

		sent  += size;
		count -= size;
	}

error:
	kfree(send_buf);
	return err;
}

static int ath3k_get_state(struct usb_device *udev, unsigned char *state)
{
	int ret, pipe = 0;
	char *buf;

	buf = kmalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	pipe = usb_rcvctrlpipe(udev, 0);
	ret = usb_control_msg(udev, pipe, ATH3K_GETSTATE,
			      USB_TYPE_VENDOR | USB_DIR_IN, 0, 0,
			      buf, sizeof(*buf), USB_CTRL_SET_TIMEOUT);

	*state = *buf;
	kfree(buf);

	return ret;
}

static int ath3k_get_version(struct usb_device *udev,
			struct ath3k_version *version)
{
	int ret, pipe = 0;
	struct ath3k_version *buf;
	const int size = sizeof(*buf);

	buf = kmalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	pipe = usb_rcvctrlpipe(udev, 0);
	ret = usb_control_msg(udev, pipe, ATH3K_GETVERSION,
			      USB_TYPE_VENDOR | USB_DIR_IN, 0, 0,
			      buf, size, USB_CTRL_SET_TIMEOUT);

	memcpy(version, buf, size);
	kfree(buf);

	return ret;
}

int get_rome_version(struct usb_device *udev, struct ath3k_version *version)
{
	struct ath3k_version fw_version;
	int ret = -1;

	if (!version) {
		BT_ERR("NULL output parameters");
		return ret;
	}

	ret = ath3k_get_version(udev, &fw_version);
	if (ret < 0) {
		BT_ERR("Failed to get Rome Firmware version");
		return ret;
	}

	switch (fw_version.rom_version) {
	case ROME1_1_USB_CHIP_VERSION:
	case ROME2_1_USB_CHIP_VERSION:
	case ROME3_0_USB_CHIP_VERSION:
	case ROME3_2_USB_CHIP_VERSION:
		memcpy(version, &fw_version, sizeof(struct ath3k_version));
		ret = 0;
		break;
	default:
		BT_ERR("Unsupported ROME USB version");
		ret = -1;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(get_rome_version);

static int ath3k_load_fwfile(struct usb_device *udev,
		const struct firmware *firmware, int header_h)
{
	u8 *send_buf;
	int err, pipe, len, size, count, sent = 0;
	int ret;

	count = firmware->size;

	send_buf = kmalloc(BULK_SIZE, GFP_KERNEL);
	if (!send_buf) {
		BT_ERR("Can't allocate memory chunk for firmware");
		return -ENOMEM;
	}

	size = min_t(uint, count, header_h);
	memcpy(send_buf, firmware->data, size);

	pipe = usb_sndctrlpipe(udev, 0);
	ret = usb_control_msg(udev, pipe, ATH3K_DNLOAD,
			USB_TYPE_VENDOR, 0, 0, send_buf,
			size, USB_CTRL_SET_TIMEOUT);
	if (ret < 0) {
		BT_ERR("Can't change to loading configuration err");
		kfree(send_buf);
		return ret;
	}

	sent += size;
	count -= size;

	while (count) {
		/* workaround the compatibility issue with xHCI controller*/
		usleep_range(TIMEGAP_USEC_MIN, TIMEGAP_USEC_MAX);

		size = min_t(uint, count, BULK_SIZE);
		pipe = usb_sndbulkpipe(udev, 0x02);

		memcpy(send_buf, firmware->data + sent, size);

		err = usb_bulk_msg(udev, pipe, send_buf, size,
					&len, 3000);
		if (err || (len != size)) {
			BT_ERR("Error in firmware loading err = %d,"
				"len = %d, size = %d", err, len, size);
			kfree(send_buf);
			return err;
		}
		sent  += size;
		count -= size;
	}

	kfree(send_buf);
	return 0;
}

static int ath3k_switch_pid(struct usb_device *udev)
{
	int pipe = 0;

	pipe = usb_sndctrlpipe(udev, 0);
	return usb_control_msg(udev, pipe, USB_REG_SWITCH_VID_PID,
			USB_TYPE_VENDOR, 0, 0,
			NULL, 0, USB_CTRL_SET_TIMEOUT);
}

static int ath3k_set_normal_mode(struct usb_device *udev)
{
	unsigned char fw_state;
	int pipe = 0, ret;

	ret = ath3k_get_state(udev, &fw_state);
	if (ret < 0) {
		BT_ERR("Can't get state to change to normal mode err");
		return ret;
	}

	if ((fw_state & ATH3K_MODE_MASK) == ATH3K_NORMAL_MODE) {
		BT_DBG("firmware was already in normal mode");
		return 0;
	}

	pipe = usb_sndctrlpipe(udev, 0);
	return usb_control_msg(udev, pipe, ATH3K_SET_NORMAL_MODE,
			USB_TYPE_VENDOR, 0, 0,
			NULL, 0, USB_CTRL_SET_TIMEOUT);
}

static int ath3k_load_patch(struct usb_device *udev,
						struct ath3k_version *version)
{
	unsigned char fw_state;
	char filename[ATH3K_NAME_LEN] = {0};
	const struct firmware *firmware;
	struct ath3k_version pt_version;
	struct rome2_1_version *rome2_1_version;
	struct rome1_1_version *rome1_1_version;
	int ret;

	ret = ath3k_get_state(udev, &fw_state);
	if (ret < 0) {
		BT_ERR("Can't get state to change to load ram patch err");
		return ret;
	}

	if ((fw_state == ATH3K_PATCH_UPDATE) ||
		(fw_state == ATH3K_PATCH_SYSCFG_UPDATE)) {
		BT_INFO("%s: Patch already downloaded(fw_state: %d)", __func__,
			fw_state);
		return 0;
	} else
		BT_DBG("%s: Downloading RamPatch(fw_state: %d)", __func__,
			fw_state);

	switch (version->rom_version) {
	case ROME1_1_USB_CHIP_VERSION:
		BT_DBG("Chip Detected as ROME1.1");
		snprintf(filename, ATH3K_NAME_LEN, ROME1_1_USB_RAMPATCH_FILE);
		break;
	case ROME2_1_USB_CHIP_VERSION:
		BT_DBG("Chip Detected as ROME2.1");
		snprintf(filename, ATH3K_NAME_LEN, ROME2_1_USB_RAMPATCH_FILE);
		break;
	case ROME3_0_USB_CHIP_VERSION:
		BT_DBG("Chip Detected as ROME3.0");
		snprintf(filename, ATH3K_NAME_LEN, ROME3_0_USB_RAMPATCH_FILE);
		break;
	case ROME3_2_USB_CHIP_VERSION:
		if (udev->descriptor.idProduct == TF1_1_USB_PRODUCT_ID) {
			BT_DBG("Chip Detected as TF1.1");
			snprintf(filename, ATH3K_NAME_LEN,
						TF1_1_USB_RAMPATCH_FILE);
		} else {
			BT_INFO("Unsupported Chip");
			return -ENODEV;
		}
		break;
	default:
		BT_DBG("Chip Detected as Ath3k");
		snprintf(filename, ATH3K_NAME_LEN, "ar3k/AthrBT_0x%08x.dfu",
		version->rom_version);
		break;
	}

	ret = request_firmware(&firmware, filename, &udev->dev);
	if (ret < 0) {
		BT_ERR("Patch file not found %s", filename);
		return ret;
	}

	if ((version->rom_version == ROME2_1_USB_CHIP_VERSION) ||
		(version->rom_version == ROME3_0_USB_CHIP_VERSION) ||
		(version->rom_version == ROME3_2_USB_CHIP_VERSION)) {
		rome2_1_version = (struct rome2_1_version *) firmware->data;
		pt_version.rom_version = rome2_1_version->build_ver;
		pt_version.build_version = rome2_1_version->patch_ver;
		BT_DBG("pt_ver.rome_ver : 0x%x", pt_version.rom_version);
		BT_DBG("pt_ver.build_ver: 0x%x", pt_version.build_version);
		BT_DBG("fw_ver.rom_ver: 0x%x", version->rom_version);
		BT_DBG("fw_ver.build_ver: 0x%x", version->build_version);
	} else if (version->rom_version == ROME1_1_USB_CHIP_VERSION) {
		rome1_1_version = (struct rome1_1_version *) firmware->data;
		pt_version.build_version = rome1_1_version->build_ver;
		pt_version.rom_version = rome1_1_version->patch_ver;
		BT_DBG("pt_ver.rom1.1_ver : 0x%x", pt_version.rom_version);
		BT_DBG("pt_ver.build1.1_ver: 0x%x", pt_version.build_version);
		BT_DBG("fw_ver.rom1.1_ver: 0x%x", version->rom_version);
		BT_DBG("fw_ver.build1.1_ver: 0x%x", version->build_version);
	} else {
		pt_version.rom_version = *(int *)(firmware->data +
						firmware->size - 8);
		pt_version.build_version = *(int *)
		(firmware->data + firmware->size - 4);
	}
	if ((pt_version.rom_version != version->rom_version) ||
		(pt_version.build_version <= version->build_version)) {
		BT_ERR("Patch file version did not match with firmware");
		release_firmware(firmware);
		return -EINVAL;
	}

	if ((version->rom_version == ROME2_1_USB_CHIP_VERSION) ||
		(version->rom_version == ROME3_0_USB_CHIP_VERSION) ||
		(version->rom_version == ROME3_2_USB_CHIP_VERSION))
		ret = ath3k_load_fwfile(udev, firmware,
						ROME2_1_USB_RAMPATCH_HEADER);
	else if (version->rom_version == ROME1_1_USB_CHIP_VERSION)
		ret = ath3k_load_fwfile(udev, firmware,
						 ROME1_1_USB_RAMPATCH_HEADER);
	else
		ret = ath3k_load_fwfile(udev, firmware, FW_HDR_SIZE);

	release_firmware(firmware);

	return ret;
}

static int ath3k_load_syscfg(struct usb_device *udev,
						struct ath3k_version *version)
{
	unsigned char fw_state;
	char filename[ATH3K_NAME_LEN] = {0};
	const struct firmware *firmware;
	int clk_value, ret;

	ret = ath3k_get_state(udev, &fw_state);
	if (ret < 0) {
		BT_ERR("Can't get state to change to load configuration err");
		return -EBUSY;
	}

	if ((fw_state == ATH3K_SYSCFG_UPDATE) ||
		(fw_state == ATH3K_PATCH_SYSCFG_UPDATE)) {
		BT_INFO("%s: NVM already downloaded(fw_state: %d)", __func__,
			fw_state);
		return 0;
	} else
		BT_DBG("%s: Downloading NVM(fw_state: %d)", __func__, fw_state);

	switch (version->ref_clock) {
	case ATH3K_XTAL_FREQ_26M:
		clk_value = 26;
		break;
	case ATH3K_XTAL_FREQ_40M:
		clk_value = 40;
		break;
	case ATH3K_XTAL_FREQ_19P2:
		clk_value = 19;
		break;
	default:
		clk_value = 0;
		break;
	}

	if (version->rom_version == ROME2_1_USB_CHIP_VERSION)
		snprintf(filename, ATH3K_NAME_LEN, ROME2_1_USB_NVM_FILE);
	else if (version->rom_version == ROME3_0_USB_CHIP_VERSION)
		snprintf(filename, ATH3K_NAME_LEN, ROME3_0_USB_NVM_FILE);
	else if (version->rom_version == ROME3_2_USB_CHIP_VERSION) {
		if (udev->descriptor.idProduct == TF1_1_USB_PRODUCT_ID)
			snprintf(filename, ATH3K_NAME_LEN, TF1_1_USB_NVM_FILE);
		else {
			BT_INFO("Unsupported Chip");
			return -ENODEV;
		}
	}
	else if (version->rom_version == ROME1_1_USB_CHIP_VERSION)
		snprintf(filename, ATH3K_NAME_LEN, ROME1_1_USB_NVM_FILE);
	else
		snprintf(filename, ATH3K_NAME_LEN, "ar3k/ramps_0x%08x_%d%s",
			version->rom_version, clk_value, ".dfu");

	ret = request_firmware(&firmware, filename, &udev->dev);
	if (ret < 0) {
		BT_ERR("Configuration file not found %s", filename);
		return ret;
	}

	if ((version->rom_version == ROME2_1_USB_CHIP_VERSION) ||
		(version->rom_version == ROME3_0_USB_CHIP_VERSION) ||
		(version->rom_version == ROME3_2_USB_CHIP_VERSION))
		ret = ath3k_load_fwfile(udev, firmware, ROME2_1_USB_NVM_HEADER);
	else if (version->rom_version == ROME1_1_USB_CHIP_VERSION)
		ret = ath3k_load_fwfile(udev, firmware, ROME1_1_USB_NVM_HEADER);
	else
		ret = ath3k_load_fwfile(udev, firmware, FW_HDR_SIZE);

	release_firmware(firmware);

	return ret;
}

int rome_download(struct usb_device *udev, struct ath3k_version *version)
{
	int ret;

	ret = ath3k_load_patch(udev, version);
	if (ret < 0) {
		BT_ERR("Loading patch file failed");
		return ret;
	}
	ret = ath3k_load_syscfg(udev, version);
	if (ret < 0) {
		BT_ERR("Loading sysconfig file failed");
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL(rome_download);

static int ath3k_probe(struct usb_interface *intf,
			const struct usb_device_id *id)
{
	const struct firmware *firmware;
	struct usb_device *udev = interface_to_usbdev(intf);
	int ret;
	struct ath3k_version version;

	BT_DBG("intf %p id %p", intf, id);

	if (intf->cur_altsetting->desc.bInterfaceNumber != 0)
		return -ENODEV;

	ret = get_rome_version(udev, &version);
	if (!ret) {
		BT_INFO("Rome detected, fw dnld will be triggered from btusb");
		return -ENODEV;
	}

	/* match device ID in ath3k blacklist table */
	if (!id->driver_info) {
		const struct usb_device_id *match;
		match = usb_match_id(intf, ath3k_blist_tbl);
		if (match)
			id = match;
	}

	/* load patch and sysconfig files for AR3012 */
	if (id->driver_info & BTUSB_ATH3012) {

		/* New firmware with patch and sysconfig files already loaded */
		if (le16_to_cpu(udev->descriptor.bcdDevice) > 0x0001)
			return -ENODEV;

		ret = ath3k_load_patch(udev, &version);
		if (ret < 0) {
			BT_ERR("Loading patch file failed");
			return ret;
		}
		ret = ath3k_load_syscfg(udev, &version);
		if (ret < 0) {
			BT_ERR("Loading sysconfig file failed");
			return ret;
		}
		ret = ath3k_set_normal_mode(udev);
		if (ret < 0) {
			BT_ERR("Set normal mode failed");
			return ret;
		}
		ath3k_switch_pid(udev);
		return 0;
	}

	ret = request_firmware(&firmware, ATH3K_FIRMWARE, &udev->dev);
	if (ret < 0) {
		if (ret == -ENOENT)
			BT_ERR("Firmware file \"%s\" not found",
							ATH3K_FIRMWARE);
		else
			BT_ERR("Firmware file \"%s\" request failed (err=%d)",
							ATH3K_FIRMWARE, ret);
		return ret;
	}

	ret = ath3k_load_firmware(udev, firmware);
	release_firmware(firmware);

	return ret;
}

static void ath3k_disconnect(struct usb_interface *intf)
{
	BT_DBG("ath3k_disconnect intf %p", intf);
}

static struct usb_driver ath3k_driver = {
	.name		= "ath3k",
	.probe		= ath3k_probe,
	.disconnect	= ath3k_disconnect,
	.id_table	= ath3k_table,
	.disable_hub_initiated_lpm = 1,
};

module_usb_driver(ath3k_driver);

MODULE_AUTHOR("Atheros Communications");
MODULE_DESCRIPTION("Atheros AR30xx firmware driver");
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
MODULE_FIRMWARE(ATH3K_FIRMWARE);
