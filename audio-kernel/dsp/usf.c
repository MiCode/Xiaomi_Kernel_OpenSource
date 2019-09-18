/* Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
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

#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/kmemleak.h>
#include <linux/mutex.h>
#include <dsp/apr_audio-v2.h>
#include "q6usm.h"
#include "usf.h"
#include "usfcdev.h"
#include "q6_init.h"

/* The driver version*/
#define DRV_VERSION "1.7.1"
#define USF_VERSION_ID 0x0171

/* Standard timeout in the asynchronous ops */
#define USF_TIMEOUT_JIFFIES (1*HZ) /* 1 sec */

/* Undefined USF device */
#define USF_UNDEF_DEV_ID 0xffff

/* TX memory mapping flag */
#define USF_VM_READ 1
/* RX memory mapping flag */
#define USF_VM_WRITE 2

/* Number of events, copied from the user space to kernel one */
#define USF_EVENTS_PORTION_SIZE 20

/* Indexes in range definitions */
#define MIN_IND 0
#define MAX_IND 1

/* The coordinates indexes */
#define X_IND 0
#define Y_IND 1
#define Z_IND 2

/* Shared memory limits */
/* max_buf_size = (port_size(65535*2) * port_num(8) * group_size(3) */
#define USF_MAX_BUF_SIZE 3145680
#define USF_MAX_BUF_NUM  32

/* max size for buffer set from user space */
#define USF_MAX_USER_BUF_SIZE 100000

/* Place for opreation result, received from QDSP6 */
#define APR_RESULT_IND 1

/* Place for US detection result, received from QDSP6 */
#define APR_US_DETECT_RESULT_IND 0

#define BITS_IN_BYTE 8

/* Time to stay awake after tx read event (e.g., proximity) */
#define STAY_AWAKE_AFTER_READ_MSECS 3000

/* The driver states */
enum usf_state_type {
	USF_IDLE_STATE,
	USF_OPENED_STATE,
	USF_CONFIGURED_STATE,
	USF_WORK_STATE,
	USF_ADSP_RESTART_STATE,
	USF_ERROR_STATE
};

/* The US detection status upon FW/HW based US detection results */
enum usf_us_detect_type {
	USF_US_DETECT_UNDEF,
	USF_US_DETECT_YES,
	USF_US_DETECT_NO
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
	/* US detection result */
	enum usf_us_detect_type us_detect_type;
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
	/*  The input devices are "input" module registered clients */
	struct input_dev *input_ifs[USF_MAX_EVENT_IND];
	/* Bitmap of types of events, conflicting to USF's ones */
	uint16_t conflicting_event_types;
	/* Bitmap of types of events from devs, conflicting with USF */
	uint16_t conflicting_event_filters;
	/* The requested buttons bitmap */
	uint16_t req_buttons_bitmap;
	/* Mutex for exclusive operations (all public APIs) */
	struct mutex mutex;
};

struct usf_input_dev_type {
	/* Input event type, supported by the input device */
	uint16_t event_type;
	/* Input device name */
	const char *input_dev_name;
	/* Input device registration function */
	int (*prepare_dev)(uint16_t, struct usf_type *,
			    struct us_input_info_type *,
			   const char *);
	/* Input event notification function */
	void (*notify_event)(struct usf_type *,
			     uint16_t,
			     struct usf_event_type *
			     );
};


/* The MAX number of the supported devices */
#define MAX_DEVS_NUMBER	1

/*
 * code for a special button that is used to show/hide a
 * hovering cursor in the input framework. Must be in
 * sync with the button code definition in the framework
 * (EventHub.h)
 */
#define BTN_USF_HOVERING_CURSOR         0x230

/* Supported buttons container */
static const int s_button_map[] = {
	BTN_STYLUS,
	BTN_STYLUS2,
	BTN_TOOL_PEN,
	BTN_TOOL_RUBBER,
	BTN_TOOL_FINGER,
	BTN_USF_HOVERING_CURSOR
};

/* The opened devices container */
static atomic_t s_opened_devs[MAX_DEVS_NUMBER];

static struct wakeup_source usf_wakeup_source;

#define USF_NAME_PREFIX "usf_"
#define USF_NAME_PREFIX_SIZE 4


static struct input_dev *allocate_dev(uint16_t ind, const char *name)
{
	struct input_dev *in_dev = input_allocate_device();

	if (in_dev == NULL) {
		pr_err("%s: input_allocate_device() failed\n", __func__);
	} else {
		/* Common part configuration */
		in_dev->name = name;
		in_dev->phys = NULL;
		in_dev->id.bustype = BUS_HOST;
		in_dev->id.vendor  = 0x0001;
		in_dev->id.product = 0x0001;
		in_dev->id.version = USF_VERSION_ID;
	}
	return in_dev;
}

static int prepare_tsc_input_device(uint16_t ind,
				struct usf_type *usf_info,
				struct us_input_info_type *input_info,
				const char *name)
{
	int i = 0;

	int num_buttons = min(ARRAY_SIZE(s_button_map),
		sizeof(input_info->req_buttons_bitmap) *
		BITS_IN_BYTE);
	uint16_t max_buttons_bitmap = ((1 << ARRAY_SIZE(s_button_map)) - 1);

	struct input_dev *in_dev = allocate_dev(ind, name);

	if (in_dev == NULL)
		return -ENOMEM;

	if (input_info->req_buttons_bitmap > max_buttons_bitmap) {
		pr_err("%s: Requested buttons[%d] exceeds max buttons available[%d]\n",
		__func__,
		input_info->req_buttons_bitmap,
		max_buttons_bitmap);
		input_free_device(in_dev);
		return -EINVAL;
	}

	usf_info->input_ifs[ind] = in_dev;
	usf_info->req_buttons_bitmap =
		input_info->req_buttons_bitmap;
	in_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	in_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	for (i = 0; i < num_buttons; i++)
		if (input_info->req_buttons_bitmap & (1 << i))
			in_dev->keybit[BIT_WORD(s_button_map[i])] |=
			BIT_MASK(s_button_map[i]);

	input_set_abs_params(in_dev, ABS_X,
			     input_info->tsc_x_dim[MIN_IND],
			     input_info->tsc_x_dim[MAX_IND],
			     0, 0);
	input_set_abs_params(in_dev, ABS_Y,
			     input_info->tsc_y_dim[MIN_IND],
			     input_info->tsc_y_dim[MAX_IND],
			     0, 0);
	input_set_abs_params(in_dev, ABS_DISTANCE,
			     input_info->tsc_z_dim[MIN_IND],
			     input_info->tsc_z_dim[MAX_IND],
			     0, 0);

	input_set_abs_params(in_dev, ABS_PRESSURE,
			     input_info->tsc_pressure[MIN_IND],
			     input_info->tsc_pressure[MAX_IND],
			     0, 0);

	input_set_abs_params(in_dev, ABS_TILT_X,
			     input_info->tsc_x_tilt[MIN_IND],
			     input_info->tsc_x_tilt[MAX_IND],
			     0, 0);
	input_set_abs_params(in_dev, ABS_TILT_Y,
			     input_info->tsc_y_tilt[MIN_IND],
			     input_info->tsc_y_tilt[MAX_IND],
			     0, 0);

	return 0;
}

static int prepare_mouse_input_device(uint16_t ind, struct usf_type *usf_info,
			struct us_input_info_type *input_info,
			const char *name)
{
	struct input_dev *in_dev = allocate_dev(ind, name);

	if (in_dev == NULL)
		return -ENOMEM;

	usf_info->input_ifs[ind] = in_dev;
	in_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);

	in_dev->keybit[BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_LEFT) |
						BIT_MASK(BTN_RIGHT) |
						BIT_MASK(BTN_MIDDLE);
	in_dev->relbit[0] =  BIT_MASK(REL_X) |
				BIT_MASK(REL_Y) |
				BIT_MASK(REL_Z);

	return 0;
}

static int prepare_keyboard_input_device(
					uint16_t ind,
					struct usf_type *usf_info,
					struct us_input_info_type *input_info,
					const char *name)
{
	struct input_dev *in_dev = allocate_dev(ind, name);

	if (in_dev == NULL)
		return -ENOMEM;

	usf_info->input_ifs[ind] = in_dev;
	in_dev->evbit[0] |= BIT_MASK(EV_KEY);
	/* All keys are permitted */
	memset(in_dev->keybit, 0xff, sizeof(in_dev->keybit));

	return 0;
}

static void notify_tsc_event(struct usf_type *usf_info,
			     uint16_t if_ind,
			     struct usf_event_type *event)

{
	int i = 0;
	int num_buttons = min(ARRAY_SIZE(s_button_map),
		sizeof(usf_info->req_buttons_bitmap) *
		BITS_IN_BYTE);

	struct input_dev *input_if = usf_info->input_ifs[if_ind];
	struct point_event_type *pe = &(event->event_data.point_event);

	input_report_abs(input_if, ABS_X, pe->coordinates[X_IND]);
	input_report_abs(input_if, ABS_Y, pe->coordinates[Y_IND]);
	input_report_abs(input_if, ABS_DISTANCE, pe->coordinates[Z_IND]);

	input_report_abs(input_if, ABS_TILT_X, pe->inclinations[X_IND]);
	input_report_abs(input_if, ABS_TILT_Y, pe->inclinations[Y_IND]);

	input_report_abs(input_if, ABS_PRESSURE, pe->pressure);
	input_report_key(input_if, BTN_TOUCH, !!(pe->pressure));

	for (i = 0; i < num_buttons; i++) {
		uint16_t mask = (1 << i),
		btn_state = !!(pe->buttons_state_bitmap & mask);
		if (usf_info->req_buttons_bitmap & mask)
			input_report_key(input_if, s_button_map[i], btn_state);
	}

	input_sync(input_if);

	pr_debug("%s: TSC event: xyz[%d;%d;%d], incl[%d;%d], pressure[%d], buttons[%d]\n",
		 __func__,
		 pe->coordinates[X_IND],
		 pe->coordinates[Y_IND],
		 pe->coordinates[Z_IND],
		 pe->inclinations[X_IND],
		 pe->inclinations[Y_IND],
		 pe->pressure,
		 pe->buttons_state_bitmap);
}

static void notify_mouse_event(struct usf_type *usf_info,
			       uint16_t if_ind,
			       struct usf_event_type *event)
{
	struct input_dev *input_if = usf_info->input_ifs[if_ind];
	struct mouse_event_type *me = &(event->event_data.mouse_event);

	input_report_rel(input_if, REL_X, me->rels[X_IND]);
	input_report_rel(input_if, REL_Y, me->rels[Y_IND]);
	input_report_rel(input_if, REL_Z, me->rels[Z_IND]);

	input_report_key(input_if, BTN_LEFT,
			 me->buttons_states & USF_BUTTON_LEFT_MASK);
	input_report_key(input_if, BTN_MIDDLE,
			 me->buttons_states & USF_BUTTON_MIDDLE_MASK);
	input_report_key(input_if, BTN_RIGHT,
			 me->buttons_states & USF_BUTTON_RIGHT_MASK);

	input_sync(input_if);

	pr_debug("%s: mouse event: dx[%d], dy[%d], buttons_states[%d]\n",
		 __func__, me->rels[X_IND],
		 me->rels[Y_IND], me->buttons_states);
}

static void notify_key_event(struct usf_type *usf_info,
			     uint16_t if_ind,
			     struct usf_event_type *event)
{
	struct input_dev *input_if = usf_info->input_ifs[if_ind];
	struct key_event_type *ke = &(event->event_data.key_event);

	input_report_key(input_if, ke->key, ke->key_state);
	input_sync(input_if);
	pr_debug("%s: key event: key[%d], state[%d]\n",
		 __func__,
		 ke->key,
		 ke->key_state);

}

static struct usf_input_dev_type s_usf_input_devs[] = {
	{USF_TSC_EVENT, "usf_tsc",
		prepare_tsc_input_device, notify_tsc_event},
	{USF_TSC_PTR_EVENT, "usf_tsc_ptr",
		prepare_tsc_input_device, notify_tsc_event},
	{USF_MOUSE_EVENT, "usf_mouse",
		prepare_mouse_input_device, notify_mouse_event},
	{USF_KEYBOARD_EVENT, "usf_kb",
		prepare_keyboard_input_device, notify_key_event},
	{USF_TSC_EXT_EVENT, "usf_tsc_ext",
		prepare_tsc_input_device, notify_tsc_event},
};

static void usf_rx_cb(uint32_t opcode, uint32_t token,
		      uint32_t *payload, void *priv)
{
	struct usf_xx_type *usf_xx = (struct usf_xx_type *) priv;

	if (usf_xx == NULL) {
		pr_err("%s: the private data is NULL\n", __func__);
		return;
	}

	switch (opcode) {
	case Q6USM_EVENT_WRITE_DONE:
		wake_up(&usf_xx->wait);
		break;

	case RESET_EVENTS:
		pr_err("%s: received RESET_EVENTS\n", __func__);
		usf_xx->usf_state = USF_ADSP_RESTART_STATE;
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
	case Q6USM_EVENT_READ_DONE:
		pr_debug("%s: acquiring %d msec wake lock\n", __func__,
				STAY_AWAKE_AFTER_READ_MSECS);
		__pm_wakeup_event(&usf_wakeup_source,
				  STAY_AWAKE_AFTER_READ_MSECS);
		if (token == USM_WRONG_TOKEN)
			usf_xx->usf_state = USF_ERROR_STATE;
		usf_xx->new_region = token;
		wake_up(&usf_xx->wait);
		break;

	case Q6USM_EVENT_SIGNAL_DETECT_RESULT:
		usf_xx->us_detect_type = (payload[APR_US_DETECT_RESULT_IND]) ?
					USF_US_DETECT_YES :
					USF_US_DETECT_NO;

		wake_up(&usf_xx->wait);
		break;

	case APR_BASIC_RSP_RESULT:
		if (payload[APR_RESULT_IND]) {
			usf_xx->usf_state = USF_ERROR_STATE;
			usf_xx->new_region = USM_WRONG_TOKEN;
			wake_up(&usf_xx->wait);
		}
		break;

	case RESET_EVENTS:
		pr_err("%s: received RESET_EVENTS\n", __func__);
		usf_xx->usf_state = USF_ADSP_RESTART_STATE;
		wake_up(&usf_xx->wait);
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
	uint16_t min_map_size = 0;

	if ((usf_xx == NULL) ||
	    (config == NULL))
		return -EINVAL;

	if ((config->buf_size == 0) ||
	    (config->buf_size > USF_MAX_BUF_SIZE) ||
	    (config->buf_num == 0) ||
	    (config->buf_num > USF_MAX_BUF_NUM)) {
		pr_err("%s: wrong params: buf_size=%d; buf_num=%d\n",
		       __func__, config->buf_size, config->buf_num);
		return -EINVAL;
	}

	data_map_size = sizeof(usf_xx->encdec_cfg.cfg_common.data_map);
	min_map_size = min(data_map_size, config->port_cnt);

	if (config->client_name != NULL) {
		if (strncpy_from_user(usf_xx->client_name,
				      (char __user *)config->client_name,
				      sizeof(usf_xx->client_name) - 1) < 0) {
			pr_err("%s: get client name failed\n", __func__);
			return -EINVAL;
		}
	}

	pr_debug("%s: name=%s; buf_size:%d; dev_id:0x%x; sample_rate:%d\n",
		__func__, usf_xx->client_name, config->buf_size,
		config->dev_id, config->sample_rate);

	pr_debug("%s: buf_num:%d; format:%d; port_cnt:%d; data_size=%d\n",
		__func__, config->buf_num, config->stream_format,
		config->port_cnt, config->params_data_size);

	pr_debug("%s: id[0]=%d, id[1]=%d, id[2]=%d, id[3]=%d, id[4]=%d,\n",
		__func__,
		config->port_id[0],
		config->port_id[1],
		config->port_id[2],
		config->port_id[3],
		config->port_id[4]);

	pr_debug("id[5]=%d, id[6]=%d, id[7]=%d\n",
		config->port_id[5],
		config->port_id[6],
		config->port_id[7]);

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
	       min_map_size);

	usf_xx->encdec_cfg.format_id = config->stream_format;
	usf_xx->encdec_cfg.params_size = config->params_data_size;
	usf_xx->user_upd_info_na = 1; /* it's used in US_GET_TX_UPDATE */

	if (config->params_data_size > 0) { /* transparent data copy */
		usf_xx->encdec_cfg.params = kzalloc(config->params_data_size,
						    GFP_KERNEL);
		/* False memory leak here - pointer in packed struct
		 * is undetected by kmemleak tool
		 */
		kmemleak_ignore(usf_xx->encdec_cfg.params);
		if (usf_xx->encdec_cfg.params == NULL) {
			pr_err("%s: params memory alloc[%d] failure\n",
				__func__,
				config->params_data_size);
			return -ENOMEM;
		}
		rc = copy_from_user(usf_xx->encdec_cfg.params,
				    (uint8_t __user *)config->params_data,
				    config->params_data_size);
		if (rc) {
			pr_err("%s: transparent data copy failure\n",
			       __func__);
			kfree(usf_xx->encdec_cfg.params);
			usf_xx->encdec_cfg.params = NULL;
			return -EFAULT;
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

static bool usf_match(uint16_t event_type_ind, struct input_dev *dev)
{
	bool rc = false;

	rc = (event_type_ind < MAX_EVENT_TYPE_NUM) &&
		((dev->name == NULL) ||
		strcmp(dev->name, USF_NAME_PREFIX));
	pr_debug("%s: name=[%s]; rc=%d\n",
		 __func__, dev->name, rc);

	return rc;
}

static bool usf_register_conflicting_events(uint16_t event_types)
{
	bool rc = true;
	uint16_t ind = 0;
	uint16_t mask = 1;

	for (ind = 0; ind < MAX_EVENT_TYPE_NUM; ++ind) {
		if (event_types & mask) {
			rc = usfcdev_register(ind, usf_match);
			if (!rc)
				break;
		}
		mask = mask << 1;
	}

	return rc;
}

static void usf_unregister_conflicting_events(uint16_t event_types)
{
	uint16_t ind = 0;
	uint16_t mask = 1;

	for (ind = 0; ind < MAX_EVENT_TYPE_NUM; ++ind) {
		if (event_types & mask)
			usfcdev_unregister(ind);
		mask = mask << 1;
	}
}

static void usf_set_event_filters(struct usf_type *usf, uint16_t event_filters)
{
	uint16_t ind = 0;
	uint16_t mask = 1;

	if (usf->conflicting_event_filters != event_filters) {
		for (ind = 0; ind < MAX_EVENT_TYPE_NUM; ++ind) {
			if (usf->conflicting_event_types & mask)
				usfcdev_set_filter(ind, event_filters&mask);
			mask = mask << 1;
		}
		usf->conflicting_event_filters = event_filters;
	}
}

static int register_input_device(struct usf_type *usf_info,
				 struct us_input_info_type *input_info)
{
	int rc = 0;
	bool ret = true;
	uint16_t ind = 0;

	if ((usf_info == NULL) ||
	    (input_info == NULL) ||
	    !(input_info->event_types & USF_ALL_EVENTS)) {
		pr_err("%s: wrong input parameter(s)\n", __func__);
		return -EINVAL;
	}

	for (ind = 0; ind < USF_MAX_EVENT_IND; ++ind) {
		if (usf_info->input_ifs[ind] != NULL) {
			pr_err("%s: input_if[%d] is already allocated\n",
				__func__, ind);
			return -EFAULT;
		}
		if ((input_info->event_types &
			s_usf_input_devs[ind].event_type) &&
		     s_usf_input_devs[ind].prepare_dev) {
			rc = (*s_usf_input_devs[ind].prepare_dev)(
				ind,
				usf_info,
				input_info,
				s_usf_input_devs[ind].input_dev_name);
			if (rc)
				return rc;

			rc = input_register_device(usf_info->input_ifs[ind]);
			if (rc) {
				pr_err("%s: input_reg_dev() failed; rc=%d\n",
					__func__, rc);
				input_free_device(usf_info->input_ifs[ind]);
				usf_info->input_ifs[ind] = NULL;
			} else {
				usf_info->event_types |=
					s_usf_input_devs[ind].event_type;
				pr_debug("%s: input device[%s] was registered\n",
					__func__,
					s_usf_input_devs[ind].input_dev_name);
			}
		} /* supported event */
	} /* event types loop */

	ret = usf_register_conflicting_events(
			input_info->conflicting_event_types);
	if (ret)
		usf_info->conflicting_event_types =
			input_info->conflicting_event_types;

	return 0;
}


static void handle_input_event(struct usf_type *usf_info,
			       uint16_t event_counter,
			       struct usf_event_type __user *event)
{
	uint16_t ind = 0;
	uint16_t events_num = 0;
	struct usf_event_type usf_events[USF_EVENTS_PORTION_SIZE];
	int rc = 0;

	if ((usf_info == NULL) ||
	    (event == NULL) || (!event_counter)) {
		return;
	}

	while (event_counter > 0) {
		if (event_counter > USF_EVENTS_PORTION_SIZE) {
			events_num = USF_EVENTS_PORTION_SIZE;
			event_counter -= USF_EVENTS_PORTION_SIZE;
		} else {
			events_num = event_counter;
			event_counter = 0;
		}
		rc = copy_from_user(usf_events,
				(struct usf_event_type __user *)event,
				events_num * sizeof(struct usf_event_type));
		if (rc) {
			pr_err("%s: copy upd_rx_info from user; rc=%d\n",
				__func__, rc);
			return;
		}
		for (ind = 0; ind < events_num; ++ind) {
			struct usf_event_type *p_event = &usf_events[ind];
			uint16_t if_ind = p_event->event_type_ind;

			if ((if_ind >= USF_MAX_EVENT_IND) ||
			    (usf_info->input_ifs[if_ind] == NULL))
				continue; /* event isn't supported */

			if (s_usf_input_devs[if_ind].notify_event)
				(*s_usf_input_devs[if_ind].notify_event)(
								usf_info,
								if_ind,
								p_event);
		} /* loop in the portion */
	} /* all events loop */
}

static int usf_start_tx(struct usf_xx_type *usf_xx)
{
	int rc = q6usm_run(usf_xx->usc, 0, 0, 0);

	pr_debug("%s: tx: q6usm_run; rc=%d\n", __func__, rc);
	if (!rc) {
		if (usf_xx->buffer_count >= USM_MIN_BUF_CNT) {
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
		} else
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

static int __usf_set_us_detection(struct usf_type *usf,
				  struct us_detect_info_type *detect_info)
{
	uint32_t timeout = 0;
	struct usm_session_cmd_detect_info *p_allocated_memory = NULL;
	struct usm_session_cmd_detect_info usm_detect_info;
	struct usm_session_cmd_detect_info *p_usm_detect_info =
						&usm_detect_info;
	uint32_t detect_info_size = sizeof(struct usm_session_cmd_detect_info);
	struct usf_xx_type *usf_xx =  &usf->usf_tx;
	int rc = 0;

	if (detect_info->us_detector != US_DETECT_FW) {
		pr_err("%s: unsupported detector: %d\n",
			__func__, detect_info->us_detector);
		return -EINVAL;
	}

	if ((detect_info->params_data_size != 0) &&
	    (detect_info->params_data != NULL)) {
		uint8_t *p_data = NULL;

		detect_info_size += detect_info->params_data_size;
		 p_allocated_memory = kzalloc(detect_info_size, GFP_KERNEL);
		if (p_allocated_memory == NULL) {
			pr_err("%s: detect_info[%d] allocation failed\n",
			       __func__, detect_info_size);
			return -ENOMEM;
		}
		p_usm_detect_info = p_allocated_memory;
		p_data = (uint8_t *)p_usm_detect_info +
			sizeof(struct usm_session_cmd_detect_info);

		rc = copy_from_user(p_data,
			(uint8_t __user *)(detect_info->params_data),
			detect_info->params_data_size);
		if (rc) {
			pr_err("%s: copy params from user; rc=%d\n",
				__func__, rc);
			kfree(p_allocated_memory);
			return -EFAULT;
		}
		p_usm_detect_info->algorithm_cfg_size =
				detect_info->params_data_size;
	} else
		usm_detect_info.algorithm_cfg_size = 0;

	p_usm_detect_info->detect_mode = detect_info->us_detect_mode;
	p_usm_detect_info->skip_interval = detect_info->skip_time;

	usf_xx->us_detect_type = USF_US_DETECT_UNDEF;

	rc = q6usm_set_us_detection(usf_xx->usc,
				    p_usm_detect_info,
				    detect_info_size);
	if (rc || (detect_info->detect_timeout == USF_NO_WAIT_TIMEOUT)) {
		kfree(p_allocated_memory);
		return rc;
	}

	/* Get US detection result */
	if (detect_info->detect_timeout == USF_INFINITIVE_TIMEOUT) {
		rc = wait_event_interruptible(usf_xx->wait,
						(usf_xx->us_detect_type !=
						USF_US_DETECT_UNDEF) ||
						(usf_xx->usf_state ==
						USF_ADSP_RESTART_STATE));
	} else {
		if (detect_info->detect_timeout == USF_DEFAULT_TIMEOUT)
			timeout = USF_TIMEOUT_JIFFIES;
		else
			timeout = detect_info->detect_timeout * HZ;
	}
	rc = wait_event_interruptible_timeout(usf_xx->wait,
					(usf_xx->us_detect_type !=
					USF_US_DETECT_UNDEF) ||
					(usf_xx->usf_state ==
					USF_ADSP_RESTART_STATE), timeout);

	/* In the case of aDSP restart, "no US" is assumed */
	if (usf_xx->usf_state == USF_ADSP_RESTART_STATE)
		rc = -EFAULT;

	/* In the case of timeout, "no US" is assumed */
	if (rc < 0)
		pr_err("%s: Getting US detection failed rc[%d]\n",
		       __func__, rc);
	else {
		usf->usf_rx.us_detect_type = usf->usf_tx.us_detect_type;
		detect_info->is_us =
			(usf_xx->us_detect_type == USF_US_DETECT_YES);
	}

	kfree(p_allocated_memory);

	return rc;
} /* __usf_set_us_detection */

static int usf_set_us_detection(struct usf_type *usf, unsigned long arg)
{
	struct us_detect_info_type detect_info;

	int rc = copy_from_user(&detect_info,
				(struct us_detect_info_type __user *) arg,
				sizeof(detect_info));

	if (rc) {
		pr_err("%s: copy detect_info from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	if (detect_info.params_data_size > USF_MAX_USER_BUF_SIZE) {
		pr_err("%s: user buffer size exceeds maximum\n",
			__func__);
		return -EFAULT;
	}

	rc = __usf_set_us_detection(usf, &detect_info);
	if (rc < 0) {
		pr_err("%s: set us detection failed; rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = copy_to_user((void __user *)arg,
			  &detect_info,
			  sizeof(detect_info));
	if (rc) {
		pr_err("%s: copy detect_info to user; rc=%d\n",
			__func__, rc);
		rc = -EFAULT;
	}

	return rc;
} /* usf_set_us_detection */

static int __usf_set_tx_info(struct usf_type *usf,
			     struct us_tx_info_type *config_tx)
{
	struct usf_xx_type *usf_xx =  &usf->usf_tx;
	int rc = 0;

	usf_xx->new_region = USM_UNDEF_TOKEN;
	usf_xx->prev_region = USM_UNDEF_TOKEN;
	usf_xx->cb = usf_tx_cb;

	init_waitqueue_head(&usf_xx->wait);

	if (config_tx->us_xx_info.client_name != NULL) {
		int res = strncpy_from_user(
			usf_xx->client_name,
			(char __user *)(config_tx->us_xx_info.client_name),
			sizeof(usf_xx->client_name)-1);
		if (res < 0) {
			pr_err("%s: get client name failed\n",
			       __func__);
			return -EINVAL;
		}
	}

	rc = config_xx(usf_xx, &(config_tx->us_xx_info));
	if (rc)
		return rc;

	rc = q6usm_open_read(usf_xx->usc,
			     usf_xx->encdec_cfg.format_id);
	if (rc)
		return rc;

	rc = q6usm_us_client_buf_alloc(OUT, usf_xx->usc,
				       usf_xx->buffer_size,
				       usf_xx->buffer_count);
	if (rc) {
		(void)q6usm_cmd(usf_xx->usc, CMD_CLOSE);
		return rc;
	}

	rc = q6usm_us_param_buf_alloc(OUT, usf_xx->usc,
			config_tx->us_xx_info.max_get_set_param_buf_size);
	if (rc) {
		(void)q6usm_cmd(usf_xx->usc, CMD_CLOSE);
		return rc;
	}

	rc = q6usm_enc_cfg_blk(usf_xx->usc,
			       &usf_xx->encdec_cfg);
	if (!rc &&
	     (config_tx->input_info.event_types != USF_NO_EVENT)) {
		rc = register_input_device(usf,
					   &(config_tx->input_info));
	}

	if (rc)
		(void)q6usm_cmd(usf_xx->usc, CMD_CLOSE);
	else
		usf_xx->usf_state = USF_CONFIGURED_STATE;

	return rc;
} /* __usf_set_tx_info */

static int usf_set_tx_info(struct usf_type *usf, unsigned long arg)
{
	struct us_tx_info_type config_tx;

	int rc = copy_from_user(&config_tx,
			    (struct us_tx_info_type __user *) arg,
			    sizeof(config_tx));

	if (rc) {
		pr_err("%s: copy config_tx from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	if (config_tx.us_xx_info.params_data_size > USF_MAX_USER_BUF_SIZE) {
		pr_err("%s: user buffer size exceeds maximum\n",
			__func__);
		return -EFAULT;
	}

	return __usf_set_tx_info(usf, &config_tx);
} /* usf_set_tx_info */

static int __usf_set_rx_info(struct usf_type *usf,
			     struct us_rx_info_type *config_rx)
{
	struct usf_xx_type *usf_xx =  &usf->usf_rx;
	int rc = 0;

	usf_xx->new_region = USM_UNDEF_TOKEN;
	usf_xx->prev_region = USM_UNDEF_TOKEN;

	usf_xx->cb = usf_rx_cb;

	rc = config_xx(usf_xx, &(config_rx->us_xx_info));
	if (rc)
		return rc;

	rc = q6usm_open_write(usf_xx->usc,
			      usf_xx->encdec_cfg.format_id);
	if (rc)
		return rc;

	rc = q6usm_us_client_buf_alloc(
				IN,
				usf_xx->usc,
				usf_xx->buffer_size,
				usf_xx->buffer_count);
	if (rc) {
		(void)q6usm_cmd(usf_xx->usc, CMD_CLOSE);
		return rc;
	}

	rc = q6usm_us_param_buf_alloc(IN, usf_xx->usc,
			config_rx->us_xx_info.max_get_set_param_buf_size);
	if (rc) {
		(void)q6usm_cmd(usf_xx->usc, CMD_CLOSE);
		return rc;
	}

	rc = q6usm_dec_cfg_blk(usf_xx->usc,
			       &usf_xx->encdec_cfg);
	if (rc)
		(void)q6usm_cmd(usf_xx->usc, CMD_CLOSE);
	else {
		init_waitqueue_head(&usf_xx->wait);
		usf_xx->usf_state = USF_CONFIGURED_STATE;
	}

	return rc;
} /* __usf_set_rx_info */

static int usf_set_rx_info(struct usf_type *usf, unsigned long arg)
{
	struct us_rx_info_type config_rx;

	int rc = copy_from_user(&config_rx,
				(struct us_rx_info_type __user *) arg,
				sizeof(config_rx));

	if (rc) {
		pr_err("%s: copy config_rx from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	if (config_rx.us_xx_info.params_data_size > USF_MAX_USER_BUF_SIZE) {
		pr_err("%s: user buffer size exceeds maximum\n",
			__func__);
		return -EFAULT;
	}

	return __usf_set_rx_info(usf, &config_rx);
} /* usf_set_rx_info */

static int __usf_get_tx_update(struct usf_type *usf,
			struct us_tx_update_info_type *upd_tx_info)
{
	unsigned long prev_jiffies = 0;
	uint32_t timeout = 0;
	struct usf_xx_type *usf_xx =  &usf->usf_tx;
	int rc = 0;

	if (!usf_xx->user_upd_info_na) {
		usf_set_event_filters(usf, upd_tx_info->event_filters);
		handle_input_event(usf,
				   upd_tx_info->event_counter,
				   upd_tx_info->event);

		/* Release available regions */
		rc = q6usm_read(usf_xx->usc,
				upd_tx_info->free_region);
		if (rc)
			return rc;
	} else
		usf_xx->user_upd_info_na = 0;

	/* Get data ready regions */
	if (upd_tx_info->timeout == USF_INFINITIVE_TIMEOUT) {
		rc = wait_event_interruptible(usf_xx->wait,
			   (usf_xx->prev_region !=
			    usf_xx->new_region) ||
			   (usf_xx->usf_state !=
			    USF_WORK_STATE));
	} else {
		if (upd_tx_info->timeout == USF_NO_WAIT_TIMEOUT)
			rc = (usf_xx->prev_region != usf_xx->new_region);
		else {
			prev_jiffies = jiffies;
			if (upd_tx_info->timeout == USF_DEFAULT_TIMEOUT) {
				timeout = USF_TIMEOUT_JIFFIES;
				rc = wait_event_timeout(
						usf_xx->wait,
						(usf_xx->prev_region !=
						 usf_xx->new_region) ||
						(usf_xx->usf_state !=
						 USF_WORK_STATE),
						timeout);
			} else {
				timeout = upd_tx_info->timeout * HZ;
				rc = wait_event_interruptible_timeout(
						usf_xx->wait,
						(usf_xx->prev_region !=
						 usf_xx->new_region) ||
						(usf_xx->usf_state !=
						 USF_WORK_STATE),
						timeout);
			}
		}
		if (!rc) {
			pr_debug("%s: timeout. prev_j=%lu; j=%lu\n",
				__func__, prev_jiffies, jiffies);
			pr_debug("%s: timeout. prev=%d; new=%d\n",
				__func__, usf_xx->prev_region,
				usf_xx->new_region);
			pr_debug("%s: timeout. free_region=%d;\n",
				__func__, upd_tx_info->free_region);
			if (usf_xx->prev_region ==
			    usf_xx->new_region) {
				pr_err("%s:read data: timeout\n",
				       __func__);
				return -ETIME;
			}
		}
	}

	if ((usf_xx->usf_state != USF_WORK_STATE) ||
	    (rc == -ERESTARTSYS)) {
		pr_err("%s: Get ready region failure; state[%d]; rc[%d]\n",
		       __func__, usf_xx->usf_state, rc);
		return -EINTR;
	}

	upd_tx_info->ready_region = usf_xx->new_region;
	usf_xx->prev_region = upd_tx_info->ready_region;

	if (upd_tx_info->ready_region == USM_WRONG_TOKEN) {
		pr_err("%s: TX path corrupted; prev=%d\n",
		       __func__, usf_xx->prev_region);
		return -EIO;
	}

	return rc;
} /* __usf_get_tx_update */

static int usf_get_tx_update(struct usf_type *usf, unsigned long arg)
{
	struct us_tx_update_info_type upd_tx_info;

	int rc = copy_from_user(&upd_tx_info,
				(struct us_tx_update_info_type __user *) arg,
				sizeof(upd_tx_info));

	if (rc < 0) {
		pr_err("%s: copy upd_tx_info from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	rc = __usf_get_tx_update(usf, &upd_tx_info);
	if (rc < 0) {
		pr_err("%s: get tx update failed; rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = copy_to_user((void __user *)arg,
			  &upd_tx_info,
			  sizeof(upd_tx_info));
	if (rc) {
		pr_err("%s: copy upd_tx_info to user; rc=%d\n",
			__func__, rc);
		rc = -EFAULT;
	}

	return rc;
} /* usf_get_tx_update */

static int __usf_set_rx_update(struct usf_xx_type *usf_xx,
			       struct us_rx_update_info_type *upd_rx_info)
{
	int rc = 0;

	/* Send available data regions */
	if (upd_rx_info->ready_region !=
	    usf_xx->buffer_count) {
		rc = q6usm_write(
			usf_xx->usc,
			upd_rx_info->ready_region);
		if (rc)
			return rc;
	}

	/* Get free regions */
	rc = wait_event_timeout(
		usf_xx->wait,
		!q6usm_is_write_buf_full(
			usf_xx->usc,
			&(upd_rx_info->free_region)) ||
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
		}
	}

	return rc;
} /* __usf_set_rx_update */

static int usf_set_rx_update(struct usf_xx_type *usf_xx, unsigned long arg)
{
	struct us_rx_update_info_type upd_rx_info;

	int rc = copy_from_user(&upd_rx_info,
				(struct us_rx_update_info_type __user *) arg,
				sizeof(upd_rx_info));

	if (rc) {
		pr_err("%s: copy upd_rx_info from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	rc = __usf_set_rx_update(usf_xx, &upd_rx_info);
	if (rc < 0) {
		pr_err("%s: set rx update failed; rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = copy_to_user((void __user *)arg,
			&upd_rx_info,
			sizeof(upd_rx_info));
	if (rc) {
		pr_err("%s: copy rx_info to user; rc=%d\n",
			__func__, rc);
		rc = -EFAULT;
	}

	return rc;
} /* usf_set_rx_update */

static void usf_release_input(struct usf_type *usf)
{
	uint16_t ind = 0;

	usf_unregister_conflicting_events(
					usf->conflicting_event_types);
	usf->conflicting_event_types = 0;
	for (ind = 0; ind < USF_MAX_EVENT_IND; ++ind) {
		if (usf->input_ifs[ind] == NULL)
			continue;
		input_unregister_device(usf->input_ifs[ind]);
		usf->input_ifs[ind] = NULL;
		pr_debug("%s input_unregister_device[%s]\n",
			 __func__,
			 s_usf_input_devs[ind].input_dev_name);
	}
} /* usf_release_input */

static int usf_stop_tx(struct usf_type *usf)
{
	struct usf_xx_type *usf_xx =  &usf->usf_tx;

	usf_release_input(usf);
	usf_disable(usf_xx);

	return 0;
} /* usf_stop_tx */

static int __usf_get_version(struct us_version_info_type *version_info)
{
	int rc = 0;

	if (version_info->buf_size < sizeof(DRV_VERSION)) {
		pr_err("%s: buf_size (%d) < version string size (%zu)\n",
			__func__, version_info->buf_size, sizeof(DRV_VERSION));
		return -EINVAL;
	}

	rc = copy_to_user((void __user *)(version_info->pbuf),
			  DRV_VERSION,
			  sizeof(DRV_VERSION));
	if (rc) {
		pr_err("%s: copy to version_info.pbuf; rc=%d\n",
			__func__, rc);
		rc = -EFAULT;
	}

	return rc;
} /* __usf_get_version */

static int usf_get_version(unsigned long arg)
{
	struct us_version_info_type version_info;

	int rc = copy_from_user(&version_info,
				(struct us_version_info_type __user *) arg,
				sizeof(version_info));

	if (rc) {
		pr_err("%s: copy version_info from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	rc = __usf_get_version(&version_info);
	if (rc < 0) {
		pr_err("%s: get version failed; rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = copy_to_user((void __user *)arg,
			  &version_info,
			  sizeof(version_info));
	if (rc) {
		pr_err("%s: copy version_info to user; rc=%d\n",
			__func__, rc);
		rc = -EFAULT;
	}

	return rc;
} /* usf_get_version */

static int __usf_set_stream_param(struct usf_xx_type *usf_xx,
				struct us_stream_param_type *set_stream_param,
				int dir)
{
	struct us_client *usc = usf_xx->usc;
	struct us_port_data *port;
	int rc = 0;

	if (usc == NULL) {
		pr_err("%s: usc is null\n",
			__func__);
		return -EFAULT;
	}

	port = &usc->port[dir];
	if (port == NULL) {
		pr_err("%s: port is null\n",
			__func__);
		return -EFAULT;
	}

	if (port->param_buf == NULL) {
		pr_err("%s: parameter buffer is null\n",
			__func__);
		return -EFAULT;
	}

	if (set_stream_param->buf_size > port->param_buf_size) {
		pr_err("%s: buf_size (%d) > maximum buf size (%d)\n",
			__func__, set_stream_param->buf_size,
			port->param_buf_size);
		return -EINVAL;
	}

	if (set_stream_param->buf_size == 0) {
		pr_err("%s: buf_size is 0\n", __func__);
		return -EINVAL;
	}

	rc = copy_from_user(port->param_buf,
			(uint8_t __user *) set_stream_param->pbuf,
			set_stream_param->buf_size);
	if (rc) {
		pr_err("%s: copy param buf from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	rc = q6usm_set_us_stream_param(dir, usc, set_stream_param->module_id,
					set_stream_param->param_id,
					set_stream_param->buf_size);
	if (rc) {
		pr_err("%s: q6usm_set_us_stream_param failed; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	return rc;
}

static int usf_set_stream_param(struct usf_xx_type *usf_xx,
				unsigned long arg, int dir)
{
	struct us_stream_param_type set_stream_param;
	int rc = 0;

	rc = copy_from_user(&set_stream_param,
			(struct us_stream_param_type __user *) arg,
			sizeof(set_stream_param));

	if (rc) {
		pr_err("%s: copy set_stream_param from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	return __usf_set_stream_param(usf_xx, &set_stream_param, dir);
} /* usf_set_stream_param */

static int __usf_get_stream_param(struct usf_xx_type *usf_xx,
				struct us_stream_param_type *get_stream_param,
				int dir)
{
	struct us_client *usc = usf_xx->usc;
	struct us_port_data *port;
	int rc = 0;

	if (usc == NULL) {
		pr_err("%s: us_client is null\n",
			__func__);
		return -EFAULT;
	}

	port = &usc->port[dir];

	if (port->param_buf == NULL) {
		pr_err("%s: parameter buffer is null\n",
			__func__);
		return -EFAULT;
	}

	if (get_stream_param->buf_size > port->param_buf_size) {
		pr_err("%s: buf_size (%d) > maximum buf size (%d)\n",
			__func__, get_stream_param->buf_size,
			port->param_buf_size);
		return -EINVAL;
	}

	if (get_stream_param->buf_size == 0) {
		pr_err("%s: buf_size is 0\n", __func__);
		return -EINVAL;
	}

	rc = q6usm_get_us_stream_param(dir, usc, get_stream_param->module_id,
					get_stream_param->param_id,
					get_stream_param->buf_size);
	if (rc) {
		pr_err("%s: q6usm_get_us_stream_param failed; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	rc = copy_to_user((uint8_t __user *) get_stream_param->pbuf,
			port->param_buf,
			get_stream_param->buf_size);
	if (rc) {
		pr_err("%s: copy param buf to user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	return rc;
}

static int usf_get_stream_param(struct usf_xx_type *usf_xx,
				unsigned long arg, int dir)
{
	struct us_stream_param_type get_stream_param;
	int rc = 0;

	rc = copy_from_user(&get_stream_param,
			(struct us_stream_param_type __user *) arg,
			sizeof(get_stream_param));

	if (rc) {
		pr_err("%s: copy get_stream_param from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	return __usf_get_stream_param(usf_xx, &get_stream_param, dir);
} /* usf_get_stream_param */

static long __usf_ioctl(struct usf_type *usf,
		unsigned int cmd,
		unsigned long arg)
{

	int rc = 0;
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
		if ((usf_xx->usf_state == USF_WORK_STATE)
			|| (usf_xx->usf_state == USF_ADSP_RESTART_STATE))
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
		if ((usf_xx->usf_state == USF_WORK_STATE)
			|| (usf_xx->usf_state == USF_ADSP_RESTART_STATE))
			usf_disable(usf_xx);
		else {
			pr_err("%s: stop_rx: wrong state[%d]\n",
			       __func__,
			       usf_xx->usf_state);
			return -EBADFD;
		}
		break;
	} /* US_STOP_RX */

	case US_SET_DETECTION: {
		struct usf_xx_type *usf_xx = &usf->usf_tx;

		if (usf_xx->usf_state == USF_WORK_STATE)
			rc = usf_set_us_detection(usf, arg);
		else {
			pr_err("%s: set us detection: wrong state[%d]\n",
			       __func__,
			       usf_xx->usf_state);
			rc = -EBADFD;
		}
		break;
	} /* US_SET_DETECTION */

	case US_GET_VERSION: {
		rc = usf_get_version(arg);
		break;
	} /* US_GET_VERSION */

	case US_SET_TX_STREAM_PARAM: {
		rc = usf_set_stream_param(&usf->usf_tx, arg, OUT);
		break;
	} /* US_SET_TX_STREAM_PARAM */

	case US_GET_TX_STREAM_PARAM: {
		rc = usf_get_stream_param(&usf->usf_tx, arg, OUT);
		break;
	} /* US_GET_TX_STREAM_PARAM */

	case US_SET_RX_STREAM_PARAM: {
		rc = usf_set_stream_param(&usf->usf_rx, arg, IN);
		break;
	} /* US_SET_RX_STREAM_PARAM */

	case US_GET_RX_STREAM_PARAM: {
		rc = usf_get_stream_param(&usf->usf_rx, arg, IN);
		break;
	} /* US_GET_RX_STREAM_PARAM */

	default:
		pr_err("%s: unsupported IOCTL command [%d]\n",
		       __func__,
		       cmd);
		rc = -ENOTTY;
		break;
	}

	if (rc &&
	    ((cmd == US_SET_TX_INFO) ||
	     (cmd == US_SET_RX_INFO)))
		release_xx(usf_xx);

	return rc;
} /* __usf_ioctl */

static long usf_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct usf_type *usf = file->private_data;
	int rc = 0;

	mutex_lock(&usf->mutex);
	rc = __usf_ioctl(usf, cmd, arg);
	mutex_unlock(&usf->mutex);

	return rc;
} /* usf_ioctl */

#ifdef CONFIG_COMPAT

#define US_SET_TX_INFO32   _IOW(USF_IOCTL_MAGIC, 0, \
				struct us_tx_info_type32)
#define US_GET_TX_UPDATE32 _IOWR(USF_IOCTL_MAGIC, 2, \
				struct us_tx_update_info_type32)
#define US_SET_RX_INFO32   _IOW(USF_IOCTL_MAGIC, 3, \
				struct us_rx_info_type32)
#define US_SET_RX_UPDATE32 _IOWR(USF_IOCTL_MAGIC, 4, \
				struct us_rx_update_info_type32)
#define US_SET_DETECTION32 _IOWR(USF_IOCTL_MAGIC, 8, \
				struct us_detect_info_type32)
#define US_GET_VERSION32  _IOWR(USF_IOCTL_MAGIC, 9, \
				struct us_version_info_type32)
#define US_SET_TX_STREAM_PARAM32   _IOW(USF_IOCTL_MAGIC, 10, \
				struct us_stream_param_type32)
#define US_GET_TX_STREAM_PARAM32  _IOWR(USF_IOCTL_MAGIC, 11, \
				struct us_stream_param_type32)
#define US_SET_RX_STREAM_PARAM32   _IOW(USF_IOCTL_MAGIC, 12, \
				struct us_stream_param_type32)
#define US_GET_RX_STREAM_PARAM32  _IOWR(USF_IOCTL_MAGIC, 13, \
				struct us_stream_param_type32)

/* Info structure common for TX and RX */
struct us_xx_info_type32 {
/* Input:  general info */
/* Name of the client - event calculator, ptr to char */
	const compat_uptr_t client_name;
/* Selected device identification, accepted in the kernel's CAD */
	uint32_t dev_id;
/* 0 - point_epos type; (e.g. 1 - gr_mmrd) */
	uint32_t stream_format;
/* Required sample rate in Hz */
	uint32_t sample_rate;
/* Size of a buffer (bytes) for US data transfer between the module and USF */
	uint32_t buf_size;
/* Number of the buffers for the US data transfer */
	uint16_t buf_num;
/* Number of the microphones (TX) or speakers(RX) */
	uint16_t port_cnt;
/* Microphones(TX) or speakers(RX) indexes in their enumeration */
	uint8_t  port_id[USF_MAX_PORT_NUM];
/* Bits per sample 16 or 32 */
	uint16_t bits_per_sample;
/* Input:  Transparent info for encoder in the LPASS */
/* Parameters data size in bytes */
	uint16_t params_data_size;
/* Pointer to the parameters, ptr to uint8_t */
	compat_uptr_t params_data;
/* Max size of buffer for get and set parameter */
	uint32_t max_get_set_param_buf_size;
};

struct us_tx_info_type32 {
/* Common info. This struct includes ptr and therefore the 32 version */
	struct us_xx_info_type32 us_xx_info;
/* Info specific for TX. This struct doesn't include long or ptr
 * and therefore no 32 version
 */
	struct us_input_info_type input_info;
};

struct us_tx_update_info_type32 {
/* Input  general: */
/* Number of calculated events */
	uint16_t event_counter;
/* Calculated events or NULL, ptr to struct usf_event_type */
	compat_uptr_t event;
/* Pointer (read index) to the end of available region */
/* in the shared US data memory */
	uint32_t free_region;
/* Time (sec) to wait for data or special values: */
/* USF_NO_WAIT_TIMEOUT, USF_INFINITIVE_TIMEOUT, USF_DEFAULT_TIMEOUT */
	uint32_t timeout;
/* Events (from conflicting devs) to be disabled/enabled */
	uint16_t event_filters;

/* Input  transparent data: */
/* Parameters size */
	uint16_t params_data_size;
/* Pointer to the parameters, ptr to uint8_t */
	compat_uptr_t params_data;
/* Output parameters: */
/* Pointer (write index) to the end of ready US data region */
/* in the shared memory */
	uint32_t ready_region;
};

struct us_rx_info_type32 {
	/* Common info */
	struct us_xx_info_type32 us_xx_info;
	/* Info specific for RX*/
};

struct us_rx_update_info_type32 {
/* Input  general: */
/* Pointer (write index) to the end of ready US data region */
/* in the shared memory */
	uint32_t ready_region;
/* Input  transparent data: */
/* Parameters size */
	uint16_t params_data_size;
/* pPointer to the parameters, ptr to uint8_t */
	compat_uptr_t params_data;
/* Output parameters: */
/* Pointer (read index) to the end of available region */
/* in the shared US data memory */
	uint32_t free_region;
};

struct us_detect_info_type32 {
/* US detection place (HW|FW) */
/* NA in the Active and OFF states */
	enum us_detect_place_enum us_detector;
/* US detection mode */
	enum us_detect_mode_enum  us_detect_mode;
/* US data dropped during this time (msec) */
	uint32_t skip_time;
/* Transparent data size */
	uint16_t params_data_size;
/* Pointer to the transparent data, ptr to uint8_t */
	compat_uptr_t params_data;
/* Time (sec) to wait for US presence event */
	uint32_t detect_timeout;
/* Out parameter: US presence */
	bool is_us;
};

struct us_version_info_type32 {
/* Size of memory for the version string */
	uint16_t buf_size;
/* Pointer to the memory for the version string, ptr to char */
	compat_uptr_t pbuf;
};

struct us_stream_param_type32 {
/* Id of module */
	uint32_t module_id;
/* Id of parameter */
	uint32_t param_id;
/* Size of memory of the parameter buffer */
	uint32_t buf_size;
/* Pointer to the memory of the parameter buffer */
	compat_uptr_t pbuf;
};

static void usf_compat_xx_info_type(struct us_xx_info_type32 *us_xx_info32,
				   struct us_xx_info_type *us_xx_info)
{
	int i = 0;

	us_xx_info->client_name = compat_ptr(us_xx_info32->client_name);
	us_xx_info->dev_id = us_xx_info32->dev_id;
	us_xx_info->stream_format = us_xx_info32->stream_format;
	us_xx_info->sample_rate = us_xx_info32->sample_rate;
	us_xx_info->buf_size = us_xx_info32->buf_size;
	us_xx_info->buf_num = us_xx_info32->buf_num;
	us_xx_info->port_cnt = us_xx_info32->port_cnt;
	for (i = 0; i < USF_MAX_PORT_NUM; i++)
		us_xx_info->port_id[i] = us_xx_info32->port_id[i];
	us_xx_info->bits_per_sample = us_xx_info32->bits_per_sample;
	us_xx_info->params_data_size = us_xx_info32->params_data_size;
	us_xx_info->params_data = compat_ptr(us_xx_info32->params_data);
	us_xx_info->max_get_set_param_buf_size =
			    us_xx_info32->max_get_set_param_buf_size;
}

static int usf_set_tx_info32(struct usf_type *usf, unsigned long arg)
{
	struct us_tx_info_type32 config_tx32;
	struct us_tx_info_type config_tx;

	int rc = copy_from_user(&config_tx32,
			    (struct us_tx_info_type32 __user *) arg,
			    sizeof(config_tx32));

	if (rc) {
		pr_err("%s: copy config_tx from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}
	memset(&config_tx, 0, sizeof(config_tx));
	usf_compat_xx_info_type(&(config_tx32.us_xx_info),
				&(config_tx.us_xx_info));
	config_tx.input_info = config_tx32.input_info;

	return __usf_set_tx_info(usf, &config_tx);
} /* usf_set_tx_info 32*/

static int usf_set_rx_info32(struct usf_type *usf, unsigned long arg)
{
	struct us_rx_info_type32 config_rx32;
	struct us_rx_info_type config_rx;

	int rc = copy_from_user(&config_rx32,
				(struct us_rx_info_type32 __user *) arg,
				sizeof(config_rx32));

	if (rc) {
		pr_err("%s: copy config_rx from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}
	memset(&config_rx, 0, sizeof(config_rx));
	usf_compat_xx_info_type(&(config_rx32.us_xx_info),
				&(config_rx.us_xx_info));

	return __usf_set_rx_info(usf, &config_rx);
} /* usf_set_rx_info32 */

static int usf_get_tx_update32(struct usf_type *usf, unsigned long arg)
{
	struct us_tx_update_info_type32 upd_tx_info32;
	struct us_tx_update_info_type upd_tx_info;

	int rc = copy_from_user(&upd_tx_info32,
				(struct us_tx_update_info_type32 __user *) arg,
				sizeof(upd_tx_info32));

	if (rc) {
		pr_err("%s: copy upd_tx_info32 from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	memset(&upd_tx_info, 0, sizeof(upd_tx_info));
	upd_tx_info.event_counter = upd_tx_info32.event_counter;
	upd_tx_info.event = compat_ptr(upd_tx_info32.event);
	upd_tx_info.free_region = upd_tx_info32.free_region;
	upd_tx_info.timeout = upd_tx_info32.timeout;
	upd_tx_info.event_filters = upd_tx_info32.event_filters;
	upd_tx_info.params_data_size = upd_tx_info32.params_data_size;
	upd_tx_info.params_data = compat_ptr(upd_tx_info32.params_data);
	upd_tx_info.ready_region = upd_tx_info32.ready_region;

	rc = __usf_get_tx_update(usf, &upd_tx_info);
	if (rc < 0) {
		pr_err("%s: get tx update failed; rc=%d\n",
			__func__, rc);
		return rc;
	}

	/* Update only the fields that were changed */
	upd_tx_info32.ready_region = upd_tx_info.ready_region;

	rc = copy_to_user((void __user *)arg, &upd_tx_info32,
			  sizeof(upd_tx_info32));
	if (rc) {
		pr_err("%s: copy upd_tx_info32 to user; rc=%d\n",
			__func__, rc);
		rc = -EFAULT;
	}

	return rc;
} /* usf_get_tx_update */

static int usf_set_rx_update32(struct usf_xx_type *usf_xx, unsigned long arg)
{
	struct us_rx_update_info_type32 upd_rx_info32;
	struct us_rx_update_info_type upd_rx_info;

	int rc = copy_from_user(&upd_rx_info32,
				(struct us_rx_update_info_type32 __user *) arg,
				sizeof(upd_rx_info32));

	if (rc) {
		pr_err("%s: copy upd_rx_info32 from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	memset(&upd_rx_info, 0, sizeof(upd_rx_info));
	upd_rx_info.ready_region = upd_rx_info32.ready_region;
	upd_rx_info.params_data_size = upd_rx_info32.params_data_size;
	upd_rx_info.params_data = compat_ptr(upd_rx_info32.params_data);
	upd_rx_info.free_region = upd_rx_info32.free_region;

	rc = __usf_set_rx_update(usf_xx, &upd_rx_info);
	if (rc < 0) {
		pr_err("%s: set rx update failed; rc=%d\n",
			__func__, rc);
		return rc;
	}

	/* Update only the fields that were changed */
	upd_rx_info32.free_region = upd_rx_info.free_region;

	rc = copy_to_user((void __user *)arg,
			&upd_rx_info32,
			sizeof(upd_rx_info32));
	if (rc) {
		pr_err("%s: copy rx_info32 to user; rc=%d\n",
			__func__, rc);
		rc = -EFAULT;
	}

	return rc;
} /* usf_set_rx_update32 */

static int usf_set_us_detection32(struct usf_type *usf, unsigned long arg)
{
	struct us_detect_info_type32 detect_info32;
	struct us_detect_info_type detect_info;

	int rc = copy_from_user(&detect_info32,
				(struct us_detect_info_type32 __user *) arg,
				sizeof(detect_info32));

	if (rc) {
		pr_err("%s: copy detect_info32 from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	if (detect_info32.params_data_size > USF_MAX_USER_BUF_SIZE) {
		pr_err("%s: user buffer size exceeds maximum\n",
			__func__);
		return -EFAULT;
	}

	memset(&detect_info, 0, sizeof(detect_info));
	detect_info.us_detector = detect_info32.us_detector;
	detect_info.us_detect_mode = detect_info32.us_detect_mode;
	detect_info.skip_time = detect_info32.skip_time;
	detect_info.params_data_size = detect_info32.params_data_size;
	detect_info.params_data = compat_ptr(detect_info32.params_data);
	detect_info.detect_timeout = detect_info32.detect_timeout;
	detect_info.is_us = detect_info32.is_us;

	rc = __usf_set_us_detection(usf, &detect_info);
	if (rc < 0) {
		pr_err("%s: set us detection failed; rc=%d\n",
			__func__, rc);
		return rc;
	}

	/* Update only the fields that were changed */
	detect_info32.is_us = detect_info.is_us;

	rc = copy_to_user((void __user *)arg,
			  &detect_info32,
			  sizeof(detect_info32));
	if (rc) {
		pr_err("%s: copy detect_info32 to user; rc=%d\n",
			__func__, rc);
		rc = -EFAULT;
	}

	return rc;
} /* usf_set_us_detection32 */

static int usf_get_version32(unsigned long arg)
{
	struct us_version_info_type32 version_info32;
	struct us_version_info_type version_info;

	int rc = copy_from_user(&version_info32,
				(struct us_version_info_type32 __user *) arg,
				sizeof(version_info32));

	if (rc) {
		pr_err("%s: copy version_info32 from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	memset(&version_info, 0, sizeof(version_info));
	version_info.buf_size = version_info32.buf_size;
	version_info.pbuf = compat_ptr(version_info32.pbuf);

	rc = __usf_get_version(&version_info);
	if (rc < 0) {
		pr_err("%s: get version failed; rc=%d\n",
			__func__, rc);
		return rc;
	}

	/* None of the fields were changed */

	rc = copy_to_user((void __user *)arg,
			  &version_info32,
			  sizeof(version_info32));
	if (rc) {
		pr_err("%s: copy version_info32 to user; rc=%d\n",
			__func__, rc);
		rc = -EFAULT;
	}

	return rc;
} /* usf_get_version32 */

static int usf_set_stream_param32(struct usf_xx_type *usf_xx,
				unsigned long arg, int dir)
{
	struct us_stream_param_type32 set_stream_param32;
	struct us_stream_param_type set_stream_param;
	int rc = 0;

	rc = copy_from_user(&set_stream_param32,
			(struct us_stream_param_type32 __user *) arg,
			sizeof(set_stream_param32));

	if (rc) {
		pr_err("%s: copy set_stream_param from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	memset(&set_stream_param, 0, sizeof(set_stream_param));
	set_stream_param.module_id = set_stream_param32.module_id;
	set_stream_param.param_id = set_stream_param32.param_id;
	set_stream_param.buf_size = set_stream_param32.buf_size;
	set_stream_param.pbuf = compat_ptr(set_stream_param32.pbuf);

	return __usf_set_stream_param(usf_xx, &set_stream_param, dir);
} /* usf_set_stream_param32 */

static int usf_get_stream_param32(struct usf_xx_type *usf_xx,
				unsigned long arg, int dir)
{
	struct us_stream_param_type32 get_stream_param32;
	struct us_stream_param_type get_stream_param;
	int rc = 0;

	rc = copy_from_user(&get_stream_param32,
			(struct us_stream_param_type32 __user *) arg,
			sizeof(get_stream_param32));

	if (rc) {
		pr_err("%s: copy get_stream_param from user; rc=%d\n",
			__func__, rc);
		return -EFAULT;
	}

	memset(&get_stream_param, 0, sizeof(get_stream_param));
	get_stream_param.module_id = get_stream_param32.module_id;
	get_stream_param.param_id = get_stream_param32.param_id;
	get_stream_param.buf_size = get_stream_param32.buf_size;
	get_stream_param.pbuf = compat_ptr(get_stream_param32.pbuf);

	return __usf_get_stream_param(usf_xx, &get_stream_param, dir);
} /* usf_get_stream_param32 */

static long __usf_compat_ioctl(struct usf_type *usf,
			     unsigned int cmd,
			     unsigned long arg)
{
	int rc = 0;
	struct usf_xx_type *usf_xx = NULL;

	switch (cmd) {
	case US_START_TX:
	case US_START_RX:
	case US_STOP_TX:
	case US_STOP_RX: {
		return __usf_ioctl(usf, cmd, arg);
	}

	case US_SET_TX_INFO32: {
		usf_xx = &usf->usf_tx;
		if (usf_xx->usf_state == USF_OPENED_STATE)
			rc = usf_set_tx_info32(usf, arg);
		else {
			pr_err("%s: set_tx_info32: wrong state[%d]\n",
			       __func__,
			       usf_xx->usf_state);
			return -EBADFD;
		}

		break;
	} /* US_SET_TX_INFO32 */

	case US_SET_RX_INFO32: {
		usf_xx = &usf->usf_rx;
		if (usf_xx->usf_state == USF_OPENED_STATE)
			rc = usf_set_rx_info32(usf, arg);
		else {
			pr_err("%s: set_rx_info32: wrong state[%d]\n",
				__func__,
				usf_xx->usf_state);
			return -EBADFD;
		}

		break;
	} /* US_SET_RX_INFO32 */

	case US_GET_TX_UPDATE32: {
		struct usf_xx_type *usf_xx = &usf->usf_tx;

		if (usf_xx->usf_state == USF_WORK_STATE)
			rc = usf_get_tx_update32(usf, arg);
		else {
			pr_err("%s: get_tx_update32: wrong state[%d]\n",
			       __func__,
			       usf_xx->usf_state);
			rc = -EBADFD;
		}
		break;
	} /* US_GET_TX_UPDATE32 */

	case US_SET_RX_UPDATE32: {
		struct usf_xx_type *usf_xx = &usf->usf_rx;

		if (usf_xx->usf_state == USF_WORK_STATE)
			rc = usf_set_rx_update32(usf_xx, arg);
		else {
			pr_err("%s: set_rx_update: wrong state[%d]\n",
			       __func__,
			       usf_xx->usf_state);
			rc = -EBADFD;
		}
		break;
	} /* US_SET_RX_UPDATE32 */

	case US_SET_DETECTION32: {
		struct usf_xx_type *usf_xx = &usf->usf_tx;

		if (usf_xx->usf_state == USF_WORK_STATE)
			rc = usf_set_us_detection32(usf, arg);
		else {
			pr_err("%s: set us detection: wrong state[%d]\n",
			       __func__,
			       usf_xx->usf_state);
			rc = -EBADFD;
		}
		break;
	} /* US_SET_DETECTION32 */

	case US_GET_VERSION32: {
		rc = usf_get_version32(arg);
		break;
	} /* US_GET_VERSION32 */

	case US_SET_TX_STREAM_PARAM32: {
		rc = usf_set_stream_param32(&usf->usf_tx, arg, OUT);
		break;
	} /* US_SET_TX_STREAM_PARAM32 */

	case US_GET_TX_STREAM_PARAM32: {
		rc = usf_get_stream_param32(&usf->usf_tx, arg, OUT);
		break;
	} /* US_GET_TX_STREAM_PARAM32 */

	case US_SET_RX_STREAM_PARAM32: {
		rc = usf_set_stream_param32(&usf->usf_rx, arg, IN);
		break;
	} /* US_SET_RX_STREAM_PARAM32 */

	case US_GET_RX_STREAM_PARAM32: {
		rc = usf_get_stream_param32(&usf->usf_rx, arg, IN);
		break;
	} /* US_GET_RX_STREAM_PARAM32 */

	default:
		pr_err("%s: unsupported IOCTL command [%d]\n",
		       __func__,
		       cmd);
		rc = -ENOTTY;
		break;
	}

	if (rc &&
	    ((cmd == US_SET_TX_INFO) ||
	     (cmd == US_SET_RX_INFO)))
		release_xx(usf_xx);

	return rc;
} /* __usf_compat_ioctl */

static long usf_compat_ioctl(struct file *file,
			     unsigned int cmd,
			     unsigned long arg)
{
	struct usf_type *usf = file->private_data;
	int rc = 0;

	mutex_lock(&usf->mutex);
	rc = __usf_compat_ioctl(usf, cmd, arg);
	mutex_unlock(&usf->mutex);

	return rc;
} /* usf_compat_ioctl */
#endif /* CONFIG_COMPAT */

static int usf_mmap(struct file *file, struct vm_area_struct *vms)
{
	struct usf_type *usf = file->private_data;
	int dir = OUT;
	struct usf_xx_type *usf_xx = &usf->usf_tx;
	int rc = 0;

	mutex_lock(&usf->mutex);
	if (vms->vm_flags & USF_VM_WRITE) { /* RX buf mapping */
		dir = IN;
		usf_xx = &usf->usf_rx;
	}
	rc = q6usm_get_virtual_address(dir, usf_xx->usc, vms);
	mutex_unlock(&usf->mutex);

	return rc;
}

static uint16_t add_opened_dev(int minor)
{
	uint16_t ind = 0;

	for (ind = 0; ind < MAX_DEVS_NUMBER; ++ind) {
		if (minor == atomic_cmpxchg(&s_opened_devs[ind], 0, minor)) {
			pr_err("%s: device %d is already opened\n",
			       __func__, minor);
			return USF_UNDEF_DEV_ID;
		} else {
			pr_debug("%s: device %d is added; ind=%d\n",
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
	if (usf == NULL)
		return -ENOMEM;

	wakeup_source_init(&usf_wakeup_source, "usf");

	file->private_data = usf;
	usf->dev_ind = dev_ind;

	usf->usf_tx.usf_state = USF_OPENED_STATE;
	usf->usf_rx.usf_state = USF_OPENED_STATE;

	usf->usf_tx.us_detect_type = USF_US_DETECT_UNDEF;
	usf->usf_rx.us_detect_type = USF_US_DETECT_UNDEF;

	mutex_init(&usf->mutex);

	pr_debug("%s:usf in open\n", __func__);
	return 0;
}

static int usf_release(struct inode *inode, struct file *file)
{
	struct usf_type *usf = file->private_data;

	pr_debug("%s: release entry\n", __func__);

	mutex_lock(&usf->mutex);
	usf_release_input(usf);

	usf_disable(&usf->usf_tx);
	usf_disable(&usf->usf_rx);

	atomic_set(&s_opened_devs[usf->dev_ind], 0);

	wakeup_source_trash(&usf_wakeup_source);
	mutex_unlock(&usf->mutex);
	mutex_destroy(&usf->mutex);
	kfree(usf);
	pr_debug("%s: release exit\n", __func__);
	return 0;
}

extern long usf_compat_ioctl(struct file *file,
			     unsigned int cmd,
			     unsigned long arg);

static const struct file_operations usf_fops = {
	.owner                  = THIS_MODULE,
	.open                   = usf_open,
	.release                = usf_release,
	.unlocked_ioctl = usf_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = usf_compat_ioctl,
#endif /* CONFIG_COMPAT */
	.mmap                   = usf_mmap,
};

static struct miscdevice usf_misc[MAX_DEVS_NUMBER] = {
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

	pr_debug("%s: USF SW version %s.\n", __func__, DRV_VERSION);
	pr_debug("%s: Max %d devs registration\n", __func__, MAX_DEVS_NUMBER);

	for (ind = 0; ind < MAX_DEVS_NUMBER; ++ind) {
		rc = misc_register(&usf_misc[ind]);
		if (rc) {
			pr_err("%s: misc_register() failed ind=%d; rc = %d\n",
			       __func__, ind, rc);
			break;
		}
	}
	if (!rc) q6usm_init();

	return rc;
}
module_init(usf_init);

static void __exit usf_exit(void)
{
        uint16_t ind = 0;

        for (ind = 0; ind < MAX_DEVS_NUMBER; ++ind)
		misc_deregister(&usf_misc[ind]);
}
module_exit(usf_exit);
MODULE_DESCRIPTION("Ultrasound framework driver");
MODULE_LICENSE("GPL v2");
