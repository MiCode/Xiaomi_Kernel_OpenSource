/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __CCCI_RINGBUF_H__
#define __CCCI_RINGBUF_H__
enum ccci_ringbuf_error {
	CCCI_RINGBUF_OK = 0,
	CCCI_RINGBUF_PARAM_ERR,
	CCCI_RINGBUF_NOT_ENOUGH,
	CCCI_RINGBUF_BAD_HEADER,
	CCCI_RINGBUF_BAD_FOOTER,
	CCCI_RINGBUF_NOT_COMPLETE,
	CCCI_RINGBUF_EMPTY,
};

struct ccci_ringbuf {
	struct {
		unsigned int read;
		unsigned int write;
		unsigned int length;
	} rx_control, tx_control;
	unsigned char buffer[0];
};
#define CCCI_RINGBUF_CTL_LEN (8+sizeof(struct ccci_ringbuf)+8)

int ccci_ringbuf_readable(struct ccci_ringbuf *ringbuf);
int ccci_ringbuf_writeable(struct ccci_ringbuf *ringbuf,
	unsigned int write_size);
struct ccci_ringbuf *ccci_create_ringbuf(unsigned char *buf,
	int buf_size, int rx_size, int tx_size);
int ccci_ringbuf_read(struct ccci_ringbuf *ringbuf,
	unsigned char *buf, int read_size);
int ccci_ringbuf_write(struct ccci_ringbuf *ringbuf,
	unsigned char *data, int data_len);
void ccci_ringbuf_move_rpointer(struct ccci_ringbuf *ringbuf,
	int read_size);
void ccci_ringbuf_reset(struct ccci_ringbuf *ringbuf, int dir);
#endif				/* __CCCI_RINGBUF_H__ */
