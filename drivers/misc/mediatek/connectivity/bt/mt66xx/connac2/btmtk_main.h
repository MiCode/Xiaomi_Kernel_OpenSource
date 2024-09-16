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
#ifndef __BTMTK_MAIN_H__
#define __BTMTK_MAIN_H__
#include "btmtk_define.h"
#include "btmtk_chip_if.h"


int btmtk_allocate_hci_device(struct btmtk_dev *bdev, int hci_bus_type);
int btmtk_free_hci_device(struct btmtk_dev *bdev, int hci_bus_type);
int btmtk_recv(struct hci_dev *hdev, const u8 *data, size_t count);
int btmtk_recv_event(struct hci_dev *hdev, struct sk_buff *skb);
int btmtk_recv_acl(struct hci_dev *hdev, struct sk_buff *skb);
int btmtk_recv_iso(struct hci_dev *hdev, struct sk_buff *skb);
int btmtk_dispatch_event(struct hci_dev *hdev, struct sk_buff *skb);
int btmtk_dispatch_acl(struct hci_dev *hdev, struct sk_buff *skb);
int btmtk_send_init_cmds(struct hci_dev *hdev);
int btmtk_send_deinit_cmds(struct hci_dev *hdev);
int btmtk_main_send_cmd(struct hci_dev *hdev, const uint8_t *cmd, const int cmd_len, const int tx_state);
uint8_t *_internal_evt_result(u_int8_t wmt_evt_result);
int btmtk_load_code_from_bin(u8 **image, char *bin_name,
					 struct device *dev, u32 *code_len, u8 retry);
int btmtk_calibration_flow(struct hci_dev *hdev);


/**
  * HW Common commands
  */
int32_t btmtk_set_sleep(struct hci_dev*, u_int8_t need_wait);
int32_t btmtk_set_power_on(struct hci_dev*, u_int8_t for_precal);
int32_t btmtk_set_power_off(struct hci_dev*, u_int8_t for_precal);
int32_t btmtk_send_wmt_reset(struct hci_dev *hdev);
int32_t btmtk_send_wmt_power_on_cmd(struct hci_dev *hdev);
int32_t btmtk_send_wmt_power_off_cmd(struct hci_dev *hdev);
int32_t btmtk_send_calibration_cmd(struct hci_dev *hdev);
int32_t btmtk_send_blank_status_cmd(struct hci_dev *hdev, int32_t blank);
int32_t btmtk_send_utc_sync_cmd(void);

#if (USE_DEVICE_NODE == 1)
uint8_t btmtk_rx_data_valid(void);
int32_t btmtk_send_data(struct hci_dev *hdev, uint8_t *buf, uint32_t count);
int32_t btmtk_receive_data(struct hci_dev * hdev,uint8_t * buf,uint32_t count);
void btmtk_rx_flush(void);
void btmtk_register_rx_event_cb(struct btmtk_dev *bdev, BT_RX_EVENT_CB cb);
int main_driver_init(void);
void main_driver_exit(void);
#endif

//static inline struct sk_buff *mtk_add_stp(struct btmtk_dev *bdev, struct sk_buff *skb);

#define hci_dev_clear_flag(hdev, nr)  clear_bit((nr), (hdev)->dev_flags)

/* h4_recv */
#define hci_skb_pkt_type(skb) bt_cb((skb))->pkt_type
#define hci_skb_expect(skb) bt_cb((skb))->expect
#define hci_skb_opcode(skb) bt_cb((skb))->hci.opcode

/* HCI bus types */
#define HCI_VIRTUAL	0
#define HCI_USB		1
#define HCI_PCCARD	2
#define HCI_UART	3
#define HCI_RS232	4
#define HCI_PCI		5
#define HCI_SDIO	6
#define HCI_SPI		7
#define HCI_I2C		8
#define HCI_SMD		9

struct h4_recv_pkt {
	u8  type;	/* Packet type */
	u8  hlen;	/* Header length */
	u8  loff;	/* Data length offset in header */
	u8  lsize;	/* Data length field size */
	u16 maxlen;	/* Max overall packet length */
	int (*recv)(struct hci_dev *hdev, struct sk_buff *skb);
};

#define H4_RECV_ACL \
	.type = HCI_ACLDATA_PKT, \
	.hlen = HCI_ACL_HDR_SIZE, \
	.loff = 2, \
	.lsize = 2, \
	.maxlen = HCI_MAX_FRAME_SIZE \

#define H4_RECV_SCO \
	.type = HCI_SCODATA_PKT, \
	.hlen = HCI_SCO_HDR_SIZE, \
	.loff = 2, \
	.lsize = 1, \
	.maxlen = HCI_MAX_SCO_SIZE

#define H4_RECV_EVENT \
	.type = HCI_EVENT_PKT, \
	.hlen = HCI_EVENT_HDR_SIZE, \
	.loff = 1, \
	.lsize = 1, \
	.maxlen = HCI_MAX_EVENT_SIZE

// TODO: replace by kernel constant if kernel support new spec
#define HCI_ISODATA_PKT		0x05
#define HCI_ISO_HDR_SIZE	4
#define H4_RECV_ISO \
	.type = HCI_ISODATA_PKT, \
	.hlen = HCI_ISO_HDR_SIZE, \
	.loff = 2, \
	.lsize = 2, \
	.maxlen = HCI_MAX_FRAME_SIZE

#endif /* __BTMTK_MAIN_H__ */
