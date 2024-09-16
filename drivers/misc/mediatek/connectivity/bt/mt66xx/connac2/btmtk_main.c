/**
 *  Copyright (c) 2018 MediaTek Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "btmtk_define.h"
#include "btmtk_main.h"
#include "btmtk_drv.h"
#include "btmtk_chip_if.h"

#define MTKBT_UNSLEEPABLE_LOCK(x, y)	spin_lock_irqsave(x, y)
#define MTKBT_UNSLEEPABLE_UNLOCK(x, y)	spin_unlock_irqsave(x, y)
#define HCI_COMMAND_COMPLETE_EVT		0x0E
#define HCI_COMMAND_STATUS_EVT			0x0F

/**
 * Global parameters(mtkbt_)
 */
uint8_t btmtk_log_lvl = BTMTK_LOG_LVL_DEF;
struct bt_dbg_st g_bt_dbg_st;

static int main_init(void)
{
	return 0;
}

static int main_exit(void)
{
	return 0;
}

/* HCI receive mechnism */


static inline struct sk_buff *h4_recv_buf(struct hci_dev *hdev,
					  struct sk_buff *skb,
					  const unsigned char *buffer,
					  int count,
					  const struct h4_recv_pkt *pkts,
					  int pkts_count)
{
	/* Check for error from previous call */
	if (IS_ERR(skb))
		skb = NULL;

	while (count) {
		int i, len;

		if (!skb) {
			for (i = 0; i < pkts_count; i++) {
				if (buffer[0] != (&pkts[i])->type)
					continue;

				skb = bt_skb_alloc((&pkts[i])->maxlen,
						   GFP_ATOMIC);
				if (!skb)
					return ERR_PTR(-ENOMEM);

				hci_skb_pkt_type(skb) = (&pkts[i])->type;
				hci_skb_expect(skb) = (&pkts[i])->hlen;
				break;
			}

			/* Check for invalid packet type */
			if (!skb) {
				BTMTK_ERR("skb is NULL");
				btmtk_set_sleep(hdev, FALSE);
				return ERR_PTR(-EILSEQ);
			}

			count -= 1;
			buffer += 1;
		}

		len = min_t(uint, hci_skb_expect(skb) - skb->len, count);
		memcpy(skb_put(skb, len), buffer, len);
		/*
			If kernel version > 4.x
			skb_put_data(skb, buffer, len);
		*/

		count -= len;
		buffer += len;

		/* Check for partial packet */
		if (skb->len < hci_skb_expect(skb))
			continue;

		for (i = 0; i < pkts_count; i++) {
			if (hci_skb_pkt_type(skb) == (&pkts[i])->type)
				break;
		}

		if (i >= pkts_count) {
			BTMTK_ERR("i (%d) >= pkts_count (%d)", i, pkts_count);
			kfree_skb(skb);
			btmtk_set_sleep(hdev, FALSE);
			return ERR_PTR(-EILSEQ);
		}

		if (skb->len == (&pkts[i])->hlen) {
			u16 dlen;

			switch ((&pkts[i])->lsize) {
			case 0:
				/* No variable data length */
				dlen = 0;
				break;
			case 1:
				/* Single octet variable length */
				dlen = skb->data[(&pkts[i])->loff];
				hci_skb_expect(skb) += dlen;

				if (skb_tailroom(skb) < dlen) {
					BTMTK_ERR("1. skb_tailroom(skb) < dlen (%d)", dlen);
					kfree_skb(skb);
					btmtk_set_sleep(hdev, FALSE);
					return ERR_PTR(-EMSGSIZE);
				}
				break;
			case 2:
				/* Double octet variable length */
				dlen = get_unaligned_le16(skb->data +
							  (&pkts[i])->loff);
				/* parse ISO packet len*/
				if ((&pkts[i])->type == HCI_ISODATA_PKT) {
					unsigned char *cp = (unsigned char *)&dlen + 1;
					*cp = *cp & 0x3F;
				}
				hci_skb_expect(skb) += dlen;

				if (skb_tailroom(skb) < dlen) {
					BTMTK_ERR("2. skb_tailroom(skb) < dlen (%d)", dlen);
					kfree_skb(skb);
					btmtk_set_sleep(hdev, FALSE);
					return ERR_PTR(-EMSGSIZE);
				}
				break;
			default:
				/* Unsupported variable length */
				BTMTK_ERR("Unsupported variable length");
				kfree_skb(skb);
				btmtk_set_sleep(hdev, FALSE);
				return ERR_PTR(-EILSEQ);
			}

			if (!dlen) {
				/* No more data, complete frame */
				(&pkts[i])->recv(hdev, skb);
				btmtk_set_sleep(hdev, FALSE);
				if (skb)
					kfree_skb(skb);
				skb = NULL;
			}
		} else {
			/* Complete frame */
			(&pkts[i])->recv(hdev, skb);
			btmtk_set_sleep(hdev, FALSE);
			if (skb)
				kfree_skb(skb);
			skb = NULL;
		}
	}

	return skb;
}

static const struct h4_recv_pkt mtk_recv_pkts[] = {
	{ H4_RECV_ACL,      .recv = btmtk_recv_acl },
	{ H4_RECV_SCO,      .recv = hci_recv_frame },
	{ H4_RECV_EVENT,    .recv = btmtk_recv_event },
	{ H4_RECV_ISO,      .recv = btmtk_recv_iso },
};
#if ENABLESTP
static inline struct sk_buff *mtk_add_stp(struct btmtk_dev *bdev, struct sk_buff *skb)
{
	struct mtk_stp_hdr *shdr;
	int dlen, err = 0, type = 0;
	u8 stp_crc[] = {0x00, 0x00};

	if (unlikely(skb_headroom(skb) < sizeof(*shdr)) ||
		(skb_tailroom(skb) < MTK_STP_TLR_SIZE)) {
		BTMTK_DBG("%s, add pskb_expand_head, headroom = %d, tailroom = %d",
				__func__, skb_headroom(skb), skb_tailroom(skb));

		err = pskb_expand_head(skb, sizeof(*shdr), MTK_STP_TLR_SIZE,
					   GFP_ATOMIC);
	}
	dlen = skb->len;
	shdr = (void *) skb_push(skb, sizeof(*shdr));
	shdr->prefix = 0x80;
	shdr->dlen = cpu_to_be16((dlen & 0x0fff) | (type << 12));
	shdr->cs = 0;
	// Add the STP trailer
	// kernel version > 4.20
	// skb_put_zero(skb, MTK_STP_TLR_SIZE);
	// kernel version < 4.20
	skb_put(skb, sizeof(stp_crc));

	return skb;
}

static const unsigned char *
mtk_stp_split(struct btmtk_dev *bdev, const unsigned char *data, int count,
	      int *sz_h4)
{
	struct mtk_stp_hdr *shdr;

	/* The cursor is reset when all the data of STP is consumed out */
	if (!bdev->stp_dlen && bdev->stp_cursor >= 6) {
		bdev->stp_cursor = 0;
		BTMTK_ERR("reset cursor = %d\n", bdev->stp_cursor);
	}

	/* Filling pad until all STP info is obtained */
	while (bdev->stp_cursor < 6 && count > 0) {
		bdev->stp_pad[bdev->stp_cursor] = *data;
		BTMTK_ERR("fill stp format (%02x, %d, %d)\n",
		   bdev->stp_pad[bdev->stp_cursor], bdev->stp_cursor, count);
		bdev->stp_cursor++;
		data++;
		count--;
	}

	/* Retrieve STP info and have a sanity check */
	if (!bdev->stp_dlen && bdev->stp_cursor >= 6) {
		shdr = (struct mtk_stp_hdr *)&bdev->stp_pad[2];
		bdev->stp_dlen = be16_to_cpu(shdr->dlen) & 0x0fff;
		BTMTK_ERR("stp format (%02x, %02x)",
			   shdr->prefix, bdev->stp_dlen);

		/* Resync STP when unexpected data is being read */
		if (shdr->prefix != 0x80 || bdev->stp_dlen > 2048) {
			BTMTK_ERR("stp format unexpect (%02x, %02x)",
				   shdr->prefix, bdev->stp_dlen);
			BTMTK_ERR("reset cursor = %d\n", bdev->stp_cursor);
			bdev->stp_cursor = 2;
			bdev->stp_dlen = 0;
		}
	}

	/* Directly quit when there's no data found for H4 can process */
	if (count <= 0)
		return NULL;

	/* Tranlate to how much the size of data H4 can handle so far */
	*sz_h4 = min_t(int, count, bdev->stp_dlen);

	/* Update the remaining size of STP packet */
	bdev->stp_dlen -= *sz_h4;

	/* Data points to STP payload which can be handled by H4 */
	return data;
}
#endif

#if (USE_DEVICE_NODE == 1)
void btmtk_rx_flush(void)
{
	rx_queue_flush();
}

uint8_t btmtk_rx_data_valid(void)
{
	return !is_rx_queue_empty();
}

int32_t btmtk_send_data(struct hci_dev *hdev, uint8_t *buf, uint32_t count)
{
	struct sk_buff *skb = NULL;
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);

	skb = alloc_skb(count + BT_SKB_RESERVE, GFP_KERNEL);
	if (skb == NULL) {
		BTMTK_ERR("%s allocate skb failed!!", __func__);
		return -1;
	}

	/* Reserv for core and drivers use */
	skb_reserve(skb , BT_SKB_RESERVE);
	bt_cb(skb)->pkt_type = buf[0];
	memcpy(skb->data, buf, count);
	skb->len = count;

	skb_queue_tail(&bdev->tx_queue, skb);
	wake_up_interruptible(&bdev->tx_waitq);

	return count;
}

void btmtk_register_rx_event_cb(struct btmtk_dev *bdev, BT_RX_EVENT_CB cb)
{
	bdev->rx_event_cb = cb;
	btmtk_rx_flush();
}

int32_t btmtk_receive_data(struct hci_dev *hdev, uint8_t *buf, uint32_t count)
{
	uint32_t read_bytes;

	rx_dequeue(buf, count, &read_bytes);
	/* TODO: disable quick PS mode by traffic density */
	return read_bytes;
}
#endif

int btmtk_recv(struct hci_dev *hdev, const u8 *data, size_t count)
{
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);
	int err;
#if ENABLESTP
	const unsigned char *p_left = data, *p_h4 = NULL;
	int sz_left = count, sz_h4 = 0, adv = 0;

	while (sz_left > 0) {
		/*  The serial data received from MT7622 BT controller is
		 *  at all time padded around with the STP header and tailer.
		 *
		 *  A full STP packet is looking like
		 *   -----------------------------------
		 *  | STP header  |  H:4   | STP tailer |
		 *   -----------------------------------
		 *  but it doesn't guarantee to contain a full H:4 packet which
		 *  means that it's possible for multiple STP packets forms a
		 *  full H:4 packet that means extra STP header + length doesn't
		 *  indicate a full H:4 frame, things can fragment. Whose length
		 *  recorded in STP header just shows up the most length the
		 *  H:4 engine can handle currently.
		 */
		p_h4 = mtk_stp_split(bdev, p_left, sz_left, &sz_h4);
		if (!p_h4)
			break;

		adv = p_h4 - p_left;
		sz_left -= adv;
		p_left += adv;
		bdev->rx_skb = h4_recv_buf(bdev->hdev, bdev->rx_skb, p_h4,
					   sz_h4, mtk_recv_pkts,
					   ARRAY_SIZE(mtk_recv_pkts));

		if (IS_ERR(bdev->rx_skb)) {
			err = PTR_ERR(bdev->rx_skb);
			BTMTK_ERR("Frame reassembly failed (%d)", err);
			bdev->rx_skb = NULL;
			return err;
		}

		sz_left -= sz_h4;
		p_left += sz_h4;
	}
#else
	bdev->rx_skb = h4_recv_buf(hdev, bdev->rx_skb, data,
				   count, mtk_recv_pkts,
				   ARRAY_SIZE(mtk_recv_pkts));

	if (IS_ERR(bdev->rx_skb)) {
		err = PTR_ERR(bdev->rx_skb);
		BTMTK_ERR("Frame reassembly failed (%d)", err);
		bdev->rx_skb = NULL;
		return err;
	}
#endif

	return 0;
}
int btmtk_dispatch_acl(struct hci_dev *hdev, struct sk_buff *skb)
{
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);

	if (skb->data[0]== 0x6f && skb->data[1]== 0xfc && skb->len > 12) {
		/* coredump data done
		 * For Example : TotalTimeForDump=0xxxxxxx, (xx secs)
		 */
		if (skb->data[4]== 0x54 && skb->data[5] == 0x6F &&
			skb->data[6]== 0x74 && skb->data[7] == 0x61 &&
			skb->data[8]== 0x6C && skb->data[9] == 0x54 &&
			skb->data[10]== 0x69 && skb->data[11] == 0x6D &&
			skb->data[12]== 0x65) {
			/* coredump end, do reset */
			BTMTK_INFO("%s coredump done", __func__);
			msleep(3000);
			bdev->subsys_reset= 1;
		}
		return 1;
	} else if (skb->data[0]== 0xff && skb->data[1] == 0x05) {
		BTMTK_DBG("%s correct picus log by ACL", __func__);
	}
	return 0;
}

int btmtk_dispatch_event(struct hci_dev *hdev, struct sk_buff *skb)
{

	/* For Picus */
	if (skb->data[0]== 0xff && skb->data[2] == 0x50) {
		BTMTK_DBG("%s correct picus log format by EVT", __func__);
	}
	return btmtk_cif_dispatch_event(hdev, skb);
}

int btmtk_recv_acl(struct hci_dev *hdev, struct sk_buff *skb)
{
	int err = 0, skip_pkt = 0;

	skip_pkt = btmtk_dispatch_acl(hdev, skb);
	if(skip_pkt == 0)
#if (USE_DEVICE_NODE == 0)
		err = hci_recv_frame(hdev, skb);
#else
		err = rx_skb_enqueue(skb);
#endif

	return err;
}
#if (DRIVER_CMD_CHECK == 1)

void btmtk_check_event(struct sk_buff *skb)
{
	u8 event_code = skb->data[0];
	u16 cmd_opcode;

	if(event_code == HCI_COMMAND_COMPLETE_EVT) {
		cmd_opcode = (skb->data[3] << 8) | skb->data[4];

		if(cmd_list_check(cmd_opcode) == FALSE) {
			BTMTK_ERR("%s No match command %4X", __func__,cmd_opcode);
		} else {
			cmd_list_remove(cmd_opcode);
			update_command_response_workqueue();
		}
	} else if(event_code == HCI_COMMAND_STATUS_EVT) {
		cmd_opcode = (skb->data[4] << 8) | skb->data[5];

		if(cmd_list_check(cmd_opcode) == FALSE) {
			BTMTK_ERR("%s No match command %4X", __func__,cmd_opcode);
		} else {
			cmd_list_remove(cmd_opcode);
			update_command_response_workqueue();
		}
	}
}
#endif

int btmtk_recv_event(struct hci_dev *hdev, struct sk_buff *skb)
{
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);
	int err = 0;

#if (DRIVER_CMD_CHECK == 1)
	if (bdev->cmd_timeout_check == TRUE) {
		btmtk_check_event(skb);
	}
#endif

#if 0
	/* Fix up the vendor event id with 0xff for vendor specific instead
	 * of 0xe4 so that event send via monitoring socket can be parsed
	 * properly.
	 */
	if (hdr->evt == 0xe4) {
		BTMTK_DBG("%s: hdr->evt is %02x", __func__, hdr->evt);
		hdr->evt = HCI_EV_VENDOR;
	}
#endif

	/* When someone waits for the WMT event, the skb is being cloned
	 * and being processed the events from there then.
	 */
	if (test_bit(BTMTKUART_TX_WAIT_VND_EVT, &bdev->tx_state)) {
		/* check is corresponding wmt event */
		if (btmtk_dispatch_event(hdev, skb) != WMT_EVT_SKIP) {
			bdev->evt_skb = skb_copy(skb, GFP_KERNEL);
			if (!bdev->evt_skb) {
				err = -ENOMEM;
				BTMTK_ERR("%s: WMT event copy to evt_skb failed, err = %d", __func__, err);
				goto out;
			}

			if (test_and_clear_bit(BTMTKUART_TX_WAIT_VND_EVT, &bdev->tx_state)) {
				BTMTK_DBG("%s: clear bit BTMTKUART_TX_WAIT_VND_EVT", __func__);
				wake_up(&bdev->p_wait_event_q);
				BTMTK_DBG("%s: wake_up p_wait_event_q", __func__);
			} else {
				BTMTK_ERR("%s: test_and_clear_bit(BTMTKUART_TX_WAIT_VND_EVT) fail", __func__);
				if (bdev->evt_skb)
					kfree_skb(bdev->evt_skb);
				bdev->evt_skb = NULL;
			}
			goto out;
		} else {
			// may be normal packet, continue put skb to rx queue
			BTMTK_INFO("%s: may be normal packet!", __func__);
		}
	}

	/* User trx debug function, put rx event to callbacl */
	if (g_bt_dbg_st.trx_enable) {
		g_bt_dbg_st.trx_cb(skb->data, (int)skb->len);
	}

#if (USE_DEVICE_NODE == 0)
	err = hci_recv_frame(hdev, skb);
#else
	err = rx_skb_enqueue(skb);
#endif

	if (err < 0) {
		BTMTK_ERR("%s: hci_recv_failed, err = %d", __func__, err);
	}
	return 0;

out:
	return err;
}

int btmtk_recv_iso(struct hci_dev *hdev, struct sk_buff *skb)
{
	int err = 0;

#if (USE_DEVICE_NODE == 0)
	err = hci_recv_frame(hdev, skb);
#else
	err = rx_skb_enqueue(skb);
#endif
	return err;
}


int btmtk_main_send_cmd(struct hci_dev *hdev, const uint8_t *cmd, const int cmd_len, const int tx_state)
{
	struct sk_buff *skb = NULL;
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);
	int ret = 0;

	skb = alloc_skb(cmd_len + BT_SKB_RESERVE, GFP_ATOMIC);
	if (skb == NULL) {
		BTMTK_ERR("%s allocate skb failed!!", __func__);
		goto err_free_skb;
	}
	/* Reserv for core and drivers use */
	skb_reserve(skb , 7);
	bt_cb(skb)->pkt_type = HCI_COMMAND_PKT;
	memcpy(skb->data, cmd, cmd_len);
	skb->len = cmd_len;

#if ENABLESTP
	skb = mtk_add_stp(bdev, skb);
#endif

	BTMTK_DBG_RAW(skb->data, skb->len, "%s, len = %d, send cmd: ", __func__, skb->len);

	set_bit(tx_state, &bdev->tx_state);
#if SUPPORT_BT_THREAD
	skb_queue_tail(&bdev->tx_queue, skb);
	wake_up_interruptible(&bdev->tx_waitq);
#else
	btmtk_cif_send_cmd(hdev, skb->data, skb->len, 5, 0, 0);
#endif
	ret = wait_event_timeout(bdev->p_wait_event_q,
			bdev->evt_skb != NULL || tx_state == BTMTKUART_TX_SKIP_VENDOR_EVT, 2*HZ);
	BTMTK_INFO("%s, ret = %d", __func__, ret);

err_free_skb:
#if (SUPPORT_BT_THREAD == 0)
	if (skb)
		kfree_skb(skb);
#endif
	if (tx_state == BTMTKUART_TX_WAIT_VND_EVT) {
		if (bdev->evt_skb)
			kfree_skb(bdev->evt_skb);
		bdev->evt_skb = NULL;
	}

	return ret;
}

uint8_t *_internal_evt_result(u_int8_t wmt_evt_result)
{

	if (wmt_evt_result == WMT_EVT_SUCCESS)
		return "WMT_EVT_SUCCESS";
	else if (wmt_evt_result == WMT_EVT_FAIL)
		return "WMT_EVT_FAIL";
	else if (wmt_evt_result == WMT_EVT_INVALID)
		return "WMT_EVT_INVALID";
	else
		return "WMT_EVT_SKIP";
}

int btmtk_load_code_from_bin(u8 **image, char *bin_name,
					 struct device *dev, u32 *code_len, u8 retry)
{
	const struct firmware *fw_entry = NULL;
	int err = 0;

	do {
		err = request_firmware(&fw_entry, bin_name, dev);
		if (err == 0) {
			break;
		} else if (retry <= 0) {
			*image = NULL;
			BTMTK_ERR("%s: request_firmware %d times fail!!! err = %d", __func__, 10, err);
			return -1;
		}
		BTMTK_ERR("%s: request_firmware fail!!! err = %d, retry = %d", __func__, err, retry);
		msleep(100);
	} while (retry-- > 0);

	*image = vmalloc(ALIGN_4(fw_entry->size));
	if (*image == NULL) {
		*code_len = 0;
		BTMTK_ERR("%s: vmalloc failed!! error code = %d", __func__, err);
		return -1;
	}

	memcpy(*image, fw_entry->data, fw_entry->size);
	*code_len = fw_entry->size;

	release_firmware(fw_entry);
	return 0;
}

int btmtk_calibration_flow(struct hci_dev *hdev)
{
	int ret = 0;
	ret = btmtk_send_calibration_cmd(hdev);
	BTMTK_INFO("%s done", __func__);
	return ret;
}

#if ENABLESTP
static int btmtk_send_set_stp_cmd(struct hci_dev *hdev)
{
	u8 cmd[] = { 0x01, 0x6F, 0xFC, 0x09, 0x01, 0x04, 0x05, 0x00, 0x03, 0x11, 0x0E, 0x00, 0x00};
	/* To-Do, for event check */
	/* u8 event[] = { 0x04, 0xE4, 0x06, 0x02, 0x04, 0x02, 0x00, 0x00, 0x03}; */
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);

	btmtk_main_send_cmd(hdev, cmd, sizeof(cmd), BTMTKUART_TX_WAIT_VND_EVT);

	BTMTK_INFO("%s done", __func__);
	return 0;
}

static int btmtk_send_set_stp1_cmd(struct hci_dev *hdev)
{
	u8 cmd[] = {0x01, 0x6F, 0xFC, 0x0C, 0x01, 0x08, 0x08, 0x00, 0x02, 0x01, 0x00, 0x01, 0x08, 0x00, 0x00, 0x80};
	/* To-Do, for event check */
	/* u8 event[] = {0x04, 0xE4, 0x10, 0x02, 0x08,
			0x0C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x08, 0x00, 0x00, 0x80, 0x63, 0x76, 0x00, 0x00}; */
	//struct btmtk_dev *bdev = hci_get_drvdata(hdev);

	btmtk_main_send_cmd(hdev, cmd, sizeof(cmd), BTMTKUART_TX_WAIT_VND_EVT);

	BTMTK_INFO("%s done", __func__);
	return 0;
}
#endif

/**
 * Kernel HCI Interface Registeration
 */
static int bt_flush(struct hci_dev *hdev)
{
#if SUPPORT_BT_THREAD
	struct btmtk_dev *bdev =  hci_get_drvdata(hdev);

	skb_queue_purge(&bdev->tx_queue);
#endif
	return 0;
}

int bt_close(struct hci_dev *hdev)
{
	BTMTK_INFO("%s", __func__);

	/* Power off */
	btmtk_set_power_off(hdev, FALSE);
	clear_bit(HCI_RUNNING, &hdev->flags);

	return 0;
}

static int bt_setup(struct hci_dev *hdev)
{
	int ret = 0;
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);

	BTMTK_INFO("%s", __func__);

	/* 1. Power on */
	ret = btmtk_set_power_on(hdev, FALSE);
	if (ret) {
		BTMTK_ERR("btmtk_set_power_on fail");
		return ret;
	}
#if ENABLESTP
	btmtk_send_set_stp_cmd(hdev);
	btmtk_send_set_stp1_cmd(hdev);
#endif

	/* 3. Do calibration */
#if 0   /* skip temporary for bring up state */
	ret = btmtk_calibration_flow(hdev);
	if (ret) {
		BTMTK_ERR("btmtk_calibration_flow fail");
		goto func_on_fail;
	}
#endif

	/* Set bt to sleep mode */
	btmtk_set_sleep(hdev, TRUE);

	return 0;
}

static int bt_open(struct hci_dev *hdev)
{
	int ret = 0;
	BTMTK_INFO("%s", __func__);

	if (test_bit(HCI_RUNNING, &hdev->flags)) {
		BTMTK_WARN("BT already on!\n");
		return -EIO;
	}

#if BLUEDROID
	ret = bt_setup(hdev);
#endif

	if (!ret)
		set_bit(HCI_RUNNING, &hdev->flags);

	BTMTK_INFO("HCI running bit = %d", test_bit(HCI_RUNNING, &hdev->flags));
	return ret;
}

static int bt_send_frame(struct hci_dev *hdev, struct sk_buff *skb)
{
#if SUPPORT_BT_THREAD
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);
#endif

	BTMTK_INFO("%s\n", __func__);
	memcpy(skb_push(skb, 1), &bt_cb(skb)->pkt_type, 1);
#if ENABLESTP
	skb = mtk_add_stp(bdev, skb);
#endif

#if SUPPORT_BT_THREAD
	skb_queue_tail(&bdev->tx_queue, skb);
	wake_up_interruptible(&bdev->tx_waitq);
#else
	btmtk_cif_send_cmd(hdev, skb->data,	skb->len, 5, 0, 0);
#endif

	return 0;
}

int btmtk_allocate_hci_device(struct btmtk_dev *bdev, int hci_bus_type)
{
	struct hci_dev *hdev;
#if (USE_DEVICE_NODE == 0)
	int err = 0;
#endif

	/* Add hci device */
	hdev = hci_alloc_dev();
	if (!hdev)
		return -ENOMEM;
	hdev->bus = hci_bus_type;

	bdev->hdev = hdev;
	hci_set_drvdata(hdev, bdev);

	/* register hci callback */
	hdev->open	= bt_open;
	hdev->close	= bt_close;
	hdev->flush	= bt_flush;
	hdev->send	= bt_send_frame;
	hdev->setup	= bt_setup;

	init_waitqueue_head(&bdev->p_wait_event_q);
#if SUPPORT_BT_THREAD
	skb_queue_head_init(&bdev->tx_queue);
#endif
#if (USE_DEVICE_NODE == 0)
	SET_HCIDEV_DEV(hdev, BTMTK_GET_DEV(bdev));

	err = hci_register_dev(hdev);
	/* After hci_register_dev completed
	 * It will set dev_flags to HCI_SETUP
	 * That cause vendor_lib create socket failed
	 */
	if (err < 0) {
		BTMTK_INFO("%s can't register", __func__);
		hci_free_dev(hdev);
		return err;
	}

	/*set_bit(HCI_RUNNING, &hdev->flags);
	set_bit(HCI_QUIRK_RAW_DEVICE, &hdev->quirks);*/
#if BLUEDROID
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0))
	clear_bit(HCI_SETUP, &hdev->dev_flags);
#else
	hci_dev_clear_flag(hdev, HCI_SETUP);
#endif
#endif	// BLUEDROID
#endif	// USE_DEVICE_NODE == 0
	set_bit(BTMTKUART_REQUIRED_DOWNLOAD, &bdev->tx_state);

	BTMTK_INFO("%s done", __func__);
	return 0;
}

int32_t btmtk_free_hci_device(struct btmtk_dev *bdev, int hci_bus_type)
{
	hci_unregister_dev(bdev->hdev);
	hci_free_dev(bdev->hdev);

	bdev->hdev = NULL;
	return 0;
}

#if (USE_DEVICE_NODE == 1)
#undef __init
#undef __exit
#define __init
#define __exit
#endif

/**
 * Kernel Module init/exit Functions
 */
int __init main_driver_init(void)
{
	int ret = -1;

	BTMTK_INFO("%s", __func__);
	ret = main_init();
	if (ret < 0)
		return ret;

	ret = btmtk_cif_register();
	if (ret < 0) {
		BTMTK_ERR("*** USB registration failed(%d)! ***", ret);
		return ret;
	}
	BTMTK_INFO("%s: Done", __func__);
	return 0;
}

void __exit main_driver_exit(void)
{
	BTMTK_INFO("%s", __func__);
	btmtk_cif_deregister();
	main_exit();
}

#if (USE_DEVICE_NODE == 0)
module_init(main_driver_init);
module_exit(main_driver_exit);
#endif

/**
 * Module Common Information
 */
MODULE_DESCRIPTION("Mediatek Bluetooth Driver");
MODULE_VERSION(VERSION SUBVER);
MODULE_LICENSE("GPL");
