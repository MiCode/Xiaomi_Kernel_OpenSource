/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _CONN_DEDICATED_LOG_EMI_H_
#define _CONN_DEDICATED_LOG_EMI_H_

#define CONNLOG_EMI_32_BYTE_ALIGNED 32 /* connsys EMI cache is 32-byte aligned */
#define CONNLOG_CONTROL_RING_BUFFER_BASE_SIZE 64 /* Reserve for setup ring buffer base address  */
#define CONNLOG_CONTROL_RING_BUFFER_RESERVE_SIZE 32
#define CONNLOG_IRQ_COUNTER_BASE 48
#define CONNLOG_READY_PATTERN_BASE 56
#define CONNLOG_READY_PATTERN_BASE_SIZE 8

/* Init emi_offset_table */
/* EMI structure
 * +-+-+ +----------------------+ Offset
 *   |   |       Header         |
 *   |   |      size: 0x40      |
 *   |   +----------------------+ +0x40 //CONNLOG_CONTROL_RING_BUFFER_BASE_SIZE
 *   |   |                      |   +0x00: read
 *   |   |        MCU           |   +0x04: write
 *   |   |                      |   +0x20: buf start //CONNLOG_CONTROL_RING_BUFFER_RESERVE_SIZE
 *   |   +----------------------+
 *   |   |                      | //reserved, CONNLOG_EMI_32_BYTE_ALIGNED
 *   |   +----------------------+ +(0x40 + 0x20 + MCU_SIZE + 0x20)
 *   |   |                      |   +0x00: read
 *   |   |       WIFI           |   +0x04: write
 *   v   |                      |   +0x20: buf
 *       |                      |
 *       +----------------------+
 * Size  |                      | //reserved, CONNLOG_EMI_32_BYTE_ALIGNED
 *       +----------------------+ +(0x40 + 0x20 + MCU_SIZE + 0x20) +
 *       |                      |  (WIFI_SIZE + 0x20 + 0x20)
 *       |        BT            |   +0x00: read
 *   ^   |                      |   +0x04: write
 *   |   |                      |   +0x20: buf
 *   |   +----------------------+
 *   |   |                      | //reserved, CONNLOG_EMI_32_BYTE_ALIGNED
 *   |   +----------------------+ +(0x40 + 0x20 + MCU_SIZE + 0x20) +
 *   |   |                      |  (WIFI_SIZE + 0x20 + 0x20) +
 *   |   |        GPS           |  (BT_SIZE + 0x20 + 0x20)
 *   |   |                      |   +0x00: read
 *   |   +----------------------+   +0x04: write
 *   |   |       padding        |   +0x20: buf
 * +-+-+ +----------------------+
 *
 * Header detail:
 *       +---------------+--------------+---------------+--------------+ Offset
 *       |  MCU base     |  MCU size    |    WIFI base  |  WIFI size   |
 *       +---------------+--------------+---------------+--------------+ +0x10
 *       |  BT base      |   BT size    |     GPS base  |  GPS size    |
 *       +---------------+--------------+---------------+--------------+ +0x20
 *       |                       Reserved 16 byte                      |
 *       +---------------+--------------+---------------+--------------+ +0x30
 *       |  IRQ count    |  IRQ submit  |                              |
 *       | received by   |    by MCU    |          "EMIFWLOG"          |
 *       |   driver      |              |                              |
 *       +---------------+--------------+---------------+--------------+ +0x40
 *      +0              +4             +8              +C             +F
 */

#endif
