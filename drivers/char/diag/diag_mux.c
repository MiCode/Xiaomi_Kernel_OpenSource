// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2014-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/diagchar.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/ratelimit.h>
#include <linux/kmemleak.h>
#include "diagchar.h"
#include "diagfwd.h"
#include "diag_mux.h"
#include "diag_usb.h"
#include "diag_memorydevice.h"
#include "diagfwd_peripheral.h"
#include "diag_ipc_logging.h"

struct diag_mux_state_t *diag_mux;
static struct diag_logger_t usb_logger;
static struct diag_logger_t md_logger;

static struct diag_logger_ops usb_log_ops = {
	.open = diag_usb_connect_all,
	.close = diag_usb_disconnect_all,
	.queue_read = diag_usb_queue_read,
	.open_device = diag_usb_connect_device,
	.close_device = diag_usb_disconnect_device,
	.clear_tbl_entries = NULL,
	.write = diag_usb_write,
	.close_peripheral = NULL
};

static struct diag_logger_ops md_log_ops = {
	.open = diag_md_open_all,
	.close = diag_md_close_all,
	.open_device = diag_md_open_device,
	.close_device = diag_md_close_device,
	.clear_tbl_entries = diag_md_clear_tbl_entries,
	.queue_read = NULL,
	.write = diag_md_write,
	.close_peripheral = diag_md_close_peripheral,
};

int diag_mux_init(void)
{
	int proc;
	diag_mux = kzalloc(sizeof(struct diag_mux_state_t),
			 GFP_KERNEL);
	if (!diag_mux)
		return -ENOMEM;
	kmemleak_not_leak(diag_mux);

	usb_logger.mode = DIAG_USB_MODE;
	usb_logger.log_ops = &usb_log_ops;

	md_logger.mode = DIAG_MEMORY_DEVICE_MODE;
	md_logger.log_ops = &md_log_ops;
	diag_md_init();

	/*
	 * Set USB logging as the default logger. This is the mode
	 * Diag should be in when it initializes.
	 */
	diag_mux->usb_ptr = &usb_logger;
	diag_mux->md_ptr = &md_logger;
	for (proc = 0; proc < NUM_MUX_PROC; proc++) {
		diag_mux->logger[proc] = &usb_logger;
		diag_mux->mux_mask[proc] = 0;
		diag_mux->mode[proc] = DIAG_USB_MODE;
	}
	return 0;
}

void diag_mux_exit(void)
{
	kfree(diag_mux);
}

int diag_mux_register(int proc, int ctx, struct diag_mux_ops *ops)
{
	int err = 0;

	if (!ops)
		return -EINVAL;

	if (proc < 0 || proc >= NUM_MUX_PROC)
		return 0;

	/* Register with USB logger */
	usb_logger.ops[proc] = ops;
	err = diag_usb_register(proc, ctx, ops);
	if (err) {
		pr_err("diag: MUX: unable to register usb operations for proc: %d, err: %d\n",
		       proc, err);
		return err;
	}

	md_logger.ops[proc] = ops;
	err = diag_md_register(proc, ctx, ops);
	if (err) {
		pr_err("diag: MUX: unable to register md operations for proc: %d, err: %d\n",
		       proc, err);
		return err;
	}

	return 0;
}

int diag_mux_queue_read(int proc)
{
	struct diag_logger_t *logger = NULL;

	if (proc < 0 || proc >= NUM_MUX_PROC)
		return -EINVAL;
	if (!diag_mux)
		return -EIO;

	if (diag_mux->mode[proc] == DIAG_MULTI_MODE)
		logger = diag_mux->usb_ptr;
	else
		logger = diag_mux->logger[proc];

	if (logger && logger->log_ops && logger->log_ops->queue_read)
		return logger->log_ops->queue_read(proc);

	return 0;
}

int diag_mux_write(int proc, unsigned char *buf, int len, int ctx)
{
	struct diag_logger_t *logger = NULL;
	int peripheral = -EINVAL, type = -EINVAL, log_sink;
	unsigned char *offset = NULL;

	if (proc < 0 || proc >= NUM_MUX_PROC || !buf)
		return -EINVAL;
	if (!diag_mux)
		return -EIO;
	if (proc == DIAG_LOCAL_PROC) {
		peripheral = diag_md_get_peripheral(ctx);
		if (peripheral < 0) {
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
				"diag:%s:%d invalid peripheral = %d\n",
				__func__, __LINE__, peripheral);
			return -EINVAL;
		}

	} else {
		peripheral = 0;
	}

	if (MD_PERIPHERAL_MASK(peripheral) & diag_mux->mux_mask[proc]) {
		logger = diag_mux->md_ptr;
		log_sink = DIAG_MEMORY_DEVICE_MODE;
	} else {
		logger = diag_mux->usb_ptr;
		log_sink = DIAG_USB_MODE;
	}

	if (!proc) {
		type = GET_BUF_TYPE(ctx);
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"diag: Packet from PD: %d, type: %d, len: %d to be written to %s\n",
			peripheral, type, len,
			(log_sink ? "MD_device" : "USB"));

		if (type == TYPE_CMD) {
			if (driver->p_hdlc_disabled[peripheral])
				offset = buf + 4;
			else
				offset = buf;

			DIAG_LOG(DIAG_DEBUG_CMD_INFO,
				"diag: cmd rsp (%02x %02x %02x %02x) from PD: %d to be written to %s\n",
				*(offset), *(offset+1), *(offset+2),
				*(offset+3), peripheral,
				(log_sink ? "MD_device" : "USB"));
		}
	}
	if (logger && logger->log_ops && logger->log_ops->write)
		return logger->log_ops->write(proc, buf, len, ctx);
	return 0;
}

int diag_mux_close_peripheral(int proc, uint8_t peripheral)
{
	struct diag_logger_t *logger = NULL;

	if (proc < 0 || proc >= NUM_MUX_PROC)
		return -EINVAL;
	/* Peripheral should account for Apps data as well */
	if (peripheral > NUM_PERIPHERALS) {
		if (!driver->num_pd_session)
			return -EINVAL;
		if (peripheral > NUM_MD_SESSIONS)
			return -EINVAL;
	}

	if (!diag_mux)
		return -EIO;

	if (MD_PERIPHERAL_MASK(peripheral) & diag_mux->mux_mask[proc])
		logger = diag_mux->md_ptr;
	else
		logger = diag_mux->logger[proc];

	if (logger && logger->log_ops && logger->log_ops->close_peripheral)
		return logger->log_ops->close_peripheral(proc, peripheral);
	return 0;
}

void diag_mux_close_device(int proc)
{
	struct diag_logger_t *logger = NULL;

	if (!diag_mux)
		return;

	logger = diag_mux->logger[proc];

	if (logger && logger->log_ops && logger->log_ops->clear_tbl_entries)
		logger->log_ops->clear_tbl_entries(proc);

}

int diag_mux_switch_logging(int proc, int *req_mode, int *peripheral_mask)
{
	unsigned int new_mask = 0;

	if (!req_mode)
		return -EINVAL;

	if (*peripheral_mask <= 0 ||
		(*peripheral_mask > (DIAG_CON_ALL | DIAG_CON_UPD_ALL))) {
		pr_err("diag: mask %d in %s\n", *peripheral_mask, __func__);
		return -EINVAL;
	}

	switch (*req_mode) {
	case DIAG_USB_MODE:
		new_mask = ~(*peripheral_mask) & diag_mux->mux_mask[proc];
		if (new_mask != DIAG_CON_NONE)
			*req_mode = DIAG_MULTI_MODE;
		if (new_mask == DIAG_CON_ALL)
			*req_mode = DIAG_MEMORY_DEVICE_MODE;
		break;
	case DIAG_MEMORY_DEVICE_MODE:
		new_mask = (*peripheral_mask) | diag_mux->mux_mask[proc];
		if (new_mask != DIAG_CON_ALL)
			*req_mode = DIAG_MULTI_MODE;
		break;
	default:
		pr_err("diag: Invalid mode %d in %s\n", *req_mode, __func__);
		return -EINVAL;
	}

	switch (diag_mux->mode[proc]) {
	case DIAG_USB_MODE:
		if (*req_mode == DIAG_MEMORY_DEVICE_MODE) {
			diag_mux->usb_ptr->log_ops->close_device(proc);
			diag_mux->logger[proc] = diag_mux->md_ptr;
			diag_mux->md_ptr->log_ops->open_device(proc);
		} else if (*req_mode == DIAG_MULTI_MODE) {
			diag_mux->md_ptr->log_ops->open_device(proc);
			diag_mux->logger[proc] = NULL;
		}
		break;
	case DIAG_MEMORY_DEVICE_MODE:
		if (*req_mode == DIAG_USB_MODE) {
			diag_mux->md_ptr->log_ops->close_device(proc);
			diag_mux->logger[proc] = diag_mux->usb_ptr;
			diag_mux->usb_ptr->log_ops->open_device(proc);
		} else if (*req_mode == DIAG_MULTI_MODE) {
			diag_mux->usb_ptr->log_ops->open_device(proc);
			diag_mux->logger[proc] = NULL;
		}
		break;
	case DIAG_MULTI_MODE:
		if (*req_mode == DIAG_USB_MODE) {
			diag_mux->md_ptr->log_ops->close_device(proc);
			diag_mux->logger[proc] = diag_mux->usb_ptr;
		} else if (*req_mode == DIAG_MEMORY_DEVICE_MODE) {
			diag_mux->usb_ptr->log_ops->close_device(proc);
			diag_mux->logger[proc] = diag_mux->md_ptr;
		}
		break;
	}
	diag_mux->mode[proc] = *req_mode;
	diag_mux->mux_mask[proc] = new_mask;
	*peripheral_mask = new_mask;
	return 0;
}
