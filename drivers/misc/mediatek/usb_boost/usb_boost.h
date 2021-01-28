/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
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

/* void usb_boost_by_id(int id); */
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
