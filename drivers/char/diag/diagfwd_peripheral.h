/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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

#ifndef DIAGFWD_PERIPHERAL_H
#define DIAGFWD_PERIPHERAL_H

#define PERIPHERAL_BUF_SZ		16384
#define MAX_PERIPHERAL_BUF_SZ		32768
#define MAX_PERIPHERAL_HDLC_BUF_SZ	65539

#define TRANSPORT_UNKNOWN		-1
#define TRANSPORT_SOCKET		0
#define TRANSPORT_RPMSG			1
#define NUM_TRANSPORT			2
#define NUM_WRITE_BUFFERS		2
#define PERIPHERAL_MASK(x)					\
	((x == PERIPHERAL_MODEM) ? DIAG_CON_MPSS :		\
	((x == PERIPHERAL_LPASS) ? DIAG_CON_LPASS :		\
	((x == PERIPHERAL_WCNSS) ? DIAG_CON_WCNSS :		\
	((x == PERIPHERAL_SENSORS) ? DIAG_CON_SENSORS : \
	((x == PERIPHERAL_WDSP) ? DIAG_CON_WDSP : \
	((x == PERIPHERAL_CDSP) ? DIAG_CON_CDSP : 0))))))	\

#define PERIPHERAL_STRING(x)					\
	((x == PERIPHERAL_MODEM) ? "MODEM" :			\
	((x == PERIPHERAL_LPASS) ? "LPASS" :			\
	((x == PERIPHERAL_WCNSS) ? "WCNSS" :			\
	((x == PERIPHERAL_SENSORS) ? "SENSORS" :		\
	((x == PERIPHERAL_WDSP) ? "WDSP" :			\
	((x == PERIPHERAL_CDSP) ? "CDSP" : "UNKNOWN"))))))	\

struct diagfwd_buf_t {
	unsigned char *data;
	unsigned char *data_raw;
	uint32_t len;
	uint32_t len_raw;
	atomic_t in_busy;
	int ctxt;
};

struct diag_channel_ops {
	void (*open)(struct diagfwd_info *fwd_info);
	void (*close)(struct diagfwd_info *fwd_info);
	void (*read_done)(struct diagfwd_info *fwd_info,
			  unsigned char *buf, int len);
};

struct diag_peripheral_ops {
	void (*open)(void *ctxt);
	void (*close)(void *ctxt);
	int (*write)(void *ctxt, unsigned char *buf, int len);
	int (*read)(void *ctxt, unsigned char *buf, int len);
	void (*queue_read)(void *ctxt);
};

struct diag_id_info {
	uint8_t diagid_val;
	uint8_t pd;
	char *reg_str;
};

struct diagfwd_info {
	uint8_t peripheral;
	uint8_t type;
	uint8_t transport;
	uint8_t inited;
	uint8_t ch_open;
	uint8_t num_pd;
	int cpd_len_1;
	int cpd_len_2;
	int upd_len[MAX_PERIPHERAL_UPD][2];
	atomic_t opened;
	unsigned long read_bytes;
	unsigned long write_bytes;
	struct mutex buf_mutex;
	struct mutex data_mutex;
	void *ctxt;
	struct diag_id_info root_diag_id;
	struct diag_id_info upd_diag_id[MAX_PERIPHERAL_UPD];
	struct diagfwd_buf_t *buf_1;
	struct diagfwd_buf_t *buf_2;
	struct diagfwd_buf_t *buf_upd[MAX_PERIPHERAL_UPD][2];
	struct diagfwd_buf_t *buf_ptr[NUM_WRITE_BUFFERS];
	struct diag_peripheral_ops *p_ops;
	struct diag_channel_ops *c_ops;
};

extern struct diagfwd_info peripheral_info[NUM_TYPES][NUM_PERIPHERALS];

int diagfwd_peripheral_init(void);
void diagfwd_peripheral_exit(void);

void diagfwd_close_transport(uint8_t transport, uint8_t peripheral);

void diagfwd_open(uint8_t peripheral, uint8_t type);
void diagfwd_early_open(uint8_t peripheral);

void diagfwd_late_open(struct diagfwd_info *fwd_info);
void diagfwd_close(uint8_t peripheral, uint8_t type);

int diag_md_get_peripheral(int ctxt);

int diagfwd_register(uint8_t transport, uint8_t peripheral, uint8_t type,
		     void *ctxt, struct diag_peripheral_ops *ops,
		     struct diagfwd_info **fwd_ctxt);
int diagfwd_cntl_register(uint8_t transport, uint8_t peripheral, void *ctxt,
			  struct diag_peripheral_ops *ops,
			  struct diagfwd_info **fwd_ctxt);
void diagfwd_deregister(uint8_t peripheral, uint8_t type, void *ctxt);

int diagfwd_write(uint8_t peripheral, uint8_t type, void *buf, int len);
void diagfwd_write_done(uint8_t peripheral, uint8_t type, int buf_num);
void diagfwd_buffers_init(struct diagfwd_info *fwd_info);

/*
 * The following functions are called by the channels
 */
int diagfwd_channel_open(struct diagfwd_info *fwd_info);
int diagfwd_channel_close(struct diagfwd_info *fwd_info);
void diagfwd_channel_read(struct diagfwd_info *fwd_info);
int diagfwd_channel_read_done(struct diagfwd_info *fwd_info,
			      unsigned char *buf, uint32_t len);
int diagfwd_write_buffer_done(struct diagfwd_info *fwd_info, const void *ptr);

#endif
