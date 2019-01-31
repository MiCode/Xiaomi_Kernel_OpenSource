/*
 * Copyright (C) 2017 MediaTek Inc.
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


/* #define DEBUG */
/* #define VERBOSE_DEBUG */

#define DRIVER_AUTHOR   "Juelun Guo <jlguo@via-telecom.com>"
#define DRIVER_DESC     "Rawbulk Gadget - transport data from CP to Gadget"
#define DRIVER_VERSION  "1.0.2"
#define DRIVER_NAME     "usb_rawbulk"

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/version.h>

#include <linux/usb/composite.h>
#include "viatel_rawbulk.h"


#define C2K_TTY_USB_SKIP
#ifdef C2K_USB_UT
#include <linux/random.h>
#define UT_CMD 3
#define UT_CLR_ERR 4
#define SZ 4096
int ut_err;
#endif

/* sysfs attr idx assignment */
enum _attr_idx {
	ATTR_ENABLE = 0,	/** enable switch for Rawbulk */
#ifdef SUPPORT_LEGACY_CONTROL
	ATTR_ENABLE_C,		/** enable switch too, but for legacy */
#endif
	ATTR_AUTORECONN,	/** enable to rebind cp when it reconnect */
	ATTR_STATISTICS,	/** Rawbulk summary/statistics for one pipe */
	ATTR_NUPS,		/** upstream transfers count */
	ATTR_NDOWNS,		/** downstram transfers count */
	ATTR_UPSIZE,		/** upstream buffer for each transaction */
	ATTR_DOWNSIZE,		/** downstram buffer for each transaction */
	ATTR_DTR,		/** DTR control, only for Data-Call port */
};

struct rawbulk_function *prealloced_functions[_MAX_TID] = { NULL };

struct rawbulk_function *rawbulk_lookup_function(int transfer_id)
{
	C2K_DBG("%s\n", __func__);
	if (transfer_id >= 0 && transfer_id < _MAX_TID)
		return prealloced_functions[transfer_id];
	return NULL;
}
EXPORT_SYMBOL_GPL(rawbulk_lookup_function);


static inline int check_enable_state(struct rawbulk_function *fn)
{
	int enab;
	unsigned long flags;

	C2K_DBG("%s\n", __func__);
	spin_lock_irqsave(&fn->lock, flags);
	enab = fn->enable ? 1 : 0;
	C2K_DBG("enab(%d)\n", enab);
	spin_unlock_irqrestore(&fn->lock, flags);
	return enab;
}

int rawbulk_check_enable(struct rawbulk_function *fn)
{
	C2K_DBG("%s\n", __func__);
	return check_enable_state(fn);
}
EXPORT_SYMBOL_GPL(rawbulk_check_enable);


static inline void set_enable_state(struct rawbulk_function *fn, int enab)
{
	unsigned long flags;

	spin_lock_irqsave(&fn->lock, flags);
	fn->enable = !!enab;
	spin_unlock_irqrestore(&fn->lock, flags);
}

void rawbulk_disable_function(struct rawbulk_function *fn)
{
	C2K_NOTE("enable to 0\n");
	set_enable_state(fn, 0);
}
EXPORT_SYMBOL_GPL(rawbulk_disable_function);


#define port_to_rawbulk(p) container_of(p, struct rawbulk_function, port)
#define function_to_rawbulk(f) container_of(f, struct rawbulk_function,\
						function)


static void init_endpoint_desc(struct usb_endpoint_descriptor *epdesc, int in,
				int maxpacksize)
{
	struct usb_endpoint_descriptor template = {
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,
	};

	*epdesc = template;
	if (in)
		epdesc->bEndpointAddress = USB_DIR_IN;
	else
		epdesc->bEndpointAddress = USB_DIR_OUT;
	epdesc->wMaxPacketSize = cpu_to_le16(maxpacksize);
}

static void init_interface_desc(struct usb_interface_descriptor *ifdesc)
{
	struct usb_interface_descriptor template = {
		.bLength = USB_DT_INTERFACE_SIZE,
		.bDescriptorType = USB_DT_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 2,
		.bInterfaceClass = 0xff,	/* USB_CLASS_VENDOR_SPEC, */
		.bInterfaceSubClass = 0xff,
		.bInterfaceProtocol = 0xff,
		.iInterface = 0,
	};

	*ifdesc = template;
}

static ssize_t rawbulk_attr_show(struct device *dev, struct device_attribute
				 *attr, char *buf);
static ssize_t rawbulk_attr_store(struct device *dev, struct device_attribute
				  *attr, const char *buf, size_t count);

static inline void add_device_attr(struct rawbulk_function *fn, int n,
				const char *name, int mode)
{
	if (n < MAX_ATTRIBUTES) {
		sysfs_attr_init(&fn->attr[n].attr);
		fn->attr[n].attr.name = name;
		fn->attr[n].attr.mode = mode;
		fn->attr[n].show = rawbulk_attr_show;
		fn->attr[n].store = rawbulk_attr_store;
	}
}

static int which_attr(struct rawbulk_function *fn, struct device_attribute
		      *attr)
{
	int n;

	for (n = 0; n < fn->max_attrs; n++) {
		if (attr == &fn->attr[n])
			return n;
	}
	return -1;
}

static ssize_t rawbulk_attr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int n;
	int idx;
	int enab;
	struct rawbulk_function *fn;
	ssize_t count = 0;

	for (n = 0; n < _MAX_TID; n++) {
		fn = rawbulk_lookup_function(n);
		if (fn->dev == dev) {
			idx = which_attr(fn, attr);
			break;
		}
#ifdef SUPPORT_LEGACY_CONTROL
		if (!strcmp(attr->attr.name, fn->shortname)) {
			idx = ATTR_ENABLE_C;
			break;
		}
#endif
	}

	if (n == _MAX_TID)
		return 0;

	enab = check_enable_state(fn);
	switch (idx) {
	case ATTR_ENABLE:
		count = snprintf(buf, PAGE_SIZE, "%d", enab);
		break;
#ifdef SUPPORT_LEGACY_CONTROL
	case ATTR_ENABLE_C:
		count = snprintf(buf, PAGE_SIZE, "%s", en ? "gadget" : "tty");
		break;
#endif
	case ATTR_AUTORECONN:
		count = snprintf(buf, PAGE_SIZE, "%d", fn->autoreconn);
		break;
	case ATTR_STATISTICS:
		count = rawbulk_transfer_statistics(fn->transfer_id, buf);
		break;
	case ATTR_NUPS:
		count = snprintf(buf, PAGE_SIZE,
		"nups<%d>,udata<%d>,ucnt<%d>,drop<%d>,aloc_fai<%d>,tran<%d>\n",
		enab ? fn->nups : -1,
		upstream_data[fn->transfer_id],
		upstream_cnt[fn->transfer_id],
		total_drop[fn->transfer_id],
		alloc_fail[fn->transfer_id],
		total_tran[fn->transfer_id]);
		break;
	case ATTR_NDOWNS:
		count = snprintf(buf, PAGE_SIZE, "%d", enab ? fn->ndowns : -1);
		break;
	case ATTR_UPSIZE:
		count = snprintf(buf, PAGE_SIZE, "%d", enab ? fn->upsz : -1);
		break;
	case ATTR_DOWNSIZE:
		count = snprintf(buf, PAGE_SIZE, "%d", enab ? fn->downsz : -1);
		break;
	default:
		break;
	}
	return count;
}

#ifdef C2K_USB_UT
int total_cnt;
void do_push_upstream(int transfer_id, char *buf, int len)
{
	int ret;

	while (1) {
		ret = rawbulk_push_upstream_buffer(transfer_id, buf, len);
		if (ret == len || ret == 0) {
			C2K_DBG("push ret(%d)\n", ret);
#ifdef C2K_USB_PERF
			total_cnt += 1;
			if (total_cnt == 1024) {
				C2K_NOTE("4MB got\n");
				total_cnt = 0;
			}
#endif
			break;
		}
		C2K_DBG("push ret(%d)\n", ret);
#ifndef C2K_USB_PERF
		udelay(1000);
#endif

	}
}
#endif

static ssize_t rawbulk_attr_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	int n;
	int rc = 0;
	int idx = -1;
	int nups;
	int ndowns;
	int upsz;
	int downsz;

	struct rawbulk_function *fn;

	for (n = 0; n < _MAX_TID; n++) {
		fn = rawbulk_lookup_function(n);
		if (fn->dev == dev) {
			idx = which_attr(fn, attr);
			break;
		}
#ifdef SUPPORT_LEGACY_CONTROL
		if (!strcmp(attr->attr.name, fn->shortname)) {
			idx = ATTR_ENABLE_C;
			break;
		}
#endif
	}

	if (idx < 0) {
		C2K_ERR("sorry, I cannot find rawbulk fn '%s'\n",
			attr->attr.name);
		goto exit;
	}

	nups = fn->nups;
	ndowns = fn->ndowns;
	upsz = fn->upsz;
	downsz = fn->downsz;

	/* find out which attr(file) we write */
#ifdef SUPPORT_LEGACY_CONTROL
	if (idx <= ATTR_ENABLE_C) {
#else
	if (idx == ATTR_ENABLE) {
#endif
		int enable;
		long tmp;

#ifdef SUPPORT_LEGACY_CONTROL
		if (idx == ATTR_ENABLE) {
#else
		{
#endif
			int ret;

			ret = kstrtol(buf, 0, &tmp);
			enable = (int)tmp;
			C2K_NOTE("enable:%d\n", enable);

#ifdef C2K_USB_UT
			if (enable == UT_CMD) {
				int i;
				unsigned int len;
				int transfer_id = n;
				static char last_end;
				char *buf = kmalloc(SZ, GFP_KERNEL);

				C2K_NOTE("buf(%p)\n", buf);

/* #define C2K_USB_PERF */
#ifdef C2K_USB_PERF
				total_cnt = 0;
				memset(buf, 0x41, SZ);
#endif
				while (!signal_pending(current)) {
#ifdef C2K_USB_PERF
					len = SZ;
#else
					if (ut_err) {
						C2K_NOTE("errrrrrrrrrrrrrr\n");
						break;
					}
					get_random_bytes(&len, sizeof(len));
					C2K_NOTE("len before(%d)\n", len);
					len %= SZ;
					len += 1;
					C2K_NOTE("len after(%d)\n", len);
					buf[0] = last_end + 1;
					for (i = 1; i < len; i++)
						buf[i] = buf[i - 1] + 1;
					last_end = buf[i - 1];
#endif
					do_push_upstream(transfer_id, buf, len)
						;
#ifdef C2K_USB_PERF
					/* simulate 4MB per sec */
					udelay(delay_set);
#endif
				}
				if (buf) {
					C2K_NOTE("free buf\n");
					kfree(buf);
				}
				ut_err = 0;
				C2K_NOTE("exit UT\n");
				goto exit;
			} else if (enable == UT_CLR_ERR) {
				ut_err = 0;
				goto exit;
			}
#endif

#ifdef SUPPORT_LEGACY_CONTROL
		} else if (idx == ATTR_ENABLE_C) {
			if (!strncmp(buf, "tty", 3))
				enable = 0;
			else if (!strncmp(buf, "gadget", 6))
				enable = 1;
			else {
				C2K_ERR("invalid option(%s) for bypas\n", buf);
				goto exit;
			}
		} else
			goto exit;
#else
		}
#endif				/* SUPPORT_LEGACY_CONTROL */


		if ((check_enable_state(fn) != (!!enable))) {
			C2K_NOTE("enable:%d\n", enable);
			set_enable_state(fn, enable);
			if (!!enable && fn->activated) {
				C2K_NOTE("enable rb %s channel,activated %s\n",
					fn->shortname, fn->activated ?
					"yes" : "no");
				if (fn->transfer_id == RAWBULK_TID_MODEM) {
					/* clear DTR to endup last session
					 * between AP and CP
					 */
					modem_dtr_set(1, 1);
					modem_dtr_set(0, 1);
					modem_dtr_set(1, 1);
					modem_dcd_state();
				}


				/* Start rawbulk transfer */
				__pm_stay_awake(&fn->keep_awake);
				rc = rawbulk_start_transactions(
					fn->transfer_id, nups, ndowns, upsz,
					downsz);
				if (rc < 0)
					C2K_ERR("%s rc = %d bypass failed\n",
						__func__, rc);
			} else {
				/* Stop rawbulk transfer */
				C2K_NOTE("D.S rb %s chan,been activated %s\n",
					fn->shortname,
					fn->activated ? "yes" : "no");
				rawbulk_stop_transactions(fn->transfer_id);
				if (fn->transfer_id == RAWBULK_TID_MODEM) {
					/* clear DTR automatically when disable
					 * modem rawbulk
					 */
					modem_dtr_set(1, 1);
					modem_dtr_set(0, 1);
					modem_dtr_set(1, 1);
					modem_dcd_state();
				}

				__pm_relax(&fn->keep_awake);
			}
		}
	} else if (idx == ATTR_DTR) {
		if (fn->transfer_id == RAWBULK_TID_MODEM) {
			if (check_enable_state(fn)) {
				int val, ret;
				long tmp;

				ret = kstrtol(buf, 0, &tmp);
				val = (int)tmp;
				modem_dtr_set(val, 1);
			}
		}
	} else if (idx == ATTR_AUTORECONN) {
		int val, ret;
		long tmp;

		ret = kstrtol(buf, 0, &tmp);
		val = (int)tmp;
		fn->autoreconn = !!val;
	} else {
		int val, ret;
		long tmp;

		ret = kstrtol(buf, 0, &tmp);
		val = (int)tmp;
		switch (idx) {
		case ATTR_NUPS:
			nups = val;
			break;
		case ATTR_NDOWNS:
			ndowns = val;
			break;
		case ATTR_UPSIZE:
			upsz = val;
			break;
		case ATTR_DOWNSIZE:
			downsz = val;
			break;
		default:
			goto exit;
		}

		if (!check_enable_state(fn))
			goto exit;

		if (!fn->activated)
			goto exit;

		rawbulk_stop_transactions(fn->transfer_id);
		rc = rawbulk_start_transactions(fn->transfer_id, nups, ndowns,
						upsz, downsz);
		if (rc >= 0) {
			fn->nups = nups;
			fn->ndowns = ndowns;
			fn->upsz = upsz;
			fn->downsz = downsz;
		} else {
			rawbulk_stop_transactions(fn->transfer_id);
			__pm_relax(&fn->keep_awake);
			C2K_NOTE("enable to 0\n");
			set_enable_state(fn, 0);
		}
	}

exit:
	return count;
}

static int rawbulk_create_files(struct rawbulk_function *fn)
{
	int n, rc;

	for (n = 0; n < fn->max_attrs; n++) {
#ifdef SUPPORT_LEGACY_CONTROL
		if (n == ATTR_ENABLE_C)
			continue;
#endif
		rc = device_create_file(fn->dev, &fn->attr[n]);
		if (rc < 0) {
			while (--n >= 0) {
#ifdef SUPPORT_LEGACY_CONTROL
				if (n == ATTR_ENABLE_C)
					continue;
#endif
				device_remove_file(fn->dev, &fn->attr[n]);
			}
			return rc;
		}
	}
	return 0;
}

static void rawbulk_remove_files(struct rawbulk_function *fn)
{
	int n = fn->max_attrs;

	while (--n >= 0) {
#ifdef SUPPORT_LEGACY_CONTROL
		if (n == ATTR_ENABLE_C)
			continue;
#endif
		device_remove_file(fn->dev, &fn->attr[n]);
	}
}




/*****************************************************************************/
static struct class *rawbulk_class;

static struct _function_init_stuff {
	const char *longname;
	const char *shortname;
	const char *iString;
	unsigned int nups;
	unsigned int ndowns;
	unsigned int upsz;
	unsigned int downsz;
	bool autoreconn;
	bool pushable;
} _init_params[] = {
#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
	{
	"rawbulk-modem", "data", "Modem Port", 32, 32, 4096, 4096, true, false}
	, {
	"rawbulk-ets", "ets", "ETS Port", 32, 32, 4096, 4096, true, false}, {
	"rawbulk-at", "atc", "AT Channel", 3, 3, 4096, 4096, true, false}, {
	"rawbulk-pcv", "pcv", "PCM Voice", 1, 1, 4096, 4096, true, false}, {
	"rawbulk-gps", "gps", "LBS GPS Port", 1, 1, 4096, 4096, true, false}, {
	},			/* End of configurations */
#else
	{
	"rawbulk-pcv", "pcv", "PCM Voice", 1, 1, 4096, 4096, true, false}, {
	"rawbulk-modem", "data", "Modem Port", 32, 32, 4096, 4096, true, false}
	, {
	"rawbulk-dummy0", "dummy0", "DUMMY0", 1, 1, 4096, 4096, true, false}, {
	"rawbulk-at", "atc", "AT Channel", 3, 3, 4096, 4096, true, false}, {
	"rawbulk-gps", "gps", "LBS GPS Port", 1, 1, 4096, 4096, true, false}, {
	"rawbulk-dummy1", "dummy1", "DUMMY1", 1, 1, 4096, 4096, true, false}, {
	"rawbulk-dummy2", "dummy2", "DUMMY2", 1, 1, 4096, 4096, true, false}, {
	"rawbulk-ets", "ets", "ETS Port", 32, 32, 4096, 4096, true, false}, {
	},			/* End of configurations */
#endif
};

static __init struct rawbulk_function *rawbulk_alloc_function(int transfer_id)
{
	int rc;
	struct rawbulk_function *fn;

	C2K_NOTE("%s\n", __func__);

	if (transfer_id == _MAX_TID)
		return NULL;

	fn = kzalloc(sizeof(*fn), GFP_KERNEL);
	if (IS_ERR(fn))
		return NULL;

	/* init default features of rawbulk functions */
	fn->longname = _init_params[transfer_id].longname;
	fn->shortname = _init_params[transfer_id].shortname;
	fn->string_defs[0].s = _init_params[transfer_id].iString;
	fn->nups = _init_params[transfer_id].nups;
	fn->ndowns = _init_params[transfer_id].ndowns;
	fn->upsz = _init_params[transfer_id].upsz;
	fn->downsz = _init_params[transfer_id].downsz;
	fn->autoreconn = _init_params[transfer_id].autoreconn;

	fn->tty_minor = -1;
	/* init descriptors */
	init_interface_desc(&fn->interface);
	init_endpoint_desc(&fn->fs_bulkin_endpoint, 1, 512);
	init_endpoint_desc(&fn->hs_bulkin_endpoint, 1, 512);
	init_endpoint_desc(&fn->fs_bulkout_endpoint, 0, 512);
	init_endpoint_desc(&fn->hs_bulkout_endpoint, 0, 512);

	fn->fs_descs[INTF_DESC] = (struct usb_descriptor_header *)
				&fn->interface;
	fn->fs_descs[BULKIN_DESC] = (struct usb_descriptor_header *)
				&fn->fs_bulkin_endpoint;
	fn->fs_descs[BULKOUT_DESC] = (struct usb_descriptor_header *)
				&fn->fs_bulkout_endpoint;

	fn->hs_descs[INTF_DESC] = (struct usb_descriptor_header *)
				&fn->interface;
	fn->hs_descs[BULKIN_DESC] = (struct usb_descriptor_header *)
				&fn->hs_bulkin_endpoint;
	fn->hs_descs[BULKOUT_DESC] = (struct usb_descriptor_header *)
				&fn->hs_bulkout_endpoint;

	fn->string_table.language = 0x0409;
	fn->string_table.strings = fn->string_defs;
	fn->strings[0] = &fn->string_table;
	fn->strings[1] = NULL;

	fn->transfer_id = transfer_id;
	fn->tty_throttled = 0;
	/* init function callbacks */
	fn->function.strings = fn->strings;
	fn->function.fs_descriptors = fn->fs_descs;
	fn->function.hs_descriptors = fn->hs_descs;

	/* init device attributes */
	add_device_attr(fn, ATTR_ENABLE, "enable", 0660);
#ifdef SUPPORT_LEGACY_CONTROL
	add_device_attr(fn, ATTR_ENABLE_C, fn->shortname, 0660);
#endif
	add_device_attr(fn, ATTR_AUTORECONN, "autoreconn", 0660);
	add_device_attr(fn, ATTR_STATISTICS, "statistics", 0660);
	add_device_attr(fn, ATTR_NUPS, "nups", 0660);
	add_device_attr(fn, ATTR_NDOWNS, "ndowns", 0660);
	add_device_attr(fn, ATTR_UPSIZE, "ups_size", 0660);
	add_device_attr(fn, ATTR_DOWNSIZE, "downs_size", 0660);

	fn->max_attrs = ATTR_DOWNSIZE + 1;

	if (transfer_id == RAWBULK_TID_MODEM) {
		add_device_attr(fn, ATTR_DTR, "dtr", 0220);
		fn->max_attrs++;
	}

	fn->dev = device_create(rawbulk_class, NULL, MKDEV(0,
				fn->transfer_id), NULL, fn->shortname);
	if (IS_ERR(fn->dev)) {
		kfree(fn);
		return NULL;
	}
	rc = rawbulk_create_files(fn);
	if (rc < 0) {
		device_destroy(rawbulk_class, fn->dev->devt);
		kfree(fn);
		return NULL;
	}

	spin_lock_init(&fn->lock);
	wakeup_source_init(&fn->keep_awake, fn->longname);
	return fn;
}

static void rawbulk_destroy_function(struct rawbulk_function *fn)
{
	C2K_DBG("%s\n", __func__);

	if (!fn)
		return;
	wakeup_source_trash(&fn->keep_awake);
	rawbulk_remove_files(fn);
	device_destroy(rawbulk_class, fn->dev->devt);
	kfree(fn);
}

#ifdef SUPPORT_LEGACY_CONTROL
static struct attribute *legacy_sysfs[_MAX_TID + 1] = { NULL };

static struct attribute_group legacy_sysfs_group = {
	.attrs = legacy_sysfs,
};

struct kobject *legacy_sysfs_stuff;
#endif				/* SUPPORT_LEGACY_CONTROL */

static int __init init(void)
{
	int n;
	int rc = 0;

	C2K_NOTE("rawbulk functions init.\n");
	rawbulk_class = class_create(THIS_MODULE, "usb_rawbulk");
	if (IS_ERR(rawbulk_class))
		return PTR_ERR(rawbulk_class);

	for (n = 0; n < _MAX_TID; n++) {
		struct rawbulk_function *fn = rawbulk_alloc_function(n);

		if (IS_ERR(fn)) {
			while (n--)
				rawbulk_destroy_function
				(prealloced_functions[n]);
			rc = PTR_ERR(fn);
			break;
		}
		prealloced_functions[n] = fn;
#ifdef SUPPORT_LEGACY_CONTROL
		legacy_sysfs[n] = &fn->attr[ATTR_ENABLE_C].attr;
#endif
	}

	if (rc < 0) {
		class_destroy(rawbulk_class);
		return rc;
	}
#ifdef SUPPORT_LEGACY_CONTROL
	/* make compatiable with old bypass sysfs access */
	legacy_sysfs_stuff = kobject_create_and_add("usb_bypass", NULL);
	if (legacy_sysfs_stuff) {
		rc = sysfs_create_group(legacy_sysfs_stuff,
					&legacy_sysfs_group);
		if (rc < 0)
			C2K_ERR("failed to create legacy bypass,continue\n");
	}
#endif				/* SUPPORT_LEGACY_CONTROL */

	return 0;
}
module_init(init);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
