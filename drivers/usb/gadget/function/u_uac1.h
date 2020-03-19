/*
 * u_uac1.h - Utility definitions for UAC1 function
 *
 * Copyright (C) 2016 Ruslan Bilovol <ruslan.bilovol@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __U_UAC1_H
#define __U_UAC1_H

#include <linux/usb/composite.h>

#define UAC1_OUT_EP_MAX_PACKET_SIZE	200
#define UAC1_DEF_CCHMASK	0x3
#define UAC1_DEF_CSRATE		48000
#define UAC1_DEF_CSSIZE		2
#define UAC1_DEF_PCHMASK	0x3
#define UAC1_DEF_PSRATE		48000
#define UAC1_DEF_PSSIZE		2
#define UAC1_DEF_REQ_NUM	2


struct f_uac1_opts {
	struct usb_function_instance	func_inst;
	struct f_uac1			*uac1;
	int				c_chmask;
	int				c_srate;
	int				c_ssize;
	int				p_chmask;
	int				p_srate;
	int				p_ssize;
	int				req_number;
	unsigned			bound:1;

	struct mutex			lock;
	int				refcnt;
};

#define UAC_DT_MIXER_UNIT_SIZE		13
/* 4.3.2.3 Mixer Unit Descriptor */
struct uac1_mixer_unit_descriptor {
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubtype;
	__u8 bUnitID;
	__u8 bNrInPins;
	__u8 baSourceID[2];
	__u8  bNrChannels;
	__le16 wChannelConfig;
	__u8  iChannelNames;
	__u8 bmControls;
	__u8  iMixer;
} __packed;

#endif /* __U_UAC1_H */
