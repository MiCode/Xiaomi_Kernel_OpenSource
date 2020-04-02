/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _UAPI_QCOTA_H
#define _UAPI_QCOTA_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define QCE_OTA_MAX_BEARER   31
#define OTA_KEY_SIZE 16   /* 128 bits of keys. */

enum qce_ota_dir_enum {
	QCE_OTA_DIR_UPLINK   = 0,
	QCE_OTA_DIR_DOWNLINK = 1,
	QCE_OTA_DIR_LAST
};

enum qce_ota_algo_enum {
	QCE_OTA_ALGO_KASUMI = 0,
	QCE_OTA_ALGO_SNOW3G = 1,
	QCE_OTA_ALGO_LAST
};

/**
 * struct qce_f8_req - qce f8 request
 * @data_in:	packets input data stream to be ciphered.
 *		If NULL, streaming mode operation.
 * @data_out:	ciphered packets output data.
 * @data_len:	length of data_in and data_out in bytes.
 * @count_c:	count-C, ciphering sequence number, 32 bit
 * @bearer:	5 bit of radio bearer identifier.
 * @ckey:	128 bits of confidentiality key,
 *		ckey[0] bit 127-120, ckey[1] bit 119-112,.., ckey[15] bit 7-0.
 * @direction:	uplink or donwlink.
 * @algorithm:	Kasumi, or Snow3G.
 *
 * If data_in is NULL, the engine will run in a special mode called
 * key stream mode. In this special mode, the engine will generate
 * key stream output for the number of bytes specified in the
 * data_len, based on the input parameters of direction, algorithm,
 * ckey, bearer, and count_c. The data_len is restricted to
 * the length of multiple of 16 bytes.  Application can then take the
 * output stream, do a exclusive or to the input data stream, and
 * generate the final cipher data stream.
 */
struct qce_f8_req {
	__u8  *data_in;
	__u8  *data_out;
	__u16  data_len;
	__u32  count_c;
	__u8   bearer;
	__u8   ckey[OTA_KEY_SIZE];
	enum qce_ota_dir_enum  direction;
	enum qce_ota_algo_enum algorithm;
};

/**
 * struct qce_f8_multi_pkt_req - qce f8 multiple packet request
 *			Muliptle packets with uniform size, and
 *			F8 ciphering parameters can be ciphered in a
 *			single request.
 *
 * @num_pkt:		number of packets.
 *
 * @cipher_start:	ciphering starts offset within a packet.
 *
 * @cipher_size:	number of bytes to be ciphered within a packet.
 *
 * @qce_f8_req:		description of the packet and F8 parameters.
 *			The following fields have special meaning for
 *			multiple packet operation,
 *
 *	@data_len:	data_len indicates the length of a packet.
 *
 *	@data_in:	packets are concatenated together in a byte
 *			stream started at data_in.
 *
 *	@data_out:	The returned ciphered output for multiple
 *			packets.
 *			Each packet ciphered output are concatenated
 *			together into a byte stream started at data_out.
 *			Note, each ciphered packet output area from
 *			offset 0 to cipher_start-1, and from offset
 *			cipher_size to data_len -1 are remained
 *			unaltered from packet input area.
 *	@count_c:	count-C of the first packet, 32 bit.
 *
 *
 *   In one request, multiple packets can be ciphered, and output to the
 *   data_out stream.
 *
 *   Packet data are laid out contiguously in sequence in data_in,
 *   and data_out area. Every packet is identical size.
 *   If the PDU is not byte aligned, set the data_len value of
 *   to the rounded up value of the packet size. Eg, PDU size of
 *   253 bits, set the packet size to 32 bytes. Next packet starts on
 *   the next byte boundary.
 *
 *   For each packet, data from offset 0 to cipher_start
 *   will be left unchanged and output to the data_out area.
 *   This area of the packet can be for the RLC header, which is not
 *   to be ciphered.
 *
 *   The ciphering of a packet starts from offset cipher_start, for
 *   cipher_size bytes of data. Data starting from
 *   offset cipher_start + cipher_size to the end of packet will be left
 *   unchanged and output to the dataOut area.
 *
 *   For each packet the input arguments of bearer, direction,
 *   ckey, algorithm have to be the same. count_c is the ciphering sequence
 *   number of the first packet. The 2nd packet's ciphering sequence
 *   number is assumed to be count_c + 1. The 3rd packet's ciphering sequence
 *   number is count_c + 2.....
 *
 */
struct qce_f8_multi_pkt_req {
	__u16    num_pkt;
	__u16    cipher_start;
	__u16    cipher_size;
	struct qce_f8_req qce_f8_req;
};

/**
 * struct qce_f8_variable_multi_pkt_req - qce f8 multiple packet request
 *                      Muliptle packets with variable size, and
 *                      F8 ciphering parameters can be ciphered in a
 *                      single request.
 *
 * @num_pkt:            number of packets.
 *
 * @cipher_iov[]:       array of iov of packets to be ciphered.
 *
 *
 * @qce_f8_req:         description of the packet and F8 parameters.
 *                      The following fields have special meaning for
 *                      multiple packet operation,
 *
 *      @data_len:      ignored.
 *
 *      @data_in:       ignored.
 *
 *      @data_out:      ignored.
 *
 *      @count_c:       count-C of the first packet, 32 bit.
 *
 *
 *   In one request, multiple packets can be ciphered.
 *
 *   The i-th packet are defined in cipher_iov[i-1].
 *   The ciphering of i-th packet starts from offset 0 of the PDU specified
 *   by cipher_iov[i-1].addr, for cipher_iov[i-1].size bytes of data.
 *   If the PDU is not byte aligned, set the cipher_iov[i-1].size value
 *   to the rounded up value of the packet size. Eg, PDU size of
 *   253 bits, set the packet size to 32 bytes.
 *
 *   Ciphering are done in place. That is, the ciphering
 *   input and output data are both in cipher_iov[i-1].addr for the i-th
 *   packet.
 *
 *   For each packet the input arguments of bearer, direction,
 *   ckey, algorithm have to be the same. count_c is the ciphering sequence
 *   number of the first packet. The 2nd packet's ciphering sequence
 *   number is assumed to be count_c + 1. The 3rd packet's ciphering sequence
 *   number is count_c + 2.....
 */

#define MAX_NUM_V_MULTI_PKT 20
struct cipher_iov {
	unsigned char  *addr;
	unsigned short  size;
};

struct qce_f8_variable_multi_pkt_req {
	unsigned short    num_pkt;
	struct cipher_iov cipher_iov[MAX_NUM_V_MULTI_PKT];
	struct qce_f8_req qce_f8_req;
};

/**
 * struct qce_f9_req - qce f9 request
 * @message:	message
 * @msize:	message size in bytes (include the last partial byte).
 * @last_bits:	valid bits in the last byte of message.
 * @mac_i:	32 bit message authentication code, to be returned.
 * @fresh:	random 32 bit number, one per user.
 * @count_i:	32 bit count-I integrity sequence number.
 * @direction:	uplink or donwlink.
 * @ikey:	128 bits of integrity key,
 *		ikey[0] bit 127-120, ikey[1] bit 119-112,.., ikey[15] bit 7-0.
 * @algorithm:  Kasumi, or Snow3G.
 */
struct qce_f9_req {
	__u8   *message;
	__u16   msize;
	__u8    last_bits;
	__u32   mac_i;
	__u32   fresh;
	__u32   count_i;
	enum qce_ota_dir_enum direction;
	__u8    ikey[OTA_KEY_SIZE];
	enum qce_ota_algo_enum algorithm;
};

#define QCOTA_IOC_MAGIC     0x85

#define QCOTA_F8_REQ _IOWR(QCOTA_IOC_MAGIC, 1, struct qce_f8_req)
#define QCOTA_F8_MPKT_REQ _IOWR(QCOTA_IOC_MAGIC, 2, struct qce_f8_multi_pkt_req)
#define QCOTA_F9_REQ _IOWR(QCOTA_IOC_MAGIC, 3, struct qce_f9_req)
#define QCOTA_F8_V_MPKT_REQ _IOWR(QCOTA_IOC_MAGIC, 4,\
				struct qce_f8_variable_multi_pkt_req)

#endif /* _UAPI_QCOTA_H */
