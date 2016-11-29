/*
 * Initialization protocol for HECI driver
 *
 * Copyright (c) 2003-2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/export.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include "heci_dev.h"
#include "hbm.h"
#include "client.h"
#include "utils.h"
#include "platform-config.h"

const char *heci_dev_state_str(int state)
{
	switch (state) {
	case HECI_DEV_INITIALIZING:
		return	"INITIALIZING";
	case HECI_DEV_INIT_CLIENTS:
		return	"INIT_CLIENTS";
	case HECI_DEV_ENABLED:
		return	"ENABLED";
	case HECI_DEV_RESETTING:
		return	"RESETTING";
	case HECI_DEV_DISABLED:
		return	"DISABLED";
	case HECI_DEV_POWER_DOWN:
		return	"POWER_DOWN";
	case HECI_DEV_POWER_UP:
		return	"POWER_UP";
	default:
		return "unkown";
	}
}
EXPORT_SYMBOL(heci_dev_state_str);

void heci_device_init(struct heci_device *dev)
{
	/* setup our list array */
	INIT_LIST_HEAD(&dev->cl_list);
	INIT_LIST_HEAD(&dev->device_list);
	init_waitqueue_head(&dev->wait_hw_ready);
	init_waitqueue_head(&dev->wait_hbm_recvd_msg);
	init_waitqueue_head(&dev->wait_dma_ready);
	dev->dev_state = HECI_DEV_INITIALIZING;

	/*
	 * We need to reserve something, because client #0
	 * is reserved for HECI bus messages
	 */
	bitmap_zero(dev->host_clients_map, HECI_CLIENTS_MAX);
	dev->open_handle_count = 0;

	/*
	 * Reserving the first three client IDs
	 * 0: Reserved for HECI Bus Message communications
	 * 1: Reserved for Watchdog
	 * 2: Reserved for AMTHI
	 */
	bitmap_set(dev->host_clients_map, 0, 3);
	/*****************************/

	heci_io_list_init(&dev->read_list);

	/* Init IPC processing and free lists */
	INIT_LIST_HEAD(&dev->wr_processing_list_head.link);
	INIT_LIST_HEAD(&dev->wr_free_list_head.link);
	do {
		int	i;

		for (i = 0; i < IPC_TX_FIFO_SIZE; ++i) {
			struct wr_msg_ctl_info	*tx_buf;

			tx_buf = kmalloc(sizeof(struct wr_msg_ctl_info),
				GFP_KERNEL);
			if (!tx_buf) {
				/*
				 * ERROR: decide what to do with it.
				 * IPC buffers may be limited or not available
				 * at all - although this shouldn't happen
				 */
				dev_err(&dev->pdev->dev, "[heci-ish]: failure in Tx FIFO allocations (%d)\n",
					i);
				break;
			}
			memset(tx_buf, 0, sizeof(struct wr_msg_ctl_info));
			list_add_tail(&tx_buf->link,
				&dev->wr_free_list_head.link);
		}
		printk(KERN_ALERT "[heci-ish]: success Tx FIFO allocations\n");
	} while (0);
}
EXPORT_SYMBOL_GPL(heci_device_init);

/**
 * heci_start - initializes host and fw to start work.
 *
 * @dev: the device structure
 *
 * returns 0 on success, <0 on failure.
 */
int heci_start(struct heci_device *dev)
{
	heci_hw_config(dev);
#ifdef FORCE_FW_INIT_RESET
	/* wait for FW-initiated reset flow, indefinitely */
	heci_hw_start(dev);
	/* 16/6/2014: changed this 2->5 seconds following MCG assertion.
	 * Once this was 10 seconds, lowered to 2.
	 * TODO: check out all FW ISS/SEC path how much it should be */

	/*timed_wait_for_timeout(WAIT_FOR_CONNECT_SLICE, dev->recvd_hw_ready,
		(10*HZ));*/
	if (!dev->recvd_hw_ready)
		wait_event_timeout(dev->wait_hw_ready, dev->recvd_hw_ready,
			10*HZ);
	/*
	 * Lock only after FW-reset flow worked or failed.
	 * otherwise interrupts BH will be locked
	 */
	if (dev->recvd_hw_ready)
		goto	reset_done;
	dev_err(&dev->pdev->dev, "[heci-ish] %s(): Timed out waiting for FW-initiated reset\n",
		__func__);
#if 1
	goto	err;	/* DEBUGDEBUGDEBUG: raise timeout for FW-initiated reset
			 * to 10 s and don't sent host-initiated reset flow */
#endif
	/* DEBUGDEBUGDEBUG: Below code until 'reset_done:' is defunct */
#else
#endif
	/* acknowledge interrupt and stop interupts */
	dev_dbg(&dev->pdev->dev, "reset in start the heci device.\n");
	heci_reset(dev, 1);

reset_done:
	if (heci_hbm_start_wait(dev)) {
		dev_err(&dev->pdev->dev, "HBM haven't started");
		goto err;
	}

	if (!heci_host_is_ready(dev)) {
		dev_err(&dev->pdev->dev, "host is not ready.\n");
		goto err;
	}

	if (!heci_hw_is_ready(dev)) {
		dev_err(&dev->pdev->dev, "ME is not ready.\n");
		goto err;
	}

	/*if (dev->version.major_version != HBM_MAJOR_VERSION ||
	    dev->version.minor_version != HBM_MINOR_VERSION) {
		dev_dbg(&dev->pdev->dev, "HECI start failed.\n");
		goto err;
	}*/

	dev_dbg(&dev->pdev->dev, "link layer has been established.\n");

	/*suspend & resume notification - send QUERY_SUBSCRIBERS msg*/
	query_subscribers(dev);

	return 0;
err:
	dev_err(&dev->pdev->dev, "link layer initialization failed.\n");
	dev->dev_state = HECI_DEV_DISABLED;
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(heci_start);

/**
 * heci_reset - resets host and fw.
 *
 * @dev: the device structure
 * @interrupts_enabled: if interrupt should be enabled after reset.
 */
void heci_reset(struct heci_device *dev, int interrupts_enabled)
{
	bool unexpected;
	int ret;

	unexpected = (dev->dev_state != HECI_DEV_INITIALIZING &&
			dev->dev_state != HECI_DEV_DISABLED &&
			dev->dev_state != HECI_DEV_POWER_DOWN &&
			dev->dev_state != HECI_DEV_POWER_UP);

	ret = heci_hw_reset(dev);
	if (ret) {
		dev_err(&dev->pdev->dev, "hw reset failed disabling the device\n");
		interrupts_enabled = false;
		dev->dev_state = HECI_DEV_DISABLED;
	}

	dev->hbm_state = HECI_HBM_IDLE;

	if (dev->dev_state != HECI_DEV_INITIALIZING) {
		if (dev->dev_state != HECI_DEV_DISABLED &&
		    dev->dev_state != HECI_DEV_POWER_DOWN)
			dev->dev_state = HECI_DEV_RESETTING;

		heci_cl_all_disconnect(dev);
	}

	if (unexpected)
		dev_warn(&dev->pdev->dev, "unexpected reset: dev_state = %s\n",
			 heci_dev_state_str(dev->dev_state));

	if (!interrupts_enabled) {
		dev_dbg(&dev->pdev->dev, "intr not enabled end of reset\n");
		return;
	}
	dev_dbg(&dev->pdev->dev, "before sending HOST start\n");
	ret = heci_hw_start(dev);
	if (ret) {
		dev_err(&dev->pdev->dev, "hw_start failed disabling the device\n");
		dev->dev_state = HECI_DEV_DISABLED;
		return;
	}

	dev_dbg(&dev->pdev->dev, "link is established start sending messages.\n");
	/* link is established * start sending messages.  */

	dev->dev_state = HECI_DEV_INIT_CLIENTS;
	dev->hbm_state = HECI_HBM_START;
	heci_hbm_start_req(dev);
	ISH_DBG_PRINT(KERN_ALERT "%s(): after heci_hbm_start_req()\n",
		__func__);
	/* wake up all readings so they can be interrupted */
	heci_cl_all_read_wakeup(dev);
}
EXPORT_SYMBOL_GPL(heci_reset);

void heci_stop(struct heci_device *dev)
{
	dev_dbg(&dev->pdev->dev, "stopping the device.\n");
	dev->dev_state = HECI_DEV_POWER_DOWN;
	heci_reset(dev, 0);
	flush_scheduled_work();
}
EXPORT_SYMBOL_GPL(heci_stop);

