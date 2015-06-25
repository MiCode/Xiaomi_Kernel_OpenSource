/*
 * STMicroelectronics st_sensor_hub ymodem protocol
 *
 * Copyright 2014 STMicroelectronics Inc.
 * Denis Ciocca <denis.ciocca@st.com>
 * Copyright (C) 2015 XiaoMi, Inc.
 *
 * Licensed under the GPL-2.
 */

#ifndef ST_SENSOR_HUB_YMODEM_H
#define ST_SENSOR_HUB_YMODEM_H

#include "st_sensor_hub.h"

#define ST_HUB_YMODEM_START_COMMAND		'X'
#define ST_HUB_YMODEM_RUN_COMMAND		'N'
#define ST_HUB_YMODEM_FW_VERSION_COMMAND	'V'
#define ST_HUB_YMODEM_SET_RAM_ADDR		'M'

#define ST_HUB_YMODEM_RAM2_ADDR			(0x200000c0)

#define ST_HUB_YMODEM_SLEEP_MS			(10)
#define ST_HUB_YMODEM_NUM_RETRY			(10)

#define ST_HUB_RAM1_SIZE			(65536)
#define ST_HUB_RAM2_SIZE			(65334)

#define YMODEM_PACKET_HEADER_SIZE		(3)
#define YMODEM_PACKET_128_SIZE			(128)
#define YMODEM_PACKET_1K_SIZE			(1024)
#define YMODEM_PACKET_SPLIT_SIZE		(12)

#define YMODEM_SOH_CODE				(0x01)
#define YMODEM_STX_CODE				(0x02)
#define YMODEM_EOT_CODE				(0x04)
#define YMODEM_ACK_CODE				(0x06)

int st_sensor_hub_send_firmware_ymodem(struct st_hub_data *hdata,
						const struct firmware *fw);

int st_sensor_hub_read_ramloader_version(struct st_hub_data *hdata);

#endif /* ST_SENSOR_HUB_YMODEM_H */
