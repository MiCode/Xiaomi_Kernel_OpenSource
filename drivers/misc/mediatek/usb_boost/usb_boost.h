/*
 * Copyright (C) 2016 MediaTek Inc.
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
#ifndef _MTK_USB_BOOST_H
#define _MTK_USB_BOOST_H

enum{
	TYPE_CPU_FREQ,
	TYPE_CPU_CORE,
	TYPE_DRAM_VCORE,
	_TYPE_MAXID
};

enum{
	ACT_HOLD,
	ACT_RELEASE,
	_ACT_MAXID
};

struct act_arg_obj {
	int arg1;
	int arg2;
	int arg3;
};

void usb_boost_set_para_and_arg(int id, int *para, int para_range,
	struct act_arg_obj *act_arg);

void usb_boost_by_id(int id);
void usb_boost(void);
int usb_boost_init(void);

void register_usb_boost_act(int type_id, int action_id,
	int (*func)(struct act_arg_obj *arg));

/* #define USB_BOOST_DBG_ENABLE */
#define USB_BOOST_NOTICE(fmt, args...) \
	pr_notice("USB_BOOST, <%s(), %d> " fmt, __func__, __LINE__, ## args)

#ifdef USB_BOOST_DBG_ENABLE
#define USB_BOOST_DBG(fmt, args...) \
	pr_notice("USB_BOOST, <%s(), %d> " fmt, __func__, __LINE__, ## args)
#else
#define USB_BOOST_DBG(fmt, args...) do {} while (0)
#endif

#endif
