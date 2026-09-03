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

#ifndef __WCN_SIPC_H__
#define __WCN_SIPC_H__
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
//#include <linux/swcnblk.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include "bus_common.h"
#include "mdbg_type.h"
#include "wcn_dbg.h"
#include "wcn_types.h"

#define SIPC_SBUF_HEAD_RESERV 4
#define SIPC_SBLOCK_HEAD_RESERV 0

#define mbuf_list_iter(head, num, pos, posn) \
	for (pos = head, posn = 0; posn < num && pos; posn++, pos = pos->next)

enum wcn_sipc_chn_index {
	SIPC_ATCMD_TX = 0,
	SIPC_ATCMD_RX,
	SIPC_LOOPCHECK_RX,
	SIPC_ASSERT_RX,
	SIPC_LOG_RX,
	SIPC_BT_TX =  8, /* reserve */
	SIPC_BT_RX,
	SIPC_FM_TX = 12, /* reserve */
	SIPC_FM_RX,
	SIPC_WIFI_CMD_TX = 16, /* reserve */
	SIPC_WIFI_CMD_RX,
	SIPC_WIFI_DATA0_TX,
	SIPC_WIFI_DATA0_RX,
	SIPC_WIFI_DATA1_TX,
	SIPC_WIFI_DATA1_RX,
	SIPC_CHN_NUM
};

struct wcn_sipc_info_t {
	struct device_node *np;
	u32 sipc_wcn_version;
	u32 sipc_chn_status;
	u32 sipc_channel_state[SIPC_CHN_NUM];
	struct mutex status_lock;
};

struct sbuf_info {
	u8	bufid;
	u32	len;
	u32	bufnum;
	u32	txbufsize;
	u32	rxbufsize;
};

/* swcnblk */
struct sblock_info {
	u32	txblocknum;
	u32	txblocksize;
	u32	rxblocknum;
	u32	rxblocksize;
	u32 basemem;
	u32 alignsize;
	u32 mapped_smem_base;
};

struct mbuf_t_list {
	struct mbuf_t	*mbuf_head;
	struct mbuf_t	*mbuf_tail;
	u32	mbuf_num;
};

struct sipc_channel_statictics {
	long long int mbuf_send_to_bus;
	long long int mbuf_recv_from_bus;
	long long int buf_alloc_num;
	long long int buf_free_num;
	long long int mbuf_alloc_num;
	long long int mbuf_free_num;
	long long int mbuf_giveback_to_user;
	long long int mbuf_recv_from_user;

	long long int report_num;
	long long int to_accept;
	spinlock_t lock;
};

struct sipc_chn_info {
	u8 index;
	u8 relate_index;
	u8 inout;
	u8 chntype;	/* sbuf/sblock */
	u8 chn;
	u8 dst;
	bool sipc_chn_status;
	u8 need_reserve;
	struct mchn_ops_t *ops;

	struct mbuf_t_list	push_queue;
	struct mutex pushq_lock;
	struct mbuf_t_list	pop_queue;
	struct mutex popq_lock;
	union {
		struct sbuf_info sbuf;
		struct sblock_info sblk;
	};
	struct task_struct *wcn_sipc_thread;
	struct completion	callback_complete;
	struct sipc_channel_statictics  chn_static;
};

int wcn_sipc_channel_dir(int index);
struct sipc_chn_info *wcn_sipc_channel_get(int index);
void wcn_sipc_chn_set_status_all_false(void);

#ifdef WCN_SIPC_DBG
#define WCN_HERE WCN_INFO("[%s] %d\n", __func__, __LINE__)
#define WCN_HERE_CHN(x) WCN_INFO("[%s] %d chn[%d]\n", __func__, __LINE__, x)
#else
#define WCN_HERE
#define WCN_HERE_CHN(x)
#endif
#endif
