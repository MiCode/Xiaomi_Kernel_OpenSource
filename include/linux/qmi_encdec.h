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

#ifndef _QMI_ENCDEC_H_
#define _QMI_ENCDEC_H_

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/socket.h>
#include <linux/gfp.h>

#define QMI_REQUEST_CONTROL_FLAG 0x00
#define QMI_RESPONSE_CONTROL_FLAG 0x02
#define QMI_INDICATION_CONTROL_FLAG 0x04
#define QMI_HEADER_SIZE 7

/**
 * elem_type - Enum to identify the data type of elements in a data
 *             structure.
 */
enum elem_type {
	QMI_OPT_FLAG = 1,
	QMI_DATA_LEN,
	QMI_UNSIGNED_1_BYTE,
	QMI_UNSIGNED_2_BYTE,
	QMI_UNSIGNED_4_BYTE,
	QMI_UNSIGNED_8_BYTE,
	QMI_SIGNED_2_BYTE_ENUM,
	QMI_SIGNED_4_BYTE_ENUM,
	QMI_STRUCT,
	QMI_EOTI,
};

/**
 * array_type - Enum to identify if an element in a data structure is
 *              an array. If so, then is it a static length array or a
 *              variable length array.
 */
enum array_type {
	NO_ARRAY = 0,
	STATIC_ARRAY = 1,
	VAR_LEN_ARRAY = 2,
};

/**
 * elem_info - Data structure to specify information about an element
 *               in a data structure. An array of this data structure
 *               can be used to specify info about a complex data
 *               structure to be encoded/decoded.
 *
 * @data_type: Data type of this element.
 * @elem_len: Array length of this element, if an array.
 * @elem_size: Size of a single instance of this data type.
 * @is_array: Array type of this element.
 * @tlv_type: QMI message specific type to identify which element
 *            is present in an incoming message.
 * @offset: To identify the address of the first instance of this
 *          element in the data structure.
 * @ei_array: Array to provide information about the nested structure
 *            within a data structure to be encoded/decoded.
 */
struct elem_info {
	enum elem_type data_type;
	uint32_t elem_len;
	uint32_t elem_size;
	enum array_type is_array;
	uint8_t tlv_type;
	uint32_t offset;
	struct elem_info *ei_array;
};

/**
 * @msg_desc - Describe about the main/outer structure to be
 *		  encoded/decoded.
 *
 * @max_msg_len: Maximum possible length of the QMI message.
 * @ei_array: Array to provide information about a data structure.
 */
struct msg_desc {
	uint16_t msg_id;
	int max_msg_len;
	struct elem_info *ei_array;
};

struct qmi_header {
	unsigned char cntl_flag;
	uint16_t txn_id;
	uint16_t msg_id;
	uint16_t msg_len;
} __attribute__((__packed__));

static inline void encode_qmi_header(unsigned char *buf,
			unsigned char cntl_flag, uint16_t txn_id,
			uint16_t msg_id, uint16_t msg_len)
{
	struct qmi_header *hdr = (struct qmi_header *)buf;

	hdr->cntl_flag = cntl_flag;
	hdr->txn_id = txn_id;
	hdr->msg_id = msg_id;
	hdr->msg_len = msg_len;
}

static inline void decode_qmi_header(unsigned char *buf,
			unsigned char *cntl_flag, uint16_t *txn_id,
			uint16_t *msg_id, uint16_t *msg_len)
{
	struct qmi_header *hdr = (struct qmi_header *)buf;

	*cntl_flag = hdr->cntl_flag;
	*txn_id = hdr->txn_id;
	*msg_id = hdr->msg_id;
	*msg_len = hdr->msg_len;
}

#ifdef CONFIG_QMI_ENCDEC
/**
 * qmi_kernel_encode() - Encode to QMI message wire format
 * @desc: Pointer to structure descriptor.
 * @out_buf: Buffer to hold the encoded QMI message.
 * @out_buf_len: Length of the out buffer.
 * @in_c_struct: C Structure to be encoded.
 *
 * @return: size of encoded message on success, < 0 on error.
 */
int qmi_kernel_encode(struct msg_desc *desc,
		      void *out_buf, uint32_t out_buf_len,
		      void *in_c_struct);

/**
 * qmi_kernel_decode() - Decode to C Structure format
 * @desc: Pointer to structure descriptor.
 * @out_c_struct: Buffer to hold the decoded C structure.
 * @in_buf: Buffer containg the QMI message to be decoded.
 * @in_buf_len: Length of the incoming QMI message.
 *
 * @return: 0 on success, < 0 on error.
 */
int qmi_kernel_decode(struct msg_desc *desc, void *out_c_struct,
		      void *in_buf, uint32_t in_buf_len);

#else
static inline int qmi_kernel_encode(struct msg_desc *desc,
				    void *out_buf, uint32_t out_buf_len,
				    void *in_c_struct)
{
	return -EOPNOTSUPP;
}

static inline int qmi_kernel_decode(struct msg_desc *desc,
				    void *out_c_struct,
				    void *in_buf, uint32_t in_buf_len)
{
	return -EOPNOTSUPP;
}
#endif

#endif
