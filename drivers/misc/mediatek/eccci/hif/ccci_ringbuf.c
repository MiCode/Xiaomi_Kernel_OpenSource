// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/delay.h>
#include <linux/module.h>
#include "ccci_ringbuf.h"
#include "ccci_core.h"
#include "ccci_debug.h"

#define TAG "rbf"
#define CCIF_HEADER_LEN 8
#define CCIF_FOOTER_LEN 8
#define CCCI_HEADER_LEN 16

#define CCCI_RBF_HEADER 0xEE0000EE
#define CCCI_RBF_FOOTER 0xFF0000FF
#define CCIF_PKG_HEADER 0xAABBAABB
#define CCIF_PKG_FOOTER 0xCCDDEEFF

static inline void *rbf_memcpy(void *__dest, __const void *__src, size_t __n)
{
	int i = 0;
	unsigned char *d = (unsigned char *)__dest, *s = (unsigned char *)__src;

	for (i = __n >> 3; i > 0; i--) {
		*d++ = *s++;
		*d++ = *s++;
		*d++ = *s++;
		*d++ = *s++;
		*d++ = *s++;
		*d++ = *s++;
		*d++ = *s++;
		*d++ = *s++;
	}

	if (__n & 1 << 2) {
		*d++ = *s++;
		*d++ = *s++;
		*d++ = *s++;
		*d++ = *s++;
	}

	if (__n & 1 << 1) {
		*d++ = *s++;
		*d++ = *s++;
	}

	if (__n & 1)
		*d++ = *s++;

	return __dest;
}

/* #define rbf_memcpy memcpy */
#define CCIF_RBF_READ(bufaddr, output_addr, read_size, read_pos, buflen)\
do {\
	if (read_pos + read_size < buflen) {\
		rbf_memcpy((unsigned char *)output_addr,\
			(unsigned char *)(bufaddr) + read_pos, read_size);\
	} else {\
		rbf_memcpy((unsigned char *)output_addr,\
		(unsigned char *)(bufaddr) + read_pos, buflen - read_pos);\
		output_addr = (unsigned char *)output_addr + buflen - read_pos;\
		rbf_memcpy((unsigned char *)output_addr, \
			(unsigned char *)(bufaddr),\
			read_size - (buflen - read_pos));\
	} \
} while (0)
#define CCIF_RBF_WRITE(bufaddr, data_addr, data_size, write_pos, buflen)\
do {\
	if (write_pos + data_size < buflen) {\
		rbf_memcpy((unsigned char *)(bufaddr) + write_pos,\
			(unsigned char *)data_addr, data_size);\
	} else {\
		rbf_memcpy((unsigned char *)(bufaddr) + write_pos,\
			(unsigned char *)data_addr,  buflen - write_pos);\
		data_addr = (unsigned char *)data_addr + buflen - write_pos;\
		rbf_memcpy((unsigned char *)(bufaddr), \
			(unsigned char *)data_addr,\
			data_size - (buflen - write_pos));\
	} \
} while (0)

static void ccci_ringbuf_dump(int md_id, unsigned char *title,
	unsigned char *buffer, unsigned int read,
	unsigned int length, int dump_size)
{
	int i, j;
	unsigned char tmp_buf[256];
	unsigned char buf[256];
	unsigned int write = read + dump_size;
	int ret = 0;

	if (write >= length)
		write -= length;
	CCCI_MEM_LOG_TAG(md_id, TAG,
		"%s rbdump: buf=0x%p, read=%d, write=%d\n",
		title, buffer, read, write);
	read = (read >> 3) << 3;
	/* 8byte align*/
	write = ((write + 7) >> 3) << 3;
	if (write >= length)
		write -= length;
	CCCI_MEM_LOG_TAG(md_id, TAG,
		"rbdump:aligned read=%d,write=%d\n", read,
		write);
	i = read;
	while (1) {
		memset(tmp_buf, 0, sizeof(tmp_buf));
		ret = snprintf(tmp_buf, sizeof(tmp_buf), "%08X:", i);
		if (ret < 0 || ret >= sizeof(tmp_buf)) {
			CCCI_ERROR_LOG(md_id, TAG,
				"%s-%d:snprintf fail,ret = %d\n", __func__, __LINE__, ret);
			return;
		}
		for (j = 0; j < 4; j++) {
			ret = snprintf(buf, sizeof(tmp_buf), "%s", tmp_buf);
			if (ret < 0 || ret >= sizeof(tmp_buf)) {
				CCCI_ERROR_LOG(md_id, TAG,
					"%s-%d:snprintf fail,ret = %d\n", __func__, __LINE__, ret);
				return;
			}
			ret = snprintf(tmp_buf, sizeof(tmp_buf),
				 "%s %02X%02X%02X%02X", buf, *(buffer + i),
				 *(buffer + i + 1), *(buffer + i + 2),
				 *(buffer + i + 3));
			if (ret < 0 || ret >= sizeof(tmp_buf)) {
				CCCI_ERROR_LOG(md_id, TAG,
					"%s-%d:snprintf fail,ret = %d\n", __func__, __LINE__, ret);
				return;
			}
			i += sizeof(unsigned int);
			if (i >= length)
				i -= length;
			if (i == write)
				goto OUT;
		}
		CCCI_MEM_LOG_TAG(md_id, TAG, "%s\n", tmp_buf);
	}
 OUT:
	CCCI_MEM_LOG_TAG(md_id, TAG, "%s\n", tmp_buf);
}

struct ccci_ringbuf *ccci_create_ringbuf(int md_id, unsigned char *buf,
	int buf_size, int rx_size, int tx_size)
{
	int buflen;
	struct ccci_ringbuf *ringbuf = NULL;

	buflen = CCCI_RINGBUF_CTL_LEN + rx_size + tx_size;
	CCCI_NORMAL_LOG(md_id, TAG,
		"crb:buf vir_addr=0x%llx, buf_size=%d,buflen=%d,rx_size=%d,tx_size=%d,ctr_len=%zu\n",
			buf, buf_size, buflen, rx_size, tx_size, CCCI_RINGBUF_CTL_LEN);
	if (buf_size < buflen)
		return NULL;
	memset_io(buf, 0x0, buflen);
	/*set ccif header */
	*((unsigned int *)buf) = CCCI_RBF_HEADER;
	*((unsigned int *)(buf + sizeof(unsigned int)))
		= CCCI_RBF_HEADER;
	CCCI_NORMAL_LOG(md_id, TAG,
		"crb:Header(0x%p)=0x%x %x\n", buf,
		*((unsigned int *)buf),
		*((unsigned int *)(buf + sizeof(unsigned int))));
	/*set ccif footer */
	*((unsigned int *)(buf + buflen - sizeof(unsigned int)))
		= CCCI_RBF_FOOTER;
	*((unsigned int *)(buf + buflen - 2 * sizeof(unsigned int)))
		= CCCI_RBF_FOOTER;
	CCCI_NORMAL_LOG(md_id, TAG,
		"crb:Footer(0x%p)=0x%x %x\n",
		buf + buflen - sizeof(int),
		*((unsigned int *)(buf + buflen - sizeof(unsigned int))),
		*((unsigned int *)(buf + buflen - 2 * sizeof(unsigned int))));
	buf += sizeof(int) * 2;
	ringbuf = (struct ccci_ringbuf *)buf;
	ringbuf->rx_control.length = rx_size;
	ringbuf->rx_control.read = 0;
	ringbuf->rx_control.write = 0;
	ringbuf->tx_control.length = tx_size;
	ringbuf->tx_control.read = 0;
	ringbuf->tx_control.write = 0;
	CCCI_NORMAL_LOG(md_id, TAG, "crb:rbf=0x%llx\n", ringbuf);
	return ringbuf;
}

int ccci_ringbuf_writeable(int md_id, struct ccci_ringbuf *ringbuf,
	unsigned int write_size)
{
	int read, write, size, length;

	if (ringbuf == NULL) {
		CCCI_ERROR_LOG(md_id, TAG,
			"rbwb param error,ringbuf == NULL\n");
		return -CCCI_RINGBUF_PARAM_ERR;
	}
	read = (unsigned int)(ringbuf->tx_control.read);
	write = (unsigned int)(ringbuf->tx_control.write);
	length = (unsigned int)(ringbuf->tx_control.length);
	if (write_size > length) {
		if (length > 0)
			CCCI_ERROR_LOG(md_id, TAG,
				"rbwb param error,writesize(%d) > length(%d)\n",
				write_size, length);
		return -CCCI_RINGBUF_PARAM_ERR;
	}
	write_size += CCIF_HEADER_LEN + CCIF_FOOTER_LEN;
	/* 8 byte align */
	write_size = (((write_size + 7) >> 3) << 3);
	if (read == write) {
		size = length - 1;
	} else if (read < write) {
		size = length - write - 1;
		size += read;
	} else {
		size = read - write - 1;
	}

	return (write_size < size) ? write_size : -(write_size - size);
}

int ccci_ringbuf_write(int md_id, struct ccci_ringbuf *ringbuf,
	unsigned char *data, int data_len)
{
	int aligned_data_len;
	unsigned int read, write, length;
	unsigned char *tx_buffer = NULL;
	unsigned char *h_ptr = NULL;

	unsigned int header[2] = { CCIF_PKG_HEADER, 0x0 };
	unsigned int footer[2] = { CCIF_PKG_FOOTER, CCIF_PKG_FOOTER };

	if (ringbuf == NULL || data_len == 0 || data == NULL)
		return -CCCI_RINGBUF_PARAM_ERR;
	if (ccci_ringbuf_writeable(md_id, ringbuf, data_len) <= 0)
		return -CCCI_RINGBUF_NOT_ENOUGH;
	read = (unsigned int)(ringbuf->tx_control.read);
	write = (unsigned int)(ringbuf->tx_control.write);
	length = (unsigned int)(ringbuf->tx_control.length);
	tx_buffer = ringbuf->buffer + ringbuf->rx_control.length;
	header[1] = data_len;
	h_ptr = (unsigned char *)header;
	CCIF_RBF_WRITE(tx_buffer, h_ptr, CCIF_HEADER_LEN, write, length);
	write += CCIF_HEADER_LEN;
	if (write >= length)
		write -= length;
	CCIF_RBF_WRITE(tx_buffer, data, data_len, write, length);
	/* 8 byte align */
	aligned_data_len = ((((unsigned int)(data_len + 7)) >> 3) << 3);
	write += aligned_data_len;
	if (write >= length)
		write -= length;
	h_ptr = (unsigned char *)footer;
	CCIF_RBF_WRITE(tx_buffer, h_ptr, CCIF_FOOTER_LEN, write, length);
	write += CCIF_FOOTER_LEN;
	if (write >= length)
		write -= length;
	CCCI_DEBUG_LOG(md_id, TAG,
	"rbw: rbf=0x%p,tx_buf=0x%p,o_write=%d,n_write=%d,datalen=%d,aligned_data_len=%d,HLEN=%d,LEN=%d,read=%d\n",
	ringbuf, tx_buffer, ringbuf->tx_control.write, write,
	data_len, aligned_data_len, 16, length, ringbuf->tx_control.read);

	/* insure sequential execution */
	mb();

	ringbuf->tx_control.write = write;

	return data_len;
}

int ccci_ringbuf_readable(int md_id, struct ccci_ringbuf *ringbuf)
{
	unsigned char *rx_buffer = NULL;
	unsigned char *outptr = NULL;
	unsigned int read, write, ccci_pkg_len, ccif_pkg_len;
	unsigned int footer_pos, length;
	unsigned int header[2] = { 0 };
	unsigned int footer[2] = { 0 };
	int size;

	if (ringbuf == NULL) {
		CCCI_ERROR_LOG(md_id, TAG, "rbrdb param error,ringbuf==NULL\n");
		return -CCCI_RINGBUF_PARAM_ERR;
	}
	read = (unsigned int)(ringbuf->rx_control.read);
	write = (unsigned int)(ringbuf->rx_control.write);
	length = (unsigned int)(ringbuf->rx_control.length);
	rx_buffer = ringbuf->buffer;
	size = write - read;
	if (size < 0)
		size += length;

	CCCI_DEBUG_LOG(md_id, TAG,
	"rbrdb:rbf=%p,rx_buf=0x%p,read=%d,write=%d,len=%d\n",
	ringbuf, rx_buffer, read, write, length);
	if (size < CCIF_HEADER_LEN + CCIF_FOOTER_LEN + CCCI_HEADER_LEN)
		return -CCCI_RINGBUF_EMPTY;
	outptr = (unsigned char *)header;
	CCIF_RBF_READ(rx_buffer, outptr, CCIF_HEADER_LEN, read, length);
	if (header[0] != CCIF_PKG_HEADER) {
		CCCI_NORMAL_LOG(md_id, TAG,
		"rbrdb:rbf=%p,rx_buf=0x%p,read=%d,write=%d,len=%d\n",
		ringbuf, rx_buffer, read, write, length);
		CCCI_ERROR_LOG(md_id, TAG,
			"rbrdb:rx_buffer=0x%p header 0x%x!=0xAABBAABB\n",
			rx_buffer, header[0]);
		ccci_ringbuf_dump(md_id, "readable",
			rx_buffer, read, length, size);
		return -CCCI_RINGBUF_BAD_HEADER;
	}
	ccci_pkg_len = header[1];

	ccif_pkg_len = CCIF_HEADER_LEN + ccci_pkg_len + CCIF_FOOTER_LEN;
	/* 8 byte align */
	ccif_pkg_len = (((ccif_pkg_len + 7) >> 3) << 3);
	if (ccif_pkg_len > size) {
		CCCI_ERROR_LOG(md_id, TAG,
			"rbrdb:header ccif_pkg_len(%d) > all data size(%d)\n",
			ccif_pkg_len, size);
		return -CCCI_RINGBUF_NOT_COMPLETE;
	}
	footer_pos = read + ccif_pkg_len - CCIF_FOOTER_LEN;
	if (footer_pos >= length)
		footer_pos -= length;
	outptr = (unsigned char *)footer;
	CCIF_RBF_READ(rx_buffer, outptr, CCIF_FOOTER_LEN, footer_pos, length);
	if (footer[0] != CCIF_PKG_FOOTER || footer[1] != CCIF_PKG_FOOTER) {
		CCCI_ERROR_LOG(md_id, TAG,
		"rbrdb:ccif_pkg_len=0x%x,footer_pos=0x%x, footer 0x%x %x!=0xCCDDEEFF CCDDEEFF\n",
		ccif_pkg_len, footer_pos, footer[0], footer[1]);
		ccci_ringbuf_dump(md_id, "readable",
			rx_buffer, read, length, ccif_pkg_len + 8);
		return -CCCI_RINGBUF_BAD_FOOTER;
	}
	return ccci_pkg_len;
}

int ccci_ringbuf_read(int md_id, struct ccci_ringbuf *ringbuf,
	unsigned char *buf, int read_size)
{
	unsigned int read, write, length;

	if (ringbuf == NULL || read_size == 0 || buf == NULL)
		return -CCCI_RINGBUF_PARAM_ERR;
	read = (unsigned int)(ringbuf->rx_control.read);
	write = (unsigned int)(ringbuf->rx_control.write);
	length = (unsigned int)(ringbuf->rx_control.length);
	/* skip header */
	read += CCIF_HEADER_LEN;
	if (read >= length)
		read -= length;
	CCIF_RBF_READ(ringbuf->buffer, buf, read_size, read, length);

	return read_size;
}

void ccci_ringbuf_move_rpointer(int md_id, struct ccci_ringbuf *ringbuf,
	int read_size)
{
	unsigned int read, length;

	if (ringbuf->rx_control.read == 0
		&& ringbuf->rx_control.write == 0) {
		CCCI_ERROR_LOG(md_id, TAG,
			"move_rpointer, rbf=%p has been reset\n", ringbuf);
		return;
	}
	read = (unsigned int)(ringbuf->rx_control.read);
	length = (unsigned int)(ringbuf->rx_control.length);
	/* Update read pointer */
	read += read_size + CCIF_HEADER_LEN + CCIF_FOOTER_LEN;
	/* 8 byte align */
	read = (((read + 7) >> 3) << 3);
	if (read >= length)
		read -= length;
	ringbuf->rx_control.read = read;
}

void ccci_ringbuf_reset(int md_id, struct ccci_ringbuf *ringbuf, int dir)
{
	if (dir == 0) {
		ringbuf->rx_control.read = 0;
		ringbuf->rx_control.write = 0;
		CCCI_DEBUG_LOG(md_id, TAG, "rbrst:rbf=%p rx\n", ringbuf);
	} else {
		ringbuf->tx_control.read = 0;
		ringbuf->tx_control.write = 0;
		CCCI_DEBUG_LOG(md_id, TAG, "rbrst:rbf=%p tx\n", ringbuf);
	}
}

