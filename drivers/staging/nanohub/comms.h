/*
 * Copyright (C) 2016 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _NANOHUB_COMMS_H
#define _NANOHUB_COMMS_H

struct __attribute__ ((__packed__)) nanohub_packet {
	u8 sync;
	u32 seq;
	u32 reason;
	u8 len;
	u8 data[];
};

struct __attribute__ ((__packed__)) nanohub_packet_pad {
	u8 pad[3];
	struct nanohub_packet
	 packet;
};

struct __attribute__ ((__packed__)) nanohub_packet_crc {
	u32 crc;
};

struct nanohub_data;

struct nanohub_comms {
	struct semaphore sem;
	u32 seq;
	int timeout_write;
	int timeout_ack;
	int timeout_reply;
	int (*open)(void *);
	void (*close)(void *);
	int (*write)(void *, u8 *, int, int);
	int (*read)(void *, u8 *, int, int);

	union {
		struct i2c_client *i2c_client;
		struct spi_device *spi_device;
	};

	u8 *tx_buffer;
	u8 *rx_buffer;
};

int nanohub_comms_kernel_download(struct nanohub_data *data,
				const u8 *image, size_t length);
int nanohub_comms_app_download(struct nanohub_data *data,
				const u8 *image, size_t length);
int nanohub_comms_rx_retrans_boottime(struct nanohub_data *data,
						u32 cmd, u8 *rx, size_t rx_len,
						int retrans_cnt,
						int retrans_delay);
int nanohub_comms_tx_rx_retrans(struct nanohub_data *data, u32 cmd,
				const u8 *tx, u8 tx_len,
				u8 *rx, size_t rx_len, bool user,
				int retrans_cnt, int retrans_delay);

#define ERROR_NACK			-1
#define ERROR_BUSY			-2

#define MAX_UINT8			((1 << (8 * sizeof(u8))) - 1)

#define COMMS_SYNC			0x31
#define COMMS_FLASH_KERNEL_ID		0x1
#define COMMS_FLASH_EEDATA_ID		0x2
#define COMMS_FLASH_APP_ID		0x4

#define CMD_COMMS_ACK			0x00000000
#define CMD_COMMS_NACK			0x00000001
#define CMD_COMMS_BUSY			0x00000002

#define CMD_COMMS_GET_OS_HW_VERSIONS	0x00001000
#define CMD_COMMS_GET_APP_VERSIONS	0x00001001
#define CMD_COMMS_QUERY_APP_INFO	0x00001002

#define CMD_COMMS_START_KERNEL_UPLOAD	0x00001040
#define CMD_COMMS_KERNEL_CHUNK		0x00001041
#define CMD_COMMS_FINISH_KERNEL_UPLOAD	0x00001042

#define CMD_COMMS_START_APP_UPLOAD	0x00001050
#define CMD_COMMS_APP_CHUNK		0x00001051

#define CMD_COMMS_CLR_GET_INTR		0x00001080
#define CMD_COMMS_MASK_INTR		0x00001081
#define CMD_COMMS_UNMASK_INTR		0x00001082
#define CMD_COMMS_READ			0x00001090
#define CMD_COMMS_WRITE			0x00001091

#define CHUNK_REPLY_ACCEPTED		0
#define CHUNK_REPLY_WAIT                1
#define CHUNK_REPLY_RESEND              2
#define CHUNK_REPLY_RESTART             3
#define CHUNK_REPLY_CANCEL              4
#define CHUNK_REPLY_CANCEL_NO_RETRY     5

#define UPLOAD_REPLY_SUCCESS			0
#define UPLOAD_REPLY_PROCESSING			1
#define UPLOAD_REPLY_WAITING_FOR_DATA		2
#define UPLOAD_REPLY_APP_SEC_KEY_NOT_FOUND	3
#define UPLOAD_REPLY_APP_SEC_HEADER_ERROR	4
#define UPLOAD_REPLY_APP_SEC_TOO_MUCH_DATA	5
#define UPLOAD_REPLY_APP_SEC_TOO_LITTLE_DATA	6
#define UPLOAD_REPLY_APP_SEC_SIG_VERIFY_FAIL	7
#define UPLOAD_REPLY_APP_SEC_SIG_DECODE_FAIL	8
#define UPLOAD_REPLY_APP_SEC_SIG_ROOT_UNKNOWN	9
#define UPLOAD_REPLY_APP_SEC_MEMORY_ERROR	10
#define UPLOAD_REPLY_APP_SEC_INVALID_DATA	11
#define UPLOAD_REPLY_APP_SEC_BAD		12

static inline int nanohub_comms_write(struct nanohub_data *data,
				      const u8 *buffer, size_t buffer_len)
{
	u8 ret;

	if (nanohub_comms_tx_rx_retrans
	    (data, CMD_COMMS_WRITE, buffer, buffer_len, &ret, sizeof(ret), true,
	     10, 10) == sizeof(ret)) {
		if (ret)
			return buffer_len;
		else
			return 0;
	} else {
		return ERROR_NACK;
	}
}

ssize_t nanohub_external_write(const char *buffer, size_t length);
#endif
