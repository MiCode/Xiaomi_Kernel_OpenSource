// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017 MediaTek Inc.
 */

#define DRIVER_AUTHOR   "jlguo <jlguo@via-telecom.com>"
#define DRIVER_DESC     "Rawbulk Gadget - transport data from CP to Gadget"
#define DRIVER_VERSION  "1.0.2"

#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/usb/composite.h>
#include "viatel_rawbulk.h"

#include <linux/module.h>
#include <linux/configfs.h>
#include "configfs.h"

/* int setdtr, data_connect; */
/* struct work_struct flow_control; */
/* struct work_struct dtr_status; */

#define function_to_rbf(f) container_of(f, struct rawbulk_function, function)
static int usb_state;

void simple_setup_complete(struct usb_ep *ep, struct usb_request *req)
{
	;			/* DO NOTHING */
}

#define FINISH_DISABLE 0
#define START_DISABLE  1

int rawbulk_function_setup(struct usb_function *f, const struct
			   usb_ctrlrequest * ctrl)
{
	struct rawbulk_function *fn = function_to_rbf(f);
	unsigned int setdtr = 0;
	unsigned int data_connect = 0;
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request *req = cdev->req;
	int value = -EOPNOTSUPP;
	u16 w_index = le16_to_cpu(ctrl->wIndex);
	u16 w_value = le16_to_cpu(ctrl->wValue);
	u16 w_length = le16_to_cpu(ctrl->wLength);

	C2K_NOTE("%s\n", __func__);


	if (ctrl->bRequest) {
		C2K_NOTE("ctrl->bRequestType = %0x  ctrl->bRequest = %0x\n",
			ctrl->bRequestType, ctrl->bRequest);
	}
	switch (ctrl->bRequest) {
	case 0x01:
		if (ctrl->bRequestType == (USB_DIR_OUT | USB_TYPE_VENDOR)) {
			/* set/clear DTR 0x40 */
			C2K_NOTE("setdtr = %d, wvalue =%d\n", setdtr, w_value);
			if (fn->activated) {
				setdtr = w_value & 0x01;
				/* schedule_work(&flow_control); */
				modem_dtr_set(setdtr, 0);
				modem_dcd_state();
			}
			value = 0;
		}
		break;
	case 0x02:
		if (ctrl->bRequestType == (USB_DIR_IN | USB_TYPE_VENDOR)) {
			/* DSR | CD109 0xC0 */
			/* schedule_work(&dtr_status); */
			data_connect = modem_dcd_state();
			/* modem_dtr_query(&data_connect, 0); */
			if (fn->activated) {
				if (data_connect && fn->enable) {
					*((unsigned char *)req->buf) = 0x3;
					C2K_NOTE("connect %d\n", data_connect);

				} else {
					*((unsigned char *)req->buf) = 0x2;
					C2K_NOTE("disconnect=%d, setdtr=%d\n",
					data_connect, setdtr);
				}
			} else	/* set CD CSR state to 0 if modem bypass not
				 * inactive
				 */
				*((unsigned char *)req->buf) = 0x0;
			value = 1;
		}
		break;
	case 0x03:
		if (ctrl->bRequestType == (USB_DIR_OUT | USB_TYPE_VENDOR)) {
			/* xcvr 0x40 */
			C2K_NOTE("CTRL SET XCVR 0x%02x\n", w_value);
			value = 0;
		}
		break;
	case 0x04:
		if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_VENDOR) {
			if (ctrl->bRequestType & USB_DIR_IN) {	/* 0xC0 */
				/* return ID */
				sprintf(req->buf, "CBP_8.2");
				value = 1;
			} else {	/* 0x40 */
				value = 0;
			}
		}
		break;
	case 0x05:
		if (ctrl->bRequestType == (USB_DIR_IN | USB_TYPE_VENDOR)) {
			/* connect status 0xC0 */
			C2K_NOTE("CTRL CONNECT STATUS\n");
			*((unsigned char *)req->buf) = 0x0;
			value = 1;
		}
		break;
	default:
		C2K_NOTE("invalid control req%02x.%02x v%04x i%04x l%d\n",
			 ctrl->bRequestType, ctrl->bRequest, w_value, w_index,
			w_length);
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		req->zero = 0;
		req->length = value;
		req->complete = simple_setup_complete;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			C2K_ERR("response err %d\n", value);
	}
	/* device either stalls (value < 0) or reports success */
	return value;

}

static void rawbulk_auto_reconnect(int transfer_id)
{
	int rc;
	struct rawbulk_function *fn = rawbulk_lookup_function(transfer_id);

	C2K_NOTE("%s\n", __func__);
	if (!fn || fn->autoreconn == 0)
		return;

	if (rawbulk_check_enable(fn) && fn->activated) {
		C2K_ERR("start %s again automatically.\n", fn->longname);
		rc = rawbulk_start_transactions(transfer_id, fn->nups,
						fn->ndowns, fn->upsz,
						fn->downsz);
		if (rc < 0)
			rawbulk_disable_function(fn);
	}
}

int rawbulk_function_bind(struct usb_configuration *c, struct
			  usb_function * f)
{
	int rc, ifnum;
	struct rawbulk_function *fn = function_to_rbf(f);
	struct usb_gadget *gadget = c->cdev->gadget;
	struct usb_ep *ep_out, *ep_in;

	C2K_ERR("%s\n", __func__);
	rc = usb_interface_id(c, f);
	if (rc < 0)
		return rc;
	ifnum = rc;

	fn->interface.bInterfaceNumber = cpu_to_le16(ifnum);

	if (fn->string_defs[0].id == 0) {
		rc = usb_string_id(c->cdev);
		if (rc < 0)
			return rc;
		fn->string_defs[0].id = rc;
		fn->interface.iInterface = rc;
	}

	ep_out = usb_ep_autoconfig(gadget, (struct usb_endpoint_descriptor *)
				   fn->fs_descs[BULKOUT_DESC]);
	if (!ep_out) {
		C2K_ERR("%s %d config ep_out error\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ep_in = usb_ep_autoconfig(gadget, (struct usb_endpoint_descriptor *)
				  fn->fs_descs[BULKIN_DESC]);
	if (!ep_in) {
		usb_ep_disable(ep_out);
		C2K_ERR("%s %d config ep_in error\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ep_out->driver_data = fn;
	ep_in->driver_data = fn;
	fn->bulk_out = ep_out;
	fn->bulk_in = ep_in;

	f->fs_descriptors = usb_copy_descriptors(fn->fs_descs);
	if (unlikely(!f->fs_descriptors))
		return -ENOMEM;

	if (gadget_is_dualspeed(gadget)) {
		fn->hs_bulkin_endpoint.bEndpointAddress =
				fn->fs_bulkin_endpoint.bEndpointAddress;
		fn->hs_bulkout_endpoint.bEndpointAddress =
				fn->fs_bulkout_endpoint.bEndpointAddress;
	}

	fn->cdev = c->cdev;
	fn->activated = 0;

	return rawbulk_bind_function(fn->transfer_id, f, ep_out, ep_in,
				rawbulk_auto_reconnect);

}

void rawbulk_function_unbind(struct usb_configuration *c, struct
			     usb_function * f)
{
	struct rawbulk_function *fn = function_to_rbf(f);

	C2K_NOTE("%s\n", __func__);

	/* cancel_work_sync(&fn->activator); */
	/* cancel_work_sync(&flow_control); */
	/* cancel_work_sync(&dtr_status); */
	rawbulk_unbind_function(fn->transfer_id);
}

static void do_activate(struct work_struct *data)
{
	struct rawbulk_function *fn = container_of(data,
					struct rawbulk_function, activator);
	int rc;
	struct usb_function *functionp = &(fn->function);

	C2K_ERR("%s usb state %s, <%d>\n", __func__,
		(fn->activated ? "connect" : "disconnect"), fn->transfer_id);
	if (fn->activated) {	/* enumerated */
		/* enabled endpoints */
		rc = config_ep_by_speed(fn->cdev->gadget, functionp,
					fn->bulk_out);
		if (rc < 0) {
			C2K_ERR("failed to config speed rawbulk %s %d\n",
			       fn->bulk_out->name, rc);
			return;
		}
		rc = usb_ep_enable(fn->bulk_out);
		if (rc < 0) {
			C2K_ERR("failed to enable rawbulk %s %d\n",
				fn->bulk_out->name, rc);
			return;
		}

		rc = config_ep_by_speed(fn->cdev->gadget, functionp,
					fn->bulk_in);
		if (rc < 0) {
			C2K_ERR("failed to config speed rawbulk %s %d\n",
			       fn->bulk_in->name, rc);
			return;
		}
		rc = usb_ep_enable(fn->bulk_in);
		if (rc < 0) {
			C2K_ERR("failed to enable rawbulk %s %d\n",
				fn->bulk_in->name, rc);
			usb_ep_disable(fn->bulk_out);
			return;
		}

		/* start rawbulk if enabled */
		if (rawbulk_check_enable(fn)) {
			__pm_stay_awake(fn->keep_awake);
			rc = rawbulk_start_transactions(fn->transfer_id,
							fn->nups, fn->ndowns,
							fn->upsz, fn->downsz);
			if (rc < 0)
				/* rawbulk_disable_function(fn); */
				C2K_ERR("%s: failed bypass, channel id = %d\n",
				       __func__, fn->transfer_id);
		}

	} else {		/* disconnect */
		if (rawbulk_check_enable(fn)) {
			if (fn->transfer_id == RAWBULK_TID_MODEM) {
				/* this in interrupt, but DTR need be set
				 * firstly then clear it
				 */
				modem_dtr_set(1, 1);
				modem_dtr_set(0, 1);
				modem_dtr_set(1, 1);
				modem_dcd_state();
			}

			rawbulk_stop_transactions(fn->transfer_id);
			/* keep the enable state, so we can enable again in
			 * next time
			 */
			/* set_enable_state(fn, 0); */
			__pm_relax(fn->keep_awake);
		}


		usb_ep_disable(fn->bulk_out);
		usb_ep_disable(fn->bulk_in);

		fn->bulk_out->driver_data = NULL;
		fn->bulk_in->driver_data = NULL;
	}
}

static void rawbulk_usb_state_set(int state)
{
	usb_state = state ? 1 : 0;
	C2K_ERR("%s usb_state = %d\n", __func__, usb_state);
}

int rawbulk_usb_state_check(void)
{
	return usb_state ? 1 : 0;
}
EXPORT_SYMBOL_GPL(rawbulk_usb_state_check);

static int rawbulk_function_setalt(struct usb_function *f, unsigned int intf,
				unsigned int alt)
{
	struct rawbulk_function *fn = function_to_rbf(f);

	C2K_NOTE("%s\n", __func__);
	fn->activated = 1;
	rawbulk_usb_state_set(fn->activated);
	schedule_work(&fn->activator);
	return 0;
}

static void rawbulk_function_disable(struct usb_function *f)
{
	struct rawbulk_function *fn = function_to_rbf(f);

	C2K_NOTE("%s, %s\n", __func__, f->name);

	fn->activated = 0;
	rawbulk_usb_state_set(fn->activated);
	schedule_work(&fn->activator);
}

int rawbulk_bind_config(struct usb_configuration *c, int transfer_id)
{

	int rc;
	struct rawbulk_function *fn = rawbulk_lookup_function(transfer_id);

	if (!fn)
		return -ENOMEM;

	C2K_ERR("add %s to config.\n", fn->longname);

	if (fn->string_defs[0].id == 0) {
		rc = usb_string_id(c->cdev);
		if (rc < 0)
			return rc;
		fn->string_defs[0].id = rc;
		fn->interface.iInterface = rc;
	}

	if (!fn->initialized) {
		fn->function.name = fn->longname;
		fn->function.setup = rawbulk_function_setup;
		fn->function.bind = rawbulk_function_bind;
		fn->function.unbind = rawbulk_function_unbind;
		fn->function.set_alt = rawbulk_function_setalt;
		fn->function.disable = rawbulk_function_disable;

		INIT_WORK(&fn->activator, do_activate);
		fn->initialized = 1;
	}

	rc = usb_add_function(c, &fn->function);
	if (rc < 0)
		C2K_ERR("%s - failed to config %d.\n", __func__, rc);

	return rc;
}

static struct rawbulk_instance *to_rawbulk_instance(struct config_item *item)
{
	return container_of(to_config_group(item), struct rawbulk_instance,
		func_inst.group);
}

static void rawbulk_attr_release(struct config_item *item)
{
	struct rawbulk_instance *fi_rawbulk = to_rawbulk_instance(item);

	usb_put_function_instance(&fi_rawbulk->func_inst);
}

static struct configfs_item_operations rawbulk_item_ops = {
	.release        = rawbulk_attr_release,
};

static struct config_item_type rawbulk_func_type = {
	.ct_item_ops    = &rawbulk_item_ops,
	.ct_owner       = THIS_MODULE,
};

static void rawbulk_free_inst(struct usb_function_instance *fi)
{
	struct rawbulk_instance *fi_rawbulk;

	fi_rawbulk = container_of(fi, struct rawbulk_instance, func_inst);
	kfree(fi_rawbulk);
}

static void rawbulk_free(struct usb_function *f)
{
	/*NO-OP: no function specific resource allocation in rawbulk_alloc*/
}

struct usb_function_instance *alloc_inst_rawbulk(int transfer_id)
{
	struct rawbulk_instance *fi_rawbulk;

	fi_rawbulk = kzalloc(sizeof(*fi_rawbulk), GFP_KERNEL);
	if (!fi_rawbulk)
		return ERR_PTR(-ENOMEM);
	fi_rawbulk->func_inst.free_func_inst = rawbulk_free_inst;

	config_group_init_type_name(&fi_rawbulk->func_inst.group,
				"", &rawbulk_func_type);

	return &fi_rawbulk->func_inst;
}

static struct usb_function *rawbulk_alloc(struct usb_function_instance *fi,
							int transfer_id)
{
	struct rawbulk_function *fn = rawbulk_lookup_function(transfer_id);

	if (!fn)
		return ERR_PTR(-ENOMEM);

	C2K_ERR("add %s to config.\n", fn->longname);

	if (!fn->initialized) {
		fn->function.name = fn->longname;
		fn->function.setup = rawbulk_function_setup;
		fn->function.bind = rawbulk_function_bind;
		fn->function.unbind = rawbulk_function_unbind;
		fn->function.set_alt = rawbulk_function_setalt;
		fn->function.disable = rawbulk_function_disable;
		fn->function.free_func = rawbulk_free;

		INIT_WORK(&fn->activator, do_activate);
		fn->initialized = 1;
	}

	return &fn->function;
}

static struct usb_function_instance *via_modem_alloc_inst(void)
{
	return alloc_inst_rawbulk(RAWBULK_TID_MODEM);
}

static struct usb_function *via_modem_alloc(struct usb_function_instance *fi)
{
	return rawbulk_alloc(fi, RAWBULK_TID_MODEM);
}

DECLARE_USB_FUNCTION_INIT(via_modem, via_modem_alloc_inst, via_modem_alloc);
MODULE_LICENSE("GPL");

static struct usb_function_instance *via_ets_alloc_inst(void)
{
	return alloc_inst_rawbulk(RAWBULK_TID_ETS);
}

static struct usb_function *via_ets_alloc(struct usb_function_instance *fi)
{
	return rawbulk_alloc(fi, RAWBULK_TID_ETS);
}

DECLARE_USB_FUNCTION_INIT(via_ets, via_ets_alloc_inst, via_ets_alloc);
MODULE_LICENSE("GPL");

static struct usb_function_instance *via_atc_alloc_inst(void)
{
	return alloc_inst_rawbulk(RAWBULK_TID_AT);
}

static struct usb_function *via_atc_alloc(struct usb_function_instance *fi)
{
	return rawbulk_alloc(fi, RAWBULK_TID_AT);
}

DECLARE_USB_FUNCTION_INIT(via_atc, via_atc_alloc_inst, via_atc_alloc);
MODULE_LICENSE("GPL");

static struct usb_function_instance *via_pcv_alloc_inst(void)
{
	return alloc_inst_rawbulk(RAWBULK_TID_PCV);
}

static struct usb_function *via_pcv_alloc(struct usb_function_instance *fi)
{
	return rawbulk_alloc(fi, RAWBULK_TID_PCV);
}

DECLARE_USB_FUNCTION_INIT(via_pcv, via_pcv_alloc_inst, via_pcv_alloc);
MODULE_LICENSE("GPL");

static struct usb_function_instance *via_gps_alloc_inst(void)
{
	return alloc_inst_rawbulk(RAWBULK_TID_GPS);
}

static struct usb_function *via_gps_alloc(struct usb_function_instance *fi)
{
	return rawbulk_alloc(fi, RAWBULK_TID_GPS);
}

DECLARE_USB_FUNCTION_INIT(via_gps, via_gps_alloc_inst, via_gps_alloc);
MODULE_LICENSE("GPL");
