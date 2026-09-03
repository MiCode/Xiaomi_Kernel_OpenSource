// SPDX-License-Identifier: GPL-2.0
/*
 * File:shub_protocol.c
 * Author:Sensor Hub Team
 *
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include "shub_common.h"
#include "shub_protocol.h"

/**
 * Function :  shub_search_flag
 * Description:
 * it find the matic word form the data buffer
 * Parameters:
 * stream : point the current  parse  data
 * data : point the uart buffer data
 * len       : the receive data length
 * processed_len : it deal with data len
 * Return : void
 */
static void shub_search_flag(struct shub_data_processor *stream,
			     u8 *data, u16 len,
			     u16 *processed_len)
{
	u16 headsize = stream->head_size;
	u8 *start_data = data;
	int i = 0;

	/* the magic number is 4 '~' */
	for (i = 0; i < len; i++) {
		if (*start_data == 0x7E) {
			headsize++;
			/* we got the 4 magic '~'  */
			if (headsize == SHUB_MAGIC_NUMBER_LEN) {
				start_data++;
				memset(stream->cur_header, SHUB_MAGIC_NUMBER,
				       SHUB_MAGIC_NUMBER_LEN);
				stream->state = SHUB_RECV_COLLECT_HEAD;
				break;
			}
		} else {
			headsize = 0;
		}
		start_data++;
	}
	stream->head_size = headsize;
	*processed_len = start_data - data;
}

/**
 * Function :  shub_checksum
 * Description :
 * it Calculate the CRC for the 8 bytes in head buffer
 * Parameters :
 * head_data: point the head data
 * Return : void
 */
static u16 shub_checksum(u8 *head_data)
{
	/*
	 * The first 4 octet is 0x7e
	 * 0x7e7e + 0x7e7e = 0xfcfc
	 */
	u32 sum = 0xfcfc;
	u16 n_add;

	head_data += 4;
	n_add = *head_data++;
	n_add <<= 8;
	n_add += *head_data++;
	sum += n_add;

	n_add = *head_data++;
	n_add <<= 8;
	n_add += *head_data++;
	sum += n_add;

	/* The carry is 2 at most, so we need 2 additions at most. */
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

/**
 * Function :  shub_data_checksum
 * Description :
 * it auto fill the encode head context in one packet
 * Parameters:
 * data : point the send data context
 * out_len : the packet length
 * Return :
 * output CRC bytes
 */
static u16 shub_data_checksum(u8 *data, u16 out_len)
{
	unsigned int sum = 0;
	u16 n_add;

	if (!data || out_len == 0)
		return 0;

	while (out_len > 2) {
		n_add = *data++;
		n_add <<= 8;
		n_add += *data++;
		sum += n_add;
		out_len -= 2;
	}
	if (out_len == 2) {
		n_add = *data++;
		n_add <<= 8;
		n_add += *data++;
		sum += n_add;
	} else {
		n_add = *data++;
		n_add <<= 8;
		sum += n_add;
	}
	/*The carry is 2 at most, so we need 2 additions at most. */
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);

	return (u16)(~sum);
}

static void research_flag(struct shub_data_processor *stream)
{
	u8 *start = stream->cur_header;
	int i = 0;

	for (i = SHUB_MAX_HEAD_LEN - 1; i > 2 ;  i--) {
		if ((i == 3) && (start[SHUB_MAX_HEAD_LEN - 1] != 0x7e)) {
			stream->head_size = 0;
			stream->state = SHUB_RECV_SEARCH_FLAG;
			break;
		}

		if ((start[i] == 0x7e)
			&& (start[i - 1] == 0x7e)
			&& (start[i - 2] == 0x7e)
			&& (start[i - 3] == 0x7e)) {
			memmove(start, start + i - 3, SHUB_MAX_HEAD_LEN - (i - 3));
			stream->head_size = SHUB_MAX_HEAD_LEN - (i - 3);
			stream->state = SHUB_RECV_COLLECT_HEAD;
			break;
		}
	}

	if ((stream->head_size == SHUB_MAX_HEAD_LEN) &&
		(start[SHUB_MAX_HEAD_LEN - 1] == 0x7e)) {
		u16 value_2_byte = 0;

		value_2_byte |= start[7];
		value_2_byte <<= 8;
		value_2_byte |= start[8];

		if ((0xffff & value_2_byte) == 0x7e7e)
			stream->head_size = 3;
		else if ((0xff & value_2_byte) == 0x7e)
			stream->head_size = 2;
		else
			stream->head_size = 1;

		stream->state = SHUB_RECV_SEARCH_FLAG;
	}
}

static void shub_collect_header(struct shub_data_processor *stream,
				u8 *data, u16 len,
				u16 *processed_len)
{
	u16 headsize = stream->head_size;
	u16 remain_len = SHUB_MAX_HEAD_LEN - headsize;
	u16 processed_length;
	u16 crc;
	u16 crc_inframe;

	processed_length = remain_len > len ? len : remain_len;
	memcpy(stream->cur_header + headsize, data, processed_length);
	headsize += processed_length;
	*processed_len = processed_length;
	stream->head_size = headsize;

	if (headsize != SHUB_MAX_HEAD_LEN) /* We have not got 10 bytes*/
		return;

	/* We have got 10 bytes
	 * Calculate the checksum (only 8 bytes in head buffer)
	 */
	crc = shub_checksum(stream->cur_header);
	crc_inframe = stream->cur_header[8];
	crc_inframe <<= 8;
	crc_inframe |= stream->cur_header[9];
	if (crc == crc_inframe)	{ /* We have got a right header*/
		u16 data_len;

		/* Set the frame length here*/
		data_len = stream->cur_header[6];
		data_len <<= 8;
		data_len |= stream->cur_header[7];
		stream->data_len = data_len;
		stream->cmd_data.type = stream->cur_header[4];
		stream->cmd_data.subtype = stream->cur_header[5];
		stream->cmd_data.length = data_len;
		if (data_len == 0) {
			shub_dispatch(&stream->cmd_data);
			shub_init_parse_packet(stream);
		} else {
			if (data_len <
			    (MAX_MSG_BUFF_SIZE - SHUB_MAX_HEAD_LEN -
			     SHUB_MAX_DATA_CRC)) {
				stream->state = SHUB_RECV_DATA;
			} else {
				research_flag(stream);
				pr_err("dataLen=%d\n", data_len);
			}
		}
	} else {
		pr_err("crc_inframe=0x%x crc=0x%x\n", crc_inframe, crc);
		research_flag(stream);
	}
}

static int shub_collect_data(struct shub_data_processor *stream,
			     u8 *data, u16 len,
			     u16 *processed_len)
{
	u16 n_frame_remain =
	    stream->data_len - stream->received_data_len + SHUB_MAX_DATA_CRC;
	struct cmd_data *p_packet = &stream->cmd_data;
	u16 n_copy = n_frame_remain > len ? len : n_frame_remain;
	u16 data_crc;
	u16 crc_inframe;
	u16 i;

	memcpy(p_packet->buff + stream->received_data_len, data, n_copy);
	stream->received_data_len += n_copy;

	*processed_len = n_copy;
	/*  Have we got the whole frame? */
	if (stream->received_data_len ==
		(stream->data_len + SHUB_MAX_DATA_CRC)) {
		data_crc = shub_data_checksum(p_packet->buff, p_packet->length);
		crc_inframe = p_packet->buff[p_packet->length];
		crc_inframe <<= 8;
		crc_inframe |= p_packet->buff[(p_packet->length + 1)];
		if (data_crc == crc_inframe) {
			shub_dispatch(&stream->cmd_data);
		} else {
			if (p_packet->subtype == SHUB_MEMDUMP_DATA_SUBTYPE ||
			    p_packet->subtype == SHUB_MEMDUMP_CODE_SUBTYPE) {
				/* gps_dispatch(&stream->cmd_data); */
			} else {
				pr_info
				("err type=%d,subtype=%d len=%d\n",
				 p_packet->type, p_packet->subtype,
				 p_packet->length);
				pr_err
				("err CRC=%d crc_inframe=%d *processed_len=%d\n",
				 data_crc, crc_inframe, *processed_len);
			}

			/* fix data_crc != crc_inframe error*/
			for (i = 0; i < stream->received_data_len; i++) {
				if (stream->cmd_data.buff[i] == 0x7E) {
					*processed_len =
						((i - (stream->received_data_len - n_copy)) >= 0) ?
						(i - (stream->received_data_len - n_copy)) : 0;
					pr_err("it has processed %d data", *processed_len);
					break;
				}
			}
		}
		shub_init_parse_packet(stream);
	}

	return 0;
}

/**
 * Function:  shub_init_parse_packet
 * Description :
 * the init SHUB parse data
 * Parameters :
 * stream : point the current parse data
 * Return :
 * TRUE   One  frame completed
 * FALSE  One  frame not completed
 *  Negetive Error
 */
void shub_init_parse_packet(struct shub_data_processor *stream)
{
	stream->state = SHUB_RECV_SEARCH_FLAG;
	stream->head_size = 0;
	stream->received_data_len = 0;
	stream->data_len = 0;
}

/**
 * Function:  shub_parse_one_packet
 * Description :
 * Parse the input data
 * Parameters :
 * stream : point the current parse data
 * data : point the buffer data
 * len  : the receive data length
 * Return :
 * TRUE     One  frame completed
 * FALSE    One  frame not completed
 * Negetive  Error
 */
int shub_parse_one_packet(struct shub_data_processor *stream,
			  u8 *data, u16 len)
{
	u8 *input = data;
	u16 remain_len = len;
	u16 processed_len = 0;

	if (!stream || !data)
		return -EINVAL;

	while (remain_len) {
		switch (stream->state) {
		case SHUB_RECV_SEARCH_FLAG:
			shub_search_flag(stream, input, remain_len,
					 &processed_len);
			break;
		case SHUB_RECV_COLLECT_HEAD:
			shub_collect_header(stream, input, remain_len,
					    &processed_len);
			break;
		case SHUB_RECV_DATA:
			shub_collect_data(stream, input, remain_len,
					  &processed_len);
			break;
		default:
			break;
		}
		remain_len -= processed_len;
		input += processed_len;
	}

	return 0;
}

void shub_fill_head(struct cmd_data *in_data, u8 *out_data)
{
	u8 *data = out_data;
	u16 crc = 0;

	*data++ = SHUB_MAGIC_NUMBER;
	*data++ = SHUB_MAGIC_NUMBER;
	*data++ = SHUB_MAGIC_NUMBER;
	*data++ = SHUB_MAGIC_NUMBER;
	*data++ = in_data->type;
	*data++ = in_data->subtype;
	*data++ = SHUB_GET_HIGH_BYTE(in_data->length);
	*data++ = SHUB_GET_LOW_BYTE(in_data->length);
	/*calc crc */
	crc = shub_checksum(out_data);
	*data++ = SHUB_GET_HIGH_BYTE(crc);
	*data++ = SHUB_GET_LOW_BYTE(crc);
}

int shub_encode_one_packet(struct cmd_data *in_data,
			   u8 *out_data,
			   u16 out_len)
{
	int len = 0;
	u8 *crc_data = NULL;
	u16 data_checksum;

	if (!in_data) {
		pr_info("NULL == in_data");
		return -EINVAL;
	}

	if (out_len <= SHUB_MAX_HEAD_LEN) {
		pr_info("  out_len == %d", out_len);
		return -EINVAL;
	}

	len = in_data->length;
	/* First fill the SHUB head context */
	shub_fill_head(in_data, out_data);
	if (len) {
		memcpy((out_data + SHUB_MAX_HEAD_LEN), in_data->buff, len);
		data_checksum = shub_data_checksum(in_data->buff, len);
		crc_data = out_data + SHUB_MAX_HEAD_LEN + len;
		*crc_data++ = SHUB_GET_HIGH_BYTE(data_checksum);
		*crc_data++ = SHUB_GET_LOW_BYTE(data_checksum);
		len += SHUB_MAX_HEAD_LEN;
		len += SHUB_MAX_DATA_CRC;
	}

	return len;
}

MODULE_DESCRIPTION("Sensorhub protocol support");
MODULE_LICENSE("GPL v2");
