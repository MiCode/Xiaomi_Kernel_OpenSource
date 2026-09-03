/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#ifndef __WCN_TXRX_H__
#define __WCN_TXRX_H__
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/wait.h>

#include <misc/wcn_bus.h>

#define SMP_HEADERFLAG 0X7E7E7E7E
#define SMP_RESERVEDFLAG 0X5A5A
#define SMP_DSP_CHANNEL_NUM 0X88
#define SMP_DSP_TYPE 0X9D
#define SMP_DSP_DUMP_TYPE 0X32

#define SYSNC_CODE_LEN 0X4
#define CHKSUM_LEN 0X2
#define ARMLOG_HEAD 9

#define SMP_HEAD_STR "at+smphead="

struct ring_rx_data {
	unsigned char		*addr;
	unsigned int		len;
	unsigned int		fifo_id;
	struct list_head	entry;
	int channel;
	struct mbuf_t *head;
	struct mbuf_t *tail;
	int num;
};

struct ring_device {
	struct mdbg_ring_t	*ring;
	struct wakeup_source	*rw_wake_lock;
	spinlock_t		rw_lock;
	struct mutex mdbg_read_mutex;
	struct list_head	rx_head;
	struct work_struct	rx_task;
	long int flag_smp;
};

struct sme_head_tag {
	unsigned int seq_num;
	unsigned short len;
	unsigned char type;
	unsigned char subtype;
};

struct smp_head {
	unsigned int sync_code;
	unsigned short length;
	unsigned char channel_num;
	unsigned char packet_type;
	unsigned short reserved;
	unsigned short check_sum;
};

enum smp_diag_subtype_t {
	NORMAL_INFO = 0X0,
	DUMP_MEM_DATA,
	DUMP_MEM_END,
};

#define WCNBUS_TX 1
#define WCNBUS_RX 0

enum wcn_sipcbus_channel_t {
	WCN_SIPC_AT_TX = 0,
	WCN_SIPC_LOOPCHECK_RX = 2,
	WCN_SIPC_AT_RX = 1,
	WCN_SIPC_ASSERT_RX = 3,
	WCN_SIPC_RING_RX = 4,
	WCN_SIPC_RSV_RX,
};

enum wcnbus_channel_t {
	WCN_AT_TX = 0,
	WCN_LOOPCHECK_RX = 12,
	WCN_AT_RX,
	WCN_ASSERT_RX,
	WCN_RING_RX,
	/* TODO: The port num is temp,need to consult with cp */
	WCN_ADSP_RING_RX = 19,
	WCN_RSV_RX,
};

enum mdbg_channel_ops_t {
	MDBG_AT_TX_OPS = 0,
	MDBG_AT_RX_OPS,
	MDBG_LOOPCHECK_RX_OPS,
	MDBG_ASSERT_RX_OPS,
	MDBG_RING_RX_OPS,
	MDBG_ADSP_RING_RX_OPS,
	MDBG_RSV_OPS,
};

enum {
	MDBG_SUBTYPE_RING = 0,
	MDBG_SUBTYPE_LOOPCHECK,
	MDBG_SUBTYPE_AT,
	MDBG_SUBTYPE_ASSERT,
};

struct mchn_ops_t *get_mdbg_proc_op(void);

void mdbg_pt_ring_reg(void);
void mdbg_pt_ring_unreg(void);
int mdbg_ring_init(void);
void mdbg_ring_remove(void);
long int mdbg_send(char *buf, size_t len, unsigned int subtype);
long int mdbg_receive(void *buf, int len);
int mdbg_tx_cb(int channel, struct mbuf_t *head,
	       struct mbuf_t *tail, int num);
long mdbg_content_len(void);
int mdbg_read_release(unsigned int fifo_id);
bool mdbg_rx_count_change(void);

#endif
