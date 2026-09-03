/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _WCN_CA_TRUSTY_H
#define _WCN_CA_TRUSTY_H

#define IV_BYTE_LEN 16

struct sys_img_header {
	u32  magic_num;            // "BTHD"="0x42544844"="boothead"
	u32  version;              // 1
	u8   payload_hash[32];     // sha256 hash value
	u64  img_addr;             // image loaded address
	u32  img_size;             // image size
	u8   iv_data[IV_BYTE_LEN]; // the parameter of AES crypto
	u32  is_packed;            // packed image, 0:false 1:true
	u32  firmware_size;        // runtime firmware size
	u32  img_real_size;        //image real size
	u32  img_signed_size;      //image full size after signed
	u8   reserved[424];        // 424 + 17*4 + 16 + 4 = 512
	u32  slot;                 // spl double slot: slota or slotb
};

#define SEC_IMAGE_MAGIC 0x42544844
#define SEC_IMAGE_HDR_SIZE sizeof(struct sys_img_header)

enum secureboot_command {
	KERNELBOOTCP_BIT                = 1,
	KERNELBOOTCP_REQ_SHIFT          = 1,

	KERNEL_BOOTCP_VERIFY_ALL        = (0 << KERNELBOOTCP_REQ_SHIFT),
	KERNEL_BOOTCP_UNLOCK_DDR        = (1 << KERNELBOOTCP_REQ_SHIFT),
	KERNEL_BOOTCP_VERIFY_VDSP       = (2 << KERNELBOOTCP_REQ_SHIFT),
	KERNEL_BOOTCP_VERIFY_WCN       = (3 << KERNELBOOTCP_REQ_SHIFT),
	KERNEL_BOOTCP_VERIFY_GPS       = (4 << KERNELBOOTCP_REQ_SHIFT),
};

/* Size of the footer.                 */
/* original definition in avb_footer.h */
#define AVB_FOOTER_SIZE    64

/* Size of  partition name .          */
#define PART_NAME_SIZE      32

struct KBC_IMAGE_S {
	u64 img_addr;  // the base address of image to verify
	u32 img_len;   // length of image
	u32 map_len;   // mapping length
	/* Infos from QieGang: CONFIG_VBOOT_V2 is enabled */
	u8  footer[AVB_FOOTER_SIZE];
	u8  partition[PART_NAME_SIZE];
};

struct KBC_LOAD_TABLE_W {
	struct KBC_IMAGE_S wcn_fw;
	u16    flag;      // not use
	u16    is_packed; // is packed image
};

struct KBC_LOAD_TABLE_G {
	struct KBC_IMAGE_S gps_fw;
	u16    flag;      // not use
	u16    is_packed; // is packed image
};

/**
 * kernelbootcp_message - Serial header for communicating with KBC server
 * @cmd: the command, one of kernelbootcp_command.
 * @payload: start of the serialized command specific payload
 */
struct kernelbootcp_message {
	u32 cmd;
	u8  payload[0];
};

struct wcn_ca_tipc_ctx {
	int state;
	struct mutex lock;
	struct tipc_chan *chan;
	wait_queue_head_t readq;
	struct list_head rx_msg_queue;
};

struct firmware_verify_ctrl {
	u32 wcn_or_gnss_bin; // 1 for wcn, 2 for gnss.
	const char *tipc_chan_name;
	phys_addr_t bin_base_addr;
	u32 bin_length;
	struct wcn_ca_tipc_ctx *ca_tipc_ctx;
};

int wcn_firmware_sec_verify(u32 wcn_or_gnss_bin,
		phys_addr_t bin_base_addr, u32 bin_length);

#endif
