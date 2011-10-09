/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <asm/mach-types.h>
#include <sound/apr_audio.h>
#include <mach/qdsp6v2/usf.h>
#include "q6usm.h"

/* The driver version*/
#define DRV_VERSION "1.0"

/* Standard timeout in the asynchronous ops */
#define USF_TIMEOUT_JIFFIES (3*HZ) /* 3 sec */

/* Undefined USF device */
#define USF_UNDEF_DEV_ID 0xffff

/* RX memory mapping flag */
#define	USF_VM_WRITE	2

/* The driver states */
enum usf_state_type {
	USF_IDLE_STATE,
	USF_OPENED_STATE,
	USF_CONFIGURED_STATE,
	USF_WORK_STATE,
	USF_ERROR_STATE
};

struct usf_xx_type {
	/* Name of the client - event calculator */
	char client_name[USF_MAX_CLIENT_NAME_SIZE];
	/* The driver state in TX or RX direction */
	enum usf_state_type usf_state;
	/* wait for q6 events mechanism */
	wait_queue_head_t wait;
	/* IF with q6usm info */
	struct us_client *usc;
	/* Q6:USM' Encoder/decoder configuration */
	struct us_encdec_cfg encdec_cfg;
	/* Shared buffer (with Q6:USM) size */
	uint32_t buffer_size;
	/* Number of the shared buffers (with Q6:USM) */
	uint32_t buffer_count;
	/* Shared memory (Cyclic buffer with 1 gap) control */
	uint32_t new_region;
	uint32_t prev_region;
	/* Q6:USM's events handler */
	void (*cb)(uint32_t, uint32_t, uint32_t *, void *);
	/* User's update info isn't acceptable */
	u8 user_upd_info_na;
};

struct usf_type {
	/* TX device component configuration & control */
	struct usf_xx_type usf_tx;
	/* RX device component configuration & control */
	struct usf_xx_type usf_rx;
	/* Index into the opened device container */
	/* To prevent mutual usage of the same device */
	uint16_t dev_ind;
	/* Event types, supported by device */
	uint16_t event_types;
	/*  The device is "input" module registered client */
	struct input_dev *input_if;
};

/* The MAX number of the supported devices */
#define MAX_DEVS_NUMBER	1

/* The opened devices container */
static int s_opened_devs[MAX_DEVS_NUMBER];

static void usf_rx_cb(uint32_t opcode, uint32_t token,
		      uint32_t *payload, void *priv)
{
	struct usf_xx_type *usf_xx = (struct usf_xx_type *) priv;

	if (usf_xx == NULL) {
		pr_err("%s: the private data is NULL\n", __func__);
		return;
	}

	switch (opcode) {
	case USM_DATA_EVENT_WRITE_DONE:
		wake_up(&usf_xx->wait);
		break;
	default:
		break;
	}
}

static void usf_tx_cb(uint32_t opcode, uint32_t token,
		      uint32_t *payload, void *priv)
{
	struct usf_xx_type *usf_xx = (struct usf_xx_type *) priv;

	if (usf_xx == NULL) {
		pr_err("%s: the private data is NULL\n", __func__);
		return;
	}

	switch (opcode) {
	case USM_DATA_EVENT_READ_DONE:
		if (token == USM_WRONG_TOKEN)
			usf_xx->usf_state = USF_ERROR_STATE;
		usf_xx->new_region = token;
		wake_up(&usf_xx->wait);
		break;

	case APR_BASIC_RSP_RESULT:
		if (payload[1]) {
			usf_xx->usf_state = USF_ERROR_STATE;
			usf_xx->new_region = USM_WRONG_TOKEN;
			wake_up(&usf_xx->wait);
		}
		break;

	default:
		break;
	}
}

static void release_xx(struct usf_xx_type *usf_xx)
{
	if (usf_xx != NULL) {
		if (usf_xx->usc) {
			q6usm_us_client_free(usf_xx->usc);
			usf_xx->usc = NULL;
		}

		if (usf_xx->encdec_cfg.params != NULL) {
			kfree(usf_xx->encdec_cfg.params);
			usf_xx->encdec_cfg.params = NULL;
		}
	}
}

static void usf_disable(struct usf_xx_type *usf_xx)
{
	if (usf_xx != NULL) {
		if ((usf_xx->usf_state != USF_IDLE_STATE) &&
		    (usf_xx->usf_state != USF_OPENED_STATE)) {
			(void)q6usm_cmd(usf_xx->usc, CMD_CLOSE);
			usf_xx->usf_state = USF_OPENED_STATE;
			wake_up(&usf_xx->wait);
		}
		release_xx(usf_xx);
	}
}

static int config_xx(struct usf_xx_type *usf_xx, struct us_xx_info_type *config)
{
	int rc = 0;
	uint16_t data_map_size = 0;

	if ((usf_xx == NULL) ||
	    (config == NULL))
		return -EINVAL;

	data_map_size = sizeof(usf_xx->encdec_cfg.cfg_common.data_map);

	if (config->client_name != NULL) {
		if (strncpy_from_user(usf_xx->client_name,
				      config->client_name,
				      sizeof(usf_xx->client_name) - 1) < 0) {
			pr_err("%s: get client name failed\n", __func__);
			return -EINVAL;
		}
	}

	pr_info("%s: name=%s; buf_size:%d; dev_id:0x%x; sample_rate:%d\n",
		__func__, usf_xx->client_name, config->buf_size,
		config->dev_id, config->sample_rate);

	pr_info("%s: buf_num:%d; format:%d; port_cnt:%d; data_size=%d\n",
		__func__, config->buf_num, config->stream_format,
		config->port_cnt, config->params_data_size);

	pr_debug("%s: p_id[0]=%d, p_id[1]=%d, p_id[2]=%d, p_id[3]=%d\n",
		__func__,
		config->port_id[0],
		config->port_id[1],
		config->port_id[2],
		config->port_id[3]);

	if (data_map_size < config->port_cnt) {
		pr_err("%s: number of supported ports:%d < requested:%d\n",
			__func__,
			data_map_size,
			config->port_cnt);
		return -EINVAL;
	}

	/* q6usm allocation & configuration */
	usf_xx->buffer_size = config->buf_size;
	usf_xx->buffer_count = config->buf_num;
	usf_xx->encdec_cfg.cfg_common.bits_per_sample =
				config->bits_per_sample;
	usf_xx->encdec_cfg.cfg_common.sample_rate = config->sample_rate;
	/* AFE port e.g. AFE_PORT_ID_SLIMBUS_MULTI_CHAN_1_RX */
	usf_xx->encdec_cfg.cfg_common.dev_id = config->dev_id;

	usf_xx->encdec_cfg.cfg_common.ch_cfg = config->port_cnt;
	memcpy((void *)&usf_xx->encdec_cfg.cfg_common.data_map,
	       (void *)config->port_id,
	       config->port_cnt);
	if (rc) {
		pr_err("%s: ports offsets copy failure\n", __func__);
		return -EINVAL;
	}

	usf_xx->encdec_cfg.format_id = config->stream_format;
	usf_xx->encdec_cfg.params_size = config->params_data_size;
	usf_xx->user_upd_info_na = 1; /* it's used in US_GET_TX_UPDATE */

	if (config->params_data_size > 0) { /* transparent data copy */
		usf_xx->encdec_cfg.params = kzalloc(config->params_data_size,
						    GFP_KERNEL);
		if (usf_xx->encdec_cfg.params == NULL) {
			pr_err("%s: params memory alloc[%d] failure\n",
				__func__,
				config->params_data_size);
			return -ENOMEM;
		}
		rc = copy_from_user(usf_xx->encdec_cfg.params,
				    config->params_data,
				    config->params_data_size);
		if (rc) {
			pr_err("%s: transparent data copy failure\n",
			       __func__);
			kfree(usf_xx->encdec_cfg.params);
			usf_xx->encdec_cfg.params = NULL;
			return -EINVAL;
		}
		pr_debug("%s: params_size[%d]; params[%d,%d,%d,%d, %d]\n",
			 __func__,
			 config->params_data_size,
			 usf_xx->encdec_cfg.params[0],
			 usf_xx->encdec_cfg.params[1],
			 usf_xx->encdec_cfg.params[2],
			 usf_xx->encdec_cfg.params[3],
			 usf_xx->encdec_cfg.params[4]
			);
	}

	usf_xx->usc = q6usm_us_client_alloc(usf_xx->cb, (void *)usf_xx);
	if (!usf_xx->usc) {
		pr_err("%s: Could not allocate q6usm client\n", __func__);
		rc = -EFAULT;
	}

	return rc;
}

static int register_input_device(struct usf_type *usf_info,
				 struct us_input_info_type *input_info)
{
	int rc = 0;
	struct input_dev *input_dev = NULL;

	if ((usf_info == NULL) ||
	    (input_info == NULL) ||
	    !(input_info->event_types & USF_ALL_EVENTS)) {
		pr_err("%s: wrong input parameter(s)\n", __func__);
		return -EINVAL;
	}

	if (usf_info->input_if != NULL) {
		pr_err("%s: input_if is already allocated\n", __func__);
		return -EFAULT;
	}

	input_dev = input_allocate_device();
	if (input_dev == NULL) {
		pr_err("%s: input_allocate_device() failed\n", __func__);
		return -ENOMEM;
	}

	/* Common part configuration */
	input_dev->name = (const char *)(usf_info->usf_tx.client_name);
	input_dev->phys = NULL;
	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor  = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0001;

	if (input_info->event_types & USF_TSC_EVENT) {
		/* TSC part configuration */
		input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
		input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
		input_set_abs_params(input_dev, ABS_X,
				     input_info->tsc_x_dim[0],
				     input_info->tsc_x_dim[1],
				     input_info->tsc_x_fuzz, 0);
		input_set_abs_params(input_dev, ABS_Y,
				     input_info->tsc_y_dim[0],
				     input_info->tsc_y_dim[1],
				     input_info->tsc_y_fuzz, 0);

		input_set_abs_params(input_dev, ABS_PRESSURE,
				     input_info->tsc_pressure[0],
				     input_info->tsc_pressure[1], 0, 0);

	}

	if (input_info->event_types & USF_MOUSE_EVENT) {
		/* Mouse part configuration */
		input_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);

		input_dev->keybit[BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_LEFT) |
							BIT_MASK(BTN_RIGHT) |
							BIT_MASK(BTN_MIDDLE);
		input_dev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y);
	}

	if (input_info->event_types & USF_KEYBOARD_EVENT) {
		/* Keyboard part configuration */
		input_dev->evbit[0] |= BIT_MASK(EV_KEY);

		/* All keys are permitted */
		memset(input_dev->keybit, 0xff, sizeof(input_dev->keybit));
	}

	rc = input_register_device(input_dev);
	if (rc) {
		pr_err("%s: input_register_device() failed; rc=%d\n",
		       __func__, rc);
		input_free_device(input_dev);
	} else {
		usf_info->input_if = input_dev;
		usf_info->event_types = input_info->event_types;
		pr_info("%s: input device[%s] was registered\n",
			__func__, input_dev->name);
	}

	return rc;

}

static void notify_tsc_event(struct input_dev *input_if,
			     int x, int y, int pressure)
{
	if (pressure) {
		input_report_abs(input_if, ABS_X, x);
		input_report_abs(input_if, ABS_Y, y);
		input_report_abs(input_if, ABS_PRESSURE, pressure);
		input_report_key(input_if, BTN_TOUCH, !!pressure);
	} else {
		input_report_abs(input_if, ABS_PRESSURE, 0);
		input_report_key(input_if, BTN_TOUCH, 0);
	}

	input_sync(input_if);

	pr_debug("%s: TSC event: x[%d], y[%d], pressure[%d]\n",
		 __func__, x, y, pressure);
}

static void notify_mouse_event(struct input_dev *input_if,
			       struct mouse_event_type *me)
{
	if (me == NULL) {
		pr_err("%s: mouse event is NULL\n", __func__);
		return;
	}

	input_report_rel(input_if, REL_X, me->rels[0]);
	input_report_rel(input_if, REL_Y, me->rels[1]);

	input_report_key(input_if, BTN_LEFT,
			 me->buttons_states & USF_BUTTON_LEFT_MASK);
	input_report_key(input_if, BTN_MIDDLE,
			 me->buttons_states & USF_BUTTON_MIDDLE_MASK);
	input_report_key(input_if, BTN_RIGHT,
			 me->buttons_states & USF_BUTTON_RIGHT_MASK);

	input_sync(input_if);

	pr_debug("%s: mouse event: dx[%d], dy[%d], buttons_states[%d]\n",
		 __func__, me->rels[0], me->rels[1], me->buttons_states);
}

static void notify_key_event(struct input_dev *input_if,
			       struct key_event_type *ke)
{
	if (ke == NULL) {
		pr_err("%s: key event is NULL\n", __func__);
		return;
	}

	input_report_key(input_if, ke->key, ke->key_state);
	input_sync(input_if);
	pr_debug("%s: key event: key[%d], state[%d]\n",
		 __func__,
		 ke->key,
		 ke->key_state);

}

static void handle_input_event(struct usf_type *usf_info,
			       uint16_t event_counter,
			       struct usf_event_type *event)
{
	struct input_dev *input_if = NULL;
	uint16_t ind = 0;

	if ((usf_info == NULL) || (usf_info->input_if == NULL) ||
	    (event == NULL) || (!event_counter)) {
		return;
	}

	input_if = usf_info->input_if;

	for (ind = 0; ind < event_counter; ++ind) {
		event += ind;
		if (event->event_type & usf_info->event_types) {
			/* the event is supported */
			if (event->event_type & USF_TSC_EVENT) {
				struct point_event_type *pe =
					&(event->event_data.point_event);
				if (pe->coordinates_type ==
				    USF_PIX_COORDINATE) {
					notify_tsc_event(input_if,
							 pe->coordinates[0],
							 pe->coordinates[1],
							 pe->pressure);
				} else
					pr_debug("%s: wrong coord type:%d\n",
						 __func__,
						 pe->coordinates_type);

				continue;
			}

			if (event->event_type & USF_MOUSE_EVENT) {
				notify_mouse_event(input_if,
					&(event->event_data.mouse_event));
				continue;
			}

			if (event->event_type & USF_KEYBOARD_EVENT) {
				notify_key_event(input_if,
					&(event->event_data.key_event));
				continue;
			}
		} /* the event is supported */
	}
}

static int usf_start_tx(struct usf_xx_type *usf_xx)
{
	int rc = q6usm_run(usf_xx->usc, 0, 0, 0);

	pr_debug("%s: tx: q6usm_run; rc=%d\n", __func__, rc);
	if (!rc) {
		/* supply all buffers */
		rc = q6usm_read(usf_xx->usc,
				usf_xx->buffer_count);
		pr_debug("%s: q6usm_read[%d]\n",
			 __func__, rc);

		if (rc)
			pr_err("%s: buf read failed",
			       __func__);
		else
			usf_xx->usf_state =
				USF_WORK_STATE;
	}

	return rc;
} /* usf_start_tx */

static int usf_start_rx(struct usf_xx_type *usf_xx)
{
	int rc = q6usm_run(usf_xx->usc, 0, 0, 0);

	pr_debug("%s: rx: q6usm_run; rc=%d\n",
		 __func__, rc);
	if (!rc)
		usf_xx->usf_state = USF_WORK_STATE;

	return rc;
} /* usf_start_rx */

static int usf_set_tx_info(struct usf_type *usf, unsigned long arg)
{
	struct us_tx_info_type config_tx;
	const char *name = NULL;
	struct usf_xx_type *usf_xx =  &usf->usf_tx;
	int rc = copy_from_user(&config_tx,
			    (void *) arg,
			    sizeof(config_tx));

	if (rc) {
		pr_err("%s: copy error[%d]\n",
			__func__, rc);
		return -EINVAL;
	}

	name = config_tx.us_xx_info.client_name;

	usf_xx->new_region = USM_UNDEF_TOKEN;
	usf_xx->prev_region = USM_UNDEF_TOKEN;
	usf_xx->cb = usf_tx_cb;

	init_waitqueue_head(&usf_xx->wait);

	if (name != NULL) {
		int res = strncpy_from_user(
			usf_xx->client_name,
			name,
			sizeof(usf_xx->client_name)-1);
		if (res < 0) {
			pr_err("%s: get client name failed\n",
			       __func__);
			return -EINVAL;
		}
	}

	rc = config_xx(usf_xx, &config_tx.us_xx_info);
	if (rc)
		return rc;

	rc = q6usm_open_read(usf_xx->usc,
			     usf_xx->encdec_cfg.format_id);
	if (rc)
		return rc;

	rc = q6usm_us_client_buf_alloc(OUT, usf_xx->usc,
				       usf_xx->buffer_size,
				       usf_xx->buffer_count);
	if (rc)
		return rc;

	rc = q6usm_enc_cfg_blk(usf_xx->usc,
			       &usf_xx->encdec_cfg);
	if (!rc) {
		rc = register_input_device(usf,
					   &config_tx.input_info);
		if (!rc)
			usf_xx->usf_state = USF_CONFIGURED_STATE;
	}

	return rc;
} /* usf_set_tx_info */

static int usf_set_rx_info(struct usf_type *usf, unsigned long arg)
{
	struct us_rx_info_type config_rx;
	struct usf_xx_type *usf_xx =  &usf->usf_rx;
	int rc = copy_from_user(&config_rx,
			    (void *) arg,
			    sizeof(config_rx));

	if (rc) {
		pr_err("%s: copy_from_user() failed[%d]\n",
			__func__, rc);
		return -EINVAL;
	}

	usf_xx->new_region = USM_UNDEF_TOKEN;
	usf_xx->prev_region = USM_UNDEF_TOKEN;

	usf_xx->cb = usf_rx_cb;

	rc = config_xx(usf_xx, &config_rx.us_xx_info);
	if (rc)
		return rc;

	rc = q6usm_open_write(usf_xx->usc,
			      usf_xx->encdec_cfg.format_id);
	if (rc)
		return rc;

	if (usf_xx->buffer_size && usf_xx->buffer_count) {
		rc = q6usm_us_client_buf_alloc(
					IN,
					usf_xx->usc,
					usf_xx->buffer_size,
					usf_xx->buffer_count);
		if (rc)
			return rc;
	}

	rc = q6usm_dec_cfg_blk(usf_xx->usc,
			       &usf_xx->encdec_cfg);
	if (!rc) {
		init_waitqueue_head(&usf_xx->wait);
		usf_xx->usf_state = USF_CONFIGURED_STATE;
	}

	return rc;
} /* usf_set_rx_info */


static int usf_get_tx_update(struct usf_type *usf, unsigned long arg)
{
	struct us_tx_update_info_type upd_tx_info;
	unsigned long prev_jiffies = 0;
	struct usf_xx_type *usf_xx =  &usf->usf_tx;
	int rc = copy_from_user(&upd_tx_info, (void *) arg,
			    sizeof(upd_tx_info));

	if (rc) {
		pr_err("%s: get_update: copy_from_user() failed[%d]\n",
		       __func__, rc);
		return -EINVAL;
	}

	if (!usf_xx->user_upd_info_na) {
		handle_input_event(usf,
				   upd_tx_info.event_counter,
				   upd_tx_info.event);

		/* Release available regions */
		rc = q6usm_read(usf_xx->usc,
				upd_tx_info.free_region);
		if (rc)
			return rc;
	} else
		usf_xx->user_upd_info_na = 0;

	/* Get data ready regions */
	prev_jiffies = jiffies;
	rc = wait_event_timeout(usf_xx->wait,
				(usf_xx->prev_region !=
				 usf_xx->new_region) ||
				(usf_xx->usf_state !=
				 USF_WORK_STATE),
				USF_TIMEOUT_JIFFIES);

	if (!rc) {
		pr_debug("%s: timeout. prev_j=%lu; j=%lu\n",
			__func__, prev_jiffies, jiffies);
		pr_debug("%s: timeout. prev=%d; new=%d\n",
			__func__, usf_xx->prev_region,
			usf_xx->new_region);
		pr_debug("%s: timeout. free_region=%d;\n",
			__func__, upd_tx_info.free_region);
		if (usf_xx->prev_region ==
		    usf_xx->new_region) {
			pr_err("%s:read data: timeout\n",
			       __func__);
			return -ETIME;
		}
	}

	if (usf_xx->usf_state != USF_WORK_STATE) {
		pr_err("%s: TX device is in not work state[%d]\n",
		       __func__, usf_xx->usf_state);
		return -EINTR;
	}

	upd_tx_info.ready_region = usf_xx->new_region;
	rc = copy_to_user((void __user *)arg, &upd_tx_info,
			  sizeof(upd_tx_info));
	if (upd_tx_info.ready_region == USM_WRONG_TOKEN) {
		pr_err("%s: TX path corrupted; prev=%d\n",
		       __func__, usf_xx->prev_region);
		rc = -EIO;
	}
	usf_xx->prev_region = upd_tx_info.ready_region;

	return rc;
} /* usf_get_tx_update */

static int usf_set_rx_update(struct usf_xx_type *usf_xx, unsigned long arg)
{
	struct us_rx_update_info_type upd_rx_info;
	int rc = copy_from_user(&upd_rx_info, (void *) arg,
			    sizeof(upd_rx_info));

	if (rc) {
		pr_err("%s: get_update: copy_from_user() failed[%d]\n",
		       __func__, rc);
		return -EINVAL;
	}

	/* Send available data regions */
	if (upd_rx_info.ready_region !=
	    usf_xx->buffer_count) {
		rc = q6usm_write(
			usf_xx->usc,
			upd_rx_info.ready_region);
		if (rc)
			return rc;
	}

	/* Get free regions */
	rc = wait_event_timeout(
		usf_xx->wait,
		!q6usm_is_write_buf_full(
			usf_xx->usc,
			&upd_rx_info.free_region) ||
		(usf_xx->usf_state == USF_IDLE_STATE),
		USF_TIMEOUT_JIFFIES);

	if (!rc) {
		rc = -ETIME;
		pr_err("%s:timeout. wait for write buf not full\n",
		       __func__);
	} else {
		if (usf_xx->usf_state !=
		    USF_WORK_STATE) {
			pr_err("%s: RX: state[%d]\n",
			       __func__,
			       usf_xx->usf_state);
			rc = -EINTR;
		} else
			rc = copy_to_user(
				(void __user *)arg,
				&upd_rx_info,
				sizeof(upd_rx_info));
	}

	return rc;
} /* usf_set_rx_update */

static int usf_stop_tx(struct usf_type *usf)
{
	struct usf_xx_type *usf_xx =  &usf->usf_tx;

	if (usf->input_if != NULL) {
		input_unregister_device(usf->input_if);
		usf->input_if = NULL;
		pr_info("%s input_unregister_device",
			__func__);
	}
	usf_disable(usf_xx);

	return 0;
} /* usf_stop_tx */

static long usf_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	struct usf_type *usf = file->private_data;
	struct usf_xx_type *usf_xx = NULL;

	switch (cmd) {
	case US_START_TX: {
		usf_xx = &usf->usf_tx;
		if (usf_xx->usf_state == USF_CONFIGURED_STATE)
			rc = usf_start_tx(usf_xx);
		else {
			pr_err("%s: start_tx: wrong state[%d]\n",
			       __func__,
			       usf_xx->usf_state);
			return -EBADFD;
		}
		break;
	}

	case US_START_RX: {
		usf_xx = &usf->usf_rx;
		if (usf_xx->usf_state == USF_CONFIGURED_STATE)
			rc = usf_start_rx(usf_xx);
		else {
			pr_err("%s: start_rx: wrong state[%d]\n",
				__func__,
				usf_xx->usf_state);
			return -EBADFD;
		}
		break;
	}

	case US_SET_TX_INFO: {
		usf_xx = &usf->usf_tx;
		if (usf_xx->usf_state == USF_OPENED_STATE)
			rc = usf_set_tx_info(usf, arg);
		else {
			pr_err("%s: set_tx_info: wrong state[%d]\n",
			       __func__,
			       usf_xx->usf_state);
			return -EBADFD;
		}

		break;
	} /* US_SET_TX_INFO */

	case US_SET_RX_INFO: {
		usf_xx = &usf->usf_rx;
		if (usf_xx->usf_state == USF_OPENED_STATE)
			rc = usf_set_rx_info(usf, arg);
		else {
			pr_err("%s: set_rx_info: wrong state[%d]\n",
				__func__,
				usf_xx->usf_state);
			return -EBADFD;
		}

		break;
	} /* US_SET_RX_INFO */

	case US_GET_TX_UPDATE: {
		struct usf_xx_type *usf_xx = &usf->usf_tx;
		if (usf_xx->usf_state == USF_WORK_STATE)
			rc = usf_get_tx_update(usf, arg);
		else {
			pr_err("%s: get_tx_update: wrong state[%d]\n", __func__,
			       usf_xx->usf_state);
			rc = -EBADFD;
		}
		break;
	} /* US_GET_TX_UPDATE */

	case US_SET_RX_UPDATE: {
		struct usf_xx_type *usf_xx = &usf->usf_rx;
		if (usf_xx->usf_state == USF_WORK_STATE)
			rc = usf_set_rx_update(usf_xx, arg);
		else {
			pr_err("%s: set_rx_update: wrong state[%d]\n",
			       __func__,
			       usf_xx->usf_state);
			rc = -EBADFD;
		}
		break;
	} /* US_SET_RX_UPDATE */

	case US_STOP_TX: {
		usf_xx = &usf->usf_tx;
		if (usf_xx->usf_state == USF_WORK_STATE)
			rc = usf_stop_tx(usf);
		else {
			pr_err("%s: stop_tx: wrong state[%d]\n",
			       __func__,
			       usf_xx->usf_state);
			return -EBADFD;
		}
		break;
	} /* US_STOP_TX */

	case US_STOP_RX: {
		usf_xx = &usf->usf_rx;
		if (usf_xx->usf_state == USF_WORK_STATE)
			usf_disable(usf_xx);
		else {
			pr_err("%s: stop_rx: wrong state[%d]\n",
			       __func__,
			       usf_xx->usf_state);
			return -EBADFD;
		}
		break;
	} /* US_STOP_RX */

	default:
		rc = -EINVAL;
		break;
	}

	if (rc &&
	    ((cmd == US_SET_TX_INFO) ||
	     (cmd == US_SET_RX_INFO)))
		release_xx(usf_xx);

	return rc;
} /* usf_ioctl */

static int usf_mmap(struct file *file, struct vm_area_struct *vms)
{
	struct usf_type *usf = file->private_data;
	int dir = OUT;
	struct usf_xx_type *usf_xx = &usf->usf_tx;

	if (vms->vm_flags & USF_VM_WRITE) { /* RX buf mapping */
		dir = IN;
		usf_xx = &usf->usf_rx;
	}

	return q6usm_get_virtual_address(dir, usf_xx->usc, vms);
}

static uint16_t add_opened_dev(int minor)
{
	uint16_t ind = 0;

	for (ind = 0; ind < MAX_DEVS_NUMBER; ++ind) {
		if (minor == s_opened_devs[ind]) {
			pr_err("%s: device %d is already opened\n",
			       __func__, minor);
			return USF_UNDEF_DEV_ID;
		}

		if (s_opened_devs[ind] == 0) {
			s_opened_devs[ind] = minor;
			pr_info("%s: device %d is added; ind=%d\n",
				__func__, minor, ind);
			return ind;
		}
	}

	pr_err("%s: there is no place for device %d\n",
	       __func__, minor);
	return USF_UNDEF_DEV_ID;
}

static int usf_open(struct inode *inode, struct file *file)
{
	struct usf_type *usf =  NULL;
	uint16_t dev_ind = 0;
	int minor = MINOR(inode->i_rdev);

	dev_ind = add_opened_dev(minor);
	if (dev_ind == USF_UNDEF_DEV_ID)
		return -EBUSY;

	usf = kzalloc(sizeof(struct usf_type), GFP_KERNEL);
	if (usf == NULL) {
		pr_err("%s:usf allocation failed\n", __func__);
		return -ENOMEM;
	}

	file->private_data = usf;
	usf->dev_ind = dev_ind;

	usf->usf_tx.usf_state = USF_OPENED_STATE;
	usf->usf_rx.usf_state = USF_OPENED_STATE;

	pr_info("%s:usf in open\n", __func__);
	return 0;
}


static int usf_release(struct inode *inode, struct file *file)
{
	struct usf_type *usf = file->private_data;

	pr_info("%s: release entry\n", __func__);

	if (usf->input_if != NULL) {
		input_unregister_device(usf->input_if);
		usf->input_if = NULL;
		pr_info("%s input_unregister_device\n",  __func__);
	}

	usf_disable(&usf->usf_tx);
	usf_disable(&usf->usf_rx);

	s_opened_devs[usf->dev_ind] = 0;

	kfree(usf);
	pr_info("%s: release exit\n", __func__);
	return 0;
}

static const struct file_operations usf_fops = {
	.owner                  = THIS_MODULE,
	.open                   = usf_open,
	.release                = usf_release,
	.unlocked_ioctl = usf_ioctl,
	.mmap                   = usf_mmap,
};

struct miscdevice usf_misc[MAX_DEVS_NUMBER] = {
	{
		.minor  = MISC_DYNAMIC_MINOR,
		.name   = "usf1",
		.fops   = &usf_fops,
	},
};

static int __init usf_init(void)
{
	int rc = 0;
	uint16_t ind = 0;

	pr_info("%s: USF SW version %s.\n", __func__, DRV_VERSION);
	pr_info("%s: Max %d devs registration\n", __func__, MAX_DEVS_NUMBER);

	for (ind = 0; ind < MAX_DEVS_NUMBER; ++ind) {
		rc = misc_register(&usf_misc[ind]);
		if (rc) {
			pr_err("%s: misc_register() failed ind=%d; rc = %d\n",
			       __func__, ind, rc);
			break;
		}
	}

	return rc;
}

device_initcall(usf_init);

MODULE_DESCRIPTION("Ultrasound framework driver");
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL v2");
