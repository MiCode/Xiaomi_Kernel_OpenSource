/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_TINYSYS_IPI_H__
#define __MTK_TINYSYS_IPI_H__

#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-mbox.h>


#define IPI_SEND_WAIT       0
#define IPI_SEND_POLLING    1

enum mtk_ipi_dev {
	IPI_DEV_SCP,
	IPI_DEV_SSPM,
	IPI_DEV_ADSP,
	IPI_DEV_MCUPM,
	IPI_DEV_CPUEB,
	IPI_DEV_APUSYS,
	IPI_DEV_GPUEB,
	IPI_DEV_VCP,
	IPI_DEV_TOTAL,
};

enum ipi_stage {
	SEND_MSG,
	ISR_RECV_MSGV,
	RECV_MSG,
	SEND_ACK,
	ISR_RECV_ACK,
	RECV_ACK,
	UNUSED = 0xF,
};

typedef int (*ipi_tx_cb_t)(void *);

struct ipimon_s {
	unsigned short idx;
	unsigned long long ts;
};

/**
 * struct mtk_ipi_chan_table - channel table that belong to mtk_ipi_device
 * @ept: the rpmsg endpoint of this channel
 * @rpchan: info used to create the endpoint
 * @pin_send: the mbox send pin table address of this channel
 * @pin_recv: the mbox receive pin table address of this channel
 * @holder: keep 1 if there are ipi waiters (to wait the reply)
 * @ipi_stage: transmission stage for t0~t5 (default is 0xF)
 * @ipi_seqno: sequence count of the IPI pin processed
 * @ipi_record: timestamp of each ipi transmission stage
 *
 * All of these data should be initialized by mtk_ipi_device_register()
 */
struct mtk_ipi_chan_table {
	struct rpmsg_endpoint *ept;
	struct mtk_rpmsg_channel_info *rpchan;
	struct mtk_mbox_pin_send *pin_send;
	struct mtk_mbox_pin_recv *pin_recv;
	atomic_t holder;
	unsigned int ipi_stage: 4,
		 ipi_seqno : 28;
	struct ipimon_s ipi_record[3];
	int trysend_count;
	int polling_count;
};

/**
 * struct mtk_ipi_device - device for represent the tinysys using mtk ipi
 * @name: name of tinysys device
 * @id: device id (used to match between rpmsg drivers and devices)
 * @mrpdev: mtk rpmsg channel device
 * @mbdev: mtk mbox device
 * @table: channel table with endpoint & channel_info & mbox_pin info
 * @mutex_ipi_reg: the lock must be taken when user register ipi
 * @pre_cb: the callback handler before ipi send data
 * @post_cb: the callback handler after ipi send data
 * @prdata: private data for the callback use
 * @timeout_handler: the callback for waiting reply timeout
 * @lock_monitor: the lock for dump ipi timestamp
 * @ipi_last_done: the last processed ipi transmission
 * @ipi_inited: set when mtk_ipi_device_register() done
 *
 * Tinysys platform has necessary to define the vcalue of 'name', 'id', 'mbdev';
 * and optional to define the 'pre_cb', 'post_cb', 'prdata', 'timeout_handler'.
 * Othes would be initialized by mtk_ipi_device_register().
 */
struct mtk_ipi_device  {
	const char *name;
	int id;
	struct mtk_rpmsg_device *mrpdev;
	struct mtk_mbox_device *mbdev;
	struct mtk_ipi_chan_table *table;
	struct mutex mutex_ipi_reg;
	ipi_tx_cb_t pre_cb;
	ipi_tx_cb_t post_cb;
	void *prdata;
	void (*timeout_handler)(int id);
	spinlock_t lock_monitor;
	int ipi_last_done;
	int ipi_inited;
};


#define IPI_ACTION_DONE		0
#define IPI_DEV_ILLEGAL		-1 /* ipi device is not initial */
#define IPI_DUPLEX		-2 /* the ipi has be registered */
#define IPI_UNAVAILABLE		-3 /* can't find this ipi pin define */
#define IPI_NO_MSGBUF		-4 /* receiver doesn't has message buffer */
#define IPI_NO_MEMORY		-5 /* message length is large than defined */
#define IPI_PIN_BUSY		-6 /* send message timeout */
#define IPI_COMPL_TIMEOUT	-7 /* polling or wait for ack ipi timeout */
#define IPI_PRE_CB_FAIL		-8 /* pre-callback fail */
#define IPI_POST_CB_FAIL	-9 /* post-callback fail */
#define IPI_RPMSG_ERR		-99 /* some error from rpmsg layer */


int mtk_ipi_device_register(struct mtk_ipi_device *ipidev,
		struct platform_device *pdev, struct mtk_mbox_device *mbox,
		unsigned int ipi_chan_count);
int mtk_ipi_device_reset(struct mtk_ipi_device *ipidev);

int mtk_ipi_register(struct mtk_ipi_device *ipidev, int ipi_id,
		mbox_pin_cb_t cb, void *prdata, void *msg);
int mtk_ipi_unregister(struct mtk_ipi_device *ipidev, int ipi_id);

int mtk_ipi_send(struct mtk_ipi_device *ipidev, int ipi_id,
		int opt, void *data, int len, int retry_timeout);
int mtk_ipi_send_compl(struct mtk_ipi_device *ipidev, int ipi_id,
		int opt, void *data, int len, unsigned long timeout);
int mtk_ipi_recv(struct mtk_ipi_device *ipidev, int ipi_id);
int mtk_ipi_recv_reply(struct mtk_ipi_device *ipidev, int ipi_id,
		void *reply_data, int len);

void ipi_monitor_dump(struct mtk_ipi_device *ipidev);
void mtk_ipi_tracking(struct mtk_ipi_device *ipidev, bool en);

#endif
