/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _QMI_ENCDEC_PRIV_H_
#define _QMI_ENCDEC_PRIV_H_

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/socket.h>
#include <linux/gfp.h>
#include <linux/qmi_encdec.h>

#define QMI_ENCDEC_ENCODE_TLV(type, length, p_dst) do { \
	*p_dst++ = type; \
	*p_dst++ = ((uint8_t)((length) & 0xFF)); \
	*p_dst++ = ((uint8_t)(((length) >> 8) & 0xFF)); \
} while (0)

#define QMI_ENCDEC_DECODE_TLV(p_type, p_length, p_src) do { \
	*p_type = (uint8_t)*p_src++; \
	*p_length = (uint8_t)*p_src++; \
	*p_length |= ((uint8_t)*p_src) << 8; \
} while (0)

#define QMI_ENCDEC_ENCODE_N_BYTES(p_dst, p_src, size) \
do { \
	memcpy(p_dst, p_src, size); \
	p_dst = (uint8_t *)p_dst + size; \
	p_src = (uint8_t *)p_src + size; \
} while (0)

#define QMI_ENCDEC_DECODE_N_BYTES(p_dst, p_src, size) \
do { \
	memcpy(p_dst, p_src, size); \
	p_dst = (uint8_t *)p_dst + size; \
	p_src = (uint8_t *)p_src + size; \
} while (0)

#define UPDATE_ENCODE_VARIABLES(temp_si, buf_dst, \
				encoded_bytes, tlv_len, encode_tlv, rc) \
do { \
	buf_dst = (uint8_t *)buf_dst + rc; \
	encoded_bytes += rc; \
	tlv_len += rc; \
	temp_si = temp_si + 1; \
	encode_tlv = 1; \
} while (0)

#define UPDATE_DECODE_VARIABLES(buf_src, decoded_bytes, rc) \
do { \
	buf_src = (uint8_t *)buf_src + rc; \
	decoded_bytes += rc; \
} while (0)

#endif
