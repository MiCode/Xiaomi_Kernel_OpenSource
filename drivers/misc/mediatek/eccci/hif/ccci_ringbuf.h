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

#ifndef __CCCI_RINGBUF_H__
#define __CCCI_RINGBUF_H__
typedef enum {
	CCCI_RINGBUF_OK = 0,
	CCCI_RINGBUF_PARAM_ERR,
	CCCI_RINGBUF_NOT_ENOUGH,
	CCCI_RINGBUF_BAD_HEADER,
	CCCI_RINGBUF_BAD_FOOTER,
	CCCI_RINGBUF_NOT_COMPLETE,
	CCCI_RINGBUF_EMPTY,
} ccci_ringbuf_error;

struct ccci_ringbuf {
	struct {
		unsigned int read;
		unsigned int write;
		unsigned int length;
	} rx_control, tx_control;
	unsigned char buffer[0];
};
#define CCCI_RINGBUF_CTL_LEN (8+sizeof(struct ccci_ringbuf)+8)

int ccci_ringbuf_readable(int md_id, struct ccci_ringbuf *ringbuf);
int ccci_ringbuf_writeable(int md_id, struct ccci_ringbuf *ringbuf,
	unsigned int write_size);
struct ccci_ringbuf *ccci_create_ringbuf(int md_id, unsigned char *buf,
	int buf_size, int rx_size, int tx_size);
int ccci_ringbuf_read(int md_id, struct ccci_ringbuf *ringbuf,
	unsigned char *buf, int read_size);
int ccci_ringbuf_write(int md_id, struct ccci_ringbuf *ringbuf,
	unsigned char *data, int data_len);
void ccci_ringbuf_move_rpointer(int md_id, struct ccci_ringbuf *ringbuf,
	int read_size);
void ccci_ringbuf_reset(int md_id, struct ccci_ringbuf *ringbuf, int dir);
#endif				/* __CCCI_RINGBUF_H__ */
