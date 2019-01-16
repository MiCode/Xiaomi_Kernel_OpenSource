/*
 * Rawbulk Gadget Function Driver from VIA Telecom
 *
 * Copyright (C) 2011 VIA Telecom, Inc.
 * Author: Karfield Chen (kfchen@via-telecom.com)
 * Copyright (C) 2012 VIA Telecom, Inc.
 * Author: Juelun Guo (jlguo@via-telecom.com)
 * Changes:
 *
 * Sep 2012: Juelun Guo <jlguo@via-telecom.com>
 *           Version 1.0.2
 *           changed to support for sdio bypass.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
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
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

#include <linux/usb/composite.h>
#include <mach/viatel_rawbulk.h>

#if 1
//#ifdef DEBUG
#define ldbg(f, a...) printk(KERN_INFO "%s - " f "\n", __func__, ##a)
#else
#define ldbg(...) {}
#endif
/* sysfs attr idx assignment */
enum _attr_idx {
    ATTR_ENABLE = 0,            /** enable switch for Rawbulk */
#ifdef SUPPORT_LEGACY_CONTROL
    ATTR_ENABLE_C,              /** enable switch too, but for legacy */
#endif
    ATTR_AUTORECONN,            /** enable to rebind cp when it reconnect */
    ATTR_STATISTICS,            /** Rawbulk summary/statistics for one pipe */
    ATTR_NUPS,                  /** upstream transfers count */
    ATTR_NDOWNS,                /** downstram transfers count */
    ATTR_UPSIZE,                /** upstream buffer for each transaction */
    ATTR_DOWNSIZE,              /** downstram buffer for each transaction */
    ATTR_DTR,                   /** DTR control, only for Data-Call port */
};

/* USB gadget framework is not strong enough, and not be compatiable with some
 * controller, such as musb.
 * in musb driver, the usb_request's member list is used internally, but in some
 * applications it used in function driver too. to avoid this, here we
 * re-wrap the usb_request */
struct usb_request_wrapper {
    struct list_head list;
    struct usb_request *request;
    int length;
    struct rawbulk_function *fn;
    //char buffer[0];
    char *buffer;
};

static inline struct usb_request_wrapper *get_wrapper(struct usb_request *req) {
    if (!req->buf)
        return NULL;
    //return container_of(req->buf, struct usb_request_wrapper, buffer);
    return (struct usb_request_wrapper *)req->context;
}

/* Internal TTY functions/data */
static int rawbulk_tty_activate(struct tty_port *port, struct tty_struct
        *tty);
static void rawbulk_tty_deactivate(struct tty_port *port);
static int rawbulk_tty_install(struct tty_driver *driver, struct tty_struct
        *tty);
static void rawbulk_tty_cleanup(struct tty_struct *tty);
static int rawbulk_tty_open(struct tty_struct *tty, struct file *flip);
static void rawbulk_tty_hangup(struct tty_struct *tty);
static void rawbulk_tty_close(struct tty_struct *tty, struct file *flip);
static int rawbulk_tty_write(struct tty_struct *tty, const unsigned char *buf,
        int count);
static int rawbulk_tty_write_room(struct tty_struct *tty);
static int rawbulk_tty_chars_in_buffer(struct tty_struct *tty);
static void rawbulk_tty_throttle(struct tty_struct *tty);
static void rawbulk_tty_unthrottle(struct tty_struct *tty);
static void rawbulk_tty_set_termios(struct tty_struct *tty, struct ktermios
        *old);
static int rawbulk_tty_tiocmget(struct tty_struct *tty);
static int rawbulk_tty_tiocmset(struct tty_struct *tty, unsigned int set,
        unsigned int clear);

//extern int rawbulk_usb_state_check(void);

static struct tty_port_operations rawbulk_tty_port_ops = {
    .activate = rawbulk_tty_activate,
    .shutdown = rawbulk_tty_deactivate,
};

static struct tty_operations rawbulk_tty_ops = {
	.open =			rawbulk_tty_open,
	.close =		rawbulk_tty_close,
	.write =		rawbulk_tty_write,
	.hangup = 		rawbulk_tty_hangup,
	.write_room =	rawbulk_tty_write_room,
	.set_termios =	rawbulk_tty_set_termios,
	.throttle =		rawbulk_tty_throttle,
	.unthrottle =	rawbulk_tty_unthrottle,
	.chars_in_buffer =	rawbulk_tty_chars_in_buffer,
	.tiocmget =		rawbulk_tty_tiocmget,
	.tiocmset =		rawbulk_tty_tiocmset,
	.cleanup = 		rawbulk_tty_cleanup,
	.install = 		rawbulk_tty_install,
};

struct tty_driver *rawbulk_tty_driver;

struct rawbulk_function *prealloced_functions[_MAX_TID] = { NULL };
struct rawbulk_function *rawbulk_lookup_function(int transfer_id) {
    ldbg("%s\n", __func__);
    if (transfer_id >= 0 && transfer_id < _MAX_TID)
        return prealloced_functions[transfer_id];
    return NULL;
}
EXPORT_SYMBOL_GPL(rawbulk_lookup_function);

static struct rawbulk_function *lookup_by_tty_minor(int minor) {
    int n;
    struct rawbulk_function *fn;
    ldbg("%s\n", __func__);
    for (n = 0; n < _MAX_TID; n ++) {
        fn = prealloced_functions[n];
        if (fn->tty_minor == minor)
            return fn;
    }
    return NULL;
}

static inline int check_enable_state(struct rawbulk_function *fn) {
    int enab;
    unsigned long flags;
    ldbg("%s\n", __func__);
    spin_lock_irqsave(&fn->lock, flags);
    enab = fn->enable? 1: 0;
    spin_unlock_irqrestore(&fn->lock, flags);
    return enab;
}

int rawbulk_check_enable(struct rawbulk_function *fn) {
    ldbg("%s\n", __func__);
    return check_enable_state(fn);
}
EXPORT_SYMBOL_GPL(rawbulk_check_enable);

static inline int check_tty_opened(struct rawbulk_function *fn) {
    int opened;
    unsigned long flags;
    ldbg("%s\n", __func__);
    spin_lock_irqsave(&fn->lock, flags);
    opened = fn->tty_opened? 1: 0;
    spin_unlock_irqrestore(&fn->lock, flags);
    return opened;
}

static inline void set_enable_state(struct rawbulk_function *fn, int enab) {
    unsigned long flags;
    spin_lock_irqsave(&fn->lock, flags);
    fn->enable = !!enab;
    spin_unlock_irqrestore(&fn->lock, flags);
}

void rawbulk_disable_function(struct rawbulk_function *fn) {
    set_enable_state(fn, 0);
}
EXPORT_SYMBOL_GPL(rawbulk_disable_function);

static inline void set_tty_opened(struct rawbulk_function *fn, int opened) {
    unsigned long flags;
    spin_lock_irqsave(&fn->lock, flags);
    fn->tty_opened = !!opened;
    spin_unlock_irqrestore(&fn->lock, flags);
}

#define port_to_rawbulk(p) container_of(p, struct rawbulk_function, port)
#define function_to_rawbulk(f) container_of(f, struct rawbulk_function, function)

static inline struct usb_request_wrapper *get_req(struct list_head *head, spinlock_t
        *lock) {
    unsigned long flags;
    struct usb_request_wrapper *req = NULL;
    spin_lock_irqsave(lock, flags);
    if (!list_empty(head)) {
		req = list_first_entry(head, struct usb_request_wrapper, list);
		list_del(&req->list);
    }
    spin_unlock_irqrestore(lock, flags);
    return req;
}

static inline struct usb_request_wrapper *get_req_without_del(struct list_head *head, spinlock_t
        *lock) {
    unsigned long flags;
    struct usb_request_wrapper *req = NULL;
    spin_lock_irqsave(lock, flags);
    if (!list_empty(head)) {
		req = list_first_entry(head, struct usb_request_wrapper, list);
    }
    spin_unlock_irqrestore(lock, flags);
    return req;
}
static inline void put_req(struct usb_request_wrapper *req, struct list_head *head,
        spinlock_t *lock) {
    unsigned long flags;
    spin_lock_irqsave(lock, flags);
	list_add_tail(&req->list, head);
    spin_unlock_irqrestore(lock, flags);
}

static inline void move_req(struct usb_request_wrapper *req, struct list_head *head,
        spinlock_t *lock) {
    unsigned long flags;
    spin_lock_irqsave(lock, flags);
	list_move_tail(&req->list, head);
    spin_unlock_irqrestore(lock, flags);
}

static inline void insert_req(struct usb_request_wrapper *req, struct list_head *head,
        spinlock_t *lock) {
    unsigned long flags;
    spin_lock_irqsave(lock, flags);
	list_add(&req->list, head);
    spin_unlock_irqrestore(lock, flags);
}

static void init_endpoint_desc(struct usb_endpoint_descriptor *epdesc, int in,
        int maxpacksize) {
    struct usb_endpoint_descriptor template = {
        .bLength =      USB_DT_ENDPOINT_SIZE,
        .bDescriptorType =  USB_DT_ENDPOINT,
        .bmAttributes =     USB_ENDPOINT_XFER_BULK,
    };

    *epdesc = template;
    if (in)
        epdesc->bEndpointAddress = USB_DIR_IN;
    else
        epdesc->bEndpointAddress = USB_DIR_OUT;
    epdesc->wMaxPacketSize = cpu_to_le16(maxpacksize);
}

static void init_interface_desc(struct usb_interface_descriptor *ifdesc) {
    struct usb_interface_descriptor template = {
        .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = 0xff,//USB_CLASS_VENDOR_SPEC,
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

static inline void add_device_attr(struct rawbulk_function *fn, int n, const char
        *name, int mode) {
    if (n < MAX_ATTRIBUTES) {
        fn->attr[n].attr.name = name;
        fn->attr[n].attr.mode = mode;
        fn->attr[n].show = rawbulk_attr_show;
        fn->attr[n].store = rawbulk_attr_store;
    }
}

static int which_attr(struct rawbulk_function *fn, struct device_attribute
        *attr) {
    int n;
    for (n = 0; n < fn->max_attrs; n ++) {
        if (attr == &fn->attr[n])
            return n;
    }
    return -1;
}

static ssize_t rawbulk_attr_show(struct device *dev, struct device_attribute *attr,
        char *buf) {
    int n;
    int idx;
    int enab;
    struct rawbulk_function *fn;

    for (n = 0; n < _MAX_TID; n ++) {
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
            return sprintf(buf, "%d", enab);
#ifdef SUPPORT_LEGACY_CONTROL
        case ATTR_ENABLE_C:
            return sprintf(buf, "%s", enab? "gadget": "tty");
#endif
        case ATTR_AUTORECONN:
            return sprintf(buf, "%d", fn->autoreconn);
        case ATTR_STATISTICS:
            return rawbulk_transfer_statistics(fn->transfer_id, buf);
        case ATTR_NUPS:
            return sprintf(buf, "%d", enab? fn->nups: -1);
        case ATTR_NDOWNS:
            return sprintf(buf, "%d", enab? fn->ndowns: -1);
        case ATTR_UPSIZE:
            return sprintf(buf, "%d", enab? fn->upsz: -1);
        case ATTR_DOWNSIZE:
            return sprintf(buf, "%d", enab? fn->downsz: -1);
        default:
            break;
    }
    return 0;
}

static ssize_t rawbulk_attr_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count) {
    int n;
    int rc = 0;
    int idx = -1;
    int nups;
    int ndowns;
    int upsz;
    int downsz;

    struct rawbulk_function *fn;
        
    for (n = 0; n < _MAX_TID; n ++) {
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
        printk(KERN_ERR "sorry, I cannot find rawbulk fn '%s'\n", attr->attr.name);
        goto exit;
    }

    nups = fn->nups;
    ndowns = fn->ndowns;
    upsz = fn->upsz;
    downsz = fn->downsz;

    /* find out which attr(file) we write */
#ifdef SUPPORT_LEGACY_CONTROL
    if (idx <= ATTR_ENABLE_C)
#else
    if (idx == ATTR_ENABLE)
#endif
    {
        int enable;
        if (idx == ATTR_ENABLE) {
            enable = simple_strtoul(buf, NULL, 10);
#ifdef SUPPORT_LEGACY_CONTROL
        } else if (idx == ATTR_ENABLE_C) {
            if (!strncmp(buf, "tty", 3))
                enable = 0;
            else if (!strncmp(buf, "gadget", 6))
                enable = 1;
            else {
                printk(KERN_ERR "invalid option(%s) for bypass, try again...\n", buf);
                goto exit;
            }
#endif /* SUPPORT_LEGACY_CONTROL */
        } else
            goto exit;

        if ((check_enable_state(fn) != (!!enable))) {
            set_enable_state(fn, enable);
            if (!!enable && fn->activated) {
                printk(KERN_INFO "enable rawbulk %s channel, been activated? %s\n",
                        fn->shortname, fn->activated? "yes": "no");            
                if (fn->transfer_id == RAWBULK_TID_MODEM) {
                    /* clear DTR to endup last session between AP and CP */
                     modem_dtr_set(1, 1);
                     modem_dtr_set(0, 1);
                     modem_dtr_set(1, 1);
                     modem_dcd_state();
                }
                
                /* Stop TTY i/o */
                rawbulk_tty_stop_io(fn);

                /* Start rawbulk transfer */
                wake_lock(&fn->keep_awake);
                rc = rawbulk_start_transactions(fn->transfer_id, nups,
                        ndowns, upsz, downsz);
                if (rc < 0) {
                    printk(KERN_ERR "%s rc = %d bypass failed\n", __func__, rc);
                    //set_enable_state(fn, !enable);
                    //rawbulk_tty_start_io(fn);
                }
            } else {
                /* Stop rawbulk transfer */
                printk(KERN_INFO "disable rawbulk %s channel, been activated? %s\n",
                        fn->shortname, fn->activated? "yes": "no");           
                rawbulk_stop_transactions(fn->transfer_id);
                if (fn->transfer_id == RAWBULK_TID_MODEM) {
                    /* clear DTR automatically when disable modem rawbulk */
                    modem_dtr_set(1, 1);
                    modem_dtr_set(0, 1);
                    modem_dtr_set(1, 1);
                    modem_dcd_state();
                }
                
                wake_unlock(&fn->keep_awake);

                /* Start TTY i/o */
                rawbulk_tty_start_io(fn);
            }
        }
    } else if (idx == ATTR_DTR) {
        if (fn->transfer_id == RAWBULK_TID_MODEM) {
            if (check_enable_state(fn))
                modem_dtr_set(simple_strtoul(buf, NULL, 10), 1);
        }
    } else if (idx == ATTR_AUTORECONN) {
        fn->autoreconn = !!simple_strtoul(buf, NULL, 10);
    } else {
        int val = simple_strtoul(buf, NULL, 10);
        switch (idx) {
            case ATTR_NUPS: nups = val; break;
            case ATTR_NDOWNS: ndowns = val; break;
            case ATTR_UPSIZE: upsz = val; break;
            case ATTR_DOWNSIZE: downsz = val; break;
            default: goto exit;
        }

        if (!check_enable_state(fn))
            goto exit;

        if (!fn->activated)
            goto exit;

        rawbulk_stop_transactions(fn->transfer_id);
        rc = rawbulk_start_transactions(fn->transfer_id, nups, ndowns, upsz,
                downsz);
        if (rc >= 0) {
            fn->nups = nups;
            fn->ndowns = ndowns;
            fn->upsz = upsz;
            fn->downsz = downsz;
        } else {
            rawbulk_stop_transactions(fn->transfer_id);
            wake_unlock(&fn->keep_awake);
            set_enable_state(fn, 0);
        }
    }

exit:
    return count;
}

static int rawbulk_create_files(struct rawbulk_function *fn) {
    int n, rc;
    
    for (n = 0; n < fn->max_attrs; n ++){
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

static void rawbulk_remove_files(struct rawbulk_function *fn) {
    int n = fn->max_attrs;
    while (--n >= 0) {
#ifdef SUPPORT_LEGACY_CONTROL
        if (n == ATTR_ENABLE_C)
            continue;
#endif
        device_remove_file(fn->dev, &fn->attr[n]);
    }
}

#define RAWBULK_TTY_MINORS      255
static int tty_minors[RAWBULK_TTY_MINORS] = { 0 };
static int alloc_tty_minor(struct rawbulk_function *fn) {
    int n;
    ldbg("%s\n", __func__);
    if (check_tty_opened(fn))
        printk(KERN_WARNING "ttyRB%d has opened yet %s has been disconnected\n",
            fn->tty_minor, fn->longname);
    for (n = 0; n < RAWBULK_TTY_MINORS; n ++) {
        if (tty_minors[n] == 0) {
            tty_minors[n] = fn->transfer_id + 0x8000;
            return n;
        }
    }
    return -ENODEV;
}

static void free_tty_minor(struct rawbulk_function *fn) {
    int n;
    struct tty_struct **ttys = rawbulk_tty_driver->ttys;
    ldbg("%s\n", __func__);
    if (!check_tty_opened(fn))
        tty_minors[fn->tty_minor] = 0;
    /* Clear the closed tty minors of this port */
    for (n = 0; n < RAWBULK_TTY_MINORS; n ++) {
        if ((tty_minors[n] == fn->transfer_id + 0x8000) && ttys[n]) {
            struct tty_struct *tty = ttys[n];
            if (tty->count > 0)
                continue; /* Keep the minor while tty is being used */
            if (test_bit(TTY_CLOSING, &tty->flags)) {
                /* FIXME: Cannot close the tty_struct! */
                printk(KERN_INFO "cannot recycle the minor %d because " \
                        "the TTY_CLOSING flags is still on\n", n);
                continue;
            }
            tty_minors[n] = 0;
        }
    }
}

int rawbulk_register_tty(struct rawbulk_function *fn) {
    struct device *dev;
    ldbg("%s\n", __func__);

    spin_lock_init(&fn->tx_lock);
    spin_lock_init(&fn->rx_lock);
    INIT_LIST_HEAD(&fn->tx_free);
    INIT_LIST_HEAD(&fn->rx_free);
    INIT_LIST_HEAD(&fn->tx_inproc);
    INIT_LIST_HEAD(&fn->rx_inproc);
    INIT_LIST_HEAD(&fn->rx_throttled);

    fn->tty_minor = alloc_tty_minor(fn);
    if (fn->tty_minor < 0)
        return -ENODEV;
    fn->tty_opened = 0;
    fn->last_pushed = 0;
    tty_port_init(&fn->port);
    fn->port.ops = &rawbulk_tty_port_ops;

    /* Bring up the tty device */
    dev = tty_register_device(rawbulk_tty_driver, fn->tty_minor, fn->dev);
    if (IS_ERR(dev))
        printk(KERN_ERR "Failed to attach ttyRB%d to %s device.\n",
                fn->tty_minor, fn->longname);

    return 0;
}
EXPORT_SYMBOL_GPL(rawbulk_register_tty);

void rawbulk_unregister_tty(struct rawbulk_function *fn) {
    tty_unregister_device(rawbulk_tty_driver, fn->tty_minor);
    free_tty_minor(fn);
}
EXPORT_SYMBOL_GPL(rawbulk_unregister_tty);

/******************************************************************************/

/* Call this after all request has detached! */
void rawbulk_tty_free_request(struct rawbulk_function *fn) {
    struct usb_request_wrapper *req;
    ldbg("%s\n", __func__);

    while ((req = get_req(&fn->rx_free, &fn->rx_lock))) {
        //kfree(req->buffer);
        free_page((unsigned long) req->buffer);
        usb_ep_free_request(fn->bulk_out, req->request);
        kfree(req);
    }

    while ((req = get_req(&fn->tx_free, &fn->tx_lock))) {
        //kfree(req->buffer);
        free_page((unsigned long) req->buffer);
        usb_ep_free_request(fn->bulk_in, req->request);
        kfree(req);
    }
}
EXPORT_SYMBOL_GPL(rawbulk_tty_free_request);

#define print_request_data(req) { \
	if (req->status < 0) { \
		printk(KERN_INFO "%s: request failed %d\n", __func__, req->status); \
	} else { \
		int n; \
		printk(KERN_INFO "%s: data(%d):", __func__, req->actual); \
		for (n = 0; n < req->actual; n ++) { \
			char c = *((char *)(req->buf + n)); \
			if (c > 0x20 && c < 0x7e) \
				printk("%c", c); \
			else \
				printk("."); \
		} \
                printk(", hex: "); \
		for (n = 0; n < req->actual; n ++) { \
			char c = *((char *)(req->buf + n)); \
			printk("%02x ", c); \
		} \
		printk("\n"); \
	} \
}

static void rawbulk_tty_rx_complete(struct usb_ep *ep, struct usb_request *req);
static void rawbulk_tty_tx_complete(struct usb_ep *ep, struct usb_request *req);

int rawbulk_tty_alloc_request(struct rawbulk_function *fn) {
    int n;
    struct usb_request_wrapper *req;
    unsigned long flags;
    ldbg("%s\n", __func__);

    spin_lock_irqsave(&fn->lock, flags);
    if (!fn->bulk_out || !fn->bulk_in) {
        spin_unlock_irqrestore(&fn->lock, flags);
        return -ENODEV;
    }
    spin_unlock_irqrestore(&fn->lock, flags);

    /* Allocate and init rx request */
    for (n = 0; n < MAX_TTY_RX; n++) {
        //req = kzalloc(sizeof(struct usb_request_wrapper) +
        //        fn->upsz*sizeof(unsigned char), GFP_KERNEL);
        req = kmalloc(sizeof(struct usb_request_wrapper), GFP_KERNEL);
        if (!req)
            break;
            
        req->buffer = (char *)__get_free_page(GFP_KERNEL);
        if (!req->buffer) {
            kfree(req);
            break;
        }
        req->request = usb_ep_alloc_request(fn->bulk_out, GFP_KERNEL);
        if (!req->request) {
            //kfree(req->buffer);
            free_page((unsigned long) req->buffer);
            kfree(req);
            break;
        }

        req->fn = fn;
        INIT_LIST_HEAD(&req->list);
        req->length = fn->upsz;
        req->request->buf = req->buffer;
        req->request->length = fn->upsz;
        req->request->complete = rawbulk_tty_rx_complete;
        req->request->context = req;
        put_req(req, &fn->rx_free, &fn->rx_lock);
    }

    if (n < MAX_TTY_RX) {
        /* free allocated request */
        rawbulk_tty_free_request(fn);
        return -ENOMEM;
    }

    /* Allocate and init tx request */
    for (n = 0; n < MAX_TTY_TX; n++) {
        //req = kzalloc(sizeof(struct usb_request_wrapper) +
        //        fn->downsz* sizeof(unsigned char), GFP_KERNEL);
        req = kmalloc(sizeof(struct usb_request_wrapper), GFP_KERNEL);
        if (!req)
            break;
            
        req->buffer = (char *)__get_free_page(GFP_KERNEL);
        if (!req->buffer) {
            kfree(req);
            break;
        }
        
        if (!req)
            break;

        req->request = usb_ep_alloc_request(fn->bulk_in, GFP_KERNEL);
        if (!req->request) {
            //kfree(req->buffer);
            free_page((unsigned long) req->buffer);
            kfree(req);
            break;
        }

        req->fn = fn;
        INIT_LIST_HEAD(&req->list);
        req->length = fn->downsz;
        req->request->zero = 0;
        req->request->length = fn->downsz;
        req->request->buf = req->buffer;
        req->request->complete = rawbulk_tty_tx_complete;
        req->request->context = req;
        put_req(req, &fn->tx_free, &fn->tx_lock);
    }

    if (n < MAX_TTY_TX) {
        /* free allocated request */
        rawbulk_tty_free_request(fn);
        return -ENOMEM;
    }
    return 0;
}
EXPORT_SYMBOL_GPL(rawbulk_tty_alloc_request);

int rawbulk_tty_stop_io(struct rawbulk_function *fn) {
    struct usb_request_wrapper *req;
    ldbg("%s\n", __func__);

    if (!check_tty_opened(fn))
        return -ENODEV;
    if(fn->tty)
      tty_wakeup(fn->tty);
    flush_workqueue(fn->tty_push_wq);
    while ((req = get_req_without_del(&fn->rx_inproc, &fn->rx_lock))) {
        if (req->request->status == -EINPROGRESS) {                      
            usb_ep_dequeue(fn->bulk_out, req->request);  
        } else {
            msleep(5);
        }
    }
    while ((req = get_req_without_del(&fn->tx_inproc, &fn->tx_lock))) {       
        if (req->request->status == -EINPROGRESS) {
            usb_ep_dequeue(fn->bulk_in, req->request);           
        } else {       
            msleep(5);
        }
    }
 
    return 0;
}
EXPORT_SYMBOL_GPL(rawbulk_tty_stop_io);

int rawbulk_tty_start_io(struct rawbulk_function *fn) {
    int ret;
    struct usb_request_wrapper *req;
    ldbg("%s\n", __func__);
    
    if (!check_tty_opened(fn))
        return -ENODEV;

    while ((req = get_req(&fn->rx_free, &fn->rx_lock))) {
        put_req(req, &fn->rx_inproc, &fn->rx_lock);
//        if(rawbulk_usb_state_check())
            ret = usb_ep_queue(fn->bulk_out, req->request, GFP_ATOMIC);
//        else
//            return -ENODEV;
        if (ret < 0) {
            move_req(req, &fn->rx_free, &fn->rx_lock);
            return ret;
        }
    }

    return 0;
}
EXPORT_SYMBOL_GPL(rawbulk_tty_start_io);

/* Complete the data send stage */
static void rawbulk_tty_tx_complete(struct usb_ep *ep, struct usb_request *req) {
    struct usb_request_wrapper *r = get_wrapper(req);
    struct rawbulk_function *fn;
    ldbg("%s\n", __func__);
    
    if(!r) {
        return;
    }  
        
    fn = r->fn;    
	print_request_data(req);
    move_req(r, &fn->tx_free, &fn->tx_lock);
    if(fn->tty)
        tty_wakeup(fn->tty);
}

static void rawbulk_tty_push_data(struct work_struct *work) {
    struct usb_request_wrapper *r;
    struct tty_struct *tty;
    struct rawbulk_function *fn = container_of(work, struct rawbulk_function, tty_push_work);
    int ret = 0;
    ldbg("%s\n", __func__);
    
    if (!check_tty_opened(fn))
        return;

    tty = tty_port_tty_get(&fn->port);
    if(!tty){
        return;
	}
    // tty->minimum_to_wake = 1; /* wakeup to read even pushed only once */
    /* walk the throttled list, push all the data to TTY */
    while ((r = get_req(&fn->rx_throttled, &fn->rx_lock))) {
        char *sbuf;
        int push_count;
        struct usb_request *req = r->request;

        if (req->status < 0) {
            put_req(r, &fn->rx_free, &fn->rx_lock);
            continue;
        }
        sbuf = req->buf + fn->last_pushed;
        push_count = req->actual - fn->last_pushed;
        if (push_count) {
            int count = tty_insert_flip_string(tty, sbuf, push_count);
            if (count < push_count) {
                /* We met throttled again */
                fn->last_pushed += count;
                if (count)
                    tty_flip_buffer_push(tty);
                insert_req(r, &fn->rx_throttled, &fn->rx_lock);
                break;
            }
            tty_flip_buffer_push(tty);
            fn->last_pushed = 0;
        }
        put_req(r, &fn->rx_free, &fn->rx_lock);
    }
    tty_kref_put(tty);
    
    if(fn->tty_throttled == 1) {
        return;
    }
    
    while (check_tty_opened(fn) && (r = get_req(&fn->rx_free, &fn->rx_lock))) {
        put_req(r, &fn->rx_inproc, &fn->rx_lock);
        r->request->length = fn->downsz;
        r->request->buf = r->buffer;
//        if(rawbulk_usb_state_check())
            ret = usb_ep_queue(fn->bulk_out, r->request, GFP_ATOMIC);
//        else
//            return;
        if (ret < 0) {
            move_req(r, &fn->rx_free, &fn->rx_lock);
            break;
        }
    }
}

/* Recieve data from host */
static void rawbulk_tty_rx_complete(struct usb_ep *ep, struct usb_request *req) {
    int ret;
    struct usb_request_wrapper *r = get_wrapper(req);
    struct rawbulk_function *fn;
    struct tty_struct *tty;
    ldbg("%s\n", __func__);
            
    if(!r) {       
        return;
    }   
    fn = r->fn;
    tty = fn->tty;
    
	print_request_data(req);
    if (req->status < 0 || !check_tty_opened(fn)) {
        move_req(r, &fn->rx_free, &fn->rx_lock);
        return;
    }

    /* Move the request to the throttled queue */
    move_req(r, &fn->rx_throttled, &fn->rx_lock);
    /* Start to push data */
    queue_work(fn->tty_push_wq, &fn->tty_push_work);

}

/* Start to queue request on BULK OUT endpoint, when tty is try to open */
static int rawbulk_tty_activate(struct tty_port *port, struct tty_struct
        *tty) {
    struct rawbulk_function *fn = port_to_rawbulk(port);
    ldbg("%s\n", __func__);
    /* Low latency for tty */

    port->low_latency = 1;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 100))
	tty->termios.c_cc[VMIN] = 1;
#else
    tty->termios->c_cc[VMIN] = 1;
#endif
    fn->tty_throttled = 0;
    return rawbulk_tty_start_io(fn);
}

static void rawbulk_tty_deactivate(struct tty_port *port) {
    struct usb_request_wrapper *req;
    struct rawbulk_function *fn = port_to_rawbulk(port);
    ldbg("%s\n", __func__);
    
    /* This is a little different from stop_io */
    /*
    while ((req = get_req(&fn->rx_inproc, &fn->rx_lock))) {
        if(req->request->status == -EINPROGRESS) {
            put_req(req, &fn->rx_inproc, &fn->rx_lock);
            usb_ep_dequeue(fn->bulk_out, req->request);
        } else
            put_req(req, &fn->rx_free, &fn->rx_lock);
    }
    
    while ((req = get_req(&fn->tx_inproc, &fn->tx_lock))) {                
        if(req->request->status == -EINPROGRESS) {
            put_req(req, &fn->tx_inproc, &fn->tx_lock);
            usb_ep_dequeue(fn->bulk_in, req->request);           
        } else
            put_req(req, &fn->tx_free, &fn->tx_lock);
    }
 
    while ((req = get_req(&fn->rx_throttled, &fn->rx_lock)))
        put_req(req, &fn->rx_free, &fn->rx_lock);        
    */
}

static int rawbulk_tty_write(struct tty_struct *tty, const unsigned char *buf,
        int count) {
    int ret = 0;
    int submitted = 0;
    struct usb_request_wrapper *req;
    struct rawbulk_function *fn = tty->driver_data;
    ldbg("%s\n", __func__);

    if (check_enable_state(fn))
        return -EBUSY;

    /* Get new request(s) that freed, fill it, queue it to the endpoint */
    while ((req = get_req(&fn->tx_free, &fn->tx_lock)) && check_tty_opened(fn)) {
        int length = count - submitted;
        if (length <= 0) {
            tty_wakeup(tty);
            put_req(req, &fn->tx_free, &fn->tx_lock);
            break;
        }
        if (length > fn->downsz)
            length = fn->downsz;
        memcpy(req->buffer, buf + submitted, length);
        req->request->length = length;
        req->request->zero = ((length % 512) == 0);//fn->bulk_in->maxpacket) == 0);
        put_req(req, &fn->tx_inproc, &fn->tx_lock);
//        if(rawbulk_usb_state_check())
            ret = usb_ep_queue(fn->bulk_in, req->request, GFP_ATOMIC);
//        else
//            return -ENODEV;
        if (ret < 0) {
            move_req(req, &fn->tx_free, &fn->tx_lock);
            printk(KERN_ERR "%s: requeue ep_in usb request failed\n", __func__);
            return ret;
        }
        submitted += length;
    }

    return submitted;
}

static int rawbulk_tty_write_room(struct tty_struct *tty) {
    int room = 0;
    unsigned long flags;
    struct usb_request_wrapper *req;
    struct rawbulk_function *fn = tty->driver_data;
    ldbg("%s\n", __func__);

    if (check_enable_state(fn))
        return 0;

    spin_lock_irqsave(&fn->tx_lock, flags);
    list_for_each_entry(req, &fn->tx_free, list)
        room += req->length;
    spin_unlock_irqrestore(&fn->tx_lock, flags);

    return room;
}

static int rawbulk_tty_chars_in_buffer(struct tty_struct *tty) {
    int chars = 0;
    unsigned long flags;
    struct usb_request_wrapper *req;
    struct rawbulk_function *fn = tty->driver_data;
    ldbg("%s\n", __func__);

    if (check_enable_state(fn))
        return 0;

    spin_lock_irqsave(&fn->tx_lock, flags);
    list_for_each_entry(req, &fn->tx_inproc, list) {
        if (req->request->status < 0)
            continue;
        chars += req->request->length;
    }
    spin_unlock_irqrestore(&fn->tx_lock, flags);

    return chars;
}

static void rawbulk_tty_throttle(struct tty_struct *tty) {
    struct usb_request_wrapper *req;
    struct rawbulk_function *fn = tty->driver_data;
    ldbg("%s\n", __func__);
    
    if(!fn)
        return;
    fn->tty_throttled = 1;
    /* Stop the processing requests */
    /*while ((req = get_req(&fn->rx_inproc, &fn->rx_lock))) {
        if(req->request->status == -EINPROGRESS) {
            put_req(req, &fn->rx_inproc, &fn->rx_lock);
            usb_ep_dequeue(fn->bulk_out, req->request);
        } else
            put_req(req, &fn->rx_free, &fn->rx_lock);
    }*/
}

static void rawbulk_tty_unthrottle(struct tty_struct *tty) {
    int ret = 0;
    struct usb_request_wrapper *req;
    struct rawbulk_function *fn = tty->driver_data;
    ldbg("%s\n", __func__);
    
    if(!fn)
        return;
    /* Try to push the throttled requests' data to TTY */
    fn->tty_throttled = 0;
    queue_work(fn->tty_push_wq, &fn->tty_push_work);
    
    /* Restart the free requests */
   /* while ((req = get_req(&fn->rx_free, &fn->rx_lock))) {
        put_req(req, &fn->rx_inproc, &fn->rx_lock);
        //printk("%s, id = %d req requeue\n", __func__, fn->transfer_id);
        if(rawbulk_usb_state_check())
            ret = usb_ep_queue(fn->bulk_out, req->request, GFP_ATOMIC);
        if (ret < 0) { 
            printk("%s, id = %d req queue failed\n", __func__, fn->transfer_id);
            move_req(req, &fn->rx_free, &fn->rx_lock);
            break;
        }
    }*/
}

static void rawbulk_tty_set_termios(struct tty_struct *tty, struct ktermios
        *old) {
    //struct rawbulk_function *fn = tty->driver_data;
}

static int rawbulk_tty_tiocmget(struct tty_struct *tty) {
    //struct rawbulk_function *fn = tty->driver_data;
    return 0;
}

static int rawbulk_tty_tiocmset(struct tty_struct *tty, unsigned int set,
        unsigned int clear) {
    //struct rawbulk_function *fn = tty->driver_data;
    return 0;
}

static int rawbulk_tty_install(struct tty_driver *driver, struct tty_struct
        *tty) {
    int ret = 0;
    struct rawbulk_function *fn = lookup_by_tty_minor(tty->index);
    ldbg("%s\n", __func__);

    if (!fn)
        return -ENODEV;

    ret = tty_init_termios(tty);
    if (ret < 0)
        return ret;

    tty->driver_data = fn;
    fn->tty = tty;

    /* Final install (we use the default method) */
    tty_driver_kref_get(driver);
    tty->count++;
    driver->ttys[tty->index] = tty;
    return ret;
}

static void rawbulk_tty_cleanup(struct tty_struct *tty) {
    struct rawbulk_function *fn = lookup_by_tty_minor(tty->index);
    ldbg("%s\n", __func__);
    tty->driver_data = NULL;
    if (fn)
        fn->tty = NULL;
}

static int rawbulk_tty_open(struct tty_struct *tty, struct file *flip) {
    int ret;
    struct rawbulk_function *fn = lookup_by_tty_minor(tty->index);
    ldbg("%s\n", __func__);

    if (!fn)
        return -ENODEV;

    if(fn->cbp_reset)
        return -EPIPE;
    tty->driver_data = fn;
    fn->tty = tty;

    if (check_enable_state(fn))
        return -EBUSY;

    if (check_tty_opened(fn))
        return -EBUSY;

    set_tty_opened(fn, 1);

    ret = tty_port_open(&fn->port, tty, flip);
    if (ret < 0)
        return ret;
    return 0;
}

static void rawbulk_tty_hangup(struct tty_struct *tty) {
    struct rawbulk_function *fn = tty->driver_data;
    ldbg("%s\n", __func__);
    if (!fn)
        return;
    tty_port_hangup(&fn->port);
}

static void rawbulk_tty_close(struct tty_struct *tty, struct file *flip) {
    struct rawbulk_function *fn = tty->driver_data;
    struct usb_request_wrapper *req;
    ldbg("%s\n", __func__);

    if (!fn)
        return;
         
    set_tty_opened(fn, 0);
    flush_workqueue(fn->tty_push_wq);    
    while ((req = get_req_without_del(&fn->rx_inproc, &fn->rx_lock))) {
        if(req->request->status == -EINPROGRESS) {
            usb_ep_dequeue(fn->bulk_out, req->request);
        } else {
            msleep(5);
        }
    }
    
    while ((req = get_req_without_del(&fn->tx_inproc, &fn->tx_lock))) {                
        if(req->request->status == -EINPROGRESS) {
            usb_ep_dequeue(fn->bulk_in, req->request);           
        } else {
            msleep(5);
        }
    }          

    tty_port_close(&fn->port, tty, flip);
}

static __init int rawbulk_tty_init(void) {
    int ret;
    struct tty_driver *drv = alloc_tty_driver(_MAX_TID);
    ldbg("%s\n", __func__);
    if (!drv)
        return -ENOMEM;

    drv->owner = THIS_MODULE;
    drv->driver_name = "rawbulkport";
    drv->name = "ttyRB";//prefix
    drv->type = TTY_DRIVER_TYPE_SERIAL;
    drv->subtype = SERIAL_TYPE_NORMAL;
    drv->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
    drv->init_termios = tty_std_termios;
    drv->init_termios.c_cflag =
        B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    drv->init_termios.c_lflag = 0;
    drv->init_termios.c_ispeed = 9600;
    drv->init_termios.c_ospeed = 9600;

    tty_set_operations(drv, &rawbulk_tty_ops);

    ret = tty_register_driver(drv);
    if (ret < 0) {
        put_tty_driver(drv);
    } else
        rawbulk_tty_driver = drv;
    return ret;
}

/******************************************************************************/
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
    {"rawbulk-modem", "data", "Modem Port", 16, 16, 4096, 4096, true, false },
    {"rawbulk-ets", "ets", "ETS Port", 4, 4, 4096, 4096, true, false },
    {"rawbulk-at", "atc", "AT Channel", 3, 3, 4096, 4096, true, false },
    {"rawbulk-pcv", "pcv", "PCM Voice", 1, 1, 4096, 4096, true, false },
    {"rawbulk-gps", "gps", "LBS GPS Port", 1, 1, 4096, 4096, true, false },
    { }, /* End of configurations */
};

static __init struct rawbulk_function *rawbulk_alloc_function(int transfer_id) {
    int rc;
    struct rawbulk_function *fn;
    ldbg("%s\n", __func__);
    
    if (transfer_id == _MAX_TID)
        return NULL;

    fn = kzalloc(sizeof *fn, GFP_KERNEL);
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

    fn->fs_descs[INTF_DESC] = (struct usb_descriptor_header *) &fn->interface;
    fn->fs_descs[BULKIN_DESC] = (struct usb_descriptor_header *) &fn->fs_bulkin_endpoint;
    fn->fs_descs[BULKOUT_DESC] = (struct usb_descriptor_header *) &fn->fs_bulkout_endpoint;

    fn->hs_descs[INTF_DESC] = (struct usb_descriptor_header *) &fn->interface;
    fn->hs_descs[BULKIN_DESC] = (struct usb_descriptor_header *) &fn->hs_bulkin_endpoint;
    fn->hs_descs[BULKOUT_DESC] = (struct usb_descriptor_header *) &fn->hs_bulkout_endpoint;

    fn->string_table.language = 0x0409;
    fn->string_table.strings = fn->string_defs;
    fn->strings[0] = &fn->string_table;
    fn->strings[1] = NULL;

    fn->transfer_id = transfer_id;
    fn->tty_throttled = 0;
    /* init function callbacks */
    fn->function.strings = fn->strings;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 100))
    fn->function.fs_descriptors = fn->fs_descs;
#else
	fn->function.descriptors = fn->fs_descs;
#endif
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
        fn->max_attrs ++;
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
    wake_lock_init(&fn->keep_awake, WAKE_LOCK_SUSPEND, fn->longname);
    return fn;
}

static void rawbulk_destory_function(struct rawbulk_function *fn) {
    ldbg("%s\n", __func__);
    
    if (!fn)
        return;
    wake_lock_destroy(&fn->keep_awake);
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
#endif /* SUPPORT_LEGACY_CONTROL */

static int __init init(void) {
    int n;
    int rc = 0;

    printk(KERN_INFO "rawbulk functions init.\n");
	rawbulk_class = class_create(THIS_MODULE, "usb_rawbulk");
	if (IS_ERR(rawbulk_class))
		return PTR_ERR(rawbulk_class);

    for (n = 0; n < _MAX_TID; n ++) {
        struct rawbulk_function *fn = rawbulk_alloc_function(n);
        if (IS_ERR(fn)) {
            while (n --)
                rawbulk_destory_function(prealloced_functions[n]);
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
        rc = sysfs_create_group(legacy_sysfs_stuff, &legacy_sysfs_group);
        if (rc < 0)
            printk(KERN_ERR "failed to create legacy bypass sys-stuff, but continue...\n");
    }
#endif /* SUPPORT_LEGACY_CONTROL */

    rc = rawbulk_tty_init();
    if (rc < 0) {
        printk(KERN_ERR "failed to init rawbulk tty driver.\n");
        return rc;
    }

    for (n = 0; n < _MAX_TID; n ++) {
        struct rawbulk_function *fn = prealloced_functions[n];
        rc = rawbulk_register_tty(fn);
        if (rc < 0) {
            printk(KERN_ERR "fatal error: we cannot create ttyRB%d for %s\n",
                    n, fn->longname);
            return rc;
        }
        INIT_WORK(&fn->tty_push_work, rawbulk_tty_push_data);
        fn->tty_push_wq = create_singlethread_workqueue(fn->shortname);
        if (!fn->tty_push_wq)
	        return -ENOMEM;
    }
   
    return 0;
}

module_init(init);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");

