/*
 * Rawbulk Gadget Function Driver from VIA Telecom
 *
 * Copyright (C) 2011 VIA Telecom, Inc.
 * Author: Juelun Guo (jlguo@via-telecom.com)
 *
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

#define DRIVER_AUTHOR   "jlguo <jlguo@via-telecom.com>"
#define DRIVER_DESC     "Rawbulk Gadget - transport data from CP to Gadget"
#define DRIVER_VERSION  "1.0.1"

#include <linux/err.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/usb/composite.h>
#include <mach/viatel_rawbulk.h>

#define VERBOSE_DEBUG
#ifdef VERBOSE_DEBUG
#define ldbg(fmt, args...) \
    printk(KERN_INFO "%s: " fmt "\n", __func__, ##args)
#else
#define ldbg(args...)
#endif

//int setdtr, data_connect;
//struct work_struct flow_control;
//struct work_struct dtr_status;

#define function_to_rbf(f) container_of(f, struct rawbulk_function, function)
static int usb_state;

void simple_setup_complete(struct usb_ep *ep,
                struct usb_request *req) {
    ;//DO NOTHING
}

#define FINISH_DISABLE 0
#define START_DISABLE  1
 
int rawbulk_function_setup(struct usb_function *f, const struct
        usb_ctrlrequest *ctrl) {
    struct rawbulk_function *fn = function_to_rbf(f);
    unsigned int  setdtr = 0;
    unsigned int data_connect = 0;
    struct usb_composite_dev *cdev = f->config->cdev;
    struct usb_request *req = cdev->req;
    int         value = -EOPNOTSUPP;
    u16         w_index = le16_to_cpu(ctrl->wIndex);
    u16         w_value = le16_to_cpu(ctrl->wValue);
    u16         w_length = le16_to_cpu(ctrl->wLength);

    ldbg("%s\n", __func__);

    
   if(ctrl->bRequest) {
         ldbg("ctrl->bRequestType = %0x  ctrl->bRequest = %0x \n", ctrl->bRequestType, ctrl->bRequest);
   }
   switch(ctrl->bRequest) {
        case 0x01:
            if(ctrl->bRequestType == (USB_DIR_OUT | USB_TYPE_VENDOR)) {//0x40
                // set/clear DTR 
                ldbg("setdtr = %d, w_value =%d\n", setdtr, w_value);
                if(fn->activated){
                    setdtr = w_value & 0x01;                 
                    //schedule_work(&flow_control);
                    modem_dtr_set(setdtr, 0);
                    modem_dcd_state();
                }                                 
                value = 0;
            }
            break;
        case 0x02:
            if(ctrl->bRequestType == (USB_DIR_IN | USB_TYPE_VENDOR)) {//0xC0
                // DSR | CD109 
                //schedule_work(&dtr_status);
                data_connect = modem_dcd_state();
                //modem_dtr_query(&data_connect, 0);
                if(fn->activated) {                   
                    if(data_connect && fn->enable) {
                        *((unsigned char *)req->buf) = 0x3;
                        ldbg("connect %d\n", data_connect);
                                            
                    }
                    else {
                        *((unsigned char *)req->buf) = 0x2;
                        ldbg("disconnect=%d, setdtr=%d\n", data_connect, setdtr);
                    }              
                }
                else //set CD CSR state to 0 if modem bypass not inactive
                    *((unsigned char *)req->buf) = 0x0;
                value = 1;
            }
            break;
        case 0x03:
            if(ctrl->bRequestType == (USB_DIR_OUT | USB_TYPE_VENDOR)) {//0x40
                // xcvr 
                ldbg("CTRL SET XCVR 0x%02x\n", w_value);
                value = 0;
            }
            break;
        case 0x04:
            if((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_VENDOR) {
                if(ctrl->bRequestType & USB_DIR_IN) {//0xC0
                    // return ID
                    sprintf(req->buf, "CBP_8.2");
                    value = 1;
                } else {//0x40
                    value = 0;
                }
            }
            break;
        case 0x05:
            if(ctrl->bRequestType == (USB_DIR_IN | USB_TYPE_VENDOR)) {//0xC0
                // connect status 
                ldbg("CTRL CONNECT STATUS\n");
                *((unsigned char *)req->buf) = 0x0;
                value = 1;
            }
            break;
        default:
            ldbg("invalid control req%02x.%02x v%04x i%04x l%d\n",
                    ctrl->bRequestType, ctrl->bRequest,
                    w_value, w_index, w_length);
    }

    // respond with data transfer or status phase? 
       if (value >= 0) {
        req->zero = 0;
        req->length = value;
        req->complete = simple_setup_complete;
        value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
        if (value < 0)
            printk(KERN_ERR "response err %d\n", value);
        }

    // device either stalls (value < 0) or reports success 
    return value;

}

static void rawbulk_auto_reconnect(int transfer_id) {
    int rc;
    struct rawbulk_function *fn = rawbulk_lookup_function(transfer_id);
    ldbg("%s\n", __func__);
    if (!fn || fn->autoreconn == 0)
        return;

    if (rawbulk_check_enable(fn) && fn->activated) {
        printk(KERN_INFO "start %s again automatically.\n", fn->longname);
        rc = rawbulk_start_transactions(transfer_id, fn->nups,
                fn->ndowns, fn->upsz, fn->downsz);
        if (rc < 0) {
            rawbulk_disable_function(fn);
            rawbulk_tty_start_io(fn);
        }
    }
}

int rawbulk_function_bind(struct usb_configuration *c, struct
        usb_function *f) {
    int rc, ifnum;
    struct rawbulk_function *fn = function_to_rbf(f);
    struct usb_gadget *gadget = c->cdev->gadget;
    struct usb_ep *ep_out, *ep_in;
    
    printk("%s\n", __func__);
    rc = usb_interface_id(c, f);
    if (rc < 0)
        return rc;
    ifnum = rc;

    fn->interface.bInterfaceNumber = cpu_to_le16(ifnum);

    ep_out = usb_ep_autoconfig(gadget, (struct usb_endpoint_descriptor *)
            fn->fs_descs[BULKOUT_DESC]);
    if (!ep_out) {
        printk(KERN_ERR "%s %d config ep_out error  \n", __FUNCTION__,__LINE__);
        return -ENOMEM;
    }

    ep_in = usb_ep_autoconfig(gadget, (struct usb_endpoint_descriptor *)
            fn->fs_descs[BULKIN_DESC]);
    if (!ep_in) {
        usb_ep_disable(ep_out);
        printk(KERN_ERR "%s %d config ep_in error  \n", __FUNCTION__,__LINE__);
        return -ENOMEM;
    }

    ep_out->driver_data = fn;
    ep_in->driver_data = fn;
    fn->bulk_out = ep_out;
    fn->bulk_in = ep_in;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 100))
    f->fs_descriptors = usb_copy_descriptors(fn->fs_descs);
	if (unlikely(!f->fs_descriptors))
		return -ENOMEM;
#else
	f->descriptors = usb_copy_descriptors(fn->fs_descs);
	if (unlikely(!f->descriptors))
		return -ENOMEM;
#endif

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
        usb_function *f) {
    struct rawbulk_function *fn = function_to_rbf(f);
     
    ldbg("%s\n", __func__);

    //cancel_work_sync(&fn->activator);
    //cancel_work_sync(&flow_control);
    //cancel_work_sync(&dtr_status);
    rawbulk_unbind_function(fn->transfer_id);
}

/*void modem_flow_control(struct work_struct *work) {    
    modem_dtr_set(!!setdtr);
    modem_dtr_query(&data_connect, 1);   
}

void dtr_status_query(struct work_struct *work) {  
    modem_dtr_query(&data_connect, 1);   
}
*/
static void do_activate(struct work_struct *data) {
    struct rawbulk_function *fn = container_of(data, struct rawbulk_function,
            activator);
    int rc;
    struct usb_function *functionp = &(fn->function);
   
    printk("%s usb state %s \n", __func__, (fn->activated?"connect":"disconnect"));
    if (fn->activated) { /* enumerated */
        /* enabled endpoints */
        rc = config_ep_by_speed(fn->cdev->gadget, functionp, fn->bulk_out);
        if (rc < 0) {
            printk(KERN_ERR "failed to config speed rawbulk %s %d\n",
                    fn->bulk_out->name, rc);
            return;
        }
        rc = usb_ep_enable(fn->bulk_out);
        if (rc < 0) {
            printk(KERN_ERR "failed to enable rawbulk %s %d\n",
                    fn->bulk_out->name, rc);
            return;
        }
        
        rc = config_ep_by_speed(fn->cdev->gadget, functionp, fn->bulk_in);
        if (rc < 0) {
            printk(KERN_ERR "failed to config speed rawbulk %s %d\n",
                    fn->bulk_in->name, rc);
            return;
        }
        rc = usb_ep_enable(fn->bulk_in);
        if (rc < 0) {
            printk(KERN_ERR "failed to enable rawbulk %s %d\n",
                    fn->bulk_in->name, rc);
            usb_ep_disable(fn->bulk_out);
            return;
        }

        /* start rawbulk if enabled */
        if (rawbulk_check_enable(fn)) {
            wake_lock(&fn->keep_awake);
            rc = rawbulk_start_transactions(fn->transfer_id, fn->nups,
                    fn->ndowns, fn->upsz, fn->downsz);
            if (rc < 0)
                //rawbulk_disable_function(fn);
                printk(KERN_ERR "%s: failed to  bypass, channel id = %d\n", __func__, fn->transfer_id);
        }

        /* start tty io */
        rc = rawbulk_tty_alloc_request(fn);
        if (rc < 0)
            return;
        if (!rawbulk_check_enable(fn))
            rawbulk_tty_start_io(fn);
    } else { /* disconnect */            
        if (rawbulk_check_enable(fn)) {
            if (fn->transfer_id == RAWBULK_TID_MODEM) {
                /* this in interrupt, but DTR need be set firstly then clear it
                 * */
                modem_dtr_set(1, 1);
                modem_dtr_set(0, 1);
                modem_dtr_set(1, 1);
                modem_dcd_state();
            }
            
            rawbulk_stop_transactions(fn->transfer_id);
            /* keep the enable state, so we can enable again in next time */
            //set_enable_state(fn, 0);
            wake_unlock(&fn->keep_awake);
        } else
            rawbulk_tty_stop_io(fn);

        rawbulk_tty_free_request(fn);

        usb_ep_disable(fn->bulk_out);
        usb_ep_disable(fn->bulk_in);

        fn->bulk_out->driver_data = NULL;
        fn->bulk_in->driver_data = NULL;
    }
}

static void rawbulk_usb_state_set(int state) {
        usb_state = state?1:0;
        printk("%s usb_state = %d\n", __func__, usb_state);
}

int rawbulk_usb_state_check(void) {
     return usb_state?1:0;
}
EXPORT_SYMBOL_GPL(rawbulk_usb_state_check);

static int rawbulk_function_setalt(struct usb_function *f, unsigned intf,
        unsigned alt) {
    struct rawbulk_function *fn = function_to_rbf(f);
    ldbg("%s \n", __func__);
    fn->activated = 1;
    rawbulk_usb_state_set(fn->activated);
    schedule_work(&fn->activator);
    return 0;
}

static void rawbulk_function_disable(struct usb_function *f) {
    struct rawbulk_function *fn = function_to_rbf(f);

    ldbg("%s \n", __func__);

    fn->activated = 0;
    rawbulk_usb_state_set(fn->activated);
    schedule_work(&fn->activator);
}

int rawbulk_bind_config(struct usb_configuration *c, int transfer_id) {

    int rc;   
    struct rawbulk_function *fn = rawbulk_lookup_function(transfer_id);

    if (!fn)
        return -ENOMEM;
        
    printk(KERN_INFO "add %s to config.\n", fn->longname);

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
        /*if(fn->transfer_id == RAWBULK_TID_MODEM) {
            INIT_WORK(&flow_control, modem_flow_control);
            INIT_WORK(&dtr_status, dtr_status_query);
        }*/ 
            fn->initialized = 1;
    }

    rc = usb_add_function(c, &fn->function);
    if (rc < 0)
        printk(KERN_ERR "%s - failed to config %d.\n", __func__, rc);

    return rc;
}
