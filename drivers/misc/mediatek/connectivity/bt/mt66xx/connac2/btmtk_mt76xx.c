/*
 *  Copyright (c) 2016,2017 MediaTek Inc.
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
#include "btmtk_chip_if.h"


int32_t btmtk_load_rom_patch_766x(struct hci_dev *hdev)
{
	char *tmp_str;
	s32 sent_len;
	u32 patch_len = 0;
	u32 cur_len = 0;
	int first_block = 1;
	u8 phase;
	u32 written = 0;
	int ret = 0;

	u8 *pos;
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);
	/* To-Do, for event check */
	/* u8 event[] = {0x04, 0xE4, 0x05, 0x02, 0x01, 0x01, 0x00, 0x00}; */

	unsigned char		*rom_patch;
	unsigned char		*rom_patch_bin_file_name;
	unsigned int		rom_patch_len = 0;

	struct sk_buff *skb = NULL;

	rom_patch_bin_file_name = kzalloc(MAX_BIN_FILE_NAME_LEN, GFP_KERNEL);

	/* To-Do
	 * Read CHIP ID for bin_file_name
	 */
	memcpy(rom_patch_bin_file_name, "mt7915_patch_e1_hdr.bin", 23);
	//memcpy(rom_patch_bin_file_name, "mt7663_patch_e2_hdr.bin", 23);

	btmtk_load_code_from_bin(&rom_patch,
			rom_patch_bin_file_name, NULL,
			&rom_patch_len, 10);

	tmp_str = rom_patch;

	tmp_str = rom_patch + 16;
	BTMTK_INFO("%s: platform = %c%c%c%c\n", __func__, tmp_str[0], tmp_str[1], tmp_str[2], tmp_str[3]);

	tmp_str = rom_patch + 20;
	BTMTK_INFO("%s: HW/SW version = %c%c%c%c\n", __func__, tmp_str[0], tmp_str[1], tmp_str[2], tmp_str[3]);

	tmp_str = rom_patch + 24;
	BTMTK_INFO("loading rom patch...\n");

	cur_len = 0x00;
	patch_len = rom_patch_len - PATCH_INFO_SIZE;

	BTMTK_INFO("%s: patch_len = %d\n", __func__, patch_len);

	BTMTK_INFO("%s: loading rom patch...\n", __func__);


	pos = kmalloc(UPLOAD_PATCH_UNIT, GFP_ATOMIC);

	/* loading rom patch */
	while (1) {
		s32 sent_len_max = UPLOAD_PATCH_UNIT - PATCH_HEADER_SIZE;

		sent_len = (patch_len - cur_len) >= sent_len_max ? sent_len_max : (patch_len - cur_len);

		if (sent_len > 0) {
			if (first_block == 1) {
				if (sent_len < sent_len_max)
					phase = PATCH_PHASE3;
				else
					phase = PATCH_PHASE1;
				first_block = 0;
			} else if (sent_len == sent_len_max) {
				if (patch_len - cur_len == sent_len_max)
					phase = PATCH_PHASE3;
				else
					phase = PATCH_PHASE2;
			} else {
				phase = PATCH_PHASE3;
			}


			/* prepare HCI header */
			pos[0] = 0x02;
			pos[1] = 0x6F;
			pos[2] = 0xFC;
			pos[3] = (sent_len + 5) & 0xFF;
			pos[4] = ((sent_len + 5) >> 8) & 0xFF;

			/* prepare WMT header */
			pos[5] = 0x01;
			pos[6] = 0x01;
			pos[7] = (sent_len + 1) & 0xFF;
			pos[8] = ((sent_len + 1) >> 8) & 0xFF;

			pos[9] = phase;
			memcpy(&pos[10], rom_patch + PATCH_INFO_SIZE + cur_len,
				sent_len);
			ret = btmtk_main_send_cmd(hdev, pos, sent_len + PATCH_HEADER_SIZE, BTMTKUART_TX_WAIT_VND_EVT);
			if (ret == 0) {
				BTMTK_INFO("%s: send patch failed, terminate", __func__);
				goto err;
			}
			cur_len = cur_len + sent_len;
			BTMTK_INFO("%s: sent_len = %d, cur_len = %d, phase = %d, written = %d", __func__, sent_len, cur_len, phase, written);

			/*if (btmtk_usb_get_rom_patch_result() < 0)
				goto error2;*/

		} else {
			btmtk_send_wmt_reset(hdev);
			BTMTK_INFO("%s: loading rom patch... Done", __func__);
			if (bdev->subsys_reset == 1) {
				skb = alloc_skb(BT_SKB_RESERVE, GFP_ATOMIC);
				if (skb == NULL) {
					BTMTK_ERR("%s allocate skb failed!!", __func__);
				} else {
					BTMTK_INFO("%s: send hw_err!!", __func__);
					hci_skb_pkt_type(skb) = HCI_EVENT_PKT;
					skb->data[0] = 0x10;
					skb->data[1] = 0x01;
					skb->data[2] = 0x01;
					hci_recv_frame(hdev, skb);
				}
				bdev->subsys_reset = 0;
			}
			clear_bit(BTMTKUART_REQUIRED_DOWNLOAD, &bdev->tx_state);
			break;
		}
	}
err:
	return ret;
}

static inline int btmtk_send_wmt_power_on_cmd_766x(struct hci_dev *hdev)
{
	/* Support 7668 and 7663 */
	u8 cmd[] = { 0x01, 0x6F, 0xFC, 0x06, 0x01, 0x06, 0x02, 0x00, 0x00, 0x01 };
	/* To-Do, for event check */
	/* u8 event[] = { 0x04, 0xE4, 0x05, 0x02, 0x06, 0x01, 0x00 }; */

	btmtk_main_send_cmd(hdev, cmd, sizeof(cmd), BTMTKUART_TX_WAIT_VND_EVT);

	BTMTK_INFO("%s done", __func__);
	return 0;
}

static inline int btmtk_76xx_send_wmt_reset(struct hci_dev *hdev)
{
	/* Support 7668 and 7663 */
	u8 cmd[] = { 0x01, 0x6F, 0xFC, 0x05, 0x01, 0x07, 0x01, 0x00, 0x04 };
	/* To-Do, for event check */
	/* u8 event[] = { 0x04, 0xE4, 0x05, 0x02, 0x07, 0x01, 0x00, 0x00 }; */

	btmtk_main_send_cmd(hdev, cmd, sizeof(cmd), BTMTKUART_TX_WAIT_VND_EVT);

	BTMTK_INFO("%s done", __func__);
	return 0;
}

static int btmtk_send_hci_tci_set_sleep_cmd_766x(struct hci_dev *hdev)
{
	u8 cmd[] = { 0x01, 0x7A, 0xFC, 0x07, 0x05, 0x40, 0x06, 0x40, 0x06, 0x00, 0x00 };
	/* To-Do, for event check */
	/* u8 event[] = { 0x04, 0x0E, 0x04, 0x01, 0x7A, 0xFC, 0x00 }; */

	//struct btmtk_dev *bdev = hci_get_drvdata(hdev);

	btmtk_main_send_cmd(hdev, cmd, sizeof(cmd), BTMTKUART_TX_WAIT_VND_EVT);

	BTMTK_INFO("%s done", __func__);

	return 0;
}

int32_t btmtk_send_wmt_power_on_cmd(struct hci_dev *hdev)
{
	return btmtk_send_wmt_power_on_cmd_766x(hdev);
}


int32_t btmtk_set_power_on(struct hci_dev *hdev)
{
	return 0;
}

int32_t btmtk_set_sleep(struct hci_dev *hdev, u_int8_t need_wait)
{
	return btmtk_send_hci_tci_set_sleep_cmd_766x(hdev);
}
