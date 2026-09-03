// SPDX-License-Identifier: GPL-2.0-only
/*
 * sprd_pd.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __LINUX_USB_SPRD_PD_H
#define __LINUX_USB_SPRD_PD_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/usb/typec.h>

/* USB PD Messages */
enum sprd_pd_ctrl_msg_type {
	/* 0 Reserved */
	SPRD_PD_CTRL_GOOD_CRC = 1,
	SPRD_PD_CTRL_GOTO_MIN = 2,
	SPRD_PD_CTRL_ACCEPT = 3,
	SPRD_PD_CTRL_REJECT = 4,
	SPRD_PD_CTRL_PING = 5,
	SPRD_PD_CTRL_PS_RDY = 6,
	SPRD_PD_CTRL_GET_SOURCE_CAP = 7,
	SPRD_PD_CTRL_GET_SINK_CAP = 8,
	SPRD_PD_CTRL_DR_SWAP = 9,
	SPRD_PD_CTRL_PR_SWAP = 10,
	SPRD_PD_CTRL_VCONN_SWAP = 11,
	SPRD_PD_CTRL_WAIT = 12,
	SPRD_PD_CTRL_SOFT_RESET = 13,
	SPRD_PD_CTRL_DATA_RESET = 14,
	/* 15 Reserved */
	SPRD_PD_CTRL_NOT_SUPP = 16,
	SPRD_PD_CTRL_GET_SOURCE_CAP_EXT = 17,
	SPRD_PD_CTRL_GET_STATUS = 18,
	SPRD_PD_CTRL_FR_SWAP = 19,
	SPRD_PD_CTRL_GET_PPS_STATUS = 20,
	SPRD_PD_CTRL_GET_COUNTRY_CODES = 21,
	/* 22 Reserved */
	SPRD_PD_CTRL_GET_SINK_CAP_EXT = 22,
	SPRD_PD_CTRL_GET_SOURCE_INFO = 23,
	SPRD_PD_CTRL_GET_REVISION = 24,
	/* 25-31 Reserved */
};

enum sprd_pd_data_msg_type {
	/* 0 Reserved */
	SPRD_PD_DATA_SOURCE_CAP = 1,
	SPRD_PD_DATA_REQUEST = 2,
	SPRD_PD_DATA_BIST = 3,
	SPRD_PD_DATA_SINK_CAP = 4,
	SPRD_PD_DATA_BATT_STATUS = 5,
	SPRD_PD_DATA_ALERT = 6,
	SPRD_PD_DATA_GET_COUNTRY_INFO = 7,
	/* 8-14 Reserved */
	SPRD_PD_DATA_REVISION = 12,
	SPRD_PD_DATA_VENDOR_DEF = 15,
	/* 16-31 Reserved */
};

enum sprd_pd_ext_msg_type {
	/* 0 Reserved */
	SPRD_PD_EXT_SOURCE_CAP_EXT = 1,
	SPRD_PD_EXT_STATUS = 2,
	SPRD_PD_EXT_GET_BATT_CAP = 3,
	SPRD_PD_EXT_GET_BATT_STATUS = 4,
	SPRD_PD_EXT_BATT_CAP = 5,
	SPRD_PD_EXT_GET_MANUFACTURER_INFO = 6,
	SPRD_PD_EXT_MANUFACTURER_INFO = 7,
	SPRD_PD_EXT_SECURITY_REQUEST = 8,
	SPRD_PD_EXT_SECURITY_RESPONSE = 9,
	SPRD_PD_EXT_FW_UPDATE_REQUEST = 10,
	SPRD_PD_EXT_FW_UPDATE_RESPONSE = 11,
	SPRD_PD_EXT_PPS_STATUS = 12,
	SPRD_PD_EXT_COUNTRY_INFO = 13,
	SPRD_PD_EXT_COUNTRY_CODES = 14,
	SPRD_PD_EXT_SINK_CAPABILITIES_EXTENDED = 15,
	/* 15-31 Reserved */
};

#define SPRD_PD_REV10				0x0
#define SPRD_PD_REV20				0x1
#define SPRD_PD_REV30				0x2
#define SPRD_PD_MAX_REV				SPRD_PD_REV30

#define SPRD_PD_HEADER_EXT_HDR			BIT(15)
#define SPRD_PD_HEADER_EXT_SHIFT		15
#define SPRD_PD_HEADER_EXT_MASK			0x1
#define SPRD_PD_HEADER_CNT_SHIFT		12
#define SPRD_PD_HEADER_CNT_MASK			0x7
#define SPRD_PD_HEADER_ID_SHIFT			9
#define SPRD_PD_HEADER_ID_MASK			0x7
#define SPRD_PD_HEADER_PWR_ROLE			BIT(8)
#define SPRD_PD_HEADER_REV_SHIFT		6
#define SPRD_PD_HEADER_REV_MASK			0x3
#define SPRD_PD_HEADER_DATA_ROLE		BIT(5)
#define SPRD_PD_HEADER_TYPE_SHIFT		0
#define SPRD_PD_HEADER_TYPE_MASK		0x1f

#define SPRD_PD_HEADER(type, pwr, data, rev, id, cnt, ext_hdr)		\
	((((type) & SPRD_PD_HEADER_TYPE_MASK) << SPRD_PD_HEADER_TYPE_SHIFT) |	\
	 ((pwr) == TYPEC_SOURCE ? SPRD_PD_HEADER_PWR_ROLE : 0) |		\
	 ((data) == TYPEC_HOST ? SPRD_PD_HEADER_DATA_ROLE : 0) |		\
	 (rev << SPRD_PD_HEADER_REV_SHIFT) |					\
	 (((id) & SPRD_PD_HEADER_ID_MASK) << SPRD_PD_HEADER_ID_SHIFT) |		\
	 (((cnt) & SPRD_PD_HEADER_CNT_MASK) << SPRD_PD_HEADER_CNT_SHIFT) |	\
	 ((ext_hdr) ? SPRD_PD_HEADER_EXT_HDR : 0))

#define SPRD_PD_HEADER_LE(type, pwr, data, rev, id, cnt) \
	cpu_to_le16(SPRD_PD_HEADER((type), (pwr), (data), (rev), (id), (cnt), (0)))

#define SPRD_PD_HEADER_EXT(type, pwr, data, rev, id, cnt) \
	SPRD_PD_HEADER((type), (pwr), (data), (rev), (id), (cnt), (1))

#define SPRD_PD_HEADER_EXT_LE(type, pwr, data, rev, id, cnt) \
	cpu_to_le16(SPRD_PD_HEADER((type), (pwr), (data), (rev), (id), (cnt), (1)))

static inline unsigned int sprd_pd_header_ext(u16 header)
{
	return (header >> SPRD_PD_HEADER_EXT_SHIFT) & SPRD_PD_HEADER_EXT_MASK;
}

static inline unsigned int sprd_pd_header_ext_le(__le16 header)
{
	return sprd_pd_header_ext(le16_to_cpu(header));
}

static inline unsigned int sprd_pd_header_cnt(u16 header)
{
	return (header >> SPRD_PD_HEADER_CNT_SHIFT) & SPRD_PD_HEADER_CNT_MASK;
}

static inline unsigned int sprd_pd_header_cnt_le(__le16 header)
{
	return sprd_pd_header_cnt(le16_to_cpu(header));
}

static inline unsigned int sprd_pd_header_type(u16 header)
{
	return (header >> SPRD_PD_HEADER_TYPE_SHIFT) & SPRD_PD_HEADER_TYPE_MASK;
}

static inline unsigned int sprd_pd_header_type_le(__le16 header)
{
	return sprd_pd_header_type(le16_to_cpu(header));
}

static inline unsigned int sprd_pd_header_msgid(u16 header)
{
	return (header >> SPRD_PD_HEADER_ID_SHIFT) & SPRD_PD_HEADER_ID_MASK;
}

static inline unsigned int sprd_pd_header_msgid_le(__le16 header)
{
	return sprd_pd_header_msgid(le16_to_cpu(header));
}

static inline unsigned int sprd_pd_header_rev(u16 header)
{
	return (header >> SPRD_PD_HEADER_REV_SHIFT) & SPRD_PD_HEADER_REV_MASK;
}

static inline unsigned int sprd_pd_header_rev_le(__le16 header)
{
	return sprd_pd_header_rev(le16_to_cpu(header));
}

#define SPRD_PD_EXT_HDR_CHUNKED			BIT(15)
#define SPRD_PD_EXT_HDR_CHUNK_NUM_SHIFT		11
#define SPRD_PD_EXT_HDR_CHUNK_NUM_MASK		0xf
#define SPRD_PD_EXT_HDR_REQ_CHUNK		BIT(10)
#define SPRD_PD_EXT_HDR_REQ_CHUNK_SHIFT		10
#define SPRD_PD_EXT_HDR_DATA_SIZE_SHIFT		0
#define SPRD_PD_EXT_HDR_DATA_SIZE_MASK		0x1ff

#define SPRD_PD_EXT_HDR(data_size, req_chunk, chunk_num, chunked)				\
	((((data_size) & SPRD_PD_EXT_HDR_DATA_SIZE_MASK) << SPRD_PD_EXT_HDR_DATA_SIZE_SHIFT) |	\
	 ((req_chunk) ? SPRD_PD_EXT_HDR_REQ_CHUNK : 0) |					\
	 (((chunk_num) & SPRD_PD_EXT_HDR_CHUNK_NUM_MASK) << SPRD_PD_EXT_HDR_CHUNK_NUM_SHIFT) |	\
	 ((chunked) ? SPRD_PD_EXT_HDR_CHUNKED : 0))

#define SPRD_PD_EXT_HDR_LE(data_size, req_chunk, chunk_num, chunked) \
	cpu_to_le16(SPRD_PD_EXT_HDR((data_size), (req_chunk), (chunk_num), (chunked)))

static inline unsigned int sprd_pd_ext_header_chunk_num(u16 ext_header)
{
	return (ext_header >> SPRD_PD_EXT_HDR_CHUNK_NUM_SHIFT) &
		SPRD_PD_EXT_HDR_CHUNK_NUM_MASK;
}

static inline unsigned int sprd_pd_ext_header_chunk_num_le(__le16 ext_header)
{
	return sprd_pd_ext_header_chunk_num(le16_to_cpu(ext_header));
}

static inline unsigned int sprd_pd_ext_header_data_size(u16 ext_header)
{
	return (ext_header >> SPRD_PD_EXT_HDR_DATA_SIZE_SHIFT) &
		SPRD_PD_EXT_HDR_DATA_SIZE_MASK;
}

static inline unsigned int sprd_pd_ext_header_data_size_le(__le16 ext_header)
{
	return sprd_pd_ext_header_data_size(le16_to_cpu(ext_header));
}

static inline unsigned int sprd_pd_ext_header_request_chunk(u16 ext_header)
{
	return (ext_header & SPRD_PD_EXT_HDR_REQ_CHUNK) >>
		SPRD_PD_EXT_HDR_REQ_CHUNK_SHIFT;
}

static inline unsigned int sprd_pd_ext_header_request_chunk_le(__le16 ext_header)
{
	return sprd_pd_ext_header_request_chunk(le16_to_cpu(ext_header));
}

#define SPRD_PD_MAX_PAYLOAD		7
#define SPRD_PD_EXT_MAX_CHUNK_DATA	26
#define SPRD_PD_EXT_MAX_MSG_LEN		260

/**
  * struct sprd_pd_chunked_ext_message_data - PD chunked extended message data as
  *					 seen on wire
  * @header:    PD extended message header
  * @data:      PD extended message data
  */
struct sprd_pd_chunked_ext_message_data {
	__le16 header;
	u8 data[SPRD_PD_EXT_MAX_CHUNK_DATA];
} __packed;

/**
  * struct sprd_pd_message - PD message as seen on wire
  * @header:    PD message header
  * @payload:   PD message payload
  * @ext_msg:   PD message chunked extended message data
  */
struct sprd_pd_message {
	__le16 header;
	union {
		__le32 payload[SPRD_PD_MAX_PAYLOAD];
		struct sprd_pd_chunked_ext_message_data ext_msg;
	};
} __packed;

/* PDO: Power Data Object */
#define SPRD_PDO_MAX_OBJECTS		7

enum sprd_pd_pdo_type {
	SPRD_PDO_TYPE_FIXED = 0,
	SPRD_PDO_TYPE_BATT = 1,
	SPRD_PDO_TYPE_VAR = 2,
	SPRD_PDO_TYPE_APDO = 3,
};

#define SPRD_PDO_TYPE_SHIFT			30
#define SPRD_PDO_TYPE_MASK			0x3

#define SPRD_PDO_TYPE(t)			((t) << SPRD_PDO_TYPE_SHIFT)

#define SPRD_PDO_VOLT_MASK			0x3ff
#define SPRD_PDO_CURR_MASK			0x3ff
#define SPRD_PDO_PWR_MASK			0x3ff

#define SPRD_PDO_FIXED_DUAL_ROLE		BIT(29)	/* Power role swap supported */
#define SPRD_PDO_FIXED_SUSPEND			BIT(28) /* USB Suspend supported (Source) */
#define SPRD_PDO_FIXED_HIGHER_CAP		BIT(28) /* Requires more than vSafe5V (Sink) */
#define SPRD_PDO_FIXED_EXTPOWER			BIT(27) /* Externally powered */
#define SPRD_PDO_FIXED_USB_COMM			BIT(26) /* USB communications capable */
#define SPRD_PDO_FIXED_DATA_SWAP		BIT(25) /* Data role swap supported */
/* Unchunked Extended Message supported (Source) */
#define SPRD_PDO_FIXED_UNCHUNK_EXT		BIT(24)
#define SPRD_PDO_FIXED_FRS_CURR_MASK		(BIT(24) | BIT(23)) /* FR_Swap Current (Sink) */
#define SPRD_PDO_FIXED_VOLT_SHIFT		10	/* 50mV units */
#define SPRD_PDO_FIXED_CURR_SHIFT		0	/* 10mA units */

#define SPRD_PDO_FIXED_VOLT(mv)		\
	((((mv) / 50) & SPRD_PDO_VOLT_MASK) << SPRD_PDO_FIXED_VOLT_SHIFT)

#define SPRD_PDO_FIXED_CURR(ma)		\
	((((ma) / 10) & SPRD_PDO_CURR_MASK) << SPRD_PDO_FIXED_CURR_SHIFT)

#define SPRD_PDO_FIXED(mv, ma, flags)			\
	(SPRD_PDO_TYPE(SPRD_PDO_TYPE_FIXED) | (flags) |		\
	 SPRD_PDO_FIXED_VOLT(mv) | SPRD_PDO_FIXED_CURR(ma))

#define SPRD_VSAFE5V				5000 /* mv units */

#define SPRD_PDO_BATT_MAX_VOLT_SHIFT		20	/* 50mV units */
#define SPRD_PDO_BATT_MIN_VOLT_SHIFT		10	/* 50mV units */
#define SPRD_PDO_BATT_MAX_PWR_SHIFT		0	/* 250mW units */

#define SPRD_PDO_BATT_MIN_VOLT(mv)		\
	((((mv) / 50) & SPRD_PDO_VOLT_MASK) << SPRD_PDO_BATT_MIN_VOLT_SHIFT)

#define SPRD_PDO_BATT_MAX_VOLT(mv)		\
	((((mv) / 50) & SPRD_PDO_VOLT_MASK) << SPRD_PDO_BATT_MAX_VOLT_SHIFT)

#define SPRD_PDO_BATT_MAX_POWER(mw)		\
	((((mw) / 250) & SPRD_PDO_PWR_MASK) << SPRD_PDO_BATT_MAX_PWR_SHIFT)

#define SPRD_PDO_BATT(min_mv, max_mv, max_mw)			\
	(SPRD_PDO_TYPE(SPRD_PDO_TYPE_BATT) | SPRD_PDO_BATT_MIN_VOLT(min_mv) |	\
	 SPRD_PDO_BATT_MAX_VOLT(max_mv) | SPRD_PDO_BATT_MAX_POWER(max_mw))

#define SPRD_PDO_VAR_MAX_VOLT_SHIFT		20	/* 50mV units */
#define SPRD_PDO_VAR_MIN_VOLT_SHIFT		10	/* 50mV units */
#define SPRD_PDO_VAR_MAX_CURR_SHIFT		0	/* 10mA units */

#define SPRD_PDO_VAR_MIN_VOLT(mv)		\
	((((mv) / 50) & SPRD_PDO_VOLT_MASK) << SPRD_PDO_VAR_MIN_VOLT_SHIFT)

#define SPRD_PDO_VAR_MAX_VOLT(mv)		\
	((((mv) / 50) & SPRD_PDO_VOLT_MASK) << SPRD_PDO_VAR_MAX_VOLT_SHIFT)

#define SPRD_PDO_VAR_MAX_CURR(ma)		\
	((((ma) / 10) & SPRD_PDO_CURR_MASK) << SPRD_PDO_VAR_MAX_CURR_SHIFT)

#define SPRD_PDO_VAR(min_mv, max_mv, max_ma)				\
	(SPRD_PDO_TYPE(SPRD_PDO_TYPE_VAR) | SPRD_PDO_VAR_MIN_VOLT(min_mv) |	\
	 SPRD_PDO_VAR_MAX_VOLT(max_mv) | SPRD_PDO_VAR_MAX_CURR(max_ma))

enum sprd_pd_apdo_type {
	SPRD_APDO_TYPE_PPS = 0,
};

#define SPRD_PDO_APDO_TYPE_SHIFT	28	/* Only valid value currently is 0x0 - PPS */
#define SPRD_PDO_APDO_TYPE_MASK		0x3

#define SPRD_PDO_APDO_TYPE(t)	((t) << SPRD_PDO_APDO_TYPE_SHIFT)

#define SPRD_PDO_PPS_APDO_MAX_VOLT_SHIFT	17	/* 100mV units */
#define SPRD_PDO_PPS_APDO_MIN_VOLT_SHIFT	8	/* 100mV units */
#define SPRD_PDO_PPS_APDO_MAX_CURR_SHIFT	0	/* 50mA units */

#define SPRD_PDO_PPS_APDO_VOLT_MASK		0xff
#define SPRD_PDO_PPS_APDO_CURR_MASK		0x7f

#define SPRD_PDO_PPS_APDO_MIN_VOLT(mv)	\
	((((mv) / 100) & SPRD_PDO_PPS_APDO_VOLT_MASK) << SPRD_PDO_PPS_APDO_MIN_VOLT_SHIFT)

#define SPRD_PDO_PPS_APDO_MAX_VOLT(mv)	\
	((((mv) / 100) & SPRD_PDO_PPS_APDO_VOLT_MASK) << SPRD_PDO_PPS_APDO_MAX_VOLT_SHIFT)

#define SPRD_PDO_PPS_APDO_MAX_CURR(ma)	\
	((((ma) / 50) & SPRD_PDO_PPS_APDO_CURR_MASK) << SPRD_PDO_PPS_APDO_MAX_CURR_SHIFT)

#define SPRD_PDO_PPS_APDO(min_mv, max_mv, max_ma)				\
	(SPRD_PDO_TYPE(SPRD_PDO_TYPE_APDO) | SPRD_PDO_APDO_TYPE(SPRD_APDO_TYPE_PPS) |	\
	SPRD_PDO_PPS_APDO_MIN_VOLT(min_mv) | SPRD_PDO_PPS_APDO_MAX_VOLT(max_mv) |	\
	SPRD_PDO_PPS_APDO_MAX_CURR(max_ma))

static inline enum sprd_pd_pdo_type sprd_pdo_type(u32 pdo)
{
	return (pdo >> SPRD_PDO_TYPE_SHIFT) & SPRD_PDO_TYPE_MASK;
}

static inline unsigned int sprd_pdo_fixed_voltage(u32 pdo)
{
	return ((pdo >> SPRD_PDO_FIXED_VOLT_SHIFT) & SPRD_PDO_VOLT_MASK) * 50;
}

static inline unsigned int sprd_pdo_min_voltage(u32 pdo)
{
	return ((pdo >> SPRD_PDO_VAR_MIN_VOLT_SHIFT) & SPRD_PDO_VOLT_MASK) * 50;
}

static inline unsigned int sprd_pdo_max_voltage(u32 pdo)
{
	return ((pdo >> SPRD_PDO_VAR_MAX_VOLT_SHIFT) & SPRD_PDO_VOLT_MASK) * 50;
}

static inline unsigned int sprd_pdo_max_current(u32 pdo)
{
	return ((pdo >> SPRD_PDO_VAR_MAX_CURR_SHIFT) & SPRD_PDO_CURR_MASK) * 10;
}

static inline unsigned int sprd_pdo_max_power(u32 pdo)
{
	return ((pdo >> SPRD_PDO_BATT_MAX_PWR_SHIFT) & SPRD_PDO_PWR_MASK) * 250;
}

static inline enum sprd_pd_apdo_type sprd_pdo_apdo_type(u32 pdo)
{
	return (pdo >> SPRD_PDO_APDO_TYPE_SHIFT) & SPRD_PDO_APDO_TYPE_MASK;
}

static inline unsigned int sprd_pdo_pps_apdo_min_voltage(u32 pdo)
{
	return ((pdo >> SPRD_PDO_PPS_APDO_MIN_VOLT_SHIFT) & SPRD_PDO_PPS_APDO_VOLT_MASK) * 100;
}

static inline unsigned int sprd_pdo_pps_apdo_max_voltage(u32 pdo)
{
	return ((pdo >> SPRD_PDO_PPS_APDO_MAX_VOLT_SHIFT) & SPRD_PDO_PPS_APDO_VOLT_MASK) * 100;
}

static inline unsigned int sprd_pdo_pps_apdo_max_current(u32 pdo)
{
	return ((pdo >> SPRD_PDO_PPS_APDO_MAX_CURR_SHIFT) & SPRD_PDO_PPS_APDO_CURR_MASK) * 50;
}

/* RDO: Request Data Object */
#define SPRD_RDO_OBJ_POS_SHIFT			28
#define SPRD_RDO_OBJ_POS_MASK			0x7
#define SPRD_RDO_GIVE_BACK			BIT(27)	/* Supports reduced operating current */
#define SPRD_RDO_CAP_MISMATCH			BIT(26) /* Not satisfied by source caps */
#define SPRD_RDO_USB_COMM			BIT(25) /* USB communications capable */
#define SPRD_RDO_NO_SUSPEND			BIT(24) /* USB Suspend not supported */

#define SPRD_RDO_PWR_MASK			0x3ff
#define SPRD_RDO_CURR_MASK			0x3ff

#define SPRD_RDO_FIXED_OP_CURR_SHIFT		10
#define SPRD_RDO_FIXED_MAX_CURR_SHIFT		0

#define SPRD_RDO_OBJ(idx)		\
	(((idx) & SPRD_RDO_OBJ_POS_MASK) << SPRD_RDO_OBJ_POS_SHIFT)

#define SPRD_PDO_FIXED_OP_CURR(ma)		\
	((((ma) / 10) & SPRD_RDO_CURR_MASK) << SPRD_RDO_FIXED_OP_CURR_SHIFT)

#define SPRD_PDO_FIXED_MAX_CURR(ma)		\
	((((ma) / 10) & SPRD_RDO_CURR_MASK) << SPRD_RDO_FIXED_MAX_CURR_SHIFT)

#define SPRD_RDO_FIXED(idx, op_ma, max_ma, flags)			\
	(SPRD_RDO_OBJ(idx) | (flags) |				\
	 SPRD_PDO_FIXED_OP_CURR(op_ma) | SPRD_PDO_FIXED_MAX_CURR(max_ma))

#define SPRD_RDO_BATT_OP_PWR_SHIFT		10	/* 250mW units */
#define SPRD_RDO_BATT_MAX_PWR_SHIFT		0	/* 250mW units */

#define SPRD_RDO_BATT_OP_PWR(mw)		\
	((((mw) / 250) & SPRD_RDO_PWR_MASK) << SPRD_RDO_BATT_OP_PWR_SHIFT)

#define SPRD_RDO_BATT_MAX_PWR(mw)		\
	((((mw) / 250) & SPRD_RDO_PWR_MASK) << SPRD_RDO_BATT_MAX_PWR_SHIFT)

#define SPRD_RDO_BATT(idx, op_mw, max_mw, flags)			\
	(SPRD_RDO_OBJ(idx) | (flags) |				\
	 SPRD_RDO_BATT_OP_PWR(op_mw) | SPRD_RDO_BATT_MAX_PWR(max_mw))

#define SPRD_RDO_PROG_VOLT_MASK			0x7ff
#define SPRD_RDO_PROG_CURR_MASK			0x7f

#define SPRD_RDO_PROG_VOLT_SHIFT		9
#define SPRD_RDO_PROG_CURR_SHIFT		0

#define SPRD_RDO_PROG_VOLT_MV_STEP		20
#define SPRD_RDO_PROG_CURR_MA_STEP		50

#define SPRD_PDO_PROG_OUT_VOLT(mv)		\
	((((mv) / SPRD_RDO_PROG_VOLT_MV_STEP) & SPRD_RDO_PROG_VOLT_MASK)	\
	 << SPRD_RDO_PROG_VOLT_SHIFT)

#define SPRD_PDO_PROG_OP_CURR(ma)		\
	((((ma) / SPRD_RDO_PROG_CURR_MA_STEP) & SPRD_RDO_PROG_CURR_MASK)	\
	 << SPRD_RDO_PROG_CURR_SHIFT)

#define SPRD_RDO_PROG(idx, out_mv, op_ma, flags)			\
	(SPRD_RDO_OBJ(idx) | (flags) |				\
	 SPRD_PDO_PROG_OUT_VOLT(out_mv) | SPRD_PDO_PROG_OP_CURR(op_ma))

static inline unsigned int sprd_rdo_index(u32 rdo)
{
	return (rdo >> SPRD_RDO_OBJ_POS_SHIFT) & SPRD_RDO_OBJ_POS_MASK;
}

static inline unsigned int sprd_rdo_op_current(u32 rdo)
{
	return ((rdo >> SPRD_RDO_FIXED_OP_CURR_SHIFT) & SPRD_RDO_CURR_MASK) * 10;
}

static inline unsigned int sprd_rdo_max_current(u32 rdo)
{
	return ((rdo >> SPRD_RDO_FIXED_MAX_CURR_SHIFT) &
		SPRD_RDO_CURR_MASK) * 10;
}

static inline unsigned int sprd_rdo_op_power(u32 rdo)
{
	return ((rdo >> SPRD_RDO_BATT_OP_PWR_SHIFT) & SPRD_RDO_PWR_MASK) * 250;
}

static inline unsigned int sprd_rdo_max_power(u32 rdo)
{
	return ((rdo >> SPRD_RDO_BATT_MAX_PWR_SHIFT) & SPRD_RDO_PWR_MASK) * 250;
}

/* USB PD timers and counters */
#define SPRD_PD_T_NO_RESPONSE			5000	/* 4.5 - 5.5 seconds */
#define SPRD_PD_T_DB_DETECT			10000	/* 10 - 15 seconds */
#define SPRD_PD_T_SEND_SOURCE_CAP		150	/* 100 - 200 ms, PD3.1 Rec Val: 150ms */
#define SPRD_PD_T_SEND_SOURCE_CAP_RESET		700

/*
 * tSenderResponse value:
 * a) PD2.0 / PD3.0 standard protocol: 24ms - 30ms
 * b) PD3.1 standard protocol: 27ms - 33ms, Rev Val: 30ms
 */
#define SPRD_PD_T_SENDER_RESPONSE		24
#define SPRD_PD_T_SENDER_RESPONSE_PD2		27	/* 24 - 30 ms, relaxed */
#define SPRD_PD_T_SENDER_RESPONSE_PD3		30	/* 27 - 33 ms, relaxed */

#define SPRD_PD_T_SENDER_RESPONSE_DR		200	/* 24 - 30 ms, relaxed */
#define SPRD_PD_T_SENDER_RESPONSE_PR		150	/* 24 - 30 ms, relaxed */
#define SPRD_PD_T_SENDER_RESPONSE_RESET		700
#define SPRD_PD_T_SOURCE_ACTIVITY		45
#define SPRD_PD_T_SINK_ACTIVITY			135
#define SPRD_PD_T_SINK_WAIT_CAP			460	/* 310 - 620 ms */
#define SPRD_PD_T_SINK_WAIT_CAP_PR		620	/* 310 - 620 ms */
#define SPRD_PD_T_PS_TRANSITION			500
#define SPRD_PD_T_SRC_TRANSITION		35
#define SPRD_PD_T_DRP_SNK			40
#define SPRD_PD_T_DRP_SRC			30
#define SPRD_PD_T_PS_SOURCE_OFF			835 /* SPR mode: 750ms-920ms */
#define SPRD_PD_T_PS_SOURCE_ON			100 /* allow cc debounce, send source caps later */
#define SPRD_PD_T_PS_SOURCE_ON_RESET		5
#define SPRD_PD_T_PS_SOURCE_ON_SWAP		420 /* 390 - 480 ms, relaxed */
#define SPRD_PD_T_PS_HARD_RESET			30
#define SPRD_PD_T_SRC_RECOVER			760
#define SPRD_PD_T_SRC_RECOVER_MAX		1000
#define SPRD_PD_T_SRC_TURN_ON			275
#define SPRD_PD_T_SAFE_0V			650
#define SPRD_PD_T_VCONN_SOURCE_ON		100
#define SPRD_PD_T_SINK_REQUEST			100	/* 100 ms minimum */
#define SPRD_PD_T_ERROR_RECOVERY		100	/* minimum 25 is insufficient */
#define SPRD_PD_T_SRCSWAPSTDBY			200     /* Maximum of 650ms */
#define SPRD_PD_T_NEWSRC			250     /* Maximum of 275ms */
#define SPRD_PD_T_NEWSRC_SWAP			150     /* Maximum of 275ms */
#define SPRD_PD_T_SWAP_SRC_START		50	/* Minimum of 20ms */

#define SPRD_PD_T_DRP_TRY			100	/* 75 - 150 ms */
#define SPRD_PD_T_DRP_TRYWAIT			600	/* 400 - 800 ms */

#define SPRD_PD_T_CC_DEBOUNCE			5	/* 100 - 200 ms */
#define SPRD_PD_T_CC_DEBOUNCE_SWAP		100	/* 100 - 200 ms */
#define SPRD_PD_T_PD_DEBOUNCE			20	/* 10 - 20 ms */

#define SPRD_PD_N_CAPS_COUNT			(SPRD_PD_T_NO_RESPONSE / SPRD_PD_T_SEND_SOURCE_CAP)
#define SPRD_PD_N_HARD_RESET_COUNT		2
#define SPRD_PD_T_PRO_ERR_SOFTRESET		0	/* max 15 ms */

#define SPRD_PD_T_CHUNK_NOT_SUPP		40	/* 40 - 50 ms */
#define SPRD_PD_T_VBUS_ON_COMPENSATE		360

#endif /* __LINUX_USB_SPRD_PD_H */
