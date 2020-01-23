/*
 *  Bluetooth supports for Qualcomm Atheros chips
 *
 *  Copyright (c) 2017 The Linux Foundation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation
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
#include <linux/firmware.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include <linux/ctype.h>

#include "btqca.h"
#include "hci_uart.h"

#define VERSION "0.1"

#define MAX_PATCH_FILE_SIZE (200*1024)
#define MAX_NVM_FILE_SIZE   (10*1024)

#define QCA_BT_ADDR_FORMAT	"%04x:%02x:%06x"

#define QCA_BT_ADDR_FIELD_COUNT	3

bdaddr_t qca_bdaddr = { { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } };

qca_enque_send_callback qca_enq_send_cb;

u32 qca_bt_version;

static int wait_for_sending(struct hci_dev *hdev, int count)
{
	struct hci_uart *hu = hci_get_drvdata(hdev);

	while (test_bit(HCI_UART_SENDING, &hu->tx_state)) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(1));
		set_current_state(TASK_RUNNING);
		if (-- count <= 0)
			return -ETIMEDOUT;
	}
	return 0;
}

static int qca_patch_ver_req(struct hci_dev *hdev, u32 *qca_ver)
{
	struct sk_buff *skb;
	struct edl_hst_event_hdr *hst_edl;
	struct edl_event_hdr *rome_edl;
	struct qca_version *ver;
	char cmd;
	int err = 0;

	BT_DBG("%s: QCA Patch Version Request", hdev->name);

	cmd = EDL_PATCH_VER_REQ_CMD;
	skb = __hci_cmd_sync_ev(hdev, EDL_PATCH_CMD_OPCODE, EDL_PATCH_CMD_LEN,
				&cmd, HCI_VENDOR_PKT, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		err = PTR_ERR(skb);
		BT_ERR("%s: Failed to read version of QCA (%d)", hdev->name,
		       err);
		return err;
	}

	if (skb->data[0] != 0x00) {
		BT_ERR("%s: Wrong packet received skb->data[0] = 0x%x",
				hdev->name, skb->data[0]);
		err = -EIO;
		goto out;
	} else if (skb->data[1] == EDL_APP_VER_RES_EVT) {
		rome_edl = (struct edl_event_hdr *)(skb->data);
		ver = (struct qca_version *)(rome_edl->data);
		cancel_delayed_work(&hdev->cmd_timer);
		BT_DBG("%s: QCA Got VSE", hdev->name);
	} else if (skb->data[1] == EDL_PATCH_VER_RES_EVT) {
		hst_edl = (struct edl_hst_event_hdr *)(skb->data);
		ver = (struct qca_version *)(hst_edl->data);
		BT_DBG("%s: QCA Got CC", hdev->name);
	} else {
		BT_ERR("%s: Wrong packet received skb->data[1] = 0x%x",
				hdev->name, skb->data[1]);
		err = -EIO;
		goto out;
	}
	BT_INFO("%s: QCA Chip Version info:", hdev->name);
	BT_INFO("%s: Product:0x%08x", hdev->name, le32_to_cpu(ver->product_id));
	BT_INFO("%s: Patch  :0x%08x", hdev->name, le16_to_cpu(ver->patch_ver));
	BT_INFO("%s: ROM    :0x%08x", hdev->name, le16_to_cpu(ver->rome_ver));
	BT_INFO("%s: SOC    :0x%08x", hdev->name, le32_to_cpu(ver->soc_id));

	*qca_ver = (le32_to_cpu(ver->soc_id) << 16) |
			(le16_to_cpu(ver->rome_ver) & 0x0000ffff);

out:
	kfree_skb(skb);

	return err;
}

static int qca_reset(struct hci_dev *hdev)
{
	struct sk_buff *skb;
	int err;

	BT_DBG("%s: QCA HCI_RESET", hdev->name);

	skb = __hci_cmd_sync(hdev, HCI_OP_RESET, 0, NULL, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		err = PTR_ERR(skb);
		BT_ERR("%s: Reset failed (%d)", hdev->name, err);
		return err;
	}

	kfree_skb(skb);

	return 0;
}

static void qca_tlv_check_data(struct rome_config *config,
				const struct firmware *fw)
{
	const u8 *data;
	u32 type_len;
	u16 tag_id, tag_len;
	int idx, length;
	struct tlv_type_hdr *tlv;
	struct tlv_type_patch *tlv_patch;
	struct tlv_type_nvm *tlv_nvm;

	tlv = (struct tlv_type_hdr *)fw->data;

	type_len = le32_to_cpu(tlv->type_len);
	length = (type_len >> 8) & 0x00ffffff;

	BT_DBG("TLV Type\t\t : 0x%x", type_len & 0x000000ff);
	BT_DBG("Length\t\t : %d bytes", length);

	switch (config->type) {
	case TLV_TYPE_PATCH:
		tlv_patch = (struct tlv_type_patch *)tlv->data;
		BT_DBG("Total Length\t\t : %d bytes",
		       le32_to_cpu(tlv_patch->total_size));
		BT_DBG("Patch Data Length\t : %d bytes",
		       le32_to_cpu(tlv_patch->data_length));
		BT_DBG("Signing Format Version : 0x%x",
		       tlv_patch->format_version);
		BT_DBG("Signature Algorithm\t : 0x%x",
		       tlv_patch->signature);
		BT_DBG("Reserved\t\t : 0x%x",
		       le16_to_cpu(tlv_patch->reserved1));
		BT_DBG("Product ID\t\t : 0x%04x",
		       le16_to_cpu(tlv_patch->product_id));
		BT_DBG("Rom Build Version\t : 0x%04x",
		       le16_to_cpu(tlv_patch->rom_build));
		BT_DBG("Patch Version\t\t : 0x%04x",
		       le16_to_cpu(tlv_patch->patch_version));
		BT_DBG("Reserved\t\t : 0x%x",
		       le16_to_cpu(tlv_patch->reserved2));
		BT_DBG("Patch Entry Address\t : 0x%x",
		       le32_to_cpu(tlv_patch->entry));
		break;

	case TLV_TYPE_NVM:
		idx = 0;
		data = tlv->data;
		while (idx < length) {
			tlv_nvm = (struct tlv_type_nvm *)(data + idx);

			tag_id = le16_to_cpu(tlv_nvm->tag_id);
			tag_len = le16_to_cpu(tlv_nvm->tag_len);

			/* Update NVM tags as needed */
			switch (tag_id) {
			case EDL_TAG_ID_BD_ADDRESS:
				if (bacmp(&qca_bdaddr, BDADDR_NONE) != 0)
					memcpy(tlv_nvm->data, &qca_bdaddr, 6);
				break;

			case EDL_TAG_ID_HCI:
				if (qca_bt_version == ROME_VER_3_2) {
				/* enable/disable software inband sleep */
#ifdef SUPPORT_BT_QCA_SIBS
					tlv_nvm->data[0] |= 0x80;
#else
					tlv_nvm->data[0] &= ~0x80;
#endif
					/* UART Baud Rate */
					tlv_nvm->data[2] =
						config->user_baud_rate;
				} else if (qca_bt_version == HST_VER_2_0)
					tlv_nvm->data[1] =
						config->user_baud_rate;
				break;

			case EDL_TAG_ID_DEEP_SLEEP:
			/* Sleep enable mask
			 * enabling/disabling deep sleep feature on controller.
			 */
#ifdef SUPPORT_BT_QCA_SIBS
				tlv_nvm->data[0] |= 0x01;
#else
				tlv_nvm->data[0] &= ~0x01;
#endif
				/* enable software inband sleep */
				if (qca_bt_version == HST_VER_2_0)
#ifdef SUPPORT_BT_QCA_SIBS
					tlv_nvm->data[1] |= 0x01;
#else
					tlv_nvm->data[1] &= ~0x01;
#endif
				break;
			}

			idx += (sizeof(u16) + sizeof(u16) + 8 + tag_len);
		}
		break;

	default:
		BT_ERR("Unknown TLV type %d", config->type);
		break;
	}
}

static int qca_tlv_send_segment_optimised(struct hci_dev *hdev,
			int idx, int seg_size, const u8 *data)
{
	struct sk_buff *skb;
	u8 param[MAX_SIZE_PER_TLV_SEGMENT + 2];
	int len = HCI_COMMAND_HDR_SIZE + seg_size + 2;
	struct hci_command_hdr *hdr;
	u32 plen;
	int err = 0;

	BT_DBG("%s: QCA Download segment #%d size %d", hdev->name,
		idx, seg_size);

	plen = seg_size + 2;

	skb = bt_skb_alloc(len, GFP_ATOMIC);
	if (!skb) {
		BT_ERR("Failed to allocate memory to send segment");
		return -ENOMEM;
	}

	hdr = skb_put(skb, HCI_COMMAND_HDR_SIZE);
	hdr->opcode = cpu_to_le16(EDL_PATCH_CMD_OPCODE);
	hdr->plen   = plen;

	param[0] = EDL_PATCH_TLV_REQ_CMD;
	param[1] = seg_size;
	memcpy(param + 2, data, seg_size);

	skb_put_data(skb, param, plen);

	hci_skb_pkt_type(skb) = HCI_COMMAND_PKT;
	hci_skb_opcode(skb) = EDL_PATCH_CMD_OPCODE;

	if (!qca_enq_send_cb) {
		BT_ERR("No send callback");
		return -EUNATCH;
	}

	err = wait_for_sending(hdev, 2000);
	if (err) {
		BT_ERR("wait timeout before send");
		return err;
	}

	err = qca_enq_send_cb(hdev, skb);
	if (err) {
		BT_ERR("send failed");
		return -EIO;
	}

	err = wait_for_sending(hdev, 2000);
	if (err) {
		BT_ERR("wait timeout after send");
		return err;
	}

	return err;
}

static int hst_tlv_send_segment_sync(struct hci_dev *hdev, int idx,
					int seg_size, const u8 *data)
{
	struct sk_buff *skb;
	u8 cmd[MAX_SIZE_PER_TLV_SEGMENT + 2];
	int err = 0;

	BT_DBG("%s: HST Download segment #%d size %d", hdev->name,
		idx, seg_size);

	cmd[0] = EDL_PATCH_TLV_REQ_CMD;
	cmd[1] = seg_size;
	memcpy(cmd + 2, data, seg_size);

	skb = __hci_cmd_sync_ev(hdev, EDL_PATCH_CMD_OPCODE, seg_size + 2, cmd,
				HCI_VENDOR_PKT, HCI_INIT_TIMEOUT);

	if (IS_ERR(skb)) {
		err = PTR_ERR(skb);
		BT_ERR("%s: Failed to send TLV segment (%d)", hdev->name, err);
		return err;
	}

	if (skb->data[0] != 0x00 ||
		skb->data[1] != EDL_PATCH_TLV_REQ_CMD) {
		BT_ERR("%s: Get error reply");
		err = -EIO;
	}

	kfree_skb(skb);
	return err;
}

static int rome_tlv_send_segment_sync(struct hci_dev *hdev, int idx,
					int seg_size, const u8 *data)
{
	struct sk_buff *skb;
	struct edl_event_hdr *edl;
	struct tlv_seg_resp *tlv_resp;
	u8 cmd[MAX_SIZE_PER_TLV_SEGMENT + 2];
	int err = 0;

	BT_DBG("%s: ROME Download segment #%d size %d", hdev->name,
		idx, seg_size);

	cmd[0] = EDL_PATCH_TLV_REQ_CMD;
	cmd[1] = seg_size;
	memcpy(cmd + 2, data, seg_size);

	skb = __hci_cmd_sync_ev(hdev, EDL_PATCH_CMD_OPCODE, seg_size + 2, cmd,
				HCI_VENDOR_PKT, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		err = PTR_ERR(skb);
		BT_ERR("%s: Failed to send TLV segment (%d)", hdev->name, err);
		return err;
	}

	if (skb->len != sizeof(*edl) + sizeof(*tlv_resp)) {
		BT_ERR("%s: TLV response size mismatch", hdev->name);
		err = -EILSEQ;
		goto out;
	}

	edl = (struct edl_event_hdr *)(skb->data);
	if (!edl) {
		BT_ERR("%s: TLV with no header", hdev->name);
		err = -EILSEQ;
		goto out;
	}

	tlv_resp = (struct tlv_seg_resp *)(edl->data);

	if (edl->cresp != EDL_CMD_REQ_RES_EVT ||
	    edl->rtype != EDL_TVL_DNLD_RES_EVT || tlv_resp->result != 0x00) {
		BT_ERR("%s: TLV with error stat 0x%x rtype 0x%x (0x%x)",
		       hdev->name, edl->cresp, edl->rtype, tlv_resp->result);
		err = -EIO;
	}

out:
	cancel_delayed_work(&hdev->cmd_timer);
	kfree_skb(skb);
	msleep(20);

	return err;
}

static int rome_tlv_download_request(struct hci_dev *hdev,
		const struct firmware *fw, struct rome_config *config)
{
	const u8 *buffer, *data;
	int total_segment, remain_size;
	int ret, i;

	if (!fw || !fw->data)
		return -EINVAL;

	total_segment = fw->size / MAX_SIZE_PER_TLV_SEGMENT;
	remain_size = fw->size % MAX_SIZE_PER_TLV_SEGMENT;

	BT_INFO("%s: Total segment num %d remain size %d total size %zu",
			hdev->name, total_segment, remain_size, fw->size);

	data = fw->data;
	if (config->type == TLV_TYPE_PATCH) {
		for (i = 0; i < total_segment; i++) {
			buffer = data + i * MAX_SIZE_PER_TLV_SEGMENT;
			ret = qca_tlv_send_segment_optimised(hdev, i,
					MAX_SIZE_PER_TLV_SEGMENT, buffer);
			if (ret < 0)
				return -EIO;
		}

		if (remain_size) {
			buffer = data +
				total_segment *	MAX_SIZE_PER_TLV_SEGMENT;
			ret = rome_tlv_send_segment_sync(hdev, total_segment,
						 remain_size, buffer);
			if (ret < 0)
				return -EIO;
		}
	} else if (config->type == TLV_TYPE_NVM) {
		for (i = 0; i < total_segment; i++) {
			buffer = data + i * MAX_SIZE_PER_TLV_SEGMENT;
			ret = qca_tlv_send_segment_optimised(hdev, i,
					MAX_SIZE_PER_TLV_SEGMENT, buffer);
			if (ret < 0)
				return -EIO;
		}

		if (remain_size) {
			buffer = data +
				total_segment * MAX_SIZE_PER_TLV_SEGMENT;
			ret = rome_tlv_send_segment_sync(hdev, total_segment,
					remain_size, buffer);
			if (ret < 0)
				return -EIO;
		}
	}
	return 0;
}

static int hst_tlv_download_request(struct hci_dev *hdev,
		const struct firmware *fw, struct rome_config *config)
{
	const u8 *buffer, *data;
	int total_segment, remain_size;
	int ret, i;

	if (!fw || !fw->data)
		return -EINVAL;

	total_segment = fw->size / MAX_SIZE_PER_TLV_SEGMENT;
	remain_size = fw->size % MAX_SIZE_PER_TLV_SEGMENT;

	BT_DBG("%s: Total segment num %d remain size %d total size %zu",
	       hdev->name, total_segment, remain_size, fw->size);

	data = fw->data;
	if (config->type == TLV_TYPE_PATCH) {
		for (i = 0; i < total_segment; i++) {
			buffer = data + i * MAX_SIZE_PER_TLV_SEGMENT;
			ret = qca_tlv_send_segment_optimised(hdev, i,
					MAX_SIZE_PER_TLV_SEGMENT, buffer);
			if (ret < 0)
				return -EIO;
		}

		if (remain_size) {
			buffer = data +
				total_segment * MAX_SIZE_PER_TLV_SEGMENT;
			ret = hst_tlv_send_segment_sync(hdev, total_segment,
						remain_size, buffer);
			if (ret < 0)
				return -EIO;
		}
	} else if (config->type == TLV_TYPE_NVM) {
		for (i = 0; i < total_segment; i++) {
			buffer = data + i * MAX_SIZE_PER_TLV_SEGMENT;
			ret = hst_tlv_send_segment_sync(hdev, i,
					MAX_SIZE_PER_TLV_SEGMENT, buffer);
			if (ret < 0)
				return -EIO;
		}

		if (remain_size) {
			buffer = data +
				total_segment * MAX_SIZE_PER_TLV_SEGMENT;
			ret = hst_tlv_send_segment_sync(hdev, total_segment,
						remain_size, buffer);
			if (ret < 0)
				return -EIO;
		}
	}

	return 0;
}


static int qca_download_firmware(struct hci_dev *hdev,
				  struct rome_config *config)
{
	const struct firmware *fw;
	u32 type_len, length;
	struct tlv_type_hdr *tlv;
	int ret;

	BT_INFO("%s: QCA Downloading file: %s", hdev->name, config->fwname);
	ret = request_firmware(&fw, config->fwname, &hdev->dev);

	if (ret || !fw || !fw->data || fw->size <= 0) {
		BT_ERR("Failed to request file: err = (%d)", ret);
		ret = ret ? ret : -EINVAL;
		return ret;
	}

	if (config->type != TLV_TYPE_NVM &&
		config->type != TLV_TYPE_PATCH) {
		ret = -EINVAL;
		BT_ERR("TLV_NVM dload: wrong config type selected");
		goto exit;
	}

	if (config->type == TLV_TYPE_PATCH &&
		(fw->size > MAX_PATCH_FILE_SIZE)) {
		ret = -EINVAL;
		BT_ERR("TLV_PATCH dload: wrong patch file sizes");
		goto exit;
	} else if (config->type == TLV_TYPE_NVM &&
		(fw->size > MAX_NVM_FILE_SIZE)) {
		ret = -EINVAL;
		BT_ERR("TLV_NVM dload: wrong NVM file sizes");
		goto exit;
	}

	if (fw->size < sizeof(struct tlv_type_hdr)) {
		ret = -EINVAL;
		BT_ERR("Firmware size smaller to fit minimum value");
		goto exit;
	}

	tlv = (struct tlv_type_hdr *)fw->data;
	type_len = le32_to_cpu(tlv->type_len);
	length = (type_len >> 8) & 0x00ffffff;

	if (fw->size - 4 != length) {
		ret = -EINVAL;
		BT_ERR("Requested size not matching size in header");
		goto exit;
	}

	qca_tlv_check_data(config, fw);
	if (qca_bt_version == ROME_VER_3_2)
		ret = rome_tlv_download_request(hdev, fw, config);
	else if (qca_bt_version == HST_VER_2_0)
		ret = hst_tlv_download_request(hdev, fw, config);

	if (ret)
		BT_ERR("Failed to download FW: error = (%d)", ret);
	else
		BT_INFO("%s: QCA %s download is completed", hdev->name,
			(config->type == TLV_TYPE_PATCH) ? "Patch" : "NVM");

exit:
	release_firmware(fw);
	return ret;
}

int qca_set_bdaddr_rome(struct hci_dev *hdev, const bdaddr_t *bdaddr)
{
	struct sk_buff *skb;
	u8 cmd[9];
	int err;

	cmd[0] = EDL_NVM_ACCESS_SET_REQ_CMD;
	/* Set the TAG ID of 0x02 for NVM set and size of tag */
	cmd[1] = 0x02;
	cmd[2] = sizeof(bdaddr_t);
	memcpy(cmd + 3, bdaddr, sizeof(bdaddr_t));
	skb = __hci_cmd_sync_ev(hdev, EDL_NVM_ACCESS_OPCODE, sizeof(cmd), cmd,
				HCI_VENDOR_PKT, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		err = PTR_ERR(skb);
		BT_ERR("%s: Change address command failed (%d)",
		       hdev->name, err);
		return err;
	}

	kfree_skb(skb);

	return 0;
}
EXPORT_SYMBOL_GPL(qca_set_bdaddr_rome);

static char *qca_bda;

module_param_named(QCA_BDA, qca_bda, charp, 0444);

static void qca_get_bda(struct hci_dev *hdev, const char *str, bdaddr_t *bda)
{
	u32 nap = 0, uap = 0, lap = 0;

	if (!bda)
		return;

	BT_INFO("%s: QCA_BDA=%s", hdev->name, str);

	if ((str != NULL) && (sscanf(str, QCA_BT_ADDR_FORMAT, &nap, &uap, &lap)
		== QCA_BT_ADDR_FIELD_COUNT)) {

		bda->b[0] = (u8)(lap & 0xff);
		bda->b[1] = (u8)((lap >> 8) & 0xff);
		bda->b[2] = (u8)((lap >> 16) & 0xff);

		bda->b[3] = (u8)(uap & 0xff);

		bda->b[4] = (u8)(nap & 0xff);
		bda->b[5] = (u8)((nap >> 8) & 0xff);
	} else {
		BT_INFO("No valid BDA, create random one");

		bda->b[5] = 0x00;
		bda->b[4] = 0x02;
		bda->b[3] = 0x5b;
		get_random_bytes(bda, 3);
	}

	BT_INFO("%s: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x", hdev->name, bda->b[5],
		bda->b[4], bda->b[3], bda->b[2], bda->b[1], bda->b[0]);
}

int qca_uart_setup_rome(struct hci_dev *hdev, uint8_t baudrate,
				qca_enque_send_callback callback)
{
	struct rome_config config;
	u32 qca_ver = 0;
	int err;

	BT_DBG("%s: QCA setup on UART", hdev->name);

	if (callback == NULL) {
		BT_ERR("%s: No send callback", hdev->name);
		return -EUNATCH;
	}

	qca_enq_send_cb = callback;

	config.user_baud_rate = baudrate;

	qca_get_bda(hdev, qca_bda, &qca_bdaddr);

	err = qca_patch_ver_req(hdev, &qca_ver);
	if (err < 0 || qca_ver == 0) {
		BT_ERR("%s: Failed to get version 0x%x", hdev->name, err);
		return err;
	}

	BT_INFO("%s: QCA controller version 0x%08x", hdev->name, qca_ver);

	if (!(qca_ver == ROME_VER_3_2 || qca_ver == HST_VER_2_0)) {
		BT_ERR("%s: Not supported chip version 0x%x",
						hdev->name, qca_ver);
		return -EUNATCH;
	}

	qca_bt_version = qca_ver;
	/* Download rampatch file */
	config.type = TLV_TYPE_PATCH;
	if (qca_ver == ROME_VER_3_2)
		snprintf(config.fwname, sizeof(config.fwname),
					"image/btfw32.tlv", qca_ver);
	else if (qca_ver == HST_VER_2_0)
		snprintf(config.fwname, sizeof(config.fwname),
					"image/htbtfw20.tlv", qca_ver);

	err = qca_download_firmware(hdev, &config);
	if (err < 0) {
		BT_ERR("%s: Failed to download patch (%d)", hdev->name, err);
		return err;
	}

	/* Give the controller some time to get ready to receive the NVM */
	msleep(10);

	/* Download NVM configuration */
	config.type = TLV_TYPE_NVM;
	if (qca_ver == ROME_VER_3_2)
		snprintf(config.fwname, sizeof(config.fwname),
					"image/btnv32.bin", qca_ver);
	else if (qca_ver == HST_VER_2_0)
		snprintf(config.fwname, sizeof(config.fwname),
					 "image/htnv20.bin", qca_ver);

	err = qca_download_firmware(hdev, &config);
	if (err < 0) {
		BT_ERR("%s: Failed to download NVM (%d)", hdev->name, err);
		return err;
	}

	err = qca_patch_ver_req(hdev, &qca_ver);
	if (err < 0 || qca_ver == 0) {
		BT_ERR("%s: Failed to get version 0x%x", hdev->name, err);
		return err;
	}

	/* Perform HCI reset */
	err = qca_reset(hdev);
	if (err < 0) {
		BT_ERR("%s: Failed to run HCI_RESET (%d)", hdev->name, err);
		return err;
	}

	msleep(100);
	BT_INFO("%s: QCA setup on UART is completed", hdev->name);

	return 0;
}
EXPORT_SYMBOL_GPL(qca_uart_setup_rome);

MODULE_AUTHOR("Ben Young Tae Kim <ytkim@qca.qualcomm.com>");
MODULE_DESCRIPTION("Bluetooth support for Qualcomm Atheros family ver " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
