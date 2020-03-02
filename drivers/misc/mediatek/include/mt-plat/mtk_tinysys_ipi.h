/*
 * Copyright (C) 2019 MediaTek Inc.
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
#ifndef __MTK_TINYSYS_IPI_H__
#define __MTK_TINYSYS_IPI_H__

#include <linux/platform_device.h>
#include <mt-plat/mtk-mbox.h>


#define IPI_SEND_WAIT       0
#define IPI_SEND_POLLING    1

enum mtk_ipi_dev {
	IPI_DEV_SCP,
	IPI_DEV_SSPM,
	IPI_DEV_ADSP,
	IPI_DEV_MCUPM,
	IPI_DEV_CPUEB,
	IPI_DEV_APUSYS,
	IPI_DEV_TOTAL,
};

typedef int (*ipi_tx_cb_t)(void *);

/**
 * struct mtk_ipi_chan_table - channel table that belong to mtk_ipi_device
 * @ept: the rpmsg endpoint of this channel
 * @rpchan: info used to create the endpoint
 * @pin_send: the mbox send pin table address of this channel
 * @pin_recv: the mbox receive pin table address of this channel
 *
 * All of these data should be initialized by mtk_ipi_device_register()
 */
struct mtk_ipi_chan_table {
	struct rpmsg_endpoint *ept;
	struct mtk_rpmsg_channel_info *rpchan;
	struct mtk_mbox_pin_send *pin_send;
	struct mtk_mbox_pin_recv *pin_recv;
	// TODO: ipi_monitor[]
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
 * @ipi_inited: set when mtk_ipi_device_register() done
 *
 * The value of mrpdev, table, mutex_ipi_reg, ipi_inited would be initialized by
 * mtk_ipi_device_register(), others should be declared by tinysys platform.
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
	int ipi_inited;
};


#define IPI_ACTION_DONE		0
#define IPI_DEV_ILLEGAL		-1 /* ipi device is not initial */
#define IPI_DUPLEX			-2 /* the ipi has be registered */
#define IPI_UNAVAILABLE		-3 /* can't find this ipi pin define */
#define IPI_NO_MSGBUF		-4 /* receiver doesn't has message buffer */
#define IPI_NO_MEMORY		-5 /* message length is large than defined */
#define IPI_PIN_BUSY		-6 /* send message timeout */
#define IPI_COMPL_TIMEOUT	-7 /* polling or wait for ack ipi timeout */
#define IPI_RPMSG_ERR		-99 /* some error from rpmsg layer */


int mtk_ipi_device_register(struct mtk_ipi_device *ipidev,
		struct platform_device *pdev, struct mtk_mbox_device *mbox,
		unsigned int ipi_chan_count);
int mtk_ipi_register(struct mtk_ipi_device *ipidev, int ipi_id,
		void *cb, void *prdata, void *msg);
int mtk_ipi_send(struct mtk_ipi_device *ipidev, int ipi_id,
		int opt, void *data, int len, int retry_timeout);
int mtk_ipi_send_compl(struct mtk_ipi_device *ipidev, int ipi_id,
		int opt, void *data, int len, unsigned long timeout);
int mtk_ipi_recv(struct mtk_ipi_device *ipidev, int ipi_id);
int mtk_ipi_recv_reply(struct mtk_ipi_device *ipidev, int ipi_id,
		void *reply_data, int len);

#endif
