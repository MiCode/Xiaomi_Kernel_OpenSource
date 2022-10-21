/** ***************************************************************************
 * @file nano_input.c
 *
 * @brief implent nanosic virtual input driver such as keyboard , mouse and touch
 *
 * <em>Copyright (C) 2010, Nanosic, Inc.  All rights reserved.</em>
 * Author : Bin.yuan bin.yuan@nanosic.com 
 * */

/*
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

#include <linux/module.h>
#include <linux/input.h>
#include <linux/hid.h>

#include "nano_macro.h"
#include "nano_input.h"

static struct hid_device * gVirtualConsumerDev;
static struct hid_device * gVirtualKeyboardDev;
static struct hid_device * gVirtualTouchDev;
static struct hid_device * gVirtualMouseDev;

/*定义虚拟化的键盘,鼠标,触摸设备*/
static struct nano_input_create_dev gConsumerDevice =
{
    .name = "Xiaomi Consumer",
    .rd_data = HID_ConsumerReportDescriptor,
    .rd_size = sizeof(HID_ConsumerReportDescriptor),
    .vendor  = 0x15D9,
    .product = 0xA4,
    .bus     = BUS_VIRTUAL,
};

static struct nano_input_create_dev gKeyboardDevice =
{
    .name = "Xiaomi Keyboard",
    .rd_data = HID_KeyboardReportDescriptor,
    .rd_size = sizeof(HID_KeyboardReportDescriptor),
    .vendor  = 0x15D9,
    .product = 0xA3,
    .bus     = BUS_VIRTUAL,
};
/*鼠标通道*/
static struct nano_input_create_dev gMouseDevice =
{
    .name = "Xiaomi Mouse",
    .rd_data = HID_MouseReportDescriptor,
    .rd_size = sizeof(HID_MouseReportDescriptor),
    .vendor  = 0x15D9,
    .product = 0xA2,
    .bus     = BUS_VIRTUAL,
};
/*触摸通道*/
static struct nano_input_create_dev gTouchDevice =
{
    .name = "Xiaomi Touch",
    .rd_data = HID_TouchReportDescriptor,
    .rd_size = sizeof(HID_TouchReportDescriptor),
    .vendor  = 0x15D9,
    .product = 0xA1,
    .bus     = BUS_VIRTUAL,
};

/** **************************************************************************
 * @brief Nanosic_input_write
 *        data inject to input layer , 键盘,鼠标,多指数据的注入
 ** */
int 
Nanosic_input_write(EM_PacketType type ,void *buf, size_t len)
{
    int ret = -1;
	switch(type)
	{
		case EM_PACKET_KEYBOARD:
            //dbgprint(DEBUG_LEVEL,"report keyboard event\n");
            ret = hid_input_report(gVirtualKeyboardDev, HID_INPUT_REPORT, buf,min_t(size_t, len, 100), 0);
            if(ret)
                dbgprint(ERROR_LEVEL,"Nanosic_input_write：report keyboard event err: %d \n",ret);
			break;

        case EM_PACKET_CONSUMER:
            ret = hid_input_report(gVirtualConsumerDev, HID_INPUT_REPORT, buf,min_t(size_t, len, 100), 0);
            if(ret)
                dbgprint(ERROR_LEVEL,"Nanosic_input_write：report keyboard event err: %d \n",ret);
            break;

		case EM_PACKET_MOUSE:
            //dbgprint(DEBUG_LEVEL,"report mouse event \n");
            hid_input_report(gVirtualMouseDev, HID_INPUT_REPORT, buf,min_t(size_t, len, 100), 0);
			break;

		case EM_PACKET_TOUCH:
            //dbgprint(DEBUG_LEVEL,"report touch event\n");
            hid_input_report(gVirtualTouchDev, HID_INPUT_REPORT, buf,min_t(size_t, len, 100), 0);
			break;
			
		default:
			break;
	}
	
	return 0;
}

/** ************************************************************************//**
 *  @func Nanosic_input_hid_start
 *  
 *  @brief
 *
 ** */
static int 
Nanosic_input_hid_start(struct hid_device *hid)
{
    dbgprint(DEBUG_LEVEL,"**** Nanosic_input_hid_start ****\n");
    return 0;
}

/** ************************************************************************//**
 *  @func Nanosic_input_hid_stop
 *  
 *  @brief
 *
 ** */
static void 
Nanosic_input_hid_stop(struct hid_device *hid)
{
    hid->claimed = 0;
    //dbgprint(DEBUG_LEVEL,"**** Nanosic_input_hid_stop ****\n");
}

/** ************************************************************************//**
 *  @func Nanosic_input_hid_open
 *  
 *  @brief
 *
 ** */
static int 
Nanosic_input_hid_open(struct hid_device *hid)
{
    //dbgprint(DEBUG_LEVEL,"**** Nanosic_input_hid_open ****\n");
    return 0;
}

/** ************************************************************************//**
 *  @func Nanosic_input_hid_close
 *  
 *  @brief
 *
 ** */
static void 
Nanosic_input_hid_close(struct hid_device *hid)
{
    //dbgprint(DEBUG_LEVEL,"**** Nanosic_input_hid_close ****\n");
}

/** ************************************************************************//**
 *  @func Nanosic_input_hid_parse
 *  
 *  @brief  OK
 *
 ** */
static int 
Nanosic_input_hid_parse(struct hid_device *hid)
{
	struct nano_input_create_dev* ev = hid->driver_data;
    //dbgprint(DEBUG_LEVEL,"**** Nanosic_input_hid_parse ****\n");
	return hid_parse_report(hid, ev->rd_data, ev->rd_size);
}

/** ************************************************************************//**
 *  @func Nanosic_input_raw_request
 *
 *  @brief support send raw report to hidraw device
 *
 ** */
static int
Nanosic_input_raw_request (struct hid_device *hdev, unsigned char reportnum,
                                        __u8 *buf, size_t len, unsigned char rtype,int reqtype)
{
    return len;
}

/** ************************************************************************//**
 *  @func Nanosic_input_set_report
 *  
 *  @brief support set report to i2c slave device
 *
 ** */
static int
Nanosic_input_set_report(struct hid_device *hid, __u8 *buf, size_t size)
{

    if(IS_ERR(hid)){
        dbgprint(ERROR_LEVEL, "Invaild argment\n");
        return -EINVAL;
    }

    if(size <= 0){
        dbgprint(ERROR_LEVEL, "Invaild argment\n");
        return -EINVAL;
    }

    if(IS_ERR(buf)){
        dbgprint(ERROR_LEVEL, "Invaild argment\n");
        return -EINVAL;
    }

    rawdata_show("would not write i2c cmd" ,buf , size);
    return -EINVAL;
    
    /*return Nanosic_i2c_write(gI2c_client,buf , size);*/
}

/** ************************************************************************//**
 *  @func Nanosic_hid_ll_driver
 *  
 *  @brief 
 *
 ** */
static struct hid_ll_driver Nanosic_hid_ll_driver = {
	.start = Nanosic_input_hid_start,       /*call on probe dev*/
	.stop  = Nanosic_input_hid_stop,        /*call on remove dev*/
	.open  = Nanosic_input_hid_open,        /*call on input layer open*/
	.close = Nanosic_input_hid_close,       /*call on i input layer close*/
	.parse = Nanosic_input_hid_parse,       /*copy report map description*/
	.raw_request = Nanosic_input_raw_request,
	.output_report = Nanosic_input_set_report,
};

/** ************************************************************************//**
 *  @func Nanosic_input_create
 *  
 *  @brief 
 *
 ** */
static struct hid_device * 
Nanosic_input_create(struct nano_input_create_dev* ev)
{
	struct hid_device *hid = NULL;
	int ret;

    hid = hid_allocate_device();
    if (IS_ERR_OR_NULL(hid)) {
        ret = PTR_ERR(hid);
        return NULL;
    }

    strncpy(hid->name, ev->name, 127);
    hid->name[127] = 0;
    strncpy(hid->phys, ev->phys, 63);
    hid->phys[63] = 0;
    strncpy(hid->uniq, ev->uniq, 63);
    hid->uniq[63] = 0;

    hid->ll_driver = &Nanosic_hid_ll_driver;
    hid->bus = ev->bus;
    hid->vendor = ev->vendor;
    hid->product = ev->product;
    hid->version = ev->version;
    hid->country = ev->country;
    hid->driver_data = ev;
    hid->dev.parent = NULL;

    ret = hid_add_device(hid);
    if (ret) {
        hid_err(hid, "Cannot register HID device\n");
        goto err_hid;
    }

    return hid;

err_hid:
    
	hid_destroy_device(hid);
    return NULL;
}

/** ************************************************************************//**
 *  @func Nanosic_input_register
 *  
 *  @brief register hid virtual device { 注册对应的input 虚拟设备 }
 *
 ** */
int
Nanosic_input_register(void)
{
	gVirtualKeyboardDev = Nanosic_input_create(&gKeyboardDevice);
	if(IS_ERR_OR_NULL(gVirtualKeyboardDev))
		goto _err1;

	gVirtualMouseDev = Nanosic_input_create(&gMouseDevice);
	if(IS_ERR_OR_NULL(gVirtualMouseDev))
		goto _err2;

	gVirtualTouchDev = Nanosic_input_create(&gTouchDevice);
	if(IS_ERR_OR_NULL(gVirtualTouchDev))
		goto _err3;

	gVirtualConsumerDev = Nanosic_input_create(&gConsumerDevice);
	if(IS_ERR_OR_NULL(gVirtualConsumerDev))
		goto _err4;
	dbgprint(DEBUG_LEVEL,"input create ok\n");
    
	return 0;
_err4:
	hid_destroy_device(gVirtualTouchDev);
_err3:
	hid_destroy_device(gVirtualMouseDev);
_err2:
	hid_destroy_device(gVirtualKeyboardDev);
_err1:
	dbgprint(ERROR_LEVEL,"input create err\n");
	return -1;
}

/** ************************************************************************//**
 *  @func Nanosic_input_release
 *  
 *  @brief
 *
 ** */
int 
Nanosic_input_release(void)
{
	if(!IS_ERR_OR_NULL(gVirtualKeyboardDev))
		hid_destroy_device(gVirtualKeyboardDev);

	if(!IS_ERR_OR_NULL(gVirtualMouseDev))
		hid_destroy_device(gVirtualMouseDev);

	if(!IS_ERR_OR_NULL(gVirtualTouchDev))
		hid_destroy_device(gVirtualTouchDev);

	if(!IS_ERR_OR_NULL(gVirtualConsumerDev))
		hid_destroy_device(gVirtualConsumerDev);

	dbgprint(DEBUG_LEVEL,"input release ok\n");

	return 0;
}

