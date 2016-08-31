/*
 * STMicroelectronics st_sensor_hub ymodem protocol
 *
 * Copyright 2014 STMicroelectronics Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Author: MCD Application Team,
 * Denis Ciocca <denis.ciocca@st.com>
 *
 * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License").
 * http://www.st.com/software_license_agreement_liberty_v2
 */

#ifndef __YMODEM_H_
#define __YMODEM_H_

#define ST_HUB_YMODEM_SLEEP_MS		(5)

#define PACKET_HEADER			(3)
#define PACKET_TRAILER			(2)
#define PACKET_OVERHEAD			(PACKET_HEADER + PACKET_TRAILER)
#define PACKET_SIZE			(128)
#define PACKET_1K_SIZE			(1024)

#define FILE_NAME_LENGTH		(256)
#define FILE_SIZE_LENGTH		(16)

#define SOH				(0x01)
#define STX				(0x02)
#define EOT				(0x04)
#define ACK				(0x06)
#define NAK				(0x15)
#define CA				(0x18)
#define CRC16				(0x43)

#define ST_HUB_YMODEM_FW_VERSION_COMMAND	'V'

#define ABORT1				(0x41)
#define ABORT2				(0x61)

#define NAK_TIMEOUT			(0x100000)
#define MAX_ERRORS			(5)

int st_sensor_hub_send_firmware_ymodem(struct i2c_client *client,
							u8 *data, size_t size);

int st_sensor_hub_read_ramloader_version(struct i2c_client *client,
								u8 *fw_version);

#endif /* __YMODEM_H_ */
