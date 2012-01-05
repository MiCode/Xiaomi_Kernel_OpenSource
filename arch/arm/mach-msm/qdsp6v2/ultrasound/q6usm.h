/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __Q6_USM_H__
#define __Q6_USM_H__

#include <mach/qdsp6v2/apr_us.h>

/* cyclic buffer with 1 gap support */
#define USM_MIN_BUF_CNT 3

#define FORMAT_USPS_EPOS	0x00000000
#define FORMAT_USRAW		0x00000001
#define INVALID_FORMAT		0xffffffff

#define IN			0x000
#define OUT			0x001

#define USM_WRONG_TOKEN		0xffffffff
#define USM_UNDEF_TOKEN		0xfffffffe

#define CMD_CLOSE		0x0004

/* bit 0:1 represents priority of stream */
#define STREAM_PRIORITY_NORMAL	0x0000
#define STREAM_PRIORITY_LOW	0x0001
#define STREAM_PRIORITY_HIGH	0x0002

/* bit 4 represents META enable of encoded data buffer */
#define BUFFER_META_ENABLE	0x0010

struct us_region {
	dma_addr_t	phys;
	/* If == NULL, the region isn't allocated */
	void		*data;
	/* number of buffers in the region */
	uint32_t	buf_cnt;
	/* size of buffer */
	uint32_t	buf_size;
};

struct us_port_data {
	dma_addr_t	phys;
	/* cyclic region of buffers with 1 gap */
	void		*data;
	/* number of buffers in the region */
	uint32_t	buf_cnt;
	/* size of buffer */
	uint32_t	buf_size;
	/* TX: write index */
	uint32_t	dsp_buf;
	/* TX: read index */
	uint32_t	cpu_buf;
	/* expected token from dsp */
	uint32_t	expected_token;
	/* read or write locks */
	struct mutex	lock;
	spinlock_t	dsp_lock;
};

struct us_client {
	int			session;
	/* idx:1 out port, 0: in port*/
	struct us_port_data	port[2];

	struct apr_svc		*apr;
	struct mutex		cmd_lock;

	atomic_t		cmd_state;
	atomic_t		eos_state;
	wait_queue_head_t	cmd_wait;

	void (*cb)(uint32_t, uint32_t, uint32_t *, void *);
	void			*priv;
};

int q6usm_run(struct us_client *usc, uint32_t flags,
	      uint32_t msw_ts, uint32_t lsw_ts);
int q6usm_cmd(struct us_client *usc, int cmd);
int q6usm_us_client_buf_alloc(unsigned int dir, struct us_client *usc,
			      unsigned int bufsz, unsigned int bufcnt);
int q6usm_enc_cfg_blk(struct us_client *usc, struct us_encdec_cfg *us_cfg);
int q6usm_dec_cfg_blk(struct us_client *usc, struct us_encdec_cfg *us_cfg);
int q6usm_read(struct us_client *usc, uint32_t read_ind);
struct us_client *q6usm_us_client_alloc(
	void (*cb)(uint32_t, uint32_t, uint32_t *, void *),
	void *priv);
int q6usm_open_read(struct us_client *usc, uint32_t format);
void q6usm_us_client_free(struct us_client *usc);
int q6usm_memory_map(struct us_client *usc, uint32_t buf_add,
		     int dir, uint32_t bufsz, uint32_t bufcnt);
int q6usm_memory_unmap(struct us_client *usc, uint32_t buf_add,
		       int dir);

uint32_t q6usm_get_ready_data(int dir, struct us_client *usc);
uint32_t q6usm_get_virtual_address(int dir, struct us_client *usc,
				   struct vm_area_struct *vms);

int q6usm_open_write(struct us_client *usc,  uint32_t format);
int q6usm_write(struct us_client *usc, uint32_t write_ind);
bool q6usm_is_write_buf_full(struct us_client *usc, uint32_t* free_region);

int q6usm_set_us_detection(struct us_client *usc,
			   struct usm_session_cmd_detect_info *detect_info,
			   uint16_t detect_info_size);

#endif /* __Q6_USM_H__ */
